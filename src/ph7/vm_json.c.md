# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 295/427 lines (69.09%)

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
|    8 |  268 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  269 |  |
|   10 |  270 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  271 | `	/* Return the error code */` |
|   10 |  272 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    4 |  273 | `	SXUNUSED(nArg); /* cc warning */` |
|    4 |  274 | `	SXUNUSED(apArg);` |
|   10 |  275 | `	return PH7_OK;` |
|    2 |  276 |  |
|    - |  277 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  278 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  279 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  280 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  281 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  282 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  283 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  284 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  285 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  286 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  287 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  288 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  289 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  290 | `/*` |
|    - |  291 | ` * Tokenize an entire JSON input.` |
|    - |  292 | ` * Get a single low-level token from the input file.` |
|    - |  293 | ` * Update the stream pointer so that it points to the first` |
|    - |  294 | ` * character beyond the extracted token.` |
|    - |  295 | ` */` |
|   86 |  296 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    2 |  297 |  |
|   88 |  298 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  299 | `	SyString *pStr;` |
|    - |  300 | `	int c;` |
|    - |  301 | `	/* Ignore leading white spaces */` |
|   92 |  302 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  303 | `		/* Advance the stream cursor */` |
|    6 |  304 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  305 | `			/* Update line counter */` |
|  ! 0 |  306 | `			pStream->nLine++;` |
|  ! 0 |  307 | `		}` |
|    6 |  308 | `		pStream->zText++;` |
|    2 |  309 | `	}` |
|   88 |  310 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  311 | `		/* End of input reached */` |
|  ! 0 |  312 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  313 | `		return SXERR_EOF;` |
|    - |  314 | `	}` |
|    - |  315 | `	/* Record token starting position and line */` |
|   88 |  316 | `	pToken->nLine = pStream->nLine;` |
|   88 |  317 | `	pToken->pUserData = 0;` |
|   88 |  318 | `	pStr = &pToken->sData;` |
|   88 |  319 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  110 |  320 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|   63 |  321 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  322 | `			/* Single character */` |
|   52 |  323 | `			c = pStream->zText[0];` |
|    - |  324 | `			/* Set token type */` |
|   52 |  325 | `			switch(c){` |
|    7 |  326 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   12 |  327 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|    8 |  328 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|    7 |  329 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   12 |  330 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   13 |  331 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  332 | `			default:` |
|  ! 0 |  333 | `				break;` |
|    - |  334 | `			}` |
|    - |  335 | `			/* Advance the stream cursor */` |
|   52 |  336 | `			pStream->zText++;` |
|   63 |  337 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  338 | `		/* JSON string */` |
|   18 |  339 | `		pStream->zText++;` |
|   18 |  340 | `		pStr->zString++;` |
|    - |  341 | `		/* Delimit the string */` |
|   56 |  342 | `		while( pStream->zText < pStream->zEnd ){` |
|   56 |  343 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   18 |  344 | `				break;` |
|    - |  345 | `			}` |
|   40 |  346 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  347 | `				/* Update line counter */` |
|  ! 0 |  348 | `				pStream->nLine++;` |
|  ! 0 |  349 | `			}` |
|   40 |  350 | `			pStream->zText++;` |
|    2 |  351 | `		}` |
|   18 |  352 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  353 | `			/* Missing closing '"' */` |
|  ! 0 |  354 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  355 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  356 | `		}else{` |
|   18 |  357 | `			pToken->nType = JSON_TK_STR;` |
|   18 |  358 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    2 |  359 | `		}` |
|   30 |  360 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    - |  361 | `		/* Number */` |
|   15 |  362 | `		pStream->zText++;` |
|   15 |  363 | `		pToken->nType = JSON_TK_NUM;` |
|   15 |  364 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  365 | `			pStream->zText++;` |
|  ! 0 |  366 | `		}` |
|   15 |  367 | `		if( pStream->zText < pStream->zEnd ){` |
|   15 |  368 | `			c = pStream->zText[0];` |
|   15 |  369 | `			if( c == '.' ){` |
|    - |  370 | `					/* Real number */` |
|  ! 0 |  371 | `					pStream->zText++;` |
|  ! 0 |  372 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  373 | `						pStream->zText++;` |
|  ! 0 |  374 | `					}` |
|  ! 0 |  375 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  376 | `						c = pStream->zText[0];` |
|  ! 0 |  377 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  378 | `							pStream->zText++;` |
|  ! 0 |  379 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  380 | `								c = pStream->zText[0];` |
|  ! 0 |  381 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  382 | `									pStream->zText++;` |
|  ! 0 |  383 | `								}` |
|  ! 0 |  384 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  385 | `									pStream->zText++;` |
|  ! 0 |  386 | `								}` |
|  ! 0 |  387 | `							}` |
|  ! 0 |  388 | `						}` |
|  ! 0 |  389 | `					}` |
|   15 |  390 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  391 | `					/* Real number */` |
|  ! 0 |  392 | `					pStream->zText++;` |
|  ! 0 |  393 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  394 | `						c = pStream->zText[0];` |
|  ! 0 |  395 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  396 | `							pStream->zText++;` |
|  ! 0 |  397 | `						}` |
|  ! 0 |  398 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  399 | `							pStream->zText++;` |
|  ! 0 |  400 | `						}` |
|  ! 0 |  401 | `					}` |
|  ! 0 |  402 | `				}` |
|    8 |  403 | `			}` |
|   18 |  404 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|    6 |  405 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  406 | `			/* boolean true */` |
|  ! 0 |  407 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  408 | `			/* Advance the stream cursor */` |
|  ! 0 |  409 | `			pStream->zText += sizeof("true")-1;` |
|   11 |  410 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|    6 |  411 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  412 | `			/* boolean false */` |
|  ! 0 |  413 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  414 | `			/* Advance the stream cursor */` |
|  ! 0 |  415 | `			pStream->zText += sizeof("false")-1;` |
|   11 |  416 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|    6 |  417 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  418 | `			/* NULL */` |
|  ! 0 |  419 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  420 | `			/* Advance the stream cursor */` |
|  ! 0 |  421 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  422 | `	}else{` |
|    - |  423 | `		/* Unexpected token */` |
|    8 |  424 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  425 | `		/* Advance the stream cursor */` |
|    8 |  426 | `		pStream->zText++;` |
|    8 |  427 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  428 | `		/* Abort processing immediatley */` |
|    8 |  429 | `		return SXERR_ABORT;` |
|    - |  430 | `	}` |
|    - |  431 | `	/* record token length */` |
|   82 |  432 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   82 |  433 | `	if( pToken->nType == JSON_TK_STR ){` |
|   18 |  434 | `		pStr->nByte--;` |
|    8 |  435 | `	}` |
|    - |  436 | `	/* Return to the lexer */` |
|   82 |  437 | `	return SXRET_OK;` |
|   45 |  438 |  |
|    - |  439 | `/*` |
|    - |  440 | ` * JSON decoded input consumer callback signature.` |
|    - |  441 | ` */` |
|    - |  442 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  443 | `/*` |
|    - |  444 | ` * JSON decoder state is kept in the following structure.` |
|    - |  445 | ` */` |
|    - |  446 | `typedef struct json_decoder json_decoder;` |
|    - |  447 | `struct json_decoder` |
|    - |  448 |  |
|    - |  449 | `	ph7_context *pCtx; /* Call context */` |
|    - |  450 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  451 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  452 | `	int iFlags;        /* Configuration flags */` |
|    - |  453 | `	SyToken *pIn;      /* Token stream */` |
|    - |  454 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  455 | `	int rec_depth;     /* Recursion limit */` |
|    - |  456 | `	int rec_count;     /* Current nesting level */` |
|    - |  457 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  458 | `};` |
|    - |  459 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  460 | `/* Forward declaration */` |
|    - |  461 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  462 | `/*` |
|    - |  463 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  464 | ` * the result in the given ph7_value.` |
|    - |  465 | ` */` |
|   16 |  466 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    2 |  467 |  |
|   18 |  468 | `	const char *zIn = pStr->zString;` |
|   18 |  469 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  470 | `	const char *zCur;` |
|    - |  471 | `	int c;` |
|    - |  472 | `	/* Mark the value as a string */` |
|   18 |  473 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|    8 |  474 | `	for(;;){` |
|   18 |  475 | `		zCur = zIn;` |
|   56 |  476 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   40 |  477 | `			zIn++;` |
|    2 |  478 | `		}` |
|   18 |  479 | `		if( zIn > zCur ){` |
|    - |  480 | `			/* Append chunk verbatim */` |
|   18 |  481 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|    8 |  482 | `		}` |
|   18 |  483 | `		zIn++;` |
|   18 |  484 | `		if( zIn >= zEnd ){` |
|    - |  485 | `			/* End of the input reached */` |
|   18 |  486 | `			break;` |
|    - |  487 | `		}` |
|  ! 0 |  488 | `		c = zIn[0];` |
|    - |  489 | `		/* Unescape the character */` |
|  ! 0 |  490 | `		switch(c){` |
|  ! 0 |  491 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  492 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  493 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  494 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  495 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  496 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  497 | `		default:` |
|  ! 0 |  498 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  499 | `			break;` |
|    - |  500 | `		}` |
|    - |  501 | `		/* Advance the stream cursor */` |
|  ! 0 |  502 | `		zIn++;` |
|  ! 0 |  503 | `	}` |
|   18 |  504 |  |
|    - |  505 | `/*` |
|    - |  506 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  507 | ` * According to wikipedia` |
|    - |  508 | ` * JSON's basic types are:` |
|    - |  509 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  510 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  511 | ` *   Boolean (true or false)` |
|    - |  512 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  513 | ` *    do not need to be of the same type)` |
|    - |  514 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  515 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  516 | ` *     be distinct from each other)` |
|    - |  517 | ` *   null (empty)` |
|    - |  518 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  519 | ` */` |
|   34 |  520 | `static sxi32 VmJsonDecode(` |
|    - |  521 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  522 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    2 |  523 | `	){` |
|    - |  524 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  525 | `	sxi32 rc;` |
|    - |  526 | `	/* Check if we do not nest to much */` |
|   36 |  527 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  528 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  529 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  530 | `		return SXERR_ABORT;` |
|    - |  531 | `	}` |
|   36 |  532 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  533 | `		/* Scalar value */` |
|   22 |  534 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   22 |  535 | `		if( pWorker == 0 ){` |
|  ! 0 |  536 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  537 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  538 | `			return SXERR_ABORT;` |
|    - |  539 | `		}` |
|    - |  540 | `		/* Reflect the JSON image */` |
|   22 |  541 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  542 | `			/* Nullify the value.*/` |
|  ! 0 |  543 | `			ph7_value_null(pWorker);` |
|   22 |  544 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  545 | `			/* Boolean value */` |
|  ! 0 |  546 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   22 |  547 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   15 |  548 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  549 | `			/*` |
|    - |  550 | `			 * Numeric value.` |
|    - |  551 | `			 * Get a string representation first then try to get a numeric` |
|    - |  552 | `			 * value.` |
|    - |  553 | `			 */` |
|   15 |  554 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  555 | `			/* Obtain a numeric representation */` |
|   15 |  556 | `			PH7_MemObjToNumeric(pWorker);` |
|    8 |  557 | `		}else{` |
|    - |  558 | `			/* Dequote the string */` |
|    8 |  559 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  560 | `		}` |
|    - |  561 | `		/* Invoke the consumer callback */` |
|   22 |  562 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   22 |  563 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  564 | `			return SXERR_ABORT;` |
|    - |  565 | `		}` |
|    - |  566 | `		/* All done,advance the stream cursor */` |
|   22 |  567 | `		pDecoder->pIn++;` |
|   26 |  568 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  569 | `		ProcJsonConsumer xOld;` |
|    - |  570 | `		void *pOld;` |
|    - |  571 | `		/* Array representation*/` |
|    7 |  572 | `		pDecoder->pIn++;` |
|    - |  573 | `		/* Create a working array */` |
|    7 |  574 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|    7 |  575 | `		if( pWorker == 0 ){` |
|  ! 0 |  576 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  577 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  578 | `			return SXERR_ABORT;` |
|    - |  579 | `		}` |
|    - |  580 | `		/* Save the old consumer */` |
|    7 |  581 | `		xOld = pDecoder->xConsumer;` |
|    7 |  582 | `		pOld = pDecoder->pUserData;` |
|    - |  583 | `		/* Set the new consumer */` |
|    7 |  584 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|    7 |  585 | `		pDecoder->pUserData = pWorker;` |
|    - |  586 | `		/* Decode the array */` |
|   10 |  587 | `		for(;;){` |
|    - |  588 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  589 | `			 * do this.` |
|    - |  590 | `			 */` |
|   29 |  591 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    9 |  592 | `				pDecoder->pIn++;` |
|    1 |  593 | `			}` |
|   21 |  594 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|    7 |  595 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|    7 |  596 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    3 |  597 | `				}` |
|    7 |  598 | `				break;` |
|    - |  599 | `			}` |
|    - |  600 | `			/* Recurse and decode the entry */` |
|   15 |  601 | `			pDecoder->rec_count++;` |
|   15 |  602 | `			rc = VmJsonDecode(pDecoder,0);` |
|   15 |  603 | `			pDecoder->rec_count--;` |
|   15 |  604 | `			if( rc == SXERR_ABORT ){` |
|    - |  605 | `				/* Abort processing immediately */` |
|  ! 0 |  606 | `				return SXERR_ABORT;` |
|    - |  607 | `			}` |
|    - |  608 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   15 |  609 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   14 |  610 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  611 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  612 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  613 | `					return SXERR_ABORT;` |
|    - |  614 | `			}` |
|    1 |  615 | `		}` |
|    - |  616 | `		/* Restore the old consumer */` |
|    7 |  617 | `		pDecoder->xConsumer = xOld;` |
|    7 |  618 | `		pDecoder->pUserData = pOld;` |
|    - |  619 | `		/* Invoke the old consumer on the decoded array */` |
|    7 |  620 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   13 |  621 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  622 | `		ProcJsonConsumer xOld;` |
|    - |  623 | `		ph7_value *pKey;` |
|    - |  624 | `		void *pOld;` |
|    - |  625 | `		/* Object representation*/` |
|   10 |  626 | `		pDecoder->pIn++;` |
|    - |  627 | `		/* Return the object as an associative array */` |
|   10 |  628 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|    3 |  629 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|    - |  630 | `				"JSON Objects are always returned as an associative array"` |
|    - |  631 | `				);` |
|    1 |  632 | `		}` |
|    - |  633 | `		/* Create a working array */` |
|   10 |  634 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   10 |  635 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   10 |  636 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  637 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  638 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  639 | `			return SXERR_ABORT;` |
|    - |  640 | `		}` |
|    - |  641 | `		/* Save the old consumer */` |
|   10 |  642 | `		xOld = pDecoder->xConsumer;` |
|   10 |  643 | `		pOld = pDecoder->pUserData;` |
|    - |  644 | `		/* Set the new consumer */` |
|   10 |  645 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   10 |  646 | `		pDecoder->pUserData = pWorker;` |
|    - |  647 | `		/* Decode the object */` |
|    9 |  648 | `		for(;;){` |
|    - |  649 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  650 | `			 * do this.` |
|    - |  651 | `			 */` |
|   24 |  652 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    5 |  653 | `				pDecoder->pIn++;` |
|    1 |  654 | `			}` |
|   20 |  655 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|   10 |  656 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|    8 |  657 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    3 |  658 | `				}` |
|   10 |  659 | `				break;` |
|    - |  660 | `			}` |
|   10 |  661 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   12 |  662 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  663 | `					/* Syntax error,return immediately */` |
|  ! 0 |  664 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  665 | `					return SXERR_ABORT;` |
|    - |  666 | `			}` |
|    - |  667 | `			/* Dequote the key */` |
|   12 |  668 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  669 | `			/* Jump the key and the colon */` |
|   12 |  670 | `			pDecoder->pIn += 2;` |
|    - |  671 | `			/* Recurse and decode the value */` |
|   12 |  672 | `			pDecoder->rec_count++;` |
|   12 |  673 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   12 |  674 | `			pDecoder->rec_count--;` |
|   12 |  675 | `			if( rc == SXERR_ABORT ){` |
|    - |  676 | `				/* Abort processing immediately */` |
|  ! 0 |  677 | `				return SXERR_ABORT;` |
|    - |  678 | `			}` |
|    - |  679 | `			/* Reset the internal buffer of the key */` |
|   12 |  680 | `			ph7_value_reset_string_cursor(pKey);` |
|    - |  681 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    2 |  682 | `		}` |
|    - |  683 | `		/* Restore the old consumer */` |
|   10 |  684 | `		pDecoder->xConsumer = xOld;` |
|   10 |  685 | `		pDecoder->pUserData = pOld;` |
|    - |  686 | `		/* Invoke the old consumer on the decoded object*/` |
|   10 |  687 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - |  688 | `		/* Release the key */` |
|   10 |  689 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|    6 |  690 | `	}else{` |
|    - |  691 | `		/* Unexpected token */` |
|  ! 0 |  692 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - |  693 | `	}` |
|    - |  694 | `	/* Release the worker variable */` |
|   36 |  695 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   36 |  696 | `	return SXRET_OK;` |
|   19 |  697 |  |
|    - |  698 | `/*` |
|    - |  699 | ` * The following JSON decoder callback is invoked each time` |
|    - |  700 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - |  701 | ` * is being decoded.` |
|    - |  702 | ` */` |
|   24 |  703 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  704 |  |
|   26 |  705 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - |  706 | `	/* Insert the entry */` |
|   26 |  707 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   12 |  708 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - |  709 | `	/* All done */` |
|   26 |  710 | `	return SXRET_OK;` |
|    2 |  711 |  |
|    - |  712 | `/*` |
|    - |  713 | ` * Standard JSON decoder callback.` |
|    - |  714 | ` */` |
|   10 |  715 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  716 |  |
|    - |  717 | `	/* Return the value directly */` |
|   12 |  718 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|    5 |  719 | `	SXUNUSED(pKey); /* cc warning */` |
|    5 |  720 | `	SXUNUSED(pUserData);` |
|    - |  721 | `	/* All done */` |
|   12 |  722 | `	return SXRET_OK;` |
|    2 |  723 |  |
|    - |  724 | `/*` |
|    - |  725 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - |  726 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - |  727 | ` * Parameters` |
|    - |  728 | ` *  $json` |
|    - |  729 | ` *    The json string being decoded.` |
|    - |  730 | ` * $assoc` |
|    - |  731 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - |  732 | ` * $depth` |
|    - |  733 | ` *   User specified recursion depth.` |
|    - |  734 | ` * $options` |
|    - |  735 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - |  736 | ` * (default is to cast large integers as floats)` |
|    - |  737 | ` * Return` |
|    - |  738 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - |  739 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - |  740 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - |  741 | ` */` |
|   18 |  742 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  743 |  |
|   20 |  744 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  745 | `	json_decoder sDecoder;` |
|    - |  746 | `	const char *zIn;` |
|    - |  747 | `	SySet sToken;` |
|    - |  748 | `	SyLex sLex;` |
|    - |  749 | `	int nByte;` |
|    - |  750 | `	sxi32 rc;` |
|   20 |  751 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  752 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 |  753 | `		ph7_result_null(pCtx);` |
|  ! 0 |  754 | `		return PH7_OK;` |
|    - |  755 | `	}` |
|    - |  756 | `	/* Extract the JSON string */` |
|   20 |  757 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   20 |  758 | `	if( nByte < 1 ){` |
|    - |  759 | `		/* Empty string,return NULL */` |
|    3 |  760 | `		ph7_result_null(pCtx);` |
|    3 |  761 | `		return PH7_OK;` |
|    - |  762 | `	}` |
|    - |  763 | `	/* Clear JSON error code */` |
|   18 |  764 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  765 | `	/* Tokenize the input */` |
|   18 |  766 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   18 |  767 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   18 |  768 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   18 |  769 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - |  770 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|    8 |  771 | `		SyLexRelease(&sLex);` |
|    8 |  772 | `		SySetRelease(&sToken);` |
|    - |  773 | `		/* return NULL */` |
|    8 |  774 | `		ph7_result_null(pCtx);` |
|    8 |  775 | `		return PH7_OK;` |
|    - |  776 | `	}` |
|    - |  777 | `	/* Fill the decoder */` |
|   12 |  778 | `	sDecoder.pCtx = pCtx;` |
|   12 |  779 | `	sDecoder.pErr = &pVm->json_rc;` |
|   12 |  780 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   12 |  781 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   12 |  782 | `	sDecoder.iFlags = 0;` |
|   12 |  783 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|    - |  784 | `		/* Returned objects will be converted into associative arrays */` |
|   10 |  785 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|    4 |  786 | `	}` |
|   12 |  787 | `	sDecoder.rec_depth = 32;` |
|   12 |  788 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|  ! 0 |  789 | `		int nDepth = ph7_value_to_int(apArg[2]);` |
|  ! 0 |  790 | `		if( nDepth > 1 && nDepth < 32 ){` |
|  ! 0 |  791 | `			sDecoder.rec_depth = nDepth;` |
|  ! 0 |  792 | `		}` |
|  ! 0 |  793 | `	}` |
|   12 |  794 | `	sDecoder.rec_count = 0;` |
|    - |  795 | `	/* Set a default consumer */` |
|   12 |  796 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   12 |  797 | `	sDecoder.pUserData = 0;` |
|    - |  798 | `	/* Decode the raw JSON input */` |
|   12 |  799 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   12 |  800 | `	if( rc == SXERR_ABORT \|\|  pVm->json_rc != JSON_ERROR_NONE ){` |
|    - |  801 | `		/*` |
|    - |  802 | `		 * Something goes wrong while decoding JSON input.Return NULL.` |
|    - |  803 | `		 */` |
|  ! 0 |  804 | `		ph7_result_null(pCtx);` |
|  ! 0 |  805 | `	}` |
|    - |  806 | `	/* Clean-up the mess left behind */` |
|   12 |  807 | `	SyLexRelease(&sLex);` |
|   12 |  808 | `	SySetRelease(&sToken);` |
|    - |  809 | `	/* All done */` |
|   12 |  810 | `	return PH7_OK;` |
|   11 |  811 |  |
|    - |  812 |  |
