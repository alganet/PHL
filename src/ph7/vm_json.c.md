# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 264/425 lines (62.12%)

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
|    - |   25 | `	int iFlags;        /* JSON encoding flags */` |
|    - |   26 | `	int nRecCount;     /* Recursion count */` |
|    - |   27 | `};` |
|    - |   28 | `/*` |
|    - |   29 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|    - |   30 | ` * According to wikipedia` |
|    - |   31 | ` * JSON's basic types are:` |
|    - |   32 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |   33 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |   34 | ` *   Boolean (true or false)` |
|    - |   35 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |   36 | ` *    do not need to be of the same type)` |
|    - |   37 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |   38 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |   39 | ` *     be distinct from each other)` |
|    - |   40 | ` *   null (empty)` |
|    - |   41 | ` * Non-significant white space may be added freely around the "structural characters"` |
|    - |   42 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |   43 | ` */` |
|    8 |   44 | `static sxi32 VmJsonEncode(` |
|    - |   45 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |   46 | `	json_private_data *pData /* Context data */` |
|    1 |   47 | `	){` |
|    9 |   48 | `		ph7_context *pCtx = pData->pCtx;` |
|    9 |   49 | `		int iFlags = pData->iFlags;` |
|    - |   50 | `		int nByte;` |
|    9 |   51 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |   52 | `			/* null */` |
|  ! 0 |   53 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|    9 |   54 | `		}else if( ph7_value_is_bool(pIn) ){` |
|  ! 0 |   55 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |   56 | `			int iLen;` |
|    - |   57 | `			/* true/false */` |
|  ! 0 |   58 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|  ! 0 |   59 | `			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);` |
|   12 |   60 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|    - |   61 | `			const char *zNum;` |
|    - |   62 | `			/* Get a string representation of the number */` |
|    7 |   63 | `			zNum = ph7_value_to_string(pIn,&nByte);` |
|    7 |   64 | `			ph7_result_string(pCtx,zNum,nByte);` |
|    6 |   65 | `		}else if( ph7_value_is_string(pIn) ){` |
|  ! 0 |   66 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |   67 | `				const char *zNum;` |
|    - |   68 | `				/* Encodes numeric strings as numbers. */` |
|  ! 0 |   69 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    - |   70 | `				/* Get a string representation of the number */` |
|  ! 0 |   71 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|  ! 0 |   72 | `				ph7_result_string(pCtx,zNum,nByte);` |
|  ! 0 |   73 | `			}else{` |
|    - |   74 | `				const char *zIn,*zEnd;` |
|    - |   75 | `				int c;` |
|    - |   76 | `				/* Encode the string */` |
|  ! 0 |   77 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|  ! 0 |   78 | `				zEnd = &zIn[nByte];` |
|    - |   79 | `				/* Append the double quote */` |
|  ! 0 |   80 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|  ! 0 |   81 | `				for(;;){` |
|  ! 0 |   82 | `					if( zIn >= zEnd ){` |
|    - |   83 | `						/* No more input to process */` |
|  ! 0 |   84 | `						break;` |
|    - |   85 | `					}` |
|  ! 0 |   86 | `					c = zIn[0];` |
|    - |   87 | `					/* Advance the stream cursor */` |
|  ! 0 |   88 | `					zIn++;` |
|  ! 0 |   89 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |   90 | `						/* All < and > are converted to \u003C and \u003E */` |
|  ! 0 |   91 | `						if( c == '<' ){` |
|  ! 0 |   92 | `							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|  ! 0 |   93 | `						}else{` |
|  ! 0 |   94 | `							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|    - |   95 | `						}` |
|  ! 0 |   96 | `						continue;` |
|  ! 0 |   97 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |   98 | `						/* All &s are converted to \u0026.  */` |
|  ! 0 |   99 | `						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|  ! 0 |  100 | `						continue;` |
|  ! 0 |  101 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  102 | `						/* All ' are converted to \u0027.   */` |
|  ! 0 |  103 | `						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|  ! 0 |  104 | `						continue;` |
|  ! 0 |  105 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  106 | `						/* All " are converted to \u0022. */` |
|  ! 0 |  107 | `						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|  ! 0 |  108 | `						continue;` |
|    - |  109 | `					}` |
|  ! 0 |  110 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|    - |  111 | `						/* Unescape the character */` |
|  ! 0 |  112 | `						ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|  ! 0 |  113 | `					}` |
|    - |  114 | `					/* Append character verbatim */` |
|  ! 0 |  115 | `					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  116 | `				}` |
|    - |  117 | `				/* Append the double quote */` |
|  ! 0 |  118 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|  ! 0 |  119 | `			}` |
|    3 |  120 | `		}else if( ph7_value_is_array(pIn) ){` |
|    3 |  121 | `			int c = '[',d = ']';` |
|    - |  122 | `			/* Encode the array */` |
|    3 |  123 | `			pData->isFirst = 1;` |
|    3 |  124 | `			if( iFlags & JSON_FORCE_OBJECT ){` |
|    - |  125 | `				/* Outputs an object rather than an array */` |
|  ! 0 |  126 | `				c = '{';` |
|  ! 0 |  127 | `				d = '}';` |
|  ! 0 |  128 | `			}` |
|    - |  129 | `			/* Append the square bracket or curly braces */` |
|    3 |  130 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    - |  131 | `			/* Iterate throw array entries */` |
|    3 |  132 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  133 | `			/* Append the closing square bracket or curly braces */` |
|    3 |  134 | `			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));` |
|    1 |  135 | `		}else if( ph7_value_is_object(pIn) ){` |
|    - |  136 | `			/* Encode the class instance */` |
|  ! 0 |  137 | `			pData->isFirst = 1;` |
|    - |  138 | `			/* Append the curly braces */` |
|  ! 0 |  139 | `			ph7_result_string(pCtx,"{",(int)sizeof(char));` |
|    - |  140 | `			/* Iterate throw class attribute */` |
|  ! 0 |  141 | `			ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|    - |  142 | `			/* Append the closing curly braces  */` |
|  ! 0 |  143 | `			ph7_result_string(pCtx,"}",(int)sizeof(char));` |
|  ! 0 |  144 | `		}else{` |
|    - |  145 | `			/* Can't happen */` |
|  ! 0 |  146 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|    - |  147 | `		}` |
|    - |  148 | `		/* All done */` |
|    9 |  149 | `		return PH7_OK;` |
|    1 |  150 |  |
|    - |  151 | `/*` |
|    - |  152 | ` * The following walker callback is invoked each time we need` |
|    - |  153 | ` * to encode an array to JSON.` |
|    - |  154 | ` */` |
|    6 |  155 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    1 |  156 |  |
|    7 |  157 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|    7 |  158 | `	if( pJson->nRecCount > 31 ){` |
|    - |  159 | `		/* Recursion limit reached,return immediately */` |
|  ! 0 |  160 | `		return PH7_OK;` |
|    - |  161 | `	}` |
|    7 |  162 | `	if( !pJson->isFirst ){` |
|    - |  163 | `		/* Append the colon first */` |
|    5 |  164 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|    2 |  165 | `	}` |
|    7 |  166 | `	if( pJson->iFlags & JSON_FORCE_OBJECT ){` |
|    - |  167 | `		/* Outputs an object rather than an array */` |
|    - |  168 | `		const char *zKey;` |
|    - |  169 | `		int nByte;` |
|    - |  170 | `		/* Extract a string representation of the key */` |
|  ! 0 |  171 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  172 | `		/* Append the key and the double colon */` |
|  ! 0 |  173 | `		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);` |
|  ! 0 |  174 | `	}` |
|    - |  175 | `	/* Encode the value */` |
|    7 |  176 | `	pJson->nRecCount++;` |
|    7 |  177 | `	VmJsonEncode(pValue,pJson);` |
|    7 |  178 | `	pJson->nRecCount--;` |
|    7 |  179 | `	pJson->isFirst = 0;` |
|    7 |  180 | `	return PH7_OK;` |
|    4 |  181 |  |
|    - |  182 | `/*` |
|    - |  183 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  184 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  185 | ` */` |
|  ! 0 |  186 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|  ! 0 |  187 |  |
|  ! 0 |  188 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  ! 0 |  189 | `	if( pJson->nRecCount > 31 ){` |
|    - |  190 | `		/* Recursion limit reached,return immediately */` |
|  ! 0 |  191 | `		return PH7_OK;` |
|    - |  192 | `	}` |
|  ! 0 |  193 | `	if( !pJson->isFirst ){` |
|    - |  194 | `		/* Append the colon first */` |
|  ! 0 |  195 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|  ! 0 |  196 | `	}` |
|    - |  197 | `	/* Append the attribute name and the double colon first */` |
|  ! 0 |  198 | `	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);` |
|    - |  199 | `	/* Encode the value */` |
|  ! 0 |  200 | `	pJson->nRecCount++;` |
|  ! 0 |  201 | `	VmJsonEncode(pValue,pJson);` |
|  ! 0 |  202 | `	pJson->nRecCount--;` |
|  ! 0 |  203 | `	pJson->isFirst = 0;` |
|  ! 0 |  204 | `	return PH7_OK;` |
|  ! 0 |  205 |  |
|    - |  206 | `/*` |
|    - |  207 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  208 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  209 | ` * Parameters` |
|    - |  210 | ` *  $value` |
|    - |  211 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  212 | ` * $options` |
|    - |  213 | ` *  Bitmask consisting of:` |
|    - |  214 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  215 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  216 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  217 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  218 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  219 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  220 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  221 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  222 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  223 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  224 | ` * Return` |
|    - |  225 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  226 | ` */` |
|    2 |  227 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  228 |  |
|    - |  229 | `	json_private_data sJson;` |
|    3 |  230 | `	if( nArg < 1 ){` |
|    - |  231 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  232 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  233 | `		return PH7_OK;` |
|    - |  234 | `	}` |
|    - |  235 | `	/* Prepare the JSON data */` |
|    3 |  236 | `	sJson.nRecCount = 0;` |
|    3 |  237 | `	sJson.pCtx = pCtx;` |
|    3 |  238 | `	sJson.isFirst = 1;` |
|    3 |  239 | `	sJson.iFlags = 0;` |
|    3 |  240 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  241 | `		/* Extract option flags */` |
|  ! 0 |  242 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|  ! 0 |  243 | `	}` |
|    - |  244 | `	/* Perform the encoding operation */` |
|    3 |  245 | `	VmJsonEncode(apArg[0],&sJson);` |
|    - |  246 | `	/* All done */` |
|    3 |  247 | `	return PH7_OK;` |
|    2 |  248 |  |
|    - |  249 | `/*` |
|    - |  250 | ` * int json_last_error(void)` |
|    - |  251 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  252 | ` * Parameters` |
|    - |  253 | ` *  None` |
|    - |  254 | ` * Return` |
|    - |  255 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  256 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  257 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  258 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  259 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  260 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  261 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  262 | ` */` |
|    8 |  263 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  264 |  |
|   10 |  265 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  266 | `	/* Return the error code */` |
|   10 |  267 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    4 |  268 | `	SXUNUSED(nArg); /* cc warning */` |
|    4 |  269 | `	SXUNUSED(apArg);` |
|   10 |  270 | `	return PH7_OK;` |
|    2 |  271 |  |
|    - |  272 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  273 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  274 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  275 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  276 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  277 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  278 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  279 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  280 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  281 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  282 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  283 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  284 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  285 | `/*` |
|    - |  286 | ` * Tokenize an entire JSON input.` |
|    - |  287 | ` * Get a single low-level token from the input file.` |
|    - |  288 | ` * Update the stream pointer so that it points to the first` |
|    - |  289 | ` * character beyond the extracted token.` |
|    - |  290 | ` */` |
|   60 |  291 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    2 |  292 |  |
|   62 |  293 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  294 | `	SyString *pStr;` |
|    - |  295 | `	int c;` |
|    - |  296 | `	/* Ignore leading white spaces */` |
|   66 |  297 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  298 | `		/* Advance the stream cursor */` |
|    6 |  299 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  300 | `			/* Update line counter */` |
|  ! 0 |  301 | `			pStream->nLine++;` |
|  ! 0 |  302 | `		}` |
|    6 |  303 | `		pStream->zText++;` |
|    2 |  304 | `	}` |
|   62 |  305 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  306 | `		/* End of input reached */` |
|  ! 0 |  307 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  308 | `		return SXERR_EOF;` |
|    - |  309 | `	}` |
|    - |  310 | `	/* Record token starting position and line */` |
|   62 |  311 | `	pToken->nLine = pStream->nLine;` |
|   62 |  312 | `	pToken->pUserData = 0;` |
|   62 |  313 | `	pStr = &pToken->sData;` |
|   62 |  314 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|   77 |  315 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|   44 |  316 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  317 | `			/* Single character */` |
|   36 |  318 | `			c = pStream->zText[0];` |
|    - |  319 | `			/* Set token type */` |
|   36 |  320 | `			switch(c){` |
|    5 |  321 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   10 |  322 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|    6 |  323 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|    5 |  324 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|    8 |  325 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|    9 |  326 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  327 | `			default:` |
|  ! 0 |  328 | `				break;` |
|    - |  329 | `			}` |
|    - |  330 | `			/* Advance the stream cursor */` |
|   36 |  331 | `			pStream->zText++;` |
|   45 |  332 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  333 | `		/* JSON string */` |
|   10 |  334 | `		pStream->zText++;` |
|   10 |  335 | `		pStr->zString++;` |
|    - |  336 | `		/* Delimit the string */` |
|   32 |  337 | `		while( pStream->zText < pStream->zEnd ){` |
|   32 |  338 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   10 |  339 | `				break;` |
|    - |  340 | `			}` |
|   24 |  341 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  342 | `				/* Update line counter */` |
|  ! 0 |  343 | `				pStream->nLine++;` |
|  ! 0 |  344 | `			}` |
|   24 |  345 | `			pStream->zText++;` |
|    2 |  346 | `		}` |
|   10 |  347 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  348 | `			/* Missing closing '"' */` |
|  ! 0 |  349 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  350 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  351 | `		}else{` |
|   10 |  352 | `			pToken->nType = JSON_TK_STR;` |
|   10 |  353 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    2 |  354 | `		}` |
|   24 |  355 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    - |  356 | `		/* Number */` |
|   13 |  357 | `		pStream->zText++;` |
|   13 |  358 | `		pToken->nType = JSON_TK_NUM;` |
|   13 |  359 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  360 | `			pStream->zText++;` |
|  ! 0 |  361 | `		}` |
|   13 |  362 | `		if( pStream->zText < pStream->zEnd ){` |
|   13 |  363 | `			c = pStream->zText[0];` |
|   13 |  364 | `			if( c == '.' ){` |
|    - |  365 | `					/* Real number */` |
|  ! 0 |  366 | `					pStream->zText++;` |
|  ! 0 |  367 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  368 | `						pStream->zText++;` |
|  ! 0 |  369 | `					}` |
|  ! 0 |  370 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  371 | `						c = pStream->zText[0];` |
|  ! 0 |  372 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  373 | `							pStream->zText++;` |
|  ! 0 |  374 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  375 | `								c = pStream->zText[0];` |
|  ! 0 |  376 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  377 | `									pStream->zText++;` |
|  ! 0 |  378 | `								}` |
|  ! 0 |  379 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  380 | `									pStream->zText++;` |
|  ! 0 |  381 | `								}` |
|  ! 0 |  382 | `							}` |
|  ! 0 |  383 | `						}` |
|  ! 0 |  384 | `					}` |
|   13 |  385 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  386 | `					/* Real number */` |
|  ! 0 |  387 | `					pStream->zText++;` |
|  ! 0 |  388 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  389 | `						c = pStream->zText[0];` |
|  ! 0 |  390 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  391 | `							pStream->zText++;` |
|  ! 0 |  392 | `						}` |
|  ! 0 |  393 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  394 | `							pStream->zText++;` |
|  ! 0 |  395 | `						}` |
|  ! 0 |  396 | `					}` |
|  ! 0 |  397 | `				}` |
|    7 |  398 | `			}` |
|   17 |  399 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|    6 |  400 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  401 | `			/* boolean true */` |
|  ! 0 |  402 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  403 | `			/* Advance the stream cursor */` |
|  ! 0 |  404 | `			pStream->zText += sizeof("true")-1;` |
|   11 |  405 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|    6 |  406 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  407 | `			/* boolean false */` |
|  ! 0 |  408 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  409 | `			/* Advance the stream cursor */` |
|  ! 0 |  410 | `			pStream->zText += sizeof("false")-1;` |
|   11 |  411 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|    6 |  412 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  413 | `			/* NULL */` |
|  ! 0 |  414 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  415 | `			/* Advance the stream cursor */` |
|  ! 0 |  416 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  417 | `	}else{` |
|    - |  418 | `		/* Unexpected token */` |
|    8 |  419 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  420 | `		/* Advance the stream cursor */` |
|    8 |  421 | `		pStream->zText++;` |
|    8 |  422 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  423 | `		/* Abort processing immediatley */` |
|    8 |  424 | `		return SXERR_ABORT;` |
|    - |  425 | `	}` |
|    - |  426 | `	/* record token length */` |
|   56 |  427 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   56 |  428 | `	if( pToken->nType == JSON_TK_STR ){` |
|   10 |  429 | `		pStr->nByte--;` |
|    4 |  430 | `	}` |
|    - |  431 | `	/* Return to the lexer */` |
|   56 |  432 | `	return SXRET_OK;` |
|   32 |  433 |  |
|    - |  434 | `/*` |
|    - |  435 | ` * JSON decoded input consumer callback signature.` |
|    - |  436 | ` */` |
|    - |  437 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  438 | `/*` |
|    - |  439 | ` * JSON decoder state is kept in the following structure.` |
|    - |  440 | ` */` |
|    - |  441 | `typedef struct json_decoder json_decoder;` |
|    - |  442 | `struct json_decoder` |
|    - |  443 |  |
|    - |  444 | `	ph7_context *pCtx; /* Call context */` |
|    - |  445 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  446 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  447 | `	int iFlags;        /* Configuration flags */` |
|    - |  448 | `	SyToken *pIn;      /* Token stream */` |
|    - |  449 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  450 | `	int rec_depth;     /* Recursion limit */` |
|    - |  451 | `	int rec_count;     /* Current nesting level */` |
|    - |  452 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  453 | `};` |
|    - |  454 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  455 | `/* Forward declaration */` |
|    - |  456 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  457 | `/*` |
|    - |  458 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  459 | ` * the result in the given ph7_value.` |
|    - |  460 | ` */` |
|    8 |  461 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    2 |  462 |  |
|   10 |  463 | `	const char *zIn = pStr->zString;` |
|   10 |  464 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  465 | `	const char *zCur;` |
|    - |  466 | `	int c;` |
|    - |  467 | `	/* Mark the value as a string */` |
|   10 |  468 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|    4 |  469 | `	for(;;){` |
|   10 |  470 | `		zCur = zIn;` |
|   32 |  471 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   24 |  472 | `			zIn++;` |
|    2 |  473 | `		}` |
|   10 |  474 | `		if( zIn > zCur ){` |
|    - |  475 | `			/* Append chunk verbatim */` |
|   10 |  476 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|    4 |  477 | `		}` |
|   10 |  478 | `		zIn++;` |
|   10 |  479 | `		if( zIn >= zEnd ){` |
|    - |  480 | `			/* End of the input reached */` |
|   10 |  481 | `			break;` |
|    - |  482 | `		}` |
|  ! 0 |  483 | `		c = zIn[0];` |
|    - |  484 | `		/* Unescape the character */` |
|  ! 0 |  485 | `		switch(c){` |
|  ! 0 |  486 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  487 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  488 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  489 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  490 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  491 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  492 | `		default:` |
|  ! 0 |  493 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  494 | `			break;` |
|    - |  495 | `		}` |
|    - |  496 | `		/* Advance the stream cursor */` |
|  ! 0 |  497 | `		zIn++;` |
|  ! 0 |  498 | `	}` |
|   10 |  499 |  |
|    - |  500 | `/*` |
|    - |  501 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  502 | ` * According to wikipedia` |
|    - |  503 | ` * JSON's basic types are:` |
|    - |  504 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  505 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  506 | ` *   Boolean (true or false)` |
|    - |  507 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  508 | ` *    do not need to be of the same type)` |
|    - |  509 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  510 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  511 | ` *     be distinct from each other)` |
|    - |  512 | ` *   null (empty)` |
|    - |  513 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  514 | ` */` |
|   24 |  515 | `static sxi32 VmJsonDecode(` |
|    - |  516 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  517 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    2 |  518 | `	){` |
|    - |  519 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  520 | `	sxi32 rc;` |
|    - |  521 | `	/* Check if we do not nest to much */` |
|   26 |  522 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  523 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  524 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  525 | `		return SXERR_ABORT;` |
|    - |  526 | `	}` |
|   26 |  527 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  528 | `		/* Scalar value */` |
|   16 |  529 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   16 |  530 | `		if( pWorker == 0 ){` |
|  ! 0 |  531 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  532 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  533 | `			return SXERR_ABORT;` |
|    - |  534 | `		}` |
|    - |  535 | `		/* Reflect the JSON image */` |
|   16 |  536 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  537 | `			/* Nullify the value.*/` |
|  ! 0 |  538 | `			ph7_value_null(pWorker);` |
|   16 |  539 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  540 | `			/* Boolean value */` |
|  ! 0 |  541 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   16 |  542 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   13 |  543 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  544 | `			/*` |
|    - |  545 | `			 * Numeric value.` |
|    - |  546 | `			 * Get a string representation first then try to get a numeric` |
|    - |  547 | `			 * value.` |
|    - |  548 | `			 */` |
|   13 |  549 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  550 | `			/* Obtain a numeric representation */` |
|   13 |  551 | `			PH7_MemObjToNumeric(pWorker);` |
|    7 |  552 | `		}else{` |
|    - |  553 | `			/* Dequote the string */` |
|    3 |  554 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  555 | `		}` |
|    - |  556 | `		/* Invoke the consumer callback */` |
|   16 |  557 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   16 |  558 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  559 | `			return SXERR_ABORT;` |
|    - |  560 | `		}` |
|    - |  561 | `		/* All done,advance the stream cursor */` |
|   16 |  562 | `		pDecoder->pIn++;` |
|   19 |  563 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  564 | `		ProcJsonConsumer xOld;` |
|    - |  565 | `		void *pOld;` |
|    - |  566 | `		/* Array representation*/` |
|    5 |  567 | `		pDecoder->pIn++;` |
|    - |  568 | `		/* Create a working array */` |
|    5 |  569 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|    5 |  570 | `		if( pWorker == 0 ){` |
|  ! 0 |  571 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  572 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  573 | `			return SXERR_ABORT;` |
|    - |  574 | `		}` |
|    - |  575 | `		/* Save the old consumer */` |
|    5 |  576 | `		xOld = pDecoder->xConsumer;` |
|    5 |  577 | `		pOld = pDecoder->pUserData;` |
|    - |  578 | `		/* Set the new consumer */` |
|    5 |  579 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|    5 |  580 | `		pDecoder->pUserData = pWorker;` |
|    - |  581 | `		/* Decode the array */` |
|    7 |  582 | `		for(;;){` |
|    - |  583 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  584 | `			 * do this.` |
|    - |  585 | `			 */` |
|   21 |  586 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    7 |  587 | `				pDecoder->pIn++;` |
|    1 |  588 | `			}` |
|   15 |  589 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|    5 |  590 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|    5 |  591 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    2 |  592 | `				}` |
|    5 |  593 | `				break;` |
|    - |  594 | `			}` |
|    - |  595 | `			/* Recurse and decode the entry */` |
|   11 |  596 | `			pDecoder->rec_count++;` |
|   11 |  597 | `			rc = VmJsonDecode(pDecoder,0);` |
|   11 |  598 | `			pDecoder->rec_count--;` |
|   11 |  599 | `			if( rc == SXERR_ABORT ){` |
|    - |  600 | `				/* Abort processing immediately */` |
|  ! 0 |  601 | `				return SXERR_ABORT;` |
|    - |  602 | `			}` |
|    - |  603 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   11 |  604 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   10 |  605 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  606 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  607 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  608 | `					return SXERR_ABORT;` |
|    - |  609 | `			}` |
|    1 |  610 | `		}` |
|    - |  611 | `		/* Restore the old consumer */` |
|    5 |  612 | `		pDecoder->xConsumer = xOld;` |
|    5 |  613 | `		pDecoder->pUserData = pOld;` |
|    - |  614 | `		/* Invoke the old consumer on the decoded array */` |
|    5 |  615 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   10 |  616 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  617 | `		ProcJsonConsumer xOld;` |
|    - |  618 | `		ph7_value *pKey;` |
|    - |  619 | `		void *pOld;` |
|    - |  620 | `		/* Object representation*/` |
|    8 |  621 | `		pDecoder->pIn++;` |
|    - |  622 | `		/* Return the object as an associative array */` |
|    8 |  623 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|    3 |  624 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|    - |  625 | `				"JSON Objects are always returned as an associative array"` |
|    - |  626 | `				);` |
|    1 |  627 | `		}` |
|    - |  628 | `		/* Create a working array */` |
|    8 |  629 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|    8 |  630 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|    8 |  631 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  632 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  633 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  634 | `			return SXERR_ABORT;` |
|    - |  635 | `		}` |
|    - |  636 | `		/* Save the old consumer */` |
|    8 |  637 | `		xOld = pDecoder->xConsumer;` |
|    8 |  638 | `		pOld = pDecoder->pUserData;` |
|    - |  639 | `		/* Set the new consumer */` |
|    8 |  640 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|    8 |  641 | `		pDecoder->pUserData = pWorker;` |
|    - |  642 | `		/* Decode the object */` |
|    6 |  643 | `		for(;;){` |
|    - |  644 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  645 | `			 * do this.` |
|    - |  646 | `			 */` |
|   16 |  647 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    3 |  648 | `				pDecoder->pIn++;` |
|    1 |  649 | `			}` |
|   14 |  650 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|    8 |  651 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|    6 |  652 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    2 |  653 | `				}` |
|    8 |  654 | `				break;` |
|    - |  655 | `			}` |
|    6 |  656 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|    8 |  657 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  658 | `					/* Syntax error,return immediately */` |
|  ! 0 |  659 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  660 | `					return SXERR_ABORT;` |
|    - |  661 | `			}` |
|    - |  662 | `			/* Dequote the key */` |
|    8 |  663 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  664 | `			/* Jump the key and the colon */` |
|    8 |  665 | `			pDecoder->pIn += 2;` |
|    - |  666 | `			/* Recurse and decode the value */` |
|    8 |  667 | `			pDecoder->rec_count++;` |
|    8 |  668 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|    8 |  669 | `			pDecoder->rec_count--;` |
|    8 |  670 | `			if( rc == SXERR_ABORT ){` |
|    - |  671 | `				/* Abort processing immediately */` |
|  ! 0 |  672 | `				return SXERR_ABORT;` |
|    - |  673 | `			}` |
|    - |  674 | `			/* Reset the internal buffer of the key */` |
|    8 |  675 | `			ph7_value_reset_string_cursor(pKey);` |
|    - |  676 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    2 |  677 | `		}` |
|    - |  678 | `		/* Restore the old consumer */` |
|    8 |  679 | `		pDecoder->xConsumer = xOld;` |
|    8 |  680 | `		pDecoder->pUserData = pOld;` |
|    - |  681 | `		/* Invoke the old consumer on the decoded object*/` |
|    8 |  682 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - |  683 | `		/* Release the key */` |
|    8 |  684 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|    5 |  685 | `	}else{` |
|    - |  686 | `		/* Unexpected token */` |
|  ! 0 |  687 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - |  688 | `	}` |
|    - |  689 | `	/* Release the worker variable */` |
|   26 |  690 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   26 |  691 | `	return SXRET_OK;` |
|   14 |  692 |  |
|    - |  693 | `/*` |
|    - |  694 | ` * The following JSON decoder callback is invoked each time` |
|    - |  695 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - |  696 | ` * is being decoded.` |
|    - |  697 | ` */` |
|   16 |  698 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  699 |  |
|   18 |  700 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - |  701 | `	/* Insert the entry */` |
|   18 |  702 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|    8 |  703 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - |  704 | `	/* All done */` |
|   18 |  705 | `	return SXRET_OK;` |
|    2 |  706 |  |
|    - |  707 | `/*` |
|    - |  708 | ` * Standard JSON decoder callback.` |
|    - |  709 | ` */` |
|    8 |  710 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  711 |  |
|    - |  712 | `	/* Return the value directly */` |
|   10 |  713 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|    4 |  714 | `	SXUNUSED(pKey); /* cc warning */` |
|    4 |  715 | `	SXUNUSED(pUserData);` |
|    - |  716 | `	/* All done */` |
|   10 |  717 | `	return SXRET_OK;` |
|    2 |  718 |  |
|    - |  719 | `/*` |
|    - |  720 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - |  721 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - |  722 | ` * Parameters` |
|    - |  723 | ` *  $json` |
|    - |  724 | ` *    The json string being decoded.` |
|    - |  725 | ` * $assoc` |
|    - |  726 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - |  727 | ` * $depth` |
|    - |  728 | ` *   User specified recursion depth.` |
|    - |  729 | ` * $options` |
|    - |  730 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - |  731 | ` * (default is to cast large integers as floats)` |
|    - |  732 | ` * Return` |
|    - |  733 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - |  734 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - |  735 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - |  736 | ` */` |
|   16 |  737 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  738 |  |
|   18 |  739 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  740 | `	json_decoder sDecoder;` |
|    - |  741 | `	const char *zIn;` |
|    - |  742 | `	SySet sToken;` |
|    - |  743 | `	SyLex sLex;` |
|    - |  744 | `	int nByte;` |
|    - |  745 | `	sxi32 rc;` |
|   18 |  746 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  747 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 |  748 | `		ph7_result_null(pCtx);` |
|  ! 0 |  749 | `		return PH7_OK;` |
|    - |  750 | `	}` |
|    - |  751 | `	/* Extract the JSON string */` |
|   18 |  752 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   18 |  753 | `	if( nByte < 1 ){` |
|    - |  754 | `		/* Empty string,return NULL */` |
|    3 |  755 | `		ph7_result_null(pCtx);` |
|    3 |  756 | `		return PH7_OK;` |
|    - |  757 | `	}` |
|    - |  758 | `	/* Clear JSON error code */` |
|   16 |  759 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  760 | `	/* Tokenize the input */` |
|   16 |  761 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16 |  762 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   16 |  763 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   16 |  764 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - |  765 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|    8 |  766 | `		SyLexRelease(&sLex);` |
|    8 |  767 | `		SySetRelease(&sToken);` |
|    - |  768 | `		/* return NULL */` |
|    8 |  769 | `		ph7_result_null(pCtx);` |
|    8 |  770 | `		return PH7_OK;` |
|    - |  771 | `	}` |
|    - |  772 | `	/* Fill the decoder */` |
|   10 |  773 | `	sDecoder.pCtx = pCtx;` |
|   10 |  774 | `	sDecoder.pErr = &pVm->json_rc;` |
|   10 |  775 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   10 |  776 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   10 |  777 | `	sDecoder.iFlags = 0;` |
|   10 |  778 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|    - |  779 | `		/* Returned objects will be converted into associative arrays */` |
|    8 |  780 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|    3 |  781 | `	}` |
|   10 |  782 | `	sDecoder.rec_depth = 32;` |
|   10 |  783 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|  ! 0 |  784 | `		int nDepth = ph7_value_to_int(apArg[2]);` |
|  ! 0 |  785 | `		if( nDepth > 1 && nDepth < 32 ){` |
|  ! 0 |  786 | `			sDecoder.rec_depth = nDepth;` |
|  ! 0 |  787 | `		}` |
|  ! 0 |  788 | `	}` |
|   10 |  789 | `	sDecoder.rec_count = 0;` |
|    - |  790 | `	/* Set a default consumer */` |
|   10 |  791 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   10 |  792 | `	sDecoder.pUserData = 0;` |
|    - |  793 | `	/* Decode the raw JSON input */` |
|   10 |  794 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   10 |  795 | `	if( rc == SXERR_ABORT \|\|  pVm->json_rc != JSON_ERROR_NONE ){` |
|    - |  796 | `		/*` |
|    - |  797 | `		 * Something goes wrong while decoding JSON input.Return NULL.` |
|    - |  798 | `		 */` |
|  ! 0 |  799 | `		ph7_result_null(pCtx);` |
|  ! 0 |  800 | `	}` |
|    - |  801 | `	/* Clean-up the mess left behind */` |
|   10 |  802 | `	SyLexRelease(&sLex);` |
|   10 |  803 | `	SySetRelease(&sToken);` |
|    - |  804 | `	/* All done */` |
|   10 |  805 | `	return PH7_OK;` |
|   10 |  806 |  |
|    - |  807 |  |
