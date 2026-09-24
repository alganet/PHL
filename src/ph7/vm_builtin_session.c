/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <time.h>
#ifndef PH7_DISABLE_BUILTIN_FUNC
#ifndef PH7_DISABLE_DISK_IO
/*
 * Sessions: file-backed session_* functions over the $_SESSION superglobal.
 *
 * This was an embedded-PHP chunk holding its state on a private `__SessS` class
 * with five static properties, plus five `__sess_*` PHP helpers. All six names
 * are gone: the state is on the VM (pVm->iSessStatus / sSessId / sSessName /
 * sSessPath) and the functions are these C routines. Moving the
 * state off a PHP class also decoupled the INI subsystem, which used to reach
 * into `__SessS::$name` / `$path` to live-wire session.name / session.save_path
 * and now reads the same VM fields.
 *
 * Session files keep php's DEFAULT "php" serialize-handler format
 * (`key|<serialized>` runs), so they still interoperate with a stock php install
 * both ways. The primitives that format needs -- serialize/unserialize -- and the
 * file IO are reached by CALLING the engine's own builtins by name rather than
 * duplicating them here: one mechanism (VmSessCall) for all of them, which is
 * also what keeps the stream-wrapper behaviour of file_get_contents/file_put_contents
 * identical to what the chunk had.
 */

/* php's session_status() values (PHP_SESSION_DISABLED is never reported here). */
#define VM_SESSION_NONE   1
#define VM_SESSION_ACTIVE 2

/*
 * Call an engine builtin by name. The session logic is a thin layer over
 * serialize/unserialize/file IO, and re-implementing those in C would fork
 * behaviour that has to stay identical (stream wrappers, the serialize grammar).
 */
static sxi32 VmSessCall(ph7_vm *pVm,const char *zFunc,int nArg,ph7_value **apArg,ph7_value *pResult)
{
	ph7_value sName;
	SyString sStr;
	sxi32 rc;
	PH7_MemObjInit(pVm,&sName);
	SyStringInitFromBuf(&sStr,zFunc,SyStrlen(zFunc));
	PH7_MemObjInitFromString(pVm,&sName,&sStr);
	rc = PH7_VmCallUserFunction(&(*pVm),&sName,nArg,apArg,pResult);
	PH7_MemObjRelease(&sName);
	return rc;
}
static void VmSessStrArg(ph7_vm *pVm,ph7_value *pOut,const char *zVal,sxu32 nVal)
{
	SyString sStr;
	PH7_MemObjInit(pVm,pOut);
	SyStringInitFromBuf(&sStr,zVal,nVal);
	PH7_MemObjInitFromString(pVm,pOut,&sStr);
}
/*
 * The save path, resolving the lazy default the way the chunk did: an unset path
 * becomes sys_get_temp_dir() with any trailing slash removed, and is remembered.
 */
static void VmSessResolvePath(ph7_vm *pVm)
{
	ph7_value sRes;
	if( SyBlobLength(&pVm->sSessPath) > 0 ){
		return;
	}
	PH7_MemObjInit(pVm,&sRes);
	if( VmSessCall(pVm,"sys_get_temp_dir",0,0,&sRes) == SXRET_OK ){
		const char *zTmp = (const char *)SyBlobData(&sRes.sBlob);
		sxu32 nTmp = SyBlobLength(&sRes.sBlob);
		while( nTmp > 0 && zTmp[nTmp-1] == '/' ){ nTmp--; }
		SyBlobReset(&pVm->sSessPath);
		SyBlobAppend(&pVm->sSessPath,zTmp,nTmp);
	}
	PH7_MemObjRelease(&sRes);
}
/* php joins the save path and the file name with PHP_DIR_SEPARATOR, which a
 * warning naming the file shows: a backslash on Windows. */
#ifdef __WINNT__
#define VM_SESS_FILE_PREFIX "\\sess_"
#else
#define VM_SESS_FILE_PREFIX "/sess_"
#endif
/* "<save_path>/sess_<id>" */
static void VmSessFile(ph7_vm *pVm,SyBlob *pOut)
{
	VmSessResolvePath(pVm);
	SyBlobReset(pOut);
	SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));
	SyBlobAppend(pOut,VM_SESS_FILE_PREFIX,sizeof(VM_SESS_FILE_PREFIX)-1);
	SyBlobAppend(pOut,SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId));
}
/* php's PS_MAX_SID_LENGTH: the longest id it will read or make. */
#define VM_SESS_MAX_ID 256

/*
 * A fresh id: 32 characters over php's 4-bits-per-character alphabet, which is
 * lowercase hex. The two directives that would widen either number,
 * session.sid_length and session.sid_bits_per_character, are DEPRECATED in php 8.4
 * — §10 does not carry php's deprecated surface, so php's defaults are the only
 * shape here and an id made by either engine reads the same way to the other.
 */
static void VmSessGenId(ph7_vm *pVm,SyBlob *pOut)
{
	static const char zAlpha[] = "0123456789abcdef";
	unsigned char zRaw[32];
	int i;
	SyBlobReset(pOut);
	SyRandomness(&pVm->sPrng,zRaw,sizeof(zRaw));
	for( i = 0 ; i < 32 ; i++ ){
		char c = zAlpha[zRaw[i] & 15];
		SyBlobAppend(pOut,&c,1);
	}
}
/*
 * php's php_session_valid_key(): an id is 1..256 bytes of A-Z, a-z, 0-9, "-", ",".
 * The id names a FILE in the save path, which is why the set is this small.
 */
static int VmSessIdValid(const char *z,sxu32 n)
{
	sxu32 i;
	if( n < 1 || n > VM_SESS_MAX_ID ){
		return 0;
	}
	for( i = 0 ; i < n ; i++ ){
		char c = z[i];
		if( !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')
		   || (c >= 'A' && c <= 'Z') || c == ',' || c == '-') ){
			return 0;
		}
	}
	return 1;
}
/*
 * The bytes php drops an id for WITHOUT a word, before it ever validates it: the
 * ones that would break the Set-Cookie header or a log line it lands in. An id
 * carrying one is thrown away and a fresh one made, where any other invalid
 * character is reported and refuses the start outright.
 */
static int VmSessIdDangerous(const char *z,sxu32 n)
{
	sxu32 i;
	for( i = 0 ; i < n ; i++ ){
		if( SyByteFind("\r\n\t <>'\"\\",sizeof("\r\n\t <>'\"\\")-1,z[i],0) == SXRET_OK ){
			return 1;
		}
	}
	return 0;
}
/*
 * Read the session file into pOut, answering FALSE when there is none.
 *
 * The existence check is not an optimization: file_get_contents() warns on a
 * missing path, and a first-ever session_start() has no file yet -- the PHP chunk
 * guarded the read with file_exists() for exactly this reason.
 */
static int VmSessReadFile(ph7_vm *pVm,SyBlob *pFile,ph7_value *pOut)
{
	ph7_value sPath,sExists;
	ph7_value *apA[1];
	int bOk;
	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));
	PH7_MemObjInit(pVm,&sExists);
	apA[0] = &sPath;
	VmSessCall(pVm,"file_exists",1,apA,&sExists);
	/* PH7_MemObjToBool converts IN PLACE and answers a STATUS, not the boolean --
	 * the value lands in x.iVal. Reading its return as the answer makes every
	 * existence check false, which silently empties the session on every reload. */
	PH7_MemObjToBool(&sExists);
	bOk = sExists.x.iVal != 0;
	PH7_MemObjRelease(&sExists);
	if( bOk ){
		VmSessCall(pVm,"file_get_contents",1,apA,pOut);
		bOk = (pOut->iFlags & MEMOBJ_STRING) != 0;
	}
	PH7_MemObjRelease(&sPath);
	return bOk;
}
/*
 * Delete the session file if it is there. Same reason the read is guarded:
 * unlink() warns on a missing path, and destroying a session that was never
 * written (or regenerating an id before the first write) is normal.
 */
static void VmSessUnlinkIfExists(ph7_vm *pVm,SyBlob *pFile)
{
	ph7_value sPath,sExists;
	ph7_value *apA[1];
	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));
	PH7_MemObjInit(pVm,&sExists);
	apA[0] = &sPath;
	VmSessCall(pVm,"file_exists",1,apA,&sExists);
	PH7_MemObjToBool(&sExists);
	if( sExists.x.iVal ){
		VmSessCall(pVm,"unlink",1,apA,0);
	}
	PH7_MemObjRelease(&sExists);
	PH7_MemObjRelease(&sPath);
}
/* $_SESSION, or NULL when the superglobal is somehow absent. */
static ph7_value * VmSessArray(ph7_vm *pVm)
{
	return PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);
}
/*
 * php's three session serialize handlers (session.serialize_handler).
 *
 * `php` is a run of `key|<serialized value>`, `php_binary` the same with a
 * one-byte key LENGTH in place of the delimiter, and `php_serialize` one
 * serialize() of the whole array. The first two therefore cannot spell every
 * $_SESSION key, and php drops what they cannot spell rather than writing a
 * payload it could not read back.
 */
#define VM_SESS_SER_PHP       0
#define VM_SESS_SER_BINARY    1
#define VM_SESS_SER_SERIALIZE 2
/* php's PS_BIN_MAX: php_binary holds the key length in ONE byte with 0x80 taken
 * as its PS_BIN_UNDEF marker, so 127 bytes is the longest key it can name. */
#define VM_SESS_BIN_MAX       127

/*
 * Which serializer is configured, or -1 for a name php has no handler for.
 * ini_set() refuses an unknown one, so only the php.ini/-d path can arm it -- and
 * session_start() is where php reports it and refuses to start.
 */
static int VmSessSerializerOrErr(ph7_vm *pVm)
{
	SyBlob sVal;
	const char *zVal;
	sxu32 nVal;
	int iRet = -1;
	SyBlobInit(&sVal,&pVm->sAllocator);
	PH7_VmIniGetStr(pVm,"session.serialize_handler",&sVal);
	zVal = (const char *)SyBlobData(&sVal);
	nVal = SyBlobLength(&sVal);
	if( nVal == sizeof("php")-1 && SyMemcmp(zVal,"php",nVal) == 0 ){
		iRet = VM_SESS_SER_PHP;
	}else if( nVal == sizeof("php_binary")-1 && SyMemcmp(zVal,"php_binary",nVal) == 0 ){
		iRet = VM_SESS_SER_BINARY;
	}else if( nVal == sizeof("php_serialize")-1 && SyMemcmp(zVal,"php_serialize",nVal) == 0 ){
		iRet = VM_SESS_SER_SERIALIZE;
	}
	SyBlobRelease(&sVal);
	return iRet;
}
static int VmSessSerializer(ph7_vm *pVm)
{
	int iRet = VmSessSerializerOrErr(pVm);
	return iRet < 0 ? VM_SESS_SER_PHP : iRet;
}
/* serialize() one value through the engine's own builtin. */
static void VmSessSerializeValue(ph7_vm *pVm,ph7_value *pVal,SyBlob *pOut)
{
	ph7_value sSer;
	ph7_value *apArg[1];
	PH7_MemObjInit(pVm,&sSer);
	apArg[0] = pVal;
	VmSessCall(pVm,"serialize",1,apArg,&sSer);
	SyBlobAppend(pOut,SyBlobData(&sSer.sBlob),SyBlobLength(&sSer.sBlob));
	PH7_MemObjRelease(&sSer);
}
/*
 * Serialize $_SESSION into pOut with the configured handler. Answers 0 when php
 * refuses the payload outright -- the `php` handler's key carrying its own
 * delimiter -- having raised the diagnostic under zWho, which is the whole prefix
 * php puts on it: "session_encode()", "session_write_close()" or, from the
 * request-shutdown writer, "PHP Request Shutdown".
 */
static int VmSessEncode(ph7_vm *pVm,SyBlob *pOut,const char *zWho)
{
	ph7_value *pSess = VmSessArray(pVm);
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode;
	int iSer = VmSessSerializer(pVm);
	sxu32 n;
	SyBlobReset(pOut);
	if( pSess == 0 || (pSess->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 1;
	}
	if( iSer == VM_SESS_SER_SERIALIZE ){
		/* One serialize() of the array itself, so a numeric key and a key holding
		 * a '|' both simply round-trip. */
		VmSessSerializeValue(pVm,pSess,pOut);
		return 1;
	}
	pMap = (ph7_hashmap *)pSess->x.pOther;
	pNode = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry && pNode ; n++, pNode = pNode->pPrev ){
		ph7_value *pVal;
		const char *zKey;
		sxu32 nKey;
		char zMsg[256];
		if( pNode->iType == HASHMAP_INT_NODE ){
			/* Both formats key by NAME; an integer key has no spelling in either. */
			SyBufferFormat(zMsg,sizeof(zMsg),"%s: Skipping numeric key %qd",zWho,pNode->xKey.iKey);
			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
			continue;
		}
		zKey = (const char *)SyBlobData(&pNode->xKey.sKey);
		nKey = SyBlobLength(&pNode->xKey.sKey);
		if( iSer == VM_SESS_SER_BINARY ){
			if( nKey > VM_SESS_BIN_MAX ){
				continue;   /* no length byte can name it; php drops it in silence */
			}
		}else if( SyByteFind(zKey,nKey,'|',0) == SXRET_OK ){
			/* The delimiter inside a key would make the payload unreadable, so php
			 * writes NOTHING rather than a file it cannot parse back. */
			SyBufferFormat(zMsg,sizeof(zMsg),
				"%s: Failed to write session data. Data contains invalid key \"%.*s\"",
				zWho,(int)nKey,zKey);
			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
			SyBlobReset(pOut);
			return 0;
		}
		pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
		if( pVal == 0 ){
			continue;
		}
		if( iSer == VM_SESS_SER_BINARY ){
			char c = (char)nKey;
			SyBlobAppend(pOut,&c,1);
			SyBlobAppend(pOut,zKey,nKey);
		}else{
			SyBlobAppend(pOut,zKey,nKey);
			SyBlobAppend(pOut,"|",1);
		}
		VmSessSerializeValue(pVm,pVal,pOut);
	}
	return 1;
}
/*
 * php's php_session_normalize_vars(): a walk of the session variables that reports
 * the ones its store cannot NAME and touches nothing else. It is not the encoder —
 * a value the serializer would refuse is not looked at here, and neither is a key
 * carrying the delimiter — and it is why session_decode() reports a numeric key
 * before it has written anything.
 */
static void VmSessNormalizeVars(ph7_vm *pVm,const char *zWho)
{
	ph7_value *pSess = VmSessArray(pVm);
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode;
	sxu32 n;
	if( pSess == 0 || (pSess->iFlags & MEMOBJ_HASHMAP) == 0
	 || VmSessSerializer(pVm) == VM_SESS_SER_SERIALIZE ){
		return;
	}
	pMap = (ph7_hashmap *)pSess->x.pOther;
	pNode = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry && pNode ; n++, pNode = pNode->pPrev ){
		char zMsg[128];
		if( pNode->iType != HASHMAP_INT_NODE ){
			continue;
		}
		SyBufferFormat(zMsg,sizeof(zMsg),"%s: Skipping numeric key %qd",zWho,pNode->xKey.iKey);
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
	}
}
/*
 * Store one decoded `name => value` pair into the session array. The name goes in
 * RAW, which is php's php_set_session_var(): a store holding `7|i:1;` comes back
 * as the string key "7" in both engines, not the integer key an array subscript
 * would have folded it to.
 */
static void VmSessPut(ph7_vm *pVm,ph7_value *pDest,const char *zKey,sxu32 nKey,ph7_value *pVal)
{
	(void)pVm;
	PH7_HashmapInsertRawKey((ph7_hashmap *)pDest->x.pOther,zKey,nKey,pVal);
}
/* Give pDest a fresh empty array. */
static void VmSessEmptyArray(ph7_vm *pVm,ph7_value *pDest)
{
	PH7_MemObjRelease(pDest);
	pDest->x.pOther = PH7_NewHashmap(pVm,0,0);
	if( pDest->x.pOther ){
		MemObjSetType(pDest,MEMOBJ_HASHMAP);
	}
}
/*
 * Read a payload back into pDest. The `php` and `php_binary` handlers MERGE their
 * names into whatever is already there (which is what makes session_decode() an
 * overlay), while `php_serialize` REPLACES the whole variable -- so php really
 * does leave $_SESSION an int for `session_decode('i:5;')`.
 *
 * Answers 1 when php reports the payload decoded, 0 when it does not, and -1 when
 * a __wakeup()/__unserialize() threw: the exception is the diagnostic then, and
 * the caller propagates it instead of reporting a decode failure.
 */
static int VmSessDecodeInto(ph7_context *pCtx,const char *zSrc,sxu32 nLen,ph7_value *pDest)
{
	ph7_vm *pVm = pCtx->pVm;
	int iSer = VmSessSerializer(pVm);
	sxu32 nPos = 0;
	if( iSer == VM_SESS_SER_SERIALIZE ){
		ph7_value sVal;
		int nRead = 0;
		sxi32 rc;
		PH7_MemObjInit(pVm,&sVal);
		rc = nLen > 0
			? PH7_VmUnserializeOne(pCtx,zSrc,(int)nLen,&nRead,&sVal)
			: SXERR_SYNTAX;
		if( rc == PH7_EXCEPTION ){
			PH7_MemObjRelease(&sVal);
			return -1;
		}
		if( rc == SXRET_OK && (sVal.iFlags & MEMOBJ_NULL) == 0 ){
			PH7_MemObjRelease(pDest);
			PH7_MemObjStore(&sVal,pDest);
		}else{
			/* A payload that did not decode, and php's serialized NULL, both leave
			 * the variable an empty array; only the EMPTY payload is still a
			 * success, php's `result || !vallen`. */
			VmSessEmptyArray(pVm,pDest);
		}
		PH7_MemObjRelease(&sVal);
		return rc == SXRET_OK || nLen == 0;
	}
	if( (pDest->iFlags & MEMOBJ_HASHMAP) == 0 ){
		VmSessEmptyArray(pVm,pDest);
		if( (pDest->iFlags & MEMOBJ_HASHMAP) == 0 ){
			return 0;
		}
	}
	while( nPos < nLen ){
		const char *zKey;
		sxu32 nKey;
		ph7_value sVal;
		int nRead = 0;
		sxi32 rc;
		if( iSer == VM_SESS_SER_BINARY ){
			/* The length byte, with php's PS_BIN_UNDEF bit masked off. */
			nKey = (sxu32)(((const unsigned char *)zSrc)[nPos] & 0x7f);
			if( nPos + 1 + nKey > nLen ){
				return 0;
			}
			zKey = &zSrc[nPos + 1];
			nPos += 1 + nKey;
		}else{
			sxu32 nBar = nPos;
			while( nBar < nLen && zSrc[nBar] != '|' ){ nBar++; }
			if( nBar >= nLen ){
				return 0;   /* a trailing run with no delimiter is a failed decode */
			}
			zKey = &zSrc[nPos];
			nKey = nBar - nPos;
			nPos = nBar + 1;
		}
		PH7_MemObjInit(pVm,&sVal);
		rc = nPos < nLen
			? PH7_VmUnserializeOne(pCtx,&zSrc[nPos],(int)(nLen - nPos),&nRead,&sVal)
			: SXERR_SYNTAX;
		if( rc != SXRET_OK || nRead <= 0 ){
			/* nRead cannot be 0 for a value that parsed, but the loop's only
			 * guarantee of progress is this step -- and the bytes are a STORE, i.e.
			 * whatever was last written to the save path. */
			PH7_MemObjRelease(&sVal);
			return rc == PH7_EXCEPTION ? -1 : 0;
		}
		VmSessPut(pVm,pDest,zKey,nKey,&sVal);
		PH7_MemObjRelease(&sVal);
		nPos += (sxu32)nRead;
	}
	return 1;
}
/*
 * php's answer to a store it cannot read: the whole session goes — file, id and
 * variables — rather than the script running on half of one. A corrupt payload and
 * an attacker-supplied one look the same from here, which is why it is not a
 * partial load. zFunc names the caller php blames.
 */
static void VmSessDestroyBadStore(ph7_vm *pVm,SyBlob *pFile,const char *zFunc)
{
	ph7_value *pSess = VmSessArray(pVm);
	char zMsg[128];
	VmSessUnlinkIfExists(pVm,pFile);
	pVm->iSessStatus = VM_SESSION_NONE;
	SyBlobReset(&pVm->sSessId);
	if( pSess ){
		VmSessEmptyArray(pVm,pSess);
	}
	SyBufferFormat(zMsg,sizeof(zMsg),
		"%s(): Failed to decode session object. Session has been destroyed",zFunc);
	PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
}
/*
 * Load the stored copy into $_SESSION, which php always starts EMPTY here — only
 * session_decode() overlays what is already there. Answers 1 when the session is
 * loaded, 0 when the store was destroyed instead, and -1 for a pending exception.
 */
static int VmSessLoad(ph7_context *pCtx,SyBlob *pFile,const char *zFunc)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pSess = VmSessArray(pVm);
	ph7_value sRes;
	int iDec = 1;
	PH7_MemObjInit(pVm,&sRes);
	if( pSess ){
		VmSessEmptyArray(pVm,pSess);
		if( VmSessReadFile(pVm,pFile,&sRes) ){
			iDec = VmSessDecodeInto(pCtx,(const char *)SyBlobData(&sRes.sBlob),
				SyBlobLength(&sRes.sBlob),pSess);
		}
	}
	PH7_MemObjRelease(&sRes);
	if( iDec == 0 ){
		VmSessDestroyBadStore(pVm,pFile,zFunc);
	}
	return iDec;
}
/* int session_status() */
static int vm_builtin_session_status(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	/* A serializer name with no handler behind it leaves php's session module
	 * DISABLED — reported from the start, before anything tries to open one. */
	ph7_result_int(pCtx,VmSessSerializerOrErr(pVm) < 0 ? 0 : pVm->iSessStatus);
	return PH7_OK;
}
/*
 * The three "cannot change while active / after headers" guards php puts on the
 * id, the name and the save path. Answers TRUE (warning already raised) when the
 * write must be refused.
 */
static int VmSessLocked(ph7_context *pCtx,const char *zFunc,const char *zWhat,int bHeaders)
{
	ph7_vm *pVm = pCtx->pVm;
	char zMsg[192];
	if( pVm->iSessStatus == VM_SESSION_ACTIVE ){
		SyBufferFormat(zMsg,sizeof(zMsg),"%s(): %s cannot be changed when a session is active",
			zFunc,zWhat);
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
		return 1;
	}
	if( bHeaders && pVm->bHeadersSent ){
		SyBufferFormat(zMsg,sizeof(zMsg),
			"%s(): %s cannot be changed after headers have already been sent",zFunc,zWhat);
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
		return 1;
	}
	return 0;
}
/*
 * session_id() / session_name() / session_save_path(): read the current value,
 * or set it and answer the previous one. One body, three directives.
 */
static int VmSessAccessor(ph7_context *pCtx,int nArg,ph7_value **apArg,
	SyBlob *pSlot,const char *zFunc,const char *zWhat,int bRtrimSlash)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sOld;
	if( pSlot == &pVm->sSessPath ){
		VmSessResolvePath(pVm);
	}
	SyBlobInit(&sOld,&pVm->sAllocator);
	SyBlobAppend(&sOld,SyBlobData(pSlot),SyBlobLength(pSlot));
	if( nArg < 1 || ph7_value_is_null(apArg[0]) ){
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));
		SyBlobRelease(&sOld);
		return PH7_OK;
	}
	if( VmSessLocked(pCtx,zFunc,zWhat,pSlot != &pVm->sSessPath) ){
		SyBlobRelease(&sOld);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	{
		int nNew = 0;
		const char *zNew = ph7_value_to_string(apArg[0],&nNew);
		sxu32 nLen = (sxu32)nNew;
		if( bRtrimSlash ){
			while( nLen > 0 && zNew[nLen-1] == '/' ){ nLen--; }
		}
		SyBlobReset(pSlot);
		SyBlobAppend(pSlot,zNew,nLen);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));
	SyBlobRelease(&sOld);
	return PH7_OK;
}
static int vm_builtin_session_id(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessId,
		"session_id","Session ID",0);
}
static int vm_builtin_session_name(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessName,
		"session_name","Session name",0);
}
static int vm_builtin_session_save_path(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessPath,
		"session_save_path","Session save path",1);
}
/*
 * session_start()'s $options array -- declared in aBuiltinSig[] and read by
 * NOTHING, so `session_start(['name' => 'MYSID', 'cookie_lifetime' => 3600])`,
 * php's documented way to configure a session at the one point it can still be
 * configured, was accepted and dropped in silence.
 *
 * Each key is a session.<key> directive applied for this request; the one that is
 * not is `read_and_close`, which asks for the session to be closed again the
 * moment it has been read. A key the directive table refuses is reported and the
 * session still starts; a key that is not a STRING, or a value that is not a
 * scalar, is a hard error before anything is opened.
 */
static void VmSessWrite(ph7_vm *pVm,const char *zWho);
struct VmSessStartOpts {
	ph7_vm *pVm;
	int bReadClose;
	int iFail;          /* 1 = non-string key, 2 = non-scalar value */
	char zBadKey[64];
	char zBadType[32];
};
static int VmSessStartOptWalker(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	struct VmSessStartOpts *pOpt = (struct VmSessStartOpts *)pUserData;
	char zName[128];
	const char *zKey;
	int nKey = 0, nVal = 0;
	const char *zVal;
	if( !ph7_value_is_string(pKey) ){
		pOpt->iFail = 1;
		return SXERR_ABORT;
	}
	zKey = ph7_value_to_string(pKey,&nKey);
	if( nKey > (int)sizeof(pOpt->zBadKey)-1 ){
		nKey = (int)sizeof(pOpt->zBadKey)-1;
	}
	SyMemcpy(zKey,pOpt->zBadKey,(sxu32)nKey);
	pOpt->zBadKey[nKey] = 0;
	if( !ph7_value_is_string(pVal) && !ph7_value_is_int(pVal) && !ph7_value_is_bool(pVal) ){
		const char *zType = PH7_MemObjTypeDump(pVal);
		sxu32 nType = (sxu32)SyStrlen(zType);
		if( nType > sizeof(pOpt->zBadType)-1 ){
			nType = sizeof(pOpt->zBadType)-1;
		}
		SyMemcpy(zType,pOpt->zBadType,nType);
		pOpt->zBadType[nType] = 0;
		pOpt->iFail = 2;
		return SXERR_ABORT;
	}
	if( nKey == (int)sizeof("read_and_close")-1
	 && SyMemcmp(pOpt->zBadKey,"read_and_close",(sxu32)nKey) == 0 ){
		pOpt->bReadClose = ph7_value_to_bool(pVal);
		return PH7_OK;
	}
	/* php stringifies the value the way ini_set() does, a bool becoming "1"/"". */
	if( ph7_value_is_bool(pVal) ){
		zVal = ph7_value_to_bool(pVal) ? "1" : "";
		nVal = (int)SyStrlen(zVal);
	}else{
		zVal = ph7_value_to_string(pVal,&nVal);
	}
	SyBufferFormat(zName,sizeof(zName),"session.%s",pOpt->zBadKey);
	if( !PH7_VmIniSet(pOpt->pVm,zName,(sxu32)SyStrlen(zName),zVal,(sxu32)nVal,
		"session_start()") ){
		char zMsg[160];
		SyBufferFormat(zMsg,sizeof(zMsg),
			"session_start(): Setting option \"%s\" failed",pOpt->zBadKey);
		PH7_VmThrowError(pOpt->pVm,0,PH7_CTX_WARNING,zMsg);
	}
	return PH7_OK;
}
/*
 * The Set-Cookie that carries the id, built out of the seven session.cookie_*
 * directives rather than the name and the id alone -- which is what this used to
 * send, so every session cookie went out with no path, no expiry and no
 * HttpOnly/SameSite whatever the configuration said, and a program that had set
 * `session.cookie_secure` was still handing its id to a plaintext request.
 *
 * php sends it on every start and again whenever the id changes; it is a plain
 * response header, so the CLI has nowhere to put it and simply does not.
 */
static void VmSessSendCookie(ph7_vm *pVm)
{
	SyBlob sPath,sDomain,sSame;
	sxi64 iLife;
	if( !PH7_VmIniGetBool(pVm,"session.use_cookies",1) ){
		return;
	}
	/* Replace, never accumulate: the reply carries ONE id. */
	PH7_VmRemoveCookieByName(pVm,(const char *)SyBlobData(&pVm->sSessName),
		SyBlobLength(&pVm->sSessName));
	SyBlobInit(&sPath,&pVm->sAllocator);
	SyBlobInit(&sDomain,&pVm->sAllocator);
	SyBlobInit(&sSame,&pVm->sAllocator);
	PH7_VmIniGetStr(pVm,"session.cookie_path",&sPath);
	PH7_VmIniGetStr(pVm,"session.cookie_domain",&sDomain);
	PH7_VmIniGetStr(pVm,"session.cookie_samesite",&sSame);
	iLife = PH7_VmIniGetInt(pVm,"session.cookie_lifetime",0);
	PH7_VmEmitCookie(pVm,
		(const char *)SyBlobData(&pVm->sSessName),SyBlobLength(&pVm->sSessName),
		(const char *)SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId),1,
		/* A lifetime is a DURATION here and an absolute time on the wire; 0 is
		 * php's "until the browser closes", which sends no expiry at all. */
		iLife > 0 ? (sxi64)time(0) + iLife : 0,
		(const char *)SyBlobData(&sPath),SyBlobLength(&sPath),
		(const char *)SyBlobData(&sDomain),SyBlobLength(&sDomain),
		PH7_VmIniGetBool(pVm,"session.cookie_secure",0),
		PH7_VmIniGetBool(pVm,"session.cookie_httponly",0),
		(const char *)SyBlobData(&sSame),SyBlobLength(&sSame),
		PH7_VmIniGetBool(pVm,"session.cookie_partitioned",0));
	SyBlobRelease(&sPath);
	SyBlobRelease(&sDomain);
	SyBlobRelease(&sSame);
}
/* bool session_start(array $options = []) */
static int vm_builtin_session_start(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	struct VmSessStartOpts sOpt;
	SyBlob sFile;
	int iLoad;
	SyZero(&sOpt,sizeof(sOpt));
	sOpt.pVm = pVm;
	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){
		ph7_array_walk(apArg[0],VmSessStartOptWalker,&sOpt);
		if( sOpt.iFail == 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"session_start(): Argument #1 ($options) must be of type array with"
				" keys as string");
		}
		if( sOpt.iFail == 2 ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"session_start(): Option \"%s\" must be of type string|int|bool, %s given",
				sOpt.zBadKey,sOpt.zBadType);
		}
	}
	if( pVm->iSessStatus == VM_SESSION_ACTIVE ){
		PH7_VmThrowError(pVm,0,PH7_CTX_NOTICE,
			"session_start(): Ignoring session_start() because a session is already active");
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( pVm->bHeadersSent ){
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
			"session_start(): Session cannot be started after headers have already been sent");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( VmSessSerializerOrErr(pVm) < 0 ){
		/* Only php.ini / -d can arm a handler that does not exist; php reports it
		 * here and starts nothing at all. */
		SyBlob sVal;
		char zMsg[192];
		SyBlobInit(&sVal,&pVm->sAllocator);
		PH7_VmIniGetStr(pVm,"session.serialize_handler",&sVal);
		SyBufferFormat(zMsg,sizeof(zMsg),
			"session_start(): Cannot find session serialization handler \"%.*s\""
			" - session startup failed",
			(int)SyBlobLength(&sVal),(const char *)SyBlobData(&sVal));
		SyBlobRelease(&sVal);
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( SyBlobLength(&pVm->sSessId) > 0
	 && VmSessIdDangerous((const char *)SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId)) ){
		/* A byte that would break the header this id is about to be written into:
		 * php throws the id away without a word and makes a fresh one. */
		SyBlobReset(&pVm->sSessId);
	}
	if( SyBlobLength(&pVm->sSessId) == 0 ){
		/* Adopt the id the client sent, when it is one php would accept. An id a
		 * REQUEST supplied is simply not adopted when it is not — the visitor does
		 * not get to end the request — where the one a script SET is reported below. */
		ph7_value *pCookie = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);
		if( pCookie && (pCookie->iFlags & MEMOBJ_HASHMAP) ){
			ph7_value sKey,sVal;
			ph7_hashmap_node *pNode = 0;
			VmSessStrArg(pVm,&sKey,(const char *)SyBlobData(&pVm->sSessName),
				SyBlobLength(&pVm->sSessName));
			PH7_MemObjInit(pVm,&sVal);
			if( PH7_HashmapLookup((ph7_hashmap *)pCookie->x.pOther,&sKey,&pNode) == SXRET_OK
			 && pNode ){
				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);
			}
			if( (sVal.iFlags & MEMOBJ_STRING)
			 && VmSessIdValid((const char *)SyBlobData(&sVal.sBlob),SyBlobLength(&sVal.sBlob)) ){
				SyBlobReset(&pVm->sSessId);
				SyBlobAppend(&pVm->sSessId,SyBlobData(&sVal.sBlob),SyBlobLength(&sVal.sBlob));
			}
			PH7_MemObjRelease(&sVal);
			PH7_MemObjRelease(&sKey);
		}
	}
	if( SyBlobLength(&pVm->sSessId) > 0
	 && !VmSessIdValid((const char *)SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId)) ){
		/* The id names a FILE under the save path, so php refuses to open anything at
		 * all for one outside its alphabet — and reports the store's own failure with
		 * it, since that is the read that never happened. */
		char zMsg[224];
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
			"session_start(): Session ID is too long or contains illegal characters."
			" Only the A-Z, a-z, 0-9, \"-\", and \",\" characters are allowed");
		VmSessResolvePath(pVm);
		SyBufferFormat(zMsg,sizeof(zMsg),
			"session_start(): Failed to read session data: files (path: %.*s)",
			(int)SyBlobLength(&pVm->sSessPath),(const char *)SyBlobData(&pVm->sSessPath));
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
		SyBlobReset(&pVm->sSessId);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( SyBlobLength(&pVm->sSessId) == 0 ){
		VmSessGenId(pVm,&pVm->sSessId);
	}
	SyBlobInit(&sFile,&pVm->sAllocator);
	VmSessFile(pVm,&sFile);
	iLoad = VmSessLoad(pCtx,&sFile,"session_start");
	SyBlobRelease(&sFile);
	if( iLoad < 0 ){
		return PH7_EXCEPTION;
	}
	if( iLoad == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pVm->iSessStatus = VM_SESSION_ACTIVE;
	VmSessSendCookie(pVm);
	if( sOpt.bReadClose ){
		/* php's `read_and_close`: the store is read and released again before the
		 * script runs, so a request that only READS the session does not hold its
		 * lock for the rest of its life. */
		VmSessWrite(pVm,"session_start()");
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Write the open session back to its file and close it. Shared by the builtin and
 * by the request-shutdown writer, which is why it takes only the VM: at shutdown
 * there is no calling frame to report against.
 */
static void VmSessPutFile(ph7_vm *pVm,SyBlob *pFile,const char *zData,sxu32 nData)
{
	ph7_value sPath,sPayload;
	ph7_value *apA[2];
	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));
	VmSessStrArg(pVm,&sPayload,zData,nData);
	apA[0] = &sPath;
	apA[1] = &sPayload;
	VmSessCall(pVm,"file_put_contents",2,apA,0);
	PH7_MemObjRelease(&sPath);
	PH7_MemObjRelease(&sPayload);
}
/* Encode the open session and write it to the file the CURRENT id names. */
static void VmSessSave(ph7_vm *pVm,const char *zWho)
{
	SyBlob sFile,sData;
	SyBlobInit(&sFile,&pVm->sAllocator);
	SyBlobInit(&sData,&pVm->sAllocator);
	VmSessFile(pVm,&sFile);
	/* An encode php refused still gets written — as the EMPTY payload, which is
	 * what its store is handed when the serializer answers nothing. The stale copy
	 * does not survive either way. */
	VmSessEncode(pVm,&sData,zWho);
	VmSessPutFile(pVm,&sFile,(const char *)SyBlobData(&sData),SyBlobLength(&sData));
	SyBlobRelease(&sFile);
	SyBlobRelease(&sData);
}
static void VmSessWrite(ph7_vm *pVm,const char *zWho)
{
	VmSessSave(pVm,zWho);
	pVm->iSessStatus = VM_SESSION_NONE;
}
/* bool session_write_close() / session_commit() */
static int vm_builtin_session_write_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	VmSessWrite(pVm,"session_write_close()");
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * php writes an open session back at REQUEST SHUTDOWN — from the session module's
 * own RSHUTDOWN, which runs after the script's register_shutdown_function()
 * callbacks and after the output buffers are flushed. Registering the writer as a
 * shutdown callback (what this did) put it FIRST in that list, so a `$_SESSION`
 * entry written from inside a shutdown callback — the flash-message / last-seen
 * idiom — was silently dropped.
 */
PH7_PRIVATE void PH7_VmSessionShutdown(ph7_vm *pVm)
{
	if( pVm->iSessStatus == VM_SESSION_ACTIVE ){
		/* php names this caller "PHP Request Shutdown" — there is no frame to
		 * report against, so a serializer diagnostic raised here says so. */
		VmSessWrite(pVm,"PHP Request Shutdown");
	}
}
/* bool session_abort() — drop the in-memory session without writing it back */
static int vm_builtin_session_abort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pCtx->pVm->iSessStatus != VM_SESSION_ACTIVE ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pCtx->pVm->iSessStatus = VM_SESSION_NONE;
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool session_reset() — re-read the stored copy over the in-memory one */
static int vm_builtin_session_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sFile;
	int iLoad;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sFile,&pVm->sAllocator);
	VmSessFile(pVm,&sFile);
	iLoad = VmSessLoad(pCtx,&sFile,"session_reset");
	SyBlobRelease(&sFile);
	if( iLoad < 0 ){
		return PH7_EXCEPTION;
	}
	/* php answers TRUE even when the store it just read was the one it had to
	 * destroy: the reset itself did happen. */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool session_unset() — empty $_SESSION, keep the session open */
static int vm_builtin_session_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pSess;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pSess = VmSessArray(pVm);
	if( pSess ){
		PH7_MemObjRelease(pSess);
		pSess->x.pOther = PH7_NewHashmap(pVm,0,0);
		if( pSess->x.pOther ){
			MemObjSetType(pSess,MEMOBJ_HASHMAP);
		}
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool session_destroy() */
static int vm_builtin_session_destroy(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sFile;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
			"session_destroy(): Trying to destroy uninitialized session");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sFile,&pVm->sAllocator);
	VmSessFile(pVm,&sFile);
	VmSessUnlinkIfExists(pVm,&sFile);
	SyBlobRelease(&sFile);
	pVm->iSessStatus = VM_SESSION_NONE;
	SyBlobReset(&pVm->sSessId);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool session_regenerate_id(bool $delete_old_session = false) */
static int vm_builtin_session_regenerate_id(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sFile;
	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
			"session_regenerate_id(): Session ID cannot be regenerated when there is no active session");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sFile,&pVm->sAllocator);
	if( nArg > 0 && ph7_value_to_bool(apArg[0]) ){
		VmSessFile(pVm,&sFile);
		VmSessUnlinkIfExists(pVm,&sFile);
	}else{
		/* The old id keeps what the session holds NOW. A regenerating request is the
		 * one whose reply may not arrive, so php leaves the previous id readable
		 * rather than a session that exists under neither id. */
		VmSessSave(pVm,"session_regenerate_id()");
	}
	VmSessGenId(pVm,&pVm->sSessId);
	/* The new id's store is OPENED, which for the files handler means it exists and
	 * is empty until the session closes over it. */
	VmSessFile(pVm,&sFile);
	VmSessPutFile(pVm,&sFile,"",0);
	SyBlobRelease(&sFile);
	/* The client is told the new id here; without this the browser keeps sending
	 * the old one and the regenerated session is unreachable. */
	VmSessSendCookie(pVm);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * The seven cookie directives, in the order php's session_get_cookie_params()
 * reports them and with the TYPE each one is reported as: an int lifetime, three
 * bools, three strings.
 */
static const struct {
	const char *zKey;
	const char *zIni;
	int iKind;    /* 0 = string, 1 = int, 2 = bool */
} aSessCookieParam[] = {
	{ "lifetime",    "session.cookie_lifetime",    1 },
	{ "path",        "session.cookie_path",        0 },
	{ "domain",      "session.cookie_domain",      0 },
	{ "secure",      "session.cookie_secure",      2 },
	{ "partitioned", "session.cookie_partitioned", 2 },
	{ "httponly",    "session.cookie_httponly",    2 },
	{ "samesite",    "session.cookie_samesite",    0 },
};
/* array session_get_cookie_params() */
static int vm_builtin_session_get_cookie_params(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pArray,*pVal;
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aSessCookieParam) ; n++ ){
		if( aSessCookieParam[n].iKind == 1 ){
			ph7_value_int64(pVal,PH7_VmIniGetInt(pVm,aSessCookieParam[n].zIni,0));
		}else if( aSessCookieParam[n].iKind == 2 ){
			ph7_value_bool(pVal,PH7_VmIniGetBool(pVm,aSessCookieParam[n].zIni,0));
		}else{
			SyBlob sVal;
			SyBlobInit(&sVal,&pVm->sAllocator);
			PH7_VmIniGetStr(pVm,aSessCookieParam[n].zIni,&sVal);
			ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sVal),
				(const char *)SyBlobData(&sVal));
			SyBlobRelease(&sVal);
		}
		ph7_array_add_strkey_elem(pArray,aSessCookieParam[n].zKey,pVal);
		ph7_value_reset_string_cursor(pVal);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* Push one cookie parameter through the ini table, which is its only store. */
static void VmSessSetCookieIni(ph7_vm *pVm,const char *zIni,const char *zVal,int nVal)
{
	ph7_value sName,sVal;
	ph7_value *apA[2];
	VmSessStrArg(pVm,&sName,zIni,(sxu32)SyStrlen(zIni));
	VmSessStrArg(pVm,&sVal,zVal,(sxu32)nVal);
	apA[0] = &sName;
	apA[1] = &sVal;
	VmSessCall(pVm,"ini_set",2,apA,0);
	PH7_MemObjRelease(&sName);
	PH7_MemObjRelease(&sVal);
}
static void VmSessSetCookieBool(ph7_vm *pVm,const char *zIni,int bVal)
{
	VmSessSetCookieIni(pVm,zIni,bVal ? "1" : "0",1);
}
struct VmSessCookieArgs {
	ph7_vm *pVm;
	int nApplied;
	char zBadKey[64];
};
static int VmSessCookieOptWalker(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	struct VmSessCookieArgs *pArgs = (struct VmSessCookieArgs *)pUserData;
	const char *zKey;
	int nKey = 0;
	sxu32 n;
	zKey = ph7_value_to_string(pKey,&nKey);
	for( n = 0 ; n < SX_ARRAYSIZE(aSessCookieParam) ; n++ ){
		if( nKey == (int)SyStrlen(aSessCookieParam[n].zKey)
		 && SyStrnicmp(zKey,aSessCookieParam[n].zKey,(sxu32)nKey) == 0 ){
			if( aSessCookieParam[n].iKind == 2 ){
				VmSessSetCookieBool(pArgs->pVm,aSessCookieParam[n].zIni,
					ph7_value_to_bool(pVal));
			}else{
				int nVal = 0;
				const char *zVal = ph7_value_to_string(pVal,&nVal);
				VmSessSetCookieIni(pArgs->pVm,aSessCookieParam[n].zIni,zVal,nVal);
			}
			pArgs->nApplied++;
			return PH7_OK;
		}
	}
	/* php reports the key and carries on; it is only an error when NONE of the
	 * keys were ones it knows. */
	if( pArgs->zBadKey[0] == 0 ){
		sxu32 nCopy = (sxu32)nKey;
		if( nCopy > sizeof(pArgs->zBadKey)-1 ){
			nCopy = sizeof(pArgs->zBadKey)-1;
		}
		SyMemcpy(zKey,pArgs->zBadKey,nCopy);
		pArgs->zBadKey[nCopy] = 0;
	}
	{
		char zMsg[160];
		SyBufferFormat(zMsg,sizeof(zMsg),
			"session_set_cookie_params(): Argument #1 ($lifetime_or_options)"
			" contains an unrecognized key \"%.*s\"",nKey,zKey);
		PH7_VmThrowError(pArgs->pVm,0,PH7_CTX_WARNING,zMsg);
	}
	return PH7_OK;
}
/*
 * bool session_set_cookie_params(array|int $lifetime_or_options, ?string $path = null,
 *     ?string $domain = null, ?bool $secure = null, ?bool $httponly = null)
 */
static int vm_builtin_session_set_cookie_params(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"session_set_cookie_params() expects at least 1 argument, 0 given");
	}
	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_int(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"session_set_cookie_params(): Argument #1 ($lifetime_or_options) must be"
			" of type array|int, %s given",PH7_MemObjTypeDump(apArg[0]));
	}
	/* Both refusals are about the header the parameters would have gone into: one
	 * already written, or one this session already sent. */
	if( VmSessLocked(pCtx,"session_set_cookie_params","Session cookie parameters",1) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( ph7_value_is_array(apArg[0]) ){
		struct VmSessCookieArgs sArgs;
		SyZero(&sArgs,sizeof(sArgs));
		sArgs.pVm = pVm;
		ph7_array_walk(apArg[0],VmSessCookieOptWalker,&sArgs);
		if( sArgs.nApplied < 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"session_set_cookie_params(): Argument #1 ($lifetime_or_options) must"
				" contain at least 1 valid key");
		}
	}else{
		sxi64 iLife = ph7_value_to_int64(apArg[0]);
		char zLife[32];
		int nLife;
		if( iLife < 0 ){
			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
				"session_set_cookie_params(): CookieLifetime cannot be negative");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		nLife = SyBufferFormat(zLife,sizeof(zLife),"%qd",iLife);
		VmSessSetCookieIni(pVm,"session.cookie_lifetime",zLife,nLife);
		if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
			int nVal = 0;
			const char *zVal = ph7_value_to_string(apArg[1],&nVal);
			VmSessSetCookieIni(pVm,"session.cookie_path",zVal,nVal);
		}
		if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
			int nVal = 0;
			const char *zVal = ph7_value_to_string(apArg[2],&nVal);
			VmSessSetCookieIni(pVm,"session.cookie_domain",zVal,nVal);
		}
		if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){
			VmSessSetCookieBool(pVm,"session.cookie_secure",ph7_value_to_bool(apArg[3]));
		}
		if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){
			VmSessSetCookieBool(pVm,"session.cookie_httponly",ph7_value_to_bool(apArg[4]));
		}
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* string|false session_create_id(string $prefix = "") */
static int vm_builtin_session_create_id(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zPfx = "";
	int nPfx = 0;
	SyBlob sId;
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		zPfx = ph7_value_to_string(apArg[0],&nPfx);
	}
	if( nPfx > VM_SESS_MAX_ID ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"session_create_id(): Argument #1 ($prefix) cannot be longer than %d characters",
			VM_SESS_MAX_ID);
	}
	if( nPfx > 0 && !VmSessIdValid(zPfx,(sxu32)nPfx) ){
		/* The prefix becomes the front of an id, so it lives under the id's own
		 * alphabet — php reports it and answers false rather than making one it
		 * would then refuse to start. */
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
			"session_create_id(): Prefix cannot contain special characters."
			" Only the A-Z, a-z, 0-9, \"-\", and \",\" characters are allowed");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sId,&pVm->sAllocator);
	VmSessGenId(pVm,&sId);
	ph7_result_string(pCtx,zPfx,nPfx);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sId),(int)SyBlobLength(&sId));
	SyBlobRelease(&sId);
	return PH7_OK;
}
/* string|false session_encode() */
static int vm_builtin_session_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sData;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
			"session_encode(): Cannot encode non-existent session");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sData,&pVm->sAllocator);
	if( VmSessEncode(pVm,&sData,"session_encode()") ){
		ph7_result_string(pCtx,(const char *)SyBlobData(&sData),(int)SyBlobLength(&sData));
	}else{
		ph7_result_bool(pCtx,0);
	}
	SyBlobRelease(&sData);
	return PH7_OK;
}
/* bool session_decode(string $data) */
static int vm_builtin_session_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pSess;
	const char *zData;
	int nData = 0,iDec;
	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
			"session_decode(): Session data cannot be decoded when there is no active session");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pSess = VmSessArray(pVm);
	if( pSess == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zData = ph7_value_to_string(apArg[0],&nData);
	VmSessNormalizeVars(pVm,"session_decode()");
	/* Unlike session_start(), this one decodes OVER whatever $_SESSION already
	 * holds — for the two keyed handlers; php_serialize replaces the variable. */
	iDec = VmSessDecodeInto(pCtx,zData,(sxu32)nData,pSess);
	if( iDec < 0 ){
		return PH7_EXCEPTION;
	}
	if( iDec == 0 ){
		SyBlob sFile;
		SyBlobInit(&sFile,&pVm->sAllocator);
		VmSessFile(pVm,&sFile);
		VmSessDestroyBadStore(pVm,&sFile,"session_decode");
		SyBlobRelease(&sFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "session_status",        vm_builtin_session_status        },
		{ "session_id",            vm_builtin_session_id            },
		{ "session_name",          vm_builtin_session_name          },
		{ "session_save_path",     vm_builtin_session_save_path     },
		{ "session_start",         vm_builtin_session_start         },
		{ "session_write_close",   vm_builtin_session_write_close   },
		{ "session_commit",        vm_builtin_session_write_close   },
		{ "session_abort",         vm_builtin_session_abort         },
		{ "session_reset",         vm_builtin_session_reset         },
		{ "session_unset",         vm_builtin_session_unset         },
		{ "session_destroy",       vm_builtin_session_destroy       },
		{ "session_regenerate_id", vm_builtin_session_regenerate_id },
		{ "session_encode",        vm_builtin_session_encode        },
		{ "session_decode",        vm_builtin_session_decode        },
		{ "session_create_id",     vm_builtin_session_create_id     },
		{ "session_get_cookie_params", vm_builtin_session_get_cookie_params },
		{ "session_set_cookie_params", vm_builtin_session_set_cookie_params },
	};
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	return SXRET_OK;
}

#endif /* PH7_DISABLE_DISK_IO */
#endif /* PH7_DISABLE_BUILTIN_FUNC */

#if defined(PH7_DISABLE_BUILTIN_FUNC) || defined(PH7_DISABLE_DISK_IO)
/* Tiny build: no sessions (builtin funcs / disk IO disabled) */
PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }
PH7_PRIVATE void PH7_VmSessionShutdown(ph7_vm *pVm){ (void)pVm; }
#endif
