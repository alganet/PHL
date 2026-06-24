# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 378/511 lines (73.97%)

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
|    - |   29 | `};` |
|    - |   30 | `/*` |
|    - |   31 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|    - |   32 | ` * According to wikipedia` |
|    - |   33 | ` * JSON's basic types are:` |
|    - |   34 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |   35 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |   36 | ` *   Boolean (true or false)` |
|    - |   37 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |   38 | ` *    do not need to be of the same type)` |
|    - |   39 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |   40 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |   41 | ` *     be distinct from each other)` |
|    - |   42 | ` *   null (empty)` |
|    - |   43 | ` * Non-significant white space may be added freely around the "structural characters"` |
|    - |   44 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |   45 | ` */` |
|  158 |   46 | `static sxi32 VmJsonEncode(` |
|    - |   47 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |   48 | `	json_private_data *pData /* Context data */` |
|    1 |   49 | `	){` |
|  159 |   50 | `		ph7_context *pCtx = pData->pCtx;` |
|  159 |   51 | `		int iFlags = pData->iFlags;` |
|    - |   52 | `		int nByte;` |
|  159 |   53 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |   54 | `			/* null */` |
|  ! 0 |   55 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|  159 |   56 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   13 |   57 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |   58 | `			int iLen;` |
|    - |   59 | `			/* true/false */` |
|   13 |   60 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   13 |   61 | `			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);` |
|  175 |   62 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|    - |   63 | `			const char *zNum;` |
|    - |   64 | `			/* Get a string representation of the number */` |
|   45 |   65 | `			zNum = ph7_value_to_string(pIn,&nByte);` |
|   45 |   66 | `			ph7_result_string(pCtx,zNum,nByte);` |
|  125 |   67 | `		}else if( ph7_value_is_string(pIn) ){` |
|   37 |   68 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |   69 | `				const char *zNum;` |
|    - |   70 | `				/* Encodes numeric strings as numbers. */` |
|  ! 0 |   71 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    - |   72 | `				/* Get a string representation of the number */` |
|  ! 0 |   73 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|  ! 0 |   74 | `				ph7_result_string(pCtx,zNum,nByte);` |
|  ! 0 |   75 | `			}else{` |
|    - |   76 | `				const char *zIn,*zEnd;` |
|    - |   77 | `				int c;` |
|    - |   78 | `				/* Encode the string */` |
|   37 |   79 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|   37 |   80 | `				zEnd = &zIn[nByte];` |
|    - |   81 | `				/* Append the double quote */` |
|   37 |   82 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|   42 |   83 | `				for(;;){` |
|   85 |   84 | `					if( zIn >= zEnd ){` |
|    - |   85 | `						/* No more input to process */` |
|   37 |   86 | `						break;` |
|    - |   87 | `					}` |
|   49 |   88 | `					c = zIn[0];` |
|    - |   89 | `					/* Advance the stream cursor */` |
|   49 |   90 | `					zIn++;` |
|   49 |   91 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |   92 | `						/* All < and > are converted to \u003C and \u003E */` |
|  ! 0 |   93 | `						if( c == '<' ){` |
|  ! 0 |   94 | `							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|  ! 0 |   95 | `						}else{` |
|  ! 0 |   96 | `							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|    - |   97 | `						}` |
|  ! 0 |   98 | `						continue;` |
|   49 |   99 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |  100 | `						/* All &s are converted to \u0026.  */` |
|  ! 0 |  101 | `						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|  ! 0 |  102 | `						continue;` |
|   49 |  103 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  104 | `						/* All ' are converted to \u0027.   */` |
|  ! 0 |  105 | `						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|  ! 0 |  106 | `						continue;` |
|   49 |  107 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  108 | `						/* All " are converted to \u0022. */` |
|  ! 0 |  109 | `						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|  ! 0 |  110 | `						continue;` |
|    - |  111 | `					}` |
|   49 |  112 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|    - |  113 | `						/* Unescape the character */` |
|  ! 0 |  114 | `						ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|  ! 0 |  115 | `					}` |
|    - |  116 | `					/* Append character verbatim */` |
|   49 |  117 | `					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    1 |  118 | `				}` |
|    - |  119 | `				/* Append the double quote */` |
|   37 |  120 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|    1 |  121 | `			}` |
|   85 |  122 | `		}else if( ph7_value_is_array(pIn) ){` |
|    - |  123 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|    - |  124 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|    - |  125 | `			 * object with stringified keys (PHP semantics). */` |
|  100 |  126 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|   50 |  127 | `				\|\| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);` |
|   51 |  128 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|   51 |  129 | `			int c = isObject ? '{' : '[';` |
|   51 |  130 | `			int d = isObject ? '}' : ']';` |
|    - |  131 | `			/* Encode the array */` |
|   51 |  132 | `			pData->isObject = isObject;` |
|   51 |  133 | `			pData->isFirst = 1;` |
|    - |  134 | `			/* Append the square bracket or curly braces */` |
|   51 |  135 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    - |  136 | `			/* Iterate throw array entries */` |
|   51 |  137 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  138 | `			/* Append the closing square bracket or curly braces */` |
|   51 |  139 | `			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));` |
|   51 |  140 | `			pData->isObject = savedObject;` |
|   42 |  141 | `		}else if( ph7_value_is_object(pIn) ){` |
|   17 |  142 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   17 |  143 | `			ph7_vm *pVm = pIn->pVm;` |
|   17 |  144 | `			ph7_class_method *pMethod = 0;` |
|    - |  145 | `			/* If the object implements JsonSerializable, encode the value` |
|    - |  146 | `			 * returned by jsonSerialize() instead of its public properties. */` |
|   16 |  147 | `			if( pVm->pJsonSerializableClass` |
|   17 |  148 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|   15 |  149 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|    7 |  150 | `			}` |
|   17 |  151 | `			if( pMethod ){` |
|    - |  152 | `				ph7_value sResult;` |
|    - |  153 | `				sxi32 rc;` |
|   15 |  154 | `				PH7_MemObjInit(pVm,&sResult);` |
|   15 |  155 | `				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|   15 |  156 | `				if( rc == PH7_EXCEPTION ){` |
|    - |  157 | `					/* Let jsonSerialize()'s throw propagate */` |
|    5 |  158 | `					PH7_MemObjRelease(&sResult);` |
|    5 |  159 | `					pData->exc = 1;` |
|    5 |  160 | `					return PH7_EXCEPTION;` |
|    - |  161 | `				}` |
|    - |  162 | `				/* Encode the returned value [scalar/array/object] */` |
|   11 |  163 | `				pData->nRecCount++;` |
|   11 |  164 | `				VmJsonEncode(&sResult,pData);` |
|   11 |  165 | `				pData->nRecCount--;` |
|   11 |  166 | `				PH7_MemObjRelease(&sResult);` |
|   11 |  167 | `				if( pData->exc ){` |
|  ! 0 |  168 | `					return PH7_EXCEPTION;` |
|    - |  169 | `				}` |
|    6 |  170 | `			}else{` |
|    - |  171 | `				/* Encode the class instance */` |
|    3 |  172 | `				pData->isFirst = 1;` |
|    - |  173 | `				/* Append the curly braces */` |
|    3 |  174 | `				ph7_result_string(pCtx,"{",(int)sizeof(char));` |
|    - |  175 | `				/* Iterate throw class attribute */` |
|    3 |  176 | `				ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|    - |  177 | `				/* Append the closing curly braces  */` |
|    3 |  178 | `				ph7_result_string(pCtx,"}",(int)sizeof(char));` |
|    - |  179 | `			}` |
|    7 |  180 | `		}else{` |
|    - |  181 | `			/* Can't happen */` |
|  ! 0 |  182 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|    - |  183 | `		}` |
|    - |  184 | `		/* All done */` |
|  155 |  185 | `		return PH7_OK;` |
|   80 |  186 |  |
|    - |  187 | `/*` |
|    - |  188 | ` * The following walker callback is invoked each time we need` |
|    - |  189 | ` * to encode an array to JSON.` |
|    - |  190 | ` */` |
|  104 |  191 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    1 |  192 |  |
|  105 |  193 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  105 |  194 | `	if( pJson->nRecCount > 31 \|\| pJson->exc ){` |
|    - |  195 | `		/* Recursion limit reached or a callback threw,return immediately */` |
|  ! 0 |  196 | `		return PH7_OK;` |
|    - |  197 | `	}` |
|  105 |  198 | `	if( !pJson->isFirst ){` |
|    - |  199 | `		/* Append the colon first */` |
|   57 |  200 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|   28 |  201 | `	}` |
|  105 |  202 | `	if( pJson->isObject ){` |
|    - |  203 | `		/* Outputs an object rather than an array */` |
|    - |  204 | `		const char *zKey;` |
|    - |  205 | `		int nByte;` |
|    - |  206 | `		/* Extract a string representation of the key */` |
|   51 |  207 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  208 | `		/* Append the key and the double colon */` |
|   51 |  209 | `		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);` |
|   25 |  210 | `	}` |
|    - |  211 | `	/* Encode the value */` |
|  105 |  212 | `	pJson->nRecCount++;` |
|  105 |  213 | `	VmJsonEncode(pValue,pJson);` |
|  105 |  214 | `	pJson->nRecCount--;` |
|  105 |  215 | `	pJson->isFirst = 0;` |
|  105 |  216 | `	return PH7_OK;` |
|   53 |  217 |  |
|    - |  218 | `/*` |
|    - |  219 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  220 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  221 | ` */` |
|    4 |  222 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|    1 |  223 |  |
|    5 |  224 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|    5 |  225 | `	if( pJson->nRecCount > 31 \|\| pJson->exc ){` |
|    - |  226 | `		/* Recursion limit reached or a callback threw,return immediately */` |
|  ! 0 |  227 | `		return PH7_OK;` |
|    - |  228 | `	}` |
|    5 |  229 | `	if( !pJson->isFirst ){` |
|    - |  230 | `		/* Append the colon first */` |
|    3 |  231 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|    1 |  232 | `	}` |
|    - |  233 | `	/* Append the attribute name and the double colon first */` |
|    5 |  234 | `	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);` |
|    - |  235 | `	/* Encode the value */` |
|    5 |  236 | `	pJson->nRecCount++;` |
|    5 |  237 | `	VmJsonEncode(pValue,pJson);` |
|    5 |  238 | `	pJson->nRecCount--;` |
|    5 |  239 | `	pJson->isFirst = 0;` |
|    5 |  240 | `	return PH7_OK;` |
|    3 |  241 |  |
|    - |  242 | `/*` |
|    - |  243 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  244 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  245 | ` * Parameters` |
|    - |  246 | ` *  $value` |
|    - |  247 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  248 | ` * $options` |
|    - |  249 | ` *  Bitmask consisting of:` |
|    - |  250 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  251 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  252 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  253 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  254 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  255 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  256 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  257 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  258 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  259 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  260 | ` * Return` |
|    - |  261 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  262 | ` */` |
|   40 |  263 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  264 |  |
|    - |  265 | `	json_private_data sJson;` |
|    - |  266 | `	sxi32 rc;` |
|   41 |  267 | `	if( nArg < 1 ){` |
|    - |  268 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  269 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  270 | `		return PH7_OK;` |
|    - |  271 | `	}` |
|    - |  272 | `	/* Prepare the JSON data */` |
|   41 |  273 | `	sJson.nRecCount = 0;` |
|   41 |  274 | `	sJson.pCtx = pCtx;` |
|   41 |  275 | `	sJson.isFirst = 1;` |
|   41 |  276 | `	sJson.iFlags = 0;` |
|   41 |  277 | `	sJson.exc = 0;` |
|   41 |  278 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  279 | `		/* Extract option flags */` |
|    3 |  280 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|    1 |  281 | `	}` |
|    - |  282 | `	/* Perform the encoding operation */` |
|   41 |  283 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|   41 |  284 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|    - |  285 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|    5 |  286 | `		return PH7_EXCEPTION;` |
|    - |  287 | `	}` |
|    - |  288 | `	/* All done */` |
|   37 |  289 | `	return PH7_OK;` |
|   21 |  290 |  |
|    - |  291 | `/*` |
|    - |  292 | ` * int json_last_error(void)` |
|    - |  293 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  294 | ` * Parameters` |
|    - |  295 | ` *  None` |
|    - |  296 | ` * Return` |
|    - |  297 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  298 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  299 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  300 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  301 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  302 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  303 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  304 | ` */` |
|   10 |  305 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  306 |  |
|   12 |  307 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  308 | `	/* Return the error code */` |
|   12 |  309 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    5 |  310 | `	SXUNUSED(nArg); /* cc warning */` |
|    5 |  311 | `	SXUNUSED(apArg);` |
|   12 |  312 | `	return PH7_OK;` |
|    2 |  313 |  |
|    - |  314 | `/*` |
|    - |  315 | ` * string json_last_error_msg(void)` |
|    - |  316 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|    - |  317 | ` * Parameters` |
|    - |  318 | ` *  None` |
|    - |  319 | ` * Return` |
|    - |  320 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|    - |  321 | ` *  code, or "No error" if no error has occurred.` |
|    - |  322 | ` */` |
|    4 |  323 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  324 |  |
|    5 |  325 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  326 | `	const char *zMsg;` |
|    5 |  327 | `	switch( pVm->json_rc ){` |
|    1 |  328 | `	case JSON_ERROR_NONE:` |
|    3 |  329 | `		zMsg = "No error";` |
|    3 |  330 | `		break;` |
|  ! 0 |  331 | `	case JSON_ERROR_DEPTH:` |
|  ! 0 |  332 | `		zMsg = "Maximum stack depth exceeded";` |
|  ! 0 |  333 | `		break;` |
|  ! 0 |  334 | `	case JSON_ERROR_STATE_MISMATCH:` |
|  ! 0 |  335 | `		zMsg = "State mismatch (invalid or malformed JSON)";` |
|  ! 0 |  336 | `		break;` |
|  ! 0 |  337 | `	case JSON_ERROR_CTRL_CHAR:` |
|  ! 0 |  338 | `		zMsg = "Control character error, possibly incorrectly encoded";` |
|  ! 0 |  339 | `		break;` |
|    1 |  340 | `	case JSON_ERROR_SYNTAX:` |
|    3 |  341 | `		zMsg = "Syntax error";` |
|    3 |  342 | `		break;` |
|  ! 0 |  343 | `	case JSON_ERROR_UTF8:` |
|  ! 0 |  344 | `		zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|  ! 0 |  345 | `		break;` |
|  ! 0 |  346 | `	default:` |
|  ! 0 |  347 | `		zMsg = "Unknown error";` |
|  ! 0 |  348 | `		break;` |
|    - |  349 | `	}` |
|    5 |  350 | `	ph7_result_string(pCtx,zMsg,-1/* Compute length automatically */);` |
|    2 |  351 | `	SXUNUSED(nArg); /* cc warning */` |
|    2 |  352 | `	SXUNUSED(apArg);` |
|    5 |  353 | `	return PH7_OK;` |
|    1 |  354 |  |
|    - |  355 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  356 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  357 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  358 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  359 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  360 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  361 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  362 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  363 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  364 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  365 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  366 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  367 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  368 | `/*` |
|    - |  369 | ` * Tokenize an entire JSON input.` |
|    - |  370 | ` * Get a single low-level token from the input file.` |
|    - |  371 | ` * Update the stream pointer so that it points to the first` |
|    - |  372 | ` * character beyond the extracted token.` |
|    - |  373 | ` */` |
|  144 |  374 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    2 |  375 |  |
|  146 |  376 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  377 | `	SyString *pStr;` |
|    - |  378 | `	int c;` |
|    - |  379 | `	/* Ignore leading white spaces */` |
|  150 |  380 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  381 | `		/* Advance the stream cursor */` |
|    6 |  382 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  383 | `			/* Update line counter */` |
|  ! 0 |  384 | `			pStream->nLine++;` |
|  ! 0 |  385 | `		}` |
|    6 |  386 | `		pStream->zText++;` |
|    2 |  387 | `	}` |
|  146 |  388 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  389 | `		/* End of input reached */` |
|  ! 0 |  390 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  391 | `		return SXERR_EOF;` |
|    - |  392 | `	}` |
|    - |  393 | `	/* Record token starting position and line */` |
|  146 |  394 | `	pToken->nLine = pStream->nLine;` |
|  146 |  395 | `	pToken->pUserData = 0;` |
|  146 |  396 | `	pStr = &pToken->sData;` |
|  146 |  397 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  144 |  398 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|  102 |  399 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  400 | `			/* Single character */` |
|   84 |  401 | `			c = pStream->zText[0];` |
|    - |  402 | `			/* Set token type */` |
|   84 |  403 | `			switch(c){` |
|    9 |  404 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   20 |  405 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|   16 |  406 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|    9 |  407 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   20 |  408 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   17 |  409 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  410 | `			default:` |
|  ! 0 |  411 | `				break;` |
|    - |  412 | `			}` |
|    - |  413 | `			/* Advance the stream cursor */` |
|   84 |  414 | `			pStream->zText++;` |
|  105 |  415 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  416 | `		/* JSON string */` |
|   26 |  417 | `		pStream->zText++;` |
|   26 |  418 | `		pStr->zString++;` |
|    - |  419 | `		/* Delimit the string */` |
|   72 |  420 | `		while( pStream->zText < pStream->zEnd ){` |
|   72 |  421 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   26 |  422 | `				break;` |
|    - |  423 | `			}` |
|   48 |  424 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  425 | `				/* Update line counter */` |
|  ! 0 |  426 | `				pStream->nLine++;` |
|  ! 0 |  427 | `			}` |
|   48 |  428 | `			pStream->zText++;` |
|    2 |  429 | `		}` |
|   26 |  430 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  431 | `			/* Missing closing '"' */` |
|  ! 0 |  432 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  433 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  434 | `		}else{` |
|   26 |  435 | `			pToken->nType = JSON_TK_STR;` |
|   26 |  436 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    2 |  437 | `		}` |
|   52 |  438 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    - |  439 | `		/* Number */` |
|   27 |  440 | `		pStream->zText++;` |
|   27 |  441 | `		pToken->nType = JSON_TK_NUM;` |
|   27 |  442 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  443 | `			pStream->zText++;` |
|  ! 0 |  444 | `		}` |
|   27 |  445 | `		if( pStream->zText < pStream->zEnd ){` |
|   27 |  446 | `			c = pStream->zText[0];` |
|   27 |  447 | `			if( c == '.' ){` |
|    - |  448 | `					/* Real number */` |
|  ! 0 |  449 | `					pStream->zText++;` |
|  ! 0 |  450 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  451 | `						pStream->zText++;` |
|  ! 0 |  452 | `					}` |
|  ! 0 |  453 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  454 | `						c = pStream->zText[0];` |
|  ! 0 |  455 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  456 | `							pStream->zText++;` |
|  ! 0 |  457 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  458 | `								c = pStream->zText[0];` |
|  ! 0 |  459 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  460 | `									pStream->zText++;` |
|  ! 0 |  461 | `								}` |
|  ! 0 |  462 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  463 | `									pStream->zText++;` |
|  ! 0 |  464 | `								}` |
|  ! 0 |  465 | `							}` |
|  ! 0 |  466 | `						}` |
|  ! 0 |  467 | `					}` |
|   27 |  468 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  469 | `					/* Real number */` |
|  ! 0 |  470 | `					pStream->zText++;` |
|  ! 0 |  471 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  472 | `						c = pStream->zText[0];` |
|  ! 0 |  473 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  474 | `							pStream->zText++;` |
|  ! 0 |  475 | `						}` |
|  ! 0 |  476 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  477 | `							pStream->zText++;` |
|  ! 0 |  478 | `						}` |
|  ! 0 |  479 | `					}` |
|  ! 0 |  480 | `				}` |
|   14 |  481 | `			}` |
|   33 |  482 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|   12 |  483 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  484 | `			/* boolean true */` |
|  ! 0 |  485 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  486 | `			/* Advance the stream cursor */` |
|  ! 0 |  487 | `			pStream->zText += sizeof("true")-1;` |
|   20 |  488 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|   12 |  489 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  490 | `			/* boolean false */` |
|  ! 0 |  491 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  492 | `			/* Advance the stream cursor */` |
|  ! 0 |  493 | `			pStream->zText += sizeof("false")-1;` |
|   20 |  494 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|   12 |  495 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  496 | `			/* NULL */` |
|  ! 0 |  497 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  498 | `			/* Advance the stream cursor */` |
|  ! 0 |  499 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  500 | `	}else{` |
|    - |  501 | `		/* Unexpected token */` |
|   14 |  502 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  503 | `		/* Advance the stream cursor */` |
|   14 |  504 | `		pStream->zText++;` |
|   14 |  505 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  506 | `		/* Abort processing immediatley */` |
|   14 |  507 | `		return SXERR_ABORT;` |
|    - |  508 | `	}` |
|    - |  509 | `	/* record token length */` |
|  134 |  510 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  134 |  511 | `	if( pToken->nType == JSON_TK_STR ){` |
|   26 |  512 | `		pStr->nByte--;` |
|   12 |  513 | `	}` |
|    - |  514 | `	/* Return to the lexer */` |
|  134 |  515 | `	return SXRET_OK;` |
|   74 |  516 |  |
|    - |  517 | `/*` |
|    - |  518 | ` * JSON decoded input consumer callback signature.` |
|    - |  519 | ` */` |
|    - |  520 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  521 | `/*` |
|    - |  522 | ` * JSON decoder state is kept in the following structure.` |
|    - |  523 | ` */` |
|    - |  524 | `typedef struct json_decoder json_decoder;` |
|    - |  525 | `struct json_decoder` |
|    - |  526 |  |
|    - |  527 | `	ph7_context *pCtx; /* Call context */` |
|    - |  528 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  529 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  530 | `	int iFlags;        /* Configuration flags */` |
|    - |  531 | `	SyToken *pIn;      /* Token stream */` |
|    - |  532 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  533 | `	int rec_depth;     /* Recursion limit */` |
|    - |  534 | `	int rec_count;     /* Current nesting level */` |
|    - |  535 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  536 | `};` |
|    - |  537 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  538 | `/* Forward declaration */` |
|    - |  539 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  540 | `/*` |
|    - |  541 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  542 | ` * the result in the given ph7_value.` |
|    - |  543 | ` */` |
|   24 |  544 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    2 |  545 |  |
|   26 |  546 | `	const char *zIn = pStr->zString;` |
|   26 |  547 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  548 | `	const char *zCur;` |
|    - |  549 | `	int c;` |
|    - |  550 | `	/* Mark the value as a string */` |
|   26 |  551 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|   12 |  552 | `	for(;;){` |
|   26 |  553 | `		zCur = zIn;` |
|   72 |  554 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   48 |  555 | `			zIn++;` |
|    2 |  556 | `		}` |
|   26 |  557 | `		if( zIn > zCur ){` |
|    - |  558 | `			/* Append chunk verbatim */` |
|   26 |  559 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|   12 |  560 | `		}` |
|   26 |  561 | `		zIn++;` |
|   26 |  562 | `		if( zIn >= zEnd ){` |
|    - |  563 | `			/* End of the input reached */` |
|   26 |  564 | `			break;` |
|    - |  565 | `		}` |
|  ! 0 |  566 | `		c = zIn[0];` |
|    - |  567 | `		/* Unescape the character */` |
|  ! 0 |  568 | `		switch(c){` |
|  ! 0 |  569 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  570 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  571 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  572 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  573 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  574 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  575 | `		default:` |
|  ! 0 |  576 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  577 | `			break;` |
|    - |  578 | `		}` |
|    - |  579 | `		/* Advance the stream cursor */` |
|  ! 0 |  580 | `		zIn++;` |
|  ! 0 |  581 | `	}` |
|   26 |  582 |  |
|    - |  583 | `/*` |
|    - |  584 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  585 | ` * According to wikipedia` |
|    - |  586 | ` * JSON's basic types are:` |
|    - |  587 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  588 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  589 | ` *   Boolean (true or false)` |
|    - |  590 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  591 | ` *    do not need to be of the same type)` |
|    - |  592 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  593 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  594 | ` *     be distinct from each other)` |
|    - |  595 | ` *   null (empty)` |
|    - |  596 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  597 | ` */` |
|   56 |  598 | `static sxi32 VmJsonDecode(` |
|    - |  599 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  600 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    2 |  601 | `	){` |
|    - |  602 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  603 | `	sxi32 rc;` |
|    - |  604 | `	/* Check if we do not nest to much */` |
|   58 |  605 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  606 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  607 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  608 | `		return SXERR_ABORT;` |
|    - |  609 | `	}` |
|   58 |  610 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  611 | `		/* Scalar value */` |
|   34 |  612 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   34 |  613 | `		if( pWorker == 0 ){` |
|  ! 0 |  614 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  615 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  616 | `			return SXERR_ABORT;` |
|    - |  617 | `		}` |
|    - |  618 | `		/* Reflect the JSON image */` |
|   34 |  619 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  620 | `			/* Nullify the value.*/` |
|  ! 0 |  621 | `			ph7_value_null(pWorker);` |
|   34 |  622 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  623 | `			/* Boolean value */` |
|  ! 0 |  624 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   34 |  625 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   27 |  626 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  627 | `			/*` |
|    - |  628 | `			 * Numeric value.` |
|    - |  629 | `			 * Get a string representation first then try to get a numeric` |
|    - |  630 | `			 * value.` |
|    - |  631 | `			 */` |
|   27 |  632 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  633 | `			/* Obtain a numeric representation */` |
|   27 |  634 | `			PH7_MemObjToNumeric(pWorker);` |
|   14 |  635 | `		}else{` |
|    - |  636 | `			/* Dequote the string */` |
|    8 |  637 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  638 | `		}` |
|    - |  639 | `		/* Invoke the consumer callback */` |
|   34 |  640 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   34 |  641 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  642 | `			return SXERR_ABORT;` |
|    - |  643 | `		}` |
|    - |  644 | `		/* All done,advance the stream cursor */` |
|   34 |  645 | `		pDecoder->pIn++;` |
|   42 |  646 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  647 | `		ProcJsonConsumer xOld;` |
|    - |  648 | `		void *pOld;` |
|    - |  649 | `		/* Array representation*/` |
|    9 |  650 | `		pDecoder->pIn++;` |
|    - |  651 | `		/* Create a working array */` |
|    9 |  652 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|    9 |  653 | `		if( pWorker == 0 ){` |
|  ! 0 |  654 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  655 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  656 | `			return SXERR_ABORT;` |
|    - |  657 | `		}` |
|    - |  658 | `		/* Save the old consumer */` |
|    9 |  659 | `		xOld = pDecoder->xConsumer;` |
|    9 |  660 | `		pOld = pDecoder->pUserData;` |
|    - |  661 | `		/* Set the new consumer */` |
|    9 |  662 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|    9 |  663 | `		pDecoder->pUserData = pWorker;` |
|    - |  664 | `		/* Decode the array */` |
|   14 |  665 | `		for(;;){` |
|    - |  666 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  667 | `			 * do this.` |
|    - |  668 | `			 */` |
|   41 |  669 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   13 |  670 | `				pDecoder->pIn++;` |
|    1 |  671 | `			}` |
|   29 |  672 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|    9 |  673 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|    9 |  674 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    4 |  675 | `				}` |
|    9 |  676 | `				break;` |
|    - |  677 | `			}` |
|    - |  678 | `			/* Recurse and decode the entry */` |
|   21 |  679 | `			pDecoder->rec_count++;` |
|   21 |  680 | `			rc = VmJsonDecode(pDecoder,0);` |
|   21 |  681 | `			pDecoder->rec_count--;` |
|   21 |  682 | `			if( rc == SXERR_ABORT ){` |
|    - |  683 | `				/* Abort processing immediately */` |
|  ! 0 |  684 | `				return SXERR_ABORT;` |
|    - |  685 | `			}` |
|    - |  686 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   21 |  687 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   20 |  688 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  689 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  690 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  691 | `					return SXERR_ABORT;` |
|    - |  692 | `			}` |
|    1 |  693 | `		}` |
|    - |  694 | `		/* Restore the old consumer */` |
|    9 |  695 | `		pDecoder->xConsumer = xOld;` |
|    9 |  696 | `		pDecoder->pUserData = pOld;` |
|    - |  697 | `		/* Invoke the old consumer on the decoded array */` |
|    9 |  698 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   22 |  699 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  700 | `		ProcJsonConsumer xOld;` |
|    - |  701 | `		ph7_value *pKey;` |
|    - |  702 | `		void *pOld;` |
|    - |  703 | `		/* Object representation*/` |
|   18 |  704 | `		pDecoder->pIn++;` |
|    - |  705 | `		/* Return the object as an associative array */` |
|   18 |  706 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|    3 |  707 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|    - |  708 | `				"JSON Objects are always returned as an associative array"` |
|    - |  709 | `				);` |
|    1 |  710 | `		}` |
|    - |  711 | `		/* Create a working array */` |
|   18 |  712 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   18 |  713 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   18 |  714 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  715 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  716 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  717 | `			return SXERR_ABORT;` |
|    - |  718 | `		}` |
|    - |  719 | `		/* Save the old consumer */` |
|   18 |  720 | `		xOld = pDecoder->xConsumer;` |
|   18 |  721 | `		pOld = pDecoder->pUserData;` |
|    - |  722 | `		/* Set the new consumer */` |
|   18 |  723 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   18 |  724 | `		pDecoder->pUserData = pWorker;` |
|    - |  725 | `		/* Decode the object */` |
|   17 |  726 | `		for(;;){` |
|    - |  727 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  728 | `			 * do this.` |
|    - |  729 | `			 */` |
|   40 |  730 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    5 |  731 | `				pDecoder->pIn++;` |
|    1 |  732 | `			}` |
|   36 |  733 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|   18 |  734 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   16 |  735 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    7 |  736 | `				}` |
|   18 |  737 | `				break;` |
|    - |  738 | `			}` |
|   18 |  739 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   20 |  740 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  741 | `					/* Syntax error,return immediately */` |
|  ! 0 |  742 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  743 | `					return SXERR_ABORT;` |
|    - |  744 | `			}` |
|    - |  745 | `			/* Dequote the key */` |
|   20 |  746 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  747 | `			/* Jump the key and the colon */` |
|   20 |  748 | `			pDecoder->pIn += 2;` |
|    - |  749 | `			/* Recurse and decode the value */` |
|   20 |  750 | `			pDecoder->rec_count++;` |
|   20 |  751 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   20 |  752 | `			pDecoder->rec_count--;` |
|   20 |  753 | `			if( rc == SXERR_ABORT ){` |
|    - |  754 | `				/* Abort processing immediately */` |
|  ! 0 |  755 | `				return SXERR_ABORT;` |
|    - |  756 | `			}` |
|    - |  757 | `			/* Reset the internal buffer of the key */` |
|   20 |  758 | `			ph7_value_reset_string_cursor(pKey);` |
|    - |  759 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    2 |  760 | `		}` |
|    - |  761 | `		/* Restore the old consumer */` |
|   18 |  762 | `		pDecoder->xConsumer = xOld;` |
|   18 |  763 | `		pDecoder->pUserData = pOld;` |
|    - |  764 | `		/* Invoke the old consumer on the decoded object*/` |
|   18 |  765 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - |  766 | `		/* Release the key */` |
|   18 |  767 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|   10 |  768 | `	}else{` |
|    - |  769 | `		/* Unexpected token */` |
|  ! 0 |  770 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - |  771 | `	}` |
|    - |  772 | `	/* Release the worker variable */` |
|   58 |  773 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   58 |  774 | `	return SXRET_OK;` |
|   30 |  775 |  |
|    - |  776 | `/*` |
|    - |  777 | ` * The following JSON decoder callback is invoked each time` |
|    - |  778 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - |  779 | ` * is being decoded.` |
|    - |  780 | ` */` |
|   38 |  781 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  782 |  |
|   40 |  783 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - |  784 | `	/* Insert the entry */` |
|   40 |  785 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   19 |  786 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - |  787 | `	/* All done */` |
|   40 |  788 | `	return SXRET_OK;` |
|    2 |  789 |  |
|    - |  790 | `/*` |
|    - |  791 | ` * Standard JSON decoder callback.` |
|    - |  792 | ` */` |
|   18 |  793 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  794 |  |
|    - |  795 | `	/* Return the value directly */` |
|   20 |  796 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|    9 |  797 | `	SXUNUSED(pKey); /* cc warning */` |
|    9 |  798 | `	SXUNUSED(pUserData);` |
|    - |  799 | `	/* All done */` |
|   20 |  800 | `	return SXRET_OK;` |
|    2 |  801 |  |
|    - |  802 | `/*` |
|    - |  803 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - |  804 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - |  805 | ` * Parameters` |
|    - |  806 | ` *  $json` |
|    - |  807 | ` *    The json string being decoded.` |
|    - |  808 | ` * $assoc` |
|    - |  809 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - |  810 | ` * $depth` |
|    - |  811 | ` *   User specified recursion depth.` |
|    - |  812 | ` * $options` |
|    - |  813 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - |  814 | ` * (default is to cast large integers as floats)` |
|    - |  815 | ` * Return` |
|    - |  816 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - |  817 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - |  818 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - |  819 | ` */` |
|    - |  820 | `/*` |
|    - |  821 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|    - |  822 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|    - |  823 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|    - |  824 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|    - |  825 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|    - |  826 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|    - |  827 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|    - |  828 | ` */` |
|   30 |  829 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth)` |
|    2 |  830 |  |
|   32 |  831 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  832 | `	json_decoder sDecoder;` |
|    - |  833 | `	SySet sToken;` |
|    - |  834 | `	SyLex sLex;` |
|    - |  835 | `	sxi32 rc;` |
|    - |  836 | `	/* Clear JSON error code */` |
|   32 |  837 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  838 | `	/* Tokenize the input */` |
|   32 |  839 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   32 |  840 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   32 |  841 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   32 |  842 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - |  843 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|   14 |  844 | `		SyLexRelease(&sLex);` |
|   14 |  845 | `		SySetRelease(&sToken);` |
|   14 |  846 | `		return pVm->json_rc;` |
|    - |  847 | `	}` |
|    - |  848 | `	/* Fill the decoder */` |
|   20 |  849 | `	sDecoder.pCtx = pCtx;` |
|   20 |  850 | `	sDecoder.pErr = &pVm->json_rc;` |
|   20 |  851 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   20 |  852 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   20 |  853 | `	sDecoder.iFlags = 0;` |
|   20 |  854 | `	if( iAssoc ){` |
|    - |  855 | `		/* Returned objects will be converted into associative arrays */` |
|   18 |  856 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|    8 |  857 | `	}` |
|   20 |  858 | `	sDecoder.rec_depth = 32;` |
|   20 |  859 | `	if( nDepth > 1 && nDepth < 32 ){` |
|  ! 0 |  860 | `		sDecoder.rec_depth = nDepth;` |
|  ! 0 |  861 | `	}` |
|   20 |  862 | `	sDecoder.rec_count = 0;` |
|    - |  863 | `	/* Set a default consumer */` |
|   20 |  864 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   20 |  865 | `	sDecoder.pUserData = 0;` |
|    - |  866 | `	/* Decode the raw JSON input */` |
|   20 |  867 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   20 |  868 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|    - |  869 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|  ! 0 |  870 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 |  871 | `	}` |
|    - |  872 | `	/* Clean-up the mess left behind */` |
|   20 |  873 | `	SyLexRelease(&sLex);` |
|   20 |  874 | `	SySetRelease(&sToken);` |
|   20 |  875 | `	return pVm->json_rc;` |
|   17 |  876 |  |
|   22 |  877 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  878 |  |
|    - |  879 | `	const char *zIn;` |
|    - |  880 | `	int nByte;` |
|   24 |  881 | `	int iAssoc = 0;` |
|   24 |  882 | `	int nDepth = 32;` |
|   24 |  883 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  884 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 |  885 | `		ph7_result_null(pCtx);` |
|  ! 0 |  886 | `		return PH7_OK;` |
|    - |  887 | `	}` |
|    - |  888 | `	/* Extract the JSON string */` |
|   24 |  889 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   24 |  890 | `	if( nByte < 1 ){` |
|    - |  891 | `		/* Empty string,return NULL */` |
|    3 |  892 | `		ph7_result_null(pCtx);` |
|    3 |  893 | `		return PH7_OK;` |
|    - |  894 | `	}` |
|   22 |  895 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|   12 |  896 | `		iAssoc = 1;` |
|    5 |  897 | `	}` |
|   22 |  898 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|  ! 0 |  899 | `		nDepth = ph7_value_to_int(apArg[2]);` |
|  ! 0 |  900 | `	}` |
|    - |  901 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|    - |  902 | `	 * call-context result; on failure we replace it with NULL. */` |
|   22 |  903 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth) != JSON_ERROR_NONE ){` |
|    - |  904 | `		/* Something goes wrong while decoding JSON input.Return NULL. */` |
|   10 |  905 | `		ph7_result_null(pCtx);` |
|    4 |  906 | `	}` |
|    - |  907 | `	/* All done */` |
|   22 |  908 | `	return PH7_OK;` |
|   13 |  909 |  |
|    - |  910 | `/*` |
|    - |  911 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|    - |  912 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|    - |  913 | ` * Parameters` |
|    - |  914 | ` *  $json   The string to validate.` |
|    - |  915 | ` *  $depth  Maximum nesting depth (clamped to the engine limit of 32).` |
|    - |  916 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|    - |  917 | ` * Return` |
|    - |  918 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|    - |  919 | ` */` |
|   12 |  920 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  921 |  |
|   13 |  922 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  923 | `	const char *zIn;` |
|    - |  924 | `	int nByte;` |
|   13 |  925 | `	int nDepth = 32;` |
|   13 |  926 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  927 | `		/* Missing/Invalid argument: not valid JSON */` |
|  ! 0 |  928 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 |  929 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  930 | `		return PH7_OK;` |
|    - |  931 | `	}` |
|    - |  932 | `	/* Extract the JSON string */` |
|   13 |  933 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   13 |  934 | `	if( nByte < 1 ){` |
|    - |  935 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|    - |  936 | `		 * silently, json_validate must record the syntax error) */` |
|    3 |  937 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    3 |  938 | `		ph7_result_bool(pCtx,0);` |
|    3 |  939 | `		return PH7_OK;` |
|    - |  940 | `	}` |
|   11 |  941 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|  ! 0 |  942 | `		nDepth = ph7_value_to_int(apArg[1]);` |
|  ! 0 |  943 | `	}` |
|    - |  944 | `	/* apArg[2] ($flags) is accepted and ignored: no decode flag is implemented.` |
|    - |  945 | `	 * Decode in associative mode so the "objects are returned as an array" warning is` |
|    - |  946 | `	 * not raised - the decoded value is discarded, only its validity matters. */` |
|   11 |  947 | `	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth) == JSON_ERROR_NONE);` |
|   11 |  948 | `	return PH7_OK;` |
|    7 |  949 |  |
|    - |  950 |  |
