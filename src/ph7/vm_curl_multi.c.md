# src/ph7/vm_curl_multi.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 529/578 lines (91.52%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#ifdef PH7_ENABLE_CURL` |
|      - |    6 | `#include "curl_int.h"` |
|      - |    7 |  |
|      - |    8 | `/*` |
|      - |    9 | ` * Section:` |
|      - |   10 | `` *    ext/curl -- the MULTI and SHARE interfaces (php's `curl_multi_*` and`` |
|      - |   11 | `` *    `curl_share_*`).`` |
|      - |   12 | ` * Status:` |
|      - |   13 | ` *    Complete: the multi handle with its option setter, its set of easy` |
|      - |   14 | ` *    handles and the verbs that drive them, plus the two SHARE classes and` |
|      - |   15 | ` *    their five verbs.` |
|      - |   16 | ` *` |
|      - |   17 | ` * WHAT A MULTI HANDLE IS HERE. libcurl's multi interface is a SET of easy` |
|      - |   18 | ` * handles plus a scheduler; php wraps the set in an object and keeps its own` |
|      - |   19 | ` * ordered list of the CurlHandle OBJECTS beside libcurl's, because two of its` |
|      - |   20 | ` * answers are about the objects and not about the library: get_handles() hands` |
|      - |   21 | ` * back the same instances that were added, and the set keeps them ALIVE --` |
|      - |   22 | `` * `unset($h)` after an add leaves the transfer running.`` |
|      - |   23 | ` *` |
|      - |   24 | ` * Every answer below is derived by asking php 8.5 and libcurl 8.5 the same` |
|      - |   25 | ` * question, the way the rest of this binding is.` |
|      - |   26 | ` */` |
|      - |   27 |  |
|      - |   28 | `/* ------------------------------------------------------------------------` |
|      - |   29 | ` * Lifetime` |
|      - |   30 | ` * ------------------------------------------------------------------------ */` |
|     28 |   31 | `static phl_curlm * CurlMultiNew(ph7_vm *pVm)` |
|      2 |   32 | `{` |
|     30 |   33 | `	phl_curlm *pMulti = (phl_curlm *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curlm));` |
|     30 |   34 | `	if( pMulti == 0 ){` |
|    ! 0 |   35 | `		return 0;` |
|      - |   36 | `	}` |
|     30 |   37 | `	SyZero(pMulti,sizeof(phl_curlm));` |
|     30 |   38 | `	pMulti->pVm = pVm;` |
|     30 |   39 | `	pMulti->pMulti = curl_multi_init();` |
|     30 |   40 | `	if( pMulti->pMulti == 0 ){` |
|    ! 0 |   41 | `		SyMemBackendFree(&pVm->sAllocator,pMulti);` |
|    ! 0 |   42 | `		return 0;` |
|      - |   43 | `	}` |
|     30 |   44 | `	pMulti->pNext = (phl_curlm *)pVm->pCurlMultis;` |
|     30 |   45 | `	pVm->pCurlMultis = pMulti;` |
|     30 |   46 | `	return pMulti;` |
|     16 |   47 | `}` |
|      - |   48 | `/*` |
|      - |   49 | ` * Drop one entry: libcurl stops scheduling the transfer, and the reference the` |
|      - |   50 | ` * set held on the object goes with it.` |
|      - |   51 | ` */` |
|     30 |   52 | `static void CurlMultiDropEntry(phl_curlm *pMulti,phl_curlm_ent *pEnt)` |
|      1 |   53 | `{` |
|     31 |   54 | `	phl_curl *pCurl = (phl_curl *)PH7_CurlEasyOfInstance(pEnt->pObj);` |
|     31 |   55 | `	if( pMulti->pMulti && pCurl && pCurl->pEasy ){` |
|     31 |   56 | `		curl_multi_remove_handle(pMulti->pMulti,pCurl->pEasy);` |
|     15 |   57 | `	}` |
|     31 |   58 | `	if( pEnt->pVal ){` |
|     31 |   59 | `		ph7_release_value(pMulti->pVm,pEnt->pVal);` |
|     15 |   60 | `	}` |
|     31 |   61 | `	SyMemBackendFree(&pMulti->pVm->sAllocator,pEnt);` |
|     31 |   62 | `}` |
|     56 |   63 | `static void CurlMultiFree(phl_curlm *pMulti)` |
|      2 |   64 | `{` |
|     58 |   65 | `	phl_curlm_ent *pEnt = pMulti->pHandles;` |
|     84 |   66 | `	while( pEnt ){` |
|     27 |   67 | `		phl_curlm_ent *pNext = pEnt->pNext;` |
|     27 |   68 | `		CurlMultiDropEntry(pMulti,pEnt);` |
|     27 |   69 | `		pEnt = pNext;` |
|      1 |   70 | `	}` |
|     58 |   71 | `	pMulti->pHandles = 0;` |
|     58 |   72 | `	if( pMulti->pMulti ){` |
|      - |   73 | `		/* Every easy handle is out by now: curl_multi_cleanup() on a set that` |
|      - |   74 | `		 * still holds one leaves that handle's connection in a freed cache. */` |
|     30 |   75 | `		curl_multi_cleanup(pMulti->pMulti);` |
|     30 |   76 | `		pMulti->pMulti = 0;` |
|     14 |   77 | `	}` |
|     58 |   78 | `	if( pMulti->pPushCb ){` |
|      3 |   79 | `		ph7_release_value(pMulti->pVm,pMulti->pPushCb);` |
|      3 |   80 | `		pMulti->pPushCb = 0;` |
|      1 |   81 | `	}` |
|     58 |   82 | `}` |
|      - |   83 | `/*` |
|      - |   84 | ` * Free every registered multi. Runs BEFORE the easy-handle sweep (see` |
|      - |   85 | ` * PH7_CurlVmReset), because a live multi still points at the CURL*s it was` |
|      - |   86 | ` * given and curl_multi_remove_handle has to reach them.` |
|      - |   87 | ` */` |
|   6717 |   88 | `PH7_PRIVATE void PH7_CurlMultiVmSweep(ph7_vm *pVm)` |
|      5 |   89 | `{` |
|   6722 |   90 | `	phl_curlm *pMulti = (phl_curlm *)pVm->pCurlMultis;` |
|   6750 |   91 | `	while( pMulti ){` |
|     30 |   92 | `		phl_curlm *pNext = pMulti->pNext;` |
|     30 |   93 | `		PH7_CurlBlankSlot(pMulti->pOwner);` |
|     30 |   94 | `		CurlMultiFree(pMulti);` |
|     30 |   95 | `		SyMemBackendFree(&pVm->sAllocator,pMulti);` |
|     30 |   96 | `		pMulti = pNext;` |
|      2 |   97 | `	}` |
|   6722 |   98 | `	pVm->pCurlMultis = 0;` |
|   6722 |   99 | `}` |
|      - |  100 | `/*` |
|      - |  101 | ` * The object is going away: tear the set down now rather than at VM reset, so` |
|      - |  102 | ` * a script that drops its last reference releases the sockets there. The shell` |
|      - |  103 | ` * stays on the registry (the sweep frees it) because the slot is still` |
|      - |  104 | ` * reachable while the instance is being torn down.` |
|      - |  105 | ` */` |
|     28 |  106 | `static void CurlMultiInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      2 |  107 | `{` |
|     30 |  108 | `	phl_curlm *pMulti = (phl_curlm *)PH7_CurlSlotOf(pThis);` |
|     14 |  109 | `	SXUNUSED(pVm);` |
|     30 |  110 | `	if( pMulti == 0 \|\| pMulti->pOwner != pThis ){` |
|    ! 0 |  111 | `		return;` |
|      - |  112 | `	}` |
|     30 |  113 | `	CurlMultiFree(pMulti);` |
|     30 |  114 | `	pMulti->pOwner = 0;` |
|     16 |  115 | `}` |
|      - |  116 | `/*` |
|      - |  117 | ` * The CurlMultiHandle argument of every verb. The signature table has already` |
|      - |  118 | ` * screened the TYPE, so a miss here means an object the engine tore down.` |
|      - |  119 | ` */` |
|    288 |  120 | `static phl_curlm * CurlMultiArg(int nArg,ph7_value **apArg)` |
|      1 |  121 | `{` |
|      - |  122 | `	ph7_class_instance *pThis;` |
|    289 |  123 | `	pThis = (nArg > 0 && apArg && ph7_value_is_object(apArg[0])) ?` |
|    428 |  124 | `		(ph7_class_instance *)apArg[0]->x.pOther : 0;` |
|    289 |  125 | `	return (phl_curlm *)PH7_CurlSlotOf(pThis);` |
|      1 |  126 | `}` |
|      - |  127 | `/* The CurlHandle argument of add/remove, as an object. */` |
|     56 |  128 | `static ph7_class_instance * CurlEasyArgObj(int nArg,ph7_value **apArg)` |
|      1 |  129 | `{` |
|     57 |  130 | `	if( nArg < 2 \|\| apArg == 0 \|\| !ph7_value_is_object(apArg[1]) ){` |
|    ! 0 |  131 | `		return 0;` |
|      - |  132 | `	}` |
|     57 |  133 | `	return (ph7_class_instance *)apArg[1]->x.pOther;` |
|     29 |  134 | `}` |
|      - |  135 |  |
|      - |  136 | `/* ------------------------------------------------------------------------` |
|      - |  137 | ` * The verbs` |
|      - |  138 | ` * ------------------------------------------------------------------------ */` |
|      - |  139 | `/* CurlMultiHandle curl_multi_init() */` |
|     28 |  140 | `static int vm_builtin_curl_multi_init(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  141 | `{` |
|     30 |  142 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  143 | `	ph7_class_instance *pThis;` |
|      - |  144 | `	phl_curlm *pMulti;` |
|     14 |  145 | `	SXUNUSED(nArg);` |
|     14 |  146 | `	SXUNUSED(apArg);` |
|     30 |  147 | `	pMulti = CurlMultiNew(pVm);` |
|     30 |  148 | `	if( pMulti == 0 ){` |
|    ! 0 |  149 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  150 | `		return PH7_OK;` |
|      - |  151 | `	}` |
|     30 |  152 | `	pThis = PH7_CurlNewInstance(pVm,"CurlMultiHandle",sizeof("CurlMultiHandle")-1);` |
|     30 |  153 | `	if( pThis == 0 \|\| PH7_CurlSlotAttach(pThis,(void *)pMulti) != 0 ){` |
|    ! 0 |  154 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  155 | `		return PH7_OK;` |
|      - |  156 | `	}` |
|     30 |  157 | `	pMulti->pOwner = pThis;` |
|     30 |  158 | `	PH7_NativeResultObject(pCtx,pThis);` |
|     30 |  159 | `	return PH7_OK;` |
|     16 |  160 | `}` |
|      - |  161 | `/*` |
|      - |  162 | ` * void curl_multi_close(CurlMultiHandle $multi_handle)` |
|      - |  163 | ` *` |
|      - |  164 | ` * EMPTIES the set, and does not free it. php 8 turned the resource into an` |
|      - |  165 | ` * object, so the object's own teardown is what frees the CURLM -- but unlike` |
|      - |  166 | ` * curl_close(), which is a pure no-op, this one drops every added handle:` |
|      - |  167 | ` * get_handles() answers the empty array afterwards, and with it go the` |
|      - |  168 | ` * references the set held. The multi then still works, which is how the two` |
|      - |  169 | ` * halves are told apart -- add_handle answers CURLM_OK again, and a second` |
|      - |  170 | ` * close is not an error. Freeing here would make each of those a` |
|      - |  171 | ` * use-after-free on a script php runs happily.` |
|      - |  172 | ` */` |
|      4 |  173 | `static int vm_builtin_curl_multi_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  174 | `{` |
|      5 |  175 | `	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);` |
|      5 |  176 | `	if( pMulti ){` |
|      5 |  177 | `		phl_curlm_ent *pEnt = pMulti->pHandles;` |
|      9 |  178 | `		while( pEnt ){` |
|      5 |  179 | `			phl_curlm_ent *pNext = pEnt->pNext;` |
|      5 |  180 | `			CurlMultiDropEntry(pMulti,pEnt);` |
|      5 |  181 | `			pEnt = pNext;` |
|      1 |  182 | `		}` |
|      5 |  183 | `		pMulti->pHandles = 0;` |
|      2 |  184 | `	}` |
|      5 |  185 | `	ph7_result_null(pCtx);` |
|      5 |  186 | `	return PH7_OK;` |
|      1 |  187 | `}` |
|      - |  188 | `/* int curl_multi_errno(CurlMultiHandle $multi_handle) */` |
|     42 |  189 | `static int vm_builtin_curl_multi_errno(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  190 | `{` |
|     43 |  191 | `	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);` |
|     43 |  192 | `	ph7_result_int(pCtx,pMulti ? pMulti->iLastErr : 0);` |
|     43 |  193 | `	return PH7_OK;` |
|      1 |  194 | `}` |
|      - |  195 | `/* The entry an object already has in this set, or 0. */` |
|     36 |  196 | `static phl_curlm_ent * CurlMultiFind(phl_curlm *pMulti,ph7_class_instance *pObj)` |
|      1 |  197 | `{` |
|     37 |  198 | `	phl_curlm_ent *pEnt = pMulti->pHandles;` |
|     51 |  199 | `	while( pEnt ){` |
|     15 |  200 | `		if( pEnt->pObj == pObj ){` |
|    ! 0 |  201 | `			return pEnt;` |
|      - |  202 | `		}` |
|     15 |  203 | `		pEnt = pEnt->pNext;` |
|      1 |  204 | `	}` |
|     37 |  205 | `	return 0;` |
|     19 |  206 | `}` |
|      - |  207 | `/* Append one entry, holding the object's reference through a php value. */` |
|     36 |  208 | `static int CurlMultiAppend(phl_curlm *pMulti,ph7_value *pObjVal,ph7_class_instance *pObj)` |
|      1 |  209 | `{` |
|     37 |  210 | `	ph7_vm *pVm = pMulti->pVm;` |
|      - |  211 | `	phl_curlm_ent *pEnt,*pTail;` |
|     37 |  212 | `	pEnt = (phl_curlm_ent *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curlm_ent));` |
|     37 |  213 | `	if( pEnt == 0 ){` |
|    ! 0 |  214 | `		return -1;` |
|      - |  215 | `	}` |
|     37 |  216 | `	SyZero(pEnt,sizeof(phl_curlm_ent));` |
|     37 |  217 | `	pEnt->pObj = pObj;` |
|     37 |  218 | `	pEnt->pVal = ph7_new_scalar(pVm);` |
|     37 |  219 | `	if( pEnt->pVal == 0 ){` |
|    ! 0 |  220 | `		SyMemBackendFree(&pVm->sAllocator,pEnt);` |
|    ! 0 |  221 | `		return -1;` |
|      - |  222 | `	}` |
|      - |  223 | `	/* The set OWNS a reference: php's multi keeps the handle alive for as long` |
|      - |  224 | ``	 * as it holds it, so `unset($h)` after an add is not the end of the`` |
|      - |  225 | `	 * transfer. */` |
|     37 |  226 | `	PH7_MemObjStore(pObjVal,pEnt->pVal);` |
|     37 |  227 | `	if( pMulti->pHandles == 0 ){` |
|     27 |  228 | `		pMulti->pHandles = pEnt;` |
|     27 |  229 | `		return 0;` |
|      - |  230 | `	}` |
|     11 |  231 | `	pTail = pMulti->pHandles;` |
|     15 |  232 | `	while( pTail->pNext ){` |
|      5 |  233 | `		pTail = pTail->pNext;` |
|      1 |  234 | `	}` |
|     11 |  235 | `	pTail->pNext = pEnt;` |
|     11 |  236 | `	return 0;` |
|     19 |  237 | `}` |
|      - |  238 | `/*` |
|      - |  239 | ` * int curl_multi_add_handle(CurlMultiHandle $multi_handle, CurlHandle $handle)` |
|      - |  240 | ` *` |
|      - |  241 | ` * The CURLMcode comes from libcurl, and the two failures a script can produce` |
|      - |  242 | ` * are one code: adding a handle this set already holds, and adding one another` |
|      - |  243 | ` * set holds, are both CURLM_ADDED_ALREADY (7). The set is only extended when` |
|      - |  244 | ` * the library accepted the handle, so a refused add leaves get_handles()` |
|      - |  245 | ` * unchanged.` |
|      - |  246 | ` *` |
|      - |  247 | ` * The body BUFFER is reset here, which is php's answer and not libcurl's: a` |
|      - |  248 | ` * handle added for a second transfer answers only what the second one wrote,` |
|      - |  249 | ` * so curl_multi_getcontent() after the re-add and before the exec is the empty` |
|      - |  250 | ` * string rather than the previous body.` |
|      - |  251 | ` */` |
|     46 |  252 | `static int vm_builtin_curl_multi_add_handle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  253 | `{` |
|     47 |  254 | `	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);` |
|     47 |  255 | `	ph7_class_instance *pObj = CurlEasyArgObj(nArg,apArg);` |
|     47 |  256 | `	phl_curl *pCurl = pObj ? (phl_curl *)PH7_CurlEasyOfInstance(pObj) : 0;` |
|      - |  257 | `	CURLMcode rc;` |
|     47 |  258 | `	if( pMulti == 0 \|\| pMulti->pMulti == 0 \|\| pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|    ! 0 |  259 | `		ph7_result_int(pCtx,CURLM_BAD_HANDLE);` |
|    ! 0 |  260 | `		return PH7_OK;` |
|      - |  261 | `	}` |
|     47 |  262 | `	rc = curl_multi_add_handle(pMulti->pMulti,pCurl->pEasy);` |
|     47 |  263 | `	pMulti->iLastErr = (int)rc;` |
|     47 |  264 | `	if( rc == CURLM_OK ){` |
|     37 |  265 | `		PH7_CurlBodyReset(pCurl);` |
|     37 |  266 | `		if( CurlMultiFind(pMulti,pObj) == 0 && CurlMultiAppend(pMulti,apArg[1],pObj) != 0 ){` |
|      - |  267 | `			/* Out of memory building the entry: undo the add rather than leave` |
|      - |  268 | `			 * libcurl scheduling a transfer this set cannot report on. */` |
|    ! 0 |  269 | `			curl_multi_remove_handle(pMulti->pMulti,pCurl->pEasy);` |
|    ! 0 |  270 | `			rc = CURLM_OUT_OF_MEMORY;` |
|    ! 0 |  271 | `			pMulti->iLastErr = (int)rc;` |
|    ! 0 |  272 | `		}` |
|     18 |  273 | `	}` |
|     47 |  274 | `	ph7_result_int(pCtx,(int)rc);` |
|     47 |  275 | `	return PH7_OK;` |
|     24 |  276 | `}` |
|      - |  277 | `/*` |
|      - |  278 | ` * int curl_multi_remove_handle(CurlMultiHandle $multi_handle, CurlHandle $handle)` |
|      - |  279 | ` *` |
|      - |  280 | ` * Removing a handle this set never held is libcurl's CURLM_BAD_EASY_HANDLE (2)` |
|      - |  281 | ` * when ANOTHER set holds it, and CURLM_OK when no set does -- the library's` |
|      - |  282 | ` * distinction, not php's, and the reason the answer is read off libcurl rather` |
|      - |  283 | ` * than off the list here.` |
|      - |  284 | ` */` |
|     10 |  285 | `static int vm_builtin_curl_multi_remove_handle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  286 | `{` |
|     11 |  287 | `	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);` |
|     11 |  288 | `	ph7_class_instance *pObj = CurlEasyArgObj(nArg,apArg);` |
|     11 |  289 | `	phl_curl *pCurl = pObj ? (phl_curl *)PH7_CurlEasyOfInstance(pObj) : 0;` |
|      - |  290 | `	CURLMcode rc;` |
|     11 |  291 | `	if( pMulti == 0 \|\| pMulti->pMulti == 0 \|\| pCurl == 0 \|\| pCurl->pEasy == 0 ){` |
|    ! 0 |  292 | `		ph7_result_int(pCtx,CURLM_BAD_HANDLE);` |
|    ! 0 |  293 | `		return PH7_OK;` |
|      - |  294 | `	}` |
|     11 |  295 | `	rc = curl_multi_remove_handle(pMulti->pMulti,pCurl->pEasy);` |
|     11 |  296 | `	pMulti->iLastErr = (int)rc;` |
|     11 |  297 | `	if( rc == CURLM_OK ){` |
|      9 |  298 | `		phl_curlm_ent *pEnt = pMulti->pHandles,*pPrev = 0;` |
|     15 |  299 | `		while( pEnt ){` |
|     13 |  300 | `			if( pEnt->pObj == pObj ){` |
|      7 |  301 | `				if( pPrev ){` |
|      3 |  302 | `					pPrev->pNext = pEnt->pNext;` |
|      2 |  303 | `				}else{` |
|      5 |  304 | `					pMulti->pHandles = pEnt->pNext;` |
|      - |  305 | `				}` |
|      - |  306 | `				/* Already out of libcurl's set: drop the reference only. */` |
|      7 |  307 | `				if( pEnt->pVal ){` |
|      7 |  308 | `					ph7_release_value(pMulti->pVm,pEnt->pVal);` |
|      3 |  309 | `				}` |
|      7 |  310 | `				SyMemBackendFree(&pMulti->pVm->sAllocator,pEnt);` |
|      7 |  311 | `				break;` |
|      - |  312 | `			}` |
|      7 |  313 | `			pPrev = pEnt;` |
|      7 |  314 | `			pEnt = pEnt->pNext;` |
|      1 |  315 | `		}` |
|      4 |  316 | `	}` |
|     11 |  317 | `	ph7_result_int(pCtx,(int)rc);` |
|     11 |  318 | `	return PH7_OK;` |
|      6 |  319 | `}` |
|      - |  320 | `/*` |
|      - |  321 | ` * array curl_multi_get_handles(CurlMultiHandle $multi_handle)` |
|      - |  322 | ` *` |
|      - |  323 | ` * php's own list, not libcurl's: the answer is the same OBJECTS that were` |
|      - |  324 | ` * added, in the order they were added, re-indexed from 0 -- so a handle` |
|      - |  325 | ` * removed from the middle leaves no hole, and one removed and added again is` |
|      - |  326 | ` * last. It does not touch the error state.` |
|      - |  327 | ` */` |
|     16 |  328 | `static int vm_builtin_curl_multi_get_handles(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  329 | `{` |
|     17 |  330 | `	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);` |
|     17 |  331 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|      - |  332 | `	phl_curlm_ent *pEnt;` |
|     17 |  333 | `	if( pArray == 0 ){` |
|    ! 0 |  334 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  335 | `		return PH7_OK;` |
|      - |  336 | `	}` |
|     39 |  337 | `	for( pEnt = pMulti ? pMulti->pHandles : 0 ; pEnt ; pEnt = pEnt->pNext ){` |
|     23 |  338 | `		ph7_array_add_elem(pArray,0,pEnt->pVal);` |
|     12 |  339 | `	}` |
|     17 |  340 | `	ph7_result_value(pCtx,pArray);` |
|     17 |  341 | `	return PH7_OK;` |
|      9 |  342 | `}` |
|      - |  343 |  |
|      - |  344 | `/* ------------------------------------------------------------------------` |
|      - |  345 | ` * Driving the set` |
|      - |  346 | ` * ------------------------------------------------------------------------ */` |
|      - |  347 | `/*` |
|      - |  348 | ` * int curl_multi_exec(CurlMultiHandle $multi_handle, int &$still_running)` |
|      - |  349 | ` *` |
|      - |  350 | ` * One turn of libcurl's scheduler over every handle in the set, and the count` |
|      - |  351 | ` * of transfers still going written back through the reference. It is the whole` |
|      - |  352 | ` * loop a program writes: exec, select, exec again until nothing runs.` |
|      - |  353 | ` *` |
|      - |  354 | ` * Every handle's handlers are installed before each turn rather than once at` |
|      - |  355 | ` * add time, because a script may point a handle somewhere else between two` |
|      - |  356 | ` * turns and nothing else would notice. The CONTEXT they carry is this call's,` |
|      - |  357 | ` * which is what makes a body with no destination of its own print through the` |
|      - |  358 | ` * VM's output consumer (so an ob_start() around the loop catches it) and what` |
|      - |  359 | ` * a callback's throw is raised on.` |
|      - |  360 | ` *` |
|      - |  361 | ` * A throw is parked per handle by the callback rail and raised HERE, once the` |
|      - |  362 | ` * library has unwound -- the transfer itself carries on, so the set's own` |
|      - |  363 | ` * answer for it is the CURLcode it really ended in.` |
|      - |  364 | ` */` |
|     42 |  365 | `static int vm_builtin_curl_multi_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  366 | `{` |
|     43 |  367 | `	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);` |
|      - |  368 | `	phl_curlm_ent *pEnt;` |
|      - |  369 | `	ph7_value sVal;` |
|     43 |  370 | `	sxi32 rcExc = PH7_OK;` |
|     43 |  371 | `	int nRunning = 0;` |
|     43 |  372 | `	CURLMcode rc = CURLM_BAD_HANDLE;` |
|     43 |  373 | `	if( pMulti && pMulti->pMulti ){` |
|     97 |  374 | `		for( pEnt = pMulti->pHandles ; pEnt ; pEnt = pEnt->pNext ){` |
|     55 |  375 | `			phl_curl *pCurl = (phl_curl *)PH7_CurlEasyOfInstance(pEnt->pObj);` |
|     55 |  376 | `			if( pCurl ){` |
|     55 |  377 | `				PH7_CurlBeginTransfer(pCurl,pCtx);` |
|     30 |  378 | `			}` |
|     31 |  379 | `		}` |
|     43 |  380 | `		rc = curl_multi_perform(pMulti->pMulti,&nRunning);` |
|     97 |  381 | `		for( pEnt = pMulti->pHandles ; pEnt ; pEnt = pEnt->pNext ){` |
|     55 |  382 | `			phl_curl *pCurl = (phl_curl *)PH7_CurlEasyOfInstance(pEnt->pObj);` |
|     55 |  383 | `			if( pCurl == 0 ){` |
|    ! 0 |  384 | `				continue;` |
|      - |  385 | `			}` |
|     55 |  386 | `			PH7_CurlEndTransfer(pCurl);` |
|     55 |  387 | `			if( pCurl->iCbExc != 0 ){` |
|      - |  388 | `				/* The first throw of the turn is the one that travels; the rest` |
|      - |  389 | `				 * are dropped with the same rule a second callback follows. */` |
|      2 |  390 | `				if( rcExc == PH7_OK ){` |
|      2 |  391 | `					rcExc = pCurl->iCbExc;` |
|      1 |  392 | `				}` |
|      2 |  393 | `				pCurl->iCbExc = 0;` |
|      1 |  394 | `			}` |
|     31 |  395 | `		}` |
|     43 |  396 | `		pMulti->iLastErr = (int)rc;` |
|     23 |  397 | `	}` |
|     43 |  398 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sVal,(sxi64)nRunning);` |
|     43 |  399 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],&sVal);` |
|     43 |  400 | `	PH7_MemObjRelease(&sVal);` |
|     43 |  401 | `	ph7_result_int(pCtx,(int)rc);` |
|     43 |  402 | `	return rcExc;` |
|      1 |  403 | `}` |
|      - |  404 | `/*` |
|      - |  405 | ` * int curl_multi_select(CurlMultiHandle $multi_handle, float $timeout = 1.0)` |
|      - |  406 | ` *` |
|      - |  407 | ` * Waits for one of the set's sockets to become readable or writable and` |
|      - |  408 | ` * answers how many did -- or 0 when the timeout ran out first, which is also` |
|      - |  409 | ` * the immediate answer for a set with no socket at all: the wait is libcurl's` |
|      - |  410 | ` * curl_multi_wait(), which does not sleep for a set it has nothing to wait on.` |
|      - |  411 | ` * An error is -1, and the set's error state is NOT touched either way.` |
|      - |  412 | ` *` |
|      - |  413 | ` * The bound on the timeout is php's, not libcurl's: the seconds become an int` |
|      - |  414 | ` * of MILLISECONDS, so anything past INT_MAX of them is refused up front -- and` |
|      - |  415 | ` * so is a NaN, which the comparison below rejects by not being >= 0.` |
|      - |  416 | ` */` |
|     56 |  417 | `static int vm_builtin_curl_multi_select(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  418 | `{` |
|     57 |  419 | `	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);` |
|     57 |  420 | `	double rTimeout = nArg > 1 ? (double)ph7_value_to_double(apArg[1]) : 1.0;` |
|     57 |  421 | `	int nFds = 0;` |
|      - |  422 | `	CURLMcode rc;` |
|     57 |  423 | `	if( !(rTimeout >= 0.0) \|\| rTimeout > (double)SXI32_HIGH / 1000.0 ){` |
|     13 |  424 | `		ph7_result_bool(pCtx,0);` |
|     13 |  425 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  426 | `			"curl_multi_select(): Argument #2 ($timeout) must be between 0 and %f",` |
|      - |  427 | `			(double)SXI32_HIGH / 1000.0);` |
|      - |  428 | `	}` |
|     45 |  429 | `	if( pMulti == 0 \|\| pMulti->pMulti == 0 ){` |
|    ! 0 |  430 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  431 | `		return PH7_OK;` |
|      - |  432 | `	}` |
|     45 |  433 | `	rc = curl_multi_wait(pMulti->pMulti,0,0,(int)(rTimeout * 1000.0),&nFds);` |
|     45 |  434 | `	ph7_result_int(pCtx,rc == CURLM_OK ? nFds : -1);` |
|     45 |  435 | `	return PH7_OK;` |
|     31 |  436 | `}` |
|      - |  437 | `/*` |
|      - |  438 | ` * array\|false curl_multi_info_read(CurlMultiHandle $multi_handle, int &$queued_messages = null)` |
|      - |  439 | ` *` |
|      - |  440 | ` * One message off libcurl's queue -- always a CURLMSG_DONE -- as php's three` |
|      - |  441 | ` * keys in php's order, with the CurlHandle OBJECT that was added rather than` |
|      - |  442 | ` * the CURL* the message names. The count still queued is written back only` |
|      - |  443 | ` * when there WAS a message: an empty queue answers false and leaves the` |
|      - |  444 | ` * reference exactly as the caller left it.` |
|      - |  445 | ` *` |
|      - |  446 | ` * This is also the verb that gives the easy handle its error state. Until the` |
|      - |  447 | ` * message is read, curl_errno() on a handle whose multi transfer already` |
|      - |  448 | ` * finished still answers 0 -- the transfer reported to the SET, and reading the` |
|      - |  449 | ` * message is what moves the result onto the handle. libcurl wrote the detailed` |
|      - |  450 | ` * text into the handle's error buffer during the transfer; a code with no` |
|      - |  451 | ` * detail falls back to its own sentence, the same as the easy rail.` |
|      - |  452 | ` */` |
|     20 |  453 | `static int vm_builtin_curl_multi_info_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  454 | `{` |
|     21 |  455 | `	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);` |
|      - |  456 | `	ph7_value *pArray,*pVal;` |
|      - |  457 | `	phl_curlm_ent *pEnt;` |
|      - |  458 | `	CURLMsg *pMsg;` |
|     21 |  459 | `	int nQueued = 0;` |
|     21 |  460 | `	if( pMulti == 0 \|\| pMulti->pMulti == 0 ){` |
|    ! 0 |  461 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  462 | `		return PH7_OK;` |
|      - |  463 | `	}` |
|     21 |  464 | `	pMsg = curl_multi_info_read(pMulti->pMulti,&nQueued);` |
|     21 |  465 | `	if( pMsg == 0 ){` |
|     13 |  466 | `		ph7_result_bool(pCtx,0);` |
|     13 |  467 | `		return PH7_OK;` |
|      - |  468 | `	}` |
|      8 |  469 | `	pArray = ph7_context_new_array(pCtx);` |
|      8 |  470 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      8 |  471 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  472 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  473 | `		return PH7_OK;` |
|      - |  474 | `	}` |
|      8 |  475 | `	ph7_value_int64(pVal,(sxi64)pMsg->msg);` |
|      8 |  476 | `	ph7_array_add_strkey_elem(pArray,"msg",pVal);` |
|      8 |  477 | `	ph7_value_int64(pVal,(sxi64)pMsg->data.result);` |
|      8 |  478 | `	ph7_array_add_strkey_elem(pArray,"result",pVal);` |
|     10 |  479 | `	for( pEnt = pMulti->pHandles ; pEnt ; pEnt = pEnt->pNext ){` |
|     10 |  480 | `		phl_curl *pCurl = (phl_curl *)PH7_CurlEasyOfInstance(pEnt->pObj);` |
|     10 |  481 | `		if( pCurl == 0 \|\| pCurl->pEasy != pMsg->easy_handle ){` |
|      2 |  482 | `			continue;` |
|      - |  483 | `		}` |
|      8 |  484 | `		PH7_CurlRecordResult(pCurl,(int)pMsg->data.result);` |
|      8 |  485 | `		ph7_array_add_strkey_elem(pArray,"handle",pEnt->pVal);` |
|      8 |  486 | `		break;` |
|    ! 0 |  487 | `	}` |
|      8 |  488 | `	if( nArg > 1 ){` |
|      4 |  489 | `		ph7_value_int64(pVal,(sxi64)nQueued);` |
|      4 |  490 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pVal);` |
|      2 |  491 | `	}` |
|      8 |  492 | `	ph7_result_value(pCtx,pArray);` |
|      8 |  493 | `	return PH7_OK;` |
|     11 |  494 | `}` |
|      - |  495 | `/*` |
|      - |  496 | ` * ?string curl_multi_getcontent(CurlHandle $handle)` |
|      - |  497 | ` *` |
|      - |  498 | ` * The body a RETURNTRANSFER handle collected -- the only way to reach it after` |
|      - |  499 | ` * a multi transfer, which has no call to answer it. It reads the DESTINATION` |
|      - |  500 | ` * that stands now rather than what the last transfer did, so a handle whose` |
|      - |  501 | ` * body went to the output, to a callback or to a stream answers null, and one` |
|      - |  502 | ` * that was reset after collecting a body answers null too. The verb belongs to` |
|      - |  503 | ` * the easy handle: it is the same buffer a plain curl_exec() answers, and` |
|      - |  504 | ` * asking twice answers twice.` |
|      - |  505 | ` */` |
|     30 |  506 | `static int vm_builtin_curl_multi_getcontent(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  507 | `{` |
|     31 |  508 | `	ph7_class_instance *pThis = (nArg > 0 && apArg && ph7_value_is_object(apArg[0])) ?` |
|     45 |  509 | `		(ph7_class_instance *)apArg[0]->x.pOther : 0;` |
|     31 |  510 | `	phl_curl *pCurl = pThis ? (phl_curl *)PH7_CurlEasyOfInstance(pThis) : 0;` |
|     31 |  511 | `	if( pCurl == 0 \|\| pCurl->iWriteDest != PHL_CURL_DEST_RETURN ){` |
|     11 |  512 | `		ph7_result_null(pCtx);` |
|     11 |  513 | `		return PH7_OK;` |
|      - |  514 | `	}` |
|     21 |  515 | `	PH7_CurlResultBody(pCtx,pCurl);` |
|     21 |  516 | `	return PH7_OK;` |
|     16 |  517 | `}` |
|      - |  518 |  |
|      - |  519 | `/* ------------------------------------------------------------------------` |
|      - |  520 | ` * curl_multi_setopt()` |
|      - |  521 | ` * ------------------------------------------------------------------------ */` |
|      - |  522 | `/*` |
|      - |  523 | ` * php's nine multi options, in two kinds. Everything but the push callback is a` |
|      - |  524 | ` * long libcurl reads, and php converts whatever it was given -- a string, an` |
|      - |  525 | ` * array, null -- with the ordinary int cast rather than refusing it.` |
|      - |  526 | ` *` |
|      - |  527 | ` * An option php does not know is a ValueError, and the handle's error state` |
|      - |  528 | ` * moves with it: curl_multi_errno() answers CURLM_UNKNOWN_OPTION (6) after the` |
|      - |  529 | ` * throw, a code php declares no constant for.` |
|      - |  530 | ` */` |
|      - |  531 | `#define PHL_CURLM_OPT_LONG 0` |
|      - |  532 | `#define PHL_CURLM_OPT_PUSH 1` |
|      - |  533 | `static const struct CurlMultiOptDef {` |
|      - |  534 | `	int iOpt;` |
|      - |  535 | `	int iKind;` |
|      - |  536 | `	const char *zName;` |
|      - |  537 | `} aCurlMultiOpt[] = {` |
|      - |  538 | `	{ CURLMOPT_PIPELINING,                  PHL_CURLM_OPT_LONG, "CURLMOPT_PIPELINING" },` |
|      - |  539 | `	{ CURLMOPT_MAXCONNECTS,                 PHL_CURLM_OPT_LONG, "CURLMOPT_MAXCONNECTS" },` |
|      - |  540 | `	{ CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE,   PHL_CURLM_OPT_LONG, "CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE" },` |
|      - |  541 | `	{ CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE, PHL_CURLM_OPT_LONG, "CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE" },` |
|      - |  542 | `	{ CURLMOPT_MAX_HOST_CONNECTIONS,        PHL_CURLM_OPT_LONG, "CURLMOPT_MAX_HOST_CONNECTIONS" },` |
|      - |  543 | `	{ CURLMOPT_MAX_PIPELINE_LENGTH,         PHL_CURLM_OPT_LONG, "CURLMOPT_MAX_PIPELINE_LENGTH" },` |
|      - |  544 | `	{ CURLMOPT_MAX_TOTAL_CONNECTIONS,       PHL_CURLM_OPT_LONG, "CURLMOPT_MAX_TOTAL_CONNECTIONS" },` |
|      - |  545 | `	{ CURLMOPT_MAX_CONCURRENT_STREAMS,      PHL_CURLM_OPT_LONG, "CURLMOPT_MAX_CONCURRENT_STREAMS" },` |
|      - |  546 | `	{ CURLMOPT_PUSHFUNCTION,                PHL_CURLM_OPT_PUSH, "CURLMOPT_PUSHFUNCTION" }` |
|      - |  547 | `};` |
|     52 |  548 | `static const struct CurlMultiOptDef * CurlMultiOptFind(sxi64 iOpt)` |
|      1 |  549 | `{` |
|      - |  550 | `	sxu32 n;` |
|    269 |  551 | `	for( n = 0 ; n < SX_ARRAYSIZE(aCurlMultiOpt) ; ++n ){` |
|    267 |  552 | `		if( (sxi64)aCurlMultiOpt[n].iOpt == iOpt ){` |
|     51 |  553 | `			return &aCurlMultiOpt[n];` |
|      - |  554 | `		}` |
|    109 |  555 | `	}` |
|      3 |  556 | `	return 0;` |
|     27 |  557 | `}` |
|      - |  558 | `/*` |
|      - |  559 | ` * bool curl_multi_setopt(CurlMultiHandle $multi_handle, int $option, mixed $value)` |
|      - |  560 | ` *` |
|      - |  561 | ` * CURLMOPT_PIPELINING carries a diagnostic of php's own: libcurl dropped HTTP/1` |
|      - |  562 | `` * pipelining, so CURLPIPE_HTTP1 (which is what a plain `true` casts to) is a`` |
|      - |  563 | ` * WARNING naming the constant, and the call still answers true.` |
|      - |  564 | ` */` |
|     52 |  565 | `static int vm_builtin_curl_multi_setopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  566 | `{` |
|     53 |  567 | `	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);` |
|      - |  568 | `	const struct CurlMultiOptDef *pDef;` |
|      - |  569 | `	sxi64 iOpt;` |
|      - |  570 | `	CURLMcode rc;` |
|     53 |  571 | `	if( pMulti == 0 \|\| pMulti->pMulti == 0 ){` |
|    ! 0 |  572 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  573 | `		return PH7_OK;` |
|      - |  574 | `	}` |
|     53 |  575 | `	iOpt = ph7_value_to_int64(apArg[1]);` |
|     53 |  576 | `	pDef = CurlMultiOptFind(iOpt);` |
|     53 |  577 | `	if( pDef == 0 ){` |
|      3 |  578 | `		pMulti->iLastErr = (int)CURLM_UNKNOWN_OPTION;` |
|      3 |  579 | `		ph7_result_bool(pCtx,0);` |
|      3 |  580 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  581 | `			"curl_multi_setopt(): Argument #2 ($option) is not a valid cURL multi option");` |
|      - |  582 | `	}` |
|     51 |  583 | `	if( pDef->iKind == PHL_CURLM_OPT_PUSH ){` |
|     17 |  584 | `		sxi32 rcThrow = PH7_OK;` |
|     25 |  585 | `		int rcCb = PH7_CurlSetCallback(pCtx,pMulti->pVm,&pMulti->pPushCb,apArg[2],` |
|     16 |  586 | `			"curl_multi_setopt","#2 ($option)",pDef->zName,FALSE,&rcThrow);` |
|     17 |  587 | `		if( rcCb < 0 ){` |
|     13 |  588 | `			ph7_result_bool(pCtx,0);` |
|     13 |  589 | `			return rcThrow;` |
|      - |  590 | `		}` |
|      5 |  591 | `		if( rcCb == 0 ){` |
|    ! 0 |  592 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  593 | `			return PH7_OK;` |
|      - |  594 | `		}` |
|      - |  595 | `		/*` |
|      - |  596 | `		 * The callable is validated and retained -- php's whole setopt answer` |
|      - |  597 | `		 * for this option, TypeErrors included -- and no push callback is` |
|      - |  598 | `		 * installed on the library. libcurl's default with none is to DENY` |
|      - |  599 | `		 * every pushed stream, which is what a php callback answering` |
|      - |  600 | `		 * CURL_PUSH_DENY produces, so the only script this differs for is one` |
|      - |  601 | `		 * whose callback would have said CURL_PUSH_OK. Calling it needs a php` |
|      - |  602 | `		 * CurlHandle over an easy handle LIBCURL owns and hands out mid-push,` |
|      - |  603 | `		 * and only an HTTP/2 peer that actually pushes can derive what that` |
|      - |  604 | `		 * ownership is -- no corpus here has one. It is recorded.` |
|      - |  605 | `		 */` |
|      5 |  606 | `		ph7_result_bool(pCtx,1);` |
|      5 |  607 | `		return PH7_OK;` |
|      - |  608 | `	}` |
|     35 |  609 | `	if( iOpt == CURLMOPT_PIPELINING && ph7_value_to_int64(apArg[2]) == CURLPIPE_HTTP1 ){` |
|      3 |  610 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|      - |  611 | `			"CURLPIPE_HTTP1 is no longer supported");` |
|      1 |  612 | `	}` |
|     35 |  613 | `	rc = curl_multi_setopt(pMulti->pMulti,(CURLMoption)pDef->iOpt,` |
|      - |  614 | `		(long)ph7_value_to_int64(apArg[2]));` |
|     35 |  615 | `	pMulti->iLastErr = (int)rc;` |
|     35 |  616 | `	ph7_result_bool(pCtx,rc == CURLM_OK);` |
|     35 |  617 | `	return PH7_OK;` |
|     27 |  618 | `}` |
|      - |  619 |  |
|      - |  620 | `/* ------------------------------------------------------------------------` |
|      - |  621 | ` * The SHARE surface` |
|      - |  622 | ` * ------------------------------------------------------------------------ */` |
|      - |  623 | `/*` |
|      - |  624 | ` * A share handle is a cache several easy handles read and write together --` |
|      - |  625 | ` * php has two classes over it, and they are NOT related by inheritance:` |
|      - |  626 | ` * curl_share_close() and curl_share_setopt() take a CurlShareHandle and refuse` |
|      - |  627 | ` * a CurlSharePersistentHandle by type, which is how php keeps a process-wide` |
|      - |  628 | ` * cache out of the reach of the verbs that would reconfigure it.` |
|      - |  629 | ` */` |
|      - |  630 | `static const char * const CurlShareClass = "CurlShareHandle";` |
|      - |  631 | `static const char * const CurlSharePersistClass = "CurlSharePersistentHandle";` |
|      - |  632 |  |
|      - |  633 | `/*` |
|      - |  634 | ` * One record. pBorrow is the cache a PERSISTENT record joins rather than` |
|      - |  635 | ` * creates: its object is its own (php's two persistent handles are never` |
|      - |  636 | ` * identical, and never equal either), and the CURLSH under them is one.` |
|      - |  637 | ` */` |
|     24 |  638 | `static phl_curlsh * CurlShareNew(ph7_vm *pVm,CURLSH *pBorrow)` |
|      2 |  639 | `{` |
|     26 |  640 | `	phl_curlsh *pSh = (phl_curlsh *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curlsh));` |
|     26 |  641 | `	if( pSh == 0 ){` |
|    ! 0 |  642 | `		return 0;` |
|      - |  643 | `	}` |
|     26 |  644 | `	SyZero(pSh,sizeof(phl_curlsh));` |
|     26 |  645 | `	pSh->pVm = pVm;` |
|     26 |  646 | `	pSh->pShare = pBorrow ? pBorrow : curl_share_init();` |
|     26 |  647 | `	pSh->bOwnsShare = pBorrow == 0;` |
|     26 |  648 | `	if( pSh->pShare == 0 ){` |
|    ! 0 |  649 | `		SyMemBackendFree(&pVm->sAllocator,pSh);` |
|    ! 0 |  650 | `		return 0;` |
|      - |  651 | `	}` |
|     26 |  652 | `	pSh->pNext = (phl_curlsh *)pVm->pCurlShares;` |
|     26 |  653 | `	pVm->pCurlShares = pSh;` |
|     26 |  654 | `	return pSh;` |
|     14 |  655 | `}` |
|      - |  656 | `/*` |
|      - |  657 | ` * Free every registered share. Runs LAST (see PH7_CurlVmReset): libcurl` |
|      - |  658 | ` * refuses to clean up a share an easy handle is still attached to, so every` |
|      - |  659 | ` * CURL* has to have been cleaned up first or this leaks the cache.` |
|      - |  660 | ` */` |
|   6717 |  661 | `PH7_PRIVATE void PH7_CurlShareVmSweep(ph7_vm *pVm)` |
|      5 |  662 | `{` |
|   6722 |  663 | `	phl_curlsh *pSh = (phl_curlsh *)pVm->pCurlShares;` |
|   6746 |  664 | `	while( pSh ){` |
|     26 |  665 | `		phl_curlsh *pNext = pSh->pNext;` |
|     26 |  666 | `		PH7_CurlBlankSlot(pSh->pOwner);` |
|     26 |  667 | `		if( pSh->pShare && pSh->bOwnsShare ){` |
|      9 |  668 | `			curl_share_cleanup(pSh->pShare);` |
|      4 |  669 | `		}` |
|     26 |  670 | `		SyMemBackendFree(&pVm->sAllocator,pSh);` |
|     26 |  671 | `		pSh = pNext;` |
|      2 |  672 | `	}` |
|   6722 |  673 | `	pVm->pCurlShares = 0;` |
|   6722 |  674 | `}` |
|      - |  675 | `/*` |
|      - |  676 | ` * The object is going away. A PERSISTENT share is NOT torn down here: it is` |
|      - |  677 | ` * the point of the class that it outlives the objects handed out for it, and` |
|      - |  678 | ` * the sweep is what closes it. An ordinary one is closed with its last object,` |
|      - |  679 | ` * the way every other handle class here is -- unless a live transfer still` |
|      - |  680 | ` * names it, which libcurl refuses to cleanup and which the sweep will pick up` |
|      - |  681 | ` * once the easy handles are gone.` |
|      - |  682 | ` */` |
|     24 |  683 | `static void CurlShareInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      2 |  684 | `{` |
|     26 |  685 | `	phl_curlsh *pSh = (phl_curlsh *)PH7_CurlSlotOf(pThis);` |
|     12 |  686 | `	SXUNUSED(pVm);` |
|     26 |  687 | `	if( pSh == 0 \|\| pSh->pOwner != pThis ){` |
|    ! 0 |  688 | `		return;` |
|      - |  689 | `	}` |
|      - |  690 | `	/* The instance is being torn down: the record must stop naming it whatever` |
|      - |  691 | `	 * happens next, or the sweep blanks a slot in freed memory. */` |
|     26 |  692 | `	pSh->pOwner = 0;` |
|     26 |  693 | `	if( pSh->bPersistent \|\| !pSh->bOwnsShare ){` |
|     17 |  694 | `		return;` |
|      - |  695 | `	}` |
|     10 |  696 | `	if( pSh->pShare && curl_share_cleanup(pSh->pShare) == CURLSHE_OK ){` |
|      - |  697 | `		/* CURLSHE_IN_USE says a transfer still names it: leave it to the sweep,` |
|      - |  698 | `		 * which runs after every easy handle is gone. */` |
|      8 |  699 | `		pSh->pShare = 0;` |
|      3 |  700 | `	}` |
|     14 |  701 | `}` |
|      - |  702 | `/* The record behind a share argument of either class. */` |
|     68 |  703 | `static phl_curlsh * CurlShareArg(int nArg,ph7_value **apArg)` |
|      1 |  704 | `{` |
|      - |  705 | `	ph7_class_instance *pThis;` |
|     69 |  706 | `	pThis = (nArg > 0 && apArg && ph7_value_is_object(apArg[0])) ?` |
|    102 |  707 | `		(ph7_class_instance *)apArg[0]->x.pOther : 0;` |
|     69 |  708 | `	return (phl_curlsh *)PH7_CurlSlotOf(pThis);` |
|      1 |  709 | `}` |
|      - |  710 | `/* Hand back a new object over one record. */` |
|      8 |  711 | `static int CurlShareResult(ph7_context *pCtx,phl_curlsh *pSh,const char *zClass)` |
|      2 |  712 | `{` |
|     10 |  713 | `	ph7_class_instance *pThis = PH7_CurlNewInstance(pCtx->pVm,zClass,(int)SyStrlen(zClass));` |
|     10 |  714 | `	if( pThis == 0 \|\| PH7_CurlSlotAttach(pThis,(void *)pSh) != 0 ){` |
|    ! 0 |  715 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  716 | `		return PH7_OK;` |
|      - |  717 | `	}` |
|     10 |  718 | `	if( pSh->pOwner == 0 ){` |
|     10 |  719 | `		pSh->pOwner = pThis;` |
|      4 |  720 | `	}` |
|     10 |  721 | `	PH7_NativeResultObject(pCtx,pThis);` |
|     10 |  722 | `	return PH7_OK;` |
|      6 |  723 | `}` |
|      - |  724 | `/* CurlShareHandle curl_share_init() */` |
|      8 |  725 | `static int vm_builtin_curl_share_init(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  726 | `{` |
|      - |  727 | `	phl_curlsh *pSh;` |
|      4 |  728 | `	SXUNUSED(nArg);` |
|      4 |  729 | `	SXUNUSED(apArg);` |
|     10 |  730 | `	pSh = CurlShareNew(pCtx->pVm,0);` |
|     10 |  731 | `	if( pSh == 0 ){` |
|    ! 0 |  732 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  733 | `		return PH7_OK;` |
|      - |  734 | `	}` |
|     10 |  735 | `	return CurlShareResult(pCtx,pSh,CurlShareClass);` |
|      6 |  736 | `}` |
|      - |  737 | `/*` |
|      - |  738 | ` * void curl_share_close(CurlShareHandle $share_handle)` |
|      - |  739 | ` *` |
|      - |  740 | ` * A NO-OP, the same finding curl_close() carries: the object's own teardown is` |
|      - |  741 | ` * what frees the cache, so a closed share still takes options and still shares` |
|      - |  742 | ` * -- and it does not clear the error state either.` |
|      - |  743 | ` */` |
|      4 |  744 | `static int vm_builtin_curl_share_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  745 | `{` |
|      2 |  746 | `	SXUNUSED(nArg);` |
|      2 |  747 | `	SXUNUSED(apArg);` |
|      5 |  748 | `	ph7_result_null(pCtx);` |
|      5 |  749 | `	return PH7_OK;` |
|      1 |  750 | `}` |
|      - |  751 | `/* int curl_share_errno(CurlShareHandle $share_handle) */` |
|     34 |  752 | `static int vm_builtin_curl_share_errno(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  753 | `{` |
|     35 |  754 | `	phl_curlsh *pSh = CurlShareArg(nArg,apArg);` |
|     35 |  755 | `	ph7_result_int(pCtx,pSh ? pSh->iLastErr : 0);` |
|     35 |  756 | `	return PH7_OK;` |
|      1 |  757 | `}` |
|      - |  758 | `/*` |
|      - |  759 | ` * bool curl_share_setopt(CurlShareHandle $share_handle, int $option, mixed $value)` |
|      - |  760 | ` *` |
|      - |  761 | ` * Two options, and the VALUE is libcurl's to judge: php casts whatever it was` |
|      - |  762 | `` * given to a long and hands it over, so `"3"` and `3.7` are both`` |
|      - |  763 | ` * CURL_LOCK_DATA_DNS while a null, a true and an out-of-range number are the` |
|      - |  764 | ` * library's CURLSHE_BAD_OPTION -- false, with the code left on the handle. An` |
|      - |  765 | ` * option php does not know is a ValueError, and it leaves the same code` |
|      - |  766 | ` * behind, so the throw and the refusal are not alternatives.` |
|      - |  767 | ` */` |
|     34 |  768 | `static int vm_builtin_curl_share_setopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  769 | `{` |
|     35 |  770 | `	phl_curlsh *pSh = CurlShareArg(nArg,apArg);` |
|      - |  771 | `	sxi64 iOpt;` |
|      - |  772 | `	CURLSHcode rc;` |
|     35 |  773 | `	if( pSh == 0 \|\| pSh->pShare == 0 ){` |
|    ! 0 |  774 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  775 | `		return PH7_OK;` |
|      - |  776 | `	}` |
|     35 |  777 | `	iOpt = ph7_value_to_int64(apArg[1]);` |
|     35 |  778 | `	if( iOpt != CURLSHOPT_SHARE && iOpt != CURLSHOPT_UNSHARE ){` |
|      5 |  779 | `		pSh->iLastErr = (int)CURLSHE_BAD_OPTION;` |
|      5 |  780 | `		ph7_result_bool(pCtx,0);` |
|      5 |  781 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  782 | `			"curl_share_setopt(): Argument #2 ($option) is not a valid cURL share option");` |
|      - |  783 | `	}` |
|     31 |  784 | `	rc = curl_share_setopt(pSh->pShare,(CURLSHoption)iOpt,` |
|      - |  785 | `		(long)ph7_value_to_int64(apArg[2]));` |
|     31 |  786 | `	pSh->iLastErr = (int)rc;` |
|     31 |  787 | `	ph7_result_bool(pCtx,rc == CURLSHE_OK);` |
|     31 |  788 | `	return PH7_OK;` |
|     18 |  789 | `}` |
|      - |  790 | `/*` |
|      - |  791 | ` * The option set a persistent share is asked for, as a bit per` |
|      - |  792 | ` * CURL_LOCK_DATA_*, or -1 with the refusal already thrown.` |
|      - |  793 | ` *` |
|      - |  794 | ` * php validates ELEMENT BY ELEMENT in the array's own order, and the three` |
|      - |  795 | ` * refusals are three different sentences: a value no int cast applies to is a` |
|      - |  796 | ` * TypeError naming the type it got; one that is not a CURL_LOCK_DATA_* is a` |
|      - |  797 | ` * ValueError; and COOKIE is a ValueError of its own, because a cookie jar` |
|      - |  798 | ` * shared across php REQUESTS would hand one visitor's cookies to the next.` |
|      - |  799 | ` * The set itself is normalized -- sorted, and each name once -- which is what` |
|      - |  800 | `` * the `options` property answers.`` |
|      - |  801 | ` */` |
|      - |  802 | `#define PHL_CURLSH_LOCK_MIN CURL_LOCK_DATA_COOKIE` |
|      - |  803 | `#define PHL_CURLSH_LOCK_MAX CURL_LOCK_DATA_PSL` |
|      - |  804 | `struct CurlShareOptWalk {` |
|      - |  805 | `	ph7_context *pCtx;` |
|      - |  806 | `	sxi64 iMask;` |
|      - |  807 | `	sxi32 rcThrow;` |
|      - |  808 | `	int bFailed;` |
|      - |  809 | `};` |
|     46 |  810 | `static int CurlShareOptOne(ph7_value *pKey,ph7_value *pVal,void *pUser)` |
|      1 |  811 | `{` |
|     47 |  812 | `	struct CurlShareOptWalk *pW = (struct CurlShareOptWalk *)pUser;` |
|      - |  813 | `	sxi64 iVal;` |
|     23 |  814 | `	SXUNUSED(pKey);` |
|     72 |  815 | `	if( ph7_value_is_array(pVal) \|\| ph7_value_is_object(pVal) \|\|` |
|     47 |  816 | `		(ph7_value_is_string(pVal) && !PH7_MemObjIsNumeric(pVal)) ){` |
|     10 |  817 | `		pW->rcThrow = PH7_VmThrowException(pW->pCtx,"TypeError",` |
|      - |  818 | `			"curl_share_init_persistent(): Argument #1 ($share_options) must contain "` |
|      3 |  819 | `			"only int values, %s given",PH7_MemObjTypeDump(pVal));` |
|      7 |  820 | `		pW->bFailed = 1;` |
|      7 |  821 | `		return PH7_ABORT;` |
|      - |  822 | `	}` |
|     41 |  823 | `	iVal = ph7_value_to_int64(pVal);` |
|     41 |  824 | `	if( iVal < PHL_CURLSH_LOCK_MIN \|\| iVal > PHL_CURLSH_LOCK_MAX ){` |
|     11 |  825 | `		pW->rcThrow = PH7_VmThrowException(pW->pCtx,"ValueError",` |
|      - |  826 | `			"curl_share_init_persistent(): Argument #1 ($share_options) must contain "` |
|      - |  827 | `			"only CURL_LOCK_DATA_* constants");` |
|     11 |  828 | `		pW->bFailed = 1;` |
|     11 |  829 | `		return PH7_ABORT;` |
|      - |  830 | `	}` |
|     31 |  831 | `	if( iVal == CURL_LOCK_DATA_COOKIE ){` |
|      7 |  832 | `		pW->rcThrow = PH7_VmThrowException(pW->pCtx,"ValueError",` |
|      - |  833 | `			"curl_share_init_persistent(): Argument #1 ($share_options) must not contain "` |
|      - |  834 | `			"CURL_LOCK_DATA_COOKIE because sharing cookies across PHP requests is unsafe");` |
|      7 |  835 | `		pW->bFailed = 1;` |
|      7 |  836 | `		return PH7_ABORT;` |
|      - |  837 | `	}` |
|     25 |  838 | `	pW->iMask \|= ((sxi64)1 << iVal);` |
|     25 |  839 | `	return PH7_OK;` |
|     24 |  840 | `}` |
|      - |  841 | `/*` |
|      - |  842 | ` * CurlSharePersistentHandle curl_share_init_persistent(array $share_options)` |
|      - |  843 | ` *` |
|      - |  844 | ` * php's 8.5 addition, and the newest thing in the extension: a share that is` |
|      - |  845 | ` * meant to outlive the request, so two calls asking for the same set get two` |
|      - |  846 | ` * OBJECTS over one cache rather than two caches. The set is what they are` |
|      - |  847 | ` * matched on, and the object carries it back as a readonly property -- the one` |
|      - |  848 | ` * property any handle class here declares.` |
|      - |  849 | ` */` |
|     40 |  850 | `static int vm_builtin_curl_share_init_persistent(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  851 | `{` |
|     41 |  852 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  853 | `	struct CurlShareOptWalk sWalk;` |
|      - |  854 | `	ph7_class_instance *pThis;` |
|      - |  855 | `	phl_curlsh *pSh;` |
|      - |  856 | `	ph7_value *pOpts;` |
|      - |  857 | `	int i;` |
|     41 |  858 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) \|\| ph7_array_count(apArg[0]) < 1 ){` |
|      3 |  859 | `		ph7_result_bool(pCtx,0);` |
|      3 |  860 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  861 | `			"curl_share_init_persistent(): Argument #1 ($share_options) must not be empty");` |
|      - |  862 | `	}` |
|     39 |  863 | `	sWalk.pCtx = pCtx;` |
|     39 |  864 | `	sWalk.iMask = 0;` |
|     39 |  865 | `	sWalk.rcThrow = PH7_OK;` |
|     39 |  866 | `	sWalk.bFailed = 0;` |
|     39 |  867 | `	ph7_array_walk(apArg[0],CurlShareOptOne,&sWalk);` |
|     39 |  868 | `	if( sWalk.bFailed ){` |
|     23 |  869 | `		ph7_result_bool(pCtx,0);` |
|     23 |  870 | `		return sWalk.rcThrow;` |
|      - |  871 | `	}` |
|      - |  872 | `	/* The same set answers the same cache, which is what "persistent" means` |
|      - |  873 | `	 * here: the registry is per-VM, since a CURLSH shared across VMs would` |
|      - |  874 | `	 * outlive the allocator that tracks it and be read by two threads of a` |
|      - |  875 | ``	 * `-S` server at once. */`` |
|     65 |  876 | `	for( pSh = (phl_curlsh *)pVm->pCurlShares ; pSh ; pSh = pSh->pNext ){` |
|     59 |  877 | `		if( pSh->bPersistent && pSh->bOwnsShare && pSh->iMask == sWalk.iMask ){` |
|     11 |  878 | `			break;` |
|      - |  879 | `		}` |
|     25 |  880 | `	}` |
|     17 |  881 | `	if( pSh ){` |
|      - |  882 | `		/* Join the cache, with a record (and so an object) of this call's own. */` |
|     11 |  883 | `		pSh = CurlShareNew(pVm,pSh->pShare);` |
|      6 |  884 | `	}else{` |
|      7 |  885 | `		pSh = CurlShareNew(pVm,0);` |
|      7 |  886 | `		if( pSh ){` |
|     37 |  887 | `			for( i = PHL_CURLSH_LOCK_MIN ; i <= PHL_CURLSH_LOCK_MAX ; ++i ){` |
|     31 |  888 | `				if( sWalk.iMask & ((sxi64)1 << i) ){` |
|      9 |  889 | `					curl_share_setopt(pSh->pShare,CURLSHOPT_SHARE,(long)i);` |
|      4 |  890 | `				}` |
|     16 |  891 | `			}` |
|      3 |  892 | `		}` |
|      - |  893 | `	}` |
|     17 |  894 | `	if( pSh == 0 ){` |
|    ! 0 |  895 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  896 | `		return PH7_OK;` |
|      - |  897 | `	}` |
|     17 |  898 | `	pSh->bPersistent = 1;` |
|     17 |  899 | `	pSh->iMask = sWalk.iMask;` |
|      - |  900 | `	/*` |
|      - |  901 | `	 * Every call gets its OWN object -- php's two are never identical, and` |
|      - |  902 | `	 * never equal either -- and each carries the normalized set. The property` |
|      - |  903 | `	 * is readonly and protected(set), so this write is the only one it will` |
|      - |  904 | `	 * ever take.` |
|      - |  905 | `	 */` |
|     17 |  906 | `	pThis = PH7_CurlNewInstance(pVm,CurlSharePersistClass,(int)SyStrlen(CurlSharePersistClass));` |
|     17 |  907 | `	if( pThis == 0 \|\| PH7_CurlSlotAttach(pThis,(void *)pSh) != 0 ){` |
|    ! 0 |  908 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  909 | `		return PH7_OK;` |
|      - |  910 | `	}` |
|     17 |  911 | `	if( pSh->pOwner == 0 ){` |
|     17 |  912 | `		pSh->pOwner = pThis;` |
|      8 |  913 | `	}` |
|     17 |  914 | `	pOpts = ph7_new_array(pVm);` |
|     17 |  915 | `	if( pOpts ){` |
|     17 |  916 | `		ph7_value *pItem = ph7_new_scalar(pVm);` |
|     17 |  917 | `		if( pItem ){` |
|     97 |  918 | `			for( i = PHL_CURLSH_LOCK_MIN ; i <= PHL_CURLSH_LOCK_MAX ; ++i ){` |
|     81 |  919 | `				if( sWalk.iMask & ((sxi64)1 << i) ){` |
|     19 |  920 | `					ph7_value_int64(pItem,(sxi64)i);` |
|     19 |  921 | `					ph7_array_add_elem(pOpts,0,pItem);` |
|      9 |  922 | `				}` |
|     41 |  923 | `			}` |
|     17 |  924 | `			ph7_release_value(pVm,pItem);` |
|      8 |  925 | `		}` |
|     17 |  926 | `		PH7_NativeSetProp(pVm,pThis,"options",sizeof("options")-1,pOpts);` |
|     17 |  927 | `		ph7_release_value(pVm,pOpts);` |
|      8 |  928 | `	}` |
|     17 |  929 | `	PH7_NativeResultObject(pCtx,pThis);` |
|     17 |  930 | `	return PH7_OK;` |
|     21 |  931 | `}` |
|      - |  932 | `/*` |
|      - |  933 | ` * CURLOPT_SHARE, reached from curl_setopt's option table. Only a share object` |
|      - |  934 | ` * of either class is attached; php takes anything else in silence.` |
|      - |  935 | ` *` |
|      - |  936 | ` * What makes it a share is that its record is on the share REGISTRY -- a` |
|      - |  937 | ` * cheaper and safer question than its class name, since a CurlHandle and a` |
|      - |  938 | ` * CurlMultiHandle keep their own records in the very same hidden slot.` |
|      - |  939 | ` */` |
|     22 |  940 | `PH7_PRIVATE int PH7_CurlSetShare(phl_curl *pCurl,ph7_value *pVal)` |
|      1 |  941 | `{` |
|      - |  942 | `	void *pRec;` |
|      - |  943 | `	phl_curlsh *pSh;` |
|     23 |  944 | `	if( !ph7_value_is_object(pVal) ){` |
|      9 |  945 | `		return 0;` |
|      - |  946 | `	}` |
|     15 |  947 | `	pRec = PH7_CurlSlotOf((ph7_class_instance *)pVal->x.pOther);` |
|     15 |  948 | `	if( pRec == 0 ){` |
|      3 |  949 | `		return 0;` |
|      - |  950 | `	}` |
|    103 |  951 | `	for( pSh = (phl_curlsh *)pCurl->pVm->pCurlShares ; pSh ; pSh = pSh->pNext ){` |
|     99 |  952 | `		if( (void *)pSh == pRec ){` |
|      9 |  953 | `			break;` |
|      - |  954 | `		}` |
|     46 |  955 | `	}` |
|     13 |  956 | `	if( pSh == 0 \|\| pSh->pShare == 0 ){` |
|      5 |  957 | `		return 0;` |
|      - |  958 | `	}` |
|      9 |  959 | `	curl_easy_setopt(pCurl->pEasy,CURLOPT_SHARE,pSh->pShare);` |
|      - |  960 | `	/* The handle holds the OBJECT: libcurl keeps the CURLSH pointer and reads` |
|      - |  961 | `	 * it during every transfer, so the share must outlive the handle naming` |
|      - |  962 | `	 * it. */` |
|      9 |  963 | `	if( pCurl->pShare ){` |
|      7 |  964 | `		ph7_release_value(pCurl->pVm,pCurl->pShare);` |
|      3 |  965 | `	}` |
|      9 |  966 | `	pCurl->pShare = ph7_new_scalar(pCurl->pVm);` |
|      9 |  967 | `	if( pCurl->pShare ){` |
|      9 |  968 | `		PH7_MemObjStore(pVal,pCurl->pShare);` |
|      4 |  969 | `	}` |
|      9 |  970 | `	return 1;` |
|     12 |  971 | `}` |
|      - |  972 |  |
|      - |  973 | `/* ===== Installation ===== */` |
|      - |  974 |  |
|   7925 |  975 | `PH7_PRIVATE sxi32 PH7_VmInstallCurlMulti(ph7_vm *pVm)` |
|      5 |  976 | `{` |
|      - |  977 | `	static const struct {` |
|      - |  978 | `		const char *zName;` |
|      - |  979 | `		ProchHostFunction xFunc;` |
|      - |  980 | `	} aFunc[] = {` |
|      - |  981 | `		{ "curl_multi_init",          vm_builtin_curl_multi_init          },` |
|      - |  982 | `		{ "curl_multi_close",         vm_builtin_curl_multi_close         },` |
|      - |  983 | `		{ "curl_multi_errno",         vm_builtin_curl_multi_errno         },` |
|      - |  984 | `		{ "curl_multi_setopt",        vm_builtin_curl_multi_setopt        },` |
|      - |  985 | `		{ "curl_multi_add_handle",    vm_builtin_curl_multi_add_handle    },` |
|      - |  986 | `		{ "curl_multi_remove_handle", vm_builtin_curl_multi_remove_handle },` |
|      - |  987 | `		{ "curl_multi_get_handles",   vm_builtin_curl_multi_get_handles   },` |
|      - |  988 | `		{ "curl_multi_exec",          vm_builtin_curl_multi_exec          },` |
|      - |  989 | `		{ "curl_multi_select",        vm_builtin_curl_multi_select        },` |
|      - |  990 | `		{ "curl_multi_info_read",     vm_builtin_curl_multi_info_read     },` |
|      - |  991 | `		{ "curl_multi_getcontent",    vm_builtin_curl_multi_getcontent    },` |
|      - |  992 | `		{ "curl_share_init",          vm_builtin_curl_share_init          },` |
|      - |  993 | `		{ "curl_share_init_persistent", vm_builtin_curl_share_init_persistent },` |
|      - |  994 | `		{ "curl_share_close",         vm_builtin_curl_share_close         },` |
|      - |  995 | `		{ "curl_share_errno",         vm_builtin_curl_share_errno         },` |
|      - |  996 | `		{ "curl_share_setopt",        vm_builtin_curl_share_setopt        }` |
|      - |  997 | `	};` |
|      - |  998 | `	/*` |
|      - |  999 | `	 * The set, and nothing else: php's CurlMultiHandle declares no method, no` |
|      - | 1000 | `	 * constant and no property, and prints as an empty object on every` |
|      - | 1001 | `	 * presentation surface. The one slot here is engine storage, hidden so it` |
|      - | 1002 | `	 * appears on none of them.` |
|      - | 1003 | `	 */` |
|      - | 1004 | `	static const PH7_NativePropDef aProp[] = {` |
|      - | 1005 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }` |
|      - | 1006 | `	};` |
|      - | 1007 | `	/*` |
|      - | 1008 | `	 * FINAL, NOINSTANTIATE with php's own per-class sentence, NOSERIALIZE and` |
|      - | 1009 | `	 * -- unlike CurlHandle -- NOCLONE: libcurl has no curl_multi_duphandle, so` |
|      - | 1010 | ``	 * php's `clone $mh` is "Trying to clone an uncloneable object".`` |
|      - | 1011 | `	 */` |
|      - | 1012 | `	/*` |
|      - | 1013 | `	 * The persistent share is the only handle class in this extension with a` |
|      - | 1014 | ``	 * php-visible property: `public protected(set) readonly array $options`,`` |
|      - | 1015 | `	 * the set it was asked for, normalized. It is written once by the factory` |
|      - | 1016 | ``	 * and refused every other way -- a store, an `unset()`, an indirect`` |
|      - | 1017 | ``	 * modification through `[]` and a dynamic property beside it.`` |
|      - | 1018 | `	 */` |
|      - | 1019 | `	static const PH7_NativePropDef aShareProp[] = {` |
|      - | 1020 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }` |
|      - | 1021 | `	};` |
|      - | 1022 | `	static const PH7_NativePropDef aPersistProp[] = {` |
|      - | 1023 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 1024 | `		{ "options", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|      - | 1025 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "array" }` |
|      - | 1026 | `	};` |
|      - | 1027 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 1028 | `		/* PH7_CLASS_HANDLE_ID: like the easy handle, php's cast_object answers this` |
|      - | 1029 | ``		 * one's OBJECT HANDLE for `(int)`, silently. The two SHARE classes below do`` |
|      - | 1030 | `		 * not -- php never made them resources, so they keep the ordinary refusal. */` |
|      - | 1031 | `		{ "CurlMultiHandle", 0, 0,` |
|      - | 1032 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE\|PH7_CLASS_HANDLE_ID,` |
|      - | 1033 | `		  0, 0, 0, 0,` |
|      - | 1034 | `		  aProp, SX_ARRAYSIZE(aProp),` |
|      - | 1035 | `		  CurlMultiInstanceRelease, 0, 0 },` |
|      - | 1036 | `		{ "CurlShareHandle", 0, 0,` |
|      - | 1037 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|      - | 1038 | `		  0, 0, 0, 0,` |
|      - | 1039 | `		  aShareProp, SX_ARRAYSIZE(aShareProp),` |
|      - | 1040 | `		  CurlShareInstanceRelease, 0, 0 },` |
|      - | 1041 | `		/* NOT a subclass of CurlShareHandle: php's share verbs refuse this one` |
|      - | 1042 | `		 * BY TYPE, which is what keeps a process-wide cache out of the reach of` |
|      - | 1043 | `		 * the setter that would reconfigure it. */` |
|      - | 1044 | `		{ "CurlSharePersistentHandle", 0, 0,` |
|      - | 1045 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|      - | 1046 | `		  0, 0, 0, 0,` |
|      - | 1047 | `		  aPersistProp, SX_ARRAYSIZE(aPersistProp),` |
|      - | 1048 | `		  CurlShareInstanceRelease, 0, 0 }` |
|      - | 1049 | `	};` |
|      - | 1050 | `	sxu32 n;` |
|      - | 1051 | `	sxi32 rc;` |
|   7930 | 1052 | `	pVm->pCurlMultis = 0;` |
|   7930 | 1053 | `	pVm->pCurlShares = 0;` |
| 134730 | 1054 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; ++n ){` |
| 126805 | 1055 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  63317 | 1056 | `	}` |
|   7930 | 1057 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|   7930 | 1058 | `	if( rc == SXRET_OK ){` |
|   7930 | 1059 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"CurlMultiHandle",` |
|      - | 1060 | `			sizeof("CurlMultiHandle")-1,FALSE,0);` |
|   7930 | 1061 | `		if( pClass ){` |
|   7930 | 1062 | `			pClass->zNewRefusal =` |
|      - | 1063 | `				"Cannot directly construct CurlMultiHandle, use curl_multi_init() instead";` |
|   7930 | 1064 | `			pClass->xCmp = PH7_NativeCmpOpaqueHandle;` |
|   3957 | 1065 | `		}` |
|   7930 | 1066 | `		pClass = PH7_VmExtractClass(&(*pVm),"CurlShareHandle",` |
|      - | 1067 | `			sizeof("CurlShareHandle")-1,FALSE,0);` |
|   7930 | 1068 | `		if( pClass ){` |
|   7930 | 1069 | `			pClass->zNewRefusal =` |
|      - | 1070 | `				"Cannot directly construct CurlShareHandle, use curl_share_init() instead";` |
|   7930 | 1071 | `			pClass->xCmp = PH7_NativeCmpOpaqueHandle;` |
|   3957 | 1072 | `		}` |
|   7930 | 1073 | `		pClass = PH7_VmExtractClass(&(*pVm),"CurlSharePersistentHandle",` |
|      - | 1074 | `			sizeof("CurlSharePersistentHandle")-1,FALSE,0);` |
|   7930 | 1075 | `		if( pClass ){` |
|   7930 | 1076 | `			pClass->zNewRefusal =` |
|      - | 1077 | `				"Cannot directly construct CurlSharePersistentHandle, "` |
|      - | 1078 | `				"use curl_share_init_persistent() instead";` |
|   7930 | 1079 | `			pClass->xCmp = PH7_NativeCmpOpaqueHandle;` |
|   3957 | 1080 | `		}` |
|   3957 | 1081 | `	}` |
|   7930 | 1082 | `	return rc;` |
|      5 | 1083 | `}` |
|      - | 1084 |  |
|      - | 1085 | `#else` |
|      - | 1086 | `/* Ensure non-empty translation unit when curl is disabled (MSVC C4206) */` |
|      - | 1087 | `typedef int vm_curl_multi_unused;` |
|      - | 1088 | `#endif /* PH7_ENABLE_CURL */` |
|      - | 1089 |  |
