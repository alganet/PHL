# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 935/1020 lines (91.67%)

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
|   4698 |   63 | `static int VmJsonPathHolds(json_private_data *pData,void *pPtr)` |
|      5 |   64 | `{` |
|   4703 |   65 | `	void **apEntry = (void **)SySetBasePtr(&pData->aPath);` |
|   4703 |   66 | `	sxu32 i,n = SySetUsed(&pData->aPath);` |
| 790901 |   67 | `	for( i = 0 ; i < n ; ++i ){` |
| 786211 |   68 | `		if( apEntry[i] == pPtr ){` |
|      9 |   69 | `			return 1;` |
|      - |   70 | `		}` |
| 393104 |   71 | `	}` |
|   4695 |   72 | `	return 0;` |
|   2354 |   73 | `}` |
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
|     76 |   89 | `static sxi32 VmJsonEmitReal(ph7_context *pCtx,double rVal,int iFlags)` |
|      2 |   90 | `{` |
|      - |   91 | `	SyBlob sNum;` |
|      - |   92 | `	char *z;` |
|      - |   93 | `	sxu32 i,n;` |
|      - |   94 | `	sxi32 rc;` |
|     78 |   95 | `	int bFrac = 0;` |
|     78 |   96 | `	SyBlobInit(&sNum,&pCtx->pVm->sAllocator);` |
|     78 |   97 | `	PH7_AppendShortestReal(&sNum,rVal);` |
|     78 |   98 | `	z = (char *)SyBlobData(&sNum);` |
|     78 |   99 | `	n = SyBlobLength(&sNum);` |
|     78 |  100 | `	if( z == 0 \|\| n < 1 ){` |
|    ! 0 |  101 | `		SyBlobRelease(&sNum);` |
|    ! 0 |  102 | `		return SXERR_MEM; /* treated as OOM by JSON_EMIT */` |
|      - |  103 | `	}` |
|    434 |  104 | `	for( i = 0 ; i < n ; i++ ){` |
|    358 |  105 | `		if( z[i] == 'E' ){` |
|     11 |  106 | `			z[i] = 'e';` |
|      5 |  107 | `		}` |
|    358 |  108 | `		if( z[i] == 'e' \|\| z[i] == '.' ){` |
|     49 |  109 | `			bFrac = 1;` |
|     24 |  110 | `		}` |
|    180 |  111 | `	}` |
|     78 |  112 | `	if( !bFrac && (iFlags & JSON_PRESERVE_ZERO_FRACTION) != 0 ){` |
|     11 |  113 | `		if( SyBlobAppend(&sNum,".0",2) != SXRET_OK ){` |
|    ! 0 |  114 | `			SyBlobRelease(&sNum);` |
|    ! 0 |  115 | `			return SXERR_MEM;` |
|      - |  116 | `		}` |
|     11 |  117 | `		z = (char *)SyBlobData(&sNum);` |
|     11 |  118 | `		n = SyBlobLength(&sNum);` |
|      5 |  119 | `	}` |
|     78 |  120 | `	rc = ph7_result_string(pCtx,(const char *)z,(int)n);` |
|     78 |  121 | `	SyBlobRelease(&sNum);` |
|     78 |  122 | `	return rc;` |
|     40 |  123 | `}` |
|      - |  124 | `/*` |
|      - |  125 | ` * JSON_PRETTY_PRINT helper: emit a newline followed by (depth * 4) spaces, so a` |
|      - |  126 | ` * container's members are laid out one-per-line and indented like php. A no-op` |
|      - |  127 | ` * unless JSON_PRETTY_PRINT is set. Returns SXRET_OK or an OOM status; callers` |
|      - |  128 | ` * wrap it in JSON_EMIT so an allocation failure trips the ->oom rail.` |
|      - |  129 | ` */` |
|  10540 |  130 | `static sxi32 VmJsonPretty(json_private_data *pJson,int depth)` |
|      5 |  131 | `{` |
|  10545 |  132 | `	ph7_context *pCtx = pJson->pCtx;` |
|      - |  133 | `	sxi32 rc;` |
|      - |  134 | `	int i;` |
|  10545 |  135 | `	if( (pJson->iFlags & JSON_PRETTY_PRINT) == 0 ){` |
|  10377 |  136 | `		return SXRET_OK;` |
|      - |  137 | `	}` |
|    171 |  138 | `	rc = ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    417 |  139 | `	for( i = 0 ; i < depth && rc == SXRET_OK ; ++i ){` |
|    249 |  140 | `		rc = ph7_result_string(pCtx,"    ",(int)sizeof("    ")-1);` |
|    126 |  141 | `	}` |
|    171 |  142 | `	return rc;` |
|   5275 |  143 | `}` |
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
|   2922 |  239 | `static sxi32 VmJsonEncodeString(json_private_data *pData,const char *zIn,int nByte,int bKey)` |
|      5 |  240 | `{` |
|   2927 |  241 | `	ph7_context *pCtx = pData->pCtx;` |
|   2927 |  242 | `	int iFlags = pData->iFlags;` |
|   2927 |  243 | `	const char *zEnd = &zIn[nByte];` |
|      - |  244 | `	sxi32 rc;` |
|      - |  245 | `	char c;` |
|   2922 |  246 | `	if( (iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR) != 0` |
|   1468 |  247 | `	 && (iFlags & (JSON_INVALID_UTF8_IGNORE\|JSON_INVALID_UTF8_SUBSTITUTE)) == 0` |
|     19 |  248 | `	 && VmJsonStrHasBadUtf8(zIn,nByte) ){` |
|      9 |  249 | `		pCtx->pVm->json_rc = JSON_ERROR_UTF8;` |
|      6 |  250 | `		return bKey ? ph7_result_string(pCtx,"\"\"",2)` |
|      8 |  251 | `		            : ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|      - |  252 | `	}` |
|   2919 |  253 | `	rc = ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|   2919 |  254 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  255 | `		return rc;` |
|      - |  256 | `	}` |
|   6474 |  257 | `	for(;;){` |
|  13061 |  258 | `		if( zIn >= zEnd ){` |
|      - |  259 | `			/* No more input to process */` |
|   2891 |  260 | `			break;` |
|      - |  261 | `		}` |
|  10175 |  262 | `		if( (unsigned char)zIn[0] >= 0x80 ){` |
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
|  10039 |  303 | `		c = zIn[0];` |
|      - |  304 | `		/* Advance the stream cursor */` |
|  10039 |  305 | `		zIn++;` |
|  10039 |  306 | `		if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|      - |  307 | `			/* All < and > are converted to \u003C and \u003E */` |
|      5 |  308 | `			if( c == '<' ){` |
|      3 |  309 | `				rc = ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|      2 |  310 | `			}else{` |
|      3 |  311 | `				rc = ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|      1 |  312 | `			}` |
|  10037 |  313 | `		}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|      - |  314 | `			/* All &s are converted to \u0026.  */` |
|      3 |  315 | `			rc = ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|  10034 |  316 | `		}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|      - |  317 | `			/* All ' are converted to \u0027.   */` |
|      3 |  318 | `			rc = ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|  10032 |  319 | `		}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|      - |  320 | `			/* All " are converted to \u0022. */` |
|      3 |  321 | `			rc = ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|  10030 |  322 | `		}else if( (unsigned char)c < 0x20 ){` |
|      - |  323 | `			/* Control characters (band A #4): php emits the short escapes for` |
|      - |  324 | `			 * \b \f \n \r \t and \u00xx for the rest — pre-fix these were` |
|      - |  325 | `			 * emitted RAW (invalid JSON). */` |
|      - |  326 | `			static const char zHex[] = "0123456789abcdef";` |
|    140 |  327 | `			char zEsc[6] = { '\\', 'u', '0', '0', 0, 0 };` |
|    140 |  328 | `			switch(c){` |
|    ! 0 |  329 | `			case '\b': rc = ph7_result_string(pCtx,"\\b",2); break;` |
|    ! 0 |  330 | `			case '\f': rc = ph7_result_string(pCtx,"\\f",2); break;` |
|     64 |  331 | `			case '\n': rc = ph7_result_string(pCtx,"\\n",2); break;` |
|     28 |  332 | `			case '\r': rc = ph7_result_string(pCtx,"\\r",2); break;` |
|     20 |  333 | `			case '\t': rc = ph7_result_string(pCtx,"\\t",2); break;` |
|     16 |  334 | `			default:` |
|     33 |  335 | `				zEsc[4] = zHex[(c >> 4) & 0x0F];` |
|     33 |  336 | `				zEsc[5] = zHex[c & 0x0F];` |
|     33 |  337 | `				rc = ph7_result_string(pCtx,zEsc,6);` |
|     32 |  338 | `				break;` |
|      - |  339 | `			}` |
|     71 |  340 | `		}else{` |
|   9891 |  341 | `			if( c == '"' \|\| c == '\\' ){` |
|      - |  342 | `				/* Escape the quote/backslash (php escapes the backslash` |
|      - |  343 | `				 * unconditionally — the old code wrongly tied it to` |
|      - |  344 | `				 * JSON_UNESCAPED_SLASHES, which governs '/' below) */` |
|     66 |  345 | `				rc = ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|   9859 |  346 | `			}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){` |
|      - |  347 | `				/* php escapes forward slashes by default */` |
|    104 |  348 | `				rc = ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|     53 |  349 | `			}else{` |
|   9725 |  350 | `				rc = SXRET_OK;` |
|      - |  351 | `			}` |
|   9891 |  352 | `			if( rc == SXRET_OK ){` |
|      - |  353 | `				/* Append character verbatim */` |
|   9891 |  354 | `				rc = ph7_result_string(pCtx,&c,(int)sizeof(char));` |
|   4943 |  355 | `			}` |
|      - |  356 | `		}` |
|  10039 |  357 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  358 | `			return rc;` |
|      - |  359 | `		}` |
|      5 |  360 | `	}` |
|   2891 |  361 | `	return ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|   1466 |  362 | `}` |
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
|    108 |  388 | `static int VmJsonPresent(ph7_class_instance *pThis,json_private_data *pData)` |
|      5 |  389 | `{` |
|    113 |  390 | `	ph7_context *pCtx = pData->pCtx;` |
|      - |  391 | `	ph7_value sPresent;` |
|      - |  392 | `	int savedObject;` |
|    113 |  393 | `	PH7_MemObjInit(pThis->pVm,&sPresent);` |
|    113 |  394 | `	if( PH7_MemObjToHashmap(&sPresent) != SXRET_OK ){` |
|    ! 0 |  395 | `		PH7_MemObjRelease(&sPresent);` |
|    ! 0 |  396 | `		return 0;` |
|      - |  397 | `	}` |
|    113 |  398 | `	if( !PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|     89 |  399 | `		PH7_MemObjRelease(&sPresent);` |
|     89 |  400 | `		return 0;` |
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
|     59 |  416 | `}` |
|   7772 |  417 | `static sxi32 VmJsonEncode(` |
|      - |  418 | `	ph7_value *pIn,          /* Encode this value */` |
|      - |  419 | `	json_private_data *pData /* Context data */` |
|      5 |  420 | `	){` |
|   7777 |  421 | `		ph7_context *pCtx = pData->pCtx;` |
|   7777 |  422 | `		int iFlags = pData->iFlags;` |
|      - |  423 | `		int nByte;` |
|   7777 |  424 | `		if( ph7_value_is_resource(pIn) ){` |
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
|   7769 |  438 | `		}else if( ph7_value_is_null(pIn) ){` |
|      - |  439 | `			/* null */` |
|     45 |  440 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|   7746 |  441 | `		}else if( ph7_value_is_bool(pIn) ){` |
|    103 |  442 | `			int iBool = ph7_value_to_bool(pIn);` |
|      - |  443 | `			int iLen;` |
|      - |  444 | `			/* true/false */` |
|    103 |  445 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|    103 |  446 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
|   7678 |  447 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|   1794 |  448 | `			if( ph7_value_is_float(pIn) ){` |
|     86 |  449 | `				double rVal = ph7_value_to_double(pIn);` |
|      - |  450 | `				/* php rejects Inf/NaN: json_encode returns FALSE with` |
|      - |  451 | `				 * json_last_error() == JSON_ERROR_INF_OR_NAN (they have no JSON` |
|      - |  452 | `				 * representation), instead of emitting the invalid bare token. */` |
|     86 |  453 | `				if( PH7_IS_NAN(rVal) \|\| PH7_IS_INF(rVal) ){` |
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
|     70 |  469 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,rVal,iFlags));` |
|     36 |  470 | `			}else{` |
|      - |  471 | `				const char *zNum;` |
|      - |  472 | `				/* Get a string representation of the number */` |
|   1119 |  473 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|   1119 |  474 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|      5 |  475 | `			}` |
|   7022 |  476 | `		}else if( ph7_value_is_string(pIn) ){` |
|   1733 |  477 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|      - |  478 | `				/* Encodes numeric strings as numbers (same float shapes). */` |
|      9 |  479 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|      9 |  480 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn),iFlags));` |
|      5 |  481 | `			}else{` |
|      - |  482 | `				const char *zIn;` |
|      - |  483 | `				/* Encode the string */` |
|   1725 |  484 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|   1725 |  485 | `				JSON_EMIT(pData,VmJsonEncodeString(pData,zIn,nByte,0));` |
|      5 |  486 | `			}` |
|   5567 |  487 | `		}else if( ph7_value_is_array(pIn) ){` |
|      - |  488 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|      - |  489 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|      - |  490 | `			 * object with stringified keys (PHP semantics). */` |
|   4543 |  491 | `			ph7_hashmap *pMap = (ph7_hashmap *)pIn->x.pOther;` |
|   9080 |  492 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|   4538 |  493 | `				\|\| !PH7_HashmapIsList(pMap);` |
|   4543 |  494 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|   4543 |  495 | `			int c = isObject ? '{' : '[';` |
|   4543 |  496 | `			int d = isObject ? '}' : ']';` |
|      - |  497 | `			/* An array the encoder is already inside of (reached through a` |
|      - |  498 | `			 * reference cycle) is php's JSON_ERROR_RECURSION — PHL used to` |
|      - |  499 | `			 * descend into it and answer a TRUNCATED nesting in silence.` |
|      - |  500 | `			 * JSON_PARTIAL_OUTPUT_ON_ERROR substitutes null for the cycle. */` |
|   4543 |  501 | `			if( VmJsonPathHolds(pData,(void *)pMap) ){` |
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
|   4539 |  517 | `			if( (ph7_int64)pData->nRecCount >= pData->nMaxDepth ){` |
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
|   4531 |  529 | `			if( SySetPut(&pData->aPath,(const void *)&pMap) != SXRET_OK ){` |
|    ! 0 |  530 | `				pData->oom = 1;` |
|    ! 0 |  531 | `				return PH7_OK;` |
|      - |  532 | `			}` |
|      - |  533 | `			/* Encode the array */` |
|   4531 |  534 | `			pData->isObject = isObject;` |
|   4531 |  535 | `			pData->isFirst = 1;` |
|      - |  536 | `			/* Append the square bracket or curly braces */` |
|   4531 |  537 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|      - |  538 | `			/* Iterate throw array entries */` |
|   4531 |  539 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|   4531 |  540 | `			(void)SySetPop(&pData->aPath);` |
|      - |  541 | `			/* Bail if a nested append ran out of memory before the closer */` |
|   4531 |  542 | `			if( pData->oom ){` |
|    ! 0 |  543 | `				return PH7_OK;` |
|      - |  544 | `			}` |
|      - |  545 | `			/* Pretty-print: a non-empty container closes on its own line,` |
|      - |  546 | `			 * indented one level less than its members (isFirst is still 1` |
|      - |  547 | `			 * only when no entry was emitted -> keep "[]"/"{}" tight). */` |
|   4531 |  548 | `			if( !pData->isFirst ){` |
|   4365 |  549 | `				JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|   2180 |  550 | `			}` |
|      - |  551 | `			/* Append the closing square bracket or curly braces */` |
|   4531 |  552 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|   4531 |  553 | `			pData->isObject = savedObject;` |
|   2428 |  554 | `		}else if( ph7_value_is_object(pIn) ){` |
|    165 |  555 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|    165 |  556 | `			ph7_vm *pVm = pIn->pVm;` |
|    165 |  557 | `			ph7_class_method *pMethod = 0;` |
|    165 |  558 | `			int bProps = 1; /* encode the property view below (cleared when a` |
|      - |  559 | `			                 * jsonSerialize() result replaces it) */` |
|      - |  560 | `			/* An object the encoder is already inside of is php's` |
|      - |  561 | `			 * JSON_ERROR_RECURSION, checked BEFORE the jsonSerialize dispatch` |
|      - |  562 | `			 * (PHL used to re-dispatch until the C stack ran out — a segfault` |
|      - |  563 | ``			 * on `return $this;`). */`` |
|    165 |  564 | `			if( VmJsonPathHolds(pData,(void *)pThis) ){` |
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
|    156 |  577 | `			if( pVm->pJsonSerializableClass` |
|    161 |  578 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|     35 |  579 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|     17 |  580 | `			}` |
|    161 |  581 | `			if( pMethod == 0 && (pThis->pClass->iFlags & PH7_CLASS_ENUM) != 0 ){` |
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
|    147 |  600 | `			if( pMethod ){` |
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
|    143 |  647 | `			if( bProps && (ph7_int64)pData->nRecCount >= pData->nMaxDepth ){` |
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
|    141 |  659 | `			if( bProps && SySetPut(&pData->aPath,(const void *)&pThis) != SXRET_OK ){` |
|    ! 0 |  660 | `				pData->oom = 1;` |
|    ! 0 |  661 | `				return PH7_OK;` |
|      - |  662 | `			}` |
|    141 |  663 | `			if( !bProps ){` |
|      - |  664 | `				/* jsonSerialize()'s result replaced the property view above */` |
|    127 |  665 | `			}else if( VmJsonPresent(pThis,pData) ){` |
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
|     89 |  688 | `				pData->isFirst = 1;` |
|      - |  689 | `				/* Append the curly braces */` |
|     89 |  690 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|     89 |  691 | `				SySetInit(&sNames,&pVm->sAllocator,sizeof(SyString));` |
|     89 |  692 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|    271 |  693 | `				while( (pAttrEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    186 |  694 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|    182 |  695 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN))` |
|    167 |  696 | `					 \|\| pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     53 |  697 | `						continue;` |
|      - |  698 | `					}` |
|    136 |  699 | `					if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|     18 |  700 | `						continue; /* typed, never written: not there yet (php) */` |
|      - |  701 | `					}` |
|    116 |  702 | `					if( SyStringLength(&pVmAttr->pAttr->sName) > 0` |
|    119 |  703 | `					 && SyStringData(&pVmAttr->pAttr->sName)[0] == 0 ){` |
|      - |  704 | `						/* A MANGLED key stored raw (the __PHP_Incomplete_Class` |
|      - |  705 | `						 * carrier): php's json encoder reads it as non-public` |
|      - |  706 | `						 * and skips it. */` |
|      5 |  707 | `						continue;` |
|      - |  708 | `					}` |
|    112 |  709 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     60 |  710 | `					 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|    ! 0 |  711 | `						continue; /* virtual set-only property: no value to encode (php) */` |
|      - |  712 | `					}` |
|    116 |  713 | `					SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|      4 |  714 | `				}` |
|     89 |  715 | `				aName = (SyString *)SySetBasePtr(&sNames);` |
|     89 |  716 | `				nName = SySetUsed(&sNames);` |
|    201 |  717 | `				for( iName = 0 ; iName < nName ; ++iName ){` |
|      - |  718 | `					VmClassAttr *pVmAttr;` |
|    116 |  719 | `					ph7_value *pAttrVal = 0;` |
|      - |  720 | `					ph7_value sHookVal;` |
|      - |  721 | `					sxi32 rcHk;` |
|    116 |  722 | `					pAttrEntry = PH7_ClassInstanceAttrEntry(pThis,aName[iName].zString,aName[iName].nByte);` |
|    116 |  723 | `					if( pAttrEntry == 0 ){` |
|    ! 0 |  724 | `						continue; /* unset by an earlier hook */` |
|      - |  725 | `					}` |
|    116 |  726 | `					pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|    116 |  727 | `					PH7_MemObjInit(pVm,&sHookVal);` |
|    116 |  728 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|    116 |  729 | `					if( rcHk == SXRET_OK ){` |
|     11 |  730 | `						pAttrVal = &sHookVal;` |
|    111 |  731 | `					}else if( rcHk == SXERR_NOTFOUND ){` |
|      - |  732 | `						/* Encode a COPY: the encoder casts scalars in place` |
|      - |  733 | `						 * (ph7_value_to_string), which must not corrupt the` |
|      - |  734 | `						 * live attribute slot. */` |
|    106 |  735 | `						ph7_value *pRaw = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    106 |  736 | `						if( pRaw ){` |
|    106 |  737 | `							PH7_MemObjStore(pRaw,&sHookVal);` |
|    106 |  738 | `							pAttrVal = &sHookVal;` |
|     51 |  739 | `						}` |
|     55 |  740 | `					}else{` |
|      - |  741 | `						/* the get hook threw — propagate like jsonSerialize() */` |
|    ! 0 |  742 | `						PH7_MemObjRelease(&sHookVal);` |
|    ! 0 |  743 | `						SySetRelease(&sNames);` |
|    ! 0 |  744 | `						pData->exc = 1;` |
|    ! 0 |  745 | `						return PH7_EXCEPTION;` |
|      - |  746 | `					}` |
|    116 |  747 | `					if( pAttrVal ){` |
|    116 |  748 | `						VmJsonObjectEncode(&pVmAttr->pAttr->sName,pAttrVal,pData);` |
|     56 |  749 | `					}` |
|    116 |  750 | `					PH7_MemObjRelease(&sHookVal);` |
|    116 |  751 | `					if( pData->exc ){` |
|    ! 0 |  752 | `						SySetRelease(&sNames);` |
|    ! 0 |  753 | `						return PH7_EXCEPTION; /* a nested jsonSerialize()/hook threw */` |
|      - |  754 | `					}` |
|    116 |  755 | `					if( pData->oom ){` |
|    ! 0 |  756 | `						SySetRelease(&sNames);` |
|    ! 0 |  757 | `						return PH7_OK;` |
|      - |  758 | `					}` |
|     60 |  759 | `				}` |
|     89 |  760 | `				SySetRelease(&sNames);` |
|      - |  761 | `				/* Pretty-print: non-empty object closes on its own indented line. */` |
|     89 |  762 | `				if( !pData->isFirst ){` |
|     72 |  763 | `					JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|     34 |  764 | `				}` |
|      - |  765 | `				/* Append the closing curly braces  */` |
|     89 |  766 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|      - |  767 | `			}` |
|    141 |  768 | `			if( bProps ){` |
|    113 |  769 | `				(void)SySetPop(&pData->aPath);` |
|     54 |  770 | `			}` |
|     73 |  771 | `		}else{` |
|      - |  772 | `			/* Can't happen */` |
|    ! 0 |  773 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|      - |  774 | `		}` |
|      - |  775 | `		/* All done */` |
|   7717 |  776 | `		return PH7_OK;` |
|   3891 |  777 | `}` |
|      - |  778 | `/*` |
|      - |  779 | ` * The following walker callback is invoked each time we need` |
|      - |  780 | ` * to encode an array to JSON.` |
|      - |  781 | ` */` |
|   5988 |  782 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 |  783 | `{` |
|   5993 |  784 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|   5993 |  785 | `	if( pJson->exc \|\| pJson->oom \|\| pJson->fail ){` |
|      - |  786 | `		/* A callback threw, OOM, or the value is unencodable (the result is` |
|      - |  787 | `		 * discarded) — return immediately. Depth is no longer decided here:` |
|      - |  788 | `		 * the container arms enforce json_encode's $depth where a '['/'{'` |
|      - |  789 | `		 * opens (the old flat 31 cap TRUNCATED a deep value in silence). */` |
|      3 |  790 | `		return PH7_OK;` |
|      - |  791 | `	}` |
|   5991 |  792 | `	if( !pJson->isFirst ){` |
|      - |  793 | `		/* Append the comma separating this entry from the previous one */` |
|   1617 |  794 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|    806 |  795 | `	}` |
|      - |  796 | `	/* Pretty-print: every member starts on its own indented line (one level` |
|      - |  797 | `	 * deeper than the enclosing container). */` |
|   5991 |  798 | `	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));` |
|   5991 |  799 | `	if( pJson->isObject ){` |
|      - |  800 | `		/* Outputs an object rather than an array */` |
|      - |  801 | `		const char *zKey;` |
|      - |  802 | `		int nByte;` |
|      - |  803 | `		/* Extract a string representation of the key */` |
|   1095 |  804 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|      - |  805 | `		/* Append the quoted key and the colon. The key goes through the same` |
|      - |  806 | `		 * escaper as a string VALUE (php escapes both identically): emitting it` |
|      - |  807 | `		 * raw produced invalid JSON for any key holding '"', '\' or a control` |
|      - |  808 | `		 * character. */` |
|   1095 |  809 | `		JSON_EMIT(pJson,VmJsonEncodeString(pJson,zKey,nByte,1));` |
|   1095 |  810 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,":",(int)sizeof(char)));` |
|      - |  811 | `		/* php puts a space after the colon in pretty mode */` |
|   1095 |  812 | `		if( pJson->iFlags & JSON_PRETTY_PRINT ){` |
|     61 |  813 | `			JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));` |
|     29 |  814 | `		}` |
|    545 |  815 | `	}` |
|      - |  816 | `	/* Encode the value */` |
|   5991 |  817 | `	pJson->nRecCount++;` |
|   5991 |  818 | `	VmJsonEncode(pValue,pJson);` |
|   5991 |  819 | `	pJson->nRecCount--;` |
|   5991 |  820 | `	pJson->isFirst = 0;` |
|   5991 |  821 | `	return PH7_OK;` |
|   2999 |  822 | `}` |
|      - |  823 | `/*` |
|      - |  824 | ` * The following walker callback is invoked each time we need to encode` |
|      - |  825 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|      - |  826 | ` */` |
|    112 |  827 | `static int VmJsonObjectEncode(const SyString *pAttr,ph7_value *pValue,void *pUserData)` |
|      4 |  828 | `{` |
|    116 |  829 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|    116 |  830 | `	if( pJson->exc \|\| pJson->oom \|\| pJson->fail ){` |
|      - |  831 | `		/* A callback threw, OOM, or the value is unencodable (the result is` |
|      - |  832 | `		 * discarded) — return immediately. Depth is no longer decided here:` |
|      - |  833 | `		 * the container arms enforce json_encode's $depth where a '['/'{'` |
|      - |  834 | `		 * opens (the old flat 31 cap TRUNCATED a deep value in silence). */` |
|    ! 0 |  835 | `		return PH7_OK;` |
|      - |  836 | `	}` |
|    116 |  837 | `	if( !pJson->isFirst ){` |
|      - |  838 | `		/* Append the comma separating this entry from the previous one */` |
|     47 |  839 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|     22 |  840 | `	}` |
|      - |  841 | `	/* Pretty-print: member on its own indented line, one level deeper. */` |
|    116 |  842 | `	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));` |
|      - |  843 | `	/* Append the quoted attribute name and the colon — escaped like a string` |
|      - |  844 | `	 * value, same as the array-key path above. */` |
|    116 |  845 | `	JSON_EMIT(pJson,VmJsonEncodeString(pJson,SyStringData(pAttr),(int)SyStringLength(pAttr),1));` |
|    116 |  846 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,":",(int)sizeof(char)));` |
|      - |  847 | `	/* php puts a space after the colon in pretty mode */` |
|    116 |  848 | `	if( pJson->iFlags & JSON_PRETTY_PRINT ){` |
|      9 |  849 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));` |
|      4 |  850 | `	}` |
|      - |  851 | `	/* Encode the value */` |
|    116 |  852 | `	pJson->nRecCount++;` |
|    116 |  853 | `	VmJsonEncode(pValue,pJson);` |
|    116 |  854 | `	pJson->nRecCount--;` |
|    116 |  855 | `	pJson->isFirst = 0;` |
|    116 |  856 | `	return PH7_OK;` |
|     60 |  857 | `}` |
|      - |  858 | `/*` |
|      - |  859 | ` * string json_encode(mixed $value [, int $flags = 0 [, int $depth = 512 ]])` |
|      - |  860 | ` *  Returns a string containing the JSON representation of value.` |
|      - |  861 | ` * Parameters` |
|      - |  862 | ` *  $value` |
|      - |  863 | ` *  The value being encoded. Can be any type except a resource` |
|      - |  864 | ` *  (a resource is JSON_ERROR_UNSUPPORTED_TYPE).` |
|      - |  865 | ` * $options` |
|      - |  866 | ` *  Bitmask consisting of:` |
|      - |  867 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|      - |  868 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|      - |  869 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|      - |  870 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|      - |  871 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|      - |  872 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|      - |  873 | ` *  JSON_BIGINT_AS_STRING   Decode flag (large ints as strings), not an encode flag.` |
|      - |  874 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|      - |  875 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|      - |  876 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|      - |  877 | ` * Return` |
|      - |  878 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|      - |  879 | ` */` |
|      - |  880 | `static const char * JsonErrorMsg(int rc); /* defined below, near json_last_error_msg */` |
|   1636 |  881 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  882 | `{` |
|      - |  883 | `	json_private_data sJson;` |
|      - |  884 | `	sxi32 rc;` |
|   1641 |  885 | `	if( nArg < 1 ){` |
|      - |  886 | `		/* Missing arguments,return FALSE */` |
|    ! 0 |  887 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  888 | `		return PH7_OK;` |
|      - |  889 | `	}` |
|      - |  890 | `	/* Prepare the JSON data */` |
|   1641 |  891 | `	sJson.nRecCount = 0;` |
|   1641 |  892 | `	sJson.pCtx = pCtx;` |
|   1641 |  893 | `	sJson.isFirst = 1;` |
|   1641 |  894 | `	sJson.iFlags = 0;` |
|   1641 |  895 | `	sJson.exc = 0;` |
|   1641 |  896 | `	sJson.oom = 0;` |
|   1641 |  897 | `	sJson.fail = 0;` |
|   1641 |  898 | `	sJson.failRc = JSON_ERROR_NON_BACKED_ENUM;` |
|   1641 |  899 | `	sJson.nMaxDepth = 512; /* php's default */` |
|   1641 |  900 | `	SySetInit(&sJson.aPath,&pCtx->pVm->sAllocator,sizeof(void *));` |
|   1641 |  901 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - |  902 | `		/* Extract option flags */` |
|    141 |  903 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|     69 |  904 | `	}` |
|   1641 |  905 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      - |  906 | `		/* $depth. Unlike json_decode's, php runs NO range screen here: 0 or a` |
|      - |  907 | `		 * negative value simply makes every container JSON_ERROR_DEPTH, and any` |
|      - |  908 | `		 * large int is accepted (the type screen has already run). */` |
|     27 |  909 | `		sJson.nMaxDepth = ph7_value_to_int64(apArg[2]);` |
|     27 |  910 | `		if( sJson.nMaxDepth > PH7_JSON_DEPTH_CEILING ){` |
|      - |  911 | `			/* Engine stack-safety bound (see PH7_JSON_DEPTH_CEILING). */` |
|    ! 0 |  912 | `			sJson.nMaxDepth = PH7_JSON_DEPTH_CEILING;` |
|    ! 0 |  913 | `		}` |
|     13 |  914 | `	}` |
|   1641 |  915 | `	pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|      - |  916 | `	/* Perform the encoding operation */` |
|   1641 |  917 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|   1641 |  918 | `	SySetRelease(&sJson.aPath);` |
|   1641 |  919 | `	if( sJson.oom ){` |
|      - |  920 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|      - |  921 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|    ! 0 |  922 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  923 | `	}` |
|   1641 |  924 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|      - |  925 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|      5 |  926 | `		return PH7_EXCEPTION;` |
|      - |  927 | `	}` |
|   1637 |  928 | `	if( sJson.fail ){` |
|      - |  929 | `		/* Unencodable value (Inf/NaN, or a php 8.1 non-backed enum case): the` |
|      - |  930 | `		 * whole encode fails — discard whatever was emitted and return FALSE. */` |
|     59 |  931 | `		pCtx->pVm->json_rc = sJson.failRc;` |
|     59 |  932 | `		if( sJson.iFlags & JSON_THROW_ON_ERROR ){` |
|      - |  933 | `			/* php: raise a JsonException carrying json_last_error_msg() instead` |
|      - |  934 | `			 * of returning FALSE. */` |
|     10 |  935 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|      6 |  936 | `				(sxi32)pCtx->pVm->json_rc,"%s",` |
|      6 |  937 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|      - |  938 | `		}` |
|     53 |  939 | `		ph7_result_bool(pCtx,0);` |
|     53 |  940 | `		return PH7_OK;` |
|      - |  941 | `	}` |
|      - |  942 | `	/* All done */` |
|   1579 |  943 | `	return PH7_OK;` |
|    823 |  944 | `}` |
|      - |  945 | `#undef JSON_EMIT` |
|      - |  946 | `/*` |
|      - |  947 | ` * int json_last_error(void)` |
|      - |  948 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|      - |  949 | ` * Parameters` |
|      - |  950 | ` *  None` |
|      - |  951 | ` * Return` |
|      - |  952 | ` *  Returns an integer, the value can be one of the following constants:` |
|      - |  953 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|      - |  954 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|      - |  955 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|      - |  956 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|      - |  957 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|      - |  958 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|      - |  959 | ` */` |
|    248 |  960 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  961 | `{` |
|    251 |  962 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  963 | `	/* Return the error code */` |
|    251 |  964 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    124 |  965 | `	SXUNUSED(nArg); /* cc warning */` |
|    124 |  966 | `	SXUNUSED(apArg);` |
|    251 |  967 | `	return PH7_OK;` |
|      3 |  968 | `}` |
|      - |  969 | `/*` |
|      - |  970 | ` * string json_last_error_msg(void)` |
|      - |  971 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|      - |  972 | ` * Parameters` |
|      - |  973 | ` *  None` |
|      - |  974 | ` * Return` |
|      - |  975 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|      - |  976 | ` *  code, or "No error" if no error has occurred.` |
|      - |  977 | ` */` |
|      - |  978 | `/* Human-readable message for a json_rc code. Shared by json_last_error_msg()` |
|      - |  979 | ` * and the JSON_THROW_ON_ERROR path (php's JsonException message is exactly this` |
|      - |  980 | ` * text). */` |
|     72 |  981 | `static const char * JsonErrorMsg(int rc)` |
|      2 |  982 | `{` |
|     74 |  983 | `	switch( rc ){` |
|     31 |  984 | `	case JSON_ERROR_NONE:            return "No error";` |
|    ! 0 |  985 | `	case JSON_ERROR_DEPTH:           return "Maximum stack depth exceeded";` |
|    ! 0 |  986 | `	case JSON_ERROR_STATE_MISMATCH:  return "State mismatch (invalid or malformed JSON)";` |
|      3 |  987 | `	case JSON_ERROR_CTRL_CHAR:       return "Control character error, possibly incorrectly encoded";` |
|     18 |  988 | `	case JSON_ERROR_SYNTAX:          return "Syntax error";` |
|      7 |  989 | `	case JSON_ERROR_UTF8:            return "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|      3 |  990 | `	case JSON_ERROR_RECURSION:       return "Recursion detected";` |
|      3 |  991 | `	case JSON_ERROR_INF_OR_NAN:     return "Inf and NaN cannot be JSON encoded";` |
|      5 |  992 | `	case JSON_ERROR_UNSUPPORTED_TYPE: return "Type is not supported";` |
|      3 |  993 | `	case JSON_ERROR_INVALID_PROPERTY_NAME: return "The decoded property name is invalid";` |
|      9 |  994 | `	case JSON_ERROR_UTF16:           return "Single unpaired UTF-16 surrogate in unicode escape";` |
|    ! 0 |  995 | `	case JSON_ERROR_NON_BACKED_ENUM: return "Non-backed enums have no default serialization";` |
|    ! 0 |  996 | `	default:                         return "Unknown error";` |
|      - |  997 | `	}` |
|     38 |  998 | `}` |
|     60 |  999 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1000 | `{` |
|     62 | 1001 | `	ph7_result_string(pCtx,JsonErrorMsg(pCtx->pVm->json_rc),-1/* auto length */);` |
|     30 | 1002 | `	SXUNUSED(nArg); /* cc warning */` |
|     30 | 1003 | `	SXUNUSED(apArg);` |
|     62 | 1004 | `	return PH7_OK;` |
|      2 | 1005 | `}` |
|      - | 1006 | `/* Possible tokens from the JSON tokenization process */` |
|      - | 1007 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|      - | 1008 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|      - | 1009 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|      - | 1010 | `#define JSON_TK_NULL    0x008 /* null */` |
|      - | 1011 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|      - | 1012 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|      - | 1013 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|      - | 1014 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|      - | 1015 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|      - | 1016 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|      - | 1017 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|      - | 1018 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|      - | 1019 | `/*` |
|      - | 1020 | ` * Tokenize an entire JSON input.` |
|      - | 1021 | ` * Get a single low-level token from the input file.` |
|      - | 1022 | ` * Update the stream pointer so that it points to the first` |
|      - | 1023 | ` * character beyond the extracted token.` |
|      - | 1024 | ` */` |
|   5462 | 1025 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|      4 | 1026 | `{` |
|   5466 | 1027 | `	int *pJsonErr = (int *)pUserData;` |
|      - | 1028 | `	SyString *pStr;` |
|      - | 1029 | `	int c;` |
|      - | 1030 | `	/* Ignore leading white spaces */` |
|   5528 | 1031 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|      - | 1032 | `		/* Advance the stream cursor */` |
|     64 | 1033 | `		if( pStream->zText[0] == '\n' ){` |
|      - | 1034 | `			/* Update line counter */` |
|      9 | 1035 | `			pStream->nLine++;` |
|      4 | 1036 | `		}` |
|     64 | 1037 | `		pStream->zText++;` |
|      2 | 1038 | `	}` |
|   5466 | 1039 | `	if( pStream->zText >= pStream->zEnd ){` |
|      - | 1040 | `		/* End of input reached */` |
|     12 | 1041 | `		SXUNUSED(pCtxData); /* cc warning */` |
|     26 | 1042 | `		return SXERR_EOF;` |
|      - | 1043 | `	}` |
|      - | 1044 | `	/* Record token starting position and line */` |
|   5442 | 1045 | `	pToken->nLine = pStream->nLine;` |
|   5442 | 1046 | `	pToken->pUserData = 0;` |
|   5442 | 1047 | `	pStr = &pToken->sData;` |
|   5442 | 1048 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|   5438 | 1049 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|   1699 | 1050 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|      - | 1051 | `			/* Single character */` |
|   5074 | 1052 | `			c = pStream->zText[0];` |
|      - | 1053 | `			/* Set token type */` |
|   5074 | 1054 | `			switch(c){` |
|   2418 | 1055 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|     80 | 1056 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|     63 | 1057 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|   2390 | 1058 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|     86 | 1059 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|     56 | 1060 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|    ! 0 | 1061 | `			default:` |
|    ! 0 | 1062 | `				break;` |
|      - | 1063 | `			}` |
|      - | 1064 | `			/* Advance the stream cursor */` |
|   5074 | 1065 | `			pStream->zText++;` |
|   2907 | 1066 | `	}else if( pStream->zText[0] == '"') {` |
|      - | 1067 | `		/* JSON string */` |
|    160 | 1068 | `		pStream->zText++;` |
|    160 | 1069 | `		pStr->zString++;` |
|      - | 1070 | `		/* Delimit the string. The backslash state is tracked explicitly: the old` |
|      - | 1071 | `		 * "the previous byte is not a backslash" test mis-read an ESCAPED` |
|      - | 1072 | `		 * backslash sitting before the closing quote, so the perfectly valid` |
|      - | 1073 | `		 * "\\" (a one-character string holding a backslash) was reported as an` |
|      - | 1074 | `		 * unterminated string — json_decode('"\\\\"') answered NULL with a` |
|      - | 1075 | `		 * syntax error where php answers "\". */` |
|    680 | 1076 | `		while( pStream->zText < pStream->zEnd ){` |
|    678 | 1077 | `			if( pStream->zText[0] == '\\' ){` |
|      - | 1078 | `				/* Whatever follows belongs to the escape, closing quote` |
|      - | 1079 | `				 * included; VmJsonDequoteString below decides if it is legal. */` |
|     91 | 1080 | `				pStream->zText++;` |
|     91 | 1081 | `				if( pStream->zText >= pStream->zEnd ){` |
|    ! 0 | 1082 | `					break;` |
|      - | 1083 | `				}` |
|     91 | 1084 | `				pStream->zText++;` |
|     91 | 1085 | `				continue;` |
|      - | 1086 | `			}` |
|    588 | 1087 | `			if( pStream->zText[0] == '"' ){` |
|    158 | 1088 | `				break;` |
|      - | 1089 | `			}` |
|    434 | 1090 | `			if( (unsigned char)pStream->zText[0] < 0x20 ){` |
|      - | 1091 | `				/* php: a control character must be escaped inside a JSON string;` |
|      - | 1092 | `				 * a raw one is JSON_ERROR_CTRL_CHAR (a literal newline included). */` |
|    ! 0 | 1093 | `				pToken->nType = JSON_TK_INVALID;` |
|    ! 0 | 1094 | `				*pJsonErr = JSON_ERROR_CTRL_CHAR;` |
|    ! 0 | 1095 | `				return SXERR_ABORT;` |
|      - | 1096 | `			}` |
|    434 | 1097 | `			pStream->zText++;` |
|      4 | 1098 | `		}` |
|    160 | 1099 | `		if( pStream->zText >= pStream->zEnd ){` |
|      - | 1100 | `			/* Missing closing '"'. php reports this as JSON_ERROR_CTRL_CHAR, not` |
|      - | 1101 | `			 * a syntax error: its scanner runs the string off the end of the` |
|      - | 1102 | `			 * input and lands in the same state an unescaped control character` |
|      - | 1103 | `			 * puts it in. */` |
|      3 | 1104 | `			pToken->nType = JSON_TK_INVALID;` |
|      3 | 1105 | `			*pJsonErr = JSON_ERROR_CTRL_CHAR;` |
|      2 | 1106 | `		}else{` |
|    158 | 1107 | `			pToken->nType = JSON_TK_STR;` |
|    158 | 1108 | `			pStream->zText++; /* Jump the closing double quotes */` |
|      - | 1109 | `		}` |
|    294 | 1110 | `	}else if( (pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]))` |
|    149 | 1111 | `		\|\| (pStream->zText[0] == '-' && &pStream->zText[1] < pStream->zEnd` |
|     22 | 1112 | `			&& pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1])) ){` |
|      - | 1113 | `		/* Number, held to JSON's grammar:` |
|      - | 1114 | `		 *   -?(0\|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?` |
|      - | 1115 | `		 * The old scanner took any digit soup, so "01", "-01", "5." and "1e"` |
|      - | 1116 | `		 * all DECODED (and json_validate() answered TRUE) where php reports` |
|      - | 1117 | `		 * JSON_ERROR_SYNTAX — accepting documents no JSON producer emits. */` |
|    182 | 1118 | `		int bBad = 0;` |
|    182 | 1119 | `		if( pStream->zText[0] == '-' ){` |
|     25 | 1120 | `			pStream->zText++;` |
|     11 | 1121 | `		}` |
|    182 | 1122 | `		if( pStream->zText[0] == '0' ){` |
|     21 | 1123 | `			pStream->zText++;` |
|      - | 1124 | `			/* JSON forbids a leading zero ahead of another digit */` |
|     21 | 1125 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     11 | 1126 | `				bBad = 1;` |
|      5 | 1127 | `			}` |
|     10 | 1128 | `		}` |
|    540 | 1129 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    362 | 1130 | `			pStream->zText++;` |
|      4 | 1131 | `		}` |
|    182 | 1132 | `		if( pStream->zText < pStream->zEnd && pStream->zText[0] == '.' ){` |
|     20 | 1133 | `			pStream->zText++;` |
|      - | 1134 | `			/* JSON requires at least one digit after the point */` |
|     20 | 1135 | `			if( pStream->zText >= pStream->zEnd \|\| pStream->zText[0] >= 0xc0 \|\| !SyisDigit(pStream->zText[0]) ){` |
|      5 | 1136 | `				bBad = 1;` |
|      2 | 1137 | `			}` |
|     36 | 1138 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     18 | 1139 | `				pStream->zText++;` |
|      2 | 1140 | `			}` |
|      9 | 1141 | `		}` |
|    182 | 1142 | `		if( pStream->zText < pStream->zEnd && (pStream->zText[0] == 'e' \|\| pStream->zText[0] == 'E') ){` |
|     21 | 1143 | `			pStream->zText++;` |
|     21 | 1144 | `			if( pStream->zText < pStream->zEnd && (pStream->zText[0] == '+' \|\| pStream->zText[0] == '-') ){` |
|      9 | 1145 | `				pStream->zText++;` |
|      4 | 1146 | `			}` |
|      - | 1147 | `			/* ...and at least one digit in the exponent */` |
|     21 | 1148 | `			if( pStream->zText >= pStream->zEnd \|\| pStream->zText[0] >= 0xc0 \|\| !SyisDigit(pStream->zText[0]) ){` |
|      7 | 1149 | `				bBad = 1;` |
|      3 | 1150 | `			}` |
|     35 | 1151 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     15 | 1152 | `				pStream->zText++;` |
|      1 | 1153 | `			}` |
|     10 | 1154 | `		}` |
|    182 | 1155 | `		if( bBad ){` |
|     21 | 1156 | `			pToken->nType = JSON_TK_INVALID;` |
|     21 | 1157 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|     21 | 1158 | `			return SXERR_ABORT;` |
|      - | 1159 | `		}` |
|    162 | 1160 | `		pToken->nType = JSON_TK_NUM;` |
|    124 | 1161 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|     18 | 1162 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|      - | 1163 | `			/* boolean true */` |
|      3 | 1164 | `			pToken->nType = JSON_TK_TRUE;` |
|      - | 1165 | `			/* Advance the stream cursor */` |
|      3 | 1166 | `			pStream->zText += sizeof("true")-1;` |
|     43 | 1167 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|     16 | 1168 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|      - | 1169 | `			/* boolean false */` |
|    ! 0 | 1170 | `			pToken->nType = JSON_TK_FALSE;` |
|      - | 1171 | `			/* Advance the stream cursor */` |
|    ! 0 | 1172 | `			pStream->zText += sizeof("false")-1;` |
|     42 | 1173 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|     16 | 1174 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|      - | 1175 | `			/* NULL */` |
|      3 | 1176 | `			pToken->nType = JSON_TK_NULL;` |
|      - | 1177 | `			/* Advance the stream cursor */` |
|      3 | 1178 | `			pStream->zText += sizeof("null")-1;` |
|      2 | 1179 | `	}else{` |
|      - | 1180 | `		/* Unexpected token — but a byte that is not valid UTF-8 is php's` |
|      - | 1181 | `		 * JSON_ERROR_UTF8, not a syntax error, wherever in the document it sits` |
|      - | 1182 | `		 * (a valid non-ASCII character outside a string stays a syntax error).` |
|      - | 1183 | `		 * The JSON_INVALID_UTF8_* flags do NOT reach here: php applies them` |
|      - | 1184 | `		 * inside string tokens only. */` |
|      - | 1185 | `		sxu32 nLen;` |
|     32 | 1186 | `		pToken->nType = JSON_TK_INVALID;` |
|     53 | 1187 | `		*pJsonErr = ((unsigned char)pStream->zText[0] >= 0x80` |
|     21 | 1188 | `			&& PH7_Utf8ReadStrict((const unsigned char *)pStream->zText,` |
|     18 | 1189 | `				(sxu32)(pStream->zEnd - pStream->zText),&nLen) < 0)` |
|     21 | 1190 | `			? JSON_ERROR_UTF8 : JSON_ERROR_SYNTAX;` |
|      - | 1191 | `		/* Advance the stream cursor */` |
|     32 | 1192 | `		pStream->zText++;` |
|      - | 1193 | `		/* Abort processing immediatley */` |
|     32 | 1194 | `		return SXERR_ABORT;` |
|      - | 1195 | `	}` |
|      - | 1196 | `	/* record token length */` |
|   5392 | 1197 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   5392 | 1198 | `	if( pToken->nType == JSON_TK_STR ){` |
|    158 | 1199 | `		pStr->nByte--;` |
|     77 | 1200 | `	}` |
|      - | 1201 | `	/* Return to the lexer */` |
|   5392 | 1202 | `	return SXRET_OK;` |
|   2735 | 1203 | `}` |
|      - | 1204 | `/*` |
|      - | 1205 | ` * JSON decoded input consumer callback signature.` |
|      - | 1206 | ` */` |
|      - | 1207 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|      - | 1208 | `/*` |
|      - | 1209 | ` * JSON decoder state is kept in the following structure.` |
|      - | 1210 | ` */` |
|      - | 1211 | `typedef struct json_decoder json_decoder;` |
|      - | 1212 | `struct json_decoder` |
|      - | 1213 | `{` |
|      - | 1214 | `	ph7_context *pCtx; /* Call context */` |
|      - | 1215 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|      - | 1216 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|      - | 1217 | `	int iFlags;        /* Configuration flags */` |
|      - | 1218 | `	int iUserFlags;    /* json_decode()'s own $flags (JSON_INVALID_UTF8_* live here) */` |
|      - | 1219 | `	SyToken *pIn;      /* Token stream */` |
|      - | 1220 | `	SyToken *pEnd;     /* End of the token stream */` |
|      - | 1221 | `	int rec_depth;     /* Recursion limit */` |
|      - | 1222 | `	int rec_count;     /* Current nesting level */` |
|      - | 1223 | `	int *pErr;         /* JSON decoding error if any */` |
|      - | 1224 | `};` |
|      - | 1225 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|      - | 1226 | `/* Forward declaration */` |
|      - | 1227 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|      - | 1228 | `/*` |
|      - | 1229 | ` * Read the four hex digits of a \uXXXX escape out of z[0..n-1]. Returns` |
|      - | 1230 | ` * SXRET_OK and the value, or SXERR_SYNTAX when fewer than four are there or one` |
|      - | 1231 | ` * is not a hex digit (php: JSON_ERROR_SYNTAX).` |
|      - | 1232 | ` */` |
|     62 | 1233 | `static sxi32 VmJsonHex4(const char *z,sxu32 n,sxu32 *pVal)` |
|      1 | 1234 | `{` |
|     63 | 1235 | `	sxu32 v = 0;` |
|      - | 1236 | `	int i;` |
|     63 | 1237 | `	if( n < 4 ){` |
|      3 | 1238 | `		return SXERR_SYNTAX;` |
|      - | 1239 | `	}` |
|    293 | 1240 | `	for( i = 0 ; i < 4 ; ++i ){` |
|    235 | 1241 | `		int c = (unsigned char)z[i];` |
|    235 | 1242 | `		if( c >= '0' && c <= '9' ){` |
|    147 | 1243 | `			v = (v << 4) \| (sxu32)(c - '0');` |
|    162 | 1244 | `		}else if( c >= 'a' && c <= 'f' ){` |
|     79 | 1245 | `			v = (v << 4) \| (sxu32)(c - 'a' + 10);` |
|     50 | 1246 | `		}else if( c >= 'A' && c <= 'F' ){` |
|      9 | 1247 | `			v = (v << 4) \| (sxu32)(c - 'A' + 10);` |
|      5 | 1248 | `		}else{` |
|      3 | 1249 | `			return SXERR_SYNTAX;` |
|      - | 1250 | `		}` |
|    117 | 1251 | `	}` |
|     59 | 1252 | `	*pVal = v;` |
|     59 | 1253 | `	return SXRET_OK;` |
|     32 | 1254 | `}` |
|      - | 1255 | `/*` |
|      - | 1256 | ` * Append one run of un-escaped string bytes, checking that it really is UTF-8:` |
|      - | 1257 | ` * php rejects a JSON document carrying a byte that is not text with` |
|      - | 1258 | ` * JSON_ERROR_UTF8, exactly as it refuses to ENCODE one. Only inside a string do` |
|      - | 1259 | ` * the JSON_INVALID_UTF8_* flags apply — a stray byte between tokens is an error` |
|      - | 1260 | ` * either way. Returns JSON_ERROR_NONE or JSON_ERROR_UTF8.` |
|      - | 1261 | ` */` |
|    132 | 1262 | `static int VmJsonAppendChecked(ph7_value *pWorker,const char *zIn,sxu32 nByte,int iFlags)` |
|      4 | 1263 | `{` |
|    136 | 1264 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|    136 | 1265 | `	sxu32 i = 0,iRun = 0,nLen;` |
|    298 | 1266 | `	while( i < nByte ){` |
|    178 | 1267 | `		if( z[i] < 0x80 ){` |
|    154 | 1268 | `			i++;` |
|    154 | 1269 | `			continue;` |
|      - | 1270 | `		}` |
|     25 | 1271 | `		if( PH7_Utf8ReadStrict(&z[i],nByte - i,&nLen) >= 0 ){` |
|      3 | 1272 | `			i += nLen;` |
|      3 | 1273 | `			continue;` |
|      - | 1274 | `		}` |
|     23 | 1275 | `		if( (iFlags & (JSON_INVALID_UTF8_IGNORE\|JSON_INVALID_UTF8_SUBSTITUTE)) == 0 ){` |
|     13 | 1276 | `			return JSON_ERROR_UTF8;` |
|      - | 1277 | `		}` |
|      - | 1278 | `		/* With BOTH flags set php's decoder substitutes while its encoder drops` |
|      - | 1279 | `		 * (probed both ways); the order of these two tests is that asymmetry,` |
|      - | 1280 | `		 * not an oversight. */` |
|      - | 1281 | `		/* Flush what is good, then stand in for the run */` |
|     11 | 1282 | `		if( i > iRun ){` |
|     11 | 1283 | `			ph7_value_string(pWorker,&zIn[iRun],(int)(i - iRun));` |
|      5 | 1284 | `		}` |
|     11 | 1285 | `		nLen = VmJsonBadUtf8Len(&z[i],nByte - i);` |
|     11 | 1286 | `		if( iFlags & JSON_INVALID_UTF8_SUBSTITUTE ){` |
|      7 | 1287 | `			ph7_value_string(pWorker,"\357\277\275",3); /* U+FFFD */` |
|      3 | 1288 | `		}` |
|     11 | 1289 | `		i += nLen;` |
|     11 | 1290 | `		iRun = i;` |
|      1 | 1291 | `	}` |
|    124 | 1292 | `	if( i > iRun ){` |
|    124 | 1293 | `		ph7_value_string(pWorker,&zIn[iRun],(int)(i - iRun));` |
|     60 | 1294 | `	}` |
|    124 | 1295 | `	return JSON_ERROR_NONE;` |
|     70 | 1296 | `}` |
|      - | 1297 | `/*` |
|      - | 1298 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|      - | 1299 | ` * the result in the given ph7_value. Returns JSON_ERROR_NONE, or the json_rc` |
|      - | 1300 | ` * php reports for the malformed escape it stopped on.` |
|      - | 1301 | ` *` |
|      - | 1302 | ` * The \uXXXX form used to fall through to the default branch, which dropped the` |
|      - | 1303 | ` * backslash and kept the rest as literal text: json_decode('"é"') answered` |
|      - | 1304 | ` * the five characters u00e9 instead of "é". \b was mangled the same way (it` |
|      - | 1305 | ` * answered "b"), and an escape JSON does not define (\q) was silently accepted` |
|      - | 1306 | ` * where php raises a syntax error.` |
|      - | 1307 | ` */` |
|    154 | 1308 | `static int VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker,int iFlags)` |
|      4 | 1309 | `{` |
|    158 | 1310 | `	const char *zIn = pStr->zString;` |
|    158 | 1311 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|      - | 1312 | `	const char *zCur;` |
|      - | 1313 | `	int c;` |
|      - | 1314 | `	/* Mark the value as a string */` |
|    158 | 1315 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|    110 | 1316 | `	for(;;){` |
|    224 | 1317 | `		zCur = zIn;` |
|    404 | 1318 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|    184 | 1319 | `			zIn++;` |
|      4 | 1320 | `		}` |
|    224 | 1321 | `		if( zIn > zCur ){` |
|    136 | 1322 | `			int rcChunk = VmJsonAppendChecked(pWorker,zCur,(sxu32)(zIn-zCur),iFlags);` |
|    136 | 1323 | `			if( rcChunk != JSON_ERROR_NONE ){` |
|     13 | 1324 | `				return rcChunk;` |
|      - | 1325 | `			}` |
|     60 | 1326 | `		}` |
|    212 | 1327 | `		zIn++;` |
|    212 | 1328 | `		if( zIn >= zEnd ){` |
|      - | 1329 | `			/* End of the input reached */` |
|    132 | 1330 | `			break;` |
|      - | 1331 | `		}` |
|     81 | 1332 | `		c = zIn[0];` |
|      - | 1333 | `		/* Unescape the character */` |
|     81 | 1334 | `		switch(c){` |
|      7 | 1335 | `		case '"':  ph7_value_string(pWorker,"\"",(int)sizeof(char)); break;` |
|      7 | 1336 | `		case '\\': ph7_value_string(pWorker,"\\",(int)sizeof(char)); break;` |
|      3 | 1337 | `		case '/':  ph7_value_string(pWorker,"/",(int)sizeof(char)); break;` |
|      3 | 1338 | `		case 'b':  ph7_value_string(pWorker,"\b",(int)sizeof(char)); break;` |
|      3 | 1339 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|      5 | 1340 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|      3 | 1341 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|      3 | 1342 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|     26 | 1343 | `		case 'u': {` |
|      - | 1344 | `			/* \uXXXX, and the surrogate PAIR that is JSON's only way to spell a` |
|      - | 1345 | `			 * code point above the BMP. An unpaired half is php's` |
|      - | 1346 | `			 * JSON_ERROR_UTF16, distinct from a malformed escape. */` |
|      - | 1347 | `			unsigned char zUtf8[4];` |
|     53 | 1348 | `			unsigned char *zW = zUtf8;` |
|     53 | 1349 | `			sxu32 cp,cpLow = 0; /* cpLow pre-set: MSVC /W4 flags the short-circuit as a maybe-uninitialized read */` |
|     53 | 1350 | `			if( VmJsonHex4(&zIn[1],(sxu32)(zEnd - zIn - 1),&cp) != SXRET_OK ){` |
|      9 | 1351 | `				return JSON_ERROR_SYNTAX;` |
|      - | 1352 | `			}` |
|     49 | 1353 | `			zIn += 4;` |
|     49 | 1354 | `			if( cp >= 0xDC00 && cp <= 0xDFFF ){` |
|      3 | 1355 | `				return JSON_ERROR_UTF16; /* a low half with no high half before it */` |
|      - | 1356 | `			}` |
|     47 | 1357 | `			if( cp >= 0xD800 && cp <= 0xDBFF ){` |
|     14 | 1358 | `				if( zEnd - zIn < 3 \|\| zIn[1] != '\\' \|\| zIn[2] != 'u'` |
|     10 | 1359 | `				 \|\| VmJsonHex4(&zIn[3],(sxu32)(zEnd - zIn - 3),&cpLow) != SXRET_OK` |
|     11 | 1360 | `				 \|\| cpLow < 0xDC00 \|\| cpLow > 0xDFFF ){` |
|      7 | 1361 | `					return JSON_ERROR_UTF16;` |
|      - | 1362 | `				}` |
|      9 | 1363 | `				cp = 0x10000 + ((cp - 0xD800) << 10) + (cpLow - 0xDC00);` |
|      9 | 1364 | `				zIn += 6;` |
|      4 | 1365 | `			}` |
|     41 | 1366 | `			SX_WRITE_UTF8(zW,cp);` |
|     41 | 1367 | `			ph7_value_string(pWorker,(const char *)zUtf8,(int)(zW - zUtf8));` |
|     41 | 1368 | `			break;` |
|      - | 1369 | `		}` |
|      1 | 1370 | `		default:` |
|      - | 1371 | `			/* Not one of JSON's nine escapes */` |
|      3 | 1372 | `			return JSON_ERROR_SYNTAX;` |
|      - | 1373 | `		}` |
|      - | 1374 | `		/* Advance the stream cursor */` |
|     67 | 1375 | `		zIn++;` |
|      1 | 1376 | `	}` |
|    132 | 1377 | `	return JSON_ERROR_NONE;` |
|     81 | 1378 | `}` |
|      - | 1379 | `/*` |
|      - | 1380 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|      - | 1381 | ` * According to wikipedia` |
|      - | 1382 | ` * JSON's basic types are:` |
|      - | 1383 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|      - | 1384 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|      - | 1385 | ` *   Boolean (true or false)` |
|      - | 1386 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|      - | 1387 | ` *    do not need to be of the same type)` |
|      - | 1388 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|      - | 1389 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|      - | 1390 | ` *     be distinct from each other)` |
|      - | 1391 | ` *   null (empty)` |
|      - | 1392 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|      - | 1393 | ` */` |
|   2702 | 1394 | `static sxi32 VmJsonDecode(` |
|      - | 1395 | `	json_decoder *pDecoder, /* JSON decoder */` |
|      - | 1396 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|      4 | 1397 | `	){` |
|      - | 1398 | `	ph7_value *pWorker; /* Worker variable */` |
|      - | 1399 | `	sxi32 rc;` |
|      - | 1400 | `	int rcQ;            /* VmJsonDequoteString() status */` |
|      - | 1401 | `	/* Nothing left to decode: the token stream is empty (a whitespace-only input` |
|      - | 1402 | `	 * tokenizes to NO tokens at all, so pIn/pEnd are both the NULL base pointer of an` |
|      - | 1403 | `	 * empty set) or a member value is missing after its colon ('{"a":'). Both are a` |
|      - | 1404 | `	 * syntax error for php; without this screen the reads below dereference pEnd. */` |
|   2706 | 1405 | `	if( pDecoder->pIn >= pDecoder->pEnd ){` |
|     26 | 1406 | `		*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|     26 | 1407 | `		return SXERR_ABORT;` |
|      - | 1408 | `	}` |
|   2682 | 1409 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|      - | 1410 | `		/* Scalar value */` |
|    226 | 1411 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|    226 | 1412 | `		if( pWorker == 0 ){` |
|    ! 0 | 1413 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 1414 | `			/* Abort the decoding operation immediately */` |
|    ! 0 | 1415 | `			return SXERR_ABORT;` |
|      - | 1416 | `		}` |
|      - | 1417 | `		/* Reflect the JSON image */` |
|    226 | 1418 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|      - | 1419 | `			/* Nullify the value.*/` |
|      3 | 1420 | `			ph7_value_null(pWorker);` |
|    225 | 1421 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|      - | 1422 | `			/* Boolean value */` |
|      3 | 1423 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|    223 | 1424 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|    148 | 1425 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|      - | 1426 | `			/*` |
|      - | 1427 | `			 * Numeric value.` |
|      - | 1428 | `			 * Get a string representation first then try to get a numeric` |
|      - | 1429 | `			 * value.` |
|      - | 1430 | `			 */` |
|    148 | 1431 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|      - | 1432 | `			/* Obtain a numeric representation */` |
|    148 | 1433 | `			PH7_MemObjToNumeric(pWorker);` |
|    144 | 1434 | `			if( (pDecoder->iUserFlags & JSON_BIGINT_AS_STRING) != 0` |
|     82 | 1435 | `			 && ph7_value_is_float(pWorker) ){` |
|      - | 1436 | `				/* php: an INTEGER literal beyond int64 normally lands on a` |
|      - | 1437 | `				 * float; under JSON_BIGINT_AS_STRING it stays the EXACT source` |
|      - | 1438 | `				 * text as a string. Only integer SHAPES qualify — a '.', 'e'` |
|      - | 1439 | `				 * or 'E' anywhere means the document asked for the float. */` |
|      - | 1440 | `				sxu32 iCh;` |
|      9 | 1441 | `				int bIntShape = 1;` |
|     93 | 1442 | `				for( iCh = 0 ; iCh < pStr->nByte ; ++iCh ){` |
|     88 | 1443 | `					if( pStr->zString[iCh] == '.' \|\| pStr->zString[iCh] == 'e'` |
|     86 | 1444 | `					 \|\| pStr->zString[iCh] == 'E' ){` |
|      5 | 1445 | `						bIntShape = 0;` |
|      5 | 1446 | `						break;` |
|      - | 1447 | `					}` |
|     43 | 1448 | `				}` |
|      9 | 1449 | `				if( bIntShape ){` |
|      5 | 1450 | `					ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|      2 | 1451 | `				}` |
|      4 | 1452 | `			}` |
|     76 | 1453 | `		}else{` |
|      - | 1454 | `			/* Dequote the string */` |
|     76 | 1455 | `			rcQ = VmJsonDequoteString(&pDecoder->pIn->sData,pWorker,pDecoder->iUserFlags);` |
|     76 | 1456 | `			if( rcQ != JSON_ERROR_NONE ){` |
|     25 | 1457 | `				*pDecoder->pErr = rcQ;` |
|     25 | 1458 | `				return SXERR_ABORT;` |
|      - | 1459 | `			}` |
|      - | 1460 | `		}` |
|      - | 1461 | `		/* Invoke the consumer callback */` |
|    202 | 1462 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|    202 | 1463 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 | 1464 | `			return SXERR_ABORT;` |
|      - | 1465 | `		}` |
|      - | 1466 | `		/* All done,advance the stream cursor */` |
|    202 | 1467 | `		pDecoder->pIn++;` |
|   2559 | 1468 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|      - | 1469 | `		ProcJsonConsumer xOld;` |
|      - | 1470 | `		void *pOld;` |
|      - | 1471 | `		/* php's $depth counts CONTAINERS: a '[' opening at 1-based nesting` |
|      - | 1472 | `		 * level L is JSON_ERROR_DEPTH when L >= $depth, an EMPTY container` |
|      - | 1473 | `		 * included ("[]" at $depth 1 already fails), while a scalar never` |
|      - | 1474 | `		 * consults $depth at all. rec_count holds L-1 here. */` |
|   2392 | 1475 | `		if( pDecoder->rec_count + 1 >= pDecoder->rec_depth ){` |
|     11 | 1476 | `			*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|     11 | 1477 | `			return SXERR_ABORT;` |
|      - | 1478 | `		}` |
|      - | 1479 | `		/* Array representation*/` |
|   2382 | 1480 | `		pDecoder->pIn++;` |
|      - | 1481 | `		/* Create a working array */` |
|   2382 | 1482 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   2382 | 1483 | `		if( pWorker == 0 ){` |
|    ! 0 | 1484 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 1485 | `			/* Abort the decoding operation immediately */` |
|    ! 0 | 1486 | `			return SXERR_ABORT;` |
|      - | 1487 | `		}` |
|      - | 1488 | `		/* Save the old consumer */` |
|   2382 | 1489 | `		xOld = pDecoder->xConsumer;` |
|   2382 | 1490 | `		pOld = pDecoder->pUserData;` |
|      - | 1491 | `		/* Set the new consumer */` |
|   2382 | 1492 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   2382 | 1493 | `		pDecoder->pUserData = pWorker;` |
|      - | 1494 | `		/* Decode the array */` |
|   1877 | 1495 | `		for(;;){` |
|      - | 1496 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|      - | 1497 | `			 * do this.` |
|      - | 1498 | `			 */` |
|   3792 | 1499 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|     38 | 1500 | `				pDecoder->pIn++;` |
|      4 | 1501 | `			}` |
|   3758 | 1502 | `			if( pDecoder->pIn >= pDecoder->pEnd ){` |
|      - | 1503 | `				/* Ran out of tokens before the closing ']': php rejects an` |
|      - | 1504 | `				 * unterminated array as a syntax error. */` |
|      8 | 1505 | `				*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      8 | 1506 | `				return SXERR_ABORT;` |
|      - | 1507 | `			}` |
|   3752 | 1508 | `			if( pDecoder->pIn->nType & JSON_TK_CSB /*']'*/ ){` |
|   1350 | 1509 | `				pDecoder->pIn++; /* Jump the trailing ']' */` |
|   1350 | 1510 | `				break;` |
|      - | 1511 | `			}` |
|      - | 1512 | `			/* Recurse and decode the entry */` |
|   2406 | 1513 | `			pDecoder->rec_count++;` |
|   2406 | 1514 | `			rc = VmJsonDecode(pDecoder,0);` |
|   2406 | 1515 | `			pDecoder->rec_count--;` |
|   2406 | 1516 | `			if( rc == SXERR_ABORT ){` |
|      - | 1517 | `				/* Abort processing immediately */` |
|   1027 | 1518 | `				return SXERR_ABORT;` |
|      - | 1519 | `			}` |
|      - | 1520 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   1380 | 1521 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   1374 | 1522 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|      - | 1523 | `					/* Unexpected token,abort immediatley */` |
|    ! 0 | 1524 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1525 | `					return SXERR_ABORT;` |
|      - | 1526 | `			}` |
|      4 | 1527 | `		}` |
|      - | 1528 | `		/* Restore the old consumer */` |
|   1350 | 1529 | `		pDecoder->xConsumer = xOld;` |
|   1350 | 1530 | `		pDecoder->pUserData = pOld;` |
|      - | 1531 | `		/* Invoke the old consumer on the decoded array */` |
|   1350 | 1532 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    745 | 1533 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|      - | 1534 | `		ProcJsonConsumer xOld;` |
|      - | 1535 | `		ph7_value *pKey;` |
|      - | 1536 | `		void *pOld;` |
|      - | 1537 | `		/* Same container rule as '[' above. */` |
|     72 | 1538 | `		if( pDecoder->rec_count + 1 >= pDecoder->rec_depth ){` |
|    ! 0 | 1539 | `			*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|    ! 0 | 1540 | `			return SXERR_ABORT;` |
|      - | 1541 | `		}` |
|      - | 1542 | `		/* Object representation*/` |
|     72 | 1543 | `		pDecoder->pIn++;` |
|      - | 1544 | `		/* Decode into a working array first; unless the caller asked for` |
|      - | 1545 | `		 * associative arrays (assoc=true / JSON_OBJECT_AS_ARRAY), it is converted` |
|      - | 1546 | `		 * to a stdClass below so json_decode('{...}') returns an object like php. */` |
|     72 | 1547 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|     72 | 1548 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|     72 | 1549 | `		if( pWorker == 0 \|\| pKey == 0){` |
|    ! 0 | 1550 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 1551 | `			/* Abort the decoding operation immediately */` |
|    ! 0 | 1552 | `			return SXERR_ABORT;` |
|      - | 1553 | `		}` |
|      - | 1554 | `		/* Save the old consumer */` |
|     72 | 1555 | `		xOld = pDecoder->xConsumer;` |
|     72 | 1556 | `		pOld = pDecoder->pUserData;` |
|      - | 1557 | `		/* Set the new consumer */` |
|     72 | 1558 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|     72 | 1559 | `		pDecoder->pUserData = pWorker;` |
|      - | 1560 | `		/* Decode the object */` |
|     69 | 1561 | `		for(;;){` |
|      - | 1562 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|      - | 1563 | `			 * do this.` |
|      - | 1564 | `			 */` |
|    156 | 1565 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|     17 | 1566 | `				pDecoder->pIn++;` |
|      3 | 1567 | `			}` |
|    142 | 1568 | `			if( pDecoder->pIn >= pDecoder->pEnd ){` |
|      - | 1569 | `				/* Ran out of tokens before the closing '}': php rejects an` |
|      - | 1570 | `				 * unterminated object as a syntax error. */` |
|      3 | 1571 | `				*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      3 | 1572 | `				return SXERR_ABORT;` |
|      - | 1573 | `			}` |
|    140 | 1574 | `			if( pDecoder->pIn->nType & JSON_TK_CCB /*'}'*/ ){` |
|     59 | 1575 | `				pDecoder->pIn++; /* Jump the trailing '}' */` |
|     59 | 1576 | `				break;` |
|      - | 1577 | `			}` |
|     80 | 1578 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|     84 | 1579 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|      - | 1580 | `					/* Syntax error,return immediately */` |
|    ! 0 | 1581 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1582 | `					return SXERR_ABORT;` |
|      - | 1583 | `			}` |
|      - | 1584 | `			/* Dequote the key */` |
|     84 | 1585 | `			rcQ = VmJsonDequoteString(&pDecoder->pIn->sData,pKey,pDecoder->iUserFlags);` |
|     84 | 1586 | `			if( rcQ != JSON_ERROR_NONE ){` |
|      3 | 1587 | `				*pDecoder->pErr = rcQ;` |
|      3 | 1588 | `				return SXERR_ABORT;` |
|      - | 1589 | `			}` |
|     82 | 1590 | `			if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|      - | 1591 | `				/* Decoding to an OBJECT: php refuses a property name whose` |
|      - | 1592 | `				 * FIRST byte is NUL ("\0...") with` |
|      - | 1593 | `				 * JSON_ERROR_INVALID_PROPERTY_NAME — that prefix is reserved` |
|      - | 1594 | `				 * for its mangled private/protected names. A NUL further in is` |
|      - | 1595 | `				 * legal, and array mode (assoc / JSON_OBJECT_AS_ARRAY /` |
|      - | 1596 | `				 * json_validate) takes any key. PHL used to build the property` |
|      - | 1597 | `				 * in silence. */` |
|      - | 1598 | `				int nKeyByte;` |
|     40 | 1599 | `				const char *zKey = ph7_value_to_string(pKey,&nKeyByte);` |
|     40 | 1600 | `				if( nKeyByte > 0 && zKey[0] == '\0' ){` |
|      3 | 1601 | `					*pDecoder->pErr = JSON_ERROR_INVALID_PROPERTY_NAME;` |
|      3 | 1602 | `					return SXERR_ABORT;` |
|      - | 1603 | `				}` |
|     17 | 1604 | `			}` |
|      - | 1605 | `			/* Jump the key and the colon */` |
|     80 | 1606 | `			pDecoder->pIn += 2;` |
|      - | 1607 | `			/* Recurse and decode the value */` |
|     80 | 1608 | `			pDecoder->rec_count++;` |
|     80 | 1609 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|     80 | 1610 | `			pDecoder->rec_count--;` |
|     80 | 1611 | `			if( rc == SXERR_ABORT ){` |
|      - | 1612 | `				/* Abort processing immediately */` |
|      8 | 1613 | `				return SXERR_ABORT;` |
|      - | 1614 | `			}` |
|      - | 1615 | `			/* Reset the internal buffer of the key */` |
|     73 | 1616 | `			ph7_value_reset_string_cursor(pKey);` |
|      - | 1617 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|      3 | 1618 | `		}` |
|      - | 1619 | `		/* Restore the old consumer */` |
|     59 | 1620 | `		pDecoder->xConsumer = xOld;` |
|     59 | 1621 | `		pDecoder->pUserData = pOld;` |
|      - | 1622 | `		/* php returns a stdClass for a JSON object (one dynamic property per member,` |
|      - | 1623 | `		 * nested objects already converted by the recursion) unless assoc was asked. */` |
|     59 | 1624 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|     23 | 1625 | `			PH7_MemObjToObject(pWorker);` |
|     10 | 1626 | `		}` |
|      - | 1627 | `		/* Invoke the old consumer on the decoded object*/` |
|     59 | 1628 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|      - | 1629 | `		/* Release the key */` |
|     59 | 1630 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|     31 | 1631 | `	}else{` |
|      - | 1632 | `		/* Unexpected token */` |
|    ! 0 | 1633 | `		return SXERR_ABORT; /* Abort immediately */` |
|      - | 1634 | `	}` |
|      - | 1635 | `	/* Release the worker variable */` |
|   1604 | 1636 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   1604 | 1637 | `	return SXRET_OK;` |
|   1355 | 1638 | `}` |
|      - | 1639 | `/*` |
|      - | 1640 | ` * The following JSON decoder callback is invoked each time` |
|      - | 1641 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|      - | 1642 | ` * is being decoded.` |
|      - | 1643 | ` */` |
|   1446 | 1644 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|      4 | 1645 | `{` |
|   1450 | 1646 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 1647 | `	/* Insert the entry */` |
|   1450 | 1648 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|    723 | 1649 | `	SXUNUSED(pCtx); /* cc warning */` |
|      - | 1650 | `	/* All done */` |
|   1450 | 1651 | `	return SXRET_OK;` |
|      4 | 1652 | `}` |
|      - | 1653 | `/*` |
|      - | 1654 | ` * Standard JSON decoder callback.` |
|      - | 1655 | ` */` |
|    154 | 1656 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|      4 | 1657 | `{` |
|      - | 1658 | `	/* Return the value directly */` |
|    158 | 1659 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|     77 | 1660 | `	SXUNUSED(pKey); /* cc warning */` |
|     77 | 1661 | `	SXUNUSED(pUserData);` |
|      - | 1662 | `	/* All done */` |
|    158 | 1663 | `	return SXRET_OK;` |
|      4 | 1664 | `}` |
|      - | 1665 | `/*` |
|      - | 1666 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 512[,int $options = 0 ]]])` |
|      - | 1667 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|      - | 1668 | ` * Parameters` |
|      - | 1669 | ` *  $json` |
|      - | 1670 | ` *    The json string being decoded.` |
|      - | 1671 | ` * $assoc` |
|      - | 1672 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|      - | 1673 | ` * $depth` |
|      - | 1674 | ` *   User specified recursion depth.` |
|      - | 1675 | ` * $options` |
|      - | 1676 | ` *   Bitmask of JSON decode options: JSON_OBJECT_AS_ARRAY (objects decode as` |
|      - | 1677 | ` *   associative arrays when $assoc is NULL), JSON_BIGINT_AS_STRING (an integer` |
|      - | 1678 | ` *   beyond int64 stays the exact source text instead of a float),` |
|      - | 1679 | ` *   JSON_INVALID_UTF8_IGNORE/_SUBSTITUTE and JSON_THROW_ON_ERROR` |
|      - | 1680 | ` * Return` |
|      - | 1681 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|      - | 1682 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|      - | 1683 | ` *  or if the encoded data is deeper than the recursion limit.` |
|      - | 1684 | ` */` |
|      - | 1685 | `/*` |
|      - | 1686 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|      - | 1687 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|      - | 1688 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|      - | 1689 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|      - | 1690 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|      - | 1691 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|      - | 1692 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|      - | 1693 | ` */` |
|    276 | 1694 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth,int iUserFlags)` |
|      4 | 1695 | `{` |
|    280 | 1696 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1697 | `	json_decoder sDecoder;` |
|      - | 1698 | `	SySet sToken;` |
|      - | 1699 | `	SyLex sLex;` |
|      - | 1700 | `	sxi32 rc;` |
|      - | 1701 | `	/* Clear JSON error code */` |
|    280 | 1702 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|      - | 1703 | `	/* Tokenize the input */` |
|    280 | 1704 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|    280 | 1705 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|    280 | 1706 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|    280 | 1707 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|      - | 1708 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|     54 | 1709 | `		SyLexRelease(&sLex);` |
|     54 | 1710 | `		SySetRelease(&sToken);` |
|     54 | 1711 | `		return pVm->json_rc;` |
|      - | 1712 | `	}` |
|      - | 1713 | `	/* Fill the decoder */` |
|    228 | 1714 | `	sDecoder.pCtx = pCtx;` |
|    228 | 1715 | `	sDecoder.pErr = &pVm->json_rc;` |
|    228 | 1716 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    228 | 1717 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|    228 | 1718 | `	sDecoder.iFlags = 0;` |
|    228 | 1719 | `	if( iAssoc ){` |
|      - | 1720 | `		/* Returned objects will be converted into associative arrays */` |
|    135 | 1721 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|     66 | 1722 | `	}` |
|    228 | 1723 | `	sDecoder.iUserFlags = iUserFlags;` |
|      - | 1724 | `	/* php's $depth (default 512; the callers' ValueError screens guarantee` |
|      - | 1725 | `	 * 1..INT_MAX-1), bounded by the engine's stack-safety ceiling. The old code` |
|      - | 1726 | `	 * CLAMPED it to an engine limit of 32, so a 40-deep document php decodes` |
|      - | 1727 | `	 * answered NULL/JSON_ERROR_DEPTH. Recursion is bounded by the INPUT's` |
|      - | 1728 | `	 * actual nesting, never by the requested ceiling. */` |
|    228 | 1729 | `	sDecoder.rec_depth = nDepth > PH7_JSON_DEPTH_CEILING ? PH7_JSON_DEPTH_CEILING : nDepth;` |
|    228 | 1730 | `	sDecoder.rec_count = 0;` |
|      - | 1731 | `	/* Set a default consumer */` |
|    228 | 1732 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|    228 | 1733 | `	sDecoder.pUserData = 0;` |
|      - | 1734 | `	/* Decode the raw JSON input */` |
|    228 | 1735 | `	rc = VmJsonDecode(&sDecoder,0);` |
|    228 | 1736 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|      - | 1737 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|    ! 0 | 1738 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1739 | `	}` |
|    228 | 1740 | `	if( pVm->json_rc == JSON_ERROR_NONE && sDecoder.pIn < sDecoder.pEnd ){` |
|      - | 1741 | `		/* php requires the whole input to be ONE JSON value; tokens left after a` |
|      - | 1742 | `		 * complete value (e.g. '"a":1', '{}x', '1 2') are a syntax error. */` |
|      3 | 1743 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|      1 | 1744 | `	}` |
|      - | 1745 | `	/* Clean-up the mess left behind */` |
|    228 | 1746 | `	SyLexRelease(&sLex);` |
|    228 | 1747 | `	SySetRelease(&sToken);` |
|    228 | 1748 | `	return pVm->json_rc;` |
|    142 | 1749 | `}` |
|    248 | 1750 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1751 | `{` |
|      - | 1752 | `	const char *zIn;` |
|      - | 1753 | `	int nByte;` |
|    252 | 1754 | `	int iAssoc = 0;` |
|    252 | 1755 | `	int nDepth = 512;` |
|    252 | 1756 | `	int iFlags = 0;` |
|      - | 1757 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1758 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|    252 | 1759 | `	if( nArg < 1 ){` |
|      - | 1760 | `		/* Missing/Invalid arguments, return NULL */` |
|    ! 0 | 1761 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1762 | `		return PH7_OK;` |
|      - | 1763 | `	}` |
|    252 | 1764 | `	if( nArg > 3 && ph7_value_is_int(apArg[3]) ){` |
|      - | 1765 | `		/* $flags (JSON_OBJECT_AS_ARRAY / JSON_BIGINT_AS_STRING /` |
|      - | 1766 | `		 * JSON_INVALID_UTF8_* / JSON_THROW_ON_ERROR). */` |
|     40 | 1767 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|     19 | 1768 | `	}` |
|      - | 1769 | `	/* Extract the JSON string */` |
|    252 | 1770 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|    252 | 1771 | `	if( nByte < 1 ){` |
|      - | 1772 | `		/* Empty string: php records a syntax error (json_last_error() == 4) and` |
|      - | 1773 | `		 * returns NULL, or raises a JsonException with JSON_THROW_ON_ERROR. */` |
|      9 | 1774 | `		pCtx->pVm->json_rc = JSON_ERROR_SYNTAX;` |
|      9 | 1775 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|    ! 0 | 1776 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|    ! 0 | 1777 | `				JSON_ERROR_SYNTAX,"%s",JsonErrorMsg(JSON_ERROR_SYNTAX));` |
|      - | 1778 | `		}` |
|      9 | 1779 | `		ph7_result_null(pCtx);` |
|      9 | 1780 | `		return PH7_OK;` |
|      - | 1781 | `	}` |
|    246 | 1782 | `	if( nArg > 1 ){` |
|      - | 1783 | `		/* php's $associative is a ?bool: an explicit true/false decides on its` |
|      - | 1784 | `		 * own (false beats the flag), and only NULL lets JSON_OBJECT_AS_ARRAY` |
|      - | 1785 | `		 * answer instead. */` |
|    149 | 1786 | `		if( ph7_value_is_null(apArg[1]) ){` |
|      7 | 1787 | `			iAssoc = (iFlags & JSON_OBJECT_AS_ARRAY) != 0;` |
|      4 | 1788 | `		}else{` |
|    143 | 1789 | `			iAssoc = ph7_value_to_bool(apArg[1]) != 0;` |
|      - | 1790 | `		}` |
|     73 | 1791 | `	}` |
|    246 | 1792 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      - | 1793 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);` |
|      - | 1794 | `		 * read as int64 so a value above INT_MAX is detected, not truncated. */` |
|     66 | 1795 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);` |
|      - | 1796 | `		/* php clears the json error state before validating $depth, so a caught` |
|      - | 1797 | `		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal` |
|      - | 1798 | `		 * path resets it again inside VmJsonDecodeInput). */` |
|     66 | 1799 | `		pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|     66 | 1800 | `		if( nWant <= 0 ){` |
|      9 | 1801 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1802 | `				"json_decode(): Argument #3 ($depth) must be greater than 0");` |
|      - | 1803 | `		}` |
|     58 | 1804 | `		if( nWant > 2147483647 ){` |
|      3 | 1805 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1806 | `				"json_decode(): Argument #3 ($depth) must be less than 2147483647");` |
|      - | 1807 | `		}` |
|     56 | 1808 | `		nDepth = (int)nWant;` |
|     27 | 1809 | `	}` |
|      - | 1810 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|      - | 1811 | `	 * call-context result; on failure we replace it with NULL (or throw). */` |
|    236 | 1812 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth,iFlags) != JSON_ERROR_NONE ){` |
|      - | 1813 | `		/* Something goes wrong while decoding JSON input. */` |
|    105 | 1814 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|      - | 1815 | `			/* php: raise a JsonException carrying json_last_error_msg() text. */` |
|     11 | 1816 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|      6 | 1817 | `				(sxi32)pCtx->pVm->json_rc,"%s",` |
|      6 | 1818 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|      - | 1819 | `		}` |
|     99 | 1820 | `		ph7_result_null(pCtx);` |
|     48 | 1821 | `	}` |
|      - | 1822 | `	/* All done */` |
|    230 | 1823 | `	return PH7_OK;` |
|    128 | 1824 | `}` |
|      - | 1825 | `/*` |
|      - | 1826 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|      - | 1827 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|      - | 1828 | ` * Parameters` |
|      - | 1829 | ` *  $json   The string to validate.` |
|      - | 1830 | ` *  $depth  Maximum nesting depth (php's default of 512, honored verbatim).` |
|      - | 1831 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|      - | 1832 | ` * Return` |
|      - | 1833 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|      - | 1834 | ` */` |
|     58 | 1835 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1836 | `{` |
|     60 | 1837 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1838 | `	const char *zIn;` |
|      - | 1839 | `	int nByte;` |
|     60 | 1840 | `	int nDepth = 512;` |
|     60 | 1841 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1842 | `		/* Missing/Invalid argument: not valid JSON */` |
|    ! 0 | 1843 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    ! 0 | 1844 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1845 | `		return PH7_OK;` |
|      - | 1846 | `	}` |
|      - | 1847 | `	/* Extract the JSON string */` |
|     60 | 1848 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|     60 | 1849 | `	if( nByte < 1 ){` |
|      - | 1850 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|      - | 1851 | `		 * silently, json_validate must record the syntax error) */` |
|      6 | 1852 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|      6 | 1853 | `		ph7_result_bool(pCtx,0);` |
|      6 | 1854 | `		return PH7_OK;` |
|      - | 1855 | `	}` |
|     56 | 1856 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1857 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */` |
|     24 | 1858 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|      - | 1859 | `		/* Clear the json error state before validating $depth (php parity), so a` |
|      - | 1860 | `		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */` |
|     24 | 1861 | `		pVm->json_rc = JSON_ERROR_NONE;` |
|     24 | 1862 | `		if( nWant <= 0 ){` |
|      5 | 1863 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1864 | `				"json_validate(): Argument #2 ($depth) must be greater than 0");` |
|      - | 1865 | `		}` |
|     20 | 1866 | `		if( nWant > 2147483647 ){` |
|      3 | 1867 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1868 | `				"json_validate(): Argument #2 ($depth) must be less than 2147483647");` |
|      - | 1869 | `		}` |
|     18 | 1870 | `		nDepth = (int)nWant;` |
|      8 | 1871 | `	}` |
|      - | 1872 | `	/* php's only ACCEPTED $flags value here is JSON_INVALID_UTF8_IGNORE, which` |
|      - | 1873 | `	 * makes a payload with undecodable bytes VALID; it rides the same rail as` |
|      - | 1874 | `	 * json_decode's. Any other bit is a ValueError naming the one flag there is --` |
|      - | 1875 | `	 * php refuses the whole json_decode set for this function, and PHL used to take` |
|      - | 1876 | `	 * whatever it was given and validate on. Decode in associative mode so the` |
|      - | 1877 | `	 * "objects are returned as an array" warning is not raised - the decoded value` |
|      - | 1878 | `	 * is discarded, only its validity matters. */` |
|      - | 1879 | `	{` |
|     31 | 1880 | `		int iFlags = (nArg > 2 && ph7_value_is_int(apArg[2]))` |
|     34 | 1881 | `			? ph7_value_to_int(apArg[2]) : 0;` |
|     50 | 1882 | `		if( (iFlags & ~JSON_INVALID_UTF8_IGNORE) != 0 ){` |
|      5 | 1883 | `			pVm->json_rc = JSON_ERROR_NONE;` |
|      5 | 1884 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1885 | `				"json_validate(): Argument #3 ($flags) must be a valid flag "` |
|      - | 1886 | `				"(allowed flags: JSON_INVALID_UTF8_IGNORE)");` |
|      - | 1887 | `		}` |
|     68 | 1888 | `		ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth,iFlags)` |
|     22 | 1889 | `			== JSON_ERROR_NONE);` |
|      - | 1890 | `	}` |
|     46 | 1891 | `	return PH7_OK;` |
|     31 | 1892 | `}` |
|      - | 1893 |  |
