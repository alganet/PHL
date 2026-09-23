# src/ph7/vm_builtin_session.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 357/452 lines (78.98%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |    7 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - |    8 | `/*` |
|     - |    9 | ` * Sessions: file-backed session_* functions over the $_SESSION superglobal.` |
|     - |   10 | ` *` |
|     - |   11 | `` * This was an embedded-PHP chunk holding its state on a private `__SessS` class`` |
|     - |   12 | `` * with five static properties, plus five `__sess_*` PHP helpers. All six names`` |
|     - |   13 | ` * are gone: the state is on the VM (pVm->iSessStatus / sSessId / sSessName /` |
|     - |   14 | ` * sSessPath / bSessWired) and the functions are these C routines. Moving the` |
|     - |   15 | ` * state off a PHP class also decoupled the INI subsystem, which used to reach` |
|     - |   16 | `` * into `__SessS::$name` / `$path` to live-wire session.name / session.save_path`` |
|     - |   17 | ` * and now reads the same VM fields.` |
|     - |   18 | ` *` |
|     - |   19 | ` * Session files keep php's DEFAULT "php" serialize-handler format` |
|     - |   20 | `` * (`key\|<serialized>` runs), so they still interoperate with a stock php install`` |
|     - |   21 | ` * both ways. The primitives that format needs -- serialize/unserialize -- and the` |
|     - |   22 | ` * file IO are reached by CALLING the engine's own builtins by name rather than` |
|     - |   23 | ` * duplicating them here: one mechanism (VmSessCall) for all of them, which is` |
|     - |   24 | ` * also what keeps the stream-wrapper behaviour of file_get_contents/file_put_contents` |
|     - |   25 | ` * identical to what the chunk had.` |
|     - |   26 | ` */` |
|     - |   27 |  |
|     - |   28 | `/* php's session_status() values (PHP_SESSION_DISABLED is never reported here). */` |
|     - |   29 | `#define VM_SESSION_NONE   1` |
|     - |   30 | `#define VM_SESSION_ACTIVE 2` |
|     - |   31 |  |
|     - |   32 | `/*` |
|     - |   33 | ` * Call an engine builtin by name. The session logic is a thin layer over` |
|     - |   34 | ` * serialize/unserialize/file IO, and re-implementing those in C would fork` |
|     - |   35 | ` * behaviour that has to stay identical (stream wrappers, the serialize grammar).` |
|     - |   36 | ` */` |
|    28 |   37 | `static sxi32 VmSessCall(ph7_vm *pVm,const char *zFunc,int nArg,ph7_value **apArg,ph7_value *pResult)` |
|     1 |   38 | `{` |
|     - |   39 | `	ph7_value sName;` |
|     - |   40 | `	SyString sStr;` |
|     - |   41 | `	sxi32 rc;` |
|    29 |   42 | `	PH7_MemObjInit(pVm,&sName);` |
|    29 |   43 | `	SyStringInitFromBuf(&sStr,zFunc,SyStrlen(zFunc));` |
|    29 |   44 | `	PH7_MemObjInitFromString(pVm,&sName,&sStr);` |
|    29 |   45 | `	rc = PH7_VmCallUserFunction(&(*pVm),&sName,nArg,apArg,pResult);` |
|    29 |   46 | `	PH7_MemObjRelease(&sName);` |
|    29 |   47 | `	return rc;` |
|     1 |   48 | `}` |
|    30 |   49 | `static void VmSessStrArg(ph7_vm *pVm,ph7_value *pOut,const char *zVal,sxu32 nVal)` |
|     1 |   50 | `{` |
|     - |   51 | `	SyString sStr;` |
|    31 |   52 | `	PH7_MemObjInit(pVm,pOut);` |
|    31 |   53 | `	SyStringInitFromBuf(&sStr,zVal,nVal);` |
|    31 |   54 | `	PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|    31 |   55 | `}` |
|     - |   56 | `/*` |
|     - |   57 | ` * The save path, resolving the lazy default the way the chunk did: an unset path` |
|     - |   58 | ` * becomes sys_get_temp_dir() with any trailing slash removed, and is remembered.` |
|     - |   59 | ` */` |
|    12 |   60 | `static void VmSessResolvePath(ph7_vm *pVm)` |
|     1 |   61 | `{` |
|     - |   62 | `	ph7_value sRes;` |
|    13 |   63 | `	if( SyBlobLength(&pVm->sSessPath) > 0 ){` |
|    11 |   64 | `		return;` |
|     - |   65 | `	}` |
|     3 |   66 | `	PH7_MemObjInit(pVm,&sRes);` |
|     3 |   67 | `	if( VmSessCall(pVm,"sys_get_temp_dir",0,0,&sRes) == SXRET_OK ){` |
|     3 |   68 | `		const char *zTmp = (const char *)SyBlobData(&sRes.sBlob);` |
|     3 |   69 | `		sxu32 nTmp = SyBlobLength(&sRes.sBlob);` |
|     3 |   70 | `		while( nTmp > 0 && zTmp[nTmp-1] == '/' ){ nTmp--; }` |
|     3 |   71 | `		SyBlobReset(&pVm->sSessPath);` |
|     3 |   72 | `		SyBlobAppend(&pVm->sSessPath,zTmp,nTmp);` |
|     1 |   73 | `	}` |
|     3 |   74 | `	PH7_MemObjRelease(&sRes);` |
|     7 |   75 | `}` |
|     - |   76 | `/* "<save_path>/sess_<id>" */` |
|    10 |   77 | `static void VmSessFile(ph7_vm *pVm,SyBlob *pOut)` |
|     1 |   78 | `{` |
|    11 |   79 | `	VmSessResolvePath(pVm);` |
|    11 |   80 | `	SyBlobReset(pOut);` |
|    11 |   81 | `	SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|    11 |   82 | `	SyBlobAppend(pOut,"/sess_",sizeof("/sess_")-1);` |
|    11 |   83 | `	SyBlobAppend(pOut,SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId));` |
|    11 |   84 | `}` |
|     - |   85 | `/*` |
|     - |   86 | ` * A fresh 32-char id over php's session-id alphabet (32 symbols, so 5 bits per` |
|     - |   87 | ` * character taken from random bytes).` |
|     - |   88 | ` */` |
|     4 |   89 | `static void VmSessGenId(ph7_vm *pVm,SyBlob *pOut)` |
|     1 |   90 | `{` |
|     - |   91 | `	static const char zAlpha[] = "0123456789abcdefghijklmnopqrstuv";` |
|     - |   92 | `	unsigned char zRaw[32];` |
|     - |   93 | `	int i;` |
|     5 |   94 | `	SyBlobReset(pOut);` |
|     5 |   95 | `	SyRandomness(&pVm->sPrng,zRaw,sizeof(zRaw));` |
|   133 |   96 | `	for( i = 0 ; i < 32 ; i++ ){` |
|   129 |   97 | `		char c = zAlpha[zRaw[i] & 31];` |
|   129 |   98 | `		SyBlobAppend(pOut,&c,1);` |
|    65 |   99 | `	}` |
|     5 |  100 | `}` |
|     - |  101 | `/*` |
|     - |  102 | ` * Position just past ONE serialized value starting at nPos -- enough of php's` |
|     - |  103 | ` * serialize grammar for a session payload, and the reason the decoder can find` |
|     - |  104 | `` * the `key\|value` boundaries at all: a value may contain '\|' and braces.`` |
|     - |  105 | ` */` |
|     8 |  106 | `static sxu32 VmSessScanFragment(const char *zSrc,sxu32 nLen,sxu32 nPos)` |
|     1 |  107 | `{` |
|     - |  108 | `	char c;` |
|     9 |  109 | `	if( nPos >= nLen ){` |
|   ! 0 |  110 | `		return nLen;` |
|     - |  111 | `	}` |
|     9 |  112 | `	c = zSrc[nPos];` |
|     9 |  113 | `	if( c == 'N' ){` |
|   ! 0 |  114 | `		return nPos + 2 <= nLen ? nPos + 2 : nLen;` |
|     - |  115 | `	}` |
|     9 |  116 | `	if( c == 'i' \|\| c == 'd' \|\| c == 'b' ){` |
|     3 |  117 | `		sxu32 i = nPos;` |
|    11 |  118 | `		while( i < nLen && zSrc[i] != ';' ){ i++; }` |
|     3 |  119 | `		return i < nLen ? i + 1 : nLen;` |
|     - |  120 | `	}` |
|     7 |  121 | `	if( c == 's' ){` |
|     - |  122 | `		/* s:<len>:"<len bytes>"; -- the byte count is authoritative, so embedded` |
|     - |  123 | `		 * quotes and semicolons cannot confuse the scan. */` |
|     5 |  124 | `		sxu32 i = nPos + 2;` |
|     5 |  125 | `		sxi32 nStr = 0;` |
|     5 |  126 | `		sxu32 nStart = i;` |
|     9 |  127 | `		while( i < nLen && zSrc[i] != ':' ){ i++; }` |
|     5 |  128 | `		if( i > nStart ){` |
|     5 |  129 | `			SyStrToInt32(&zSrc[nStart],i - nStart,(void *)&nStr,0);` |
|     2 |  130 | `		}` |
|     5 |  131 | `		i += 2; /* ':' then the opening '"' */` |
|     5 |  132 | `		i += (sxu32)(nStr < 0 ? 0 : nStr);` |
|     5 |  133 | `		i += 2; /* closing '"' then ';' */` |
|     5 |  134 | `		return i > nLen ? nLen : i;` |
|     - |  135 | `	}` |
|     3 |  136 | `	if( c == 'a' \|\| c == 'O' ){` |
|     3 |  137 | `		sxu32 i = nPos;` |
|     3 |  138 | `		int iDepth = 1;` |
|    11 |  139 | `		while( i < nLen && zSrc[i] != '{' ){ i++; }` |
|     3 |  140 | `		if( i >= nLen ){` |
|   ! 0 |  141 | `			return nLen;` |
|     - |  142 | `		}` |
|     3 |  143 | `		i++;` |
|    31 |  144 | `		while( i < nLen && iDepth > 0 ){` |
|    29 |  145 | `			if( zSrc[i] == 's' && i + 1 < nLen && zSrc[i+1] == ':' ){` |
|     - |  146 | `				/* skip a string wholesale so braces inside it do not count */` |
|     3 |  147 | `				i = VmSessScanFragment(zSrc,nLen,i);` |
|     3 |  148 | `				continue;` |
|     - |  149 | `			}` |
|    27 |  150 | `			if( zSrc[i] == '{' ){` |
|   ! 0 |  151 | `				iDepth++;` |
|    27 |  152 | `			}else if( zSrc[i] == '}' ){` |
|     3 |  153 | `				iDepth--;` |
|     1 |  154 | `			}` |
|    27 |  155 | `			i++;` |
|     1 |  156 | `		}` |
|     3 |  157 | `		return i;` |
|     - |  158 | `	}` |
|     - |  159 | `	{` |
|   ! 0 |  160 | `		sxu32 i = nPos;` |
|   ! 0 |  161 | `		while( i < nLen && zSrc[i] != ';' ){ i++; }` |
|   ! 0 |  162 | `		return i < nLen ? i + 1 : nLen;` |
|     - |  163 | `	}` |
|     5 |  164 | `}` |
|     - |  165 | `/*` |
|     - |  166 | ` * Read the session file into pOut, answering FALSE when there is none.` |
|     - |  167 | ` *` |
|     - |  168 | ` * The existence check is not an optimization: file_get_contents() warns on a` |
|     - |  169 | ` * missing path, and a first-ever session_start() has no file yet -- the PHP chunk` |
|     - |  170 | ` * guarded the read with file_exists() for exactly this reason.` |
|     - |  171 | ` */` |
|     4 |  172 | `static int VmSessReadFile(ph7_vm *pVm,SyBlob *pFile,ph7_value *pOut)` |
|     1 |  173 | `{` |
|     - |  174 | `	ph7_value sPath,sExists;` |
|     - |  175 | `	ph7_value *apA[1];` |
|     - |  176 | `	int bOk;` |
|     5 |  177 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));` |
|     5 |  178 | `	PH7_MemObjInit(pVm,&sExists);` |
|     5 |  179 | `	apA[0] = &sPath;` |
|     5 |  180 | `	VmSessCall(pVm,"file_exists",1,apA,&sExists);` |
|     - |  181 | `	/* PH7_MemObjToBool converts IN PLACE and answers a STATUS, not the boolean --` |
|     - |  182 | `	 * the value lands in x.iVal. Reading its return as the answer makes every` |
|     - |  183 | `	 * existence check false, which silently empties the session on every reload. */` |
|     5 |  184 | `	PH7_MemObjToBool(&sExists);` |
|     5 |  185 | `	bOk = sExists.x.iVal != 0;` |
|     5 |  186 | `	PH7_MemObjRelease(&sExists);` |
|     5 |  187 | `	if( bOk ){` |
|     3 |  188 | `		VmSessCall(pVm,"file_get_contents",1,apA,pOut);` |
|     3 |  189 | `		bOk = (pOut->iFlags & MEMOBJ_STRING) != 0;` |
|     1 |  190 | `	}` |
|     5 |  191 | `	PH7_MemObjRelease(&sPath);` |
|     5 |  192 | `	return bOk;` |
|     1 |  193 | `}` |
|     - |  194 | `/*` |
|     - |  195 | ` * Delete the session file if it is there. Same reason the read is guarded:` |
|     - |  196 | ` * unlink() warns on a missing path, and destroying a session that was never` |
|     - |  197 | ` * written (or regenerating an id before the first write) is normal.` |
|     - |  198 | ` */` |
|     2 |  199 | `static void VmSessUnlinkIfExists(ph7_vm *pVm,SyBlob *pFile)` |
|     1 |  200 | `{` |
|     - |  201 | `	ph7_value sPath,sExists;` |
|     - |  202 | `	ph7_value *apA[1];` |
|     3 |  203 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));` |
|     3 |  204 | `	PH7_MemObjInit(pVm,&sExists);` |
|     3 |  205 | `	apA[0] = &sPath;` |
|     3 |  206 | `	VmSessCall(pVm,"file_exists",1,apA,&sExists);` |
|     3 |  207 | `	PH7_MemObjToBool(&sExists);` |
|     3 |  208 | `	if( sExists.x.iVal ){` |
|   ! 0 |  209 | `		VmSessCall(pVm,"unlink",1,apA,0);` |
|   ! 0 |  210 | `	}` |
|     3 |  211 | `	PH7_MemObjRelease(&sExists);` |
|     3 |  212 | `	PH7_MemObjRelease(&sPath);` |
|     3 |  213 | `}` |
|     - |  214 | `/* $_SESSION, or NULL when the superglobal is somehow absent. */` |
|     6 |  215 | `static ph7_value * VmSessArray(ph7_vm *pVm)` |
|     1 |  216 | `{` |
|     7 |  217 | `	return PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     1 |  218 | `}` |
|     - |  219 | `/*` |
|     - |  220 | ` * Replace $_SESSION with the decoded contents of a payload in php's "php"` |
|     - |  221 | `` * handler format: a run of `key\|<serialized value>` with no separators.`` |
|     - |  222 | ` */` |
|     2 |  223 | `static void VmSessDecodeInto(ph7_vm *pVm,const char *zSrc,sxu32 nLen,ph7_value *pDest)` |
|     1 |  224 | `{` |
|     3 |  225 | `	sxu32 nPos = 0;` |
|     - |  226 | `	ph7_hashmap *pMap;` |
|     3 |  227 | `	PH7_MemObjRelease(pDest);` |
|     3 |  228 | `	pDest->x.pOther = PH7_NewHashmap(&(*pVm),0,0);` |
|     3 |  229 | `	if( pDest->x.pOther == 0 ){` |
|   ! 0 |  230 | `		return;` |
|     - |  231 | `	}` |
|     3 |  232 | `	MemObjSetType(pDest,MEMOBJ_HASHMAP);` |
|     3 |  233 | `	pMap = (ph7_hashmap *)pDest->x.pOther;` |
|     9 |  234 | `	while( nPos < nLen ){` |
|     7 |  235 | `		sxu32 nBar = nPos;` |
|     - |  236 | `		sxu32 nEnd;` |
|     - |  237 | `		ph7_value sKey,sFrag,sVal;` |
|    23 |  238 | `		while( nBar < nLen && zSrc[nBar] != '\|' ){ nBar++; }` |
|     7 |  239 | `		if( nBar >= nLen ){` |
|   ! 0 |  240 | `			break;` |
|     - |  241 | `		}` |
|     7 |  242 | `		nEnd = VmSessScanFragment(zSrc,nLen,nBar + 1);` |
|     7 |  243 | `		VmSessStrArg(pVm,&sKey,&zSrc[nPos],nBar - nPos);` |
|     7 |  244 | `		VmSessStrArg(pVm,&sFrag,&zSrc[nBar + 1],nEnd - (nBar + 1));` |
|     7 |  245 | `		PH7_MemObjInit(pVm,&sVal);` |
|     - |  246 | `		{` |
|     - |  247 | `			ph7_value *apArg[1];` |
|     7 |  248 | `			apArg[0] = &sFrag;` |
|     7 |  249 | `			VmSessCall(pVm,"unserialize",1,apArg,&sVal);` |
|     - |  250 | `		}` |
|     7 |  251 | `		PH7_HashmapInsert(pMap,&sKey,&sVal);` |
|     7 |  252 | `		PH7_MemObjRelease(&sKey);` |
|     7 |  253 | `		PH7_MemObjRelease(&sFrag);` |
|     7 |  254 | `		PH7_MemObjRelease(&sVal);` |
|     7 |  255 | `		nPos = nEnd;` |
|     1 |  256 | `	}` |
|     2 |  257 | `}` |
|     - |  258 | ``/* The inverse: every $_SESSION entry as `key\|<serialized value>`. */`` |
|     2 |  259 | `static void VmSessEncode(ph7_vm *pVm,SyBlob *pOut)` |
|     1 |  260 | `{` |
|     3 |  261 | `	ph7_value *pSess = VmSessArray(pVm);` |
|     - |  262 | `	ph7_hashmap *pMap;` |
|     - |  263 | `	ph7_hashmap_node *pNode;` |
|     - |  264 | `	sxu32 n;` |
|     3 |  265 | `	SyBlobReset(pOut);` |
|     3 |  266 | `	if( pSess == 0 \|\| (pSess->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  267 | `		return;` |
|     - |  268 | `	}` |
|     3 |  269 | `	pMap = (ph7_hashmap *)pSess->x.pOther;` |
|     3 |  270 | `	pNode = pMap->pFirst;` |
|     9 |  271 | `	for( n = 0 ; n < pMap->nEntry && pNode ; n++ ){` |
|     - |  272 | `		ph7_value sKey,sSer;` |
|     - |  273 | `		ph7_value *pVal;` |
|     7 |  274 | `		PH7_MemObjInit(pVm,&sKey);` |
|     7 |  275 | `		PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|     7 |  276 | `		PH7_MemObjToString(&sKey);` |
|     7 |  277 | `		SyBlobAppend(pOut,SyBlobData(&sKey.sBlob),SyBlobLength(&sKey.sBlob));` |
|     7 |  278 | `		SyBlobAppend(pOut,"\|",1);` |
|     7 |  279 | `		PH7_MemObjInit(pVm,&sSer);` |
|     7 |  280 | `		pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     7 |  281 | `		if( pVal ){` |
|     - |  282 | `			ph7_value *apArg[1];` |
|     7 |  283 | `			apArg[0] = pVal;` |
|     7 |  284 | `			VmSessCall(pVm,"serialize",1,apArg,&sSer);` |
|     7 |  285 | `			SyBlobAppend(pOut,SyBlobData(&sSer.sBlob),SyBlobLength(&sSer.sBlob));` |
|     3 |  286 | `		}` |
|     7 |  287 | `		PH7_MemObjRelease(&sKey);` |
|     7 |  288 | `		PH7_MemObjRelease(&sSer);` |
|     7 |  289 | `		pNode = pNode->pPrev;` |
|     4 |  290 | `	}` |
|     2 |  291 | `}` |
|     - |  292 | `/* int session_status() */` |
|     8 |  293 | `static int vm_builtin_session_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  294 | `{` |
|     4 |  295 | `	SXUNUSED(nArg);` |
|     4 |  296 | `	SXUNUSED(apArg);` |
|     9 |  297 | `	ph7_result_int(pCtx,pCtx->pVm->iSessStatus);` |
|     9 |  298 | `	return PH7_OK;` |
|     1 |  299 | `}` |
|     - |  300 | `/*` |
|     - |  301 | ` * The three "cannot change while active / after headers" guards php puts on the` |
|     - |  302 | ` * id, the name and the save path. Answers TRUE (warning already raised) when the` |
|     - |  303 | ` * write must be refused.` |
|     - |  304 | ` */` |
|     4 |  305 | `static int VmSessLocked(ph7_context *pCtx,const char *zFunc,const char *zWhat,int bHeaders)` |
|     1 |  306 | `{` |
|     5 |  307 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  308 | `	char zMsg[192];` |
|     5 |  309 | `	if( pVm->iSessStatus == VM_SESSION_ACTIVE ){` |
|   ! 0 |  310 | `		SyBufferFormat(zMsg,sizeof(zMsg),"%s(): %s cannot be changed when a session is active",` |
|   ! 0 |  311 | `			zFunc,zWhat);` |
|   ! 0 |  312 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|   ! 0 |  313 | `		return 1;` |
|     - |  314 | `	}` |
|     5 |  315 | `	if( bHeaders && pVm->bHeadersSent ){` |
|   ! 0 |  316 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|   ! 0 |  317 | `			"%s(): %s cannot be changed after headers have already been sent",zFunc,zWhat);` |
|   ! 0 |  318 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|   ! 0 |  319 | `		return 1;` |
|     - |  320 | `	}` |
|     5 |  321 | `	return 0;` |
|     3 |  322 | `}` |
|     - |  323 | `/*` |
|     - |  324 | ` * session_id() / session_name() / session_save_path(): read the current value,` |
|     - |  325 | ` * or set it and answer the previous one. One body, three directives.` |
|     - |  326 | ` */` |
|    24 |  327 | `static int VmSessAccessor(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|     - |  328 | `	SyBlob *pSlot,const char *zFunc,const char *zWhat,int bRtrimSlash)` |
|     2 |  329 | `{` |
|    26 |  330 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  331 | `	SyBlob sOld;` |
|    26 |  332 | `	if( pSlot == &pVm->sSessPath ){` |
|     3 |  333 | `		VmSessResolvePath(pVm);` |
|     1 |  334 | `	}` |
|    26 |  335 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|    26 |  336 | `	SyBlobAppend(&sOld,SyBlobData(pSlot),SyBlobLength(pSlot));` |
|    26 |  337 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|    22 |  338 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|    22 |  339 | `		SyBlobRelease(&sOld);` |
|    22 |  340 | `		return PH7_OK;` |
|     - |  341 | `	}` |
|     5 |  342 | `	if( VmSessLocked(pCtx,zFunc,zWhat,pSlot != &pVm->sSessPath) ){` |
|   ! 0 |  343 | `		SyBlobRelease(&sOld);` |
|   ! 0 |  344 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  345 | `		return PH7_OK;` |
|     - |  346 | `	}` |
|     - |  347 | `	{` |
|     5 |  348 | `		int nNew = 0;` |
|     5 |  349 | `		const char *zNew = ph7_value_to_string(apArg[0],&nNew);` |
|     5 |  350 | `		sxu32 nLen = (sxu32)nNew;` |
|     5 |  351 | `		if( bRtrimSlash ){` |
|     3 |  352 | `			while( nLen > 0 && zNew[nLen-1] == '/' ){ nLen--; }` |
|     1 |  353 | `		}` |
|     5 |  354 | `		SyBlobReset(pSlot);` |
|     5 |  355 | `		SyBlobAppend(pSlot,zNew,nLen);` |
|     - |  356 | `	}` |
|     5 |  357 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|     5 |  358 | `	SyBlobRelease(&sOld);` |
|     5 |  359 | `	return PH7_OK;` |
|    14 |  360 | `}` |
|    16 |  361 | `static int vm_builtin_session_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  362 | `{` |
|    17 |  363 | `	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessId,` |
|     - |  364 | `		"session_id","Session ID",0);` |
|     1 |  365 | `}` |
|     6 |  366 | `static int vm_builtin_session_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  367 | `{` |
|     8 |  368 | `	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessName,` |
|     - |  369 | `		"session_name","Session name",0);` |
|     2 |  370 | `}` |
|     2 |  371 | `static int vm_builtin_session_save_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  372 | `{` |
|     3 |  373 | `	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessPath,` |
|     - |  374 | `		"session_save_path","Session save path",1);` |
|     1 |  375 | `}` |
|     - |  376 | `/* bool session_start(array $options = []) */` |
|     6 |  377 | `static int vm_builtin_session_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  378 | `{` |
|     8 |  379 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  380 | `	SyBlob sFile;` |
|     - |  381 | `	ph7_value sRes;` |
|     3 |  382 | `	SXUNUSED(nArg);` |
|     3 |  383 | `	SXUNUSED(apArg);` |
|     8 |  384 | `	if( pVm->iSessStatus == VM_SESSION_ACTIVE ){` |
|   ! 0 |  385 | `		PH7_VmThrowError(pVm,0,PH7_CTX_NOTICE,` |
|     - |  386 | `			"session_start(): Ignoring session_start() because a session is already active");` |
|   ! 0 |  387 | `		ph7_result_bool(pCtx,1);` |
|   ! 0 |  388 | `		return PH7_OK;` |
|     - |  389 | `	}` |
|     8 |  390 | `	if( pVm->bHeadersSent ){` |
|     3 |  391 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|     - |  392 | `			"session_start(): Session cannot be started after headers have already been sent");` |
|     3 |  393 | `		ph7_result_bool(pCtx,0);` |
|     3 |  394 | `		return PH7_OK;` |
|     - |  395 | `	}` |
|     5 |  396 | `	if( SyBlobLength(&pVm->sSessId) == 0 ){` |
|     - |  397 | `		/* Adopt the id the client sent, when it is one php would accept. */` |
|     3 |  398 | `		ph7_value *pCookie = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|     3 |  399 | `		int bAdopted = 0;` |
|     3 |  400 | `		if( pCookie && (pCookie->iFlags & MEMOBJ_HASHMAP) ){` |
|     - |  401 | `			ph7_value sKey,sVal;` |
|     3 |  402 | `			ph7_hashmap_node *pNode = 0;` |
|     4 |  403 | `			VmSessStrArg(pVm,&sKey,(const char *)SyBlobData(&pVm->sSessName),` |
|     1 |  404 | `				SyBlobLength(&pVm->sSessName));` |
|     3 |  405 | `			PH7_MemObjInit(pVm,&sVal);` |
|     2 |  406 | `			if( PH7_HashmapLookup((ph7_hashmap *)pCookie->x.pOther,&sKey,&pNode) == SXRET_OK` |
|     2 |  407 | `			 && pNode ){` |
|   ! 0 |  408 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|   ! 0 |  409 | `			}` |
|     3 |  410 | `			if( sVal.iFlags & MEMOBJ_STRING ){` |
|   ! 0 |  411 | `				const char *z = (const char *)SyBlobData(&sVal.sBlob);` |
|   ! 0 |  412 | `				sxu32 n = SyBlobLength(&sVal.sBlob);` |
|     - |  413 | `				sxu32 i;` |
|   ! 0 |  414 | `				int bOk = n >= 1 && n <= 128;` |
|   ! 0 |  415 | `				for( i = 0 ; bOk && i < n ; i++ ){` |
|   ! 0 |  416 | `					char c = z[i];` |
|   ! 0 |  417 | `					if( !((c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z')` |
|   ! 0 |  418 | `					   \|\| (c >= 'A' && c <= 'Z') \|\| c == ',' \|\| c == '-') ){` |
|   ! 0 |  419 | `						bOk = 0;` |
|   ! 0 |  420 | `					}` |
|   ! 0 |  421 | `				}` |
|   ! 0 |  422 | `				if( bOk ){` |
|   ! 0 |  423 | `					SyBlobReset(&pVm->sSessId);` |
|   ! 0 |  424 | `					SyBlobAppend(&pVm->sSessId,z,n);` |
|   ! 0 |  425 | `					bAdopted = 1;` |
|   ! 0 |  426 | `				}` |
|   ! 0 |  427 | `			}` |
|     3 |  428 | `			PH7_MemObjRelease(&sVal);` |
|     3 |  429 | `			PH7_MemObjRelease(&sKey);` |
|     1 |  430 | `		}` |
|     3 |  431 | `		if( !bAdopted ){` |
|     3 |  432 | `			VmSessGenId(pVm,&pVm->sSessId);` |
|     1 |  433 | `		}` |
|     1 |  434 | `	}` |
|     5 |  435 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|     5 |  436 | `	VmSessFile(pVm,&sFile);` |
|     5 |  437 | `	PH7_MemObjInit(pVm,&sRes);` |
|     - |  438 | `	{` |
|     5 |  439 | `		ph7_value *pSess = VmSessArray(pVm);` |
|     5 |  440 | `		int bRead = VmSessReadFile(pVm,&sFile,&sRes);` |
|     5 |  441 | `		if( pSess ){` |
|     5 |  442 | `			if( bRead ){` |
|     4 |  443 | `				VmSessDecodeInto(pVm,(const char *)SyBlobData(&sRes.sBlob),` |
|     1 |  444 | `					SyBlobLength(&sRes.sBlob),pSess);` |
|     2 |  445 | `			}else{` |
|     - |  446 | `				/* No file yet: php starts with an empty session. */` |
|     3 |  447 | `				PH7_MemObjRelease(pSess);` |
|     3 |  448 | `				pSess->x.pOther = PH7_NewHashmap(pVm,0,0);` |
|     3 |  449 | `				if( pSess->x.pOther ){` |
|     3 |  450 | `					MemObjSetType(pSess,MEMOBJ_HASHMAP);` |
|     1 |  451 | `				}` |
|     - |  452 | `			}` |
|     2 |  453 | `		}` |
|     - |  454 | `	}` |
|     5 |  455 | `	PH7_MemObjRelease(&sRes);` |
|     5 |  456 | `	SyBlobRelease(&sFile);` |
|     5 |  457 | `	pVm->iSessStatus = VM_SESSION_ACTIVE;` |
|     5 |  458 | `	if( !pVm->bSessWired ){` |
|     - |  459 | `		ph7_value sCb,sName,sId;` |
|     - |  460 | `		ph7_value *apA[2];` |
|     3 |  461 | `		pVm->bSessWired = 1;` |
|     3 |  462 | `		VmSessStrArg(pVm,&sCb,"session_write_close",sizeof("session_write_close")-1);` |
|     3 |  463 | `		apA[0] = &sCb;` |
|     3 |  464 | `		VmSessCall(pVm,"register_shutdown_function",1,apA,0);` |
|     3 |  465 | `		PH7_MemObjRelease(&sCb);` |
|     4 |  466 | `		VmSessStrArg(pVm,&sName,(const char *)SyBlobData(&pVm->sSessName),` |
|     1 |  467 | `			SyBlobLength(&pVm->sSessName));` |
|     4 |  468 | `		VmSessStrArg(pVm,&sId,(const char *)SyBlobData(&pVm->sSessId),` |
|     1 |  469 | `			SyBlobLength(&pVm->sSessId));` |
|     3 |  470 | `		apA[0] = &sName;` |
|     3 |  471 | `		apA[1] = &sId;` |
|     3 |  472 | `		VmSessCall(pVm,"setcookie",2,apA,0);` |
|     3 |  473 | `		PH7_MemObjRelease(&sName);` |
|     3 |  474 | `		PH7_MemObjRelease(&sId);` |
|     1 |  475 | `	}` |
|     5 |  476 | `	ph7_result_bool(pCtx,1);` |
|     5 |  477 | `	return PH7_OK;` |
|     5 |  478 | `}` |
|     - |  479 | `/* bool session_write_close() / session_commit() */` |
|     4 |  480 | `static int vm_builtin_session_write_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  481 | `{` |
|     5 |  482 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  483 | `	SyBlob sFile,sData;` |
|     - |  484 | `	ph7_value sPath,sPayload;` |
|     - |  485 | `	ph7_value *apA[2];` |
|     2 |  486 | `	SXUNUSED(nArg);` |
|     2 |  487 | `	SXUNUSED(apArg);` |
|     5 |  488 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|     3 |  489 | `		ph7_result_bool(pCtx,0);` |
|     3 |  490 | `		return PH7_OK;` |
|     - |  491 | `	}` |
|     3 |  492 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|     3 |  493 | `	SyBlobInit(&sData,&pVm->sAllocator);` |
|     3 |  494 | `	VmSessFile(pVm,&sFile);` |
|     3 |  495 | `	VmSessEncode(pVm,&sData);` |
|     3 |  496 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(&sFile),SyBlobLength(&sFile));` |
|     3 |  497 | `	VmSessStrArg(pVm,&sPayload,(const char *)SyBlobData(&sData),SyBlobLength(&sData));` |
|     3 |  498 | `	apA[0] = &sPath;` |
|     3 |  499 | `	apA[1] = &sPayload;` |
|     3 |  500 | `	VmSessCall(pVm,"file_put_contents",2,apA,0);` |
|     3 |  501 | `	PH7_MemObjRelease(&sPath);` |
|     3 |  502 | `	PH7_MemObjRelease(&sPayload);` |
|     3 |  503 | `	SyBlobRelease(&sFile);` |
|     3 |  504 | `	SyBlobRelease(&sData);` |
|     3 |  505 | `	pVm->iSessStatus = VM_SESSION_NONE;` |
|     3 |  506 | `	ph7_result_bool(pCtx,1);` |
|     3 |  507 | `	return PH7_OK;` |
|     3 |  508 | `}` |
|     - |  509 | `/* bool session_abort() — drop the in-memory session without writing it back */` |
|   ! 0 |  510 | `static int vm_builtin_session_abort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  511 | `{` |
|   ! 0 |  512 | `	SXUNUSED(nArg);` |
|   ! 0 |  513 | `	SXUNUSED(apArg);` |
|   ! 0 |  514 | `	if( pCtx->pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|   ! 0 |  515 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  516 | `		return PH7_OK;` |
|     - |  517 | `	}` |
|   ! 0 |  518 | `	pCtx->pVm->iSessStatus = VM_SESSION_NONE;` |
|   ! 0 |  519 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 |  520 | `	return PH7_OK;` |
|   ! 0 |  521 | `}` |
|     - |  522 | `/* bool session_reset() — re-read the stored copy over the in-memory one */` |
|   ! 0 |  523 | `static int vm_builtin_session_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  524 | `{` |
|   ! 0 |  525 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  526 | `	SyBlob sFile;` |
|     - |  527 | `	ph7_value sRes;` |
|     - |  528 | `	ph7_value *pSess;` |
|   ! 0 |  529 | `	SXUNUSED(nArg);` |
|   ! 0 |  530 | `	SXUNUSED(apArg);` |
|   ! 0 |  531 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|   ! 0 |  532 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  533 | `		return PH7_OK;` |
|     - |  534 | `	}` |
|   ! 0 |  535 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|   ! 0 |  536 | `	VmSessFile(pVm,&sFile);` |
|   ! 0 |  537 | `	PH7_MemObjInit(pVm,&sRes);` |
|   ! 0 |  538 | `	pSess = VmSessArray(pVm);` |
|   ! 0 |  539 | `	if( pSess ){` |
|   ! 0 |  540 | `		if( VmSessReadFile(pVm,&sFile,&sRes) ){` |
|   ! 0 |  541 | `			VmSessDecodeInto(pVm,(const char *)SyBlobData(&sRes.sBlob),` |
|   ! 0 |  542 | `				SyBlobLength(&sRes.sBlob),pSess);` |
|   ! 0 |  543 | `		}else{` |
|   ! 0 |  544 | `			PH7_MemObjRelease(pSess);` |
|   ! 0 |  545 | `			pSess->x.pOther = PH7_NewHashmap(pVm,0,0);` |
|   ! 0 |  546 | `			if( pSess->x.pOther ){` |
|   ! 0 |  547 | `				MemObjSetType(pSess,MEMOBJ_HASHMAP);` |
|   ! 0 |  548 | `			}` |
|     - |  549 | `		}` |
|   ! 0 |  550 | `	}` |
|   ! 0 |  551 | `	PH7_MemObjRelease(&sRes);` |
|   ! 0 |  552 | `	SyBlobRelease(&sFile);` |
|   ! 0 |  553 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 |  554 | `	return PH7_OK;` |
|   ! 0 |  555 | `}` |
|     - |  556 | `/* bool session_unset() — empty $_SESSION, keep the session open */` |
|     2 |  557 | `static int vm_builtin_session_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  558 | `{` |
|     3 |  559 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  560 | `	ph7_value *pSess;` |
|     1 |  561 | `	SXUNUSED(nArg);` |
|     1 |  562 | `	SXUNUSED(apArg);` |
|     3 |  563 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|     3 |  564 | `		ph7_result_bool(pCtx,0);` |
|     3 |  565 | `		return PH7_OK;` |
|     - |  566 | `	}` |
|   ! 0 |  567 | `	pSess = VmSessArray(pVm);` |
|   ! 0 |  568 | `	if( pSess ){` |
|   ! 0 |  569 | `		PH7_MemObjRelease(pSess);` |
|   ! 0 |  570 | `		pSess->x.pOther = PH7_NewHashmap(pVm,0,0);` |
|   ! 0 |  571 | `		if( pSess->x.pOther ){` |
|   ! 0 |  572 | `			MemObjSetType(pSess,MEMOBJ_HASHMAP);` |
|   ! 0 |  573 | `		}` |
|   ! 0 |  574 | `	}` |
|   ! 0 |  575 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 |  576 | `	return PH7_OK;` |
|     2 |  577 | `}` |
|     - |  578 | `/* bool session_destroy() */` |
|     4 |  579 | `static int vm_builtin_session_destroy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  580 | `{` |
|     6 |  581 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  582 | `	SyBlob sFile;` |
|     2 |  583 | `	SXUNUSED(nArg);` |
|     2 |  584 | `	SXUNUSED(apArg);` |
|     6 |  585 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|     3 |  586 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|     - |  587 | `			"session_destroy(): Trying to destroy uninitialized session");` |
|     3 |  588 | `		ph7_result_bool(pCtx,0);` |
|     3 |  589 | `		return PH7_OK;` |
|     - |  590 | `	}` |
|     3 |  591 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|     3 |  592 | `	VmSessFile(pVm,&sFile);` |
|     3 |  593 | `	VmSessUnlinkIfExists(pVm,&sFile);` |
|     3 |  594 | `	SyBlobRelease(&sFile);` |
|     3 |  595 | `	pVm->iSessStatus = VM_SESSION_NONE;` |
|     3 |  596 | `	SyBlobReset(&pVm->sSessId);` |
|     3 |  597 | `	ph7_result_bool(pCtx,1);` |
|     3 |  598 | `	return PH7_OK;` |
|     4 |  599 | `}` |
|     - |  600 | `/* bool session_regenerate_id(bool $delete_old_session = false) */` |
|     4 |  601 | `static int vm_builtin_session_regenerate_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  602 | `{` |
|     6 |  603 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  604 | `	SyBlob sFile;` |
|     6 |  605 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|     3 |  606 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|     - |  607 | `			"session_regenerate_id(): Session ID cannot be regenerated when there is no active session");` |
|     3 |  608 | `		ph7_result_bool(pCtx,0);` |
|     3 |  609 | `		return PH7_OK;` |
|     - |  610 | `	}` |
|     3 |  611 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|     3 |  612 | `	VmSessFile(pVm,&sFile);` |
|     3 |  613 | `	if( nArg > 0 && ph7_value_to_bool(apArg[0]) ){` |
|   ! 0 |  614 | `		VmSessUnlinkIfExists(pVm,&sFile);` |
|   ! 0 |  615 | `	}` |
|     3 |  616 | `	SyBlobRelease(&sFile);` |
|     3 |  617 | `	VmSessGenId(pVm,&pVm->sSessId);` |
|     3 |  618 | `	ph7_result_bool(pCtx,1);` |
|     3 |  619 | `	return PH7_OK;` |
|     4 |  620 | `}` |
|  4670 |  621 | `PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm)` |
|     5 |  622 | `{` |
|     - |  623 | `	static const struct {` |
|     - |  624 | `		const char *zName;` |
|     - |  625 | `		ProchHostFunction xFunc;` |
|     - |  626 | `	} aFunc[] = {` |
|     - |  627 | `		{ "session_status",        vm_builtin_session_status        },` |
|     - |  628 | `		{ "session_id",            vm_builtin_session_id            },` |
|     - |  629 | `		{ "session_name",          vm_builtin_session_name          },` |
|     - |  630 | `		{ "session_save_path",     vm_builtin_session_save_path     },` |
|     - |  631 | `		{ "session_start",         vm_builtin_session_start         },` |
|     - |  632 | `		{ "session_write_close",   vm_builtin_session_write_close   },` |
|     - |  633 | `		{ "session_commit",        vm_builtin_session_write_close   },` |
|     - |  634 | `		{ "session_abort",         vm_builtin_session_abort         },` |
|     - |  635 | `		{ "session_reset",         vm_builtin_session_reset         },` |
|     - |  636 | `		{ "session_unset",         vm_builtin_session_unset         },` |
|     - |  637 | `		{ "session_destroy",       vm_builtin_session_destroy       },` |
|     - |  638 | `		{ "session_regenerate_id", vm_builtin_session_regenerate_id },` |
|     - |  639 | `	};` |
|     - |  640 | `	sxu32 n;` |
| 60715 |  641 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 56045 |  642 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 28025 |  643 | `	}` |
|  4675 |  644 | `	return SXRET_OK;` |
|     5 |  645 | `}` |
|     - |  646 |  |
|     - |  647 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     - |  648 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - |  649 |  |
|     - |  650 | `#if defined(PH7_DISABLE_BUILTIN_FUNC) \|\| defined(PH7_DISABLE_DISK_IO)` |
|     - |  651 | `/* Tiny build: no sessions (builtin funcs / disk IO disabled) */` |
|     - |  652 | `PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|     - |  653 | `#endif` |
|     - |  654 |  |
