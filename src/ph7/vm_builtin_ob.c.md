# src/ph7/vm_builtin_ob.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 157/206 lines (76.21%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|     - |    8 | `/*` |
|     - |    9 | ` * void ob_clean(void)` |
|     - |   10 | ` *  This function discards the contents of the output buffer.` |
|     - |   11 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|     - |   12 | ` * Parameter` |
|     - |   13 | ` *  None` |
|     - |   14 | ` * Return` |
|     - |   15 | ` *  No value is returned.` |
|     - |   16 | ` */` |
|     2 |   17 | `PH7_PRIVATE int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   18 |  |
|     3 |   19 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |   20 | `	VmObEntry *pOb;` |
|     1 |   21 | `	SXUNUSED(nArg); /* cc warning */` |
|     1 |   22 | `	SXUNUSED(apArg);` |
|     - |   23 | `	/* Peek the top most OB */` |
|     3 |   24 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     3 |   25 | `	if( pOb ){` |
|     3 |   26 | `		SyBlobRelease(&pOb->sOB);` |
|     1 |   27 | `	}` |
|     3 |   28 | `	return PH7_OK;` |
|     1 |   29 |  |
|     - |   30 | `/*` |
|     - |   31 | ` * bool ob_end_clean(void)` |
|     - |   32 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|     - |   33 | ` *  This function discards the contents of the topmost output buffer and turns` |
|     - |   34 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|     - |   35 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|     - |   36 | ` *  are discarded when ob_end_clean() is called.` |
|     - |   37 | ` * Parameter` |
|     - |   38 | ` *  None` |
|     - |   39 | ` * Return` |
|     - |   40 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|     - |   41 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|     - |   42 | ` * (possible for special buffer)` |
|     - |   43 | ` */` |
|  3958 |   44 | `PH7_PRIVATE int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   45 |  |
|  3960 |   46 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |   47 | `	VmObEntry *pOb;` |
|     - |   48 | `	/* Pop the top most OB */` |
|  3960 |   49 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|  3960 |   50 | `	if( pOb == 0){` |
|     - |   51 | `		/* No such OB,return FALSE */` |
|   ! 0 |   52 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |   53 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |   54 | `		SXUNUSED(apArg);` |
|   ! 0 |   55 | `	}else{` |
|     - |   56 | `		/* Release */` |
|  3960 |   57 | `		VmObRestore(pVm,pOb);` |
|     - |   58 | `		/* Return true */` |
|  3960 |   59 | `		ph7_result_bool(pCtx,1);` |
|     - |   60 | `	}` |
|  3960 |   61 | `	return PH7_OK;` |
|     2 |   62 |  |
|     - |   63 | `/*` |
|     - |   64 | ` * string ob_get_contents(void)` |
|     - |   65 | ` *  Gets the contents of the output buffer without clearing it.` |
|     - |   66 | ` * Parameter` |
|     - |   67 | ` *  None` |
|     - |   68 | ` * Return` |
|     - |   69 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|     - |   70 | ` */` |
|     6 |   71 | `PH7_PRIVATE int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   72 |  |
|     7 |   73 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |   74 | `	VmObEntry *pOb;` |
|     - |   75 | `	/* Peek the top most OB */` |
|     7 |   76 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     7 |   77 | `	if( pOb == 0 ){` |
|     - |   78 | `		/* No active OB,return FALSE */` |
|   ! 0 |   79 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |   80 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |   81 | `		SXUNUSED(apArg);` |
|   ! 0 |   82 | `	}else{` |
|     - |   83 | `		/* Return contents */` |
|     7 |   84 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|     - |   85 | `	}` |
|     7 |   86 | `	return PH7_OK;` |
|     1 |   87 |  |
|     - |   88 | `/*` |
|     - |   89 | ` * string ob_get_clean(void)` |
|     - |   90 | ` * string ob_get_flush(void)` |
|     - |   91 | ` *  Get current buffer contents and delete current output buffer.` |
|     - |   92 | ` * Parameter` |
|     - |   93 | ` *  None` |
|     - |   94 | ` * Return` |
|     - |   95 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|     - |   96 | ` */` |
|  5184 |   97 | `PH7_PRIVATE int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   98 |  |
|  5186 |   99 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  100 | `	VmObEntry *pOb;` |
|     - |  101 | `	/* Pop the top most OB */` |
|  5186 |  102 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|  5186 |  103 | `	if( pOb == 0 ){` |
|     - |  104 | `		/* No active OB,return FALSE */` |
|   ! 0 |  105 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  106 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  107 | `		SXUNUSED(apArg);` |
|   ! 0 |  108 | `	}else{` |
|     - |  109 | `		/* Return contents */` |
|  5186 |  110 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */` |
|     - |  111 | `		/* Release */` |
|  5186 |  112 | `		VmObRestore(pVm,pOb);` |
|     - |  113 | `	}` |
|  5186 |  114 | `	return PH7_OK;` |
|     2 |  115 |  |
|     - |  116 | `/*` |
|     - |  117 | ` * int ob_get_length(void)` |
|     - |  118 | ` *  Return the length of the output buffer.` |
|     - |  119 | ` * Parameter` |
|     - |  120 | ` *  None` |
|     - |  121 | ` * Return` |
|     - |  122 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|     - |  123 | ` */` |
|     2 |  124 | `PH7_PRIVATE int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  125 |  |
|     3 |  126 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  127 | `	VmObEntry *pOb;` |
|     - |  128 | `	/* Peek the top most OB */` |
|     3 |  129 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     3 |  130 | `	if( pOb == 0 ){` |
|     - |  131 | `		/* No active OB,return FALSE */` |
|   ! 0 |  132 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  133 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  134 | `		SXUNUSED(apArg);` |
|   ! 0 |  135 | `	}else{` |
|     - |  136 | `		/* Return OB length */` |
|     3 |  137 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|     - |  138 | `	}` |
|     3 |  139 | `	return PH7_OK;` |
|     1 |  140 |  |
|     - |  141 | `/*` |
|     - |  142 | ` * int ob_get_level(void)` |
|     - |  143 | ` *  Returns the nesting level of the output buffering mechanism.` |
|     - |  144 | ` * Parameter` |
|     - |  145 | ` *  None` |
|     - |  146 | ` * Return` |
|     - |  147 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|     - |  148 | ` */` |
|     6 |  149 | `PH7_PRIVATE int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  150 |  |
|     7 |  151 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  152 | `	int iNest;` |
|     3 |  153 | `	SXUNUSED(nArg); /* cc warning */` |
|     3 |  154 | `	SXUNUSED(apArg);` |
|     - |  155 | `	/* Nesting level */` |
|     7 |  156 | `	iNest = (int)SySetUsed(&pVm->aOB);` |
|     - |  157 | `	/* Return the nesting value */` |
|     7 |  158 | `	ph7_result_int(pCtx,iNest);` |
|     7 |  159 | `	return PH7_OK;` |
|     1 |  160 |  |
|     - |  161 | `/*` |
|     - |  162 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|     - |  163 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|     - |  164 | ` * Refer to the implementation of [ob_start()] for more information.` |
|     - |  165 | ` */` |
| 10302 |  166 | `PH7_PRIVATE int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|     2 |  167 |  |
| 10304 |  168 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|     - |  169 | `	VmObEntry *pEntry;` |
|     - |  170 | `	ph7_value sResult;` |
|     - |  171 | `	/* Peek the top most entry */` |
| 10304 |  172 | `	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);` |
| 10304 |  173 | `	if( pEntry == 0 ){` |
|     - |  174 | `		/* CAN'T HAPPEN */` |
|   ! 0 |  175 | `		return PH7_OK;` |
|     - |  176 | `	}` |
| 10304 |  177 | `	PH7_MemObjInit(pVm,&sResult);` |
| 10304 |  178 | `	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){` |
|     - |  179 | `		ph7_value sArg,*apArg[2];` |
|     - |  180 | `		/* Fill the first argument */` |
|   ! 0 |  181 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|   ! 0 |  182 | `		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);` |
|   ! 0 |  183 | `		apArg[0] = &sArg;` |
|     - |  184 | `		/* Call the 'filter' callback */` |
|   ! 0 |  185 | `		pVm->nObDepth++;` |
|   ! 0 |  186 | `		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult);` |
|   ! 0 |  187 | `		pVm->nObDepth--;` |
|   ! 0 |  188 | `		if( sResult.iFlags & MEMOBJ_STRING ){` |
|     - |  189 | `			/* Extract the function result */` |
|   ! 0 |  190 | `			pData = SyBlobData(&sResult.sBlob);` |
|   ! 0 |  191 | `			nDataLen = SyBlobLength(&sResult.sBlob);` |
|   ! 0 |  192 | `		}` |
|   ! 0 |  193 | `		PH7_MemObjRelease(&sArg);` |
|   ! 0 |  194 | `	}` |
| 10304 |  195 | `	if( nDataLen > 0 ){` |
|     - |  196 | `		/* Redirect the VM output to the internal buffer */` |
| 10304 |  197 | `		SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
|  5151 |  198 | `	}` |
|     - |  199 | `	/* Release */` |
| 10304 |  200 | `	PH7_MemObjRelease(&sResult);` |
| 10304 |  201 | `	return PH7_OK;` |
|  5153 |  202 |  |
|     - |  203 | `/*` |
|     - |  204 | ` * Restore the default consumer.` |
|     - |  205 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|     - |  206 | ` * information.` |
|     - |  207 | ` */` |
|  9144 |  208 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|     2 |  209 |  |
|  9146 |  210 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|  9146 |  211 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|     - |  212 | `		/* No more stackable OB */` |
|  9128 |  213 | `		pCons->xConsumer = pCons->xDef;` |
|  9128 |  214 | `		pCons->pUserData = pCons->pDefData;` |
|  4563 |  215 | `	}` |
|     - |  216 | `	/* Release OB data */` |
|  9146 |  217 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|  9146 |  218 | `	SyBlobRelease(&pEntry->sOB);` |
|  9146 |  219 |  |
|     - |  220 | `/*` |
|     - |  221 | ` * bool ob_start([ callback $output_callback] )` |
|     - |  222 | ` * This function will turn output buffering on. While output buffering is active no output` |
|     - |  223 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|     - |  224 | ` *  buffer.` |
|     - |  225 | ` * Parameter` |
|     - |  226 | ` *  $output_callback` |
|     - |  227 | ` *   An optional output_callback function may be specified. This function takes a string` |
|     - |  228 | ` *   as a parameter and should return a string. The function will be called when the output` |
|     - |  229 | ` *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)` |
|     - |  230 | ` *   or when the output buffer is flushed to the browser at the end of the request.` |
|     - |  231 | ` *   When output_callback is called, it will receive the contents of the output buffer` |
|     - |  232 | ` *   as its parameter and is expected to return a new output buffer as a result, which will` |
|     - |  233 | ` *   be sent to the browser. If the output_callback is not a callable function, this function` |
|     - |  234 | ` *   will return FALSE.` |
|     - |  235 | ` *   If the callback function has two parameters, the second parameter is filled with` |
|     - |  236 | ` *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT` |
|     - |  237 | ` *   and PHP_OUTPUT_HANDLER_END.` |
|     - |  238 | ` *   If output_callback returns FALSE original input is sent to the browser.` |
|     - |  239 | ` *   The output_callback parameter may be bypassed by passing a NULL value.` |
|     - |  240 | ` * Return` |
|     - |  241 | ` *   Returns TRUE on success or FALSE on failure.` |
|     - |  242 | ` */` |
|  9144 |  243 | `PH7_PRIVATE int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  244 |  |
|  9146 |  245 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  246 | `	VmObEntry sOb;` |
|     - |  247 | `	sxi32 rc;` |
|     - |  248 | `	/* Initialize the OB entry */` |
|  9146 |  249 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|  9146 |  250 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|  9146 |  251 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) ){` |
|     - |  252 | `		/* Save the callback name for later invocation */` |
|   ! 0 |  253 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|   ! 0 |  254 | `	}` |
|     - |  255 | `	/* Push in the stack */` |
|  9146 |  256 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|  9146 |  257 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  258 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|   ! 0 |  259 | `	}else{` |
|  9146 |  260 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     - |  261 | `		/* Substitute the default VM consumer */` |
|  9146 |  262 | `		if( pCons->xConsumer != VmObConsumer ){` |
|  9128 |  263 | `			pCons->xDef = pCons->xConsumer;` |
|  9128 |  264 | `			pCons->pDefData = pCons->pUserData;` |
|     - |  265 | `			/* Install the new consumer */` |
|  9128 |  266 | `			pCons->xConsumer = VmObConsumer;` |
|  9128 |  267 | `			pCons->pUserData = pVm;` |
|  4563 |  268 | `		}` |
|     - |  269 | `	}` |
|  9146 |  270 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|  9146 |  271 | `	return PH7_OK;` |
|     2 |  272 |  |
|     - |  273 | `/*` |
|     - |  274 | ` * Flush Output buffer to the default VM output consumer.` |
|     - |  275 | ` * Refer to the implementation of [ob_flush()] for more` |
|     - |  276 | ` * information.` |
|     - |  277 | ` */` |
|     4 |  278 | `static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)` |
|     1 |  279 |  |
|     5 |  280 | `	SyBlob *pBlob = &pEntry->sOB;` |
|     - |  281 | `	sxi32 rc;` |
|     - |  282 | `	/* Flush contents */` |
|     5 |  283 | `	rc = PH7_OK;` |
|     5 |  284 | `	if( SyBlobLength(pBlob) > 0 ){` |
|     - |  285 | `		/* Call the VM output consumer */` |
|     5 |  286 | `		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);` |
|     - |  287 | `		/* Increment VM output counter */` |
|     5 |  288 | `		pVm->nOutputLen += SyBlobLength(pBlob);` |
|     5 |  289 | `		if( rc != PH7_ABORT ){` |
|     5 |  290 | `			rc = PH7_OK;` |
|     2 |  291 | `		}` |
|     2 |  292 | `	}` |
|     5 |  293 | `	if( bRelease ){` |
|     3 |  294 | `		VmObRestore(&(*pVm),pEntry);` |
|     2 |  295 | `	}else{` |
|     - |  296 | `		/* Reset the blob */` |
|     3 |  297 | `		SyBlobReset(pBlob);` |
|     - |  298 | `	}` |
|     5 |  299 | `	return rc;` |
|     1 |  300 |  |
|     - |  301 | `/*` |
|     - |  302 | ` * void ob_flush(void)` |
|     - |  303 | ` * void flush(void)` |
|     - |  304 | ` *  Flush (send) the output buffer.` |
|     - |  305 | ` * Parameter` |
|     - |  306 | ` *  None` |
|     - |  307 | ` * Return` |
|     - |  308 | ` *  No return value.` |
|     - |  309 | ` */` |
|     2 |  310 | `PH7_PRIVATE int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  311 |  |
|     3 |  312 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  313 | `	VmObEntry *pOb;` |
|     - |  314 | `	sxi32 rc;` |
|     - |  315 | `	/* Peek the top most OB entry */` |
|     3 |  316 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     3 |  317 | `	if( pOb == 0 ){` |
|     - |  318 | `		/* Empty stack,return immediately */` |
|   ! 0 |  319 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  320 | `		SXUNUSED(apArg);` |
|   ! 0 |  321 | `		return PH7_OK;` |
|     - |  322 | `	}` |
|     - |  323 | `	/* Flush contents */` |
|     3 |  324 | `	rc = VmObFlush(pVm,pOb,FALSE);` |
|     3 |  325 | `	return rc;` |
|     2 |  326 |  |
|     - |  327 | `/*` |
|     - |  328 | ` * bool ob_end_flush(void)` |
|     - |  329 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|     - |  330 | ` * Parameter` |
|     - |  331 | ` *  None` |
|     - |  332 | ` * Return` |
|     - |  333 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|     - |  334 | ` *  that you called the function without an active buffer or that for some reason` |
|     - |  335 | ` *  a buffer could not be deleted (possible for special buffer).` |
|     - |  336 | ` */` |
|     2 |  337 | `PH7_PRIVATE int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  338 |  |
|     3 |  339 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  340 | `	VmObEntry *pOb;` |
|     - |  341 | `	sxi32 rc;` |
|     - |  342 | `	/* Pop the top most OB entry */` |
|     3 |  343 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     3 |  344 | `	if( pOb == 0 ){` |
|     - |  345 | `		/* Empty stack,return FALSE */` |
|   ! 0 |  346 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  347 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  348 | `		SXUNUSED(apArg);` |
|   ! 0 |  349 | `		return PH7_OK;` |
|     - |  350 | `	}` |
|     - |  351 | `	/* Flush contents */` |
|     3 |  352 | `	rc = VmObFlush(pVm,pOb,TRUE);` |
|     - |  353 | `	/* Return true */` |
|     3 |  354 | `	ph7_result_bool(pCtx,1);` |
|     3 |  355 | `	return rc;` |
|     2 |  356 |  |
|     - |  357 | `/*` |
|     - |  358 | ` * void ob_implicit_flush([int $flag = true ])` |
|     - |  359 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|     - |  360 | ` *  Implicit flushing will result in a flush operation after every` |
|     - |  361 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|     - |  362 | ` * Parameter` |
|     - |  363 | ` *  $flag` |
|     - |  364 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|     - |  365 | ` * Return` |
|     - |  366 | ` *   Nothing` |
|     - |  367 | ` */` |
|     4 |  368 | `PH7_PRIVATE int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  369 |  |
|     - |  370 | `	/* NOTE: As of this version,this function is a no-op.` |
|     - |  371 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|     - |  372 | `	 */` |
|     2 |  373 | `	SXUNUSED(pCtx);` |
|     2 |  374 | `	SXUNUSED(nArg); /* cc warning */` |
|     2 |  375 | `	SXUNUSED(apArg);` |
|     5 |  376 | `	return PH7_OK;` |
|     1 |  377 |  |
|     - |  378 | `/*` |
|     - |  379 | ` * array ob_list_handlers(void)` |
|     - |  380 | ` *  Lists all output handlers in use.` |
|     - |  381 | ` * Parameter` |
|     - |  382 | ` *  None` |
|     - |  383 | ` * Return` |
|     - |  384 | ` *  This will return an array with the output handlers in use (if any).` |
|     - |  385 | ` */` |
|     2 |  386 | `PH7_PRIVATE int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  387 |  |
|     3 |  388 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  389 | `	ph7_value *pArray;` |
|     - |  390 | `	VmObEntry *aEntry;` |
|     - |  391 | `	ph7_value sVal;` |
|     - |  392 | `	sxu32 n;` |
|     3 |  393 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|     - |  394 | `		/* Empty stack,return null */` |
|   ! 0 |  395 | `		ph7_result_null(pCtx);` |
|   ! 0 |  396 | `		return PH7_OK;` |
|     - |  397 | `	}` |
|     - |  398 | `	/* Create a new array */` |
|     3 |  399 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  400 | `	if( pArray == 0 ){` |
|     - |  401 | `		/* Out of memory,return NULL */` |
|   ! 0 |  402 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  403 | `		SXUNUSED(apArg);` |
|   ! 0 |  404 | `		ph7_result_null(pCtx);` |
|   ! 0 |  405 | `		return PH7_OK;` |
|     - |  406 | `	}` |
|     3 |  407 | `	PH7_MemObjInit(pVm,&sVal);` |
|     - |  408 | `	/* Point to the installed OB entries */` |
|     3 |  409 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|     - |  410 | `	/* Perform the requested operation */` |
|     5 |  411 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){` |
|     3 |  412 | `		VmObEntry *pEntry = &aEntry[n];` |
|     - |  413 | `		/* Extract handler name */` |
|     3 |  414 | `		SyBlobReset(&sVal.sBlob);` |
|     3 |  415 | `		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){` |
|     - |  416 | `			/* Callback,dup it's name */` |
|   ! 0 |  417 | `			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);` |
|     3 |  418 | `		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){` |
|   ! 0 |  419 | `			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);` |
|   ! 0 |  420 | `		}else{` |
|     3 |  421 | `			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);` |
|     - |  422 | `		}` |
|     3 |  423 | `		sVal.iFlags = MEMOBJ_STRING;` |
|     - |  424 | `		/* Perform the insertion */` |
|     3 |  425 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|     2 |  426 | `	}` |
|     3 |  427 | `	PH7_MemObjRelease(&sVal);` |
|     - |  428 | `	/* Return the freshly created array */` |
|     3 |  429 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  430 | `	return PH7_OK;` |
|     2 |  431 |  |
|     - |  432 |  |
