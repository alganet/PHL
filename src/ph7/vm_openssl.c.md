# src/ph7/vm_openssl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1786/2219 lines (80.49%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#ifdef PH7_ENABLE_OPENSSL` |
|      - |    6 | `#include "openssl_int.h"` |
|      - |    7 |  |
|      - |    8 | `/*` |
|      - |    9 | ` * Section:` |
|      - |   10 | ` *    ext/openssl -- php's binding of OpenSSL. This unit carries the` |
|      - |   11 | ` *    library-wide surface (the version, the error ring, the algorithm` |
|      - |   12 | ` *    listings), the DIGEST and CIPHER halves, and the random/derivation` |
|      - |   13 | ` *    helpers. The certificate containers live in vm_openssl_x509.c.` |
|      - |   14 | ` * Status:` |
|      - |   15 | ` *    In progress.` |
|      - |   16 | ` *` |
|      - |   17 | ` * WHY A BINDING AND NOT A REIMPLEMENTATION. Same call as ext/zlib and` |
|      - |   18 | ` * ext/curl, and for the sharpest version of the reason those two record: the` |
|      - |   19 | ` * bytes a program depends on here are a SPECIFIC library's. AES-256-GCM has` |
|      - |   20 | ` * one answer, but which cipher NAMES exist, what an unknown one says, which` |
|      - |   21 | ` * of them are AEAD, what a decode failure's error string reads and what` |
|      - |   22 | ` * OPENSSL_VERSION_TEXT is are all the linked OpenSSL's, and a program that` |
|      - |   23 | ` * encrypts under php and decrypts under PHL needs exactly them. So every` |
|      - |   24 | ` * answer below is derived by asking php 8.5 and OpenSSL 3.0 the same` |
|      - |   25 | ` * question, never by reading php's source -- see the per-function notes for` |
|      - |   26 | ` * the ones a careful reading would have got wrong.` |
|      - |   27 | ` *` |
|      - |   28 | ` * TWO CRYPTO STACKS. This is not the TLS ext/curl uses: libcurl carries its` |
|      - |   29 | ` * own backend (OpenSSL on Linux, Schannel through vcpkg on Windows), so on` |
|      - |   30 | ` * Windows the binary holds two implementations that share nothing. php's own` |
|      - |   31 | `` * Windows build has exactly the same split, which is why `curl_version()`'s`` |
|      - |   32 | ` * ssl_version and OPENSSL_VERSION_TEXT are two different questions there and` |
|      - |   33 | ` * Composer's platform repository asks both.` |
|      - |   34 | ` *` |
|      - |   35 | ` * MEMORY MODEL. OpenSSL stays on its own (system) allocator, like libcurl,` |
|      - |   36 | ` * libxml2 and sqlite3 before it: routing it through SyMemBackend would subject` |
|      - |   37 | ` * library internals to the PHL_MAX_ALLOC fault injection the stress tier uses,` |
|      - |   38 | ` * and a half-initialized EVP context is not something OpenSSL unwinds.` |
|      - |   39 | ` *` |
|      - |   40 | ` * WHAT IS VERSION-DEPENDENT, AND THEREFORE NOT PINNED. The algorithm listings` |
|      - |   41 | `` * (`openssl_get_md_methods`, `openssl_get_cipher_methods`,`` |
|      - |   42 | `` * `openssl_get_curve_names`), OPENSSL_VERSION_TEXT/NUMBER and the error`` |
|      - |   43 | ` * strings all move with the linked library -- this box's 3.0.13 answers 21 and` |
|      - |   44 | ` * 124 where the Windows guest's 3.6.3 answers something else. The corpus` |
|      - |   45 | ` * therefore pins round trips, refusals and the shape of a listing, never its` |
|      - |   46 | `` * length. Same discipline the `libxml-version-answers-differ` note records for`` |
|      - |   47 | ` * ext/dom.` |
|      - |   48 | ` */` |
|      - |   49 |  |
|      - |   50 | `/* ------------------------------------------------------------------------` |
|      - |   51 | ` * php's error ring` |
|      - |   52 | ` * ------------------------------------------------------------------------ */` |
|      - |   53 | `/*` |
|      - |   54 | ` * Drain OpenSSL's thread error queue into the VM's ring. php does this after` |
|      - |   55 | ` * an operation fails rather than handing the queue over, and the ring is what` |
|      - |   56 | ` * openssl_error_string() reads. A full ring drops its OLDEST entry, which is` |
|      - |   57 | ` * why twenty-five failures answer the last fifteen.` |
|      - |   58 | ` */` |
|    552 |   59 | `PH7_PRIVATE void PH7_SslStoreErrors(ph7_vm *pVm)` |
|      4 |   60 | `{` |
|    556 |   61 | `	phl_ssl_errors *pRing = (phl_ssl_errors *)pVm->pSslErrors;` |
|      - |   62 | `	unsigned long iErr;` |
|    556 |   63 | `	if( pRing == 0 ){` |
|     14 |   64 | `		pRing = (phl_ssl_errors *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_ssl_errors));` |
|     14 |   65 | `		if( pRing == 0 ){` |
|    ! 0 |   66 | `			ERR_clear_error();` |
|    ! 0 |   67 | `			return;` |
|      - |   68 | `		}` |
|     14 |   69 | `		SyZero(pRing,sizeof(phl_ssl_errors));` |
|     14 |   70 | `		pVm->pSslErrors = pRing;` |
|      5 |   71 | `	}` |
|   1107 |   72 | `	while( (iErr = ERR_get_error()) != 0 ){` |
|    281 |   73 | `		pRing->iTop = (pRing->iTop + 1) % PHL_SSL_ERR_RING;` |
|    281 |   74 | `		pRing->aErr[pRing->iTop] = (int)iErr;` |
|    281 |   75 | `		if( pRing->iTop == pRing->iBottom ){` |
|      9 |   76 | `			pRing->iBottom = (pRing->iBottom + 1) % PHL_SSL_ERR_RING;` |
|      4 |   77 | `		}` |
|      4 |   78 | `	}` |
|    282 |   79 | `}` |
|      - |   80 | `/*` |
|      - |   81 | ` * openssl_error_string(): one entry per call, oldest first, false when the` |
|      - |   82 | ` * ring is empty. The drain happens HERE too, not only on the failure paths --` |
|      - |   83 | `` * `openssl_digest($s,'nope')` is a refusal php screens by name and never`` |
|      - |   84 | `` * stores for, and its `error:0308010C:...` still comes back.`` |
|      - |   85 | ` */` |
|    368 |   86 | `static int vm_builtin_openssl_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |   87 | `{` |
|    372 |   88 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |   89 | `	phl_ssl_errors *pRing;` |
|      - |   90 | `	unsigned long iErr;` |
|      - |   91 | `	char zBuf[256];` |
|    186 |   92 | `	SXUNUSED(nArg);` |
|    186 |   93 | `	SXUNUSED(apArg);` |
|    372 |   94 | `	PH7_SslStoreErrors(pVm);` |
|    372 |   95 | `	pRing = (phl_ssl_errors *)pVm->pSslErrors;` |
|    372 |   96 | `	if( pRing == 0 \|\| pRing->iTop == pRing->iBottom ){` |
|    118 |   97 | `		ph7_result_bool(pCtx,0);` |
|    118 |   98 | `		return PH7_OK;` |
|      - |   99 | `	}` |
|    258 |  100 | `	pRing->iBottom = (pRing->iBottom + 1) % PHL_SSL_ERR_RING;` |
|    258 |  101 | `	iErr = (unsigned long)(long)pRing->aErr[pRing->iBottom];` |
|    258 |  102 | `	if( pRing->aErr[pRing->iBottom] == 0 ){` |
|    ! 0 |  103 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  104 | `		return PH7_OK;` |
|      - |  105 | `	}` |
|    258 |  106 | `	ERR_error_string_n(iErr,zBuf,sizeof(zBuf));` |
|    258 |  107 | `	ph7_result_string(pCtx,zBuf,-1);` |
|    258 |  108 | `	return PH7_OK;` |
|    190 |  109 | `}` |
|      - |  110 |  |
|      - |  111 | `/* ------------------------------------------------------------------------` |
|      - |  112 | ` * The handle records` |
|      - |  113 | ` * ------------------------------------------------------------------------ */` |
|      - |  114 | `/*` |
|      - |  115 | ` * The three handle classes keep their library pointer in the same hidden` |
|      - |  116 | `` * `__res` slot ext/curl's do, and for the same reason: a PH7 resource carries`` |
|      - |  117 | ` * no destructor, so the record is ALSO chained on a per-VM registry and the` |
|      - |  118 | ` * sweep at reset/release is what frees a handle a script dropped without` |
|      - |  119 | ` * unsetting.` |
|      - |  120 | ` */` |
|    ! 0 |  121 | `static void SslBlankSlot(ph7_class_instance *pOwner)` |
|    ! 0 |  122 | `{` |
|      - |  123 | `	SyString sAttr;` |
|      - |  124 | `	ph7_value *pRes;` |
|    ! 0 |  125 | `	if( pOwner == 0 ){` |
|    ! 0 |  126 | `		return;` |
|      - |  127 | `	}` |
|    ! 0 |  128 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|    ! 0 |  129 | `	pRes = PH7_ClassInstanceFetchAttr(pOwner,&sAttr);` |
|    ! 0 |  130 | `	if( pRes ){` |
|    ! 0 |  131 | `		PH7_MemObjRelease(pRes);` |
|    ! 0 |  132 | `		MemObjSetType(pRes,MEMOBJ_NULL);` |
|    ! 0 |  133 | `	}` |
|    ! 0 |  134 | `}` |
|    374 |  135 | `static phl_ssl_obj * SslRecOfInstance(ph7_class_instance *pThis)` |
|      2 |  136 | `{` |
|      - |  137 | `	SyString sAttr;` |
|      - |  138 | `	ph7_value *pRes;` |
|    376 |  139 | `	if( pThis == 0 ){` |
|    ! 0 |  140 | `		return 0;` |
|      - |  141 | `	}` |
|    376 |  142 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|    376 |  143 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    376 |  144 | `	if( pRes == 0 \|\| !ph7_value_is_resource(pRes) ){` |
|    ! 0 |  145 | `		return 0;` |
|      - |  146 | `	}` |
|    376 |  147 | `	return (phl_ssl_obj *)pRes->x.pOther;` |
|    189 |  148 | `}` |
|    102 |  149 | `static int SslAttachSlot(ph7_class_instance *pThis,phl_ssl_obj *pRec)` |
|      2 |  150 | `{` |
|      - |  151 | `	SyString sAttr;` |
|      - |  152 | `	ph7_value *pRes;` |
|    104 |  153 | `	if( pThis == 0 ){` |
|    ! 0 |  154 | `		return -1;` |
|      - |  155 | `	}` |
|    104 |  156 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|    104 |  157 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    104 |  158 | `	if( pRes == 0 ){` |
|    ! 0 |  159 | `		return -1;` |
|      - |  160 | `	}` |
|    104 |  161 | `	PH7_MemObjRelease(pRes);` |
|    104 |  162 | `	pRes->x.pOther = (void *)pRec;` |
|    104 |  163 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|    104 |  164 | `	return 0;` |
|     53 |  165 | `}` |
|      - |  166 | `/* Free the library object a record owns. The record itself lives in the VM's` |
|      - |  167 | ` * allocator and is released with it. */` |
|    196 |  168 | `PH7_PRIVATE void PH7_SslFreeObject(phl_ssl_obj *pObj)` |
|      2 |  169 | `{` |
|    198 |  170 | `	if( pObj == 0 \|\| pObj->pHandle == 0 ){` |
|     96 |  171 | `		return;` |
|      - |  172 | `	}` |
|    104 |  173 | `	switch( pObj->iKind ){` |
|     23 |  174 | `		case PHL_SSL_KIND_CERT: X509_free((X509 *)pObj->pHandle);         break;` |
|     15 |  175 | `		case PHL_SSL_KIND_CSR:  X509_REQ_free((X509_REQ *)pObj->pHandle); break;` |
|     68 |  176 | `		case PHL_SSL_KIND_KEY:  EVP_PKEY_free((EVP_PKEY *)pObj->pHandle); break;` |
|    ! 0 |  177 | `		default: break;` |
|      - |  178 | `	}` |
|    104 |  179 | `	pObj->pHandle = 0;` |
|    100 |  180 | `}` |
|      - |  181 | `/*` |
|      - |  182 | ` * The object is going away: free its library handle now rather than at VM` |
|      - |  183 | ` * reset, so a script that drops its last reference releases the key there.` |
|      - |  184 | ` * The shell stays on the registry (the sweep frees it) because the slot is` |
|      - |  185 | ` * still reachable while the instance is being torn down.` |
|      - |  186 | ` */` |
|     94 |  187 | `static void SslInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      2 |  188 | `{` |
|     96 |  189 | `	phl_ssl_obj *pObj = SslRecOfInstance(pThis);` |
|     47 |  190 | `	SXUNUSED(pVm);` |
|     96 |  191 | `	if( pObj == 0 \|\| pObj->pOwner != pThis ){` |
|    ! 0 |  192 | `		return;` |
|      - |  193 | `	}` |
|     96 |  194 | `	PH7_SslFreeObject(pObj);` |
|     96 |  195 | `	pObj->pOwner = 0;` |
|     49 |  196 | `}` |
|      - |  197 | `/*` |
|      - |  198 | ` * Build one handle object: the instance, its record and the registry link.` |
|      - |  199 | ` * Answers the record; *ppInst is the instance the caller hands back.` |
|      - |  200 | ` */` |
|    102 |  201 | `PH7_PRIVATE phl_ssl_obj * PH7_SslNewObject(ph7_vm *pVm,int iKind,void *pHandle,` |
|      - |  202 | `	ph7_class_instance **ppInst)` |
|      2 |  203 | `{` |
|      - |  204 | `	static const char * const azClass[] = {` |
|      - |  205 | `		PHL_SSL_CLASS_CERT, PHL_SSL_CLASS_CSR, PHL_SSL_CLASS_KEY` |
|      - |  206 | `	};` |
|      - |  207 | `	ph7_class_instance *pInst;` |
|      - |  208 | `	ph7_class *pClass;` |
|      - |  209 | `	phl_ssl_obj *pObj;` |
|      - |  210 | `	const char *zClass;` |
|    104 |  211 | `	if( iKind < 0 \|\| iKind > PHL_SSL_KIND_KEY ){` |
|    ! 0 |  212 | `		return 0;` |
|      - |  213 | `	}` |
|    104 |  214 | `	zClass = azClass[iKind];` |
|    104 |  215 | `	pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|    104 |  216 | `	if( pClass == 0 ){` |
|    ! 0 |  217 | `		return 0;` |
|      - |  218 | `	}` |
|    104 |  219 | `	pInst = PH7_NewClassInstance(pVm,pClass);` |
|    104 |  220 | `	if( pInst == 0 ){` |
|    ! 0 |  221 | `		return 0;` |
|      - |  222 | `	}` |
|    104 |  223 | `	pObj = (phl_ssl_obj *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_ssl_obj));` |
|    104 |  224 | `	if( pObj == 0 ){` |
|    ! 0 |  225 | `		return 0;` |
|      - |  226 | `	}` |
|    104 |  227 | `	SyZero(pObj,sizeof(phl_ssl_obj));` |
|    104 |  228 | `	pObj->iKind = iKind;` |
|    104 |  229 | `	pObj->pHandle = pHandle;` |
|    104 |  230 | `	pObj->pOwner = pInst;` |
|    104 |  231 | `	pObj->pNext = (phl_ssl_obj *)pVm->pSslObjs;` |
|    104 |  232 | `	pVm->pSslObjs = pObj;` |
|    104 |  233 | `	if( SslAttachSlot(pInst,pObj) != 0 ){` |
|    ! 0 |  234 | `		SslBlankSlot(pInst);` |
|    ! 0 |  235 | `		pObj->pOwner = 0;` |
|    ! 0 |  236 | `		pObj->pHandle = 0;` |
|    ! 0 |  237 | `		return 0;` |
|      - |  238 | `	}` |
|    104 |  239 | `	if( ppInst ){` |
|    104 |  240 | `		*ppInst = pInst;` |
|     51 |  241 | `	}` |
|    104 |  242 | `	return pObj;` |
|     53 |  243 | `}` |
|      - |  244 | `/* The library pointer behind a value that IS one of the three handle classes,` |
|      - |  245 | ` * or 0 for anything else -- including a handle whose owner has been closed. */` |
|    280 |  246 | `PH7_PRIVATE void * PH7_SslHandleOf(ph7_value *pVal,int iKind)` |
|      2 |  247 | `{` |
|      - |  248 | `	phl_ssl_obj *pObj;` |
|    282 |  249 | `	if( pVal == 0 \|\| !ph7_value_is_object(pVal) ){` |
|    ! 0 |  250 | `		return 0;` |
|      - |  251 | `	}` |
|    282 |  252 | `	pObj = SslRecOfInstance((ph7_class_instance *)pVal->x.pOther);` |
|    282 |  253 | `	if( pObj == 0 \|\| pObj->iKind != iKind ){` |
|      5 |  254 | `		return 0;` |
|      - |  255 | `	}` |
|    278 |  256 | `	return pObj->pHandle;` |
|    142 |  257 | `}` |
|      - |  258 | `/* Hand a freshly created library object back as this call's return value. The` |
|      - |  259 | ` * handle is CONSUMED: on failure it is freed here rather than leaked. */` |
|     96 |  260 | `PH7_PRIVATE int PH7_SslResultObject(ph7_context *pCtx,int iKind,void *pHandle)` |
|      2 |  261 | `{` |
|     98 |  262 | `	ph7_class_instance *pInst = 0;` |
|      - |  263 | `	phl_ssl_obj *pObj;` |
|     98 |  264 | `	if( pHandle == 0 ){` |
|    ! 0 |  265 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  266 | `		return PH7_OK;` |
|      - |  267 | `	}` |
|     98 |  268 | `	pObj = PH7_SslNewObject(pCtx->pVm,iKind,pHandle,&pInst);` |
|     98 |  269 | `	if( pObj == 0 \|\| pInst == 0 ){` |
|    ! 0 |  270 | `		switch( iKind ){` |
|    ! 0 |  271 | `			case PHL_SSL_KIND_CERT: X509_free((X509 *)pHandle);         break;` |
|    ! 0 |  272 | `			case PHL_SSL_KIND_CSR:  X509_REQ_free((X509_REQ *)pHandle); break;` |
|    ! 0 |  273 | `			case PHL_SSL_KIND_KEY:  EVP_PKEY_free((EVP_PKEY *)pHandle); break;` |
|    ! 0 |  274 | `			default: break;` |
|      - |  275 | `		}` |
|    ! 0 |  276 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  277 | `		return PH7_OK;` |
|      - |  278 | `	}` |
|     98 |  279 | `	PH7_NativeResultObject(pCtx,pInst);` |
|     98 |  280 | `	return PH7_OK;` |
|     50 |  281 | `}` |
|      - |  282 | `/* Free every registered handle. Called from the VM reset (a reused VM must not` |
|      - |  283 | ` * inherit the previous program's keys) and from the release before the` |
|      - |  284 | ` * allocator holding the records goes away. */` |
|   7011 |  285 | `static void SslFreeAllObjects(ph7_vm *pVm)` |
|      5 |  286 | `{` |
|   7016 |  287 | `	phl_ssl_obj *pObj = (phl_ssl_obj *)pVm->pSslObjs;` |
|   7118 |  288 | `	while( pObj ){` |
|    104 |  289 | `		phl_ssl_obj *pNext = pObj->pNext;` |
|    104 |  290 | `		PH7_SslFreeObject(pObj);` |
|    104 |  291 | `		SyMemBackendFree(&pVm->sAllocator,pObj);` |
|    104 |  292 | `		pObj = pNext;` |
|      2 |  293 | `	}` |
|   7016 |  294 | `	pVm->pSslObjs = 0;` |
|   7016 |  295 | `}` |
|     16 |  296 | `PH7_PRIVATE void PH7_SslVmReset(ph7_vm *pVm)` |
|    ! 0 |  297 | `{` |
|     16 |  298 | `	SslFreeAllObjects(pVm);` |
|     16 |  299 | `	if( pVm->pSslErrors ){` |
|    ! 0 |  300 | `		SyZero(pVm->pSslErrors,sizeof(phl_ssl_errors));` |
|    ! 0 |  301 | `	}` |
|     16 |  302 | `	ERR_clear_error();` |
|     16 |  303 | `}` |
|   6995 |  304 | `PH7_PRIVATE void PH7_SslVmRelease(ph7_vm *pVm)` |
|      5 |  305 | `{` |
|   7000 |  306 | `	SslFreeAllObjects(pVm);` |
|   7000 |  307 | `	pVm->pSslErrors = 0;   /* the allocator releases the ring with everything else */` |
|   7000 |  308 | `	ERR_clear_error();` |
|   7000 |  309 | `}` |
|      - |  310 |  |
|      - |  311 | `/* ------------------------------------------------------------------------` |
|      - |  312 | ` * Algorithm lookup` |
|      - |  313 | ` * ------------------------------------------------------------------------ */` |
|      - |  314 | `/*` |
|      - |  315 | `` * The EVP_MD behind php's `string\|int $algorithm`. An INT is one of php's own`` |
|      - |  316 | ` * OPENSSL_ALGO_* -- a numbering of php's, not OpenSSL's, which is why the` |
|      - |  317 | ` * table is spelled here rather than derived. A STRING goes through TWO doors` |
|      - |  318 | ` * in a fixed order, and the order is observable rather than cosmetic:` |
|      - |  319 | `` * EVP_get_digestbyname first (which knows the aliases, `RSA-SHA256` among`` |
|      - |  320 | ` * them, and pushes NOTHING when it misses), then EVP_MD_fetch (which knows` |
|      - |  321 | ` * only the provider's canonical spellings and DOES push` |
|      - |  322 | `` * `error:0308010C:digital envelope routines::unsupported` on a miss). Fetching`` |
|      - |  323 | ` * first would leave that error in the ring after every successful alias` |
|      - |  324 | ` * lookup, and openssl_error_string() would hand a script an error for a call` |
|      - |  325 | ` * that worked.` |
|      - |  326 | ` *` |
|      - |  327 | ` * A fetched digest is reference-counted and has to be freed; a legacy one must` |
|      - |  328 | ` * not be. *ppFetched is the one the caller owns.` |
|      - |  329 | ` */` |
|    206 |  330 | `PH7_PRIVATE const EVP_MD * PH7_SslDigestOfValue(ph7_value *pVal,EVP_MD **ppFetched)` |
|      3 |  331 | `{` |
|    209 |  332 | `	const EVP_MD *pMd = 0;` |
|    209 |  333 | `	*ppFetched = 0;` |
|    209 |  334 | `	if( pVal == 0 ){` |
|    ! 0 |  335 | `		return 0;` |
|      - |  336 | `	}` |
|    209 |  337 | `	if( ph7_value_is_string(pVal) ){` |
|      - |  338 | `		const char *zName;` |
|    195 |  339 | `		int nName = 0;` |
|    195 |  340 | `		zName = ph7_value_to_string(pVal,&nName);` |
|    195 |  341 | `		if( nName < 1 ){` |
|    ! 0 |  342 | `			return 0;` |
|      - |  343 | `		}` |
|    195 |  344 | `		pMd = EVP_get_digestbyname(zName);` |
|    195 |  345 | `		if( pMd == 0 ){` |
|     69 |  346 | `			*ppFetched = EVP_MD_fetch(0,zName,0);` |
|     69 |  347 | `			pMd = *ppFetched;` |
|     33 |  348 | `		}` |
|    195 |  349 | `		return pMd;` |
|      - |  350 | `	}` |
|      - |  351 | `	/* php's OPENSSL_ALGO_* numbering. The gap at 4/5 is php's own -- the two` |
|      - |  352 | `	 * DSS entries it removed -- and a number outside the set is unknown` |
|      - |  353 | `	 * rather than a fallback. */` |
|     15 |  354 | `	switch( (int)ph7_value_to_int64(pVal) ){` |
|      7 |  355 | `		case 1:  pMd = EVP_sha1();      break;` |
|    ! 0 |  356 | `		case 2:  pMd = EVP_md5();       break;` |
|    ! 0 |  357 | `		case 3:  pMd = EVP_md4();       break;` |
|    ! 0 |  358 | `		case 6:  pMd = EVP_sha224();    break;` |
|      9 |  359 | `		case 7:  pMd = EVP_sha256();    break;` |
|    ! 0 |  360 | `		case 8:  pMd = EVP_sha384();    break;` |
|    ! 0 |  361 | `		case 9:  pMd = EVP_sha512();    break;` |
|    ! 0 |  362 | `		case 10: pMd = EVP_ripemd160(); break;` |
|    ! 0 |  363 | `		default: pMd = 0;               break;` |
|      - |  364 | `	}` |
|     15 |  365 | `	return pMd;` |
|    106 |  366 | `}` |
|      - |  367 | `/*` |
|      - |  368 | ` * The EVP_CIPHER behind a name. Two doors, because OpenSSL 3 has two: the` |
|      - |  369 | ` * legacy OBJ table (EVP_get_cipherbyname, which knows "aes-256-cbc" and every` |
|      - |  370 | ` * alias) and the provider fetch (which knows the canonical names a` |
|      - |  371 | ` * openssl_get_cipher_methods() listing is built from). php's "Unknown cipher` |
|      - |  372 | ` * algorithm" is what BOTH missing looks like. A fetched cipher is` |
|      - |  373 | ` * reference-counted and has to be freed; a legacy one must not be.` |
|      - |  374 | ` */` |
|    118 |  375 | `static const EVP_CIPHER * SslCipherByName(const char *zName,EVP_CIPHER **ppFetched)` |
|      2 |  376 | `{` |
|      - |  377 | `	const EVP_CIPHER *pCipher;` |
|    120 |  378 | `	*ppFetched = 0;` |
|    120 |  379 | `	pCipher = EVP_get_cipherbyname(zName);` |
|    120 |  380 | `	if( pCipher ){` |
|    110 |  381 | `		return pCipher;` |
|      - |  382 | `	}` |
|     12 |  383 | `	*ppFetched = EVP_CIPHER_fetch(0,zName,0);` |
|     12 |  384 | `	return *ppFetched;` |
|     61 |  385 | `}` |
|      - |  386 |  |
|      - |  387 | `/* ------------------------------------------------------------------------` |
|      - |  388 | ` * The listings` |
|      - |  389 | ` * ------------------------------------------------------------------------ */` |
|      - |  390 | `/*` |
|      - |  391 | ` * A name collector: the two listings differ in where the names COME FROM, not` |
|      - |  392 | ` * in what happens to them.` |
|      - |  393 | ` */` |
|      - |  394 | `typedef struct SslNameSink SslNameSink;` |
|      - |  395 | `struct SslNameSink {` |
|      - |  396 | `	SySet sOfft;       /* sxu32 offsets into sPool -- the pool MOVES as it grows,` |
|      - |  397 | `	                    * so a pointer collected during the walk would dangle */` |
|      - |  398 | `	SyBlob sPool;` |
|      - |  399 | `	int bLower;        /* the cipher listing lowercases every name; the digest one does not */` |
|      - |  400 | `	int bAliases;      /* an alias listing keeps the rows a plain one drops */` |
|      - |  401 | `};` |
|  34960 |  402 | `static int SslStrCmp(const char *zL,const char *zR)` |
|      2 |  403 | `{` |
| 105272 |  404 | `	while( *zL && *zL == *zR ){` |
|  70312 |  405 | `		++zL;` |
|  70312 |  406 | `		++zR;` |
|      2 |  407 | `	}` |
|  34962 |  408 | `	return (int)((unsigned char)*zL) - (int)((unsigned char)*zR);` |
|      2 |  409 | `}` |
|    899 |  410 | `static void SslSinkAdd(SslNameSink *pSink,const char *zName)` |
|      1 |  411 | `{` |
|      - |  412 | `	sxu32 nOfft;` |
|      - |  413 | `	int n,i;` |
|    900 |  414 | `	if( zName == 0 ){` |
|    ! 0 |  415 | `		return;` |
|      - |  416 | `	}` |
|    900 |  417 | `	n = (int)SyStrlen(zName);` |
|    900 |  418 | `	if( n < 1 ){` |
|    ! 0 |  419 | `		return;` |
|      - |  420 | `	}` |
|    900 |  421 | `	nOfft = SyBlobLength(&pSink->sPool);` |
|    899 |  422 | `	if( SyBlobAppend(&pSink->sPool,zName,(sxu32)n) != SXRET_OK` |
|    900 |  423 | `	 \|\| SyBlobAppend(&pSink->sPool,"",1) != SXRET_OK ){` |
|    ! 0 |  424 | `		return;` |
|      - |  425 | `	}` |
|    900 |  426 | `	if( pSink->bLower ){` |
|    742 |  427 | `		char *zCopy = (char *)SyBlobData(&pSink->sPool) + nOfft;` |
|  11728 |  428 | `		for( i = 0 ; i < n ; ++i ){` |
|  10987 |  429 | `			zCopy[i] = (char)SyToLower(zCopy[i]);` |
|   5741 |  430 | `		}` |
|    383 |  431 | `	}` |
|    900 |  432 | `	SySetPut(&pSink->sOfft,(const void *)&nOfft);` |
|    465 |  433 | `}` |
|      - |  434 | `/* Sort (plain byte order, which is what php's listings come back in) and` |
|      - |  435 | ` * build the array, dropping the duplicates an alias walk produces. */` |
|      8 |  436 | `static void SslSinkFlush(ph7_context *pCtx,SslNameSink *pSink,ph7_value *pArray,ph7_value *pVal)` |
|      1 |  437 | `{` |
|      9 |  438 | `	sxu32 *aOfft = (sxu32 *)SySetBasePtr(&pSink->sOfft);` |
|      9 |  439 | `	sxu32 nUsed = SySetUsed(&pSink->sOfft);` |
|      9 |  440 | `	const char *zBase = (const char *)SyBlobData(&pSink->sPool);` |
|      9 |  441 | `	const char *zPrev = 0;` |
|      - |  442 | `	sxu32 i,j;` |
|      4 |  443 | `	SXUNUSED(pCtx);` |
|      - |  444 | `	/* Insertion sort: the lists are a few hundred rows at most and this needs` |
|      - |  445 | `	 * no context-carrying qsort (whose spelling differs per platform). */` |
|    900 |  446 | `	for( i = 1 ; i < nUsed ; ++i ){` |
|    892 |  447 | `		sxu32 nKey = aOfft[i];` |
|    892 |  448 | `		j = i;` |
|  34087 |  449 | `		while( j > 0 && SslStrCmp(zBase + aOfft[j - 1],zBase + nKey) > 0 ){` |
|  33196 |  450 | `			aOfft[j] = aOfft[j - 1];` |
|  33196 |  451 | `			--j;` |
|      1 |  452 | `		}` |
|    892 |  453 | `		aOfft[j] = nKey;` |
|    461 |  454 | `	}` |
|    908 |  455 | `	for( i = 0 ; i < nUsed ; ++i ){` |
|    900 |  456 | `		const char *zName = zBase + aOfft[i];` |
|    900 |  457 | `		if( zPrev && SslStrCmp(zPrev,zName) == 0 ){` |
|    ! 0 |  458 | `			continue;` |
|      - |  459 | `		}` |
|    900 |  460 | `		zPrev = zName;` |
|    900 |  461 | `		ph7_value_string(pVal,zName,-1);` |
|    900 |  462 | `		ph7_array_add_elem(pArray,0,pVal);` |
|    900 |  463 | `		ph7_value_reset_string_cursor(pVal);` |
|    465 |  464 | `	}` |
|      9 |  465 | `}` |
|      - |  466 | `/*` |
|      - |  467 | ` * The DIGEST walk. php reads digests out of the legacy OBJ_NAME table, where` |
|      - |  468 | ` * an ALIAS is a row that names a target: the plain listing keeps the rows with` |
|      - |  469 | ` * no target and the alias listing keeps them all.` |
|      - |  470 | ` */` |
|    230 |  471 | `static void SslMdListCb(const EVP_MD *pMd,const char *zFrom,const char *zTo,void *pArg)` |
|      1 |  472 | `{` |
|    231 |  473 | `	SslNameSink *pSink = (SslNameSink *)pArg;` |
|    118 |  474 | `	SXUNUSED(pMd);` |
|    231 |  475 | `	if( zTo != 0 && pSink->bAliases == 0 ){` |
|     73 |  476 | `		return;` |
|      - |  477 | `	}` |
|    159 |  478 | `	SslSinkAdd(pSink,zFrom);` |
|    119 |  479 | `}` |
|    482 |  480 | `static void SslCipherNameCb(const char *zName,void *pArg)` |
|      1 |  481 | `{` |
|    483 |  482 | `	SslSinkAdd((SslNameSink *)pArg,zName);` |
|    483 |  483 | `}` |
|    518 |  484 | `static void SslCipherListCb(EVP_CIPHER *pCipher,void *pArg)` |
|      1 |  485 | `{` |
|    519 |  486 | `	SslNameSink *pSink = (SslNameSink *)pArg;` |
|    519 |  487 | `	if( pSink->bAliases ){` |
|    260 |  488 | `		EVP_CIPHER_names_do_all(pCipher,SslCipherNameCb,pArg);` |
|    136 |  489 | `	}else{` |
|    260 |  490 | `		SslSinkAdd(pSink,EVP_CIPHER_get0_name(pCipher));` |
|      - |  491 | `	}` |
|    519 |  492 | `}` |
|      - |  493 | `/*` |
|      - |  494 | ` * openssl_get_md_methods() and openssl_get_cipher_methods() are NOT the same` |
|      - |  495 | ` * listing with a different table, and finding that out cost a differential` |
|      - |  496 | ` * sweep: php reads DIGESTS out of the legacy OBJ_NAME table (which is where` |
|      - |  497 | `` * `blake2b512` comes from -- the provider calls it `BLAKE2B-512`, and php's`` |
|      - |  498 | ` * answer has no such row) and CIPHERS out of the provider, lowercasing every` |
|      - |  499 | ` * name and dropping the duplicates that produces. Reading either the way the` |
|      - |  500 | ` * other is read answers a list php never gives: 22 digests instead of 21, or` |
|      - |  501 | ` * 170 cipher aliases instead of 234.` |
|      - |  502 | ` */` |
|      8 |  503 | `static int SslMethodList(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCipher)` |
|      1 |  504 | `{` |
|      - |  505 | `	SslNameSink sSink;` |
|      - |  506 | `	ph7_value *pArray,*pVal;` |
|      9 |  507 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 |  508 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      9 |  509 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  510 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  511 | `		return PH7_OK;` |
|      - |  512 | `	}` |
|      9 |  513 | `	SySetInit(&sSink.sOfft,&pCtx->pVm->sAllocator,sizeof(sxu32));` |
|      9 |  514 | `	SyBlobInit(&sSink.sPool,&pCtx->pVm->sAllocator);` |
|      9 |  515 | `	sSink.bLower = bCipher;` |
|      9 |  516 | `	sSink.bAliases = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;` |
|      9 |  517 | `	if( bCipher ){` |
|      5 |  518 | `		EVP_CIPHER_do_all_provided(0,SslCipherListCb,(void *)&sSink);` |
|      3 |  519 | `	}else{` |
|      5 |  520 | `		EVP_MD_do_all_sorted(SslMdListCb,(void *)&sSink);` |
|      - |  521 | `	}` |
|      9 |  522 | `	SslSinkFlush(pCtx,&sSink,pArray,pVal);` |
|      9 |  523 | `	SySetRelease(&sSink.sOfft);` |
|      9 |  524 | `	SyBlobRelease(&sSink.sPool);` |
|      9 |  525 | `	ph7_result_value(pCtx,pArray);` |
|      9 |  526 | `	return PH7_OK;` |
|      5 |  527 | `}` |
|      4 |  528 | `static int vm_builtin_openssl_get_md_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  529 | `{` |
|      5 |  530 | `	return SslMethodList(pCtx,nArg,apArg,0);` |
|      1 |  531 | `}` |
|      4 |  532 | `static int vm_builtin_openssl_get_cipher_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  533 | `{` |
|      5 |  534 | `	return SslMethodList(pCtx,nArg,apArg,1);` |
|      1 |  535 | `}` |
|      - |  536 | `/*` |
|      - |  537 | ` * openssl_get_curve_names(): the library's built-in EC curves, by SHORT name,` |
|      - |  538 | ` * in the order EC_get_builtin_curves answers -- not sorted, which is a` |
|      - |  539 | ` * difference from the two listings above and is php's.` |
|      - |  540 | ` */` |
|      2 |  541 | `static int vm_builtin_openssl_get_curve_names(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  542 | `{` |
|      - |  543 | `	EC_builtin_curve *aCurve;` |
|      - |  544 | `	ph7_value *pArray,*pVal;` |
|      - |  545 | `	size_t nCurve,i;` |
|      1 |  546 | `	SXUNUSED(nArg);` |
|      1 |  547 | `	SXUNUSED(apArg);` |
|      3 |  548 | `	nCurve = EC_get_builtin_curves(0,0);` |
|      3 |  549 | `	if( nCurve < 1 ){` |
|    ! 0 |  550 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  551 | `		return PH7_OK;` |
|      - |  552 | `	}` |
|      4 |  553 | `	aCurve = (EC_builtin_curve *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|      1 |  554 | `		(sxu32)(sizeof(EC_builtin_curve) * nCurve));` |
|      3 |  555 | `	if( aCurve == 0 ){` |
|    ! 0 |  556 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  557 | `		return PH7_OK;` |
|      - |  558 | `	}` |
|      3 |  559 | `	if( EC_get_builtin_curves(aCurve,nCurve) != nCurve ){` |
|    ! 0 |  560 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aCurve);` |
|    ! 0 |  561 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  562 | `		return PH7_OK;` |
|      - |  563 | `	}` |
|      3 |  564 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 |  565 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      3 |  566 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  567 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aCurve);` |
|    ! 0 |  568 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  569 | `		return PH7_OK;` |
|      - |  570 | `	}` |
|    167 |  571 | `	for( i = 0 ; i < nCurve ; ++i ){` |
|    165 |  572 | `		const char *zName = OBJ_nid2sn(aCurve[i].nid);` |
|    165 |  573 | `		if( zName == 0 ){` |
|    ! 0 |  574 | `			continue;` |
|      - |  575 | `		}` |
|    165 |  576 | `		ph7_value_string(pVal,zName,-1);` |
|    165 |  577 | `		ph7_array_add_elem(pArray,0,pVal);` |
|    165 |  578 | `		ph7_value_reset_string_cursor(pVal);` |
|     83 |  579 | `	}` |
|      3 |  580 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,aCurve);` |
|      3 |  581 | `	ph7_result_value(pCtx,pArray);` |
|      3 |  582 | `	return PH7_OK;` |
|      2 |  583 | `}` |
|      - |  584 | `/*` |
|      - |  585 | ` * openssl_get_cert_locations(): where the library looks for a CA bundle, plus` |
|      - |  586 | ` * the two ini directives php lets a build override them with. Composer's` |
|      - |  587 | ` * ca-bundle reads this to decide whether the system store is usable at all,` |
|      - |  588 | ` * so the KEY SET is the contract even where the paths are the box's.` |
|      - |  589 | ` */` |
|      2 |  590 | `static int vm_builtin_openssl_get_cert_locations(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  591 | `{` |
|      - |  592 | `	ph7_value *pArray,*pVal;` |
|      - |  593 | `	const char *zIni;` |
|      - |  594 | `	SyBlob sIni;` |
|      1 |  595 | `	SXUNUSED(nArg);` |
|      1 |  596 | `	SXUNUSED(apArg);` |
|      3 |  597 | `	SyBlobInit(&sIni,&pCtx->pVm->sAllocator);` |
|      3 |  598 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 |  599 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      3 |  600 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  601 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  602 | `		return PH7_OK;` |
|      - |  603 | `	}` |
|      - |  604 | `#define SSL_LOC(K,V) \` |
|      - |  605 | `	ph7_value_string(pVal,(V) ? (V) : "",-1); \` |
|      - |  606 | `	ph7_array_add_strkey_elem(pArray,K,pVal); \` |
|      - |  607 | `	ph7_value_reset_string_cursor(pVal)` |
|      3 |  608 | `	SSL_LOC("default_cert_file",X509_get_default_cert_file());` |
|      3 |  609 | `	SSL_LOC("default_cert_file_env",X509_get_default_cert_file_env());` |
|      3 |  610 | `	SSL_LOC("default_cert_dir",X509_get_default_cert_dir());` |
|      3 |  611 | `	SSL_LOC("default_cert_dir_env",X509_get_default_cert_dir_env());` |
|      3 |  612 | `	SSL_LOC("default_private_dir",X509_get_default_private_dir());` |
|      3 |  613 | `	SSL_LOC("default_default_cert_area",X509_get_default_cert_area());` |
|      3 |  614 | `	PH7_VmIniGetStr(pCtx->pVm,"openssl.cafile",&sIni);` |
|      3 |  615 | `	SyBlobAppend(&sIni,"",1);` |
|      3 |  616 | `	zIni = (const char *)SyBlobData(&sIni);` |
|      3 |  617 | `	SSL_LOC("ini_cafile",zIni);` |
|      3 |  618 | `	SyBlobReset(&sIni);` |
|      3 |  619 | `	PH7_VmIniGetStr(pCtx->pVm,"openssl.capath",&sIni);` |
|      3 |  620 | `	SyBlobAppend(&sIni,"",1);` |
|      3 |  621 | `	zIni = (const char *)SyBlobData(&sIni);` |
|      3 |  622 | `	SSL_LOC("ini_capath",zIni);` |
|      3 |  623 | `	SyBlobRelease(&sIni);` |
|      - |  624 | `#undef SSL_LOC` |
|      3 |  625 | `	ph7_result_value(pCtx,pArray);` |
|      3 |  626 | `	return PH7_OK;` |
|      2 |  627 | `}` |
|      - |  628 |  |
|      - |  629 | `/* ------------------------------------------------------------------------` |
|      - |  630 | ` * Digests` |
|      - |  631 | ` * ------------------------------------------------------------------------ */` |
|     82 |  632 | `static int vm_builtin_openssl_digest(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  633 | `{` |
|      - |  634 | `	unsigned char aOut[EVP_MAX_MD_SIZE];` |
|     83 |  635 | `	unsigned int nOut = 0;` |
|      - |  636 | `	const EVP_MD *pMd;` |
|     83 |  637 | `	EVP_MD *pFetched = 0;` |
|      - |  638 | `	EVP_MD_CTX *pMdCtx;` |
|      - |  639 | `	const char *zData;` |
|     83 |  640 | `	int nData = 0,bRaw = 0;` |
|     83 |  641 | `	if( nArg < 2 ){` |
|    ! 0 |  642 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  643 | `		return PH7_OK;` |
|      - |  644 | `	}` |
|     83 |  645 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|     83 |  646 | `	pMd = PH7_SslDigestOfValue(apArg[1],&pFetched);` |
|     83 |  647 | `	if( pMd == 0 ){` |
|     59 |  648 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");` |
|     59 |  649 | `		ph7_result_bool(pCtx,0);` |
|     59 |  650 | `		return PH7_OK;` |
|      - |  651 | `	}` |
|     25 |  652 | `	if( nArg > 2 ){` |
|      3 |  653 | `		bRaw = ph7_value_to_bool(apArg[2]);` |
|      1 |  654 | `	}` |
|     25 |  655 | `	pMdCtx = EVP_MD_CTX_new();` |
|     25 |  656 | `	if( pMdCtx == 0 ){` |
|    ! 0 |  657 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|    ! 0 |  658 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 |  659 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  660 | `		return PH7_OK;` |
|      - |  661 | `	}` |
|     24 |  662 | `	if( EVP_DigestInit(pMdCtx,pMd) != 1` |
|     24 |  663 | `	 \|\| EVP_DigestUpdate(pMdCtx,zData,(size_t)(nData > 0 ? nData : 0)) != 1` |
|     25 |  664 | `	 \|\| EVP_DigestFinal(pMdCtx,aOut,&nOut) != 1 ){` |
|    ! 0 |  665 | `		EVP_MD_CTX_free(pMdCtx);` |
|    ! 0 |  666 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|    ! 0 |  667 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 |  668 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  669 | `		return PH7_OK;` |
|      - |  670 | `	}` |
|     25 |  671 | `	EVP_MD_CTX_free(pMdCtx);` |
|     25 |  672 | `	if( pFetched ){ EVP_MD_free(pFetched); }` |
|     25 |  673 | `	if( bRaw ){` |
|      3 |  674 | `		ph7_result_string(pCtx,(const char *)aOut,(int)nOut);` |
|      2 |  675 | `	}else{` |
|      - |  676 | `		char zHex[EVP_MAX_MD_SIZE * 2];` |
|      - |  677 | `		static const char zDigit[] = "0123456789abcdef";` |
|      - |  678 | `		unsigned int i;` |
|    695 |  679 | `		for( i = 0 ; i < nOut ; ++i ){` |
|    673 |  680 | `			zHex[i * 2]     = zDigit[(aOut[i] >> 4) & 0x0F];` |
|    673 |  681 | `			zHex[i * 2 + 1] = zDigit[aOut[i] & 0x0F];` |
|    337 |  682 | `		}` |
|     23 |  683 | `		ph7_result_string(pCtx,zHex,(int)(nOut * 2));` |
|      - |  684 | `	}` |
|     25 |  685 | `	return PH7_OK;` |
|     42 |  686 | `}` |
|      - |  687 |  |
|      - |  688 | `/* ------------------------------------------------------------------------` |
|      - |  689 | ` * Ciphers` |
|      - |  690 | ` * ------------------------------------------------------------------------ */` |
|      - |  691 | `/*` |
|      - |  692 | ` * What an AEAD cipher needs that an ordinary one does not. CCM is the awkward` |
|      - |  693 | ` * one and php treats it separately at three points: its tag length must be set` |
|      - |  694 | ` * BEFORE the key even when encrypting, the plaintext length has to be` |
|      - |  695 | ` * declared in a first Update with a NULL buffer, and the whole message is one` |
|      - |  696 | ` * run rather than a stream.` |
|      - |  697 | ` */` |
|      - |  698 | `typedef struct SslCipherMode SslCipherMode;` |
|      - |  699 | `struct SslCipherMode {` |
|      - |  700 | `	int bAead;` |
|      - |  701 | `	int bSingleRun;      /* CCM: length declared up front, one Update */` |
|      - |  702 | `	int bSetTagLength;   /* CCM: EVP_CTRL_AEAD_SET_TAG before the key */` |
|      - |  703 | `};` |
|     82 |  704 | `static void SslLoadCipherMode(SslCipherMode *pMode,const EVP_CIPHER *pCipher)` |
|      1 |  705 | `{` |
|     83 |  706 | `	int iMode = EVP_CIPHER_get_mode(pCipher);` |
|     83 |  707 | `	pMode->bAead = 0;` |
|     83 |  708 | `	pMode->bSingleRun = 0;` |
|     83 |  709 | `	pMode->bSetTagLength = 0;` |
|     83 |  710 | `	switch( iMode ){` |
|     11 |  711 | `		case EVP_CIPH_GCM_MODE:` |
|      - |  712 | `		case EVP_CIPH_OCB_MODE:` |
|     23 |  713 | `			pMode->bAead = 1;` |
|     23 |  714 | `			break;` |
|      4 |  715 | `		case EVP_CIPH_CCM_MODE:` |
|      9 |  716 | `			pMode->bAead = 1;` |
|      9 |  717 | `			pMode->bSingleRun = 1;` |
|      9 |  718 | `			pMode->bSetTagLength = 1;` |
|      9 |  719 | `			break;` |
|     26 |  720 | `		default:` |
|      - |  721 | `			/* chacha20-poly1305 is AEAD without being one of the three MODES:` |
|      - |  722 | `			 * OpenSSL files it as a stream cipher and marks it with a FLAG` |
|      - |  723 | ``			 * instead. Reading the mode alone answers `The tag cannot be used`` |
|      - |  724 | ``			 * because the cipher algorithm does not support AEAD` for the one`` |
|      - |  725 | `			 * AEAD cipher a modern program is most likely to reach for. */` |
|     53 |  726 | `			if( (EVP_CIPHER_get_flags(pCipher) & EVP_CIPH_FLAG_AEAD_CIPHER) != 0 ){` |
|      9 |  727 | `				pMode->bAead = 1;` |
|      4 |  728 | `			}` |
|     52 |  729 | `			break;` |
|      - |  730 | `	}` |
|     83 |  731 | `}` |
|      - |  732 | `/*` |
|      - |  733 | ` * php's IV screen, warnings and all -- and its POSITION, which a differential` |
|      - |  734 | ` * sweep had to find: this runs AFTER the cipher is installed on the context` |
|      - |  735 | ` * and its three sentences describe what it then DOES. A short IV is padded` |
|      - |  736 | ` * with NULs, a long one is truncated, and neither is an error. An EMPTY one is` |
|      - |  737 | ` * padded SILENTLY here; the "potentially insecure" warning about it is a` |
|      - |  738 | ` * separate check that runs BEFORE the init (see SslWarnEmptyIv), which is why` |
|      - |  739 | `` * a cipher this build cannot initialize at all -- `bf-cbc`, whose provider is`` |
|      - |  740 | ` * not loaded -- still raises that one sentence and none of these.` |
|      - |  741 | ` *` |
|      - |  742 | ` * The AEAD branch is not a screen at all: it is where php SETS the nonce` |
|      - |  743 | ` * length, so an empty IV reaches EVP_CTRL_AEAD_SET_IVLEN as a zero and its` |
|      - |  744 | ` * refusal is what a script sees.` |
|      - |  745 | ` *` |
|      - |  746 | ` * Answers 1 when *ppIv is a buffer the caller frees, 0 when it points into the` |
|      - |  747 | ` * argument, and -1/-2 for a failure (-2 having already reported itself).` |
|      - |  748 | ` */` |
|     80 |  749 | `static int SslPrepareIv(ph7_context *pCtx,EVP_CIPHER_CTX *pEvp,const EVP_CIPHER *pCipher,` |
|      - |  750 | `	const char *zIv,int nIv,unsigned char **ppIv,int *pnIv,SslCipherMode *pMode)` |
|      1 |  751 | `{` |
|     81 |  752 | `	int nWant = EVP_CIPHER_get_iv_length(pCipher);` |
|      - |  753 | `	unsigned char *zBuf;` |
|     81 |  754 | `	if( pMode->bAead ){` |
|     37 |  755 | `		if( EVP_CIPHER_CTX_ctrl(pEvp,EVP_CTRL_AEAD_SET_IVLEN,nIv,0) != 1 ){` |
|      3 |  756 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  757 | `				"Setting of IV length for AEAD mode failed");` |
|      3 |  758 | `			return -2;` |
|      - |  759 | `		}` |
|     35 |  760 | `		*ppIv = (unsigned char *)zIv;` |
|     35 |  761 | `		*pnIv = nIv;` |
|     35 |  762 | `		return 0;` |
|      - |  763 | `	}` |
|     45 |  764 | `	if( nWant < 0 ){` |
|    ! 0 |  765 | `		nWant = 0;` |
|    ! 0 |  766 | `	}` |
|     45 |  767 | `	if( nIv == nWant ){` |
|     33 |  768 | `		*ppIv = nWant > 0 ? (unsigned char *)zIv : 0;` |
|     33 |  769 | `		*pnIv = nWant;` |
|     33 |  770 | `		return 0;` |
|      - |  771 | `	}` |
|     13 |  772 | `	zBuf = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nWant + 1));` |
|     13 |  773 | `	if( zBuf == 0 ){` |
|    ! 0 |  774 | `		return -1;` |
|      - |  775 | `	}` |
|     13 |  776 | `	SyZero(zBuf,(sxu32)(nWant + 1));` |
|     13 |  777 | `	if( nIv < 1 ){` |
|      - |  778 | `		/* php's own "BC behavior": all zeros, and nothing said here. */` |
|     11 |  779 | `	}else if( nIv < nWant ){` |
|      4 |  780 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  781 | `			"IV passed is only %d bytes long, cipher expects an IV of precisely %d bytes, padding with \\0",` |
|      1 |  782 | `			nIv,nWant);` |
|      3 |  783 | `		SyMemcpy(zIv,zBuf,(sxu32)nIv);` |
|      2 |  784 | `	}else{` |
|      - |  785 | `		/* Including the nWant == 0 case: a cipher that takes NO iv still` |
|      - |  786 | `		 * reports the one it was handed, and says it expected zero. */` |
|     10 |  787 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  788 | `			"IV passed is %d bytes long which is longer than the %d expected by selected cipher, truncating",` |
|      3 |  789 | `			nIv,nWant);` |
|      7 |  790 | `		if( nWant > 0 ){` |
|      5 |  791 | `			SyMemcpy(zIv,zBuf,(sxu32)nWant);` |
|      2 |  792 | `		}` |
|      - |  793 | `	}` |
|     13 |  794 | `	*ppIv = zBuf;` |
|     13 |  795 | `	*pnIv = nWant;` |
|     13 |  796 | `	return 1;   /* caller frees */` |
|     41 |  797 | `}` |
|      - |  798 | `/*` |
|      - |  799 | ` * The one IV sentence php raises BEFORE the cipher is installed: an ENCRYPT` |
|      - |  800 | ` * with no IV at all, on a non-AEAD cipher that wants one.` |
|      - |  801 | ` */` |
|     44 |  802 | `static void SslWarnEmptyIv(ph7_context *pCtx,const EVP_CIPHER *pCipher,int nIv,SslCipherMode *pMode)` |
|      1 |  803 | `{` |
|     45 |  804 | `	if( nIv == 0 && !pMode->bAead && EVP_CIPHER_get_iv_length(pCipher) > 0 ){` |
|      3 |  805 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  806 | `			"Using an empty Initialization Vector (iv) is potentially insecure and not recommended");` |
|      1 |  807 | `	}` |
|     45 |  808 | `}` |
|      - |  809 | `/*` |
|      - |  810 | ` * php's KEY screen. Three cases, and the third is the one a reading would` |
|      - |  811 | ` * miss -- it was found by draining the error queue after every call in a` |
|      - |  812 | ` * differential sweep:` |
|      - |  813 | ` *` |
|      - |  814 | ` *  - the passphrase is SHORTER than the cipher's key. php zero-pads it,` |
|      - |  815 | ` *    unless OPENSSL_DONT_ZERO_PAD_KEY is set, which asks the CIPHER to take` |
|      - |  816 | ` *    the short key instead and answers "Key length cannot be set for the` |
|      - |  817 | ` *    cipher algorithm" for the fixed-length ciphers that refuse.` |
|      - |  818 | ` *  - the passphrase is exactly the key length. Nothing happens.` |
|      - |  819 | ` *  - the passphrase is LONGER. php still ASKS the cipher to take it, whatever` |
|      - |  820 | ` *    the options say, and when the cipher refuses php ignores the refusal and` |
|      - |  821 | ` *    uses the truncated key -- but the refusal is already in the error queue,` |
|      - |  822 | `` *    so a successful `openssl_encrypt()` with a 40-byte passphrase leaves`` |
|      - |  823 | `` *    `error:03000082:digital envelope routines::invalid key length` for the`` |
|      - |  824 | ` *    next openssl_error_string(). That stray error IS php's answer.` |
|      - |  825 | ` */` |
|     78 |  826 | `static int SslPrepareKey(ph7_context *pCtx,EVP_CIPHER_CTX *pEvp,` |
|      - |  827 | `	const char *zPass,int nPass,int iOptions,unsigned char **ppKey,int *pbFreeKey)` |
|      1 |  828 | `{` |
|     79 |  829 | `	int nWant = EVP_CIPHER_CTX_get_key_length(pEvp);` |
|      - |  830 | `	unsigned char *zBuf;` |
|     79 |  831 | `	*pbFreeKey = 0;` |
|     79 |  832 | `	if( nWant > nPass ){` |
|      4 |  833 | `		if( (iOptions & 4 /* OPENSSL_DONT_ZERO_PAD_KEY */)` |
|      4 |  834 | `		 && !EVP_CIPHER_CTX_set_key_length(pEvp,nPass) ){` |
|      3 |  835 | `			PH7_SslStoreErrors(pCtx->pVm);` |
|      3 |  836 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  837 | `				"Key length cannot be set for the cipher algorithm");` |
|      3 |  838 | `			return -1;` |
|      - |  839 | `		}` |
|      3 |  840 | `		zBuf = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nWant > 0 ? nWant : 1));` |
|      3 |  841 | `		if( zBuf == 0 ){` |
|    ! 0 |  842 | `			return -2;` |
|      - |  843 | `		}` |
|      3 |  844 | `		SyZero(zBuf,(sxu32)(nWant > 0 ? nWant : 1));` |
|      3 |  845 | `		if( nPass > 0 ){` |
|      3 |  846 | `			SyMemcpy(zPass,zBuf,(sxu32)nPass);` |
|      1 |  847 | `		}` |
|      3 |  848 | `		*ppKey = zBuf;` |
|      3 |  849 | `		*pbFreeKey = 1;` |
|      3 |  850 | `		return 0;` |
|      - |  851 | `	}` |
|     75 |  852 | `	if( nPass > nWant && !EVP_CIPHER_CTX_set_key_length(pEvp,nPass) ){` |
|      5 |  853 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      2 |  854 | `	}` |
|     75 |  855 | `	*ppKey = (unsigned char *)zPass;` |
|     75 |  856 | `	return 0;` |
|     40 |  857 | `}` |
|     74 |  858 | `static int SslB64Consumer(const void *pData,unsigned int nLen,void *pUser)` |
|      1 |  859 | `{` |
|     75 |  860 | `	return SyBlobAppend((SyBlob *)pUser,pData,nLen);` |
|      1 |  861 | `}` |
|     50 |  862 | `static int vm_builtin_openssl_encrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  863 | `{` |
|     51 |  864 | `	const char *zData,*zMethod,*zPass,*zIv = "",*zAad = "";` |
|     51 |  865 | `	int nData = 0,nMethod = 0,nPass = 0,nIv = 0,nAad = 0;` |
|     51 |  866 | `	int iOptions = 0,iTagLen = 16,bFreeIv = 0,bFreeKey = 0,nIvUse = 0;` |
|      - |  867 | `	sxi64 iOpt;` |
|      - |  868 | `	const EVP_CIPHER *pCipher;` |
|     51 |  869 | `	EVP_CIPHER_CTX *pEvp = 0;` |
|     51 |  870 | `	EVP_CIPHER *pFetched = 0;` |
|      - |  871 | `	SslCipherMode sMode;` |
|     51 |  872 | `	unsigned char *zKey = 0,*zIvUse = 0,*zOut = 0;` |
|      - |  873 | `	unsigned char aTag[EVP_MAX_MD_SIZE];` |
|     51 |  874 | `	int nOut = 0,nFinal = 0,nMax,rc = 0;` |
|     51 |  875 | `	if( nArg < 3 ){` |
|    ! 0 |  876 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  877 | `		return PH7_OK;` |
|      - |  878 | `	}` |
|     51 |  879 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|     51 |  880 | `	zMethod = ph7_value_to_string(apArg[1],&nMethod);` |
|     51 |  881 | `	zPass = ph7_value_to_string(apArg[2],&nPass);` |
|      - |  882 | `	/* An EMPTY php string reads back as a NULL pointer here, and OpenSSL's` |
|      - |  883 | `	 * Update refuses a NULL input even with a zero length -- which turned the` |
|      - |  884 | `	 * round trip of an empty plaintext through an AEAD cipher into a false.` |
|      - |  885 | ``	 * See the `native-empty-string-slot-reads-null` note. */`` |
|     51 |  886 | `	if( zData == 0 ){ zData = ""; }` |
|     51 |  887 | `	if( nArg > 3 ){` |
|     51 |  888 | `		iOpt = ph7_value_to_int64(apArg[3]);` |
|     51 |  889 | `		iOptions = (int)iOpt;` |
|     25 |  890 | `	}` |
|     51 |  891 | `	if( nArg > 4 ){` |
|     51 |  892 | `		zIv = ph7_value_to_string(apArg[4],&nIv);` |
|     25 |  893 | `	}` |
|     51 |  894 | `	if( nArg > 6 ){` |
|     17 |  895 | `		zAad = ph7_value_to_string(apArg[6],&nAad);` |
|      8 |  896 | `	}` |
|     51 |  897 | `	if( nArg > 7 ){` |
|     15 |  898 | `		iTagLen = (int)ph7_value_to_int64(apArg[7]);` |
|      7 |  899 | `	}` |
|     51 |  900 | `	pCipher = nMethod > 0 ? SslCipherByName(zMethod,&pFetched) : 0;` |
|     51 |  901 | `	if( pCipher == 0 ){` |
|      5 |  902 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown cipher algorithm");` |
|      5 |  903 | `		ph7_result_bool(pCtx,0);` |
|      5 |  904 | `		return PH7_OK;` |
|      - |  905 | `	}` |
|     47 |  906 | `	SslLoadCipherMode(&sMode,pCipher);` |
|     47 |  907 | `	if( sMode.bAead && nArg > 5 && (iTagLen < 4 \|\| iTagLen > 16) ){` |
|      - |  908 | `		/* php reports an out-of-range tag length through the FAILURE of the` |
|      - |  909 | `		 * retrieval rather than as its own screen, which is why a 0 and a 17` |
|      - |  910 | `		 * both read "Retrieving verification tag failed". */` |
|      3 |  911 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Retrieving verification tag failed");` |
|      3 |  912 | `		if( pFetched ){ EVP_CIPHER_free(pFetched); }` |
|      3 |  913 | `		ph7_result_bool(pCtx,0);` |
|      3 |  914 | `		return PH7_OK;` |
|      - |  915 | `	}` |
|     45 |  916 | `	pEvp = EVP_CIPHER_CTX_new();` |
|     45 |  917 | `	if( pEvp == 0 ){` |
|    ! 0 |  918 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 |  919 | `		if( pFetched ){ EVP_CIPHER_free(pFetched); }` |
|    ! 0 |  920 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  921 | `		return PH7_OK;` |
|      - |  922 | `	}` |
|     45 |  923 | `	SslWarnEmptyIv(pCtx,pCipher,nIv,&sMode);` |
|     45 |  924 | `	if( EVP_EncryptInit_ex(pEvp,pCipher,0,0,0) != 1 ){` |
|    ! 0 |  925 | `		rc = -1;` |
|    ! 0 |  926 | `		goto done;` |
|      - |  927 | `	}` |
|     45 |  928 | `	bFreeIv = SslPrepareIv(pCtx,pEvp,pCipher,zIv,nIv,&zIvUse,&nIvUse,&sMode);` |
|     45 |  929 | `	if( bFreeIv < 0 ){` |
|      3 |  930 | `		rc = bFreeIv == -2 ? -2 : -1;` |
|      3 |  931 | `		goto done;` |
|      - |  932 | `	}` |
|     43 |  933 | `	if( sMode.bSetTagLength ){` |
|      5 |  934 | `		if( EVP_CIPHER_CTX_ctrl(pEvp,EVP_CTRL_AEAD_SET_TAG,iTagLen,0) != 1 ){` |
|    ! 0 |  935 | `			rc = -1;` |
|    ! 0 |  936 | `			goto done;` |
|      - |  937 | `		}` |
|      2 |  938 | `	}` |
|     43 |  939 | `	if( SslPrepareKey(pCtx,pEvp,zPass,nPass,iOptions,&zKey,&bFreeKey) != 0 ){` |
|      3 |  940 | `		rc = -2;` |
|      3 |  941 | `		goto done;` |
|      - |  942 | `	}` |
|     41 |  943 | `	if( EVP_EncryptInit_ex(pEvp,0,0,zKey,zIvUse) != 1 ){` |
|    ! 0 |  944 | `		rc = -1;` |
|    ! 0 |  945 | `		goto done;` |
|      - |  946 | `	}` |
|     41 |  947 | `	if( iOptions & 2 /* OPENSSL_ZERO_PADDING */ ){` |
|      5 |  948 | `		EVP_CIPHER_CTX_set_padding(pEvp,0);` |
|      2 |  949 | `	}` |
|     41 |  950 | `	nMax = nData + EVP_CIPHER_get_block_size(pCipher);` |
|     41 |  951 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nMax > 0 ? nMax : 1));` |
|     41 |  952 | `	if( zOut == 0 ){` |
|    ! 0 |  953 | `		rc = -2;` |
|    ! 0 |  954 | `		goto done;` |
|      - |  955 | `	}` |
|     41 |  956 | `	if( sMode.bSingleRun ){` |
|      5 |  957 | `		int nDummy = 0;` |
|      5 |  958 | `		if( EVP_EncryptUpdate(pEvp,0,&nDummy,0,nData) != 1 ){` |
|    ! 0 |  959 | `			rc = -1;` |
|    ! 0 |  960 | `			goto done;` |
|      - |  961 | `		}` |
|      2 |  962 | `	}` |
|     41 |  963 | `	if( nAad > 0 && sMode.bAead ){` |
|      7 |  964 | `		int nDummy = 0;` |
|      7 |  965 | `		if( EVP_EncryptUpdate(pEvp,0,&nDummy,(const unsigned char *)zAad,nAad) != 1 ){` |
|    ! 0 |  966 | `			rc = -1;` |
|    ! 0 |  967 | `			goto done;` |
|      - |  968 | `		}` |
|      3 |  969 | `	}` |
|     41 |  970 | `	if( EVP_EncryptUpdate(pEvp,zOut,&nOut,(const unsigned char *)zData,nData) != 1 ){` |
|    ! 0 |  971 | `		rc = -1;` |
|    ! 0 |  972 | `		goto done;` |
|      - |  973 | `	}` |
|     41 |  974 | `	if( EVP_EncryptFinal_ex(pEvp,zOut + nOut,&nFinal) != 1 ){` |
|      3 |  975 | `		rc = -1;` |
|      3 |  976 | `		goto done;` |
|      - |  977 | `	}` |
|     39 |  978 | `	nOut += nFinal;` |
|     39 |  979 | `	if( sMode.bAead && nArg > 5 ){` |
|     15 |  980 | `		if( EVP_CIPHER_CTX_ctrl(pEvp,EVP_CTRL_AEAD_GET_TAG,iTagLen,aTag) != 1 ){` |
|    ! 0 |  981 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Retrieving verification tag failed");` |
|    ! 0 |  982 | `			rc = -2;` |
|    ! 0 |  983 | `			goto done;` |
|      - |  984 | `		}` |
|      7 |  985 | `		{` |
|     15 |  986 | `			ph7_value *pTag = ph7_context_new_scalar(pCtx);` |
|     15 |  987 | `			if( pTag ){` |
|     15 |  988 | `				ph7_value_string(pTag,(const char *)aTag,iTagLen);` |
|     15 |  989 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[5],pTag);` |
|     15 |  990 | `				ph7_context_release_value(pCtx,pTag);` |
|      8 |  991 | `			}` |
|      - |  992 | `		}` |
|     32 |  993 | `	}else if( nArg > 5 ){` |
|      - |  994 | `		/* A tag was ASKED for on a cipher that has none: php leaves the` |
|      - |  995 | `		 * variable NULL and runs the encryption anyway -- it is the DECRYPT` |
|      - |  996 | `		 * side that refuses a tag it cannot use. */` |
|      3 |  997 | `		ph7_value *pTag = ph7_context_new_scalar(pCtx);` |
|      3 |  998 | `		if( pTag ){` |
|      3 |  999 | `			ph7_value_null(pTag);` |
|      3 | 1000 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[5],pTag);` |
|      3 | 1001 | `			ph7_context_release_value(pCtx,pTag);` |
|      1 | 1002 | `		}` |
|      1 | 1003 | `	}` |
|     58 | 1004 | `	if( iOptions & 1 /* OPENSSL_RAW_DATA */ ){` |
|     29 | 1005 | `		ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|     15 | 1006 | `	}else{` |
|      - | 1007 | `		SyBlob sB64;` |
|     11 | 1008 | `		SyBlobInit(&sB64,&pCtx->pVm->sAllocator);` |
|     11 | 1009 | `		SyBase64Encode((const char *)zOut,(sxu32)nOut,SslB64Consumer,(void *)&sB64);` |
|     11 | 1010 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sB64),(int)SyBlobLength(&sB64));` |
|     11 | 1011 | `		SyBlobRelease(&sB64);` |
|      - | 1012 | `	}` |
|     22 | 1013 | `done:` |
|     45 | 1014 | `	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }` |
|     45 | 1015 | `	if( bFreeIv > 0 && zIvUse ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zIvUse); }` |
|     45 | 1016 | `	if( bFreeKey && zKey ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zKey); }` |
|     45 | 1017 | `	if( pEvp ){ EVP_CIPHER_CTX_free(pEvp); }` |
|     45 | 1018 | `	if( pFetched ){ EVP_CIPHER_free(pFetched); }` |
|     45 | 1019 | `	if( rc != 0 ){` |
|      7 | 1020 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      7 | 1021 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1022 | `	}` |
|     45 | 1023 | `	return PH7_OK;` |
|     26 | 1024 | `}` |
|     38 | 1025 | `static int vm_builtin_openssl_decrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1026 | `{` |
|     39 | 1027 | `	const char *zData,*zMethod,*zPass,*zIv = "",*zAad = "",*zTag = 0;` |
|     39 | 1028 | `	int nData = 0,nMethod = 0,nPass = 0,nIv = 0,nAad = 0,nTag = 0;` |
|     39 | 1029 | `	int iOptions = 0,bFreeIv = 0,bFreeKey = 0,nIvUse = 0,bHasTag = 0;` |
|      - | 1030 | `	const EVP_CIPHER *pCipher;` |
|     39 | 1031 | `	EVP_CIPHER_CTX *pEvp = 0;` |
|     39 | 1032 | `	EVP_CIPHER *pFetched = 0;` |
|      - | 1033 | `	SslCipherMode sMode;` |
|      - | 1034 | `	SyBlob sRaw;` |
|     39 | 1035 | `	unsigned char *zKey = 0,*zIvUse = 0,*zOut = 0;` |
|     39 | 1036 | `	int nOut = 0,nFinal = 0,rc = 0,bRawInit = 0;` |
|     39 | 1037 | `	if( nArg < 3 ){` |
|    ! 0 | 1038 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1039 | `		return PH7_OK;` |
|      - | 1040 | `	}` |
|     39 | 1041 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|     39 | 1042 | `	zMethod = ph7_value_to_string(apArg[1],&nMethod);` |
|     39 | 1043 | `	zPass = ph7_value_to_string(apArg[2],&nPass);` |
|     39 | 1044 | `	if( zData == 0 ){ zData = ""; }   /* see openssl_encrypt() above */` |
|     39 | 1045 | `	if( nArg > 3 ){` |
|     39 | 1046 | `		iOptions = (int)ph7_value_to_int64(apArg[3]);` |
|     19 | 1047 | `	}` |
|     39 | 1048 | `	if( nArg > 4 ){` |
|     39 | 1049 | `		zIv = ph7_value_to_string(apArg[4],&nIv);` |
|     19 | 1050 | `	}` |
|     39 | 1051 | `	if( nArg > 5 && !ph7_value_is_null(apArg[5]) ){` |
|     21 | 1052 | `		zTag = ph7_value_to_string(apArg[5],&nTag);` |
|     21 | 1053 | `		bHasTag = 1;` |
|     10 | 1054 | `	}` |
|     39 | 1055 | `	if( nArg > 6 ){` |
|     21 | 1056 | `		zAad = ph7_value_to_string(apArg[6],&nAad);` |
|     10 | 1057 | `	}` |
|     39 | 1058 | `	pCipher = nMethod > 0 ? SslCipherByName(zMethod,&pFetched) : 0;` |
|     39 | 1059 | `	if( pCipher == 0 ){` |
|      3 | 1060 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown cipher algorithm");` |
|      3 | 1061 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1062 | `		return PH7_OK;` |
|      - | 1063 | `	}` |
|     37 | 1064 | `	SslLoadCipherMode(&sMode,pCipher);` |
|     37 | 1065 | `	SyBlobInit(&sRaw,&pCtx->pVm->sAllocator);` |
|     37 | 1066 | `	bRawInit = 1;` |
|     37 | 1067 | `	if( (iOptions & 1 /* OPENSSL_RAW_DATA */) == 0 ){` |
|      5 | 1068 | `		if( nData > 0 ){` |
|      5 | 1069 | `			SyBase64Decode(zData,(sxu32)nData,SslB64Consumer,(void *)&sRaw);` |
|      2 | 1070 | `		}` |
|      5 | 1071 | `		zData = (const char *)SyBlobData(&sRaw);` |
|      5 | 1072 | `		nData = (int)SyBlobLength(&sRaw);` |
|      5 | 1073 | `		if( zData == 0 ){ zData = ""; }` |
|      2 | 1074 | `	}` |
|     37 | 1075 | `	pEvp = EVP_CIPHER_CTX_new();` |
|     37 | 1076 | `	if( pEvp == 0 ){` |
|    ! 0 | 1077 | `		rc = -1;` |
|    ! 0 | 1078 | `		goto done;` |
|      - | 1079 | `	}` |
|     37 | 1080 | `	if( EVP_DecryptInit_ex(pEvp,pCipher,0,0,0) != 1 ){` |
|    ! 0 | 1081 | `		rc = -1;` |
|    ! 0 | 1082 | `		goto done;` |
|      - | 1083 | `	}` |
|     37 | 1084 | `	bFreeIv = SslPrepareIv(pCtx,pEvp,pCipher,zIv,nIv,&zIvUse,&nIvUse,&sMode);` |
|     37 | 1085 | `	if( bFreeIv < 0 ){` |
|    ! 0 | 1086 | `		rc = bFreeIv == -2 ? -2 : -1;` |
|    ! 0 | 1087 | `		goto done;` |
|      - | 1088 | `	}` |
|     37 | 1089 | `	if( bHasTag && !sMode.bAead ){` |
|      - | 1090 | `		/* NOT a refusal, which is the whole point of this line: php says the` |
|      - | 1091 | `		 * tag cannot be used and then decrypts anyway, so a correct call that` |
|      - | 1092 | `		 * happens to pass a tag still answers its plaintext. It also sits` |
|      - | 1093 | `		 * between the IV screen and the key one -- a short IV reports first,` |
|      - | 1094 | `		 * a bad key reports after. */` |
|      3 | 1095 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1096 | `			"The tag cannot be used because the cipher algorithm does not support AEAD");` |
|      1 | 1097 | `	}` |
|     37 | 1098 | `	if( sMode.bSetTagLength ){` |
|      - | 1099 | `		/* CCM wants the tag AND its length before the key: the mode carries no` |
|      - | 1100 | `		 * Final, so the value it will verify against has to be on the context` |
|      - | 1101 | `		 * by the time the key is installed. GCM and OCB take theirs after. */` |
|      5 | 1102 | `		if( nTag < 1 ){` |
|    ! 0 | 1103 | `			rc = -2;` |
|    ! 0 | 1104 | `			goto done;` |
|      - | 1105 | `		}` |
|      5 | 1106 | `		if( EVP_CIPHER_CTX_ctrl(pEvp,EVP_CTRL_AEAD_SET_TAG,nTag,(void *)zTag) != 1 ){` |
|    ! 0 | 1107 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1108 | `				"Setting tag for AEAD cipher decryption failed");` |
|    ! 0 | 1109 | `			rc = -1;` |
|    ! 0 | 1110 | `			goto done;` |
|      - | 1111 | `		}` |
|      2 | 1112 | `	}` |
|     37 | 1113 | `	if( SslPrepareKey(pCtx,pEvp,zPass,nPass,iOptions,&zKey,&bFreeKey) != 0 ){` |
|    ! 0 | 1114 | `		rc = -2;` |
|    ! 0 | 1115 | `		goto done;` |
|      - | 1116 | `	}` |
|     37 | 1117 | `	if( EVP_DecryptInit_ex(pEvp,0,0,zKey,zIvUse) != 1 ){` |
|    ! 0 | 1118 | `		rc = -1;` |
|    ! 0 | 1119 | `		goto done;` |
|      - | 1120 | `	}` |
|     37 | 1121 | `	if( sMode.bAead && !sMode.bSetTagLength ){` |
|     17 | 1122 | `		if( nTag < 1 ){` |
|      - | 1123 | `			/* No tag at all on an AEAD cipher: a flat false, no message and` |
|      - | 1124 | ``			 * nothing left in the error ring. php raises no `A tag should be`` |
|      - | 1125 | ``			 * provided` sentence here. */`` |
|      3 | 1126 | `			rc = -2;` |
|      3 | 1127 | `			goto done;` |
|      - | 1128 | `		}` |
|     15 | 1129 | `		if( EVP_CIPHER_CTX_ctrl(pEvp,EVP_CTRL_AEAD_SET_TAG,nTag,(void *)zTag) != 1 ){` |
|      3 | 1130 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1131 | `				"Setting tag for AEAD cipher decryption failed");` |
|      3 | 1132 | `			rc = -1;` |
|      3 | 1133 | `			goto done;` |
|      - | 1134 | `		}` |
|      6 | 1135 | `	}` |
|     33 | 1136 | `	if( iOptions & 2 /* OPENSSL_ZERO_PADDING */ ){` |
|      3 | 1137 | `		EVP_CIPHER_CTX_set_padding(pEvp,0);` |
|      1 | 1138 | `	}` |
|     49 | 1139 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     32 | 1140 | `		(sxu32)(nData + EVP_CIPHER_get_block_size(pCipher) + 1));` |
|     33 | 1141 | `	if( zOut == 0 ){` |
|    ! 0 | 1142 | `		rc = -2;` |
|    ! 0 | 1143 | `		goto done;` |
|      - | 1144 | `	}` |
|     33 | 1145 | `	if( sMode.bSingleRun ){` |
|      5 | 1146 | `		int nDummy = 0;` |
|      5 | 1147 | `		if( EVP_DecryptUpdate(pEvp,0,&nDummy,0,nData) != 1 ){` |
|    ! 0 | 1148 | `			rc = -1;` |
|    ! 0 | 1149 | `			goto done;` |
|      - | 1150 | `		}` |
|      2 | 1151 | `	}` |
|     33 | 1152 | `	if( nAad > 0 && sMode.bAead ){` |
|     11 | 1153 | `		int nDummy = 0;` |
|     11 | 1154 | `		if( EVP_DecryptUpdate(pEvp,0,&nDummy,(const unsigned char *)zAad,nAad) != 1 ){` |
|    ! 0 | 1155 | `			rc = -1;` |
|    ! 0 | 1156 | `			goto done;` |
|      - | 1157 | `		}` |
|      5 | 1158 | `	}` |
|     33 | 1159 | `	if( EVP_DecryptUpdate(pEvp,zOut,&nOut,(const unsigned char *)zData,nData) != 1 ){` |
|    ! 0 | 1160 | `		rc = -1;` |
|    ! 0 | 1161 | `		goto done;` |
|      - | 1162 | `	}` |
|     33 | 1163 | `	if( sMode.bSingleRun ){` |
|      - | 1164 | `		/* CCM verifies in the Update; there is no Final to run. */` |
|      5 | 1165 | `		ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|      5 | 1166 | `		goto done;` |
|      - | 1167 | `	}` |
|     29 | 1168 | `	if( EVP_DecryptFinal_ex(pEvp,zOut + nOut,&nFinal) != 1 ){` |
|     11 | 1169 | `		rc = -1;` |
|     11 | 1170 | `		goto done;` |
|      - | 1171 | `	}` |
|     19 | 1172 | `	nOut += nFinal;` |
|     19 | 1173 | `	ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|     18 | 1174 | `done:` |
|     37 | 1175 | `	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }` |
|     37 | 1176 | `	if( bFreeIv > 0 && zIvUse ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zIvUse); }` |
|     37 | 1177 | `	if( bFreeKey && zKey ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zKey); }` |
|     37 | 1178 | `	if( bRawInit ){ SyBlobRelease(&sRaw); }` |
|     37 | 1179 | `	if( pEvp ){ EVP_CIPHER_CTX_free(pEvp); }` |
|     37 | 1180 | `	if( pFetched ){ EVP_CIPHER_free(pFetched); }` |
|     37 | 1181 | `	if( rc != 0 ){` |
|     15 | 1182 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|     15 | 1183 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1184 | `	}` |
|     37 | 1185 | `	return PH7_OK;` |
|     20 | 1186 | `}` |
|      - | 1187 | `/*` |
|      - | 1188 | ` * The two cipher-length questions. They differ from openssl_encrypt()'s` |
|      - | 1189 | ` * screen in one way that is php's and not obvious: an EMPTY name here is a` |
|      - | 1190 | ` * ValueError ("must not be empty") where openssl_encrypt() calls the same` |
|      - | 1191 | ` * empty name an unknown algorithm and warns.` |
|      - | 1192 | ` */` |
|     24 | 1193 | `static int SslCipherLength(ph7_context *pCtx,int nArg,ph7_value **apArg,int bKey)` |
|      1 | 1194 | `{` |
|      - | 1195 | `	const char *zName;` |
|     25 | 1196 | `	int nName = 0,nLen;` |
|      - | 1197 | `	const EVP_CIPHER *pCipher;` |
|     25 | 1198 | `	EVP_CIPHER *pFetched = 0;` |
|     25 | 1199 | `	if( nArg < 1 ){` |
|    ! 0 | 1200 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1201 | `		return PH7_OK;` |
|      - | 1202 | `	}` |
|     25 | 1203 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|     25 | 1204 | `	if( nName < 1 ){` |
|      7 | 1205 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      2 | 1206 | `			"%s(): Argument #1 ($cipher_algo) must not be empty",ph7_function_name(pCtx));` |
|      - | 1207 | `	}` |
|     21 | 1208 | `	pCipher = SslCipherByName(zName,&pFetched);` |
|     21 | 1209 | `	if( pCipher == 0 ){` |
|      5 | 1210 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown cipher algorithm");` |
|      5 | 1211 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1212 | `		return PH7_OK;` |
|      - | 1213 | `	}` |
|     17 | 1214 | `	nLen = bKey ? EVP_CIPHER_get_key_length(pCipher) : EVP_CIPHER_get_iv_length(pCipher);` |
|     17 | 1215 | `	if( pFetched ){` |
|    ! 0 | 1216 | `		EVP_CIPHER_free(pFetched);` |
|    ! 0 | 1217 | `	}` |
|     17 | 1218 | `	ph7_result_int(pCtx,nLen);` |
|     17 | 1219 | `	return PH7_OK;` |
|     13 | 1220 | `}` |
|     12 | 1221 | `static int vm_builtin_openssl_cipher_iv_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1222 | `{` |
|     13 | 1223 | `	return SslCipherLength(pCtx,nArg,apArg,0);` |
|      1 | 1224 | `}` |
|     12 | 1225 | `static int vm_builtin_openssl_cipher_key_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1226 | `{` |
|     13 | 1227 | `	return SslCipherLength(pCtx,nArg,apArg,1);` |
|      1 | 1228 | `}` |
|      - | 1229 |  |
|      - | 1230 | `/* ------------------------------------------------------------------------` |
|      - | 1231 | ` * Randomness and key derivation` |
|      - | 1232 | ` * ------------------------------------------------------------------------ */` |
|      - | 1233 | `/*` |
|      - | 1234 | `` * openssl_random_pseudo_bytes(): php's `$strong_result` is written on SUCCESS`` |
|      - | 1235 | ` * only, and it has been a constant true since php dropped the pseudo-random` |
|      - | 1236 | ` * fallback -- RAND_bytes either produces cryptographic bytes or fails.` |
|      - | 1237 | ` */` |
|     10 | 1238 | `static int vm_builtin_openssl_random_pseudo_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1239 | `{` |
|      - | 1240 | `	sxi64 iLen;` |
|      - | 1241 | `	unsigned char *zBuf;` |
|     11 | 1242 | `	if( nArg < 1 ){` |
|    ! 0 | 1243 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1244 | `		return PH7_OK;` |
|      - | 1245 | `	}` |
|     11 | 1246 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|     11 | 1247 | `	if( iLen < 1 ){` |
|      7 | 1248 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      2 | 1249 | `			"%s(): Argument #1 ($length) must be greater than 0",ph7_function_name(pCtx));` |
|      - | 1250 | `	}` |
|      7 | 1251 | `	if( iLen > SXI32_HIGH ){` |
|    ! 0 | 1252 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1253 | `			"%s(): Argument #1 ($length) must be less than or equal to %d",` |
|    ! 0 | 1254 | `			ph7_function_name(pCtx),SXI32_HIGH);` |
|      - | 1255 | `	}` |
|      7 | 1256 | `	zBuf = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      7 | 1257 | `	if( zBuf == 0 ){` |
|    ! 0 | 1258 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1259 | `		return PH7_OK;` |
|      - | 1260 | `	}` |
|      7 | 1261 | `	if( RAND_bytes(zBuf,(int)iLen) != 1 ){` |
|    ! 0 | 1262 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zBuf);` |
|    ! 0 | 1263 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 1264 | `		return PH7_VmThrowException(pCtx,"Exception","%s(): Cannot generate a random string",` |
|    ! 0 | 1265 | `			ph7_function_name(pCtx));` |
|      - | 1266 | `	}` |
|      7 | 1267 | `	if( nArg > 1 ){` |
|      3 | 1268 | `		ph7_value *pStrong = ph7_context_new_scalar(pCtx);` |
|      3 | 1269 | `		if( pStrong ){` |
|      3 | 1270 | `			ph7_value_bool(pStrong,1);` |
|      3 | 1271 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pStrong);` |
|      3 | 1272 | `			ph7_context_release_value(pCtx,pStrong);` |
|      1 | 1273 | `		}` |
|      1 | 1274 | `	}` |
|      7 | 1275 | `	ph7_result_string(pCtx,(const char *)zBuf,(int)iLen);` |
|      7 | 1276 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zBuf);` |
|      7 | 1277 | `	return PH7_OK;` |
|      6 | 1278 | `}` |
|      - | 1279 | `/*` |
|      - | 1280 | ` * openssl_pbkdf2(). Two screens are php's own ValueErrors and the third is` |
|      - | 1281 | ` * not a screen at all: a zero ITERATION count is a flat false with no message,` |
|      - | 1282 | ` * because php hands it to PKCS5_PBKDF2_HMAC and reports what that refuses.` |
|      - | 1283 | ` */` |
|     10 | 1284 | `static int vm_builtin_openssl_pbkdf2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1285 | `{` |
|      - | 1286 | `	const char *zPass,*zSalt;` |
|     11 | 1287 | `	int nPass = 0,nSalt = 0;` |
|      - | 1288 | `	sxi64 iKeyLen,iIter;` |
|      - | 1289 | `	const EVP_MD *pMd;` |
|     11 | 1290 | `	EVP_MD *pFetched = 0;` |
|      - | 1291 | `	unsigned char *zOut;` |
|     11 | 1292 | `	if( nArg < 4 ){` |
|    ! 0 | 1293 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1294 | `		return PH7_OK;` |
|      - | 1295 | `	}` |
|     11 | 1296 | `	zPass = ph7_value_to_string(apArg[0],&nPass);` |
|     11 | 1297 | `	zSalt = ph7_value_to_string(apArg[1],&nSalt);` |
|     11 | 1298 | `	iKeyLen = ph7_value_to_int64(apArg[2]);` |
|     11 | 1299 | `	iIter = ph7_value_to_int64(apArg[3]);` |
|     11 | 1300 | `	if( iKeyLen < 1 \|\| iKeyLen > SXI32_HIGH ){` |
|      4 | 1301 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 1302 | `			"%s(): Argument #3 ($key_length) must be greater than 0",ph7_function_name(pCtx));` |
|      - | 1303 | `	}` |
|      9 | 1304 | `	if( iIter > SXI32_HIGH ){` |
|    ! 0 | 1305 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1306 | `			"%s(): Argument #4 ($iterations) must be less than or equal to %d",` |
|    ! 0 | 1307 | `			ph7_function_name(pCtx),SXI32_HIGH);` |
|      - | 1308 | `	}` |
|      9 | 1309 | `	if( nArg > 4 ){` |
|      9 | 1310 | `		pMd = PH7_SslDigestOfValue(apArg[4],&pFetched);` |
|      5 | 1311 | `	}else{` |
|    ! 0 | 1312 | `		pMd = EVP_sha1();` |
|      - | 1313 | `	}` |
|      9 | 1314 | `	if( pMd == 0 ){` |
|      3 | 1315 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");` |
|      3 | 1316 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1317 | `		return PH7_OK;` |
|      - | 1318 | `	}` |
|      7 | 1319 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iKeyLen);` |
|      7 | 1320 | `	if( zOut == 0 ){` |
|    ! 0 | 1321 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|    ! 0 | 1322 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1323 | `		return PH7_OK;` |
|      - | 1324 | `	}` |
|      9 | 1325 | `	if( PKCS5_PBKDF2_HMAC(zPass,nPass,(const unsigned char *)zSalt,nSalt,` |
|      7 | 1326 | `			(int)iIter,pMd,(int)iKeyLen,zOut) != 1 ){` |
|      3 | 1327 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|      3 | 1328 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|      3 | 1329 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      3 | 1330 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1331 | `		return PH7_OK;` |
|      - | 1332 | `	}` |
|      5 | 1333 | `	if( pFetched ){ EVP_MD_free(pFetched); }` |
|      5 | 1334 | `	ph7_result_string(pCtx,(const char *)zOut,(int)iKeyLen);` |
|      5 | 1335 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|      5 | 1336 | `	return PH7_OK;` |
|      6 | 1337 | `}` |
|      - | 1338 |  |
|      - | 1339 |  |
|      - | 1340 | `/* ------------------------------------------------------------------------` |
|      - | 1341 | ` * Keys` |
|      - | 1342 | ` * ------------------------------------------------------------------------ */` |
|      - | 1343 | `/*` |
|      - | 1344 | `` * A `$key` argument's four shapes, and what each means. php declares these`` |
|      - | 1345 | ` * parameters UNTYPED because there is no type that covers them:` |
|      - | 1346 | ` *` |
|      - | 1347 | ` *   an OpenSSLAsymmetricKey       -- the key itself` |
|      - | 1348 | ` *   an OpenSSLCertificate         -- its SUBJECT's public key (public doors only)` |
|      - | 1349 | `` *   a string                      -- PEM or DER bytes, or a `file://` path`` |
|      - | 1350 | ` *   an array [$key, $passphrase]  -- a private key and what unlocks it` |
|      - | 1351 | ` *` |
|      - | 1352 | ` * A certificate PEM is also accepted as a public key STRING, which is what` |
|      - | 1353 | `` * lets `openssl_verify($data, $sig, $certPem)` work -- the ordinary spelling`` |
|      - | 1354 | ` * in a program that has a certificate rather than a bare key.` |
|      - | 1355 | ` *` |
|      - | 1356 | ` * Answers 0 for anything it cannot read, having left OpenSSL's own reason in` |
|      - | 1357 | ` * the error ring; the CALLER supplies php's sentence, since the five doors` |
|      - | 1358 | ` * word it five different ways ("cannot be coerced into a private key", "key` |
|      - | 1359 | ` * parameter is not a valid public key", "Unable to coerce parameter 4 into a` |
|      - | 1360 | ` * private key", ...). *pbOwn says whether the caller frees it.` |
|      - | 1361 | ` */` |
|      - | 1362 | `/*` |
|      - | 1363 | ` * Does this key carry its PRIVATE half? php asks the key rather than` |
|      - | 1364 | ` * remembering how it was made, so a key read out of a private PEM is private` |
|      - | 1365 | ` * whichever door produced the object. The three probes are the three shapes a` |
|      - | 1366 | `` * private component takes: RSA's `d`, the finite-field/EC private BIGNUM, and`` |
|      - | 1367 | ` * the raw octet string an Edwards/Montgomery key keeps. Each miss pushes an` |
|      - | 1368 | ` * error, so the whole check runs between a MARK and a pop -- php's own answer` |
|      - | 1369 | ` * comes from inspecting the structure and leaves the error ring alone.` |
|      - | 1370 | ` */` |
|    142 | 1371 | `static int SslKeyIsPrivate(EVP_PKEY *pKey)` |
|      2 | 1372 | `{` |
|    144 | 1373 | `	BIGNUM *pBn = 0;` |
|    144 | 1374 | `	size_t nOct = 0;` |
|    144 | 1375 | `	int bPriv = 0;` |
|    144 | 1376 | `	if( pKey == 0 ){` |
|    ! 0 | 1377 | `		return 0;` |
|      - | 1378 | `	}` |
|    144 | 1379 | `	ERR_set_mark();` |
|    144 | 1380 | `	if( EVP_PKEY_get_bn_param(pKey,OSSL_PKEY_PARAM_RSA_D,&pBn) == 1 ){` |
|    130 | 1381 | `		bPriv = 1;` |
|     80 | 1382 | `	}else if( EVP_PKEY_get_bn_param(pKey,OSSL_PKEY_PARAM_PRIV_KEY,&pBn) == 1 ){` |
|     10 | 1383 | `		bPriv = 1;` |
|     11 | 1384 | `	}else if( EVP_PKEY_get_octet_string_param(pKey,OSSL_PKEY_PARAM_PRIV_KEY,0,0,&nOct) == 1 ){` |
|    ! 0 | 1385 | `		bPriv = 1;` |
|    ! 0 | 1386 | `	}` |
|    144 | 1387 | `	if( pBn ){` |
|    138 | 1388 | `		BN_clear_free(pBn);` |
|     68 | 1389 | `	}` |
|    144 | 1390 | `	ERR_pop_to_mark();` |
|    144 | 1391 | `	return bPriv;` |
|     73 | 1392 | `}` |
|    180 | 1393 | `static const char * SslFilePrefix(const char *zStr,int nStr,int *pnRest)` |
|      2 | 1394 | `{` |
|      - | 1395 | `	static const char zPfx[] = "file://";` |
|    182 | 1396 | `	int nPfx = (int)(sizeof(zPfx) - 1);` |
|    182 | 1397 | `	if( nStr <= nPfx \|\| SyStrnicmp(zStr,zPfx,(sxu32)nPfx) != 0 ){` |
|    180 | 1398 | `		return 0;` |
|      - | 1399 | `	}` |
|      3 | 1400 | `	*pnRest = nStr - nPfx;` |
|      3 | 1401 | `	return zStr + nPfx;` |
|     92 | 1402 | `}` |
|      - | 1403 | `/*` |
|      - | 1404 | ` * The bytes behind a string argument: either the argument itself or, for a` |
|      - | 1405 | `` * `file://` spelling, what that file holds. Reads through the engine's stream`` |
|      - | 1406 | ` * layer, so a userland wrapper and a phar both answer.` |
|      - | 1407 | ` */` |
|    180 | 1408 | `PH7_PRIVATE int PH7_SslBytesOfValue(ph7_context *pCtx,ph7_value *pVal,SyBlob *pOut,` |
|      - | 1409 | `	const char **pzData,int *pnData)` |
|      2 | 1410 | `{` |
|      - | 1411 | `	const char *zStr,*zPath;` |
|    182 | 1412 | `	int nStr = 0,nPath = 0;` |
|      - | 1413 | `	const ph7_io_stream *pStream;` |
|      - | 1414 | `	void *pHandle;` |
|    182 | 1415 | `	zStr = ph7_value_to_string(pVal,&nStr);` |
|    182 | 1416 | `	if( zStr == 0 ){` |
|    ! 0 | 1417 | `		zStr = "";` |
|    ! 0 | 1418 | `	}` |
|    182 | 1419 | `	zPath = SslFilePrefix(zStr,nStr,&nPath);` |
|    182 | 1420 | `	if( zPath == 0 ){` |
|    180 | 1421 | `		*pzData = zStr;` |
|    180 | 1422 | `		*pnData = nStr;` |
|    180 | 1423 | `		return 0;` |
|      - | 1424 | `	}` |
|      3 | 1425 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,nPath);` |
|      3 | 1426 | `	pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zPath,PH7_IO_OPEN_RDONLY,` |
|      1 | 1427 | `		FALSE,0,FALSE,0,0) : 0;` |
|      3 | 1428 | `	if( pHandle == 0 ){` |
|    ! 0 | 1429 | `		return -1;` |
|      - | 1430 | `	}` |
|      3 | 1431 | `	PH7_StreamReadWholeFile(pHandle,pStream,pOut);` |
|      3 | 1432 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      3 | 1433 | `	*pzData = (const char *)SyBlobData(pOut);` |
|      3 | 1434 | `	*pnData = (int)SyBlobLength(pOut);` |
|      3 | 1435 | `	if( *pzData == 0 ){` |
|    ! 0 | 1436 | `		*pzData = "";` |
|    ! 0 | 1437 | `	}` |
|      3 | 1438 | `	return 0;` |
|     92 | 1439 | `}` |
|      - | 1440 | `/* The nth VALUE of an array, in insertion order -- what php's positional` |
|      - | 1441 | `` * reads of `[$key, $passphrase]` and of openssl_seal()'s recipient list mean.`` |
|      - | 1442 | ` * The engine has no by-index array accessor, so the walk is spelled here. */` |
|      6 | 1443 | `static ph7_value * SslArrayNth(ph7_value *pArray,int n)` |
|      1 | 1444 | `{` |
|      - | 1445 | `	ph7_hashmap *pMap;` |
|      - | 1446 | `	ph7_hashmap_node *pEntry;` |
|      - | 1447 | `	sxu32 i;` |
|      7 | 1448 | `	if( pArray == 0 \|\| !ph7_value_is_array(pArray) \|\| n < 0 ){` |
|    ! 0 | 1449 | `		return 0;` |
|      - | 1450 | `	}` |
|      7 | 1451 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      7 | 1452 | `	pEntry = pMap->pFirst;` |
|      9 | 1453 | `	for( i = 0 ; i < pMap->nEntry ; ++i ){` |
|      9 | 1454 | `		if( (int)i == n ){` |
|      7 | 1455 | `			return HashmapExtractNodeValue(pEntry);` |
|      - | 1456 | `		}` |
|      3 | 1457 | `		pEntry = pEntry->pPrev;   /* forward walk (reverse link) */` |
|      2 | 1458 | `	}` |
|    ! 0 | 1459 | `	return 0;` |
|      4 | 1460 | `}` |
|      - | 1461 | `/*` |
|      - | 1462 | `` * The `_to_file` half of the five export doors. It writes through OpenSSL's`` |
|      - | 1463 | ` * own file BIO rather than the engine's stream layer, and that is php's choice` |
|      - | 1464 | ` * showing through rather than a shortcut: a path that cannot be opened leaves` |
|      - | 1465 | ``  * `system library::No such file or directory` and `BIO routines::no such file` `` |
|      - | 1466 | ` * in the error ring, which is what a script reads back. The cost is the same` |
|      - | 1467 | ` * one php pays -- these doors write to the filesystem and to nothing else, so` |
|      - | 1468 | `` * a `phar://` or userland-wrapper path is not a destination here.`` |
|      - | 1469 | ` */` |
|      6 | 1470 | `PH7_PRIVATE int PH7_SslWriteFileArg(ph7_context *pCtx,const char *zPath,int nPath,` |
|      - | 1471 | `	const void *pData,sxu32 nData)` |
|      2 | 1472 | `{` |
|      - | 1473 | `	char *zZ;` |
|      - | 1474 | `	BIO *pOut;` |
|      8 | 1475 | `	int rc = -1;` |
|      8 | 1476 | `	if( zPath == 0 \|\| nPath < 1 ){` |
|    ! 0 | 1477 | `		return -1;` |
|      - | 1478 | `	}` |
|      8 | 1479 | `	zZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPath + 1));` |
|      8 | 1480 | `	if( zZ == 0 ){` |
|    ! 0 | 1481 | `		return -1;` |
|      - | 1482 | `	}` |
|      8 | 1483 | `	SyMemcpy(zPath,zZ,(sxu32)nPath);` |
|      8 | 1484 | `	zZ[nPath] = 0;` |
|      8 | 1485 | `	pOut = BIO_new_file(zZ,"wb");` |
|      8 | 1486 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);` |
|      8 | 1487 | `	if( pOut == 0 ){` |
|    ! 0 | 1488 | `		return -1;` |
|      - | 1489 | `	}` |
|      8 | 1490 | `	if( nData == 0 \|\| BIO_write(pOut,pData,(int)nData) == (int)nData ){` |
|      8 | 1491 | `		rc = 0;` |
|      3 | 1492 | `	}` |
|      8 | 1493 | `	BIO_free(pOut);` |
|      8 | 1494 | `	return rc;` |
|      5 | 1495 | `}` |
|      - | 1496 | `/* Read a whole file through the engine's stream layer -- the certificate` |
|      - | 1497 | `` * unit's `$ca_info` paths and the PKCS#7 input files. */`` |
|    ! 0 | 1498 | `PH7_PRIVATE int PH7_SslReadFileArg(ph7_context *pCtx,const char *zPath,int nPath,SyBlob *pOut)` |
|    ! 0 | 1499 | `{` |
|      - | 1500 | `	const ph7_io_stream *pStream;` |
|      - | 1501 | `	void *pHandle;` |
|    ! 0 | 1502 | `	const char *zTarget = zPath;` |
|    ! 0 | 1503 | `	if( zPath == 0 \|\| nPath < 1 ){` |
|    ! 0 | 1504 | `		return -1;` |
|      - | 1505 | `	}` |
|    ! 0 | 1506 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zTarget,nPath);` |
|    ! 0 | 1507 | `	pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zTarget,PH7_IO_OPEN_RDONLY,` |
|    ! 0 | 1508 | `		FALSE,0,FALSE,0,0) : 0;` |
|    ! 0 | 1509 | `	if( pHandle == 0 ){` |
|    ! 0 | 1510 | `		return -1;` |
|      - | 1511 | `	}` |
|    ! 0 | 1512 | `	PH7_StreamReadWholeFile(pHandle,pStream,pOut);` |
|    ! 0 | 1513 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    ! 0 | 1514 | `	return 0;` |
|    ! 0 | 1515 | `}` |
|      - | 1516 | `/* One NUL-terminated copy of a passphrase, for OpenSSL's default password` |
|      - | 1517 | `` * callback (which takes `u` as a C string when no callback is given). */`` |
|    122 | 1518 | `static char * SslPassCopy(ph7_context *pCtx,const char *zPass,int nPass)` |
|      2 | 1519 | `{` |
|      - | 1520 | `	char *zBuf;` |
|    124 | 1521 | `	if( zPass == 0 \|\| nPass < 1 ){` |
|    114 | 1522 | `		return 0;` |
|      - | 1523 | `	}` |
|     12 | 1524 | `	zBuf = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPass + 1));` |
|     12 | 1525 | `	if( zBuf == 0 ){` |
|    ! 0 | 1526 | `		return 0;` |
|      - | 1527 | `	}` |
|     12 | 1528 | `	SyMemcpy(zPass,zBuf,(sxu32)nPass);` |
|     12 | 1529 | `	zBuf[nPass] = 0;` |
|     12 | 1530 | `	return zBuf;` |
|     63 | 1531 | `}` |
|      - | 1532 | `/*` |
|      - | 1533 | ` * The passphrase callback. It is ALWAYS installed, even when there is no` |
|      - | 1534 | ` * passphrase, and that is the point: OpenSSL's default callback PROMPTS on the` |
|      - | 1535 | ` * terminal when it is handed nothing, so an encrypted key opened without a` |
|      - | 1536 | ` * passphrase would stop a CLI program dead waiting for a human. php inherits` |
|      - | 1537 | ` * that prompt; this answers the empty string instead, which is the same` |
|      - | 1538 | `` * `false` php gives when its input is not a terminal.`` |
|      - | 1539 | ` */` |
|     12 | 1540 | `static int SslPassCb(char *zBuf,int nBuf,int rwflag,void *pUser)` |
|      2 | 1541 | `{` |
|     14 | 1542 | `	const char *zPass = (const char *)pUser;` |
|     14 | 1543 | `	int n = zPass ? (int)SyStrlen(zPass) : 0;` |
|      6 | 1544 | `	SXUNUSED(rwflag);` |
|     14 | 1545 | `	if( n > nBuf ){` |
|    ! 0 | 1546 | `		n = nBuf;` |
|    ! 0 | 1547 | `	}` |
|     14 | 1548 | `	if( n > 0 ){` |
|     12 | 1549 | `		SyMemcpy(zPass,zBuf,(sxu32)n);` |
|      5 | 1550 | `	}` |
|     14 | 1551 | `	return n;` |
|      2 | 1552 | `}` |
|    122 | 1553 | `static EVP_PKEY * SslKeyFromBytes(const char *zData,int nData,int bPublic,char *zPass)` |
|      2 | 1554 | `{` |
|      - | 1555 | `	BIO *pBio;` |
|    124 | 1556 | `	EVP_PKEY *pKey = 0;` |
|    124 | 1557 | `	if( nData < 1 ){` |
|      5 | 1558 | `		return 0;` |
|      - | 1559 | `	}` |
|    120 | 1560 | `	pBio = BIO_new_mem_buf(zData,nData);` |
|    120 | 1561 | `	if( pBio == 0 ){` |
|    ! 0 | 1562 | `		return 0;` |
|      - | 1563 | `	}` |
|      - | 1564 | `	/* PEM only, both halves: a string holding DER is` |
|      - | 1565 | ``	 * `error:1E08010C:DECODER routines::unsupported` and false under php, and`` |
|      - | 1566 | ``	 * a DER key reaches these doors through a `file://` path instead. */`` |
|    120 | 1567 | `	if( bPublic ){` |
|     78 | 1568 | `		pKey = PEM_read_bio_PUBKEY(pBio,0,0,0);` |
|     78 | 1569 | `		if( pKey == 0 ){` |
|      - | 1570 | `			/* A certificate is a public key a program HAS, so php reads one` |
|      - | 1571 | ``			 * here too -- `openssl_verify($d, $s, $certPem)` is the ordinary`` |
|      - | 1572 | `			 * spelling in a program that was handed a certificate. */` |
|      - | 1573 | `			X509 *pCert;` |
|     14 | 1574 | `			ERR_set_mark();` |
|     14 | 1575 | `			BIO_reset(pBio);` |
|     14 | 1576 | `			pCert = PEM_read_bio_X509(pBio,0,0,0);` |
|     14 | 1577 | `			if( pCert ){` |
|      3 | 1578 | `				ERR_clear_last_mark();` |
|      3 | 1579 | `				pKey = X509_get_pubkey(pCert);` |
|      3 | 1580 | `				X509_free(pCert);` |
|      2 | 1581 | `			}else{` |
|     11 | 1582 | `				ERR_pop_to_mark();` |
|      - | 1583 | `			}` |
|      6 | 1584 | `		}` |
|     40 | 1585 | `	}else{` |
|     44 | 1586 | `		pKey = PEM_read_bio_PrivateKey(pBio,0,SslPassCb,(void *)zPass);` |
|      - | 1587 | `	}` |
|    120 | 1588 | `	BIO_free(pBio);` |
|    120 | 1589 | `	return pKey;` |
|     63 | 1590 | `}` |
|    276 | 1591 | `PH7_PRIVATE EVP_PKEY * PH7_SslKeyOfValue(ph7_context *pCtx,ph7_value *pVal,int bPublic,int *pbOwn)` |
|      2 | 1592 | `{` |
|      - | 1593 | `	SyBlob sFile;` |
|    278 | 1594 | `	const char *zData = 0,*zPass = 0;` |
|    278 | 1595 | `	int nData = 0,nPass = 0,rc;` |
|    278 | 1596 | `	char *zPassZ = 0;` |
|    278 | 1597 | `	EVP_PKEY *pKey = 0;` |
|    278 | 1598 | `	ph7_value *pKeyVal = pVal;` |
|    278 | 1599 | `	*pbOwn = 0;` |
|    278 | 1600 | `	if( pVal == 0 ){` |
|    ! 0 | 1601 | `		return 0;` |
|      - | 1602 | `	}` |
|    278 | 1603 | `	if( ph7_value_is_object(pVal) ){` |
|    148 | 1604 | `		void *pHandle = PH7_SslHandleOf(pVal,PHL_SSL_KIND_KEY);` |
|    148 | 1605 | `		if( pHandle ){` |
|      - | 1606 | `			/* A key OBJECT carries its half with it, and php checks: a private` |
|      - | 1607 | `			 * key at a public door and a public key at a private one are both` |
|      - | 1608 | `			 * refusals, each with its own sentence, and each is followed by` |
|      - | 1609 | `			 * the CALLER's own. A key STRING gets neither message -- it simply` |
|      - | 1610 | `			 * fails to parse -- which is why these two live here rather than` |
|      - | 1611 | `			 * in the byte reader. */` |
|    144 | 1612 | `			int bPriv = SslKeyIsPrivate((EVP_PKEY *)pHandle);` |
|    144 | 1613 | `			if( bPublic && bPriv ){` |
|     11 | 1614 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1615 | `					"Don't know how to get public key from this private key");` |
|     11 | 1616 | `				return 0;` |
|      - | 1617 | `			}` |
|    134 | 1618 | `			if( !bPublic && !bPriv ){` |
|      3 | 1619 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1620 | `					"Supplied key param is a public key");` |
|      3 | 1621 | `				return 0;` |
|      - | 1622 | `			}` |
|    132 | 1623 | `			return (EVP_PKEY *)pHandle;` |
|      - | 1624 | `		}` |
|      5 | 1625 | `		pHandle = PH7_SslHandleOf(pVal,PHL_SSL_KIND_CERT);` |
|      5 | 1626 | `		if( pHandle && bPublic ){` |
|      5 | 1627 | `			EVP_PKEY *pPub = X509_get_pubkey((X509 *)pHandle);` |
|      5 | 1628 | `			if( pPub ){` |
|      5 | 1629 | `				*pbOwn = 1;` |
|      2 | 1630 | `			}` |
|      5 | 1631 | `			return pPub;` |
|      - | 1632 | `		}` |
|    ! 0 | 1633 | `		return 0;` |
|      - | 1634 | `	}` |
|    132 | 1635 | `	if( ph7_value_is_array(pVal) ){` |
|      - | 1636 | `		/* php's [$key, $passphrase] pair, and it is a SHAPE rather than a` |
|      - | 1637 | ``		 * position: the keys 0 and 1 must both be there. An `['key' => …,`` |
|      - | 1638 | ``		 * 'pass' => …]` spelling is `ValueError: Key array must be of the form`` |
|      - | 1639 | ``		 * array(0 => key, 1 => phrase)` -- a bare sentence with no function`` |
|      - | 1640 | `		 * name in front of it, unlike every other diagnostic here -- and so is` |
|      - | 1641 | `		 * an array of one. A THIRD element is ignored. The check runs before` |
|      - | 1642 | `		 * the public/private split, so a pair handed to a public door throws` |
|      - | 1643 | `		 * the same thing rather than being refused as "not a public key". */` |
|     20 | 1644 | `		ph7_value *pFirst = ph7_array_fetch(pVal,"0",-1);` |
|     20 | 1645 | `		ph7_value *pSecond = ph7_array_fetch(pVal,"1",-1);` |
|     20 | 1646 | `		if( pFirst == 0 \|\| pSecond == 0 ){` |
|     13 | 1647 | `			*pbOwn = -1;   /* the caller raises php's ValueError */` |
|     13 | 1648 | `			return 0;` |
|      - | 1649 | `		}` |
|      8 | 1650 | `		if( bPublic ){` |
|    ! 0 | 1651 | `			return 0;` |
|      - | 1652 | `		}` |
|      8 | 1653 | `		if( ph7_value_is_object(pFirst) ){` |
|    ! 0 | 1654 | `			return PH7_SslKeyOfValue(pCtx,pFirst,bPublic,pbOwn);` |
|      - | 1655 | `		}` |
|      8 | 1656 | `		if( !ph7_value_is_null(pSecond) ){` |
|      8 | 1657 | `			zPass = ph7_value_to_string(pSecond,&nPass);` |
|      3 | 1658 | `		}` |
|      8 | 1659 | `		pKeyVal = pFirst;` |
|      3 | 1660 | `	}` |
|    120 | 1661 | `	if( !ph7_value_is_string(pKeyVal) ){` |
|    ! 0 | 1662 | `		return 0;` |
|      - | 1663 | `	}` |
|    120 | 1664 | `	SyBlobInit(&sFile,&pCtx->pVm->sAllocator);` |
|    120 | 1665 | `	rc = PH7_SslBytesOfValue(pCtx,pKeyVal,&sFile,&zData,&nData);` |
|    120 | 1666 | `	if( rc == 0 ){` |
|    120 | 1667 | `		zPassZ = SslPassCopy(pCtx,zPass,nPass);` |
|    120 | 1668 | `		pKey = SslKeyFromBytes(zData,nData,bPublic,zPassZ);` |
|    120 | 1669 | `		if( zPassZ ){` |
|      8 | 1670 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,zPassZ);` |
|      3 | 1671 | `		}` |
|     59 | 1672 | `	}` |
|    120 | 1673 | `	SyBlobRelease(&sFile);` |
|    120 | 1674 | `	if( pKey ){` |
|     88 | 1675 | `		*pbOwn = 1;` |
|     45 | 1676 | `	}else{` |
|     34 | 1677 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      - | 1678 | `	}` |
|    120 | 1679 | `	return pKey;` |
|    140 | 1680 | `}` |
|      - | 1681 | ``/* The private-key door with php's separate `$passphrase` argument (rather than`` |
|      - | 1682 | ` * the array pair): openssl_pkey_get_private() and openssl_pkey_export(). */` |
|     42 | 1683 | `static EVP_PKEY * SslPrivateKeyWithPass(ph7_context *pCtx,ph7_value *pVal,` |
|      - | 1684 | `	const char *zPass,int nPass,int *pbOwn)` |
|      2 | 1685 | `{` |
|      - | 1686 | `	SyBlob sFile;` |
|     44 | 1687 | `	const char *zData = 0;` |
|      - | 1688 | `	char *zPassZ;` |
|     44 | 1689 | `	EVP_PKEY *pKey = 0;` |
|     44 | 1690 | `	int nData = 0;` |
|     44 | 1691 | `	*pbOwn = 0;` |
|     44 | 1692 | `	if( pVal == 0 ){` |
|    ! 0 | 1693 | `		return 0;` |
|      - | 1694 | `	}` |
|     44 | 1695 | `	if( ph7_value_is_object(pVal) \|\| ph7_value_is_array(pVal) \|\| zPass == 0 \|\| nPass < 1 ){` |
|     40 | 1696 | `		return PH7_SslKeyOfValue(pCtx,pVal,0,pbOwn);` |
|      - | 1697 | `	}` |
|      5 | 1698 | `	if( !ph7_value_is_string(pVal) ){` |
|    ! 0 | 1699 | `		return 0;` |
|      - | 1700 | `	}` |
|      5 | 1701 | `	SyBlobInit(&sFile,&pCtx->pVm->sAllocator);` |
|      5 | 1702 | `	if( PH7_SslBytesOfValue(pCtx,pVal,&sFile,&zData,&nData) == 0 ){` |
|      5 | 1703 | `		zPassZ = SslPassCopy(pCtx,zPass,nPass);` |
|      5 | 1704 | `		pKey = SslKeyFromBytes(zData,nData,0,zPassZ);` |
|      5 | 1705 | `		if( zPassZ ){` |
|      5 | 1706 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,zPassZ);` |
|      2 | 1707 | `		}` |
|      2 | 1708 | `	}` |
|      5 | 1709 | `	SyBlobRelease(&sFile);` |
|      5 | 1710 | `	if( pKey ){` |
|      3 | 1711 | `		*pbOwn = 1;` |
|      2 | 1712 | `	}else{` |
|      3 | 1713 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      - | 1714 | `	}` |
|      5 | 1715 | `	return pKey;` |
|     23 | 1716 | `}` |
|    148 | 1717 | `static void SslReleaseKey(EVP_PKEY *pKey,int bOwn)` |
|      2 | 1718 | `{` |
|    150 | 1719 | `	if( pKey && bOwn > 0 ){` |
|     62 | 1720 | `		EVP_PKEY_free(pKey);` |
|     30 | 1721 | `	}` |
|    150 | 1722 | `}` |
|      - | 1723 | `/* php's bare ValueError for a malformed key pair. Every door that takes a` |
|      - | 1724 | `` * `$key` reports it, and none of them puts its own name in front. */`` |
|     12 | 1725 | `PH7_PRIVATE int PH7_SslArrayShapeError(ph7_context *pCtx)` |
|      1 | 1726 | `{` |
|     13 | 1727 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1728 | `		"Key array must be of the form array(0 => key, 1 => phrase)");` |
|      1 | 1729 | `}` |
|      - | 1730 | `/* ---- openssl_pkey_new() ---- */` |
|      - | 1731 | `/*` |
|      - | 1732 | ` * php's key TYPE numbering is its own (OPENSSL_KEYTYPE_*), and the four` |
|      - | 1733 | ` * generated types map onto OpenSSL's names. The four that only appear in` |
|      - | 1734 | ` * get_details -- X25519, ED25519, X448, ED448 -- are read but never` |
|      - | 1735 | `` * GENERATED by php's `private_key_type`, which is why they are absent here.`` |
|      - | 1736 | ` */` |
|     32 | 1737 | `static int SslKeyTypeOf(EVP_PKEY *pKey)` |
|      2 | 1738 | `{` |
|     34 | 1739 | `	switch( EVP_PKEY_get_base_id(pKey) ){` |
|      6 | 1740 | `		case EVP_PKEY_RSA:` |
|     14 | 1741 | `		case EVP_PKEY_RSA_PSS: return 0;   /* OPENSSL_KEYTYPE_RSA */` |
|    ! 0 | 1742 | `		case EVP_PKEY_DSA:     return 1;` |
|    ! 0 | 1743 | `		case EVP_PKEY_DH:      return 2;` |
|     14 | 1744 | `		case EVP_PKEY_EC:      return 3;` |
|      3 | 1745 | `		case EVP_PKEY_X25519:  return 4;` |
|      3 | 1746 | `		case EVP_PKEY_ED25519: return 5;` |
|      3 | 1747 | `		case EVP_PKEY_X448:    return 6;` |
|      3 | 1748 | `		case EVP_PKEY_ED448:   return 7;` |
|    ! 0 | 1749 | `		default:               return -1;` |
|      - | 1750 | `	}` |
|     18 | 1751 | `}` |
|      - | 1752 | `/* One BIGNUM out of an options sub-array, as php reads it: a binary string in` |
|      - | 1753 | ` * network byte order. */` |
|     38 | 1754 | `static BIGNUM * SslBnFromArray(ph7_value *pArray,const char *zKey)` |
|      1 | 1755 | `{` |
|     39 | 1756 | `	ph7_value *pVal = pArray ? ph7_array_fetch(pArray,zKey,-1) : 0;` |
|      - | 1757 | `	const char *zRaw;` |
|     39 | 1758 | `	int nRaw = 0;` |
|     39 | 1759 | `	if( pVal == 0 \|\| !ph7_value_is_string(pVal) ){` |
|     13 | 1760 | `		return 0;` |
|      - | 1761 | `	}` |
|     27 | 1762 | `	zRaw = ph7_value_to_string(pVal,&nRaw);` |
|     27 | 1763 | `	if( zRaw == 0 \|\| nRaw < 1 ){` |
|    ! 0 | 1764 | `		return 0;` |
|      - | 1765 | `	}` |
|     27 | 1766 | `	return BN_bin2bn((const unsigned char *)zRaw,nRaw,0);` |
|     20 | 1767 | `}` |
|     22 | 1768 | `static int SslBldPush(OSSL_PARAM_BLD *pBld,const char *zName,BIGNUM *pBn)` |
|      1 | 1769 | `{` |
|     23 | 1770 | `	return pBn ? OSSL_PARAM_BLD_push_BN(pBld,zName,pBn) : 0;` |
|      1 | 1771 | `}` |
|      - | 1772 | `/*` |
|      - | 1773 | `` * A key built from its COMPONENTS -- `openssl_pkey_new(['rsa' => [...]])` and`` |
|      - | 1774 | ` * its three siblings. php requires the PRIVATE half in every one of them: an` |
|      - | 1775 | `` * `['rsa' => ['n' => …, 'e' => …]]` with no `d` is a flat false rather than a`` |
|      - | 1776 | ` * public key, which is the one thing a caller who reads the manual would get` |
|      - | 1777 | ` * wrong.` |
|      - | 1778 | ` */` |
|     48 | 1779 | `static EVP_PKEY * SslKeyFromComponents(ph7_context *pCtx,ph7_value *pOpts)` |
|      2 | 1780 | `{` |
|      - | 1781 | `	static const struct { const char *zGroup; const char *zType; } aKind[] = {` |
|      - | 1782 | `		{ "rsa", "RSA" }, { "dsa", "DSA" }, { "dh", "DH" }, { "ec", "EC" }` |
|      - | 1783 | `	};` |
|     50 | 1784 | `	ph7_value *pGroup = 0;` |
|     50 | 1785 | `	const char *zType = 0;` |
|      - | 1786 | `	OSSL_PARAM_BLD *pBld;` |
|     50 | 1787 | `	OSSL_PARAM *pParams = 0;` |
|     50 | 1788 | `	EVP_PKEY_CTX *pKCtx = 0;` |
|     50 | 1789 | `	EVP_PKEY *pKey = 0;` |
|      - | 1790 | `	BIGNUM *aBn[8];` |
|     50 | 1791 | `	int nBn = 0,i,bOk = 0;` |
|     50 | 1792 | `	unsigned char *zPoint = 0;` |
|    224 | 1793 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(aKind) ; ++i ){` |
|    182 | 1794 | `		pGroup = ph7_array_fetch(pOpts,aKind[i].zGroup,-1);` |
|    182 | 1795 | `		if( pGroup && ph7_value_is_array(pGroup) ){` |
|      7 | 1796 | `			zType = aKind[i].zType;` |
|      7 | 1797 | `			break;` |
|      - | 1798 | `		}` |
|    176 | 1799 | `		pGroup = 0;` |
|     89 | 1800 | `	}` |
|     50 | 1801 | `	if( zType == 0 ){` |
|     44 | 1802 | `		return 0;` |
|      - | 1803 | `	}` |
|      7 | 1804 | `	pBld = OSSL_PARAM_BLD_new();` |
|      7 | 1805 | `	if( pBld == 0 ){` |
|    ! 0 | 1806 | `		return 0;` |
|      - | 1807 | `	}` |
|      7 | 1808 | `	SyZero(aBn,sizeof(aBn));` |
|      7 | 1809 | `	if( SslStrCmp(zType,"RSA") == 0 ){` |
|      - | 1810 | `		static const struct { const char *zPhp; const char *zParam; int bReq; } aRsa[] = {` |
|      - | 1811 | `			{ "n",    OSSL_PKEY_PARAM_RSA_N,           1 },` |
|      - | 1812 | `			{ "e",    OSSL_PKEY_PARAM_RSA_E,           1 },` |
|      - | 1813 | `			{ "d",    OSSL_PKEY_PARAM_RSA_D,           1 },` |
|      - | 1814 | `			{ "p",    OSSL_PKEY_PARAM_RSA_FACTOR1,     0 },` |
|      - | 1815 | `			{ "q",    OSSL_PKEY_PARAM_RSA_FACTOR2,     0 },` |
|      - | 1816 | `			{ "dmp1", OSSL_PKEY_PARAM_RSA_EXPONENT1,   0 },` |
|      - | 1817 | `			{ "dmq1", OSSL_PKEY_PARAM_RSA_EXPONENT2,   0 },` |
|      - | 1818 | `			{ "iqmp", OSSL_PKEY_PARAM_RSA_COEFFICIENT1,0 }` |
|      - | 1819 | `		};` |
|      5 | 1820 | `		bOk = 1;` |
|     37 | 1821 | `		for( i = 0 ; i < (int)SX_ARRAYSIZE(aRsa) ; ++i ){` |
|     33 | 1822 | `			BIGNUM *pBn = SslBnFromArray(pGroup,aRsa[i].zPhp);` |
|     33 | 1823 | `			if( pBn == 0 ){` |
|     13 | 1824 | `				if( aRsa[i].bReq ){ bOk = 0; }` |
|     13 | 1825 | `				continue;` |
|      - | 1826 | `			}` |
|     21 | 1827 | `			aBn[nBn++] = pBn;` |
|     21 | 1828 | `			if( !SslBldPush(pBld,aRsa[i].zParam,pBn) ){ bOk = 0; }` |
|     11 | 1829 | `		}` |
|      5 | 1830 | `	}else if( SslStrCmp(zType,"EC") == 0 ){` |
|      3 | 1831 | `		ph7_value *pName = ph7_array_fetch(pGroup,"curve_name",-1);` |
|      3 | 1832 | `		BIGNUM *pX = SslBnFromArray(pGroup,"x");` |
|      3 | 1833 | `		BIGNUM *pY = SslBnFromArray(pGroup,"y");` |
|      3 | 1834 | `		BIGNUM *pD = SslBnFromArray(pGroup,"d");` |
|      3 | 1835 | `		int nCurve = 0;` |
|      3 | 1836 | `		const char *zCurve = pName ? ph7_value_to_string(pName,&nCurve) : 0;` |
|      3 | 1837 | `		if( pX ){ aBn[nBn++] = pX; }` |
|      3 | 1838 | `		if( pY ){ aBn[nBn++] = pY; }` |
|      3 | 1839 | `		if( pD ){ aBn[nBn++] = pD; }` |
|      3 | 1840 | `		if( zCurve && nCurve > 0 ){` |
|      3 | 1841 | `			bOk = OSSL_PARAM_BLD_push_utf8_string(pBld,OSSL_PKEY_PARAM_GROUP_NAME,zCurve,(size_t)nCurve);` |
|      1 | 1842 | `		}` |
|      3 | 1843 | `		if( bOk && pD ){` |
|      3 | 1844 | `			bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_PRIV_KEY,pD);` |
|      2 | 1845 | `		}else{` |
|    ! 0 | 1846 | `			bOk = 0;` |
|      - | 1847 | `		}` |
|      3 | 1848 | `		if( bOk && pX && pY ){` |
|      - | 1849 | `			/* An EC public key is the ENCODED POINT rather than a pair of` |
|      - | 1850 | `			 * numbers: 0x04 followed by X and Y, each padded to the field's` |
|      - | 1851 | `			 * width. */` |
|      3 | 1852 | `			EC_GROUP *pGrp = EC_GROUP_new_by_curve_name(OBJ_sn2nid(zCurve));` |
|      3 | 1853 | `			int nField = pGrp ? (EC_GROUP_get_degree(pGrp) + 7) / 8 : 0;` |
|      3 | 1854 | `			if( nField > 0 ){` |
|      4 | 1855 | `				zPoint = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|      2 | 1856 | `					(sxu32)(1 + 2 * nField));` |
|      3 | 1857 | `				if( zPoint ){` |
|      3 | 1858 | `					SyZero(zPoint,(sxu32)(1 + 2 * nField));` |
|      3 | 1859 | `					zPoint[0] = 0x04;` |
|      3 | 1860 | `					BN_bn2binpad(pX,zPoint + 1,nField);` |
|      3 | 1861 | `					BN_bn2binpad(pY,zPoint + 1 + nField,nField);` |
|      4 | 1862 | `					bOk = OSSL_PARAM_BLD_push_octet_string(pBld,OSSL_PKEY_PARAM_PUB_KEY,` |
|      2 | 1863 | `						zPoint,(size_t)(1 + 2 * nField));` |
|      2 | 1864 | `				}else{` |
|    ! 0 | 1865 | `					bOk = 0;` |
|      - | 1866 | `				}` |
|      2 | 1867 | `			}else{` |
|    ! 0 | 1868 | `				bOk = 0;` |
|      - | 1869 | `			}` |
|      3 | 1870 | `			if( pGrp ){` |
|      3 | 1871 | `				EC_GROUP_free(pGrp);` |
|      1 | 1872 | `			}` |
|      1 | 1873 | `		}` |
|      2 | 1874 | `	}else{` |
|      - | 1875 | `		/* DSA and DH share the finite-field parameter names; DH has no q. */` |
|    ! 0 | 1876 | `		BIGNUM *pP = SslBnFromArray(pGroup,"p");` |
|    ! 0 | 1877 | `		BIGNUM *pQ = SslBnFromArray(pGroup,"q");` |
|    ! 0 | 1878 | `		BIGNUM *pG = SslBnFromArray(pGroup,"g");` |
|    ! 0 | 1879 | `		BIGNUM *pPriv = SslBnFromArray(pGroup,"priv_key");` |
|    ! 0 | 1880 | `		BIGNUM *pPub = SslBnFromArray(pGroup,"pub_key");` |
|    ! 0 | 1881 | `		if( pP ){ aBn[nBn++] = pP; }` |
|    ! 0 | 1882 | `		if( pQ ){ aBn[nBn++] = pQ; }` |
|    ! 0 | 1883 | `		if( pG ){ aBn[nBn++] = pG; }` |
|    ! 0 | 1884 | `		if( pPriv ){ aBn[nBn++] = pPriv; }` |
|    ! 0 | 1885 | `		if( pPub ){ aBn[nBn++] = pPub; }` |
|    ! 0 | 1886 | `		bOk = pP && pG && pPriv;` |
|    ! 0 | 1887 | `		if( bOk ){ bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_FFC_P,pP); }` |
|    ! 0 | 1888 | `		if( bOk && pQ ){ bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_FFC_Q,pQ); }` |
|    ! 0 | 1889 | `		if( bOk ){ bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_FFC_G,pG); }` |
|    ! 0 | 1890 | `		if( bOk ){ bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_PRIV_KEY,pPriv); }` |
|    ! 0 | 1891 | `		if( bOk && pPub ){ bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_PUB_KEY,pPub); }` |
|      - | 1892 | `	}` |
|      7 | 1893 | `	if( bOk ){` |
|      5 | 1894 | `		pParams = OSSL_PARAM_BLD_to_param(pBld);` |
|      2 | 1895 | `	}` |
|      7 | 1896 | `	if( pParams ){` |
|      5 | 1897 | `		pKCtx = EVP_PKEY_CTX_new_from_name(0,zType,0);` |
|      2 | 1898 | `	}` |
|      7 | 1899 | `	if( pKCtx && EVP_PKEY_fromdata_init(pKCtx) > 0 ){` |
|      5 | 1900 | `		if( EVP_PKEY_fromdata(pKCtx,&pKey,EVP_PKEY_KEYPAIR,pParams) <= 0 ){` |
|    ! 0 | 1901 | `			pKey = 0;` |
|    ! 0 | 1902 | `		}` |
|      2 | 1903 | `	}` |
|      7 | 1904 | `	if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }` |
|      7 | 1905 | `	if( pParams ){ OSSL_PARAM_free(pParams); }` |
|      7 | 1906 | `	OSSL_PARAM_BLD_free(pBld);` |
|     33 | 1907 | `	for( i = 0 ; i < nBn ; ++i ){ BN_free(aBn[i]); }` |
|      7 | 1908 | `	if( zPoint ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zPoint); }` |
|      7 | 1909 | `	if( pKey == 0 ){` |
|      3 | 1910 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      1 | 1911 | `	}` |
|      7 | 1912 | `	return pKey;` |
|     26 | 1913 | `}` |
|      - | 1914 | `/*` |
|      - | 1915 | `` * php's `private_key_type` covers all EIGHT of its OPENSSL_KEYTYPE_* numbers,`` |
|      - | 1916 | ` * not just the four a reading of the manual suggests: 4..7 are X25519,` |
|      - | 1917 | ` * ED25519, X448 and ED448, and each generates without a bit length or a curve.` |
|      - | 1918 | `` * Anything outside 0..7 is `Unsupported private key type`.`` |
|      - | 1919 | ` *` |
|      - | 1920 | ` * The three FAMILIES need three different generation shapes, and getting that` |
|      - | 1921 | ` * wrong is a silent false rather than an error: RSA takes its size as a keygen` |
|      - | 1922 | ` * parameter, DSA and DH need their DOMAIN PARAMETERS generated first and the` |
|      - | 1923 | ` * key generated from those, and EC/Edwards take a group name or nothing at all.` |
|      - | 1924 | ` */` |
|     42 | 1925 | `static const char * SslKeyTypeName(sxi64 iType)` |
|      2 | 1926 | `{` |
|     44 | 1927 | `	switch( iType ){` |
|     20 | 1928 | `		case 0: return "RSA";` |
|    ! 0 | 1929 | `		case 1: return "DSA";` |
|    ! 0 | 1930 | `		case 2: return "DH";` |
|     14 | 1931 | `		case 3: return "EC";` |
|      3 | 1932 | `		case 4: return "X25519";` |
|      3 | 1933 | `		case 5: return "ED25519";` |
|      3 | 1934 | `		case 6: return "X448";` |
|      3 | 1935 | `		case 7: return "ED448";` |
|      5 | 1936 | `		default: return 0;` |
|      - | 1937 | `	}` |
|     23 | 1938 | `}` |
|    ! 0 | 1939 | `static EVP_PKEY * SslGenFfcKey(const char *zType,int nBits)` |
|    ! 0 | 1940 | `{` |
|      - | 1941 | `	EVP_PKEY_CTX *pCtx1,*pCtx2;` |
|    ! 0 | 1942 | `	EVP_PKEY *pParams = 0,*pKey = 0;` |
|      - | 1943 | `	OSSL_PARAM aP[2];` |
|    ! 0 | 1944 | `	unsigned int nB = (unsigned int)nBits;` |
|    ! 0 | 1945 | `	pCtx1 = EVP_PKEY_CTX_new_from_name(0,zType,0);` |
|    ! 0 | 1946 | `	if( pCtx1 == 0 ){` |
|    ! 0 | 1947 | `		return 0;` |
|      - | 1948 | `	}` |
|    ! 0 | 1949 | `	aP[0] = OSSL_PARAM_construct_uint(OSSL_PKEY_PARAM_FFC_PBITS,&nB);` |
|    ! 0 | 1950 | `	aP[1] = OSSL_PARAM_construct_end();` |
|    ! 0 | 1951 | `	if( EVP_PKEY_paramgen_init(pCtx1) <= 0` |
|    ! 0 | 1952 | `	 \|\| EVP_PKEY_CTX_set_params(pCtx1,aP) <= 0` |
|    ! 0 | 1953 | `	 \|\| EVP_PKEY_paramgen(pCtx1,&pParams) <= 0 ){` |
|    ! 0 | 1954 | `		EVP_PKEY_CTX_free(pCtx1);` |
|    ! 0 | 1955 | `		return 0;` |
|      - | 1956 | `	}` |
|    ! 0 | 1957 | `	EVP_PKEY_CTX_free(pCtx1);` |
|    ! 0 | 1958 | `	pCtx2 = EVP_PKEY_CTX_new(pParams,0);` |
|    ! 0 | 1959 | `	if( pCtx2 && EVP_PKEY_keygen_init(pCtx2) > 0 ){` |
|    ! 0 | 1960 | `		if( EVP_PKEY_keygen(pCtx2,&pKey) <= 0 ){` |
|    ! 0 | 1961 | `			pKey = 0;` |
|    ! 0 | 1962 | `		}` |
|    ! 0 | 1963 | `	}` |
|    ! 0 | 1964 | `	if( pCtx2 ){ EVP_PKEY_CTX_free(pCtx2); }` |
|    ! 0 | 1965 | `	EVP_PKEY_free(pParams);` |
|    ! 0 | 1966 | `	return pKey;` |
|    ! 0 | 1967 | `}` |
|      - | 1968 | `/*` |
|      - | 1969 | ` * The generator behind openssl_pkey_new() and openssl_csr_new()'s "no key` |
|      - | 1970 | ` * given" path, which php serves from the same code. Raises php's own warning` |
|      - | 1971 | ` * for a refused size, type or curve and answers 0.` |
|      - | 1972 | ` */` |
|     42 | 1973 | `PH7_PRIVATE EVP_PKEY * PH7_SslGenerateKey(ph7_context *pCtx,sxi64 iBits,sxi64 iType,` |
|      - | 1974 | `	const char *zCurve,int nCurve)` |
|      2 | 1975 | `{` |
|     44 | 1976 | `	EVP_PKEY_CTX *pKCtx = 0;` |
|     44 | 1977 | `	EVP_PKEY *pKey = 0;` |
|     44 | 1978 | `	const char *zType = SslKeyTypeName(iType);` |
|     44 | 1979 | `	if( zType == 0 ){` |
|      5 | 1980 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unsupported private key type");` |
|      5 | 1981 | `		return 0;` |
|      - | 1982 | `	}` |
|     40 | 1983 | `	if( iType <= 2 && iBits < 384 ){` |
|      4 | 1984 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 1985 | `			"Private key length must be at least 384 bits, configured to %d",(int)iBits);` |
|      3 | 1986 | `		return 0;` |
|      - | 1987 | `	}` |
|     38 | 1988 | `	if( iType == 3 ){` |
|      - | 1989 | `		int nid;` |
|     14 | 1990 | `		if( zCurve == 0 \|\| nCurve < 1 ){` |
|      - | 1991 | `			/* php has no default curve: the option is REQUIRED and its absence` |
|      - | 1992 | `			 * is a configuration complaint rather than a fallback. */` |
|      3 | 1993 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1994 | `				"Missing configuration value: \"curve_name\" not set");` |
|      3 | 1995 | `			return 0;` |
|      - | 1996 | `		}` |
|     12 | 1997 | `		nid = OBJ_sn2nid(zCurve);` |
|     12 | 1998 | `		if( nid == NID_undef ){` |
|      4 | 1999 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 2000 | `				"Unknown elliptic curve (short) name %.*s",nCurve,zCurve);` |
|      3 | 2001 | `			return 0;` |
|      - | 2002 | `		}` |
|     10 | 2003 | `		pKCtx = EVP_PKEY_CTX_new_from_name(0,"EC",0);` |
|      8 | 2004 | `		if( pKCtx && EVP_PKEY_keygen_init(pKCtx) > 0` |
|     10 | 2005 | `		 && EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pKCtx,nid) > 0 ){` |
|     10 | 2006 | `			if( EVP_PKEY_keygen(pKCtx,&pKey) <= 0 ){` |
|    ! 0 | 2007 | `				pKey = 0;` |
|    ! 0 | 2008 | `			}` |
|      4 | 2009 | `		}` |
|     10 | 2010 | `		if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }` |
|     10 | 2011 | `		return pKey;` |
|      - | 2012 | `	}` |
|     26 | 2013 | `	if( iType == 1 \|\| iType == 2 ){` |
|    ! 0 | 2014 | `		return SslGenFfcKey(zType,(int)iBits);` |
|      - | 2015 | `	}` |
|     26 | 2016 | `	pKCtx = EVP_PKEY_CTX_new_from_name(0,zType,0);` |
|     26 | 2017 | `	if( pKCtx && EVP_PKEY_keygen_init(pKCtx) > 0 ){` |
|     26 | 2018 | `		if( iType == 0 ){` |
|     18 | 2019 | `			EVP_PKEY_CTX_set_rsa_keygen_bits(pKCtx,(int)iBits);` |
|      8 | 2020 | `		}` |
|     26 | 2021 | `		if( EVP_PKEY_keygen(pKCtx,&pKey) <= 0 ){` |
|    ! 0 | 2022 | `			pKey = 0;` |
|    ! 0 | 2023 | `		}` |
|     12 | 2024 | `	}` |
|     26 | 2025 | `	if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }` |
|     26 | 2026 | `	return pKey;` |
|     23 | 2027 | `}` |
|     48 | 2028 | `static int vm_builtin_openssl_pkey_new(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2029 | `{` |
|     50 | 2030 | `	ph7_value *pOpts = 0,*pVal;` |
|     50 | 2031 | `	EVP_PKEY *pKey = 0;` |
|     50 | 2032 | `	sxi64 iBits = 2048,iType = 0;` |
|     50 | 2033 | `	const char *zCurve = 0;` |
|     50 | 2034 | `	int nCurve = 0;` |
|     50 | 2035 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|     50 | 2036 | `		pOpts = apArg[0];` |
|     24 | 2037 | `	}` |
|     50 | 2038 | `	if( pOpts ){` |
|     50 | 2039 | `		pKey = SslKeyFromComponents(pCtx,pOpts);` |
|     50 | 2040 | `		if( pKey ){` |
|      5 | 2041 | `			return PH7_SslResultObject(pCtx,PHL_SSL_KIND_KEY,(void *)pKey);` |
|      - | 2042 | `		}` |
|     44 | 2043 | `		if( ph7_array_fetch(pOpts,"rsa",-1) \|\| ph7_array_fetch(pOpts,"dsa",-1)` |
|     44 | 2044 | `		 \|\| ph7_array_fetch(pOpts,"dh",-1) \|\| ph7_array_fetch(pOpts,"ec",-1) ){` |
|      3 | 2045 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2046 | `			return PH7_OK;` |
|      - | 2047 | `		}` |
|     44 | 2048 | `		pVal = ph7_array_fetch(pOpts,"private_key_bits",-1);` |
|     44 | 2049 | `		if( pVal ){` |
|     20 | 2050 | `			iBits = ph7_value_to_int64(pVal);` |
|      9 | 2051 | `		}` |
|     44 | 2052 | `		pVal = ph7_array_fetch(pOpts,"private_key_type",-1);` |
|     44 | 2053 | `		if( pVal ){` |
|     36 | 2054 | `			iType = ph7_value_to_int64(pVal);` |
|     17 | 2055 | `		}` |
|     44 | 2056 | `		pVal = ph7_array_fetch(pOpts,"curve_name",-1);` |
|     44 | 2057 | `		if( pVal ){` |
|     12 | 2058 | `			zCurve = ph7_value_to_string(pVal,&nCurve);` |
|      5 | 2059 | `		}` |
|     21 | 2060 | `	}` |
|     44 | 2061 | `	pKey = PH7_SslGenerateKey(pCtx,iBits,iType,zCurve,nCurve);` |
|     44 | 2062 | `	if( pKey == 0 ){` |
|     11 | 2063 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|     11 | 2064 | `		ph7_result_bool(pCtx,0);` |
|     11 | 2065 | `		return PH7_OK;` |
|      - | 2066 | `	}` |
|     34 | 2067 | `	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_KEY,(void *)pKey);` |
|     26 | 2068 | `}` |
|      - | 2069 | `/* ---- the two import doors ---- */` |
|     64 | 2070 | `static int SslGetKeyDoor(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPublic)` |
|      2 | 2071 | `{` |
|     66 | 2072 | `	const char *zPass = 0;` |
|     66 | 2073 | `	int nPass = 0,bOwn = 0;` |
|      - | 2074 | `	EVP_PKEY *pKey;` |
|     66 | 2075 | `	if( nArg < 1 ){` |
|    ! 0 | 2076 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2077 | `		return PH7_OK;` |
|      - | 2078 | `	}` |
|     66 | 2079 | `	if( !bPublic && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      5 | 2080 | `		zPass = ph7_value_to_string(apArg[1],&nPass);` |
|      2 | 2081 | `	}` |
|     45 | 2082 | `	pKey = bPublic ? PH7_SslKeyOfValue(pCtx,apArg[0],1,&bOwn)` |
|     53 | 2083 | `	               : SslPrivateKeyWithPass(pCtx,apArg[0],zPass,nPass,&bOwn);` |
|     66 | 2084 | `	if( bOwn < 0 ){` |
|     13 | 2085 | `		ph7_result_bool(pCtx,0);` |
|     13 | 2086 | `		return PH7_SslArrayShapeError(pCtx);` |
|      - | 2087 | `	}` |
|     54 | 2088 | `	if( pKey == 0 ){` |
|     24 | 2089 | `		ph7_result_bool(pCtx,0);` |
|     24 | 2090 | `		return PH7_OK;` |
|      - | 2091 | `	}` |
|     32 | 2092 | `	if( !bOwn ){` |
|      - | 2093 | `		/* Already an OpenSSLAsymmetricKey: php hands back the SAME key rather` |
|      - | 2094 | `		 * than a copy, so the object identity survives the round trip. */` |
|      3 | 2095 | `		ph7_result_value(pCtx,apArg[0]);` |
|      3 | 2096 | `		return PH7_OK;` |
|      - | 2097 | `	}` |
|     30 | 2098 | `	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_KEY,(void *)pKey);` |
|     34 | 2099 | `}` |
|     42 | 2100 | `static int vm_builtin_openssl_pkey_get_private(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2101 | `{` |
|     44 | 2102 | `	return SslGetKeyDoor(pCtx,nArg,apArg,0);` |
|      2 | 2103 | `}` |
|     22 | 2104 | `static int vm_builtin_openssl_pkey_get_public(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2105 | `{` |
|     23 | 2106 | `	return SslGetKeyDoor(pCtx,nArg,apArg,1);` |
|      1 | 2107 | `}` |
|      - | 2108 | `/*` |
|      - | 2109 | ` * openssl_pkey_free(), openssl_free_key() and openssl_x509_free() are NOT` |
|      - | 2110 | ` * here, and their absence is deliberate. php 8 made all three no-ops (a key` |
|      - | 2111 | ` * and a certificate are objects now, freed with their last reference) and` |
|      - | 2112 | `` * marks each `deprecated since 8.0`; the scope policy's rule is that what php merely`` |
|      - | 2113 | ` * deprecates this engine REMOVES, so a program that calls one fails loudly` |
|      - | 2114 | ` * instead of being quietly told it is doing nothing. The three names keep` |
|      - | 2115 | ` * their rows in the extension partition -- every reader filters against the` |
|      - | 2116 | ` * live VM, so they simply do not appear -- and the one call site any of the` |
|      - | 2117 | ` * four target projects reaches is recorded with the gate's baselines.` |
|      - | 2118 | ` */` |
|      - | 2119 | `/* ---- openssl_pkey_get_details() ---- */` |
|    132 | 2120 | `static void SslDetailBn(ph7_context *pCtx,EVP_PKEY *pKey,ph7_value *pArray,ph7_value *pVal,` |
|      - | 2121 | `	const char *zParam,const char *zName)` |
|      2 | 2122 | `{` |
|    134 | 2123 | `	BIGNUM *pBn = 0;` |
|      - | 2124 | `	unsigned char *zBuf;` |
|      - | 2125 | `	int nLen;` |
|    134 | 2126 | `	if( EVP_PKEY_get_bn_param(pKey,zParam,&pBn) != 1 \|\| pBn == 0 ){` |
|     13 | 2127 | `		return;` |
|      - | 2128 | `	}` |
|    122 | 2129 | `	nLen = BN_num_bytes(pBn);` |
|    122 | 2130 | `	zBuf = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nLen > 0 ? nLen : 1));` |
|    122 | 2131 | `	if( zBuf ){` |
|    122 | 2132 | `		BN_bn2bin(pBn,zBuf);` |
|    122 | 2133 | `		ph7_value_string(pVal,(const char *)zBuf,nLen);` |
|    122 | 2134 | `		ph7_array_add_strkey_elem(pArray,zName,pVal);` |
|    122 | 2135 | `		ph7_value_reset_string_cursor(pVal);` |
|    122 | 2136 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zBuf);` |
|     60 | 2137 | `	}` |
|    122 | 2138 | `	BN_free(pBn);` |
|     68 | 2139 | `}` |
|     32 | 2140 | `static int vm_builtin_openssl_pkey_get_details(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2141 | `{` |
|      - | 2142 | `	EVP_PKEY *pKey;` |
|      - | 2143 | `	ph7_value *pArray,*pVal,*pSub;` |
|      - | 2144 | `	BIO *pBio;` |
|      - | 2145 | `	int iType;` |
|     34 | 2146 | `	if( nArg < 1 ){` |
|    ! 0 | 2147 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2148 | `		return PH7_OK;` |
|      - | 2149 | `	}` |
|     34 | 2150 | `	pKey = (EVP_PKEY *)PH7_SslHandleOf(apArg[0],PHL_SSL_KIND_KEY);` |
|     34 | 2151 | `	if( pKey == 0 ){` |
|    ! 0 | 2152 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2153 | `		return PH7_OK;` |
|      - | 2154 | `	}` |
|     34 | 2155 | `	pArray = ph7_context_new_array(pCtx);` |
|     34 | 2156 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     34 | 2157 | `	pSub = ph7_context_new_array(pCtx);` |
|     34 | 2158 | `	if( pArray == 0 \|\| pVal == 0 \|\| pSub == 0 ){` |
|    ! 0 | 2159 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2160 | `		return PH7_OK;` |
|      - | 2161 | `	}` |
|     34 | 2162 | `	ph7_value_int(pVal,EVP_PKEY_get_bits(pKey));` |
|     34 | 2163 | `	ph7_array_add_strkey_elem(pArray,"bits",pVal);` |
|      - | 2164 | ``	/* php's `key` is the PUBLIC half in PEM, whatever kind of key this is. */`` |
|     34 | 2165 | `	pBio = BIO_new(BIO_s_mem());` |
|     34 | 2166 | `	if( pBio ){` |
|     34 | 2167 | `		char *zMem = 0;` |
|      - | 2168 | `		long nMem;` |
|     34 | 2169 | `		if( PEM_write_bio_PUBKEY(pBio,pKey) == 1 ){` |
|     34 | 2170 | `			nMem = BIO_get_mem_data(pBio,&zMem);` |
|     34 | 2171 | `			ph7_value_string_format(pVal,"%.*s",(int)nMem,zMem);` |
|     34 | 2172 | `			ph7_array_add_strkey_elem(pArray,"key",pVal);` |
|     34 | 2173 | `			ph7_value_reset_string_cursor(pVal);` |
|     16 | 2174 | `		}` |
|     34 | 2175 | `		BIO_free(pBio);` |
|     16 | 2176 | `	}` |
|     34 | 2177 | `	iType = SslKeyTypeOf(pKey);` |
|     34 | 2178 | `	switch( iType ){` |
|      6 | 2179 | `		case 0:` |
|     14 | 2180 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_N,"n");` |
|     14 | 2181 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_E,"e");` |
|     14 | 2182 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_D,"d");` |
|     14 | 2183 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_FACTOR1,"p");` |
|     14 | 2184 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_FACTOR2,"q");` |
|     14 | 2185 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_EXPONENT1,"dmp1");` |
|     14 | 2186 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_EXPONENT2,"dmq1");` |
|     14 | 2187 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_COEFFICIENT1,"iqmp");` |
|     14 | 2188 | `			ph7_array_add_strkey_elem(pArray,"rsa",pSub);` |
|     14 | 2189 | `			break;` |
|    ! 0 | 2190 | `		case 1:` |
|    ! 0 | 2191 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_FFC_P,"p");` |
|    ! 0 | 2192 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_FFC_Q,"q");` |
|    ! 0 | 2193 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_FFC_G,"g");` |
|    ! 0 | 2194 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_PRIV_KEY,"priv_key");` |
|    ! 0 | 2195 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_PUB_KEY,"pub_key");` |
|    ! 0 | 2196 | `			ph7_array_add_strkey_elem(pArray,"dsa",pSub);` |
|    ! 0 | 2197 | `			break;` |
|    ! 0 | 2198 | `		case 2:` |
|    ! 0 | 2199 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_FFC_P,"p");` |
|    ! 0 | 2200 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_FFC_G,"g");` |
|    ! 0 | 2201 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_PRIV_KEY,"priv_key");` |
|    ! 0 | 2202 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_PUB_KEY,"pub_key");` |
|    ! 0 | 2203 | `			ph7_array_add_strkey_elem(pArray,"dh",pSub);` |
|    ! 0 | 2204 | `			break;` |
|      4 | 2205 | `		case 4: case 5: case 6: case 7: {` |
|      - | 2206 | `			/* The Edwards and Montgomery keys have no numbers to report: php` |
|      - | 2207 | `			 * gives them the two RAW octet strings, under a group named after` |
|      - | 2208 | `			 * the curve itself. */` |
|      - | 2209 | `			static const char * const azRaw[] = { "x25519","ed25519","x448","ed448" };` |
|      - | 2210 | `			unsigned char aBuf[128];` |
|      9 | 2211 | `			size_t nRaw = 0;` |
|     12 | 2212 | `			if( EVP_PKEY_get_octet_string_param(pKey,OSSL_PKEY_PARAM_PRIV_KEY,` |
|      9 | 2213 | `					aBuf,sizeof(aBuf),&nRaw) == 1 ){` |
|      9 | 2214 | `				ph7_value_string(pVal,(const char *)aBuf,(int)nRaw);` |
|      9 | 2215 | `				ph7_array_add_strkey_elem(pSub,"priv_key",pVal);` |
|      9 | 2216 | `				ph7_value_reset_string_cursor(pVal);` |
|      4 | 2217 | `			}` |
|      9 | 2218 | `			nRaw = 0;` |
|     12 | 2219 | `			if( EVP_PKEY_get_octet_string_param(pKey,OSSL_PKEY_PARAM_PUB_KEY,` |
|      9 | 2220 | `					aBuf,sizeof(aBuf),&nRaw) == 1 ){` |
|      9 | 2221 | `				ph7_value_string(pVal,(const char *)aBuf,(int)nRaw);` |
|      9 | 2222 | `				ph7_array_add_strkey_elem(pSub,"pub_key",pVal);` |
|      9 | 2223 | `				ph7_value_reset_string_cursor(pVal);` |
|      4 | 2224 | `			}` |
|      9 | 2225 | `			ph7_array_add_strkey_elem(pArray,azRaw[iType - 4],pSub);` |
|      9 | 2226 | `			break;` |
|      - | 2227 | `		}` |
|      6 | 2228 | `		case 3: {` |
|      - | 2229 | `			char zGroup[128];` |
|     14 | 2230 | `			size_t nGroup = 0;` |
|     18 | 2231 | `			if( EVP_PKEY_get_utf8_string_param(pKey,OSSL_PKEY_PARAM_GROUP_NAME,` |
|     14 | 2232 | `					zGroup,sizeof(zGroup),&nGroup) == 1 ){` |
|     14 | 2233 | `				int nid = OBJ_sn2nid(zGroup);` |
|     14 | 2234 | `				ph7_value_string(pVal,zGroup,(int)nGroup);` |
|     14 | 2235 | `				ph7_array_add_strkey_elem(pSub,"curve_name",pVal);` |
|     14 | 2236 | `				ph7_value_reset_string_cursor(pVal);` |
|     14 | 2237 | `				if( nid != NID_undef ){` |
|      - | 2238 | `					char zOid[128];` |
|     14 | 2239 | `					ASN1_OBJECT *pObj = OBJ_nid2obj(nid);` |
|     14 | 2240 | `					int nOid = pObj ? OBJ_obj2txt(zOid,(int)sizeof(zOid),pObj,1) : 0;` |
|     14 | 2241 | `					if( nOid > 0 ){` |
|     14 | 2242 | `						ph7_value_string(pVal,zOid,nOid);` |
|     14 | 2243 | `						ph7_array_add_strkey_elem(pSub,"curve_oid",pVal);` |
|     14 | 2244 | `						ph7_value_reset_string_cursor(pVal);` |
|      6 | 2245 | `					}` |
|      6 | 2246 | `				}` |
|      6 | 2247 | `			}` |
|     14 | 2248 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_EC_PUB_X,"x");` |
|     14 | 2249 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_EC_PUB_Y,"y");` |
|     14 | 2250 | `			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_PRIV_KEY,"d");` |
|     14 | 2251 | `			ph7_array_add_strkey_elem(pArray,"ec",pSub);` |
|     12 | 2252 | `			break;` |
|      - | 2253 | `		}` |
|    ! 0 | 2254 | `		default:` |
|    ! 0 | 2255 | `			break;` |
|      - | 2256 | `	}` |
|     34 | 2257 | `	ph7_value_int(pVal,iType);` |
|     34 | 2258 | `	ph7_array_add_strkey_elem(pArray,"type",pVal);` |
|     34 | 2259 | `	ph7_result_value(pCtx,pArray);` |
|     34 | 2260 | `	return PH7_OK;` |
|     18 | 2261 | `}` |
|      - | 2262 | `/* ---- openssl_pkey_export() and its file twin ---- */` |
|     20 | 2263 | `static int SslPkeyExport(ph7_context *pCtx,int nArg,ph7_value **apArg,int bToFile)` |
|      2 | 2264 | `{` |
|      - | 2265 | `	EVP_PKEY *pKey;` |
|      - | 2266 | `	BIO *pBio;` |
|     22 | 2267 | `	const EVP_CIPHER *pCipher = 0;` |
|     22 | 2268 | `	const char *zPass = 0,*zOut = 0;` |
|     22 | 2269 | `	int nPass = 0,bOwn = 0,rc = 0,iPassArg = bToFile ? 2 : 2;` |
|      - | 2270 | `	long nOut;` |
|     22 | 2271 | `	char *zMem = 0;` |
|     22 | 2272 | `	if( nArg < 2 ){` |
|    ! 0 | 2273 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2274 | `		return PH7_OK;` |
|      - | 2275 | `	}` |
|     22 | 2276 | `	pKey = PH7_SslKeyOfValue(pCtx,apArg[0],0,&bOwn);` |
|     22 | 2277 | `	if( bOwn < 0 ){` |
|    ! 0 | 2278 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2279 | `		return PH7_SslArrayShapeError(pCtx);` |
|      - | 2280 | `	}` |
|     22 | 2281 | `	if( pKey == 0 ){` |
|      3 | 2282 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Cannot get key from parameter 1");` |
|      3 | 2283 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2284 | `		return PH7_OK;` |
|      - | 2285 | `	}` |
|     20 | 2286 | `	if( nArg > iPassArg && !ph7_value_is_null(apArg[iPassArg]) ){` |
|      6 | 2287 | `		zPass = ph7_value_to_string(apArg[iPassArg],&nPass);` |
|      2 | 2288 | `	}` |
|     20 | 2289 | `	if( zPass && nPass > 0 ){` |
|      6 | 2290 | `		pCipher = EVP_aes_256_cbc();` |
|      2 | 2291 | `	}` |
|     20 | 2292 | `	pBio = BIO_new(BIO_s_mem());` |
|     20 | 2293 | `	if( pBio == 0 ){` |
|    ! 0 | 2294 | `		SslReleaseKey(pKey,bOwn);` |
|    ! 0 | 2295 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2296 | `		return PH7_OK;` |
|      - | 2297 | `	}` |
|     20 | 2298 | `	if( PEM_write_bio_PKCS8PrivateKey(pBio,pKey,pCipher,(char *)zPass,nPass,0,0) != 1 ){` |
|    ! 0 | 2299 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 2300 | `		rc = -1;` |
|    ! 0 | 2301 | `	}` |
|     20 | 2302 | `	if( rc == 0 ){` |
|     20 | 2303 | `		nOut = BIO_get_mem_data(pBio,&zMem);` |
|     20 | 2304 | `		zOut = zMem;` |
|     20 | 2305 | `		if( bToFile ){` |
|      3 | 2306 | `			int nPath = 0;` |
|      3 | 2307 | `			const char *zPath = ph7_value_to_string(apArg[1],&nPath);` |
|      3 | 2308 | `			rc = PH7_SslWriteFileArg(pCtx,zPath,nPath,zOut,(sxu32)nOut);` |
|      2 | 2309 | `		}else{` |
|     18 | 2310 | `			ph7_value *pRes = ph7_context_new_scalar(pCtx);` |
|     18 | 2311 | `			if( pRes ){` |
|     18 | 2312 | `				ph7_value_string(pRes,zOut,(int)nOut);` |
|     18 | 2313 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);` |
|     18 | 2314 | `				ph7_context_release_value(pCtx,pRes);` |
|      8 | 2315 | `			}` |
|      - | 2316 | `		}` |
|      9 | 2317 | `	}` |
|     20 | 2318 | `	BIO_free(pBio);` |
|     20 | 2319 | `	SslReleaseKey(pKey,bOwn);` |
|     20 | 2320 | `	ph7_result_bool(pCtx,rc == 0);` |
|     20 | 2321 | `	return PH7_OK;` |
|     12 | 2322 | `}` |
|     18 | 2323 | `static int vm_builtin_openssl_pkey_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2324 | `{` |
|     20 | 2325 | `	return SslPkeyExport(pCtx,nArg,apArg,0);` |
|      2 | 2326 | `}` |
|      2 | 2327 | `static int vm_builtin_openssl_pkey_export_to_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2328 | `{` |
|      3 | 2329 | `	return SslPkeyExport(pCtx,nArg,apArg,1);` |
|      1 | 2330 | `}` |
|      - | 2331 |  |
|      - | 2332 | `/* ------------------------------------------------------------------------` |
|      - | 2333 | ` * Signatures and asymmetric encryption` |
|      - | 2334 | ` * ------------------------------------------------------------------------ */` |
|      - | 2335 | `/*` |
|      - | 2336 | `` * openssl_sign() / openssl_verify(). php's `$algorithm` is the string\|int the`` |
|      - | 2337 | `` * digest helper reads, and `$padding` is an RSA padding mode -- 0 meaning`` |
|      - | 2338 | ` * "whatever the key's default is", which for RSA is PKCS#1 v1.5.` |
|      - | 2339 | ` */` |
|      - | 2340 | `/*` |
|      - | 2341 | ` * openssl_sign() and openssl_verify() SCREEN the padding where the four raw` |
|      - | 2342 | ` * RSA doors do not: only 0 (the key's own default), PKCS#1 v1.5 and PSS are` |
|      - | 2343 | ` * accepted, and every other number -- OAEP included, which the raw doors take` |
|      - | 2344 | `` * happily -- is `Unknown padding type`. The two report that refusal`` |
|      - | 2345 | ` * differently: sign answers false, verify answers -1.` |
|      - | 2346 | ` *` |
|      - | 2347 | ` * The salt length is deliberately NOT set. php leaves it at OpenSSL's own` |
|      - | 2348 | ` * default (the maximum the key allows), and pinning it to the digest's length` |
|      - | 2349 | ` * makes SHA-512 PSS impossible on a 1024-bit key -- 64 + 64 + 2 does not fit` |
|      - | 2350 | ` * in 128 bytes -- where php signs it without complaint.` |
|      - | 2351 | ` */` |
|     72 | 2352 | `static int SslPaddingIsSignable(int iPadding)` |
|      2 | 2353 | `{` |
|     74 | 2354 | `	return iPadding == 0 \|\| iPadding == RSA_PKCS1_PADDING \|\| iPadding == RSA_PKCS1_PSS_PADDING;` |
|      2 | 2355 | `}` |
|     52 | 2356 | `static int SslApplyRsaPadding(EVP_PKEY_CTX *pKCtx,int iPadding)` |
|      2 | 2357 | `{` |
|     54 | 2358 | `	if( iPadding == 0 ){` |
|     42 | 2359 | `		return 1;` |
|      - | 2360 | `	}` |
|     13 | 2361 | `	return EVP_PKEY_CTX_set_rsa_padding(pKCtx,iPadding) > 0;` |
|     28 | 2362 | `}` |
|     34 | 2363 | `static int vm_builtin_openssl_sign(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2364 | `{` |
|      - | 2365 | `	EVP_PKEY *pKey;` |
|     36 | 2366 | `	EVP_MD_CTX *pMdCtx = 0;` |
|     36 | 2367 | `	EVP_PKEY_CTX *pKCtx = 0;` |
|      - | 2368 | `	const EVP_MD *pMd;` |
|     36 | 2369 | `	EVP_MD *pFetched = 0;` |
|      - | 2370 | `	const char *zData;` |
|     36 | 2371 | `	unsigned char *zSig = 0;` |
|     36 | 2372 | `	size_t nSig = 0;` |
|     36 | 2373 | `	int nData = 0,bOwn = 0,iPadding = 0,rc = -1;` |
|     36 | 2374 | `	if( nArg < 3 ){` |
|    ! 0 | 2375 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2376 | `		return PH7_OK;` |
|      - | 2377 | `	}` |
|     36 | 2378 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|     36 | 2379 | `	if( zData == 0 ){ zData = ""; }` |
|     36 | 2380 | `	if( nArg > 3 ){` |
|     32 | 2381 | `		pMd = PH7_SslDigestOfValue(apArg[3],&pFetched);` |
|     17 | 2382 | `	}else{` |
|      5 | 2383 | `		pMd = EVP_sha1();` |
|      - | 2384 | `	}` |
|     36 | 2385 | `	if( pMd == 0 ){` |
|      3 | 2386 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");` |
|      3 | 2387 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2388 | `		return PH7_OK;` |
|      - | 2389 | `	}` |
|     34 | 2390 | `	if( nArg > 4 ){` |
|     15 | 2391 | `		iPadding = (int)ph7_value_to_int64(apArg[4]);` |
|      7 | 2392 | `	}` |
|     34 | 2393 | `	if( !SslPaddingIsSignable(iPadding) ){` |
|      7 | 2394 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|      7 | 2395 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown padding type");` |
|      7 | 2396 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2397 | `		return PH7_OK;` |
|      - | 2398 | `	}` |
|     28 | 2399 | `	pKey = PH7_SslKeyOfValue(pCtx,apArg[2],0,&bOwn);` |
|     28 | 2400 | `	if( bOwn < 0 ){` |
|    ! 0 | 2401 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|    ! 0 | 2402 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2403 | `		return PH7_SslArrayShapeError(pCtx);` |
|      - | 2404 | `	}` |
|     28 | 2405 | `	if( pKey == 0 ){` |
|      5 | 2406 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|      5 | 2407 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2408 | `			"Supplied key param cannot be coerced into a private key");` |
|      5 | 2409 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2410 | `		return PH7_OK;` |
|      - | 2411 | `	}` |
|     24 | 2412 | `	pMdCtx = EVP_MD_CTX_new();` |
|     22 | 2413 | `	if( pMdCtx && EVP_DigestSignInit(pMdCtx,&pKCtx,pMd,0,pKey) == 1` |
|     22 | 2414 | `	 && SslApplyRsaPadding(pKCtx,iPadding)` |
|     24 | 2415 | `	 && EVP_DigestSign(pMdCtx,0,&nSig,(const unsigned char *)zData,(size_t)nData) == 1 ){` |
|     24 | 2416 | `		zSig = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nSig);` |
|     24 | 2417 | `		if( zSig && EVP_DigestSign(pMdCtx,zSig,&nSig,(const unsigned char *)zData,(size_t)nData) == 1 ){` |
|     24 | 2418 | `			ph7_value *pRes = ph7_context_new_scalar(pCtx);` |
|     24 | 2419 | `			if( pRes ){` |
|     24 | 2420 | `				ph7_value_string(pRes,(const char *)zSig,(int)nSig);` |
|     24 | 2421 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);` |
|     24 | 2422 | `				ph7_context_release_value(pCtx,pRes);` |
|     11 | 2423 | `			}` |
|     24 | 2424 | `			rc = 0;` |
|     11 | 2425 | `		}` |
|     11 | 2426 | `	}` |
|     24 | 2427 | `	if( zSig ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zSig); }` |
|     24 | 2428 | `	if( pMdCtx ){ EVP_MD_CTX_free(pMdCtx); }` |
|     24 | 2429 | `	if( pFetched ){ EVP_MD_free(pFetched); }` |
|     24 | 2430 | `	SslReleaseKey(pKey,bOwn);` |
|     24 | 2431 | `	if( rc != 0 ){` |
|    ! 0 | 2432 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 2433 | `	}` |
|     24 | 2434 | `	ph7_result_bool(pCtx,rc == 0);` |
|     24 | 2435 | `	return PH7_OK;` |
|     19 | 2436 | `}` |
|     42 | 2437 | `static int vm_builtin_openssl_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2438 | `{` |
|      - | 2439 | `	EVP_PKEY *pKey;` |
|     44 | 2440 | `	EVP_MD_CTX *pMdCtx = 0;` |
|     44 | 2441 | `	EVP_PKEY_CTX *pKCtx = 0;` |
|      - | 2442 | `	const EVP_MD *pMd;` |
|     44 | 2443 | `	EVP_MD *pFetched = 0;` |
|      - | 2444 | `	const char *zData,*zSig;` |
|     44 | 2445 | `	int nData = 0,nSig = 0,bOwn = 0,iPadding = 0,iRc;` |
|     44 | 2446 | `	if( nArg < 3 ){` |
|    ! 0 | 2447 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2448 | `		return PH7_OK;` |
|      - | 2449 | `	}` |
|     44 | 2450 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|     44 | 2451 | `	zSig = ph7_value_to_string(apArg[1],&nSig);` |
|     44 | 2452 | `	if( zData == 0 ){ zData = ""; }` |
|     44 | 2453 | `	if( zSig == 0 ){ zSig = ""; }` |
|     44 | 2454 | `	if( nArg > 3 ){` |
|     40 | 2455 | `		pMd = PH7_SslDigestOfValue(apArg[3],&pFetched);` |
|     21 | 2456 | `	}else{` |
|      5 | 2457 | `		pMd = EVP_sha1();` |
|      - | 2458 | `	}` |
|     44 | 2459 | `	if( pMd == 0 ){` |
|      3 | 2460 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");` |
|      3 | 2461 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2462 | `		return PH7_OK;` |
|      - | 2463 | `	}` |
|     42 | 2464 | `	if( nArg > 4 ){` |
|     15 | 2465 | `		iPadding = (int)ph7_value_to_int64(apArg[4]);` |
|      7 | 2466 | `	}` |
|     42 | 2467 | `	if( !SslPaddingIsSignable(iPadding) ){` |
|      7 | 2468 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|      7 | 2469 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown padding type");` |
|      7 | 2470 | `		ph7_result_int(pCtx,-1);` |
|      7 | 2471 | `		return PH7_OK;` |
|      - | 2472 | `	}` |
|     36 | 2473 | `	pKey = PH7_SslKeyOfValue(pCtx,apArg[2],1,&bOwn);` |
|     36 | 2474 | `	if( bOwn < 0 ){` |
|    ! 0 | 2475 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|    ! 0 | 2476 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2477 | `		return PH7_SslArrayShapeError(pCtx);` |
|      - | 2478 | `	}` |
|     36 | 2479 | `	if( pKey == 0 ){` |
|      5 | 2480 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|      5 | 2481 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2482 | `			"Supplied key param cannot be coerced into a public key");` |
|      5 | 2483 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2484 | `		return PH7_OK;` |
|      - | 2485 | `	}` |
|     32 | 2486 | `	pMdCtx = EVP_MD_CTX_new();` |
|     32 | 2487 | `	iRc = -1;` |
|     30 | 2488 | `	if( pMdCtx && EVP_DigestVerifyInit(pMdCtx,&pKCtx,pMd,0,pKey) == 1` |
|     32 | 2489 | `	 && SslApplyRsaPadding(pKCtx,iPadding) ){` |
|     47 | 2490 | `		iRc = EVP_DigestVerify(pMdCtx,(const unsigned char *)zSig,(size_t)nSig,` |
|     15 | 2491 | `			(const unsigned char *)zData,(size_t)nData);` |
|     15 | 2492 | `	}` |
|     32 | 2493 | `	if( pMdCtx ){ EVP_MD_CTX_free(pMdCtx); }` |
|     32 | 2494 | `	if( pFetched ){ EVP_MD_free(pFetched); }` |
|     32 | 2495 | `	SslReleaseKey(pKey,bOwn);` |
|     32 | 2496 | `	if( iRc < 0 ){` |
|      - | 2497 | `		/* php's -1 is a FAILED verification rather than an error: the three` |
|      - | 2498 | ``		 * answers are 1, 0 and -1, and only a bad key or digest is `false`. */`` |
|    ! 0 | 2499 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 2500 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2501 | `		return PH7_OK;` |
|      - | 2502 | `	}` |
|     32 | 2503 | `	if( iRc != 1 ){` |
|     15 | 2504 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      7 | 2505 | `	}` |
|     32 | 2506 | `	ph7_result_int(pCtx,iRc == 1 ? 1 : 0);` |
|     32 | 2507 | `	return PH7_OK;` |
|     23 | 2508 | `}` |
|      - | 2509 | `/*` |
|      - | 2510 | `` * The four raw RSA doors. `$padding` defaults to PKCS#1 v1.5 in all four, and`` |
|      - | 2511 | ` * the two DECRYPT directions size their output from the key rather than from` |
|      - | 2512 | ` * the input -- a plaintext shorter than the modulus comes back at its own` |
|      - | 2513 | ` * length, which is what the padding removed.` |
|      - | 2514 | ` */` |
|     54 | 2515 | `static int SslRsaDoor(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPublicKey,int bEncrypt,` |
|      - | 2516 | `	const char *zBadKey)` |
|      1 | 2517 | `{` |
|      - | 2518 | `	EVP_PKEY *pKey;` |
|     55 | 2519 | `	EVP_PKEY_CTX *pKCtx = 0;` |
|      - | 2520 | `	const char *zIn;` |
|     55 | 2521 | `	unsigned char *zOut = 0;` |
|     55 | 2522 | `	size_t nOut = 0;` |
|     55 | 2523 | `	int nIn = 0,bOwn = 0,iPadding = RSA_PKCS1_PADDING,rc = -1;` |
|     55 | 2524 | `	if( nArg < 3 ){` |
|    ! 0 | 2525 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2526 | `		return PH7_OK;` |
|      - | 2527 | `	}` |
|     55 | 2528 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|     55 | 2529 | `	if( zIn == 0 ){ zIn = ""; }` |
|     55 | 2530 | `	if( nArg > 3 ){` |
|     49 | 2531 | `		iPadding = (int)ph7_value_to_int64(apArg[3]);` |
|     24 | 2532 | `	}` |
|     55 | 2533 | `	pKey = PH7_SslKeyOfValue(pCtx,apArg[2],bPublicKey,&bOwn);` |
|     55 | 2534 | `	if( bOwn < 0 ){` |
|    ! 0 | 2535 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2536 | `		return PH7_SslArrayShapeError(pCtx);` |
|      - | 2537 | `	}` |
|     55 | 2538 | `	if( pKey == 0 ){` |
|      7 | 2539 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zBadKey);` |
|      7 | 2540 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2541 | `		return PH7_OK;` |
|      - | 2542 | `	}` |
|     49 | 2543 | `	pKCtx = EVP_PKEY_CTX_new(pKey,0);` |
|     49 | 2544 | `	if( pKCtx ){` |
|      - | 2545 | `		int (*xInit)(EVP_PKEY_CTX *);` |
|      - | 2546 | `		int (*xRun)(EVP_PKEY_CTX *,unsigned char *,size_t *,const unsigned char *,size_t);` |
|     49 | 2547 | `		if( bEncrypt ){` |
|     37 | 2548 | `			xInit = bPublicKey ? EVP_PKEY_encrypt_init : EVP_PKEY_sign_init;` |
|     37 | 2549 | `			xRun  = bPublicKey ? EVP_PKEY_encrypt : 0;` |
|     19 | 2550 | `		}else{` |
|     13 | 2551 | `			xInit = bPublicKey ? EVP_PKEY_verify_recover_init : EVP_PKEY_decrypt_init;` |
|     13 | 2552 | `			xRun  = bPublicKey ? EVP_PKEY_verify_recover : EVP_PKEY_decrypt;` |
|      - | 2553 | `		}` |
|     49 | 2554 | `		if( bEncrypt && !bPublicKey ){` |
|      - | 2555 | `			/* A PRIVATE encrypt is a raw signature: OpenSSL spells it` |
|      - | 2556 | `			 * sign_init/sign with no digest, which is what php's` |
|      - | 2557 | `			 * openssl_private_encrypt() produces and what` |
|      - | 2558 | `			 * openssl_public_decrypt() recovers. */` |
|     19 | 2559 | `			xRun = EVP_PKEY_sign;` |
|      9 | 2560 | `		}` |
|     48 | 2561 | `		if( xInit(pKCtx) > 0` |
|     48 | 2562 | `		 && EVP_PKEY_CTX_set_rsa_padding(pKCtx,iPadding) > 0` |
|     46 | 2563 | `		 && xRun(pKCtx,0,&nOut,(const unsigned char *)zIn,(size_t)nIn) > 0 ){` |
|     43 | 2564 | `			zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     42 | 2565 | `				(sxu32)(nOut > 0 ? nOut : 1));` |
|     43 | 2566 | `			if( zOut && xRun(pKCtx,zOut,&nOut,(const unsigned char *)zIn,(size_t)nIn) > 0 ){` |
|     25 | 2567 | `				ph7_value *pRes = ph7_context_new_scalar(pCtx);` |
|     25 | 2568 | `				if( pRes ){` |
|     25 | 2569 | `					ph7_value_string(pRes,(const char *)zOut,(int)nOut);` |
|     25 | 2570 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);` |
|     25 | 2571 | `					ph7_context_release_value(pCtx,pRes);` |
|     12 | 2572 | `				}` |
|     25 | 2573 | `				rc = 0;` |
|     12 | 2574 | `			}` |
|     21 | 2575 | `		}` |
|     24 | 2576 | `	}` |
|     49 | 2577 | `	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }` |
|     49 | 2578 | `	if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }` |
|     49 | 2579 | `	SslReleaseKey(pKey,bOwn);` |
|     49 | 2580 | `	if( rc != 0 ){` |
|     25 | 2581 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|     12 | 2582 | `	}` |
|     49 | 2583 | `	ph7_result_bool(pCtx,rc == 0);` |
|     49 | 2584 | `	return PH7_OK;` |
|     28 | 2585 | `}` |
|     22 | 2586 | `static int vm_builtin_openssl_public_encrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2587 | `{` |
|     23 | 2588 | `	return SslRsaDoor(pCtx,nArg,apArg,1,1,"key parameter is not a valid public key");` |
|      1 | 2589 | `}` |
|     10 | 2590 | `static int vm_builtin_openssl_private_decrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2591 | `{` |
|     11 | 2592 | `	return SslRsaDoor(pCtx,nArg,apArg,0,0,"key parameter is not a valid private key");` |
|      1 | 2593 | `}` |
|     18 | 2594 | `static int vm_builtin_openssl_private_encrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2595 | `{` |
|     19 | 2596 | `	return SslRsaDoor(pCtx,nArg,apArg,0,1,"key parameter is not a valid private key");` |
|      1 | 2597 | `}` |
|      4 | 2598 | `static int vm_builtin_openssl_public_decrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2599 | `{` |
|      5 | 2600 | `	return SslRsaDoor(pCtx,nArg,apArg,1,0,"key parameter is not a valid public key");` |
|      1 | 2601 | `}` |
|      - | 2602 | `/*` |
|      - | 2603 | ` * openssl_seal() / openssl_open(): a symmetric key per message, encrypted once` |
|      - | 2604 | ` * per recipient. The sealed LENGTH is what seal() answers, not a boolean.` |
|      - | 2605 | ` */` |
|      8 | 2606 | `static int vm_builtin_openssl_seal(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2607 | `{` |
|      - | 2608 | `	const EVP_CIPHER *pCipher;` |
|      9 | 2609 | `	EVP_CIPHER *pFetched = 0;` |
|      9 | 2610 | `	EVP_CIPHER_CTX *pEvp = 0;` |
|      9 | 2611 | `	EVP_PKEY **apKey = 0;` |
|      9 | 2612 | `	unsigned char **apEk = 0;` |
|      9 | 2613 | `	int *aEkLen = 0,*aOwn = 0;` |
|      9 | 2614 | `	unsigned char *zIv = 0,*zOut = 0;` |
|      - | 2615 | `	const char *zData,*zMethod;` |
|      9 | 2616 | `	ph7_value *pKeys,*pVal = 0,*pEkArray = 0;` |
|      9 | 2617 | `	int nData = 0,nMethod = 0,nKey = 0,i,nOut = 0,nFinal = 0,nIv,rc = -1,bShape = 0;` |
|      9 | 2618 | `	if( nArg < 5 ){` |
|    ! 0 | 2619 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2620 | `		return PH7_OK;` |
|      - | 2621 | `	}` |
|      9 | 2622 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|      9 | 2623 | `	if( zData == 0 ){ zData = ""; }` |
|      9 | 2624 | `	pKeys = apArg[3];` |
|      9 | 2625 | `	if( !ph7_value_is_array(pKeys) \|\| (nKey = (int)ph7_array_count(pKeys)) < 1 ){` |
|      4 | 2626 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 2627 | `			"%s(): Argument #4 ($public_key) must not be empty",ph7_function_name(pCtx));` |
|      - | 2628 | `	}` |
|      7 | 2629 | `	zMethod = ph7_value_to_string(apArg[4],&nMethod);` |
|      7 | 2630 | `	pCipher = nMethod > 0 ? SslCipherByName(zMethod,&pFetched) : 0;` |
|      7 | 2631 | `	if( pCipher == 0 ){` |
|      3 | 2632 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown cipher algorithm");` |
|      3 | 2633 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2634 | `		return PH7_OK;` |
|      - | 2635 | `	}` |
|      - | 2636 | `	/* Zeroed AS THEY ARE TAKEN, not after the four succeed: the cleanup at` |
|      - | 2637 | ``	 * `done:` walks apKey[] and aOwn[], and a partial allocation failure would`` |
|      - | 2638 | `	 * otherwise send it through uninitialized pointers. */` |
|      5 | 2639 | `	apKey = (EVP_PKEY **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(sizeof(void *) * nKey));` |
|      5 | 2640 | `	if( apKey ){ SyZero(apKey,(sxu32)(sizeof(void *) * nKey)); }` |
|      5 | 2641 | `	apEk = (unsigned char **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(sizeof(void *) * nKey));` |
|      5 | 2642 | `	if( apEk ){ SyZero(apEk,(sxu32)(sizeof(void *) * nKey)); }` |
|      5 | 2643 | `	aEkLen = (int *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(sizeof(int) * nKey));` |
|      5 | 2644 | `	if( aEkLen ){ SyZero(aEkLen,(sxu32)(sizeof(int) * nKey)); }` |
|      5 | 2645 | `	aOwn = (int *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(sizeof(int) * nKey));` |
|      5 | 2646 | `	if( aOwn ){ SyZero(aOwn,(sxu32)(sizeof(int) * nKey)); }` |
|      5 | 2647 | `	if( apKey == 0 \|\| apEk == 0 \|\| aEkLen == 0 \|\| aOwn == 0 ){` |
|    ! 0 | 2648 | `		goto done;` |
|      - | 2649 | `	}` |
|      9 | 2650 | `	for( i = 0 ; i < nKey ; ++i ){` |
|      7 | 2651 | `		ph7_value *pEntry = SslArrayNth(pKeys,i);` |
|      7 | 2652 | `		apKey[i] = pEntry ? PH7_SslKeyOfValue(pCtx,pEntry,1,&aOwn[i]) : 0;` |
|      7 | 2653 | `		if( aOwn[i] < 0 ){` |
|    ! 0 | 2654 | `			bShape = 1;` |
|    ! 0 | 2655 | `			goto done;` |
|      - | 2656 | `		}` |
|      7 | 2657 | `		if( apKey[i] == 0 ){` |
|      4 | 2658 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 2659 | `				"Not a public key (%dth member of pubkeys)",i + 1);` |
|      3 | 2660 | `			goto done;` |
|      - | 2661 | `		}` |
|      7 | 2662 | `		apEk[i] = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|      4 | 2663 | `			(sxu32)EVP_PKEY_get_size(apKey[i]));` |
|      5 | 2664 | `		if( apEk[i] == 0 ){` |
|    ! 0 | 2665 | `			goto done;` |
|      - | 2666 | `		}` |
|      3 | 2667 | `	}` |
|      3 | 2668 | `	nIv = EVP_CIPHER_get_iv_length(pCipher);` |
|      3 | 2669 | `	if( nIv > 0 ){` |
|      3 | 2670 | `		zIv = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nIv);` |
|      3 | 2671 | `		if( zIv == 0 ){` |
|    ! 0 | 2672 | `			goto done;` |
|      - | 2673 | `		}` |
|      1 | 2674 | `	}` |
|      3 | 2675 | `	pEvp = EVP_CIPHER_CTX_new();` |
|      4 | 2676 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|      2 | 2677 | `		(sxu32)(nData + EVP_CIPHER_get_block_size(pCipher) + 1));` |
|      3 | 2678 | `	if( pEvp == 0 \|\| zOut == 0 ){` |
|    ! 0 | 2679 | `		goto done;` |
|      - | 2680 | `	}` |
|      2 | 2681 | `	if( EVP_SealInit(pEvp,pCipher,apEk,aEkLen,zIv,apKey,nKey) <= 0` |
|      2 | 2682 | `	 \|\| EVP_SealUpdate(pEvp,zOut,&nOut,(const unsigned char *)zData,nData) != 1` |
|      3 | 2683 | `	 \|\| EVP_SealFinal(pEvp,zOut + nOut,&nFinal) != 1 ){` |
|    ! 0 | 2684 | `		goto done;` |
|      - | 2685 | `	}` |
|      3 | 2686 | `	nOut += nFinal;` |
|      3 | 2687 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      3 | 2688 | `	pEkArray = ph7_context_new_array(pCtx);` |
|      3 | 2689 | `	if( pVal == 0 \|\| pEkArray == 0 ){` |
|    ! 0 | 2690 | `		goto done;` |
|      - | 2691 | `	}` |
|      3 | 2692 | `	ph7_value_string(pVal,(const char *)zOut,nOut);` |
|      3 | 2693 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pVal);` |
|      3 | 2694 | `	ph7_value_reset_string_cursor(pVal);` |
|      7 | 2695 | `	for( i = 0 ; i < nKey ; ++i ){` |
|      5 | 2696 | `		ph7_value_string(pVal,(const char *)apEk[i],aEkLen[i]);` |
|      5 | 2697 | `		ph7_array_add_elem(pEkArray,0,pVal);` |
|      5 | 2698 | `		ph7_value_reset_string_cursor(pVal);` |
|      3 | 2699 | `	}` |
|      3 | 2700 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pEkArray);` |
|      3 | 2701 | `	if( nArg > 5 ){` |
|      3 | 2702 | `		ph7_value_string(pVal,(const char *)(zIv ? zIv : (unsigned char *)""),nIv > 0 ? nIv : 0);` |
|      3 | 2703 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[5],pVal);` |
|      3 | 2704 | `		ph7_value_reset_string_cursor(pVal);` |
|      1 | 2705 | `	}` |
|      3 | 2706 | `	rc = 0;` |
|      2 | 2707 | `done:` |
|      5 | 2708 | `	if( apKey ){` |
|     11 | 2709 | `		for( i = 0 ; i < nKey ; ++i ){` |
|      7 | 2710 | `			SslReleaseKey(apKey[i],aOwn ? aOwn[i] : 0);` |
|      7 | 2711 | `			if( apEk && apEk[i] ){ SyMemBackendFree(&pCtx->pVm->sAllocator,apEk[i]); }` |
|      4 | 2712 | `		}` |
|      5 | 2713 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,apKey);` |
|      2 | 2714 | `	}` |
|      5 | 2715 | `	if( apEk ){ SyMemBackendFree(&pCtx->pVm->sAllocator,apEk); }` |
|      5 | 2716 | `	if( aEkLen ){ SyMemBackendFree(&pCtx->pVm->sAllocator,aEkLen); }` |
|      5 | 2717 | `	if( aOwn ){ SyMemBackendFree(&pCtx->pVm->sAllocator,aOwn); }` |
|      5 | 2718 | `	if( zIv ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zIv); }` |
|      5 | 2719 | `	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }` |
|      5 | 2720 | `	if( pEvp ){ EVP_CIPHER_CTX_free(pEvp); }` |
|      5 | 2721 | `	if( pFetched ){ EVP_CIPHER_free(pFetched); }` |
|      5 | 2722 | `	if( bShape ){` |
|    ! 0 | 2723 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2724 | `		return PH7_SslArrayShapeError(pCtx);` |
|      - | 2725 | `	}` |
|      5 | 2726 | `	if( rc != 0 ){` |
|      3 | 2727 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      3 | 2728 | `		ph7_result_bool(pCtx,0);` |
|      2 | 2729 | `	}else{` |
|      3 | 2730 | `		ph7_result_int(pCtx,nOut);` |
|      - | 2731 | `	}` |
|      5 | 2732 | `	return PH7_OK;` |
|      5 | 2733 | `}` |
|      6 | 2734 | `static int vm_builtin_openssl_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2735 | `{` |
|      - | 2736 | `	const EVP_CIPHER *pCipher;` |
|      7 | 2737 | `	EVP_CIPHER *pFetched = 0;` |
|      7 | 2738 | `	EVP_CIPHER_CTX *pEvp = 0;` |
|      - | 2739 | `	EVP_PKEY *pKey;` |
|      7 | 2740 | `	const char *zData,*zEk,*zMethod,*zIv = 0;` |
|      7 | 2741 | `	unsigned char *zOut = 0;` |
|      7 | 2742 | `	int nData = 0,nEk = 0,nMethod = 0,nIv = 0,bOwn = 0,nOut = 0,nFinal = 0,rc = -1;` |
|      7 | 2743 | `	if( nArg < 5 ){` |
|    ! 0 | 2744 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2745 | `		return PH7_OK;` |
|      - | 2746 | `	}` |
|      7 | 2747 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|      7 | 2748 | `	zEk = ph7_value_to_string(apArg[2],&nEk);` |
|      7 | 2749 | `	zMethod = ph7_value_to_string(apArg[4],&nMethod);` |
|      7 | 2750 | `	if( zData == 0 ){ zData = ""; }` |
|      7 | 2751 | `	if( zEk == 0 ){ zEk = ""; }` |
|      7 | 2752 | `	if( nArg > 5 && !ph7_value_is_null(apArg[5]) ){` |
|      7 | 2753 | `		zIv = ph7_value_to_string(apArg[5],&nIv);` |
|      3 | 2754 | `	}` |
|      7 | 2755 | `	pCipher = nMethod > 0 ? SslCipherByName(zMethod,&pFetched) : 0;` |
|      7 | 2756 | `	if( pCipher == 0 ){` |
|    ! 0 | 2757 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown cipher algorithm");` |
|    ! 0 | 2758 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2759 | `		return PH7_OK;` |
|      - | 2760 | `	}` |
|      7 | 2761 | `	pKey = PH7_SslKeyOfValue(pCtx,apArg[3],0,&bOwn);` |
|      7 | 2762 | `	if( bOwn < 0 ){` |
|    ! 0 | 2763 | `		if( pFetched ){ EVP_CIPHER_free(pFetched); }` |
|    ! 0 | 2764 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2765 | `		return PH7_SslArrayShapeError(pCtx);` |
|      - | 2766 | `	}` |
|      7 | 2767 | `	if( pKey == 0 ){` |
|      3 | 2768 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2769 | `			"Unable to coerce parameter 4 into a private key");` |
|      3 | 2770 | `		if( pFetched ){ EVP_CIPHER_free(pFetched); }` |
|      3 | 2771 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2772 | `		return PH7_OK;` |
|      - | 2773 | `	}` |
|      5 | 2774 | `	pEvp = EVP_CIPHER_CTX_new();` |
|      7 | 2775 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|      4 | 2776 | `		(sxu32)(nData + EVP_CIPHER_get_block_size(pCipher) + 1));` |
|      5 | 2777 | `	if( pEvp == 0 \|\| zOut == 0 ){` |
|    ! 0 | 2778 | `		goto done;` |
|      - | 2779 | `	}` |
|      6 | 2780 | `	if( EVP_OpenInit(pEvp,pCipher,(const unsigned char *)zEk,nEk,` |
|      4 | 2781 | `			(const unsigned char *)zIv,pKey) <= 0` |
|      4 | 2782 | `	 \|\| EVP_OpenUpdate(pEvp,zOut,&nOut,(const unsigned char *)zData,nData) != 1` |
|      5 | 2783 | `	 \|\| EVP_OpenFinal(pEvp,zOut + nOut,&nFinal) != 1 ){` |
|    ! 0 | 2784 | `		goto done;` |
|      - | 2785 | `	}` |
|      5 | 2786 | `	nOut += nFinal;` |
|      - | 2787 | `	{` |
|      5 | 2788 | `		ph7_value *pRes = ph7_context_new_scalar(pCtx);` |
|      5 | 2789 | `		if( pRes ){` |
|      5 | 2790 | `			ph7_value_string(pRes,(const char *)zOut,nOut);` |
|      5 | 2791 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);` |
|      5 | 2792 | `			ph7_context_release_value(pCtx,pRes);` |
|      2 | 2793 | `		}` |
|      - | 2794 | `	}` |
|      5 | 2795 | `	rc = 0;` |
|      2 | 2796 | `done:` |
|      5 | 2797 | `	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }` |
|      5 | 2798 | `	if( pEvp ){ EVP_CIPHER_CTX_free(pEvp); }` |
|      5 | 2799 | `	if( pFetched ){ EVP_CIPHER_free(pFetched); }` |
|      5 | 2800 | `	SslReleaseKey(pKey,bOwn);` |
|      5 | 2801 | `	if( rc != 0 ){` |
|    ! 0 | 2802 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 2803 | `	}` |
|      5 | 2804 | `	ph7_result_bool(pCtx,rc == 0);` |
|      5 | 2805 | `	return PH7_OK;` |
|      4 | 2806 | `}` |
|      - | 2807 | `/*` |
|      - | 2808 | ` * openssl_pkey_derive(): one side's public key and the other's private one.` |
|      - | 2809 | ` *` |
|      - | 2810 | ` * php's THIRD parameter is not here. It deprecates the parameter itself rather` |
|      - | 2811 | ` * than any particular value -- an explicit 0 raises "the $key_length parameter` |
|      - | 2812 | ` * is deprecated as it is either ignored or truncates the key" just as a 16` |
|      - | 2813 | ` * does -- because the exchange produces what it produces and asking for fewer` |
|      - | 2814 | ` * bytes hands back a PREFIX that is not a shorter shared secret. The scope policy refuses` |
|      - | 2815 | ` * the spelling, so the signature declares two parameters and a third argument` |
|      - | 2816 | `` * is `expects exactly 2 arguments, 3 given`.`` |
|      - | 2817 | ` */` |
|     10 | 2818 | `static int vm_builtin_openssl_pkey_derive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2819 | `{` |
|      - | 2820 | `	EVP_PKEY *pPub,*pPriv;` |
|     12 | 2821 | `	EVP_PKEY_CTX *pKCtx = 0;` |
|     12 | 2822 | `	unsigned char *zOut = 0;` |
|     12 | 2823 | `	size_t nOut = 0;` |
|     12 | 2824 | `	int bOwnPub = 0,bOwnPriv = 0,rc = -1;` |
|     12 | 2825 | `	if( nArg < 2 ){` |
|    ! 0 | 2826 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2827 | `		return PH7_OK;` |
|      - | 2828 | `	}` |
|     12 | 2829 | `	pPub = PH7_SslKeyOfValue(pCtx,apArg[0],1,&bOwnPub);` |
|     12 | 2830 | `	pPriv = PH7_SslKeyOfValue(pCtx,apArg[1],0,&bOwnPriv);` |
|     12 | 2831 | `	if( bOwnPub < 0 \|\| bOwnPriv < 0 ){` |
|    ! 0 | 2832 | `		SslReleaseKey(pPub,bOwnPub);` |
|    ! 0 | 2833 | `		SslReleaseKey(pPriv,bOwnPriv);` |
|    ! 0 | 2834 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2835 | `		return PH7_SslArrayShapeError(pCtx);` |
|      - | 2836 | `	}` |
|     12 | 2837 | `	if( pPub == 0 \|\| pPriv == 0 ){` |
|      5 | 2838 | `		SslReleaseKey(pPub,bOwnPub);` |
|      5 | 2839 | `		SslReleaseKey(pPriv,bOwnPriv);` |
|      5 | 2840 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      5 | 2841 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2842 | `		return PH7_OK;` |
|      - | 2843 | `	}` |
|      8 | 2844 | `	pKCtx = EVP_PKEY_CTX_new(pPriv,0);` |
|      6 | 2845 | `	if( pKCtx && EVP_PKEY_derive_init(pKCtx) > 0` |
|      6 | 2846 | `	 && EVP_PKEY_derive_set_peer(pKCtx,pPub) > 0` |
|      8 | 2847 | `	 && EVP_PKEY_derive(pKCtx,0,&nOut) > 0 ){` |
|      8 | 2848 | `		zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nOut > 0 ? nOut : 1));` |
|      8 | 2849 | `		if( zOut && EVP_PKEY_derive(pKCtx,zOut,&nOut) > 0 ){` |
|      8 | 2850 | `			ph7_result_string(pCtx,(const char *)zOut,(int)nOut);` |
|      8 | 2851 | `			rc = 0;` |
|      3 | 2852 | `		}` |
|      3 | 2853 | `	}` |
|      8 | 2854 | `	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }` |
|      8 | 2855 | `	if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }` |
|      8 | 2856 | `	SslReleaseKey(pPub,bOwnPub);` |
|      8 | 2857 | `	SslReleaseKey(pPriv,bOwnPriv);` |
|      8 | 2858 | `	if( rc != 0 ){` |
|    ! 0 | 2859 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 2860 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2861 | `	}` |
|      8 | 2862 | `	return PH7_OK;` |
|      7 | 2863 | `}` |
|      - | 2864 | `/*` |
|      - | 2865 | ` * openssl_dh_compute_key(): the DH-only spelling of the same exchange, whose` |
|      - | 2866 | ` * public side is the RAW public value rather than an encoded key.` |
|      - | 2867 | ` */` |
|    ! 0 | 2868 | `static int vm_builtin_openssl_dh_compute_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2869 | `{` |
|    ! 0 | 2870 | `	EVP_PKEY *pPriv,*pPeer = 0;` |
|    ! 0 | 2871 | `	EVP_PKEY_CTX *pKCtx = 0;` |
|    ! 0 | 2872 | `	OSSL_PARAM_BLD *pBld = 0;` |
|    ! 0 | 2873 | `	OSSL_PARAM *pParams = 0;` |
|    ! 0 | 2874 | `	BIGNUM *pPub = 0,*pP = 0,*pG = 0;` |
|      - | 2875 | `	const char *zPub;` |
|    ! 0 | 2876 | `	unsigned char *zOut = 0;` |
|    ! 0 | 2877 | `	size_t nOut = 0;` |
|    ! 0 | 2878 | `	int nPub = 0,rc = -1;` |
|    ! 0 | 2879 | `	if( nArg < 2 ){` |
|    ! 0 | 2880 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2881 | `		return PH7_OK;` |
|      - | 2882 | `	}` |
|    ! 0 | 2883 | `	zPub = ph7_value_to_string(apArg[0],&nPub);` |
|    ! 0 | 2884 | `	pPriv = (EVP_PKEY *)PH7_SslHandleOf(apArg[1],PHL_SSL_KIND_KEY);` |
|    ! 0 | 2885 | `	if( pPriv == 0 \|\| zPub == 0 \|\| nPub < 1 ){` |
|    ! 0 | 2886 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2887 | `		return PH7_OK;` |
|      - | 2888 | `	}` |
|    ! 0 | 2889 | `	if( EVP_PKEY_get_bn_param(pPriv,OSSL_PKEY_PARAM_FFC_P,&pP) != 1` |
|    ! 0 | 2890 | `	 \|\| EVP_PKEY_get_bn_param(pPriv,OSSL_PKEY_PARAM_FFC_G,&pG) != 1 ){` |
|    ! 0 | 2891 | `		goto done;` |
|      - | 2892 | `	}` |
|    ! 0 | 2893 | `	pPub = BN_bin2bn((const unsigned char *)zPub,nPub,0);` |
|    ! 0 | 2894 | `	pBld = OSSL_PARAM_BLD_new();` |
|    ! 0 | 2895 | `	if( pPub == 0 \|\| pBld == 0 ){` |
|    ! 0 | 2896 | `		goto done;` |
|      - | 2897 | `	}` |
|    ! 0 | 2898 | `	if( !OSSL_PARAM_BLD_push_BN(pBld,OSSL_PKEY_PARAM_FFC_P,pP)` |
|    ! 0 | 2899 | `	 \|\| !OSSL_PARAM_BLD_push_BN(pBld,OSSL_PKEY_PARAM_FFC_G,pG)` |
|    ! 0 | 2900 | `	 \|\| !OSSL_PARAM_BLD_push_BN(pBld,OSSL_PKEY_PARAM_PUB_KEY,pPub) ){` |
|    ! 0 | 2901 | `		goto done;` |
|      - | 2902 | `	}` |
|    ! 0 | 2903 | `	pParams = OSSL_PARAM_BLD_to_param(pBld);` |
|    ! 0 | 2904 | `	if( pParams == 0 ){` |
|    ! 0 | 2905 | `		goto done;` |
|      - | 2906 | `	}` |
|    ! 0 | 2907 | `	pKCtx = EVP_PKEY_CTX_new_from_name(0,"DH",0);` |
|    ! 0 | 2908 | `	if( pKCtx == 0 \|\| EVP_PKEY_fromdata_init(pKCtx) <= 0` |
|    ! 0 | 2909 | `	 \|\| EVP_PKEY_fromdata(pKCtx,&pPeer,EVP_PKEY_PUBLIC_KEY,pParams) <= 0 ){` |
|    ! 0 | 2910 | `		goto done;` |
|      - | 2911 | `	}` |
|    ! 0 | 2912 | `	EVP_PKEY_CTX_free(pKCtx);` |
|    ! 0 | 2913 | `	pKCtx = EVP_PKEY_CTX_new(pPriv,0);` |
|    ! 0 | 2914 | `	if( pKCtx && EVP_PKEY_derive_init(pKCtx) > 0` |
|    ! 0 | 2915 | `	 && EVP_PKEY_derive_set_peer(pKCtx,pPeer) > 0` |
|    ! 0 | 2916 | `	 && EVP_PKEY_derive(pKCtx,0,&nOut) > 0 ){` |
|    ! 0 | 2917 | `		zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nOut > 0 ? nOut : 1));` |
|    ! 0 | 2918 | `		if( zOut && EVP_PKEY_derive(pKCtx,zOut,&nOut) > 0 ){` |
|    ! 0 | 2919 | `			ph7_result_string(pCtx,(const char *)zOut,(int)nOut);` |
|    ! 0 | 2920 | `			rc = 0;` |
|    ! 0 | 2921 | `		}` |
|    ! 0 | 2922 | `	}` |
|    ! 0 | 2923 | `done:` |
|    ! 0 | 2924 | `	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }` |
|    ! 0 | 2925 | `	if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }` |
|    ! 0 | 2926 | `	if( pPeer ){ EVP_PKEY_free(pPeer); }` |
|    ! 0 | 2927 | `	if( pParams ){ OSSL_PARAM_free(pParams); }` |
|    ! 0 | 2928 | `	if( pBld ){ OSSL_PARAM_BLD_free(pBld); }` |
|    ! 0 | 2929 | `	if( pPub ){ BN_free(pPub); }` |
|    ! 0 | 2930 | `	if( pP ){ BN_free(pP); }` |
|    ! 0 | 2931 | `	if( pG ){ BN_free(pG); }` |
|    ! 0 | 2932 | `	if( rc != 0 ){` |
|    ! 0 | 2933 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 2934 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2935 | `	}` |
|    ! 0 | 2936 | `	return PH7_OK;` |
|    ! 0 | 2937 | `}` |
|      - | 2938 | `/*` |
|      - | 2939 | ` * The SPKAC family. php's own asymmetry lives here and is reproduced rather` |
|      - | 2940 | `` * than smoothed: openssl_spki_new() answers a string PREFIXED with `SPKAC=`,`` |
|      - | 2941 | ` * and the three readers take the bare base64 -- so handing new()'s answer` |
|      - | 2942 | `` * straight back to verify() is `Unable to decode supplied SPKAC` under php too.`` |
|      - | 2943 | ` */` |
|     14 | 2944 | `static NETSCAPE_SPKI * SslSpkiOfArg(ph7_context *pCtx,ph7_value *pVal)` |
|      1 | 2945 | `{` |
|      - | 2946 | `	const char *zStr;` |
|     15 | 2947 | `	int nStr = 0;` |
|      7 | 2948 | `	SXUNUSED(pCtx);` |
|     15 | 2949 | `	zStr = ph7_value_to_string(pVal,&nStr);` |
|     15 | 2950 | `	if( zStr == 0 \|\| nStr < 1 ){` |
|    ! 0 | 2951 | `		return 0;` |
|      - | 2952 | `	}` |
|     15 | 2953 | `	return NETSCAPE_SPKI_b64_decode(zStr,nStr);` |
|      8 | 2954 | `}` |
|      4 | 2955 | `static int vm_builtin_openssl_spki_new(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2956 | `{` |
|      - | 2957 | `	EVP_PKEY *pKey;` |
|      - | 2958 | `	NETSCAPE_SPKI *pSpki;` |
|      - | 2959 | `	const EVP_MD *pMd;` |
|      5 | 2960 | `	EVP_MD *pFetched = 0;` |
|      - | 2961 | `	const char *zChallenge;` |
|      - | 2962 | `	char *zOut;` |
|      5 | 2963 | `	int nChallenge = 0;` |
|      5 | 2964 | `	if( nArg < 2 ){` |
|    ! 0 | 2965 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2966 | `		return PH7_OK;` |
|      - | 2967 | `	}` |
|      5 | 2968 | `	pKey = (EVP_PKEY *)PH7_SslHandleOf(apArg[0],PHL_SSL_KIND_KEY);` |
|      5 | 2969 | `	zChallenge = ph7_value_to_string(apArg[1],&nChallenge);` |
|      5 | 2970 | `	if( pKey == 0 ){` |
|    ! 0 | 2971 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2972 | `		return PH7_OK;` |
|      - | 2973 | `	}` |
|      5 | 2974 | `	if( nArg > 2 ){` |
|      5 | 2975 | `		pMd = PH7_SslDigestOfValue(apArg[2],&pFetched);` |
|      4 | 2976 | `	}else{` |
|    ! 0 | 2977 | `		pMd = EVP_md5();` |
|      - | 2978 | `	}` |
|      5 | 2979 | `	if( pMd == 0 ){` |
|    ! 0 | 2980 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");` |
|    ! 0 | 2981 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2982 | `		return PH7_OK;` |
|      - | 2983 | `	}` |
|      5 | 2984 | `	pSpki = NETSCAPE_SPKI_new();` |
|      5 | 2985 | `	if( pSpki == 0 ){` |
|    ! 0 | 2986 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|    ! 0 | 2987 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 2988 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2989 | `		return PH7_OK;` |
|      - | 2990 | `	}` |
|      4 | 2991 | `	if( (nChallenge > 0 && ASN1_STRING_set(pSpki->spkac->challenge,zChallenge,nChallenge) != 1)` |
|      4 | 2992 | `	 \|\| NETSCAPE_SPKI_set_pubkey(pSpki,pKey) != 1` |
|      4 | 2993 | `	 \|\| NETSCAPE_SPKI_sign(pSpki,pKey,pMd) <= 0 ){` |
|      2 | 2994 | `		NETSCAPE_SPKI_free(pSpki);` |
|      2 | 2995 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|    ! 0 | 2996 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 2997 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2998 | `		return PH7_OK;` |
|      - | 2999 | `	}` |
|      3 | 3000 | `	zOut = NETSCAPE_SPKI_b64_encode(pSpki);` |
|      3 | 3001 | `	NETSCAPE_SPKI_free(pSpki);` |
|      3 | 3002 | `	if( pFetched ){ EVP_MD_free(pFetched); }` |
|      3 | 3003 | `	if( zOut == 0 ){` |
|    ! 0 | 3004 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 3005 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3006 | `		return PH7_OK;` |
|      - | 3007 | `	}` |
|      3 | 3008 | `	ph7_result_string_format(pCtx,"SPKAC=%s",zOut);` |
|      3 | 3009 | `	OPENSSL_free(zOut);` |
|      3 | 3010 | `	return PH7_OK;` |
|      2 | 3011 | `}` |
|      6 | 3012 | `static int vm_builtin_openssl_spki_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3013 | `{` |
|      - | 3014 | `	NETSCAPE_SPKI *pSpki;` |
|      - | 3015 | `	EVP_PKEY *pKey;` |
|      - | 3016 | `	int iRc;` |
|      7 | 3017 | `	if( nArg < 1 ){` |
|    ! 0 | 3018 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3019 | `		return PH7_OK;` |
|      - | 3020 | `	}` |
|      7 | 3021 | `	pSpki = SslSpkiOfArg(pCtx,apArg[0]);` |
|      7 | 3022 | `	if( pSpki == 0 ){` |
|      5 | 3023 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      5 | 3024 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to decode supplied SPKAC");` |
|      5 | 3025 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3026 | `		return PH7_OK;` |
|      - | 3027 | `	}` |
|      3 | 3028 | `	pKey = NETSCAPE_SPKI_get_pubkey(pSpki);` |
|      3 | 3029 | `	iRc = pKey ? NETSCAPE_SPKI_verify(pSpki,pKey) : -1;` |
|      3 | 3030 | `	if( pKey ){ EVP_PKEY_free(pKey); }` |
|      3 | 3031 | `	NETSCAPE_SPKI_free(pSpki);` |
|      3 | 3032 | `	if( iRc != 1 ){` |
|    ! 0 | 3033 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 3034 | `	}` |
|      3 | 3035 | `	ph7_result_bool(pCtx,iRc == 1);` |
|      3 | 3036 | `	return PH7_OK;` |
|      4 | 3037 | `}` |
|      4 | 3038 | `static int vm_builtin_openssl_spki_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3039 | `{` |
|      - | 3040 | `	NETSCAPE_SPKI *pSpki;` |
|      - | 3041 | `	EVP_PKEY *pKey;` |
|      - | 3042 | `	BIO *pBio;` |
|      5 | 3043 | `	char *zMem = 0;` |
|      - | 3044 | `	long nMem;` |
|      5 | 3045 | `	if( nArg < 1 ){` |
|    ! 0 | 3046 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3047 | `		return PH7_OK;` |
|      - | 3048 | `	}` |
|      5 | 3049 | `	pSpki = SslSpkiOfArg(pCtx,apArg[0]);` |
|      5 | 3050 | `	if( pSpki == 0 ){` |
|      3 | 3051 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      3 | 3052 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to decode supplied SPKAC");` |
|      3 | 3053 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3054 | `		return PH7_OK;` |
|      - | 3055 | `	}` |
|      3 | 3056 | `	pKey = NETSCAPE_SPKI_get_pubkey(pSpki);` |
|      3 | 3057 | `	NETSCAPE_SPKI_free(pSpki);` |
|      3 | 3058 | `	pBio = pKey ? BIO_new(BIO_s_mem()) : 0;` |
|      3 | 3059 | `	if( pBio == 0 \|\| PEM_write_bio_PUBKEY(pBio,pKey) != 1 ){` |
|    ! 0 | 3060 | `		if( pBio ){ BIO_free(pBio); }` |
|    ! 0 | 3061 | `		if( pKey ){ EVP_PKEY_free(pKey); }` |
|    ! 0 | 3062 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    ! 0 | 3063 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3064 | `		return PH7_OK;` |
|      - | 3065 | `	}` |
|      3 | 3066 | `	nMem = BIO_get_mem_data(pBio,&zMem);` |
|      3 | 3067 | `	ph7_result_string(pCtx,zMem,(int)nMem);` |
|      3 | 3068 | `	BIO_free(pBio);` |
|      3 | 3069 | `	EVP_PKEY_free(pKey);` |
|      3 | 3070 | `	return PH7_OK;` |
|      3 | 3071 | `}` |
|      4 | 3072 | `static int vm_builtin_openssl_spki_export_challenge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3073 | `{` |
|      - | 3074 | `	NETSCAPE_SPKI *pSpki;` |
|      5 | 3075 | `	if( nArg < 1 ){` |
|    ! 0 | 3076 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3077 | `		return PH7_OK;` |
|      - | 3078 | `	}` |
|      5 | 3079 | `	pSpki = SslSpkiOfArg(pCtx,apArg[0]);` |
|      5 | 3080 | `	if( pSpki == 0 ){` |
|      3 | 3081 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|      3 | 3082 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to decode SPKAC");` |
|      3 | 3083 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3084 | `		return PH7_OK;` |
|      - | 3085 | `	}` |
|      4 | 3086 | `	ph7_result_string(pCtx,(const char *)ASN1_STRING_get0_data(pSpki->spkac->challenge),` |
|      2 | 3087 | `		ASN1_STRING_length(pSpki->spkac->challenge));` |
|      3 | 3088 | `	NETSCAPE_SPKI_free(pSpki);` |
|      3 | 3089 | `	return PH7_OK;` |
|      3 | 3090 | `}` |
|      - | 3091 |  |
|      - | 3092 | `/* ------------------------------------------------------------------------` |
|      - | 3093 | ` * Installation` |
|      - | 3094 | ` * ------------------------------------------------------------------------ */` |
|      - | 3095 | `/*` |
|      - | 3096 | ` * The three handle classes. php's OpenSSLCertificate,` |
|      - | 3097 | ` * OpenSSLCertificateSigningRequest and OpenSSLAsymmetricKey each declare no` |
|      - | 3098 | ` * method, no constant and no property, print as an empty object on every` |
|      - | 3099 | `` * presentation surface, refuse `new`, refuse `clone` and refuse serialization`` |
|      - | 3100 | ` * -- so the one slot here is engine storage, hidden so it appears on none of` |
|      - | 3101 | `` * them. `(int)` on one is the object handle, silently, which is`` |
|      - | 3102 | ` * PH7_CLASS_HANDLE_ID.` |
|      - | 3103 | ` */` |
|   8445 | 3104 | `PH7_PRIVATE sxi32 PH7_VmInstallOpenSsl(ph7_vm *pVm)` |
|      5 | 3105 | `{` |
|      - | 3106 | `	static const PH7_NativePropDef aProp[] = {` |
|      - | 3107 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }` |
|      - | 3108 | `	};` |
|      - | 3109 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 3110 | `		{ PHL_SSL_CLASS_CERT, 0, 0,` |
|      - | 3111 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE` |
|      - | 3112 | `		  \|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_HANDLE_ID,` |
|      - | 3113 | `		  0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), SslInstanceRelease, 0, 0 },` |
|      - | 3114 | `		{ PHL_SSL_CLASS_CSR, 0, 0,` |
|      - | 3115 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE` |
|      - | 3116 | `		  \|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_HANDLE_ID,` |
|      - | 3117 | `		  0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), SslInstanceRelease, 0, 0 },` |
|      - | 3118 | `		{ PHL_SSL_CLASS_KEY, 0, 0,` |
|      - | 3119 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE` |
|      - | 3120 | `		  \|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_HANDLE_ID,` |
|      - | 3121 | `		  0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), SslInstanceRelease, 0, 0 }` |
|      - | 3122 | `	};` |
|      - | 3123 | `	static const struct { const char *zName; const char *zRefusal; } aRefusal[] = {` |
|      - | 3124 | `		{ PHL_SSL_CLASS_CERT,` |
|      - | 3125 | `		  "Cannot directly construct OpenSSLCertificate, use openssl_x509_read() instead" },` |
|      - | 3126 | `		{ PHL_SSL_CLASS_CSR,` |
|      - | 3127 | `		  "Cannot directly construct OpenSSLCertificateSigningRequest, use openssl_csr_new() instead" },` |
|      - | 3128 | `		{ PHL_SSL_CLASS_KEY,` |
|      - | 3129 | `		  "Cannot directly construct OpenSSLAsymmetricKey, use openssl_pkey_new() instead" }` |
|      - | 3130 | `	};` |
|      - | 3131 | `	sxi32 rc;` |
|      - | 3132 | `	sxu32 n;` |
|   8450 | 3133 | `	pVm->pSslObjs = 0;` |
|   8450 | 3134 | `	pVm->pSslErrors = 0;` |
|   8450 | 3135 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|   8450 | 3136 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 3137 | `		return rc;` |
|      - | 3138 | `	}` |
|  33785 | 3139 | `	for( n = 0 ; n < SX_ARRAYSIZE(aRefusal) ; ++n ){` |
|  37991 | 3140 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),aRefusal[n].zName,` |
|  25335 | 3141 | `			(sxu32)SyStrlen(aRefusal[n].zName),FALSE,0);` |
|  25340 | 3142 | `		if( pClass ){` |
|  25340 | 3143 | `			pClass->zNewRefusal = aRefusal[n].zRefusal;` |
|  25340 | 3144 | `			pClass->xCmp = PH7_NativeCmpOpaqueHandle;` |
|  12651 | 3145 | `		}` |
|  12656 | 3146 | `	}` |
|   8450 | 3147 | `	return PH7_VmInstallOpenSslX509(&(*pVm));` |
|   4222 | 3148 | `}` |
|      - | 3149 | `/* The functions this unit owns, in php's own registration order. The` |
|      - | 3150 | ` * certificate half registers its own (vm_openssl_x509.c). */` |
|   8445 | 3151 | `PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslFuncTable(sxu32 *pnEntry)` |
|      5 | 3152 | `{` |
|      - | 3153 | `	static const ph7_builtin_func aFunc[] = {` |
|      - | 3154 | `		{ "openssl_pbkdf2",               vm_builtin_openssl_pbkdf2               },` |
|      - | 3155 | `		{ "openssl_error_string",         vm_builtin_openssl_error_string         },` |
|      - | 3156 | `		{ "openssl_get_md_methods",       vm_builtin_openssl_get_md_methods       },` |
|      - | 3157 | `		{ "openssl_get_cipher_methods",   vm_builtin_openssl_get_cipher_methods   },` |
|      - | 3158 | `		{ "openssl_get_curve_names",      vm_builtin_openssl_get_curve_names      },` |
|      - | 3159 | `		{ "openssl_digest",               vm_builtin_openssl_digest               },` |
|      - | 3160 | `		{ "openssl_encrypt",              vm_builtin_openssl_encrypt              },` |
|      - | 3161 | `		{ "openssl_decrypt",              vm_builtin_openssl_decrypt              },` |
|      - | 3162 | `		{ "openssl_cipher_iv_length",     vm_builtin_openssl_cipher_iv_length     },` |
|      - | 3163 | `		{ "openssl_cipher_key_length",    vm_builtin_openssl_cipher_key_length    },` |
|      - | 3164 | `		{ "openssl_random_pseudo_bytes",  vm_builtin_openssl_random_pseudo_bytes  },` |
|      - | 3165 | `		{ "openssl_get_cert_locations",   vm_builtin_openssl_get_cert_locations   },` |
|      - | 3166 | `		{ "openssl_pkey_new",             vm_builtin_openssl_pkey_new             },` |
|      - | 3167 | `		{ "openssl_pkey_export_to_file",  vm_builtin_openssl_pkey_export_to_file  },` |
|      - | 3168 | `		{ "openssl_pkey_export",          vm_builtin_openssl_pkey_export          },` |
|      - | 3169 | `		{ "openssl_pkey_get_public",      vm_builtin_openssl_pkey_get_public      },` |
|      - | 3170 | `		{ "openssl_get_publickey",        vm_builtin_openssl_pkey_get_public      },` |
|      - | 3171 | `		{ "openssl_pkey_get_private",     vm_builtin_openssl_pkey_get_private     },` |
|      - | 3172 | `		{ "openssl_get_privatekey",       vm_builtin_openssl_pkey_get_private     },` |
|      - | 3173 | `		{ "openssl_pkey_get_details",     vm_builtin_openssl_pkey_get_details     },` |
|      - | 3174 | `		{ "openssl_private_encrypt",      vm_builtin_openssl_private_encrypt      },` |
|      - | 3175 | `		{ "openssl_private_decrypt",      vm_builtin_openssl_private_decrypt      },` |
|      - | 3176 | `		{ "openssl_public_encrypt",       vm_builtin_openssl_public_encrypt       },` |
|      - | 3177 | `		{ "openssl_public_decrypt",       vm_builtin_openssl_public_decrypt       },` |
|      - | 3178 | `		{ "openssl_sign",                 vm_builtin_openssl_sign                 },` |
|      - | 3179 | `		{ "openssl_verify",               vm_builtin_openssl_verify               },` |
|      - | 3180 | `		{ "openssl_seal",                 vm_builtin_openssl_seal                 },` |
|      - | 3181 | `		{ "openssl_open",                 vm_builtin_openssl_open                 },` |
|      - | 3182 | `		{ "openssl_dh_compute_key",       vm_builtin_openssl_dh_compute_key       },` |
|      - | 3183 | `		{ "openssl_pkey_derive",          vm_builtin_openssl_pkey_derive          },` |
|      - | 3184 | `		{ "openssl_spki_new",             vm_builtin_openssl_spki_new             },` |
|      - | 3185 | `		{ "openssl_spki_verify",          vm_builtin_openssl_spki_verify          },` |
|      - | 3186 | `		{ "openssl_spki_export",          vm_builtin_openssl_spki_export          },` |
|      - | 3187 | `		{ "openssl_spki_export_challenge",vm_builtin_openssl_spki_export_challenge}` |
|      - | 3188 | `	};` |
|   8450 | 3189 | `	*pnEntry = (sxu32)SX_ARRAYSIZE(aFunc);` |
|   8450 | 3190 | `	return aFunc;` |
|      5 | 3191 | `}` |
|      - | 3192 |  |
|      - | 3193 | `#else` |
|      - | 3194 | `/* Ensure non-empty translation unit when openssl is disabled (MSVC C4206) */` |
|      - | 3195 | `typedef int vm_openssl_unused;` |
|      - | 3196 | `#endif /* PH7_ENABLE_OPENSSL */` |
|      - | 3197 |  |
