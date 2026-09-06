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
	int isObject;      /* True if the current array level is encoded as a JSON object */
	int iFlags;        /* JSON encoding flags */
	int nRecCount;     /* Recursion count */
	int exc;           /* True if a jsonSerialize() callback threw an exception */
	int oom;           /* True if a result append ran out of memory (raises a fatal) */
	int fail;          /* True if the value is unencodable (php 8.1: a non-backed
	                    * enum case) — json_encode returns FALSE */
};
/*
 * Emit into the JSON result, flagging OOM on the shared data and bailing out
 * of the current encode function (which returns PH7_OK; the top-level
 * vm_builtin_json_encode checks ->oom and raises a non-catchable fatal). Used
 * for every ph7_result_string/ph7_result_string_format append below.
 */
#define JSON_EMIT(pD, call) do { if( (call) != SXRET_OK ){ (pD)->oom = 1; return PH7_OK; } } while(0)
/*
 * Emit a float in php's json shape: PH7_AppendShortestReal (the shared
 * serialize/var_export shortest-round-trip formatter, php's
 * serialize_precision=-1) with the exponent marker lowercased (json prints
 * 1.0e+17 where serialize prints 1.0E+17).
 */
static sxi32 VmJsonEmitReal(ph7_context *pCtx,double rVal)
{
	SyBlob sNum;
	char *z;
	sxu32 i,n;
	sxi32 rc;
	SyBlobInit(&sNum,&pCtx->pVm->sAllocator);
	PH7_AppendShortestReal(&sNum,rVal);
	z = (char *)SyBlobData(&sNum);
	n = SyBlobLength(&sNum);
	if( z == 0 || n < 1 ){
		SyBlobRelease(&sNum);
		return SXERR_MEM; /* treated as OOM by JSON_EMIT */
	}
	for( i = 0 ; i < n ; i++ ){
		if( z[i] == 'E' ){
			z[i] = 'e';
		}
	}
	rc = ph7_result_string(pCtx,(const char *)z,(int)n);
	SyBlobRelease(&sNum);
	return rc;
}
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
			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));
		}else if( ph7_value_is_bool(pIn) ){
			int iBool = ph7_value_to_bool(pIn);
			int iLen;
			/* true/false */
			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");
			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));
		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){
			if( ph7_value_is_float(pIn) ){
				/* php's json float output follows serialize_precision
				 * (shortest round-trip, like serialize/var_export), NOT the
				 * echo/cast precision of 14 — with a lowercase exponent
				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,
				 * 1.0 -> 1, -0.0 -> -0. */
				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));
			}else{
				const char *zNum;
				/* Get a string representation of the number */
				zNum = ph7_value_to_string(pIn,&nByte);
				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));
			}
		}else if( ph7_value_is_string(pIn) ){
			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){
				/* Encodes numeric strings as numbers (same float shapes). */
				PH7_MemObjToReal(pIn); /* Force a numeric cast */
				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));
			}else{
				const char *zIn,*zEnd;
				int c;
				/* Encode the string */
				zIn = ph7_value_to_string(pIn,&nByte);
				zEnd = &zIn[nByte];
				/* Append the double quote */
				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));
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
							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1));
						}else{
							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1));
						}
						continue;
					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){
						/* All &s are converted to \u0026.  */
						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1));
						continue;
					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){
						/* All ' are converted to \u0027.   */
						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1));
						continue;
					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){
						/* All " are converted to \u0022. */
						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1));
						continue;
					}
					if( c == '"' || c == '\\' ){
						/* Escape the quote/backslash (php escapes the backslash
						 * unconditionally — the old code wrongly tied it to
						 * JSON_UNESCAPED_SLASHES, which governs '/' below) */
						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));
					}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){
						/* php escapes forward slashes by default */
						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));
					}else if( (unsigned char)c < 0x20 ){
						/* Control characters (band A #4): php emits the short
						 * escapes for \b \f \n \r \t and \u00xx for the rest —
						 * pre-fix these were emitted RAW (invalid JSON). */
						static const char zHex[] = "0123456789abcdef";
						char zEsc[6] = { '\\', 'u', '0', '0', 0, 0 };
						switch(c){
						case '\b': JSON_EMIT(pData,ph7_result_string(pCtx,"\\b",2)); break;
						case '\f': JSON_EMIT(pData,ph7_result_string(pCtx,"\\f",2)); break;
						case '\n': JSON_EMIT(pData,ph7_result_string(pCtx,"\\n",2)); break;
						case '\r': JSON_EMIT(pData,ph7_result_string(pCtx,"\\r",2)); break;
						case '\t': JSON_EMIT(pData,ph7_result_string(pCtx,"\\t",2)); break;
						default:
							zEsc[4] = zHex[(c >> 4) & 0x0F];
							zEsc[5] = zHex[c & 0x0F];
							JSON_EMIT(pData,ph7_result_string(pCtx,zEsc,6));
							break;
						}
						continue;
					}
					/* Append character verbatim */
					JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));
				}
				/* Append the double quote */
				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));
			}
		}else if( ph7_value_is_array(pIn) ){
			/* An array encodes as a JSON array iff it is a "list" [consecutive
			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an
			 * object with stringified keys (PHP semantics). */
			int isObject = (iFlags & JSON_FORCE_OBJECT)
				|| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);
			int savedObject = pData->isObject; /* restore for sibling entries after recursion */
			int c = isObject ? '{' : '[';
			int d = isObject ? '}' : ']';
			/* Encode the array */
			pData->isObject = isObject;
			pData->isFirst = 1;
			/* Append the square bracket or curly braces */
			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));
			/* Iterate throw array entries */
			ph7_array_walk(pIn,VmJsonArrayEncode,pData);
			/* Bail if a nested append ran out of memory before the closer */
			if( pData->oom ){
				return PH7_OK;
			}
			/* Append the closing square bracket or curly braces */
			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));
			pData->isObject = savedObject;
		}else if( ph7_value_is_object(pIn) ){
			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;
			ph7_vm *pVm = pIn->pVm;
			ph7_class_method *pMethod = 0;
			/* If the object implements JsonSerializable, encode the value
			 * returned by jsonSerialize() instead of its public properties.
			 * An enum implementing it explicitly also takes this path (php). */
			if( pVm->pJsonSerializableClass
				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){
				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);
			}
			if( pMethod == 0 && (pThis->pClass->iFlags & PH7_CLASS_ENUM) != 0 ){
				/* php 8.1: a BACKED enum case encodes as its backing value; a
				 * pure enum case has no default serialization — json_encode
				 * returns false. */
				ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pThis);
				if( pBacking ){
					pData->nRecCount++;
					VmJsonEncode(pBacking,pData);
					pData->nRecCount--;
				}else{
					pData->fail = 1;
				}
				return PH7_OK;
			}
			if( pMethod ){
				ph7_value sResult;
				sxi32 rc;
				PH7_MemObjInit(pVm,&sResult);
				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);
				if( rc == PH7_EXCEPTION ){
					/* Let jsonSerialize()'s throw propagate */
					PH7_MemObjRelease(&sResult);
					pData->exc = 1;
					return PH7_EXCEPTION;
				}
				/* Encode the returned value [scalar/array/object] */
				pData->nRecCount++;
				VmJsonEncode(&sResult,pData);
				pData->nRecCount--;
				PH7_MemObjRelease(&sResult);
				if( pData->exc ){
					return PH7_EXCEPTION;
				}
				if( pData->oom ){
					return PH7_OK;
				}
			}else{
				SyHashEntry *pAttrEntry;
				SySet sNames;
				SyString *aName;
				sxu32 iName,nName;
				/* Encode the class instance: php serializes only PUBLIC
				 * non-static properties, reading through a PHP 8.4 get hook
				 * when one is declared (virtual properties included). The
				 * names are SNAPSHOTTED first — a hook dispatched mid-walk may
				 * re-enter an hAttr walk on this instance (the hash has a
				 * single embedded loop cursor) or unset()/create properties;
				 * names point into class-owned attr storage and each is
				 * re-looked-up before use. */
				pData->isFirst = 1;
				/* Append the curly braces */
				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));
				SySetInit(&sNames,&pVm->sAllocator,sizeof(SyString));
				SyHashResetLoopCursor(&pThis->hAttr);
				while( (pAttrEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
					VmClassAttr *pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;
					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT))
					 || pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){
						continue;
					}
					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_VIRTUAL))
					 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){
						continue; /* virtual set-only property: no value to encode (php) */
					}
					SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);
				}
				aName = (SyString *)SySetBasePtr(&sNames);
				nName = SySetUsed(&sNames);
				for( iName = 0 ; iName < nName ; ++iName ){
					VmClassAttr *pVmAttr;
					ph7_value *pAttrVal = 0;
					ph7_value sHookVal;
					sxi32 rcHk;
					pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)aName[iName].zString,aName[iName].nByte);
					if( pAttrEntry == 0 ){
						continue; /* unset by an earlier hook */
					}
					pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;
					PH7_MemObjInit(pVm,&sHookVal);
					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);
					if( rcHk == SXRET_OK ){
						pAttrVal = &sHookVal;
					}else if( rcHk == SXERR_NOTFOUND ){
						/* Encode a COPY: the encoder casts scalars in place
						 * (ph7_value_to_string), which must not corrupt the
						 * live attribute slot. */
						ph7_value *pRaw = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
						if( pRaw ){
							PH7_MemObjStore(pRaw,&sHookVal);
							pAttrVal = &sHookVal;
						}
					}else{
						/* the get hook threw — propagate like jsonSerialize() */
						PH7_MemObjRelease(&sHookVal);
						SySetRelease(&sNames);
						pData->exc = 1;
						return PH7_EXCEPTION;
					}
					if( pAttrVal ){
						VmJsonObjectEncode(SyStringData(&pVmAttr->pAttr->sName),pAttrVal,pData);
					}
					PH7_MemObjRelease(&sHookVal);
					if( pData->exc ){
						SySetRelease(&sNames);
						return PH7_EXCEPTION; /* a nested jsonSerialize()/hook threw */
					}
					if( pData->oom ){
						SySetRelease(&sNames);
						return PH7_OK;
					}
				}
				SySetRelease(&sNames);
				/* Append the closing curly braces  */
				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));
			}
		}else{
			/* Can't happen */
			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));
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
	if( pJson->nRecCount > 31 || pJson->exc || pJson->oom ){
		/* Recursion limit reached, a callback threw, or OOM — return immediately */
		return PH7_OK;
	}
	if( !pJson->isFirst ){
		/* Append the colon first */
		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));
	}
	if( pJson->isObject ){
		/* Outputs an object rather than an array */
		const char *zKey;
		int nByte;
		/* Extract a string representation of the key */
		zKey = ph7_value_to_string(pKey,&nByte);
		/* Append the quoted key and the colon (checked, so an OOM here is caught
		 * rather than silently truncating; matches the prior "%.*s" emit byte for
		 * byte — keys are not JSON-escaped, a pre-existing behavior). */
		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));
		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zKey,nByte));
		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));
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
	if( pJson->nRecCount > 31 || pJson->exc || pJson->oom ){
		/* Recursion limit reached, a callback threw, or OOM — return immediately */
		return PH7_OK;
	}
	if( !pJson->isFirst ){
		/* Append the colon first */
		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));
	}
	/* Append the quoted attribute name and the colon (checked; matches the prior
	 * "%s" emit byte for byte — attribute names are not JSON-escaped). */
	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));
	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zAttr,-1));
	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));
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
	sxi32 rc;
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
	sJson.exc = 0;
	sJson.oom = 0;
	sJson.fail = 0;
	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){
		/* Extract option flags */
		sJson.iFlags = ph7_value_to_int(apArg[1]);
	}
	pCtx->pVm->json_rc = JSON_ERROR_NONE;
	/* Perform the encoding operation */
	rc = VmJsonEncode(apArg[0],&sJson);
	if( sJson.oom ){
		/* A result append ran out of memory: raise a non-catchable fatal,
		 * distinct from a JSON-encoding error (json_last_error untouched). */
		return PH7_ContextMemoryError(pCtx);
	}
	if( rc == PH7_EXCEPTION || sJson.exc ){
		/* A jsonSerialize() callback threw — propagate so the exception unwinds */
		return PH7_EXCEPTION;
	}
	if( sJson.fail ){
		/* Unencodable value (php 8.1: non-backed enum case): the whole encode
		 * fails — discard whatever was emitted and return FALSE. */
		pCtx->pVm->json_rc = JSON_ERROR_NON_BACKED_ENUM;
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* All done */
	return PH7_OK;
}
#undef JSON_EMIT
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
/*
 * string json_last_error_msg(void)
 *  Returns the error string of the last JSON encoding/decoding operation.
 * Parameters
 *  None
 * Return
 *  Returns the human-readable message corresponding to the last json_last_error()
 *  code, or "No error" if no error has occurred.
 */
PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zMsg;
	switch( pVm->json_rc ){
	case JSON_ERROR_NONE:
		zMsg = "No error";
		break;
	case JSON_ERROR_DEPTH:
		zMsg = "Maximum stack depth exceeded";
		break;
	case JSON_ERROR_STATE_MISMATCH:
		zMsg = "State mismatch (invalid or malformed JSON)";
		break;
	case JSON_ERROR_CTRL_CHAR:
		zMsg = "Control character error, possibly incorrectly encoded";
		break;
	case JSON_ERROR_SYNTAX:
		zMsg = "Syntax error";
		break;
	case JSON_ERROR_UTF8:
		zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded";
		break;
	case JSON_ERROR_NON_BACKED_ENUM:
		zMsg = "Non-backed enums have no default serialization";
		break;
	default:
		zMsg = "Unknown error";
		break;
	}
	ph7_result_string(pCtx,zMsg,-1/* Compute length automatically */);
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
/*
 * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().
 * On success the decoded value is delivered through the default decoder (i.e: it becomes
 * the call-context result, which json_validate's caller then overwrites with a boolean).
 * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a
 * non-zero json_err_code otherwise. A generic decoder abort without a specific code
 * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single
 * value, preserving the original "abort || error => failure" json_decode semantics.
 */
static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth)
{
	ph7_vm *pVm = pCtx->pVm;
	json_decoder sDecoder;
	SySet sToken;
	SyLex sLex;
	sxi32 rc;
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
		return pVm->json_rc;
	}
	/* Fill the decoder */
	sDecoder.pCtx = pCtx;
	sDecoder.pErr = &pVm->json_rc;
	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);
	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];
	sDecoder.iFlags = 0;
	if( iAssoc ){
		/* Returned objects will be converted into associative arrays */
		sDecoder.iFlags |= JSON_DECODE_ASSOC;
	}
	sDecoder.rec_depth = 32;
	if( nDepth > 1 && nDepth < 32 ){
		sDecoder.rec_depth = nDepth;
	}
	sDecoder.rec_count = 0;
	/* Set a default consumer */
	sDecoder.xConsumer = VmJsonDefaultDecoder;
	sDecoder.pUserData = 0;
	/* Decode the raw JSON input */
	rc = VmJsonDecode(&sDecoder,0);
	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){
		/* Generic abort with no specific code: treat as a syntax error */
		pVm->json_rc = JSON_ERROR_SYNTAX;
	}
	/* Clean-up the mess left behind */
	SyLexRelease(&sLex);
	SySetRelease(&sToken);
	return pVm->json_rc;
}
PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte;
	int iAssoc = 0;
	int nDepth = 32;
	/* php coerces a scalar argument to string here (weak mode); the shared ZPP
	 * screen in vm.c has already rejected the values that cannot coerce. */
	if( nArg < 1 ){
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
	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){
		iAssoc = 1;
	}
	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){
		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);
		 * read as int64 so a value above INT_MAX is detected, not truncated. */
		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);
		/* php clears the json error state before validating $depth, so a caught
		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal
		 * path resets it again inside VmJsonDecodeInput). */
		pCtx->pVm->json_rc = JSON_ERROR_NONE;
		if( nWant <= 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"json_decode(): Argument #3 ($depth) must be greater than 0");
		}
		if( nWant > 2147483647 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"json_decode(): Argument #3 ($depth) must be less than 2147483647");
		}
		nDepth = (int)nWant;
	}
	/* Decode the raw JSON input.The default consumer sets the decoded value as the
	 * call-context result; on failure we replace it with NULL. */
	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth) != JSON_ERROR_NONE ){
		/* Something goes wrong while decoding JSON input.Return NULL. */
		ph7_result_null(pCtx);
	}
	/* All done */
	return PH7_OK;
}
/*
 * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])
 *  Validates whether a string is valid JSON without materializing a value.
 * Parameters
 *  $json   The string to validate.
 *  $depth  Maximum nesting depth (clamped to the engine limit of 32).
 *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).
 * Return
 *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().
 */
PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zIn;
	int nByte;
	int nDepth = 32;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument: not valid JSON */
		pVm->json_rc = JSON_ERROR_SYNTAX;
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the JSON string */
	zIn = ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		/* The empty string is not valid JSON (unlike json_decode, which returns NULL
		 * silently, json_validate must record the syntax error) */
		pVm->json_rc = JSON_ERROR_SYNTAX;
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){
		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */
		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);
		/* Clear the json error state before validating $depth (php parity), so a
		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */
		pVm->json_rc = JSON_ERROR_NONE;
		if( nWant <= 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"json_validate(): Argument #2 ($depth) must be greater than 0");
		}
		if( nWant > 2147483647 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"json_validate(): Argument #2 ($depth) must be less than 2147483647");
		}
		nDepth = (int)nWant;
	}
	/* apArg[2] ($flags) is accepted and ignored: no decode flag is implemented.
	 * Decode in associative mode so the "objects are returned as an array" warning is
	 * not raised - the decoded value is discarded, only its validity matters. */
	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth) == JSON_ERROR_NONE);
	return PH7_OK;
}
