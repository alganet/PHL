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
 * Position just past ONE serialized value starting at nPos -- enough of php's
 * serialize grammar for a session payload, and the reason the decoder can find
 * the `key|value` boundaries at all: a value may contain '|' and braces.
 */
static sxu32 VmSessScanFragment(const char *zSrc,sxu32 nLen,sxu32 nPos)
{
	char c;
	if( nPos >= nLen ){
		return nLen;
	}
	c = zSrc[nPos];
	if( c == 'N' ){
		return nPos + 2 <= nLen ? nPos + 2 : nLen;
	}
	if( c == 'i' || c == 'd' || c == 'b' ){
		sxu32 i = nPos;
		while( i < nLen && zSrc[i] != ';' ){ i++; }
		return i < nLen ? i + 1 : nLen;
	}
	if( c == 's' ){
		/* s:<len>:"<len bytes>"; -- the byte count is authoritative, so embedded
		 * quotes and semicolons cannot confuse the scan. */
		sxu32 i = nPos + 2;
		sxi32 nStr = 0;
		sxu32 nStart = i;
		while( i < nLen && zSrc[i] != ':' ){ i++; }
		if( i > nStart ){
			SyStrToInt32(&zSrc[nStart],i - nStart,(void *)&nStr,0);
		}
		i += 2; /* ':' then the opening '"' */
		i += (sxu32)(nStr < 0 ? 0 : nStr);
		i += 2; /* closing '"' then ';' */
		return i > nLen ? nLen : i;
	}
	if( c == 'a' || c == 'O' ){
		sxu32 i = nPos;
		int iDepth = 1;
		while( i < nLen && zSrc[i] != '{' ){ i++; }
		if( i >= nLen ){
			return nLen;
		}
		i++;
		while( i < nLen && iDepth > 0 ){
			if( zSrc[i] == 's' && i + 1 < nLen && zSrc[i+1] == ':' ){
				/* skip a string wholesale so braces inside it do not count */
				i = VmSessScanFragment(zSrc,nLen,i);
				continue;
			}
			if( zSrc[i] == '{' ){
				iDepth++;
			}else if( zSrc[i] == '}' ){
				iDepth--;
			}
			i++;
		}
		return i;
	}
	{
		sxu32 i = nPos;
		while( i < nLen && zSrc[i] != ';' ){ i++; }
		return i < nLen ? i + 1 : nLen;
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
 * Replace $_SESSION with the decoded contents of a payload in php's "php"
 * handler format: a run of `key|<serialized value>` with no separators.
 */
static void VmSessDecodeInto(ph7_vm *pVm,const char *zSrc,sxu32 nLen,ph7_value *pDest)
{
	sxu32 nPos = 0;
	ph7_hashmap *pMap;
	PH7_MemObjRelease(pDest);
	pDest->x.pOther = PH7_NewHashmap(&(*pVm),0,0);
	if( pDest->x.pOther == 0 ){
		return;
	}
	MemObjSetType(pDest,MEMOBJ_HASHMAP);
	pMap = (ph7_hashmap *)pDest->x.pOther;
	while( nPos < nLen ){
		sxu32 nBar = nPos;
		sxu32 nEnd;
		ph7_value sKey,sFrag,sVal;
		while( nBar < nLen && zSrc[nBar] != '|' ){ nBar++; }
		if( nBar >= nLen ){
			break;
		}
		nEnd = VmSessScanFragment(zSrc,nLen,nBar + 1);
		VmSessStrArg(pVm,&sKey,&zSrc[nPos],nBar - nPos);
		VmSessStrArg(pVm,&sFrag,&zSrc[nBar + 1],nEnd - (nBar + 1));
		PH7_MemObjInit(pVm,&sVal);
		{
			ph7_value *apArg[1];
			apArg[0] = &sFrag;
			VmSessCall(pVm,"unserialize",1,apArg,&sVal);
		}
		PH7_HashmapInsert(pMap,&sKey,&sVal);
		PH7_MemObjRelease(&sKey);
		PH7_MemObjRelease(&sFrag);
		PH7_MemObjRelease(&sVal);
		nPos = nEnd;
	}
}
/* The inverse: every $_SESSION entry as `key|<serialized value>`. */
static void VmSessEncode(ph7_vm *pVm,SyBlob *pOut)
{
	ph7_value *pSess = VmSessArray(pVm);
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode;
	sxu32 n;
	SyBlobReset(pOut);
	if( pSess == 0 || (pSess->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return;
	}
	pMap = (ph7_hashmap *)pSess->x.pOther;
	pNode = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry && pNode ; n++ ){
		ph7_value sKey,sSer;
		ph7_value *pVal;
		PH7_MemObjInit(pVm,&sKey);
		PH7_HashmapExtractNodeKey(pNode,&sKey);
		PH7_MemObjToString(&sKey);
		SyBlobAppend(pOut,SyBlobData(&sKey.sBlob),SyBlobLength(&sKey.sBlob));
		SyBlobAppend(pOut,"|",1);
		PH7_MemObjInit(pVm,&sSer);
		pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
		if( pVal ){
			ph7_value *apArg[1];
			apArg[0] = pVal;
			VmSessCall(pVm,"serialize",1,apArg,&sSer);
			SyBlobAppend(pOut,SyBlobData(&sSer.sBlob),SyBlobLength(&sSer.sBlob));
		}
		PH7_MemObjRelease(&sKey);
		PH7_MemObjRelease(&sSer);
		pNode = pNode->pPrev;
	}
}
/* int session_status() */
static int vm_builtin_session_status(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int(pCtx,pCtx->pVm->iSessStatus);
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
	ph7_value sRes;
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
	PH7_MemObjInit(pVm,&sRes);
	{
		ph7_value *pSess = VmSessArray(pVm);
		int bRead = VmSessReadFile(pVm,&sFile,&sRes);
		if( pSess ){
			if( bRead ){
				VmSessDecodeInto(pVm,(const char *)SyBlobData(&sRes.sBlob),
					SyBlobLength(&sRes.sBlob),pSess);
			}else{
				/* No file yet: php starts with an empty session. */
				PH7_MemObjRelease(pSess);
				pSess->x.pOther = PH7_NewHashmap(pVm,0,0);
				if( pSess->x.pOther ){
					MemObjSetType(pSess,MEMOBJ_HASHMAP);
				}
			}
		}
	}
	PH7_MemObjRelease(&sRes);
	SyBlobRelease(&sFile);
	pVm->iSessStatus = VM_SESSION_ACTIVE;
	if( !pVm->bSessWired ){
		ph7_value sCb,sName,sId;
		ph7_value *apA[2];
		pVm->bSessWired = 1;
		VmSessStrArg(pVm,&sCb,"session_write_close",sizeof("session_write_close")-1);
		apA[0] = &sCb;
		VmSessCall(pVm,"register_shutdown_function",1,apA,0);
		PH7_MemObjRelease(&sCb);
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
/* bool session_write_close() / session_commit() */
static int vm_builtin_session_write_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sFile,sData;
	ph7_value sPath,sPayload;
	ph7_value *apA[2];
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sFile,&pVm->sAllocator);
	SyBlobInit(&sData,&pVm->sAllocator);
	VmSessFile(pVm,&sFile);
	VmSessEncode(pVm,&sData);
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
	ph7_result_bool(pCtx,1);
	return PH7_OK;
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
	ph7_value sRes;
	ph7_value *pSess;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sFile,&pVm->sAllocator);
	VmSessFile(pVm,&sFile);
	PH7_MemObjInit(pVm,&sRes);
	pSess = VmSessArray(pVm);
	if( pSess ){
		if( VmSessReadFile(pVm,&sFile,&sRes) ){
			VmSessDecodeInto(pVm,(const char *)SyBlobData(&sRes.sBlob),
				SyBlobLength(&sRes.sBlob),pSess);
		}else{
			PH7_MemObjRelease(pSess);
			pSess->x.pOther = PH7_NewHashmap(pVm,0,0);
			if( pSess->x.pOther ){
				MemObjSetType(pSess,MEMOBJ_HASHMAP);
			}
		}
	}
	PH7_MemObjRelease(&sRes);
	SyBlobRelease(&sFile);
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
#endif
