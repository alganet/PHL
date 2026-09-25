# src/ph7/vm_builtin_session.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1212/1361 lines (89.05%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#include "ph7int.h"` |
|      - |    6 | `#include <time.h>` |
|      - |    7 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |    8 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |    9 | `/*` |
|      - |   10 | ` * Sessions: file-backed session_* functions over the $_SESSION superglobal.` |
|      - |   11 | ` *` |
|      - |   12 | `` * This was an embedded-PHP chunk holding its state on a private `__SessS` class`` |
|      - |   13 | `` * with five static properties, plus five `__sess_*` PHP helpers. All six names`` |
|      - |   14 | ` * are gone: the state is on the VM (pVm->iSessStatus / sSessId / sSessName /` |
|      - |   15 | ` * sSessPath) and the functions are these C routines. Moving the` |
|      - |   16 | ` * state off a PHP class also decoupled the INI subsystem, which used to reach` |
|      - |   17 | `` * into `__SessS::$name` / `$path` to live-wire session.name / session.save_path`` |
|      - |   18 | ` * and now reads the same VM fields.` |
|      - |   19 | ` *` |
|      - |   20 | ` * Session files keep php's DEFAULT "php" serialize-handler format` |
|      - |   21 | `` * (`key\|<serialized>` runs), so they still interoperate with a stock php install`` |
|      - |   22 | ` * both ways. The primitives that format needs -- serialize/unserialize -- and the` |
|      - |   23 | ` * file IO are reached by CALLING the engine's own builtins by name rather than` |
|      - |   24 | ` * duplicating them here: one mechanism (VmSessCall) for all of them, which is` |
|      - |   25 | ` * also what keeps the stream-wrapper behaviour of file_get_contents/file_put_contents` |
|      - |   26 | ` * identical to what the chunk had.` |
|      - |   27 | ` */` |
|      - |   28 |  |
|      - |   29 | `/* php's session_status() values (PHP_SESSION_DISABLED is never reported here). */` |
|      - |   30 | `#define VM_SESSION_NONE   1` |
|      - |   31 | `#define VM_SESSION_ACTIVE 2` |
|      - |   32 |  |
|      - |   33 | `/*` |
|      - |   34 | ` * Call an engine builtin by name. The session logic is a thin layer over` |
|      - |   35 | ` * serialize/unserialize/file IO, and re-implementing those in C would fork` |
|      - |   36 | ` * behaviour that has to stay identical (stream wrappers, the serialize grammar).` |
|      - |   37 | ` */` |
|    589 |   38 | `static sxi32 VmSessCall(ph7_vm *pVm,const char *zFunc,int nArg,ph7_value **apArg,ph7_value *pResult)` |
|      4 |   39 | `{` |
|      - |   40 | `	ph7_value sName;` |
|      - |   41 | `	SyString sStr;` |
|      - |   42 | `	sxi32 rc;` |
|    593 |   43 | `	PH7_MemObjInit(pVm,&sName);` |
|    593 |   44 | `	SyStringInitFromBuf(&sStr,zFunc,SyStrlen(zFunc));` |
|    593 |   45 | `	PH7_MemObjInitFromString(pVm,&sName,&sStr);` |
|    593 |   46 | `	rc = PH7_VmCallUserFunction(&(*pVm),&sName,nArg,apArg,pResult);` |
|    593 |   47 | `	PH7_MemObjRelease(&sName);` |
|    593 |   48 | `	return rc;` |
|      4 |   49 | `}` |
|    767 |   50 | `static void VmSessStrArg(ph7_vm *pVm,ph7_value *pOut,const char *zVal,sxu32 nVal)` |
|      4 |   51 | `{` |
|      - |   52 | `	SyString sStr;` |
|    771 |   53 | `	PH7_MemObjInit(pVm,pOut);` |
|    771 |   54 | `	SyStringInitFromBuf(&sStr,zVal,nVal);` |
|    771 |   55 | `	PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|    771 |   56 | `}` |
|      - |   57 | `/*` |
|      - |   58 | ` * The save path, resolving the lazy default the way the chunk did: an unset path` |
|      - |   59 | ` * becomes sys_get_temp_dir() with any trailing slash removed, and is remembered.` |
|      - |   60 | ` */` |
|    259 |   61 | `static void VmSessResolvePath(ph7_vm *pVm)` |
|      4 |   62 | `{` |
|      - |   63 | `	ph7_value sRes;` |
|    263 |   64 | `	if( SyBlobLength(&pVm->sSessPath) > 0 ){` |
|    241 |   65 | `		return;` |
|      - |   66 | `	}` |
|     26 |   67 | `	PH7_MemObjInit(pVm,&sRes);` |
|     26 |   68 | `	if( VmSessCall(pVm,"sys_get_temp_dir",0,0,&sRes) == SXRET_OK ){` |
|     26 |   69 | `		const char *zTmp = (const char *)SyBlobData(&sRes.sBlob);` |
|     26 |   70 | `		sxu32 nTmp = SyBlobLength(&sRes.sBlob);` |
|     26 |   71 | `		while( nTmp > 0 && zTmp[nTmp-1] == '/' ){ nTmp--; }` |
|     26 |   72 | `		SyBlobReset(&pVm->sSessPath);` |
|     26 |   73 | `		SyBlobAppend(&pVm->sSessPath,zTmp,nTmp);` |
|     11 |   74 | `	}` |
|     26 |   75 | `	PH7_MemObjRelease(&sRes);` |
|    133 |   76 | `}` |
|      - |   77 | `/* php joins the save path and the file name with PHP_DIR_SEPARATOR, which a` |
|      - |   78 | ` * warning naming the file shows: a backslash on Windows. */` |
|      - |   79 | `#ifdef __WINNT__` |
|      - |   80 | `#define VM_SESS_FILE_PREFIX "\\sess_"` |
|      - |   81 | `#else` |
|      - |   82 | `#define VM_SESS_FILE_PREFIX "/sess_"` |
|      - |   83 | `#endif` |
|      - |   84 | `/* "<save_path>/sess_<id>" */` |
|    198 |   85 | `static void VmSessFile(ph7_vm *pVm,SyBlob *pOut)` |
|      4 |   86 | `{` |
|    202 |   87 | `	VmSessResolvePath(pVm);` |
|    202 |   88 | `	SyBlobReset(pOut);` |
|    202 |   89 | `	SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|    202 |   90 | `	SyBlobAppend(pOut,VM_SESS_FILE_PREFIX,sizeof(VM_SESS_FILE_PREFIX)-1);` |
|    202 |   91 | `	SyBlobAppend(pOut,SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId));` |
|    202 |   92 | `}` |
|      - |   93 | `/* php's PS_MAX_SID_LENGTH: the longest id it will read or make. */` |
|      - |   94 | `#define VM_SESS_MAX_ID 256` |
|      - |   95 |  |
|      - |   96 | `/*` |
|      - |   97 | ` * A fresh id: 32 characters over php's 4-bits-per-character alphabet, which is` |
|      - |   98 | ` * lowercase hex. The two directives that would widen either number,` |
|      - |   99 | ` * session.sid_length and session.sid_bits_per_character, are DEPRECATED in php 8.4` |
|      - |  100 | ` * — §10 does not carry php's deprecated surface, so php's defaults are the only` |
|      - |  101 | ` * shape here and an id made by either engine reads the same way to the other.` |
|      - |  102 | ` */` |
|     26 |  103 | `static void VmSessGenId(ph7_vm *pVm,SyBlob *pOut)` |
|      3 |  104 | `{` |
|      - |  105 | `	static const char zAlpha[] = "0123456789abcdef";` |
|      - |  106 | `	unsigned char zRaw[32];` |
|      - |  107 | `	int i;` |
|     29 |  108 | `	SyBlobReset(pOut);` |
|     29 |  109 | `	SyRandomness(&pVm->sPrng,zRaw,sizeof(zRaw));` |
|    861 |  110 | `	for( i = 0 ; i < 32 ; i++ ){` |
|    835 |  111 | `		char c = zAlpha[zRaw[i] & 15];` |
|    835 |  112 | `		SyBlobAppend(pOut,&c,1);` |
|    419 |  113 | `	}` |
|     29 |  114 | `}` |
|      - |  115 | `/*` |
|      - |  116 | ` * php's php_session_valid_key(): an id is 1..256 bytes of A-Z, a-z, 0-9, "-", ",".` |
|      - |  117 | ` * The id names a FILE in the save path, which is why the set is this small.` |
|      - |  118 | ` */` |
|     98 |  119 | `static int VmSessIdValid(const char *z,sxu32 n)` |
|      4 |  120 | `{` |
|      - |  121 | `	sxu32 i;` |
|    102 |  122 | `	if( n < 1 \|\| n > VM_SESS_MAX_ID ){` |
|    ! 0 |  123 | `		return 0;` |
|      - |  124 | `	}` |
|   1287 |  125 | `	for( i = 0 ; i < n ; i++ ){` |
|   1199 |  126 | `		char c = z[i];` |
|   1213 |  127 | `		if( !((c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z')` |
|    312 |  128 | `		   \|\| (c >= 'A' && c <= 'Z') \|\| c == ',' \|\| c == '-') ){` |
|     11 |  129 | `			return 0;` |
|      - |  130 | `		}` |
|    606 |  131 | `	}` |
|     92 |  132 | `	return 1;` |
|     53 |  133 | `}` |
|      - |  134 | `/*` |
|      - |  135 | ` * The bytes php drops an id for WITHOUT a word, before it ever validates it: the` |
|      - |  136 | ` * ones that would break the Set-Cookie header or a log line it lands in. An id` |
|      - |  137 | ` * carrying one is thrown away and a fresh one made, where any other invalid` |
|      - |  138 | ` * character is reported and refuses the start outright.` |
|      - |  139 | ` */` |
|     94 |  140 | `static int VmSessIdDangerous(const char *z,sxu32 n)` |
|      4 |  141 | `{` |
|      - |  142 | `	sxu32 i;` |
|   1267 |  143 | `	for( i = 0 ; i < n ; i++ ){` |
|   1177 |  144 | `		if( SyByteFind("\r\n\t <>'\"\\",sizeof("\r\n\t <>'\"\\")-1,z[i],0) == SXRET_OK ){` |
|      5 |  145 | `			return 1;` |
|      - |  146 | `		}` |
|    598 |  147 | `	}` |
|     94 |  148 | `	return 0;` |
|     51 |  149 | `}` |
|      - |  150 | `/*` |
|      - |  151 | ` * Read the session file into pOut, answering FALSE when there is none.` |
|      - |  152 | ` *` |
|      - |  153 | ` * The existence check is not an optimization: file_get_contents() warns on a` |
|      - |  154 | ` * missing path, and a first-ever session_start() has no file yet -- the PHP chunk` |
|      - |  155 | ` * guarded the read with file_exists() for exactly this reason.` |
|      - |  156 | ` */` |
|     74 |  157 | `static int VmSessReadFile(ph7_vm *pVm,SyBlob *pFile,ph7_value *pOut)` |
|      4 |  158 | `{` |
|      - |  159 | `	ph7_value sPath,sExists;` |
|      - |  160 | `	ph7_value *apA[1];` |
|      - |  161 | `	int bOk;` |
|     78 |  162 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));` |
|     78 |  163 | `	PH7_MemObjInit(pVm,&sExists);` |
|     78 |  164 | `	apA[0] = &sPath;` |
|     78 |  165 | `	VmSessCall(pVm,"file_exists",1,apA,&sExists);` |
|      - |  166 | `	/* PH7_MemObjToBool converts IN PLACE and answers a STATUS, not the boolean --` |
|      - |  167 | `	 * the value lands in x.iVal. Reading its return as the answer makes every` |
|      - |  168 | `	 * existence check false, which silently empties the session on every reload. */` |
|     78 |  169 | `	PH7_MemObjToBool(&sExists);` |
|     78 |  170 | `	bOk = sExists.x.iVal != 0;` |
|     78 |  171 | `	PH7_MemObjRelease(&sExists);` |
|     78 |  172 | `	if( bOk ){` |
|     74 |  173 | `		VmSessCall(pVm,"file_get_contents",1,apA,pOut);` |
|     74 |  174 | `		bOk = (pOut->iFlags & MEMOBJ_STRING) != 0;` |
|     35 |  175 | `	}` |
|     78 |  176 | `	PH7_MemObjRelease(&sPath);` |
|     78 |  177 | `	return bOk;` |
|      4 |  178 | `}` |
|      - |  179 | `/*` |
|      - |  180 | ` * Delete the session file if it is there. Same reason the read is guarded:` |
|      - |  181 | ` * unlink() warns on a missing path, and destroying a session that was never` |
|      - |  182 | ` * written (or regenerating an id before the first write) is normal.` |
|      - |  183 | ` */` |
|      6 |  184 | `static void VmSessUnlinkIfExists(ph7_vm *pVm,SyBlob *pFile)` |
|      3 |  185 | `{` |
|      - |  186 | `	ph7_value sPath,sExists;` |
|      - |  187 | `	ph7_value *apA[1];` |
|      9 |  188 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));` |
|      9 |  189 | `	PH7_MemObjInit(pVm,&sExists);` |
|      9 |  190 | `	apA[0] = &sPath;` |
|      9 |  191 | `	VmSessCall(pVm,"file_exists",1,apA,&sExists);` |
|      9 |  192 | `	PH7_MemObjToBool(&sExists);` |
|      9 |  193 | `	if( sExists.x.iVal ){` |
|      9 |  194 | `		VmSessCall(pVm,"unlink",1,apA,0);` |
|      3 |  195 | `	}` |
|      9 |  196 | `	PH7_MemObjRelease(&sExists);` |
|      9 |  197 | `	PH7_MemObjRelease(&sPath);` |
|      9 |  198 | `}` |
|      - |  199 | `/* $_SESSION, or NULL when the superglobal is somehow absent. */` |
|    208 |  200 | `static ph7_value * VmSessArray(ph7_vm *pVm)` |
|      4 |  201 | `{` |
|    212 |  202 | `	return PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|      4 |  203 | `}` |
|      - |  204 | `/*` |
|      - |  205 | ` * php's three session serialize handlers (session.serialize_handler).` |
|      - |  206 | ` *` |
|      - |  207 | `` * `php` is a run of `key\|<serialized value>`, `php_binary` the same with a`` |
|      - |  208 | `` * one-byte key LENGTH in place of the delimiter, and `php_serialize` one`` |
|      - |  209 | ` * serialize() of the whole array. The first two therefore cannot spell every` |
|      - |  210 | ` * $_SESSION key, and php drops what they cannot spell rather than writing a` |
|      - |  211 | ` * payload it could not read back.` |
|      - |  212 | ` */` |
|      - |  213 | `#define VM_SESS_SER_PHP       0` |
|      - |  214 | `#define VM_SESS_SER_BINARY    1` |
|      - |  215 | `#define VM_SESS_SER_SERIALIZE 2` |
|      - |  216 | `/* php's PS_BIN_MAX: php_binary holds the key length in ONE byte with 0x80 taken` |
|      - |  217 | ` * as its PS_BIN_UNDEF marker, so 127 bytes is the longest key it can name. */` |
|      - |  218 | `#define VM_SESS_BIN_MAX       127` |
|      - |  219 |  |
|      - |  220 | `/*` |
|      - |  221 | ` * Which serializer is configured, or -1 for a name php has no handler for.` |
|      - |  222 | ` * ini_set() refuses an unknown one, so only the php.ini/-d path can arm it -- and` |
|      - |  223 | ` * session_start() is where php reports it and refuses to start.` |
|      - |  224 | ` */` |
|    338 |  225 | `static int VmSessSerializerOrErr(ph7_vm *pVm)` |
|      4 |  226 | `{` |
|      - |  227 | `	SyBlob sVal;` |
|      - |  228 | `	const char *zVal;` |
|      - |  229 | `	sxu32 nVal;` |
|    342 |  230 | `	int iRet = -1;` |
|    342 |  231 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|    342 |  232 | `	PH7_VmIniGetStr(pVm,"session.serialize_handler",&sVal);` |
|    342 |  233 | `	zVal = (const char *)SyBlobData(&sVal);` |
|    342 |  234 | `	nVal = SyBlobLength(&sVal);` |
|    342 |  235 | `	if( nVal == sizeof("php")-1 && SyMemcmp(zVal,"php",nVal) == 0 ){` |
|    310 |  236 | `		iRet = VM_SESS_SER_PHP;` |
|    187 |  237 | `	}else if( nVal == sizeof("php_binary")-1 && SyMemcmp(zVal,"php_binary",nVal) == 0 ){` |
|     15 |  238 | `		iRet = VM_SESS_SER_BINARY;` |
|     27 |  239 | `	}else if( nVal == sizeof("php_serialize")-1 && SyMemcmp(zVal,"php_serialize",nVal) == 0 ){` |
|     15 |  240 | `		iRet = VM_SESS_SER_SERIALIZE;` |
|      7 |  241 | `	}` |
|    342 |  242 | `	SyBlobRelease(&sVal);` |
|    342 |  243 | `	return iRet;` |
|      4 |  244 | `}` |
|    190 |  245 | `static int VmSessSerializer(ph7_vm *pVm)` |
|      4 |  246 | `{` |
|    194 |  247 | `	int iRet = VmSessSerializerOrErr(pVm);` |
|    194 |  248 | `	return iRet < 0 ? VM_SESS_SER_PHP : iRet;` |
|      4 |  249 | `}` |
|      - |  250 | `/* serialize() one value through the engine's own builtin. */` |
|     88 |  251 | `static void VmSessSerializeValue(ph7_vm *pVm,ph7_value *pVal,SyBlob *pOut)` |
|      4 |  252 | `{` |
|      - |  253 | `	ph7_value sSer;` |
|      - |  254 | `	ph7_value *apArg[1];` |
|     92 |  255 | `	PH7_MemObjInit(pVm,&sSer);` |
|     92 |  256 | `	apArg[0] = pVal;` |
|     92 |  257 | `	VmSessCall(pVm,"serialize",1,apArg,&sSer);` |
|     92 |  258 | `	SyBlobAppend(pOut,SyBlobData(&sSer.sBlob),SyBlobLength(&sSer.sBlob));` |
|     92 |  259 | `	PH7_MemObjRelease(&sSer);` |
|     92 |  260 | `}` |
|      - |  261 | `/*` |
|      - |  262 | ` * Serialize $_SESSION into pOut with the configured handler. Answers 0 when php` |
|      - |  263 | `` * refuses the payload outright -- the `php` handler's key carrying its own`` |
|      - |  264 | ` * delimiter -- having raised the diagnostic under zWho, which is the whole prefix` |
|      - |  265 | ` * php puts on it: "session_encode()", "session_write_close()" or, from the` |
|      - |  266 | ` * request-shutdown writer, "PHP Request Shutdown".` |
|      - |  267 | ` */` |
|     96 |  268 | `static int VmSessEncode(ph7_vm *pVm,SyBlob *pOut,const char *zWho)` |
|      4 |  269 | `{` |
|    100 |  270 | `	ph7_value *pSess = VmSessArray(pVm);` |
|      - |  271 | `	ph7_hashmap *pMap;` |
|      - |  272 | `	ph7_hashmap_node *pNode;` |
|    100 |  273 | `	int iSer = VmSessSerializer(pVm);` |
|      - |  274 | `	sxu32 n;` |
|    100 |  275 | `	SyBlobReset(pOut);` |
|    100 |  276 | `	if( pSess == 0 \|\| (pSess->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  277 | `		return 1;` |
|      - |  278 | `	}` |
|    100 |  279 | `	if( iSer == VM_SESS_SER_SERIALIZE ){` |
|      - |  280 | `		/* One serialize() of the array itself, so a numeric key and a key holding` |
|      - |  281 | `		 * a '\|' both simply round-trip. */` |
|      7 |  282 | `		VmSessSerializeValue(pVm,pSess,pOut);` |
|      7 |  283 | `		return 1;` |
|      - |  284 | `	}` |
|     94 |  285 | `	pMap = (ph7_hashmap *)pSess->x.pOther;` |
|     94 |  286 | `	pNode = pMap->pFirst;` |
|    186 |  287 | `	for( n = 0 ; n < pMap->nEntry && pNode ; n++, pNode = pNode->pPrev ){` |
|      - |  288 | `		ph7_value *pVal;` |
|      - |  289 | `		const char *zKey;` |
|      - |  290 | `		sxu32 nKey;` |
|      - |  291 | `		char zMsg[256];` |
|    100 |  292 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|      - |  293 | `			/* Both formats key by NAME; an integer key has no spelling in either. */` |
|     12 |  294 | `			SyBufferFormat(zMsg,sizeof(zMsg),"%s: Skipping numeric key %qd",zWho,pNode->xKey.iKey);` |
|     12 |  295 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|     12 |  296 | `			continue;` |
|      - |  297 | `		}` |
|     90 |  298 | `		zKey = (const char *)SyBlobData(&pNode->xKey.sKey);` |
|     90 |  299 | `		nKey = SyBlobLength(&pNode->xKey.sKey);` |
|     90 |  300 | `		if( iSer == VM_SESS_SER_BINARY ){` |
|     19 |  301 | `			if( nKey > VM_SESS_BIN_MAX ){` |
|    ! 0 |  302 | `				continue;   /* no length byte can name it; php drops it in silence */` |
|      1 |  303 | `			}` |
|     81 |  304 | `		}else if( SyByteFind(zKey,nKey,'\|',0) == SXRET_OK ){` |
|      - |  305 | `			/* The delimiter inside a key would make the payload unreadable, so php` |
|      - |  306 | `			 * writes NOTHING rather than a file it cannot parse back. */` |
|      7 |  307 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  308 | `				"%s: Failed to write session data. Data contains invalid key \"%.*s\"",` |
|      2 |  309 | `				zWho,(int)nKey,zKey);` |
|      5 |  310 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      5 |  311 | `			SyBlobReset(pOut);` |
|      5 |  312 | `			return 0;` |
|      - |  313 | `		}` |
|     86 |  314 | `		pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     86 |  315 | `		if( pVal == 0 ){` |
|    ! 0 |  316 | `			continue;` |
|      - |  317 | `		}` |
|     86 |  318 | `		if( iSer == VM_SESS_SER_BINARY ){` |
|     19 |  319 | `			char c = (char)nKey;` |
|     19 |  320 | `			SyBlobAppend(pOut,&c,1);` |
|     19 |  321 | `			SyBlobAppend(pOut,zKey,nKey);` |
|     10 |  322 | `		}else{` |
|     68 |  323 | `			SyBlobAppend(pOut,zKey,nKey);` |
|     68 |  324 | `			SyBlobAppend(pOut,"\|",1);` |
|      - |  325 | `		}` |
|     86 |  326 | `		VmSessSerializeValue(pVm,pVal,pOut);` |
|     45 |  327 | `	}` |
|     90 |  328 | `	return 1;` |
|     52 |  329 | `}` |
|      - |  330 | `/*` |
|      - |  331 | ` * php's php_session_normalize_vars(): a walk of the session variables that reports` |
|      - |  332 | ` * the ones its store cannot NAME and touches nothing else. It is not the encoder —` |
|      - |  333 | ` * a value the serializer would refuse is not looked at here, and neither is a key` |
|      - |  334 | ` * carrying the delimiter — and it is why session_decode() reports a numeric key` |
|      - |  335 | ` * before it has written anything.` |
|      - |  336 | ` */` |
|      8 |  337 | `static void VmSessNormalizeVars(ph7_vm *pVm,const char *zWho)` |
|      1 |  338 | `{` |
|      9 |  339 | `	ph7_value *pSess = VmSessArray(pVm);` |
|      - |  340 | `	ph7_hashmap *pMap;` |
|      - |  341 | `	ph7_hashmap_node *pNode;` |
|      - |  342 | `	sxu32 n;` |
|      8 |  343 | `	if( pSess == 0 \|\| (pSess->iFlags & MEMOBJ_HASHMAP) == 0` |
|      9 |  344 | `	 \|\| VmSessSerializer(pVm) == VM_SESS_SER_SERIALIZE ){` |
|    ! 0 |  345 | `		return;` |
|      - |  346 | `	}` |
|      9 |  347 | `	pMap = (ph7_hashmap *)pSess->x.pOther;` |
|      9 |  348 | `	pNode = pMap->pFirst;` |
|     21 |  349 | `	for( n = 0 ; n < pMap->nEntry && pNode ; n++, pNode = pNode->pPrev ){` |
|      - |  350 | `		char zMsg[128];` |
|     13 |  351 | `		if( pNode->iType != HASHMAP_INT_NODE ){` |
|     11 |  352 | `			continue;` |
|      - |  353 | `		}` |
|      3 |  354 | `		SyBufferFormat(zMsg,sizeof(zMsg),"%s: Skipping numeric key %qd",zWho,pNode->xKey.iKey);` |
|      3 |  355 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      2 |  356 | `	}` |
|      5 |  357 | `}` |
|      - |  358 | `/*` |
|      - |  359 | `` * Store one decoded `name => value` pair into the session array. The name goes in`` |
|      - |  360 | `` * RAW, which is php's php_set_session_var(): a store holding `7\|i:1;` comes back`` |
|      - |  361 | ` * as the string key "7" in both engines, not the integer key an array subscript` |
|      - |  362 | ` * would have folded it to.` |
|      - |  363 | ` */` |
|     32 |  364 | `static void VmSessPut(ph7_vm *pVm,ph7_value *pDest,const char *zKey,sxu32 nKey,ph7_value *pVal)` |
|      3 |  365 | `{` |
|     16 |  366 | `	(void)pVm;` |
|     35 |  367 | `	PH7_HashmapInsertRawKey((ph7_hashmap *)pDest->x.pOther,zKey,nKey,pVal);` |
|     35 |  368 | `}` |
|      - |  369 | `/* Give pDest a fresh empty array. */` |
|     98 |  370 | `static void VmSessEmptyArray(ph7_vm *pVm,ph7_value *pDest)` |
|      4 |  371 | `{` |
|    102 |  372 | `	PH7_MemObjRelease(pDest);` |
|    102 |  373 | `	pDest->x.pOther = PH7_NewHashmap(pVm,0,0);` |
|    102 |  374 | `	if( pDest->x.pOther ){` |
|    102 |  375 | `		MemObjSetType(pDest,MEMOBJ_HASHMAP);` |
|     49 |  376 | `	}` |
|    102 |  377 | `}` |
|      - |  378 | `/*` |
|      - |  379 | `` * Read a payload back into pDest. The `php` and `php_binary` handlers MERGE their`` |
|      - |  380 | ` * names into whatever is already there (which is what makes session_decode() an` |
|      - |  381 | `` * overlay), while `php_serialize` REPLACES the whole variable -- so php really`` |
|      - |  382 | `` * does leave $_SESSION an int for `session_decode('i:5;')`.`` |
|      - |  383 | ` *` |
|      - |  384 | ` * Answers 1 when php reports the payload decoded, 0 when it does not, and -1 when` |
|      - |  385 | ` * a __wakeup()/__unserialize() threw: the exception is the diagnostic then, and` |
|      - |  386 | ` * the caller propagates it instead of reporting a decode failure.` |
|      - |  387 | ` */` |
|     86 |  388 | `static int VmSessDecodeInto(ph7_context *pCtx,const char *zSrc,sxu32 nLen,ph7_value *pDest)` |
|      4 |  389 | `{` |
|     90 |  390 | `	ph7_vm *pVm = pCtx->pVm;` |
|     90 |  391 | `	int iSer = VmSessSerializer(pVm);` |
|     90 |  392 | `	sxu32 nPos = 0;` |
|     90 |  393 | `	if( iSer == VM_SESS_SER_SERIALIZE ){` |
|      - |  394 | `		ph7_value sVal;` |
|      5 |  395 | `		int nRead = 0;` |
|      - |  396 | `		sxi32 rc;` |
|      5 |  397 | `		PH7_MemObjInit(pVm,&sVal);` |
|      5 |  398 | `		rc = nLen > 0` |
|      2 |  399 | `			? PH7_VmUnserializeOne(pCtx,zSrc,(int)nLen,&nRead,&sVal)` |
|      2 |  400 | `			: SXERR_SYNTAX;` |
|      5 |  401 | `		if( rc == PH7_EXCEPTION ){` |
|    ! 0 |  402 | `			PH7_MemObjRelease(&sVal);` |
|    ! 0 |  403 | `			return -1;` |
|      - |  404 | `		}` |
|      5 |  405 | `		if( rc == SXRET_OK && (sVal.iFlags & MEMOBJ_NULL) == 0 ){` |
|      3 |  406 | `			PH7_MemObjRelease(pDest);` |
|      3 |  407 | `			PH7_MemObjStore(&sVal,pDest);` |
|      2 |  408 | `		}else{` |
|      - |  409 | `			/* A payload that did not decode, and php's serialized NULL, both leave` |
|      - |  410 | `			 * the variable an empty array; only the EMPTY payload is still a` |
|      - |  411 | ``			 * success, php's `result \|\| !vallen`. */`` |
|      3 |  412 | `			VmSessEmptyArray(pVm,pDest);` |
|      - |  413 | `		}` |
|      5 |  414 | `		PH7_MemObjRelease(&sVal);` |
|      5 |  415 | `		return rc == SXRET_OK \|\| nLen == 0;` |
|      - |  416 | `	}` |
|     86 |  417 | `	if( (pDest->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  418 | `		VmSessEmptyArray(pVm,pDest);` |
|    ! 0 |  419 | `		if( (pDest->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  420 | `			return 0;` |
|      - |  421 | `		}` |
|    ! 0 |  422 | `	}` |
|    118 |  423 | `	while( nPos < nLen ){` |
|      - |  424 | `		const char *zKey;` |
|      - |  425 | `		sxu32 nKey;` |
|      - |  426 | `		ph7_value sVal;` |
|     37 |  427 | `		int nRead = 0;` |
|      - |  428 | `		sxi32 rc;` |
|     37 |  429 | `		if( iSer == VM_SESS_SER_BINARY ){` |
|      - |  430 | `			/* The length byte, with php's PS_BIN_UNDEF bit masked off. */` |
|      7 |  431 | `			nKey = (sxu32)(((const unsigned char *)zSrc)[nPos] & 0x7f);` |
|      7 |  432 | `			if( nPos + 1 + nKey > nLen ){` |
|    ! 0 |  433 | `				return 0;` |
|      - |  434 | `			}` |
|      7 |  435 | `			zKey = &zSrc[nPos + 1];` |
|      7 |  436 | `			nPos += 1 + nKey;` |
|      4 |  437 | `		}else{` |
|     31 |  438 | `			sxu32 nBar = nPos;` |
|    111 |  439 | `			while( nBar < nLen && zSrc[nBar] != '\|' ){ nBar++; }` |
|     31 |  440 | `			if( nBar >= nLen ){` |
|      3 |  441 | `				return 0;   /* a trailing run with no delimiter is a failed decode */` |
|      - |  442 | `			}` |
|     29 |  443 | `			zKey = &zSrc[nPos];` |
|     29 |  444 | `			nKey = nBar - nPos;` |
|     29 |  445 | `			nPos = nBar + 1;` |
|      - |  446 | `		}` |
|     35 |  447 | `		PH7_MemObjInit(pVm,&sVal);` |
|     35 |  448 | `		rc = nPos < nLen` |
|     32 |  449 | `			? PH7_VmUnserializeOne(pCtx,&zSrc[nPos],(int)(nLen - nPos),&nRead,&sVal)` |
|     16 |  450 | `			: SXERR_SYNTAX;` |
|     35 |  451 | `		if( rc != SXRET_OK \|\| nRead <= 0 ){` |
|      - |  452 | `			/* nRead cannot be 0 for a value that parsed, but the loop's only` |
|      - |  453 | `			 * guarantee of progress is this step -- and the bytes are a STORE, i.e.` |
|      - |  454 | `			 * whatever was last written to the save path. */` |
|    ! 0 |  455 | `			PH7_MemObjRelease(&sVal);` |
|    ! 0 |  456 | `			return rc == PH7_EXCEPTION ? -1 : 0;` |
|      - |  457 | `		}` |
|     35 |  458 | `		VmSessPut(pVm,pDest,zKey,nKey,&sVal);` |
|     35 |  459 | `		PH7_MemObjRelease(&sVal);` |
|     35 |  460 | `		nPos += (sxu32)nRead;` |
|      3 |  461 | `	}` |
|     84 |  462 | `	return 1;` |
|     47 |  463 | `}` |
|      - |  464 | `/*` |
|      - |  465 | ` * session_set_save_handler(): the store a program brings of its own.` |
|      - |  466 | ` *` |
|      - |  467 | ` * php reaches a store through six operations -- open, close, read, write,` |
|      - |  468 | ` * destroy, gc -- plus three optional ones: create_sid, validateId and` |
|      - |  469 | `` * updateTimestamp. The built-in `files` store implements them in C above; a`` |
|      - |  470 | ` * userland handler replaces it wholesale, which is how a session lands in a` |
|      - |  471 | ` * database, in redis, or anywhere the process can reach.` |
|      - |  472 | ` *` |
|      - |  473 | ` * The handler is kept as the INSTANCE and each operation is dispatched as the` |
|      - |  474 | `` * array callable `[$handler, 'read']`, so the engine's own dispatcher resolves it`` |
|      - |  475 | `` * like any other method call — visibility, inheritance and `parent::` included.`` |
|      - |  476 | ` */` |
|      - |  477 | `#define VM_SESS_OP_OPEN     0` |
|      - |  478 | `#define VM_SESS_OP_CLOSE    1` |
|      - |  479 | `#define VM_SESS_OP_READ     2` |
|      - |  480 | `#define VM_SESS_OP_WRITE    3` |
|      - |  481 | `#define VM_SESS_OP_DESTROY  4` |
|      - |  482 | `#define VM_SESS_OP_GC       5` |
|      - |  483 | `#define VM_SESS_OP_CREATE   6` |
|      - |  484 | `#define VM_SESS_OP_VALIDATE 7` |
|      - |  485 | `#define VM_SESS_OP_UPDATE   8` |
|      - |  486 |  |
|      - |  487 | `static const char * const azSessOp[] = {` |
|      - |  488 | `	"open","close","read","write","destroy","gc","create_sid","validateId","updateTimestamp"` |
|      - |  489 | `};` |
|      - |  490 | `/* TRUE when a userland handler is installed. */` |
|    417 |  491 | `static int VmSessHasUser(ph7_vm *pVm)` |
|      4 |  492 | `{` |
|    421 |  493 | `	return (pVm->sSessHandler.iFlags & MEMOBJ_OBJ) != 0;` |
|      4 |  494 | `}` |
|      - |  495 | `/*` |
|      - |  496 | ` * Call one store operation. Answers 0 when the handler does not offer it (php` |
|      - |  497 | ` * treats the three optional ones as absent then), 1 when it ran, -1 on a throw.` |
|      - |  498 | ` */` |
|    104 |  499 | `static int VmSessUserCall(ph7_vm *pVm,int iOp,int nArg,ph7_value **apArg,ph7_value *pResult)` |
|      3 |  500 | `{` |
|      - |  501 | `	ph7_value sCallable;` |
|      - |  502 | `	sxi32 rc;` |
|    107 |  503 | `	int iRet = 1;` |
|    107 |  504 | `	ph7_class_instance *pThis = (ph7_class_instance *)pVm->sSessHandler.x.pOther;` |
|      - |  505 | `	ph7_value sName;` |
|    107 |  506 | `	PH7_MemObjInit(pVm,&sCallable);` |
|    104 |  507 | `	if( (pVm->sSessHandler.iFlags & MEMOBJ_OBJ) == 0 \|\| pThis == 0` |
|    107 |  508 | `	 \|\| PH7_ClassExtractMethod(pThis->pClass,azSessOp[iOp],` |
|    156 |  509 | `		(sxu32)SyStrlen(azSessOp[iOp])) == 0 ){` |
|      - |  510 | `		/* The three optional operations are simply absent when the handler does` |
|      - |  511 | `		 * not implement their interface. */` |
|    ! 0 |  512 | `		PH7_MemObjRelease(&sCallable);` |
|    ! 0 |  513 | `		return 0;` |
|      - |  514 | `	}` |
|    107 |  515 | `	sCallable.x.pOther = PH7_NewHashmap(pVm,0,0);` |
|    107 |  516 | `	if( sCallable.x.pOther == 0 ){` |
|    ! 0 |  517 | `		PH7_MemObjRelease(&sCallable);` |
|    ! 0 |  518 | `		return 0;` |
|      - |  519 | `	}` |
|    107 |  520 | `	MemObjSetType(&sCallable,MEMOBJ_HASHMAP);` |
|    107 |  521 | `	PH7_HashmapInsert((ph7_hashmap *)sCallable.x.pOther,0,&pVm->sSessHandler);` |
|    107 |  522 | `	VmSessStrArg(pVm,&sName,azSessOp[iOp],(sxu32)SyStrlen(azSessOp[iOp]));` |
|    107 |  523 | `	PH7_HashmapInsert((ph7_hashmap *)sCallable.x.pOther,0,&sName);` |
|    107 |  524 | `	PH7_MemObjRelease(&sName);` |
|    107 |  525 | `	rc = PH7_VmCallUserFunction(pVm,&sCallable,nArg,apArg,pResult);` |
|    107 |  526 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 |  527 | `		iRet = -1;` |
|    107 |  528 | `	}else if( rc != SXRET_OK ){` |
|    ! 0 |  529 | `		iRet = 0;` |
|    ! 0 |  530 | `	}` |
|    107 |  531 | `	PH7_MemObjRelease(&sCallable);` |
|    107 |  532 | `	return iRet;` |
|     55 |  533 | `}` |
|      - |  534 | `/* open($path, $name) — php calls it once, before the first read. */` |
|     28 |  535 | `static void VmSessUserOpen(ph7_vm *pVm)` |
|      3 |  536 | `{` |
|      - |  537 | `	ph7_value sPath,sName,sRes;` |
|      - |  538 | `	ph7_value *apA[2];` |
|     31 |  539 | `	if( pVm->bSessOpened \|\| !VmSessHasUser(pVm) ){` |
|      5 |  540 | `		return;` |
|      - |  541 | `	}` |
|     27 |  542 | `	pVm->bSessOpened = 1;` |
|     27 |  543 | `	VmSessResolvePath(pVm);` |
|     39 |  544 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(&pVm->sSessPath),` |
|     12 |  545 | `		SyBlobLength(&pVm->sSessPath));` |
|     39 |  546 | `	VmSessStrArg(pVm,&sName,(const char *)SyBlobData(&pVm->sSessName),` |
|     12 |  547 | `		SyBlobLength(&pVm->sSessName));` |
|     27 |  548 | `	PH7_MemObjInit(pVm,&sRes);` |
|     27 |  549 | `	apA[0] = &sPath;` |
|     27 |  550 | `	apA[1] = &sName;` |
|     27 |  551 | `	VmSessUserCall(pVm,VM_SESS_OP_OPEN,2,apA,&sRes);` |
|     27 |  552 | `	PH7_MemObjRelease(&sRes);` |
|     27 |  553 | `	PH7_MemObjRelease(&sName);` |
|     27 |  554 | `	PH7_MemObjRelease(&sPath);` |
|     17 |  555 | `}` |
|      - |  556 | `/* Release the store: a userland handler's close(), once per open(). */` |
|     90 |  557 | `static void VmSessCloseStore(ph7_vm *pVm)` |
|      4 |  558 | `{` |
|      - |  559 | `	ph7_value sRes;` |
|     94 |  560 | `	if( !VmSessHasUser(pVm) \|\| !pVm->bSessOpened ){` |
|     70 |  561 | `		return;` |
|      - |  562 | `	}` |
|     27 |  563 | `	pVm->bSessOpened = 0;` |
|     27 |  564 | `	PH7_MemObjInit(pVm,&sRes);` |
|     27 |  565 | `	VmSessUserCall(pVm,VM_SESS_OP_CLOSE,0,0,&sRes);` |
|     27 |  566 | `	PH7_MemObjRelease(&sRes);` |
|     49 |  567 | `}` |
|      - |  568 | `/* One id-shaped call: read($id) / destroy($id). */` |
|     28 |  569 | `static int VmSessUserId(ph7_vm *pVm,int iOp,ph7_value *pResult)` |
|      3 |  570 | `{` |
|      - |  571 | `	ph7_value sId;` |
|      - |  572 | `	ph7_value *apA[1];` |
|      - |  573 | `	int iRet;` |
|     45 |  574 | `	VmSessStrArg(pVm,&sId,(const char *)SyBlobData(&pVm->sSessId),` |
|     14 |  575 | `		SyBlobLength(&pVm->sSessId));` |
|     31 |  576 | `	apA[0] = &sId;` |
|     31 |  577 | `	iRet = VmSessUserCall(pVm,iOp,1,apA,pResult);` |
|     31 |  578 | `	PH7_MemObjRelease(&sId);` |
|     31 |  579 | `	return iRet;` |
|      3 |  580 | `}` |
|      - |  581 | `/*` |
|      - |  582 | ` * php's answer to a store it cannot read: the whole session goes — file, id and` |
|      - |  583 | ` * variables — rather than the script running on half of one. A corrupt payload and` |
|      - |  584 | ` * an attacker-supplied one look the same from here, which is why it is not a` |
|      - |  585 | ` * partial load. zFunc names the caller php blames.` |
|      - |  586 | ` */` |
|      2 |  587 | `static void VmSessDestroyBadStore(ph7_vm *pVm,SyBlob *pFile,const char *zFunc)` |
|      1 |  588 | `{` |
|      3 |  589 | `	ph7_value *pSess = VmSessArray(pVm);` |
|      - |  590 | `	char zMsg[128];` |
|      3 |  591 | `	VmSessUnlinkIfExists(pVm,pFile);` |
|      3 |  592 | `	pVm->iSessStatus = VM_SESSION_NONE;` |
|      3 |  593 | `	SyBlobReset(&pVm->sSessId);` |
|      3 |  594 | `	if( pSess ){` |
|      3 |  595 | `		VmSessEmptyArray(pVm,pSess);` |
|      1 |  596 | `	}` |
|      4 |  597 | `	SyBufferFormat(zMsg,sizeof(zMsg),` |
|      1 |  598 | `		"%s(): Failed to decode session object. Session has been destroyed",zFunc);` |
|      3 |  599 | `	PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      3 |  600 | `}` |
|      - |  601 | `/*` |
|      - |  602 | `` * Call a builtin with the engine's diagnostics SUPPRESSED, php's `@`. The store`` |
|      - |  603 | ` * layer reports its own failures in php's words ("open(%s, O_RDWR) failed: ..."),` |
|      - |  604 | ` * so the wrapper's "Failed to open stream" underneath it must not leak out.` |
|      - |  605 | ` */` |
|    225 |  606 | `static sxi32 VmSessCallQuiet(ph7_vm *pVm,const char *zFunc,int nArg,ph7_value **apArg,` |
|      - |  607 | `	ph7_value *pResult)` |
|      4 |  608 | `{` |
|      - |  609 | `	sxi32 rc;` |
|    229 |  610 | `	pVm->nErrSuppress++;` |
|    229 |  611 | `	rc = VmSessCall(pVm,zFunc,nArg,apArg,pResult);` |
|    229 |  612 | `	if( pVm->nErrSuppress > 0 ){` |
|    229 |  613 | `		pVm->nErrSuppress--;` |
|    111 |  614 | `	}` |
|    229 |  615 | `	return rc;` |
|      4 |  616 | `}` |
|      - |  617 | `/* TRUE when calling zFunc(zPath) answers true. */` |
|    116 |  618 | `static int VmSessPathIs(ph7_vm *pVm,const char *zFunc,SyBlob *pPath)` |
|      4 |  619 | `{` |
|      - |  620 | `	ph7_value sPath,sRes;` |
|      - |  621 | `	ph7_value *apA[1];` |
|      - |  622 | `	int bOk;` |
|    120 |  623 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pPath),SyBlobLength(pPath));` |
|    120 |  624 | `	PH7_MemObjInit(pVm,&sRes);` |
|    120 |  625 | `	apA[0] = &sPath;` |
|    120 |  626 | `	VmSessCallQuiet(pVm,zFunc,1,apA,&sRes);` |
|    120 |  627 | `	PH7_MemObjToBool(&sRes);` |
|    120 |  628 | `	bOk = sRes.x.iVal != 0;` |
|    120 |  629 | `	PH7_MemObjRelease(&sRes);` |
|    120 |  630 | `	PH7_MemObjRelease(&sPath);` |
|    120 |  631 | `	return bOk;` |
|      4 |  632 | `}` |
|      - |  633 | `/*` |
|      - |  634 | ` * php OPENS the store at session_start(), O_CREAT\|O_RDWR 0600 -- so the file` |
|      - |  635 | ` * exists, empty, from the moment the session starts rather than from the first` |
|      - |  636 | ` * write, and a save path it cannot open is a REFUSED start. PHL created nothing` |
|      - |  637 | ` * and read a missing file as an empty session, so a mistyped session.save_path` |
|      - |  638 | ` * silently handed every request a blank session and threw its writes away.` |
|      - |  639 | ` *` |
|      - |  640 | ` * Answers 1 when the store is usable; 0 when php would have refused, having` |
|      - |  641 | ` * raised both of its diagnostics under zFunc.` |
|      - |  642 | ` */` |
|      - |  643 | `static void VmSessPutFileQuiet(ph7_vm *pVm,SyBlob *pFile,const char *zData,sxu32 nData,` |
|      - |  644 | `	int bQuiet);` |
|     72 |  645 | `static int VmSessOpenStore(ph7_vm *pVm,SyBlob *pFile,const char *zFunc)` |
|      4 |  646 | `{` |
|      - |  647 | `	char zMsg[512];` |
|      - |  648 | `	int iErr;` |
|      - |  649 | `	const char *zWhy;` |
|     76 |  650 | `	if( VmSessPathIs(pVm,"file_exists",pFile) ){` |
|     33 |  651 | `		return 1;` |
|      - |  652 | `	}` |
|     46 |  653 | `	VmSessPutFileQuiet(pVm,pFile,"",0,1);` |
|     46 |  654 | `	if( VmSessPathIs(pVm,"file_exists",pFile) ){` |
|      - |  655 | `		/* php's store is readable by its own user and nobody else. */` |
|      - |  656 | `		ph7_value sPath,sMode;` |
|      - |  657 | `		ph7_value *apA[2];` |
|     44 |  658 | `		VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));` |
|     44 |  659 | `		PH7_MemObjInit(pVm,&sMode);` |
|     44 |  660 | `		PH7_MemObjInitFromInt(pVm,&sMode,0600);` |
|     44 |  661 | `		apA[0] = &sPath;` |
|     44 |  662 | `		apA[1] = &sMode;` |
|     44 |  663 | `		VmSessCallQuiet(pVm,"chmod",2,apA,0);` |
|     44 |  664 | `		PH7_MemObjRelease(&sMode);` |
|     44 |  665 | `		PH7_MemObjRelease(&sPath);` |
|     44 |  666 | `		return 1;` |
|      - |  667 | `	}` |
|      3 |  668 | `	VmSessResolvePath(pVm);` |
|      3 |  669 | `	if( !VmSessPathIs(pVm,"is_dir",&pVm->sSessPath) ){` |
|      3 |  670 | `		iErr = 2;   /* ENOENT */` |
|      2 |  671 | `	}else{` |
|    ! 0 |  672 | `		iErr = 13;  /* EACCES: the directory is there and will not take the file */` |
|      - |  673 | `	}` |
|      3 |  674 | `	zWhy = VfsStrerror(iErr);` |
|      4 |  675 | `	SyBufferFormat(zMsg,sizeof(zMsg),"%s(): open(%.*s, O_RDWR) failed: %s (%d)",` |
|      2 |  676 | `		zFunc,(int)SyBlobLength(pFile),(const char *)SyBlobData(pFile),zWhy,iErr);` |
|      3 |  677 | `	PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      4 |  678 | `	SyBufferFormat(zMsg,sizeof(zMsg),` |
|      1 |  679 | `		"%s(): Failed to read session data: files (path: %.*s)",zFunc,` |
|      2 |  680 | `		(int)SyBlobLength(&pVm->sSessPath),(const char *)SyBlobData(&pVm->sSessPath));` |
|      3 |  681 | `	PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      3 |  682 | `	return 0;` |
|     40 |  683 | `}` |
|      - |  684 | `/*` |
|      - |  685 | ` * php's garbage collection, the FILES half: every store in the save path whose last` |
|      - |  686 | ` * change is older than the given lifetime goes, and the count is the answer. Only the` |
|      - |  687 | `` * `sess_` prefix is touched -- the save path is an ordinary directory and may hold`` |
|      - |  688 | ` * anything else.` |
|      - |  689 | ` *` |
|      - |  690 | ` * Split from the dispatcher below because SessionHandler::gc() -- php's built-in` |
|      - |  691 | ` * files store, exposed as a class precisely so a program can put itself in front of` |
|      - |  692 | ` * it -- must reach THIS and not the dispatcher. It used to call the dispatcher, which` |
|      - |  693 | ` * saw the user handler installed and called that handler's gc()... whose parent::gc()` |
|      - |  694 | `` * is SessionHandler::gc(). `session_set_save_handler(new SessionHandler())` plus any`` |
|      - |  695 | ` * collection at all was an unbounded recursion, and the collection did not have to be` |
|      - |  696 | ` * asked for: php's DEFAULT gc_probability runs it from one session_start() in a` |
|      - |  697 | ` * hundred, so the documented decorator idiom had a 1%-per-request fatal in it. Every` |
|      - |  698 | ` * other SessionHandler method already called the file-level primitive directly.` |
|      - |  699 | ` */` |
|      5 |  700 | `static sxi64 VmSessGcFiles(ph7_vm *pVm,sxi64 iMaxLife)` |
|      2 |  701 | `{` |
|      - |  702 | `	ph7_value sDir,sList;` |
|      - |  703 | `	ph7_value *apA[1];` |
|      - |  704 | `	ph7_hashmap *pMap;` |
|      - |  705 | `	ph7_hashmap_node *pNode;` |
|      7 |  706 | `	sxi64 iCut = (sxi64)time(0) - iMaxLife;` |
|      7 |  707 | `	sxi64 nGone = 0;` |
|      - |  708 | `	sxu32 n;` |
|      7 |  709 | `	VmSessResolvePath(pVm);` |
|      9 |  710 | `	VmSessStrArg(pVm,&sDir,(const char *)SyBlobData(&pVm->sSessPath),` |
|      2 |  711 | `		SyBlobLength(&pVm->sSessPath));` |
|      7 |  712 | `	PH7_MemObjInit(pVm,&sList);` |
|      7 |  713 | `	apA[0] = &sDir;` |
|      7 |  714 | `	VmSessCallQuiet(pVm,"scandir",1,apA,&sList);` |
|      7 |  715 | `	PH7_MemObjRelease(&sDir);` |
|      7 |  716 | `	if( (sList.iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  717 | `		PH7_MemObjRelease(&sList);` |
|    ! 0 |  718 | `		return 0;` |
|      - |  719 | `	}` |
|      7 |  720 | `	pMap = (ph7_hashmap *)sList.x.pOther;` |
|      7 |  721 | `	pNode = pMap->pFirst;` |
|     29 |  722 | `	for( n = 0 ; n < pMap->nEntry && pNode ; n++, pNode = pNode->pPrev ){` |
|     24 |  723 | `		ph7_value *pName = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|      - |  724 | `		SyBlob sPath;` |
|      - |  725 | `		ph7_value sArg,sTime;` |
|      - |  726 | `		ph7_value *apB[1];` |
|      - |  727 | `		const char *zName;` |
|      - |  728 | `		sxu32 nName;` |
|     24 |  729 | `		if( pName == 0 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  730 | `			continue;` |
|      - |  731 | `		}` |
|     24 |  732 | `		zName = (const char *)SyBlobData(&pName->sBlob);` |
|     24 |  733 | `		nName = SyBlobLength(&pName->sBlob);` |
|     24 |  734 | `		if( nName <= sizeof("sess_")-1 \|\| SyMemcmp(zName,"sess_",sizeof("sess_")-1) != 0 ){` |
|     14 |  735 | `			continue;` |
|      - |  736 | `		}` |
|     12 |  737 | `		SyBlobInit(&sPath,&pVm->sAllocator);` |
|     12 |  738 | `		SyBlobAppend(&sPath,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|     12 |  739 | `		SyBlobAppend(&sPath,"/",1);` |
|     12 |  740 | `		SyBlobAppend(&sPath,zName,nName);` |
|     12 |  741 | `		VmSessStrArg(pVm,&sArg,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|     12 |  742 | `		PH7_MemObjInit(pVm,&sTime);` |
|     12 |  743 | `		apB[0] = &sArg;` |
|     12 |  744 | `		VmSessCallQuiet(pVm,"filemtime",1,apB,&sTime);` |
|     12 |  745 | `		if( (sTime.iFlags & MEMOBJ_INT) && sTime.x.iVal < iCut ){` |
|      8 |  746 | `			VmSessCallQuiet(pVm,"unlink",1,apB,0);` |
|      8 |  747 | `			nGone++;` |
|      3 |  748 | `		}` |
|     12 |  749 | `		PH7_MemObjRelease(&sTime);` |
|     12 |  750 | `		PH7_MemObjRelease(&sArg);` |
|     12 |  751 | `		SyBlobRelease(&sPath);` |
|      6 |  752 | `	}` |
|      7 |  753 | `	PH7_MemObjRelease(&sList);` |
|      7 |  754 | `	return nGone;` |
|      4 |  755 | `}` |
|      - |  756 | `/*` |
|      - |  757 | ` * Run the collector the SESSION is configured with: the user handler's gc() when one` |
|      - |  758 | ` * is installed, the files sweep otherwise. session_gc() and the probabilistic sweep` |
|      - |  759 | ` * come through here.` |
|      - |  760 | ` */` |
|      9 |  761 | `static sxi64 VmSessGc(ph7_vm *pVm)` |
|      2 |  762 | `{` |
|     11 |  763 | `	sxi64 iMaxLife = PH7_VmIniGetInt(pVm,"session.gc_maxlifetime",1440);` |
|     11 |  764 | `	if( VmSessHasUser(pVm) ){` |
|      - |  765 | `		ph7_value sLife,sRes;` |
|      - |  766 | `		ph7_value *apA[1];` |
|      8 |  767 | `		sxi64 nGone = 0;` |
|      8 |  768 | `		PH7_MemObjInit(pVm,&sLife);` |
|      8 |  769 | `		PH7_MemObjInitFromInt(pVm,&sLife,iMaxLife);` |
|      8 |  770 | `		PH7_MemObjInit(pVm,&sRes);` |
|      8 |  771 | `		apA[0] = &sLife;` |
|      8 |  772 | `		if( VmSessUserCall(pVm,VM_SESS_OP_GC,1,apA,&sRes) > 0 ){` |
|      8 |  773 | `			PH7_MemObjToInteger(&sRes);` |
|      8 |  774 | `			nGone = sRes.x.iVal;` |
|      3 |  775 | `		}` |
|      8 |  776 | `		PH7_MemObjRelease(&sRes);` |
|      8 |  777 | `		PH7_MemObjRelease(&sLife);` |
|      8 |  778 | `		return nGone;` |
|      - |  779 | `	}` |
|      4 |  780 | `	return VmSessGcFiles(pVm,iMaxLife);` |
|      6 |  781 | `}` |
|      - |  782 | `/*` |
|      - |  783 | ` * php runs the collector on a session_start() with probability` |
|      - |  784 | ` * gc_probability/gc_divisor: one request in a hundred pays for everybody by` |
|      - |  785 | ` * default, and gc_probability of 0 turns it off.` |
|      - |  786 | ` */` |
|     94 |  787 | `static void VmSessMaybeGc(ph7_vm *pVm)` |
|      4 |  788 | `{` |
|     98 |  789 | `	sxi64 iProb = PH7_VmIniGetInt(pVm,"session.gc_probability",1);` |
|     98 |  790 | `	sxi64 iDiv = PH7_VmIniGetInt(pVm,"session.gc_divisor",100);` |
|     98 |  791 | `	sxu32 nRand = 0;` |
|     98 |  792 | `	if( iProb <= 0 \|\| iDiv <= 0 ){` |
|     27 |  793 | `		return;` |
|      - |  794 | `	}` |
|     74 |  795 | `	SyRandomness(&pVm->sPrng,&nRand,sizeof(nRand));` |
|     74 |  796 | `	if( (sxi64)((double)iDiv * ((double)nRand / 4294967296.0)) < iProb ){` |
|      4 |  797 | `		VmSessGc(pVm);` |
|      1 |  798 | `	}` |
|     51 |  799 | `}` |
|      - |  800 | `/*` |
|      - |  801 | ` * Load the stored copy into $_SESSION, which php always starts EMPTY here — only` |
|      - |  802 | ` * session_decode() overlays what is already there. Answers 1 when the session is` |
|      - |  803 | ` * loaded, 0 when the store was destroyed instead, and -1 for a pending exception.` |
|      - |  804 | ` */` |
|     94 |  805 | `static int VmSessLoad(ph7_context *pCtx,SyBlob *pFile,const char *zFunc)` |
|      4 |  806 | `{` |
|     98 |  807 | `	ph7_vm *pVm = pCtx->pVm;` |
|     98 |  808 | `	ph7_value *pSess = VmSessArray(pVm);` |
|      - |  809 | `	ph7_value sRes;` |
|     98 |  810 | `	int iDec = 1;` |
|     98 |  811 | `	PH7_MemObjInit(pVm,&sRes);` |
|     98 |  812 | `	if( pSess ){` |
|      - |  813 | `		int bRead;` |
|     98 |  814 | `		VmSessEmptyArray(pVm,pSess);` |
|     98 |  815 | `		if( VmSessHasUser(pVm) ){` |
|     27 |  816 | `			VmSessUserOpen(pVm);` |
|     51 |  817 | `			bRead = VmSessUserId(pVm,VM_SESS_OP_READ,&sRes) > 0` |
|     24 |  818 | `				&& (sRes.iFlags & MEMOBJ_STRING) && SyBlobLength(&sRes.sBlob) > 0;` |
|     15 |  819 | `		}else{` |
|     74 |  820 | `			bRead = VmSessReadFile(pVm,pFile,&sRes);` |
|      - |  821 | `		}` |
|      - |  822 | `		/* What the store handed back is what session.lazy_write compares the` |
|      - |  823 | `		 * eventual write against. */` |
|     98 |  824 | `		SyBlobReset(&pVm->sSessData);` |
|     98 |  825 | `		if( bRead ){` |
|     82 |  826 | `			SyBlobAppend(&pVm->sSessData,SyBlobData(&sRes.sBlob),SyBlobLength(&sRes.sBlob));` |
|     39 |  827 | `		}` |
|     98 |  828 | `		if( bRead ){` |
|    121 |  829 | `			iDec = VmSessDecodeInto(pCtx,(const char *)SyBlobData(&sRes.sBlob),` |
|     39 |  830 | `				SyBlobLength(&sRes.sBlob),pSess);` |
|     39 |  831 | `		}` |
|     47 |  832 | `	}` |
|     98 |  833 | `	PH7_MemObjRelease(&sRes);` |
|     98 |  834 | `	if( iDec == 0 ){` |
|    ! 0 |  835 | `		VmSessDestroyBadStore(pVm,pFile,zFunc);` |
|    ! 0 |  836 | `	}` |
|     98 |  837 | `	return iDec;` |
|      4 |  838 | `}` |
|      - |  839 | `/* int session_status() */` |
|     46 |  840 | `static int vm_builtin_session_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  841 | `{` |
|     50 |  842 | `	ph7_vm *pVm = pCtx->pVm;` |
|     23 |  843 | `	SXUNUSED(nArg);` |
|     23 |  844 | `	SXUNUSED(apArg);` |
|      - |  845 | `	/* A serializer name with no handler behind it leaves php's session module` |
|      - |  846 | `	 * DISABLED — reported from the start, before anything tries to open one. */` |
|     50 |  847 | `	ph7_result_int(pCtx,VmSessSerializerOrErr(pVm) < 0 ? 0 : pVm->iSessStatus);` |
|     50 |  848 | `	return PH7_OK;` |
|      4 |  849 | `}` |
|      - |  850 | `/*` |
|      - |  851 | ` * The three "cannot change while active / after headers" guards php puts on the` |
|      - |  852 | ` * id, the name and the save path. Answers TRUE (warning already raised) when the` |
|      - |  853 | ` * write must be refused.` |
|      - |  854 | ` */` |
|    124 |  855 | `static int VmSessLocked(ph7_context *pCtx,const char *zFunc,const char *zWhat,int bHeaders)` |
|      4 |  856 | `{` |
|    128 |  857 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  858 | `	char zMsg[192];` |
|    128 |  859 | `	if( pVm->iSessStatus == VM_SESSION_ACTIVE ){` |
|      4 |  860 | `		SyBufferFormat(zMsg,sizeof(zMsg),"%s(): %s cannot be changed when a session is active",` |
|      1 |  861 | `			zFunc,zWhat);` |
|      3 |  862 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      3 |  863 | `		return 1;` |
|      - |  864 | `	}` |
|    126 |  865 | `	if( bHeaders && pVm->bHeadersSent ){` |
|    ! 0 |  866 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|    ! 0 |  867 | `			"%s(): %s cannot be changed after headers have already been sent",zFunc,zWhat);` |
|    ! 0 |  868 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|    ! 0 |  869 | `		return 1;` |
|      - |  870 | `	}` |
|    126 |  871 | `	return 0;` |
|     66 |  872 | `}` |
|      - |  873 | `/*` |
|      - |  874 | ` * session_id() / session_name() / session_save_path(): read the current value,` |
|      - |  875 | ` * or set it and answer the previous one. One body, three directives.` |
|      - |  876 | ` */` |
|    148 |  877 | `static int VmSessAccessor(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|      - |  878 | `	SyBlob *pSlot,const char *zFunc,const char *zWhat,int bRtrimSlash)` |
|      4 |  879 | `{` |
|    152 |  880 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  881 | `	SyBlob sOld;` |
|    152 |  882 | `	if( pSlot == &pVm->sSessPath ){` |
|     22 |  883 | `		VmSessResolvePath(pVm);` |
|      9 |  884 | `	}` |
|    152 |  885 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|    152 |  886 | `	SyBlobAppend(&sOld,SyBlobData(pSlot),SyBlobLength(pSlot));` |
|    152 |  887 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|     58 |  888 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|     58 |  889 | `		SyBlobRelease(&sOld);` |
|     58 |  890 | `		return PH7_OK;` |
|      - |  891 | `	}` |
|     98 |  892 | `	if( VmSessLocked(pCtx,zFunc,zWhat,pSlot != &pVm->sSessPath) ){` |
|    ! 0 |  893 | `		SyBlobRelease(&sOld);` |
|    ! 0 |  894 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  895 | `		return PH7_OK;` |
|      - |  896 | `	}` |
|      - |  897 | `	{` |
|     98 |  898 | `		int nNew = 0;` |
|     98 |  899 | `		const char *zNew = ph7_value_to_string(apArg[0],&nNew);` |
|     98 |  900 | `		sxu32 nLen = (sxu32)nNew;` |
|     98 |  901 | `		if( bRtrimSlash ){` |
|     22 |  902 | `			while( nLen > 0 && zNew[nLen-1] == '/' ){ nLen--; }` |
|      9 |  903 | `		}` |
|     98 |  904 | `		SyBlobReset(pSlot);` |
|     98 |  905 | `		SyBlobAppend(pSlot,zNew,nLen);` |
|      - |  906 | `	}` |
|     98 |  907 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|     98 |  908 | `	SyBlobRelease(&sOld);` |
|     98 |  909 | `	return PH7_OK;` |
|     78 |  910 | `}` |
|    110 |  911 | `static int vm_builtin_session_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  912 | `{` |
|    114 |  913 | `	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessId,` |
|      - |  914 | `		"session_id","Session ID",0);` |
|      4 |  915 | `}` |
|     20 |  916 | `static int vm_builtin_session_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  917 | `{` |
|     22 |  918 | `	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessName,` |
|      - |  919 | `		"session_name","Session name",0);` |
|      2 |  920 | `}` |
|     18 |  921 | `static int vm_builtin_session_save_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  922 | `{` |
|     22 |  923 | `	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessPath,` |
|      - |  924 | `		"session_save_path","Session save path",1);` |
|      4 |  925 | `}` |
|      - |  926 | `/*` |
|      - |  927 | ` * session_start()'s $options array -- declared in aBuiltinSig[] and read by` |
|      - |  928 | `` * NOTHING, so `session_start(['name' => 'MYSID', 'cookie_lifetime' => 3600])`,`` |
|      - |  929 | ` * php's documented way to configure a session at the one point it can still be` |
|      - |  930 | ` * configured, was accepted and dropped in silence.` |
|      - |  931 | ` *` |
|      - |  932 | ` * Each key is a session.<key> directive applied for this request; the one that is` |
|      - |  933 | `` * not is `read_and_close`, which asks for the session to be closed again the`` |
|      - |  934 | ` * moment it has been read. A key the directive table refuses is reported and the` |
|      - |  935 | ` * session still starts; a key that is not a STRING, or a value that is not a` |
|      - |  936 | ` * scalar, is a hard error before anything is opened.` |
|      - |  937 | ` */` |
|      - |  938 | `static void VmSessWrite(ph7_vm *pVm,const char *zWho);` |
|      - |  939 | `static void VmSessSendCacheHeaders(ph7_vm *pVm);` |
|      - |  940 | `struct VmSessStartOpts {` |
|      - |  941 | `	ph7_vm *pVm;` |
|      - |  942 | `	int bReadClose;` |
|      - |  943 | `	int iFail;          /* 1 = non-string key, 2 = non-scalar value */` |
|      - |  944 | `	char zBadKey[64];` |
|      - |  945 | `	char zBadType[32];` |
|      - |  946 | `};` |
|     34 |  947 | `static int VmSessStartOptWalker(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 |  948 | `{` |
|     35 |  949 | `	struct VmSessStartOpts *pOpt = (struct VmSessStartOpts *)pUserData;` |
|      - |  950 | `	char zName[128];` |
|      - |  951 | `	const char *zKey;` |
|     35 |  952 | `	int nKey = 0, nVal = 0;` |
|      - |  953 | `	const char *zVal;` |
|     35 |  954 | `	if( !ph7_value_is_string(pKey) ){` |
|      3 |  955 | `		pOpt->iFail = 1;` |
|      3 |  956 | `		return SXERR_ABORT;` |
|      - |  957 | `	}` |
|     33 |  958 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     33 |  959 | `	if( nKey > (int)sizeof(pOpt->zBadKey)-1 ){` |
|    ! 0 |  960 | `		nKey = (int)sizeof(pOpt->zBadKey)-1;` |
|    ! 0 |  961 | `	}` |
|     33 |  962 | `	SyMemcpy(zKey,pOpt->zBadKey,(sxu32)nKey);` |
|     33 |  963 | `	pOpt->zBadKey[nKey] = 0;` |
|     33 |  964 | `	if( !ph7_value_is_string(pVal) && !ph7_value_is_int(pVal) && !ph7_value_is_bool(pVal) ){` |
|      3 |  965 | `		const char *zType = PH7_MemObjTypeDump(pVal);` |
|      3 |  966 | `		sxu32 nType = (sxu32)SyStrlen(zType);` |
|      3 |  967 | `		if( nType > sizeof(pOpt->zBadType)-1 ){` |
|    ! 0 |  968 | `			nType = sizeof(pOpt->zBadType)-1;` |
|    ! 0 |  969 | `		}` |
|      3 |  970 | `		SyMemcpy(zType,pOpt->zBadType,nType);` |
|      3 |  971 | `		pOpt->zBadType[nType] = 0;` |
|      3 |  972 | `		pOpt->iFail = 2;` |
|      3 |  973 | `		return SXERR_ABORT;` |
|      - |  974 | `	}` |
|     30 |  975 | `	if( nKey == (int)sizeof("read_and_close")-1` |
|     18 |  976 | `	 && SyMemcmp(pOpt->zBadKey,"read_and_close",(sxu32)nKey) == 0 ){` |
|      3 |  977 | `		pOpt->bReadClose = ph7_value_to_bool(pVal);` |
|      3 |  978 | `		return PH7_OK;` |
|      - |  979 | `	}` |
|      - |  980 | `	/* php stringifies the value the way ini_set() does, a bool becoming "1"/"". */` |
|     29 |  981 | `	if( ph7_value_is_bool(pVal) ){` |
|    ! 0 |  982 | `		zVal = ph7_value_to_bool(pVal) ? "1" : "";` |
|    ! 0 |  983 | `		nVal = (int)SyStrlen(zVal);` |
|    ! 0 |  984 | `	}else{` |
|     29 |  985 | `		zVal = ph7_value_to_string(pVal,&nVal);` |
|      - |  986 | `	}` |
|     29 |  987 | `	SyBufferFormat(zName,sizeof(zName),"session.%s",pOpt->zBadKey);` |
|     29 |  988 | `	if( !PH7_VmIniSet(pOpt->pVm,zName,(sxu32)SyStrlen(zName),zVal,(sxu32)nVal,` |
|      - |  989 | `		"session_start()") ){` |
|      - |  990 | `		char zMsg[160];` |
|     13 |  991 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      8 |  992 | `			"session_start(): Setting option \"%s\" failed",pOpt->zBadKey);` |
|      9 |  993 | `		PH7_VmThrowError(pOpt->pVm,0,PH7_CTX_WARNING,zMsg);` |
|      4 |  994 | `	}` |
|     29 |  995 | `	return PH7_OK;` |
|     18 |  996 | `}` |
|      - |  997 | `/*` |
|      - |  998 | ` * The Set-Cookie that carries the id, built out of the seven session.cookie_*` |
|      - |  999 | ` * directives rather than the name and the id alone -- which is what this used to` |
|      - | 1000 | ` * send, so every session cookie went out with no path, no expiry and no` |
|      - | 1001 | ` * HttpOnly/SameSite whatever the configuration said, and a program that had set` |
|      - | 1002 | `` * `session.cookie_secure` was still handing its id to a plaintext request.`` |
|      - | 1003 | ` *` |
|      - | 1004 | ` * php sends it on every start and again whenever the id changes; it is a plain` |
|      - | 1005 | ` * response header, so the CLI has nowhere to put it and simply does not.` |
|      - | 1006 | ` */` |
|    104 | 1007 | `static void VmSessSendCookie(ph7_vm *pVm)` |
|      4 | 1008 | `{` |
|      - | 1009 | `	SyBlob sPath,sDomain,sSame;` |
|      - | 1010 | `	sxi64 iLife;` |
|    108 | 1011 | `	if( !PH7_VmIniGetBool(pVm,"session.use_cookies",1) ){` |
|    ! 0 | 1012 | `		return;` |
|      - | 1013 | `	}` |
|      - | 1014 | `	/* Replace, never accumulate: the reply carries ONE id. */` |
|    160 | 1015 | `	PH7_VmRemoveCookieByName(pVm,(const char *)SyBlobData(&pVm->sSessName),` |
|     52 | 1016 | `		SyBlobLength(&pVm->sSessName));` |
|    108 | 1017 | `	SyBlobInit(&sPath,&pVm->sAllocator);` |
|    108 | 1018 | `	SyBlobInit(&sDomain,&pVm->sAllocator);` |
|    108 | 1019 | `	SyBlobInit(&sSame,&pVm->sAllocator);` |
|    108 | 1020 | `	PH7_VmIniGetStr(pVm,"session.cookie_path",&sPath);` |
|    108 | 1021 | `	PH7_VmIniGetStr(pVm,"session.cookie_domain",&sDomain);` |
|    108 | 1022 | `	PH7_VmIniGetStr(pVm,"session.cookie_samesite",&sSame);` |
|    108 | 1023 | `	iLife = PH7_VmIniGetInt(pVm,"session.cookie_lifetime",0);` |
|    264 | 1024 | `	PH7_VmEmitCookie(pVm,` |
|    104 | 1025 | `		(const char *)SyBlobData(&pVm->sSessName),SyBlobLength(&pVm->sSessName),` |
|    104 | 1026 | `		(const char *)SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId),1,` |
|      - | 1027 | `		/* A lifetime is a DURATION here and an absolute time on the wire; 0 is` |
|      - | 1028 | `		 * php's "until the browser closes", which sends no expiry at all. */` |
|     54 | 1029 | `		iLife > 0 ? (sxi64)time(0) + iLife : 0,` |
|    104 | 1030 | `		(const char *)SyBlobData(&sPath),SyBlobLength(&sPath),` |
|    104 | 1031 | `		(const char *)SyBlobData(&sDomain),SyBlobLength(&sDomain),` |
|     52 | 1032 | `		PH7_VmIniGetBool(pVm,"session.cookie_secure",0),` |
|     52 | 1033 | `		PH7_VmIniGetBool(pVm,"session.cookie_httponly",0),` |
|    104 | 1034 | `		(const char *)SyBlobData(&sSame),SyBlobLength(&sSame),` |
|     52 | 1035 | `		PH7_VmIniGetBool(pVm,"session.cookie_partitioned",0));` |
|    108 | 1036 | `	SyBlobRelease(&sPath);` |
|    108 | 1037 | `	SyBlobRelease(&sDomain);` |
|    108 | 1038 | `	SyBlobRelease(&sSame);` |
|     56 | 1039 | `}` |
|      - | 1040 | `/* bool session_start(array $options = []) */` |
|    108 | 1041 | `static int vm_builtin_session_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1042 | `{` |
|    112 | 1043 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1044 | `	struct VmSessStartOpts sOpt;` |
|      - | 1045 | `	SyBlob sFile;` |
|      - | 1046 | `	int iLoad;` |
|    112 | 1047 | `	SyZero(&sOpt,sizeof(sOpt));` |
|    112 | 1048 | `	sOpt.pVm = pVm;` |
|    112 | 1049 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|     19 | 1050 | `		ph7_array_walk(apArg[0],VmSessStartOptWalker,&sOpt);` |
|     19 | 1051 | `		if( sOpt.iFail == 1 ){` |
|      3 | 1052 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1053 | `				"session_start(): Argument #1 ($options) must be of type array with"` |
|      - | 1054 | `				" keys as string");` |
|      - | 1055 | `		}` |
|     17 | 1056 | `		if( sOpt.iFail == 2 ){` |
|      4 | 1057 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1058 | `				"session_start(): Option \"%s\" must be of type string\|int\|bool, %s given",` |
|      1 | 1059 | `				sOpt.zBadKey,sOpt.zBadType);` |
|      - | 1060 | `		}` |
|      7 | 1061 | `	}` |
|    108 | 1062 | `	if( pVm->iSessStatus == VM_SESSION_ACTIVE ){` |
|    ! 0 | 1063 | `		PH7_VmThrowError(pVm,0,PH7_CTX_NOTICE,` |
|      - | 1064 | `			"session_start(): Ignoring session_start() because a session is already active");` |
|    ! 0 | 1065 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 1066 | `		return PH7_OK;` |
|      - | 1067 | `	}` |
|    108 | 1068 | `	if( pVm->bHeadersSent ){` |
|      3 | 1069 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1070 | `			"session_start(): Session cannot be started after headers have already been sent");` |
|      3 | 1071 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1072 | `		return PH7_OK;` |
|      - | 1073 | `	}` |
|    106 | 1074 | `	if( VmSessSerializerOrErr(pVm) < 0 ){` |
|      - | 1075 | `		/* Only php.ini / -d can arm a handler that does not exist; php reports it` |
|      - | 1076 | `		 * here and starts nothing at all. */` |
|      - | 1077 | `		SyBlob sVal;` |
|      - | 1078 | `		char zMsg[192];` |
|      3 | 1079 | `		SyBlobInit(&sVal,&pVm->sAllocator);` |
|      3 | 1080 | `		PH7_VmIniGetStr(pVm,"session.serialize_handler",&sVal);` |
|      4 | 1081 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 1082 | `			"session_start(): Cannot find session serialization handler \"%.*s\""` |
|      - | 1083 | `			" - session startup failed",` |
|      2 | 1084 | `			(int)SyBlobLength(&sVal),(const char *)SyBlobData(&sVal));` |
|      3 | 1085 | `		SyBlobRelease(&sVal);` |
|      3 | 1086 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      3 | 1087 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1088 | `		return PH7_OK;` |
|      - | 1089 | `	}` |
|    100 | 1090 | `	if( SyBlobLength(&pVm->sSessId) > 0` |
|    101 | 1091 | `	 && VmSessIdDangerous((const char *)SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId)) ){` |
|      - | 1092 | `		/* A byte that would break the header this id is about to be written into:` |
|      - | 1093 | `		 * php throws the id away without a word and makes a fresh one. */` |
|      5 | 1094 | `		SyBlobReset(&pVm->sSessId);` |
|      2 | 1095 | `	}` |
|    104 | 1096 | `	if( SyBlobLength(&pVm->sSessId) == 0 ){` |
|      - | 1097 | `		/* Adopt the id the client sent, when it is one php would accept. An id a` |
|      - | 1098 | `		 * REQUEST supplied is simply not adopted when it is not — the visitor does` |
|      - | 1099 | `		 * not get to end the request — where the one a script SET is reported below. */` |
|     13 | 1100 | `		ph7_value *pCookie = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|     13 | 1101 | `		if( pCookie && (pCookie->iFlags & MEMOBJ_HASHMAP) ){` |
|      - | 1102 | `			ph7_value sKey,sVal;` |
|     13 | 1103 | `			ph7_hashmap_node *pNode = 0;` |
|     18 | 1104 | `			VmSessStrArg(pVm,&sKey,(const char *)SyBlobData(&pVm->sSessName),` |
|      5 | 1105 | `				SyBlobLength(&pVm->sSessName));` |
|     13 | 1106 | `			PH7_MemObjInit(pVm,&sVal);` |
|     10 | 1107 | `			if( PH7_HashmapLookup((ph7_hashmap *)pCookie->x.pOther,&sKey,&pNode) == SXRET_OK` |
|      8 | 1108 | `			 && pNode ){` |
|    ! 0 | 1109 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|    ! 0 | 1110 | `			}` |
|     10 | 1111 | `			if( (sVal.iFlags & MEMOBJ_STRING)` |
|      8 | 1112 | `			 && VmSessIdValid((const char *)SyBlobData(&sVal.sBlob),SyBlobLength(&sVal.sBlob)) ){` |
|    ! 0 | 1113 | `				SyBlobReset(&pVm->sSessId);` |
|    ! 0 | 1114 | `				SyBlobAppend(&pVm->sSessId,SyBlobData(&sVal.sBlob),SyBlobLength(&sVal.sBlob));` |
|    ! 0 | 1115 | `			}` |
|     13 | 1116 | `			PH7_MemObjRelease(&sVal);` |
|     13 | 1117 | `			PH7_MemObjRelease(&sKey);` |
|      5 | 1118 | `		}` |
|      5 | 1119 | `	}` |
|    100 | 1120 | `	if( SyBlobLength(&pVm->sSessId) > 0` |
|     99 | 1121 | `	 && !VmSessIdValid((const char *)SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId)) ){` |
|      - | 1122 | `		/* The id names a FILE under the save path, so php refuses to open anything at` |
|      - | 1123 | `		 * all for one outside its alphabet — and reports the store's own failure with` |
|      - | 1124 | `		 * it, since that is the read that never happened. */` |
|      - | 1125 | `		char zMsg[224];` |
|      5 | 1126 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1127 | `			"session_start(): Session ID is too long or contains illegal characters."` |
|      - | 1128 | `			" Only the A-Z, a-z, 0-9, \"-\", and \",\" characters are allowed");` |
|      5 | 1129 | `		VmSessResolvePath(pVm);` |
|      7 | 1130 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 1131 | `			"session_start(): Failed to read session data: files (path: %.*s)",` |
|      4 | 1132 | `			(int)SyBlobLength(&pVm->sSessPath),(const char *)SyBlobData(&pVm->sSessPath));` |
|      5 | 1133 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      5 | 1134 | `		SyBlobReset(&pVm->sSessId);` |
|      5 | 1135 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1136 | `		return PH7_OK;` |
|      - | 1137 | `	}` |
|     96 | 1138 | `	if( SyBlobLength(&pVm->sSessId) > 0` |
|     95 | 1139 | `	 && PH7_VmIniGetBool(pVm,"session.use_strict_mode",0) ){` |
|      - | 1140 | `		/* session.use_strict_mode: php refuses an id no STORE has ever seen and` |
|      - | 1141 | `		 * makes a new one, so a visitor cannot choose their own session id and` |
|      - | 1142 | `		 * hand the link to somebody else (session fixation). A userland handler` |
|      - | 1143 | `		 * answers through validateId(); the files store answers by whether the` |
|      - | 1144 | `		 * store is there. */` |
|      - | 1145 | `		int bKnown;` |
|      3 | 1146 | `		if( VmSessHasUser(pVm) ){` |
|      - | 1147 | `			ph7_value sRes;` |
|      3 | 1148 | `			VmSessUserOpen(pVm);` |
|      3 | 1149 | `			PH7_MemObjInit(pVm,&sRes);` |
|      3 | 1150 | `			if( VmSessUserId(pVm,VM_SESS_OP_VALIDATE,&sRes) > 0 ){` |
|      3 | 1151 | `				PH7_MemObjToBool(&sRes);` |
|      3 | 1152 | `				bKnown = sRes.x.iVal != 0;` |
|      2 | 1153 | `			}else{` |
|    ! 0 | 1154 | `				bKnown = 1;   /* no validateId(): php has nothing to refuse with */` |
|      - | 1155 | `			}` |
|      3 | 1156 | `			PH7_MemObjRelease(&sRes);` |
|      2 | 1157 | `		}else{` |
|      - | 1158 | `			SyBlob sProbe;` |
|    ! 0 | 1159 | `			SyBlobInit(&sProbe,&pVm->sAllocator);` |
|    ! 0 | 1160 | `			VmSessFile(pVm,&sProbe);` |
|    ! 0 | 1161 | `			bKnown = VmSessPathIs(pVm,"file_exists",&sProbe);` |
|    ! 0 | 1162 | `			SyBlobRelease(&sProbe);` |
|      - | 1163 | `		}` |
|      3 | 1164 | `		if( !bKnown ){` |
|      3 | 1165 | `			SyBlobReset(&pVm->sSessId);` |
|      1 | 1166 | `		}` |
|      1 | 1167 | `	}` |
|    100 | 1168 | `	if( SyBlobLength(&pVm->sSessId) == 0 && VmSessHasUser(pVm) ){` |
|      - | 1169 | `		/* A handler implementing SessionIdInterface makes the id: a store that` |
|      - | 1170 | `		 * knows how to key itself is the one that should choose the key. */` |
|      - | 1171 | `		ph7_value sRes;` |
|      3 | 1172 | `		VmSessUserOpen(pVm);` |
|      3 | 1173 | `		PH7_MemObjInit(pVm,&sRes);` |
|      3 | 1174 | `		if( VmSessUserCall(pVm,VM_SESS_OP_CREATE,0,0,&sRes) > 0 ){` |
|      3 | 1175 | `			PH7_MemObjToString(&sRes);` |
|      3 | 1176 | `			if( SyBlobLength(&sRes.sBlob) > 0 ){` |
|      3 | 1177 | `				SyBlobReset(&pVm->sSessId);` |
|      3 | 1178 | `				SyBlobAppend(&pVm->sSessId,SyBlobData(&sRes.sBlob),SyBlobLength(&sRes.sBlob));` |
|      1 | 1179 | `			}` |
|      1 | 1180 | `		}` |
|      3 | 1181 | `		PH7_MemObjRelease(&sRes);` |
|      1 | 1182 | `	}` |
|    100 | 1183 | `	if( SyBlobLength(&pVm->sSessId) == 0 ){` |
|     13 | 1184 | `		VmSessGenId(pVm,&pVm->sSessId);` |
|      5 | 1185 | `	}` |
|    100 | 1186 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|    100 | 1187 | `	VmSessFile(pVm,&sFile);` |
|    100 | 1188 | `	if( !VmSessHasUser(pVm) && !VmSessOpenStore(pVm,&sFile,"session_start") ){` |
|      3 | 1189 | `		SyBlobRelease(&sFile);` |
|      3 | 1190 | `		SyBlobReset(&pVm->sSessId);` |
|      3 | 1191 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1192 | `		return PH7_OK;` |
|      - | 1193 | `	}` |
|     98 | 1194 | `	iLoad = VmSessLoad(pCtx,&sFile,"session_start");` |
|     98 | 1195 | `	SyBlobRelease(&sFile);` |
|     98 | 1196 | `	if( iLoad < 0 ){` |
|    ! 0 | 1197 | `		return PH7_EXCEPTION;` |
|      - | 1198 | `	}` |
|     98 | 1199 | `	if( iLoad == 0 ){` |
|    ! 0 | 1200 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1201 | `		return PH7_OK;` |
|      - | 1202 | `	}` |
|      - | 1203 | `	/* php collects HERE -- after the store is open and this session has been read,` |
|      - | 1204 | `	 * which is the only order in which a handler's gc() can use the connection its` |
|      - | 1205 | `	 * own open() made. PHL swept before either, so a user handler was asked to` |
|      - | 1206 | `	 * collect a store it had not been told to open yet: its log read gc,open,read` |
|      - | 1207 | `	 * where php's reads open,read,gc. */` |
|     98 | 1208 | `	VmSessMaybeGc(pVm);` |
|     98 | 1209 | `	pVm->iSessStatus = VM_SESSION_ACTIVE;` |
|     98 | 1210 | `	VmSessSendCookie(pVm);` |
|     98 | 1211 | `	VmSessSendCacheHeaders(pVm);` |
|     98 | 1212 | `	if( sOpt.bReadClose ){` |
|      - | 1213 | ``		/* php's `read_and_close`: the store is read and released again before the`` |
|      - | 1214 | `		 * script runs, so a request that only READS the session does not hold its` |
|      - | 1215 | `		 * lock for the rest of its life. */` |
|      3 | 1216 | `		VmSessWrite(pVm,"session_start()");` |
|      1 | 1217 | `	}` |
|     98 | 1218 | `	ph7_result_bool(pCtx,1);` |
|     98 | 1219 | `	return PH7_OK;` |
|     58 | 1220 | `}` |
|      - | 1221 | `/*` |
|      - | 1222 | ` * Write the open session back to its file and close it. Shared by the builtin and` |
|      - | 1223 | ` * by the request-shutdown writer, which is why it takes only the VM: at shutdown` |
|      - | 1224 | ` * there is no calling frame to report against.` |
|      - | 1225 | ` */` |
|    122 | 1226 | `static void VmSessPutFileQuiet(ph7_vm *pVm,SyBlob *pFile,const char *zData,sxu32 nData,` |
|      - | 1227 | `	int bQuiet)` |
|      4 | 1228 | `{` |
|      - | 1229 | `	ph7_value sPath,sPayload;` |
|      - | 1230 | `	ph7_value *apA[2];` |
|    126 | 1231 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));` |
|    126 | 1232 | `	VmSessStrArg(pVm,&sPayload,zData,nData);` |
|    126 | 1233 | `	apA[0] = &sPath;` |
|    126 | 1234 | `	apA[1] = &sPayload;` |
|    126 | 1235 | `	if( bQuiet ){` |
|     46 | 1236 | `		VmSessCallQuiet(pVm,"file_put_contents",2,apA,0);` |
|     25 | 1237 | `	}else{` |
|     84 | 1238 | `		VmSessCall(pVm,"file_put_contents",2,apA,0);` |
|      - | 1239 | `	}` |
|    126 | 1240 | `	PH7_MemObjRelease(&sPath);` |
|    126 | 1241 | `	PH7_MemObjRelease(&sPayload);` |
|    126 | 1242 | `}` |
|     80 | 1243 | `static void VmSessPutFile(ph7_vm *pVm,SyBlob *pFile,const char *zData,sxu32 nData)` |
|      4 | 1244 | `{` |
|     84 | 1245 | `	VmSessPutFileQuiet(pVm,pFile,zData,nData,0);` |
|     84 | 1246 | `}` |
|      - | 1247 | `/* Encode the open session and write it to the file the CURRENT id names. */` |
|     86 | 1248 | `static void VmSessSave(ph7_vm *pVm,const char *zWho)` |
|      4 | 1249 | `{` |
|      - | 1250 | `	SyBlob sFile,sData;` |
|     90 | 1251 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|     90 | 1252 | `	SyBlobInit(&sData,&pVm->sAllocator);` |
|     90 | 1253 | `	VmSessFile(pVm,&sFile);` |
|      - | 1254 | `	/* An encode php refused still gets written — as the EMPTY payload, which is` |
|      - | 1255 | `	 * what its store is handed when the serializer answers nothing. The stale copy` |
|      - | 1256 | `	 * does not survive either way. */` |
|     90 | 1257 | `	VmSessEncode(pVm,&sData,zWho);` |
|     90 | 1258 | `	if( VmSessHasUser(pVm) ){` |
|      - | 1259 | `		ph7_value sId,sPayload,sRes;` |
|      - | 1260 | `		ph7_value *apA[2];` |
|     23 | 1261 | `		int iOp = VM_SESS_OP_WRITE;` |
|     20 | 1262 | `		if( PH7_VmIniGetBool(pVm,"session.lazy_write",1)` |
|     20 | 1263 | `		 && SyBlobLength(&pVm->sSessData) > 0` |
|     13 | 1264 | `		 && SyBlobLength(&sData) == SyBlobLength(&pVm->sSessData)` |
|      9 | 1265 | `		 && SyMemcmp(SyBlobData(&sData),SyBlobData(&pVm->sSessData),` |
|      6 | 1266 | `			SyBlobLength(&sData)) == 0 ){` |
|      - | 1267 | `			/* session.lazy_write, php's default: a session whose data did not` |
|      - | 1268 | `			 * change is not written again — the store is only told the session was` |
|      - | 1269 | `			 * USED, so its expiry moves without the payload going over the wire.` |
|      - | 1270 | `			 * A handler that does not implement the interface still gets write().` |
|      - | 1271 | `			 *` |
|      - | 1272 | `			 * "Did not change" is a comparison against what the READ returned, so` |
|      - | 1273 | `			 * there has to have BEEN one: php keeps the read's value and skips this` |
|      - | 1274 | `			 * branch entirely when the store had nothing (its read reports failure` |
|      - | 1275 | `			 * for an absent session). Comparing lengths alone made a BRAND-NEW id` |
|      - | 1276 | `			 * whose $_SESSION stayed empty look unchanged — both payloads empty — so` |
|      - | 1277 | `			 * the one call that would have created the store never happened, and a` |
|      - | 1278 | `			 * handler that implements write() but not the optional updateTimestamp()` |
|      - | 1279 | `			 * was never told about the session at all. */` |
|      6 | 1280 | `			iOp = VM_SESS_OP_UPDATE;` |
|      2 | 1281 | `		}` |
|     33 | 1282 | `		VmSessStrArg(pVm,&sId,(const char *)SyBlobData(&pVm->sSessId),` |
|     10 | 1283 | `			SyBlobLength(&pVm->sSessId));` |
|     23 | 1284 | `		VmSessStrArg(pVm,&sPayload,(const char *)SyBlobData(&sData),SyBlobLength(&sData));` |
|     23 | 1285 | `		PH7_MemObjInit(pVm,&sRes);` |
|     23 | 1286 | `		apA[0] = &sId;` |
|     23 | 1287 | `		apA[1] = &sPayload;` |
|     20 | 1288 | `		if( iOp == VM_SESS_OP_WRITE` |
|     15 | 1289 | `		 \|\| VmSessUserCall(pVm,VM_SESS_OP_UPDATE,2,apA,&sRes) == 0 ){` |
|     19 | 1290 | `			VmSessUserCall(pVm,VM_SESS_OP_WRITE,2,apA,&sRes);` |
|      8 | 1291 | `		}` |
|     23 | 1292 | `		PH7_MemObjRelease(&sRes);` |
|     23 | 1293 | `		PH7_MemObjRelease(&sPayload);` |
|     23 | 1294 | `		PH7_MemObjRelease(&sId);` |
|     13 | 1295 | `	}else{` |
|     70 | 1296 | `		VmSessPutFile(pVm,&sFile,(const char *)SyBlobData(&sData),SyBlobLength(&sData));` |
|      - | 1297 | `	}` |
|     90 | 1298 | `	SyBlobRelease(&sFile);` |
|     90 | 1299 | `	SyBlobRelease(&sData);` |
|     90 | 1300 | `}` |
|     78 | 1301 | `static void VmSessWrite(ph7_vm *pVm,const char *zWho)` |
|      4 | 1302 | `{` |
|     82 | 1303 | `	VmSessSave(pVm,zWho);` |
|     82 | 1304 | `	VmSessCloseStore(pVm);` |
|     82 | 1305 | `	pVm->iSessStatus = VM_SESSION_NONE;` |
|     82 | 1306 | `}` |
|      - | 1307 | `/* bool session_write_close() / session_commit() */` |
|     66 | 1308 | `static int vm_builtin_session_write_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1309 | `{` |
|     70 | 1310 | `	ph7_vm *pVm = pCtx->pVm;` |
|     33 | 1311 | `	SXUNUSED(nArg);` |
|     33 | 1312 | `	SXUNUSED(apArg);` |
|     70 | 1313 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      3 | 1314 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1315 | `		return PH7_OK;` |
|      - | 1316 | `	}` |
|     68 | 1317 | `	VmSessWrite(pVm,"session_write_close()");` |
|     68 | 1318 | `	ph7_result_bool(pCtx,1);` |
|     68 | 1319 | `	return PH7_OK;` |
|     37 | 1320 | `}` |
|      - | 1321 | `/*` |
|      - | 1322 | ` * php writes an open session back at REQUEST SHUTDOWN — from the session module's` |
|      - | 1323 | ` * own RSHUTDOWN, which runs after the script's register_shutdown_function()` |
|      - | 1324 | ` * callbacks and after the output buffers are flushed. Registering the writer as a` |
|      - | 1325 | ``  * shutdown callback (what this did) put it FIRST in that list, so a `$_SESSION` `` |
|      - | 1326 | ` * entry written from inside a shutdown callback — the flash-message / last-seen` |
|      - | 1327 | ` * idiom — was silently dropped.` |
|      - | 1328 | ` */` |
|   4562 | 1329 | `PH7_PRIVATE void PH7_VmSessionShutdown(ph7_vm *pVm)` |
|      5 | 1330 | `{` |
|   4567 | 1331 | `	if( pVm->iSessStatus == VM_SESSION_ACTIVE ){` |
|      - | 1332 | `		/* php names this caller "PHP Request Shutdown" — there is no frame to` |
|      - | 1333 | `		 * report against, so a serializer diagnostic raised here says so. */` |
|     12 | 1334 | `		VmSessWrite(pVm,"PHP Request Shutdown");` |
|      6 | 1335 | `	}` |
|   4567 | 1336 | `}` |
|      - | 1337 | `/* bool session_abort() — drop the in-memory session without writing it back */` |
|     10 | 1338 | `static int vm_builtin_session_abort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1339 | `{` |
|      5 | 1340 | `	SXUNUSED(nArg);` |
|      5 | 1341 | `	SXUNUSED(apArg);` |
|     12 | 1342 | `	if( pCtx->pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|    ! 0 | 1343 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1344 | `		return PH7_OK;` |
|      - | 1345 | `	}` |
|      - | 1346 | `	/* Nothing is written, but the store is still RELEASED: abandoning the session` |
|      - | 1347 | `	 * is the end of this request's use of it. */` |
|     12 | 1348 | `	VmSessCloseStore(pCtx->pVm);` |
|     12 | 1349 | `	pCtx->pVm->iSessStatus = VM_SESSION_NONE;` |
|     12 | 1350 | `	ph7_result_bool(pCtx,1);` |
|     12 | 1351 | `	return PH7_OK;` |
|      7 | 1352 | `}` |
|      - | 1353 | `/* bool session_reset() — re-read the stored copy over the in-memory one */` |
|    ! 0 | 1354 | `static int vm_builtin_session_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1355 | `{` |
|    ! 0 | 1356 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1357 | `	SyBlob sFile;` |
|      - | 1358 | `	int iLoad;` |
|    ! 0 | 1359 | `	SXUNUSED(nArg);` |
|    ! 0 | 1360 | `	SXUNUSED(apArg);` |
|    ! 0 | 1361 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|    ! 0 | 1362 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1363 | `		return PH7_OK;` |
|      - | 1364 | `	}` |
|    ! 0 | 1365 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|    ! 0 | 1366 | `	VmSessFile(pVm,&sFile);` |
|    ! 0 | 1367 | `	iLoad = VmSessLoad(pCtx,&sFile,"session_reset");` |
|    ! 0 | 1368 | `	SyBlobRelease(&sFile);` |
|    ! 0 | 1369 | `	if( iLoad < 0 ){` |
|    ! 0 | 1370 | `		return PH7_EXCEPTION;` |
|      - | 1371 | `	}` |
|      - | 1372 | `	/* php answers TRUE even when the store it just read was the one it had to` |
|      - | 1373 | `	 * destroy: the reset itself did happen. */` |
|    ! 0 | 1374 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 1375 | `	return PH7_OK;` |
|    ! 0 | 1376 | `}` |
|      - | 1377 | `/* bool session_unset() — empty $_SESSION, keep the session open */` |
|      2 | 1378 | `static int vm_builtin_session_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1379 | `{` |
|      3 | 1380 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1381 | `	ph7_value *pSess;` |
|      1 | 1382 | `	SXUNUSED(nArg);` |
|      1 | 1383 | `	SXUNUSED(apArg);` |
|      3 | 1384 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      3 | 1385 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1386 | `		return PH7_OK;` |
|      - | 1387 | `	}` |
|    ! 0 | 1388 | `	pSess = VmSessArray(pVm);` |
|    ! 0 | 1389 | `	if( pSess ){` |
|    ! 0 | 1390 | `		PH7_MemObjRelease(pSess);` |
|    ! 0 | 1391 | `		pSess->x.pOther = PH7_NewHashmap(pVm,0,0);` |
|    ! 0 | 1392 | `		if( pSess->x.pOther ){` |
|    ! 0 | 1393 | `			MemObjSetType(pSess,MEMOBJ_HASHMAP);` |
|    ! 0 | 1394 | `		}` |
|    ! 0 | 1395 | `	}` |
|    ! 0 | 1396 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 1397 | `	return PH7_OK;` |
|      2 | 1398 | `}` |
|      - | 1399 | `/* bool session_destroy() */` |
|      6 | 1400 | `static int vm_builtin_session_destroy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1401 | `{` |
|      8 | 1402 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1403 | `	SyBlob sFile;` |
|      3 | 1404 | `	SXUNUSED(nArg);` |
|      3 | 1405 | `	SXUNUSED(apArg);` |
|      8 | 1406 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      3 | 1407 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1408 | `			"session_destroy(): Trying to destroy uninitialized session");` |
|      3 | 1409 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1410 | `		return PH7_OK;` |
|      - | 1411 | `	}` |
|      6 | 1412 | `	if( VmSessHasUser(pVm) ){` |
|      - | 1413 | `		ph7_value sRes;` |
|      3 | 1414 | `		PH7_MemObjInit(pVm,&sRes);` |
|      3 | 1415 | `		VmSessUserId(pVm,VM_SESS_OP_DESTROY,&sRes);` |
|      3 | 1416 | `		PH7_MemObjRelease(&sRes);` |
|      - | 1417 | `		/* The store is released as well: destroying is the end of this session's` |
|      - | 1418 | `		 * use of it, exactly as closing is. */` |
|      3 | 1419 | `		VmSessCloseStore(pVm);` |
|      2 | 1420 | `	}else{` |
|      3 | 1421 | `		SyBlobInit(&sFile,&pVm->sAllocator);` |
|      3 | 1422 | `		VmSessFile(pVm,&sFile);` |
|      3 | 1423 | `		VmSessUnlinkIfExists(pVm,&sFile);` |
|      3 | 1424 | `		SyBlobRelease(&sFile);` |
|      - | 1425 | `	}` |
|      6 | 1426 | `	pVm->iSessStatus = VM_SESSION_NONE;` |
|      6 | 1427 | `	SyBlobReset(&pVm->sSessId);` |
|      6 | 1428 | `	ph7_result_bool(pCtx,1);` |
|      6 | 1429 | `	return PH7_OK;` |
|      5 | 1430 | `}` |
|      - | 1431 | `/* bool session_regenerate_id(bool $delete_old_session = false) */` |
|     12 | 1432 | `static int vm_builtin_session_regenerate_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1433 | `{` |
|     15 | 1434 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1435 | `	SyBlob sFile;` |
|     15 | 1436 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      3 | 1437 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1438 | `			"session_regenerate_id(): Session ID cannot be regenerated when there is no active session");` |
|      3 | 1439 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1440 | `		return PH7_OK;` |
|      - | 1441 | `	}` |
|     12 | 1442 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|     12 | 1443 | `	if( nArg > 0 && ph7_value_to_bool(apArg[0]) ){` |
|      3 | 1444 | `		VmSessFile(pVm,&sFile);` |
|      3 | 1445 | `		VmSessUnlinkIfExists(pVm,&sFile);` |
|      2 | 1446 | `	}else{` |
|      - | 1447 | `		/* The old id keeps what the session holds NOW. A regenerating request is the` |
|      - | 1448 | `		 * one whose reply may not arrive, so php leaves the previous id readable` |
|      - | 1449 | `		 * rather than a session that exists under neither id. */` |
|     10 | 1450 | `		VmSessSave(pVm,"session_regenerate_id()");` |
|      - | 1451 | `	}` |
|     12 | 1452 | `	VmSessGenId(pVm,&pVm->sSessId);` |
|      - | 1453 | `	/* The new id's store is OPENED, which for the files handler means it exists and` |
|      - | 1454 | `	 * is empty until the session closes over it. */` |
|     12 | 1455 | `	VmSessFile(pVm,&sFile);` |
|     12 | 1456 | `	VmSessPutFile(pVm,&sFile,"",0);` |
|     12 | 1457 | `	SyBlobRelease(&sFile);` |
|      - | 1458 | `	/* The client is told the new id here; without this the browser keeps sending` |
|      - | 1459 | `	 * the old one and the regenerated session is unreachable. */` |
|     12 | 1460 | `	VmSessSendCookie(pVm);` |
|     12 | 1461 | `	ph7_result_bool(pCtx,1);` |
|     12 | 1462 | `	return PH7_OK;` |
|      9 | 1463 | `}` |
|      - | 1464 | `/*` |
|      - | 1465 | ` * The seven cookie directives, in the order php's session_get_cookie_params()` |
|      - | 1466 | ` * reports them and with the TYPE each one is reported as: an int lifetime, three` |
|      - | 1467 | ` * bools, three strings.` |
|      - | 1468 | ` */` |
|      - | 1469 | `static const struct {` |
|      - | 1470 | `	const char *zKey;` |
|      - | 1471 | `	const char *zIni;` |
|      - | 1472 | `	int iKind;    /* 0 = string, 1 = int, 2 = bool */` |
|      - | 1473 | `} aSessCookieParam[] = {` |
|      - | 1474 | `	{ "lifetime",    "session.cookie_lifetime",    1 },` |
|      - | 1475 | `	{ "path",        "session.cookie_path",        0 },` |
|      - | 1476 | `	{ "domain",      "session.cookie_domain",      0 },` |
|      - | 1477 | `	{ "secure",      "session.cookie_secure",      2 },` |
|      - | 1478 | `	{ "partitioned", "session.cookie_partitioned", 2 },` |
|      - | 1479 | `	{ "httponly",    "session.cookie_httponly",    2 },` |
|      - | 1480 | `	{ "samesite",    "session.cookie_samesite",    0 },` |
|      - | 1481 | `};` |
|      - | 1482 | `/* array session_get_cookie_params() */` |
|      8 | 1483 | `static int vm_builtin_session_get_cookie_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1484 | `{` |
|      9 | 1485 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1486 | `	ph7_value *pArray,*pVal;` |
|      - | 1487 | `	sxu32 n;` |
|      4 | 1488 | `	SXUNUSED(nArg);` |
|      4 | 1489 | `	SXUNUSED(apArg);` |
|      9 | 1490 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 1491 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      9 | 1492 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 | 1493 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1494 | `		return PH7_OK;` |
|      - | 1495 | `	}` |
|     65 | 1496 | `	for( n = 0 ; n < SX_ARRAYSIZE(aSessCookieParam) ; n++ ){` |
|     57 | 1497 | `		if( aSessCookieParam[n].iKind == 1 ){` |
|      9 | 1498 | `			ph7_value_int64(pVal,PH7_VmIniGetInt(pVm,aSessCookieParam[n].zIni,0));` |
|     53 | 1499 | `		}else if( aSessCookieParam[n].iKind == 2 ){` |
|     25 | 1500 | `			ph7_value_bool(pVal,PH7_VmIniGetBool(pVm,aSessCookieParam[n].zIni,0));` |
|     13 | 1501 | `		}else{` |
|      - | 1502 | `			SyBlob sVal;` |
|     25 | 1503 | `			SyBlobInit(&sVal,&pVm->sAllocator);` |
|     25 | 1504 | `			PH7_VmIniGetStr(pVm,aSessCookieParam[n].zIni,&sVal);` |
|     37 | 1505 | `			ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sVal),` |
|     24 | 1506 | `				(const char *)SyBlobData(&sVal));` |
|     25 | 1507 | `			SyBlobRelease(&sVal);` |
|      - | 1508 | `		}` |
|     57 | 1509 | `		ph7_array_add_strkey_elem(pArray,aSessCookieParam[n].zKey,pVal);` |
|     57 | 1510 | `		ph7_value_reset_string_cursor(pVal);` |
|     29 | 1511 | `	}` |
|      9 | 1512 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 1513 | `	return PH7_OK;` |
|      5 | 1514 | `}` |
|      - | 1515 | `/* Push one cookie parameter through the ini table, which is its only store. */` |
|     18 | 1516 | `static void VmSessSetCookieIni(ph7_vm *pVm,const char *zIni,const char *zVal,int nVal)` |
|      1 | 1517 | `{` |
|      - | 1518 | `	ph7_value sName,sVal;` |
|      - | 1519 | `	ph7_value *apA[2];` |
|     19 | 1520 | `	VmSessStrArg(pVm,&sName,zIni,(sxu32)SyStrlen(zIni));` |
|     19 | 1521 | `	VmSessStrArg(pVm,&sVal,zVal,(sxu32)nVal);` |
|     19 | 1522 | `	apA[0] = &sName;` |
|     19 | 1523 | `	apA[1] = &sVal;` |
|     19 | 1524 | `	VmSessCall(pVm,"ini_set",2,apA,0);` |
|     19 | 1525 | `	PH7_MemObjRelease(&sName);` |
|     19 | 1526 | `	PH7_MemObjRelease(&sVal);` |
|     19 | 1527 | `}` |
|      6 | 1528 | `static void VmSessSetCookieBool(ph7_vm *pVm,const char *zIni,int bVal)` |
|      1 | 1529 | `{` |
|      7 | 1530 | `	VmSessSetCookieIni(pVm,zIni,bVal ? "1" : "0",1);` |
|      7 | 1531 | `}` |
|      - | 1532 | `struct VmSessCookieArgs {` |
|      - | 1533 | `	ph7_vm *pVm;` |
|      - | 1534 | `	int nApplied;` |
|      - | 1535 | `	char zBadKey[64];` |
|      - | 1536 | `};` |
|     12 | 1537 | `static int VmSessCookieOptWalker(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 | 1538 | `{` |
|     13 | 1539 | `	struct VmSessCookieArgs *pArgs = (struct VmSessCookieArgs *)pUserData;` |
|      - | 1540 | `	const char *zKey;` |
|     13 | 1541 | `	int nKey = 0;` |
|      - | 1542 | `	sxu32 n;` |
|     13 | 1543 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     63 | 1544 | `	for( n = 0 ; n < SX_ARRAYSIZE(aSessCookieParam) ; n++ ){` |
|     58 | 1545 | `		if( nKey == (int)SyStrlen(aSessCookieParam[n].zKey)` |
|     36 | 1546 | `		 && SyStrnicmp(zKey,aSessCookieParam[n].zKey,(sxu32)nKey) == 0 ){` |
|      9 | 1547 | `			if( aSessCookieParam[n].iKind == 2 ){` |
|      4 | 1548 | `				VmSessSetCookieBool(pArgs->pVm,aSessCookieParam[n].zIni,` |
|      1 | 1549 | `					ph7_value_to_bool(pVal));` |
|      2 | 1550 | `			}else{` |
|      7 | 1551 | `				int nVal = 0;` |
|      7 | 1552 | `				const char *zVal = ph7_value_to_string(pVal,&nVal);` |
|      7 | 1553 | `				VmSessSetCookieIni(pArgs->pVm,aSessCookieParam[n].zIni,zVal,nVal);` |
|      - | 1554 | `			}` |
|      9 | 1555 | `			pArgs->nApplied++;` |
|      9 | 1556 | `			return PH7_OK;` |
|      - | 1557 | `		}` |
|     26 | 1558 | `	}` |
|      - | 1559 | `	/* php reports the key and carries on; it is only an error when NONE of the` |
|      - | 1560 | `	 * keys were ones it knows. */` |
|      5 | 1561 | `	if( pArgs->zBadKey[0] == 0 ){` |
|      5 | 1562 | `		sxu32 nCopy = (sxu32)nKey;` |
|      5 | 1563 | `		if( nCopy > sizeof(pArgs->zBadKey)-1 ){` |
|    ! 0 | 1564 | `			nCopy = sizeof(pArgs->zBadKey)-1;` |
|    ! 0 | 1565 | `		}` |
|      5 | 1566 | `		SyMemcpy(zKey,pArgs->zBadKey,nCopy);` |
|      5 | 1567 | `		pArgs->zBadKey[nCopy] = 0;` |
|      2 | 1568 | `	}` |
|      - | 1569 | `	{` |
|      - | 1570 | `		char zMsg[160];` |
|      7 | 1571 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 1572 | `			"session_set_cookie_params(): Argument #1 ($lifetime_or_options)"` |
|      2 | 1573 | `			" contains an unrecognized key \"%.*s\"",nKey,zKey);` |
|      5 | 1574 | `		PH7_VmThrowError(pArgs->pVm,0,PH7_CTX_WARNING,zMsg);` |
|      - | 1575 | `	}` |
|      5 | 1576 | `	return PH7_OK;` |
|      7 | 1577 | `}` |
|      - | 1578 | `/*` |
|      - | 1579 | ` * bool session_set_cookie_params(array\|int $lifetime_or_options, ?string $path = null,` |
|      - | 1580 | ` *     ?string $domain = null, ?bool $secure = null, ?bool $httponly = null)` |
|      - | 1581 | ` */` |
|     16 | 1582 | `static int vm_builtin_session_set_cookie_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1583 | `{` |
|     17 | 1584 | `	ph7_vm *pVm = pCtx->pVm;` |
|     17 | 1585 | `	if( nArg < 1 ){` |
|    ! 0 | 1586 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      - | 1587 | `			"session_set_cookie_params() expects at least 1 argument, 0 given");` |
|      - | 1588 | `	}` |
|     17 | 1589 | `	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_int(apArg[0]) ){` |
|      4 | 1590 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1591 | `			"session_set_cookie_params(): Argument #1 ($lifetime_or_options) must be"` |
|      1 | 1592 | `			" of type array\|int, %s given",PH7_MemObjTypeDump(apArg[0]));` |
|      - | 1593 | `	}` |
|      - | 1594 | `	/* Both refusals are about the header the parameters would have gone into: one` |
|      - | 1595 | `	 * already written, or one this session already sent. */` |
|     15 | 1596 | `	if( VmSessLocked(pCtx,"session_set_cookie_params","Session cookie parameters",1) ){` |
|      3 | 1597 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1598 | `		return PH7_OK;` |
|      - | 1599 | `	}` |
|     13 | 1600 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 1601 | `		struct VmSessCookieArgs sArgs;` |
|      9 | 1602 | `		SyZero(&sArgs,sizeof(sArgs));` |
|      9 | 1603 | `		sArgs.pVm = pVm;` |
|      9 | 1604 | `		ph7_array_walk(apArg[0],VmSessCookieOptWalker,&sArgs);` |
|      9 | 1605 | `		if( sArgs.nApplied < 1 ){` |
|      5 | 1606 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1607 | `				"session_set_cookie_params(): Argument #1 ($lifetime_or_options) must"` |
|      - | 1608 | `				" contain at least 1 valid key");` |
|      - | 1609 | `		}` |
|      3 | 1610 | `	}else{` |
|      5 | 1611 | `		sxi64 iLife = ph7_value_to_int64(apArg[0]);` |
|      - | 1612 | `		char zLife[32];` |
|      - | 1613 | `		int nLife;` |
|      5 | 1614 | `		if( iLife < 0 ){` |
|      3 | 1615 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1616 | `				"session_set_cookie_params(): CookieLifetime cannot be negative");` |
|      3 | 1617 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1618 | `			return PH7_OK;` |
|      - | 1619 | `		}` |
|      3 | 1620 | `		nLife = SyBufferFormat(zLife,sizeof(zLife),"%qd",iLife);` |
|      3 | 1621 | `		VmSessSetCookieIni(pVm,"session.cookie_lifetime",zLife,nLife);` |
|      3 | 1622 | `		if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      3 | 1623 | `			int nVal = 0;` |
|      3 | 1624 | `			const char *zVal = ph7_value_to_string(apArg[1],&nVal);` |
|      3 | 1625 | `			VmSessSetCookieIni(pVm,"session.cookie_path",zVal,nVal);` |
|      1 | 1626 | `		}` |
|      3 | 1627 | `		if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      3 | 1628 | `			int nVal = 0;` |
|      3 | 1629 | `			const char *zVal = ph7_value_to_string(apArg[2],&nVal);` |
|      3 | 1630 | `			VmSessSetCookieIni(pVm,"session.cookie_domain",zVal,nVal);` |
|      1 | 1631 | `		}` |
|      3 | 1632 | `		if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|      3 | 1633 | `			VmSessSetCookieBool(pVm,"session.cookie_secure",ph7_value_to_bool(apArg[3]));` |
|      1 | 1634 | `		}` |
|      3 | 1635 | `		if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|      3 | 1636 | `			VmSessSetCookieBool(pVm,"session.cookie_httponly",ph7_value_to_bool(apArg[4]));` |
|      1 | 1637 | `		}` |
|      - | 1638 | `	}` |
|      7 | 1639 | `	ph7_result_bool(pCtx,1);` |
|      7 | 1640 | `	return PH7_OK;` |
|      9 | 1641 | `}` |
|      - | 1642 | `/*` |
|      - | 1643 | ` * php's four cache limiters, and the headers each one puts on a reply that` |
|      - | 1644 | `` * carries a session. A session is per-visitor state, so the default (`nocache`)`` |
|      - | 1645 | `` * tells every cache in the path not to keep the page at all; `public` is the`` |
|      - | 1646 | `` * opt-out, and the two `private` forms let the BROWSER keep it while no shared`` |
|      - | 1647 | ` * cache may. A limiter php does not know (including the empty one, which is how a` |
|      - | 1648 | ` * program turns this off) sends nothing -- php validates nothing here.` |
|      - | 1649 | ` *` |
|      - | 1650 | `` * `Expires: Thu, 19 Nov 1981 08:52:00 GMT` is php's own already-expired constant,`` |
|      - | 1651 | ` * and Last-Modified is the mtime of the script that started the session.` |
|      - | 1652 | ` */` |
|     92 | 1653 | `static void VmSessSendCacheHeaders(ph7_vm *pVm)` |
|      4 | 1654 | `{` |
|      - | 1655 | `	SyBlob sLimiter;` |
|      - | 1656 | `	const char *zLim;` |
|      - | 1657 | `	sxu32 nLim;` |
|      - | 1658 | `	sxi64 iExpire;` |
|      - | 1659 | `	char zBuf[128];` |
|      - | 1660 | `	int nBuf;` |
|     96 | 1661 | `	if( !pVm->bHttpContext ){` |
|     86 | 1662 | `		return;` |
|      - | 1663 | `	}` |
|     10 | 1664 | `	SyBlobInit(&sLimiter,&pVm->sAllocator);` |
|     10 | 1665 | `	PH7_VmIniGetStr(pVm,"session.cache_limiter",&sLimiter);` |
|     10 | 1666 | `	zLim = (const char *)SyBlobData(&sLimiter);` |
|     10 | 1667 | `	nLim = SyBlobLength(&sLimiter);` |
|     10 | 1668 | `	iExpire = PH7_VmIniGetInt(pVm,"session.cache_expire",180) * 60;` |
|     10 | 1669 | `	if( nLim == sizeof("nocache")-1 && SyMemcmp(zLim,"nocache",nLim) == 0 ){` |
|      4 | 1670 | `		PH7_VmSetResponseHeader(pVm,"Expires","Thu, 19 Nov 1981 08:52:00 GMT",` |
|      - | 1671 | `			sizeof("Thu, 19 Nov 1981 08:52:00 GMT")-1);` |
|      4 | 1672 | `		PH7_VmSetResponseHeader(pVm,"Cache-Control","no-store, no-cache, must-revalidate",` |
|      - | 1673 | `			sizeof("no-store, no-cache, must-revalidate")-1);` |
|      4 | 1674 | `		PH7_VmSetResponseHeader(pVm,"Pragma","no-cache",sizeof("no-cache")-1);` |
|      8 | 1675 | `	}else if( (nLim == sizeof("public")-1 && SyMemcmp(zLim,"public",nLim) == 0)` |
|      5 | 1676 | `	       \|\| (nLim == sizeof("private")-1 && SyMemcmp(zLim,"private",nLim) == 0)` |
|      3 | 1677 | `	       \|\| (nLim == sizeof("private_no_expire")-1` |
|      2 | 1678 | `	        && SyMemcmp(zLim,"private_no_expire",nLim) == 0) ){` |
|      6 | 1679 | `		int bPublic = nLim == sizeof("public")-1;` |
|      6 | 1680 | `		int bNoExpire = nLim == sizeof("private_no_expire")-1;` |
|      6 | 1681 | `		if( bPublic ){` |
|      2 | 1682 | `			nBuf = PH7_VmHttpDate((sxi64)time(0) + iExpire,zBuf,(int)sizeof(zBuf));` |
|      2 | 1683 | `			if( nBuf > 0 ){` |
|      2 | 1684 | `				PH7_VmSetResponseHeader(pVm,"Expires",zBuf,(sxu32)nBuf);` |
|      1 | 1685 | `			}` |
|      5 | 1686 | `		}else if( !bNoExpire ){` |
|      2 | 1687 | `			PH7_VmSetResponseHeader(pVm,"Expires","Thu, 19 Nov 1981 08:52:00 GMT",` |
|      - | 1688 | `				sizeof("Thu, 19 Nov 1981 08:52:00 GMT")-1);` |
|      1 | 1689 | `		}` |
|      9 | 1690 | `		nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%s, max-age=%qd",` |
|      3 | 1691 | `			bPublic ? "public" : "private",iExpire);` |
|      6 | 1692 | `		PH7_VmSetResponseHeader(pVm,"Cache-Control",zBuf,(sxu32)nBuf);` |
|      - | 1693 | `		{` |
|      - | 1694 | `			/* The document's own age: the script that is running. */` |
|      6 | 1695 | `			SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      6 | 1696 | `			if( pFile && pFile->nByte > 0 ){` |
|      - | 1697 | `				ph7_value sPath,sTime;` |
|      - | 1698 | `				ph7_value *apA[1];` |
|      6 | 1699 | `				VmSessStrArg(pVm,&sPath,pFile->zString,pFile->nByte);` |
|      6 | 1700 | `				PH7_MemObjInit(pVm,&sTime);` |
|      6 | 1701 | `				apA[0] = &sPath;` |
|      6 | 1702 | `				VmSessCallQuiet(pVm,"filemtime",1,apA,&sTime);` |
|      6 | 1703 | `				if( sTime.iFlags & MEMOBJ_INT ){` |
|      6 | 1704 | `					nBuf = PH7_VmHttpDate(sTime.x.iVal,zBuf,(int)sizeof(zBuf));` |
|      6 | 1705 | `					if( nBuf > 0 ){` |
|      6 | 1706 | `						PH7_VmSetResponseHeader(pVm,"Last-Modified",zBuf,(sxu32)nBuf);` |
|      3 | 1707 | `					}` |
|      3 | 1708 | `				}` |
|      6 | 1709 | `				PH7_MemObjRelease(&sTime);` |
|      6 | 1710 | `				PH7_MemObjRelease(&sPath);` |
|      3 | 1711 | `			}` |
|      - | 1712 | `		}` |
|      3 | 1713 | `	}` |
|     12 | 1714 | `	SyBlobRelease(&sLimiter);` |
|     51 | 1715 | `}` |
|      - | 1716 | `/* string\|false session_cache_limiter(?string $value = null) */` |
|     16 | 1717 | `static int vm_builtin_session_cache_limiter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1718 | `{` |
|     17 | 1719 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1720 | `	SyBlob sOld;` |
|     17 | 1721 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|     17 | 1722 | `	PH7_VmIniGetStr(pVm,"session.cache_limiter",&sOld);` |
|     17 | 1723 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|      9 | 1724 | `		int nVal = 0;` |
|      - | 1725 | `		const char *zVal;` |
|      9 | 1726 | `		if( pVm->iSessStatus == VM_SESSION_ACTIVE ){` |
|      - | 1727 | `			/* The headers went out with the session; there is nothing left to` |
|      - | 1728 | `			 * decide. */` |
|      3 | 1729 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1730 | `				"session_cache_limiter(): Session cache limiter cannot be changed"` |
|      - | 1731 | `				" when a session is active");` |
|      3 | 1732 | `			SyBlobRelease(&sOld);` |
|      3 | 1733 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1734 | `			return PH7_OK;` |
|      - | 1735 | `		}` |
|      7 | 1736 | `		zVal = ph7_value_to_string(apArg[0],&nVal);` |
|     10 | 1737 | `		PH7_VmIniSet(pVm,"session.cache_limiter",sizeof("session.cache_limiter")-1,` |
|      3 | 1738 | `			zVal,(sxu32)nVal,"session_cache_limiter()");` |
|      3 | 1739 | `	}` |
|     15 | 1740 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|     15 | 1741 | `	SyBlobRelease(&sOld);` |
|     15 | 1742 | `	return PH7_OK;` |
|      9 | 1743 | `}` |
|      - | 1744 | `/* int\|false session_cache_expire(?int $value = null) */` |
|     12 | 1745 | `static int vm_builtin_session_cache_expire(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1746 | `{` |
|     13 | 1747 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 | 1748 | `	sxi64 iOld = PH7_VmIniGetInt(pVm,"session.cache_expire",180);` |
|     13 | 1749 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|      7 | 1750 | `		if( pVm->iSessStatus == VM_SESSION_ACTIVE ){` |
|      - | 1751 | `			/* php answers the CURRENT value here rather than false, unlike its` |
|      - | 1752 | `			 * neighbour. */` |
|      3 | 1753 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1754 | `				"session_cache_expire(): Session cache expiration cannot be changed"` |
|      - | 1755 | `				" when a session is active");` |
|      2 | 1756 | `		}else{` |
|      - | 1757 | `			char zVal[32];` |
|      5 | 1758 | `			int nVal = SyBufferFormat(zVal,sizeof(zVal),"%qd",ph7_value_to_int64(apArg[0]));` |
|      7 | 1759 | `			PH7_VmIniSet(pVm,"session.cache_expire",sizeof("session.cache_expire")-1,` |
|      2 | 1760 | `				zVal,(sxu32)nVal,"session_cache_expire()");` |
|      - | 1761 | `		}` |
|      3 | 1762 | `	}` |
|     13 | 1763 | `	ph7_result_int64(pCtx,iOld);` |
|     13 | 1764 | `	return PH7_OK;` |
|      1 | 1765 | `}` |
|      - | 1766 | `/*` |
|      - | 1767 | `` * SessionHandler: php's built-in `files` store, exposed as a class so a program`` |
|      - | 1768 | `` * can DECORATE it -- `class Locking extends SessionHandler { public function`` |
|      - | 1769 | `` * read($id) { …; return parent::read($id); } }`. Its methods are the same C`` |
|      - | 1770 | ` * routines the engine's own store uses, so the two cannot drift.` |
|      - | 1771 | ` */` |
|      4 | 1772 | `static int vm_builtin_SessionHandler_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1773 | `{` |
|      5 | 1774 | `	ph7_vm *pVm = pCtx->pVm;` |
|      5 | 1775 | `	if( nArg > 0 ){` |
|      5 | 1776 | `		int nPath = 0;` |
|      5 | 1777 | `		const char *zPath = ph7_value_to_string(apArg[0],&nPath);` |
|      5 | 1778 | `		if( nPath > 0 ){` |
|      5 | 1779 | `			while( nPath > 0 && zPath[nPath-1] == '/' ){ nPath--; }` |
|      5 | 1780 | `			SyBlobReset(&pVm->sSessPath);` |
|      5 | 1781 | `			SyBlobAppend(&pVm->sSessPath,zPath,(sxu32)nPath);` |
|      2 | 1782 | `		}` |
|      2 | 1783 | `	}` |
|      5 | 1784 | `	ph7_result_bool(pCtx,1);` |
|      5 | 1785 | `	return PH7_OK;` |
|      1 | 1786 | `}` |
|      4 | 1787 | `static int vm_builtin_SessionHandler_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1788 | `{` |
|      2 | 1789 | `	SXUNUSED(nArg);` |
|      2 | 1790 | `	SXUNUSED(apArg);` |
|      5 | 1791 | `	ph7_result_bool(pCtx,1);` |
|      5 | 1792 | `	return PH7_OK;` |
|      1 | 1793 | `}` |
|      - | 1794 | `/* Build "<save_path>/sess_<id>" for an id the CALLER named. */` |
|      8 | 1795 | `static void VmSessFileFor(ph7_vm *pVm,ph7_value *pId,SyBlob *pOut)` |
|      1 | 1796 | `{` |
|      9 | 1797 | `	int nId = 0;` |
|      9 | 1798 | `	const char *zId = pId ? ph7_value_to_string(pId,&nId) : "";` |
|      9 | 1799 | `	VmSessResolvePath(pVm);` |
|      9 | 1800 | `	SyBlobReset(pOut);` |
|      9 | 1801 | `	SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|      9 | 1802 | `	SyBlobAppend(pOut,VM_SESS_FILE_PREFIX,sizeof(VM_SESS_FILE_PREFIX)-1);` |
|      9 | 1803 | `	SyBlobAppend(pOut,zId,(sxu32)nId);` |
|      9 | 1804 | `}` |
|      4 | 1805 | `static int vm_builtin_SessionHandler_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1806 | `{` |
|      5 | 1807 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1808 | `	SyBlob sFile;` |
|      - | 1809 | `	ph7_value sData;` |
|      5 | 1810 | `	if( nArg < 1 ){` |
|    ! 0 | 1811 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1812 | `		return PH7_OK;` |
|      - | 1813 | `	}` |
|      5 | 1814 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|      5 | 1815 | `	VmSessFileFor(pVm,apArg[0],&sFile);` |
|      5 | 1816 | `	PH7_MemObjInit(pVm,&sData);` |
|      5 | 1817 | `	if( VmSessReadFile(pVm,&sFile,&sData) ){` |
|    ! 0 | 1818 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sData.sBlob),` |
|    ! 0 | 1819 | `			(int)SyBlobLength(&sData.sBlob));` |
|    ! 0 | 1820 | `	}else{` |
|      - | 1821 | `		/* php's files handler answers the EMPTY string for a store that is not` |
|      - | 1822 | `		 * there; false is reserved for a read that failed. */` |
|      5 | 1823 | `		ph7_result_string(pCtx,"",0);` |
|      - | 1824 | `	}` |
|      5 | 1825 | `	PH7_MemObjRelease(&sData);` |
|      5 | 1826 | `	SyBlobRelease(&sFile);` |
|      5 | 1827 | `	return PH7_OK;` |
|      3 | 1828 | `}` |
|      4 | 1829 | `static int vm_builtin_SessionHandler_write(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1830 | `{` |
|      5 | 1831 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1832 | `	SyBlob sFile;` |
|      5 | 1833 | `	int nData = 0;` |
|      5 | 1834 | `	const char *zData = "";` |
|      5 | 1835 | `	if( nArg < 1 ){` |
|    ! 0 | 1836 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1837 | `		return PH7_OK;` |
|      - | 1838 | `	}` |
|      5 | 1839 | `	if( nArg > 1 ){` |
|      5 | 1840 | `		zData = ph7_value_to_string(apArg[1],&nData);` |
|      2 | 1841 | `	}` |
|      5 | 1842 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|      5 | 1843 | `	VmSessFileFor(pVm,apArg[0],&sFile);` |
|      5 | 1844 | `	VmSessPutFile(pVm,&sFile,zData,(sxu32)nData);` |
|      5 | 1845 | `	SyBlobRelease(&sFile);` |
|      5 | 1846 | `	ph7_result_bool(pCtx,1);` |
|      5 | 1847 | `	return PH7_OK;` |
|      3 | 1848 | `}` |
|    ! 0 | 1849 | `static int vm_builtin_SessionHandler_destroy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1850 | `{` |
|    ! 0 | 1851 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1852 | `	SyBlob sFile;` |
|    ! 0 | 1853 | `	if( nArg < 1 ){` |
|    ! 0 | 1854 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1855 | `		return PH7_OK;` |
|      - | 1856 | `	}` |
|    ! 0 | 1857 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|    ! 0 | 1858 | `	VmSessFileFor(pVm,apArg[0],&sFile);` |
|    ! 0 | 1859 | `	VmSessUnlinkIfExists(pVm,&sFile);` |
|    ! 0 | 1860 | `	SyBlobRelease(&sFile);` |
|    ! 0 | 1861 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 1862 | `	return PH7_OK;` |
|    ! 0 | 1863 | `}` |
|      2 | 1864 | `static int vm_builtin_SessionHandler_gc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1865 | `{` |
|      - | 1866 | `	/* The FILES sweep, never the dispatcher: this IS the files handler, and a` |
|      - | 1867 | `	 * program that decorates it is the user handler the dispatcher would call.` |
|      - | 1868 | `	 * php's signature takes the lifetime, so an explicit one wins over the ini. */` |
|      3 | 1869 | `	sxi64 iMaxLife = nArg > 0` |
|      2 | 1870 | `		? ph7_value_to_int64(apArg[0])` |
|      1 | 1871 | `		: PH7_VmIniGetInt(pCtx->pVm,"session.gc_maxlifetime",1440);` |
|      3 | 1872 | `	ph7_result_int64(pCtx,VmSessGcFiles(pCtx->pVm,iMaxLife));` |
|      3 | 1873 | `	return PH7_OK;` |
|      1 | 1874 | `}` |
|    ! 0 | 1875 | `static int vm_builtin_SessionHandler_create_sid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1876 | `{` |
|      - | 1877 | `	SyBlob sId;` |
|    ! 0 | 1878 | `	SXUNUSED(nArg);` |
|    ! 0 | 1879 | `	SXUNUSED(apArg);` |
|    ! 0 | 1880 | `	SyBlobInit(&sId,&pCtx->pVm->sAllocator);` |
|    ! 0 | 1881 | `	VmSessGenId(pCtx->pVm,&sId);` |
|    ! 0 | 1882 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sId),(int)SyBlobLength(&sId));` |
|    ! 0 | 1883 | `	SyBlobRelease(&sId);` |
|    ! 0 | 1884 | `	return PH7_OK;` |
|    ! 0 | 1885 | `}` |
|      - | 1886 | `/*` |
|      - | 1887 | ` * bool session_set_save_handler(SessionHandlerInterface $handler,` |
|      - | 1888 | ` *     bool $register_shutdown = true)` |
|      - | 1889 | ` * bool session_set_save_handler(callable $open, callable $close, callable $read,` |
|      - | 1890 | ` *     callable $write, callable $destroy, callable $gc,` |
|      - | 1891 | ` *     callable $create_sid = ?, callable $validate_sid = ?,` |
|      - | 1892 | ` *     callable $update_timestamp = ?)` |
|      - | 1893 | ` */` |
|     14 | 1894 | `static int vm_builtin_session_set_save_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1895 | `{` |
|     18 | 1896 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1897 | `	ph7_class *pIface;` |
|      - | 1898 | `	ph7_class_instance *pThis;` |
|     18 | 1899 | `	if( nArg < 1 ){` |
|    ! 0 | 1900 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      - | 1901 | `			"session_set_save_handler() expects at least 1 argument, 0 given");` |
|      - | 1902 | `	}` |
|      - | 1903 | `	/* The handler decides what the store IS, so php will not take one once a` |
|      - | 1904 | `	 * session is open or a header has gone out. */` |
|     18 | 1905 | `	if( VmSessLocked(pCtx,"session_set_save_handler","Session save handler",1) ){` |
|    ! 0 | 1906 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1907 | `		return PH7_OK;` |
|      - | 1908 | `	}` |
|     18 | 1909 | `	pIface = PH7_VmExtractClass(pVm,"SessionHandlerInterface",` |
|      - | 1910 | `		sizeof("SessionHandlerInterface")-1,0,0);` |
|     25 | 1911 | `	pThis = (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|     14 | 1912 | `		? (ph7_class_instance *)apArg[0]->x.pOther : 0;` |
|     18 | 1913 | `	if( pThis == 0 \|\| pIface == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pIface) ){` |
|      - | 1914 | `		/* php ALSO takes six-to-nine callables here and DEPRECATES that spelling` |
|      - | 1915 | `		 * (8.4: "Providing individual callbacks instead of an object implementing` |
|      - | 1916 | `		 * SessionHandlerInterface is deprecated"). §10 targets php's` |
|      - | 1917 | `		 * non-deprecated surface, so the callables form is refused rather than` |
|      - | 1918 | `		 * carried — with php's own message for a first argument that is not a` |
|      - | 1919 | `		 * handler, which is what each of those callables is. */` |
|      3 | 1920 | `		const char *zGiven = "";` |
|      3 | 1921 | `		if( pThis && pThis->pClass ){` |
|      3 | 1922 | `			zGiven = pThis->pClass->sName.zString;` |
|      2 | 1923 | `		}else{` |
|    ! 0 | 1924 | `			zGiven = ph7_type_name(apArg[0]);` |
|      - | 1925 | `		}` |
|      4 | 1926 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1927 | `			"session_set_save_handler(): Argument #1 ($open) must be of type"` |
|      1 | 1928 | `			" SessionHandlerInterface, %s given",zGiven);` |
|      - | 1929 | `	}` |
|     15 | 1930 | `	PH7_MemObjRelease(&pVm->sSessHandler);` |
|     15 | 1931 | `	PH7_MemObjStore(apArg[0],&pVm->sSessHandler);` |
|     15 | 1932 | `	pVm->bSessOpened = 0;` |
|     15 | 1933 | `	PH7_VmIniSet(pVm,"session.save_handler",sizeof("session.save_handler")-1,` |
|      - | 1934 | `		"user",sizeof("user")-1,"session_set_save_handler()");` |
|     15 | 1935 | `	ph7_result_bool(pCtx,1);` |
|     15 | 1936 | `	return PH7_OK;` |
|     11 | 1937 | `}` |
|      - | 1938 | `/* string\|false session_module_name(?string $module = null) */` |
|     12 | 1939 | `static int vm_builtin_session_module_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1940 | `{` |
|     14 | 1941 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1942 | `	SyBlob sOld;` |
|     14 | 1943 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|     14 | 1944 | `	PH7_VmIniGetStr(pVm,"session.save_handler",&sOld);` |
|     14 | 1945 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|      3 | 1946 | `		int nVal = 0;` |
|      3 | 1947 | `		const char *zVal = ph7_value_to_string(apArg[0],&nVal);` |
|      3 | 1948 | `		if( VmSessLocked(pCtx,"session_module_name","Session save handler module",1) ){` |
|    ! 0 | 1949 | `			SyBlobRelease(&sOld);` |
|    ! 0 | 1950 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1951 | `			return PH7_OK;` |
|      - | 1952 | `		}` |
|      2 | 1953 | `		if( !(nVal == 5 && SyMemcmp(zVal,"files",5) == 0)` |
|      2 | 1954 | `		 && !(nVal == 4 && SyMemcmp(zVal,"user",4) == 0) ){` |
|      - | 1955 | `			/* php looks the module up in its registered list; this build registers` |
|      - | 1956 | ``			 * the `files` store and the `user` one session_set_save_handler()`` |
|      - | 1957 | ``			 * installs, which is every module a CLI-plus-`-S` engine has. */`` |
|      - | 1958 | `			char zMsg[160];` |
|    ! 0 | 1959 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 1960 | `				"session_module_name(): Argument #1 ($module) must be a valid"` |
|      - | 1961 | `				" session handler");` |
|    ! 0 | 1962 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,zMsg);` |
|    ! 0 | 1963 | `			SyBlobRelease(&sOld);` |
|    ! 0 | 1964 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1965 | `			return PH7_OK;` |
|      - | 1966 | `		}` |
|      3 | 1967 | `		if( nVal == 5 ){` |
|      - | 1968 | `			/* Back to the built-in store: the userland handler is dropped. */` |
|      3 | 1969 | `			PH7_MemObjRelease(&pVm->sSessHandler);` |
|      3 | 1970 | `			pVm->bSessOpened = 0;` |
|      1 | 1971 | `		}` |
|      4 | 1972 | `		PH7_VmIniSet(pVm,"session.save_handler",sizeof("session.save_handler")-1,` |
|      1 | 1973 | `			zVal,(sxu32)nVal,"session_module_name()");` |
|      1 | 1974 | `	}` |
|     14 | 1975 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|     14 | 1976 | `	SyBlobRelease(&sOld);` |
|     14 | 1977 | `	return PH7_OK;` |
|      8 | 1978 | `}` |
|      - | 1979 | `/* int\|false session_gc() */` |
|      - | 1980 |  |
|      8 | 1981 | `static int vm_builtin_session_gc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1982 | `{` |
|     10 | 1983 | `	ph7_vm *pVm = pCtx->pVm;` |
|      4 | 1984 | `	SXUNUSED(nArg);` |
|      4 | 1985 | `	SXUNUSED(apArg);` |
|     10 | 1986 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      - | 1987 | `		/* The collector is the STORE's, and php only reaches a store through an` |
|      - | 1988 | `		 * open session. */` |
|      3 | 1989 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1990 | `			"session_gc(): Session cannot be garbage collected when there is no active session");` |
|      3 | 1991 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1992 | `		return PH7_OK;` |
|      - | 1993 | `	}` |
|      8 | 1994 | `	ph7_result_int64(pCtx,VmSessGc(pVm));` |
|      8 | 1995 | `	return PH7_OK;` |
|      6 | 1996 | `}` |
|      - | 1997 | `/*` |
|      - | 1998 | ` * void session_register_shutdown()` |
|      - | 1999 | ` *` |
|      - | 2000 | ` * php's own escape hatch for a script that installs a shutdown function which` |
|      - | 2001 | ` * calls exit(): the remaining callbacks are skipped, so php re-registers the` |
|      - | 2002 | ` * session writer as a callback of its OWN to make sure the session is still` |
|      - | 2003 | ` * written. The writer here runs from the VM's request shutdown, past every` |
|      - | 2004 | ` * callback and past a halt, so there is nothing left for this to arrange.` |
|      - | 2005 | ` */` |
|    ! 0 | 2006 | `static int vm_builtin_session_register_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2007 | `{` |
|    ! 0 | 2008 | `	SXUNUSED(nArg);` |
|    ! 0 | 2009 | `	SXUNUSED(apArg);` |
|    ! 0 | 2010 | `	ph7_result_null(pCtx);` |
|    ! 0 | 2011 | `	return PH7_OK;` |
|    ! 0 | 2012 | `}` |
|      - | 2013 | `/* string\|false session_create_id(string $prefix = "") */` |
|     14 | 2014 | `static int vm_builtin_session_create_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2015 | `{` |
|     15 | 2016 | `	ph7_vm *pVm = pCtx->pVm;` |
|     15 | 2017 | `	const char *zPfx = "";` |
|     15 | 2018 | `	int nPfx = 0;` |
|      - | 2019 | `	SyBlob sId;` |
|     15 | 2020 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     13 | 2021 | `		zPfx = ph7_value_to_string(apArg[0],&nPfx);` |
|      6 | 2022 | `	}` |
|     15 | 2023 | `	if( nPfx > VM_SESS_MAX_ID ){` |
|      3 | 2024 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2025 | `			"session_create_id(): Argument #1 ($prefix) cannot be longer than %d characters",` |
|      - | 2026 | `			VM_SESS_MAX_ID);` |
|      - | 2027 | `	}` |
|     13 | 2028 | `	if( nPfx > 0 && !VmSessIdValid(zPfx,(sxu32)nPfx) ){` |
|      - | 2029 | `		/* The prefix becomes the front of an id, so it lives under the id's own` |
|      - | 2030 | `		 * alphabet — php reports it and answers false rather than making one it` |
|      - | 2031 | `		 * would then refuse to start. */` |
|      7 | 2032 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 2033 | `			"session_create_id(): Prefix cannot contain special characters."` |
|      - | 2034 | `			" Only the A-Z, a-z, 0-9, \"-\", and \",\" characters are allowed");` |
|      7 | 2035 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2036 | `		return PH7_OK;` |
|      - | 2037 | `	}` |
|      7 | 2038 | `	SyBlobInit(&sId,&pVm->sAllocator);` |
|      7 | 2039 | `	VmSessGenId(pVm,&sId);` |
|      7 | 2040 | `	ph7_result_string(pCtx,zPfx,nPfx);` |
|      7 | 2041 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sId),(int)SyBlobLength(&sId));` |
|      7 | 2042 | `	SyBlobRelease(&sId);` |
|      7 | 2043 | `	return PH7_OK;` |
|      8 | 2044 | `}` |
|      - | 2045 | `/* string\|false session_encode() */` |
|     10 | 2046 | `static int vm_builtin_session_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2047 | `{` |
|     11 | 2048 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 2049 | `	SyBlob sData;` |
|      5 | 2050 | `	SXUNUSED(nArg);` |
|      5 | 2051 | `	SXUNUSED(apArg);` |
|     11 | 2052 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|    ! 0 | 2053 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 2054 | `			"session_encode(): Cannot encode non-existent session");` |
|    ! 0 | 2055 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2056 | `		return PH7_OK;` |
|      - | 2057 | `	}` |
|     11 | 2058 | `	SyBlobInit(&sData,&pVm->sAllocator);` |
|     11 | 2059 | `	if( VmSessEncode(pVm,&sData,"session_encode()") ){` |
|      9 | 2060 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sData),(int)SyBlobLength(&sData));` |
|      5 | 2061 | `	}else{` |
|      3 | 2062 | `		ph7_result_bool(pCtx,0);` |
|      - | 2063 | `	}` |
|     11 | 2064 | `	SyBlobRelease(&sData);` |
|     11 | 2065 | `	return PH7_OK;` |
|      6 | 2066 | `}` |
|      - | 2067 | `/* bool session_decode(string $data) */` |
|     10 | 2068 | `static int vm_builtin_session_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2069 | `{` |
|     11 | 2070 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 2071 | `	ph7_value *pSess;` |
|      - | 2072 | `	const char *zData;` |
|     11 | 2073 | `	int nData = 0,iDec;` |
|     11 | 2074 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      3 | 2075 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 2076 | `			"session_decode(): Session data cannot be decoded when there is no active session");` |
|      3 | 2077 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2078 | `		return PH7_OK;` |
|      - | 2079 | `	}` |
|      9 | 2080 | `	if( nArg < 1 ){` |
|    ! 0 | 2081 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2082 | `		return PH7_OK;` |
|      - | 2083 | `	}` |
|      9 | 2084 | `	pSess = VmSessArray(pVm);` |
|      9 | 2085 | `	if( pSess == 0 ){` |
|    ! 0 | 2086 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2087 | `		return PH7_OK;` |
|      - | 2088 | `	}` |
|      9 | 2089 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|      9 | 2090 | `	VmSessNormalizeVars(pVm,"session_decode()");` |
|      - | 2091 | `	/* Unlike session_start(), this one decodes OVER whatever $_SESSION already` |
|      - | 2092 | `	 * holds — for the two keyed handlers; php_serialize replaces the variable. */` |
|      9 | 2093 | `	iDec = VmSessDecodeInto(pCtx,zData,(sxu32)nData,pSess);` |
|      9 | 2094 | `	if( iDec < 0 ){` |
|    ! 0 | 2095 | `		return PH7_EXCEPTION;` |
|      - | 2096 | `	}` |
|      9 | 2097 | `	if( iDec == 0 ){` |
|      - | 2098 | `		SyBlob sFile;` |
|      3 | 2099 | `		SyBlobInit(&sFile,&pVm->sAllocator);` |
|      3 | 2100 | `		VmSessFile(pVm,&sFile);` |
|      3 | 2101 | `		VmSessDestroyBadStore(pVm,&sFile,"session_decode");` |
|      3 | 2102 | `		SyBlobRelease(&sFile);` |
|      3 | 2103 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2104 | `		return PH7_OK;` |
|      - | 2105 | `	}` |
|      7 | 2106 | `	ph7_result_bool(pCtx,1);` |
|      7 | 2107 | `	return PH7_OK;` |
|      6 | 2108 | `}` |
|      - | 2109 | `/*` |
|      - | 2110 | ` * php's three save-handler interfaces, and the class that implements the first` |
|      - | 2111 | `` * two over the built-in `files` store. The interfaces declare no return types:`` |
|      - | 2112 | ` * php's own stubs do not, so a handler written against them may answer whatever` |
|      - | 2113 | ` * its store answers.` |
|      - | 2114 | ` */` |
|   5146 | 2115 | `static sxi32 VmSessInstallClasses(ph7_vm *pVm)` |
|      5 | 2116 | `{` |
|      - | 2117 | `	static const PH7_NativeMethodDef aIface[] = {` |
|      - | 2118 | `		{ "open",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $path, string $name", 0, 0 },` |
|      - | 2119 | `		{ "close",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|      - | 2120 | `		{ "read",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $id", 0, 0 },` |
|      - | 2121 | `		{ "write",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $id, string $data", 0, 0 },` |
|      - | 2122 | `		{ "destroy", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $id", 0, 0 },` |
|      - | 2123 | `		{ "gc",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "int $max_lifetime", 0, 0 },` |
|      - | 2124 | `	};` |
|      - | 2125 | `	static const PH7_NativeMethodDef aIdIface[] = {` |
|      - | 2126 | `		{ "create_sid", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|      - | 2127 | `	};` |
|      - | 2128 | `	static const PH7_NativeMethodDef aStampIface[] = {` |
|      - | 2129 | `		{ "validateId",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $id", 0, 0 },` |
|      - | 2130 | `		{ "updateTimestamp", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $id, string $data", 0, 0 },` |
|      - | 2131 | `	};` |
|      - | 2132 | `	static const PH7_NativeMethodDef aHandler[] = {` |
|      - | 2133 | `		{ "open",    PH7_MOD_PUBLIC, "string $path, string $name", 0,` |
|      - | 2134 | `		  vm_builtin_SessionHandler_open },` |
|      - | 2135 | `		{ "close",   PH7_MOD_PUBLIC, "", 0, vm_builtin_SessionHandler_close },` |
|      - | 2136 | `		{ "read",    PH7_MOD_PUBLIC, "string $id", 0, vm_builtin_SessionHandler_read },` |
|      - | 2137 | `		{ "write",   PH7_MOD_PUBLIC, "string $id, string $data", 0,` |
|      - | 2138 | `		  vm_builtin_SessionHandler_write },` |
|      - | 2139 | `		{ "destroy", PH7_MOD_PUBLIC, "string $id", 0, vm_builtin_SessionHandler_destroy },` |
|      - | 2140 | `		{ "gc",      PH7_MOD_PUBLIC, "int $max_lifetime", 0, vm_builtin_SessionHandler_gc },` |
|      - | 2141 | `		{ "create_sid", PH7_MOD_PUBLIC, "", 0, vm_builtin_SessionHandler_create_sid },` |
|      - | 2142 | `	};` |
|      - | 2143 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 2144 | `		{ "SessionHandlerInterface", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 2145 | `		  aIface, SX_ARRAYSIZE(aIface), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 2146 | `		{ "SessionIdInterface", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 2147 | `		  aIdIface, SX_ARRAYSIZE(aIdIface), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 2148 | `		{ "SessionUpdateTimestampHandlerInterface", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 2149 | `		  aStampIface, SX_ARRAYSIZE(aStampIface), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 2150 | `		{ "SessionHandler", 0, "SessionHandlerInterface,SessionIdInterface", 0,` |
|      - | 2151 | `		  aHandler, SX_ARRAYSIZE(aHandler), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 2152 | `	};` |
|   5151 | 2153 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|      5 | 2154 | `}` |
|   5146 | 2155 | `PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm)` |
|      5 | 2156 | `{` |
|      - | 2157 | `	static const struct {` |
|      - | 2158 | `		const char *zName;` |
|      - | 2159 | `		ProchHostFunction xFunc;` |
|      - | 2160 | `	} aFunc[] = {` |
|      - | 2161 | `		{ "session_status",        vm_builtin_session_status        },` |
|      - | 2162 | `		{ "session_id",            vm_builtin_session_id            },` |
|      - | 2163 | `		{ "session_name",          vm_builtin_session_name          },` |
|      - | 2164 | `		{ "session_save_path",     vm_builtin_session_save_path     },` |
|      - | 2165 | `		{ "session_start",         vm_builtin_session_start         },` |
|      - | 2166 | `		{ "session_write_close",   vm_builtin_session_write_close   },` |
|      - | 2167 | `		{ "session_commit",        vm_builtin_session_write_close   },` |
|      - | 2168 | `		{ "session_abort",         vm_builtin_session_abort         },` |
|      - | 2169 | `		{ "session_reset",         vm_builtin_session_reset         },` |
|      - | 2170 | `		{ "session_unset",         vm_builtin_session_unset         },` |
|      - | 2171 | `		{ "session_destroy",       vm_builtin_session_destroy       },` |
|      - | 2172 | `		{ "session_regenerate_id", vm_builtin_session_regenerate_id },` |
|      - | 2173 | `		{ "session_encode",        vm_builtin_session_encode        },` |
|      - | 2174 | `		{ "session_decode",        vm_builtin_session_decode        },` |
|      - | 2175 | `		{ "session_create_id",     vm_builtin_session_create_id     },` |
|      - | 2176 | `		{ "session_gc",            vm_builtin_session_gc            },` |
|      - | 2177 | `		{ "session_module_name",   vm_builtin_session_module_name   },` |
|      - | 2178 | `		{ "session_set_save_handler", vm_builtin_session_set_save_handler },` |
|      - | 2179 | `		{ "session_cache_limiter", vm_builtin_session_cache_limiter },` |
|      - | 2180 | `		{ "session_cache_expire",  vm_builtin_session_cache_expire  },` |
|      - | 2181 | `		{ "session_register_shutdown", vm_builtin_session_register_shutdown },` |
|      - | 2182 | `		{ "session_get_cookie_params", vm_builtin_session_get_cookie_params },` |
|      - | 2183 | `		{ "session_set_cookie_params", vm_builtin_session_set_cookie_params },` |
|      - | 2184 | `	};` |
|      - | 2185 | `	sxu32 n;` |
| 123509 | 2186 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 118363 | 2187 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  59184 | 2188 | `	}` |
|   5151 | 2189 | `	return VmSessInstallClasses(pVm);` |
|      5 | 2190 | `}` |
|      - | 2191 |  |
|      - | 2192 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 2193 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2194 |  |
|      - | 2195 | `#if defined(PH7_DISABLE_BUILTIN_FUNC) \|\| defined(PH7_DISABLE_DISK_IO)` |
|      - | 2196 | `/* Tiny build: no sessions (builtin funcs / disk IO disabled) */` |
|      - | 2197 | `PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|      - | 2198 | `PH7_PRIVATE void PH7_VmSessionShutdown(ph7_vm *pVm){ (void)pVm; }` |
|      - | 2199 | `#endif` |
|      - | 2200 |  |
