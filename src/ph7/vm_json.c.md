# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 336/487 lines (68.99%)

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
|    - |   28 | `};` |
|    - |   29 | `/*` |
|    - |   30 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|    - |   31 | ` * According to wikipedia` |
|    - |   32 | ` * JSON's basic types are:` |
|    - |   33 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |   34 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |   35 | ` *   Boolean (true or false)` |
|    - |   36 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |   37 | ` *    do not need to be of the same type)` |
|    - |   38 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |   39 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |   40 | ` *     be distinct from each other)` |
|    - |   41 | ` *   null (empty)` |
|    - |   42 | ` * Non-significant white space may be added freely around the "structural characters"` |
|    - |   43 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |   44 | ` */` |
|  104 |   45 | `static sxi32 VmJsonEncode(` |
|    - |   46 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |   47 | `	json_private_data *pData /* Context data */` |
|    1 |   48 | `	){` |
|  105 |   49 | `		ph7_context *pCtx = pData->pCtx;` |
|  105 |   50 | `		int iFlags = pData->iFlags;` |
|    - |   51 | `		int nByte;` |
|  105 |   52 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |   53 | `			/* null */` |
|  ! 0 |   54 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|  105 |   55 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   13 |   56 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |   57 | `			int iLen;` |
|    - |   58 | `			/* true/false */` |
|   13 |   59 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   13 |   60 | `			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);` |
|  112 |   61 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|    - |   62 | `			const char *zNum;` |
|    - |   63 | `			/* Get a string representation of the number */` |
|   27 |   64 | `			zNum = ph7_value_to_string(pIn,&nByte);` |
|   27 |   65 | `			ph7_result_string(pCtx,zNum,nByte);` |
|   80 |   66 | `		}else if( ph7_value_is_string(pIn) ){` |
|   31 |   67 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |   68 | `				const char *zNum;` |
|    - |   69 | `				/* Encodes numeric strings as numbers. */` |
|  ! 0 |   70 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    - |   71 | `				/* Get a string representation of the number */` |
|  ! 0 |   72 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|  ! 0 |   73 | `				ph7_result_string(pCtx,zNum,nByte);` |
|  ! 0 |   74 | `			}else{` |
|    - |   75 | `				const char *zIn,*zEnd;` |
|    - |   76 | `				int c;` |
|    - |   77 | `				/* Encode the string */` |
|   31 |   78 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|   31 |   79 | `				zEnd = &zIn[nByte];` |
|    - |   80 | `				/* Append the double quote */` |
|   31 |   81 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|   30 |   82 | `				for(;;){` |
|   61 |   83 | `					if( zIn >= zEnd ){` |
|    - |   84 | `						/* No more input to process */` |
|   31 |   85 | `						break;` |
|    - |   86 | `					}` |
|   31 |   87 | `					c = zIn[0];` |
|    - |   88 | `					/* Advance the stream cursor */` |
|   31 |   89 | `					zIn++;` |
|   31 |   90 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |   91 | `						/* All < and > are converted to \u003C and \u003E */` |
|  ! 0 |   92 | `						if( c == '<' ){` |
|  ! 0 |   93 | `							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|  ! 0 |   94 | `						}else{` |
|  ! 0 |   95 | `							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|    - |   96 | `						}` |
|  ! 0 |   97 | `						continue;` |
|   31 |   98 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |   99 | `						/* All &s are converted to \u0026.  */` |
|  ! 0 |  100 | `						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|  ! 0 |  101 | `						continue;` |
|   31 |  102 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  103 | `						/* All ' are converted to \u0027.   */` |
|  ! 0 |  104 | `						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|  ! 0 |  105 | `						continue;` |
|   31 |  106 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  107 | `						/* All " are converted to \u0022. */` |
|  ! 0 |  108 | `						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|  ! 0 |  109 | `						continue;` |
|    - |  110 | `					}` |
|   31 |  111 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|    - |  112 | `						/* Unescape the character */` |
|  ! 0 |  113 | `						ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|  ! 0 |  114 | `					}` |
|    - |  115 | `					/* Append character verbatim */` |
|   31 |  116 | `					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    1 |  117 | `				}` |
|    - |  118 | `				/* Append the double quote */` |
|   31 |  119 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|    1 |  120 | `			}` |
|   52 |  121 | `		}else if( ph7_value_is_array(pIn) ){` |
|    - |  122 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|    - |  123 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|    - |  124 | `			 * object with stringified keys (PHP semantics). */` |
|   72 |  125 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|   36 |  126 | `				\|\| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);` |
|   37 |  127 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|   37 |  128 | `			int c = isObject ? '{' : '[';` |
|   37 |  129 | `			int d = isObject ? '}' : ']';` |
|    - |  130 | `			/* Encode the array */` |
|   37 |  131 | `			pData->isObject = isObject;` |
|   37 |  132 | `			pData->isFirst = 1;` |
|    - |  133 | `			/* Append the square bracket or curly braces */` |
|   37 |  134 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    - |  135 | `			/* Iterate throw array entries */` |
|   37 |  136 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  137 | `			/* Append the closing square bracket or curly braces */` |
|   37 |  138 | `			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));` |
|   37 |  139 | `			pData->isObject = savedObject;` |
|   18 |  140 | `		}else if( ph7_value_is_object(pIn) ){` |
|    - |  141 | `			/* Encode the class instance */` |
|  ! 0 |  142 | `			pData->isFirst = 1;` |
|    - |  143 | `			/* Append the curly braces */` |
|  ! 0 |  144 | `			ph7_result_string(pCtx,"{",(int)sizeof(char));` |
|    - |  145 | `			/* Iterate throw class attribute */` |
|  ! 0 |  146 | `			ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|    - |  147 | `			/* Append the closing curly braces  */` |
|  ! 0 |  148 | `			ph7_result_string(pCtx,"}",(int)sizeof(char));` |
|  ! 0 |  149 | `		}else{` |
|    - |  150 | `			/* Can't happen */` |
|  ! 0 |  151 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|    - |  152 | `		}` |
|    - |  153 | `		/* All done */` |
|  105 |  154 | `		return PH7_OK;` |
|    1 |  155 |  |
|    - |  156 | `/*` |
|    - |  157 | ` * The following walker callback is invoked each time we need` |
|    - |  158 | ` * to encode an array to JSON.` |
|    - |  159 | ` */` |
|   78 |  160 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    1 |  161 |  |
|   79 |  162 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|   79 |  163 | `	if( pJson->nRecCount > 31 ){` |
|    - |  164 | `		/* Recursion limit reached,return immediately */` |
|  ! 0 |  165 | `		return PH7_OK;` |
|    - |  166 | `	}` |
|   79 |  167 | `	if( !pJson->isFirst ){` |
|    - |  168 | `		/* Append the colon first */` |
|   45 |  169 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|   22 |  170 | `	}` |
|   79 |  171 | `	if( pJson->isObject ){` |
|    - |  172 | `		/* Outputs an object rather than an array */` |
|    - |  173 | `		const char *zKey;` |
|    - |  174 | `		int nByte;` |
|    - |  175 | `		/* Extract a string representation of the key */` |
|   31 |  176 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  177 | `		/* Append the key and the double colon */` |
|   31 |  178 | `		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);` |
|   15 |  179 | `	}` |
|    - |  180 | `	/* Encode the value */` |
|   79 |  181 | `	pJson->nRecCount++;` |
|   79 |  182 | `	VmJsonEncode(pValue,pJson);` |
|   79 |  183 | `	pJson->nRecCount--;` |
|   79 |  184 | `	pJson->isFirst = 0;` |
|   79 |  185 | `	return PH7_OK;` |
|   40 |  186 |  |
|    - |  187 | `/*` |
|    - |  188 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  189 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  190 | ` */` |
|  ! 0 |  191 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|  ! 0 |  192 |  |
|  ! 0 |  193 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  ! 0 |  194 | `	if( pJson->nRecCount > 31 ){` |
|    - |  195 | `		/* Recursion limit reached,return immediately */` |
|  ! 0 |  196 | `		return PH7_OK;` |
|    - |  197 | `	}` |
|  ! 0 |  198 | `	if( !pJson->isFirst ){` |
|    - |  199 | `		/* Append the colon first */` |
|  ! 0 |  200 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|  ! 0 |  201 | `	}` |
|    - |  202 | `	/* Append the attribute name and the double colon first */` |
|  ! 0 |  203 | `	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);` |
|    - |  204 | `	/* Encode the value */` |
|  ! 0 |  205 | `	pJson->nRecCount++;` |
|  ! 0 |  206 | `	VmJsonEncode(pValue,pJson);` |
|  ! 0 |  207 | `	pJson->nRecCount--;` |
|  ! 0 |  208 | `	pJson->isFirst = 0;` |
|  ! 0 |  209 | `	return PH7_OK;` |
|  ! 0 |  210 |  |
|    - |  211 | `/*` |
|    - |  212 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  213 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  214 | ` * Parameters` |
|    - |  215 | ` *  $value` |
|    - |  216 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  217 | ` * $options` |
|    - |  218 | ` *  Bitmask consisting of:` |
|    - |  219 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  220 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  221 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  222 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  223 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  224 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  225 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  226 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  227 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  228 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  229 | ` * Return` |
|    - |  230 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  231 | ` */` |
|   26 |  232 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  233 |  |
|    - |  234 | `	json_private_data sJson;` |
|   27 |  235 | `	if( nArg < 1 ){` |
|    - |  236 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  237 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  238 | `		return PH7_OK;` |
|    - |  239 | `	}` |
|    - |  240 | `	/* Prepare the JSON data */` |
|   27 |  241 | `	sJson.nRecCount = 0;` |
|   27 |  242 | `	sJson.pCtx = pCtx;` |
|   27 |  243 | `	sJson.isFirst = 1;` |
|   27 |  244 | `	sJson.iFlags = 0;` |
|   27 |  245 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  246 | `		/* Extract option flags */` |
|    3 |  247 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|    1 |  248 | `	}` |
|    - |  249 | `	/* Perform the encoding operation */` |
|   27 |  250 | `	VmJsonEncode(apArg[0],&sJson);` |
|    - |  251 | `	/* All done */` |
|   27 |  252 | `	return PH7_OK;` |
|   14 |  253 |  |
|    - |  254 | `/*` |
|    - |  255 | ` * int json_last_error(void)` |
|    - |  256 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  257 | ` * Parameters` |
|    - |  258 | ` *  None` |
|    - |  259 | ` * Return` |
|    - |  260 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  261 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  262 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  263 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  264 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  265 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  266 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  267 | ` */` |
|   10 |  268 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  269 |  |
|   12 |  270 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  271 | `	/* Return the error code */` |
|   12 |  272 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    5 |  273 | `	SXUNUSED(nArg); /* cc warning */` |
|    5 |  274 | `	SXUNUSED(apArg);` |
|   12 |  275 | `	return PH7_OK;` |
|    2 |  276 |  |
|    - |  277 | `/*` |
|    - |  278 | ` * string json_last_error_msg(void)` |
|    - |  279 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|    - |  280 | ` * Parameters` |
|    - |  281 | ` *  None` |
|    - |  282 | ` * Return` |
|    - |  283 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|    - |  284 | ` *  code, or "No error" if no error has occurred.` |
|    - |  285 | ` */` |
|    4 |  286 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  287 |  |
|    5 |  288 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  289 | `	const char *zMsg;` |
|    5 |  290 | `	switch( pVm->json_rc ){` |
|    1 |  291 | `	case JSON_ERROR_NONE:` |
|    3 |  292 | `		zMsg = "No error";` |
|    3 |  293 | `		break;` |
|  ! 0 |  294 | `	case JSON_ERROR_DEPTH:` |
|  ! 0 |  295 | `		zMsg = "Maximum stack depth exceeded";` |
|  ! 0 |  296 | `		break;` |
|  ! 0 |  297 | `	case JSON_ERROR_STATE_MISMATCH:` |
|  ! 0 |  298 | `		zMsg = "State mismatch (invalid or malformed JSON)";` |
|  ! 0 |  299 | `		break;` |
|  ! 0 |  300 | `	case JSON_ERROR_CTRL_CHAR:` |
|  ! 0 |  301 | `		zMsg = "Control character error, possibly incorrectly encoded";` |
|  ! 0 |  302 | `		break;` |
|    1 |  303 | `	case JSON_ERROR_SYNTAX:` |
|    3 |  304 | `		zMsg = "Syntax error";` |
|    3 |  305 | `		break;` |
|  ! 0 |  306 | `	case JSON_ERROR_UTF8:` |
|  ! 0 |  307 | `		zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|  ! 0 |  308 | `		break;` |
|  ! 0 |  309 | `	default:` |
|  ! 0 |  310 | `		zMsg = "Unknown error";` |
|  ! 0 |  311 | `		break;` |
|    - |  312 | `	}` |
|    5 |  313 | `	ph7_result_string(pCtx,zMsg,-1/* Compute length automatically */);` |
|    2 |  314 | `	SXUNUSED(nArg); /* cc warning */` |
|    2 |  315 | `	SXUNUSED(apArg);` |
|    5 |  316 | `	return PH7_OK;` |
|    1 |  317 |  |
|    - |  318 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  319 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  320 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  321 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  322 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  323 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  324 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  325 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  326 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  327 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  328 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  329 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  330 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  331 | `/*` |
|    - |  332 | ` * Tokenize an entire JSON input.` |
|    - |  333 | ` * Get a single low-level token from the input file.` |
|    - |  334 | ` * Update the stream pointer so that it points to the first` |
|    - |  335 | ` * character beyond the extracted token.` |
|    - |  336 | ` */` |
|  144 |  337 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    2 |  338 |  |
|  146 |  339 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  340 | `	SyString *pStr;` |
|    - |  341 | `	int c;` |
|    - |  342 | `	/* Ignore leading white spaces */` |
|  150 |  343 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  344 | `		/* Advance the stream cursor */` |
|    6 |  345 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  346 | `			/* Update line counter */` |
|  ! 0 |  347 | `			pStream->nLine++;` |
|  ! 0 |  348 | `		}` |
|    6 |  349 | `		pStream->zText++;` |
|    2 |  350 | `	}` |
|  146 |  351 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  352 | `		/* End of input reached */` |
|  ! 0 |  353 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  354 | `		return SXERR_EOF;` |
|    - |  355 | `	}` |
|    - |  356 | `	/* Record token starting position and line */` |
|  146 |  357 | `	pToken->nLine = pStream->nLine;` |
|  146 |  358 | `	pToken->pUserData = 0;` |
|  146 |  359 | `	pStr = &pToken->sData;` |
|  146 |  360 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  183 |  361 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|  102 |  362 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  363 | `			/* Single character */` |
|   84 |  364 | `			c = pStream->zText[0];` |
|    - |  365 | `			/* Set token type */` |
|   84 |  366 | `			switch(c){` |
|    9 |  367 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   20 |  368 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|   16 |  369 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|    9 |  370 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   20 |  371 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   17 |  372 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  373 | `			default:` |
|  ! 0 |  374 | `				break;` |
|    - |  375 | `			}` |
|    - |  376 | `			/* Advance the stream cursor */` |
|   84 |  377 | `			pStream->zText++;` |
|  105 |  378 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  379 | `		/* JSON string */` |
|   26 |  380 | `		pStream->zText++;` |
|   26 |  381 | `		pStr->zString++;` |
|    - |  382 | `		/* Delimit the string */` |
|   72 |  383 | `		while( pStream->zText < pStream->zEnd ){` |
|   72 |  384 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   26 |  385 | `				break;` |
|    - |  386 | `			}` |
|   48 |  387 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  388 | `				/* Update line counter */` |
|  ! 0 |  389 | `				pStream->nLine++;` |
|  ! 0 |  390 | `			}` |
|   48 |  391 | `			pStream->zText++;` |
|    2 |  392 | `		}` |
|   26 |  393 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  394 | `			/* Missing closing '"' */` |
|  ! 0 |  395 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  396 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  397 | `		}else{` |
|   26 |  398 | `			pToken->nType = JSON_TK_STR;` |
|   26 |  399 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    2 |  400 | `		}` |
|   52 |  401 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    - |  402 | `		/* Number */` |
|   27 |  403 | `		pStream->zText++;` |
|   27 |  404 | `		pToken->nType = JSON_TK_NUM;` |
|   27 |  405 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  406 | `			pStream->zText++;` |
|  ! 0 |  407 | `		}` |
|   27 |  408 | `		if( pStream->zText < pStream->zEnd ){` |
|   27 |  409 | `			c = pStream->zText[0];` |
|   27 |  410 | `			if( c == '.' ){` |
|    - |  411 | `					/* Real number */` |
|  ! 0 |  412 | `					pStream->zText++;` |
|  ! 0 |  413 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  414 | `						pStream->zText++;` |
|  ! 0 |  415 | `					}` |
|  ! 0 |  416 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  417 | `						c = pStream->zText[0];` |
|  ! 0 |  418 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  419 | `							pStream->zText++;` |
|  ! 0 |  420 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  421 | `								c = pStream->zText[0];` |
|  ! 0 |  422 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  423 | `									pStream->zText++;` |
|  ! 0 |  424 | `								}` |
|  ! 0 |  425 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  426 | `									pStream->zText++;` |
|  ! 0 |  427 | `								}` |
|  ! 0 |  428 | `							}` |
|  ! 0 |  429 | `						}` |
|  ! 0 |  430 | `					}` |
|   27 |  431 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  432 | `					/* Real number */` |
|  ! 0 |  433 | `					pStream->zText++;` |
|  ! 0 |  434 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  435 | `						c = pStream->zText[0];` |
|  ! 0 |  436 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  437 | `							pStream->zText++;` |
|  ! 0 |  438 | `						}` |
|  ! 0 |  439 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  440 | `							pStream->zText++;` |
|  ! 0 |  441 | `						}` |
|  ! 0 |  442 | `					}` |
|  ! 0 |  443 | `				}` |
|   14 |  444 | `			}` |
|   33 |  445 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|   12 |  446 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  447 | `			/* boolean true */` |
|  ! 0 |  448 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  449 | `			/* Advance the stream cursor */` |
|  ! 0 |  450 | `			pStream->zText += sizeof("true")-1;` |
|   20 |  451 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|   12 |  452 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  453 | `			/* boolean false */` |
|  ! 0 |  454 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  455 | `			/* Advance the stream cursor */` |
|  ! 0 |  456 | `			pStream->zText += sizeof("false")-1;` |
|   20 |  457 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|   12 |  458 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  459 | `			/* NULL */` |
|  ! 0 |  460 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  461 | `			/* Advance the stream cursor */` |
|  ! 0 |  462 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  463 | `	}else{` |
|    - |  464 | `		/* Unexpected token */` |
|   14 |  465 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  466 | `		/* Advance the stream cursor */` |
|   14 |  467 | `		pStream->zText++;` |
|   14 |  468 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  469 | `		/* Abort processing immediatley */` |
|   14 |  470 | `		return SXERR_ABORT;` |
|    - |  471 | `	}` |
|    - |  472 | `	/* record token length */` |
|  134 |  473 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  134 |  474 | `	if( pToken->nType == JSON_TK_STR ){` |
|   26 |  475 | `		pStr->nByte--;` |
|   12 |  476 | `	}` |
|    - |  477 | `	/* Return to the lexer */` |
|  134 |  478 | `	return SXRET_OK;` |
|   74 |  479 |  |
|    - |  480 | `/*` |
|    - |  481 | ` * JSON decoded input consumer callback signature.` |
|    - |  482 | ` */` |
|    - |  483 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  484 | `/*` |
|    - |  485 | ` * JSON decoder state is kept in the following structure.` |
|    - |  486 | ` */` |
|    - |  487 | `typedef struct json_decoder json_decoder;` |
|    - |  488 | `struct json_decoder` |
|    - |  489 |  |
|    - |  490 | `	ph7_context *pCtx; /* Call context */` |
|    - |  491 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  492 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  493 | `	int iFlags;        /* Configuration flags */` |
|    - |  494 | `	SyToken *pIn;      /* Token stream */` |
|    - |  495 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  496 | `	int rec_depth;     /* Recursion limit */` |
|    - |  497 | `	int rec_count;     /* Current nesting level */` |
|    - |  498 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  499 | `};` |
|    - |  500 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  501 | `/* Forward declaration */` |
|    - |  502 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  503 | `/*` |
|    - |  504 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  505 | ` * the result in the given ph7_value.` |
|    - |  506 | ` */` |
|   24 |  507 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    2 |  508 |  |
|   26 |  509 | `	const char *zIn = pStr->zString;` |
|   26 |  510 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  511 | `	const char *zCur;` |
|    - |  512 | `	int c;` |
|    - |  513 | `	/* Mark the value as a string */` |
|   26 |  514 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|   12 |  515 | `	for(;;){` |
|   26 |  516 | `		zCur = zIn;` |
|   72 |  517 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   48 |  518 | `			zIn++;` |
|    2 |  519 | `		}` |
|   26 |  520 | `		if( zIn > zCur ){` |
|    - |  521 | `			/* Append chunk verbatim */` |
|   26 |  522 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|   12 |  523 | `		}` |
|   26 |  524 | `		zIn++;` |
|   26 |  525 | `		if( zIn >= zEnd ){` |
|    - |  526 | `			/* End of the input reached */` |
|   26 |  527 | `			break;` |
|    - |  528 | `		}` |
|  ! 0 |  529 | `		c = zIn[0];` |
|    - |  530 | `		/* Unescape the character */` |
|  ! 0 |  531 | `		switch(c){` |
|  ! 0 |  532 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  533 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  534 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  535 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  536 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  537 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  538 | `		default:` |
|  ! 0 |  539 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  540 | `			break;` |
|    - |  541 | `		}` |
|    - |  542 | `		/* Advance the stream cursor */` |
|  ! 0 |  543 | `		zIn++;` |
|  ! 0 |  544 | `	}` |
|   26 |  545 |  |
|    - |  546 | `/*` |
|    - |  547 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  548 | ` * According to wikipedia` |
|    - |  549 | ` * JSON's basic types are:` |
|    - |  550 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  551 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  552 | ` *   Boolean (true or false)` |
|    - |  553 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  554 | ` *    do not need to be of the same type)` |
|    - |  555 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  556 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  557 | ` *     be distinct from each other)` |
|    - |  558 | ` *   null (empty)` |
|    - |  559 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  560 | ` */` |
|   56 |  561 | `static sxi32 VmJsonDecode(` |
|    - |  562 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  563 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    2 |  564 | `	){` |
|    - |  565 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  566 | `	sxi32 rc;` |
|    - |  567 | `	/* Check if we do not nest to much */` |
|   58 |  568 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  569 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  570 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  571 | `		return SXERR_ABORT;` |
|    - |  572 | `	}` |
|   58 |  573 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  574 | `		/* Scalar value */` |
|   34 |  575 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   34 |  576 | `		if( pWorker == 0 ){` |
|  ! 0 |  577 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  578 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  579 | `			return SXERR_ABORT;` |
|    - |  580 | `		}` |
|    - |  581 | `		/* Reflect the JSON image */` |
|   34 |  582 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  583 | `			/* Nullify the value.*/` |
|  ! 0 |  584 | `			ph7_value_null(pWorker);` |
|   34 |  585 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  586 | `			/* Boolean value */` |
|  ! 0 |  587 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   34 |  588 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   27 |  589 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  590 | `			/*` |
|    - |  591 | `			 * Numeric value.` |
|    - |  592 | `			 * Get a string representation first then try to get a numeric` |
|    - |  593 | `			 * value.` |
|    - |  594 | `			 */` |
|   27 |  595 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  596 | `			/* Obtain a numeric representation */` |
|   27 |  597 | `			PH7_MemObjToNumeric(pWorker);` |
|   14 |  598 | `		}else{` |
|    - |  599 | `			/* Dequote the string */` |
|    8 |  600 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  601 | `		}` |
|    - |  602 | `		/* Invoke the consumer callback */` |
|   34 |  603 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   34 |  604 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  605 | `			return SXERR_ABORT;` |
|    - |  606 | `		}` |
|    - |  607 | `		/* All done,advance the stream cursor */` |
|   34 |  608 | `		pDecoder->pIn++;` |
|   42 |  609 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  610 | `		ProcJsonConsumer xOld;` |
|    - |  611 | `		void *pOld;` |
|    - |  612 | `		/* Array representation*/` |
|    9 |  613 | `		pDecoder->pIn++;` |
|    - |  614 | `		/* Create a working array */` |
|    9 |  615 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|    9 |  616 | `		if( pWorker == 0 ){` |
|  ! 0 |  617 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  618 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  619 | `			return SXERR_ABORT;` |
|    - |  620 | `		}` |
|    - |  621 | `		/* Save the old consumer */` |
|    9 |  622 | `		xOld = pDecoder->xConsumer;` |
|    9 |  623 | `		pOld = pDecoder->pUserData;` |
|    - |  624 | `		/* Set the new consumer */` |
|    9 |  625 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|    9 |  626 | `		pDecoder->pUserData = pWorker;` |
|    - |  627 | `		/* Decode the array */` |
|   14 |  628 | `		for(;;){` |
|    - |  629 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  630 | `			 * do this.` |
|    - |  631 | `			 */` |
|   41 |  632 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   13 |  633 | `				pDecoder->pIn++;` |
|    1 |  634 | `			}` |
|   29 |  635 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|    9 |  636 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|    9 |  637 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    4 |  638 | `				}` |
|    9 |  639 | `				break;` |
|    - |  640 | `			}` |
|    - |  641 | `			/* Recurse and decode the entry */` |
|   21 |  642 | `			pDecoder->rec_count++;` |
|   21 |  643 | `			rc = VmJsonDecode(pDecoder,0);` |
|   21 |  644 | `			pDecoder->rec_count--;` |
|   21 |  645 | `			if( rc == SXERR_ABORT ){` |
|    - |  646 | `				/* Abort processing immediately */` |
|  ! 0 |  647 | `				return SXERR_ABORT;` |
|    - |  648 | `			}` |
|    - |  649 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   21 |  650 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   20 |  651 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  652 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  653 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  654 | `					return SXERR_ABORT;` |
|    - |  655 | `			}` |
|    1 |  656 | `		}` |
|    - |  657 | `		/* Restore the old consumer */` |
|    9 |  658 | `		pDecoder->xConsumer = xOld;` |
|    9 |  659 | `		pDecoder->pUserData = pOld;` |
|    - |  660 | `		/* Invoke the old consumer on the decoded array */` |
|    9 |  661 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   22 |  662 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  663 | `		ProcJsonConsumer xOld;` |
|    - |  664 | `		ph7_value *pKey;` |
|    - |  665 | `		void *pOld;` |
|    - |  666 | `		/* Object representation*/` |
|   18 |  667 | `		pDecoder->pIn++;` |
|    - |  668 | `		/* Return the object as an associative array */` |
|   18 |  669 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|    3 |  670 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|    - |  671 | `				"JSON Objects are always returned as an associative array"` |
|    - |  672 | `				);` |
|    1 |  673 | `		}` |
|    - |  674 | `		/* Create a working array */` |
|   18 |  675 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   18 |  676 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   18 |  677 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  678 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  679 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  680 | `			return SXERR_ABORT;` |
|    - |  681 | `		}` |
|    - |  682 | `		/* Save the old consumer */` |
|   18 |  683 | `		xOld = pDecoder->xConsumer;` |
|   18 |  684 | `		pOld = pDecoder->pUserData;` |
|    - |  685 | `		/* Set the new consumer */` |
|   18 |  686 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   18 |  687 | `		pDecoder->pUserData = pWorker;` |
|    - |  688 | `		/* Decode the object */` |
|   17 |  689 | `		for(;;){` |
|    - |  690 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  691 | `			 * do this.` |
|    - |  692 | `			 */` |
|   40 |  693 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    5 |  694 | `				pDecoder->pIn++;` |
|    1 |  695 | `			}` |
|   36 |  696 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|   18 |  697 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   16 |  698 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    7 |  699 | `				}` |
|   18 |  700 | `				break;` |
|    - |  701 | `			}` |
|   18 |  702 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   20 |  703 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  704 | `					/* Syntax error,return immediately */` |
|  ! 0 |  705 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  706 | `					return SXERR_ABORT;` |
|    - |  707 | `			}` |
|    - |  708 | `			/* Dequote the key */` |
|   20 |  709 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  710 | `			/* Jump the key and the colon */` |
|   20 |  711 | `			pDecoder->pIn += 2;` |
|    - |  712 | `			/* Recurse and decode the value */` |
|   20 |  713 | `			pDecoder->rec_count++;` |
|   20 |  714 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   20 |  715 | `			pDecoder->rec_count--;` |
|   20 |  716 | `			if( rc == SXERR_ABORT ){` |
|    - |  717 | `				/* Abort processing immediately */` |
|  ! 0 |  718 | `				return SXERR_ABORT;` |
|    - |  719 | `			}` |
|    - |  720 | `			/* Reset the internal buffer of the key */` |
|   20 |  721 | `			ph7_value_reset_string_cursor(pKey);` |
|    - |  722 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    2 |  723 | `		}` |
|    - |  724 | `		/* Restore the old consumer */` |
|   18 |  725 | `		pDecoder->xConsumer = xOld;` |
|   18 |  726 | `		pDecoder->pUserData = pOld;` |
|    - |  727 | `		/* Invoke the old consumer on the decoded object*/` |
|   18 |  728 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - |  729 | `		/* Release the key */` |
|   18 |  730 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|   10 |  731 | `	}else{` |
|    - |  732 | `		/* Unexpected token */` |
|  ! 0 |  733 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - |  734 | `	}` |
|    - |  735 | `	/* Release the worker variable */` |
|   58 |  736 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   58 |  737 | `	return SXRET_OK;` |
|   30 |  738 |  |
|    - |  739 | `/*` |
|    - |  740 | ` * The following JSON decoder callback is invoked each time` |
|    - |  741 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - |  742 | ` * is being decoded.` |
|    - |  743 | ` */` |
|   38 |  744 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  745 |  |
|   40 |  746 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - |  747 | `	/* Insert the entry */` |
|   40 |  748 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   19 |  749 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - |  750 | `	/* All done */` |
|   40 |  751 | `	return SXRET_OK;` |
|    2 |  752 |  |
|    - |  753 | `/*` |
|    - |  754 | ` * Standard JSON decoder callback.` |
|    - |  755 | ` */` |
|   18 |  756 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  757 |  |
|    - |  758 | `	/* Return the value directly */` |
|   20 |  759 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|    9 |  760 | `	SXUNUSED(pKey); /* cc warning */` |
|    9 |  761 | `	SXUNUSED(pUserData);` |
|    - |  762 | `	/* All done */` |
|   20 |  763 | `	return SXRET_OK;` |
|    2 |  764 |  |
|    - |  765 | `/*` |
|    - |  766 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - |  767 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - |  768 | ` * Parameters` |
|    - |  769 | ` *  $json` |
|    - |  770 | ` *    The json string being decoded.` |
|    - |  771 | ` * $assoc` |
|    - |  772 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - |  773 | ` * $depth` |
|    - |  774 | ` *   User specified recursion depth.` |
|    - |  775 | ` * $options` |
|    - |  776 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - |  777 | ` * (default is to cast large integers as floats)` |
|    - |  778 | ` * Return` |
|    - |  779 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - |  780 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - |  781 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - |  782 | ` */` |
|    - |  783 | `/*` |
|    - |  784 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|    - |  785 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|    - |  786 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|    - |  787 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|    - |  788 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|    - |  789 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|    - |  790 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|    - |  791 | ` */` |
|   30 |  792 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth)` |
|    2 |  793 |  |
|   32 |  794 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  795 | `	json_decoder sDecoder;` |
|    - |  796 | `	SySet sToken;` |
|    - |  797 | `	SyLex sLex;` |
|    - |  798 | `	sxi32 rc;` |
|    - |  799 | `	/* Clear JSON error code */` |
|   32 |  800 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  801 | `	/* Tokenize the input */` |
|   32 |  802 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   32 |  803 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   32 |  804 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   32 |  805 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - |  806 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|   14 |  807 | `		SyLexRelease(&sLex);` |
|   14 |  808 | `		SySetRelease(&sToken);` |
|   14 |  809 | `		return pVm->json_rc;` |
|    - |  810 | `	}` |
|    - |  811 | `	/* Fill the decoder */` |
|   20 |  812 | `	sDecoder.pCtx = pCtx;` |
|   20 |  813 | `	sDecoder.pErr = &pVm->json_rc;` |
|   20 |  814 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   20 |  815 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   20 |  816 | `	sDecoder.iFlags = 0;` |
|   20 |  817 | `	if( iAssoc ){` |
|    - |  818 | `		/* Returned objects will be converted into associative arrays */` |
|   18 |  819 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|    8 |  820 | `	}` |
|   20 |  821 | `	sDecoder.rec_depth = 32;` |
|   20 |  822 | `	if( nDepth > 1 && nDepth < 32 ){` |
|  ! 0 |  823 | `		sDecoder.rec_depth = nDepth;` |
|  ! 0 |  824 | `	}` |
|   20 |  825 | `	sDecoder.rec_count = 0;` |
|    - |  826 | `	/* Set a default consumer */` |
|   20 |  827 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   20 |  828 | `	sDecoder.pUserData = 0;` |
|    - |  829 | `	/* Decode the raw JSON input */` |
|   20 |  830 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   20 |  831 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|    - |  832 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|  ! 0 |  833 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 |  834 | `	}` |
|    - |  835 | `	/* Clean-up the mess left behind */` |
|   20 |  836 | `	SyLexRelease(&sLex);` |
|   20 |  837 | `	SySetRelease(&sToken);` |
|   20 |  838 | `	return pVm->json_rc;` |
|   17 |  839 |  |
|   22 |  840 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  841 |  |
|    - |  842 | `	const char *zIn;` |
|    - |  843 | `	int nByte;` |
|   24 |  844 | `	int iAssoc = 0;` |
|   24 |  845 | `	int nDepth = 32;` |
|   24 |  846 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  847 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 |  848 | `		ph7_result_null(pCtx);` |
|  ! 0 |  849 | `		return PH7_OK;` |
|    - |  850 | `	}` |
|    - |  851 | `	/* Extract the JSON string */` |
|   24 |  852 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   24 |  853 | `	if( nByte < 1 ){` |
|    - |  854 | `		/* Empty string,return NULL */` |
|    3 |  855 | `		ph7_result_null(pCtx);` |
|    3 |  856 | `		return PH7_OK;` |
|    - |  857 | `	}` |
|   22 |  858 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|   12 |  859 | `		iAssoc = 1;` |
|    5 |  860 | `	}` |
|   22 |  861 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|  ! 0 |  862 | `		nDepth = ph7_value_to_int(apArg[2]);` |
|  ! 0 |  863 | `	}` |
|    - |  864 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|    - |  865 | `	 * call-context result; on failure we replace it with NULL. */` |
|   22 |  866 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth) != JSON_ERROR_NONE ){` |
|    - |  867 | `		/* Something goes wrong while decoding JSON input.Return NULL. */` |
|   10 |  868 | `		ph7_result_null(pCtx);` |
|    4 |  869 | `	}` |
|    - |  870 | `	/* All done */` |
|   22 |  871 | `	return PH7_OK;` |
|   13 |  872 |  |
|    - |  873 | `/*` |
|    - |  874 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|    - |  875 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|    - |  876 | ` * Parameters` |
|    - |  877 | ` *  $json   The string to validate.` |
|    - |  878 | ` *  $depth  Maximum nesting depth (clamped to the engine limit of 32).` |
|    - |  879 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|    - |  880 | ` * Return` |
|    - |  881 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|    - |  882 | ` */` |
|   12 |  883 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  884 |  |
|   13 |  885 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  886 | `	const char *zIn;` |
|    - |  887 | `	int nByte;` |
|   13 |  888 | `	int nDepth = 32;` |
|   13 |  889 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  890 | `		/* Missing/Invalid argument: not valid JSON */` |
|  ! 0 |  891 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 |  892 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  893 | `		return PH7_OK;` |
|    - |  894 | `	}` |
|    - |  895 | `	/* Extract the JSON string */` |
|   13 |  896 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   13 |  897 | `	if( nByte < 1 ){` |
|    - |  898 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|    - |  899 | `		 * silently, json_validate must record the syntax error) */` |
|    3 |  900 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    3 |  901 | `		ph7_result_bool(pCtx,0);` |
|    3 |  902 | `		return PH7_OK;` |
|    - |  903 | `	}` |
|   11 |  904 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|  ! 0 |  905 | `		nDepth = ph7_value_to_int(apArg[1]);` |
|  ! 0 |  906 | `	}` |
|    - |  907 | `	/* apArg[2] ($flags) is accepted and ignored: no decode flag is implemented.` |
|    - |  908 | `	 * Decode in associative mode so the "objects are returned as an array" warning is` |
|    - |  909 | `	 * not raised - the decoded value is discarded, only its validity matters. */` |
|   11 |  910 | `	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth) == JSON_ERROR_NONE);` |
|   11 |  911 | `	return PH7_OK;` |
|    7 |  912 |  |
|    - |  913 |  |
