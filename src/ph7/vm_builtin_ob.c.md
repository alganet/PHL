# src/ph7/vm_builtin_ob.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 157/209 lines (75.12%)

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
|     1 |   18 | `{` |
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
|     1 |   29 | `}` |
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
|  4200 |   44 | `PH7_PRIVATE int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |   45 | `{` |
|  4205 |   46 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |   47 | `	VmObEntry *pOb;` |
|     - |   48 | `	/* Pop the top most OB */` |
|  4205 |   49 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|  4205 |   50 | `	if( pOb == 0){` |
|     - |   51 | `		/* No such OB,return FALSE */` |
|   ! 0 |   52 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |   53 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |   54 | `		SXUNUSED(apArg);` |
|   ! 0 |   55 | `	}else{` |
|     - |   56 | `		/* Release */` |
|  4205 |   57 | `		VmObRestore(pVm,pOb);` |
|     - |   58 | `		/* Return true */` |
|  4205 |   59 | `		ph7_result_bool(pCtx,1);` |
|     - |   60 | `	}` |
|  4205 |   61 | `	return PH7_OK;` |
|     5 |   62 | `}` |
|     - |   63 | `/*` |
|     - |   64 | ` * string ob_get_contents(void)` |
|     - |   65 | ` *  Gets the contents of the output buffer without clearing it.` |
|     - |   66 | ` * Parameter` |
|     - |   67 | ` *  None` |
|     - |   68 | ` * Return` |
|     - |   69 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|     - |   70 | ` */` |
|     6 |   71 | `PH7_PRIVATE int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |   72 | `{` |
|     9 |   73 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |   74 | `	VmObEntry *pOb;` |
|     - |   75 | `	/* Peek the top most OB */` |
|     9 |   76 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     9 |   77 | `	if( pOb == 0 ){` |
|     - |   78 | `		/* No active OB,return FALSE */` |
|   ! 0 |   79 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |   80 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |   81 | `		SXUNUSED(apArg);` |
|   ! 0 |   82 | `	}else{` |
|     - |   83 | `		/* Return contents */` |
|     9 |   84 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|     - |   85 | `	}` |
|     9 |   86 | `	return PH7_OK;` |
|     3 |   87 | `}` |
|     - |   88 | `/*` |
|     - |   89 | ` * string ob_get_clean(void)` |
|     - |   90 | ` * string ob_get_flush(void)` |
|     - |   91 | ` *  Get current buffer contents and delete current output buffer.` |
|     - |   92 | ` * Parameter` |
|     - |   93 | ` *  None` |
|     - |   94 | ` * Return` |
|     - |   95 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|     - |   96 | ` */` |
|  5486 |   97 | `PH7_PRIVATE int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |   98 | `{` |
|  5489 |   99 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  100 | `	VmObEntry *pOb;` |
|     - |  101 | `	/* Pop the top most OB */` |
|  5489 |  102 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|  5489 |  103 | `	if( pOb == 0 ){` |
|     - |  104 | `		/* No active OB,return FALSE */` |
|   ! 0 |  105 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  106 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  107 | `		SXUNUSED(apArg);` |
|   ! 0 |  108 | `	}else{` |
|     - |  109 | `		/* Return contents */` |
|  5489 |  110 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */` |
|     - |  111 | `		/* Release */` |
|  5489 |  112 | `		VmObRestore(pVm,pOb);` |
|     - |  113 | `	}` |
|  5489 |  114 | `	return PH7_OK;` |
|     3 |  115 | `}` |
|     - |  116 | `/*` |
|     - |  117 | ` * int ob_get_length(void)` |
|     - |  118 | ` *  Return the length of the output buffer.` |
|     - |  119 | ` * Parameter` |
|     - |  120 | ` *  None` |
|     - |  121 | ` * Return` |
|     - |  122 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|     - |  123 | ` */` |
|     2 |  124 | `PH7_PRIVATE int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  125 | `{` |
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
|     1 |  140 | `}` |
|     - |  141 | `/*` |
|     - |  142 | ` * int ob_get_level(void)` |
|     - |  143 | ` *  Returns the nesting level of the output buffering mechanism.` |
|     - |  144 | ` * Parameter` |
|     - |  145 | ` *  None` |
|     - |  146 | ` * Return` |
|     - |  147 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|     - |  148 | ` */` |
|     6 |  149 | `PH7_PRIVATE int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  150 | `{` |
|     7 |  151 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  152 | `	int iNest;` |
|     3 |  153 | `	SXUNUSED(nArg); /* cc warning */` |
|     3 |  154 | `	SXUNUSED(apArg);` |
|     - |  155 | `	/* Nesting level */` |
|     7 |  156 | `	iNest = (int)SySetUsed(&pVm->aOB);` |
|     - |  157 | `	/* Return the nesting value */` |
|     7 |  158 | `	ph7_result_int(pCtx,iNest);` |
|     7 |  159 | `	return PH7_OK;` |
|     1 |  160 | `}` |
|     - |  161 | `/*` |
|     - |  162 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|     - |  163 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|     - |  164 | ` * Refer to the implementation of [ob_start()] for more information.` |
|     - |  165 | ` */` |
| 22758 |  166 | `PH7_PRIVATE int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|     5 |  167 | `{` |
| 22763 |  168 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|     - |  169 | `	VmObEntry *pEntry;` |
|     - |  170 | `	ph7_value sResult;` |
|     - |  171 | `	/* Peek the top most entry */` |
| 22763 |  172 | `	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);` |
| 22763 |  173 | `	if( pEntry == 0 ){` |
|     - |  174 | `		/* CAN'T HAPPEN */` |
|   ! 0 |  175 | `		return PH7_OK;` |
|     - |  176 | `	}` |
| 22763 |  177 | `	PH7_MemObjInit(pVm,&sResult);` |
| 22763 |  178 | `	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){` |
|     - |  179 | `		ph7_value sArg,sPhase,*apArg[2];` |
|     - |  180 | `		/* Fill the first argument */` |
|   ! 0 |  181 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|   ! 0 |  182 | `		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);` |
|   ! 0 |  183 | `		apArg[0] = &sArg;` |
|     - |  184 | `		/* php calls the handler as ($buffer, int $phase) — a handler` |
|     - |  185 | `		 * declaring both as required must not trip the arity check. The` |
|     - |  186 | `		 * phase is PHP_OUTPUT_HANDLER_WRITE (0); the per-write phase` |
|     - |  187 | `		 * bitmask semantics (START/FINAL) are not modeled. */` |
|   ! 0 |  188 | `		PH7_MemObjInitFromInt(pVm,&sPhase,0);` |
|   ! 0 |  189 | `		apArg[1] = &sPhase;` |
|     - |  190 | `		/* Call the 'filter' callback */` |
|   ! 0 |  191 | `		pVm->nObDepth++;` |
|   ! 0 |  192 | `		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,2,apArg,&sResult);` |
|   ! 0 |  193 | `		pVm->nObDepth--;` |
|   ! 0 |  194 | `		if( sResult.iFlags & MEMOBJ_STRING ){` |
|     - |  195 | `			/* Extract the function result */` |
|   ! 0 |  196 | `			pData = SyBlobData(&sResult.sBlob);` |
|   ! 0 |  197 | `			nDataLen = SyBlobLength(&sResult.sBlob);` |
|   ! 0 |  198 | `		}` |
|   ! 0 |  199 | `		PH7_MemObjRelease(&sArg);` |
|   ! 0 |  200 | `		PH7_MemObjRelease(&sPhase);` |
|   ! 0 |  201 | `	}` |
| 22763 |  202 | `	if( nDataLen > 0 ){` |
|     - |  203 | `		/* Redirect the VM output to the internal buffer */` |
| 22763 |  204 | `		SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
| 11379 |  205 | `	}` |
|     - |  206 | `	/* Release */` |
| 22763 |  207 | `	PH7_MemObjRelease(&sResult);` |
| 22763 |  208 | `	return PH7_OK;` |
| 11384 |  209 | `}` |
|     - |  210 | `/*` |
|     - |  211 | ` * Restore the default consumer.` |
|     - |  212 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|     - |  213 | ` * information.` |
|     - |  214 | ` */` |
|  9688 |  215 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|     5 |  216 | `{` |
|  9693 |  217 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|  9693 |  218 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|     - |  219 | `		/* No more stackable OB */` |
|  9675 |  220 | `		pCons->xConsumer = pCons->xDef;` |
|  9675 |  221 | `		pCons->pUserData = pCons->pDefData;` |
|  4835 |  222 | `	}` |
|     - |  223 | `	/* Release OB data */` |
|  9693 |  224 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|  9693 |  225 | `	SyBlobRelease(&pEntry->sOB);` |
|  9693 |  226 | `}` |
|     - |  227 | `/*` |
|     - |  228 | ` * bool ob_start([ callback $output_callback] )` |
|     - |  229 | ` * This function will turn output buffering on. While output buffering is active no output` |
|     - |  230 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|     - |  231 | ` *  buffer.` |
|     - |  232 | ` * Parameter` |
|     - |  233 | ` *  $output_callback` |
|     - |  234 | ` *   An optional output_callback function may be specified. This function takes a string` |
|     - |  235 | ` *   as a parameter and should return a string. The function will be called when the output` |
|     - |  236 | ` *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)` |
|     - |  237 | ` *   or when the output buffer is flushed to the browser at the end of the request.` |
|     - |  238 | ` *   When output_callback is called, it will receive the contents of the output buffer` |
|     - |  239 | ` *   as its parameter and is expected to return a new output buffer as a result, which will` |
|     - |  240 | ` *   be sent to the browser. If the output_callback is not a callable function, this function` |
|     - |  241 | ` *   will return FALSE.` |
|     - |  242 | ` *   If the callback function has two parameters, the second parameter is filled with` |
|     - |  243 | ` *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT` |
|     - |  244 | ` *   and PHP_OUTPUT_HANDLER_END.` |
|     - |  245 | ` *   If output_callback returns FALSE original input is sent to the browser.` |
|     - |  246 | ` *   The output_callback parameter may be bypassed by passing a NULL value.` |
|     - |  247 | ` * Return` |
|     - |  248 | ` *   Returns TRUE on success or FALSE on failure.` |
|     - |  249 | ` */` |
|  9688 |  250 | `PH7_PRIVATE int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  251 | `{` |
|  9693 |  252 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  253 | `	VmObEntry sOb;` |
|     - |  254 | `	sxi32 rc;` |
|     - |  255 | `	/* Initialize the OB entry */` |
|  9693 |  256 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|  9693 |  257 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|  9693 |  258 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) ){` |
|     - |  259 | `		/* Save the callback name for later invocation (MEMOBJ_OBJ = a Closure callback). */` |
|   ! 0 |  260 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|   ! 0 |  261 | `	}` |
|     - |  262 | `	/* Push in the stack */` |
|  9693 |  263 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|  9693 |  264 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  265 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|   ! 0 |  266 | `	}else{` |
|  9693 |  267 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     - |  268 | `		/* Substitute the default VM consumer */` |
|  9693 |  269 | `		if( pCons->xConsumer != VmObConsumer ){` |
|  9675 |  270 | `			pCons->xDef = pCons->xConsumer;` |
|  9675 |  271 | `			pCons->pDefData = pCons->pUserData;` |
|     - |  272 | `			/* Install the new consumer */` |
|  9675 |  273 | `			pCons->xConsumer = VmObConsumer;` |
|  9675 |  274 | `			pCons->pUserData = pVm;` |
|  4835 |  275 | `		}` |
|     - |  276 | `	}` |
|  9693 |  277 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|  9693 |  278 | `	return PH7_OK;` |
|     5 |  279 | `}` |
|     - |  280 | `/*` |
|     - |  281 | ` * Flush Output buffer to the default VM output consumer.` |
|     - |  282 | ` * Refer to the implementation of [ob_flush()] for more` |
|     - |  283 | ` * information.` |
|     - |  284 | ` */` |
|     4 |  285 | `static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)` |
|     2 |  286 | `{` |
|     6 |  287 | `	SyBlob *pBlob = &pEntry->sOB;` |
|     - |  288 | `	sxi32 rc;` |
|     - |  289 | `	/* Flush contents */` |
|     6 |  290 | `	rc = PH7_OK;` |
|     6 |  291 | `	if( SyBlobLength(pBlob) > 0 ){` |
|     - |  292 | `		/* Call the VM output consumer */` |
|     6 |  293 | `		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);` |
|     - |  294 | `		/* Increment VM output counter */` |
|     6 |  295 | `		pVm->nOutputLen += SyBlobLength(pBlob);` |
|     6 |  296 | `		if( rc != PH7_ABORT ){` |
|     6 |  297 | `			rc = PH7_OK;` |
|     2 |  298 | `		}` |
|     2 |  299 | `	}` |
|     6 |  300 | `	if( bRelease ){` |
|     3 |  301 | `		VmObRestore(&(*pVm),pEntry);` |
|     2 |  302 | `	}else{` |
|     - |  303 | `		/* Reset the blob */` |
|     3 |  304 | `		SyBlobReset(pBlob);` |
|     - |  305 | `	}` |
|     6 |  306 | `	return rc;` |
|     2 |  307 | `}` |
|     - |  308 | `/*` |
|     - |  309 | ` * void ob_flush(void)` |
|     - |  310 | ` * void flush(void)` |
|     - |  311 | ` *  Flush (send) the output buffer.` |
|     - |  312 | ` * Parameter` |
|     - |  313 | ` *  None` |
|     - |  314 | ` * Return` |
|     - |  315 | ` *  No return value.` |
|     - |  316 | ` */` |
|     2 |  317 | `PH7_PRIVATE int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  318 | `{` |
|     3 |  319 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  320 | `	VmObEntry *pOb;` |
|     - |  321 | `	sxi32 rc;` |
|     - |  322 | `	/* Peek the top most OB entry */` |
|     3 |  323 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     3 |  324 | `	if( pOb == 0 ){` |
|     - |  325 | `		/* Empty stack,return immediately */` |
|   ! 0 |  326 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  327 | `		SXUNUSED(apArg);` |
|   ! 0 |  328 | `		return PH7_OK;` |
|     - |  329 | `	}` |
|     - |  330 | `	/* Flush contents */` |
|     3 |  331 | `	rc = VmObFlush(pVm,pOb,FALSE);` |
|     3 |  332 | `	return rc;` |
|     2 |  333 | `}` |
|     - |  334 | `/*` |
|     - |  335 | ` * bool ob_end_flush(void)` |
|     - |  336 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|     - |  337 | ` * Parameter` |
|     - |  338 | ` *  None` |
|     - |  339 | ` * Return` |
|     - |  340 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|     - |  341 | ` *  that you called the function without an active buffer or that for some reason` |
|     - |  342 | ` *  a buffer could not be deleted (possible for special buffer).` |
|     - |  343 | ` */` |
|     2 |  344 | `PH7_PRIVATE int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  345 | `{` |
|     3 |  346 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  347 | `	VmObEntry *pOb;` |
|     - |  348 | `	sxi32 rc;` |
|     - |  349 | `	/* Pop the top most OB entry */` |
|     3 |  350 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     3 |  351 | `	if( pOb == 0 ){` |
|     - |  352 | `		/* Empty stack,return FALSE */` |
|   ! 0 |  353 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  354 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  355 | `		SXUNUSED(apArg);` |
|   ! 0 |  356 | `		return PH7_OK;` |
|     - |  357 | `	}` |
|     - |  358 | `	/* Flush contents */` |
|     3 |  359 | `	rc = VmObFlush(pVm,pOb,TRUE);` |
|     - |  360 | `	/* Return true */` |
|     3 |  361 | `	ph7_result_bool(pCtx,1);` |
|     3 |  362 | `	return rc;` |
|     2 |  363 | `}` |
|     - |  364 | `/*` |
|     - |  365 | ` * void ob_implicit_flush([int $flag = true ])` |
|     - |  366 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|     - |  367 | ` *  Implicit flushing will result in a flush operation after every` |
|     - |  368 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|     - |  369 | ` * Parameter` |
|     - |  370 | ` *  $flag` |
|     - |  371 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|     - |  372 | ` * Return` |
|     - |  373 | ` *   Nothing` |
|     - |  374 | ` */` |
|     4 |  375 | `PH7_PRIVATE int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  376 | `{` |
|     - |  377 | `	/* NOTE: As of this version,this function is a no-op.` |
|     - |  378 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|     - |  379 | `	 */` |
|     2 |  380 | `	SXUNUSED(pCtx);` |
|     2 |  381 | `	SXUNUSED(nArg); /* cc warning */` |
|     2 |  382 | `	SXUNUSED(apArg);` |
|     5 |  383 | `	return PH7_OK;` |
|     1 |  384 | `}` |
|     - |  385 | `/*` |
|     - |  386 | ` * array ob_list_handlers(void)` |
|     - |  387 | ` *  Lists all output handlers in use.` |
|     - |  388 | ` * Parameter` |
|     - |  389 | ` *  None` |
|     - |  390 | ` * Return` |
|     - |  391 | ` *  This will return an array with the output handlers in use (if any).` |
|     - |  392 | ` */` |
|     2 |  393 | `PH7_PRIVATE int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  394 | `{` |
|     3 |  395 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  396 | `	ph7_value *pArray;` |
|     - |  397 | `	VmObEntry *aEntry;` |
|     - |  398 | `	ph7_value sVal;` |
|     - |  399 | `	sxu32 n;` |
|     3 |  400 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|     - |  401 | `		/* Empty stack,return null */` |
|   ! 0 |  402 | `		ph7_result_null(pCtx);` |
|   ! 0 |  403 | `		return PH7_OK;` |
|     - |  404 | `	}` |
|     - |  405 | `	/* Create a new array */` |
|     3 |  406 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  407 | `	if( pArray == 0 ){` |
|     - |  408 | `		/* Out of memory,return NULL */` |
|   ! 0 |  409 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  410 | `		SXUNUSED(apArg);` |
|   ! 0 |  411 | `		ph7_result_null(pCtx);` |
|   ! 0 |  412 | `		return PH7_OK;` |
|     - |  413 | `	}` |
|     3 |  414 | `	PH7_MemObjInit(pVm,&sVal);` |
|     - |  415 | `	/* Point to the installed OB entries */` |
|     3 |  416 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|     - |  417 | `	/* Perform the requested operation */` |
|     5 |  418 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){` |
|     3 |  419 | `		VmObEntry *pEntry = &aEntry[n];` |
|     - |  420 | `		/* Extract handler name */` |
|     3 |  421 | `		SyBlobReset(&sVal.sBlob);` |
|     3 |  422 | `		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){` |
|     - |  423 | `			/* Callback,dup it's name */` |
|   ! 0 |  424 | `			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);` |
|     3 |  425 | `		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){` |
|   ! 0 |  426 | `			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);` |
|   ! 0 |  427 | `		}else{` |
|     3 |  428 | `			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);` |
|     - |  429 | `		}` |
|     3 |  430 | `		sVal.iFlags = MEMOBJ_STRING;` |
|     - |  431 | `		/* Perform the insertion */` |
|     3 |  432 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|     2 |  433 | `	}` |
|     3 |  434 | `	PH7_MemObjRelease(&sVal);` |
|     - |  435 | `	/* Return the freshly created array */` |
|     3 |  436 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  437 | `	return PH7_OK;` |
|     2 |  438 | `}` |
|     - |  439 |  |
