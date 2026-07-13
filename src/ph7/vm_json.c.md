# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 443/572 lines (77.45%)

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
|    - |   15 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);` |
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
|    - |   30 | `};` |
|    - |   31 | `/*` |
|    - |   32 | ` * Emit into the JSON result, flagging OOM on the shared data and bailing out` |
|    - |   33 | ` * of the current encode function (which returns PH7_OK; the top-level` |
|    - |   34 | ` * vm_builtin_json_encode checks ->oom and raises a non-catchable fatal). Used` |
|    - |   35 | ` * for every ph7_result_string/ph7_result_string_format append below.` |
|    - |   36 | ` */` |
|    - |   37 | `#define JSON_EMIT(pD, call) do { if( (call) != SXRET_OK ){ (pD)->oom = 1; return PH7_OK; } } while(0)` |
|    - |   38 | `/*` |
|    - |   39 | ` * Emit a float in php's json shape: PH7_AppendShortestReal (the shared` |
|    - |   40 | ` * serialize/var_export shortest-round-trip formatter, php's` |
|    - |   41 | ` * serialize_precision=-1) with the exponent marker lowercased (json prints` |
|    - |   42 | ` * 1.0e+17 where serialize prints 1.0E+17).` |
|    - |   43 | ` */` |
|   20 |   44 | `static sxi32 VmJsonEmitReal(ph7_context *pCtx,double rVal)` |
|    1 |   45 | `{` |
|    - |   46 | `	SyBlob sNum;` |
|    - |   47 | `	char *z;` |
|    - |   48 | `	sxu32 i,n;` |
|    - |   49 | `	sxi32 rc;` |
|   21 |   50 | `	SyBlobInit(&sNum,&pCtx->pVm->sAllocator);` |
|   21 |   51 | `	PH7_AppendShortestReal(&sNum,rVal);` |
|   21 |   52 | `	z = (char *)SyBlobData(&sNum);` |
|   21 |   53 | `	n = SyBlobLength(&sNum);` |
|   21 |   54 | `	if( z == 0 \|\| n < 1 ){` |
|  ! 0 |   55 | `		SyBlobRelease(&sNum);` |
|  ! 0 |   56 | `		return SXERR_MEM; /* treated as OOM by JSON_EMIT */` |
|    - |   57 | `	}` |
|  217 |   58 | `	for( i = 0 ; i < n ; i++ ){` |
|  197 |   59 | `		if( z[i] == 'E' ){` |
|    7 |   60 | `			z[i] = 'e';` |
|    3 |   61 | `		}` |
|   99 |   62 | `	}` |
|   21 |   63 | `	rc = ph7_result_string(pCtx,(const char *)z,(int)n);` |
|   21 |   64 | `	SyBlobRelease(&sNum);` |
|   21 |   65 | `	return rc;` |
|   11 |   66 | `}` |
|    - |   67 | `/*` |
|    - |   68 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|    - |   69 | ` * According to wikipedia` |
|    - |   70 | ` * JSON's basic types are:` |
|    - |   71 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |   72 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |   73 | ` *   Boolean (true or false)` |
|    - |   74 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |   75 | ` *    do not need to be of the same type)` |
|    - |   76 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |   77 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |   78 | ` *     be distinct from each other)` |
|    - |   79 | ` *   null (empty)` |
|    - |   80 | ` * Non-significant white space may be added freely around the "structural characters"` |
|    - |   81 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |   82 | ` */` |
|  554 |   83 | `static sxi32 VmJsonEncode(` |
|    - |   84 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |   85 | `	json_private_data *pData /* Context data */` |
|    2 |   86 | `	){` |
|  556 |   87 | `		ph7_context *pCtx = pData->pCtx;` |
|  556 |   88 | `		int iFlags = pData->iFlags;` |
|    - |   89 | `		int nByte;` |
|  556 |   90 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |   91 | `			/* null */` |
|    5 |   92 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|  553 |   93 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   15 |   94 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |   95 | `			int iLen;` |
|    - |   96 | `			/* true/false */` |
|   15 |   97 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   15 |   98 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
|  544 |   99 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|  244 |  100 | `			if( ph7_value_is_float(pIn) ){` |
|    - |  101 | `				/* php's json float output follows serialize_precision` |
|    - |  102 | `				 * (shortest round-trip, like serialize/var_export), NOT the` |
|    - |  103 | `				 * echo/cast precision of 14 — with a lowercase exponent` |
|    - |  104 | `				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,` |
|    - |  105 | `				 * 1.0 -> 1, -0.0 -> -0. */` |
|   17 |  106 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    9 |  107 | `			}else{` |
|    - |  108 | `				const char *zNum;` |
|    - |  109 | `				/* Get a string representation of the number */` |
|  147 |  110 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|  147 |  111 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|    1 |  112 | `			}` |
|  457 |  113 | `		}else if( ph7_value_is_string(pIn) ){` |
|  162 |  114 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |  115 | `				/* Encodes numeric strings as numbers (same float shapes). */` |
|    5 |  116 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    5 |  117 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    3 |  118 | `			}else{` |
|    - |  119 | `				const char *zIn,*zEnd;` |
|    - |  120 | `				int c;` |
|    - |  121 | `				/* Encode the string */` |
|  158 |  122 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|  158 |  123 | `				zEnd = &zIn[nByte];` |
|    - |  124 | `				/* Append the double quote */` |
|  158 |  125 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|  274 |  126 | `				for(;;){` |
|  556 |  127 | `					if( zIn >= zEnd ){` |
|    - |  128 | `						/* No more input to process */` |
|  158 |  129 | `						break;` |
|    - |  130 | `					}` |
|  400 |  131 | `					c = zIn[0];` |
|    - |  132 | `					/* Advance the stream cursor */` |
|  400 |  133 | `					zIn++;` |
|  400 |  134 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |  135 | `						/* All < and > are converted to \u003C and \u003E */` |
|  ! 0 |  136 | `						if( c == '<' ){` |
|  ! 0 |  137 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1));` |
|  ! 0 |  138 | `						}else{` |
|  ! 0 |  139 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1));` |
|    - |  140 | `						}` |
|  ! 0 |  141 | `						continue;` |
|  400 |  142 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |  143 | `						/* All &s are converted to \u0026.  */` |
|  ! 0 |  144 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1));` |
|  ! 0 |  145 | `						continue;` |
|  400 |  146 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  147 | `						/* All ' are converted to \u0027.   */` |
|  ! 0 |  148 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1));` |
|  ! 0 |  149 | `						continue;` |
|  400 |  150 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  151 | `						/* All " are converted to \u0022. */` |
|  ! 0 |  152 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1));` |
|  ! 0 |  153 | `						continue;` |
|    - |  154 | `					}` |
|  400 |  155 | `					if( c == '"' \|\| c == '\\' ){` |
|    - |  156 | `						/* Escape the quote/backslash (php escapes the backslash` |
|    - |  157 | `						 * unconditionally — the old code wrongly tied it to` |
|    - |  158 | `						 * JSON_UNESCAPED_SLASHES, which governs '/' below) */` |
|    3 |  159 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  398 |  160 | `					}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){` |
|    - |  161 | `						/* php escapes forward slashes by default */` |
|    7 |  162 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  394 |  163 | `					}else if( (unsigned char)c < 0x20 ){` |
|    - |  164 | `						/* Control characters (band A #4): php emits the short` |
|    - |  165 | `						 * escapes for \b \f \n \r \t and \u00xx for the rest —` |
|    - |  166 | `						 * pre-fix these were emitted RAW (invalid JSON). */` |
|    - |  167 | `						static const char zHex[] = "0123456789abcdef";` |
|    7 |  168 | `						char zEsc[6] = { '\\', 'u', '0', '0', 0, 0 };` |
|    7 |  169 | `						switch(c){` |
|  ! 0 |  170 | `						case '\b': JSON_EMIT(pData,ph7_result_string(pCtx,"\\b",2)); break;` |
|  ! 0 |  171 | `						case '\f': JSON_EMIT(pData,ph7_result_string(pCtx,"\\f",2)); break;` |
|    3 |  172 | `						case '\n': JSON_EMIT(pData,ph7_result_string(pCtx,"\\n",2)); break;` |
|  ! 0 |  173 | `						case '\r': JSON_EMIT(pData,ph7_result_string(pCtx,"\\r",2)); break;` |
|    3 |  174 | `						case '\t': JSON_EMIT(pData,ph7_result_string(pCtx,"\\t",2)); break;` |
|    1 |  175 | `						default:` |
|    3 |  176 | `							zEsc[4] = zHex[(c >> 4) & 0x0F];` |
|    3 |  177 | `							zEsc[5] = zHex[c & 0x0F];` |
|    3 |  178 | `							JSON_EMIT(pData,ph7_result_string(pCtx,zEsc,6));` |
|    2 |  179 | `							break;` |
|    - |  180 | `						}` |
|    7 |  181 | `						continue;` |
|    - |  182 | `					}` |
|    - |  183 | `					/* Append character verbatim */` |
|  394 |  184 | `					JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    2 |  185 | `				}` |
|    - |  186 | `				/* Append the double quote */` |
|  158 |  187 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|    2 |  188 | `			}` |
|  296 |  189 | `		}else if( ph7_value_is_array(pIn) ){` |
|    - |  190 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|    - |  191 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|    - |  192 | `			 * object with stringified keys (PHP semantics). */` |
|  320 |  193 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|  160 |  194 | `				\|\| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);` |
|  161 |  195 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|  161 |  196 | `			int c = isObject ? '{' : '[';` |
|  161 |  197 | `			int d = isObject ? '}' : ']';` |
|    - |  198 | `			/* Encode the array */` |
|  161 |  199 | `			pData->isObject = isObject;` |
|  161 |  200 | `			pData->isFirst = 1;` |
|    - |  201 | `			/* Append the square bracket or curly braces */` |
|  161 |  202 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    - |  203 | `			/* Iterate throw array entries */` |
|  161 |  204 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  205 | `			/* Bail if a nested append ran out of memory before the closer */` |
|  161 |  206 | `			if( pData->oom ){` |
|  ! 0 |  207 | `				return PH7_OK;` |
|    - |  208 | `			}` |
|    - |  209 | `			/* Append the closing square bracket or curly braces */` |
|  161 |  210 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|  161 |  211 | `			pData->isObject = savedObject;` |
|  136 |  212 | `		}else if( ph7_value_is_object(pIn) ){` |
|   56 |  213 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   56 |  214 | `			ph7_vm *pVm = pIn->pVm;` |
|   56 |  215 | `			ph7_class_method *pMethod = 0;` |
|    - |  216 | `			/* If the object implements JsonSerializable, encode the value` |
|    - |  217 | `			 * returned by jsonSerialize() instead of its public properties. */` |
|   54 |  218 | `			if( pVm->pJsonSerializableClass` |
|   56 |  219 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|   15 |  220 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|    7 |  221 | `			}` |
|   56 |  222 | `			if( pMethod ){` |
|    - |  223 | `				ph7_value sResult;` |
|    - |  224 | `				sxi32 rc;` |
|   15 |  225 | `				PH7_MemObjInit(pVm,&sResult);` |
|   15 |  226 | `				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|   15 |  227 | `				if( rc == PH7_EXCEPTION ){` |
|    - |  228 | `					/* Let jsonSerialize()'s throw propagate */` |
|    5 |  229 | `					PH7_MemObjRelease(&sResult);` |
|    5 |  230 | `					pData->exc = 1;` |
|    5 |  231 | `					return PH7_EXCEPTION;` |
|    - |  232 | `				}` |
|    - |  233 | `				/* Encode the returned value [scalar/array/object] */` |
|   11 |  234 | `				pData->nRecCount++;` |
|   11 |  235 | `				VmJsonEncode(&sResult,pData);` |
|   11 |  236 | `				pData->nRecCount--;` |
|   11 |  237 | `				PH7_MemObjRelease(&sResult);` |
|   11 |  238 | `				if( pData->exc ){` |
|  ! 0 |  239 | `					return PH7_EXCEPTION;` |
|    - |  240 | `				}` |
|   11 |  241 | `				if( pData->oom ){` |
|  ! 0 |  242 | `					return PH7_OK;` |
|    - |  243 | `				}` |
|    6 |  244 | `			}else{` |
|    - |  245 | `				/* Encode the class instance */` |
|   42 |  246 | `				pData->isFirst = 1;` |
|    - |  247 | `				/* Append the curly braces */` |
|   42 |  248 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|    - |  249 | `				/* Iterate throw class attribute */` |
|   42 |  250 | `				ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|   42 |  251 | `				if( pData->oom ){` |
|  ! 0 |  252 | `					return PH7_OK;` |
|    - |  253 | `				}` |
|    - |  254 | `				/* Append the closing curly braces  */` |
|   42 |  255 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|    - |  256 | `			}` |
|   27 |  257 | `		}else{` |
|    - |  258 | `			/* Can't happen */` |
|  ! 0 |  259 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    - |  260 | `		}` |
|    - |  261 | `		/* All done */` |
|  552 |  262 | `		return PH7_OK;` |
|  279 |  263 | `}` |
|    - |  264 | `/*` |
|    - |  265 | ` * The following walker callback is invoked each time we need` |
|    - |  266 | ` * to encode an array to JSON.` |
|    - |  267 | ` */` |
|  292 |  268 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    1 |  269 | `{` |
|  293 |  270 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  293 |  271 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  272 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  273 | `		return PH7_OK;` |
|    - |  274 | `	}` |
|  293 |  275 | `	if( !pJson->isFirst ){` |
|    - |  276 | `		/* Append the colon first */` |
|  153 |  277 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   76 |  278 | `	}` |
|  293 |  279 | `	if( pJson->isObject ){` |
|    - |  280 | `		/* Outputs an object rather than an array */` |
|    - |  281 | `		const char *zKey;` |
|    - |  282 | `		int nByte;` |
|    - |  283 | `		/* Extract a string representation of the key */` |
|  153 |  284 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  285 | `		/* Append the quoted key and the colon (checked, so an OOM here is caught` |
|    - |  286 | `		 * rather than silently truncating; matches the prior "%.*s" emit byte for` |
|    - |  287 | `		 * byte — keys are not JSON-escaped, a pre-existing behavior). */` |
|  153 |  288 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|  153 |  289 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zKey,nByte));` |
|  153 |  290 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|   76 |  291 | `	}` |
|    - |  292 | `	/* Encode the value */` |
|  293 |  293 | `	pJson->nRecCount++;` |
|  293 |  294 | `	VmJsonEncode(pValue,pJson);` |
|  293 |  295 | `	pJson->nRecCount--;` |
|  293 |  296 | `	pJson->isFirst = 0;` |
|  293 |  297 | `	return PH7_OK;` |
|  147 |  298 | `}` |
|    - |  299 | `/*` |
|    - |  300 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  301 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  302 | ` */` |
|   62 |  303 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|    2 |  304 | `{` |
|   64 |  305 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|   64 |  306 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  307 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  308 | `		return PH7_OK;` |
|    - |  309 | `	}` |
|   64 |  310 | `	if( !pJson->isFirst ){` |
|    - |  311 | `		/* Append the colon first */` |
|   25 |  312 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   12 |  313 | `	}` |
|    - |  314 | `	/* Append the quoted attribute name and the colon (checked; matches the prior` |
|    - |  315 | `	 * "%s" emit byte for byte — attribute names are not JSON-escaped). */` |
|   64 |  316 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|   64 |  317 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zAttr,-1));` |
|   64 |  318 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|    - |  319 | `	/* Encode the value */` |
|   64 |  320 | `	pJson->nRecCount++;` |
|   64 |  321 | `	VmJsonEncode(pValue,pJson);` |
|   64 |  322 | `	pJson->nRecCount--;` |
|   64 |  323 | `	pJson->isFirst = 0;` |
|   64 |  324 | `	return PH7_OK;` |
|   33 |  325 | `}` |
|    - |  326 | `/*` |
|    - |  327 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  328 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  329 | ` * Parameters` |
|    - |  330 | ` *  $value` |
|    - |  331 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  332 | ` * $options` |
|    - |  333 | ` *  Bitmask consisting of:` |
|    - |  334 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  335 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  336 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  337 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  338 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  339 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  340 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  341 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  342 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  343 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  344 | ` * Return` |
|    - |  345 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  346 | ` */` |
|  190 |  347 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  348 | `{` |
|    - |  349 | `	json_private_data sJson;` |
|    - |  350 | `	sxi32 rc;` |
|  192 |  351 | `	if( nArg < 1 ){` |
|    - |  352 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  353 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  354 | `		return PH7_OK;` |
|    - |  355 | `	}` |
|    - |  356 | `	/* Prepare the JSON data */` |
|  192 |  357 | `	sJson.nRecCount = 0;` |
|  192 |  358 | `	sJson.pCtx = pCtx;` |
|  192 |  359 | `	sJson.isFirst = 1;` |
|  192 |  360 | `	sJson.iFlags = 0;` |
|  192 |  361 | `	sJson.exc = 0;` |
|  192 |  362 | `	sJson.oom = 0;` |
|  192 |  363 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  364 | `		/* Extract option flags */` |
|    7 |  365 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|    3 |  366 | `	}` |
|    - |  367 | `	/* Perform the encoding operation */` |
|  192 |  368 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|  192 |  369 | `	if( sJson.oom ){` |
|    - |  370 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|    - |  371 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|  ! 0 |  372 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  373 | `	}` |
|  192 |  374 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|    - |  375 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|    5 |  376 | `		return PH7_EXCEPTION;` |
|    - |  377 | `	}` |
|    - |  378 | `	/* All done */` |
|  188 |  379 | `	return PH7_OK;` |
|   97 |  380 | `}` |
|    - |  381 | `#undef JSON_EMIT` |
|    - |  382 | `/*` |
|    - |  383 | ` * int json_last_error(void)` |
|    - |  384 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  385 | ` * Parameters` |
|    - |  386 | ` *  None` |
|    - |  387 | ` * Return` |
|    - |  388 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  389 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  390 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  391 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  392 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  393 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  394 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  395 | ` */` |
|   12 |  396 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  397 | `{` |
|   14 |  398 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  399 | `	/* Return the error code */` |
|   14 |  400 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    6 |  401 | `	SXUNUSED(nArg); /* cc warning */` |
|    6 |  402 | `	SXUNUSED(apArg);` |
|   14 |  403 | `	return PH7_OK;` |
|    2 |  404 | `}` |
|    - |  405 | `/*` |
|    - |  406 | ` * string json_last_error_msg(void)` |
|    - |  407 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|    - |  408 | ` * Parameters` |
|    - |  409 | ` *  None` |
|    - |  410 | ` * Return` |
|    - |  411 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|    - |  412 | ` *  code, or "No error" if no error has occurred.` |
|    - |  413 | ` */` |
|    4 |  414 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  415 | `{` |
|    5 |  416 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  417 | `	const char *zMsg;` |
|    5 |  418 | `	switch( pVm->json_rc ){` |
|    1 |  419 | `	case JSON_ERROR_NONE:` |
|    3 |  420 | `		zMsg = "No error";` |
|    3 |  421 | `		break;` |
|  ! 0 |  422 | `	case JSON_ERROR_DEPTH:` |
|  ! 0 |  423 | `		zMsg = "Maximum stack depth exceeded";` |
|  ! 0 |  424 | `		break;` |
|  ! 0 |  425 | `	case JSON_ERROR_STATE_MISMATCH:` |
|  ! 0 |  426 | `		zMsg = "State mismatch (invalid or malformed JSON)";` |
|  ! 0 |  427 | `		break;` |
|  ! 0 |  428 | `	case JSON_ERROR_CTRL_CHAR:` |
|  ! 0 |  429 | `		zMsg = "Control character error, possibly incorrectly encoded";` |
|  ! 0 |  430 | `		break;` |
|    1 |  431 | `	case JSON_ERROR_SYNTAX:` |
|    3 |  432 | `		zMsg = "Syntax error";` |
|    3 |  433 | `		break;` |
|  ! 0 |  434 | `	case JSON_ERROR_UTF8:` |
|  ! 0 |  435 | `		zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|  ! 0 |  436 | `		break;` |
|  ! 0 |  437 | `	default:` |
|  ! 0 |  438 | `		zMsg = "Unknown error";` |
|  ! 0 |  439 | `		break;` |
|    - |  440 | `	}` |
|    5 |  441 | `	ph7_result_string(pCtx,zMsg,-1/* Compute length automatically */);` |
|    2 |  442 | `	SXUNUSED(nArg); /* cc warning */` |
|    2 |  443 | `	SXUNUSED(apArg);` |
|    5 |  444 | `	return PH7_OK;` |
|    1 |  445 | `}` |
|    - |  446 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  447 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  448 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  449 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  450 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  451 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  452 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  453 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  454 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  455 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  456 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  457 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  458 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  459 | `/*` |
|    - |  460 | ` * Tokenize an entire JSON input.` |
|    - |  461 | ` * Get a single low-level token from the input file.` |
|    - |  462 | ` * Update the stream pointer so that it points to the first` |
|    - |  463 | ` * character beyond the extracted token.` |
|    - |  464 | ` */` |
|  160 |  465 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    2 |  466 | `{` |
|  162 |  467 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  468 | `	SyString *pStr;` |
|    - |  469 | `	int c;` |
|    - |  470 | `	/* Ignore leading white spaces */` |
|  166 |  471 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  472 | `		/* Advance the stream cursor */` |
|    6 |  473 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  474 | `			/* Update line counter */` |
|  ! 0 |  475 | `			pStream->nLine++;` |
|  ! 0 |  476 | `		}` |
|    6 |  477 | `		pStream->zText++;` |
|    2 |  478 | `	}` |
|  162 |  479 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  480 | `		/* End of input reached */` |
|  ! 0 |  481 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  482 | `		return SXERR_EOF;` |
|    - |  483 | `	}` |
|    - |  484 | `	/* Record token starting position and line */` |
|  162 |  485 | `	pToken->nLine = pStream->nLine;` |
|  162 |  486 | `	pToken->pUserData = 0;` |
|  162 |  487 | `	pStr = &pToken->sData;` |
|  162 |  488 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  160 |  489 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|  110 |  490 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  491 | `			/* Single character */` |
|   94 |  492 | `			c = pStream->zText[0];` |
|    - |  493 | `			/* Set token type */` |
|   94 |  494 | `			switch(c){` |
|   13 |  495 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   22 |  496 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|   16 |  497 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|   13 |  498 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   20 |  499 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   17 |  500 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  501 | `			default:` |
|  ! 0 |  502 | `				break;` |
|    - |  503 | `			}` |
|    - |  504 | `			/* Advance the stream cursor */` |
|   94 |  505 | `			pStream->zText++;` |
|  116 |  506 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  507 | `		/* JSON string */` |
|   26 |  508 | `		pStream->zText++;` |
|   26 |  509 | `		pStr->zString++;` |
|    - |  510 | `		/* Delimit the string */` |
|   72 |  511 | `		while( pStream->zText < pStream->zEnd ){` |
|   72 |  512 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   26 |  513 | `				break;` |
|    - |  514 | `			}` |
|   48 |  515 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  516 | `				/* Update line counter */` |
|  ! 0 |  517 | `				pStream->nLine++;` |
|  ! 0 |  518 | `			}` |
|   48 |  519 | `			pStream->zText++;` |
|    2 |  520 | `		}` |
|   26 |  521 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  522 | `			/* Missing closing '"' */` |
|  ! 0 |  523 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  524 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  525 | `		}else{` |
|   26 |  526 | `			pToken->nType = JSON_TK_STR;` |
|   26 |  527 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    2 |  528 | `		}` |
|   58 |  529 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    - |  530 | `		/* Number */` |
|   31 |  531 | `		pStream->zText++;` |
|   31 |  532 | `		pToken->nType = JSON_TK_NUM;` |
|   31 |  533 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  534 | `			pStream->zText++;` |
|  ! 0 |  535 | `		}` |
|   31 |  536 | `		if( pStream->zText < pStream->zEnd ){` |
|   31 |  537 | `			c = pStream->zText[0];` |
|   31 |  538 | `			if( c == '.' ){` |
|    - |  539 | `					/* Real number */` |
|  ! 0 |  540 | `					pStream->zText++;` |
|  ! 0 |  541 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  542 | `						pStream->zText++;` |
|  ! 0 |  543 | `					}` |
|  ! 0 |  544 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  545 | `						c = pStream->zText[0];` |
|  ! 0 |  546 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  547 | `							pStream->zText++;` |
|  ! 0 |  548 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  549 | `								c = pStream->zText[0];` |
|  ! 0 |  550 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  551 | `									pStream->zText++;` |
|  ! 0 |  552 | `								}` |
|  ! 0 |  553 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  554 | `									pStream->zText++;` |
|  ! 0 |  555 | `								}` |
|  ! 0 |  556 | `							}` |
|  ! 0 |  557 | `						}` |
|  ! 0 |  558 | `					}` |
|   31 |  559 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  560 | `					/* Real number */` |
|  ! 0 |  561 | `					pStream->zText++;` |
|  ! 0 |  562 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  563 | `						c = pStream->zText[0];` |
|  ! 0 |  564 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  565 | `							pStream->zText++;` |
|  ! 0 |  566 | `						}` |
|  ! 0 |  567 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  568 | `							pStream->zText++;` |
|  ! 0 |  569 | `						}` |
|  ! 0 |  570 | `					}` |
|  ! 0 |  571 | `				}` |
|   16 |  572 | `			}` |
|   37 |  573 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|   12 |  574 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  575 | `			/* boolean true */` |
|  ! 0 |  576 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  577 | `			/* Advance the stream cursor */` |
|  ! 0 |  578 | `			pStream->zText += sizeof("true")-1;` |
|   22 |  579 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|   12 |  580 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  581 | `			/* boolean false */` |
|  ! 0 |  582 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  583 | `			/* Advance the stream cursor */` |
|  ! 0 |  584 | `			pStream->zText += sizeof("false")-1;` |
|   22 |  585 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|   12 |  586 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  587 | `			/* NULL */` |
|  ! 0 |  588 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  589 | `			/* Advance the stream cursor */` |
|  ! 0 |  590 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  591 | `	}else{` |
|    - |  592 | `		/* Unexpected token */` |
|   16 |  593 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  594 | `		/* Advance the stream cursor */` |
|   16 |  595 | `		pStream->zText++;` |
|   16 |  596 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  597 | `		/* Abort processing immediatley */` |
|   16 |  598 | `		return SXERR_ABORT;` |
|    - |  599 | `	}` |
|    - |  600 | `	/* record token length */` |
|  148 |  601 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  148 |  602 | `	if( pToken->nType == JSON_TK_STR ){` |
|   26 |  603 | `		pStr->nByte--;` |
|   12 |  604 | `	}` |
|    - |  605 | `	/* Return to the lexer */` |
|  148 |  606 | `	return SXRET_OK;` |
|   82 |  607 | `}` |
|    - |  608 | `/*` |
|    - |  609 | ` * JSON decoded input consumer callback signature.` |
|    - |  610 | ` */` |
|    - |  611 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  612 | `/*` |
|    - |  613 | ` * JSON decoder state is kept in the following structure.` |
|    - |  614 | ` */` |
|    - |  615 | `typedef struct json_decoder json_decoder;` |
|    - |  616 | `struct json_decoder` |
|    - |  617 | `{` |
|    - |  618 | `	ph7_context *pCtx; /* Call context */` |
|    - |  619 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  620 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  621 | `	int iFlags;        /* Configuration flags */` |
|    - |  622 | `	SyToken *pIn;      /* Token stream */` |
|    - |  623 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  624 | `	int rec_depth;     /* Recursion limit */` |
|    - |  625 | `	int rec_count;     /* Current nesting level */` |
|    - |  626 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  627 | `};` |
|    - |  628 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  629 | `/* Forward declaration */` |
|    - |  630 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  631 | `/*` |
|    - |  632 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  633 | ` * the result in the given ph7_value.` |
|    - |  634 | ` */` |
|   24 |  635 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    2 |  636 | `{` |
|   26 |  637 | `	const char *zIn = pStr->zString;` |
|   26 |  638 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  639 | `	const char *zCur;` |
|    - |  640 | `	int c;` |
|    - |  641 | `	/* Mark the value as a string */` |
|   26 |  642 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|   12 |  643 | `	for(;;){` |
|   26 |  644 | `		zCur = zIn;` |
|   72 |  645 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   48 |  646 | `			zIn++;` |
|    2 |  647 | `		}` |
|   26 |  648 | `		if( zIn > zCur ){` |
|    - |  649 | `			/* Append chunk verbatim */` |
|   26 |  650 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|   12 |  651 | `		}` |
|   26 |  652 | `		zIn++;` |
|   26 |  653 | `		if( zIn >= zEnd ){` |
|    - |  654 | `			/* End of the input reached */` |
|   26 |  655 | `			break;` |
|    - |  656 | `		}` |
|  ! 0 |  657 | `		c = zIn[0];` |
|    - |  658 | `		/* Unescape the character */` |
|  ! 0 |  659 | `		switch(c){` |
|  ! 0 |  660 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  661 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  662 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  663 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  664 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  665 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  666 | `		default:` |
|  ! 0 |  667 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  668 | `			break;` |
|    - |  669 | `		}` |
|    - |  670 | `		/* Advance the stream cursor */` |
|  ! 0 |  671 | `		zIn++;` |
|  ! 0 |  672 | `	}` |
|   26 |  673 | `}` |
|    - |  674 | `/*` |
|    - |  675 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  676 | ` * According to wikipedia` |
|    - |  677 | ` * JSON's basic types are:` |
|    - |  678 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  679 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  680 | ` *   Boolean (true or false)` |
|    - |  681 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  682 | ` *    do not need to be of the same type)` |
|    - |  683 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  684 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  685 | ` *     be distinct from each other)` |
|    - |  686 | ` *   null (empty)` |
|    - |  687 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  688 | ` */` |
|   64 |  689 | `static sxi32 VmJsonDecode(` |
|    - |  690 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  691 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    2 |  692 | `	){` |
|    - |  693 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  694 | `	sxi32 rc;` |
|    - |  695 | `	/* Check if we do not nest to much */` |
|   66 |  696 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  697 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  698 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  699 | `		return SXERR_ABORT;` |
|    - |  700 | `	}` |
|   66 |  701 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  702 | `		/* Scalar value */` |
|   38 |  703 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   38 |  704 | `		if( pWorker == 0 ){` |
|  ! 0 |  705 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  706 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  707 | `			return SXERR_ABORT;` |
|    - |  708 | `		}` |
|    - |  709 | `		/* Reflect the JSON image */` |
|   38 |  710 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  711 | `			/* Nullify the value.*/` |
|  ! 0 |  712 | `			ph7_value_null(pWorker);` |
|   38 |  713 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  714 | `			/* Boolean value */` |
|  ! 0 |  715 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   38 |  716 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   31 |  717 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  718 | `			/*` |
|    - |  719 | `			 * Numeric value.` |
|    - |  720 | `			 * Get a string representation first then try to get a numeric` |
|    - |  721 | `			 * value.` |
|    - |  722 | `			 */` |
|   31 |  723 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  724 | `			/* Obtain a numeric representation */` |
|   31 |  725 | `			PH7_MemObjToNumeric(pWorker);` |
|   16 |  726 | `		}else{` |
|    - |  727 | `			/* Dequote the string */` |
|    8 |  728 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  729 | `		}` |
|    - |  730 | `		/* Invoke the consumer callback */` |
|   38 |  731 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   38 |  732 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  733 | `			return SXERR_ABORT;` |
|    - |  734 | `		}` |
|    - |  735 | `		/* All done,advance the stream cursor */` |
|   38 |  736 | `		pDecoder->pIn++;` |
|   48 |  737 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  738 | `		ProcJsonConsumer xOld;` |
|    - |  739 | `		void *pOld;` |
|    - |  740 | `		/* Array representation*/` |
|   13 |  741 | `		pDecoder->pIn++;` |
|    - |  742 | `		/* Create a working array */` |
|   13 |  743 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   13 |  744 | `		if( pWorker == 0 ){` |
|  ! 0 |  745 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  746 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  747 | `			return SXERR_ABORT;` |
|    - |  748 | `		}` |
|    - |  749 | `		/* Save the old consumer */` |
|   13 |  750 | `		xOld = pDecoder->xConsumer;` |
|   13 |  751 | `		pOld = pDecoder->pUserData;` |
|    - |  752 | `		/* Set the new consumer */` |
|   13 |  753 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   13 |  754 | `		pDecoder->pUserData = pWorker;` |
|    - |  755 | `		/* Decode the array */` |
|   18 |  756 | `		for(;;){` |
|    - |  757 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  758 | `			 * do this.` |
|    - |  759 | `			 */` |
|   49 |  760 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   13 |  761 | `				pDecoder->pIn++;` |
|    1 |  762 | `			}` |
|   37 |  763 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|   13 |  764 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   13 |  765 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    6 |  766 | `				}` |
|   13 |  767 | `				break;` |
|    - |  768 | `			}` |
|    - |  769 | `			/* Recurse and decode the entry */` |
|   25 |  770 | `			pDecoder->rec_count++;` |
|   25 |  771 | `			rc = VmJsonDecode(pDecoder,0);` |
|   25 |  772 | `			pDecoder->rec_count--;` |
|   25 |  773 | `			if( rc == SXERR_ABORT ){` |
|    - |  774 | `				/* Abort processing immediately */` |
|  ! 0 |  775 | `				return SXERR_ABORT;` |
|    - |  776 | `			}` |
|    - |  777 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   25 |  778 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   24 |  779 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  780 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  781 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  782 | `					return SXERR_ABORT;` |
|    - |  783 | `			}` |
|    1 |  784 | `		}` |
|    - |  785 | `		/* Restore the old consumer */` |
|   13 |  786 | `		pDecoder->xConsumer = xOld;` |
|   13 |  787 | `		pDecoder->pUserData = pOld;` |
|    - |  788 | `		/* Invoke the old consumer on the decoded array */` |
|   13 |  789 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   24 |  790 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  791 | `		ProcJsonConsumer xOld;` |
|    - |  792 | `		ph7_value *pKey;` |
|    - |  793 | `		void *pOld;` |
|    - |  794 | `		/* Object representation*/` |
|   18 |  795 | `		pDecoder->pIn++;` |
|    - |  796 | `		/* Return the object as an associative array */` |
|   18 |  797 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|    3 |  798 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|    - |  799 | `				"JSON Objects are always returned as an associative array"` |
|    - |  800 | `				);` |
|    1 |  801 | `		}` |
|    - |  802 | `		/* Create a working array */` |
|   18 |  803 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   18 |  804 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   18 |  805 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  806 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  807 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  808 | `			return SXERR_ABORT;` |
|    - |  809 | `		}` |
|    - |  810 | `		/* Save the old consumer */` |
|   18 |  811 | `		xOld = pDecoder->xConsumer;` |
|   18 |  812 | `		pOld = pDecoder->pUserData;` |
|    - |  813 | `		/* Set the new consumer */` |
|   18 |  814 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   18 |  815 | `		pDecoder->pUserData = pWorker;` |
|    - |  816 | `		/* Decode the object */` |
|   17 |  817 | `		for(;;){` |
|    - |  818 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  819 | `			 * do this.` |
|    - |  820 | `			 */` |
|   40 |  821 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    5 |  822 | `				pDecoder->pIn++;` |
|    1 |  823 | `			}` |
|   36 |  824 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|   18 |  825 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   16 |  826 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    7 |  827 | `				}` |
|   18 |  828 | `				break;` |
|    - |  829 | `			}` |
|   18 |  830 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   20 |  831 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  832 | `					/* Syntax error,return immediately */` |
|  ! 0 |  833 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  834 | `					return SXERR_ABORT;` |
|    - |  835 | `			}` |
|    - |  836 | `			/* Dequote the key */` |
|   20 |  837 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  838 | `			/* Jump the key and the colon */` |
|   20 |  839 | `			pDecoder->pIn += 2;` |
|    - |  840 | `			/* Recurse and decode the value */` |
|   20 |  841 | `			pDecoder->rec_count++;` |
|   20 |  842 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   20 |  843 | `			pDecoder->rec_count--;` |
|   20 |  844 | `			if( rc == SXERR_ABORT ){` |
|    - |  845 | `				/* Abort processing immediately */` |
|  ! 0 |  846 | `				return SXERR_ABORT;` |
|    - |  847 | `			}` |
|    - |  848 | `			/* Reset the internal buffer of the key */` |
|   20 |  849 | `			ph7_value_reset_string_cursor(pKey);` |
|    - |  850 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    2 |  851 | `		}` |
|    - |  852 | `		/* Restore the old consumer */` |
|   18 |  853 | `		pDecoder->xConsumer = xOld;` |
|   18 |  854 | `		pDecoder->pUserData = pOld;` |
|    - |  855 | `		/* Invoke the old consumer on the decoded object*/` |
|   18 |  856 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - |  857 | `		/* Release the key */` |
|   18 |  858 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|   10 |  859 | `	}else{` |
|    - |  860 | `		/* Unexpected token */` |
|  ! 0 |  861 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - |  862 | `	}` |
|    - |  863 | `	/* Release the worker variable */` |
|   66 |  864 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   66 |  865 | `	return SXRET_OK;` |
|   34 |  866 | `}` |
|    - |  867 | `/*` |
|    - |  868 | ` * The following JSON decoder callback is invoked each time` |
|    - |  869 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - |  870 | ` * is being decoded.` |
|    - |  871 | ` */` |
|   42 |  872 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  873 | `{` |
|   44 |  874 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - |  875 | `	/* Insert the entry */` |
|   44 |  876 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   21 |  877 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - |  878 | `	/* All done */` |
|   44 |  879 | `	return SXRET_OK;` |
|    2 |  880 | `}` |
|    - |  881 | `/*` |
|    - |  882 | ` * Standard JSON decoder callback.` |
|    - |  883 | ` */` |
|   22 |  884 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  885 | `{` |
|    - |  886 | `	/* Return the value directly */` |
|   24 |  887 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|   11 |  888 | `	SXUNUSED(pKey); /* cc warning */` |
|   11 |  889 | `	SXUNUSED(pUserData);` |
|    - |  890 | `	/* All done */` |
|   24 |  891 | `	return SXRET_OK;` |
|    2 |  892 | `}` |
|    - |  893 | `/*` |
|    - |  894 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - |  895 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - |  896 | ` * Parameters` |
|    - |  897 | ` *  $json` |
|    - |  898 | ` *    The json string being decoded.` |
|    - |  899 | ` * $assoc` |
|    - |  900 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - |  901 | ` * $depth` |
|    - |  902 | ` *   User specified recursion depth.` |
|    - |  903 | ` * $options` |
|    - |  904 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - |  905 | ` * (default is to cast large integers as floats)` |
|    - |  906 | ` * Return` |
|    - |  907 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - |  908 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - |  909 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - |  910 | ` */` |
|    - |  911 | `/*` |
|    - |  912 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|    - |  913 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|    - |  914 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|    - |  915 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|    - |  916 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|    - |  917 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|    - |  918 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|    - |  919 | ` */` |
|   36 |  920 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth)` |
|    2 |  921 | `{` |
|   38 |  922 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  923 | `	json_decoder sDecoder;` |
|    - |  924 | `	SySet sToken;` |
|    - |  925 | `	SyLex sLex;` |
|    - |  926 | `	sxi32 rc;` |
|    - |  927 | `	/* Clear JSON error code */` |
|   38 |  928 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  929 | `	/* Tokenize the input */` |
|   38 |  930 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   38 |  931 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   38 |  932 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   38 |  933 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - |  934 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|   16 |  935 | `		SyLexRelease(&sLex);` |
|   16 |  936 | `		SySetRelease(&sToken);` |
|   16 |  937 | `		return pVm->json_rc;` |
|    - |  938 | `	}` |
|    - |  939 | `	/* Fill the decoder */` |
|   24 |  940 | `	sDecoder.pCtx = pCtx;` |
|   24 |  941 | `	sDecoder.pErr = &pVm->json_rc;` |
|   24 |  942 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   24 |  943 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   24 |  944 | `	sDecoder.iFlags = 0;` |
|   24 |  945 | `	if( iAssoc ){` |
|    - |  946 | `		/* Returned objects will be converted into associative arrays */` |
|   22 |  947 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|   10 |  948 | `	}` |
|   24 |  949 | `	sDecoder.rec_depth = 32;` |
|   24 |  950 | `	if( nDepth > 1 && nDepth < 32 ){` |
|    3 |  951 | `		sDecoder.rec_depth = nDepth;` |
|    1 |  952 | `	}` |
|   24 |  953 | `	sDecoder.rec_count = 0;` |
|    - |  954 | `	/* Set a default consumer */` |
|   24 |  955 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   24 |  956 | `	sDecoder.pUserData = 0;` |
|    - |  957 | `	/* Decode the raw JSON input */` |
|   24 |  958 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   24 |  959 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|    - |  960 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|  ! 0 |  961 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 |  962 | `	}` |
|    - |  963 | `	/* Clean-up the mess left behind */` |
|   24 |  964 | `	SyLexRelease(&sLex);` |
|   24 |  965 | `	SySetRelease(&sToken);` |
|   24 |  966 | `	return pVm->json_rc;` |
|   20 |  967 | `}` |
|   38 |  968 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  969 | `{` |
|    - |  970 | `	const char *zIn;` |
|    - |  971 | `	int nByte;` |
|   40 |  972 | `	int iAssoc = 0;` |
|   40 |  973 | `	int nDepth = 32;` |
|   40 |  974 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  975 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 |  976 | `		ph7_result_null(pCtx);` |
|  ! 0 |  977 | `		return PH7_OK;` |
|    - |  978 | `	}` |
|    - |  979 | `	/* Extract the JSON string */` |
|   40 |  980 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   40 |  981 | `	if( nByte < 1 ){` |
|    - |  982 | `		/* Empty string,return NULL */` |
|    6 |  983 | `		ph7_result_null(pCtx);` |
|    6 |  984 | `		return PH7_OK;` |
|    - |  985 | `	}` |
|   36 |  986 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|   24 |  987 | `		iAssoc = 1;` |
|   11 |  988 | `	}` |
|   36 |  989 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    - |  990 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);` |
|    - |  991 | `		 * read as int64 so a value above INT_MAX is detected, not truncated. */` |
|   13 |  992 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);` |
|    - |  993 | `		/* php clears the json error state before validating $depth, so a caught` |
|    - |  994 | `		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal` |
|    - |  995 | `		 * path resets it again inside VmJsonDecodeInput). */` |
|   13 |  996 | `		pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|   13 |  997 | `		if( nWant <= 0 ){` |
|    9 |  998 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  999 | `				"json_decode(): Argument #3 ($depth) must be greater than 0");` |
|    - | 1000 | `		}` |
|    5 | 1001 | `		if( nWant > 2147483647 ){` |
|    3 | 1002 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1003 | `				"json_decode(): Argument #3 ($depth) must be less than 2147483647");` |
|    - | 1004 | `		}` |
|    3 | 1005 | `		nDepth = (int)nWant;` |
|    1 | 1006 | `	}` |
|    - | 1007 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|    - | 1008 | `	 * call-context result; on failure we replace it with NULL. */` |
|   26 | 1009 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth) != JSON_ERROR_NONE ){` |
|    - | 1010 | `		/* Something goes wrong while decoding JSON input.Return NULL. */` |
|   12 | 1011 | `		ph7_result_null(pCtx);` |
|    5 | 1012 | `	}` |
|    - | 1013 | `	/* All done */` |
|   26 | 1014 | `	return PH7_OK;` |
|   21 | 1015 | `}` |
|    - | 1016 | `/*` |
|    - | 1017 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|    - | 1018 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|    - | 1019 | ` * Parameters` |
|    - | 1020 | ` *  $json   The string to validate.` |
|    - | 1021 | ` *  $depth  Maximum nesting depth (clamped to the engine limit of 32).` |
|    - | 1022 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|    - | 1023 | ` * Return` |
|    - | 1024 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|    - | 1025 | ` */` |
|   20 | 1026 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1027 | `{` |
|   21 | 1028 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1029 | `	const char *zIn;` |
|    - | 1030 | `	int nByte;` |
|   21 | 1031 | `	int nDepth = 32;` |
|   21 | 1032 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1033 | `		/* Missing/Invalid argument: not valid JSON */` |
|  ! 0 | 1034 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1035 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1036 | `		return PH7_OK;` |
|    - | 1037 | `	}` |
|    - | 1038 | `	/* Extract the JSON string */` |
|   21 | 1039 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   21 | 1040 | `	if( nByte < 1 ){` |
|    - | 1041 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|    - | 1042 | `		 * silently, json_validate must record the syntax error) */` |
|    3 | 1043 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    3 | 1044 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1045 | `		return PH7_OK;` |
|    - | 1046 | `	}` |
|   19 | 1047 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - | 1048 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */` |
|    9 | 1049 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|    - | 1050 | `		/* Clear the json error state before validating $depth (php parity), so a` |
|    - | 1051 | `		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */` |
|    9 | 1052 | `		pVm->json_rc = JSON_ERROR_NONE;` |
|    9 | 1053 | `		if( nWant <= 0 ){` |
|    5 | 1054 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1055 | `				"json_validate(): Argument #2 ($depth) must be greater than 0");` |
|    - | 1056 | `		}` |
|    5 | 1057 | `		if( nWant > 2147483647 ){` |
|    3 | 1058 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1059 | `				"json_validate(): Argument #2 ($depth) must be less than 2147483647");` |
|    - | 1060 | `		}` |
|    3 | 1061 | `		nDepth = (int)nWant;` |
|    1 | 1062 | `	}` |
|    - | 1063 | `	/* apArg[2] ($flags) is accepted and ignored: no decode flag is implemented.` |
|    - | 1064 | `	 * Decode in associative mode so the "objects are returned as an array" warning is` |
|    - | 1065 | `	 * not raised - the decoded value is discarded, only its validity matters. */` |
|   13 | 1066 | `	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth) == JSON_ERROR_NONE);` |
|   13 | 1067 | `	return PH7_OK;` |
|   11 | 1068 | `}` |
|    - | 1069 |  |
