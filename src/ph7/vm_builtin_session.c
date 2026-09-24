/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifndef PH7_DISABLE_BUILTIN_FUNC
#ifndef PH7_DISABLE_DISK_IO
/*
 * Sessions: file-backed session_* functions over the $_SESSION superglobal.
 *
 * This was an embedded-PHP chunk holding its state on a private `__SessS` class
 * with five static properties, plus five `__sess_*` PHP helpers. All six names
 * are gone: the state is on the VM (pVm->iSessStatus / sSessId / sSessName /
 * sSessPath / bSessWired) and the functions are these C routines. Moving the
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
/* "<save_path>/sess_<id>" */
static void VmSessFile(ph7_vm *pVm,SyBlob *pOut)
{
	VmSessResolvePath(pVm);
	SyBlobReset(pOut);
	SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));
	SyBlobAppend(pOut,"/sess_",sizeof("/sess_")-1);
	SyBlobAppend(pOut,SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId));
}
/*
 * A fresh 32-char id over php's session-id alphabet (32 symbols, so 5 bits per
 * character taken from random bytes).
 */
static void VmSessGenId(ph7_vm *pVm,SyBlob *pOut)
{
	static const char zAlpha[] = "0123456789abcdefghijklmnopqrstuv";
	unsigned char zRaw[32];
	int i;
	SyBlobReset(pOut);
	SyRandomness(&pVm->sPrng,zRaw,sizeof(zRaw));
	for( i = 0 ; i < 32 ; i++ ){
		char c = zAlpha[zRaw[i] & 31];
		SyBlobAppend(pOut,&c,1);
	}
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
/* bool session_start(array $options = []) */
static int vm_builtin_session_start(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sFile;
	int iLoad;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
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
	if( SyBlobLength(&pVm->sSessId) == 0 ){
		/* Adopt the id the client sent, when it is one php would accept. */
		ph7_value *pCookie = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);
		int bAdopted = 0;
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
			if( sVal.iFlags & MEMOBJ_STRING ){
				const char *z = (const char *)SyBlobData(&sVal.sBlob);
				sxu32 n = SyBlobLength(&sVal.sBlob);
				sxu32 i;
				int bOk = n >= 1 && n <= 128;
				for( i = 0 ; bOk && i < n ; i++ ){
					char c = z[i];
					if( !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')
					   || (c >= 'A' && c <= 'Z') || c == ',' || c == '-') ){
						bOk = 0;
					}
				}
				if( bOk ){
					SyBlobReset(&pVm->sSessId);
					SyBlobAppend(&pVm->sSessId,z,n);
					bAdopted = 1;
				}
			}
			PH7_MemObjRelease(&sVal);
			PH7_MemObjRelease(&sKey);
		}
		if( !bAdopted ){
			VmSessGenId(pVm,&pVm->sSessId);
		}
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
	if( !pVm->bSessWired ){
		ph7_value sName,sId;
		ph7_value *apA[2];
		pVm->bSessWired = 1;
		VmSessStrArg(pVm,&sName,(const char *)SyBlobData(&pVm->sSessName),
			SyBlobLength(&pVm->sSessName));
		VmSessStrArg(pVm,&sId,(const char *)SyBlobData(&pVm->sSessId),
			SyBlobLength(&pVm->sSessId));
		apA[0] = &sName;
		apA[1] = &sId;
		VmSessCall(pVm,"setcookie",2,apA,0);
		PH7_MemObjRelease(&sName);
		PH7_MemObjRelease(&sId);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Write the open session back to its file and close it. Shared by the builtin and
 * by the request-shutdown writer, which is why it takes only the VM: at shutdown
 * there is no calling frame to report against.
 */
static void VmSessWrite(ph7_vm *pVm,const char *zWho)
{
	SyBlob sFile,sData;
	ph7_value sPath,sPayload;
	ph7_value *apA[2];
	SyBlobInit(&sFile,&pVm->sAllocator);
	SyBlobInit(&sData,&pVm->sAllocator);
	VmSessFile(pVm,&sFile);
	/* An encode php refused still gets written — as the EMPTY payload, which is
	 * what its store is handed when the serializer answers nothing. The session
	 * closes either way and the stale copy does not survive. */
	VmSessEncode(pVm,&sData,zWho);
	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(&sFile),SyBlobLength(&sFile));
	VmSessStrArg(pVm,&sPayload,(const char *)SyBlobData(&sData),SyBlobLength(&sData));
	apA[0] = &sPath;
	apA[1] = &sPayload;
	VmSessCall(pVm,"file_put_contents",2,apA,0);
	PH7_MemObjRelease(&sPath);
	PH7_MemObjRelease(&sPayload);
	SyBlobRelease(&sFile);
	SyBlobRelease(&sData);
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
	VmSessFile(pVm,&sFile);
	if( nArg > 0 && ph7_value_to_bool(apArg[0]) ){
		VmSessUnlinkIfExists(pVm,&sFile);
	}
	SyBlobRelease(&sFile);
	VmSessGenId(pVm,&pVm->sSessId);
	ph7_result_bool(pCtx,1);
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
