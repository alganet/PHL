# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 752/837 lines (89.84%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `/*` |
|    - |    8 | ` * Section:` |
|    - |    9 | ` *  JSON encoding/decoding routines.` |
|    - |   10 | ` * Status:` |
|    - |   11 | ` *    Devel.` |
|    - |   12 | ` */` |
|    - |   13 | `/* Forward reference */` |
|    - |   14 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|    - |   15 | `static int VmJsonObjectEncode(const SyString *pAttr,ph7_value *pValue,void *pUserData);` |
|    - |   16 | `/*` |
|    - |   17 | ` * JSON encoder state is stored in an instance` |
|    - |   18 | ` * of the following structure.` |
|    - |   19 | ` */` |
|    - |   20 | `typedef struct json_private_data json_private_data;` |
|    - |   21 | `struct json_private_data` |
|    - |   22 | `{` |
|    - |   23 | `	ph7_context *pCtx; /* Call context */` |
|    - |   24 | `	int isFirst;       /* True if first encoded entry */` |
|    - |   25 | `	int isObject;      /* True if the current array level is encoded as a JSON object */` |
|    - |   26 | `	int iFlags;        /* JSON encoding flags */` |
|    - |   27 | `	int nRecCount;     /* Recursion count */` |
|    - |   28 | `	int exc;           /* True if a jsonSerialize() callback threw an exception */` |
|    - |   29 | `	int oom;           /* True if a result append ran out of memory (raises a fatal) */` |
|    - |   30 | `	int fail;          /* True if the value is unencodable — json_encode returns` |
|    - |   31 | `	                    * FALSE (or throws under JSON_THROW_ON_ERROR) */` |
|    - |   32 | `	int failRc;        /* json_rc to report for a ->fail (INF_OR_NAN vs` |
|    - |   33 | `	                    * NON_BACKED_ENUM) */` |
|    - |   34 | `};` |
|    - |   35 | `/*` |
|    - |   36 | ` * Emit into the JSON result, flagging OOM on the shared data and bailing out` |
|    - |   37 | ` * of the current encode function (which returns PH7_OK; the top-level` |
|    - |   38 | ` * vm_builtin_json_encode checks ->oom and raises a non-catchable fatal). Used` |
|    - |   39 | ` * for every ph7_result_string/ph7_result_string_format append below.` |
|    - |   40 | ` */` |
|    - |   41 | `#define JSON_EMIT(pD, call) do { if( (call) != SXRET_OK ){ (pD)->oom = 1; return PH7_OK; } } while(0)` |
|    - |   42 | `/*` |
|    - |   43 | ` * Emit a float in php's json shape: PH7_AppendShortestReal (the shared` |
|    - |   44 | ` * serialize/var_export shortest-round-trip formatter, php's` |
|    - |   45 | ` * serialize_precision=-1) with the exponent marker lowercased (json prints` |
|    - |   46 | ` * 1.0e+17 where serialize prints 1.0E+17).` |
|    - |   47 | ` */` |
|   30 |   48 | `static sxi32 VmJsonEmitReal(ph7_context *pCtx,double rVal)` |
|    2 |   49 | `{` |
|    - |   50 | `	SyBlob sNum;` |
|    - |   51 | `	char *z;` |
|    - |   52 | `	sxu32 i,n;` |
|    - |   53 | `	sxi32 rc;` |
|   32 |   54 | `	SyBlobInit(&sNum,&pCtx->pVm->sAllocator);` |
|   32 |   55 | `	PH7_AppendShortestReal(&sNum,rVal);` |
|   32 |   56 | `	z = (char *)SyBlobData(&sNum);` |
|   32 |   57 | `	n = SyBlobLength(&sNum);` |
|   32 |   58 | `	if( z == 0 \|\| n < 1 ){` |
|  ! 0 |   59 | `		SyBlobRelease(&sNum);` |
|  ! 0 |   60 | `		return SXERR_MEM; /* treated as OOM by JSON_EMIT */` |
|    - |   61 | `	}` |
|  248 |   62 | `	for( i = 0 ; i < n ; i++ ){` |
|  218 |   63 | `		if( z[i] == 'E' ){` |
|    7 |   64 | `			z[i] = 'e';` |
|    3 |   65 | `		}` |
|  110 |   66 | `	}` |
|   32 |   67 | `	rc = ph7_result_string(pCtx,(const char *)z,(int)n);` |
|   32 |   68 | `	SyBlobRelease(&sNum);` |
|   32 |   69 | `	return rc;` |
|   17 |   70 | `}` |
|    - |   71 | `/*` |
|    - |   72 | ` * JSON_PRETTY_PRINT helper: emit a newline followed by (depth * 4) spaces, so a` |
|    - |   73 | ` * container's members are laid out one-per-line and indented like php. A no-op` |
|    - |   74 | ` * unless JSON_PRETTY_PRINT is set. Returns SXRET_OK or an OOM status; callers` |
|    - |   75 | ` * wrap it in JSON_EMIT so an allocation failure trips the ->oom rail.` |
|    - |   76 | ` */` |
| 2280 |   77 | `static sxi32 VmJsonPretty(json_private_data *pJson,int depth)` |
|    5 |   78 | `{` |
| 2285 |   79 | `	ph7_context *pCtx = pJson->pCtx;` |
|    - |   80 | `	sxi32 rc;` |
|    - |   81 | `	int i;` |
| 2285 |   82 | `	if( (pJson->iFlags & JSON_PRETTY_PRINT) == 0 ){` |
| 2237 |   83 | `		return SXRET_OK;` |
|    - |   84 | `	}` |
|   49 |   85 | `	rc = ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|  121 |   86 | `	for( i = 0 ; i < depth && rc == SXRET_OK ; ++i ){` |
|   73 |   87 | `		rc = ph7_result_string(pCtx,"    ",(int)sizeof("    ")-1);` |
|   37 |   88 | `	}` |
|   49 |   89 | `	return rc;` |
| 1145 |   90 | `}` |
|    - |   91 | `/*` |
|    - |   92 | ` * Byte length of the ill-formed UTF-8 run at z[0..n-1] as php's JSON encoder` |
|    - |   93 | ` * measures it: a byte that could LEAD a sequence (C2..F4) swallows every` |
|    - |   94 | ` * following byte that is merely continuation-SHAPED (10xxxxxx), up to the` |
|    - |   95 | ` * length its lead announces, and the whole prefix is ONE error. So "\xed\xa0\x80"` |
|    - |   96 | ` * (a surrogate) is a single JSON_ERROR_UTF8 / a single U+FFFD, while` |
|    - |   97 | ` * "\xf5\x80\x80\x80" is four — F5 leads nothing, so each byte fails alone.` |
|    - |   98 | ` *` |
|    - |   99 | ` * php's mbstring measures the same runs with the STRICTER per-lead ranges` |
|    - |  100 | ` * (builtin_mb.c's MbUtf8BadLen), which is why the two disagree on a surrogate:` |
|    - |  101 | ` * mb_strtolower("\xed\xa0\x80") is "???" while json substitutes one U+FFFD.` |
|    - |  102 | ` * Two php decoders, two rules — each matched where it belongs.` |
|    - |  103 | ` */` |
|   90 |  104 | `static sxu32 VmJsonBadUtf8Len(const unsigned char *z,sxu32 n)` |
|    1 |  105 | `{` |
|   91 |  106 | `	sxu32 c = z[0],need,i;` |
|   91 |  107 | `	if( c >= 0xC2 && c <= 0xDF ){` |
|    7 |  108 | `		need = 2;` |
|   88 |  109 | `	}else if( c >= 0xE0 && c <= 0xEF ){` |
|   13 |  110 | `		need = 3;` |
|   79 |  111 | `	}else if( c >= 0xF0 && c <= 0xF4 ){` |
|    7 |  112 | `		need = 4;` |
|    4 |  113 | `	}else{` |
|   67 |  114 | `		return 1; /* 80..C1 or F5..FF: leads nothing */` |
|    - |  115 | `	}` |
|   55 |  116 | `	for( i = 1 ; i < need && i < n && (z[i] & 0xC0) == 0x80 ; ++i ){}` |
|   25 |  117 | `	return i;` |
|   46 |  118 | `}` |
|    - |  119 | `/*` |
|    - |  120 | ` * Emit one code point as php's \uXXXX escape (lowercase hex), spelling anything` |
|    - |  121 | ` * outside the BMP as the UTF-16 surrogate pair JSON has no other way to carry:` |
|    - |  122 | ` * U+1F600 is "😀", exactly like php.` |
|    - |  123 | ` */` |
|   64 |  124 | `static sxi32 VmJsonEmitUnicodeEscape(ph7_context *pCtx,sxu32 cp)` |
|    1 |  125 | `{` |
|    - |  126 | `	static const char zHex[] = "0123456789abcdef";` |
|    - |  127 | `	sxu32 aUnit[2];` |
|    - |  128 | `	int nUnit,i;` |
|    - |  129 | `	char zEsc[12];` |
|   65 |  130 | `	if( cp >= 0x10000 ){` |
|    5 |  131 | `		sxu32 v = cp - 0x10000;` |
|    5 |  132 | `		aUnit[0] = 0xD800 + (v >> 10);` |
|    5 |  133 | `		aUnit[1] = 0xDC00 + (v & 0x3FF);` |
|    5 |  134 | `		nUnit = 2;` |
|    3 |  135 | `	}else{` |
|   61 |  136 | `		aUnit[0] = cp;` |
|   61 |  137 | `		nUnit = 1;` |
|    - |  138 | `	}` |
|  133 |  139 | `	for( i = 0 ; i < nUnit ; ++i ){` |
|   69 |  140 | `		zEsc[i*6 + 0] = '\\';` |
|   69 |  141 | `		zEsc[i*6 + 1] = 'u';` |
|   69 |  142 | `		zEsc[i*6 + 2] = zHex[(aUnit[i] >> 12) & 0x0F];` |
|   69 |  143 | `		zEsc[i*6 + 3] = zHex[(aUnit[i] >>  8) & 0x0F];` |
|   69 |  144 | `		zEsc[i*6 + 4] = zHex[(aUnit[i] >>  4) & 0x0F];` |
|   69 |  145 | `		zEsc[i*6 + 5] = zHex[ aUnit[i]        & 0x0F];` |
|   35 |  146 | `	}` |
|   65 |  147 | `	return ph7_result_string(pCtx,zEsc,nUnit * 6);` |
|    1 |  148 | `}` |
|    - |  149 | `/*` |
|    - |  150 | ` * Emit one JSON string literal — the opening quote, the escaped body, the` |
|    - |  151 | ` * closing quote. Shared by the string VALUE path and by both KEY paths (array` |
|    - |  152 | ` * keys and object property names), which used to append their bytes raw: a key` |
|    - |  153 | ` * carrying a '"', a backslash or a control character produced UNPARSEABLE` |
|    - |  154 | ` * output (php: json_encode(["a\"b"=>1]) is {"a\"b":1}, PHL emitted {"a"b":1}).` |
|    - |  155 | ` * Everything php escapes in a string it escapes in a key, the JSON_HEX_*` |
|    - |  156 | ` * and JSON_UNESCAPED_SLASHES flags included.` |
|    - |  157 | ` *` |
|    - |  158 | ` * Non-ASCII is escaped as \uXXXX by DEFAULT, which is what php does and what` |
|    - |  159 | ` * JSON_UNESCAPED_UNICODE turns off — PHL used to emit the raw UTF-8 bytes` |
|    - |  160 | ` * unconditionally, i.e. behave as if that flag were always set (the flag was` |
|    - |  161 | ` * defined but never read). Even with it set php still escapes U+2028/U+2029,` |
|    - |  162 | ` * the two line terminators JavaScript's eval() chokes on, unless` |
|    - |  163 | ` * JSON_UNESCAPED_LINE_TERMINATORS is set too.` |
|    - |  164 | ` */` |
| 1634 |  165 | `static sxi32 VmJsonEncodeString(json_private_data *pData,const char *zIn,int nByte)` |
|    5 |  166 | `{` |
| 1639 |  167 | `	ph7_context *pCtx = pData->pCtx;` |
| 1639 |  168 | `	int iFlags = pData->iFlags;` |
| 1639 |  169 | `	const char *zEnd = &zIn[nByte];` |
|    - |  170 | `	sxi32 rc;` |
|    - |  171 | `	char c;` |
| 1639 |  172 | `	rc = ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
| 1639 |  173 | `	if( rc != SXRET_OK ){` |
|  ! 0 |  174 | `		return rc;` |
|    - |  175 | `	}` |
| 2793 |  176 | `	for(;;){` |
| 5699 |  177 | `		if( zIn >= zEnd ){` |
|    - |  178 | `			/* No more input to process */` |
| 1611 |  179 | `			break;` |
|    - |  180 | `		}` |
| 4093 |  181 | `		if( (unsigned char)zIn[0] >= 0x80 ){` |
|    - |  182 | `			/* A UTF-8 sequence: decode it strictly, since \uXXXX needs the code` |
|    - |  183 | `			 * point and not the bytes. */` |
|    - |  184 | `			sxu32 nLen,cp;` |
|  137 |  185 | `			sxi32 iCp = PH7_Utf8ReadStrict((const unsigned char *)zIn,(sxu32)(zEnd - zIn),&nLen);` |
|  137 |  186 | `			if( iCp < 0 ){` |
|    - |  187 | `				/* Ill-formed. php REFUSES to encode it: json_encode returns` |
|    - |  188 | `				 * false with json_last_error() == JSON_ERROR_UTF8, because` |
|    - |  189 | `				 * there is no honest JSON spelling for a byte that is not` |
|    - |  190 | `				 * text. PHL used to pass the byte through, so the caller got a` |
|    - |  191 | `				 * valid-looking payload php would never have produced and no` |
|    - |  192 | `				 * error check could see it. The two JSON_INVALID_UTF8_* flags` |
|    - |  193 | `				 * are the opt-outs php offers. */` |
|   81 |  194 | `				nLen = VmJsonBadUtf8Len((const unsigned char *)zIn,(sxu32)(zEnd - zIn));` |
|   81 |  195 | `				if( iFlags & JSON_INVALID_UTF8_IGNORE ){` |
|   25 |  196 | `					rc = SXRET_OK; /* drop the run */` |
|   69 |  197 | `				}else if( iFlags & JSON_INVALID_UTF8_SUBSTITUTE ){` |
|   29 |  198 | `					rc = (iFlags & JSON_UNESCAPED_UNICODE)` |
|    2 |  199 | `						? ph7_result_string(pCtx,"\357\277\275",3) /* U+FFFD */` |
|   27 |  200 | `						: VmJsonEmitUnicodeEscape(pCtx,0xFFFD);` |
|   15 |  201 | `				}else{` |
|   29 |  202 | `					pData->fail = 1;` |
|   29 |  203 | `					pData->failRc = JSON_ERROR_UTF8;` |
|   29 |  204 | `					return SXRET_OK; /* the whole encode is discarded */` |
|    - |  205 | `				}` |
|   27 |  206 | `			}else{` |
|   57 |  207 | `				cp = (sxu32)iCp;` |
|   56 |  208 | `				if( (iFlags & JSON_UNESCAPED_UNICODE) == 0` |
|   40 |  209 | `				 \|\| ((cp == 0x2028 \|\| cp == 0x2029)` |
|   14 |  210 | `				  && (iFlags & JSON_UNESCAPED_LINE_TERMINATORS) == 0) ){` |
|   39 |  211 | `					rc = VmJsonEmitUnicodeEscape(pCtx,cp);` |
|   20 |  212 | `				}else{` |
|   19 |  213 | `					rc = ph7_result_string(pCtx,zIn,(int)nLen);` |
|    - |  214 | `				}` |
|    - |  215 | `			}` |
|  109 |  216 | `			zIn += nLen;` |
|  109 |  217 | `			if( rc != SXRET_OK ){` |
|  ! 0 |  218 | `				return rc;` |
|    - |  219 | `			}` |
|  109 |  220 | `			continue;` |
|    - |  221 | `		}` |
| 3957 |  222 | `		c = zIn[0];` |
|    - |  223 | `		/* Advance the stream cursor */` |
| 3957 |  224 | `		zIn++;` |
| 3957 |  225 | `		if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |  226 | `			/* All < and > are converted to \u003C and \u003E */` |
|    5 |  227 | `			if( c == '<' ){` |
|    3 |  228 | `				rc = ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|    2 |  229 | `			}else{` |
|    3 |  230 | `				rc = ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|    1 |  231 | `			}` |
| 3955 |  232 | `		}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |  233 | `			/* All &s are converted to \u0026.  */` |
|    3 |  234 | `			rc = ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
| 3952 |  235 | `		}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  236 | `			/* All ' are converted to \u0027.   */` |
|    3 |  237 | `			rc = ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
| 3950 |  238 | `		}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  239 | `			/* All " are converted to \u0022. */` |
|    3 |  240 | `			rc = ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
| 3948 |  241 | `		}else if( (unsigned char)c < 0x20 ){` |
|    - |  242 | `			/* Control characters (band A #4): php emits the short escapes for` |
|    - |  243 | `			 * \b \f \n \r \t and \u00xx for the rest — pre-fix these were` |
|    - |  244 | `			 * emitted RAW (invalid JSON). */` |
|    - |  245 | `			static const char zHex[] = "0123456789abcdef";` |
|   77 |  246 | `			char zEsc[6] = { '\\', 'u', '0', '0', 0, 0 };` |
|   77 |  247 | `			switch(c){` |
|  ! 0 |  248 | `			case '\b': rc = ph7_result_string(pCtx,"\\b",2); break;` |
|  ! 0 |  249 | `			case '\f': rc = ph7_result_string(pCtx,"\\f",2); break;` |
|   47 |  250 | `			case '\n': rc = ph7_result_string(pCtx,"\\n",2); break;` |
|   18 |  251 | `			case '\r': rc = ph7_result_string(pCtx,"\\r",2); break;` |
|   12 |  252 | `			case '\t': rc = ph7_result_string(pCtx,"\\t",2); break;` |
|    2 |  253 | `			default:` |
|    5 |  254 | `				zEsc[4] = zHex[(c >> 4) & 0x0F];` |
|    5 |  255 | `				zEsc[5] = zHex[c & 0x0F];` |
|    5 |  256 | `				rc = ph7_result_string(pCtx,zEsc,6);` |
|    4 |  257 | `				break;` |
|    - |  258 | `			}` |
|   40 |  259 | `		}else{` |
| 3873 |  260 | `			if( c == '"' \|\| c == '\\' ){` |
|    - |  261 | `				/* Escape the quote/backslash (php escapes the backslash` |
|    - |  262 | `				 * unconditionally — the old code wrongly tied it to` |
|    - |  263 | `				 * JSON_UNESCAPED_SLASHES, which governs '/' below) */` |
|   16 |  264 | `				rc = ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
| 3866 |  265 | `			}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){` |
|    - |  266 | `				/* php escapes forward slashes by default */` |
|    9 |  267 | `				rc = ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|    5 |  268 | `			}else{` |
| 3851 |  269 | `				rc = SXRET_OK;` |
|    - |  270 | `			}` |
| 3873 |  271 | `			if( rc == SXRET_OK ){` |
|    - |  272 | `				/* Append character verbatim */` |
| 3873 |  273 | `				rc = ph7_result_string(pCtx,&c,(int)sizeof(char));` |
| 1934 |  274 | `			}` |
|    - |  275 | `		}` |
| 3957 |  276 | `		if( rc != SXRET_OK ){` |
|  ! 0 |  277 | `			return rc;` |
|    - |  278 | `		}` |
|    5 |  279 | `	}` |
| 1611 |  280 | `	return ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|  822 |  281 | `}` |
|    - |  282 | `/*` |
|    - |  283 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|    - |  284 | ` * According to wikipedia` |
|    - |  285 | ` * JSON's basic types are:` |
|    - |  286 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  287 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  288 | ` *   Boolean (true or false)` |
|    - |  289 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  290 | ` *    do not need to be of the same type)` |
|    - |  291 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  292 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  293 | ` *     be distinct from each other)` |
|    - |  294 | ` *   null (empty)` |
|    - |  295 | ` * Non-significant white space may be added freely around the "structural characters"` |
|    - |  296 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  297 | ` */` |
| 2436 |  298 | `static sxi32 VmJsonEncode(` |
|    - |  299 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |  300 | `	json_private_data *pData /* Context data */` |
|    5 |  301 | `	){` |
| 2441 |  302 | `		ph7_context *pCtx = pData->pCtx;` |
| 2441 |  303 | `		int iFlags = pData->iFlags;` |
|    - |  304 | `		int nByte;` |
| 2441 |  305 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |  306 | `			/* null */` |
|    7 |  307 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
| 2434 |  308 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   20 |  309 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |  310 | `			int iLen;` |
|    - |  311 | `			/* true/false */` |
|   20 |  312 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   20 |  313 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
| 2423 |  314 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|  905 |  315 | `			if( ph7_value_is_float(pIn) ){` |
|   34 |  316 | `				double rVal = ph7_value_to_double(pIn);` |
|    - |  317 | `				/* php rejects Inf/NaN: json_encode returns FALSE with` |
|    - |  318 | `				 * json_last_error() == JSON_ERROR_INF_OR_NAN (they have no JSON` |
|    - |  319 | `				 * representation), instead of emitting the invalid bare token. */` |
|   34 |  320 | `				if( PH7_IS_NAN(rVal) \|\| PH7_IS_INF(rVal) ){` |
|    7 |  321 | `					pData->fail = 1;` |
|    7 |  322 | `					pData->failRc = JSON_ERROR_INF_OR_NAN;` |
|    7 |  323 | `					return PH7_OK;` |
|    - |  324 | `				}` |
|    - |  325 | `				/* php's json float output follows serialize_precision` |
|    - |  326 | `				 * (shortest round-trip, like serialize/var_export), NOT the` |
|    - |  327 | `				 * echo/cast precision of 14 — with a lowercase exponent` |
|    - |  328 | `				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,` |
|    - |  329 | `				 * 1.0 -> 1, -0.0 -> -0. */` |
|   28 |  330 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,rVal));` |
|   15 |  331 | `			}else{` |
|    - |  332 | `				const char *zNum;` |
|    - |  333 | `				/* Get a string representation of the number */` |
|  575 |  334 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|  575 |  335 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|    5 |  336 | `			}` |
| 2113 |  337 | `		}else if( ph7_value_is_string(pIn) ){` |
| 1033 |  338 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |  339 | `				/* Encodes numeric strings as numbers (same float shapes). */` |
|    5 |  340 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    5 |  341 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    3 |  342 | `			}else{` |
|    - |  343 | `				const char *zIn;` |
|    - |  344 | `				/* Encode the string */` |
| 1029 |  345 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
| 1029 |  346 | `				JSON_EMIT(pData,VmJsonEncodeString(pData,zIn,nByte));` |
|    5 |  347 | `			}` |
| 1301 |  348 | `		}else if( ph7_value_is_array(pIn) ){` |
|    - |  349 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|    - |  350 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|    - |  351 | `			 * object with stringified keys (PHP semantics). */` |
| 1388 |  352 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|  692 |  353 | `				\|\| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);` |
|  697 |  354 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|  697 |  355 | `			int c = isObject ? '{' : '[';` |
|  697 |  356 | `			int d = isObject ? '}' : ']';` |
|    - |  357 | `			/* Encode the array */` |
|  697 |  358 | `			pData->isObject = isObject;` |
|  697 |  359 | `			pData->isFirst = 1;` |
|    - |  360 | `			/* Append the square bracket or curly braces */` |
|  697 |  361 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    - |  362 | `			/* Iterate throw array entries */` |
|  697 |  363 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  364 | `			/* Bail if a nested append ran out of memory before the closer */` |
|  697 |  365 | `			if( pData->oom ){` |
|  ! 0 |  366 | `				return PH7_OK;` |
|    - |  367 | `			}` |
|    - |  368 | `			/* Pretty-print: a non-empty container closes on its own line,` |
|    - |  369 | `			 * indented one level less than its members (isFirst is still 1` |
|    - |  370 | `			 * only when no entry was emitted -> keep "[]"/"{}" tight). */` |
|  697 |  371 | `			if( !pData->isFirst ){` |
|  627 |  372 | `				JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|  311 |  373 | `			}` |
|    - |  374 | `			/* Append the closing square bracket or curly braces */` |
|  697 |  375 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|  697 |  376 | `			pData->isObject = savedObject;` |
|  441 |  377 | `		}else if( ph7_value_is_object(pIn) ){` |
|   95 |  378 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   95 |  379 | `			ph7_vm *pVm = pIn->pVm;` |
|   95 |  380 | `			ph7_class_method *pMethod = 0;` |
|    - |  381 | `			/* If the object implements JsonSerializable, encode the value` |
|    - |  382 | `			 * returned by jsonSerialize() instead of its public properties.` |
|    - |  383 | `			 * An enum implementing it explicitly also takes this path (php). */` |
|   90 |  384 | `			if( pVm->pJsonSerializableClass` |
|   95 |  385 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|   17 |  386 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|    8 |  387 | `			}` |
|   95 |  388 | `			if( pMethod == 0 && (pThis->pClass->iFlags & PH7_CLASS_ENUM) != 0 ){` |
|    - |  389 | `				/* php 8.1: a BACKED enum case encodes as its backing value; a` |
|    - |  390 | `				 * pure enum case has no default serialization — json_encode` |
|    - |  391 | `				 * returns false. */` |
|    9 |  392 | `				ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pThis);` |
|    9 |  393 | `				if( pBacking ){` |
|    7 |  394 | `					pData->nRecCount++;` |
|    7 |  395 | `					VmJsonEncode(pBacking,pData);` |
|    7 |  396 | `					pData->nRecCount--;` |
|    4 |  397 | `				}else{` |
|    3 |  398 | `					pData->fail = 1;` |
|    3 |  399 | `					pData->failRc = JSON_ERROR_NON_BACKED_ENUM;` |
|    - |  400 | `				}` |
|    9 |  401 | `				return PH7_OK;` |
|    - |  402 | `			}` |
|   87 |  403 | `			if( pMethod ){` |
|    - |  404 | `				ph7_value sResult;` |
|    - |  405 | `				sxi32 rc;` |
|   17 |  406 | `				PH7_MemObjInit(pVm,&sResult);` |
|   17 |  407 | `				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|   17 |  408 | `				if( rc == PH7_EXCEPTION ){` |
|    - |  409 | `					/* Let jsonSerialize()'s throw propagate */` |
|    5 |  410 | `					PH7_MemObjRelease(&sResult);` |
|    5 |  411 | `					pData->exc = 1;` |
|    5 |  412 | `					return PH7_EXCEPTION;` |
|    - |  413 | `				}` |
|    - |  414 | `				/* Encode the returned value [scalar/array/object] */` |
|   13 |  415 | `				pData->nRecCount++;` |
|   13 |  416 | `				VmJsonEncode(&sResult,pData);` |
|   13 |  417 | `				pData->nRecCount--;` |
|   13 |  418 | `				PH7_MemObjRelease(&sResult);` |
|   13 |  419 | `				if( pData->exc ){` |
|  ! 0 |  420 | `					return PH7_EXCEPTION;` |
|    - |  421 | `				}` |
|   13 |  422 | `				if( pData->oom ){` |
|  ! 0 |  423 | `					return PH7_OK;` |
|    - |  424 | `				}` |
|    7 |  425 | `			}else{` |
|    - |  426 | `				SyHashEntry *pAttrEntry;` |
|    - |  427 | `				SySet sNames;` |
|    - |  428 | `				SyString *aName;` |
|    - |  429 | `				sxu32 iName,nName;` |
|    - |  430 | `				/* Encode the class instance: php serializes only PUBLIC` |
|    - |  431 | `				 * non-static properties, reading through a PHP 8.4 get hook` |
|    - |  432 | `				 * when one is declared (virtual properties included). The` |
|    - |  433 | `				 * names are SNAPSHOTTED first — a hook dispatched mid-walk may` |
|    - |  434 | `				 * re-enter an hAttr walk on this instance (the hash has a` |
|    - |  435 | `				 * single embedded loop cursor) or unset()/create properties;` |
|    - |  436 | `				 * names point into class-owned attr storage and each is` |
|    - |  437 | `				 * re-looked-up before use. */` |
|   71 |  438 | `				pData->isFirst = 1;` |
|    - |  439 | `				/* Append the curly braces */` |
|   71 |  440 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|   71 |  441 | `				SySetInit(&sNames,&pVm->sAllocator,sizeof(SyString));` |
|   71 |  442 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|  183 |  443 | `				while( (pAttrEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|  117 |  444 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|  112 |  445 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|  116 |  446 | `					 \|\| pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|   13 |  447 | `						continue;` |
|    - |  448 | `					}` |
|  102 |  449 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|   56 |  450 | `					 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|    3 |  451 | `						continue; /* virtual set-only property: no value to encode (php) */` |
|    - |  452 | `					}` |
|  105 |  453 | `					SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|    5 |  454 | `				}` |
|   71 |  455 | `				aName = (SyString *)SySetBasePtr(&sNames);` |
|   71 |  456 | `				nName = SySetUsed(&sNames);` |
|  171 |  457 | `				for( iName = 0 ; iName < nName ; ++iName ){` |
|    - |  458 | `					VmClassAttr *pVmAttr;` |
|  105 |  459 | `					ph7_value *pAttrVal = 0;` |
|    - |  460 | `					ph7_value sHookVal;` |
|    - |  461 | `					sxi32 rcHk;` |
|  105 |  462 | `					pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)aName[iName].zString,aName[iName].nByte);` |
|  105 |  463 | `					if( pAttrEntry == 0 ){` |
|  ! 0 |  464 | `						continue; /* unset by an earlier hook */` |
|    - |  465 | `					}` |
|  105 |  466 | `					pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|  105 |  467 | `					PH7_MemObjInit(pVm,&sHookVal);` |
|  105 |  468 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|  105 |  469 | `					if( rcHk == SXRET_OK ){` |
|   11 |  470 | `						pAttrVal = &sHookVal;` |
|  100 |  471 | `					}else if( rcHk == SXERR_NOTFOUND ){` |
|    - |  472 | `						/* Encode a COPY: the encoder casts scalars in place` |
|    - |  473 | `						 * (ph7_value_to_string), which must not corrupt the` |
|    - |  474 | `						 * live attribute slot. */` |
|   95 |  475 | `						ph7_value *pRaw = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   95 |  476 | `						if( pRaw ){` |
|   95 |  477 | `							PH7_MemObjStore(pRaw,&sHookVal);` |
|   95 |  478 | `							pAttrVal = &sHookVal;` |
|   45 |  479 | `						}` |
|   50 |  480 | `					}else{` |
|    - |  481 | `						/* the get hook threw — propagate like jsonSerialize() */` |
|  ! 0 |  482 | `						PH7_MemObjRelease(&sHookVal);` |
|  ! 0 |  483 | `						SySetRelease(&sNames);` |
|  ! 0 |  484 | `						pData->exc = 1;` |
|  ! 0 |  485 | `						return PH7_EXCEPTION;` |
|    - |  486 | `					}` |
|  105 |  487 | `					if( pAttrVal ){` |
|  105 |  488 | `						VmJsonObjectEncode(&pVmAttr->pAttr->sName,pAttrVal,pData);` |
|   50 |  489 | `					}` |
|  105 |  490 | `					PH7_MemObjRelease(&sHookVal);` |
|  105 |  491 | `					if( pData->exc ){` |
|  ! 0 |  492 | `						SySetRelease(&sNames);` |
|  ! 0 |  493 | `						return PH7_EXCEPTION; /* a nested jsonSerialize()/hook threw */` |
|    - |  494 | `					}` |
|  105 |  495 | `					if( pData->oom ){` |
|  ! 0 |  496 | `						SySetRelease(&sNames);` |
|  ! 0 |  497 | `						return PH7_OK;` |
|    - |  498 | `					}` |
|   55 |  499 | `				}` |
|   71 |  500 | `				SySetRelease(&sNames);` |
|    - |  501 | `				/* Pretty-print: non-empty object closes on its own indented line. */` |
|   71 |  502 | `				if( !pData->isFirst ){` |
|   63 |  503 | `					JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|   29 |  504 | `				}` |
|    - |  505 | `				/* Append the closing curly braces  */` |
|   71 |  506 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|    - |  507 | `			}` |
|   44 |  508 | `		}else{` |
|    - |  509 | `			/* Can't happen */` |
|  ! 0 |  510 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    - |  511 | `		}` |
|    - |  512 | `		/* All done */` |
| 2423 |  513 | `		return PH7_OK;` |
| 1223 |  514 | `}` |
|    - |  515 | `/*` |
|    - |  516 | ` * The following walker callback is invoked each time we need` |
|    - |  517 | ` * to encode an array to JSON.` |
|    - |  518 | ` */` |
| 1502 |  519 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    5 |  520 | `{` |
| 1507 |  521 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
| 1507 |  522 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom \|\| pJson->fail ){` |
|    - |  523 | `		/* Recursion limit reached, a callback threw, OOM, or the value is` |
|    - |  524 | `		 * unencodable (the result is discarded) — return immediately */` |
|    3 |  525 | `		return PH7_OK;` |
|    - |  526 | `	}` |
| 1505 |  527 | `	if( !pJson->isFirst ){` |
|    - |  528 | `		/* Append the comma separating this entry from the previous one */` |
|  882 |  529 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|  439 |  530 | `	}` |
|    - |  531 | `	/* Pretty-print: every member starts on its own indented line (one level` |
|    - |  532 | `	 * deeper than the enclosing container). */` |
| 1505 |  533 | `	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));` |
| 1505 |  534 | `	if( pJson->isObject ){` |
|    - |  535 | `		/* Outputs an object rather than an array */` |
|    - |  536 | `		const char *zKey;` |
|    - |  537 | `		int nByte;` |
|    - |  538 | `		/* Extract a string representation of the key */` |
|  513 |  539 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  540 | `		/* Append the quoted key and the colon. The key goes through the same` |
|    - |  541 | `		 * escaper as a string VALUE (php escapes both identically): emitting it` |
|    - |  542 | `		 * raw produced invalid JSON for any key holding '"', '\' or a control` |
|    - |  543 | `		 * character. */` |
|  513 |  544 | `		JSON_EMIT(pJson,VmJsonEncodeString(pJson,zKey,nByte));` |
|  513 |  545 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,":",(int)sizeof(char)));` |
|    - |  546 | `		/* php puts a space after the colon in pretty mode */` |
|  513 |  547 | `		if( pJson->iFlags & JSON_PRETTY_PRINT ){` |
|   19 |  548 | `			JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));` |
|    9 |  549 | `		}` |
|  255 |  550 | `	}` |
|    - |  551 | `	/* Encode the value */` |
| 1505 |  552 | `	pJson->nRecCount++;` |
| 1505 |  553 | `	VmJsonEncode(pValue,pJson);` |
| 1505 |  554 | `	pJson->nRecCount--;` |
| 1505 |  555 | `	pJson->isFirst = 0;` |
| 1505 |  556 | `	return PH7_OK;` |
|  756 |  557 | `}` |
|    - |  558 | `/*` |
|    - |  559 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  560 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  561 | ` */` |
|  100 |  562 | `static int VmJsonObjectEncode(const SyString *pAttr,ph7_value *pValue,void *pUserData)` |
|    5 |  563 | `{` |
|  105 |  564 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  105 |  565 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom \|\| pJson->fail ){` |
|    - |  566 | `		/* Recursion limit reached, a callback threw, OOM, or the value is` |
|    - |  567 | `		 * unencodable (the result is discarded) — return immediately */` |
|  ! 0 |  568 | `		return PH7_OK;` |
|    - |  569 | `	}` |
|  105 |  570 | `	if( !pJson->isFirst ){` |
|    - |  571 | `		/* Append the comma separating this entry from the previous one */` |
|   45 |  572 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   21 |  573 | `	}` |
|    - |  574 | `	/* Pretty-print: member on its own indented line, one level deeper. */` |
|  105 |  575 | `	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));` |
|    - |  576 | `	/* Append the quoted attribute name and the colon — escaped like a string` |
|    - |  577 | `	 * value, same as the array-key path above. */` |
|  105 |  578 | `	JSON_EMIT(pJson,VmJsonEncodeString(pJson,SyStringData(pAttr),(int)SyStringLength(pAttr)));` |
|  105 |  579 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,":",(int)sizeof(char)));` |
|    - |  580 | `	/* php puts a space after the colon in pretty mode */` |
|  105 |  581 | `	if( pJson->iFlags & JSON_PRETTY_PRINT ){` |
|    9 |  582 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));` |
|    4 |  583 | `	}` |
|    - |  584 | `	/* Encode the value */` |
|  105 |  585 | `	pJson->nRecCount++;` |
|  105 |  586 | `	VmJsonEncode(pValue,pJson);` |
|  105 |  587 | `	pJson->nRecCount--;` |
|  105 |  588 | `	pJson->isFirst = 0;` |
|  105 |  589 | `	return PH7_OK;` |
|   55 |  590 | `}` |
|    - |  591 | `/*` |
|    - |  592 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  593 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  594 | ` * Parameters` |
|    - |  595 | ` *  $value` |
|    - |  596 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  597 | ` * $options` |
|    - |  598 | ` *  Bitmask consisting of:` |
|    - |  599 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  600 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  601 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  602 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  603 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  604 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  605 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  606 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  607 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  608 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  609 | ` * Return` |
|    - |  610 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  611 | ` */` |
|    - |  612 | `static const char * JsonErrorMsg(int rc); /* defined below, near json_last_error_msg */` |
|  818 |  613 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 |  614 | `{` |
|    - |  615 | `	json_private_data sJson;` |
|    - |  616 | `	sxi32 rc;` |
|  823 |  617 | `	if( nArg < 1 ){` |
|    - |  618 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  619 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  620 | `		return PH7_OK;` |
|    - |  621 | `	}` |
|    - |  622 | `	/* Prepare the JSON data */` |
|  823 |  623 | `	sJson.nRecCount = 0;` |
|  823 |  624 | `	sJson.pCtx = pCtx;` |
|  823 |  625 | `	sJson.isFirst = 1;` |
|  823 |  626 | `	sJson.iFlags = 0;` |
|  823 |  627 | `	sJson.exc = 0;` |
|  823 |  628 | `	sJson.oom = 0;` |
|  823 |  629 | `	sJson.fail = 0;` |
|  823 |  630 | `	sJson.failRc = JSON_ERROR_NON_BACKED_ENUM;` |
|  823 |  631 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  632 | `		/* Extract option flags */` |
|   72 |  633 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|   35 |  634 | `	}` |
|  823 |  635 | `	pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  636 | `	/* Perform the encoding operation */` |
|  823 |  637 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|  823 |  638 | `	if( sJson.oom ){` |
|    - |  639 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|    - |  640 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|  ! 0 |  641 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  642 | `	}` |
|  823 |  643 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|    - |  644 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|    5 |  645 | `		return PH7_EXCEPTION;` |
|    - |  646 | `	}` |
|  819 |  647 | `	if( sJson.fail ){` |
|    - |  648 | `		/* Unencodable value (Inf/NaN, or a php 8.1 non-backed enum case): the` |
|    - |  649 | `		 * whole encode fails — discard whatever was emitted and return FALSE. */` |
|   37 |  650 | `		pCtx->pVm->json_rc = sJson.failRc;` |
|   37 |  651 | `		if( sJson.iFlags & JSON_THROW_ON_ERROR ){` |
|    - |  652 | `			/* php: raise a JsonException carrying json_last_error_msg() instead` |
|    - |  653 | `			 * of returning FALSE. */` |
|    7 |  654 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|    4 |  655 | `				(sxi32)pCtx->pVm->json_rc,"%s",` |
|    4 |  656 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|    - |  657 | `		}` |
|   33 |  658 | `		ph7_result_bool(pCtx,0);` |
|   33 |  659 | `		return PH7_OK;` |
|    - |  660 | `	}` |
|    - |  661 | `	/* All done */` |
|  783 |  662 | `	return PH7_OK;` |
|  414 |  663 | `}` |
|    - |  664 | `#undef JSON_EMIT` |
|    - |  665 | `/*` |
|    - |  666 | ` * int json_last_error(void)` |
|    - |  667 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  668 | ` * Parameters` |
|    - |  669 | ` *  None` |
|    - |  670 | ` * Return` |
|    - |  671 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  672 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  673 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  674 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  675 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  676 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  677 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  678 | ` */` |
|  136 |  679 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  680 | `{` |
|  139 |  681 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  682 | `	/* Return the error code */` |
|  139 |  683 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|   68 |  684 | `	SXUNUSED(nArg); /* cc warning */` |
|   68 |  685 | `	SXUNUSED(apArg);` |
|  139 |  686 | `	return PH7_OK;` |
|    3 |  687 | `}` |
|    - |  688 | `/*` |
|    - |  689 | ` * string json_last_error_msg(void)` |
|    - |  690 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|    - |  691 | ` * Parameters` |
|    - |  692 | ` *  None` |
|    - |  693 | ` * Return` |
|    - |  694 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|    - |  695 | ` *  code, or "No error" if no error has occurred.` |
|    - |  696 | ` */` |
|    - |  697 | `/* Human-readable message for a json_rc code. Shared by json_last_error_msg()` |
|    - |  698 | ` * and the JSON_THROW_ON_ERROR path (php's JsonException message is exactly this` |
|    - |  699 | ` * text). */` |
|   64 |  700 | `static const char * JsonErrorMsg(int rc)` |
|    2 |  701 | `{` |
|   66 |  702 | `	switch( rc ){` |
|   31 |  703 | `	case JSON_ERROR_NONE:            return "No error";` |
|  ! 0 |  704 | `	case JSON_ERROR_DEPTH:           return "Maximum stack depth exceeded";` |
|  ! 0 |  705 | `	case JSON_ERROR_STATE_MISMATCH:  return "State mismatch (invalid or malformed JSON)";` |
|    3 |  706 | `	case JSON_ERROR_CTRL_CHAR:       return "Control character error, possibly incorrectly encoded";` |
|   18 |  707 | `	case JSON_ERROR_SYNTAX:          return "Syntax error";` |
|    7 |  708 | `	case JSON_ERROR_UTF8:            return "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|    3 |  709 | `	case JSON_ERROR_INF_OR_NAN:     return "Inf and NaN cannot be JSON encoded";` |
|    9 |  710 | `	case JSON_ERROR_UTF16:           return "Single unpaired UTF-16 surrogate in unicode escape";` |
|  ! 0 |  711 | `	case JSON_ERROR_NON_BACKED_ENUM: return "Non-backed enums have no default serialization";` |
|  ! 0 |  712 | `	default:                         return "Unknown error";` |
|    - |  713 | `	}` |
|   34 |  714 | `}` |
|   54 |  715 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  716 | `{` |
|   56 |  717 | `	ph7_result_string(pCtx,JsonErrorMsg(pCtx->pVm->json_rc),-1/* auto length */);` |
|   27 |  718 | `	SXUNUSED(nArg); /* cc warning */` |
|   27 |  719 | `	SXUNUSED(apArg);` |
|   56 |  720 | `	return PH7_OK;` |
|    2 |  721 | `}` |
|    - |  722 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  723 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  724 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  725 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  726 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  727 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  728 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  729 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  730 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  731 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  732 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  733 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  734 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  735 | `/*` |
|    - |  736 | ` * Tokenize an entire JSON input.` |
|    - |  737 | ` * Get a single low-level token from the input file.` |
|    - |  738 | ` * Update the stream pointer so that it points to the first` |
|    - |  739 | ` * character beyond the extracted token.` |
|    - |  740 | ` */` |
|  516 |  741 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    4 |  742 | `{` |
|  520 |  743 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  744 | `	SyString *pStr;` |
|    - |  745 | `	int c;` |
|    - |  746 | `	/* Ignore leading white spaces */` |
|  582 |  747 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  748 | `		/* Advance the stream cursor */` |
|   64 |  749 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  750 | `			/* Update line counter */` |
|    9 |  751 | `			pStream->nLine++;` |
|    4 |  752 | `		}` |
|   64 |  753 | `		pStream->zText++;` |
|    2 |  754 | `	}` |
|  520 |  755 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  756 | `		/* End of input reached */` |
|   12 |  757 | `		SXUNUSED(pCtxData); /* cc warning */` |
|   26 |  758 | `		return SXERR_EOF;` |
|    - |  759 | `	}` |
|    - |  760 | `	/* Record token starting position and line */` |
|  496 |  761 | `	pToken->nLine = pStream->nLine;` |
|  496 |  762 | `	pToken->pUserData = 0;` |
|  496 |  763 | `	pStr = &pToken->sData;` |
|  496 |  764 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  492 |  765 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|  365 |  766 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  767 | `			/* Single character */` |
|  250 |  768 | `			c = pStream->zText[0];` |
|    - |  769 | `			/* Set token type */` |
|  250 |  770 | `			switch(c){` |
|   38 |  771 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   54 |  772 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|   37 |  773 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|   30 |  774 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   58 |  775 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   52 |  776 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  777 | `			default:` |
|  ! 0 |  778 | `				break;` |
|    - |  779 | `			}` |
|    - |  780 | `			/* Advance the stream cursor */` |
|  250 |  781 | `			pStream->zText++;` |
|  373 |  782 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  783 | `		/* JSON string */` |
|  132 |  784 | `		pStream->zText++;` |
|  132 |  785 | `		pStr->zString++;` |
|    - |  786 | `		/* Delimit the string. The backslash state is tracked explicitly: the old` |
|    - |  787 | `		 * "the previous byte is not a backslash" test mis-read an ESCAPED` |
|    - |  788 | `		 * backslash sitting before the closing quote, so the perfectly valid` |
|    - |  789 | `		 * "\\" (a one-character string holding a backslash) was reported as an` |
|    - |  790 | `		 * unterminated string — json_decode('"\\\\"') answered NULL with a` |
|    - |  791 | `		 * syntax error where php answers "\". */` |
|  572 |  792 | `		while( pStream->zText < pStream->zEnd ){` |
|  570 |  793 | `			if( pStream->zText[0] == '\\' ){` |
|    - |  794 | `				/* Whatever follows belongs to the escape, closing quote` |
|    - |  795 | `				 * included; VmJsonDequoteString below decides if it is legal. */` |
|   81 |  796 | `				pStream->zText++;` |
|   81 |  797 | `				if( pStream->zText >= pStream->zEnd ){` |
|  ! 0 |  798 | `					break;` |
|    - |  799 | `				}` |
|   81 |  800 | `				pStream->zText++;` |
|   81 |  801 | `				continue;` |
|    - |  802 | `			}` |
|  490 |  803 | `			if( pStream->zText[0] == '"' ){` |
|  130 |  804 | `				break;` |
|    - |  805 | `			}` |
|  364 |  806 | `			if( (unsigned char)pStream->zText[0] < 0x20 ){` |
|    - |  807 | `				/* php: a control character must be escaped inside a JSON string;` |
|    - |  808 | `				 * a raw one is JSON_ERROR_CTRL_CHAR (a literal newline included). */` |
|  ! 0 |  809 | `				pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  810 | `				*pJsonErr = JSON_ERROR_CTRL_CHAR;` |
|  ! 0 |  811 | `				return SXERR_ABORT;` |
|    - |  812 | `			}` |
|  364 |  813 | `			pStream->zText++;` |
|    4 |  814 | `		}` |
|  132 |  815 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  816 | `			/* Missing closing '"'. php reports this as JSON_ERROR_CTRL_CHAR, not` |
|    - |  817 | `			 * a syntax error: its scanner runs the string off the end of the` |
|    - |  818 | `			 * input and lands in the same state an unescaped control character` |
|    - |  819 | `			 * puts it in. */` |
|    3 |  820 | `			pToken->nType = JSON_TK_INVALID;` |
|    3 |  821 | `			*pJsonErr = JSON_ERROR_CTRL_CHAR;` |
|    2 |  822 | `		}else{` |
|  130 |  823 | `			pToken->nType = JSON_TK_STR;` |
|  130 |  824 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    - |  825 | `		}` |
|  186 |  826 | `	}else if( (pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]))` |
|   91 |  827 | `		\|\| (pStream->zText[0] == '-' && &pStream->zText[1] < pStream->zEnd` |
|   12 |  828 | `			&& pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1])) ){` |
|    - |  829 | `		/* Number (JSON allows an optional leading minus). Consuming the first` |
|    - |  830 | `		 * character here covers both the '-' and a leading digit; the digit run` |
|    - |  831 | `		 * below then eats the integer part. */` |
|   90 |  832 | `		pStream->zText++;` |
|   90 |  833 | `		pToken->nType = JSON_TK_NUM;` |
|  102 |  834 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|   14 |  835 | `			pStream->zText++;` |
|    2 |  836 | `		}` |
|   90 |  837 | `		if( pStream->zText < pStream->zEnd ){` |
|   86 |  838 | `			c = pStream->zText[0];` |
|   86 |  839 | `			if( c == '.' ){` |
|    - |  840 | `					/* Real number */` |
|    3 |  841 | `					pStream->zText++;` |
|    5 |  842 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    3 |  843 | `						pStream->zText++;` |
|    1 |  844 | `					}` |
|    3 |  845 | `					if( pStream->zText < pStream->zEnd ){` |
|    3 |  846 | `						c = pStream->zText[0];` |
|    3 |  847 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  848 | `							pStream->zText++;` |
|  ! 0 |  849 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  850 | `								c = pStream->zText[0];` |
|  ! 0 |  851 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  852 | `									pStream->zText++;` |
|  ! 0 |  853 | `								}` |
|  ! 0 |  854 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  855 | `									pStream->zText++;` |
|  ! 0 |  856 | `								}` |
|  ! 0 |  857 | `							}` |
|  ! 0 |  858 | `						}` |
|    2 |  859 | `					}` |
|   85 |  860 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  861 | `					/* Real number */` |
|  ! 0 |  862 | `					pStream->zText++;` |
|  ! 0 |  863 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  864 | `						c = pStream->zText[0];` |
|  ! 0 |  865 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  866 | `							pStream->zText++;` |
|  ! 0 |  867 | `						}` |
|  ! 0 |  868 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  869 | `							pStream->zText++;` |
|  ! 0 |  870 | `						}` |
|  ! 0 |  871 | `					}` |
|  ! 0 |  872 | `				}` |
|   45 |  873 | `			}` |
|   85 |  874 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|   16 |  875 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  876 | `			/* boolean true */` |
|    3 |  877 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  878 | `			/* Advance the stream cursor */` |
|    3 |  879 | `			pStream->zText += sizeof("true")-1;` |
|   40 |  880 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|   14 |  881 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  882 | `			/* boolean false */` |
|  ! 0 |  883 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  884 | `			/* Advance the stream cursor */` |
|  ! 0 |  885 | `			pStream->zText += sizeof("false")-1;` |
|   39 |  886 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|   14 |  887 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  888 | `			/* NULL */` |
|  ! 0 |  889 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  890 | `			/* Advance the stream cursor */` |
|  ! 0 |  891 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  892 | `	}else{` |
|    - |  893 | `		/* Unexpected token — but a byte that is not valid UTF-8 is php's` |
|    - |  894 | `		 * JSON_ERROR_UTF8, not a syntax error, wherever in the document it sits` |
|    - |  895 | `		 * (a valid non-ASCII character outside a string stays a syntax error).` |
|    - |  896 | `		 * The JSON_INVALID_UTF8_* flags do NOT reach here: php applies them` |
|    - |  897 | `		 * inside string tokens only. */` |
|    - |  898 | `		sxu32 nLen;` |
|   32 |  899 | `		pToken->nType = JSON_TK_INVALID;` |
|   53 |  900 | `		*pJsonErr = ((unsigned char)pStream->zText[0] >= 0x80` |
|   21 |  901 | `			&& PH7_Utf8ReadStrict((const unsigned char *)pStream->zText,` |
|   18 |  902 | `				(sxu32)(pStream->zEnd - pStream->zText),&nLen) < 0)` |
|   21 |  903 | `			? JSON_ERROR_UTF8 : JSON_ERROR_SYNTAX;` |
|    - |  904 | `		/* Advance the stream cursor */` |
|   32 |  905 | `		pStream->zText++;` |
|    - |  906 | `		/* Abort processing immediatley */` |
|   32 |  907 | `		return SXERR_ABORT;` |
|    - |  908 | `	}` |
|    - |  909 | `	/* record token length */` |
|  466 |  910 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  466 |  911 | `	if( pToken->nType == JSON_TK_STR ){` |
|  130 |  912 | `		pStr->nByte--;` |
|   63 |  913 | `	}` |
|    - |  914 | `	/* Return to the lexer */` |
|  466 |  915 | `	return SXRET_OK;` |
|  262 |  916 | `}` |
|    - |  917 | `/*` |
|    - |  918 | ` * JSON decoded input consumer callback signature.` |
|    - |  919 | ` */` |
|    - |  920 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  921 | `/*` |
|    - |  922 | ` * JSON decoder state is kept in the following structure.` |
|    - |  923 | ` */` |
|    - |  924 | `typedef struct json_decoder json_decoder;` |
|    - |  925 | `struct json_decoder` |
|    - |  926 | `{` |
|    - |  927 | `	ph7_context *pCtx; /* Call context */` |
|    - |  928 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  929 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  930 | `	int iFlags;        /* Configuration flags */` |
|    - |  931 | `	int iUserFlags;    /* json_decode()'s own $flags (JSON_INVALID_UTF8_* live here) */` |
|    - |  932 | `	SyToken *pIn;      /* Token stream */` |
|    - |  933 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  934 | `	int rec_depth;     /* Recursion limit */` |
|    - |  935 | `	int rec_count;     /* Current nesting level */` |
|    - |  936 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  937 | `};` |
|    - |  938 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  939 | `/* Forward declaration */` |
|    - |  940 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  941 | `/*` |
|    - |  942 | ` * Read the four hex digits of a \uXXXX escape out of z[0..n-1]. Returns` |
|    - |  943 | ` * SXRET_OK and the value, or SXERR_SYNTAX when fewer than four are there or one` |
|    - |  944 | ` * is not a hex digit (php: JSON_ERROR_SYNTAX).` |
|    - |  945 | ` */` |
|   52 |  946 | `static sxi32 VmJsonHex4(const char *z,sxu32 n,sxu32 *pVal)` |
|    1 |  947 | `{` |
|   53 |  948 | `	sxu32 v = 0;` |
|    - |  949 | `	int i;` |
|   53 |  950 | `	if( n < 4 ){` |
|    3 |  951 | `		return SXERR_SYNTAX;` |
|    - |  952 | `	}` |
|  243 |  953 | `	for( i = 0 ; i < 4 ; ++i ){` |
|  195 |  954 | `		int c = (unsigned char)z[i];` |
|  195 |  955 | `		if( c >= '0' && c <= '9' ){` |
|  107 |  956 | `			v = (v << 4) \| (sxu32)(c - '0');` |
|  142 |  957 | `		}else if( c >= 'a' && c <= 'f' ){` |
|   79 |  958 | `			v = (v << 4) \| (sxu32)(c - 'a' + 10);` |
|   50 |  959 | `		}else if( c >= 'A' && c <= 'F' ){` |
|    9 |  960 | `			v = (v << 4) \| (sxu32)(c - 'A' + 10);` |
|    5 |  961 | `		}else{` |
|    3 |  962 | `			return SXERR_SYNTAX;` |
|    - |  963 | `		}` |
|   97 |  964 | `	}` |
|   49 |  965 | `	*pVal = v;` |
|   49 |  966 | `	return SXRET_OK;` |
|   27 |  967 | `}` |
|    - |  968 | `/*` |
|    - |  969 | ` * Append one run of un-escaped string bytes, checking that it really is UTF-8:` |
|    - |  970 | ` * php rejects a JSON document carrying a byte that is not text with` |
|    - |  971 | ` * JSON_ERROR_UTF8, exactly as it refuses to ENCODE one. Only inside a string do` |
|    - |  972 | ` * the JSON_INVALID_UTF8_* flags apply — a stray byte between tokens is an error` |
|    - |  973 | ` * either way. Returns JSON_ERROR_NONE or JSON_ERROR_UTF8.` |
|    - |  974 | ` */` |
|  102 |  975 | `static int VmJsonAppendChecked(ph7_value *pWorker,const char *zIn,sxu32 nByte,int iFlags)` |
|    4 |  976 | `{` |
|  106 |  977 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|  106 |  978 | `	sxu32 i = 0,iRun = 0,nLen;` |
|  238 |  979 | `	while( i < nByte ){` |
|  148 |  980 | `		if( z[i] < 0x80 ){` |
|  124 |  981 | `			i++;` |
|  124 |  982 | `			continue;` |
|    - |  983 | `		}` |
|   25 |  984 | `		if( PH7_Utf8ReadStrict(&z[i],nByte - i,&nLen) >= 0 ){` |
|    3 |  985 | `			i += nLen;` |
|    3 |  986 | `			continue;` |
|    - |  987 | `		}` |
|   23 |  988 | `		if( (iFlags & (JSON_INVALID_UTF8_IGNORE\|JSON_INVALID_UTF8_SUBSTITUTE)) == 0 ){` |
|   13 |  989 | `			return JSON_ERROR_UTF8;` |
|    - |  990 | `		}` |
|    - |  991 | `		/* With BOTH flags set php's decoder substitutes while its encoder drops` |
|    - |  992 | `		 * (probed both ways); the order of these two tests is that asymmetry,` |
|    - |  993 | `		 * not an oversight. */` |
|    - |  994 | `		/* Flush what is good, then stand in for the run */` |
|   11 |  995 | `		if( i > iRun ){` |
|   11 |  996 | `			ph7_value_string(pWorker,&zIn[iRun],(int)(i - iRun));` |
|    5 |  997 | `		}` |
|   11 |  998 | `		nLen = VmJsonBadUtf8Len(&z[i],nByte - i);` |
|   11 |  999 | `		if( iFlags & JSON_INVALID_UTF8_SUBSTITUTE ){` |
|    7 | 1000 | `			ph7_value_string(pWorker,"\357\277\275",3); /* U+FFFD */` |
|    3 | 1001 | `		}` |
|   11 | 1002 | `		i += nLen;` |
|   11 | 1003 | `		iRun = i;` |
|    1 | 1004 | `	}` |
|   94 | 1005 | `	if( i > iRun ){` |
|   94 | 1006 | `		ph7_value_string(pWorker,&zIn[iRun],(int)(i - iRun));` |
|   45 | 1007 | `	}` |
|   94 | 1008 | `	return JSON_ERROR_NONE;` |
|   55 | 1009 | `}` |
|    - | 1010 | `/*` |
|    - | 1011 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - | 1012 | ` * the result in the given ph7_value. Returns JSON_ERROR_NONE, or the json_rc` |
|    - | 1013 | ` * php reports for the malformed escape it stopped on.` |
|    - | 1014 | ` *` |
|    - | 1015 | ` * The \uXXXX form used to fall through to the default branch, which dropped the` |
|    - | 1016 | ` * backslash and kept the rest as literal text: json_decode('"é"') answered` |
|    - | 1017 | ` * the five characters u00e9 instead of "é". \b was mangled the same way (it` |
|    - | 1018 | ` * answered "b"), and an escape JSON does not define (\q) was silently accepted` |
|    - | 1019 | ` * where php raises a syntax error.` |
|    - | 1020 | ` */` |
|  126 | 1021 | `static int VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker,int iFlags)` |
|    4 | 1022 | `{` |
|  130 | 1023 | `	const char *zIn = pStr->zString;` |
|  130 | 1024 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - | 1025 | `	const char *zCur;` |
|    - | 1026 | `	int c;` |
|    - | 1027 | `	/* Mark the value as a string */` |
|  130 | 1028 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|   91 | 1029 | `	for(;;){` |
|  186 | 1030 | `		zCur = zIn;` |
|  336 | 1031 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  154 | 1032 | `			zIn++;` |
|    4 | 1033 | `		}` |
|  186 | 1034 | `		if( zIn > zCur ){` |
|  106 | 1035 | `			int rcChunk = VmJsonAppendChecked(pWorker,zCur,(sxu32)(zIn-zCur),iFlags);` |
|  106 | 1036 | `			if( rcChunk != JSON_ERROR_NONE ){` |
|   13 | 1037 | `				return rcChunk;` |
|    - | 1038 | `			}` |
|   45 | 1039 | `		}` |
|  174 | 1040 | `		zIn++;` |
|  174 | 1041 | `		if( zIn >= zEnd ){` |
|    - | 1042 | `			/* End of the input reached */` |
|  104 | 1043 | `			break;` |
|    - | 1044 | `		}` |
|   71 | 1045 | `		c = zIn[0];` |
|    - | 1046 | `		/* Unescape the character */` |
|   71 | 1047 | `		switch(c){` |
|    7 | 1048 | `		case '"':  ph7_value_string(pWorker,"\"",(int)sizeof(char)); break;` |
|    7 | 1049 | `		case '\\': ph7_value_string(pWorker,"\\",(int)sizeof(char)); break;` |
|    3 | 1050 | `		case '/':  ph7_value_string(pWorker,"/",(int)sizeof(char)); break;` |
|    3 | 1051 | `		case 'b':  ph7_value_string(pWorker,"\b",(int)sizeof(char)); break;` |
|    3 | 1052 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|    5 | 1053 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|    3 | 1054 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|    3 | 1055 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|   21 | 1056 | `		case 'u': {` |
|    - | 1057 | `			/* \uXXXX, and the surrogate PAIR that is JSON's only way to spell a` |
|    - | 1058 | `			 * code point above the BMP. An unpaired half is php's` |
|    - | 1059 | `			 * JSON_ERROR_UTF16, distinct from a malformed escape. */` |
|    - | 1060 | `			unsigned char zUtf8[4];` |
|   43 | 1061 | `			unsigned char *zW = zUtf8;` |
|   43 | 1062 | `			sxu32 cp,cpLow = 0; /* cpLow pre-set: MSVC /W4 flags the short-circuit as a maybe-uninitialized read */` |
|   43 | 1063 | `			if( VmJsonHex4(&zIn[1],(sxu32)(zEnd - zIn - 1),&cp) != SXRET_OK ){` |
|    9 | 1064 | `				return JSON_ERROR_SYNTAX;` |
|    - | 1065 | `			}` |
|   39 | 1066 | `			zIn += 4;` |
|   39 | 1067 | `			if( cp >= 0xDC00 && cp <= 0xDFFF ){` |
|    3 | 1068 | `				return JSON_ERROR_UTF16; /* a low half with no high half before it */` |
|    - | 1069 | `			}` |
|   37 | 1070 | `			if( cp >= 0xD800 && cp <= 0xDBFF ){` |
|   14 | 1071 | `				if( zEnd - zIn < 3 \|\| zIn[1] != '\\' \|\| zIn[2] != 'u'` |
|   10 | 1072 | `				 \|\| VmJsonHex4(&zIn[3],(sxu32)(zEnd - zIn - 3),&cpLow) != SXRET_OK` |
|   11 | 1073 | `				 \|\| cpLow < 0xDC00 \|\| cpLow > 0xDFFF ){` |
|    7 | 1074 | `					return JSON_ERROR_UTF16;` |
|    - | 1075 | `				}` |
|    9 | 1076 | `				cp = 0x10000 + ((cp - 0xD800) << 10) + (cpLow - 0xDC00);` |
|    9 | 1077 | `				zIn += 6;` |
|    4 | 1078 | `			}` |
|   31 | 1079 | `			SX_WRITE_UTF8(zW,cp);` |
|   31 | 1080 | `			ph7_value_string(pWorker,(const char *)zUtf8,(int)(zW - zUtf8));` |
|   31 | 1081 | `			break;` |
|    - | 1082 | `		}` |
|    1 | 1083 | `		default:` |
|    - | 1084 | `			/* Not one of JSON's nine escapes */` |
|    3 | 1085 | `			return JSON_ERROR_SYNTAX;` |
|    - | 1086 | `		}` |
|    - | 1087 | `		/* Advance the stream cursor */` |
|   57 | 1088 | `		zIn++;` |
|    1 | 1089 | `	}` |
|  104 | 1090 | `	return JSON_ERROR_NONE;` |
|   67 | 1091 | `}` |
|    - | 1092 | `/*` |
|    - | 1093 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - | 1094 | ` * According to wikipedia` |
|    - | 1095 | ` * JSON's basic types are:` |
|    - | 1096 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - | 1097 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - | 1098 | ` *   Boolean (true or false)` |
|    - | 1099 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - | 1100 | ` *    do not need to be of the same type)` |
|    - | 1101 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - | 1102 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - | 1103 | ` *     be distinct from each other)` |
|    - | 1104 | ` *   null (empty)` |
|    - | 1105 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - | 1106 | ` */` |
|  248 | 1107 | `static sxi32 VmJsonDecode(` |
|    - | 1108 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - | 1109 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    4 | 1110 | `	){` |
|    - | 1111 | `	ph7_value *pWorker; /* Worker variable */` |
|    - | 1112 | `	sxi32 rc;` |
|    - | 1113 | `	int rcQ;            /* VmJsonDequoteString() status */` |
|    - | 1114 | `	/* Nothing left to decode: the token stream is empty (a whitespace-only input` |
|    - | 1115 | `	 * tokenizes to NO tokens at all, so pIn/pEnd are both the NULL base pointer of an` |
|    - | 1116 | `	 * empty set) or a member value is missing after its colon ('{"a":'). Both are a` |
|    - | 1117 | `	 * syntax error for php; without this screen the reads below dereference pEnd. */` |
|  252 | 1118 | `	if( pDecoder->pIn >= pDecoder->pEnd ){` |
|   26 | 1119 | `		*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|   26 | 1120 | `		return SXERR_ABORT;` |
|    - | 1121 | `	}` |
|    - | 1122 | `	/* Check if we do not nest to much */` |
|  228 | 1123 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - | 1124 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 | 1125 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 | 1126 | `		return SXERR_ABORT;` |
|    - | 1127 | `	}` |
|  228 | 1128 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - | 1129 | `		/* Scalar value */` |
|  156 | 1130 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|  156 | 1131 | `		if( pWorker == 0 ){` |
|  ! 0 | 1132 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - | 1133 | `			/* Abort the decoding operation immediately */` |
|  ! 0 | 1134 | `			return SXERR_ABORT;` |
|    - | 1135 | `		}` |
|    - | 1136 | `		/* Reflect the JSON image */` |
|  156 | 1137 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - | 1138 | `			/* Nullify the value.*/` |
|  ! 0 | 1139 | `			ph7_value_null(pWorker);` |
|  156 | 1140 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - | 1141 | `			/* Boolean value */` |
|    3 | 1142 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|  155 | 1143 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   80 | 1144 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - | 1145 | `			/*` |
|    - | 1146 | `			 * Numeric value.` |
|    - | 1147 | `			 * Get a string representation first then try to get a numeric` |
|    - | 1148 | `			 * value.` |
|    - | 1149 | `			 */` |
|   80 | 1150 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - | 1151 | `			/* Obtain a numeric representation */` |
|   80 | 1152 | `			PH7_MemObjToNumeric(pWorker);` |
|   42 | 1153 | `		}else{` |
|    - | 1154 | `			/* Dequote the string */` |
|   76 | 1155 | `			rcQ = VmJsonDequoteString(&pDecoder->pIn->sData,pWorker,pDecoder->iUserFlags);` |
|   76 | 1156 | `			if( rcQ != JSON_ERROR_NONE ){` |
|   25 | 1157 | `				*pDecoder->pErr = rcQ;` |
|   25 | 1158 | `				return SXERR_ABORT;` |
|    - | 1159 | `			}` |
|    - | 1160 | `		}` |
|    - | 1161 | `		/* Invoke the consumer callback */` |
|  132 | 1162 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|  132 | 1163 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 | 1164 | `			return SXERR_ABORT;` |
|    - | 1165 | `		}` |
|    - | 1166 | `		/* All done,advance the stream cursor */` |
|  132 | 1167 | `		pDecoder->pIn++;` |
|  140 | 1168 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - | 1169 | `		ProcJsonConsumer xOld;` |
|    - | 1170 | `		void *pOld;` |
|    - | 1171 | `		/* Array representation*/` |
|   34 | 1172 | `		pDecoder->pIn++;` |
|    - | 1173 | `		/* Create a working array */` |
|   34 | 1174 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   34 | 1175 | `		if( pWorker == 0 ){` |
|  ! 0 | 1176 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - | 1177 | `			/* Abort the decoding operation immediately */` |
|  ! 0 | 1178 | `			return SXERR_ABORT;` |
|    - | 1179 | `		}` |
|    - | 1180 | `		/* Save the old consumer */` |
|   34 | 1181 | `		xOld = pDecoder->xConsumer;` |
|   34 | 1182 | `		pOld = pDecoder->pUserData;` |
|    - | 1183 | `		/* Set the new consumer */` |
|   34 | 1184 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   34 | 1185 | `		pDecoder->pUserData = pWorker;` |
|    - | 1186 | `		/* Decode the array */` |
|   43 | 1187 | `		for(;;){` |
|    - | 1188 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - | 1189 | `			 * do this.` |
|    - | 1190 | `			 */` |
|  122 | 1191 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   36 | 1192 | `				pDecoder->pIn++;` |
|    4 | 1193 | `			}` |
|   90 | 1194 | `			if( pDecoder->pIn >= pDecoder->pEnd ){` |
|    - | 1195 | `				/* Ran out of tokens before the closing ']': php rejects an` |
|    - | 1196 | `				 * unterminated array as a syntax error. */` |
|    8 | 1197 | `				*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|    8 | 1198 | `				return SXERR_ABORT;` |
|    - | 1199 | `			}` |
|   84 | 1200 | `			if( pDecoder->pIn->nType & JSON_TK_CSB /*']'*/ ){` |
|   26 | 1201 | `				pDecoder->pIn++; /* Jump the trailing ']' */` |
|   26 | 1202 | `				break;` |
|    - | 1203 | `			}` |
|    - | 1204 | `			/* Recurse and decode the entry */` |
|   62 | 1205 | `			pDecoder->rec_count++;` |
|   62 | 1206 | `			rc = VmJsonDecode(pDecoder,0);` |
|   62 | 1207 | `			pDecoder->rec_count--;` |
|   62 | 1208 | `			if( rc == SXERR_ABORT ){` |
|    - | 1209 | `				/* Abort processing immediately */` |
|    3 | 1210 | `				return SXERR_ABORT;` |
|    - | 1211 | `			}` |
|    - | 1212 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   60 | 1213 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   54 | 1214 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - | 1215 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 | 1216 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1217 | `					return SXERR_ABORT;` |
|    - | 1218 | `			}` |
|    4 | 1219 | `		}` |
|    - | 1220 | `		/* Restore the old consumer */` |
|   26 | 1221 | `		pDecoder->xConsumer = xOld;` |
|   26 | 1222 | `		pDecoder->pUserData = pOld;` |
|    - | 1223 | `		/* Invoke the old consumer on the decoded array */` |
|   26 | 1224 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   57 | 1225 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - | 1226 | `		ProcJsonConsumer xOld;` |
|    - | 1227 | `		ph7_value *pKey;` |
|    - | 1228 | `		void *pOld;` |
|    - | 1229 | `		/* Object representation*/` |
|   46 | 1230 | `		pDecoder->pIn++;` |
|    - | 1231 | `		/* Decode into a working array first; unless the caller asked for` |
|    - | 1232 | `		 * associative arrays (assoc=true / JSON_OBJECT_AS_ARRAY), it is converted` |
|    - | 1233 | `		 * to a stdClass below so json_decode('{...}') returns an object like php. */` |
|   46 | 1234 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   46 | 1235 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   46 | 1236 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 | 1237 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - | 1238 | `			/* Abort the decoding operation immediately */` |
|  ! 0 | 1239 | `			return SXERR_ABORT;` |
|    - | 1240 | `		}` |
|    - | 1241 | `		/* Save the old consumer */` |
|   46 | 1242 | `		xOld = pDecoder->xConsumer;` |
|   46 | 1243 | `		pOld = pDecoder->pUserData;` |
|    - | 1244 | `		/* Set the new consumer */` |
|   46 | 1245 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   46 | 1246 | `		pDecoder->pUserData = pWorker;` |
|    - | 1247 | `		/* Decode the object */` |
|   43 | 1248 | `		for(;;){` |
|    - | 1249 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - | 1250 | `			 * do this.` |
|    - | 1251 | `			 */` |
|  102 | 1252 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   15 | 1253 | `				pDecoder->pIn++;` |
|    3 | 1254 | `			}` |
|   90 | 1255 | `			if( pDecoder->pIn >= pDecoder->pEnd ){` |
|    - | 1256 | `				/* Ran out of tokens before the closing '}': php rejects an` |
|    - | 1257 | `				 * unterminated object as a syntax error. */` |
|    3 | 1258 | `				*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|    3 | 1259 | `				return SXERR_ABORT;` |
|    - | 1260 | `			}` |
|   88 | 1261 | `			if( pDecoder->pIn->nType & JSON_TK_CCB /*'}'*/ ){` |
|   35 | 1262 | `				pDecoder->pIn++; /* Jump the trailing '}' */` |
|   35 | 1263 | `				break;` |
|    - | 1264 | `			}` |
|   52 | 1265 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   56 | 1266 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - | 1267 | `					/* Syntax error,return immediately */` |
|  ! 0 | 1268 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1269 | `					return SXERR_ABORT;` |
|    - | 1270 | `			}` |
|    - | 1271 | `			/* Dequote the key */` |
|   56 | 1272 | `			rcQ = VmJsonDequoteString(&pDecoder->pIn->sData,pKey,pDecoder->iUserFlags);` |
|   56 | 1273 | `			if( rcQ != JSON_ERROR_NONE ){` |
|    3 | 1274 | `				*pDecoder->pErr = rcQ;` |
|    3 | 1275 | `				return SXERR_ABORT;` |
|    - | 1276 | `			}` |
|    - | 1277 | `			/* Jump the key and the colon */` |
|   54 | 1278 | `			pDecoder->pIn += 2;` |
|    - | 1279 | `			/* Recurse and decode the value */` |
|   54 | 1280 | `			pDecoder->rec_count++;` |
|   54 | 1281 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   54 | 1282 | `			pDecoder->rec_count--;` |
|   54 | 1283 | `			if( rc == SXERR_ABORT ){` |
|    - | 1284 | `				/* Abort processing immediately */` |
|    8 | 1285 | `				return SXERR_ABORT;` |
|    - | 1286 | `			}` |
|    - | 1287 | `			/* Reset the internal buffer of the key */` |
|   47 | 1288 | `			ph7_value_reset_string_cursor(pKey);` |
|    - | 1289 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    3 | 1290 | `		}` |
|    - | 1291 | `		/* Restore the old consumer */` |
|   35 | 1292 | `		pDecoder->xConsumer = xOld;` |
|   35 | 1293 | `		pDecoder->pUserData = pOld;` |
|    - | 1294 | `		/* php returns a stdClass for a JSON object (one dynamic property per member,` |
|    - | 1295 | `		 * nested objects already converted by the recursion) unless assoc was asked. */` |
|   35 | 1296 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|   12 | 1297 | `			PH7_MemObjToObject(pWorker);` |
|    5 | 1298 | `		}` |
|    - | 1299 | `		/* Invoke the old consumer on the decoded object*/` |
|   35 | 1300 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - | 1301 | `		/* Release the key */` |
|   35 | 1302 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|   19 | 1303 | `	}else{` |
|    - | 1304 | `		/* Unexpected token */` |
|  ! 0 | 1305 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - | 1306 | `	}` |
|    - | 1307 | `	/* Release the worker variable */` |
|  186 | 1308 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|  186 | 1309 | `	return SXRET_OK;` |
|  128 | 1310 | `}` |
|    - | 1311 | `/*` |
|    - | 1312 | ` * The following JSON decoder callback is invoked each time` |
|    - | 1313 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - | 1314 | ` * is being decoded.` |
|    - | 1315 | ` */` |
|  100 | 1316 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    4 | 1317 | `{` |
|  104 | 1318 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - | 1319 | `	/* Insert the entry */` |
|  104 | 1320 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   50 | 1321 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - | 1322 | `	/* All done */` |
|  104 | 1323 | `	return SXRET_OK;` |
|    4 | 1324 | `}` |
|    - | 1325 | `/*` |
|    - | 1326 | ` * Standard JSON decoder callback.` |
|    - | 1327 | ` */` |
|   82 | 1328 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    4 | 1329 | `{` |
|    - | 1330 | `	/* Return the value directly */` |
|   86 | 1331 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|   41 | 1332 | `	SXUNUSED(pKey); /* cc warning */` |
|   41 | 1333 | `	SXUNUSED(pUserData);` |
|    - | 1334 | `	/* All done */` |
|   86 | 1335 | `	return SXRET_OK;` |
|    4 | 1336 | `}` |
|    - | 1337 | `/*` |
|    - | 1338 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - | 1339 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - | 1340 | ` * Parameters` |
|    - | 1341 | ` *  $json` |
|    - | 1342 | ` *    The json string being decoded.` |
|    - | 1343 | ` * $assoc` |
|    - | 1344 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - | 1345 | ` * $depth` |
|    - | 1346 | ` *   User specified recursion depth.` |
|    - | 1347 | ` * $options` |
|    - | 1348 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - | 1349 | ` * (default is to cast large integers as floats)` |
|    - | 1350 | ` * Return` |
|    - | 1351 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - | 1352 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - | 1353 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - | 1354 | ` */` |
|    - | 1355 | `/*` |
|    - | 1356 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|    - | 1357 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|    - | 1358 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|    - | 1359 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|    - | 1360 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|    - | 1361 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|    - | 1362 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|    - | 1363 | ` */` |
|  172 | 1364 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth,int iUserFlags)` |
|    4 | 1365 | `{` |
|  176 | 1366 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1367 | `	json_decoder sDecoder;` |
|    - | 1368 | `	SySet sToken;` |
|    - | 1369 | `	SyLex sLex;` |
|    - | 1370 | `	sxi32 rc;` |
|    - | 1371 | `	/* Clear JSON error code */` |
|  176 | 1372 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - | 1373 | `	/* Tokenize the input */` |
|  176 | 1374 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|  176 | 1375 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|  176 | 1376 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|  176 | 1377 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - | 1378 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|   34 | 1379 | `		SyLexRelease(&sLex);` |
|   34 | 1380 | `		SySetRelease(&sToken);` |
|   34 | 1381 | `		return pVm->json_rc;` |
|    - | 1382 | `	}` |
|    - | 1383 | `	/* Fill the decoder */` |
|  144 | 1384 | `	sDecoder.pCtx = pCtx;` |
|  144 | 1385 | `	sDecoder.pErr = &pVm->json_rc;` |
|  144 | 1386 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|  144 | 1387 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|  144 | 1388 | `	sDecoder.iFlags = 0;` |
|  144 | 1389 | `	if( iAssoc ){` |
|    - | 1390 | `		/* Returned objects will be converted into associative arrays */` |
|   63 | 1391 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|   30 | 1392 | `	}` |
|  144 | 1393 | `	sDecoder.iUserFlags = iUserFlags;` |
|  144 | 1394 | `	sDecoder.rec_depth = 32;` |
|  144 | 1395 | `	if( nDepth > 1 && nDepth < 32 ){` |
|    3 | 1396 | `		sDecoder.rec_depth = nDepth;` |
|    1 | 1397 | `	}` |
|  144 | 1398 | `	sDecoder.rec_count = 0;` |
|    - | 1399 | `	/* Set a default consumer */` |
|  144 | 1400 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|  144 | 1401 | `	sDecoder.pUserData = 0;` |
|    - | 1402 | `	/* Decode the raw JSON input */` |
|  144 | 1403 | `	rc = VmJsonDecode(&sDecoder,0);` |
|  144 | 1404 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|    - | 1405 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|  ! 0 | 1406 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1407 | `	}` |
|  144 | 1408 | `	if( pVm->json_rc == JSON_ERROR_NONE && sDecoder.pIn < sDecoder.pEnd ){` |
|    - | 1409 | `		/* php requires the whole input to be ONE JSON value; tokens left after a` |
|    - | 1410 | `		 * complete value (e.g. '"a":1', '{}x', '1 2') are a syntax error. */` |
|    3 | 1411 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    1 | 1412 | `	}` |
|    - | 1413 | `	/* Clean-up the mess left behind */` |
|  144 | 1414 | `	SyLexRelease(&sLex);` |
|  144 | 1415 | `	SySetRelease(&sToken);` |
|  144 | 1416 | `	return pVm->json_rc;` |
|   90 | 1417 | `}` |
|  160 | 1418 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 | 1419 | `{` |
|    - | 1420 | `	const char *zIn;` |
|    - | 1421 | `	int nByte;` |
|  164 | 1422 | `	int iAssoc = 0;` |
|  164 | 1423 | `	int nDepth = 32;` |
|  164 | 1424 | `	int iFlags = 0;` |
|    - | 1425 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|    - | 1426 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|  164 | 1427 | `	if( nArg < 1 ){` |
|    - | 1428 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 | 1429 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1430 | `		return PH7_OK;` |
|    - | 1431 | `	}` |
|  164 | 1432 | `	if( nArg > 3 && ph7_value_is_int(apArg[3]) ){` |
|    - | 1433 | `		/* $flags — only JSON_THROW_ON_ERROR is honored (see NEWPLAN §5). */` |
|   20 | 1434 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|    9 | 1435 | `	}` |
|    - | 1436 | `	/* Extract the JSON string */` |
|  164 | 1437 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  164 | 1438 | `	if( nByte < 1 ){` |
|    - | 1439 | `		/* Empty string: php records a syntax error (json_last_error() == 4) and` |
|    - | 1440 | `		 * returns NULL, or raises a JsonException with JSON_THROW_ON_ERROR. */` |
|    9 | 1441 | `		pCtx->pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    9 | 1442 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|  ! 0 | 1443 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|  ! 0 | 1444 | `				JSON_ERROR_SYNTAX,"%s",JsonErrorMsg(JSON_ERROR_SYNTAX));` |
|    - | 1445 | `		}` |
|    9 | 1446 | `		ph7_result_null(pCtx);` |
|    9 | 1447 | `		return PH7_OK;` |
|    - | 1448 | `	}` |
|  158 | 1449 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|   59 | 1450 | `		iAssoc = 1;` |
|   28 | 1451 | `	}` |
|  158 | 1452 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    - | 1453 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);` |
|    - | 1454 | `		 * read as int64 so a value above INT_MAX is detected, not truncated. */` |
|   32 | 1455 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);` |
|    - | 1456 | `		/* php clears the json error state before validating $depth, so a caught` |
|    - | 1457 | `		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal` |
|    - | 1458 | `		 * path resets it again inside VmJsonDecodeInput). */` |
|   32 | 1459 | `		pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|   32 | 1460 | `		if( nWant <= 0 ){` |
|    9 | 1461 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1462 | `				"json_decode(): Argument #3 ($depth) must be greater than 0");` |
|    - | 1463 | `		}` |
|   24 | 1464 | `		if( nWant > 2147483647 ){` |
|    3 | 1465 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1466 | `				"json_decode(): Argument #3 ($depth) must be less than 2147483647");` |
|    - | 1467 | `		}` |
|   22 | 1468 | `		nDepth = (int)nWant;` |
|   10 | 1469 | `	}` |
|    - | 1470 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|    - | 1471 | `	 * call-context result; on failure we replace it with NULL (or throw). */` |
|  148 | 1472 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth,iFlags) != JSON_ERROR_NONE ){` |
|    - | 1473 | `		/* Something goes wrong while decoding JSON input. */` |
|   77 | 1474 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|    - | 1475 | `			/* php: raise a JsonException carrying json_last_error_msg() text. */` |
|   11 | 1476 | `			return PH7_VmThrowExceptionCode(pCtx,"JsonException",` |
|    6 | 1477 | `				(sxi32)pCtx->pVm->json_rc,"%s",` |
|    6 | 1478 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|    - | 1479 | `		}` |
|   71 | 1480 | `		ph7_result_null(pCtx);` |
|   34 | 1481 | `	}` |
|    - | 1482 | `	/* All done */` |
|  142 | 1483 | `	return PH7_OK;` |
|   84 | 1484 | `}` |
|    - | 1485 | `/*` |
|    - | 1486 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|    - | 1487 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|    - | 1488 | ` * Parameters` |
|    - | 1489 | ` *  $json   The string to validate.` |
|    - | 1490 | ` *  $depth  Maximum nesting depth (clamped to the engine limit of 32).` |
|    - | 1491 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|    - | 1492 | ` * Return` |
|    - | 1493 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|    - | 1494 | ` */` |
|   38 | 1495 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1496 | `{` |
|   40 | 1497 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1498 | `	const char *zIn;` |
|    - | 1499 | `	int nByte;` |
|   40 | 1500 | `	int nDepth = 32;` |
|   40 | 1501 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1502 | `		/* Missing/Invalid argument: not valid JSON */` |
|  ! 0 | 1503 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1504 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1505 | `		return PH7_OK;` |
|    - | 1506 | `	}` |
|    - | 1507 | `	/* Extract the JSON string */` |
|   40 | 1508 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   40 | 1509 | `	if( nByte < 1 ){` |
|    - | 1510 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|    - | 1511 | `		 * silently, json_validate must record the syntax error) */` |
|    6 | 1512 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    6 | 1513 | `		ph7_result_bool(pCtx,0);` |
|    6 | 1514 | `		return PH7_OK;` |
|    - | 1515 | `	}` |
|   36 | 1516 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - | 1517 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */` |
|   11 | 1518 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|    - | 1519 | `		/* Clear the json error state before validating $depth (php parity), so a` |
|    - | 1520 | `		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */` |
|   11 | 1521 | `		pVm->json_rc = JSON_ERROR_NONE;` |
|   11 | 1522 | `		if( nWant <= 0 ){` |
|    5 | 1523 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1524 | `				"json_validate(): Argument #2 ($depth) must be greater than 0");` |
|    - | 1525 | `		}` |
|    7 | 1526 | `		if( nWant > 2147483647 ){` |
|    3 | 1527 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1528 | `				"json_validate(): Argument #2 ($depth) must be less than 2147483647");` |
|    - | 1529 | `		}` |
|    5 | 1530 | `		nDepth = (int)nWant;` |
|    2 | 1531 | `	}` |
|    - | 1532 | `	/* php's only meaningful $flags value here is JSON_INVALID_UTF8_IGNORE, which` |
|    - | 1533 | `	 * makes a payload with undecodable bytes VALID; it rides the same rail as` |
|    - | 1534 | `	 * json_decode's. Decode in associative mode so the "objects are returned as an` |
|    - | 1535 | `	 * array" warning is not raised - the decoded value is discarded, only its` |
|    - | 1536 | `	 * validity matters. */` |
|   59 | 1537 | `	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth,` |
|   15 | 1538 | `		(nArg > 2 && ph7_value_is_int(apArg[2])) ? ph7_value_to_int(apArg[2]) : 0)` |
|   14 | 1539 | `		== JSON_ERROR_NONE);` |
|   30 | 1540 | `	return PH7_OK;` |
|   21 | 1541 | `}` |
|    - | 1542 |  |
