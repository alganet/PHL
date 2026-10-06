# src/ph7/vm_builtin_session.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1227/1369 lines (89.63%)

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
|    611 |   38 | `static sxi32 VmSessCall(ph7_vm *pVm,const char *zFunc,int nArg,ph7_value **apArg,ph7_value *pResult)` |
|      4 |   39 | `{` |
|      - |   40 | `	ph7_value sName;` |
|      - |   41 | `	SyString sStr;` |
|      - |   42 | `	sxi32 rc;` |
|    615 |   43 | `	PH7_MemObjInit(pVm,&sName);` |
|    615 |   44 | `	SyStringInitFromBuf(&sStr,zFunc,SyStrlen(zFunc));` |
|    615 |   45 | `	PH7_MemObjInitFromString(pVm,&sName,&sStr);` |
|    615 |   46 | `	rc = PH7_VmCallUserFunction(&(*pVm),&sName,nArg,apArg,pResult);` |
|    615 |   47 | `	PH7_MemObjRelease(&sName);` |
|    615 |   48 | `	return rc;` |
|      4 |   49 | `}` |
|    787 |   50 | `static void VmSessStrArg(ph7_vm *pVm,ph7_value *pOut,const char *zVal,sxu32 nVal)` |
|      4 |   51 | `{` |
|      - |   52 | `	SyString sStr;` |
|    791 |   53 | `	PH7_MemObjInit(pVm,pOut);` |
|    791 |   54 | `	SyStringInitFromBuf(&sStr,zVal,nVal);` |
|    791 |   55 | `	PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|    791 |   56 | `}` |
|      - |   57 | `/*` |
|      - |   58 | ` * The save path, resolving the lazy default the way the chunk did: an unset path` |
|      - |   59 | ` * becomes sys_get_temp_dir() with any trailing slash removed, and is remembered.` |
|      - |   60 | ` */` |
|    270 |   61 | `static void VmSessResolvePath(ph7_vm *pVm)` |
|      4 |   62 | `{` |
|      - |   63 | `	ph7_value sRes;` |
|    274 |   64 | `	if( SyBlobLength(&pVm->sSessPath) > 0 ){` |
|    248 |   65 | `		return;` |
|      - |   66 | `	}` |
|     30 |   67 | `	PH7_MemObjInit(pVm,&sRes);` |
|     30 |   68 | `	if( VmSessCall(pVm,"sys_get_temp_dir",0,0,&sRes) == SXRET_OK ){` |
|     30 |   69 | `		const char *zTmp = (const char *)SyBlobData(&sRes.sBlob);` |
|     30 |   70 | `		sxu32 nTmp = SyBlobLength(&sRes.sBlob);` |
|     30 |   71 | `		while( nTmp > 0 && zTmp[nTmp-1] == '/' ){ nTmp--; }` |
|     30 |   72 | `		SyBlobReset(&pVm->sSessPath);` |
|     30 |   73 | `		SyBlobAppend(&pVm->sSessPath,zTmp,nTmp);` |
|     13 |   74 | `	}` |
|     30 |   75 | `	PH7_MemObjRelease(&sRes);` |
|    139 |   76 | `}` |
|      - |   77 | `/* php joins the save path and the file name with PHP_DIR_SEPARATOR, which a` |
|      - |   78 | ` * warning naming the file shows: a backslash on Windows. */` |
|      - |   79 | `#ifdef __WINNT__` |
|      - |   80 | `#define VM_SESS_FILE_PREFIX "\\sess_"` |
|      - |   81 | `#else` |
|      - |   82 | `#define VM_SESS_FILE_PREFIX "/sess_"` |
|      - |   83 | `#endif` |
|      - |   84 | `/* "<save_path>/sess_<id>" */` |
|    202 |   85 | `static void VmSessFile(ph7_vm *pVm,SyBlob *pOut)` |
|      4 |   86 | `{` |
|    206 |   87 | `	VmSessResolvePath(pVm);` |
|    206 |   88 | `	SyBlobReset(pOut);` |
|    206 |   89 | `	SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|    206 |   90 | `	SyBlobAppend(pOut,VM_SESS_FILE_PREFIX,sizeof(VM_SESS_FILE_PREFIX)-1);` |
|    206 |   91 | `	SyBlobAppend(pOut,SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId));` |
|    206 |   92 | `}` |
|      - |   93 | `/* php's PS_MAX_SID_LENGTH: the longest id it will read or make. */` |
|      - |   94 | `#define VM_SESS_MAX_ID 256` |
|      - |   95 |  |
|      - |   96 | `/*` |
|      - |   97 | ` * A fresh id: 32 characters over php's 4-bits-per-character alphabet, which is` |
|      - |   98 | ` * lowercase hex. The two directives that would widen either number,` |
|      - |   99 | ` * session.sid_length and session.sid_bits_per_character, are DEPRECATED in php 8.4` |
|      - |  100 | ` * — the scope policy does not carry php's deprecated surface, so php's defaults are the only` |
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
|    100 |  119 | `static int VmSessIdValid(const char *z,sxu32 n)` |
|      4 |  120 | `{` |
|      - |  121 | `	sxu32 i;` |
|    104 |  122 | `	if( n < 1 \|\| n > VM_SESS_MAX_ID ){` |
|    ! 0 |  123 | `		return 0;` |
|      - |  124 | `	}` |
|   1309 |  125 | `	for( i = 0 ; i < n ; i++ ){` |
|   1219 |  126 | `		char c = z[i];` |
|   1233 |  127 | `		if( !((c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z')` |
|    333 |  128 | `		   \|\| (c >= 'A' && c <= 'Z') \|\| c == ',' \|\| c == '-') ){` |
|     11 |  129 | `			return 0;` |
|      - |  130 | `		}` |
|    616 |  131 | `	}` |
|     94 |  132 | `	return 1;` |
|     54 |  133 | `}` |
|      - |  134 | `/*` |
|      - |  135 | ` * The bytes php drops an id for WITHOUT a word, before it ever validates it: the` |
|      - |  136 | ` * ones that would break the Set-Cookie header or a log line it lands in. An id` |
|      - |  137 | ` * carrying one is thrown away and a fresh one made, where any other invalid` |
|      - |  138 | ` * character is reported and refuses the start outright.` |
|      - |  139 | ` */` |
|     96 |  140 | `static int VmSessIdDangerous(const char *z,sxu32 n)` |
|      4 |  141 | `{` |
|      - |  142 | `	sxu32 i;` |
|   1289 |  143 | `	for( i = 0 ; i < n ; i++ ){` |
|   1197 |  144 | `		if( SyByteFind("\r\n\t <>'\"\\",sizeof("\r\n\t <>'\"\\")-1,z[i],0) == SXRET_OK ){` |
|      5 |  145 | `			return 1;` |
|      - |  146 | `		}` |
|    608 |  147 | `	}` |
|     96 |  148 | `	return 0;` |
|     52 |  149 | `}` |
|      - |  150 | `/*` |
|      - |  151 | ` * Read the session file into pOut, answering FALSE when there is none.` |
|      - |  152 | ` *` |
|      - |  153 | ` * The existence check is not an optimization: file_get_contents() warns on a` |
|      - |  154 | ` * missing path, and a first-ever session_start() has no file yet -- the PHP chunk` |
|      - |  155 | ` * guarded the read with file_exists() for exactly this reason.` |
|      - |  156 | ` */` |
|     76 |  157 | `static int VmSessReadFile(ph7_vm *pVm,SyBlob *pFile,ph7_value *pOut)` |
|      4 |  158 | `{` |
|      - |  159 | `	ph7_value sPath,sExists;` |
|      - |  160 | `	ph7_value *apA[1];` |
|      - |  161 | `	int bOk;` |
|     80 |  162 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));` |
|     80 |  163 | `	PH7_MemObjInit(pVm,&sExists);` |
|     80 |  164 | `	apA[0] = &sPath;` |
|     80 |  165 | `	VmSessCall(pVm,"file_exists",1,apA,&sExists);` |
|      - |  166 | `	/* PH7_MemObjToBool converts IN PLACE and answers a STATUS, not the boolean --` |
|      - |  167 | `	 * the value lands in x.iVal. Reading its return as the answer makes every` |
|      - |  168 | `	 * existence check false, which silently empties the session on every reload. */` |
|     80 |  169 | `	PH7_MemObjToBool(&sExists);` |
|     80 |  170 | `	bOk = sExists.x.iVal != 0;` |
|     80 |  171 | `	PH7_MemObjRelease(&sExists);` |
|     80 |  172 | `	if( bOk ){` |
|     76 |  173 | `		VmSessCall(pVm,"file_get_contents",1,apA,pOut);` |
|     76 |  174 | `		bOk = (pOut->iFlags & MEMOBJ_STRING) != 0;` |
|     36 |  175 | `	}` |
|     80 |  176 | `	PH7_MemObjRelease(&sPath);` |
|     80 |  177 | `	return bOk;` |
|      4 |  178 | `}` |
|      - |  179 | `/*` |
|      - |  180 | ` * Delete the session file if it is there. Same reason the read is guarded:` |
|      - |  181 | ` * unlink() warns on a missing path, and destroying a session that was never` |
|      - |  182 | ` * written (or regenerating an id before the first write) is normal.` |
|      - |  183 | ` */` |
|      6 |  184 | `static void VmSessUnlinkIfExists(ph7_vm *pVm,SyBlob *pFile)` |
|      2 |  185 | `{` |
|      - |  186 | `	ph7_value sPath,sExists;` |
|      - |  187 | `	ph7_value *apA[1];` |
|      8 |  188 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));` |
|      8 |  189 | `	PH7_MemObjInit(pVm,&sExists);` |
|      8 |  190 | `	apA[0] = &sPath;` |
|      8 |  191 | `	VmSessCall(pVm,"file_exists",1,apA,&sExists);` |
|      8 |  192 | `	PH7_MemObjToBool(&sExists);` |
|      8 |  193 | `	if( sExists.x.iVal ){` |
|      8 |  194 | `		VmSessCall(pVm,"unlink",1,apA,0);` |
|      3 |  195 | `	}` |
|      8 |  196 | `	PH7_MemObjRelease(&sExists);` |
|      8 |  197 | `	PH7_MemObjRelease(&sPath);` |
|      8 |  198 | `}` |
|      - |  199 | `/* $_SESSION, or NULL when the superglobal is somehow absent. */` |
|    212 |  200 | `static ph7_value * VmSessArray(ph7_vm *pVm)` |
|      4 |  201 | `{` |
|    216 |  202 | `	return PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
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
|    344 |  225 | `static int VmSessSerializerOrErr(ph7_vm *pVm)` |
|      4 |  226 | `{` |
|      - |  227 | `	SyBlob sVal;` |
|      - |  228 | `	const char *zVal;` |
|      - |  229 | `	sxu32 nVal;` |
|    348 |  230 | `	int iRet = -1;` |
|    348 |  231 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|    348 |  232 | `	PH7_VmIniGetStr(pVm,"session.serialize_handler",&sVal);` |
|    348 |  233 | `	zVal = (const char *)SyBlobData(&sVal);` |
|    348 |  234 | `	nVal = SyBlobLength(&sVal);` |
|    348 |  235 | `	if( nVal == sizeof("php")-1 && SyMemcmp(zVal,"php",nVal) == 0 ){` |
|    316 |  236 | `		iRet = VM_SESS_SER_PHP;` |
|    190 |  237 | `	}else if( nVal == sizeof("php_binary")-1 && SyMemcmp(zVal,"php_binary",nVal) == 0 ){` |
|     15 |  238 | `		iRet = VM_SESS_SER_BINARY;` |
|     27 |  239 | `	}else if( nVal == sizeof("php_serialize")-1 && SyMemcmp(zVal,"php_serialize",nVal) == 0 ){` |
|     15 |  240 | `		iRet = VM_SESS_SER_SERIALIZE;` |
|      7 |  241 | `	}` |
|    348 |  242 | `	SyBlobRelease(&sVal);` |
|    348 |  243 | `	return iRet;` |
|      4 |  244 | `}` |
|    194 |  245 | `static int VmSessSerializer(ph7_vm *pVm)` |
|      4 |  246 | `{` |
|    198 |  247 | `	int iRet = VmSessSerializerOrErr(pVm);` |
|    198 |  248 | `	return iRet < 0 ? VM_SESS_SER_PHP : iRet;` |
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
|     98 |  268 | `static int VmSessEncode(ph7_vm *pVm,SyBlob *pOut,const char *zWho)` |
|      4 |  269 | `{` |
|    102 |  270 | `	ph7_value *pSess = VmSessArray(pVm);` |
|      - |  271 | `	ph7_hashmap *pMap;` |
|      - |  272 | `	ph7_hashmap_node *pNode;` |
|    102 |  273 | `	int iSer = VmSessSerializer(pVm);` |
|      - |  274 | `	sxu32 n;` |
|    102 |  275 | `	SyBlobReset(pOut);` |
|    102 |  276 | `	if( pSess == 0 \|\| (pSess->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  277 | `		return 1;` |
|      - |  278 | `	}` |
|    102 |  279 | `	if( iSer == VM_SESS_SER_SERIALIZE ){` |
|      - |  280 | `		/* One serialize() of the array itself, so a numeric key and a key holding` |
|      - |  281 | `		 * a '\|' both simply round-trip. */` |
|      7 |  282 | `		VmSessSerializeValue(pVm,pSess,pOut);` |
|      7 |  283 | `		return 1;` |
|      - |  284 | `	}` |
|     96 |  285 | `	pMap = (ph7_hashmap *)pSess->x.pOther;` |
|     96 |  286 | `	pNode = pMap->pFirst;` |
|    188 |  287 | `	for( n = 0 ; n < pMap->nEntry && pNode ; n++, pNode = pNode->pPrev ){` |
|      - |  288 | `		ph7_value *pVal;` |
|      - |  289 | `		const char *zKey;` |
|      - |  290 | `		sxu32 nKey;` |
|      - |  291 | `		char zMsg[256];` |
|    100 |  292 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|      - |  293 | `			/* Both formats key by NAME; an integer key has no spelling in either. */` |
|     11 |  294 | `			SyBufferFormat(zMsg,sizeof(zMsg),"%s: Skipping numeric key %qd",zWho,pNode->xKey.iKey);` |
|     11 |  295 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|     11 |  296 | `			continue;` |
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
|     86 |  314 | `		pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
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
|     92 |  328 | `	return 1;` |
|     53 |  329 | `}` |
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
|    100 |  370 | `static void VmSessEmptyArray(ph7_vm *pVm,ph7_value *pDest)` |
|      4 |  371 | `{` |
|    104 |  372 | `	PH7_MemObjRelease(pDest);` |
|    104 |  373 | `	pDest->x.pOther = PH7_NewHashmap(pVm,0,0);` |
|    104 |  374 | `	if( pDest->x.pOther ){` |
|    104 |  375 | `		MemObjSetType(pDest,MEMOBJ_HASHMAP);` |
|     50 |  376 | `	}` |
|    104 |  377 | `}` |
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
|     88 |  388 | `static int VmSessDecodeInto(ph7_context *pCtx,const char *zSrc,sxu32 nLen,ph7_value *pDest)` |
|      4 |  389 | `{` |
|     92 |  390 | `	ph7_vm *pVm = pCtx->pVm;` |
|     92 |  391 | `	int iSer = VmSessSerializer(pVm);` |
|     92 |  392 | `	sxu32 nPos = 0;` |
|     92 |  393 | `	if( iSer == VM_SESS_SER_SERIALIZE ){` |
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
|     88 |  417 | `	if( (pDest->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  418 | `		VmSessEmptyArray(pVm,pDest);` |
|    ! 0 |  419 | `		if( (pDest->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  420 | `			return 0;` |
|      - |  421 | `		}` |
|    ! 0 |  422 | `	}` |
|    120 |  423 | `	while( nPos < nLen ){` |
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
|     86 |  462 | `	return 1;` |
|     48 |  463 | `}` |
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
|    426 |  491 | `static int VmSessHasUser(ph7_vm *pVm)` |
|      4 |  492 | `{` |
|    430 |  493 | `	return (pVm->sSessHandler.iFlags & MEMOBJ_OBJ) != 0;` |
|      4 |  494 | `}` |
|      - |  495 | `/*` |
|      - |  496 | ` * Call one store operation. Answers 0 when the handler does not offer it (php` |
|      - |  497 | ` * treats the three optional ones as absent then), 1 when it ran, -1 on a throw.` |
|      - |  498 | ` */` |
|    104 |  499 | `static int VmSessUserCall(ph7_vm *pVm,int iOp,int nArg,ph7_value **apArg,ph7_value *pResult)` |
|      4 |  500 | `{` |
|      - |  501 | `	ph7_value sCallable;` |
|      - |  502 | `	sxi32 rc;` |
|    108 |  503 | `	int iRet = 1;` |
|    108 |  504 | `	ph7_class_instance *pThis = (ph7_class_instance *)pVm->sSessHandler.x.pOther;` |
|      - |  505 | `	ph7_value sName;` |
|    108 |  506 | `	PH7_MemObjInit(pVm,&sCallable);` |
|    104 |  507 | `	if( (pVm->sSessHandler.iFlags & MEMOBJ_OBJ) == 0 \|\| pThis == 0` |
|    108 |  508 | `	 \|\| PH7_ClassExtractMethod(pThis->pClass,azSessOp[iOp],` |
|    156 |  509 | `		(sxu32)SyStrlen(azSessOp[iOp])) == 0 ){` |
|      - |  510 | `		/* The three optional operations are simply absent when the handler does` |
|      - |  511 | `		 * not implement their interface. */` |
|    ! 0 |  512 | `		PH7_MemObjRelease(&sCallable);` |
|    ! 0 |  513 | `		return 0;` |
|      - |  514 | `	}` |
|    108 |  515 | `	sCallable.x.pOther = PH7_NewHashmap(pVm,0,0);` |
|    108 |  516 | `	if( sCallable.x.pOther == 0 ){` |
|    ! 0 |  517 | `		PH7_MemObjRelease(&sCallable);` |
|    ! 0 |  518 | `		return 0;` |
|      - |  519 | `	}` |
|    108 |  520 | `	MemObjSetType(&sCallable,MEMOBJ_HASHMAP);` |
|    108 |  521 | `	PH7_HashmapInsert((ph7_hashmap *)sCallable.x.pOther,0,&pVm->sSessHandler);` |
|    108 |  522 | `	VmSessStrArg(pVm,&sName,azSessOp[iOp],(sxu32)SyStrlen(azSessOp[iOp]));` |
|    108 |  523 | `	PH7_HashmapInsert((ph7_hashmap *)sCallable.x.pOther,0,&sName);` |
|    108 |  524 | `	PH7_MemObjRelease(&sName);` |
|    108 |  525 | `	rc = PH7_VmCallUserFunction(pVm,&sCallable,nArg,apArg,pResult);` |
|    108 |  526 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 |  527 | `		iRet = -1;` |
|    108 |  528 | `	}else if( rc != SXRET_OK ){` |
|    ! 0 |  529 | `		iRet = 0;` |
|    ! 0 |  530 | `	}` |
|    108 |  531 | `	PH7_MemObjRelease(&sCallable);` |
|    108 |  532 | `	return iRet;` |
|     56 |  533 | `}` |
|      - |  534 | `/* open($path, $name) — php calls it once, before the first read. */` |
|     28 |  535 | `static void VmSessUserOpen(ph7_vm *pVm)` |
|      4 |  536 | `{` |
|      - |  537 | `	ph7_value sPath,sName,sRes;` |
|      - |  538 | `	ph7_value *apA[2];` |
|     32 |  539 | `	if( pVm->bSessOpened \|\| !VmSessHasUser(pVm) ){` |
|      5 |  540 | `		return;` |
|      - |  541 | `	}` |
|     28 |  542 | `	pVm->bSessOpened = 1;` |
|     28 |  543 | `	VmSessResolvePath(pVm);` |
|     40 |  544 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(&pVm->sSessPath),` |
|     12 |  545 | `		SyBlobLength(&pVm->sSessPath));` |
|     40 |  546 | `	VmSessStrArg(pVm,&sName,(const char *)SyBlobData(&pVm->sSessName),` |
|     12 |  547 | `		SyBlobLength(&pVm->sSessName));` |
|     28 |  548 | `	PH7_MemObjInit(pVm,&sRes);` |
|     28 |  549 | `	apA[0] = &sPath;` |
|     28 |  550 | `	apA[1] = &sName;` |
|     28 |  551 | `	VmSessUserCall(pVm,VM_SESS_OP_OPEN,2,apA,&sRes);` |
|     28 |  552 | `	PH7_MemObjRelease(&sRes);` |
|     28 |  553 | `	PH7_MemObjRelease(&sName);` |
|     28 |  554 | `	PH7_MemObjRelease(&sPath);` |
|     18 |  555 | `}` |
|      - |  556 | `/* Release the store: a userland handler's close(), once per open(). */` |
|     92 |  557 | `static void VmSessCloseStore(ph7_vm *pVm)` |
|      4 |  558 | `{` |
|      - |  559 | `	ph7_value sRes;` |
|     96 |  560 | `	if( !VmSessHasUser(pVm) \|\| !pVm->bSessOpened ){` |
|     72 |  561 | `		return;` |
|      - |  562 | `	}` |
|     28 |  563 | `	pVm->bSessOpened = 0;` |
|     28 |  564 | `	PH7_MemObjInit(pVm,&sRes);` |
|     28 |  565 | `	VmSessUserCall(pVm,VM_SESS_OP_CLOSE,0,0,&sRes);` |
|     28 |  566 | `	PH7_MemObjRelease(&sRes);` |
|     50 |  567 | `}` |
|      - |  568 | `/* One id-shaped call: read($id) / destroy($id). */` |
|     28 |  569 | `static int VmSessUserId(ph7_vm *pVm,int iOp,ph7_value *pResult)` |
|      4 |  570 | `{` |
|      - |  571 | `	ph7_value sId;` |
|      - |  572 | `	ph7_value *apA[1];` |
|      - |  573 | `	int iRet;` |
|     46 |  574 | `	VmSessStrArg(pVm,&sId,(const char *)SyBlobData(&pVm->sSessId),` |
|     14 |  575 | `		SyBlobLength(&pVm->sSessId));` |
|     32 |  576 | `	apA[0] = &sId;` |
|     32 |  577 | `	iRet = VmSessUserCall(pVm,iOp,1,apA,pResult);` |
|     32 |  578 | `	PH7_MemObjRelease(&sId);` |
|     32 |  579 | `	return iRet;` |
|      4 |  580 | `}` |
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
|    237 |  606 | `static sxi32 VmSessCallQuiet(ph7_vm *pVm,const char *zFunc,int nArg,ph7_value **apArg,` |
|      - |  607 | `	ph7_value *pResult)` |
|      4 |  608 | `{` |
|      - |  609 | `	sxi32 rc;` |
|    241 |  610 | `	pVm->nErrSuppress++;` |
|    241 |  611 | `	rc = VmSessCall(pVm,zFunc,nArg,apArg,pResult);` |
|    241 |  612 | `	if( pVm->nErrSuppress > 0 ){` |
|    241 |  613 | `		pVm->nErrSuppress--;` |
|    120 |  614 | `	}` |
|    241 |  615 | `	return rc;` |
|      4 |  616 | `}` |
|      - |  617 | `/* TRUE when calling zFunc(zPath) answers true. */` |
|    120 |  618 | `static int VmSessPathIs(ph7_vm *pVm,const char *zFunc,SyBlob *pPath)` |
|      4 |  619 | `{` |
|      - |  620 | `	ph7_value sPath,sRes;` |
|      - |  621 | `	ph7_value *apA[1];` |
|      - |  622 | `	int bOk;` |
|    124 |  623 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pPath),SyBlobLength(pPath));` |
|    124 |  624 | `	PH7_MemObjInit(pVm,&sRes);` |
|    124 |  625 | `	apA[0] = &sPath;` |
|    124 |  626 | `	VmSessCallQuiet(pVm,zFunc,1,apA,&sRes);` |
|    124 |  627 | `	PH7_MemObjToBool(&sRes);` |
|    124 |  628 | `	bOk = sRes.x.iVal != 0;` |
|    124 |  629 | `	PH7_MemObjRelease(&sRes);` |
|    124 |  630 | `	PH7_MemObjRelease(&sPath);` |
|    124 |  631 | `	return bOk;` |
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
|     74 |  645 | `static int VmSessOpenStore(ph7_vm *pVm,SyBlob *pFile,const char *zFunc)` |
|      4 |  646 | `{` |
|      - |  647 | `	char zMsg[512];` |
|      - |  648 | `	int iErr;` |
|      - |  649 | `	const char *zWhy;` |
|     78 |  650 | `	if( VmSessPathIs(pVm,"file_exists",pFile) ){` |
|     33 |  651 | `		return 1;` |
|      - |  652 | `	}` |
|     48 |  653 | `	VmSessPutFileQuiet(pVm,pFile,"",0,1);` |
|     48 |  654 | `	if( VmSessPathIs(pVm,"file_exists",pFile) ){` |
|      - |  655 | `		/* php's store is readable by its own user and nobody else. */` |
|      - |  656 | `		ph7_value sPath,sMode;` |
|      - |  657 | `		ph7_value *apA[2];` |
|     46 |  658 | `		VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));` |
|     46 |  659 | `		PH7_MemObjInit(pVm,&sMode);` |
|     46 |  660 | `		PH7_MemObjInitFromInt(pVm,&sMode,0600);` |
|     46 |  661 | `		apA[0] = &sPath;` |
|     46 |  662 | `		apA[1] = &sMode;` |
|     46 |  663 | `		VmSessCallQuiet(pVm,"chmod",2,apA,0);` |
|     46 |  664 | `		PH7_MemObjRelease(&sMode);` |
|     46 |  665 | `		PH7_MemObjRelease(&sPath);` |
|     46 |  666 | `		return 1;` |
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
|     41 |  683 | `}` |
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
|      6 |  700 | `static sxi64 VmSessGcFiles(ph7_vm *pVm,sxi64 iMaxLife)` |
|      2 |  701 | `{` |
|      - |  702 | `	ph7_value sDir,sList;` |
|      - |  703 | `	ph7_value *apA[1];` |
|      - |  704 | `	ph7_hashmap *pMap;` |
|      - |  705 | `	ph7_hashmap_node *pNode;` |
|      8 |  706 | `	sxi64 iCut = (sxi64)time(0) - iMaxLife;` |
|      8 |  707 | `	sxi64 nGone = 0;` |
|      - |  708 | `	sxu32 n;` |
|      8 |  709 | `	VmSessResolvePath(pVm);` |
|     11 |  710 | `	VmSessStrArg(pVm,&sDir,(const char *)SyBlobData(&pVm->sSessPath),` |
|      3 |  711 | `		SyBlobLength(&pVm->sSessPath));` |
|      8 |  712 | `	PH7_MemObjInit(pVm,&sList);` |
|      8 |  713 | `	apA[0] = &sDir;` |
|      8 |  714 | `	VmSessCallQuiet(pVm,"scandir",1,apA,&sList);` |
|      8 |  715 | `	PH7_MemObjRelease(&sDir);` |
|      8 |  716 | `	if( (sList.iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  717 | `		PH7_MemObjRelease(&sList);` |
|    ! 0 |  718 | `		return 0;` |
|      - |  719 | `	}` |
|      8 |  720 | `	pMap = (ph7_hashmap *)sList.x.pOther;` |
|      8 |  721 | `	pNode = pMap->pFirst;` |
|     35 |  722 | `	for( n = 0 ; n < pMap->nEntry && pNode ; n++, pNode = pNode->pPrev ){` |
|     29 |  723 | `		ph7_value *pName = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|      - |  724 | `		SyBlob sPath;` |
|      - |  725 | `		ph7_value sArg,sTime;` |
|      - |  726 | `		ph7_value *apB[1];` |
|      - |  727 | `		const char *zName;` |
|      - |  728 | `		sxu32 nName;` |
|     29 |  729 | `		if( pName == 0 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  730 | `			continue;` |
|      - |  731 | `		}` |
|     29 |  732 | `		zName = (const char *)SyBlobData(&pName->sBlob);` |
|     29 |  733 | `		nName = SyBlobLength(&pName->sBlob);` |
|     29 |  734 | `		if( nName <= sizeof("sess_")-1 \|\| SyMemcmp(zName,"sess_",sizeof("sess_")-1) != 0 ){` |
|     16 |  735 | `			continue;` |
|      - |  736 | `		}` |
|     15 |  737 | `		SyBlobInit(&sPath,&pVm->sAllocator);` |
|     15 |  738 | `		SyBlobAppend(&sPath,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|     15 |  739 | `		SyBlobAppend(&sPath,"/",1);` |
|     15 |  740 | `		SyBlobAppend(&sPath,zName,nName);` |
|     15 |  741 | `		VmSessStrArg(pVm,&sArg,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|     15 |  742 | `		PH7_MemObjInit(pVm,&sTime);` |
|     15 |  743 | `		apB[0] = &sArg;` |
|     15 |  744 | `		VmSessCallQuiet(pVm,"filemtime",1,apB,&sTime);` |
|     15 |  745 | `		if( (sTime.iFlags & MEMOBJ_INT) && sTime.x.iVal < iCut ){` |
|      8 |  746 | `			VmSessCallQuiet(pVm,"unlink",1,apB,0);` |
|      8 |  747 | `			nGone++;` |
|      3 |  748 | `		}` |
|     15 |  749 | `		PH7_MemObjRelease(&sTime);` |
|     15 |  750 | `		PH7_MemObjRelease(&sArg);` |
|     15 |  751 | `		SyBlobRelease(&sPath);` |
|     10 |  752 | `	}` |
|      8 |  753 | `	PH7_MemObjRelease(&sList);` |
|      8 |  754 | `	return nGone;` |
|      5 |  755 | `}` |
|      - |  756 | `/*` |
|      - |  757 | ` * Run the collector the SESSION is configured with: the user handler's gc() when one` |
|      - |  758 | ` * is installed, the files sweep otherwise. session_gc() and the probabilistic sweep` |
|      - |  759 | ` * come through here.` |
|      - |  760 | ` */` |
|     10 |  761 | `static sxi64 VmSessGc(ph7_vm *pVm)` |
|      3 |  762 | `{` |
|     13 |  763 | `	sxi64 iMaxLife = PH7_VmIniGetInt(pVm,"session.gc_maxlifetime",1440);` |
|     13 |  764 | `	if( VmSessHasUser(pVm) ){` |
|      - |  765 | `		ph7_value sLife,sRes;` |
|      - |  766 | `		ph7_value *apA[1];` |
|      9 |  767 | `		sxi64 nGone = 0;` |
|      9 |  768 | `		PH7_MemObjInit(pVm,&sLife);` |
|      9 |  769 | `		PH7_MemObjInitFromInt(pVm,&sLife,iMaxLife);` |
|      9 |  770 | `		PH7_MemObjInit(pVm,&sRes);` |
|      9 |  771 | `		apA[0] = &sLife;` |
|      9 |  772 | `		if( VmSessUserCall(pVm,VM_SESS_OP_GC,1,apA,&sRes) > 0 ){` |
|      9 |  773 | `			PH7_MemObjToInteger(&sRes);` |
|      9 |  774 | `			nGone = sRes.x.iVal;` |
|      3 |  775 | `		}` |
|      9 |  776 | `		PH7_MemObjRelease(&sRes);` |
|      9 |  777 | `		PH7_MemObjRelease(&sLife);` |
|      9 |  778 | `		return nGone;` |
|      - |  779 | `	}` |
|      5 |  780 | `	return VmSessGcFiles(pVm,iMaxLife);` |
|      8 |  781 | `}` |
|      - |  782 | `/*` |
|      - |  783 | ` * php runs the collector on a session_start() with probability` |
|      - |  784 | ` * gc_probability/gc_divisor: one request in a hundred pays for everybody by` |
|      - |  785 | ` * default, and gc_probability of 0 turns it off.` |
|      - |  786 | ` */` |
|     96 |  787 | `static void VmSessMaybeGc(ph7_vm *pVm)` |
|      4 |  788 | `{` |
|    100 |  789 | `	sxi64 iProb = PH7_VmIniGetInt(pVm,"session.gc_probability",1);` |
|    100 |  790 | `	sxi64 iDiv = PH7_VmIniGetInt(pVm,"session.gc_divisor",100);` |
|    100 |  791 | `	sxu32 nRand = 0;` |
|    100 |  792 | `	if( iProb <= 0 \|\| iDiv <= 0 ){` |
|     27 |  793 | `		return;` |
|      - |  794 | `	}` |
|     76 |  795 | `	SyRandomness(&pVm->sPrng,&nRand,sizeof(nRand));` |
|     76 |  796 | `	if( (sxi64)((double)iDiv * ((double)nRand / 4294967296.0)) < iProb ){` |
|      5 |  797 | `		VmSessGc(pVm);` |
|      2 |  798 | `	}` |
|     52 |  799 | `}` |
|      - |  800 | `/*` |
|      - |  801 | ` * Load the stored copy into $_SESSION, which php always starts EMPTY here — only` |
|      - |  802 | ` * session_decode() overlays what is already there. Answers 1 when the session is` |
|      - |  803 | ` * loaded, 0 when the store was destroyed instead, and -1 for a pending exception.` |
|      - |  804 | ` */` |
|     96 |  805 | `static int VmSessLoad(ph7_context *pCtx,SyBlob *pFile,const char *zFunc)` |
|      4 |  806 | `{` |
|    100 |  807 | `	ph7_vm *pVm = pCtx->pVm;` |
|    100 |  808 | `	ph7_value *pSess = VmSessArray(pVm);` |
|      - |  809 | `	ph7_value sRes;` |
|    100 |  810 | `	int iDec = 1;` |
|    100 |  811 | `	PH7_MemObjInit(pVm,&sRes);` |
|    100 |  812 | `	if( pSess ){` |
|      - |  813 | `		int bRead;` |
|    100 |  814 | `		VmSessEmptyArray(pVm,pSess);` |
|    100 |  815 | `		if( VmSessHasUser(pVm) ){` |
|     28 |  816 | `			VmSessUserOpen(pVm);` |
|     52 |  817 | `			bRead = VmSessUserId(pVm,VM_SESS_OP_READ,&sRes) > 0` |
|     24 |  818 | `				&& (sRes.iFlags & MEMOBJ_STRING) && SyBlobLength(&sRes.sBlob) > 0;` |
|     16 |  819 | `		}else{` |
|     76 |  820 | `			bRead = VmSessReadFile(pVm,pFile,&sRes);` |
|      - |  821 | `		}` |
|      - |  822 | `		/* What the store handed back is what session.lazy_write compares the` |
|      - |  823 | `		 * eventual write against. */` |
|    100 |  824 | `		SyBlobReset(&pVm->sSessData);` |
|    100 |  825 | `		if( bRead ){` |
|     84 |  826 | `			SyBlobAppend(&pVm->sSessData,SyBlobData(&sRes.sBlob),SyBlobLength(&sRes.sBlob));` |
|     40 |  827 | `		}` |
|    100 |  828 | `		if( bRead ){` |
|    124 |  829 | `			iDec = VmSessDecodeInto(pCtx,(const char *)SyBlobData(&sRes.sBlob),` |
|     40 |  830 | `				SyBlobLength(&sRes.sBlob),pSess);` |
|     40 |  831 | `		}` |
|     48 |  832 | `	}` |
|    100 |  833 | `	PH7_MemObjRelease(&sRes);` |
|    100 |  834 | `	if( iDec == 0 ){` |
|    ! 0 |  835 | `		VmSessDestroyBadStore(pVm,pFile,zFunc);` |
|    ! 0 |  836 | `	}` |
|    100 |  837 | `	return iDec;` |
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
|    136 |  855 | `static int VmSessLocked(ph7_context *pCtx,const char *zFunc,const char *zWhat,int bHeaders)` |
|      4 |  856 | `{` |
|    140 |  857 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  858 | `	char zMsg[192];` |
|    140 |  859 | `	int bActive = (pVm->iSessStatus == VM_SESSION_ACTIVE);` |
|    140 |  860 | `	if( bActive \|\| (bHeaders && pVm->bHeadersSent) ){` |
|      - |  861 | `		/* php names WHERE, every time: the session_start() that opened the` |
|      - |  862 | `		 * session, or the output that began the response. */` |
|      - |  863 | `		SyBlob sMsg;` |
|     14 |  864 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      4 |  865 | `			bActive ? "%s(): %s cannot be changed when a session is active"` |
|      - |  866 | `			        : "%s(): %s cannot be changed after headers have already been sent",` |
|      4 |  867 | `			zFunc,zWhat);` |
|     10 |  868 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     10 |  869 | `		SyBlobAppend(&sMsg,zMsg,(sxu32)SyStrlen(zMsg));` |
|     10 |  870 | `		PH7_VmAppendWhere(pVm,&sMsg,bActive);` |
|     10 |  871 | `		SyBlobNullAppend(&sMsg);` |
|     10 |  872 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     10 |  873 | `		SyBlobRelease(&sMsg);` |
|     10 |  874 | `		return 1;` |
|      - |  875 | `	}` |
|    132 |  876 | `	return 0;` |
|     72 |  877 | `}` |
|      - |  878 | `/*` |
|      - |  879 | ` * session_id() / session_name() / session_save_path(): read the current value,` |
|      - |  880 | ` * or set it and answer the previous one. One body, three directives.` |
|      - |  881 | ` */` |
|    160 |  882 | `static int VmSessAccessor(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|      - |  883 | `	SyBlob *pSlot,const char *zFunc,const char *zWhat,int bRtrimSlash)` |
|      4 |  884 | `{` |
|    164 |  885 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  886 | `	SyBlob sOld;` |
|    164 |  887 | `	if( pSlot == &pVm->sSessPath ){` |
|     28 |  888 | `		VmSessResolvePath(pVm);` |
|     12 |  889 | `	}` |
|    164 |  890 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|    164 |  891 | `	SyBlobAppend(&sOld,SyBlobData(pSlot),SyBlobLength(pSlot));` |
|    164 |  892 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|     58 |  893 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|     58 |  894 | `		SyBlobRelease(&sOld);` |
|     58 |  895 | `		return PH7_OK;` |
|      - |  896 | `	}` |
|    110 |  897 | `	if( VmSessLocked(pCtx,zFunc,zWhat,pSlot != &pVm->sSessPath) ){` |
|      7 |  898 | `		SyBlobRelease(&sOld);` |
|      7 |  899 | `		ph7_result_bool(pCtx,0);` |
|      7 |  900 | `		return PH7_OK;` |
|      - |  901 | `	}` |
|      - |  902 | `	{` |
|    104 |  903 | `		int nNew = 0;` |
|    104 |  904 | `		const char *zNew = ph7_value_to_string(apArg[0],&nNew);` |
|    104 |  905 | `		sxu32 nLen = (sxu32)nNew;` |
|    104 |  906 | `		if( bRtrimSlash ){` |
|     26 |  907 | `			while( nLen > 0 && zNew[nLen-1] == '/' ){ nLen--; }` |
|     11 |  908 | `		}` |
|    104 |  909 | `		SyBlobReset(pSlot);` |
|    104 |  910 | `		SyBlobAppend(pSlot,zNew,nLen);` |
|      - |  911 | `	}` |
|    104 |  912 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|    104 |  913 | `	SyBlobRelease(&sOld);` |
|    104 |  914 | `	return PH7_OK;` |
|     84 |  915 | `}` |
|    114 |  916 | `static int vm_builtin_session_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  917 | `{` |
|    118 |  918 | `	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessId,` |
|      - |  919 | `		"session_id","Session ID",0);` |
|      4 |  920 | `}` |
|     22 |  921 | `static int vm_builtin_session_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  922 | `{` |
|     25 |  923 | `	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessName,` |
|      - |  924 | `		"session_name","Session name",0);` |
|      3 |  925 | `}` |
|     24 |  926 | `static int vm_builtin_session_save_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  927 | `{` |
|     28 |  928 | `	return VmSessAccessor(pCtx,nArg,apArg,&pCtx->pVm->sSessPath,` |
|      - |  929 | `		"session_save_path","Session save path",1);` |
|      4 |  930 | `}` |
|      - |  931 | `/*` |
|      - |  932 | ` * session_start()'s $options array -- declared in aBuiltinSig[] and read by` |
|      - |  933 | `` * NOTHING, so `session_start(['name' => 'MYSID', 'cookie_lifetime' => 3600])`,`` |
|      - |  934 | ` * php's documented way to configure a session at the one point it can still be` |
|      - |  935 | ` * configured, was accepted and dropped in silence.` |
|      - |  936 | ` *` |
|      - |  937 | ` * Each key is a session.<key> directive applied for this request; the one that is` |
|      - |  938 | `` * not is `read_and_close`, which asks for the session to be closed again the`` |
|      - |  939 | ` * moment it has been read. A key the directive table refuses is reported and the` |
|      - |  940 | ` * session still starts; a key that is not a STRING, or a value that is not a` |
|      - |  941 | ` * scalar, is a hard error before anything is opened.` |
|      - |  942 | ` */` |
|      - |  943 | `static void VmSessWrite(ph7_vm *pVm,const char *zWho);` |
|      - |  944 | `static void VmSessSendCacheHeaders(ph7_vm *pVm);` |
|      - |  945 | `struct VmSessStartOpts {` |
|      - |  946 | `	ph7_vm *pVm;` |
|      - |  947 | `	int bReadClose;` |
|      - |  948 | `	int iFail;          /* 1 = non-string key, 2 = non-scalar value */` |
|      - |  949 | `	char zBadKey[64];` |
|      - |  950 | `	char zBadType[32];` |
|      - |  951 | `};` |
|     34 |  952 | `static int VmSessStartOptWalker(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 |  953 | `{` |
|     35 |  954 | `	struct VmSessStartOpts *pOpt = (struct VmSessStartOpts *)pUserData;` |
|      - |  955 | `	char zName[128];` |
|      - |  956 | `	const char *zKey;` |
|     35 |  957 | `	int nKey = 0, nVal = 0;` |
|      - |  958 | `	const char *zVal;` |
|     35 |  959 | `	if( !ph7_value_is_string(pKey) ){` |
|      3 |  960 | `		pOpt->iFail = 1;` |
|      3 |  961 | `		return SXERR_ABORT;` |
|      - |  962 | `	}` |
|     33 |  963 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     33 |  964 | `	if( nKey > (int)sizeof(pOpt->zBadKey)-1 ){` |
|    ! 0 |  965 | `		nKey = (int)sizeof(pOpt->zBadKey)-1;` |
|    ! 0 |  966 | `	}` |
|     33 |  967 | `	SyMemcpy(zKey,pOpt->zBadKey,(sxu32)nKey);` |
|     33 |  968 | `	pOpt->zBadKey[nKey] = 0;` |
|     33 |  969 | `	if( !ph7_value_is_string(pVal) && !ph7_value_is_int(pVal) && !ph7_value_is_bool(pVal) ){` |
|      3 |  970 | `		const char *zType = PH7_MemObjTypeDump(pVal);` |
|      3 |  971 | `		sxu32 nType = (sxu32)SyStrlen(zType);` |
|      3 |  972 | `		if( nType > sizeof(pOpt->zBadType)-1 ){` |
|    ! 0 |  973 | `			nType = sizeof(pOpt->zBadType)-1;` |
|    ! 0 |  974 | `		}` |
|      3 |  975 | `		SyMemcpy(zType,pOpt->zBadType,nType);` |
|      3 |  976 | `		pOpt->zBadType[nType] = 0;` |
|      3 |  977 | `		pOpt->iFail = 2;` |
|      3 |  978 | `		return SXERR_ABORT;` |
|      - |  979 | `	}` |
|     30 |  980 | `	if( nKey == (int)sizeof("read_and_close")-1` |
|     18 |  981 | `	 && SyMemcmp(pOpt->zBadKey,"read_and_close",(sxu32)nKey) == 0 ){` |
|      3 |  982 | `		pOpt->bReadClose = ph7_value_to_bool(pVal);` |
|      3 |  983 | `		return PH7_OK;` |
|      - |  984 | `	}` |
|      - |  985 | `	/* php stringifies the value the way ini_set() does, a bool becoming "1"/"". */` |
|     29 |  986 | `	if( ph7_value_is_bool(pVal) ){` |
|    ! 0 |  987 | `		zVal = ph7_value_to_bool(pVal) ? "1" : "";` |
|    ! 0 |  988 | `		nVal = (int)SyStrlen(zVal);` |
|    ! 0 |  989 | `	}else{` |
|     29 |  990 | `		zVal = ph7_value_to_string(pVal,&nVal);` |
|      - |  991 | `	}` |
|     29 |  992 | `	SyBufferFormat(zName,sizeof(zName),"session.%s",pOpt->zBadKey);` |
|     29 |  993 | `	if( !PH7_VmIniSet(pOpt->pVm,zName,(sxu32)SyStrlen(zName),zVal,(sxu32)nVal,` |
|      - |  994 | `		"session_start()") ){` |
|      - |  995 | `		char zMsg[160];` |
|     13 |  996 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      8 |  997 | `			"session_start(): Setting option \"%s\" failed",pOpt->zBadKey);` |
|      9 |  998 | `		PH7_VmThrowError(pOpt->pVm,0,PH7_CTX_WARNING,zMsg);` |
|      4 |  999 | `	}` |
|     29 | 1000 | `	return PH7_OK;` |
|     18 | 1001 | `}` |
|      - | 1002 | `/*` |
|      - | 1003 | ` * The Set-Cookie that carries the id, built out of the seven session.cookie_*` |
|      - | 1004 | ` * directives rather than the name and the id alone -- which is what this used to` |
|      - | 1005 | ` * send, so every session cookie went out with no path, no expiry and no` |
|      - | 1006 | ` * HttpOnly/SameSite whatever the configuration said, and a program that had set` |
|      - | 1007 | `` * `session.cookie_secure` was still handing its id to a plaintext request.`` |
|      - | 1008 | ` *` |
|      - | 1009 | ` * php sends it on every start and again whenever the id changes; it is a plain` |
|      - | 1010 | ` * response header, so the CLI has nowhere to put it and simply does not.` |
|      - | 1011 | ` */` |
|    106 | 1012 | `static void VmSessSendCookie(ph7_vm *pVm)` |
|      4 | 1013 | `{` |
|      - | 1014 | `	SyBlob sPath,sDomain,sSame;` |
|      - | 1015 | `	sxi64 iLife;` |
|    110 | 1016 | `	if( !PH7_VmIniGetBool(pVm,"session.use_cookies",1) ){` |
|    ! 0 | 1017 | `		return;` |
|      - | 1018 | `	}` |
|      - | 1019 | `	/* Replace, never accumulate: the reply carries ONE id. */` |
|    163 | 1020 | `	PH7_VmRemoveCookieByName(pVm,(const char *)SyBlobData(&pVm->sSessName),` |
|     53 | 1021 | `		SyBlobLength(&pVm->sSessName));` |
|    110 | 1022 | `	SyBlobInit(&sPath,&pVm->sAllocator);` |
|    110 | 1023 | `	SyBlobInit(&sDomain,&pVm->sAllocator);` |
|    110 | 1024 | `	SyBlobInit(&sSame,&pVm->sAllocator);` |
|    110 | 1025 | `	PH7_VmIniGetStr(pVm,"session.cookie_path",&sPath);` |
|    110 | 1026 | `	PH7_VmIniGetStr(pVm,"session.cookie_domain",&sDomain);` |
|    110 | 1027 | `	PH7_VmIniGetStr(pVm,"session.cookie_samesite",&sSame);` |
|    110 | 1028 | `	iLife = PH7_VmIniGetInt(pVm,"session.cookie_lifetime",0);` |
|    269 | 1029 | `	PH7_VmEmitCookie(pVm,` |
|    106 | 1030 | `		(const char *)SyBlobData(&pVm->sSessName),SyBlobLength(&pVm->sSessName),` |
|    106 | 1031 | `		(const char *)SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId),1,` |
|      - | 1032 | `		/* A lifetime is a DURATION here and an absolute time on the wire; 0 is` |
|      - | 1033 | `		 * php's "until the browser closes", which sends no expiry at all. */` |
|     55 | 1034 | `		iLife > 0 ? (sxi64)time(0) + iLife : 0,` |
|    106 | 1035 | `		(const char *)SyBlobData(&sPath),SyBlobLength(&sPath),` |
|    106 | 1036 | `		(const char *)SyBlobData(&sDomain),SyBlobLength(&sDomain),` |
|     53 | 1037 | `		PH7_VmIniGetBool(pVm,"session.cookie_secure",0),` |
|     53 | 1038 | `		PH7_VmIniGetBool(pVm,"session.cookie_httponly",0),` |
|    106 | 1039 | `		(const char *)SyBlobData(&sSame),SyBlobLength(&sSame),` |
|     53 | 1040 | `		PH7_VmIniGetBool(pVm,"session.cookie_partitioned",0));` |
|    110 | 1041 | `	SyBlobRelease(&sPath);` |
|    110 | 1042 | `	SyBlobRelease(&sDomain);` |
|    110 | 1043 | `	SyBlobRelease(&sSame);` |
|     57 | 1044 | `}` |
|      - | 1045 | `/* bool session_start(array $options = []) */` |
|    112 | 1046 | `static int vm_builtin_session_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1047 | `{` |
|    116 | 1048 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1049 | `	struct VmSessStartOpts sOpt;` |
|      - | 1050 | `	SyBlob sFile;` |
|      - | 1051 | `	int iLoad;` |
|    116 | 1052 | `	SyZero(&sOpt,sizeof(sOpt));` |
|    116 | 1053 | `	sOpt.pVm = pVm;` |
|    116 | 1054 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|     19 | 1055 | `		ph7_array_walk(apArg[0],VmSessStartOptWalker,&sOpt);` |
|     19 | 1056 | `		if( sOpt.iFail == 1 ){` |
|      3 | 1057 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1058 | `				"session_start(): Argument #1 ($options) must be of type array with"` |
|      - | 1059 | `				" keys as string");` |
|      - | 1060 | `		}` |
|     17 | 1061 | `		if( sOpt.iFail == 2 ){` |
|      4 | 1062 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1063 | `				"session_start(): Option \"%s\" must be of type string\|int\|bool, %s given",` |
|      1 | 1064 | `				sOpt.zBadKey,sOpt.zBadType);` |
|      - | 1065 | `		}` |
|      7 | 1066 | `	}` |
|    112 | 1067 | `	if( pVm->iSessStatus == VM_SESSION_ACTIVE ){` |
|    ! 0 | 1068 | `		PH7_VmThrowError(pVm,0,PH7_CTX_NOTICE,` |
|      - | 1069 | `			"session_start(): Ignoring session_start() because a session is already active");` |
|    ! 0 | 1070 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 1071 | `		return PH7_OK;` |
|      - | 1072 | `	}` |
|    112 | 1073 | `	if( pVm->bHeadersSent ){` |
|      - | 1074 | `		SyBlob sMsg;` |
|      6 | 1075 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      6 | 1076 | `		SyBlobAppend(&sMsg,"session_start(): Session cannot be started after headers have already been sent",` |
|      - | 1077 | `			sizeof("session_start(): Session cannot be started after headers have already been sent")-1);` |
|      6 | 1078 | `		PH7_VmAppendWhere(pVm,&sMsg,0);` |
|      6 | 1079 | `		SyBlobNullAppend(&sMsg);` |
|      6 | 1080 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      6 | 1081 | `		SyBlobRelease(&sMsg);` |
|      6 | 1082 | `		ph7_result_bool(pCtx,0);` |
|      6 | 1083 | `		return PH7_OK;` |
|      - | 1084 | `	}` |
|    108 | 1085 | `	if( VmSessSerializerOrErr(pVm) < 0 ){` |
|      - | 1086 | `		/* Only php.ini / -d can arm a handler that does not exist; php reports it` |
|      - | 1087 | `		 * here and starts nothing at all. */` |
|      - | 1088 | `		SyBlob sVal;` |
|      - | 1089 | `		char zMsg[192];` |
|      3 | 1090 | `		SyBlobInit(&sVal,&pVm->sAllocator);` |
|      3 | 1091 | `		PH7_VmIniGetStr(pVm,"session.serialize_handler",&sVal);` |
|      4 | 1092 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 1093 | `			"session_start(): Cannot find session serialization handler \"%.*s\""` |
|      - | 1094 | `			" - session startup failed",` |
|      2 | 1095 | `			(int)SyBlobLength(&sVal),(const char *)SyBlobData(&sVal));` |
|      3 | 1096 | `		SyBlobRelease(&sVal);` |
|      3 | 1097 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      3 | 1098 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1099 | `		return PH7_OK;` |
|      - | 1100 | `	}` |
|    102 | 1101 | `	if( SyBlobLength(&pVm->sSessId) > 0` |
|    103 | 1102 | `	 && VmSessIdDangerous((const char *)SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId)) ){` |
|      - | 1103 | `		/* A byte that would break the header this id is about to be written into:` |
|      - | 1104 | `		 * php throws the id away without a word and makes a fresh one. */` |
|      5 | 1105 | `		SyBlobReset(&pVm->sSessId);` |
|      2 | 1106 | `	}` |
|    106 | 1107 | `	if( SyBlobLength(&pVm->sSessId) == 0 ){` |
|      - | 1108 | `		/* Adopt the id the client sent, when it is one php would accept. An id a` |
|      - | 1109 | `		 * REQUEST supplied is simply not adopted when it is not — the visitor does` |
|      - | 1110 | `		 * not get to end the request — where the one a script SET is reported below. */` |
|     13 | 1111 | `		ph7_value *pCookie = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|     13 | 1112 | `		if( pCookie && (pCookie->iFlags & MEMOBJ_HASHMAP) ){` |
|      - | 1113 | `			ph7_value sKey,sVal;` |
|     13 | 1114 | `			ph7_hashmap_node *pNode = 0;` |
|     18 | 1115 | `			VmSessStrArg(pVm,&sKey,(const char *)SyBlobData(&pVm->sSessName),` |
|      5 | 1116 | `				SyBlobLength(&pVm->sSessName));` |
|     13 | 1117 | `			PH7_MemObjInit(pVm,&sVal);` |
|     10 | 1118 | `			if( PH7_HashmapLookup((ph7_hashmap *)pCookie->x.pOther,&sKey,&pNode) == SXRET_OK` |
|      8 | 1119 | `			 && pNode ){` |
|    ! 0 | 1120 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|    ! 0 | 1121 | `			}` |
|     10 | 1122 | `			if( (sVal.iFlags & MEMOBJ_STRING)` |
|      8 | 1123 | `			 && VmSessIdValid((const char *)SyBlobData(&sVal.sBlob),SyBlobLength(&sVal.sBlob)) ){` |
|    ! 0 | 1124 | `				SyBlobReset(&pVm->sSessId);` |
|    ! 0 | 1125 | `				SyBlobAppend(&pVm->sSessId,SyBlobData(&sVal.sBlob),SyBlobLength(&sVal.sBlob));` |
|    ! 0 | 1126 | `			}` |
|     13 | 1127 | `			PH7_MemObjRelease(&sVal);` |
|     13 | 1128 | `			PH7_MemObjRelease(&sKey);` |
|      5 | 1129 | `		}` |
|      5 | 1130 | `	}` |
|    102 | 1131 | `	if( SyBlobLength(&pVm->sSessId) > 0` |
|    101 | 1132 | `	 && !VmSessIdValid((const char *)SyBlobData(&pVm->sSessId),SyBlobLength(&pVm->sSessId)) ){` |
|      - | 1133 | `		/* The id names a FILE under the save path, so php refuses to open anything at` |
|      - | 1134 | `		 * all for one outside its alphabet — and reports the store's own failure with` |
|      - | 1135 | `		 * it, since that is the read that never happened. */` |
|      - | 1136 | `		char zMsg[224];` |
|      5 | 1137 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1138 | `			"session_start(): Session ID is too long or contains illegal characters."` |
|      - | 1139 | `			" Only the A-Z, a-z, 0-9, \"-\", and \",\" characters are allowed");` |
|      5 | 1140 | `		VmSessResolvePath(pVm);` |
|      7 | 1141 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 1142 | `			"session_start(): Failed to read session data: files (path: %.*s)",` |
|      4 | 1143 | `			(int)SyBlobLength(&pVm->sSessPath),(const char *)SyBlobData(&pVm->sSessPath));` |
|      5 | 1144 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      5 | 1145 | `		SyBlobReset(&pVm->sSessId);` |
|      5 | 1146 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1147 | `		return PH7_OK;` |
|      - | 1148 | `	}` |
|     98 | 1149 | `	if( SyBlobLength(&pVm->sSessId) > 0` |
|     97 | 1150 | `	 && PH7_VmIniGetBool(pVm,"session.use_strict_mode",0) ){` |
|      - | 1151 | `		/* session.use_strict_mode: php refuses an id no STORE has ever seen and` |
|      - | 1152 | `		 * makes a new one, so a visitor cannot choose their own session id and` |
|      - | 1153 | `		 * hand the link to somebody else (session fixation). A userland handler` |
|      - | 1154 | `		 * answers through validateId(); the files store answers by whether the` |
|      - | 1155 | `		 * store is there. */` |
|      - | 1156 | `		int bKnown;` |
|      3 | 1157 | `		if( VmSessHasUser(pVm) ){` |
|      - | 1158 | `			ph7_value sRes;` |
|      3 | 1159 | `			VmSessUserOpen(pVm);` |
|      3 | 1160 | `			PH7_MemObjInit(pVm,&sRes);` |
|      3 | 1161 | `			if( VmSessUserId(pVm,VM_SESS_OP_VALIDATE,&sRes) > 0 ){` |
|      3 | 1162 | `				PH7_MemObjToBool(&sRes);` |
|      3 | 1163 | `				bKnown = sRes.x.iVal != 0;` |
|      2 | 1164 | `			}else{` |
|    ! 0 | 1165 | `				bKnown = 1;   /* no validateId(): php has nothing to refuse with */` |
|      - | 1166 | `			}` |
|      3 | 1167 | `			PH7_MemObjRelease(&sRes);` |
|      2 | 1168 | `		}else{` |
|      - | 1169 | `			SyBlob sProbe;` |
|    ! 0 | 1170 | `			SyBlobInit(&sProbe,&pVm->sAllocator);` |
|    ! 0 | 1171 | `			VmSessFile(pVm,&sProbe);` |
|    ! 0 | 1172 | `			bKnown = VmSessPathIs(pVm,"file_exists",&sProbe);` |
|    ! 0 | 1173 | `			SyBlobRelease(&sProbe);` |
|      - | 1174 | `		}` |
|      3 | 1175 | `		if( !bKnown ){` |
|      3 | 1176 | `			SyBlobReset(&pVm->sSessId);` |
|      1 | 1177 | `		}` |
|      1 | 1178 | `	}` |
|    102 | 1179 | `	if( SyBlobLength(&pVm->sSessId) == 0 && VmSessHasUser(pVm) ){` |
|      - | 1180 | `		/* A handler implementing SessionIdInterface makes the id: a store that` |
|      - | 1181 | `		 * knows how to key itself is the one that should choose the key. */` |
|      - | 1182 | `		ph7_value sRes;` |
|      3 | 1183 | `		VmSessUserOpen(pVm);` |
|      3 | 1184 | `		PH7_MemObjInit(pVm,&sRes);` |
|      3 | 1185 | `		if( VmSessUserCall(pVm,VM_SESS_OP_CREATE,0,0,&sRes) > 0 ){` |
|      3 | 1186 | `			PH7_MemObjToString(&sRes);` |
|      3 | 1187 | `			if( SyBlobLength(&sRes.sBlob) > 0 ){` |
|      3 | 1188 | `				SyBlobReset(&pVm->sSessId);` |
|      3 | 1189 | `				SyBlobAppend(&pVm->sSessId,SyBlobData(&sRes.sBlob),SyBlobLength(&sRes.sBlob));` |
|      1 | 1190 | `			}` |
|      1 | 1191 | `		}` |
|      3 | 1192 | `		PH7_MemObjRelease(&sRes);` |
|      1 | 1193 | `	}` |
|    102 | 1194 | `	if( SyBlobLength(&pVm->sSessId) == 0 ){` |
|     13 | 1195 | `		VmSessGenId(pVm,&pVm->sSessId);` |
|      5 | 1196 | `	}` |
|    102 | 1197 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|    102 | 1198 | `	VmSessFile(pVm,&sFile);` |
|    102 | 1199 | `	if( !VmSessHasUser(pVm) && !VmSessOpenStore(pVm,&sFile,"session_start") ){` |
|      3 | 1200 | `		SyBlobRelease(&sFile);` |
|      3 | 1201 | `		SyBlobReset(&pVm->sSessId);` |
|      3 | 1202 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1203 | `		return PH7_OK;` |
|      - | 1204 | `	}` |
|    100 | 1205 | `	iLoad = VmSessLoad(pCtx,&sFile,"session_start");` |
|    100 | 1206 | `	SyBlobRelease(&sFile);` |
|    100 | 1207 | `	if( iLoad < 0 ){` |
|    ! 0 | 1208 | `		return PH7_EXCEPTION;` |
|      - | 1209 | `	}` |
|    100 | 1210 | `	if( iLoad == 0 ){` |
|    ! 0 | 1211 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1212 | `		return PH7_OK;` |
|      - | 1213 | `	}` |
|      - | 1214 | `	/* php collects HERE -- after the store is open and this session has been read,` |
|      - | 1215 | `	 * which is the only order in which a handler's gc() can use the connection its` |
|      - | 1216 | `	 * own open() made. PHL swept before either, so a user handler was asked to` |
|      - | 1217 | `	 * collect a store it had not been told to open yet: its log read gc,open,read` |
|      - | 1218 | `	 * where php's reads open,read,gc. */` |
|    100 | 1219 | `	VmSessMaybeGc(pVm);` |
|    100 | 1220 | `	pVm->iSessStatus = VM_SESSION_ACTIVE;` |
|      - | 1221 | `	/* Remember WHERE, for the refusals a later write to a session directive` |
|      - | 1222 | `	 * meets. */` |
|    100 | 1223 | `	PH7_VmSetSessionOrigin(pVm);` |
|    100 | 1224 | `	VmSessSendCookie(pVm);` |
|    100 | 1225 | `	VmSessSendCacheHeaders(pVm);` |
|    100 | 1226 | `	if( sOpt.bReadClose ){` |
|      - | 1227 | ``		/* php's `read_and_close`: the store is read and released again before the`` |
|      - | 1228 | `		 * script runs, so a request that only READS the session does not hold its` |
|      - | 1229 | `		 * lock for the rest of its life. */` |
|      3 | 1230 | `		VmSessWrite(pVm,"session_start()");` |
|      1 | 1231 | `	}` |
|    100 | 1232 | `	ph7_result_bool(pCtx,1);` |
|    100 | 1233 | `	return PH7_OK;` |
|     60 | 1234 | `}` |
|      - | 1235 | `/*` |
|      - | 1236 | ` * Write the open session back to its file and close it. Shared by the builtin and` |
|      - | 1237 | ` * by the request-shutdown writer, which is why it takes only the VM: at shutdown` |
|      - | 1238 | ` * there is no calling frame to report against.` |
|      - | 1239 | ` */` |
|    126 | 1240 | `static void VmSessPutFileQuiet(ph7_vm *pVm,SyBlob *pFile,const char *zData,sxu32 nData,` |
|      - | 1241 | `	int bQuiet)` |
|      4 | 1242 | `{` |
|      - | 1243 | `	ph7_value sPath,sPayload;` |
|      - | 1244 | `	ph7_value *apA[2];` |
|    130 | 1245 | `	VmSessStrArg(pVm,&sPath,(const char *)SyBlobData(pFile),SyBlobLength(pFile));` |
|    130 | 1246 | `	VmSessStrArg(pVm,&sPayload,zData,nData);` |
|    130 | 1247 | `	apA[0] = &sPath;` |
|    130 | 1248 | `	apA[1] = &sPayload;` |
|    130 | 1249 | `	if( bQuiet ){` |
|     48 | 1250 | `		VmSessCallQuiet(pVm,"file_put_contents",2,apA,0);` |
|     26 | 1251 | `	}else{` |
|     86 | 1252 | `		VmSessCall(pVm,"file_put_contents",2,apA,0);` |
|      - | 1253 | `	}` |
|    130 | 1254 | `	PH7_MemObjRelease(&sPath);` |
|    130 | 1255 | `	PH7_MemObjRelease(&sPayload);` |
|    130 | 1256 | `}` |
|     82 | 1257 | `static void VmSessPutFile(ph7_vm *pVm,SyBlob *pFile,const char *zData,sxu32 nData)` |
|      4 | 1258 | `{` |
|     86 | 1259 | `	VmSessPutFileQuiet(pVm,pFile,zData,nData,0);` |
|     86 | 1260 | `}` |
|      - | 1261 | `/* Encode the open session and write it to the file the CURRENT id names. */` |
|     88 | 1262 | `static void VmSessSave(ph7_vm *pVm,const char *zWho)` |
|      4 | 1263 | `{` |
|      - | 1264 | `	SyBlob sFile,sData;` |
|     92 | 1265 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|     92 | 1266 | `	SyBlobInit(&sData,&pVm->sAllocator);` |
|     92 | 1267 | `	VmSessFile(pVm,&sFile);` |
|      - | 1268 | `	/* An encode php refused still gets written — as the EMPTY payload, which is` |
|      - | 1269 | `	 * what its store is handed when the serializer answers nothing. The stale copy` |
|      - | 1270 | `	 * does not survive either way. */` |
|     92 | 1271 | `	VmSessEncode(pVm,&sData,zWho);` |
|     92 | 1272 | `	if( VmSessHasUser(pVm) ){` |
|      - | 1273 | `		ph7_value sId,sPayload,sRes;` |
|      - | 1274 | `		ph7_value *apA[2];` |
|     24 | 1275 | `		int iOp = VM_SESS_OP_WRITE;` |
|     20 | 1276 | `		if( PH7_VmIniGetBool(pVm,"session.lazy_write",1)` |
|     20 | 1277 | `		 && SyBlobLength(&pVm->sSessData) > 0` |
|     13 | 1278 | `		 && SyBlobLength(&sData) == SyBlobLength(&pVm->sSessData)` |
|     10 | 1279 | `		 && SyMemcmp(SyBlobData(&sData),SyBlobData(&pVm->sSessData),` |
|      6 | 1280 | `			SyBlobLength(&sData)) == 0 ){` |
|      - | 1281 | `			/* session.lazy_write, php's default: a session whose data did not` |
|      - | 1282 | `			 * change is not written again — the store is only told the session was` |
|      - | 1283 | `			 * USED, so its expiry moves without the payload going over the wire.` |
|      - | 1284 | `			 * A handler that does not implement the interface still gets write().` |
|      - | 1285 | `			 *` |
|      - | 1286 | `			 * "Did not change" is a comparison against what the READ returned, so` |
|      - | 1287 | `			 * there has to have BEEN one: php keeps the read's value and skips this` |
|      - | 1288 | `			 * branch entirely when the store had nothing (its read reports failure` |
|      - | 1289 | `			 * for an absent session). Comparing lengths alone made a BRAND-NEW id` |
|      - | 1290 | `			 * whose $_SESSION stayed empty look unchanged — both payloads empty — so` |
|      - | 1291 | `			 * the one call that would have created the store never happened, and a` |
|      - | 1292 | `			 * handler that implements write() but not the optional updateTimestamp()` |
|      - | 1293 | `			 * was never told about the session at all. */` |
|      6 | 1294 | `			iOp = VM_SESS_OP_UPDATE;` |
|      2 | 1295 | `		}` |
|     34 | 1296 | `		VmSessStrArg(pVm,&sId,(const char *)SyBlobData(&pVm->sSessId),` |
|     10 | 1297 | `			SyBlobLength(&pVm->sSessId));` |
|     24 | 1298 | `		VmSessStrArg(pVm,&sPayload,(const char *)SyBlobData(&sData),SyBlobLength(&sData));` |
|     24 | 1299 | `		PH7_MemObjInit(pVm,&sRes);` |
|     24 | 1300 | `		apA[0] = &sId;` |
|     24 | 1301 | `		apA[1] = &sPayload;` |
|     20 | 1302 | `		if( iOp == VM_SESS_OP_WRITE` |
|     16 | 1303 | `		 \|\| VmSessUserCall(pVm,VM_SESS_OP_UPDATE,2,apA,&sRes) == 0 ){` |
|     20 | 1304 | `			VmSessUserCall(pVm,VM_SESS_OP_WRITE,2,apA,&sRes);` |
|      8 | 1305 | `		}` |
|     24 | 1306 | `		PH7_MemObjRelease(&sRes);` |
|     24 | 1307 | `		PH7_MemObjRelease(&sPayload);` |
|     24 | 1308 | `		PH7_MemObjRelease(&sId);` |
|     14 | 1309 | `	}else{` |
|     72 | 1310 | `		VmSessPutFile(pVm,&sFile,(const char *)SyBlobData(&sData),SyBlobLength(&sData));` |
|      - | 1311 | `	}` |
|     92 | 1312 | `	SyBlobRelease(&sFile);` |
|     92 | 1313 | `	SyBlobRelease(&sData);` |
|     92 | 1314 | `}` |
|     80 | 1315 | `static void VmSessWrite(ph7_vm *pVm,const char *zWho)` |
|      4 | 1316 | `{` |
|     84 | 1317 | `	VmSessSave(pVm,zWho);` |
|     84 | 1318 | `	VmSessCloseStore(pVm);` |
|     84 | 1319 | `	pVm->iSessStatus = VM_SESSION_NONE;` |
|     84 | 1320 | `}` |
|      - | 1321 | `/* bool session_write_close() / session_commit() */` |
|     68 | 1322 | `static int vm_builtin_session_write_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1323 | `{` |
|     72 | 1324 | `	ph7_vm *pVm = pCtx->pVm;` |
|     34 | 1325 | `	SXUNUSED(nArg);` |
|     34 | 1326 | `	SXUNUSED(apArg);` |
|     72 | 1327 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      3 | 1328 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1329 | `		return PH7_OK;` |
|      - | 1330 | `	}` |
|     70 | 1331 | `	VmSessWrite(pVm,"session_write_close()");` |
|     70 | 1332 | `	ph7_result_bool(pCtx,1);` |
|     70 | 1333 | `	return PH7_OK;` |
|     38 | 1334 | `}` |
|      - | 1335 | `/*` |
|      - | 1336 | ` * php writes an open session back at REQUEST SHUTDOWN — from the session module's` |
|      - | 1337 | ` * own RSHUTDOWN, which runs after the script's register_shutdown_function()` |
|      - | 1338 | ` * callbacks and after the output buffers are flushed. Registering the writer as a` |
|      - | 1339 | ``  * shutdown callback (what this did) put it FIRST in that list, so a `$_SESSION` `` |
|      - | 1340 | ` * entry written from inside a shutdown callback — the flash-message / last-seen` |
|      - | 1341 | ` * idiom — was silently dropped.` |
|      - | 1342 | ` */` |
|   6885 | 1343 | `PH7_PRIVATE void PH7_VmSessionShutdown(ph7_vm *pVm)` |
|      5 | 1344 | `{` |
|   6890 | 1345 | `	if( pVm->iSessStatus == VM_SESSION_ACTIVE ){` |
|      - | 1346 | `		/* php names this caller "PHP Request Shutdown" — there is no frame to` |
|      - | 1347 | `		 * report against, so a serializer diagnostic raised here says so. */` |
|     12 | 1348 | `		VmSessWrite(pVm,"PHP Request Shutdown");` |
|      6 | 1349 | `	}` |
|   6890 | 1350 | `}` |
|      - | 1351 | `/* bool session_abort() — drop the in-memory session without writing it back */` |
|     10 | 1352 | `static int vm_builtin_session_abort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1353 | `{` |
|      5 | 1354 | `	SXUNUSED(nArg);` |
|      5 | 1355 | `	SXUNUSED(apArg);` |
|     11 | 1356 | `	if( pCtx->pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|    ! 0 | 1357 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1358 | `		return PH7_OK;` |
|      - | 1359 | `	}` |
|      - | 1360 | `	/* Nothing is written, but the store is still RELEASED: abandoning the session` |
|      - | 1361 | `	 * is the end of this request's use of it. */` |
|     11 | 1362 | `	VmSessCloseStore(pCtx->pVm);` |
|     11 | 1363 | `	pCtx->pVm->iSessStatus = VM_SESSION_NONE;` |
|     11 | 1364 | `	ph7_result_bool(pCtx,1);` |
|     11 | 1365 | `	return PH7_OK;` |
|      6 | 1366 | `}` |
|      - | 1367 | `/* bool session_reset() — re-read the stored copy over the in-memory one */` |
|    ! 0 | 1368 | `static int vm_builtin_session_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1369 | `{` |
|    ! 0 | 1370 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1371 | `	SyBlob sFile;` |
|      - | 1372 | `	int iLoad;` |
|    ! 0 | 1373 | `	SXUNUSED(nArg);` |
|    ! 0 | 1374 | `	SXUNUSED(apArg);` |
|    ! 0 | 1375 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|    ! 0 | 1376 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1377 | `		return PH7_OK;` |
|      - | 1378 | `	}` |
|    ! 0 | 1379 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|    ! 0 | 1380 | `	VmSessFile(pVm,&sFile);` |
|    ! 0 | 1381 | `	iLoad = VmSessLoad(pCtx,&sFile,"session_reset");` |
|    ! 0 | 1382 | `	SyBlobRelease(&sFile);` |
|    ! 0 | 1383 | `	if( iLoad < 0 ){` |
|    ! 0 | 1384 | `		return PH7_EXCEPTION;` |
|      - | 1385 | `	}` |
|      - | 1386 | `	/* php answers TRUE even when the store it just read was the one it had to` |
|      - | 1387 | `	 * destroy: the reset itself did happen. */` |
|    ! 0 | 1388 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 1389 | `	return PH7_OK;` |
|    ! 0 | 1390 | `}` |
|      - | 1391 | `/* bool session_unset() — empty $_SESSION, keep the session open */` |
|      2 | 1392 | `static int vm_builtin_session_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1393 | `{` |
|      3 | 1394 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1395 | `	ph7_value *pSess;` |
|      1 | 1396 | `	SXUNUSED(nArg);` |
|      1 | 1397 | `	SXUNUSED(apArg);` |
|      3 | 1398 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      3 | 1399 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1400 | `		return PH7_OK;` |
|      - | 1401 | `	}` |
|    ! 0 | 1402 | `	pSess = VmSessArray(pVm);` |
|    ! 0 | 1403 | `	if( pSess ){` |
|    ! 0 | 1404 | `		PH7_MemObjRelease(pSess);` |
|    ! 0 | 1405 | `		pSess->x.pOther = PH7_NewHashmap(pVm,0,0);` |
|    ! 0 | 1406 | `		if( pSess->x.pOther ){` |
|    ! 0 | 1407 | `			MemObjSetType(pSess,MEMOBJ_HASHMAP);` |
|    ! 0 | 1408 | `		}` |
|    ! 0 | 1409 | `	}` |
|    ! 0 | 1410 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 1411 | `	return PH7_OK;` |
|      2 | 1412 | `}` |
|      - | 1413 | `/* bool session_destroy() */` |
|      6 | 1414 | `static int vm_builtin_session_destroy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1415 | `{` |
|      9 | 1416 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1417 | `	SyBlob sFile;` |
|      3 | 1418 | `	SXUNUSED(nArg);` |
|      3 | 1419 | `	SXUNUSED(apArg);` |
|      9 | 1420 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      3 | 1421 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1422 | `			"session_destroy(): Trying to destroy uninitialized session");` |
|      3 | 1423 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1424 | `		return PH7_OK;` |
|      - | 1425 | `	}` |
|      6 | 1426 | `	if( VmSessHasUser(pVm) ){` |
|      - | 1427 | `		ph7_value sRes;` |
|      3 | 1428 | `		PH7_MemObjInit(pVm,&sRes);` |
|      3 | 1429 | `		VmSessUserId(pVm,VM_SESS_OP_DESTROY,&sRes);` |
|      3 | 1430 | `		PH7_MemObjRelease(&sRes);` |
|      - | 1431 | `		/* The store is released as well: destroying is the end of this session's` |
|      - | 1432 | `		 * use of it, exactly as closing is. */` |
|      3 | 1433 | `		VmSessCloseStore(pVm);` |
|      2 | 1434 | `	}else{` |
|      3 | 1435 | `		SyBlobInit(&sFile,&pVm->sAllocator);` |
|      3 | 1436 | `		VmSessFile(pVm,&sFile);` |
|      3 | 1437 | `		VmSessUnlinkIfExists(pVm,&sFile);` |
|      3 | 1438 | `		SyBlobRelease(&sFile);` |
|      - | 1439 | `	}` |
|      6 | 1440 | `	pVm->iSessStatus = VM_SESSION_NONE;` |
|      6 | 1441 | `	SyBlobReset(&pVm->sSessId);` |
|      6 | 1442 | `	ph7_result_bool(pCtx,1);` |
|      6 | 1443 | `	return PH7_OK;` |
|      6 | 1444 | `}` |
|      - | 1445 | `/* bool session_regenerate_id(bool $delete_old_session = false) */` |
|     12 | 1446 | `static int vm_builtin_session_regenerate_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1447 | `{` |
|     15 | 1448 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1449 | `	SyBlob sFile;` |
|     15 | 1450 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      3 | 1451 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1452 | `			"session_regenerate_id(): Session ID cannot be regenerated when there is no active session");` |
|      3 | 1453 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1454 | `		return PH7_OK;` |
|      - | 1455 | `	}` |
|     12 | 1456 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|     12 | 1457 | `	if( nArg > 0 && ph7_value_to_bool(apArg[0]) ){` |
|      3 | 1458 | `		VmSessFile(pVm,&sFile);` |
|      3 | 1459 | `		VmSessUnlinkIfExists(pVm,&sFile);` |
|      2 | 1460 | `	}else{` |
|      - | 1461 | `		/* The old id keeps what the session holds NOW. A regenerating request is the` |
|      - | 1462 | `		 * one whose reply may not arrive, so php leaves the previous id readable` |
|      - | 1463 | `		 * rather than a session that exists under neither id. */` |
|     10 | 1464 | `		VmSessSave(pVm,"session_regenerate_id()");` |
|      - | 1465 | `	}` |
|     12 | 1466 | `	VmSessGenId(pVm,&pVm->sSessId);` |
|      - | 1467 | `	/* The new id's store is OPENED, which for the files handler means it exists and` |
|      - | 1468 | `	 * is empty until the session closes over it. */` |
|     12 | 1469 | `	VmSessFile(pVm,&sFile);` |
|     12 | 1470 | `	VmSessPutFile(pVm,&sFile,"",0);` |
|     12 | 1471 | `	SyBlobRelease(&sFile);` |
|      - | 1472 | `	/* The client is told the new id here; without this the browser keeps sending` |
|      - | 1473 | `	 * the old one and the regenerated session is unreachable. */` |
|     12 | 1474 | `	VmSessSendCookie(pVm);` |
|     12 | 1475 | `	ph7_result_bool(pCtx,1);` |
|     12 | 1476 | `	return PH7_OK;` |
|      9 | 1477 | `}` |
|      - | 1478 | `/*` |
|      - | 1479 | ` * The seven cookie directives, in the order php's session_get_cookie_params()` |
|      - | 1480 | ` * reports them and with the TYPE each one is reported as: an int lifetime, three` |
|      - | 1481 | ` * bools, three strings.` |
|      - | 1482 | ` */` |
|      - | 1483 | `static const struct {` |
|      - | 1484 | `	const char *zKey;` |
|      - | 1485 | `	const char *zIni;` |
|      - | 1486 | `	int iKind;    /* 0 = string, 1 = int, 2 = bool */` |
|      - | 1487 | `} aSessCookieParam[] = {` |
|      - | 1488 | `	{ "lifetime",    "session.cookie_lifetime",    1 },` |
|      - | 1489 | `	{ "path",        "session.cookie_path",        0 },` |
|      - | 1490 | `	{ "domain",      "session.cookie_domain",      0 },` |
|      - | 1491 | `	{ "secure",      "session.cookie_secure",      2 },` |
|      - | 1492 | `	{ "partitioned", "session.cookie_partitioned", 2 },` |
|      - | 1493 | `	{ "httponly",    "session.cookie_httponly",    2 },` |
|      - | 1494 | `	{ "samesite",    "session.cookie_samesite",    0 },` |
|      - | 1495 | `};` |
|      - | 1496 | `/* array session_get_cookie_params() */` |
|      8 | 1497 | `static int vm_builtin_session_get_cookie_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1498 | `{` |
|      9 | 1499 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1500 | `	ph7_value *pArray,*pVal;` |
|      - | 1501 | `	sxu32 n;` |
|      4 | 1502 | `	SXUNUSED(nArg);` |
|      4 | 1503 | `	SXUNUSED(apArg);` |
|      9 | 1504 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 1505 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      9 | 1506 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 | 1507 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1508 | `		return PH7_OK;` |
|      - | 1509 | `	}` |
|     65 | 1510 | `	for( n = 0 ; n < SX_ARRAYSIZE(aSessCookieParam) ; n++ ){` |
|     57 | 1511 | `		if( aSessCookieParam[n].iKind == 1 ){` |
|      9 | 1512 | `			ph7_value_int64(pVal,PH7_VmIniGetInt(pVm,aSessCookieParam[n].zIni,0));` |
|     53 | 1513 | `		}else if( aSessCookieParam[n].iKind == 2 ){` |
|     25 | 1514 | `			ph7_value_bool(pVal,PH7_VmIniGetBool(pVm,aSessCookieParam[n].zIni,0));` |
|     13 | 1515 | `		}else{` |
|      - | 1516 | `			SyBlob sVal;` |
|     25 | 1517 | `			SyBlobInit(&sVal,&pVm->sAllocator);` |
|     25 | 1518 | `			PH7_VmIniGetStr(pVm,aSessCookieParam[n].zIni,&sVal);` |
|     37 | 1519 | `			ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sVal),` |
|     24 | 1520 | `				(const char *)SyBlobData(&sVal));` |
|     25 | 1521 | `			SyBlobRelease(&sVal);` |
|      - | 1522 | `		}` |
|     57 | 1523 | `		ph7_array_add_strkey_elem(pArray,aSessCookieParam[n].zKey,pVal);` |
|     57 | 1524 | `		ph7_value_reset_string_cursor(pVal);` |
|     29 | 1525 | `	}` |
|      9 | 1526 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 1527 | `	return PH7_OK;` |
|      5 | 1528 | `}` |
|      - | 1529 | `/* Push one cookie parameter through the ini table, which is its only store. */` |
|     18 | 1530 | `static void VmSessSetCookieIni(ph7_vm *pVm,const char *zIni,const char *zVal,int nVal)` |
|      1 | 1531 | `{` |
|      - | 1532 | `	ph7_value sName,sVal;` |
|      - | 1533 | `	ph7_value *apA[2];` |
|     19 | 1534 | `	VmSessStrArg(pVm,&sName,zIni,(sxu32)SyStrlen(zIni));` |
|     19 | 1535 | `	VmSessStrArg(pVm,&sVal,zVal,(sxu32)nVal);` |
|     19 | 1536 | `	apA[0] = &sName;` |
|     19 | 1537 | `	apA[1] = &sVal;` |
|     19 | 1538 | `	VmSessCall(pVm,"ini_set",2,apA,0);` |
|     19 | 1539 | `	PH7_MemObjRelease(&sName);` |
|     19 | 1540 | `	PH7_MemObjRelease(&sVal);` |
|     19 | 1541 | `}` |
|      6 | 1542 | `static void VmSessSetCookieBool(ph7_vm *pVm,const char *zIni,int bVal)` |
|      1 | 1543 | `{` |
|      7 | 1544 | `	VmSessSetCookieIni(pVm,zIni,bVal ? "1" : "0",1);` |
|      7 | 1545 | `}` |
|      - | 1546 | `struct VmSessCookieArgs {` |
|      - | 1547 | `	ph7_vm *pVm;` |
|      - | 1548 | `	int nApplied;` |
|      - | 1549 | `	char zBadKey[64];` |
|      - | 1550 | `};` |
|     12 | 1551 | `static int VmSessCookieOptWalker(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 | 1552 | `{` |
|     13 | 1553 | `	struct VmSessCookieArgs *pArgs = (struct VmSessCookieArgs *)pUserData;` |
|      - | 1554 | `	const char *zKey;` |
|     13 | 1555 | `	int nKey = 0;` |
|      - | 1556 | `	sxu32 n;` |
|     13 | 1557 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     63 | 1558 | `	for( n = 0 ; n < SX_ARRAYSIZE(aSessCookieParam) ; n++ ){` |
|     58 | 1559 | `		if( nKey == (int)SyStrlen(aSessCookieParam[n].zKey)` |
|     36 | 1560 | `		 && SyStrnicmp(zKey,aSessCookieParam[n].zKey,(sxu32)nKey) == 0 ){` |
|      9 | 1561 | `			if( aSessCookieParam[n].iKind == 2 ){` |
|      4 | 1562 | `				VmSessSetCookieBool(pArgs->pVm,aSessCookieParam[n].zIni,` |
|      1 | 1563 | `					ph7_value_to_bool(pVal));` |
|      2 | 1564 | `			}else{` |
|      7 | 1565 | `				int nVal = 0;` |
|      7 | 1566 | `				const char *zVal = ph7_value_to_string(pVal,&nVal);` |
|      7 | 1567 | `				VmSessSetCookieIni(pArgs->pVm,aSessCookieParam[n].zIni,zVal,nVal);` |
|      - | 1568 | `			}` |
|      9 | 1569 | `			pArgs->nApplied++;` |
|      9 | 1570 | `			return PH7_OK;` |
|      - | 1571 | `		}` |
|     26 | 1572 | `	}` |
|      - | 1573 | `	/* php reports the key and carries on; it is only an error when NONE of the` |
|      - | 1574 | `	 * keys were ones it knows. */` |
|      5 | 1575 | `	if( pArgs->zBadKey[0] == 0 ){` |
|      5 | 1576 | `		sxu32 nCopy = (sxu32)nKey;` |
|      5 | 1577 | `		if( nCopy > sizeof(pArgs->zBadKey)-1 ){` |
|    ! 0 | 1578 | `			nCopy = sizeof(pArgs->zBadKey)-1;` |
|    ! 0 | 1579 | `		}` |
|      5 | 1580 | `		SyMemcpy(zKey,pArgs->zBadKey,nCopy);` |
|      5 | 1581 | `		pArgs->zBadKey[nCopy] = 0;` |
|      2 | 1582 | `	}` |
|      - | 1583 | `	{` |
|      - | 1584 | `		char zMsg[160];` |
|      7 | 1585 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 1586 | `			"session_set_cookie_params(): Argument #1 ($lifetime_or_options)"` |
|      2 | 1587 | `			" contains an unrecognized key \"%.*s\"",nKey,zKey);` |
|      5 | 1588 | `		PH7_VmThrowError(pArgs->pVm,0,PH7_CTX_WARNING,zMsg);` |
|      - | 1589 | `	}` |
|      5 | 1590 | `	return PH7_OK;` |
|      7 | 1591 | `}` |
|      - | 1592 | `/*` |
|      - | 1593 | ` * bool session_set_cookie_params(array\|int $lifetime_or_options, ?string $path = null,` |
|      - | 1594 | ` *     ?string $domain = null, ?bool $secure = null, ?bool $httponly = null)` |
|      - | 1595 | ` */` |
|     16 | 1596 | `static int vm_builtin_session_set_cookie_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1597 | `{` |
|     17 | 1598 | `	ph7_vm *pVm = pCtx->pVm;` |
|     17 | 1599 | `	if( nArg < 1 ){` |
|    ! 0 | 1600 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      - | 1601 | `			"session_set_cookie_params() expects at least 1 argument, 0 given");` |
|      - | 1602 | `	}` |
|     17 | 1603 | `	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_int(apArg[0]) ){` |
|      4 | 1604 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1605 | `			"session_set_cookie_params(): Argument #1 ($lifetime_or_options) must be"` |
|      1 | 1606 | `			" of type array\|int, %s given",PH7_MemObjTypeDump(apArg[0]));` |
|      - | 1607 | `	}` |
|      - | 1608 | `	/* Both refusals are about the header the parameters would have gone into: one` |
|      - | 1609 | `	 * already written, or one this session already sent. */` |
|     15 | 1610 | `	if( VmSessLocked(pCtx,"session_set_cookie_params","Session cookie parameters",1) ){` |
|      3 | 1611 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1612 | `		return PH7_OK;` |
|      - | 1613 | `	}` |
|     13 | 1614 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 1615 | `		struct VmSessCookieArgs sArgs;` |
|      9 | 1616 | `		SyZero(&sArgs,sizeof(sArgs));` |
|      9 | 1617 | `		sArgs.pVm = pVm;` |
|      9 | 1618 | `		ph7_array_walk(apArg[0],VmSessCookieOptWalker,&sArgs);` |
|      9 | 1619 | `		if( sArgs.nApplied < 1 ){` |
|      5 | 1620 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1621 | `				"session_set_cookie_params(): Argument #1 ($lifetime_or_options) must"` |
|      - | 1622 | `				" contain at least 1 valid key");` |
|      - | 1623 | `		}` |
|      3 | 1624 | `	}else{` |
|      5 | 1625 | `		sxi64 iLife = ph7_value_to_int64(apArg[0]);` |
|      - | 1626 | `		char zLife[32];` |
|      - | 1627 | `		int nLife;` |
|      5 | 1628 | `		if( iLife < 0 ){` |
|      3 | 1629 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1630 | `				"session_set_cookie_params(): CookieLifetime cannot be negative");` |
|      3 | 1631 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1632 | `			return PH7_OK;` |
|      - | 1633 | `		}` |
|      3 | 1634 | `		nLife = SyBufferFormat(zLife,sizeof(zLife),"%qd",iLife);` |
|      3 | 1635 | `		VmSessSetCookieIni(pVm,"session.cookie_lifetime",zLife,nLife);` |
|      3 | 1636 | `		if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      3 | 1637 | `			int nVal = 0;` |
|      3 | 1638 | `			const char *zVal = ph7_value_to_string(apArg[1],&nVal);` |
|      3 | 1639 | `			VmSessSetCookieIni(pVm,"session.cookie_path",zVal,nVal);` |
|      1 | 1640 | `		}` |
|      3 | 1641 | `		if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      3 | 1642 | `			int nVal = 0;` |
|      3 | 1643 | `			const char *zVal = ph7_value_to_string(apArg[2],&nVal);` |
|      3 | 1644 | `			VmSessSetCookieIni(pVm,"session.cookie_domain",zVal,nVal);` |
|      1 | 1645 | `		}` |
|      3 | 1646 | `		if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|      3 | 1647 | `			VmSessSetCookieBool(pVm,"session.cookie_secure",ph7_value_to_bool(apArg[3]));` |
|      1 | 1648 | `		}` |
|      3 | 1649 | `		if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|      3 | 1650 | `			VmSessSetCookieBool(pVm,"session.cookie_httponly",ph7_value_to_bool(apArg[4]));` |
|      1 | 1651 | `		}` |
|      - | 1652 | `	}` |
|      7 | 1653 | `	ph7_result_bool(pCtx,1);` |
|      7 | 1654 | `	return PH7_OK;` |
|      9 | 1655 | `}` |
|      - | 1656 | `/*` |
|      - | 1657 | ` * php's four cache limiters, and the headers each one puts on a reply that` |
|      - | 1658 | `` * carries a session. A session is per-visitor state, so the default (`nocache`)`` |
|      - | 1659 | `` * tells every cache in the path not to keep the page at all; `public` is the`` |
|      - | 1660 | `` * opt-out, and the two `private` forms let the BROWSER keep it while no shared`` |
|      - | 1661 | ` * cache may. A limiter php does not know (including the empty one, which is how a` |
|      - | 1662 | ` * program turns this off) sends nothing -- php validates nothing here.` |
|      - | 1663 | ` *` |
|      - | 1664 | `` * `Expires: Thu, 19 Nov 1981 08:52:00 GMT` is php's own already-expired constant,`` |
|      - | 1665 | ` * and Last-Modified is the mtime of the script that started the session.` |
|      - | 1666 | ` */` |
|     94 | 1667 | `static void VmSessSendCacheHeaders(ph7_vm *pVm)` |
|      4 | 1668 | `{` |
|      - | 1669 | `	SyBlob sLimiter;` |
|      - | 1670 | `	const char *zLim;` |
|      - | 1671 | `	sxu32 nLim;` |
|      - | 1672 | `	sxi64 iExpire;` |
|      - | 1673 | `	char zBuf[128];` |
|      - | 1674 | `	int nBuf;` |
|     98 | 1675 | `	if( !pVm->bHttpContext ){` |
|     88 | 1676 | `		return;` |
|      - | 1677 | `	}` |
|     10 | 1678 | `	SyBlobInit(&sLimiter,&pVm->sAllocator);` |
|     10 | 1679 | `	PH7_VmIniGetStr(pVm,"session.cache_limiter",&sLimiter);` |
|     10 | 1680 | `	zLim = (const char *)SyBlobData(&sLimiter);` |
|     10 | 1681 | `	nLim = SyBlobLength(&sLimiter);` |
|     10 | 1682 | `	iExpire = PH7_VmIniGetInt(pVm,"session.cache_expire",180) * 60;` |
|     10 | 1683 | `	if( nLim == sizeof("nocache")-1 && SyMemcmp(zLim,"nocache",nLim) == 0 ){` |
|      4 | 1684 | `		PH7_VmSetResponseHeader(pVm,"Expires","Thu, 19 Nov 1981 08:52:00 GMT",` |
|      - | 1685 | `			sizeof("Thu, 19 Nov 1981 08:52:00 GMT")-1);` |
|      4 | 1686 | `		PH7_VmSetResponseHeader(pVm,"Cache-Control","no-store, no-cache, must-revalidate",` |
|      - | 1687 | `			sizeof("no-store, no-cache, must-revalidate")-1);` |
|      4 | 1688 | `		PH7_VmSetResponseHeader(pVm,"Pragma","no-cache",sizeof("no-cache")-1);` |
|      8 | 1689 | `	}else if( (nLim == sizeof("public")-1 && SyMemcmp(zLim,"public",nLim) == 0)` |
|      5 | 1690 | `	       \|\| (nLim == sizeof("private")-1 && SyMemcmp(zLim,"private",nLim) == 0)` |
|      3 | 1691 | `	       \|\| (nLim == sizeof("private_no_expire")-1` |
|      2 | 1692 | `	        && SyMemcmp(zLim,"private_no_expire",nLim) == 0) ){` |
|      6 | 1693 | `		int bPublic = nLim == sizeof("public")-1;` |
|      6 | 1694 | `		int bNoExpire = nLim == sizeof("private_no_expire")-1;` |
|      6 | 1695 | `		if( bPublic ){` |
|      2 | 1696 | `			nBuf = PH7_VmHttpDate((sxi64)time(0) + iExpire,zBuf,(int)sizeof(zBuf));` |
|      2 | 1697 | `			if( nBuf > 0 ){` |
|      2 | 1698 | `				PH7_VmSetResponseHeader(pVm,"Expires",zBuf,(sxu32)nBuf);` |
|      1 | 1699 | `			}` |
|      5 | 1700 | `		}else if( !bNoExpire ){` |
|      2 | 1701 | `			PH7_VmSetResponseHeader(pVm,"Expires","Thu, 19 Nov 1981 08:52:00 GMT",` |
|      - | 1702 | `				sizeof("Thu, 19 Nov 1981 08:52:00 GMT")-1);` |
|      1 | 1703 | `		}` |
|      9 | 1704 | `		nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%s, max-age=%qd",` |
|      3 | 1705 | `			bPublic ? "public" : "private",iExpire);` |
|      6 | 1706 | `		PH7_VmSetResponseHeader(pVm,"Cache-Control",zBuf,(sxu32)nBuf);` |
|      - | 1707 | `		{` |
|      - | 1708 | `			/* The document's own age: the script that is running. */` |
|      6 | 1709 | `			SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      6 | 1710 | `			if( pFile && pFile->nByte > 0 ){` |
|      - | 1711 | `				ph7_value sPath,sTime;` |
|      - | 1712 | `				ph7_value *apA[1];` |
|      6 | 1713 | `				VmSessStrArg(pVm,&sPath,pFile->zString,pFile->nByte);` |
|      6 | 1714 | `				PH7_MemObjInit(pVm,&sTime);` |
|      6 | 1715 | `				apA[0] = &sPath;` |
|      6 | 1716 | `				VmSessCallQuiet(pVm,"filemtime",1,apA,&sTime);` |
|      6 | 1717 | `				if( sTime.iFlags & MEMOBJ_INT ){` |
|      6 | 1718 | `					nBuf = PH7_VmHttpDate(sTime.x.iVal,zBuf,(int)sizeof(zBuf));` |
|      6 | 1719 | `					if( nBuf > 0 ){` |
|      6 | 1720 | `						PH7_VmSetResponseHeader(pVm,"Last-Modified",zBuf,(sxu32)nBuf);` |
|      3 | 1721 | `					}` |
|      3 | 1722 | `				}` |
|      6 | 1723 | `				PH7_MemObjRelease(&sTime);` |
|      6 | 1724 | `				PH7_MemObjRelease(&sPath);` |
|      3 | 1725 | `			}` |
|      - | 1726 | `		}` |
|      3 | 1727 | `	}` |
|     12 | 1728 | `	SyBlobRelease(&sLimiter);` |
|     52 | 1729 | `}` |
|      - | 1730 | `/* string\|false session_cache_limiter(?string $value = null) */` |
|     16 | 1731 | `static int vm_builtin_session_cache_limiter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1732 | `{` |
|     17 | 1733 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1734 | `	SyBlob sOld;` |
|     17 | 1735 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|     17 | 1736 | `	PH7_VmIniGetStr(pVm,"session.cache_limiter",&sOld);` |
|     17 | 1737 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|      9 | 1738 | `		int nVal = 0;` |
|      - | 1739 | `		const char *zVal;` |
|      9 | 1740 | `		if( pVm->iSessStatus == VM_SESSION_ACTIVE ){` |
|      - | 1741 | `			/* The headers went out with the session; there is nothing left to` |
|      - | 1742 | `			 * decide. */` |
|      3 | 1743 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1744 | `				"session_cache_limiter(): Session cache limiter cannot be changed"` |
|      - | 1745 | `				" when a session is active");` |
|      3 | 1746 | `			SyBlobRelease(&sOld);` |
|      3 | 1747 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1748 | `			return PH7_OK;` |
|      - | 1749 | `		}` |
|      7 | 1750 | `		zVal = ph7_value_to_string(apArg[0],&nVal);` |
|     10 | 1751 | `		PH7_VmIniSet(pVm,"session.cache_limiter",sizeof("session.cache_limiter")-1,` |
|      3 | 1752 | `			zVal,(sxu32)nVal,"session_cache_limiter()");` |
|      3 | 1753 | `	}` |
|     15 | 1754 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|     15 | 1755 | `	SyBlobRelease(&sOld);` |
|     15 | 1756 | `	return PH7_OK;` |
|      9 | 1757 | `}` |
|      - | 1758 | `/* int\|false session_cache_expire(?int $value = null) */` |
|     12 | 1759 | `static int vm_builtin_session_cache_expire(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1760 | `{` |
|     13 | 1761 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 | 1762 | `	sxi64 iOld = PH7_VmIniGetInt(pVm,"session.cache_expire",180);` |
|     13 | 1763 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|      7 | 1764 | `		if( pVm->iSessStatus == VM_SESSION_ACTIVE ){` |
|      - | 1765 | `			/* php answers the CURRENT value here rather than false, unlike its` |
|      - | 1766 | `			 * neighbour. */` |
|      3 | 1767 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 1768 | `				"session_cache_expire(): Session cache expiration cannot be changed"` |
|      - | 1769 | `				" when a session is active");` |
|      2 | 1770 | `		}else{` |
|      - | 1771 | `			char zVal[32];` |
|      5 | 1772 | `			int nVal = SyBufferFormat(zVal,sizeof(zVal),"%qd",ph7_value_to_int64(apArg[0]));` |
|      7 | 1773 | `			PH7_VmIniSet(pVm,"session.cache_expire",sizeof("session.cache_expire")-1,` |
|      2 | 1774 | `				zVal,(sxu32)nVal,"session_cache_expire()");` |
|      - | 1775 | `		}` |
|      3 | 1776 | `	}` |
|     13 | 1777 | `	ph7_result_int64(pCtx,iOld);` |
|     13 | 1778 | `	return PH7_OK;` |
|      1 | 1779 | `}` |
|      - | 1780 | `/*` |
|      - | 1781 | `` * SessionHandler: php's built-in `files` store, exposed as a class so a program`` |
|      - | 1782 | `` * can DECORATE it -- `class Locking extends SessionHandler { public function`` |
|      - | 1783 | `` * read($id) { …; return parent::read($id); } }`. Its methods are the same C`` |
|      - | 1784 | ` * routines the engine's own store uses, so the two cannot drift.` |
|      - | 1785 | ` */` |
|      4 | 1786 | `static int vm_builtin_SessionHandler_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1787 | `{` |
|      6 | 1788 | `	ph7_vm *pVm = pCtx->pVm;` |
|      6 | 1789 | `	if( nArg > 0 ){` |
|      6 | 1790 | `		int nPath = 0;` |
|      6 | 1791 | `		const char *zPath = ph7_value_to_string(apArg[0],&nPath);` |
|      6 | 1792 | `		if( nPath > 0 ){` |
|      6 | 1793 | `			while( nPath > 0 && zPath[nPath-1] == '/' ){ nPath--; }` |
|      6 | 1794 | `			SyBlobReset(&pVm->sSessPath);` |
|      6 | 1795 | `			SyBlobAppend(&pVm->sSessPath,zPath,(sxu32)nPath);` |
|      2 | 1796 | `		}` |
|      2 | 1797 | `	}` |
|      6 | 1798 | `	ph7_result_bool(pCtx,1);` |
|      6 | 1799 | `	return PH7_OK;` |
|      2 | 1800 | `}` |
|      4 | 1801 | `static int vm_builtin_SessionHandler_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1802 | `{` |
|      2 | 1803 | `	SXUNUSED(nArg);` |
|      2 | 1804 | `	SXUNUSED(apArg);` |
|      6 | 1805 | `	ph7_result_bool(pCtx,1);` |
|      6 | 1806 | `	return PH7_OK;` |
|      2 | 1807 | `}` |
|      - | 1808 | `/* Build "<save_path>/sess_<id>" for an id the CALLER named. */` |
|      8 | 1809 | `static void VmSessFileFor(ph7_vm *pVm,ph7_value *pId,SyBlob *pOut)` |
|      2 | 1810 | `{` |
|     10 | 1811 | `	int nId = 0;` |
|     10 | 1812 | `	const char *zId = pId ? ph7_value_to_string(pId,&nId) : "";` |
|     10 | 1813 | `	VmSessResolvePath(pVm);` |
|     10 | 1814 | `	SyBlobReset(pOut);` |
|     10 | 1815 | `	SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|     10 | 1816 | `	SyBlobAppend(pOut,VM_SESS_FILE_PREFIX,sizeof(VM_SESS_FILE_PREFIX)-1);` |
|     10 | 1817 | `	SyBlobAppend(pOut,zId,(sxu32)nId);` |
|     10 | 1818 | `}` |
|      4 | 1819 | `static int vm_builtin_SessionHandler_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1820 | `{` |
|      6 | 1821 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1822 | `	SyBlob sFile;` |
|      - | 1823 | `	ph7_value sData;` |
|      6 | 1824 | `	if( nArg < 1 ){` |
|    ! 0 | 1825 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1826 | `		return PH7_OK;` |
|      - | 1827 | `	}` |
|      6 | 1828 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|      6 | 1829 | `	VmSessFileFor(pVm,apArg[0],&sFile);` |
|      6 | 1830 | `	PH7_MemObjInit(pVm,&sData);` |
|      6 | 1831 | `	if( VmSessReadFile(pVm,&sFile,&sData) ){` |
|    ! 0 | 1832 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sData.sBlob),` |
|    ! 0 | 1833 | `			(int)SyBlobLength(&sData.sBlob));` |
|    ! 0 | 1834 | `	}else{` |
|      - | 1835 | `		/* php's files handler answers the EMPTY string for a store that is not` |
|      - | 1836 | `		 * there; false is reserved for a read that failed. */` |
|      6 | 1837 | `		ph7_result_string(pCtx,"",0);` |
|      - | 1838 | `	}` |
|      6 | 1839 | `	PH7_MemObjRelease(&sData);` |
|      6 | 1840 | `	SyBlobRelease(&sFile);` |
|      6 | 1841 | `	return PH7_OK;` |
|      4 | 1842 | `}` |
|      4 | 1843 | `static int vm_builtin_SessionHandler_write(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1844 | `{` |
|      6 | 1845 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1846 | `	SyBlob sFile;` |
|      6 | 1847 | `	int nData = 0;` |
|      6 | 1848 | `	const char *zData = "";` |
|      6 | 1849 | `	if( nArg < 1 ){` |
|    ! 0 | 1850 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1851 | `		return PH7_OK;` |
|      - | 1852 | `	}` |
|      6 | 1853 | `	if( nArg > 1 ){` |
|      6 | 1854 | `		zData = ph7_value_to_string(apArg[1],&nData);` |
|      2 | 1855 | `	}` |
|      6 | 1856 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|      6 | 1857 | `	VmSessFileFor(pVm,apArg[0],&sFile);` |
|      6 | 1858 | `	VmSessPutFile(pVm,&sFile,zData,(sxu32)nData);` |
|      6 | 1859 | `	SyBlobRelease(&sFile);` |
|      6 | 1860 | `	ph7_result_bool(pCtx,1);` |
|      6 | 1861 | `	return PH7_OK;` |
|      4 | 1862 | `}` |
|    ! 0 | 1863 | `static int vm_builtin_SessionHandler_destroy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1864 | `{` |
|    ! 0 | 1865 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1866 | `	SyBlob sFile;` |
|    ! 0 | 1867 | `	if( nArg < 1 ){` |
|    ! 0 | 1868 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1869 | `		return PH7_OK;` |
|      - | 1870 | `	}` |
|    ! 0 | 1871 | `	SyBlobInit(&sFile,&pVm->sAllocator);` |
|    ! 0 | 1872 | `	VmSessFileFor(pVm,apArg[0],&sFile);` |
|    ! 0 | 1873 | `	VmSessUnlinkIfExists(pVm,&sFile);` |
|    ! 0 | 1874 | `	SyBlobRelease(&sFile);` |
|    ! 0 | 1875 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 1876 | `	return PH7_OK;` |
|    ! 0 | 1877 | `}` |
|      2 | 1878 | `static int vm_builtin_SessionHandler_gc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1879 | `{` |
|      - | 1880 | `	/* The FILES sweep, never the dispatcher: this IS the files handler, and a` |
|      - | 1881 | `	 * program that decorates it is the user handler the dispatcher would call.` |
|      - | 1882 | `	 * php's signature takes the lifetime, so an explicit one wins over the ini. */` |
|      3 | 1883 | `	sxi64 iMaxLife = nArg > 0` |
|      2 | 1884 | `		? ph7_value_to_int64(apArg[0])` |
|      1 | 1885 | `		: PH7_VmIniGetInt(pCtx->pVm,"session.gc_maxlifetime",1440);` |
|      3 | 1886 | `	ph7_result_int64(pCtx,VmSessGcFiles(pCtx->pVm,iMaxLife));` |
|      3 | 1887 | `	return PH7_OK;` |
|      1 | 1888 | `}` |
|    ! 0 | 1889 | `static int vm_builtin_SessionHandler_create_sid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1890 | `{` |
|      - | 1891 | `	SyBlob sId;` |
|    ! 0 | 1892 | `	SXUNUSED(nArg);` |
|    ! 0 | 1893 | `	SXUNUSED(apArg);` |
|    ! 0 | 1894 | `	SyBlobInit(&sId,&pCtx->pVm->sAllocator);` |
|    ! 0 | 1895 | `	VmSessGenId(pCtx->pVm,&sId);` |
|    ! 0 | 1896 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sId),(int)SyBlobLength(&sId));` |
|    ! 0 | 1897 | `	SyBlobRelease(&sId);` |
|    ! 0 | 1898 | `	return PH7_OK;` |
|    ! 0 | 1899 | `}` |
|      - | 1900 | `/*` |
|      - | 1901 | ` * bool session_set_save_handler(SessionHandlerInterface $handler,` |
|      - | 1902 | ` *     bool $register_shutdown = true)` |
|      - | 1903 | ` * bool session_set_save_handler(callable $open, callable $close, callable $read,` |
|      - | 1904 | ` *     callable $write, callable $destroy, callable $gc,` |
|      - | 1905 | ` *     callable $create_sid = ?, callable $validate_sid = ?,` |
|      - | 1906 | ` *     callable $update_timestamp = ?)` |
|      - | 1907 | ` */` |
|     14 | 1908 | `static int vm_builtin_session_set_save_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1909 | `{` |
|     18 | 1910 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1911 | `	ph7_class *pIface;` |
|      - | 1912 | `	ph7_class_instance *pThis;` |
|     18 | 1913 | `	if( nArg < 1 ){` |
|    ! 0 | 1914 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      - | 1915 | `			"session_set_save_handler() expects at least 1 argument, 0 given");` |
|      - | 1916 | `	}` |
|      - | 1917 | `	/* The handler decides what the store IS, so php will not take one once a` |
|      - | 1918 | `	 * session is open or a header has gone out. */` |
|     18 | 1919 | `	if( VmSessLocked(pCtx,"session_set_save_handler","Session save handler",1) ){` |
|    ! 0 | 1920 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1921 | `		return PH7_OK;` |
|      - | 1922 | `	}` |
|     18 | 1923 | `	pIface = PH7_VmExtractClass(pVm,"SessionHandlerInterface",` |
|      - | 1924 | `		sizeof("SessionHandlerInterface")-1,0,0);` |
|     25 | 1925 | `	pThis = (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|     14 | 1926 | `		? (ph7_class_instance *)apArg[0]->x.pOther : 0;` |
|     18 | 1927 | `	if( pThis == 0 \|\| pIface == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pIface) ){` |
|      - | 1928 | `		/* php ALSO takes six-to-nine callables here and DEPRECATES that spelling` |
|      - | 1929 | `		 * (8.4: "Providing individual callbacks instead of an object implementing` |
|      - | 1930 | `		 * SessionHandlerInterface is deprecated"). The scope policy targets php's` |
|      - | 1931 | `		 * non-deprecated surface, so the callables form is refused rather than` |
|      - | 1932 | `		 * carried — with php's own message for a first argument that is not a` |
|      - | 1933 | `		 * handler, which is what each of those callables is. */` |
|      3 | 1934 | `		const char *zGiven = "";` |
|      3 | 1935 | `		if( pThis && pThis->pClass ){` |
|      3 | 1936 | `			zGiven = pThis->pClass->sName.zString;` |
|      2 | 1937 | `		}else{` |
|    ! 0 | 1938 | `			zGiven = ph7_type_name(apArg[0]);` |
|      - | 1939 | `		}` |
|      4 | 1940 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1941 | `			"session_set_save_handler(): Argument #1 ($open) must be of type"` |
|      1 | 1942 | `			" SessionHandlerInterface, %s given",zGiven);` |
|      - | 1943 | `	}` |
|     16 | 1944 | `	PH7_MemObjRelease(&pVm->sSessHandler);` |
|     16 | 1945 | `	PH7_MemObjStore(apArg[0],&pVm->sSessHandler);` |
|     16 | 1946 | `	pVm->bSessOpened = 0;` |
|     16 | 1947 | `	PH7_VmIniSet(pVm,"session.save_handler",sizeof("session.save_handler")-1,` |
|      - | 1948 | `		"user",sizeof("user")-1,"session_set_save_handler()");` |
|     16 | 1949 | `	ph7_result_bool(pCtx,1);` |
|     16 | 1950 | `	return PH7_OK;` |
|     11 | 1951 | `}` |
|      - | 1952 | `/* string\|false session_module_name(?string $module = null) */` |
|     12 | 1953 | `static int vm_builtin_session_module_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1954 | `{` |
|     14 | 1955 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1956 | `	SyBlob sOld;` |
|     14 | 1957 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|     14 | 1958 | `	PH7_VmIniGetStr(pVm,"session.save_handler",&sOld);` |
|     14 | 1959 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|      3 | 1960 | `		int nVal = 0;` |
|      3 | 1961 | `		const char *zVal = ph7_value_to_string(apArg[0],&nVal);` |
|      3 | 1962 | `		if( VmSessLocked(pCtx,"session_module_name","Session save handler module",1) ){` |
|    ! 0 | 1963 | `			SyBlobRelease(&sOld);` |
|    ! 0 | 1964 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1965 | `			return PH7_OK;` |
|      - | 1966 | `		}` |
|      2 | 1967 | `		if( !(nVal == 5 && SyMemcmp(zVal,"files",5) == 0)` |
|      2 | 1968 | `		 && !(nVal == 4 && SyMemcmp(zVal,"user",4) == 0) ){` |
|      - | 1969 | `			/* php looks the module up in its registered list; this build registers` |
|      - | 1970 | ``			 * the `files` store and the `user` one session_set_save_handler()`` |
|      - | 1971 | ``			 * installs, which is every module a CLI-plus-`-S` engine has. */`` |
|      - | 1972 | `			char zMsg[160];` |
|    ! 0 | 1973 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 1974 | `				"session_module_name(): Argument #1 ($module) must be a valid"` |
|      - | 1975 | `				" session handler");` |
|    ! 0 | 1976 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,zMsg);` |
|    ! 0 | 1977 | `			SyBlobRelease(&sOld);` |
|    ! 0 | 1978 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1979 | `			return PH7_OK;` |
|      - | 1980 | `		}` |
|      3 | 1981 | `		if( nVal == 5 ){` |
|      - | 1982 | `			/* Back to the built-in store: the userland handler is dropped. */` |
|      3 | 1983 | `			PH7_MemObjRelease(&pVm->sSessHandler);` |
|      3 | 1984 | `			pVm->bSessOpened = 0;` |
|      1 | 1985 | `		}` |
|      4 | 1986 | `		PH7_VmIniSet(pVm,"session.save_handler",sizeof("session.save_handler")-1,` |
|      1 | 1987 | `			zVal,(sxu32)nVal,"session_module_name()");` |
|      1 | 1988 | `	}` |
|     14 | 1989 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|     14 | 1990 | `	SyBlobRelease(&sOld);` |
|     14 | 1991 | `	return PH7_OK;` |
|      8 | 1992 | `}` |
|      - | 1993 | `/* int\|false session_gc() */` |
|      - | 1994 |  |
|      8 | 1995 | `static int vm_builtin_session_gc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1996 | `{` |
|     10 | 1997 | `	ph7_vm *pVm = pCtx->pVm;` |
|      4 | 1998 | `	SXUNUSED(nArg);` |
|      4 | 1999 | `	SXUNUSED(apArg);` |
|     10 | 2000 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      - | 2001 | `		/* The collector is the STORE's, and php only reaches a store through an` |
|      - | 2002 | `		 * open session. */` |
|      3 | 2003 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 2004 | `			"session_gc(): Session cannot be garbage collected when there is no active session");` |
|      3 | 2005 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2006 | `		return PH7_OK;` |
|      - | 2007 | `	}` |
|      8 | 2008 | `	ph7_result_int64(pCtx,VmSessGc(pVm));` |
|      8 | 2009 | `	return PH7_OK;` |
|      6 | 2010 | `}` |
|      - | 2011 | `/*` |
|      - | 2012 | ` * void session_register_shutdown()` |
|      - | 2013 | ` *` |
|      - | 2014 | ` * php's own escape hatch for a script that installs a shutdown function which` |
|      - | 2015 | ` * calls exit(): the remaining callbacks are skipped, so php re-registers the` |
|      - | 2016 | ` * session writer as a callback of its OWN to make sure the session is still` |
|      - | 2017 | ` * written. The writer here runs from the VM's request shutdown, past every` |
|      - | 2018 | ` * callback and past a halt, so there is nothing left for this to arrange.` |
|      - | 2019 | ` */` |
|    ! 0 | 2020 | `static int vm_builtin_session_register_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2021 | `{` |
|    ! 0 | 2022 | `	SXUNUSED(nArg);` |
|    ! 0 | 2023 | `	SXUNUSED(apArg);` |
|    ! 0 | 2024 | `	ph7_result_null(pCtx);` |
|    ! 0 | 2025 | `	return PH7_OK;` |
|    ! 0 | 2026 | `}` |
|      - | 2027 | `/* string\|false session_create_id(string $prefix = "") */` |
|     14 | 2028 | `static int vm_builtin_session_create_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2029 | `{` |
|     15 | 2030 | `	ph7_vm *pVm = pCtx->pVm;` |
|     15 | 2031 | `	const char *zPfx = "";` |
|     15 | 2032 | `	int nPfx = 0;` |
|      - | 2033 | `	SyBlob sId;` |
|     15 | 2034 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     13 | 2035 | `		zPfx = ph7_value_to_string(apArg[0],&nPfx);` |
|      6 | 2036 | `	}` |
|     15 | 2037 | `	if( nPfx > VM_SESS_MAX_ID ){` |
|      3 | 2038 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2039 | `			"session_create_id(): Argument #1 ($prefix) cannot be longer than %d characters",` |
|      - | 2040 | `			VM_SESS_MAX_ID);` |
|      - | 2041 | `	}` |
|     13 | 2042 | `	if( nPfx > 0 && !VmSessIdValid(zPfx,(sxu32)nPfx) ){` |
|      - | 2043 | `		/* The prefix becomes the front of an id, so it lives under the id's own` |
|      - | 2044 | `		 * alphabet — php reports it and answers false rather than making one it` |
|      - | 2045 | `		 * would then refuse to start. */` |
|      7 | 2046 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 2047 | `			"session_create_id(): Prefix cannot contain special characters."` |
|      - | 2048 | `			" Only the A-Z, a-z, 0-9, \"-\", and \",\" characters are allowed");` |
|      7 | 2049 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2050 | `		return PH7_OK;` |
|      - | 2051 | `	}` |
|      7 | 2052 | `	SyBlobInit(&sId,&pVm->sAllocator);` |
|      7 | 2053 | `	VmSessGenId(pVm,&sId);` |
|      7 | 2054 | `	ph7_result_string(pCtx,zPfx,nPfx);` |
|      7 | 2055 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sId),(int)SyBlobLength(&sId));` |
|      7 | 2056 | `	SyBlobRelease(&sId);` |
|      7 | 2057 | `	return PH7_OK;` |
|      8 | 2058 | `}` |
|      - | 2059 | `/* string\|false session_encode() */` |
|     10 | 2060 | `static int vm_builtin_session_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2061 | `{` |
|     11 | 2062 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 2063 | `	SyBlob sData;` |
|      5 | 2064 | `	SXUNUSED(nArg);` |
|      5 | 2065 | `	SXUNUSED(apArg);` |
|     11 | 2066 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|    ! 0 | 2067 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 2068 | `			"session_encode(): Cannot encode non-existent session");` |
|    ! 0 | 2069 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2070 | `		return PH7_OK;` |
|      - | 2071 | `	}` |
|     11 | 2072 | `	SyBlobInit(&sData,&pVm->sAllocator);` |
|     11 | 2073 | `	if( VmSessEncode(pVm,&sData,"session_encode()") ){` |
|      9 | 2074 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sData),(int)SyBlobLength(&sData));` |
|      5 | 2075 | `	}else{` |
|      3 | 2076 | `		ph7_result_bool(pCtx,0);` |
|      - | 2077 | `	}` |
|     11 | 2078 | `	SyBlobRelease(&sData);` |
|     11 | 2079 | `	return PH7_OK;` |
|      6 | 2080 | `}` |
|      - | 2081 | `/* bool session_decode(string $data) */` |
|     10 | 2082 | `static int vm_builtin_session_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2083 | `{` |
|     11 | 2084 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 2085 | `	ph7_value *pSess;` |
|      - | 2086 | `	const char *zData;` |
|     11 | 2087 | `	int nData = 0,iDec;` |
|     11 | 2088 | `	if( pVm->iSessStatus != VM_SESSION_ACTIVE ){` |
|      3 | 2089 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - | 2090 | `			"session_decode(): Session data cannot be decoded when there is no active session");` |
|      3 | 2091 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2092 | `		return PH7_OK;` |
|      - | 2093 | `	}` |
|      9 | 2094 | `	if( nArg < 1 ){` |
|    ! 0 | 2095 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2096 | `		return PH7_OK;` |
|      - | 2097 | `	}` |
|      9 | 2098 | `	pSess = VmSessArray(pVm);` |
|      9 | 2099 | `	if( pSess == 0 ){` |
|    ! 0 | 2100 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2101 | `		return PH7_OK;` |
|      - | 2102 | `	}` |
|      9 | 2103 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|      9 | 2104 | `	VmSessNormalizeVars(pVm,"session_decode()");` |
|      - | 2105 | `	/* Unlike session_start(), this one decodes OVER whatever $_SESSION already` |
|      - | 2106 | `	 * holds — for the two keyed handlers; php_serialize replaces the variable. */` |
|      9 | 2107 | `	iDec = VmSessDecodeInto(pCtx,zData,(sxu32)nData,pSess);` |
|      9 | 2108 | `	if( iDec < 0 ){` |
|    ! 0 | 2109 | `		return PH7_EXCEPTION;` |
|      - | 2110 | `	}` |
|      9 | 2111 | `	if( iDec == 0 ){` |
|      - | 2112 | `		SyBlob sFile;` |
|      3 | 2113 | `		SyBlobInit(&sFile,&pVm->sAllocator);` |
|      3 | 2114 | `		VmSessFile(pVm,&sFile);` |
|      3 | 2115 | `		VmSessDestroyBadStore(pVm,&sFile,"session_decode");` |
|      3 | 2116 | `		SyBlobRelease(&sFile);` |
|      3 | 2117 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2118 | `		return PH7_OK;` |
|      - | 2119 | `	}` |
|      7 | 2120 | `	ph7_result_bool(pCtx,1);` |
|      7 | 2121 | `	return PH7_OK;` |
|      6 | 2122 | `}` |
|      - | 2123 | `/*` |
|      - | 2124 | ` * php's three save-handler interfaces, and the class that implements the first` |
|      - | 2125 | `` * two over the built-in `files` store.`` |
|      - | 2126 | ` *` |
|      - | 2127 | ` * Every method here declares a return type, and every one of them is php's` |
|      - | 2128 | `` * TENTATIVE kind (the leading `@`) -- which is what lets a handler written`` |
|      - | 2129 | ` * before php 8.1 keep answering whatever its store answers. The rows used to` |
|      - | 2130 | ` * declare none at all, on the belief that php's stubs declare none; php's` |
|      - | 2131 | ` * stubs declare all sixteen, and a return-type sweep of both engines reports` |
|      - | 2132 | `` * every one. `read` is the pair's odd one (`string\|false`, not `string`), and`` |
|      - | 2133 | `` * `gc` answers php's `int\|false` -- the count of records it removed.`` |
|      - | 2134 | ` */` |
|   8445 | 2135 | `static sxi32 VmSessInstallClasses(ph7_vm *pVm)` |
|      5 | 2136 | `{` |
|      - | 2137 | `	static const PH7_NativeMethodDef aIface[] = {` |
|      - | 2138 | `		{ "open",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $path, string $name", "@bool", 0 },` |
|      - | 2139 | `		{ "close",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|      - | 2140 | `		{ "read",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $id", "@string\|false", 0 },` |
|      - | 2141 | `		{ "write",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $id, string $data", "@bool", 0 },` |
|      - | 2142 | `		{ "destroy", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $id", "@bool", 0 },` |
|      - | 2143 | `		{ "gc",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "int $max_lifetime", "@int\|false", 0 },` |
|      - | 2144 | `	};` |
|      - | 2145 | `	static const PH7_NativeMethodDef aIdIface[] = {` |
|      - | 2146 | `		{ "create_sid", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@string", 0 },` |
|      - | 2147 | `	};` |
|      - | 2148 | `	static const PH7_NativeMethodDef aStampIface[] = {` |
|      - | 2149 | `		{ "validateId",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $id", "@bool", 0 },` |
|      - | 2150 | `		{ "updateTimestamp", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $id, string $data", "@bool", 0 },` |
|      - | 2151 | `	};` |
|      - | 2152 | `	static const PH7_NativeMethodDef aHandler[] = {` |
|      - | 2153 | `		{ "open",    PH7_MOD_PUBLIC, "string $path, string $name", "@bool",` |
|      - | 2154 | `		  vm_builtin_SessionHandler_open },` |
|      - | 2155 | `		{ "close",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SessionHandler_close },` |
|      - | 2156 | `		{ "read",    PH7_MOD_PUBLIC, "string $id", "@string\|false", vm_builtin_SessionHandler_read },` |
|      - | 2157 | `		{ "write",   PH7_MOD_PUBLIC, "string $id, string $data", "@bool",` |
|      - | 2158 | `		  vm_builtin_SessionHandler_write },` |
|      - | 2159 | `		{ "destroy", PH7_MOD_PUBLIC, "string $id", "@bool", vm_builtin_SessionHandler_destroy },` |
|      - | 2160 | `		{ "gc",      PH7_MOD_PUBLIC, "int $max_lifetime", "@int\|false", vm_builtin_SessionHandler_gc },` |
|      - | 2161 | `		{ "create_sid", PH7_MOD_PUBLIC, "", "@string", vm_builtin_SessionHandler_create_sid },` |
|      - | 2162 | `	};` |
|      - | 2163 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 2164 | `		{ "SessionHandlerInterface", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 2165 | `		  aIface, SX_ARRAYSIZE(aIface), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 2166 | `		{ "SessionIdInterface", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 2167 | `		  aIdIface, SX_ARRAYSIZE(aIdIface), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 2168 | `		{ "SessionUpdateTimestampHandlerInterface", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 2169 | `		  aStampIface, SX_ARRAYSIZE(aStampIface), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 2170 | `		{ "SessionHandler", 0, "SessionHandlerInterface,SessionIdInterface", 0,` |
|      - | 2171 | `		  aHandler, SX_ARRAYSIZE(aHandler), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 2172 | `	};` |
|   8450 | 2173 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|      5 | 2174 | `}` |
|   8445 | 2175 | `PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm)` |
|      5 | 2176 | `{` |
|      - | 2177 | `	static const struct {` |
|      - | 2178 | `		const char *zName;` |
|      - | 2179 | `		ProchHostFunction xFunc;` |
|      - | 2180 | `	} aFunc[] = {` |
|      - | 2181 | `		{ "session_status",        vm_builtin_session_status        },` |
|      - | 2182 | `		{ "session_id",            vm_builtin_session_id            },` |
|      - | 2183 | `		{ "session_name",          vm_builtin_session_name          },` |
|      - | 2184 | `		{ "session_save_path",     vm_builtin_session_save_path     },` |
|      - | 2185 | `		{ "session_start",         vm_builtin_session_start         },` |
|      - | 2186 | `		{ "session_write_close",   vm_builtin_session_write_close   },` |
|      - | 2187 | `		{ "session_commit",        vm_builtin_session_write_close   },` |
|      - | 2188 | `		{ "session_abort",         vm_builtin_session_abort         },` |
|      - | 2189 | `		{ "session_reset",         vm_builtin_session_reset         },` |
|      - | 2190 | `		{ "session_unset",         vm_builtin_session_unset         },` |
|      - | 2191 | `		{ "session_destroy",       vm_builtin_session_destroy       },` |
|      - | 2192 | `		{ "session_regenerate_id", vm_builtin_session_regenerate_id },` |
|      - | 2193 | `		{ "session_encode",        vm_builtin_session_encode        },` |
|      - | 2194 | `		{ "session_decode",        vm_builtin_session_decode        },` |
|      - | 2195 | `		{ "session_create_id",     vm_builtin_session_create_id     },` |
|      - | 2196 | `		{ "session_gc",            vm_builtin_session_gc            },` |
|      - | 2197 | `		{ "session_module_name",   vm_builtin_session_module_name   },` |
|      - | 2198 | `		{ "session_set_save_handler", vm_builtin_session_set_save_handler },` |
|      - | 2199 | `		{ "session_cache_limiter", vm_builtin_session_cache_limiter },` |
|      - | 2200 | `		{ "session_cache_expire",  vm_builtin_session_cache_expire  },` |
|      - | 2201 | `		{ "session_register_shutdown", vm_builtin_session_register_shutdown },` |
|      - | 2202 | `		{ "session_get_cookie_params", vm_builtin_session_get_cookie_params },` |
|      - | 2203 | `		{ "session_set_cookie_params", vm_builtin_session_set_cookie_params },` |
|      - | 2204 | `	};` |
|      - | 2205 | `	sxu32 n;` |
| 202685 | 2206 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 194240 | 2207 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  96996 | 2208 | `	}` |
|   8450 | 2209 | `	return VmSessInstallClasses(pVm);` |
|      5 | 2210 | `}` |
|      - | 2211 |  |
|      - | 2212 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 2213 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2214 |  |
|      - | 2215 | `#if defined(PH7_DISABLE_BUILTIN_FUNC) \|\| defined(PH7_DISABLE_DISK_IO)` |
|      - | 2216 | `/* Tiny build: no sessions (builtin funcs / disk IO disabled) */` |
|      - | 2217 | `PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|      - | 2218 | `PH7_PRIVATE void PH7_VmSessionShutdown(ph7_vm *pVm){ (void)pVm; }` |
|      - | 2219 | `#endif` |
|      - | 2220 |  |
