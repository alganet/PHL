# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 458/590 lines (77.63%)

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
|    - |   30 | `	int fail;          /* True if the value is unencodable (php 8.1: a non-backed` |
|    - |   31 | `	                    * enum case) — json_encode returns FALSE */` |
|    - |   32 | `};` |
|    - |   33 | `/*` |
|    - |   34 | ` * Emit into the JSON result, flagging OOM on the shared data and bailing out` |
|    - |   35 | ` * of the current encode function (which returns PH7_OK; the top-level` |
|    - |   36 | ` * vm_builtin_json_encode checks ->oom and raises a non-catchable fatal). Used` |
|    - |   37 | ` * for every ph7_result_string/ph7_result_string_format append below.` |
|    - |   38 | ` */` |
|    - |   39 | `#define JSON_EMIT(pD, call) do { if( (call) != SXRET_OK ){ (pD)->oom = 1; return PH7_OK; } } while(0)` |
|    - |   40 | `/*` |
|    - |   41 | ` * Emit a float in php's json shape: PH7_AppendShortestReal (the shared` |
|    - |   42 | ` * serialize/var_export shortest-round-trip formatter, php's` |
|    - |   43 | ` * serialize_precision=-1) with the exponent marker lowercased (json prints` |
|    - |   44 | ` * 1.0e+17 where serialize prints 1.0E+17).` |
|    - |   45 | ` */` |
|   20 |   46 | `static sxi32 VmJsonEmitReal(ph7_context *pCtx,double rVal)` |
|    1 |   47 | `{` |
|    - |   48 | `	SyBlob sNum;` |
|    - |   49 | `	char *z;` |
|    - |   50 | `	sxu32 i,n;` |
|    - |   51 | `	sxi32 rc;` |
|   21 |   52 | `	SyBlobInit(&sNum,&pCtx->pVm->sAllocator);` |
|   21 |   53 | `	PH7_AppendShortestReal(&sNum,rVal);` |
|   21 |   54 | `	z = (char *)SyBlobData(&sNum);` |
|   21 |   55 | `	n = SyBlobLength(&sNum);` |
|   21 |   56 | `	if( z == 0 \|\| n < 1 ){` |
|  ! 0 |   57 | `		SyBlobRelease(&sNum);` |
|  ! 0 |   58 | `		return SXERR_MEM; /* treated as OOM by JSON_EMIT */` |
|    - |   59 | `	}` |
|  217 |   60 | `	for( i = 0 ; i < n ; i++ ){` |
|  197 |   61 | `		if( z[i] == 'E' ){` |
|    7 |   62 | `			z[i] = 'e';` |
|    3 |   63 | `		}` |
|   99 |   64 | `	}` |
|   21 |   65 | `	rc = ph7_result_string(pCtx,(const char *)z,(int)n);` |
|   21 |   66 | `	SyBlobRelease(&sNum);` |
|   21 |   67 | `	return rc;` |
|   11 |   68 | `}` |
|    - |   69 | `/*` |
|    - |   70 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|    - |   71 | ` * According to wikipedia` |
|    - |   72 | ` * JSON's basic types are:` |
|    - |   73 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |   74 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |   75 | ` *   Boolean (true or false)` |
|    - |   76 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |   77 | ` *    do not need to be of the same type)` |
|    - |   78 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |   79 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |   80 | ` *     be distinct from each other)` |
|    - |   81 | ` *   null (empty)` |
|    - |   82 | ` * Non-significant white space may be added freely around the "structural characters"` |
|    - |   83 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |   84 | ` */` |
|  824 |   85 | `static sxi32 VmJsonEncode(` |
|    - |   86 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |   87 | `	json_private_data *pData /* Context data */` |
|    2 |   88 | `	){` |
|  826 |   89 | `		ph7_context *pCtx = pData->pCtx;` |
|  826 |   90 | `		int iFlags = pData->iFlags;` |
|    - |   91 | `		int nByte;` |
|  826 |   92 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |   93 | `			/* null */` |
|    5 |   94 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|  823 |   95 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   15 |   96 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |   97 | `			int iLen;` |
|    - |   98 | `			/* true/false */` |
|   15 |   99 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   15 |  100 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
|  814 |  101 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|  406 |  102 | `			if( ph7_value_is_float(pIn) ){` |
|    - |  103 | `				/* php's json float output follows serialize_precision` |
|    - |  104 | `				 * (shortest round-trip, like serialize/var_export), NOT the` |
|    - |  105 | `				 * echo/cast precision of 14 — with a lowercase exponent` |
|    - |  106 | `				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,` |
|    - |  107 | `				 * 1.0 -> 1, -0.0 -> -0. */` |
|   17 |  108 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    9 |  109 | `			}else{` |
|    - |  110 | `				const char *zNum;` |
|    - |  111 | `				/* Get a string representation of the number */` |
|  255 |  112 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|  255 |  113 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|    1 |  114 | `			}` |
|  673 |  115 | `		}else if( ph7_value_is_string(pIn) ){` |
|  260 |  116 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |  117 | `				/* Encodes numeric strings as numbers (same float shapes). */` |
|    5 |  118 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    5 |  119 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    3 |  120 | `			}else{` |
|    - |  121 | `				const char *zIn,*zEnd;` |
|    - |  122 | `				int c;` |
|    - |  123 | `				/* Encode the string */` |
|  256 |  124 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|  256 |  125 | `				zEnd = &zIn[nByte];` |
|    - |  126 | `				/* Append the double quote */` |
|  256 |  127 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|  479 |  128 | `				for(;;){` |
|  966 |  129 | `					if( zIn >= zEnd ){` |
|    - |  130 | `						/* No more input to process */` |
|  256 |  131 | `						break;` |
|    - |  132 | `					}` |
|  712 |  133 | `					c = zIn[0];` |
|    - |  134 | `					/* Advance the stream cursor */` |
|  712 |  135 | `					zIn++;` |
|  712 |  136 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |  137 | `						/* All < and > are converted to \u003C and \u003E */` |
|  ! 0 |  138 | `						if( c == '<' ){` |
|  ! 0 |  139 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1));` |
|  ! 0 |  140 | `						}else{` |
|  ! 0 |  141 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1));` |
|    - |  142 | `						}` |
|  ! 0 |  143 | `						continue;` |
|  712 |  144 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |  145 | `						/* All &s are converted to \u0026.  */` |
|  ! 0 |  146 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1));` |
|  ! 0 |  147 | `						continue;` |
|  712 |  148 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  149 | `						/* All ' are converted to \u0027.   */` |
|  ! 0 |  150 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1));` |
|  ! 0 |  151 | `						continue;` |
|  712 |  152 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  153 | `						/* All " are converted to \u0022. */` |
|  ! 0 |  154 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1));` |
|  ! 0 |  155 | `						continue;` |
|    - |  156 | `					}` |
|  712 |  157 | `					if( c == '"' \|\| c == '\\' ){` |
|    - |  158 | `						/* Escape the quote/backslash (php escapes the backslash` |
|    - |  159 | `						 * unconditionally — the old code wrongly tied it to` |
|    - |  160 | `						 * JSON_UNESCAPED_SLASHES, which governs '/' below) */` |
|    3 |  161 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  710 |  162 | `					}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){` |
|    - |  163 | `						/* php escapes forward slashes by default */` |
|    7 |  164 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  706 |  165 | `					}else if( (unsigned char)c < 0x20 ){` |
|    - |  166 | `						/* Control characters (band A #4): php emits the short` |
|    - |  167 | `						 * escapes for \b \f \n \r \t and \u00xx for the rest —` |
|    - |  168 | `						 * pre-fix these were emitted RAW (invalid JSON). */` |
|    - |  169 | `						static const char zHex[] = "0123456789abcdef";` |
|    7 |  170 | `						char zEsc[6] = { '\\', 'u', '0', '0', 0, 0 };` |
|    7 |  171 | `						switch(c){` |
|  ! 0 |  172 | `						case '\b': JSON_EMIT(pData,ph7_result_string(pCtx,"\\b",2)); break;` |
|  ! 0 |  173 | `						case '\f': JSON_EMIT(pData,ph7_result_string(pCtx,"\\f",2)); break;` |
|    3 |  174 | `						case '\n': JSON_EMIT(pData,ph7_result_string(pCtx,"\\n",2)); break;` |
|  ! 0 |  175 | `						case '\r': JSON_EMIT(pData,ph7_result_string(pCtx,"\\r",2)); break;` |
|    3 |  176 | `						case '\t': JSON_EMIT(pData,ph7_result_string(pCtx,"\\t",2)); break;` |
|    1 |  177 | `						default:` |
|    3 |  178 | `							zEsc[4] = zHex[(c >> 4) & 0x0F];` |
|    3 |  179 | `							zEsc[5] = zHex[c & 0x0F];` |
|    3 |  180 | `							JSON_EMIT(pData,ph7_result_string(pCtx,zEsc,6));` |
|    2 |  181 | `							break;` |
|    - |  182 | `						}` |
|    7 |  183 | `						continue;` |
|    - |  184 | `					}` |
|    - |  185 | `					/* Append character verbatim */` |
|  706 |  186 | `					JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    2 |  187 | `				}` |
|    - |  188 | `				/* Append the double quote */` |
|  256 |  189 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|    2 |  190 | `			}` |
|  409 |  191 | `		}else if( ph7_value_is_array(pIn) ){` |
|    - |  192 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|    - |  193 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|    - |  194 | `			 * object with stringified keys (PHP semantics). */` |
|  432 |  195 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|  216 |  196 | `				\|\| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);` |
|  217 |  197 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|  217 |  198 | `			int c = isObject ? '{' : '[';` |
|  217 |  199 | `			int d = isObject ? '}' : ']';` |
|    - |  200 | `			/* Encode the array */` |
|  217 |  201 | `			pData->isObject = isObject;` |
|  217 |  202 | `			pData->isFirst = 1;` |
|    - |  203 | `			/* Append the square bracket or curly braces */` |
|  217 |  204 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    - |  205 | `			/* Iterate throw array entries */` |
|  217 |  206 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  207 | `			/* Bail if a nested append ran out of memory before the closer */` |
|  217 |  208 | `			if( pData->oom ){` |
|  ! 0 |  209 | `				return PH7_OK;` |
|    - |  210 | `			}` |
|    - |  211 | `			/* Append the closing square bracket or curly braces */` |
|  217 |  212 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|  217 |  213 | `			pData->isObject = savedObject;` |
|  172 |  214 | `		}else if( ph7_value_is_object(pIn) ){` |
|   64 |  215 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   64 |  216 | `			ph7_vm *pVm = pIn->pVm;` |
|   64 |  217 | `			ph7_class_method *pMethod = 0;` |
|    - |  218 | `			/* If the object implements JsonSerializable, encode the value` |
|    - |  219 | `			 * returned by jsonSerialize() instead of its public properties.` |
|    - |  220 | `			 * An enum implementing it explicitly also takes this path (php). */` |
|   62 |  221 | `			if( pVm->pJsonSerializableClass` |
|   64 |  222 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|   15 |  223 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|    7 |  224 | `			}` |
|   64 |  225 | `			if( pMethod == 0 && (pThis->pClass->iFlags & PH7_CLASS_ENUM) != 0 ){` |
|    - |  226 | `				/* php 8.1: a BACKED enum case encodes as its backing value; a` |
|    - |  227 | `				 * pure enum case has no default serialization — json_encode` |
|    - |  228 | `				 * returns false. */` |
|    9 |  229 | `				ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pThis);` |
|    9 |  230 | `				if( pBacking ){` |
|    7 |  231 | `					pData->nRecCount++;` |
|    7 |  232 | `					VmJsonEncode(pBacking,pData);` |
|    7 |  233 | `					pData->nRecCount--;` |
|    4 |  234 | `				}else{` |
|    3 |  235 | `					pData->fail = 1;` |
|    - |  236 | `				}` |
|    9 |  237 | `				return PH7_OK;` |
|    - |  238 | `			}` |
|   56 |  239 | `			if( pMethod ){` |
|    - |  240 | `				ph7_value sResult;` |
|    - |  241 | `				sxi32 rc;` |
|   15 |  242 | `				PH7_MemObjInit(pVm,&sResult);` |
|   15 |  243 | `				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|   15 |  244 | `				if( rc == PH7_EXCEPTION ){` |
|    - |  245 | `					/* Let jsonSerialize()'s throw propagate */` |
|    5 |  246 | `					PH7_MemObjRelease(&sResult);` |
|    5 |  247 | `					pData->exc = 1;` |
|    5 |  248 | `					return PH7_EXCEPTION;` |
|    - |  249 | `				}` |
|    - |  250 | `				/* Encode the returned value [scalar/array/object] */` |
|   11 |  251 | `				pData->nRecCount++;` |
|   11 |  252 | `				VmJsonEncode(&sResult,pData);` |
|   11 |  253 | `				pData->nRecCount--;` |
|   11 |  254 | `				PH7_MemObjRelease(&sResult);` |
|   11 |  255 | `				if( pData->exc ){` |
|  ! 0 |  256 | `					return PH7_EXCEPTION;` |
|    - |  257 | `				}` |
|   11 |  258 | `				if( pData->oom ){` |
|  ! 0 |  259 | `					return PH7_OK;` |
|    - |  260 | `				}` |
|    6 |  261 | `			}else{` |
|    - |  262 | `				/* Encode the class instance */` |
|   42 |  263 | `				pData->isFirst = 1;` |
|    - |  264 | `				/* Append the curly braces */` |
|   42 |  265 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|    - |  266 | `				/* Iterate throw class attribute */` |
|   42 |  267 | `				ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|   42 |  268 | `				if( pData->oom ){` |
|  ! 0 |  269 | `					return PH7_OK;` |
|    - |  270 | `				}` |
|    - |  271 | `				/* Append the closing curly braces  */` |
|   42 |  272 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|    - |  273 | `			}` |
|   27 |  274 | `		}else{` |
|    - |  275 | `			/* Can't happen */` |
|  ! 0 |  276 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    - |  277 | `		}` |
|    - |  278 | `		/* All done */` |
|  814 |  279 | `		return PH7_OK;` |
|  414 |  280 | `}` |
|    - |  281 | `/*` |
|    - |  282 | ` * The following walker callback is invoked each time we need` |
|    - |  283 | ` * to encode an array to JSON.` |
|    - |  284 | ` */` |
|  496 |  285 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    1 |  286 | `{` |
|  497 |  287 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  497 |  288 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  289 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  290 | `		return PH7_OK;` |
|    - |  291 | `	}` |
|  497 |  292 | `	if( !pJson->isFirst ){` |
|    - |  293 | `		/* Append the colon first */` |
|  305 |  294 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|  152 |  295 | `	}` |
|  497 |  296 | `	if( pJson->isObject ){` |
|    - |  297 | `		/* Outputs an object rather than an array */` |
|    - |  298 | `		const char *zKey;` |
|    - |  299 | `		int nByte;` |
|    - |  300 | `		/* Extract a string representation of the key */` |
|  255 |  301 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  302 | `		/* Append the quoted key and the colon (checked, so an OOM here is caught` |
|    - |  303 | `		 * rather than silently truncating; matches the prior "%.*s" emit byte for` |
|    - |  304 | `		 * byte — keys are not JSON-escaped, a pre-existing behavior). */` |
|  255 |  305 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|  255 |  306 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zKey,nByte));` |
|  255 |  307 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|  127 |  308 | `	}` |
|    - |  309 | `	/* Encode the value */` |
|  497 |  310 | `	pJson->nRecCount++;` |
|  497 |  311 | `	VmJsonEncode(pValue,pJson);` |
|  497 |  312 | `	pJson->nRecCount--;` |
|  497 |  313 | `	pJson->isFirst = 0;` |
|  497 |  314 | `	return PH7_OK;` |
|  249 |  315 | `}` |
|    - |  316 | `/*` |
|    - |  317 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  318 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  319 | ` */` |
|   62 |  320 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|    2 |  321 | `{` |
|   64 |  322 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|   64 |  323 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  324 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  325 | `		return PH7_OK;` |
|    - |  326 | `	}` |
|   64 |  327 | `	if( !pJson->isFirst ){` |
|    - |  328 | `		/* Append the colon first */` |
|   25 |  329 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   12 |  330 | `	}` |
|    - |  331 | `	/* Append the quoted attribute name and the colon (checked; matches the prior` |
|    - |  332 | `	 * "%s" emit byte for byte — attribute names are not JSON-escaped). */` |
|   64 |  333 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|   64 |  334 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zAttr,-1));` |
|   64 |  335 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|    - |  336 | `	/* Encode the value */` |
|   64 |  337 | `	pJson->nRecCount++;` |
|   64 |  338 | `	VmJsonEncode(pValue,pJson);` |
|   64 |  339 | `	pJson->nRecCount--;` |
|   64 |  340 | `	pJson->isFirst = 0;` |
|   64 |  341 | `	return PH7_OK;` |
|   33 |  342 | `}` |
|    - |  343 | `/*` |
|    - |  344 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  345 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  346 | ` * Parameters` |
|    - |  347 | ` *  $value` |
|    - |  348 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  349 | ` * $options` |
|    - |  350 | ` *  Bitmask consisting of:` |
|    - |  351 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  352 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  353 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  354 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  355 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  356 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  357 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  358 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  359 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  360 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  361 | ` * Return` |
|    - |  362 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  363 | ` */` |
|  250 |  364 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  365 | `{` |
|    - |  366 | `	json_private_data sJson;` |
|    - |  367 | `	sxi32 rc;` |
|  252 |  368 | `	if( nArg < 1 ){` |
|    - |  369 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  370 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  371 | `		return PH7_OK;` |
|    - |  372 | `	}` |
|    - |  373 | `	/* Prepare the JSON data */` |
|  252 |  374 | `	sJson.nRecCount = 0;` |
|  252 |  375 | `	sJson.pCtx = pCtx;` |
|  252 |  376 | `	sJson.isFirst = 1;` |
|  252 |  377 | `	sJson.iFlags = 0;` |
|  252 |  378 | `	sJson.exc = 0;` |
|  252 |  379 | `	sJson.oom = 0;` |
|  252 |  380 | `	sJson.fail = 0;` |
|  252 |  381 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  382 | `		/* Extract option flags */` |
|    7 |  383 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|    3 |  384 | `	}` |
|  252 |  385 | `	pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  386 | `	/* Perform the encoding operation */` |
|  252 |  387 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|  252 |  388 | `	if( sJson.oom ){` |
|    - |  389 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|    - |  390 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|  ! 0 |  391 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  392 | `	}` |
|  252 |  393 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|    - |  394 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|    5 |  395 | `		return PH7_EXCEPTION;` |
|    - |  396 | `	}` |
|  248 |  397 | `	if( sJson.fail ){` |
|    - |  398 | `		/* Unencodable value (php 8.1: non-backed enum case): the whole encode` |
|    - |  399 | `		 * fails — discard whatever was emitted and return FALSE. */` |
|    3 |  400 | `		pCtx->pVm->json_rc = JSON_ERROR_NON_BACKED_ENUM;` |
|    3 |  401 | `		ph7_result_bool(pCtx,0);` |
|    3 |  402 | `		return PH7_OK;` |
|    - |  403 | `	}` |
|    - |  404 | `	/* All done */` |
|  246 |  405 | `	return PH7_OK;` |
|  127 |  406 | `}` |
|    - |  407 | `#undef JSON_EMIT` |
|    - |  408 | `/*` |
|    - |  409 | ` * int json_last_error(void)` |
|    - |  410 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  411 | ` * Parameters` |
|    - |  412 | ` *  None` |
|    - |  413 | ` * Return` |
|    - |  414 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  415 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  416 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  417 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  418 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  419 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  420 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  421 | ` */` |
|   12 |  422 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  423 | `{` |
|   14 |  424 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  425 | `	/* Return the error code */` |
|   14 |  426 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    6 |  427 | `	SXUNUSED(nArg); /* cc warning */` |
|    6 |  428 | `	SXUNUSED(apArg);` |
|   14 |  429 | `	return PH7_OK;` |
|    2 |  430 | `}` |
|    - |  431 | `/*` |
|    - |  432 | ` * string json_last_error_msg(void)` |
|    - |  433 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|    - |  434 | ` * Parameters` |
|    - |  435 | ` *  None` |
|    - |  436 | ` * Return` |
|    - |  437 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|    - |  438 | ` *  code, or "No error" if no error has occurred.` |
|    - |  439 | ` */` |
|    4 |  440 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  441 | `{` |
|    5 |  442 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  443 | `	const char *zMsg;` |
|    5 |  444 | `	switch( pVm->json_rc ){` |
|    1 |  445 | `	case JSON_ERROR_NONE:` |
|    3 |  446 | `		zMsg = "No error";` |
|    3 |  447 | `		break;` |
|  ! 0 |  448 | `	case JSON_ERROR_DEPTH:` |
|  ! 0 |  449 | `		zMsg = "Maximum stack depth exceeded";` |
|  ! 0 |  450 | `		break;` |
|  ! 0 |  451 | `	case JSON_ERROR_STATE_MISMATCH:` |
|  ! 0 |  452 | `		zMsg = "State mismatch (invalid or malformed JSON)";` |
|  ! 0 |  453 | `		break;` |
|  ! 0 |  454 | `	case JSON_ERROR_CTRL_CHAR:` |
|  ! 0 |  455 | `		zMsg = "Control character error, possibly incorrectly encoded";` |
|  ! 0 |  456 | `		break;` |
|    1 |  457 | `	case JSON_ERROR_SYNTAX:` |
|    3 |  458 | `		zMsg = "Syntax error";` |
|    3 |  459 | `		break;` |
|  ! 0 |  460 | `	case JSON_ERROR_UTF8:` |
|  ! 0 |  461 | `		zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|  ! 0 |  462 | `		break;` |
|  ! 0 |  463 | `	case JSON_ERROR_NON_BACKED_ENUM:` |
|  ! 0 |  464 | `		zMsg = "Non-backed enums have no default serialization";` |
|  ! 0 |  465 | `		break;` |
|  ! 0 |  466 | `	default:` |
|  ! 0 |  467 | `		zMsg = "Unknown error";` |
|  ! 0 |  468 | `		break;` |
|    - |  469 | `	}` |
|    5 |  470 | `	ph7_result_string(pCtx,zMsg,-1/* Compute length automatically */);` |
|    2 |  471 | `	SXUNUSED(nArg); /* cc warning */` |
|    2 |  472 | `	SXUNUSED(apArg);` |
|    5 |  473 | `	return PH7_OK;` |
|    1 |  474 | `}` |
|    - |  475 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  476 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  477 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  478 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  479 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  480 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  481 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  482 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  483 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  484 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  485 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  486 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  487 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  488 | `/*` |
|    - |  489 | ` * Tokenize an entire JSON input.` |
|    - |  490 | ` * Get a single low-level token from the input file.` |
|    - |  491 | ` * Update the stream pointer so that it points to the first` |
|    - |  492 | ` * character beyond the extracted token.` |
|    - |  493 | ` */` |
|  160 |  494 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    2 |  495 | `{` |
|  162 |  496 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  497 | `	SyString *pStr;` |
|    - |  498 | `	int c;` |
|    - |  499 | `	/* Ignore leading white spaces */` |
|  166 |  500 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  501 | `		/* Advance the stream cursor */` |
|    6 |  502 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  503 | `			/* Update line counter */` |
|  ! 0 |  504 | `			pStream->nLine++;` |
|  ! 0 |  505 | `		}` |
|    6 |  506 | `		pStream->zText++;` |
|    2 |  507 | `	}` |
|  162 |  508 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  509 | `		/* End of input reached */` |
|  ! 0 |  510 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  511 | `		return SXERR_EOF;` |
|    - |  512 | `	}` |
|    - |  513 | `	/* Record token starting position and line */` |
|  162 |  514 | `	pToken->nLine = pStream->nLine;` |
|  162 |  515 | `	pToken->pUserData = 0;` |
|  162 |  516 | `	pStr = &pToken->sData;` |
|  162 |  517 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  160 |  518 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|  110 |  519 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  520 | `			/* Single character */` |
|   94 |  521 | `			c = pStream->zText[0];` |
|    - |  522 | `			/* Set token type */` |
|   94 |  523 | `			switch(c){` |
|   13 |  524 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   22 |  525 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|   16 |  526 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|   13 |  527 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   20 |  528 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   17 |  529 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  530 | `			default:` |
|  ! 0 |  531 | `				break;` |
|    - |  532 | `			}` |
|    - |  533 | `			/* Advance the stream cursor */` |
|   94 |  534 | `			pStream->zText++;` |
|  116 |  535 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  536 | `		/* JSON string */` |
|   26 |  537 | `		pStream->zText++;` |
|   26 |  538 | `		pStr->zString++;` |
|    - |  539 | `		/* Delimit the string */` |
|   72 |  540 | `		while( pStream->zText < pStream->zEnd ){` |
|   72 |  541 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   26 |  542 | `				break;` |
|    - |  543 | `			}` |
|   48 |  544 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  545 | `				/* Update line counter */` |
|  ! 0 |  546 | `				pStream->nLine++;` |
|  ! 0 |  547 | `			}` |
|   48 |  548 | `			pStream->zText++;` |
|    2 |  549 | `		}` |
|   26 |  550 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  551 | `			/* Missing closing '"' */` |
|  ! 0 |  552 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  553 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  554 | `		}else{` |
|   26 |  555 | `			pToken->nType = JSON_TK_STR;` |
|   26 |  556 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    2 |  557 | `		}` |
|   58 |  558 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    - |  559 | `		/* Number */` |
|   31 |  560 | `		pStream->zText++;` |
|   31 |  561 | `		pToken->nType = JSON_TK_NUM;` |
|   31 |  562 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  563 | `			pStream->zText++;` |
|  ! 0 |  564 | `		}` |
|   31 |  565 | `		if( pStream->zText < pStream->zEnd ){` |
|   31 |  566 | `			c = pStream->zText[0];` |
|   31 |  567 | `			if( c == '.' ){` |
|    - |  568 | `					/* Real number */` |
|  ! 0 |  569 | `					pStream->zText++;` |
|  ! 0 |  570 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  571 | `						pStream->zText++;` |
|  ! 0 |  572 | `					}` |
|  ! 0 |  573 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  574 | `						c = pStream->zText[0];` |
|  ! 0 |  575 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  576 | `							pStream->zText++;` |
|  ! 0 |  577 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  578 | `								c = pStream->zText[0];` |
|  ! 0 |  579 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  580 | `									pStream->zText++;` |
|  ! 0 |  581 | `								}` |
|  ! 0 |  582 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  583 | `									pStream->zText++;` |
|  ! 0 |  584 | `								}` |
|  ! 0 |  585 | `							}` |
|  ! 0 |  586 | `						}` |
|  ! 0 |  587 | `					}` |
|   31 |  588 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  589 | `					/* Real number */` |
|  ! 0 |  590 | `					pStream->zText++;` |
|  ! 0 |  591 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  592 | `						c = pStream->zText[0];` |
|  ! 0 |  593 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  594 | `							pStream->zText++;` |
|  ! 0 |  595 | `						}` |
|  ! 0 |  596 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  597 | `							pStream->zText++;` |
|  ! 0 |  598 | `						}` |
|  ! 0 |  599 | `					}` |
|  ! 0 |  600 | `				}` |
|   16 |  601 | `			}` |
|   37 |  602 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|   12 |  603 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  604 | `			/* boolean true */` |
|  ! 0 |  605 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  606 | `			/* Advance the stream cursor */` |
|  ! 0 |  607 | `			pStream->zText += sizeof("true")-1;` |
|   22 |  608 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|   12 |  609 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  610 | `			/* boolean false */` |
|  ! 0 |  611 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  612 | `			/* Advance the stream cursor */` |
|  ! 0 |  613 | `			pStream->zText += sizeof("false")-1;` |
|   22 |  614 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|   12 |  615 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  616 | `			/* NULL */` |
|  ! 0 |  617 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  618 | `			/* Advance the stream cursor */` |
|  ! 0 |  619 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  620 | `	}else{` |
|    - |  621 | `		/* Unexpected token */` |
|   16 |  622 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  623 | `		/* Advance the stream cursor */` |
|   16 |  624 | `		pStream->zText++;` |
|   16 |  625 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  626 | `		/* Abort processing immediatley */` |
|   16 |  627 | `		return SXERR_ABORT;` |
|    - |  628 | `	}` |
|    - |  629 | `	/* record token length */` |
|  148 |  630 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  148 |  631 | `	if( pToken->nType == JSON_TK_STR ){` |
|   26 |  632 | `		pStr->nByte--;` |
|   12 |  633 | `	}` |
|    - |  634 | `	/* Return to the lexer */` |
|  148 |  635 | `	return SXRET_OK;` |
|   82 |  636 | `}` |
|    - |  637 | `/*` |
|    - |  638 | ` * JSON decoded input consumer callback signature.` |
|    - |  639 | ` */` |
|    - |  640 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  641 | `/*` |
|    - |  642 | ` * JSON decoder state is kept in the following structure.` |
|    - |  643 | ` */` |
|    - |  644 | `typedef struct json_decoder json_decoder;` |
|    - |  645 | `struct json_decoder` |
|    - |  646 | `{` |
|    - |  647 | `	ph7_context *pCtx; /* Call context */` |
|    - |  648 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  649 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  650 | `	int iFlags;        /* Configuration flags */` |
|    - |  651 | `	SyToken *pIn;      /* Token stream */` |
|    - |  652 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  653 | `	int rec_depth;     /* Recursion limit */` |
|    - |  654 | `	int rec_count;     /* Current nesting level */` |
|    - |  655 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  656 | `};` |
|    - |  657 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  658 | `/* Forward declaration */` |
|    - |  659 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  660 | `/*` |
|    - |  661 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  662 | ` * the result in the given ph7_value.` |
|    - |  663 | ` */` |
|   24 |  664 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    2 |  665 | `{` |
|   26 |  666 | `	const char *zIn = pStr->zString;` |
|   26 |  667 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  668 | `	const char *zCur;` |
|    - |  669 | `	int c;` |
|    - |  670 | `	/* Mark the value as a string */` |
|   26 |  671 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|   12 |  672 | `	for(;;){` |
|   26 |  673 | `		zCur = zIn;` |
|   72 |  674 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   48 |  675 | `			zIn++;` |
|    2 |  676 | `		}` |
|   26 |  677 | `		if( zIn > zCur ){` |
|    - |  678 | `			/* Append chunk verbatim */` |
|   26 |  679 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|   12 |  680 | `		}` |
|   26 |  681 | `		zIn++;` |
|   26 |  682 | `		if( zIn >= zEnd ){` |
|    - |  683 | `			/* End of the input reached */` |
|   26 |  684 | `			break;` |
|    - |  685 | `		}` |
|  ! 0 |  686 | `		c = zIn[0];` |
|    - |  687 | `		/* Unescape the character */` |
|  ! 0 |  688 | `		switch(c){` |
|  ! 0 |  689 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  690 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  691 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  692 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  693 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  694 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  695 | `		default:` |
|  ! 0 |  696 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  697 | `			break;` |
|    - |  698 | `		}` |
|    - |  699 | `		/* Advance the stream cursor */` |
|  ! 0 |  700 | `		zIn++;` |
|  ! 0 |  701 | `	}` |
|   26 |  702 | `}` |
|    - |  703 | `/*` |
|    - |  704 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  705 | ` * According to wikipedia` |
|    - |  706 | ` * JSON's basic types are:` |
|    - |  707 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  708 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  709 | ` *   Boolean (true or false)` |
|    - |  710 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  711 | ` *    do not need to be of the same type)` |
|    - |  712 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  713 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  714 | ` *     be distinct from each other)` |
|    - |  715 | ` *   null (empty)` |
|    - |  716 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  717 | ` */` |
|   64 |  718 | `static sxi32 VmJsonDecode(` |
|    - |  719 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  720 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    2 |  721 | `	){` |
|    - |  722 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  723 | `	sxi32 rc;` |
|    - |  724 | `	/* Check if we do not nest to much */` |
|   66 |  725 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  726 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  727 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  728 | `		return SXERR_ABORT;` |
|    - |  729 | `	}` |
|   66 |  730 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  731 | `		/* Scalar value */` |
|   38 |  732 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   38 |  733 | `		if( pWorker == 0 ){` |
|  ! 0 |  734 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  735 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  736 | `			return SXERR_ABORT;` |
|    - |  737 | `		}` |
|    - |  738 | `		/* Reflect the JSON image */` |
|   38 |  739 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  740 | `			/* Nullify the value.*/` |
|  ! 0 |  741 | `			ph7_value_null(pWorker);` |
|   38 |  742 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  743 | `			/* Boolean value */` |
|  ! 0 |  744 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   38 |  745 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   31 |  746 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  747 | `			/*` |
|    - |  748 | `			 * Numeric value.` |
|    - |  749 | `			 * Get a string representation first then try to get a numeric` |
|    - |  750 | `			 * value.` |
|    - |  751 | `			 */` |
|   31 |  752 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  753 | `			/* Obtain a numeric representation */` |
|   31 |  754 | `			PH7_MemObjToNumeric(pWorker);` |
|   16 |  755 | `		}else{` |
|    - |  756 | `			/* Dequote the string */` |
|    8 |  757 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  758 | `		}` |
|    - |  759 | `		/* Invoke the consumer callback */` |
|   38 |  760 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   38 |  761 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  762 | `			return SXERR_ABORT;` |
|    - |  763 | `		}` |
|    - |  764 | `		/* All done,advance the stream cursor */` |
|   38 |  765 | `		pDecoder->pIn++;` |
|   48 |  766 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  767 | `		ProcJsonConsumer xOld;` |
|    - |  768 | `		void *pOld;` |
|    - |  769 | `		/* Array representation*/` |
|   13 |  770 | `		pDecoder->pIn++;` |
|    - |  771 | `		/* Create a working array */` |
|   13 |  772 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   13 |  773 | `		if( pWorker == 0 ){` |
|  ! 0 |  774 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  775 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  776 | `			return SXERR_ABORT;` |
|    - |  777 | `		}` |
|    - |  778 | `		/* Save the old consumer */` |
|   13 |  779 | `		xOld = pDecoder->xConsumer;` |
|   13 |  780 | `		pOld = pDecoder->pUserData;` |
|    - |  781 | `		/* Set the new consumer */` |
|   13 |  782 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   13 |  783 | `		pDecoder->pUserData = pWorker;` |
|    - |  784 | `		/* Decode the array */` |
|   18 |  785 | `		for(;;){` |
|    - |  786 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  787 | `			 * do this.` |
|    - |  788 | `			 */` |
|   49 |  789 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   13 |  790 | `				pDecoder->pIn++;` |
|    1 |  791 | `			}` |
|   37 |  792 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|   13 |  793 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   13 |  794 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    6 |  795 | `				}` |
|   13 |  796 | `				break;` |
|    - |  797 | `			}` |
|    - |  798 | `			/* Recurse and decode the entry */` |
|   25 |  799 | `			pDecoder->rec_count++;` |
|   25 |  800 | `			rc = VmJsonDecode(pDecoder,0);` |
|   25 |  801 | `			pDecoder->rec_count--;` |
|   25 |  802 | `			if( rc == SXERR_ABORT ){` |
|    - |  803 | `				/* Abort processing immediately */` |
|  ! 0 |  804 | `				return SXERR_ABORT;` |
|    - |  805 | `			}` |
|    - |  806 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   25 |  807 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   24 |  808 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  809 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  810 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  811 | `					return SXERR_ABORT;` |
|    - |  812 | `			}` |
|    1 |  813 | `		}` |
|    - |  814 | `		/* Restore the old consumer */` |
|   13 |  815 | `		pDecoder->xConsumer = xOld;` |
|   13 |  816 | `		pDecoder->pUserData = pOld;` |
|    - |  817 | `		/* Invoke the old consumer on the decoded array */` |
|   13 |  818 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   24 |  819 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  820 | `		ProcJsonConsumer xOld;` |
|    - |  821 | `		ph7_value *pKey;` |
|    - |  822 | `		void *pOld;` |
|    - |  823 | `		/* Object representation*/` |
|   18 |  824 | `		pDecoder->pIn++;` |
|    - |  825 | `		/* Return the object as an associative array */` |
|   18 |  826 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|    3 |  827 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|    - |  828 | `				"JSON Objects are always returned as an associative array"` |
|    - |  829 | `				);` |
|    1 |  830 | `		}` |
|    - |  831 | `		/* Create a working array */` |
|   18 |  832 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   18 |  833 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   18 |  834 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  835 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  836 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  837 | `			return SXERR_ABORT;` |
|    - |  838 | `		}` |
|    - |  839 | `		/* Save the old consumer */` |
|   18 |  840 | `		xOld = pDecoder->xConsumer;` |
|   18 |  841 | `		pOld = pDecoder->pUserData;` |
|    - |  842 | `		/* Set the new consumer */` |
|   18 |  843 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   18 |  844 | `		pDecoder->pUserData = pWorker;` |
|    - |  845 | `		/* Decode the object */` |
|   17 |  846 | `		for(;;){` |
|    - |  847 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  848 | `			 * do this.` |
|    - |  849 | `			 */` |
|   40 |  850 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    5 |  851 | `				pDecoder->pIn++;` |
|    1 |  852 | `			}` |
|   36 |  853 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|   18 |  854 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   16 |  855 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    7 |  856 | `				}` |
|   18 |  857 | `				break;` |
|    - |  858 | `			}` |
|   18 |  859 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   20 |  860 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  861 | `					/* Syntax error,return immediately */` |
|  ! 0 |  862 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  863 | `					return SXERR_ABORT;` |
|    - |  864 | `			}` |
|    - |  865 | `			/* Dequote the key */` |
|   20 |  866 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  867 | `			/* Jump the key and the colon */` |
|   20 |  868 | `			pDecoder->pIn += 2;` |
|    - |  869 | `			/* Recurse and decode the value */` |
|   20 |  870 | `			pDecoder->rec_count++;` |
|   20 |  871 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   20 |  872 | `			pDecoder->rec_count--;` |
|   20 |  873 | `			if( rc == SXERR_ABORT ){` |
|    - |  874 | `				/* Abort processing immediately */` |
|  ! 0 |  875 | `				return SXERR_ABORT;` |
|    - |  876 | `			}` |
|    - |  877 | `			/* Reset the internal buffer of the key */` |
|   20 |  878 | `			ph7_value_reset_string_cursor(pKey);` |
|    - |  879 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    2 |  880 | `		}` |
|    - |  881 | `		/* Restore the old consumer */` |
|   18 |  882 | `		pDecoder->xConsumer = xOld;` |
|   18 |  883 | `		pDecoder->pUserData = pOld;` |
|    - |  884 | `		/* Invoke the old consumer on the decoded object*/` |
|   18 |  885 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - |  886 | `		/* Release the key */` |
|   18 |  887 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|   10 |  888 | `	}else{` |
|    - |  889 | `		/* Unexpected token */` |
|  ! 0 |  890 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - |  891 | `	}` |
|    - |  892 | `	/* Release the worker variable */` |
|   66 |  893 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   66 |  894 | `	return SXRET_OK;` |
|   34 |  895 | `}` |
|    - |  896 | `/*` |
|    - |  897 | ` * The following JSON decoder callback is invoked each time` |
|    - |  898 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - |  899 | ` * is being decoded.` |
|    - |  900 | ` */` |
|   42 |  901 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  902 | `{` |
|   44 |  903 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - |  904 | `	/* Insert the entry */` |
|   44 |  905 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   21 |  906 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - |  907 | `	/* All done */` |
|   44 |  908 | `	return SXRET_OK;` |
|    2 |  909 | `}` |
|    - |  910 | `/*` |
|    - |  911 | ` * Standard JSON decoder callback.` |
|    - |  912 | ` */` |
|   22 |  913 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  914 | `{` |
|    - |  915 | `	/* Return the value directly */` |
|   24 |  916 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|   11 |  917 | `	SXUNUSED(pKey); /* cc warning */` |
|   11 |  918 | `	SXUNUSED(pUserData);` |
|    - |  919 | `	/* All done */` |
|   24 |  920 | `	return SXRET_OK;` |
|    2 |  921 | `}` |
|    - |  922 | `/*` |
|    - |  923 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - |  924 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - |  925 | ` * Parameters` |
|    - |  926 | ` *  $json` |
|    - |  927 | ` *    The json string being decoded.` |
|    - |  928 | ` * $assoc` |
|    - |  929 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - |  930 | ` * $depth` |
|    - |  931 | ` *   User specified recursion depth.` |
|    - |  932 | ` * $options` |
|    - |  933 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - |  934 | ` * (default is to cast large integers as floats)` |
|    - |  935 | ` * Return` |
|    - |  936 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - |  937 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - |  938 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - |  939 | ` */` |
|    - |  940 | `/*` |
|    - |  941 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|    - |  942 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|    - |  943 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|    - |  944 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|    - |  945 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|    - |  946 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|    - |  947 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|    - |  948 | ` */` |
|   36 |  949 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth)` |
|    2 |  950 | `{` |
|   38 |  951 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  952 | `	json_decoder sDecoder;` |
|    - |  953 | `	SySet sToken;` |
|    - |  954 | `	SyLex sLex;` |
|    - |  955 | `	sxi32 rc;` |
|    - |  956 | `	/* Clear JSON error code */` |
|   38 |  957 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  958 | `	/* Tokenize the input */` |
|   38 |  959 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   38 |  960 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   38 |  961 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   38 |  962 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - |  963 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|   16 |  964 | `		SyLexRelease(&sLex);` |
|   16 |  965 | `		SySetRelease(&sToken);` |
|   16 |  966 | `		return pVm->json_rc;` |
|    - |  967 | `	}` |
|    - |  968 | `	/* Fill the decoder */` |
|   24 |  969 | `	sDecoder.pCtx = pCtx;` |
|   24 |  970 | `	sDecoder.pErr = &pVm->json_rc;` |
|   24 |  971 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   24 |  972 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   24 |  973 | `	sDecoder.iFlags = 0;` |
|   24 |  974 | `	if( iAssoc ){` |
|    - |  975 | `		/* Returned objects will be converted into associative arrays */` |
|   22 |  976 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|   10 |  977 | `	}` |
|   24 |  978 | `	sDecoder.rec_depth = 32;` |
|   24 |  979 | `	if( nDepth > 1 && nDepth < 32 ){` |
|    3 |  980 | `		sDecoder.rec_depth = nDepth;` |
|    1 |  981 | `	}` |
|   24 |  982 | `	sDecoder.rec_count = 0;` |
|    - |  983 | `	/* Set a default consumer */` |
|   24 |  984 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   24 |  985 | `	sDecoder.pUserData = 0;` |
|    - |  986 | `	/* Decode the raw JSON input */` |
|   24 |  987 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   24 |  988 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|    - |  989 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|  ! 0 |  990 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 |  991 | `	}` |
|    - |  992 | `	/* Clean-up the mess left behind */` |
|   24 |  993 | `	SyLexRelease(&sLex);` |
|   24 |  994 | `	SySetRelease(&sToken);` |
|   24 |  995 | `	return pVm->json_rc;` |
|   20 |  996 | `}` |
|   38 |  997 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  998 | `{` |
|    - |  999 | `	const char *zIn;` |
|    - | 1000 | `	int nByte;` |
|   40 | 1001 | `	int iAssoc = 0;` |
|   40 | 1002 | `	int nDepth = 32;` |
|   40 | 1003 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1004 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 | 1005 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1006 | `		return PH7_OK;` |
|    - | 1007 | `	}` |
|    - | 1008 | `	/* Extract the JSON string */` |
|   40 | 1009 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   40 | 1010 | `	if( nByte < 1 ){` |
|    - | 1011 | `		/* Empty string,return NULL */` |
|    6 | 1012 | `		ph7_result_null(pCtx);` |
|    6 | 1013 | `		return PH7_OK;` |
|    - | 1014 | `	}` |
|   36 | 1015 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|   24 | 1016 | `		iAssoc = 1;` |
|   11 | 1017 | `	}` |
|   36 | 1018 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    - | 1019 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);` |
|    - | 1020 | `		 * read as int64 so a value above INT_MAX is detected, not truncated. */` |
|   13 | 1021 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);` |
|    - | 1022 | `		/* php clears the json error state before validating $depth, so a caught` |
|    - | 1023 | `		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal` |
|    - | 1024 | `		 * path resets it again inside VmJsonDecodeInput). */` |
|   13 | 1025 | `		pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|   13 | 1026 | `		if( nWant <= 0 ){` |
|    9 | 1027 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1028 | `				"json_decode(): Argument #3 ($depth) must be greater than 0");` |
|    - | 1029 | `		}` |
|    5 | 1030 | `		if( nWant > 2147483647 ){` |
|    3 | 1031 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1032 | `				"json_decode(): Argument #3 ($depth) must be less than 2147483647");` |
|    - | 1033 | `		}` |
|    3 | 1034 | `		nDepth = (int)nWant;` |
|    1 | 1035 | `	}` |
|    - | 1036 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|    - | 1037 | `	 * call-context result; on failure we replace it with NULL. */` |
|   26 | 1038 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth) != JSON_ERROR_NONE ){` |
|    - | 1039 | `		/* Something goes wrong while decoding JSON input.Return NULL. */` |
|   12 | 1040 | `		ph7_result_null(pCtx);` |
|    5 | 1041 | `	}` |
|    - | 1042 | `	/* All done */` |
|   26 | 1043 | `	return PH7_OK;` |
|   21 | 1044 | `}` |
|    - | 1045 | `/*` |
|    - | 1046 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|    - | 1047 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|    - | 1048 | ` * Parameters` |
|    - | 1049 | ` *  $json   The string to validate.` |
|    - | 1050 | ` *  $depth  Maximum nesting depth (clamped to the engine limit of 32).` |
|    - | 1051 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|    - | 1052 | ` * Return` |
|    - | 1053 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|    - | 1054 | ` */` |
|   20 | 1055 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1056 | `{` |
|   21 | 1057 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1058 | `	const char *zIn;` |
|    - | 1059 | `	int nByte;` |
|   21 | 1060 | `	int nDepth = 32;` |
|   21 | 1061 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1062 | `		/* Missing/Invalid argument: not valid JSON */` |
|  ! 0 | 1063 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1064 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1065 | `		return PH7_OK;` |
|    - | 1066 | `	}` |
|    - | 1067 | `	/* Extract the JSON string */` |
|   21 | 1068 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   21 | 1069 | `	if( nByte < 1 ){` |
|    - | 1070 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|    - | 1071 | `		 * silently, json_validate must record the syntax error) */` |
|    3 | 1072 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    3 | 1073 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1074 | `		return PH7_OK;` |
|    - | 1075 | `	}` |
|   19 | 1076 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - | 1077 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */` |
|    9 | 1078 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|    - | 1079 | `		/* Clear the json error state before validating $depth (php parity), so a` |
|    - | 1080 | `		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */` |
|    9 | 1081 | `		pVm->json_rc = JSON_ERROR_NONE;` |
|    9 | 1082 | `		if( nWant <= 0 ){` |
|    5 | 1083 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1084 | `				"json_validate(): Argument #2 ($depth) must be greater than 0");` |
|    - | 1085 | `		}` |
|    5 | 1086 | `		if( nWant > 2147483647 ){` |
|    3 | 1087 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1088 | `				"json_validate(): Argument #2 ($depth) must be less than 2147483647");` |
|    - | 1089 | `		}` |
|    3 | 1090 | `		nDepth = (int)nWant;` |
|    1 | 1091 | `	}` |
|    - | 1092 | `	/* apArg[2] ($flags) is accepted and ignored: no decode flag is implemented.` |
|    - | 1093 | `	 * Decode in associative mode so the "objects are returned as an array" warning is` |
|    - | 1094 | `	 * not raised - the decoded value is discarded, only its validity matters. */` |
|   13 | 1095 | `	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth) == JSON_ERROR_NONE);` |
|   13 | 1096 | `	return PH7_OK;` |
|   11 | 1097 | `}` |
|    - | 1098 |  |
