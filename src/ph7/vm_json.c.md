# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 934/1018 lines (91.75%)

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
|      - |   42 | `};` |
|      - |   43 | `/*` |
|      - |   44 | ` * Stack-safety ceiling on the EFFECTIVE json depth, both directions. php honors` |
|      - |   45 | ` * $depth up to INT_MAX and relies on a dynamic guard that asks the platform how` |
|      - |   46 | ` * much C stack is left; PHL's encoder and decoder recurse on the same C stack` |
|      - |   47 | ` * with no such probe, so a requested depth above this bound is clamped to it and` |
|      - |   48 | ` * a value/document nested deeper reports JSON_ERROR_DEPTH — loud, like the` |
|      - |   49 | ` * SERIALIZE_MAX_DEPTH and HTTP_QUERY_MAX_DEPTH bounds this follows. 4096 is` |
|      - |   50 | ` * eight times php's default of 512 and holds ~0.5MB of frames on MSVC's 1MB` |
|      - |   51 | ` * default stack (measured ~10x smaller on glibc's 8MB). Ports with small stacks` |
|      - |   52 | ` * (ESP32 task stacks are KBs) can override it at build time.` |
|      - |   53 | ` */` |
|      - |   54 | `#ifndef PH7_JSON_DEPTH_CEILING` |
|      - |   55 | `#define PH7_JSON_DEPTH_CEILING 4096` |
|      - |   56 | `#endif` |
|      - |   57 | `/*` |
|      - |   58 | ` * True if pPtr (a hashmap or class instance) is a container the encoder is` |
|      - |   59 | ` * currently INSIDE of. Only the active path is searched, so a value appearing` |
|      - |   60 | ` * twice as SIBLINGS ([$a,$a]) stays legal like php — recursion means` |
|      - |   61 | ` * self-containment, not sharing.` |
|      - |   62 | ` */` |
|   4250 |   63 | `static int VmJsonPathHolds(json_private_data *pData,void *pPtr)` |
|      5 |   64 | `{` |
|   4255 |   65 | `	void **apEntry = (void **)SySetBasePtr(&pData->aPath);` |
|   4255 |   66 | `	sxu32 i,n = SySetUsed(&pData->aPath);` |
| 790383 |   67 | `	for( i = 0 ; i < n ; ++i ){` |
| 786140 |   68 | `		if( apEntry[i] == pPtr ){` |
|      9 |   69 | `			return 1;` |
|      - |   70 | `		}` |
| 393068 |   71 | `	}` |
|   4247 |   72 | `	return 0;` |
|   2130 |   73 | `}` |
|      - |   74 | `/*` |
|      - |   75 | ` * Emit into the JSON result, flagging OOM on the shared data and bailing out` |
|      - |   76 | ` * of the current encode function (which returns PH7_OK; the top-level` |
|      - |   77 | ` * vm_builtin_json_encode checks ->oom and raises a non-catchable fatal). Used` |
|      - |   78 | ` * for every ph7_result_string/ph7_result_string_format append below.` |
|      - |   79 | ` */` |
|      - |   80 | `#define JSON_EMIT(pD, call) do { if( (call) != SXRET_OK ){ (pD)->oom = 1; return PH7_OK; } } while(0)` |
|      - |   81 | `/*` |
|      - |   82 | ` * Emit a float in php's json shape: PH7_AppendShortestReal (the shared` |
|      - |   83 | ` * serialize/var_export shortest-round-trip formatter, php's` |
|      - |   84 | ` * serialize_precision=-1) with the exponent marker lowercased (json prints` |
|      - |   85 | ` * 1.0e+17 where serialize prints 1.0E+17). Under JSON_PRESERVE_ZERO_FRACTION a` |
|      - |   86 | ` * value whose shortest form carries no '.' or exponent gets ".0" appended, so` |
|      - |   87 | ` * 1.0 stays a FLOAT on the round trip ("1.0", "-0.0") the way php keeps it.` |
|      - |   88 | ` */` |
|     68 |   89 | `static sxi32 VmJsonEmitReal(ph7_context *pCtx,double rVal,int iFlags)` |
|      2 |   90 | `{` |
|      - |   91 | `	SyBlob sNum;` |
|      - |   92 | `	char *z;` |
|      - |   93 | `	sxu32 i,n;` |
|      - |   94 | `	sxi32 rc;` |
|     70 |   95 | `	int bFrac = 0;` |
|     70 |   96 | `	SyBlobInit(&sNum,&pCtx->pVm->sAllocator);` |
|     70 |   97 | `	PH7_AppendShortestReal(&sNum,rVal);` |
|     70 |   98 | `	z = (char *)SyBlobData(&sNum);` |
|     70 |   99 | `	n = SyBlobLength(&sNum);` |
|     70 |  100 | `	if( z == 0 \|\| n < 1 ){` |
|    ! 0 |  101 | `		SyBlobRelease(&sNum);` |
|    ! 0 |  102 | `		return SXERR_MEM; /* treated as OOM by JSON_EMIT */` |
|      - |  103 | `	}` |
|    402 |  104 | `	for( i = 0 ; i < n ; i++ ){` |
|    334 |  105 | `		if( z[i] == 'E' ){` |
|     11 |  106 | `			z[i] = 'e';` |
|      5 |  107 | `		}` |
|    334 |  108 | `		if( z[i] == 'e' \|\| z[i] == '.' ){` |
|     41 |  109 | `			bFrac = 1;` |
|     20 |  110 | `		}` |
|    168 |  111 | `	}` |
|     70 |  112 | `	if( !bFrac && (iFlags & JSON_PRESERVE_ZERO_FRACTION) != 0 ){` |
|     11 |  113 | `		if( SyBlobAppend(&sNum,".0",2) != SXRET_OK ){` |
|    ! 0 |  114 | `			SyBlobRelease(&sNum);` |
|    ! 0 |  115 | `			return SXERR_MEM;` |
|      - |  116 | `		}` |
|     11 |  117 | `		z = (char *)SyBlobData(&sNum);` |
|     11 |  118 | `		n = SyBlobLength(&sNum);` |
|      5 |  119 | `	}` |
|     70 |  120 | `	rc = ph7_result_string(pCtx,(const char *)z,(int)n);` |
|     70 |  121 | `	SyBlobRelease(&sNum);` |
|     70 |  122 | `	return rc;` |
|     36 |  123 | `}` |
|      - |  124 | `/*` |
|      - |  125 | ` * JSON_PRETTY_PRINT helper: emit a newline followed by (depth * 4) spaces, so a` |
|      - |  126 | ` * container's members are laid out one-per-line and indented like php. A no-op` |
|      - |  127 | ` * unless JSON_PRETTY_PRINT is set. Returns SXRET_OK or an OOM status; callers` |
|      - |  128 | ` * wrap it in JSON_EMIT so an allocation failure trips the ->oom rail.` |
|      - |  129 | ` */` |
|   9270 |  130 | `static sxi32 VmJsonPretty(json_private_data *pJson,int depth)` |
|      5 |  131 | `{` |
|   9275 |  132 | `	ph7_context *pCtx = pJson->pCtx;` |
|      - |  133 | `	sxi32 rc;` |
|      - |  134 | `	int i;` |
|   9275 |  135 | `	if( (pJson->iFlags & JSON_PRETTY_PRINT) == 0 ){` |
|   9211 |  136 | `		return SXRET_OK;` |
|      - |  137 | `	}` |
|     66 |  138 | `	rc = ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    154 |  139 | `	for( i = 0 ; i < depth && rc == SXRET_OK ; ++i ){` |
|     90 |  140 | `		rc = ph7_result_string(pCtx,"    ",(int)sizeof("    ")-1);` |
|     46 |  141 | `	}` |
|     66 |  142 | `	return rc;` |
|   4640 |  143 | `}` |
|      - |  144 | `/*` |
|      - |  145 | ` * Byte length of the ill-formed UTF-8 run at z[0..n-1] as php's JSON encoder` |
|      - |  146 | ` * measures it: a byte that could LEAD a sequence (C2..F4) swallows every` |
|      - |  147 | ` * following byte that is merely continuation-SHAPED (10xxxxxx), up to the` |
|      - |  148 | ` * length its lead announces, and the whole prefix is ONE error. So "\xed\xa0\x80"` |
|      - |  149 | ` * (a surrogate) is a single JSON_ERROR_UTF8 / a single U+FFFD, while` |
|      - |  150 | ` * "\xf5\x80\x80\x80" is four — F5 leads nothing, so each byte fails alone.` |
|      - |  151 | ` *` |
|      - |  152 | ` * php's mbstring measures the same runs with the STRICTER per-lead ranges` |
|      - |  153 | ` * (builtin_mb.c's MbUtf8BadLen), which is why the two disagree on a surrogate:` |
|      - |  154 | ` * mb_strtolower("\xed\xa0\x80") is "???" while json substitutes one U+FFFD.` |
|      - |  155 | ` * Two php decoders, two rules — each matched where it belongs.` |
|      - |  156 | ` */` |
|     90 |  157 | `static sxu32 VmJsonBadUtf8Len(const unsigned char *z,sxu32 n)` |
|      1 |  158 | `{` |
|     91 |  159 | `	sxu32 c = z[0],need,i;` |
|     91 |  160 | `	if( c >= 0xC2 && c <= 0xDF ){` |
|      7 |  161 | `		need = 2;` |
|     88 |  162 | `	}else if( c >= 0xE0 && c <= 0xEF ){` |
|     13 |  163 | `		need = 3;` |
|     79 |  164 | `	}else if( c >= 0xF0 && c <= 0xF4 ){` |
|      7 |  165 | `		need = 4;` |
|      4 |  166 | `	}else{` |
|     67 |  167 | `		return 1; /* 80..C1 or F5..FF: leads nothing */` |
|      - |  168 | `	}` |
|     55 |  169 | `	for( i = 1 ; i < need && i < n && (z[i] & 0xC0) == 0x80 ; ++i ){}` |
|     25 |  170 | `	return i;` |
|     46 |  171 | `}` |
|      - |  172 | `/*` |
|      - |  173 | ` * Emit one code point as php's \uXXXX escape (lowercase hex), spelling anything` |
|      - |  174 | ` * outside the BMP as the UTF-16 surrogate pair JSON has no other way to carry:` |
|      - |  175 | ` * U+1F600 is "😀", exactly like php.` |
|      - |  176 | ` */` |
|     64 |  177 | `static sxi32 VmJsonEmitUnicodeEscape(ph7_context *pCtx,sxu32 cp)` |
|      1 |  178 | `{` |
|      - |  179 | `	static const char zHex[] = "0123456789abcdef";` |
|      - |  180 | `	sxu32 aUnit[2];` |
|      - |  181 | `	int nUnit,i;` |
|      - |  182 | `	char zEsc[12];` |
|     65 |  183 | `	if( cp >= 0x10000 ){` |
|      5 |  184 | `		sxu32 v = cp - 0x10000;` |
|      5 |  185 | `		aUnit[0] = 0xD800 + (v >> 10);` |
|      5 |  186 | `		aUnit[1] = 0xDC00 + (v & 0x3FF);` |
|      5 |  187 | `		nUnit = 2;` |
|      3 |  188 | `	}else{` |
|     61 |  189 | `		aUnit[0] = cp;` |
|     61 |  190 | `		nUnit = 1;` |
|      - |  191 | `	}` |
|    133 |  192 | `	for( i = 0 ; i < nUnit ; ++i ){` |
|     69 |  193 | `		zEsc[i*6 + 0] = '\\';` |
|     69 |  194 | `		zEsc[i*6 + 1] = 'u';` |
|     69 |  195 | `		zEsc[i*6 + 2] = zHex[(aUnit[i] >> 12) & 0x0F];` |
|     69 |  196 | `		zEsc[i*6 + 3] = zHex[(aUnit[i] >>  8) & 0x0F];` |
|     69 |  197 | `		zEsc[i*6 + 4] = zHex[(aUnit[i] >>  4) & 0x0F];` |
|     69 |  198 | `		zEsc[i*6 + 5] = zHex[ aUnit[i]        & 0x0F];` |
|     35 |  199 | `	}` |
|     65 |  200 | `	return ph7_result_string(pCtx,zEsc,nUnit * 6);` |
|      1 |  201 | `}` |
|      - |  202 | `/*` |
|      - |  203 | ` * Emit one JSON string literal — the opening quote, the escaped body, the` |
|      - |  204 | ` * closing quote. Shared by the string VALUE path and by both KEY paths (array` |
|      - |  205 | ` * keys and object property names), which used to append their bytes raw: a key` |
|      - |  206 | ` * carrying a '"', a backslash or a control character produced UNPARSEABLE` |
|      - |  207 | ` * output (php: json_encode(["a\"b"=>1]) is {"a\"b":1}, PHL emitted {"a"b":1}).` |
|      - |  208 | ` * Everything php escapes in a string it escapes in a key, the JSON_HEX_*` |
|      - |  209 | ` * and JSON_UNESCAPED_SLASHES flags included.` |
|      - |  210 | ` *` |
|      - |  211 | ` * Non-ASCII is escaped as \uXXXX by DEFAULT, which is what php does and what` |
|      - |  212 | ` * JSON_UNESCAPED_UNICODE turns off — PHL used to emit the raw UTF-8 bytes` |
|      - |  213 | ` * unconditionally, i.e. behave as if that flag were always set (the flag was` |
|      - |  214 | ` * defined but never read). Even with it set php still escapes U+2028/U+2029,` |
|      - |  215 | ` * the two line terminators JavaScript's eval() chokes on, unless` |
|      - |  216 | ` * JSON_UNESCAPED_LINE_TERMINATORS is set too.` |
|      - |  217 | ` *` |
|      - |  218 | ` * bKey selects JSON_PARTIAL_OUTPUT_ON_ERROR's substitute for an ill-formed` |
|      - |  219 | ` * string: php replaces a VALUE with null and a KEY (array key or property` |
|      - |  220 | ` * name) with "" — the verdict must land before anything is emitted, because` |
|      - |  221 | ` * the replacement covers the WHOLE string, not the tail after the bad byte.` |
|      - |  222 | ` */` |
|     14 |  223 | `static int VmJsonStrHasBadUtf8(const char *zIn,int nByte)` |
|      1 |  224 | `{` |
|     15 |  225 | `	const unsigned char *z = (const unsigned char *)zIn,*zEnd = (const unsigned char *)&zIn[nByte];` |
|      - |  226 | `	sxu32 nLen;` |
|     37 |  227 | `	while( z < zEnd ){` |
|     31 |  228 | `		if( z[0] < 0x80 ){` |
|     23 |  229 | `			z++;` |
|     23 |  230 | `			continue;` |
|      - |  231 | `		}` |
|      9 |  232 | `		if( PH7_Utf8ReadStrict(z,(sxu32)(zEnd - z),&nLen) < 0 ){` |
|      9 |  233 | `			return 1;` |
|      - |  234 | `		}` |
|    ! 0 |  235 | `		z += nLen;` |
|    ! 0 |  236 | `	}` |
|      7 |  237 | `	return 0;` |
|      8 |  238 | `}` |
|   2072 |  239 | `static sxi32 VmJsonEncodeString(json_private_data *pData,const char *zIn,int nByte,int bKey)` |
|      5 |  240 | `{` |
|   2077 |  241 | `	ph7_context *pCtx = pData->pCtx;` |
|   2077 |  242 | `	int iFlags = pData->iFlags;` |
|   2077 |  243 | `	const char *zEnd = &zIn[nByte];` |
|      - |  244 | `	sxi32 rc;` |
|      - |  245 | `	char c;` |
|   2072 |  246 | `	if( (iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR) != 0` |
|   1043 |  247 | `	 && (iFlags & (JSON_INVALID_UTF8_IGNORE\|JSON_INVALID_UTF8_SUBSTITUTE)) == 0` |
|     19 |  248 | `	 && VmJsonStrHasBadUtf8(zIn,nByte) ){` |
|      9 |  249 | `		pCtx->pVm->json_rc = JSON_ERROR_UTF8;` |
|      6 |  250 | `		return bKey ? ph7_result_string(pCtx,"\"\"",2)` |
|      8 |  251 | `		            : ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|      - |  252 | `	}` |
|   2069 |  253 | `	rc = ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|   2069 |  254 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  255 | `		return rc;` |
|      - |  256 | `	}` |
|   4242 |  257 | `	for(;;){` |
|   8597 |  258 | `		if( zIn >= zEnd ){` |
|      - |  259 | `			/* No more input to process */` |
|   2041 |  260 | `			break;` |
|      - |  261 | `		}` |
|   6561 |  262 | `		if( (unsigned char)zIn[0] >= 0x80 ){` |
|      - |  263 | `			/* A UTF-8 sequence: decode it strictly, since \uXXXX needs the code` |
|      - |  264 | `			 * point and not the bytes. */` |
|      - |  265 | `			sxu32 nLen,cp;` |
|    137 |  266 | `			sxi32 iCp = PH7_Utf8ReadStrict((const unsigned char *)zIn,(sxu32)(zEnd - zIn),&nLen);` |
|    137 |  267 | `			if( iCp < 0 ){` |
|      - |  268 | `				/* Ill-formed. php REFUSES to encode it: json_encode returns` |
|      - |  269 | `				 * false with json_last_error() == JSON_ERROR_UTF8, because` |
|      - |  270 | `				 * there is no honest JSON spelling for a byte that is not` |
|      - |  271 | `				 * text. PHL used to pass the byte through, so the caller got a` |
|      - |  272 | `				 * valid-looking payload php would never have produced and no` |
|      - |  273 | `				 * error check could see it. The two JSON_INVALID_UTF8_* flags` |
|      - |  274 | `				 * are the opt-outs php offers. */` |
|     81 |  275 | `				nLen = VmJsonBadUtf8Len((const unsigned char *)zIn,(sxu32)(zEnd - zIn));` |
|     81 |  276 | `				if( iFlags & JSON_INVALID_UTF8_IGNORE ){` |
|     25 |  277 | `					rc = SXRET_OK; /* drop the run */` |
|     69 |  278 | `				}else if( iFlags & JSON_INVALID_UTF8_SUBSTITUTE ){` |
|     29 |  279 | `					rc = (iFlags & JSON_UNESCAPED_UNICODE)` |
|      2 |  280 | `						? ph7_result_string(pCtx,"\357\277\275",3) /* U+FFFD */` |
|     27 |  281 | `						: VmJsonEmitUnicodeEscape(pCtx,0xFFFD);` |
|     15 |  282 | `				}else{` |
|     29 |  283 | `					pData->fail = 1;` |
|     29 |  284 | `					pData->failRc = JSON_ERROR_UTF8;` |
|     29 |  285 | `					return SXRET_OK; /* the whole encode is discarded */` |
|      - |  286 | `				}` |
|     27 |  287 | `			}else{` |
|     57 |  288 | `				cp = (sxu32)iCp;` |
|     56 |  289 | `				if( (iFlags & JSON_UNESCAPED_UNICODE) == 0` |
|     40 |  290 | `				 \|\| ((cp == 0x2028 \|\| cp == 0x2029)` |
|     14 |  291 | `				  && (iFlags & JSON_UNESCAPED_LINE_TERMINATORS) == 0) ){` |
|     39 |  292 | `					rc = VmJsonEmitUnicodeEscape(pCtx,cp);` |
|     20 |  293 | `				}else{` |
|     19 |  294 | `					rc = ph7_result_string(pCtx,zIn,(int)nLen);` |
|      - |  295 | `				}` |
|      - |  296 | `			}` |
|    109 |  297 | `			zIn += nLen;` |
|    109 |  298 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  299 | `				return rc;` |
|      - |  300 | `			}` |
|    109 |  301 | `			continue;` |
|      - |  302 | `		}` |
|   6425 |  303 | `		c = zIn[0];` |
|      - |  304 | `		/* Advance the stream cursor */` |
|   6425 |  305 | `		zIn++;` |
|   6425 |  306 | `		if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|      - |  307 | `			/* All < and > are converted to \u003C and \u003E */` |
|      5 |  308 | `			if( c == '<' ){` |
|      3 |  309 | `				rc = ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|      2 |  310 | `			}else{` |
|      3 |  311 | `				rc = ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|      1 |  312 | `			}` |
|   6423 |  313 | `		}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|      - |  314 | `			/* All &s are converted to \u0026.  */` |
|      3 |  315 | `			rc = ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|   6420 |  316 | `		}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|      - |  317 | `			/* All ' are converted to \u0027.   */` |
|      3 |  318 | `			rc = ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|   6418 |  319 | `		}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|      - |  320 | `			/* All " are converted to \u0022. */` |
|      3 |  321 | `			rc = ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|   6416 |  322 | `		}else if( (unsigned char)c < 0x20 ){` |
|      - |  323 | `			/* Control characters (band A #4): php emits the short escapes for` |
|      - |  324 | `			 * \b \f \n \r \t and \u00xx for the rest — pre-fix these were` |
|      - |  325 | `			 * emitted RAW (invalid JSON). */` |
|      - |  326 | `			static const char zHex[] = "0123456789abcdef";` |
|    101 |  327 | `			char zEsc[6] = { '\\', 'u', '0', '0', 0, 0 };` |
|    101 |  328 | `			switch(c){` |
|    ! 0 |  329 | `			case '\b': rc = ph7_result_string(pCtx,"\\b",2); break;` |
|    ! 0 |  330 | `			case '\f': rc = ph7_result_string(pCtx,"\\f",2); break;` |
|     47 |  331 | `			case '\n': rc = ph7_result_string(pCtx,"\\n",2); break;` |
|     18 |  332 | `			case '\r': rc = ph7_result_string(pCtx,"\\r",2); break;` |
|     12 |  333 | `			case '\t': rc = ph7_result_string(pCtx,"\\t",2); break;` |
|     14 |  334 | `			default:` |
|     29 |  335 | `				zEsc[4] = zHex[(c >> 4) & 0x0F];` |
|     29 |  336 | `				zEsc[5] = zHex[c & 0x0F];` |
|     29 |  337 | `				rc = ph7_result_string(pCtx,zEsc,6);` |
|     28 |  338 | `				break;` |
|      - |  339 | `			}` |
|     52 |  340 | `		}else{` |
|   6317 |  341 | `			if( c == '"' \|\| c == '\\' ){` |
|      - |  342 | `				/* Escape the quote/backslash (php escapes the backslash` |
|      - |  343 | `				 * unconditionally — the old code wrongly tied it to` |
|      - |  344 | `				 * JSON_UNESCAPED_SLASHES, which governs '/' below) */` |
|     32 |  345 | `				rc = ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|   6302 |  346 | `			}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){` |
|      - |  347 | `				/* php escapes forward slashes by default */` |
|      9 |  348 | `				rc = ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|      5 |  349 | `			}else{` |
|   6279 |  350 | `				rc = SXRET_OK;` |
|      - |  351 | `			}` |
|   6317 |  352 | `			if( rc == SXRET_OK ){` |
|      - |  353 | `				/* Append character verbatim */` |
|   6317 |  354 | `				rc = ph7_result_string(pCtx,&c,(int)sizeof(char));` |
|   3156 |  355 | `			}` |
|      - |  356 | `		}` |
|   6425 |  357 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  358 | `			return rc;` |
|      - |  359 | `		}` |
|      5 |  360 | `	}` |
|   2041 |  361 | `	return ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|   1041 |  362 | `}` |
|      - |  363 | `/*` |
|      - |  364 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|      - |  365 | ` * According to wikipedia` |
|      - |  366 | ` * JSON's basic types are:` |
|      - |  367 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|      - |  368 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|      - |  369 | ` *   Boolean (true or false)` |
|      - |  370 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|      - |  371 | ` *    do not need to be of the same type)` |
|      - |  372 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|      - |  373 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|      - |  374 | ` *     be distinct from each other)` |
|      - |  375 | ` *   null (empty)` |
|      - |  376 | ` * Non-significant white space may be added freely around the "structural characters"` |
|      - |  377 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|      - |  378 | ` */` |
|      - |  379 | `/*` |
|      - |  380 | ` * Encode a native class's PRESENTED shape, php's get_properties handler answering` |
|      - |  381 | ` * the JSON purpose. Answers 0 when the class declares no hook, so the caller falls` |
|      - |  382 | ` * through to the ordinary property walk.` |
|      - |  383 | ` *` |
|      - |  384 | ` * php emits an OBJECT here whatever the presented keys look like — an ArrayObject` |
|      - |  385 | `` * holding a plain list is `{"0":1,"1":2}`, never `[1,2]` — so the list test the`` |
|      - |  386 | ` * array arm makes is deliberately not made.` |
|      - |  387 | ` */` |
|    106 |  388 | `static int VmJsonPresent(ph7_class_instance *pThis,json_private_data *pData)` |
|      5 |  389 | `{` |
|    111 |  390 | `	ph7_context *pCtx = pData->pCtx;` |
|      - |  391 | `	ph7_value sPresent;` |
|      - |  392 | `	int savedObject;` |
|    111 |  393 | `	PH7_MemObjInit(pThis->pVm,&sPresent);` |
|    111 |  394 | `	if( PH7_MemObjToHashmap(&sPresent) != SXRET_OK ){` |
|    ! 0 |  395 | `		PH7_MemObjRelease(&sPresent);` |
|    ! 0 |  396 | `		return 0;` |
|      - |  397 | `	}` |
|    111 |  398 | `	if( !PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|     86 |  399 | `		PH7_MemObjRelease(&sPresent);` |
|     86 |  400 | `		return 0;` |
|      - |  401 | `	}` |
|     26 |  402 | `	savedObject = pData->isObject;` |
|     26 |  403 | `	pData->isObject = 1;` |
|     26 |  404 | `	pData->isFirst = 1;` |
|     26 |  405 | `	JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|     26 |  406 | `	ph7_array_walk(&sPresent,VmJsonArrayEncode,pData);` |
|     26 |  407 | `	if( !pData->oom ){` |
|     26 |  408 | `		if( !pData->isFirst ){` |
|     15 |  409 | `			JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|      7 |  410 | `		}` |
|     26 |  411 | `		JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|     12 |  412 | `	}` |
|     26 |  413 | `	pData->isObject = savedObject;` |
|     26 |  414 | `	PH7_MemObjRelease(&sPresent);` |
|     26 |  415 | `	return 1;` |
|     58 |  416 | `}` |
|   6466 |  417 | `static sxi32 VmJsonEncode(` |
|      - |  418 | `	ph7_value *pIn,          /* Encode this value */` |
|      - |  419 | `	json_private_data *pData /* Context data */` |
|      5 |  420 | `	){` |
|   6471 |  421 | `		ph7_context *pCtx = pData->pCtx;` |
|   6471 |  422 | `		int iFlags = pData->iFlags;` |
|      - |  423 | `		int nByte;` |
|   6471 |  424 | `		if( ph7_value_is_resource(pIn) ){` |
|      - |  425 | `			/* php: a resource has no JSON representation — the whole encode` |
|      - |  426 | `			 * fails with JSON_ERROR_UNSUPPORTED_TYPE (PHL used to emit "null"` |
|      - |  427 | `			 * in silence, an answer php never gives). Under` |
|      - |  428 | `			 * JSON_PARTIAL_OUTPUT_ON_ERROR the substitute IS null, with the` |
|      - |  429 | `			 * error recorded. */` |
|      9 |  430 | `			if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){` |
|      3 |  431 | `				pCtx->pVm->json_rc = JSON_ERROR_UNSUPPORTED_TYPE;` |
|      3 |  432 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|      3 |  433 | `				return PH7_OK;` |
|      - |  434 | `			}` |
|      7 |  435 | `			pData->fail = 1;` |
|      7 |  436 | `			pData->failRc = JSON_ERROR_UNSUPPORTED_TYPE;` |
|      7 |  437 | `			return PH7_OK;` |
|   6463 |  438 | `		}else if( ph7_value_is_null(pIn) ){` |
|      - |  439 | `			/* null */` |
|     15 |  440 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|   6452 |  441 | `		}else if( ph7_value_is_bool(pIn) ){` |
|     28 |  442 | `			int iBool = ph7_value_to_bool(pIn);` |
|      - |  443 | `			int iLen;` |
|      - |  444 | `			/* true/false */` |
|     28 |  445 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|     28 |  446 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
|   6433 |  447 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|   1484 |  448 | `			if( ph7_value_is_float(pIn) ){` |
|     78 |  449 | `				double rVal = ph7_value_to_double(pIn);` |
|      - |  450 | `				/* php rejects Inf/NaN: json_encode returns FALSE with` |
|      - |  451 | `				 * json_last_error() == JSON_ERROR_INF_OR_NAN (they have no JSON` |
|      - |  452 | `				 * representation), instead of emitting the invalid bare token. */` |
|     78 |  453 | `				if( PH7_IS_NAN(rVal) \|\| PH7_IS_INF(rVal) ){` |
|     17 |  454 | `					if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){` |
|      - |  455 | `						/* php's substitute for an Inf/NaN member is 0 */` |
|     11 |  456 | `						pCtx->pVm->json_rc = JSON_ERROR_INF_OR_NAN;` |
|     11 |  457 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"0",(int)sizeof(char)));` |
|     11 |  458 | `						return PH7_OK;` |
|      - |  459 | `					}` |
|      7 |  460 | `					pData->fail = 1;` |
|      7 |  461 | `					pData->failRc = JSON_ERROR_INF_OR_NAN;` |
|      7 |  462 | `					return PH7_OK;` |
|      - |  463 | `				}` |
|      - |  464 | `				/* php's json float output follows serialize_precision` |
|      - |  465 | `				 * (shortest round-trip, like serialize/var_export), NOT the` |
|      - |  466 | `				 * echo/cast precision of 14 — with a lowercase exponent` |
|      - |  467 | `				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,` |
|      - |  468 | `				 * 1.0 -> 1, -0.0 -> -0. */` |
|     62 |  469 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,rVal,iFlags));` |
|     32 |  470 | `			}else{` |
|      - |  471 | `				const char *zNum;` |
|      - |  472 | `				/* Get a string representation of the number */` |
|    920 |  473 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|    920 |  474 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|      4 |  475 | `			}` |
|   5919 |  476 | `		}else if( ph7_value_is_string(pIn) ){` |
|   1180 |  477 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|      - |  478 | `				/* Encodes numeric strings as numbers (same float shapes). */` |
|      9 |  479 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|      9 |  480 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn),iFlags));` |
|      5 |  481 | `			}else{` |
|      - |  482 | `				const char *zIn;` |
|      - |  483 | `				/* Encode the string */` |
|   1172 |  484 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|   1172 |  485 | `				JSON_EMIT(pData,VmJsonEncodeString(pData,zIn,nByte,0));` |
|      4 |  486 | `			}` |
|   4843 |  487 | `		}else if( ph7_value_is_array(pIn) ){` |
|      - |  488 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|      - |  489 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|      - |  490 | `			 * object with stringified keys (PHP semantics). */` |
|   4097 |  491 | `			ph7_hashmap *pMap = (ph7_hashmap *)pIn->x.pOther;` |
|   8188 |  492 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|   4092 |  493 | `				\|\| !PH7_HashmapIsList(pMap);` |
|   4097 |  494 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|   4097 |  495 | `			int c = isObject ? '{' : '[';` |
|   4097 |  496 | `			int d = isObject ? '}' : ']';` |
|      - |  497 | `			/* An array the encoder is already inside of (reached through a` |
|      - |  498 | `			 * reference cycle) is php's JSON_ERROR_RECURSION — PHL used to` |
|      - |  499 | `			 * descend into it and answer a TRUNCATED nesting in silence.` |
|      - |  500 | `			 * JSON_PARTIAL_OUTPUT_ON_ERROR substitutes null for the cycle. */` |
|   4097 |  501 | `			if( VmJsonPathHolds(pData,(void *)pMap) ){` |
|      5 |  502 | `				if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){` |
|      3 |  503 | `					pCtx->pVm->json_rc = JSON_ERROR_RECURSION;` |
|      9 |  504 | `					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|      3 |  505 | `					return PH7_OK;` |
|      - |  506 | `				}` |
|      3 |  507 | `				pData->fail = 1;` |
|      3 |  508 | `				pData->failRc = JSON_ERROR_RECURSION;` |
|      3 |  509 | `				return PH7_OK;` |
|      - |  510 | `			}` |
|      - |  511 | `			/* php checks $depth where a container OPENS: one already enclosed` |
|      - |  512 | `			 * by $depth containers (scalars are exempt) is JSON_ERROR_DEPTH.` |
|      - |  513 | `			 * Under JSON_PARTIAL_OUTPUT_ON_ERROR php records the error and` |
|      - |  514 | `			 * keeps ENCODING past the limit — PHL follows until the stack` |
|      - |  515 | `			 * ceiling, where a null stands in for what it will not recurse` |
|      - |  516 | `			 * into. */` |
|   4093 |  517 | `			if( (ph7_int64)pData->nRecCount >= pData->nMaxDepth ){` |
|     11 |  518 | `				if( (iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR) == 0 ){` |
|      9 |  519 | `					pData->fail = 1;` |
|      9 |  520 | `					pData->failRc = JSON_ERROR_DEPTH;` |
|      9 |  521 | `					return PH7_OK;` |
|      - |  522 | `				}` |
|      3 |  523 | `				pCtx->pVm->json_rc = JSON_ERROR_DEPTH;` |
|      3 |  524 | `				if( pData->nRecCount >= PH7_JSON_DEPTH_CEILING ){` |
|    ! 0 |  525 | `					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    ! 0 |  526 | `					return PH7_OK;` |
|      - |  527 | `				}` |
|      1 |  528 | `			}` |
|   4085 |  529 | `			if( SySetPut(&pData->aPath,(const void *)&pMap) != SXRET_OK ){` |
|    ! 0 |  530 | `				pData->oom = 1;` |
|    ! 0 |  531 | `				return PH7_OK;` |
|      - |  532 | `			}` |
|      - |  533 | `			/* Encode the array */` |
|   4085 |  534 | `			pData->isObject = isObject;` |
|   4085 |  535 | `			pData->isFirst = 1;` |
|      - |  536 | `			/* Append the square bracket or curly braces */` |
|   4085 |  537 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|      - |  538 | `			/* Iterate throw array entries */` |
|   4085 |  539 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|   4085 |  540 | `			(void)SySetPop(&pData->aPath);` |
|      - |  541 | `			/* Bail if a nested append ran out of memory before the closer */` |
|   4085 |  542 | `			if( pData->oom ){` |
|    ! 0 |  543 | `				return PH7_OK;` |
|      - |  544 | `			}` |
|      - |  545 | `			/* Pretty-print: a non-empty container closes on its own line,` |
|      - |  546 | `			 * indented one level less than its members (isFirst is still 1` |
|      - |  547 | `			 * only when no entry was emitted -> keep "[]"/"{}" tight). */` |
|   4085 |  548 | `			if( !pData->isFirst ){` |
|   3959 |  549 | `				JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|   1977 |  550 | `			}` |
|      - |  551 | `			/* Append the closing square bracket or curly braces */` |
|   4085 |  552 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|   4085 |  553 | `			pData->isObject = savedObject;` |
|   2203 |  554 | `		}else if( ph7_value_is_object(pIn) ){` |
|    163 |  555 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|    163 |  556 | `			ph7_vm *pVm = pIn->pVm;` |
|    163 |  557 | `			ph7_class_method *pMethod = 0;` |
|    163 |  558 | `			int bProps = 1; /* encode the property view below (cleared when a` |
|      - |  559 | `			                 * jsonSerialize() result replaces it) */` |
|      - |  560 | `			/* An object the encoder is already inside of is php's` |
|      - |  561 | `			 * JSON_ERROR_RECURSION, checked BEFORE the jsonSerialize dispatch` |
|      - |  562 | `			 * (PHL used to re-dispatch until the C stack ran out — a segfault` |
|      - |  563 | ``			 * on `return $this;`). */`` |
|    163 |  564 | `			if( VmJsonPathHolds(pData,(void *)pThis) ){` |
|      5 |  565 | `				if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){` |
|    ! 0 |  566 | `					pCtx->pVm->json_rc = JSON_ERROR_RECURSION;` |
|     12 |  567 | `					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    ! 0 |  568 | `					return PH7_OK;` |
|      - |  569 | `				}` |
|      5 |  570 | `				pData->fail = 1;` |
|      5 |  571 | `				pData->failRc = JSON_ERROR_RECURSION;` |
|      5 |  572 | `				return PH7_OK;` |
|      - |  573 | `			}` |
|      - |  574 | `			/* If the object implements JsonSerializable, encode the value` |
|      - |  575 | `			 * returned by jsonSerialize() instead of its public properties.` |
|      - |  576 | `			 * An enum implementing it explicitly also takes this path (php). */` |
|    154 |  577 | `			if( pVm->pJsonSerializableClass` |
|    159 |  578 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|     35 |  579 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|     17 |  580 | `			}` |
|    159 |  581 | `			if( pMethod == 0 && (pThis->pClass->iFlags & PH7_CLASS_ENUM) != 0 ){` |
|      - |  582 | `				/* php 8.1: a BACKED enum case encodes as its backing value; a` |
|      - |  583 | `				 * pure enum case has no default serialization — json_encode` |
|      - |  584 | `				 * returns false. */` |
|     15 |  585 | `				ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pThis);` |
|     15 |  586 | `				if( pBacking ){` |
|     11 |  587 | `					pData->nRecCount++;` |
|     11 |  588 | `					VmJsonEncode(pBacking,pData);` |
|     11 |  589 | `					pData->nRecCount--;` |
|     10 |  590 | `				}else if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){` |
|      - |  591 | `					/* php's substitute for a non-backed case is 0 */` |
|      3 |  592 | `					pCtx->pVm->json_rc = JSON_ERROR_NON_BACKED_ENUM;` |
|      3 |  593 | `					JSON_EMIT(pData,ph7_result_string(pCtx,"0",(int)sizeof(char)));` |
|      2 |  594 | `				}else{` |
|      3 |  595 | `					pData->fail = 1;` |
|      3 |  596 | `					pData->failRc = JSON_ERROR_NON_BACKED_ENUM;` |
|      - |  597 | `				}` |
|     15 |  598 | `				return PH7_OK;` |
|      - |  599 | `			}` |
|    145 |  600 | `			if( pMethod ){` |
|      - |  601 | `				ph7_value sResult;` |
|      - |  602 | `				sxi32 rc;` |
|     35 |  603 | `				PH7_MemObjInit(pVm,&sResult);` |
|     35 |  604 | `				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|     35 |  605 | `				if( rc == PH7_EXCEPTION ){` |
|      - |  606 | `					/* Let jsonSerialize()'s throw propagate */` |
|      5 |  607 | `					PH7_MemObjRelease(&sResult);` |
|      5 |  608 | `					pData->exc = 1;` |
|      5 |  609 | `					return PH7_EXCEPTION;` |
|      - |  610 | `				}` |
|     30 |  611 | `				if( ph7_value_is_object(&sResult)` |
|     17 |  612 | `				 && (ph7_class_instance *)sResult.x.pOther == pThis ){` |
|      - |  613 | `					/* php's one self-reference exception: jsonSerialize()` |
|      - |  614 | `					 * returning $this encodes the object's own property view —` |
|      - |  615 | `					 * no re-dispatch, no recursion error. */` |
|      3 |  616 | `					PH7_MemObjRelease(&sResult);` |
|      2 |  617 | `				}else{` |
|     29 |  618 | `					bProps = 0;` |
|      - |  619 | `					/* Encode the returned value [scalar/array/object]. The` |
|      - |  620 | `					 * object stays ON the path while its replacement encodes` |
|      - |  621 | ``					 * (`return [$this]` is php's recursion error), and the`` |
|      - |  622 | `					 * result sits at the object's own nesting level — the old` |
|      - |  623 | `					 * nRecCount++ here indented a JSON_PRETTY_PRINT result one` |
|      - |  624 | `					 * level deeper than php and would have charged $depth for a` |
|      - |  625 | `					 * container php does not charge. */` |
|     29 |  626 | `					if( SySetPut(&pData->aPath,(const void *)&pThis) != SXRET_OK ){` |
|    ! 0 |  627 | `						PH7_MemObjRelease(&sResult);` |
|    ! 0 |  628 | `						pData->oom = 1;` |
|    ! 0 |  629 | `						return PH7_OK;` |
|      - |  630 | `					}` |
|     29 |  631 | `					VmJsonEncode(&sResult,pData);` |
|     29 |  632 | `					(void)SySetPop(&pData->aPath);` |
|     29 |  633 | `					PH7_MemObjRelease(&sResult);` |
|     29 |  634 | `					if( pData->exc ){` |
|    ! 0 |  635 | `						return PH7_EXCEPTION;` |
|      - |  636 | `					}` |
|     29 |  637 | `					if( pData->oom ){` |
|    ! 0 |  638 | `						return PH7_OK;` |
|      - |  639 | `					}` |
|      - |  640 | `				}` |
|     15 |  641 | `			}` |
|      - |  642 | `			/* php checks $depth where a container OPENS — the '{' of the` |
|      - |  643 | `			 * property view below; a SCALAR jsonSerialize() result and an enum` |
|      - |  644 | `			 * backing value are exempt, so the check sits here and not at the` |
|      - |  645 | `			 * arm's entry. The PARTIAL_OUTPUT rule mirrors the array arm's:` |
|      - |  646 | `			 * record the error, keep encoding, null at the stack ceiling. */` |
|    141 |  647 | `			if( bProps && (ph7_int64)pData->nRecCount >= pData->nMaxDepth ){` |
|      3 |  648 | `				if( (iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR) == 0 ){` |
|      3 |  649 | `					pData->fail = 1;` |
|      3 |  650 | `					pData->failRc = JSON_ERROR_DEPTH;` |
|      3 |  651 | `					return PH7_OK;` |
|      - |  652 | `				}` |
|    ! 0 |  653 | `				pCtx->pVm->json_rc = JSON_ERROR_DEPTH;` |
|    ! 0 |  654 | `				if( pData->nRecCount >= PH7_JSON_DEPTH_CEILING ){` |
|    ! 0 |  655 | `					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    ! 0 |  656 | `					return PH7_OK;` |
|      - |  657 | `				}` |
|    ! 0 |  658 | `			}` |
|    139 |  659 | `			if( bProps && SySetPut(&pData->aPath,(const void *)&pThis) != SXRET_OK ){` |
|    ! 0 |  660 | `				pData->oom = 1;` |
|    ! 0 |  661 | `				return PH7_OK;` |
|      - |  662 | `			}` |
|    139 |  663 | `			if( !bProps ){` |
|      - |  664 | `				/* jsonSerialize()'s result replaced the property view above */` |
|    125 |  665 | `			}else if( VmJsonPresent(pThis,pData) ){` |
|      - |  666 | `				/* A native class with php's get_properties handler: json is one of` |
|      - |  667 | `				 * the purposes that handler serves (php's ZEND_PROP_PURPOSE_JSON),` |
|      - |  668 | `				 * so a DateTime encodes as date/timezone_type/timezone and an` |
|      - |  669 | `				 * ArrayObject as its ELEMENTS — where walking the real slots below` |
|      - |  670 | `				 * finds nothing, every one of them being a hidden engine slot.` |
|      - |  671 | `				 * Handled inside VmJsonPresent so this arm is just the dispatch. */` |
|     26 |  672 | `				if( pData->exc ){` |
|    ! 0 |  673 | `					return PH7_EXCEPTION;` |
|      - |  674 | `				}` |
|     14 |  675 | `			}else{` |
|      - |  676 | `				SyHashEntry *pAttrEntry;` |
|      - |  677 | `				SySet sNames;` |
|      - |  678 | `				SyString *aName;` |
|      - |  679 | `				sxu32 iName,nName;` |
|      - |  680 | `				/* Encode the class instance: php serializes only PUBLIC` |
|      - |  681 | `				 * non-static properties, reading through a PHP 8.4 get hook` |
|      - |  682 | `				 * when one is declared (virtual properties included). The` |
|      - |  683 | `				 * names are SNAPSHOTTED first — a hook dispatched mid-walk may` |
|      - |  684 | `				 * re-enter an hAttr walk on this instance (the hash has a` |
|      - |  685 | `				 * single embedded loop cursor) or unset()/create properties;` |
|      - |  686 | `				 * names point into class-owned attr storage and each is` |
|      - |  687 | `				 * re-looked-up before use. */` |
|     86 |  688 | `				pData->isFirst = 1;` |
|      - |  689 | `				/* Append the curly braces */` |
|     86 |  690 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|     86 |  691 | `				SySetInit(&sNames,&pVm->sAllocator,sizeof(SyString));` |
|     86 |  692 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|    246 |  693 | `				while( (pAttrEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    164 |  694 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|    160 |  695 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN))` |
|    146 |  696 | `					 \|\| pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     47 |  697 | `						continue;` |
|      - |  698 | `					}` |
|    116 |  699 | `					if( SyStringLength(&pVmAttr->pAttr->sName) > 0` |
|    119 |  700 | `					 && SyStringData(&pVmAttr->pAttr->sName)[0] == 0 ){` |
|      - |  701 | `						/* A MANGLED key stored raw (the __PHP_Incomplete_Class` |
|      - |  702 | `						 * carrier): php's json encoder reads it as non-public` |
|      - |  703 | `						 * and skips it. */` |
|      5 |  704 | `						continue;` |
|      - |  705 | `					}` |
|    112 |  706 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     60 |  707 | `					 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      3 |  708 | `						continue; /* virtual set-only property: no value to encode (php) */` |
|      - |  709 | `					}` |
|    114 |  710 | `					SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|      4 |  711 | `				}` |
|     86 |  712 | `				aName = (SyString *)SySetBasePtr(&sNames);` |
|     86 |  713 | `				nName = SySetUsed(&sNames);` |
|    196 |  714 | `				for( iName = 0 ; iName < nName ; ++iName ){` |
|      - |  715 | `					VmClassAttr *pVmAttr;` |
|    114 |  716 | `					ph7_value *pAttrVal = 0;` |
|      - |  717 | `					ph7_value sHookVal;` |
|      - |  718 | `					sxi32 rcHk;` |
|    114 |  719 | `					pAttrEntry = PH7_ClassInstanceAttrEntry(pThis,aName[iName].zString,aName[iName].nByte);` |
|    114 |  720 | `					if( pAttrEntry == 0 ){` |
|    ! 0 |  721 | `						continue; /* unset by an earlier hook */` |
|      - |  722 | `					}` |
|    114 |  723 | `					pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|    114 |  724 | `					PH7_MemObjInit(pVm,&sHookVal);` |
|    114 |  725 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|    114 |  726 | `					if( rcHk == SXRET_OK ){` |
|     11 |  727 | `						pAttrVal = &sHookVal;` |
|    109 |  728 | `					}else if( rcHk == SXERR_NOTFOUND ){` |
|      - |  729 | `						/* Encode a COPY: the encoder casts scalars in place` |
|      - |  730 | `						 * (ph7_value_to_string), which must not corrupt the` |
|      - |  731 | `						 * live attribute slot. */` |
|    104 |  732 | `						ph7_value *pRaw = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    104 |  733 | `						if( pRaw ){` |
|    104 |  734 | `							PH7_MemObjStore(pRaw,&sHookVal);` |
|    104 |  735 | `							pAttrVal = &sHookVal;` |
|     50 |  736 | `						}` |
|     54 |  737 | `					}else{` |
|      - |  738 | `						/* the get hook threw — propagate like jsonSerialize() */` |
|    ! 0 |  739 | `						PH7_MemObjRelease(&sHookVal);` |
|    ! 0 |  740 | `						SySetRelease(&sNames);` |
|    ! 0 |  741 | `						pData->exc = 1;` |
|    ! 0 |  742 | `						return PH7_EXCEPTION;` |
|      - |  743 | `					}` |
|    114 |  744 | `					if( pAttrVal ){` |
|    114 |  745 | `						VmJsonObjectEncode(&pVmAttr->pAttr->sName,pAttrVal,pData);` |
|     55 |  746 | `					}` |
|    114 |  747 | `					PH7_MemObjRelease(&sHookVal);` |
|    114 |  748 | `					if( pData->exc ){` |
|    ! 0 |  749 | `						SySetRelease(&sNames);` |
|    ! 0 |  750 | `						return PH7_EXCEPTION; /* a nested jsonSerialize()/hook threw */` |
|      - |  751 | `					}` |
|    114 |  752 | `					if( pData->oom ){` |
|    ! 0 |  753 | `						SySetRelease(&sNames);` |
|    ! 0 |  754 | `						return PH7_OK;` |
|      - |  755 | `					}` |
|     59 |  756 | `				}` |
|     86 |  757 | `				SySetRelease(&sNames);` |
|      - |  758 | `				/* Pretty-print: non-empty object closes on its own indented line. */` |
|     86 |  759 | `				if( !pData->isFirst ){` |
|     70 |  760 | `					JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|     33 |  761 | `				}` |
|      - |  762 | `				/* Append the closing curly braces  */` |
|     86 |  763 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|      - |  764 | `			}` |
|    139 |  765 | `			if( bProps ){` |
|    111 |  766 | `				(void)SySetPop(&pData->aPath);` |
|     53 |  767 | `			}` |
|     72 |  768 | `		}else{` |
|      - |  769 | `			/* Can't happen */` |
|    ! 0 |  770 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|      - |  771 | `		}` |
|      - |  772 | `		/* All done */` |
|   6411 |  773 | `		return PH7_OK;` |
|   3238 |  774 | `}` |
|      - |  775 | `/*` |
|      - |  776 | ` * The following walker callback is invoked each time we need` |
|      - |  777 | ` * to encode an array to JSON.` |
|      - |  778 | ` */` |
|   5128 |  779 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 |  780 | `{` |
|   5133 |  781 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|   5133 |  782 | `	if( pJson->exc \|\| pJson->oom \|\| pJson->fail ){` |
|      - |  783 | `		/* A callback threw, OOM, or the value is unencodable (the result is` |
|      - |  784 | `		 * discarded) — return immediately. Depth is no longer decided here:` |
|      - |  785 | `		 * the container arms enforce json_encode's $depth where a '['/'{'` |
|      - |  786 | `		 * opens (the old flat 31 cap TRUNCATED a deep value in silence). */` |
|      3 |  787 | `		return PH7_OK;` |
|      - |  788 | `	}` |
|   5131 |  789 | `	if( !pJson->isFirst ){` |
|      - |  790 | `		/* Append the comma separating this entry from the previous one */` |
|   1163 |  791 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|    579 |  792 | `	}` |
|      - |  793 | `	/* Pretty-print: every member starts on its own indented line (one level` |
|      - |  794 | `	 * deeper than the enclosing container). */` |
|   5131 |  795 | `	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));` |
|   5131 |  796 | `	if( pJson->isObject ){` |
|      - |  797 | `		/* Outputs an object rather than an array */` |
|      - |  798 | `		const char *zKey;` |
|      - |  799 | `		int nByte;` |
|      - |  800 | `		/* Extract a string representation of the key */` |
|    798 |  801 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|      - |  802 | `		/* Append the quoted key and the colon. The key goes through the same` |
|      - |  803 | `		 * escaper as a string VALUE (php escapes both identically): emitting it` |
|      - |  804 | `		 * raw produced invalid JSON for any key holding '"', '\' or a control` |
|      - |  805 | `		 * character. */` |
|    798 |  806 | `		JSON_EMIT(pJson,VmJsonEncodeString(pJson,zKey,nByte,1));` |
|    798 |  807 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,":",(int)sizeof(char)));` |
|      - |  808 | `		/* php puts a space after the colon in pretty mode */` |
|    798 |  809 | `		if( pJson->iFlags & JSON_PRETTY_PRINT ){` |
|     22 |  810 | `			JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));` |
|     10 |  811 | `		}` |
|    397 |  812 | `	}` |
|      - |  813 | `	/* Encode the value */` |
|   5131 |  814 | `	pJson->nRecCount++;` |
|   5131 |  815 | `	VmJsonEncode(pValue,pJson);` |
|   5131 |  816 | `	pJson->nRecCount--;` |
|   5131 |  817 | `	pJson->isFirst = 0;` |
|   5131 |  818 | `	return PH7_OK;` |
|   2569 |  819 | `}` |
|      - |  820 | `/*` |
|      - |  821 | ` * The following walker callback is invoked each time we need to encode` |
|      - |  822 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|      - |  823 | ` */` |
|    110 |  824 | `static int VmJsonObjectEncode(const SyString *pAttr,ph7_value *pValue,void *pUserData)` |
|      4 |  825 | `{` |
|    114 |  826 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|    114 |  827 | `	if( pJson->exc \|\| pJson->oom \|\| pJson->fail ){` |
|      - |  828 | `		/* A callback threw, OOM, or the value is unencodable (the result is` |
|      - |  829 | `		 * discarded) — return immediately. Depth is no longer decided here:` |
|      - |  830 | `		 * the container arms enforce json_encode's $depth where a '['/'{'` |
|      - |  831 | `		 * opens (the old flat 31 cap TRUNCATED a deep value in silence). */` |
|    ! 0 |  832 | `		return PH7_OK;` |
|      - |  833 | `	}` |
|    114 |  834 | `	if( !pJson->isFirst ){` |
|      - |  835 | `		/* Append the comma separating this entry from the previous one */` |
|     47 |  836 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|     22 |  837 | `	}` |
|      - |  838 | `	/* Pretty-print: member on its own indented line, one level deeper. */` |
|    114 |  839 | `	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));` |
|      - |  840 | `	/* Append the quoted attribute name and the colon — escaped like a string` |
|      - |  841 | `	 * value, same as the array-key path above. */` |
|    114 |  842 | `	JSON_EMIT(pJson,VmJsonEncodeString(pJson,SyStringData(pAttr),(int)SyStringLength(pAttr),1));` |
|    114 |  843 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,":",(int)sizeof(char)));` |
|      - |  844 | `	/* php puts a space after the colon in pretty mode */` |
|    114 |  845 | `	if( pJson->iFlags & JSON_PRETTY_PRINT ){` |
|      9 |  846 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));` |
|      4 |  847 | `	}` |
|      - |  848 | `	/* Encode the value */` |
|    114 |  849 | `	pJson->nRecCount++;` |
|    114 |  850 | `	VmJsonEncode(pValue,pJson);` |
|    114 |  851 | `	pJson->nRecCount--;` |
|    114 |  852 | `	pJson->isFirst = 0;` |
|    114 |  853 | `	return PH7_OK;` |
|     59 |  854 | `}` |
|      - |  855 | `/*` |
|      - |  856 | ` * string json_encode(mixed $value [, int $flags = 0 [, int $depth = 512 ]])` |
|      - |  857 | ` *  Returns a string containing the JSON representation of value.` |
|      - |  858 | ` * Parameters` |
|      - |  859 | ` *  $value` |
|      - |  860 | ` *  The value being encoded. Can be any type except a resource` |
|      - |  861 | ` *  (a resource is JSON_ERROR_UNSUPPORTED_TYPE).` |
|      - |  862 | ` * $options` |
|      - |  863 | ` *  Bitmask consisting of:` |
|      - |  864 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|      - |  865 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|      - |  866 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|      - |  867 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|      - |  868 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|      - |  869 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|      - |  870 | ` *  JSON_BIGINT_AS_STRING   Decode flag (large ints as strings), not an encode flag.` |
|      - |  871 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|      - |  872 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|      - |  873 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|      - |  874 | ` * Return` |
|      - |  875 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|      - |  876 | ` */` |
|      - |  877 | `static const char * JsonErrorMsg(int rc); /* defined below, near json_last_error_msg */` |
|   1192 |  878 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  879 | `{` |
|      - |  880 | `	json_private_data sJson;` |
|      - |  881 | `	sxi32 rc;` |
|   1197 |  882 | `	if( nArg < 1 ){` |
|      - |  883 | `		/* Missing arguments,return FALSE */` |
|    ! 0 |  884 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  885 | `		return PH7_OK;` |
|      - |  886 | `	}` |
|      - |  887 | `	/* Prepare the JSON data */` |
|   1197 |  888 | `	sJson.nRecCount = 0;` |
|   1197 |  889 | `	sJson.pCtx = pCtx;` |
|   1197 |  890 | `	sJson.isFirst = 1;` |
|   1197 |  891 | `	sJson.iFlags = 0;` |
|   1197 |  892 | `	sJson.exc = 0;` |
|   1197 |  893 | `	sJson.oom = 0;` |
|   1197 |  894 | `	sJson.fail = 0;` |
|   1197 |  895 | `	sJson.failRc = JSON_ERROR_NON_BACKED_ENUM;` |
|   1197 |  896 | `	sJson.nMaxDepth = 512; /* php's default */` |
|   1197 |  897 | `	SySetInit(&sJson.aPath,&pCtx->pVm->sAllocator,sizeof(void *));` |
|   1197 |  898 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - |  899 | `		/* Extract option flags */` |
|    138 |  900 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|     68 |  901 | `	}` |
|   1197 |  902 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      - |  903 | `		/* $depth. Unlike json_decode's, php runs NO range screen here: 0 or a` |
|      - |  904 | `		 * negative value simply makes every container JSON_ERROR_DEPTH, and any` |
|      - |  905 | `		 * large int is accepted (the type screen has already run). */` |
|     27 |  906 | `		sJson.nMaxDepth = ph7_value_to_int64(apArg[2]);` |
|     27 |  907 | `		if( sJson.nMaxDepth > PH7_JSON_DEPTH_CEILING ){` |
|      - |  908 | `			/* Engine stack-safety bound (see PH7_JSON_DEPTH_CEILING). */` |
|    ! 0 |  909 | `			sJson.nMaxDepth = PH7_JSON_DEPTH_CEILING;` |
|    ! 0 |  910 | `		}` |
|     13 |  911 | `	}` |
|   1197 |  912 | `	pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|      - |  913 | `	/* Perform the encoding operation */` |
|   1197 |  914 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|   1197 |  915 | `	SySetRelease(&sJson.aPath);` |
|   1197 |  916 | `	if( sJson.oom ){` |
|      - |  917 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|      - |  918 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|    ! 0 |  919 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  920 | `	}` |
|   1197 |  921 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|      - |  922 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|      5 |  923 | `		return PH7_EXCEPTION;` |
|      - |  924 | `	}` |
|   1193 |  925 | `	if( sJson.fail ){` |
|      - |  926 | `		/* Unencodable value (Inf/NaN, or a php 8.1 non-backed enum case): the` |
|      - |  927 | `		 * whole encode fails — discard whatever was emitted and return FALSE. */` |
|     59 |  928 | `		pCtx->pVm->json_rc = sJson.failRc;` |
|     59 |  929 | `		if( sJson.iFlags & JSON_THROW_ON_ERROR ){` |
|      - |  930 | `			/* php: raise a JsonException carrying json_last_error_msg() instead` |
|      - |  931 | `			 * of returning FALSE. */` |
|     10 |  932 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|      6 |  933 | `				(sxi32)pCtx->pVm->json_rc,"%s",` |
|      6 |  934 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|      - |  935 | `		}` |
|     53 |  936 | `		ph7_result_bool(pCtx,0);` |
|     53 |  937 | `		return PH7_OK;` |
|      - |  938 | `	}` |
|      - |  939 | `	/* All done */` |
|   1135 |  940 | `	return PH7_OK;` |
|    601 |  941 | `}` |
|      - |  942 | `#undef JSON_EMIT` |
|      - |  943 | `/*` |
|      - |  944 | ` * int json_last_error(void)` |
|      - |  945 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|      - |  946 | ` * Parameters` |
|      - |  947 | ` *  None` |
|      - |  948 | ` * Return` |
|      - |  949 | ` *  Returns an integer, the value can be one of the following constants:` |
|      - |  950 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|      - |  951 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|      - |  952 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|      - |  953 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|      - |  954 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|      - |  955 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|      - |  956 | ` */` |
|    248 |  957 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  958 | `{` |
|    251 |  959 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  960 | `	/* Return the error code */` |
|    251 |  961 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    124 |  962 | `	SXUNUSED(nArg); /* cc warning */` |
|    124 |  963 | `	SXUNUSED(apArg);` |
|    251 |  964 | `	return PH7_OK;` |
|      3 |  965 | `}` |
|      - |  966 | `/*` |
|      - |  967 | ` * string json_last_error_msg(void)` |
|      - |  968 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|      - |  969 | ` * Parameters` |
|      - |  970 | ` *  None` |
|      - |  971 | ` * Return` |
|      - |  972 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|      - |  973 | ` *  code, or "No error" if no error has occurred.` |
|      - |  974 | ` */` |
|      - |  975 | `/* Human-readable message for a json_rc code. Shared by json_last_error_msg()` |
|      - |  976 | ` * and the JSON_THROW_ON_ERROR path (php's JsonException message is exactly this` |
|      - |  977 | ` * text). */` |
|     72 |  978 | `static const char * JsonErrorMsg(int rc)` |
|      2 |  979 | `{` |
|     74 |  980 | `	switch( rc ){` |
|     31 |  981 | `	case JSON_ERROR_NONE:            return "No error";` |
|    ! 0 |  982 | `	case JSON_ERROR_DEPTH:           return "Maximum stack depth exceeded";` |
|    ! 0 |  983 | `	case JSON_ERROR_STATE_MISMATCH:  return "State mismatch (invalid or malformed JSON)";` |
|      3 |  984 | `	case JSON_ERROR_CTRL_CHAR:       return "Control character error, possibly incorrectly encoded";` |
|     18 |  985 | `	case JSON_ERROR_SYNTAX:          return "Syntax error";` |
|      7 |  986 | `	case JSON_ERROR_UTF8:            return "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|      3 |  987 | `	case JSON_ERROR_RECURSION:       return "Recursion detected";` |
|      3 |  988 | `	case JSON_ERROR_INF_OR_NAN:     return "Inf and NaN cannot be JSON encoded";` |
|      5 |  989 | `	case JSON_ERROR_UNSUPPORTED_TYPE: return "Type is not supported";` |
|      3 |  990 | `	case JSON_ERROR_INVALID_PROPERTY_NAME: return "The decoded property name is invalid";` |
|      9 |  991 | `	case JSON_ERROR_UTF16:           return "Single unpaired UTF-16 surrogate in unicode escape";` |
|    ! 0 |  992 | `	case JSON_ERROR_NON_BACKED_ENUM: return "Non-backed enums have no default serialization";` |
|    ! 0 |  993 | `	default:                         return "Unknown error";` |
|      - |  994 | `	}` |
|     38 |  995 | `}` |
|     60 |  996 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  997 | `{` |
|     62 |  998 | `	ph7_result_string(pCtx,JsonErrorMsg(pCtx->pVm->json_rc),-1/* auto length */);` |
|     30 |  999 | `	SXUNUSED(nArg); /* cc warning */` |
|     30 | 1000 | `	SXUNUSED(apArg);` |
|     62 | 1001 | `	return PH7_OK;` |
|      2 | 1002 | `}` |
|      - | 1003 | `/* Possible tokens from the JSON tokenization process */` |
|      - | 1004 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|      - | 1005 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|      - | 1006 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|      - | 1007 | `#define JSON_TK_NULL    0x008 /* null */` |
|      - | 1008 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|      - | 1009 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|      - | 1010 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|      - | 1011 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|      - | 1012 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|      - | 1013 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|      - | 1014 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|      - | 1015 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|      - | 1016 | `/*` |
|      - | 1017 | ` * Tokenize an entire JSON input.` |
|      - | 1018 | ` * Get a single low-level token from the input file.` |
|      - | 1019 | ` * Update the stream pointer so that it points to the first` |
|      - | 1020 | ` * character beyond the extracted token.` |
|      - | 1021 | ` */` |
|   5462 | 1022 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|      4 | 1023 | `{` |
|   5466 | 1024 | `	int *pJsonErr = (int *)pUserData;` |
|      - | 1025 | `	SyString *pStr;` |
|      - | 1026 | `	int c;` |
|      - | 1027 | `	/* Ignore leading white spaces */` |
|   5528 | 1028 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|      - | 1029 | `		/* Advance the stream cursor */` |
|     64 | 1030 | `		if( pStream->zText[0] == '\n' ){` |
|      - | 1031 | `			/* Update line counter */` |
|      9 | 1032 | `			pStream->nLine++;` |
|      4 | 1033 | `		}` |
|     64 | 1034 | `		pStream->zText++;` |
|      2 | 1035 | `	}` |
|   5466 | 1036 | `	if( pStream->zText >= pStream->zEnd ){` |
|      - | 1037 | `		/* End of input reached */` |
|     12 | 1038 | `		SXUNUSED(pCtxData); /* cc warning */` |
|     26 | 1039 | `		return SXERR_EOF;` |
|      - | 1040 | `	}` |
|      - | 1041 | `	/* Record token starting position and line */` |
|   5442 | 1042 | `	pToken->nLine = pStream->nLine;` |
|   5442 | 1043 | `	pToken->pUserData = 0;` |
|   5442 | 1044 | `	pStr = &pToken->sData;` |
|   5442 | 1045 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|   5438 | 1046 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|   1699 | 1047 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|      - | 1048 | `			/* Single character */` |
|   5074 | 1049 | `			c = pStream->zText[0];` |
|      - | 1050 | `			/* Set token type */` |
|   5074 | 1051 | `			switch(c){` |
|   2418 | 1052 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|     80 | 1053 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|     63 | 1054 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|   2390 | 1055 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|     86 | 1056 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|     56 | 1057 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|    ! 0 | 1058 | `			default:` |
|    ! 0 | 1059 | `				break;` |
|      - | 1060 | `			}` |
|      - | 1061 | `			/* Advance the stream cursor */` |
|   5074 | 1062 | `			pStream->zText++;` |
|   2907 | 1063 | `	}else if( pStream->zText[0] == '"') {` |
|      - | 1064 | `		/* JSON string */` |
|    160 | 1065 | `		pStream->zText++;` |
|    160 | 1066 | `		pStr->zString++;` |
|      - | 1067 | `		/* Delimit the string. The backslash state is tracked explicitly: the old` |
|      - | 1068 | `		 * "the previous byte is not a backslash" test mis-read an ESCAPED` |
|      - | 1069 | `		 * backslash sitting before the closing quote, so the perfectly valid` |
|      - | 1070 | `		 * "\\" (a one-character string holding a backslash) was reported as an` |
|      - | 1071 | `		 * unterminated string — json_decode('"\\\\"') answered NULL with a` |
|      - | 1072 | `		 * syntax error where php answers "\". */` |
|    680 | 1073 | `		while( pStream->zText < pStream->zEnd ){` |
|    678 | 1074 | `			if( pStream->zText[0] == '\\' ){` |
|      - | 1075 | `				/* Whatever follows belongs to the escape, closing quote` |
|      - | 1076 | `				 * included; VmJsonDequoteString below decides if it is legal. */` |
|     91 | 1077 | `				pStream->zText++;` |
|     91 | 1078 | `				if( pStream->zText >= pStream->zEnd ){` |
|    ! 0 | 1079 | `					break;` |
|      - | 1080 | `				}` |
|     91 | 1081 | `				pStream->zText++;` |
|     91 | 1082 | `				continue;` |
|      - | 1083 | `			}` |
|    588 | 1084 | `			if( pStream->zText[0] == '"' ){` |
|    158 | 1085 | `				break;` |
|      - | 1086 | `			}` |
|    434 | 1087 | `			if( (unsigned char)pStream->zText[0] < 0x20 ){` |
|      - | 1088 | `				/* php: a control character must be escaped inside a JSON string;` |
|      - | 1089 | `				 * a raw one is JSON_ERROR_CTRL_CHAR (a literal newline included). */` |
|    ! 0 | 1090 | `				pToken->nType = JSON_TK_INVALID;` |
|    ! 0 | 1091 | `				*pJsonErr = JSON_ERROR_CTRL_CHAR;` |
|    ! 0 | 1092 | `				return SXERR_ABORT;` |
|      - | 1093 | `			}` |
|    434 | 1094 | `			pStream->zText++;` |
|      4 | 1095 | `		}` |
|    160 | 1096 | `		if( pStream->zText >= pStream->zEnd ){` |
|      - | 1097 | `			/* Missing closing '"'. php reports this as JSON_ERROR_CTRL_CHAR, not` |
|      - | 1098 | `			 * a syntax error: its scanner runs the string off the end of the` |
|      - | 1099 | `			 * input and lands in the same state an unescaped control character` |
|      - | 1100 | `			 * puts it in. */` |
|      3 | 1101 | `			pToken->nType = JSON_TK_INVALID;` |
|      3 | 1102 | `			*pJsonErr = JSON_ERROR_CTRL_CHAR;` |
|      2 | 1103 | `		}else{` |
|    158 | 1104 | `			pToken->nType = JSON_TK_STR;` |
|    158 | 1105 | `			pStream->zText++; /* Jump the closing double quotes */` |
|      - | 1106 | `		}` |
|    294 | 1107 | `	}else if( (pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]))` |
|    149 | 1108 | `		\|\| (pStream->zText[0] == '-' && &pStream->zText[1] < pStream->zEnd` |
|     22 | 1109 | `			&& pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1])) ){` |
|      - | 1110 | `		/* Number, held to JSON's grammar:` |
|      - | 1111 | `		 *   -?(0\|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?` |
|      - | 1112 | `		 * The old scanner took any digit soup, so "01", "-01", "5." and "1e"` |
|      - | 1113 | `		 * all DECODED (and json_validate() answered TRUE) where php reports` |
|      - | 1114 | `		 * JSON_ERROR_SYNTAX — accepting documents no JSON producer emits. */` |
|    182 | 1115 | `		int bBad = 0;` |
|    182 | 1116 | `		if( pStream->zText[0] == '-' ){` |
|     25 | 1117 | `			pStream->zText++;` |
|     11 | 1118 | `		}` |
|    182 | 1119 | `		if( pStream->zText[0] == '0' ){` |
|     21 | 1120 | `			pStream->zText++;` |
|      - | 1121 | `			/* JSON forbids a leading zero ahead of another digit */` |
|     21 | 1122 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     11 | 1123 | `				bBad = 1;` |
|      5 | 1124 | `			}` |
|     10 | 1125 | `		}` |
|    540 | 1126 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    362 | 1127 | `			pStream->zText++;` |
|      4 | 1128 | `		}` |
|    182 | 1129 | `		if( pStream->zText < pStream->zEnd && pStream->zText[0] == '.' ){` |
|     20 | 1130 | `			pStream->zText++;` |
|      - | 1131 | `			/* JSON requires at least one digit after the point */` |
|     20 | 1132 | `			if( pStream->zText >= pStream->zEnd \|\| pStream->zText[0] >= 0xc0 \|\| !SyisDigit(pStream->zText[0]) ){` |
|      5 | 1133 | `				bBad = 1;` |
|      2 | 1134 | `			}` |
|     36 | 1135 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     18 | 1136 | `				pStream->zText++;` |
|      2 | 1137 | `			}` |
|      9 | 1138 | `		}` |
|    182 | 1139 | `		if( pStream->zText < pStream->zEnd && (pStream->zText[0] == 'e' \|\| pStream->zText[0] == 'E') ){` |
|     21 | 1140 | `			pStream->zText++;` |
|     21 | 1141 | `			if( pStream->zText < pStream->zEnd && (pStream->zText[0] == '+' \|\| pStream->zText[0] == '-') ){` |
|      9 | 1142 | `				pStream->zText++;` |
|      4 | 1143 | `			}` |
|      - | 1144 | `			/* ...and at least one digit in the exponent */` |
|     21 | 1145 | `			if( pStream->zText >= pStream->zEnd \|\| pStream->zText[0] >= 0xc0 \|\| !SyisDigit(pStream->zText[0]) ){` |
|      7 | 1146 | `				bBad = 1;` |
|      3 | 1147 | `			}` |
|     35 | 1148 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     15 | 1149 | `				pStream->zText++;` |
|      1 | 1150 | `			}` |
|     10 | 1151 | `		}` |
|    182 | 1152 | `		if( bBad ){` |
|     21 | 1153 | `			pToken->nType = JSON_TK_INVALID;` |
|     21 | 1154 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|     21 | 1155 | `			return SXERR_ABORT;` |
|      - | 1156 | `		}` |
|    162 | 1157 | `		pToken->nType = JSON_TK_NUM;` |
|    124 | 1158 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|     18 | 1159 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|      - | 1160 | `			/* boolean true */` |
|      3 | 1161 | `			pToken->nType = JSON_TK_TRUE;` |
|      - | 1162 | `			/* Advance the stream cursor */` |
|      3 | 1163 | `			pStream->zText += sizeof("true")-1;` |
|     43 | 1164 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|     16 | 1165 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|      - | 1166 | `			/* boolean false */` |
|    ! 0 | 1167 | `			pToken->nType = JSON_TK_FALSE;` |
|      - | 1168 | `			/* Advance the stream cursor */` |
|    ! 0 | 1169 | `			pStream->zText += sizeof("false")-1;` |
|     42 | 1170 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|     16 | 1171 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|      - | 1172 | `			/* NULL */` |
|      3 | 1173 | `			pToken->nType = JSON_TK_NULL;` |
|      - | 1174 | `			/* Advance the stream cursor */` |
|      3 | 1175 | `			pStream->zText += sizeof("null")-1;` |
|      2 | 1176 | `	}else{` |
|      - | 1177 | `		/* Unexpected token — but a byte that is not valid UTF-8 is php's` |
|      - | 1178 | `		 * JSON_ERROR_UTF8, not a syntax error, wherever in the document it sits` |
|      - | 1179 | `		 * (a valid non-ASCII character outside a string stays a syntax error).` |
|      - | 1180 | `		 * The JSON_INVALID_UTF8_* flags do NOT reach here: php applies them` |
|      - | 1181 | `		 * inside string tokens only. */` |
|      - | 1182 | `		sxu32 nLen;` |
|     32 | 1183 | `		pToken->nType = JSON_TK_INVALID;` |
|     53 | 1184 | `		*pJsonErr = ((unsigned char)pStream->zText[0] >= 0x80` |
|     21 | 1185 | `			&& PH7_Utf8ReadStrict((const unsigned char *)pStream->zText,` |
|     18 | 1186 | `				(sxu32)(pStream->zEnd - pStream->zText),&nLen) < 0)` |
|     21 | 1187 | `			? JSON_ERROR_UTF8 : JSON_ERROR_SYNTAX;` |
|      - | 1188 | `		/* Advance the stream cursor */` |
|     32 | 1189 | `		pStream->zText++;` |
|      - | 1190 | `		/* Abort processing immediatley */` |
|     32 | 1191 | `		return SXERR_ABORT;` |
|      - | 1192 | `	}` |
|      - | 1193 | `	/* record token length */` |
|   5392 | 1194 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   5392 | 1195 | `	if( pToken->nType == JSON_TK_STR ){` |
|    158 | 1196 | `		pStr->nByte--;` |
|     77 | 1197 | `	}` |
|      - | 1198 | `	/* Return to the lexer */` |
|   5392 | 1199 | `	return SXRET_OK;` |
|   2735 | 1200 | `}` |
|      - | 1201 | `/*` |
|      - | 1202 | ` * JSON decoded input consumer callback signature.` |
|      - | 1203 | ` */` |
|      - | 1204 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|      - | 1205 | `/*` |
|      - | 1206 | ` * JSON decoder state is kept in the following structure.` |
|      - | 1207 | ` */` |
|      - | 1208 | `typedef struct json_decoder json_decoder;` |
|      - | 1209 | `struct json_decoder` |
|      - | 1210 | `{` |
|      - | 1211 | `	ph7_context *pCtx; /* Call context */` |
|      - | 1212 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|      - | 1213 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|      - | 1214 | `	int iFlags;        /* Configuration flags */` |
|      - | 1215 | `	int iUserFlags;    /* json_decode()'s own $flags (JSON_INVALID_UTF8_* live here) */` |
|      - | 1216 | `	SyToken *pIn;      /* Token stream */` |
|      - | 1217 | `	SyToken *pEnd;     /* End of the token stream */` |
|      - | 1218 | `	int rec_depth;     /* Recursion limit */` |
|      - | 1219 | `	int rec_count;     /* Current nesting level */` |
|      - | 1220 | `	int *pErr;         /* JSON decoding error if any */` |
|      - | 1221 | `};` |
|      - | 1222 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|      - | 1223 | `/* Forward declaration */` |
|      - | 1224 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|      - | 1225 | `/*` |
|      - | 1226 | ` * Read the four hex digits of a \uXXXX escape out of z[0..n-1]. Returns` |
|      - | 1227 | ` * SXRET_OK and the value, or SXERR_SYNTAX when fewer than four are there or one` |
|      - | 1228 | ` * is not a hex digit (php: JSON_ERROR_SYNTAX).` |
|      - | 1229 | ` */` |
|     62 | 1230 | `static sxi32 VmJsonHex4(const char *z,sxu32 n,sxu32 *pVal)` |
|      1 | 1231 | `{` |
|     63 | 1232 | `	sxu32 v = 0;` |
|      - | 1233 | `	int i;` |
|     63 | 1234 | `	if( n < 4 ){` |
|      3 | 1235 | `		return SXERR_SYNTAX;` |
|      - | 1236 | `	}` |
|    293 | 1237 | `	for( i = 0 ; i < 4 ; ++i ){` |
|    235 | 1238 | `		int c = (unsigned char)z[i];` |
|    235 | 1239 | `		if( c >= '0' && c <= '9' ){` |
|    147 | 1240 | `			v = (v << 4) \| (sxu32)(c - '0');` |
|    162 | 1241 | `		}else if( c >= 'a' && c <= 'f' ){` |
|     79 | 1242 | `			v = (v << 4) \| (sxu32)(c - 'a' + 10);` |
|     50 | 1243 | `		}else if( c >= 'A' && c <= 'F' ){` |
|      9 | 1244 | `			v = (v << 4) \| (sxu32)(c - 'A' + 10);` |
|      5 | 1245 | `		}else{` |
|      3 | 1246 | `			return SXERR_SYNTAX;` |
|      - | 1247 | `		}` |
|    117 | 1248 | `	}` |
|     59 | 1249 | `	*pVal = v;` |
|     59 | 1250 | `	return SXRET_OK;` |
|     32 | 1251 | `}` |
|      - | 1252 | `/*` |
|      - | 1253 | ` * Append one run of un-escaped string bytes, checking that it really is UTF-8:` |
|      - | 1254 | ` * php rejects a JSON document carrying a byte that is not text with` |
|      - | 1255 | ` * JSON_ERROR_UTF8, exactly as it refuses to ENCODE one. Only inside a string do` |
|      - | 1256 | ` * the JSON_INVALID_UTF8_* flags apply — a stray byte between tokens is an error` |
|      - | 1257 | ` * either way. Returns JSON_ERROR_NONE or JSON_ERROR_UTF8.` |
|      - | 1258 | ` */` |
|    132 | 1259 | `static int VmJsonAppendChecked(ph7_value *pWorker,const char *zIn,sxu32 nByte,int iFlags)` |
|      4 | 1260 | `{` |
|    136 | 1261 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|    136 | 1262 | `	sxu32 i = 0,iRun = 0,nLen;` |
|    298 | 1263 | `	while( i < nByte ){` |
|    178 | 1264 | `		if( z[i] < 0x80 ){` |
|    154 | 1265 | `			i++;` |
|    154 | 1266 | `			continue;` |
|      - | 1267 | `		}` |
|     25 | 1268 | `		if( PH7_Utf8ReadStrict(&z[i],nByte - i,&nLen) >= 0 ){` |
|      3 | 1269 | `			i += nLen;` |
|      3 | 1270 | `			continue;` |
|      - | 1271 | `		}` |
|     23 | 1272 | `		if( (iFlags & (JSON_INVALID_UTF8_IGNORE\|JSON_INVALID_UTF8_SUBSTITUTE)) == 0 ){` |
|     13 | 1273 | `			return JSON_ERROR_UTF8;` |
|      - | 1274 | `		}` |
|      - | 1275 | `		/* With BOTH flags set php's decoder substitutes while its encoder drops` |
|      - | 1276 | `		 * (probed both ways); the order of these two tests is that asymmetry,` |
|      - | 1277 | `		 * not an oversight. */` |
|      - | 1278 | `		/* Flush what is good, then stand in for the run */` |
|     11 | 1279 | `		if( i > iRun ){` |
|     11 | 1280 | `			ph7_value_string(pWorker,&zIn[iRun],(int)(i - iRun));` |
|      5 | 1281 | `		}` |
|     11 | 1282 | `		nLen = VmJsonBadUtf8Len(&z[i],nByte - i);` |
|     11 | 1283 | `		if( iFlags & JSON_INVALID_UTF8_SUBSTITUTE ){` |
|      7 | 1284 | `			ph7_value_string(pWorker,"\357\277\275",3); /* U+FFFD */` |
|      3 | 1285 | `		}` |
|     11 | 1286 | `		i += nLen;` |
|     11 | 1287 | `		iRun = i;` |
|      1 | 1288 | `	}` |
|    124 | 1289 | `	if( i > iRun ){` |
|    124 | 1290 | `		ph7_value_string(pWorker,&zIn[iRun],(int)(i - iRun));` |
|     60 | 1291 | `	}` |
|    124 | 1292 | `	return JSON_ERROR_NONE;` |
|     70 | 1293 | `}` |
|      - | 1294 | `/*` |
|      - | 1295 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|      - | 1296 | ` * the result in the given ph7_value. Returns JSON_ERROR_NONE, or the json_rc` |
|      - | 1297 | ` * php reports for the malformed escape it stopped on.` |
|      - | 1298 | ` *` |
|      - | 1299 | ` * The \uXXXX form used to fall through to the default branch, which dropped the` |
|      - | 1300 | ` * backslash and kept the rest as literal text: json_decode('"é"') answered` |
|      - | 1301 | ` * the five characters u00e9 instead of "é". \b was mangled the same way (it` |
|      - | 1302 | ` * answered "b"), and an escape JSON does not define (\q) was silently accepted` |
|      - | 1303 | ` * where php raises a syntax error.` |
|      - | 1304 | ` */` |
|    154 | 1305 | `static int VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker,int iFlags)` |
|      4 | 1306 | `{` |
|    158 | 1307 | `	const char *zIn = pStr->zString;` |
|    158 | 1308 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|      - | 1309 | `	const char *zCur;` |
|      - | 1310 | `	int c;` |
|      - | 1311 | `	/* Mark the value as a string */` |
|    158 | 1312 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|    110 | 1313 | `	for(;;){` |
|    224 | 1314 | `		zCur = zIn;` |
|    404 | 1315 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|    184 | 1316 | `			zIn++;` |
|      4 | 1317 | `		}` |
|    224 | 1318 | `		if( zIn > zCur ){` |
|    136 | 1319 | `			int rcChunk = VmJsonAppendChecked(pWorker,zCur,(sxu32)(zIn-zCur),iFlags);` |
|    136 | 1320 | `			if( rcChunk != JSON_ERROR_NONE ){` |
|     13 | 1321 | `				return rcChunk;` |
|      - | 1322 | `			}` |
|     60 | 1323 | `		}` |
|    212 | 1324 | `		zIn++;` |
|    212 | 1325 | `		if( zIn >= zEnd ){` |
|      - | 1326 | `			/* End of the input reached */` |
|    132 | 1327 | `			break;` |
|      - | 1328 | `		}` |
|     81 | 1329 | `		c = zIn[0];` |
|      - | 1330 | `		/* Unescape the character */` |
|     81 | 1331 | `		switch(c){` |
|      7 | 1332 | `		case '"':  ph7_value_string(pWorker,"\"",(int)sizeof(char)); break;` |
|      7 | 1333 | `		case '\\': ph7_value_string(pWorker,"\\",(int)sizeof(char)); break;` |
|      3 | 1334 | `		case '/':  ph7_value_string(pWorker,"/",(int)sizeof(char)); break;` |
|      3 | 1335 | `		case 'b':  ph7_value_string(pWorker,"\b",(int)sizeof(char)); break;` |
|      3 | 1336 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|      5 | 1337 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|      3 | 1338 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|      3 | 1339 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|     26 | 1340 | `		case 'u': {` |
|      - | 1341 | `			/* \uXXXX, and the surrogate PAIR that is JSON's only way to spell a` |
|      - | 1342 | `			 * code point above the BMP. An unpaired half is php's` |
|      - | 1343 | `			 * JSON_ERROR_UTF16, distinct from a malformed escape. */` |
|      - | 1344 | `			unsigned char zUtf8[4];` |
|     53 | 1345 | `			unsigned char *zW = zUtf8;` |
|     53 | 1346 | `			sxu32 cp,cpLow = 0; /* cpLow pre-set: MSVC /W4 flags the short-circuit as a maybe-uninitialized read */` |
|     53 | 1347 | `			if( VmJsonHex4(&zIn[1],(sxu32)(zEnd - zIn - 1),&cp) != SXRET_OK ){` |
|      9 | 1348 | `				return JSON_ERROR_SYNTAX;` |
|      - | 1349 | `			}` |
|     49 | 1350 | `			zIn += 4;` |
|     49 | 1351 | `			if( cp >= 0xDC00 && cp <= 0xDFFF ){` |
|      3 | 1352 | `				return JSON_ERROR_UTF16; /* a low half with no high half before it */` |
|      - | 1353 | `			}` |
|     47 | 1354 | `			if( cp >= 0xD800 && cp <= 0xDBFF ){` |
|     14 | 1355 | `				if( zEnd - zIn < 3 \|\| zIn[1] != '\\' \|\| zIn[2] != 'u'` |
|     10 | 1356 | `				 \|\| VmJsonHex4(&zIn[3],(sxu32)(zEnd - zIn - 3),&cpLow) != SXRET_OK` |
|     11 | 1357 | `				 \|\| cpLow < 0xDC00 \|\| cpLow > 0xDFFF ){` |
|      7 | 1358 | `					return JSON_ERROR_UTF16;` |
|      - | 1359 | `				}` |
|      9 | 1360 | `				cp = 0x10000 + ((cp - 0xD800) << 10) + (cpLow - 0xDC00);` |
|      9 | 1361 | `				zIn += 6;` |
|      4 | 1362 | `			}` |
|     41 | 1363 | `			SX_WRITE_UTF8(zW,cp);` |
|     41 | 1364 | `			ph7_value_string(pWorker,(const char *)zUtf8,(int)(zW - zUtf8));` |
|     41 | 1365 | `			break;` |
|      - | 1366 | `		}` |
|      1 | 1367 | `		default:` |
|      - | 1368 | `			/* Not one of JSON's nine escapes */` |
|      3 | 1369 | `			return JSON_ERROR_SYNTAX;` |
|      - | 1370 | `		}` |
|      - | 1371 | `		/* Advance the stream cursor */` |
|     67 | 1372 | `		zIn++;` |
|      1 | 1373 | `	}` |
|    132 | 1374 | `	return JSON_ERROR_NONE;` |
|     81 | 1375 | `}` |
|      - | 1376 | `/*` |
|      - | 1377 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|      - | 1378 | ` * According to wikipedia` |
|      - | 1379 | ` * JSON's basic types are:` |
|      - | 1380 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|      - | 1381 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|      - | 1382 | ` *   Boolean (true or false)` |
|      - | 1383 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|      - | 1384 | ` *    do not need to be of the same type)` |
|      - | 1385 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|      - | 1386 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|      - | 1387 | ` *     be distinct from each other)` |
|      - | 1388 | ` *   null (empty)` |
|      - | 1389 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|      - | 1390 | ` */` |
|   2702 | 1391 | `static sxi32 VmJsonDecode(` |
|      - | 1392 | `	json_decoder *pDecoder, /* JSON decoder */` |
|      - | 1393 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|      4 | 1394 | `	){` |
|      - | 1395 | `	ph7_value *pWorker; /* Worker variable */` |
|      - | 1396 | `	sxi32 rc;` |
|      - | 1397 | `	int rcQ;            /* VmJsonDequoteString() status */` |
|      - | 1398 | `	/* Nothing left to decode: the token stream is empty (a whitespace-only input` |
|      - | 1399 | `	 * tokenizes to NO tokens at all, so pIn/pEnd are both the NULL base pointer of an` |
|      - | 1400 | `	 * empty set) or a member value is missing after its colon ('{"a":'). Both are a` |
|      - | 1401 | `	 * syntax error for php; without this screen the reads below dereference pEnd. */` |
|   2706 | 1402 | `	if( pDecoder->pIn >= pDecoder->pEnd ){` |
|     26 | 1403 | `		*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|     26 | 1404 | `		return SXERR_ABORT;` |
|      - | 1405 | `	}` |
|   2682 | 1406 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|      - | 1407 | `		/* Scalar value */` |
|    226 | 1408 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|    226 | 1409 | `		if( pWorker == 0 ){` |
|    ! 0 | 1410 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 1411 | `			/* Abort the decoding operation immediately */` |
|    ! 0 | 1412 | `			return SXERR_ABORT;` |
|      - | 1413 | `		}` |
|      - | 1414 | `		/* Reflect the JSON image */` |
|    226 | 1415 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|      - | 1416 | `			/* Nullify the value.*/` |
|      3 | 1417 | `			ph7_value_null(pWorker);` |
|    225 | 1418 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|      - | 1419 | `			/* Boolean value */` |
|      3 | 1420 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|    223 | 1421 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|    148 | 1422 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|      - | 1423 | `			/*` |
|      - | 1424 | `			 * Numeric value.` |
|      - | 1425 | `			 * Get a string representation first then try to get a numeric` |
|      - | 1426 | `			 * value.` |
|      - | 1427 | `			 */` |
|    148 | 1428 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|      - | 1429 | `			/* Obtain a numeric representation */` |
|    148 | 1430 | `			PH7_MemObjToNumeric(pWorker);` |
|    144 | 1431 | `			if( (pDecoder->iUserFlags & JSON_BIGINT_AS_STRING) != 0` |
|     82 | 1432 | `			 && ph7_value_is_float(pWorker) ){` |
|      - | 1433 | `				/* php: an INTEGER literal beyond int64 normally lands on a` |
|      - | 1434 | `				 * float; under JSON_BIGINT_AS_STRING it stays the EXACT source` |
|      - | 1435 | `				 * text as a string. Only integer SHAPES qualify — a '.', 'e'` |
|      - | 1436 | `				 * or 'E' anywhere means the document asked for the float. */` |
|      - | 1437 | `				sxu32 iCh;` |
|      9 | 1438 | `				int bIntShape = 1;` |
|     93 | 1439 | `				for( iCh = 0 ; iCh < pStr->nByte ; ++iCh ){` |
|     88 | 1440 | `					if( pStr->zString[iCh] == '.' \|\| pStr->zString[iCh] == 'e'` |
|     86 | 1441 | `					 \|\| pStr->zString[iCh] == 'E' ){` |
|      5 | 1442 | `						bIntShape = 0;` |
|      5 | 1443 | `						break;` |
|      - | 1444 | `					}` |
|     43 | 1445 | `				}` |
|      9 | 1446 | `				if( bIntShape ){` |
|      5 | 1447 | `					ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|      2 | 1448 | `				}` |
|      4 | 1449 | `			}` |
|     76 | 1450 | `		}else{` |
|      - | 1451 | `			/* Dequote the string */` |
|     76 | 1452 | `			rcQ = VmJsonDequoteString(&pDecoder->pIn->sData,pWorker,pDecoder->iUserFlags);` |
|     76 | 1453 | `			if( rcQ != JSON_ERROR_NONE ){` |
|     25 | 1454 | `				*pDecoder->pErr = rcQ;` |
|     25 | 1455 | `				return SXERR_ABORT;` |
|      - | 1456 | `			}` |
|      - | 1457 | `		}` |
|      - | 1458 | `		/* Invoke the consumer callback */` |
|    202 | 1459 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|    202 | 1460 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 | 1461 | `			return SXERR_ABORT;` |
|      - | 1462 | `		}` |
|      - | 1463 | `		/* All done,advance the stream cursor */` |
|    202 | 1464 | `		pDecoder->pIn++;` |
|   2559 | 1465 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|      - | 1466 | `		ProcJsonConsumer xOld;` |
|      - | 1467 | `		void *pOld;` |
|      - | 1468 | `		/* php's $depth counts CONTAINERS: a '[' opening at 1-based nesting` |
|      - | 1469 | `		 * level L is JSON_ERROR_DEPTH when L >= $depth, an EMPTY container` |
|      - | 1470 | `		 * included ("[]" at $depth 1 already fails), while a scalar never` |
|      - | 1471 | `		 * consults $depth at all. rec_count holds L-1 here. */` |
|   2392 | 1472 | `		if( pDecoder->rec_count + 1 >= pDecoder->rec_depth ){` |
|     11 | 1473 | `			*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|     11 | 1474 | `			return SXERR_ABORT;` |
|      - | 1475 | `		}` |
|      - | 1476 | `		/* Array representation*/` |
|   2382 | 1477 | `		pDecoder->pIn++;` |
|      - | 1478 | `		/* Create a working array */` |
|   2382 | 1479 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   2382 | 1480 | `		if( pWorker == 0 ){` |
|    ! 0 | 1481 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 1482 | `			/* Abort the decoding operation immediately */` |
|    ! 0 | 1483 | `			return SXERR_ABORT;` |
|      - | 1484 | `		}` |
|      - | 1485 | `		/* Save the old consumer */` |
|   2382 | 1486 | `		xOld = pDecoder->xConsumer;` |
|   2382 | 1487 | `		pOld = pDecoder->pUserData;` |
|      - | 1488 | `		/* Set the new consumer */` |
|   2382 | 1489 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   2382 | 1490 | `		pDecoder->pUserData = pWorker;` |
|      - | 1491 | `		/* Decode the array */` |
|   1877 | 1492 | `		for(;;){` |
|      - | 1493 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|      - | 1494 | `			 * do this.` |
|      - | 1495 | `			 */` |
|   3792 | 1496 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|     38 | 1497 | `				pDecoder->pIn++;` |
|      4 | 1498 | `			}` |
|   3758 | 1499 | `			if( pDecoder->pIn >= pDecoder->pEnd ){` |
|      - | 1500 | `				/* Ran out of tokens before the closing ']': php rejects an` |
|      - | 1501 | `				 * unterminated array as a syntax error. */` |
|      8 | 1502 | `				*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      8 | 1503 | `				return SXERR_ABORT;` |
|      - | 1504 | `			}` |
|   3752 | 1505 | `			if( pDecoder->pIn->nType & JSON_TK_CSB /*']'*/ ){` |
|   1350 | 1506 | `				pDecoder->pIn++; /* Jump the trailing ']' */` |
|   1350 | 1507 | `				break;` |
|      - | 1508 | `			}` |
|      - | 1509 | `			/* Recurse and decode the entry */` |
|   2406 | 1510 | `			pDecoder->rec_count++;` |
|   2406 | 1511 | `			rc = VmJsonDecode(pDecoder,0);` |
|   2406 | 1512 | `			pDecoder->rec_count--;` |
|   2406 | 1513 | `			if( rc == SXERR_ABORT ){` |
|      - | 1514 | `				/* Abort processing immediately */` |
|   1027 | 1515 | `				return SXERR_ABORT;` |
|      - | 1516 | `			}` |
|      - | 1517 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   1380 | 1518 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   1374 | 1519 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|      - | 1520 | `					/* Unexpected token,abort immediatley */` |
|    ! 0 | 1521 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1522 | `					return SXERR_ABORT;` |
|      - | 1523 | `			}` |
|      4 | 1524 | `		}` |
|      - | 1525 | `		/* Restore the old consumer */` |
|   1350 | 1526 | `		pDecoder->xConsumer = xOld;` |
|   1350 | 1527 | `		pDecoder->pUserData = pOld;` |
|      - | 1528 | `		/* Invoke the old consumer on the decoded array */` |
|   1350 | 1529 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    745 | 1530 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|      - | 1531 | `		ProcJsonConsumer xOld;` |
|      - | 1532 | `		ph7_value *pKey;` |
|      - | 1533 | `		void *pOld;` |
|      - | 1534 | `		/* Same container rule as '[' above. */` |
|     72 | 1535 | `		if( pDecoder->rec_count + 1 >= pDecoder->rec_depth ){` |
|    ! 0 | 1536 | `			*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|    ! 0 | 1537 | `			return SXERR_ABORT;` |
|      - | 1538 | `		}` |
|      - | 1539 | `		/* Object representation*/` |
|     72 | 1540 | `		pDecoder->pIn++;` |
|      - | 1541 | `		/* Decode into a working array first; unless the caller asked for` |
|      - | 1542 | `		 * associative arrays (assoc=true / JSON_OBJECT_AS_ARRAY), it is converted` |
|      - | 1543 | `		 * to a stdClass below so json_decode('{...}') returns an object like php. */` |
|     72 | 1544 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|     72 | 1545 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|     72 | 1546 | `		if( pWorker == 0 \|\| pKey == 0){` |
|    ! 0 | 1547 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 1548 | `			/* Abort the decoding operation immediately */` |
|    ! 0 | 1549 | `			return SXERR_ABORT;` |
|      - | 1550 | `		}` |
|      - | 1551 | `		/* Save the old consumer */` |
|     72 | 1552 | `		xOld = pDecoder->xConsumer;` |
|     72 | 1553 | `		pOld = pDecoder->pUserData;` |
|      - | 1554 | `		/* Set the new consumer */` |
|     72 | 1555 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|     72 | 1556 | `		pDecoder->pUserData = pWorker;` |
|      - | 1557 | `		/* Decode the object */` |
|     69 | 1558 | `		for(;;){` |
|      - | 1559 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|      - | 1560 | `			 * do this.` |
|      - | 1561 | `			 */` |
|    156 | 1562 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|     17 | 1563 | `				pDecoder->pIn++;` |
|      3 | 1564 | `			}` |
|    142 | 1565 | `			if( pDecoder->pIn >= pDecoder->pEnd ){` |
|      - | 1566 | `				/* Ran out of tokens before the closing '}': php rejects an` |
|      - | 1567 | `				 * unterminated object as a syntax error. */` |
|      3 | 1568 | `				*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      3 | 1569 | `				return SXERR_ABORT;` |
|      - | 1570 | `			}` |
|    140 | 1571 | `			if( pDecoder->pIn->nType & JSON_TK_CCB /*'}'*/ ){` |
|     59 | 1572 | `				pDecoder->pIn++; /* Jump the trailing '}' */` |
|     59 | 1573 | `				break;` |
|      - | 1574 | `			}` |
|     80 | 1575 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|     84 | 1576 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|      - | 1577 | `					/* Syntax error,return immediately */` |
|    ! 0 | 1578 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1579 | `					return SXERR_ABORT;` |
|      - | 1580 | `			}` |
|      - | 1581 | `			/* Dequote the key */` |
|     84 | 1582 | `			rcQ = VmJsonDequoteString(&pDecoder->pIn->sData,pKey,pDecoder->iUserFlags);` |
|     84 | 1583 | `			if( rcQ != JSON_ERROR_NONE ){` |
|      3 | 1584 | `				*pDecoder->pErr = rcQ;` |
|      3 | 1585 | `				return SXERR_ABORT;` |
|      - | 1586 | `			}` |
|     82 | 1587 | `			if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|      - | 1588 | `				/* Decoding to an OBJECT: php refuses a property name whose` |
|      - | 1589 | `				 * FIRST byte is NUL ("\0...") with` |
|      - | 1590 | `				 * JSON_ERROR_INVALID_PROPERTY_NAME — that prefix is reserved` |
|      - | 1591 | `				 * for its mangled private/protected names. A NUL further in is` |
|      - | 1592 | `				 * legal, and array mode (assoc / JSON_OBJECT_AS_ARRAY /` |
|      - | 1593 | `				 * json_validate) takes any key. PHL used to build the property` |
|      - | 1594 | `				 * in silence. */` |
|      - | 1595 | `				int nKeyByte;` |
|     40 | 1596 | `				const char *zKey = ph7_value_to_string(pKey,&nKeyByte);` |
|     40 | 1597 | `				if( nKeyByte > 0 && zKey[0] == '\0' ){` |
|      3 | 1598 | `					*pDecoder->pErr = JSON_ERROR_INVALID_PROPERTY_NAME;` |
|      3 | 1599 | `					return SXERR_ABORT;` |
|      - | 1600 | `				}` |
|     17 | 1601 | `			}` |
|      - | 1602 | `			/* Jump the key and the colon */` |
|     80 | 1603 | `			pDecoder->pIn += 2;` |
|      - | 1604 | `			/* Recurse and decode the value */` |
|     80 | 1605 | `			pDecoder->rec_count++;` |
|     80 | 1606 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|     80 | 1607 | `			pDecoder->rec_count--;` |
|     80 | 1608 | `			if( rc == SXERR_ABORT ){` |
|      - | 1609 | `				/* Abort processing immediately */` |
|      8 | 1610 | `				return SXERR_ABORT;` |
|      - | 1611 | `			}` |
|      - | 1612 | `			/* Reset the internal buffer of the key */` |
|     73 | 1613 | `			ph7_value_reset_string_cursor(pKey);` |
|      - | 1614 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|      3 | 1615 | `		}` |
|      - | 1616 | `		/* Restore the old consumer */` |
|     59 | 1617 | `		pDecoder->xConsumer = xOld;` |
|     59 | 1618 | `		pDecoder->pUserData = pOld;` |
|      - | 1619 | `		/* php returns a stdClass for a JSON object (one dynamic property per member,` |
|      - | 1620 | `		 * nested objects already converted by the recursion) unless assoc was asked. */` |
|     59 | 1621 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|     23 | 1622 | `			PH7_MemObjToObject(pWorker);` |
|     10 | 1623 | `		}` |
|      - | 1624 | `		/* Invoke the old consumer on the decoded object*/` |
|     59 | 1625 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|      - | 1626 | `		/* Release the key */` |
|     59 | 1627 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|     31 | 1628 | `	}else{` |
|      - | 1629 | `		/* Unexpected token */` |
|    ! 0 | 1630 | `		return SXERR_ABORT; /* Abort immediately */` |
|      - | 1631 | `	}` |
|      - | 1632 | `	/* Release the worker variable */` |
|   1604 | 1633 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   1604 | 1634 | `	return SXRET_OK;` |
|   1355 | 1635 | `}` |
|      - | 1636 | `/*` |
|      - | 1637 | ` * The following JSON decoder callback is invoked each time` |
|      - | 1638 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|      - | 1639 | ` * is being decoded.` |
|      - | 1640 | ` */` |
|   1446 | 1641 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|      4 | 1642 | `{` |
|   1450 | 1643 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 1644 | `	/* Insert the entry */` |
|   1450 | 1645 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|    723 | 1646 | `	SXUNUSED(pCtx); /* cc warning */` |
|      - | 1647 | `	/* All done */` |
|   1450 | 1648 | `	return SXRET_OK;` |
|      4 | 1649 | `}` |
|      - | 1650 | `/*` |
|      - | 1651 | ` * Standard JSON decoder callback.` |
|      - | 1652 | ` */` |
|    154 | 1653 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|      4 | 1654 | `{` |
|      - | 1655 | `	/* Return the value directly */` |
|    158 | 1656 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|     77 | 1657 | `	SXUNUSED(pKey); /* cc warning */` |
|     77 | 1658 | `	SXUNUSED(pUserData);` |
|      - | 1659 | `	/* All done */` |
|    158 | 1660 | `	return SXRET_OK;` |
|      4 | 1661 | `}` |
|      - | 1662 | `/*` |
|      - | 1663 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 512[,int $options = 0 ]]])` |
|      - | 1664 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|      - | 1665 | ` * Parameters` |
|      - | 1666 | ` *  $json` |
|      - | 1667 | ` *    The json string being decoded.` |
|      - | 1668 | ` * $assoc` |
|      - | 1669 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|      - | 1670 | ` * $depth` |
|      - | 1671 | ` *   User specified recursion depth.` |
|      - | 1672 | ` * $options` |
|      - | 1673 | ` *   Bitmask of JSON decode options: JSON_OBJECT_AS_ARRAY (objects decode as` |
|      - | 1674 | ` *   associative arrays when $assoc is NULL), JSON_BIGINT_AS_STRING (an integer` |
|      - | 1675 | ` *   beyond int64 stays the exact source text instead of a float),` |
|      - | 1676 | ` *   JSON_INVALID_UTF8_IGNORE/_SUBSTITUTE and JSON_THROW_ON_ERROR` |
|      - | 1677 | ` * Return` |
|      - | 1678 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|      - | 1679 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|      - | 1680 | ` *  or if the encoded data is deeper than the recursion limit.` |
|      - | 1681 | ` */` |
|      - | 1682 | `/*` |
|      - | 1683 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|      - | 1684 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|      - | 1685 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|      - | 1686 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|      - | 1687 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|      - | 1688 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|      - | 1689 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|      - | 1690 | ` */` |
|    276 | 1691 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth,int iUserFlags)` |
|      4 | 1692 | `{` |
|    280 | 1693 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1694 | `	json_decoder sDecoder;` |
|      - | 1695 | `	SySet sToken;` |
|      - | 1696 | `	SyLex sLex;` |
|      - | 1697 | `	sxi32 rc;` |
|      - | 1698 | `	/* Clear JSON error code */` |
|    280 | 1699 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|      - | 1700 | `	/* Tokenize the input */` |
|    280 | 1701 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|    280 | 1702 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|    280 | 1703 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|    280 | 1704 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|      - | 1705 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|     54 | 1706 | `		SyLexRelease(&sLex);` |
|     54 | 1707 | `		SySetRelease(&sToken);` |
|     54 | 1708 | `		return pVm->json_rc;` |
|      - | 1709 | `	}` |
|      - | 1710 | `	/* Fill the decoder */` |
|    228 | 1711 | `	sDecoder.pCtx = pCtx;` |
|    228 | 1712 | `	sDecoder.pErr = &pVm->json_rc;` |
|    228 | 1713 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    228 | 1714 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|    228 | 1715 | `	sDecoder.iFlags = 0;` |
|    228 | 1716 | `	if( iAssoc ){` |
|      - | 1717 | `		/* Returned objects will be converted into associative arrays */` |
|    135 | 1718 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|     66 | 1719 | `	}` |
|    228 | 1720 | `	sDecoder.iUserFlags = iUserFlags;` |
|      - | 1721 | `	/* php's $depth (default 512; the callers' ValueError screens guarantee` |
|      - | 1722 | `	 * 1..INT_MAX-1), bounded by the engine's stack-safety ceiling. The old code` |
|      - | 1723 | `	 * CLAMPED it to an engine limit of 32, so a 40-deep document php decodes` |
|      - | 1724 | `	 * answered NULL/JSON_ERROR_DEPTH. Recursion is bounded by the INPUT's` |
|      - | 1725 | `	 * actual nesting, never by the requested ceiling. */` |
|    228 | 1726 | `	sDecoder.rec_depth = nDepth > PH7_JSON_DEPTH_CEILING ? PH7_JSON_DEPTH_CEILING : nDepth;` |
|    228 | 1727 | `	sDecoder.rec_count = 0;` |
|      - | 1728 | `	/* Set a default consumer */` |
|    228 | 1729 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|    228 | 1730 | `	sDecoder.pUserData = 0;` |
|      - | 1731 | `	/* Decode the raw JSON input */` |
|    228 | 1732 | `	rc = VmJsonDecode(&sDecoder,0);` |
|    228 | 1733 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|      - | 1734 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|    ! 0 | 1735 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1736 | `	}` |
|    228 | 1737 | `	if( pVm->json_rc == JSON_ERROR_NONE && sDecoder.pIn < sDecoder.pEnd ){` |
|      - | 1738 | `		/* php requires the whole input to be ONE JSON value; tokens left after a` |
|      - | 1739 | `		 * complete value (e.g. '"a":1', '{}x', '1 2') are a syntax error. */` |
|      3 | 1740 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|      1 | 1741 | `	}` |
|      - | 1742 | `	/* Clean-up the mess left behind */` |
|    228 | 1743 | `	SyLexRelease(&sLex);` |
|    228 | 1744 | `	SySetRelease(&sToken);` |
|    228 | 1745 | `	return pVm->json_rc;` |
|    142 | 1746 | `}` |
|    248 | 1747 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1748 | `{` |
|      - | 1749 | `	const char *zIn;` |
|      - | 1750 | `	int nByte;` |
|    252 | 1751 | `	int iAssoc = 0;` |
|    252 | 1752 | `	int nDepth = 512;` |
|    252 | 1753 | `	int iFlags = 0;` |
|      - | 1754 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1755 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|    252 | 1756 | `	if( nArg < 1 ){` |
|      - | 1757 | `		/* Missing/Invalid arguments, return NULL */` |
|    ! 0 | 1758 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1759 | `		return PH7_OK;` |
|      - | 1760 | `	}` |
|    252 | 1761 | `	if( nArg > 3 && ph7_value_is_int(apArg[3]) ){` |
|      - | 1762 | `		/* $flags (JSON_OBJECT_AS_ARRAY / JSON_BIGINT_AS_STRING /` |
|      - | 1763 | `		 * JSON_INVALID_UTF8_* / JSON_THROW_ON_ERROR). */` |
|     40 | 1764 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     19 | 1765 | `	}` |
|      - | 1766 | `	/* Extract the JSON string */` |
|    252 | 1767 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    252 | 1768 | `	if( nByte < 1 ){` |
|      - | 1769 | `		/* Empty string: php records a syntax error (json_last_error() == 4) and` |
|      - | 1770 | `		 * returns NULL, or raises a JsonException with JSON_THROW_ON_ERROR. */` |
|      9 | 1771 | `		pCtx->pVm->json_rc = JSON_ERROR_SYNTAX;` |
|      9 | 1772 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|    ! 0 | 1773 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|    ! 0 | 1774 | `				JSON_ERROR_SYNTAX,"%s",JsonErrorMsg(JSON_ERROR_SYNTAX));` |
|      - | 1775 | `		}` |
|      9 | 1776 | `		ph7_result_null(pCtx);` |
|      9 | 1777 | `		return PH7_OK;` |
|      - | 1778 | `	}` |
|    246 | 1779 | `	if( nArg > 1 ){` |
|      - | 1780 | `		/* php's $associative is a ?bool: an explicit true/false decides on its` |
|      - | 1781 | `		 * own (false beats the flag), and only NULL lets JSON_OBJECT_AS_ARRAY` |
|      - | 1782 | `		 * answer instead. */` |
|    149 | 1783 | `		if( ph7_value_is_null(apArg[1]) ){` |
|      7 | 1784 | `			iAssoc = (iFlags & JSON_OBJECT_AS_ARRAY) != 0;` |
|      4 | 1785 | `		}else{` |
|    143 | 1786 | `			iAssoc = ph7_value_to_bool(apArg[1]) != 0;` |
|      - | 1787 | `		}` |
|     73 | 1788 | `	}` |
|    246 | 1789 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      - | 1790 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);` |
|      - | 1791 | `		 * read as int64 so a value above INT_MAX is detected, not truncated. */` |
|     66 | 1792 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);` |
|      - | 1793 | `		/* php clears the json error state before validating $depth, so a caught` |
|      - | 1794 | `		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal` |
|      - | 1795 | `		 * path resets it again inside VmJsonDecodeInput). */` |
|     66 | 1796 | `		pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|     66 | 1797 | `		if( nWant <= 0 ){` |
|      9 | 1798 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1799 | `				"json_decode(): Argument #3 ($depth) must be greater than 0");` |
|      - | 1800 | `		}` |
|     58 | 1801 | `		if( nWant > 2147483647 ){` |
|      3 | 1802 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1803 | `				"json_decode(): Argument #3 ($depth) must be less than 2147483647");` |
|      - | 1804 | `		}` |
|     56 | 1805 | `		nDepth = (int)nWant;` |
|     27 | 1806 | `	}` |
|      - | 1807 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|      - | 1808 | `	 * call-context result; on failure we replace it with NULL (or throw). */` |
|    236 | 1809 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth,iFlags) != JSON_ERROR_NONE ){` |
|      - | 1810 | `		/* Something goes wrong while decoding JSON input. */` |
|    105 | 1811 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|      - | 1812 | `			/* php: raise a JsonException carrying json_last_error_msg() text. */` |
|     11 | 1813 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|      6 | 1814 | `				(sxi32)pCtx->pVm->json_rc,"%s",` |
|      6 | 1815 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|      - | 1816 | `		}` |
|     99 | 1817 | `		ph7_result_null(pCtx);` |
|     48 | 1818 | `	}` |
|      - | 1819 | `	/* All done */` |
|    230 | 1820 | `	return PH7_OK;` |
|    128 | 1821 | `}` |
|      - | 1822 | `/*` |
|      - | 1823 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|      - | 1824 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|      - | 1825 | ` * Parameters` |
|      - | 1826 | ` *  $json   The string to validate.` |
|      - | 1827 | ` *  $depth  Maximum nesting depth (php's default of 512, honored verbatim).` |
|      - | 1828 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|      - | 1829 | ` * Return` |
|      - | 1830 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|      - | 1831 | ` */` |
|     58 | 1832 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1833 | `{` |
|     61 | 1834 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1835 | `	const char *zIn;` |
|      - | 1836 | `	int nByte;` |
|     61 | 1837 | `	int nDepth = 512;` |
|     61 | 1838 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1839 | `		/* Missing/Invalid argument: not valid JSON */` |
|    ! 0 | 1840 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1841 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1842 | `		return PH7_OK;` |
|      - | 1843 | `	}` |
|      - | 1844 | `	/* Extract the JSON string */` |
|     61 | 1845 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|     61 | 1846 | `	if( nByte < 1 ){` |
|      - | 1847 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|      - | 1848 | `		 * silently, json_validate must record the syntax error) */` |
|      6 | 1849 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|      6 | 1850 | `		ph7_result_bool(pCtx,0);` |
|      6 | 1851 | `		return PH7_OK;` |
|      - | 1852 | `	}` |
|     57 | 1853 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1854 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */` |
|     24 | 1855 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|      - | 1856 | `		/* Clear the json error state before validating $depth (php parity), so a` |
|      - | 1857 | `		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */` |
|     24 | 1858 | `		pVm->json_rc = JSON_ERROR_NONE;` |
|     24 | 1859 | `		if( nWant <= 0 ){` |
|      5 | 1860 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1861 | `				"json_validate(): Argument #2 ($depth) must be greater than 0");` |
|      - | 1862 | `		}` |
|     20 | 1863 | `		if( nWant > 2147483647 ){` |
|      3 | 1864 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1865 | `				"json_validate(): Argument #2 ($depth) must be less than 2147483647");` |
|      - | 1866 | `		}` |
|     18 | 1867 | `		nDepth = (int)nWant;` |
|      8 | 1868 | `	}` |
|      - | 1869 | `	/* php's only ACCEPTED $flags value here is JSON_INVALID_UTF8_IGNORE, which` |
|      - | 1870 | `	 * makes a payload with undecodable bytes VALID; it rides the same rail as` |
|      - | 1871 | `	 * json_decode's. Any other bit is a ValueError naming the one flag there is --` |
|      - | 1872 | `	 * php refuses the whole json_decode set for this function, and PHL used to take` |
|      - | 1873 | `	 * whatever it was given and validate on. Decode in associative mode so the` |
|      - | 1874 | `	 * "objects are returned as an array" warning is not raised - the decoded value` |
|      - | 1875 | `	 * is discarded, only its validity matters. */` |
|      - | 1876 | `	{` |
|     32 | 1877 | `		int iFlags = (nArg > 2 && ph7_value_is_int(apArg[2]))` |
|     34 | 1878 | `			? ph7_value_to_int(apArg[2]) : 0;` |
|     51 | 1879 | `		if( (iFlags & ~JSON_INVALID_UTF8_IGNORE) != 0 ){` |
|      5 | 1880 | `			pVm->json_rc = JSON_ERROR_NONE;` |
|      5 | 1881 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1882 | `				"json_validate(): Argument #3 ($flags) must be a valid flag "` |
|      - | 1883 | `				"(allowed flags: JSON_INVALID_UTF8_IGNORE)");` |
|      - | 1884 | `		}` |
|     69 | 1885 | `		ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth,iFlags)` |
|     22 | 1886 | `			== JSON_ERROR_NONE);` |
|      - | 1887 | `	}` |
|     47 | 1888 | `	return PH7_OK;` |
|     32 | 1889 | `}` |
|      - | 1890 |  |
