# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 428/557 lines (76.84%)

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
|  400 |   83 | `static sxi32 VmJsonEncode(` |
|    - |   84 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |   85 | `	json_private_data *pData /* Context data */` |
|    2 |   86 | `	){` |
|  402 |   87 | `		ph7_context *pCtx = pData->pCtx;` |
|  402 |   88 | `		int iFlags = pData->iFlags;` |
|    - |   89 | `		int nByte;` |
|  402 |   90 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |   91 | `			/* null */` |
|  ! 0 |   92 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|  400 |   93 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   15 |   94 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |   95 | `			int iLen;` |
|    - |   96 | `			/* true/false */` |
|   15 |   97 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   15 |   98 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
|  394 |   99 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|  169 |  100 | `			if( ph7_value_is_float(pIn) ){` |
|    - |  101 | `				/* php's json float output follows serialize_precision` |
|    - |  102 | `				 * (shortest round-trip, like serialize/var_export), NOT the` |
|    - |  103 | `				 * echo/cast precision of 14 — with a lowercase exponent` |
|    - |  104 | `				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,` |
|    - |  105 | `				 * 1.0 -> 1, -0.0 -> -0. */` |
|   17 |  106 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    9 |  107 | `			}else{` |
|    - |  108 | `				const char *zNum;` |
|    - |  109 | `				/* Get a string representation of the number */` |
|   97 |  110 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|   97 |  111 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|    1 |  112 | `			}` |
|  332 |  113 | `		}else if( ph7_value_is_string(pIn) ){` |
|  112 |  114 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |  115 | `				/* Encodes numeric strings as numbers (same float shapes). */` |
|    5 |  116 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    5 |  117 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    3 |  118 | `			}else{` |
|    - |  119 | `				const char *zIn,*zEnd;` |
|    - |  120 | `				int c;` |
|    - |  121 | `				/* Encode the string */` |
|  108 |  122 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|  108 |  123 | `				zEnd = &zIn[nByte];` |
|    - |  124 | `				/* Append the double quote */` |
|  108 |  125 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|  150 |  126 | `				for(;;){` |
|  302 |  127 | `					if( zIn >= zEnd ){` |
|    - |  128 | `						/* No more input to process */` |
|  108 |  129 | `						break;` |
|    - |  130 | `					}` |
|  196 |  131 | `					c = zIn[0];` |
|    - |  132 | `					/* Advance the stream cursor */` |
|  196 |  133 | `					zIn++;` |
|  196 |  134 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |  135 | `						/* All < and > are converted to \u003C and \u003E */` |
|  ! 0 |  136 | `						if( c == '<' ){` |
|  ! 0 |  137 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1));` |
|  ! 0 |  138 | `						}else{` |
|  ! 0 |  139 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1));` |
|    - |  140 | `						}` |
|  ! 0 |  141 | `						continue;` |
|  196 |  142 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |  143 | `						/* All &s are converted to \u0026.  */` |
|  ! 0 |  144 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1));` |
|  ! 0 |  145 | `						continue;` |
|  196 |  146 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  147 | `						/* All ' are converted to \u0027.   */` |
|  ! 0 |  148 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1));` |
|  ! 0 |  149 | `						continue;` |
|  196 |  150 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  151 | `						/* All " are converted to \u0022. */` |
|  ! 0 |  152 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1));` |
|  ! 0 |  153 | `						continue;` |
|    - |  154 | `					}` |
|  196 |  155 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|    - |  156 | `						/* Unescape the character */` |
|  ! 0 |  157 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  ! 0 |  158 | `					}` |
|    - |  159 | `					/* Append character verbatim */` |
|  196 |  160 | `					JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    2 |  161 | `				}` |
|    - |  162 | `				/* Append the double quote */` |
|  108 |  163 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|    2 |  164 | `			}` |
|  221 |  165 | `		}else if( ph7_value_is_array(pIn) ){` |
|    - |  166 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|    - |  167 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|    - |  168 | `			 * object with stringified keys (PHP semantics). */` |
|  220 |  169 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|  110 |  170 | `				\|\| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);` |
|  111 |  171 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|  111 |  172 | `			int c = isObject ? '{' : '[';` |
|  111 |  173 | `			int d = isObject ? '}' : ']';` |
|    - |  174 | `			/* Encode the array */` |
|  111 |  175 | `			pData->isObject = isObject;` |
|  111 |  176 | `			pData->isFirst = 1;` |
|    - |  177 | `			/* Append the square bracket or curly braces */` |
|  111 |  178 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    - |  179 | `			/* Iterate throw array entries */` |
|  111 |  180 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  181 | `			/* Bail if a nested append ran out of memory before the closer */` |
|  111 |  182 | `			if( pData->oom ){` |
|  ! 0 |  183 | `				return PH7_OK;` |
|    - |  184 | `			}` |
|    - |  185 | `			/* Append the closing square bracket or curly braces */` |
|  111 |  186 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|  111 |  187 | `			pData->isObject = savedObject;` |
|  111 |  188 | `		}else if( ph7_value_is_object(pIn) ){` |
|   56 |  189 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   56 |  190 | `			ph7_vm *pVm = pIn->pVm;` |
|   56 |  191 | `			ph7_class_method *pMethod = 0;` |
|    - |  192 | `			/* If the object implements JsonSerializable, encode the value` |
|    - |  193 | `			 * returned by jsonSerialize() instead of its public properties. */` |
|   54 |  194 | `			if( pVm->pJsonSerializableClass` |
|   56 |  195 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|   15 |  196 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|    7 |  197 | `			}` |
|   56 |  198 | `			if( pMethod ){` |
|    - |  199 | `				ph7_value sResult;` |
|    - |  200 | `				sxi32 rc;` |
|   15 |  201 | `				PH7_MemObjInit(pVm,&sResult);` |
|   15 |  202 | `				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|   15 |  203 | `				if( rc == PH7_EXCEPTION ){` |
|    - |  204 | `					/* Let jsonSerialize()'s throw propagate */` |
|    5 |  205 | `					PH7_MemObjRelease(&sResult);` |
|    5 |  206 | `					pData->exc = 1;` |
|    5 |  207 | `					return PH7_EXCEPTION;` |
|    - |  208 | `				}` |
|    - |  209 | `				/* Encode the returned value [scalar/array/object] */` |
|   11 |  210 | `				pData->nRecCount++;` |
|   11 |  211 | `				VmJsonEncode(&sResult,pData);` |
|   11 |  212 | `				pData->nRecCount--;` |
|   11 |  213 | `				PH7_MemObjRelease(&sResult);` |
|   11 |  214 | `				if( pData->exc ){` |
|  ! 0 |  215 | `					return PH7_EXCEPTION;` |
|    - |  216 | `				}` |
|   11 |  217 | `				if( pData->oom ){` |
|  ! 0 |  218 | `					return PH7_OK;` |
|    - |  219 | `				}` |
|    6 |  220 | `			}else{` |
|    - |  221 | `				/* Encode the class instance */` |
|   42 |  222 | `				pData->isFirst = 1;` |
|    - |  223 | `				/* Append the curly braces */` |
|   42 |  224 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|    - |  225 | `				/* Iterate throw class attribute */` |
|   42 |  226 | `				ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|   42 |  227 | `				if( pData->oom ){` |
|  ! 0 |  228 | `					return PH7_OK;` |
|    - |  229 | `				}` |
|    - |  230 | `				/* Append the closing curly braces  */` |
|   42 |  231 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|    - |  232 | `			}` |
|   27 |  233 | `		}else{` |
|    - |  234 | `			/* Can't happen */` |
|  ! 0 |  235 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    - |  236 | `		}` |
|    - |  237 | `		/* All done */` |
|  398 |  238 | `		return PH7_OK;` |
|  202 |  239 | `}` |
|    - |  240 | `/*` |
|    - |  241 | ` * The following walker callback is invoked each time we need` |
|    - |  242 | ` * to encode an array to JSON.` |
|    - |  243 | ` */` |
|  202 |  244 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    1 |  245 | `{` |
|  203 |  246 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  203 |  247 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  248 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  249 | `		return PH7_OK;` |
|    - |  250 | `	}` |
|  203 |  251 | `	if( !pJson->isFirst ){` |
|    - |  252 | `		/* Append the colon first */` |
|  105 |  253 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   52 |  254 | `	}` |
|  203 |  255 | `	if( pJson->isObject ){` |
|    - |  256 | `		/* Outputs an object rather than an array */` |
|    - |  257 | `		const char *zKey;` |
|    - |  258 | `		int nByte;` |
|    - |  259 | `		/* Extract a string representation of the key */` |
|   81 |  260 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  261 | `		/* Append the quoted key and the colon (checked, so an OOM here is caught` |
|    - |  262 | `		 * rather than silently truncating; matches the prior "%.*s" emit byte for` |
|    - |  263 | `		 * byte — keys are not JSON-escaped, a pre-existing behavior). */` |
|   81 |  264 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|   81 |  265 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zKey,nByte));` |
|   81 |  266 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|   40 |  267 | `	}` |
|    - |  268 | `	/* Encode the value */` |
|  203 |  269 | `	pJson->nRecCount++;` |
|  203 |  270 | `	VmJsonEncode(pValue,pJson);` |
|  203 |  271 | `	pJson->nRecCount--;` |
|  203 |  272 | `	pJson->isFirst = 0;` |
|  203 |  273 | `	return PH7_OK;` |
|  102 |  274 | `}` |
|    - |  275 | `/*` |
|    - |  276 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  277 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  278 | ` */` |
|   62 |  279 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|    2 |  280 | `{` |
|   64 |  281 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|   64 |  282 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  283 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  284 | `		return PH7_OK;` |
|    - |  285 | `	}` |
|   64 |  286 | `	if( !pJson->isFirst ){` |
|    - |  287 | `		/* Append the colon first */` |
|   25 |  288 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   12 |  289 | `	}` |
|    - |  290 | `	/* Append the quoted attribute name and the colon (checked; matches the prior` |
|    - |  291 | `	 * "%s" emit byte for byte — attribute names are not JSON-escaped). */` |
|   64 |  292 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|   64 |  293 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zAttr,-1));` |
|   64 |  294 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|    - |  295 | `	/* Encode the value */` |
|   64 |  296 | `	pJson->nRecCount++;` |
|   64 |  297 | `	VmJsonEncode(pValue,pJson);` |
|   64 |  298 | `	pJson->nRecCount--;` |
|   64 |  299 | `	pJson->isFirst = 0;` |
|   64 |  300 | `	return PH7_OK;` |
|   33 |  301 | `}` |
|    - |  302 | `/*` |
|    - |  303 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  304 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  305 | ` * Parameters` |
|    - |  306 | ` *  $value` |
|    - |  307 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  308 | ` * $options` |
|    - |  309 | ` *  Bitmask consisting of:` |
|    - |  310 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  311 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  312 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  313 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  314 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  315 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  316 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  317 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  318 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  319 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  320 | ` * Return` |
|    - |  321 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  322 | ` */` |
|  126 |  323 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  324 | `{` |
|    - |  325 | `	json_private_data sJson;` |
|    - |  326 | `	sxi32 rc;` |
|  128 |  327 | `	if( nArg < 1 ){` |
|    - |  328 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  329 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  330 | `		return PH7_OK;` |
|    - |  331 | `	}` |
|    - |  332 | `	/* Prepare the JSON data */` |
|  128 |  333 | `	sJson.nRecCount = 0;` |
|  128 |  334 | `	sJson.pCtx = pCtx;` |
|  128 |  335 | `	sJson.isFirst = 1;` |
|  128 |  336 | `	sJson.iFlags = 0;` |
|  128 |  337 | `	sJson.exc = 0;` |
|  128 |  338 | `	sJson.oom = 0;` |
|  128 |  339 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  340 | `		/* Extract option flags */` |
|    5 |  341 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|    2 |  342 | `	}` |
|    - |  343 | `	/* Perform the encoding operation */` |
|  128 |  344 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|  128 |  345 | `	if( sJson.oom ){` |
|    - |  346 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|    - |  347 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|  ! 0 |  348 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  349 | `	}` |
|  128 |  350 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|    - |  351 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|    5 |  352 | `		return PH7_EXCEPTION;` |
|    - |  353 | `	}` |
|    - |  354 | `	/* All done */` |
|  124 |  355 | `	return PH7_OK;` |
|   65 |  356 | `}` |
|    - |  357 | `#undef JSON_EMIT` |
|    - |  358 | `/*` |
|    - |  359 | ` * int json_last_error(void)` |
|    - |  360 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  361 | ` * Parameters` |
|    - |  362 | ` *  None` |
|    - |  363 | ` * Return` |
|    - |  364 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  365 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  366 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  367 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  368 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  369 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  370 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  371 | ` */` |
|   12 |  372 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  373 | `{` |
|   14 |  374 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  375 | `	/* Return the error code */` |
|   14 |  376 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    6 |  377 | `	SXUNUSED(nArg); /* cc warning */` |
|    6 |  378 | `	SXUNUSED(apArg);` |
|   14 |  379 | `	return PH7_OK;` |
|    2 |  380 | `}` |
|    - |  381 | `/*` |
|    - |  382 | ` * string json_last_error_msg(void)` |
|    - |  383 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|    - |  384 | ` * Parameters` |
|    - |  385 | ` *  None` |
|    - |  386 | ` * Return` |
|    - |  387 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|    - |  388 | ` *  code, or "No error" if no error has occurred.` |
|    - |  389 | ` */` |
|    4 |  390 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  391 | `{` |
|    5 |  392 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  393 | `	const char *zMsg;` |
|    5 |  394 | `	switch( pVm->json_rc ){` |
|    1 |  395 | `	case JSON_ERROR_NONE:` |
|    3 |  396 | `		zMsg = "No error";` |
|    3 |  397 | `		break;` |
|  ! 0 |  398 | `	case JSON_ERROR_DEPTH:` |
|  ! 0 |  399 | `		zMsg = "Maximum stack depth exceeded";` |
|  ! 0 |  400 | `		break;` |
|  ! 0 |  401 | `	case JSON_ERROR_STATE_MISMATCH:` |
|  ! 0 |  402 | `		zMsg = "State mismatch (invalid or malformed JSON)";` |
|  ! 0 |  403 | `		break;` |
|  ! 0 |  404 | `	case JSON_ERROR_CTRL_CHAR:` |
|  ! 0 |  405 | `		zMsg = "Control character error, possibly incorrectly encoded";` |
|  ! 0 |  406 | `		break;` |
|    1 |  407 | `	case JSON_ERROR_SYNTAX:` |
|    3 |  408 | `		zMsg = "Syntax error";` |
|    3 |  409 | `		break;` |
|  ! 0 |  410 | `	case JSON_ERROR_UTF8:` |
|  ! 0 |  411 | `		zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|  ! 0 |  412 | `		break;` |
|  ! 0 |  413 | `	default:` |
|  ! 0 |  414 | `		zMsg = "Unknown error";` |
|  ! 0 |  415 | `		break;` |
|    - |  416 | `	}` |
|    5 |  417 | `	ph7_result_string(pCtx,zMsg,-1/* Compute length automatically */);` |
|    2 |  418 | `	SXUNUSED(nArg); /* cc warning */` |
|    2 |  419 | `	SXUNUSED(apArg);` |
|    5 |  420 | `	return PH7_OK;` |
|    1 |  421 | `}` |
|    - |  422 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  423 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  424 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  425 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  426 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  427 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  428 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  429 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  430 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  431 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  432 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  433 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  434 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  435 | `/*` |
|    - |  436 | ` * Tokenize an entire JSON input.` |
|    - |  437 | ` * Get a single low-level token from the input file.` |
|    - |  438 | ` * Update the stream pointer so that it points to the first` |
|    - |  439 | ` * character beyond the extracted token.` |
|    - |  440 | ` */` |
|  160 |  441 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    2 |  442 | `{` |
|  162 |  443 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  444 | `	SyString *pStr;` |
|    - |  445 | `	int c;` |
|    - |  446 | `	/* Ignore leading white spaces */` |
|  166 |  447 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  448 | `		/* Advance the stream cursor */` |
|    6 |  449 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  450 | `			/* Update line counter */` |
|  ! 0 |  451 | `			pStream->nLine++;` |
|  ! 0 |  452 | `		}` |
|    6 |  453 | `		pStream->zText++;` |
|    2 |  454 | `	}` |
|  162 |  455 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  456 | `		/* End of input reached */` |
|  ! 0 |  457 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  458 | `		return SXERR_EOF;` |
|    - |  459 | `	}` |
|    - |  460 | `	/* Record token starting position and line */` |
|  162 |  461 | `	pToken->nLine = pStream->nLine;` |
|  162 |  462 | `	pToken->pUserData = 0;` |
|  162 |  463 | `	pStr = &pToken->sData;` |
|  162 |  464 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  160 |  465 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|  110 |  466 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  467 | `			/* Single character */` |
|   94 |  468 | `			c = pStream->zText[0];` |
|    - |  469 | `			/* Set token type */` |
|   94 |  470 | `			switch(c){` |
|   13 |  471 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   22 |  472 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|   16 |  473 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|   13 |  474 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   20 |  475 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   17 |  476 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  477 | `			default:` |
|  ! 0 |  478 | `				break;` |
|    - |  479 | `			}` |
|    - |  480 | `			/* Advance the stream cursor */` |
|   94 |  481 | `			pStream->zText++;` |
|  116 |  482 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  483 | `		/* JSON string */` |
|   26 |  484 | `		pStream->zText++;` |
|   26 |  485 | `		pStr->zString++;` |
|    - |  486 | `		/* Delimit the string */` |
|   72 |  487 | `		while( pStream->zText < pStream->zEnd ){` |
|   72 |  488 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   26 |  489 | `				break;` |
|    - |  490 | `			}` |
|   48 |  491 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  492 | `				/* Update line counter */` |
|  ! 0 |  493 | `				pStream->nLine++;` |
|  ! 0 |  494 | `			}` |
|   48 |  495 | `			pStream->zText++;` |
|    2 |  496 | `		}` |
|   26 |  497 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  498 | `			/* Missing closing '"' */` |
|  ! 0 |  499 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  500 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  501 | `		}else{` |
|   26 |  502 | `			pToken->nType = JSON_TK_STR;` |
|   26 |  503 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    2 |  504 | `		}` |
|   58 |  505 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    - |  506 | `		/* Number */` |
|   31 |  507 | `		pStream->zText++;` |
|   31 |  508 | `		pToken->nType = JSON_TK_NUM;` |
|   31 |  509 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  510 | `			pStream->zText++;` |
|  ! 0 |  511 | `		}` |
|   31 |  512 | `		if( pStream->zText < pStream->zEnd ){` |
|   31 |  513 | `			c = pStream->zText[0];` |
|   31 |  514 | `			if( c == '.' ){` |
|    - |  515 | `					/* Real number */` |
|  ! 0 |  516 | `					pStream->zText++;` |
|  ! 0 |  517 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  518 | `						pStream->zText++;` |
|  ! 0 |  519 | `					}` |
|  ! 0 |  520 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  521 | `						c = pStream->zText[0];` |
|  ! 0 |  522 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  523 | `							pStream->zText++;` |
|  ! 0 |  524 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  525 | `								c = pStream->zText[0];` |
|  ! 0 |  526 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  527 | `									pStream->zText++;` |
|  ! 0 |  528 | `								}` |
|  ! 0 |  529 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  530 | `									pStream->zText++;` |
|  ! 0 |  531 | `								}` |
|  ! 0 |  532 | `							}` |
|  ! 0 |  533 | `						}` |
|  ! 0 |  534 | `					}` |
|   31 |  535 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  536 | `					/* Real number */` |
|  ! 0 |  537 | `					pStream->zText++;` |
|  ! 0 |  538 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  539 | `						c = pStream->zText[0];` |
|  ! 0 |  540 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  541 | `							pStream->zText++;` |
|  ! 0 |  542 | `						}` |
|  ! 0 |  543 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  544 | `							pStream->zText++;` |
|  ! 0 |  545 | `						}` |
|  ! 0 |  546 | `					}` |
|  ! 0 |  547 | `				}` |
|   16 |  548 | `			}` |
|   37 |  549 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|   12 |  550 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  551 | `			/* boolean true */` |
|  ! 0 |  552 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  553 | `			/* Advance the stream cursor */` |
|  ! 0 |  554 | `			pStream->zText += sizeof("true")-1;` |
|   22 |  555 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|   12 |  556 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  557 | `			/* boolean false */` |
|  ! 0 |  558 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  559 | `			/* Advance the stream cursor */` |
|  ! 0 |  560 | `			pStream->zText += sizeof("false")-1;` |
|   22 |  561 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|   12 |  562 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  563 | `			/* NULL */` |
|  ! 0 |  564 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  565 | `			/* Advance the stream cursor */` |
|  ! 0 |  566 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  567 | `	}else{` |
|    - |  568 | `		/* Unexpected token */` |
|   16 |  569 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  570 | `		/* Advance the stream cursor */` |
|   16 |  571 | `		pStream->zText++;` |
|   16 |  572 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  573 | `		/* Abort processing immediatley */` |
|   16 |  574 | `		return SXERR_ABORT;` |
|    - |  575 | `	}` |
|    - |  576 | `	/* record token length */` |
|  148 |  577 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  148 |  578 | `	if( pToken->nType == JSON_TK_STR ){` |
|   26 |  579 | `		pStr->nByte--;` |
|   12 |  580 | `	}` |
|    - |  581 | `	/* Return to the lexer */` |
|  148 |  582 | `	return SXRET_OK;` |
|   82 |  583 | `}` |
|    - |  584 | `/*` |
|    - |  585 | ` * JSON decoded input consumer callback signature.` |
|    - |  586 | ` */` |
|    - |  587 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  588 | `/*` |
|    - |  589 | ` * JSON decoder state is kept in the following structure.` |
|    - |  590 | ` */` |
|    - |  591 | `typedef struct json_decoder json_decoder;` |
|    - |  592 | `struct json_decoder` |
|    - |  593 | `{` |
|    - |  594 | `	ph7_context *pCtx; /* Call context */` |
|    - |  595 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  596 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  597 | `	int iFlags;        /* Configuration flags */` |
|    - |  598 | `	SyToken *pIn;      /* Token stream */` |
|    - |  599 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  600 | `	int rec_depth;     /* Recursion limit */` |
|    - |  601 | `	int rec_count;     /* Current nesting level */` |
|    - |  602 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  603 | `};` |
|    - |  604 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  605 | `/* Forward declaration */` |
|    - |  606 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  607 | `/*` |
|    - |  608 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  609 | ` * the result in the given ph7_value.` |
|    - |  610 | ` */` |
|   24 |  611 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    2 |  612 | `{` |
|   26 |  613 | `	const char *zIn = pStr->zString;` |
|   26 |  614 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  615 | `	const char *zCur;` |
|    - |  616 | `	int c;` |
|    - |  617 | `	/* Mark the value as a string */` |
|   26 |  618 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|   12 |  619 | `	for(;;){` |
|   26 |  620 | `		zCur = zIn;` |
|   72 |  621 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   48 |  622 | `			zIn++;` |
|    2 |  623 | `		}` |
|   26 |  624 | `		if( zIn > zCur ){` |
|    - |  625 | `			/* Append chunk verbatim */` |
|   26 |  626 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|   12 |  627 | `		}` |
|   26 |  628 | `		zIn++;` |
|   26 |  629 | `		if( zIn >= zEnd ){` |
|    - |  630 | `			/* End of the input reached */` |
|   26 |  631 | `			break;` |
|    - |  632 | `		}` |
|  ! 0 |  633 | `		c = zIn[0];` |
|    - |  634 | `		/* Unescape the character */` |
|  ! 0 |  635 | `		switch(c){` |
|  ! 0 |  636 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  637 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  638 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  639 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  640 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  641 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  642 | `		default:` |
|  ! 0 |  643 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  644 | `			break;` |
|    - |  645 | `		}` |
|    - |  646 | `		/* Advance the stream cursor */` |
|  ! 0 |  647 | `		zIn++;` |
|  ! 0 |  648 | `	}` |
|   26 |  649 | `}` |
|    - |  650 | `/*` |
|    - |  651 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  652 | ` * According to wikipedia` |
|    - |  653 | ` * JSON's basic types are:` |
|    - |  654 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  655 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  656 | ` *   Boolean (true or false)` |
|    - |  657 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  658 | ` *    do not need to be of the same type)` |
|    - |  659 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  660 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  661 | ` *     be distinct from each other)` |
|    - |  662 | ` *   null (empty)` |
|    - |  663 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  664 | ` */` |
|   64 |  665 | `static sxi32 VmJsonDecode(` |
|    - |  666 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  667 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    2 |  668 | `	){` |
|    - |  669 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  670 | `	sxi32 rc;` |
|    - |  671 | `	/* Check if we do not nest to much */` |
|   66 |  672 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  673 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  674 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  675 | `		return SXERR_ABORT;` |
|    - |  676 | `	}` |
|   66 |  677 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  678 | `		/* Scalar value */` |
|   38 |  679 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   38 |  680 | `		if( pWorker == 0 ){` |
|  ! 0 |  681 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  682 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  683 | `			return SXERR_ABORT;` |
|    - |  684 | `		}` |
|    - |  685 | `		/* Reflect the JSON image */` |
|   38 |  686 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  687 | `			/* Nullify the value.*/` |
|  ! 0 |  688 | `			ph7_value_null(pWorker);` |
|   38 |  689 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  690 | `			/* Boolean value */` |
|  ! 0 |  691 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   38 |  692 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   31 |  693 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  694 | `			/*` |
|    - |  695 | `			 * Numeric value.` |
|    - |  696 | `			 * Get a string representation first then try to get a numeric` |
|    - |  697 | `			 * value.` |
|    - |  698 | `			 */` |
|   31 |  699 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  700 | `			/* Obtain a numeric representation */` |
|   31 |  701 | `			PH7_MemObjToNumeric(pWorker);` |
|   16 |  702 | `		}else{` |
|    - |  703 | `			/* Dequote the string */` |
|    8 |  704 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  705 | `		}` |
|    - |  706 | `		/* Invoke the consumer callback */` |
|   38 |  707 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   38 |  708 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  709 | `			return SXERR_ABORT;` |
|    - |  710 | `		}` |
|    - |  711 | `		/* All done,advance the stream cursor */` |
|   38 |  712 | `		pDecoder->pIn++;` |
|   48 |  713 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  714 | `		ProcJsonConsumer xOld;` |
|    - |  715 | `		void *pOld;` |
|    - |  716 | `		/* Array representation*/` |
|   13 |  717 | `		pDecoder->pIn++;` |
|    - |  718 | `		/* Create a working array */` |
|   13 |  719 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   13 |  720 | `		if( pWorker == 0 ){` |
|  ! 0 |  721 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  722 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  723 | `			return SXERR_ABORT;` |
|    - |  724 | `		}` |
|    - |  725 | `		/* Save the old consumer */` |
|   13 |  726 | `		xOld = pDecoder->xConsumer;` |
|   13 |  727 | `		pOld = pDecoder->pUserData;` |
|    - |  728 | `		/* Set the new consumer */` |
|   13 |  729 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   13 |  730 | `		pDecoder->pUserData = pWorker;` |
|    - |  731 | `		/* Decode the array */` |
|   18 |  732 | `		for(;;){` |
|    - |  733 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  734 | `			 * do this.` |
|    - |  735 | `			 */` |
|   49 |  736 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   13 |  737 | `				pDecoder->pIn++;` |
|    1 |  738 | `			}` |
|   37 |  739 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|   13 |  740 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   13 |  741 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    6 |  742 | `				}` |
|   13 |  743 | `				break;` |
|    - |  744 | `			}` |
|    - |  745 | `			/* Recurse and decode the entry */` |
|   25 |  746 | `			pDecoder->rec_count++;` |
|   25 |  747 | `			rc = VmJsonDecode(pDecoder,0);` |
|   25 |  748 | `			pDecoder->rec_count--;` |
|   25 |  749 | `			if( rc == SXERR_ABORT ){` |
|    - |  750 | `				/* Abort processing immediately */` |
|  ! 0 |  751 | `				return SXERR_ABORT;` |
|    - |  752 | `			}` |
|    - |  753 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   25 |  754 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   24 |  755 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  756 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  757 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  758 | `					return SXERR_ABORT;` |
|    - |  759 | `			}` |
|    1 |  760 | `		}` |
|    - |  761 | `		/* Restore the old consumer */` |
|   13 |  762 | `		pDecoder->xConsumer = xOld;` |
|   13 |  763 | `		pDecoder->pUserData = pOld;` |
|    - |  764 | `		/* Invoke the old consumer on the decoded array */` |
|   13 |  765 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   24 |  766 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  767 | `		ProcJsonConsumer xOld;` |
|    - |  768 | `		ph7_value *pKey;` |
|    - |  769 | `		void *pOld;` |
|    - |  770 | `		/* Object representation*/` |
|   18 |  771 | `		pDecoder->pIn++;` |
|    - |  772 | `		/* Return the object as an associative array */` |
|   18 |  773 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|    3 |  774 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|    - |  775 | `				"JSON Objects are always returned as an associative array"` |
|    - |  776 | `				);` |
|    1 |  777 | `		}` |
|    - |  778 | `		/* Create a working array */` |
|   18 |  779 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   18 |  780 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   18 |  781 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  782 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  783 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  784 | `			return SXERR_ABORT;` |
|    - |  785 | `		}` |
|    - |  786 | `		/* Save the old consumer */` |
|   18 |  787 | `		xOld = pDecoder->xConsumer;` |
|   18 |  788 | `		pOld = pDecoder->pUserData;` |
|    - |  789 | `		/* Set the new consumer */` |
|   18 |  790 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   18 |  791 | `		pDecoder->pUserData = pWorker;` |
|    - |  792 | `		/* Decode the object */` |
|   17 |  793 | `		for(;;){` |
|    - |  794 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  795 | `			 * do this.` |
|    - |  796 | `			 */` |
|   40 |  797 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    5 |  798 | `				pDecoder->pIn++;` |
|    1 |  799 | `			}` |
|   36 |  800 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|   18 |  801 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   16 |  802 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    7 |  803 | `				}` |
|   18 |  804 | `				break;` |
|    - |  805 | `			}` |
|   18 |  806 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   20 |  807 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  808 | `					/* Syntax error,return immediately */` |
|  ! 0 |  809 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  810 | `					return SXERR_ABORT;` |
|    - |  811 | `			}` |
|    - |  812 | `			/* Dequote the key */` |
|   20 |  813 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  814 | `			/* Jump the key and the colon */` |
|   20 |  815 | `			pDecoder->pIn += 2;` |
|    - |  816 | `			/* Recurse and decode the value */` |
|   20 |  817 | `			pDecoder->rec_count++;` |
|   20 |  818 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   20 |  819 | `			pDecoder->rec_count--;` |
|   20 |  820 | `			if( rc == SXERR_ABORT ){` |
|    - |  821 | `				/* Abort processing immediately */` |
|  ! 0 |  822 | `				return SXERR_ABORT;` |
|    - |  823 | `			}` |
|    - |  824 | `			/* Reset the internal buffer of the key */` |
|   20 |  825 | `			ph7_value_reset_string_cursor(pKey);` |
|    - |  826 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    2 |  827 | `		}` |
|    - |  828 | `		/* Restore the old consumer */` |
|   18 |  829 | `		pDecoder->xConsumer = xOld;` |
|   18 |  830 | `		pDecoder->pUserData = pOld;` |
|    - |  831 | `		/* Invoke the old consumer on the decoded object*/` |
|   18 |  832 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - |  833 | `		/* Release the key */` |
|   18 |  834 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|   10 |  835 | `	}else{` |
|    - |  836 | `		/* Unexpected token */` |
|  ! 0 |  837 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - |  838 | `	}` |
|    - |  839 | `	/* Release the worker variable */` |
|   66 |  840 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   66 |  841 | `	return SXRET_OK;` |
|   34 |  842 | `}` |
|    - |  843 | `/*` |
|    - |  844 | ` * The following JSON decoder callback is invoked each time` |
|    - |  845 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - |  846 | ` * is being decoded.` |
|    - |  847 | ` */` |
|   42 |  848 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  849 | `{` |
|   44 |  850 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - |  851 | `	/* Insert the entry */` |
|   44 |  852 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   21 |  853 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - |  854 | `	/* All done */` |
|   44 |  855 | `	return SXRET_OK;` |
|    2 |  856 | `}` |
|    - |  857 | `/*` |
|    - |  858 | ` * Standard JSON decoder callback.` |
|    - |  859 | ` */` |
|   22 |  860 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  861 | `{` |
|    - |  862 | `	/* Return the value directly */` |
|   24 |  863 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|   11 |  864 | `	SXUNUSED(pKey); /* cc warning */` |
|   11 |  865 | `	SXUNUSED(pUserData);` |
|    - |  866 | `	/* All done */` |
|   24 |  867 | `	return SXRET_OK;` |
|    2 |  868 | `}` |
|    - |  869 | `/*` |
|    - |  870 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - |  871 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - |  872 | ` * Parameters` |
|    - |  873 | ` *  $json` |
|    - |  874 | ` *    The json string being decoded.` |
|    - |  875 | ` * $assoc` |
|    - |  876 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - |  877 | ` * $depth` |
|    - |  878 | ` *   User specified recursion depth.` |
|    - |  879 | ` * $options` |
|    - |  880 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - |  881 | ` * (default is to cast large integers as floats)` |
|    - |  882 | ` * Return` |
|    - |  883 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - |  884 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - |  885 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - |  886 | ` */` |
|    - |  887 | `/*` |
|    - |  888 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|    - |  889 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|    - |  890 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|    - |  891 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|    - |  892 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|    - |  893 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|    - |  894 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|    - |  895 | ` */` |
|   36 |  896 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth)` |
|    2 |  897 | `{` |
|   38 |  898 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  899 | `	json_decoder sDecoder;` |
|    - |  900 | `	SySet sToken;` |
|    - |  901 | `	SyLex sLex;` |
|    - |  902 | `	sxi32 rc;` |
|    - |  903 | `	/* Clear JSON error code */` |
|   38 |  904 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  905 | `	/* Tokenize the input */` |
|   38 |  906 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   38 |  907 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   38 |  908 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   38 |  909 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - |  910 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|   16 |  911 | `		SyLexRelease(&sLex);` |
|   16 |  912 | `		SySetRelease(&sToken);` |
|   16 |  913 | `		return pVm->json_rc;` |
|    - |  914 | `	}` |
|    - |  915 | `	/* Fill the decoder */` |
|   24 |  916 | `	sDecoder.pCtx = pCtx;` |
|   24 |  917 | `	sDecoder.pErr = &pVm->json_rc;` |
|   24 |  918 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   24 |  919 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   24 |  920 | `	sDecoder.iFlags = 0;` |
|   24 |  921 | `	if( iAssoc ){` |
|    - |  922 | `		/* Returned objects will be converted into associative arrays */` |
|   22 |  923 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|   10 |  924 | `	}` |
|   24 |  925 | `	sDecoder.rec_depth = 32;` |
|   24 |  926 | `	if( nDepth > 1 && nDepth < 32 ){` |
|    3 |  927 | `		sDecoder.rec_depth = nDepth;` |
|    1 |  928 | `	}` |
|   24 |  929 | `	sDecoder.rec_count = 0;` |
|    - |  930 | `	/* Set a default consumer */` |
|   24 |  931 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   24 |  932 | `	sDecoder.pUserData = 0;` |
|    - |  933 | `	/* Decode the raw JSON input */` |
|   24 |  934 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   24 |  935 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|    - |  936 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|  ! 0 |  937 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 |  938 | `	}` |
|    - |  939 | `	/* Clean-up the mess left behind */` |
|   24 |  940 | `	SyLexRelease(&sLex);` |
|   24 |  941 | `	SySetRelease(&sToken);` |
|   24 |  942 | `	return pVm->json_rc;` |
|   20 |  943 | `}` |
|   38 |  944 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  945 | `{` |
|    - |  946 | `	const char *zIn;` |
|    - |  947 | `	int nByte;` |
|   40 |  948 | `	int iAssoc = 0;` |
|   40 |  949 | `	int nDepth = 32;` |
|   40 |  950 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  951 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 |  952 | `		ph7_result_null(pCtx);` |
|  ! 0 |  953 | `		return PH7_OK;` |
|    - |  954 | `	}` |
|    - |  955 | `	/* Extract the JSON string */` |
|   40 |  956 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   40 |  957 | `	if( nByte < 1 ){` |
|    - |  958 | `		/* Empty string,return NULL */` |
|    6 |  959 | `		ph7_result_null(pCtx);` |
|    6 |  960 | `		return PH7_OK;` |
|    - |  961 | `	}` |
|   36 |  962 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|   24 |  963 | `		iAssoc = 1;` |
|   11 |  964 | `	}` |
|   36 |  965 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    - |  966 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);` |
|    - |  967 | `		 * read as int64 so a value above INT_MAX is detected, not truncated. */` |
|   13 |  968 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);` |
|    - |  969 | `		/* php clears the json error state before validating $depth, so a caught` |
|    - |  970 | `		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal` |
|    - |  971 | `		 * path resets it again inside VmJsonDecodeInput). */` |
|   13 |  972 | `		pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|   13 |  973 | `		if( nWant <= 0 ){` |
|    9 |  974 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  975 | `				"json_decode(): Argument #3 ($depth) must be greater than 0");` |
|    - |  976 | `		}` |
|    5 |  977 | `		if( nWant > 2147483647 ){` |
|    3 |  978 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  979 | `				"json_decode(): Argument #3 ($depth) must be less than 2147483647");` |
|    - |  980 | `		}` |
|    3 |  981 | `		nDepth = (int)nWant;` |
|    1 |  982 | `	}` |
|    - |  983 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|    - |  984 | `	 * call-context result; on failure we replace it with NULL. */` |
|   26 |  985 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth) != JSON_ERROR_NONE ){` |
|    - |  986 | `		/* Something goes wrong while decoding JSON input.Return NULL. */` |
|   12 |  987 | `		ph7_result_null(pCtx);` |
|    5 |  988 | `	}` |
|    - |  989 | `	/* All done */` |
|   26 |  990 | `	return PH7_OK;` |
|   21 |  991 | `}` |
|    - |  992 | `/*` |
|    - |  993 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|    - |  994 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|    - |  995 | ` * Parameters` |
|    - |  996 | ` *  $json   The string to validate.` |
|    - |  997 | ` *  $depth  Maximum nesting depth (clamped to the engine limit of 32).` |
|    - |  998 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|    - |  999 | ` * Return` |
|    - | 1000 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|    - | 1001 | ` */` |
|   20 | 1002 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1003 | `{` |
|   21 | 1004 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1005 | `	const char *zIn;` |
|    - | 1006 | `	int nByte;` |
|   21 | 1007 | `	int nDepth = 32;` |
|   21 | 1008 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1009 | `		/* Missing/Invalid argument: not valid JSON */` |
|  ! 0 | 1010 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1011 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1012 | `		return PH7_OK;` |
|    - | 1013 | `	}` |
|    - | 1014 | `	/* Extract the JSON string */` |
|   21 | 1015 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   21 | 1016 | `	if( nByte < 1 ){` |
|    - | 1017 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|    - | 1018 | `		 * silently, json_validate must record the syntax error) */` |
|    3 | 1019 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    3 | 1020 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1021 | `		return PH7_OK;` |
|    - | 1022 | `	}` |
|   19 | 1023 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - | 1024 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */` |
|    9 | 1025 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|    - | 1026 | `		/* Clear the json error state before validating $depth (php parity), so a` |
|    - | 1027 | `		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */` |
|    9 | 1028 | `		pVm->json_rc = JSON_ERROR_NONE;` |
|    9 | 1029 | `		if( nWant <= 0 ){` |
|    5 | 1030 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1031 | `				"json_validate(): Argument #2 ($depth) must be greater than 0");` |
|    - | 1032 | `		}` |
|    5 | 1033 | `		if( nWant > 2147483647 ){` |
|    3 | 1034 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1035 | `				"json_validate(): Argument #2 ($depth) must be less than 2147483647");` |
|    - | 1036 | `		}` |
|    3 | 1037 | `		nDepth = (int)nWant;` |
|    1 | 1038 | `	}` |
|    - | 1039 | `	/* apArg[2] ($flags) is accepted and ignored: no decode flag is implemented.` |
|    - | 1040 | `	 * Decode in associative mode so the "objects are returned as an array" warning is` |
|    - | 1041 | `	 * not raised - the decoded value is discarded, only its validity matters. */` |
|   13 | 1042 | `	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth) == JSON_ERROR_NONE);` |
|   13 | 1043 | `	return PH7_OK;` |
|   11 | 1044 | `}` |
|    - | 1045 |  |
