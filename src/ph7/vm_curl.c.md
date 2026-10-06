# src/ph7/vm_curl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1567/1815 lines (86.34%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    4 | ` */` |
|       - |    5 | `#ifdef PH7_ENABLE_CURL` |
|       - |    6 | `#include "curl_int.h"` |
|       - |    7 | `#include <errno.h>` |
|       - |    8 |  |
|       - |    9 | `/*` |
|       - |   10 | ` * Section:` |
|       - |   11 | ` *    ext/curl -- php's binding of libcurl.` |
|       - |   12 | ` * Status:` |
|       - |   13 | ` *    In progress. This unit currently carries the library-wide surface:` |
|       - |   14 | ` *    curl_version() and the three strerror() families. The handle classes,` |
|       - |   15 | ` *    the option table and the transfer verbs follow.` |
|       - |   16 | ` *` |
|       - |   17 | ` * WHY A BINDING AND NOT A REIMPLEMENTATION. php's ext/curl is a thin shell` |
|       - |   18 | ` * over libcurl, so the contract a program depends on is the LIBRARY's, not` |
|       - |   19 | ` * the extension's -- the same relationship ext/pdo_sqlite has with` |
|       - |   20 | ` * libsqlite3. Every answer here is therefore derived by asking php 8.5 and` |
|       - |   21 | ` * libcurl 8.5 the same question, never by reading php's source: see the` |
|       - |   22 | ` * per-function notes below for the ones a careful reading would have got` |
|       - |   23 | ` * wrong.` |
|       - |   24 | ` *` |
|       - |   25 | ` * MEMORY MODEL. libcurl stays on its own (system) allocator, like libxml2 and` |
|       - |   26 | ` * sqlite3 before it. Routing it through SyMemBackend would subject library` |
|       - |   27 | ` * internals to the PHL_MAX_ALLOC fault injection the stress tier uses, and` |
|       - |   28 | ` * libcurl does not tolerate mid-transfer OOM injection the way engine code` |
|       - |   29 | ` * does.` |
|       - |   30 | ` */` |
|       - |   31 |  |
|       - |   32 | `/*` |
|       - |   33 | ` * curl_global_init() once per process. libcurl's own documentation calls this` |
|       - |   34 | ` * not thread-safe, so it must not be left to the first curl_easy_init() on` |
|       - |   35 | ` * whichever thread gets there first (the -S server pre-forks, and` |
|       - |   36 | ` * PH7_ENABLE_THREADS builds share the process).` |
|       - |   37 | ` */` |
|    8447 |   38 | `PH7_PRIVATE void PH7_CurlGlobalInit(void)` |
|       5 |   39 | `{` |
|       - |   40 | `	static int bInit = 0;` |
|    8452 |   41 | `	if( !bInit ){` |
|    8450 |   42 | `		curl_global_init(CURL_GLOBAL_DEFAULT);` |
|    8450 |   43 | `		bInit = 1;` |
|    4217 |   44 | `	}` |
|    8452 |   45 | `}` |
|       - |   46 |  |
|       - |   47 | `/* ------------------------------------------------------------------------` |
|       - |   48 | ` * Handle lifetime` |
|       - |   49 | ` * ------------------------------------------------------------------------ */` |
|       - |   50 | `/*` |
|       - |   51 | ``  * A handle is reached from its CurlHandle object through the hidden `__res` `` |
|       - |   52 | ` * slot and is ALSO chained on the per-VM registry, because a PH7 resource` |
|       - |   53 | ` * carries no destructor: the sweep at VM reset/release is what closes a` |
|       - |   54 | ` * transfer a script left open. Unlike ext/pdo's connections, a CurlHandle IS` |
|       - |   55 | ` * cloneable (php maps clone to curl_easy_duphandle), so the clone gets its own` |
|       - |   56 | ` * record and its own libcurl handle -- never a second object over one CURL*.` |
|       - |   57 | ` */` |
|       - |   58 | `static void CurlFreeMime(phl_curl *pCurl);` |
|       - |   59 | `static int CurlCbParked(phl_curl *pCurl);` |
|       - |   60 | `static void CurlCbPark(phl_curl *pCurl,sxi32 rc);` |
|       - |   61 | `static int CurlSetPostFieldsArray(ph7_context *pCtx,phl_curl *pCurl,ph7_value *pVal,sxi32 *pRc);` |
|       - |   62 |  |
|     338 |   63 | `static phl_curl * CurlNewHandle(ph7_vm *pVm)` |
|       3 |   64 | `{` |
|     341 |   65 | `	phl_curl *pCurl = (phl_curl *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curl));` |
|     341 |   66 | `	if( pCurl == 0 ){` |
|     ! 0 |   67 | `		return 0;` |
|       - |   68 | `	}` |
|     341 |   69 | `	SyZero(pCurl,sizeof(phl_curl));` |
|     341 |   70 | `	pCurl->pVm = pVm;` |
|     341 |   71 | `	SyBlobInit(&pCurl->sBody,&pVm->sAllocator);` |
|     341 |   72 | `	pCurl->pEasy = curl_easy_init();` |
|     341 |   73 | `	if( pCurl->pEasy == 0 ){` |
|     ! 0 |   74 | `		SyMemBackendFree(&pVm->sAllocator,pCurl);` |
|     ! 0 |   75 | `		return 0;` |
|       - |   76 | `	}` |
|       - |   77 | `	/*` |
|       - |   78 | `	 * php gives every handle its own error buffer at creation, and that is` |
|       - |   79 | `	 * what curl_error() reports -- not curl_easy_strerror() of the code. It` |
|       - |   80 | `	 * has to be re-applied after curl_easy_reset(), which clears it.` |
|       - |   81 | `	 */` |
|     341 |   82 | `	curl_easy_setopt(pCurl->pEasy,CURLOPT_ERRORBUFFER,pCurl->zErrBuf);` |
|     341 |   83 | `	pCurl->pNext = (phl_curl *)pVm->pCurlHandles;` |
|     341 |   84 | `	pVm->pCurlHandles = pCurl;` |
|     341 |   85 | `	return pCurl;` |
|     172 |   86 | `}` |
|       - |   87 | `/*` |
|       - |   88 | ` * The slot a handle keeps one option's curl_slist in, created on first use.` |
|       - |   89 | ` * Keyed by option, so setting CURLOPT_HTTPHEADER twice replaces one list` |
|       - |   90 | ` * rather than accumulating two.` |
|       - |   91 | ` */` |
|      30 |   92 | `static phl_curl_slist * CurlSlistSlot(phl_curl *pCurl,sxi64 iOpt)` |
|       1 |   93 | `{` |
|      31 |   94 | `	phl_curl_slist *pSlot = pCurl->pSlists;` |
|      35 |   95 | `	while( pSlot ){` |
|      21 |   96 | `		if( pSlot->iOpt == iOpt ){` |
|      17 |   97 | `			return pSlot;` |
|       - |   98 | `		}` |
|       5 |   99 | `		pSlot = pSlot->pNext;` |
|       1 |  100 | `	}` |
|      15 |  101 | `	pSlot = (phl_curl_slist *)SyMemBackendAlloc(&pCurl->pVm->sAllocator,sizeof(phl_curl_slist));` |
|      15 |  102 | `	if( pSlot == 0 ){` |
|     ! 0 |  103 | `		return 0;` |
|       - |  104 | `	}` |
|      15 |  105 | `	SyZero(pSlot,sizeof(phl_curl_slist));` |
|      15 |  106 | `	pSlot->iOpt = iOpt;` |
|      15 |  107 | `	pSlot->pNext = pCurl->pSlists;` |
|      15 |  108 | `	pCurl->pSlists = pSlot;` |
|      15 |  109 | `	return pSlot;` |
|      16 |  110 | `}` |
|     750 |  111 | `static void CurlFreeSlists(phl_curl *pCurl)` |
|       3 |  112 | `{` |
|     753 |  113 | `	phl_curl_slist *pSlot = pCurl->pSlists;` |
|     767 |  114 | `	while( pSlot ){` |
|      15 |  115 | `		phl_curl_slist *pNext = pSlot->pNext;` |
|      15 |  116 | `		if( pSlot->pList ){` |
|      15 |  117 | `			curl_slist_free_all(pSlot->pList);` |
|       7 |  118 | `		}` |
|      15 |  119 | `		SyMemBackendFree(&pCurl->pVm->sAllocator,pSlot);` |
|      15 |  120 | `		pSlot = pNext;` |
|       1 |  121 | `	}` |
|     753 |  122 | `	pCurl->pSlists = 0;` |
|     753 |  123 | `}` |
|     750 |  124 | `static void CurlDropCallbacks(phl_curl *pCurl)` |
|       3 |  125 | `{` |
|       - |  126 | `	ph7_value **apCb[9];` |
|       - |  127 | `	int i;` |
|     753 |  128 | `	apCb[0] = &pCurl->pWriteCb;` |
|     753 |  129 | `	apCb[1] = &pCurl->pHeaderCb;` |
|     753 |  130 | `	apCb[2] = &pCurl->pXferCb;` |
|     753 |  131 | `	apCb[3] = &pCurl->pReadCb;` |
|     753 |  132 | `	apCb[4] = &pCurl->pDebugCb;` |
|       - |  133 | `	/* The stream slots go with them: they are php-side state too, and` |
|       - |  134 | `	 * curl_reset() puts every option back. */` |
|     753 |  135 | `	apCb[5] = &pCurl->pWriteStream;` |
|     753 |  136 | `	apCb[6] = &pCurl->pHeaderStream;` |
|     753 |  137 | `	apCb[7] = &pCurl->pStderrStream;` |
|     753 |  138 | `	apCb[8] = &pCurl->pPreReqCb;` |
|    7503 |  139 | `	for( i = 0 ; i < 9 ; ++i ){` |
|    6753 |  140 | `		if( *apCb[i] ){` |
|     107 |  141 | `			ph7_release_value(pCurl->pVm,*apCb[i]);` |
|     107 |  142 | `			*apCb[i] = 0;` |
|      53 |  143 | `		}` |
|    3378 |  144 | `	}` |
|     753 |  145 | `	if( pCurl->pReadStream ){` |
|       4 |  146 | `		ph7_release_value(pCurl->pVm,pCurl->pReadStream);` |
|       4 |  147 | `		pCurl->pReadStream = 0;` |
|       2 |  148 | `	}` |
|     753 |  149 | `	if( pCurl->pShare ){` |
|       - |  150 | `		/* curl_easy_reset() has just dropped libcurl's own pointer to it. */` |
|       3 |  151 | `		ph7_release_value(pCurl->pVm,pCurl->pShare);` |
|       3 |  152 | `		pCurl->pShare = 0;` |
|       1 |  153 | `	}` |
|     753 |  154 | `	pCurl->iHeaderDest = PHL_CURL_HDR_IGNORE;` |
|     753 |  155 | `}` |
|       - |  156 | `/*` |
|       - |  157 | ` * Give a record its OWN copy of a stored value -- a retained callable, or the` |
|       - |  158 | ` * one CURLOPT_PRIVATE keeps. The two handles must not share one ph7_value:` |
|       - |  159 | ` * whichever released it first would leave the other reading freed memory, and` |
|       - |  160 | ` * for a callable that read happens from inside libcurl.` |
|       - |  161 | ` */` |
|     432 |  162 | `static void CurlCopyValue(ph7_vm *pVm,ph7_value **ppSlot,ph7_value *pFrom)` |
|       1 |  163 | `{` |
|     433 |  164 | `	if( pFrom == 0 ){` |
|     423 |  165 | `		return;` |
|       - |  166 | `	}` |
|      11 |  167 | `	*ppSlot = ph7_new_scalar(pVm);` |
|      11 |  168 | `	if( *ppSlot ){` |
|      11 |  169 | `		PH7_MemObjStore(pFrom,*ppSlot);` |
|       5 |  170 | `	}` |
|     217 |  171 | `}` |
|     734 |  172 | `static void CurlFreeHandle(phl_curl *pCurl)` |
|       3 |  173 | `{` |
|     737 |  174 | `	if( pCurl->pEasy ){` |
|       - |  175 | `		/* The handle goes first: libcurl reads the lists during a transfer and` |
|       - |  176 | `		 * must not be left pointing at freed memory even for an instant. */` |
|     377 |  177 | `		curl_easy_cleanup(pCurl->pEasy);` |
|     377 |  178 | `		pCurl->pEasy = 0;` |
|     187 |  179 | `	}` |
|     737 |  180 | `	CurlFreeSlists(pCurl);` |
|     737 |  181 | `	CurlDropCallbacks(pCurl);` |
|     737 |  182 | `	CurlFreeMime(pCurl);` |
|     737 |  183 | `	SyBlobRelease(&pCurl->sBody);` |
|     737 |  184 | `	if( pCurl->pPrivate ){` |
|       7 |  185 | `		ph7_release_value(pCurl->pVm,pCurl->pPrivate);` |
|       7 |  186 | `		pCurl->pPrivate = 0;` |
|       3 |  187 | `	}` |
|     737 |  188 | `	if( pCurl->pShare ){` |
|     ! 0 |  189 | `		ph7_release_value(pCurl->pVm,pCurl->pShare);` |
|     ! 0 |  190 | `		pCurl->pShare = 0;` |
|     ! 0 |  191 | `	}` |
|     737 |  192 | `}` |
|       - |  193 | `/*` |
|       - |  194 | ` * Free every registered handle. Called from PH7_CurlVmReset (a reused VM --` |
|       - |  195 | ` * the -S server's -- must not answer the next request through a connection the` |
|       - |  196 | ` * previous one opened) and from PH7_CurlVmRelease before the allocator holding` |
|       - |  197 | ` * the shells is torn down.` |
|       - |  198 | ` */` |
|    7011 |  199 | `static void CurlVmSweep(ph7_vm *pVm)` |
|       5 |  200 | `{` |
|       - |  201 | `	phl_curl *pCurl;` |
|       - |  202 | `	/* The multis first: one still holds the easy handles it was given, and` |
|       - |  203 | `	 * curl_multi_remove_handle has to reach a CURL* that is still there. */` |
|    7016 |  204 | `	PH7_CurlMultiVmSweep(&(*pVm));` |
|    7016 |  205 | `	pCurl = (phl_curl *)pVm->pCurlHandles;` |
|    7390 |  206 | `	while( pCurl ){` |
|     377 |  207 | `		phl_curl *pNext = pCurl->pNext;` |
|     377 |  208 | `		PH7_CurlBlankSlot(pCurl->pOwner);` |
|     377 |  209 | `		CurlFreeHandle(pCurl);` |
|     377 |  210 | `		SyMemBackendFree(&pVm->sAllocator,pCurl);` |
|     377 |  211 | `		pCurl = pNext;` |
|       3 |  212 | `	}` |
|    7016 |  213 | `	pVm->pCurlHandles = 0;` |
|       - |  214 | `	/* And the shares last: one an easy handle was still attached to refuses to` |
|       - |  215 | `	 * be cleaned up, so nothing may name it by now. */` |
|    7016 |  216 | `	PH7_CurlShareVmSweep(&(*pVm));` |
|    7016 |  217 | `}` |
|      16 |  218 | `PH7_PRIVATE void PH7_CurlVmReset(ph7_vm *pVm)` |
|     ! 0 |  219 | `{` |
|      16 |  220 | `	CurlVmSweep(&(*pVm));` |
|      16 |  221 | `}` |
|    6995 |  222 | `PH7_PRIVATE void PH7_CurlVmRelease(ph7_vm *pVm)` |
|       5 |  223 | `{` |
|    7000 |  224 | `	CurlVmSweep(&(*pVm));` |
|    7000 |  225 | `}` |
|       - |  226 | `/*` |
|       - |  227 | ` * Blank the hidden slot of the object whose record we are about to free, so` |
|       - |  228 | ` * the object cannot outlive its record and then read freed memory to ask` |
|       - |  229 | ` * whether it still owns one.` |
|       - |  230 | ` *` |
|       - |  231 | ` * The three slot helpers are shared with the multi/share unit rather than` |
|       - |  232 | ` * duplicated there: every handle class in this extension keeps its record in` |
|       - |  233 | `` * the same hidden `__res` slot, so the only thing that differs is what the`` |
|       - |  234 | ` * pointer points AT.` |
|       - |  235 | ` */` |
|     426 |  236 | `PH7_PRIVATE void PH7_CurlBlankSlot(ph7_class_instance *pOwner)` |
|       3 |  237 | `{` |
|       - |  238 | `	SyString sAttr;` |
|       - |  239 | `	ph7_value *pRes;` |
|     429 |  240 | `	if( pOwner == 0 ){` |
|     415 |  241 | `		return;` |
|       - |  242 | `	}` |
|      16 |  243 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|      16 |  244 | `	pRes = PH7_ClassInstanceFetchAttr(pOwner,&sAttr);` |
|      16 |  245 | `	if( pRes ){` |
|      16 |  246 | `		PH7_MemObjRelease(pRes);` |
|      16 |  247 | `		MemObjSetType(pRes,MEMOBJ_NULL);` |
|       7 |  248 | `	}` |
|     216 |  249 | `}` |
|       - |  250 | ``/* The record behind a `__res` slot value. */`` |
|    2522 |  251 | `static void * CurlRecOfValue(ph7_value *pVal)` |
|       3 |  252 | `{` |
|    2525 |  253 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|       3 |  254 | `		return 0;` |
|       - |  255 | `	}` |
|    2523 |  256 | `	return pVal->x.pOther;` |
|    1274 |  257 | `}` |
|    2522 |  258 | `PH7_PRIVATE void * PH7_CurlSlotOf(ph7_class_instance *pThis)` |
|       3 |  259 | `{` |
|       - |  260 | `	SyString sAttr;` |
|    2525 |  261 | `	if( pThis == 0 ){` |
|     ! 0 |  262 | `		return 0;` |
|       - |  263 | `	}` |
|    2525 |  264 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|    2525 |  265 | `	return CurlRecOfValue(PH7_ClassInstanceFetchAttr(pThis,&sAttr));` |
|    1274 |  266 | `}` |
|       - |  267 | `/* Store one record in the receiver's hidden slot. */` |
|     426 |  268 | `PH7_PRIVATE int PH7_CurlSlotAttach(ph7_class_instance *pThis,void *pRec)` |
|       3 |  269 | `{` |
|       - |  270 | `	SyString sAttr;` |
|       - |  271 | `	ph7_value *pRes;` |
|     429 |  272 | `	if( pThis == 0 ){` |
|     ! 0 |  273 | `		return -1;` |
|       - |  274 | `	}` |
|     429 |  275 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|     429 |  276 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|     429 |  277 | `	if( pRes == 0 ){` |
|     ! 0 |  278 | `		return -1;` |
|       - |  279 | `	}` |
|     429 |  280 | `	PH7_MemObjRelease(pRes);` |
|     429 |  281 | `	pRes->x.pOther = pRec;` |
|     429 |  282 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|     429 |  283 | `	return 0;` |
|     216 |  284 | `}` |
|    2100 |  285 | `static phl_curl * CurlOfInstance(ph7_class_instance *pThis)` |
|       3 |  286 | `{` |
|    2103 |  287 | `	return (phl_curl *)PH7_CurlSlotOf(pThis);` |
|       3 |  288 | `}` |
|     374 |  289 | `static int CurlAttach(ph7_class_instance *pThis,phl_curl *pCurl)` |
|       3 |  290 | `{` |
|     377 |  291 | `	if( PH7_CurlSlotAttach(pThis,(void *)pCurl) != 0 ){` |
|     ! 0 |  292 | `		return -1;` |
|       - |  293 | `	}` |
|     377 |  294 | `	pCurl->pOwner = pThis;` |
|     377 |  295 | `	return 0;` |
|     190 |  296 | `}` |
|       - |  297 | `/*` |
|       - |  298 | ` * The phl_curl behind a CurlHandle the multi unit was handed. The signature` |
|       - |  299 | ` * table has already screened the class, so a miss is an engine-torn-down` |
|       - |  300 | ` * object rather than anything a script can write.` |
|       - |  301 | ` */` |
|     234 |  302 | `PH7_PRIVATE void * PH7_CurlEasyOfInstance(ph7_class_instance *pThis)` |
|       1 |  303 | `{` |
|     235 |  304 | `	return (void *)CurlOfInstance(pThis);` |
|       1 |  305 | `}` |
|       - |  306 | `/*` |
|       - |  307 | ` * The object is going away: close its transfer now rather than at VM reset, so` |
|       - |  308 | ` * a script that drops its last reference releases the socket there. The shell` |
|       - |  309 | ` * stays on the registry (the sweep frees it) because the slot is still` |
|       - |  310 | ` * reachable while the instance is being torn down.` |
|       - |  311 | ` */` |
|     360 |  312 | `static void CurlInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       3 |  313 | `{` |
|     363 |  314 | `	phl_curl *pCurl = CurlOfInstance(pThis);` |
|     180 |  315 | `	SXUNUSED(pVm);` |
|     363 |  316 | `	if( pCurl == 0 \|\| pCurl->pOwner != pThis ){` |
|     ! 0 |  317 | `		return;` |
|       - |  318 | `	}` |
|     363 |  319 | `	CurlFreeHandle(pCurl);` |
|     363 |  320 | `	pCurl->pOwner = 0;` |
|     183 |  321 | `}` |
|       - |  322 | `/*` |
|       - |  323 | ` * php's clone_obj for CurlHandle: curl_easy_duphandle(). Every OPTION comes` |
|       - |  324 | ` * across and nothing else does -- the copy starts with a clean error state` |
|       - |  325 | ` * even when the source's last transfer failed.` |
|       - |  326 | ` *` |
|       - |  327 | ` * The error buffer is why this needs care rather than a bare duphandle.` |
|       - |  328 | ` * CURLOPT_ERRORBUFFER is an option like any other, so libcurl copies its` |
|       - |  329 | ` * VALUE: the clone would point at the SOURCE's buffer, write its own failures` |
|       - |  330 | ` * into it (php's answer for the source would change when the clone failed) and` |
|       - |  331 | ` * keep writing there after the source was freed. Re-pointing it at the clone's` |
|       - |  332 | ` * own storage is what php does and what makes the two independent.` |
|       - |  333 | ` *` |
|       - |  334 | `` * Runs after the slot-by-slot copy, so the clone's `__res` currently holds the`` |
|       - |  335 | ` * SOURCE's record: every exit here has to overwrite or blank it, or two` |
|       - |  336 | ` * instances would free one CURL*.` |
|       - |  337 | ` */` |
|      36 |  338 | `static void CurlInstanceClone(ph7_vm *pVm,ph7_class_instance *pClone,ph7_class_instance *pSrc)` |
|       1 |  339 | `{` |
|      37 |  340 | `	phl_curl *pFrom = CurlOfInstance(pSrc);` |
|       - |  341 | `	phl_curl *pNew;` |
|       - |  342 | `	CURL *pDup;` |
|      37 |  343 | `	if( pFrom == 0 \|\| pFrom->pEasy == 0 ){` |
|     ! 0 |  344 | `		PH7_CurlBlankSlot(pClone);` |
|     ! 0 |  345 | `		return;` |
|       - |  346 | `	}` |
|      37 |  347 | `	pNew = (phl_curl *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curl));` |
|      37 |  348 | `	if( pNew == 0 ){` |
|     ! 0 |  349 | `		PH7_CurlBlankSlot(pClone);` |
|     ! 0 |  350 | `		return;` |
|       - |  351 | `	}` |
|      37 |  352 | `	SyZero(pNew,sizeof(phl_curl));` |
|      37 |  353 | `	pNew->pVm = pVm;` |
|      37 |  354 | `	SyBlobInit(&pNew->sBody,&pVm->sAllocator);` |
|      37 |  355 | `	pDup = curl_easy_duphandle(pFrom->pEasy);` |
|      37 |  356 | `	if( pDup == 0 ){` |
|     ! 0 |  357 | `		SyMemBackendFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  358 | `		PH7_CurlBlankSlot(pClone);` |
|     ! 0 |  359 | `		return;` |
|       - |  360 | `	}` |
|      37 |  361 | `	pNew->pEasy = pDup;` |
|      37 |  362 | `	curl_easy_setopt(pDup,CURLOPT_ERRORBUFFER,pNew->zErrBuf);` |
|       - |  363 | `	/*` |
|       - |  364 | `	 * duphandle copies the option VALUES, and every handler this engine` |
|       - |  365 | `	 * installs carries the SOURCE record as its argument. The copy installs its` |
|       - |  366 | `	 * own before it transfers, but nothing may be left pointing at another` |
|       - |  367 | `	 * handle's record in between -- so they come off here, which also puts the` |
|       - |  368 | `	 * copy back on libcurl's defaults exactly as a fresh handle is.` |
|       - |  369 | `	 */` |
|      37 |  370 | `	curl_easy_setopt(pDup,CURLOPT_WRITEFUNCTION,(curl_write_callback)0);` |
|      37 |  371 | `	curl_easy_setopt(pDup,CURLOPT_WRITEDATA,(void *)0);` |
|      37 |  372 | `	curl_easy_setopt(pDup,CURLOPT_HEADERFUNCTION,(curl_write_callback)0);` |
|      37 |  373 | `	curl_easy_setopt(pDup,CURLOPT_HEADERDATA,(void *)0);` |
|      37 |  374 | `	curl_easy_setopt(pDup,CURLOPT_READFUNCTION,(curl_read_callback)0);` |
|      37 |  375 | `	curl_easy_setopt(pDup,CURLOPT_READDATA,(void *)0);` |
|      37 |  376 | `	curl_easy_setopt(pDup,CURLOPT_DEBUGFUNCTION,(curl_debug_callback)0);` |
|      37 |  377 | `	curl_easy_setopt(pDup,CURLOPT_DEBUGDATA,(void *)0);` |
|      37 |  378 | `	curl_easy_setopt(pDup,CURLOPT_XFERINFOFUNCTION,(curl_xferinfo_callback)0);` |
|      37 |  379 | `	curl_easy_setopt(pDup,CURLOPT_XFERINFODATA,(void *)0);` |
|      37 |  380 | `	curl_easy_setopt(pDup,CURLOPT_PREREQFUNCTION,(curl_prereq_callback)0);` |
|      37 |  381 | `	curl_easy_setopt(pDup,CURLOPT_PREREQDATA,(void *)0);` |
|       - |  382 | `	/*` |
|       - |  383 | `	 * The slists have to be rebuilt for the copy, and pointed at from the copy,` |
|       - |  384 | `	 * for the same reason the source owns them: libcurl stores the POINTER.` |
|       - |  385 | `	 * Left alone, the copy would read the source's lists and keep reading them` |
|       - |  386 | `	 * after the source freed them. (php duplicates them here too, which is the` |
|       - |  387 | `	 * evidence that duphandle does not.)` |
|       - |  388 | `	 */` |
|       - |  389 | `	{` |
|      37 |  390 | `		phl_curl_slist *pFromSlot = pFrom->pSlists;` |
|      41 |  391 | `		while( pFromSlot ){` |
|       5 |  392 | `			struct curl_slist *pCopy = 0;` |
|       5 |  393 | `			struct curl_slist *pWalk = pFromSlot->pList;` |
|       5 |  394 | `			int bOk = 1;` |
|       9 |  395 | `			while( pWalk ){` |
|       5 |  396 | `				struct curl_slist *pNextNode = curl_slist_append(pCopy,pWalk->data);` |
|       5 |  397 | `				if( pNextNode == 0 ){ bOk = 0; break; }` |
|       5 |  398 | `				pCopy = pNextNode;` |
|       5 |  399 | `				pWalk = pWalk->next;` |
|       1 |  400 | `			}` |
|       5 |  401 | `			if( bOk ){` |
|       5 |  402 | `				phl_curl_slist *pSlot = CurlSlistSlot(pNew,pFromSlot->iOpt);` |
|       5 |  403 | `				if( pSlot ){` |
|       5 |  404 | `					pSlot->pList = pCopy;` |
|       5 |  405 | `					curl_easy_setopt(pDup,(CURLoption)pFromSlot->iOpt,pCopy);` |
|       2 |  406 | `				}else if( pCopy ){` |
|     ! 0 |  407 | `					curl_slist_free_all(pCopy);` |
|       1 |  408 | `				}` |
|       2 |  409 | `			}else if( pCopy ){` |
|     ! 0 |  410 | `				curl_slist_free_all(pCopy);` |
|     ! 0 |  411 | `			}` |
|       5 |  412 | `			pFromSlot = pFromSlot->pNext;` |
|       1 |  413 | `		}` |
|       - |  414 | `	}` |
|       - |  415 | `	/*` |
|       - |  416 | `	 * The state libcurl knows nothing about travels too. php copies the write` |
|       - |  417 | `	 * destination and every retained callable into the duplicate -- a copied` |
|       - |  418 | `	 * handle that answered its body and called back would otherwise print it` |
|       - |  419 | `	 * and call nothing, which is the same wrong answer twice.` |
|       - |  420 | `	 */` |
|      37 |  421 | `	pNew->iWriteDest = pFrom->iWriteDest;` |
|      37 |  422 | `	pNew->bXferIsProgress = pFrom->bXferIsProgress;` |
|      37 |  423 | `	CurlCopyValue(pVm,&pNew->pWriteCb,pFrom->pWriteCb);` |
|      37 |  424 | `	CurlCopyValue(pVm,&pNew->pHeaderCb,pFrom->pHeaderCb);` |
|      37 |  425 | `	CurlCopyValue(pVm,&pNew->pXferCb,pFrom->pXferCb);` |
|      37 |  426 | `	CurlCopyValue(pVm,&pNew->pPrivate,pFrom->pPrivate);` |
|      37 |  427 | `	CurlCopyValue(pVm,&pNew->pReadCb,pFrom->pReadCb);` |
|      37 |  428 | `	CurlCopyValue(pVm,&pNew->pDebugCb,pFrom->pDebugCb);` |
|      37 |  429 | `	CurlCopyValue(pVm,&pNew->pPreReqCb,pFrom->pPreReqCb);` |
|      37 |  430 | `	CurlCopyValue(pVm,&pNew->pWriteStream,pFrom->pWriteStream);` |
|      37 |  431 | `	CurlCopyValue(pVm,&pNew->pHeaderStream,pFrom->pHeaderStream);` |
|      37 |  432 | `	CurlCopyValue(pVm,&pNew->pStderrStream,pFrom->pStderrStream);` |
|      37 |  433 | `	CurlCopyValue(pVm,&pNew->pReadStream,pFrom->pReadStream);` |
|       - |  434 | `	/* duphandle copies the CURLSH pointer, so the copy names the same share and` |
|       - |  435 | `	 * has to hold it alive too. */` |
|      37 |  436 | `	CurlCopyValue(pVm,&pNew->pShare,pFrom->pShare);` |
|      37 |  437 | `	pNew->iHeaderDest = pFrom->iHeaderDest;` |
|      37 |  438 | `	pNew->pNext = (phl_curl *)pVm->pCurlHandles;` |
|      37 |  439 | `	pVm->pCurlHandles = pNew;` |
|       - |  440 | `	/*` |
|       - |  441 | `	 * A multipart body is REBUILT rather than copied, which is what php does` |
|       - |  442 | `	 * and the only thing that can be done: duphandle copies a callback part by` |
|       - |  443 | `	 * copying its argument, so the copy would read the source's open streams` |
|       - |  444 | `	 * and both would try to close them. Rebuilding re-opens every CURLFile,` |
|       - |  445 | `	 * which is visible -- a copy made after the file was unlinked fails where` |
|       - |  446 | `	 * the original still succeeds -- and that is php's answer too.` |
|       - |  447 | `	 */` |
|      37 |  448 | `	if( pFrom->pPostArray ){` |
|       8 |  449 | `		sxi32 rcThrow = PH7_OK;` |
|       8 |  450 | `		CurlSetPostFieldsArray(0,pNew,pFrom->pPostArray,&rcThrow);` |
|       4 |  451 | `	}` |
|      37 |  452 | `	if( CurlAttach(pClone,pNew) != 0 ){` |
|     ! 0 |  453 | `		PH7_CurlBlankSlot(pClone);` |
|     ! 0 |  454 | `	}` |
|      19 |  455 | `}` |
|       - |  456 |  |
|       - |  457 | `/*` |
|       - |  458 | ` * The CurlHandle argument of every verb. The signature table has already` |
|       - |  459 | ` * screened the TYPE (php's "must be of type CurlHandle, null given" comes from` |
|       - |  460 | ` * there), so a miss here means the object is one the engine tore down -- which` |
|       - |  461 | ` * php cannot produce and which must not be a crash.` |
|       - |  462 | ` */` |
|    1446 |  463 | `static phl_curl * CurlArg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  464 | `{` |
|       - |  465 | `	ph7_class_instance *pThis;` |
|     723 |  466 | `	SXUNUSED(nArg);` |
|    1447 |  467 | `	pThis = (apArg && ph7_value_is_object(apArg[0])) ?` |
|    2169 |  468 | `		(ph7_class_instance *)apArg[0]->x.pOther : 0;` |
|    1447 |  469 | `	if( pThis == 0 ){` |
|     ! 0 |  470 | `		return 0;` |
|       - |  471 | `	}` |
|     723 |  472 | `	SXUNUSED(pCtx);` |
|    1447 |  473 | `	return CurlOfInstance(pThis);` |
|     724 |  474 | `}` |
|       - |  475 |  |
|       - |  476 | `/* ===== Constants ===== */` |
|       - |  477 |  |
|       - |  478 | `/*` |
|       - |  479 | ` * php's whole ext/curl constant surface: 679 names, dumped from php 8.5.9's` |
|       - |  480 | ` * own get_defined_constants(true)['curl'] and kept in php's REGISTRATION` |
|       - |  481 | ` * order (which is what get_defined_constants reports).` |
|       - |  482 | ` *` |
|       - |  483 | ` * 676 of the 679 are libcurl symbols, and every one of those was diffed` |
|       - |  484 | ` * against the library: php's value and libcurl's agree on all 676, so naming` |
|       - |  485 | ` * the SYMBOL rather than the number is what keeps PHL and a php built against` |
|       - |  486 | ` * the same library in step -- and it is why no version gate is needed. The` |
|       - |  487 | ` * floor in curl_int.h is 8.5.0, the whole table compiles against it, and a` |
|       - |  488 | ` * newer libcurl only ever ADDS names.` |
|       - |  489 | ` *` |
|       - |  490 | ` * The three that are NOT libcurl symbols are php's own inventions and are the` |
|       - |  491 | ` * only literals here: CURLOPT_RETURNTRANSFER and CURLOPT_BINARYTRANSFER are` |
|       - |  492 | ` * php-side options in a range no libcurl option occupies, and` |
|       - |  493 | ` * CURLOPT_SAFE_UPLOAD is php's -1 sentinel.` |
|       - |  494 | ` *` |
|       - |  495 | ` * The values do not all fit an int: CURLAUTH_ONLY is 2147483648 and` |
|       - |  496 | ` * CURLAUTH_ANY is -17 (libcurl's ~CURLAUTH_DIGEST_IE over an unsigned long),` |
|       - |  497 | ` * so the row carries a 64-bit value and the expander reads it through the row` |
|       - |  498 | ` * pointer rather than through SX_INT_TO_PTR.` |
|       - |  499 | ` */` |
|       - |  500 | `static const struct CurlConstant {` |
|       - |  501 | `	const char *zName;` |
|       - |  502 | `	sxi64 iValue;` |
|       - |  503 | `} aCurlConst[] = {` |
|       - |  504 | `	{ "CURLOPT_AUTOREFERER",                     CURLOPT_AUTOREFERER },` |
|       - |  505 | `	{ "CURLOPT_BINARYTRANSFER",                  19914 },` |
|       - |  506 | `	{ "CURLOPT_BUFFERSIZE",                      CURLOPT_BUFFERSIZE },` |
|       - |  507 | `	{ "CURLOPT_CAINFO",                          CURLOPT_CAINFO },` |
|       - |  508 | `	{ "CURLOPT_CAPATH",                          CURLOPT_CAPATH },` |
|       - |  509 | `	{ "CURLOPT_CONNECTTIMEOUT",                  CURLOPT_CONNECTTIMEOUT },` |
|       - |  510 | `	{ "CURLOPT_COOKIE",                          CURLOPT_COOKIE },` |
|       - |  511 | `	{ "CURLOPT_COOKIEFILE",                      CURLOPT_COOKIEFILE },` |
|       - |  512 | `	{ "CURLOPT_COOKIEJAR",                       CURLOPT_COOKIEJAR },` |
|       - |  513 | `	{ "CURLOPT_COOKIESESSION",                   CURLOPT_COOKIESESSION },` |
|       - |  514 | `	{ "CURLOPT_CRLF",                            CURLOPT_CRLF },` |
|       - |  515 | `	{ "CURLOPT_CUSTOMREQUEST",                   CURLOPT_CUSTOMREQUEST },` |
|       - |  516 | `	{ "CURLOPT_DNS_CACHE_TIMEOUT",               CURLOPT_DNS_CACHE_TIMEOUT },` |
|       - |  517 | `	{ "CURLOPT_DNS_USE_GLOBAL_CACHE",            CURLOPT_DNS_USE_GLOBAL_CACHE },` |
|       - |  518 | `	{ "CURLOPT_EGDSOCKET",                       CURLOPT_EGDSOCKET },` |
|       - |  519 | `	{ "CURLOPT_ENCODING",                        CURLOPT_ENCODING },` |
|       - |  520 | `	{ "CURLOPT_FAILONERROR",                     CURLOPT_FAILONERROR },` |
|       - |  521 | `	{ "CURLOPT_FILE",                            CURLOPT_FILE },` |
|       - |  522 | `	{ "CURLOPT_FILETIME",                        CURLOPT_FILETIME },` |
|       - |  523 | `	{ "CURLOPT_FOLLOWLOCATION",                  CURLOPT_FOLLOWLOCATION },` |
|       - |  524 | `	{ "CURLOPT_FORBID_REUSE",                    CURLOPT_FORBID_REUSE },` |
|       - |  525 | `	{ "CURLOPT_FRESH_CONNECT",                   CURLOPT_FRESH_CONNECT },` |
|       - |  526 | `	{ "CURLOPT_FTPAPPEND",                       CURLOPT_FTPAPPEND },` |
|       - |  527 | `	{ "CURLOPT_FTPLISTONLY",                     CURLOPT_FTPLISTONLY },` |
|       - |  528 | `	{ "CURLOPT_FTPPORT",                         CURLOPT_FTPPORT },` |
|       - |  529 | `	{ "CURLOPT_FTP_USE_EPRT",                    CURLOPT_FTP_USE_EPRT },` |
|       - |  530 | `	{ "CURLOPT_FTP_USE_EPSV",                    CURLOPT_FTP_USE_EPSV },` |
|       - |  531 | `	{ "CURLOPT_HEADER",                          CURLOPT_HEADER },` |
|       - |  532 | `	{ "CURLOPT_HEADERFUNCTION",                  CURLOPT_HEADERFUNCTION },` |
|       - |  533 | `	{ "CURLOPT_HTTP200ALIASES",                  CURLOPT_HTTP200ALIASES },` |
|       - |  534 | `	{ "CURLOPT_HTTPGET",                         CURLOPT_HTTPGET },` |
|       - |  535 | `	{ "CURLOPT_HTTPHEADER",                      CURLOPT_HTTPHEADER },` |
|       - |  536 | `	{ "CURLOPT_HTTPPROXYTUNNEL",                 CURLOPT_HTTPPROXYTUNNEL },` |
|       - |  537 | `	{ "CURLOPT_HTTP_VERSION",                    CURLOPT_HTTP_VERSION },` |
|       - |  538 | `	{ "CURLOPT_INFILE",                          CURLOPT_INFILE },` |
|       - |  539 | `	{ "CURLOPT_INFILESIZE",                      CURLOPT_INFILESIZE },` |
|       - |  540 | `	{ "CURLOPT_INFILESIZE_LARGE",                CURLOPT_INFILESIZE_LARGE },` |
|       - |  541 | `	{ "CURLOPT_INTERFACE",                       CURLOPT_INTERFACE },` |
|       - |  542 | `	{ "CURLOPT_KRB4LEVEL",                       CURLOPT_KRB4LEVEL },` |
|       - |  543 | `	{ "CURLOPT_LOW_SPEED_LIMIT",                 CURLOPT_LOW_SPEED_LIMIT },` |
|       - |  544 | `	{ "CURLOPT_LOW_SPEED_TIME",                  CURLOPT_LOW_SPEED_TIME },` |
|       - |  545 | `	{ "CURLOPT_MAXCONNECTS",                     CURLOPT_MAXCONNECTS },` |
|       - |  546 | `	{ "CURLOPT_MAXREDIRS",                       CURLOPT_MAXREDIRS },` |
|       - |  547 | `	{ "CURLOPT_NETRC",                           CURLOPT_NETRC },` |
|       - |  548 | `	{ "CURLOPT_NOBODY",                          CURLOPT_NOBODY },` |
|       - |  549 | `	{ "CURLOPT_NOPROGRESS",                      CURLOPT_NOPROGRESS },` |
|       - |  550 | `	{ "CURLOPT_NOSIGNAL",                        CURLOPT_NOSIGNAL },` |
|       - |  551 | `	{ "CURLOPT_PORT",                            CURLOPT_PORT },` |
|       - |  552 | `	{ "CURLOPT_POST",                            CURLOPT_POST },` |
|       - |  553 | `	{ "CURLOPT_POSTFIELDS",                      CURLOPT_POSTFIELDS },` |
|       - |  554 | `	{ "CURLOPT_POSTQUOTE",                       CURLOPT_POSTQUOTE },` |
|       - |  555 | `	{ "CURLOPT_PREQUOTE",                        CURLOPT_PREQUOTE },` |
|       - |  556 | `	{ "CURLOPT_PRIVATE",                         CURLOPT_PRIVATE },` |
|       - |  557 | `	{ "CURLOPT_PROGRESSFUNCTION",                CURLOPT_PROGRESSFUNCTION },` |
|       - |  558 | `	{ "CURLOPT_PROXY",                           CURLOPT_PROXY },` |
|       - |  559 | `	{ "CURLOPT_PROXYPORT",                       CURLOPT_PROXYPORT },` |
|       - |  560 | `	{ "CURLOPT_PROXYTYPE",                       CURLOPT_PROXYTYPE },` |
|       - |  561 | `	{ "CURLOPT_PROXYUSERPWD",                    CURLOPT_PROXYUSERPWD },` |
|       - |  562 | `	{ "CURLOPT_PUT",                             CURLOPT_PUT },` |
|       - |  563 | `	{ "CURLOPT_QUOTE",                           CURLOPT_QUOTE },` |
|       - |  564 | `	{ "CURLOPT_RANDOM_FILE",                     CURLOPT_RANDOM_FILE },` |
|       - |  565 | `	{ "CURLOPT_RANGE",                           CURLOPT_RANGE },` |
|       - |  566 | `	{ "CURLOPT_READDATA",                        CURLOPT_READDATA },` |
|       - |  567 | `	{ "CURLOPT_READFUNCTION",                    CURLOPT_READFUNCTION },` |
|       - |  568 | `	{ "CURLOPT_REFERER",                         CURLOPT_REFERER },` |
|       - |  569 | `	{ "CURLOPT_RESUME_FROM",                     CURLOPT_RESUME_FROM },` |
|       - |  570 | `	{ "CURLOPT_RETURNTRANSFER",                  19913 },` |
|       - |  571 | `	{ "CURLOPT_SHARE",                           CURLOPT_SHARE },` |
|       - |  572 | `	{ "CURLOPT_SSLCERT",                         CURLOPT_SSLCERT },` |
|       - |  573 | `	{ "CURLOPT_SSLCERTPASSWD",                   CURLOPT_SSLCERTPASSWD },` |
|       - |  574 | `	{ "CURLOPT_SSLCERTTYPE",                     CURLOPT_SSLCERTTYPE },` |
|       - |  575 | `	{ "CURLOPT_SSLENGINE",                       CURLOPT_SSLENGINE },` |
|       - |  576 | `	{ "CURLOPT_SSLENGINE_DEFAULT",               CURLOPT_SSLENGINE_DEFAULT },` |
|       - |  577 | `	{ "CURLOPT_SSLKEY",                          CURLOPT_SSLKEY },` |
|       - |  578 | `	{ "CURLOPT_SSLKEYPASSWD",                    CURLOPT_SSLKEYPASSWD },` |
|       - |  579 | `	{ "CURLOPT_SSLKEYTYPE",                      CURLOPT_SSLKEYTYPE },` |
|       - |  580 | `	{ "CURLOPT_SSLVERSION",                      CURLOPT_SSLVERSION },` |
|       - |  581 | `	{ "CURLOPT_SSL_CIPHER_LIST",                 CURLOPT_SSL_CIPHER_LIST },` |
|       - |  582 | `	{ "CURLOPT_SSL_VERIFYHOST",                  CURLOPT_SSL_VERIFYHOST },` |
|       - |  583 | `	{ "CURLOPT_SSL_VERIFYPEER",                  CURLOPT_SSL_VERIFYPEER },` |
|       - |  584 | `	{ "CURLOPT_STDERR",                          CURLOPT_STDERR },` |
|       - |  585 | `	{ "CURLOPT_TELNETOPTIONS",                   CURLOPT_TELNETOPTIONS },` |
|       - |  586 | `	{ "CURLOPT_TIMECONDITION",                   CURLOPT_TIMECONDITION },` |
|       - |  587 | `	{ "CURLOPT_TIMEOUT",                         CURLOPT_TIMEOUT },` |
|       - |  588 | `	{ "CURLOPT_TIMEVALUE",                       CURLOPT_TIMEVALUE },` |
|       - |  589 | `	{ "CURLOPT_TRANSFERTEXT",                    CURLOPT_TRANSFERTEXT },` |
|       - |  590 | `	{ "CURLOPT_UNRESTRICTED_AUTH",               CURLOPT_UNRESTRICTED_AUTH },` |
|       - |  591 | `	{ "CURLOPT_UPLOAD",                          CURLOPT_UPLOAD },` |
|       - |  592 | `	{ "CURLOPT_URL",                             CURLOPT_URL },` |
|       - |  593 | `	{ "CURLOPT_USERAGENT",                       CURLOPT_USERAGENT },` |
|       - |  594 | `	{ "CURLOPT_USERPWD",                         CURLOPT_USERPWD },` |
|       - |  595 | `	{ "CURLOPT_VERBOSE",                         CURLOPT_VERBOSE },` |
|       - |  596 | `	{ "CURLOPT_WRITEFUNCTION",                   CURLOPT_WRITEFUNCTION },` |
|       - |  597 | `	{ "CURLOPT_WRITEHEADER",                     CURLOPT_WRITEHEADER },` |
|       - |  598 | `	{ "CURLOPT_XFERINFOFUNCTION",                CURLOPT_XFERINFOFUNCTION },` |
|       - |  599 | `	{ "CURLOPT_DEBUGFUNCTION",                   CURLOPT_DEBUGFUNCTION },` |
|       - |  600 | `	{ "CURLINFO_TEXT",                           CURLINFO_TEXT },` |
|       - |  601 | `	{ "CURLINFO_HEADER_IN",                      CURLINFO_HEADER_IN },` |
|       - |  602 | `	{ "CURLINFO_DATA_IN",                        CURLINFO_DATA_IN },` |
|       - |  603 | `	{ "CURLINFO_DATA_OUT",                       CURLINFO_DATA_OUT },` |
|       - |  604 | `	{ "CURLINFO_SSL_DATA_OUT",                   CURLINFO_SSL_DATA_OUT },` |
|       - |  605 | `	{ "CURLINFO_SSL_DATA_IN",                    CURLINFO_SSL_DATA_IN },` |
|       - |  606 | `	{ "CURLE_ABORTED_BY_CALLBACK",               CURLE_ABORTED_BY_CALLBACK },` |
|       - |  607 | `	{ "CURLE_BAD_CALLING_ORDER",                 CURLE_BAD_CALLING_ORDER },` |
|       - |  608 | `	{ "CURLE_BAD_CONTENT_ENCODING",              CURLE_BAD_CONTENT_ENCODING },` |
|       - |  609 | `	{ "CURLE_BAD_DOWNLOAD_RESUME",               CURLE_BAD_DOWNLOAD_RESUME },` |
|       - |  610 | `	{ "CURLE_BAD_FUNCTION_ARGUMENT",             CURLE_BAD_FUNCTION_ARGUMENT },` |
|       - |  611 | `	{ "CURLE_BAD_PASSWORD_ENTERED",              CURLE_BAD_PASSWORD_ENTERED },` |
|       - |  612 | `	{ "CURLE_COULDNT_CONNECT",                   CURLE_COULDNT_CONNECT },` |
|       - |  613 | `	{ "CURLE_COULDNT_RESOLVE_HOST",              CURLE_COULDNT_RESOLVE_HOST },` |
|       - |  614 | `	{ "CURLE_COULDNT_RESOLVE_PROXY",             CURLE_COULDNT_RESOLVE_PROXY },` |
|       - |  615 | `	{ "CURLE_FAILED_INIT",                       CURLE_FAILED_INIT },` |
|       - |  616 | `	{ "CURLE_FILE_COULDNT_READ_FILE",            CURLE_FILE_COULDNT_READ_FILE },` |
|       - |  617 | `	{ "CURLE_FTP_ACCESS_DENIED",                 CURLE_FTP_ACCESS_DENIED },` |
|       - |  618 | `	{ "CURLE_FTP_BAD_DOWNLOAD_RESUME",           CURLE_FTP_BAD_DOWNLOAD_RESUME },` |
|       - |  619 | `	{ "CURLE_FTP_CANT_GET_HOST",                 CURLE_FTP_CANT_GET_HOST },` |
|       - |  620 | `	{ "CURLE_FTP_CANT_RECONNECT",                CURLE_FTP_CANT_RECONNECT },` |
|       - |  621 | `	{ "CURLE_FTP_COULDNT_GET_SIZE",              CURLE_FTP_COULDNT_GET_SIZE },` |
|       - |  622 | `	{ "CURLE_FTP_COULDNT_RETR_FILE",             CURLE_FTP_COULDNT_RETR_FILE },` |
|       - |  623 | `	{ "CURLE_FTP_COULDNT_SET_ASCII",             CURLE_FTP_COULDNT_SET_ASCII },` |
|       - |  624 | `	{ "CURLE_FTP_COULDNT_SET_BINARY",            CURLE_FTP_COULDNT_SET_BINARY },` |
|       - |  625 | `	{ "CURLE_FTP_COULDNT_STOR_FILE",             CURLE_FTP_COULDNT_STOR_FILE },` |
|       - |  626 | `	{ "CURLE_FTP_COULDNT_USE_REST",              CURLE_FTP_COULDNT_USE_REST },` |
|       - |  627 | `	{ "CURLE_FTP_PARTIAL_FILE",                  CURLE_FTP_PARTIAL_FILE },` |
|       - |  628 | `	{ "CURLE_FTP_PORT_FAILED",                   CURLE_FTP_PORT_FAILED },` |
|       - |  629 | `	{ "CURLE_FTP_QUOTE_ERROR",                   CURLE_FTP_QUOTE_ERROR },` |
|       - |  630 | `	{ "CURLE_FTP_USER_PASSWORD_INCORRECT",       CURLE_FTP_USER_PASSWORD_INCORRECT },` |
|       - |  631 | `	{ "CURLE_FTP_WEIRD_227_FORMAT",              CURLE_FTP_WEIRD_227_FORMAT },` |
|       - |  632 | `	{ "CURLE_FTP_WEIRD_PASS_REPLY",              CURLE_FTP_WEIRD_PASS_REPLY },` |
|       - |  633 | `	{ "CURLE_FTP_WEIRD_PASV_REPLY",              CURLE_FTP_WEIRD_PASV_REPLY },` |
|       - |  634 | `	{ "CURLE_FTP_WEIRD_SERVER_REPLY",            CURLE_FTP_WEIRD_SERVER_REPLY },` |
|       - |  635 | `	{ "CURLE_FTP_WEIRD_USER_REPLY",              CURLE_FTP_WEIRD_USER_REPLY },` |
|       - |  636 | `	{ "CURLE_FTP_WRITE_ERROR",                   CURLE_FTP_WRITE_ERROR },` |
|       - |  637 | `	{ "CURLE_FUNCTION_NOT_FOUND",                CURLE_FUNCTION_NOT_FOUND },` |
|       - |  638 | `	{ "CURLE_GOT_NOTHING",                       CURLE_GOT_NOTHING },` |
|       - |  639 | `	{ "CURLE_HTTP_NOT_FOUND",                    CURLE_HTTP_NOT_FOUND },` |
|       - |  640 | `	{ "CURLE_HTTP_PORT_FAILED",                  CURLE_HTTP_PORT_FAILED },` |
|       - |  641 | `	{ "CURLE_HTTP_POST_ERROR",                   CURLE_HTTP_POST_ERROR },` |
|       - |  642 | `	{ "CURLE_HTTP_RANGE_ERROR",                  CURLE_HTTP_RANGE_ERROR },` |
|       - |  643 | `	{ "CURLE_HTTP_RETURNED_ERROR",               CURLE_HTTP_RETURNED_ERROR },` |
|       - |  644 | `	{ "CURLE_LDAP_CANNOT_BIND",                  CURLE_LDAP_CANNOT_BIND },` |
|       - |  645 | `	{ "CURLE_LDAP_SEARCH_FAILED",                CURLE_LDAP_SEARCH_FAILED },` |
|       - |  646 | `	{ "CURLE_LIBRARY_NOT_FOUND",                 CURLE_LIBRARY_NOT_FOUND },` |
|       - |  647 | `	{ "CURLE_MALFORMAT_USER",                    CURLE_MALFORMAT_USER },` |
|       - |  648 | `	{ "CURLE_OBSOLETE",                          CURLE_OBSOLETE },` |
|       - |  649 | `	{ "CURLE_OK",                                CURLE_OK },` |
|       - |  650 | `	{ "CURLE_OPERATION_TIMEDOUT",                CURLE_OPERATION_TIMEDOUT },` |
|       - |  651 | `	{ "CURLE_OPERATION_TIMEOUTED",               CURLE_OPERATION_TIMEOUTED },` |
|       - |  652 | `	{ "CURLE_OUT_OF_MEMORY",                     CURLE_OUT_OF_MEMORY },` |
|       - |  653 | `	{ "CURLE_PARTIAL_FILE",                      CURLE_PARTIAL_FILE },` |
|       - |  654 | `	{ "CURLE_READ_ERROR",                        CURLE_READ_ERROR },` |
|       - |  655 | `	{ "CURLE_RECV_ERROR",                        CURLE_RECV_ERROR },` |
|       - |  656 | `	{ "CURLE_SEND_ERROR",                        CURLE_SEND_ERROR },` |
|       - |  657 | `	{ "CURLE_SHARE_IN_USE",                      CURLE_SHARE_IN_USE },` |
|       - |  658 | `	{ "CURLE_SSL_CACERT",                        CURLE_SSL_CACERT },` |
|       - |  659 | `	{ "CURLE_SSL_CERTPROBLEM",                   CURLE_SSL_CERTPROBLEM },` |
|       - |  660 | `	{ "CURLE_SSL_CIPHER",                        CURLE_SSL_CIPHER },` |
|       - |  661 | `	{ "CURLE_SSL_CONNECT_ERROR",                 CURLE_SSL_CONNECT_ERROR },` |
|       - |  662 | `	{ "CURLE_SSL_ENGINE_NOTFOUND",               CURLE_SSL_ENGINE_NOTFOUND },` |
|       - |  663 | `	{ "CURLE_SSL_ENGINE_SETFAILED",              CURLE_SSL_ENGINE_SETFAILED },` |
|       - |  664 | `	{ "CURLE_SSL_PEER_CERTIFICATE",              CURLE_SSL_PEER_CERTIFICATE },` |
|       - |  665 | `	{ "CURLE_SSL_PINNEDPUBKEYNOTMATCH",          CURLE_SSL_PINNEDPUBKEYNOTMATCH },` |
|       - |  666 | `	{ "CURLE_TELNET_OPTION_SYNTAX",              CURLE_TELNET_OPTION_SYNTAX },` |
|       - |  667 | `	{ "CURLE_TOO_MANY_REDIRECTS",                CURLE_TOO_MANY_REDIRECTS },` |
|       - |  668 | `	{ "CURLE_UNKNOWN_TELNET_OPTION",             CURLE_UNKNOWN_TELNET_OPTION },` |
|       - |  669 | `	{ "CURLE_UNSUPPORTED_PROTOCOL",              CURLE_UNSUPPORTED_PROTOCOL },` |
|       - |  670 | `	{ "CURLE_URL_MALFORMAT",                     CURLE_URL_MALFORMAT },` |
|       - |  671 | `	{ "CURLE_URL_MALFORMAT_USER",                CURLE_URL_MALFORMAT_USER },` |
|       - |  672 | `	{ "CURLE_WRITE_ERROR",                       CURLE_WRITE_ERROR },` |
|       - |  673 | `	{ "CURLINFO_CONNECT_TIME",                   CURLINFO_CONNECT_TIME },` |
|       - |  674 | `	{ "CURLINFO_CONTENT_LENGTH_DOWNLOAD",        CURLINFO_CONTENT_LENGTH_DOWNLOAD },` |
|       - |  675 | `	{ "CURLINFO_CONTENT_LENGTH_UPLOAD",          CURLINFO_CONTENT_LENGTH_UPLOAD },` |
|       - |  676 | `	{ "CURLINFO_CONTENT_TYPE",                   CURLINFO_CONTENT_TYPE },` |
|       - |  677 | `	{ "CURLINFO_EFFECTIVE_URL",                  CURLINFO_EFFECTIVE_URL },` |
|       - |  678 | `	{ "CURLINFO_FILETIME",                       CURLINFO_FILETIME },` |
|       - |  679 | `	{ "CURLINFO_HEADER_OUT",                     CURLINFO_HEADER_OUT },` |
|       - |  680 | `	{ "CURLINFO_HEADER_SIZE",                    CURLINFO_HEADER_SIZE },` |
|       - |  681 | `	{ "CURLINFO_HTTP_CODE",                      CURLINFO_HTTP_CODE },` |
|       - |  682 | `	{ "CURLINFO_LASTONE",                        CURLINFO_LASTONE },` |
|       - |  683 | `	{ "CURLINFO_NAMELOOKUP_TIME",                CURLINFO_NAMELOOKUP_TIME },` |
|       - |  684 | `	{ "CURLINFO_PRETRANSFER_TIME",               CURLINFO_PRETRANSFER_TIME },` |
|       - |  685 | `	{ "CURLINFO_PRIVATE",                        CURLINFO_PRIVATE },` |
|       - |  686 | `	{ "CURLINFO_REDIRECT_COUNT",                 CURLINFO_REDIRECT_COUNT },` |
|       - |  687 | `	{ "CURLINFO_REDIRECT_TIME",                  CURLINFO_REDIRECT_TIME },` |
|       - |  688 | `	{ "CURLINFO_REQUEST_SIZE",                   CURLINFO_REQUEST_SIZE },` |
|       - |  689 | `	{ "CURLINFO_SIZE_DOWNLOAD",                  CURLINFO_SIZE_DOWNLOAD },` |
|       - |  690 | `	{ "CURLINFO_SIZE_UPLOAD",                    CURLINFO_SIZE_UPLOAD },` |
|       - |  691 | `	{ "CURLINFO_SPEED_DOWNLOAD",                 CURLINFO_SPEED_DOWNLOAD },` |
|       - |  692 | `	{ "CURLINFO_SPEED_UPLOAD",                   CURLINFO_SPEED_UPLOAD },` |
|       - |  693 | `	{ "CURLINFO_SSL_VERIFYRESULT",               CURLINFO_SSL_VERIFYRESULT },` |
|       - |  694 | `	{ "CURLINFO_STARTTRANSFER_TIME",             CURLINFO_STARTTRANSFER_TIME },` |
|       - |  695 | `	{ "CURLINFO_TOTAL_TIME",                     CURLINFO_TOTAL_TIME },` |
|       - |  696 | `	{ "CURLINFO_EFFECTIVE_METHOD",               CURLINFO_EFFECTIVE_METHOD },` |
|       - |  697 | `	{ "CURLINFO_CAPATH",                         CURLINFO_CAPATH },` |
|       - |  698 | `	{ "CURLINFO_CAINFO",                         CURLINFO_CAINFO },` |
|       - |  699 | `	{ "CURLMSG_DONE",                            CURLMSG_DONE },` |
|       - |  700 | `	{ "CURLVERSION_NOW",                         CURLVERSION_NOW },` |
|       - |  701 | `	{ "CURLM_BAD_EASY_HANDLE",                   CURLM_BAD_EASY_HANDLE },` |
|       - |  702 | `	{ "CURLM_BAD_HANDLE",                        CURLM_BAD_HANDLE },` |
|       - |  703 | `	{ "CURLM_CALL_MULTI_PERFORM",                CURLM_CALL_MULTI_PERFORM },` |
|       - |  704 | `	{ "CURLM_INTERNAL_ERROR",                    CURLM_INTERNAL_ERROR },` |
|       - |  705 | `	{ "CURLM_OK",                                CURLM_OK },` |
|       - |  706 | `	{ "CURLM_OUT_OF_MEMORY",                     CURLM_OUT_OF_MEMORY },` |
|       - |  707 | `	{ "CURLM_ADDED_ALREADY",                     CURLM_ADDED_ALREADY },` |
|       - |  708 | `	{ "CURLPROXY_HTTP",                          CURLPROXY_HTTP },` |
|       - |  709 | `	{ "CURLPROXY_SOCKS4",                        CURLPROXY_SOCKS4 },` |
|       - |  710 | `	{ "CURLPROXY_SOCKS5",                        CURLPROXY_SOCKS5 },` |
|       - |  711 | `	{ "CURLSHOPT_NONE",                          CURLSHOPT_NONE },` |
|       - |  712 | `	{ "CURLSHOPT_SHARE",                         CURLSHOPT_SHARE },` |
|       - |  713 | `	{ "CURLSHOPT_UNSHARE",                       CURLSHOPT_UNSHARE },` |
|       - |  714 | `	{ "CURL_HTTP_VERSION_1_0",                   CURL_HTTP_VERSION_1_0 },` |
|       - |  715 | `	{ "CURL_HTTP_VERSION_1_1",                   CURL_HTTP_VERSION_1_1 },` |
|       - |  716 | `	{ "CURL_HTTP_VERSION_NONE",                  CURL_HTTP_VERSION_NONE },` |
|       - |  717 | `	{ "CURL_LOCK_DATA_COOKIE",                   CURL_LOCK_DATA_COOKIE },` |
|       - |  718 | `	{ "CURL_LOCK_DATA_DNS",                      CURL_LOCK_DATA_DNS },` |
|       - |  719 | `	{ "CURL_LOCK_DATA_SSL_SESSION",              CURL_LOCK_DATA_SSL_SESSION },` |
|       - |  720 | `	{ "CURL_NETRC_IGNORED",                      CURL_NETRC_IGNORED },` |
|       - |  721 | `	{ "CURL_NETRC_OPTIONAL",                     CURL_NETRC_OPTIONAL },` |
|       - |  722 | `	{ "CURL_NETRC_REQUIRED",                     CURL_NETRC_REQUIRED },` |
|       - |  723 | `	{ "CURL_SSLVERSION_DEFAULT",                 CURL_SSLVERSION_DEFAULT },` |
|       - |  724 | `	{ "CURL_SSLVERSION_SSLv2",                   CURL_SSLVERSION_SSLv2 },` |
|       - |  725 | `	{ "CURL_SSLVERSION_SSLv3",                   CURL_SSLVERSION_SSLv3 },` |
|       - |  726 | `	{ "CURL_SSLVERSION_TLSv1",                   CURL_SSLVERSION_TLSv1 },` |
|       - |  727 | `	{ "CURL_TIMECOND_IFMODSINCE",                CURL_TIMECOND_IFMODSINCE },` |
|       - |  728 | `	{ "CURL_TIMECOND_IFUNMODSINCE",              CURL_TIMECOND_IFUNMODSINCE },` |
|       - |  729 | `	{ "CURL_TIMECOND_LASTMOD",                   CURL_TIMECOND_LASTMOD },` |
|       - |  730 | `	{ "CURL_TIMECOND_NONE",                      CURL_TIMECOND_NONE },` |
|       - |  731 | `	{ "CURL_VERSION_ASYNCHDNS",                  CURL_VERSION_ASYNCHDNS },` |
|       - |  732 | `	{ "CURL_VERSION_CONV",                       CURL_VERSION_CONV },` |
|       - |  733 | `	{ "CURL_VERSION_DEBUG",                      CURL_VERSION_DEBUG },` |
|       - |  734 | `	{ "CURL_VERSION_GSSNEGOTIATE",               CURL_VERSION_GSSNEGOTIATE },` |
|       - |  735 | `	{ "CURL_VERSION_IDN",                        CURL_VERSION_IDN },` |
|       - |  736 | `	{ "CURL_VERSION_IPV6",                       CURL_VERSION_IPV6 },` |
|       - |  737 | `	{ "CURL_VERSION_KERBEROS4",                  CURL_VERSION_KERBEROS4 },` |
|       - |  738 | `	{ "CURL_VERSION_LARGEFILE",                  CURL_VERSION_LARGEFILE },` |
|       - |  739 | `	{ "CURL_VERSION_LIBZ",                       CURL_VERSION_LIBZ },` |
|       - |  740 | `	{ "CURL_VERSION_NTLM",                       CURL_VERSION_NTLM },` |
|       - |  741 | `	{ "CURL_VERSION_SPNEGO",                     CURL_VERSION_SPNEGO },` |
|       - |  742 | `	{ "CURL_VERSION_SSL",                        CURL_VERSION_SSL },` |
|       - |  743 | `	{ "CURL_VERSION_SSPI",                       CURL_VERSION_SSPI },` |
|       - |  744 | `	{ "CURLOPT_HTTPAUTH",                        CURLOPT_HTTPAUTH },` |
|       - |  745 | `	{ "CURLAUTH_ANY",                            CURLAUTH_ANY },` |
|       - |  746 | `	{ "CURLAUTH_ANYSAFE",                        CURLAUTH_ANYSAFE },` |
|       - |  747 | `	{ "CURLAUTH_BASIC",                          CURLAUTH_BASIC },` |
|       - |  748 | `	{ "CURLAUTH_DIGEST",                         CURLAUTH_DIGEST },` |
|       - |  749 | `	{ "CURLAUTH_GSSNEGOTIATE",                   CURLAUTH_GSSNEGOTIATE },` |
|       - |  750 | `	{ "CURLAUTH_NONE",                           CURLAUTH_NONE },` |
|       - |  751 | `	{ "CURLAUTH_NTLM",                           CURLAUTH_NTLM },` |
|       - |  752 | `	{ "CURLINFO_HTTP_CONNECTCODE",               CURLINFO_HTTP_CONNECTCODE },` |
|       - |  753 | `	{ "CURLOPT_FTP_CREATE_MISSING_DIRS",         CURLOPT_FTP_CREATE_MISSING_DIRS },` |
|       - |  754 | `	{ "CURLOPT_PROXYAUTH",                       CURLOPT_PROXYAUTH },` |
|       - |  755 | `	{ "CURLE_FILESIZE_EXCEEDED",                 CURLE_FILESIZE_EXCEEDED },` |
|       - |  756 | `	{ "CURLE_LDAP_INVALID_URL",                  CURLE_LDAP_INVALID_URL },` |
|       - |  757 | `	{ "CURLINFO_HTTPAUTH_AVAIL",                 CURLINFO_HTTPAUTH_AVAIL },` |
|       - |  758 | `	{ "CURLINFO_RESPONSE_CODE",                  CURLINFO_RESPONSE_CODE },` |
|       - |  759 | `	{ "CURLINFO_PROXYAUTH_AVAIL",                CURLINFO_PROXYAUTH_AVAIL },` |
|       - |  760 | `	{ "CURLOPT_FTP_RESPONSE_TIMEOUT",            CURLOPT_FTP_RESPONSE_TIMEOUT },` |
|       - |  761 | `	{ "CURLOPT_SERVER_RESPONSE_TIMEOUT",         CURLOPT_SERVER_RESPONSE_TIMEOUT },` |
|       - |  762 | `	{ "CURLOPT_IPRESOLVE",                       CURLOPT_IPRESOLVE },` |
|       - |  763 | `	{ "CURLOPT_MAXFILESIZE",                     CURLOPT_MAXFILESIZE },` |
|       - |  764 | `	{ "CURL_IPRESOLVE_V4",                       CURL_IPRESOLVE_V4 },` |
|       - |  765 | `	{ "CURL_IPRESOLVE_V6",                       CURL_IPRESOLVE_V6 },` |
|       - |  766 | `	{ "CURL_IPRESOLVE_WHATEVER",                 CURL_IPRESOLVE_WHATEVER },` |
|       - |  767 | `	{ "CURLE_FTP_SSL_FAILED",                    CURLE_FTP_SSL_FAILED },` |
|       - |  768 | `	{ "CURLFTPSSL_ALL",                          CURLFTPSSL_ALL },` |
|       - |  769 | `	{ "CURLFTPSSL_CONTROL",                      CURLFTPSSL_CONTROL },` |
|       - |  770 | `	{ "CURLFTPSSL_NONE",                         CURLFTPSSL_NONE },` |
|       - |  771 | `	{ "CURLFTPSSL_TRY",                          CURLFTPSSL_TRY },` |
|       - |  772 | `	{ "CURLOPT_FTP_SSL",                         CURLOPT_FTP_SSL },` |
|       - |  773 | `	{ "CURLOPT_NETRC_FILE",                      CURLOPT_NETRC_FILE },` |
|       - |  774 | `	{ "CURLOPT_MAXFILESIZE_LARGE",               CURLOPT_MAXFILESIZE_LARGE },` |
|       - |  775 | `	{ "CURLOPT_TCP_NODELAY",                     CURLOPT_TCP_NODELAY },` |
|       - |  776 | `	{ "CURLFTPAUTH_DEFAULT",                     CURLFTPAUTH_DEFAULT },` |
|       - |  777 | `	{ "CURLFTPAUTH_SSL",                         CURLFTPAUTH_SSL },` |
|       - |  778 | `	{ "CURLFTPAUTH_TLS",                         CURLFTPAUTH_TLS },` |
|       - |  779 | `	{ "CURLOPT_FTPSSLAUTH",                      CURLOPT_FTPSSLAUTH },` |
|       - |  780 | `	{ "CURLOPT_FTP_ACCOUNT",                     CURLOPT_FTP_ACCOUNT },` |
|       - |  781 | `	{ "CURLINFO_OS_ERRNO",                       CURLINFO_OS_ERRNO },` |
|       - |  782 | `	{ "CURLINFO_NUM_CONNECTS",                   CURLINFO_NUM_CONNECTS },` |
|       - |  783 | `	{ "CURLINFO_SSL_ENGINES",                    CURLINFO_SSL_ENGINES },` |
|       - |  784 | `	{ "CURLINFO_COOKIELIST",                     CURLINFO_COOKIELIST },` |
|       - |  785 | `	{ "CURLOPT_COOKIELIST",                      CURLOPT_COOKIELIST },` |
|       - |  786 | `	{ "CURLOPT_IGNORE_CONTENT_LENGTH",           CURLOPT_IGNORE_CONTENT_LENGTH },` |
|       - |  787 | `	{ "CURLOPT_FTP_SKIP_PASV_IP",                CURLOPT_FTP_SKIP_PASV_IP },` |
|       - |  788 | `	{ "CURLOPT_FTP_FILEMETHOD",                  CURLOPT_FTP_FILEMETHOD },` |
|       - |  789 | `	{ "CURLOPT_CONNECT_ONLY",                    CURLOPT_CONNECT_ONLY },` |
|       - |  790 | `	{ "CURLOPT_LOCALPORT",                       CURLOPT_LOCALPORT },` |
|       - |  791 | `	{ "CURLOPT_LOCALPORTRANGE",                  CURLOPT_LOCALPORTRANGE },` |
|       - |  792 | `	{ "CURLFTPMETHOD_DEFAULT",                   CURLFTPMETHOD_DEFAULT },` |
|       - |  793 | `	{ "CURLFTPMETHOD_MULTICWD",                  CURLFTPMETHOD_MULTICWD },` |
|       - |  794 | `	{ "CURLFTPMETHOD_NOCWD",                     CURLFTPMETHOD_NOCWD },` |
|       - |  795 | `	{ "CURLFTPMETHOD_SINGLECWD",                 CURLFTPMETHOD_SINGLECWD },` |
|       - |  796 | `	{ "CURLINFO_FTP_ENTRY_PATH",                 CURLINFO_FTP_ENTRY_PATH },` |
|       - |  797 | `	{ "CURLOPT_FTP_ALTERNATIVE_TO_USER",         CURLOPT_FTP_ALTERNATIVE_TO_USER },` |
|       - |  798 | `	{ "CURLOPT_MAX_RECV_SPEED_LARGE",            CURLOPT_MAX_RECV_SPEED_LARGE },` |
|       - |  799 | `	{ "CURLOPT_MAX_SEND_SPEED_LARGE",            CURLOPT_MAX_SEND_SPEED_LARGE },` |
|       - |  800 | `	{ "CURLE_SSL_CACERT_BADFILE",                CURLE_SSL_CACERT_BADFILE },` |
|       - |  801 | `	{ "CURLOPT_SSL_SESSIONID_CACHE",             CURLOPT_SSL_SESSIONID_CACHE },` |
|       - |  802 | `	{ "CURLMOPT_PIPELINING",                     CURLMOPT_PIPELINING },` |
|       - |  803 | `	{ "CURLE_SSH",                               CURLE_SSH },` |
|       - |  804 | `	{ "CURLOPT_FTP_SSL_CCC",                     CURLOPT_FTP_SSL_CCC },` |
|       - |  805 | `	{ "CURLOPT_SSH_AUTH_TYPES",                  CURLOPT_SSH_AUTH_TYPES },` |
|       - |  806 | `	{ "CURLOPT_SSH_PRIVATE_KEYFILE",             CURLOPT_SSH_PRIVATE_KEYFILE },` |
|       - |  807 | `	{ "CURLOPT_SSH_PUBLIC_KEYFILE",              CURLOPT_SSH_PUBLIC_KEYFILE },` |
|       - |  808 | `	{ "CURLFTPSSL_CCC_ACTIVE",                   CURLFTPSSL_CCC_ACTIVE },` |
|       - |  809 | `	{ "CURLFTPSSL_CCC_NONE",                     CURLFTPSSL_CCC_NONE },` |
|       - |  810 | `	{ "CURLFTPSSL_CCC_PASSIVE",                  CURLFTPSSL_CCC_PASSIVE },` |
|       - |  811 | `	{ "CURLOPT_CONNECTTIMEOUT_MS",               CURLOPT_CONNECTTIMEOUT_MS },` |
|       - |  812 | `	{ "CURLOPT_HTTP_CONTENT_DECODING",           CURLOPT_HTTP_CONTENT_DECODING },` |
|       - |  813 | `	{ "CURLOPT_HTTP_TRANSFER_DECODING",          CURLOPT_HTTP_TRANSFER_DECODING },` |
|       - |  814 | `	{ "CURLOPT_TIMEOUT_MS",                      CURLOPT_TIMEOUT_MS },` |
|       - |  815 | `	{ "CURLMOPT_MAXCONNECTS",                    CURLMOPT_MAXCONNECTS },` |
|       - |  816 | `	{ "CURLOPT_KRBLEVEL",                        CURLOPT_KRBLEVEL },` |
|       - |  817 | `	{ "CURLOPT_NEW_DIRECTORY_PERMS",             CURLOPT_NEW_DIRECTORY_PERMS },` |
|       - |  818 | `	{ "CURLOPT_NEW_FILE_PERMS",                  CURLOPT_NEW_FILE_PERMS },` |
|       - |  819 | `	{ "CURLOPT_APPEND",                          CURLOPT_APPEND },` |
|       - |  820 | `	{ "CURLOPT_DIRLISTONLY",                     CURLOPT_DIRLISTONLY },` |
|       - |  821 | `	{ "CURLOPT_USE_SSL",                         CURLOPT_USE_SSL },` |
|       - |  822 | `	{ "CURLUSESSL_ALL",                          CURLUSESSL_ALL },` |
|       - |  823 | `	{ "CURLUSESSL_CONTROL",                      CURLUSESSL_CONTROL },` |
|       - |  824 | `	{ "CURLUSESSL_NONE",                         CURLUSESSL_NONE },` |
|       - |  825 | `	{ "CURLUSESSL_TRY",                          CURLUSESSL_TRY },` |
|       - |  826 | `	{ "CURLOPT_SSH_HOST_PUBLIC_KEY_MD5",         CURLOPT_SSH_HOST_PUBLIC_KEY_MD5 },` |
|       - |  827 | `	{ "CURLOPT_PROXY_TRANSFER_MODE",             CURLOPT_PROXY_TRANSFER_MODE },` |
|       - |  828 | `	{ "CURLPAUSE_ALL",                           CURLPAUSE_ALL },` |
|       - |  829 | `	{ "CURLPAUSE_CONT",                          CURLPAUSE_CONT },` |
|       - |  830 | `	{ "CURLPAUSE_RECV",                          CURLPAUSE_RECV },` |
|       - |  831 | `	{ "CURLPAUSE_RECV_CONT",                     CURLPAUSE_RECV_CONT },` |
|       - |  832 | `	{ "CURLPAUSE_SEND",                          CURLPAUSE_SEND },` |
|       - |  833 | `	{ "CURLPAUSE_SEND_CONT",                     CURLPAUSE_SEND_CONT },` |
|       - |  834 | `	{ "CURL_READFUNC_PAUSE",                     CURL_READFUNC_PAUSE },` |
|       - |  835 | `	{ "CURL_WRITEFUNC_PAUSE",                    CURL_WRITEFUNC_PAUSE },` |
|       - |  836 | `	{ "CURLPROXY_SOCKS4A",                       CURLPROXY_SOCKS4A },` |
|       - |  837 | `	{ "CURLPROXY_SOCKS5_HOSTNAME",               CURLPROXY_SOCKS5_HOSTNAME },` |
|       - |  838 | `	{ "CURLINFO_REDIRECT_URL",                   CURLINFO_REDIRECT_URL },` |
|       - |  839 | `	{ "CURLINFO_APPCONNECT_TIME",                CURLINFO_APPCONNECT_TIME },` |
|       - |  840 | `	{ "CURLINFO_PRIMARY_IP",                     CURLINFO_PRIMARY_IP },` |
|       - |  841 | `	{ "CURLOPT_ADDRESS_SCOPE",                   CURLOPT_ADDRESS_SCOPE },` |
|       - |  842 | `	{ "CURLOPT_CRLFILE",                         CURLOPT_CRLFILE },` |
|       - |  843 | `	{ "CURLOPT_ISSUERCERT",                      CURLOPT_ISSUERCERT },` |
|       - |  844 | `	{ "CURLOPT_KEYPASSWD",                       CURLOPT_KEYPASSWD },` |
|       - |  845 | `	{ "CURLSSH_AUTH_ANY",                        CURLSSH_AUTH_ANY },` |
|       - |  846 | `	{ "CURLSSH_AUTH_DEFAULT",                    CURLSSH_AUTH_DEFAULT },` |
|       - |  847 | `	{ "CURLSSH_AUTH_HOST",                       CURLSSH_AUTH_HOST },` |
|       - |  848 | `	{ "CURLSSH_AUTH_KEYBOARD",                   CURLSSH_AUTH_KEYBOARD },` |
|       - |  849 | `	{ "CURLSSH_AUTH_NONE",                       CURLSSH_AUTH_NONE },` |
|       - |  850 | `	{ "CURLSSH_AUTH_PASSWORD",                   CURLSSH_AUTH_PASSWORD },` |
|       - |  851 | `	{ "CURLSSH_AUTH_PUBLICKEY",                  CURLSSH_AUTH_PUBLICKEY },` |
|       - |  852 | `	{ "CURLINFO_CERTINFO",                       CURLINFO_CERTINFO },` |
|       - |  853 | `	{ "CURLOPT_CERTINFO",                        CURLOPT_CERTINFO },` |
|       - |  854 | `	{ "CURLOPT_PASSWORD",                        CURLOPT_PASSWORD },` |
|       - |  855 | `	{ "CURLOPT_POSTREDIR",                       CURLOPT_POSTREDIR },` |
|       - |  856 | `	{ "CURLOPT_PROXYPASSWORD",                   CURLOPT_PROXYPASSWORD },` |
|       - |  857 | `	{ "CURLOPT_PROXYUSERNAME",                   CURLOPT_PROXYUSERNAME },` |
|       - |  858 | `	{ "CURLOPT_USERNAME",                        CURLOPT_USERNAME },` |
|       - |  859 | `	{ "CURL_REDIR_POST_301",                     CURL_REDIR_POST_301 },` |
|       - |  860 | `	{ "CURL_REDIR_POST_302",                     CURL_REDIR_POST_302 },` |
|       - |  861 | `	{ "CURL_REDIR_POST_ALL",                     CURL_REDIR_POST_ALL },` |
|       - |  862 | `	{ "CURLAUTH_DIGEST_IE",                      CURLAUTH_DIGEST_IE },` |
|       - |  863 | `	{ "CURLINFO_CONDITION_UNMET",                CURLINFO_CONDITION_UNMET },` |
|       - |  864 | `	{ "CURLOPT_NOPROXY",                         CURLOPT_NOPROXY },` |
|       - |  865 | `	{ "CURLOPT_PROTOCOLS",                       CURLOPT_PROTOCOLS },` |
|       - |  866 | `	{ "CURLOPT_REDIR_PROTOCOLS",                 CURLOPT_REDIR_PROTOCOLS },` |
|       - |  867 | `	{ "CURLOPT_SOCKS5_GSSAPI_NEC",               CURLOPT_SOCKS5_GSSAPI_NEC },` |
|       - |  868 | `	{ "CURLOPT_SOCKS5_GSSAPI_SERVICE",           CURLOPT_SOCKS5_GSSAPI_SERVICE },` |
|       - |  869 | `	{ "CURLOPT_TFTP_BLKSIZE",                    CURLOPT_TFTP_BLKSIZE },` |
|       - |  870 | `	{ "CURLPROTO_ALL",                           CURLPROTO_ALL },` |
|       - |  871 | `	{ "CURLPROTO_DICT",                          CURLPROTO_DICT },` |
|       - |  872 | `	{ "CURLPROTO_FILE",                          CURLPROTO_FILE },` |
|       - |  873 | `	{ "CURLPROTO_FTP",                           CURLPROTO_FTP },` |
|       - |  874 | `	{ "CURLPROTO_FTPS",                          CURLPROTO_FTPS },` |
|       - |  875 | `	{ "CURLPROTO_HTTP",                          CURLPROTO_HTTP },` |
|       - |  876 | `	{ "CURLPROTO_HTTPS",                         CURLPROTO_HTTPS },` |
|       - |  877 | `	{ "CURLPROTO_LDAP",                          CURLPROTO_LDAP },` |
|       - |  878 | `	{ "CURLPROTO_LDAPS",                         CURLPROTO_LDAPS },` |
|       - |  879 | `	{ "CURLPROTO_SCP",                           CURLPROTO_SCP },` |
|       - |  880 | `	{ "CURLPROTO_SFTP",                          CURLPROTO_SFTP },` |
|       - |  881 | `	{ "CURLPROTO_TELNET",                        CURLPROTO_TELNET },` |
|       - |  882 | `	{ "CURLPROTO_TFTP",                          CURLPROTO_TFTP },` |
|       - |  883 | `	{ "CURLPROXY_HTTP_1_0",                      CURLPROXY_HTTP_1_0 },` |
|       - |  884 | `	{ "CURLFTP_CREATE_DIR",                      CURLFTP_CREATE_DIR },` |
|       - |  885 | `	{ "CURLFTP_CREATE_DIR_NONE",                 CURLFTP_CREATE_DIR_NONE },` |
|       - |  886 | `	{ "CURLFTP_CREATE_DIR_RETRY",                CURLFTP_CREATE_DIR_RETRY },` |
|       - |  887 | `	{ "CURL_VERSION_CURLDEBUG",                  CURL_VERSION_CURLDEBUG },` |
|       - |  888 | `	{ "CURLOPT_SSH_KNOWNHOSTS",                  CURLOPT_SSH_KNOWNHOSTS },` |
|       - |  889 | `	{ "CURLKHMATCH_OK",                          CURLKHMATCH_OK },` |
|       - |  890 | `	{ "CURLKHMATCH_MISMATCH",                    CURLKHMATCH_MISMATCH },` |
|       - |  891 | `	{ "CURLKHMATCH_MISSING",                     CURLKHMATCH_MISSING },` |
|       - |  892 | `	{ "CURLKHMATCH_LAST",                        CURLKHMATCH_LAST },` |
|       - |  893 | `	{ "CURLINFO_RTSP_CLIENT_CSEQ",               CURLINFO_RTSP_CLIENT_CSEQ },` |
|       - |  894 | `	{ "CURLINFO_RTSP_CSEQ_RECV",                 CURLINFO_RTSP_CSEQ_RECV },` |
|       - |  895 | `	{ "CURLINFO_RTSP_SERVER_CSEQ",               CURLINFO_RTSP_SERVER_CSEQ },` |
|       - |  896 | `	{ "CURLINFO_RTSP_SESSION_ID",                CURLINFO_RTSP_SESSION_ID },` |
|       - |  897 | `	{ "CURLOPT_FTP_USE_PRET",                    CURLOPT_FTP_USE_PRET },` |
|       - |  898 | `	{ "CURLOPT_MAIL_FROM",                       CURLOPT_MAIL_FROM },` |
|       - |  899 | `	{ "CURLOPT_MAIL_RCPT",                       CURLOPT_MAIL_RCPT },` |
|       - |  900 | `	{ "CURLOPT_RTSP_CLIENT_CSEQ",                CURLOPT_RTSP_CLIENT_CSEQ },` |
|       - |  901 | `	{ "CURLOPT_RTSP_REQUEST",                    CURLOPT_RTSP_REQUEST },` |
|       - |  902 | `	{ "CURLOPT_RTSP_SERVER_CSEQ",                CURLOPT_RTSP_SERVER_CSEQ },` |
|       - |  903 | `	{ "CURLOPT_RTSP_SESSION_ID",                 CURLOPT_RTSP_SESSION_ID },` |
|       - |  904 | `	{ "CURLOPT_RTSP_STREAM_URI",                 CURLOPT_RTSP_STREAM_URI },` |
|       - |  905 | `	{ "CURLOPT_RTSP_TRANSPORT",                  CURLOPT_RTSP_TRANSPORT },` |
|       - |  906 | `	{ "CURLPROTO_IMAP",                          CURLPROTO_IMAP },` |
|       - |  907 | `	{ "CURLPROTO_IMAPS",                         CURLPROTO_IMAPS },` |
|       - |  908 | `	{ "CURLPROTO_POP3",                          CURLPROTO_POP3 },` |
|       - |  909 | `	{ "CURLPROTO_POP3S",                         CURLPROTO_POP3S },` |
|       - |  910 | `	{ "CURLPROTO_RTSP",                          CURLPROTO_RTSP },` |
|       - |  911 | `	{ "CURLPROTO_SMTP",                          CURLPROTO_SMTP },` |
|       - |  912 | `	{ "CURLPROTO_SMTPS",                         CURLPROTO_SMTPS },` |
|       - |  913 | `	{ "CURL_RTSPREQ_ANNOUNCE",                   CURL_RTSPREQ_ANNOUNCE },` |
|       - |  914 | `	{ "CURL_RTSPREQ_DESCRIBE",                   CURL_RTSPREQ_DESCRIBE },` |
|       - |  915 | `	{ "CURL_RTSPREQ_GET_PARAMETER",              CURL_RTSPREQ_GET_PARAMETER },` |
|       - |  916 | `	{ "CURL_RTSPREQ_OPTIONS",                    CURL_RTSPREQ_OPTIONS },` |
|       - |  917 | `	{ "CURL_RTSPREQ_PAUSE",                      CURL_RTSPREQ_PAUSE },` |
|       - |  918 | `	{ "CURL_RTSPREQ_PLAY",                       CURL_RTSPREQ_PLAY },` |
|       - |  919 | `	{ "CURL_RTSPREQ_RECEIVE",                    CURL_RTSPREQ_RECEIVE },` |
|       - |  920 | `	{ "CURL_RTSPREQ_RECORD",                     CURL_RTSPREQ_RECORD },` |
|       - |  921 | `	{ "CURL_RTSPREQ_SET_PARAMETER",              CURL_RTSPREQ_SET_PARAMETER },` |
|       - |  922 | `	{ "CURL_RTSPREQ_SETUP",                      CURL_RTSPREQ_SETUP },` |
|       - |  923 | `	{ "CURL_RTSPREQ_TEARDOWN",                   CURL_RTSPREQ_TEARDOWN },` |
|       - |  924 | `	{ "CURLINFO_LOCAL_IP",                       CURLINFO_LOCAL_IP },` |
|       - |  925 | `	{ "CURLINFO_LOCAL_PORT",                     CURLINFO_LOCAL_PORT },` |
|       - |  926 | `	{ "CURLINFO_PRIMARY_PORT",                   CURLINFO_PRIMARY_PORT },` |
|       - |  927 | `	{ "CURLOPT_FNMATCH_FUNCTION",                CURLOPT_FNMATCH_FUNCTION },` |
|       - |  928 | `	{ "CURLOPT_WILDCARDMATCH",                   CURLOPT_WILDCARDMATCH },` |
|       - |  929 | `	{ "CURLPROTO_RTMP",                          CURLPROTO_RTMP },` |
|       - |  930 | `	{ "CURLPROTO_RTMPE",                         CURLPROTO_RTMPE },` |
|       - |  931 | `	{ "CURLPROTO_RTMPS",                         CURLPROTO_RTMPS },` |
|       - |  932 | `	{ "CURLPROTO_RTMPT",                         CURLPROTO_RTMPT },` |
|       - |  933 | `	{ "CURLPROTO_RTMPTE",                        CURLPROTO_RTMPTE },` |
|       - |  934 | `	{ "CURLPROTO_RTMPTS",                        CURLPROTO_RTMPTS },` |
|       - |  935 | `	{ "CURL_FNMATCHFUNC_FAIL",                   CURL_FNMATCHFUNC_FAIL },` |
|       - |  936 | `	{ "CURL_FNMATCHFUNC_MATCH",                  CURL_FNMATCHFUNC_MATCH },` |
|       - |  937 | `	{ "CURL_FNMATCHFUNC_NOMATCH",                CURL_FNMATCHFUNC_NOMATCH },` |
|       - |  938 | `	{ "CURLPROTO_GOPHER",                        CURLPROTO_GOPHER },` |
|       - |  939 | `	{ "CURLAUTH_ONLY",                           CURLAUTH_ONLY },` |
|       - |  940 | `	{ "CURLOPT_RESOLVE",                         CURLOPT_RESOLVE },` |
|       - |  941 | `	{ "CURLOPT_TLSAUTH_PASSWORD",                CURLOPT_TLSAUTH_PASSWORD },` |
|       - |  942 | `	{ "CURLOPT_TLSAUTH_TYPE",                    CURLOPT_TLSAUTH_TYPE },` |
|       - |  943 | `	{ "CURLOPT_TLSAUTH_USERNAME",                CURLOPT_TLSAUTH_USERNAME },` |
|       - |  944 | `	{ "CURL_TLSAUTH_SRP",                        CURL_TLSAUTH_SRP },` |
|       - |  945 | `	{ "CURL_VERSION_TLSAUTH_SRP",                CURL_VERSION_TLSAUTH_SRP },` |
|       - |  946 | `	{ "CURLOPT_ACCEPT_ENCODING",                 CURLOPT_ACCEPT_ENCODING },` |
|       - |  947 | `	{ "CURLOPT_TRANSFER_ENCODING",               CURLOPT_TRANSFER_ENCODING },` |
|       - |  948 | `	{ "CURLAUTH_NTLM_WB",                        CURLAUTH_NTLM_WB },` |
|       - |  949 | `	{ "CURLGSSAPI_DELEGATION_FLAG",              CURLGSSAPI_DELEGATION_FLAG },` |
|       - |  950 | `	{ "CURLGSSAPI_DELEGATION_POLICY_FLAG",       CURLGSSAPI_DELEGATION_POLICY_FLAG },` |
|       - |  951 | `	{ "CURLOPT_GSSAPI_DELEGATION",               CURLOPT_GSSAPI_DELEGATION },` |
|       - |  952 | `	{ "CURL_VERSION_NTLM_WB",                    CURL_VERSION_NTLM_WB },` |
|       - |  953 | `	{ "CURLOPT_ACCEPTTIMEOUT_MS",                CURLOPT_ACCEPTTIMEOUT_MS },` |
|       - |  954 | `	{ "CURLOPT_DNS_SERVERS",                     CURLOPT_DNS_SERVERS },` |
|       - |  955 | `	{ "CURLOPT_MAIL_AUTH",                       CURLOPT_MAIL_AUTH },` |
|       - |  956 | `	{ "CURLOPT_SSL_OPTIONS",                     CURLOPT_SSL_OPTIONS },` |
|       - |  957 | `	{ "CURLOPT_TCP_KEEPALIVE",                   CURLOPT_TCP_KEEPALIVE },` |
|       - |  958 | `	{ "CURLOPT_TCP_KEEPIDLE",                    CURLOPT_TCP_KEEPIDLE },` |
|       - |  959 | `	{ "CURLOPT_TCP_KEEPINTVL",                   CURLOPT_TCP_KEEPINTVL },` |
|       - |  960 | `	{ "CURLSSLOPT_ALLOW_BEAST",                  CURLSSLOPT_ALLOW_BEAST },` |
|       - |  961 | `	{ "CURL_REDIR_POST_303",                     CURL_REDIR_POST_303 },` |
|       - |  962 | `	{ "CURLSSH_AUTH_AGENT",                      CURLSSH_AUTH_AGENT },` |
|       - |  963 | `	{ "CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE",      CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE },` |
|       - |  964 | `	{ "CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE",    CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE },` |
|       - |  965 | `	{ "CURLMOPT_MAX_HOST_CONNECTIONS",           CURLMOPT_MAX_HOST_CONNECTIONS },` |
|       - |  966 | `	{ "CURLMOPT_MAX_PIPELINE_LENGTH",            CURLMOPT_MAX_PIPELINE_LENGTH },` |
|       - |  967 | `	{ "CURLMOPT_MAX_TOTAL_CONNECTIONS",          CURLMOPT_MAX_TOTAL_CONNECTIONS },` |
|       - |  968 | `	{ "CURLOPT_SASL_IR",                         CURLOPT_SASL_IR },` |
|       - |  969 | `	{ "CURLOPT_DNS_INTERFACE",                   CURLOPT_DNS_INTERFACE },` |
|       - |  970 | `	{ "CURLOPT_DNS_LOCAL_IP4",                   CURLOPT_DNS_LOCAL_IP4 },` |
|       - |  971 | `	{ "CURLOPT_DNS_LOCAL_IP6",                   CURLOPT_DNS_LOCAL_IP6 },` |
|       - |  972 | `	{ "CURLOPT_XOAUTH2_BEARER",                  CURLOPT_XOAUTH2_BEARER },` |
|       - |  973 | `	{ "CURL_HTTP_VERSION_2_0",                   CURL_HTTP_VERSION_2_0 },` |
|       - |  974 | `	{ "CURL_VERSION_HTTP2",                      CURL_VERSION_HTTP2 },` |
|       - |  975 | `	{ "CURLOPT_LOGIN_OPTIONS",                   CURLOPT_LOGIN_OPTIONS },` |
|       - |  976 | `	{ "CURL_SSLVERSION_TLSv1_0",                 CURL_SSLVERSION_TLSv1_0 },` |
|       - |  977 | `	{ "CURL_SSLVERSION_TLSv1_1",                 CURL_SSLVERSION_TLSv1_1 },` |
|       - |  978 | `	{ "CURL_SSLVERSION_TLSv1_2",                 CURL_SSLVERSION_TLSv1_2 },` |
|       - |  979 | `	{ "CURLOPT_EXPECT_100_TIMEOUT_MS",           CURLOPT_EXPECT_100_TIMEOUT_MS },` |
|       - |  980 | `	{ "CURLOPT_SSL_ENABLE_ALPN",                 CURLOPT_SSL_ENABLE_ALPN },` |
|       - |  981 | `	{ "CURLOPT_SSL_ENABLE_NPN",                  CURLOPT_SSL_ENABLE_NPN },` |
|       - |  982 | `	{ "CURLHEADER_SEPARATE",                     CURLHEADER_SEPARATE },` |
|       - |  983 | `	{ "CURLHEADER_UNIFIED",                      CURLHEADER_UNIFIED },` |
|       - |  984 | `	{ "CURLOPT_HEADEROPT",                       CURLOPT_HEADEROPT },` |
|       - |  985 | `	{ "CURLOPT_PROXYHEADER",                     CURLOPT_PROXYHEADER },` |
|       - |  986 | `	{ "CURLAUTH_NEGOTIATE",                      CURLAUTH_NEGOTIATE },` |
|       - |  987 | `	{ "CURL_VERSION_GSSAPI",                     CURL_VERSION_GSSAPI },` |
|       - |  988 | `	{ "CURLOPT_PINNEDPUBLICKEY",                 CURLOPT_PINNEDPUBLICKEY },` |
|       - |  989 | `	{ "CURLOPT_UNIX_SOCKET_PATH",                CURLOPT_UNIX_SOCKET_PATH },` |
|       - |  990 | `	{ "CURLPROTO_SMB",                           CURLPROTO_SMB },` |
|       - |  991 | `	{ "CURLPROTO_SMBS",                          CURLPROTO_SMBS },` |
|       - |  992 | `	{ "CURL_VERSION_KERBEROS5",                  CURL_VERSION_KERBEROS5 },` |
|       - |  993 | `	{ "CURL_VERSION_UNIX_SOCKETS",               CURL_VERSION_UNIX_SOCKETS },` |
|       - |  994 | `	{ "CURLOPT_SSL_VERIFYSTATUS",                CURLOPT_SSL_VERIFYSTATUS },` |
|       - |  995 | `	{ "CURLOPT_PATH_AS_IS",                      CURLOPT_PATH_AS_IS },` |
|       - |  996 | `	{ "CURLOPT_SSL_FALSESTART",                  CURLOPT_SSL_FALSESTART },` |
|       - |  997 | `	{ "CURL_HTTP_VERSION_2",                     CURL_HTTP_VERSION_2 },` |
|       - |  998 | `	{ "CURLOPT_PIPEWAIT",                        CURLOPT_PIPEWAIT },` |
|       - |  999 | `	{ "CURLOPT_PROXY_SERVICE_NAME",              CURLOPT_PROXY_SERVICE_NAME },` |
|       - | 1000 | `	{ "CURLOPT_SERVICE_NAME",                    CURLOPT_SERVICE_NAME },` |
|       - | 1001 | `	{ "CURLPIPE_NOTHING",                        CURLPIPE_NOTHING },` |
|       - | 1002 | `	{ "CURLPIPE_HTTP1",                          CURLPIPE_HTTP1 },` |
|       - | 1003 | `	{ "CURLPIPE_MULTIPLEX",                      CURLPIPE_MULTIPLEX },` |
|       - | 1004 | `	{ "CURLSSLOPT_NO_REVOKE",                    CURLSSLOPT_NO_REVOKE },` |
|       - | 1005 | `	{ "CURLOPT_DEFAULT_PROTOCOL",                CURLOPT_DEFAULT_PROTOCOL },` |
|       - | 1006 | `	{ "CURLOPT_STREAM_WEIGHT",                   CURLOPT_STREAM_WEIGHT },` |
|       - | 1007 | `	{ "CURLMOPT_PUSHFUNCTION",                   CURLMOPT_PUSHFUNCTION },` |
|       - | 1008 | `	{ "CURL_PUSH_OK",                            CURL_PUSH_OK },` |
|       - | 1009 | `	{ "CURL_PUSH_DENY",                          CURL_PUSH_DENY },` |
|       - | 1010 | `	{ "CURL_HTTP_VERSION_2TLS",                  CURL_HTTP_VERSION_2TLS },` |
|       - | 1011 | `	{ "CURL_VERSION_PSL",                        CURL_VERSION_PSL },` |
|       - | 1012 | `	{ "CURLOPT_TFTP_NO_OPTIONS",                 CURLOPT_TFTP_NO_OPTIONS },` |
|       - | 1013 | `	{ "CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE",     CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE },` |
|       - | 1014 | `	{ "CURLOPT_CONNECT_TO",                      CURLOPT_CONNECT_TO },` |
|       - | 1015 | `	{ "CURLOPT_TCP_FASTOPEN",                    CURLOPT_TCP_FASTOPEN },` |
|       - | 1016 | `	{ "CURLINFO_HTTP_VERSION",                   CURLINFO_HTTP_VERSION },` |
|       - | 1017 | `	{ "CURLE_WEIRD_SERVER_REPLY",                CURLE_WEIRD_SERVER_REPLY },` |
|       - | 1018 | `	{ "CURLOPT_KEEP_SENDING_ON_ERROR",           CURLOPT_KEEP_SENDING_ON_ERROR },` |
|       - | 1019 | `	{ "CURL_SSLVERSION_TLSv1_3",                 CURL_SSLVERSION_TLSv1_3 },` |
|       - | 1020 | `	{ "CURL_VERSION_HTTPS_PROXY",                CURL_VERSION_HTTPS_PROXY },` |
|       - | 1021 | `	{ "CURLINFO_PROTOCOL",                       CURLINFO_PROTOCOL },` |
|       - | 1022 | `	{ "CURLINFO_PROXY_SSL_VERIFYRESULT",         CURLINFO_PROXY_SSL_VERIFYRESULT },` |
|       - | 1023 | `	{ "CURLINFO_SCHEME",                         CURLINFO_SCHEME },` |
|       - | 1024 | `	{ "CURLOPT_PRE_PROXY",                       CURLOPT_PRE_PROXY },` |
|       - | 1025 | `	{ "CURLOPT_PROXY_CAINFO",                    CURLOPT_PROXY_CAINFO },` |
|       - | 1026 | `	{ "CURLOPT_PROXY_CAPATH",                    CURLOPT_PROXY_CAPATH },` |
|       - | 1027 | `	{ "CURLOPT_PROXY_CRLFILE",                   CURLOPT_PROXY_CRLFILE },` |
|       - | 1028 | `	{ "CURLOPT_PROXY_KEYPASSWD",                 CURLOPT_PROXY_KEYPASSWD },` |
|       - | 1029 | `	{ "CURLOPT_PROXY_PINNEDPUBLICKEY",           CURLOPT_PROXY_PINNEDPUBLICKEY },` |
|       - | 1030 | `	{ "CURLOPT_PROXY_SSL_CIPHER_LIST",           CURLOPT_PROXY_SSL_CIPHER_LIST },` |
|       - | 1031 | `	{ "CURLOPT_PROXY_SSL_OPTIONS",               CURLOPT_PROXY_SSL_OPTIONS },` |
|       - | 1032 | `	{ "CURLOPT_PROXY_SSL_VERIFYHOST",            CURLOPT_PROXY_SSL_VERIFYHOST },` |
|       - | 1033 | `	{ "CURLOPT_PROXY_SSL_VERIFYPEER",            CURLOPT_PROXY_SSL_VERIFYPEER },` |
|       - | 1034 | `	{ "CURLOPT_PROXY_SSLCERT",                   CURLOPT_PROXY_SSLCERT },` |
|       - | 1035 | `	{ "CURLOPT_PROXY_SSLCERTTYPE",               CURLOPT_PROXY_SSLCERTTYPE },` |
|       - | 1036 | `	{ "CURLOPT_PROXY_SSLKEY",                    CURLOPT_PROXY_SSLKEY },` |
|       - | 1037 | `	{ "CURLOPT_PROXY_SSLKEYTYPE",                CURLOPT_PROXY_SSLKEYTYPE },` |
|       - | 1038 | `	{ "CURLOPT_PROXY_SSLVERSION",                CURLOPT_PROXY_SSLVERSION },` |
|       - | 1039 | `	{ "CURLOPT_PROXY_TLSAUTH_PASSWORD",          CURLOPT_PROXY_TLSAUTH_PASSWORD },` |
|       - | 1040 | `	{ "CURLOPT_PROXY_TLSAUTH_TYPE",              CURLOPT_PROXY_TLSAUTH_TYPE },` |
|       - | 1041 | `	{ "CURLOPT_PROXY_TLSAUTH_USERNAME",          CURLOPT_PROXY_TLSAUTH_USERNAME },` |
|       - | 1042 | `	{ "CURLPROXY_HTTPS",                         CURLPROXY_HTTPS },` |
|       - | 1043 | `	{ "CURL_MAX_READ_SIZE",                      CURL_MAX_READ_SIZE },` |
|       - | 1044 | `	{ "CURLOPT_ABSTRACT_UNIX_SOCKET",            CURLOPT_ABSTRACT_UNIX_SOCKET },` |
|       - | 1045 | `	{ "CURL_SSLVERSION_MAX_DEFAULT",             CURL_SSLVERSION_MAX_DEFAULT },` |
|       - | 1046 | `	{ "CURL_SSLVERSION_MAX_NONE",                CURL_SSLVERSION_MAX_NONE },` |
|       - | 1047 | `	{ "CURL_SSLVERSION_MAX_TLSv1_0",             CURL_SSLVERSION_MAX_TLSv1_0 },` |
|       - | 1048 | `	{ "CURL_SSLVERSION_MAX_TLSv1_1",             CURL_SSLVERSION_MAX_TLSv1_1 },` |
|       - | 1049 | `	{ "CURL_SSLVERSION_MAX_TLSv1_2",             CURL_SSLVERSION_MAX_TLSv1_2 },` |
|       - | 1050 | `	{ "CURL_SSLVERSION_MAX_TLSv1_3",             CURL_SSLVERSION_MAX_TLSv1_3 },` |
|       - | 1051 | `	{ "CURLOPT_SUPPRESS_CONNECT_HEADERS",        CURLOPT_SUPPRESS_CONNECT_HEADERS },` |
|       - | 1052 | `	{ "CURLAUTH_GSSAPI",                         CURLAUTH_GSSAPI },` |
|       - | 1053 | `	{ "CURLINFO_CONTENT_LENGTH_DOWNLOAD_T",      CURLINFO_CONTENT_LENGTH_DOWNLOAD_T },` |
|       - | 1054 | `	{ "CURLINFO_CONTENT_LENGTH_UPLOAD_T",        CURLINFO_CONTENT_LENGTH_UPLOAD_T },` |
|       - | 1055 | `	{ "CURLINFO_SIZE_DOWNLOAD_T",                CURLINFO_SIZE_DOWNLOAD_T },` |
|       - | 1056 | `	{ "CURLINFO_SIZE_UPLOAD_T",                  CURLINFO_SIZE_UPLOAD_T },` |
|       - | 1057 | `	{ "CURLINFO_SPEED_DOWNLOAD_T",               CURLINFO_SPEED_DOWNLOAD_T },` |
|       - | 1058 | `	{ "CURLINFO_SPEED_UPLOAD_T",                 CURLINFO_SPEED_UPLOAD_T },` |
|       - | 1059 | `	{ "CURLOPT_REQUEST_TARGET",                  CURLOPT_REQUEST_TARGET },` |
|       - | 1060 | `	{ "CURLOPT_SOCKS5_AUTH",                     CURLOPT_SOCKS5_AUTH },` |
|       - | 1061 | `	{ "CURLOPT_SSH_COMPRESSION",                 CURLOPT_SSH_COMPRESSION },` |
|       - | 1062 | `	{ "CURL_VERSION_MULTI_SSL",                  CURL_VERSION_MULTI_SSL },` |
|       - | 1063 | `	{ "CURL_VERSION_BROTLI",                     CURL_VERSION_BROTLI },` |
|       - | 1064 | `	{ "CURL_LOCK_DATA_CONNECT",                  CURL_LOCK_DATA_CONNECT },` |
|       - | 1065 | `	{ "CURLSSH_AUTH_GSSAPI",                     CURLSSH_AUTH_GSSAPI },` |
|       - | 1066 | `	{ "CURLINFO_FILETIME_T",                     CURLINFO_FILETIME_T },` |
|       - | 1067 | `	{ "CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS",       CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS },` |
|       - | 1068 | `	{ "CURLOPT_TIMEVALUE_LARGE",                 CURLOPT_TIMEVALUE_LARGE },` |
|       - | 1069 | `	{ "CURLOPT_DNS_SHUFFLE_ADDRESSES",           CURLOPT_DNS_SHUFFLE_ADDRESSES },` |
|       - | 1070 | `	{ "CURLOPT_HAPROXYPROTOCOL",                 CURLOPT_HAPROXYPROTOCOL },` |
|       - | 1071 | `	{ "CURL_LOCK_DATA_PSL",                      CURL_LOCK_DATA_PSL },` |
|       - | 1072 | `	{ "CURLAUTH_BEARER",                         CURLAUTH_BEARER },` |
|       - | 1073 | `	{ "CURLINFO_APPCONNECT_TIME_T",              CURLINFO_APPCONNECT_TIME_T },` |
|       - | 1074 | `	{ "CURLINFO_CONNECT_TIME_T",                 CURLINFO_CONNECT_TIME_T },` |
|       - | 1075 | `	{ "CURLINFO_NAMELOOKUP_TIME_T",              CURLINFO_NAMELOOKUP_TIME_T },` |
|       - | 1076 | `	{ "CURLINFO_PRETRANSFER_TIME_T",             CURLINFO_PRETRANSFER_TIME_T },` |
|       - | 1077 | `	{ "CURLINFO_REDIRECT_TIME_T",                CURLINFO_REDIRECT_TIME_T },` |
|       - | 1078 | `	{ "CURLINFO_STARTTRANSFER_TIME_T",           CURLINFO_STARTTRANSFER_TIME_T },` |
|       - | 1079 | `	{ "CURLINFO_TOTAL_TIME_T",                   CURLINFO_TOTAL_TIME_T },` |
|       - | 1080 | `	{ "CURLINFO_CONN_ID",                        CURLINFO_CONN_ID },` |
|       - | 1081 | `	{ "CURLOPT_DISALLOW_USERNAME_IN_URL",        CURLOPT_DISALLOW_USERNAME_IN_URL },` |
|       - | 1082 | `	{ "CURLOPT_PROXY_TLS13_CIPHERS",             CURLOPT_PROXY_TLS13_CIPHERS },` |
|       - | 1083 | `	{ "CURLOPT_TLS13_CIPHERS",                   CURLOPT_TLS13_CIPHERS },` |
|       - | 1084 | `	{ "CURLOPT_DOH_URL",                         CURLOPT_DOH_URL },` |
|       - | 1085 | `	{ "CURLOPT_UPKEEP_INTERVAL_MS",              CURLOPT_UPKEEP_INTERVAL_MS },` |
|       - | 1086 | `	{ "CURLOPT_UPLOAD_BUFFERSIZE",               CURLOPT_UPLOAD_BUFFERSIZE },` |
|       - | 1087 | `	{ "CURLOPT_HTTP09_ALLOWED",                  CURLOPT_HTTP09_ALLOWED },` |
|       - | 1088 | `	{ "CURLALTSVC_H1",                           CURLALTSVC_H1 },` |
|       - | 1089 | `	{ "CURLALTSVC_H2",                           CURLALTSVC_H2 },` |
|       - | 1090 | `	{ "CURLALTSVC_H3",                           CURLALTSVC_H3 },` |
|       - | 1091 | `	{ "CURLALTSVC_READONLYFILE",                 CURLALTSVC_READONLYFILE },` |
|       - | 1092 | `	{ "CURLOPT_ALTSVC",                          CURLOPT_ALTSVC },` |
|       - | 1093 | `	{ "CURLOPT_ALTSVC_CTRL",                     CURLOPT_ALTSVC_CTRL },` |
|       - | 1094 | `	{ "CURL_VERSION_ALTSVC",                     CURL_VERSION_ALTSVC },` |
|       - | 1095 | `	{ "CURLOPT_MAXAGE_CONN",                     CURLOPT_MAXAGE_CONN },` |
|       - | 1096 | `	{ "CURLOPT_SASL_AUTHZID",                    CURLOPT_SASL_AUTHZID },` |
|       - | 1097 | `	{ "CURL_VERSION_HTTP3",                      CURL_VERSION_HTTP3 },` |
|       - | 1098 | `	{ "CURLINFO_RETRY_AFTER",                    CURLINFO_RETRY_AFTER },` |
|       - | 1099 | `	{ "CURL_HTTP_VERSION_3",                     CURL_HTTP_VERSION_3 },` |
|       - | 1100 | `	{ "CURLMOPT_MAX_CONCURRENT_STREAMS",         CURLMOPT_MAX_CONCURRENT_STREAMS },` |
|       - | 1101 | `	{ "CURLSSLOPT_NO_PARTIALCHAIN",              CURLSSLOPT_NO_PARTIALCHAIN },` |
|       - | 1102 | `	{ "CURLOPT_MAIL_RCPT_ALLLOWFAILS",           CURLOPT_MAIL_RCPT_ALLLOWFAILS },` |
|       - | 1103 | `	{ "CURLSSLOPT_REVOKE_BEST_EFFORT",           CURLSSLOPT_REVOKE_BEST_EFFORT },` |
|       - | 1104 | `	{ "CURLOPT_ISSUERCERT_BLOB",                 CURLOPT_ISSUERCERT_BLOB },` |
|       - | 1105 | `	{ "CURLOPT_PROXY_ISSUERCERT",                CURLOPT_PROXY_ISSUERCERT },` |
|       - | 1106 | `	{ "CURLOPT_PROXY_ISSUERCERT_BLOB",           CURLOPT_PROXY_ISSUERCERT_BLOB },` |
|       - | 1107 | `	{ "CURLOPT_PROXY_SSLCERT_BLOB",              CURLOPT_PROXY_SSLCERT_BLOB },` |
|       - | 1108 | `	{ "CURLOPT_PROXY_SSLKEY_BLOB",               CURLOPT_PROXY_SSLKEY_BLOB },` |
|       - | 1109 | `	{ "CURLOPT_SSLCERT_BLOB",                    CURLOPT_SSLCERT_BLOB },` |
|       - | 1110 | `	{ "CURLOPT_SSLKEY_BLOB",                     CURLOPT_SSLKEY_BLOB },` |
|       - | 1111 | `	{ "CURLPROTO_MQTT",                          CURLPROTO_MQTT },` |
|       - | 1112 | `	{ "CURLSSLOPT_NATIVE_CA",                    CURLSSLOPT_NATIVE_CA },` |
|       - | 1113 | `	{ "CURL_VERSION_UNICODE",                    CURL_VERSION_UNICODE },` |
|       - | 1114 | `	{ "CURL_VERSION_ZSTD",                       CURL_VERSION_ZSTD },` |
|       - | 1115 | `	{ "CURLE_PROXY",                             CURLE_PROXY },` |
|       - | 1116 | `	{ "CURLINFO_PROXY_ERROR",                    CURLINFO_PROXY_ERROR },` |
|       - | 1117 | `	{ "CURLOPT_SSL_EC_CURVES",                   CURLOPT_SSL_EC_CURVES },` |
|       - | 1118 | `	{ "CURLPX_BAD_ADDRESS_TYPE",                 CURLPX_BAD_ADDRESS_TYPE },` |
|       - | 1119 | `	{ "CURLPX_BAD_VERSION",                      CURLPX_BAD_VERSION },` |
|       - | 1120 | `	{ "CURLPX_CLOSED",                           CURLPX_CLOSED },` |
|       - | 1121 | `	{ "CURLPX_GSSAPI",                           CURLPX_GSSAPI },` |
|       - | 1122 | `	{ "CURLPX_GSSAPI_PERMSG",                    CURLPX_GSSAPI_PERMSG },` |
|       - | 1123 | `	{ "CURLPX_GSSAPI_PROTECTION",                CURLPX_GSSAPI_PROTECTION },` |
|       - | 1124 | `	{ "CURLPX_IDENTD",                           CURLPX_IDENTD },` |
|       - | 1125 | `	{ "CURLPX_IDENTD_DIFFER",                    CURLPX_IDENTD_DIFFER },` |
|       - | 1126 | `	{ "CURLPX_LONG_HOSTNAME",                    CURLPX_LONG_HOSTNAME },` |
|       - | 1127 | `	{ "CURLPX_LONG_PASSWD",                      CURLPX_LONG_PASSWD },` |
|       - | 1128 | `	{ "CURLPX_LONG_USER",                        CURLPX_LONG_USER },` |
|       - | 1129 | `	{ "CURLPX_NO_AUTH",                          CURLPX_NO_AUTH },` |
|       - | 1130 | `	{ "CURLPX_OK",                               CURLPX_OK },` |
|       - | 1131 | `	{ "CURLPX_RECV_ADDRESS",                     CURLPX_RECV_ADDRESS },` |
|       - | 1132 | `	{ "CURLPX_RECV_AUTH",                        CURLPX_RECV_AUTH },` |
|       - | 1133 | `	{ "CURLPX_RECV_CONNECT",                     CURLPX_RECV_CONNECT },` |
|       - | 1134 | `	{ "CURLPX_RECV_REQACK",                      CURLPX_RECV_REQACK },` |
|       - | 1135 | `	{ "CURLPX_REPLY_ADDRESS_TYPE_NOT_SUPPORTED", CURLPX_REPLY_ADDRESS_TYPE_NOT_SUPPORTED },` |
|       - | 1136 | `	{ "CURLPX_REPLY_COMMAND_NOT_SUPPORTED",      CURLPX_REPLY_COMMAND_NOT_SUPPORTED },` |
|       - | 1137 | `	{ "CURLPX_REPLY_CONNECTION_REFUSED",         CURLPX_REPLY_CONNECTION_REFUSED },` |
|       - | 1138 | `	{ "CURLPX_REPLY_GENERAL_SERVER_FAILURE",     CURLPX_REPLY_GENERAL_SERVER_FAILURE },` |
|       - | 1139 | `	{ "CURLPX_REPLY_HOST_UNREACHABLE",           CURLPX_REPLY_HOST_UNREACHABLE },` |
|       - | 1140 | `	{ "CURLPX_REPLY_NETWORK_UNREACHABLE",        CURLPX_REPLY_NETWORK_UNREACHABLE },` |
|       - | 1141 | `	{ "CURLPX_REPLY_NOT_ALLOWED",                CURLPX_REPLY_NOT_ALLOWED },` |
|       - | 1142 | `	{ "CURLPX_REPLY_TTL_EXPIRED",                CURLPX_REPLY_TTL_EXPIRED },` |
|       - | 1143 | `	{ "CURLPX_REPLY_UNASSIGNED",                 CURLPX_REPLY_UNASSIGNED },` |
|       - | 1144 | `	{ "CURLPX_REQUEST_FAILED",                   CURLPX_REQUEST_FAILED },` |
|       - | 1145 | `	{ "CURLPX_RESOLVE_HOST",                     CURLPX_RESOLVE_HOST },` |
|       - | 1146 | `	{ "CURLPX_SEND_AUTH",                        CURLPX_SEND_AUTH },` |
|       - | 1147 | `	{ "CURLPX_SEND_CONNECT",                     CURLPX_SEND_CONNECT },` |
|       - | 1148 | `	{ "CURLPX_SEND_REQUEST",                     CURLPX_SEND_REQUEST },` |
|       - | 1149 | `	{ "CURLPX_UNKNOWN_FAIL",                     CURLPX_UNKNOWN_FAIL },` |
|       - | 1150 | `	{ "CURLPX_UNKNOWN_MODE",                     CURLPX_UNKNOWN_MODE },` |
|       - | 1151 | `	{ "CURLPX_USER_REJECTED",                    CURLPX_USER_REJECTED },` |
|       - | 1152 | `	{ "CURLHSTS_ENABLE",                         CURLHSTS_ENABLE },` |
|       - | 1153 | `	{ "CURLHSTS_READONLYFILE",                   CURLHSTS_READONLYFILE },` |
|       - | 1154 | `	{ "CURLOPT_HSTS",                            CURLOPT_HSTS },` |
|       - | 1155 | `	{ "CURLOPT_HSTS_CTRL",                       CURLOPT_HSTS_CTRL },` |
|       - | 1156 | `	{ "CURL_VERSION_HSTS",                       CURL_VERSION_HSTS },` |
|       - | 1157 | `	{ "CURLAUTH_AWS_SIGV4",                      CURLAUTH_AWS_SIGV4 },` |
|       - | 1158 | `	{ "CURLOPT_AWS_SIGV4",                       CURLOPT_AWS_SIGV4 },` |
|       - | 1159 | `	{ "CURLINFO_REFERER",                        CURLINFO_REFERER },` |
|       - | 1160 | `	{ "CURLOPT_DOH_SSL_VERIFYHOST",              CURLOPT_DOH_SSL_VERIFYHOST },` |
|       - | 1161 | `	{ "CURLOPT_DOH_SSL_VERIFYPEER",              CURLOPT_DOH_SSL_VERIFYPEER },` |
|       - | 1162 | `	{ "CURLOPT_DOH_SSL_VERIFYSTATUS",            CURLOPT_DOH_SSL_VERIFYSTATUS },` |
|       - | 1163 | `	{ "CURL_VERSION_GSASL",                      CURL_VERSION_GSASL },` |
|       - | 1164 | `	{ "CURLOPT_CAINFO_BLOB",                     CURLOPT_CAINFO_BLOB },` |
|       - | 1165 | `	{ "CURLOPT_PROXY_CAINFO_BLOB",               CURLOPT_PROXY_CAINFO_BLOB },` |
|       - | 1166 | `	{ "CURLSSLOPT_AUTO_CLIENT_CERT",             CURLSSLOPT_AUTO_CLIENT_CERT },` |
|       - | 1167 | `	{ "CURLOPT_MAXLIFETIME_CONN",                CURLOPT_MAXLIFETIME_CONN },` |
|       - | 1168 | `	{ "CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256",      CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256 },` |
|       - | 1169 | `	{ "CURLOPT_PREREQFUNCTION",                  CURLOPT_PREREQFUNCTION },` |
|       - | 1170 | `	{ "CURL_PREREQFUNC_OK",                      CURL_PREREQFUNC_OK },` |
|       - | 1171 | `	{ "CURL_PREREQFUNC_ABORT",                   CURL_PREREQFUNC_ABORT },` |
|       - | 1172 | `	{ "CURLOPT_MIME_OPTIONS",                    CURLOPT_MIME_OPTIONS },` |
|       - | 1173 | `	{ "CURLMIMEOPT_FORMESCAPE",                  CURLMIMEOPT_FORMESCAPE },` |
|       - | 1174 | `	{ "CURLOPT_SSH_HOSTKEYFUNCTION",             CURLOPT_SSH_HOSTKEYFUNCTION },` |
|       - | 1175 | `	{ "CURLOPT_PROTOCOLS_STR",                   CURLOPT_PROTOCOLS_STR },` |
|       - | 1176 | `	{ "CURLOPT_REDIR_PROTOCOLS_STR",             CURLOPT_REDIR_PROTOCOLS_STR },` |
|       - | 1177 | `	{ "CURLOPT_WS_OPTIONS",                      CURLOPT_WS_OPTIONS },` |
|       - | 1178 | `	{ "CURLWS_RAW_MODE",                         CURLWS_RAW_MODE },` |
|       - | 1179 | `	{ "CURLOPT_CA_CACHE_TIMEOUT",                CURLOPT_CA_CACHE_TIMEOUT },` |
|       - | 1180 | `	{ "CURLOPT_QUICK_EXIT",                      CURLOPT_QUICK_EXIT },` |
|       - | 1181 | `	{ "CURL_HTTP_VERSION_3ONLY",                 CURL_HTTP_VERSION_3ONLY },` |
|       - | 1182 | `	{ "CURLOPT_SAFE_UPLOAD",                     -1 },` |
|       - | 1183 | `};` |
|       - | 1184 |  |
|   56763 | 1185 | `static void CurlConstExpand(ph7_value *pVal,void *pUserData)` |
|       5 | 1186 | `{` |
|   56768 | 1187 | `	ph7_value_int64(pVal,((const struct CurlConstant *)pUserData)->iValue);` |
|   56768 | 1188 | `}` |
|       - | 1189 |  |
|    6985 | 1190 | `PH7_PRIVATE void PH7_RegisterCurlConstants(ph7_vm *pVm)` |
|       5 | 1191 | `{` |
|       - | 1192 | `	sxu32 n;` |
| 4749805 | 1193 | `	for( n = 0 ; n < SX_ARRAYSIZE(aCurlConst) ; ++n ){` |
| 7110493 | 1194 | `		ph7_create_constant(&(*pVm),aCurlConst[n].zName,CurlConstExpand,` |
| 4742815 | 1195 | `			(void *)&aCurlConst[n]);` |
| 2367678 | 1196 | `	}` |
|    6990 | 1197 | `}` |
|       - | 1198 |  |
|       - | 1199 | `/* ===== curl_version() ===== */` |
|       - | 1200 |  |
|       - | 1201 | `/*` |
|       - | 1202 | ` * php's feature-name table: the bit each name reports, in php's own order.` |
|       - | 1203 | ` * The names are php's spelling, not libcurl's constant tails ("GSS-Negotiate",` |
|       - | 1204 | ` * "krb4", "TLS-SRP", "NTLMWB", "CharConv"), and all 29 were verified against` |
|       - | 1205 | ` * both the oracle's feature_list and curl_version_info()'s features word.` |
|       - | 1206 | ` */` |
|       - | 1207 | `static const struct CurlFeatureName {` |
|       - | 1208 | `	const char *zName;` |
|       - | 1209 | `	unsigned int iBit;` |
|       - | 1210 | `} aCurlFeature[] = {` |
|       - | 1211 | `	{ "AsynchDNS",     CURL_VERSION_ASYNCHDNS     },` |
|       - | 1212 | `	{ "CharConv",      CURL_VERSION_CONV          },` |
|       - | 1213 | `	{ "Debug",         CURL_VERSION_DEBUG         },` |
|       - | 1214 | `	{ "GSS-Negotiate", CURL_VERSION_GSSNEGOTIATE  },` |
|       - | 1215 | `	{ "IDN",           CURL_VERSION_IDN           },` |
|       - | 1216 | `	{ "IPv6",          CURL_VERSION_IPV6          },` |
|       - | 1217 | `	{ "krb4",          CURL_VERSION_KERBEROS4     },` |
|       - | 1218 | `	{ "Largefile",     CURL_VERSION_LARGEFILE     },` |
|       - | 1219 | `	{ "libz",          CURL_VERSION_LIBZ          },` |
|       - | 1220 | `	{ "NTLM",          CURL_VERSION_NTLM          },` |
|       - | 1221 | `	{ "NTLMWB",        CURL_VERSION_NTLM_WB       },` |
|       - | 1222 | `	{ "SPNEGO",        CURL_VERSION_SPNEGO        },` |
|       - | 1223 | `	{ "SSL",           CURL_VERSION_SSL           },` |
|       - | 1224 | `	{ "SSPI",          CURL_VERSION_SSPI          },` |
|       - | 1225 | `	{ "TLS-SRP",       CURL_VERSION_TLSAUTH_SRP   },` |
|       - | 1226 | `	{ "HTTP2",         CURL_VERSION_HTTP2         },` |
|       - | 1227 | `	{ "GSSAPI",        CURL_VERSION_GSSAPI        },` |
|       - | 1228 | `	{ "KERBEROS5",     CURL_VERSION_KERBEROS5     },` |
|       - | 1229 | `	{ "UNIX_SOCKETS",  CURL_VERSION_UNIX_SOCKETS  },` |
|       - | 1230 | `	{ "PSL",           CURL_VERSION_PSL           },` |
|       - | 1231 | `	{ "HTTPS_PROXY",   CURL_VERSION_HTTPS_PROXY   },` |
|       - | 1232 | `	{ "MULTI_SSL",     CURL_VERSION_MULTI_SSL     },` |
|       - | 1233 | `	{ "BROTLI",        CURL_VERSION_BROTLI        },` |
|       - | 1234 | `	{ "ALTSVC",        CURL_VERSION_ALTSVC        },` |
|       - | 1235 | `	{ "HTTP3",         CURL_VERSION_HTTP3         },` |
|       - | 1236 | `	{ "UNICODE",       CURL_VERSION_UNICODE       },` |
|       - | 1237 | `	{ "ZSTD",          CURL_VERSION_ZSTD          },` |
|       - | 1238 | `	{ "HSTS",          CURL_VERSION_HSTS          },` |
|       - | 1239 | `	{ "GSASL",         CURL_VERSION_GSASL         }` |
|       - | 1240 | `};` |
|       - | 1241 |  |
|       - | 1242 | `/* An int entry on the answer. */` |
|      14 | 1243 | `static void CurlVersionAddInt(ph7_value *pArray,ph7_value *pWorker,` |
|       - | 1244 | `	const char *zKey,sxi64 iVal)` |
|       1 | 1245 | `{` |
|      15 | 1246 | `	ph7_value_int64(pWorker,iVal);` |
|      15 | 1247 | `	ph7_array_add_strkey_elem(pArray,zKey,pWorker);` |
|      15 | 1248 | `}` |
|       - | 1249 |  |
|       - | 1250 | `/*` |
|       - | 1251 | ` * A string entry on the answer. php prints the EMPTY STRING for a field` |
|       - | 1252 | `` * libcurl left NULL -- `ares` is NULL in every build without c-ares and the`` |
|       - | 1253 | ` * oracle still answers string(0) "" for it, never null.` |
|       - | 1254 | ` */` |
|      16 | 1255 | `static void CurlVersionAddStr(ph7_value *pArray,ph7_value *pWorker,` |
|       - | 1256 | `	const char *zKey,const char *zVal)` |
|       1 | 1257 | `{` |
|      17 | 1258 | `	ph7_value_reset_string_cursor(pWorker);` |
|      17 | 1259 | `	ph7_value_string(pWorker,zVal ? zVal : "",-1);` |
|      17 | 1260 | `	ph7_array_add_strkey_elem(pArray,zKey,pWorker);` |
|      17 | 1261 | `	ph7_value_reset_string_cursor(pWorker);` |
|      17 | 1262 | `}` |
|       - | 1263 |  |
|       - | 1264 | `/* array\|false curl_version() */` |
|       2 | 1265 | `static int vm_builtin_curl_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1266 | `{` |
|       - | 1267 | `	curl_version_info_data *pInfo;` |
|       - | 1268 | `	ph7_value *pArray,*pWorker,*pFeature,*pProto;` |
|       - | 1269 | `	const char * const *pzProto;` |
|       - | 1270 | `	sxu32 n;` |
|       1 | 1271 | `	SXUNUSED(nArg);` |
|       1 | 1272 | `	SXUNUSED(apArg);` |
|       3 | 1273 | `	PH7_CurlGlobalInit();` |
|       3 | 1274 | `	pInfo = curl_version_info(CURLVERSION_NOW);` |
|       3 | 1275 | `	if( pInfo == 0 ){` |
|       - | 1276 | `		/* php's own guard: the library answered nothing, so neither do we. */` |
|     ! 0 | 1277 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1278 | `		return PH7_OK;` |
|       - | 1279 | `	}` |
|       3 | 1280 | `	pArray   = ph7_context_new_array(pCtx);` |
|       3 | 1281 | `	pWorker  = ph7_context_new_scalar(pCtx);` |
|       3 | 1282 | `	pFeature = ph7_context_new_array(pCtx);` |
|       3 | 1283 | `	pProto   = ph7_context_new_array(pCtx);` |
|       3 | 1284 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pFeature == 0 \|\| pProto == 0 ){` |
|     ! 0 | 1285 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 | 1286 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1287 | `		return PH7_OK;` |
|       - | 1288 | `	}` |
|       - | 1289 | `	/* The key ORDER is php's and is user-visible through print_r/var_dump and` |
|       - | 1290 | `	 * a foreach; it is not libcurl's struct order. */` |
|       3 | 1291 | `	CurlVersionAddInt(pArray,pWorker,"version_number",(sxi64)pInfo->version_num);` |
|       3 | 1292 | `	CurlVersionAddInt(pArray,pWorker,"age",(sxi64)pInfo->age);` |
|       3 | 1293 | `	CurlVersionAddInt(pArray,pWorker,"features",(sxi64)pInfo->features);` |
|      61 | 1294 | `	for( n = 0 ; n < SX_ARRAYSIZE(aCurlFeature) ; ++n ){` |
|      59 | 1295 | `		ph7_value_bool(pWorker,(pInfo->features & aCurlFeature[n].iBit) ? 1 : 0);` |
|      59 | 1296 | `		ph7_array_add_strkey_elem(pFeature,aCurlFeature[n].zName,pWorker);` |
|      30 | 1297 | `	}` |
|       3 | 1298 | `	ph7_array_add_strkey_elem(pArray,"feature_list",pFeature);` |
|       3 | 1299 | `	CurlVersionAddInt(pArray,pWorker,"ssl_version_number",(sxi64)pInfo->ssl_version_num);` |
|       3 | 1300 | `	CurlVersionAddStr(pArray,pWorker,"version",pInfo->version);` |
|       3 | 1301 | `	CurlVersionAddStr(pArray,pWorker,"host",pInfo->host);` |
|       3 | 1302 | `	CurlVersionAddStr(pArray,pWorker,"ssl_version",pInfo->ssl_version);` |
|       3 | 1303 | `	CurlVersionAddStr(pArray,pWorker,"libz_version",pInfo->libz_version);` |
|      58 | 1304 | `	for( pzProto = pInfo->protocols ; pzProto && *pzProto ; ++pzProto ){` |
|      56 | 1305 | `		ph7_value_reset_string_cursor(pWorker);` |
|      56 | 1306 | `		ph7_value_string(pWorker,*pzProto,-1);` |
|      56 | 1307 | `		ph7_array_add_elem(pProto,0,pWorker);` |
|      26 | 1308 | `	}` |
|       3 | 1309 | `	ph7_value_reset_string_cursor(pWorker);` |
|       3 | 1310 | `	ph7_array_add_strkey_elem(pArray,"protocols",pProto);` |
|       - | 1311 | `	/*` |
|       - | 1312 | `	 * Everything below is gated on the struct's OWN age, which is what php` |
|       - | 1313 | `	 * gates on -- and php stops at the brotli block even against a library` |
|       - | 1314 | `	 * reporting age 10, so zstd_version and the fields after it are absent` |
|       - | 1315 | `	 * from the answer even where libcurl reports them. Reproducing php means` |
|       - | 1316 | `	 * stopping here too, not exposing what the library happens to know.` |
|       - | 1317 | `	 */` |
|       3 | 1318 | `	if( pInfo->age >= CURLVERSION_SECOND ){` |
|       3 | 1319 | `		CurlVersionAddStr(pArray,pWorker,"ares",pInfo->ares);` |
|       3 | 1320 | `		CurlVersionAddInt(pArray,pWorker,"ares_num",(sxi64)pInfo->ares_num);` |
|       1 | 1321 | `	}` |
|       3 | 1322 | `	if( pInfo->age >= CURLVERSION_THIRD ){` |
|       3 | 1323 | `		CurlVersionAddStr(pArray,pWorker,"libidn",pInfo->libidn);` |
|       1 | 1324 | `	}` |
|       3 | 1325 | `	if( pInfo->age >= CURLVERSION_FOURTH ){` |
|       3 | 1326 | `		CurlVersionAddInt(pArray,pWorker,"iconv_ver_num",(sxi64)pInfo->iconv_ver_num);` |
|       3 | 1327 | `		CurlVersionAddStr(pArray,pWorker,"libssh_version",pInfo->libssh_version);` |
|       1 | 1328 | `	}` |
|       3 | 1329 | `	if( pInfo->age >= CURLVERSION_FIFTH ){` |
|       3 | 1330 | `		CurlVersionAddInt(pArray,pWorker,"brotli_ver_num",(sxi64)pInfo->brotli_ver_num);` |
|       3 | 1331 | `		CurlVersionAddStr(pArray,pWorker,"brotli_version",pInfo->brotli_version);` |
|       1 | 1332 | `	}` |
|       3 | 1333 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 1334 | `	return PH7_OK;` |
|       2 | 1335 | `}` |
|       - | 1336 |  |
|       - | 1337 | `/* ===== The three strerror families ===== */` |
|       - | 1338 |  |
|       - | 1339 | `/*` |
|       - | 1340 | ` * All three are pass-throughs, and the pass-through is the point: the codes` |
|       - | 1341 | ` * are C enums, so php narrows its zend_long argument to the enum's width` |
|       - | 1342 | ` * before the library ever sees it. That truncation is user-visible --` |
|       - | 1343 | ` * curl_multi_strerror(PHP_INT_MAX) answers "Please call curl_multi_perform()` |
|       - | 1344 | ` * soon" because the value arrives as -1 (CURLM_CALL_MULTI_PERFORM), not` |
|       - | 1345 | ` * because PHP_INT_MAX means anything. Casting to int here reproduces it; a` |
|       - | 1346 | ` * range check would not.` |
|       - | 1347 | ` *` |
|       - | 1348 | ` * The declared return type is ?string for the same reason php's is: the` |
|       - | 1349 | ` * answer is whatever the library's pointer says, including NULL.` |
|       - | 1350 | ` */` |
|      18 | 1351 | `static int CurlStrError(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 1352 | `	const char *(*xStr)(int))` |
|       1 | 1353 | `{` |
|      19 | 1354 | `	int iCode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : 0;` |
|      19 | 1355 | `	const char *zMsg = xStr(iCode);` |
|      19 | 1356 | `	if( zMsg == 0 ){` |
|     ! 0 | 1357 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1358 | `	}else{` |
|      19 | 1359 | `		ph7_result_string(pCtx,zMsg,-1);` |
|       - | 1360 | `	}` |
|      19 | 1361 | `	return PH7_OK;` |
|       1 | 1362 | `}` |
|      10 | 1363 | `static const char * CurlEasyStrErrorTrampoline(int iCode)` |
|       1 | 1364 | `{` |
|      11 | 1365 | `	return curl_easy_strerror((CURLcode)iCode);` |
|       1 | 1366 | `}` |
|       6 | 1367 | `static const char * CurlMultiStrErrorTrampoline(int iCode)` |
|       1 | 1368 | `{` |
|       7 | 1369 | `	return curl_multi_strerror((CURLMcode)iCode);` |
|       1 | 1370 | `}` |
|       2 | 1371 | `static const char * CurlShareStrErrorTrampoline(int iCode)` |
|       1 | 1372 | `{` |
|       3 | 1373 | `	return curl_share_strerror((CURLSHcode)iCode);` |
|       1 | 1374 | `}` |
|       - | 1375 | `/* ?string curl_strerror(int $error_code) */` |
|      10 | 1376 | `static int vm_builtin_curl_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1377 | `{` |
|      11 | 1378 | `	return CurlStrError(pCtx,nArg,apArg,CurlEasyStrErrorTrampoline);` |
|       1 | 1379 | `}` |
|       - | 1380 | `/* ?string curl_multi_strerror(int $error_code) */` |
|       6 | 1381 | `static int vm_builtin_curl_multi_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1382 | `{` |
|       7 | 1383 | `	return CurlStrError(pCtx,nArg,apArg,CurlMultiStrErrorTrampoline);` |
|       1 | 1384 | `}` |
|       - | 1385 | `/* ?string curl_share_strerror(int $error_code) */` |
|       2 | 1386 | `static int vm_builtin_curl_share_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1387 | `{` |
|       3 | 1388 | `	return CurlStrError(pCtx,nArg,apArg,CurlShareStrErrorTrampoline);` |
|       1 | 1389 | `}` |
|       - | 1390 |  |
|       - | 1391 | `/* ===== curl_setopt() ===== */` |
|       - | 1392 |  |
|       - | 1393 | `/*` |
|       - | 1394 | ` * What php DOES with the value it is handed, per option.` |
|       - | 1395 | ` *` |
|       - | 1396 | ` * The kinds were derived by sweeping all 269 options against 17 value types` |
|       - | 1397 | ` * and reading what php answered; they are not libcurl's own typing, though` |
|       - | 1398 | ` * they mostly follow from it. libcurl encodes a type in the option NUMBER` |
|       - | 1399 | ` * (below 10000 long, 10000-19999 pointer, 20000+ function, 30000+ off_t,` |
|       - | 1400 | ` * 40000+ blob) and php's switch agrees with that bucket for 247 of the 269 --` |
|       - | 1401 | ` * the exceptions are the whole reason this is a table rather than arithmetic:` |
|       - | 1402 | ` * ten pointer options take an ARRAY php turns into a curl_slist, five take a` |
|       - | 1403 | ` * php STREAM, and six are php's own (RETURNTRANSFER and BINARYTRANSFER exist` |
|       - | 1404 | ` * in no libcurl, PRIVATE stores a php value, SHARE takes a share handle,` |
|       - | 1405 | ` * SAFE_UPLOAD refuses to be turned off, DNS_USE_GLOBAL_CACHE is accepted and` |
|       - | 1406 | ` * ignored).` |
|       - | 1407 | ` *` |
|       - | 1408 | ` * The table is also the VALIDATOR. php does not ask libcurl whether an option` |
|       - | 1409 | ` * exists: an unknown number never reaches the library, it falls off the end of` |
|       - | 1410 | ` * php's switch. So a number a newer libcurl knows and this table does not is a` |
|       - | 1411 | ` * ValueError here exactly as it is in php built against the older library.` |
|       - | 1412 | ` */` |
|       - | 1413 | `#define CURL_OPT_LONG       0   /* zval -> long, straight to libcurl */` |
|       - | 1414 | `#define CURL_OPT_STRING     1   /* zval -> string, NUL-screened */` |
|       - | 1415 | `#define CURL_OPT_SLIST      2   /* array -> curl_slist owned by the handle */` |
|       - | 1416 | `#define CURL_OPT_CALLBACK   3   /* a php callable (its own slice) */` |
|       - | 1417 | `#define CURL_OPT_FILE       4   /* a php stream (its own slice) */` |
|       - | 1418 | `#define CURL_OPT_SAFEUP     5   /* php's own: truthy only */` |
|       - | 1419 | `#define CURL_OPT_RETURN     6   /* php's own: exec answers the body */` |
|       - | 1420 | `#define CURL_OPT_PRIVATE    7   /* php's own: stores the value itself */` |
|       - | 1421 | `#define CURL_OPT_SHARE      8   /* php's own: a CurlShareHandle */` |
|       - | 1422 | `#define CURL_OPT_POSTFIELDS 9   /* string or array (the upload slice) */` |
|       - | 1423 | `#define CURL_OPT_IGNORE    10   /* accepted and read by nothing, like php */` |
|       - | 1424 |  |
|       - | 1425 | `static const struct CurlOptDef {` |
|       - | 1426 | `	sxi64 iOpt;` |
|       - | 1427 | `	int iKind;` |
|       - | 1428 | `} aCurlOpt[] = {` |
|       - | 1429 | `	{ -1,                                CURL_OPT_SAFEUP     },  /* -1 */` |
|       - | 1430 | `	{ CURLOPT_PORT,                      CURL_OPT_LONG       },  /* 3 */` |
|       - | 1431 | `	{ CURLOPT_TIMEOUT,                   CURL_OPT_LONG       },  /* 13 */` |
|       - | 1432 | `	{ CURLOPT_INFILESIZE,                CURL_OPT_LONG       },  /* 14 */` |
|       - | 1433 | `	{ CURLOPT_LOW_SPEED_LIMIT,           CURL_OPT_LONG       },  /* 19 */` |
|       - | 1434 | `	{ CURLOPT_LOW_SPEED_TIME,            CURL_OPT_LONG       },  /* 20 */` |
|       - | 1435 | `	{ CURLOPT_RESUME_FROM,               CURL_OPT_LONG       },  /* 21 */` |
|       - | 1436 | `	{ CURLOPT_CRLF,                      CURL_OPT_LONG       },  /* 27 */` |
|       - | 1437 | `	{ CURLOPT_SSLVERSION,                CURL_OPT_LONG       },  /* 32 */` |
|       - | 1438 | `	{ CURLOPT_TIMECONDITION,             CURL_OPT_LONG       },  /* 33 */` |
|       - | 1439 | `	{ CURLOPT_TIMEVALUE,                 CURL_OPT_LONG       },  /* 34 */` |
|       - | 1440 | `	{ CURLOPT_VERBOSE,                   CURL_OPT_LONG       },  /* 41 */` |
|       - | 1441 | `	{ CURLOPT_HEADER,                    CURL_OPT_LONG       },  /* 42 */` |
|       - | 1442 | `	{ CURLOPT_NOPROGRESS,                CURL_OPT_LONG       },  /* 43 */` |
|       - | 1443 | `	{ CURLOPT_NOBODY,                    CURL_OPT_LONG       },  /* 44 */` |
|       - | 1444 | `	{ CURLOPT_FAILONERROR,               CURL_OPT_LONG       },  /* 45 */` |
|       - | 1445 | `	{ CURLOPT_UPLOAD,                    CURL_OPT_LONG       },  /* 46 */` |
|       - | 1446 | `	{ CURLOPT_POST,                      CURL_OPT_LONG       },  /* 47 */` |
|       - | 1447 | `	{ CURLOPT_DIRLISTONLY,               CURL_OPT_LONG       },  /* 48 */` |
|       - | 1448 | `	{ CURLOPT_FTPLISTONLY,               CURL_OPT_LONG       },  /* 48 */` |
|       - | 1449 | `	{ CURLOPT_APPEND,                    CURL_OPT_LONG       },  /* 50 */` |
|       - | 1450 | `	{ CURLOPT_FTPAPPEND,                 CURL_OPT_LONG       },  /* 50 */` |
|       - | 1451 | `	{ CURLOPT_NETRC,                     CURL_OPT_LONG       },  /* 51 */` |
|       - | 1452 | `	{ CURLOPT_FOLLOWLOCATION,            CURL_OPT_LONG       },  /* 52 */` |
|       - | 1453 | `	{ CURLOPT_TRANSFERTEXT,              CURL_OPT_LONG       },  /* 53 */` |
|       - | 1454 | `	{ CURLOPT_PUT,                       CURL_OPT_LONG       },  /* 54 */` |
|       - | 1455 | `	{ CURLOPT_AUTOREFERER,               CURL_OPT_LONG       },  /* 58 */` |
|       - | 1456 | `	{ CURLOPT_PROXYPORT,                 CURL_OPT_LONG       },  /* 59 */` |
|       - | 1457 | `	{ CURLOPT_HTTPPROXYTUNNEL,           CURL_OPT_LONG       },  /* 61 */` |
|       - | 1458 | `	{ CURLOPT_SSL_VERIFYPEER,            CURL_OPT_LONG       },  /* 64 */` |
|       - | 1459 | `	{ CURLOPT_MAXREDIRS,                 CURL_OPT_LONG       },  /* 68 */` |
|       - | 1460 | `	{ CURLOPT_FILETIME,                  CURL_OPT_LONG       },  /* 69 */` |
|       - | 1461 | `	{ CURLOPT_MAXCONNECTS,               CURL_OPT_LONG       },  /* 71 */` |
|       - | 1462 | `	{ CURLOPT_FRESH_CONNECT,             CURL_OPT_LONG       },  /* 74 */` |
|       - | 1463 | `	{ CURLOPT_FORBID_REUSE,              CURL_OPT_LONG       },  /* 75 */` |
|       - | 1464 | `	{ CURLOPT_CONNECTTIMEOUT,            CURL_OPT_LONG       },  /* 78 */` |
|       - | 1465 | `	{ CURLOPT_HTTPGET,                   CURL_OPT_LONG       },  /* 80 */` |
|       - | 1466 | `	{ CURLOPT_SSL_VERIFYHOST,            CURL_OPT_LONG       },  /* 81 */` |
|       - | 1467 | `	{ CURLOPT_HTTP_VERSION,              CURL_OPT_LONG       },  /* 84 */` |
|       - | 1468 | `	{ CURLOPT_FTP_USE_EPSV,              CURL_OPT_LONG       },  /* 85 */` |
|       - | 1469 | `	{ CURLOPT_SSLENGINE_DEFAULT,         CURL_OPT_LONG       },  /* 90 */` |
|       - | 1470 | `	{ CURLOPT_DNS_USE_GLOBAL_CACHE,      CURL_OPT_IGNORE     },  /* 91 */` |
|       - | 1471 | `	{ CURLOPT_DNS_CACHE_TIMEOUT,         CURL_OPT_LONG       },  /* 92 */` |
|       - | 1472 | `	{ CURLOPT_COOKIESESSION,             CURL_OPT_LONG       },  /* 96 */` |
|       - | 1473 | `	{ CURLOPT_BUFFERSIZE,                CURL_OPT_LONG       },  /* 98 */` |
|       - | 1474 | `	{ CURLOPT_NOSIGNAL,                  CURL_OPT_LONG       },  /* 99 */` |
|       - | 1475 | `	{ CURLOPT_PROXYTYPE,                 CURL_OPT_LONG       },  /* 101 */` |
|       - | 1476 | `	{ CURLOPT_UNRESTRICTED_AUTH,         CURL_OPT_LONG       },  /* 105 */` |
|       - | 1477 | `	{ CURLOPT_FTP_USE_EPRT,              CURL_OPT_LONG       },  /* 106 */` |
|       - | 1478 | `	{ CURLOPT_HTTPAUTH,                  CURL_OPT_LONG       },  /* 107 */` |
|       - | 1479 | `	{ CURLOPT_FTP_CREATE_MISSING_DIRS,   CURL_OPT_LONG       },  /* 110 */` |
|       - | 1480 | `	{ CURLOPT_PROXYAUTH,                 CURL_OPT_LONG       },  /* 111 */` |
|       - | 1481 | `	{ CURLOPT_FTP_RESPONSE_TIMEOUT,      CURL_OPT_LONG       },  /* 112 */` |
|       - | 1482 | `	{ CURLOPT_SERVER_RESPONSE_TIMEOUT,   CURL_OPT_LONG       },  /* 112 */` |
|       - | 1483 | `	{ CURLOPT_IPRESOLVE,                 CURL_OPT_LONG       },  /* 113 */` |
|       - | 1484 | `	{ CURLOPT_MAXFILESIZE,               CURL_OPT_LONG       },  /* 114 */` |
|       - | 1485 | `	{ CURLOPT_FTP_SSL,                   CURL_OPT_LONG       },  /* 119 */` |
|       - | 1486 | `	{ CURLOPT_USE_SSL,                   CURL_OPT_LONG       },  /* 119 */` |
|       - | 1487 | `	{ CURLOPT_TCP_NODELAY,               CURL_OPT_LONG       },  /* 121 */` |
|       - | 1488 | `	{ CURLOPT_FTPSSLAUTH,                CURL_OPT_LONG       },  /* 129 */` |
|       - | 1489 | `	{ CURLOPT_IGNORE_CONTENT_LENGTH,     CURL_OPT_LONG       },  /* 136 */` |
|       - | 1490 | `	{ CURLOPT_FTP_SKIP_PASV_IP,          CURL_OPT_LONG       },  /* 137 */` |
|       - | 1491 | `	{ CURLOPT_FTP_FILEMETHOD,            CURL_OPT_LONG       },  /* 138 */` |
|       - | 1492 | `	{ CURLOPT_LOCALPORT,                 CURL_OPT_LONG       },  /* 139 */` |
|       - | 1493 | `	{ CURLOPT_LOCALPORTRANGE,            CURL_OPT_LONG       },  /* 140 */` |
|       - | 1494 | `	{ CURLOPT_CONNECT_ONLY,              CURL_OPT_LONG       },  /* 141 */` |
|       - | 1495 | `	{ CURLOPT_SSL_SESSIONID_CACHE,       CURL_OPT_LONG       },  /* 150 */` |
|       - | 1496 | `	{ CURLOPT_SSH_AUTH_TYPES,            CURL_OPT_LONG       },  /* 151 */` |
|       - | 1497 | `	{ CURLOPT_FTP_SSL_CCC,               CURL_OPT_LONG       },  /* 154 */` |
|       - | 1498 | `	{ CURLOPT_TIMEOUT_MS,                CURL_OPT_LONG       },  /* 155 */` |
|       - | 1499 | `	{ CURLOPT_CONNECTTIMEOUT_MS,         CURL_OPT_LONG       },  /* 156 */` |
|       - | 1500 | `	{ CURLOPT_HTTP_TRANSFER_DECODING,    CURL_OPT_LONG       },  /* 157 */` |
|       - | 1501 | `	{ CURLOPT_HTTP_CONTENT_DECODING,     CURL_OPT_LONG       },  /* 158 */` |
|       - | 1502 | `	{ CURLOPT_NEW_FILE_PERMS,            CURL_OPT_LONG       },  /* 159 */` |
|       - | 1503 | `	{ CURLOPT_NEW_DIRECTORY_PERMS,       CURL_OPT_LONG       },  /* 160 */` |
|       - | 1504 | `	{ CURLOPT_POSTREDIR,                 CURL_OPT_LONG       },  /* 161 */` |
|       - | 1505 | `	{ CURLOPT_PROXY_TRANSFER_MODE,       CURL_OPT_LONG       },  /* 166 */` |
|       - | 1506 | `	{ CURLOPT_ADDRESS_SCOPE,             CURL_OPT_LONG       },  /* 171 */` |
|       - | 1507 | `	{ CURLOPT_CERTINFO,                  CURL_OPT_LONG       },  /* 172 */` |
|       - | 1508 | `	{ CURLOPT_TFTP_BLKSIZE,              CURL_OPT_LONG       },  /* 178 */` |
|       - | 1509 | `	{ CURLOPT_SOCKS5_GSSAPI_NEC,         CURL_OPT_LONG       },  /* 180 */` |
|       - | 1510 | `	{ CURLOPT_PROTOCOLS,                 CURL_OPT_LONG       },  /* 181 */` |
|       - | 1511 | `	{ CURLOPT_REDIR_PROTOCOLS,           CURL_OPT_LONG       },  /* 182 */` |
|       - | 1512 | `	{ CURLOPT_FTP_USE_PRET,              CURL_OPT_LONG       },  /* 188 */` |
|       - | 1513 | `	{ CURLOPT_RTSP_REQUEST,              CURL_OPT_LONG       },  /* 189 */` |
|       - | 1514 | `	{ CURLOPT_RTSP_CLIENT_CSEQ,          CURL_OPT_LONG       },  /* 193 */` |
|       - | 1515 | `	{ CURLOPT_RTSP_SERVER_CSEQ,          CURL_OPT_LONG       },  /* 194 */` |
|       - | 1516 | `	{ CURLOPT_WILDCARDMATCH,             CURL_OPT_LONG       },  /* 197 */` |
|       - | 1517 | `	{ CURLOPT_TRANSFER_ENCODING,         CURL_OPT_LONG       },  /* 207 */` |
|       - | 1518 | `	{ CURLOPT_GSSAPI_DELEGATION,         CURL_OPT_LONG       },  /* 210 */` |
|       - | 1519 | `	{ CURLOPT_ACCEPTTIMEOUT_MS,          CURL_OPT_LONG       },  /* 212 */` |
|       - | 1520 | `	{ CURLOPT_TCP_KEEPALIVE,             CURL_OPT_LONG       },  /* 213 */` |
|       - | 1521 | `	{ CURLOPT_TCP_KEEPIDLE,              CURL_OPT_LONG       },  /* 214 */` |
|       - | 1522 | `	{ CURLOPT_TCP_KEEPINTVL,             CURL_OPT_LONG       },  /* 215 */` |
|       - | 1523 | `	{ CURLOPT_SSL_OPTIONS,               CURL_OPT_LONG       },  /* 216 */` |
|       - | 1524 | `	{ CURLOPT_SASL_IR,                   CURL_OPT_LONG       },  /* 218 */` |
|       - | 1525 | `	{ CURLOPT_SSL_ENABLE_NPN,            CURL_OPT_LONG       },  /* 225 */` |
|       - | 1526 | `	{ CURLOPT_SSL_ENABLE_ALPN,           CURL_OPT_LONG       },  /* 226 */` |
|       - | 1527 | `	{ CURLOPT_EXPECT_100_TIMEOUT_MS,     CURL_OPT_LONG       },  /* 227 */` |
|       - | 1528 | `	{ CURLOPT_HEADEROPT,                 CURL_OPT_LONG       },  /* 229 */` |
|       - | 1529 | `	{ CURLOPT_SSL_VERIFYSTATUS,          CURL_OPT_LONG       },  /* 232 */` |
|       - | 1530 | `	{ CURLOPT_SSL_FALSESTART,            CURL_OPT_LONG       },  /* 233 */` |
|       - | 1531 | `	{ CURLOPT_PATH_AS_IS,                CURL_OPT_LONG       },  /* 234 */` |
|       - | 1532 | `	{ CURLOPT_PIPEWAIT,                  CURL_OPT_LONG       },  /* 237 */` |
|       - | 1533 | `	{ CURLOPT_STREAM_WEIGHT,             CURL_OPT_LONG       },  /* 239 */` |
|       - | 1534 | `	{ CURLOPT_TFTP_NO_OPTIONS,           CURL_OPT_LONG       },  /* 242 */` |
|       - | 1535 | `	{ CURLOPT_TCP_FASTOPEN,              CURL_OPT_LONG       },  /* 244 */` |
|       - | 1536 | `	{ CURLOPT_KEEP_SENDING_ON_ERROR,     CURL_OPT_LONG       },  /* 245 */` |
|       - | 1537 | `	{ CURLOPT_PROXY_SSL_VERIFYPEER,      CURL_OPT_LONG       },  /* 248 */` |
|       - | 1538 | `	{ CURLOPT_PROXY_SSL_VERIFYHOST,      CURL_OPT_LONG       },  /* 249 */` |
|       - | 1539 | `	{ CURLOPT_PROXY_SSLVERSION,          CURL_OPT_LONG       },  /* 250 */` |
|       - | 1540 | `	{ CURLOPT_PROXY_SSL_OPTIONS,         CURL_OPT_LONG       },  /* 261 */` |
|       - | 1541 | `	{ CURLOPT_SUPPRESS_CONNECT_HEADERS,  CURL_OPT_LONG       },  /* 265 */` |
|       - | 1542 | `	{ CURLOPT_SOCKS5_AUTH,               CURL_OPT_LONG       },  /* 267 */` |
|       - | 1543 | `	{ CURLOPT_SSH_COMPRESSION,           CURL_OPT_LONG       },  /* 268 */` |
|       - | 1544 | `	{ CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS, CURL_OPT_LONG       },  /* 271 */` |
|       - | 1545 | `	{ CURLOPT_HAPROXYPROTOCOL,           CURL_OPT_LONG       },  /* 274 */` |
|       - | 1546 | `	{ CURLOPT_DNS_SHUFFLE_ADDRESSES,     CURL_OPT_LONG       },  /* 275 */` |
|       - | 1547 | `	{ CURLOPT_DISALLOW_USERNAME_IN_URL,  CURL_OPT_LONG       },  /* 278 */` |
|       - | 1548 | `	{ CURLOPT_UPLOAD_BUFFERSIZE,         CURL_OPT_LONG       },  /* 280 */` |
|       - | 1549 | `	{ CURLOPT_UPKEEP_INTERVAL_MS,        CURL_OPT_LONG       },  /* 281 */` |
|       - | 1550 | `	{ CURLOPT_HTTP09_ALLOWED,            CURL_OPT_LONG       },  /* 285 */` |
|       - | 1551 | `	{ CURLOPT_ALTSVC_CTRL,               CURL_OPT_LONG       },  /* 286 */` |
|       - | 1552 | `	{ CURLOPT_MAXAGE_CONN,               CURL_OPT_LONG       },  /* 288 */` |
|       - | 1553 | `	{ CURLOPT_MAIL_RCPT_ALLLOWFAILS,     CURL_OPT_LONG       },  /* 290 */` |
|       - | 1554 | `	{ CURLOPT_HSTS_CTRL,                 CURL_OPT_LONG       },  /* 299 */` |
|       - | 1555 | `	{ CURLOPT_DOH_SSL_VERIFYPEER,        CURL_OPT_LONG       },  /* 306 */` |
|       - | 1556 | `	{ CURLOPT_DOH_SSL_VERIFYHOST,        CURL_OPT_LONG       },  /* 307 */` |
|       - | 1557 | `	{ CURLOPT_DOH_SSL_VERIFYSTATUS,      CURL_OPT_LONG       },  /* 308 */` |
|       - | 1558 | `	{ CURLOPT_MAXLIFETIME_CONN,          CURL_OPT_LONG       },  /* 314 */` |
|       - | 1559 | `	{ CURLOPT_MIME_OPTIONS,              CURL_OPT_LONG       },  /* 315 */` |
|       - | 1560 | `	{ CURLOPT_WS_OPTIONS,                CURL_OPT_LONG       },  /* 320 */` |
|       - | 1561 | `	{ CURLOPT_CA_CACHE_TIMEOUT,          CURL_OPT_LONG       },  /* 321 */` |
|       - | 1562 | `	{ CURLOPT_QUICK_EXIT,                CURL_OPT_LONG       },  /* 322 */` |
|       - | 1563 | `	{ CURLOPT_FILE,                      CURL_OPT_FILE       },  /* 10001 */` |
|       - | 1564 | `	{ CURLOPT_URL,                       CURL_OPT_STRING     },  /* 10002 */` |
|       - | 1565 | `	{ CURLOPT_PROXY,                     CURL_OPT_STRING     },  /* 10004 */` |
|       - | 1566 | `	{ CURLOPT_USERPWD,                   CURL_OPT_STRING     },  /* 10005 */` |
|       - | 1567 | `	{ CURLOPT_PROXYUSERPWD,              CURL_OPT_STRING     },  /* 10006 */` |
|       - | 1568 | `	{ CURLOPT_RANGE,                     CURL_OPT_STRING     },  /* 10007 */` |
|       - | 1569 | `	{ CURLOPT_INFILE,                    CURL_OPT_FILE       },  /* 10009 */` |
|       - | 1570 | `	{ CURLOPT_READDATA,                  CURL_OPT_FILE       },  /* 10009 */` |
|       - | 1571 | `	{ CURLOPT_POSTFIELDS,                CURL_OPT_POSTFIELDS },  /* 10015 */` |
|       - | 1572 | `	{ CURLOPT_REFERER,                   CURL_OPT_STRING     },  /* 10016 */` |
|       - | 1573 | `	{ CURLOPT_FTPPORT,                   CURL_OPT_STRING     },  /* 10017 */` |
|       - | 1574 | `	{ CURLOPT_USERAGENT,                 CURL_OPT_STRING     },  /* 10018 */` |
|       - | 1575 | `	{ CURLOPT_COOKIE,                    CURL_OPT_STRING     },  /* 10022 */` |
|       - | 1576 | `	{ CURLOPT_HTTPHEADER,                CURL_OPT_SLIST      },  /* 10023 */` |
|       - | 1577 | `	{ CURLOPT_SSLCERT,                   CURL_OPT_STRING     },  /* 10025 */` |
|       - | 1578 | `	{ CURLOPT_KEYPASSWD,                 CURL_OPT_STRING     },  /* 10026 */` |
|       - | 1579 | `	{ CURLOPT_SSLCERTPASSWD,             CURL_OPT_STRING     },  /* 10026 */` |
|       - | 1580 | `	{ CURLOPT_SSLKEYPASSWD,              CURL_OPT_STRING     },  /* 10026 */` |
|       - | 1581 | `	{ CURLOPT_QUOTE,                     CURL_OPT_SLIST      },  /* 10028 */` |
|       - | 1582 | `	{ CURLOPT_WRITEHEADER,               CURL_OPT_FILE       },  /* 10029 */` |
|       - | 1583 | `	{ CURLOPT_COOKIEFILE,                CURL_OPT_STRING     },  /* 10031 */` |
|       - | 1584 | `	{ CURLOPT_CUSTOMREQUEST,             CURL_OPT_STRING     },  /* 10036 */` |
|       - | 1585 | `	{ CURLOPT_STDERR,                    CURL_OPT_FILE       },  /* 10037 */` |
|       - | 1586 | `	{ CURLOPT_POSTQUOTE,                 CURL_OPT_SLIST      },  /* 10039 */` |
|       - | 1587 | `	{ CURLOPT_INTERFACE,                 CURL_OPT_STRING     },  /* 10062 */` |
|       - | 1588 | `	{ CURLOPT_KRB4LEVEL,                 CURL_OPT_STRING     },  /* 10063 */` |
|       - | 1589 | `	{ CURLOPT_KRBLEVEL,                  CURL_OPT_STRING     },  /* 10063 */` |
|       - | 1590 | `	{ CURLOPT_CAINFO,                    CURL_OPT_STRING     },  /* 10065 */` |
|       - | 1591 | `	{ CURLOPT_TELNETOPTIONS,             CURL_OPT_SLIST      },  /* 10070 */` |
|       - | 1592 | `	{ CURLOPT_RANDOM_FILE,               CURL_OPT_STRING     },  /* 10076 */` |
|       - | 1593 | `	{ CURLOPT_EGDSOCKET,                 CURL_OPT_STRING     },  /* 10077 */` |
|       - | 1594 | `	{ CURLOPT_COOKIEJAR,                 CURL_OPT_STRING     },  /* 10082 */` |
|       - | 1595 | `	{ CURLOPT_SSL_CIPHER_LIST,           CURL_OPT_STRING     },  /* 10083 */` |
|       - | 1596 | `	{ CURLOPT_SSLCERTTYPE,               CURL_OPT_STRING     },  /* 10086 */` |
|       - | 1597 | `	{ CURLOPT_SSLKEY,                    CURL_OPT_STRING     },  /* 10087 */` |
|       - | 1598 | `	{ CURLOPT_SSLKEYTYPE,                CURL_OPT_STRING     },  /* 10088 */` |
|       - | 1599 | `	{ CURLOPT_SSLENGINE,                 CURL_OPT_STRING     },  /* 10089 */` |
|       - | 1600 | `	{ CURLOPT_PREQUOTE,                  CURL_OPT_SLIST      },  /* 10093 */` |
|       - | 1601 | `	{ CURLOPT_CAPATH,                    CURL_OPT_STRING     },  /* 10097 */` |
|       - | 1602 | `	{ CURLOPT_SHARE,                     CURL_OPT_SHARE      },  /* 10100 */` |
|       - | 1603 | `	{ CURLOPT_ACCEPT_ENCODING,           CURL_OPT_STRING     },  /* 10102 */` |
|       - | 1604 | `	{ CURLOPT_ENCODING,                  CURL_OPT_STRING     },  /* 10102 */` |
|       - | 1605 | `	{ CURLOPT_PRIVATE,                   CURL_OPT_PRIVATE    },  /* 10103 */` |
|       - | 1606 | `	{ CURLOPT_HTTP200ALIASES,            CURL_OPT_SLIST      },  /* 10104 */` |
|       - | 1607 | `	{ CURLOPT_NETRC_FILE,                CURL_OPT_STRING     },  /* 10118 */` |
|       - | 1608 | `	{ CURLOPT_FTP_ACCOUNT,               CURL_OPT_STRING     },  /* 10134 */` |
|       - | 1609 | `	{ CURLOPT_COOKIELIST,                CURL_OPT_STRING     },  /* 10135 */` |
|       - | 1610 | `	{ CURLOPT_FTP_ALTERNATIVE_TO_USER,   CURL_OPT_STRING     },  /* 10147 */` |
|       - | 1611 | `	{ CURLOPT_SSH_PUBLIC_KEYFILE,        CURL_OPT_STRING     },  /* 10152 */` |
|       - | 1612 | `	{ CURLOPT_SSH_PRIVATE_KEYFILE,       CURL_OPT_STRING     },  /* 10153 */` |
|       - | 1613 | `	{ CURLOPT_SSH_HOST_PUBLIC_KEY_MD5,   CURL_OPT_STRING     },  /* 10162 */` |
|       - | 1614 | `	{ CURLOPT_CRLFILE,                   CURL_OPT_STRING     },  /* 10169 */` |
|       - | 1615 | `	{ CURLOPT_ISSUERCERT,                CURL_OPT_STRING     },  /* 10170 */` |
|       - | 1616 | `	{ CURLOPT_USERNAME,                  CURL_OPT_STRING     },  /* 10173 */` |
|       - | 1617 | `	{ CURLOPT_PASSWORD,                  CURL_OPT_STRING     },  /* 10174 */` |
|       - | 1618 | `	{ CURLOPT_PROXYUSERNAME,             CURL_OPT_STRING     },  /* 10175 */` |
|       - | 1619 | `	{ CURLOPT_PROXYPASSWORD,             CURL_OPT_STRING     },  /* 10176 */` |
|       - | 1620 | `	{ CURLOPT_NOPROXY,                   CURL_OPT_STRING     },  /* 10177 */` |
|       - | 1621 | `	{ CURLOPT_SOCKS5_GSSAPI_SERVICE,     CURL_OPT_STRING     },  /* 10179 */` |
|       - | 1622 | `	{ CURLOPT_SSH_KNOWNHOSTS,            CURL_OPT_STRING     },  /* 10183 */` |
|       - | 1623 | `	{ CURLOPT_MAIL_FROM,                 CURL_OPT_STRING     },  /* 10186 */` |
|       - | 1624 | `	{ CURLOPT_MAIL_RCPT,                 CURL_OPT_SLIST      },  /* 10187 */` |
|       - | 1625 | `	{ CURLOPT_RTSP_SESSION_ID,           CURL_OPT_STRING     },  /* 10190 */` |
|       - | 1626 | `	{ CURLOPT_RTSP_STREAM_URI,           CURL_OPT_STRING     },  /* 10191 */` |
|       - | 1627 | `	{ CURLOPT_RTSP_TRANSPORT,            CURL_OPT_STRING     },  /* 10192 */` |
|       - | 1628 | `	{ CURLOPT_RESOLVE,                   CURL_OPT_SLIST      },  /* 10203 */` |
|       - | 1629 | `	{ CURLOPT_TLSAUTH_USERNAME,          CURL_OPT_STRING     },  /* 10204 */` |
|       - | 1630 | `	{ CURLOPT_TLSAUTH_PASSWORD,          CURL_OPT_STRING     },  /* 10205 */` |
|       - | 1631 | `	{ CURLOPT_TLSAUTH_TYPE,              CURL_OPT_STRING     },  /* 10206 */` |
|       - | 1632 | `	{ CURLOPT_DNS_SERVERS,               CURL_OPT_STRING     },  /* 10211 */` |
|       - | 1633 | `	{ CURLOPT_MAIL_AUTH,                 CURL_OPT_STRING     },  /* 10217 */` |
|       - | 1634 | `	{ CURLOPT_XOAUTH2_BEARER,            CURL_OPT_STRING     },  /* 10220 */` |
|       - | 1635 | `	{ CURLOPT_DNS_INTERFACE,             CURL_OPT_STRING     },  /* 10221 */` |
|       - | 1636 | `	{ CURLOPT_DNS_LOCAL_IP4,             CURL_OPT_STRING     },  /* 10222 */` |
|       - | 1637 | `	{ CURLOPT_DNS_LOCAL_IP6,             CURL_OPT_STRING     },  /* 10223 */` |
|       - | 1638 | `	{ CURLOPT_LOGIN_OPTIONS,             CURL_OPT_STRING     },  /* 10224 */` |
|       - | 1639 | `	{ CURLOPT_PROXYHEADER,               CURL_OPT_SLIST      },  /* 10228 */` |
|       - | 1640 | `	{ CURLOPT_PINNEDPUBLICKEY,           CURL_OPT_STRING     },  /* 10230 */` |
|       - | 1641 | `	{ CURLOPT_UNIX_SOCKET_PATH,          CURL_OPT_STRING     },  /* 10231 */` |
|       - | 1642 | `	{ CURLOPT_PROXY_SERVICE_NAME,        CURL_OPT_STRING     },  /* 10235 */` |
|       - | 1643 | `	{ CURLOPT_SERVICE_NAME,              CURL_OPT_STRING     },  /* 10236 */` |
|       - | 1644 | `	{ CURLOPT_DEFAULT_PROTOCOL,          CURL_OPT_STRING     },  /* 10238 */` |
|       - | 1645 | `	{ CURLOPT_CONNECT_TO,                CURL_OPT_SLIST      },  /* 10243 */` |
|       - | 1646 | `	{ CURLOPT_PROXY_CAINFO,              CURL_OPT_STRING     },  /* 10246 */` |
|       - | 1647 | `	{ CURLOPT_PROXY_CAPATH,              CURL_OPT_STRING     },  /* 10247 */` |
|       - | 1648 | `	{ CURLOPT_PROXY_TLSAUTH_USERNAME,    CURL_OPT_STRING     },  /* 10251 */` |
|       - | 1649 | `	{ CURLOPT_PROXY_TLSAUTH_PASSWORD,    CURL_OPT_STRING     },  /* 10252 */` |
|       - | 1650 | `	{ CURLOPT_PROXY_TLSAUTH_TYPE,        CURL_OPT_STRING     },  /* 10253 */` |
|       - | 1651 | `	{ CURLOPT_PROXY_SSLCERT,             CURL_OPT_STRING     },  /* 10254 */` |
|       - | 1652 | `	{ CURLOPT_PROXY_SSLCERTTYPE,         CURL_OPT_STRING     },  /* 10255 */` |
|       - | 1653 | `	{ CURLOPT_PROXY_SSLKEY,              CURL_OPT_STRING     },  /* 10256 */` |
|       - | 1654 | `	{ CURLOPT_PROXY_SSLKEYTYPE,          CURL_OPT_STRING     },  /* 10257 */` |
|       - | 1655 | `	{ CURLOPT_PROXY_KEYPASSWD,           CURL_OPT_STRING     },  /* 10258 */` |
|       - | 1656 | `	{ CURLOPT_PROXY_SSL_CIPHER_LIST,     CURL_OPT_STRING     },  /* 10259 */` |
|       - | 1657 | `	{ CURLOPT_PROXY_CRLFILE,             CURL_OPT_STRING     },  /* 10260 */` |
|       - | 1658 | `	{ CURLOPT_PRE_PROXY,                 CURL_OPT_STRING     },  /* 10262 */` |
|       - | 1659 | `	{ CURLOPT_PROXY_PINNEDPUBLICKEY,     CURL_OPT_STRING     },  /* 10263 */` |
|       - | 1660 | `	{ CURLOPT_ABSTRACT_UNIX_SOCKET,      CURL_OPT_STRING     },  /* 10264 */` |
|       - | 1661 | `	{ CURLOPT_REQUEST_TARGET,            CURL_OPT_STRING     },  /* 10266 */` |
|       - | 1662 | `	{ CURLOPT_TLS13_CIPHERS,             CURL_OPT_STRING     },  /* 10276 */` |
|       - | 1663 | `	{ CURLOPT_PROXY_TLS13_CIPHERS,       CURL_OPT_STRING     },  /* 10277 */` |
|       - | 1664 | `	{ CURLOPT_DOH_URL,                   CURL_OPT_STRING     },  /* 10279 */` |
|       - | 1665 | `	{ CURLOPT_ALTSVC,                    CURL_OPT_STRING     },  /* 10287 */` |
|       - | 1666 | `	{ CURLOPT_SASL_AUTHZID,              CURL_OPT_STRING     },  /* 10289 */` |
|       - | 1667 | `	{ CURLOPT_PROXY_ISSUERCERT,          CURL_OPT_STRING     },  /* 10296 */` |
|       - | 1668 | `	{ CURLOPT_SSL_EC_CURVES,             CURL_OPT_STRING     },  /* 10298 */` |
|       - | 1669 | `	{ CURLOPT_HSTS,                      CURL_OPT_STRING     },  /* 10300 */` |
|       - | 1670 | `	{ CURLOPT_AWS_SIGV4,                 CURL_OPT_STRING     },  /* 10305 */` |
|       - | 1671 | `	{ CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256, CURL_OPT_STRING     },  /* 10311 */` |
|       - | 1672 | `	{ CURLOPT_PROTOCOLS_STR,             CURL_OPT_STRING     },  /* 10318 */` |
|       - | 1673 | `	{ CURLOPT_REDIR_PROTOCOLS_STR,       CURL_OPT_STRING     },  /* 10319 */` |
|       - | 1674 | `	{ 19913,                             CURL_OPT_RETURN     },  /* 19913 */` |
|       - | 1675 | `	{ 19914,                             CURL_OPT_IGNORE     },  /* 19914 */` |
|       - | 1676 | `	{ CURLOPT_WRITEFUNCTION,             CURL_OPT_CALLBACK   },  /* 20011 */` |
|       - | 1677 | `	{ CURLOPT_READFUNCTION,              CURL_OPT_CALLBACK   },  /* 20012 */` |
|       - | 1678 | `	{ CURLOPT_PROGRESSFUNCTION,          CURL_OPT_CALLBACK   },  /* 20056 */` |
|       - | 1679 | `	{ CURLOPT_HEADERFUNCTION,            CURL_OPT_CALLBACK   },  /* 20079 */` |
|       - | 1680 | `	{ CURLOPT_DEBUGFUNCTION,             CURL_OPT_CALLBACK   },  /* 20094 */` |
|       - | 1681 | `	{ CURLOPT_FNMATCH_FUNCTION,          CURL_OPT_CALLBACK   },  /* 20200 */` |
|       - | 1682 | `	{ CURLOPT_XFERINFOFUNCTION,          CURL_OPT_CALLBACK   },  /* 20219 */` |
|       - | 1683 | `	{ CURLOPT_PREREQFUNCTION,            CURL_OPT_CALLBACK   },  /* 20312 */` |
|       - | 1684 | `	{ CURLOPT_SSH_HOSTKEYFUNCTION,       CURL_OPT_CALLBACK   },  /* 20316 */` |
|       - | 1685 | `	{ CURLOPT_INFILESIZE_LARGE,          CURL_OPT_LONG       },  /* 30115 */` |
|       - | 1686 | `	{ CURLOPT_MAXFILESIZE_LARGE,         CURL_OPT_LONG       },  /* 30117 */` |
|       - | 1687 | `	{ CURLOPT_MAX_SEND_SPEED_LARGE,      CURL_OPT_LONG       },  /* 30145 */` |
|       - | 1688 | `	{ CURLOPT_MAX_RECV_SPEED_LARGE,      CURL_OPT_LONG       },  /* 30146 */` |
|       - | 1689 | `	{ CURLOPT_TIMEVALUE_LARGE,           CURL_OPT_LONG       },  /* 30270 */` |
|       - | 1690 | `	{ CURLOPT_SSLCERT_BLOB,              CURL_OPT_STRING     },  /* 40291 */` |
|       - | 1691 | `	{ CURLOPT_SSLKEY_BLOB,               CURL_OPT_STRING     },  /* 40292 */` |
|       - | 1692 | `	{ CURLOPT_PROXY_SSLCERT_BLOB,        CURL_OPT_STRING     },  /* 40293 */` |
|       - | 1693 | `	{ CURLOPT_PROXY_SSLKEY_BLOB,         CURL_OPT_STRING     },  /* 40294 */` |
|       - | 1694 | `	{ CURLOPT_ISSUERCERT_BLOB,           CURL_OPT_STRING     },  /* 40295 */` |
|       - | 1695 | `	{ CURLOPT_PROXY_ISSUERCERT_BLOB,     CURL_OPT_STRING     },  /* 40297 */` |
|       - | 1696 | `	{ CURLOPT_CAINFO_BLOB,               CURL_OPT_STRING     },  /* 40309 */` |
|       - | 1697 | `	{ CURLOPT_PROXY_CAINFO_BLOB,         CURL_OPT_STRING     },  /* 40310 */` |
|       - | 1698 | `};` |
|       - | 1699 |  |
|       - | 1700 | `/*` |
|       - | 1701 | ` * The option's php NAME, for the diagnostics that print one ("The` |
|       - | 1702 | ` * CURLOPT_HTTPHEADER option must have an array value"). Read out of the` |
|       - | 1703 | ` * constant table rather than repeated, so the two can never disagree.` |
|       - | 1704 | ` */` |
|     126 | 1705 | `static const char * CurlOptName(sxi64 iOpt)` |
|       1 | 1706 | `{` |
|       - | 1707 | `	sxu32 n;` |
|   13605 | 1708 | `	for( n = 0 ; n < SX_ARRAYSIZE(aCurlConst) ; ++n ){` |
|   13604 | 1709 | `		if( aCurlConst[n].iValue == iOpt` |
|    6866 | 1710 | `		 && SyStrncmp(aCurlConst[n].zName,"CURLOPT_",sizeof("CURLOPT_")-1) == 0 ){` |
|     127 | 1711 | `			return aCurlConst[n].zName;` |
|       - | 1712 | `		}` |
|    6740 | 1713 | `	}` |
|     ! 0 | 1714 | `	return "CURLOPT_UNKNOWN";` |
|      64 | 1715 | `}` |
|       - | 1716 |  |
|     838 | 1717 | `static const struct CurlOptDef * CurlOptFind(sxi64 iOpt)` |
|       1 | 1718 | `{` |
|       - | 1719 | `	sxu32 n;` |
|  120719 | 1720 | `	for( n = 0 ; n < SX_ARRAYSIZE(aCurlOpt) ; ++n ){` |
|  120709 | 1721 | `		if( aCurlOpt[n].iOpt == iOpt ){` |
|     829 | 1722 | `			return &aCurlOpt[n];` |
|       - | 1723 | `		}` |
|   59941 | 1724 | `	}` |
|      11 | 1725 | `	return 0;` |
|     420 | 1726 | `}` |
|       - | 1727 |  |
|       - | 1728 | `/* ===== The multipart body ===== */` |
|       - | 1729 |  |
|       - | 1730 | `/*` |
|       - | 1731 | ` * CURLOPT_POSTFIELDS with an ARRAY is a multipart/form-data request, and php` |
|       - | 1732 | ` * builds it out of libcurl's mime API rather than encoding anything itself --` |
|       - | 1733 | ` * which is why the boundary is the LIBRARY's and no test may pin it.` |
|       - | 1734 | ` *` |
|       - | 1735 | ` * php's walk is one level deep and no more. Each entry becomes a part named by` |
|       - | 1736 | ` * its KEY (an integer key spelled in decimal), and a value that is itself an` |
|       - | 1737 | ` * array is walked once more with the OUTER key repeated -- so` |
|       - | 1738 | `` * `['a' => ['x','y']]` is two parts both named "a". A third level has no rule`` |
|       - | 1739 | ` * of its own: the value is stringified, which is php's "Array to string` |
|       - | 1740 | ` * conversion" warning and the five letters "Array".` |
|       - | 1741 | ` *` |
|       - | 1742 | ` * A part's NAME reaches libcurl as a C string and stops at a NUL; its CONTENT` |
|       - | 1743 | ` * does not, because the data calls carry a length. Both are php's answers.` |
|       - | 1744 | ` */` |
|     838 | 1745 | `static void CurlFreeParts(phl_curl *pCurl)` |
|       3 | 1746 | `{` |
|     841 | 1747 | `	phl_curl_part *pPart = pCurl->pParts;` |
|     871 | 1748 | `	while( pPart ){` |
|      30 | 1749 | `		phl_curl_part *pNext = pPart->pNext;` |
|      30 | 1750 | `		if( pPart->pHandle && pPart->pStream ){` |
|      24 | 1751 | `			PH7_StreamCloseHandle(pPart->pStream,pPart->pHandle);` |
|      12 | 1752 | `		}` |
|      30 | 1753 | `		SyMemBackendFree(&pCurl->pVm->sAllocator,pPart);` |
|      30 | 1754 | `		pPart = pNext;` |
|     ! 0 | 1755 | `	}` |
|     841 | 1756 | `	pCurl->pParts = 0;` |
|     841 | 1757 | `}` |
|       - | 1758 | `/*` |
|       - | 1759 | ` * Drop the multipart body a handle carries: the mime first (libcurl reads the` |
|       - | 1760 | ` * parts through it), then the streams those parts were reading, then the array` |
|       - | 1761 | ` * kept for a rebuild.` |
|       - | 1762 | ` */` |
|     778 | 1763 | `static void CurlFreeMime(phl_curl *pCurl)` |
|       3 | 1764 | `{` |
|     781 | 1765 | `	if( pCurl->pMime ){` |
|      58 | 1766 | `		curl_mime_free(pCurl->pMime);` |
|      58 | 1767 | `		pCurl->pMime = 0;` |
|      29 | 1768 | `	}` |
|     781 | 1769 | `	CurlFreeParts(pCurl);` |
|     781 | 1770 | `	if( pCurl->pPostArray ){` |
|      58 | 1771 | `		ph7_release_value(pCurl->pVm,pCurl->pPostArray);` |
|      58 | 1772 | `		pCurl->pPostArray = 0;` |
|      29 | 1773 | `	}` |
|     781 | 1774 | `}` |
|       - | 1775 | `/*` |
|       - | 1776 | ` * The read side of a file part. A part whose stream never opened answers` |
|       - | 1777 | ` * CURL_READFUNC_ABORT, which is php's answer for a missing or unreadable` |
|       - | 1778 | ` * upload: curl_exec fails with CURLE_ABORTED_BY_CALLBACK and says nothing` |
|       - | 1779 | ` * else. A read that FAILS mid-file is php's E_NOTICE naming the size and the` |
|       - | 1780 | ` * errno -- the same sentence hash_file() prints, because it is the same stream` |
|       - | 1781 | ` * layer underneath -- and then the same abort.` |
|       - | 1782 | ` */` |
|      28 | 1783 | `static size_t CurlPartRead(char *zBuf,size_t nSize,size_t nMemb,void *pArg)` |
|     ! 0 | 1784 | `{` |
|      28 | 1785 | `	phl_curl_part *pPart = (phl_curl_part *)pArg;` |
|      28 | 1786 | `	phl_curl *pCurl = pPart->pOwner;` |
|      28 | 1787 | `	ph7_int64 nWant = (ph7_int64)(nSize * nMemb);` |
|       - | 1788 | `	ph7_int64 n;` |
|      28 | 1789 | `	if( pPart->pHandle == 0 \|\| pPart->pStream == 0 \|\| pPart->pStream->xRead == 0 ){` |
|       6 | 1790 | `		if( pPart->bNoPath && pCurl ){` |
|       - | 1791 | `			/* php opens the source again when it reads, so a CURLFile naming` |
|       - | 1792 | `			 * nothing refuses TWICE: once from the setter, and once from here.` |
|       - | 1793 | `			 * Noted only -- the refusal itself is raised by curl_exec, after` |
|       - | 1794 | `			 * the library has unwound, so that it carries curl_exec's frame. */` |
|     ! 0 | 1795 | `			pCurl->bNoPathRead = 1;` |
|     ! 0 | 1796 | `		}` |
|       6 | 1797 | `		return CURL_READFUNC_ABORT;` |
|       - | 1798 | `	}` |
|      22 | 1799 | `	if( nWant < 1 ){` |
|     ! 0 | 1800 | `		return 0;` |
|       - | 1801 | `	}` |
|       - | 1802 | `	/* php reads its streams in 8 KB pieces whatever libcurl asked for, and the` |
|       - | 1803 | `	 * size it names in the failure notice below is that piece. */` |
|      22 | 1804 | `	if( nWant > 8192 ){` |
|      11 | 1805 | `		nWant = 8192;` |
|     ! 0 | 1806 | `	}` |
|      22 | 1807 | `	n = pPart->pStream->xRead(pPart->pHandle,zBuf,nWant);` |
|      22 | 1808 | `	if( n < 0 ){` |
|       - | 1809 | `		/* php's own sentence for a read that fails rather than ends -- the one` |
|       - | 1810 | `		 * that tells a directory apart from an empty file. */` |
|       2 | 1811 | `		if( pCurl && pCurl->pExecCtx ){` |
|       4 | 1812 | `			ph7_context_throw_error_format(pCurl->pExecCtx,PH7_CTX_NOTICE,` |
|       - | 1813 | `				"Read of %d bytes failed with errno=%d %s",` |
|       2 | 1814 | `				(int)nWant,errno,VfsStrerror(errno));` |
|       1 | 1815 | `		}` |
|       2 | 1816 | `		return CURL_READFUNC_ABORT;` |
|       - | 1817 | `	}` |
|      20 | 1818 | `	return (size_t)n;` |
|      14 | 1819 | `}` |
|       2 | 1820 | `static int CurlPartSeek(void *pArg,curl_off_t iOfft,int iOrigin)` |
|     ! 0 | 1821 | `{` |
|       2 | 1822 | `	phl_curl_part *pPart = (phl_curl_part *)pArg;` |
|       - | 1823 | `	int whence;` |
|       2 | 1824 | `	if( pPart->pHandle == 0 \|\| pPart->pStream == 0 \|\| pPart->pStream->xSeek == 0 ){` |
|     ! 0 | 1825 | `		return CURL_SEEKFUNC_CANTSEEK;` |
|       - | 1826 | `	}` |
|       - | 1827 | `	/* The stream layer's own whence spelling: 0 SET, 1 CUR, 2 END. */` |
|       2 | 1828 | `	whence = iOrigin == SEEK_END ? 2 : (iOrigin == SEEK_CUR ? 1 : 0);` |
|       2 | 1829 | `	return pPart->pStream->xSeek(pPart->pHandle,(ph7_int64)iOfft,whence) == PH7_OK` |
|       1 | 1830 | `		? CURL_SEEKFUNC_OK : CURL_SEEKFUNC_CANTSEEK;` |
|       1 | 1831 | `}` |
|       - | 1832 | `/*` |
|       - | 1833 | `` * The size of an open stream, php's way: the `size` field of its own stat.`` |
|       - | 1834 | ` * Answers -1 when the device has no stat at all (a wrapper that only reads),` |
|       - | 1835 | ` * which libcurl reads as "length unknown" and sends chunked.` |
|       - | 1836 | ` */` |
|      24 | 1837 | `static curl_off_t CurlStreamSize(ph7_vm *pVm,const ph7_io_stream *pStream,void *pHandle)` |
|     ! 0 | 1838 | `{` |
|       - | 1839 | `	ph7_value *pArray,*pWorker,*pSize;` |
|      24 | 1840 | `	curl_off_t nOut = -1;` |
|      24 | 1841 | `	if( pStream == 0 \|\| pStream->xStat == 0 ){` |
|     ! 0 | 1842 | `		return -1;` |
|       - | 1843 | `	}` |
|      24 | 1844 | `	pArray = ph7_new_array(pVm);` |
|      24 | 1845 | `	pWorker = ph7_new_scalar(pVm);` |
|      24 | 1846 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|     ! 0 | 1847 | `		if( pArray ){ ph7_release_value(pVm,pArray); }` |
|     ! 0 | 1848 | `		if( pWorker ){ ph7_release_value(pVm,pWorker); }` |
|     ! 0 | 1849 | `		return -1;` |
|       - | 1850 | `	}` |
|      24 | 1851 | `	if( pStream->xStat(pHandle,pArray,pWorker) == PH7_OK ){` |
|      24 | 1852 | `		pSize = ph7_array_fetch(pArray,"size",sizeof("size")-1);` |
|      24 | 1853 | `		if( pSize ){` |
|      24 | 1854 | `			ph7_int64 n = ph7_value_to_int64(pSize);` |
|      24 | 1855 | `			if( n >= 0 ){` |
|      24 | 1856 | `				nOut = (curl_off_t)n;` |
|      12 | 1857 | `			}` |
|      12 | 1858 | `		}` |
|      12 | 1859 | `	}` |
|      24 | 1860 | `	ph7_release_value(pVm,pArray);` |
|      24 | 1861 | `	ph7_release_value(pVm,pWorker);` |
|      24 | 1862 | `	return nOut;` |
|      12 | 1863 | `}` |
|       - | 1864 | `/*` |
|       - | 1865 | ` * Open one CURLFile's name and hand libcurl a part that reads it.` |
|       - | 1866 | ` *` |
|       - | 1867 | ` * The open happens HERE, at setopt time, which is php's timing and is visible` |
|       - | 1868 | ` * from a script: a file unlinked between setopt and exec still uploads, and one` |
|       - | 1869 | ` * TRUNCATED between them sends the size it had (libcurl was told the length up` |
|       - | 1870 | ` * front, so the transfer then stalls -- php's answer too).` |
|       - | 1871 | ` */` |
|      30 | 1872 | `static phl_curl_part * CurlPartOpen(phl_curl *pCurl,const char *zPath,int nPath)` |
|     ! 0 | 1873 | `{` |
|      30 | 1874 | `	ph7_vm *pVm = pCurl->pVm;` |
|       - | 1875 | `	phl_curl_part *pPart;` |
|       - | 1876 | `	const ph7_io_stream *pStream;` |
|      30 | 1877 | `	pPart = (phl_curl_part *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curl_part));` |
|      30 | 1878 | `	if( pPart == 0 ){` |
|     ! 0 | 1879 | `		return 0;` |
|       - | 1880 | `	}` |
|      30 | 1881 | `	SyZero(pPart,sizeof(phl_curl_part));` |
|      30 | 1882 | `	pPart->nSize = -1;` |
|      30 | 1883 | `	pPart->pOwner = pCurl;` |
|      30 | 1884 | `	pPart->bNoPath = nPath < 0;` |
|       - | 1885 | `	/* nPath < 0 is the caller saying "there is nothing to open": the part` |
|       - | 1886 | `	 * exists so that the transfer aborts, which is what a refused upload does` |
|       - | 1887 | `	 * in php too. */` |
|      30 | 1888 | `	pStream = nPath < 0 ? 0 : PH7_VmGetStreamDevice(pVm,&zPath,nPath);` |
|      30 | 1889 | `	if( pStream && pStream->xOpen && pStream->xRead ){` |
|      30 | 1890 | `		pPart->pStream = pStream;` |
|       - | 1891 | `		/* Silent on failure: php reports a missing upload at exec, through the` |
|       - | 1892 | `		 * abort the read callback answers, and never at the setter. */` |
|      30 | 1893 | `		pPart->pHandle = PH7_StreamOpenHandle(pVm,pStream,zPath,PH7_IO_OPEN_RDONLY,` |
|       - | 1894 | `			FALSE,0,FALSE,0,0);` |
|      15 | 1895 | `	}` |
|      30 | 1896 | `	if( pPart->pHandle ){` |
|       - | 1897 | `		/*` |
|       - | 1898 | `		 * The declared LENGTH, which is what keeps the request` |
|       - | 1899 | `		 * Content-Length'd rather than chunked, and php reads it from the` |
|       - | 1900 | `		 * STAT of the open handle -- not by seeking to the end, which is a` |
|       - | 1901 | `		 * different number for a directory: lseek(SEEK_END) on one answers the` |
|       - | 1902 | `		 * filesystem's maximum offset (INT64_MAX on tmpfs) where fstat answers` |
|       - | 1903 | `		 * its real size. Handing libcurl the first of those makes an upload` |
|       - | 1904 | `		 * that php refuses succeed with an empty body.` |
|       - | 1905 | `		 */` |
|      24 | 1906 | `		pPart->nSize = CurlStreamSize(pVm,pStream,pPart->pHandle);` |
|      24 | 1907 | `		if( pPart->nSize < 0 && pStream->xSeek && pStream->xTell ){` |
|       - | 1908 | `			/* A wrapper with no stat of its own -- php://temp, data:// --` |
|       - | 1909 | `			 * still knows where its end is, and php declares a length for` |
|       - | 1910 | `			 * those too. Only a device that can do neither goes out chunked. */` |
|     ! 0 | 1911 | `			if( pStream->xSeek(pPart->pHandle,0,2/*SEEK_END*/) == PH7_OK ){` |
|     ! 0 | 1912 | `				ph7_int64 nEnd = pStream->xTell(pPart->pHandle);` |
|     ! 0 | 1913 | `				if( pStream->xSeek(pPart->pHandle,0,0/*SEEK_SET*/) == PH7_OK && nEnd >= 0 ){` |
|     ! 0 | 1914 | `					pPart->nSize = (curl_off_t)nEnd;` |
|     ! 0 | 1915 | `				}` |
|     ! 0 | 1916 | `			}` |
|     ! 0 | 1917 | `		}` |
|      12 | 1918 | `	}` |
|      30 | 1919 | `	pPart->pNext = pCurl->pParts;` |
|      30 | 1920 | `	pCurl->pParts = pPart;` |
|      30 | 1921 | `	return pPart;` |
|      15 | 1922 | `}` |
|       - | 1923 |  |
|       - | 1924 | `/* ===== CURLFile and CURLStringFile ===== */` |
|       - | 1925 |  |
|       - | 1926 | `/*` |
|       - | 1927 | ` * The two upload boxes, and the only classes in this extension a script may` |
|       - | 1928 | ` * construct. Neither is FINAL and neither declares a private constructor, so a` |
|       - | 1929 | ` * subclass is ordinary php -- and CURLFile is the one class here that is not` |
|       - | 1930 | ` * serializable while CURLStringFile IS, which is php's split and not a rule:` |
|       - | 1931 | ` * the file box names something on disk that another process may not have,` |
|       - | 1932 | ` * the string box carries its own bytes.` |
|       - | 1933 | ` *` |
|       - | 1934 | ` * Their properties are ORDINARY public typed slots, readable and writable from` |
|       - | 1935 | ` * php, and the getters/setters are the php-4-era spelling of the same three.` |
|       - | 1936 | `` * That is why nothing here is hidden and nothing is validated after `new`: the`` |
|       - | 1937 | ` * mime builder reads whatever the slots hold at the moment setopt is called.` |
|       - | 1938 | ` */` |
|       - | 1939 | `#define CURLFILE_NAME     "name"` |
|       - | 1940 | `#define CURLFILE_MIME     "mime"` |
|       - | 1941 | `#define CURLFILE_POSTNAME "postname"` |
|       - | 1942 | `#define CURLSTR_DATA      "data"` |
|       - | 1943 |  |
|       - | 1944 | `/*` |
|       - | 1945 | ` * php screens the FILENAME for a NUL byte and nothing else -- not the mime` |
|       - | 1946 | ` * type, not the posted name, both of which reach libcurl through an API that` |
|       - | 1947 | ` * takes a C string and will simply stop at the byte. Measured, not assumed.` |
|       - | 1948 | ` */` |
|      46 | 1949 | `static int CurlFileFill(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,` |
|       - | 1950 | `	ph7_value **apArg,const char *zWho)` |
|       1 | 1951 | `{` |
|      47 | 1952 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1953 | `	const char *zName;` |
|      47 | 1954 | `	int nName = 0;` |
|      47 | 1955 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|     ! 0 | 1956 | `		return PH7_OK;` |
|       - | 1957 | `	}` |
|      47 | 1958 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|      47 | 1959 | `	if( SyByteFind(zName,(sxu32)nName,0,0) == SXRET_OK ){` |
|      10 | 1960 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       3 | 1961 | `			"%s: Argument #1 ($filename) must not contain any null bytes",zWho);` |
|       - | 1962 | `	}` |
|      41 | 1963 | `	PH7_NativeSetAttrStr(pVm,pThis,CURLFILE_NAME,zName,nName);` |
|      41 | 1964 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      11 | 1965 | `		int nMime = 0;` |
|      11 | 1966 | `		const char *zMime = ph7_value_to_string(apArg[1],&nMime);` |
|      11 | 1967 | `		PH7_NativeSetAttrStr(pVm,pThis,CURLFILE_MIME,zMime,nMime);` |
|       5 | 1968 | `	}` |
|      41 | 1969 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      11 | 1970 | `		int nPost = 0;` |
|      11 | 1971 | `		const char *zPost = ph7_value_to_string(apArg[2],&nPost);` |
|      11 | 1972 | `		PH7_NativeSetAttrStr(pVm,pThis,CURLFILE_POSTNAME,zPost,nPost);` |
|       5 | 1973 | `	}` |
|      41 | 1974 | `	return PH7_OK;` |
|      24 | 1975 | `}` |
|      42 | 1976 | `static int vm_builtin_CURLFile_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1977 | `{` |
|       - | 1978 | `	/* php names the DECLARING class in the refusal, so a subclass of CURLFile` |
|       - | 1979 | ``	 * still reports `CURLFile::__construct()`. */`` |
|      43 | 1980 | `	return CurlFileFill(pCtx,PH7_ContextThis(pCtx),nArg,apArg,"CURLFile::__construct()");` |
|       1 | 1981 | `}` |
|       - | 1982 | `/* The three getters, each a slot read; php's return types are TENTATIVE. */` |
|      12 | 1983 | `static int CurlFileGet(ph7_context *pCtx,const char *zProp)` |
|       1 | 1984 | `{` |
|      13 | 1985 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      13 | 1986 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zProp) : 0;` |
|      13 | 1987 | `	if( pVal ){` |
|      13 | 1988 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 1989 | `	}else{` |
|     ! 0 | 1990 | `		ph7_result_null(pCtx);` |
|       - | 1991 | `	}` |
|      13 | 1992 | `	return PH7_OK;` |
|       1 | 1993 | `}` |
|       4 | 1994 | `static int vm_builtin_CURLFile_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1995 | `{` |
|       2 | 1996 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 | 1997 | `	return CurlFileGet(pCtx,CURLFILE_NAME);` |
|       1 | 1998 | `}` |
|       4 | 1999 | `static int vm_builtin_CURLFile_getMimeType(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2000 | `{` |
|       2 | 2001 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 | 2002 | `	return CurlFileGet(pCtx,CURLFILE_MIME);` |
|       1 | 2003 | `}` |
|       4 | 2004 | `static int vm_builtin_CURLFile_getPostFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2005 | `{` |
|       2 | 2006 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 | 2007 | `	return CurlFileGet(pCtx,CURLFILE_POSTNAME);` |
|       1 | 2008 | `}` |
|       4 | 2009 | `static int CurlFileSet(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zProp)` |
|       1 | 2010 | `{` |
|       5 | 2011 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       5 | 2012 | `	if( pThis && nArg > 0 ){` |
|       5 | 2013 | `		int nVal = 0;` |
|       5 | 2014 | `		const char *zVal = ph7_value_to_string(apArg[0],&nVal);` |
|       5 | 2015 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,zProp,zVal,nVal);` |
|       2 | 2016 | `	}` |
|       5 | 2017 | `	return PH7_OK;` |
|       1 | 2018 | `}` |
|       2 | 2019 | `static int vm_builtin_CURLFile_setMimeType(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2020 | `{` |
|       3 | 2021 | `	return CurlFileSet(pCtx,nArg,apArg,CURLFILE_MIME);` |
|       1 | 2022 | `}` |
|       2 | 2023 | `static int vm_builtin_CURLFile_setPostFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2024 | `{` |
|       3 | 2025 | `	return CurlFileSet(pCtx,nArg,apArg,CURLFILE_POSTNAME);` |
|       1 | 2026 | `}` |
|      12 | 2027 | `static int vm_builtin_CURLStringFile_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2028 | `{` |
|      13 | 2029 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      13 | 2030 | `	ph7_vm *pVm = pCtx->pVm;` |
|      13 | 2031 | `	int n = 0;` |
|       - | 2032 | `	const char *z;` |
|      13 | 2033 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|     ! 0 | 2034 | `		return PH7_OK;` |
|       - | 2035 | `	}` |
|       - | 2036 | `	/* No NUL screen anywhere here: the DATA is length-carrying by definition` |
|       - | 2037 | `	 * and php screens neither of the other two. */` |
|      13 | 2038 | `	z = ph7_value_to_string(apArg[0],&n);` |
|      13 | 2039 | `	PH7_NativeSetAttrStr(pVm,pThis,CURLSTR_DATA,z,n);` |
|      13 | 2040 | `	z = ph7_value_to_string(apArg[1],&n);` |
|      13 | 2041 | `	PH7_NativeSetAttrStr(pVm,pThis,CURLFILE_POSTNAME,z,n);` |
|       - | 2042 | `	/* The third slot is always written, default included: php's signature` |
|       - | 2043 | ``	 * fills it, and a CURLStringFile whose `mime` stayed UNINITIALIZED reads`` |
|       - | 2044 | `	 * back as an error rather than as php's octet-stream. */` |
|      13 | 2045 | `	if( nArg > 2 ){` |
|       5 | 2046 | `		z = ph7_value_to_string(apArg[2],&n);` |
|       3 | 2047 | `	}else{` |
|       9 | 2048 | `		z = "application/octet-stream";` |
|       9 | 2049 | `		n = (int)sizeof("application/octet-stream") - 1;` |
|       - | 2050 | `	}` |
|      13 | 2051 | `	PH7_NativeSetAttrStr(pVm,pThis,CURLFILE_MIME,z,n);` |
|      13 | 2052 | `	return PH7_OK;` |
|       7 | 2053 | `}` |
|       - | 2054 | `/* CURLFile curl_file_create(string $filename, ?string $mime_type = null, ?string $posted_filename = null) */` |
|       4 | 2055 | `static int vm_builtin_curl_file_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2056 | `{` |
|       5 | 2057 | `	ph7_class_instance *pThis = PH7_CurlNewInstance(pCtx->pVm,"CURLFile",sizeof("CURLFile")-1);` |
|       - | 2058 | `	int rc;` |
|       5 | 2059 | `	if( pThis == 0 ){` |
|     ! 0 | 2060 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2061 | `		return PH7_OK;` |
|       - | 2062 | `	}` |
|       - | 2063 | `	/* The same three writes the constructor makes, and the same refusal --` |
|       - | 2064 | `	 * named for THIS function, which is how php words it here. */` |
|       5 | 2065 | `	rc = CurlFileFill(pCtx,pThis,nArg,apArg,"curl_file_create()");` |
|       5 | 2066 | `	if( rc != PH7_OK ){` |
|       3 | 2067 | `		PH7_ClassInstanceUnref(pThis);` |
|       3 | 2068 | `		return rc;` |
|       - | 2069 | `	}` |
|       3 | 2070 | `	PH7_NativeResultObject(pCtx,pThis);` |
|       3 | 2071 | `	return PH7_OK;` |
|       3 | 2072 | `}` |
|       - | 2073 |  |
|       - | 2074 | `/* ===== The multipart walk ===== */` |
|       - | 2075 |  |
|       - | 2076 | `struct CurlMimeBuild {` |
|       - | 2077 | `	ph7_context *pCtx;` |
|       - | 2078 | `	phl_curl *pCurl;` |
|       - | 2079 | `	curl_mime *pMime;` |
|       - | 2080 | `	ph7_class *pFileClass;    /* CURLFile, or 0 if it somehow is not mounted */` |
|       - | 2081 | `	ph7_class *pStrFileClass; /* CURLStringFile */` |
|       - | 2082 | `	const char *zName;        /* the OUTER key, while a nested array is walked */` |
|       - | 2083 | `	int bNested;              /* inside the one level of nesting php allows */` |
|       - | 2084 | `	int bFailed;` |
|       - | 2085 | `	sxi32 rcThrow;            /* the first throw a cast raised, or PH7_OK */` |
|       - | 2086 | `};` |
|       - | 2087 | `/*` |
|       - | 2088 | ` * libcurl's mime API takes C STRINGS, and a php string is a pointer and a` |
|       - | 2089 | ` * length that need not be terminated -- a native slot read hands back the` |
|       - | 2090 | ` * bytes of a blob whose tail is whatever the last value left there. So every` |
|       - | 2091 | ` * name, filename and type handed to the library is copied through here first.` |
|       - | 2092 | ` * (Skipping it is not a subtle bug: the second CURLFile in a script uploads` |
|       - | 2093 | ` * under the first one's filename.)` |
|       - | 2094 | ` */` |
|     172 | 2095 | `static char * CurlCStr(ph7_vm *pVm,const char *z,int n)` |
|     ! 0 | 2096 | `{` |
|       - | 2097 | `	char *zOut;` |
|     172 | 2098 | `	if( n < 0 ){` |
|     ! 0 | 2099 | `		n = 0;` |
|     ! 0 | 2100 | `	}` |
|     172 | 2101 | `	zOut = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)n + 1);` |
|     172 | 2102 | `	if( zOut == 0 ){` |
|     ! 0 | 2103 | `		return 0;` |
|       - | 2104 | `	}` |
|     172 | 2105 | `	if( n > 0 ){` |
|     116 | 2106 | `		SyMemcpy(z,zOut,(sxu32)n);` |
|      58 | 2107 | `	}` |
|     172 | 2108 | `	zOut[n] = 0;` |
|     172 | 2109 | `	return zOut;` |
|      86 | 2110 | `}` |
|     178 | 2111 | `static void CurlCStrFree(ph7_vm *pVm,char *z)` |
|     ! 0 | 2112 | `{` |
|     178 | 2113 | `	if( z ){` |
|     172 | 2114 | `		SyMemBackendFree(&pVm->sAllocator,z);` |
|      86 | 2115 | `	}` |
|     178 | 2116 | `}` |
|       - | 2117 | `/* A slot of one of the upload boxes, as a C string the caller frees. */` |
|      98 | 2118 | `static char * CurlAttrCStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zProp,int *pnOut)` |
|     ! 0 | 2119 | `{` |
|      98 | 2120 | `	const char *z = 0;` |
|      98 | 2121 | `	int n = 0;` |
|      98 | 2122 | `	PH7_NativeAttrStr(pObj,zProp,&z,&n);` |
|      98 | 2123 | `	if( pnOut ){` |
|      98 | 2124 | `		*pnOut = n;` |
|      49 | 2125 | `	}` |
|      98 | 2126 | `	return CurlCStr(pVm,z,n);` |
|     ! 0 | 2127 | `}` |
|       - | 2128 | `/* The key of a part, in php's spelling: a string key as written, an integer` |
|       - | 2129 | ` * key in decimal. */` |
|      74 | 2130 | `static char * CurlMimeKey(ph7_vm *pVm,ph7_value *pKey)` |
|     ! 0 | 2131 | `{` |
|       - | 2132 | `	char zBuf[32];` |
|      74 | 2133 | `	int nKey = 0;` |
|       - | 2134 | `	const char *zKey;` |
|      74 | 2135 | `	if( ph7_value_is_string(pKey) ){` |
|      68 | 2136 | `		zKey = ph7_value_to_string(pKey,&nKey);` |
|      68 | 2137 | `		return CurlCStr(pVm,zKey,nKey);` |
|       - | 2138 | `	}` |
|       6 | 2139 | `	nKey = (int)SyBufferFormat(zBuf,sizeof(zBuf),"%qd",ph7_value_to_int64(pKey));` |
|       6 | 2140 | `	return CurlCStr(pVm,zBuf,nKey);` |
|      37 | 2141 | `}` |
|       - | 2142 | `/* One ordinary field: the value stringified, the name as given. */` |
|      42 | 2143 | `static void CurlMimeSimple(struct CurlMimeBuild *pB,const char *zName,ph7_value *pVal)` |
|     ! 0 | 2144 | `{` |
|       - | 2145 | `	curl_mimepart *pPart;` |
|      42 | 2146 | `	const char *zVal = "";` |
|      42 | 2147 | `	int nVal = 0;` |
|       - | 2148 | `	sxi32 rcSv;` |
|      42 | 2149 | `	rcSv = PH7_ValueToStringUV(pB->pCtx,pVal,&zVal,&nVal);` |
|      42 | 2150 | `	if( rcSv != SXRET_OK ){` |
|       - | 2151 | `		/* php's cast throws and still answers "" -- and its loop keeps` |
|       - | 2152 | `		 * walking, so the part is added empty and the entries after it are` |
|       - | 2153 | `		 * added too. */` |
|     ! 0 | 2154 | `		if( pB->rcThrow == PH7_OK ){` |
|     ! 0 | 2155 | `			pB->rcThrow = rcSv;` |
|     ! 0 | 2156 | `		}` |
|     ! 0 | 2157 | `		zVal = "";` |
|     ! 0 | 2158 | `		nVal = 0;` |
|     ! 0 | 2159 | `	}` |
|      42 | 2160 | `	pPart = curl_mime_addpart(pB->pMime);` |
|      42 | 2161 | `	if( pPart == 0 ){` |
|     ! 0 | 2162 | `		pB->bFailed = 1;` |
|     ! 0 | 2163 | `		return;` |
|       - | 2164 | `	}` |
|       - | 2165 | `	/* curl_mime_name takes a C string, so a NUL in the key truncates it --` |
|       - | 2166 | `	 * php's answer, because php calls the same function. */` |
|      42 | 2167 | `	if( curl_mime_name(pPart,zName) != CURLE_OK` |
|      42 | 2168 | `	 \|\| curl_mime_data(pPart,zVal,(size_t)nVal) != CURLE_OK ){` |
|     ! 0 | 2169 | `		pB->bFailed = 1;` |
|     ! 0 | 2170 | `	}` |
|      21 | 2171 | `}` |
|       - | 2172 | `/* A CURLFile: the stream part, plus php's two optional overrides. */` |
|      30 | 2173 | `static void CurlMimeFile(struct CurlMimeBuild *pB,const char *zName,ph7_class_instance *pObj)` |
|     ! 0 | 2174 | `{` |
|      30 | 2175 | `	ph7_vm *pVm = pB->pCurl->pVm;` |
|       - | 2176 | `	curl_mimepart *pPart;` |
|       - | 2177 | `	phl_curl_part *pSrc;` |
|       - | 2178 | `	char *zPath,*zMime,*zPost;` |
|      30 | 2179 | `	int nPath = 0,nMime = 0,nPost = 0;` |
|      30 | 2180 | `	zPath = CurlAttrCStr(pVm,pObj,CURLFILE_NAME,&nPath);` |
|      30 | 2181 | `	zMime = CurlAttrCStr(pVm,pObj,CURLFILE_MIME,&nMime);` |
|      30 | 2182 | `	zPost = CurlAttrCStr(pVm,pObj,CURLFILE_POSTNAME,&nPost);` |
|      30 | 2183 | `	if( zPath == 0 \|\| zMime == 0 \|\| zPost == 0 ){` |
|     ! 0 | 2184 | `		pB->bFailed = 1;` |
|     ! 0 | 2185 | `		goto done;` |
|       - | 2186 | `	}` |
|      30 | 2187 | `	if( nPath < 1 ){` |
|       - | 2188 | `		/* php's stream layer refuses an empty path with this ValueError, and` |
|       - | 2189 | `		 * the refusal reaches the script from the SETTER. PHL's own open` |
|       - | 2190 | `		 * answers a warning and false instead, so the sentence is spelled` |
|       - | 2191 | `		 * here rather than left to differ. The part is still added, with` |
|       - | 2192 | `		 * nothing to read: a caught refusal must not leave a handle that then` |
|       - | 2193 | `		 * POSTs an empty body as though the upload had worked. */` |
|     ! 0 | 2194 | `		if( pB->rcThrow == PH7_OK ){` |
|     ! 0 | 2195 | `			pB->rcThrow = PH7_VmThrowException(pB->pCtx,"ValueError",` |
|       - | 2196 | `				"Path must not be empty");` |
|     ! 0 | 2197 | `		}` |
|     ! 0 | 2198 | `	}` |
|      30 | 2199 | `	pSrc = CurlPartOpen(pB->pCurl,zPath,nPath > 0 ? nPath : -1);` |
|      30 | 2200 | `	if( pSrc == 0 ){` |
|     ! 0 | 2201 | `		pB->bFailed = 1;` |
|     ! 0 | 2202 | `		goto done;` |
|       - | 2203 | `	}` |
|      30 | 2204 | `	pPart = curl_mime_addpart(pB->pMime);` |
|      30 | 2205 | `	if( pPart == 0 ){` |
|     ! 0 | 2206 | `		pB->bFailed = 1;` |
|     ! 0 | 2207 | `		goto done;` |
|       - | 2208 | `	}` |
|       - | 2209 | `	/*` |
|       - | 2210 | `	 * No FREE callback: curl_easy_duphandle copies a callback part by copying` |
|       - | 2211 | `	 * its argument, so a duplicate that freed on teardown would tear down the` |
|       - | 2212 | `	 * SOURCE's stream. The handle owns the record and frees it itself.` |
|       - | 2213 | `	 *` |
|       - | 2214 | `	 * The TYPE is always stated, php's default included: libcurl guesses one` |
|       - | 2215 | `	 * for a part it opened itself and states nothing for a callback part, so` |
|       - | 2216 | ``	 * leaving it out drops the `Content-Type: application/octet-stream` line`` |
|       - | 2217 | `	 * php sends for a CURLFile with no mime type of its own.` |
|       - | 2218 | `	 */` |
|      30 | 2219 | `	if( curl_mime_name(pPart,zName) != CURLE_OK` |
|      30 | 2220 | `	 \|\| curl_mime_data_cb(pPart,pSrc->nSize,CurlPartRead,CurlPartSeek,0,pSrc) != CURLE_OK` |
|      30 | 2221 | `	 \|\| curl_mime_filename(pPart,nPost > 0 ? zPost : zPath) != CURLE_OK` |
|      30 | 2222 | `	 \|\| curl_mime_type(pPart,nMime > 0 ? zMime : "application/octet-stream") != CURLE_OK ){` |
|     ! 0 | 2223 | `		pB->bFailed = 1;` |
|     ! 0 | 2224 | `	}` |
|      15 | 2225 | `done:` |
|      30 | 2226 | `	CurlCStrFree(pVm,zPath);` |
|      30 | 2227 | `	CurlCStrFree(pVm,zMime);` |
|      30 | 2228 | `	CurlCStrFree(pVm,zPost);` |
|      30 | 2229 | `}` |
|       - | 2230 | `/* A CURLStringFile: the same part shape with the bytes in hand. */` |
|       4 | 2231 | `static void CurlMimeStringFile(struct CurlMimeBuild *pB,const char *zName,ph7_class_instance *pObj)` |
|     ! 0 | 2232 | `{` |
|       4 | 2233 | `	ph7_vm *pVm = pB->pCurl->pVm;` |
|       - | 2234 | `	curl_mimepart *pPart;` |
|       4 | 2235 | `	const char *zData = 0;` |
|       - | 2236 | `	char *zMime,*zPost;` |
|       4 | 2237 | `	int nData = 0,nMime = 0,nPost = 0;` |
|       - | 2238 | `	/* The DATA is the one string that is not a C string: curl_mime_data takes` |
|       - | 2239 | `	 * a length, so a NUL inside it survives -- php's answer. */` |
|       4 | 2240 | `	PH7_NativeAttrStr(pObj,CURLSTR_DATA,&zData,&nData);` |
|       4 | 2241 | `	zMime = CurlAttrCStr(pVm,pObj,CURLFILE_MIME,&nMime);` |
|       4 | 2242 | `	zPost = CurlAttrCStr(pVm,pObj,CURLFILE_POSTNAME,&nPost);` |
|       4 | 2243 | `	if( zMime == 0 \|\| zPost == 0 ){` |
|     ! 0 | 2244 | `		pB->bFailed = 1;` |
|     ! 0 | 2245 | `		goto done;` |
|       - | 2246 | `	}` |
|       4 | 2247 | `	pPart = curl_mime_addpart(pB->pMime);` |
|       4 | 2248 | `	if( pPart == 0 ){` |
|     ! 0 | 2249 | `		pB->bFailed = 1;` |
|     ! 0 | 2250 | `		goto done;` |
|       - | 2251 | `	}` |
|       4 | 2252 | `	if( curl_mime_name(pPart,zName) != CURLE_OK` |
|       4 | 2253 | `	 \|\| curl_mime_data(pPart,nData > 0 ? zData : "",(size_t)(nData > 0 ? nData : 0)) != CURLE_OK` |
|       4 | 2254 | `	 \|\| curl_mime_filename(pPart,zPost) != CURLE_OK` |
|       4 | 2255 | `	 \|\| curl_mime_type(pPart,nMime > 0 ? zMime : "application/octet-stream") != CURLE_OK ){` |
|     ! 0 | 2256 | `		pB->bFailed = 1;` |
|     ! 0 | 2257 | `	}` |
|       2 | 2258 | `done:` |
|       4 | 2259 | `	CurlCStrFree(pVm,zMime);` |
|       4 | 2260 | `	CurlCStrFree(pVm,zPost);` |
|       4 | 2261 | `}` |
|      80 | 2262 | `static int CurlMimeWalk(ph7_value *pKey,ph7_value *pVal,void *pUser)` |
|     ! 0 | 2263 | `{` |
|      80 | 2264 | `	struct CurlMimeBuild *pB = (struct CurlMimeBuild *)pUser;` |
|      80 | 2265 | `	ph7_vm *pVm = pB->pCurl->pVm;` |
|      80 | 2266 | `	char *zOwn = 0;` |
|       - | 2267 | `	const char *zName;` |
|      80 | 2268 | `	if( pB->bFailed ){` |
|     ! 0 | 2269 | `		return PH7_ABORT;` |
|       - | 2270 | `	}` |
|      80 | 2271 | `	if( pB->bNested ){` |
|       - | 2272 | `		/* Inside a nested array the OUTER key names every part, and the key` |
|       - | 2273 | `		 * here is thrown away -- php's rule, and the reason two values under` |
|       - | 2274 | `		 * one key answer two parts of the same name. */` |
|       6 | 2275 | `		zName = pB->zName;` |
|       3 | 2276 | `	}else{` |
|      74 | 2277 | `		zOwn = CurlMimeKey(pVm,pKey);` |
|      74 | 2278 | `		if( zOwn == 0 ){` |
|     ! 0 | 2279 | `			pB->bFailed = 1;` |
|     ! 0 | 2280 | `			return PH7_ABORT;` |
|       - | 2281 | `		}` |
|      74 | 2282 | `		zName = zOwn;` |
|      74 | 2283 | `		if( ph7_value_is_array(pVal) ){` |
|       - | 2284 | `			/* One level, and one only: a third would recurse, and php's does` |
|       - | 2285 | `			 * not -- it stringifies, warning and all. */` |
|       4 | 2286 | `			pB->zName = zOwn;` |
|       4 | 2287 | `			pB->bNested = 1;` |
|       4 | 2288 | `			ph7_array_walk(pVal,CurlMimeWalk,pB);` |
|       4 | 2289 | `			pB->bNested = 0;` |
|       4 | 2290 | `			pB->zName = 0;` |
|       4 | 2291 | `			CurlCStrFree(pVm,zOwn);` |
|       4 | 2292 | `			return pB->bFailed ? PH7_ABORT : PH7_OK;` |
|       - | 2293 | `		}` |
|       - | 2294 | `	}` |
|      76 | 2295 | `	if( ph7_value_is_object(pVal) ){` |
|      36 | 2296 | `		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|      36 | 2297 | `		if( pObj && pB->pFileClass && PH7_VmInstanceOf(pObj->pClass,pB->pFileClass) ){` |
|      30 | 2298 | `			CurlMimeFile(pB,zName,pObj);` |
|      30 | 2299 | `			goto done;` |
|       - | 2300 | `		}` |
|       6 | 2301 | `		if( pObj && pB->pStrFileClass && PH7_VmInstanceOf(pObj->pClass,pB->pStrFileClass) ){` |
|       4 | 2302 | `			CurlMimeStringFile(pB,zName,pObj);` |
|       4 | 2303 | `			goto done;` |
|       - | 2304 | `		}` |
|       1 | 2305 | `	}` |
|      42 | 2306 | `	CurlMimeSimple(pB,zName,pVal);` |
|      38 | 2307 | `done:` |
|      76 | 2308 | `	CurlCStrFree(pVm,zOwn);` |
|      76 | 2309 | `	return pB->bFailed ? PH7_ABORT : PH7_OK;` |
|      40 | 2310 | `}` |
|       - | 2311 | `/*` |
|       - | 2312 | ` * CURLOPT_POSTFIELDS with an array. An EMPTY one is not a multipart body at` |
|       - | 2313 | ` * all: php sets the ordinary empty string, so the request is a POST with a` |
|       - | 2314 | ` * zero-length urlencoded body and no boundary anywhere.` |
|       - | 2315 | ` */` |
|      62 | 2316 | `static int CurlSetPostFieldsArray(ph7_context *pCtx,phl_curl *pCurl,ph7_value *pVal,sxi32 *pRc)` |
|     ! 0 | 2317 | `{` |
|       - | 2318 | `	/* pCtx is 0 on the CLONE path, which has no calling context of its own:` |
|       - | 2319 | `	 * every value in the array cast cleanly when the option was first set, so` |
|       - | 2320 | `	 * there is nothing left to report. */` |
|      62 | 2321 | `	ph7_vm *pVm = pCurl->pVm;` |
|       - | 2322 | `	struct CurlMimeBuild sB;` |
|       - | 2323 | `	curl_mime *pMime;` |
|      62 | 2324 | `	if( ph7_array_count(pVal) < 1 ){` |
|       2 | 2325 | `		CurlFreeMime(pCurl);` |
|       2 | 2326 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_POSTFIELDSIZE,(long)0);` |
|       2 | 2327 | `		return curl_easy_setopt(pCurl->pEasy,CURLOPT_COPYPOSTFIELDS,"") == CURLE_OK ? 1 : 0;` |
|       - | 2328 | `	}` |
|      60 | 2329 | `	pMime = curl_mime_init(pCurl->pEasy);` |
|      60 | 2330 | `	if( pMime == 0 ){` |
|     ! 0 | 2331 | `		return 0;` |
|       - | 2332 | `	}` |
|      60 | 2333 | `	SyZero(&sB,sizeof(sB));` |
|      60 | 2334 | `	sB.pCtx = pCtx;` |
|      60 | 2335 | `	sB.pCurl = pCurl;` |
|      60 | 2336 | `	sB.pMime = pMime;` |
|      60 | 2337 | `	sB.rcThrow = PH7_OK;` |
|      60 | 2338 | `	sB.pFileClass = PH7_VmExtractClass(pVm,"CURLFile",sizeof("CURLFile")-1,FALSE,0);` |
|      60 | 2339 | `	sB.pStrFileClass = PH7_VmExtractClass(pVm,"CURLStringFile",sizeof("CURLStringFile")-1,FALSE,0);` |
|       - | 2340 | `	/* The parts of the PREVIOUS body must not be freed while libcurl still` |
|       - | 2341 | `	 * points at them, so the old mime is dropped only after the new one is` |
|       - | 2342 | `	 * installed -- and the new parts are chained on the handle as they open,` |
|       - | 2343 | `	 * which is why the old list is taken aside first. */` |
|       - | 2344 | `	{` |
|      60 | 2345 | `		phl_curl_part *pOldParts = pCurl->pParts;` |
|      60 | 2346 | `		curl_mime *pOldMime = pCurl->pMime;` |
|       - | 2347 | `		phl_curl_part *pNewParts;` |
|      60 | 2348 | `		pCurl->pParts = 0;` |
|      60 | 2349 | `		ph7_array_walk(pVal,CurlMimeWalk,&sB);` |
|      60 | 2350 | `		pNewParts = pCurl->pParts;    /* whatever the walk opened */` |
|      60 | 2351 | `		pCurl->pParts = pOldParts;` |
|      60 | 2352 | `		if( sB.bFailed \|\| curl_easy_setopt(pCurl->pEasy,CURLOPT_MIMEPOST,pMime) != CURLE_OK ){` |
|       - | 2353 | `			/* Nothing was installed, so the handle keeps the body it had and` |
|       - | 2354 | `			 * only the half-built one is torn down. */` |
|     ! 0 | 2355 | `			curl_mime_free(pMime);` |
|     ! 0 | 2356 | `			pCurl->pParts = pNewParts;` |
|     ! 0 | 2357 | `			CurlFreeParts(pCurl);` |
|     ! 0 | 2358 | `			pCurl->pParts = pOldParts;` |
|     ! 0 | 2359 | `			if( sB.rcThrow != PH7_OK ){` |
|     ! 0 | 2360 | `				*pRc = sB.rcThrow;` |
|     ! 0 | 2361 | `				return -1;` |
|       - | 2362 | `			}` |
|     ! 0 | 2363 | `			return 0;` |
|       - | 2364 | `		}` |
|       - | 2365 | `		/* Installed: libcurl no longer points at the old body, so it goes. */` |
|      60 | 2366 | `		if( pOldMime ){` |
|       2 | 2367 | `			curl_mime_free(pOldMime);` |
|       1 | 2368 | `		}` |
|      60 | 2369 | `		CurlFreeParts(pCurl);` |
|      60 | 2370 | `		pCurl->pParts = pNewParts;` |
|      60 | 2371 | `		pCurl->pMime = pMime;` |
|       - | 2372 | `	}` |
|       - | 2373 | `	/* php keeps the ARRAY: a copied handle rebuilds the whole structure from` |
|       - | 2374 | `	 * it, because a mime whose parts read through callbacks cannot be shared` |
|       - | 2375 | `	 * between two handles. */` |
|      60 | 2376 | `	if( pCurl->pPostArray ){` |
|       2 | 2377 | `		ph7_release_value(pVm,pCurl->pPostArray);` |
|       1 | 2378 | `	}` |
|      60 | 2379 | `	pCurl->pPostArray = ph7_new_scalar(pVm);` |
|      60 | 2380 | `	if( pCurl->pPostArray ){` |
|      60 | 2381 | `		PH7_MemObjStore(pVal,pCurl->pPostArray);` |
|      30 | 2382 | `	}` |
|      60 | 2383 | `	if( sB.rcThrow != PH7_OK ){` |
|     ! 0 | 2384 | `		*pRc = sB.rcThrow;` |
|     ! 0 | 2385 | `		return -1;` |
|       - | 2386 | `	}` |
|      60 | 2387 | `	return 1;` |
|      31 | 2388 | `}` |
|       - | 2389 |  |
|       - | 2390 | `/* ===== The handle verbs ===== */` |
|       - | 2391 |  |
|       - | 2392 | `/*` |
|       - | 2393 | `` * A bare instance of one of the handle classes. `new` on them is refused`` |
|       - | 2394 | ` * (php's "Cannot directly construct ..."), and this is the door the refusal` |
|       - | 2395 | ` * leaves open: the engine's own creation step, never the opcode's.` |
|       - | 2396 | ` */` |
|     418 | 2397 | `PH7_PRIVATE ph7_class_instance * PH7_CurlNewInstance(ph7_vm *pVm,const char *zName,int nName)` |
|       3 | 2398 | `{` |
|     421 | 2399 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,0,0);` |
|     421 | 2400 | `	return pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|       3 | 2401 | `}` |
|       - | 2402 |  |
|       - | 2403 | `/* CurlHandle\|false curl_init(?string $url = null) */` |
|     340 | 2404 | `static int vm_builtin_curl_init(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2405 | `{` |
|     343 | 2406 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2407 | `	ph7_class_instance *pThis;` |
|       - | 2408 | `	phl_curl *pCurl;` |
|     343 | 2409 | `	const char *zUrl = 0;` |
|     343 | 2410 | `	int nUrl = 0;` |
|     343 | 2411 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     252 | 2412 | `		zUrl = ph7_value_to_string(apArg[0],&nUrl);` |
|       - | 2413 | `		/*` |
|       - | 2414 | `		 * php screens the URL for an embedded NUL before libcurl sees it,` |
|       - | 2415 | `		 * because libcurl takes a C string and would silently stop at the` |
|       - | 2416 | `		 * byte. The sentence says "cURL option" even though the argument is` |
|       - | 2417 | `		 * $url -- it is the shared option-setter's wording, reached from here.` |
|       - | 2418 | `		 */` |
|     252 | 2419 | `		if( SyByteFind(zUrl,(sxu32)nUrl,0,0) == SXRET_OK ){` |
|       - | 2420 | `			/* The status a throw installs IS the builtin's status: answering` |
|       - | 2421 | `			 * PH7_OK leaves the throw half-raised, and the next script run in` |
|       - | 2422 | `			 * the same interpreter prints nothing at all. */` |
|       3 | 2423 | `			ph7_result_bool(pCtx,0);` |
|       3 | 2424 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2425 | `				"curl_init(): cURL option must not contain any null bytes");` |
|       - | 2426 | `		}` |
|     124 | 2427 | `	}` |
|     341 | 2428 | `	pCurl = CurlNewHandle(pVm);` |
|     341 | 2429 | `	if( pCurl == 0 ){` |
|     ! 0 | 2430 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2431 | `		return PH7_OK;` |
|       - | 2432 | `	}` |
|     341 | 2433 | `	pThis = PH7_CurlNewInstance(pVm,"CurlHandle",sizeof("CurlHandle")-1);` |
|     341 | 2434 | `	if( pThis == 0 \|\| CurlAttach(pThis,pCurl) != 0 ){` |
|     ! 0 | 2435 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2436 | `		return PH7_OK;` |
|       - | 2437 | `	}` |
|     341 | 2438 | `	if( zUrl ){` |
|     250 | 2439 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_URL,zUrl);` |
|     124 | 2440 | `	}` |
|     341 | 2441 | `	PH7_NativeResultObject(pCtx,pThis);` |
|     341 | 2442 | `	return PH7_OK;` |
|     173 | 2443 | `}` |
|       - | 2444 | `/*` |
|       - | 2445 | ` * void curl_close(CurlHandle $handle)` |
|       - | 2446 | ` *` |
|       - | 2447 | ` * A NO-OP, which is the whole finding. php 8 turned the resource into an` |
|       - | 2448 | ` * object, and the object's own teardown is what frees the handle -- so after` |
|       - | 2449 | ` * curl_close() the handle still works: curl_setopt() answers true, curl_exec()` |
|       - | 2450 | ` * runs the transfer, curl_errno() reports it. Freeing here (which is what the` |
|       - | 2451 | ` * name says and what php 7 did) would make every one of those a use-after-free` |
|       - | 2452 | ` * on a script php runs happily.` |
|       - | 2453 | ` */` |
|       8 | 2454 | `static int vm_builtin_curl_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2455 | `{` |
|       4 | 2456 | `	SXUNUSED(nArg);` |
|       4 | 2457 | `	SXUNUSED(apArg);` |
|       9 | 2458 | `	ph7_result_null(pCtx);` |
|       9 | 2459 | `	return PH7_OK;` |
|       1 | 2460 | `}` |
|       - | 2461 | `/*` |
|       - | 2462 | ` * void curl_reset(CurlHandle $handle)` |
|       - | 2463 | ` *` |
|       - | 2464 | ` * Every OPTION goes back to its default -- and the error state does NOT.` |
|       - | 2465 | ` * curl_reset() on a handle whose last transfer failed leaves curl_errno()` |
|       - | 2466 | ` * reporting that failure; only the option setters clear it. (Probing the verbs` |
|       - | 2467 | ` * one at a time is what shows this: a sweep that resets after a setopt sees a` |
|       - | 2468 | ` * cleared errno and credits the wrong verb.)` |
|       - | 2469 | ` *` |
|       - | 2470 | ` * "Every option" includes the ones libcurl never saw: the retained callables` |
|       - | 2471 | ` * and the write destination are php's own state, and php resets them here` |
|       - | 2472 | ` * alongside the library's -- so a reset handle prints its next body rather` |
|       - | 2473 | ` * than answering it, and calls nothing. The slists go with them: libcurl has` |
|       - | 2474 | ` * just dropped every pointer to one, so the lists this handle owns are` |
|       - | 2475 | ` * unreachable and freeing them here is the last chance before close.` |
|       - | 2476 | ` */` |
|      16 | 2477 | `static int vm_builtin_curl_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2478 | `{` |
|      17 | 2479 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|      17 | 2480 | `	if( pCurl && pCurl->pEasy ){` |
|      17 | 2481 | `		curl_easy_reset(pCurl->pEasy);` |
|       - | 2482 | `		/* curl_easy_reset() drops the error buffer with everything else. */` |
|      17 | 2483 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_ERRORBUFFER,pCurl->zErrBuf);` |
|      17 | 2484 | `		CurlDropCallbacks(pCurl);` |
|      17 | 2485 | `		CurlFreeSlists(pCurl);` |
|      17 | 2486 | `		CurlFreeMime(pCurl);` |
|      17 | 2487 | `		pCurl->iWriteDest = PHL_CURL_DEST_STDOUT;` |
|      17 | 2488 | `		pCurl->bXferIsProgress = 0;` |
|       8 | 2489 | `	}` |
|      17 | 2490 | `	ph7_result_null(pCtx);` |
|      17 | 2491 | `	return PH7_OK;` |
|       1 | 2492 | `}` |
|       - | 2493 | `/*` |
|       - | 2494 | ` * php's error state after a REFUSED option: errno 48 and libcurl's own text` |
|       - | 2495 | ` * for it. The ValueError is thrown and the handle still reports the failure --` |
|       - | 2496 | `` * `curl_setopt($h, 999999, 1)` in a try/catch leaves curl_errno() at 48 -- so`` |
|       - | 2497 | ` * the two are not alternatives, they both happen.` |
|       - | 2498 | ` */` |
|      10 | 2499 | `static void CurlSetErr(phl_curl *pCurl,int iCode)` |
|       1 | 2500 | `{` |
|       - | 2501 | `	const char *zMsg;` |
|      11 | 2502 | `	if( pCurl == 0 ){` |
|     ! 0 | 2503 | `		return;` |
|       - | 2504 | `	}` |
|      11 | 2505 | `	pCurl->iLastErr = iCode;` |
|      11 | 2506 | `	zMsg = curl_easy_strerror((CURLcode)iCode);` |
|      11 | 2507 | `	pCurl->zErrBuf[0] = 0;` |
|      11 | 2508 | `	if( zMsg ){` |
|      11 | 2509 | `		sxu32 nMsg = SyStrlen(zMsg);` |
|      11 | 2510 | `		if( nMsg > sizeof(pCurl->zErrBuf) - 1 ){ nMsg = sizeof(pCurl->zErrBuf) - 1; }` |
|      11 | 2511 | `		SyMemcpy(zMsg,pCurl->zErrBuf,nMsg);` |
|      11 | 2512 | `		pCurl->zErrBuf[nMsg] = 0;` |
|       5 | 2513 | `	}` |
|       6 | 2514 | `}` |
|       - | 2515 | `/*` |
|       - | 2516 | ` * Every setter clears the handle's error FIRST (curl_setopt and` |
|       - | 2517 | ` * curl_setopt_array are two of the three verbs that do; curl_upkeep is the` |
|       - | 2518 | ` * third, and curl_reset notably is not).` |
|       - | 2519 | ` */` |
|     824 | 2520 | `static void CurlClearErr(phl_curl *pCurl)` |
|       1 | 2521 | `{` |
|     825 | 2522 | `	if( pCurl ){` |
|     825 | 2523 | `		pCurl->iLastErr = 0;` |
|     825 | 2524 | `		pCurl->zErrBuf[0] = 0;` |
|     412 | 2525 | `	}` |
|     825 | 2526 | `}` |
|       - | 2527 | `/*` |
|       - | 2528 | ` * A string option's value. php stringifies ANYTHING for these -- null becomes` |
|       - | 2529 | ` * "", an array becomes "Array" with the ordinary conversion warning, an object` |
|       - | 2530 | ` * is the ordinary "could not be converted to string" Error -- and then screens` |
|       - | 2531 | ` * the result for a NUL, because libcurl takes a C string and would silently` |
|       - | 2532 | ` * stop at the byte. The screen is on the whole VALUE, which is why the same` |
|       - | 2533 | ` * sentence appears from curl_init().` |
|       - | 2534 | ` *` |
|       - | 2535 | ` * A slist ELEMENT is stringified the same way but is NOT screened: php lets a` |
|       - | 2536 | ` * header carrying a NUL through, which is measured, not assumed.` |
|       - | 2537 | ` */` |
|      46 | 2538 | `static int CurlSetString(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal,sxi32 *pRc)` |
|       1 | 2539 | `{` |
|       - | 2540 | `	const char *zVal;` |
|      47 | 2541 | `	int nVal = 0;` |
|       - | 2542 | `	sxi32 rcSv;` |
|       - | 2543 | `	/* The USER-VISIBLE coercion, not the embedder API: an array has to warn` |
|       - | 2544 | `	 * "Array to string conversion" and an object with no __toString() has to` |
|       - | 2545 | `	 * be php's catchable Error, both of which the silent ph7_value_to_string()` |
|       - | 2546 | `	 * skips. */` |
|      47 | 2547 | `	rcSv = PH7_ValueToStringUV(pCtx,pVal,&zVal,&nVal);` |
|      47 | 2548 | `	if( rcSv != SXRET_OK ){` |
|       - | 2549 | `		/* The cast threw -- and php's cast has ALREADY HAPPENED: it answers an` |
|       - | 2550 | `		 * empty string beside the throw, and the option is set from that` |
|       - | 2551 | `` 		 * before anything unwinds. So `curl_setopt($h, CURLOPT_URL, $obj)` `` |
|       - | 2552 | `		 * really does leave the handle with no URL at all, where reporting the` |
|       - | 2553 | `		 * throw and skipping the write would leave the old one standing.` |
|       - | 2554 | `		 *` |
|       - | 2555 | `		 * Its status is the BUILTIN's status: swallowing it and answering` |
|       - | 2556 | `		 * PH7_OK leaves the engine with a half-installed throw, and the next` |
|       - | 2557 | `		 * script run in the same interpreter prints nothing. */` |
|       7 | 2558 | `		curl_easy_setopt(pCurl->pEasy,(CURLoption)iOpt,"");` |
|       7 | 2559 | `		*pRc = rcSv;` |
|       7 | 2560 | `		return -1;` |
|       - | 2561 | `	}` |
|      41 | 2562 | `	if( SyByteFind(zVal,(sxu32)nVal,0,0) == SXRET_OK ){` |
|       5 | 2563 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2564 | `			"curl_setopt(): cURL option must not contain any null bytes");` |
|       5 | 2565 | `		return -1;` |
|       - | 2566 | `	}` |
|      37 | 2567 | `	return curl_easy_setopt(pCurl->pEasy,(CURLoption)iOpt,zVal) == CURLE_OK ? 1 : 0;` |
|      24 | 2568 | `}` |
|       - | 2569 | `/*` |
|       - | 2570 | ` * A long option's value, with the one option php screens by hand.` |
|       - | 2571 | ` * CURLOPT_SSL_VERIFYHOST no longer has a meaningful 1: libcurl treats it as 2,` |
|       - | 2572 | ` * and php says so at E_NOTICE before passing 2 along -- so a program that` |
|       - | 2573 | ` * still writes 1 gets php's sentence, not silence.` |
|       - | 2574 | ` */` |
|     216 | 2575 | `static int CurlSetLong(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal)` |
|       1 | 2576 | `{` |
|     217 | 2577 | `	sxi64 iVal = ph7_value_to_int64(pVal);` |
|     217 | 2578 | `	if( iOpt == CURLOPT_SSL_VERIFYHOST && iVal == 1 ){` |
|       - | 2579 | `		/* ph7_context_throw_error already prints "curl_setopt(): " */` |
|       5 | 2580 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,` |
|       - | 2581 | `			"CURLOPT_SSL_VERIFYHOST no longer accepts the value 1, "` |
|       - | 2582 | `			"value 2 will be used instead");` |
|       5 | 2583 | `		iVal = 2;` |
|       2 | 2584 | `	}` |
|     217 | 2585 | `	return curl_easy_setopt(pCurl->pEasy,(CURLoption)iOpt,(long)iVal) == CURLE_OK ? 1 : 0;` |
|       1 | 2586 | `}` |
|       - | 2587 | `/*` |
|       - | 2588 | ` * An slist option's value: the array's VALUES in order, keys ignored, each` |
|       - | 2589 | ` * stringified. libcurl does not copy the list, so the handle owns it until the` |
|       - | 2590 | ` * option is set again or the handle is freed -- which is what pSlist is for.` |
|       - | 2591 | ` */` |
|       - | 2592 | `struct CurlSlistBuild {` |
|       - | 2593 | `	ph7_context *pCtx;` |
|       - | 2594 | `	struct curl_slist *pList;` |
|       - | 2595 | `	int bFailed;` |
|       - | 2596 | `	int bThrew;` |
|       - | 2597 | `	sxi32 rcThrow;` |
|       - | 2598 | `};` |
|      40 | 2599 | `static int CurlSlistWalk(ph7_value *pKey,ph7_value *pVal,void *pUser)` |
|       1 | 2600 | `{` |
|      41 | 2601 | `	struct CurlSlistBuild *pB = (struct CurlSlistBuild *)pUser;` |
|       - | 2602 | `	struct curl_slist *pNext;` |
|       - | 2603 | `	const char *zVal;` |
|      41 | 2604 | `	int nVal = 0;` |
|      20 | 2605 | `	SXUNUSED(pKey);` |
|      41 | 2606 | `	if( pB->bFailed ){` |
|     ! 0 | 2607 | `		return PH7_OK;` |
|       - | 2608 | `	}` |
|       - | 2609 | `	/* Same user-visible coercion the scalar options get -- an object in a` |
|       - | 2610 | `	 * header list is php's Error, not a silent "Object" -- but NO null-byte` |
|       - | 2611 | `	 * screen: php lets a list element carrying one through. */` |
|       - | 2612 | `	{` |
|      41 | 2613 | `		sxi32 rcSv = PH7_ValueToStringUV(pB->pCtx,pVal,&zVal,&nVal);` |
|      41 | 2614 | `		if( rcSv != SXRET_OK ){` |
|       - | 2615 | `			/* php's cast of an object with no __toString() throws AND answers` |
|       - | 2616 | `			 * an empty string, and the loop that asked keeps walking: the` |
|       - | 2617 | `			 * element becomes "", the elements after it are still appended and` |
|       - | 2618 | `			 * the finished list still REPLACES whatever the option held. Only` |
|       - | 2619 | `			 * the throw is remembered here. */` |
|       5 | 2620 | `			if( !pB->bThrew ){` |
|       5 | 2621 | `				pB->bThrew = 1;` |
|       5 | 2622 | `				pB->rcThrow = rcSv;` |
|       2 | 2623 | `			}` |
|       5 | 2624 | `			zVal = "";` |
|       5 | 2625 | `			nVal = 0;` |
|       2 | 2626 | `		}` |
|       - | 2627 | `	}` |
|       - | 2628 | `	/* The value is a TEMPORARY the walker owns for this call only, and` |
|       - | 2629 | `	 * curl_slist_append copies it, so nothing is kept past the return. */` |
|      41 | 2630 | `	pNext = curl_slist_append(pB->pList,zVal ? zVal : "");` |
|      41 | 2631 | `	if( pNext == 0 ){` |
|     ! 0 | 2632 | `		pB->bFailed = 1;` |
|     ! 0 | 2633 | `		return PH7_OK;` |
|       - | 2634 | `	}` |
|      41 | 2635 | `	pB->pList = pNext;` |
|      41 | 2636 | `	return PH7_OK;` |
|      21 | 2637 | `}` |
|      28 | 2638 | `static int CurlSetSlist(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal,` |
|       - | 2639 | `	const char *zOptName,sxi32 *pRc)` |
|       1 | 2640 | `{` |
|       - | 2641 | `	struct CurlSlistBuild sB;` |
|       - | 2642 | `	phl_curl_slist *pSlot;` |
|      29 | 2643 | `	if( !ph7_value_is_array(pVal) ){` |
|       4 | 2644 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       1 | 2645 | `			"curl_setopt(): The %s option must have an array value",zOptName);` |
|       3 | 2646 | `		return -1;` |
|       - | 2647 | `	}` |
|      27 | 2648 | `	sB.pCtx = pCtx;` |
|      27 | 2649 | `	sB.pList = 0;` |
|      27 | 2650 | `	sB.bFailed = 0;` |
|      27 | 2651 | `	sB.bThrew = 0;` |
|      27 | 2652 | `	sB.rcThrow = PH7_OK;` |
|      27 | 2653 | `	ph7_array_walk(pVal,CurlSlistWalk,&sB);` |
|      27 | 2654 | `	if( sB.bFailed ){` |
|     ! 0 | 2655 | `		if( sB.pList ){` |
|     ! 0 | 2656 | `			curl_slist_free_all(sB.pList);` |
|     ! 0 | 2657 | `		}` |
|     ! 0 | 2658 | `		if( sB.bThrew ){` |
|     ! 0 | 2659 | `			*pRc = sB.rcThrow;` |
|     ! 0 | 2660 | `			return -1;` |
|       - | 2661 | `		}` |
|     ! 0 | 2662 | `		return 0;` |
|       - | 2663 | `	}` |
|       - | 2664 | `	/* Replace whatever this option held before: the previous list stays alive` |
|       - | 2665 | `	 * until libcurl has been pointed at the new one. */` |
|      27 | 2666 | `	pSlot = CurlSlistSlot(pCurl,iOpt);` |
|      27 | 2667 | `	if( pSlot == 0 ){` |
|     ! 0 | 2668 | `		if( sB.pList ){` |
|     ! 0 | 2669 | `			curl_slist_free_all(sB.pList);` |
|     ! 0 | 2670 | `		}` |
|     ! 0 | 2671 | `		return 0;` |
|       - | 2672 | `	}` |
|      27 | 2673 | `	if( curl_easy_setopt(pCurl->pEasy,(CURLoption)iOpt,sB.pList) != CURLE_OK ){` |
|     ! 0 | 2674 | `		if( sB.pList ){` |
|     ! 0 | 2675 | `			curl_slist_free_all(sB.pList);` |
|     ! 0 | 2676 | `		}` |
|     ! 0 | 2677 | `		return 0;` |
|       - | 2678 | `	}` |
|      27 | 2679 | `	if( pSlot->pList ){` |
|      15 | 2680 | `		curl_slist_free_all(pSlot->pList);` |
|       7 | 2681 | `	}` |
|      27 | 2682 | `	pSlot->pList = sB.pList;` |
|      27 | 2683 | `	if( sB.bThrew ){` |
|       - | 2684 | `		/* An element threw, and the list php built from the rest is still the` |
|       - | 2685 | `		 * one the handle keeps -- installed above, and only now does the throw` |
|       - | 2686 | `		 * travel. */` |
|       5 | 2687 | `		*pRc = sB.rcThrow;` |
|       5 | 2688 | `		return -1;` |
|       - | 2689 | `	}` |
|      23 | 2690 | `	return 1;` |
|      15 | 2691 | `}` |
|       - | 2692 | `/* ===== The php STREAMS a transfer can be pointed at ===== */` |
|       - | 2693 |  |
|       - | 2694 | `/*` |
|       - | 2695 | ` * php's screen for the five options that take a stream, worded as php words` |
|       - | 2696 | ` * it -- and it is worded twice: a value that is not a resource AT ALL and a` |
|       - | 2697 | ` * resource that has been closed report different sentences, both TypeErrors.` |
|       - | 2698 | ` * The three WRITE destinations screen once more, for a handle opened read-only,` |
|       - | 2699 | ` * and that one is a ValueError.` |
|       - | 2700 | ` *` |
|       - | 2701 | ` * null is accepted by all five and clears the option, which is how a script` |
|       - | 2702 | ` * puts the default back.` |
|       - | 2703 | ` */` |
|      90 | 2704 | `static io_private * CurlStreamArg(ph7_context *pCtx,ph7_value *pVal,int bWritable,` |
|       - | 2705 | `	const char *zFunc,sxi32 *pRc)` |
|       1 | 2706 | `{` |
|       - | 2707 | `	io_private *pDev;` |
|      91 | 2708 | `	if( !ph7_value_is_resource(pVal) ){` |
|      61 | 2709 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|      20 | 2710 | `			"%s(): supplied argument is not a valid File-Handle resource",zFunc);` |
|      41 | 2711 | `		return 0;` |
|       - | 2712 | `	}` |
|      51 | 2713 | `	pDev = (io_private *)ph7_value_to_resource(pVal);` |
|      51 | 2714 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       - | 2715 | `		/* "resource", not "argument": php tells a closed handle apart from a` |
|       - | 2716 | `		 * value that was never one. */` |
|      16 | 2717 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       5 | 2718 | `			"%s(): supplied resource is not a valid File-Handle resource",zFunc);` |
|      11 | 2719 | `		return 0;` |
|       - | 2720 | `	}` |
|      41 | 2721 | `	if( bWritable ){` |
|       - | 2722 | `		/* The mode the opener asked for is what php reads too: anything but a` |
|       - | 2723 | `		 * bare "r" can be written. */` |
|      29 | 2724 | `		const char *zMode = pDev->zMode;` |
|      29 | 2725 | `		if( zMode[0] == 0 \|\| (zMode[0] == 'r' && SyByteFind(zMode,(sxu32)SyStrlen(zMode),'+',0) != SXRET_OK) ){` |
|      10 | 2726 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       3 | 2727 | `				"%s(): The provided file handle must be writable",zFunc);` |
|       7 | 2728 | `			return 0;` |
|       - | 2729 | `		}` |
|      11 | 2730 | `	}` |
|      35 | 2731 | `	return pDev;` |
|      46 | 2732 | `}` |
|       - | 2733 | `/* The open stream a stored value names, or 0 if the script has closed it. */` |
|      54 | 2734 | `static io_private * CurlStreamOf(ph7_value *pVal)` |
|       1 | 2735 | `{` |
|       - | 2736 | `	io_private *pDev;` |
|      55 | 2737 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|       4 | 2738 | `		return 0;` |
|       - | 2739 | `	}` |
|      51 | 2740 | `	pDev = (io_private *)ph7_value_to_resource(pVal);` |
|      51 | 2741 | `	return IO_PRIVATE_INVALID(pDev) ? 0 : pDev;` |
|      29 | 2742 | `}` |
|       - | 2743 | `/* Store one of the five, or clear it when the value is null. */` |
|     102 | 2744 | `static int CurlSetStreamSlot(ph7_context *pCtx,phl_curl *pCurl,ph7_value **ppSlot,` |
|       - | 2745 | `	ph7_value *pVal,int bWritable,const char *zFunc,sxi32 *pRc)` |
|       1 | 2746 | `{` |
|     103 | 2747 | `	ph7_vm *pVm = pCurl->pVm;` |
|     103 | 2748 | `	if( ph7_value_is_null(pVal) ){` |
|      13 | 2749 | `		if( *ppSlot ){` |
|      13 | 2750 | `			ph7_release_value(pVm,*ppSlot);` |
|      13 | 2751 | `			*ppSlot = 0;` |
|       6 | 2752 | `		}` |
|      13 | 2753 | `		return 1;` |
|       - | 2754 | `	}` |
|      91 | 2755 | `	if( CurlStreamArg(pCtx,pVal,bWritable,zFunc,pRc) == 0 ){` |
|      57 | 2756 | `		return -1;` |
|       - | 2757 | `	}` |
|      35 | 2758 | `	if( *ppSlot ){` |
|       5 | 2759 | `		ph7_release_value(pVm,*ppSlot);` |
|       2 | 2760 | `	}` |
|      35 | 2761 | `	*ppSlot = ph7_new_scalar(pVm);` |
|      35 | 2762 | `	if( *ppSlot == 0 ){` |
|     ! 0 | 2763 | `		return 0;` |
|       - | 2764 | `	}` |
|      35 | 2765 | `	PH7_MemObjStore(pVal,*ppSlot);` |
|      35 | 2766 | `	return 1;` |
|      52 | 2767 | `}` |
|       - | 2768 |  |
|       - | 2769 | `/*` |
|       - | 2770 | ` * CURLOPT_POSTFIELDS, everything that is not an array: the request BODY.` |
|       - | 2771 | ` *` |
|       - | 2772 | ` * php stringifies the value the way every other string option does -- an int,` |
|       - | 2773 | ` * a float, a bool, null and a resource all have a spelling, an object needs a` |
|       - | 2774 | ` * __toString() and is otherwise the ordinary Error -- and then hands libcurl` |
|       - | 2775 | ` * the LENGTH beside the bytes. That pair is the difference from a plain string` |
|       - | 2776 | ` * option: a body may contain a NUL and php does not screen for one, so the` |
|       - | 2777 | ` * option is set through CURLOPT_POSTFIELDSIZE + CURLOPT_COPYPOSTFIELDS rather` |
|       - | 2778 | ` * than through CURLOPT_POSTFIELDS, whose C string would stop at the byte.` |
|       - | 2779 | ` *` |
|       - | 2780 | ` * COPYPOSTFIELDS is also the reason nothing here has to be retained: libcurl` |
|       - | 2781 | ` * takes its own copy, so the caller's string may die the moment setopt` |
|       - | 2782 | `` * returns, which is what php's own answer for `$s = str_repeat(...); setopt();`` |
|       - | 2783 | `` * unset($s);` shows.`` |
|       - | 2784 | ` */` |
|      26 | 2785 | `static int CurlSetPostFields(ph7_context *pCtx,phl_curl *pCurl,ph7_value *pVal,sxi32 *pRc)` |
|     ! 0 | 2786 | `{` |
|       - | 2787 | `	const char *zVal;` |
|      26 | 2788 | `	int nVal = 0;` |
|       - | 2789 | `	sxi32 rcSv;` |
|      26 | 2790 | `	rcSv = PH7_ValueToStringUV(pCtx,pVal,&zVal,&nVal);` |
|      26 | 2791 | `	if( rcSv != SXRET_OK ){` |
|       - | 2792 | `		/* The cast threw and still happened: php sets the body to the empty` |
|       - | 2793 | `		 * string it answered, so the handle is a POST with no content after` |
|       - | 2794 | `		 * the throw travels. */` |
|       2 | 2795 | `		zVal = "";` |
|       2 | 2796 | `		nVal = 0;` |
|       2 | 2797 | `		*pRc = rcSv;` |
|       1 | 2798 | `	}` |
|       - | 2799 | `	/* The size FIRST: COPYPOSTFIELDS reads it to know how much to copy, and` |
|       - | 2800 | `	 * reads the string's own length only when it is still at the -1 default. */` |
|      26 | 2801 | `	if( curl_easy_setopt(pCurl->pEasy,CURLOPT_POSTFIELDSIZE,(long)nVal) != CURLE_OK ){` |
|     ! 0 | 2802 | `		return rcSv != SXRET_OK ? -1 : 0;` |
|       - | 2803 | `	}` |
|      26 | 2804 | `	if( curl_easy_setopt(pCurl->pEasy,CURLOPT_COPYPOSTFIELDS,zVal) != CURLE_OK ){` |
|     ! 0 | 2805 | `		return rcSv != SXRET_OK ? -1 : 0;` |
|       - | 2806 | `	}` |
|      26 | 2807 | `	return rcSv != SXRET_OK ? -1 : 1;` |
|      13 | 2808 | `}` |
|       - | 2809 | `/*` |
|       - | 2810 | ` * One option, the whole switch. Answers 1 (true), 0 (false) or -1 (a throw is` |
|       - | 2811 | ` * already installed).` |
|       - | 2812 | ` */` |
|     820 | 2813 | `static int CurlSetOne(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal,` |
|       - | 2814 | `	const char *zFunc,sxi32 *pRc)` |
|       1 | 2815 | `{` |
|     821 | 2816 | `	const struct CurlOptDef *pDef = CurlOptFind(iOpt);` |
|     821 | 2817 | `	if( pDef == 0 ){` |
|       - | 2818 | `		/* php's switch has no arm for it, so the library never sees it -- and` |
|       - | 2819 | `		 * the handle records CURLE_UNKNOWN_OPTION all the same. */` |
|       7 | 2820 | `		CurlSetErr(pCurl,CURLE_UNKNOWN_OPTION);` |
|      10 | 2821 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       3 | 2822 | `			"%s(): Argument #2 ($option) is not a valid cURL option",zFunc);` |
|       7 | 2823 | `		return -1;` |
|       - | 2824 | `	}` |
|     815 | 2825 | `	switch( pDef->iKind ){` |
|     108 | 2826 | `	case CURL_OPT_LONG:` |
|     217 | 2827 | `		return CurlSetLong(pCtx,pCurl,iOpt,pVal);` |
|      23 | 2828 | `	case CURL_OPT_STRING:` |
|      47 | 2829 | `		return CurlSetString(pCtx,pCurl,iOpt,pVal,pRc);` |
|      14 | 2830 | `	case CURL_OPT_SLIST:` |
|      29 | 2831 | `		return CurlSetSlist(pCtx,pCurl,iOpt,pVal,CurlOptName(iOpt),pRc);` |
|       6 | 2832 | `	case CURL_OPT_SAFEUP:` |
|       - | 2833 | `		/* php's -1: safe uploads cannot be turned off any more, and the` |
|       - | 2834 | `		 * refusal is on the VALUE's truthiness, not its type. */` |
|      13 | 2835 | `		if( !ph7_value_to_bool(pVal) ){` |
|      10 | 2836 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       3 | 2837 | `				"%s(): Disabling safe uploads is no longer supported",zFunc);` |
|       7 | 2838 | `			return -1;` |
|       - | 2839 | `		}` |
|       7 | 2840 | `		return 1;` |
|      51 | 2841 | `	case CURL_OPT_FILE:` |
|       - | 2842 | `		/* The five php STREAM options. Three of them are DESTINATIONS and move` |
|       - | 2843 | `		 * the same one-setting-wins fields CURLOPT_RETURNTRANSFER and the` |
|       - | 2844 | `		 * callbacks move; the read pair is the upload source and moves` |
|       - | 2845 | `		 * nothing. */` |
|     103 | 2846 | `		switch( (int)iOpt ){` |
|      14 | 2847 | `		case CURLOPT_FILE: {` |
|      29 | 2848 | `			int rcS = CurlSetStreamSlot(pCtx,pCurl,&pCurl->pWriteStream,pVal,TRUE,zFunc,pRc);` |
|      29 | 2849 | `			if( rcS == 1 ){` |
|      17 | 2850 | `				pCurl->iWriteDest = pCurl->pWriteStream ? PHL_CURL_DEST_FILE` |
|       8 | 2851 | `				                                        : PHL_CURL_DEST_STDOUT;` |
|       8 | 2852 | `			}` |
|      29 | 2853 | `			return rcS;` |
|       - | 2854 | `		}` |
|       9 | 2855 | `		case CURLOPT_WRITEHEADER: {` |
|      19 | 2856 | `			int rcS = CurlSetStreamSlot(pCtx,pCurl,&pCurl->pHeaderStream,pVal,TRUE,zFunc,pRc);` |
|      19 | 2857 | `			if( rcS == 1 ){` |
|       7 | 2858 | `				pCurl->iHeaderDest = pCurl->pHeaderStream ? PHL_CURL_HDR_FILE` |
|       3 | 2859 | `				                                          : PHL_CURL_HDR_IGNORE;` |
|       3 | 2860 | `			}` |
|      19 | 2861 | `			return rcS;` |
|       - | 2862 | `		}` |
|      10 | 2863 | `		case CURLOPT_STDERR:` |
|      21 | 2864 | `			return CurlSetStreamSlot(pCtx,pCurl,&pCurl->pStderrStream,pVal,TRUE,zFunc,pRc);` |
|      18 | 2865 | `		case CURLOPT_INFILE:` |
|       - | 2866 | `			/* CURLOPT_READDATA is the same number: one slot, either spelling,` |
|       - | 2867 | `			 * and no writability screen -- php reads from it. */` |
|      37 | 2868 | `			return CurlSetStreamSlot(pCtx,pCurl,&pCurl->pReadStream,pVal,FALSE,zFunc,pRc);` |
|     ! 0 | 2869 | `		default:` |
|     ! 0 | 2870 | `			break;` |
|       - | 2871 | `		}` |
|     ! 0 | 2872 | `		break;` |
|      49 | 2873 | `	case CURL_OPT_CALLBACK:` |
|      99 | 2874 | `		switch( (int)iOpt ){` |
|       3 | 2875 | `		case CURLOPT_READFUNCTION:` |
|       9 | 2876 | `			return PH7_CurlSetCallback(pCtx,pCurl->pVm,&pCurl->pReadCb,pVal,zFunc,` |
|       3 | 2877 | `				"#3 ($value)",CurlOptName(iOpt),TRUE,pRc);` |
|       2 | 2878 | `		case CURLOPT_DEBUGFUNCTION:` |
|       6 | 2879 | `			return PH7_CurlSetCallback(pCtx,pCurl->pVm,&pCurl->pDebugCb,pVal,zFunc,` |
|       2 | 2880 | `				"#3 ($value)",CurlOptName(iOpt),TRUE,pRc);` |
|       3 | 2881 | `		case CURLOPT_PREREQFUNCTION:` |
|       9 | 2882 | `			return PH7_CurlSetCallback(pCtx,pCurl->pVm,&pCurl->pPreReqCb,pVal,zFunc,` |
|       3 | 2883 | `				"#3 ($value)",CurlOptName(iOpt),TRUE,pRc);` |
|      32 | 2884 | `		case CURLOPT_WRITEFUNCTION: {` |
|       - | 2885 | `			/* The one option that moves the destination as well as the slot:` |
|       - | 2886 | `			 * a callback claims the body, and a null hands it back to the` |
|       - | 2887 | `			 * DEFAULT rather than to whatever CURLOPT_RETURNTRANSFER last` |
|       - | 2888 | `			 * said. */` |
|      97 | 2889 | `			int rcCb = PH7_CurlSetCallback(pCtx,pCurl->pVm,&pCurl->pWriteCb,pVal,zFunc,` |
|      32 | 2890 | `				"#3 ($value)",CurlOptName(iOpt),TRUE,pRc);` |
|      65 | 2891 | `			if( rcCb == 1 ){` |
|      59 | 2892 | `				pCurl->iWriteDest = pCurl->pWriteCb ? PHL_CURL_DEST_USER` |
|      29 | 2893 | `				                                    : PHL_CURL_DEST_STDOUT;` |
|      29 | 2894 | `			}` |
|      65 | 2895 | `			return rcCb;` |
|       - | 2896 | `		}` |
|       4 | 2897 | `		case CURLOPT_HEADERFUNCTION: {` |
|       - | 2898 | `			/* The header destination moves with it, exactly as the body's does` |
|       - | 2899 | `			 * with CURLOPT_WRITEFUNCTION. */` |
|      13 | 2900 | `			int rcCb = PH7_CurlSetCallback(pCtx,pCurl->pVm,&pCurl->pHeaderCb,pVal,zFunc,` |
|       4 | 2901 | `				"#3 ($value)",CurlOptName(iOpt),TRUE,pRc);` |
|       9 | 2902 | `			if( rcCb == 1 ){` |
|       9 | 2903 | `				pCurl->iHeaderDest = pCurl->pHeaderCb ? PHL_CURL_HDR_USER` |
|       4 | 2904 | `				                                      : PHL_CURL_HDR_IGNORE;` |
|       4 | 2905 | `			}` |
|       9 | 2906 | `			return rcCb;` |
|       - | 2907 | `		}` |
|       5 | 2908 | `		case CURLOPT_XFERINFOFUNCTION:` |
|       - | 2909 | `		case CURLOPT_PROGRESSFUNCTION: {` |
|      16 | 2910 | `			int rcCb = PH7_CurlSetCallback(pCtx,pCurl->pVm,&pCurl->pXferCb,pVal,zFunc,` |
|       5 | 2911 | `				"#3 ($value)",CurlOptName(iOpt),TRUE,pRc);` |
|      11 | 2912 | `			if( rcCb == 1 ){` |
|      11 | 2913 | `				pCurl->bXferIsProgress = (iOpt == CURLOPT_PROGRESSFUNCTION);` |
|       5 | 2914 | `			}` |
|      11 | 2915 | `			return rcCb;` |
|       - | 2916 | `		}` |
|     ! 0 | 2917 | `		default:` |
|     ! 0 | 2918 | `			break;` |
|       - | 2919 | `		}` |
|     ! 0 | 2920 | `		break;` |
|      95 | 2921 | `	case CURL_OPT_RETURN:` |
|       - | 2922 | `		/* php's own option, and a FLAG: every value is accepted (setopt always` |
|       - | 2923 | `		 * answers true) and only its truthiness matters. It moves the same` |
|       - | 2924 | `		 * destination CURLOPT_WRITEFUNCTION moves, so setting it after a` |
|       - | 2925 | `		 * callback takes the body back off that callback. */` |
|     191 | 2926 | `		pCurl->iWriteDest = ph7_value_to_bool(pVal) ? PHL_CURL_DEST_RETURN` |
|      95 | 2927 | `		                                            : PHL_CURL_DEST_STDOUT;` |
|     191 | 2928 | `		return 1;` |
|      10 | 2929 | `	case CURL_OPT_PRIVATE:` |
|       - | 2930 | `		/* php keeps the value ITSELF -- any type, an object by reference and` |
|       - | 2931 | `		 * an array by copy, exactly as an ordinary assignment would. Nothing` |
|       - | 2932 | `		 * reaches libcurl, so it always answers true. */` |
|      21 | 2933 | `		if( pCurl->pPrivate ){` |
|      19 | 2934 | `			ph7_release_value(pCurl->pVm,pCurl->pPrivate);` |
|       9 | 2935 | `		}` |
|      21 | 2936 | `		pCurl->pPrivate = ph7_new_scalar(pCurl->pVm);` |
|      21 | 2937 | `		if( pCurl->pPrivate == 0 ){` |
|     ! 0 | 2938 | `			return 0;` |
|       - | 2939 | `		}` |
|      21 | 2940 | `		PH7_MemObjStore(pVal,pCurl->pPrivate);` |
|      21 | 2941 | `		return 1;` |
|      40 | 2942 | `	case CURL_OPT_POSTFIELDS:` |
|      80 | 2943 | `		if( ph7_value_is_array(pVal) ){` |
|      54 | 2944 | `			return CurlSetPostFieldsArray(pCtx,pCurl,pVal,pRc);` |
|       - | 2945 | `		}` |
|       - | 2946 | `		/* A body set the plain way replaces a multipart one -- libcurl decides` |
|       - | 2947 | `		 * that by itself (the last of the two options set wins), but the mime` |
|       - | 2948 | `		 * this handle OWNS would otherwise outlive its last reader. */` |
|      26 | 2949 | `		CurlFreeMime(pCurl);` |
|      26 | 2950 | `		return CurlSetPostFields(pCtx,pCurl,pVal,pRc);` |
|      11 | 2951 | `	case CURL_OPT_SHARE:` |
|       - | 2952 | `		/*` |
|       - | 2953 | `		 * The one option php accepts ANY value for and screens nothing: a share` |
|       - | 2954 | `		 * handle of either class is attached, and everything else -- a null, an` |
|       - | 2955 | `		 * int, an array, another CurlHandle -- is taken and dropped in silence,` |
|       - | 2956 | `		 * with true answered either way. There is no getter to tell the two` |
|       - | 2957 | `		 * apart, so the only visible difference is whether the transfers then` |
|       - | 2958 | `		 * share a cache.` |
|       - | 2959 | `		 */` |
|      23 | 2960 | `		PH7_CurlSetShare(pCurl,pVal);` |
|      23 | 2961 | `		return 1;` |
|     ! 0 | 2962 | `	case CURL_OPT_IGNORE:` |
|     ! 0 | 2963 | `		return 1;` |
|     ! 0 | 2964 | `	default:` |
|     ! 0 | 2965 | `		break;` |
|       - | 2966 | `	}` |
|       - | 2967 | `	/* The kinds this build does not implement. Refusing loudly beats answering` |
|       - | 2968 | `	 * true and transferring something else. */` |
|     ! 0 | 2969 | `	*pRc = PH7_VmThrowException(pCtx,"Error",` |
|     ! 0 | 2970 | `		"%s(): option %s is not implemented yet in this build",zFunc,CurlOptName(iOpt));` |
|     ! 0 | 2971 | `	return -1;` |
|     411 | 2972 | `}` |
|       - | 2973 | `/* bool curl_setopt(CurlHandle $handle, int $option, mixed $value) */` |
|     806 | 2974 | `static int vm_builtin_curl_setopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2975 | `{` |
|     807 | 2976 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|     807 | 2977 | `	sxi32 rcOut = PH7_OK;` |
|       - | 2978 | `	int rc;` |
|     807 | 2979 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 2980 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2981 | `		return PH7_OK;` |
|       - | 2982 | `	}` |
|     807 | 2983 | `	CurlClearErr(pCurl);` |
|     807 | 2984 | `	rc = CurlSetOne(pCtx,pCurl,ph7_value_to_int64(apArg[1]),apArg[2],"curl_setopt",&rcOut);` |
|     807 | 2985 | `	ph7_result_bool(pCtx,rc == 1);` |
|     807 | 2986 | `	return rcOut;` |
|     404 | 2987 | `}` |
|       - | 2988 | `/*` |
|       - | 2989 | ` * bool curl_setopt_array(CurlHandle $handle, array $options)` |
|       - | 2990 | ` *` |
|       - | 2991 | ` * The options are applied IN ORDER and the walk stops at the first refusal --` |
|       - | 2992 | ` * a bad third entry leaves the first two applied. Its two ValueErrors are not` |
|       - | 2993 | ` * curl_setopt's and are not each other's: a key that is not an option NUMBER` |
|       - | 2994 | ` * says "must contain only valid cURL options", a key that is not an integer at` |
|       - | 2995 | ` * all says "contains an invalid cURL option". (A numeric STRING key is neither:` |
|       - | 2996 | ` * php's array normalizes it to an int before this ever sees it.)` |
|       - | 2997 | ` */` |
|       - | 2998 | `struct CurlSetoptArray {` |
|       - | 2999 | `	ph7_context *pCtx;` |
|       - | 3000 | `	phl_curl *pCurl;` |
|       - | 3001 | `	int rc;          /* 1 all applied, 0 a false, -1 a throw is installed */` |
|       - | 3002 | `	sxi32 rcOut;     /* the status a throwing coercion wants propagated */` |
|       - | 3003 | `};` |
|      20 | 3004 | `static int CurlSetoptArrayWalk(ph7_value *pKey,ph7_value *pVal,void *pUser)` |
|       1 | 3005 | `{` |
|      21 | 3006 | `	struct CurlSetoptArray *pW = (struct CurlSetoptArray *)pUser;` |
|       - | 3007 | `	int rc;` |
|      21 | 3008 | `	if( pW->rc != 1 ){` |
|     ! 0 | 3009 | `		return PH7_ABORT;` |
|       - | 3010 | `	}` |
|      21 | 3011 | `	if( !ph7_value_is_int(pKey) ){` |
|       3 | 3012 | `		pW->rcOut = PH7_VmThrowException(pW->pCtx,"ValueError",` |
|       - | 3013 | `			"curl_setopt_array(): Argument #2 ($options) contains an invalid cURL option");` |
|       3 | 3014 | `		pW->rc = -1;` |
|       3 | 3015 | `		return PH7_ABORT;` |
|       - | 3016 | `	}` |
|      19 | 3017 | `	if( CurlOptFind(ph7_value_to_int64(pKey)) == 0 ){` |
|       5 | 3018 | `		CurlSetErr(pW->pCurl,CURLE_UNKNOWN_OPTION);` |
|       5 | 3019 | `		pW->rcOut = PH7_VmThrowException(pW->pCtx,"ValueError",` |
|       - | 3020 | `			"curl_setopt_array(): Argument #2 ($options) must contain only valid cURL options");` |
|       5 | 3021 | `		pW->rc = -1;` |
|       5 | 3022 | `		return PH7_ABORT;` |
|       - | 3023 | `	}` |
|      15 | 3024 | `	rc = CurlSetOne(pW->pCtx,pW->pCurl,ph7_value_to_int64(pKey),pVal,"curl_setopt_array",&pW->rcOut);` |
|      15 | 3025 | `	if( rc != 1 ){` |
|       3 | 3026 | `		pW->rc = rc;` |
|       3 | 3027 | `		return PH7_ABORT;` |
|       - | 3028 | `	}` |
|      13 | 3029 | `	return PH7_OK;` |
|      11 | 3030 | `}` |
|      14 | 3031 | `static int vm_builtin_curl_setopt_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3032 | `{` |
|      15 | 3033 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 3034 | `	struct CurlSetoptArray sW;` |
|      15 | 3035 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 3036 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3037 | `		return PH7_OK;` |
|       - | 3038 | `	}` |
|      15 | 3039 | `	CurlClearErr(pCurl);` |
|      15 | 3040 | `	sW.pCtx = pCtx;` |
|      15 | 3041 | `	sW.pCurl = pCurl;` |
|      15 | 3042 | `	sW.rc = 1;` |
|      15 | 3043 | `	sW.rcOut = PH7_OK;` |
|      15 | 3044 | `	ph7_array_walk(apArg[1],CurlSetoptArrayWalk,&sW);` |
|      15 | 3045 | `	ph7_result_bool(pCtx,sW.rc == 1);` |
|      15 | 3046 | `	return sW.rcOut;` |
|       8 | 3047 | `}` |
|       - | 3048 |  |
|       - | 3049 | `/*` |
|       - | 3050 | ` * CurlHandle\|false curl_copy_handle(CurlHandle $handle)` |
|       - | 3051 | ` *` |
|       - | 3052 | `` * The same duphandle `clone` does, through a function -- php's two spellings`` |
|       - | 3053 | ` * of one operation, and they answer alike down to the clean error state on the` |
|       - | 3054 | ` * copy.` |
|       - | 3055 | ` */` |
|      24 | 3056 | `static int vm_builtin_curl_copy_handle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3057 | `{` |
|      25 | 3058 | `	ph7_vm *pVm = pCtx->pVm;` |
|      25 | 3059 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 3060 | `	ph7_class_instance *pThis;` |
|      25 | 3061 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 3062 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3063 | `		return PH7_OK;` |
|       - | 3064 | `	}` |
|      25 | 3065 | `	pThis = PH7_CurlNewInstance(pVm,"CurlHandle",sizeof("CurlHandle")-1);` |
|      25 | 3066 | `	if( pThis == 0 ){` |
|     ! 0 | 3067 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3068 | `		return PH7_OK;` |
|       - | 3069 | `	}` |
|      25 | 3070 | `	CurlInstanceClone(pVm,pThis,(ph7_class_instance *)apArg[0]->x.pOther);` |
|      25 | 3071 | `	if( CurlOfInstance(pThis) == 0 ){` |
|       - | 3072 | `		/* the dup failed; hand back php's false rather than an empty handle */` |
|     ! 0 | 3073 | `		PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3074 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3075 | `		return PH7_OK;` |
|       - | 3076 | `	}` |
|      25 | 3077 | `	PH7_NativeResultObject(pCtx,pThis);` |
|      25 | 3078 | `	return PH7_OK;` |
|      13 | 3079 | `}` |
|       - | 3080 | `/* int curl_errno(CurlHandle $handle) */` |
|     130 | 3081 | `static int vm_builtin_curl_errno(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3082 | `{` |
|     131 | 3083 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|     131 | 3084 | `	ph7_result_int(pCtx,pCurl ? pCurl->iLastErr : 0);` |
|     131 | 3085 | `	return PH7_OK;` |
|       1 | 3086 | `}` |
|       - | 3087 | `/*` |
|       - | 3088 | ` * string curl_error(CurlHandle $handle)` |
|       - | 3089 | ` *` |
|       - | 3090 | ` * The ERROR BUFFER, not curl_easy_strerror(): libcurl writes a sentence naming` |
|       - | 3091 | ` * the host and port it could not reach, where the code's own text is the` |
|       - | 3092 | ` * generic "Couldn't connect to server".` |
|       - | 3093 | ` *` |
|       - | 3094 | ` * It is gated on the recorded CODE, not on the buffer: php answers the empty` |
|       - | 3095 | ` * string whenever curl_errno() is 0, whatever libcurl left behind. The two` |
|       - | 3096 | ` * disagree on the multi rail, where a transfer can be over and its buffer` |
|       - | 3097 | ` * written while the handle's code is still 0 -- reading the message is what` |
|       - | 3098 | ` * moves the result onto the handle, and until then php reports nothing.` |
|       - | 3099 | ` */` |
|      26 | 3100 | `static int vm_builtin_curl_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3101 | `{` |
|      27 | 3102 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|      27 | 3103 | `	if( pCurl == 0 \|\| pCurl->iLastErr == 0 ){` |
|      19 | 3104 | `		ph7_result_string(pCtx,"",0);` |
|      19 | 3105 | `		return PH7_OK;` |
|       - | 3106 | `	}` |
|       9 | 3107 | `	ph7_result_string(pCtx,pCurl->zErrBuf,-1);` |
|       9 | 3108 | `	return PH7_OK;` |
|      14 | 3109 | `}` |
|       - | 3110 |  |
|       - | 3111 |  |
|       - | 3112 | `/* ===== curl_getinfo() ===== */` |
|       - | 3113 |  |
|       - | 3114 | `/*` |
|       - | 3115 | ` * A CURLINFO_* carries its own TYPE in the number: libcurl masks the top bits` |
|       - | 3116 | ` * with CURLINFO_TYPEMASK, and every one of the 75 selectors php exposes agrees` |
|       - | 3117 | ` * with its bucket -- string, long, double, slist/pointer, off_t. So the` |
|       - | 3118 | ` * selector form needs no table at all, only the mask, and a selector a newer` |
|       - | 3119 | ` * libcurl adds works the moment its constant exists.` |
|       - | 3120 | ` *` |
|       - | 3121 | ` * The eight names that are NOT infos (CURLINFO_TEXT, HEADER_IN, DATA_OUT and` |
|       - | 3122 | ` * the rest of the DEBUGFUNCTION set, plus CURLINFO_LASTONE) fall outside every` |
|       - | 3123 | ` * bucket, which is exactly why php answers false for them -- the same false an` |
|       - | 3124 | ` * unknown number gets. No throw, ever, for any selector.` |
|       - | 3125 | ` */` |
|     154 | 3126 | `static int CurlInfoOne(ph7_context *pCtx,phl_curl *pCurl,sxi64 iInfo)` |
|       1 | 3127 | `{` |
|     155 | 3128 | `	CURL *pE = pCurl->pEasy;` |
|     155 | 3129 | `	if( iInfo == CURLINFO_PRIVATE ){` |
|       - | 3130 | `		/* Not libcurl's private pointer, which php never sets: the php VALUE` |
|       - | 3131 | `		 * CURLOPT_PRIVATE stored, and FALSE when there was none. */` |
|      27 | 3132 | `		if( pCurl->pPrivate ){` |
|      25 | 3133 | `			ph7_result_value(pCtx,pCurl->pPrivate);` |
|      13 | 3134 | `		}else{` |
|       3 | 3135 | `			ph7_result_bool(pCtx,0);` |
|       - | 3136 | `		}` |
|      27 | 3137 | `		return PH7_OK;` |
|       - | 3138 | `	}` |
|     129 | 3139 | `	switch( (int)(iInfo & CURLINFO_TYPEMASK) ){` |
|      18 | 3140 | `	case CURLINFO_STRING: {` |
|      37 | 3141 | `		char *zVal = 0;` |
|      37 | 3142 | `		if( curl_easy_getinfo(pE,(CURLINFO)iInfo,&zVal) != CURLE_OK \|\| zVal == 0 ){` |
|       - | 3143 | `			/* libcurl had nothing for it: php's answer is false, not "". */` |
|       5 | 3144 | `			ph7_result_bool(pCtx,0);` |
|       3 | 3145 | `		}else{` |
|      33 | 3146 | `			ph7_result_string(pCtx,zVal,-1);` |
|       - | 3147 | `		}` |
|      37 | 3148 | `		return PH7_OK;` |
|       - | 3149 | `	}` |
|      17 | 3150 | `	case CURLINFO_LONG: {` |
|      35 | 3151 | `		long iVal = 0;` |
|      35 | 3152 | `		if( curl_easy_getinfo(pE,(CURLINFO)iInfo,&iVal) != CURLE_OK ){` |
|     ! 0 | 3153 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3154 | `		}else{` |
|      35 | 3155 | `			ph7_result_int64(pCtx,(sxi64)iVal);` |
|       - | 3156 | `		}` |
|      35 | 3157 | `		return PH7_OK;` |
|       - | 3158 | `	}` |
|      10 | 3159 | `	case CURLINFO_DOUBLE: {` |
|      21 | 3160 | `		double rVal = 0.0;` |
|      21 | 3161 | `		if( curl_easy_getinfo(pE,(CURLINFO)iInfo,&rVal) != CURLE_OK ){` |
|     ! 0 | 3162 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3163 | `		}else{` |
|      21 | 3164 | `			ph7_result_double(pCtx,rVal);` |
|       - | 3165 | `		}` |
|      21 | 3166 | `		return PH7_OK;` |
|       - | 3167 | `	}` |
|       1 | 3168 | `	case CURLINFO_OFF_T: {` |
|       3 | 3169 | `		curl_off_t iVal = 0;` |
|       3 | 3170 | `		if( curl_easy_getinfo(pE,(CURLINFO)iInfo,&iVal) != CURLE_OK ){` |
|     ! 0 | 3171 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3172 | `		}else{` |
|       3 | 3173 | `			ph7_result_int64(pCtx,(sxi64)iVal);` |
|       - | 3174 | `		}` |
|       3 | 3175 | `		return PH7_OK;` |
|       - | 3176 | `	}` |
|      10 | 3177 | `	case CURLINFO_SLIST: {` |
|       - | 3178 | `		/* Two shapes share this bucket. CERTINFO is a struct curl_certinfo,` |
|       - | 3179 | `		 * NOT a curl_slist, so reading it as one would walk the wrong type --` |
|       - | 3180 | `		 * php answers an array of per-certificate arrays for it and a flat` |
|       - | 3181 | `		 * array of strings for the other two. */` |
|      21 | 3182 | `		if( iInfo == CURLINFO_CERTINFO ){` |
|      19 | 3183 | `			struct curl_certinfo *pCi = 0;` |
|      19 | 3184 | `			ph7_value *pOut = ph7_context_new_array(pCtx);` |
|       - | 3185 | `			ph7_value *pRow;` |
|       - | 3186 | `			int i;` |
|      19 | 3187 | `			if( pOut == 0 ){` |
|     ! 0 | 3188 | `				ph7_result_bool(pCtx,0);` |
|     ! 0 | 3189 | `				return PH7_OK;` |
|       - | 3190 | `			}` |
|      19 | 3191 | `			if( curl_easy_getinfo(pE,CURLINFO_CERTINFO,&pCi) == CURLE_OK && pCi ){` |
|      19 | 3192 | `				for( i = 0 ; i < pCi->num_of_certs ; ++i ){` |
|     ! 0 | 3193 | `					struct curl_slist *pWalk = pCi->certinfo[i];` |
|     ! 0 | 3194 | `					pRow = ph7_context_new_array(pCtx);` |
|     ! 0 | 3195 | `					if( pRow == 0 ){ break; }` |
|     ! 0 | 3196 | `					while( pWalk ){` |
|     ! 0 | 3197 | `						ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     ! 0 | 3198 | `						if( pElem == 0 ){ break; }` |
|     ! 0 | 3199 | `						ph7_value_string(pElem,pWalk->data ? pWalk->data : "",-1);` |
|     ! 0 | 3200 | `						ph7_array_add_elem(pRow,0,pElem);` |
|     ! 0 | 3201 | `						ph7_context_release_value(pCtx,pElem);` |
|     ! 0 | 3202 | `						pWalk = pWalk->next;` |
|     ! 0 | 3203 | `					}` |
|     ! 0 | 3204 | `					ph7_array_add_elem(pOut,0,pRow);` |
|     ! 0 | 3205 | `					ph7_context_release_value(pCtx,pRow);` |
|     ! 0 | 3206 | `				}` |
|       9 | 3207 | `			}` |
|      19 | 3208 | `			ph7_result_value(pCtx,pOut);` |
|      19 | 3209 | `			return PH7_OK;` |
|     ! 0 | 3210 | `		}else{` |
|       3 | 3211 | `			struct curl_slist *pList = 0;` |
|       3 | 3212 | `			ph7_value *pOut = ph7_context_new_array(pCtx);` |
|       3 | 3213 | `			if( pOut == 0 ){` |
|     ! 0 | 3214 | `				ph7_result_bool(pCtx,0);` |
|     ! 0 | 3215 | `				return PH7_OK;` |
|       - | 3216 | `			}` |
|       3 | 3217 | `			if( curl_easy_getinfo(pE,(CURLINFO)iInfo,&pList) == CURLE_OK && pList ){` |
|     ! 0 | 3218 | `				struct curl_slist *pWalk = pList;` |
|     ! 0 | 3219 | `				ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     ! 0 | 3220 | `				while( pWalk && pElem ){` |
|     ! 0 | 3221 | `					ph7_value_reset_string_cursor(pElem);` |
|     ! 0 | 3222 | `					ph7_value_string(pElem,pWalk->data ? pWalk->data : "",-1);` |
|     ! 0 | 3223 | `					ph7_array_add_elem(pOut,0,pElem);` |
|     ! 0 | 3224 | `					pWalk = pWalk->next;` |
|     ! 0 | 3225 | `				}` |
|     ! 0 | 3226 | `				curl_slist_free_all(pList);` |
|     ! 0 | 3227 | `			}` |
|       3 | 3228 | `			ph7_result_value(pCtx,pOut);` |
|       3 | 3229 | `			return PH7_OK;` |
|       - | 3230 | `		}` |
|       - | 3231 | `	}` |
|       8 | 3232 | `	default:` |
|      16 | 3233 | `		break;` |
|       - | 3234 | `	}` |
|       - | 3235 | `	/* Not an info at all. */` |
|      17 | 3236 | `	ph7_result_bool(pCtx,0);` |
|      17 | 3237 | `	return PH7_OK;` |
|      78 | 3238 | `}` |
|       - | 3239 |  |
|       - | 3240 | `/*` |
|       - | 3241 | ` * The no-selector array: php's 41 entries, in php's order, each from the` |
|       - | 3242 | ` * selector named beside it. Five more exist only past a libcurl version, and` |
|       - | 3243 | ` * php gates them on the HEADER's version; so do these rows, so a build against` |
|       - | 3244 | ` * a newer libcurl answers php's 46. The order is user-visible through print_r and a` |
|       - | 3245 | ` * foreach, and it is not libcurl's.` |
|       - | 3246 | ` *` |
|       - | 3247 | ` * bNullStays is the row that a reading would never produce. Through a SELECTOR` |
|       - | 3248 | ` * every string info libcurl left NULL answers false; in the ARRAY they answer` |
|       - | 3249 | ` * the EMPTY STRING instead -- except content_type, which stays NULL. So the` |
|       - | 3250 | ` * same missing value is rendered three different ways depending on how it was` |
|       - | 3251 | ` * asked for, and only content_type keeps php's null here.` |
|       - | 3252 | ` */` |
|       - | 3253 | `static const struct CurlInfoKey {` |
|       - | 3254 | `	const char *zKey;` |
|       - | 3255 | `	CURLINFO iInfo;` |
|       - | 3256 | `	int bNullStays;` |
|       - | 3257 | `} aCurlInfoKey[] = {` |
|       - | 3258 | `	{ "url",                     CURLINFO_EFFECTIVE_URL,          0 },` |
|       - | 3259 | `	{ "content_type",            CURLINFO_CONTENT_TYPE,           1 },` |
|       - | 3260 | `	{ "http_code",               CURLINFO_RESPONSE_CODE,          0 },` |
|       - | 3261 | `	{ "header_size",             CURLINFO_HEADER_SIZE,            0 },` |
|       - | 3262 | `	{ "request_size",            CURLINFO_REQUEST_SIZE,           0 },` |
|       - | 3263 | `	{ "filetime",                CURLINFO_FILETIME,               0 },` |
|       - | 3264 | `	{ "ssl_verify_result",       CURLINFO_SSL_VERIFYRESULT,       0 },` |
|       - | 3265 | `	{ "redirect_count",          CURLINFO_REDIRECT_COUNT,         0 },` |
|       - | 3266 | `	{ "total_time",              CURLINFO_TOTAL_TIME,             0 },` |
|       - | 3267 | `	{ "namelookup_time",         CURLINFO_NAMELOOKUP_TIME,        0 },` |
|       - | 3268 | `	{ "connect_time",            CURLINFO_CONNECT_TIME,           0 },` |
|       - | 3269 | `	{ "pretransfer_time",        CURLINFO_PRETRANSFER_TIME,       0 },` |
|       - | 3270 | `	{ "size_upload",             CURLINFO_SIZE_UPLOAD,            0 },` |
|       - | 3271 | `	{ "size_download",           CURLINFO_SIZE_DOWNLOAD,          0 },` |
|       - | 3272 | `	{ "speed_download",          CURLINFO_SPEED_DOWNLOAD,         0 },` |
|       - | 3273 | `	{ "speed_upload",            CURLINFO_SPEED_UPLOAD,           0 },` |
|       - | 3274 | `	{ "download_content_length", CURLINFO_CONTENT_LENGTH_DOWNLOAD,0 },` |
|       - | 3275 | `	{ "upload_content_length",   CURLINFO_CONTENT_LENGTH_UPLOAD,  0 },` |
|       - | 3276 | `	{ "starttransfer_time",      CURLINFO_STARTTRANSFER_TIME,     0 },` |
|       - | 3277 | `	{ "redirect_time",           CURLINFO_REDIRECT_TIME,          0 },` |
|       - | 3278 | `	{ "redirect_url",            CURLINFO_REDIRECT_URL,           0 },` |
|       - | 3279 | `	{ "primary_ip",              CURLINFO_PRIMARY_IP,             0 },` |
|       - | 3280 | `	{ "certinfo",                CURLINFO_CERTINFO,               0 },` |
|       - | 3281 | `	{ "primary_port",            CURLINFO_PRIMARY_PORT,           0 },` |
|       - | 3282 | `	{ "local_ip",                CURLINFO_LOCAL_IP,               0 },` |
|       - | 3283 | `	{ "local_port",              CURLINFO_LOCAL_PORT,             0 },` |
|       - | 3284 | `	{ "http_version",            CURLINFO_HTTP_VERSION,           0 },` |
|       - | 3285 | `	{ "protocol",                CURLINFO_PROTOCOL,               0 },` |
|       - | 3286 | `	{ "ssl_verifyresult",        CURLINFO_PROXY_SSL_VERIFYRESULT, 0 },` |
|       - | 3287 | `	{ "scheme",                  CURLINFO_SCHEME,                 0 },` |
|       - | 3288 | `	{ "appconnect_time_us",      CURLINFO_APPCONNECT_TIME_T,      0 },` |
|       - | 3289 | `#if LIBCURL_VERSION_NUM >= 0x080600` |
|       - | 3290 | `	{ "queue_time_us",           CURLINFO_QUEUE_TIME_T,           0 },` |
|       - | 3291 | `#endif` |
|       - | 3292 | `	{ "connect_time_us",         CURLINFO_CONNECT_TIME_T,         0 },` |
|       - | 3293 | `	{ "namelookup_time_us",      CURLINFO_NAMELOOKUP_TIME_T,      0 },` |
|       - | 3294 | `	{ "pretransfer_time_us",     CURLINFO_PRETRANSFER_TIME_T,     0 },` |
|       - | 3295 | `	{ "redirect_time_us",        CURLINFO_REDIRECT_TIME_T,        0 },` |
|       - | 3296 | `	{ "starttransfer_time_us",   CURLINFO_STARTTRANSFER_TIME_T,   0 },` |
|       - | 3297 | `#if LIBCURL_VERSION_NUM >= 0x080a00` |
|       - | 3298 | `	{ "posttransfer_time_us",    CURLINFO_POSTTRANSFER_TIME_T,    0 },` |
|       - | 3299 | `#endif` |
|       - | 3300 | `	{ "total_time_us",           CURLINFO_TOTAL_TIME_T,           0 },` |
|       - | 3301 | `	{ "effective_method",        CURLINFO_EFFECTIVE_METHOD,       0 },` |
|       - | 3302 | `	{ "capath",                  CURLINFO_CAPATH,                 0 },` |
|       - | 3303 | `	{ "cainfo",                  CURLINFO_CAINFO,                 0 },` |
|       - | 3304 | `#if LIBCURL_VERSION_NUM >= 0x080700` |
|       - | 3305 | `	{ "used_proxy",              CURLINFO_USED_PROXY,             0 },` |
|       - | 3306 | `#endif` |
|       - | 3307 | `#if LIBCURL_VERSION_NUM >= 0x080c00` |
|       - | 3308 | `	{ "httpauth_used",           CURLINFO_HTTPAUTH_USED,          0 },` |
|       - | 3309 | `	{ "proxyauth_used",          CURLINFO_PROXYAUTH_USED,         0 },` |
|       - | 3310 | `#endif` |
|       - | 3311 | `	{ "conn_id",                 CURLINFO_CONN_ID,                0 }` |
|       - | 3312 | `};` |
|       - | 3313 |  |
|       - | 3314 | `/* mixed curl_getinfo(CurlHandle $handle, ?int $option = null) */` |
|     154 | 3315 | `static int vm_builtin_curl_getinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3316 | `{` |
|     155 | 3317 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 3318 | `	ph7_value *pArray,*pWorker;` |
|       - | 3319 | `	sxu32 n;` |
|     155 | 3320 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 3321 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3322 | `		return PH7_OK;` |
|       - | 3323 | `	}` |
|     155 | 3324 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     139 | 3325 | `		return CurlInfoOne(pCtx,pCurl,ph7_value_to_int64(apArg[1]));` |
|       - | 3326 | `	}` |
|      17 | 3327 | `	pArray  = ph7_context_new_array(pCtx);` |
|      17 | 3328 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|      17 | 3329 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|     ! 0 | 3330 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3331 | `		return PH7_OK;` |
|       - | 3332 | `	}` |
|     713 | 3333 | `	for( n = 0 ; n < SX_ARRAYSIZE(aCurlInfoKey) ; ++n ){` |
|     697 | 3334 | `		const struct CurlInfoKey *pK = &aCurlInfoKey[n];` |
|     697 | 3335 | `		switch( (int)(pK->iInfo & CURLINFO_TYPEMASK) ){` |
|      72 | 3336 | `		case CURLINFO_STRING: {` |
|     145 | 3337 | `			char *zVal = 0;` |
|     145 | 3338 | `			PH7_MemObjRelease(pWorker);` |
|     145 | 3339 | `			if( curl_easy_getinfo(pCurl->pEasy,pK->iInfo,&zVal) != CURLE_OK \|\| zVal == 0 ){` |
|      65 | 3340 | `				if( pK->bNullStays ){` |
|      17 | 3341 | `					ph7_value_null(pWorker);` |
|       9 | 3342 | `				}else{` |
|      49 | 3343 | `					ph7_value_string(pWorker,"",0);` |
|       - | 3344 | `				}` |
|      41 | 3345 | `			}else{` |
|      81 | 3346 | `				ph7_value_string(pWorker,zVal,-1);` |
|       - | 3347 | `			}` |
|     145 | 3348 | `			break;` |
|       - | 3349 | `		}` |
|      88 | 3350 | `		case CURLINFO_LONG: {` |
|     201 | 3351 | `			long iVal = 0;` |
|     201 | 3352 | `			curl_easy_getinfo(pCurl->pEasy,pK->iInfo,&iVal);` |
|     201 | 3353 | `			PH7_MemObjRelease(pWorker);` |
|     201 | 3354 | `			ph7_value_int64(pWorker,(sxi64)iVal);` |
|     201 | 3355 | `			break;` |
|       - | 3356 | `		}` |
|      96 | 3357 | `		case CURLINFO_DOUBLE: {` |
|     193 | 3358 | `			double rVal = 0.0;` |
|     193 | 3359 | `			curl_easy_getinfo(pCurl->pEasy,pK->iInfo,&rVal);` |
|     193 | 3360 | `			PH7_MemObjRelease(pWorker);` |
|     193 | 3361 | `			ph7_value_double(pWorker,rVal);` |
|     193 | 3362 | `			break;` |
|       - | 3363 | `		}` |
|      64 | 3364 | `		case CURLINFO_OFF_T: {` |
|     145 | 3365 | `			curl_off_t iVal = 0;` |
|     145 | 3366 | `			curl_easy_getinfo(pCurl->pEasy,pK->iInfo,&iVal);` |
|     145 | 3367 | `			PH7_MemObjRelease(pWorker);` |
|     145 | 3368 | `			ph7_value_int64(pWorker,(sxi64)iVal);` |
|     145 | 3369 | `			break;` |
|       - | 3370 | `		}` |
|       8 | 3371 | `		default: {` |
|       - | 3372 | `			/* certinfo, the one array in the table: build it through the` |
|       - | 3373 | `			 * selector path so the two answers cannot drift apart. */` |
|      17 | 3374 | `			ph7_value *pSaved = pCtx->pRet;` |
|       8 | 3375 | `			SXUNUSED(pSaved);` |
|      17 | 3376 | `			CurlInfoOne(pCtx,pCurl,(sxi64)pK->iInfo);` |
|      17 | 3377 | `			PH7_MemObjRelease(pWorker);` |
|      17 | 3378 | `			PH7_MemObjStore(pCtx->pRet,pWorker);` |
|      16 | 3379 | `			break;` |
|       - | 3380 | `		}` |
|       - | 3381 | `		}` |
|     697 | 3382 | `		ph7_array_add_strkey_elem(pArray,pK->zKey,pWorker);` |
|     369 | 3383 | `	}` |
|      17 | 3384 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3385 | `	return PH7_OK;` |
|      78 | 3386 | `}` |
|       - | 3387 |  |
|       - | 3388 |  |
|       - | 3389 |  |
|       - | 3390 | `/* ===== The callbacks ===== */` |
|       - | 3391 |  |
|       - | 3392 | `/*` |
|       - | 3393 | ` * The rule every callback here shares, and the reason they share one shape:` |
|       - | 3394 | ` * libcurl is in the middle of a transfer when the engine re-enters PHP, and a` |
|       - | 3395 | ` * throw out of that PHP cannot travel back through libcurl's C frames. So the` |
|       - | 3396 | ` * status is PARKED on the handle and curl_exec() raises exactly that status` |
|       - | 3397 | ` * once the library has unwound. A second callback while one is parked does not` |
|       - | 3398 | ` * re-enter PHP at all.` |
|       - | 3399 | ` *` |
|       - | 3400 | ` * WHAT THE THROW ANSWERS THE LIBRARY. Not a failure: php hands libcurl the` |
|       - | 3401 | ` * value that means CARRY ON -- the chunk's own length for a writer, 0 for the` |
|       - | 3402 | ` * progress and prerequisite callbacks -- so a callback that threw does not stop` |
|       - | 3403 | ` * the transfer. The whole body still arrives, the status line is still read,` |
|       - | 3404 | ` * and only the exception says anything happened: curl_exec() answers false with` |
|       - | 3405 | ` * it, and curl_errno() reports whatever the completed transfer really ended in` |
|       - | 3406 | ` * (0 for one that succeeded). Answering a FAILURE instead was silently visible` |
|       - | 3407 | ` * in three places -- CURLINFO_SIZE_DOWNLOAD went to 0, a prerequisite throw` |
|       - | 3408 | ` * left CURLINFO_HTTP_CODE at 0, and under a multi handle` |
|       - | 3409 | ` * curl_multi_info_read() reported CURLE_WRITE_ERROR where php reports` |
|       - | 3410 | ` * CURLE_OK.` |
|       - | 3411 | ` *` |
|       - | 3412 | ` * The one callback that is NOT on this rail is the READ one: php's own answer` |
|       - | 3413 | ` * there leaves the transfer waiting for the length it declared (recorded), so it` |
|       - | 3414 | ` * keeps its own shape.` |
|       - | 3415 | ` */` |
|     182 | 3416 | `static int CurlCbParked(phl_curl *pCurl)` |
|       1 | 3417 | `{` |
|     183 | 3418 | `	return pCurl->iCbExc != 0;` |
|       1 | 3419 | `}` |
|      16 | 3420 | `static void CurlCbPark(phl_curl *pCurl,sxi32 rc)` |
|       1 | 3421 | `{` |
|      17 | 3422 | `	pCurl->iCbExc = PH7_CALLBACK_UNWOUND(rc) ? rc : PH7_EXCEPTION;` |
|      17 | 3423 | `}` |
|       - | 3424 | `/*` |
|       - | 3425 | ` * Store one callable on the handle. php accepts every callable SHAPE here --` |
|       - | 3426 | ` * a closure, a function name, both array forms, "Class::method", an invokable` |
|       - | 3427 | ` * object -- and null, which puts the default back. Anything else is a TypeError` |
|       - | 3428 | ` * whose tail says which way it was wrong.` |
|       - | 3429 | ` */` |
|     114 | 3430 | `PH7_PRIVATE int PH7_CurlSetCallback(ph7_context *pCtx,ph7_vm *pVm,ph7_value **ppSlot,` |
|       - | 3431 | `	ph7_value *pVal,const char *zFunc,const char *zArg,const char *zOpt,int bNullClears,` |
|       - | 3432 | `	sxi32 *pRc)` |
|       1 | 3433 | `{` |
|     115 | 3434 | `	if( bNullClears && ph7_value_is_null(pVal) ){` |
|       5 | 3435 | `		if( *ppSlot ){` |
|       3 | 3436 | `			ph7_release_value(pVm,*ppSlot);` |
|       3 | 3437 | `			*ppSlot = 0;` |
|       1 | 3438 | `		}` |
|       5 | 3439 | `		return 1;` |
|       - | 3440 | `	}` |
|     111 | 3441 | `	if( !ph7_value_is_string(pVal) && !ph7_value_is_array(pVal) && !ph7_value_is_object(pVal) ){` |
|      10 | 3442 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3443 | `			"%s(): Argument %s must be a valid callback for option %s, "` |
|       3 | 3444 | `			"no array or string given",zFunc,zArg,zOpt);` |
|       7 | 3445 | `		return -1;` |
|       - | 3446 | `	}` |
|     105 | 3447 | `	if( ph7_value_is_array(pVal) && ph7_array_count(pVal) != 2 ){` |
|      10 | 3448 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3449 | `			"%s(): Argument %s must be a valid callback for option %s, "` |
|       3 | 3450 | `			"array callback must have exactly two members",zFunc,zArg,zOpt);` |
|       7 | 3451 | `		return -1;` |
|       - | 3452 | `	}` |
|      99 | 3453 | `	if( !PH7_VmIsCallable(pVm,pVal,FALSE) ){` |
|       7 | 3454 | `		if( ph7_value_is_string(pVal) ){` |
|       5 | 3455 | `			int nName = 0;` |
|       5 | 3456 | `			const char *zName = ph7_value_to_string(pVal,&nName);` |
|       7 | 3457 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3458 | `				"%s(): Argument %s must be a valid callback for option %s, "` |
|       - | 3459 | `				"function \"%.*s\" not found or invalid function name",` |
|       2 | 3460 | `				zFunc,zArg,zOpt,nName,zName);` |
|       3 | 3461 | `		}else{` |
|       4 | 3462 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3463 | `				"%s(): Argument %s must be a valid callback for option %s, "` |
|       1 | 3464 | `				"no array or string given",zFunc,zArg,zOpt);` |
|       - | 3465 | `		}` |
|       7 | 3466 | `		return -1;` |
|       - | 3467 | `	}` |
|      93 | 3468 | `	if( *ppSlot ){` |
|       3 | 3469 | `		ph7_release_value(pVm,*ppSlot);` |
|       1 | 3470 | `	}` |
|       - | 3471 | `	/* The handle owns its own copy: libcurl may call this long after the` |
|       - | 3472 | `	 * caller's value is gone. */` |
|      93 | 3473 | `	*ppSlot = ph7_new_scalar(pVm);` |
|      93 | 3474 | `	if( *ppSlot == 0 ){` |
|     ! 0 | 3475 | `		return 0;` |
|       - | 3476 | `	}` |
|      93 | 3477 | `	PH7_MemObjStore(pVal,*ppSlot);` |
|      93 | 3478 | `	return 1;` |
|      58 | 3479 | `}` |
|       - | 3480 | `/* The body/header sink: php hands the callback (handle, chunk) and reads the` |
|       - | 3481 | ` * BYTE COUNT back. Anything but the chunk's own length stops the transfer with` |
|       - | 3482 | ` * CURLE_WRITE_ERROR -- 0, -1, true and a non-numeric string alike. */` |
|      82 | 3483 | `static size_t CurlCbWrite(char *zData,size_t nSize,size_t nMemb,void *pUser,` |
|       - | 3484 | `	phl_curl *pCurl,ph7_value *pCb)` |
|       1 | 3485 | `{` |
|      83 | 3486 | `	ph7_vm *pVm = pCurl->pVm;` |
|      83 | 3487 | `	size_t nTotal = nSize * nMemb;` |
|       - | 3488 | `	ph7_value sArgs[2],sRes,*apArg[2];` |
|       - | 3489 | `	sxi32 rc;` |
|       - | 3490 | `	size_t nOut;` |
|      41 | 3491 | `	SXUNUSED(pUser);` |
|      83 | 3492 | `	if( CurlCbParked(pCurl) ){` |
|      12 | 3493 | `		return nTotal;   /* parked: consumed, so the transfer carries on */` |
|       - | 3494 | `	}` |
|      71 | 3495 | `	PH7_MemObjInit(pVm,&sArgs[0]);` |
|      71 | 3496 | `	PH7_MemObjInit(pVm,&sArgs[1]);` |
|      71 | 3497 | `	PH7_MemObjInit(pVm,&sRes);` |
|      71 | 3498 | `	if( pCurl->pOwner ){` |
|      71 | 3499 | `		sArgs[0].x.pOther = pCurl->pOwner;` |
|      71 | 3500 | `		sArgs[0].iFlags = MEMOBJ_OBJ;` |
|      71 | 3501 | `		pCurl->pOwner->iRef++;   /* the argument holds a reference for the call */` |
|      35 | 3502 | `	}` |
|      71 | 3503 | `	ph7_value_string(&sArgs[1],zData,(int)nTotal);` |
|      71 | 3504 | `	apArg[0] = &sArgs[0];` |
|      71 | 3505 | `	apArg[1] = &sArgs[1];` |
|      71 | 3506 | `	rc = PH7_VmCallUserFunction(pVm,pCb,2,apArg,&sRes);` |
|      71 | 3507 | `	if( rc != SXRET_OK ){` |
|      11 | 3508 | `		CurlCbPark(pCurl,rc);` |
|      11 | 3509 | `		nOut = nTotal;` |
|       6 | 3510 | `	}else{` |
|      61 | 3511 | `		nOut = (size_t)ph7_value_to_int64(&sRes);` |
|       - | 3512 | `	}` |
|      71 | 3513 | `	PH7_MemObjRelease(&sRes);` |
|      71 | 3514 | `	PH7_MemObjRelease(&sArgs[0]);` |
|      71 | 3515 | `	PH7_MemObjRelease(&sArgs[1]);` |
|      71 | 3516 | `	return nOut;` |
|      42 | 3517 | `}` |
|      34 | 3518 | `static size_t CurlWriteThunk(char *zData,size_t nSize,size_t nMemb,void *pUser)` |
|       1 | 3519 | `{` |
|      35 | 3520 | `	phl_curl *pCurl = (phl_curl *)pUser;` |
|      35 | 3521 | `	return CurlCbWrite(zData,nSize,nMemb,pUser,pCurl,pCurl->pWriteCb);` |
|       1 | 3522 | `}` |
|      48 | 3523 | `static size_t CurlHeaderThunk(char *zData,size_t nSize,size_t nMemb,void *pUser)` |
|     ! 0 | 3524 | `{` |
|      48 | 3525 | `	phl_curl *pCurl = (phl_curl *)pUser;` |
|      48 | 3526 | `	return CurlCbWrite(zData,nSize,nMemb,pUser,pCurl,pCurl->pHeaderCb);` |
|     ! 0 | 3527 | `}` |
|       - | 3528 | `/*` |
|       - | 3529 | ` * The progress callback, in php's two spellings. XFERINFOFUNCTION takes the` |
|       - | 3530 | ` * five (handle, dltotal, dlnow, ultotal, ulnow) as INTs; PROGRESSFUNCTION is` |
|       - | 3531 | ` * the same five and the same order -- php passes the modern shape to both.` |
|       - | 3532 | ` * A non-zero return aborts the transfer (CURLE_ABORTED_BY_CALLBACK) -- which` |
|       - | 3533 | ` * is why a THROW answers zero here: php's transfer runs to completion with the` |
|       - | 3534 | ` * exception waiting for the call to unwind.` |
|       - | 3535 | ` */` |
|      62 | 3536 | `static int CurlXferThunk(void *pUser,curl_off_t dlTotal,curl_off_t dlNow,` |
|       - | 3537 | `	curl_off_t ulTotal,curl_off_t ulNow)` |
|       1 | 3538 | `{` |
|      63 | 3539 | `	phl_curl *pCurl = (phl_curl *)pUser;` |
|      63 | 3540 | `	ph7_vm *pVm = pCurl->pVm;` |
|       - | 3541 | `	ph7_value sArgs[5],sRes,*apArg[5];` |
|       - | 3542 | `	sxi32 rc;` |
|       - | 3543 | `	int i,iOut;` |
|      63 | 3544 | `	if( CurlCbParked(pCurl) \|\| pCurl->pXferCb == 0 ){` |
|      19 | 3545 | `		return 0;` |
|       - | 3546 | `	}` |
|     259 | 3547 | `	for( i = 0 ; i < 5 ; ++i ){` |
|     216 | 3548 | `		PH7_MemObjInit(pVm,&sArgs[i]);` |
|     216 | 3549 | `		apArg[i] = &sArgs[i];` |
|     101 | 3550 | `	}` |
|      44 | 3551 | `	if( pCurl->pOwner ){` |
|      44 | 3552 | `		sArgs[0].x.pOther = pCurl->pOwner;` |
|      44 | 3553 | `		sArgs[0].iFlags = MEMOBJ_OBJ;` |
|      44 | 3554 | `		pCurl->pOwner->iRef++;   /* the argument holds a reference for the call */` |
|      20 | 3555 | `	}` |
|      44 | 3556 | `	ph7_value_int64(&sArgs[1],(sxi64)dlTotal);` |
|      44 | 3557 | `	ph7_value_int64(&sArgs[2],(sxi64)dlNow);` |
|      44 | 3558 | `	ph7_value_int64(&sArgs[3],(sxi64)ulTotal);` |
|      44 | 3559 | `	ph7_value_int64(&sArgs[4],(sxi64)ulNow);` |
|      44 | 3560 | `	PH7_MemObjInit(pVm,&sRes);` |
|      44 | 3561 | `	rc = PH7_VmCallUserFunction(pVm,pCurl->pXferCb,5,apArg,&sRes);` |
|      44 | 3562 | `	if( rc != SXRET_OK ){` |
|       2 | 3563 | `		CurlCbPark(pCurl,rc);` |
|       2 | 3564 | `		iOut = 0;` |
|       1 | 3565 | `	}else{` |
|      42 | 3566 | `		iOut = (int)ph7_value_to_int64(&sRes);` |
|       - | 3567 | `	}` |
|      44 | 3568 | `	PH7_MemObjRelease(&sRes);` |
|     259 | 3569 | `	for( i = 0 ; i < 5 ; ++i ){` |
|     216 | 3570 | `		PH7_MemObjRelease(&sArgs[i]);` |
|     101 | 3571 | `	}` |
|      44 | 3572 | `	return iOut;` |
|      29 | 3573 | `}` |
|       - | 3574 |  |
|       - | 3575 | `/*` |
|       - | 3576 | ` * The stream SINKS: a body or a header block written to a php stream. A write` |
|       - | 3577 | ` * that does not take everything stops the transfer, which is libcurl's rule` |
|       - | 3578 | ` * for any short write, and a stream the script closed after naming it is the` |
|       - | 3579 | ` * same short write rather than a crash.` |
|       - | 3580 | ` */` |
|      20 | 3581 | `static size_t CurlStreamSink(char *zData,size_t nSize,size_t nMemb,void *pUser)` |
|       1 | 3582 | `{` |
|      21 | 3583 | `	io_private *pDev = CurlStreamOf((ph7_value *)pUser);` |
|      21 | 3584 | `	size_t nTotal = nSize * nMemb;` |
|       - | 3585 | `	ph7_int64 n;` |
|      21 | 3586 | `	if( pDev == 0 \|\| nTotal < 1 ){` |
|     ! 0 | 3587 | `		return pDev == 0 ? 0 : nTotal;` |
|       - | 3588 | `	}` |
|      21 | 3589 | `	n = PH7_StreamWrite(pDev,zData,(ph7_int64)nTotal);` |
|      21 | 3590 | `	return n < 0 ? 0 : (size_t)n;` |
|      11 | 3591 | `}` |
|       - | 3592 | `/*` |
|       - | 3593 | ` * The header sink, which has one job more than the body's: php installs a` |
|       - | 3594 | ` * header callback on EVERY transfer so that the headers do not fall through to` |
|       - | 3595 | ` * the body's destination, and the callback drops them when nothing asked.` |
|       - | 3596 | ` */` |
|     998 | 3597 | `static size_t CurlHeaderSink(char *zData,size_t nSize,size_t nMemb,void *pUser)` |
|       1 | 3598 | `{` |
|     999 | 3599 | `	phl_curl *pCurl = (phl_curl *)pUser;` |
|     999 | 3600 | `	size_t nTotal = nSize * nMemb;` |
|     999 | 3601 | `	if( pCurl->iHeaderDest == PHL_CURL_HDR_FILE ){` |
|      14 | 3602 | `		return CurlStreamSink(zData,nSize,nMemb,(void *)pCurl->pHeaderStream);` |
|       - | 3603 | `	}` |
|     985 | 3604 | `	return nTotal;   /* IGNORE: read and dropped, which is php's default */` |
|     513 | 3605 | `}` |
|       - | 3606 | `/*` |
|       - | 3607 | ` * The upload SOURCE. php has two: a stream it reads itself, and a callback it` |
|       - | 3608 | ` * asks for the bytes -- and the callback is handed the stream too, as its` |
|       - | 3609 | ` * second argument, so the documented idiom is a callback that fread()s the` |
|       - | 3610 | ` * handle it was given.` |
|       - | 3611 | ` *` |
|       - | 3612 | ` * A callback that THROWS ends the read without aborting the transfer: php` |
|       - | 3613 | ` * answers zero bytes, so libcurl waits for the length that was declared and` |
|       - | 3614 | ` * the transfer ends in CURLE_OPERATION_TIMEDOUT with the exception on top.` |
|       - | 3615 | ` * That is measured, not chosen -- the write callback's own throw aborts.` |
|       - | 3616 | ` */` |
|       8 | 3617 | `static size_t CurlReadThunk(char *zBuf,size_t nSize,size_t nMemb,void *pUser)` |
|     ! 0 | 3618 | `{` |
|       8 | 3619 | `	phl_curl *pCurl = (phl_curl *)pUser;` |
|       8 | 3620 | `	ph7_vm *pVm = pCurl->pVm;` |
|       8 | 3621 | `	size_t nWant = nSize * nMemb;` |
|       8 | 3622 | `	io_private *pDev = CurlStreamOf(pCurl->pReadStream);` |
|       - | 3623 | `	ph7_value sArgs[3],sRes,*apArg[3];` |
|       - | 3624 | `	const char *zOut;` |
|       8 | 3625 | `	int nOut = 0,i;` |
|       - | 3626 | `	sxi32 rc;` |
|       8 | 3627 | `	if( pCurl->pReadCb == 0 ){` |
|       - | 3628 | `		ph7_int64 n;` |
|       2 | 3629 | `		if( pDev == 0 \|\| nWant < 1 ){` |
|     ! 0 | 3630 | `			return 0;` |
|       - | 3631 | `		}` |
|       2 | 3632 | `		n = PH7_StreamRead(pDev,zBuf,(ph7_int64)nWant);` |
|       2 | 3633 | `		return n < 1 ? 0 : (size_t)n;` |
|       - | 3634 | `	}` |
|       6 | 3635 | `	if( CurlCbParked(pCurl) ){` |
|     ! 0 | 3636 | `		return 0;` |
|       - | 3637 | `	}` |
|      24 | 3638 | `	for( i = 0 ; i < 3 ; ++i ){` |
|      18 | 3639 | `		PH7_MemObjInit(pVm,&sArgs[i]);` |
|      18 | 3640 | `		apArg[i] = &sArgs[i];` |
|       9 | 3641 | `	}` |
|       6 | 3642 | `	if( pCurl->pOwner ){` |
|       6 | 3643 | `		sArgs[0].x.pOther = pCurl->pOwner;` |
|       6 | 3644 | `		sArgs[0].iFlags = MEMOBJ_OBJ;` |
|       6 | 3645 | `		pCurl->pOwner->iRef++;` |
|       3 | 3646 | `	}` |
|       6 | 3647 | `	if( pDev ){` |
|       2 | 3648 | `		ph7_value_resource(&sArgs[1],pDev);` |
|       1 | 3649 | `	}` |
|       6 | 3650 | `	ph7_value_int64(&sArgs[2],(sxi64)nWant);` |
|       6 | 3651 | `	PH7_MemObjInit(pVm,&sRes);` |
|       6 | 3652 | `	rc = PH7_VmCallUserFunction(pVm,pCurl->pReadCb,3,apArg,&sRes);` |
|       6 | 3653 | `	if( rc != SXRET_OK ){` |
|       2 | 3654 | `		CurlCbPark(pCurl,rc);` |
|       - | 3655 | `		/* Zero bytes, not an abort: libcurl goes on waiting for the length the` |
|       - | 3656 | `		 * transfer declared, so the CURLcode the handle ends with is the` |
|       - | 3657 | `		 * timeout -- and php reports it beside the exception. */` |
|       2 | 3658 | `		nOut = 0;` |
|       1 | 3659 | `	}else{` |
|       4 | 3660 | `		zOut = ph7_value_to_string(&sRes,&nOut);` |
|       4 | 3661 | `		if( nOut > (int)nWant ){` |
|     ! 0 | 3662 | `			nOut = (int)nWant;` |
|     ! 0 | 3663 | `		}` |
|       4 | 3664 | `		if( nOut > 0 ){` |
|       4 | 3665 | `			SyMemcpy(zOut,zBuf,(sxu32)nOut);` |
|       2 | 3666 | `		}` |
|       - | 3667 | `	}` |
|       6 | 3668 | `	PH7_MemObjRelease(&sRes);` |
|      24 | 3669 | `	for( i = 0 ; i < 3 ; ++i ){` |
|      18 | 3670 | `		PH7_MemObjRelease(&sArgs[i]);` |
|       9 | 3671 | `	}` |
|       6 | 3672 | `	return (size_t)(nOut < 0 ? 0 : nOut);` |
|       4 | 3673 | `}` |
|       - | 3674 | `/*` |
|       - | 3675 | ` * libcurl's trace, in php's two spellings.` |
|       - | 3676 | ` *` |
|       - | 3677 | ` * CURLOPT_STDERR takes a php stream where libcurl wants a FILE*, which no` |
|       - | 3678 | ` * portable cast produces from this engine's handles (a Windows one carries a` |
|       - | 3679 | ` * HANDLE and not a descriptor at all). So the trace is rendered here instead,` |
|       - | 3680 | ` * from libcurl's own debug callback and with libcurl's own prefixes -- the` |
|       - | 3681 | ` * same three kinds its default writer prints and in the same shape, one prefix` |
|       - | 3682 | ` * per CHUNK rather than per line.` |
|       - | 3683 | ` *` |
|       - | 3684 | ` * A php DEBUGFUNCTION takes the callback outright, and then nothing reaches` |
|       - | 3685 | ` * CURLOPT_STDERR at all: libcurl's own writer is what a debug callback` |
|       - | 3686 | ` * replaces, and php answers the same way.` |
|       - | 3687 | ` */` |
|       - | 3688 | `static const char * const azCurlTracePrefix[] = {` |
|       - | 3689 | `	"* ", "< ", "> ", "{ ", "} ", "{ ", "} "` |
|       - | 3690 | `};` |
|      52 | 3691 | `static int CurlDebugThunk(CURL *pEasy,curl_infotype eType,char *zData,size_t nSize,void *pUser)` |
|     ! 0 | 3692 | `{` |
|      52 | 3693 | `	phl_curl *pCurl = (phl_curl *)pUser;` |
|      52 | 3694 | `	ph7_vm *pVm = pCurl->pVm;` |
|      28 | 3695 | `	SXUNUSED(pEasy);` |
|      52 | 3696 | `	if( pCurl->pDebugCb ){` |
|       - | 3697 | `		ph7_value sArgs[3],sRes,*apArg[3];` |
|       - | 3698 | `		sxi32 rc;` |
|       - | 3699 | `		int i;` |
|      26 | 3700 | `		if( CurlCbParked(pCurl) ){` |
|     ! 0 | 3701 | `			return 0;` |
|       - | 3702 | `		}` |
|     104 | 3703 | `		for( i = 0 ; i < 3 ; ++i ){` |
|      78 | 3704 | `			PH7_MemObjInit(pVm,&sArgs[i]);` |
|      78 | 3705 | `			apArg[i] = &sArgs[i];` |
|      42 | 3706 | `		}` |
|      26 | 3707 | `		if( pCurl->pOwner ){` |
|      26 | 3708 | `			sArgs[0].x.pOther = pCurl->pOwner;` |
|      26 | 3709 | `			sArgs[0].iFlags = MEMOBJ_OBJ;` |
|      26 | 3710 | `			pCurl->pOwner->iRef++;` |
|      14 | 3711 | `		}` |
|      26 | 3712 | `		ph7_value_int64(&sArgs[1],(sxi64)eType);` |
|      26 | 3713 | `		ph7_value_string(&sArgs[2],zData,(int)nSize);` |
|      26 | 3714 | `		PH7_MemObjInit(pVm,&sRes);` |
|      26 | 3715 | `		rc = PH7_VmCallUserFunction(pVm,pCurl->pDebugCb,3,apArg,&sRes);` |
|      26 | 3716 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3717 | `			CurlCbPark(pCurl,rc);` |
|     ! 0 | 3718 | `		}` |
|      26 | 3719 | `		PH7_MemObjRelease(&sRes);` |
|     104 | 3720 | `		for( i = 0 ; i < 3 ; ++i ){` |
|      78 | 3721 | `			PH7_MemObjRelease(&sArgs[i]);` |
|      42 | 3722 | `		}` |
|      26 | 3723 | `		return 0;` |
|       - | 3724 | `	}` |
|       - | 3725 | `	{` |
|      26 | 3726 | `		io_private *pDev = CurlStreamOf(pCurl->pStderrStream);` |
|       - | 3727 | `		/* libcurl's own writer prints the text and the two header kinds and` |
|       - | 3728 | `		 * drops the DATA blocks; anything else here would be output php's` |
|       - | 3729 | `		 * CURLOPT_STDERR never produced. */` |
|      26 | 3730 | `		if( pDev == 0 \|\| (eType != CURLINFO_TEXT && eType != CURLINFO_HEADER_IN` |
|      11 | 3731 | `		                  && eType != CURLINFO_HEADER_OUT) ){` |
|       2 | 3732 | `			return 0;` |
|       - | 3733 | `		}` |
|      24 | 3734 | `		PH7_StreamWrite(pDev,azCurlTracePrefix[(int)eType],2);` |
|      24 | 3735 | `		if( nSize > 0 ){` |
|      24 | 3736 | `			PH7_StreamWrite(pDev,zData,(ph7_int64)nSize);` |
|      13 | 3737 | `		}` |
|       - | 3738 | `	}` |
|      24 | 3739 | `	return 0;` |
|      28 | 3740 | `}` |
|       - | 3741 |  |
|       - | 3742 | `/*` |
|       - | 3743 | ` * CURLOPT_PREREQFUNCTION: the one hook that runs BEFORE the request goes out,` |
|       - | 3744 | ` * once the connection is up. php hands it the handle and the four addresses` |
|       - | 3745 | ` * libcurl has by then -- the peer's IP and port, and this end's -- and reads` |
|       - | 3746 | ` * CURL_PREREQFUNC_OK or CURL_PREREQFUNC_ABORT back; the abort is the` |
|       - | 3747 | ` * CURLE_ABORTED_BY_CALLBACK every other refusing callback answers with.` |
|       - | 3748 | ` *` |
|       - | 3749 | ` * A throw parks on the same rail as the rest and answers OK here: php's` |
|       - | 3750 | ` * request goes out and the response arrives, so CURLINFO_HTTP_CODE reports the` |
|       - | 3751 | ` * status of a transfer that really happened rather than 0.` |
|       - | 3752 | ` */` |
|       6 | 3753 | `static int CurlPreReqThunk(void *pUser,char *zConnIp,char *zLocalIp,int nConnPort,int nLocalPort)` |
|     ! 0 | 3754 | `{` |
|       6 | 3755 | `	phl_curl *pCurl = (phl_curl *)pUser;` |
|       6 | 3756 | `	ph7_vm *pVm = pCurl->pVm;` |
|       - | 3757 | `	ph7_value sArgs[5],sRes,*apArg[5];` |
|       - | 3758 | `	sxi32 rc;` |
|       - | 3759 | `	int i,iOut;` |
|       6 | 3760 | `	if( CurlCbParked(pCurl) \|\| pCurl->pPreReqCb == 0 ){` |
|     ! 0 | 3761 | `		return CURL_PREREQFUNC_OK;` |
|       - | 3762 | `	}` |
|      36 | 3763 | `	for( i = 0 ; i < 5 ; ++i ){` |
|      30 | 3764 | `		PH7_MemObjInit(pVm,&sArgs[i]);` |
|      30 | 3765 | `		apArg[i] = &sArgs[i];` |
|      15 | 3766 | `	}` |
|       6 | 3767 | `	if( pCurl->pOwner ){` |
|       6 | 3768 | `		sArgs[0].x.pOther = pCurl->pOwner;` |
|       6 | 3769 | `		sArgs[0].iFlags = MEMOBJ_OBJ;` |
|       6 | 3770 | `		pCurl->pOwner->iRef++;` |
|       3 | 3771 | `	}` |
|       6 | 3772 | `	ph7_value_string(&sArgs[1],zConnIp ? zConnIp : "",-1);` |
|       6 | 3773 | `	ph7_value_string(&sArgs[2],zLocalIp ? zLocalIp : "",-1);` |
|       6 | 3774 | `	ph7_value_int64(&sArgs[3],(sxi64)nConnPort);` |
|       6 | 3775 | `	ph7_value_int64(&sArgs[4],(sxi64)nLocalPort);` |
|       6 | 3776 | `	PH7_MemObjInit(pVm,&sRes);` |
|       6 | 3777 | `	rc = PH7_VmCallUserFunction(pVm,pCurl->pPreReqCb,5,apArg,&sRes);` |
|       6 | 3778 | `	if( rc != SXRET_OK ){` |
|       2 | 3779 | `		CurlCbPark(pCurl,rc);` |
|       2 | 3780 | `		iOut = CURL_PREREQFUNC_OK;` |
|       1 | 3781 | `	}else{` |
|       4 | 3782 | `		iOut = (int)ph7_value_to_int64(&sRes);` |
|       - | 3783 | `	}` |
|       6 | 3784 | `	PH7_MemObjRelease(&sRes);` |
|      36 | 3785 | `	for( i = 0 ; i < 5 ; ++i ){` |
|      30 | 3786 | `		PH7_MemObjRelease(&sArgs[i]);` |
|      15 | 3787 | `	}` |
|       6 | 3788 | `	return iOut;` |
|       3 | 3789 | `}` |
|       - | 3790 |  |
|       - | 3791 | `/* ===== curl_exec() ===== */` |
|       - | 3792 |  |
|       - | 3793 | `/*` |
|       - | 3794 | ` * Where the body goes when no callback and no stream has claimed it: the` |
|       - | 3795 | ` * buffer CURLOPT_RETURNTRANSFER answers with, or the script's own output --` |
|       - | 3796 | ` * which has to be the VM's output consumer, not stdout, so an ob_start() around` |
|       - | 3797 | ` * either exec verb captures it the way php's does.` |
|       - | 3798 | ` *` |
|       - | 3799 | ` * The buffer belongs to the HANDLE and not to the call, which is php's model` |
|       - | 3800 | ` * and the only one the multi rail can use: a transfer driven by` |
|       - | 3801 | ` * curl_multi_exec() has no call to answer, so curl_multi_getcontent() reads the` |
|       - | 3802 | ` * bytes off the handle afterwards. It is emptied at the START of a transfer --` |
|       - | 3803 | ` * curl_exec(), and curl_multi_add_handle() for the multi rail -- and never at` |
|       - | 3804 | ` * the end, so the last body stays readable for as long as the destination` |
|       - | 3805 | ` * stands.` |
|       - | 3806 | ` */` |
|     262 | 3807 | `PH7_PRIVATE void PH7_CurlBodyReset(phl_curl *pCurl)` |
|       1 | 3808 | `{` |
|     263 | 3809 | `	SyBlobReset(&pCurl->sBody);` |
|     263 | 3810 | `}` |
|     164 | 3811 | `static size_t CurlBodySink(char *zData,size_t nSize,size_t nMemb,void *pUser)` |
|       1 | 3812 | `{` |
|     165 | 3813 | `	phl_curl *pCurl = (phl_curl *)pUser;` |
|     165 | 3814 | `	size_t nTotal = nSize * nMemb;` |
|     165 | 3815 | `	if( nTotal < 1 ){` |
|     ! 0 | 3816 | `		return 0;` |
|       - | 3817 | `	}` |
|     165 | 3818 | `	if( pCurl->iWriteDest == PHL_CURL_DEST_RETURN ){` |
|     151 | 3819 | `		if( SyBlobAppend(&pCurl->sBody,zData,(sxu32)nTotal) != SXRET_OK ){` |
|     ! 0 | 3820 | `			return 0;   /* short write: libcurl turns this into CURLE_WRITE_ERROR */` |
|       1 | 3821 | `		}` |
|      90 | 3822 | `	}else if( pCurl->pExecCtx ){` |
|      15 | 3823 | `		ph7_context_output(pCurl->pExecCtx,zData,(int)nTotal);` |
|       8 | 3824 | `	}else{` |
|     ! 0 | 3825 | `		return 0;   /* no call to print through: a short write, not a crash */` |
|       - | 3826 | `	}` |
|     165 | 3827 | `	return nTotal;` |
|      83 | 3828 | `}` |
|       - | 3829 | `/*` |
|       - | 3830 | ` * Install every handler this transfer needs and take the CONTEXT it runs` |
|       - | 3831 | ` * under. Shared with the multi rail, which installs for each of its handles` |
|       - | 3832 | ` * before every curl_multi_perform() -- a destination can be changed between` |
|       - | 3833 | ` * two of them, and there is no other moment that would notice.` |
|       - | 3834 | ` */` |
|     280 | 3835 | `PH7_PRIVATE void PH7_CurlBeginTransfer(phl_curl *pCurl,ph7_context *pCtx)` |
|       1 | 3836 | `{` |
|     281 | 3837 | `	if( pCurl->pEasy == 0 ){` |
|     ! 0 | 3838 | `		return;` |
|       - | 3839 | `	}` |
|     281 | 3840 | `	pCurl->iCbExc = 0;` |
|     281 | 3841 | `	pCurl->bNoPathRead = 0;` |
|       - | 3842 | `	/* An upload part reads from inside libcurl and may have a diagnostic to` |
|       - | 3843 | `	 * raise; this is the context it belongs to, and the one the default` |
|       - | 3844 | `	 * destination prints through. */` |
|     281 | 3845 | `	pCurl->pExecCtx = pCtx;` |
|     281 | 3846 | `	if( pCurl->iWriteDest == PHL_CURL_DEST_USER && pCurl->pWriteCb ){` |
|       - | 3847 | `		/* A php WRITEFUNCTION replaces the destination entirely: neither the` |
|       - | 3848 | `		 * buffer nor the output gets the body. */` |
|      47 | 3849 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEFUNCTION,CurlWriteThunk);` |
|      47 | 3850 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEDATA,(void *)pCurl);` |
|     259 | 3851 | `	}else if( pCurl->iWriteDest == PHL_CURL_DEST_FILE ){` |
|       7 | 3852 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEFUNCTION,CurlStreamSink);` |
|       7 | 3853 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEDATA,(void *)pCurl->pWriteStream);` |
|       4 | 3854 | `	}else{` |
|     229 | 3855 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEFUNCTION,CurlBodySink);` |
|     229 | 3856 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEDATA,(void *)pCurl);` |
|       - | 3857 | `	}` |
|       - | 3858 | `	/*` |
|       - | 3859 | `	 * The header callback is installed on EVERY transfer, php's way: without` |
|       - | 3860 | `	 * one libcurl would fall back to the body's destination for a response` |
|       - | 3861 | `	 * whose headers were asked for, and php's default is to drop them.` |
|       - | 3862 | `	 */` |
|     281 | 3863 | `	if( pCurl->iHeaderDest == PHL_CURL_HDR_USER && pCurl->pHeaderCb ){` |
|       6 | 3864 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_HEADERFUNCTION,CurlHeaderThunk);` |
|       6 | 3865 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_HEADERDATA,(void *)pCurl);` |
|       3 | 3866 | `	}else{` |
|     275 | 3867 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_HEADERFUNCTION,CurlHeaderSink);` |
|     275 | 3868 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_HEADERDATA,(void *)pCurl);` |
|       - | 3869 | `	}` |
|     281 | 3870 | `	if( pCurl->pReadCb \|\| pCurl->pReadStream ){` |
|       8 | 3871 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_READFUNCTION,CurlReadThunk);` |
|       8 | 3872 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_READDATA,(void *)pCurl);` |
|       4 | 3873 | `	}` |
|     281 | 3874 | `	if( pCurl->pDebugCb \|\| pCurl->pStderrStream ){` |
|       6 | 3875 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_DEBUGFUNCTION,CurlDebugThunk);` |
|       6 | 3876 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_DEBUGDATA,(void *)pCurl);` |
|       3 | 3877 | `	}` |
|     281 | 3878 | `	if( pCurl->pPreReqCb ){` |
|       6 | 3879 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_PREREQFUNCTION,CurlPreReqThunk);` |
|       6 | 3880 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_PREREQDATA,(void *)pCurl);` |
|       3 | 3881 | `	}` |
|     281 | 3882 | `	if( pCurl->pXferCb ){` |
|      11 | 3883 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_XFERINFOFUNCTION,CurlXferThunk);` |
|      11 | 3884 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_XFERINFODATA,(void *)pCurl);` |
|       5 | 3885 | `	}` |
|     144 | 3886 | `}` |
|     280 | 3887 | `PH7_PRIVATE void PH7_CurlEndTransfer(phl_curl *pCurl)` |
|       1 | 3888 | `{` |
|     281 | 3889 | `	pCurl->pExecCtx = 0;` |
|     281 | 3890 | `}` |
|       - | 3891 | `/*` |
|       - | 3892 | ` * What a finished transfer leaves on the handle. libcurl fills the error` |
|       - | 3893 | ` * buffer itself; when it left it empty (some codes carry no detail) php still` |
|       - | 3894 | ` * answers the code's own text.` |
|       - | 3895 | ` */` |
|     234 | 3896 | `PH7_PRIVATE void PH7_CurlRecordResult(phl_curl *pCurl,int iCode)` |
|       1 | 3897 | `{` |
|     235 | 3898 | `	pCurl->iLastErr = iCode;` |
|     235 | 3899 | `	if( iCode != CURLE_OK && pCurl->zErrBuf[0] == 0 ){` |
|     ! 0 | 3900 | `		CurlSetErr(pCurl,iCode);` |
|     ! 0 | 3901 | `	}` |
|     235 | 3902 | `}` |
|       - | 3903 | `/* The body a RETURNTRANSFER handle is holding, as this call's answer. */` |
|     160 | 3904 | `PH7_PRIVATE void PH7_CurlResultBody(ph7_context *pCtx,phl_curl *pCurl)` |
|       1 | 3905 | `{` |
|     161 | 3906 | `	sxu32 nLen = SyBlobLength(&pCurl->sBody);` |
|     161 | 3907 | `	ph7_result_string(pCtx,nLen > 0 ? (const char *)SyBlobData(&pCurl->sBody) : "",(int)nLen);` |
|     161 | 3908 | `}` |
|       - | 3909 | `/* string\|bool curl_exec(CurlHandle $handle) */` |
|     226 | 3910 | `static int vm_builtin_curl_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3911 | `{` |
|     227 | 3912 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 3913 | `	CURLcode rc;` |
|     227 | 3914 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 3915 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3916 | `		return PH7_OK;` |
|       - | 3917 | `	}` |
|     227 | 3918 | `	PH7_CurlBodyReset(pCurl);` |
|     227 | 3919 | `	PH7_CurlBeginTransfer(pCurl,pCtx);` |
|     227 | 3920 | `	rc = curl_easy_perform(pCurl->pEasy);` |
|     227 | 3921 | `	PH7_CurlEndTransfer(pCurl);` |
|       - | 3922 | `	/*` |
|       - | 3923 | `	 * A callback threw: libcurl has unwound now, so this is where the parked` |
|       - | 3924 | `	 * status is raised. The handle keeps the CURLcode the transfer really` |
|       - | 3925 | `	 * ended in -- 0 for the ordinary case, since a throwing callback no longer` |
|       - | 3926 | `	 * stops anything, and the read callback's own timeout for the one shape` |
|       - | 3927 | `	 * that still ends badly.` |
|       - | 3928 | `	 */` |
|     227 | 3929 | `	if( pCurl->iCbExc != 0 ){` |
|      15 | 3930 | `		sxi32 rcExc = pCurl->iCbExc;` |
|      15 | 3931 | `		pCurl->iCbExc = 0;` |
|      15 | 3932 | `		PH7_CurlRecordResult(pCurl,(int)rc);` |
|      15 | 3933 | `		ph7_result_bool(pCtx,0);` |
|      15 | 3934 | `		return rcExc;` |
|       - | 3935 | `	}` |
|     213 | 3936 | `	if( pCurl->bNoPathRead ){` |
|       - | 3937 | `		/* An upload part with no source of its own: php refuses at the read as` |
|       - | 3938 | `		 * well as at the setter, and this is where the second refusal lands. */` |
|     ! 0 | 3939 | `		pCurl->bNoPathRead = 0;` |
|     ! 0 | 3940 | `		pCurl->iLastErr = 0;` |
|     ! 0 | 3941 | `		pCurl->zErrBuf[0] = 0;` |
|     ! 0 | 3942 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3943 | `		return PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");` |
|       - | 3944 | `	}` |
|     213 | 3945 | `	PH7_CurlRecordResult(pCurl,(int)rc);` |
|     213 | 3946 | `	if( rc != CURLE_OK ){` |
|      43 | 3947 | `		ph7_result_bool(pCtx,0);` |
|      43 | 3948 | `		return PH7_OK;` |
|       - | 3949 | `	}` |
|     171 | 3950 | `	if( pCurl->iWriteDest == PHL_CURL_DEST_RETURN ){` |
|     141 | 3951 | `		PH7_CurlResultBody(pCtx,pCurl);` |
|      71 | 3952 | `	}else{` |
|       - | 3953 | `		/* Every other destination answers TRUE, the callback one included:` |
|       - | 3954 | `		 * php reads the return value off the DESTINATION, so a handle carrying` |
|       - | 3955 | `		 * both a RETURNTRANSFER and a later WRITEFUNCTION answers true and not` |
|       - | 3956 | `		 * the empty buffer nothing filled. */` |
|      31 | 3957 | `		ph7_result_bool(pCtx,1);` |
|       - | 3958 | `	}` |
|     171 | 3959 | `	return PH7_OK;` |
|     114 | 3960 | `}` |
|       - | 3961 |  |
|       - | 3962 |  |
|       - | 3963 | `/* ===== escape / unescape / upkeep / pause ===== */` |
|       - | 3964 |  |
|       - | 3965 | `/*` |
|       - | 3966 | ` * curl_escape / curl_unescape are libcurl's own encoders, called with an` |
|       - | 3967 | ` * explicit LENGTH in both directions -- so they are NUL-clean where a C-string` |
|       - | 3968 | ` * call would not be: escaping "\0" answers "%00" and unescaping "%00" answers` |
|       - | 3969 | ` * the byte back. Neither touches the handle's error state.` |
|       - | 3970 | ` *` |
|       - | 3971 | `` * They are also not urlencode(): `+` is escaped to %2B on the way out and is`` |
|       - | 3972 | ` * NOT decoded to a space on the way back.` |
|       - | 3973 | ` */` |
|      18 | 3974 | `static int vm_builtin_curl_escape(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3975 | `{` |
|      19 | 3976 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 3977 | `	const char *zIn;` |
|      19 | 3978 | `	int nIn = 0;` |
|       - | 3979 | `	char *zOut;` |
|      19 | 3980 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 3981 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3982 | `		return PH7_OK;` |
|       - | 3983 | `	}` |
|      19 | 3984 | `	zIn = ph7_value_to_string(apArg[1],&nIn);` |
|      19 | 3985 | `	zOut = curl_easy_escape(pCurl->pEasy,zIn ? zIn : "",nIn);` |
|      19 | 3986 | `	if( zOut == 0 ){` |
|     ! 0 | 3987 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3988 | `		return PH7_OK;` |
|       - | 3989 | `	}` |
|      19 | 3990 | `	ph7_result_string(pCtx,zOut,-1);` |
|      19 | 3991 | `	curl_free(zOut);` |
|      19 | 3992 | `	return PH7_OK;` |
|      10 | 3993 | `}` |
|      16 | 3994 | `static int vm_builtin_curl_unescape(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3995 | `{` |
|      17 | 3996 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 3997 | `	const char *zIn;` |
|      17 | 3998 | `	int nIn = 0, nOut = 0;` |
|       - | 3999 | `	char *zOut;` |
|      17 | 4000 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 4001 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4002 | `		return PH7_OK;` |
|       - | 4003 | `	}` |
|      17 | 4004 | `	zIn = ph7_value_to_string(apArg[1],&nIn);` |
|      17 | 4005 | `	zOut = curl_easy_unescape(pCurl->pEasy,zIn ? zIn : "",nIn,&nOut);` |
|      17 | 4006 | `	if( zOut == 0 ){` |
|     ! 0 | 4007 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4008 | `		return PH7_OK;` |
|       - | 4009 | `	}` |
|       - | 4010 | `	/* The answer can carry a NUL of its own, so it is taken by LENGTH. */` |
|      17 | 4011 | `	ph7_result_string(pCtx,zOut,nOut);` |
|      17 | 4012 | `	curl_free(zOut);` |
|      17 | 4013 | `	return PH7_OK;` |
|       9 | 4014 | `}` |
|       - | 4015 | `/* bool curl_upkeep(CurlHandle $handle) -- one of the three verbs that CLEAR */` |
|       4 | 4016 | `static int vm_builtin_curl_upkeep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4017 | `{` |
|       5 | 4018 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       5 | 4019 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 4020 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4021 | `		return PH7_OK;` |
|       - | 4022 | `	}` |
|       5 | 4023 | `	CurlClearErr(pCurl);` |
|       5 | 4024 | `	ph7_result_bool(pCtx,curl_easy_upkeep(pCurl->pEasy) == CURLE_OK);` |
|       5 | 4025 | `	return PH7_OK;` |
|       3 | 4026 | `}` |
|       - | 4027 | `/*` |
|       - | 4028 | ` * int curl_pause(CurlHandle $handle, int $flags)` |
|       - | 4029 | ` *` |
|       - | 4030 | ` * Answers libcurl's CURLcode as an INT rather than a bool -- and leaves the` |
|       - | 4031 | ` * handle's error state alone, so a pause on a handle with no transfer reports` |
|       - | 4032 | ` * 43 (CURLE_BAD_FUNCTION_ARGUMENT) while curl_errno() still says whatever the` |
|       - | 4033 | ` * last transfer said.` |
|       - | 4034 | ` */` |
|      12 | 4035 | `static int vm_builtin_curl_pause(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4036 | `{` |
|      13 | 4037 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|      13 | 4038 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 4039 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4040 | `		return PH7_OK;` |
|       - | 4041 | `	}` |
|      13 | 4042 | `	ph7_result_int(pCtx,(int)curl_easy_pause(pCurl->pEasy,(int)ph7_value_to_int64(apArg[1])));` |
|      13 | 4043 | `	return PH7_OK;` |
|       7 | 4044 | `}` |
|       - | 4045 |  |
|       - | 4046 | `/* ===== Installation ===== */` |
|       - | 4047 |  |
|    8445 | 4048 | `PH7_PRIVATE sxi32 PH7_VmInstallCurl(ph7_vm *pVm)` |
|       5 | 4049 | `{` |
|       - | 4050 | `	static const struct {` |
|       - | 4051 | `		const char *zName;` |
|       - | 4052 | `		ProchHostFunction xFunc;` |
|       - | 4053 | `	} aFunc[] = {` |
|       - | 4054 | `		{ "curl_version",        vm_builtin_curl_version        },` |
|       - | 4055 | `		{ "curl_strerror",       vm_builtin_curl_strerror       },` |
|       - | 4056 | `		{ "curl_multi_strerror", vm_builtin_curl_multi_strerror },` |
|       - | 4057 | `		{ "curl_share_strerror", vm_builtin_curl_share_strerror },` |
|       - | 4058 | `		{ "curl_init",           vm_builtin_curl_init           },` |
|       - | 4059 | `		{ "curl_close",          vm_builtin_curl_close          },` |
|       - | 4060 | `		{ "curl_reset",          vm_builtin_curl_reset          },` |
|       - | 4061 | `		{ "curl_errno",          vm_builtin_curl_errno          },` |
|       - | 4062 | `		{ "curl_error",          vm_builtin_curl_error          },` |
|       - | 4063 | `		{ "curl_copy_handle",    vm_builtin_curl_copy_handle    },` |
|       - | 4064 | `		{ "curl_setopt",         vm_builtin_curl_setopt         },` |
|       - | 4065 | `		{ "curl_setopt_array",   vm_builtin_curl_setopt_array   },` |
|       - | 4066 | `		{ "curl_getinfo",        vm_builtin_curl_getinfo        },` |
|       - | 4067 | `		{ "curl_exec",           vm_builtin_curl_exec           },` |
|       - | 4068 | `		{ "curl_escape",         vm_builtin_curl_escape         },` |
|       - | 4069 | `		{ "curl_unescape",       vm_builtin_curl_unescape       },` |
|       - | 4070 | `		{ "curl_upkeep",         vm_builtin_curl_upkeep         },` |
|       - | 4071 | `		{ "curl_pause",          vm_builtin_curl_pause          },` |
|       - | 4072 | `		{ "curl_file_create",    vm_builtin_curl_file_create    }` |
|       - | 4073 | `	};` |
|       - | 4074 | `	/*` |
|       - | 4075 | `	 * The libcurl handle, and nothing else: php's CurlHandle declares no` |
|       - | 4076 | `	 * method, no constant and no property, and prints as an empty object on` |
|       - | 4077 | `	 * every presentation surface. The one slot here is engine storage, hidden` |
|       - | 4078 | `	 * so it appears on none of them.` |
|       - | 4079 | `	 */` |
|       - | 4080 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 4081 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }` |
|       - | 4082 | `	};` |
|       - | 4083 | `	/*` |
|       - | 4084 | ``	 * FINAL (php refuses `class X extends CurlHandle`), NOINSTANTIATE with`` |
|       - | 4085 | `	 * php's own per-class sentence, and NOSERIALIZE -- but NOT NOCLONE, which` |
|       - | 4086 | `	 * is where CurlHandle parts company with every other handle class here:` |
|       - | 4087 | ``	 * php maps clone to curl_easy_duphandle(), so `clone $h` answers a second,`` |
|       - | 4088 | `	 * independent handle. ReflectionClass::isInstantiable() still reports true` |
|       - | 4089 | `	 * for it, which is php's answer too, because the refusal lives in the` |
|       - | 4090 | `	 * creation step rather than in a private constructor.` |
|       - | 4091 | `	 */` |
|       - | 4092 | `	/*` |
|       - | 4093 | `	 * The two upload boxes. php's CURLFile gives its three slots an empty` |
|       - | 4094 | `	 * default and CURLStringFile gives its three none at all -- so an unset` |
|       - | 4095 | `	 * CURLStringFile property is UNINITIALIZED where a CURLFile one reads back` |
|       - | 4096 | `	 * "". Neither class is final, and only the file box refuses serialization,` |
|       - | 4097 | `	 * which is php's own asymmetry: the string box carries its bytes with it` |
|       - | 4098 | `	 * and the file box names something another process may not have.` |
|       - | 4099 | `	 *` |
|       - | 4100 | ``	 * The getters carry TENTATIVE return types (php's `@tentative-return-type`,`` |
|       - | 4101 | ``	 * the leading `@` here), so getReturnType() answers null for them exactly`` |
|       - | 4102 | `	 * as it does under php.` |
|       - | 4103 | `	 */` |
|       - | 4104 | `	static const PH7_NativePropDef aFileProp[] = {` |
|       - | 4105 | `		{ CURLFILE_NAME,     PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" },` |
|       - | 4106 | `		{ CURLFILE_MIME,     PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" },` |
|       - | 4107 | `		{ CURLFILE_POSTNAME, PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }` |
|       - | 4108 | `	};` |
|       - | 4109 | `	static const PH7_NativeMethodDef aFileMethod[] = {` |
|       - | 4110 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|       - | 4111 | `		  "string $filename, ?string $mime_type = null, ?string $posted_filename = null", 0,` |
|       - | 4112 | `		  vm_builtin_CURLFile_construct },` |
|       - | 4113 | `		{ "getFilename",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_CURLFile_getFilename },` |
|       - | 4114 | `		{ "getMimeType",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_CURLFile_getMimeType },` |
|       - | 4115 | `		{ "getPostFilename", PH7_MOD_PUBLIC, "", "@string", vm_builtin_CURLFile_getPostFilename },` |
|       - | 4116 | `		{ "setMimeType",     PH7_MOD_PUBLIC, "string $mime_type", "@void",` |
|       - | 4117 | `		  vm_builtin_CURLFile_setMimeType },` |
|       - | 4118 | `		{ "setPostFilename", PH7_MOD_PUBLIC, "string $posted_filename", "@void",` |
|       - | 4119 | `		  vm_builtin_CURLFile_setPostFilename }` |
|       - | 4120 | `	};` |
|       - | 4121 | `	static const PH7_NativePropDef aStrFileProp[] = {` |
|       - | 4122 | `		{ CURLSTR_DATA,      PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|       - | 4123 | `		{ CURLFILE_POSTNAME, PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|       - | 4124 | `		{ CURLFILE_MIME,     PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" }` |
|       - | 4125 | `	};` |
|       - | 4126 | `	static const PH7_NativeMethodDef aStrFileMethod[] = {` |
|       - | 4127 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|       - | 4128 | `		  "string $data, string $postname, string $mime = 'application/octet-stream'", 0,` |
|       - | 4129 | `		  vm_builtin_CURLStringFile_construct }` |
|       - | 4130 | `	};` |
|       - | 4131 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 4132 | `		/* PH7_CLASS_HANDLE_ID: php's cast_object for this class answers the object` |
|       - | 4133 | ``		 * handle for `(int)`, silently -- see the flag. */`` |
|       - | 4134 | `		{ "CurlHandle", 0, 0,` |
|       - | 4135 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_HANDLE_ID,` |
|       - | 4136 | `		  0, 0, 0, 0,` |
|       - | 4137 | `		  aProp, SX_ARRAYSIZE(aProp),` |
|       - | 4138 | `		  CurlInstanceRelease, 0, 0 },` |
|       - | 4139 | `		{ "CURLFile", 0, 0, PH7_CLASS_NOSERIALIZE,` |
|       - | 4140 | `		  aFileMethod, SX_ARRAYSIZE(aFileMethod), 0, 0,` |
|       - | 4141 | `		  aFileProp, SX_ARRAYSIZE(aFileProp), 0, 0, 0 },` |
|       - | 4142 | `		{ "CURLStringFile", 0, 0, 0,` |
|       - | 4143 | `		  aStrFileMethod, SX_ARRAYSIZE(aStrFileMethod), 0, 0,` |
|       - | 4144 | `		  aStrFileProp, SX_ARRAYSIZE(aStrFileProp), 0, 0, 0 }` |
|       - | 4145 | `	};` |
|       - | 4146 | `	sxu32 n;` |
|       - | 4147 | `	sxi32 rc;` |
|    8450 | 4148 | `	PH7_CurlGlobalInit();` |
|    8450 | 4149 | `	pVm->pCurlHandles = 0;` |
|  168905 | 4150 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; ++n ){` |
|  160460 | 4151 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|   80128 | 4152 | `	}` |
|    8450 | 4153 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    8450 | 4154 | `	if( rc == SXRET_OK ){` |
|    8450 | 4155 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"CurlHandle",sizeof("CurlHandle")-1,FALSE,0);` |
|    8450 | 4156 | `		if( pClass ){` |
|    8450 | 4157 | `			pClass->zNewRefusal =` |
|       - | 4158 | `				"Cannot directly construct CurlHandle, use curl_init() instead";` |
|       - | 4159 | ``			/* php's clone_obj: `clone $h` is curl_easy_duphandle(), which is`` |
|       - | 4160 | `			 * why this class alone is not PH7_CLASS_NOCLONE. Stated on the` |
|       - | 4161 | `			 * MOUNTED class, like every other handler hook. */` |
|    8450 | 4162 | `			pClass->xClone = CurlInstanceClone;` |
|       - | 4163 | `			/* ...and php's compare handler, which recognizes nothing: a handle is` |
|       - | 4164 | `			 * UNCOMPARABLE with everything but itself. */` |
|    8450 | 4165 | `			pClass->xCmp = PH7_NativeCmpOpaqueHandle;` |
|    4217 | 4166 | `		}` |
|    4217 | 4167 | `	}` |
|    8450 | 4168 | `	if( rc == SXRET_OK ){` |
|       - | 4169 | `		/* The multi and share halves, in their own unit (vm_curl_multi.c). */` |
|    8450 | 4170 | `		rc = PH7_VmInstallCurlMulti(&(*pVm));` |
|    4217 | 4171 | `	}` |
|    8450 | 4172 | `	return rc;` |
|       5 | 4173 | `}` |
|       - | 4174 |  |
|       - | 4175 | `#else` |
|       - | 4176 | `/* Ensure non-empty translation unit when curl is disabled (MSVC C4206) */` |
|       - | 4177 | `typedef int vm_curl_unused;` |
|       - | 4178 | `#endif /* PH7_ENABLE_CURL */` |
|       - | 4179 |  |
