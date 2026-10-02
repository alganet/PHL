# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 943/1027 lines (91.82%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * Section:` |
|      - |    9 | ` *  JSON encoding/decoding routines.` |
|      - |   10 | ` * Status:` |
|      - |   11 | ` *    Devel.` |
|      - |   12 | ` */` |
|      - |   13 | `/* Forward reference */` |
|      - |   14 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|      - |   15 | `static int VmJsonObjectEncode(const SyString *pAttr,ph7_value *pValue,void *pUserData);` |
|      - |   16 | `/*` |
|      - |   17 | ` * JSON encoder state is stored in an instance` |
|      - |   18 | ` * of the following structure.` |
|      - |   19 | ` */` |
|      - |   20 | `typedef struct json_private_data json_private_data;` |
|      - |   21 | `struct json_private_data` |
|      - |   22 | `{` |
|      - |   23 | `	ph7_context *pCtx; /* Call context */` |
|      - |   24 | `	int isFirst;       /* True if first encoded entry */` |
|      - |   25 | `	int isObject;      /* True if the current array level is encoded as a JSON object */` |
|      - |   26 | `	int iFlags;        /* JSON encoding flags */` |
|      - |   27 | `	int nRecCount;     /* Recursion count */` |
|      - |   28 | `	int exc;           /* True if a jsonSerialize() callback threw an exception */` |
|      - |   29 | `	int oom;           /* True if a result append ran out of memory (raises a fatal) */` |
|      - |   30 | `	int fail;          /* True if the value is unencodable — json_encode returns` |
|      - |   31 | `	                    * FALSE (or throws under JSON_THROW_ON_ERROR) */` |
|      - |   32 | `	int failRc;        /* json_rc to report for a ->fail (INF_OR_NAN vs` |
|      - |   33 | `	                    * NON_BACKED_ENUM) */` |
|      - |   34 | `	ph7_int64 nMaxDepth; /* json_encode's $depth: a container may only OPEN while` |
|      - |   35 | `	                      * fewer than this many containers enclose it. php runs no` |
|      - |   36 | `	                      * range screen here — 0 or a negative value simply makes` |
|      - |   37 | `	                      * every container JSON_ERROR_DEPTH. */` |
|      - |   38 | `	SySet aPath;       /* Containers on the current encode path (ph7_hashmap* /` |
|      - |   39 | `	                    * ph7_class_instance*), pushed on entry and popped on exit:` |
|      - |   40 | `	                    * meeting one again below itself is php's` |
|      - |   41 | `	                    * JSON_ERROR_RECURSION, not an infinite descent. */` |
|      - |   42 | `	int bPresented;    /* The entries being walked are a native class's PRESENTED` |
|      - |   43 | `	                    * shape (php's get_properties), which carries the object's` |
|      - |   44 | `	                    * own table MANGLED. json_encode emits public properties` |
|      - |   45 | `	                    * only, and the mangling is exactly what says which those` |
|      - |   46 | ``	                    * are -- without it a subclass's `protected $p` reached the`` |
|      - |   47 | `	                    * output as the key "\u0000*\u0000p". Not set for a plain` |
|      - |   48 | `	                    * ARRAY, where php really does emit such a key` |
|      - |   49 | `	                    * (json_encode((array)$o) shows the mangled names). */` |
|      - |   50 | `};` |
|      - |   51 | `/*` |
|      - |   52 | ` * Stack-safety ceiling on the EFFECTIVE json depth, both directions. php honors` |
|      - |   53 | ` * $depth up to INT_MAX and relies on a dynamic guard that asks the platform how` |
|      - |   54 | ` * much C stack is left; PHL's encoder and decoder recurse on the same C stack` |
|      - |   55 | ` * with no such probe, so a requested depth above this bound is clamped to it and` |
|      - |   56 | ` * a value/document nested deeper reports JSON_ERROR_DEPTH — loud, like the` |
|      - |   57 | ` * SERIALIZE_MAX_DEPTH and HTTP_QUERY_MAX_DEPTH bounds this follows. 4096 is` |
|      - |   58 | ` * eight times php's default of 512 and holds ~0.5MB of frames on MSVC's 1MB` |
|      - |   59 | ` * default stack (measured ~10x smaller on glibc's 8MB). Ports with small stacks` |
|      - |   60 | ` * (ESP32 task stacks are KBs) can override it at build time.` |
|      - |   61 | ` */` |
|      - |   62 | `#ifndef PH7_JSON_DEPTH_CEILING` |
|      - |   63 | `#define PH7_JSON_DEPTH_CEILING 4096` |
|      - |   64 | `#endif` |
|      - |   65 | `/*` |
|      - |   66 | ` * True if pPtr (a hashmap or class instance) is a container the encoder is` |
|      - |   67 | ` * currently INSIDE of. Only the active path is searched, so a value appearing` |
|      - |   68 | ` * twice as SIBLINGS ([$a,$a]) stays legal like php — recursion means` |
|      - |   69 | ` * self-containment, not sharing.` |
|      - |   70 | ` */` |
|   8996 |   71 | `static int VmJsonPathHolds(json_private_data *pData,void *pPtr)` |
|      5 |   72 | `{` |
|   9001 |   73 | `	void **apEntry = (void **)SySetBasePtr(&pData->aPath);` |
|   9001 |   74 | `	sxu32 i,n = SySetUsed(&pData->aPath);` |
| 797317 |   75 | `	for( i = 0 ; i < n ; ++i ){` |
| 788329 |   76 | `		if( apEntry[i] == pPtr ){` |
|      9 |   77 | `			return 1;` |
|      - |   78 | `		}` |
| 394163 |   79 | `	}` |
|   8993 |   80 | `	return 0;` |
|   4503 |   81 | `}` |
|      - |   82 | `/*` |
|      - |   83 | ` * Emit into the JSON result, flagging OOM on the shared data and bailing out` |
|      - |   84 | ` * of the current encode function (which returns PH7_OK; the top-level` |
|      - |   85 | ` * vm_builtin_json_encode checks ->oom and raises a non-catchable fatal). Used` |
|      - |   86 | ` * for every ph7_result_string/ph7_result_string_format append below.` |
|      - |   87 | ` */` |
|      - |   88 | `#define JSON_EMIT(pD, call) do { if( (call) != SXRET_OK ){ (pD)->oom = 1; return PH7_OK; } } while(0)` |
|      - |   89 | `/*` |
|      - |   90 | ` * Emit a float in php's json shape: PH7_AppendShortestReal (the shared` |
|      - |   91 | ` * serialize/var_export shortest-round-trip formatter, php's` |
|      - |   92 | ` * serialize_precision=-1) with the exponent marker lowercased (json prints` |
|      - |   93 | ` * 1.0e+17 where serialize prints 1.0E+17). Under JSON_PRESERVE_ZERO_FRACTION a` |
|      - |   94 | ` * value whose shortest form carries no '.' or exponent gets ".0" appended, so` |
|      - |   95 | ` * 1.0 stays a FLOAT on the round trip ("1.0", "-0.0") the way php keeps it.` |
|      - |   96 | ` */` |
|    212 |   97 | `static sxi32 VmJsonEmitReal(ph7_context *pCtx,double rVal,int iFlags)` |
|      3 |   98 | `{` |
|      - |   99 | `	SyBlob sNum;` |
|      - |  100 | `	char *z;` |
|      - |  101 | `	sxu32 i,n;` |
|      - |  102 | `	sxi32 rc;` |
|    215 |  103 | `	int bFrac = 0;` |
|    215 |  104 | `	SyBlobInit(&sNum,&pCtx->pVm->sAllocator);` |
|    215 |  105 | `	PH7_AppendShortestReal(&sNum,rVal);` |
|    215 |  106 | `	z = (char *)SyBlobData(&sNum);` |
|    215 |  107 | `	n = SyBlobLength(&sNum);` |
|    215 |  108 | `	if( z == 0 \|\| n < 1 ){` |
|    ! 0 |  109 | `		SyBlobRelease(&sNum);` |
|    ! 0 |  110 | `		return SXERR_MEM; /* treated as OOM by JSON_EMIT */` |
|      - |  111 | `	}` |
|    893 |  112 | `	for( i = 0 ; i < n ; i++ ){` |
|    681 |  113 | `		if( z[i] == 'E' ){` |
|     17 |  114 | `			z[i] = 'e';` |
|      8 |  115 | `		}` |
|    681 |  116 | `		if( z[i] == 'e' \|\| z[i] == '.' ){` |
|    114 |  117 | `			bFrac = 1;` |
|     56 |  118 | `		}` |
|    342 |  119 | `	}` |
|    215 |  120 | `	if( !bFrac && (iFlags & JSON_PRESERVE_ZERO_FRACTION) != 0 ){` |
|     11 |  121 | `		if( SyBlobAppend(&sNum,".0",2) != SXRET_OK ){` |
|    ! 0 |  122 | `			SyBlobRelease(&sNum);` |
|    ! 0 |  123 | `			return SXERR_MEM;` |
|      - |  124 | `		}` |
|     11 |  125 | `		z = (char *)SyBlobData(&sNum);` |
|     11 |  126 | `		n = SyBlobLength(&sNum);` |
|      5 |  127 | `	}` |
|    215 |  128 | `	rc = ph7_result_string(pCtx,(const char *)z,(int)n);` |
|    215 |  129 | `	SyBlobRelease(&sNum);` |
|    215 |  130 | `	return rc;` |
|    109 |  131 | `}` |
|      - |  132 | `/*` |
|      - |  133 | ` * JSON_PRETTY_PRINT helper: emit a newline followed by (depth * 4) spaces, so a` |
|      - |  134 | ` * container's members are laid out one-per-line and indented like php. A no-op` |
|      - |  135 | ` * unless JSON_PRETTY_PRINT is set. Returns SXRET_OK or an OOM status; callers` |
|      - |  136 | ` * wrap it in JSON_EMIT so an allocation failure trips the ->oom rail.` |
|      - |  137 | ` */` |
|  25062 |  138 | `static sxi32 VmJsonPretty(json_private_data *pJson,int depth)` |
|      5 |  139 | `{` |
|  25067 |  140 | `	ph7_context *pCtx = pJson->pCtx;` |
|      - |  141 | `	sxi32 rc;` |
|      - |  142 | `	int i;` |
|  25067 |  143 | `	if( (pJson->iFlags & JSON_PRETTY_PRINT) == 0 ){` |
|  24899 |  144 | `		return SXRET_OK;` |
|      - |  145 | `	}` |
|    171 |  146 | `	rc = ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    417 |  147 | `	for( i = 0 ; i < depth && rc == SXRET_OK ; ++i ){` |
|    249 |  148 | `		rc = ph7_result_string(pCtx,"    ",(int)sizeof("    ")-1);` |
|    126 |  149 | `	}` |
|    171 |  150 | `	return rc;` |
|  12536 |  151 | `}` |
|      - |  152 | `/*` |
|      - |  153 | ` * Byte length of the ill-formed UTF-8 run at z[0..n-1] as php's JSON encoder` |
|      - |  154 | ` * measures it: a byte that could LEAD a sequence (C2..F4) swallows every` |
|      - |  155 | ` * following byte that is merely continuation-SHAPED (10xxxxxx), up to the` |
|      - |  156 | ` * length its lead announces, and the whole prefix is ONE error. So "\xed\xa0\x80"` |
|      - |  157 | ` * (a surrogate) is a single JSON_ERROR_UTF8 / a single U+FFFD, while` |
|      - |  158 | ` * "\xf5\x80\x80\x80" is four — F5 leads nothing, so each byte fails alone.` |
|      - |  159 | ` *` |
|      - |  160 | ` * php's mbstring measures the same runs with the STRICTER per-lead ranges` |
|      - |  161 | ` * (builtin_mb.c's MbUtf8BadLen), which is why the two disagree on a surrogate:` |
|      - |  162 | ` * mb_strtolower("\xed\xa0\x80") is "???" while json substitutes one U+FFFD.` |
|      - |  163 | ` * Two php decoders, two rules — each matched where it belongs.` |
|      - |  164 | ` */` |
|    150 |  165 | `static sxu32 VmJsonBadUtf8Len(const unsigned char *z,sxu32 n)` |
|      1 |  166 | `{` |
|    151 |  167 | `	sxu32 c = z[0],need,i;` |
|    151 |  168 | `	if( c >= 0xC2 && c <= 0xDF ){` |
|     67 |  169 | `		need = 2;` |
|    118 |  170 | `	}else if( c >= 0xE0 && c <= 0xEF ){` |
|     13 |  171 | `		need = 3;` |
|     79 |  172 | `	}else if( c >= 0xF0 && c <= 0xF4 ){` |
|      7 |  173 | `		need = 4;` |
|      4 |  174 | `	}else{` |
|     67 |  175 | `		return 1; /* 80..C1 or F5..FF: leads nothing */` |
|      - |  176 | `	}` |
|    115 |  177 | `	for( i = 1 ; i < need && i < n && (z[i] & 0xC0) == 0x80 ; ++i ){}` |
|     85 |  178 | `	return i;` |
|     76 |  179 | `}` |
|      - |  180 | `/*` |
|      - |  181 | ` * Emit one code point as php's \uXXXX escape (lowercase hex), spelling anything` |
|      - |  182 | ` * outside the BMP as the UTF-16 surrogate pair JSON has no other way to carry:` |
|      - |  183 | ` * U+1F600 is "😀", exactly like php.` |
|      - |  184 | ` */` |
|    624 |  185 | `static sxi32 VmJsonEmitUnicodeEscape(ph7_context *pCtx,sxu32 cp)` |
|      1 |  186 | `{` |
|      - |  187 | `	static const char zHex[] = "0123456789abcdef";` |
|      - |  188 | `	sxu32 aUnit[2];` |
|      - |  189 | `	int nUnit,i;` |
|      - |  190 | `	char zEsc[12];` |
|    625 |  191 | `	if( cp >= 0x10000 ){` |
|      5 |  192 | `		sxu32 v = cp - 0x10000;` |
|      5 |  193 | `		aUnit[0] = 0xD800 + (v >> 10);` |
|      5 |  194 | `		aUnit[1] = 0xDC00 + (v & 0x3FF);` |
|      5 |  195 | `		nUnit = 2;` |
|      3 |  196 | `	}else{` |
|    621 |  197 | `		aUnit[0] = cp;` |
|    621 |  198 | `		nUnit = 1;` |
|      - |  199 | `	}` |
|   1253 |  200 | `	for( i = 0 ; i < nUnit ; ++i ){` |
|    629 |  201 | `		zEsc[i*6 + 0] = '\\';` |
|    629 |  202 | `		zEsc[i*6 + 1] = 'u';` |
|    629 |  203 | `		zEsc[i*6 + 2] = zHex[(aUnit[i] >> 12) & 0x0F];` |
|    629 |  204 | `		zEsc[i*6 + 3] = zHex[(aUnit[i] >>  8) & 0x0F];` |
|    629 |  205 | `		zEsc[i*6 + 4] = zHex[(aUnit[i] >>  4) & 0x0F];` |
|    629 |  206 | `		zEsc[i*6 + 5] = zHex[ aUnit[i]        & 0x0F];` |
|    315 |  207 | `	}` |
|    625 |  208 | `	return ph7_result_string(pCtx,zEsc,nUnit * 6);` |
|      1 |  209 | `}` |
|      - |  210 | `/*` |
|      - |  211 | ` * Emit one JSON string literal — the opening quote, the escaped body, the` |
|      - |  212 | ` * closing quote. Shared by the string VALUE path and by both KEY paths (array` |
|      - |  213 | ` * keys and object property names), which used to append their bytes raw: a key` |
|      - |  214 | ` * carrying a '"', a backslash or a control character produced UNPARSEABLE` |
|      - |  215 | ` * output (php: json_encode(["a\"b"=>1]) is {"a\"b":1}, PHL emitted {"a"b":1}).` |
|      - |  216 | ` * Everything php escapes in a string it escapes in a key, the JSON_HEX_*` |
|      - |  217 | ` * and JSON_UNESCAPED_SLASHES flags included.` |
|      - |  218 | ` *` |
|      - |  219 | ` * Non-ASCII is escaped as \uXXXX by DEFAULT, which is what php does and what` |
|      - |  220 | ` * JSON_UNESCAPED_UNICODE turns off — PHL used to emit the raw UTF-8 bytes` |
|      - |  221 | ` * unconditionally, i.e. behave as if that flag were always set (the flag was` |
|      - |  222 | ` * defined but never read). Even with it set php still escapes U+2028/U+2029,` |
|      - |  223 | ` * the two line terminators JavaScript's eval() chokes on, unless` |
|      - |  224 | ` * JSON_UNESCAPED_LINE_TERMINATORS is set too.` |
|      - |  225 | ` *` |
|      - |  226 | ` * bKey selects JSON_PARTIAL_OUTPUT_ON_ERROR's substitute for an ill-formed` |
|      - |  227 | ` * string: php replaces a VALUE with null and a KEY (array key or property` |
|      - |  228 | ` * name) with "" — the verdict must land before anything is emitted, because` |
|      - |  229 | ` * the replacement covers the WHOLE string, not the tail after the bad byte.` |
|      - |  230 | ` */` |
|     14 |  231 | `static int VmJsonStrHasBadUtf8(const char *zIn,int nByte)` |
|      1 |  232 | `{` |
|     15 |  233 | `	const unsigned char *z = (const unsigned char *)zIn,*zEnd = (const unsigned char *)&zIn[nByte];` |
|      - |  234 | `	sxu32 nLen;` |
|     37 |  235 | `	while( z < zEnd ){` |
|     31 |  236 | `		if( z[0] < 0x80 ){` |
|     23 |  237 | `			z++;` |
|     23 |  238 | `			continue;` |
|      - |  239 | `		}` |
|      9 |  240 | `		if( PH7_Utf8ReadStrict(z,(sxu32)(zEnd - z),&nLen) < 0 ){` |
|      9 |  241 | `			return 1;` |
|      - |  242 | `		}` |
|    ! 0 |  243 | `		z += nLen;` |
|    ! 0 |  244 | `	}` |
|      7 |  245 | `	return 0;` |
|      8 |  246 | `}` |
|  15468 |  247 | `static sxi32 VmJsonEncodeString(json_private_data *pData,const char *zIn,int nByte,int bKey)` |
|      5 |  248 | `{` |
|  15473 |  249 | `	ph7_context *pCtx = pData->pCtx;` |
|  15473 |  250 | `	int iFlags = pData->iFlags;` |
|  15473 |  251 | `	const char *zEnd = &zIn[nByte];` |
|      - |  252 | `	sxi32 rc;` |
|      - |  253 | `	char c;` |
|  15468 |  254 | `	if( (iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR) != 0` |
|   7741 |  255 | `	 && (iFlags & (JSON_INVALID_UTF8_IGNORE\|JSON_INVALID_UTF8_SUBSTITUTE)) == 0` |
|     19 |  256 | `	 && VmJsonStrHasBadUtf8(zIn,nByte) ){` |
|      9 |  257 | `		pCtx->pVm->json_rc = JSON_ERROR_UTF8;` |
|      6 |  258 | `		return bKey ? ph7_result_string(pCtx,"\"\"",2)` |
|      8 |  259 | `		            : ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|      - |  260 | `	}` |
|  15465 |  261 | `	rc = ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|  15465 |  262 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  263 | `		return rc;` |
|      - |  264 | `	}` |
|  55573 |  265 | `	for(;;){` |
| 112073 |  266 | `		if( zIn >= zEnd ){` |
|      - |  267 | `			/* No more input to process */` |
|  15377 |  268 | `			break;` |
|      - |  269 | `		}` |
|  96701 |  270 | `		if( (unsigned char)zIn[0] >= 0x80 ){` |
|      - |  271 | `			/* A UTF-8 sequence: decode it strictly, since \uXXXX needs the code` |
|      - |  272 | `			 * point and not the bytes. */` |
|      - |  273 | `			sxu32 nLen,cp;` |
|   1011 |  274 | `			sxi32 iCp = PH7_Utf8ReadStrict((const unsigned char *)zIn,(sxu32)(zEnd - zIn),&nLen);` |
|   1011 |  275 | `			if( iCp < 0 ){` |
|      - |  276 | `				/* Ill-formed. php REFUSES to encode it: json_encode returns` |
|      - |  277 | `				 * false with json_last_error() == JSON_ERROR_UTF8, because` |
|      - |  278 | `				 * there is no honest JSON spelling for a byte that is not` |
|      - |  279 | `				 * text. PHL used to pass the byte through, so the caller got a` |
|      - |  280 | `				 * valid-looking payload php would never have produced and no` |
|      - |  281 | `				 * error check could see it. The two JSON_INVALID_UTF8_* flags` |
|      - |  282 | `				 * are the opt-outs php offers. */` |
|    141 |  283 | `				nLen = VmJsonBadUtf8Len((const unsigned char *)zIn,(sxu32)(zEnd - zIn));` |
|    141 |  284 | `				if( iFlags & JSON_INVALID_UTF8_IGNORE ){` |
|     25 |  285 | `					rc = SXRET_OK; /* drop the run */` |
|    129 |  286 | `				}else if( iFlags & JSON_INVALID_UTF8_SUBSTITUTE ){` |
|     29 |  287 | `					rc = (iFlags & JSON_UNESCAPED_UNICODE)` |
|      2 |  288 | `						? ph7_result_string(pCtx,"\357\277\275",3) /* U+FFFD */` |
|     27 |  289 | `						: VmJsonEmitUnicodeEscape(pCtx,0xFFFD);` |
|     15 |  290 | `				}else{` |
|     89 |  291 | `					pData->fail = 1;` |
|     89 |  292 | `					pData->failRc = JSON_ERROR_UTF8;` |
|     89 |  293 | `					return SXRET_OK; /* the whole encode is discarded */` |
|      - |  294 | `				}` |
|     27 |  295 | `			}else{` |
|    871 |  296 | `				cp = (sxu32)iCp;` |
|    870 |  297 | `				if( (iFlags & JSON_UNESCAPED_UNICODE) == 0` |
|    574 |  298 | `				 \|\| ((cp == 0x2028 \|\| cp == 0x2029)` |
|    141 |  299 | `				  && (iFlags & JSON_UNESCAPED_LINE_TERMINATORS) == 0) ){` |
|    599 |  300 | `					rc = VmJsonEmitUnicodeEscape(pCtx,cp);` |
|    300 |  301 | `				}else{` |
|    273 |  302 | `					rc = ph7_result_string(pCtx,zIn,(int)nLen);` |
|      - |  303 | `				}` |
|      - |  304 | `			}` |
|    923 |  305 | `			zIn += nLen;` |
|    923 |  306 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  307 | `				return rc;` |
|      - |  308 | `			}` |
|    923 |  309 | `			continue;` |
|      - |  310 | `		}` |
|  95691 |  311 | `		c = zIn[0];` |
|      - |  312 | `		/* Advance the stream cursor */` |
|  95691 |  313 | `		zIn++;` |
|  95691 |  314 | `		if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|      - |  315 | `			/* All < and > are converted to \u003C and \u003E */` |
|      5 |  316 | `			if( c == '<' ){` |
|      3 |  317 | `				rc = ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|      2 |  318 | `			}else{` |
|      3 |  319 | `				rc = ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|      1 |  320 | `			}` |
|  95689 |  321 | `		}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|      - |  322 | `			/* All &s are converted to \u0026.  */` |
|      3 |  323 | `			rc = ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|  95686 |  324 | `		}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|      - |  325 | `			/* All ' are converted to \u0027.   */` |
|      3 |  326 | `			rc = ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|  95684 |  327 | `		}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|      - |  328 | `			/* All " are converted to \u0022. */` |
|      3 |  329 | `			rc = ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|  95682 |  330 | `		}else if( (unsigned char)c < 0x20 ){` |
|      - |  331 | `			/* Control characters (band A #4): php emits the short escapes for` |
|      - |  332 | `			 * \b \f \n \r \t and \u00xx for the rest — pre-fix these were` |
|      - |  333 | `			 * emitted RAW (invalid JSON). */` |
|      - |  334 | `			static const char zHex[] = "0123456789abcdef";` |
|    575 |  335 | `			char zEsc[6] = { '\\', 'u', '0', '0', 0, 0 };` |
|    575 |  336 | `			switch(c){` |
|    ! 0 |  337 | `			case '\b': rc = ph7_result_string(pCtx,"\\b",2); break;` |
|     13 |  338 | `			case '\f': rc = ph7_result_string(pCtx,"\\f",2); break;` |
|    329 |  339 | `			case '\n': rc = ph7_result_string(pCtx,"\\n",2); break;` |
|     56 |  340 | `			case '\r': rc = ph7_result_string(pCtx,"\\r",2); break;` |
|     30 |  341 | `			case '\t': rc = ph7_result_string(pCtx,"\\t",2); break;` |
|     76 |  342 | `			default:` |
|    153 |  343 | `				zEsc[4] = zHex[(c >> 4) & 0x0F];` |
|    153 |  344 | `				zEsc[5] = zHex[c & 0x0F];` |
|    153 |  345 | `				rc = ph7_result_string(pCtx,zEsc,6);` |
|    152 |  346 | `				break;` |
|      - |  347 | `			}` |
|    289 |  348 | `		}else{` |
|  95109 |  349 | `			if( c == '"' \|\| c == '\\' ){` |
|      - |  350 | `				/* Escape the quote/backslash (php escapes the backslash` |
|      - |  351 | `				 * unconditionally — the old code wrongly tied it to` |
|      - |  352 | `				 * JSON_UNESCAPED_SLASHES, which governs '/' below) */` |
|   1137 |  353 | `				rc = ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|  94543 |  354 | `			}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){` |
|      - |  355 | `				/* php escapes forward slashes by default */` |
|    793 |  356 | `				rc = ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|    398 |  357 | `			}else{` |
|  93187 |  358 | `				rc = SXRET_OK;` |
|      - |  359 | `			}` |
|  95109 |  360 | `			if( rc == SXRET_OK ){` |
|      - |  361 | `				/* Append character verbatim */` |
|  95109 |  362 | `				rc = ph7_result_string(pCtx,&c,(int)sizeof(char));` |
|  47552 |  363 | `			}` |
|      - |  364 | `		}` |
|  95691 |  365 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  366 | `			return rc;` |
|      - |  367 | `		}` |
|      5 |  368 | `	}` |
|  15377 |  369 | `	return ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|   7739 |  370 | `}` |
|      - |  371 | `/*` |
|      - |  372 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|      - |  373 | ` * According to wikipedia` |
|      - |  374 | ` * JSON's basic types are:` |
|      - |  375 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|      - |  376 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|      - |  377 | ` *   Boolean (true or false)` |
|      - |  378 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|      - |  379 | ` *    do not need to be of the same type)` |
|      - |  380 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|      - |  381 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|      - |  382 | ` *     be distinct from each other)` |
|      - |  383 | ` *   null (empty)` |
|      - |  384 | ` * Non-significant white space may be added freely around the "structural characters"` |
|      - |  385 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|      - |  386 | ` */` |
|      - |  387 | `/*` |
|      - |  388 | ` * Encode a native class's PRESENTED shape, php's get_properties handler answering` |
|      - |  389 | ` * the JSON purpose. Answers 0 when the class declares no hook, so the caller falls` |
|      - |  390 | ` * through to the ordinary property walk.` |
|      - |  391 | ` *` |
|      - |  392 | ` * php emits an OBJECT here whatever the presented keys look like — an ArrayObject` |
|      - |  393 | `` * holding a plain list is `{"0":1,"1":2}`, never `[1,2]` — so the list test the`` |
|      - |  394 | ` * array arm makes is deliberately not made.` |
|      - |  395 | ` */` |
|    242 |  396 | `static int VmJsonPresent(ph7_class_instance *pThis,json_private_data *pData)` |
|      5 |  397 | `{` |
|    247 |  398 | `	ph7_context *pCtx = pData->pCtx;` |
|      - |  399 | `	ph7_value sPresent;` |
|      - |  400 | `	int savedObject,savedPresented;` |
|    247 |  401 | `	PH7_MemObjInit(pThis->pVm,&sPresent);` |
|    247 |  402 | `	if( PH7_MemObjToHashmap(&sPresent) != SXRET_OK ){` |
|    ! 0 |  403 | `		PH7_MemObjRelease(&sPresent);` |
|    ! 0 |  404 | `		return 0;` |
|      - |  405 | `	}` |
|    247 |  406 | `	if( !PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|    159 |  407 | `		PH7_MemObjRelease(&sPresent);` |
|    159 |  408 | `		return 0;` |
|      - |  409 | `	}` |
|     91 |  410 | `	savedObject = pData->isObject;` |
|     91 |  411 | `	savedPresented = pData->bPresented;` |
|     91 |  412 | `	pData->isObject = 1;` |
|     91 |  413 | `	pData->bPresented = 1;` |
|     91 |  414 | `	pData->isFirst = 1;` |
|     91 |  415 | `	JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|     91 |  416 | `	ph7_array_walk(&sPresent,VmJsonArrayEncode,pData);` |
|     91 |  417 | `	if( !pData->oom ){` |
|     91 |  418 | `		if( !pData->isFirst ){` |
|     60 |  419 | `			JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|     29 |  420 | `		}` |
|     91 |  421 | `		JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|     44 |  422 | `	}` |
|     91 |  423 | `	pData->isObject = savedObject;` |
|     91 |  424 | `	pData->bPresented = savedPresented;` |
|     91 |  425 | `	PH7_MemObjRelease(&sPresent);` |
|     91 |  426 | `	return 1;` |
|    126 |  427 | `}` |
|  23578 |  428 | `static sxi32 VmJsonEncode(` |
|      - |  429 | `	ph7_value *pIn,          /* Encode this value */` |
|      - |  430 | `	json_private_data *pData /* Context data */` |
|      5 |  431 | `	){` |
|  23583 |  432 | `		ph7_context *pCtx = pData->pCtx;` |
|  23583 |  433 | `		int iFlags = pData->iFlags;` |
|      - |  434 | `		int nByte;` |
|  23583 |  435 | `		if( ph7_value_is_resource(pIn) ){` |
|      - |  436 | `			/* php: a resource has no JSON representation — the whole encode` |
|      - |  437 | `			 * fails with JSON_ERROR_UNSUPPORTED_TYPE (PHL used to emit "null"` |
|      - |  438 | `			 * in silence, an answer php never gives). Under` |
|      - |  439 | `			 * JSON_PARTIAL_OUTPUT_ON_ERROR the substitute IS null, with the` |
|      - |  440 | `			 * error recorded. */` |
|      9 |  441 | `			if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){` |
|      3 |  442 | `				pCtx->pVm->json_rc = JSON_ERROR_UNSUPPORTED_TYPE;` |
|      3 |  443 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|      3 |  444 | `				return PH7_OK;` |
|      - |  445 | `			}` |
|      7 |  446 | `			pData->fail = 1;` |
|      7 |  447 | `			pData->failRc = JSON_ERROR_UNSUPPORTED_TYPE;` |
|      7 |  448 | `			return PH7_OK;` |
|  23575 |  449 | `		}else if( ph7_value_is_null(pIn) ){` |
|      - |  450 | `			/* null */` |
|    254 |  451 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|  23449 |  452 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   1947 |  453 | `			int iBool = ph7_value_to_bool(pIn);` |
|      - |  454 | `			int iLen;` |
|      - |  455 | `			/* true/false */` |
|   1947 |  456 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   1947 |  457 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
|  22354 |  458 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|   7409 |  459 | `			if( ph7_value_is_float(pIn) ){` |
|    225 |  460 | `				double rVal = ph7_value_to_double(pIn);` |
|      - |  461 | `				/* php rejects Inf/NaN: json_encode returns FALSE with` |
|      - |  462 | `				 * json_last_error() == JSON_ERROR_INF_OR_NAN (they have no JSON` |
|      - |  463 | `				 * representation), instead of emitting the invalid bare token. */` |
|    225 |  464 | `				if( PH7_IS_NAN(rVal) \|\| PH7_IS_INF(rVal) ){` |
|     19 |  465 | `					if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){` |
|      - |  466 | `						/* php's substitute for an Inf/NaN member is 0 */` |
|     11 |  467 | `						pCtx->pVm->json_rc = JSON_ERROR_INF_OR_NAN;` |
|     11 |  468 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"0",(int)sizeof(char)));` |
|     11 |  469 | `						return PH7_OK;` |
|      - |  470 | `					}` |
|      9 |  471 | `					pData->fail = 1;` |
|      9 |  472 | `					pData->failRc = JSON_ERROR_INF_OR_NAN;` |
|      9 |  473 | `					return PH7_OK;` |
|      - |  474 | `				}` |
|      - |  475 | `				/* php's json float output follows serialize_precision` |
|      - |  476 | `				 * (shortest round-trip, like serialize/var_export), NOT the` |
|      - |  477 | `				 * echo/cast precision of 14 — with a lowercase exponent` |
|      - |  478 | `				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,` |
|      - |  479 | `				 * 1.0 -> 1, -0.0 -> -0. */` |
|    207 |  480 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,rVal,iFlags));` |
|    105 |  481 | `			}else{` |
|      - |  482 | `				const char *zNum;` |
|      - |  483 | `				/* Get a string representation of the number */` |
|   4725 |  484 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|   4725 |  485 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|      5 |  486 | `			}` |
|  18903 |  487 | `		}else if( ph7_value_is_string(pIn) ){` |
|   7445 |  488 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|      - |  489 | `				/* Encodes numeric strings as numbers (same float shapes). */` |
|      9 |  490 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|      9 |  491 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn),iFlags));` |
|      5 |  492 | `			}else{` |
|      - |  493 | `				const char *zIn;` |
|      - |  494 | `				/* Encode the string */` |
|   7437 |  495 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|   7437 |  496 | `				JSON_EMIT(pData,VmJsonEncodeString(pData,zIn,nByte,0));` |
|      5 |  497 | `			}` |
|  12721 |  498 | `		}else if( ph7_value_is_array(pIn) ){` |
|      - |  499 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|      - |  500 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|      - |  501 | `			 * object with stringified keys (PHP semantics). */` |
|   8707 |  502 | `			ph7_hashmap *pMap = (ph7_hashmap *)pIn->x.pOther;` |
|  17408 |  503 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|   8702 |  504 | `				\|\| !PH7_HashmapIsList(pMap);` |
|   8707 |  505 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|   8707 |  506 | `			int c = isObject ? '{' : '[';` |
|   8707 |  507 | `			int d = isObject ? '}' : ']';` |
|      - |  508 | `			/* An array the encoder is already inside of (reached through a` |
|      - |  509 | `			 * reference cycle) is php's JSON_ERROR_RECURSION — PHL used to` |
|      - |  510 | `			 * descend into it and answer a TRUNCATED nesting in silence.` |
|      - |  511 | `			 * JSON_PARTIAL_OUTPUT_ON_ERROR substitutes null for the cycle. */` |
|   8707 |  512 | `			if( VmJsonPathHolds(pData,(void *)pMap) ){` |
|      5 |  513 | `				if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){` |
|      3 |  514 | `					pCtx->pVm->json_rc = JSON_ERROR_RECURSION;` |
|      9 |  515 | `					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|      3 |  516 | `					return PH7_OK;` |
|      - |  517 | `				}` |
|      3 |  518 | `				pData->fail = 1;` |
|      3 |  519 | `				pData->failRc = JSON_ERROR_RECURSION;` |
|      3 |  520 | `				return PH7_OK;` |
|      - |  521 | `			}` |
|      - |  522 | `			/* php checks $depth where a container OPENS: one already enclosed` |
|      - |  523 | `			 * by $depth containers (scalars are exempt) is JSON_ERROR_DEPTH.` |
|      - |  524 | `			 * Under JSON_PARTIAL_OUTPUT_ON_ERROR php records the error and` |
|      - |  525 | `			 * keeps ENCODING past the limit — PHL follows until the stack` |
|      - |  526 | `			 * ceiling, where a null stands in for what it will not recurse` |
|      - |  527 | `			 * into. */` |
|   8703 |  528 | `			if( (ph7_int64)pData->nRecCount >= pData->nMaxDepth ){` |
|     11 |  529 | `				if( (iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR) == 0 ){` |
|      9 |  530 | `					pData->fail = 1;` |
|      9 |  531 | `					pData->failRc = JSON_ERROR_DEPTH;` |
|      9 |  532 | `					return PH7_OK;` |
|      - |  533 | `				}` |
|      3 |  534 | `				pCtx->pVm->json_rc = JSON_ERROR_DEPTH;` |
|      3 |  535 | `				if( pData->nRecCount >= PH7_JSON_DEPTH_CEILING ){` |
|    ! 0 |  536 | `					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    ! 0 |  537 | `					return PH7_OK;` |
|      - |  538 | `				}` |
|      1 |  539 | `			}` |
|   8695 |  540 | `			if( SySetPut(&pData->aPath,(const void *)&pMap) != SXRET_OK ){` |
|    ! 0 |  541 | `				pData->oom = 1;` |
|    ! 0 |  542 | `				return PH7_OK;` |
|      - |  543 | `			}` |
|      - |  544 | `			/* Encode the array */` |
|   8695 |  545 | `			pData->isObject = isObject;` |
|   8695 |  546 | `			pData->isFirst = 1;` |
|      - |  547 | `			/* Append the square bracket or curly braces */` |
|   8695 |  548 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|      - |  549 | `			/* Iterate throw array entries */` |
|   8695 |  550 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|   8695 |  551 | `			(void)SySetPop(&pData->aPath);` |
|      - |  552 | `			/* Bail if a nested append ran out of memory before the closer */` |
|   8695 |  553 | `			if( pData->oom ){` |
|    ! 0 |  554 | `				return PH7_OK;` |
|      - |  555 | `			}` |
|      - |  556 | `			/* Pretty-print: a non-empty container closes on its own line,` |
|      - |  557 | `			 * indented one level less than its members (isFirst is still 1` |
|      - |  558 | `			 * only when no entry was emitted -> keep "[]"/"{}" tight). */` |
|   8695 |  559 | `			if( !pData->isFirst ){` |
|   7643 |  560 | `				JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|   3819 |  561 | `			}` |
|      - |  562 | `			/* Append the closing square bracket or curly braces */` |
|   8695 |  563 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|   8695 |  564 | `			pData->isObject = savedObject;` |
|   4644 |  565 | `		}else if( ph7_value_is_object(pIn) ){` |
|    299 |  566 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|    299 |  567 | `			ph7_vm *pVm = pIn->pVm;` |
|    299 |  568 | `			ph7_class_method *pMethod = 0;` |
|    299 |  569 | `			int bProps = 1; /* encode the property view below (cleared when a` |
|      - |  570 | `			                 * jsonSerialize() result replaces it) */` |
|      - |  571 | `			/* An object the encoder is already inside of is php's` |
|      - |  572 | `			 * JSON_ERROR_RECURSION, checked BEFORE the jsonSerialize dispatch` |
|      - |  573 | `			 * (PHL used to re-dispatch until the C stack ran out — a segfault` |
|      - |  574 | ``			 * on `return $this;`). */`` |
|    299 |  575 | `			if( VmJsonPathHolds(pData,(void *)pThis) ){` |
|      5 |  576 | `				if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){` |
|    ! 0 |  577 | `					pCtx->pVm->json_rc = JSON_ERROR_RECURSION;` |
|     12 |  578 | `					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    ! 0 |  579 | `					return PH7_OK;` |
|      - |  580 | `				}` |
|      5 |  581 | `				pData->fail = 1;` |
|      5 |  582 | `				pData->failRc = JSON_ERROR_RECURSION;` |
|      5 |  583 | `				return PH7_OK;` |
|      - |  584 | `			}` |
|      - |  585 | `			/* If the object implements JsonSerializable, encode the value` |
|      - |  586 | `			 * returned by jsonSerialize() instead of its public properties.` |
|      - |  587 | `			 * An enum implementing it explicitly also takes this path (php). */` |
|    290 |  588 | `			if( pVm->pJsonSerializableClass` |
|    295 |  589 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|     35 |  590 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|     17 |  591 | `			}` |
|    295 |  592 | `			if( pMethod == 0 && (pThis->pClass->iFlags & PH7_CLASS_ENUM) != 0 ){` |
|      - |  593 | `				/* php 8.1: a BACKED enum case encodes as its backing value; a` |
|      - |  594 | `				 * pure enum case has no default serialization — json_encode` |
|      - |  595 | `				 * returns false. */` |
|     15 |  596 | `				ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pThis);` |
|     15 |  597 | `				if( pBacking ){` |
|     11 |  598 | `					pData->nRecCount++;` |
|     11 |  599 | `					VmJsonEncode(pBacking,pData);` |
|     11 |  600 | `					pData->nRecCount--;` |
|     10 |  601 | `				}else if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){` |
|      - |  602 | `					/* php's substitute for a non-backed case is 0 */` |
|      3 |  603 | `					pCtx->pVm->json_rc = JSON_ERROR_NON_BACKED_ENUM;` |
|      3 |  604 | `					JSON_EMIT(pData,ph7_result_string(pCtx,"0",(int)sizeof(char)));` |
|      2 |  605 | `				}else{` |
|      3 |  606 | `					pData->fail = 1;` |
|      3 |  607 | `					pData->failRc = JSON_ERROR_NON_BACKED_ENUM;` |
|      - |  608 | `				}` |
|     15 |  609 | `				return PH7_OK;` |
|      - |  610 | `			}` |
|    281 |  611 | `			if( pMethod ){` |
|      - |  612 | `				ph7_value sResult;` |
|      - |  613 | `				sxi32 rc;` |
|     35 |  614 | `				PH7_MemObjInit(pVm,&sResult);` |
|     35 |  615 | `				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|     35 |  616 | `				if( rc == PH7_EXCEPTION ){` |
|      - |  617 | `					/* Let jsonSerialize()'s throw propagate */` |
|      5 |  618 | `					PH7_MemObjRelease(&sResult);` |
|      5 |  619 | `					pData->exc = 1;` |
|      5 |  620 | `					return PH7_EXCEPTION;` |
|      - |  621 | `				}` |
|     30 |  622 | `				if( ph7_value_is_object(&sResult)` |
|     17 |  623 | `				 && (ph7_class_instance *)sResult.x.pOther == pThis ){` |
|      - |  624 | `					/* php's one self-reference exception: jsonSerialize()` |
|      - |  625 | `					 * returning $this encodes the object's own property view —` |
|      - |  626 | `					 * no re-dispatch, no recursion error. */` |
|      3 |  627 | `					PH7_MemObjRelease(&sResult);` |
|      2 |  628 | `				}else{` |
|     29 |  629 | `					bProps = 0;` |
|      - |  630 | `					/* Encode the returned value [scalar/array/object]. The` |
|      - |  631 | `					 * object stays ON the path while its replacement encodes` |
|      - |  632 | ``					 * (`return [$this]` is php's recursion error), and the`` |
|      - |  633 | `					 * result sits at the object's own nesting level — the old` |
|      - |  634 | `					 * nRecCount++ here indented a JSON_PRETTY_PRINT result one` |
|      - |  635 | `					 * level deeper than php and would have charged $depth for a` |
|      - |  636 | `					 * container php does not charge. */` |
|     29 |  637 | `					if( SySetPut(&pData->aPath,(const void *)&pThis) != SXRET_OK ){` |
|    ! 0 |  638 | `						PH7_MemObjRelease(&sResult);` |
|    ! 0 |  639 | `						pData->oom = 1;` |
|    ! 0 |  640 | `						return PH7_OK;` |
|      - |  641 | `					}` |
|     29 |  642 | `					VmJsonEncode(&sResult,pData);` |
|     29 |  643 | `					(void)SySetPop(&pData->aPath);` |
|     29 |  644 | `					PH7_MemObjRelease(&sResult);` |
|     29 |  645 | `					if( pData->exc ){` |
|    ! 0 |  646 | `						return PH7_EXCEPTION;` |
|      - |  647 | `					}` |
|     29 |  648 | `					if( pData->oom ){` |
|    ! 0 |  649 | `						return PH7_OK;` |
|      - |  650 | `					}` |
|      - |  651 | `				}` |
|     15 |  652 | `			}` |
|      - |  653 | `			/* php checks $depth where a container OPENS — the '{' of the` |
|      - |  654 | `			 * property view below; a SCALAR jsonSerialize() result and an enum` |
|      - |  655 | `			 * backing value are exempt, so the check sits here and not at the` |
|      - |  656 | `			 * arm's entry. The PARTIAL_OUTPUT rule mirrors the array arm's:` |
|      - |  657 | `			 * record the error, keep encoding, null at the stack ceiling. */` |
|    277 |  658 | `			if( bProps && (ph7_int64)pData->nRecCount >= pData->nMaxDepth ){` |
|      3 |  659 | `				if( (iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR) == 0 ){` |
|      3 |  660 | `					pData->fail = 1;` |
|      3 |  661 | `					pData->failRc = JSON_ERROR_DEPTH;` |
|      3 |  662 | `					return PH7_OK;` |
|      - |  663 | `				}` |
|    ! 0 |  664 | `				pCtx->pVm->json_rc = JSON_ERROR_DEPTH;` |
|    ! 0 |  665 | `				if( pData->nRecCount >= PH7_JSON_DEPTH_CEILING ){` |
|    ! 0 |  666 | `					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    ! 0 |  667 | `					return PH7_OK;` |
|      - |  668 | `				}` |
|    ! 0 |  669 | `			}` |
|    275 |  670 | `			if( bProps && SySetPut(&pData->aPath,(const void *)&pThis) != SXRET_OK ){` |
|    ! 0 |  671 | `				pData->oom = 1;` |
|    ! 0 |  672 | `				return PH7_OK;` |
|      - |  673 | `			}` |
|    275 |  674 | `			if( !bProps ){` |
|      - |  675 | `				/* jsonSerialize()'s result replaced the property view above */` |
|    261 |  676 | `			}else if( VmJsonPresent(pThis,pData) ){` |
|      - |  677 | `				/* A native class with php's get_properties handler: json is one of` |
|      - |  678 | `				 * the purposes that handler serves (php's ZEND_PROP_PURPOSE_JSON),` |
|      - |  679 | `				 * so a DateTime encodes as date/timezone_type/timezone and an` |
|      - |  680 | `				 * ArrayObject as its ELEMENTS — where walking the real slots below` |
|      - |  681 | `				 * finds nothing, every one of them being a hidden engine slot.` |
|      - |  682 | `				 * Handled inside VmJsonPresent so this arm is just the dispatch. */` |
|     91 |  683 | `				if( pData->exc ){` |
|    ! 0 |  684 | `					return PH7_EXCEPTION;` |
|      - |  685 | `				}` |
|     47 |  686 | `			}else{` |
|      - |  687 | `				SyHashEntry *pAttrEntry;` |
|      - |  688 | `				SySet sNames;` |
|      - |  689 | `				SyString *aName;` |
|      - |  690 | `				sxu32 iName,nName;` |
|      - |  691 | `				/* Encode the class instance: php serializes only PUBLIC` |
|      - |  692 | `				 * non-static properties, reading through a PHP 8.4 get hook` |
|      - |  693 | `				 * when one is declared (virtual properties included). The` |
|      - |  694 | `				 * names are SNAPSHOTTED first — a hook dispatched mid-walk may` |
|      - |  695 | `				 * re-enter an hAttr walk on this instance (the hash has a` |
|      - |  696 | `				 * single embedded loop cursor) or unset()/create properties;` |
|      - |  697 | `				 * names point into class-owned attr storage and each is` |
|      - |  698 | `				 * re-looked-up before use. */` |
|    159 |  699 | `				pData->isFirst = 1;` |
|      - |  700 | `				/* Append the curly braces */` |
|    159 |  701 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|    159 |  702 | `				SySetInit(&sNames,&pVm->sAllocator,sizeof(SyString));` |
|    159 |  703 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|    527 |  704 | `				while( (pAttrEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    372 |  705 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|    368 |  706 | `					if( PH7_ATTR_UNPRESENTED(pVmAttr)` |
|    310 |  707 | `					 \|\| pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     78 |  708 | `						continue;` |
|      - |  709 | `					}` |
|    298 |  710 | `					if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|     28 |  711 | `						continue; /* typed, never written: not there yet (php) */` |
|      - |  712 | `					}` |
|    268 |  713 | `					if( SyStringLength(&pVmAttr->pAttr->sName) > 0` |
|    271 |  714 | `					 && SyStringData(&pVmAttr->pAttr->sName)[0] == 0 ){` |
|      - |  715 | `						/* A MANGLED key stored raw (the __PHP_Incomplete_Class` |
|      - |  716 | `						 * carrier): php's json encoder reads it as non-public` |
|      - |  717 | `						 * and skips it. */` |
|      5 |  718 | `						continue;` |
|      - |  719 | `					}` |
|    264 |  720 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|    136 |  721 | `					 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|    ! 0 |  722 | `						continue; /* virtual set-only property: no value to encode (php) */` |
|      - |  723 | `					}` |
|    268 |  724 | `					SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|      4 |  725 | `				}` |
|    159 |  726 | `				aName = (SyString *)SySetBasePtr(&sNames);` |
|    159 |  727 | `				nName = SySetUsed(&sNames);` |
|    423 |  728 | `				for( iName = 0 ; iName < nName ; ++iName ){` |
|      - |  729 | `					VmClassAttr *pVmAttr;` |
|    268 |  730 | `					ph7_value *pAttrVal = 0;` |
|      - |  731 | `					ph7_value sHookVal;` |
|      - |  732 | `					sxi32 rcHk;` |
|    268 |  733 | `					pAttrEntry = PH7_ClassInstanceAttrEntry(pThis,aName[iName].zString,aName[iName].nByte);` |
|    268 |  734 | `					if( pAttrEntry == 0 ){` |
|    ! 0 |  735 | `						continue; /* unset by an earlier hook */` |
|      - |  736 | `					}` |
|    268 |  737 | `					pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|    268 |  738 | `					PH7_MemObjInit(pVm,&sHookVal);` |
|    268 |  739 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|    268 |  740 | `					if( rcHk == SXRET_OK ){` |
|     11 |  741 | `						pAttrVal = &sHookVal;` |
|    263 |  742 | `					}else if( rcHk == SXERR_NOTFOUND ){` |
|      - |  743 | `						/* Encode a COPY: the encoder casts scalars in place` |
|      - |  744 | `						 * (ph7_value_to_string), which must not corrupt the` |
|      - |  745 | `						 * live attribute slot. */` |
|    258 |  746 | `						ph7_value *pRaw = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    258 |  747 | `						if( pRaw ){` |
|    258 |  748 | `							PH7_MemObjStore(pRaw,&sHookVal);` |
|    258 |  749 | `							pAttrVal = &sHookVal;` |
|    127 |  750 | `						}` |
|    131 |  751 | `					}else{` |
|      - |  752 | `						/* the get hook threw — propagate like jsonSerialize() */` |
|    ! 0 |  753 | `						PH7_MemObjRelease(&sHookVal);` |
|    ! 0 |  754 | `						SySetRelease(&sNames);` |
|    ! 0 |  755 | `						pData->exc = 1;` |
|    ! 0 |  756 | `						return PH7_EXCEPTION;` |
|      - |  757 | `					}` |
|    268 |  758 | `					if( pAttrVal ){` |
|    268 |  759 | `						VmJsonObjectEncode(&pVmAttr->pAttr->sName,pAttrVal,pData);` |
|    132 |  760 | `					}` |
|    268 |  761 | `					PH7_MemObjRelease(&sHookVal);` |
|    268 |  762 | `					if( pData->exc ){` |
|    ! 0 |  763 | `						SySetRelease(&sNames);` |
|    ! 0 |  764 | `						return PH7_EXCEPTION; /* a nested jsonSerialize()/hook threw */` |
|      - |  765 | `					}` |
|    268 |  766 | `					if( pData->oom ){` |
|    ! 0 |  767 | `						SySetRelease(&sNames);` |
|    ! 0 |  768 | `						return PH7_OK;` |
|      - |  769 | `					}` |
|    136 |  770 | `				}` |
|    159 |  771 | `				SySetRelease(&sNames);` |
|      - |  772 | `				/* Pretty-print: non-empty object closes on its own indented line. */` |
|    159 |  773 | `				if( !pData->isFirst ){` |
|    130 |  774 | `					JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|     63 |  775 | `				}` |
|      - |  776 | `				/* Append the closing curly braces  */` |
|    159 |  777 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|      - |  778 | `			}` |
|    275 |  779 | `			if( bProps ){` |
|    247 |  780 | `				(void)SySetPop(&pData->aPath);` |
|    121 |  781 | `			}` |
|    140 |  782 | `		}else{` |
|      - |  783 | `			/* Can't happen */` |
|    ! 0 |  784 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|      - |  785 | `		}` |
|      - |  786 | `		/* All done */` |
|  23521 |  787 | `		return PH7_OK;` |
|  11794 |  788 | `}` |
|      - |  789 | `/*` |
|      - |  790 | ` * The following walker callback is invoked each time we need` |
|      - |  791 | ` * to encode an array to JSON.` |
|      - |  792 | ` */` |
|  17064 |  793 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 |  794 | `{` |
|  17069 |  795 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  17064 |  796 | `	if( pJson->bPresented && pJson->isObject && pKey` |
|   7792 |  797 | `	 && (pKey->iFlags & MEMOBJ_STRING) && SyBlobLength(&pKey->sBlob) > 0` |
|   6465 |  798 | `	 && ((const char *)SyBlobData(&pKey->sBlob))[0] == 0 ){` |
|     21 |  799 | `		return PH7_OK;   /* a non-public property of the object behind the shape */` |
|      - |  800 | `	}` |
|  17049 |  801 | `	if( pJson->exc \|\| pJson->oom \|\| pJson->fail ){` |
|      - |  802 | `		/* A callback threw, OOM, or the value is unencodable (the result is` |
|      - |  803 | `		 * discarded) — return immediately. Depth is no longer decided here:` |
|      - |  804 | `		 * the container arms enforce json_encode's $depth where a '['/'{'` |
|      - |  805 | `		 * opens (the old flat 31 cap TRUNCATED a deep value in silence). */` |
|     69 |  806 | `		return PH7_OK;` |
|      - |  807 | `	}` |
|  16981 |  808 | `	if( !pJson->isFirst ){` |
|      - |  809 | `		/* Append the comma separating this entry from the previous one */` |
|   9285 |  810 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   4640 |  811 | `	}` |
|      - |  812 | `	/* Pretty-print: every member starts on its own indented line (one level` |
|      - |  813 | `	 * deeper than the enclosing container). */` |
|  16981 |  814 | `	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));` |
|  16981 |  815 | `	if( pJson->isObject ){` |
|      - |  816 | `		/* Outputs an object rather than an array */` |
|      - |  817 | `		const char *zKey;` |
|      - |  818 | `		int nByte;` |
|      - |  819 | `		/* Extract a string representation of the key */` |
|   7777 |  820 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|      - |  821 | `		/* Append the quoted key and the colon. The key goes through the same` |
|      - |  822 | `		 * escaper as a string VALUE (php escapes both identically): emitting it` |
|      - |  823 | `		 * raw produced invalid JSON for any key holding '"', '\' or a control` |
|      - |  824 | `		 * character. */` |
|   7777 |  825 | `		JSON_EMIT(pJson,VmJsonEncodeString(pJson,zKey,nByte,1));` |
|   7777 |  826 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,":",(int)sizeof(char)));` |
|      - |  827 | `		/* php puts a space after the colon in pretty mode */` |
|   7777 |  828 | `		if( pJson->iFlags & JSON_PRETTY_PRINT ){` |
|     61 |  829 | `			JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));` |
|     29 |  830 | `		}` |
|   3886 |  831 | `	}` |
|      - |  832 | `	/* Encode the value */` |
|  16981 |  833 | `	pJson->nRecCount++;` |
|  16981 |  834 | `	VmJsonEncode(pValue,pJson);` |
|  16981 |  835 | `	pJson->nRecCount--;` |
|  16981 |  836 | `	pJson->isFirst = 0;` |
|  16981 |  837 | `	return PH7_OK;` |
|   8537 |  838 | `}` |
|      - |  839 | `/*` |
|      - |  840 | ` * The following walker callback is invoked each time we need to encode` |
|      - |  841 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|      - |  842 | ` */` |
|    264 |  843 | `static int VmJsonObjectEncode(const SyString *pAttr,ph7_value *pValue,void *pUserData)` |
|      4 |  844 | `{` |
|    268 |  845 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|    268 |  846 | `	if( pJson->exc \|\| pJson->oom \|\| pJson->fail ){` |
|      - |  847 | `		/* A callback threw, OOM, or the value is unencodable (the result is` |
|      - |  848 | `		 * discarded) — return immediately. Depth is no longer decided here:` |
|      - |  849 | `		 * the container arms enforce json_encode's $depth where a '['/'{'` |
|      - |  850 | `		 * opens (the old flat 31 cap TRUNCATED a deep value in silence). */` |
|    ! 0 |  851 | `		return PH7_OK;` |
|      - |  852 | `	}` |
|    268 |  853 | `	if( !pJson->isFirst ){` |
|      - |  854 | `		/* Append the comma separating this entry from the previous one */` |
|    142 |  855 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|     69 |  856 | `	}` |
|      - |  857 | `	/* Pretty-print: member on its own indented line, one level deeper. */` |
|    268 |  858 | `	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));` |
|      - |  859 | `	/* Append the quoted attribute name and the colon — escaped like a string` |
|      - |  860 | `	 * value, same as the array-key path above. */` |
|    268 |  861 | `	JSON_EMIT(pJson,VmJsonEncodeString(pJson,SyStringData(pAttr),(int)SyStringLength(pAttr),1));` |
|    268 |  862 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,":",(int)sizeof(char)));` |
|      - |  863 | `	/* php puts a space after the colon in pretty mode */` |
|    268 |  864 | `	if( pJson->iFlags & JSON_PRETTY_PRINT ){` |
|      9 |  865 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));` |
|      4 |  866 | `	}` |
|      - |  867 | `	/* Encode the value */` |
|    268 |  868 | `	pJson->nRecCount++;` |
|    268 |  869 | `	VmJsonEncode(pValue,pJson);` |
|    268 |  870 | `	pJson->nRecCount--;` |
|    268 |  871 | `	pJson->isFirst = 0;` |
|    268 |  872 | `	return PH7_OK;` |
|    136 |  873 | `}` |
|      - |  874 | `/*` |
|      - |  875 | ` * string json_encode(mixed $value [, int $flags = 0 [, int $depth = 512 ]])` |
|      - |  876 | ` *  Returns a string containing the JSON representation of value.` |
|      - |  877 | ` * Parameters` |
|      - |  878 | ` *  $value` |
|      - |  879 | ` *  The value being encoded. Can be any type except a resource` |
|      - |  880 | ` *  (a resource is JSON_ERROR_UNSUPPORTED_TYPE).` |
|      - |  881 | ` * $options` |
|      - |  882 | ` *  Bitmask consisting of:` |
|      - |  883 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|      - |  884 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|      - |  885 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|      - |  886 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|      - |  887 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|      - |  888 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|      - |  889 | ` *  JSON_BIGINT_AS_STRING   Decode flag (large ints as strings), not an encode flag.` |
|      - |  890 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|      - |  891 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|      - |  892 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|      - |  893 | ` * Return` |
|      - |  894 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|      - |  895 | ` */` |
|      - |  896 | `static const char * JsonErrorMsg(int rc); /* defined below, near json_last_error_msg */` |
|   6300 |  897 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  898 | `{` |
|      - |  899 | `	json_private_data sJson;` |
|      - |  900 | `	sxi32 rc;` |
|   6305 |  901 | `	if( nArg < 1 ){` |
|      - |  902 | `		/* Missing arguments,return FALSE */` |
|    ! 0 |  903 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  904 | `		return PH7_OK;` |
|      - |  905 | `	}` |
|      - |  906 | `	/* Prepare the JSON data */` |
|   6305 |  907 | `	sJson.nRecCount = 0;` |
|   6305 |  908 | `	sJson.pCtx = pCtx;` |
|   6305 |  909 | `	sJson.isFirst = 1;` |
|   6305 |  910 | `	sJson.iFlags = 0;` |
|   6305 |  911 | `	sJson.exc = 0;` |
|   6305 |  912 | `	sJson.oom = 0;` |
|   6305 |  913 | `	sJson.fail = 0;` |
|   6305 |  914 | `	sJson.failRc = JSON_ERROR_NON_BACKED_ENUM;` |
|   6305 |  915 | `	sJson.nMaxDepth = 512; /* php's default */` |
|   6305 |  916 | `	SySetInit(&sJson.aPath,&pCtx->pVm->sAllocator,sizeof(void *));` |
|   6305 |  917 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - |  918 | `		/* Extract option flags */` |
|    635 |  919 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|    316 |  920 | `	}` |
|   6305 |  921 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      - |  922 | `		/* $depth. Unlike json_decode's, php runs NO range screen here: 0 or a` |
|      - |  923 | `		 * negative value simply makes every container JSON_ERROR_DEPTH, and any` |
|      - |  924 | `		 * large int is accepted (the type screen has already run). */` |
|     27 |  925 | `		sJson.nMaxDepth = ph7_value_to_int64(apArg[2]);` |
|     27 |  926 | `		if( sJson.nMaxDepth > PH7_JSON_DEPTH_CEILING ){` |
|      - |  927 | `			/* Engine stack-safety bound (see PH7_JSON_DEPTH_CEILING). */` |
|    ! 0 |  928 | `			sJson.nMaxDepth = PH7_JSON_DEPTH_CEILING;` |
|    ! 0 |  929 | `		}` |
|     13 |  930 | `	}` |
|   6305 |  931 | `	pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|      - |  932 | `	/* Perform the encoding operation */` |
|   6305 |  933 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|   6305 |  934 | `	SySetRelease(&sJson.aPath);` |
|   6305 |  935 | `	if( sJson.oom ){` |
|      - |  936 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|      - |  937 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|    ! 0 |  938 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  939 | `	}` |
|   6305 |  940 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|      - |  941 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|      5 |  942 | `		return PH7_EXCEPTION;` |
|      - |  943 | `	}` |
|   6301 |  944 | `	if( sJson.fail ){` |
|      - |  945 | `		/* Unencodable value (Inf/NaN, or a php 8.1 non-backed enum case): the` |
|      - |  946 | `		 * whole encode fails — discard whatever was emitted and return FALSE. */` |
|    121 |  947 | `		pCtx->pVm->json_rc = sJson.failRc;` |
|    121 |  948 | `		if( sJson.iFlags & JSON_THROW_ON_ERROR ){` |
|      - |  949 | `			/* php: raise a JsonException carrying json_last_error_msg() instead` |
|      - |  950 | `			 * of returning FALSE. */` |
|     10 |  951 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|      6 |  952 | `				(sxi32)pCtx->pVm->json_rc,"%s",` |
|      6 |  953 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|      - |  954 | `		}` |
|    115 |  955 | `		ph7_result_bool(pCtx,0);` |
|    115 |  956 | `		return PH7_OK;` |
|      - |  957 | `	}` |
|      - |  958 | `	/* All done */` |
|   6181 |  959 | `	return PH7_OK;` |
|   3155 |  960 | `}` |
|      - |  961 | `#undef JSON_EMIT` |
|      - |  962 | `/*` |
|      - |  963 | ` * int json_last_error(void)` |
|      - |  964 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|      - |  965 | ` * Parameters` |
|      - |  966 | ` *  None` |
|      - |  967 | ` * Return` |
|      - |  968 | ` *  Returns an integer, the value can be one of the following constants:` |
|      - |  969 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|      - |  970 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|      - |  971 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|      - |  972 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|      - |  973 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|      - |  974 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|      - |  975 | ` */` |
|    248 |  976 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  977 | `{` |
|    251 |  978 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  979 | `	/* Return the error code */` |
|    251 |  980 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    124 |  981 | `	SXUNUSED(nArg); /* cc warning */` |
|    124 |  982 | `	SXUNUSED(apArg);` |
|    251 |  983 | `	return PH7_OK;` |
|      3 |  984 | `}` |
|      - |  985 | `/*` |
|      - |  986 | ` * string json_last_error_msg(void)` |
|      - |  987 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|      - |  988 | ` * Parameters` |
|      - |  989 | ` *  None` |
|      - |  990 | ` * Return` |
|      - |  991 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|      - |  992 | ` *  code, or "No error" if no error has occurred.` |
|      - |  993 | ` */` |
|      - |  994 | `/* Human-readable message for a json_rc code. Shared by json_last_error_msg()` |
|      - |  995 | ` * and the JSON_THROW_ON_ERROR path (php's JsonException message is exactly this` |
|      - |  996 | ` * text). */` |
|     72 |  997 | `static const char * JsonErrorMsg(int rc)` |
|      2 |  998 | `{` |
|     74 |  999 | `	switch( rc ){` |
|     31 | 1000 | `	case JSON_ERROR_NONE:            return "No error";` |
|    ! 0 | 1001 | `	case JSON_ERROR_DEPTH:           return "Maximum stack depth exceeded";` |
|    ! 0 | 1002 | `	case JSON_ERROR_STATE_MISMATCH:  return "State mismatch (invalid or malformed JSON)";` |
|      3 | 1003 | `	case JSON_ERROR_CTRL_CHAR:       return "Control character error, possibly incorrectly encoded";` |
|     18 | 1004 | `	case JSON_ERROR_SYNTAX:          return "Syntax error";` |
|      7 | 1005 | `	case JSON_ERROR_UTF8:            return "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|      3 | 1006 | `	case JSON_ERROR_RECURSION:       return "Recursion detected";` |
|      3 | 1007 | `	case JSON_ERROR_INF_OR_NAN:     return "Inf and NaN cannot be JSON encoded";` |
|      5 | 1008 | `	case JSON_ERROR_UNSUPPORTED_TYPE: return "Type is not supported";` |
|      3 | 1009 | `	case JSON_ERROR_INVALID_PROPERTY_NAME: return "The decoded property name is invalid";` |
|      9 | 1010 | `	case JSON_ERROR_UTF16:           return "Single unpaired UTF-16 surrogate in unicode escape";` |
|    ! 0 | 1011 | `	case JSON_ERROR_NON_BACKED_ENUM: return "Non-backed enums have no default serialization";` |
|    ! 0 | 1012 | `	default:                         return "Unknown error";` |
|      - | 1013 | `	}` |
|     38 | 1014 | `}` |
|     60 | 1015 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1016 | `{` |
|     62 | 1017 | `	ph7_result_string(pCtx,JsonErrorMsg(pCtx->pVm->json_rc),-1/* auto length */);` |
|     30 | 1018 | `	SXUNUSED(nArg); /* cc warning */` |
|     30 | 1019 | `	SXUNUSED(apArg);` |
|     62 | 1020 | `	return PH7_OK;` |
|      2 | 1021 | `}` |
|      - | 1022 | `/* Possible tokens from the JSON tokenization process */` |
|      - | 1023 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|      - | 1024 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|      - | 1025 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|      - | 1026 | `#define JSON_TK_NULL    0x008 /* null */` |
|      - | 1027 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|      - | 1028 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|      - | 1029 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|      - | 1030 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|      - | 1031 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|      - | 1032 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|      - | 1033 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|      - | 1034 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|      - | 1035 | `/*` |
|      - | 1036 | ` * Tokenize an entire JSON input.` |
|      - | 1037 | ` * Get a single low-level token from the input file.` |
|      - | 1038 | ` * Update the stream pointer so that it points to the first` |
|      - | 1039 | ` * character beyond the extracted token.` |
|      - | 1040 | ` */` |
|   5490 | 1041 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|      4 | 1042 | `{` |
|   5494 | 1043 | `	int *pJsonErr = (int *)pUserData;` |
|      - | 1044 | `	SyString *pStr;` |
|      - | 1045 | `	int c;` |
|      - | 1046 | `	/* Ignore leading white spaces */` |
|   5556 | 1047 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|      - | 1048 | `		/* Advance the stream cursor */` |
|     64 | 1049 | `		if( pStream->zText[0] == '\n' ){` |
|      - | 1050 | `			/* Update line counter */` |
|      9 | 1051 | `			pStream->nLine++;` |
|      4 | 1052 | `		}` |
|     64 | 1053 | `		pStream->zText++;` |
|      2 | 1054 | `	}` |
|   5494 | 1055 | `	if( pStream->zText >= pStream->zEnd ){` |
|      - | 1056 | `		/* End of input reached */` |
|     12 | 1057 | `		SXUNUSED(pCtxData); /* cc warning */` |
|     26 | 1058 | `		return SXERR_EOF;` |
|      - | 1059 | `	}` |
|      - | 1060 | `	/* Record token starting position and line */` |
|   5470 | 1061 | `	pToken->nLine = pStream->nLine;` |
|   5470 | 1062 | `	pToken->pUserData = 0;` |
|   5470 | 1063 | `	pStr = &pToken->sData;` |
|   5470 | 1064 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|   5466 | 1065 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|   1719 | 1066 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|      - | 1067 | `			/* Single character */` |
|   5090 | 1068 | `			c = pStream->zText[0];` |
|      - | 1069 | `			/* Set token type */` |
|   5090 | 1070 | `			switch(c){` |
|   2418 | 1071 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|     84 | 1072 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|     67 | 1073 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|   2390 | 1074 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|     92 | 1075 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|     58 | 1076 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|    ! 0 | 1077 | `			default:` |
|    ! 0 | 1078 | `				break;` |
|      - | 1079 | `			}` |
|      - | 1080 | `			/* Advance the stream cursor */` |
|   5090 | 1081 | `			pStream->zText++;` |
|   2927 | 1082 | `	}else if( pStream->zText[0] == '"') {` |
|      - | 1083 | `		/* JSON string */` |
|    170 | 1084 | `		pStream->zText++;` |
|    170 | 1085 | `		pStr->zString++;` |
|      - | 1086 | `		/* Delimit the string. The backslash state is tracked explicitly: the old` |
|      - | 1087 | `		 * "the previous byte is not a backslash" test mis-read an ESCAPED` |
|      - | 1088 | `		 * backslash sitting before the closing quote, so the perfectly valid` |
|      - | 1089 | `		 * "\\" (a one-character string holding a backslash) was reported as an` |
|      - | 1090 | `		 * unterminated string — json_decode('"\\\\"') answered NULL with a` |
|      - | 1091 | `		 * syntax error where php answers "\". */` |
|    704 | 1092 | `		while( pStream->zText < pStream->zEnd ){` |
|    702 | 1093 | `			if( pStream->zText[0] == '\\' ){` |
|      - | 1094 | `				/* Whatever follows belongs to the escape, closing quote` |
|      - | 1095 | `				 * included; VmJsonDequoteString below decides if it is legal. */` |
|     91 | 1096 | `				pStream->zText++;` |
|     91 | 1097 | `				if( pStream->zText >= pStream->zEnd ){` |
|    ! 0 | 1098 | `					break;` |
|      - | 1099 | `				}` |
|     91 | 1100 | `				pStream->zText++;` |
|     91 | 1101 | `				continue;` |
|      - | 1102 | `			}` |
|    612 | 1103 | `			if( pStream->zText[0] == '"' ){` |
|    168 | 1104 | `				break;` |
|      - | 1105 | `			}` |
|    448 | 1106 | `			if( (unsigned char)pStream->zText[0] < 0x20 ){` |
|      - | 1107 | `				/* php: a control character must be escaped inside a JSON string;` |
|      - | 1108 | `				 * a raw one is JSON_ERROR_CTRL_CHAR (a literal newline included). */` |
|    ! 0 | 1109 | `				pToken->nType = JSON_TK_INVALID;` |
|    ! 0 | 1110 | `				*pJsonErr = JSON_ERROR_CTRL_CHAR;` |
|    ! 0 | 1111 | `				return SXERR_ABORT;` |
|      - | 1112 | `			}` |
|    448 | 1113 | `			pStream->zText++;` |
|      4 | 1114 | `		}` |
|    170 | 1115 | `		if( pStream->zText >= pStream->zEnd ){` |
|      - | 1116 | `			/* Missing closing '"'. php reports this as JSON_ERROR_CTRL_CHAR, not` |
|      - | 1117 | `			 * a syntax error: its scanner runs the string off the end of the` |
|      - | 1118 | `			 * input and lands in the same state an unescaped control character` |
|      - | 1119 | `			 * puts it in. */` |
|      3 | 1120 | `			pToken->nType = JSON_TK_INVALID;` |
|      3 | 1121 | `			*pJsonErr = JSON_ERROR_CTRL_CHAR;` |
|      2 | 1122 | `		}else{` |
|    168 | 1123 | `			pToken->nType = JSON_TK_STR;` |
|    168 | 1124 | `			pStream->zText++; /* Jump the closing double quotes */` |
|      - | 1125 | `		}` |
|    301 | 1126 | `	}else if( (pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]))` |
|    150 | 1127 | `		\|\| (pStream->zText[0] == '-' && &pStream->zText[1] < pStream->zEnd` |
|     22 | 1128 | `			&& pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1])) ){` |
|      - | 1129 | `		/* Number, held to JSON's grammar:` |
|      - | 1130 | `		 *   -?(0\|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?` |
|      - | 1131 | `		 * The old scanner took any digit soup, so "01", "-01", "5." and "1e"` |
|      - | 1132 | `		 * all DECODED (and json_validate() answered TRUE) where php reports` |
|      - | 1133 | `		 * JSON_ERROR_SYNTAX — accepting documents no JSON producer emits. */` |
|    184 | 1134 | `		int bBad = 0;` |
|    184 | 1135 | `		if( pStream->zText[0] == '-' ){` |
|     25 | 1136 | `			pStream->zText++;` |
|     11 | 1137 | `		}` |
|    184 | 1138 | `		if( pStream->zText[0] == '0' ){` |
|     21 | 1139 | `			pStream->zText++;` |
|      - | 1140 | `			/* JSON forbids a leading zero ahead of another digit */` |
|     21 | 1141 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     11 | 1142 | `				bBad = 1;` |
|      5 | 1143 | `			}` |
|     10 | 1144 | `		}` |
|    544 | 1145 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    364 | 1146 | `			pStream->zText++;` |
|      4 | 1147 | `		}` |
|    184 | 1148 | `		if( pStream->zText < pStream->zEnd && pStream->zText[0] == '.' ){` |
|     20 | 1149 | `			pStream->zText++;` |
|      - | 1150 | `			/* JSON requires at least one digit after the point */` |
|     20 | 1151 | `			if( pStream->zText >= pStream->zEnd \|\| pStream->zText[0] >= 0xc0 \|\| !SyisDigit(pStream->zText[0]) ){` |
|      5 | 1152 | `				bBad = 1;` |
|      2 | 1153 | `			}` |
|     36 | 1154 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     18 | 1155 | `				pStream->zText++;` |
|      2 | 1156 | `			}` |
|      9 | 1157 | `		}` |
|    184 | 1158 | `		if( pStream->zText < pStream->zEnd && (pStream->zText[0] == 'e' \|\| pStream->zText[0] == 'E') ){` |
|     21 | 1159 | `			pStream->zText++;` |
|     21 | 1160 | `			if( pStream->zText < pStream->zEnd && (pStream->zText[0] == '+' \|\| pStream->zText[0] == '-') ){` |
|      9 | 1161 | `				pStream->zText++;` |
|      4 | 1162 | `			}` |
|      - | 1163 | `			/* ...and at least one digit in the exponent */` |
|     21 | 1164 | `			if( pStream->zText >= pStream->zEnd \|\| pStream->zText[0] >= 0xc0 \|\| !SyisDigit(pStream->zText[0]) ){` |
|      7 | 1165 | `				bBad = 1;` |
|      3 | 1166 | `			}` |
|     35 | 1167 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     15 | 1168 | `				pStream->zText++;` |
|      1 | 1169 | `			}` |
|     10 | 1170 | `		}` |
|    184 | 1171 | `		if( bBad ){` |
|     21 | 1172 | `			pToken->nType = JSON_TK_INVALID;` |
|     21 | 1173 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|     21 | 1174 | `			return SXERR_ABORT;` |
|      - | 1175 | `		}` |
|    164 | 1176 | `		pToken->nType = JSON_TK_NUM;` |
|    125 | 1177 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|     18 | 1178 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|      - | 1179 | `			/* boolean true */` |
|      3 | 1180 | `			pToken->nType = JSON_TK_TRUE;` |
|      - | 1181 | `			/* Advance the stream cursor */` |
|      3 | 1182 | `			pStream->zText += sizeof("true")-1;` |
|     43 | 1183 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|     16 | 1184 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|      - | 1185 | `			/* boolean false */` |
|    ! 0 | 1186 | `			pToken->nType = JSON_TK_FALSE;` |
|      - | 1187 | `			/* Advance the stream cursor */` |
|    ! 0 | 1188 | `			pStream->zText += sizeof("false")-1;` |
|     42 | 1189 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|     16 | 1190 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|      - | 1191 | `			/* NULL */` |
|      3 | 1192 | `			pToken->nType = JSON_TK_NULL;` |
|      - | 1193 | `			/* Advance the stream cursor */` |
|      3 | 1194 | `			pStream->zText += sizeof("null")-1;` |
|      2 | 1195 | `	}else{` |
|      - | 1196 | `		/* Unexpected token — but a byte that is not valid UTF-8 is php's` |
|      - | 1197 | `		 * JSON_ERROR_UTF8, not a syntax error, wherever in the document it sits` |
|      - | 1198 | `		 * (a valid non-ASCII character outside a string stays a syntax error).` |
|      - | 1199 | `		 * The JSON_INVALID_UTF8_* flags do NOT reach here: php applies them` |
|      - | 1200 | `		 * inside string tokens only. */` |
|      - | 1201 | `		sxu32 nLen;` |
|     32 | 1202 | `		pToken->nType = JSON_TK_INVALID;` |
|     53 | 1203 | `		*pJsonErr = ((unsigned char)pStream->zText[0] >= 0x80` |
|     21 | 1204 | `			&& PH7_Utf8ReadStrict((const unsigned char *)pStream->zText,` |
|     18 | 1205 | `				(sxu32)(pStream->zEnd - pStream->zText),&nLen) < 0)` |
|     21 | 1206 | `			? JSON_ERROR_UTF8 : JSON_ERROR_SYNTAX;` |
|      - | 1207 | `		/* Advance the stream cursor */` |
|     32 | 1208 | `		pStream->zText++;` |
|      - | 1209 | `		/* Abort processing immediatley */` |
|     32 | 1210 | `		return SXERR_ABORT;` |
|      - | 1211 | `	}` |
|      - | 1212 | `	/* record token length */` |
|   5420 | 1213 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   5420 | 1214 | `	if( pToken->nType == JSON_TK_STR ){` |
|    168 | 1215 | `		pStr->nByte--;` |
|     82 | 1216 | `	}` |
|      - | 1217 | `	/* Return to the lexer */` |
|   5420 | 1218 | `	return SXRET_OK;` |
|   2749 | 1219 | `}` |
|      - | 1220 | `/*` |
|      - | 1221 | ` * JSON decoded input consumer callback signature.` |
|      - | 1222 | ` */` |
|      - | 1223 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|      - | 1224 | `/*` |
|      - | 1225 | ` * JSON decoder state is kept in the following structure.` |
|      - | 1226 | ` */` |
|      - | 1227 | `typedef struct json_decoder json_decoder;` |
|      - | 1228 | `struct json_decoder` |
|      - | 1229 | `{` |
|      - | 1230 | `	ph7_context *pCtx; /* Call context */` |
|      - | 1231 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|      - | 1232 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|      - | 1233 | `	int iFlags;        /* Configuration flags */` |
|      - | 1234 | `	int iUserFlags;    /* json_decode()'s own $flags (JSON_INVALID_UTF8_* live here) */` |
|      - | 1235 | `	SyToken *pIn;      /* Token stream */` |
|      - | 1236 | `	SyToken *pEnd;     /* End of the token stream */` |
|      - | 1237 | `	int rec_depth;     /* Recursion limit */` |
|      - | 1238 | `	int rec_count;     /* Current nesting level */` |
|      - | 1239 | `	int *pErr;         /* JSON decoding error if any */` |
|      - | 1240 | `};` |
|      - | 1241 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|      - | 1242 | `/* Forward declaration */` |
|      - | 1243 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|      - | 1244 | `/*` |
|      - | 1245 | ` * Read the four hex digits of a \uXXXX escape out of z[0..n-1]. Returns` |
|      - | 1246 | ` * SXRET_OK and the value, or SXERR_SYNTAX when fewer than four are there or one` |
|      - | 1247 | ` * is not a hex digit (php: JSON_ERROR_SYNTAX).` |
|      - | 1248 | ` */` |
|     62 | 1249 | `static sxi32 VmJsonHex4(const char *z,sxu32 n,sxu32 *pVal)` |
|      1 | 1250 | `{` |
|     63 | 1251 | `	sxu32 v = 0;` |
|      - | 1252 | `	int i;` |
|     63 | 1253 | `	if( n < 4 ){` |
|      3 | 1254 | `		return SXERR_SYNTAX;` |
|      - | 1255 | `	}` |
|    293 | 1256 | `	for( i = 0 ; i < 4 ; ++i ){` |
|    235 | 1257 | `		int c = (unsigned char)z[i];` |
|    235 | 1258 | `		if( c >= '0' && c <= '9' ){` |
|    147 | 1259 | `			v = (v << 4) \| (sxu32)(c - '0');` |
|    162 | 1260 | `		}else if( c >= 'a' && c <= 'f' ){` |
|     79 | 1261 | `			v = (v << 4) \| (sxu32)(c - 'a' + 10);` |
|     50 | 1262 | `		}else if( c >= 'A' && c <= 'F' ){` |
|      9 | 1263 | `			v = (v << 4) \| (sxu32)(c - 'A' + 10);` |
|      5 | 1264 | `		}else{` |
|      3 | 1265 | `			return SXERR_SYNTAX;` |
|      - | 1266 | `		}` |
|    117 | 1267 | `	}` |
|     59 | 1268 | `	*pVal = v;` |
|     59 | 1269 | `	return SXRET_OK;` |
|     32 | 1270 | `}` |
|      - | 1271 | `/*` |
|      - | 1272 | ` * Append one run of un-escaped string bytes, checking that it really is UTF-8:` |
|      - | 1273 | ` * php rejects a JSON document carrying a byte that is not text with` |
|      - | 1274 | ` * JSON_ERROR_UTF8, exactly as it refuses to ENCODE one. Only inside a string do` |
|      - | 1275 | ` * the JSON_INVALID_UTF8_* flags apply — a stray byte between tokens is an error` |
|      - | 1276 | ` * either way. Returns JSON_ERROR_NONE or JSON_ERROR_UTF8.` |
|      - | 1277 | ` */` |
|    142 | 1278 | `static int VmJsonAppendChecked(ph7_value *pWorker,const char *zIn,sxu32 nByte,int iFlags)` |
|      4 | 1279 | `{` |
|    146 | 1280 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|    146 | 1281 | `	sxu32 i = 0,iRun = 0,nLen;` |
|    322 | 1282 | `	while( i < nByte ){` |
|    192 | 1283 | `		if( z[i] < 0x80 ){` |
|    168 | 1284 | `			i++;` |
|    168 | 1285 | `			continue;` |
|      - | 1286 | `		}` |
|     25 | 1287 | `		if( PH7_Utf8ReadStrict(&z[i],nByte - i,&nLen) >= 0 ){` |
|      3 | 1288 | `			i += nLen;` |
|      3 | 1289 | `			continue;` |
|      - | 1290 | `		}` |
|     23 | 1291 | `		if( (iFlags & (JSON_INVALID_UTF8_IGNORE\|JSON_INVALID_UTF8_SUBSTITUTE)) == 0 ){` |
|     13 | 1292 | `			return JSON_ERROR_UTF8;` |
|      - | 1293 | `		}` |
|      - | 1294 | `		/* With BOTH flags set php's decoder substitutes while its encoder drops` |
|      - | 1295 | `		 * (probed both ways); the order of these two tests is that asymmetry,` |
|      - | 1296 | `		 * not an oversight. */` |
|      - | 1297 | `		/* Flush what is good, then stand in for the run */` |
|     11 | 1298 | `		if( i > iRun ){` |
|     11 | 1299 | `			ph7_value_string(pWorker,&zIn[iRun],(int)(i - iRun));` |
|      5 | 1300 | `		}` |
|     11 | 1301 | `		nLen = VmJsonBadUtf8Len(&z[i],nByte - i);` |
|     11 | 1302 | `		if( iFlags & JSON_INVALID_UTF8_SUBSTITUTE ){` |
|      7 | 1303 | `			ph7_value_string(pWorker,"\357\277\275",3); /* U+FFFD */` |
|      3 | 1304 | `		}` |
|     11 | 1305 | `		i += nLen;` |
|     11 | 1306 | `		iRun = i;` |
|      1 | 1307 | `	}` |
|    134 | 1308 | `	if( i > iRun ){` |
|    134 | 1309 | `		ph7_value_string(pWorker,&zIn[iRun],(int)(i - iRun));` |
|     65 | 1310 | `	}` |
|    134 | 1311 | `	return JSON_ERROR_NONE;` |
|     75 | 1312 | `}` |
|      - | 1313 | `/*` |
|      - | 1314 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|      - | 1315 | ` * the result in the given ph7_value. Returns JSON_ERROR_NONE, or the json_rc` |
|      - | 1316 | ` * php reports for the malformed escape it stopped on.` |
|      - | 1317 | ` *` |
|      - | 1318 | ` * The \uXXXX form used to fall through to the default branch, which dropped the` |
|      - | 1319 | ` * backslash and kept the rest as literal text: json_decode('"é"') answered` |
|      - | 1320 | ` * the five characters u00e9 instead of "é". \b was mangled the same way (it` |
|      - | 1321 | ` * answered "b"), and an escape JSON does not define (\q) was silently accepted` |
|      - | 1322 | ` * where php raises a syntax error.` |
|      - | 1323 | ` */` |
|    164 | 1324 | `static int VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker,int iFlags)` |
|      4 | 1325 | `{` |
|    168 | 1326 | `	const char *zIn = pStr->zString;` |
|    168 | 1327 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|      - | 1328 | `	const char *zCur;` |
|      - | 1329 | `	int c;` |
|      - | 1330 | `	/* Mark the value as a string */` |
|    168 | 1331 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|    115 | 1332 | `	for(;;){` |
|    234 | 1333 | `		zCur = zIn;` |
|    428 | 1334 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|    198 | 1335 | `			zIn++;` |
|      4 | 1336 | `		}` |
|    234 | 1337 | `		if( zIn > zCur ){` |
|    146 | 1338 | `			int rcChunk = VmJsonAppendChecked(pWorker,zCur,(sxu32)(zIn-zCur),iFlags);` |
|    146 | 1339 | `			if( rcChunk != JSON_ERROR_NONE ){` |
|     13 | 1340 | `				return rcChunk;` |
|      - | 1341 | `			}` |
|     65 | 1342 | `		}` |
|    222 | 1343 | `		zIn++;` |
|    222 | 1344 | `		if( zIn >= zEnd ){` |
|      - | 1345 | `			/* End of the input reached */` |
|    142 | 1346 | `			break;` |
|      - | 1347 | `		}` |
|     81 | 1348 | `		c = zIn[0];` |
|      - | 1349 | `		/* Unescape the character */` |
|     81 | 1350 | `		switch(c){` |
|      7 | 1351 | `		case '"':  ph7_value_string(pWorker,"\"",(int)sizeof(char)); break;` |
|      7 | 1352 | `		case '\\': ph7_value_string(pWorker,"\\",(int)sizeof(char)); break;` |
|      3 | 1353 | `		case '/':  ph7_value_string(pWorker,"/",(int)sizeof(char)); break;` |
|      3 | 1354 | `		case 'b':  ph7_value_string(pWorker,"\b",(int)sizeof(char)); break;` |
|      3 | 1355 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|      5 | 1356 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|      3 | 1357 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|      3 | 1358 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|     26 | 1359 | `		case 'u': {` |
|      - | 1360 | `			/* \uXXXX, and the surrogate PAIR that is JSON's only way to spell a` |
|      - | 1361 | `			 * code point above the BMP. An unpaired half is php's` |
|      - | 1362 | `			 * JSON_ERROR_UTF16, distinct from a malformed escape. */` |
|      - | 1363 | `			unsigned char zUtf8[4];` |
|     53 | 1364 | `			unsigned char *zW = zUtf8;` |
|     53 | 1365 | `			sxu32 cp,cpLow = 0; /* cpLow pre-set: MSVC /W4 flags the short-circuit as a maybe-uninitialized read */` |
|     53 | 1366 | `			if( VmJsonHex4(&zIn[1],(sxu32)(zEnd - zIn - 1),&cp) != SXRET_OK ){` |
|      9 | 1367 | `				return JSON_ERROR_SYNTAX;` |
|      - | 1368 | `			}` |
|     49 | 1369 | `			zIn += 4;` |
|     49 | 1370 | `			if( cp >= 0xDC00 && cp <= 0xDFFF ){` |
|      3 | 1371 | `				return JSON_ERROR_UTF16; /* a low half with no high half before it */` |
|      - | 1372 | `			}` |
|     47 | 1373 | `			if( cp >= 0xD800 && cp <= 0xDBFF ){` |
|     14 | 1374 | `				if( zEnd - zIn < 3 \|\| zIn[1] != '\\' \|\| zIn[2] != 'u'` |
|     10 | 1375 | `				 \|\| VmJsonHex4(&zIn[3],(sxu32)(zEnd - zIn - 3),&cpLow) != SXRET_OK` |
|     11 | 1376 | `				 \|\| cpLow < 0xDC00 \|\| cpLow > 0xDFFF ){` |
|      7 | 1377 | `					return JSON_ERROR_UTF16;` |
|      - | 1378 | `				}` |
|      9 | 1379 | `				cp = 0x10000 + ((cp - 0xD800) << 10) + (cpLow - 0xDC00);` |
|      9 | 1380 | `				zIn += 6;` |
|      4 | 1381 | `			}` |
|     41 | 1382 | `			SX_WRITE_UTF8(zW,cp);` |
|     41 | 1383 | `			ph7_value_string(pWorker,(const char *)zUtf8,(int)(zW - zUtf8));` |
|     41 | 1384 | `			break;` |
|      - | 1385 | `		}` |
|      1 | 1386 | `		default:` |
|      - | 1387 | `			/* Not one of JSON's nine escapes */` |
|      3 | 1388 | `			return JSON_ERROR_SYNTAX;` |
|      - | 1389 | `		}` |
|      - | 1390 | `		/* Advance the stream cursor */` |
|     67 | 1391 | `		zIn++;` |
|      1 | 1392 | `	}` |
|    142 | 1393 | `	return JSON_ERROR_NONE;` |
|     86 | 1394 | `}` |
|      - | 1395 | `/*` |
|      - | 1396 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|      - | 1397 | ` * According to wikipedia` |
|      - | 1398 | ` * JSON's basic types are:` |
|      - | 1399 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|      - | 1400 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|      - | 1401 | ` *   Boolean (true or false)` |
|      - | 1402 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|      - | 1403 | ` *    do not need to be of the same type)` |
|      - | 1404 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|      - | 1405 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|      - | 1406 | ` *     be distinct from each other)` |
|      - | 1407 | ` *   null (empty)` |
|      - | 1408 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|      - | 1409 | ` */` |
|   2712 | 1410 | `static sxi32 VmJsonDecode(` |
|      - | 1411 | `	json_decoder *pDecoder, /* JSON decoder */` |
|      - | 1412 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|      4 | 1413 | `	){` |
|      - | 1414 | `	ph7_value *pWorker; /* Worker variable */` |
|      - | 1415 | `	sxi32 rc;` |
|      - | 1416 | `	int rcQ;            /* VmJsonDequoteString() status */` |
|      - | 1417 | `	/* Nothing left to decode: the token stream is empty (a whitespace-only input` |
|      - | 1418 | `	 * tokenizes to NO tokens at all, so pIn/pEnd are both the NULL base pointer of an` |
|      - | 1419 | `	 * empty set) or a member value is missing after its colon ('{"a":'). Both are a` |
|      - | 1420 | `	 * syntax error for php; without this screen the reads below dereference pEnd. */` |
|   2716 | 1421 | `	if( pDecoder->pIn >= pDecoder->pEnd ){` |
|     26 | 1422 | `		*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|     26 | 1423 | `		return SXERR_ABORT;` |
|      - | 1424 | `	}` |
|   2692 | 1425 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|      - | 1426 | `		/* Scalar value */` |
|    232 | 1427 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|    232 | 1428 | `		if( pWorker == 0 ){` |
|    ! 0 | 1429 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 1430 | `			/* Abort the decoding operation immediately */` |
|    ! 0 | 1431 | `			return SXERR_ABORT;` |
|      - | 1432 | `		}` |
|      - | 1433 | `		/* Reflect the JSON image */` |
|    232 | 1434 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|      - | 1435 | `			/* Nullify the value.*/` |
|      3 | 1436 | `			ph7_value_null(pWorker);` |
|    231 | 1437 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|      - | 1438 | `			/* Boolean value */` |
|      3 | 1439 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|    229 | 1440 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|    150 | 1441 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|      - | 1442 | `			/*` |
|      - | 1443 | `			 * Numeric value.` |
|      - | 1444 | `			 * Get a string representation first then try to get a numeric` |
|      - | 1445 | `			 * value.` |
|      - | 1446 | `			 */` |
|    150 | 1447 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|      - | 1448 | `			/* Obtain a numeric representation */` |
|    150 | 1449 | `			PH7_MemObjToNumeric(pWorker);` |
|    146 | 1450 | `			if( (pDecoder->iUserFlags & JSON_BIGINT_AS_STRING) != 0` |
|     83 | 1451 | `			 && ph7_value_is_float(pWorker) ){` |
|      - | 1452 | `				/* php: an INTEGER literal beyond int64 normally lands on a` |
|      - | 1453 | `				 * float; under JSON_BIGINT_AS_STRING it stays the EXACT source` |
|      - | 1454 | `				 * text as a string. Only integer SHAPES qualify — a '.', 'e'` |
|      - | 1455 | `				 * or 'E' anywhere means the document asked for the float. */` |
|      - | 1456 | `				sxu32 iCh;` |
|      9 | 1457 | `				int bIntShape = 1;` |
|     93 | 1458 | `				for( iCh = 0 ; iCh < pStr->nByte ; ++iCh ){` |
|     88 | 1459 | `					if( pStr->zString[iCh] == '.' \|\| pStr->zString[iCh] == 'e'` |
|     86 | 1460 | `					 \|\| pStr->zString[iCh] == 'E' ){` |
|      5 | 1461 | `						bIntShape = 0;` |
|      5 | 1462 | `						break;` |
|      - | 1463 | `					}` |
|     43 | 1464 | `				}` |
|      9 | 1465 | `				if( bIntShape ){` |
|      5 | 1466 | `					ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|      2 | 1467 | `				}` |
|      4 | 1468 | `			}` |
|     77 | 1469 | `		}else{` |
|      - | 1470 | `			/* Dequote the string */` |
|     80 | 1471 | `			rcQ = VmJsonDequoteString(&pDecoder->pIn->sData,pWorker,pDecoder->iUserFlags);` |
|     80 | 1472 | `			if( rcQ != JSON_ERROR_NONE ){` |
|     25 | 1473 | `				*pDecoder->pErr = rcQ;` |
|     25 | 1474 | `				return SXERR_ABORT;` |
|      - | 1475 | `			}` |
|      - | 1476 | `		}` |
|      - | 1477 | `		/* Invoke the consumer callback */` |
|    208 | 1478 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|    208 | 1479 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 | 1480 | `			return SXERR_ABORT;` |
|      - | 1481 | `		}` |
|      - | 1482 | `		/* All done,advance the stream cursor */` |
|    208 | 1483 | `		pDecoder->pIn++;` |
|   2566 | 1484 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|      - | 1485 | `		ProcJsonConsumer xOld;` |
|      - | 1486 | `		void *pOld;` |
|      - | 1487 | `		/* php's $depth counts CONTAINERS: a '[' opening at 1-based nesting` |
|      - | 1488 | `		 * level L is JSON_ERROR_DEPTH when L >= $depth, an EMPTY container` |
|      - | 1489 | `		 * included ("[]" at $depth 1 already fails), while a scalar never` |
|      - | 1490 | `		 * consults $depth at all. rec_count holds L-1 here. */` |
|   2392 | 1491 | `		if( pDecoder->rec_count + 1 >= pDecoder->rec_depth ){` |
|     11 | 1492 | `			*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|     11 | 1493 | `			return SXERR_ABORT;` |
|      - | 1494 | `		}` |
|      - | 1495 | `		/* Array representation*/` |
|   2382 | 1496 | `		pDecoder->pIn++;` |
|      - | 1497 | `		/* Create a working array */` |
|   2382 | 1498 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   2382 | 1499 | `		if( pWorker == 0 ){` |
|    ! 0 | 1500 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 1501 | `			/* Abort the decoding operation immediately */` |
|    ! 0 | 1502 | `			return SXERR_ABORT;` |
|      - | 1503 | `		}` |
|      - | 1504 | `		/* Save the old consumer */` |
|   2382 | 1505 | `		xOld = pDecoder->xConsumer;` |
|   2382 | 1506 | `		pOld = pDecoder->pUserData;` |
|      - | 1507 | `		/* Set the new consumer */` |
|   2382 | 1508 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   2382 | 1509 | `		pDecoder->pUserData = pWorker;` |
|      - | 1510 | `		/* Decode the array */` |
|   1877 | 1511 | `		for(;;){` |
|      - | 1512 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|      - | 1513 | `			 * do this.` |
|      - | 1514 | `			 */` |
|   3792 | 1515 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|     38 | 1516 | `				pDecoder->pIn++;` |
|      4 | 1517 | `			}` |
|   3758 | 1518 | `			if( pDecoder->pIn >= pDecoder->pEnd ){` |
|      - | 1519 | `				/* Ran out of tokens before the closing ']': php rejects an` |
|      - | 1520 | `				 * unterminated array as a syntax error. */` |
|      8 | 1521 | `				*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      8 | 1522 | `				return SXERR_ABORT;` |
|      - | 1523 | `			}` |
|   3752 | 1524 | `			if( pDecoder->pIn->nType & JSON_TK_CSB /*']'*/ ){` |
|   1350 | 1525 | `				pDecoder->pIn++; /* Jump the trailing ']' */` |
|   1350 | 1526 | `				break;` |
|      - | 1527 | `			}` |
|      - | 1528 | `			/* Recurse and decode the entry */` |
|   2406 | 1529 | `			pDecoder->rec_count++;` |
|   2406 | 1530 | `			rc = VmJsonDecode(pDecoder,0);` |
|   2406 | 1531 | `			pDecoder->rec_count--;` |
|   2406 | 1532 | `			if( rc == SXERR_ABORT ){` |
|      - | 1533 | `				/* Abort processing immediately */` |
|   1027 | 1534 | `				return SXERR_ABORT;` |
|      - | 1535 | `			}` |
|      - | 1536 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   1380 | 1537 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   1374 | 1538 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|      - | 1539 | `					/* Unexpected token,abort immediatley */` |
|    ! 0 | 1540 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1541 | `					return SXERR_ABORT;` |
|      - | 1542 | `			}` |
|      4 | 1543 | `		}` |
|      - | 1544 | `		/* Restore the old consumer */` |
|   1350 | 1545 | `		pDecoder->xConsumer = xOld;` |
|   1350 | 1546 | `		pDecoder->pUserData = pOld;` |
|      - | 1547 | `		/* Invoke the old consumer on the decoded array */` |
|   1350 | 1548 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    749 | 1549 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|      - | 1550 | `		ProcJsonConsumer xOld;` |
|      - | 1551 | `		ph7_value *pKey;` |
|      - | 1552 | `		void *pOld;` |
|      - | 1553 | `		/* Same container rule as '[' above. */` |
|     76 | 1554 | `		if( pDecoder->rec_count + 1 >= pDecoder->rec_depth ){` |
|    ! 0 | 1555 | `			*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|    ! 0 | 1556 | `			return SXERR_ABORT;` |
|      - | 1557 | `		}` |
|      - | 1558 | `		/* Object representation*/` |
|     76 | 1559 | `		pDecoder->pIn++;` |
|      - | 1560 | `		/* Decode into a working array first; unless the caller asked for` |
|      - | 1561 | `		 * associative arrays (assoc=true / JSON_OBJECT_AS_ARRAY), it is converted` |
|      - | 1562 | `		 * to a stdClass below so json_decode('{...}') returns an object like php. */` |
|     76 | 1563 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|     76 | 1564 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|     76 | 1565 | `		if( pWorker == 0 \|\| pKey == 0){` |
|    ! 0 | 1566 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 1567 | `			/* Abort the decoding operation immediately */` |
|    ! 0 | 1568 | `			return SXERR_ABORT;` |
|      - | 1569 | `		}` |
|      - | 1570 | `		/* Save the old consumer */` |
|     76 | 1571 | `		xOld = pDecoder->xConsumer;` |
|     76 | 1572 | `		pOld = pDecoder->pUserData;` |
|      - | 1573 | `		/* Set the new consumer */` |
|     76 | 1574 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|     76 | 1575 | `		pDecoder->pUserData = pWorker;` |
|      - | 1576 | `		/* Decode the object */` |
|     74 | 1577 | `		for(;;){` |
|      - | 1578 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|      - | 1579 | `			 * do this.` |
|      - | 1580 | `			 */` |
|    168 | 1581 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|     19 | 1582 | `				pDecoder->pIn++;` |
|      3 | 1583 | `			}` |
|    152 | 1584 | `			if( pDecoder->pIn >= pDecoder->pEnd ){` |
|      - | 1585 | `				/* Ran out of tokens before the closing '}': php rejects an` |
|      - | 1586 | `				 * unterminated object as a syntax error. */` |
|      3 | 1587 | `				*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      3 | 1588 | `				return SXERR_ABORT;` |
|      - | 1589 | `			}` |
|    150 | 1590 | `			if( pDecoder->pIn->nType & JSON_TK_CCB /*'}'*/ ){` |
|     63 | 1591 | `				pDecoder->pIn++; /* Jump the trailing '}' */` |
|     63 | 1592 | `				break;` |
|      - | 1593 | `			}` |
|     86 | 1594 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|     90 | 1595 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|      - | 1596 | `					/* Syntax error,return immediately */` |
|    ! 0 | 1597 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1598 | `					return SXERR_ABORT;` |
|      - | 1599 | `			}` |
|      - | 1600 | `			/* Dequote the key */` |
|     90 | 1601 | `			rcQ = VmJsonDequoteString(&pDecoder->pIn->sData,pKey,pDecoder->iUserFlags);` |
|     90 | 1602 | `			if( rcQ != JSON_ERROR_NONE ){` |
|      3 | 1603 | `				*pDecoder->pErr = rcQ;` |
|      3 | 1604 | `				return SXERR_ABORT;` |
|      - | 1605 | `			}` |
|     88 | 1606 | `			if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|      - | 1607 | `				/* Decoding to an OBJECT: php refuses a property name whose` |
|      - | 1608 | `				 * FIRST byte is NUL ("\0...") with` |
|      - | 1609 | `				 * JSON_ERROR_INVALID_PROPERTY_NAME — that prefix is reserved` |
|      - | 1610 | `				 * for its mangled private/protected names. A NUL further in is` |
|      - | 1611 | `				 * legal, and array mode (assoc / JSON_OBJECT_AS_ARRAY /` |
|      - | 1612 | `				 * json_validate) takes any key. PHL used to build the property` |
|      - | 1613 | `				 * in silence. */` |
|      - | 1614 | `				int nKeyByte;` |
|     42 | 1615 | `				const char *zKey = ph7_value_to_string(pKey,&nKeyByte);` |
|     42 | 1616 | `				if( nKeyByte > 0 && zKey[0] == '\0' ){` |
|      3 | 1617 | `					*pDecoder->pErr = JSON_ERROR_INVALID_PROPERTY_NAME;` |
|      3 | 1618 | `					return SXERR_ABORT;` |
|      - | 1619 | `				}` |
|     18 | 1620 | `			}` |
|      - | 1621 | `			/* Jump the key and the colon */` |
|     86 | 1622 | `			pDecoder->pIn += 2;` |
|      - | 1623 | `			/* Recurse and decode the value */` |
|     86 | 1624 | `			pDecoder->rec_count++;` |
|     86 | 1625 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|     86 | 1626 | `			pDecoder->rec_count--;` |
|     86 | 1627 | `			if( rc == SXERR_ABORT ){` |
|      - | 1628 | `				/* Abort processing immediately */` |
|      8 | 1629 | `				return SXERR_ABORT;` |
|      - | 1630 | `			}` |
|      - | 1631 | `			/* Reset the internal buffer of the key */` |
|     79 | 1632 | `			ph7_value_reset_string_cursor(pKey);` |
|      - | 1633 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|      3 | 1634 | `		}` |
|      - | 1635 | `		/* Restore the old consumer */` |
|     63 | 1636 | `		pDecoder->xConsumer = xOld;` |
|     63 | 1637 | `		pDecoder->pUserData = pOld;` |
|      - | 1638 | `		/* php returns a stdClass for a JSON object (one dynamic property per member,` |
|      - | 1639 | `		 * nested objects already converted by the recursion) unless assoc was asked. */` |
|     63 | 1640 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|     25 | 1641 | `			PH7_MemObjToObject(pWorker);` |
|     11 | 1642 | `		}` |
|      - | 1643 | `		/* Invoke the old consumer on the decoded object*/` |
|     63 | 1644 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|      - | 1645 | `		/* Release the key */` |
|     63 | 1646 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|     33 | 1647 | `	}else{` |
|      - | 1648 | `		/* Unexpected token */` |
|    ! 0 | 1649 | `		return SXERR_ABORT; /* Abort immediately */` |
|      - | 1650 | `	}` |
|      - | 1651 | `	/* Release the worker variable */` |
|   1614 | 1652 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   1614 | 1653 | `	return SXRET_OK;` |
|   1360 | 1654 | `}` |
|      - | 1655 | `/*` |
|      - | 1656 | ` * The following JSON decoder callback is invoked each time` |
|      - | 1657 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|      - | 1658 | ` * is being decoded.` |
|      - | 1659 | ` */` |
|   1452 | 1660 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|      4 | 1661 | `{` |
|   1456 | 1662 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 1663 | `	/* Insert the entry */` |
|   1456 | 1664 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|    726 | 1665 | `	SXUNUSED(pCtx); /* cc warning */` |
|      - | 1666 | `	/* All done */` |
|   1456 | 1667 | `	return SXRET_OK;` |
|      4 | 1668 | `}` |
|      - | 1669 | `/*` |
|      - | 1670 | ` * Standard JSON decoder callback.` |
|      - | 1671 | ` */` |
|    158 | 1672 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|      4 | 1673 | `{` |
|      - | 1674 | `	/* Return the value directly */` |
|    162 | 1675 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|     79 | 1676 | `	SXUNUSED(pKey); /* cc warning */` |
|     79 | 1677 | `	SXUNUSED(pUserData);` |
|      - | 1678 | `	/* All done */` |
|    162 | 1679 | `	return SXRET_OK;` |
|      4 | 1680 | `}` |
|      - | 1681 | `/*` |
|      - | 1682 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 512[,int $options = 0 ]]])` |
|      - | 1683 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|      - | 1684 | ` * Parameters` |
|      - | 1685 | ` *  $json` |
|      - | 1686 | ` *    The json string being decoded.` |
|      - | 1687 | ` * $assoc` |
|      - | 1688 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|      - | 1689 | ` * $depth` |
|      - | 1690 | ` *   User specified recursion depth.` |
|      - | 1691 | ` * $options` |
|      - | 1692 | ` *   Bitmask of JSON decode options: JSON_OBJECT_AS_ARRAY (objects decode as` |
|      - | 1693 | ` *   associative arrays when $assoc is NULL), JSON_BIGINT_AS_STRING (an integer` |
|      - | 1694 | ` *   beyond int64 stays the exact source text instead of a float),` |
|      - | 1695 | ` *   JSON_INVALID_UTF8_IGNORE/_SUBSTITUTE and JSON_THROW_ON_ERROR` |
|      - | 1696 | ` * Return` |
|      - | 1697 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|      - | 1698 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|      - | 1699 | ` *  or if the encoded data is deeper than the recursion limit.` |
|      - | 1700 | ` */` |
|      - | 1701 | `/*` |
|      - | 1702 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|      - | 1703 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|      - | 1704 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|      - | 1705 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|      - | 1706 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|      - | 1707 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|      - | 1708 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|      - | 1709 | ` */` |
|    280 | 1710 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth,int iUserFlags)` |
|      4 | 1711 | `{` |
|    284 | 1712 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1713 | `	json_decoder sDecoder;` |
|      - | 1714 | `	SySet sToken;` |
|      - | 1715 | `	SyLex sLex;` |
|      - | 1716 | `	sxi32 rc;` |
|      - | 1717 | `	/* Clear JSON error code */` |
|    284 | 1718 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|      - | 1719 | `	/* Tokenize the input */` |
|    284 | 1720 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|    284 | 1721 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|    284 | 1722 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|    284 | 1723 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|      - | 1724 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|     54 | 1725 | `		SyLexRelease(&sLex);` |
|     54 | 1726 | `		SySetRelease(&sToken);` |
|     54 | 1727 | `		return pVm->json_rc;` |
|      - | 1728 | `	}` |
|      - | 1729 | `	/* Fill the decoder */` |
|    232 | 1730 | `	sDecoder.pCtx = pCtx;` |
|    232 | 1731 | `	sDecoder.pErr = &pVm->json_rc;` |
|    232 | 1732 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    232 | 1733 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|    232 | 1734 | `	sDecoder.iFlags = 0;` |
|    232 | 1735 | `	if( iAssoc ){` |
|      - | 1736 | `		/* Returned objects will be converted into associative arrays */` |
|    137 | 1737 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|     67 | 1738 | `	}` |
|    232 | 1739 | `	sDecoder.iUserFlags = iUserFlags;` |
|      - | 1740 | `	/* php's $depth (default 512; the callers' ValueError screens guarantee` |
|      - | 1741 | `	 * 1..INT_MAX-1), bounded by the engine's stack-safety ceiling. The old code` |
|      - | 1742 | `	 * CLAMPED it to an engine limit of 32, so a 40-deep document php decodes` |
|      - | 1743 | `	 * answered NULL/JSON_ERROR_DEPTH. Recursion is bounded by the INPUT's` |
|      - | 1744 | `	 * actual nesting, never by the requested ceiling. */` |
|    232 | 1745 | `	sDecoder.rec_depth = nDepth > PH7_JSON_DEPTH_CEILING ? PH7_JSON_DEPTH_CEILING : nDepth;` |
|    232 | 1746 | `	sDecoder.rec_count = 0;` |
|      - | 1747 | `	/* Set a default consumer */` |
|    232 | 1748 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|    232 | 1749 | `	sDecoder.pUserData = 0;` |
|      - | 1750 | `	/* Decode the raw JSON input */` |
|    232 | 1751 | `	rc = VmJsonDecode(&sDecoder,0);` |
|    232 | 1752 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|      - | 1753 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|    ! 0 | 1754 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1755 | `	}` |
|    232 | 1756 | `	if( pVm->json_rc == JSON_ERROR_NONE && sDecoder.pIn < sDecoder.pEnd ){` |
|      - | 1757 | `		/* php requires the whole input to be ONE JSON value; tokens left after a` |
|      - | 1758 | `		 * complete value (e.g. '"a":1', '{}x', '1 2') are a syntax error. */` |
|      3 | 1759 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|      1 | 1760 | `	}` |
|      - | 1761 | `	/* Clean-up the mess left behind */` |
|    232 | 1762 | `	SyLexRelease(&sLex);` |
|    232 | 1763 | `	SySetRelease(&sToken);` |
|    232 | 1764 | `	return pVm->json_rc;` |
|    144 | 1765 | `}` |
|    252 | 1766 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1767 | `{` |
|      - | 1768 | `	const char *zIn;` |
|      - | 1769 | `	int nByte;` |
|    256 | 1770 | `	int iAssoc = 0;` |
|    256 | 1771 | `	int nDepth = 512;` |
|    256 | 1772 | `	int iFlags = 0;` |
|      - | 1773 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1774 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|    256 | 1775 | `	if( nArg < 1 ){` |
|      - | 1776 | `		/* Missing/Invalid arguments, return NULL */` |
|    ! 0 | 1777 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1778 | `		return PH7_OK;` |
|      - | 1779 | `	}` |
|    256 | 1780 | `	if( nArg > 3 && ph7_value_is_int(apArg[3]) ){` |
|      - | 1781 | `		/* $flags (JSON_OBJECT_AS_ARRAY / JSON_BIGINT_AS_STRING /` |
|      - | 1782 | `		 * JSON_INVALID_UTF8_* / JSON_THROW_ON_ERROR). */` |
|     40 | 1783 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     19 | 1784 | `	}` |
|      - | 1785 | `	/* Extract the JSON string */` |
|    256 | 1786 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    256 | 1787 | `	if( nByte < 1 ){` |
|      - | 1788 | `		/* Empty string: php records a syntax error (json_last_error() == 4) and` |
|      - | 1789 | `		 * returns NULL, or raises a JsonException with JSON_THROW_ON_ERROR. */` |
|      9 | 1790 | `		pCtx->pVm->json_rc = JSON_ERROR_SYNTAX;` |
|      9 | 1791 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|    ! 0 | 1792 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|    ! 0 | 1793 | `				JSON_ERROR_SYNTAX,"%s",JsonErrorMsg(JSON_ERROR_SYNTAX));` |
|      - | 1794 | `		}` |
|      9 | 1795 | `		ph7_result_null(pCtx);` |
|      9 | 1796 | `		return PH7_OK;` |
|      - | 1797 | `	}` |
|    250 | 1798 | `	if( nArg > 1 ){` |
|      - | 1799 | `		/* php's $associative is a ?bool: an explicit true/false decides on its` |
|      - | 1800 | `		 * own (false beats the flag), and only NULL lets JSON_OBJECT_AS_ARRAY` |
|      - | 1801 | `		 * answer instead. */` |
|    151 | 1802 | `		if( ph7_value_is_null(apArg[1]) ){` |
|      7 | 1803 | `			iAssoc = (iFlags & JSON_OBJECT_AS_ARRAY) != 0;` |
|      4 | 1804 | `		}else{` |
|    145 | 1805 | `			iAssoc = ph7_value_to_bool(apArg[1]) != 0;` |
|      - | 1806 | `		}` |
|     74 | 1807 | `	}` |
|    250 | 1808 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      - | 1809 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);` |
|      - | 1810 | `		 * read as int64 so a value above INT_MAX is detected, not truncated. */` |
|     66 | 1811 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);` |
|      - | 1812 | `		/* php clears the json error state before validating $depth, so a caught` |
|      - | 1813 | `		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal` |
|      - | 1814 | `		 * path resets it again inside VmJsonDecodeInput). */` |
|     66 | 1815 | `		pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|     66 | 1816 | `		if( nWant <= 0 ){` |
|      9 | 1817 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1818 | `				"json_decode(): Argument #3 ($depth) must be greater than 0");` |
|      - | 1819 | `		}` |
|     58 | 1820 | `		if( nWant > 2147483647 ){` |
|      3 | 1821 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1822 | `				"json_decode(): Argument #3 ($depth) must be less than 2147483647");` |
|      - | 1823 | `		}` |
|     56 | 1824 | `		nDepth = (int)nWant;` |
|     27 | 1825 | `	}` |
|      - | 1826 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|      - | 1827 | `	 * call-context result; on failure we replace it with NULL (or throw). */` |
|    240 | 1828 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth,iFlags) != JSON_ERROR_NONE ){` |
|      - | 1829 | `		/* Something goes wrong while decoding JSON input. */` |
|    105 | 1830 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|      - | 1831 | `			/* php: raise a JsonException carrying json_last_error_msg() text. */` |
|     11 | 1832 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|      6 | 1833 | `				(sxi32)pCtx->pVm->json_rc,"%s",` |
|      6 | 1834 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|      - | 1835 | `		}` |
|     99 | 1836 | `		ph7_result_null(pCtx);` |
|     48 | 1837 | `	}` |
|      - | 1838 | `	/* All done */` |
|    234 | 1839 | `	return PH7_OK;` |
|    130 | 1840 | `}` |
|      - | 1841 | `/*` |
|      - | 1842 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|      - | 1843 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|      - | 1844 | ` * Parameters` |
|      - | 1845 | ` *  $json   The string to validate.` |
|      - | 1846 | ` *  $depth  Maximum nesting depth (php's default of 512, honored verbatim).` |
|      - | 1847 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|      - | 1848 | ` * Return` |
|      - | 1849 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|      - | 1850 | ` */` |
|     58 | 1851 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1852 | `{` |
|     61 | 1853 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1854 | `	const char *zIn;` |
|      - | 1855 | `	int nByte;` |
|     61 | 1856 | `	int nDepth = 512;` |
|     61 | 1857 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1858 | `		/* Missing/Invalid argument: not valid JSON */` |
|    ! 0 | 1859 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1860 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1861 | `		return PH7_OK;` |
|      - | 1862 | `	}` |
|      - | 1863 | `	/* Extract the JSON string */` |
|     61 | 1864 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|     61 | 1865 | `	if( nByte < 1 ){` |
|      - | 1866 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|      - | 1867 | `		 * silently, json_validate must record the syntax error) */` |
|      6 | 1868 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|      6 | 1869 | `		ph7_result_bool(pCtx,0);` |
|      6 | 1870 | `		return PH7_OK;` |
|      - | 1871 | `	}` |
|     57 | 1872 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1873 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */` |
|     24 | 1874 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|      - | 1875 | `		/* Clear the json error state before validating $depth (php parity), so a` |
|      - | 1876 | `		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */` |
|     24 | 1877 | `		pVm->json_rc = JSON_ERROR_NONE;` |
|     24 | 1878 | `		if( nWant <= 0 ){` |
|      5 | 1879 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1880 | `				"json_validate(): Argument #2 ($depth) must be greater than 0");` |
|      - | 1881 | `		}` |
|     20 | 1882 | `		if( nWant > 2147483647 ){` |
|      3 | 1883 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1884 | `				"json_validate(): Argument #2 ($depth) must be less than 2147483647");` |
|      - | 1885 | `		}` |
|     18 | 1886 | `		nDepth = (int)nWant;` |
|      8 | 1887 | `	}` |
|      - | 1888 | `	/* php's only ACCEPTED $flags value here is JSON_INVALID_UTF8_IGNORE, which` |
|      - | 1889 | `	 * makes a payload with undecodable bytes VALID; it rides the same rail as` |
|      - | 1890 | `	 * json_decode's. Any other bit is a ValueError naming the one flag there is --` |
|      - | 1891 | `	 * php refuses the whole json_decode set for this function, and PHL used to take` |
|      - | 1892 | `	 * whatever it was given and validate on. Decode in associative mode so the` |
|      - | 1893 | `	 * "objects are returned as an array" warning is not raised - the decoded value` |
|      - | 1894 | `	 * is discarded, only its validity matters. */` |
|      - | 1895 | `	{` |
|     32 | 1896 | `		int iFlags = (nArg > 2 && ph7_value_is_int(apArg[2]))` |
|     34 | 1897 | `			? ph7_value_to_int(apArg[2]) : 0;` |
|     51 | 1898 | `		if( (iFlags & ~JSON_INVALID_UTF8_IGNORE) != 0 ){` |
|      5 | 1899 | `			pVm->json_rc = JSON_ERROR_NONE;` |
|      5 | 1900 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1901 | `				"json_validate(): Argument #3 ($flags) must be a valid flag "` |
|      - | 1902 | `				"(allowed flags: JSON_INVALID_UTF8_IGNORE)");` |
|      - | 1903 | `		}` |
|     69 | 1904 | `		ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth,iFlags)` |
|     22 | 1905 | `			== JSON_ERROR_NONE);` |
|      - | 1906 | `	}` |
|     47 | 1907 | `	return PH7_OK;` |
|     32 | 1908 | `}` |
|      - | 1909 |  |
