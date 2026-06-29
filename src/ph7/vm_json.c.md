# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 387/524 lines (73.85%)

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
|    - |   22 |  |
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
|    - |   39 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|    - |   40 | ` * According to wikipedia` |
|    - |   41 | ` * JSON's basic types are:` |
|    - |   42 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |   43 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |   44 | ` *   Boolean (true or false)` |
|    - |   45 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |   46 | ` *    do not need to be of the same type)` |
|    - |   47 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |   48 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |   49 | ` *     be distinct from each other)` |
|    - |   50 | ` *   null (empty)` |
|    - |   51 | ` * Non-significant white space may be added freely around the "structural characters"` |
|    - |   52 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |   53 | ` */` |
|  184 |   54 | `static sxi32 VmJsonEncode(` |
|    - |   55 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |   56 | `	json_private_data *pData /* Context data */` |
|    2 |   57 | `	){` |
|  186 |   58 | `		ph7_context *pCtx = pData->pCtx;` |
|  186 |   59 | `		int iFlags = pData->iFlags;` |
|    - |   60 | `		int nByte;` |
|  186 |   61 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |   62 | `			/* null */` |
|  ! 0 |   63 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|  184 |   64 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   13 |   65 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |   66 | `			int iLen;` |
|    - |   67 | `			/* true/false */` |
|   13 |   68 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   13 |   69 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
|  207 |   70 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|    - |   71 | `			const char *zNum;` |
|    - |   72 | `			/* Get a string representation of the number */` |
|   57 |   73 | `			zNum = ph7_value_to_string(pIn,&nByte);` |
|   57 |   74 | `			JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|  145 |   75 | `		}else if( ph7_value_is_string(pIn) ){` |
|   44 |   76 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |   77 | `				const char *zNum;` |
|    - |   78 | `				/* Encodes numeric strings as numbers. */` |
|  ! 0 |   79 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    - |   80 | `				/* Get a string representation of the number */` |
|  ! 0 |   81 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|  ! 0 |   82 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|  ! 0 |   83 | `			}else{` |
|    - |   84 | `				const char *zIn,*zEnd;` |
|    - |   85 | `				int c;` |
|    - |   86 | `				/* Encode the string */` |
|   44 |   87 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|   44 |   88 | `				zEnd = &zIn[nByte];` |
|    - |   89 | `				/* Append the double quote */` |
|   44 |   90 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|   54 |   91 | `				for(;;){` |
|  110 |   92 | `					if( zIn >= zEnd ){` |
|    - |   93 | `						/* No more input to process */` |
|   44 |   94 | `						break;` |
|    - |   95 | `					}` |
|   68 |   96 | `					c = zIn[0];` |
|    - |   97 | `					/* Advance the stream cursor */` |
|   68 |   98 | `					zIn++;` |
|   68 |   99 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |  100 | `						/* All < and > are converted to \u003C and \u003E */` |
|  ! 0 |  101 | `						if( c == '<' ){` |
|  ! 0 |  102 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1));` |
|  ! 0 |  103 | `						}else{` |
|  ! 0 |  104 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1));` |
|    - |  105 | `						}` |
|  ! 0 |  106 | `						continue;` |
|   68 |  107 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |  108 | `						/* All &s are converted to \u0026.  */` |
|  ! 0 |  109 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1));` |
|  ! 0 |  110 | `						continue;` |
|   68 |  111 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  112 | `						/* All ' are converted to \u0027.   */` |
|  ! 0 |  113 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1));` |
|  ! 0 |  114 | `						continue;` |
|   68 |  115 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  116 | `						/* All " are converted to \u0022. */` |
|  ! 0 |  117 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1));` |
|  ! 0 |  118 | `						continue;` |
|    - |  119 | `					}` |
|   68 |  120 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|    - |  121 | `						/* Unescape the character */` |
|  ! 0 |  122 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  ! 0 |  123 | `					}` |
|    - |  124 | `					/* Append character verbatim */` |
|   68 |  125 | `					JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    2 |  126 | `				}` |
|    - |  127 | `				/* Append the double quote */` |
|   44 |  128 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|    2 |  129 | `			}` |
|   97 |  130 | `		}else if( ph7_value_is_array(pIn) ){` |
|    - |  131 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|    - |  132 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|    - |  133 | `			 * object with stringified keys (PHP semantics). */` |
|  100 |  134 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|   50 |  135 | `				\|\| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);` |
|   51 |  136 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|   51 |  137 | `			int c = isObject ? '{' : '[';` |
|   51 |  138 | `			int d = isObject ? '}' : ']';` |
|    - |  139 | `			/* Encode the array */` |
|   51 |  140 | `			pData->isObject = isObject;` |
|   51 |  141 | `			pData->isFirst = 1;` |
|    - |  142 | `			/* Append the square bracket or curly braces */` |
|   51 |  143 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    - |  144 | `			/* Iterate throw array entries */` |
|   51 |  145 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  146 | `			/* Bail if a nested append ran out of memory before the closer */` |
|   51 |  147 | `			if( pData->oom ){` |
|  ! 0 |  148 | `				return PH7_OK;` |
|    - |  149 | `			}` |
|    - |  150 | `			/* Append the closing square bracket or curly braces */` |
|   51 |  151 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|   51 |  152 | `			pData->isObject = savedObject;` |
|   51 |  153 | `		}else if( ph7_value_is_object(pIn) ){` |
|   26 |  154 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   26 |  155 | `			ph7_vm *pVm = pIn->pVm;` |
|   26 |  156 | `			ph7_class_method *pMethod = 0;` |
|    - |  157 | `			/* If the object implements JsonSerializable, encode the value` |
|    - |  158 | `			 * returned by jsonSerialize() instead of its public properties. */` |
|   24 |  159 | `			if( pVm->pJsonSerializableClass` |
|   26 |  160 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|   15 |  161 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|    7 |  162 | `			}` |
|   26 |  163 | `			if( pMethod ){` |
|    - |  164 | `				ph7_value sResult;` |
|    - |  165 | `				sxi32 rc;` |
|   15 |  166 | `				PH7_MemObjInit(pVm,&sResult);` |
|   15 |  167 | `				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|   15 |  168 | `				if( rc == PH7_EXCEPTION ){` |
|    - |  169 | `					/* Let jsonSerialize()'s throw propagate */` |
|    5 |  170 | `					PH7_MemObjRelease(&sResult);` |
|    5 |  171 | `					pData->exc = 1;` |
|    5 |  172 | `					return PH7_EXCEPTION;` |
|    - |  173 | `				}` |
|    - |  174 | `				/* Encode the returned value [scalar/array/object] */` |
|   11 |  175 | `				pData->nRecCount++;` |
|   11 |  176 | `				VmJsonEncode(&sResult,pData);` |
|   11 |  177 | `				pData->nRecCount--;` |
|   11 |  178 | `				PH7_MemObjRelease(&sResult);` |
|   11 |  179 | `				if( pData->exc ){` |
|  ! 0 |  180 | `					return PH7_EXCEPTION;` |
|    - |  181 | `				}` |
|   11 |  182 | `				if( pData->oom ){` |
|  ! 0 |  183 | `					return PH7_OK;` |
|    - |  184 | `				}` |
|    6 |  185 | `			}else{` |
|    - |  186 | `				/* Encode the class instance */` |
|   12 |  187 | `				pData->isFirst = 1;` |
|    - |  188 | `				/* Append the curly braces */` |
|   12 |  189 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|    - |  190 | `				/* Iterate throw class attribute */` |
|   12 |  191 | `				ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|   12 |  192 | `				if( pData->oom ){` |
|  ! 0 |  193 | `					return PH7_OK;` |
|    - |  194 | `				}` |
|    - |  195 | `				/* Append the closing curly braces  */` |
|   12 |  196 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|    - |  197 | `			}` |
|   12 |  198 | `		}else{` |
|    - |  199 | `			/* Can't happen */` |
|  ! 0 |  200 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    - |  201 | `		}` |
|    - |  202 | `		/* All done */` |
|  182 |  203 | `		return PH7_OK;` |
|   94 |  204 |  |
|    - |  205 | `/*` |
|    - |  206 | ` * The following walker callback is invoked each time we need` |
|    - |  207 | ` * to encode an array to JSON.` |
|    - |  208 | ` */` |
|  104 |  209 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    1 |  210 |  |
|  105 |  211 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  105 |  212 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  213 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  214 | `		return PH7_OK;` |
|    - |  215 | `	}` |
|  105 |  216 | `	if( !pJson->isFirst ){` |
|    - |  217 | `		/* Append the colon first */` |
|   57 |  218 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   28 |  219 | `	}` |
|  105 |  220 | `	if( pJson->isObject ){` |
|    - |  221 | `		/* Outputs an object rather than an array */` |
|    - |  222 | `		const char *zKey;` |
|    - |  223 | `		int nByte;` |
|    - |  224 | `		/* Extract a string representation of the key */` |
|   51 |  225 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  226 | `		/* Append the quoted key and the colon (checked, so an OOM here is caught` |
|    - |  227 | `		 * rather than silently truncating; matches the prior "%.*s" emit byte for` |
|    - |  228 | `		 * byte — keys are not JSON-escaped, a pre-existing behavior). */` |
|   51 |  229 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|   51 |  230 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zKey,nByte));` |
|   51 |  231 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|   25 |  232 | `	}` |
|    - |  233 | `	/* Encode the value */` |
|  105 |  234 | `	pJson->nRecCount++;` |
|  105 |  235 | `	VmJsonEncode(pValue,pJson);` |
|  105 |  236 | `	pJson->nRecCount--;` |
|  105 |  237 | `	pJson->isFirst = 0;` |
|  105 |  238 | `	return PH7_OK;` |
|   53 |  239 |  |
|    - |  240 | `/*` |
|    - |  241 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  242 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  243 | ` */` |
|   22 |  244 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|    2 |  245 |  |
|   24 |  246 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|   24 |  247 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  248 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  249 | `		return PH7_OK;` |
|    - |  250 | `	}` |
|   24 |  251 | `	if( !pJson->isFirst ){` |
|    - |  252 | `		/* Append the colon first */` |
|   13 |  253 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|    6 |  254 | `	}` |
|    - |  255 | `	/* Append the quoted attribute name and the colon (checked; matches the prior` |
|    - |  256 | `	 * "%s" emit byte for byte — attribute names are not JSON-escaped). */` |
|   24 |  257 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|   24 |  258 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zAttr,-1));` |
|   24 |  259 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|    - |  260 | `	/* Encode the value */` |
|   24 |  261 | `	pJson->nRecCount++;` |
|   24 |  262 | `	VmJsonEncode(pValue,pJson);` |
|   24 |  263 | `	pJson->nRecCount--;` |
|   24 |  264 | `	pJson->isFirst = 0;` |
|   24 |  265 | `	return PH7_OK;` |
|   13 |  266 |  |
|    - |  267 | `/*` |
|    - |  268 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  269 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  270 | ` * Parameters` |
|    - |  271 | ` *  $value` |
|    - |  272 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  273 | ` * $options` |
|    - |  274 | ` *  Bitmask consisting of:` |
|    - |  275 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  276 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  277 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  278 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  279 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  280 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  281 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  282 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  283 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  284 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  285 | ` * Return` |
|    - |  286 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  287 | ` */` |
|   48 |  288 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  289 |  |
|    - |  290 | `	json_private_data sJson;` |
|    - |  291 | `	sxi32 rc;` |
|   50 |  292 | `	if( nArg < 1 ){` |
|    - |  293 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  294 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  295 | `		return PH7_OK;` |
|    - |  296 | `	}` |
|    - |  297 | `	/* Prepare the JSON data */` |
|   50 |  298 | `	sJson.nRecCount = 0;` |
|   50 |  299 | `	sJson.pCtx = pCtx;` |
|   50 |  300 | `	sJson.isFirst = 1;` |
|   50 |  301 | `	sJson.iFlags = 0;` |
|   50 |  302 | `	sJson.exc = 0;` |
|   50 |  303 | `	sJson.oom = 0;` |
|   50 |  304 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  305 | `		/* Extract option flags */` |
|    3 |  306 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|    1 |  307 | `	}` |
|    - |  308 | `	/* Perform the encoding operation */` |
|   50 |  309 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|   50 |  310 | `	if( sJson.oom ){` |
|    - |  311 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|    - |  312 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|  ! 0 |  313 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  314 | `	}` |
|   50 |  315 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|    - |  316 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|    5 |  317 | `		return PH7_EXCEPTION;` |
|    - |  318 | `	}` |
|    - |  319 | `	/* All done */` |
|   46 |  320 | `	return PH7_OK;` |
|   26 |  321 |  |
|    - |  322 | `#undef JSON_EMIT` |
|    - |  323 | `/*` |
|    - |  324 | ` * int json_last_error(void)` |
|    - |  325 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  326 | ` * Parameters` |
|    - |  327 | ` *  None` |
|    - |  328 | ` * Return` |
|    - |  329 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  330 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  331 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  332 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  333 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  334 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  335 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  336 | ` */` |
|   10 |  337 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  338 |  |
|   12 |  339 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  340 | `	/* Return the error code */` |
|   12 |  341 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    5 |  342 | `	SXUNUSED(nArg); /* cc warning */` |
|    5 |  343 | `	SXUNUSED(apArg);` |
|   12 |  344 | `	return PH7_OK;` |
|    2 |  345 |  |
|    - |  346 | `/*` |
|    - |  347 | ` * string json_last_error_msg(void)` |
|    - |  348 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|    - |  349 | ` * Parameters` |
|    - |  350 | ` *  None` |
|    - |  351 | ` * Return` |
|    - |  352 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|    - |  353 | ` *  code, or "No error" if no error has occurred.` |
|    - |  354 | ` */` |
|    4 |  355 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  356 |  |
|    5 |  357 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  358 | `	const char *zMsg;` |
|    5 |  359 | `	switch( pVm->json_rc ){` |
|    1 |  360 | `	case JSON_ERROR_NONE:` |
|    3 |  361 | `		zMsg = "No error";` |
|    3 |  362 | `		break;` |
|  ! 0 |  363 | `	case JSON_ERROR_DEPTH:` |
|  ! 0 |  364 | `		zMsg = "Maximum stack depth exceeded";` |
|  ! 0 |  365 | `		break;` |
|  ! 0 |  366 | `	case JSON_ERROR_STATE_MISMATCH:` |
|  ! 0 |  367 | `		zMsg = "State mismatch (invalid or malformed JSON)";` |
|  ! 0 |  368 | `		break;` |
|  ! 0 |  369 | `	case JSON_ERROR_CTRL_CHAR:` |
|  ! 0 |  370 | `		zMsg = "Control character error, possibly incorrectly encoded";` |
|  ! 0 |  371 | `		break;` |
|    1 |  372 | `	case JSON_ERROR_SYNTAX:` |
|    3 |  373 | `		zMsg = "Syntax error";` |
|    3 |  374 | `		break;` |
|  ! 0 |  375 | `	case JSON_ERROR_UTF8:` |
|  ! 0 |  376 | `		zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|  ! 0 |  377 | `		break;` |
|  ! 0 |  378 | `	default:` |
|  ! 0 |  379 | `		zMsg = "Unknown error";` |
|  ! 0 |  380 | `		break;` |
|    - |  381 | `	}` |
|    5 |  382 | `	ph7_result_string(pCtx,zMsg,-1/* Compute length automatically */);` |
|    2 |  383 | `	SXUNUSED(nArg); /* cc warning */` |
|    2 |  384 | `	SXUNUSED(apArg);` |
|    5 |  385 | `	return PH7_OK;` |
|    1 |  386 |  |
|    - |  387 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  388 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  389 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  390 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  391 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  392 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  393 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  394 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  395 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  396 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  397 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  398 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  399 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  400 | `/*` |
|    - |  401 | ` * Tokenize an entire JSON input.` |
|    - |  402 | ` * Get a single low-level token from the input file.` |
|    - |  403 | ` * Update the stream pointer so that it points to the first` |
|    - |  404 | ` * character beyond the extracted token.` |
|    - |  405 | ` */` |
|  144 |  406 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    2 |  407 |  |
|  146 |  408 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  409 | `	SyString *pStr;` |
|    - |  410 | `	int c;` |
|    - |  411 | `	/* Ignore leading white spaces */` |
|  150 |  412 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  413 | `		/* Advance the stream cursor */` |
|    6 |  414 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  415 | `			/* Update line counter */` |
|  ! 0 |  416 | `			pStream->nLine++;` |
|  ! 0 |  417 | `		}` |
|    6 |  418 | `		pStream->zText++;` |
|    2 |  419 | `	}` |
|  146 |  420 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  421 | `		/* End of input reached */` |
|  ! 0 |  422 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  423 | `		return SXERR_EOF;` |
|    - |  424 | `	}` |
|    - |  425 | `	/* Record token starting position and line */` |
|  146 |  426 | `	pToken->nLine = pStream->nLine;` |
|  146 |  427 | `	pToken->pUserData = 0;` |
|  146 |  428 | `	pStr = &pToken->sData;` |
|  146 |  429 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  183 |  430 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|  102 |  431 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  432 | `			/* Single character */` |
|   84 |  433 | `			c = pStream->zText[0];` |
|    - |  434 | `			/* Set token type */` |
|   84 |  435 | `			switch(c){` |
|    9 |  436 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   20 |  437 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|   16 |  438 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|    9 |  439 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   20 |  440 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   17 |  441 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  442 | `			default:` |
|  ! 0 |  443 | `				break;` |
|    - |  444 | `			}` |
|    - |  445 | `			/* Advance the stream cursor */` |
|   84 |  446 | `			pStream->zText++;` |
|  105 |  447 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  448 | `		/* JSON string */` |
|   26 |  449 | `		pStream->zText++;` |
|   26 |  450 | `		pStr->zString++;` |
|    - |  451 | `		/* Delimit the string */` |
|   72 |  452 | `		while( pStream->zText < pStream->zEnd ){` |
|   72 |  453 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   26 |  454 | `				break;` |
|    - |  455 | `			}` |
|   48 |  456 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  457 | `				/* Update line counter */` |
|  ! 0 |  458 | `				pStream->nLine++;` |
|  ! 0 |  459 | `			}` |
|   48 |  460 | `			pStream->zText++;` |
|    2 |  461 | `		}` |
|   26 |  462 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  463 | `			/* Missing closing '"' */` |
|  ! 0 |  464 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  465 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  466 | `		}else{` |
|   26 |  467 | `			pToken->nType = JSON_TK_STR;` |
|   26 |  468 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    2 |  469 | `		}` |
|   52 |  470 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    - |  471 | `		/* Number */` |
|   27 |  472 | `		pStream->zText++;` |
|   27 |  473 | `		pToken->nType = JSON_TK_NUM;` |
|   27 |  474 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  475 | `			pStream->zText++;` |
|  ! 0 |  476 | `		}` |
|   27 |  477 | `		if( pStream->zText < pStream->zEnd ){` |
|   27 |  478 | `			c = pStream->zText[0];` |
|   27 |  479 | `			if( c == '.' ){` |
|    - |  480 | `					/* Real number */` |
|  ! 0 |  481 | `					pStream->zText++;` |
|  ! 0 |  482 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  483 | `						pStream->zText++;` |
|  ! 0 |  484 | `					}` |
|  ! 0 |  485 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  486 | `						c = pStream->zText[0];` |
|  ! 0 |  487 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  488 | `							pStream->zText++;` |
|  ! 0 |  489 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  490 | `								c = pStream->zText[0];` |
|  ! 0 |  491 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  492 | `									pStream->zText++;` |
|  ! 0 |  493 | `								}` |
|  ! 0 |  494 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  495 | `									pStream->zText++;` |
|  ! 0 |  496 | `								}` |
|  ! 0 |  497 | `							}` |
|  ! 0 |  498 | `						}` |
|  ! 0 |  499 | `					}` |
|   27 |  500 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  501 | `					/* Real number */` |
|  ! 0 |  502 | `					pStream->zText++;` |
|  ! 0 |  503 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  504 | `						c = pStream->zText[0];` |
|  ! 0 |  505 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  506 | `							pStream->zText++;` |
|  ! 0 |  507 | `						}` |
|  ! 0 |  508 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  509 | `							pStream->zText++;` |
|  ! 0 |  510 | `						}` |
|  ! 0 |  511 | `					}` |
|  ! 0 |  512 | `				}` |
|   14 |  513 | `			}` |
|   33 |  514 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|   12 |  515 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  516 | `			/* boolean true */` |
|  ! 0 |  517 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  518 | `			/* Advance the stream cursor */` |
|  ! 0 |  519 | `			pStream->zText += sizeof("true")-1;` |
|   20 |  520 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|   12 |  521 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  522 | `			/* boolean false */` |
|  ! 0 |  523 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  524 | `			/* Advance the stream cursor */` |
|  ! 0 |  525 | `			pStream->zText += sizeof("false")-1;` |
|   20 |  526 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|   12 |  527 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  528 | `			/* NULL */` |
|  ! 0 |  529 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  530 | `			/* Advance the stream cursor */` |
|  ! 0 |  531 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  532 | `	}else{` |
|    - |  533 | `		/* Unexpected token */` |
|   14 |  534 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  535 | `		/* Advance the stream cursor */` |
|   14 |  536 | `		pStream->zText++;` |
|   14 |  537 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  538 | `		/* Abort processing immediatley */` |
|   14 |  539 | `		return SXERR_ABORT;` |
|    - |  540 | `	}` |
|    - |  541 | `	/* record token length */` |
|  134 |  542 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  134 |  543 | `	if( pToken->nType == JSON_TK_STR ){` |
|   26 |  544 | `		pStr->nByte--;` |
|   12 |  545 | `	}` |
|    - |  546 | `	/* Return to the lexer */` |
|  134 |  547 | `	return SXRET_OK;` |
|   74 |  548 |  |
|    - |  549 | `/*` |
|    - |  550 | ` * JSON decoded input consumer callback signature.` |
|    - |  551 | ` */` |
|    - |  552 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  553 | `/*` |
|    - |  554 | ` * JSON decoder state is kept in the following structure.` |
|    - |  555 | ` */` |
|    - |  556 | `typedef struct json_decoder json_decoder;` |
|    - |  557 | `struct json_decoder` |
|    - |  558 |  |
|    - |  559 | `	ph7_context *pCtx; /* Call context */` |
|    - |  560 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  561 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  562 | `	int iFlags;        /* Configuration flags */` |
|    - |  563 | `	SyToken *pIn;      /* Token stream */` |
|    - |  564 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  565 | `	int rec_depth;     /* Recursion limit */` |
|    - |  566 | `	int rec_count;     /* Current nesting level */` |
|    - |  567 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  568 | `};` |
|    - |  569 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  570 | `/* Forward declaration */` |
|    - |  571 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  572 | `/*` |
|    - |  573 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  574 | ` * the result in the given ph7_value.` |
|    - |  575 | ` */` |
|   24 |  576 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    2 |  577 |  |
|   26 |  578 | `	const char *zIn = pStr->zString;` |
|   26 |  579 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  580 | `	const char *zCur;` |
|    - |  581 | `	int c;` |
|    - |  582 | `	/* Mark the value as a string */` |
|   26 |  583 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|   12 |  584 | `	for(;;){` |
|   26 |  585 | `		zCur = zIn;` |
|   72 |  586 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   48 |  587 | `			zIn++;` |
|    2 |  588 | `		}` |
|   26 |  589 | `		if( zIn > zCur ){` |
|    - |  590 | `			/* Append chunk verbatim */` |
|   26 |  591 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|   12 |  592 | `		}` |
|   26 |  593 | `		zIn++;` |
|   26 |  594 | `		if( zIn >= zEnd ){` |
|    - |  595 | `			/* End of the input reached */` |
|   26 |  596 | `			break;` |
|    - |  597 | `		}` |
|  ! 0 |  598 | `		c = zIn[0];` |
|    - |  599 | `		/* Unescape the character */` |
|  ! 0 |  600 | `		switch(c){` |
|  ! 0 |  601 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  602 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  603 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  604 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  605 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  606 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  607 | `		default:` |
|  ! 0 |  608 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  609 | `			break;` |
|    - |  610 | `		}` |
|    - |  611 | `		/* Advance the stream cursor */` |
|  ! 0 |  612 | `		zIn++;` |
|  ! 0 |  613 | `	}` |
|   26 |  614 |  |
|    - |  615 | `/*` |
|    - |  616 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  617 | ` * According to wikipedia` |
|    - |  618 | ` * JSON's basic types are:` |
|    - |  619 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  620 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  621 | ` *   Boolean (true or false)` |
|    - |  622 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  623 | ` *    do not need to be of the same type)` |
|    - |  624 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  625 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  626 | ` *     be distinct from each other)` |
|    - |  627 | ` *   null (empty)` |
|    - |  628 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  629 | ` */` |
|   56 |  630 | `static sxi32 VmJsonDecode(` |
|    - |  631 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  632 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    2 |  633 | `	){` |
|    - |  634 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  635 | `	sxi32 rc;` |
|    - |  636 | `	/* Check if we do not nest to much */` |
|   58 |  637 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  638 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  639 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  640 | `		return SXERR_ABORT;` |
|    - |  641 | `	}` |
|   58 |  642 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  643 | `		/* Scalar value */` |
|   34 |  644 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   34 |  645 | `		if( pWorker == 0 ){` |
|  ! 0 |  646 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  647 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  648 | `			return SXERR_ABORT;` |
|    - |  649 | `		}` |
|    - |  650 | `		/* Reflect the JSON image */` |
|   34 |  651 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  652 | `			/* Nullify the value.*/` |
|  ! 0 |  653 | `			ph7_value_null(pWorker);` |
|   34 |  654 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  655 | `			/* Boolean value */` |
|  ! 0 |  656 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   34 |  657 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   27 |  658 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  659 | `			/*` |
|    - |  660 | `			 * Numeric value.` |
|    - |  661 | `			 * Get a string representation first then try to get a numeric` |
|    - |  662 | `			 * value.` |
|    - |  663 | `			 */` |
|   27 |  664 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  665 | `			/* Obtain a numeric representation */` |
|   27 |  666 | `			PH7_MemObjToNumeric(pWorker);` |
|   14 |  667 | `		}else{` |
|    - |  668 | `			/* Dequote the string */` |
|    8 |  669 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  670 | `		}` |
|    - |  671 | `		/* Invoke the consumer callback */` |
|   34 |  672 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   34 |  673 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  674 | `			return SXERR_ABORT;` |
|    - |  675 | `		}` |
|    - |  676 | `		/* All done,advance the stream cursor */` |
|   34 |  677 | `		pDecoder->pIn++;` |
|   42 |  678 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  679 | `		ProcJsonConsumer xOld;` |
|    - |  680 | `		void *pOld;` |
|    - |  681 | `		/* Array representation*/` |
|    9 |  682 | `		pDecoder->pIn++;` |
|    - |  683 | `		/* Create a working array */` |
|    9 |  684 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|    9 |  685 | `		if( pWorker == 0 ){` |
|  ! 0 |  686 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  687 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  688 | `			return SXERR_ABORT;` |
|    - |  689 | `		}` |
|    - |  690 | `		/* Save the old consumer */` |
|    9 |  691 | `		xOld = pDecoder->xConsumer;` |
|    9 |  692 | `		pOld = pDecoder->pUserData;` |
|    - |  693 | `		/* Set the new consumer */` |
|    9 |  694 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|    9 |  695 | `		pDecoder->pUserData = pWorker;` |
|    - |  696 | `		/* Decode the array */` |
|   14 |  697 | `		for(;;){` |
|    - |  698 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  699 | `			 * do this.` |
|    - |  700 | `			 */` |
|   41 |  701 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   13 |  702 | `				pDecoder->pIn++;` |
|    1 |  703 | `			}` |
|   29 |  704 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|    9 |  705 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|    9 |  706 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    4 |  707 | `				}` |
|    9 |  708 | `				break;` |
|    - |  709 | `			}` |
|    - |  710 | `			/* Recurse and decode the entry */` |
|   21 |  711 | `			pDecoder->rec_count++;` |
|   21 |  712 | `			rc = VmJsonDecode(pDecoder,0);` |
|   21 |  713 | `			pDecoder->rec_count--;` |
|   21 |  714 | `			if( rc == SXERR_ABORT ){` |
|    - |  715 | `				/* Abort processing immediately */` |
|  ! 0 |  716 | `				return SXERR_ABORT;` |
|    - |  717 | `			}` |
|    - |  718 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   21 |  719 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   20 |  720 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  721 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  722 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  723 | `					return SXERR_ABORT;` |
|    - |  724 | `			}` |
|    1 |  725 | `		}` |
|    - |  726 | `		/* Restore the old consumer */` |
|    9 |  727 | `		pDecoder->xConsumer = xOld;` |
|    9 |  728 | `		pDecoder->pUserData = pOld;` |
|    - |  729 | `		/* Invoke the old consumer on the decoded array */` |
|    9 |  730 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   22 |  731 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  732 | `		ProcJsonConsumer xOld;` |
|    - |  733 | `		ph7_value *pKey;` |
|    - |  734 | `		void *pOld;` |
|    - |  735 | `		/* Object representation*/` |
|   18 |  736 | `		pDecoder->pIn++;` |
|    - |  737 | `		/* Return the object as an associative array */` |
|   18 |  738 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|    3 |  739 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|    - |  740 | `				"JSON Objects are always returned as an associative array"` |
|    - |  741 | `				);` |
|    1 |  742 | `		}` |
|    - |  743 | `		/* Create a working array */` |
|   18 |  744 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   18 |  745 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   18 |  746 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  747 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  748 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  749 | `			return SXERR_ABORT;` |
|    - |  750 | `		}` |
|    - |  751 | `		/* Save the old consumer */` |
|   18 |  752 | `		xOld = pDecoder->xConsumer;` |
|   18 |  753 | `		pOld = pDecoder->pUserData;` |
|    - |  754 | `		/* Set the new consumer */` |
|   18 |  755 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   18 |  756 | `		pDecoder->pUserData = pWorker;` |
|    - |  757 | `		/* Decode the object */` |
|   17 |  758 | `		for(;;){` |
|    - |  759 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  760 | `			 * do this.` |
|    - |  761 | `			 */` |
|   40 |  762 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    5 |  763 | `				pDecoder->pIn++;` |
|    1 |  764 | `			}` |
|   36 |  765 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|   18 |  766 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   16 |  767 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    7 |  768 | `				}` |
|   18 |  769 | `				break;` |
|    - |  770 | `			}` |
|   18 |  771 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   20 |  772 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  773 | `					/* Syntax error,return immediately */` |
|  ! 0 |  774 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  775 | `					return SXERR_ABORT;` |
|    - |  776 | `			}` |
|    - |  777 | `			/* Dequote the key */` |
|   20 |  778 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  779 | `			/* Jump the key and the colon */` |
|   20 |  780 | `			pDecoder->pIn += 2;` |
|    - |  781 | `			/* Recurse and decode the value */` |
|   20 |  782 | `			pDecoder->rec_count++;` |
|   20 |  783 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   20 |  784 | `			pDecoder->rec_count--;` |
|   20 |  785 | `			if( rc == SXERR_ABORT ){` |
|    - |  786 | `				/* Abort processing immediately */` |
|  ! 0 |  787 | `				return SXERR_ABORT;` |
|    - |  788 | `			}` |
|    - |  789 | `			/* Reset the internal buffer of the key */` |
|   20 |  790 | `			ph7_value_reset_string_cursor(pKey);` |
|    - |  791 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    2 |  792 | `		}` |
|    - |  793 | `		/* Restore the old consumer */` |
|   18 |  794 | `		pDecoder->xConsumer = xOld;` |
|   18 |  795 | `		pDecoder->pUserData = pOld;` |
|    - |  796 | `		/* Invoke the old consumer on the decoded object*/` |
|   18 |  797 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - |  798 | `		/* Release the key */` |
|   18 |  799 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|   10 |  800 | `	}else{` |
|    - |  801 | `		/* Unexpected token */` |
|  ! 0 |  802 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - |  803 | `	}` |
|    - |  804 | `	/* Release the worker variable */` |
|   58 |  805 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   58 |  806 | `	return SXRET_OK;` |
|   30 |  807 |  |
|    - |  808 | `/*` |
|    - |  809 | ` * The following JSON decoder callback is invoked each time` |
|    - |  810 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - |  811 | ` * is being decoded.` |
|    - |  812 | ` */` |
|   38 |  813 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  814 |  |
|   40 |  815 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - |  816 | `	/* Insert the entry */` |
|   40 |  817 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   19 |  818 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - |  819 | `	/* All done */` |
|   40 |  820 | `	return SXRET_OK;` |
|    2 |  821 |  |
|    - |  822 | `/*` |
|    - |  823 | ` * Standard JSON decoder callback.` |
|    - |  824 | ` */` |
|   18 |  825 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  826 |  |
|    - |  827 | `	/* Return the value directly */` |
|   20 |  828 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|    9 |  829 | `	SXUNUSED(pKey); /* cc warning */` |
|    9 |  830 | `	SXUNUSED(pUserData);` |
|    - |  831 | `	/* All done */` |
|   20 |  832 | `	return SXRET_OK;` |
|    2 |  833 |  |
|    - |  834 | `/*` |
|    - |  835 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - |  836 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - |  837 | ` * Parameters` |
|    - |  838 | ` *  $json` |
|    - |  839 | ` *    The json string being decoded.` |
|    - |  840 | ` * $assoc` |
|    - |  841 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - |  842 | ` * $depth` |
|    - |  843 | ` *   User specified recursion depth.` |
|    - |  844 | ` * $options` |
|    - |  845 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - |  846 | ` * (default is to cast large integers as floats)` |
|    - |  847 | ` * Return` |
|    - |  848 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - |  849 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - |  850 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - |  851 | ` */` |
|    - |  852 | `/*` |
|    - |  853 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|    - |  854 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|    - |  855 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|    - |  856 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|    - |  857 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|    - |  858 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|    - |  859 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|    - |  860 | ` */` |
|   30 |  861 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth)` |
|    2 |  862 |  |
|   32 |  863 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  864 | `	json_decoder sDecoder;` |
|    - |  865 | `	SySet sToken;` |
|    - |  866 | `	SyLex sLex;` |
|    - |  867 | `	sxi32 rc;` |
|    - |  868 | `	/* Clear JSON error code */` |
|   32 |  869 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  870 | `	/* Tokenize the input */` |
|   32 |  871 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   32 |  872 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   32 |  873 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   32 |  874 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - |  875 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|   14 |  876 | `		SyLexRelease(&sLex);` |
|   14 |  877 | `		SySetRelease(&sToken);` |
|   14 |  878 | `		return pVm->json_rc;` |
|    - |  879 | `	}` |
|    - |  880 | `	/* Fill the decoder */` |
|   20 |  881 | `	sDecoder.pCtx = pCtx;` |
|   20 |  882 | `	sDecoder.pErr = &pVm->json_rc;` |
|   20 |  883 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   20 |  884 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   20 |  885 | `	sDecoder.iFlags = 0;` |
|   20 |  886 | `	if( iAssoc ){` |
|    - |  887 | `		/* Returned objects will be converted into associative arrays */` |
|   18 |  888 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|    8 |  889 | `	}` |
|   20 |  890 | `	sDecoder.rec_depth = 32;` |
|   20 |  891 | `	if( nDepth > 1 && nDepth < 32 ){` |
|  ! 0 |  892 | `		sDecoder.rec_depth = nDepth;` |
|  ! 0 |  893 | `	}` |
|   20 |  894 | `	sDecoder.rec_count = 0;` |
|    - |  895 | `	/* Set a default consumer */` |
|   20 |  896 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   20 |  897 | `	sDecoder.pUserData = 0;` |
|    - |  898 | `	/* Decode the raw JSON input */` |
|   20 |  899 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   20 |  900 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|    - |  901 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|  ! 0 |  902 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 |  903 | `	}` |
|    - |  904 | `	/* Clean-up the mess left behind */` |
|   20 |  905 | `	SyLexRelease(&sLex);` |
|   20 |  906 | `	SySetRelease(&sToken);` |
|   20 |  907 | `	return pVm->json_rc;` |
|   17 |  908 |  |
|   22 |  909 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  910 |  |
|    - |  911 | `	const char *zIn;` |
|    - |  912 | `	int nByte;` |
|   24 |  913 | `	int iAssoc = 0;` |
|   24 |  914 | `	int nDepth = 32;` |
|   24 |  915 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  916 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 |  917 | `		ph7_result_null(pCtx);` |
|  ! 0 |  918 | `		return PH7_OK;` |
|    - |  919 | `	}` |
|    - |  920 | `	/* Extract the JSON string */` |
|   24 |  921 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   24 |  922 | `	if( nByte < 1 ){` |
|    - |  923 | `		/* Empty string,return NULL */` |
|    3 |  924 | `		ph7_result_null(pCtx);` |
|    3 |  925 | `		return PH7_OK;` |
|    - |  926 | `	}` |
|   22 |  927 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|   12 |  928 | `		iAssoc = 1;` |
|    5 |  929 | `	}` |
|   22 |  930 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|  ! 0 |  931 | `		nDepth = ph7_value_to_int(apArg[2]);` |
|  ! 0 |  932 | `	}` |
|    - |  933 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|    - |  934 | `	 * call-context result; on failure we replace it with NULL. */` |
|   22 |  935 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth) != JSON_ERROR_NONE ){` |
|    - |  936 | `		/* Something goes wrong while decoding JSON input.Return NULL. */` |
|   10 |  937 | `		ph7_result_null(pCtx);` |
|    4 |  938 | `	}` |
|    - |  939 | `	/* All done */` |
|   22 |  940 | `	return PH7_OK;` |
|   13 |  941 |  |
|    - |  942 | `/*` |
|    - |  943 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|    - |  944 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|    - |  945 | ` * Parameters` |
|    - |  946 | ` *  $json   The string to validate.` |
|    - |  947 | ` *  $depth  Maximum nesting depth (clamped to the engine limit of 32).` |
|    - |  948 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|    - |  949 | ` * Return` |
|    - |  950 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|    - |  951 | ` */` |
|   12 |  952 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  953 |  |
|   13 |  954 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  955 | `	const char *zIn;` |
|    - |  956 | `	int nByte;` |
|   13 |  957 | `	int nDepth = 32;` |
|   13 |  958 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  959 | `		/* Missing/Invalid argument: not valid JSON */` |
|  ! 0 |  960 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 |  961 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  962 | `		return PH7_OK;` |
|    - |  963 | `	}` |
|    - |  964 | `	/* Extract the JSON string */` |
|   13 |  965 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   13 |  966 | `	if( nByte < 1 ){` |
|    - |  967 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|    - |  968 | `		 * silently, json_validate must record the syntax error) */` |
|    3 |  969 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    3 |  970 | `		ph7_result_bool(pCtx,0);` |
|    3 |  971 | `		return PH7_OK;` |
|    - |  972 | `	}` |
|   11 |  973 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|  ! 0 |  974 | `		nDepth = ph7_value_to_int(apArg[1]);` |
|  ! 0 |  975 | `	}` |
|    - |  976 | `	/* apArg[2] ($flags) is accepted and ignored: no decode flag is implemented.` |
|    - |  977 | `	 * Decode in associative mode so the "objects are returned as an array" warning is` |
|    - |  978 | `	 * not raised - the decoded value is discarded, only its validity matters. */` |
|   11 |  979 | `	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth) == JSON_ERROR_NONE);` |
|   11 |  980 | `	return PH7_OK;` |
|    7 |  981 |  |
|    - |  982 |  |
