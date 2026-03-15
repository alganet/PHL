/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *  JSON encoding/decoding routines.
 * Status:
 *    Devel.
 */
/* Forward reference */
static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);
static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);
/*
 * JSON encoder state is stored in an instance
 * of the following structure.
 */
typedef struct json_private_data json_private_data;
struct json_private_data
{
	ph7_context *pCtx; /* Call context */
	int isFirst;       /* True if first encoded entry */
	int iFlags;        /* JSON encoding flags */
	int nRecCount;     /* Recursion count */
};
/*
 * Returns the JSON representation of a value.In other word perform a JSON encoding operation.
 * According to wikipedia
 * JSON's basic types are:
 *   Number (double precision floating-point format in JavaScript, generally depends on implementation)
 *   String (double-quoted Unicode, with backslash escaping)
 *   Boolean (true or false)
 *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values
 *    do not need to be of the same type)
 *   Object (an unordered collection of key:value pairs with the ':' character separating the key
 *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should
 *     be distinct from each other)
 *   null (empty)
 * Non-significant white space may be added freely around the "structural characters"
 * (i.e. the brackets "[{]}", colon ":" and comma ",").
 */
static sxi32 VmJsonEncode(
	ph7_value *pIn,          /* Encode this value */
	json_private_data *pData /* Context data */
	){
		ph7_context *pCtx = pData->pCtx;
		int iFlags = pData->iFlags;
		int nByte;
		if( ph7_value_is_null(pIn) || ph7_value_is_resource(pIn)){
			/* null */
			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);
		}else if( ph7_value_is_bool(pIn) ){
			int iBool = ph7_value_to_bool(pIn);
			int iLen;
			/* true/false */
			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");
			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);
		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){
			const char *zNum;
			/* Get a string representation of the number */
			zNum = ph7_value_to_string(pIn,&nByte);
			ph7_result_string(pCtx,zNum,nByte);
		}else if( ph7_value_is_string(pIn) ){
			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){
				const char *zNum;
				/* Encodes numeric strings as numbers. */
				PH7_MemObjToReal(pIn); /* Force a numeric cast */
				/* Get a string representation of the number */
				zNum = ph7_value_to_string(pIn,&nByte);
				ph7_result_string(pCtx,zNum,nByte);
			}else{
				const char *zIn,*zEnd;
				int c;
				/* Encode the string */
				zIn = ph7_value_to_string(pIn,&nByte);
				zEnd = &zIn[nByte];
				/* Append the double quote */
				ph7_result_string(pCtx,"\"",(int)sizeof(char));
				for(;;){
					if( zIn >= zEnd ){
						/* No more input to process */
						break;
					}
					c = zIn[0];
					/* Advance the stream cursor */
					zIn++;
					if( (c == '<' || c == '>') && (iFlags & JSON_HEX_TAG) ){
						/* All < and > are converted to \u003C and \u003E */
						if( c == '<' ){
							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);
						}else{
							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);
						}
						continue;
					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){
						/* All &s are converted to \u0026.  */
						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);
						continue;
					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){
						/* All ' are converted to \u0027.   */
						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);
						continue;
					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){
						/* All " are converted to \u0022. */
						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);
						continue;
					}
					if( c == '"' || (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){
						/* Unescape the character */
						ph7_result_string(pCtx,"\\",(int)sizeof(char));
					}
					/* Append character verbatim */
					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
				}
				/* Append the double quote */
				ph7_result_string(pCtx,"\"",(int)sizeof(char));
			}
		}else if( ph7_value_is_array(pIn) ){
			int c = '[',d = ']';
			/* Encode the array */
			pData->isFirst = 1;
			if( iFlags & JSON_FORCE_OBJECT ){
				/* Outputs an object rather than an array */
				c = '{';
				d = '}';
			}
			/* Append the square bracket or curly braces */
			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
			/* Iterate throw array entries */
			ph7_array_walk(pIn,VmJsonArrayEncode,pData);
			/* Append the closing square bracket or curly braces */
			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));
		}else if( ph7_value_is_object(pIn) ){
			/* Encode the class instance */
			pData->isFirst = 1;
			/* Append the curly braces */
			ph7_result_string(pCtx,"{",(int)sizeof(char));
			/* Iterate throw class attribute */
			ph7_object_walk(pIn,VmJsonObjectEncode,pData);
			/* Append the closing curly braces  */
			ph7_result_string(pCtx,"}",(int)sizeof(char));
		}else{
			/* Can't happen */
			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);
		}
		/* All done */
		return PH7_OK;
}
/*
 * The following walker callback is invoked each time we need
 * to encode an array to JSON.
 */
static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	json_private_data *pJson = (json_private_data *)pUserData;
	if( pJson->nRecCount > 31 ){
		/* Recursion limit reached,return immediately */
		return PH7_OK;
	}
	if( !pJson->isFirst ){
		/* Append the colon first */
		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));
	}
	if( pJson->iFlags & JSON_FORCE_OBJECT ){
		/* Outputs an object rather than an array */
		const char *zKey;
		int nByte;
		/* Extract a string representation of the key */
		zKey = ph7_value_to_string(pKey,&nByte);
		/* Append the key and the double colon */
		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);
	}
	/* Encode the value */
	pJson->nRecCount++;
	VmJsonEncode(pValue,pJson);
	pJson->nRecCount--;
	pJson->isFirst = 0;
	return PH7_OK;
}
/*
 * The following walker callback is invoked each time we need to encode
 * a class instance [i.e: Object in the PHP jargon] to JSON.
 */
static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)
{
	json_private_data *pJson = (json_private_data *)pUserData;
	if( pJson->nRecCount > 31 ){
		/* Recursion limit reached,return immediately */
		return PH7_OK;
	}
	if( !pJson->isFirst ){
		/* Append the colon first */
		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));
	}
	/* Append the attribute name and the double colon first */
	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);
	/* Encode the value */
	pJson->nRecCount++;
	VmJsonEncode(pValue,pJson);
	pJson->nRecCount--;
	pJson->isFirst = 0;
	return PH7_OK;
}
/*
 * string json_encode(mixed $value [, int $options = 0 ])
 *  Returns a string containing the JSON representation of value.
 * Parameters
 *  $value
 *  The value being encoded. Can be any type except a resource.
 * $options
 *  Bitmask consisting of:
 *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.
 *  JSON_HEX_AMP   All &s are converted to \u0026.
 *  JSON_HEX_APOS  All ' are converted to \u0027.
 *  JSON_HEX_QUOT  All " are converted to \u0022.
 *  JSON_FORCE_OBJECT  Outputs an object rather than an array.
 *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.
 *  JSON_BIGINT_AS_STRING   Not used
 *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.
 *  JSON_UNESCAPED_SLASHES  Don't escape '/'
 *  JSON_UNESCAPED_UNICODE  Not used.
 * Return
 *  Returns a JSON encoded string on success. FALSE otherwise
 */
PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	json_private_data sJson;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Prepare the JSON data */
	sJson.nRecCount = 0;
	sJson.pCtx = pCtx;
	sJson.isFirst = 1;
	sJson.iFlags = 0;
	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){
		/* Extract option flags */
		sJson.iFlags = ph7_value_to_int(apArg[1]);
	}
	/* Perform the encoding operation */
	VmJsonEncode(apArg[0],&sJson);
	/* All done */
	return PH7_OK;
}
/*
 * int json_last_error(void)
 *  Returns the last error (if any) occurred during the last JSON encoding/decoding.
 * Parameters
 *  None
 * Return
 *  Returns an integer, the value can be one of the following constants:
 *  JSON_ERROR_NONE            No error has occurred.
 *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.
 *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.
 *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.
 *  JSON_ERROR_SYNTAX          Syntax error.
 *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.
 */
PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	/* Return the error code */
	ph7_result_int(pCtx,pVm->json_rc);
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	return PH7_OK;
}
/* Possible tokens from the JSON tokenization process */
#define JSON_TK_TRUE    0x001 /* Boolean true */
#define JSON_TK_FALSE   0x002 /* Boolean false */
#define JSON_TK_STR     0x004 /* String enclosed in double quotes */
#define JSON_TK_NULL    0x008 /* null */
#define JSON_TK_NUM     0x010 /* Numeric */
#define JSON_TK_OCB     0x020 /* Open curly braces '{' */
#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */
#define JSON_TK_OSB     0x080 /* Open square bracke '[' */
#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */
#define JSON_TK_COLON   0x200 /* Single colon ':' */
#define JSON_TK_COMMA   0x400 /* Single comma ',' */
#define JSON_TK_INVALID 0x800 /* Unexpected token */
/*
 * Tokenize an entire JSON input.
 * Get a single low-level token from the input file.
 * Update the stream pointer so that it points to the first
 * character beyond the extracted token.
 */
static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)
{
	int *pJsonErr = (int *)pUserData;
	SyString *pStr;
	int c;
	/* Ignore leading white spaces */
	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){
		/* Advance the stream cursor */
		if( pStream->zText[0] == '\n' ){
			/* Update line counter */
			pStream->nLine++;
		}
		pStream->zText++;
	}
	if( pStream->zText >= pStream->zEnd ){
		/* End of input reached */
		SXUNUSED(pCtxData); /* cc warning */
		return SXERR_EOF;
	}
	/* Record token starting position and line */
	pToken->nLine = pStream->nLine;
	pToken->pUserData = 0;
	pStr = &pToken->sData;
	SyStringInitFromBuf(pStr,pStream->zText,0);
	if( pStream->zText[0] == '{' || pStream->zText[0] == '[' || pStream->zText[0] == '}' || pStream->zText[0] == ']'
		|| pStream->zText[0] == ':' || pStream->zText[0] == ',' ){
			/* Single character */
			c = pStream->zText[0];
			/* Set token type */
			switch(c){
			case '[': pToken->nType = JSON_TK_OSB;   break;
			case '{': pToken->nType = JSON_TK_OCB;   break;
			case '}': pToken->nType = JSON_TK_CCB;   break;
			case ']': pToken->nType = JSON_TK_CSB;   break;
			case ':': pToken->nType = JSON_TK_COLON; break;
			case ',': pToken->nType = JSON_TK_COMMA; break;
			default:
				break;
			}
			/* Advance the stream cursor */
			pStream->zText++;
	}else if( pStream->zText[0] == '"') {
		/* JSON string */
		pStream->zText++;
		pStr->zString++;
		/* Delimit the string */
		while( pStream->zText < pStream->zEnd ){
			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){
				break;
			}
			if( pStream->zText[0] == '\n' ){
				/* Update line counter */
				pStream->nLine++;
			}
			pStream->zText++;
		}
		if( pStream->zText >= pStream->zEnd ){
			/* Missing closing '"' */
			pToken->nType = JSON_TK_INVALID;
			*pJsonErr = JSON_ERROR_SYNTAX;
		}else{
			pToken->nType = JSON_TK_STR;
			pStream->zText++; /* Jump the closing double quotes */
		}
	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
		/* Number */
		pStream->zText++;
		pToken->nType = JSON_TK_NUM;
		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
			pStream->zText++;
		}
		if( pStream->zText < pStream->zEnd ){
			c = pStream->zText[0];
			if( c == '.' ){
					/* Real number */
					pStream->zText++;
					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
						pStream->zText++;
					}
					if( pStream->zText < pStream->zEnd ){
						c = pStream->zText[0];
						if( c=='e' || c=='E' ){
							pStream->zText++;
							if( pStream->zText < pStream->zEnd ){
								c = pStream->zText[0];
								if( c =='+' || c=='-' ){
									pStream->zText++;
								}
								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
									pStream->zText++;
								}
							}
						}
					}
				}else if( c=='e' || c=='E' ){
					/* Real number */
					pStream->zText++;
					if( pStream->zText < pStream->zEnd ){
						c = pStream->zText[0];
						if( c =='+' || c=='-' ){
							pStream->zText++;
						}
						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
							pStream->zText++;
						}
					}
				}
			}
	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&
		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){
			/* boolean true */
			pToken->nType = JSON_TK_TRUE;
			/* Advance the stream cursor */
			pStream->zText += sizeof("true")-1;
	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&
		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){
			/* boolean false */
			pToken->nType = JSON_TK_FALSE;
			/* Advance the stream cursor */
			pStream->zText += sizeof("false")-1;
	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&
		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){
			/* NULL */
			pToken->nType = JSON_TK_NULL;
			/* Advance the stream cursor */
			pStream->zText += sizeof("null")-1;
	}else{
		/* Unexpected token */
		pToken->nType = JSON_TK_INVALID;
		/* Advance the stream cursor */
		pStream->zText++;
		*pJsonErr = JSON_ERROR_SYNTAX;
		/* Abort processing immediatley */
		return SXERR_ABORT;
	}
	/* record token length */
	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
	if( pToken->nType == JSON_TK_STR ){
		pStr->nByte--;
	}
	/* Return to the lexer */
	return SXRET_OK;
}
/*
 * JSON decoded input consumer callback signature.
 */
typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);
/*
 * JSON decoder state is kept in the following structure.
 */
typedef struct json_decoder json_decoder;
struct json_decoder
{
	ph7_context *pCtx; /* Call context */
	ProcJsonConsumer xConsumer; /* Consumer callback */
	void *pUserData;   /* Last argument to xConsumer() */
	int iFlags;        /* Configuration flags */
	SyToken *pIn;      /* Token stream */
	SyToken *pEnd;     /* End of the token stream */
	int rec_depth;     /* Recursion limit */
	int rec_count;     /* Current nesting level */
	int *pErr;         /* JSON decoding error if any */
};
#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */
/* Forward declaration */
static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);
/*
 * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store
 * the result in the given ph7_value.
 */
static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)
{
	const char *zIn = pStr->zString;
	const char *zEnd = &pStr->zString[pStr->nByte];
	const char *zCur;
	int c;
	/* Mark the value as a string */
	ph7_value_string(pWorker,"",0); /* Empty string */
	for(;;){
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '\\' ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append chunk verbatim */
			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));
		}
		zIn++;
		if( zIn >= zEnd ){
			/* End of the input reached */
			break;
		}
		c = zIn[0];
		/* Unescape the character */
		switch(c){
		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;
		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;
		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;
		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;
		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;
		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;
		default:
			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));
			break;
		}
		/* Advance the stream cursor */
		zIn++;
	}
}
/*
 * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.
 * According to wikipedia
 * JSON's basic types are:
 *   Number (double precision floating-point format in JavaScript, generally depends on implementation)
 *   String (double-quoted Unicode, with backslash escaping)
 *   Boolean (true or false)
 *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values
 *    do not need to be of the same type)
 *   Object (an unordered collection of key:value pairs with the ':' character separating the key
 *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should
 *     be distinct from each other)
 *   null (empty)
 * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").
 */
static sxi32 VmJsonDecode(
	json_decoder *pDecoder, /* JSON decoder */
	ph7_value *pArrayKey    /* Key for the decoded array */
	){
	ph7_value *pWorker; /* Worker variable */
	sxi32 rc;
	/* Check if we do not nest to much */
	if( pDecoder->rec_count >= pDecoder->rec_depth ){
		/* Nesting limit reached,abort decoding immediately */
		*pDecoder->pErr = JSON_ERROR_DEPTH;
		return SXERR_ABORT;
	}
	if( pDecoder->pIn->nType & (JSON_TK_STR|JSON_TK_TRUE|JSON_TK_FALSE|JSON_TK_NULL|JSON_TK_NUM) ){
		/* Scalar value */
		pWorker = ph7_context_new_scalar(pDecoder->pCtx);
		if( pWorker == 0 ){
			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			/* Abort the decoding operation immediately */
			return SXERR_ABORT;
		}
		/* Reflect the JSON image */
		if( pDecoder->pIn->nType & JSON_TK_NULL ){
			/* Nullify the value.*/
			ph7_value_null(pWorker);
		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE|JSON_TK_FALSE) ){
			/* Boolean value */
			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );
		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){
			SyString *pStr = &pDecoder->pIn->sData;
			/*
			 * Numeric value.
			 * Get a string representation first then try to get a numeric
			 * value.
			 */
			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);
			/* Obtain a numeric representation */
			PH7_MemObjToNumeric(pWorker);
		}else{
			/* Dequote the string */
			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);
		}
		/* Invoke the consumer callback */
		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* All done,advance the stream cursor */
		pDecoder->pIn++;
	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {
		ProcJsonConsumer xOld;
		void *pOld;
		/* Array representation*/
		pDecoder->pIn++;
		/* Create a working array */
		pWorker = ph7_context_new_array(pDecoder->pCtx);
		if( pWorker == 0 ){
			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			/* Abort the decoding operation immediately */
			return SXERR_ABORT;
		}
		/* Save the old consumer */
		xOld = pDecoder->xConsumer;
		pOld = pDecoder->pUserData;
		/* Set the new consumer */
		pDecoder->xConsumer = VmJsonArrayDecoder;
		pDecoder->pUserData = pWorker;
		/* Decode the array */
		for(;;){
			/* Jump trailing comma. Note that the standard PHP engine will not let you
			 * do this.
			 */
			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){
				pDecoder->pIn++;
			}
			if( pDecoder->pIn >= pDecoder->pEnd || (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){
				if( pDecoder->pIn < pDecoder->pEnd ){
					pDecoder->pIn++; /* Jump the trailing ']' */
				}
				break;
			}
			/* Recurse and decode the entry */
			pDecoder->rec_count++;
			rc = VmJsonDecode(pDecoder,0);
			pDecoder->rec_count--;
			if( rc == SXERR_ABORT ){
				/* Abort processing immediately */
				return SXERR_ABORT;
			}
			/*The cursor is automatically advanced by the VmJsonDecode() function */
			if( (pDecoder->pIn < pDecoder->pEnd) &&
				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/|JSON_TK_COMMA/*','*/))==0) ){
					/* Unexpected token,abort immediatley */
					*pDecoder->pErr = JSON_ERROR_SYNTAX;
					return SXERR_ABORT;
			}
		}
		/* Restore the old consumer */
		pDecoder->xConsumer = xOld;
		pDecoder->pUserData = pOld;
		/* Invoke the old consumer on the decoded array */
		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);
	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {
		ProcJsonConsumer xOld;
		ph7_value *pKey;
		void *pOld;
		/* Object representation*/
		pDecoder->pIn++;
		/* Return the object as an associative array */
		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){
			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,
				"JSON Objects are always returned as an associative array"
				);
		}
		/* Create a working array */
		pWorker = ph7_context_new_array(pDecoder->pCtx);
		pKey = ph7_context_new_scalar(pDecoder->pCtx);
		if( pWorker == 0 || pKey == 0){
			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			/* Abort the decoding operation immediately */
			return SXERR_ABORT;
		}
		/* Save the old consumer */
		xOld = pDecoder->xConsumer;
		pOld = pDecoder->pUserData;
		/* Set the new consumer */
		pDecoder->xConsumer = VmJsonArrayDecoder;
		pDecoder->pUserData = pWorker;
		/* Decode the object */
		for(;;){
			/* Jump trailing comma. Note that the standard PHP engine will not let you
			 * do this.
			 */
			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){
				pDecoder->pIn++;
			}
			if( pDecoder->pIn >= pDecoder->pEnd || (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){
				if( pDecoder->pIn < pDecoder->pEnd ){
					pDecoder->pIn++; /* Jump the trailing ']' */
				}
				break;
			}
			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 || &pDecoder->pIn[1] >= pDecoder->pEnd
				|| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){
					/* Syntax error,return immediately */
					*pDecoder->pErr = JSON_ERROR_SYNTAX;
					return SXERR_ABORT;
			}
			/* Dequote the key */
			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);
			/* Jump the key and the colon */
			pDecoder->pIn += 2;
			/* Recurse and decode the value */
			pDecoder->rec_count++;
			rc = VmJsonDecode(pDecoder,pKey);
			pDecoder->rec_count--;
			if( rc == SXERR_ABORT ){
				/* Abort processing immediately */
				return SXERR_ABORT;
			}
			/* Reset the internal buffer of the key */
			ph7_value_reset_string_cursor(pKey);
			/*The cursor is automatically advanced by the VmJsonDecode() function */
		}
		/* Restore the old consumer */
		pDecoder->xConsumer = xOld;
		pDecoder->pUserData = pOld;
		/* Invoke the old consumer on the decoded object*/
		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);
		/* Release the key */
		ph7_context_release_value(pDecoder->pCtx,pKey);
	}else{
		/* Unexpected token */
		return SXERR_ABORT; /* Abort immediately */
	}
	/* Release the worker variable */
	ph7_context_release_value(pDecoder->pCtx,pWorker);
	return SXRET_OK;
}
/*
 * The following JSON decoder callback is invoked each time
 * a JSON array representation [i.e: [15,"hello",FALSE] ]
 * is being decoded.
 */
static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)
{
	ph7_value *pArray = (ph7_value *)pUserData;
	/* Insert the entry */
	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */
	SXUNUSED(pCtx); /* cc warning */
	/* All done */
	return SXRET_OK;
}
/*
 * Standard JSON decoder callback.
 */
static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)
{
	/* Return the value directly */
	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */
	SXUNUSED(pKey); /* cc warning */
	SXUNUSED(pUserData);
	/* All done */
	return SXRET_OK;
}
/*
 * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])
 *  Takes a JSON encoded string and converts it into a PHP variable.
 * Parameters
 *  $json
 *    The json string being decoded.
 * $assoc
 *   When TRUE, returned objects will be converted into associative arrays.
 * $depth
 *   User specified recursion depth.
 * $options
 *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported
 * (default is to cast large integers as floats)
 * Return
 *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)
 *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded
 *  or if the encoded data is deeper than the recursion limit.
 */
PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	json_decoder sDecoder;
	const char *zIn;
	SySet sToken;
	SyLex sLex;
	int nByte;
	sxi32 rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments, return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the JSON string */
	zIn = ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		/* Empty string,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Clear JSON error code */
	pVm->json_rc = JSON_ERROR_NONE;
	/* Tokenize the input */
	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));
	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);
	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);
	if( pVm->json_rc != JSON_ERROR_NONE ){
		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */
		SyLexRelease(&sLex);
		SySetRelease(&sToken);
		/* return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the decoder */
	sDecoder.pCtx = pCtx;
	sDecoder.pErr = &pVm->json_rc;
	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);
	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];
	sDecoder.iFlags = 0;
	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){
		/* Returned objects will be converted into associative arrays */
		sDecoder.iFlags |= JSON_DECODE_ASSOC;
	}
	sDecoder.rec_depth = 32;
	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){
		int nDepth = ph7_value_to_int(apArg[2]);
		if( nDepth > 1 && nDepth < 32 ){
			sDecoder.rec_depth = nDepth;
		}
	}
	sDecoder.rec_count = 0;
	/* Set a default consumer */
	sDecoder.xConsumer = VmJsonDefaultDecoder;
	sDecoder.pUserData = 0;
	/* Decode the raw JSON input */
	rc = VmJsonDecode(&sDecoder,0);
	if( rc == SXERR_ABORT ||  pVm->json_rc != JSON_ERROR_NONE ){
		/*
		 * Something goes wrong while decoding JSON input.Return NULL.
		 */
		ph7_result_null(pCtx);
	}
	/* Clean-up the mess left behind */
	SyLexRelease(&sLex);
	SySetRelease(&sToken);
	/* All done */
	return PH7_OK;
}
