# src/ph7/vm_exec.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2622/3137 lines (83.58%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `#include <math.h>` |
|        - |    8 | `/*` |
|        - |    9 | ` * Section:` |
|        - |   10 | ` *    The bytecode interpreter: VmByteCodeExec, its dispatch loop` |
|        - |   11 | ` *    (VmByteCodeExecBody) and the operand-stack growth + trampoline` |
|        - |   12 | ` *    call-finish machinery the loop is welded to. Split from vm.c after the` |
|        - |   13 | ` *    vm_ops_* extractions shrank the loop enough to fit its own unit; the` |
|        - |   14 | ` *    remaining inline arms (CALL, YIELD/YIELD_FROM, the finally family,` |
|        - |   15 | ` *    SPREAD, and the hot loads/stores/arith) share loop-invocation state` |
|        - |   16 | ` *    (pCallTop, ppBaseOwner/pnBaseCap, Suspend/SkipFuncBody/Done labels)` |
|        - |   17 | ` *    that must stay inside one function.` |
|        - |   18 | ` * Status:` |
|        - |   19 | ` *    Stable.` |
|        - |   20 | ` */` |
|        - |   21 | `/*` |
|        - |   22 | ` * OP_SPREAD stack growth (removes the old VM_STACK_GUARD expansion cap).` |
|        - |   23 | ` *` |
|        - |   24 | ` * The operand stack of the CURRENTLY-RUNNING activation is about to receive more` |
|        - |   25 | ` * spread elements than its remaining slack holds. Realloc the buffer so the` |
|        - |   26 | ` * expansion — and the rest of the body's normal pushes — fit, then fix up every` |
|        - |   27 | ` * pointer that aimed into the old buffer. Returns 1 on success (the caller may` |
|        - |   28 | ` * proceed with the expansion), 0 on OOM (the caller keeps the old buffer and` |
|        - |   29 | ` * raises the historical guard error, preserving pre-growth soundness).` |
|        - |   30 | ` *` |
|        - |   31 | ` * nNeed is the live-slot count the stack must hold after the pending expansion;` |
|        - |   32 | ` * the grown capacity adds VM_STACK_GUARD back on top so the remainder of the body` |
|        - |   33 | ` * keeps its slack. ONLY this activation's references move, and they are all here:` |
|        - |   34 | ` *   - the dispatch locals pStack/pTos (via *ppStack / *ppTos) and the boundary` |
|        - |   35 | ` *     copies in sState (pState->pStack/pTos + the tracked capacity nStackCap)` |
|        - |   36 | ` *   - the owner slot: for a trampoline callee, its record's pFrameStack and the` |
|        - |   37 | ` *     recycle capacity nStackCap; for the base activation (top-level, mini-program,` |
|        - |   38 | ` *     coroutine body, callback), *ppBaseOwner / *pnBaseCap — whatever storage the` |
|        - |   39 | ` *     native entry frees (a local, pVm->aOps, or pCtx->pStack/nStackCap)` |
|        - |   40 | ` *   - aSpreadRun[].pStart entries anchored in the OLD buffer (this call's earlier` |
|        - |   41 | ` *     spreads) — an unfixed pStart would desync PHP 8.1 named-arg replay after the` |
|        - |   42 | ` *     realloc. Enclosing activations own DISTINCT buffers, so their runs (pStart` |
|        - |   43 | ` *     outside [pOld, pOld+nOldCap)) are deliberately left untouched.` |
|        - |   44 | ` */` |
|      244 |   45 | `static int VmGrowOperandStack(ph7_vm *pVm, sxu32 nNeed,` |
|        - |   46 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |   47 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        4 |   48 | `{` |
|      248 |   49 | `	ph7_value *pOld = *ppStack;` |
|      248 |   50 | `	sxu32 nOldCap = pState->nStackCap;` |
|        - |   51 | `	sxu32 nNewCap, nReq, nMaxCap, i, nRun;` |
|        - |   52 | `	ph7_value *pNew;` |
|        - |   53 | `	VmSpreadRun *aRun;` |
|        - |   54 | `	/* Size the grown buffer to the post-expansion live depth (nNeed) PLUS the` |
|        - |   55 | `	 * activation's original full budget (nStackOrig = nMaxStack + VM_STACK_GUARD) as` |
|        - |   56 | `	 * headroom. That headroom is essential: only OP_SPREAD re-checks capacity, so any` |
|        - |   57 | `	 * ordinary arg pushes that FOLLOW this spread in the same call (e.g.` |
|        - |   58 | ``	 * `foo(...$big, a1..aN)`) must fit — and the rest of the body adds at most`` |
|        - |   59 | `	 * nMaxStack above the current point. Crucially the headroom is relative to the` |
|        - |   60 | `	 * ORIGINAL capacity, NOT the grown nOldCap: basing it on nOldCap would ratchet` |
|        - |   61 | `	 * capacity up on every spread (nOldCap already includes prior growth), leaking` |
|        - |   62 | `	 * without bound across statements that share one operand stack until it pins at` |
|        - |   63 | `	 * nMaxCap. nNeed resets between statements (the stack pops back), so this does not. */` |
|      248 |   64 | `	nReq = nNeed + pState->nStackOrig;` |
|      248 |   65 | `	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */` |
|      ! 0 |   66 | `		nReq = SXU32_HIGH;` |
|      ! 0 |   67 | `	}` |
|        - |   68 | `	/* SyMemBackendRealloc's size argument is sxu32, so the byte count` |
|        - |   69 | `	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack` |
|        - |   70 | `	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and` |
|        - |   71 | `	 * the init loop below would run off it. If the true requirement exceeds the` |
|        - |   72 | `	 * representable cap, treat it as OOM (the caller raises the guard error). */` |
|      248 |   73 | `	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);` |
|      248 |   74 | `	if( nReq > nMaxCap ){` |
|      ! 0 |   75 | `		return 0;` |
|        - |   76 | `	}` |
|      248 |   77 | `	if( nReq <= nOldCap ){` |
|      143 |   78 | `		return 1; /* already fits — no growth needed */` |
|        - |   79 | `	}` |
|      108 |   80 | `	nNewCap = nReq;` |
|        - |   81 | `	/* Amortize repeated spreads in one argument list: at least double, but never` |
|        - |   82 | `	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't` |
|        - |   83 | `	 * itself overflow.) */` |
|      108 |   84 | `	if( nNewCap < nOldCap * 2 ){` |
|      108 |   85 | `		sxu32 nDbl = nOldCap * 2;` |
|      108 |   86 | `		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }` |
|      108 |   87 | `		if( nNewCap < nDbl ){ nNewCap = nDbl; }` |
|       52 |   88 | `	}` |
|      160 |   89 | `	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,` |
|       52 |   90 | `		nNewCap * sizeof(ph7_value));` |
|      108 |   91 | `	if( pNew == 0 ){` |
|      ! 0 |   92 | `		return 0; /* OOM: caller keeps pOld and raises the guard error */` |
|        - |   93 | `	}` |
|        - |   94 | `	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */` |
|     6526 |   95 | `	for( i = nOldCap; i < nNewCap; i++ ){` |
|     6422 |   96 | `		PH7_MemObjInit(pVm, &pNew[i]);` |
|     6422 |   97 | `		pNew[i].nIdx = SXU32_HIGH;` |
|     3213 |   98 | `	}` |
|        - |   99 | `	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */` |
|      108 |  100 | `	*ppTos = pNew + (*ppTos - pOld);` |
|      108 |  101 | `	*ppStack = pNew;` |
|      108 |  102 | `	pState->pStack = pNew + (pState->pStack - pOld);` |
|      108 |  103 | `	pState->pTos = pNew + (pState->pTos - pOld);` |
|      108 |  104 | `	pState->nStackCap = nNewCap;` |
|      108 |  105 | `	if( pCallTop ){` |
|        - |  106 | `		/* This activation is a trampoline callee: its record owns the buffer. */` |
|       80 |  107 | `		pCallTop->sCall.pFrameStack = pNew;` |
|       80 |  108 | `		pCallTop->sCall.nStackCap = nNewCap;` |
|       41 |  109 | `	}else{` |
|        - |  110 | `		/* Base activation: the native entry frees *ppBaseOwner (and, for a` |
|        - |  111 | `		 * resumable coroutine, persists *pnBaseCap across suspend/resume). */` |
|       29 |  112 | `		if( ppBaseOwner ){ *ppBaseOwner = pNew; }` |
|       29 |  113 | `		if( pnBaseCap ){ *pnBaseCap = nNewCap; }` |
|        - |  114 | `	}` |
|        - |  115 | `	/* Re-anchor this activation's captured spread runs (pStart into the old` |
|        - |  116 | `	 * buffer). Enclosing-activation runs live in other buffers — leave them. */` |
|      108 |  117 | `	nRun = SySetUsed(&pVm->aSpreadRun);` |
|      108 |  118 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      108 |  119 | `	for( i = 0; i < nRun; i++ ){` |
|      ! 0 |  120 | `		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){` |
|      ! 0 |  121 | `			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);` |
|      ! 0 |  122 | `		}` |
|      ! 0 |  123 | `	}` |
|      108 |  124 | `	return 1;` |
|      126 |  125 | `}` |
|        - |  126 | `/*` |
|        - |  127 | ` * Ensure the running activation's operand stack can hold a spread of nEntry` |
|        - |  128 | ` * elements pushed at *ppTos (the source slot becomes the first element, so the` |
|        - |  129 | ` * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.` |
|        - |  130 | ` * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.` |
|        - |  131 | ` */` |
|      280 |  132 | `static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,` |
|        - |  133 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |  134 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        4 |  135 | `{` |
|        - |  136 | `	sxu32 nNeed;` |
|      284 |  137 | `	if( nEntry == 0 ){` |
|       39 |  138 | `		return 1; /* empty spread never grows the stack */` |
|        - |  139 | `	}` |
|      248 |  140 | `	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);` |
|      370 |  141 | `	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,` |
|      122 |  142 | `		pCallTop, ppBaseOwner, pnBaseCap);` |
|      144 |  143 | `}` |
|        - |  144 | `/*` |
|        - |  145 | ` * Terminal teardown of one VmByteCodeExec activation — the former` |
|        - |  146 | ` * Done/Suspend/Abort/Exception label bodies, one home (BYTECODE.md stage 1).` |
|        - |  147 | ` *` |
|        - |  148 | ` * SXRET_OK (Done): whenever the REAL body returns, its pending-return slot` |
|        - |  149 | ` * must be empty — the materialize at OP_DONE/OP_POP_EXCEPTION already moved` |
|        - |  150 | ` * the value into pResult and cleared bHasRet, so the clear is normally a` |
|        - |  151 | ` * no-op; it only fires on a path that reached Done with a stale slot,` |
|        - |  152 | ` * preventing a leak. The !bReturnPropagates guard is essential: a` |
|        - |  153 | ` * catch/finally MINI-PROGRAM runs in its body's own frame (VmLocalExec adds` |
|        - |  154 | ` * no frame), so pEntryFrame is that body — wiping its slot would destroy the` |
|        - |  155 | ` * return the body is about to take.` |
|        - |  156 | ` * PH7_SUSPEND: a generator/fiber body never suspends mid-completion of a` |
|        - |  157 | ` * catch/finally return, so its frame's slot is empty (nothing to clear) and` |
|        - |  158 | ` * its operand stack is preserved in place — the ctx owns it.` |
|        - |  159 | ` * PH7_ABORT / PH7_EXCEPTION: abnormal unwind — discard the body's pending` |
|        - |  160 | ` * return (an escaping exception supersedes it, per PHP) and release every` |
|        - |  161 | ` * live operand slot down to the activation's stack base.` |
|        - |  162 | ` */` |
|   156208 |  163 | `static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)` |
|        5 |  164 | `{` |
|    78104 |  165 | `	SXUNUSED(pVm);` |
|   156213 |  166 | `	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){` |
|   152621 |  167 | `		VmClearFrameReturn(pState->pEntryFrame);` |
|    76308 |  168 | `	}` |
|   156213 |  169 | `	SySetRelease(pArg);` |
|   156213 |  170 | `	if( rcTerm == PH7_ABORT \|\| rcTerm == PH7_EXCEPTION ){` |
|     2827 |  171 | `		while( pTos >= pState->pStack ){` |
|     1781 |  172 | `			PH7_MemObjRelease(pTos);` |
|     1781 |  173 | `			pTos--;` |
|        5 |  174 | `		}` |
|      523 |  175 | `	}` |
|   156213 |  176 | `	return rcTerm;` |
|        5 |  177 | `}` |
|        - |  178 | `/*` |
|        - |  179 | ` * Finish one user-function call at the "pop" boundary of the callee's` |
|        - |  180 | ` * activation: pop-time accounting (recursion depth, aSelf), by-ref-return` |
|        - |  181 | ` * fixup, callee-threw routing (inline resume / recorded resume / propagate),` |
|        - |  182 | ` * operand-stack free and frame teardown. Extracted verbatim from the OP_CALL` |
|        - |  183 | ` * epilogue (BYTECODE.md stage 1) so the stage-2 trampoline can run the same` |
|        - |  184 | ` * code when a record is popped at OP_DONE instead of after a native return.` |
|        - |  185 | ` * pCaller->pc / pCaller->pTos are authoritative across this boundary; the` |
|        - |  186 | ` * dispatch loop syncs its locals around the call. Returns PH7_OK (continue` |
|        - |  187 | ` * the caller, possibly at a redirected pc), PH7_ABORT, PH7_SUSPEND (the ctx` |
|        - |  188 | ` * state was re-saved at the caller's level) or PH7_EXCEPTION.` |
|        - |  189 | ` */` |
|    88366 |  190 | `static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)` |
|        5 |  191 | `{` |
|        - |  192 | `	ph7_value *pObj;` |
|        - |  193 | `	/* Decrement nesting level */` |
|    88371 |  194 | `	pVm->nRecursionDepth--;` |
|    88371 |  195 | `	if( pCallee->bSelfPushed ){` |
|        - |  196 | `		/* Pop class name */` |
|    24717 |  197 | `		(void)SySetPop(&pVm->aSelf);` |
|    12356 |  198 | `	}` |
|    88371 |  199 | `	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  200 | `		/* Return by reference,reflect that */` |
|       52 |  201 | `		if( pCallee->nLastRef != SXU32_HIGH ){` |
|       52 |  202 | `			VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pCallee->pFrame->sLocal);` |
|        - |  203 | `			sxu32 i;` |
|        - |  204 | `			/* Make sure the referenced object is not a local variable */` |
|       98 |  205 | `			for( i = 0 ; i < SySetUsed(&pCallee->pFrame->sLocal) ; ++i ){` |
|       48 |  206 | `				if( pCallee->nLastRef == aSlot[i].nIdx ){` |
|      ! 0 |  207 | `					pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pCallee->nLastRef);` |
|      ! 0 |  208 | `					if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  209 | `						VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  210 | `							"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  211 | `							&pCallee->pVmFunc->sName);` |
|      ! 0 |  212 | `					}` |
|      ! 0 |  213 | `					pCallee->nLastRef = SXU32_HIGH;` |
|      ! 0 |  214 | `					break;` |
|        - |  215 | `				}` |
|       25 |  216 | `			}` |
|       27 |  217 | `		}else{` |
|      ! 0 |  218 | `			if( (pCaller->pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  219 | `				VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  220 | `					"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  221 | `					&pCallee->pVmFunc->sName);` |
|      ! 0 |  222 | `			}` |
|        - |  223 | `		}` |
|       52 |  224 | `		pCaller->pTos->nIdx = pCallee->nLastRef;` |
|       25 |  225 | `	}` |
|    88371 |  226 | `	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  227 | `		/* The callee threw (or its finally threw past it). If an in-place catch` |
|        - |  228 | `		 * recorded a resume target owned by THIS caller's body, resume there and` |
|        - |  229 | `		 * consume the target (VmRecordedResume); when the catcher is an outer exec` |
|        - |  230 | `		 * — or this is a callback with no bytecode to resume into — propagate so` |
|        - |  231 | `		 * the owning exec lands. This replaces the old "is the caller's parent a` |
|        - |  232 | `		 * resumable try frame" test, which resumed at the caller's OWN try even` |
|        - |  233 | `		 * when the finally's throw was caught further out, losing that catch's` |
|        - |  234 | `		 * return (ROOT B, face c). */` |
|        - |  235 | `		sxi32 iResumePc;` |
|     1077 |  236 | `		VmFrame *pParentFrame = pCallee->pFrame->pParent;` |
|     1077 |  237 | `		if( !pCaller->is_callback && pVm->pInlineInstr == (void *)pCaller->aInstr ){` |
|        - |  238 | `			/* ROOT C: the callee's throw was caught by an inline try in THIS caller` |
|        - |  239 | `			 * (generator body). Drain the operand stack (incl. the unwritten result` |
|        - |  240 | `			 * slot) to the try's base and land at its catch/finally. */` |
|        5 |  241 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iInlineDrain ){` |
|        3 |  242 | `				PH7_MemObjRelease(pCaller->pTos);` |
|        3 |  243 | `				pCaller->pTos--;` |
|        1 |  244 | `			}` |
|        3 |  245 | `			pCaller->pc = (sxi32)pVm->iInlinePc - 1;` |
|        3 |  246 | `			pVm->pInlineInstr = 0;` |
|        3 |  247 | `			rc = PH7_OK;` |
|     1076 |  248 | `		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){` |
|        - |  249 | `			/* Pop the result */` |
|      607 |  250 | `			VmPopOperand(&pCaller->pTos,1);` |
|      607 |  251 | `			pCaller->pc = iResumePc;` |
|      607 |  252 | `			rc = PH7_OK;` |
|      306 |  253 | `		}else{` |
|      473 |  254 | `			if( pParentFrame->pParent ){` |
|      469 |  255 | `				rc = PH7_EXCEPTION;` |
|      237 |  256 | `			}else{` |
|        - |  257 | `				/* Continue normal execution */` |
|        6 |  258 | `				rc = PH7_OK;` |
|        - |  259 | `			}` |
|        - |  260 | `		}` |
|      536 |  261 | `	}` |
|        - |  262 | `	/* Recycle the operand stack for the next same-size call (BYTECODE stage 7),` |
|        - |  263 | `	 * or free it if the pool is full. Its allocated size is tracked in` |
|        - |  264 | `	 * pCallee->nStackCap (nMaxStack + VM_STACK_GUARD, or larger if an OP_SPREAD grew` |
|        - |  265 | `	 * it) — exactly what the buffer holds. (NULL when the function body was skipped.)` |
|        - |  266 | `	 *` |
|        - |  267 | `	 * Never on rc == PH7_SUSPEND: that path (unreachable in the stage-4 model,` |
|        - |  268 | `	 * where a deep suspend parks its whole record segment before reaching here)` |
|        - |  269 | `	 * would leave the callee stack owned by the suspended ctx, so recycling it` |
|        - |  270 | `	 * would hand a live fiber's operand stack to the next call. The guard keeps` |
|        - |  271 | `	 * that invariant explicit and robust to future coroutine changes. */` |
|    88371 |  272 | `	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){` |
|        - |  273 | `		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee` |
|        - |  274 | `		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly` |
|        - |  275 | `		 * the allocated slot count either way. */` |
|    88243 |  276 | `		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap);` |
|    44168 |  277 | `	}` |
|        - |  278 | `	/* Leave the frame */` |
|    88371 |  279 | `	VmLeaveFrame(&(*pVm));` |
|    88371 |  280 | `	if( rc == PH7_ABORT ){` |
|      332 |  281 | `		return PH7_ABORT;` |
|        - |  282 | `	}` |
|    88043 |  283 | `	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  284 | `		/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  285 | `		 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  286 | `		 * overwriting the state saved by the inner level.` |
|        - |  287 | `		 * pTos points to the result slot (not yet written).` |
|        - |  288 | `		 * Save nTos one below so resume pushes at the result slot. */` |
|      ! 0 |  289 | `		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);` |
|      ! 0 |  290 | `		return PH7_SUSPEND;` |
|        - |  291 | `	}` |
|    88043 |  292 | `	if( rc == PH7_EXCEPTION ){` |
|      469 |  293 | `		return PH7_EXCEPTION;` |
|        - |  294 | `	}` |
|    87579 |  295 | `	return PH7_OK;` |
|    44237 |  296 | `}` |
|        - |  297 | `/*` |
|        - |  298 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  299 | ` *` |
|        - |  300 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  301 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  302 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  303 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  304 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  305 | ` * then the program execution is halted.` |
|        - |  306 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  307 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  308 | ` * or to reset the VM to it's initial state.` |
|        - |  309 | ` */` |
|        - |  310 | `static sxi32 VmByteCodeExecBody(ph7_vm *pVm,VmInstr *aInstr,ph7_value *pStack,int nTos,` |
|        - |  311 | `	ph7_value *pResult,sxu32 *pLastRef,int is_callback,sxi32 nPc,` |
|        - |  312 | `	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,VmParkedSegment *pAdoptSegment,` |
|        - |  313 | `	ph7_value **ppBaseOwner,sxu32 *pnBaseCap,sxu32 nStackOrig);` |
|        - |  314 | `/*` |
|        - |  315 | ` * Native-nesting guard around the executor. PHP->PHP calls run iteratively` |
|        - |  316 | ` * (the stage-2 trampoline), but every OTHER (re-)entry — mini-programs,` |
|        - |  317 | ` * C->PHP callbacks, ctx start/resume, eval/include — is still one real C` |
|        - |  318 | ` * activation of VmByteCodeExecBody. nMaxDepth no longer bounds them (it is` |
|        - |  319 | ` * PHP call depth, raisable to memory-bound values since the clamp removal),` |
|        - |  320 | ` * so this counter is what actually protects the C stack: recursive` |
|        - |  321 | ` * eval/include towers, nested coroutine-resume chains and self-recursive` |
|        - |  322 | ` * C-callback compositions hit a clean fatal instead of overflowing. The limit` |
|        - |  323 | ` * lives in pVm->nMaxNativeDepth — a per-platform default (256 host / 16 small-` |
|        - |  324 | ` * stack embedders, VmInit) overridable via PH7_VM_CONFIG_NATIVE_DEPTH. This is` |
|        - |  325 | ` * still a coarse frame-count net rather than php's stack-byte measurement, so` |
|        - |  326 | ` * the host default is conservative — well below the old config clamp's <1024` |
|        - |  327 | ` * ceiling so it holds on the fattest frames (the callback path drags in` |
|        - |  328 | ` * usort/mergesort/trampoline C frames per re-entry, and instrumented builds` |
|        - |  329 | ` * inflate every frame), while far beyond any realistic eval/include/callback` |
|        - |  330 | ` * nesting.` |
|        - |  331 | ` */` |
|   156566 |  332 | `PH7_PRIVATE sxi32 VmByteCodeExec(` |
|        - |  333 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  334 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  335 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  336 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  337 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  338 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  339 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  340 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  341 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  342 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|        - |  343 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|        - |  344 | `	ph7_value **ppBaseOwner, /* Storage slot the native entry frees for this invocation's BASE (pCallTop==0) operand stack — a local, pVm->aOps or pCtx->pStack. An OP_SPREAD that grows the base stack writes the new pointer here so the entry frees the right buffer. */` |
|        - |  345 | `	sxu32 *pnBaseCap, /* Storage for the base stack's capacity (resumable coroutines persist it across suspend/resume); updated alongside *ppBaseOwner on base-stack growth. Also the initial capacity read at entry. */` |
|        - |  346 | `	sxu32 nStackOrig /* The base stack's ORIGINAL (ungrown) allocation size. Unlike *pnBaseCap (which is the CURRENT, possibly-grown capacity on a coroutine resume), this is fixed, so OP_SPREAD growth headroom stays bounded across resumes. */` |
|        - |  347 | `	)` |
|        5 |  348 | `{` |
|        - |  349 | `	sxi32 rc;` |
|        - |  350 | `	sxi32 nSavedBrc;` |
|        - |  351 | `	sxu32 nSavedLine;` |
|   156571 |  352 | `	if( VmNativeNestingExceeded(pVm) ){` |
|        5 |  353 | `		return VmNativeNestingFatal(pVm);` |
|        - |  354 | `	}` |
|        - |  355 | `	/* A fresh native exec entered while a C-boundary throw is parked` |
|        - |  356 | `	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the` |
|        - |  357 | `	 * very opcode that swallowed the throw) must run CLEAN: the parked status` |
|        - |  358 | `	 * belongs to the interrupted outer exec's fetch-point router, not to this` |
|        - |  359 | `	 * one. Save+clear on entry, merge back on exit — the outer status is` |
|        - |  360 | `	 * restored unless this exec parked its own unconsumed (newer) one, with` |
|        - |  361 | `	 * PH7_ABORT dominating either way. */` |
|   156567 |  362 | `	nSavedBrc = pVm->nBoundaryRc;` |
|   156567 |  363 | `	pVm->nBoundaryRc = 0;` |
|        - |  364 | `	/* The executing source line belongs to the ACTIVATION. A nested body -- a called` |
|        - |  365 | `	 * function, but equally an attribute-default or default-argument mini-program --` |
|        - |  366 | `	 * runs its own bytecode with its own lines, so it must not leave the caller` |
|        - |  367 | ``	 * reporting the callee's position: `new Exception` stamped line 1 because the`` |
|        - |  368 | ``	 * class's `protected $message = '';` default ran (from the embedded chunk) between`` |
|        - |  369 | `	 * OP_NEW and the stamp. Save on entry, restore on exit. */` |
|   156567 |  370 | `	nSavedLine = pVm->nCurLine;` |
|   156567 |  371 | `	pVm->nVmExecDepth++;` |
|   234848 |  372 | `	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,` |
|    78281 |  373 | `		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);` |
|   156567 |  374 | `	pVm->nVmExecDepth--;` |
|   156567 |  375 | `	pVm->nCurLine = nSavedLine;` |
|   156567 |  376 | `	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT \|\| pVm->nBoundaryRc == 0) ){` |
|       16 |  377 | `		pVm->nBoundaryRc = nSavedBrc;` |
|        6 |  378 | `	}` |
|   156567 |  379 | `	return rc;` |
|    78288 |  380 | `}` |
|   156562 |  381 | `static sxi32 VmByteCodeExecBody(` |
|        - |  382 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  383 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  384 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  385 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  386 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  387 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  388 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  389 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  390 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  391 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|        - |  392 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|        - |  393 | `	ph7_value **ppBaseOwner, /* Base (pCallTop==0) operand-stack owner slot the native entry frees; OP_SPREAD growth of the base stack writes the new pointer here (see VmGrowOperandStack). */` |
|        - |  394 | `	sxu32 *pnBaseCap, /* Base stack capacity (persisted across coroutine suspend/resume); read for the initial capacity and updated on base-stack growth. */` |
|        - |  395 | `	sxu32 nStackOrig /* Base stack's ORIGINAL (ungrown) allocation size — the fixed headroom reference for OP_SPREAD growth (see nStackOrig in VmExecState). */` |
|        - |  396 | `	)` |
|        5 |  397 | `{` |
|        - |  398 | `	VmInstr *pInstr;` |
|        - |  399 | `	ph7_value *pTos;` |
|        - |  400 | `	SySet aArg;` |
|   156567 |  401 | `	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record` |
|        - |  402 | `	                            * stack (BYTECODE stage 2); NULL = executing the` |
|        - |  403 | `	                            * bottom activation. */` |
|        - |  404 | `	VmExecState sState; /* This activation's boundary state (BYTECODE.md stage 1):` |
|        - |  405 | `	                     * everything a suspended/nested activation must restore.` |
|        - |  406 | `	                     * pc/pTos stay in locals for the hot loop and are synced` |
|        - |  407 | `	                     * into sState only around the call epilogue (stage 2 turns` |
|        - |  408 | `	                     * that boundary into an explicit record push/pop). */` |
|        - |  409 | `	sxi32 pc;` |
|        - |  410 | `	sxi32 rc;` |
|   156567 |  411 | `	sState.aInstr = aInstr;` |
|   156567 |  412 | `	sState.pStack = pStack;` |
|   156567 |  413 | `	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */` |
|   156567 |  414 | `	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */` |
|   156567 |  415 | `	sState.pResult = pResult;` |
|   156567 |  416 | `	sState.pLastRef = pLastRef;` |
|   156567 |  417 | `	sState.pEnforceRetFunc = pEnforceRetFunc;` |
|   156567 |  418 | `	sState.is_callback = (sxu8)(is_callback ? 1 : 0);` |
|   156567 |  419 | `	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);` |
|        - |  420 | `	/* Argument container */` |
|   156567 |  421 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|   156567 |  422 | `	if( nTos < 0 ){` |
|   141519 |  423 | `		pTos = &pStack[-1];` |
|    70762 |  424 | `	}else{` |
|    15053 |  425 | `		pTos = &pStack[nTos];` |
|        - |  426 | `	}` |
|   156567 |  427 | `	sState.pTos = pTos;` |
|   156567 |  428 | `	sState.pc = nPc;` |
|        - |  429 | `	/* Finally-drain base. For a resumed generator/fiber TOP-LEVEL body, its own` |
|        - |  430 | `	 * exception handlers were just re-published above the caller depth` |
|        - |  431 | `	 * (VmRestoreCtxState), so the live SySetUsed over-counts; take the` |
|        - |  432 | `	 * caller-depth base recorded on the ctx instead.` |
|        - |  433 | `	 *` |
|        - |  434 | `	 * The discriminator is the OPERAND STACK, not the frame: the body exec runs on` |
|        - |  435 | `	 * the ctx's own pStack, whereas every nested frame-less mini-program run within` |
|        - |  436 | `	 * it — a default-argument / constructor trampoline, or a catch/finally body via` |
|        - |  437 | `	 * VmLocalExec — runs on a FRESH operand stack while SHARING pVm->pFrame (those` |
|        - |  438 | `	 * mini-programs do not push a VM frame). A pFrame-only guard therefore misfires` |
|        - |  439 | `	 * for such a mini-program and hands it the generator's low caller-base; its` |
|        - |  440 | `	 * terminal OP_DONE then drains VmDrainFinally down to that base, tearing down a` |
|        - |  441 | ``	 * live try that the surrounding finally had just opened (e.g. `finally { try {`` |
|        - |  442 | ``	 * throw new E(); } catch (E) {} }` in a generator: the `new E()` trampoline's`` |
|        - |  443 | `	 * OP_DONE popped the inner try before its OP_THROW ran, so the throw escaped` |
|        - |  444 | `	 * uncaught). Requiring pStack == pCtx->pStack pins the override to the resumed` |
|        - |  445 | `	 * body itself; nested mini-programs fall through to the correct live depth. */` |
|   156562 |  446 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame` |
|     2814 |  447 | `	 && pStack == pVm->pActiveCtx->pStack ){` |
|     1805 |  448 | `		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;` |
|      905 |  449 | `	}else{` |
|   154767 |  450 | `		sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|        - |  451 | `	}` |
|   156567 |  452 | `	sState.pEntryFrame = pVm->pFrame;` |
|   156567 |  453 | `	pc = nPc;` |
|        - |  454 | `	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call` |
|        - |  455 | `	 * adopts its parked record segment here. VmResumeCtx hands the segment in` |
|        - |  456 | `	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a` |
|        - |  457 | `	 * nested mini-program/callback run inside the resumed body — after it rebased` |
|        - |  458 | `	 * the segment's exception floors and pushed the resume value into the innermost` |
|        - |  459 | `	 * stack. Locals switch to the innermost activation so the dispatch loop` |
|        - |  460 | `	 * continues inside the callee; the record chain is restored so its completion` |
|        - |  461 | `	 * unwinds back through the body. */` |
|   156567 |  462 | `	if( pAdoptSegment ){` |
|      149 |  463 | `		VmParkedSegment *pSeg = pAdoptSegment;` |
|      149 |  464 | `		pCallTop = pSeg->pCallTop;` |
|      149 |  465 | `		sState = pSeg->sState;` |
|      149 |  466 | `		aInstr = sState.aInstr;` |
|      149 |  467 | `		pStack = sState.pStack;` |
|        - |  468 | `		/* pc is already nPc (== pCtx->pc, the innermost's post-suspend pc) from the` |
|        - |  469 | `		 * init above; only the stack/top move to the innermost activation. */` |
|      149 |  470 | `		pTos = &pStack[nTos];   /* nTos == pCtx->nTos — innermost, resume value pushed */` |
|      149 |  471 | `		SyMemBackendFree(&pVm->sAllocator,pSeg); /* holder only; its contents are now live */` |
|       72 |  472 | `	}` |
|        - |  473 | `/*` |
|        - |  474 | ` * Route an enforcement helper's (or yield-from delegate's) return code from inside` |
|        - |  475 | ` * the main switch: proceed on SXRET_OK, abort on PH7_ABORT, and on PH7_EXCEPTION` |
|        - |  476 | ` * resume at the landing pad of the body that actually caught the exception in place` |
|        - |  477 | ` * (VmRecordedResume) or, when it was caught by an outer exec, unwind out of the VM` |
|        - |  478 | ` * loop. Replaces the old "jump to pVm->pFrame's nearest iExceptionJump" which ran` |
|        - |  479 | ` * the statement after the try even when the catch was at an enclosing frame (ROOT B,` |
|        - |  480 | `` * face b — `yield from` over a throwing sub-generator).`` |
|        - |  481 | ` */` |
|        - |  482 | `/* Dispatch-routing macros: bodies live in vm_dispatch.h, written against the` |
|        - |  483 | ` * VM_EXIT_* primitives. Here they bind to the loop's own control flow. */` |
|        - |  484 | `#define VM_EXIT_BREAK break` |
|        - |  485 | `#define VM_EXIT_ABORT goto Abort` |
|        - |  486 | `#define VM_EXIT_EXCEPTION goto Exception` |
|        - |  487 | `#include "vm_dispatch.h"` |
|        - |  488 | `	/* Generator::throw() inject-at-yield: when this invocation is a resumed generator/fiber` |
|        - |  489 | `	 * body carrying a pending injected exception, raise it HERE — once, before the dispatch` |
|        - |  490 | `	 * loop (pc is already at the resume point) — so the existing OP_THROW route` |
|        - |  491 | `	 * (VmThrowException + VmRecordedResume) lands it at the generator's own try/catch landing` |
|        - |  492 | `	 * pad WITHOUT reconstructing the suspended exception frame on pVm->pFrame. Because` |
|        - |  493 | `	 * pVm->pFrame stays the body, the return/finally/sRet subsystem is untouched (this is why` |
|        - |  494 | `	 * the reverted frame-reconstruction approach's regression cannot recur). Behaviorally` |
|        - |  495 | ``	 * identical to a `throw` executed at the yield point: if the generator's own try catches`` |
|        - |  496 | `	 * it we resume after the try; otherwise it propagates to the throw() caller (ROOT B lands` |
|        - |  497 | `	 * the caller's handler) and the ctx closes. pInjected is one-shot and only meaningful at` |
|        - |  498 | `	 * the resume pc, so this is checked once at entry — NOT per-instruction — keeping the hot` |
|        - |  499 | `	 * dispatch loop untouched for all normal code. The pFrame gate keeps a nested call / catch` |
|        - |  500 | `	 * mini-program sharing the ctx (a separate VmByteCodeExec entry) from re-firing.` |
|        - |  501 | `	 *` |
|        - |  502 | ``	 * Exception: when this body is suspended mid `yield from` over an inner Generator`` |
|        - |  503 | `	 * (iDelegateState==3), a Generator::throw() on the OUTER generator must be forwarded` |
|        - |  504 | `	 * INTO the delegate (PHP yield-from transparency), not raised here. Leave pInjected` |
|        - |  505 | `	 * set and skip; the resume pc is that OP_YIELD_FROM, which consumes and forwards it. */` |
|   156562 |  506 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected` |
|     1556 |  507 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       63 |  508 | `	 && pVm->pActiveCtx->iDelegateState != 3 ){` |
|       58 |  509 | `		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;` |
|        - |  510 | `		VmFrame *pThrowFrame;` |
|        - |  511 | `		sxi32 iResumePc;` |
|       58 |  512 | `		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */` |
|        - |  513 | `		/* Raising the injection at the yield-from point abandons any array/Iterator` |
|        - |  514 | `		 * delegation in progress (state 1/2 — state 3 was forwarded, not raised here).` |
|        - |  515 | `		 * Tear the delegate down so that if the generator's own try catches this and` |
|        - |  516 | ``		 * runs on to a LATER `yield from`, that opcode classifies its operand fresh`` |
|        - |  517 | `		 * instead of resuming this now-stale delegate cursor. */` |
|       58 |  518 | `		if( pVm->pActiveCtx->iDelegateState != 0 ){` |
|        3 |  519 | `			PH7_MemObjRelease(&pVm->pActiveCtx->sDelegate);` |
|        3 |  520 | `			pVm->pActiveCtx->pDelegateNode = 0;` |
|        3 |  521 | `			pVm->pActiveCtx->iDelegateState = 0;` |
|        1 |  522 | `		}` |
|       58 |  523 | `		pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       58 |  524 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|       58 |  525 | `		rc = VmThrowException(&(*pVm),pInj);` |
|       58 |  526 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  527 | `			goto Abort;` |
|        - |  528 | `		}` |
|       58 |  529 | `		if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - |  530 | `			/* ROOT C: the inject was caught by an inline try in THIS generator. Drain the` |
|        - |  531 | `			 * abandoned mid-expression operands and land at the catch/finally body. This is` |
|        - |  532 | `			 * the pre-loop path (the first fetch uses pc directly), so no -1 adjustment. */` |
|       92 |  533 | `			while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|       48 |  534 | `				PH7_MemObjRelease(pTos);` |
|       48 |  535 | `				pTos--;` |
|        4 |  536 | `			}` |
|       48 |  537 | `			pc = (sxi32)pVm->iInlinePc;` |
|       48 |  538 | `			pVm->pInlineInstr = 0;` |
|       34 |  539 | `		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - |  540 | `			/* Caught by THIS generator's own try. (VmRecordedResume returns FALSE unless a` |
|        - |  541 | `			 * catch recorded a resume target for this exec, so rc need not be pre-checked;` |
|        - |  542 | `			 * unlike OP_THROW there is no lexical-try fallthrough here — the else just` |
|        - |  543 | `			 * propagates.) Drain the abandoned mid-expression operand slots back to the` |
|        - |  544 | `			 * catching try's base, then land at its pad (iResumePc is landing-1 for the` |
|        - |  545 | `			 * dispatcher's trailing pc++; the loop below fetches at pc with no leading pc++,` |
|        - |  546 | `			 * so add 1 to land on the pad itself). */` |
|      ! 0 |  547 | `			while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      ! 0 |  548 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  549 | `				pTos--;` |
|      ! 0 |  550 | `			}` |
|      ! 0 |  551 | `			pc = iResumePc + 1;` |
|      ! 0 |  552 | `		}else{` |
|        - |  553 | `			/* Not caught in this generator (no match, or caught by an outer/caller frame` |
|        - |  554 | `			 * that ROOT B will land at its own OP_CALL site): propagate out so the ctx` |
|        - |  555 | `			 * closes and the caller sees the exception. */` |
|       12 |  556 | `			goto Exception;` |
|        - |  557 | `		}` |
|       22 |  558 | `	}` |
|        - |  559 | `	/* Force-close entry (VmCloseCtx): a suspended generator being destroyed runs its` |
|        - |  560 | ``	 * pending `finally` blocks. Instead of resuming at the yield, redirect straight`` |
|        - |  561 | ``	 * into the innermost open try's finally as if a `return` had crossed every`` |
|        - |  562 | `	 * enclosing finally (mirrors OP_SET_FINALLY_RET; OP_END_FINALLY then threads the` |
|        - |  563 | `	 * PH7_FA_RETURN out through the whole chain and completes the body). Gate on the` |
|        - |  564 | `	 * BODY bytecode (aInstr == the function program) so nested mini-programs sharing` |
|        - |  565 | `	 * this ctx/frame never re-fire it; bClosing stays set so OP_YIELD can reject a` |
|        - |  566 | `	 * yield reached inside one of these finallys. */` |
|   156552 |  567 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing` |
|     1541 |  568 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       43 |  569 | `	 && aInstr == (VmInstr *)SySetBasePtr(&pVm->pActiveCtx->pFunc->aByteCode) ){` |
|        - |  570 | `		VmFinallyAction sAct;` |
|       42 |  571 | `		sxu32 iFpc = 0;` |
|       42 |  572 | `		int nCross = -1; /* cross every enclosing finally of this body */` |
|       42 |  573 | `		SyZero(&sAct,sizeof(sAct));` |
|       42 |  574 | `		sAct.eKind = PH7_FA_RETURN;` |
|       42 |  575 | `		sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       42 |  576 | `		PH7_MemObjInit(pVm,&sAct.sRet); /* discarded return; getReturn() is moot post-close */` |
|       42 |  577 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|       21 |  578 | `			sAct.nCross = nCross;` |
|       21 |  579 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       21 |  580 | `			pc = (sxi32)iFpc; /* pre-loop: the first fetch uses pc directly (no -1) */` |
|       11 |  581 | `		}else{` |
|        - |  582 | `			/* No open try had a finally: nothing to run, complete the body. */` |
|       21 |  583 | `			PH7_MemObjRelease(&sAct.sRet);` |
|       21 |  584 | `			goto Done;` |
|        - |  585 | `		}` |
|       10 |  586 | `	}` |
|        - |  587 | `	/* Execute as much as we can */` |
|  8373192 |  588 | `	for(;;){` |
|      ! 0 |  589 | `VmLoopFetch:` |
|        - |  590 | `		/* C-boundary throw routing (band A #1): a PHP callee invoked from a C` |
|        - |  591 | `		 * site with no status channel — a __toString/__toInt cast, __get/__set/` |
|        - |  592 | `		 * offsetGet/offsetSet, __clone, __destruct, a user callback inside a` |
|        - |  593 | `		 * builtin — raised, and the site continued with a fallback value; the` |
|        - |  594 | `		 * invocation boundary parked the status here (VmBoundaryPark). Route it` |
|        - |  595 | `		 * exactly as the throw site's dispatch macro would have: abort, land at` |
|        - |  596 | `		 * an inline-try redirect, resume at a recorded in-place catch (draining` |
|        - |  597 | `		 * the abandoned mid-expression operands to the catching try's base), or` |
|        - |  598 | `		 * propagate out of this exec. Checked at the fetch point, so a swallowed` |
|        - |  599 | `		 * throw outlives at most the C remainder of ONE opcode instead of` |
|        - |  600 | `		 * silently resuming the surrounding PHP code with a bogus value.` |
|        - |  601 | `		 * The pending write-back sweep shares this one guard so the hot` |
|        - |  602 | `		 * no-hooks path pays a single predicted branch per fetch. */` |
| 16911492 |  603 | `		if( pVm->nBoundaryRc != 0 \|\| SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      745 |  604 | `			if( pVm->nBoundaryRc != 0 ){` |
|      177 |  605 | `				sxi32 rcBr = pVm->nBoundaryRc;` |
|      177 |  606 | `				pVm->nBoundaryRc = 0;` |
|      177 |  607 | `				if( rcBr == PH7_ABORT ){` |
|       68 |  608 | `					goto Abort;` |
|        - |  609 | `				}` |
|      110 |  610 | `				if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - |  611 | `					/* Caught by an inline try (generator body) THIS exec owns: drain` |
|        - |  612 | `					 * and land (pre-fetch path: pc is used directly, no trailing ++). */` |
|        5 |  613 | `					while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|        3 |  614 | `						PH7_MemObjRelease(pTos);` |
|        3 |  615 | `						pTos--;` |
|        1 |  616 | `					}` |
|        3 |  617 | `					pc = (sxi32)pVm->iInlinePc;` |
|        3 |  618 | `					pVm->pInlineInstr = 0;` |
|        2 |  619 | `				}else{` |
|        - |  620 | `					sxi32 iBrPc;` |
|      108 |  621 | `					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){` |
|        - |  622 | `						/* Caught in place by a try THIS exec owns: drain the abandoned` |
|        - |  623 | `						 * operands to the catching try's base and land at its pad` |
|        - |  624 | `						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;` |
|        - |  625 | `						 * pre-fetch here, so +1 lands on the pad itself). */` |
|      168 |  626 | `						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|       82 |  627 | `							PH7_MemObjRelease(pTos);` |
|       82 |  628 | `							pTos--;` |
|        2 |  629 | `						}` |
|       88 |  630 | `						pc = iBrPc + 1;` |
|       45 |  631 | `					}else{` |
|        - |  632 | `						/* Caught by an outer exec (or uncaught-with-report pending):` |
|        - |  633 | `						 * unwind out of this exec; the owner's macros/router land it. */` |
|       22 |  634 | `						goto Exception;` |
|        - |  635 | `					}` |
|        - |  636 | `				}` |
|       44 |  637 | `			}` |
|        - |  638 | `			/* Stale write-back sweep: a pending entry whose OWNING activation is` |
|        - |  639 | `			 * fetching any pc outside its armed window [nJmpPc, nPc] is dead — for` |
|        - |  640 | `			 * an RMW entry (window = the modify op alone) a routed throw abandoned` |
|        - |  641 | `			 * the arming statement mid-flight; for a ??= entry either a throw` |
|        - |  642 | `			 * abandoned the RHS or the short-circuit jump landed past the` |
|        - |  643 | `			 * OP_NULLC_STORE (the assign is skipped). Drop it — no set dispatch` |
|        - |  644 | `			 * (php: the throw/skip discards the write). A nested exec (even a` |
|        - |  645 | `			 * recursive one over the same bytecode) has a different operand-stack` |
|        - |  646 | `			 * base and leaves enclosing entries alone; entries below a live top` |
|        - |  647 | `			 * are reached as the drops expose them. */` |
|      666 |  648 | `			while( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      569 |  649 | `				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      568 |  650 | `				if( pTopRmw->pOwnerStack != (void *)pStack \|\| pTopRmw->pInstrs != (void *)aInstr` |
|       89 |  651 | `				 \|\| ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){` |
|      281 |  652 | `					break; /* not ours, or legitimately in flight */` |
|        - |  653 | `				}` |
|        9 |  654 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 |  655 | `			}` |
|      328 |  656 | `		}` |
|        - |  657 | `		/* Fetch the instruction to execute */` |
| 16911406 |  658 | `		pInstr = &aInstr[pc];` |
| 16911406 |  659 | `		if( pInstr->nLine ){` |
|        - |  660 | `			/* Publish the source position for diagnostics, debug_backtrace() and` |
|        - |  661 | `			 * Throwable. Instructions the compiler could not attribute (nLine 0)` |
|        - |  662 | `			 * leave the last known line standing rather than reporting line 0. */` |
| 16698036 |  663 | `			pVm->nCurLine = pInstr->nLine;` |
|  8354667 |  664 | `		}` |
| 16911406 |  665 | `		rc = SXRET_OK;` |
|        - |  666 | `/*` |
|        - |  667 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  668 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  669 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  670 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  671 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  672 | ` */` |
| 16911406 |  673 | `		switch(pInstr->iOp){` |
|        - |  674 | `/*` |
|        - |  675 | ` * DONE: P1 * *` |
|        - |  676 | ` *` |
|        - |  677 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  678 | ` * and return immediately.` |
|        - |  679 | ` */` |
|   120281 |  680 | `case PH7_OP_DONE:` |
|   240665 |  681 | `	if( pInstr->iP2 && sState.bReturnPropagates ){` |
|        - |  682 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - |  683 | `		 * onto the body frame this catch/finally returns from (skip the transparent` |
|        - |  684 | `		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION` |
|        - |  685 | `		 * materializes it into sState.pResult. Drain any finally opened within this body` |
|        - |  686 | `		 * first (nested try/finally inside the catch), which may overwrite the same` |
|        - |  687 | `		 * frame's slot (finally-over-catch). */` |
|      189 |  688 | `		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|      189 |  689 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      187 |  690 | `			PH7_MemObjStore(pTos,&pTgt->sRet);` |
|      187 |  691 | `			VmPopOperand(&pTos,1);` |
|       96 |  692 | `		}else{` |
|        3 |  693 | ``			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */`` |
|        - |  694 | `		}` |
|      189 |  695 | `		pTgt->bHasRet = 1;` |
|      189 |  696 | `		pTgt->nRetGen++;` |
|      189 |  697 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|      189 |  698 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  699 | `			goto Abort;` |
|        - |  700 | `		}` |
|      189 |  701 | `		if( rc == PH7_EXCEPTION ){` |
|        - |  702 | `			/* A drained finally threw past itself — it discards this return. */` |
|      ! 0 |  703 | `			goto Exception;` |
|        - |  704 | `		}` |
|      189 |  705 | `		goto Done;` |
|        - |  706 | `	}` |
|        - |  707 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  708 | `	 * the fiber start/resume paths) set sState.pEnforceRetFunc, so this branch is` |
|        - |  709 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  710 | `	 * callback trampolines, and the main script. */` |
|   240476 |  711 | `	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)` |
|     9207 |  712 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  713 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  714 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  715 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  716 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  717 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  718 | `		 * exception. */` |
|     9205 |  719 | `		ph7_value *pRetVal = 0;` |
|     9205 |  720 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|     9089 |  721 | `			pRetVal = pTos;` |
|     4542 |  722 | `		}` |
|     9205 |  723 | `		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);` |
|     9205 |  724 | `		if( rc == PH7_ABORT ) goto Abort;` |
|     9199 |  725 | `		if( rc == PH7_EXCEPTION ){` |
|       21 |  726 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|       19 |  727 | `				PH7_MemObjRelease(pTos);` |
|       19 |  728 | `				pTos--;` |
|        8 |  729 | `			}` |
|       21 |  730 | `			goto Exception;` |
|        - |  731 | `		}` |
|        - |  732 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  733 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  734 | `		 * defensively we clear the pointer after a successful check). */` |
|     9181 |  735 | `		sState.pEnforceRetFunc = 0;` |
|     4588 |  736 | `	}` |
|   240457 |  737 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|   135911 |  738 | `		if( sState.pLastRef ){` |
|    78027 |  739 | `			*sState.pLastRef = pTos->nIdx;` |
|    39060 |  740 | `		}` |
|   135911 |  741 | `		if( sState.pResult ){` |
|        - |  742 | `			/* Execution result */` |
|   128947 |  743 | `			PH7_MemObjStore(pTos,sState.pResult);` |
|    64520 |  744 | `		}` |
|   135911 |  745 | `		VmPopOperand(&pTos,1);` |
|   172553 |  746 | `	}else if( sState.pLastRef ){` |
|        - |  747 | `		/* Nothing referenced — also the throw-unwind path: the compiler routes` |
|        - |  748 | `		 * an uncaught exception to this terminal OP_DONE with iP1 set but an` |
|        - |  749 | `		 * empty operand stack (pTos == pStack-1), so there is no return value to` |
|        - |  750 | `		 * store. Guarding on pTos >= pStack (matching the two sibling branches` |
|        - |  751 | `		 * above) avoids the below-base read that crashed under glibc/ASan. */` |
|     8821 |  752 | `		*sState.pLastRef = SXU32_HIGH;` |
|     4408 |  753 | `	}` |
|        - |  754 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  755 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  756 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  757 | `	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering` |
|        - |  758 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  759 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  760 | `	 * block can override it (the finally writes this body frame's sRet slot,` |
|        - |  761 | `	 * materialized below).` |
|        - |  762 | `	 */` |
|   240457 |  763 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|   240457 |  764 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  765 | `		goto Abort;` |
|        - |  766 | `	}` |
|   240457 |  767 | `	if( rc == PH7_EXCEPTION ){` |
|        - |  768 | `		/* A drained finally threw past itself, discarding the value this OP_DONE` |
|        - |  769 | `		 * stored into sState.pResult. If an enclosing try IN THIS function caught the new` |
|        - |  770 | `		 * exception in place, resume at its landing pad; otherwise unwind (the` |
|        - |  771 | `		 * caller's exception-resume pops the stored result). */` |
|        - |  772 | `		sxi32 iResumePc;` |
|        5 |  773 | `		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 |  774 | `			pc = iResumePc;` |
|        3 |  775 | `			break;` |
|        - |  776 | `		}` |
|        3 |  777 | `		goto Exception;` |
|        - |  778 | `	}` |
|   240453 |  779 | `	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){` |
|        - |  780 | `		/* A catch/finally issued a 'return' targeting THIS body. If the body is` |
|        - |  781 | `		 * actually unwinding because an exception escaped it (terminal OP_DONE on` |
|        - |  782 | `		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type` |
|        - |  783 | `		 * enforcement above), that exception supersedes the return: discard it.` |
|        - |  784 | `		 * Otherwise materialize it as this function's result. */` |
|        9 |  785 | `		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){` |
|      ! 0 |  786 | `			VmClearFrameReturn(sState.pEntryFrame);` |
|      ! 0 |  787 | `		}else{` |
|        9 |  788 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|        - |  789 | `		}` |
|        3 |  790 | `	}` |
|   240453 |  791 | `	goto Done;` |
|        - |  792 | `/*` |
|        - |  793 | ` * HALT: P1 * *` |
|        - |  794 | ` *` |
|        - |  795 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  796 | ` * and abort immediately.` |
|        - |  797 | ` */` |
|       12 |  798 | `case PH7_OP_HALT:` |
|       28 |  799 | `	if( pInstr->iP1 ){` |
|        - |  800 | `#ifdef UNTRUST` |
|        - |  801 | `		if( pTos < pStack ){` |
|        - |  802 | `			goto Abort;` |
|        - |  803 | `		}` |
|        - |  804 | `#endif` |
|       28 |  805 | `		if( sState.pLastRef ){` |
|        6 |  806 | `			*sState.pLastRef = pTos->nIdx;` |
|        2 |  807 | `		}` |
|       28 |  808 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       16 |  809 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  810 | `				/* Output the exit message */` |
|       22 |  811 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        6 |  812 | `					pVm->sVmConsumer.pUserData);` |
|       16 |  813 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|       10 |  814 | `			}` |
|       20 |  815 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  816 | `			/* Record exit status */` |
|       14 |  817 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        6 |  818 | `		}` |
|       28 |  819 | `		VmPopOperand(&pTos,1);` |
|       12 |  820 | `	}else if( sState.pLastRef ){` |
|        - |  821 | `		/* Nothing referenced */` |
|      ! 0 |  822 | `		*sState.pLastRef = SXU32_HIGH;` |
|      ! 0 |  823 | `	}` |
|        - |  824 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  825 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  826 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  827 | `	 */` |
|       28 |  828 | `	pVm->bHaltRequested = 1;` |
|       28 |  829 | `	goto Abort;` |
|        - |  830 | `/*` |
|        - |  831 | ` * JMP: * P2 *` |
|        - |  832 | ` *` |
|        - |  833 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  834 | ` * the one at index P2 from the beginning of the program.` |
|        - |  835 | ` */` |
|   317429 |  836 | `case PH7_OP_JMP:` |
|   635313 |  837 | `	pc = pInstr->iP2 - 1;` |
|   635313 |  838 | `	break;` |
|        - |  839 | `/*` |
|        - |  840 | ` * JZ: P1 P2 *` |
|        - |  841 | ` *` |
|        - |  842 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  843 | ` * entry in the stack if P1 is zero.` |
|        - |  844 | ` */` |
|   826408 |  845 | `case PH7_OP_JZ:` |
|        - |  846 | `#ifdef UNTRUST` |
|        - |  847 | `	if( pTos < pStack ){` |
|        - |  848 | `		goto Abort;` |
|        - |  849 | `	}` |
|        - |  850 | `#endif` |
|        - |  851 | `	/* Get a boolean value */` |
|  1654015 |  852 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     1004 |  853 | `		PH7_MemObjToBool(pTos);` |
|      500 |  854 | `	}` |
|  1654015 |  855 | `	if( !pTos->x.iVal ){` |
|        - |  856 | `		/* Take the jump */` |
|   856813 |  857 | `		pc = pInstr->iP2 - 1;` |
|   428502 |  858 | `	}` |
|  1654015 |  859 | `	if( !pInstr->iP1 ){` |
|  1336637 |  860 | `		VmPopOperand(&pTos,1);` |
|   668737 |  861 | `	}` |
|  1654015 |  862 | `	break;` |
|        - |  863 | `/*` |
|        - |  864 | ` * JNZ: P1 P2 *` |
|        - |  865 | ` *` |
|        - |  866 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  867 | ` * entry in the stack if P1 is zero.` |
|        - |  868 | ` */` |
|    85613 |  869 | `case PH7_OP_JNZ:` |
|        - |  870 | `#ifdef UNTRUST` |
|        - |  871 | `	if( pTos < pStack ){` |
|        - |  872 | `		goto Abort;` |
|        - |  873 | `	}` |
|        - |  874 | `#endif` |
|        - |  875 | `	/* Get a boolean value */` |
|   171329 |  876 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  877 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  878 | `	}` |
|   171329 |  879 | `	if( pTos->x.iVal ){` |
|        - |  880 | `		/* Take the jump */` |
|     9545 |  881 | `		pc = pInstr->iP2 - 1;` |
|     4770 |  882 | `	}` |
|   171329 |  883 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  884 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  885 | `	}` |
|   171329 |  886 | `	break;` |
|        - |  887 | `/*` |
|        - |  888 | ` * NOOP: * * *` |
|        - |  889 | ` *` |
|        - |  890 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  891 | ` * destination.` |
|        - |  892 | ` */` |
|      ! 0 |  893 | `case PH7_OP_NOOP:` |
|      ! 0 |  894 | `	break;` |
|        - |  895 | `/*` |
|        - |  896 | ` * POP: P1 * *` |
|        - |  897 | ` *` |
|        - |  898 | ` * Pop P1 elements from the operand stack.` |
|        - |  899 | ` */` |
|   655448 |  900 | `case PH7_OP_POP: {` |
|  1311743 |  901 | `	sxi32 n = pInstr->iP1;` |
|  1311743 |  902 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  903 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       79 |  904 | `		n = (sxi32)(pTos - pStack);` |
|       38 |  905 | `	}` |
|  1311743 |  906 | `	VmPopOperand(&pTos,n);` |
|  1311743 |  907 | `	break;` |
|        - |  908 | `				 }` |
|        - |  909 | `/*` |
|        - |  910 | ` * DUP: * * *` |
|        - |  911 | ` *` |
|        - |  912 | ` * Duplicate the top of the stack.` |
|        - |  913 | ` */` |
|       59 |  914 | `case PH7_OP_DUP:` |
|        - |  915 | `#ifdef UNTRUST` |
|        - |  916 | `	if( pTos < pStack ){` |
|        - |  917 | `		goto Abort;` |
|        - |  918 | `	}` |
|        - |  919 | `#endif` |
|      121 |  920 | `	pTos++;` |
|      121 |  921 | `	PH7_MemObjInit(pVm,pTos);` |
|      121 |  922 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|      121 |  923 | `	break;` |
|        - |  924 | `/*` |
|        - |  925 | ` * NSSWITCH: * * P3` |
|        - |  926 | ` *` |
|        - |  927 | ` * Switch the active namespace at runtime.` |
|        - |  928 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  929 | ` */` |
|    49092 |  930 | `case PH7_OP_NSSWITCH:` |
|    98189 |  931 | `	SyBlobReset(&pVm->sNamespace);` |
|    98189 |  932 | `	if( pInstr->p3 ){` |
|     4005 |  933 | `		const char *zNs = (const char *)pInstr->p3;` |
|     4005 |  934 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|     2000 |  935 | `	}` |
|        - |  936 | `	/* Clear namespace-scoped use-const imports */` |
|    98189 |  937 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    98189 |  938 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    98189 |  939 | `	break;` |
|        - |  940 | `/* OP_USECONST P1 * P3` |
|        - |  941 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  942 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  943 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  944 | ` */` |
|        7 |  945 | `case PH7_OP_USECONST: {` |
|       16 |  946 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  947 | `	if( azPair ){` |
|       16 |  948 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  949 | `	}` |
|       16 |  950 | `	break;` |
|        - |  951 | `				}` |
|        - |  952 | `/*` |
|        - |  953 | ` * CVT_INT: * * *` |
|        - |  954 | ` *` |
|        - |  955 | ` * Force the top of the stack to be an integer.` |
|        - |  956 | ` */` |
|     1002 |  957 | `case PH7_OP_CVT_INT:` |
|        - |  958 | `#ifdef UNTRUST` |
|        - |  959 | `	if( pTos < pStack ){` |
|        - |  960 | `		goto Abort;` |
|        - |  961 | `	}` |
|        - |  962 | `#endif` |
|     2009 |  963 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      909 |  964 | `		PH7_MemObjToInteger(pTos);` |
|      452 |  965 | `	}` |
|        - |  966 | `	/* Invalidate any prior representation */` |
|     2009 |  967 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|     2009 |  968 | `	break;` |
|        - |  969 | `/*` |
|        - |  970 | ` * CVT_REAL: * * *` |
|        - |  971 | ` *` |
|        - |  972 | ` * Force the top of the stack to be a real.` |
|        - |  973 | ` */` |
|       38 |  974 | `case PH7_OP_CVT_REAL:` |
|        - |  975 | `#ifdef UNTRUST` |
|        - |  976 | `	if( pTos < pStack ){` |
|        - |  977 | `		goto Abort;` |
|        - |  978 | `	}` |
|        - |  979 | `#endif` |
|       79 |  980 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       41 |  981 | `		PH7_MemObjToReal(pTos);` |
|       19 |  982 | `	}` |
|        - |  983 | `	/* Invalidate any prior representation */` |
|       79 |  984 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       79 |  985 | `	break;` |
|        - |  986 | `/*` |
|        - |  987 | ` * CVT_STR: * * *` |
|        - |  988 | ` *` |
|        - |  989 | ` * Force the top of the stack to be a string.` |
|        - |  990 | ` */` |
|     2730 |  991 | `case PH7_OP_CVT_STR:` |
|        - |  992 | `#ifdef UNTRUST` |
|        - |  993 | `	if( pTos < pStack ){` |
|        - |  994 | `		goto Abort;` |
|        - |  995 | `	}` |
|        - |  996 | `#endif` |
|     5465 |  997 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     2448 |  998 | `		PH7_MemObjToString(pTos);` |
|     1222 |  999 | `	}` |
|     5465 | 1000 | `	break;` |
|        - | 1001 | `/*` |
|        - | 1002 | ` * CVT_BOOL: * * *` |
|        - | 1003 | ` *` |
|        - | 1004 | ` * Force the top of the stack to be a boolean.` |
|        - | 1005 | ` */` |
|       66 | 1006 | `case PH7_OP_CVT_BOOL:` |
|        - | 1007 | `#ifdef UNTRUST` |
|        - | 1008 | `	if( pTos < pStack ){` |
|        - | 1009 | `		goto Abort;` |
|        - | 1010 | `	}` |
|        - | 1011 | `#endif` |
|      133 | 1012 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       37 | 1013 | `		PH7_MemObjToBool(pTos);` |
|       18 | 1014 | `	}` |
|      133 | 1015 | `	break;` |
|        - | 1016 | `/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it` |
|        - | 1017 | ` * always raises "The (unset) cast is no longer supported" (php 8 removed` |
|        - | 1018 | ` * the cast), so a program containing it never compiles. The switch has no` |
|        - | 1019 | ` * default arm, so abort loudly rather than fall through as a silent no-op` |
|        - | 1020 | ` * if a future emitter ever produces one without the compile error. */` |
|      ! 0 | 1021 | `case PH7_OP_CVT_NULL:` |
|      ! 0 | 1022 | `	goto Abort;` |
|        - | 1023 | `/*` |
|        - | 1024 | ` * CVT_NUMC: * * *` |
|        - | 1025 | ` *` |
|        - | 1026 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - | 1027 | ` */` |
|      ! 0 | 1028 | `case PH7_OP_CVT_NUMC:` |
|        - | 1029 | `#ifdef UNTRUST` |
|        - | 1030 | `	if( pTos < pStack ){` |
|        - | 1031 | `		goto Abort;` |
|        - | 1032 | `	}` |
|        - | 1033 | `#endif` |
|        - | 1034 | `	/* Force a numeric cast */` |
|      ! 0 | 1035 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 | 1036 | `	break;` |
|        - | 1037 | `/*` |
|        - | 1038 | ` * CVT_ARRAY: * * *` |
|        - | 1039 | ` *` |
|        - | 1040 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - | 1041 | ` */` |
|       14 | 1042 | `case PH7_OP_CVT_ARRAY:` |
|        - | 1043 | `#ifdef UNTRUST` |
|        - | 1044 | `	if( pTos < pStack ){` |
|        - | 1045 | `		goto Abort;` |
|        - | 1046 | `	}` |
|        - | 1047 | `#endif` |
|        - | 1048 | `	/* Force a hashmap cast */` |
|       29 | 1049 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       29 | 1050 | `	if( rc != SXRET_OK ){` |
|        - | 1051 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 | 1052 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - | 1053 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 | 1054 | `	}` |
|       29 | 1055 | `	break;` |
|        - | 1056 | `/*` |
|        - | 1057 | ` * CVT_OBJ: * * *` |
|        - | 1058 | ` *` |
|        - | 1059 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - | 1060 | ` */` |
|       17 | 1061 | `case PH7_OP_CVT_OBJ:` |
|        - | 1062 | `#ifdef UNTRUST` |
|        - | 1063 | `	if( pTos < pStack ){` |
|        - | 1064 | `		goto Abort;` |
|        - | 1065 | `	}` |
|        - | 1066 | `#endif` |
|       35 | 1067 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1068 | `		/* Force a 'stdClass()' cast */` |
|       35 | 1069 | `		PH7_MemObjToObject(pTos);` |
|       17 | 1070 | `	}` |
|       35 | 1071 | `	break;` |
|        - | 1072 | `/*` |
|        - | 1073 | ` * ERR_CTRL * * *` |
|        - | 1074 | ` *` |
|        - | 1075 | ` * Error control operator.` |
|        - | 1076 | ` */` |
|     3393 | 1077 | `case PH7_OP_UNSET_VAR: {` |
|        - | 1078 | `	VmOpRc rcOp;` |
|     6791 | 1079 | `	sState.pTos = pTos;` |
|     6791 | 1080 | `	sState.pc = pc;` |
|     6791 | 1081 | `	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);` |
|     6791 | 1082 | `	pTos = sState.pTos;` |
|     6791 | 1083 | `	pc = sState.pc;` |
|     6791 | 1084 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 1085 | `		goto Abort;` |
|     6789 | 1086 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1087 | `		goto Exception;` |
|        - | 1088 | `	}` |
|     6789 | 1089 | `	break;` |
|        - | 1090 | `					  }` |
|    37701 | 1091 | `case PH7_OP_ERR_CTRL:` |
|        - | 1092 | `	/*` |
|        - | 1093 | `	 * Error-control operator '@'. Emitted as a PAIR around the suppressed` |
|        - | 1094 | `	 * expression: iP1=1 opens the window (before the operand is evaluated),` |
|        - | 1095 | `	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),` |
|        - | 1096 | `	 * which went unnoticed only because the engine raised so few diagnostics;` |
|        - | 1097 | `	 * every warning/notice/deprecation the parity work added leaked straight` |
|        - | 1098 | ``	 * through `@`. The window nests, and php 8 does NOT let '@' swallow`` |
|        - | 1099 | `	 * fatals or exceptions — those unwind past the closing instruction, so` |
|        - | 1100 | `	 * the depth is reset on the exception path rather than decremented here.` |
|        - | 1101 | `	 */` |
|    75407 | 1102 | `	if( pInstr->iP1 ){` |
|    37707 | 1103 | `		pVm->nErrSuppress++;` |
|    56556 | 1104 | `	}else if( pVm->nErrSuppress > 0 ){` |
|    37705 | 1105 | `		pVm->nErrSuppress--;` |
|    18850 | 1106 | `	}` |
|    75407 | 1107 | `	break;` |
|        - | 1108 | `/*` |
|        - | 1109 | ` * IS_A * * *` |
|        - | 1110 | ` *` |
|        - | 1111 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - | 1112 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - | 1113 | ` * holding a class name or an object).` |
|        - | 1114 | ` * Push TRUE on success. FALSE otherwise.` |
|        - | 1115 | ` */` |
|      387 | 1116 | `case PH7_OP_IS_A:{` |
|      779 | 1117 | `	ph7_value *pNos = &pTos[-1];` |
|      779 | 1118 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - | 1119 | `#ifdef UNTRUST` |
|        - | 1120 | `	if( pNos < pStack ){` |
|        - | 1121 | `		goto Abort;` |
|        - | 1122 | `	}` |
|        - | 1123 | `#endif` |
|      779 | 1124 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      621 | 1125 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      621 | 1126 | `		ph7_class *pClass = 0;` |
|        - | 1127 | `		/* Extract the target class */` |
|      621 | 1128 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - | 1129 | `			/* Instance already loaded */` |
|      ! 0 | 1130 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      621 | 1131 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      621 | 1132 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      621 | 1133 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - | 1134 | `			/* Handle self/static/parent keywords */` |
|      621 | 1135 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        6 | 1136 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      619 | 1137 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 | 1138 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      616 | 1139 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        6 | 1140 | `				pClass = PH7_VmResolveParentClass(&(*pVm));` |
|        4 | 1141 | `			}else{` |
|      611 | 1142 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - | 1143 | `			}` |
|      308 | 1144 | `		}` |
|      621 | 1145 | `		if( pClass ){` |
|        - | 1146 | `			/* Perform the query */` |
|      621 | 1147 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|      308 | 1148 | `		}` |
|      308 | 1149 | `	}` |
|        - | 1150 | `	/* Push result */` |
|      779 | 1151 | `	VmPopOperand(&pTos,1);` |
|      779 | 1152 | `	PH7_MemObjRelease(pTos);` |
|      779 | 1153 | `	pTos->x.iVal = iRes;` |
|      779 | 1154 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      779 | 1155 | `	break;` |
|        - | 1156 | `				 }` |
|        - | 1157 |  |
|        - | 1158 | `/*` |
|        - | 1159 | ` * LOADC P1 P2 *` |
|        - | 1160 | ` *` |
|        - | 1161 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - | 1162 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - | 1163 | ` */` |
|  1465012 | 1164 | `case PH7_OP_LOADC: {` |
|        - | 1165 | `	ph7_value *pObj;` |
|        - | 1166 | `	/* Reserve a room */` |
|  2931165 | 1167 | `	pTos++;` |
|  2931165 | 1168 | `	if( pInstr->iP1 & PH7_LOADC_NOKEY ){` |
|        - | 1169 | `		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */` |
|    19059 | 1170 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19059 | 1171 | `		SyBlobReset(&pTos->sBlob);` |
|    19059 | 1172 | `		pTos->iFlags \|= MEMOBJ_AUX_NOKEY;` |
|    19059 | 1173 | `		pTos->nIdx = SXU32_HIGH;` |
|    19059 | 1174 | `		break;` |
|        - | 1175 | `	}` |
|  2912111 | 1176 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2912111 | 1177 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - | 1178 | `			SyHashEntry *pEntry;` |
|        - | 1179 | `			/* Check use const imports first — imports take precedence */` |
|        - | 1180 | `			{` |
|        - | 1181 | `				SyHashEntry *pConstImport;` |
|    34733 | 1182 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    23152 | 1183 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    23157 | 1184 | `				if( pConstImport ){` |
|       11 | 1185 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 | 1186 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 | 1187 | `					if( pEntry ){` |
|       11 | 1188 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 | 1189 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 1190 | `						SyBlobReset(&pTos->sBlob);` |
|       11 | 1191 | `						VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|       11 | 1192 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 | 1193 | `						break;` |
|        - | 1194 | `					}` |
|        - | 1195 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 | 1196 | `				}` |
|        - | 1197 | `			}` |
|        - | 1198 | `			/* Candidate for expansion via user defined callbacks */` |
|    23147 | 1199 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    23147 | 1200 | `			if( pEntry ){` |
|    23137 | 1201 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - | 1202 | `				/* Set a NULL default value */` |
|    23137 | 1203 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    23137 | 1204 | `				SyBlobReset(&pTos->sBlob);` |
|        - | 1205 | `				/* Invoke the callback and deal with the expanded value */` |
|    23137 | 1206 | `				VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|        - | 1207 | `				/* Mark as constant */` |
|    23137 | 1208 | `				pTos->nIdx = SXU32_HIGH;` |
|    23137 | 1209 | `				break;` |
|        - | 1210 | `			}` |
|        - | 1211 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - | 1212 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - | 1213 | `			 * use-const imports → current NS → global → string fallback).` |
|        - | 1214 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - | 1215 | `			{` |
|       12 | 1216 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|       12 | 1217 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - | 1218 | `				sxu32 j;` |
|       12 | 1219 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       82 | 1220 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       71 | 1221 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       36 | 1222 | `				}` |
|       12 | 1223 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - | 1224 | `					/* Try current_namespace\name */` |
|      ! 0 | 1225 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 | 1226 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 | 1227 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 | 1228 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 | 1229 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 | 1230 | `					if( pEntry ){` |
|      ! 0 | 1231 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 | 1232 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 1233 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 | 1234 | `						VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|      ! 0 | 1235 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 1236 | `						break;` |
|        - | 1237 | `					}` |
|        - | 1238 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 | 1239 | `				}` |
|        - | 1240 | `				{` |
|        - | 1241 | `					/*` |
|        - | 1242 | `					 * php 8 has no bare-word fallback: an unresolved constant is a catchable` |
|        - | 1243 | `					 * Error, not its own name as a string. PH7 answered "X" for an unknown` |
|        - | 1244 | `					 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —` |
|        - | 1245 | `					 * silently became a string and flowed on.` |
|        - | 1246 | `					 *` |
|        - | 1247 | `					 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call` |
|        - | 1248 | ``					 * boundary, so neither a bare `break` nor `goto Exception` is correct`` |
|        - | 1249 | `					 * here (see the macro).` |
|        - | 1250 | `					 */` |
|        - | 1251 | `					SyBlob sMsg;` |
|       12 | 1252 | `					SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       12 | 1253 | `					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);` |
|       12 | 1254 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       12 | 1255 | `					SyBlobReset(&pTos->sBlob);` |
|       12 | 1256 | `					pTos->nIdx = SXU32_HIGH;` |
|       17 | 1257 | `					rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|        5 | 1258 | `						SyBlobLength(&sMsg));` |
|       12 | 1259 | `					SyBlobRelease(&sMsg);` |
|       12 | 1260 | `					if( rc == SXERR_ABORT ){` |
|        3 | 1261 | `						goto Abort;` |
|        - | 1262 | `					}` |
|        9 | 1263 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 1264 | `				}` |
|        - | 1265 | `			}` |
|        - | 1266 | `		}` |
|  2888959 | 1267 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1445050 | 1268 | `	}else{` |
|        - | 1269 | `		/* Set a NULL value */` |
|      ! 0 | 1270 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 1271 | `	}` |
|        - | 1272 | `	/* Mark as constant */` |
|  2888959 | 1273 | `	pTos->nIdx = SXU32_HIGH;` |
|  2888959 | 1274 | `	break;` |
|        - | 1275 | `				  }` |
|        - | 1276 | `/*` |
|        - | 1277 | ` * LOAD: P1 * P3` |
|        - | 1278 | ` *` |
|        - | 1279 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - | 1280 | ` * from the P3 operand.` |
|        - | 1281 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - | 1282 | ` * the variable if non existent and push the NULL constant instead.` |
|        - | 1283 | ` */` |
|  2218088 | 1284 | `case PH7_OP_LOAD:{` |
|        - | 1285 | `	ph7_value *pObj;` |
|        - | 1286 | `	SyString sName;` |
|  4439705 | 1287 | `	if( pInstr->p3 == 0 ){` |
|        - | 1288 | `		/* Take the variable name from the top of the stack */` |
|        - | 1289 | `#ifdef UNTRUST` |
|        - | 1290 | `		if( pTos < pStack ){` |
|        - | 1291 | `			goto Abort;` |
|        - | 1292 | `		}` |
|        - | 1293 | `#endif` |
|        - | 1294 | `		/* Force a string cast */` |
|       17 | 1295 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1296 | `			PH7_MemObjToString(pTos);` |
|      ! 0 | 1297 | `		}` |
|       17 | 1298 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        9 | 1299 | `	}else{` |
|  4439689 | 1300 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 1301 | `		/* Reserve a room for the target object */` |
|  4439689 | 1302 | `		pTos++;` |
|        - | 1303 | `	}` |
|        - | 1304 | `	/* Extract the requested memory object */` |
|  4439705 | 1305 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  4439705 | 1306 | `	if( pObj == 0 ){` |
|       31 | 1307 | `		if( pInstr->iP1 ){` |
|        - | 1308 | `			/* Variable not found,load NULL */` |
|       31 | 1309 | `			if( !pInstr->p3 ){` |
|      ! 0 | 1310 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 | 1311 | `			}else{` |
|       31 | 1312 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 1313 | `			}` |
|       31 | 1314 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  2218104 | 1315 | `			break;` |
|      ! 0 | 1316 | `		}else{` |
|        - | 1317 | `			/* Fatal error */` |
|      ! 0 | 1318 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 1319 | `			goto Abort;` |
|        - | 1320 | `		}` |
|        - | 1321 | `	}` |
|        - | 1322 | `	/* Load variable contents */` |
|  4439675 | 1323 | `	PH7_MemObjLoad(pObj,pTos);` |
|  4439675 | 1324 | `	pTos->nIdx = pObj->nIdx;` |
|  4439675 | 1325 | `	break;` |
|        - | 1326 | `				   }` |
|        - | 1327 | `/*` |
|        - | 1328 | ` * LOAD_MAP P1 * *` |
|        - | 1329 | ` *` |
|        - | 1330 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - | 1331 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - | 1332 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - | 1333 | ` */` |
|    36788 | 1334 | `case PH7_OP_LOAD_MAP: {` |
|        - | 1335 | `	VmOpRc rcOp;` |
|    73581 | 1336 | `	sState.pTos = pTos;` |
|    73581 | 1337 | `	sState.pc = pc;` |
|    73581 | 1338 | `	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);` |
|    73581 | 1339 | `	pTos = sState.pTos;` |
|    73581 | 1340 | `	pc = sState.pc;` |
|    73581 | 1341 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1342 | `		goto Abort;` |
|    73581 | 1343 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       15 | 1344 | `		goto Exception;` |
|        - | 1345 | `	}` |
|    73567 | 1346 | `	break;` |
|        - | 1347 | `					  }` |
|        - | 1348 | `/*` |
|        - | 1349 | ` * LOAD_LIST: P1 * *` |
|        - | 1350 | ` *` |
|        - | 1351 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - | 1352 | ` * This is the VM implementation of the list() PHP construct.` |
|        - | 1353 | ` * Caveats:` |
|        - | 1354 | ` *  This implementation support only a single nesting level.` |
|        - | 1355 | ` */` |
|      132 | 1356 | `case PH7_OP_LOAD_LIST: {` |
|        - | 1357 | `	VmOpRc rcOp;` |
|      269 | 1358 | `	sState.pTos = pTos;` |
|      269 | 1359 | `	sState.pc = pc;` |
|      269 | 1360 | `	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);` |
|      269 | 1361 | `	pTos = sState.pTos;` |
|      269 | 1362 | `	pc = sState.pc;` |
|      269 | 1363 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1364 | `		goto Abort;` |
|      269 | 1365 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1366 | `		goto Exception;` |
|        - | 1367 | `	}` |
|      269 | 1368 | `	break;` |
|        - | 1369 | `					  }` |
|        - | 1370 | `/*` |
|        - | 1371 | ` * LOAD_IDX: P1 P2 *` |
|        - | 1372 | ` *` |
|        - | 1373 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - | 1374 | ` * from the stack.` |
|        - | 1375 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - | 1376 | ` * instead.` |
|        - | 1377 | ` */` |
|   337567 | 1378 | `case PH7_OP_LOAD_IDX: {` |
|        - | 1379 | `	VmOpRc rcOp;` |
|   675785 | 1380 | `	sState.pTos = pTos;` |
|   675785 | 1381 | `	sState.pc = pc;` |
|   675785 | 1382 | `	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);` |
|   675785 | 1383 | `	pTos = sState.pTos;` |
|   675785 | 1384 | `	pc = sState.pc;` |
|   675785 | 1385 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1386 | `		goto Abort;` |
|   675785 | 1387 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1388 | `		goto Exception;` |
|        - | 1389 | `	}` |
|   675785 | 1390 | `	break;` |
|        - | 1391 | `					  }` |
|        - | 1392 | `/*` |
|        - | 1393 | ` * LOAD_CLOSURE * * P3` |
|        - | 1394 | ` *` |
|        - | 1395 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - | 1396 | ` * name in the stack.` |
|        - | 1397 | ` */` |
|      569 | 1398 | `case PH7_OP_LOAD_CLOSURE: {` |
|        - | 1399 | `	VmOpRc rcOp;` |
|     1143 | 1400 | `	sState.pTos = pTos;` |
|     1143 | 1401 | `	sState.pc = pc;` |
|     1143 | 1402 | `	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);` |
|     1143 | 1403 | `	pTos = sState.pTos;` |
|     1143 | 1404 | `	pc = sState.pc;` |
|     1143 | 1405 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1406 | `		goto Abort;` |
|     1143 | 1407 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1408 | `		goto Exception;` |
|        - | 1409 | `	}` |
|     1143 | 1410 | `	break;` |
|        - | 1411 | `					  }` |
|        - | 1412 | `/*` |
|        - | 1413 | ` * LOAD_FCC P1 * *` |
|        - | 1414 | ` *` |
|        - | 1415 | ` * First-class callable: wrap the callee in a Closure object instead of calling it.` |
|        - | 1416 | ` *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS` |
|        - | 1417 | ` *           (from the callee's OP_LOADC). Replace it in place with a Closure whose` |
|        - | 1418 | ` *           $__fn is that name; the existing string-callable dispatch resolves it.` |
|        - | 1419 | ` *           (OOM degrades to leaving the name string on the stack — still callable.)` |
|        - | 1420 | ` *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name` |
|        - | 1421 | ` *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds` |
|        - | 1422 | ` *           $this (scope = its class); a class-name-string target is a static callable` |
|        - | 1423 | ` *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —` |
|        - | 1424 | ` *           the popped target leaves no name string to keep.)` |
|        - | 1425 | ` */` |
|       40 | 1426 | `case PH7_OP_LOAD_FCC:{` |
|       81 | 1427 | `	if( pInstr->iP1 == 1 ){` |
|        - | 1428 | ``		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)`` |
|        - | 1429 | `		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —` |
|        - | 1430 | `		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,` |
|        - | 1431 | ``		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a`` |
|        - | 1432 | `		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is` |
|        - | 1433 | `		 * (graceful degradation), and so is the original on OOM — still whatever it was. */` |
|        - | 1434 | `		ph7_class_instance *pCloObj;` |
|       45 | 1435 | `		if( VmValueIsClosure(pVm, pTos) ){` |
|        3 | 1436 | `			break;` |
|        - | 1437 | `		}` |
|       43 | 1438 | `		pCloObj = VmFccWrapValue(pVm, pTos);` |
|       43 | 1439 | `		if( pCloObj ){` |
|       43 | 1440 | `			PH7_MemObjRelease(pTos);` |
|       43 | 1441 | `			pCloObj->iRef++;` |
|       43 | 1442 | `			pTos->x.pOther = pCloObj;` |
|       43 | 1443 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       21 | 1444 | `		}` |
|       22 | 1445 | `	}else{` |
|        - | 1446 | `		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]` |
|        - | 1447 | `		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its` |
|        - | 1448 | `		 * class); a class-name string target is a static callable (scope = that class). */` |
|       37 | 1449 | `		ph7_value *pTarget = &pTos[-1];` |
|        - | 1450 | `		SyString sName;` |
|        - | 1451 | `		ph7_class_instance *pCloObj;` |
|       37 | 1452 | `		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));` |
|       37 | 1453 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|       19 | 1454 | `			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;` |
|       19 | 1455 | `			pCloObj = VmCreateClosure(pVm, &sName, pBoundThis, &pBoundThis->pClass->sName);` |
|       28 | 1456 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|        - | 1457 | ``			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class`` |
|        - | 1458 | `			 * now, so the closure binds the concrete scope (matching PHP). */` |
|       19 | 1459 | `			ph7_class *pScopeCls = VmFccResolveScope(pVm, pTarget);` |
|       19 | 1460 | `			pCloObj = pScopeCls ? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;` |
|       10 | 1461 | `		}else{` |
|      ! 0 | 1462 | `			pCloObj = 0;` |
|        - | 1463 | `		}` |
|        - | 1464 | `		/* Pop the method name and the target, push the Closure. */` |
|       37 | 1465 | `		PH7_MemObjRelease(pTos);` |
|       37 | 1466 | `		pTos--;` |
|       37 | 1467 | `		PH7_MemObjRelease(pTos);` |
|       37 | 1468 | `		if( pCloObj ){` |
|       37 | 1469 | `			pCloObj->iRef++;` |
|       37 | 1470 | `			pTos->x.pOther = pCloObj;` |
|       37 | 1471 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       19 | 1472 | `		}else{` |
|      ! 0 | 1473 | `			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */` |
|        - | 1474 | `		}` |
|        - | 1475 | `	}` |
|       79 | 1476 | `	break;` |
|        - | 1477 | `					 }` |
|        - | 1478 | `/*` |
|        - | 1479 | ` * STORE * P2 P3` |
|        - | 1480 | ` *` |
|        - | 1481 | ` * Perform a store (Assignment) operation.` |
|        - | 1482 | ` */` |
|   264800 | 1483 | `case PH7_OP_STORE: {` |
|        - | 1484 | `	ph7_value *pObj;` |
|        - | 1485 | `	SyString sName;` |
|        - | 1486 | `#ifdef UNTRUST` |
|        - | 1487 | `	if( pTos < pStack ){` |
|        - | 1488 | `		goto Abort;` |
|        - | 1489 | `	}` |
|        - | 1490 | `#endif` |
|   529997 | 1491 | `	if( pInstr->iP2 ){` |
|        - | 1492 | `		sxu32 nIdx;` |
|        - | 1493 | `		sxi32 rcT;` |
|        - | 1494 | `		/* Member store operation */` |
|    11665 | 1495 | `		nIdx = pTos->nIdx;` |
|    11665 | 1496 | `		VmPopOperand(&pTos,1);` |
|    11665 | 1497 | `		if( pVm->pMagicSetThis ){` |
|        - | 1498 | `			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain` |
|        - | 1499 | `			 * store to a missing/inaccessible property on a class declaring __set.` |
|        - | 1500 | `			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on` |
|        - | 1501 | `			 * the stack as the assignment expression's result — php's semantics` |
|        - | 1502 | `			 * (no property is created; a throw rides the boundary rail). */` |
|       11 | 1503 | `			ph7_class_instance *pSetThis = pVm->pMagicSetThis;` |
|        - | 1504 | `			SyString sSetName;` |
|       11 | 1505 | `			pVm->pMagicSetThis = 0;` |
|       11 | 1506 | `			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));` |
|       11 | 1507 | `			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);` |
|       11 | 1508 | `			PH7_ClassInstanceUnref(pSetThis);` |
|       11 | 1509 | `			SyBlobReset(&pVm->sMagicSetName);` |
|       11 | 1510 | `			break;` |
|        - | 1511 | `		}` |
|    11655 | 1512 | `		if( pVm->pHookSetThis ){` |
|        - | 1513 | `			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a` |
|        - | 1514 | `			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with` |
|        - | 1515 | `			 * the rvalue (which stays on the stack as the assignment expression's` |
|        - | 1516 | ``			 * result); a `set => expr` hook's return value is stored into the`` |
|        - | 1517 | `			 * BACKING slot with the ordinary typed enforcement. A property with` |
|        - | 1518 | `			 * only a get hook is php's catchable "is read-only" Error. */` |
|       31 | 1519 | `			ph7_class_instance *pHThis = pVm->pHookSetThis;` |
|       31 | 1520 | `			ph7_class_attr *pHAttr = pVm->pHookSetAttr;` |
|       31 | 1521 | `			sxu32 nBackIdx = pVm->nHookSetIdx;` |
|        - | 1522 | `			sxi32 rcHs;` |
|       31 | 1523 | `			pVm->pHookSetThis = 0;` |
|       31 | 1524 | `			pVm->pHookSetAttr = 0;` |
|       31 | 1525 | `			pVm->nHookSetIdx = SXU32_HIGH;` |
|       31 | 1526 | `			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);` |
|       31 | 1527 | `			PH7_ClassInstanceUnref(pHThis);` |
|       31 | 1528 | `			if( rcHs == PH7_ABORT ){` |
|      ! 0 | 1529 | `				goto Abort;` |
|        - | 1530 | `			}` |
|       31 | 1531 | `			break;` |
|        - | 1532 | `		}` |
|    11625 | 1533 | `		if( nIdx == SXU32_HIGH ){` |
|        3 | 1534 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 1535 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        3 | 1536 | `			pTos->nIdx = SXU32_HIGH;` |
|        2 | 1537 | `		}else{` |
|        - | 1538 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - | 1539 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|    11623 | 1540 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);` |
|    11623 | 1541 | `			if( rcT == PH7_ABORT ){` |
|       13 | 1542 | `				goto Abort;` |
|        - | 1543 | `			}` |
|    11613 | 1544 | `			if( rcT == PH7_EXCEPTION ){` |
|        - | 1545 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - | 1546 | `				 * control to the nearest catch block if any, otherwise` |
|        - | 1547 | `				 * propagate out of the VM loop. */` |
|       73 | 1548 | `				VmPopOperand(&pTos,1);` |
|        - | 1549 | `				{` |
|        - | 1550 | `					sxi32 iRp;` |
|       73 | 1551 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|       69 | 1552 | `						pc = iRp;` |
|       69 | 1553 | `						break;` |
|        - | 1554 | `					}` |
|        - | 1555 | `				}` |
|        5 | 1556 | `				goto Exception;` |
|        - | 1557 | `			}` |
|        - | 1558 | `			/* Point to the desired memory object */` |
|    11545 | 1559 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    11545 | 1560 | `			if( pObj ){` |
|        - | 1561 | `				/* Perform the store operation */` |
|    11545 | 1562 | `				PH7_MemObjStore(pTos,pObj);` |
|     5770 | 1563 | `			}` |
|        - | 1564 | `		}` |
|    11547 | 1565 | `		break;` |
|   518337 | 1566 | `	}else if( pInstr->p3 == 0 ){` |
|        - | 1567 | `		/* Take the variable name from the next on the stack */` |
|        9 | 1568 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1569 | `			/* Force a string cast */` |
|      ! 0 | 1570 | `			PH7_MemObjToString(pTos);` |
|      ! 0 | 1571 | `		}` |
|        9 | 1572 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        9 | 1573 | `		pTos--;` |
|        - | 1574 | `#ifdef UNTRUST` |
|        - | 1575 | `		if( pTos < pStack  ){` |
|        - | 1576 | `			goto Abort;` |
|        - | 1577 | `		}` |
|        - | 1578 | `#endif` |
|        5 | 1579 | `	}else{` |
|   518329 | 1580 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 1581 | `	}` |
|   518332 | 1582 | `	if( sName.nByte == sizeof("GLOBALS")-1` |
|   260727 | 1583 | `	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){` |
|        6 | 1584 | `		if( pInstr->p3 ){` |
|        - | 1585 | `			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time` |
|        - | 1586 | `			 * fatal there; raised at the store site here with the same` |
|        - | 1587 | `			 * message and the same non-catchable outcome). Element writes` |
|        - | 1588 | `			 * are unaffected. */` |
|        3 | 1589 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 1590 | `				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 1591 | `			pVm->iExitStatus = 255;` |
|        3 | 1592 | `			pVm->bHaltRequested = 1;` |
|        3 | 1593 | `			goto Abort;` |
|        - | 1594 | `		}` |
|        - | 1595 | `		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's` |
|        - | 1596 | `		 * compile-time case: php quietly creates an ordinary symbol-table` |
|        - | 1597 | `		 * entry named GLOBALS, leaving the auto-global view intact. */` |
|        3 | 1598 | `		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);` |
|        3 | 1599 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 1600 | `		break;` |
|        - | 1601 | `	}` |
|        - | 1602 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   518333 | 1603 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   518333 | 1604 | `	if( pObj == 0 ){` |
|      ! 0 | 1605 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1606 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 1607 | `		goto Abort;` |
|        - | 1608 | `	}` |
|   518333 | 1609 | `	if( !pInstr->p3 ){` |
|        7 | 1610 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 1611 | `	}` |
|        - | 1612 | `	/* Perform the store operation */` |
|   518333 | 1613 | `	PH7_MemObjStore(pTos,pObj);` |
|   518333 | 1614 | `	break;` |
|        - | 1615 | `				   }` |
|        - | 1616 | `/*` |
|        - | 1617 | ` * STORE_IDX:   P1 * P3` |
|        - | 1618 | ` * STORE_IDX_R: P1 * P3` |
|        - | 1619 | ` *` |
|        - | 1620 | ` * Perfrom a store operation an a hashmap entry.` |
|        - | 1621 | ` */` |
|   120161 | 1622 | `case PH7_OP_STORE_IDX:` |
|        - | 1623 | `case PH7_OP_STORE_IDX_REF: {` |
|        - | 1624 | `	VmOpRc rcOp;` |
|   240327 | 1625 | `	sState.pTos = pTos;` |
|   240327 | 1626 | `	sState.pc = pc;` |
|   240327 | 1627 | `	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);` |
|   240327 | 1628 | `	pTos = sState.pTos;` |
|   240327 | 1629 | `	pc = sState.pc;` |
|   240327 | 1630 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 1631 | `		goto Abort;` |
|   240325 | 1632 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       12 | 1633 | `		goto Exception;` |
|        - | 1634 | `	}` |
|   240315 | 1635 | `	break;` |
|        - | 1636 | `					  }` |
|        - | 1637 | `/*` |
|        - | 1638 | ` * INCR: P1 * *` |
|        - | 1639 | ` *` |
|        - | 1640 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - | 1641 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - | 1642 | ` * the stack and increment after that.` |
|        - | 1643 | ` */` |
|   209454 | 1644 | `case PH7_OP_INCR:` |
|        - | 1645 | `#ifdef UNTRUST` |
|        - | 1646 | `	if( pTos < pStack ){` |
|        - | 1647 | `		goto Abort;` |
|        - | 1648 | `	}` |
|        - | 1649 | `#endif` |
|        - | 1650 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|        - | 1651 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 1652 | `	 * — which otherwise skips object/array/resource operands. */` |
|   419363 | 1653 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 1654 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 1655 | `	 * php's TypeError, raised BEFORE any set dispatch (the type guard below` |
|        - | 1656 | `	 * would skip the mutation and the tail write-back would otherwise call` |
|        - | 1657 | `	 * the set hook with the unchanged value). */` |
|   419352 | 1658 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|   209907 | 1659 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 1660 | `		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 1661 | `		if( pTopInc->iKind == VM_HOOK_PEND_RMW && pTopInc->nScratchIdx == pTos->nIdx ){` |
|        - | 1662 | `			SyBlob sErrMsg;` |
|        3 | 1663 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 1664 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        3 | 1665 | `				SyBlobAppend(&sErrMsg,"Cannot increment array",sizeof("Cannot increment array")-1);` |
|        1 | 1666 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1667 | `				SyBlobFormat(&sErrMsg,"Cannot increment %z",` |
|      ! 0 | 1668 | `					&((ph7_class_instance *)pTos->x.pOther)->pClass->sName);` |
|      ! 0 | 1669 | `			}else{` |
|      ! 0 | 1670 | `				SyBlobAppend(&sErrMsg,"Cannot increment resource",sizeof("Cannot increment resource")-1);` |
|        - | 1671 | `			}` |
|        3 | 1672 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 1673 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 1674 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 1675 | `			break;` |
|        - | 1676 | `		}` |
|      ! 0 | 1677 | `	}` |
|   419355 | 1678 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   419355 | 1679 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 1680 | `			ph7_value *pObj;` |
|   419355 | 1681 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   419355 | 1682 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 1683 | `					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric` |
|        - | 1684 | `					 * string (it points at str_increment() instead); PHL rejects it. */` |
|        - | 1685 | `					SyBlob sErrMsg;` |
|        3 | 1686 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 1687 | `					SyBlobAppend(&sErrMsg,` |
|        - | 1688 | `						"Increment on a non-numeric string is not supported, use str_increment() instead",` |
|        - | 1689 | `						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);` |
|        3 | 1690 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 1691 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 1692 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 1693 | `					break;` |
|      ! 0 | 1694 | `				}else{` |
|        - | 1695 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - | 1696 | `					 * original value: pTos may alias pObj's blob via` |
|        - | 1697 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - | 1698 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - | 1699 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - | 1700 | `					 * so its old-value view survives the coercion. */` |
|   419353 | 1701 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        8 | 1702 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        3 | 1703 | `					}` |
|        - | 1704 | `					/* Force a numeric cast on the variable */` |
|   419353 | 1705 | `					PH7_MemObjToNumeric(pObj);` |
|   419353 | 1706 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        5 | 1707 | `						pObj->rVal++;` |
|        - | 1708 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 1709 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 1710 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 1711 | `						 * integer-valued real. */` |
|        5 | 1712 | `						PH7_MemObjTryInteger(pObj);` |
|        3 | 1713 | `					}else{` |
|        - | 1714 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 1715 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 1716 | `						sxi64 r;` |
|   419349 | 1717 | `						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 1718 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        5 | 1719 | `							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;` |
|        5 | 1720 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 1721 | `#else` |
|        - | 1722 | `							pObj->x.iVal = r;` |
|        - | 1723 | `#endif` |
|        3 | 1724 | `						}else{` |
|   419345 | 1725 | `							pObj->x.iVal = r;` |
|        - | 1726 | `						}` |
|        - | 1727 | `					}` |
|   419353 | 1728 | `					if( pInstr->iP1 ){` |
|        - | 1729 | `						/* Pre-increment: result is the new value. */` |
|      137 | 1730 | `						PH7_MemObjStore(pObj,pTos);` |
|       68 | 1731 | `					}` |
|        - | 1732 | `					/* Post-increment: pTos retains the old value (a string` |
|        - | 1733 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - | 1734 | `				}` |
|   209899 | 1735 | `			}` |
|   209904 | 1736 | `		}else{` |
|      ! 0 | 1737 | `			if( pInstr->iP1 ){` |
|      ! 0 | 1738 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 | 1739 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 | 1740 | `				}else{` |
|        - | 1741 | `					/* Force a numeric cast */` |
|      ! 0 | 1742 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 1743 | `					/* Pre-increment */` |
|      ! 0 | 1744 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 1745 | `						pTos->rVal++;` |
|        - | 1746 | `						/* Try to get an integer representation */` |
|      ! 0 | 1747 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 1748 | `					}else{` |
|        - | 1749 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 1750 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 1751 | `						sxi64 r;` |
|      ! 0 | 1752 | `						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 1753 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 1754 | `							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;` |
|      ! 0 | 1755 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 1756 | `#else` |
|        - | 1757 | `							pTos->x.iVal = r;` |
|        - | 1758 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 1759 | `#endif` |
|      ! 0 | 1760 | `						}else{` |
|      ! 0 | 1761 | `							pTos->x.iVal = r;` |
|      ! 0 | 1762 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 1763 | `						}` |
|        - | 1764 | `					}` |
|        - | 1765 | `				}` |
|      ! 0 | 1766 | `			}` |
|        - | 1767 | `		}` |
|   209899 | 1768 | `	}` |
|   419353 | 1769 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|   419353 | 1770 | `	break;` |
|        - | 1771 | `/*` |
|        - | 1772 | ` * DECR: P1 * *` |
|        - | 1773 | ` *` |
|        - | 1774 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - | 1775 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - | 1776 | ` * and decrement after that.` |
|        - | 1777 | ` */` |
|      102 | 1778 | `case PH7_OP_DECR:` |
|        - | 1779 | `#ifdef UNTRUST` |
|        - | 1780 | `	if( pTos < pStack ){` |
|        - | 1781 | `		goto Abort;` |
|        - | 1782 | `	}` |
|        - | 1783 | `#endif` |
|        - | 1784 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|        - | 1785 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 1786 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|        - | 1787 | `	 * property currently holding null). */` |
|      207 | 1788 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 1789 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 1790 | `	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —` |
|        - | 1791 | ``	 * `--` on null is php's no-op and its write-back still dispatches set). */`` |
|      198 | 1792 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|      103 | 1793 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 1794 | `		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 1795 | `		if( pTopDec->iKind == VM_HOOK_PEND_RMW && pTopDec->nScratchIdx == pTos->nIdx ){` |
|        - | 1796 | `			SyBlob sErrMsg;` |
|        3 | 1797 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 1798 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        3 | 1799 | `				SyBlobAppend(&sErrMsg,"Cannot decrement array",sizeof("Cannot decrement array")-1);` |
|        1 | 1800 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1801 | `				SyBlobFormat(&sErrMsg,"Cannot decrement %z",` |
|      ! 0 | 1802 | `					&((ph7_class_instance *)pTos->x.pOther)->pClass->sName);` |
|      ! 0 | 1803 | `			}else{` |
|      ! 0 | 1804 | `				SyBlobAppend(&sErrMsg,"Cannot decrement resource",sizeof("Cannot decrement resource")-1);` |
|        - | 1805 | `			}` |
|        3 | 1806 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 1807 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 1808 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 1809 | `			break;` |
|        - | 1810 | `		}` |
|      ! 0 | 1811 | `	}` |
|        - | 1812 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op) -- but 8.3`` |
|        - | 1813 | `	 * deprecates that no-op, same as the non-numeric-string one below. */` |
|      199 | 1814 | `	if( pTos->iFlags & MEMOBJ_NULL ){` |
|        - | 1815 | `		/* E_WARNING, not E_DEPRECATED -- php reports this one at errno 2. */` |
|      ! 0 | 1816 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 1817 | `			"Decrement on type null has no effect, this will change in the next major version of PHP");` |
|      ! 0 | 1818 | `	}` |
|      199 | 1819 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      199 | 1820 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 1821 | `			ph7_value *pObj;` |
|      199 | 1822 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      199 | 1823 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 1824 | ``					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string`` |
|        - | 1825 | `					 * (php has no string decrement); PHL rejects it. */` |
|        - | 1826 | `					SyBlob sErrMsg;` |
|        3 | 1827 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 1828 | `					SyBlobAppend(&sErrMsg,` |
|        - | 1829 | `						"Decrement on a non-numeric string is not supported",` |
|        - | 1830 | `						sizeof("Decrement on a non-numeric string is not supported")-1);` |
|        3 | 1831 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 1832 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 1833 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 1834 | `					break;` |
|      ! 0 | 1835 | `				}else{` |
|        - | 1836 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - | 1837 | `					 * post-decrement must preserve pTos's original value, which` |
|        - | 1838 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - | 1839 | `					 * Force pTos to own its blob before coercing pObj. */` |
|      197 | 1840 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 | 1841 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 | 1842 | `					}` |
|      197 | 1843 | `					PH7_MemObjToNumeric(pObj);` |
|      197 | 1844 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 | 1845 | `						pObj->rVal--;` |
|        - | 1846 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 1847 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 1848 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 1849 | `						 * integer-valued real. */` |
|        9 | 1850 | `						PH7_MemObjTryInteger(pObj);` |
|        5 | 1851 | `					}else{` |
|        - | 1852 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 1853 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 1854 | `						sxi64 r;` |
|      189 | 1855 | `						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 1856 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        3 | 1857 | `							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;` |
|        3 | 1858 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 1859 | `#else` |
|        - | 1860 | `							pObj->x.iVal = r;` |
|        - | 1861 | `#endif` |
|        2 | 1862 | `						}else{` |
|      187 | 1863 | `							pObj->x.iVal = r;` |
|        - | 1864 | `						}` |
|        - | 1865 | `					}` |
|      197 | 1866 | `					if( pInstr->iP1 ){` |
|        - | 1867 | `						/* Pre-decrement: result is the new value. */` |
|        3 | 1868 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 | 1869 | `					}` |
|        - | 1870 | `					/* Post-decrement: pTos retains the old value. */` |
|        - | 1871 | `				}` |
|       97 | 1872 | `			}` |
|      100 | 1873 | `		}else{` |
|      ! 0 | 1874 | `			if( pInstr->iP1 ){` |
|      ! 0 | 1875 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - | 1876 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 | 1877 | `				}else{` |
|        - | 1878 | `					/* Force a numeric cast */` |
|      ! 0 | 1879 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 1880 | `					/* Pre-decrement */` |
|      ! 0 | 1881 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 1882 | `						pTos->rVal--;` |
|        - | 1883 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 | 1884 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 1885 | `					}else{` |
|        - | 1886 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 1887 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 1888 | `						sxi64 r;` |
|      ! 0 | 1889 | `						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 1890 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 1891 | `							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;` |
|      ! 0 | 1892 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 1893 | `#else` |
|        - | 1894 | `							pTos->x.iVal = r;` |
|        - | 1895 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 1896 | `#endif` |
|      ! 0 | 1897 | `						}else{` |
|      ! 0 | 1898 | `							pTos->x.iVal = r;` |
|      ! 0 | 1899 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 1900 | `						}` |
|        - | 1901 | `					}` |
|        - | 1902 | `				}` |
|      ! 0 | 1903 | `			}` |
|        - | 1904 | `		}` |
|       97 | 1905 | `	}` |
|      197 | 1906 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|      197 | 1907 | `	break;` |
|        - | 1908 | `/*` |
|        - | 1909 | ` * UMINUS: * * *` |
|        - | 1910 | ` *` |
|        - | 1911 | ` * Perform a unary minus operation.` |
|        - | 1912 | ` */` |
|    35273 | 1913 | `case PH7_OP_UMINUS:` |
|        - | 1914 | `#ifdef UNTRUST` |
|        - | 1915 | `	if( pTos < pStack ){` |
|        - | 1916 | `		goto Abort;` |
|        - | 1917 | `	}` |
|        - | 1918 | `#endif` |
|        - | 1919 | `	/* Force a numeric (integer,real or both) cast */` |
|    70551 | 1920 | `	PH7_MemObjToNumeric(pTos);` |
|    70551 | 1921 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      108 | 1922 | `		pTos->rVal = -pTos->rVal;` |
|       53 | 1923 | `	}` |
|    70551 | 1924 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    70465 | 1925 | `		if( pTos->x.iVal == SMALLEST_INT64 ){` |
|        - | 1926 | `			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a` |
|        - | 1927 | `			 * REAL representation is already present it is the negated` |
|        - | 1928 | `			 * authoritative value, so just drop the now-stale cached int. The` |
|        - | 1929 | `			 * integer-only build has no float type, so it wraps (two's` |
|        - | 1930 | `			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */` |
|        - | 1931 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        5 | 1932 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 | 1933 | `				pTos->rVal = -(ph7_real)pTos->x.iVal;` |
|        5 | 1934 | `				MemObjSetType(pTos,MEMOBJ_REAL);` |
|        3 | 1935 | `			}else{` |
|      ! 0 | 1936 | `				pTos->iFlags &= ~MEMOBJ_INT;` |
|        - | 1937 | `			}` |
|        - | 1938 | `#else` |
|        - | 1939 | `			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);` |
|        - | 1940 | `#endif` |
|        3 | 1941 | `		}else{` |
|    70461 | 1942 | `			pTos->x.iVal = -pTos->x.iVal;` |
|        - | 1943 | `		}` |
|    35230 | 1944 | `	}` |
|    70551 | 1945 | `	break;` |
|        - | 1946 | `/*` |
|        - | 1947 | ` * UPLUS: * * *` |
|        - | 1948 | ` *` |
|        - | 1949 | ` * Perform a unary plus operation.` |
|        - | 1950 | ` */` |
|       18 | 1951 | `case PH7_OP_UPLUS:` |
|        - | 1952 | `#ifdef UNTRUST` |
|        - | 1953 | `	if( pTos < pStack ){` |
|        - | 1954 | `		goto Abort;` |
|        - | 1955 | `	}` |
|        - | 1956 | `#endif` |
|        - | 1957 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 | 1958 | `	PH7_MemObjToNumeric(pTos);` |
|       37 | 1959 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 1960 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 | 1961 | `	}` |
|       37 | 1962 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 | 1963 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 | 1964 | `	}` |
|       37 | 1965 | `	break;` |
|        - | 1966 | `/*` |
|        - | 1967 | ` * OP_LNOT: * * *` |
|        - | 1968 | ` *` |
|        - | 1969 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - | 1970 | ` * with its complement.` |
|        - | 1971 | ` */` |
|    60244 | 1972 | `case PH7_OP_LNOT:` |
|        - | 1973 | `#ifdef UNTRUST` |
|        - | 1974 | `	if( pTos < pStack ){` |
|        - | 1975 | `		goto Abort;` |
|        - | 1976 | `	}` |
|        - | 1977 | `#endif` |
|        - | 1978 | `	/* Force a boolean cast */` |
|   120844 | 1979 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      119 | 1980 | `		PH7_MemObjToBool(pTos);` |
|       57 | 1981 | `	}` |
|   120844 | 1982 | `	pTos->x.iVal = !pTos->x.iVal;` |
|   120844 | 1983 | `	break;` |
|        - | 1984 | `/*` |
|        - | 1985 | ` * OP_BITNOT: * * *` |
|        - | 1986 | ` *` |
|        - | 1987 | ` * Interpret the top of the stack as an value.Replace it` |
|        - | 1988 | ` * with its ones-complement.` |
|        - | 1989 | ` */` |
|        5 | 1990 | `case PH7_OP_BITNOT:` |
|        - | 1991 | `#ifdef UNTRUST` |
|        - | 1992 | `	if( pTos < pStack ){` |
|        - | 1993 | `		goto Abort;` |
|        - | 1994 | `	}` |
|        - | 1995 | `#endif` |
|        - | 1996 | `	/* Force an integer cast (php deprecates a lossy float here too) */` |
|       11 | 1997 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       11 | 1998 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|       11 | 1999 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2000 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 2001 | `	}` |
|       11 | 2002 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       11 | 2003 | `	break;` |
|        - | 2004 | `/* OP_MUL * * *` |
|        - | 2005 | ` * OP_MUL_STORE * * *` |
|        - | 2006 | ` *` |
|        - | 2007 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - | 2008 | ` * and push the result back onto the stack.` |
|        - | 2009 | ` */` |
|     1434 | 2010 | `case PH7_OP_MUL:` |
|        - | 2011 | `case PH7_OP_MUL_STORE: {` |
|        - | 2012 | `	VmOpRc rcOp;` |
|     2871 | 2013 | `	sState.pTos = pTos;` |
|     2871 | 2014 | `	sState.pc = pc;` |
|     2871 | 2015 | `	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);` |
|     2871 | 2016 | `	pTos = sState.pTos;` |
|     2871 | 2017 | `	pc = sState.pc;` |
|     2871 | 2018 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2019 | `		goto Abort;` |
|     2871 | 2020 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2021 | `		goto Exception;` |
|        - | 2022 | `	}` |
|     2871 | 2023 | `	break;` |
|        - | 2024 | `					  }` |
|        - | 2025 | `/* OP_POW * * *` |
|        - | 2026 | ` * OP_POW_STORE * * *` |
|        - | 2027 | ` *` |
|        - | 2028 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - | 2029 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - | 2030 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - | 2031 | ` * fits in sxi64; otherwise the result is a double.` |
|        - | 2032 | ` */` |
|       68 | 2033 | `case PH7_OP_POW:` |
|        - | 2034 | `case PH7_OP_POW_STORE: {` |
|        - | 2035 | `	VmOpRc rcOp;` |
|      137 | 2036 | `	sState.pTos = pTos;` |
|      137 | 2037 | `	sState.pc = pc;` |
|      137 | 2038 | `	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);` |
|      137 | 2039 | `	pTos = sState.pTos;` |
|      137 | 2040 | `	pc = sState.pc;` |
|      137 | 2041 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2042 | `		goto Abort;` |
|      137 | 2043 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 2044 | `		goto Exception;` |
|        - | 2045 | `	}` |
|      135 | 2046 | `	break;` |
|        - | 2047 | `					  }` |
|        - | 2048 | `/* OP_ADD * * *` |
|        - | 2049 | ` *` |
|        - | 2050 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 2051 | ` * and push the result back onto the stack.` |
|        - | 2052 | ` */` |
|     5539 | 2053 | `case PH7_OP_ADD:{` |
|    11083 | 2054 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2055 | `#ifdef UNTRUST` |
|        - | 2056 | `	if( pNos < pStack ){` |
|        - | 2057 | `		goto Abort;` |
|        - | 2058 | `	}` |
|        - | 2059 | `#endif` |
|        - | 2060 | `	{` |
|        - | 2061 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|        - | 2062 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|        - | 2063 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|        - | 2064 | `		SyBlob sArMsg;` |
|    11083 | 2065 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    11083 | 2066 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 2067 | `			sxi32 rcAr;` |
|        9 | 2068 | `			VmPopOperand(&pTos,1);` |
|        9 | 2069 | `			PH7_MemObjRelease(pTos);` |
|        9 | 2070 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        9 | 2071 | `			pTos->nIdx = SXU32_HIGH;` |
|       13 | 2072 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|        4 | 2073 | `				SyBlobLength(&sArMsg));` |
|        9 | 2074 | `			SyBlobRelease(&sArMsg);` |
|        9 | 2075 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|        9 | 2076 | `			rc = rcAr;` |
|        9 | 2077 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2078 | `		}` |
|    11075 | 2079 | `		SyBlobRelease(&sArMsg);` |
|        - | 2080 | `	}` |
|        - | 2081 | `	/* Perform the addition */` |
|    11075 | 2082 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|    11075 | 2083 | `	VmPopOperand(&pTos,1);` |
|    11075 | 2084 | `	break;` |
|        - | 2085 | `				}` |
|        - | 2086 | `/*` |
|        - | 2087 | ` * OP_ADD_STORE * * *` |
|        - | 2088 | ` *` |
|        - | 2089 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 2090 | ` * and push the result back onto the stack.` |
|        - | 2091 | ` */` |
|      979 | 2092 | `case PH7_OP_ADD_STORE:{` |
|     1963 | 2093 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2094 | `	ph7_value *pObj;` |
|        - | 2095 | `	sxu32 nIdx;` |
|        - | 2096 | `#ifdef UNTRUST` |
|        - | 2097 | `	if( pNos < pStack ){` |
|        - | 2098 | `		goto Abort;` |
|        - | 2099 | `	}` |
|        - | 2100 | `#endif` |
|        - | 2101 | `	{` |
|        - | 2102 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 2103 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 2104 | `		SyBlob sArMsg;` |
|     1963 | 2105 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     1963 | 2106 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 2107 | `			sxi32 rcAr;` |
|      ! 0 | 2108 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 2109 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 2110 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 2111 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 2112 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 2113 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 2114 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 2115 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 2116 | `			rc = rcAr;` |
|      ! 0 | 2117 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2118 | `		}` |
|     1963 | 2119 | `		SyBlobRelease(&sArMsg);` |
|        - | 2120 | `	}` |
|        - | 2121 | `	/* Perform the addition */` |
|     1963 | 2122 | `	nIdx = pTos->nIdx;` |
|     1963 | 2123 | `	if( nIdx == pVm->nGlobalIdx ){` |
|        - | 2124 | `		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment` |
|        - | 2125 | `		 * (a compile-time fatal in php; raised here, same message). */` |
|        3 | 2126 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 2127 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 2128 | `		pVm->iExitStatus = 255;` |
|        3 | 2129 | `		pVm->bHaltRequested = 1;` |
|        3 | 2130 | `		goto Abort;` |
|        - | 2131 | `	}` |
|     1961 | 2132 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - | 2133 | `	/* Peform the store operation */` |
|     1961 | 2134 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 2135 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1961 | 2136 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1961 | 2137 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1961 | 2138 | `		PH7_MemObjStore(pTos,pObj);` |
|      978 | 2139 | `	}` |
|     1961 | 2140 | `	PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|        - | 2141 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1961 | 2142 | `	PH7_MemObjStore(pTos,pNos);` |
|     1961 | 2143 | `	VmPopOperand(&pTos,1);` |
|     1961 | 2144 | `	break;` |
|        - | 2145 | `				}` |
|        - | 2146 | `/* OP_SUB * * *` |
|        - | 2147 | ` *` |
|        - | 2148 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 2149 | ` * first (what was next on the stack) from the second (the` |
|        - | 2150 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 2151 | ` */` |
|     4104 | 2152 | `case PH7_OP_SUB: {` |
|        - | 2153 | `	VmOpRc rcOp;` |
|     8213 | 2154 | `	sState.pTos = pTos;` |
|     8213 | 2155 | `	sState.pc = pc;` |
|     8213 | 2156 | `	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);` |
|     8213 | 2157 | `	pTos = sState.pTos;` |
|     8213 | 2158 | `	pc = sState.pc;` |
|     8213 | 2159 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2160 | `		goto Abort;` |
|     8213 | 2161 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2162 | `		goto Exception;` |
|        - | 2163 | `	}` |
|     8213 | 2164 | `	break;` |
|        - | 2165 | `					  }` |
|        - | 2166 | `/* OP_SUB_STORE * * *` |
|        - | 2167 | ` *` |
|        - | 2168 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 2169 | ` * first (what was next on the stack) from the second (the` |
|        - | 2170 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 2171 | ` */` |
|        6 | 2172 | `case PH7_OP_SUB_STORE: {` |
|        - | 2173 | `	VmOpRc rcOp;` |
|       14 | 2174 | `	sState.pTos = pTos;` |
|       14 | 2175 | `	sState.pc = pc;` |
|       14 | 2176 | `	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);` |
|       14 | 2177 | `	pTos = sState.pTos;` |
|       14 | 2178 | `	pc = sState.pc;` |
|       14 | 2179 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2180 | `		goto Abort;` |
|       14 | 2181 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 2182 | `		goto Exception;` |
|        - | 2183 | `	}` |
|       12 | 2184 | `	break;` |
|        - | 2185 | `					  }` |
|        - | 2186 |  |
|        - | 2187 | `/*` |
|        - | 2188 | ` * OP_MOD * * *` |
|        - | 2189 | ` *` |
|        - | 2190 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2191 | ` * first (what was next on the stack) from the second (the` |
|        - | 2192 | ` * top of the stack) and push the remainder after division` |
|        - | 2193 | ` * onto the stack.` |
|        - | 2194 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 2195 | ` */` |
|      525 | 2196 | `case PH7_OP_MOD: {` |
|        - | 2197 | `	VmOpRc rcOp;` |
|     1055 | 2198 | `	sState.pTos = pTos;` |
|     1055 | 2199 | `	sState.pc = pc;` |
|     1055 | 2200 | `	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);` |
|     1055 | 2201 | `	pTos = sState.pTos;` |
|     1055 | 2202 | `	pc = sState.pc;` |
|     1055 | 2203 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2204 | `		goto Abort;` |
|     1055 | 2205 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 2206 | `		goto Exception;` |
|        - | 2207 | `	}` |
|     1051 | 2208 | `	break;` |
|        - | 2209 | `					  }` |
|        - | 2210 | `/*` |
|        - | 2211 | ` * OP_MOD_STORE * * *` |
|        - | 2212 | ` *` |
|        - | 2213 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2214 | ` * first (what was next on the stack) from the second (the` |
|        - | 2215 | ` * top of the stack) and push the remainder after division` |
|        - | 2216 | ` * onto the stack.` |
|        - | 2217 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 2218 | ` */` |
|        4 | 2219 | `case PH7_OP_MOD_STORE: {` |
|        - | 2220 | `	VmOpRc rcOp;` |
|        9 | 2221 | `	sState.pTos = pTos;` |
|        9 | 2222 | `	sState.pc = pc;` |
|        9 | 2223 | `	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);` |
|        9 | 2224 | `	pTos = sState.pTos;` |
|        9 | 2225 | `	pc = sState.pc;` |
|        9 | 2226 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2227 | `		goto Abort;` |
|        9 | 2228 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 2229 | `		goto Exception;` |
|        - | 2230 | `	}` |
|        5 | 2231 | `	break;` |
|        - | 2232 | `					  }` |
|        - | 2233 | `/*` |
|        - | 2234 | ` * OP_DIV * * *` |
|        - | 2235 | ` *` |
|        - | 2236 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2237 | ` * first (what was next on the stack) from the second (the` |
|        - | 2238 | ` * top of the stack) and push the result onto the stack.` |
|        - | 2239 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 2240 | ` */` |
|       47 | 2241 | `case PH7_OP_DIV: {` |
|        - | 2242 | `	VmOpRc rcOp;` |
|       97 | 2243 | `	sState.pTos = pTos;` |
|       97 | 2244 | `	sState.pc = pc;` |
|       97 | 2245 | `	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);` |
|       97 | 2246 | `	pTos = sState.pTos;` |
|       97 | 2247 | `	pc = sState.pc;` |
|       97 | 2248 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2249 | `		goto Abort;` |
|       97 | 2250 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 2251 | `		goto Exception;` |
|        - | 2252 | `	}` |
|       91 | 2253 | `	break;` |
|        - | 2254 | `					  }` |
|        - | 2255 | `/*` |
|        - | 2256 | ` * OP_DIV_STORE * * *` |
|        - | 2257 | ` *` |
|        - | 2258 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2259 | ` * first (what was next on the stack) from the second (the` |
|        - | 2260 | ` * top of the stack) and push the result onto the stack.` |
|        - | 2261 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 2262 | ` */` |
|        4 | 2263 | `case PH7_OP_DIV_STORE:{` |
|        9 | 2264 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2265 | `	ph7_value *pObj;` |
|        - | 2266 | `	ph7_real a,b,r;` |
|        - | 2267 | `#ifdef UNTRUST` |
|        - | 2268 | `	if( pNos < pStack ){` |
|        - | 2269 | `		goto Abort;` |
|        - | 2270 | `	}` |
|        - | 2271 | `#endif` |
|        - | 2272 | `	{` |
|        - | 2273 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 2274 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 2275 | `		SyBlob sArMsg;` |
|        9 | 2276 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|        9 | 2277 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"/",&sArMsg) != SXRET_OK ){` |
|        - | 2278 | `			sxi32 rcAr;` |
|      ! 0 | 2279 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 2280 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 2281 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 2282 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 2283 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 2284 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 2285 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 2286 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 2287 | `			rc = rcAr;` |
|      ! 0 | 2288 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2289 | `		}` |
|        9 | 2290 | `		SyBlobRelease(&sArMsg);` |
|        - | 2291 | `	}` |
|        - | 2292 | `	/* Force the operands to be real */` |
|        9 | 2293 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 | 2294 | `		PH7_MemObjToReal(pTos);` |
|        4 | 2295 | `	}` |
|        9 | 2296 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 | 2297 | `		PH7_MemObjToReal(pNos);` |
|        4 | 2298 | `	}` |
|        - | 2299 | `	/* Perform the requested operation */` |
|        9 | 2300 | `	a = pTos->rVal;` |
|        9 | 2301 | `	b = pNos->rVal;` |
|        9 | 2302 | `	if( b == 0 ){` |
|        - | 2303 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|        - | 2304 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|        3 | 2305 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|        3 | 2306 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      ! 0 | 2307 | `	}else{` |
|        7 | 2308 | `		r = a/b;` |
|        - | 2309 | `		/* Push the result */` |
|        7 | 2310 | `		pNos->rVal = r;` |
|        7 | 2311 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - | 2312 | `		/* Try to get an integer representation */` |
|        7 | 2313 | `		PH7_MemObjTryInteger(pNos);` |
|        - | 2314 | `	}` |
|        7 | 2315 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 2316 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        7 | 2317 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        7 | 2318 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        7 | 2319 | `		PH7_MemObjStore(pNos,pObj);` |
|        3 | 2320 | `	}` |
|        7 | 2321 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|        7 | 2322 | `	VmPopOperand(&pTos,1);` |
|        7 | 2323 | `	break;` |
|        - | 2324 | `				}` |
|        - | 2325 | `/* OP_BAND * * *` |
|        - | 2326 | ` *` |
|        - | 2327 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2328 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 2329 | ` * two elements.` |
|        - | 2330 | `*/` |
|        - | 2331 | `/* OP_BOR * * *` |
|        - | 2332 | ` *` |
|        - | 2333 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2334 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 2335 | ` * two elements.` |
|        - | 2336 | ` */` |
|        - | 2337 | `/* OP_BXOR * * *` |
|        - | 2338 | ` *` |
|        - | 2339 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2340 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 2341 | ` * two elements.` |
|        - | 2342 | ` */` |
|      460 | 2343 | `case PH7_OP_BAND:` |
|        - | 2344 | `case PH7_OP_BOR:` |
|        - | 2345 | `case PH7_OP_BXOR:{` |
|      925 | 2346 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2347 | `	sxi64 a,b,r;` |
|        - | 2348 | `#ifdef UNTRUST` |
|        - | 2349 | `	if( pNos < pStack ){` |
|        - | 2350 | `		goto Abort;` |
|        - | 2351 | `	}` |
|        - | 2352 | `#endif` |
|        - | 2353 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|      925 | 2354 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|      925 | 2355 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      925 | 2356 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      925 | 2357 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      925 | 2358 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2359 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 2360 | `	}` |
|      925 | 2361 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2362 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 | 2363 | `	}` |
|        - | 2364 | `	/* Perform the requested operation */` |
|      925 | 2365 | `	a = pNos->x.iVal;` |
|      925 | 2366 | `	b = pTos->x.iVal;` |
|      925 | 2367 | `	switch(pInstr->iOp){` |
|       42 | 2368 | `	case PH7_OP_BOR_STORE:` |
|       86 | 2369 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 | 2370 | `	case PH7_OP_BXOR_STORE:` |
|       15 | 2371 | `	case PH7_OP_BXOR: r = a^b; break;` |
|      411 | 2372 | `	case PH7_OP_BAND_STORE:` |
|      411 | 2373 | `	case PH7_OP_BAND:` |
|      827 | 2374 | `	default:          r = a&b; break;` |
|        - | 2375 | `	}` |
|        - | 2376 | `	/* Push the result */` |
|      925 | 2377 | `	pNos->x.iVal = r;` |
|      925 | 2378 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      925 | 2379 | `	VmPopOperand(&pTos,1);` |
|      925 | 2380 | `	break;` |
|        - | 2381 | `				 }` |
|        - | 2382 | `/* OP_BAND_STORE * * *` |
|        - | 2383 | ` *` |
|        - | 2384 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2385 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 2386 | ` * two elements.` |
|        - | 2387 | `*/` |
|        - | 2388 | `/* OP_BOR_STORE * * *` |
|        - | 2389 | ` *` |
|        - | 2390 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2391 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 2392 | ` * two elements.` |
|        - | 2393 | ` */` |
|        - | 2394 | `/* OP_BXOR_STORE * * *` |
|        - | 2395 | ` *` |
|        - | 2396 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2397 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 2398 | ` * two elements.` |
|        - | 2399 | ` */` |
|       17 | 2400 | `case PH7_OP_BAND_STORE:` |
|        - | 2401 | `case PH7_OP_BOR_STORE:` |
|        - | 2402 | `case PH7_OP_BXOR_STORE:{` |
|       35 | 2403 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2404 | `	ph7_value *pObj;` |
|        - | 2405 | `	sxi64 a,b,r;` |
|        - | 2406 | `#ifdef UNTRUST` |
|        - | 2407 | `	if( pNos < pStack ){` |
|        - | 2408 | `		goto Abort;` |
|        - | 2409 | `	}` |
|        - | 2410 | `#endif` |
|        - | 2411 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|       35 | 2412 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|       35 | 2413 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|       35 | 2414 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       35 | 2415 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|       35 | 2416 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2417 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 2418 | `	}` |
|       35 | 2419 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2420 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 | 2421 | `	}` |
|        - | 2422 | `	/* Perform the requested operation */` |
|       35 | 2423 | `	a = pTos->x.iVal;` |
|       35 | 2424 | `	b = pNos->x.iVal;` |
|       35 | 2425 | `	switch(pInstr->iOp){` |
|       10 | 2426 | `	case PH7_OP_BOR_STORE:` |
|       21 | 2427 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 | 2428 | `	case PH7_OP_BXOR_STORE:` |
|        9 | 2429 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 | 2430 | `	case PH7_OP_BAND_STORE:` |
|        3 | 2431 | `	case PH7_OP_BAND:` |
|        7 | 2432 | `	default:          r = a&b; break;` |
|        - | 2433 | `	}` |
|        - | 2434 | `	/* Push the result */` |
|       35 | 2435 | `	pNos->x.iVal = r;` |
|       35 | 2436 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       35 | 2437 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 2438 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       35 | 2439 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       35 | 2440 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       35 | 2441 | `		PH7_MemObjStore(pNos,pObj);` |
|       17 | 2442 | `	}` |
|       35 | 2443 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       35 | 2444 | `	VmPopOperand(&pTos,1);` |
|       35 | 2445 | `	break;` |
|        - | 2446 | `				 }` |
|        - | 2447 | `/* OP_SHL * * *` |
|        - | 2448 | ` *` |
|        - | 2449 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2450 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 2451 | ` * left by N bits where N is the top element on the stack.` |
|        - | 2452 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 2453 | ` */` |
|        - | 2454 | `/* OP_SHR * * *` |
|        - | 2455 | ` *` |
|        - | 2456 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2457 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 2458 | ` * right by N bits where N is the top element on the stack.` |
|        - | 2459 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 2460 | ` */` |
|       29 | 2461 | `case PH7_OP_SHL:` |
|        - | 2462 | `case PH7_OP_SHR: {` |
|        - | 2463 | `	VmOpRc rcOp;` |
|       60 | 2464 | `	sState.pTos = pTos;` |
|       60 | 2465 | `	sState.pc = pc;` |
|       60 | 2466 | `	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);` |
|       60 | 2467 | `	pTos = sState.pTos;` |
|       60 | 2468 | `	pc = sState.pc;` |
|       60 | 2469 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2470 | `		goto Abort;` |
|       60 | 2471 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2472 | `		goto Exception;` |
|        - | 2473 | `	}` |
|       60 | 2474 | `	break;` |
|        - | 2475 | `					  }` |
|        - | 2476 | `/*  OP_SHL_STORE * * *` |
|        - | 2477 | ` *` |
|        - | 2478 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2479 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 2480 | ` * left by N bits where N is the top element on the stack.` |
|        - | 2481 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 2482 | ` */` |
|        - | 2483 | `/* OP_SHR_STORE * * *` |
|        - | 2484 | ` *` |
|        - | 2485 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2486 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 2487 | ` * right by N bits where N is the top element on the stack.` |
|        - | 2488 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 2489 | ` */` |
|        9 | 2490 | `case PH7_OP_SHL_STORE:` |
|        - | 2491 | `case PH7_OP_SHR_STORE: {` |
|        - | 2492 | `	VmOpRc rcOp;` |
|       19 | 2493 | `	sState.pTos = pTos;` |
|       19 | 2494 | `	sState.pc = pc;` |
|       19 | 2495 | `	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);` |
|       19 | 2496 | `	pTos = sState.pTos;` |
|       19 | 2497 | `	pc = sState.pc;` |
|       19 | 2498 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2499 | `		goto Abort;` |
|       19 | 2500 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2501 | `		goto Exception;` |
|        - | 2502 | `	}` |
|       19 | 2503 | `	break;` |
|        - | 2504 | `					  }` |
|        - | 2505 | `/* CAT:  P1 * *` |
|        - | 2506 | ` *` |
|        - | 2507 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - | 2508 | ` * back.` |
|        - | 2509 | ` */` |
|    85313 | 2510 | `case PH7_OP_CAT:{` |
|        - | 2511 | `	ph7_value *pNos,*pCur;` |
|   170631 | 2512 | `	if( pInstr->iP1 < 1 ){` |
|   142427 | 2513 | `		pNos = &pTos[-1];` |
|    71216 | 2514 | `	}else{` |
|    28209 | 2515 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - | 2516 | `	}` |
|        - | 2517 | `#ifdef UNTRUST` |
|        - | 2518 | `	if( pNos < pStack ){` |
|        - | 2519 | `		goto Abort;` |
|        - | 2520 | `	}` |
|        - | 2521 | `#endif` |
|        - | 2522 | `	/* Force a string cast */` |
|   170631 | 2523 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1929 | 2524 | `		PH7_MemObjToString(pNos);` |
|      962 | 2525 | `	}` |
|   170631 | 2526 | `	pCur = &pNos[1];` |
|   344595 | 2527 | `	while( pCur <= pTos ){` |
|   173969 | 2528 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    51559 | 2529 | `			PH7_MemObjToString(pCur);` |
|    25777 | 2530 | `		}` |
|        - | 2531 | `		/* Perform the concatenation */` |
|   173969 | 2532 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   173893 | 2533 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - | 2534 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 | 2535 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 2536 | `				goto Abort;` |
|        - | 2537 | `			}` |
|    86944 | 2538 | `		}` |
|   173969 | 2539 | `		SyBlobRelease(&pCur->sBlob);` |
|   173969 | 2540 | `		pCur++;` |
|        5 | 2541 | `	}` |
|   170631 | 2542 | `	pTos = pNos;` |
|   170631 | 2543 | `	break;` |
|        - | 2544 | `				}` |
|        - | 2545 | `/*  CAT_STORE: * * *` |
|        - | 2546 | ` *` |
|        - | 2547 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - | 2548 | ` * back.` |
|        - | 2549 | ` */` |
|    11997 | 2550 | `case PH7_OP_CAT_STORE:{` |
|    23999 | 2551 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2552 | `	ph7_value *pObj;` |
|        - | 2553 | `	sxu32 nIdx;` |
|        - | 2554 | `#ifdef UNTRUST` |
|        - | 2555 | `	if( pNos < pStack ){` |
|        - | 2556 | `		goto Abort;` |
|        - | 2557 | `	}` |
|        - | 2558 | `#endif` |
|        - | 2559 | `	/* The right operand must be a string to append it */` |
|    23999 | 2560 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       59 | 2561 | `		PH7_MemObjToString(pNos);` |
|       29 | 2562 | `	}` |
|    23999 | 2563 | `	nIdx = pTos->nIdx;` |
|        - | 2564 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - | 2565 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - | 2566 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - | 2567 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - | 2568 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - | 2569 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - | 2570 | `	 * the source we copy from — references share the slot index, so one check` |
|        - | 2571 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - | 2572 | `	 * must run before any mutation (left to the slow path).` |
|        - | 2573 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - | 2574 | `	 * and remains O(n^2) by design. */` |
|    23994 | 2575 | `	if( nIdx != SXU32_HIGH` |
|    23994 | 2576 | `	 && nIdx != pNos->nIdx` |
|    23990 | 2577 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|    23991 | 2578 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|    14590 | 2579 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|    23985 | 2580 | `		if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 2581 | `			/* e.g. $x = 5; $x .= "a";  ->  "5a" */` |
|        3 | 2582 | `			PH7_MemObjToString(pObj);` |
|        1 | 2583 | `		}` |
|    23985 | 2584 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|    23981 | 2585 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 2586 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - | 2587 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 | 2588 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 2589 | `				goto Abort;` |
|        - | 2590 | `			}` |
|    11988 | 2591 | `		}` |
|        - | 2592 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - | 2593 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - | 2594 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - | 2595 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - | 2596 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - | 2597 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - | 2598 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - | 2599 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - | 2600 | `		 * the same slot is appended to again later in the statement` |
|        - | 2601 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - | 2602 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - | 2603 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|    23985 | 2604 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 | 2605 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 | 2606 | `		}` |
|        - | 2607 | `		/* A hooked lvalue's scratch slot was appended in place — dispatch the` |
|        - | 2608 | `		 * set side now (the consume reads the computed value from the slot). */` |
|    23985 | 2609 | `		PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|    23985 | 2610 | `		pNos->nIdx = SXU32_HIGH;` |
|    23985 | 2611 | `		VmPopOperand(&pTos,1);` |
|    23992 | 2612 | `		break;` |
|        - | 2613 | `	}` |
|        - | 2614 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|       16 | 2615 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 2616 | `		/* Force a string cast */` |
|        6 | 2617 | `		PH7_MemObjToString(pTos);` |
|        2 | 2618 | `	}` |
|        - | 2619 | `	/* Perform the concatenation (Reverse order) */` |
|       16 | 2620 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 | 2621 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 2622 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - | 2623 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 | 2624 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 2625 | `			goto Abort;` |
|        - | 2626 | `		}` |
|        7 | 2627 | `	}` |
|        - | 2628 | `	/* Perform the store operation */` |
|       16 | 2629 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 2630 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 | 2631 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       16 | 2632 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 | 2633 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 | 2634 | `	}` |
|       11 | 2635 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       11 | 2636 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 | 2637 | `	VmPopOperand(&pTos,1);` |
|       11 | 2638 | `	break;` |
|        - | 2639 | `				}` |
|        - | 2640 | `/* OP_AND: * * *` |
|        - | 2641 | ` *` |
|        - | 2642 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - | 2643 | ` * two values and push the resulting boolean value back onto the` |
|        - | 2644 | ` * stack.` |
|        - | 2645 | ` */` |
|        - | 2646 | `/* OP_OR: * * *` |
|        - | 2647 | ` *` |
|        - | 2648 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - | 2649 | ` * two values and push the resulting boolean value back onto the` |
|        - | 2650 | ` * stack.` |
|        - | 2651 | ` */` |
|   142287 | 2652 | `case PH7_OP_LAND:` |
|        - | 2653 | `case PH7_OP_LOR: {` |
|        - | 2654 | `	VmOpRc rcOp;` |
|   285029 | 2655 | `	sState.pTos = pTos;` |
|   285029 | 2656 | `	sState.pc = pc;` |
|   285029 | 2657 | `	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);` |
|   285029 | 2658 | `	pTos = sState.pTos;` |
|   285029 | 2659 | `	pc = sState.pc;` |
|   285029 | 2660 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2661 | `		goto Abort;` |
|   285029 | 2662 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2663 | `		goto Exception;` |
|        - | 2664 | `	}` |
|   285029 | 2665 | `	break;` |
|        - | 2666 | `					  }` |
|        - | 2667 | `/*` |
|        - | 2668 | ` * OP_NULLC: * * *` |
|        - | 2669 | ` * Null coalescing operator '??'.` |
|        - | 2670 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - | 2671 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - | 2672 | ` */` |
|        - | 2673 | `/*` |
|        - | 2674 | ` * OP_NULLC: * P2 *` |
|        - | 2675 | ` * Short-circuit null coalescing '??'.` |
|        - | 2676 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - | 2677 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - | 2678 | ` */` |
|      368 | 2679 | `case PH7_OP_NULLC: {` |
|        - | 2680 | `#ifdef UNTRUST` |
|        - | 2681 | `	if( pTos < pStack ){` |
|        - | 2682 | `		goto Abort;` |
|        - | 2683 | `	}` |
|        - | 2684 | `#endif` |
|      741 | 2685 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 2686 | `		/* Left is not null — keep it and skip the RHS */` |
|      429 | 2687 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|      217 | 2688 | `	}else{` |
|        - | 2689 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|      316 | 2690 | `		VmPopOperand(&pTos, 1);` |
|        - | 2691 | `	}` |
|      741 | 2692 | `	break;` |
|        - | 2693 | `}` |
|        - | 2694 | `/*` |
|        - | 2695 | ` * OP_NULLC_JMP: * P2 *` |
|        - | 2696 | ` * Null coalescing assignment short-circuit.` |
|        - | 2697 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - | 2698 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - | 2699 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - | 2700 | ` */` |
|       44 | 2701 | `case PH7_OP_NULLC_JMP: {` |
|        - | 2702 | `#ifdef UNTRUST` |
|        - | 2703 | `	if( pTos < pStack ){` |
|        - | 2704 | `		goto Abort;` |
|        - | 2705 | `	}` |
|        - | 2706 | `#endif` |
|       91 | 2707 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       30 | 2708 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —` |
|        - | 2709 | `		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is` |
|        - | 2710 | `		 * dropped by the fetch-point sweep (the landing pc is past its` |
|        - | 2711 | `		 * OP_NULLC_STORE window): the skipped assign never dispatches. */` |
|       14 | 2712 | `	}` |
|       91 | 2713 | `	break;` |
|        - | 2714 | `}` |
|        - | 2715 | `/*` |
|        - | 2716 | ` * OP_NULLC_STORE: * * *` |
|        - | 2717 | ` * Null coalescing assignment store.` |
|        - | 2718 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - | 2719 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - | 2720 | ` * expression result.` |
|        - | 2721 | ` */` |
|        - | 2722 | `/*` |
|        - | 2723 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - | 2724 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - | 2725 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - | 2726 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - | 2727 | ` * non-null, fall through without modifying the stack so the following` |
|        - | 2728 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - | 2729 | ` */` |
|       53 | 2730 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - | 2731 | `#ifdef UNTRUST` |
|        - | 2732 | `	if( pTos < pStack ){` |
|        - | 2733 | `		goto Abort;` |
|        - | 2734 | `	}` |
|        - | 2735 | `#endif` |
|      109 | 2736 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - | 2737 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - | 2738 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       44 | 2739 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       21 | 2740 | `	}` |
|      109 | 2741 | `	break;` |
|        - | 2742 | `}` |
|       27 | 2743 | `case PH7_OP_NULLC_STORE: {` |
|        - | 2744 | `	VmOpRc rcOp;` |
|       57 | 2745 | `	sState.pTos = pTos;` |
|       57 | 2746 | `	sState.pc = pc;` |
|       57 | 2747 | `	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);` |
|       57 | 2748 | `	pTos = sState.pTos;` |
|       57 | 2749 | `	pc = sState.pc;` |
|       57 | 2750 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2751 | `		goto Abort;` |
|       57 | 2752 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2753 | `		goto Exception;` |
|        - | 2754 | `	}` |
|       57 | 2755 | `	break;` |
|        - | 2756 | `					  }` |
|        - | 2757 | `/*` |
|        - | 2758 | ` * OP_SPREAD: * * *` |
|        - | 2759 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - | 2760 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - | 2761 | ` * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this` |
|        - | 2762 | ` * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —` |
|        - | 2763 | ` * the CALL may not be the next instruction, and may be an inner call whose own` |
|        - | 2764 | ` * spreads must stay scoped to it.` |
|        - | 2765 | ` * The expansion tail is shared between the plain-array and the materialized` |
|        - | 2766 | ` * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.` |
|        - | 2767 | ` */` |
|      140 | 2768 | `case PH7_OP_SPREAD: {` |
|        - | 2769 | `#ifdef UNTRUST` |
|        - | 2770 | `	if( pTos < pStack ){` |
|        - | 2771 | `		goto Abort;` |
|        - | 2772 | `	}` |
|        - | 2773 | `#endif` |
|        - | 2774 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - | 2775 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - | 2776 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - | 2777 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - | 2778 | `	 * be freed immediately. */` |
|      284 | 2779 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 | 2780 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - | 2781 | `		sxi32 rcW;` |
|        3 | 2782 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 | 2783 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 | 2784 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 | 2785 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 2786 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 2787 | `			goto Exception;` |
|        - | 2788 | `		}` |
|        - | 2789 | `		/* Grow the operand stack if this expansion would overflow it (no longer a` |
|        - | 2790 | `		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a` |
|        - | 2791 | `		 * single argument — the historical bounded-fallback, now only under OOM. */` |
|        4 | 2792 | `		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,` |
|        1 | 2793 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 2794 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 2795 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 2796 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 2797 | `				pTmpMap->nEntry);` |
|      ! 0 | 2798 | `			break;` |
|        - | 2799 | `		}` |
|        3 | 2800 | `		VmSpreadExpandMap(pVm, &pTos, pTmpMap);` |
|        3 | 2801 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 | 2802 | `		break;` |
|        - | 2803 | `	}` |
|      282 | 2804 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      282 | 2805 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      421 | 2806 | `		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,` |
|      139 | 2807 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 2808 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 2809 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 2810 | `				pMap->nEntry);` |
|      ! 0 | 2811 | `			break;` |
|        - | 2812 | `		}` |
|      282 | 2813 | `		VmSpreadExpandMap(pVm, &pTos, pMap);` |
|      139 | 2814 | `	}` |
|        - | 2815 | `	/* else: not an array — leave as-is (single arg) */` |
|      282 | 2816 | `	break;` |
|        - | 2817 | `}` |
|        - | 2818 | `/*` |
|        - | 2819 | ` * OP_FLAG_SPREAD: * * *` |
|        - | 2820 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - | 2821 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - | 2822 | ` */` |
|      340 | 2823 | `case PH7_OP_FLAG_SPREAD: {` |
|        - | 2824 | `#ifdef UNTRUST` |
|        - | 2825 | `	if( pTos < pStack ){` |
|        - | 2826 | `		goto Abort;` |
|        - | 2827 | `	}` |
|        - | 2828 | `#endif` |
|      683 | 2829 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|      683 | 2830 | `	break;` |
|        - | 2831 | `}` |
|        - | 2832 | `/* OP_LXOR: * * *` |
|        - | 2833 | ` *` |
|        - | 2834 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - | 2835 | ` * two values and push the resulting boolean value back onto the` |
|        - | 2836 | ` * stack.` |
|        - | 2837 | ` * According to the PHP language reference manual:` |
|        - | 2838 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - | 2839 | ` *  TRUE,but not both.` |
|        - | 2840 | ` */` |
|        5 | 2841 | `case PH7_OP_LXOR:{` |
|       11 | 2842 | `	ph7_value *pNos = &pTos[-1];` |
|       11 | 2843 | `	sxi32 v = 0;` |
|        - | 2844 | `#ifdef UNTRUST` |
|        - | 2845 | `	if( pNos < pStack ){` |
|        - | 2846 | `		goto Abort;` |
|        - | 2847 | `	}` |
|        - | 2848 | `#endif` |
|        - | 2849 | `	/* Force a boolean cast */` |
|       11 | 2850 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 2851 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 2852 | `	}` |
|       11 | 2853 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 2854 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 | 2855 | `	}` |
|       11 | 2856 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 | 2857 | `		v = 1;` |
|        3 | 2858 | `	}` |
|       11 | 2859 | `	VmPopOperand(&pTos,1);` |
|       11 | 2860 | `	pTos->x.iVal = v;` |
|       11 | 2861 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 | 2862 | `	break;` |
|        - | 2863 | `				 }` |
|        - | 2864 | `/* OP_EQ P1 P2 P3` |
|        - | 2865 | ` *` |
|        - | 2866 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - | 2867 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - | 2868 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2869 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2870 | ` */` |
|        - | 2871 | `/* OP_NEQ P1 P2 P3` |
|        - | 2872 | ` *` |
|        - | 2873 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - | 2874 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 2875 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2876 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2877 | ` */` |
|     5434 | 2878 | `case PH7_OP_EQ:` |
|        - | 2879 | `case PH7_OP_NEQ: {` |
|        - | 2880 | `	VmOpRc rcOp;` |
|    10873 | 2881 | `	sState.pTos = pTos;` |
|    10873 | 2882 | `	sState.pc = pc;` |
|    10873 | 2883 | `	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);` |
|    10873 | 2884 | `	pTos = sState.pTos;` |
|    10873 | 2885 | `	pc = sState.pc;` |
|    10873 | 2886 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2887 | `		goto Abort;` |
|    10873 | 2888 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2889 | `		goto Exception;` |
|        - | 2890 | `	}` |
|    10873 | 2891 | `	break;` |
|        - | 2892 | `					  }` |
|        - | 2893 | `/* OP_TEQ P1 P2 *` |
|        - | 2894 | ` *` |
|        - | 2895 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - | 2896 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 2897 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2898 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2899 | ` */` |
|   220863 | 2900 | `case PH7_OP_TEQ: {` |
|        - | 2901 | `	VmOpRc rcOp;` |
|   441829 | 2902 | `	sState.pTos = pTos;` |
|   441829 | 2903 | `	sState.pc = pc;` |
|   441829 | 2904 | `	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);` |
|   441829 | 2905 | `	pTos = sState.pTos;` |
|   441829 | 2906 | `	pc = sState.pc;` |
|   441829 | 2907 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2908 | `		goto Abort;` |
|   441829 | 2909 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2910 | `		goto Exception;` |
|        - | 2911 | `	}` |
|   441829 | 2912 | `	break;` |
|        - | 2913 | `					  }` |
|        - | 2914 | `/* OP_TNE P1 P2 *` |
|        - | 2915 | ` *` |
|        - | 2916 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - | 2917 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - | 2918 | ` * instruction.` |
|        - | 2919 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2920 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2921 | ` *` |
|        - | 2922 | ` */` |
|   167070 | 2923 | `case PH7_OP_TNE: {` |
|        - | 2924 | `	VmOpRc rcOp;` |
|   334243 | 2925 | `	sState.pTos = pTos;` |
|   334243 | 2926 | `	sState.pc = pc;` |
|   334243 | 2927 | `	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);` |
|   334243 | 2928 | `	pTos = sState.pTos;` |
|   334243 | 2929 | `	pc = sState.pc;` |
|   334243 | 2930 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2931 | `		goto Abort;` |
|   334243 | 2932 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2933 | `		goto Exception;` |
|        - | 2934 | `	}` |
|   334243 | 2935 | `	break;` |
|        - | 2936 | `					  }` |
|        - | 2937 | `/* OP_LT P1 P2 P3` |
|        - | 2938 | ` *` |
|        - | 2939 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 2940 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 2941 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 2942 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2943 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2944 | ` *` |
|        - | 2945 | ` */` |
|        - | 2946 | `/* OP_LE P1 P2 P3` |
|        - | 2947 | ` *` |
|        - | 2948 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 2949 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 2950 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 2951 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2952 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2953 | ` *` |
|        - | 2954 | ` */` |
|   165634 | 2955 | `case PH7_OP_LT:` |
|        - | 2956 | `case PH7_OP_LE: {` |
|        - | 2957 | `	VmOpRc rcOp;` |
|   331821 | 2958 | `	sState.pTos = pTos;` |
|   331821 | 2959 | `	sState.pc = pc;` |
|   331821 | 2960 | `	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);` |
|   331821 | 2961 | `	pTos = sState.pTos;` |
|   331821 | 2962 | `	pc = sState.pc;` |
|   331821 | 2963 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2964 | `		goto Abort;` |
|   331821 | 2965 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2966 | `		goto Exception;` |
|        - | 2967 | `	}` |
|   331821 | 2968 | `	break;` |
|        - | 2969 | `					  }` |
|        - | 2970 | `/* OP_GT P1 P2 P3` |
|        - | 2971 | ` *` |
|        - | 2972 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 2973 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 2974 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 2975 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2976 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2977 | ` *` |
|        - | 2978 | ` */` |
|        - | 2979 | `/* OP_GE P1 P2 P3` |
|        - | 2980 | ` *` |
|        - | 2981 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 2982 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 2983 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 2984 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2985 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2986 | ` *` |
|        - | 2987 | ` */` |
|    79532 | 2988 | `case PH7_OP_GT:` |
|        - | 2989 | `case PH7_OP_GE: {` |
|        - | 2990 | `	VmOpRc rcOp;` |
|   159167 | 2991 | `	sState.pTos = pTos;` |
|   159167 | 2992 | `	sState.pc = pc;` |
|   159167 | 2993 | `	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);` |
|   159167 | 2994 | `	pTos = sState.pTos;` |
|   159167 | 2995 | `	pc = sState.pc;` |
|   159167 | 2996 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2997 | `		goto Abort;` |
|   159167 | 2998 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2999 | `		goto Exception;` |
|        - | 3000 | `	}` |
|   159167 | 3001 | `	break;` |
|        - | 3002 | `					  }` |
|        - | 3003 | `/* OP_SPACESHIP * * *` |
|        - | 3004 | ` *` |
|        - | 3005 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - | 3006 | ` *   -1 if left < right` |
|        - | 3007 | ` *    0 if left == right` |
|        - | 3008 | ` *    1 if left > right` |
|        - | 3009 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - | 3010 | ` */` |
|      208 | 3011 | `case PH7_OP_SPACESHIP: {` |
|        - | 3012 | `	VmOpRc rcOp;` |
|      417 | 3013 | `	sState.pTos = pTos;` |
|      417 | 3014 | `	sState.pc = pc;` |
|      417 | 3015 | `	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);` |
|      417 | 3016 | `	pTos = sState.pTos;` |
|      417 | 3017 | `	pc = sState.pc;` |
|      417 | 3018 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3019 | `		goto Abort;` |
|      417 | 3020 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3021 | `		goto Exception;` |
|        - | 3022 | `	}` |
|      417 | 3023 | `	break;` |
|        - | 3024 | `					  }` |
|        - | 3025 | `/*` |
|        - | 3026 | ` * OP_LOAD_REF * * *` |
|        - | 3027 | ` * Push the index of a referenced object on the stack.` |
|        - | 3028 | ` */` |
|       60 | 3029 | `case PH7_OP_LOAD_REF: {` |
|        - | 3030 | `	sxu32 nIdx;` |
|        - | 3031 | `#ifdef UNTRUST` |
|        - | 3032 | `	if( pTos < pStack ){` |
|        - | 3033 | `		goto Abort;` |
|        - | 3034 | `	}` |
|        - | 3035 | `#endif` |
|        - | 3036 | `	/* Extract memory object index */` |
|      121 | 3037 | `	nIdx = pTos->nIdx;` |
|      121 | 3038 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - | 3039 | `		/* Nullify the object */` |
|      121 | 3040 | `		PH7_MemObjRelease(pTos);` |
|        - | 3041 | `		/* Mark as constant and store the index on the top of the stack */` |
|      121 | 3042 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      121 | 3043 | `		pTos->nIdx = SXU32_HIGH;` |
|      121 | 3044 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       60 | 3045 | `	}` |
|      121 | 3046 | `	break;` |
|        - | 3047 | `					  }` |
|        - | 3048 | `/*` |
|        - | 3049 | ` * OP_STORE_REF * * P3` |
|        - | 3050 | ` * Perform an assignment operation by reference.` |
|        - | 3051 | ` */` |
|       26 | 3052 | `case PH7_OP_STORE_REF: {` |
|        - | 3053 | `	VmOpRc rcOp;` |
|       55 | 3054 | `	sState.pTos = pTos;` |
|       55 | 3055 | `	sState.pc = pc;` |
|       55 | 3056 | `	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);` |
|       55 | 3057 | `	pTos = sState.pTos;` |
|       55 | 3058 | `	pc = sState.pc;` |
|       55 | 3059 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3060 | `		goto Abort;` |
|       53 | 3061 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3062 | `		goto Exception;` |
|        - | 3063 | `	}` |
|       53 | 3064 | `	break;` |
|        - | 3065 | `					  }` |
|        - | 3066 | `/*` |
|        - | 3067 | ` * OP_UPLINK P1 * *` |
|        - | 3068 | ` * Link a variable to the top active VM frame.` |
|        - | 3069 | ` * This is used to implement the 'global' PHP construct.` |
|        - | 3070 | ` */` |
|       27 | 3071 | `case PH7_OP_UPLINK: {` |
|       59 | 3072 | `	if( pVm->pFrame->pParent ){` |
|       59 | 3073 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - | 3074 | `		SyString sName;` |
|        - | 3075 | `		/* Perform the link */` |
|      123 | 3076 | `		while( pLink <= pTos ){` |
|       69 | 3077 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 3078 | `				/* Force a string cast */` |
|      ! 0 | 3079 | `				PH7_MemObjToString(pLink);` |
|      ! 0 | 3080 | `			}` |
|       69 | 3081 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       69 | 3082 | `			if( sName.nByte > 0 ){` |
|       69 | 3083 | `				VmFrameLink(&(*pVm),&sName);` |
|       32 | 3084 | `			}` |
|       69 | 3085 | `			pLink++;` |
|        5 | 3086 | `		}` |
|       27 | 3087 | `	}` |
|       59 | 3088 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       59 | 3089 | `	break;` |
|        - | 3090 | `					}` |
|        - | 3091 | `/*` |
|        - | 3092 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - | 3093 | ` * Push an exception in the corresponding container so that` |
|        - | 3094 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - | 3095 | ` */` |
|     1335 | 3096 | `case PH7_OP_LOAD_EXCEPTION: {` |
|        - | 3097 | `	VmOpRc rcOp;` |
|     2675 | 3098 | `	sState.pTos = pTos;` |
|     2675 | 3099 | `	sState.pc = pc;` |
|     2675 | 3100 | `	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);` |
|     2675 | 3101 | `	pTos = sState.pTos;` |
|     2675 | 3102 | `	pc = sState.pc;` |
|     2675 | 3103 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3104 | `		goto Abort;` |
|     2675 | 3105 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3106 | `		goto Exception;` |
|        - | 3107 | `	}` |
|     2675 | 3108 | `	break;` |
|        - | 3109 | `					  }` |
|        - | 3110 | `/*` |
|        - | 3111 | ` * OP_POP_EXCEPTION * * P3` |
|        - | 3112 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - | 3113 | ` */` |
|     1168 | 3114 | `case PH7_OP_POP_EXCEPTION: {` |
|     2341 | 3115 | `	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;` |
|        - | 3116 | `	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on` |
|        - | 3117 | `	 * top (matched by compiled origin). pException == NULL means this try's` |
|        - | 3118 | `	 * activation was already consumed (an in-place catch handled a throw and` |
|        - | 3119 | `	 * ran the finally itself); compiled fields keep coming from p3. */` |
|     2341 | 3120 | `	ph7_exception *pException = 0;` |
|     2341 | 3121 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|      405 | 3122 | `		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      405 | 3123 | `		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];` |
|        - | 3124 | `		/* Same compiled origin is NOT enough: under recursion, when THIS` |
|        - | 3125 | `		 * level's activation was already consumed by an in-place catch (the` |
|        - | 3126 | `		 * resume lands right here), the top can be an OUTER level's activation` |
|        - | 3127 | `		 * of the same lexical try — popping it would run that level's finally` |
|        - | 3128 | `		 * early and orphan its handler (probed: recursive try/catch/finally` |
|        - | 3129 | `		 * lost the outer catch entirely). The activation must also belong to` |
|        - | 3130 | `		 * the CURRENT body frame. */` |
|      400 | 3131 | `		if( VmExcMatches(pTop,pCompiledExc)` |
|      401 | 3132 | `		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){` |
|      391 | 3133 | `			pException = pTop;` |
|      391 | 3134 | `			(void)SySetPop(&pVm->aException);` |
|      193 | 3135 | `		}` |
|      200 | 3136 | `	}` |
|     2341 | 3137 | `	if( pCompiledExc->iInlined ){` |
|        - | 3138 | `		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.` |
|        - | 3139 | `		 * Pop this try's handler off aException so a throw in the finally propagates to` |
|        - | 3140 | `		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the` |
|        - | 3141 | `		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the` |
|        - | 3142 | `		 * compiler-emitted JMP falls into the finally. Without a finally, this is the` |
|        - | 3143 | `		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */` |
|      159 | 3144 | `		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */` |
|      159 | 3145 | `		if( pCompiledExc->iHasFinally ){` |
|        - | 3146 | `			VmFinallyAction sAct;` |
|       12 | 3147 | `			SyZero(&sAct,sizeof(sAct));` |
|       12 | 3148 | `			sAct.eKind = PH7_FA_FALLTHROUGH;` |
|       12 | 3149 | `			sAct.iNextPc = pCompiledExc->iEndCatchPc;` |
|       12 | 3150 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        - | 3151 | `			/* keep the transparent frame for OP_END_FINALLY */` |
|      154 | 3152 | `		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       18 | 3153 | `			VmLeaveFrame(&(*pVm));` |
|        7 | 3154 | `		}` |
|      159 | 3155 | `		break;` |
|        - | 3156 | `	}` |
|        - | 3157 | `	/* Leave the exception frame. It is normally on top here (a try that fell through` |
|        - | 3158 | `	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a` |
|        - | 3159 | `	 * RESUMED generator/fiber body whose yield was inside this try, that exception` |
|        - | 3160 | `	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),` |
|        - | 3161 | `	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the` |
|        - | 3162 | `	 * coroutine (and defeat the bHasRet materialization just below, which must see the` |
|        - | 3163 | `	 * body). Only leave a genuine exception frame. */` |
|     2187 | 3164 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|     1585 | 3165 | `		VmLeaveFrame(&(*pVm));` |
|      790 | 3166 | `	}` |
|        - | 3167 | `	/* Execute the finally block if present and not already executed by the` |
|        - | 3168 | `	 * catch path. No live activation (pException == NULL) means an in-place` |
|        - | 3169 | `	 * catch consumed it — and that path runs the finally itself — so skip. */` |
|     2187 | 3170 | `	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){` |
|        - | 3171 | `		sxi32 rcFinally;` |
|       33 | 3172 | `		VmExcRelease(&(*pVm),pException);` |
|       33 | 3173 | `		pException = 0;` |
|       33 | 3174 | `		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);` |
|       33 | 3175 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 | 3176 | `			goto Abort;` |
|        - | 3177 | `		}` |
|       33 | 3178 | `		if( rcFinally == PH7_EXCEPTION ){` |
|        - | 3179 | `			/* The finally threw past itself. If an enclosing try IN THIS function` |
|        - | 3180 | `			 * caught the new exception in place, resume at its landing pad;` |
|        - | 3181 | `			 * otherwise it was caught at an outer frame, so unwind this function. */` |
|        - | 3182 | `			sxi32 iResumePc;` |
|        5 | 3183 | `			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 3184 | `				pc = iResumePc;` |
|        3 | 3185 | `				break;` |
|        - | 3186 | `			}` |
|        3 | 3187 | `			goto Exception;` |
|        - | 3188 | `		}` |
|       12 | 3189 | `	}` |
|     2183 | 3190 | `	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */` |
|     2183 | 3191 | `	if( VmSkipExceptionFrames(pVm->pFrame)->bHasRet ){` |
|        - | 3192 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - | 3193 | `		 * function. The return targets the body frame this try belongs to. Drain` |
|        - | 3194 | `		 * outer finally blocks first, then — only in the real function body` |
|        - | 3195 | `		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline` |
|        - | 3196 | `		 * try within a catch/finally) propagate outward so the owning body returns. */` |
|      167 | 3197 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|      167 | 3198 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3199 | `			goto Abort;` |
|        - | 3200 | `		}` |
|      167 | 3201 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 3202 | `			goto Exception;` |
|        - | 3203 | `		}` |
|      167 | 3204 | `		if( !sState.bReturnPropagates ){` |
|      161 | 3205 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|       78 | 3206 | `		}` |
|      167 | 3207 | `		goto Done;` |
|        - | 3208 | `	}` |
|     2021 | 3209 | `	break;` |
|        - | 3210 | `							}` |
|        - | 3211 | `/*` |
|        - | 3212 | ` * OP_CATCH iP1(catch-index) * P3(ph7_exception)` |
|        - | 3213 | ` * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on` |
|        - | 3214 | ` * pException->pInflight by VmThrowInline) into the catch variable, resolved in the` |
|        - | 3215 | ` * enclosing body's scope (PHP: a catch shares the surrounding variable scope).` |
|        - | 3216 | ` */` |
|       33 | 3217 | `case PH7_OP_CATCH: {` |
|        - | 3218 | `	VmOpRc rcOp;` |
|       71 | 3219 | `	sState.pTos = pTos;` |
|       71 | 3220 | `	sState.pc = pc;` |
|       71 | 3221 | `	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);` |
|       71 | 3222 | `	pTos = sState.pTos;` |
|       71 | 3223 | `	pc = sState.pc;` |
|       71 | 3224 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3225 | `		goto Abort;` |
|       71 | 3226 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3227 | `		goto Exception;` |
|        - | 3228 | `	}` |
|       71 | 3229 | `	break;` |
|        - | 3230 | `					  }` |
|        - | 3231 | `/*` |
|        - | 3232 | ` * OP_END_FINALLY * P3(ph7_exception)` |
|        - | 3233 | ` * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch` |
|        - | 3234 | ` * the pending action queued when the finally was entered (fall-through / re-throw /` |
|        - | 3235 | ` * return / break-continue). A return/break threads out through each enclosing finally` |
|        - | 3236 | ` * via pException->iNextFinallyPc.` |
|        - | 3237 | ` */` |
|       22 | 3238 | `case PH7_OP_END_FINALLY: {` |
|       48 | 3239 | `	ph7_exception *pExc = (ph7_exception *)pInstr->p3;` |
|        - | 3240 | `	VmFinallyAction sAct;` |
|       48 | 3241 | `	int eKind = PH7_FA_FALLTHROUGH;` |
|        - | 3242 | `	/* Leave the try's transparent frame kept alive across the finally. */` |
|       48 | 3243 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        3 | 3244 | `		VmLeaveFrame(&(*pVm));` |
|        1 | 3245 | `	}` |
|       48 | 3246 | `	if( SySetUsed(&pVm->aFinallyAction) > 0 ){` |
|       48 | 3247 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);` |
|       48 | 3248 | `		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];` |
|       48 | 3249 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|       48 | 3250 | `		eKind = sAct.eKind;` |
|       26 | 3251 | `	}else{` |
|      ! 0 | 3252 | `		SyZero(&sAct,sizeof(sAct));` |
|      ! 0 | 3253 | `		sAct.iNextPc = pExc->iEndCatchPc;` |
|        - | 3254 | `	}` |
|       48 | 3255 | `	if( eKind == PH7_FA_FALLTHROUGH ){` |
|       10 | 3256 | `		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;` |
|       13 | 3257 | `		break;` |
|       40 | 3258 | `	}else if( eKind == PH7_FA_JMP ){` |
|        - | 3259 | `		/* break/continue: run the remaining crossed finallys, then take the jump. */` |
|        3 | 3260 | `		sxu32 iFpc = 0;` |
|        3 | 3261 | `		int nCross = sAct.nCross;` |
|        3 | 3262 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|      ! 0 | 3263 | `			sAct.nCross = nCross;` |
|      ! 0 | 3264 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      ! 0 | 3265 | `			pc = (sxi32)iFpc - 1;` |
|      ! 0 | 3266 | `			break;` |
|        - | 3267 | `		}` |
|        3 | 3268 | `		pc = (sxi32)sAct.iNextPc - 1;` |
|        3 | 3269 | `		break;` |
|       38 | 3270 | `	}else if( eKind == PH7_FA_RETHROW ){` |
|        8 | 3271 | `		ph7_class_instance *pRe = sAct.pExc;` |
|        - | 3272 | `		sxi32 _iRpE;` |
|        8 | 3273 | `		rc = VmThrowException(&(*pVm),pRe);` |
|        8 | 3274 | `		if( pRe ){ PH7_ClassInstanceUnref(pRe); }` |
|        8 | 3275 | `		if( rc == SXERR_ABORT ){ goto Abort; }` |
|        8 | 3276 | `		PH7_INLINE_RESUME_BREAK()` |
|        8 | 3277 | `		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }` |
|        8 | 3278 | `		goto Exception;` |
|      ! 0 | 3279 | `	}else{ /* PH7_FA_RETURN */` |
|       31 | 3280 | `		sxu32 iFpc = 0;` |
|       31 | 3281 | `		int nCross = sAct.nCross;` |
|       31 | 3282 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        - | 3283 | `			/* Thread the return through the next enclosing finally. */` |
|        6 | 3284 | `			sAct.nCross = nCross;` |
|        6 | 3285 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        6 | 3286 | `			pc = (sxi32)iFpc - 1;` |
|        6 | 3287 | `			break;` |
|        - | 3288 | `		}` |
|        - | 3289 | `		/* No enclosing finally left: materialize the return from this body. */` |
|       27 | 3290 | `		if( sAct.bHasRetVal && sState.pResult ){` |
|        6 | 3291 | `			PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|        2 | 3292 | `		}` |
|       27 | 3293 | `		PH7_MemObjRelease(&sAct.sRet);` |
|       27 | 3294 | `		goto Done;` |
|        - | 3295 | `	}` |
|        - | 3296 | `						 }` |
|        - | 3297 | `/*` |
|        - | 3298 | ` * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)` |
|        - | 3299 | `` * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a`` |
|        - | 3300 | ` * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);` |
|        - | 3301 | ` * OP_END_FINALLY threads it out through the finally chain and then returns.` |
|        - | 3302 | ` */` |
|       15 | 3303 | `case PH7_OP_SET_FINALLY_RET: {` |
|        - | 3304 | `	VmFinallyAction sAct;` |
|       35 | 3305 | `	sxu32 iFpc = 0;` |
|       35 | 3306 | `	int nCross = -1; /* a return crosses every enclosing finally in this function */` |
|       35 | 3307 | `	SyZero(&sAct,sizeof(sAct));` |
|       35 | 3308 | `	sAct.eKind = PH7_FA_RETURN;` |
|       35 | 3309 | `	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       35 | 3310 | `	PH7_MemObjInit(pVm,&sAct.sRet);` |
|       35 | 3311 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|       31 | 3312 | `		PH7_MemObjStore(pTos,&sAct.sRet);` |
|       31 | 3313 | `		sAct.bHasRetVal = 1;` |
|       31 | 3314 | `		VmPopOperand(&pTos,1);` |
|       13 | 3315 | `	}` |
|       35 | 3316 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        9 | 3317 | `		sAct.nCross = nCross;` |
|        9 | 3318 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        9 | 3319 | `		pc = (sxi32)iFpc - 1;` |
|        9 | 3320 | `		break;` |
|        - | 3321 | `	}` |
|        - | 3322 | `	/* No enclosing finally left: return now. */` |
|       29 | 3323 | `	if( sAct.bHasRetVal && sState.pResult ){` |
|       27 | 3324 | `		PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|       11 | 3325 | `	}` |
|       29 | 3326 | `	PH7_MemObjRelease(&sAct.sRet);` |
|       29 | 3327 | `	goto Done;` |
|        - | 3328 | `						 }` |
|        - | 3329 | `/*` |
|        - | 3330 | ` * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)` |
|        - | 3331 | `` * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action`` |
|        - | 3332 | ` * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.` |
|        - | 3333 | ` */` |
|        1 | 3334 | `case PH7_OP_SET_FINALLY_JMP: {` |
|        - | 3335 | `	VmFinallyAction sAct;` |
|        3 | 3336 | `	sxu32 iFpc = 0;` |
|        3 | 3337 | `	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */` |
|        3 | 3338 | `	SyZero(&sAct,sizeof(sAct));` |
|        3 | 3339 | `	sAct.eKind = PH7_FA_JMP;` |
|        3 | 3340 | `	sAct.iNextPc = pInstr->iP2;` |
|        3 | 3341 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        3 | 3342 | `		sAct.nCross = nCross;` |
|        3 | 3343 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        3 | 3344 | `		pc = (sxi32)iFpc - 1;` |
|        3 | 3345 | `		break;` |
|        - | 3346 | `	}` |
|        - | 3347 | `	/* No finally among the crossed trys: just take the break/continue jump. */` |
|      ! 0 | 3348 | `	pc = (sxi32)sAct.iNextPc - 1;` |
|      ! 0 | 3349 | `	break;` |
|        - | 3350 | `						 }` |
|        - | 3351 | `/*` |
|        - | 3352 | ` * OP_THROW * P2 *` |
|        - | 3353 | ` * Throw an user exception.` |
|        - | 3354 | ` */` |
|      366 | 3355 | `case PH7_OP_THROW: {` |
|        - | 3356 | `	VmOpRc rcOp;` |
|      737 | 3357 | `	sState.pTos = pTos;` |
|      737 | 3358 | `	sState.pc = pc;` |
|      737 | 3359 | `	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);` |
|      737 | 3360 | `	pTos = sState.pTos;` |
|      737 | 3361 | `	pc = sState.pc;` |
|      737 | 3362 | `	if( rcOp == VM_OP_ABORT ){` |
|       28 | 3363 | `		goto Abort;` |
|      713 | 3364 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      415 | 3365 | `		goto Exception;` |
|        - | 3366 | `	}` |
|      303 | 3367 | `	break;` |
|        - | 3368 | `					  }` |
|        - | 3369 | `/*` |
|        - | 3370 | ` * OP_FOREACH_INIT * P2 P3` |
|        - | 3371 | ` * Prepare a foreach step.` |
|        - | 3372 | ` */` |
|    11702 | 3373 | `case PH7_OP_FOREACH_INIT: {` |
|        - | 3374 | `	VmOpRc rcOp;` |
|    23409 | 3375 | `	sState.pTos = pTos;` |
|    23409 | 3376 | `	sState.pc = pc;` |
|    23409 | 3377 | `	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);` |
|    23409 | 3378 | `	pTos = sState.pTos;` |
|    23409 | 3379 | `	pc = sState.pc;` |
|    23409 | 3380 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3381 | `		goto Abort;` |
|    23409 | 3382 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3383 | `		goto Exception;` |
|        - | 3384 | `	}` |
|    23409 | 3385 | `	break;` |
|        - | 3386 | `					  }` |
|        - | 3387 | `/*` |
|        - | 3388 | ` * OP_FOREACH_STEP * P2 P3` |
|        - | 3389 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - | 3390 | ` */` |
|   129602 | 3391 | `case PH7_OP_FOREACH_STEP: {` |
|        - | 3392 | `	VmOpRc rcOp;` |
|   259209 | 3393 | `	sState.pTos = pTos;` |
|   259209 | 3394 | `	sState.pc = pc;` |
|   259209 | 3395 | `	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);` |
|   259209 | 3396 | `	pTos = sState.pTos;` |
|   259209 | 3397 | `	pc = sState.pc;` |
|   259209 | 3398 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3399 | `		goto Abort;` |
|   259207 | 3400 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3401 | `		goto Exception;` |
|        - | 3402 | `	}` |
|   259207 | 3403 | `	break;` |
|        - | 3404 | `						  }` |
|        - | 3405 | `/*` |
|        - | 3406 | ` * OP_MEMBER P1 P2` |
|        - | 3407 | ` * Load class attribute/method on the stack.` |
|        - | 3408 | ` */` |
|    21731 | 3409 | `case PH7_OP_MEMBER: {` |
|        - | 3410 | `	VmOpRc rcOp;` |
|    43467 | 3411 | `	sState.pTos = pTos;` |
|    43467 | 3412 | `	sState.pc = pc;` |
|    43467 | 3413 | `	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);` |
|    43467 | 3414 | `	pTos = sState.pTos;` |
|    43467 | 3415 | `	pc = sState.pc;` |
|    43467 | 3416 | `	if( rcOp == VM_OP_ABORT ){` |
|        8 | 3417 | `		goto Abort;` |
|    43461 | 3418 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       11 | 3419 | `		goto Exception;` |
|        - | 3420 | `	}` |
|    43451 | 3421 | `	break;` |
|        - | 3422 | `					  }` |
|        - | 3423 | `/*` |
|        - | 3424 | ` * OP_NEW P1 * * *` |
|        - | 3425 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - | 3426 | ` */` |
|     2370 | 3427 | `case PH7_OP_NEW: {` |
|        - | 3428 | `	VmOpRc rcOp;` |
|     4745 | 3429 | `	sState.pTos = pTos;` |
|     4745 | 3430 | `	sState.pc = pc;` |
|     4745 | 3431 | `	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);` |
|     4745 | 3432 | `	pTos = sState.pTos;` |
|     4745 | 3433 | `	pc = sState.pc;` |
|     4745 | 3434 | `	if( rcOp == VM_OP_ABORT ){` |
|        5 | 3435 | `		goto Abort;` |
|     4741 | 3436 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 3437 | `		goto Exception;` |
|        - | 3438 | `	}` |
|     4733 | 3439 | `	break;` |
|        - | 3440 | `					  }` |
|        - | 3441 | `/*` |
|        - | 3442 | ` * OP_CLONE * * *` |
|        - | 3443 | ` * Perfome a clone operation.` |
|        - | 3444 | ` */` |
|      107 | 3445 | `case PH7_OP_CLONE: {` |
|        - | 3446 | `	VmOpRc rcOp;` |
|      219 | 3447 | `	sState.pTos = pTos;` |
|      219 | 3448 | `	sState.pc = pc;` |
|      219 | 3449 | `	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);` |
|      219 | 3450 | `	pTos = sState.pTos;` |
|      219 | 3451 | `	pc = sState.pc;` |
|      219 | 3452 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3453 | `		goto Abort;` |
|      219 | 3454 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 3455 | `		goto Exception;` |
|        - | 3456 | `	}` |
|      217 | 3457 | `	break;` |
|        - | 3458 | `					  }` |
|        - | 3459 | `/*` |
|        - | 3460 | ` * OP_CLONE_APPLY * * *` |
|        - | 3461 | ` *  Apply the PHP 8.5 clone($obj, $withProperties) property updates. The updates` |
|        - | 3462 | ` *  array is on the stack top and the freshly-cloned object (from OP_CLONE) is` |
|        - | 3463 | ` *  directly below it. Each entry is applied as a scope-aware property write` |
|        - | 3464 | ` *  (AFTER __clone() has already run); the array is then popped, leaving the` |
|        - | 3465 | ` *  clone as the result.` |
|        - | 3466 | ` */` |
|        8 | 3467 | `case PH7_OP_CLONE_APPLY: {` |
|        - | 3468 | `	VmOpRc rcOp;` |
|       17 | 3469 | `	sState.pTos = pTos;` |
|       17 | 3470 | `	sState.pc = pc;` |
|       17 | 3471 | `	rcOp = VmExecOpCloneApply(&(*pVm),&sState,pInstr);` |
|       17 | 3472 | `	pTos = sState.pTos;` |
|       17 | 3473 | `	pc = sState.pc;` |
|       17 | 3474 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3475 | `		goto Abort;` |
|       17 | 3476 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3477 | `		goto Exception;` |
|        - | 3478 | `	}` |
|       17 | 3479 | `	break;` |
|        - | 3480 | `					  }` |
|        - | 3481 | `/*` |
|        - | 3482 | ` * OP_SWITCH * * P3` |
|        - | 3483 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - | 3484 | ` */` |
|      162 | 3485 | `case PH7_OP_SWITCH: {` |
|        - | 3486 | `	VmOpRc rcOp;` |
|      329 | 3487 | `	sState.pTos = pTos;` |
|      329 | 3488 | `	sState.pc = pc;` |
|      329 | 3489 | `	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);` |
|      329 | 3490 | `	pTos = sState.pTos;` |
|      329 | 3491 | `	pc = sState.pc;` |
|      329 | 3492 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3493 | `		goto Abort;` |
|      329 | 3494 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3495 | `		goto Exception;` |
|        - | 3496 | `	}` |
|      329 | 3497 | `	break;` |
|        - | 3498 | `					  }` |
|        - | 3499 | `/*` |
|        - | 3500 | ` * OP_MATCH * * P3` |
|        - | 3501 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - | 3502 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - | 3503 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - | 3504 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - | 3505 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - | 3506 | ` */` |
|       56 | 3507 | `case PH7_OP_MATCH: {` |
|        - | 3508 | `	VmOpRc rcOp;` |
|      115 | 3509 | `	sState.pTos = pTos;` |
|      115 | 3510 | `	sState.pc = pc;` |
|      115 | 3511 | `	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);` |
|      115 | 3512 | `	pTos = sState.pTos;` |
|      115 | 3513 | `	pc = sState.pc;` |
|      115 | 3514 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3515 | `		goto Abort;` |
|      115 | 3516 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        6 | 3517 | `		goto Exception;` |
|        - | 3518 | `	}` |
|      109 | 3519 | `	break;` |
|        - | 3520 | `					  }` |
|        - | 3521 | `/*` |
|        - | 3522 | ` * OP_YIELD P1 P2 *` |
|        - | 3523 | ` *  Yield a value from a generator function.` |
|        - | 3524 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - | 3525 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - | 3526 | ` */` |
|      582 | 3527 | `case PH7_OP_YIELD: {` |
|        - | 3528 | `	ph7_generator *pGen;` |
|     1169 | 3529 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 3530 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 | 3531 | `		goto Abort;` |
|        - | 3532 | `	}` |
|     1169 | 3533 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 3534 | ``		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's`` |
|        - | 3535 | `		 * pending finallys tried to yield — PHP forbids it. */` |
|      ! 0 | 3536 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 3537 | `			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 3538 | `			goto Abort;` |
|        - | 3539 | `		}` |
|      ! 0 | 3540 | `		goto Exception;` |
|        - | 3541 | `	}` |
|     1169 | 3542 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|     1169 | 3543 | `	if( pInstr->iP2 ){` |
|        - | 3544 | `		/* yield $key => $value: value on top, key below */` |
|        - | 3545 | `#ifdef UNTRUST` |
|        - | 3546 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - | 3547 | `#endif` |
|       70 | 3548 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       70 | 3549 | `		VmPopOperand(&pTos, 1);` |
|       70 | 3550 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       70 | 3551 | `		VmPopOperand(&pTos, 1);` |
|        - | 3552 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       70 | 3553 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|       47 | 3554 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|       47 | 3555 | `			if( nKey >= pGen->iImplicitKey ){` |
|       47 | 3556 | `				pGen->iImplicitKey = nKey + 1;` |
|       23 | 3557 | `			}` |
|       25 | 3558 | `		}` |
|     1135 | 3559 | `	}else if( pInstr->iP1 ){` |
|        - | 3560 | `		/* yield $value */` |
|        - | 3561 | `#ifdef UNTRUST` |
|        - | 3562 | `		if( pTos < pStack ) goto Abort;` |
|        - | 3563 | `#endif` |
|     1101 | 3564 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|     1101 | 3565 | `		VmPopOperand(&pTos, 1);` |
|        - | 3566 | `		/* Auto-increment key */` |
|     1101 | 3567 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|     1101 | 3568 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|     1101 | 3569 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|      553 | 3570 | `	}else{` |
|        - | 3571 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 | 3572 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 3573 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 3574 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 | 3575 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - | 3576 | `	}` |
|        - | 3577 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|     1169 | 3578 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|     1169 | 3579 | `	goto Suspend;` |
|        - | 3580 | `}` |
|        - | 3581 | `/*` |
|        - | 3582 | ` * OP_YIELD_FROM * * *` |
|        - | 3583 | ` *` |
|        - | 3584 | `` * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an`` |
|        - | 3585 | ` * array/Traversable/Generator from the OUTER generator, preserving the inner` |
|        - | 3586 | ` * keys; the expression evaluates to the inner Generator's return value (or NULL).` |
|        - | 3587 | ` *` |
|        - | 3588 | ` * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,` |
|        - | 3589 | ` * advances the per-instance delegate cursor stored on the exec context (never the` |
|        - | 3590 | ` * shared foreach aStep, so independent generator instances cannot clash). The` |
|        - | 3591 | ` * iterable operand is consumed on first entry; the expression result is pushed at` |
|        - | 3592 | ` * exhaustion — net stack effect +1, identical to OP_YIELD.` |
|        - | 3593 | ` */` |
|       90 | 3594 | `case PH7_OP_YIELD_FROM: {` |
|        - | 3595 | `	ph7_generator *pGenFrom;` |
|        - | 3596 | `	ph7_exec_ctx *pCtxFrom;` |
|        - | 3597 | `	ph7_value sKey,sVal;` |
|      185 | 3598 | `	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */` |
|      185 | 3599 | `	int bExhausted = 0;` |
|      185 | 3600 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 3601 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");` |
|      ! 0 | 3602 | `		goto Abort;` |
|        - | 3603 | `	}` |
|      185 | 3604 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 3605 | ``		/* A `yield from` reached while VmCloseCtx force-drives this destroyed`` |
|        - | 3606 | `		 * generator's pending finallys — PHP forbids it (distinct message from a` |
|        - | 3607 | ``		 * bare `yield`, mirroring the OP_YIELD guard above). */`` |
|      ! 0 | 3608 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 3609 | `			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 3610 | `			goto Abort;` |
|        - | 3611 | `		}` |
|      ! 0 | 3612 | `		goto Exception;` |
|        - | 3613 | `	}` |
|      185 | 3614 | `	pCtxFrom = pVm->pActiveCtx;` |
|      185 | 3615 | `	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;` |
|      185 | 3616 | `	PH7_MemObjInit(pVm,&sKey);` |
|      185 | 3617 | `	PH7_MemObjInit(pVm,&sVal);` |
|      185 | 3618 | `	if( pCtxFrom->iDelegateState == 0 ){` |
|        - | 3619 | `		/* First entry: classify the iterable on the stack top. */` |
|       77 | 3620 | `		int bIterable = 1;` |
|        - | 3621 | `#ifdef UNTRUST` |
|        - | 3622 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 3623 | `#endif` |
|       77 | 3624 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       26 | 3625 | `			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       26 | 3626 | `			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;` |
|       26 | 3627 | `			pCtxFrom->iDelegateState = 1;` |
|       66 | 3628 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       51 | 3629 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       51 | 3630 | `			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       51 | 3631 | `			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){` |
|       41 | 3632 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       41 | 3633 | `				pCtxFrom->iDelegateState = 3;` |
|       31 | 3634 | `			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){` |
|        9 | 3635 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        9 | 3636 | `				pCtxFrom->iDelegateState = 2;` |
|        6 | 3637 | `			}else{` |
|        5 | 3638 | `				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - | 3639 | `					sizeof("IteratorAggregate")-1,FALSE,0);` |
|        7 | 3640 | `				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){` |
|        - | 3641 | `					/* Delegate to the Iterator returned by getIterator() */` |
|        - | 3642 | `					ph7_value sIt;` |
|        5 | 3643 | `					PH7_MemObjInit(pVm,&sIt);` |
|        5 | 3644 | `					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);` |
|        5 | 3645 | `					if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){` |
|        - | 3646 | `						/* getIterator() threw/aborted: drop it, consume the` |
|        - | 3647 | `						 * operand, and propagate. */` |
|      ! 0 | 3648 | `						PH7_MemObjRelease(&sIt);` |
|      ! 0 | 3649 | `						VmPopOperand(&pTos,1);` |
|      ! 0 | 3650 | `						goto yf_propagate;` |
|        - | 3651 | `					}` |
|        4 | 3652 | `					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls` |
|        5 | 3653 | `						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){` |
|        5 | 3654 | `						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);` |
|        5 | 3655 | `						pCtxFrom->iDelegateState = 2;` |
|        3 | 3656 | `					}else{` |
|      ! 0 | 3657 | `						bIterable = 0;` |
|        - | 3658 | `					}` |
|        5 | 3659 | `					PH7_MemObjRelease(&sIt);` |
|        3 | 3660 | `				}else{` |
|      ! 0 | 3661 | `					bIterable = 0;` |
|        - | 3662 | `				}` |
|        - | 3663 | `			}` |
|       28 | 3664 | `		}else{` |
|        6 | 3665 | `			bIterable = 0;` |
|        - | 3666 | `		}` |
|       77 | 3667 | `		VmPopOperand(&pTos,1); /* Consume the iterable operand */` |
|       77 | 3668 | `		if( !bIterable ){` |
|        - | 3669 | `			/* Non-iterable source: throw a catchable Error (PHP 8.5), then` |
|        - | 3670 | `			 * funnel through the shared teardown/route path. */` |
|        6 | 3671 | `			rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 3672 | `				"Can use \"yield from\" only with arrays and Traversables",` |
|        - | 3673 | `				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);` |
|        6 | 3674 | `			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        6 | 3675 | `			goto yf_propagate;` |
|        - | 3676 | `		}` |
|       73 | 3677 | `		if( pCtxFrom->iDelegateState >= 2 ){` |
|        - | 3678 | `			/* rewind() the delegate (also starts a fresh generator) */` |
|       51 | 3679 | `			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 3680 | `				"rewind",sizeof("rewind")-1,0);` |
|       51 | 3681 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       23 | 3682 | `		}` |
|       39 | 3683 | `	}else{` |
|        - | 3684 | `		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a` |
|        - | 3685 | `		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()` |
|        - | 3686 | `		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)` |
|        - | 3687 | `		 * ignore send() and just advance with next(). */` |
|        - | 3688 | `#ifdef UNTRUST` |
|        - | 3689 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 3690 | `#endif` |
|      112 | 3691 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       67 | 3692 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|        - | 3693 | `			/* A pending Generator::throw() on the outer was parked on pInjected by the` |
|        - | 3694 | `			 * body-entry gate (which skips its own raise for state 3); forward it as a` |
|        - | 3695 | `			 * throw into the delegate, else forward the sent value (pTos, which` |
|        - | 3696 | `			 * VmResumeCtx copies into the inner's own stack). */` |
|       67 | 3697 | `			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;` |
|       67 | 3698 | `			pCtxFrom->pInjected = 0;` |
|       67 | 3699 | `			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       67 | 3700 | `				if( pInjFwd ){` |
|        - | 3701 | `					/* Borrowed ref: the outer's Generator::throw() holds it across this` |
|        - | 3702 | `					 * whole resume, so the inner inject path must not unref it. */` |
|        5 | 3703 | `					pInner->pCtx->pInjected = pInjFwd;` |
|        5 | 3704 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);` |
|        5 | 3705 | `					pInner->pCtx->pInjected = 0;` |
|        3 | 3706 | `				}else{` |
|       63 | 3707 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);` |
|        3 | 3708 | `				}` |
|       32 | 3709 | `			}else if( pInjFwd ){` |
|        - | 3710 | `				/* No live delegate to receive the throw (inner already finished/closed):` |
|        - | 3711 | `				 * raise it at the yield-from in the outer generator's own frame. */` |
|      ! 0 | 3712 | `				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);` |
|      ! 0 | 3713 | `				pTF->iFlags \|= VM_FRAME_THROW;` |
|      ! 0 | 3714 | `				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 3715 | `			}` |
|       67 | 3716 | `			PH7_MemObjRelease(pTos);` |
|       67 | 3717 | `			pTos--;` |
|       67 | 3718 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       32 | 3719 | `		}else{` |
|       48 | 3720 | `			PH7_MemObjRelease(pTos);` |
|       48 | 3721 | `			pTos--;` |
|       48 | 3722 | `			if( pCtxFrom->iDelegateState >= 2 ){` |
|       17 | 3723 | `				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 3724 | `					"next",sizeof("next")-1,0);` |
|       17 | 3725 | `				if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 3726 | `			}` |
|        - | 3727 | `		}` |
|        - | 3728 | `	}` |
|        - | 3729 | `	/* Fetch the current (key,value) of the delegate, or mark exhausted. */` |
|      171 | 3730 | `	if( pCtxFrom->iDelegateState == 1 ){` |
|       56 | 3731 | `		if( pCtxFrom->pDelegateNode == 0 ){` |
|       18 | 3732 | `			bExhausted = 1;` |
|       11 | 3733 | `		}else{` |
|       42 | 3734 | `			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);` |
|       42 | 3735 | `			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);` |
|        - | 3736 | `			/* Forward traversal follows pPrev (the hashmap's "reverse link",` |
|        - | 3737 | `			 * matching PH7_HashmapGetNextEntry). */` |
|       42 | 3738 | `			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;` |
|        - | 3739 | `		}` |
|       30 | 3740 | `	}else{` |
|      119 | 3741 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|        - | 3742 | `		ph7_value sValid;` |
|        - | 3743 | `		int isValid;` |
|      119 | 3744 | `		PH7_MemObjInit(pVm,&sValid);` |
|      119 | 3745 | `		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|      119 | 3746 | `		PH7_MemObjToBool(&sValid);` |
|      119 | 3747 | `		isValid = (sValid.x.iVal != 0);` |
|      119 | 3748 | `		PH7_MemObjRelease(&sValid);` |
|      119 | 3749 | `		if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      119 | 3750 | `		if( !isValid ){` |
|       28 | 3751 | `			bExhausted = 1;` |
|       16 | 3752 | `		}else{` |
|       95 | 3753 | `			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|       95 | 3754 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       95 | 3755 | `			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       95 | 3756 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        - | 3757 | `		}` |
|        - | 3758 | `	}` |
|      171 | 3759 | `	if( bExhausted ){` |
|        - | 3760 | `		/* Expression value: inner Generator's return value (state 3) or NULL. */` |
|        - | 3761 | `		ph7_value sResult;` |
|       42 | 3762 | `		PH7_MemObjInit(pVm,&sResult);` |
|       42 | 3763 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       23 | 3764 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|       23 | 3765 | `			if( pInner && pInner->pCtx ){` |
|       23 | 3766 | `				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);` |
|       10 | 3767 | `			}` |
|       10 | 3768 | `		}` |
|       42 | 3769 | `		PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       42 | 3770 | `		pCtxFrom->pDelegateNode = 0;` |
|       42 | 3771 | `		pCtxFrom->iDelegateState = 0;` |
|       42 | 3772 | `		pTos++;` |
|       42 | 3773 | `		PH7_MemObjStore(&sResult,pTos);` |
|       42 | 3774 | `		PH7_MemObjRelease(&sResult);` |
|       42 | 3775 | `		PH7_MemObjRelease(&sKey);` |
|       42 | 3776 | `		PH7_MemObjRelease(&sVal);` |
|       42 | 3777 | `		break; /* fall through to pc+1 with the result on the stack top */` |
|        - | 3778 | `	}` |
|        - | 3779 | `	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.` |
|        - | 3780 | `	 * The outer generator's implicit auto-key counter is NOT advanced by the` |
|        - | 3781 | ``	 * delegated keys — PHP keeps it independent across `yield from`, so a later`` |
|        - | 3782 | ``	 * plain `yield` continues from the outer's own counter. */`` |
|      133 | 3783 | `	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);` |
|      133 | 3784 | `	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);` |
|      133 | 3785 | `	PH7_MemObjRelease(&sKey);` |
|      133 | 3786 | `	PH7_MemObjRelease(&sVal);` |
|        - | 3787 | `	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */` |
|      133 | 3788 | `	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));` |
|      133 | 3789 | `	goto Suspend;` |
|        7 | 3790 | `yf_propagate:` |
|        - | 3791 | `	/* A delegate iterator method threw/aborted (or the source was non-iterable):` |
|        - | 3792 | `	 * tear down the delegation, then route via the shared dispatch macro. rcm is` |
|        - | 3793 | `	 * always PH7_EXCEPTION or PH7_ABORT here. */` |
|       17 | 3794 | `	PH7_MemObjRelease(&sKey);` |
|       17 | 3795 | `	PH7_MemObjRelease(&sVal);` |
|       17 | 3796 | `	PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       17 | 3797 | `	pCtxFrom->pDelegateNode = 0;` |
|       17 | 3798 | `	pCtxFrom->iDelegateState = 0;` |
|       17 | 3799 | `	PH7_DISPATCH_ENFORCE_RC(rcm)` |
|      ! 0 | 3800 | `	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */` |
|        - | 3801 | `}` |
|        - | 3802 | `/*` |
|        - | 3803 | ` * OP_CALL P1 * *` |
|        - | 3804 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - | 3805 | ` *  function on the stack.` |
|        - | 3806 | ` */` |
|   514698 | 3807 | `case PH7_OP_CALL: {` |
|        - | 3808 | `	/* iP2 = hasSpread (compile-time). Count only THIS call's own unpack` |
|        - | 3809 | `	 * expansion (VmSpreadOwnExtra, derived from the captured runs on top of the` |
|        - | 3810 | `	 * stack) — an INNER spread-bearing call evaluated inside this argument list` |
|        - | 3811 | ``	 * (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not`` |
|        - | 3812 | `	 * be conflated, which a single shared accumulator could not express. */` |
|  1030145 | 3813 | `	sxi32 nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|        - | 3814 | `	ph7_value *pArg;` |
|  1030145 | 3815 | `	pArg = &pTos[-nCallArgs];` |
|        - | 3816 | `	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED` |
|        - | 3817 | `	 * arguments, and a spread expanding to !=1 element shifts the actual positions` |
|        - | 3818 | `	 * of any following compile-time named args. The effective per-actual-slot name` |
|        - | 3819 | `	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)` |
|        - | 3820 | `	 * is built PER PATH against that path's finalized arg base — a method call pops` |
|        - | 3821 | `	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch` |
|        - | 3822 | `	 * site rather than built once here. */` |
|        - | 3823 | `	VmCallArgMap sEffMap;` |
|  1030145 | 3824 | `	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;` |
|        - | 3825 | `	SyHashEntry *pEntry;` |
|        - | 3826 | `	SyString sName;` |
|        - | 3827 | `	/* A Closure object is callable: unwrap it to its underlying string callable so the` |
|        - | 3828 | `	 * dispatch below handles it (rather than treating it as a generic object and looking` |
|        - | 3829 | `	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure` |
|        - | 3830 | `	 * so a plain __invoke object skips the temp-value work entirely. */` |
|  1030145 | 3831 | `	if( VmValueIsClosure(pVm,pTos) ){` |
|        - | 3832 | `		ph7_value sCallable;` |
|     1129 | 3833 | `		PH7_MemObjInit(pVm,&sCallable);` |
|     1129 | 3834 | `		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){` |
|     1129 | 3835 | `			PH7_MemObjRelease(pTos);` |
|     1129 | 3836 | `			PH7_MemObjStore(&sCallable,pTos);` |
|      562 | 3837 | `		}` |
|     1129 | 3838 | `		PH7_MemObjRelease(&sCallable);` |
|      562 | 3839 | `	}` |
|        - | 3840 | `	/* Extract function name */` |
|  1030145 | 3841 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      162 | 3842 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 3843 | `			ph7_value sResult;` |
|        - | 3844 | `			sxi32 rcArr;` |
|        - | 3845 | `			{` |
|        - | 3846 | `				/* php validates the SHAPE of an array callable first: it must hold exactly` |
|        - | 3847 | `				 * two elements. PH7 handed any array to the dispatcher, which failed` |
|        - | 3848 | ``				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */`` |
|       75 | 3849 | `				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - | 3850 | `				char zCbMsg[192];` |
|       75 | 3851 | `				const char *zCbErr = 0;` |
|       75 | 3852 | `				if( pCbMap && pCbMap->nEntry == 2 ){` |
|        - | 3853 | `					/* Shape is right; now check it actually RESOLVES. The shared dispatcher` |
|        - | 3854 | `					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for` |
|        - | 3855 | `					 * an unresolvable [class,method] pair -- silence a caller cannot detect --` |
|        - | 3856 | `					 * and its contract is relied on by call_user_func/usort, so the throw` |
|        - | 3857 | `					 * belongs here at the call site. */` |
|       73 | 3858 | `					ph7_value *pCbCls = (ph7_value *)SySetAt(&pVm->aMemObj,pCbMap->pFirst->nValIdx);` |
|       73 | 3859 | `					ph7_value *pCbMeth = (ph7_value *)SySetAt(&pVm->aMemObj,pCbMap->pFirst->pPrev->nValIdx);` |
|       73 | 3860 | `					ph7_class *pCbClass = pCbCls ? PH7_VmExtractClassFromValue(&(*pVm),pCbCls) : 0;` |
|       73 | 3861 | `					if( pCbClass == 0 ){` |
|      ! 0 | 3862 | `						SyBufferFormat(zCbMsg,sizeof(zCbMsg),"Class \"%.*s\" not found",` |
|      ! 0 | 3863 | `							pCbCls ? (int)SyBlobLength(&pCbCls->sBlob) : 0,` |
|      ! 0 | 3864 | `							pCbCls ? (const char *)SyBlobData(&pCbCls->sBlob) : "");` |
|      ! 0 | 3865 | `						zCbErr = zCbMsg;` |
|       72 | 3866 | `					}else if( pCbMeth == 0 \|\| (pCbMeth->iFlags & MEMOBJ_STRING) == 0` |
|       73 | 3867 | `						\|\| PH7_ClassExtractMethod(pCbClass,(const char *)SyBlobData(&pCbMeth->sBlob),` |
|       72 | 3868 | `							SyBlobLength(&pCbMeth->sBlob)) == 0 ){` |
|        5 | 3869 | `						SyBufferFormat(zCbMsg,sizeof(zCbMsg),"Call to undefined method %z::%.*s()",` |
|        1 | 3870 | `							&pCbClass->sName,` |
|        2 | 3871 | `							pCbMeth ? (int)SyBlobLength(&pCbMeth->sBlob) : 0,` |
|        1 | 3872 | `							pCbMeth ? (const char *)SyBlobData(&pCbMeth->sBlob) : "");` |
|        3 | 3873 | `						zCbErr = zCbMsg;` |
|        1 | 3874 | `					}` |
|       36 | 3875 | `				}` |
|       75 | 3876 | `				if( pCbMap == 0 \|\| pCbMap->nEntry != 2 \|\| zCbErr ){` |
|        - | 3877 | `					sxi32 rcCb;` |
|        5 | 3878 | `					if( pInstr->iP2 ){` |
|      ! 0 | 3879 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 3880 | `					}` |
|        5 | 3881 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 3882 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 3883 | `					}` |
|        5 | 3884 | `					PH7_MemObjRelease(pTos);` |
|        5 | 3885 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        5 | 3886 | `					pTos->nIdx = SXU32_HIGH;` |
|        5 | 3887 | `					if( zCbErr == 0 ){` |
|        3 | 3888 | `						zCbErr = "Array callback must have exactly two elements";` |
|        1 | 3889 | `					}` |
|        5 | 3890 | `					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));` |
|        5 | 3891 | `					if( rcCb == SXERR_ABORT ){ goto Abort; }` |
|        5 | 3892 | `					rc = rcCb;` |
|        5 | 3893 | `					PH7_DISPATCH_ENFORCE_RC(rc)` |
|        2 | 3894 | `				}` |
|        - | 3895 | `			}` |
|        - | 3896 | `			/* Build the effective spread-key map (and consume this call's runs)` |
|        - | 3897 | `			 * against this path's arg base (the array-callable slot isn't popped). */` |
|      112 | 3898 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|       74 | 3899 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|       75 | 3900 | `			SySetReset(&aArg);` |
|      143 | 3901 | `			while( pArg < pTos ){` |
|       69 | 3902 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       69 | 3903 | `				pArg++;` |
|        1 | 3904 | `			}` |
|       75 | 3905 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 3906 | `			/* May be a class instance and it's static method. Forward this call's named-arg map` |
|        - | 3907 | ``			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —`` |
|        - | 3908 | `			 * mirroring the __invoke-object branch below. */` |
|       75 | 3909 | `			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|       75 | 3910 | `			SySetReset(&aArg);` |
|        - | 3911 | `			/* Pop given arguments */` |
|       75 | 3912 | `			if( nCallArgs > 0 ){` |
|       53 | 3913 | `				VmPopOperand(&pTos,nCallArgs);` |
|       26 | 3914 | `			}` |
|       75 | 3915 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 | 3916 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 3917 | `				goto Abort;` |
|        - | 3918 | `			}` |
|       75 | 3919 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - | 3920 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - | 3921 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - | 3922 | `				sxi32 iResumePc;` |
|        3 | 3923 | `				PH7_MemObjRelease(&sResult);` |
|        3 | 3924 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 3925 | `					PH7_MemObjRelease(pTos);` |
|        3 | 3926 | `					pc = iResumePc;` |
|        3 | 3927 | `					break;` |
|        - | 3928 | `				}` |
|      ! 0 | 3929 | `				goto Exception;` |
|        - | 3930 | `			}` |
|        - | 3931 | `			/* Copy result */` |
|       73 | 3932 | `			PH7_MemObjStore(&sResult,pTos);` |
|       73 | 3933 | `			PH7_MemObjRelease(&sResult);` |
|      124 | 3934 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       86 | 3935 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - | 3936 | `			ph7_value sResult;` |
|        - | 3937 | `			sxi32 rcInv;` |
|        - | 3938 | `			/* __invoke object callable: the object slot isn't popped, so pArg is` |
|        - | 3939 | `			 * already this call's arg base — build the map + consume the runs. */` |
|      128 | 3940 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|       84 | 3941 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|       86 | 3942 | `			SySetReset(&aArg);` |
|      204 | 3943 | `			while( pArg < pTos ){` |
|      120 | 3944 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      120 | 3945 | `				pArg++;` |
|        2 | 3946 | `			}` |
|       86 | 3947 | `			PH7_MemObjInit(pVm,&sResult);` |
|      128 | 3948 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       84 | 3949 | `				(int)SySetUsed(&aArg),` |
|       84 | 3950 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - | 3951 | `				&sResult,` |
|       42 | 3952 | `				pEffCallMap);` |
|       86 | 3953 | `			SySetReset(&aArg);` |
|        - | 3954 | `			/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable` |
|        - | 3955 | `			 * slot itself (it pops top-down and the callable IS pTos), which for a` |
|        - | 3956 | `			 * temporary like (new Plain())(...) holds the only reference — popping it` |
|        - | 3957 | `			 * would free pThis before VmRaiseNotCallable reads its class name below.` |
|        - | 3958 | `			 * Only the not-callable branch dereferences pThis afterwards, so pin just` |
|        - | 3959 | `			 * for that case; the matching PH7_ClassInstanceUnref drops it (destroying` |
|        - | 3960 | `			 * the temporary). The other branches let the pop free the temp as before. */` |
|       86 | 3961 | `			if( rcInv == SXERR_INVALID ){` |
|       13 | 3962 | `				pThis->iRef++;` |
|        6 | 3963 | `			}` |
|       86 | 3964 | `			if( nCallArgs > 0 ){` |
|       78 | 3965 | `				VmPopOperand(&pTos,nCallArgs);` |
|       38 | 3966 | `			}` |
|       86 | 3967 | `			if( rcInv == SXERR_INVALID ){` |
|        - | 3968 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - | 3969 | `				 * sResult was already released by VmCallObjectInvoke. */` |
|       13 | 3970 | `				PH7_MemObjRelease(pTos);` |
|       13 | 3971 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 | 3972 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 | 3973 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3974 | `					goto Abort;` |
|        - | 3975 | `				}` |
|        - | 3976 | `				{` |
|        - | 3977 | `					sxi32 iRp;` |
|       13 | 3978 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|       13 | 3979 | `						pc = iRp;` |
|       13 | 3980 | `						break;` |
|        - | 3981 | `					}` |
|        - | 3982 | `				}` |
|      ! 0 | 3983 | `				goto Exception;` |
|        - | 3984 | `			}` |
|       74 | 3985 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 | 3986 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 3987 | `				goto Abort;` |
|        - | 3988 | `			}` |
|       74 | 3989 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - | 3990 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - | 3991 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - | 3992 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - | 3993 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - | 3994 | `				sxi32 iResumePc;` |
|        7 | 3995 | `				PH7_MemObjRelease(&sResult);` |
|        7 | 3996 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        5 | 3997 | `					PH7_MemObjRelease(pTos);` |
|        5 | 3998 | `					pc = iResumePc;` |
|        5 | 3999 | `					break;` |
|        - | 4000 | `				}` |
|        3 | 4001 | `				goto Exception;` |
|        - | 4002 | `			}` |
|       68 | 4003 | `			PH7_MemObjStore(&sResult,pTos);` |
|       68 | 4004 | `			PH7_MemObjRelease(&sResult);` |
|       35 | 4005 | `		}else{` |
|        - | 4006 | `			/* php: calling a non-callable is a catchable Error naming the type` |
|        - | 4007 | `			 * ("Value of type int is not callable"), or -- for an array -- the shape it` |
|        - | 4008 | `			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so` |
|        - | 4009 | ``			 * `$x()` on a number quietly evaluated to nothing. */`` |
|        - | 4010 | `			sxi32 rcNc;` |
|        - | 4011 | `			char zMsg[128];` |
|        3 | 4012 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 4013 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");` |
|      ! 0 | 4014 | `			}else{` |
|        4 | 4015 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",` |
|        1 | 4016 | `					VmArithTypeName(pTos));` |
|        - | 4017 | `			}` |
|        - | 4018 | `			/* Consume this call's captured spread runs — a non-callable target` |
|        - | 4019 | `			 * (int/float/bool/null) reaches no dispatch build site. */` |
|        3 | 4020 | `			if( pInstr->iP2 ){` |
|      ! 0 | 4021 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 4022 | `			}` |
|        - | 4023 | `			/* Pop given arguments */` |
|        3 | 4024 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 4025 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 4026 | `			}` |
|        - | 4027 | `			/* Settle the call's result slot BEFORE throwing. */` |
|        3 | 4028 | `			PH7_MemObjRelease(pTos);` |
|        3 | 4029 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 4030 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 4031 | `			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|        3 | 4032 | `			if( rcNc == SXERR_ABORT ){ goto Abort; }` |
|        3 | 4033 | `			rc = rcNc;` |
|        3 | 4034 | `			PH7_DISPATCH_ENFORCE_RC(rc)` |
|        - | 4035 | `		}` |
|      142 | 4036 | `		break;` |
|        - | 4037 | `	}` |
|  1029985 | 4038 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 4039 | `	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just` |
|        - | 4040 | `	 * anchors it to the global namespace — strip it before resolving so a` |
|        - | 4041 | ``	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */`` |
|  1029985 | 4042 | `	if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|       13 | 4043 | `		sName.zString++;` |
|       13 | 4044 | `		sName.nByte--;` |
|        6 | 4045 | `	}` |
|        - | 4046 | `	/* Check for a compiled function first.` |
|        - | 4047 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 4048 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|  1029985 | 4049 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 4050 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - | 4051 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - | 4052 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - | 4053 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - | 4054 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - | 4055 | `	{` |
|  1029985 | 4056 | `	VmCallArgMap *pCallMap = pEffCallMap;` |
|  1029985 | 4057 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - | 4058 | `		const char *zFunc;` |
|        - | 4059 | `		const char *zEnd;` |
|        - | 4060 | `		const char *z;` |
|        - | 4061 | `		SyString sGlobal;` |
|       32 | 4062 | `		zFunc = sName.zString;` |
|       32 | 4063 | `		zEnd  = zFunc + sName.nByte;` |
|       32 | 4064 | `		z = zEnd;` |
|        - | 4065 | `		/* Find last namespace separator */` |
|      286 | 4066 | `		while( z > zFunc ){` |
|      286 | 4067 | `			if( z[-1] == '\\' ){` |
|       32 | 4068 | `				break;` |
|        - | 4069 | `			}` |
|      258 | 4070 | `			z--;` |
|        4 | 4071 | `		}` |
|       32 | 4072 | `		if( z > zFunc && z < zEnd ){` |
|        - | 4073 | `			/* Retry lookup using the unqualified/global function name */` |
|       32 | 4074 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       32 | 4075 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       14 | 4076 | `		}` |
|       14 | 4077 | `	}` |
|        - | 4078 | `	} /* end VmCallArgMap namespace scope */` |
|  1029985 | 4079 | `	if( pEntry ){` |
|        - | 4080 | `		ph7_vm_func_arg *aFormalArg;` |
|        - | 4081 | `		ph7_class_instance *pThis;` |
|        - | 4082 | `		ph7_value *pFrameStack;` |
|        - | 4083 | `		ph7_vm_func *pVmFunc;` |
|        - | 4084 | `		ph7_class *pSelf;` |
|        - | 4085 | `		ph7_class *pSelfHint;` |
|        - | 4086 | `		VmFrame *pFrame;` |
|        - | 4087 | `		ph7_value *pObj;` |
|        - | 4088 | `		VmSlot sArg;` |
|        - | 4089 | `		sxu32 n;` |
|    89133 | 4090 | `		int bClosureThis = 0;` |
|    89133 | 4091 | `		ph7_class *pClosureScope = 0;` |
|        - | 4092 | `		/* initialize fields */` |
|    89133 | 4093 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    89133 | 4094 | `		pThis = 0;` |
|    89133 | 4095 | `		pSelf = 0;` |
|        - | 4096 | `		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it` |
|        - | 4097 | `		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever` |
|        - | 4098 | `		 * set for a bound plain closure, which dispatches as a function, so the method branch below` |
|        - | 4099 | `		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */` |
|    89133 | 4100 | `		if( pVm->pClosureThis ){` |
|       33 | 4101 | `			pThis = pVm->pClosureThis;` |
|       33 | 4102 | `			pVm->pClosureThis = 0;` |
|       33 | 4103 | `			bClosureThis = 1;` |
|       16 | 4104 | `		}` |
|    89133 | 4105 | `		if( pVm->pClosureScope ){` |
|        - | 4106 | `			/* May ride alongside a bound $this, or stand alone for a` |
|        - | 4107 | ``			 * scope-only rebind (`bindTo(null, Scope::class)`). */`` |
|       29 | 4108 | `			pClosureScope = pVm->pClosureScope;` |
|       29 | 4109 | `			pVm->pClosureScope = 0;` |
|       14 | 4110 | `		}` |
|    89133 | 4111 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - | 4112 | `			ph7_class_method *pMeth;` |
|        - | 4113 | `			/* Class method call */` |
|    24873 | 4114 | `			ph7_value *pTarget = &pTos[-1];` |
|    24873 | 4115 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - | 4116 | `				/* Extract the 'this' pointer */` |
|    24873 | 4117 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - | 4118 | `					/* Instance already loaded */` |
|    23837 | 4119 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|    23837 | 4120 | `					pThis->iRef++;` |
|    23837 | 4121 | `					pSelf = pThis->pClass;` |
|    11916 | 4122 | `				}` |
|    24873 | 4123 | `				if( pSelf == 0 ){` |
|     1041 | 4124 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - | 4125 | `						/* "Late Static Binding" class name */` |
|     1472 | 4126 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|      489 | 4127 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|      489 | 4128 | `					}` |
|     1041 | 4129 | `					if( pSelf == 0 ){` |
|       60 | 4130 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       29 | 4131 | `					}` |
|      518 | 4132 | `				}` |
|    24873 | 4133 | `				if( pThis == 0  ){` |
|     1041 | 4134 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|     1041 | 4135 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|     1041 | 4136 | `					if( pFrameLocal->pParent ){` |
|        - | 4137 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|      711 | 4138 | `						pThis = pFrameLocal->pThis;` |
|      711 | 4139 | `						if( pThis ){` |
|      182 | 4140 | `							pThis->iRef++;` |
|       90 | 4141 | `						}` |
|      353 | 4142 | `					}` |
|      518 | 4143 | `				}` |
|    24873 | 4144 | `				VmPopOperand(&pTos,1);` |
|    24873 | 4145 | `				PH7_MemObjRelease(pTos);` |
|        - | 4146 | `				/* Synchronize pointers. The method-name slot popped above sat BETWEEN` |
|        - | 4147 | `				 * this call's arguments and the (already-removed) target — so only now` |
|        - | 4148 | `				 * is pTos one past the last actual argument. Re-derive the unpack` |
|        - | 4149 | `				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs` |
|        - | 4150 | `				 * from run ends, which the extra target slot would otherwise offset,` |
|        - | 4151 | ``				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */`` |
|    24873 | 4152 | `				nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);` |
|    24873 | 4153 | `				pArg = &pTos[-nCallArgs];` |
|        - | 4154 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - | 4155 | `				 * user have already computed the random generated unique class method name` |
|        - | 4156 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - | 4157 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - | 4158 | `				 */` |
|    24873 | 4159 | `				while( pArg < pStack ){` |
|      ! 0 | 4160 | `					pArg++;` |
|      ! 0 | 4161 | `				}` |
|    24873 | 4162 | `				if( pSelf && pVm->bReflectBypass ){` |
|        - | 4163 | `					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection` |
|        - | 4164 | `					 * ignores visibility. Consume-once so nested calls made by the` |
|        - | 4165 | `					 * invoked body are checked normally. */` |
|       11 | 4166 | `					pVm->bReflectBypass = 0;` |
|        6 | 4167 | `				}else` |
|    24863 | 4168 | `				if( pSelf ){ /* Paranoid edition */` |
|        - | 4169 | `					/* Check if the call is allowed. php binds non-public method` |
|        - | 4170 | `					 * access by the DECLARING class (pVmFunc->pUserData — the class` |
|        - | 4171 | `					 * the callee was compiled in), NOT the instance's class: an` |
|        - | 4172 | `					 * inherited base method calling $this->priv() on a child` |
|        - | 4173 | `					 * instance passes, a child's private SHADOW doesn't hijack the` |
|        - | 4174 | `					 * check for a parent callee, and the denial message names the` |
|        - | 4175 | `					 * declaring class like php. */` |
|    24863 | 4176 | `					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|    24863 | 4177 | `					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|    24863 | 4178 | `					if( pMeth == 0 && pDeclClass != pSelf ){` |
|      ! 0 | 4179 | `						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      ! 0 | 4180 | `					}` |
|    24863 | 4181 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     2602 | 4182 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pDeclClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - | 4183 | `							/* php throws a CATCHABLE Error here. The old code merely PRINTED an` |
|        - | 4184 | ``							 * uncaught-exception report and aborted, so `try { $o->priv(); }`` |
|        - | 4185 | ``							 * catch (Error $e)` never caught it and the script died. */`` |
|        - | 4186 | `							char zMsg[256];` |
|        - | 4187 | `							sxi32 rcVis;` |
|        7 | 4188 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       10 | 4189 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|        6 | 4190 | `								zVis,(int)pDeclClass->sName.nByte,pDeclClass->sName.zString,` |
|        6 | 4191 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - | 4192 | `							/* Consume this call's captured spread runs — this visibility` |
|        - | 4193 | `							 * error exits before the pVmFunc build below. */` |
|        7 | 4194 | `							if( pInstr->iP2 ){` |
|      ! 0 | 4195 | `								VmSpreadConsume(pVm);` |
|      ! 0 | 4196 | `							}` |
|        - | 4197 | `							/* Pop given arguments, and leave the call's NULL result behind. */` |
|        7 | 4198 | `							if( nCallArgs > 0 ){` |
|      ! 0 | 4199 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 4200 | `							}` |
|        7 | 4201 | `							PH7_MemObjRelease(pTos);` |
|        7 | 4202 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 | 4203 | `							pTos->nIdx = SXU32_HIGH;` |
|        7 | 4204 | `							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|        7 | 4205 | `							if( rcVis == SXERR_ABORT ){ goto Abort; }` |
|        7 | 4206 | `							rc = rcVis;` |
|        7 | 4207 | `							PH7_DISPATCH_ENFORCE_RC(rc)` |
|        3 | 4208 | `						}` |
|     1299 | 4209 | `					}` |
|    12429 | 4210 | `				}` |
|    12434 | 4211 | `			}` |
|    12434 | 4212 | `		}` |
|        - | 4213 | `		/* pArg is now finalized for every pVmFunc callee (a method call popped its` |
|        - | 4214 | `		 * method-name slot above; functions/closures/generators keep the top base).` |
|        - | 4215 | `		 * Build the PHP 8.1 effective spread-key map here so the generator and the` |
|        - | 4216 | `		 * install path below both see it — and so this call's captured runs are` |
|        - | 4217 | `		 * consumed exactly once, against the correct base. */` |
|   133746 | 4218 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|    89128 | 4219 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 4220 | `		/* Check the PHP call-depth cap (the sole site — BYTECODE.md stage 5).` |
|        - | 4221 | `		 * Default is unbounded (heap-bound recursion, decoupled from the C stack` |
|        - | 4222 | `		 * by the stage-2 trampoline); the C stack is guarded separately by` |
|        - | 4223 | `		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and` |
|        - | 4224 | `		 * then raises a clean non-catchable fatal (was: silently set NULL and` |
|        - | 4225 | `		 * continue) and halts. */` |
|    89133 | 4226 | `		if( VmRecursionExceeded(pVm) ){` |
|        - | 4227 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - | 4228 | `			 * which walks the whole operand stack — don't release them here. */` |
|        3 | 4229 | `			VmRecursionFatal(&(*pVm));` |
|        3 | 4230 | `			goto Abort;` |
|        - | 4231 | `		}` |
|    89131 | 4232 | `		if( pVmFunc->pNextName ){` |
|        - | 4233 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      245 | 4234 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|      121 | 4235 | `		}` |
|        - | 4236 | ``		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's`` |
|        - | 4237 | ``		 * `Class::method`). Computed after overload resolution so it reflects the selected method.`` |
|        - | 4238 | ``		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed`` |
|        - | 4239 | `		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method` |
|        - | 4240 | ``		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the`` |
|        - | 4241 | `		 * USING class; we don't carry the using class on the shared struct, so fall back to the` |
|        - | 4242 | `		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for` |
|        - | 4243 | `		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */` |
|    89131 | 4244 | `		pSelfHint = pSelf;` |
|    89131 | 4245 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|    24873 | 4246 | `			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;` |
|    24873 | 4247 | `			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|    23855 | 4248 | `				pSelfHint = pDecl;` |
|    11925 | 4249 | `			}` |
|    12434 | 4250 | `		}` |
|    89131 | 4251 | `		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 4252 | `			/* Push the closure's called-class as this frame's LSB class so` |
|        - | 4253 | ``			 * `static::` inside the body resolves like php. An explicit`` |
|        - | 4254 | `			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use` |
|        - | 4255 | `			 * the class captured at the closure's creation site. self::/parent::` |
|        - | 4256 | `			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done` |
|        - | 4257 | ``			 * after pSelfHint so a `self`-typed param is unaffected. */`` |
|     1745 | 4258 | `			if( pClosureScope ){` |
|       29 | 4259 | `				pSelf = pClosureScope;` |
|     1731 | 4260 | `			}else if( pVmFunc->pLsbClass ){` |
|       60 | 4261 | `				pSelf = (ph7_class *)pVmFunc->pLsbClass;` |
|       29 | 4262 | `			}` |
|      870 | 4263 | `		}` |
|    89131 | 4264 | `		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){` |
|        - | 4265 | `			/* php 8.4 #[\Deprecated] runtime notice — once per call, before` |
|        - | 4266 | `			 * execution (generators: at the g(...) call site, like php). */` |
|      176 | 4267 | `			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,` |
|      114 | 4268 | `				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);` |
|       57 | 4269 | `		}` |
|    89131 | 4270 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - | 4271 | `			/* Generator function: return a Generator object instead of executing */` |
|        - | 4272 | `			ph7_exec_ctx *pExecCtx;` |
|        - | 4273 | `			ph7_generator *pGenerator;` |
|        - | 4274 | `			ph7_class_instance *pGenObj;` |
|        - | 4275 | `			ph7_value *pCtxAttr;` |
|        - | 4276 | `			SyString sAttrName;` |
|        - | 4277 | `			ph7_value **apCallArgs;` |
|        - | 4278 | `			int nGenArgs, iArg;` |
|        - | 4279 | `			/* Collect arguments from the operand stack */` |
|      345 | 4280 | `			nGenArgs = (int)(pTos - pArg);` |
|      345 | 4281 | `			apCallArgs = 0;` |
|      345 | 4282 | `			if( nGenArgs > 0 ){` |
|      109 | 4283 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       35 | 4284 | `					nGenArgs * sizeof(ph7_value *));` |
|       74 | 4285 | `				if( apCallArgs == 0 ){` |
|        - | 4286 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 | 4287 | `					nGenArgs = 0;` |
|      ! 0 | 4288 | `				}else{` |
|       74 | 4289 | `					VmCallArgMap *pGenMap = pEffCallMap;` |
|       74 | 4290 | `					int didReorder = 0;` |
|       74 | 4291 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - | 4292 | `						/* Named-argument reordering for generator */` |
|       10 | 4293 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|       10 | 4294 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|       10 | 4295 | `						sxu32 nNV = nF;` |
|       10 | 4296 | `						sxi32 iVIdx = -1;` |
|        - | 4297 | `						sxi32 *aGSlot;` |
|        - | 4298 | `						sxu8 *aGUsed;` |
|        - | 4299 | `						sxu32 gi;` |
|       22 | 4300 | `						for( gi = 0; gi < nF; gi++ ){` |
|       14 | 4301 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        8 | 4302 | `						}` |
|       14 | 4303 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        8 | 4304 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|       10 | 4305 | `						if( aGSlot ){` |
|       10 | 4306 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|       14 | 4307 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        4 | 4308 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|       10 | 4309 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 4310 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 4311 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4312 | `								goto Abort;` |
|        - | 4313 | `							}` |
|       10 | 4314 | `							if( rc == PH7_EXCEPTION ){` |
|        - | 4315 | `								/* A named-argument error is php's catchable \Error.` |
|        - | 4316 | `								 * No callee frame exists yet on this branch (the` |
|        - | 4317 | `								 * generator body never runs and VmEnterFrame is` |
|        - | 4318 | `								 * further down), so route it like the other` |
|        - | 4319 | `								 * pre-frame OP_CALL throws: drop the args + the` |
|        - | 4320 | `								 * function-name slot and land the enclosing try. */` |
|        3 | 4321 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        3 | 4322 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      173 | 4323 | `								PH7_INLINE_RESUME_BREAK()` |
|        3 | 4324 | `								VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 4325 | `								{` |
|        - | 4326 | `									sxi32 iRpN;` |
|        3 | 4327 | `									if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){` |
|        3 | 4328 | `										pc = iRpN;` |
|        3 | 4329 | `										break;` |
|        - | 4330 | `									}` |
|        - | 4331 | `								}` |
|      ! 0 | 4332 | `								goto Exception;` |
|        - | 4333 | `							}` |
|        8 | 4334 | `							if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){` |
|        - | 4335 | `								/* php's named-hole ArgumentCountError, checked BEFORE` |
|        - | 4336 | `								 * hole compaction: compacting first would report the` |
|        - | 4337 | `								 * positional wording with a fictitious count (g(b:2)` |
|        - | 4338 | ``								 * must be `g(): Argument #1 ($a) not passed`, not`` |
|        - | 4339 | `								 * "1 passed"). Implicit-required watermark, like the` |
|        - | 4340 | `								 * plain-call named path; a hole with NOTHING filled` |
|        - | 4341 | `								 * above it keeps php's count wording — fall through` |
|        - | 4342 | `								 * to VmFiberSetupFrame's check (the compacted count` |
|        - | 4343 | `								 * equals php's num_args there). */` |
|        8 | 4344 | `								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;` |
|        8 | 4345 | `								sxi32 iHole = -1;` |
|        8 | 4346 | `								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);` |
|       18 | 4347 | `								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){` |
|       12 | 4348 | `									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){` |
|       10 | 4349 | `										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);` |
|        4 | 4350 | `									}` |
|        7 | 4351 | `								}` |
|       18 | 4352 | `								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){` |
|        - | 4353 | `									sxu32 gj;` |
|       12 | 4354 | `									int bFound = 0;` |
|       16 | 4355 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       16 | 4356 | `										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }` |
|        3 | 4357 | `									}` |
|       12 | 4358 | `									if( !bFound && gHole + 1 <= nMaxFilledG ){` |
|      ! 0 | 4359 | `										iHole = (sxi32)gHole;` |
|      ! 0 | 4360 | `									}` |
|        7 | 4361 | `								}` |
|        8 | 4362 | `								if( iHole >= 0 ){` |
|      ! 0 | 4363 | `									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 4364 | `										(sxu32)iHole+1,&aFA[iHole].sName);` |
|      ! 0 | 4365 | `									SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 4366 | `									SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4367 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 4368 | `										goto Abort;` |
|        - | 4369 | `									}` |
|        - | 4370 | `									/* Route like the VmFiberSetupFrame throw below */` |
|      ! 0 | 4371 | `									PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 4372 | `									VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 4373 | `									{` |
|        - | 4374 | `										sxi32 iRpH;` |
|      ! 0 | 4375 | `										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 4376 | `											pc = iRpH;` |
|      ! 0 | 4377 | `											break;` |
|        - | 4378 | `										}` |
|        - | 4379 | `									}` |
|      ! 0 | 4380 | `									goto Exception;` |
|        - | 4381 | `								}` |
|        3 | 4382 | `							}` |
|        - | 4383 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - | 4384 | `							 * append overflow (variadic / positional beyond` |
|        - | 4385 | `							 * formals) so downstream sees every argument. */` |
|        - | 4386 | `							{` |
|        8 | 4387 | `								int nOut = 0;` |
|       18 | 4388 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - | 4389 | `									sxu32 gj;` |
|       16 | 4390 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       16 | 4391 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|       12 | 4392 | `											apCallArgs[nOut++] = &pArg[gj];` |
|       12 | 4393 | `											break;` |
|        - | 4394 | `										}` |
|        3 | 4395 | `									}` |
|        7 | 4396 | `								}` |
|       18 | 4397 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|       12 | 4398 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 | 4399 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 | 4400 | `									}` |
|        7 | 4401 | `								}` |
|        8 | 4402 | `								nGenArgs = nOut;` |
|        - | 4403 | `							}` |
|        8 | 4404 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        8 | 4405 | `							didReorder = 1;` |
|        3 | 4406 | `						}` |
|        - | 4407 | `						/* If aGSlot allocation failed, fall through to` |
|        - | 4408 | `						 * positional fill below — preserves arg order rather` |
|        - | 4409 | `						 * than passing an uninitialized apCallArgs. */` |
|        3 | 4410 | `					}` |
|       72 | 4411 | `					if( !didReorder ){` |
|      134 | 4412 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|       72 | 4413 | `							apCallArgs[iArg] = &pArg[iArg];` |
|       38 | 4414 | `						}` |
|       31 | 4415 | `					}` |
|        - | 4416 | `				}` |
|       34 | 4417 | `			}` |
|        - | 4418 | `			/* Create execution context and generator wrapper */` |
|      343 | 4419 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|      343 | 4420 | `			if( pExecCtx == 0 ){` |
|      ! 0 | 4421 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4422 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 4423 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 4424 | `				break;` |
|        - | 4425 | `			}` |
|      343 | 4426 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|      343 | 4427 | `			if( pGenerator == 0 ){` |
|      ! 0 | 4428 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 | 4429 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4430 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 4431 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 4432 | `				break;` |
|        - | 4433 | `			}` |
|        - | 4434 | `			/* Set up the frame with arguments, closure env, $this */` |
|      343 | 4435 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|      343 | 4436 | `			pVm->pFrame = pExecCtx->pFrame;` |
|      349 | 4437 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,` |
|      172 | 4438 | `				(pEffCallMap && pEffCallMap->bStrict) ? 1 : 0, pSelfHint,` |
|        - | 4439 | `				TRUE/*generator: the g(...) call site is in the message*/);` |
|      343 | 4440 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|      343 | 4441 | `			pExecCtx->pFrame->pParent = 0;` |
|      343 | 4442 | `			if( apCallArgs ){` |
|       72 | 4443 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       34 | 4444 | `			}` |
|      343 | 4445 | `			if( rc != SXRET_OK ){` |
|       15 | 4446 | `				VmReleaseGenerator(pVm, pGenerator);` |
|       15 | 4447 | `				if( pThis ){` |
|        3 | 4448 | `					PH7_ClassInstanceUnref(pThis);` |
|        1 | 4449 | `				}` |
|       15 | 4450 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4451 | `					goto Abort;` |
|        - | 4452 | `				}` |
|       15 | 4453 | `				if( rc == PH7_EXCEPTION ){` |
|        - | 4454 | `					/* A declared-type TypeError thrown while binding the` |
|        - | 4455 | `					 * generator's arguments (php binds + type-checks eagerly at` |
|        - | 4456 | `					 * the g(...) call site, before any resume — band A #2). If` |
|        - | 4457 | `					 * an inline try THIS exec owns caught it, land at its` |
|        - | 4458 | `					 * redirect (the drain subsumes the operand pops); else pop` |
|        - | 4459 | `					 * the args + function name and route like the other` |
|        - | 4460 | `					 * OP_CALL throw paths. */` |
|       19 | 4461 | `					PH7_INLINE_RESUME_BREAK()` |
|       13 | 4462 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 4463 | `					{` |
|        - | 4464 | `						sxi32 iRpG;` |
|       13 | 4465 | `						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){` |
|       13 | 4466 | `							pc = iRpG;` |
|       13 | 4467 | `							break;` |
|        - | 4468 | `						}` |
|        - | 4469 | `					}` |
|      ! 0 | 4470 | `					goto Exception;` |
|        - | 4471 | `				}` |
|      ! 0 | 4472 | `				break;` |
|        - | 4473 | `			}` |
|        - | 4474 | `			/* Create Generator class instance */` |
|      329 | 4475 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|      329 | 4476 | `			if( pGenObj == 0 ){` |
|      ! 0 | 4477 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 | 4478 | `				break;` |
|        - | 4479 | `			}` |
|        - | 4480 | `			/* Store generator in __ctx attribute */` |
|      329 | 4481 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      329 | 4482 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|      329 | 4483 | `			if( pCtxAttr ){` |
|      329 | 4484 | `				pCtxAttr->x.pOther = pGenerator;` |
|      329 | 4485 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      162 | 4486 | `			}` |
|        - | 4487 | `			/* Pop args and function name, push Generator object. PH7_NewClassInstance` |
|        - | 4488 | `			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do` |
|        - | 4489 | `			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope` |
|        - | 4490 | ``			 * and its __destruct (which runs pending `finally` blocks and frees the`` |
|        - | 4491 | `			 * exec context) never fires. */` |
|      329 | 4492 | `			PH7_MemObjRelease(pTos);` |
|      329 | 4493 | `			pTos = &pTos[-nCallArgs];` |
|      329 | 4494 | `			pTos->x.pOther = pGenObj;` |
|      329 | 4495 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|      329 | 4496 | `			if( pThis ){` |
|       29 | 4497 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 | 4498 | `			}` |
|      329 | 4499 | `			break;` |
|        - | 4500 | `		}` |
|        - | 4501 | `		/* Extract the formal argument set */` |
|    88791 | 4502 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - | 4503 | `		/* Create a new VM frame  */` |
|    88791 | 4504 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    88791 | 4505 | `		if( rc != SXRET_OK ){` |
|        - | 4506 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 4507 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 4508 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 4509 | `				&pVmFunc->sName);` |
|        - | 4510 | `			/* The frame that would own (and later release) $this never got created; for a bound` |
|        - | 4511 | `			 * plain closure the consumed transient is the object's ONLY ref, so release it here` |
|        - | 4512 | `			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */` |
|      ! 0 | 4513 | `			if( bClosureThis && pThis ){` |
|      ! 0 | 4514 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 | 4515 | `			}` |
|        - | 4516 | `			/* Pop given arguments */` |
|      ! 0 | 4517 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 4518 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 4519 | `			}` |
|        - | 4520 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 | 4521 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 4522 | `			break;` |
|        - | 4523 | `		}` |
|    88791 | 4524 | `		if( pClosureScope ){` |
|        - | 4525 | `			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /` |
|        - | 4526 | `			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected` |
|        - | 4527 | `			 * access inside the body resolves against it. */` |
|       27 | 4528 | `			pFrame->pBoundScope = pClosureScope;` |
|       13 | 4529 | `		}` |
|        - | 4530 | `		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band` |
|        - | 4531 | `		 * A #4): sArg over-counts (defaulted params installed, variadic packed` |
|        - | 4532 | `		 * as one entry) so php's answers can't be derived from it. */` |
|    88791 | 4533 | `		pFrame->nActualArgs = (int)(pTos - pArg);` |
|    88791 | 4534 | `		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) \|\| bClosureThis) ){` |
|        - | 4535 | `			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */` |
|        - | 4536 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|    24021 | 4537 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|    24021 | 4538 | `			if( pObj ){` |
|        - | 4539 | `				/* Reflect the change */` |
|    24021 | 4540 | `				pObj->x.pOther = pThis;` |
|    24021 | 4541 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|    12008 | 4542 | `			}` |
|    12008 | 4543 | `		}` |
|    88791 | 4544 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - | 4545 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - | 4546 | `			/* Install static variables */` |
|     1102 | 4547 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|     2200 | 4548 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|     1102 | 4549 | `				pStatic = &aStatic[n];` |
|     1102 | 4550 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - | 4551 | `					/* Initialize the static variables */` |
|       30 | 4552 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|       30 | 4553 | `					if( pObj ){` |
|        - | 4554 | `						/* Assume a NULL initialization value */` |
|       30 | 4555 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|       30 | 4556 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - | 4557 | `							/* Evaluate initialization expression (Any complex expression) */` |
|       30 | 4558 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|       13 | 4559 | `						}` |
|       30 | 4560 | `						pObj->nIdx = pStatic->nIdx;` |
|       17 | 4561 | `					}else{` |
|      ! 0 | 4562 | `						continue;` |
|        - | 4563 | `					}` |
|       13 | 4564 | `				}` |
|        - | 4565 | `				/* Install in the current frame */` |
|     1651 | 4566 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|     1098 | 4567 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      553 | 4568 | `			}` |
|      549 | 4569 | `		}` |
|        - | 4570 | `		/* Push arguments in the local frame */` |
|        - | 4571 | `		{` |
|    88791 | 4572 | `		VmCallArgMap *pCallMap3 = pEffCallMap;` |
|        - | 4573 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - | 4574 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    88791 | 4575 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    88791 | 4576 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - | 4577 | `			/* ============================================================` |
|        - | 4578 | `			 * Named-argument matching path (PHP 8.0)` |
|        - | 4579 | `			 *` |
|        - | 4580 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - | 4581 | `			 * or position, then install them in the frame.` |
|        - | 4582 | `			 * ============================================================ */` |
|      272 | 4583 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      272 | 4584 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|      272 | 4585 | `			sxi32 iVariadicIdx = -1;` |
|        - | 4586 | `			sxu32 nNonVariadic;` |
|        - | 4587 | `			sxi32 *aSlot;` |
|        - | 4588 | `			sxu8  *aUsed;` |
|        - | 4589 | `			sxu32 i;` |
|        - | 4590 | `			/* Find variadic parameter index */` |
|      770 | 4591 | `			for( i = 0; i < nFormal; i++ ){` |
|      548 | 4592 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       47 | 4593 | `					iVariadicIdx = (sxi32)i;` |
|       47 | 4594 | `					break;` |
|        - | 4595 | `				}` |
|      253 | 4596 | `			}` |
|      272 | 4597 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - | 4598 | `			/* Allocate mapping arrays */` |
|      406 | 4599 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      268 | 4600 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|      272 | 4601 | `			if( aSlot == 0 ){` |
|      ! 0 | 4602 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 | 4603 | `				goto Abort;` |
|        - | 4604 | `			}` |
|      272 | 4605 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - | 4606 | `			/* Resolve named arguments to formal parameters */` |
|      406 | 4607 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|      134 | 4608 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|      272 | 4609 | `			if( rc == PH7_ABORT ){` |
|        8 | 4610 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 | 4611 | `				goto Abort;` |
|        - | 4612 | `			}` |
|      265 | 4613 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 4614 | `				/* php's catchable \Error for a bad named argument. The callee` |
|        - | 4615 | `				 * frame is already entered but its body must not run: unwind` |
|        - | 4616 | `				 * exactly like the named-hole ArgumentCountError path below` |
|        - | 4617 | `				 * (release the not-yet-installed actuals — Pass 2's release` |
|        - | 4618 | `				 * loop has not run — pop the callee slot, mark that there is no` |
|        - | 4619 | `				 * callee operand stack, and let VmCallFinish route the throw). */` |
|        - | 4620 | `				sxu32 iRel;` |
|        5 | 4621 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       11 | 4622 | `				for( iRel = 0; iRel < nActual; iRel++ ){` |
|        7 | 4623 | `					PH7_MemObjRelease(&pArg[iRel]);` |
|        4 | 4624 | `				}` |
|        5 | 4625 | `				PH7_MemObjRelease(pTos);` |
|        5 | 4626 | `				pTos = &pTos[-nCallArgs];` |
|        5 | 4627 | `				pFrameStack = 0;` |
|        5 | 4628 | `				goto SkipFuncBody;` |
|        - | 4629 | `			}` |
|        - | 4630 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|        - | 4631 | `			{` |
|        - | 4632 | `			/* php's required watermark for the hole check below (0 disables it` |
|        - | 4633 | `			 * for hosted builtin FUNCTIONS, which self-manage — hosted-class` |
|        - | 4634 | `			 * methods and all user code get php's named-hole error), plus the` |
|        - | 4635 | `			 * highest formal slot an actual resolved to: php words a hole` |
|        - | 4636 | ``			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole`` |
|        - | 4637 | `			 * with nothing filled above it gets the positional count message` |
|        - | 4638 | `			 * (zend's RECV arg_num > EX(num_args) distinction). */` |
|      261 | 4639 | `			sxu32 nReqNamed = 0;` |
|      261 | 4640 | `			sxu32 nNVNamed = 0;` |
|      261 | 4641 | `			sxu32 nMaxFilled = 0;` |
|      261 | 4642 | `			if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){` |
|      261 | 4643 | `				nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);` |
|     1009 | 4644 | `				for( i = 0; i < nActual; i++ ){` |
|      751 | 4645 | `					if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|      305 | 4646 | `						nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|      151 | 4647 | `					}` |
|      377 | 4648 | `				}` |
|      129 | 4649 | `			}` |
|      731 | 4650 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - | 4651 | `				/* Find the stack arg mapped to formal n */` |
|      483 | 4652 | `				sxi32 iSrc = -1;` |
|      777 | 4653 | `				for( i = 0; i < nActual; i++ ){` |
|      673 | 4654 | `					if( aSlot[i] == (sxi32)n ){` |
|      379 | 4655 | `						iSrc = (sxi32)i;` |
|      379 | 4656 | `						break;` |
|        - | 4657 | `					}` |
|      149 | 4658 | `				}` |
|      483 | 4659 | `				if( iSrc >= 0 ){` |
|        - | 4660 | `					/* Argument was provided — install with type checking */` |
|      379 | 4661 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - | 4662 | `					/* An explicit null is NOT redirected to the default: PHP applies a` |
|        - | 4663 | `					 * default only for an OMITTED argument. An explicit null falls through` |
|        - | 4664 | `					 * to the type check below (TypeError for a non-nullable typed param,` |
|        - | 4665 | ``					 * kept as null for a typeless one). An implicitly-nullable `Type $x =`` |
|        - | 4666 | ``					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */`` |
|        - | 4667 | `					/* Type checking: union types */` |
|      379 | 4668 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 | 4669 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 | 4670 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 | 4671 | `							bCallIsStrict);` |
|       13 | 4672 | `						if( rcU != SXRET_OK ){` |
|        - | 4673 | `							const char *zGiven;` |
|      ! 0 | 4674 | `							const char *zExpected = "union";` |
|        - | 4675 | `							char zBuf[128];` |
|        - | 4676 | `							char zTypeBuf[128];` |
|      ! 0 | 4677 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 4678 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 | 4679 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 4680 | `								zGiven = "null";` |
|      ! 0 | 4681 | `							}else{` |
|      ! 0 | 4682 | `								zGiven = ph7_type_name(pVal);` |
|        - | 4683 | `							}` |
|      ! 0 | 4684 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 | 4685 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 | 4686 | `							}` |
|      ! 0 | 4687 | `							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|      ! 0 | 4688 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 | 4689 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 | 4690 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 4691 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 4692 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 4693 | `							pFrameStack = 0;` |
|      ! 0 | 4694 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 4695 | `							goto SkipFuncBody;` |
|        - | 4696 | `						}` |
|      371 | 4697 | `					}else if( aFormalArg[n].nType > 0` |
|      225 | 4698 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - | 4699 | `						/* Scalar/class type checking */` |
|       74 | 4700 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|        5 | 4701 | `							SyString *pName = &aFormalArg[n].sClass;` |
|        - | 4702 | `							ph7_class *pClass;` |
|        5 | 4703 | `							int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|        5 | 4704 | `							if( rcPseudo == 0 ){` |
|        - | 4705 | `								/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - | 4706 | `								char zTypeBuf[128],zGivenBuf[128];` |
|      ! 0 | 4707 | `								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|      ! 0 | 4708 | `									&aFormalArg[n].sName,` |
|      ! 0 | 4709 | `									VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 | 4710 | `									VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      ! 0 | 4711 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 | 4712 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 4713 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 | 4714 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 | 4715 | `								pFrameStack = 0;` |
|      ! 0 | 4716 | `								rc = PH7_EXCEPTION;` |
|      ! 0 | 4717 | `								goto SkipFuncBody;` |
|        - | 4718 | `							}` |
|        - | 4719 | `							/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class.` |
|        - | 4720 | ``							 * Resolve via VmResolveTypeClass so `self`/`parent` resolve and`` |
|        - | 4721 | `							 * interface/abstract hints are included (iLoadable=FALSE), then throw a` |
|        - | 4722 | `							 * catchable TypeError on mismatch — matching PHP — instead of the legacy` |
|        - | 4723 | `							 * warn + NULL-coerce (which silently ran the body with a corrupted arg). */` |
|        5 | 4724 | `							pClass = (rcPseudo == 1) ? 0 : VmResolveTypeClass(&(*pVm),pName,pSelfHint);` |
|        5 | 4725 | `							if( pClass ){` |
|        - | 4726 | `								/* Reaching here means the param is non-nullable (the guard` |
|        - | 4727 | ``								 * above skips nullable+null; a `Type $x = null` default is`` |
|        - | 4728 | `								 * marked implicitly nullable at compile time). So ANY` |
|        - | 4729 | `								 * non-object — including an explicit null — is a TypeError,` |
|        - | 4730 | `								 * matching PHP (&& below short-circuits so instanceof only` |
|        - | 4731 | `								 * derefs a real object). */` |
|        7 | 4732 | `								int bBad = !((pVal->iFlags & MEMOBJ_OBJ)` |
|        3 | 4733 | `									&& PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass));` |
|        5 | 4734 | `								if( bBad ){` |
|        - | 4735 | `									char zTypeBuf[128],zGivenBuf[128];` |
|        4 | 4736 | `									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|        2 | 4737 | `										&aFormalArg[n].sName,` |
|        2 | 4738 | `										VmSyStringToCStr(&pClass->sName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 | 4739 | `										VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        3 | 4740 | `									if( rc == PH7_ABORT ) goto Abort;` |
|        3 | 4741 | `									SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 4742 | `									PH7_MemObjRelease(pTos);` |
|        3 | 4743 | `									pTos = &pTos[-nCallArgs];` |
|        3 | 4744 | `									pFrameStack = 0;` |
|        3 | 4745 | `									rc = PH7_EXCEPTION;` |
|        3 | 4746 | `									goto SkipFuncBody;` |
|        - | 4747 | `								}` |
|        2 | 4748 | `							}` |
|       71 | 4749 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 | 4750 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 | 4751 | `								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|      ! 0 | 4752 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 | 4753 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 | 4754 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 4755 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 | 4756 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 | 4757 | `								pFrameStack = 0;` |
|      ! 0 | 4758 | `								rc = PH7_EXCEPTION;` |
|      ! 0 | 4759 | `								goto SkipFuncBody;` |
|       13 | 4760 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 4761 | `								char zTypeBuf[128];` |
|        7 | 4762 | `								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|        4 | 4763 | `									&aFormalArg[n].sName,` |
|        4 | 4764 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        2 | 4765 | `									ph7_type_name(pVal));` |
|        5 | 4766 | `								if( rc == PH7_ABORT ) goto Abort;` |
|        5 | 4767 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        5 | 4768 | `								PH7_MemObjRelease(pTos);` |
|        5 | 4769 | `								pTos = &pTos[-nCallArgs];` |
|        5 | 4770 | `								pFrameStack = 0;` |
|        5 | 4771 | `								rc = PH7_EXCEPTION;` |
|        5 | 4772 | `								goto SkipFuncBody;` |
|        - | 4773 | `							}` |
|        4 | 4774 | `						}` |
|       33 | 4775 | `					}` |
|        - | 4776 | `					/* Install: by reference or by value */` |
|      373 | 4777 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 | 4778 | `						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){` |
|        - | 4779 | `							/* php 8.1: $GLOBALS cannot be passed by reference */` |
|        - | 4780 | `							SyBlob sMsg;` |
|      ! 0 | 4781 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 4782 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 4783 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 4784 | `							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|      ! 0 | 4785 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 4786 | `								goto Abort;` |
|        - | 4787 | `							}` |
|      ! 0 | 4788 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 4789 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 4790 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 4791 | `							pFrameStack = 0;` |
|      ! 0 | 4792 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 4793 | `							goto SkipFuncBody;` |
|        - | 4794 | `						}` |
|        5 | 4795 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 | 4796 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|      ! 0 | 4797 | `							 && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){` |
|        - | 4798 | `								/* A non-lvalue bound to a by-ref parameter is a catchable Error in` |
|        - | 4799 | `								 * php — f(5) where f(&$x). PH7 only warned and quietly passed by` |
|        - | 4800 | `								 * value, so the call ran with a copy and the caller never knew.` |
|        - | 4801 | `								 * The one legitimate copy is call_user_func()'s (MEMOBJ_AUX_CUFVAL),` |
|        - | 4802 | `								 * which php also permits, with its own warning. */` |
|        - | 4803 | `								SyBlob sMsg;` |
|        - | 4804 | `								sxi32 rcRef;` |
|      ! 0 | 4805 | `								SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 4806 | `								SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 4807 | `									&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 4808 | `								rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|      ! 0 | 4809 | `									SyBlobLength(&sMsg));` |
|      ! 0 | 4810 | `								SyBlobRelease(&sMsg);` |
|      ! 0 | 4811 | `								if( rcRef == SXERR_ABORT ){` |
|      ! 0 | 4812 | `									pFrameStack = 0;` |
|      ! 0 | 4813 | `									rc = PH7_ABORT;` |
|      ! 0 | 4814 | `									goto SkipFuncBody;` |
|        - | 4815 | `								}` |
|      ! 0 | 4816 | `								pFrameStack = 0;` |
|      ! 0 | 4817 | `								rc = PH7_EXCEPTION;` |
|      ! 0 | 4818 | `								goto SkipFuncBody;` |
|        - | 4819 | `							}` |
|      ! 0 | 4820 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 4821 | `						}else{` |
|        7 | 4822 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 | 4823 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 | 4824 | `							if( pRefEntry == 0 ){` |
|        7 | 4825 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 | 4826 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 | 4827 | `								sArg.nIdx = pVal->nIdx;` |
|        5 | 4828 | `								sArg.pUserData = 0;` |
|        5 | 4829 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 | 4830 | `							}` |
|        5 | 4831 | `							pObj = 0;` |
|        - | 4832 | `						}` |
|        3 | 4833 | `					}else{` |
|      369 | 4834 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 4835 | `					}` |
|      373 | 4836 | `					if( pObj ){` |
|      369 | 4837 | `						PH7_MemObjStore(pVal,pObj);` |
|      369 | 4838 | `						sArg.nIdx = pObj->nIdx;` |
|      369 | 4839 | `						sArg.pUserData = 0;` |
|      369 | 4840 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      183 | 4841 | `					}` |
|      188 | 4842 | `				}else{` |
|        - | 4843 | `					/* Argument was NOT provided — use default or leave unset */` |
|      106 | 4844 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 4845 | `						/* Should not reach here; variadic handled separately below */` |
|      106 | 4846 | `					}else if( n < nReqNamed ){` |
|        - | 4847 | `						/* php's implicit-required rule applies to named calls` |
|        - | 4848 | `						 * too: a hole below the required watermark throws even` |
|        - | 4849 | `						 * when the formal carries a default — f($a,$b=2,$c)` |
|        - | 4850 | ``						 * called as f(a:1,c:3) is `Argument #2 ($b) not`` |
|        - | 4851 | ``						 * passed` (a hole with NO default is always below the`` |
|        - | 4852 | `						 * watermark, so this subsumes the no-default case).` |
|        - | 4853 | `						 * A hole with nothing filled ABOVE it uses php's` |
|        - | 4854 | `						 * positional count wording instead. The passed stack` |
|        - | 4855 | `						 * args were not released yet on this path (that loop` |
|        - | 4856 | `						 * runs after Pass 2) — release them before the exit. */` |
|        5 | 4857 | `						if( n + 1 > nMaxFilled ){` |
|        3 | 4858 | `							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)` |
|      ! 0 | 4859 | `								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 4860 | `									nMaxFilled,nReqNamed,nNVNamed)` |
|        3 | 4861 | `								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        1 | 4862 | `									nMaxFilled,nReqNamed,nNVNamed,TRUE);` |
|        2 | 4863 | `						}else{` |
|        3 | 4864 | `							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        - | 4865 | `						}` |
|        5 | 4866 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        9 | 4867 | `						for( i = 0; i < nActual; i++ ){` |
|        5 | 4868 | `							PH7_MemObjRelease(&pArg[i]);` |
|        3 | 4869 | `						}` |
|        5 | 4870 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 4871 | `							goto Abort;` |
|        - | 4872 | `						}` |
|        5 | 4873 | `						PH7_MemObjRelease(pTos);` |
|        5 | 4874 | `						pTos = &pTos[-nCallArgs];` |
|        5 | 4875 | `						pFrameStack = 0;` |
|        5 | 4876 | `						rc = PH7_EXCEPTION;` |
|        5 | 4877 | `						goto SkipFuncBody;` |
|      102 | 4878 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      102 | 4879 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      102 | 4880 | `						if( pObj ){` |
|      102 | 4881 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|      102 | 4882 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      102 | 4883 | `							sArg.nIdx = pObj->nIdx;` |
|      102 | 4884 | `							sArg.pUserData = 0;` |
|      102 | 4885 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 4886 | `							/* A null default on an implicitly-nullable param must stay null` |
|        - | 4887 | `							 * (see the positional-path note above). */` |
|      100 | 4888 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|       40 | 4889 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0` |
|       22 | 4890 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 4891 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 4892 | `								if( xCast ) xCast(pObj);` |
|      ! 0 | 4893 | `							}` |
|       50 | 4894 | `						}` |
|       50 | 4895 | `					}` |
|        - | 4896 | `				}` |
|      238 | 4897 | `			}` |
|        - | 4898 | `			} /* end nReqNamed scope */` |
|        - | 4899 | `			/* Handle variadic parameter */` |
|      251 | 4900 | `			if( iVariadicIdx >= 0 ){` |
|       47 | 4901 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|       47 | 4902 | `				if( pObj ){` |
|       47 | 4903 | `					PH7_MemObjToHashmap(pObj);` |
|        - | 4904 | `					{` |
|       47 | 4905 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|      447 | 4906 | `						for( i = 0; i < nActual; i++ ){` |
|      401 | 4907 | `							if( aSlot[i] == -1 ){` |
|      417 | 4908 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - | 4909 | `									/* Named variadic entry: insert with string key */` |
|        - | 4910 | `									ph7_value sKey;` |
|       93 | 4911 | `									PH7_MemObjInit(pVm, &sKey);` |
|       93 | 4912 | `									PH7_MemObjStringAppend(&sKey,` |
|       92 | 4913 | `										pCallMap3->aNames[i].zString,` |
|       92 | 4914 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       93 | 4915 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       93 | 4916 | `									PH7_MemObjRelease(&sKey);` |
|       47 | 4917 | `								}else{` |
|        - | 4918 | `									/* Positional variadic entry */` |
|      279 | 4919 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - | 4920 | `								}` |
|      185 | 4921 | `							}` |
|      201 | 4922 | `						}` |
|        - | 4923 | `					}` |
|       47 | 4924 | `					sArg.nIdx = pObj->nIdx;` |
|       47 | 4925 | `					sArg.pUserData = 0;` |
|       47 | 4926 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 | 4927 | `				}` |
|       24 | 4928 | `			}else{` |
|        - | 4929 | `				/* No variadic — preserve unresolved positional overflow` |
|        - | 4930 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - | 4931 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - | 4932 | `				 * the positional-only path's behavior. */` |
|      205 | 4933 | `				sxu32 nAnon = nNonVariadic;` |
|      543 | 4934 | `				for( i = 0; i < nActual; i++ ){` |
|      341 | 4935 | `					if( aSlot[i] == -2 ){` |
|        - | 4936 | `						char zAnonBuf[32];` |
|        - | 4937 | `						SyString sAnonName;` |
|      ! 0 | 4938 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 | 4939 | `							"[%u]apArg",nAnon);` |
|      ! 0 | 4940 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 | 4941 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 | 4942 | `						if( pObj ){` |
|      ! 0 | 4943 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 | 4944 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 | 4945 | `							sArg.pUserData = 0;` |
|      ! 0 | 4946 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 | 4947 | `						}` |
|      ! 0 | 4948 | `						nAnon++;` |
|      ! 0 | 4949 | `					}` |
|      172 | 4950 | `				}` |
|        - | 4951 | `			}` |
|        - | 4952 | `			/* Release all stack arguments */` |
|      989 | 4953 | `			for( i = 0; i < nActual; i++ ){` |
|      741 | 4954 | `				PH7_MemObjRelease(&pArg[i]);` |
|      372 | 4955 | `			}` |
|      251 | 4956 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - | 4957 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|      251 | 4958 | `			n = nFormal;` |
|      127 | 4959 | `		}else{` |
|        - | 4960 | `		/* ============================================================` |
|        - | 4961 | `		 * Positional-only matching path (original)` |
|        - | 4962 | `		 * ============================================================ */` |
|    88523 | 4963 | `		n = 0;` |
|   218435 | 4964 | `		while( pArg < pTos ){` |
|   130211 | 4965 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - | 4966 | `				/* Variadic parameter: collect all remaining args into an array */` |
|      205 | 4967 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      205 | 4968 | `				if( pObj ){` |
|        - | 4969 | `					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,` |
|        - | 4970 | `					 * reallocating pVm->aMemObj and dangling pObj — so don't read pObj->nIdx after` |
|        - | 4971 | `					 * the packing loop (pre-existing UAF, masked by the pool allocator). pMap is a` |
|        - | 4972 | `					 * separately-allocated hashmap and stays valid across the realloc. */` |
|        - | 4973 | `					sxu32 nVariadicIdx;` |
|        - | 4974 | `					/* Initialize as empty array */` |
|      205 | 4975 | `					PH7_MemObjToHashmap(pObj);` |
|      205 | 4976 | `					nVariadicIdx = pObj->nIdx;` |
|        - | 4977 | `					{` |
|      205 | 4978 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     1749 | 4979 | `						while( pArg < pTos ){` |
|        - | 4980 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - | 4981 | `							 *` |
|        - | 4982 | `							 * TODO: PHP reports the runtime element index here` |
|        - | 4983 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - | 4984 | `							 * index (always n+1, the position of the variadic). The` |
|        - | 4985 | `							 * non-union variadic path below has the same limitation;` |
|        - | 4986 | `							 * fixing both wants a separate counter for elements` |
|        - | 4987 | `							 * already packed into the variadic array. */` |
|     1551 | 4988 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 | 4989 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 | 4990 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 | 4991 | `									bCallIsStrict);` |
|       16 | 4992 | `								if( rcU != SXRET_OK ){` |
|        - | 4993 | `									const char *zGiven;` |
|        3 | 4994 | `									const char *zExpected = "union";` |
|        - | 4995 | `									char zBuf[128];` |
|        - | 4996 | `									char zTypeBuf[128];` |
|        3 | 4997 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 4998 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 | 4999 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 5000 | `										zGiven = "null";` |
|      ! 0 | 5001 | `									}else{` |
|        3 | 5002 | `										zGiven = ph7_type_name(pArg);` |
|        - | 5003 | `									}` |
|        3 | 5004 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 | 5005 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 | 5006 | `									}` |
|        4 | 5007 | `									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|        2 | 5008 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 | 5009 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 5010 | `										goto Abort;` |
|        - | 5011 | `									}` |
|        3 | 5012 | `									PH7_MemObjRelease(pTos);` |
|        3 | 5013 | `									pTos = &pTos[-nCallArgs];` |
|        3 | 5014 | `									pFrameStack = 0;` |
|        3 | 5015 | `									rc = PH7_EXCEPTION;` |
|        3 | 5016 | `									goto SkipFuncBody;` |
|        - | 5017 | `								}` |
|       14 | 5018 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 | 5019 | `								pArg++;` |
|       14 | 5020 | `								continue;` |
|        - | 5021 | `							}` |
|        - | 5022 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - | 5023 | `							 * Nullable types (?type) allow null through without coercion. */` |
|     1532 | 5024 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 | 5025 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       44 | 5026 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 | 5027 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - | 5028 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 | 5029 | `									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|      ! 0 | 5030 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 | 5031 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 5032 | `										goto Abort;` |
|        - | 5033 | `									}` |
|        - | 5034 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 | 5035 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 | 5036 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 | 5037 | `									pFrameStack = 0;` |
|      ! 0 | 5038 | `									rc = PH7_EXCEPTION;` |
|      ! 0 | 5039 | `									goto SkipFuncBody;` |
|       13 | 5040 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 5041 | `									char zTypeBuf[128];` |
|      ! 0 | 5042 | `									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|      ! 0 | 5043 | `										&aFormalArg[n].sName,` |
|      ! 0 | 5044 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 | 5045 | `										ph7_type_name(pArg));` |
|      ! 0 | 5046 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 5047 | `										goto Abort;` |
|        - | 5048 | `									}` |
|      ! 0 | 5049 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 | 5050 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 | 5051 | `									pFrameStack = 0;` |
|      ! 0 | 5052 | `									rc = PH7_EXCEPTION;` |
|      ! 0 | 5053 | `									goto SkipFuncBody;` |
|        - | 5054 | `								}` |
|        6 | 5055 | `							}` |
|     1537 | 5056 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|     1537 | 5057 | `							pArg++;` |
|        5 | 5058 | `						}` |
|        - | 5059 | `					}` |
|      203 | 5060 | `					sArg.nIdx = nVariadicIdx; /* pObj may be stale here (aMemObj realloc) — use the saved index */` |
|      203 | 5061 | `					sArg.pUserData = 0;` |
|      203 | 5062 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       99 | 5063 | `				}` |
|      203 | 5064 | `				break; /* All remaining args consumed */` |
|        - | 5065 | `			}` |
|   130011 | 5066 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 5067 | `				/* An explicit null is NOT redirected to the default (PHP applies a` |
|        - | 5068 | `				 * default only for an omitted arg); it falls through to the type check` |
|        - | 5069 | `				 * below — TypeError for a non-nullable typed param, kept as null for a` |
|        - | 5070 | ``				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE`` |
|        - | 5071 | `				 * at compile time so its check accepts null. */` |
|        - | 5072 | `				/* Union type: dispatch to the shared coercion helper. */` |
|   129541 | 5073 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|      128 | 5074 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       82 | 5075 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       41 | 5076 | `						bCallIsStrict);` |
|       87 | 5077 | `					if( rcU != SXRET_OK ){` |
|        - | 5078 | `						const char *zGiven;` |
|       26 | 5079 | `						const char *zExpected = "union";` |
|        - | 5080 | `						char zBuf[128];` |
|        - | 5081 | `						char zTypeBuf[128];` |
|       26 | 5082 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|       13 | 5083 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       21 | 5084 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|       10 | 5085 | `							zGiven = "null";` |
|        6 | 5086 | `						}else{` |
|        6 | 5087 | `							zGiven = ph7_type_name(pArg);` |
|        - | 5088 | `						}` |
|       26 | 5089 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       26 | 5090 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|       11 | 5091 | `						}` |
|       37 | 5092 | `						rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|       22 | 5093 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       26 | 5094 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 5095 | `							goto Abort;` |
|        - | 5096 | `						}` |
|       26 | 5097 | `						PH7_MemObjRelease(pTos);` |
|       26 | 5098 | `						pTos = &pTos[-nCallArgs];` |
|       26 | 5099 | `						pFrameStack = 0;` |
|       26 | 5100 | `						rc = PH7_EXCEPTION;` |
|       26 | 5101 | `						goto SkipFuncBody;` |
|        - | 5102 | `					}` |
|       34 | 5103 | `				}else` |
|        - | 5104 | `				/* Make sure the given arguments are of the correct type.` |
|        - | 5105 | `				 * Nullable types (?type) allow null through without coercion. */` |
|   129454 | 5106 | `				if( aFormalArg[n].nType > 0` |
|    71849 | 5107 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|    14009 | 5108 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - | 5109 | `						/* Argument must be a class instance [i.e: object] */` |
|      415 | 5110 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - | 5111 | `						ph7_class *pClass;` |
|      415 | 5112 | `						int rcPseudo = VmCheckPseudoType(&(*pVm),pArg,pName);` |
|      415 | 5113 | `						if( rcPseudo == 0 ){` |
|        - | 5114 | `							/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - | 5115 | `							char zTypeBuf[128],zGivenBuf[128];` |
|        7 | 5116 | `							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|        4 | 5117 | `								&aFormalArg[n].sName,` |
|        2 | 5118 | `								VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 | 5119 | `								VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf)));` |
|        5 | 5120 | `							if( rc == PH7_ABORT ) goto Abort;` |
|        5 | 5121 | `							PH7_MemObjRelease(pTos);` |
|        5 | 5122 | `							pTos = &pTos[-nCallArgs];` |
|        5 | 5123 | `							pFrameStack = 0;` |
|        5 | 5124 | `							rc = PH7_EXCEPTION;` |
|        5 | 5125 | `							goto SkipFuncBody;` |
|        - | 5126 | `						}` |
|        - | 5127 | `						/* rcPseudo==1 accepts a pseudo-type; -1 real class. Resolve via` |
|        - | 5128 | `						 * VmResolveTypeClass (self/parent + interface/abstract, iLoadable=FALSE)` |
|        - | 5129 | `						 * and throw a catchable TypeError on mismatch — matching PHP — instead of` |
|        - | 5130 | `						 * the legacy warn + NULL-coerce. (Symmetric with the positional path.) */` |
|      411 | 5131 | `						pClass = (rcPseudo == 1) ? 0 : VmResolveTypeClass(&(*pVm),pName,pSelfHint);` |
|      411 | 5132 | `						if( pClass ){` |
|        - | 5133 | `							/* Reaching here means the param is non-nullable (the guard above` |
|        - | 5134 | ``							 * skips nullable+null; a `Type $x = null` default is marked`` |
|        - | 5135 | `							 * implicitly nullable at compile time). So ANY non-object —` |
|        - | 5136 | `							 * including an explicit null — is a TypeError, matching PHP` |
|        - | 5137 | `							 * (&& below short-circuits so instanceof only derefs an object). */` |
|      423 | 5138 | `							int bBad = !((pArg->iFlags & MEMOBJ_OBJ)` |
|      209 | 5139 | `								&& PH7_VmInstanceOf(((ph7_class_instance *)pArg->x.pOther)->pClass,pClass));` |
|      223 | 5140 | `							if( bBad ){` |
|        - | 5141 | `								char zTypeBuf[128],zGivenBuf[128];` |
|       45 | 5142 | `								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|       28 | 5143 | `									&aFormalArg[n].sName,` |
|       28 | 5144 | `									VmSyStringToCStr(&pClass->sName,zTypeBuf,sizeof(zTypeBuf)),` |
|       14 | 5145 | `									VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf)));` |
|       31 | 5146 | `								if( rc == PH7_ABORT ) goto Abort;` |
|       31 | 5147 | `								PH7_MemObjRelease(pTos);` |
|       31 | 5148 | `								pTos = &pTos[-nCallArgs];` |
|       31 | 5149 | `								pFrameStack = 0;` |
|       31 | 5150 | `								rc = PH7_EXCEPTION;` |
|       31 | 5151 | `								goto SkipFuncBody;` |
|        - | 5152 | `							}` |
|      100 | 5153 | `						}` |
|    13788 | 5154 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       67 | 5155 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - | 5156 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 | 5157 | `							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|       10 | 5158 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 | 5159 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 5160 | `								goto Abort;` |
|        - | 5161 | `							}` |
|        - | 5162 | `							/* Skip function body, route through normal cleanup */` |
|       11 | 5163 | `							PH7_MemObjRelease(pTos);` |
|       11 | 5164 | `							pTos = &pTos[-nCallArgs];` |
|       11 | 5165 | `							pFrameStack = 0;` |
|       11 | 5166 | `							rc = PH7_EXCEPTION;` |
|       11 | 5167 | `							goto SkipFuncBody;` |
|       57 | 5168 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 5169 | `							char zTypeBuf[128];` |
|       45 | 5170 | `							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|       28 | 5171 | `								&aFormalArg[n].sName,` |
|       28 | 5172 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|       14 | 5173 | `								ph7_type_name(pArg));` |
|       31 | 5174 | `							if( rc == PH7_ABORT ){` |
|        6 | 5175 | `								goto Abort;` |
|        - | 5176 | `							}` |
|       26 | 5177 | `							PH7_MemObjRelease(pTos);` |
|       26 | 5178 | `							pTos = &pTos[-nCallArgs];` |
|       26 | 5179 | `							pFrameStack = 0;` |
|       26 | 5180 | `							rc = PH7_EXCEPTION;` |
|       26 | 5181 | `							goto SkipFuncBody;` |
|        - | 5182 | `						}` |
|       13 | 5183 | `					}` |
|     6967 | 5184 | `				}` |
|   129449 | 5185 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 5186 | `					/* Pass by reference */` |
|      244 | 5187 | `					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){` |
|        - | 5188 | `						/* php 8.1: $GLOBALS cannot be passed by reference —` |
|        - | 5189 | `						 * a catchable Error with php's exact wording. */` |
|        - | 5190 | `						SyBlob sMsg;` |
|        3 | 5191 | `						SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 5192 | `						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|        2 | 5193 | `							&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        3 | 5194 | `						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 5195 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 5196 | `							goto Abort;` |
|        - | 5197 | `						}` |
|        3 | 5198 | `						PH7_MemObjRelease(pTos);` |
|        3 | 5199 | `						pTos = &pTos[-nCallArgs];` |
|        3 | 5200 | `						pFrameStack = 0;` |
|        3 | 5201 | `						rc = PH7_EXCEPTION;` |
|        3 | 5202 | `						goto SkipFuncBody;` |
|        - | 5203 | `					}` |
|      242 | 5204 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        2 | 5205 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|        3 | 5206 | `						 && (pArg->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){` |
|        - | 5207 | `							/* php: a non-lvalue bound to a by-ref parameter is a catchable Error.` |
|        - | 5208 | `							 * PH7 warned and silently passed by value (same site as the other` |
|        - | 5209 | `							 * binder above). call_user_func()'s deliberate copy is exempt. */` |
|        - | 5210 | `							SyBlob sMsg;` |
|        - | 5211 | `							sxi32 rcRef;` |
|      ! 0 | 5212 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 5213 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 5214 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 5215 | `							rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|      ! 0 | 5216 | `								SyBlobLength(&sMsg));` |
|      ! 0 | 5217 | `							SyBlobRelease(&sMsg);` |
|      ! 0 | 5218 | `							return (rcRef == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 5219 | `						}` |
|        - | 5220 | `						/* Switch to pass by value */` |
|        3 | 5221 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        2 | 5222 | `					}else{` |
|        - | 5223 | `						SyHashEntry *pRefEntry;` |
|        - | 5224 | `						/* Install the referenced variable in the private function frame */` |
|      240 | 5225 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|      240 | 5226 | `						if( pRefEntry == 0 ){` |
|      358 | 5227 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|      236 | 5228 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|      240 | 5229 | `							sArg.nIdx = pArg->nIdx;` |
|      240 | 5230 | `							sArg.pUserData = 0;` |
|      240 | 5231 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      118 | 5232 | `						}` |
|      240 | 5233 | `						pObj = 0;` |
|        - | 5234 | `					}` |
|      123 | 5235 | `				}else{` |
|        - | 5236 | `					/* Pass by value,make a copy of the given argument */` |
|   129209 | 5237 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 5238 | `				}` |
|    64824 | 5239 | `			}else{` |
|        - | 5240 | `				char zName[32];` |
|        - | 5241 | `				SyString sArgName;` |
|        - | 5242 | `				/* Set a dummy name */` |
|      474 | 5243 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      474 | 5244 | `				sArgName.zString = zName;` |
|        - | 5245 | `				/* Annonymous argument */` |
|      474 | 5246 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 5247 | `			}` |
|   129917 | 5248 | `			if( pObj ){` |
|   129681 | 5249 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 5250 | `				/* Insert argument index  */` |
|   129681 | 5251 | `				sArg.nIdx = pObj->nIdx;` |
|   129681 | 5252 | `				sArg.pUserData = 0;` |
|   129681 | 5253 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    64936 | 5254 | `			}` |
|   129917 | 5255 | `			PH7_MemObjRelease(pArg);` |
|   129917 | 5256 | `			pArg++;` |
|   129917 | 5257 | `			++n;` |
|        5 | 5258 | `		}` |
|        - | 5259 | `		} /* end named vs positional branch */` |
|        - | 5260 | `		/* Set up closure environment */` |
|    88675 | 5261 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 5262 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 5263 | `			ph7_value *pValue;` |
|        - | 5264 | `			sxu32 iEnv;` |
|     1729 | 5265 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|     3759 | 5266 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|     2035 | 5267 | `				pEnv = &aEnv[iEnv];` |
|     2035 | 5268 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 5269 | `					/* Do not install null value */` |
|     1669 | 5270 | `					continue;` |
|        - | 5271 | `				}` |
|      366 | 5272 | `				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|        9 | 5273 | `				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|        - | 5274 | `					/* The Closure instance carries an explicit bound $this` |
|        - | 5275 | `					 * (bindTo/bind/call): it wins over the creation-time` |
|        - | 5276 | `					 * captured $this, php-exact. */` |
|        5 | 5277 | `					continue;` |
|        - | 5278 | `				}` |
|      365 | 5279 | `				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|        - | 5280 | `					/* Captured by reference: link the name to the shared slot` |
|        - | 5281 | `					 * (no copy), mirroring the by-ref argument install above. */` |
|      138 | 5282 | `					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|      206 | 5283 | `						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),` |
|      136 | 5284 | `							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|       68 | 5285 | `					}` |
|      138 | 5286 | `					continue;` |
|        - | 5287 | `				}` |
|      228 | 5288 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|      228 | 5289 | `				if( pValue == 0 ){` |
|      ! 0 | 5290 | `					continue;` |
|        - | 5291 | `				}` |
|        - | 5292 | `				/* Invalidate any prior representation */` |
|      228 | 5293 | `				PH7_MemObjRelease(pValue);` |
|        - | 5294 | `				/* Duplicate bound variable value */` |
|      228 | 5295 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|      115 | 5296 | `			}` |
|      862 | 5297 | `		}` |
|        - | 5298 | `		/* Too-few-arguments check, placed AFTER the passed arguments were` |
|        - | 5299 | `		 * installed and type-checked: php's RECV order means a type error on` |
|        - | 5300 | ``		 * a PASSED argument beats the count error (`f(int $x,$y)` called`` |
|        - | 5301 | `		 * f("str") is a TypeError, not ArgumentCountError). The passed args` |
|        - | 5302 | `		 * were already released by the install loop, so the standard throw` |
|        - | 5303 | `		 * exit leaks nothing. The named path never fires this (its per-hole` |
|        - | 5304 | `		 * check ran in-loop; n == nNonVariadic >= nRequired here). Hosted` |
|        - | 5305 | `		 * builtin FUNCTIONS (VM_FUNC_INTERNAL) are exempt — their PHL` |
|        - | 5306 | `		 * signatures don't always mirror php's true arity and their in-body` |
|        - | 5307 | `		 * self-checks own php's wording (stage-2 family); hosted-class` |
|        - | 5308 | `		 * METHODS get php's ZPP wording via VmThrowBuiltinTooFewArgs. */` |
|    88670 | 5309 | `		if( n < SySetUsed(&pVmFunc->aArgs)` |
|    46928 | 5310 | `		 && (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){` |
|        - | 5311 | `			sxu32 nNonVar,nReq;` |
|     3833 | 5312 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     3833 | 5313 | `			if( n < nReq ){` |
|       25 | 5314 | `				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;` |
|       25 | 5315 | `				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|        4 | 5316 | `					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        1 | 5317 | `						nPassed,nReq,nNonVar);` |
|        2 | 5318 | `				}else{` |
|       33 | 5319 | `					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       10 | 5320 | `						nPassed,nReq,nNonVar,TRUE);` |
|        - | 5321 | `				}` |
|       25 | 5322 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 5323 | `					goto Abort;` |
|        - | 5324 | `				}` |
|       25 | 5325 | `				PH7_MemObjRelease(pTos);` |
|       25 | 5326 | `				pTos = &pTos[-nCallArgs];` |
|       25 | 5327 | `				pFrameStack = 0;` |
|       25 | 5328 | `				rc = PH7_EXCEPTION;` |
|       25 | 5329 | `				goto SkipFuncBody;` |
|        - | 5330 | `			}` |
|     1903 | 5331 | `		}` |
|        - | 5332 | `		/* Process default values for remaining formal parameters */` |
|    96519 | 5333 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     8115 | 5334 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 5335 | `				/* Variadic parameter with no extra args — create empty array */` |
|      249 | 5336 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      249 | 5337 | `				if( pObj ){` |
|      249 | 5338 | `					PH7_MemObjToHashmap(pObj);` |
|      249 | 5339 | `					sArg.nIdx = pObj->nIdx;` |
|      249 | 5340 | `					sArg.pUserData = 0;` |
|      249 | 5341 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      122 | 5342 | `				}` |
|      249 | 5343 | `				n++;` |
|      249 | 5344 | `				break; /* Variadic is always last */` |
|        - | 5345 | `			}` |
|     7871 | 5346 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     7869 | 5347 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     7869 | 5348 | `				if( pObj ){` |
|        - | 5349 | `					/* Evaluate the default value and extract it's result */` |
|     7869 | 5350 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|     7869 | 5351 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 5352 | `						goto Abort;` |
|        - | 5353 | `					}` |
|        - | 5354 | `					/* Insert argument index */` |
|     7869 | 5355 | `					sArg.nIdx = pObj->nIdx;` |
|     7869 | 5356 | `					sArg.pUserData = 0;` |
|     7869 | 5357 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 5358 | `					/* Make sure the default argument is of the correct type.` |
|        - | 5359 | ``					 * A null default on an implicitly-nullable param (`int $x = null`)`` |
|        - | 5360 | `					 * must stay null — casting it to 0/""/false would diverge from PHP` |
|        - | 5361 | `					 * and contradict the explicit-null path, which now keeps it null. */` |
|     7864 | 5362 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     3846 | 5363 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)` |
|     1929 | 5364 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 5365 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 5366 | `						/* Cast to the desired type */` |
|      ! 0 | 5367 | `						xCast(pObj);` |
|      ! 0 | 5368 | `					}` |
|     3932 | 5369 | `				}` |
|     3932 | 5370 | `			}` |
|     7871 | 5371 | `			++n;` |
|        5 | 5372 | `		}` |
|        - | 5373 | `		} /* end VmCallArgMap scope */` |
|        - | 5374 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 5375 | `		 * does not return anything.` |
|        - | 5376 | `		 */` |
|    88653 | 5377 | `		PH7_MemObjRelease(pTos);` |
|    88653 | 5378 | `		pTos = &pTos[-nCallArgs];` |
|        - | 5379 | `		/* Allocate an operand stack (via the recycling allocator) and evaluate the` |
|        - | 5380 | `		 * function body. Size it to a tight static bound when the body is statically` |
|        - | 5381 | `		 * modelable (BYTECODE.md stage 7) — the big memory win for deep recursion,` |
|        - | 5382 | `		 * where one such stack lives per frame — falling back to the safe` |
|        - | 5383 | `		 * instruction-count bound otherwise.` |
|        - | 5384 | `		 *` |
|        - | 5385 | `		 * The bound is computed LAZILY on the first call and cached on the func` |
|        - | 5386 | `		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:` |
|        - | 5387 | `		 * self-computing on first use is fail-safe against any body-creation path` |
|        - | 5388 | `		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),` |
|        - | 5389 | `		 * where undersizing is a heap overflow; the amortized cost is one analysis` |
|        - | 5390 | `		 * per function. */` |
|        - | 5391 | `		{` |
|    88653 | 5392 | `			sxu32 nSlots = pVmFunc->nMaxStack;` |
|    88653 | 5393 | `			if( nSlots == 0 ){` |
|     7629 | 5394 | `				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);` |
|    11441 | 5395 | `				sxu32 nTight = VmComputeMaxStack(&(*pVm),` |
|     7624 | 5396 | `					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);` |
|     7629 | 5397 | `				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;` |
|     7629 | 5398 | `				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */` |
|     7629 | 5399 | `				pVmFunc->nMaxStack = nSlots;` |
|     3812 | 5400 | `			}` |
|    88653 | 5401 | `			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);` |
|        - | 5402 | `		}` |
|    88653 | 5403 | `		if( pFrameStack == 0 ){` |
|        - | 5404 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 5405 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 5406 | `				&pVmFunc->sName);` |
|      ! 0 | 5407 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 5408 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 5409 | `			}` |
|      ! 0 | 5410 | `			break;` |
|        - | 5411 | `		}` |
|    44275 | 5412 | `SkipFuncBody:` |
|    88781 | 5413 | `		if( pSelf ){` |
|        - | 5414 | `			/* Push class name */` |
|    24927 | 5415 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|    12461 | 5416 | `		}` |
|        - | 5417 | `		/* Increment nesting level */` |
|    88781 | 5418 | `		pVm->nRecursionDepth++;` |
|    88781 | 5419 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 5420 | `			/* Arg-binding threw: there is no body to run — finish the call` |
|        - | 5421 | `			 * immediately (no record is pushed). */` |
|        - | 5422 | `			VmCallRecord sCallee;` |
|      133 | 5423 | `			sCallee.pVmFunc = pVmFunc;` |
|      133 | 5424 | `			sCallee.pFrame = pFrame;` |
|      133 | 5425 | `			sCallee.pFrameStack = pFrameStack;` |
|      133 | 5426 | `			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|      133 | 5427 | `			sCallee.nLastRef = SXU32_HIGH;` |
|      133 | 5428 | `			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|      133 | 5429 | `			sState.pTos = pTos;` |
|      133 | 5430 | `			sState.pc = pc;` |
|      133 | 5431 | `			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);` |
|      133 | 5432 | `			pTos = sState.pTos;` |
|      133 | 5433 | `			pc = sState.pc;` |
|      133 | 5434 | `			if( rc == PH7_ABORT ){` |
|        - | 5435 | `				/* Abort processing immeditaley */` |
|      ! 0 | 5436 | `				goto Abort;` |
|      133 | 5437 | `			}else if( rc == PH7_SUSPEND ){` |
|      ! 0 | 5438 | `				goto Suspend;` |
|      133 | 5439 | `			}else if( rc == PH7_EXCEPTION ){` |
|       31 | 5440 | `				goto Exception;` |
|        - | 5441 | `			}` |
|       54 | 5442 | `		}else{` |
|        - | 5443 | `			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a` |
|        - | 5444 | `			 * call record (caller activation + in-flight call) and switch the` |
|        - | 5445 | `			 * loop's locals to the callee — a PHP->PHP call no longer grows` |
|        - | 5446 | `			 * the native stack. The record node is pool-allocated so` |
|        - | 5447 | `			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */` |
|    88653 | 5448 | `			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;` |
|    88653 | 5449 | `			if( pRec ){` |
|    86045 | 5450 | `				pVm->pIdleCallFrames = (void *)pRec->pPrev;` |
|    43074 | 5451 | `			}else{` |
|     2613 | 5452 | `				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));` |
|        - | 5453 | `			}` |
|    88653 | 5454 | `			if( pRec == 0 ){` |
|        - | 5455 | `				/* OOM: undo the push-time accounting, tear the call down and` |
|        - | 5456 | `				 * raise the non-catchable fatal (the §3.1 OOM convention —` |
|        - | 5457 | `				 * never a silent NULL). */` |
|      ! 0 | 5458 | `				pVm->nRecursionDepth--;` |
|      ! 0 | 5459 | `				if( pSelf ){` |
|      ! 0 | 5460 | `					(void)SySetPop(&pVm->aSelf);` |
|      ! 0 | 5461 | `				}` |
|      ! 0 | 5462 | `				SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|      ! 0 | 5463 | `				VmLeaveFrame(&(*pVm));` |
|      ! 0 | 5464 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 5465 | `				goto Abort;` |
|        - | 5466 | `			}` |
|    88653 | 5467 | `			sState.pTos = pTos;` |
|    88653 | 5468 | `			sState.pc = pc;` |
|    88653 | 5469 | `			pRec->sCaller = sState;` |
|    88653 | 5470 | `			pRec->sCall.pVmFunc = pVmFunc;` |
|    88653 | 5471 | `			pRec->sCall.pFrame = pFrame;` |
|    88653 | 5472 | `			pRec->sCall.pFrameStack = pFrameStack;` |
|    88653 | 5473 | `			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|    88653 | 5474 | `			pRec->sCall.nLastRef = SXU32_HIGH;` |
|    88653 | 5475 | `			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|    88653 | 5476 | `			pRec->pPrev = pCallTop;` |
|    88653 | 5477 | `			pCallTop = pRec;` |
|        - | 5478 | `			/* Switch to the callee activation (what the recursive` |
|        - | 5479 | `			 * VmByteCodeExec entry used to set up). */` |
|    88653 | 5480 | `			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);` |
|    88653 | 5481 | `			pStack = pFrameStack;` |
|    88653 | 5482 | `			pTos = &pStack[-1];` |
|    88653 | 5483 | `			pc = 0;` |
|    88653 | 5484 | `			sState.aInstr = aInstr;` |
|    88653 | 5485 | `			sState.pStack = pStack;` |
|    88653 | 5486 | `			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */` |
|    88653 | 5487 | `			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */` |
|    88653 | 5488 | `			sState.pTos = pTos;` |
|    88653 | 5489 | `			sState.pc = 0;` |
|    88653 | 5490 | `			sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|    88653 | 5491 | `			sState.pEntryFrame = pVm->pFrame;` |
|    88653 | 5492 | `			sState.pResult = pRec->sCaller.pTos;` |
|    88653 | 5493 | `			sState.pLastRef = &pRec->sCall.nLastRef;` |
|    88653 | 5494 | `			sState.pEnforceRetFunc = VmFuncHasReturnType(pVmFunc) ? pVmFunc : 0;` |
|    88653 | 5495 | `			sState.is_callback = 0;` |
|    88653 | 5496 | `			sState.bReturnPropagates = 0;` |
|    88653 | 5497 | `			goto VmLoopFetch;` |
|        - | 5498 | `		}` |
|       54 | 5499 | `	}else{` |
|        - | 5500 | `		ph7_user_func *pFunc;` |
|        - | 5501 | `		ph7_context sCtx;` |
|        - | 5502 | `		ph7_value sRet;` |
|        - | 5503 | `		/* Look for an installed foreign function.` |
|        - | 5504 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 5505 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 5506 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 5507 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   940857 | 5508 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 5509 | `		{` |
|   940857 | 5510 | `		VmCallArgMap *pCallMap2 = pEffCallMap;` |
|   940857 | 5511 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 5512 | `			/* Compiler-qualified: try short name as global fallback */` |
|       32 | 5513 | `			const char *zShort = sName.zString;` |
|        - | 5514 | `			sxu32 i;` |
|      518 | 5515 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      490 | 5516 | `				if( sName.zString[i] == '\\' ){` |
|       46 | 5517 | `					zShort = &sName.zString[i + 1];` |
|       21 | 5518 | `				}` |
|      247 | 5519 | `			}` |
|       32 | 5520 | `			if( zShort != sName.zString ){` |
|       32 | 5521 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       32 | 5522 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       14 | 5523 | `			}` |
|       14 | 5524 | `		}` |
|        - | 5525 | `		} /* end VmCallArgMap namespace scope */` |
|   940857 | 5526 | `		if( pEntry == 0 ){` |
|        - | 5527 | `			/* php accepts the "Class::method" STATIC-callable string everywhere a` |
|        - | 5528 | `			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).` |
|        - | 5529 | `			 * Split on the first "::" and route through the shared array-callable` |
|        - | 5530 | `			 * machinery ([class-name, method-name]) instead of warning undefined. */` |
|        - | 5531 | `			sxu32 iSep;` |
|       19 | 5532 | `			int bScoped = 0;` |
|      159 | 5533 | `			for( iSep = 1 ; iSep + 2 < sName.nByte ; ++iSep ){` |
|      155 | 5534 | `				if( sName.zString[iSep] == ':' && sName.zString[iSep+1] == ':' ){` |
|       13 | 5535 | `					bScoped = 1;` |
|       13 | 5536 | `					break;` |
|        - | 5537 | `				}` |
|       73 | 5538 | `			}` |
|       19 | 5539 | `			if( bScoped ){` |
|       13 | 5540 | `				ph7_hashmap *pCbMap = PH7_NewHashmap(&(*pVm),0,0);` |
|       13 | 5541 | `				if( pCbMap ){` |
|        - | 5542 | `					ph7_value sCallable,sElem,sResult;` |
|        - | 5543 | `					sxi32 rcSm;` |
|       19 | 5544 | `					pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|       12 | 5545 | `						nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|       13 | 5546 | `					SySetReset(&aArg);` |
|       31 | 5547 | `					while( pArg < pTos ){` |
|       19 | 5548 | `						SySetPut(&aArg,(const void *)&pArg);` |
|       19 | 5549 | `						pArg++;` |
|        1 | 5550 | `					}` |
|       13 | 5551 | `					PH7_MemObjInit(pVm,&sElem);` |
|       13 | 5552 | `					PH7_MemObjStringAppend(&sElem,sName.zString,iSep);` |
|       13 | 5553 | `					PH7_HashmapInsert(pCbMap,0,&sElem);` |
|       13 | 5554 | `					PH7_MemObjRelease(&sElem);` |
|       13 | 5555 | `					PH7_MemObjInit(pVm,&sElem);` |
|       13 | 5556 | `					PH7_MemObjStringAppend(&sElem,&sName.zString[iSep+2],sName.nByte-(iSep+2));` |
|       13 | 5557 | `					PH7_HashmapInsert(pCbMap,0,&sElem);` |
|       13 | 5558 | `					PH7_MemObjRelease(&sElem);` |
|       13 | 5559 | `					PH7_MemObjInit(pVm,&sCallable);` |
|       13 | 5560 | `					sCallable.x.pOther = pCbMap;` |
|       13 | 5561 | `					MemObjSetType(&sCallable,MEMOBJ_HASHMAP);` |
|       13 | 5562 | `					PH7_MemObjInit(pVm,&sResult);` |
|       19 | 5563 | `					rcSm = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,(int)SySetUsed(&aArg),` |
|       12 | 5564 | `						(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|       13 | 5565 | `					SySetReset(&aArg);` |
|       13 | 5566 | `					PH7_MemObjRelease(&sCallable);` |
|       13 | 5567 | `					if( nCallArgs > 0 ){` |
|       13 | 5568 | `						VmPopOperand(&pTos,nCallArgs);` |
|        6 | 5569 | `					}` |
|       13 | 5570 | `					if( rcSm == PH7_ABORT ){` |
|      ! 0 | 5571 | `						PH7_MemObjRelease(&sResult);` |
|      ! 0 | 5572 | `						goto Abort;` |
|        - | 5573 | `					}` |
|       13 | 5574 | `					if( rcSm == PH7_EXCEPTION ){` |
|        - | 5575 | `						sxi32 iResumePc;` |
|      ! 0 | 5576 | `						PH7_MemObjRelease(&sResult);` |
|      ! 0 | 5577 | `						if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 5578 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 5579 | `							pc = iResumePc;` |
|      ! 0 | 5580 | `							break;` |
|        - | 5581 | `						}` |
|      ! 0 | 5582 | `						goto Exception;` |
|        - | 5583 | `					}` |
|       13 | 5584 | `					PH7_MemObjStore(&sResult,pTos);` |
|       13 | 5585 | `					PH7_MemObjRelease(&sResult);` |
|       13 | 5586 | `					break;` |
|        - | 5587 | `				}` |
|      ! 0 | 5588 | `			}` |
|        - | 5589 | `			/* Call to an undefined function is a catchable Error in php 8 — it does` |
|        - | 5590 | `			 * NOT warn and hand back null and carry on, which is what PH7 did (and` |
|        - | 5591 | `			 * which quietly turned a typo into a null-propagating program). */` |
|        - | 5592 | `			{` |
|        - | 5593 | `			SyBlob sMsg;` |
|        6 | 5594 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        6 | 5595 | `			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);` |
|        - | 5596 | `			/* Consume this call's captured spread runs so they don't leak into a` |
|        - | 5597 | `			 * later call (this path never reaches VmBuildEffectiveArgMap). */` |
|        6 | 5598 | `			if( pInstr->iP2 ){` |
|      ! 0 | 5599 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 5600 | `			}` |
|        - | 5601 | `			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the` |
|        - | 5602 | `			 * compile-time arg count on the stack, and this early exit skips the` |
|        - | 5603 | `			 * arg-building loop that the normal path uses, so popping only iP1 would` |
|        - | 5604 | `			 * strand the expanded elements and corrupt the enclosing expression. */` |
|        6 | 5605 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 5606 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 5607 | `			}` |
|        6 | 5608 | `			PH7_MemObjRelease(pTos);` |
|        8 | 5609 | `			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|        2 | 5610 | `				SyBlobLength(&sMsg));` |
|        6 | 5611 | `			SyBlobRelease(&sMsg);` |
|        6 | 5612 | `			if( rc == SXERR_ABORT ){` |
|        6 | 5613 | `				goto Abort;` |
|        - | 5614 | `			}` |
|      ! 0 | 5615 | `			goto Exception;` |
|        - | 5616 | `			}` |
|        - | 5617 | `		}` |
|   940841 | 5618 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 5619 | `		/* Host function (builtin): build the effective spread-key map so the` |
|        - | 5620 | `		 * name-forwarding builtins (call_user_func & friends) relay string keys as` |
|        - | 5621 | `		 * named args, and — critically — so this call's captured runs are consumed.` |
|        - | 5622 | `		 * pArg is the top base here (a builtin call pops no method-name slot). */` |
|  1411582 | 5623 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   940836 | 5624 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 5625 | `		/* Start collecting function arguments */` |
|   940841 | 5626 | `		SySetReset(&aArg);` |
|  2519437 | 5627 | `		while( pArg < pTos ){` |
|  1578601 | 5628 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1578601 | 5629 | `			pArg++;` |
|        5 | 5630 | `		}` |
|        - | 5631 | `		/* Assume a null return value */` |
|   940841 | 5632 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 5633 | `		/* Init the call context */` |
|   940841 | 5634 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 5635 | `		/* Hand the call-site named-argument map to the builtin so name-forwarding` |
|        - | 5636 | `		 * helpers (call_user_func & friends) can relay name: arguments — and the` |
|        - | 5637 | `		 * caller's strict_types mode — to the inner callback. Forwarded whole (not` |
|        - | 5638 | `		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even` |
|        - | 5639 | `		 * when its own call site is purely positional; only the two forwarding` |
|        - | 5640 | `		 * builtins read pArgMap, so this is inert for every other host function. */` |
|   940841 | 5641 | `		sCtx.pArgMap = pEffCallMap;` |
|        - | 5642 | `		{` |
|   940841 | 5643 | `		int nGiven = (int)SySetUsed(&aArg);` |
|        - | 5644 | `		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum` |
|        - | 5645 | `		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError` |
|        - | 5646 | `		 * before the C routine runs when called with too few arguments — instead` |
|        - | 5647 | `		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""` |
|        - | 5648 | `		 * return. The message wording matches php's ZPP output byte-for-byte. */` |
|   940841 | 5649 | `		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){` |
|      707 | 5650 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 5651 | `				"%z() expects %s %d argument%s, %d given",` |
|      234 | 5652 | `				&pFunc->sName,` |
|      468 | 5653 | `				pFunc->bAtLeast ? "at least" : "exactly",` |
|      468 | 5654 | `				(int)pFunc->nMinArg,` |
|      468 | 5655 | `				pFunc->nMinArg == 1 ? "" : "s",` |
|      234 | 5656 | `				nGiven);` |
|  1411114 | 5657 | `		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,` |
|   940368 | 5658 | `			(ph7_value **)SySetBasePtr(&aArg))) ){` |
|        - | 5659 | `			/* TypeError thrown: rc carries the caught/uncaught status */` |
|       90 | 5660 | `		}else{` |
|        - | 5661 | `			/* Call the foreign function */` |
|   940203 | 5662 | `			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));` |
|        - | 5663 | `		}` |
|        - | 5664 | `		}` |
|        - | 5665 | `		/* Release the call context */` |
|   940841 | 5666 | `		VmReleaseCallContext(&sCtx);` |
|   940841 | 5667 | `		if( rc == PH7_ABORT ){` |
|        - | 5668 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 5669 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 5670 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      576 | 5671 | `			PH7_MemObjRelease(&sRet);` |
|      576 | 5672 | `			goto Abort;` |
|        - | 5673 | `		}` |
|   940269 | 5674 | `		if( rc != PH7_SUSPEND && pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 5675 | `			/* A throw raised inside this host function — directly` |
|        - | 5676 | `			 * (PH7_VmThrowException) or by a PHP callback it invoked — was` |
|        - | 5677 | `			 * caught by an INLINE try (generator body) THIS exec owns.` |
|        - | 5678 | `			 * VmThrowInline records only a pc-redirect: a direct builtin throw` |
|        - | 5679 | `			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION` |
|        - | 5680 | `			 * with the redirect pending, so the rc branches below never land` |
|        - | 5681 | `			 * it (pre-existing hole: explode("") or a throwing usort` |
|        - | 5682 | `			 * comparator inside a generator's try lost the catch AND the` |
|        - | 5683 | `			 * yield). Land at the redirect now — its drain to the try's` |
|        - | 5684 | `			 * operand base subsumes the args + name pops. */` |
|        5 | 5685 | `			PH7_MemObjRelease(&sRet);` |
|       17 | 5686 | `			PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 5687 | `		}` |
|   940265 | 5688 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 5689 | `			/* A callback invoked by this host function threw. If an in-place catch` |
|        - | 5690 | `			 * recorded a resume target owned by THIS body, resume at its landing pad` |
|        - | 5691 | `			 * (consuming the target); otherwise the exception was caught by an outer` |
|        - | 5692 | `			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW` |
|        - | 5693 | `			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which` |
|        - | 5694 | `			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */` |
|        - | 5695 | `			sxi32 iResumePc;` |
|      969 | 5696 | `			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 5697 | `				/* Caught by an outer exec, or not caught here: propagate. */` |
|      201 | 5698 | `				goto Exception;` |
|        - | 5699 | `			}` |
|        - | 5700 | `			/* Exception was caught in place by THIS body's try: pop args and the` |
|        - | 5701 | `			 * result slot to restore the pre-try stack, then resume. */` |
|      773 | 5702 | `			PH7_MemObjRelease(&sRet);` |
|      773 | 5703 | `			if( nCallArgs > 0 ){` |
|      485 | 5704 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      240 | 5705 | `			}` |
|      773 | 5706 | `			VmPopOperand(&pTos,1);` |
|      773 | 5707 | `			pc = iResumePc;` |
|      773 | 5708 | `			break;` |
|        - | 5709 | `		}` |
|   939301 | 5710 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 5711 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 5712 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 5713 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 5714 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 5715 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 5716 | `			 * body), the user-function path above will handle re-saving. */` |
|      359 | 5717 | `			PH7_MemObjRelease(&sRet);` |
|      359 | 5718 | `			if( nCallArgs > 0 ){` |
|      359 | 5719 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      177 | 5720 | `			}` |
|        - | 5721 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 5722 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|      359 | 5723 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|      359 | 5724 | `			goto Suspend;` |
|        - | 5725 | `		}` |
|   938947 | 5726 | `		if( nCallArgs > 0 ){` |
|        - | 5727 | `			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack` |
|        - | 5728 | `			 * expanded the compile-time arg count on the stack, so popping iP1` |
|        - | 5729 | `			 * strands the extra elements (or, for an unpack that expanded to fewer` |
|        - | 5730 | `			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,` |
|        - | 5731 | `			 * reading a bogus value whose stray flags sent MemObjStore into the` |
|        - | 5732 | `			 * hashmap-release path and hung). Mirrors every other CALL exit. The` |
|        - | 5733 | `			 * function-name slot (pTos) receives the return value below. */` |
|   917873 | 5734 | `			VmPopOperand(&pTos,nCallArgs);` |
|   459257 | 5735 | `		}` |
|        - | 5736 | `		/* Save foreign function return value into the (now top) function-name slot */` |
|   938947 | 5737 | `		PH7_MemObjStore(&sRet,pTos);` |
|   938947 | 5738 | `		PH7_MemObjRelease(&sRet);` |
|        - | 5739 | `	}` |
|   939045 | 5740 | `	break;` |
|        - | 5741 | `				  }` |
|        - | 5742 | `/*` |
|        - | 5743 | ` * OP_CONSUME: P1 * *` |
|        - | 5744 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 5745 | ` */` |
|    25441 | 5746 | `case PH7_OP_CONSUME: {` |
|        - | 5747 | `	VmOpRc rcOp;` |
|    50887 | 5748 | `	sState.pTos = pTos;` |
|    50887 | 5749 | `	sState.pc = pc;` |
|    50887 | 5750 | `	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);` |
|    50887 | 5751 | `	pTos = sState.pTos;` |
|    50887 | 5752 | `	pc = sState.pc;` |
|    50887 | 5753 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5754 | `		goto Abort;` |
|    50887 | 5755 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5756 | `		goto Exception;` |
|        - | 5757 | `	}` |
|    50882 | 5758 | `	break;` |
|        - | 5759 | `					  }` |
|        - | 5760 |  |
|        - | 5761 | `		} /* Switch() */` |
| 16578834 | 5762 | `		pc++; /* Next instruction in the stream */` |
|        5 | 5763 | `	} /* For(;;) */` |
|   120381 | 5764 | `Done:` |
|        - | 5765 | `	/* A stacked callee completing lands here too (its result is already in` |
|        - | 5766 | `	 * sState.pResult — the caller's operand slot); Unwind's first iteration` |
|        - | 5767 | `	 * bottoms out identically for the record-less case. */` |
|   240865 | 5768 | `	rc = SXRET_OK;` |
|   240865 | 5769 | `	goto Unwind;` |
|      823 | 5770 | `Suspend:` |
|     1651 | 5771 | `	rc = PH7_SUSPEND;` |
|     1651 | 5772 | `	if( pCallTop != 0 ){` |
|        - | 5773 | `		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment` |
|        - | 5774 | `		 * instead of the lossy unwind. pc/nTos of the innermost activation were` |
|        - | 5775 | `		 * already saved into the ctx by VmSuspendCtx; capture the rest (the` |
|        - | 5776 | `		 * record chain, the innermost activation, the suspend-time top frame)` |
|        - | 5777 | `		 * so resume re-enters HERE, inside the innermost callee, like php. The` |
|        - | 5778 | `		 * records / frames / operand stacks stay alive — nothing is freed. Only` |
|        - | 5779 | `		 * fibers reach this (generators yield only at their body level, pCallTop` |
|        - | 5780 | `		 * == 0); a suspend inside a C->PHP callback was already rejected with a` |
|        - | 5781 | `		 * FiberError before it could arrive here. */` |
|      359 | 5782 | `		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));` |
|      359 | 5783 | `		if( pSeg == 0 ){` |
|        - | 5784 | `			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST` |
|        - | 5785 | `			 * pc/nTos into the ctx, so the lossy body-level fallback would resume` |
|        - | 5786 | `			 * the callee's pc against the body stack — silent corruption, exactly` |
|        - | 5787 | `			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the` |
|        - | 5788 | `			 * §3.1 convention shared with the stage-2 record-alloc OOM site). */` |
|      ! 0 | 5789 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 5790 | `			rc = PH7_ABORT;` |
|      ! 0 | 5791 | `			goto Unwind;` |
|        - | 5792 | `		}` |
|      359 | 5793 | `		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */` |
|      359 | 5794 | `		pSeg->sState = sState;` |
|      359 | 5795 | `		pSeg->pCallTop = pCallTop;` |
|      359 | 5796 | `		pSeg->pTopFrame = pVm->pFrame;` |
|      359 | 5797 | `		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;` |
|        - | 5798 | `		{` |
|        - | 5799 | `			VmCallFrame *pRec;` |
|      359 | 5800 | `			pSeg->nRecords = 0;` |
|     1015 | 5801 | `			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){` |
|      661 | 5802 | `				pSeg->nRecords++;` |
|      333 | 5803 | `			}` |
|        - | 5804 | `		}` |
|      359 | 5805 | `		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;` |
|        - | 5806 | `		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */` |
|      359 | 5807 | `		SySetRelease(&aArg);` |
|      359 | 5808 | `		return PH7_SUSPEND;` |
|        - | 5809 | `	}` |
|     1297 | 5810 | `	goto Unwind;` |
|      372 | 5811 | `Abort:` |
|      748 | 5812 | `	rc = PH7_ABORT;` |
|      748 | 5813 | `	goto Unwind;` |
|      394 | 5814 | `Exception:` |
|      793 | 5815 | `	rc = PH7_EXCEPTION;` |
|      788 | 5816 | `	goto Unwind;` |
|   121793 | 5817 | `Unwind:` |
|        - | 5818 | `	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration` |
|        - | 5819 | `	 * finishes the top record exactly as the old per-level native return did:` |
|        - | 5820 | `	 * for ABORT/EXCEPTION, first run what the popped activation's own` |
|        - | 5821 | `	 * Abort/Exception label used to do (clear its pending return, release its` |
|        - | 5822 | `	 * operands — a stacked activation never has bReturnPropagates set), then` |
|        - | 5823 | `	 * VmCallFinish routes in the restored caller (an in-place catch there` |
|        - | 5824 | `	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —` |
|        - | 5825 | `	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible` |
|        - | 5826 | `	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).` |
|        - | 5827 | `	 * At the bottom, VmExecFinalize hands the status to the native caller. */` |
|   122272 | 5828 | `	for(;;){` |
|   244451 | 5829 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|        - | 5830 | `			/* Drop any pending hook-RMW write-backs this activation armed — its` |
|        - | 5831 | `			 * statement is abandoned (only the innermost activation at throw time` |
|        - | 5832 | `			 * can own entries: the armed window spans exactly one instruction, so` |
|        - | 5833 | `			 * no OP_CALL record ever intervenes). */` |
|     2298 | 5834 | `			while( SySetUsed(&pVm->aHookRmw) > 0` |
|     2299 | 5835 | `			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){` |
|      ! 0 | 5836 | `				VmHookRmwDropTop(&(*pVm));` |
|      ! 0 | 5837 | `			}` |
|     1147 | 5838 | `		}` |
|   244451 | 5839 | `		if( pCallTop == 0 ){` |
|   156213 | 5840 | `			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);` |
|        - | 5841 | `		}` |
|    88243 | 5842 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|     1253 | 5843 | `			VmClearFrameReturn(sState.pEntryFrame);` |
|     2541 | 5844 | `			while( pTos >= pStack ){` |
|     1293 | 5845 | `				PH7_MemObjRelease(pTos);` |
|     1293 | 5846 | `				pTos--;` |
|        5 | 5847 | `			}` |
|      624 | 5848 | `		}` |
|        - | 5849 | `		{` |
|    88243 | 5850 | `			VmCallFrame *pRec = pCallTop;` |
|    88243 | 5851 | `			sState = pRec->sCaller;` |
|    88243 | 5852 | `			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);` |
|    88243 | 5853 | `			pCallTop = pRec->pPrev;` |
|    88243 | 5854 | `			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;` |
|    88243 | 5855 | `			pVm->pIdleCallFrames = (void *)pRec;` |
|    88243 | 5856 | `			aInstr = sState.aInstr;` |
|    88243 | 5857 | `			pStack = sState.pStack;` |
|    88243 | 5858 | `			pTos = sState.pTos;` |
|    88243 | 5859 | `			pc = sState.pc;` |
|        - | 5860 | `		}` |
|    88243 | 5861 | `		if( rc == PH7_OK ){` |
|    87481 | 5862 | `			pc++; /* the loop-bottom increment this OP_CALL missed */` |
|    87481 | 5863 | `			goto VmLoopFetch;` |
|        - | 5864 | `		}` |
|        5 | 5865 | `	}` |
|    78286 | 5866 | `}` |
|        - | 5867 |  |
