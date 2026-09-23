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
static int VmJsonObjectEncode(const SyString *pAttr,ph7_value *pValue,void *pUserData);
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
	int fail;          /* True if the value is unencodable — json_encode returns
	                    * FALSE (or throws under JSON_THROW_ON_ERROR) */
	int failRc;        /* json_rc to report for a ->fail (INF_OR_NAN vs
	                    * NON_BACKED_ENUM) */
	ph7_int64 nMaxDepth; /* json_encode's $depth: a container may only OPEN while
	                      * fewer than this many containers enclose it. php runs no
	                      * range screen here — 0 or a negative value simply makes
	                      * every container JSON_ERROR_DEPTH. */
	SySet aPath;       /* Containers on the current encode path (ph7_hashmap* /
	                    * ph7_class_instance*), pushed on entry and popped on exit:
	                    * meeting one again below itself is php's
	                    * JSON_ERROR_RECURSION, not an infinite descent. */
};
/*
 * Stack-safety ceiling on the EFFECTIVE json depth, both directions. php honors
 * $depth up to INT_MAX and relies on a dynamic guard that asks the platform how
 * much C stack is left; PHL's encoder and decoder recurse on the same C stack
 * with no such probe, so a requested depth above this bound is clamped to it and
 * a value/document nested deeper reports JSON_ERROR_DEPTH — loud, like the
 * SERIALIZE_MAX_DEPTH and HTTP_QUERY_MAX_DEPTH bounds this follows. 4096 is
 * eight times php's default of 512 and holds ~0.5MB of frames on MSVC's 1MB
 * default stack (measured ~10x smaller on glibc's 8MB). Ports with small stacks
 * (ESP32 task stacks are KBs) can override it at build time.
 */
#ifndef PH7_JSON_DEPTH_CEILING
#define PH7_JSON_DEPTH_CEILING 4096
#endif
/*
 * True if pPtr (a hashmap or class instance) is a container the encoder is
 * currently INSIDE of. Only the active path is searched, so a value appearing
 * twice as SIBLINGS ([$a,$a]) stays legal like php — recursion means
 * self-containment, not sharing.
 */
static int VmJsonPathHolds(json_private_data *pData,void *pPtr)
{
	void **apEntry = (void **)SySetBasePtr(&pData->aPath);
	sxu32 i,n = SySetUsed(&pData->aPath);
	for( i = 0 ; i < n ; ++i ){
		if( apEntry[i] == pPtr ){
			return 1;
		}
	}
	return 0;
}
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
 * 1.0e+17 where serialize prints 1.0E+17). Under JSON_PRESERVE_ZERO_FRACTION a
 * value whose shortest form carries no '.' or exponent gets ".0" appended, so
 * 1.0 stays a FLOAT on the round trip ("1.0", "-0.0") the way php keeps it.
 */
static sxi32 VmJsonEmitReal(ph7_context *pCtx,double rVal,int iFlags)
{
	SyBlob sNum;
	char *z;
	sxu32 i,n;
	sxi32 rc;
	int bFrac = 0;
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
		if( z[i] == 'e' || z[i] == '.' ){
			bFrac = 1;
		}
	}
	if( !bFrac && (iFlags & JSON_PRESERVE_ZERO_FRACTION) != 0 ){
		if( SyBlobAppend(&sNum,".0",2) != SXRET_OK ){
			SyBlobRelease(&sNum);
			return SXERR_MEM;
		}
		z = (char *)SyBlobData(&sNum);
		n = SyBlobLength(&sNum);
	}
	rc = ph7_result_string(pCtx,(const char *)z,(int)n);
	SyBlobRelease(&sNum);
	return rc;
}
/*
 * JSON_PRETTY_PRINT helper: emit a newline followed by (depth * 4) spaces, so a
 * container's members are laid out one-per-line and indented like php. A no-op
 * unless JSON_PRETTY_PRINT is set. Returns SXRET_OK or an OOM status; callers
 * wrap it in JSON_EMIT so an allocation failure trips the ->oom rail.
 */
static sxi32 VmJsonPretty(json_private_data *pJson,int depth)
{
	ph7_context *pCtx = pJson->pCtx;
	sxi32 rc;
	int i;
	if( (pJson->iFlags & JSON_PRETTY_PRINT) == 0 ){
		return SXRET_OK;
	}
	rc = ph7_result_string(pCtx,"\n",(int)sizeof(char));
	for( i = 0 ; i < depth && rc == SXRET_OK ; ++i ){
		rc = ph7_result_string(pCtx,"    ",(int)sizeof("    ")-1);
	}
	return rc;
}
/*
 * Byte length of the ill-formed UTF-8 run at z[0..n-1] as php's JSON encoder
 * measures it: a byte that could LEAD a sequence (C2..F4) swallows every
 * following byte that is merely continuation-SHAPED (10xxxxxx), up to the
 * length its lead announces, and the whole prefix is ONE error. So "\xed\xa0\x80"
 * (a surrogate) is a single JSON_ERROR_UTF8 / a single U+FFFD, while
 * "\xf5\x80\x80\x80" is four — F5 leads nothing, so each byte fails alone.
 *
 * php's mbstring measures the same runs with the STRICTER per-lead ranges
 * (builtin_mb.c's MbUtf8BadLen), which is why the two disagree on a surrogate:
 * mb_strtolower("\xed\xa0\x80") is "???" while json substitutes one U+FFFD.
 * Two php decoders, two rules — each matched where it belongs.
 */
static sxu32 VmJsonBadUtf8Len(const unsigned char *z,sxu32 n)
{
	sxu32 c = z[0],need,i;
	if( c >= 0xC2 && c <= 0xDF ){
		need = 2;
	}else if( c >= 0xE0 && c <= 0xEF ){
		need = 3;
	}else if( c >= 0xF0 && c <= 0xF4 ){
		need = 4;
	}else{
		return 1; /* 80..C1 or F5..FF: leads nothing */
	}
	for( i = 1 ; i < need && i < n && (z[i] & 0xC0) == 0x80 ; ++i ){}
	return i;
}
/*
 * Emit one code point as php's \uXXXX escape (lowercase hex), spelling anything
 * outside the BMP as the UTF-16 surrogate pair JSON has no other way to carry:
 * U+1F600 is "😀", exactly like php.
 */
static sxi32 VmJsonEmitUnicodeEscape(ph7_context *pCtx,sxu32 cp)
{
	static const char zHex[] = "0123456789abcdef";
	sxu32 aUnit[2];
	int nUnit,i;
	char zEsc[12];
	if( cp >= 0x10000 ){
		sxu32 v = cp - 0x10000;
		aUnit[0] = 0xD800 + (v >> 10);
		aUnit[1] = 0xDC00 + (v & 0x3FF);
		nUnit = 2;
	}else{
		aUnit[0] = cp;
		nUnit = 1;
	}
	for( i = 0 ; i < nUnit ; ++i ){
		zEsc[i*6 + 0] = '\\';
		zEsc[i*6 + 1] = 'u';
		zEsc[i*6 + 2] = zHex[(aUnit[i] >> 12) & 0x0F];
		zEsc[i*6 + 3] = zHex[(aUnit[i] >>  8) & 0x0F];
		zEsc[i*6 + 4] = zHex[(aUnit[i] >>  4) & 0x0F];
		zEsc[i*6 + 5] = zHex[ aUnit[i]        & 0x0F];
	}
	return ph7_result_string(pCtx,zEsc,nUnit * 6);
}
/*
 * Emit one JSON string literal — the opening quote, the escaped body, the
 * closing quote. Shared by the string VALUE path and by both KEY paths (array
 * keys and object property names), which used to append their bytes raw: a key
 * carrying a '"', a backslash or a control character produced UNPARSEABLE
 * output (php: json_encode(["a\"b"=>1]) is {"a\"b":1}, PHL emitted {"a"b":1}).
 * Everything php escapes in a string it escapes in a key, the JSON_HEX_*
 * and JSON_UNESCAPED_SLASHES flags included.
 *
 * Non-ASCII is escaped as \uXXXX by DEFAULT, which is what php does and what
 * JSON_UNESCAPED_UNICODE turns off — PHL used to emit the raw UTF-8 bytes
 * unconditionally, i.e. behave as if that flag were always set (the flag was
 * defined but never read). Even with it set php still escapes U+2028/U+2029,
 * the two line terminators JavaScript's eval() chokes on, unless
 * JSON_UNESCAPED_LINE_TERMINATORS is set too.
 *
 * bKey selects JSON_PARTIAL_OUTPUT_ON_ERROR's substitute for an ill-formed
 * string: php replaces a VALUE with null and a KEY (array key or property
 * name) with "" — the verdict must land before anything is emitted, because
 * the replacement covers the WHOLE string, not the tail after the bad byte.
 */
static int VmJsonStrHasBadUtf8(const char *zIn,int nByte)
{
	const unsigned char *z = (const unsigned char *)zIn,*zEnd = (const unsigned char *)&zIn[nByte];
	sxu32 nLen;
	while( z < zEnd ){
		if( z[0] < 0x80 ){
			z++;
			continue;
		}
		if( PH7_Utf8ReadStrict(z,(sxu32)(zEnd - z),&nLen) < 0 ){
			return 1;
		}
		z += nLen;
	}
	return 0;
}
static sxi32 VmJsonEncodeString(json_private_data *pData,const char *zIn,int nByte,int bKey)
{
	ph7_context *pCtx = pData->pCtx;
	int iFlags = pData->iFlags;
	const char *zEnd = &zIn[nByte];
	sxi32 rc;
	char c;
	if( (iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR) != 0
	 && (iFlags & (JSON_INVALID_UTF8_IGNORE|JSON_INVALID_UTF8_SUBSTITUTE)) == 0
	 && VmJsonStrHasBadUtf8(zIn,nByte) ){
		pCtx->pVm->json_rc = JSON_ERROR_UTF8;
		return bKey ? ph7_result_string(pCtx,"\"\"",2)
		            : ph7_result_string(pCtx,"null",(int)sizeof("null")-1);
	}
	rc = ph7_result_string(pCtx,"\"",(int)sizeof(char));
	if( rc != SXRET_OK ){
		return rc;
	}
	for(;;){
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		if( (unsigned char)zIn[0] >= 0x80 ){
			/* A UTF-8 sequence: decode it strictly, since \uXXXX needs the code
			 * point and not the bytes. */
			sxu32 nLen,cp;
			sxi32 iCp = PH7_Utf8ReadStrict((const unsigned char *)zIn,(sxu32)(zEnd - zIn),&nLen);
			if( iCp < 0 ){
				/* Ill-formed. php REFUSES to encode it: json_encode returns
				 * false with json_last_error() == JSON_ERROR_UTF8, because
				 * there is no honest JSON spelling for a byte that is not
				 * text. PHL used to pass the byte through, so the caller got a
				 * valid-looking payload php would never have produced and no
				 * error check could see it. The two JSON_INVALID_UTF8_* flags
				 * are the opt-outs php offers. */
				nLen = VmJsonBadUtf8Len((const unsigned char *)zIn,(sxu32)(zEnd - zIn));
				if( iFlags & JSON_INVALID_UTF8_IGNORE ){
					rc = SXRET_OK; /* drop the run */
				}else if( iFlags & JSON_INVALID_UTF8_SUBSTITUTE ){
					rc = (iFlags & JSON_UNESCAPED_UNICODE)
						? ph7_result_string(pCtx,"\357\277\275",3) /* U+FFFD */
						: VmJsonEmitUnicodeEscape(pCtx,0xFFFD);
				}else{
					pData->fail = 1;
					pData->failRc = JSON_ERROR_UTF8;
					return SXRET_OK; /* the whole encode is discarded */
				}
			}else{
				cp = (sxu32)iCp;
				if( (iFlags & JSON_UNESCAPED_UNICODE) == 0
				 || ((cp == 0x2028 || cp == 0x2029)
				  && (iFlags & JSON_UNESCAPED_LINE_TERMINATORS) == 0) ){
					rc = VmJsonEmitUnicodeEscape(pCtx,cp);
				}else{
					rc = ph7_result_string(pCtx,zIn,(int)nLen);
				}
			}
			zIn += nLen;
			if( rc != SXRET_OK ){
				return rc;
			}
			continue;
		}
		c = zIn[0];
		/* Advance the stream cursor */
		zIn++;
		if( (c == '<' || c == '>') && (iFlags & JSON_HEX_TAG) ){
			/* All < and > are converted to \u003C and \u003E */
			if( c == '<' ){
				rc = ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);
			}else{
				rc = ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);
			}
		}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){
			/* All &s are converted to \u0026.  */
			rc = ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);
		}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){
			/* All ' are converted to \u0027.   */
			rc = ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);
		}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){
			/* All " are converted to \u0022. */
			rc = ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);
		}else if( (unsigned char)c < 0x20 ){
			/* Control characters (band A #4): php emits the short escapes for
			 * \b \f \n \r \t and \u00xx for the rest — pre-fix these were
			 * emitted RAW (invalid JSON). */
			static const char zHex[] = "0123456789abcdef";
			char zEsc[6] = { '\\', 'u', '0', '0', 0, 0 };
			switch(c){
			case '\b': rc = ph7_result_string(pCtx,"\\b",2); break;
			case '\f': rc = ph7_result_string(pCtx,"\\f",2); break;
			case '\n': rc = ph7_result_string(pCtx,"\\n",2); break;
			case '\r': rc = ph7_result_string(pCtx,"\\r",2); break;
			case '\t': rc = ph7_result_string(pCtx,"\\t",2); break;
			default:
				zEsc[4] = zHex[(c >> 4) & 0x0F];
				zEsc[5] = zHex[c & 0x0F];
				rc = ph7_result_string(pCtx,zEsc,6);
				break;
			}
		}else{
			if( c == '"' || c == '\\' ){
				/* Escape the quote/backslash (php escapes the backslash
				 * unconditionally — the old code wrongly tied it to
				 * JSON_UNESCAPED_SLASHES, which governs '/' below) */
				rc = ph7_result_string(pCtx,"\\",(int)sizeof(char));
			}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){
				/* php escapes forward slashes by default */
				rc = ph7_result_string(pCtx,"\\",(int)sizeof(char));
			}else{
				rc = SXRET_OK;
			}
			if( rc == SXRET_OK ){
				/* Append character verbatim */
				rc = ph7_result_string(pCtx,&c,(int)sizeof(char));
			}
		}
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	return ph7_result_string(pCtx,"\"",(int)sizeof(char));
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
/*
 * Encode a native class's PRESENTED shape, php's get_properties handler answering
 * the JSON purpose. Answers 0 when the class declares no hook, so the caller falls
 * through to the ordinary property walk.
 *
 * php emits an OBJECT here whatever the presented keys look like — an ArrayObject
 * holding a plain list is `{"0":1,"1":2}`, never `[1,2]` — so the list test the
 * array arm makes is deliberately not made.
 */
static int VmJsonPresent(ph7_class_instance *pThis,json_private_data *pData)
{
	ph7_context *pCtx = pData->pCtx;
	ph7_value sPresent;
	int savedObject;
	PH7_MemObjInit(pThis->pVm,&sPresent);
	if( PH7_MemObjToHashmap(&sPresent) != SXRET_OK ){
		PH7_MemObjRelease(&sPresent);
		return 0;
	}
	if( !PH7_ClassInstancePresent(pThis,&sPresent,0) ){
		PH7_MemObjRelease(&sPresent);
		return 0;
	}
	savedObject = pData->isObject;
	pData->isObject = 1;
	pData->isFirst = 1;
	JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));
	ph7_array_walk(&sPresent,VmJsonArrayEncode,pData);
	if( !pData->oom ){
		if( !pData->isFirst ){
			JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));
		}
		JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));
	}
	pData->isObject = savedObject;
	PH7_MemObjRelease(&sPresent);
	return 1;
}
static sxi32 VmJsonEncode(
	ph7_value *pIn,          /* Encode this value */
	json_private_data *pData /* Context data */
	){
		ph7_context *pCtx = pData->pCtx;
		int iFlags = pData->iFlags;
		int nByte;
		if( ph7_value_is_resource(pIn) ){
			/* php: a resource has no JSON representation — the whole encode
			 * fails with JSON_ERROR_UNSUPPORTED_TYPE (PHL used to emit "null"
			 * in silence, an answer php never gives). Under
			 * JSON_PARTIAL_OUTPUT_ON_ERROR the substitute IS null, with the
			 * error recorded. */
			if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){
				pCtx->pVm->json_rc = JSON_ERROR_UNSUPPORTED_TYPE;
				JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));
				return PH7_OK;
			}
			pData->fail = 1;
			pData->failRc = JSON_ERROR_UNSUPPORTED_TYPE;
			return PH7_OK;
		}else if( ph7_value_is_null(pIn) ){
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
				double rVal = ph7_value_to_double(pIn);
				/* php rejects Inf/NaN: json_encode returns FALSE with
				 * json_last_error() == JSON_ERROR_INF_OR_NAN (they have no JSON
				 * representation), instead of emitting the invalid bare token. */
				if( PH7_IS_NAN(rVal) || PH7_IS_INF(rVal) ){
					if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){
						/* php's substitute for an Inf/NaN member is 0 */
						pCtx->pVm->json_rc = JSON_ERROR_INF_OR_NAN;
						JSON_EMIT(pData,ph7_result_string(pCtx,"0",(int)sizeof(char)));
						return PH7_OK;
					}
					pData->fail = 1;
					pData->failRc = JSON_ERROR_INF_OR_NAN;
					return PH7_OK;
				}
				/* php's json float output follows serialize_precision
				 * (shortest round-trip, like serialize/var_export), NOT the
				 * echo/cast precision of 14 — with a lowercase exponent
				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,
				 * 1.0 -> 1, -0.0 -> -0. */
				JSON_EMIT(pData,VmJsonEmitReal(pCtx,rVal,iFlags));
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
				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn),iFlags));
			}else{
				const char *zIn;
				/* Encode the string */
				zIn = ph7_value_to_string(pIn,&nByte);
				JSON_EMIT(pData,VmJsonEncodeString(pData,zIn,nByte,0));
			}
		}else if( ph7_value_is_array(pIn) ){
			/* An array encodes as a JSON array iff it is a "list" [consecutive
			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an
			 * object with stringified keys (PHP semantics). */
			ph7_hashmap *pMap = (ph7_hashmap *)pIn->x.pOther;
			int isObject = (iFlags & JSON_FORCE_OBJECT)
				|| !PH7_HashmapIsList(pMap);
			int savedObject = pData->isObject; /* restore for sibling entries after recursion */
			int c = isObject ? '{' : '[';
			int d = isObject ? '}' : ']';
			/* An array the encoder is already inside of (reached through a
			 * reference cycle) is php's JSON_ERROR_RECURSION — PHL used to
			 * descend into it and answer a TRUNCATED nesting in silence.
			 * JSON_PARTIAL_OUTPUT_ON_ERROR substitutes null for the cycle. */
			if( VmJsonPathHolds(pData,(void *)pMap) ){
				if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){
					pCtx->pVm->json_rc = JSON_ERROR_RECURSION;
					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));
					return PH7_OK;
				}
				pData->fail = 1;
				pData->failRc = JSON_ERROR_RECURSION;
				return PH7_OK;
			}
			/* php checks $depth where a container OPENS: one already enclosed
			 * by $depth containers (scalars are exempt) is JSON_ERROR_DEPTH.
			 * Under JSON_PARTIAL_OUTPUT_ON_ERROR php records the error and
			 * keeps ENCODING past the limit — PHL follows until the stack
			 * ceiling, where a null stands in for what it will not recurse
			 * into. */
			if( (ph7_int64)pData->nRecCount >= pData->nMaxDepth ){
				if( (iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR) == 0 ){
					pData->fail = 1;
					pData->failRc = JSON_ERROR_DEPTH;
					return PH7_OK;
				}
				pCtx->pVm->json_rc = JSON_ERROR_DEPTH;
				if( pData->nRecCount >= PH7_JSON_DEPTH_CEILING ){
					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));
					return PH7_OK;
				}
			}
			if( SySetPut(&pData->aPath,(const void *)&pMap) != SXRET_OK ){
				pData->oom = 1;
				return PH7_OK;
			}
			/* Encode the array */
			pData->isObject = isObject;
			pData->isFirst = 1;
			/* Append the square bracket or curly braces */
			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));
			/* Iterate throw array entries */
			ph7_array_walk(pIn,VmJsonArrayEncode,pData);
			(void)SySetPop(&pData->aPath);
			/* Bail if a nested append ran out of memory before the closer */
			if( pData->oom ){
				return PH7_OK;
			}
			/* Pretty-print: a non-empty container closes on its own line,
			 * indented one level less than its members (isFirst is still 1
			 * only when no entry was emitted -> keep "[]"/"{}" tight). */
			if( !pData->isFirst ){
				JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));
			}
			/* Append the closing square bracket or curly braces */
			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));
			pData->isObject = savedObject;
		}else if( ph7_value_is_object(pIn) ){
			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;
			ph7_vm *pVm = pIn->pVm;
			ph7_class_method *pMethod = 0;
			int bProps = 1; /* encode the property view below (cleared when a
			                 * jsonSerialize() result replaces it) */
			/* An object the encoder is already inside of is php's
			 * JSON_ERROR_RECURSION, checked BEFORE the jsonSerialize dispatch
			 * (PHL used to re-dispatch until the C stack ran out — a segfault
			 * on `return $this;`). */
			if( VmJsonPathHolds(pData,(void *)pThis) ){
				if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){
					pCtx->pVm->json_rc = JSON_ERROR_RECURSION;
					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));
					return PH7_OK;
				}
				pData->fail = 1;
				pData->failRc = JSON_ERROR_RECURSION;
				return PH7_OK;
			}
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
				}else if( iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR ){
					/* php's substitute for a non-backed case is 0 */
					pCtx->pVm->json_rc = JSON_ERROR_NON_BACKED_ENUM;
					JSON_EMIT(pData,ph7_result_string(pCtx,"0",(int)sizeof(char)));
				}else{
					pData->fail = 1;
					pData->failRc = JSON_ERROR_NON_BACKED_ENUM;
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
				if( ph7_value_is_object(&sResult)
				 && (ph7_class_instance *)sResult.x.pOther == pThis ){
					/* php's one self-reference exception: jsonSerialize()
					 * returning $this encodes the object's own property view —
					 * no re-dispatch, no recursion error. */
					PH7_MemObjRelease(&sResult);
				}else{
					bProps = 0;
					/* Encode the returned value [scalar/array/object]. The
					 * object stays ON the path while its replacement encodes
					 * (`return [$this]` is php's recursion error), and the
					 * result sits at the object's own nesting level — the old
					 * nRecCount++ here indented a JSON_PRETTY_PRINT result one
					 * level deeper than php and would have charged $depth for a
					 * container php does not charge. */
					if( SySetPut(&pData->aPath,(const void *)&pThis) != SXRET_OK ){
						PH7_MemObjRelease(&sResult);
						pData->oom = 1;
						return PH7_OK;
					}
					VmJsonEncode(&sResult,pData);
					(void)SySetPop(&pData->aPath);
					PH7_MemObjRelease(&sResult);
					if( pData->exc ){
						return PH7_EXCEPTION;
					}
					if( pData->oom ){
						return PH7_OK;
					}
				}
			}
			/* php checks $depth where a container OPENS — the '{' of the
			 * property view below; a SCALAR jsonSerialize() result and an enum
			 * backing value are exempt, so the check sits here and not at the
			 * arm's entry. The PARTIAL_OUTPUT rule mirrors the array arm's:
			 * record the error, keep encoding, null at the stack ceiling. */
			if( bProps && (ph7_int64)pData->nRecCount >= pData->nMaxDepth ){
				if( (iFlags & JSON_PARTIAL_OUTPUT_ON_ERROR) == 0 ){
					pData->fail = 1;
					pData->failRc = JSON_ERROR_DEPTH;
					return PH7_OK;
				}
				pCtx->pVm->json_rc = JSON_ERROR_DEPTH;
				if( pData->nRecCount >= PH7_JSON_DEPTH_CEILING ){
					JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));
					return PH7_OK;
				}
			}
			if( bProps && SySetPut(&pData->aPath,(const void *)&pThis) != SXRET_OK ){
				pData->oom = 1;
				return PH7_OK;
			}
			if( !bProps ){
				/* jsonSerialize()'s result replaced the property view above */
			}else if( VmJsonPresent(pThis,pData) ){
				/* A native class with php's get_properties handler: json is one of
				 * the purposes that handler serves (php's ZEND_PROP_PURPOSE_JSON),
				 * so a DateTime encodes as date/timezone_type/timezone and an
				 * ArrayObject as its ELEMENTS — where walking the real slots below
				 * finds nothing, every one of them being a hidden engine slot.
				 * Handled inside VmJsonPresent so this arm is just the dispatch. */
				if( pData->exc ){
					return PH7_EXCEPTION;
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
					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_HIDDEN))
					 || pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){
						continue;
					}
					if( SyStringLength(&pVmAttr->pAttr->sName) > 0
					 && SyStringData(&pVmAttr->pAttr->sName)[0] == 0 ){
						/* A MANGLED key stored raw (the __PHP_Incomplete_Class
						 * carrier): php's json encoder reads it as non-public
						 * and skips it. */
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
						VmJsonObjectEncode(&pVmAttr->pAttr->sName,pAttrVal,pData);
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
				/* Pretty-print: non-empty object closes on its own indented line. */
				if( !pData->isFirst ){
					JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));
				}
				/* Append the closing curly braces  */
				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));
			}
			if( bProps ){
				(void)SySetPop(&pData->aPath);
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
	if( pJson->exc || pJson->oom || pJson->fail ){
		/* A callback threw, OOM, or the value is unencodable (the result is
		 * discarded) — return immediately. Depth is no longer decided here:
		 * the container arms enforce json_encode's $depth where a '['/'{'
		 * opens (the old flat 31 cap TRUNCATED a deep value in silence). */
		return PH7_OK;
	}
	if( !pJson->isFirst ){
		/* Append the comma separating this entry from the previous one */
		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));
	}
	/* Pretty-print: every member starts on its own indented line (one level
	 * deeper than the enclosing container). */
	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));
	if( pJson->isObject ){
		/* Outputs an object rather than an array */
		const char *zKey;
		int nByte;
		/* Extract a string representation of the key */
		zKey = ph7_value_to_string(pKey,&nByte);
		/* Append the quoted key and the colon. The key goes through the same
		 * escaper as a string VALUE (php escapes both identically): emitting it
		 * raw produced invalid JSON for any key holding '"', '\' or a control
		 * character. */
		JSON_EMIT(pJson,VmJsonEncodeString(pJson,zKey,nByte,1));
		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,":",(int)sizeof(char)));
		/* php puts a space after the colon in pretty mode */
		if( pJson->iFlags & JSON_PRETTY_PRINT ){
			JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));
		}
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
static int VmJsonObjectEncode(const SyString *pAttr,ph7_value *pValue,void *pUserData)
{
	json_private_data *pJson = (json_private_data *)pUserData;
	if( pJson->exc || pJson->oom || pJson->fail ){
		/* A callback threw, OOM, or the value is unencodable (the result is
		 * discarded) — return immediately. Depth is no longer decided here:
		 * the container arms enforce json_encode's $depth where a '['/'{'
		 * opens (the old flat 31 cap TRUNCATED a deep value in silence). */
		return PH7_OK;
	}
	if( !pJson->isFirst ){
		/* Append the comma separating this entry from the previous one */
		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));
	}
	/* Pretty-print: member on its own indented line, one level deeper. */
	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));
	/* Append the quoted attribute name and the colon — escaped like a string
	 * value, same as the array-key path above. */
	JSON_EMIT(pJson,VmJsonEncodeString(pJson,SyStringData(pAttr),(int)SyStringLength(pAttr),1));
	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,":",(int)sizeof(char)));
	/* php puts a space after the colon in pretty mode */
	if( pJson->iFlags & JSON_PRETTY_PRINT ){
		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));
	}
	/* Encode the value */
	pJson->nRecCount++;
	VmJsonEncode(pValue,pJson);
	pJson->nRecCount--;
	pJson->isFirst = 0;
	return PH7_OK;
}
/*
 * string json_encode(mixed $value [, int $flags = 0 [, int $depth = 512 ]])
 *  Returns a string containing the JSON representation of value.
 * Parameters
 *  $value
 *  The value being encoded. Can be any type except a resource
 *  (a resource is JSON_ERROR_UNSUPPORTED_TYPE).
 * $options
 *  Bitmask consisting of:
 *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.
 *  JSON_HEX_AMP   All &s are converted to \u0026.
 *  JSON_HEX_APOS  All ' are converted to \u0027.
 *  JSON_HEX_QUOT  All " are converted to \u0022.
 *  JSON_FORCE_OBJECT  Outputs an object rather than an array.
 *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.
 *  JSON_BIGINT_AS_STRING   Decode flag (large ints as strings), not an encode flag.
 *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.
 *  JSON_UNESCAPED_SLASHES  Don't escape '/'
 *  JSON_UNESCAPED_UNICODE  Not used.
 * Return
 *  Returns a JSON encoded string on success. FALSE otherwise
 */
static const char * JsonErrorMsg(int rc); /* defined below, near json_last_error_msg */
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
	sJson.failRc = JSON_ERROR_NON_BACKED_ENUM;
	sJson.nMaxDepth = 512; /* php's default */
	SySetInit(&sJson.aPath,&pCtx->pVm->sAllocator,sizeof(void *));
	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){
		/* Extract option flags */
		sJson.iFlags = ph7_value_to_int(apArg[1]);
	}
	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){
		/* $depth. Unlike json_decode's, php runs NO range screen here: 0 or a
		 * negative value simply makes every container JSON_ERROR_DEPTH, and any
		 * large int is accepted (the type screen has already run). */
		sJson.nMaxDepth = ph7_value_to_int64(apArg[2]);
		if( sJson.nMaxDepth > PH7_JSON_DEPTH_CEILING ){
			/* Engine stack-safety bound (see PH7_JSON_DEPTH_CEILING). */
			sJson.nMaxDepth = PH7_JSON_DEPTH_CEILING;
		}
	}
	pCtx->pVm->json_rc = JSON_ERROR_NONE;
	/* Perform the encoding operation */
	rc = VmJsonEncode(apArg[0],&sJson);
	SySetRelease(&sJson.aPath);
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
		/* Unencodable value (Inf/NaN, or a php 8.1 non-backed enum case): the
		 * whole encode fails — discard whatever was emitted and return FALSE. */
		pCtx->pVm->json_rc = sJson.failRc;
		if( sJson.iFlags & JSON_THROW_ON_ERROR ){
			/* php: raise a JsonException carrying json_last_error_msg() instead
			 * of returning FALSE. */
			return PH7_VmThrowExceptionCode(pCtx,"JsonException",
				(sxi32)pCtx->pVm->json_rc,"%s",
				JsonErrorMsg(pCtx->pVm->json_rc));
		}
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
/* Human-readable message for a json_rc code. Shared by json_last_error_msg()
 * and the JSON_THROW_ON_ERROR path (php's JsonException message is exactly this
 * text). */
static const char * JsonErrorMsg(int rc)
{
	switch( rc ){
	case JSON_ERROR_NONE:            return "No error";
	case JSON_ERROR_DEPTH:           return "Maximum stack depth exceeded";
	case JSON_ERROR_STATE_MISMATCH:  return "State mismatch (invalid or malformed JSON)";
	case JSON_ERROR_CTRL_CHAR:       return "Control character error, possibly incorrectly encoded";
	case JSON_ERROR_SYNTAX:          return "Syntax error";
	case JSON_ERROR_UTF8:            return "Malformed UTF-8 characters, possibly incorrectly encoded";
	case JSON_ERROR_RECURSION:       return "Recursion detected";
	case JSON_ERROR_INF_OR_NAN:     return "Inf and NaN cannot be JSON encoded";
	case JSON_ERROR_UNSUPPORTED_TYPE: return "Type is not supported";
	case JSON_ERROR_INVALID_PROPERTY_NAME: return "The decoded property name is invalid";
	case JSON_ERROR_UTF16:           return "Single unpaired UTF-16 surrogate in unicode escape";
	case JSON_ERROR_NON_BACKED_ENUM: return "Non-backed enums have no default serialization";
	default:                         return "Unknown error";
	}
}
PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_result_string(pCtx,JsonErrorMsg(pCtx->pVm->json_rc),-1/* auto length */);
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
		/* Delimit the string. The backslash state is tracked explicitly: the old
		 * "the previous byte is not a backslash" test mis-read an ESCAPED
		 * backslash sitting before the closing quote, so the perfectly valid
		 * "\\" (a one-character string holding a backslash) was reported as an
		 * unterminated string — json_decode('"\\\\"') answered NULL with a
		 * syntax error where php answers "\". */
		while( pStream->zText < pStream->zEnd ){
			if( pStream->zText[0] == '\\' ){
				/* Whatever follows belongs to the escape, closing quote
				 * included; VmJsonDequoteString below decides if it is legal. */
				pStream->zText++;
				if( pStream->zText >= pStream->zEnd ){
					break;
				}
				pStream->zText++;
				continue;
			}
			if( pStream->zText[0] == '"' ){
				break;
			}
			if( (unsigned char)pStream->zText[0] < 0x20 ){
				/* php: a control character must be escaped inside a JSON string;
				 * a raw one is JSON_ERROR_CTRL_CHAR (a literal newline included). */
				pToken->nType = JSON_TK_INVALID;
				*pJsonErr = JSON_ERROR_CTRL_CHAR;
				return SXERR_ABORT;
			}
			pStream->zText++;
		}
		if( pStream->zText >= pStream->zEnd ){
			/* Missing closing '"'. php reports this as JSON_ERROR_CTRL_CHAR, not
			 * a syntax error: its scanner runs the string off the end of the
			 * input and lands in the same state an unescaped control character
			 * puts it in. */
			pToken->nType = JSON_TK_INVALID;
			*pJsonErr = JSON_ERROR_CTRL_CHAR;
		}else{
			pToken->nType = JSON_TK_STR;
			pStream->zText++; /* Jump the closing double quotes */
		}
	}else if( (pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]))
		|| (pStream->zText[0] == '-' && &pStream->zText[1] < pStream->zEnd
			&& pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1])) ){
		/* Number, held to JSON's grammar:
		 *   -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
		 * The old scanner took any digit soup, so "01", "-01", "5." and "1e"
		 * all DECODED (and json_validate() answered TRUE) where php reports
		 * JSON_ERROR_SYNTAX — accepting documents no JSON producer emits. */
		int bBad = 0;
		if( pStream->zText[0] == '-' ){
			pStream->zText++;
		}
		if( pStream->zText[0] == '0' ){
			pStream->zText++;
			/* JSON forbids a leading zero ahead of another digit */
			if( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
				bBad = 1;
			}
		}
		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
			pStream->zText++;
		}
		if( pStream->zText < pStream->zEnd && pStream->zText[0] == '.' ){
			pStream->zText++;
			/* JSON requires at least one digit after the point */
			if( pStream->zText >= pStream->zEnd || pStream->zText[0] >= 0xc0 || !SyisDigit(pStream->zText[0]) ){
				bBad = 1;
			}
			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
				pStream->zText++;
			}
		}
		if( pStream->zText < pStream->zEnd && (pStream->zText[0] == 'e' || pStream->zText[0] == 'E') ){
			pStream->zText++;
			if( pStream->zText < pStream->zEnd && (pStream->zText[0] == '+' || pStream->zText[0] == '-') ){
				pStream->zText++;
			}
			/* ...and at least one digit in the exponent */
			if( pStream->zText >= pStream->zEnd || pStream->zText[0] >= 0xc0 || !SyisDigit(pStream->zText[0]) ){
				bBad = 1;
			}
			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
				pStream->zText++;
			}
		}
		if( bBad ){
			pToken->nType = JSON_TK_INVALID;
			*pJsonErr = JSON_ERROR_SYNTAX;
			return SXERR_ABORT;
		}
		pToken->nType = JSON_TK_NUM;
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
		/* Unexpected token — but a byte that is not valid UTF-8 is php's
		 * JSON_ERROR_UTF8, not a syntax error, wherever in the document it sits
		 * (a valid non-ASCII character outside a string stays a syntax error).
		 * The JSON_INVALID_UTF8_* flags do NOT reach here: php applies them
		 * inside string tokens only. */
		sxu32 nLen;
		pToken->nType = JSON_TK_INVALID;
		*pJsonErr = ((unsigned char)pStream->zText[0] >= 0x80
			&& PH7_Utf8ReadStrict((const unsigned char *)pStream->zText,
				(sxu32)(pStream->zEnd - pStream->zText),&nLen) < 0)
			? JSON_ERROR_UTF8 : JSON_ERROR_SYNTAX;
		/* Advance the stream cursor */
		pStream->zText++;
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
	int iUserFlags;    /* json_decode()'s own $flags (JSON_INVALID_UTF8_* live here) */
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
 * Read the four hex digits of a \uXXXX escape out of z[0..n-1]. Returns
 * SXRET_OK and the value, or SXERR_SYNTAX when fewer than four are there or one
 * is not a hex digit (php: JSON_ERROR_SYNTAX).
 */
static sxi32 VmJsonHex4(const char *z,sxu32 n,sxu32 *pVal)
{
	sxu32 v = 0;
	int i;
	if( n < 4 ){
		return SXERR_SYNTAX;
	}
	for( i = 0 ; i < 4 ; ++i ){
		int c = (unsigned char)z[i];
		if( c >= '0' && c <= '9' ){
			v = (v << 4) | (sxu32)(c - '0');
		}else if( c >= 'a' && c <= 'f' ){
			v = (v << 4) | (sxu32)(c - 'a' + 10);
		}else if( c >= 'A' && c <= 'F' ){
			v = (v << 4) | (sxu32)(c - 'A' + 10);
		}else{
			return SXERR_SYNTAX;
		}
	}
	*pVal = v;
	return SXRET_OK;
}
/*
 * Append one run of un-escaped string bytes, checking that it really is UTF-8:
 * php rejects a JSON document carrying a byte that is not text with
 * JSON_ERROR_UTF8, exactly as it refuses to ENCODE one. Only inside a string do
 * the JSON_INVALID_UTF8_* flags apply — a stray byte between tokens is an error
 * either way. Returns JSON_ERROR_NONE or JSON_ERROR_UTF8.
 */
static int VmJsonAppendChecked(ph7_value *pWorker,const char *zIn,sxu32 nByte,int iFlags)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,iRun = 0,nLen;
	while( i < nByte ){
		if( z[i] < 0x80 ){
			i++;
			continue;
		}
		if( PH7_Utf8ReadStrict(&z[i],nByte - i,&nLen) >= 0 ){
			i += nLen;
			continue;
		}
		if( (iFlags & (JSON_INVALID_UTF8_IGNORE|JSON_INVALID_UTF8_SUBSTITUTE)) == 0 ){
			return JSON_ERROR_UTF8;
		}
		/* With BOTH flags set php's decoder substitutes while its encoder drops
		 * (probed both ways); the order of these two tests is that asymmetry,
		 * not an oversight. */
		/* Flush what is good, then stand in for the run */
		if( i > iRun ){
			ph7_value_string(pWorker,&zIn[iRun],(int)(i - iRun));
		}
		nLen = VmJsonBadUtf8Len(&z[i],nByte - i);
		if( iFlags & JSON_INVALID_UTF8_SUBSTITUTE ){
			ph7_value_string(pWorker,"\357\277\275",3); /* U+FFFD */
		}
		i += nLen;
		iRun = i;
	}
	if( i > iRun ){
		ph7_value_string(pWorker,&zIn[iRun],(int)(i - iRun));
	}
	return JSON_ERROR_NONE;
}
/*
 * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store
 * the result in the given ph7_value. Returns JSON_ERROR_NONE, or the json_rc
 * php reports for the malformed escape it stopped on.
 *
 * The \uXXXX form used to fall through to the default branch, which dropped the
 * backslash and kept the rest as literal text: json_decode('"é"') answered
 * the five characters u00e9 instead of "é". \b was mangled the same way (it
 * answered "b"), and an escape JSON does not define (\q) was silently accepted
 * where php raises a syntax error.
 */
static int VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker,int iFlags)
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
			int rcChunk = VmJsonAppendChecked(pWorker,zCur,(sxu32)(zIn-zCur),iFlags);
			if( rcChunk != JSON_ERROR_NONE ){
				return rcChunk;
			}
		}
		zIn++;
		if( zIn >= zEnd ){
			/* End of the input reached */
			break;
		}
		c = zIn[0];
		/* Unescape the character */
		switch(c){
		case '"':  ph7_value_string(pWorker,"\"",(int)sizeof(char)); break;
		case '\\': ph7_value_string(pWorker,"\\",(int)sizeof(char)); break;
		case '/':  ph7_value_string(pWorker,"/",(int)sizeof(char)); break;
		case 'b':  ph7_value_string(pWorker,"\b",(int)sizeof(char)); break;
		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;
		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;
		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;
		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;
		case 'u': {
			/* \uXXXX, and the surrogate PAIR that is JSON's only way to spell a
			 * code point above the BMP. An unpaired half is php's
			 * JSON_ERROR_UTF16, distinct from a malformed escape. */
			unsigned char zUtf8[4];
			unsigned char *zW = zUtf8;
			sxu32 cp,cpLow = 0; /* cpLow pre-set: MSVC /W4 flags the short-circuit as a maybe-uninitialized read */
			if( VmJsonHex4(&zIn[1],(sxu32)(zEnd - zIn - 1),&cp) != SXRET_OK ){
				return JSON_ERROR_SYNTAX;
			}
			zIn += 4;
			if( cp >= 0xDC00 && cp <= 0xDFFF ){
				return JSON_ERROR_UTF16; /* a low half with no high half before it */
			}
			if( cp >= 0xD800 && cp <= 0xDBFF ){
				if( zEnd - zIn < 3 || zIn[1] != '\\' || zIn[2] != 'u'
				 || VmJsonHex4(&zIn[3],(sxu32)(zEnd - zIn - 3),&cpLow) != SXRET_OK
				 || cpLow < 0xDC00 || cpLow > 0xDFFF ){
					return JSON_ERROR_UTF16;
				}
				cp = 0x10000 + ((cp - 0xD800) << 10) + (cpLow - 0xDC00);
				zIn += 6;
			}
			SX_WRITE_UTF8(zW,cp);
			ph7_value_string(pWorker,(const char *)zUtf8,(int)(zW - zUtf8));
			break;
		}
		default:
			/* Not one of JSON's nine escapes */
			return JSON_ERROR_SYNTAX;
		}
		/* Advance the stream cursor */
		zIn++;
	}
	return JSON_ERROR_NONE;
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
	int rcQ;            /* VmJsonDequoteString() status */
	/* Nothing left to decode: the token stream is empty (a whitespace-only input
	 * tokenizes to NO tokens at all, so pIn/pEnd are both the NULL base pointer of an
	 * empty set) or a member value is missing after its colon ('{"a":'). Both are a
	 * syntax error for php; without this screen the reads below dereference pEnd. */
	if( pDecoder->pIn >= pDecoder->pEnd ){
		*pDecoder->pErr = JSON_ERROR_SYNTAX;
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
			if( (pDecoder->iUserFlags & JSON_BIGINT_AS_STRING) != 0
			 && ph7_value_is_float(pWorker) ){
				/* php: an INTEGER literal beyond int64 normally lands on a
				 * float; under JSON_BIGINT_AS_STRING it stays the EXACT source
				 * text as a string. Only integer SHAPES qualify — a '.', 'e'
				 * or 'E' anywhere means the document asked for the float. */
				sxu32 iCh;
				int bIntShape = 1;
				for( iCh = 0 ; iCh < pStr->nByte ; ++iCh ){
					if( pStr->zString[iCh] == '.' || pStr->zString[iCh] == 'e'
					 || pStr->zString[iCh] == 'E' ){
						bIntShape = 0;
						break;
					}
				}
				if( bIntShape ){
					ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);
				}
			}
		}else{
			/* Dequote the string */
			rcQ = VmJsonDequoteString(&pDecoder->pIn->sData,pWorker,pDecoder->iUserFlags);
			if( rcQ != JSON_ERROR_NONE ){
				*pDecoder->pErr = rcQ;
				return SXERR_ABORT;
			}
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
		/* php's $depth counts CONTAINERS: a '[' opening at 1-based nesting
		 * level L is JSON_ERROR_DEPTH when L >= $depth, an EMPTY container
		 * included ("[]" at $depth 1 already fails), while a scalar never
		 * consults $depth at all. rec_count holds L-1 here. */
		if( pDecoder->rec_count + 1 >= pDecoder->rec_depth ){
			*pDecoder->pErr = JSON_ERROR_DEPTH;
			return SXERR_ABORT;
		}
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
			if( pDecoder->pIn >= pDecoder->pEnd ){
				/* Ran out of tokens before the closing ']': php rejects an
				 * unterminated array as a syntax error. */
				*pDecoder->pErr = JSON_ERROR_SYNTAX;
				return SXERR_ABORT;
			}
			if( pDecoder->pIn->nType & JSON_TK_CSB /*']'*/ ){
				pDecoder->pIn++; /* Jump the trailing ']' */
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
		/* Same container rule as '[' above. */
		if( pDecoder->rec_count + 1 >= pDecoder->rec_depth ){
			*pDecoder->pErr = JSON_ERROR_DEPTH;
			return SXERR_ABORT;
		}
		/* Object representation*/
		pDecoder->pIn++;
		/* Decode into a working array first; unless the caller asked for
		 * associative arrays (assoc=true / JSON_OBJECT_AS_ARRAY), it is converted
		 * to a stdClass below so json_decode('{...}') returns an object like php. */
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
			if( pDecoder->pIn >= pDecoder->pEnd ){
				/* Ran out of tokens before the closing '}': php rejects an
				 * unterminated object as a syntax error. */
				*pDecoder->pErr = JSON_ERROR_SYNTAX;
				return SXERR_ABORT;
			}
			if( pDecoder->pIn->nType & JSON_TK_CCB /*'}'*/ ){
				pDecoder->pIn++; /* Jump the trailing '}' */
				break;
			}
			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 || &pDecoder->pIn[1] >= pDecoder->pEnd
				|| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){
					/* Syntax error,return immediately */
					*pDecoder->pErr = JSON_ERROR_SYNTAX;
					return SXERR_ABORT;
			}
			/* Dequote the key */
			rcQ = VmJsonDequoteString(&pDecoder->pIn->sData,pKey,pDecoder->iUserFlags);
			if( rcQ != JSON_ERROR_NONE ){
				*pDecoder->pErr = rcQ;
				return SXERR_ABORT;
			}
			if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){
				/* Decoding to an OBJECT: php refuses a property name whose
				 * FIRST byte is NUL ("\0...") with
				 * JSON_ERROR_INVALID_PROPERTY_NAME — that prefix is reserved
				 * for its mangled private/protected names. A NUL further in is
				 * legal, and array mode (assoc / JSON_OBJECT_AS_ARRAY /
				 * json_validate) takes any key. PHL used to build the property
				 * in silence. */
				int nKeyByte;
				const char *zKey = ph7_value_to_string(pKey,&nKeyByte);
				if( nKeyByte > 0 && zKey[0] == '\0' ){
					*pDecoder->pErr = JSON_ERROR_INVALID_PROPERTY_NAME;
					return SXERR_ABORT;
				}
			}
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
		/* php returns a stdClass for a JSON object (one dynamic property per member,
		 * nested objects already converted by the recursion) unless assoc was asked. */
		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){
			PH7_MemObjToObject(pWorker);
		}
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
 * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 512[,int $options = 0 ]]])
 *  Takes a JSON encoded string and converts it into a PHP variable.
 * Parameters
 *  $json
 *    The json string being decoded.
 * $assoc
 *   When TRUE, returned objects will be converted into associative arrays.
 * $depth
 *   User specified recursion depth.
 * $options
 *   Bitmask of JSON decode options: JSON_OBJECT_AS_ARRAY (objects decode as
 *   associative arrays when $assoc is NULL), JSON_BIGINT_AS_STRING (an integer
 *   beyond int64 stays the exact source text instead of a float),
 *   JSON_INVALID_UTF8_IGNORE/_SUBSTITUTE and JSON_THROW_ON_ERROR
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
static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth,int iUserFlags)
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
	sDecoder.iUserFlags = iUserFlags;
	/* php's $depth (default 512; the callers' ValueError screens guarantee
	 * 1..INT_MAX-1), bounded by the engine's stack-safety ceiling. The old code
	 * CLAMPED it to an engine limit of 32, so a 40-deep document php decodes
	 * answered NULL/JSON_ERROR_DEPTH. Recursion is bounded by the INPUT's
	 * actual nesting, never by the requested ceiling. */
	sDecoder.rec_depth = nDepth > PH7_JSON_DEPTH_CEILING ? PH7_JSON_DEPTH_CEILING : nDepth;
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
	if( pVm->json_rc == JSON_ERROR_NONE && sDecoder.pIn < sDecoder.pEnd ){
		/* php requires the whole input to be ONE JSON value; tokens left after a
		 * complete value (e.g. '"a":1', '{}x', '1 2') are a syntax error. */
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
	int nDepth = 512;
	int iFlags = 0;
	/* php coerces a scalar argument to string here (weak mode); the shared ZPP
	 * screen in vm.c has already rejected the values that cannot coerce. */
	if( nArg < 1 ){
		/* Missing/Invalid arguments, return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg > 3 && ph7_value_is_int(apArg[3]) ){
		/* $flags (JSON_OBJECT_AS_ARRAY / JSON_BIGINT_AS_STRING /
		 * JSON_INVALID_UTF8_* / JSON_THROW_ON_ERROR). */
		iFlags = ph7_value_to_int(apArg[3]);
	}
	/* Extract the JSON string */
	zIn = ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		/* Empty string: php records a syntax error (json_last_error() == 4) and
		 * returns NULL, or raises a JsonException with JSON_THROW_ON_ERROR. */
		pCtx->pVm->json_rc = JSON_ERROR_SYNTAX;
		if( iFlags & JSON_THROW_ON_ERROR ){
			return PH7_VmThrowExceptionCode(pCtx,"JsonException",
				JSON_ERROR_SYNTAX,"%s",JsonErrorMsg(JSON_ERROR_SYNTAX));
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* php's $associative is a ?bool: an explicit true/false decides on its
		 * own (false beats the flag), and only NULL lets JSON_OBJECT_AS_ARRAY
		 * answer instead. */
		if( ph7_value_is_null(apArg[1]) ){
			iAssoc = (iFlags & JSON_OBJECT_AS_ARRAY) != 0;
		}else{
			iAssoc = ph7_value_to_bool(apArg[1]) != 0;
		}
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
	 * call-context result; on failure we replace it with NULL (or throw). */
	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth,iFlags) != JSON_ERROR_NONE ){
		/* Something goes wrong while decoding JSON input. */
		if( iFlags & JSON_THROW_ON_ERROR ){
			/* php: raise a JsonException carrying json_last_error_msg() text. */
			return PH7_VmThrowExceptionCode(pCtx,"JsonException",
				(sxi32)pCtx->pVm->json_rc,"%s",
				JsonErrorMsg(pCtx->pVm->json_rc));
		}
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
 *  $depth  Maximum nesting depth (php's default of 512, honored verbatim).
 *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).
 * Return
 *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().
 */
PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zIn;
	int nByte;
	int nDepth = 512;
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
	/* php's only ACCEPTED $flags value here is JSON_INVALID_UTF8_IGNORE, which
	 * makes a payload with undecodable bytes VALID; it rides the same rail as
	 * json_decode's. Any other bit is a ValueError naming the one flag there is --
	 * php refuses the whole json_decode set for this function, and PHL used to take
	 * whatever it was given and validate on. Decode in associative mode so the
	 * "objects are returned as an array" warning is not raised - the decoded value
	 * is discarded, only its validity matters. */
	{
		int iFlags = (nArg > 2 && ph7_value_is_int(apArg[2]))
			? ph7_value_to_int(apArg[2]) : 0;
		if( (iFlags & ~JSON_INVALID_UTF8_IGNORE) != 0 ){
			pVm->json_rc = JSON_ERROR_NONE;
			return PH7_VmThrowException(pCtx,"ValueError",
				"json_validate(): Argument #3 ($flags) must be a valid flag "
				"(allowed flags: JSON_INVALID_UTF8_IGNORE)");
		}
		ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth,iFlags)
			== JSON_ERROR_NONE);
	}
	return PH7_OK;
}
