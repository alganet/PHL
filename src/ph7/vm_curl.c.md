# src/ph7/vm_curl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 835/995 lines (83.92%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    4 | ` */` |
|       - |    5 | `#ifdef PH7_ENABLE_CURL` |
|       - |    6 | `#include "curl_int.h"` |
|       - |    7 |  |
|       - |    8 | `/*` |
|       - |    9 | ` * Section:` |
|       - |   10 | ` *    ext/curl -- php's binding of libcurl.` |
|       - |   11 | ` * Status:` |
|       - |   12 | ` *    In progress. This unit currently carries the library-wide surface:` |
|       - |   13 | ` *    curl_version() and the three strerror() families. The handle classes,` |
|       - |   14 | ` *    the option table and the transfer verbs follow.` |
|       - |   15 | ` *` |
|       - |   16 | ` * WHY A BINDING AND NOT A REIMPLEMENTATION. php's ext/curl is a thin shell` |
|       - |   17 | ` * over libcurl, so the contract a program depends on is the LIBRARY's, not` |
|       - |   18 | ` * the extension's -- the same relationship ext/pdo_sqlite has with` |
|       - |   19 | ` * libsqlite3. Every answer here is therefore derived by asking php 8.5 and` |
|       - |   20 | ` * libcurl 8.5 the same question, never by reading php's source: see the` |
|       - |   21 | ` * per-function notes below for the ones a careful reading would have got` |
|       - |   22 | ` * wrong.` |
|       - |   23 | ` *` |
|       - |   24 | ` * MEMORY MODEL. libcurl stays on its own (system) allocator, like libxml2 and` |
|       - |   25 | ` * sqlite3 before it. Routing it through SyMemBackend would subject library` |
|       - |   26 | ` * internals to the PHL_MAX_ALLOC fault injection the stress tier uses, and` |
|       - |   27 | ` * libcurl does not tolerate mid-transfer OOM injection the way engine code` |
|       - |   28 | ` * does.` |
|       - |   29 | ` */` |
|       - |   30 |  |
|       - |   31 | `/*` |
|       - |   32 | ` * curl_global_init() once per process. libcurl's own documentation calls this` |
|       - |   33 | ` * not thread-safe, so it must not be left to the first curl_easy_init() on` |
|       - |   34 | ` * whichever thread gets there first (the -S server pre-forks, and` |
|       - |   35 | ` * PH7_ENABLE_THREADS builds share the process).` |
|       - |   36 | ` */` |
|    5256 |   37 | `PH7_PRIVATE void PH7_CurlGlobalInit(void)` |
|       5 |   38 | `{` |
|       - |   39 | `	static int bInit = 0;` |
|    5261 |   40 | `	if( !bInit ){` |
|    5259 |   41 | `		curl_global_init(CURL_GLOBAL_DEFAULT);` |
|    5259 |   42 | `		bInit = 1;` |
|    2627 |   43 | `	}` |
|    5261 |   44 | `}` |
|       - |   45 |  |
|       - |   46 | `/* ------------------------------------------------------------------------` |
|       - |   47 | ` * Handle lifetime` |
|       - |   48 | ` * ------------------------------------------------------------------------ */` |
|       - |   49 | `/*` |
|       - |   50 | ``  * A handle is reached from its CurlHandle object through the hidden `__res` `` |
|       - |   51 | ` * slot and is ALSO chained on the per-VM registry, because a PH7 resource` |
|       - |   52 | ` * carries no destructor: the sweep at VM reset/release is what closes a` |
|       - |   53 | ` * transfer a script left open. Unlike ext/pdo's connections, a CurlHandle IS` |
|       - |   54 | ` * cloneable (php maps clone to curl_easy_duphandle), so the clone gets its own` |
|       - |   55 | ` * record and its own libcurl handle -- never a second object over one CURL*.` |
|       - |   56 | ` */` |
|       - |   57 | `static void CurlBlankSlot(ph7_class_instance *pOwner);` |
|       - |   58 | `static int CurlSetCallback(ph7_context *pCtx,phl_curl *pCurl,ph7_value **ppSlot,` |
|       - |   59 | `	ph7_value *pVal,const char *zFunc,const char *zOpt,sxi32 *pRc);` |
|       - |   60 |  |
|      98 |   61 | `static phl_curl * CurlNewHandle(ph7_vm *pVm)` |
|       2 |   62 | `{` |
|     100 |   63 | `	phl_curl *pCurl = (phl_curl *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curl));` |
|     100 |   64 | `	if( pCurl == 0 ){` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|     100 |   67 | `	SyZero(pCurl,sizeof(phl_curl));` |
|     100 |   68 | `	pCurl->pVm = pVm;` |
|     100 |   69 | `	pCurl->pEasy = curl_easy_init();` |
|     100 |   70 | `	if( pCurl->pEasy == 0 ){` |
|     ! 0 |   71 | `		SyMemBackendFree(&pVm->sAllocator,pCurl);` |
|     ! 0 |   72 | `		return 0;` |
|       - |   73 | `	}` |
|       - |   74 | `	/*` |
|       - |   75 | `	 * php gives every handle its own error buffer at creation, and that is` |
|       - |   76 | `	 * what curl_error() reports -- not curl_easy_strerror() of the code. It` |
|       - |   77 | `	 * has to be re-applied after curl_easy_reset(), which clears it.` |
|       - |   78 | `	 */` |
|     100 |   79 | `	curl_easy_setopt(pCurl->pEasy,CURLOPT_ERRORBUFFER,pCurl->zErrBuf);` |
|     100 |   80 | `	pCurl->pNext = (phl_curl *)pVm->pCurlHandles;` |
|     100 |   81 | `	pVm->pCurlHandles = pCurl;` |
|     100 |   82 | `	return pCurl;` |
|      51 |   83 | `}` |
|       - |   84 | `/*` |
|       - |   85 | ` * The slot a handle keeps one option's curl_slist in, created on first use.` |
|       - |   86 | ` * Keyed by option, so setting CURLOPT_HTTPHEADER twice replaces one list` |
|       - |   87 | ` * rather than accumulating two.` |
|       - |   88 | ` */` |
|      22 |   89 | `static phl_curl_slist * CurlSlistSlot(phl_curl *pCurl,sxi64 iOpt)` |
|       1 |   90 | `{` |
|      23 |   91 | `	phl_curl_slist *pSlot = pCurl->pSlists;` |
|      27 |   92 | `	while( pSlot ){` |
|      17 |   93 | `		if( pSlot->iOpt == iOpt ){` |
|      13 |   94 | `			return pSlot;` |
|       - |   95 | `		}` |
|       5 |   96 | `		pSlot = pSlot->pNext;` |
|       1 |   97 | `	}` |
|      11 |   98 | `	pSlot = (phl_curl_slist *)SyMemBackendAlloc(&pCurl->pVm->sAllocator,sizeof(phl_curl_slist));` |
|      11 |   99 | `	if( pSlot == 0 ){` |
|     ! 0 |  100 | `		return 0;` |
|       - |  101 | `	}` |
|      11 |  102 | `	SyZero(pSlot,sizeof(phl_curl_slist));` |
|      11 |  103 | `	pSlot->iOpt = iOpt;` |
|      11 |  104 | `	pSlot->pNext = pCurl->pSlists;` |
|      11 |  105 | `	pCurl->pSlists = pSlot;` |
|      11 |  106 | `	return pSlot;` |
|      12 |  107 | `}` |
|     204 |  108 | `static void CurlFreeSlists(phl_curl *pCurl)` |
|       2 |  109 | `{` |
|     206 |  110 | `	phl_curl_slist *pSlot = pCurl->pSlists;` |
|     216 |  111 | `	while( pSlot ){` |
|      11 |  112 | `		phl_curl_slist *pNext = pSlot->pNext;` |
|      11 |  113 | `		if( pSlot->pList ){` |
|      11 |  114 | `			curl_slist_free_all(pSlot->pList);` |
|       5 |  115 | `		}` |
|      11 |  116 | `		SyMemBackendFree(&pCurl->pVm->sAllocator,pSlot);` |
|      11 |  117 | `		pSlot = pNext;` |
|       1 |  118 | `	}` |
|     206 |  119 | `	pCurl->pSlists = 0;` |
|     206 |  120 | `}` |
|     204 |  121 | `static void CurlDropCallbacks(phl_curl *pCurl)` |
|       2 |  122 | `{` |
|       - |  123 | `	ph7_value **apCb[3];` |
|       - |  124 | `	int i;` |
|     206 |  125 | `	apCb[0] = &pCurl->pWriteCb;` |
|     206 |  126 | `	apCb[1] = &pCurl->pHeaderCb;` |
|     206 |  127 | `	apCb[2] = &pCurl->pXferCb;` |
|     818 |  128 | `	for( i = 0 ; i < 3 ; ++i ){` |
|     614 |  129 | `		if( *apCb[i] ){` |
|      31 |  130 | `			ph7_release_value(pCurl->pVm,*apCb[i]);` |
|      31 |  131 | `			*apCb[i] = 0;` |
|      15 |  132 | `		}` |
|     308 |  133 | `	}` |
|     206 |  134 | `}` |
|     204 |  135 | `static void CurlFreeHandle(phl_curl *pCurl)` |
|       2 |  136 | `{` |
|     206 |  137 | `	if( pCurl->pEasy ){` |
|       - |  138 | `		/* The handle goes first: libcurl reads the lists during a transfer and` |
|       - |  139 | `		 * must not be left pointing at freed memory even for an instant. */` |
|     114 |  140 | `		curl_easy_cleanup(pCurl->pEasy);` |
|     114 |  141 | `		pCurl->pEasy = 0;` |
|      56 |  142 | `	}` |
|     206 |  143 | `	CurlFreeSlists(pCurl);` |
|     206 |  144 | `	CurlDropCallbacks(pCurl);` |
|     206 |  145 | `}` |
|       - |  146 | `/*` |
|       - |  147 | ` * Free every registered handle. Called from PH7_CurlVmReset (a reused VM --` |
|       - |  148 | ` * the -S server's -- must not answer the next request through a connection the` |
|       - |  149 | ` * previous one opened) and from PH7_CurlVmRelease before the allocator holding` |
|       - |  150 | ` * the shells is torn down.` |
|       - |  151 | ` */` |
|    4672 |  152 | `static void CurlVmSweep(ph7_vm *pVm)` |
|       5 |  153 | `{` |
|    4677 |  154 | `	phl_curl *pCurl = (phl_curl *)pVm->pCurlHandles;` |
|    4789 |  155 | `	while( pCurl ){` |
|     114 |  156 | `		phl_curl *pNext = pCurl->pNext;` |
|     114 |  157 | `		CurlBlankSlot(pCurl->pOwner);` |
|     114 |  158 | `		CurlFreeHandle(pCurl);` |
|     114 |  159 | `		SyMemBackendFree(&pVm->sAllocator,pCurl);` |
|     114 |  160 | `		pCurl = pNext;` |
|       2 |  161 | `	}` |
|    4677 |  162 | `	pVm->pCurlHandles = 0;` |
|    4677 |  163 | `}` |
|      16 |  164 | `PH7_PRIVATE void PH7_CurlVmReset(ph7_vm *pVm)` |
|     ! 0 |  165 | `{` |
|      16 |  166 | `	CurlVmSweep(&(*pVm));` |
|      16 |  167 | `}` |
|    4656 |  168 | `PH7_PRIVATE void PH7_CurlVmRelease(ph7_vm *pVm)` |
|       5 |  169 | `{` |
|    4661 |  170 | `	CurlVmSweep(&(*pVm));` |
|    4661 |  171 | `}` |
|       - |  172 | `/*` |
|       - |  173 | ` * Blank the hidden slot of the object whose record we are about to free, so` |
|       - |  174 | ` * the object cannot outlive its record and then read freed memory to ask` |
|       - |  175 | ` * whether it still owns one.` |
|       - |  176 | ` */` |
|     112 |  177 | `static void CurlBlankSlot(ph7_class_instance *pOwner)` |
|       2 |  178 | `{` |
|       - |  179 | `	SyString sAttr;` |
|       - |  180 | `	ph7_value *pRes;` |
|     114 |  181 | `	if( pOwner == 0 ){` |
|      93 |  182 | `		return;` |
|       - |  183 | `	}` |
|      22 |  184 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|      22 |  185 | `	pRes = PH7_ClassInstanceFetchAttr(pOwner,&sAttr);` |
|      22 |  186 | `	if( pRes ){` |
|      22 |  187 | `		PH7_MemObjRelease(pRes);` |
|      22 |  188 | `		MemObjSetType(pRes,MEMOBJ_NULL);` |
|      10 |  189 | `	}` |
|      58 |  190 | `}` |
|       - |  191 | ``/* The handle behind a `__res` slot value. */`` |
|     514 |  192 | `static phl_curl * CurlOfValue(ph7_value *pVal)` |
|       1 |  193 | `{` |
|     515 |  194 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|     ! 0 |  195 | `		return 0;` |
|       - |  196 | `	}` |
|     515 |  197 | `	return (phl_curl *)pVal->x.pOther;` |
|     258 |  198 | `}` |
|     514 |  199 | `static phl_curl * CurlOfInstance(ph7_class_instance *pThis)` |
|       1 |  200 | `{` |
|       - |  201 | `	SyString sAttr;` |
|     515 |  202 | `	if( pThis == 0 ){` |
|     ! 0 |  203 | `		return 0;` |
|       - |  204 | `	}` |
|     515 |  205 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|     515 |  206 | `	return CurlOfValue(PH7_ClassInstanceFetchAttr(pThis,&sAttr));` |
|     258 |  207 | `}` |
|       - |  208 | `/* Store one handle in the receiver's hidden slot. */` |
|     112 |  209 | `static int CurlAttach(ph7_class_instance *pThis,phl_curl *pCurl)` |
|       2 |  210 | `{` |
|       - |  211 | `	SyString sAttr;` |
|       - |  212 | `	ph7_value *pRes;` |
|     114 |  213 | `	if( pThis == 0 ){` |
|     ! 0 |  214 | `		return -1;` |
|       - |  215 | `	}` |
|     114 |  216 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|     114 |  217 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|     114 |  218 | `	if( pRes == 0 ){` |
|     ! 0 |  219 | `		return -1;` |
|       - |  220 | `	}` |
|     114 |  221 | `	PH7_MemObjRelease(pRes);` |
|     114 |  222 | `	pRes->x.pOther = pCurl;` |
|     114 |  223 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|     114 |  224 | `	pCurl->pOwner = pThis;` |
|     114 |  225 | `	return 0;` |
|      58 |  226 | `}` |
|       - |  227 | `/*` |
|       - |  228 | ` * The object is going away: close its transfer now rather than at VM reset, so` |
|       - |  229 | ` * a script that drops its last reference releases the socket there. The shell` |
|       - |  230 | ` * stays on the registry (the sweep frees it) because the slot is still` |
|       - |  231 | ` * reachable while the instance is being torn down.` |
|       - |  232 | ` */` |
|      92 |  233 | `static void CurlInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       1 |  234 | `{` |
|      93 |  235 | `	phl_curl *pCurl = CurlOfInstance(pThis);` |
|      46 |  236 | `	SXUNUSED(pVm);` |
|      93 |  237 | `	if( pCurl == 0 \|\| pCurl->pOwner != pThis ){` |
|     ! 0 |  238 | `		return;` |
|       - |  239 | `	}` |
|      93 |  240 | `	CurlFreeHandle(pCurl);` |
|      93 |  241 | `	pCurl->pOwner = 0;` |
|      47 |  242 | `}` |
|       - |  243 | `/*` |
|       - |  244 | ` * php's clone_obj for CurlHandle: curl_easy_duphandle(). Every OPTION comes` |
|       - |  245 | ` * across and nothing else does -- the copy starts with a clean error state` |
|       - |  246 | ` * even when the source's last transfer failed.` |
|       - |  247 | ` *` |
|       - |  248 | ` * The error buffer is why this needs care rather than a bare duphandle.` |
|       - |  249 | ` * CURLOPT_ERRORBUFFER is an option like any other, so libcurl copies its` |
|       - |  250 | ` * VALUE: the clone would point at the SOURCE's buffer, write its own failures` |
|       - |  251 | ` * into it (php's answer for the source would change when the clone failed) and` |
|       - |  252 | ` * keep writing there after the source was freed. Re-pointing it at the clone's` |
|       - |  253 | ` * own storage is what php does and what makes the two independent.` |
|       - |  254 | ` *` |
|       - |  255 | `` * Runs after the slot-by-slot copy, so the clone's `__res` currently holds the`` |
|       - |  256 | ` * SOURCE's record: every exit here has to overwrite or blank it, or two` |
|       - |  257 | ` * instances would free one CURL*.` |
|       - |  258 | ` */` |
|      14 |  259 | `static void CurlInstanceClone(ph7_vm *pVm,ph7_class_instance *pClone,ph7_class_instance *pSrc)` |
|       1 |  260 | `{` |
|      15 |  261 | `	phl_curl *pFrom = CurlOfInstance(pSrc);` |
|       - |  262 | `	phl_curl *pNew;` |
|       - |  263 | `	CURL *pDup;` |
|      15 |  264 | `	if( pFrom == 0 \|\| pFrom->pEasy == 0 ){` |
|     ! 0 |  265 | `		CurlBlankSlot(pClone);` |
|     ! 0 |  266 | `		return;` |
|       - |  267 | `	}` |
|      15 |  268 | `	pNew = (phl_curl *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curl));` |
|      15 |  269 | `	if( pNew == 0 ){` |
|     ! 0 |  270 | `		CurlBlankSlot(pClone);` |
|     ! 0 |  271 | `		return;` |
|       - |  272 | `	}` |
|      15 |  273 | `	SyZero(pNew,sizeof(phl_curl));` |
|      15 |  274 | `	pNew->pVm = pVm;` |
|      15 |  275 | `	pDup = curl_easy_duphandle(pFrom->pEasy);` |
|      15 |  276 | `	if( pDup == 0 ){` |
|     ! 0 |  277 | `		SyMemBackendFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  278 | `		CurlBlankSlot(pClone);` |
|     ! 0 |  279 | `		return;` |
|       - |  280 | `	}` |
|      15 |  281 | `	pNew->pEasy = pDup;` |
|      15 |  282 | `	curl_easy_setopt(pDup,CURLOPT_ERRORBUFFER,pNew->zErrBuf);` |
|       - |  283 | `	/*` |
|       - |  284 | `	 * The slists have to be rebuilt for the copy, and pointed at from the copy,` |
|       - |  285 | `	 * for the same reason the source owns them: libcurl stores the POINTER.` |
|       - |  286 | `	 * Left alone, the copy would read the source's lists and keep reading them` |
|       - |  287 | `	 * after the source freed them. (php duplicates them here too, which is the` |
|       - |  288 | `	 * evidence that duphandle does not.)` |
|       - |  289 | `	 */` |
|       - |  290 | `	{` |
|      15 |  291 | `		phl_curl_slist *pFromSlot = pFrom->pSlists;` |
|      19 |  292 | `		while( pFromSlot ){` |
|       5 |  293 | `			struct curl_slist *pCopy = 0;` |
|       5 |  294 | `			struct curl_slist *pWalk = pFromSlot->pList;` |
|       5 |  295 | `			int bOk = 1;` |
|       9 |  296 | `			while( pWalk ){` |
|       5 |  297 | `				struct curl_slist *pNextNode = curl_slist_append(pCopy,pWalk->data);` |
|       5 |  298 | `				if( pNextNode == 0 ){ bOk = 0; break; }` |
|       5 |  299 | `				pCopy = pNextNode;` |
|       5 |  300 | `				pWalk = pWalk->next;` |
|       1 |  301 | `			}` |
|       5 |  302 | `			if( bOk ){` |
|       5 |  303 | `				phl_curl_slist *pSlot = CurlSlistSlot(pNew,pFromSlot->iOpt);` |
|       5 |  304 | `				if( pSlot ){` |
|       5 |  305 | `					pSlot->pList = pCopy;` |
|       5 |  306 | `					curl_easy_setopt(pDup,(CURLoption)pFromSlot->iOpt,pCopy);` |
|       2 |  307 | `				}else if( pCopy ){` |
|     ! 0 |  308 | `					curl_slist_free_all(pCopy);` |
|       1 |  309 | `				}` |
|       2 |  310 | `			}else if( pCopy ){` |
|     ! 0 |  311 | `				curl_slist_free_all(pCopy);` |
|     ! 0 |  312 | `			}` |
|       5 |  313 | `			pFromSlot = pFromSlot->pNext;` |
|       1 |  314 | `		}` |
|       - |  315 | `	}` |
|      15 |  316 | `	pNew->pNext = (phl_curl *)pVm->pCurlHandles;` |
|      15 |  317 | `	pVm->pCurlHandles = pNew;` |
|      15 |  318 | `	if( CurlAttach(pClone,pNew) != 0 ){` |
|     ! 0 |  319 | `		CurlBlankSlot(pClone);` |
|     ! 0 |  320 | `	}` |
|       8 |  321 | `}` |
|       - |  322 |  |
|       - |  323 | `/*` |
|       - |  324 | ` * The CurlHandle argument of every verb. The signature table has already` |
|       - |  325 | ` * screened the TYPE (php's "must be of type CurlHandle, null given" comes from` |
|       - |  326 | ` * there), so a miss here means the object is one the engine tore down -- which` |
|       - |  327 | ` * php cannot produce and which must not be a crash.` |
|       - |  328 | ` */` |
|     402 |  329 | `static phl_curl * CurlArg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  330 | `{` |
|       - |  331 | `	ph7_class_instance *pThis;` |
|     201 |  332 | `	SXUNUSED(nArg);` |
|     403 |  333 | `	pThis = (apArg && ph7_value_is_object(apArg[0])) ?` |
|     603 |  334 | `		(ph7_class_instance *)apArg[0]->x.pOther : 0;` |
|     403 |  335 | `	if( pThis == 0 ){` |
|     ! 0 |  336 | `		return 0;` |
|       - |  337 | `	}` |
|     201 |  338 | `	SXUNUSED(pCtx);` |
|     403 |  339 | `	return CurlOfInstance(pThis);` |
|     202 |  340 | `}` |
|       - |  341 |  |
|       - |  342 | `/* ===== Constants ===== */` |
|       - |  343 |  |
|       - |  344 | `/*` |
|       - |  345 | ` * php's whole ext/curl constant surface: 679 names, dumped from php 8.5.9's` |
|       - |  346 | ` * own get_defined_constants(true)['curl'] and kept in php's REGISTRATION` |
|       - |  347 | ` * order (which is what get_defined_constants reports).` |
|       - |  348 | ` *` |
|       - |  349 | ` * 676 of the 679 are libcurl symbols, and every one of those was diffed` |
|       - |  350 | ` * against the library: php's value and libcurl's agree on all 676, so naming` |
|       - |  351 | ` * the SYMBOL rather than the number is what keeps PHL and a php built against` |
|       - |  352 | ` * the same library in step -- and it is why no version gate is needed. The` |
|       - |  353 | ` * floor in curl_int.h is 8.5.0, the whole table compiles against it, and a` |
|       - |  354 | ` * newer libcurl only ever ADDS names.` |
|       - |  355 | ` *` |
|       - |  356 | ` * The three that are NOT libcurl symbols are php's own inventions and are the` |
|       - |  357 | ` * only literals here: CURLOPT_RETURNTRANSFER and CURLOPT_BINARYTRANSFER are` |
|       - |  358 | ` * php-side options in a range no libcurl option occupies, and` |
|       - |  359 | ` * CURLOPT_SAFE_UPLOAD is php's -1 sentinel.` |
|       - |  360 | ` *` |
|       - |  361 | ` * The values do not all fit an int: CURLAUTH_ONLY is 2147483648 and` |
|       - |  362 | ` * CURLAUTH_ANY is -17 (libcurl's ~CURLAUTH_DIGEST_IE over an unsigned long),` |
|       - |  363 | ` * so the row carries a 64-bit value and the expander reads it through the row` |
|       - |  364 | ` * pointer rather than through SX_INT_TO_PTR.` |
|       - |  365 | ` */` |
|       - |  366 | `static const struct CurlConstant {` |
|       - |  367 | `	const char *zName;` |
|       - |  368 | `	sxi64 iValue;` |
|       - |  369 | `} aCurlConst[] = {` |
|       - |  370 | `	{ "CURLOPT_AUTOREFERER",                     CURLOPT_AUTOREFERER },` |
|       - |  371 | `	{ "CURLOPT_BINARYTRANSFER",                  19914 },` |
|       - |  372 | `	{ "CURLOPT_BUFFERSIZE",                      CURLOPT_BUFFERSIZE },` |
|       - |  373 | `	{ "CURLOPT_CAINFO",                          CURLOPT_CAINFO },` |
|       - |  374 | `	{ "CURLOPT_CAPATH",                          CURLOPT_CAPATH },` |
|       - |  375 | `	{ "CURLOPT_CONNECTTIMEOUT",                  CURLOPT_CONNECTTIMEOUT },` |
|       - |  376 | `	{ "CURLOPT_COOKIE",                          CURLOPT_COOKIE },` |
|       - |  377 | `	{ "CURLOPT_COOKIEFILE",                      CURLOPT_COOKIEFILE },` |
|       - |  378 | `	{ "CURLOPT_COOKIEJAR",                       CURLOPT_COOKIEJAR },` |
|       - |  379 | `	{ "CURLOPT_COOKIESESSION",                   CURLOPT_COOKIESESSION },` |
|       - |  380 | `	{ "CURLOPT_CRLF",                            CURLOPT_CRLF },` |
|       - |  381 | `	{ "CURLOPT_CUSTOMREQUEST",                   CURLOPT_CUSTOMREQUEST },` |
|       - |  382 | `	{ "CURLOPT_DNS_CACHE_TIMEOUT",               CURLOPT_DNS_CACHE_TIMEOUT },` |
|       - |  383 | `	{ "CURLOPT_DNS_USE_GLOBAL_CACHE",            CURLOPT_DNS_USE_GLOBAL_CACHE },` |
|       - |  384 | `	{ "CURLOPT_EGDSOCKET",                       CURLOPT_EGDSOCKET },` |
|       - |  385 | `	{ "CURLOPT_ENCODING",                        CURLOPT_ENCODING },` |
|       - |  386 | `	{ "CURLOPT_FAILONERROR",                     CURLOPT_FAILONERROR },` |
|       - |  387 | `	{ "CURLOPT_FILE",                            CURLOPT_FILE },` |
|       - |  388 | `	{ "CURLOPT_FILETIME",                        CURLOPT_FILETIME },` |
|       - |  389 | `	{ "CURLOPT_FOLLOWLOCATION",                  CURLOPT_FOLLOWLOCATION },` |
|       - |  390 | `	{ "CURLOPT_FORBID_REUSE",                    CURLOPT_FORBID_REUSE },` |
|       - |  391 | `	{ "CURLOPT_FRESH_CONNECT",                   CURLOPT_FRESH_CONNECT },` |
|       - |  392 | `	{ "CURLOPT_FTPAPPEND",                       CURLOPT_FTPAPPEND },` |
|       - |  393 | `	{ "CURLOPT_FTPLISTONLY",                     CURLOPT_FTPLISTONLY },` |
|       - |  394 | `	{ "CURLOPT_FTPPORT",                         CURLOPT_FTPPORT },` |
|       - |  395 | `	{ "CURLOPT_FTP_USE_EPRT",                    CURLOPT_FTP_USE_EPRT },` |
|       - |  396 | `	{ "CURLOPT_FTP_USE_EPSV",                    CURLOPT_FTP_USE_EPSV },` |
|       - |  397 | `	{ "CURLOPT_HEADER",                          CURLOPT_HEADER },` |
|       - |  398 | `	{ "CURLOPT_HEADERFUNCTION",                  CURLOPT_HEADERFUNCTION },` |
|       - |  399 | `	{ "CURLOPT_HTTP200ALIASES",                  CURLOPT_HTTP200ALIASES },` |
|       - |  400 | `	{ "CURLOPT_HTTPGET",                         CURLOPT_HTTPGET },` |
|       - |  401 | `	{ "CURLOPT_HTTPHEADER",                      CURLOPT_HTTPHEADER },` |
|       - |  402 | `	{ "CURLOPT_HTTPPROXYTUNNEL",                 CURLOPT_HTTPPROXYTUNNEL },` |
|       - |  403 | `	{ "CURLOPT_HTTP_VERSION",                    CURLOPT_HTTP_VERSION },` |
|       - |  404 | `	{ "CURLOPT_INFILE",                          CURLOPT_INFILE },` |
|       - |  405 | `	{ "CURLOPT_INFILESIZE",                      CURLOPT_INFILESIZE },` |
|       - |  406 | `	{ "CURLOPT_INFILESIZE_LARGE",                CURLOPT_INFILESIZE_LARGE },` |
|       - |  407 | `	{ "CURLOPT_INTERFACE",                       CURLOPT_INTERFACE },` |
|       - |  408 | `	{ "CURLOPT_KRB4LEVEL",                       CURLOPT_KRB4LEVEL },` |
|       - |  409 | `	{ "CURLOPT_LOW_SPEED_LIMIT",                 CURLOPT_LOW_SPEED_LIMIT },` |
|       - |  410 | `	{ "CURLOPT_LOW_SPEED_TIME",                  CURLOPT_LOW_SPEED_TIME },` |
|       - |  411 | `	{ "CURLOPT_MAXCONNECTS",                     CURLOPT_MAXCONNECTS },` |
|       - |  412 | `	{ "CURLOPT_MAXREDIRS",                       CURLOPT_MAXREDIRS },` |
|       - |  413 | `	{ "CURLOPT_NETRC",                           CURLOPT_NETRC },` |
|       - |  414 | `	{ "CURLOPT_NOBODY",                          CURLOPT_NOBODY },` |
|       - |  415 | `	{ "CURLOPT_NOPROGRESS",                      CURLOPT_NOPROGRESS },` |
|       - |  416 | `	{ "CURLOPT_NOSIGNAL",                        CURLOPT_NOSIGNAL },` |
|       - |  417 | `	{ "CURLOPT_PORT",                            CURLOPT_PORT },` |
|       - |  418 | `	{ "CURLOPT_POST",                            CURLOPT_POST },` |
|       - |  419 | `	{ "CURLOPT_POSTFIELDS",                      CURLOPT_POSTFIELDS },` |
|       - |  420 | `	{ "CURLOPT_POSTQUOTE",                       CURLOPT_POSTQUOTE },` |
|       - |  421 | `	{ "CURLOPT_PREQUOTE",                        CURLOPT_PREQUOTE },` |
|       - |  422 | `	{ "CURLOPT_PRIVATE",                         CURLOPT_PRIVATE },` |
|       - |  423 | `	{ "CURLOPT_PROGRESSFUNCTION",                CURLOPT_PROGRESSFUNCTION },` |
|       - |  424 | `	{ "CURLOPT_PROXY",                           CURLOPT_PROXY },` |
|       - |  425 | `	{ "CURLOPT_PROXYPORT",                       CURLOPT_PROXYPORT },` |
|       - |  426 | `	{ "CURLOPT_PROXYTYPE",                       CURLOPT_PROXYTYPE },` |
|       - |  427 | `	{ "CURLOPT_PROXYUSERPWD",                    CURLOPT_PROXYUSERPWD },` |
|       - |  428 | `	{ "CURLOPT_PUT",                             CURLOPT_PUT },` |
|       - |  429 | `	{ "CURLOPT_QUOTE",                           CURLOPT_QUOTE },` |
|       - |  430 | `	{ "CURLOPT_RANDOM_FILE",                     CURLOPT_RANDOM_FILE },` |
|       - |  431 | `	{ "CURLOPT_RANGE",                           CURLOPT_RANGE },` |
|       - |  432 | `	{ "CURLOPT_READDATA",                        CURLOPT_READDATA },` |
|       - |  433 | `	{ "CURLOPT_READFUNCTION",                    CURLOPT_READFUNCTION },` |
|       - |  434 | `	{ "CURLOPT_REFERER",                         CURLOPT_REFERER },` |
|       - |  435 | `	{ "CURLOPT_RESUME_FROM",                     CURLOPT_RESUME_FROM },` |
|       - |  436 | `	{ "CURLOPT_RETURNTRANSFER",                  19913 },` |
|       - |  437 | `	{ "CURLOPT_SHARE",                           CURLOPT_SHARE },` |
|       - |  438 | `	{ "CURLOPT_SSLCERT",                         CURLOPT_SSLCERT },` |
|       - |  439 | `	{ "CURLOPT_SSLCERTPASSWD",                   CURLOPT_SSLCERTPASSWD },` |
|       - |  440 | `	{ "CURLOPT_SSLCERTTYPE",                     CURLOPT_SSLCERTTYPE },` |
|       - |  441 | `	{ "CURLOPT_SSLENGINE",                       CURLOPT_SSLENGINE },` |
|       - |  442 | `	{ "CURLOPT_SSLENGINE_DEFAULT",               CURLOPT_SSLENGINE_DEFAULT },` |
|       - |  443 | `	{ "CURLOPT_SSLKEY",                          CURLOPT_SSLKEY },` |
|       - |  444 | `	{ "CURLOPT_SSLKEYPASSWD",                    CURLOPT_SSLKEYPASSWD },` |
|       - |  445 | `	{ "CURLOPT_SSLKEYTYPE",                      CURLOPT_SSLKEYTYPE },` |
|       - |  446 | `	{ "CURLOPT_SSLVERSION",                      CURLOPT_SSLVERSION },` |
|       - |  447 | `	{ "CURLOPT_SSL_CIPHER_LIST",                 CURLOPT_SSL_CIPHER_LIST },` |
|       - |  448 | `	{ "CURLOPT_SSL_VERIFYHOST",                  CURLOPT_SSL_VERIFYHOST },` |
|       - |  449 | `	{ "CURLOPT_SSL_VERIFYPEER",                  CURLOPT_SSL_VERIFYPEER },` |
|       - |  450 | `	{ "CURLOPT_STDERR",                          CURLOPT_STDERR },` |
|       - |  451 | `	{ "CURLOPT_TELNETOPTIONS",                   CURLOPT_TELNETOPTIONS },` |
|       - |  452 | `	{ "CURLOPT_TIMECONDITION",                   CURLOPT_TIMECONDITION },` |
|       - |  453 | `	{ "CURLOPT_TIMEOUT",                         CURLOPT_TIMEOUT },` |
|       - |  454 | `	{ "CURLOPT_TIMEVALUE",                       CURLOPT_TIMEVALUE },` |
|       - |  455 | `	{ "CURLOPT_TRANSFERTEXT",                    CURLOPT_TRANSFERTEXT },` |
|       - |  456 | `	{ "CURLOPT_UNRESTRICTED_AUTH",               CURLOPT_UNRESTRICTED_AUTH },` |
|       - |  457 | `	{ "CURLOPT_UPLOAD",                          CURLOPT_UPLOAD },` |
|       - |  458 | `	{ "CURLOPT_URL",                             CURLOPT_URL },` |
|       - |  459 | `	{ "CURLOPT_USERAGENT",                       CURLOPT_USERAGENT },` |
|       - |  460 | `	{ "CURLOPT_USERPWD",                         CURLOPT_USERPWD },` |
|       - |  461 | `	{ "CURLOPT_VERBOSE",                         CURLOPT_VERBOSE },` |
|       - |  462 | `	{ "CURLOPT_WRITEFUNCTION",                   CURLOPT_WRITEFUNCTION },` |
|       - |  463 | `	{ "CURLOPT_WRITEHEADER",                     CURLOPT_WRITEHEADER },` |
|       - |  464 | `	{ "CURLOPT_XFERINFOFUNCTION",                CURLOPT_XFERINFOFUNCTION },` |
|       - |  465 | `	{ "CURLOPT_DEBUGFUNCTION",                   CURLOPT_DEBUGFUNCTION },` |
|       - |  466 | `	{ "CURLINFO_TEXT",                           CURLINFO_TEXT },` |
|       - |  467 | `	{ "CURLINFO_HEADER_IN",                      CURLINFO_HEADER_IN },` |
|       - |  468 | `	{ "CURLINFO_DATA_IN",                        CURLINFO_DATA_IN },` |
|       - |  469 | `	{ "CURLINFO_DATA_OUT",                       CURLINFO_DATA_OUT },` |
|       - |  470 | `	{ "CURLINFO_SSL_DATA_OUT",                   CURLINFO_SSL_DATA_OUT },` |
|       - |  471 | `	{ "CURLINFO_SSL_DATA_IN",                    CURLINFO_SSL_DATA_IN },` |
|       - |  472 | `	{ "CURLE_ABORTED_BY_CALLBACK",               CURLE_ABORTED_BY_CALLBACK },` |
|       - |  473 | `	{ "CURLE_BAD_CALLING_ORDER",                 CURLE_BAD_CALLING_ORDER },` |
|       - |  474 | `	{ "CURLE_BAD_CONTENT_ENCODING",              CURLE_BAD_CONTENT_ENCODING },` |
|       - |  475 | `	{ "CURLE_BAD_DOWNLOAD_RESUME",               CURLE_BAD_DOWNLOAD_RESUME },` |
|       - |  476 | `	{ "CURLE_BAD_FUNCTION_ARGUMENT",             CURLE_BAD_FUNCTION_ARGUMENT },` |
|       - |  477 | `	{ "CURLE_BAD_PASSWORD_ENTERED",              CURLE_BAD_PASSWORD_ENTERED },` |
|       - |  478 | `	{ "CURLE_COULDNT_CONNECT",                   CURLE_COULDNT_CONNECT },` |
|       - |  479 | `	{ "CURLE_COULDNT_RESOLVE_HOST",              CURLE_COULDNT_RESOLVE_HOST },` |
|       - |  480 | `	{ "CURLE_COULDNT_RESOLVE_PROXY",             CURLE_COULDNT_RESOLVE_PROXY },` |
|       - |  481 | `	{ "CURLE_FAILED_INIT",                       CURLE_FAILED_INIT },` |
|       - |  482 | `	{ "CURLE_FILE_COULDNT_READ_FILE",            CURLE_FILE_COULDNT_READ_FILE },` |
|       - |  483 | `	{ "CURLE_FTP_ACCESS_DENIED",                 CURLE_FTP_ACCESS_DENIED },` |
|       - |  484 | `	{ "CURLE_FTP_BAD_DOWNLOAD_RESUME",           CURLE_FTP_BAD_DOWNLOAD_RESUME },` |
|       - |  485 | `	{ "CURLE_FTP_CANT_GET_HOST",                 CURLE_FTP_CANT_GET_HOST },` |
|       - |  486 | `	{ "CURLE_FTP_CANT_RECONNECT",                CURLE_FTP_CANT_RECONNECT },` |
|       - |  487 | `	{ "CURLE_FTP_COULDNT_GET_SIZE",              CURLE_FTP_COULDNT_GET_SIZE },` |
|       - |  488 | `	{ "CURLE_FTP_COULDNT_RETR_FILE",             CURLE_FTP_COULDNT_RETR_FILE },` |
|       - |  489 | `	{ "CURLE_FTP_COULDNT_SET_ASCII",             CURLE_FTP_COULDNT_SET_ASCII },` |
|       - |  490 | `	{ "CURLE_FTP_COULDNT_SET_BINARY",            CURLE_FTP_COULDNT_SET_BINARY },` |
|       - |  491 | `	{ "CURLE_FTP_COULDNT_STOR_FILE",             CURLE_FTP_COULDNT_STOR_FILE },` |
|       - |  492 | `	{ "CURLE_FTP_COULDNT_USE_REST",              CURLE_FTP_COULDNT_USE_REST },` |
|       - |  493 | `	{ "CURLE_FTP_PARTIAL_FILE",                  CURLE_FTP_PARTIAL_FILE },` |
|       - |  494 | `	{ "CURLE_FTP_PORT_FAILED",                   CURLE_FTP_PORT_FAILED },` |
|       - |  495 | `	{ "CURLE_FTP_QUOTE_ERROR",                   CURLE_FTP_QUOTE_ERROR },` |
|       - |  496 | `	{ "CURLE_FTP_USER_PASSWORD_INCORRECT",       CURLE_FTP_USER_PASSWORD_INCORRECT },` |
|       - |  497 | `	{ "CURLE_FTP_WEIRD_227_FORMAT",              CURLE_FTP_WEIRD_227_FORMAT },` |
|       - |  498 | `	{ "CURLE_FTP_WEIRD_PASS_REPLY",              CURLE_FTP_WEIRD_PASS_REPLY },` |
|       - |  499 | `	{ "CURLE_FTP_WEIRD_PASV_REPLY",              CURLE_FTP_WEIRD_PASV_REPLY },` |
|       - |  500 | `	{ "CURLE_FTP_WEIRD_SERVER_REPLY",            CURLE_FTP_WEIRD_SERVER_REPLY },` |
|       - |  501 | `	{ "CURLE_FTP_WEIRD_USER_REPLY",              CURLE_FTP_WEIRD_USER_REPLY },` |
|       - |  502 | `	{ "CURLE_FTP_WRITE_ERROR",                   CURLE_FTP_WRITE_ERROR },` |
|       - |  503 | `	{ "CURLE_FUNCTION_NOT_FOUND",                CURLE_FUNCTION_NOT_FOUND },` |
|       - |  504 | `	{ "CURLE_GOT_NOTHING",                       CURLE_GOT_NOTHING },` |
|       - |  505 | `	{ "CURLE_HTTP_NOT_FOUND",                    CURLE_HTTP_NOT_FOUND },` |
|       - |  506 | `	{ "CURLE_HTTP_PORT_FAILED",                  CURLE_HTTP_PORT_FAILED },` |
|       - |  507 | `	{ "CURLE_HTTP_POST_ERROR",                   CURLE_HTTP_POST_ERROR },` |
|       - |  508 | `	{ "CURLE_HTTP_RANGE_ERROR",                  CURLE_HTTP_RANGE_ERROR },` |
|       - |  509 | `	{ "CURLE_HTTP_RETURNED_ERROR",               CURLE_HTTP_RETURNED_ERROR },` |
|       - |  510 | `	{ "CURLE_LDAP_CANNOT_BIND",                  CURLE_LDAP_CANNOT_BIND },` |
|       - |  511 | `	{ "CURLE_LDAP_SEARCH_FAILED",                CURLE_LDAP_SEARCH_FAILED },` |
|       - |  512 | `	{ "CURLE_LIBRARY_NOT_FOUND",                 CURLE_LIBRARY_NOT_FOUND },` |
|       - |  513 | `	{ "CURLE_MALFORMAT_USER",                    CURLE_MALFORMAT_USER },` |
|       - |  514 | `	{ "CURLE_OBSOLETE",                          CURLE_OBSOLETE },` |
|       - |  515 | `	{ "CURLE_OK",                                CURLE_OK },` |
|       - |  516 | `	{ "CURLE_OPERATION_TIMEDOUT",                CURLE_OPERATION_TIMEDOUT },` |
|       - |  517 | `	{ "CURLE_OPERATION_TIMEOUTED",               CURLE_OPERATION_TIMEOUTED },` |
|       - |  518 | `	{ "CURLE_OUT_OF_MEMORY",                     CURLE_OUT_OF_MEMORY },` |
|       - |  519 | `	{ "CURLE_PARTIAL_FILE",                      CURLE_PARTIAL_FILE },` |
|       - |  520 | `	{ "CURLE_READ_ERROR",                        CURLE_READ_ERROR },` |
|       - |  521 | `	{ "CURLE_RECV_ERROR",                        CURLE_RECV_ERROR },` |
|       - |  522 | `	{ "CURLE_SEND_ERROR",                        CURLE_SEND_ERROR },` |
|       - |  523 | `	{ "CURLE_SHARE_IN_USE",                      CURLE_SHARE_IN_USE },` |
|       - |  524 | `	{ "CURLE_SSL_CACERT",                        CURLE_SSL_CACERT },` |
|       - |  525 | `	{ "CURLE_SSL_CERTPROBLEM",                   CURLE_SSL_CERTPROBLEM },` |
|       - |  526 | `	{ "CURLE_SSL_CIPHER",                        CURLE_SSL_CIPHER },` |
|       - |  527 | `	{ "CURLE_SSL_CONNECT_ERROR",                 CURLE_SSL_CONNECT_ERROR },` |
|       - |  528 | `	{ "CURLE_SSL_ENGINE_NOTFOUND",               CURLE_SSL_ENGINE_NOTFOUND },` |
|       - |  529 | `	{ "CURLE_SSL_ENGINE_SETFAILED",              CURLE_SSL_ENGINE_SETFAILED },` |
|       - |  530 | `	{ "CURLE_SSL_PEER_CERTIFICATE",              CURLE_SSL_PEER_CERTIFICATE },` |
|       - |  531 | `	{ "CURLE_SSL_PINNEDPUBKEYNOTMATCH",          CURLE_SSL_PINNEDPUBKEYNOTMATCH },` |
|       - |  532 | `	{ "CURLE_TELNET_OPTION_SYNTAX",              CURLE_TELNET_OPTION_SYNTAX },` |
|       - |  533 | `	{ "CURLE_TOO_MANY_REDIRECTS",                CURLE_TOO_MANY_REDIRECTS },` |
|       - |  534 | `	{ "CURLE_UNKNOWN_TELNET_OPTION",             CURLE_UNKNOWN_TELNET_OPTION },` |
|       - |  535 | `	{ "CURLE_UNSUPPORTED_PROTOCOL",              CURLE_UNSUPPORTED_PROTOCOL },` |
|       - |  536 | `	{ "CURLE_URL_MALFORMAT",                     CURLE_URL_MALFORMAT },` |
|       - |  537 | `	{ "CURLE_URL_MALFORMAT_USER",                CURLE_URL_MALFORMAT_USER },` |
|       - |  538 | `	{ "CURLE_WRITE_ERROR",                       CURLE_WRITE_ERROR },` |
|       - |  539 | `	{ "CURLINFO_CONNECT_TIME",                   CURLINFO_CONNECT_TIME },` |
|       - |  540 | `	{ "CURLINFO_CONTENT_LENGTH_DOWNLOAD",        CURLINFO_CONTENT_LENGTH_DOWNLOAD },` |
|       - |  541 | `	{ "CURLINFO_CONTENT_LENGTH_UPLOAD",          CURLINFO_CONTENT_LENGTH_UPLOAD },` |
|       - |  542 | `	{ "CURLINFO_CONTENT_TYPE",                   CURLINFO_CONTENT_TYPE },` |
|       - |  543 | `	{ "CURLINFO_EFFECTIVE_URL",                  CURLINFO_EFFECTIVE_URL },` |
|       - |  544 | `	{ "CURLINFO_FILETIME",                       CURLINFO_FILETIME },` |
|       - |  545 | `	{ "CURLINFO_HEADER_OUT",                     CURLINFO_HEADER_OUT },` |
|       - |  546 | `	{ "CURLINFO_HEADER_SIZE",                    CURLINFO_HEADER_SIZE },` |
|       - |  547 | `	{ "CURLINFO_HTTP_CODE",                      CURLINFO_HTTP_CODE },` |
|       - |  548 | `	{ "CURLINFO_LASTONE",                        CURLINFO_LASTONE },` |
|       - |  549 | `	{ "CURLINFO_NAMELOOKUP_TIME",                CURLINFO_NAMELOOKUP_TIME },` |
|       - |  550 | `	{ "CURLINFO_PRETRANSFER_TIME",               CURLINFO_PRETRANSFER_TIME },` |
|       - |  551 | `	{ "CURLINFO_PRIVATE",                        CURLINFO_PRIVATE },` |
|       - |  552 | `	{ "CURLINFO_REDIRECT_COUNT",                 CURLINFO_REDIRECT_COUNT },` |
|       - |  553 | `	{ "CURLINFO_REDIRECT_TIME",                  CURLINFO_REDIRECT_TIME },` |
|       - |  554 | `	{ "CURLINFO_REQUEST_SIZE",                   CURLINFO_REQUEST_SIZE },` |
|       - |  555 | `	{ "CURLINFO_SIZE_DOWNLOAD",                  CURLINFO_SIZE_DOWNLOAD },` |
|       - |  556 | `	{ "CURLINFO_SIZE_UPLOAD",                    CURLINFO_SIZE_UPLOAD },` |
|       - |  557 | `	{ "CURLINFO_SPEED_DOWNLOAD",                 CURLINFO_SPEED_DOWNLOAD },` |
|       - |  558 | `	{ "CURLINFO_SPEED_UPLOAD",                   CURLINFO_SPEED_UPLOAD },` |
|       - |  559 | `	{ "CURLINFO_SSL_VERIFYRESULT",               CURLINFO_SSL_VERIFYRESULT },` |
|       - |  560 | `	{ "CURLINFO_STARTTRANSFER_TIME",             CURLINFO_STARTTRANSFER_TIME },` |
|       - |  561 | `	{ "CURLINFO_TOTAL_TIME",                     CURLINFO_TOTAL_TIME },` |
|       - |  562 | `	{ "CURLINFO_EFFECTIVE_METHOD",               CURLINFO_EFFECTIVE_METHOD },` |
|       - |  563 | `	{ "CURLINFO_CAPATH",                         CURLINFO_CAPATH },` |
|       - |  564 | `	{ "CURLINFO_CAINFO",                         CURLINFO_CAINFO },` |
|       - |  565 | `	{ "CURLMSG_DONE",                            CURLMSG_DONE },` |
|       - |  566 | `	{ "CURLVERSION_NOW",                         CURLVERSION_NOW },` |
|       - |  567 | `	{ "CURLM_BAD_EASY_HANDLE",                   CURLM_BAD_EASY_HANDLE },` |
|       - |  568 | `	{ "CURLM_BAD_HANDLE",                        CURLM_BAD_HANDLE },` |
|       - |  569 | `	{ "CURLM_CALL_MULTI_PERFORM",                CURLM_CALL_MULTI_PERFORM },` |
|       - |  570 | `	{ "CURLM_INTERNAL_ERROR",                    CURLM_INTERNAL_ERROR },` |
|       - |  571 | `	{ "CURLM_OK",                                CURLM_OK },` |
|       - |  572 | `	{ "CURLM_OUT_OF_MEMORY",                     CURLM_OUT_OF_MEMORY },` |
|       - |  573 | `	{ "CURLM_ADDED_ALREADY",                     CURLM_ADDED_ALREADY },` |
|       - |  574 | `	{ "CURLPROXY_HTTP",                          CURLPROXY_HTTP },` |
|       - |  575 | `	{ "CURLPROXY_SOCKS4",                        CURLPROXY_SOCKS4 },` |
|       - |  576 | `	{ "CURLPROXY_SOCKS5",                        CURLPROXY_SOCKS5 },` |
|       - |  577 | `	{ "CURLSHOPT_NONE",                          CURLSHOPT_NONE },` |
|       - |  578 | `	{ "CURLSHOPT_SHARE",                         CURLSHOPT_SHARE },` |
|       - |  579 | `	{ "CURLSHOPT_UNSHARE",                       CURLSHOPT_UNSHARE },` |
|       - |  580 | `	{ "CURL_HTTP_VERSION_1_0",                   CURL_HTTP_VERSION_1_0 },` |
|       - |  581 | `	{ "CURL_HTTP_VERSION_1_1",                   CURL_HTTP_VERSION_1_1 },` |
|       - |  582 | `	{ "CURL_HTTP_VERSION_NONE",                  CURL_HTTP_VERSION_NONE },` |
|       - |  583 | `	{ "CURL_LOCK_DATA_COOKIE",                   CURL_LOCK_DATA_COOKIE },` |
|       - |  584 | `	{ "CURL_LOCK_DATA_DNS",                      CURL_LOCK_DATA_DNS },` |
|       - |  585 | `	{ "CURL_LOCK_DATA_SSL_SESSION",              CURL_LOCK_DATA_SSL_SESSION },` |
|       - |  586 | `	{ "CURL_NETRC_IGNORED",                      CURL_NETRC_IGNORED },` |
|       - |  587 | `	{ "CURL_NETRC_OPTIONAL",                     CURL_NETRC_OPTIONAL },` |
|       - |  588 | `	{ "CURL_NETRC_REQUIRED",                     CURL_NETRC_REQUIRED },` |
|       - |  589 | `	{ "CURL_SSLVERSION_DEFAULT",                 CURL_SSLVERSION_DEFAULT },` |
|       - |  590 | `	{ "CURL_SSLVERSION_SSLv2",                   CURL_SSLVERSION_SSLv2 },` |
|       - |  591 | `	{ "CURL_SSLVERSION_SSLv3",                   CURL_SSLVERSION_SSLv3 },` |
|       - |  592 | `	{ "CURL_SSLVERSION_TLSv1",                   CURL_SSLVERSION_TLSv1 },` |
|       - |  593 | `	{ "CURL_TIMECOND_IFMODSINCE",                CURL_TIMECOND_IFMODSINCE },` |
|       - |  594 | `	{ "CURL_TIMECOND_IFUNMODSINCE",              CURL_TIMECOND_IFUNMODSINCE },` |
|       - |  595 | `	{ "CURL_TIMECOND_LASTMOD",                   CURL_TIMECOND_LASTMOD },` |
|       - |  596 | `	{ "CURL_TIMECOND_NONE",                      CURL_TIMECOND_NONE },` |
|       - |  597 | `	{ "CURL_VERSION_ASYNCHDNS",                  CURL_VERSION_ASYNCHDNS },` |
|       - |  598 | `	{ "CURL_VERSION_CONV",                       CURL_VERSION_CONV },` |
|       - |  599 | `	{ "CURL_VERSION_DEBUG",                      CURL_VERSION_DEBUG },` |
|       - |  600 | `	{ "CURL_VERSION_GSSNEGOTIATE",               CURL_VERSION_GSSNEGOTIATE },` |
|       - |  601 | `	{ "CURL_VERSION_IDN",                        CURL_VERSION_IDN },` |
|       - |  602 | `	{ "CURL_VERSION_IPV6",                       CURL_VERSION_IPV6 },` |
|       - |  603 | `	{ "CURL_VERSION_KERBEROS4",                  CURL_VERSION_KERBEROS4 },` |
|       - |  604 | `	{ "CURL_VERSION_LARGEFILE",                  CURL_VERSION_LARGEFILE },` |
|       - |  605 | `	{ "CURL_VERSION_LIBZ",                       CURL_VERSION_LIBZ },` |
|       - |  606 | `	{ "CURL_VERSION_NTLM",                       CURL_VERSION_NTLM },` |
|       - |  607 | `	{ "CURL_VERSION_SPNEGO",                     CURL_VERSION_SPNEGO },` |
|       - |  608 | `	{ "CURL_VERSION_SSL",                        CURL_VERSION_SSL },` |
|       - |  609 | `	{ "CURL_VERSION_SSPI",                       CURL_VERSION_SSPI },` |
|       - |  610 | `	{ "CURLOPT_HTTPAUTH",                        CURLOPT_HTTPAUTH },` |
|       - |  611 | `	{ "CURLAUTH_ANY",                            CURLAUTH_ANY },` |
|       - |  612 | `	{ "CURLAUTH_ANYSAFE",                        CURLAUTH_ANYSAFE },` |
|       - |  613 | `	{ "CURLAUTH_BASIC",                          CURLAUTH_BASIC },` |
|       - |  614 | `	{ "CURLAUTH_DIGEST",                         CURLAUTH_DIGEST },` |
|       - |  615 | `	{ "CURLAUTH_GSSNEGOTIATE",                   CURLAUTH_GSSNEGOTIATE },` |
|       - |  616 | `	{ "CURLAUTH_NONE",                           CURLAUTH_NONE },` |
|       - |  617 | `	{ "CURLAUTH_NTLM",                           CURLAUTH_NTLM },` |
|       - |  618 | `	{ "CURLINFO_HTTP_CONNECTCODE",               CURLINFO_HTTP_CONNECTCODE },` |
|       - |  619 | `	{ "CURLOPT_FTP_CREATE_MISSING_DIRS",         CURLOPT_FTP_CREATE_MISSING_DIRS },` |
|       - |  620 | `	{ "CURLOPT_PROXYAUTH",                       CURLOPT_PROXYAUTH },` |
|       - |  621 | `	{ "CURLE_FILESIZE_EXCEEDED",                 CURLE_FILESIZE_EXCEEDED },` |
|       - |  622 | `	{ "CURLE_LDAP_INVALID_URL",                  CURLE_LDAP_INVALID_URL },` |
|       - |  623 | `	{ "CURLINFO_HTTPAUTH_AVAIL",                 CURLINFO_HTTPAUTH_AVAIL },` |
|       - |  624 | `	{ "CURLINFO_RESPONSE_CODE",                  CURLINFO_RESPONSE_CODE },` |
|       - |  625 | `	{ "CURLINFO_PROXYAUTH_AVAIL",                CURLINFO_PROXYAUTH_AVAIL },` |
|       - |  626 | `	{ "CURLOPT_FTP_RESPONSE_TIMEOUT",            CURLOPT_FTP_RESPONSE_TIMEOUT },` |
|       - |  627 | `	{ "CURLOPT_SERVER_RESPONSE_TIMEOUT",         CURLOPT_SERVER_RESPONSE_TIMEOUT },` |
|       - |  628 | `	{ "CURLOPT_IPRESOLVE",                       CURLOPT_IPRESOLVE },` |
|       - |  629 | `	{ "CURLOPT_MAXFILESIZE",                     CURLOPT_MAXFILESIZE },` |
|       - |  630 | `	{ "CURL_IPRESOLVE_V4",                       CURL_IPRESOLVE_V4 },` |
|       - |  631 | `	{ "CURL_IPRESOLVE_V6",                       CURL_IPRESOLVE_V6 },` |
|       - |  632 | `	{ "CURL_IPRESOLVE_WHATEVER",                 CURL_IPRESOLVE_WHATEVER },` |
|       - |  633 | `	{ "CURLE_FTP_SSL_FAILED",                    CURLE_FTP_SSL_FAILED },` |
|       - |  634 | `	{ "CURLFTPSSL_ALL",                          CURLFTPSSL_ALL },` |
|       - |  635 | `	{ "CURLFTPSSL_CONTROL",                      CURLFTPSSL_CONTROL },` |
|       - |  636 | `	{ "CURLFTPSSL_NONE",                         CURLFTPSSL_NONE },` |
|       - |  637 | `	{ "CURLFTPSSL_TRY",                          CURLFTPSSL_TRY },` |
|       - |  638 | `	{ "CURLOPT_FTP_SSL",                         CURLOPT_FTP_SSL },` |
|       - |  639 | `	{ "CURLOPT_NETRC_FILE",                      CURLOPT_NETRC_FILE },` |
|       - |  640 | `	{ "CURLOPT_MAXFILESIZE_LARGE",               CURLOPT_MAXFILESIZE_LARGE },` |
|       - |  641 | `	{ "CURLOPT_TCP_NODELAY",                     CURLOPT_TCP_NODELAY },` |
|       - |  642 | `	{ "CURLFTPAUTH_DEFAULT",                     CURLFTPAUTH_DEFAULT },` |
|       - |  643 | `	{ "CURLFTPAUTH_SSL",                         CURLFTPAUTH_SSL },` |
|       - |  644 | `	{ "CURLFTPAUTH_TLS",                         CURLFTPAUTH_TLS },` |
|       - |  645 | `	{ "CURLOPT_FTPSSLAUTH",                      CURLOPT_FTPSSLAUTH },` |
|       - |  646 | `	{ "CURLOPT_FTP_ACCOUNT",                     CURLOPT_FTP_ACCOUNT },` |
|       - |  647 | `	{ "CURLINFO_OS_ERRNO",                       CURLINFO_OS_ERRNO },` |
|       - |  648 | `	{ "CURLINFO_NUM_CONNECTS",                   CURLINFO_NUM_CONNECTS },` |
|       - |  649 | `	{ "CURLINFO_SSL_ENGINES",                    CURLINFO_SSL_ENGINES },` |
|       - |  650 | `	{ "CURLINFO_COOKIELIST",                     CURLINFO_COOKIELIST },` |
|       - |  651 | `	{ "CURLOPT_COOKIELIST",                      CURLOPT_COOKIELIST },` |
|       - |  652 | `	{ "CURLOPT_IGNORE_CONTENT_LENGTH",           CURLOPT_IGNORE_CONTENT_LENGTH },` |
|       - |  653 | `	{ "CURLOPT_FTP_SKIP_PASV_IP",                CURLOPT_FTP_SKIP_PASV_IP },` |
|       - |  654 | `	{ "CURLOPT_FTP_FILEMETHOD",                  CURLOPT_FTP_FILEMETHOD },` |
|       - |  655 | `	{ "CURLOPT_CONNECT_ONLY",                    CURLOPT_CONNECT_ONLY },` |
|       - |  656 | `	{ "CURLOPT_LOCALPORT",                       CURLOPT_LOCALPORT },` |
|       - |  657 | `	{ "CURLOPT_LOCALPORTRANGE",                  CURLOPT_LOCALPORTRANGE },` |
|       - |  658 | `	{ "CURLFTPMETHOD_DEFAULT",                   CURLFTPMETHOD_DEFAULT },` |
|       - |  659 | `	{ "CURLFTPMETHOD_MULTICWD",                  CURLFTPMETHOD_MULTICWD },` |
|       - |  660 | `	{ "CURLFTPMETHOD_NOCWD",                     CURLFTPMETHOD_NOCWD },` |
|       - |  661 | `	{ "CURLFTPMETHOD_SINGLECWD",                 CURLFTPMETHOD_SINGLECWD },` |
|       - |  662 | `	{ "CURLINFO_FTP_ENTRY_PATH",                 CURLINFO_FTP_ENTRY_PATH },` |
|       - |  663 | `	{ "CURLOPT_FTP_ALTERNATIVE_TO_USER",         CURLOPT_FTP_ALTERNATIVE_TO_USER },` |
|       - |  664 | `	{ "CURLOPT_MAX_RECV_SPEED_LARGE",            CURLOPT_MAX_RECV_SPEED_LARGE },` |
|       - |  665 | `	{ "CURLOPT_MAX_SEND_SPEED_LARGE",            CURLOPT_MAX_SEND_SPEED_LARGE },` |
|       - |  666 | `	{ "CURLE_SSL_CACERT_BADFILE",                CURLE_SSL_CACERT_BADFILE },` |
|       - |  667 | `	{ "CURLOPT_SSL_SESSIONID_CACHE",             CURLOPT_SSL_SESSIONID_CACHE },` |
|       - |  668 | `	{ "CURLMOPT_PIPELINING",                     CURLMOPT_PIPELINING },` |
|       - |  669 | `	{ "CURLE_SSH",                               CURLE_SSH },` |
|       - |  670 | `	{ "CURLOPT_FTP_SSL_CCC",                     CURLOPT_FTP_SSL_CCC },` |
|       - |  671 | `	{ "CURLOPT_SSH_AUTH_TYPES",                  CURLOPT_SSH_AUTH_TYPES },` |
|       - |  672 | `	{ "CURLOPT_SSH_PRIVATE_KEYFILE",             CURLOPT_SSH_PRIVATE_KEYFILE },` |
|       - |  673 | `	{ "CURLOPT_SSH_PUBLIC_KEYFILE",              CURLOPT_SSH_PUBLIC_KEYFILE },` |
|       - |  674 | `	{ "CURLFTPSSL_CCC_ACTIVE",                   CURLFTPSSL_CCC_ACTIVE },` |
|       - |  675 | `	{ "CURLFTPSSL_CCC_NONE",                     CURLFTPSSL_CCC_NONE },` |
|       - |  676 | `	{ "CURLFTPSSL_CCC_PASSIVE",                  CURLFTPSSL_CCC_PASSIVE },` |
|       - |  677 | `	{ "CURLOPT_CONNECTTIMEOUT_MS",               CURLOPT_CONNECTTIMEOUT_MS },` |
|       - |  678 | `	{ "CURLOPT_HTTP_CONTENT_DECODING",           CURLOPT_HTTP_CONTENT_DECODING },` |
|       - |  679 | `	{ "CURLOPT_HTTP_TRANSFER_DECODING",          CURLOPT_HTTP_TRANSFER_DECODING },` |
|       - |  680 | `	{ "CURLOPT_TIMEOUT_MS",                      CURLOPT_TIMEOUT_MS },` |
|       - |  681 | `	{ "CURLMOPT_MAXCONNECTS",                    CURLMOPT_MAXCONNECTS },` |
|       - |  682 | `	{ "CURLOPT_KRBLEVEL",                        CURLOPT_KRBLEVEL },` |
|       - |  683 | `	{ "CURLOPT_NEW_DIRECTORY_PERMS",             CURLOPT_NEW_DIRECTORY_PERMS },` |
|       - |  684 | `	{ "CURLOPT_NEW_FILE_PERMS",                  CURLOPT_NEW_FILE_PERMS },` |
|       - |  685 | `	{ "CURLOPT_APPEND",                          CURLOPT_APPEND },` |
|       - |  686 | `	{ "CURLOPT_DIRLISTONLY",                     CURLOPT_DIRLISTONLY },` |
|       - |  687 | `	{ "CURLOPT_USE_SSL",                         CURLOPT_USE_SSL },` |
|       - |  688 | `	{ "CURLUSESSL_ALL",                          CURLUSESSL_ALL },` |
|       - |  689 | `	{ "CURLUSESSL_CONTROL",                      CURLUSESSL_CONTROL },` |
|       - |  690 | `	{ "CURLUSESSL_NONE",                         CURLUSESSL_NONE },` |
|       - |  691 | `	{ "CURLUSESSL_TRY",                          CURLUSESSL_TRY },` |
|       - |  692 | `	{ "CURLOPT_SSH_HOST_PUBLIC_KEY_MD5",         CURLOPT_SSH_HOST_PUBLIC_KEY_MD5 },` |
|       - |  693 | `	{ "CURLOPT_PROXY_TRANSFER_MODE",             CURLOPT_PROXY_TRANSFER_MODE },` |
|       - |  694 | `	{ "CURLPAUSE_ALL",                           CURLPAUSE_ALL },` |
|       - |  695 | `	{ "CURLPAUSE_CONT",                          CURLPAUSE_CONT },` |
|       - |  696 | `	{ "CURLPAUSE_RECV",                          CURLPAUSE_RECV },` |
|       - |  697 | `	{ "CURLPAUSE_RECV_CONT",                     CURLPAUSE_RECV_CONT },` |
|       - |  698 | `	{ "CURLPAUSE_SEND",                          CURLPAUSE_SEND },` |
|       - |  699 | `	{ "CURLPAUSE_SEND_CONT",                     CURLPAUSE_SEND_CONT },` |
|       - |  700 | `	{ "CURL_READFUNC_PAUSE",                     CURL_READFUNC_PAUSE },` |
|       - |  701 | `	{ "CURL_WRITEFUNC_PAUSE",                    CURL_WRITEFUNC_PAUSE },` |
|       - |  702 | `	{ "CURLPROXY_SOCKS4A",                       CURLPROXY_SOCKS4A },` |
|       - |  703 | `	{ "CURLPROXY_SOCKS5_HOSTNAME",               CURLPROXY_SOCKS5_HOSTNAME },` |
|       - |  704 | `	{ "CURLINFO_REDIRECT_URL",                   CURLINFO_REDIRECT_URL },` |
|       - |  705 | `	{ "CURLINFO_APPCONNECT_TIME",                CURLINFO_APPCONNECT_TIME },` |
|       - |  706 | `	{ "CURLINFO_PRIMARY_IP",                     CURLINFO_PRIMARY_IP },` |
|       - |  707 | `	{ "CURLOPT_ADDRESS_SCOPE",                   CURLOPT_ADDRESS_SCOPE },` |
|       - |  708 | `	{ "CURLOPT_CRLFILE",                         CURLOPT_CRLFILE },` |
|       - |  709 | `	{ "CURLOPT_ISSUERCERT",                      CURLOPT_ISSUERCERT },` |
|       - |  710 | `	{ "CURLOPT_KEYPASSWD",                       CURLOPT_KEYPASSWD },` |
|       - |  711 | `	{ "CURLSSH_AUTH_ANY",                        CURLSSH_AUTH_ANY },` |
|       - |  712 | `	{ "CURLSSH_AUTH_DEFAULT",                    CURLSSH_AUTH_DEFAULT },` |
|       - |  713 | `	{ "CURLSSH_AUTH_HOST",                       CURLSSH_AUTH_HOST },` |
|       - |  714 | `	{ "CURLSSH_AUTH_KEYBOARD",                   CURLSSH_AUTH_KEYBOARD },` |
|       - |  715 | `	{ "CURLSSH_AUTH_NONE",                       CURLSSH_AUTH_NONE },` |
|       - |  716 | `	{ "CURLSSH_AUTH_PASSWORD",                   CURLSSH_AUTH_PASSWORD },` |
|       - |  717 | `	{ "CURLSSH_AUTH_PUBLICKEY",                  CURLSSH_AUTH_PUBLICKEY },` |
|       - |  718 | `	{ "CURLINFO_CERTINFO",                       CURLINFO_CERTINFO },` |
|       - |  719 | `	{ "CURLOPT_CERTINFO",                        CURLOPT_CERTINFO },` |
|       - |  720 | `	{ "CURLOPT_PASSWORD",                        CURLOPT_PASSWORD },` |
|       - |  721 | `	{ "CURLOPT_POSTREDIR",                       CURLOPT_POSTREDIR },` |
|       - |  722 | `	{ "CURLOPT_PROXYPASSWORD",                   CURLOPT_PROXYPASSWORD },` |
|       - |  723 | `	{ "CURLOPT_PROXYUSERNAME",                   CURLOPT_PROXYUSERNAME },` |
|       - |  724 | `	{ "CURLOPT_USERNAME",                        CURLOPT_USERNAME },` |
|       - |  725 | `	{ "CURL_REDIR_POST_301",                     CURL_REDIR_POST_301 },` |
|       - |  726 | `	{ "CURL_REDIR_POST_302",                     CURL_REDIR_POST_302 },` |
|       - |  727 | `	{ "CURL_REDIR_POST_ALL",                     CURL_REDIR_POST_ALL },` |
|       - |  728 | `	{ "CURLAUTH_DIGEST_IE",                      CURLAUTH_DIGEST_IE },` |
|       - |  729 | `	{ "CURLINFO_CONDITION_UNMET",                CURLINFO_CONDITION_UNMET },` |
|       - |  730 | `	{ "CURLOPT_NOPROXY",                         CURLOPT_NOPROXY },` |
|       - |  731 | `	{ "CURLOPT_PROTOCOLS",                       CURLOPT_PROTOCOLS },` |
|       - |  732 | `	{ "CURLOPT_REDIR_PROTOCOLS",                 CURLOPT_REDIR_PROTOCOLS },` |
|       - |  733 | `	{ "CURLOPT_SOCKS5_GSSAPI_NEC",               CURLOPT_SOCKS5_GSSAPI_NEC },` |
|       - |  734 | `	{ "CURLOPT_SOCKS5_GSSAPI_SERVICE",           CURLOPT_SOCKS5_GSSAPI_SERVICE },` |
|       - |  735 | `	{ "CURLOPT_TFTP_BLKSIZE",                    CURLOPT_TFTP_BLKSIZE },` |
|       - |  736 | `	{ "CURLPROTO_ALL",                           CURLPROTO_ALL },` |
|       - |  737 | `	{ "CURLPROTO_DICT",                          CURLPROTO_DICT },` |
|       - |  738 | `	{ "CURLPROTO_FILE",                          CURLPROTO_FILE },` |
|       - |  739 | `	{ "CURLPROTO_FTP",                           CURLPROTO_FTP },` |
|       - |  740 | `	{ "CURLPROTO_FTPS",                          CURLPROTO_FTPS },` |
|       - |  741 | `	{ "CURLPROTO_HTTP",                          CURLPROTO_HTTP },` |
|       - |  742 | `	{ "CURLPROTO_HTTPS",                         CURLPROTO_HTTPS },` |
|       - |  743 | `	{ "CURLPROTO_LDAP",                          CURLPROTO_LDAP },` |
|       - |  744 | `	{ "CURLPROTO_LDAPS",                         CURLPROTO_LDAPS },` |
|       - |  745 | `	{ "CURLPROTO_SCP",                           CURLPROTO_SCP },` |
|       - |  746 | `	{ "CURLPROTO_SFTP",                          CURLPROTO_SFTP },` |
|       - |  747 | `	{ "CURLPROTO_TELNET",                        CURLPROTO_TELNET },` |
|       - |  748 | `	{ "CURLPROTO_TFTP",                          CURLPROTO_TFTP },` |
|       - |  749 | `	{ "CURLPROXY_HTTP_1_0",                      CURLPROXY_HTTP_1_0 },` |
|       - |  750 | `	{ "CURLFTP_CREATE_DIR",                      CURLFTP_CREATE_DIR },` |
|       - |  751 | `	{ "CURLFTP_CREATE_DIR_NONE",                 CURLFTP_CREATE_DIR_NONE },` |
|       - |  752 | `	{ "CURLFTP_CREATE_DIR_RETRY",                CURLFTP_CREATE_DIR_RETRY },` |
|       - |  753 | `	{ "CURL_VERSION_CURLDEBUG",                  CURL_VERSION_CURLDEBUG },` |
|       - |  754 | `	{ "CURLOPT_SSH_KNOWNHOSTS",                  CURLOPT_SSH_KNOWNHOSTS },` |
|       - |  755 | `	{ "CURLKHMATCH_OK",                          CURLKHMATCH_OK },` |
|       - |  756 | `	{ "CURLKHMATCH_MISMATCH",                    CURLKHMATCH_MISMATCH },` |
|       - |  757 | `	{ "CURLKHMATCH_MISSING",                     CURLKHMATCH_MISSING },` |
|       - |  758 | `	{ "CURLKHMATCH_LAST",                        CURLKHMATCH_LAST },` |
|       - |  759 | `	{ "CURLINFO_RTSP_CLIENT_CSEQ",               CURLINFO_RTSP_CLIENT_CSEQ },` |
|       - |  760 | `	{ "CURLINFO_RTSP_CSEQ_RECV",                 CURLINFO_RTSP_CSEQ_RECV },` |
|       - |  761 | `	{ "CURLINFO_RTSP_SERVER_CSEQ",               CURLINFO_RTSP_SERVER_CSEQ },` |
|       - |  762 | `	{ "CURLINFO_RTSP_SESSION_ID",                CURLINFO_RTSP_SESSION_ID },` |
|       - |  763 | `	{ "CURLOPT_FTP_USE_PRET",                    CURLOPT_FTP_USE_PRET },` |
|       - |  764 | `	{ "CURLOPT_MAIL_FROM",                       CURLOPT_MAIL_FROM },` |
|       - |  765 | `	{ "CURLOPT_MAIL_RCPT",                       CURLOPT_MAIL_RCPT },` |
|       - |  766 | `	{ "CURLOPT_RTSP_CLIENT_CSEQ",                CURLOPT_RTSP_CLIENT_CSEQ },` |
|       - |  767 | `	{ "CURLOPT_RTSP_REQUEST",                    CURLOPT_RTSP_REQUEST },` |
|       - |  768 | `	{ "CURLOPT_RTSP_SERVER_CSEQ",                CURLOPT_RTSP_SERVER_CSEQ },` |
|       - |  769 | `	{ "CURLOPT_RTSP_SESSION_ID",                 CURLOPT_RTSP_SESSION_ID },` |
|       - |  770 | `	{ "CURLOPT_RTSP_STREAM_URI",                 CURLOPT_RTSP_STREAM_URI },` |
|       - |  771 | `	{ "CURLOPT_RTSP_TRANSPORT",                  CURLOPT_RTSP_TRANSPORT },` |
|       - |  772 | `	{ "CURLPROTO_IMAP",                          CURLPROTO_IMAP },` |
|       - |  773 | `	{ "CURLPROTO_IMAPS",                         CURLPROTO_IMAPS },` |
|       - |  774 | `	{ "CURLPROTO_POP3",                          CURLPROTO_POP3 },` |
|       - |  775 | `	{ "CURLPROTO_POP3S",                         CURLPROTO_POP3S },` |
|       - |  776 | `	{ "CURLPROTO_RTSP",                          CURLPROTO_RTSP },` |
|       - |  777 | `	{ "CURLPROTO_SMTP",                          CURLPROTO_SMTP },` |
|       - |  778 | `	{ "CURLPROTO_SMTPS",                         CURLPROTO_SMTPS },` |
|       - |  779 | `	{ "CURL_RTSPREQ_ANNOUNCE",                   CURL_RTSPREQ_ANNOUNCE },` |
|       - |  780 | `	{ "CURL_RTSPREQ_DESCRIBE",                   CURL_RTSPREQ_DESCRIBE },` |
|       - |  781 | `	{ "CURL_RTSPREQ_GET_PARAMETER",              CURL_RTSPREQ_GET_PARAMETER },` |
|       - |  782 | `	{ "CURL_RTSPREQ_OPTIONS",                    CURL_RTSPREQ_OPTIONS },` |
|       - |  783 | `	{ "CURL_RTSPREQ_PAUSE",                      CURL_RTSPREQ_PAUSE },` |
|       - |  784 | `	{ "CURL_RTSPREQ_PLAY",                       CURL_RTSPREQ_PLAY },` |
|       - |  785 | `	{ "CURL_RTSPREQ_RECEIVE",                    CURL_RTSPREQ_RECEIVE },` |
|       - |  786 | `	{ "CURL_RTSPREQ_RECORD",                     CURL_RTSPREQ_RECORD },` |
|       - |  787 | `	{ "CURL_RTSPREQ_SET_PARAMETER",              CURL_RTSPREQ_SET_PARAMETER },` |
|       - |  788 | `	{ "CURL_RTSPREQ_SETUP",                      CURL_RTSPREQ_SETUP },` |
|       - |  789 | `	{ "CURL_RTSPREQ_TEARDOWN",                   CURL_RTSPREQ_TEARDOWN },` |
|       - |  790 | `	{ "CURLINFO_LOCAL_IP",                       CURLINFO_LOCAL_IP },` |
|       - |  791 | `	{ "CURLINFO_LOCAL_PORT",                     CURLINFO_LOCAL_PORT },` |
|       - |  792 | `	{ "CURLINFO_PRIMARY_PORT",                   CURLINFO_PRIMARY_PORT },` |
|       - |  793 | `	{ "CURLOPT_FNMATCH_FUNCTION",                CURLOPT_FNMATCH_FUNCTION },` |
|       - |  794 | `	{ "CURLOPT_WILDCARDMATCH",                   CURLOPT_WILDCARDMATCH },` |
|       - |  795 | `	{ "CURLPROTO_RTMP",                          CURLPROTO_RTMP },` |
|       - |  796 | `	{ "CURLPROTO_RTMPE",                         CURLPROTO_RTMPE },` |
|       - |  797 | `	{ "CURLPROTO_RTMPS",                         CURLPROTO_RTMPS },` |
|       - |  798 | `	{ "CURLPROTO_RTMPT",                         CURLPROTO_RTMPT },` |
|       - |  799 | `	{ "CURLPROTO_RTMPTE",                        CURLPROTO_RTMPTE },` |
|       - |  800 | `	{ "CURLPROTO_RTMPTS",                        CURLPROTO_RTMPTS },` |
|       - |  801 | `	{ "CURL_FNMATCHFUNC_FAIL",                   CURL_FNMATCHFUNC_FAIL },` |
|       - |  802 | `	{ "CURL_FNMATCHFUNC_MATCH",                  CURL_FNMATCHFUNC_MATCH },` |
|       - |  803 | `	{ "CURL_FNMATCHFUNC_NOMATCH",                CURL_FNMATCHFUNC_NOMATCH },` |
|       - |  804 | `	{ "CURLPROTO_GOPHER",                        CURLPROTO_GOPHER },` |
|       - |  805 | `	{ "CURLAUTH_ONLY",                           CURLAUTH_ONLY },` |
|       - |  806 | `	{ "CURLOPT_RESOLVE",                         CURLOPT_RESOLVE },` |
|       - |  807 | `	{ "CURLOPT_TLSAUTH_PASSWORD",                CURLOPT_TLSAUTH_PASSWORD },` |
|       - |  808 | `	{ "CURLOPT_TLSAUTH_TYPE",                    CURLOPT_TLSAUTH_TYPE },` |
|       - |  809 | `	{ "CURLOPT_TLSAUTH_USERNAME",                CURLOPT_TLSAUTH_USERNAME },` |
|       - |  810 | `	{ "CURL_TLSAUTH_SRP",                        CURL_TLSAUTH_SRP },` |
|       - |  811 | `	{ "CURL_VERSION_TLSAUTH_SRP",                CURL_VERSION_TLSAUTH_SRP },` |
|       - |  812 | `	{ "CURLOPT_ACCEPT_ENCODING",                 CURLOPT_ACCEPT_ENCODING },` |
|       - |  813 | `	{ "CURLOPT_TRANSFER_ENCODING",               CURLOPT_TRANSFER_ENCODING },` |
|       - |  814 | `	{ "CURLAUTH_NTLM_WB",                        CURLAUTH_NTLM_WB },` |
|       - |  815 | `	{ "CURLGSSAPI_DELEGATION_FLAG",              CURLGSSAPI_DELEGATION_FLAG },` |
|       - |  816 | `	{ "CURLGSSAPI_DELEGATION_POLICY_FLAG",       CURLGSSAPI_DELEGATION_POLICY_FLAG },` |
|       - |  817 | `	{ "CURLOPT_GSSAPI_DELEGATION",               CURLOPT_GSSAPI_DELEGATION },` |
|       - |  818 | `	{ "CURL_VERSION_NTLM_WB",                    CURL_VERSION_NTLM_WB },` |
|       - |  819 | `	{ "CURLOPT_ACCEPTTIMEOUT_MS",                CURLOPT_ACCEPTTIMEOUT_MS },` |
|       - |  820 | `	{ "CURLOPT_DNS_SERVERS",                     CURLOPT_DNS_SERVERS },` |
|       - |  821 | `	{ "CURLOPT_MAIL_AUTH",                       CURLOPT_MAIL_AUTH },` |
|       - |  822 | `	{ "CURLOPT_SSL_OPTIONS",                     CURLOPT_SSL_OPTIONS },` |
|       - |  823 | `	{ "CURLOPT_TCP_KEEPALIVE",                   CURLOPT_TCP_KEEPALIVE },` |
|       - |  824 | `	{ "CURLOPT_TCP_KEEPIDLE",                    CURLOPT_TCP_KEEPIDLE },` |
|       - |  825 | `	{ "CURLOPT_TCP_KEEPINTVL",                   CURLOPT_TCP_KEEPINTVL },` |
|       - |  826 | `	{ "CURLSSLOPT_ALLOW_BEAST",                  CURLSSLOPT_ALLOW_BEAST },` |
|       - |  827 | `	{ "CURL_REDIR_POST_303",                     CURL_REDIR_POST_303 },` |
|       - |  828 | `	{ "CURLSSH_AUTH_AGENT",                      CURLSSH_AUTH_AGENT },` |
|       - |  829 | `	{ "CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE",      CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE },` |
|       - |  830 | `	{ "CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE",    CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE },` |
|       - |  831 | `	{ "CURLMOPT_MAX_HOST_CONNECTIONS",           CURLMOPT_MAX_HOST_CONNECTIONS },` |
|       - |  832 | `	{ "CURLMOPT_MAX_PIPELINE_LENGTH",            CURLMOPT_MAX_PIPELINE_LENGTH },` |
|       - |  833 | `	{ "CURLMOPT_MAX_TOTAL_CONNECTIONS",          CURLMOPT_MAX_TOTAL_CONNECTIONS },` |
|       - |  834 | `	{ "CURLOPT_SASL_IR",                         CURLOPT_SASL_IR },` |
|       - |  835 | `	{ "CURLOPT_DNS_INTERFACE",                   CURLOPT_DNS_INTERFACE },` |
|       - |  836 | `	{ "CURLOPT_DNS_LOCAL_IP4",                   CURLOPT_DNS_LOCAL_IP4 },` |
|       - |  837 | `	{ "CURLOPT_DNS_LOCAL_IP6",                   CURLOPT_DNS_LOCAL_IP6 },` |
|       - |  838 | `	{ "CURLOPT_XOAUTH2_BEARER",                  CURLOPT_XOAUTH2_BEARER },` |
|       - |  839 | `	{ "CURL_HTTP_VERSION_2_0",                   CURL_HTTP_VERSION_2_0 },` |
|       - |  840 | `	{ "CURL_VERSION_HTTP2",                      CURL_VERSION_HTTP2 },` |
|       - |  841 | `	{ "CURLOPT_LOGIN_OPTIONS",                   CURLOPT_LOGIN_OPTIONS },` |
|       - |  842 | `	{ "CURL_SSLVERSION_TLSv1_0",                 CURL_SSLVERSION_TLSv1_0 },` |
|       - |  843 | `	{ "CURL_SSLVERSION_TLSv1_1",                 CURL_SSLVERSION_TLSv1_1 },` |
|       - |  844 | `	{ "CURL_SSLVERSION_TLSv1_2",                 CURL_SSLVERSION_TLSv1_2 },` |
|       - |  845 | `	{ "CURLOPT_EXPECT_100_TIMEOUT_MS",           CURLOPT_EXPECT_100_TIMEOUT_MS },` |
|       - |  846 | `	{ "CURLOPT_SSL_ENABLE_ALPN",                 CURLOPT_SSL_ENABLE_ALPN },` |
|       - |  847 | `	{ "CURLOPT_SSL_ENABLE_NPN",                  CURLOPT_SSL_ENABLE_NPN },` |
|       - |  848 | `	{ "CURLHEADER_SEPARATE",                     CURLHEADER_SEPARATE },` |
|       - |  849 | `	{ "CURLHEADER_UNIFIED",                      CURLHEADER_UNIFIED },` |
|       - |  850 | `	{ "CURLOPT_HEADEROPT",                       CURLOPT_HEADEROPT },` |
|       - |  851 | `	{ "CURLOPT_PROXYHEADER",                     CURLOPT_PROXYHEADER },` |
|       - |  852 | `	{ "CURLAUTH_NEGOTIATE",                      CURLAUTH_NEGOTIATE },` |
|       - |  853 | `	{ "CURL_VERSION_GSSAPI",                     CURL_VERSION_GSSAPI },` |
|       - |  854 | `	{ "CURLOPT_PINNEDPUBLICKEY",                 CURLOPT_PINNEDPUBLICKEY },` |
|       - |  855 | `	{ "CURLOPT_UNIX_SOCKET_PATH",                CURLOPT_UNIX_SOCKET_PATH },` |
|       - |  856 | `	{ "CURLPROTO_SMB",                           CURLPROTO_SMB },` |
|       - |  857 | `	{ "CURLPROTO_SMBS",                          CURLPROTO_SMBS },` |
|       - |  858 | `	{ "CURL_VERSION_KERBEROS5",                  CURL_VERSION_KERBEROS5 },` |
|       - |  859 | `	{ "CURL_VERSION_UNIX_SOCKETS",               CURL_VERSION_UNIX_SOCKETS },` |
|       - |  860 | `	{ "CURLOPT_SSL_VERIFYSTATUS",                CURLOPT_SSL_VERIFYSTATUS },` |
|       - |  861 | `	{ "CURLOPT_PATH_AS_IS",                      CURLOPT_PATH_AS_IS },` |
|       - |  862 | `	{ "CURLOPT_SSL_FALSESTART",                  CURLOPT_SSL_FALSESTART },` |
|       - |  863 | `	{ "CURL_HTTP_VERSION_2",                     CURL_HTTP_VERSION_2 },` |
|       - |  864 | `	{ "CURLOPT_PIPEWAIT",                        CURLOPT_PIPEWAIT },` |
|       - |  865 | `	{ "CURLOPT_PROXY_SERVICE_NAME",              CURLOPT_PROXY_SERVICE_NAME },` |
|       - |  866 | `	{ "CURLOPT_SERVICE_NAME",                    CURLOPT_SERVICE_NAME },` |
|       - |  867 | `	{ "CURLPIPE_NOTHING",                        CURLPIPE_NOTHING },` |
|       - |  868 | `	{ "CURLPIPE_HTTP1",                          CURLPIPE_HTTP1 },` |
|       - |  869 | `	{ "CURLPIPE_MULTIPLEX",                      CURLPIPE_MULTIPLEX },` |
|       - |  870 | `	{ "CURLSSLOPT_NO_REVOKE",                    CURLSSLOPT_NO_REVOKE },` |
|       - |  871 | `	{ "CURLOPT_DEFAULT_PROTOCOL",                CURLOPT_DEFAULT_PROTOCOL },` |
|       - |  872 | `	{ "CURLOPT_STREAM_WEIGHT",                   CURLOPT_STREAM_WEIGHT },` |
|       - |  873 | `	{ "CURLMOPT_PUSHFUNCTION",                   CURLMOPT_PUSHFUNCTION },` |
|       - |  874 | `	{ "CURL_PUSH_OK",                            CURL_PUSH_OK },` |
|       - |  875 | `	{ "CURL_PUSH_DENY",                          CURL_PUSH_DENY },` |
|       - |  876 | `	{ "CURL_HTTP_VERSION_2TLS",                  CURL_HTTP_VERSION_2TLS },` |
|       - |  877 | `	{ "CURL_VERSION_PSL",                        CURL_VERSION_PSL },` |
|       - |  878 | `	{ "CURLOPT_TFTP_NO_OPTIONS",                 CURLOPT_TFTP_NO_OPTIONS },` |
|       - |  879 | `	{ "CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE",     CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE },` |
|       - |  880 | `	{ "CURLOPT_CONNECT_TO",                      CURLOPT_CONNECT_TO },` |
|       - |  881 | `	{ "CURLOPT_TCP_FASTOPEN",                    CURLOPT_TCP_FASTOPEN },` |
|       - |  882 | `	{ "CURLINFO_HTTP_VERSION",                   CURLINFO_HTTP_VERSION },` |
|       - |  883 | `	{ "CURLE_WEIRD_SERVER_REPLY",                CURLE_WEIRD_SERVER_REPLY },` |
|       - |  884 | `	{ "CURLOPT_KEEP_SENDING_ON_ERROR",           CURLOPT_KEEP_SENDING_ON_ERROR },` |
|       - |  885 | `	{ "CURL_SSLVERSION_TLSv1_3",                 CURL_SSLVERSION_TLSv1_3 },` |
|       - |  886 | `	{ "CURL_VERSION_HTTPS_PROXY",                CURL_VERSION_HTTPS_PROXY },` |
|       - |  887 | `	{ "CURLINFO_PROTOCOL",                       CURLINFO_PROTOCOL },` |
|       - |  888 | `	{ "CURLINFO_PROXY_SSL_VERIFYRESULT",         CURLINFO_PROXY_SSL_VERIFYRESULT },` |
|       - |  889 | `	{ "CURLINFO_SCHEME",                         CURLINFO_SCHEME },` |
|       - |  890 | `	{ "CURLOPT_PRE_PROXY",                       CURLOPT_PRE_PROXY },` |
|       - |  891 | `	{ "CURLOPT_PROXY_CAINFO",                    CURLOPT_PROXY_CAINFO },` |
|       - |  892 | `	{ "CURLOPT_PROXY_CAPATH",                    CURLOPT_PROXY_CAPATH },` |
|       - |  893 | `	{ "CURLOPT_PROXY_CRLFILE",                   CURLOPT_PROXY_CRLFILE },` |
|       - |  894 | `	{ "CURLOPT_PROXY_KEYPASSWD",                 CURLOPT_PROXY_KEYPASSWD },` |
|       - |  895 | `	{ "CURLOPT_PROXY_PINNEDPUBLICKEY",           CURLOPT_PROXY_PINNEDPUBLICKEY },` |
|       - |  896 | `	{ "CURLOPT_PROXY_SSL_CIPHER_LIST",           CURLOPT_PROXY_SSL_CIPHER_LIST },` |
|       - |  897 | `	{ "CURLOPT_PROXY_SSL_OPTIONS",               CURLOPT_PROXY_SSL_OPTIONS },` |
|       - |  898 | `	{ "CURLOPT_PROXY_SSL_VERIFYHOST",            CURLOPT_PROXY_SSL_VERIFYHOST },` |
|       - |  899 | `	{ "CURLOPT_PROXY_SSL_VERIFYPEER",            CURLOPT_PROXY_SSL_VERIFYPEER },` |
|       - |  900 | `	{ "CURLOPT_PROXY_SSLCERT",                   CURLOPT_PROXY_SSLCERT },` |
|       - |  901 | `	{ "CURLOPT_PROXY_SSLCERTTYPE",               CURLOPT_PROXY_SSLCERTTYPE },` |
|       - |  902 | `	{ "CURLOPT_PROXY_SSLKEY",                    CURLOPT_PROXY_SSLKEY },` |
|       - |  903 | `	{ "CURLOPT_PROXY_SSLKEYTYPE",                CURLOPT_PROXY_SSLKEYTYPE },` |
|       - |  904 | `	{ "CURLOPT_PROXY_SSLVERSION",                CURLOPT_PROXY_SSLVERSION },` |
|       - |  905 | `	{ "CURLOPT_PROXY_TLSAUTH_PASSWORD",          CURLOPT_PROXY_TLSAUTH_PASSWORD },` |
|       - |  906 | `	{ "CURLOPT_PROXY_TLSAUTH_TYPE",              CURLOPT_PROXY_TLSAUTH_TYPE },` |
|       - |  907 | `	{ "CURLOPT_PROXY_TLSAUTH_USERNAME",          CURLOPT_PROXY_TLSAUTH_USERNAME },` |
|       - |  908 | `	{ "CURLPROXY_HTTPS",                         CURLPROXY_HTTPS },` |
|       - |  909 | `	{ "CURL_MAX_READ_SIZE",                      CURL_MAX_READ_SIZE },` |
|       - |  910 | `	{ "CURLOPT_ABSTRACT_UNIX_SOCKET",            CURLOPT_ABSTRACT_UNIX_SOCKET },` |
|       - |  911 | `	{ "CURL_SSLVERSION_MAX_DEFAULT",             CURL_SSLVERSION_MAX_DEFAULT },` |
|       - |  912 | `	{ "CURL_SSLVERSION_MAX_NONE",                CURL_SSLVERSION_MAX_NONE },` |
|       - |  913 | `	{ "CURL_SSLVERSION_MAX_TLSv1_0",             CURL_SSLVERSION_MAX_TLSv1_0 },` |
|       - |  914 | `	{ "CURL_SSLVERSION_MAX_TLSv1_1",             CURL_SSLVERSION_MAX_TLSv1_1 },` |
|       - |  915 | `	{ "CURL_SSLVERSION_MAX_TLSv1_2",             CURL_SSLVERSION_MAX_TLSv1_2 },` |
|       - |  916 | `	{ "CURL_SSLVERSION_MAX_TLSv1_3",             CURL_SSLVERSION_MAX_TLSv1_3 },` |
|       - |  917 | `	{ "CURLOPT_SUPPRESS_CONNECT_HEADERS",        CURLOPT_SUPPRESS_CONNECT_HEADERS },` |
|       - |  918 | `	{ "CURLAUTH_GSSAPI",                         CURLAUTH_GSSAPI },` |
|       - |  919 | `	{ "CURLINFO_CONTENT_LENGTH_DOWNLOAD_T",      CURLINFO_CONTENT_LENGTH_DOWNLOAD_T },` |
|       - |  920 | `	{ "CURLINFO_CONTENT_LENGTH_UPLOAD_T",        CURLINFO_CONTENT_LENGTH_UPLOAD_T },` |
|       - |  921 | `	{ "CURLINFO_SIZE_DOWNLOAD_T",                CURLINFO_SIZE_DOWNLOAD_T },` |
|       - |  922 | `	{ "CURLINFO_SIZE_UPLOAD_T",                  CURLINFO_SIZE_UPLOAD_T },` |
|       - |  923 | `	{ "CURLINFO_SPEED_DOWNLOAD_T",               CURLINFO_SPEED_DOWNLOAD_T },` |
|       - |  924 | `	{ "CURLINFO_SPEED_UPLOAD_T",                 CURLINFO_SPEED_UPLOAD_T },` |
|       - |  925 | `	{ "CURLOPT_REQUEST_TARGET",                  CURLOPT_REQUEST_TARGET },` |
|       - |  926 | `	{ "CURLOPT_SOCKS5_AUTH",                     CURLOPT_SOCKS5_AUTH },` |
|       - |  927 | `	{ "CURLOPT_SSH_COMPRESSION",                 CURLOPT_SSH_COMPRESSION },` |
|       - |  928 | `	{ "CURL_VERSION_MULTI_SSL",                  CURL_VERSION_MULTI_SSL },` |
|       - |  929 | `	{ "CURL_VERSION_BROTLI",                     CURL_VERSION_BROTLI },` |
|       - |  930 | `	{ "CURL_LOCK_DATA_CONNECT",                  CURL_LOCK_DATA_CONNECT },` |
|       - |  931 | `	{ "CURLSSH_AUTH_GSSAPI",                     CURLSSH_AUTH_GSSAPI },` |
|       - |  932 | `	{ "CURLINFO_FILETIME_T",                     CURLINFO_FILETIME_T },` |
|       - |  933 | `	{ "CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS",       CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS },` |
|       - |  934 | `	{ "CURLOPT_TIMEVALUE_LARGE",                 CURLOPT_TIMEVALUE_LARGE },` |
|       - |  935 | `	{ "CURLOPT_DNS_SHUFFLE_ADDRESSES",           CURLOPT_DNS_SHUFFLE_ADDRESSES },` |
|       - |  936 | `	{ "CURLOPT_HAPROXYPROTOCOL",                 CURLOPT_HAPROXYPROTOCOL },` |
|       - |  937 | `	{ "CURL_LOCK_DATA_PSL",                      CURL_LOCK_DATA_PSL },` |
|       - |  938 | `	{ "CURLAUTH_BEARER",                         CURLAUTH_BEARER },` |
|       - |  939 | `	{ "CURLINFO_APPCONNECT_TIME_T",              CURLINFO_APPCONNECT_TIME_T },` |
|       - |  940 | `	{ "CURLINFO_CONNECT_TIME_T",                 CURLINFO_CONNECT_TIME_T },` |
|       - |  941 | `	{ "CURLINFO_NAMELOOKUP_TIME_T",              CURLINFO_NAMELOOKUP_TIME_T },` |
|       - |  942 | `	{ "CURLINFO_PRETRANSFER_TIME_T",             CURLINFO_PRETRANSFER_TIME_T },` |
|       - |  943 | `	{ "CURLINFO_REDIRECT_TIME_T",                CURLINFO_REDIRECT_TIME_T },` |
|       - |  944 | `	{ "CURLINFO_STARTTRANSFER_TIME_T",           CURLINFO_STARTTRANSFER_TIME_T },` |
|       - |  945 | `	{ "CURLINFO_TOTAL_TIME_T",                   CURLINFO_TOTAL_TIME_T },` |
|       - |  946 | `	{ "CURLINFO_CONN_ID",                        CURLINFO_CONN_ID },` |
|       - |  947 | `	{ "CURLOPT_DISALLOW_USERNAME_IN_URL",        CURLOPT_DISALLOW_USERNAME_IN_URL },` |
|       - |  948 | `	{ "CURLOPT_PROXY_TLS13_CIPHERS",             CURLOPT_PROXY_TLS13_CIPHERS },` |
|       - |  949 | `	{ "CURLOPT_TLS13_CIPHERS",                   CURLOPT_TLS13_CIPHERS },` |
|       - |  950 | `	{ "CURLOPT_DOH_URL",                         CURLOPT_DOH_URL },` |
|       - |  951 | `	{ "CURLOPT_UPKEEP_INTERVAL_MS",              CURLOPT_UPKEEP_INTERVAL_MS },` |
|       - |  952 | `	{ "CURLOPT_UPLOAD_BUFFERSIZE",               CURLOPT_UPLOAD_BUFFERSIZE },` |
|       - |  953 | `	{ "CURLOPT_HTTP09_ALLOWED",                  CURLOPT_HTTP09_ALLOWED },` |
|       - |  954 | `	{ "CURLALTSVC_H1",                           CURLALTSVC_H1 },` |
|       - |  955 | `	{ "CURLALTSVC_H2",                           CURLALTSVC_H2 },` |
|       - |  956 | `	{ "CURLALTSVC_H3",                           CURLALTSVC_H3 },` |
|       - |  957 | `	{ "CURLALTSVC_READONLYFILE",                 CURLALTSVC_READONLYFILE },` |
|       - |  958 | `	{ "CURLOPT_ALTSVC",                          CURLOPT_ALTSVC },` |
|       - |  959 | `	{ "CURLOPT_ALTSVC_CTRL",                     CURLOPT_ALTSVC_CTRL },` |
|       - |  960 | `	{ "CURL_VERSION_ALTSVC",                     CURL_VERSION_ALTSVC },` |
|       - |  961 | `	{ "CURLOPT_MAXAGE_CONN",                     CURLOPT_MAXAGE_CONN },` |
|       - |  962 | `	{ "CURLOPT_SASL_AUTHZID",                    CURLOPT_SASL_AUTHZID },` |
|       - |  963 | `	{ "CURL_VERSION_HTTP3",                      CURL_VERSION_HTTP3 },` |
|       - |  964 | `	{ "CURLINFO_RETRY_AFTER",                    CURLINFO_RETRY_AFTER },` |
|       - |  965 | `	{ "CURL_HTTP_VERSION_3",                     CURL_HTTP_VERSION_3 },` |
|       - |  966 | `	{ "CURLMOPT_MAX_CONCURRENT_STREAMS",         CURLMOPT_MAX_CONCURRENT_STREAMS },` |
|       - |  967 | `	{ "CURLSSLOPT_NO_PARTIALCHAIN",              CURLSSLOPT_NO_PARTIALCHAIN },` |
|       - |  968 | `	{ "CURLOPT_MAIL_RCPT_ALLLOWFAILS",           CURLOPT_MAIL_RCPT_ALLLOWFAILS },` |
|       - |  969 | `	{ "CURLSSLOPT_REVOKE_BEST_EFFORT",           CURLSSLOPT_REVOKE_BEST_EFFORT },` |
|       - |  970 | `	{ "CURLOPT_ISSUERCERT_BLOB",                 CURLOPT_ISSUERCERT_BLOB },` |
|       - |  971 | `	{ "CURLOPT_PROXY_ISSUERCERT",                CURLOPT_PROXY_ISSUERCERT },` |
|       - |  972 | `	{ "CURLOPT_PROXY_ISSUERCERT_BLOB",           CURLOPT_PROXY_ISSUERCERT_BLOB },` |
|       - |  973 | `	{ "CURLOPT_PROXY_SSLCERT_BLOB",              CURLOPT_PROXY_SSLCERT_BLOB },` |
|       - |  974 | `	{ "CURLOPT_PROXY_SSLKEY_BLOB",               CURLOPT_PROXY_SSLKEY_BLOB },` |
|       - |  975 | `	{ "CURLOPT_SSLCERT_BLOB",                    CURLOPT_SSLCERT_BLOB },` |
|       - |  976 | `	{ "CURLOPT_SSLKEY_BLOB",                     CURLOPT_SSLKEY_BLOB },` |
|       - |  977 | `	{ "CURLPROTO_MQTT",                          CURLPROTO_MQTT },` |
|       - |  978 | `	{ "CURLSSLOPT_NATIVE_CA",                    CURLSSLOPT_NATIVE_CA },` |
|       - |  979 | `	{ "CURL_VERSION_UNICODE",                    CURL_VERSION_UNICODE },` |
|       - |  980 | `	{ "CURL_VERSION_ZSTD",                       CURL_VERSION_ZSTD },` |
|       - |  981 | `	{ "CURLE_PROXY",                             CURLE_PROXY },` |
|       - |  982 | `	{ "CURLINFO_PROXY_ERROR",                    CURLINFO_PROXY_ERROR },` |
|       - |  983 | `	{ "CURLOPT_SSL_EC_CURVES",                   CURLOPT_SSL_EC_CURVES },` |
|       - |  984 | `	{ "CURLPX_BAD_ADDRESS_TYPE",                 CURLPX_BAD_ADDRESS_TYPE },` |
|       - |  985 | `	{ "CURLPX_BAD_VERSION",                      CURLPX_BAD_VERSION },` |
|       - |  986 | `	{ "CURLPX_CLOSED",                           CURLPX_CLOSED },` |
|       - |  987 | `	{ "CURLPX_GSSAPI",                           CURLPX_GSSAPI },` |
|       - |  988 | `	{ "CURLPX_GSSAPI_PERMSG",                    CURLPX_GSSAPI_PERMSG },` |
|       - |  989 | `	{ "CURLPX_GSSAPI_PROTECTION",                CURLPX_GSSAPI_PROTECTION },` |
|       - |  990 | `	{ "CURLPX_IDENTD",                           CURLPX_IDENTD },` |
|       - |  991 | `	{ "CURLPX_IDENTD_DIFFER",                    CURLPX_IDENTD_DIFFER },` |
|       - |  992 | `	{ "CURLPX_LONG_HOSTNAME",                    CURLPX_LONG_HOSTNAME },` |
|       - |  993 | `	{ "CURLPX_LONG_PASSWD",                      CURLPX_LONG_PASSWD },` |
|       - |  994 | `	{ "CURLPX_LONG_USER",                        CURLPX_LONG_USER },` |
|       - |  995 | `	{ "CURLPX_NO_AUTH",                          CURLPX_NO_AUTH },` |
|       - |  996 | `	{ "CURLPX_OK",                               CURLPX_OK },` |
|       - |  997 | `	{ "CURLPX_RECV_ADDRESS",                     CURLPX_RECV_ADDRESS },` |
|       - |  998 | `	{ "CURLPX_RECV_AUTH",                        CURLPX_RECV_AUTH },` |
|       - |  999 | `	{ "CURLPX_RECV_CONNECT",                     CURLPX_RECV_CONNECT },` |
|       - | 1000 | `	{ "CURLPX_RECV_REQACK",                      CURLPX_RECV_REQACK },` |
|       - | 1001 | `	{ "CURLPX_REPLY_ADDRESS_TYPE_NOT_SUPPORTED", CURLPX_REPLY_ADDRESS_TYPE_NOT_SUPPORTED },` |
|       - | 1002 | `	{ "CURLPX_REPLY_COMMAND_NOT_SUPPORTED",      CURLPX_REPLY_COMMAND_NOT_SUPPORTED },` |
|       - | 1003 | `	{ "CURLPX_REPLY_CONNECTION_REFUSED",         CURLPX_REPLY_CONNECTION_REFUSED },` |
|       - | 1004 | `	{ "CURLPX_REPLY_GENERAL_SERVER_FAILURE",     CURLPX_REPLY_GENERAL_SERVER_FAILURE },` |
|       - | 1005 | `	{ "CURLPX_REPLY_HOST_UNREACHABLE",           CURLPX_REPLY_HOST_UNREACHABLE },` |
|       - | 1006 | `	{ "CURLPX_REPLY_NETWORK_UNREACHABLE",        CURLPX_REPLY_NETWORK_UNREACHABLE },` |
|       - | 1007 | `	{ "CURLPX_REPLY_NOT_ALLOWED",                CURLPX_REPLY_NOT_ALLOWED },` |
|       - | 1008 | `	{ "CURLPX_REPLY_TTL_EXPIRED",                CURLPX_REPLY_TTL_EXPIRED },` |
|       - | 1009 | `	{ "CURLPX_REPLY_UNASSIGNED",                 CURLPX_REPLY_UNASSIGNED },` |
|       - | 1010 | `	{ "CURLPX_REQUEST_FAILED",                   CURLPX_REQUEST_FAILED },` |
|       - | 1011 | `	{ "CURLPX_RESOLVE_HOST",                     CURLPX_RESOLVE_HOST },` |
|       - | 1012 | `	{ "CURLPX_SEND_AUTH",                        CURLPX_SEND_AUTH },` |
|       - | 1013 | `	{ "CURLPX_SEND_CONNECT",                     CURLPX_SEND_CONNECT },` |
|       - | 1014 | `	{ "CURLPX_SEND_REQUEST",                     CURLPX_SEND_REQUEST },` |
|       - | 1015 | `	{ "CURLPX_UNKNOWN_FAIL",                     CURLPX_UNKNOWN_FAIL },` |
|       - | 1016 | `	{ "CURLPX_UNKNOWN_MODE",                     CURLPX_UNKNOWN_MODE },` |
|       - | 1017 | `	{ "CURLPX_USER_REJECTED",                    CURLPX_USER_REJECTED },` |
|       - | 1018 | `	{ "CURLHSTS_ENABLE",                         CURLHSTS_ENABLE },` |
|       - | 1019 | `	{ "CURLHSTS_READONLYFILE",                   CURLHSTS_READONLYFILE },` |
|       - | 1020 | `	{ "CURLOPT_HSTS",                            CURLOPT_HSTS },` |
|       - | 1021 | `	{ "CURLOPT_HSTS_CTRL",                       CURLOPT_HSTS_CTRL },` |
|       - | 1022 | `	{ "CURL_VERSION_HSTS",                       CURL_VERSION_HSTS },` |
|       - | 1023 | `	{ "CURLAUTH_AWS_SIGV4",                      CURLAUTH_AWS_SIGV4 },` |
|       - | 1024 | `	{ "CURLOPT_AWS_SIGV4",                       CURLOPT_AWS_SIGV4 },` |
|       - | 1025 | `	{ "CURLINFO_REFERER",                        CURLINFO_REFERER },` |
|       - | 1026 | `	{ "CURLOPT_DOH_SSL_VERIFYHOST",              CURLOPT_DOH_SSL_VERIFYHOST },` |
|       - | 1027 | `	{ "CURLOPT_DOH_SSL_VERIFYPEER",              CURLOPT_DOH_SSL_VERIFYPEER },` |
|       - | 1028 | `	{ "CURLOPT_DOH_SSL_VERIFYSTATUS",            CURLOPT_DOH_SSL_VERIFYSTATUS },` |
|       - | 1029 | `	{ "CURL_VERSION_GSASL",                      CURL_VERSION_GSASL },` |
|       - | 1030 | `	{ "CURLOPT_CAINFO_BLOB",                     CURLOPT_CAINFO_BLOB },` |
|       - | 1031 | `	{ "CURLOPT_PROXY_CAINFO_BLOB",               CURLOPT_PROXY_CAINFO_BLOB },` |
|       - | 1032 | `	{ "CURLSSLOPT_AUTO_CLIENT_CERT",             CURLSSLOPT_AUTO_CLIENT_CERT },` |
|       - | 1033 | `	{ "CURLOPT_MAXLIFETIME_CONN",                CURLOPT_MAXLIFETIME_CONN },` |
|       - | 1034 | `	{ "CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256",      CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256 },` |
|       - | 1035 | `	{ "CURLOPT_PREREQFUNCTION",                  CURLOPT_PREREQFUNCTION },` |
|       - | 1036 | `	{ "CURL_PREREQFUNC_OK",                      CURL_PREREQFUNC_OK },` |
|       - | 1037 | `	{ "CURL_PREREQFUNC_ABORT",                   CURL_PREREQFUNC_ABORT },` |
|       - | 1038 | `	{ "CURLOPT_MIME_OPTIONS",                    CURLOPT_MIME_OPTIONS },` |
|       - | 1039 | `	{ "CURLMIMEOPT_FORMESCAPE",                  CURLMIMEOPT_FORMESCAPE },` |
|       - | 1040 | `	{ "CURLOPT_SSH_HOSTKEYFUNCTION",             CURLOPT_SSH_HOSTKEYFUNCTION },` |
|       - | 1041 | `	{ "CURLOPT_PROTOCOLS_STR",                   CURLOPT_PROTOCOLS_STR },` |
|       - | 1042 | `	{ "CURLOPT_REDIR_PROTOCOLS_STR",             CURLOPT_REDIR_PROTOCOLS_STR },` |
|       - | 1043 | `	{ "CURLOPT_WS_OPTIONS",                      CURLOPT_WS_OPTIONS },` |
|       - | 1044 | `	{ "CURLWS_RAW_MODE",                         CURLWS_RAW_MODE },` |
|       - | 1045 | `	{ "CURLOPT_CA_CACHE_TIMEOUT",                CURLOPT_CA_CACHE_TIMEOUT },` |
|       - | 1046 | `	{ "CURLOPT_QUICK_EXIT",                      CURLOPT_QUICK_EXIT },` |
|       - | 1047 | `	{ "CURL_HTTP_VERSION_3ONLY",                 CURL_HTTP_VERSION_3ONLY },` |
|       - | 1048 | `	{ "CURLOPT_SAFE_UPLOAD",                     -1 },` |
|       - | 1049 | `};` |
|       - | 1050 |  |
|   43674 | 1051 | `static void CurlConstExpand(ph7_value *pVal,void *pUserData)` |
|       3 | 1052 | `{` |
|   43677 | 1053 | `	ph7_value_int64(pVal,((const struct CurlConstant *)pUserData)->iValue);` |
|   43677 | 1054 | `}` |
|       - | 1055 |  |
|    4660 | 1056 | `PH7_PRIVATE void PH7_RegisterCurlConstants(ph7_vm *pVm)` |
|       5 | 1057 | `{` |
|       - | 1058 | `	sxu32 n;` |
| 3168805 | 1059 | `	for( n = 0 ; n < SX_ARRAYSIZE(aCurlConst) ; ++n ){` |
| 4746215 | 1060 | `		ph7_create_constant(&(*pVm),aCurlConst[n].zName,CurlConstExpand,` |
| 3164140 | 1061 | `			(void *)&aCurlConst[n]);` |
| 1582075 | 1062 | `	}` |
|    4665 | 1063 | `}` |
|       - | 1064 |  |
|       - | 1065 | `/* ===== curl_version() ===== */` |
|       - | 1066 |  |
|       - | 1067 | `/*` |
|       - | 1068 | ` * php's feature-name table: the bit each name reports, in php's own order.` |
|       - | 1069 | ` * The names are php's spelling, not libcurl's constant tails ("GSS-Negotiate",` |
|       - | 1070 | ` * "krb4", "TLS-SRP", "NTLMWB", "CharConv"), and all 29 were verified against` |
|       - | 1071 | ` * both the oracle's feature_list and curl_version_info()'s features word.` |
|       - | 1072 | ` */` |
|       - | 1073 | `static const struct CurlFeatureName {` |
|       - | 1074 | `	const char *zName;` |
|       - | 1075 | `	unsigned int iBit;` |
|       - | 1076 | `} aCurlFeature[] = {` |
|       - | 1077 | `	{ "AsynchDNS",     CURL_VERSION_ASYNCHDNS     },` |
|       - | 1078 | `	{ "CharConv",      CURL_VERSION_CONV          },` |
|       - | 1079 | `	{ "Debug",         CURL_VERSION_DEBUG         },` |
|       - | 1080 | `	{ "GSS-Negotiate", CURL_VERSION_GSSNEGOTIATE  },` |
|       - | 1081 | `	{ "IDN",           CURL_VERSION_IDN           },` |
|       - | 1082 | `	{ "IPv6",          CURL_VERSION_IPV6          },` |
|       - | 1083 | `	{ "krb4",          CURL_VERSION_KERBEROS4     },` |
|       - | 1084 | `	{ "Largefile",     CURL_VERSION_LARGEFILE     },` |
|       - | 1085 | `	{ "libz",          CURL_VERSION_LIBZ          },` |
|       - | 1086 | `	{ "NTLM",          CURL_VERSION_NTLM          },` |
|       - | 1087 | `	{ "NTLMWB",        CURL_VERSION_NTLM_WB       },` |
|       - | 1088 | `	{ "SPNEGO",        CURL_VERSION_SPNEGO        },` |
|       - | 1089 | `	{ "SSL",           CURL_VERSION_SSL           },` |
|       - | 1090 | `	{ "SSPI",          CURL_VERSION_SSPI          },` |
|       - | 1091 | `	{ "TLS-SRP",       CURL_VERSION_TLSAUTH_SRP   },` |
|       - | 1092 | `	{ "HTTP2",         CURL_VERSION_HTTP2         },` |
|       - | 1093 | `	{ "GSSAPI",        CURL_VERSION_GSSAPI        },` |
|       - | 1094 | `	{ "KERBEROS5",     CURL_VERSION_KERBEROS5     },` |
|       - | 1095 | `	{ "UNIX_SOCKETS",  CURL_VERSION_UNIX_SOCKETS  },` |
|       - | 1096 | `	{ "PSL",           CURL_VERSION_PSL           },` |
|       - | 1097 | `	{ "HTTPS_PROXY",   CURL_VERSION_HTTPS_PROXY   },` |
|       - | 1098 | `	{ "MULTI_SSL",     CURL_VERSION_MULTI_SSL     },` |
|       - | 1099 | `	{ "BROTLI",        CURL_VERSION_BROTLI        },` |
|       - | 1100 | `	{ "ALTSVC",        CURL_VERSION_ALTSVC        },` |
|       - | 1101 | `	{ "HTTP3",         CURL_VERSION_HTTP3         },` |
|       - | 1102 | `	{ "UNICODE",       CURL_VERSION_UNICODE       },` |
|       - | 1103 | `	{ "ZSTD",          CURL_VERSION_ZSTD          },` |
|       - | 1104 | `	{ "HSTS",          CURL_VERSION_HSTS          },` |
|       - | 1105 | `	{ "GSASL",         CURL_VERSION_GSASL         }` |
|       - | 1106 | `};` |
|       - | 1107 |  |
|       - | 1108 | `/* An int entry on the answer. */` |
|      14 | 1109 | `static void CurlVersionAddInt(ph7_value *pArray,ph7_value *pWorker,` |
|       - | 1110 | `	const char *zKey,sxi64 iVal)` |
|       1 | 1111 | `{` |
|      15 | 1112 | `	ph7_value_int64(pWorker,iVal);` |
|      15 | 1113 | `	ph7_array_add_strkey_elem(pArray,zKey,pWorker);` |
|      15 | 1114 | `}` |
|       - | 1115 |  |
|       - | 1116 | `/*` |
|       - | 1117 | ` * A string entry on the answer. php prints the EMPTY STRING for a field` |
|       - | 1118 | `` * libcurl left NULL -- `ares` is NULL in every build without c-ares and the`` |
|       - | 1119 | ` * oracle still answers string(0) "" for it, never null.` |
|       - | 1120 | ` */` |
|      16 | 1121 | `static void CurlVersionAddStr(ph7_value *pArray,ph7_value *pWorker,` |
|       - | 1122 | `	const char *zKey,const char *zVal)` |
|       1 | 1123 | `{` |
|      17 | 1124 | `	ph7_value_reset_string_cursor(pWorker);` |
|      17 | 1125 | `	ph7_value_string(pWorker,zVal ? zVal : "",-1);` |
|      17 | 1126 | `	ph7_array_add_strkey_elem(pArray,zKey,pWorker);` |
|      17 | 1127 | `	ph7_value_reset_string_cursor(pWorker);` |
|      17 | 1128 | `}` |
|       - | 1129 |  |
|       - | 1130 | `/* array\|false curl_version() */` |
|       2 | 1131 | `static int vm_builtin_curl_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1132 | `{` |
|       - | 1133 | `	curl_version_info_data *pInfo;` |
|       - | 1134 | `	ph7_value *pArray,*pWorker,*pFeature,*pProto;` |
|       - | 1135 | `	const char * const *pzProto;` |
|       - | 1136 | `	sxu32 n;` |
|       1 | 1137 | `	SXUNUSED(nArg);` |
|       1 | 1138 | `	SXUNUSED(apArg);` |
|       3 | 1139 | `	PH7_CurlGlobalInit();` |
|       3 | 1140 | `	pInfo = curl_version_info(CURLVERSION_NOW);` |
|       3 | 1141 | `	if( pInfo == 0 ){` |
|       - | 1142 | `		/* php's own guard: the library answered nothing, so neither do we. */` |
|     ! 0 | 1143 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1144 | `		return PH7_OK;` |
|       - | 1145 | `	}` |
|       3 | 1146 | `	pArray   = ph7_context_new_array(pCtx);` |
|       3 | 1147 | `	pWorker  = ph7_context_new_scalar(pCtx);` |
|       3 | 1148 | `	pFeature = ph7_context_new_array(pCtx);` |
|       3 | 1149 | `	pProto   = ph7_context_new_array(pCtx);` |
|       3 | 1150 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pFeature == 0 \|\| pProto == 0 ){` |
|     ! 0 | 1151 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 | 1152 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1153 | `		return PH7_OK;` |
|       - | 1154 | `	}` |
|       - | 1155 | `	/* The key ORDER is php's and is user-visible through print_r/var_dump and` |
|       - | 1156 | `	 * a foreach; it is not libcurl's struct order. */` |
|       3 | 1157 | `	CurlVersionAddInt(pArray,pWorker,"version_number",(sxi64)pInfo->version_num);` |
|       3 | 1158 | `	CurlVersionAddInt(pArray,pWorker,"age",(sxi64)pInfo->age);` |
|       3 | 1159 | `	CurlVersionAddInt(pArray,pWorker,"features",(sxi64)pInfo->features);` |
|      61 | 1160 | `	for( n = 0 ; n < SX_ARRAYSIZE(aCurlFeature) ; ++n ){` |
|      59 | 1161 | `		ph7_value_bool(pWorker,(pInfo->features & aCurlFeature[n].iBit) ? 1 : 0);` |
|      59 | 1162 | `		ph7_array_add_strkey_elem(pFeature,aCurlFeature[n].zName,pWorker);` |
|      30 | 1163 | `	}` |
|       3 | 1164 | `	ph7_array_add_strkey_elem(pArray,"feature_list",pFeature);` |
|       3 | 1165 | `	CurlVersionAddInt(pArray,pWorker,"ssl_version_number",(sxi64)pInfo->ssl_version_num);` |
|       3 | 1166 | `	CurlVersionAddStr(pArray,pWorker,"version",pInfo->version);` |
|       3 | 1167 | `	CurlVersionAddStr(pArray,pWorker,"host",pInfo->host);` |
|       3 | 1168 | `	CurlVersionAddStr(pArray,pWorker,"ssl_version",pInfo->ssl_version);` |
|       3 | 1169 | `	CurlVersionAddStr(pArray,pWorker,"libz_version",pInfo->libz_version);` |
|      58 | 1170 | `	for( pzProto = pInfo->protocols ; pzProto && *pzProto ; ++pzProto ){` |
|      56 | 1171 | `		ph7_value_reset_string_cursor(pWorker);` |
|      56 | 1172 | `		ph7_value_string(pWorker,*pzProto,-1);` |
|      56 | 1173 | `		ph7_array_add_elem(pProto,0,pWorker);` |
|      26 | 1174 | `	}` |
|       3 | 1175 | `	ph7_value_reset_string_cursor(pWorker);` |
|       3 | 1176 | `	ph7_array_add_strkey_elem(pArray,"protocols",pProto);` |
|       - | 1177 | `	/*` |
|       - | 1178 | `	 * Everything below is gated on the struct's OWN age, which is what php` |
|       - | 1179 | `	 * gates on -- and php stops at the brotli block even against a library` |
|       - | 1180 | `	 * reporting age 10, so zstd_version and the fields after it are absent` |
|       - | 1181 | `	 * from the answer even where libcurl reports them. Reproducing php means` |
|       - | 1182 | `	 * stopping here too, not exposing what the library happens to know.` |
|       - | 1183 | `	 */` |
|       3 | 1184 | `	if( pInfo->age >= CURLVERSION_SECOND ){` |
|       3 | 1185 | `		CurlVersionAddStr(pArray,pWorker,"ares",pInfo->ares);` |
|       3 | 1186 | `		CurlVersionAddInt(pArray,pWorker,"ares_num",(sxi64)pInfo->ares_num);` |
|       1 | 1187 | `	}` |
|       3 | 1188 | `	if( pInfo->age >= CURLVERSION_THIRD ){` |
|       3 | 1189 | `		CurlVersionAddStr(pArray,pWorker,"libidn",pInfo->libidn);` |
|       1 | 1190 | `	}` |
|       3 | 1191 | `	if( pInfo->age >= CURLVERSION_FOURTH ){` |
|       3 | 1192 | `		CurlVersionAddInt(pArray,pWorker,"iconv_ver_num",(sxi64)pInfo->iconv_ver_num);` |
|       3 | 1193 | `		CurlVersionAddStr(pArray,pWorker,"libssh_version",pInfo->libssh_version);` |
|       1 | 1194 | `	}` |
|       3 | 1195 | `	if( pInfo->age >= CURLVERSION_FIFTH ){` |
|       3 | 1196 | `		CurlVersionAddInt(pArray,pWorker,"brotli_ver_num",(sxi64)pInfo->brotli_ver_num);` |
|       3 | 1197 | `		CurlVersionAddStr(pArray,pWorker,"brotli_version",pInfo->brotli_version);` |
|       1 | 1198 | `	}` |
|       3 | 1199 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 1200 | `	return PH7_OK;` |
|       2 | 1201 | `}` |
|       - | 1202 |  |
|       - | 1203 | `/* ===== The three strerror families ===== */` |
|       - | 1204 |  |
|       - | 1205 | `/*` |
|       - | 1206 | ` * All three are pass-throughs, and the pass-through is the point: the codes` |
|       - | 1207 | ` * are C enums, so php narrows its zend_long argument to the enum's width` |
|       - | 1208 | ` * before the library ever sees it. That truncation is user-visible --` |
|       - | 1209 | ` * curl_multi_strerror(PHP_INT_MAX) answers "Please call curl_multi_perform()` |
|       - | 1210 | ` * soon" because the value arrives as -1 (CURLM_CALL_MULTI_PERFORM), not` |
|       - | 1211 | ` * because PHP_INT_MAX means anything. Casting to int here reproduces it; a` |
|       - | 1212 | ` * range check would not.` |
|       - | 1213 | ` *` |
|       - | 1214 | ` * The declared return type is ?string for the same reason php's is: the` |
|       - | 1215 | ` * answer is whatever the library's pointer says, including NULL.` |
|       - | 1216 | ` */` |
|      18 | 1217 | `static int CurlStrError(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 1218 | `	const char *(*xStr)(int))` |
|       1 | 1219 | `{` |
|      19 | 1220 | `	int iCode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : 0;` |
|      19 | 1221 | `	const char *zMsg = xStr(iCode);` |
|      19 | 1222 | `	if( zMsg == 0 ){` |
|     ! 0 | 1223 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1224 | `	}else{` |
|      19 | 1225 | `		ph7_result_string(pCtx,zMsg,-1);` |
|       - | 1226 | `	}` |
|      19 | 1227 | `	return PH7_OK;` |
|       1 | 1228 | `}` |
|      10 | 1229 | `static const char * CurlEasyStrErrorTrampoline(int iCode)` |
|       1 | 1230 | `{` |
|      11 | 1231 | `	return curl_easy_strerror((CURLcode)iCode);` |
|       1 | 1232 | `}` |
|       6 | 1233 | `static const char * CurlMultiStrErrorTrampoline(int iCode)` |
|       1 | 1234 | `{` |
|       7 | 1235 | `	return curl_multi_strerror((CURLMcode)iCode);` |
|       1 | 1236 | `}` |
|       2 | 1237 | `static const char * CurlShareStrErrorTrampoline(int iCode)` |
|       1 | 1238 | `{` |
|       3 | 1239 | `	return curl_share_strerror((CURLSHcode)iCode);` |
|       1 | 1240 | `}` |
|       - | 1241 | `/* ?string curl_strerror(int $error_code) */` |
|      10 | 1242 | `static int vm_builtin_curl_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1243 | `{` |
|      11 | 1244 | `	return CurlStrError(pCtx,nArg,apArg,CurlEasyStrErrorTrampoline);` |
|       1 | 1245 | `}` |
|       - | 1246 | `/* ?string curl_multi_strerror(int $error_code) */` |
|       6 | 1247 | `static int vm_builtin_curl_multi_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1248 | `{` |
|       7 | 1249 | `	return CurlStrError(pCtx,nArg,apArg,CurlMultiStrErrorTrampoline);` |
|       1 | 1250 | `}` |
|       - | 1251 | `/* ?string curl_share_strerror(int $error_code) */` |
|       2 | 1252 | `static int vm_builtin_curl_share_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1253 | `{` |
|       3 | 1254 | `	return CurlStrError(pCtx,nArg,apArg,CurlShareStrErrorTrampoline);` |
|       1 | 1255 | `}` |
|       - | 1256 |  |
|       - | 1257 | `/* ===== curl_setopt() ===== */` |
|       - | 1258 |  |
|       - | 1259 | `/*` |
|       - | 1260 | ` * What php DOES with the value it is handed, per option.` |
|       - | 1261 | ` *` |
|       - | 1262 | ` * The kinds were derived by sweeping all 269 options against 17 value types` |
|       - | 1263 | ` * and reading what php answered; they are not libcurl's own typing, though` |
|       - | 1264 | ` * they mostly follow from it. libcurl encodes a type in the option NUMBER` |
|       - | 1265 | ` * (below 10000 long, 10000-19999 pointer, 20000+ function, 30000+ off_t,` |
|       - | 1266 | ` * 40000+ blob) and php's switch agrees with that bucket for 247 of the 269 --` |
|       - | 1267 | ` * the exceptions are the whole reason this is a table rather than arithmetic:` |
|       - | 1268 | ` * ten pointer options take an ARRAY php turns into a curl_slist, five take a` |
|       - | 1269 | ` * php STREAM, and six are php's own (RETURNTRANSFER and BINARYTRANSFER exist` |
|       - | 1270 | ` * in no libcurl, PRIVATE stores a php value, SHARE takes a share handle,` |
|       - | 1271 | ` * SAFE_UPLOAD refuses to be turned off, DNS_USE_GLOBAL_CACHE is accepted and` |
|       - | 1272 | ` * ignored).` |
|       - | 1273 | ` *` |
|       - | 1274 | ` * The table is also the VALIDATOR. php does not ask libcurl whether an option` |
|       - | 1275 | ` * exists: an unknown number never reaches the library, it falls off the end of` |
|       - | 1276 | ` * php's switch. So a number a newer libcurl knows and this table does not is a` |
|       - | 1277 | ` * ValueError here exactly as it is in php built against the older library.` |
|       - | 1278 | ` */` |
|       - | 1279 | `#define CURL_OPT_LONG       0   /* zval -> long, straight to libcurl */` |
|       - | 1280 | `#define CURL_OPT_STRING     1   /* zval -> string, NUL-screened */` |
|       - | 1281 | `#define CURL_OPT_SLIST      2   /* array -> curl_slist owned by the handle */` |
|       - | 1282 | `#define CURL_OPT_CALLBACK   3   /* a php callable (its own slice) */` |
|       - | 1283 | `#define CURL_OPT_FILE       4   /* a php stream (its own slice) */` |
|       - | 1284 | `#define CURL_OPT_SAFEUP     5   /* php's own: truthy only */` |
|       - | 1285 | `#define CURL_OPT_RETURN     6   /* php's own: exec answers the body */` |
|       - | 1286 | `#define CURL_OPT_PRIVATE    7   /* php's own: stores the value itself */` |
|       - | 1287 | `#define CURL_OPT_SHARE      8   /* php's own: a CurlShareHandle */` |
|       - | 1288 | `#define CURL_OPT_POSTFIELDS 9   /* string or array (the upload slice) */` |
|       - | 1289 | `#define CURL_OPT_IGNORE    10   /* accepted and read by nothing, like php */` |
|       - | 1290 |  |
|       - | 1291 | `static const struct CurlOptDef {` |
|       - | 1292 | `	sxi64 iOpt;` |
|       - | 1293 | `	int iKind;` |
|       - | 1294 | `} aCurlOpt[] = {` |
|       - | 1295 | `	{ -1,                                CURL_OPT_SAFEUP     },  /* -1 */` |
|       - | 1296 | `	{ CURLOPT_PORT,                      CURL_OPT_LONG       },  /* 3 */` |
|       - | 1297 | `	{ CURLOPT_TIMEOUT,                   CURL_OPT_LONG       },  /* 13 */` |
|       - | 1298 | `	{ CURLOPT_INFILESIZE,                CURL_OPT_LONG       },  /* 14 */` |
|       - | 1299 | `	{ CURLOPT_LOW_SPEED_LIMIT,           CURL_OPT_LONG       },  /* 19 */` |
|       - | 1300 | `	{ CURLOPT_LOW_SPEED_TIME,            CURL_OPT_LONG       },  /* 20 */` |
|       - | 1301 | `	{ CURLOPT_RESUME_FROM,               CURL_OPT_LONG       },  /* 21 */` |
|       - | 1302 | `	{ CURLOPT_CRLF,                      CURL_OPT_LONG       },  /* 27 */` |
|       - | 1303 | `	{ CURLOPT_SSLVERSION,                CURL_OPT_LONG       },  /* 32 */` |
|       - | 1304 | `	{ CURLOPT_TIMECONDITION,             CURL_OPT_LONG       },  /* 33 */` |
|       - | 1305 | `	{ CURLOPT_TIMEVALUE,                 CURL_OPT_LONG       },  /* 34 */` |
|       - | 1306 | `	{ CURLOPT_VERBOSE,                   CURL_OPT_LONG       },  /* 41 */` |
|       - | 1307 | `	{ CURLOPT_HEADER,                    CURL_OPT_LONG       },  /* 42 */` |
|       - | 1308 | `	{ CURLOPT_NOPROGRESS,                CURL_OPT_LONG       },  /* 43 */` |
|       - | 1309 | `	{ CURLOPT_NOBODY,                    CURL_OPT_LONG       },  /* 44 */` |
|       - | 1310 | `	{ CURLOPT_FAILONERROR,               CURL_OPT_LONG       },  /* 45 */` |
|       - | 1311 | `	{ CURLOPT_UPLOAD,                    CURL_OPT_LONG       },  /* 46 */` |
|       - | 1312 | `	{ CURLOPT_POST,                      CURL_OPT_LONG       },  /* 47 */` |
|       - | 1313 | `	{ CURLOPT_DIRLISTONLY,               CURL_OPT_LONG       },  /* 48 */` |
|       - | 1314 | `	{ CURLOPT_FTPLISTONLY,               CURL_OPT_LONG       },  /* 48 */` |
|       - | 1315 | `	{ CURLOPT_APPEND,                    CURL_OPT_LONG       },  /* 50 */` |
|       - | 1316 | `	{ CURLOPT_FTPAPPEND,                 CURL_OPT_LONG       },  /* 50 */` |
|       - | 1317 | `	{ CURLOPT_NETRC,                     CURL_OPT_LONG       },  /* 51 */` |
|       - | 1318 | `	{ CURLOPT_FOLLOWLOCATION,            CURL_OPT_LONG       },  /* 52 */` |
|       - | 1319 | `	{ CURLOPT_TRANSFERTEXT,              CURL_OPT_LONG       },  /* 53 */` |
|       - | 1320 | `	{ CURLOPT_PUT,                       CURL_OPT_LONG       },  /* 54 */` |
|       - | 1321 | `	{ CURLOPT_AUTOREFERER,               CURL_OPT_LONG       },  /* 58 */` |
|       - | 1322 | `	{ CURLOPT_PROXYPORT,                 CURL_OPT_LONG       },  /* 59 */` |
|       - | 1323 | `	{ CURLOPT_HTTPPROXYTUNNEL,           CURL_OPT_LONG       },  /* 61 */` |
|       - | 1324 | `	{ CURLOPT_SSL_VERIFYPEER,            CURL_OPT_LONG       },  /* 64 */` |
|       - | 1325 | `	{ CURLOPT_MAXREDIRS,                 CURL_OPT_LONG       },  /* 68 */` |
|       - | 1326 | `	{ CURLOPT_FILETIME,                  CURL_OPT_LONG       },  /* 69 */` |
|       - | 1327 | `	{ CURLOPT_MAXCONNECTS,               CURL_OPT_LONG       },  /* 71 */` |
|       - | 1328 | `	{ CURLOPT_FRESH_CONNECT,             CURL_OPT_LONG       },  /* 74 */` |
|       - | 1329 | `	{ CURLOPT_FORBID_REUSE,              CURL_OPT_LONG       },  /* 75 */` |
|       - | 1330 | `	{ CURLOPT_CONNECTTIMEOUT,            CURL_OPT_LONG       },  /* 78 */` |
|       - | 1331 | `	{ CURLOPT_HTTPGET,                   CURL_OPT_LONG       },  /* 80 */` |
|       - | 1332 | `	{ CURLOPT_SSL_VERIFYHOST,            CURL_OPT_LONG       },  /* 81 */` |
|       - | 1333 | `	{ CURLOPT_HTTP_VERSION,              CURL_OPT_LONG       },  /* 84 */` |
|       - | 1334 | `	{ CURLOPT_FTP_USE_EPSV,              CURL_OPT_LONG       },  /* 85 */` |
|       - | 1335 | `	{ CURLOPT_SSLENGINE_DEFAULT,         CURL_OPT_LONG       },  /* 90 */` |
|       - | 1336 | `	{ CURLOPT_DNS_USE_GLOBAL_CACHE,      CURL_OPT_IGNORE     },  /* 91 */` |
|       - | 1337 | `	{ CURLOPT_DNS_CACHE_TIMEOUT,         CURL_OPT_LONG       },  /* 92 */` |
|       - | 1338 | `	{ CURLOPT_COOKIESESSION,             CURL_OPT_LONG       },  /* 96 */` |
|       - | 1339 | `	{ CURLOPT_BUFFERSIZE,                CURL_OPT_LONG       },  /* 98 */` |
|       - | 1340 | `	{ CURLOPT_NOSIGNAL,                  CURL_OPT_LONG       },  /* 99 */` |
|       - | 1341 | `	{ CURLOPT_PROXYTYPE,                 CURL_OPT_LONG       },  /* 101 */` |
|       - | 1342 | `	{ CURLOPT_UNRESTRICTED_AUTH,         CURL_OPT_LONG       },  /* 105 */` |
|       - | 1343 | `	{ CURLOPT_FTP_USE_EPRT,              CURL_OPT_LONG       },  /* 106 */` |
|       - | 1344 | `	{ CURLOPT_HTTPAUTH,                  CURL_OPT_LONG       },  /* 107 */` |
|       - | 1345 | `	{ CURLOPT_FTP_CREATE_MISSING_DIRS,   CURL_OPT_LONG       },  /* 110 */` |
|       - | 1346 | `	{ CURLOPT_PROXYAUTH,                 CURL_OPT_LONG       },  /* 111 */` |
|       - | 1347 | `	{ CURLOPT_FTP_RESPONSE_TIMEOUT,      CURL_OPT_LONG       },  /* 112 */` |
|       - | 1348 | `	{ CURLOPT_SERVER_RESPONSE_TIMEOUT,   CURL_OPT_LONG       },  /* 112 */` |
|       - | 1349 | `	{ CURLOPT_IPRESOLVE,                 CURL_OPT_LONG       },  /* 113 */` |
|       - | 1350 | `	{ CURLOPT_MAXFILESIZE,               CURL_OPT_LONG       },  /* 114 */` |
|       - | 1351 | `	{ CURLOPT_FTP_SSL,                   CURL_OPT_LONG       },  /* 119 */` |
|       - | 1352 | `	{ CURLOPT_USE_SSL,                   CURL_OPT_LONG       },  /* 119 */` |
|       - | 1353 | `	{ CURLOPT_TCP_NODELAY,               CURL_OPT_LONG       },  /* 121 */` |
|       - | 1354 | `	{ CURLOPT_FTPSSLAUTH,                CURL_OPT_LONG       },  /* 129 */` |
|       - | 1355 | `	{ CURLOPT_IGNORE_CONTENT_LENGTH,     CURL_OPT_LONG       },  /* 136 */` |
|       - | 1356 | `	{ CURLOPT_FTP_SKIP_PASV_IP,          CURL_OPT_LONG       },  /* 137 */` |
|       - | 1357 | `	{ CURLOPT_FTP_FILEMETHOD,            CURL_OPT_LONG       },  /* 138 */` |
|       - | 1358 | `	{ CURLOPT_LOCALPORT,                 CURL_OPT_LONG       },  /* 139 */` |
|       - | 1359 | `	{ CURLOPT_LOCALPORTRANGE,            CURL_OPT_LONG       },  /* 140 */` |
|       - | 1360 | `	{ CURLOPT_CONNECT_ONLY,              CURL_OPT_LONG       },  /* 141 */` |
|       - | 1361 | `	{ CURLOPT_SSL_SESSIONID_CACHE,       CURL_OPT_LONG       },  /* 150 */` |
|       - | 1362 | `	{ CURLOPT_SSH_AUTH_TYPES,            CURL_OPT_LONG       },  /* 151 */` |
|       - | 1363 | `	{ CURLOPT_FTP_SSL_CCC,               CURL_OPT_LONG       },  /* 154 */` |
|       - | 1364 | `	{ CURLOPT_TIMEOUT_MS,                CURL_OPT_LONG       },  /* 155 */` |
|       - | 1365 | `	{ CURLOPT_CONNECTTIMEOUT_MS,         CURL_OPT_LONG       },  /* 156 */` |
|       - | 1366 | `	{ CURLOPT_HTTP_TRANSFER_DECODING,    CURL_OPT_LONG       },  /* 157 */` |
|       - | 1367 | `	{ CURLOPT_HTTP_CONTENT_DECODING,     CURL_OPT_LONG       },  /* 158 */` |
|       - | 1368 | `	{ CURLOPT_NEW_FILE_PERMS,            CURL_OPT_LONG       },  /* 159 */` |
|       - | 1369 | `	{ CURLOPT_NEW_DIRECTORY_PERMS,       CURL_OPT_LONG       },  /* 160 */` |
|       - | 1370 | `	{ CURLOPT_POSTREDIR,                 CURL_OPT_LONG       },  /* 161 */` |
|       - | 1371 | `	{ CURLOPT_PROXY_TRANSFER_MODE,       CURL_OPT_LONG       },  /* 166 */` |
|       - | 1372 | `	{ CURLOPT_ADDRESS_SCOPE,             CURL_OPT_LONG       },  /* 171 */` |
|       - | 1373 | `	{ CURLOPT_CERTINFO,                  CURL_OPT_LONG       },  /* 172 */` |
|       - | 1374 | `	{ CURLOPT_TFTP_BLKSIZE,              CURL_OPT_LONG       },  /* 178 */` |
|       - | 1375 | `	{ CURLOPT_SOCKS5_GSSAPI_NEC,         CURL_OPT_LONG       },  /* 180 */` |
|       - | 1376 | `	{ CURLOPT_PROTOCOLS,                 CURL_OPT_LONG       },  /* 181 */` |
|       - | 1377 | `	{ CURLOPT_REDIR_PROTOCOLS,           CURL_OPT_LONG       },  /* 182 */` |
|       - | 1378 | `	{ CURLOPT_FTP_USE_PRET,              CURL_OPT_LONG       },  /* 188 */` |
|       - | 1379 | `	{ CURLOPT_RTSP_REQUEST,              CURL_OPT_LONG       },  /* 189 */` |
|       - | 1380 | `	{ CURLOPT_RTSP_CLIENT_CSEQ,          CURL_OPT_LONG       },  /* 193 */` |
|       - | 1381 | `	{ CURLOPT_RTSP_SERVER_CSEQ,          CURL_OPT_LONG       },  /* 194 */` |
|       - | 1382 | `	{ CURLOPT_WILDCARDMATCH,             CURL_OPT_LONG       },  /* 197 */` |
|       - | 1383 | `	{ CURLOPT_TRANSFER_ENCODING,         CURL_OPT_LONG       },  /* 207 */` |
|       - | 1384 | `	{ CURLOPT_GSSAPI_DELEGATION,         CURL_OPT_LONG       },  /* 210 */` |
|       - | 1385 | `	{ CURLOPT_ACCEPTTIMEOUT_MS,          CURL_OPT_LONG       },  /* 212 */` |
|       - | 1386 | `	{ CURLOPT_TCP_KEEPALIVE,             CURL_OPT_LONG       },  /* 213 */` |
|       - | 1387 | `	{ CURLOPT_TCP_KEEPIDLE,              CURL_OPT_LONG       },  /* 214 */` |
|       - | 1388 | `	{ CURLOPT_TCP_KEEPINTVL,             CURL_OPT_LONG       },  /* 215 */` |
|       - | 1389 | `	{ CURLOPT_SSL_OPTIONS,               CURL_OPT_LONG       },  /* 216 */` |
|       - | 1390 | `	{ CURLOPT_SASL_IR,                   CURL_OPT_LONG       },  /* 218 */` |
|       - | 1391 | `	{ CURLOPT_SSL_ENABLE_NPN,            CURL_OPT_LONG       },  /* 225 */` |
|       - | 1392 | `	{ CURLOPT_SSL_ENABLE_ALPN,           CURL_OPT_LONG       },  /* 226 */` |
|       - | 1393 | `	{ CURLOPT_EXPECT_100_TIMEOUT_MS,     CURL_OPT_LONG       },  /* 227 */` |
|       - | 1394 | `	{ CURLOPT_HEADEROPT,                 CURL_OPT_LONG       },  /* 229 */` |
|       - | 1395 | `	{ CURLOPT_SSL_VERIFYSTATUS,          CURL_OPT_LONG       },  /* 232 */` |
|       - | 1396 | `	{ CURLOPT_SSL_FALSESTART,            CURL_OPT_LONG       },  /* 233 */` |
|       - | 1397 | `	{ CURLOPT_PATH_AS_IS,                CURL_OPT_LONG       },  /* 234 */` |
|       - | 1398 | `	{ CURLOPT_PIPEWAIT,                  CURL_OPT_LONG       },  /* 237 */` |
|       - | 1399 | `	{ CURLOPT_STREAM_WEIGHT,             CURL_OPT_LONG       },  /* 239 */` |
|       - | 1400 | `	{ CURLOPT_TFTP_NO_OPTIONS,           CURL_OPT_LONG       },  /* 242 */` |
|       - | 1401 | `	{ CURLOPT_TCP_FASTOPEN,              CURL_OPT_LONG       },  /* 244 */` |
|       - | 1402 | `	{ CURLOPT_KEEP_SENDING_ON_ERROR,     CURL_OPT_LONG       },  /* 245 */` |
|       - | 1403 | `	{ CURLOPT_PROXY_SSL_VERIFYPEER,      CURL_OPT_LONG       },  /* 248 */` |
|       - | 1404 | `	{ CURLOPT_PROXY_SSL_VERIFYHOST,      CURL_OPT_LONG       },  /* 249 */` |
|       - | 1405 | `	{ CURLOPT_PROXY_SSLVERSION,          CURL_OPT_LONG       },  /* 250 */` |
|       - | 1406 | `	{ CURLOPT_PROXY_SSL_OPTIONS,         CURL_OPT_LONG       },  /* 261 */` |
|       - | 1407 | `	{ CURLOPT_SUPPRESS_CONNECT_HEADERS,  CURL_OPT_LONG       },  /* 265 */` |
|       - | 1408 | `	{ CURLOPT_SOCKS5_AUTH,               CURL_OPT_LONG       },  /* 267 */` |
|       - | 1409 | `	{ CURLOPT_SSH_COMPRESSION,           CURL_OPT_LONG       },  /* 268 */` |
|       - | 1410 | `	{ CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS, CURL_OPT_LONG       },  /* 271 */` |
|       - | 1411 | `	{ CURLOPT_HAPROXYPROTOCOL,           CURL_OPT_LONG       },  /* 274 */` |
|       - | 1412 | `	{ CURLOPT_DNS_SHUFFLE_ADDRESSES,     CURL_OPT_LONG       },  /* 275 */` |
|       - | 1413 | `	{ CURLOPT_DISALLOW_USERNAME_IN_URL,  CURL_OPT_LONG       },  /* 278 */` |
|       - | 1414 | `	{ CURLOPT_UPLOAD_BUFFERSIZE,         CURL_OPT_LONG       },  /* 280 */` |
|       - | 1415 | `	{ CURLOPT_UPKEEP_INTERVAL_MS,        CURL_OPT_LONG       },  /* 281 */` |
|       - | 1416 | `	{ CURLOPT_HTTP09_ALLOWED,            CURL_OPT_LONG       },  /* 285 */` |
|       - | 1417 | `	{ CURLOPT_ALTSVC_CTRL,               CURL_OPT_LONG       },  /* 286 */` |
|       - | 1418 | `	{ CURLOPT_MAXAGE_CONN,               CURL_OPT_LONG       },  /* 288 */` |
|       - | 1419 | `	{ CURLOPT_MAIL_RCPT_ALLLOWFAILS,     CURL_OPT_LONG       },  /* 290 */` |
|       - | 1420 | `	{ CURLOPT_HSTS_CTRL,                 CURL_OPT_LONG       },  /* 299 */` |
|       - | 1421 | `	{ CURLOPT_DOH_SSL_VERIFYPEER,        CURL_OPT_LONG       },  /* 306 */` |
|       - | 1422 | `	{ CURLOPT_DOH_SSL_VERIFYHOST,        CURL_OPT_LONG       },  /* 307 */` |
|       - | 1423 | `	{ CURLOPT_DOH_SSL_VERIFYSTATUS,      CURL_OPT_LONG       },  /* 308 */` |
|       - | 1424 | `	{ CURLOPT_MAXLIFETIME_CONN,          CURL_OPT_LONG       },  /* 314 */` |
|       - | 1425 | `	{ CURLOPT_MIME_OPTIONS,              CURL_OPT_LONG       },  /* 315 */` |
|       - | 1426 | `	{ CURLOPT_WS_OPTIONS,                CURL_OPT_LONG       },  /* 320 */` |
|       - | 1427 | `	{ CURLOPT_CA_CACHE_TIMEOUT,          CURL_OPT_LONG       },  /* 321 */` |
|       - | 1428 | `	{ CURLOPT_QUICK_EXIT,                CURL_OPT_LONG       },  /* 322 */` |
|       - | 1429 | `	{ CURLOPT_FILE,                      CURL_OPT_FILE       },  /* 10001 */` |
|       - | 1430 | `	{ CURLOPT_URL,                       CURL_OPT_STRING     },  /* 10002 */` |
|       - | 1431 | `	{ CURLOPT_PROXY,                     CURL_OPT_STRING     },  /* 10004 */` |
|       - | 1432 | `	{ CURLOPT_USERPWD,                   CURL_OPT_STRING     },  /* 10005 */` |
|       - | 1433 | `	{ CURLOPT_PROXYUSERPWD,              CURL_OPT_STRING     },  /* 10006 */` |
|       - | 1434 | `	{ CURLOPT_RANGE,                     CURL_OPT_STRING     },  /* 10007 */` |
|       - | 1435 | `	{ CURLOPT_INFILE,                    CURL_OPT_FILE       },  /* 10009 */` |
|       - | 1436 | `	{ CURLOPT_READDATA,                  CURL_OPT_FILE       },  /* 10009 */` |
|       - | 1437 | `	{ CURLOPT_POSTFIELDS,                CURL_OPT_POSTFIELDS },  /* 10015 */` |
|       - | 1438 | `	{ CURLOPT_REFERER,                   CURL_OPT_STRING     },  /* 10016 */` |
|       - | 1439 | `	{ CURLOPT_FTPPORT,                   CURL_OPT_STRING     },  /* 10017 */` |
|       - | 1440 | `	{ CURLOPT_USERAGENT,                 CURL_OPT_STRING     },  /* 10018 */` |
|       - | 1441 | `	{ CURLOPT_COOKIE,                    CURL_OPT_STRING     },  /* 10022 */` |
|       - | 1442 | `	{ CURLOPT_HTTPHEADER,                CURL_OPT_SLIST      },  /* 10023 */` |
|       - | 1443 | `	{ CURLOPT_SSLCERT,                   CURL_OPT_STRING     },  /* 10025 */` |
|       - | 1444 | `	{ CURLOPT_KEYPASSWD,                 CURL_OPT_STRING     },  /* 10026 */` |
|       - | 1445 | `	{ CURLOPT_SSLCERTPASSWD,             CURL_OPT_STRING     },  /* 10026 */` |
|       - | 1446 | `	{ CURLOPT_SSLKEYPASSWD,              CURL_OPT_STRING     },  /* 10026 */` |
|       - | 1447 | `	{ CURLOPT_QUOTE,                     CURL_OPT_SLIST      },  /* 10028 */` |
|       - | 1448 | `	{ CURLOPT_WRITEHEADER,               CURL_OPT_FILE       },  /* 10029 */` |
|       - | 1449 | `	{ CURLOPT_COOKIEFILE,                CURL_OPT_STRING     },  /* 10031 */` |
|       - | 1450 | `	{ CURLOPT_CUSTOMREQUEST,             CURL_OPT_STRING     },  /* 10036 */` |
|       - | 1451 | `	{ CURLOPT_STDERR,                    CURL_OPT_FILE       },  /* 10037 */` |
|       - | 1452 | `	{ CURLOPT_POSTQUOTE,                 CURL_OPT_SLIST      },  /* 10039 */` |
|       - | 1453 | `	{ CURLOPT_INTERFACE,                 CURL_OPT_STRING     },  /* 10062 */` |
|       - | 1454 | `	{ CURLOPT_KRB4LEVEL,                 CURL_OPT_STRING     },  /* 10063 */` |
|       - | 1455 | `	{ CURLOPT_KRBLEVEL,                  CURL_OPT_STRING     },  /* 10063 */` |
|       - | 1456 | `	{ CURLOPT_CAINFO,                    CURL_OPT_STRING     },  /* 10065 */` |
|       - | 1457 | `	{ CURLOPT_TELNETOPTIONS,             CURL_OPT_SLIST      },  /* 10070 */` |
|       - | 1458 | `	{ CURLOPT_RANDOM_FILE,               CURL_OPT_STRING     },  /* 10076 */` |
|       - | 1459 | `	{ CURLOPT_EGDSOCKET,                 CURL_OPT_STRING     },  /* 10077 */` |
|       - | 1460 | `	{ CURLOPT_COOKIEJAR,                 CURL_OPT_STRING     },  /* 10082 */` |
|       - | 1461 | `	{ CURLOPT_SSL_CIPHER_LIST,           CURL_OPT_STRING     },  /* 10083 */` |
|       - | 1462 | `	{ CURLOPT_SSLCERTTYPE,               CURL_OPT_STRING     },  /* 10086 */` |
|       - | 1463 | `	{ CURLOPT_SSLKEY,                    CURL_OPT_STRING     },  /* 10087 */` |
|       - | 1464 | `	{ CURLOPT_SSLKEYTYPE,                CURL_OPT_STRING     },  /* 10088 */` |
|       - | 1465 | `	{ CURLOPT_SSLENGINE,                 CURL_OPT_STRING     },  /* 10089 */` |
|       - | 1466 | `	{ CURLOPT_PREQUOTE,                  CURL_OPT_SLIST      },  /* 10093 */` |
|       - | 1467 | `	{ CURLOPT_CAPATH,                    CURL_OPT_STRING     },  /* 10097 */` |
|       - | 1468 | `	{ CURLOPT_SHARE,                     CURL_OPT_SHARE      },  /* 10100 */` |
|       - | 1469 | `	{ CURLOPT_ACCEPT_ENCODING,           CURL_OPT_STRING     },  /* 10102 */` |
|       - | 1470 | `	{ CURLOPT_ENCODING,                  CURL_OPT_STRING     },  /* 10102 */` |
|       - | 1471 | `	{ CURLOPT_PRIVATE,                   CURL_OPT_PRIVATE    },  /* 10103 */` |
|       - | 1472 | `	{ CURLOPT_HTTP200ALIASES,            CURL_OPT_SLIST      },  /* 10104 */` |
|       - | 1473 | `	{ CURLOPT_NETRC_FILE,                CURL_OPT_STRING     },  /* 10118 */` |
|       - | 1474 | `	{ CURLOPT_FTP_ACCOUNT,               CURL_OPT_STRING     },  /* 10134 */` |
|       - | 1475 | `	{ CURLOPT_COOKIELIST,                CURL_OPT_STRING     },  /* 10135 */` |
|       - | 1476 | `	{ CURLOPT_FTP_ALTERNATIVE_TO_USER,   CURL_OPT_STRING     },  /* 10147 */` |
|       - | 1477 | `	{ CURLOPT_SSH_PUBLIC_KEYFILE,        CURL_OPT_STRING     },  /* 10152 */` |
|       - | 1478 | `	{ CURLOPT_SSH_PRIVATE_KEYFILE,       CURL_OPT_STRING     },  /* 10153 */` |
|       - | 1479 | `	{ CURLOPT_SSH_HOST_PUBLIC_KEY_MD5,   CURL_OPT_STRING     },  /* 10162 */` |
|       - | 1480 | `	{ CURLOPT_CRLFILE,                   CURL_OPT_STRING     },  /* 10169 */` |
|       - | 1481 | `	{ CURLOPT_ISSUERCERT,                CURL_OPT_STRING     },  /* 10170 */` |
|       - | 1482 | `	{ CURLOPT_USERNAME,                  CURL_OPT_STRING     },  /* 10173 */` |
|       - | 1483 | `	{ CURLOPT_PASSWORD,                  CURL_OPT_STRING     },  /* 10174 */` |
|       - | 1484 | `	{ CURLOPT_PROXYUSERNAME,             CURL_OPT_STRING     },  /* 10175 */` |
|       - | 1485 | `	{ CURLOPT_PROXYPASSWORD,             CURL_OPT_STRING     },  /* 10176 */` |
|       - | 1486 | `	{ CURLOPT_NOPROXY,                   CURL_OPT_STRING     },  /* 10177 */` |
|       - | 1487 | `	{ CURLOPT_SOCKS5_GSSAPI_SERVICE,     CURL_OPT_STRING     },  /* 10179 */` |
|       - | 1488 | `	{ CURLOPT_SSH_KNOWNHOSTS,            CURL_OPT_STRING     },  /* 10183 */` |
|       - | 1489 | `	{ CURLOPT_MAIL_FROM,                 CURL_OPT_STRING     },  /* 10186 */` |
|       - | 1490 | `	{ CURLOPT_MAIL_RCPT,                 CURL_OPT_SLIST      },  /* 10187 */` |
|       - | 1491 | `	{ CURLOPT_RTSP_SESSION_ID,           CURL_OPT_STRING     },  /* 10190 */` |
|       - | 1492 | `	{ CURLOPT_RTSP_STREAM_URI,           CURL_OPT_STRING     },  /* 10191 */` |
|       - | 1493 | `	{ CURLOPT_RTSP_TRANSPORT,            CURL_OPT_STRING     },  /* 10192 */` |
|       - | 1494 | `	{ CURLOPT_RESOLVE,                   CURL_OPT_SLIST      },  /* 10203 */` |
|       - | 1495 | `	{ CURLOPT_TLSAUTH_USERNAME,          CURL_OPT_STRING     },  /* 10204 */` |
|       - | 1496 | `	{ CURLOPT_TLSAUTH_PASSWORD,          CURL_OPT_STRING     },  /* 10205 */` |
|       - | 1497 | `	{ CURLOPT_TLSAUTH_TYPE,              CURL_OPT_STRING     },  /* 10206 */` |
|       - | 1498 | `	{ CURLOPT_DNS_SERVERS,               CURL_OPT_STRING     },  /* 10211 */` |
|       - | 1499 | `	{ CURLOPT_MAIL_AUTH,                 CURL_OPT_STRING     },  /* 10217 */` |
|       - | 1500 | `	{ CURLOPT_XOAUTH2_BEARER,            CURL_OPT_STRING     },  /* 10220 */` |
|       - | 1501 | `	{ CURLOPT_DNS_INTERFACE,             CURL_OPT_STRING     },  /* 10221 */` |
|       - | 1502 | `	{ CURLOPT_DNS_LOCAL_IP4,             CURL_OPT_STRING     },  /* 10222 */` |
|       - | 1503 | `	{ CURLOPT_DNS_LOCAL_IP6,             CURL_OPT_STRING     },  /* 10223 */` |
|       - | 1504 | `	{ CURLOPT_LOGIN_OPTIONS,             CURL_OPT_STRING     },  /* 10224 */` |
|       - | 1505 | `	{ CURLOPT_PROXYHEADER,               CURL_OPT_SLIST      },  /* 10228 */` |
|       - | 1506 | `	{ CURLOPT_PINNEDPUBLICKEY,           CURL_OPT_STRING     },  /* 10230 */` |
|       - | 1507 | `	{ CURLOPT_UNIX_SOCKET_PATH,          CURL_OPT_STRING     },  /* 10231 */` |
|       - | 1508 | `	{ CURLOPT_PROXY_SERVICE_NAME,        CURL_OPT_STRING     },  /* 10235 */` |
|       - | 1509 | `	{ CURLOPT_SERVICE_NAME,              CURL_OPT_STRING     },  /* 10236 */` |
|       - | 1510 | `	{ CURLOPT_DEFAULT_PROTOCOL,          CURL_OPT_STRING     },  /* 10238 */` |
|       - | 1511 | `	{ CURLOPT_CONNECT_TO,                CURL_OPT_SLIST      },  /* 10243 */` |
|       - | 1512 | `	{ CURLOPT_PROXY_CAINFO,              CURL_OPT_STRING     },  /* 10246 */` |
|       - | 1513 | `	{ CURLOPT_PROXY_CAPATH,              CURL_OPT_STRING     },  /* 10247 */` |
|       - | 1514 | `	{ CURLOPT_PROXY_TLSAUTH_USERNAME,    CURL_OPT_STRING     },  /* 10251 */` |
|       - | 1515 | `	{ CURLOPT_PROXY_TLSAUTH_PASSWORD,    CURL_OPT_STRING     },  /* 10252 */` |
|       - | 1516 | `	{ CURLOPT_PROXY_TLSAUTH_TYPE,        CURL_OPT_STRING     },  /* 10253 */` |
|       - | 1517 | `	{ CURLOPT_PROXY_SSLCERT,             CURL_OPT_STRING     },  /* 10254 */` |
|       - | 1518 | `	{ CURLOPT_PROXY_SSLCERTTYPE,         CURL_OPT_STRING     },  /* 10255 */` |
|       - | 1519 | `	{ CURLOPT_PROXY_SSLKEY,              CURL_OPT_STRING     },  /* 10256 */` |
|       - | 1520 | `	{ CURLOPT_PROXY_SSLKEYTYPE,          CURL_OPT_STRING     },  /* 10257 */` |
|       - | 1521 | `	{ CURLOPT_PROXY_KEYPASSWD,           CURL_OPT_STRING     },  /* 10258 */` |
|       - | 1522 | `	{ CURLOPT_PROXY_SSL_CIPHER_LIST,     CURL_OPT_STRING     },  /* 10259 */` |
|       - | 1523 | `	{ CURLOPT_PROXY_CRLFILE,             CURL_OPT_STRING     },  /* 10260 */` |
|       - | 1524 | `	{ CURLOPT_PRE_PROXY,                 CURL_OPT_STRING     },  /* 10262 */` |
|       - | 1525 | `	{ CURLOPT_PROXY_PINNEDPUBLICKEY,     CURL_OPT_STRING     },  /* 10263 */` |
|       - | 1526 | `	{ CURLOPT_ABSTRACT_UNIX_SOCKET,      CURL_OPT_STRING     },  /* 10264 */` |
|       - | 1527 | `	{ CURLOPT_REQUEST_TARGET,            CURL_OPT_STRING     },  /* 10266 */` |
|       - | 1528 | `	{ CURLOPT_TLS13_CIPHERS,             CURL_OPT_STRING     },  /* 10276 */` |
|       - | 1529 | `	{ CURLOPT_PROXY_TLS13_CIPHERS,       CURL_OPT_STRING     },  /* 10277 */` |
|       - | 1530 | `	{ CURLOPT_DOH_URL,                   CURL_OPT_STRING     },  /* 10279 */` |
|       - | 1531 | `	{ CURLOPT_ALTSVC,                    CURL_OPT_STRING     },  /* 10287 */` |
|       - | 1532 | `	{ CURLOPT_SASL_AUTHZID,              CURL_OPT_STRING     },  /* 10289 */` |
|       - | 1533 | `	{ CURLOPT_PROXY_ISSUERCERT,          CURL_OPT_STRING     },  /* 10296 */` |
|       - | 1534 | `	{ CURLOPT_SSL_EC_CURVES,             CURL_OPT_STRING     },  /* 10298 */` |
|       - | 1535 | `	{ CURLOPT_HSTS,                      CURL_OPT_STRING     },  /* 10300 */` |
|       - | 1536 | `	{ CURLOPT_AWS_SIGV4,                 CURL_OPT_STRING     },  /* 10305 */` |
|       - | 1537 | `	{ CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256, CURL_OPT_STRING     },  /* 10311 */` |
|       - | 1538 | `	{ CURLOPT_PROTOCOLS_STR,             CURL_OPT_STRING     },  /* 10318 */` |
|       - | 1539 | `	{ CURLOPT_REDIR_PROTOCOLS_STR,       CURL_OPT_STRING     },  /* 10319 */` |
|       - | 1540 | `	{ 19913,                             CURL_OPT_RETURN     },  /* 19913 */` |
|       - | 1541 | `	{ 19914,                             CURL_OPT_IGNORE     },  /* 19914 */` |
|       - | 1542 | `	{ CURLOPT_WRITEFUNCTION,             CURL_OPT_CALLBACK   },  /* 20011 */` |
|       - | 1543 | `	{ CURLOPT_READFUNCTION,              CURL_OPT_CALLBACK   },  /* 20012 */` |
|       - | 1544 | `	{ CURLOPT_PROGRESSFUNCTION,          CURL_OPT_CALLBACK   },  /* 20056 */` |
|       - | 1545 | `	{ CURLOPT_HEADERFUNCTION,            CURL_OPT_CALLBACK   },  /* 20079 */` |
|       - | 1546 | `	{ CURLOPT_DEBUGFUNCTION,             CURL_OPT_CALLBACK   },  /* 20094 */` |
|       - | 1547 | `	{ CURLOPT_FNMATCH_FUNCTION,          CURL_OPT_CALLBACK   },  /* 20200 */` |
|       - | 1548 | `	{ CURLOPT_XFERINFOFUNCTION,          CURL_OPT_CALLBACK   },  /* 20219 */` |
|       - | 1549 | `	{ CURLOPT_PREREQFUNCTION,            CURL_OPT_CALLBACK   },  /* 20312 */` |
|       - | 1550 | `	{ CURLOPT_SSH_HOSTKEYFUNCTION,       CURL_OPT_CALLBACK   },  /* 20316 */` |
|       - | 1551 | `	{ CURLOPT_INFILESIZE_LARGE,          CURL_OPT_LONG       },  /* 30115 */` |
|       - | 1552 | `	{ CURLOPT_MAXFILESIZE_LARGE,         CURL_OPT_LONG       },  /* 30117 */` |
|       - | 1553 | `	{ CURLOPT_MAX_SEND_SPEED_LARGE,      CURL_OPT_LONG       },  /* 30145 */` |
|       - | 1554 | `	{ CURLOPT_MAX_RECV_SPEED_LARGE,      CURL_OPT_LONG       },  /* 30146 */` |
|       - | 1555 | `	{ CURLOPT_TIMEVALUE_LARGE,           CURL_OPT_LONG       },  /* 30270 */` |
|       - | 1556 | `	{ CURLOPT_SSLCERT_BLOB,              CURL_OPT_STRING     },  /* 40291 */` |
|       - | 1557 | `	{ CURLOPT_SSLKEY_BLOB,               CURL_OPT_STRING     },  /* 40292 */` |
|       - | 1558 | `	{ CURLOPT_PROXY_SSLCERT_BLOB,        CURL_OPT_STRING     },  /* 40293 */` |
|       - | 1559 | `	{ CURLOPT_PROXY_SSLKEY_BLOB,         CURL_OPT_STRING     },  /* 40294 */` |
|       - | 1560 | `	{ CURLOPT_ISSUERCERT_BLOB,           CURL_OPT_STRING     },  /* 40295 */` |
|       - | 1561 | `	{ CURLOPT_PROXY_ISSUERCERT_BLOB,     CURL_OPT_STRING     },  /* 40297 */` |
|       - | 1562 | `	{ CURLOPT_CAINFO_BLOB,               CURL_OPT_STRING     },  /* 40309 */` |
|       - | 1563 | `	{ CURLOPT_PROXY_CAINFO_BLOB,         CURL_OPT_STRING     },  /* 40310 */` |
|       - | 1564 | `};` |
|       - | 1565 |  |
|       - | 1566 | `/*` |
|       - | 1567 | ` * The option's php NAME, for the diagnostics that print one ("The` |
|       - | 1568 | ` * CURLOPT_HTTPHEADER option must have an array value"). Read out of the` |
|       - | 1569 | ` * constant table rather than repeated, so the two can never disagree.` |
|       - | 1570 | ` */` |
|      60 | 1571 | `static const char * CurlOptName(sxi64 iOpt)` |
|       1 | 1572 | `{` |
|       - | 1573 | `	sxu32 n;` |
|    5057 | 1574 | `	for( n = 0 ; n < SX_ARRAYSIZE(aCurlConst) ; ++n ){` |
|    5056 | 1575 | `		if( aCurlConst[n].iValue == iOpt` |
|    2559 | 1576 | `		 && SyStrncmp(aCurlConst[n].zName,"CURLOPT_",sizeof("CURLOPT_")-1) == 0 ){` |
|      61 | 1577 | `			return aCurlConst[n].zName;` |
|       - | 1578 | `		}` |
|    2499 | 1579 | `	}` |
|     ! 0 | 1580 | `	return "CURLOPT_UNKNOWN";` |
|      31 | 1581 | `}` |
|       - | 1582 |  |
|     168 | 1583 | `static const struct CurlOptDef * CurlOptFind(sxi64 iOpt)` |
|       1 | 1584 | `{` |
|       - | 1585 | `	sxu32 n;` |
|   26449 | 1586 | `	for( n = 0 ; n < SX_ARRAYSIZE(aCurlOpt) ; ++n ){` |
|   26439 | 1587 | `		if( aCurlOpt[n].iOpt == iOpt ){` |
|     159 | 1588 | `			return &aCurlOpt[n];` |
|       - | 1589 | `		}` |
|   13141 | 1590 | `	}` |
|      11 | 1591 | `	return 0;` |
|      85 | 1592 | `}` |
|       - | 1593 |  |
|       - | 1594 | `/* ===== The handle verbs ===== */` |
|       - | 1595 |  |
|       - | 1596 | `/*` |
|       - | 1597 | `` * A bare instance of one of the handle classes. `new` on them is refused`` |
|       - | 1598 | ` * (php's "Cannot directly construct ..."), and this is the door the refusal` |
|       - | 1599 | ` * leaves open: the engine's own creation step, never the opcode's.` |
|       - | 1600 | ` */` |
|     104 | 1601 | `static ph7_class_instance * CurlNewInstance(ph7_vm *pVm,const char *zName,int nName)` |
|       2 | 1602 | `{` |
|     106 | 1603 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,0,0);` |
|     106 | 1604 | `	return pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|       2 | 1605 | `}` |
|       - | 1606 |  |
|       - | 1607 | `/* CurlHandle\|false curl_init(?string $url = null) */` |
|     100 | 1608 | `static int vm_builtin_curl_init(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1609 | `{` |
|     102 | 1610 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1611 | `	ph7_class_instance *pThis;` |
|       - | 1612 | `	phl_curl *pCurl;` |
|     102 | 1613 | `	const char *zUrl = 0;` |
|     102 | 1614 | `	int nUrl = 0;` |
|     102 | 1615 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|      74 | 1616 | `		zUrl = ph7_value_to_string(apArg[0],&nUrl);` |
|       - | 1617 | `		/*` |
|       - | 1618 | `		 * php screens the URL for an embedded NUL before libcurl sees it,` |
|       - | 1619 | `		 * because libcurl takes a C string and would silently stop at the` |
|       - | 1620 | `		 * byte. The sentence says "cURL option" even though the argument is` |
|       - | 1621 | `		 * $url -- it is the shared option-setter's wording, reached from here.` |
|       - | 1622 | `		 */` |
|      74 | 1623 | `		if( SyByteFind(zUrl,(sxu32)nUrl,0,0) == SXRET_OK ){` |
|       - | 1624 | `			/* The status a throw installs IS the builtin's status: answering` |
|       - | 1625 | `			 * PH7_OK leaves the throw half-raised, and the next script run in` |
|       - | 1626 | `			 * the same interpreter prints nothing at all. */` |
|       3 | 1627 | `			ph7_result_bool(pCtx,0);` |
|       3 | 1628 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1629 | `				"curl_init(): cURL option must not contain any null bytes");` |
|       - | 1630 | `		}` |
|      35 | 1631 | `	}` |
|     100 | 1632 | `	pCurl = CurlNewHandle(pVm);` |
|     100 | 1633 | `	if( pCurl == 0 ){` |
|     ! 0 | 1634 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1635 | `		return PH7_OK;` |
|       - | 1636 | `	}` |
|     100 | 1637 | `	pThis = CurlNewInstance(pVm,"CurlHandle",sizeof("CurlHandle")-1);` |
|     100 | 1638 | `	if( pThis == 0 \|\| CurlAttach(pThis,pCurl) != 0 ){` |
|     ! 0 | 1639 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1640 | `		return PH7_OK;` |
|       - | 1641 | `	}` |
|     100 | 1642 | `	if( zUrl ){` |
|      72 | 1643 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_URL,zUrl);` |
|      35 | 1644 | `	}` |
|     100 | 1645 | `	PH7_NativeResultObject(pCtx,pThis);` |
|     100 | 1646 | `	return PH7_OK;` |
|      52 | 1647 | `}` |
|       - | 1648 | `/*` |
|       - | 1649 | ` * void curl_close(CurlHandle $handle)` |
|       - | 1650 | ` *` |
|       - | 1651 | ` * A NO-OP, which is the whole finding. php 8 turned the resource into an` |
|       - | 1652 | ` * object, and the object's own teardown is what frees the handle -- so after` |
|       - | 1653 | ` * curl_close() the handle still works: curl_setopt() answers true, curl_exec()` |
|       - | 1654 | ` * runs the transfer, curl_errno() reports it. Freeing here (which is what the` |
|       - | 1655 | ` * name says and what php 7 did) would make every one of those a use-after-free` |
|       - | 1656 | ` * on a script php runs happily.` |
|       - | 1657 | ` */` |
|       8 | 1658 | `static int vm_builtin_curl_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1659 | `{` |
|       4 | 1660 | `	SXUNUSED(nArg);` |
|       4 | 1661 | `	SXUNUSED(apArg);` |
|       9 | 1662 | `	ph7_result_null(pCtx);` |
|       9 | 1663 | `	return PH7_OK;` |
|       1 | 1664 | `}` |
|       - | 1665 | `/*` |
|       - | 1666 | ` * void curl_reset(CurlHandle $handle)` |
|       - | 1667 | ` *` |
|       - | 1668 | ` * Every OPTION goes back to its default -- and the error state does NOT.` |
|       - | 1669 | ` * curl_reset() on a handle whose last transfer failed leaves curl_errno()` |
|       - | 1670 | ` * reporting that failure; only the option setters clear it. (Probing the verbs` |
|       - | 1671 | ` * one at a time is what shows this: a sweep that resets after a setopt sees a` |
|       - | 1672 | ` * cleared errno and credits the wrong verb.)` |
|       - | 1673 | ` */` |
|       6 | 1674 | `static int vm_builtin_curl_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1675 | `{` |
|       7 | 1676 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       7 | 1677 | `	if( pCurl && pCurl->pEasy ){` |
|       7 | 1678 | `		curl_easy_reset(pCurl->pEasy);` |
|       - | 1679 | `		/* curl_easy_reset() drops the error buffer with everything else. */` |
|       7 | 1680 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_ERRORBUFFER,pCurl->zErrBuf);` |
|       3 | 1681 | `	}` |
|       7 | 1682 | `	ph7_result_null(pCtx);` |
|       7 | 1683 | `	return PH7_OK;` |
|       1 | 1684 | `}` |
|       - | 1685 | `/*` |
|       - | 1686 | ` * php's error state after a REFUSED option: errno 48 and libcurl's own text` |
|       - | 1687 | ` * for it. The ValueError is thrown and the handle still reports the failure --` |
|       - | 1688 | `` * `curl_setopt($h, 999999, 1)` in a try/catch leaves curl_errno() at 48 -- so`` |
|       - | 1689 | ` * the two are not alternatives, they both happen.` |
|       - | 1690 | ` */` |
|      10 | 1691 | `static void CurlSetErr(phl_curl *pCurl,int iCode)` |
|       1 | 1692 | `{` |
|       - | 1693 | `	const char *zMsg;` |
|      11 | 1694 | `	if( pCurl == 0 ){` |
|     ! 0 | 1695 | `		return;` |
|       - | 1696 | `	}` |
|      11 | 1697 | `	pCurl->iLastErr = iCode;` |
|      11 | 1698 | `	zMsg = curl_easy_strerror((CURLcode)iCode);` |
|      11 | 1699 | `	pCurl->zErrBuf[0] = 0;` |
|      11 | 1700 | `	if( zMsg ){` |
|      11 | 1701 | `		sxu32 nMsg = SyStrlen(zMsg);` |
|      11 | 1702 | `		if( nMsg > sizeof(pCurl->zErrBuf) - 1 ){ nMsg = sizeof(pCurl->zErrBuf) - 1; }` |
|      11 | 1703 | `		SyMemcpy(zMsg,pCurl->zErrBuf,nMsg);` |
|      11 | 1704 | `		pCurl->zErrBuf[nMsg] = 0;` |
|       5 | 1705 | `	}` |
|       6 | 1706 | `}` |
|       - | 1707 | `/*` |
|       - | 1708 | ` * Every setter clears the handle's error FIRST (curl_setopt and` |
|       - | 1709 | ` * curl_setopt_array are two of the three verbs that do; curl_upkeep is the` |
|       - | 1710 | ` * third, and curl_reset notably is not).` |
|       - | 1711 | ` */` |
|     162 | 1712 | `static void CurlClearErr(phl_curl *pCurl)` |
|       1 | 1713 | `{` |
|     163 | 1714 | `	if( pCurl ){` |
|     163 | 1715 | `		pCurl->iLastErr = 0;` |
|     163 | 1716 | `		pCurl->zErrBuf[0] = 0;` |
|      81 | 1717 | `	}` |
|     163 | 1718 | `}` |
|       - | 1719 | `/*` |
|       - | 1720 | ` * A string option's value. php stringifies ANYTHING for these -- null becomes` |
|       - | 1721 | ` * "", an array becomes "Array" with the ordinary conversion warning, an object` |
|       - | 1722 | ` * is the ordinary "could not be converted to string" Error -- and then screens` |
|       - | 1723 | ` * the result for a NUL, because libcurl takes a C string and would silently` |
|       - | 1724 | ` * stop at the byte. The screen is on the whole VALUE, which is why the same` |
|       - | 1725 | ` * sentence appears from curl_init().` |
|       - | 1726 | ` *` |
|       - | 1727 | ` * A slist ELEMENT is stringified the same way but is NOT screened: php lets a` |
|       - | 1728 | ` * header carrying a NUL through, which is measured, not assumed.` |
|       - | 1729 | ` */` |
|      22 | 1730 | `static int CurlSetString(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal,sxi32 *pRc)` |
|       1 | 1731 | `{` |
|       - | 1732 | `	const char *zVal;` |
|      23 | 1733 | `	int nVal = 0;` |
|       - | 1734 | `	sxi32 rcSv;` |
|       - | 1735 | `	/* The USER-VISIBLE coercion, not the embedder API: an array has to warn` |
|       - | 1736 | `	 * "Array to string conversion" and an object with no __toString() has to` |
|       - | 1737 | `	 * be php's catchable Error, both of which the silent ph7_value_to_string()` |
|       - | 1738 | `	 * skips. */` |
|      23 | 1739 | `	rcSv = PH7_ValueToStringUV(pCtx,pVal,&zVal,&nVal);` |
|      23 | 1740 | `	if( rcSv != SXRET_OK ){` |
|       - | 1741 | `		/* The coercion threw. Its status is the BUILTIN's status: swallowing it` |
|       - | 1742 | `		 * and answering PH7_OK leaves the engine with a half-installed throw,` |
|       - | 1743 | `		 * and the next script run in the same interpreter prints nothing. */` |
|       3 | 1744 | `		*pRc = rcSv;` |
|       3 | 1745 | `		return -1;` |
|       - | 1746 | `	}` |
|      21 | 1747 | `	if( SyByteFind(zVal,(sxu32)nVal,0,0) == SXRET_OK ){` |
|       5 | 1748 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1749 | `			"curl_setopt(): cURL option must not contain any null bytes");` |
|       5 | 1750 | `		return -1;` |
|       - | 1751 | `	}` |
|      17 | 1752 | `	return curl_easy_setopt(pCurl->pEasy,(CURLoption)iOpt,zVal) == CURLE_OK ? 1 : 0;` |
|      12 | 1753 | `}` |
|       - | 1754 | `/*` |
|       - | 1755 | ` * A long option's value, with the one option php screens by hand.` |
|       - | 1756 | ` * CURLOPT_SSL_VERIFYHOST no longer has a meaningful 1: libcurl treats it as 2,` |
|       - | 1757 | ` * and php says so at E_NOTICE before passing 2 along -- so a program that` |
|       - | 1758 | ` * still writes 1 gets php's sentence, not silence.` |
|       - | 1759 | ` */` |
|      28 | 1760 | `static int CurlSetLong(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal)` |
|       1 | 1761 | `{` |
|      29 | 1762 | `	sxi64 iVal = ph7_value_to_int64(pVal);` |
|      29 | 1763 | `	if( iOpt == CURLOPT_SSL_VERIFYHOST && iVal == 1 ){` |
|       - | 1764 | `		/* ph7_context_throw_error already prints "curl_setopt(): " */` |
|       5 | 1765 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,` |
|       - | 1766 | `			"CURLOPT_SSL_VERIFYHOST no longer accepts the value 1, "` |
|       - | 1767 | `			"value 2 will be used instead");` |
|       5 | 1768 | `		iVal = 2;` |
|       2 | 1769 | `	}` |
|      29 | 1770 | `	return curl_easy_setopt(pCurl->pEasy,(CURLoption)iOpt,(long)iVal) == CURLE_OK ? 1 : 0;` |
|       1 | 1771 | `}` |
|       - | 1772 | `/*` |
|       - | 1773 | ` * An slist option's value: the array's VALUES in order, keys ignored, each` |
|       - | 1774 | ` * stringified. libcurl does not copy the list, so the handle owns it until the` |
|       - | 1775 | ` * option is set again or the handle is freed -- which is what pSlist is for.` |
|       - | 1776 | ` */` |
|       - | 1777 | `struct CurlSlistBuild {` |
|       - | 1778 | `	ph7_context *pCtx;` |
|       - | 1779 | `	struct curl_slist *pList;` |
|       - | 1780 | `	int bFailed;` |
|       - | 1781 | `	int bThrew;` |
|       - | 1782 | `	sxi32 rcThrow;` |
|       - | 1783 | `};` |
|      28 | 1784 | `static int CurlSlistWalk(ph7_value *pKey,ph7_value *pVal,void *pUser)` |
|       1 | 1785 | `{` |
|      29 | 1786 | `	struct CurlSlistBuild *pB = (struct CurlSlistBuild *)pUser;` |
|       - | 1787 | `	struct curl_slist *pNext;` |
|       - | 1788 | `	const char *zVal;` |
|      29 | 1789 | `	int nVal = 0;` |
|      14 | 1790 | `	SXUNUSED(pKey);` |
|      29 | 1791 | `	if( pB->bFailed ){` |
|     ! 0 | 1792 | `		return PH7_OK;` |
|       - | 1793 | `	}` |
|       - | 1794 | `	/* Same user-visible coercion the scalar options get -- an object in a` |
|       - | 1795 | `	 * header list is php's Error, not a silent "Object" -- but NO null-byte` |
|       - | 1796 | `	 * screen: php lets a list element carrying one through. */` |
|       - | 1797 | `	{` |
|      29 | 1798 | `		sxi32 rcSv = PH7_ValueToStringUV(pB->pCtx,pVal,&zVal,&nVal);` |
|      29 | 1799 | `		if( rcSv != SXRET_OK ){` |
|       3 | 1800 | `			pB->bFailed = 1;` |
|       3 | 1801 | `			pB->bThrew = 1;` |
|       3 | 1802 | `			pB->rcThrow = rcSv;` |
|       3 | 1803 | `			return PH7_ABORT;` |
|       - | 1804 | `		}` |
|       - | 1805 | `	}` |
|       - | 1806 | `	/* The value is a TEMPORARY the walker owns for this call only, and` |
|       - | 1807 | `	 * curl_slist_append copies it, so nothing is kept past the return. */` |
|      27 | 1808 | `	pNext = curl_slist_append(pB->pList,zVal ? zVal : "");` |
|      27 | 1809 | `	if( pNext == 0 ){` |
|     ! 0 | 1810 | `		pB->bFailed = 1;` |
|     ! 0 | 1811 | `		return PH7_OK;` |
|       - | 1812 | `	}` |
|      27 | 1813 | `	pB->pList = pNext;` |
|      27 | 1814 | `	return PH7_OK;` |
|      15 | 1815 | `}` |
|      22 | 1816 | `static int CurlSetSlist(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal,` |
|       - | 1817 | `	const char *zOptName,sxi32 *pRc)` |
|       1 | 1818 | `{` |
|       - | 1819 | `	struct CurlSlistBuild sB;` |
|       - | 1820 | `	phl_curl_slist *pSlot;` |
|      23 | 1821 | `	if( !ph7_value_is_array(pVal) ){` |
|       4 | 1822 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       1 | 1823 | `			"curl_setopt(): The %s option must have an array value",zOptName);` |
|       3 | 1824 | `		return -1;` |
|       - | 1825 | `	}` |
|      21 | 1826 | `	sB.pCtx = pCtx;` |
|      21 | 1827 | `	sB.pList = 0;` |
|      21 | 1828 | `	sB.bFailed = 0;` |
|      21 | 1829 | `	sB.bThrew = 0;` |
|      21 | 1830 | `	sB.rcThrow = PH7_OK;` |
|      21 | 1831 | `	ph7_array_walk(pVal,CurlSlistWalk,&sB);` |
|      21 | 1832 | `	if( sB.bFailed ){` |
|       3 | 1833 | `		if( sB.pList ){` |
|     ! 0 | 1834 | `			curl_slist_free_all(sB.pList);` |
|     ! 0 | 1835 | `		}` |
|       3 | 1836 | `		if( sB.bThrew ){` |
|       3 | 1837 | `			*pRc = sB.rcThrow;` |
|       3 | 1838 | `			return -1;` |
|       - | 1839 | `		}` |
|     ! 0 | 1840 | `		return 0;` |
|       - | 1841 | `	}` |
|       - | 1842 | `	/* Replace whatever this option held before: the previous list stays alive` |
|       - | 1843 | `	 * until libcurl has been pointed at the new one. */` |
|      19 | 1844 | `	pSlot = CurlSlistSlot(pCurl,iOpt);` |
|      19 | 1845 | `	if( pSlot == 0 ){` |
|     ! 0 | 1846 | `		if( sB.pList ){` |
|     ! 0 | 1847 | `			curl_slist_free_all(sB.pList);` |
|     ! 0 | 1848 | `		}` |
|     ! 0 | 1849 | `		return 0;` |
|       - | 1850 | `	}` |
|      19 | 1851 | `	if( curl_easy_setopt(pCurl->pEasy,(CURLoption)iOpt,sB.pList) != CURLE_OK ){` |
|     ! 0 | 1852 | `		if( sB.pList ){` |
|     ! 0 | 1853 | `			curl_slist_free_all(sB.pList);` |
|     ! 0 | 1854 | `		}` |
|     ! 0 | 1855 | `		return 0;` |
|       - | 1856 | `	}` |
|      19 | 1857 | `	if( pSlot->pList ){` |
|      11 | 1858 | `		curl_slist_free_all(pSlot->pList);` |
|       5 | 1859 | `	}` |
|      19 | 1860 | `	pSlot->pList = sB.pList;` |
|      19 | 1861 | `	return 1;` |
|      12 | 1862 | `}` |
|       - | 1863 | `/*` |
|       - | 1864 | ` * One option, the whole switch. Answers 1 (true), 0 (false) or -1 (a throw is` |
|       - | 1865 | ` * already installed).` |
|       - | 1866 | ` */` |
|     156 | 1867 | `static int CurlSetOne(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal,` |
|       - | 1868 | `	const char *zFunc,sxi32 *pRc)` |
|       1 | 1869 | `{` |
|     157 | 1870 | `	const struct CurlOptDef *pDef = CurlOptFind(iOpt);` |
|     157 | 1871 | `	if( pDef == 0 ){` |
|       - | 1872 | `		/* php's switch has no arm for it, so the library never sees it -- and` |
|       - | 1873 | `		 * the handle records CURLE_UNKNOWN_OPTION all the same. */` |
|       7 | 1874 | `		CurlSetErr(pCurl,CURLE_UNKNOWN_OPTION);` |
|      10 | 1875 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       3 | 1876 | `			"%s(): Argument #2 ($option) is not a valid cURL option",zFunc);` |
|       7 | 1877 | `		return -1;` |
|       - | 1878 | `	}` |
|     151 | 1879 | `	switch( pDef->iKind ){` |
|      14 | 1880 | `	case CURL_OPT_LONG:` |
|      29 | 1881 | `		return CurlSetLong(pCtx,pCurl,iOpt,pVal);` |
|      11 | 1882 | `	case CURL_OPT_STRING:` |
|      23 | 1883 | `		return CurlSetString(pCtx,pCurl,iOpt,pVal,pRc);` |
|      11 | 1884 | `	case CURL_OPT_SLIST:` |
|      23 | 1885 | `		return CurlSetSlist(pCtx,pCurl,iOpt,pVal,CurlOptName(iOpt),pRc);` |
|       6 | 1886 | `	case CURL_OPT_SAFEUP:` |
|       - | 1887 | `		/* php's -1: safe uploads cannot be turned off any more, and the` |
|       - | 1888 | `		 * refusal is on the VALUE's truthiness, not its type. */` |
|      13 | 1889 | `		if( !ph7_value_to_bool(pVal) ){` |
|      10 | 1890 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       3 | 1891 | `				"%s(): Disabling safe uploads is no longer supported",zFunc);` |
|       7 | 1892 | `			return -1;` |
|       - | 1893 | `		}` |
|       7 | 1894 | `		return 1;` |
|      19 | 1895 | `	case CURL_OPT_CALLBACK:` |
|      39 | 1896 | `		switch( (int)iOpt ){` |
|      17 | 1897 | `		case CURLOPT_WRITEFUNCTION:` |
|      52 | 1898 | `			return CurlSetCallback(pCtx,pCurl,&pCurl->pWriteCb,pVal,zFunc,` |
|      17 | 1899 | `				CurlOptName(iOpt),pRc);` |
|     ! 0 | 1900 | `		case CURLOPT_HEADERFUNCTION:` |
|     ! 0 | 1901 | `			return CurlSetCallback(pCtx,pCurl,&pCurl->pHeaderCb,pVal,zFunc,` |
|     ! 0 | 1902 | `				CurlOptName(iOpt),pRc);` |
|       2 | 1903 | `		case CURLOPT_XFERINFOFUNCTION:` |
|       - | 1904 | `		case CURLOPT_PROGRESSFUNCTION: {` |
|       7 | 1905 | `			int rcCb = CurlSetCallback(pCtx,pCurl,&pCurl->pXferCb,pVal,zFunc,` |
|       2 | 1906 | `				CurlOptName(iOpt),pRc);` |
|       5 | 1907 | `			if( rcCb == 1 ){` |
|       5 | 1908 | `				pCurl->bXferIsProgress = (iOpt == CURLOPT_PROGRESSFUNCTION);` |
|       2 | 1909 | `			}` |
|       5 | 1910 | `			return rcCb;` |
|       - | 1911 | `		}` |
|     ! 0 | 1912 | `		default:` |
|     ! 0 | 1913 | `			break;` |
|       - | 1914 | `		}` |
|     ! 0 | 1915 | `		break;` |
|      14 | 1916 | `	case CURL_OPT_RETURN:` |
|       - | 1917 | `		/* php's own option, and a FLAG: every value is accepted (setopt always` |
|       - | 1918 | `		 * answers true) and only its truthiness matters, at exec time. */` |
|      29 | 1919 | `		pCurl->bReturnTransfer = ph7_value_to_bool(pVal) ? 1 : 0;` |
|      29 | 1920 | `		return 1;` |
|     ! 0 | 1921 | `	case CURL_OPT_IGNORE:` |
|     ! 0 | 1922 | `		return 1;` |
|     ! 0 | 1923 | `	default:` |
|     ! 0 | 1924 | `		break;` |
|       - | 1925 | `	}` |
|       - | 1926 | `	/* The kinds whose own slices have not landed: a callable, a stream, the` |
|       - | 1927 | `	 * share handle, PRIVATE's stored value and POSTFIELDS. Refusing loudly` |
|       - | 1928 | `	 * beats answering true and transferring something else. */` |
|     ! 0 | 1929 | `	*pRc = PH7_VmThrowException(pCtx,"Error",` |
|     ! 0 | 1930 | `		"%s(): option %s is not implemented yet in this build",zFunc,CurlOptName(iOpt));` |
|     ! 0 | 1931 | `	return -1;` |
|      79 | 1932 | `}` |
|       - | 1933 | `/* bool curl_setopt(CurlHandle $handle, int $option, mixed $value) */` |
|     148 | 1934 | `static int vm_builtin_curl_setopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1935 | `{` |
|     149 | 1936 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|     149 | 1937 | `	sxi32 rcOut = PH7_OK;` |
|       - | 1938 | `	int rc;` |
|     149 | 1939 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 1940 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1941 | `		return PH7_OK;` |
|       - | 1942 | `	}` |
|     149 | 1943 | `	CurlClearErr(pCurl);` |
|     149 | 1944 | `	rc = CurlSetOne(pCtx,pCurl,ph7_value_to_int64(apArg[1]),apArg[2],"curl_setopt",&rcOut);` |
|     149 | 1945 | `	ph7_result_bool(pCtx,rc == 1);` |
|     149 | 1946 | `	return rcOut;` |
|      75 | 1947 | `}` |
|       - | 1948 | `/*` |
|       - | 1949 | ` * bool curl_setopt_array(CurlHandle $handle, array $options)` |
|       - | 1950 | ` *` |
|       - | 1951 | ` * The options are applied IN ORDER and the walk stops at the first refusal --` |
|       - | 1952 | ` * a bad third entry leaves the first two applied. Its two ValueErrors are not` |
|       - | 1953 | ` * curl_setopt's and are not each other's: a key that is not an option NUMBER` |
|       - | 1954 | ` * says "must contain only valid cURL options", a key that is not an integer at` |
|       - | 1955 | ` * all says "contains an invalid cURL option". (A numeric STRING key is neither:` |
|       - | 1956 | ` * php's array normalizes it to an int before this ever sees it.)` |
|       - | 1957 | ` */` |
|       - | 1958 | `struct CurlSetoptArray {` |
|       - | 1959 | `	ph7_context *pCtx;` |
|       - | 1960 | `	phl_curl *pCurl;` |
|       - | 1961 | `	int rc;          /* 1 all applied, 0 a false, -1 a throw is installed */` |
|       - | 1962 | `	sxi32 rcOut;     /* the status a throwing coercion wants propagated */` |
|       - | 1963 | `};` |
|      14 | 1964 | `static int CurlSetoptArrayWalk(ph7_value *pKey,ph7_value *pVal,void *pUser)` |
|       1 | 1965 | `{` |
|      15 | 1966 | `	struct CurlSetoptArray *pW = (struct CurlSetoptArray *)pUser;` |
|       - | 1967 | `	int rc;` |
|      15 | 1968 | `	if( pW->rc != 1 ){` |
|     ! 0 | 1969 | `		return PH7_ABORT;` |
|       - | 1970 | `	}` |
|      15 | 1971 | `	if( !ph7_value_is_int(pKey) ){` |
|       3 | 1972 | `		pW->rcOut = PH7_VmThrowException(pW->pCtx,"ValueError",` |
|       - | 1973 | `			"curl_setopt_array(): Argument #2 ($options) contains an invalid cURL option");` |
|       3 | 1974 | `		pW->rc = -1;` |
|       3 | 1975 | `		return PH7_ABORT;` |
|       - | 1976 | `	}` |
|      13 | 1977 | `	if( CurlOptFind(ph7_value_to_int64(pKey)) == 0 ){` |
|       5 | 1978 | `		CurlSetErr(pW->pCurl,CURLE_UNKNOWN_OPTION);` |
|       5 | 1979 | `		pW->rcOut = PH7_VmThrowException(pW->pCtx,"ValueError",` |
|       - | 1980 | `			"curl_setopt_array(): Argument #2 ($options) must contain only valid cURL options");` |
|       5 | 1981 | `		pW->rc = -1;` |
|       5 | 1982 | `		return PH7_ABORT;` |
|       - | 1983 | `	}` |
|       9 | 1984 | `	rc = CurlSetOne(pW->pCtx,pW->pCurl,ph7_value_to_int64(pKey),pVal,"curl_setopt_array",&pW->rcOut);` |
|       9 | 1985 | `	if( rc != 1 ){` |
|     ! 0 | 1986 | `		pW->rc = rc;` |
|     ! 0 | 1987 | `		return PH7_ABORT;` |
|       - | 1988 | `	}` |
|       9 | 1989 | `	return PH7_OK;` |
|       8 | 1990 | `}` |
|      10 | 1991 | `static int vm_builtin_curl_setopt_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1992 | `{` |
|      11 | 1993 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 1994 | `	struct CurlSetoptArray sW;` |
|      11 | 1995 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 1996 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1997 | `		return PH7_OK;` |
|       - | 1998 | `	}` |
|      11 | 1999 | `	CurlClearErr(pCurl);` |
|      11 | 2000 | `	sW.pCtx = pCtx;` |
|      11 | 2001 | `	sW.pCurl = pCurl;` |
|      11 | 2002 | `	sW.rc = 1;` |
|      11 | 2003 | `	sW.rcOut = PH7_OK;` |
|      11 | 2004 | `	ph7_array_walk(apArg[1],CurlSetoptArrayWalk,&sW);` |
|      11 | 2005 | `	ph7_result_bool(pCtx,sW.rc == 1);` |
|      11 | 2006 | `	return sW.rcOut;` |
|       6 | 2007 | `}` |
|       - | 2008 |  |
|       - | 2009 | `/*` |
|       - | 2010 | ` * CurlHandle\|false curl_copy_handle(CurlHandle $handle)` |
|       - | 2011 | ` *` |
|       - | 2012 | `` * The same duphandle `clone` does, through a function -- php's two spellings`` |
|       - | 2013 | ` * of one operation, and they answer alike down to the clean error state on the` |
|       - | 2014 | ` * copy.` |
|       - | 2015 | ` */` |
|       6 | 2016 | `static int vm_builtin_curl_copy_handle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2017 | `{` |
|       7 | 2018 | `	ph7_vm *pVm = pCtx->pVm;` |
|       7 | 2019 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 2020 | `	ph7_class_instance *pThis;` |
|       7 | 2021 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 2022 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2023 | `		return PH7_OK;` |
|       - | 2024 | `	}` |
|       7 | 2025 | `	pThis = CurlNewInstance(pVm,"CurlHandle",sizeof("CurlHandle")-1);` |
|       7 | 2026 | `	if( pThis == 0 ){` |
|     ! 0 | 2027 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2028 | `		return PH7_OK;` |
|       - | 2029 | `	}` |
|       7 | 2030 | `	CurlInstanceClone(pVm,pThis,(ph7_class_instance *)apArg[0]->x.pOther);` |
|       7 | 2031 | `	if( CurlOfInstance(pThis) == 0 ){` |
|       - | 2032 | `		/* the dup failed; hand back php's false rather than an empty handle */` |
|     ! 0 | 2033 | `		PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2034 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2035 | `		return PH7_OK;` |
|       - | 2036 | `	}` |
|       7 | 2037 | `	PH7_NativeResultObject(pCtx,pThis);` |
|       7 | 2038 | `	return PH7_OK;` |
|       4 | 2039 | `}` |
|       - | 2040 | `/* int curl_errno(CurlHandle $handle) */` |
|      64 | 2041 | `static int vm_builtin_curl_errno(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2042 | `{` |
|      65 | 2043 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|      65 | 2044 | `	ph7_result_int(pCtx,pCurl ? pCurl->iLastErr : 0);` |
|      65 | 2045 | `	return PH7_OK;` |
|       1 | 2046 | `}` |
|       - | 2047 | `/*` |
|       - | 2048 | ` * string curl_error(CurlHandle $handle)` |
|       - | 2049 | ` *` |
|       - | 2050 | ` * The ERROR BUFFER, not curl_easy_strerror(): libcurl writes a sentence naming` |
|       - | 2051 | ` * the host and port it could not reach, where the code's own text is the` |
|       - | 2052 | ` * generic "Couldn't connect to server". Empty when nothing has failed.` |
|       - | 2053 | ` */` |
|      18 | 2054 | `static int vm_builtin_curl_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2055 | `{` |
|      19 | 2056 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|      19 | 2057 | `	if( pCurl == 0 ){` |
|     ! 0 | 2058 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 2059 | `		return PH7_OK;` |
|       - | 2060 | `	}` |
|      19 | 2061 | `	ph7_result_string(pCtx,pCurl->zErrBuf,-1);` |
|      19 | 2062 | `	return PH7_OK;` |
|      10 | 2063 | `}` |
|       - | 2064 |  |
|       - | 2065 |  |
|       - | 2066 | `/* ===== curl_getinfo() ===== */` |
|       - | 2067 |  |
|       - | 2068 | `/*` |
|       - | 2069 | ` * A CURLINFO_* carries its own TYPE in the number: libcurl masks the top bits` |
|       - | 2070 | ` * with CURLINFO_TYPEMASK, and every one of the 75 selectors php exposes agrees` |
|       - | 2071 | ` * with its bucket -- string, long, double, slist/pointer, off_t. So the` |
|       - | 2072 | ` * selector form needs no table at all, only the mask, and a selector a newer` |
|       - | 2073 | ` * libcurl adds works the moment its constant exists.` |
|       - | 2074 | ` *` |
|       - | 2075 | ` * The eight names that are NOT infos (CURLINFO_TEXT, HEADER_IN, DATA_OUT and` |
|       - | 2076 | ` * the rest of the DEBUGFUNCTION set, plus CURLINFO_LASTONE) fall outside every` |
|       - | 2077 | ` * bucket, which is exactly why php answers false for them -- the same false an` |
|       - | 2078 | ` * unknown number gets. No throw, ever, for any selector.` |
|       - | 2079 | ` */` |
|      60 | 2080 | `static int CurlInfoOne(ph7_context *pCtx,phl_curl *pCurl,sxi64 iInfo)` |
|       1 | 2081 | `{` |
|      61 | 2082 | `	CURL *pE = pCurl->pEasy;` |
|      61 | 2083 | `	switch( (int)(iInfo & CURLINFO_TYPEMASK) ){` |
|       8 | 2084 | `	case CURLINFO_STRING: {` |
|      17 | 2085 | `		char *zVal = 0;` |
|      17 | 2086 | `		if( curl_easy_getinfo(pE,(CURLINFO)iInfo,&zVal) != CURLE_OK \|\| zVal == 0 ){` |
|       - | 2087 | `			/* libcurl had nothing for it: php's answer is false, not "". */` |
|       5 | 2088 | `			ph7_result_bool(pCtx,0);` |
|       3 | 2089 | `		}else{` |
|      13 | 2090 | `			ph7_result_string(pCtx,zVal,-1);` |
|       - | 2091 | `		}` |
|      17 | 2092 | `		return PH7_OK;` |
|       - | 2093 | `	}` |
|       2 | 2094 | `	case CURLINFO_LONG: {` |
|       5 | 2095 | `		long iVal = 0;` |
|       5 | 2096 | `		if( curl_easy_getinfo(pE,(CURLINFO)iInfo,&iVal) != CURLE_OK ){` |
|     ! 0 | 2097 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2098 | `		}else{` |
|       5 | 2099 | `			ph7_result_int64(pCtx,(sxi64)iVal);` |
|       - | 2100 | `		}` |
|       5 | 2101 | `		return PH7_OK;` |
|       - | 2102 | `	}` |
|       2 | 2103 | `	case CURLINFO_DOUBLE: {` |
|       5 | 2104 | `		double rVal = 0.0;` |
|       5 | 2105 | `		if( curl_easy_getinfo(pE,(CURLINFO)iInfo,&rVal) != CURLE_OK ){` |
|     ! 0 | 2106 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2107 | `		}else{` |
|       5 | 2108 | `			ph7_result_double(pCtx,rVal);` |
|       - | 2109 | `		}` |
|       5 | 2110 | `		return PH7_OK;` |
|       - | 2111 | `	}` |
|       1 | 2112 | `	case CURLINFO_OFF_T: {` |
|       3 | 2113 | `		curl_off_t iVal = 0;` |
|       3 | 2114 | `		if( curl_easy_getinfo(pE,(CURLINFO)iInfo,&iVal) != CURLE_OK ){` |
|     ! 0 | 2115 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2116 | `		}else{` |
|       3 | 2117 | `			ph7_result_int64(pCtx,(sxi64)iVal);` |
|       - | 2118 | `		}` |
|       3 | 2119 | `		return PH7_OK;` |
|       - | 2120 | `	}` |
|       9 | 2121 | `	case CURLINFO_SLIST: {` |
|       - | 2122 | `		/* Two shapes share this bucket. CERTINFO is a struct curl_certinfo,` |
|       - | 2123 | `		 * NOT a curl_slist, so reading it as one would walk the wrong type --` |
|       - | 2124 | `		 * php answers an array of per-certificate arrays for it and a flat` |
|       - | 2125 | `		 * array of strings for the other two. */` |
|      19 | 2126 | `		if( iInfo == CURLINFO_CERTINFO ){` |
|      17 | 2127 | `			struct curl_certinfo *pCi = 0;` |
|      17 | 2128 | `			ph7_value *pOut = ph7_context_new_array(pCtx);` |
|       - | 2129 | `			ph7_value *pRow;` |
|       - | 2130 | `			int i;` |
|      17 | 2131 | `			if( pOut == 0 ){` |
|     ! 0 | 2132 | `				ph7_result_bool(pCtx,0);` |
|     ! 0 | 2133 | `				return PH7_OK;` |
|       - | 2134 | `			}` |
|      17 | 2135 | `			if( curl_easy_getinfo(pE,CURLINFO_CERTINFO,&pCi) == CURLE_OK && pCi ){` |
|      17 | 2136 | `				for( i = 0 ; i < pCi->num_of_certs ; ++i ){` |
|     ! 0 | 2137 | `					struct curl_slist *pWalk = pCi->certinfo[i];` |
|     ! 0 | 2138 | `					pRow = ph7_context_new_array(pCtx);` |
|     ! 0 | 2139 | `					if( pRow == 0 ){ break; }` |
|     ! 0 | 2140 | `					while( pWalk ){` |
|     ! 0 | 2141 | `						ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     ! 0 | 2142 | `						if( pElem == 0 ){ break; }` |
|     ! 0 | 2143 | `						ph7_value_string(pElem,pWalk->data ? pWalk->data : "",-1);` |
|     ! 0 | 2144 | `						ph7_array_add_elem(pRow,0,pElem);` |
|     ! 0 | 2145 | `						ph7_context_release_value(pCtx,pElem);` |
|     ! 0 | 2146 | `						pWalk = pWalk->next;` |
|     ! 0 | 2147 | `					}` |
|     ! 0 | 2148 | `					ph7_array_add_elem(pOut,0,pRow);` |
|     ! 0 | 2149 | `					ph7_context_release_value(pCtx,pRow);` |
|     ! 0 | 2150 | `				}` |
|       8 | 2151 | `			}` |
|      17 | 2152 | `			ph7_result_value(pCtx,pOut);` |
|      17 | 2153 | `			return PH7_OK;` |
|     ! 0 | 2154 | `		}else{` |
|       3 | 2155 | `			struct curl_slist *pList = 0;` |
|       3 | 2156 | `			ph7_value *pOut = ph7_context_new_array(pCtx);` |
|       3 | 2157 | `			if( pOut == 0 ){` |
|     ! 0 | 2158 | `				ph7_result_bool(pCtx,0);` |
|     ! 0 | 2159 | `				return PH7_OK;` |
|       - | 2160 | `			}` |
|       3 | 2161 | `			if( curl_easy_getinfo(pE,(CURLINFO)iInfo,&pList) == CURLE_OK && pList ){` |
|     ! 0 | 2162 | `				struct curl_slist *pWalk = pList;` |
|     ! 0 | 2163 | `				ph7_value *pElem = ph7_context_new_scalar(pCtx);` |
|     ! 0 | 2164 | `				while( pWalk && pElem ){` |
|     ! 0 | 2165 | `					ph7_value_reset_string_cursor(pElem);` |
|     ! 0 | 2166 | `					ph7_value_string(pElem,pWalk->data ? pWalk->data : "",-1);` |
|     ! 0 | 2167 | `					ph7_array_add_elem(pOut,0,pElem);` |
|     ! 0 | 2168 | `					pWalk = pWalk->next;` |
|     ! 0 | 2169 | `				}` |
|     ! 0 | 2170 | `				curl_slist_free_all(pList);` |
|     ! 0 | 2171 | `			}` |
|       3 | 2172 | `			ph7_result_value(pCtx,pOut);` |
|       3 | 2173 | `			return PH7_OK;` |
|       - | 2174 | `		}` |
|       - | 2175 | `	}` |
|       8 | 2176 | `	default:` |
|      16 | 2177 | `		break;` |
|       - | 2178 | `	}` |
|       - | 2179 | `	/* Not an info at all. */` |
|      17 | 2180 | `	ph7_result_bool(pCtx,0);` |
|      17 | 2181 | `	return PH7_OK;` |
|      31 | 2182 | `}` |
|       - | 2183 |  |
|       - | 2184 | `/*` |
|       - | 2185 | ` * The no-selector array: php's 41 entries, in php's order, each from the` |
|       - | 2186 | ` * selector named beside it. Five more exist only past a libcurl version, and` |
|       - | 2187 | ` * php gates them on the HEADER's version; so do these rows, so a build against` |
|       - | 2188 | ` * a newer libcurl answers php's 46. The order is user-visible through print_r and a` |
|       - | 2189 | ` * foreach, and it is not libcurl's.` |
|       - | 2190 | ` *` |
|       - | 2191 | ` * bNullStays is the row that a reading would never produce. Through a SELECTOR` |
|       - | 2192 | ` * every string info libcurl left NULL answers false; in the ARRAY they answer` |
|       - | 2193 | ` * the EMPTY STRING instead -- except content_type, which stays NULL. So the` |
|       - | 2194 | ` * same missing value is rendered three different ways depending on how it was` |
|       - | 2195 | ` * asked for, and only content_type keeps php's null here.` |
|       - | 2196 | ` */` |
|       - | 2197 | `static const struct CurlInfoKey {` |
|       - | 2198 | `	const char *zKey;` |
|       - | 2199 | `	CURLINFO iInfo;` |
|       - | 2200 | `	int bNullStays;` |
|       - | 2201 | `} aCurlInfoKey[] = {` |
|       - | 2202 | `	{ "url",                     CURLINFO_EFFECTIVE_URL,          0 },` |
|       - | 2203 | `	{ "content_type",            CURLINFO_CONTENT_TYPE,           1 },` |
|       - | 2204 | `	{ "http_code",               CURLINFO_RESPONSE_CODE,          0 },` |
|       - | 2205 | `	{ "header_size",             CURLINFO_HEADER_SIZE,            0 },` |
|       - | 2206 | `	{ "request_size",            CURLINFO_REQUEST_SIZE,           0 },` |
|       - | 2207 | `	{ "filetime",                CURLINFO_FILETIME,               0 },` |
|       - | 2208 | `	{ "ssl_verify_result",       CURLINFO_SSL_VERIFYRESULT,       0 },` |
|       - | 2209 | `	{ "redirect_count",          CURLINFO_REDIRECT_COUNT,         0 },` |
|       - | 2210 | `	{ "total_time",              CURLINFO_TOTAL_TIME,             0 },` |
|       - | 2211 | `	{ "namelookup_time",         CURLINFO_NAMELOOKUP_TIME,        0 },` |
|       - | 2212 | `	{ "connect_time",            CURLINFO_CONNECT_TIME,           0 },` |
|       - | 2213 | `	{ "pretransfer_time",        CURLINFO_PRETRANSFER_TIME,       0 },` |
|       - | 2214 | `	{ "size_upload",             CURLINFO_SIZE_UPLOAD,            0 },` |
|       - | 2215 | `	{ "size_download",           CURLINFO_SIZE_DOWNLOAD,          0 },` |
|       - | 2216 | `	{ "speed_download",          CURLINFO_SPEED_DOWNLOAD,         0 },` |
|       - | 2217 | `	{ "speed_upload",            CURLINFO_SPEED_UPLOAD,           0 },` |
|       - | 2218 | `	{ "download_content_length", CURLINFO_CONTENT_LENGTH_DOWNLOAD,0 },` |
|       - | 2219 | `	{ "upload_content_length",   CURLINFO_CONTENT_LENGTH_UPLOAD,  0 },` |
|       - | 2220 | `	{ "starttransfer_time",      CURLINFO_STARTTRANSFER_TIME,     0 },` |
|       - | 2221 | `	{ "redirect_time",           CURLINFO_REDIRECT_TIME,          0 },` |
|       - | 2222 | `	{ "redirect_url",            CURLINFO_REDIRECT_URL,           0 },` |
|       - | 2223 | `	{ "primary_ip",              CURLINFO_PRIMARY_IP,             0 },` |
|       - | 2224 | `	{ "certinfo",                CURLINFO_CERTINFO,               0 },` |
|       - | 2225 | `	{ "primary_port",            CURLINFO_PRIMARY_PORT,           0 },` |
|       - | 2226 | `	{ "local_ip",                CURLINFO_LOCAL_IP,               0 },` |
|       - | 2227 | `	{ "local_port",              CURLINFO_LOCAL_PORT,             0 },` |
|       - | 2228 | `	{ "http_version",            CURLINFO_HTTP_VERSION,           0 },` |
|       - | 2229 | `	{ "protocol",                CURLINFO_PROTOCOL,               0 },` |
|       - | 2230 | `	{ "ssl_verifyresult",        CURLINFO_PROXY_SSL_VERIFYRESULT, 0 },` |
|       - | 2231 | `	{ "scheme",                  CURLINFO_SCHEME,                 0 },` |
|       - | 2232 | `	{ "appconnect_time_us",      CURLINFO_APPCONNECT_TIME_T,      0 },` |
|       - | 2233 | `#if LIBCURL_VERSION_NUM >= 0x080600` |
|       - | 2234 | `	{ "queue_time_us",           CURLINFO_QUEUE_TIME_T,           0 },` |
|       - | 2235 | `#endif` |
|       - | 2236 | `	{ "connect_time_us",         CURLINFO_CONNECT_TIME_T,         0 },` |
|       - | 2237 | `	{ "namelookup_time_us",      CURLINFO_NAMELOOKUP_TIME_T,      0 },` |
|       - | 2238 | `	{ "pretransfer_time_us",     CURLINFO_PRETRANSFER_TIME_T,     0 },` |
|       - | 2239 | `	{ "redirect_time_us",        CURLINFO_REDIRECT_TIME_T,        0 },` |
|       - | 2240 | `	{ "starttransfer_time_us",   CURLINFO_STARTTRANSFER_TIME_T,   0 },` |
|       - | 2241 | `#if LIBCURL_VERSION_NUM >= 0x080a00` |
|       - | 2242 | `	{ "posttransfer_time_us",    CURLINFO_POSTTRANSFER_TIME_T,    0 },` |
|       - | 2243 | `#endif` |
|       - | 2244 | `	{ "total_time_us",           CURLINFO_TOTAL_TIME_T,           0 },` |
|       - | 2245 | `	{ "effective_method",        CURLINFO_EFFECTIVE_METHOD,       0 },` |
|       - | 2246 | `	{ "capath",                  CURLINFO_CAPATH,                 0 },` |
|       - | 2247 | `	{ "cainfo",                  CURLINFO_CAINFO,                 0 },` |
|       - | 2248 | `#if LIBCURL_VERSION_NUM >= 0x080700` |
|       - | 2249 | `	{ "used_proxy",              CURLINFO_USED_PROXY,             0 },` |
|       - | 2250 | `#endif` |
|       - | 2251 | `#if LIBCURL_VERSION_NUM >= 0x080c00` |
|       - | 2252 | `	{ "httpauth_used",           CURLINFO_HTTPAUTH_USED,          0 },` |
|       - | 2253 | `	{ "proxyauth_used",          CURLINFO_PROXYAUTH_USED,         0 },` |
|       - | 2254 | `#endif` |
|       - | 2255 | `	{ "conn_id",                 CURLINFO_CONN_ID,                0 }` |
|       - | 2256 | `};` |
|       - | 2257 |  |
|       - | 2258 | `/* mixed curl_getinfo(CurlHandle $handle, ?int $option = null) */` |
|      60 | 2259 | `static int vm_builtin_curl_getinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2260 | `{` |
|      61 | 2261 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 2262 | `	ph7_value *pArray,*pWorker;` |
|       - | 2263 | `	sxu32 n;` |
|      61 | 2264 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 2265 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2266 | `		return PH7_OK;` |
|       - | 2267 | `	}` |
|      61 | 2268 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      47 | 2269 | `		return CurlInfoOne(pCtx,pCurl,ph7_value_to_int64(apArg[1]));` |
|       - | 2270 | `	}` |
|      15 | 2271 | `	pArray  = ph7_context_new_array(pCtx);` |
|      15 | 2272 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|      15 | 2273 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|     ! 0 | 2274 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2275 | `		return PH7_OK;` |
|       - | 2276 | `	}` |
|     624 | 2277 | `	for( n = 0 ; n < SX_ARRAYSIZE(aCurlInfoKey) ; ++n ){` |
|     610 | 2278 | `		const struct CurlInfoKey *pK = &aCurlInfoKey[n];` |
|     610 | 2279 | `		switch( (int)(pK->iInfo & CURLINFO_TYPEMASK) ){` |
|      63 | 2280 | `		case CURLINFO_STRING: {` |
|     127 | 2281 | `			char *zVal = 0;` |
|     127 | 2282 | `			PH7_MemObjRelease(pWorker);` |
|     127 | 2283 | `			if( curl_easy_getinfo(pCurl->pEasy,pK->iInfo,&zVal) != CURLE_OK \|\| zVal == 0 ){` |
|      57 | 2284 | `				if( pK->bNullStays ){` |
|      15 | 2285 | `					ph7_value_null(pWorker);` |
|       8 | 2286 | `				}else{` |
|      43 | 2287 | `					ph7_value_string(pWorker,"",0);` |
|       - | 2288 | `				}` |
|      36 | 2289 | `			}else{` |
|      71 | 2290 | `				ph7_value_string(pWorker,zVal,-1);` |
|       - | 2291 | `			}` |
|     127 | 2292 | `			break;` |
|       - | 2293 | `		}` |
|      77 | 2294 | `		case CURLINFO_LONG: {` |
|     176 | 2295 | `			long iVal = 0;` |
|     176 | 2296 | `			curl_easy_getinfo(pCurl->pEasy,pK->iInfo,&iVal);` |
|     176 | 2297 | `			PH7_MemObjRelease(pWorker);` |
|     176 | 2298 | `			ph7_value_int64(pWorker,(sxi64)iVal);` |
|     176 | 2299 | `			break;` |
|       - | 2300 | `		}` |
|      84 | 2301 | `		case CURLINFO_DOUBLE: {` |
|     169 | 2302 | `			double rVal = 0.0;` |
|     169 | 2303 | `			curl_easy_getinfo(pCurl->pEasy,pK->iInfo,&rVal);` |
|     169 | 2304 | `			PH7_MemObjRelease(pWorker);` |
|     169 | 2305 | `			ph7_value_double(pWorker,rVal);` |
|     169 | 2306 | `			break;` |
|       - | 2307 | `		}` |
|      56 | 2308 | `		case CURLINFO_OFF_T: {` |
|     127 | 2309 | `			curl_off_t iVal = 0;` |
|     127 | 2310 | `			curl_easy_getinfo(pCurl->pEasy,pK->iInfo,&iVal);` |
|     127 | 2311 | `			PH7_MemObjRelease(pWorker);` |
|     127 | 2312 | `			ph7_value_int64(pWorker,(sxi64)iVal);` |
|     127 | 2313 | `			break;` |
|       - | 2314 | `		}` |
|       7 | 2315 | `		default: {` |
|       - | 2316 | `			/* certinfo, the one array in the table: build it through the` |
|       - | 2317 | `			 * selector path so the two answers cannot drift apart. */` |
|      15 | 2318 | `			ph7_value *pSaved = pCtx->pRet;` |
|       7 | 2319 | `			SXUNUSED(pSaved);` |
|      15 | 2320 | `			CurlInfoOne(pCtx,pCurl,(sxi64)pK->iInfo);` |
|      15 | 2321 | `			PH7_MemObjRelease(pWorker);` |
|      15 | 2322 | `			PH7_MemObjStore(pCtx->pRet,pWorker);` |
|      14 | 2323 | `			break;` |
|       - | 2324 | `		}` |
|       - | 2325 | `		}` |
|     610 | 2326 | `		ph7_array_add_strkey_elem(pArray,pK->zKey,pWorker);` |
|     323 | 2327 | `	}` |
|      15 | 2328 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 2329 | `	return PH7_OK;` |
|      31 | 2330 | `}` |
|       - | 2331 |  |
|       - | 2332 |  |
|       - | 2333 |  |
|       - | 2334 | `/* ===== The callbacks ===== */` |
|       - | 2335 |  |
|       - | 2336 | `/*` |
|       - | 2337 | ` * The rule every callback here shares, and the reason they share one shape:` |
|       - | 2338 | ` * libcurl is in the middle of a transfer when the engine re-enters PHP, and a` |
|       - | 2339 | ` * throw out of that PHP cannot travel back through libcurl's C frames. So the` |
|       - | 2340 | ` * status is PARKED on the handle, libcurl is told to stop by returning a value` |
|       - | 2341 | ` * it reads as a failure, and curl_exec() raises exactly the parked status once` |
|       - | 2342 | ` * the library has unwound. A second callback while one is parked does not` |
|       - | 2343 | ` * re-enter PHP at all.` |
|       - | 2344 | ` *` |
|       - | 2345 | ` * php's own answer is the evidence this is right: a WRITEFUNCTION that throws` |
|       - | 2346 | ` * comes out of curl_exec() as the original exception, and leaves curl_errno()` |
|       - | 2347 | ` * at 0 -- not at the CURLE_WRITE_ERROR the short return would have produced.` |
|       - | 2348 | ` */` |
|      32 | 2349 | `static int CurlCbParked(phl_curl *pCurl)` |
|       1 | 2350 | `{` |
|      33 | 2351 | `	return pCurl->iCbExc != 0;` |
|       1 | 2352 | `}` |
|       2 | 2353 | `static void CurlCbPark(phl_curl *pCurl,sxi32 rc)` |
|       1 | 2354 | `{` |
|       3 | 2355 | `	pCurl->iCbExc = PH7_CALLBACK_UNWOUND(rc) ? rc : PH7_EXCEPTION;` |
|       3 | 2356 | `}` |
|       - | 2357 | `/*` |
|       - | 2358 | ` * Store one callable on the handle. php accepts every callable SHAPE here --` |
|       - | 2359 | ` * a closure, a function name, both array forms, "Class::method", an invokable` |
|       - | 2360 | ` * object -- and null, which puts the default back. Anything else is a TypeError` |
|       - | 2361 | ` * whose tail says which way it was wrong.` |
|       - | 2362 | ` */` |
|      38 | 2363 | `static int CurlSetCallback(ph7_context *pCtx,phl_curl *pCurl,ph7_value **ppSlot,` |
|       - | 2364 | `	ph7_value *pVal,const char *zFunc,const char *zOpt,sxi32 *pRc)` |
|       1 | 2365 | `{` |
|      39 | 2366 | `	ph7_vm *pVm = pCurl->pVm;` |
|      39 | 2367 | `	if( ph7_value_is_null(pVal) ){` |
|       3 | 2368 | `		if( *ppSlot ){` |
|     ! 0 | 2369 | `			ph7_release_value(pVm,*ppSlot);` |
|     ! 0 | 2370 | `			*ppSlot = 0;` |
|     ! 0 | 2371 | `		}` |
|       3 | 2372 | `		return 1;` |
|       - | 2373 | `	}` |
|      37 | 2374 | `	if( !ph7_value_is_string(pVal) && !ph7_value_is_array(pVal) && !ph7_value_is_object(pVal) ){` |
|       4 | 2375 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2376 | `			"%s(): Argument #3 ($value) must be a valid callback for option %s, "` |
|       1 | 2377 | `			"no array or string given",zFunc,zOpt);` |
|       3 | 2378 | `		return -1;` |
|       - | 2379 | `	}` |
|      35 | 2380 | `	if( ph7_value_is_array(pVal) && ph7_array_count(pVal) != 2 ){` |
|       4 | 2381 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2382 | `			"%s(): Argument #3 ($value) must be a valid callback for option %s, "` |
|       1 | 2383 | `			"array callback must have exactly two members",zFunc,zOpt);` |
|       3 | 2384 | `		return -1;` |
|       - | 2385 | `	}` |
|      33 | 2386 | `	if( !PH7_VmIsCallable(pVm,pVal,FALSE) ){` |
|       3 | 2387 | `		if( ph7_value_is_string(pVal) ){` |
|       3 | 2388 | `			int nName = 0;` |
|       3 | 2389 | `			const char *zName = ph7_value_to_string(pVal,&nName);` |
|       4 | 2390 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2391 | `				"%s(): Argument #3 ($value) must be a valid callback for option %s, "` |
|       - | 2392 | `				"function \"%.*s\" not found or invalid function name",` |
|       1 | 2393 | `				zFunc,zOpt,nName,zName);` |
|       2 | 2394 | `		}else{` |
|     ! 0 | 2395 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2396 | `				"%s(): Argument #3 ($value) must be a valid callback for option %s, "` |
|     ! 0 | 2397 | `				"no array or string given",zFunc,zOpt);` |
|       - | 2398 | `		}` |
|       3 | 2399 | `		return -1;` |
|       - | 2400 | `	}` |
|      31 | 2401 | `	if( *ppSlot ){` |
|     ! 0 | 2402 | `		ph7_release_value(pVm,*ppSlot);` |
|     ! 0 | 2403 | `	}` |
|       - | 2404 | `	/* The handle owns its own copy: libcurl may call this long after the` |
|       - | 2405 | `	 * caller's value is gone. */` |
|      31 | 2406 | `	*ppSlot = ph7_new_scalar(pVm);` |
|      31 | 2407 | `	if( *ppSlot == 0 ){` |
|     ! 0 | 2408 | `		return 0;` |
|       - | 2409 | `	}` |
|      31 | 2410 | `	PH7_MemObjStore(pVal,*ppSlot);` |
|      31 | 2411 | `	return 1;` |
|      20 | 2412 | `}` |
|       - | 2413 | `/* The body/header sink: php hands the callback (handle, chunk) and reads the` |
|       - | 2414 | ` * BYTE COUNT back. Anything but the chunk's own length stops the transfer with` |
|       - | 2415 | ` * CURLE_WRITE_ERROR -- 0, -1, true and a non-numeric string alike. */` |
|      14 | 2416 | `static size_t CurlCbWrite(char *zData,size_t nSize,size_t nMemb,void *pUser,` |
|       - | 2417 | `	phl_curl *pCurl,ph7_value *pCb)` |
|       1 | 2418 | `{` |
|      15 | 2419 | `	ph7_vm *pVm = pCurl->pVm;` |
|      15 | 2420 | `	size_t nTotal = nSize * nMemb;` |
|       - | 2421 | `	ph7_value sArgs[2],sRes,*apArg[2];` |
|       - | 2422 | `	sxi32 rc;` |
|       - | 2423 | `	size_t nOut;` |
|       7 | 2424 | `	SXUNUSED(pUser);` |
|      15 | 2425 | `	if( CurlCbParked(pCurl) ){` |
|     ! 0 | 2426 | `		return 0;` |
|       - | 2427 | `	}` |
|      15 | 2428 | `	PH7_MemObjInit(pVm,&sArgs[0]);` |
|      15 | 2429 | `	PH7_MemObjInit(pVm,&sArgs[1]);` |
|      15 | 2430 | `	PH7_MemObjInit(pVm,&sRes);` |
|      15 | 2431 | `	if( pCurl->pOwner ){` |
|      15 | 2432 | `		sArgs[0].x.pOther = pCurl->pOwner;` |
|      15 | 2433 | `		sArgs[0].iFlags = MEMOBJ_OBJ;` |
|      15 | 2434 | `		pCurl->pOwner->iRef++;   /* the argument holds a reference for the call */` |
|       7 | 2435 | `	}` |
|      15 | 2436 | `	ph7_value_string(&sArgs[1],zData,(int)nTotal);` |
|      15 | 2437 | `	apArg[0] = &sArgs[0];` |
|      15 | 2438 | `	apArg[1] = &sArgs[1];` |
|      15 | 2439 | `	rc = PH7_VmCallUserFunction(pVm,pCb,2,apArg,&sRes);` |
|      15 | 2440 | `	if( rc != SXRET_OK ){` |
|       3 | 2441 | `		CurlCbPark(pCurl,rc);` |
|       3 | 2442 | `		nOut = 0;` |
|       2 | 2443 | `	}else{` |
|      13 | 2444 | `		nOut = (size_t)ph7_value_to_int64(&sRes);` |
|       - | 2445 | `	}` |
|      15 | 2446 | `	PH7_MemObjRelease(&sRes);` |
|      15 | 2447 | `	PH7_MemObjRelease(&sArgs[0]);` |
|      15 | 2448 | `	PH7_MemObjRelease(&sArgs[1]);` |
|      15 | 2449 | `	return nOut;` |
|       8 | 2450 | `}` |
|      14 | 2451 | `static size_t CurlWriteThunk(char *zData,size_t nSize,size_t nMemb,void *pUser)` |
|       1 | 2452 | `{` |
|      15 | 2453 | `	phl_curl *pCurl = (phl_curl *)pUser;` |
|      15 | 2454 | `	return CurlCbWrite(zData,nSize,nMemb,pUser,pCurl,pCurl->pWriteCb);` |
|       1 | 2455 | `}` |
|     ! 0 | 2456 | `static size_t CurlHeaderThunk(char *zData,size_t nSize,size_t nMemb,void *pUser)` |
|     ! 0 | 2457 | `{` |
|     ! 0 | 2458 | `	phl_curl *pCurl = (phl_curl *)pUser;` |
|     ! 0 | 2459 | `	return CurlCbWrite(zData,nSize,nMemb,pUser,pCurl,pCurl->pHeaderCb);` |
|     ! 0 | 2460 | `}` |
|       - | 2461 | `/*` |
|       - | 2462 | ` * The progress callback, in php's two spellings. XFERINFOFUNCTION takes the` |
|       - | 2463 | ` * five (handle, dltotal, dlnow, ultotal, ulnow) as INTs; PROGRESSFUNCTION is` |
|       - | 2464 | ` * the same five and the same order -- php passes the modern shape to both.` |
|       - | 2465 | ` * A non-zero return aborts the transfer (CURLE_ABORTED_BY_CALLBACK).` |
|       - | 2466 | ` */` |
|      18 | 2467 | `static int CurlXferThunk(void *pUser,curl_off_t dlTotal,curl_off_t dlNow,` |
|       - | 2468 | `	curl_off_t ulTotal,curl_off_t ulNow)` |
|       1 | 2469 | `{` |
|      19 | 2470 | `	phl_curl *pCurl = (phl_curl *)pUser;` |
|      19 | 2471 | `	ph7_vm *pVm = pCurl->pVm;` |
|       - | 2472 | `	ph7_value sArgs[5],sRes,*apArg[5];` |
|       - | 2473 | `	sxi32 rc;` |
|       - | 2474 | `	int i,iOut;` |
|      19 | 2475 | `	if( CurlCbParked(pCurl) \|\| pCurl->pXferCb == 0 ){` |
|     ! 0 | 2476 | `		return 1;` |
|       - | 2477 | `	}` |
|     109 | 2478 | `	for( i = 0 ; i < 5 ; ++i ){` |
|      91 | 2479 | `		PH7_MemObjInit(pVm,&sArgs[i]);` |
|      91 | 2480 | `		apArg[i] = &sArgs[i];` |
|      46 | 2481 | `	}` |
|      19 | 2482 | `	if( pCurl->pOwner ){` |
|      19 | 2483 | `		sArgs[0].x.pOther = pCurl->pOwner;` |
|      19 | 2484 | `		sArgs[0].iFlags = MEMOBJ_OBJ;` |
|      19 | 2485 | `		pCurl->pOwner->iRef++;   /* the argument holds a reference for the call */` |
|       9 | 2486 | `	}` |
|      19 | 2487 | `	ph7_value_int64(&sArgs[1],(sxi64)dlTotal);` |
|      19 | 2488 | `	ph7_value_int64(&sArgs[2],(sxi64)dlNow);` |
|      19 | 2489 | `	ph7_value_int64(&sArgs[3],(sxi64)ulTotal);` |
|      19 | 2490 | `	ph7_value_int64(&sArgs[4],(sxi64)ulNow);` |
|      19 | 2491 | `	PH7_MemObjInit(pVm,&sRes);` |
|      19 | 2492 | `	rc = PH7_VmCallUserFunction(pVm,pCurl->pXferCb,5,apArg,&sRes);` |
|      19 | 2493 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2494 | `		CurlCbPark(pCurl,rc);` |
|     ! 0 | 2495 | `		iOut = 1;` |
|     ! 0 | 2496 | `	}else{` |
|      19 | 2497 | `		iOut = (int)ph7_value_to_int64(&sRes);` |
|       - | 2498 | `	}` |
|      19 | 2499 | `	PH7_MemObjRelease(&sRes);` |
|     109 | 2500 | `	for( i = 0 ; i < 5 ; ++i ){` |
|      91 | 2501 | `		PH7_MemObjRelease(&sArgs[i]);` |
|      46 | 2502 | `	}` |
|      19 | 2503 | `	return iOut;` |
|      10 | 2504 | `}` |
|       - | 2505 |  |
|       - | 2506 | `/* ===== curl_exec() ===== */` |
|       - | 2507 |  |
|       - | 2508 | `/*` |
|       - | 2509 | ` * Where the body goes. php has two destinations and CURLOPT_RETURNTRANSFER` |
|       - | 2510 | ` * picks between them: a buffer the call ANSWERS, or the script's own output --` |
|       - | 2511 | ` * which has to be the VM's output consumer, not stdout, so an ob_start() around` |
|       - | 2512 | ` * curl_exec() captures it the way php's does.` |
|       - | 2513 | ` */` |
|       - | 2514 | `struct CurlExecSink {` |
|       - | 2515 | `	ph7_context *pCtx;` |
|       - | 2516 | `	SyBlob *pBody;      /* set when RETURNTRANSFER is on */` |
|       - | 2517 | `};` |
|      12 | 2518 | `static size_t CurlExecWrite(char *zData,size_t nSize,size_t nMemb,void *pUser)` |
|       1 | 2519 | `{` |
|      13 | 2520 | `	struct CurlExecSink *pSink = (struct CurlExecSink *)pUser;` |
|      13 | 2521 | `	size_t nTotal = nSize * nMemb;` |
|      13 | 2522 | `	if( nTotal < 1 ){` |
|     ! 0 | 2523 | `		return 0;` |
|       - | 2524 | `	}` |
|      13 | 2525 | `	if( pSink->pBody ){` |
|      11 | 2526 | `		if( SyBlobAppend(pSink->pBody,zData,(sxu32)nTotal) != SXRET_OK ){` |
|     ! 0 | 2527 | `			return 0;   /* short write: libcurl turns this into CURLE_WRITE_ERROR */` |
|       - | 2528 | `		}` |
|       6 | 2529 | `	}else{` |
|       3 | 2530 | `		ph7_context_output(pSink->pCtx,zData,(int)nTotal);` |
|       - | 2531 | `	}` |
|      13 | 2532 | `	return nTotal;` |
|       7 | 2533 | `}` |
|       - | 2534 | `/* string\|bool curl_exec(CurlHandle $handle) */` |
|      40 | 2535 | `static int vm_builtin_curl_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2536 | `{` |
|      41 | 2537 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 2538 | `	struct CurlExecSink sSink;` |
|       - | 2539 | `	SyBlob sBody;` |
|       - | 2540 | `	CURLcode rc;` |
|      41 | 2541 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 2542 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2543 | `		return PH7_OK;` |
|       - | 2544 | `	}` |
|      41 | 2545 | `	SyBlobInit(&sBody,&pCurl->pVm->sAllocator);` |
|      41 | 2546 | `	sSink.pCtx = pCtx;` |
|      41 | 2547 | `	sSink.pBody = pCurl->bReturnTransfer ? &sBody : 0;` |
|      41 | 2548 | `	pCurl->iCbExc = 0;` |
|      41 | 2549 | `	if( pCurl->pWriteCb ){` |
|       - | 2550 | `		/* A php WRITEFUNCTION replaces the destination entirely: neither the` |
|       - | 2551 | `		 * buffer nor the output gets the body. */` |
|      15 | 2552 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEFUNCTION,CurlWriteThunk);` |
|      15 | 2553 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEDATA,(void *)pCurl);` |
|       8 | 2554 | `	}else{` |
|      27 | 2555 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEFUNCTION,CurlExecWrite);` |
|      27 | 2556 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEDATA,(void *)&sSink);` |
|       - | 2557 | `	}` |
|      41 | 2558 | `	if( pCurl->pHeaderCb ){` |
|     ! 0 | 2559 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_HEADERFUNCTION,CurlHeaderThunk);` |
|     ! 0 | 2560 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_HEADERDATA,(void *)pCurl);` |
|     ! 0 | 2561 | `	}` |
|      41 | 2562 | `	if( pCurl->pXferCb ){` |
|       5 | 2563 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_XFERINFOFUNCTION,CurlXferThunk);` |
|       5 | 2564 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_XFERINFODATA,(void *)pCurl);` |
|       2 | 2565 | `	}` |
|      41 | 2566 | `	rc = curl_easy_perform(pCurl->pEasy);` |
|       - | 2567 | `	/* The sink is a stack address: libcurl must not keep it past this call. */` |
|      41 | 2568 | `	curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEFUNCTION,(curl_write_callback)0);` |
|      41 | 2569 | `	curl_easy_setopt(pCurl->pEasy,CURLOPT_WRITEDATA,(void *)0);` |
|      41 | 2570 | `	if( pCurl->pHeaderCb ){` |
|     ! 0 | 2571 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_HEADERFUNCTION,(curl_write_callback)0);` |
|     ! 0 | 2572 | `		curl_easy_setopt(pCurl->pEasy,CURLOPT_HEADERDATA,(void *)0);` |
|     ! 0 | 2573 | `	}` |
|       - | 2574 | `	/*` |
|       - | 2575 | `	 * A callback threw: libcurl has unwound now, so this is where the parked` |
|       - | 2576 | `	 * status is raised -- and php leaves the handle's errno at 0 for it, not` |
|       - | 2577 | `	 * at the CURLE_WRITE_ERROR the stopping return would otherwise have set.` |
|       - | 2578 | `	 */` |
|      41 | 2579 | `	if( pCurl->iCbExc != 0 ){` |
|       3 | 2580 | `		sxi32 rcExc = pCurl->iCbExc;` |
|       3 | 2581 | `		pCurl->iCbExc = 0;` |
|       3 | 2582 | `		pCurl->iLastErr = 0;` |
|       3 | 2583 | `		pCurl->zErrBuf[0] = 0;` |
|       3 | 2584 | `		SyBlobRelease(&sBody);` |
|       3 | 2585 | `		ph7_result_bool(pCtx,0);` |
|       3 | 2586 | `		return rcExc;` |
|       - | 2587 | `	}` |
|      39 | 2588 | `	pCurl->iLastErr = (int)rc;` |
|      39 | 2589 | `	if( rc != CURLE_OK ){` |
|       - | 2590 | `		/* libcurl fills the error buffer itself; when it left it empty (some` |
|       - | 2591 | `		 * codes carry no detail) php still answers the code's own text. */` |
|      25 | 2592 | `		if( pCurl->zErrBuf[0] == 0 ){` |
|     ! 0 | 2593 | `			CurlSetErr(pCurl,(int)rc);` |
|     ! 0 | 2594 | `		}` |
|      25 | 2595 | `		SyBlobRelease(&sBody);` |
|      25 | 2596 | `		ph7_result_bool(pCtx,0);` |
|      25 | 2597 | `		return PH7_OK;` |
|       - | 2598 | `	}` |
|      15 | 2599 | `	if( pCurl->bReturnTransfer ){` |
|      11 | 2600 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody));` |
|       6 | 2601 | `	}else{` |
|       5 | 2602 | `		ph7_result_bool(pCtx,1);` |
|       - | 2603 | `	}` |
|      15 | 2604 | `	SyBlobRelease(&sBody);` |
|      15 | 2605 | `	return PH7_OK;` |
|      21 | 2606 | `}` |
|       - | 2607 |  |
|       - | 2608 |  |
|       - | 2609 | `/* ===== escape / unescape / upkeep / pause ===== */` |
|       - | 2610 |  |
|       - | 2611 | `/*` |
|       - | 2612 | ` * curl_escape / curl_unescape are libcurl's own encoders, called with an` |
|       - | 2613 | ` * explicit LENGTH in both directions -- so they are NUL-clean where a C-string` |
|       - | 2614 | ` * call would not be: escaping "\0" answers "%00" and unescaping "%00" answers` |
|       - | 2615 | ` * the byte back. Neither touches the handle's error state.` |
|       - | 2616 | ` *` |
|       - | 2617 | `` * They are also not urlencode(): `+` is escaped to %2B on the way out and is`` |
|       - | 2618 | ` * NOT decoded to a space on the way back.` |
|       - | 2619 | ` */` |
|      18 | 2620 | `static int vm_builtin_curl_escape(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2621 | `{` |
|      19 | 2622 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 2623 | `	const char *zIn;` |
|      19 | 2624 | `	int nIn = 0;` |
|       - | 2625 | `	char *zOut;` |
|      19 | 2626 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 2627 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2628 | `		return PH7_OK;` |
|       - | 2629 | `	}` |
|      19 | 2630 | `	zIn = ph7_value_to_string(apArg[1],&nIn);` |
|      19 | 2631 | `	zOut = curl_easy_escape(pCurl->pEasy,zIn ? zIn : "",nIn);` |
|      19 | 2632 | `	if( zOut == 0 ){` |
|     ! 0 | 2633 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2634 | `		return PH7_OK;` |
|       - | 2635 | `	}` |
|      19 | 2636 | `	ph7_result_string(pCtx,zOut,-1);` |
|      19 | 2637 | `	curl_free(zOut);` |
|      19 | 2638 | `	return PH7_OK;` |
|      10 | 2639 | `}` |
|      16 | 2640 | `static int vm_builtin_curl_unescape(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2641 | `{` |
|      17 | 2642 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       - | 2643 | `	const char *zIn;` |
|      17 | 2644 | `	int nIn = 0, nOut = 0;` |
|       - | 2645 | `	char *zOut;` |
|      17 | 2646 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 2647 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2648 | `		return PH7_OK;` |
|       - | 2649 | `	}` |
|      17 | 2650 | `	zIn = ph7_value_to_string(apArg[1],&nIn);` |
|      17 | 2651 | `	zOut = curl_easy_unescape(pCurl->pEasy,zIn ? zIn : "",nIn,&nOut);` |
|      17 | 2652 | `	if( zOut == 0 ){` |
|     ! 0 | 2653 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2654 | `		return PH7_OK;` |
|       - | 2655 | `	}` |
|       - | 2656 | `	/* The answer can carry a NUL of its own, so it is taken by LENGTH. */` |
|      17 | 2657 | `	ph7_result_string(pCtx,zOut,nOut);` |
|      17 | 2658 | `	curl_free(zOut);` |
|      17 | 2659 | `	return PH7_OK;` |
|       9 | 2660 | `}` |
|       - | 2661 | `/* bool curl_upkeep(CurlHandle $handle) -- one of the three verbs that CLEAR */` |
|       4 | 2662 | `static int vm_builtin_curl_upkeep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2663 | `{` |
|       5 | 2664 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|       5 | 2665 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 2666 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2667 | `		return PH7_OK;` |
|       - | 2668 | `	}` |
|       5 | 2669 | `	CurlClearErr(pCurl);` |
|       5 | 2670 | `	ph7_result_bool(pCtx,curl_easy_upkeep(pCurl->pEasy) == CURLE_OK);` |
|       5 | 2671 | `	return PH7_OK;` |
|       3 | 2672 | `}` |
|       - | 2673 | `/*` |
|       - | 2674 | ` * int curl_pause(CurlHandle $handle, int $flags)` |
|       - | 2675 | ` *` |
|       - | 2676 | ` * Answers libcurl's CURLcode as an INT rather than a bool -- and leaves the` |
|       - | 2677 | ` * handle's error state alone, so a pause on a handle with no transfer reports` |
|       - | 2678 | ` * 43 (CURLE_BAD_FUNCTION_ARGUMENT) while curl_errno() still says whatever the` |
|       - | 2679 | ` * last transfer said.` |
|       - | 2680 | ` */` |
|      12 | 2681 | `static int vm_builtin_curl_pause(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2682 | `{` |
|      13 | 2683 | `	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);` |
|      13 | 2684 | `	if( pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|     ! 0 | 2685 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2686 | `		return PH7_OK;` |
|       - | 2687 | `	}` |
|      13 | 2688 | `	ph7_result_int(pCtx,(int)curl_easy_pause(pCurl->pEasy,(int)ph7_value_to_int64(apArg[1])));` |
|      13 | 2689 | `	return PH7_OK;` |
|       7 | 2690 | `}` |
|       - | 2691 |  |
|       - | 2692 | `/* ===== Installation ===== */` |
|       - | 2693 |  |
|    5254 | 2694 | `PH7_PRIVATE sxi32 PH7_VmInstallCurl(ph7_vm *pVm)` |
|       5 | 2695 | `{` |
|       - | 2696 | `	static const struct {` |
|       - | 2697 | `		const char *zName;` |
|       - | 2698 | `		ProchHostFunction xFunc;` |
|       - | 2699 | `	} aFunc[] = {` |
|       - | 2700 | `		{ "curl_version",        vm_builtin_curl_version        },` |
|       - | 2701 | `		{ "curl_strerror",       vm_builtin_curl_strerror       },` |
|       - | 2702 | `		{ "curl_multi_strerror", vm_builtin_curl_multi_strerror },` |
|       - | 2703 | `		{ "curl_share_strerror", vm_builtin_curl_share_strerror },` |
|       - | 2704 | `		{ "curl_init",           vm_builtin_curl_init           },` |
|       - | 2705 | `		{ "curl_close",          vm_builtin_curl_close          },` |
|       - | 2706 | `		{ "curl_reset",          vm_builtin_curl_reset          },` |
|       - | 2707 | `		{ "curl_errno",          vm_builtin_curl_errno          },` |
|       - | 2708 | `		{ "curl_error",          vm_builtin_curl_error          },` |
|       - | 2709 | `		{ "curl_copy_handle",    vm_builtin_curl_copy_handle    },` |
|       - | 2710 | `		{ "curl_setopt",         vm_builtin_curl_setopt         },` |
|       - | 2711 | `		{ "curl_setopt_array",   vm_builtin_curl_setopt_array   },` |
|       - | 2712 | `		{ "curl_getinfo",        vm_builtin_curl_getinfo        },` |
|       - | 2713 | `		{ "curl_exec",           vm_builtin_curl_exec           },` |
|       - | 2714 | `		{ "curl_escape",         vm_builtin_curl_escape         },` |
|       - | 2715 | `		{ "curl_unescape",       vm_builtin_curl_unescape       },` |
|       - | 2716 | `		{ "curl_upkeep",         vm_builtin_curl_upkeep         },` |
|       - | 2717 | `		{ "curl_pause",          vm_builtin_curl_pause          }` |
|       - | 2718 | `	};` |
|       - | 2719 | `	/*` |
|       - | 2720 | `	 * The libcurl handle, and nothing else: php's CurlHandle declares no` |
|       - | 2721 | `	 * method, no constant and no property, and prints as an empty object on` |
|       - | 2722 | `	 * every presentation surface. The one slot here is engine storage, hidden` |
|       - | 2723 | `	 * so it appears on none of them.` |
|       - | 2724 | `	 */` |
|       - | 2725 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 2726 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }` |
|       - | 2727 | `	};` |
|       - | 2728 | `	/*` |
|       - | 2729 | ``	 * FINAL (php refuses `class X extends CurlHandle`), NOINSTANTIATE with`` |
|       - | 2730 | `	 * php's own per-class sentence, and NOSERIALIZE -- but NOT NOCLONE, which` |
|       - | 2731 | `	 * is where CurlHandle parts company with every other handle class here:` |
|       - | 2732 | ``	 * php maps clone to curl_easy_duphandle(), so `clone $h` answers a second,`` |
|       - | 2733 | `	 * independent handle. ReflectionClass::isInstantiable() still reports true` |
|       - | 2734 | `	 * for it, which is php's answer too, because the refusal lives in the` |
|       - | 2735 | `	 * creation step rather than in a private constructor.` |
|       - | 2736 | `	 */` |
|       - | 2737 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 2738 | `		"CurlHandle", 0, 0,` |
|       - | 2739 | `		PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOSERIALIZE,` |
|       - | 2740 | `		0, 0, 0, 0,` |
|       - | 2741 | `		aProp, SX_ARRAYSIZE(aProp),` |
|       - | 2742 | `		CurlInstanceRelease, 0, 0` |
|       - | 2743 | `	};` |
|       - | 2744 | `	sxu32 n;` |
|       - | 2745 | `	sxi32 rc;` |
|    5259 | 2746 | `	PH7_CurlGlobalInit();` |
|    5259 | 2747 | `	pVm->pCurlHandles = 0;` |
|   99831 | 2748 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; ++n ){` |
|   94577 | 2749 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|   47291 | 2750 | `	}` |
|    5259 | 2751 | `	rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    5259 | 2752 | `	if( rc == SXRET_OK ){` |
|    5259 | 2753 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"CurlHandle",sizeof("CurlHandle")-1,FALSE,0);` |
|    5259 | 2754 | `		if( pClass ){` |
|    5259 | 2755 | `			pClass->zNewRefusal =` |
|       - | 2756 | `				"Cannot directly construct CurlHandle, use curl_init() instead";` |
|       - | 2757 | ``			/* php's clone_obj: `clone $h` is curl_easy_duphandle(), which is`` |
|       - | 2758 | `			 * why this class alone is not PH7_CLASS_NOCLONE. Stated on the` |
|       - | 2759 | `			 * MOUNTED class, like every other handler hook. */` |
|    5259 | 2760 | `			pClass->xClone = CurlInstanceClone;` |
|    2627 | 2761 | `		}` |
|    2627 | 2762 | `	}` |
|    5259 | 2763 | `	return rc;` |
|       5 | 2764 | `}` |
|       - | 2765 |  |
|       - | 2766 | `#else` |
|       - | 2767 | `/* Ensure non-empty translation unit when curl is disabled (MSVC C4206) */` |
|       - | 2768 | `typedef int vm_curl_unused;` |
|       - | 2769 | `#endif /* PH7_ENABLE_CURL */` |
|       - | 2770 |  |
