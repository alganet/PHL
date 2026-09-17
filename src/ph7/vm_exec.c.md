# src/ph7/vm_exec.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2637/3152 lines (83.66%)

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
|      264 |   45 | `static int VmGrowOperandStack(ph7_vm *pVm, sxu32 nNeed,` |
|        - |   46 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |   47 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        4 |   48 | `{` |
|      268 |   49 | `	ph7_value *pOld = *ppStack;` |
|      268 |   50 | `	sxu32 nOldCap = pState->nStackCap;` |
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
|      268 |   64 | `	nReq = nNeed + pState->nStackOrig;` |
|      268 |   65 | `	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */` |
|      ! 0 |   66 | `		nReq = SXU32_HIGH;` |
|      ! 0 |   67 | `	}` |
|        - |   68 | `	/* SyMemBackendRealloc's size argument is sxu32, so the byte count` |
|        - |   69 | `	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack` |
|        - |   70 | `	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and` |
|        - |   71 | `	 * the init loop below would run off it. If the true requirement exceeds the` |
|        - |   72 | `	 * representable cap, treat it as OOM (the caller raises the guard error). */` |
|      268 |   73 | `	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);` |
|      268 |   74 | `	if( nReq > nMaxCap ){` |
|      ! 0 |   75 | `		return 0;` |
|        - |   76 | `	}` |
|      268 |   77 | `	if( nReq <= nOldCap ){` |
|      161 |   78 | `		return 1; /* already fits — no growth needed */` |
|        - |   79 | `	}` |
|      110 |   80 | `	nNewCap = nReq;` |
|        - |   81 | `	/* Amortize repeated spreads in one argument list: at least double, but never` |
|        - |   82 | `	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't` |
|        - |   83 | `	 * itself overflow.) */` |
|      110 |   84 | `	if( nNewCap < nOldCap * 2 ){` |
|      110 |   85 | `		sxu32 nDbl = nOldCap * 2;` |
|      110 |   86 | `		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }` |
|      110 |   87 | `		if( nNewCap < nDbl ){ nNewCap = nDbl; }` |
|       53 |   88 | `	}` |
|      163 |   89 | `	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,` |
|       53 |   90 | `		nNewCap * sizeof(ph7_value));` |
|      110 |   91 | `	if( pNew == 0 ){` |
|      ! 0 |   92 | `		return 0; /* OOM: caller keeps pOld and raises the guard error */` |
|        - |   93 | `	}` |
|        - |   94 | `	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */` |
|     6818 |   95 | `	for( i = nOldCap; i < nNewCap; i++ ){` |
|     6712 |   96 | `		PH7_MemObjInit(pVm, &pNew[i]);` |
|     6712 |   97 | `		pNew[i].nIdx = SXU32_HIGH;` |
|     3358 |   98 | `	}` |
|        - |   99 | `	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */` |
|      110 |  100 | `	*ppTos = pNew + (*ppTos - pOld);` |
|      110 |  101 | `	*ppStack = pNew;` |
|      110 |  102 | `	pState->pStack = pNew + (pState->pStack - pOld);` |
|      110 |  103 | `	pState->pTos = pNew + (pState->pTos - pOld);` |
|      110 |  104 | `	pState->nStackCap = nNewCap;` |
|      110 |  105 | `	if( pCallTop ){` |
|        - |  106 | `		/* This activation is a trampoline callee: its record owns the buffer. */` |
|       80 |  107 | `		pCallTop->sCall.pFrameStack = pNew;` |
|       80 |  108 | `		pCallTop->sCall.nStackCap = nNewCap;` |
|       41 |  109 | `	}else{` |
|        - |  110 | `		/* Base activation: the native entry frees *ppBaseOwner (and, for a` |
|        - |  111 | `		 * resumable coroutine, persists *pnBaseCap across suspend/resume). */` |
|       31 |  112 | `		if( ppBaseOwner ){ *ppBaseOwner = pNew; }` |
|       31 |  113 | `		if( pnBaseCap ){ *pnBaseCap = nNewCap; }` |
|        - |  114 | `	}` |
|        - |  115 | `	/* Re-anchor this activation's captured spread runs (pStart into the old` |
|        - |  116 | `	 * buffer). Enclosing-activation runs live in other buffers — leave them. */` |
|      110 |  117 | `	nRun = SySetUsed(&pVm->aSpreadRun);` |
|      110 |  118 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      110 |  119 | `	for( i = 0; i < nRun; i++ ){` |
|      ! 0 |  120 | `		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){` |
|      ! 0 |  121 | `			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);` |
|      ! 0 |  122 | `		}` |
|      ! 0 |  123 | `	}` |
|      110 |  124 | `	return 1;` |
|      136 |  125 | `}` |
|        - |  126 | `/*` |
|        - |  127 | ` * Ensure the running activation's operand stack can hold a spread of nEntry` |
|        - |  128 | ` * elements pushed at *ppTos (the source slot becomes the first element, so the` |
|        - |  129 | ` * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.` |
|        - |  130 | ` * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.` |
|        - |  131 | ` */` |
|      300 |  132 | `static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,` |
|        - |  133 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |  134 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        4 |  135 | `{` |
|        - |  136 | `	sxu32 nNeed;` |
|      304 |  137 | `	if( nEntry == 0 ){` |
|       39 |  138 | `		return 1; /* empty spread never grows the stack */` |
|        - |  139 | `	}` |
|      268 |  140 | `	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);` |
|      400 |  141 | `	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,` |
|      132 |  142 | `		pCallTop, ppBaseOwner, pnBaseCap);` |
|      154 |  143 | `}` |
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
|   160774 |  163 | `static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)` |
|        5 |  164 | `{` |
|    80387 |  165 | `	SXUNUSED(pVm);` |
|   160779 |  166 | `	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){` |
|   157077 |  167 | `		VmClearFrameReturn(pState->pEntryFrame);` |
|    78536 |  168 | `	}` |
|   160779 |  169 | `	SySetRelease(pArg);` |
|   160779 |  170 | `	if( rcTerm == PH7_ABORT \|\| rcTerm == PH7_EXCEPTION ){` |
|     2841 |  171 | `		while( pTos >= pState->pStack ){` |
|     1789 |  172 | `			PH7_MemObjRelease(pTos);` |
|     1789 |  173 | `			pTos--;` |
|        5 |  174 | `		}` |
|      526 |  175 | `	}` |
|   160779 |  176 | `	return rcTerm;` |
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
|   105897 |  190 | `static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)` |
|        5 |  191 | `{` |
|        - |  192 | `	ph7_value *pObj;` |
|        - |  193 | `	/* Decrement nesting level */` |
|   105902 |  194 | `	pVm->nRecursionDepth--;` |
|   105902 |  195 | `	if( pCallee->bSelfPushed ){` |
|        - |  196 | `		/* Pop class name */` |
|    25533 |  197 | `		(void)SySetPop(&pVm->aSelf);` |
|    12764 |  198 | `	}` |
|   105902 |  199 | `	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
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
|   105902 |  226 | `	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  227 | `		/* The callee threw (or its finally threw past it). If an in-place catch` |
|        - |  228 | `		 * recorded a resume target owned by THIS caller's body, resume there and` |
|        - |  229 | `		 * consume the target (VmRecordedResume); when the catcher is an outer exec` |
|        - |  230 | `		 * — or this is a callback with no bytecode to resume into — propagate so` |
|        - |  231 | `		 * the owning exec lands. This replaces the old "is the caller's parent a` |
|        - |  232 | `		 * resumable try frame" test, which resumed at the caller's OWN try even` |
|        - |  233 | `		 * when the finally's throw was caught further out, losing that catch's` |
|        - |  234 | `		 * return (ROOT B, face c). */` |
|        - |  235 | `		sxi32 iResumePc;` |
|     1103 |  236 | `		VmFrame *pParentFrame = pCallee->pFrame->pParent;` |
|     1103 |  237 | `		if( !pCaller->is_callback && pVm->pInlineInstr == (void *)pCaller->aInstr ){` |
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
|     1102 |  248 | `		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){` |
|        - |  249 | `			/* Pop the result */` |
|      633 |  250 | `			VmPopOperand(&pCaller->pTos,1);` |
|      633 |  251 | `			pCaller->pc = iResumePc;` |
|      633 |  252 | `			rc = PH7_OK;` |
|      319 |  253 | `		}else{` |
|      473 |  254 | `			if( pParentFrame->pParent ){` |
|      469 |  255 | `				rc = PH7_EXCEPTION;` |
|      237 |  256 | `			}else{` |
|        - |  257 | `				/* Continue normal execution */` |
|        6 |  258 | `				rc = PH7_OK;` |
|        - |  259 | `			}` |
|        - |  260 | `		}` |
|      549 |  261 | `	}` |
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
|   105902 |  272 | `	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){` |
|        - |  273 | `		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee` |
|        - |  274 | `		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly` |
|        - |  275 | `		 * the allocated slot count either way. */` |
|   105768 |  276 | `		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap);` |
|    52986 |  277 | `	}` |
|        - |  278 | `	/* Leave the frame */` |
|   105902 |  279 | `	VmLeaveFrame(&(*pVm));` |
|   105902 |  280 | `	if( rc == PH7_ABORT ){` |
|      336 |  281 | `		return PH7_ABORT;` |
|        - |  282 | `	}` |
|   105570 |  283 | `	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  284 | `		/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  285 | `		 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  286 | `		 * overwriting the state saved by the inner level.` |
|        - |  287 | `		 * pTos points to the result slot (not yet written).` |
|        - |  288 | `		 * Save nTos one below so resume pushes at the result slot. */` |
|      ! 0 |  289 | `		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);` |
|      ! 0 |  290 | `		return PH7_SUSPEND;` |
|        - |  291 | `	}` |
|   105570 |  292 | `	if( rc == PH7_EXCEPTION ){` |
|      469 |  293 | `		return PH7_EXCEPTION;` |
|        - |  294 | `	}` |
|   105106 |  295 | `	return PH7_OK;` |
|    53058 |  296 | `}` |
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
|   161132 |  332 | `PH7_PRIVATE sxi32 VmByteCodeExec(` |
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
|   161137 |  352 | `	if( VmNativeNestingExceeded(pVm) ){` |
|        6 |  353 | `		return VmNativeNestingFatal(pVm);` |
|        - |  354 | `	}` |
|        - |  355 | `	/* A fresh native exec entered while a C-boundary throw is parked` |
|        - |  356 | `	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the` |
|        - |  357 | `	 * very opcode that swallowed the throw) must run CLEAN: the parked status` |
|        - |  358 | `	 * belongs to the interrupted outer exec's fetch-point router, not to this` |
|        - |  359 | `	 * one. Save+clear on entry, merge back on exit — the outer status is` |
|        - |  360 | `	 * restored unless this exec parked its own unconsumed (newer) one, with` |
|        - |  361 | `	 * PH7_ABORT dominating either way. */` |
|   161133 |  362 | `	nSavedBrc = pVm->nBoundaryRc;` |
|   161133 |  363 | `	pVm->nBoundaryRc = 0;` |
|        - |  364 | `	/* The executing source line belongs to the ACTIVATION. A nested body -- a called` |
|        - |  365 | `	 * function, but equally an attribute-default or default-argument mini-program --` |
|        - |  366 | `	 * runs its own bytecode with its own lines, so it must not leave the caller` |
|        - |  367 | ``	 * reporting the callee's position: `new Exception` stamped line 1 because the`` |
|        - |  368 | ``	 * class's `protected $message = '';` default ran (from the embedded chunk) between`` |
|        - |  369 | `	 * OP_NEW and the stamp. Save on entry, restore on exit. */` |
|   161133 |  370 | `	nSavedLine = pVm->nCurLine;` |
|   161133 |  371 | `	pVm->nVmExecDepth++;` |
|   241697 |  372 | `	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,` |
|    80564 |  373 | `		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);` |
|   161133 |  374 | `	pVm->nVmExecDepth--;` |
|   161133 |  375 | `	pVm->nCurLine = nSavedLine;` |
|   161133 |  376 | `	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT \|\| pVm->nBoundaryRc == 0) ){` |
|       15 |  377 | `		pVm->nBoundaryRc = nSavedBrc;` |
|        6 |  378 | `	}` |
|   161133 |  379 | `	return rc;` |
|    80571 |  380 | `}` |
|   161128 |  381 | `static sxi32 VmByteCodeExecBody(` |
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
|   161133 |  401 | `	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record` |
|        - |  402 | `	                            * stack (BYTECODE stage 2); NULL = executing the` |
|        - |  403 | `	                            * bottom activation. */` |
|        - |  404 | `	VmExecState sState; /* This activation's boundary state (BYTECODE.md stage 1):` |
|        - |  405 | `	                     * everything a suspended/nested activation must restore.` |
|        - |  406 | `	                     * pc/pTos stay in locals for the hot loop and are synced` |
|        - |  407 | `	                     * into sState only around the call epilogue (stage 2 turns` |
|        - |  408 | `	                     * that boundary into an explicit record push/pop). */` |
|        - |  409 | `	sxi32 pc;` |
|        - |  410 | `	sxi32 rc;` |
|   161133 |  411 | `	sState.aInstr = aInstr;` |
|   161133 |  412 | `	sState.pStack = pStack;` |
|   161133 |  413 | `	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */` |
|   161133 |  414 | `	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */` |
|   161133 |  415 | `	sState.pResult = pResult;` |
|   161133 |  416 | `	sState.pLastRef = pLastRef;` |
|   161133 |  417 | `	sState.pEnforceRetFunc = pEnforceRetFunc;` |
|   161133 |  418 | `	sState.is_callback = (sxu8)(is_callback ? 1 : 0);` |
|   161133 |  419 | `	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);` |
|        - |  420 | `	/* Argument container */` |
|   161133 |  421 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|   161133 |  422 | `	if( nTos < 0 ){` |
|   145319 |  423 | `		pTos = &pStack[-1];` |
|    72662 |  424 | `	}else{` |
|    15819 |  425 | `		pTos = &pStack[nTos];` |
|        - |  426 | `	}` |
|   161133 |  427 | `	sState.pTos = pTos;` |
|   161133 |  428 | `	sState.pc = nPc;` |
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
|   161128 |  446 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame` |
|     2816 |  447 | `	 && pStack == pVm->pActiveCtx->pStack ){` |
|     1805 |  448 | `		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;` |
|      905 |  449 | `	}else{` |
|   159333 |  450 | `		sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|        - |  451 | `	}` |
|   161133 |  452 | `	sState.pEntryFrame = pVm->pFrame;` |
|   161133 |  453 | `	pc = nPc;` |
|        - |  454 | `	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call` |
|        - |  455 | `	 * adopts its parked record segment here. VmResumeCtx hands the segment in` |
|        - |  456 | `	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a` |
|        - |  457 | `	 * nested mini-program/callback run inside the resumed body — after it rebased` |
|        - |  458 | `	 * the segment's exception floors and pushed the resume value into the innermost` |
|        - |  459 | `	 * stack. Locals switch to the innermost activation so the dispatch loop` |
|        - |  460 | `	 * continues inside the callee; the record chain is restored so its completion` |
|        - |  461 | `	 * unwinds back through the body. */` |
|   161133 |  462 | `	if( pAdoptSegment ){` |
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
|   161128 |  506 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected` |
|     1557 |  507 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
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
|       35 |  539 | `		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
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
|       13 |  556 | `			goto Exception;` |
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
|   161118 |  567 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing` |
|     1542 |  568 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
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
|  8511257 |  588 | `	for(;;){` |
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
| 17221282 |  603 | `		if( pVm->nBoundaryRc != 0 \|\| SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      762 |  604 | `			if( pVm->nBoundaryRc != 0 ){` |
|      194 |  605 | `				sxi32 rcBr = pVm->nBoundaryRc;` |
|      194 |  606 | `				pVm->nBoundaryRc = 0;` |
|      194 |  607 | `				if( rcBr == PH7_ABORT ){` |
|       68 |  608 | `					goto Abort;` |
|        - |  609 | `				}` |
|      127 |  610 | `				if( pVm->pInlineInstr == (void *)aInstr ){` |
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
|      125 |  621 | `					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){` |
|        - |  622 | `						/* Caught in place by a try THIS exec owns: drain the abandoned` |
|        - |  623 | `						 * operands to the catching try's base and land at its pad` |
|        - |  624 | `						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;` |
|        - |  625 | `						 * pre-fetch here, so +1 lands on the pad itself). */` |
|      172 |  626 | `						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|       84 |  627 | `							PH7_MemObjRelease(pTos);` |
|       84 |  628 | `							pTos--;` |
|        2 |  629 | `						}` |
|       90 |  630 | `						pc = iBrPc + 1;` |
|       46 |  631 | `					}else{` |
|        - |  632 | `						/* Caught by an outer exec (or uncaught-with-report pending):` |
|        - |  633 | `						 * unwind out of this exec; the owner's macros/router land it. */` |
|       37 |  634 | `						goto Exception;` |
|        - |  635 | `					}` |
|        - |  636 | `				}` |
|       45 |  637 | `			}` |
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
|      668 |  648 | `			while( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      569 |  649 | `				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      568 |  650 | `				if( pTopRmw->pOwnerStack != (void *)pStack \|\| pTopRmw->pInstrs != (void *)aInstr` |
|       89 |  651 | `				 \|\| ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){` |
|      281 |  652 | `					break; /* not ours, or legitimately in flight */` |
|        - |  653 | `				}` |
|        9 |  654 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 |  655 | `			}` |
|      329 |  656 | `		}` |
|        - |  657 | `		/* Fetch the instruction to execute */` |
| 17221182 |  658 | `		pInstr = &aInstr[pc];` |
| 17221182 |  659 | `		if( pInstr->nLine ){` |
|        - |  660 | `			/* Publish the source position for diagnostics, debug_backtrace() and` |
|        - |  661 | `			 * Throwable. Instructions the compiler could not attribute (nLine 0)` |
|        - |  662 | `			 * leave the last known line standing rather than reporting line 0. */` |
| 17009523 |  663 | `			pVm->nCurLine = pInstr->nLine;` |
|  8511230 |  664 | `		}` |
| 17221182 |  665 | `		rc = SXRET_OK;` |
|        - |  666 | `/*` |
|        - |  667 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  668 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  669 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  670 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  671 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  672 | ` */` |
| 17221182 |  673 | `		switch(pInstr->iOp){` |
|        - |  674 | `/*` |
|        - |  675 | ` * DONE: P1 * *` |
|        - |  676 | ` *` |
|        - |  677 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  678 | ` * and return immediately.` |
|        - |  679 | ` */` |
|   131256 |  680 | `case PH7_OP_DONE:` |
|   262726 |  681 | `	if( pInstr->iP2 && sState.bReturnPropagates ){` |
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
|   262537 |  711 | `	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)` |
|     9213 |  712 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  713 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  714 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  715 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  716 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  717 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  718 | `		 * exception. */` |
|     9211 |  719 | `		ph7_value *pRetVal = 0;` |
|     9211 |  720 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|     9095 |  721 | `			pRetVal = pTos;` |
|     4545 |  722 | `		}` |
|     9211 |  723 | `		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);` |
|     9211 |  724 | `		if( rc == PH7_ABORT ) goto Abort;` |
|     9205 |  725 | `		if( rc == PH7_EXCEPTION ){` |
|       21 |  726 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|       19 |  727 | `				PH7_MemObjRelease(pTos);` |
|       19 |  728 | `				pTos--;` |
|        8 |  729 | `			}` |
|       21 |  730 | `			goto Exception;` |
|        - |  731 | `		}` |
|        - |  732 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  733 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  734 | `		 * defensively we clear the pointer after a successful check). */` |
|     9187 |  735 | `		sState.pEnforceRetFunc = 0;` |
|     4591 |  736 | `	}` |
|   262518 |  737 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|   158348 |  738 | `		if( sState.pLastRef ){` |
|    95384 |  739 | `			*sState.pLastRef = pTos->nIdx;` |
|    47794 |  740 | `		}` |
|   158348 |  741 | `		if( sState.pResult ){` |
|        - |  742 | `			/* Execution result */` |
|   151260 |  743 | `			PH7_MemObjStore(pTos,sState.pResult);` |
|    75732 |  744 | `		}` |
|   158348 |  745 | `		VmPopOperand(&pTos,1);` |
|   183451 |  746 | `	}else if( sState.pLastRef ){` |
|        - |  747 | `		/* Nothing referenced — also the throw-unwind path: the compiler routes` |
|        - |  748 | `		 * an uncaught exception to this terminal OP_DONE with iP1 set but an` |
|        - |  749 | `		 * empty operand stack (pTos == pStack-1), so there is no return value to` |
|        - |  750 | `		 * store. Guarding on pTos >= pStack (matching the two sibling branches` |
|        - |  751 | `		 * above) avoids the below-base read that crashed under glibc/ASan. */` |
|     8965 |  752 | `		*sState.pLastRef = SXU32_HIGH;` |
|     4480 |  753 | `	}` |
|        - |  754 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  755 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  756 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  757 | `	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering` |
|        - |  758 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  759 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  760 | `	 * block can override it (the finally writes this body frame's sRet slot,` |
|        - |  761 | `	 * materialized below).` |
|        - |  762 | `	 */` |
|   262518 |  763 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|   262518 |  764 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  765 | `		goto Abort;` |
|        - |  766 | `	}` |
|   262518 |  767 | `	if( rc == PH7_EXCEPTION ){` |
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
|   262514 |  779 | `	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){` |
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
|   262514 |  791 | `	goto Done;` |
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
|   281884 |  836 | `case PH7_OP_JMP:` |
|   563982 |  837 | `	pc = pInstr->iP2 - 1;` |
|   563982 |  838 | `	break;` |
|        - |  839 | `/*` |
|        - |  840 | ` * JZ: P1 P2 *` |
|        - |  841 | ` *` |
|        - |  842 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  843 | ` * entry in the stack if P1 is zero.` |
|        - |  844 | ` */` |
|   824714 |  845 | `case PH7_OP_JZ:` |
|        - |  846 | `#ifdef UNTRUST` |
|        - |  847 | `	if( pTos < pStack ){` |
|        - |  848 | `		goto Abort;` |
|        - |  849 | `	}` |
|        - |  850 | `#endif` |
|        - |  851 | `	/* Get a boolean value */` |
|  1650592 |  852 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     1003 |  853 | `		PH7_MemObjToBool(pTos);` |
|      500 |  854 | `	}` |
|  1650592 |  855 | `	if( !pTos->x.iVal ){` |
|        - |  856 | `		/* Take the jump */` |
|   936735 |  857 | `		pc = pInstr->iP2 - 1;` |
|   468631 |  858 | `	}` |
|  1650592 |  859 | `	if( !pInstr->iP1 ){` |
|  1407390 |  860 | `		VmPopOperand(&pTos,1);` |
|   704272 |  861 | `	}` |
|  1650592 |  862 | `	break;` |
|        - |  863 | `/*` |
|        - |  864 | ` * JNZ: P1 P2 *` |
|        - |  865 | ` *` |
|        - |  866 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  867 | ` * entry in the stack if P1 is zero.` |
|        - |  868 | ` */` |
|    94283 |  869 | `case PH7_OP_JNZ:` |
|        - |  870 | `#ifdef UNTRUST` |
|        - |  871 | `	if( pTos < pStack ){` |
|        - |  872 | `		goto Abort;` |
|        - |  873 | `	}` |
|        - |  874 | `#endif` |
|        - |  875 | `	/* Get a boolean value */` |
|   188780 |  876 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  877 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  878 | `	}` |
|   188780 |  879 | `	if( pTos->x.iVal ){` |
|        - |  880 | `		/* Take the jump */` |
|     9573 |  881 | `		pc = pInstr->iP2 - 1;` |
|     4784 |  882 | `	}` |
|   188780 |  883 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  884 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  885 | `	}` |
|   188780 |  886 | `	break;` |
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
|   689319 |  900 | `case PH7_OP_POP: {` |
|  1379688 |  901 | `	sxi32 n = pInstr->iP1;` |
|  1379688 |  902 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  903 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       79 |  904 | `		n = (sxi32)(pTos - pStack);` |
|       38 |  905 | `	}` |
|  1379688 |  906 | `	VmPopOperand(&pTos,n);` |
|  1379688 |  907 | `	break;` |
|        - |  908 | `				 }` |
|        - |  909 | `/*` |
|        - |  910 | ` * DUP: * * *` |
|        - |  911 | ` *` |
|        - |  912 | ` * Duplicate the top of the stack.` |
|        - |  913 | ` */` |
|       62 |  914 | `case PH7_OP_DUP:` |
|        - |  915 | `#ifdef UNTRUST` |
|        - |  916 | `	if( pTos < pStack ){` |
|        - |  917 | `		goto Abort;` |
|        - |  918 | `	}` |
|        - |  919 | `#endif` |
|      127 |  920 | `	pTos++;` |
|      127 |  921 | `	PH7_MemObjInit(pVm,pTos);` |
|      127 |  922 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|      127 |  923 | `	break;` |
|        - |  924 | `/*` |
|        - |  925 | ` * NSSWITCH: * * P3` |
|        - |  926 | ` *` |
|        - |  927 | ` * Switch the active namespace at runtime.` |
|        - |  928 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  929 | ` */` |
|    48771 |  930 | `case PH7_OP_NSSWITCH:` |
|    97547 |  931 | `	SyBlobReset(&pVm->sNamespace);` |
|    97547 |  932 | `	if( pInstr->p3 ){` |
|     3985 |  933 | `		const char *zNs = (const char *)pInstr->p3;` |
|     3985 |  934 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|     1990 |  935 | `	}` |
|        - |  936 | `	/* Clear namespace-scoped use-const imports */` |
|    97547 |  937 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    97547 |  938 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    97547 |  939 | `	break;` |
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
|     1003 |  957 | `case PH7_OP_CVT_INT:` |
|        - |  958 | `#ifdef UNTRUST` |
|        - |  959 | `	if( pTos < pStack ){` |
|        - |  960 | `		goto Abort;` |
|        - |  961 | `	}` |
|        - |  962 | `#endif` |
|     2011 |  963 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      911 |  964 | `		PH7_MemObjToInteger(pTos);` |
|      453 |  965 | `	}` |
|        - |  966 | `	/* Invalidate any prior representation */` |
|     2011 |  967 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|     2011 |  968 | `	break;` |
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
|       45 |  981 | `		PH7_MemObjToReal(pTos);` |
|       21 |  982 | `	}` |
|        - |  983 | `	/* Invalidate any prior representation */` |
|       79 |  984 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       79 |  985 | `	break;` |
|        - |  986 | `/*` |
|        - |  987 | ` * CVT_STR: * * *` |
|        - |  988 | ` *` |
|        - |  989 | ` * Force the top of the stack to be a string.` |
|        - |  990 | ` */` |
|     2839 |  991 | `case PH7_OP_CVT_STR:` |
|        - |  992 | `#ifdef UNTRUST` |
|        - |  993 | `	if( pTos < pStack ){` |
|        - |  994 | `		goto Abort;` |
|        - |  995 | `	}` |
|        - |  996 | `#endif` |
|     5683 |  997 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     2665 |  998 | `		PH7_MemObjToString(pTos);` |
|     1330 |  999 | `	}` |
|     5683 | 1000 | `	break;` |
|        - | 1001 | `/*` |
|        - | 1002 | ` * CVT_BOOL: * * *` |
|        - | 1003 | ` *` |
|        - | 1004 | ` * Force the top of the stack to be a boolean.` |
|        - | 1005 | ` */` |
|       64 | 1006 | `case PH7_OP_CVT_BOOL:` |
|        - | 1007 | `#ifdef UNTRUST` |
|        - | 1008 | `	if( pTos < pStack ){` |
|        - | 1009 | `		goto Abort;` |
|        - | 1010 | `	}` |
|        - | 1011 | `#endif` |
|      129 | 1012 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       37 | 1013 | `		PH7_MemObjToBool(pTos);` |
|       18 | 1014 | `	}` |
|      129 | 1015 | `	break;` |
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
|       17 | 1042 | `case PH7_OP_CVT_ARRAY:` |
|        - | 1043 | `#ifdef UNTRUST` |
|        - | 1044 | `	if( pTos < pStack ){` |
|        - | 1045 | `		goto Abort;` |
|        - | 1046 | `	}` |
|        - | 1047 | `#endif` |
|        - | 1048 | `	/* Force a hashmap cast */` |
|       36 | 1049 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       36 | 1050 | `	if( rc != SXRET_OK ){` |
|        - | 1051 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 | 1052 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - | 1053 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 | 1054 | `	}` |
|       36 | 1055 | `	break;` |
|        - | 1056 | `/*` |
|        - | 1057 | ` * CVT_OBJ: * * *` |
|        - | 1058 | ` *` |
|        - | 1059 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - | 1060 | ` */` |
|       18 | 1061 | `case PH7_OP_CVT_OBJ:` |
|        - | 1062 | `#ifdef UNTRUST` |
|        - | 1063 | `	if( pTos < pStack ){` |
|        - | 1064 | `		goto Abort;` |
|        - | 1065 | `	}` |
|        - | 1066 | `#endif` |
|       38 | 1067 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1068 | `		/* Force a 'stdClass()' cast */` |
|       38 | 1069 | `		PH7_MemObjToObject(pTos);` |
|       18 | 1070 | `	}` |
|       38 | 1071 | `	break;` |
|        - | 1072 | `/*` |
|        - | 1073 | ` * ERR_CTRL * * *` |
|        - | 1074 | ` *` |
|        - | 1075 | ` * Error control operator.` |
|        - | 1076 | ` */` |
|     3381 | 1077 | `case PH7_OP_UNSET_VAR: {` |
|        - | 1078 | `	VmOpRc rcOp;` |
|     6767 | 1079 | `	sState.pTos = pTos;` |
|     6767 | 1080 | `	sState.pc = pc;` |
|     6767 | 1081 | `	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);` |
|     6767 | 1082 | `	pTos = sState.pTos;` |
|     6767 | 1083 | `	pc = sState.pc;` |
|     6767 | 1084 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 1085 | `		goto Abort;` |
|     6765 | 1086 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1087 | `		goto Exception;` |
|        - | 1088 | `	}` |
|     6765 | 1089 | `	break;` |
|        - | 1090 | `					  }` |
|    33215 | 1091 | `case PH7_OP_ERR_CTRL:` |
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
|    66435 | 1102 | `	if( pInstr->iP1 ){` |
|    33221 | 1103 | `		pVm->nErrSuppress++;` |
|    49827 | 1104 | `	}else if( pVm->nErrSuppress > 0 ){` |
|    33219 | 1105 | `		pVm->nErrSuppress--;` |
|    16607 | 1106 | `	}` |
|    66435 | 1107 | `	break;` |
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
|  1563385 | 1164 | `case PH7_OP_LOADC: {` |
|        - | 1165 | `	ph7_value *pObj;` |
|        - | 1166 | `	/* Reserve a room */` |
|  3128675 | 1167 | `	pTos++;` |
|  3128675 | 1168 | `	if( pInstr->iP1 & PH7_LOADC_NOKEY ){` |
|        - | 1169 | `		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */` |
|    19509 | 1170 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19509 | 1171 | `		SyBlobReset(&pTos->sBlob);` |
|    19509 | 1172 | `		pTos->iFlags \|= MEMOBJ_AUX_NOKEY;` |
|    19509 | 1173 | `		pTos->nIdx = SXU32_HIGH;` |
|    19509 | 1174 | `		break;` |
|        - | 1175 | `	}` |
|  3109171 | 1176 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  3109171 | 1177 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - | 1178 | `			SyHashEntry *pEntry;` |
|        - | 1179 | `			/* Check use const imports first — imports take precedence */` |
|        - | 1180 | `			{` |
|        - | 1181 | `				SyHashEntry *pConstImport;` |
|    34913 | 1182 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    23272 | 1183 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    23277 | 1184 | `				if( pConstImport ){` |
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
|    23267 | 1199 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    23267 | 1200 | `			if( pEntry ){` |
|    23257 | 1201 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - | 1202 | `				/* Set a NULL default value */` |
|    23257 | 1203 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    23257 | 1204 | `				SyBlobReset(&pTos->sBlob);` |
|        - | 1205 | `				/* Invoke the callback and deal with the expanded value */` |
|    23257 | 1206 | `				VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|        - | 1207 | `				/* Mark as constant */` |
|    23257 | 1208 | `				pTos->nIdx = SXU32_HIGH;` |
|    23257 | 1209 | `				break;` |
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
|  3085899 | 1267 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1543902 | 1268 | `	}else{` |
|        - | 1269 | `		/* Set a NULL value */` |
|      ! 0 | 1270 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 1271 | `	}` |
|        - | 1272 | `	/* Mark as constant */` |
|  3085899 | 1273 | `	pTos->nIdx = SXU32_HIGH;` |
|  3085899 | 1274 | `	break;` |
|        - | 1275 | `				  }` |
|        - | 1276 | `/*` |
|        - | 1277 | ` * LOAD: P1 * P3` |
|        - | 1278 | ` *` |
|        - | 1279 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - | 1280 | ` * from the P3 operand.` |
|        - | 1281 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - | 1282 | ` * the variable if non existent and push the NULL constant instead.` |
|        - | 1283 | ` */` |
|  2247345 | 1284 | `case PH7_OP_LOAD:{` |
|        - | 1285 | `	ph7_value *pObj;` |
|        - | 1286 | `	SyString sName;` |
|  4498685 | 1287 | `	if( pInstr->p3 == 0 ){` |
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
|  4498669 | 1300 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 1301 | `		/* Reserve a room for the target object */` |
|  4498669 | 1302 | `		pTos++;` |
|        - | 1303 | `	}` |
|  4498685 | 1304 | `	if( pInstr->iP2 == 2 ){` |
|        - | 1305 | ``		/* Read-modify-write target (`$x++`, `$x .= 'a'`): php reads the variable`` |
|        - | 1306 | `		 * before writing, so it warns when it does not exist and THEN seeds it.` |
|        - | 1307 | `		 * Peek first (no create) purely to raise that warning; the load below` |
|        - | 1308 | ``		 * still creates the slot the operator needs. A plain `=` never gets here`` |
|        - | 1309 | `		 * — it writes without reading, and stays silent, as php does. */` |
|   404956 | 1310 | `		if( VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE) == 0 ){` |
|        7 | 1311 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        3 | 1312 | `		}` |
|   202580 | 1313 | `	}` |
|        - | 1314 | `	/* Extract the requested memory object */` |
|  4498685 | 1315 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  4498685 | 1316 | `	if( pObj == 0 ){` |
|       65 | 1317 | `		if( pInstr->iP1 ){` |
|        - | 1318 | `			/* Reading a variable that does not exist. php raises E_WARNING` |
|        - | 1319 | `			 * "Undefined variable $x" and evaluates it as NULL; PHL used to` |
|        - | 1320 | `			 * yield NULL silently, which hid typo'd names. iP2 marks the reads` |
|        - | 1321 | `			 * that must stay quiet (isset/empty — see PH7_CompileVariable);` |
|        - | 1322 | `			 * vivifying contexts never get here because they pass iP1 = 0. */` |
|       65 | 1323 | `			if( pInstr->iP2 == 0 ){` |
|        9 | 1324 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        4 | 1325 | `			}` |
|        - | 1326 | `			/* Variable not found,load NULL */` |
|       65 | 1327 | `			if( !pInstr->p3 ){` |
|      ! 0 | 1328 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 | 1329 | `			}else{` |
|       65 | 1330 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 1331 | `			}` |
|       65 | 1332 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  2247379 | 1333 | `			break;` |
|      ! 0 | 1334 | `		}else{` |
|        - | 1335 | `			/* Fatal error */` |
|      ! 0 | 1336 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 1337 | `			goto Abort;` |
|        - | 1338 | `		}` |
|        - | 1339 | `	}` |
|        - | 1340 | `	/* Load variable contents */` |
|  4498623 | 1341 | `	PH7_MemObjLoad(pObj,pTos);` |
|  4498623 | 1342 | `	pTos->nIdx = pObj->nIdx;` |
|  4498623 | 1343 | `	break;` |
|        - | 1344 | `				   }` |
|        - | 1345 | `/*` |
|        - | 1346 | ` * LOAD_MAP P1 * *` |
|        - | 1347 | ` *` |
|        - | 1348 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - | 1349 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - | 1350 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - | 1351 | ` */` |
|    36928 | 1352 | `case PH7_OP_LOAD_MAP: {` |
|        - | 1353 | `	VmOpRc rcOp;` |
|    73861 | 1354 | `	sState.pTos = pTos;` |
|    73861 | 1355 | `	sState.pc = pc;` |
|    73861 | 1356 | `	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);` |
|    73861 | 1357 | `	pTos = sState.pTos;` |
|    73861 | 1358 | `	pc = sState.pc;` |
|    73861 | 1359 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1360 | `		goto Abort;` |
|    73861 | 1361 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       15 | 1362 | `		goto Exception;` |
|        - | 1363 | `	}` |
|    73847 | 1364 | `	break;` |
|        - | 1365 | `					  }` |
|        - | 1366 | `/*` |
|        - | 1367 | ` * LOAD_LIST: P1 * *` |
|        - | 1368 | ` *` |
|        - | 1369 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - | 1370 | ` * This is the VM implementation of the list() PHP construct.` |
|        - | 1371 | ` * Caveats:` |
|        - | 1372 | ` *  This implementation support only a single nesting level.` |
|        - | 1373 | ` */` |
|      133 | 1374 | `case PH7_OP_LOAD_LIST: {` |
|        - | 1375 | `	VmOpRc rcOp;` |
|      271 | 1376 | `	sState.pTos = pTos;` |
|      271 | 1377 | `	sState.pc = pc;` |
|      271 | 1378 | `	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);` |
|      271 | 1379 | `	pTos = sState.pTos;` |
|      271 | 1380 | `	pc = sState.pc;` |
|      271 | 1381 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1382 | `		goto Abort;` |
|      271 | 1383 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1384 | `		goto Exception;` |
|        - | 1385 | `	}` |
|      271 | 1386 | `	break;` |
|        - | 1387 | `					  }` |
|        - | 1388 | `/*` |
|        - | 1389 | ` * LOAD_IDX: P1 P2 *` |
|        - | 1390 | ` *` |
|        - | 1391 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - | 1392 | ` * from the stack.` |
|        - | 1393 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - | 1394 | ` * instead.` |
|        - | 1395 | ` */` |
|   339247 | 1396 | `case PH7_OP_LOAD_IDX: {` |
|        - | 1397 | `	VmOpRc rcOp;` |
|   679240 | 1398 | `	sState.pTos = pTos;` |
|   679240 | 1399 | `	sState.pc = pc;` |
|   679240 | 1400 | `	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);` |
|   679240 | 1401 | `	pTos = sState.pTos;` |
|   679240 | 1402 | `	pc = sState.pc;` |
|   679240 | 1403 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1404 | `		goto Abort;` |
|   679240 | 1405 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1406 | `		goto Exception;` |
|        - | 1407 | `	}` |
|   679240 | 1408 | `	break;` |
|        - | 1409 | `					  }` |
|        - | 1410 | `/*` |
|        - | 1411 | ` * LOAD_CLOSURE * * P3` |
|        - | 1412 | ` *` |
|        - | 1413 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - | 1414 | ` * name in the stack.` |
|        - | 1415 | ` */` |
|      589 | 1416 | `case PH7_OP_LOAD_CLOSURE: {` |
|        - | 1417 | `	VmOpRc rcOp;` |
|     1183 | 1418 | `	sState.pTos = pTos;` |
|     1183 | 1419 | `	sState.pc = pc;` |
|     1183 | 1420 | `	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);` |
|     1183 | 1421 | `	pTos = sState.pTos;` |
|     1183 | 1422 | `	pc = sState.pc;` |
|     1183 | 1423 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1424 | `		goto Abort;` |
|     1183 | 1425 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1426 | `		goto Exception;` |
|        - | 1427 | `	}` |
|     1183 | 1428 | `	break;` |
|        - | 1429 | `					  }` |
|        - | 1430 | `/*` |
|        - | 1431 | ` * LOAD_FCC P1 * *` |
|        - | 1432 | ` *` |
|        - | 1433 | ` * First-class callable: wrap the callee in a Closure object instead of calling it.` |
|        - | 1434 | ` *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS` |
|        - | 1435 | ` *           (from the callee's OP_LOADC). Replace it in place with a Closure whose` |
|        - | 1436 | ` *           $__fn is that name; the existing string-callable dispatch resolves it.` |
|        - | 1437 | ` *           (OOM degrades to leaving the name string on the stack — still callable.)` |
|        - | 1438 | ` *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name` |
|        - | 1439 | ` *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds` |
|        - | 1440 | ` *           $this (scope = its class); a class-name-string target is a static callable` |
|        - | 1441 | ` *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —` |
|        - | 1442 | ` *           the popped target leaves no name string to keep.)` |
|        - | 1443 | ` */` |
|       41 | 1444 | `case PH7_OP_LOAD_FCC:{` |
|       84 | 1445 | `	if( pInstr->iP1 == 1 ){` |
|        - | 1446 | ``		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)`` |
|        - | 1447 | `		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —` |
|        - | 1448 | `		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,` |
|        - | 1449 | ``		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a`` |
|        - | 1450 | `		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is` |
|        - | 1451 | `		 * (graceful degradation), and so is the original on OOM — still whatever it was. */` |
|        - | 1452 | `		ph7_class_instance *pCloObj;` |
|       48 | 1453 | `		if( VmValueIsClosure(pVm, pTos) ){` |
|        3 | 1454 | `			break;` |
|        - | 1455 | `		}` |
|       46 | 1456 | `		pCloObj = VmFccWrapValue(pVm, pTos);` |
|       46 | 1457 | `		if( pCloObj ){` |
|       46 | 1458 | `			PH7_MemObjRelease(pTos);` |
|       46 | 1459 | `			pCloObj->iRef++;` |
|       46 | 1460 | `			pTos->x.pOther = pCloObj;` |
|       46 | 1461 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       22 | 1462 | `		}` |
|       24 | 1463 | `	}else{` |
|        - | 1464 | `		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]` |
|        - | 1465 | `		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its` |
|        - | 1466 | `		 * class); a class-name string target is a static callable (scope = that class). */` |
|       37 | 1467 | `		ph7_value *pTarget = &pTos[-1];` |
|        - | 1468 | `		SyString sName;` |
|        - | 1469 | `		ph7_class_instance *pCloObj;` |
|       37 | 1470 | `		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));` |
|       37 | 1471 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|       19 | 1472 | `			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;` |
|       19 | 1473 | `			pCloObj = VmCreateClosure(pVm, &sName, pBoundThis, &pBoundThis->pClass->sName);` |
|       28 | 1474 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|        - | 1475 | ``			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class`` |
|        - | 1476 | `			 * now, so the closure binds the concrete scope (matching PHP). */` |
|       19 | 1477 | `			ph7_class *pScopeCls = VmFccResolveScope(pVm, pTarget);` |
|       19 | 1478 | `			pCloObj = pScopeCls ? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;` |
|       10 | 1479 | `		}else{` |
|      ! 0 | 1480 | `			pCloObj = 0;` |
|        - | 1481 | `		}` |
|        - | 1482 | `		/* Pop the method name and the target, push the Closure. */` |
|       37 | 1483 | `		PH7_MemObjRelease(pTos);` |
|       37 | 1484 | `		pTos--;` |
|       37 | 1485 | `		PH7_MemObjRelease(pTos);` |
|       37 | 1486 | `		if( pCloObj ){` |
|       37 | 1487 | `			pCloObj->iRef++;` |
|       37 | 1488 | `			pTos->x.pOther = pCloObj;` |
|       37 | 1489 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       19 | 1490 | `		}else{` |
|      ! 0 | 1491 | `			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */` |
|        - | 1492 | `		}` |
|        - | 1493 | `	}` |
|       82 | 1494 | `	break;` |
|        - | 1495 | `					 }` |
|        - | 1496 | `/*` |
|        - | 1497 | ` * STORE * P2 P3` |
|        - | 1498 | ` *` |
|        - | 1499 | ` * Perform a store (Assignment) operation.` |
|        - | 1500 | ` */` |
|   318348 | 1501 | `case PH7_OP_STORE: {` |
|        - | 1502 | `	ph7_value *pObj;` |
|        - | 1503 | `	SyString sName;` |
|        - | 1504 | `#ifdef UNTRUST` |
|        - | 1505 | `	if( pTos < pStack ){` |
|        - | 1506 | `		goto Abort;` |
|        - | 1507 | `	}` |
|        - | 1508 | `#endif` |
|   637537 | 1509 | `	if( pInstr->iP2 ){` |
|        - | 1510 | `		sxu32 nIdx;` |
|        - | 1511 | `		sxi32 rcT;` |
|        - | 1512 | `		/* Member store operation */` |
|    11901 | 1513 | `		nIdx = pTos->nIdx;` |
|    11901 | 1514 | `		VmPopOperand(&pTos,1);` |
|    11901 | 1515 | `		if( pVm->pMagicSetThis ){` |
|        - | 1516 | `			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain` |
|        - | 1517 | `			 * store to a missing/inaccessible property on a class declaring __set.` |
|        - | 1518 | `			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on` |
|        - | 1519 | `			 * the stack as the assignment expression's result — php's semantics` |
|        - | 1520 | `			 * (no property is created; a throw rides the boundary rail). */` |
|       11 | 1521 | `			ph7_class_instance *pSetThis = pVm->pMagicSetThis;` |
|        - | 1522 | `			SyString sSetName;` |
|       11 | 1523 | `			pVm->pMagicSetThis = 0;` |
|       11 | 1524 | `			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));` |
|       11 | 1525 | `			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);` |
|       11 | 1526 | `			PH7_ClassInstanceUnref(pSetThis);` |
|       11 | 1527 | `			SyBlobReset(&pVm->sMagicSetName);` |
|       11 | 1528 | `			break;` |
|        - | 1529 | `		}` |
|    11891 | 1530 | `		if( pVm->pHookSetThis ){` |
|        - | 1531 | `			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a` |
|        - | 1532 | `			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with` |
|        - | 1533 | `			 * the rvalue (which stays on the stack as the assignment expression's` |
|        - | 1534 | ``			 * result); a `set => expr` hook's return value is stored into the`` |
|        - | 1535 | `			 * BACKING slot with the ordinary typed enforcement. A property with` |
|        - | 1536 | `			 * only a get hook is php's catchable "is read-only" Error. */` |
|       31 | 1537 | `			ph7_class_instance *pHThis = pVm->pHookSetThis;` |
|       31 | 1538 | `			ph7_class_attr *pHAttr = pVm->pHookSetAttr;` |
|       31 | 1539 | `			sxu32 nBackIdx = pVm->nHookSetIdx;` |
|        - | 1540 | `			sxi32 rcHs;` |
|       31 | 1541 | `			pVm->pHookSetThis = 0;` |
|       31 | 1542 | `			pVm->pHookSetAttr = 0;` |
|       31 | 1543 | `			pVm->nHookSetIdx = SXU32_HIGH;` |
|       31 | 1544 | `			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);` |
|       31 | 1545 | `			PH7_ClassInstanceUnref(pHThis);` |
|       31 | 1546 | `			if( rcHs == PH7_ABORT ){` |
|      ! 0 | 1547 | `				goto Abort;` |
|        - | 1548 | `			}` |
|       31 | 1549 | `			break;` |
|        - | 1550 | `		}` |
|    11861 | 1551 | `		if( nIdx == SXU32_HIGH ){` |
|        3 | 1552 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 1553 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        3 | 1554 | `			pTos->nIdx = SXU32_HIGH;` |
|        2 | 1555 | `		}else{` |
|        - | 1556 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - | 1557 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|    11859 | 1558 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);` |
|    11859 | 1559 | `			if( rcT == PH7_ABORT ){` |
|       13 | 1560 | `				goto Abort;` |
|        - | 1561 | `			}` |
|    11849 | 1562 | `			if( rcT == PH7_EXCEPTION ){` |
|        - | 1563 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - | 1564 | `				 * control to the nearest catch block if any, otherwise` |
|        - | 1565 | `				 * propagate out of the VM loop. */` |
|       73 | 1566 | `				VmPopOperand(&pTos,1);` |
|        - | 1567 | `				{` |
|        - | 1568 | `					sxi32 iRp;` |
|       73 | 1569 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|       69 | 1570 | `						pc = iRp;` |
|       69 | 1571 | `						break;` |
|        - | 1572 | `					}` |
|        - | 1573 | `				}` |
|        5 | 1574 | `				goto Exception;` |
|        - | 1575 | `			}` |
|        - | 1576 | `			/* Point to the desired memory object */` |
|    11781 | 1577 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    11781 | 1578 | `			if( pObj ){` |
|        - | 1579 | `				/* Perform the store operation */` |
|    11781 | 1580 | `				PH7_MemObjStore(pTos,pObj);` |
|     5888 | 1581 | `			}` |
|        - | 1582 | `		}` |
|    11783 | 1583 | `		break;` |
|   625641 | 1584 | `	}else if( pInstr->p3 == 0 ){` |
|        - | 1585 | `		/* Take the variable name from the next on the stack */` |
|        9 | 1586 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1587 | `			/* Force a string cast */` |
|      ! 0 | 1588 | `			PH7_MemObjToString(pTos);` |
|      ! 0 | 1589 | `		}` |
|        9 | 1590 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        9 | 1591 | `		pTos--;` |
|        - | 1592 | `#ifdef UNTRUST` |
|        - | 1593 | `		if( pTos < pStack  ){` |
|        - | 1594 | `			goto Abort;` |
|        - | 1595 | `		}` |
|        - | 1596 | `#endif` |
|        5 | 1597 | `	}else{` |
|   625633 | 1598 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 1599 | `	}` |
|   625636 | 1600 | `	if( sName.nByte == sizeof("GLOBALS")-1` |
|   314572 | 1601 | `	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){` |
|        6 | 1602 | `		if( pInstr->p3 ){` |
|        - | 1603 | `			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time` |
|        - | 1604 | `			 * fatal there; raised at the store site here with the same` |
|        - | 1605 | `			 * message and the same non-catchable outcome). Element writes` |
|        - | 1606 | `			 * are unaffected. */` |
|        3 | 1607 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 1608 | `				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 1609 | `			pVm->iExitStatus = 255;` |
|        3 | 1610 | `			pVm->bHaltRequested = 1;` |
|        3 | 1611 | `			goto Abort;` |
|        - | 1612 | `		}` |
|        - | 1613 | `		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's` |
|        - | 1614 | `		 * compile-time case: php quietly creates an ordinary symbol-table` |
|        - | 1615 | `		 * entry named GLOBALS, leaving the auto-global view intact. */` |
|        3 | 1616 | `		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);` |
|        3 | 1617 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 1618 | `		break;` |
|        - | 1619 | `	}` |
|        - | 1620 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   625637 | 1621 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   625637 | 1622 | `	if( pObj == 0 ){` |
|      ! 0 | 1623 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1624 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 1625 | `		goto Abort;` |
|        - | 1626 | `	}` |
|   625637 | 1627 | `	if( !pInstr->p3 ){` |
|        7 | 1628 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 1629 | `	}` |
|        - | 1630 | `	/* Perform the store operation */` |
|   625637 | 1631 | `	PH7_MemObjStore(pTos,pObj);` |
|   625637 | 1632 | `	break;` |
|        - | 1633 | `				   }` |
|        - | 1634 | `/*` |
|        - | 1635 | ` * STORE_IDX:   P1 * P3` |
|        - | 1636 | ` * STORE_IDX_R: P1 * P3` |
|        - | 1637 | ` *` |
|        - | 1638 | ` * Perfrom a store operation an a hashmap entry.` |
|        - | 1639 | ` */` |
|   121075 | 1640 | `case PH7_OP_STORE_IDX:` |
|        - | 1641 | `case PH7_OP_STORE_IDX_REF: {` |
|        - | 1642 | `	VmOpRc rcOp;` |
|   242155 | 1643 | `	sState.pTos = pTos;` |
|   242155 | 1644 | `	sState.pc = pc;` |
|   242155 | 1645 | `	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);` |
|   242155 | 1646 | `	pTos = sState.pTos;` |
|   242155 | 1647 | `	pc = sState.pc;` |
|   242155 | 1648 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 1649 | `		goto Abort;` |
|   242153 | 1650 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       17 | 1651 | `		goto Exception;` |
|        - | 1652 | `	}` |
|   242139 | 1653 | `	break;` |
|        - | 1654 | `					  }` |
|        - | 1655 | `/*` |
|        - | 1656 | ` * INCR: P1 * *` |
|        - | 1657 | ` *` |
|        - | 1658 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - | 1659 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - | 1660 | ` * the stack and increment after that.` |
|        - | 1661 | ` */` |
|   187491 | 1662 | `case PH7_OP_INCR:` |
|        - | 1663 | `#ifdef UNTRUST` |
|        - | 1664 | `	if( pTos < pStack ){` |
|        - | 1665 | `		goto Abort;` |
|        - | 1666 | `	}` |
|        - | 1667 | `#endif` |
|        - | 1668 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|        - | 1669 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 1670 | `	 * — which otherwise skips object/array/resource operands. */` |
|   375196 | 1671 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 1672 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 1673 | `	 * php's TypeError, raised BEFORE any set dispatch (the type guard below` |
|        - | 1674 | `	 * would skip the mutation and the tail write-back would otherwise call` |
|        - | 1675 | `	 * the set hook with the unchanged value). */` |
|   375185 | 1676 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|   187703 | 1677 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 1678 | `		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 1679 | `		if( pTopInc->iKind == VM_HOOK_PEND_RMW && pTopInc->nScratchIdx == pTos->nIdx ){` |
|        - | 1680 | `			SyBlob sErrMsg;` |
|        3 | 1681 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 1682 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        3 | 1683 | `				SyBlobAppend(&sErrMsg,"Cannot increment array",sizeof("Cannot increment array")-1);` |
|        1 | 1684 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1685 | `				SyBlobFormat(&sErrMsg,"Cannot increment %z",` |
|      ! 0 | 1686 | `					&((ph7_class_instance *)pTos->x.pOther)->pClass->sName);` |
|      ! 0 | 1687 | `			}else{` |
|      ! 0 | 1688 | `				SyBlobAppend(&sErrMsg,"Cannot increment resource",sizeof("Cannot increment resource")-1);` |
|        - | 1689 | `			}` |
|        3 | 1690 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 1691 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 1692 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 1693 | `			break;` |
|        - | 1694 | `		}` |
|      ! 0 | 1695 | `	}` |
|   375188 | 1696 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   375188 | 1697 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 1698 | `			ph7_value *pObj;` |
|   375188 | 1699 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   375188 | 1700 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 1701 | `					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric` |
|        - | 1702 | `					 * string (it points at str_increment() instead); PHL rejects it. */` |
|        - | 1703 | `					SyBlob sErrMsg;` |
|        3 | 1704 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 1705 | `					SyBlobAppend(&sErrMsg,` |
|        - | 1706 | `						"Increment on a non-numeric string is not supported, use str_increment() instead",` |
|        - | 1707 | `						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);` |
|        3 | 1708 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 1709 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 1710 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 1711 | `					break;` |
|      ! 0 | 1712 | `				}else{` |
|        - | 1713 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - | 1714 | `					 * original value: pTos may alias pObj's blob via` |
|        - | 1715 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - | 1716 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - | 1717 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - | 1718 | `					 * so its old-value view survives the coercion. */` |
|   375186 | 1719 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        9 | 1720 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        3 | 1721 | `					}` |
|        - | 1722 | `					/* Force a numeric cast on the variable */` |
|   375186 | 1723 | `					PH7_MemObjToNumeric(pObj);` |
|   375186 | 1724 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        5 | 1725 | `						pObj->rVal++;` |
|        - | 1726 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 1727 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 1728 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 1729 | `						 * integer-valued real. */` |
|        5 | 1730 | `						PH7_MemObjTryInteger(pObj);` |
|        3 | 1731 | `					}else{` |
|        - | 1732 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 1733 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 1734 | `						sxi64 r;` |
|   375182 | 1735 | `						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 1736 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        5 | 1737 | `							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;` |
|        5 | 1738 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 1739 | `#else` |
|        - | 1740 | `							pObj->x.iVal = r;` |
|        - | 1741 | `#endif` |
|        3 | 1742 | `						}else{` |
|   375178 | 1743 | `							pObj->x.iVal = r;` |
|        - | 1744 | `						}` |
|        - | 1745 | `					}` |
|   375186 | 1746 | `					if( pInstr->iP1 ){` |
|        - | 1747 | `						/* Pre-increment: result is the new value. */` |
|      137 | 1748 | `						PH7_MemObjStore(pObj,pTos);` |
|       68 | 1749 | `					}` |
|        - | 1750 | `					/* Post-increment: pTos retains the old value (a string` |
|        - | 1751 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - | 1752 | `				}` |
|   187695 | 1753 | `			}` |
|   187700 | 1754 | `		}else{` |
|      ! 0 | 1755 | `			if( pInstr->iP1 ){` |
|      ! 0 | 1756 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 | 1757 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 | 1758 | `				}else{` |
|        - | 1759 | `					/* Force a numeric cast */` |
|      ! 0 | 1760 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 1761 | `					/* Pre-increment */` |
|      ! 0 | 1762 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 1763 | `						pTos->rVal++;` |
|        - | 1764 | `						/* Try to get an integer representation */` |
|      ! 0 | 1765 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 1766 | `					}else{` |
|        - | 1767 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 1768 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 1769 | `						sxi64 r;` |
|      ! 0 | 1770 | `						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 1771 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 1772 | `							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;` |
|      ! 0 | 1773 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 1774 | `#else` |
|        - | 1775 | `							pTos->x.iVal = r;` |
|        - | 1776 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 1777 | `#endif` |
|      ! 0 | 1778 | `						}else{` |
|      ! 0 | 1779 | `							pTos->x.iVal = r;` |
|      ! 0 | 1780 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 1781 | `						}` |
|        - | 1782 | `					}` |
|        - | 1783 | `				}` |
|      ! 0 | 1784 | `			}` |
|        - | 1785 | `		}` |
|   187695 | 1786 | `	}` |
|   375186 | 1787 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|   375186 | 1788 | `	break;` |
|        - | 1789 | `/*` |
|        - | 1790 | ` * DECR: P1 * *` |
|        - | 1791 | ` *` |
|        - | 1792 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - | 1793 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - | 1794 | ` * and decrement after that.` |
|        - | 1795 | ` */` |
|      102 | 1796 | `case PH7_OP_DECR:` |
|        - | 1797 | `#ifdef UNTRUST` |
|        - | 1798 | `	if( pTos < pStack ){` |
|        - | 1799 | `		goto Abort;` |
|        - | 1800 | `	}` |
|        - | 1801 | `#endif` |
|        - | 1802 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|        - | 1803 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 1804 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|        - | 1805 | `	 * property currently holding null). */` |
|      209 | 1806 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 1807 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 1808 | `	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —` |
|        - | 1809 | ``	 * `--` on null is php's no-op and its write-back still dispatches set). */`` |
|      198 | 1810 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|      103 | 1811 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 1812 | `		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 1813 | `		if( pTopDec->iKind == VM_HOOK_PEND_RMW && pTopDec->nScratchIdx == pTos->nIdx ){` |
|        - | 1814 | `			SyBlob sErrMsg;` |
|        3 | 1815 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 1816 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        3 | 1817 | `				SyBlobAppend(&sErrMsg,"Cannot decrement array",sizeof("Cannot decrement array")-1);` |
|        1 | 1818 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1819 | `				SyBlobFormat(&sErrMsg,"Cannot decrement %z",` |
|      ! 0 | 1820 | `					&((ph7_class_instance *)pTos->x.pOther)->pClass->sName);` |
|      ! 0 | 1821 | `			}else{` |
|      ! 0 | 1822 | `				SyBlobAppend(&sErrMsg,"Cannot decrement resource",sizeof("Cannot decrement resource")-1);` |
|        - | 1823 | `			}` |
|        3 | 1824 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 1825 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 1826 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 1827 | `			break;` |
|        - | 1828 | `		}` |
|      ! 0 | 1829 | `	}` |
|        - | 1830 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op) -- but 8.3`` |
|        - | 1831 | `	 * deprecates that no-op, same as the non-numeric-string one below. */` |
|      199 | 1832 | `	if( pTos->iFlags & MEMOBJ_NULL ){` |
|        - | 1833 | `		/* E_WARNING, not E_DEPRECATED -- php reports this one at errno 2. */` |
|      ! 0 | 1834 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 1835 | `			"Decrement on type null has no effect, this will change in the next major version of PHP");` |
|      ! 0 | 1836 | `	}` |
|      199 | 1837 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      199 | 1838 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 1839 | `			ph7_value *pObj;` |
|      199 | 1840 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      199 | 1841 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 1842 | ``					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string`` |
|        - | 1843 | `					 * (php has no string decrement); PHL rejects it. */` |
|        - | 1844 | `					SyBlob sErrMsg;` |
|        3 | 1845 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 1846 | `					SyBlobAppend(&sErrMsg,` |
|        - | 1847 | `						"Decrement on a non-numeric string is not supported",` |
|        - | 1848 | `						sizeof("Decrement on a non-numeric string is not supported")-1);` |
|        3 | 1849 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 1850 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 1851 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 1852 | `					break;` |
|      ! 0 | 1853 | `				}else{` |
|        - | 1854 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - | 1855 | `					 * post-decrement must preserve pTos's original value, which` |
|        - | 1856 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - | 1857 | `					 * Force pTos to own its blob before coercing pObj. */` |
|      197 | 1858 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 | 1859 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 | 1860 | `					}` |
|      197 | 1861 | `					PH7_MemObjToNumeric(pObj);` |
|      197 | 1862 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 | 1863 | `						pObj->rVal--;` |
|        - | 1864 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 1865 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 1866 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 1867 | `						 * integer-valued real. */` |
|        9 | 1868 | `						PH7_MemObjTryInteger(pObj);` |
|        5 | 1869 | `					}else{` |
|        - | 1870 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 1871 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 1872 | `						sxi64 r;` |
|      189 | 1873 | `						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 1874 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        3 | 1875 | `							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;` |
|        3 | 1876 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 1877 | `#else` |
|        - | 1878 | `							pObj->x.iVal = r;` |
|        - | 1879 | `#endif` |
|        2 | 1880 | `						}else{` |
|      187 | 1881 | `							pObj->x.iVal = r;` |
|        - | 1882 | `						}` |
|        - | 1883 | `					}` |
|      197 | 1884 | `					if( pInstr->iP1 ){` |
|        - | 1885 | `						/* Pre-decrement: result is the new value. */` |
|        3 | 1886 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 | 1887 | `					}` |
|        - | 1888 | `					/* Post-decrement: pTos retains the old value. */` |
|        - | 1889 | `				}` |
|       97 | 1890 | `			}` |
|      100 | 1891 | `		}else{` |
|      ! 0 | 1892 | `			if( pInstr->iP1 ){` |
|      ! 0 | 1893 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - | 1894 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 | 1895 | `				}else{` |
|        - | 1896 | `					/* Force a numeric cast */` |
|      ! 0 | 1897 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 1898 | `					/* Pre-decrement */` |
|      ! 0 | 1899 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 1900 | `						pTos->rVal--;` |
|        - | 1901 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 | 1902 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 1903 | `					}else{` |
|        - | 1904 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 1905 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 1906 | `						sxi64 r;` |
|      ! 0 | 1907 | `						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 1908 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 1909 | `							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;` |
|      ! 0 | 1910 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 1911 | `#else` |
|        - | 1912 | `							pTos->x.iVal = r;` |
|        - | 1913 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 1914 | `#endif` |
|      ! 0 | 1915 | `						}else{` |
|      ! 0 | 1916 | `							pTos->x.iVal = r;` |
|      ! 0 | 1917 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 1918 | `						}` |
|        - | 1919 | `					}` |
|        - | 1920 | `				}` |
|      ! 0 | 1921 | `			}` |
|        - | 1922 | `		}` |
|       97 | 1923 | `	}` |
|      197 | 1924 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|      197 | 1925 | `	break;` |
|        - | 1926 | `/*` |
|        - | 1927 | ` * UMINUS: * * *` |
|        - | 1928 | ` *` |
|        - | 1929 | ` * Perform a unary minus operation.` |
|        - | 1930 | ` */` |
|    35217 | 1931 | `case PH7_OP_UMINUS:` |
|        - | 1932 | `#ifdef UNTRUST` |
|        - | 1933 | `	if( pTos < pStack ){` |
|        - | 1934 | `		goto Abort;` |
|        - | 1935 | `	}` |
|        - | 1936 | `#endif` |
|        - | 1937 | `	/* Force a numeric (integer,real or both) cast */` |
|    70439 | 1938 | `	PH7_MemObjToNumeric(pTos);` |
|    70439 | 1939 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      108 | 1940 | `		pTos->rVal = -pTos->rVal;` |
|       53 | 1941 | `	}` |
|    70439 | 1942 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    70353 | 1943 | `		if( pTos->x.iVal == SMALLEST_INT64 ){` |
|        - | 1944 | `			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a` |
|        - | 1945 | `			 * REAL representation is already present it is the negated` |
|        - | 1946 | `			 * authoritative value, so just drop the now-stale cached int. The` |
|        - | 1947 | `			 * integer-only build has no float type, so it wraps (two's` |
|        - | 1948 | `			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */` |
|        - | 1949 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        5 | 1950 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 | 1951 | `				pTos->rVal = -(ph7_real)pTos->x.iVal;` |
|        5 | 1952 | `				MemObjSetType(pTos,MEMOBJ_REAL);` |
|        3 | 1953 | `			}else{` |
|      ! 0 | 1954 | `				pTos->iFlags &= ~MEMOBJ_INT;` |
|        - | 1955 | `			}` |
|        - | 1956 | `#else` |
|        - | 1957 | `			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);` |
|        - | 1958 | `#endif` |
|        3 | 1959 | `		}else{` |
|    70349 | 1960 | `			pTos->x.iVal = -pTos->x.iVal;` |
|        - | 1961 | `		}` |
|    35174 | 1962 | `	}` |
|    70439 | 1963 | `	break;` |
|        - | 1964 | `/*` |
|        - | 1965 | ` * UPLUS: * * *` |
|        - | 1966 | ` *` |
|        - | 1967 | ` * Perform a unary plus operation.` |
|        - | 1968 | ` */` |
|       18 | 1969 | `case PH7_OP_UPLUS:` |
|        - | 1970 | `#ifdef UNTRUST` |
|        - | 1971 | `	if( pTos < pStack ){` |
|        - | 1972 | `		goto Abort;` |
|        - | 1973 | `	}` |
|        - | 1974 | `#endif` |
|        - | 1975 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 | 1976 | `	PH7_MemObjToNumeric(pTos);` |
|       37 | 1977 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 1978 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 | 1979 | `	}` |
|       37 | 1980 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 | 1981 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 | 1982 | `	}` |
|       37 | 1983 | `	break;` |
|        - | 1984 | `/*` |
|        - | 1985 | ` * OP_LNOT: * * *` |
|        - | 1986 | ` *` |
|        - | 1987 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - | 1988 | ` * with its complement.` |
|        - | 1989 | ` */` |
|    25234 | 1990 | `case PH7_OP_LNOT:` |
|        - | 1991 | `#ifdef UNTRUST` |
|        - | 1992 | `	if( pTos < pStack ){` |
|        - | 1993 | `		goto Abort;` |
|        - | 1994 | `	}` |
|        - | 1995 | `#endif` |
|        - | 1996 | `	/* Force a boolean cast */` |
|    50473 | 1997 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      159 | 1998 | `		PH7_MemObjToBool(pTos);` |
|       77 | 1999 | `	}` |
|    50473 | 2000 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    50473 | 2001 | `	break;` |
|        - | 2002 | `/*` |
|        - | 2003 | ` * OP_BITNOT: * * *` |
|        - | 2004 | ` *` |
|        - | 2005 | ` * Interpret the top of the stack as an value.Replace it` |
|        - | 2006 | ` * with its ones-complement.` |
|        - | 2007 | ` */` |
|        7 | 2008 | `case PH7_OP_BITNOT:` |
|        - | 2009 | `#ifdef UNTRUST` |
|        - | 2010 | `	if( pTos < pStack ){` |
|        - | 2011 | `		goto Abort;` |
|        - | 2012 | `	}` |
|        - | 2013 | `#endif` |
|        - | 2014 | `	/* Force an integer cast (php deprecates a lossy float here too) */` |
|       16 | 2015 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       16 | 2016 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|       16 | 2017 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2018 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 2019 | `	}` |
|       16 | 2020 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       16 | 2021 | `	break;` |
|        - | 2022 | `/* OP_MUL * * *` |
|        - | 2023 | ` * OP_MUL_STORE * * *` |
|        - | 2024 | ` *` |
|        - | 2025 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - | 2026 | ` * and push the result back onto the stack.` |
|        - | 2027 | ` */` |
|     1536 | 2028 | `case PH7_OP_MUL:` |
|        - | 2029 | `case PH7_OP_MUL_STORE: {` |
|        - | 2030 | `	VmOpRc rcOp;` |
|     3077 | 2031 | `	sState.pTos = pTos;` |
|     3077 | 2032 | `	sState.pc = pc;` |
|     3077 | 2033 | `	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);` |
|     3077 | 2034 | `	pTos = sState.pTos;` |
|     3077 | 2035 | `	pc = sState.pc;` |
|     3077 | 2036 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2037 | `		goto Abort;` |
|     3077 | 2038 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2039 | `		goto Exception;` |
|        - | 2040 | `	}` |
|     3077 | 2041 | `	break;` |
|        - | 2042 | `					  }` |
|        - | 2043 | `/* OP_POW * * *` |
|        - | 2044 | ` * OP_POW_STORE * * *` |
|        - | 2045 | ` *` |
|        - | 2046 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - | 2047 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - | 2048 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - | 2049 | ` * fits in sxi64; otherwise the result is a double.` |
|        - | 2050 | ` */` |
|       68 | 2051 | `case PH7_OP_POW:` |
|        - | 2052 | `case PH7_OP_POW_STORE: {` |
|        - | 2053 | `	VmOpRc rcOp;` |
|      137 | 2054 | `	sState.pTos = pTos;` |
|      137 | 2055 | `	sState.pc = pc;` |
|      137 | 2056 | `	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);` |
|      137 | 2057 | `	pTos = sState.pTos;` |
|      137 | 2058 | `	pc = sState.pc;` |
|      137 | 2059 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2060 | `		goto Abort;` |
|      137 | 2061 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 2062 | `		goto Exception;` |
|        - | 2063 | `	}` |
|      135 | 2064 | `	break;` |
|        - | 2065 | `					  }` |
|        - | 2066 | `/* OP_ADD * * *` |
|        - | 2067 | ` *` |
|        - | 2068 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 2069 | ` * and push the result back onto the stack.` |
|        - | 2070 | ` */` |
|     6337 | 2071 | `case PH7_OP_ADD:{` |
|    12679 | 2072 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2073 | `#ifdef UNTRUST` |
|        - | 2074 | `	if( pNos < pStack ){` |
|        - | 2075 | `		goto Abort;` |
|        - | 2076 | `	}` |
|        - | 2077 | `#endif` |
|        - | 2078 | `	{` |
|        - | 2079 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|        - | 2080 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|        - | 2081 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|        - | 2082 | `		SyBlob sArMsg;` |
|    12679 | 2083 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    12679 | 2084 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 2085 | `			sxi32 rcAr;` |
|        9 | 2086 | `			VmPopOperand(&pTos,1);` |
|        9 | 2087 | `			PH7_MemObjRelease(pTos);` |
|        9 | 2088 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        9 | 2089 | `			pTos->nIdx = SXU32_HIGH;` |
|       13 | 2090 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|        4 | 2091 | `				SyBlobLength(&sArMsg));` |
|        9 | 2092 | `			SyBlobRelease(&sArMsg);` |
|        9 | 2093 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|        9 | 2094 | `			rc = rcAr;` |
|        9 | 2095 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2096 | `		}` |
|    12671 | 2097 | `		SyBlobRelease(&sArMsg);` |
|        - | 2098 | `	}` |
|        - | 2099 | `	/* Perform the addition */` |
|    12671 | 2100 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|    12671 | 2101 | `	VmPopOperand(&pTos,1);` |
|    12671 | 2102 | `	break;` |
|        - | 2103 | `				}` |
|        - | 2104 | `/*` |
|        - | 2105 | ` * OP_ADD_STORE * * *` |
|        - | 2106 | ` *` |
|        - | 2107 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 2108 | ` * and push the result back onto the stack.` |
|        - | 2109 | ` */` |
|      980 | 2110 | `case PH7_OP_ADD_STORE:{` |
|     1965 | 2111 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2112 | `	ph7_value *pObj;` |
|        - | 2113 | `	sxu32 nIdx;` |
|        - | 2114 | `#ifdef UNTRUST` |
|        - | 2115 | `	if( pNos < pStack ){` |
|        - | 2116 | `		goto Abort;` |
|        - | 2117 | `	}` |
|        - | 2118 | `#endif` |
|        - | 2119 | `	{` |
|        - | 2120 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 2121 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 2122 | `		SyBlob sArMsg;` |
|     1965 | 2123 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     1965 | 2124 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 2125 | `			sxi32 rcAr;` |
|      ! 0 | 2126 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 2127 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 2128 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 2129 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 2130 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 2131 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 2132 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 2133 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 2134 | `			rc = rcAr;` |
|      ! 0 | 2135 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2136 | `		}` |
|     1965 | 2137 | `		SyBlobRelease(&sArMsg);` |
|        - | 2138 | `	}` |
|        - | 2139 | `	/* Perform the addition */` |
|     1965 | 2140 | `	nIdx = pTos->nIdx;` |
|     1965 | 2141 | `	if( nIdx == pVm->nGlobalIdx ){` |
|        - | 2142 | `		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment` |
|        - | 2143 | `		 * (a compile-time fatal in php; raised here, same message). */` |
|        3 | 2144 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 2145 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 2146 | `		pVm->iExitStatus = 255;` |
|        3 | 2147 | `		pVm->bHaltRequested = 1;` |
|        3 | 2148 | `		goto Abort;` |
|        - | 2149 | `	}` |
|     1963 | 2150 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - | 2151 | `	/* Peform the store operation */` |
|     1963 | 2152 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 2153 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1963 | 2154 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1963 | 2155 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1963 | 2156 | `		PH7_MemObjStore(pTos,pObj);` |
|      979 | 2157 | `	}` |
|     1963 | 2158 | `	PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|        - | 2159 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1963 | 2160 | `	PH7_MemObjStore(pTos,pNos);` |
|     1963 | 2161 | `	VmPopOperand(&pTos,1);` |
|     1963 | 2162 | `	break;` |
|        - | 2163 | `				}` |
|        - | 2164 | `/* OP_SUB * * *` |
|        - | 2165 | ` *` |
|        - | 2166 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 2167 | ` * first (what was next on the stack) from the second (the` |
|        - | 2168 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 2169 | ` */` |
|    12302 | 2170 | `case PH7_OP_SUB: {` |
|        - | 2171 | `	VmOpRc rcOp;` |
|    24723 | 2172 | `	sState.pTos = pTos;` |
|    24723 | 2173 | `	sState.pc = pc;` |
|    24723 | 2174 | `	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);` |
|    24723 | 2175 | `	pTos = sState.pTos;` |
|    24723 | 2176 | `	pc = sState.pc;` |
|    24723 | 2177 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2178 | `		goto Abort;` |
|    24723 | 2179 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2180 | `		goto Exception;` |
|        - | 2181 | `	}` |
|    24723 | 2182 | `	break;` |
|        - | 2183 | `					  }` |
|        - | 2184 | `/* OP_SUB_STORE * * *` |
|        - | 2185 | ` *` |
|        - | 2186 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 2187 | ` * first (what was next on the stack) from the second (the` |
|        - | 2188 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 2189 | ` */` |
|        6 | 2190 | `case PH7_OP_SUB_STORE: {` |
|        - | 2191 | `	VmOpRc rcOp;` |
|       14 | 2192 | `	sState.pTos = pTos;` |
|       14 | 2193 | `	sState.pc = pc;` |
|       14 | 2194 | `	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);` |
|       14 | 2195 | `	pTos = sState.pTos;` |
|       14 | 2196 | `	pc = sState.pc;` |
|       14 | 2197 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2198 | `		goto Abort;` |
|       14 | 2199 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 2200 | `		goto Exception;` |
|        - | 2201 | `	}` |
|       12 | 2202 | `	break;` |
|        - | 2203 | `					  }` |
|        - | 2204 |  |
|        - | 2205 | `/*` |
|        - | 2206 | ` * OP_MOD * * *` |
|        - | 2207 | ` *` |
|        - | 2208 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2209 | ` * first (what was next on the stack) from the second (the` |
|        - | 2210 | ` * top of the stack) and push the remainder after division` |
|        - | 2211 | ` * onto the stack.` |
|        - | 2212 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 2213 | ` */` |
|      525 | 2214 | `case PH7_OP_MOD: {` |
|        - | 2215 | `	VmOpRc rcOp;` |
|     1055 | 2216 | `	sState.pTos = pTos;` |
|     1055 | 2217 | `	sState.pc = pc;` |
|     1055 | 2218 | `	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);` |
|     1055 | 2219 | `	pTos = sState.pTos;` |
|     1055 | 2220 | `	pc = sState.pc;` |
|     1055 | 2221 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2222 | `		goto Abort;` |
|     1055 | 2223 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 2224 | `		goto Exception;` |
|        - | 2225 | `	}` |
|     1051 | 2226 | `	break;` |
|        - | 2227 | `					  }` |
|        - | 2228 | `/*` |
|        - | 2229 | ` * OP_MOD_STORE * * *` |
|        - | 2230 | ` *` |
|        - | 2231 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2232 | ` * first (what was next on the stack) from the second (the` |
|        - | 2233 | ` * top of the stack) and push the remainder after division` |
|        - | 2234 | ` * onto the stack.` |
|        - | 2235 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 2236 | ` */` |
|        4 | 2237 | `case PH7_OP_MOD_STORE: {` |
|        - | 2238 | `	VmOpRc rcOp;` |
|        9 | 2239 | `	sState.pTos = pTos;` |
|        9 | 2240 | `	sState.pc = pc;` |
|        9 | 2241 | `	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);` |
|        9 | 2242 | `	pTos = sState.pTos;` |
|        9 | 2243 | `	pc = sState.pc;` |
|        9 | 2244 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2245 | `		goto Abort;` |
|        9 | 2246 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 2247 | `		goto Exception;` |
|        - | 2248 | `	}` |
|        5 | 2249 | `	break;` |
|        - | 2250 | `					  }` |
|        - | 2251 | `/*` |
|        - | 2252 | ` * OP_DIV * * *` |
|        - | 2253 | ` *` |
|        - | 2254 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2255 | ` * first (what was next on the stack) from the second (the` |
|        - | 2256 | ` * top of the stack) and push the result onto the stack.` |
|        - | 2257 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 2258 | ` */` |
|       47 | 2259 | `case PH7_OP_DIV: {` |
|        - | 2260 | `	VmOpRc rcOp;` |
|       97 | 2261 | `	sState.pTos = pTos;` |
|       97 | 2262 | `	sState.pc = pc;` |
|       97 | 2263 | `	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);` |
|       97 | 2264 | `	pTos = sState.pTos;` |
|       97 | 2265 | `	pc = sState.pc;` |
|       97 | 2266 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2267 | `		goto Abort;` |
|       97 | 2268 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 2269 | `		goto Exception;` |
|        - | 2270 | `	}` |
|       91 | 2271 | `	break;` |
|        - | 2272 | `					  }` |
|        - | 2273 | `/*` |
|        - | 2274 | ` * OP_DIV_STORE * * *` |
|        - | 2275 | ` *` |
|        - | 2276 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2277 | ` * first (what was next on the stack) from the second (the` |
|        - | 2278 | ` * top of the stack) and push the result onto the stack.` |
|        - | 2279 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 2280 | ` */` |
|        4 | 2281 | `case PH7_OP_DIV_STORE:{` |
|        9 | 2282 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2283 | `	ph7_value *pObj;` |
|        - | 2284 | `	ph7_real a,b,r;` |
|        - | 2285 | `#ifdef UNTRUST` |
|        - | 2286 | `	if( pNos < pStack ){` |
|        - | 2287 | `		goto Abort;` |
|        - | 2288 | `	}` |
|        - | 2289 | `#endif` |
|        - | 2290 | `	{` |
|        - | 2291 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 2292 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 2293 | `		SyBlob sArMsg;` |
|        9 | 2294 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|        9 | 2295 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"/",&sArMsg) != SXRET_OK ){` |
|        - | 2296 | `			sxi32 rcAr;` |
|      ! 0 | 2297 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 2298 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 2299 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 2300 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 2301 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 2302 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 2303 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 2304 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 2305 | `			rc = rcAr;` |
|      ! 0 | 2306 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2307 | `		}` |
|        9 | 2308 | `		SyBlobRelease(&sArMsg);` |
|        - | 2309 | `	}` |
|        - | 2310 | `	/* Force the operands to be real */` |
|        9 | 2311 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 | 2312 | `		PH7_MemObjToReal(pTos);` |
|        4 | 2313 | `	}` |
|        9 | 2314 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 | 2315 | `		PH7_MemObjToReal(pNos);` |
|        4 | 2316 | `	}` |
|        - | 2317 | `	/* Perform the requested operation */` |
|        9 | 2318 | `	a = pTos->rVal;` |
|        9 | 2319 | `	b = pNos->rVal;` |
|        9 | 2320 | `	if( b == 0 ){` |
|        - | 2321 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|        - | 2322 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|        3 | 2323 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|        3 | 2324 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      ! 0 | 2325 | `	}else{` |
|        7 | 2326 | `		r = a/b;` |
|        - | 2327 | `		/* Push the result */` |
|        7 | 2328 | `		pNos->rVal = r;` |
|        7 | 2329 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - | 2330 | `		/* Try to get an integer representation */` |
|        7 | 2331 | `		PH7_MemObjTryInteger(pNos);` |
|        - | 2332 | `	}` |
|        7 | 2333 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 2334 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        7 | 2335 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        7 | 2336 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        7 | 2337 | `		PH7_MemObjStore(pNos,pObj);` |
|        3 | 2338 | `	}` |
|        7 | 2339 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|        7 | 2340 | `	VmPopOperand(&pTos,1);` |
|        7 | 2341 | `	break;` |
|        - | 2342 | `				}` |
|        - | 2343 | `/* OP_BAND * * *` |
|        - | 2344 | ` *` |
|        - | 2345 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2346 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 2347 | ` * two elements.` |
|        - | 2348 | `*/` |
|        - | 2349 | `/* OP_BOR * * *` |
|        - | 2350 | ` *` |
|        - | 2351 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2352 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 2353 | ` * two elements.` |
|        - | 2354 | ` */` |
|        - | 2355 | `/* OP_BXOR * * *` |
|        - | 2356 | ` *` |
|        - | 2357 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2358 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 2359 | ` * two elements.` |
|        - | 2360 | ` */` |
|      484 | 2361 | `case PH7_OP_BAND:` |
|        - | 2362 | `case PH7_OP_BOR:` |
|        - | 2363 | `case PH7_OP_BXOR:{` |
|      972 | 2364 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2365 | `	sxi64 a,b,r;` |
|        - | 2366 | `#ifdef UNTRUST` |
|        - | 2367 | `	if( pNos < pStack ){` |
|        - | 2368 | `		goto Abort;` |
|        - | 2369 | `	}` |
|        - | 2370 | `#endif` |
|        - | 2371 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|      972 | 2372 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|      972 | 2373 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      972 | 2374 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      972 | 2375 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      972 | 2376 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2377 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 2378 | `	}` |
|      972 | 2379 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2380 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 | 2381 | `	}` |
|        - | 2382 | `	/* Perform the requested operation */` |
|      972 | 2383 | `	a = pNos->x.iVal;` |
|      972 | 2384 | `	b = pTos->x.iVal;` |
|      972 | 2385 | `	switch(pInstr->iOp){` |
|       42 | 2386 | `	case PH7_OP_BOR_STORE:` |
|       86 | 2387 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 | 2388 | `	case PH7_OP_BXOR_STORE:` |
|       15 | 2389 | `	case PH7_OP_BXOR: r = a^b; break;` |
|      435 | 2390 | `	case PH7_OP_BAND_STORE:` |
|      435 | 2391 | `	case PH7_OP_BAND:` |
|      874 | 2392 | `	default:          r = a&b; break;` |
|        - | 2393 | `	}` |
|        - | 2394 | `	/* Push the result */` |
|      972 | 2395 | `	pNos->x.iVal = r;` |
|      972 | 2396 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      972 | 2397 | `	VmPopOperand(&pTos,1);` |
|      972 | 2398 | `	break;` |
|        - | 2399 | `				 }` |
|        - | 2400 | `/* OP_BAND_STORE * * *` |
|        - | 2401 | ` *` |
|        - | 2402 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2403 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 2404 | ` * two elements.` |
|        - | 2405 | `*/` |
|        - | 2406 | `/* OP_BOR_STORE * * *` |
|        - | 2407 | ` *` |
|        - | 2408 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2409 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 2410 | ` * two elements.` |
|        - | 2411 | ` */` |
|        - | 2412 | `/* OP_BXOR_STORE * * *` |
|        - | 2413 | ` *` |
|        - | 2414 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2415 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 2416 | ` * two elements.` |
|        - | 2417 | ` */` |
|       17 | 2418 | `case PH7_OP_BAND_STORE:` |
|        - | 2419 | `case PH7_OP_BOR_STORE:` |
|        - | 2420 | `case PH7_OP_BXOR_STORE:{` |
|       35 | 2421 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2422 | `	ph7_value *pObj;` |
|        - | 2423 | `	sxi64 a,b,r;` |
|        - | 2424 | `#ifdef UNTRUST` |
|        - | 2425 | `	if( pNos < pStack ){` |
|        - | 2426 | `		goto Abort;` |
|        - | 2427 | `	}` |
|        - | 2428 | `#endif` |
|        - | 2429 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|       35 | 2430 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|       35 | 2431 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|       35 | 2432 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       35 | 2433 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|       35 | 2434 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2435 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 2436 | `	}` |
|       35 | 2437 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2438 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 | 2439 | `	}` |
|        - | 2440 | `	/* Perform the requested operation */` |
|       35 | 2441 | `	a = pTos->x.iVal;` |
|       35 | 2442 | `	b = pNos->x.iVal;` |
|       35 | 2443 | `	switch(pInstr->iOp){` |
|       10 | 2444 | `	case PH7_OP_BOR_STORE:` |
|       21 | 2445 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 | 2446 | `	case PH7_OP_BXOR_STORE:` |
|        9 | 2447 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 | 2448 | `	case PH7_OP_BAND_STORE:` |
|        3 | 2449 | `	case PH7_OP_BAND:` |
|        7 | 2450 | `	default:          r = a&b; break;` |
|        - | 2451 | `	}` |
|        - | 2452 | `	/* Push the result */` |
|       35 | 2453 | `	pNos->x.iVal = r;` |
|       35 | 2454 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       35 | 2455 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 2456 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       35 | 2457 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       35 | 2458 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       35 | 2459 | `		PH7_MemObjStore(pNos,pObj);` |
|       17 | 2460 | `	}` |
|       35 | 2461 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       35 | 2462 | `	VmPopOperand(&pTos,1);` |
|       35 | 2463 | `	break;` |
|        - | 2464 | `				 }` |
|        - | 2465 | `/* OP_SHL * * *` |
|        - | 2466 | ` *` |
|        - | 2467 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2468 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 2469 | ` * left by N bits where N is the top element on the stack.` |
|        - | 2470 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 2471 | ` */` |
|        - | 2472 | `/* OP_SHR * * *` |
|        - | 2473 | ` *` |
|        - | 2474 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2475 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 2476 | ` * right by N bits where N is the top element on the stack.` |
|        - | 2477 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 2478 | ` */` |
|       29 | 2479 | `case PH7_OP_SHL:` |
|        - | 2480 | `case PH7_OP_SHR: {` |
|        - | 2481 | `	VmOpRc rcOp;` |
|       60 | 2482 | `	sState.pTos = pTos;` |
|       60 | 2483 | `	sState.pc = pc;` |
|       60 | 2484 | `	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);` |
|       60 | 2485 | `	pTos = sState.pTos;` |
|       60 | 2486 | `	pc = sState.pc;` |
|       60 | 2487 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2488 | `		goto Abort;` |
|       60 | 2489 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2490 | `		goto Exception;` |
|        - | 2491 | `	}` |
|       60 | 2492 | `	break;` |
|        - | 2493 | `					  }` |
|        - | 2494 | `/*  OP_SHL_STORE * * *` |
|        - | 2495 | ` *` |
|        - | 2496 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2497 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 2498 | ` * left by N bits where N is the top element on the stack.` |
|        - | 2499 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 2500 | ` */` |
|        - | 2501 | `/* OP_SHR_STORE * * *` |
|        - | 2502 | ` *` |
|        - | 2503 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2504 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 2505 | ` * right by N bits where N is the top element on the stack.` |
|        - | 2506 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 2507 | ` */` |
|        9 | 2508 | `case PH7_OP_SHL_STORE:` |
|        - | 2509 | `case PH7_OP_SHR_STORE: {` |
|        - | 2510 | `	VmOpRc rcOp;` |
|       19 | 2511 | `	sState.pTos = pTos;` |
|       19 | 2512 | `	sState.pc = pc;` |
|       19 | 2513 | `	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);` |
|       19 | 2514 | `	pTos = sState.pTos;` |
|       19 | 2515 | `	pc = sState.pc;` |
|       19 | 2516 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2517 | `		goto Abort;` |
|       19 | 2518 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2519 | `		goto Exception;` |
|        - | 2520 | `	}` |
|       19 | 2521 | `	break;` |
|        - | 2522 | `					  }` |
|        - | 2523 | `/* CAT:  P1 * *` |
|        - | 2524 | ` *` |
|        - | 2525 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - | 2526 | ` * back.` |
|        - | 2527 | ` */` |
|    86496 | 2528 | `case PH7_OP_CAT:{` |
|        - | 2529 | `	ph7_value *pNos,*pCur;` |
|   172997 | 2530 | `	if( pInstr->iP1 < 1 ){` |
|   144329 | 2531 | `		pNos = &pTos[-1];` |
|    72167 | 2532 | `	}else{` |
|    28673 | 2533 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - | 2534 | `	}` |
|        - | 2535 | `#ifdef UNTRUST` |
|        - | 2536 | `	if( pNos < pStack ){` |
|        - | 2537 | `		goto Abort;` |
|        - | 2538 | `	}` |
|        - | 2539 | `#endif` |
|        - | 2540 | `	/* Force a string cast */` |
|   172997 | 2541 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1909 | 2542 | `		PH7_MemObjToString(pNos);` |
|      952 | 2543 | `	}` |
|   172997 | 2544 | `	pCur = &pNos[1];` |
|   349573 | 2545 | `	while( pCur <= pTos ){` |
|   176581 | 2546 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    52445 | 2547 | `			PH7_MemObjToString(pCur);` |
|    26220 | 2548 | `		}` |
|        - | 2549 | `		/* Perform the concatenation */` |
|   176581 | 2550 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   176433 | 2551 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - | 2552 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 | 2553 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 2554 | `				goto Abort;` |
|        - | 2555 | `			}` |
|    88214 | 2556 | `		}` |
|   176581 | 2557 | `		SyBlobRelease(&pCur->sBlob);` |
|   176581 | 2558 | `		pCur++;` |
|        5 | 2559 | `	}` |
|   172997 | 2560 | `	pTos = pNos;` |
|   172997 | 2561 | `	break;` |
|        - | 2562 | `				}` |
|        - | 2563 | `/*  CAT_STORE: * * *` |
|        - | 2564 | ` *` |
|        - | 2565 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - | 2566 | ` * back.` |
|        - | 2567 | ` */` |
|    13731 | 2568 | `case PH7_OP_CAT_STORE:{` |
|    27467 | 2569 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2570 | `	ph7_value *pObj;` |
|        - | 2571 | `	sxu32 nIdx;` |
|        - | 2572 | `#ifdef UNTRUST` |
|        - | 2573 | `	if( pNos < pStack ){` |
|        - | 2574 | `		goto Abort;` |
|        - | 2575 | `	}` |
|        - | 2576 | `#endif` |
|        - | 2577 | `	/* The right operand must be a string to append it */` |
|    27467 | 2578 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       59 | 2579 | `		PH7_MemObjToString(pNos);` |
|       29 | 2580 | `	}` |
|    27467 | 2581 | `	nIdx = pTos->nIdx;` |
|        - | 2582 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - | 2583 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - | 2584 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - | 2585 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - | 2586 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - | 2587 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - | 2588 | `	 * the source we copy from — references share the slot index, so one check` |
|        - | 2589 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - | 2590 | `	 * must run before any mutation (left to the slow path).` |
|        - | 2591 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - | 2592 | `	 * and remains O(n^2) by design. */` |
|    27462 | 2593 | `	if( nIdx != SXU32_HIGH` |
|    27462 | 2594 | `	 && nIdx != pNos->nIdx` |
|    27458 | 2595 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|    27459 | 2596 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|    16327 | 2597 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|    27453 | 2598 | `		if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 2599 | `			/* e.g. $x = 5; $x .= "a";  ->  "5a" */` |
|        6 | 2600 | `			PH7_MemObjToString(pObj);` |
|        2 | 2601 | `		}` |
|    27453 | 2602 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|    27449 | 2603 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 2604 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - | 2605 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 | 2606 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 2607 | `				goto Abort;` |
|        - | 2608 | `			}` |
|    13722 | 2609 | `		}` |
|        - | 2610 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - | 2611 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - | 2612 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - | 2613 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - | 2614 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - | 2615 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - | 2616 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - | 2617 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - | 2618 | `		 * the same slot is appended to again later in the statement` |
|        - | 2619 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - | 2620 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - | 2621 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|    27453 | 2622 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 | 2623 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 | 2624 | `		}` |
|        - | 2625 | `		/* A hooked lvalue's scratch slot was appended in place — dispatch the` |
|        - | 2626 | `		 * set side now (the consume reads the computed value from the slot). */` |
|    27453 | 2627 | `		PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|    27453 | 2628 | `		pNos->nIdx = SXU32_HIGH;` |
|    27453 | 2629 | `		VmPopOperand(&pTos,1);` |
|    27460 | 2630 | `		break;` |
|        - | 2631 | `	}` |
|        - | 2632 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|       16 | 2633 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 2634 | `		/* Force a string cast */` |
|        6 | 2635 | `		PH7_MemObjToString(pTos);` |
|        2 | 2636 | `	}` |
|        - | 2637 | `	/* Perform the concatenation (Reverse order) */` |
|       16 | 2638 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 | 2639 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 2640 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - | 2641 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 | 2642 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 2643 | `			goto Abort;` |
|        - | 2644 | `		}` |
|        7 | 2645 | `	}` |
|        - | 2646 | `	/* Perform the store operation */` |
|       16 | 2647 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 2648 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 | 2649 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       16 | 2650 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 | 2651 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 | 2652 | `	}` |
|       11 | 2653 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       11 | 2654 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 | 2655 | `	VmPopOperand(&pTos,1);` |
|       11 | 2656 | `	break;` |
|        - | 2657 | `				}` |
|        - | 2658 | `/* OP_AND: * * *` |
|        - | 2659 | ` *` |
|        - | 2660 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - | 2661 | ` * two values and push the resulting boolean value back onto the` |
|        - | 2662 | ` * stack.` |
|        - | 2663 | ` */` |
|        - | 2664 | `/* OP_OR: * * *` |
|        - | 2665 | ` *` |
|        - | 2666 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - | 2667 | ` * two values and push the resulting boolean value back onto the` |
|        - | 2668 | ` * stack.` |
|        - | 2669 | ` */` |
|   116311 | 2670 | `case PH7_OP_LAND:` |
|        - | 2671 | `case PH7_OP_LOR: {` |
|        - | 2672 | `	VmOpRc rcOp;` |
|   232836 | 2673 | `	sState.pTos = pTos;` |
|   232836 | 2674 | `	sState.pc = pc;` |
|   232836 | 2675 | `	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);` |
|   232836 | 2676 | `	pTos = sState.pTos;` |
|   232836 | 2677 | `	pc = sState.pc;` |
|   232836 | 2678 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2679 | `		goto Abort;` |
|   232836 | 2680 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2681 | `		goto Exception;` |
|        - | 2682 | `	}` |
|   232836 | 2683 | `	break;` |
|        - | 2684 | `					  }` |
|        - | 2685 | `/*` |
|        - | 2686 | ` * OP_NULLC: * * *` |
|        - | 2687 | ` * Null coalescing operator '??'.` |
|        - | 2688 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - | 2689 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - | 2690 | ` */` |
|        - | 2691 | `/*` |
|        - | 2692 | ` * OP_NULLC: * P2 *` |
|        - | 2693 | ` * Short-circuit null coalescing '??'.` |
|        - | 2694 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - | 2695 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - | 2696 | ` */` |
|      377 | 2697 | `case PH7_OP_NULLC: {` |
|        - | 2698 | `#ifdef UNTRUST` |
|        - | 2699 | `	if( pTos < pStack ){` |
|        - | 2700 | `		goto Abort;` |
|        - | 2701 | `	}` |
|        - | 2702 | `#endif` |
|      759 | 2703 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 2704 | `		/* Left is not null — keep it and skip the RHS */` |
|      435 | 2705 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|      220 | 2706 | `	}else{` |
|        - | 2707 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|      328 | 2708 | `		VmPopOperand(&pTos, 1);` |
|        - | 2709 | `	}` |
|      759 | 2710 | `	break;` |
|        - | 2711 | `}` |
|        - | 2712 | `/*` |
|        - | 2713 | ` * OP_NULLC_JMP: * P2 *` |
|        - | 2714 | ` * Null coalescing assignment short-circuit.` |
|        - | 2715 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - | 2716 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - | 2717 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - | 2718 | ` */` |
|       44 | 2719 | `case PH7_OP_NULLC_JMP: {` |
|        - | 2720 | `#ifdef UNTRUST` |
|        - | 2721 | `	if( pTos < pStack ){` |
|        - | 2722 | `		goto Abort;` |
|        - | 2723 | `	}` |
|        - | 2724 | `#endif` |
|       91 | 2725 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       30 | 2726 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —` |
|        - | 2727 | `		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is` |
|        - | 2728 | `		 * dropped by the fetch-point sweep (the landing pc is past its` |
|        - | 2729 | `		 * OP_NULLC_STORE window): the skipped assign never dispatches. */` |
|       14 | 2730 | `	}` |
|       91 | 2731 | `	break;` |
|        - | 2732 | `}` |
|        - | 2733 | `/*` |
|        - | 2734 | ` * OP_NULLC_STORE: * * *` |
|        - | 2735 | ` * Null coalescing assignment store.` |
|        - | 2736 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - | 2737 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - | 2738 | ` * expression result.` |
|        - | 2739 | ` */` |
|        - | 2740 | `/*` |
|        - | 2741 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - | 2742 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - | 2743 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - | 2744 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - | 2745 | ` * non-null, fall through without modifying the stack so the following` |
|        - | 2746 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - | 2747 | ` */` |
|       53 | 2748 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - | 2749 | `#ifdef UNTRUST` |
|        - | 2750 | `	if( pTos < pStack ){` |
|        - | 2751 | `		goto Abort;` |
|        - | 2752 | `	}` |
|        - | 2753 | `#endif` |
|      108 | 2754 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - | 2755 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - | 2756 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       44 | 2757 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       21 | 2758 | `	}` |
|      108 | 2759 | `	break;` |
|        - | 2760 | `}` |
|       27 | 2761 | `case PH7_OP_NULLC_STORE: {` |
|        - | 2762 | `	VmOpRc rcOp;` |
|       57 | 2763 | `	sState.pTos = pTos;` |
|       57 | 2764 | `	sState.pc = pc;` |
|       57 | 2765 | `	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);` |
|       57 | 2766 | `	pTos = sState.pTos;` |
|       57 | 2767 | `	pc = sState.pc;` |
|       57 | 2768 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2769 | `		goto Abort;` |
|       57 | 2770 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2771 | `		goto Exception;` |
|        - | 2772 | `	}` |
|       57 | 2773 | `	break;` |
|        - | 2774 | `					  }` |
|        - | 2775 | `/*` |
|        - | 2776 | ` * OP_SPREAD: * * *` |
|        - | 2777 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - | 2778 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - | 2779 | ` * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this` |
|        - | 2780 | ` * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —` |
|        - | 2781 | ` * the CALL may not be the next instruction, and may be an inner call whose own` |
|        - | 2782 | ` * spreads must stay scoped to it.` |
|        - | 2783 | ` * The expansion tail is shared between the plain-array and the materialized` |
|        - | 2784 | ` * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.` |
|        - | 2785 | ` */` |
|      150 | 2786 | `case PH7_OP_SPREAD: {` |
|        - | 2787 | `#ifdef UNTRUST` |
|        - | 2788 | `	if( pTos < pStack ){` |
|        - | 2789 | `		goto Abort;` |
|        - | 2790 | `	}` |
|        - | 2791 | `#endif` |
|        - | 2792 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - | 2793 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - | 2794 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - | 2795 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - | 2796 | `	 * be freed immediately. */` |
|      304 | 2797 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 | 2798 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - | 2799 | `		sxi32 rcW;` |
|        3 | 2800 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 | 2801 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 | 2802 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 | 2803 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 2804 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 2805 | `			goto Exception;` |
|        - | 2806 | `		}` |
|        - | 2807 | `		/* Grow the operand stack if this expansion would overflow it (no longer a` |
|        - | 2808 | `		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a` |
|        - | 2809 | `		 * single argument — the historical bounded-fallback, now only under OOM. */` |
|        4 | 2810 | `		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,` |
|        1 | 2811 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 2812 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 2813 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 2814 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 2815 | `				pTmpMap->nEntry);` |
|      ! 0 | 2816 | `			break;` |
|        - | 2817 | `		}` |
|        3 | 2818 | `		VmSpreadExpandMap(pVm, &pTos, pTmpMap);` |
|        3 | 2819 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 | 2820 | `		break;` |
|        - | 2821 | `	}` |
|      302 | 2822 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      302 | 2823 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      451 | 2824 | `		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,` |
|      149 | 2825 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 2826 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 2827 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 2828 | `				pMap->nEntry);` |
|      ! 0 | 2829 | `			break;` |
|        - | 2830 | `		}` |
|      302 | 2831 | `		VmSpreadExpandMap(pVm, &pTos, pMap);` |
|      149 | 2832 | `	}` |
|        - | 2833 | `	/* else: not an array — leave as-is (single arg) */` |
|      302 | 2834 | `	break;` |
|        - | 2835 | `}` |
|        - | 2836 | `/*` |
|        - | 2837 | ` * OP_FLAG_SPREAD: * * *` |
|        - | 2838 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - | 2839 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - | 2840 | ` */` |
|      340 | 2841 | `case PH7_OP_FLAG_SPREAD: {` |
|        - | 2842 | `#ifdef UNTRUST` |
|        - | 2843 | `	if( pTos < pStack ){` |
|        - | 2844 | `		goto Abort;` |
|        - | 2845 | `	}` |
|        - | 2846 | `#endif` |
|      682 | 2847 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|      682 | 2848 | `	break;` |
|        - | 2849 | `}` |
|        - | 2850 | `/* OP_LXOR: * * *` |
|        - | 2851 | ` *` |
|        - | 2852 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - | 2853 | ` * two values and push the resulting boolean value back onto the` |
|        - | 2854 | ` * stack.` |
|        - | 2855 | ` * According to the PHP language reference manual:` |
|        - | 2856 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - | 2857 | ` *  TRUE,but not both.` |
|        - | 2858 | ` */` |
|        5 | 2859 | `case PH7_OP_LXOR:{` |
|       11 | 2860 | `	ph7_value *pNos = &pTos[-1];` |
|       11 | 2861 | `	sxi32 v = 0;` |
|        - | 2862 | `#ifdef UNTRUST` |
|        - | 2863 | `	if( pNos < pStack ){` |
|        - | 2864 | `		goto Abort;` |
|        - | 2865 | `	}` |
|        - | 2866 | `#endif` |
|        - | 2867 | `	/* Force a boolean cast */` |
|       11 | 2868 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 2869 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 2870 | `	}` |
|       11 | 2871 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 2872 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 | 2873 | `	}` |
|       11 | 2874 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 | 2875 | `		v = 1;` |
|        3 | 2876 | `	}` |
|       11 | 2877 | `	VmPopOperand(&pTos,1);` |
|       11 | 2878 | `	pTos->x.iVal = v;` |
|       11 | 2879 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 | 2880 | `	break;` |
|        - | 2881 | `				 }` |
|        - | 2882 | `/* OP_EQ P1 P2 P3` |
|        - | 2883 | ` *` |
|        - | 2884 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - | 2885 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - | 2886 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2887 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2888 | ` */` |
|        - | 2889 | `/* OP_NEQ P1 P2 P3` |
|        - | 2890 | ` *` |
|        - | 2891 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - | 2892 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 2893 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2894 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2895 | ` */` |
|     5449 | 2896 | `case PH7_OP_EQ:` |
|        - | 2897 | `case PH7_OP_NEQ: {` |
|        - | 2898 | `	VmOpRc rcOp;` |
|    10903 | 2899 | `	sState.pTos = pTos;` |
|    10903 | 2900 | `	sState.pc = pc;` |
|    10903 | 2901 | `	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);` |
|    10903 | 2902 | `	pTos = sState.pTos;` |
|    10903 | 2903 | `	pc = sState.pc;` |
|    10903 | 2904 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2905 | `		goto Abort;` |
|    10903 | 2906 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2907 | `		goto Exception;` |
|        - | 2908 | `	}` |
|    10903 | 2909 | `	break;` |
|        - | 2910 | `					  }` |
|        - | 2911 | `/* OP_TEQ P1 P2 *` |
|        - | 2912 | ` *` |
|        - | 2913 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - | 2914 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 2915 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2916 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2917 | ` */` |
|   231053 | 2918 | `case PH7_OP_TEQ: {` |
|        - | 2919 | `	VmOpRc rcOp;` |
|   462320 | 2920 | `	sState.pTos = pTos;` |
|   462320 | 2921 | `	sState.pc = pc;` |
|   462320 | 2922 | `	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);` |
|   462320 | 2923 | `	pTos = sState.pTos;` |
|   462320 | 2924 | `	pc = sState.pc;` |
|   462320 | 2925 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2926 | `		goto Abort;` |
|   462320 | 2927 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2928 | `		goto Exception;` |
|        - | 2929 | `	}` |
|   462320 | 2930 | `	break;` |
|        - | 2931 | `					  }` |
|        - | 2932 | `/* OP_TNE P1 P2 *` |
|        - | 2933 | ` *` |
|        - | 2934 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - | 2935 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - | 2936 | ` * instruction.` |
|        - | 2937 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2938 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2939 | ` *` |
|        - | 2940 | ` */` |
|   192801 | 2941 | `case PH7_OP_TNE: {` |
|        - | 2942 | `	VmOpRc rcOp;` |
|   385816 | 2943 | `	sState.pTos = pTos;` |
|   385816 | 2944 | `	sState.pc = pc;` |
|   385816 | 2945 | `	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);` |
|   385816 | 2946 | `	pTos = sState.pTos;` |
|   385816 | 2947 | `	pc = sState.pc;` |
|   385816 | 2948 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2949 | `		goto Abort;` |
|   385816 | 2950 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2951 | `		goto Exception;` |
|        - | 2952 | `	}` |
|   385816 | 2953 | `	break;` |
|        - | 2954 | `					  }` |
|        - | 2955 | `/* OP_LT P1 P2 P3` |
|        - | 2956 | ` *` |
|        - | 2957 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 2958 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 2959 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 2960 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2961 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2962 | ` *` |
|        - | 2963 | ` */` |
|        - | 2964 | `/* OP_LE P1 P2 P3` |
|        - | 2965 | ` *` |
|        - | 2966 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 2967 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 2968 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 2969 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2970 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2971 | ` *` |
|        - | 2972 | ` */` |
|   148871 | 2973 | `case PH7_OP_LT:` |
|        - | 2974 | `case PH7_OP_LE: {` |
|        - | 2975 | `	VmOpRc rcOp;` |
|   298165 | 2976 | `	sState.pTos = pTos;` |
|   298165 | 2977 | `	sState.pc = pc;` |
|   298165 | 2978 | `	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);` |
|   298165 | 2979 | `	pTos = sState.pTos;` |
|   298165 | 2980 | `	pc = sState.pc;` |
|   298165 | 2981 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2982 | `		goto Abort;` |
|   298165 | 2983 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2984 | `		goto Exception;` |
|        - | 2985 | `	}` |
|   298165 | 2986 | `	break;` |
|        - | 2987 | `					  }` |
|        - | 2988 | `/* OP_GT P1 P2 P3` |
|        - | 2989 | ` *` |
|        - | 2990 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 2991 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 2992 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 2993 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 2994 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 2995 | ` *` |
|        - | 2996 | ` */` |
|        - | 2997 | `/* OP_GE P1 P2 P3` |
|        - | 2998 | ` *` |
|        - | 2999 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 3000 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 3001 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 3002 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3003 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3004 | ` *` |
|        - | 3005 | ` */` |
|    88240 | 3006 | `case PH7_OP_GT:` |
|        - | 3007 | `case PH7_OP_GE: {` |
|        - | 3008 | `	VmOpRc rcOp;` |
|   176694 | 3009 | `	sState.pTos = pTos;` |
|   176694 | 3010 | `	sState.pc = pc;` |
|   176694 | 3011 | `	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);` |
|   176694 | 3012 | `	pTos = sState.pTos;` |
|   176694 | 3013 | `	pc = sState.pc;` |
|   176694 | 3014 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3015 | `		goto Abort;` |
|   176694 | 3016 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3017 | `		goto Exception;` |
|        - | 3018 | `	}` |
|   176694 | 3019 | `	break;` |
|        - | 3020 | `					  }` |
|        - | 3021 | `/* OP_SPACESHIP * * *` |
|        - | 3022 | ` *` |
|        - | 3023 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - | 3024 | ` *   -1 if left < right` |
|        - | 3025 | ` *    0 if left == right` |
|        - | 3026 | ` *    1 if left > right` |
|        - | 3027 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - | 3028 | ` */` |
|      213 | 3029 | `case PH7_OP_SPACESHIP: {` |
|        - | 3030 | `	VmOpRc rcOp;` |
|      427 | 3031 | `	sState.pTos = pTos;` |
|      427 | 3032 | `	sState.pc = pc;` |
|      427 | 3033 | `	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);` |
|      427 | 3034 | `	pTos = sState.pTos;` |
|      427 | 3035 | `	pc = sState.pc;` |
|      427 | 3036 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3037 | `		goto Abort;` |
|      427 | 3038 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3039 | `		goto Exception;` |
|        - | 3040 | `	}` |
|      427 | 3041 | `	break;` |
|        - | 3042 | `					  }` |
|        - | 3043 | `/*` |
|        - | 3044 | ` * OP_LOAD_REF * * *` |
|        - | 3045 | ` * Push the index of a referenced object on the stack.` |
|        - | 3046 | ` */` |
|       60 | 3047 | `case PH7_OP_LOAD_REF: {` |
|        - | 3048 | `	sxu32 nIdx;` |
|        - | 3049 | `#ifdef UNTRUST` |
|        - | 3050 | `	if( pTos < pStack ){` |
|        - | 3051 | `		goto Abort;` |
|        - | 3052 | `	}` |
|        - | 3053 | `#endif` |
|        - | 3054 | `	/* Extract memory object index */` |
|      121 | 3055 | `	nIdx = pTos->nIdx;` |
|      121 | 3056 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - | 3057 | `		/* Nullify the object */` |
|      121 | 3058 | `		PH7_MemObjRelease(pTos);` |
|        - | 3059 | `		/* Mark as constant and store the index on the top of the stack */` |
|      121 | 3060 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      121 | 3061 | `		pTos->nIdx = SXU32_HIGH;` |
|      121 | 3062 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       60 | 3063 | `	}` |
|      121 | 3064 | `	break;` |
|        - | 3065 | `					  }` |
|        - | 3066 | `/*` |
|        - | 3067 | ` * OP_STORE_REF * * P3` |
|        - | 3068 | ` * Perform an assignment operation by reference.` |
|        - | 3069 | ` */` |
|       26 | 3070 | `case PH7_OP_STORE_REF: {` |
|        - | 3071 | `	VmOpRc rcOp;` |
|       55 | 3072 | `	sState.pTos = pTos;` |
|       55 | 3073 | `	sState.pc = pc;` |
|       55 | 3074 | `	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);` |
|       55 | 3075 | `	pTos = sState.pTos;` |
|       55 | 3076 | `	pc = sState.pc;` |
|       55 | 3077 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3078 | `		goto Abort;` |
|       53 | 3079 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3080 | `		goto Exception;` |
|        - | 3081 | `	}` |
|       53 | 3082 | `	break;` |
|        - | 3083 | `					  }` |
|        - | 3084 | `/*` |
|        - | 3085 | ` * OP_UPLINK P1 * *` |
|        - | 3086 | ` * Link a variable to the top active VM frame.` |
|        - | 3087 | ` * This is used to implement the 'global' PHP construct.` |
|        - | 3088 | ` */` |
|       27 | 3089 | `case PH7_OP_UPLINK: {` |
|       59 | 3090 | `	if( pVm->pFrame->pParent ){` |
|       59 | 3091 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - | 3092 | `		SyString sName;` |
|        - | 3093 | `		/* Perform the link */` |
|      123 | 3094 | `		while( pLink <= pTos ){` |
|       69 | 3095 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 3096 | `				/* Force a string cast */` |
|      ! 0 | 3097 | `				PH7_MemObjToString(pLink);` |
|      ! 0 | 3098 | `			}` |
|       69 | 3099 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       69 | 3100 | `			if( sName.nByte > 0 ){` |
|       69 | 3101 | `				VmFrameLink(&(*pVm),&sName);` |
|       32 | 3102 | `			}` |
|       69 | 3103 | `			pLink++;` |
|        5 | 3104 | `		}` |
|       27 | 3105 | `	}` |
|       59 | 3106 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       59 | 3107 | `	break;` |
|        - | 3108 | `					}` |
|        - | 3109 | `/*` |
|        - | 3110 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - | 3111 | ` * Push an exception in the corresponding container so that` |
|        - | 3112 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - | 3113 | ` */` |
|     1391 | 3114 | `case PH7_OP_LOAD_EXCEPTION: {` |
|        - | 3115 | `	VmOpRc rcOp;` |
|     2787 | 3116 | `	sState.pTos = pTos;` |
|     2787 | 3117 | `	sState.pc = pc;` |
|     2787 | 3118 | `	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);` |
|     2787 | 3119 | `	pTos = sState.pTos;` |
|     2787 | 3120 | `	pc = sState.pc;` |
|     2787 | 3121 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3122 | `		goto Abort;` |
|     2787 | 3123 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3124 | `		goto Exception;` |
|        - | 3125 | `	}` |
|     2787 | 3126 | `	break;` |
|        - | 3127 | `					  }` |
|        - | 3128 | `/*` |
|        - | 3129 | ` * OP_POP_EXCEPTION * * P3` |
|        - | 3130 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - | 3131 | ` */` |
|     1224 | 3132 | `case PH7_OP_POP_EXCEPTION: {` |
|     2453 | 3133 | `	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;` |
|        - | 3134 | `	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on` |
|        - | 3135 | `	 * top (matched by compiled origin). pException == NULL means this try's` |
|        - | 3136 | `	 * activation was already consumed (an in-place catch handled a throw and` |
|        - | 3137 | `	 * ran the finally itself); compiled fields keep coming from p3. */` |
|     2453 | 3138 | `	ph7_exception *pException = 0;` |
|     2453 | 3139 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|      407 | 3140 | `		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      407 | 3141 | `		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];` |
|        - | 3142 | `		/* Same compiled origin is NOT enough: under recursion, when THIS` |
|        - | 3143 | `		 * level's activation was already consumed by an in-place catch (the` |
|        - | 3144 | `		 * resume lands right here), the top can be an OUTER level's activation` |
|        - | 3145 | `		 * of the same lexical try — popping it would run that level's finally` |
|        - | 3146 | `		 * early and orphan its handler (probed: recursive try/catch/finally` |
|        - | 3147 | `		 * lost the outer catch entirely). The activation must also belong to` |
|        - | 3148 | `		 * the CURRENT body frame. */` |
|      402 | 3149 | `		if( VmExcMatches(pTop,pCompiledExc)` |
|      403 | 3150 | `		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){` |
|      393 | 3151 | `			pException = pTop;` |
|      393 | 3152 | `			(void)SySetPop(&pVm->aException);` |
|      194 | 3153 | `		}` |
|      201 | 3154 | `	}` |
|     2453 | 3155 | `	if( pCompiledExc->iInlined ){` |
|        - | 3156 | `		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.` |
|        - | 3157 | `		 * Pop this try's handler off aException so a throw in the finally propagates to` |
|        - | 3158 | `		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the` |
|        - | 3159 | `		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the` |
|        - | 3160 | `		 * compiler-emitted JMP falls into the finally. Without a finally, this is the` |
|        - | 3161 | `		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */` |
|      159 | 3162 | `		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */` |
|      159 | 3163 | `		if( pCompiledExc->iHasFinally ){` |
|        - | 3164 | `			VmFinallyAction sAct;` |
|       12 | 3165 | `			SyZero(&sAct,sizeof(sAct));` |
|       12 | 3166 | `			sAct.eKind = PH7_FA_FALLTHROUGH;` |
|       12 | 3167 | `			sAct.iNextPc = pCompiledExc->iEndCatchPc;` |
|       12 | 3168 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        - | 3169 | `			/* keep the transparent frame for OP_END_FINALLY */` |
|      154 | 3170 | `		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       18 | 3171 | `			VmLeaveFrame(&(*pVm));` |
|        7 | 3172 | `		}` |
|      159 | 3173 | `		break;` |
|        - | 3174 | `	}` |
|        - | 3175 | `	/* Leave the exception frame. It is normally on top here (a try that fell through` |
|        - | 3176 | `	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a` |
|        - | 3177 | `	 * RESUMED generator/fiber body whose yield was inside this try, that exception` |
|        - | 3178 | `	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),` |
|        - | 3179 | `	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the` |
|        - | 3180 | `	 * coroutine (and defeat the bHasRet materialization just below, which must see the` |
|        - | 3181 | `	 * body). Only leave a genuine exception frame. */` |
|     2299 | 3182 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|     1671 | 3183 | `		VmLeaveFrame(&(*pVm));` |
|      833 | 3184 | `	}` |
|        - | 3185 | `	/* Execute the finally block if present and not already executed by the` |
|        - | 3186 | `	 * catch path. No live activation (pException == NULL) means an in-place` |
|        - | 3187 | `	 * catch consumed it — and that path runs the finally itself — so skip. */` |
|     2299 | 3188 | `	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){` |
|        - | 3189 | `		sxi32 rcFinally;` |
|       33 | 3190 | `		VmExcRelease(&(*pVm),pException);` |
|       33 | 3191 | `		pException = 0;` |
|       33 | 3192 | `		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);` |
|       33 | 3193 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 | 3194 | `			goto Abort;` |
|        - | 3195 | `		}` |
|       33 | 3196 | `		if( rcFinally == PH7_EXCEPTION ){` |
|        - | 3197 | `			/* The finally threw past itself. If an enclosing try IN THIS function` |
|        - | 3198 | `			 * caught the new exception in place, resume at its landing pad;` |
|        - | 3199 | `			 * otherwise it was caught at an outer frame, so unwind this function. */` |
|        - | 3200 | `			sxi32 iResumePc;` |
|        5 | 3201 | `			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 3202 | `				pc = iResumePc;` |
|        3 | 3203 | `				break;` |
|        - | 3204 | `			}` |
|        3 | 3205 | `			goto Exception;` |
|        - | 3206 | `		}` |
|       12 | 3207 | `	}` |
|     2295 | 3208 | `	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */` |
|     2295 | 3209 | `	if( VmSkipExceptionFrames(pVm->pFrame)->bHasRet ){` |
|        - | 3210 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - | 3211 | `		 * function. The return targets the body frame this try belongs to. Drain` |
|        - | 3212 | `		 * outer finally blocks first, then — only in the real function body` |
|        - | 3213 | `		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline` |
|        - | 3214 | `		 * try within a catch/finally) propagate outward so the owning body returns. */` |
|      167 | 3215 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|      167 | 3216 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3217 | `			goto Abort;` |
|        - | 3218 | `		}` |
|      167 | 3219 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 3220 | `			goto Exception;` |
|        - | 3221 | `		}` |
|      167 | 3222 | `		if( !sState.bReturnPropagates ){` |
|      161 | 3223 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|       78 | 3224 | `		}` |
|      167 | 3225 | `		goto Done;` |
|        - | 3226 | `	}` |
|     2133 | 3227 | `	break;` |
|        - | 3228 | `							}` |
|        - | 3229 | `/*` |
|        - | 3230 | ` * OP_CATCH iP1(catch-index) * P3(ph7_exception)` |
|        - | 3231 | ` * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on` |
|        - | 3232 | ` * pException->pInflight by VmThrowInline) into the catch variable, resolved in the` |
|        - | 3233 | ` * enclosing body's scope (PHP: a catch shares the surrounding variable scope).` |
|        - | 3234 | ` */` |
|       33 | 3235 | `case PH7_OP_CATCH: {` |
|        - | 3236 | `	VmOpRc rcOp;` |
|       71 | 3237 | `	sState.pTos = pTos;` |
|       71 | 3238 | `	sState.pc = pc;` |
|       71 | 3239 | `	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);` |
|       71 | 3240 | `	pTos = sState.pTos;` |
|       71 | 3241 | `	pc = sState.pc;` |
|       71 | 3242 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3243 | `		goto Abort;` |
|       71 | 3244 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3245 | `		goto Exception;` |
|        - | 3246 | `	}` |
|       71 | 3247 | `	break;` |
|        - | 3248 | `					  }` |
|        - | 3249 | `/*` |
|        - | 3250 | ` * OP_END_FINALLY * P3(ph7_exception)` |
|        - | 3251 | ` * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch` |
|        - | 3252 | ` * the pending action queued when the finally was entered (fall-through / re-throw /` |
|        - | 3253 | ` * return / break-continue). A return/break threads out through each enclosing finally` |
|        - | 3254 | ` * via pException->iNextFinallyPc.` |
|        - | 3255 | ` */` |
|       22 | 3256 | `case PH7_OP_END_FINALLY: {` |
|       48 | 3257 | `	ph7_exception *pExc = (ph7_exception *)pInstr->p3;` |
|        - | 3258 | `	VmFinallyAction sAct;` |
|       48 | 3259 | `	int eKind = PH7_FA_FALLTHROUGH;` |
|        - | 3260 | `	/* Leave the try's transparent frame kept alive across the finally. */` |
|       48 | 3261 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        3 | 3262 | `		VmLeaveFrame(&(*pVm));` |
|        1 | 3263 | `	}` |
|       48 | 3264 | `	if( SySetUsed(&pVm->aFinallyAction) > 0 ){` |
|       48 | 3265 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);` |
|       48 | 3266 | `		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];` |
|       48 | 3267 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|       48 | 3268 | `		eKind = sAct.eKind;` |
|       26 | 3269 | `	}else{` |
|      ! 0 | 3270 | `		SyZero(&sAct,sizeof(sAct));` |
|      ! 0 | 3271 | `		sAct.iNextPc = pExc->iEndCatchPc;` |
|        - | 3272 | `	}` |
|       48 | 3273 | `	if( eKind == PH7_FA_FALLTHROUGH ){` |
|       10 | 3274 | `		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;` |
|       13 | 3275 | `		break;` |
|       40 | 3276 | `	}else if( eKind == PH7_FA_JMP ){` |
|        - | 3277 | `		/* break/continue: run the remaining crossed finallys, then take the jump. */` |
|        3 | 3278 | `		sxu32 iFpc = 0;` |
|        3 | 3279 | `		int nCross = sAct.nCross;` |
|        3 | 3280 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|      ! 0 | 3281 | `			sAct.nCross = nCross;` |
|      ! 0 | 3282 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      ! 0 | 3283 | `			pc = (sxi32)iFpc - 1;` |
|      ! 0 | 3284 | `			break;` |
|        - | 3285 | `		}` |
|        3 | 3286 | `		pc = (sxi32)sAct.iNextPc - 1;` |
|        3 | 3287 | `		break;` |
|       38 | 3288 | `	}else if( eKind == PH7_FA_RETHROW ){` |
|        8 | 3289 | `		ph7_class_instance *pRe = sAct.pExc;` |
|        - | 3290 | `		sxi32 _iRpE;` |
|        8 | 3291 | `		rc = VmThrowException(&(*pVm),pRe);` |
|        8 | 3292 | `		if( pRe ){ PH7_ClassInstanceUnref(pRe); }` |
|        8 | 3293 | `		if( rc == SXERR_ABORT ){ goto Abort; }` |
|        8 | 3294 | `		PH7_INLINE_RESUME_BREAK()` |
|        8 | 3295 | `		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }` |
|        8 | 3296 | `		goto Exception;` |
|      ! 0 | 3297 | `	}else{ /* PH7_FA_RETURN */` |
|       31 | 3298 | `		sxu32 iFpc = 0;` |
|       31 | 3299 | `		int nCross = sAct.nCross;` |
|       31 | 3300 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        - | 3301 | `			/* Thread the return through the next enclosing finally. */` |
|        6 | 3302 | `			sAct.nCross = nCross;` |
|        6 | 3303 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        6 | 3304 | `			pc = (sxi32)iFpc - 1;` |
|        6 | 3305 | `			break;` |
|        - | 3306 | `		}` |
|        - | 3307 | `		/* No enclosing finally left: materialize the return from this body. */` |
|       27 | 3308 | `		if( sAct.bHasRetVal && sState.pResult ){` |
|        6 | 3309 | `			PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|        2 | 3310 | `		}` |
|       27 | 3311 | `		PH7_MemObjRelease(&sAct.sRet);` |
|       27 | 3312 | `		goto Done;` |
|        - | 3313 | `	}` |
|        - | 3314 | `						 }` |
|        - | 3315 | `/*` |
|        - | 3316 | ` * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)` |
|        - | 3317 | `` * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a`` |
|        - | 3318 | ` * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);` |
|        - | 3319 | ` * OP_END_FINALLY threads it out through the finally chain and then returns.` |
|        - | 3320 | ` */` |
|       15 | 3321 | `case PH7_OP_SET_FINALLY_RET: {` |
|        - | 3322 | `	VmFinallyAction sAct;` |
|       34 | 3323 | `	sxu32 iFpc = 0;` |
|       34 | 3324 | `	int nCross = -1; /* a return crosses every enclosing finally in this function */` |
|       34 | 3325 | `	SyZero(&sAct,sizeof(sAct));` |
|       34 | 3326 | `	sAct.eKind = PH7_FA_RETURN;` |
|       34 | 3327 | `	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       34 | 3328 | `	PH7_MemObjInit(pVm,&sAct.sRet);` |
|       34 | 3329 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|       30 | 3330 | `		PH7_MemObjStore(pTos,&sAct.sRet);` |
|       30 | 3331 | `		sAct.bHasRetVal = 1;` |
|       30 | 3332 | `		VmPopOperand(&pTos,1);` |
|       13 | 3333 | `	}` |
|       34 | 3334 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        9 | 3335 | `		sAct.nCross = nCross;` |
|        9 | 3336 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        9 | 3337 | `		pc = (sxi32)iFpc - 1;` |
|        9 | 3338 | `		break;` |
|        - | 3339 | `	}` |
|        - | 3340 | `	/* No enclosing finally left: return now. */` |
|       28 | 3341 | `	if( sAct.bHasRetVal && sState.pResult ){` |
|       26 | 3342 | `		PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|       11 | 3343 | `	}` |
|       28 | 3344 | `	PH7_MemObjRelease(&sAct.sRet);` |
|       28 | 3345 | `	goto Done;` |
|        - | 3346 | `						 }` |
|        - | 3347 | `/*` |
|        - | 3348 | ` * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)` |
|        - | 3349 | `` * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action`` |
|        - | 3350 | ` * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.` |
|        - | 3351 | ` */` |
|        1 | 3352 | `case PH7_OP_SET_FINALLY_JMP: {` |
|        - | 3353 | `	VmFinallyAction sAct;` |
|        3 | 3354 | `	sxu32 iFpc = 0;` |
|        3 | 3355 | `	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */` |
|        3 | 3356 | `	SyZero(&sAct,sizeof(sAct));` |
|        3 | 3357 | `	sAct.eKind = PH7_FA_JMP;` |
|        3 | 3358 | `	sAct.iNextPc = pInstr->iP2;` |
|        3 | 3359 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        3 | 3360 | `		sAct.nCross = nCross;` |
|        3 | 3361 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        3 | 3362 | `		pc = (sxi32)iFpc - 1;` |
|        3 | 3363 | `		break;` |
|        - | 3364 | `	}` |
|        - | 3365 | `	/* No finally among the crossed trys: just take the break/continue jump. */` |
|      ! 0 | 3366 | `	pc = (sxi32)sAct.iNextPc - 1;` |
|      ! 0 | 3367 | `	break;` |
|        - | 3368 | `						 }` |
|        - | 3369 | `/*` |
|        - | 3370 | ` * OP_THROW * P2 *` |
|        - | 3371 | ` * Throw an user exception.` |
|        - | 3372 | ` */` |
|      377 | 3373 | `case PH7_OP_THROW: {` |
|        - | 3374 | `	VmOpRc rcOp;` |
|      759 | 3375 | `	sState.pTos = pTos;` |
|      759 | 3376 | `	sState.pc = pc;` |
|      759 | 3377 | `	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);` |
|      759 | 3378 | `	pTos = sState.pTos;` |
|      759 | 3379 | `	pc = sState.pc;` |
|      759 | 3380 | `	if( rcOp == VM_OP_ABORT ){` |
|       32 | 3381 | `		goto Abort;` |
|      731 | 3382 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      415 | 3383 | `		goto Exception;` |
|        - | 3384 | `	}` |
|      321 | 3385 | `	break;` |
|        - | 3386 | `					  }` |
|        - | 3387 | `/*` |
|        - | 3388 | ` * OP_FOREACH_INIT * P2 P3` |
|        - | 3389 | ` * Prepare a foreach step.` |
|        - | 3390 | ` */` |
|    12038 | 3391 | `case PH7_OP_FOREACH_INIT: {` |
|        - | 3392 | `	VmOpRc rcOp;` |
|    24081 | 3393 | `	sState.pTos = pTos;` |
|    24081 | 3394 | `	sState.pc = pc;` |
|    24081 | 3395 | `	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);` |
|    24081 | 3396 | `	pTos = sState.pTos;` |
|    24081 | 3397 | `	pc = sState.pc;` |
|    24081 | 3398 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3399 | `		goto Abort;` |
|    24081 | 3400 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3401 | `		goto Exception;` |
|        - | 3402 | `	}` |
|    24081 | 3403 | `	break;` |
|        - | 3404 | `					  }` |
|        - | 3405 | `/*` |
|        - | 3406 | ` * OP_FOREACH_STEP * P2 P3` |
|        - | 3407 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - | 3408 | ` */` |
|   121291 | 3409 | `case PH7_OP_FOREACH_STEP: {` |
|        - | 3410 | `	VmOpRc rcOp;` |
|   242587 | 3411 | `	sState.pTos = pTos;` |
|   242587 | 3412 | `	sState.pc = pc;` |
|   242587 | 3413 | `	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);` |
|   242587 | 3414 | `	pTos = sState.pTos;` |
|   242587 | 3415 | `	pc = sState.pc;` |
|   242587 | 3416 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3417 | `		goto Abort;` |
|   242585 | 3418 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3419 | `		goto Exception;` |
|        - | 3420 | `	}` |
|   242585 | 3421 | `	break;` |
|        - | 3422 | `						  }` |
|        - | 3423 | `/*` |
|        - | 3424 | ` * OP_MEMBER P1 P2` |
|        - | 3425 | ` * Load class attribute/method on the stack.` |
|        - | 3426 | ` */` |
|    22549 | 3427 | `case PH7_OP_MEMBER: {` |
|        - | 3428 | `	VmOpRc rcOp;` |
|    45103 | 3429 | `	sState.pTos = pTos;` |
|    45103 | 3430 | `	sState.pc = pc;` |
|    45103 | 3431 | `	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);` |
|    45103 | 3432 | `	pTos = sState.pTos;` |
|    45103 | 3433 | `	pc = sState.pc;` |
|    45103 | 3434 | `	if( rcOp == VM_OP_ABORT ){` |
|        7 | 3435 | `		goto Abort;` |
|    45097 | 3436 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       11 | 3437 | `		goto Exception;` |
|        - | 3438 | `	}` |
|    45087 | 3439 | `	break;` |
|        - | 3440 | `					  }` |
|        - | 3441 | `/*` |
|        - | 3442 | ` * OP_NEW P1 * * *` |
|        - | 3443 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - | 3444 | ` */` |
|     2393 | 3445 | `case PH7_OP_NEW: {` |
|        - | 3446 | `	VmOpRc rcOp;` |
|     4791 | 3447 | `	sState.pTos = pTos;` |
|     4791 | 3448 | `	sState.pc = pc;` |
|     4791 | 3449 | `	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);` |
|     4791 | 3450 | `	pTos = sState.pTos;` |
|     4791 | 3451 | `	pc = sState.pc;` |
|     4791 | 3452 | `	if( rcOp == VM_OP_ABORT ){` |
|        5 | 3453 | `		goto Abort;` |
|     4787 | 3454 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 3455 | `		goto Exception;` |
|        - | 3456 | `	}` |
|     4779 | 3457 | `	break;` |
|        - | 3458 | `					  }` |
|        - | 3459 | `/*` |
|        - | 3460 | ` * OP_CLONE * * *` |
|        - | 3461 | ` * Perfome a clone operation.` |
|        - | 3462 | ` */` |
|      107 | 3463 | `case PH7_OP_CLONE: {` |
|        - | 3464 | `	VmOpRc rcOp;` |
|      219 | 3465 | `	sState.pTos = pTos;` |
|      219 | 3466 | `	sState.pc = pc;` |
|      219 | 3467 | `	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);` |
|      219 | 3468 | `	pTos = sState.pTos;` |
|      219 | 3469 | `	pc = sState.pc;` |
|      219 | 3470 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3471 | `		goto Abort;` |
|      219 | 3472 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 3473 | `		goto Exception;` |
|        - | 3474 | `	}` |
|      217 | 3475 | `	break;` |
|        - | 3476 | `					  }` |
|        - | 3477 | `/*` |
|        - | 3478 | ` * OP_CLONE_APPLY * * *` |
|        - | 3479 | ` *  Apply the PHP 8.5 clone($obj, $withProperties) property updates. The updates` |
|        - | 3480 | ` *  array is on the stack top and the freshly-cloned object (from OP_CLONE) is` |
|        - | 3481 | ` *  directly below it. Each entry is applied as a scope-aware property write` |
|        - | 3482 | ` *  (AFTER __clone() has already run); the array is then popped, leaving the` |
|        - | 3483 | ` *  clone as the result.` |
|        - | 3484 | ` */` |
|        8 | 3485 | `case PH7_OP_CLONE_APPLY: {` |
|        - | 3486 | `	VmOpRc rcOp;` |
|       17 | 3487 | `	sState.pTos = pTos;` |
|       17 | 3488 | `	sState.pc = pc;` |
|       17 | 3489 | `	rcOp = VmExecOpCloneApply(&(*pVm),&sState,pInstr);` |
|       17 | 3490 | `	pTos = sState.pTos;` |
|       17 | 3491 | `	pc = sState.pc;` |
|       17 | 3492 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3493 | `		goto Abort;` |
|       17 | 3494 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3495 | `		goto Exception;` |
|        - | 3496 | `	}` |
|       17 | 3497 | `	break;` |
|        - | 3498 | `					  }` |
|        - | 3499 | `/*` |
|        - | 3500 | ` * OP_SWITCH * * P3` |
|        - | 3501 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - | 3502 | ` */` |
|      162 | 3503 | `case PH7_OP_SWITCH: {` |
|        - | 3504 | `	VmOpRc rcOp;` |
|      329 | 3505 | `	sState.pTos = pTos;` |
|      329 | 3506 | `	sState.pc = pc;` |
|      329 | 3507 | `	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);` |
|      329 | 3508 | `	pTos = sState.pTos;` |
|      329 | 3509 | `	pc = sState.pc;` |
|      329 | 3510 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3511 | `		goto Abort;` |
|      329 | 3512 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3513 | `		goto Exception;` |
|        - | 3514 | `	}` |
|      329 | 3515 | `	break;` |
|        - | 3516 | `					  }` |
|        - | 3517 | `/*` |
|        - | 3518 | ` * OP_MATCH * * P3` |
|        - | 3519 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - | 3520 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - | 3521 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - | 3522 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - | 3523 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - | 3524 | ` */` |
|       56 | 3525 | `case PH7_OP_MATCH: {` |
|        - | 3526 | `	VmOpRc rcOp;` |
|      115 | 3527 | `	sState.pTos = pTos;` |
|      115 | 3528 | `	sState.pc = pc;` |
|      115 | 3529 | `	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);` |
|      115 | 3530 | `	pTos = sState.pTos;` |
|      115 | 3531 | `	pc = sState.pc;` |
|      115 | 3532 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3533 | `		goto Abort;` |
|      115 | 3534 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        6 | 3535 | `		goto Exception;` |
|        - | 3536 | `	}` |
|      109 | 3537 | `	break;` |
|        - | 3538 | `					  }` |
|        - | 3539 | `/*` |
|        - | 3540 | ` * OP_YIELD P1 P2 *` |
|        - | 3541 | ` *  Yield a value from a generator function.` |
|        - | 3542 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - | 3543 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - | 3544 | ` */` |
|      582 | 3545 | `case PH7_OP_YIELD: {` |
|        - | 3546 | `	ph7_generator *pGen;` |
|     1169 | 3547 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 3548 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 | 3549 | `		goto Abort;` |
|        - | 3550 | `	}` |
|     1169 | 3551 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 3552 | ``		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's`` |
|        - | 3553 | `		 * pending finallys tried to yield — PHP forbids it. */` |
|      ! 0 | 3554 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 3555 | `			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 3556 | `			goto Abort;` |
|        - | 3557 | `		}` |
|      ! 0 | 3558 | `		goto Exception;` |
|        - | 3559 | `	}` |
|     1169 | 3560 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|     1169 | 3561 | `	if( pInstr->iP2 ){` |
|        - | 3562 | `		/* yield $key => $value: value on top, key below */` |
|        - | 3563 | `#ifdef UNTRUST` |
|        - | 3564 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - | 3565 | `#endif` |
|       70 | 3566 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       70 | 3567 | `		VmPopOperand(&pTos, 1);` |
|       70 | 3568 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       70 | 3569 | `		VmPopOperand(&pTos, 1);` |
|        - | 3570 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       70 | 3571 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|       47 | 3572 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|       47 | 3573 | `			if( nKey >= pGen->iImplicitKey ){` |
|       47 | 3574 | `				pGen->iImplicitKey = nKey + 1;` |
|       23 | 3575 | `			}` |
|       25 | 3576 | `		}` |
|     1135 | 3577 | `	}else if( pInstr->iP1 ){` |
|        - | 3578 | `		/* yield $value */` |
|        - | 3579 | `#ifdef UNTRUST` |
|        - | 3580 | `		if( pTos < pStack ) goto Abort;` |
|        - | 3581 | `#endif` |
|     1101 | 3582 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|     1101 | 3583 | `		VmPopOperand(&pTos, 1);` |
|        - | 3584 | `		/* Auto-increment key */` |
|     1101 | 3585 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|     1101 | 3586 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|     1101 | 3587 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|      553 | 3588 | `	}else{` |
|        - | 3589 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 | 3590 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 3591 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 3592 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 | 3593 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - | 3594 | `	}` |
|        - | 3595 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|     1169 | 3596 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|     1169 | 3597 | `	goto Suspend;` |
|        - | 3598 | `}` |
|        - | 3599 | `/*` |
|        - | 3600 | ` * OP_YIELD_FROM * * *` |
|        - | 3601 | ` *` |
|        - | 3602 | `` * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an`` |
|        - | 3603 | ` * array/Traversable/Generator from the OUTER generator, preserving the inner` |
|        - | 3604 | ` * keys; the expression evaluates to the inner Generator's return value (or NULL).` |
|        - | 3605 | ` *` |
|        - | 3606 | ` * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,` |
|        - | 3607 | ` * advances the per-instance delegate cursor stored on the exec context (never the` |
|        - | 3608 | ` * shared foreach aStep, so independent generator instances cannot clash). The` |
|        - | 3609 | ` * iterable operand is consumed on first entry; the expression result is pushed at` |
|        - | 3610 | ` * exhaustion — net stack effect +1, identical to OP_YIELD.` |
|        - | 3611 | ` */` |
|       90 | 3612 | `case PH7_OP_YIELD_FROM: {` |
|        - | 3613 | `	ph7_generator *pGenFrom;` |
|        - | 3614 | `	ph7_exec_ctx *pCtxFrom;` |
|        - | 3615 | `	ph7_value sKey,sVal;` |
|      185 | 3616 | `	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */` |
|      185 | 3617 | `	int bExhausted = 0;` |
|      185 | 3618 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 3619 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");` |
|      ! 0 | 3620 | `		goto Abort;` |
|        - | 3621 | `	}` |
|      185 | 3622 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 3623 | ``		/* A `yield from` reached while VmCloseCtx force-drives this destroyed`` |
|        - | 3624 | `		 * generator's pending finallys — PHP forbids it (distinct message from a` |
|        - | 3625 | ``		 * bare `yield`, mirroring the OP_YIELD guard above). */`` |
|      ! 0 | 3626 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 3627 | `			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 3628 | `			goto Abort;` |
|        - | 3629 | `		}` |
|      ! 0 | 3630 | `		goto Exception;` |
|        - | 3631 | `	}` |
|      185 | 3632 | `	pCtxFrom = pVm->pActiveCtx;` |
|      185 | 3633 | `	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;` |
|      185 | 3634 | `	PH7_MemObjInit(pVm,&sKey);` |
|      185 | 3635 | `	PH7_MemObjInit(pVm,&sVal);` |
|      185 | 3636 | `	if( pCtxFrom->iDelegateState == 0 ){` |
|        - | 3637 | `		/* First entry: classify the iterable on the stack top. */` |
|       77 | 3638 | `		int bIterable = 1;` |
|        - | 3639 | `#ifdef UNTRUST` |
|        - | 3640 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 3641 | `#endif` |
|       77 | 3642 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       26 | 3643 | `			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       26 | 3644 | `			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;` |
|       26 | 3645 | `			pCtxFrom->iDelegateState = 1;` |
|       66 | 3646 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       51 | 3647 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       51 | 3648 | `			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       51 | 3649 | `			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){` |
|       41 | 3650 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       41 | 3651 | `				pCtxFrom->iDelegateState = 3;` |
|       31 | 3652 | `			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){` |
|        9 | 3653 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        9 | 3654 | `				pCtxFrom->iDelegateState = 2;` |
|        6 | 3655 | `			}else{` |
|        6 | 3656 | `				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - | 3657 | `					sizeof("IteratorAggregate")-1,FALSE,0);` |
|        8 | 3658 | `				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){` |
|        - | 3659 | `					/* Delegate to the Iterator returned by getIterator() */` |
|        - | 3660 | `					ph7_value sIt;` |
|        6 | 3661 | `					PH7_MemObjInit(pVm,&sIt);` |
|        6 | 3662 | `					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);` |
|        6 | 3663 | `					if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){` |
|        - | 3664 | `						/* getIterator() threw/aborted: drop it, consume the` |
|        - | 3665 | `						 * operand, and propagate. */` |
|      ! 0 | 3666 | `						PH7_MemObjRelease(&sIt);` |
|      ! 0 | 3667 | `						VmPopOperand(&pTos,1);` |
|      ! 0 | 3668 | `						goto yf_propagate;` |
|        - | 3669 | `					}` |
|        4 | 3670 | `					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls` |
|        6 | 3671 | `						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){` |
|        6 | 3672 | `						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);` |
|        6 | 3673 | `						pCtxFrom->iDelegateState = 2;` |
|        4 | 3674 | `					}else{` |
|      ! 0 | 3675 | `						bIterable = 0;` |
|        - | 3676 | `					}` |
|        6 | 3677 | `					PH7_MemObjRelease(&sIt);` |
|        4 | 3678 | `				}else{` |
|      ! 0 | 3679 | `					bIterable = 0;` |
|        - | 3680 | `				}` |
|        - | 3681 | `			}` |
|       28 | 3682 | `		}else{` |
|        6 | 3683 | `			bIterable = 0;` |
|        - | 3684 | `		}` |
|       77 | 3685 | `		VmPopOperand(&pTos,1); /* Consume the iterable operand */` |
|       77 | 3686 | `		if( !bIterable ){` |
|        - | 3687 | `			/* Non-iterable source: throw a catchable Error (PHP 8.5), then` |
|        - | 3688 | `			 * funnel through the shared teardown/route path. */` |
|        6 | 3689 | `			rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 3690 | `				"Can use \"yield from\" only with arrays and Traversables",` |
|        - | 3691 | `				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);` |
|        6 | 3692 | `			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        6 | 3693 | `			goto yf_propagate;` |
|        - | 3694 | `		}` |
|       73 | 3695 | `		if( pCtxFrom->iDelegateState >= 2 ){` |
|        - | 3696 | `			/* rewind() the delegate (also starts a fresh generator) */` |
|       51 | 3697 | `			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 3698 | `				"rewind",sizeof("rewind")-1,0);` |
|       51 | 3699 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       23 | 3700 | `		}` |
|       39 | 3701 | `	}else{` |
|        - | 3702 | `		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a` |
|        - | 3703 | `		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()` |
|        - | 3704 | `		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)` |
|        - | 3705 | `		 * ignore send() and just advance with next(). */` |
|        - | 3706 | `#ifdef UNTRUST` |
|        - | 3707 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 3708 | `#endif` |
|      112 | 3709 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       68 | 3710 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|        - | 3711 | `			/* A pending Generator::throw() on the outer was parked on pInjected by the` |
|        - | 3712 | `			 * body-entry gate (which skips its own raise for state 3); forward it as a` |
|        - | 3713 | `			 * throw into the delegate, else forward the sent value (pTos, which` |
|        - | 3714 | `			 * VmResumeCtx copies into the inner's own stack). */` |
|       68 | 3715 | `			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;` |
|       68 | 3716 | `			pCtxFrom->pInjected = 0;` |
|       68 | 3717 | `			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       68 | 3718 | `				if( pInjFwd ){` |
|        - | 3719 | `					/* Borrowed ref: the outer's Generator::throw() holds it across this` |
|        - | 3720 | `					 * whole resume, so the inner inject path must not unref it. */` |
|        5 | 3721 | `					pInner->pCtx->pInjected = pInjFwd;` |
|        5 | 3722 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);` |
|        5 | 3723 | `					pInner->pCtx->pInjected = 0;` |
|        3 | 3724 | `				}else{` |
|       64 | 3725 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);` |
|        4 | 3726 | `				}` |
|       32 | 3727 | `			}else if( pInjFwd ){` |
|        - | 3728 | `				/* No live delegate to receive the throw (inner already finished/closed):` |
|        - | 3729 | `				 * raise it at the yield-from in the outer generator's own frame. */` |
|      ! 0 | 3730 | `				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);` |
|      ! 0 | 3731 | `				pTF->iFlags \|= VM_FRAME_THROW;` |
|      ! 0 | 3732 | `				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 3733 | `			}` |
|       68 | 3734 | `			PH7_MemObjRelease(pTos);` |
|       68 | 3735 | `			pTos--;` |
|       68 | 3736 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       32 | 3737 | `		}else{` |
|       48 | 3738 | `			PH7_MemObjRelease(pTos);` |
|       48 | 3739 | `			pTos--;` |
|       48 | 3740 | `			if( pCtxFrom->iDelegateState >= 2 ){` |
|       17 | 3741 | `				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 3742 | `					"next",sizeof("next")-1,0);` |
|       17 | 3743 | `				if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 3744 | `			}` |
|        - | 3745 | `		}` |
|        - | 3746 | `	}` |
|        - | 3747 | `	/* Fetch the current (key,value) of the delegate, or mark exhausted. */` |
|      171 | 3748 | `	if( pCtxFrom->iDelegateState == 1 ){` |
|       56 | 3749 | `		if( pCtxFrom->pDelegateNode == 0 ){` |
|       18 | 3750 | `			bExhausted = 1;` |
|       11 | 3751 | `		}else{` |
|       42 | 3752 | `			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);` |
|       42 | 3753 | `			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);` |
|        - | 3754 | `			/* Forward traversal follows pPrev (the hashmap's "reverse link",` |
|        - | 3755 | `			 * matching PH7_HashmapGetNextEntry). */` |
|       42 | 3756 | `			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;` |
|        - | 3757 | `		}` |
|       30 | 3758 | `	}else{` |
|      119 | 3759 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|        - | 3760 | `		ph7_value sValid;` |
|        - | 3761 | `		int isValid;` |
|      119 | 3762 | `		PH7_MemObjInit(pVm,&sValid);` |
|      119 | 3763 | `		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|      119 | 3764 | `		PH7_MemObjToBool(&sValid);` |
|      119 | 3765 | `		isValid = (sValid.x.iVal != 0);` |
|      119 | 3766 | `		PH7_MemObjRelease(&sValid);` |
|      119 | 3767 | `		if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      119 | 3768 | `		if( !isValid ){` |
|       28 | 3769 | `			bExhausted = 1;` |
|       16 | 3770 | `		}else{` |
|       95 | 3771 | `			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|       95 | 3772 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       95 | 3773 | `			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       95 | 3774 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        - | 3775 | `		}` |
|        - | 3776 | `	}` |
|      171 | 3777 | `	if( bExhausted ){` |
|        - | 3778 | `		/* Expression value: inner Generator's return value (state 3) or NULL. */` |
|        - | 3779 | `		ph7_value sResult;` |
|       42 | 3780 | `		PH7_MemObjInit(pVm,&sResult);` |
|       42 | 3781 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       23 | 3782 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|       23 | 3783 | `			if( pInner && pInner->pCtx ){` |
|       23 | 3784 | `				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);` |
|       10 | 3785 | `			}` |
|       10 | 3786 | `		}` |
|       42 | 3787 | `		PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       42 | 3788 | `		pCtxFrom->pDelegateNode = 0;` |
|       42 | 3789 | `		pCtxFrom->iDelegateState = 0;` |
|       42 | 3790 | `		pTos++;` |
|       42 | 3791 | `		PH7_MemObjStore(&sResult,pTos);` |
|       42 | 3792 | `		PH7_MemObjRelease(&sResult);` |
|       42 | 3793 | `		PH7_MemObjRelease(&sKey);` |
|       42 | 3794 | `		PH7_MemObjRelease(&sVal);` |
|       42 | 3795 | `		break; /* fall through to pc+1 with the result on the stack top */` |
|        - | 3796 | `	}` |
|        - | 3797 | `	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.` |
|        - | 3798 | `	 * The outer generator's implicit auto-key counter is NOT advanced by the` |
|        - | 3799 | ``	 * delegated keys — PHP keeps it independent across `yield from`, so a later`` |
|        - | 3800 | ``	 * plain `yield` continues from the outer's own counter. */`` |
|      133 | 3801 | `	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);` |
|      133 | 3802 | `	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);` |
|      133 | 3803 | `	PH7_MemObjRelease(&sKey);` |
|      133 | 3804 | `	PH7_MemObjRelease(&sVal);` |
|        - | 3805 | `	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */` |
|      133 | 3806 | `	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));` |
|      133 | 3807 | `	goto Suspend;` |
|        7 | 3808 | `yf_propagate:` |
|        - | 3809 | `	/* A delegate iterator method threw/aborted (or the source was non-iterable):` |
|        - | 3810 | `	 * tear down the delegation, then route via the shared dispatch macro. rcm is` |
|        - | 3811 | `	 * always PH7_EXCEPTION or PH7_ABORT here. */` |
|       18 | 3812 | `	PH7_MemObjRelease(&sKey);` |
|       18 | 3813 | `	PH7_MemObjRelease(&sVal);` |
|       18 | 3814 | `	PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       18 | 3815 | `	pCtxFrom->pDelegateNode = 0;` |
|       18 | 3816 | `	pCtxFrom->iDelegateState = 0;` |
|       18 | 3817 | `	PH7_DISPATCH_ENFORCE_RC(rcm)` |
|      ! 0 | 3818 | `	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */` |
|        - | 3819 | `}` |
|        - | 3820 | `/*` |
|        - | 3821 | ` * OP_CALL P1 * *` |
|        - | 3822 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - | 3823 | ` *  function on the stack.` |
|        - | 3824 | ` */` |
|   522715 | 3825 | `case PH7_OP_CALL: {` |
|        - | 3826 | `	/* iP2 = hasSpread (compile-time). Count only THIS call's own unpack` |
|        - | 3827 | `	 * expansion (VmSpreadOwnExtra, derived from the captured runs on top of the` |
|        - | 3828 | `	 * stack) — an INNER spread-bearing call evaluated inside this argument list` |
|        - | 3829 | ``	 * (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not`` |
|        - | 3830 | `	 * be conflated, which a single shared accumulator could not express. */` |
|  1046385 | 3831 | `	sxi32 nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|        - | 3832 | `	ph7_value *pArg;` |
|  1046385 | 3833 | `	pArg = &pTos[-nCallArgs];` |
|        - | 3834 | `	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED` |
|        - | 3835 | `	 * arguments, and a spread expanding to !=1 element shifts the actual positions` |
|        - | 3836 | `	 * of any following compile-time named args. The effective per-actual-slot name` |
|        - | 3837 | `	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)` |
|        - | 3838 | `	 * is built PER PATH against that path's finalized arg base — a method call pops` |
|        - | 3839 | `	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch` |
|        - | 3840 | `	 * site rather than built once here. */` |
|        - | 3841 | `	VmCallArgMap sEffMap;` |
|  1046385 | 3842 | `	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;` |
|        - | 3843 | `	SyHashEntry *pEntry;` |
|        - | 3844 | `	SyString sName;` |
|        - | 3845 | `	/* A Closure object is callable: unwrap it to its underlying string callable so the` |
|        - | 3846 | `	 * dispatch below handles it (rather than treating it as a generic object and looking` |
|        - | 3847 | `	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure` |
|        - | 3848 | `	 * so a plain __invoke object skips the temp-value work entirely. */` |
|  1046385 | 3849 | `	if( VmValueIsClosure(pVm,pTos) ){` |
|        - | 3850 | `		ph7_value sCallable;` |
|     1157 | 3851 | `		PH7_MemObjInit(pVm,&sCallable);` |
|     1157 | 3852 | `		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){` |
|     1157 | 3853 | `			PH7_MemObjRelease(pTos);` |
|     1157 | 3854 | `			PH7_MemObjStore(&sCallable,pTos);` |
|      576 | 3855 | `		}` |
|     1157 | 3856 | `		PH7_MemObjRelease(&sCallable);` |
|      576 | 3857 | `	}` |
|        - | 3858 | `	/* Extract function name */` |
|  1046385 | 3859 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      162 | 3860 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 3861 | `			ph7_value sResult;` |
|        - | 3862 | `			sxi32 rcArr;` |
|        - | 3863 | `			{` |
|        - | 3864 | `				/* php validates the SHAPE of an array callable first: it must hold exactly` |
|        - | 3865 | `				 * two elements. PH7 handed any array to the dispatcher, which failed` |
|        - | 3866 | ``				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */`` |
|       75 | 3867 | `				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - | 3868 | `				char zCbMsg[192];` |
|       75 | 3869 | `				const char *zCbErr = 0;` |
|       75 | 3870 | `				if( pCbMap && pCbMap->nEntry == 2 ){` |
|        - | 3871 | `					/* Shape is right; now check it actually RESOLVES. The shared dispatcher` |
|        - | 3872 | `					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for` |
|        - | 3873 | `					 * an unresolvable [class,method] pair -- silence a caller cannot detect --` |
|        - | 3874 | `					 * and its contract is relied on by call_user_func/usort, so the throw` |
|        - | 3875 | `					 * belongs here at the call site. */` |
|       73 | 3876 | `					ph7_value *pCbCls = (ph7_value *)SySetAt(&pVm->aMemObj,pCbMap->pFirst->nValIdx);` |
|       73 | 3877 | `					ph7_value *pCbMeth = (ph7_value *)SySetAt(&pVm->aMemObj,pCbMap->pFirst->pPrev->nValIdx);` |
|       73 | 3878 | `					ph7_class *pCbClass = pCbCls ? PH7_VmExtractClassFromValue(&(*pVm),pCbCls) : 0;` |
|       73 | 3879 | `					if( pCbClass == 0 ){` |
|      ! 0 | 3880 | `						SyBufferFormat(zCbMsg,sizeof(zCbMsg),"Class \"%.*s\" not found",` |
|      ! 0 | 3881 | `							pCbCls ? (int)SyBlobLength(&pCbCls->sBlob) : 0,` |
|      ! 0 | 3882 | `							pCbCls ? (const char *)SyBlobData(&pCbCls->sBlob) : "");` |
|      ! 0 | 3883 | `						zCbErr = zCbMsg;` |
|       72 | 3884 | `					}else if( pCbMeth == 0 \|\| (pCbMeth->iFlags & MEMOBJ_STRING) == 0` |
|       73 | 3885 | `						\|\| PH7_ClassExtractMethod(pCbClass,(const char *)SyBlobData(&pCbMeth->sBlob),` |
|       72 | 3886 | `							SyBlobLength(&pCbMeth->sBlob)) == 0 ){` |
|        5 | 3887 | `						SyBufferFormat(zCbMsg,sizeof(zCbMsg),"Call to undefined method %z::%.*s()",` |
|        1 | 3888 | `							&pCbClass->sName,` |
|        2 | 3889 | `							pCbMeth ? (int)SyBlobLength(&pCbMeth->sBlob) : 0,` |
|        1 | 3890 | `							pCbMeth ? (const char *)SyBlobData(&pCbMeth->sBlob) : "");` |
|        3 | 3891 | `						zCbErr = zCbMsg;` |
|        1 | 3892 | `					}` |
|       36 | 3893 | `				}` |
|       75 | 3894 | `				if( pCbMap == 0 \|\| pCbMap->nEntry != 2 \|\| zCbErr ){` |
|        - | 3895 | `					sxi32 rcCb;` |
|        5 | 3896 | `					if( pInstr->iP2 ){` |
|      ! 0 | 3897 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 3898 | `					}` |
|        5 | 3899 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 3900 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 3901 | `					}` |
|        5 | 3902 | `					PH7_MemObjRelease(pTos);` |
|        5 | 3903 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        5 | 3904 | `					pTos->nIdx = SXU32_HIGH;` |
|        5 | 3905 | `					if( zCbErr == 0 ){` |
|        3 | 3906 | `						zCbErr = "Array callback must have exactly two elements";` |
|        1 | 3907 | `					}` |
|        5 | 3908 | `					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));` |
|        5 | 3909 | `					if( rcCb == SXERR_ABORT ){ goto Abort; }` |
|        5 | 3910 | `					rc = rcCb;` |
|        5 | 3911 | `					PH7_DISPATCH_ENFORCE_RC(rc)` |
|        2 | 3912 | `				}` |
|        - | 3913 | `			}` |
|        - | 3914 | `			/* Build the effective spread-key map (and consume this call's runs)` |
|        - | 3915 | `			 * against this path's arg base (the array-callable slot isn't popped). */` |
|      112 | 3916 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|       74 | 3917 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|       75 | 3918 | `			SySetReset(&aArg);` |
|      143 | 3919 | `			while( pArg < pTos ){` |
|       69 | 3920 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       69 | 3921 | `				pArg++;` |
|        1 | 3922 | `			}` |
|       75 | 3923 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 3924 | `			/* May be a class instance and it's static method. Forward this call's named-arg map` |
|        - | 3925 | ``			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —`` |
|        - | 3926 | `			 * mirroring the __invoke-object branch below. */` |
|       75 | 3927 | `			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|       75 | 3928 | `			SySetReset(&aArg);` |
|        - | 3929 | `			/* Pop given arguments */` |
|       75 | 3930 | `			if( nCallArgs > 0 ){` |
|       53 | 3931 | `				VmPopOperand(&pTos,nCallArgs);` |
|       26 | 3932 | `			}` |
|       75 | 3933 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 | 3934 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 3935 | `				goto Abort;` |
|        - | 3936 | `			}` |
|       75 | 3937 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - | 3938 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - | 3939 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - | 3940 | `				sxi32 iResumePc;` |
|        3 | 3941 | `				PH7_MemObjRelease(&sResult);` |
|        3 | 3942 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 3943 | `					PH7_MemObjRelease(pTos);` |
|        3 | 3944 | `					pc = iResumePc;` |
|        3 | 3945 | `					break;` |
|        - | 3946 | `				}` |
|      ! 0 | 3947 | `				goto Exception;` |
|        - | 3948 | `			}` |
|        - | 3949 | `			/* Copy result */` |
|       73 | 3950 | `			PH7_MemObjStore(&sResult,pTos);` |
|       73 | 3951 | `			PH7_MemObjRelease(&sResult);` |
|      124 | 3952 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       86 | 3953 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - | 3954 | `			ph7_value sResult;` |
|        - | 3955 | `			sxi32 rcInv;` |
|        - | 3956 | `			/* __invoke object callable: the object slot isn't popped, so pArg is` |
|        - | 3957 | `			 * already this call's arg base — build the map + consume the runs. */` |
|      128 | 3958 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|       84 | 3959 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|       86 | 3960 | `			SySetReset(&aArg);` |
|      204 | 3961 | `			while( pArg < pTos ){` |
|      120 | 3962 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      120 | 3963 | `				pArg++;` |
|        2 | 3964 | `			}` |
|       86 | 3965 | `			PH7_MemObjInit(pVm,&sResult);` |
|      128 | 3966 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       84 | 3967 | `				(int)SySetUsed(&aArg),` |
|       84 | 3968 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - | 3969 | `				&sResult,` |
|       42 | 3970 | `				pEffCallMap);` |
|       86 | 3971 | `			SySetReset(&aArg);` |
|        - | 3972 | `			/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable` |
|        - | 3973 | `			 * slot itself (it pops top-down and the callable IS pTos), which for a` |
|        - | 3974 | `			 * temporary like (new Plain())(...) holds the only reference — popping it` |
|        - | 3975 | `			 * would free pThis before VmRaiseNotCallable reads its class name below.` |
|        - | 3976 | `			 * Only the not-callable branch dereferences pThis afterwards, so pin just` |
|        - | 3977 | `			 * for that case; the matching PH7_ClassInstanceUnref drops it (destroying` |
|        - | 3978 | `			 * the temporary). The other branches let the pop free the temp as before. */` |
|       86 | 3979 | `			if( rcInv == SXERR_INVALID ){` |
|       13 | 3980 | `				pThis->iRef++;` |
|        6 | 3981 | `			}` |
|       86 | 3982 | `			if( nCallArgs > 0 ){` |
|       78 | 3983 | `				VmPopOperand(&pTos,nCallArgs);` |
|       38 | 3984 | `			}` |
|       86 | 3985 | `			if( rcInv == SXERR_INVALID ){` |
|        - | 3986 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - | 3987 | `				 * sResult was already released by VmCallObjectInvoke. */` |
|       13 | 3988 | `				PH7_MemObjRelease(pTos);` |
|       13 | 3989 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 | 3990 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 | 3991 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3992 | `					goto Abort;` |
|        - | 3993 | `				}` |
|        - | 3994 | `				{` |
|        - | 3995 | `					sxi32 iRp;` |
|       13 | 3996 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|       13 | 3997 | `						pc = iRp;` |
|       13 | 3998 | `						break;` |
|        - | 3999 | `					}` |
|        - | 4000 | `				}` |
|      ! 0 | 4001 | `				goto Exception;` |
|        - | 4002 | `			}` |
|       74 | 4003 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 | 4004 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 4005 | `				goto Abort;` |
|        - | 4006 | `			}` |
|       74 | 4007 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - | 4008 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - | 4009 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - | 4010 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - | 4011 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - | 4012 | `				sxi32 iResumePc;` |
|        7 | 4013 | `				PH7_MemObjRelease(&sResult);` |
|        7 | 4014 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        5 | 4015 | `					PH7_MemObjRelease(pTos);` |
|        5 | 4016 | `					pc = iResumePc;` |
|        5 | 4017 | `					break;` |
|        - | 4018 | `				}` |
|        3 | 4019 | `				goto Exception;` |
|        - | 4020 | `			}` |
|       68 | 4021 | `			PH7_MemObjStore(&sResult,pTos);` |
|       68 | 4022 | `			PH7_MemObjRelease(&sResult);` |
|       35 | 4023 | `		}else{` |
|        - | 4024 | `			/* php: calling a non-callable is a catchable Error naming the type` |
|        - | 4025 | `			 * ("Value of type int is not callable"), or -- for an array -- the shape it` |
|        - | 4026 | `			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so` |
|        - | 4027 | ``			 * `$x()` on a number quietly evaluated to nothing. */`` |
|        - | 4028 | `			sxi32 rcNc;` |
|        - | 4029 | `			char zMsg[128];` |
|        3 | 4030 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 4031 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");` |
|      ! 0 | 4032 | `			}else{` |
|        4 | 4033 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",` |
|        1 | 4034 | `					VmArithTypeName(pTos));` |
|        - | 4035 | `			}` |
|        - | 4036 | `			/* Consume this call's captured spread runs — a non-callable target` |
|        - | 4037 | `			 * (int/float/bool/null) reaches no dispatch build site. */` |
|        3 | 4038 | `			if( pInstr->iP2 ){` |
|      ! 0 | 4039 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 4040 | `			}` |
|        - | 4041 | `			/* Pop given arguments */` |
|        3 | 4042 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 4043 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 4044 | `			}` |
|        - | 4045 | `			/* Settle the call's result slot BEFORE throwing. */` |
|        3 | 4046 | `			PH7_MemObjRelease(pTos);` |
|        3 | 4047 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 4048 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 4049 | `			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|        3 | 4050 | `			if( rcNc == SXERR_ABORT ){ goto Abort; }` |
|        3 | 4051 | `			rc = rcNc;` |
|        3 | 4052 | `			PH7_DISPATCH_ENFORCE_RC(rc)` |
|        - | 4053 | `		}` |
|      142 | 4054 | `		break;` |
|        - | 4055 | `	}` |
|  1046225 | 4056 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 4057 | `	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just` |
|        - | 4058 | `	 * anchors it to the global namespace — strip it before resolving so a` |
|        - | 4059 | ``	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */`` |
|  1046225 | 4060 | `	if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|       13 | 4061 | `		sName.zString++;` |
|       13 | 4062 | `		sName.nByte--;` |
|        6 | 4063 | `	}` |
|        - | 4064 | `	/* Check for a compiled function first.` |
|        - | 4065 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 4066 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|  1046225 | 4067 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 4068 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - | 4069 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - | 4070 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - | 4071 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - | 4072 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - | 4073 | `	{` |
|  1046225 | 4074 | `	VmCallArgMap *pCallMap = pEffCallMap;` |
|  1046225 | 4075 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - | 4076 | `		const char *zFunc;` |
|        - | 4077 | `		const char *zEnd;` |
|        - | 4078 | `		const char *z;` |
|        - | 4079 | `		SyString sGlobal;` |
|       32 | 4080 | `		zFunc = sName.zString;` |
|       32 | 4081 | `		zEnd  = zFunc + sName.nByte;` |
|       32 | 4082 | `		z = zEnd;` |
|        - | 4083 | `		/* Find last namespace separator */` |
|      286 | 4084 | `		while( z > zFunc ){` |
|      286 | 4085 | `			if( z[-1] == '\\' ){` |
|       32 | 4086 | `				break;` |
|        - | 4087 | `			}` |
|      258 | 4088 | `			z--;` |
|        4 | 4089 | `		}` |
|       32 | 4090 | `		if( z > zFunc && z < zEnd ){` |
|        - | 4091 | `			/* Retry lookup using the unqualified/global function name */` |
|       32 | 4092 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       32 | 4093 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       14 | 4094 | `		}` |
|       14 | 4095 | `	}` |
|        - | 4096 | `	} /* end VmCallArgMap namespace scope */` |
|  1046225 | 4097 | `	if( pEntry ){` |
|        - | 4098 | `		ph7_vm_func_arg *aFormalArg;` |
|        - | 4099 | `		ph7_class_instance *pThis;` |
|        - | 4100 | `		ph7_value *pFrameStack;` |
|        - | 4101 | `		ph7_vm_func *pVmFunc;` |
|        - | 4102 | `		ph7_class *pSelf;` |
|        - | 4103 | `		ph7_class *pSelfHint;` |
|        - | 4104 | `		VmFrame *pFrame;` |
|        - | 4105 | `		ph7_value *pObj;` |
|        - | 4106 | `		VmSlot sArg;` |
|        - | 4107 | `		sxu32 n;` |
|   106664 | 4108 | `		int bClosureThis = 0;` |
|   106664 | 4109 | `		ph7_class *pClosureScope = 0;` |
|        - | 4110 | `		/* initialize fields */` |
|   106664 | 4111 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|   106664 | 4112 | `		pThis = 0;` |
|   106664 | 4113 | `		pSelf = 0;` |
|        - | 4114 | `		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it` |
|        - | 4115 | `		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever` |
|        - | 4116 | `		 * set for a bound plain closure, which dispatches as a function, so the method branch below` |
|        - | 4117 | `		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */` |
|   106664 | 4118 | `		if( pVm->pClosureThis ){` |
|       33 | 4119 | `			pThis = pVm->pClosureThis;` |
|       33 | 4120 | `			pVm->pClosureThis = 0;` |
|       33 | 4121 | `			bClosureThis = 1;` |
|       16 | 4122 | `		}` |
|   106664 | 4123 | `		if( pVm->pClosureScope ){` |
|        - | 4124 | `			/* May ride alongside a bound $this, or stand alone for a` |
|        - | 4125 | ``			 * scope-only rebind (`bindTo(null, Scope::class)`). */`` |
|       29 | 4126 | `			pClosureScope = pVm->pClosureScope;` |
|       29 | 4127 | `			pVm->pClosureScope = 0;` |
|       14 | 4128 | `		}` |
|   106664 | 4129 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - | 4130 | `			ph7_class_method *pMeth;` |
|        - | 4131 | `			/* Class method call */` |
|    25687 | 4132 | `			ph7_value *pTarget = &pTos[-1];` |
|    25687 | 4133 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - | 4134 | `				/* Extract the 'this' pointer */` |
|    25687 | 4135 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - | 4136 | `					/* Instance already loaded */` |
|    24649 | 4137 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|    24649 | 4138 | `					pThis->iRef++;` |
|    24649 | 4139 | `					pSelf = pThis->pClass;` |
|    12322 | 4140 | `				}` |
|    25687 | 4141 | `				if( pSelf == 0 ){` |
|     1043 | 4142 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - | 4143 | `						/* "Late Static Binding" class name */` |
|     1475 | 4144 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|      490 | 4145 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|      490 | 4146 | `					}` |
|     1043 | 4147 | `					if( pSelf == 0 ){` |
|       60 | 4148 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       29 | 4149 | `					}` |
|      519 | 4150 | `				}` |
|    25687 | 4151 | `				if( pThis == 0  ){` |
|     1043 | 4152 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|     1043 | 4153 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|     1043 | 4154 | `					if( pFrameLocal->pParent ){` |
|        - | 4155 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|      711 | 4156 | `						pThis = pFrameLocal->pThis;` |
|      711 | 4157 | `						if( pThis ){` |
|      182 | 4158 | `							pThis->iRef++;` |
|       90 | 4159 | `						}` |
|      353 | 4160 | `					}` |
|      519 | 4161 | `				}` |
|    25687 | 4162 | `				VmPopOperand(&pTos,1);` |
|    25687 | 4163 | `				PH7_MemObjRelease(pTos);` |
|        - | 4164 | `				/* Synchronize pointers. The method-name slot popped above sat BETWEEN` |
|        - | 4165 | `				 * this call's arguments and the (already-removed) target — so only now` |
|        - | 4166 | `				 * is pTos one past the last actual argument. Re-derive the unpack` |
|        - | 4167 | `				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs` |
|        - | 4168 | `				 * from run ends, which the extra target slot would otherwise offset,` |
|        - | 4169 | ``				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */`` |
|    25687 | 4170 | `				nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);` |
|    25687 | 4171 | `				pArg = &pTos[-nCallArgs];` |
|        - | 4172 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - | 4173 | `				 * user have already computed the random generated unique class method name` |
|        - | 4174 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - | 4175 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - | 4176 | `				 */` |
|    25687 | 4177 | `				while( pArg < pStack ){` |
|      ! 0 | 4178 | `					pArg++;` |
|      ! 0 | 4179 | `				}` |
|    25687 | 4180 | `				if( pSelf && pVm->bReflectBypass ){` |
|        - | 4181 | `					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection` |
|        - | 4182 | `					 * ignores visibility. Consume-once so nested calls made by the` |
|        - | 4183 | `					 * invoked body are checked normally. */` |
|       11 | 4184 | `					pVm->bReflectBypass = 0;` |
|        6 | 4185 | `				}else` |
|    25677 | 4186 | `				if( pSelf ){ /* Paranoid edition */` |
|        - | 4187 | `					/* Check if the call is allowed. php binds non-public method` |
|        - | 4188 | `					 * access by the DECLARING class (pVmFunc->pUserData — the class` |
|        - | 4189 | `					 * the callee was compiled in), NOT the instance's class: an` |
|        - | 4190 | `					 * inherited base method calling $this->priv() on a child` |
|        - | 4191 | `					 * instance passes, a child's private SHADOW doesn't hijack the` |
|        - | 4192 | `					 * check for a parent callee, and the denial message names the` |
|        - | 4193 | `					 * declaring class like php. */` |
|    25677 | 4194 | `					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|    25677 | 4195 | `					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|    25677 | 4196 | `					if( pMeth == 0 && pDeclClass != pSelf ){` |
|      ! 0 | 4197 | `						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      ! 0 | 4198 | `					}` |
|    25677 | 4199 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     2604 | 4200 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pDeclClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - | 4201 | `							/* php throws a CATCHABLE Error here. The old code merely PRINTED an` |
|        - | 4202 | ``							 * uncaught-exception report and aborted, so `try { $o->priv(); }`` |
|        - | 4203 | ``							 * catch (Error $e)` never caught it and the script died. */`` |
|        - | 4204 | `							char zMsg[256];` |
|        - | 4205 | `							sxi32 rcVis;` |
|        7 | 4206 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       10 | 4207 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|        6 | 4208 | `								zVis,(int)pDeclClass->sName.nByte,pDeclClass->sName.zString,` |
|        6 | 4209 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - | 4210 | `							/* Consume this call's captured spread runs — this visibility` |
|        - | 4211 | `							 * error exits before the pVmFunc build below. */` |
|        7 | 4212 | `							if( pInstr->iP2 ){` |
|      ! 0 | 4213 | `								VmSpreadConsume(pVm);` |
|      ! 0 | 4214 | `							}` |
|        - | 4215 | `							/* Pop given arguments, and leave the call's NULL result behind. */` |
|        7 | 4216 | `							if( nCallArgs > 0 ){` |
|      ! 0 | 4217 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 4218 | `							}` |
|        7 | 4219 | `							PH7_MemObjRelease(pTos);` |
|        7 | 4220 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 | 4221 | `							pTos->nIdx = SXU32_HIGH;` |
|        7 | 4222 | `							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|        7 | 4223 | `							if( rcVis == SXERR_ABORT ){ goto Abort; }` |
|        7 | 4224 | `							rc = rcVis;` |
|        7 | 4225 | `							PH7_DISPATCH_ENFORCE_RC(rc)` |
|        3 | 4226 | `						}` |
|     1300 | 4227 | `					}` |
|    12836 | 4228 | `				}` |
|    12841 | 4229 | `			}` |
|    12841 | 4230 | `		}` |
|        - | 4231 | `		/* pArg is now finalized for every pVmFunc callee (a method call popped its` |
|        - | 4232 | `		 * method-name slot above; functions/closures/generators keep the top base).` |
|        - | 4233 | `		 * Build the PHP 8.1 effective spread-key map here so the generator and the` |
|        - | 4234 | `		 * install path below both see it — and so this call's captured runs are` |
|        - | 4235 | `		 * consumed exactly once, against the correct base. */` |
|   160098 | 4236 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   106659 | 4237 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 4238 | `		/* Check the PHP call-depth cap (the sole site — BYTECODE.md stage 5).` |
|        - | 4239 | `		 * Default is unbounded (heap-bound recursion, decoupled from the C stack` |
|        - | 4240 | `		 * by the stage-2 trampoline); the C stack is guarded separately by` |
|        - | 4241 | `		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and` |
|        - | 4242 | `		 * then raises a clean non-catchable fatal (was: silently set NULL and` |
|        - | 4243 | `		 * continue) and halts. */` |
|   106664 | 4244 | `		if( VmRecursionExceeded(pVm) ){` |
|        - | 4245 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - | 4246 | `			 * which walks the whole operand stack — don't release them here. */` |
|        3 | 4247 | `			VmRecursionFatal(&(*pVm));` |
|        3 | 4248 | `			goto Abort;` |
|        - | 4249 | `		}` |
|   106662 | 4250 | `		if( pVmFunc->pNextName ){` |
|        - | 4251 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      242 | 4252 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|      120 | 4253 | `		}` |
|        - | 4254 | ``		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's`` |
|        - | 4255 | ``		 * `Class::method`). Computed after overload resolution so it reflects the selected method.`` |
|        - | 4256 | ``		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed`` |
|        - | 4257 | `		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method` |
|        - | 4258 | ``		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the`` |
|        - | 4259 | `		 * USING class; we don't carry the using class on the shared struct, so fall back to the` |
|        - | 4260 | `		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for` |
|        - | 4261 | `		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */` |
|   106662 | 4262 | `		pSelfHint = pSelf;` |
|   106662 | 4263 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|    25687 | 4264 | `			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;` |
|    25687 | 4265 | `			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|    24661 | 4266 | `				pSelfHint = pDecl;` |
|    12328 | 4267 | `			}` |
|    12841 | 4268 | `		}` |
|   106662 | 4269 | `		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 4270 | `			/* Push the closure's called-class as this frame's LSB class so` |
|        - | 4271 | ``			 * `static::` inside the body resolves like php. An explicit`` |
|        - | 4272 | `			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use` |
|        - | 4273 | `			 * the class captured at the closure's creation site. self::/parent::` |
|        - | 4274 | `			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done` |
|        - | 4275 | ``			 * after pSelfHint so a `self`-typed param is unaffected. */`` |
|     1811 | 4276 | `			if( pClosureScope ){` |
|       29 | 4277 | `				pSelf = pClosureScope;` |
|     1797 | 4278 | `			}else if( pVmFunc->pLsbClass ){` |
|       64 | 4279 | `				pSelf = (ph7_class *)pVmFunc->pLsbClass;` |
|       30 | 4280 | `			}` |
|      903 | 4281 | `		}` |
|   106662 | 4282 | `		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){` |
|        - | 4283 | `			/* php 8.4 #[\Deprecated] runtime notice — once per call, before` |
|        - | 4284 | `			 * execution (generators: at the g(...) call site, like php). */` |
|      173 | 4285 | `			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,` |
|      112 | 4286 | `				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);` |
|       56 | 4287 | `		}` |
|   106662 | 4288 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - | 4289 | `			/* Generator function: return a Generator object instead of executing */` |
|        - | 4290 | `			ph7_exec_ctx *pExecCtx;` |
|        - | 4291 | `			ph7_generator *pGenerator;` |
|        - | 4292 | `			ph7_class_instance *pGenObj;` |
|        - | 4293 | `			ph7_value *pCtxAttr;` |
|        - | 4294 | `			SyString sAttrName;` |
|        - | 4295 | `			ph7_value **apCallArgs;` |
|        - | 4296 | `			int nGenArgs, iArg;` |
|        - | 4297 | `			/* Collect arguments from the operand stack */` |
|      345 | 4298 | `			nGenArgs = (int)(pTos - pArg);` |
|      345 | 4299 | `			apCallArgs = 0;` |
|      345 | 4300 | `			if( nGenArgs > 0 ){` |
|      109 | 4301 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       35 | 4302 | `					nGenArgs * sizeof(ph7_value *));` |
|       74 | 4303 | `				if( apCallArgs == 0 ){` |
|        - | 4304 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 | 4305 | `					nGenArgs = 0;` |
|      ! 0 | 4306 | `				}else{` |
|       74 | 4307 | `					VmCallArgMap *pGenMap = pEffCallMap;` |
|       74 | 4308 | `					int didReorder = 0;` |
|       74 | 4309 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - | 4310 | `						/* Named-argument reordering for generator */` |
|       10 | 4311 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|       10 | 4312 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|       10 | 4313 | `						sxu32 nNV = nF;` |
|       10 | 4314 | `						sxi32 iVIdx = -1;` |
|        - | 4315 | `						sxi32 *aGSlot;` |
|        - | 4316 | `						sxu8 *aGUsed;` |
|        - | 4317 | `						sxu32 gi;` |
|       22 | 4318 | `						for( gi = 0; gi < nF; gi++ ){` |
|       14 | 4319 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        8 | 4320 | `						}` |
|       14 | 4321 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        8 | 4322 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|       10 | 4323 | `						if( aGSlot ){` |
|       10 | 4324 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|       14 | 4325 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        4 | 4326 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|       10 | 4327 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 4328 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 4329 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4330 | `								goto Abort;` |
|        - | 4331 | `							}` |
|       10 | 4332 | `							if( rc == PH7_EXCEPTION ){` |
|        - | 4333 | `								/* A named-argument error is php's catchable \Error.` |
|        - | 4334 | `								 * No callee frame exists yet on this branch (the` |
|        - | 4335 | `								 * generator body never runs and VmEnterFrame is` |
|        - | 4336 | `								 * further down), so route it like the other` |
|        - | 4337 | `								 * pre-frame OP_CALL throws: drop the args + the` |
|        - | 4338 | `								 * function-name slot and land the enclosing try. */` |
|        3 | 4339 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        3 | 4340 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      173 | 4341 | `								PH7_INLINE_RESUME_BREAK()` |
|        3 | 4342 | `								VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 4343 | `								{` |
|        - | 4344 | `									sxi32 iRpN;` |
|        3 | 4345 | `									if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){` |
|        3 | 4346 | `										pc = iRpN;` |
|        3 | 4347 | `										break;` |
|        - | 4348 | `									}` |
|        - | 4349 | `								}` |
|      ! 0 | 4350 | `								goto Exception;` |
|        - | 4351 | `							}` |
|        8 | 4352 | `							if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){` |
|        - | 4353 | `								/* php's named-hole ArgumentCountError, checked BEFORE` |
|        - | 4354 | `								 * hole compaction: compacting first would report the` |
|        - | 4355 | `								 * positional wording with a fictitious count (g(b:2)` |
|        - | 4356 | ``								 * must be `g(): Argument #1 ($a) not passed`, not`` |
|        - | 4357 | `								 * "1 passed"). Implicit-required watermark, like the` |
|        - | 4358 | `								 * plain-call named path; a hole with NOTHING filled` |
|        - | 4359 | `								 * above it keeps php's count wording — fall through` |
|        - | 4360 | `								 * to VmFiberSetupFrame's check (the compacted count` |
|        - | 4361 | `								 * equals php's num_args there). */` |
|        8 | 4362 | `								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;` |
|        8 | 4363 | `								sxi32 iHole = -1;` |
|        8 | 4364 | `								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);` |
|       18 | 4365 | `								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){` |
|       12 | 4366 | `									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){` |
|       10 | 4367 | `										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);` |
|        4 | 4368 | `									}` |
|        7 | 4369 | `								}` |
|       18 | 4370 | `								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){` |
|        - | 4371 | `									sxu32 gj;` |
|       12 | 4372 | `									int bFound = 0;` |
|       16 | 4373 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       16 | 4374 | `										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }` |
|        3 | 4375 | `									}` |
|       12 | 4376 | `									if( !bFound && gHole + 1 <= nMaxFilledG ){` |
|      ! 0 | 4377 | `										iHole = (sxi32)gHole;` |
|      ! 0 | 4378 | `									}` |
|        7 | 4379 | `								}` |
|        8 | 4380 | `								if( iHole >= 0 ){` |
|      ! 0 | 4381 | `									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 4382 | `										(sxu32)iHole+1,&aFA[iHole].sName);` |
|      ! 0 | 4383 | `									SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 4384 | `									SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4385 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 4386 | `										goto Abort;` |
|        - | 4387 | `									}` |
|        - | 4388 | `									/* Route like the VmFiberSetupFrame throw below */` |
|      ! 0 | 4389 | `									PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 4390 | `									VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 4391 | `									{` |
|        - | 4392 | `										sxi32 iRpH;` |
|      ! 0 | 4393 | `										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 4394 | `											pc = iRpH;` |
|      ! 0 | 4395 | `											break;` |
|        - | 4396 | `										}` |
|        - | 4397 | `									}` |
|      ! 0 | 4398 | `									goto Exception;` |
|        - | 4399 | `								}` |
|        3 | 4400 | `							}` |
|        - | 4401 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - | 4402 | `							 * append overflow (variadic / positional beyond` |
|        - | 4403 | `							 * formals) so downstream sees every argument. */` |
|        - | 4404 | `							{` |
|        8 | 4405 | `								int nOut = 0;` |
|       18 | 4406 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - | 4407 | `									sxu32 gj;` |
|       16 | 4408 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       16 | 4409 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|       12 | 4410 | `											apCallArgs[nOut++] = &pArg[gj];` |
|       12 | 4411 | `											break;` |
|        - | 4412 | `										}` |
|        3 | 4413 | `									}` |
|        7 | 4414 | `								}` |
|       18 | 4415 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|       12 | 4416 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 | 4417 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 | 4418 | `									}` |
|        7 | 4419 | `								}` |
|        8 | 4420 | `								nGenArgs = nOut;` |
|        - | 4421 | `							}` |
|        8 | 4422 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        8 | 4423 | `							didReorder = 1;` |
|        3 | 4424 | `						}` |
|        - | 4425 | `						/* If aGSlot allocation failed, fall through to` |
|        - | 4426 | `						 * positional fill below — preserves arg order rather` |
|        - | 4427 | `						 * than passing an uninitialized apCallArgs. */` |
|        3 | 4428 | `					}` |
|       72 | 4429 | `					if( !didReorder ){` |
|      134 | 4430 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|       72 | 4431 | `							apCallArgs[iArg] = &pArg[iArg];` |
|       38 | 4432 | `						}` |
|       31 | 4433 | `					}` |
|        - | 4434 | `				}` |
|       34 | 4435 | `			}` |
|        - | 4436 | `			/* Create execution context and generator wrapper */` |
|      343 | 4437 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|      343 | 4438 | `			if( pExecCtx == 0 ){` |
|      ! 0 | 4439 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4440 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 4441 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 4442 | `				break;` |
|        - | 4443 | `			}` |
|      343 | 4444 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|      343 | 4445 | `			if( pGenerator == 0 ){` |
|      ! 0 | 4446 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 | 4447 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4448 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 4449 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 4450 | `				break;` |
|        - | 4451 | `			}` |
|        - | 4452 | `			/* Set up the frame with arguments, closure env, $this */` |
|      343 | 4453 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|      343 | 4454 | `			pVm->pFrame = pExecCtx->pFrame;` |
|      349 | 4455 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,` |
|      172 | 4456 | `				(pEffCallMap && pEffCallMap->bStrict) ? 1 : 0, pSelfHint,` |
|        - | 4457 | `				TRUE/*generator: the g(...) call site is in the message*/);` |
|      343 | 4458 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|      343 | 4459 | `			pExecCtx->pFrame->pParent = 0;` |
|      343 | 4460 | `			if( apCallArgs ){` |
|       72 | 4461 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       34 | 4462 | `			}` |
|      343 | 4463 | `			if( rc != SXRET_OK ){` |
|       15 | 4464 | `				VmReleaseGenerator(pVm, pGenerator);` |
|       15 | 4465 | `				if( pThis ){` |
|        3 | 4466 | `					PH7_ClassInstanceUnref(pThis);` |
|        1 | 4467 | `				}` |
|       15 | 4468 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4469 | `					goto Abort;` |
|        - | 4470 | `				}` |
|       15 | 4471 | `				if( rc == PH7_EXCEPTION ){` |
|        - | 4472 | `					/* A declared-type TypeError thrown while binding the` |
|        - | 4473 | `					 * generator's arguments (php binds + type-checks eagerly at` |
|        - | 4474 | `					 * the g(...) call site, before any resume — band A #2). If` |
|        - | 4475 | `					 * an inline try THIS exec owns caught it, land at its` |
|        - | 4476 | `					 * redirect (the drain subsumes the operand pops); else pop` |
|        - | 4477 | `					 * the args + function name and route like the other` |
|        - | 4478 | `					 * OP_CALL throw paths. */` |
|       19 | 4479 | `					PH7_INLINE_RESUME_BREAK()` |
|       13 | 4480 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 4481 | `					{` |
|        - | 4482 | `						sxi32 iRpG;` |
|       13 | 4483 | `						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){` |
|       13 | 4484 | `							pc = iRpG;` |
|       13 | 4485 | `							break;` |
|        - | 4486 | `						}` |
|        - | 4487 | `					}` |
|      ! 0 | 4488 | `					goto Exception;` |
|        - | 4489 | `				}` |
|      ! 0 | 4490 | `				break;` |
|        - | 4491 | `			}` |
|        - | 4492 | `			/* Create Generator class instance */` |
|      329 | 4493 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|      329 | 4494 | `			if( pGenObj == 0 ){` |
|      ! 0 | 4495 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 | 4496 | `				break;` |
|        - | 4497 | `			}` |
|        - | 4498 | `			/* Store generator in __ctx attribute */` |
|      329 | 4499 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      329 | 4500 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|      329 | 4501 | `			if( pCtxAttr ){` |
|      329 | 4502 | `				pCtxAttr->x.pOther = pGenerator;` |
|      329 | 4503 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      162 | 4504 | `			}` |
|        - | 4505 | `			/* Pop args and function name, push Generator object. PH7_NewClassInstance` |
|        - | 4506 | `			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do` |
|        - | 4507 | `			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope` |
|        - | 4508 | ``			 * and its __destruct (which runs pending `finally` blocks and frees the`` |
|        - | 4509 | `			 * exec context) never fires. */` |
|      329 | 4510 | `			PH7_MemObjRelease(pTos);` |
|      329 | 4511 | `			pTos = &pTos[-nCallArgs];` |
|      329 | 4512 | `			pTos->x.pOther = pGenObj;` |
|      329 | 4513 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|      329 | 4514 | `			if( pThis ){` |
|       29 | 4515 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 | 4516 | `			}` |
|      329 | 4517 | `			break;` |
|        - | 4518 | `		}` |
|        - | 4519 | `		/* Extract the formal argument set */` |
|   106322 | 4520 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - | 4521 | `		/* Create a new VM frame  */` |
|   106322 | 4522 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|   106322 | 4523 | `		if( rc != SXRET_OK ){` |
|        - | 4524 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 4525 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 4526 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 4527 | `				&pVmFunc->sName);` |
|        - | 4528 | `			/* The frame that would own (and later release) $this never got created; for a bound` |
|        - | 4529 | `			 * plain closure the consumed transient is the object's ONLY ref, so release it here` |
|        - | 4530 | `			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */` |
|      ! 0 | 4531 | `			if( bClosureThis && pThis ){` |
|      ! 0 | 4532 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 | 4533 | `			}` |
|        - | 4534 | `			/* Pop given arguments */` |
|      ! 0 | 4535 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 4536 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 4537 | `			}` |
|        - | 4538 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 | 4539 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 4540 | `			break;` |
|        - | 4541 | `		}` |
|   106322 | 4542 | `		if( pClosureScope ){` |
|        - | 4543 | `			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /` |
|        - | 4544 | `			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected` |
|        - | 4545 | `			 * access inside the body resolves against it. */` |
|       27 | 4546 | `			pFrame->pBoundScope = pClosureScope;` |
|       13 | 4547 | `		}` |
|        - | 4548 | `		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band` |
|        - | 4549 | `		 * A #4): sArg over-counts (defaulted params installed, variadic packed` |
|        - | 4550 | `		 * as one entry) so php's answers can't be derived from it. */` |
|   106322 | 4551 | `		pFrame->nActualArgs = (int)(pTos - pArg);` |
|   106322 | 4552 | `		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) \|\| bClosureThis) ){` |
|        - | 4553 | `			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */` |
|        - | 4554 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|    24833 | 4555 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|    24833 | 4556 | `			if( pObj ){` |
|        - | 4557 | `				/* Reflect the change */` |
|    24833 | 4558 | `				pObj->x.pOther = pThis;` |
|    24833 | 4559 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|    12414 | 4560 | `			}` |
|    12414 | 4561 | `		}` |
|   106322 | 4562 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - | 4563 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - | 4564 | `			/* Install static variables */` |
|     1103 | 4565 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|     2201 | 4566 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|     1103 | 4567 | `				pStatic = &aStatic[n];` |
|     1103 | 4568 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - | 4569 | `					/* Initialize the static variables */` |
|       31 | 4570 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|       31 | 4571 | `					if( pObj ){` |
|        - | 4572 | `						/* Assume a NULL initialization value */` |
|       31 | 4573 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|       31 | 4574 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - | 4575 | `							/* Evaluate initialization expression (Any complex expression) */` |
|       31 | 4576 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|       13 | 4577 | `						}` |
|       31 | 4578 | `						pObj->nIdx = pStatic->nIdx;` |
|       18 | 4579 | `					}else{` |
|      ! 0 | 4580 | `						continue;` |
|        - | 4581 | `					}` |
|       13 | 4582 | `				}` |
|        - | 4583 | `				/* Install in the current frame */` |
|     1652 | 4584 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|     1098 | 4585 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      554 | 4586 | `			}` |
|      549 | 4587 | `		}` |
|        - | 4588 | `		/* Push arguments in the local frame */` |
|        - | 4589 | `		{` |
|   106322 | 4590 | `		VmCallArgMap *pCallMap3 = pEffCallMap;` |
|        - | 4591 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - | 4592 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|   106322 | 4593 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|   106322 | 4594 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - | 4595 | `			/* ============================================================` |
|        - | 4596 | `			 * Named-argument matching path (PHP 8.0)` |
|        - | 4597 | `			 *` |
|        - | 4598 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - | 4599 | `			 * or position, then install them in the frame.` |
|        - | 4600 | `			 * ============================================================ */` |
|      272 | 4601 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      272 | 4602 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|      272 | 4603 | `			sxi32 iVariadicIdx = -1;` |
|        - | 4604 | `			sxu32 nNonVariadic;` |
|        - | 4605 | `			sxi32 *aSlot;` |
|        - | 4606 | `			sxu8  *aUsed;` |
|        - | 4607 | `			sxu32 i;` |
|        - | 4608 | `			/* Find variadic parameter index */` |
|      770 | 4609 | `			for( i = 0; i < nFormal; i++ ){` |
|      548 | 4610 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       47 | 4611 | `					iVariadicIdx = (sxi32)i;` |
|       47 | 4612 | `					break;` |
|        - | 4613 | `				}` |
|      253 | 4614 | `			}` |
|      272 | 4615 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - | 4616 | `			/* Allocate mapping arrays */` |
|      406 | 4617 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      268 | 4618 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|      272 | 4619 | `			if( aSlot == 0 ){` |
|      ! 0 | 4620 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 | 4621 | `				goto Abort;` |
|        - | 4622 | `			}` |
|      272 | 4623 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - | 4624 | `			/* Resolve named arguments to formal parameters */` |
|      406 | 4625 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|      134 | 4626 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|      272 | 4627 | `			if( rc == PH7_ABORT ){` |
|        8 | 4628 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 | 4629 | `				goto Abort;` |
|        - | 4630 | `			}` |
|      265 | 4631 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 4632 | `				/* php's catchable \Error for a bad named argument. The callee` |
|        - | 4633 | `				 * frame is already entered but its body must not run: unwind` |
|        - | 4634 | `				 * exactly like the named-hole ArgumentCountError path below` |
|        - | 4635 | `				 * (release the not-yet-installed actuals — Pass 2's release` |
|        - | 4636 | `				 * loop has not run — pop the callee slot, mark that there is no` |
|        - | 4637 | `				 * callee operand stack, and let VmCallFinish route the throw). */` |
|        - | 4638 | `				sxu32 iRel;` |
|        5 | 4639 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       11 | 4640 | `				for( iRel = 0; iRel < nActual; iRel++ ){` |
|        7 | 4641 | `					PH7_MemObjRelease(&pArg[iRel]);` |
|        4 | 4642 | `				}` |
|        5 | 4643 | `				PH7_MemObjRelease(pTos);` |
|        5 | 4644 | `				pTos = &pTos[-nCallArgs];` |
|        5 | 4645 | `				pFrameStack = 0;` |
|        5 | 4646 | `				goto SkipFuncBody;` |
|        - | 4647 | `			}` |
|        - | 4648 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|        - | 4649 | `			{` |
|        - | 4650 | `			/* php's required watermark for the hole check below (0 disables it` |
|        - | 4651 | `			 * for hosted builtin FUNCTIONS, which self-manage — hosted-class` |
|        - | 4652 | `			 * methods and all user code get php's named-hole error), plus the` |
|        - | 4653 | `			 * highest formal slot an actual resolved to: php words a hole` |
|        - | 4654 | ``			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole`` |
|        - | 4655 | `			 * with nothing filled above it gets the positional count message` |
|        - | 4656 | `			 * (zend's RECV arg_num > EX(num_args) distinction). */` |
|      261 | 4657 | `			sxu32 nReqNamed = 0;` |
|      261 | 4658 | `			sxu32 nNVNamed = 0;` |
|      261 | 4659 | `			sxu32 nMaxFilled = 0;` |
|      261 | 4660 | `			if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){` |
|      261 | 4661 | `				nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);` |
|     1009 | 4662 | `				for( i = 0; i < nActual; i++ ){` |
|      751 | 4663 | `					if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|      305 | 4664 | `						nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|      151 | 4665 | `					}` |
|      377 | 4666 | `				}` |
|      129 | 4667 | `			}` |
|      731 | 4668 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - | 4669 | `				/* Find the stack arg mapped to formal n */` |
|      483 | 4670 | `				sxi32 iSrc = -1;` |
|      777 | 4671 | `				for( i = 0; i < nActual; i++ ){` |
|      673 | 4672 | `					if( aSlot[i] == (sxi32)n ){` |
|      379 | 4673 | `						iSrc = (sxi32)i;` |
|      379 | 4674 | `						break;` |
|        - | 4675 | `					}` |
|      149 | 4676 | `				}` |
|      483 | 4677 | `				if( iSrc >= 0 ){` |
|        - | 4678 | `					/* Argument was provided — install with type checking */` |
|      379 | 4679 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - | 4680 | `					/* An explicit null is NOT redirected to the default: PHP applies a` |
|        - | 4681 | `					 * default only for an OMITTED argument. An explicit null falls through` |
|        - | 4682 | `					 * to the type check below (TypeError for a non-nullable typed param,` |
|        - | 4683 | ``					 * kept as null for a typeless one). An implicitly-nullable `Type $x =`` |
|        - | 4684 | ``					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */`` |
|        - | 4685 | `					/* Type checking: union types */` |
|      379 | 4686 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 | 4687 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 | 4688 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 | 4689 | `							bCallIsStrict);` |
|       13 | 4690 | `						if( rcU != SXRET_OK ){` |
|        - | 4691 | `							const char *zGiven;` |
|      ! 0 | 4692 | `							const char *zExpected = "union";` |
|        - | 4693 | `							char zBuf[128];` |
|        - | 4694 | `							char zTypeBuf[128];` |
|      ! 0 | 4695 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 4696 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 | 4697 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 4698 | `								zGiven = "null";` |
|      ! 0 | 4699 | `							}else{` |
|      ! 0 | 4700 | `								zGiven = ph7_type_name(pVal);` |
|        - | 4701 | `							}` |
|      ! 0 | 4702 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 | 4703 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 | 4704 | `							}` |
|      ! 0 | 4705 | `							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|      ! 0 | 4706 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 | 4707 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 | 4708 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 4709 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 4710 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 4711 | `							pFrameStack = 0;` |
|      ! 0 | 4712 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 4713 | `							goto SkipFuncBody;` |
|        - | 4714 | `						}` |
|      371 | 4715 | `					}else if( aFormalArg[n].nType > 0` |
|      225 | 4716 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - | 4717 | `						/* Scalar/class type checking */` |
|       74 | 4718 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|        5 | 4719 | `							SyString *pName = &aFormalArg[n].sClass;` |
|        - | 4720 | `							ph7_class *pClass;` |
|        5 | 4721 | `							int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|        5 | 4722 | `							if( rcPseudo == 0 ){` |
|        - | 4723 | `								/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - | 4724 | `								char zTypeBuf[128],zGivenBuf[128];` |
|      ! 0 | 4725 | `								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|      ! 0 | 4726 | `									&aFormalArg[n].sName,` |
|      ! 0 | 4727 | `									VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 | 4728 | `									VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      ! 0 | 4729 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 | 4730 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 4731 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 | 4732 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 | 4733 | `								pFrameStack = 0;` |
|      ! 0 | 4734 | `								rc = PH7_EXCEPTION;` |
|      ! 0 | 4735 | `								goto SkipFuncBody;` |
|        - | 4736 | `							}` |
|        - | 4737 | `							/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class.` |
|        - | 4738 | ``							 * Resolve via VmResolveTypeClass so `self`/`parent` resolve and`` |
|        - | 4739 | `							 * interface/abstract hints are included (iLoadable=FALSE), then throw a` |
|        - | 4740 | `							 * catchable TypeError on mismatch — matching PHP — instead of the legacy` |
|        - | 4741 | `							 * warn + NULL-coerce (which silently ran the body with a corrupted arg). */` |
|        5 | 4742 | `							pClass = (rcPseudo == 1) ? 0 : VmResolveTypeClass(&(*pVm),pName,pSelfHint);` |
|        5 | 4743 | `							if( pClass ){` |
|        - | 4744 | `								/* Reaching here means the param is non-nullable (the guard` |
|        - | 4745 | ``								 * above skips nullable+null; a `Type $x = null` default is`` |
|        - | 4746 | `								 * marked implicitly nullable at compile time). So ANY` |
|        - | 4747 | `								 * non-object — including an explicit null — is a TypeError,` |
|        - | 4748 | `								 * matching PHP (&& below short-circuits so instanceof only` |
|        - | 4749 | `								 * derefs a real object). */` |
|        7 | 4750 | `								int bBad = !((pVal->iFlags & MEMOBJ_OBJ)` |
|        3 | 4751 | `									&& PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass));` |
|        5 | 4752 | `								if( bBad ){` |
|        - | 4753 | `									char zTypeBuf[128],zGivenBuf[128];` |
|        4 | 4754 | `									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|        2 | 4755 | `										&aFormalArg[n].sName,` |
|        2 | 4756 | `										VmSyStringToCStr(&pClass->sName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 | 4757 | `										VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        3 | 4758 | `									if( rc == PH7_ABORT ) goto Abort;` |
|        3 | 4759 | `									SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 4760 | `									PH7_MemObjRelease(pTos);` |
|        3 | 4761 | `									pTos = &pTos[-nCallArgs];` |
|        3 | 4762 | `									pFrameStack = 0;` |
|        3 | 4763 | `									rc = PH7_EXCEPTION;` |
|        3 | 4764 | `									goto SkipFuncBody;` |
|        - | 4765 | `								}` |
|        2 | 4766 | `							}` |
|       71 | 4767 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 | 4768 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 | 4769 | `								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|      ! 0 | 4770 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 | 4771 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 | 4772 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 4773 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 | 4774 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 | 4775 | `								pFrameStack = 0;` |
|      ! 0 | 4776 | `								rc = PH7_EXCEPTION;` |
|      ! 0 | 4777 | `								goto SkipFuncBody;` |
|       13 | 4778 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 4779 | `								char zTypeBuf[128];` |
|        7 | 4780 | `								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|        4 | 4781 | `									&aFormalArg[n].sName,` |
|        4 | 4782 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        2 | 4783 | `									ph7_type_name(pVal));` |
|        5 | 4784 | `								if( rc == PH7_ABORT ) goto Abort;` |
|        5 | 4785 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        5 | 4786 | `								PH7_MemObjRelease(pTos);` |
|        5 | 4787 | `								pTos = &pTos[-nCallArgs];` |
|        5 | 4788 | `								pFrameStack = 0;` |
|        5 | 4789 | `								rc = PH7_EXCEPTION;` |
|        5 | 4790 | `								goto SkipFuncBody;` |
|        - | 4791 | `							}` |
|        4 | 4792 | `						}` |
|       33 | 4793 | `					}` |
|        - | 4794 | `					/* Install: by reference or by value */` |
|      373 | 4795 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 | 4796 | `						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){` |
|        - | 4797 | `							/* php 8.1: $GLOBALS cannot be passed by reference */` |
|        - | 4798 | `							SyBlob sMsg;` |
|      ! 0 | 4799 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 4800 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 4801 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 4802 | `							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|      ! 0 | 4803 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 4804 | `								goto Abort;` |
|        - | 4805 | `							}` |
|      ! 0 | 4806 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 4807 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 4808 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 4809 | `							pFrameStack = 0;` |
|      ! 0 | 4810 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 4811 | `							goto SkipFuncBody;` |
|        - | 4812 | `						}` |
|        5 | 4813 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 | 4814 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|      ! 0 | 4815 | `							 && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){` |
|        - | 4816 | `								/* A non-lvalue bound to a by-ref parameter is a catchable Error in` |
|        - | 4817 | `								 * php — f(5) where f(&$x). PH7 only warned and quietly passed by` |
|        - | 4818 | `								 * value, so the call ran with a copy and the caller never knew.` |
|        - | 4819 | `								 * The one legitimate copy is call_user_func()'s (MEMOBJ_AUX_CUFVAL),` |
|        - | 4820 | `								 * which php also permits, with its own warning. */` |
|        - | 4821 | `								SyBlob sMsg;` |
|        - | 4822 | `								sxi32 rcRef;` |
|      ! 0 | 4823 | `								SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 4824 | `								SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 4825 | `									&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 4826 | `								rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|      ! 0 | 4827 | `									SyBlobLength(&sMsg));` |
|      ! 0 | 4828 | `								SyBlobRelease(&sMsg);` |
|      ! 0 | 4829 | `								if( rcRef == SXERR_ABORT ){` |
|      ! 0 | 4830 | `									pFrameStack = 0;` |
|      ! 0 | 4831 | `									rc = PH7_ABORT;` |
|      ! 0 | 4832 | `									goto SkipFuncBody;` |
|        - | 4833 | `								}` |
|      ! 0 | 4834 | `								pFrameStack = 0;` |
|      ! 0 | 4835 | `								rc = PH7_EXCEPTION;` |
|      ! 0 | 4836 | `								goto SkipFuncBody;` |
|        - | 4837 | `							}` |
|      ! 0 | 4838 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 4839 | `						}else{` |
|        7 | 4840 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 | 4841 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 | 4842 | `							if( pRefEntry == 0 ){` |
|        7 | 4843 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 | 4844 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 | 4845 | `								sArg.nIdx = pVal->nIdx;` |
|        5 | 4846 | `								sArg.pUserData = 0;` |
|        5 | 4847 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 | 4848 | `							}` |
|        5 | 4849 | `							pObj = 0;` |
|        - | 4850 | `						}` |
|        3 | 4851 | `					}else{` |
|      369 | 4852 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 4853 | `					}` |
|      373 | 4854 | `					if( pObj ){` |
|      369 | 4855 | `						PH7_MemObjStore(pVal,pObj);` |
|      369 | 4856 | `						sArg.nIdx = pObj->nIdx;` |
|      369 | 4857 | `						sArg.pUserData = 0;` |
|      369 | 4858 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      183 | 4859 | `					}` |
|      188 | 4860 | `				}else{` |
|        - | 4861 | `					/* Argument was NOT provided — use default or leave unset */` |
|      106 | 4862 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 4863 | `						/* Should not reach here; variadic handled separately below */` |
|      106 | 4864 | `					}else if( n < nReqNamed ){` |
|        - | 4865 | `						/* php's implicit-required rule applies to named calls` |
|        - | 4866 | `						 * too: a hole below the required watermark throws even` |
|        - | 4867 | `						 * when the formal carries a default — f($a,$b=2,$c)` |
|        - | 4868 | ``						 * called as f(a:1,c:3) is `Argument #2 ($b) not`` |
|        - | 4869 | ``						 * passed` (a hole with NO default is always below the`` |
|        - | 4870 | `						 * watermark, so this subsumes the no-default case).` |
|        - | 4871 | `						 * A hole with nothing filled ABOVE it uses php's` |
|        - | 4872 | `						 * positional count wording instead. The passed stack` |
|        - | 4873 | `						 * args were not released yet on this path (that loop` |
|        - | 4874 | `						 * runs after Pass 2) — release them before the exit. */` |
|        5 | 4875 | `						if( n + 1 > nMaxFilled ){` |
|        3 | 4876 | `							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)` |
|      ! 0 | 4877 | `								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 4878 | `									nMaxFilled,nReqNamed,nNVNamed)` |
|        3 | 4879 | `								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        1 | 4880 | `									nMaxFilled,nReqNamed,nNVNamed,TRUE);` |
|        2 | 4881 | `						}else{` |
|        3 | 4882 | `							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        - | 4883 | `						}` |
|        5 | 4884 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        9 | 4885 | `						for( i = 0; i < nActual; i++ ){` |
|        5 | 4886 | `							PH7_MemObjRelease(&pArg[i]);` |
|        3 | 4887 | `						}` |
|        5 | 4888 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 4889 | `							goto Abort;` |
|        - | 4890 | `						}` |
|        5 | 4891 | `						PH7_MemObjRelease(pTos);` |
|        5 | 4892 | `						pTos = &pTos[-nCallArgs];` |
|        5 | 4893 | `						pFrameStack = 0;` |
|        5 | 4894 | `						rc = PH7_EXCEPTION;` |
|        5 | 4895 | `						goto SkipFuncBody;` |
|      102 | 4896 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      102 | 4897 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      102 | 4898 | `						if( pObj ){` |
|      102 | 4899 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|      102 | 4900 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      102 | 4901 | `							sArg.nIdx = pObj->nIdx;` |
|      102 | 4902 | `							sArg.pUserData = 0;` |
|      102 | 4903 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 4904 | `							/* A null default on an implicitly-nullable param must stay null` |
|        - | 4905 | `							 * (see the positional-path note above). */` |
|      100 | 4906 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|       40 | 4907 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0` |
|       22 | 4908 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 4909 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 4910 | `								if( xCast ) xCast(pObj);` |
|      ! 0 | 4911 | `							}` |
|       50 | 4912 | `						}` |
|       50 | 4913 | `					}` |
|        - | 4914 | `				}` |
|      238 | 4915 | `			}` |
|        - | 4916 | `			} /* end nReqNamed scope */` |
|        - | 4917 | `			/* Handle variadic parameter */` |
|      251 | 4918 | `			if( iVariadicIdx >= 0 ){` |
|       47 | 4919 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|       47 | 4920 | `				if( pObj ){` |
|       47 | 4921 | `					PH7_MemObjToHashmap(pObj);` |
|        - | 4922 | `					{` |
|       47 | 4923 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|      447 | 4924 | `						for( i = 0; i < nActual; i++ ){` |
|      401 | 4925 | `							if( aSlot[i] == -1 ){` |
|      417 | 4926 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - | 4927 | `									/* Named variadic entry: insert with string key */` |
|        - | 4928 | `									ph7_value sKey;` |
|       93 | 4929 | `									PH7_MemObjInit(pVm, &sKey);` |
|       93 | 4930 | `									PH7_MemObjStringAppend(&sKey,` |
|       92 | 4931 | `										pCallMap3->aNames[i].zString,` |
|       92 | 4932 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       93 | 4933 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       93 | 4934 | `									PH7_MemObjRelease(&sKey);` |
|       47 | 4935 | `								}else{` |
|        - | 4936 | `									/* Positional variadic entry */` |
|      279 | 4937 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - | 4938 | `								}` |
|      185 | 4939 | `							}` |
|      201 | 4940 | `						}` |
|        - | 4941 | `					}` |
|       47 | 4942 | `					sArg.nIdx = pObj->nIdx;` |
|       47 | 4943 | `					sArg.pUserData = 0;` |
|       47 | 4944 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 | 4945 | `				}` |
|       24 | 4946 | `			}else{` |
|        - | 4947 | `				/* No variadic — preserve unresolved positional overflow` |
|        - | 4948 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - | 4949 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - | 4950 | `				 * the positional-only path's behavior. */` |
|      205 | 4951 | `				sxu32 nAnon = nNonVariadic;` |
|      543 | 4952 | `				for( i = 0; i < nActual; i++ ){` |
|      341 | 4953 | `					if( aSlot[i] == -2 ){` |
|        - | 4954 | `						char zAnonBuf[32];` |
|        - | 4955 | `						SyString sAnonName;` |
|      ! 0 | 4956 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 | 4957 | `							"[%u]apArg",nAnon);` |
|      ! 0 | 4958 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 | 4959 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 | 4960 | `						if( pObj ){` |
|      ! 0 | 4961 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 | 4962 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 | 4963 | `							sArg.pUserData = 0;` |
|      ! 0 | 4964 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 | 4965 | `						}` |
|      ! 0 | 4966 | `						nAnon++;` |
|      ! 0 | 4967 | `					}` |
|      172 | 4968 | `				}` |
|        - | 4969 | `			}` |
|        - | 4970 | `			/* Release all stack arguments */` |
|      989 | 4971 | `			for( i = 0; i < nActual; i++ ){` |
|      741 | 4972 | `				PH7_MemObjRelease(&pArg[i]);` |
|      372 | 4973 | `			}` |
|      251 | 4974 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - | 4975 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|      251 | 4976 | `			n = nFormal;` |
|      127 | 4977 | `		}else{` |
|        - | 4978 | `		/* ============================================================` |
|        - | 4979 | `		 * Positional-only matching path (original)` |
|        - | 4980 | `		 * ============================================================ */` |
|   106054 | 4981 | `		n = 0;` |
|   273170 | 4982 | `		while( pArg < pTos ){` |
|   167421 | 4983 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - | 4984 | `				/* Variadic parameter: collect all remaining args into an array */` |
|      205 | 4985 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      205 | 4986 | `				if( pObj ){` |
|        - | 4987 | `					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,` |
|        - | 4988 | `					 * reallocating pVm->aMemObj and dangling pObj — so don't read pObj->nIdx after` |
|        - | 4989 | `					 * the packing loop (pre-existing UAF, masked by the pool allocator). pMap is a` |
|        - | 4990 | `					 * separately-allocated hashmap and stays valid across the realloc. */` |
|        - | 4991 | `					sxu32 nVariadicIdx;` |
|        - | 4992 | `					/* Initialize as empty array */` |
|      205 | 4993 | `					PH7_MemObjToHashmap(pObj);` |
|      205 | 4994 | `					nVariadicIdx = pObj->nIdx;` |
|        - | 4995 | `					{` |
|      205 | 4996 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     1749 | 4997 | `						while( pArg < pTos ){` |
|        - | 4998 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - | 4999 | `							 *` |
|        - | 5000 | `							 * TODO: PHP reports the runtime element index here` |
|        - | 5001 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - | 5002 | `							 * index (always n+1, the position of the variadic). The` |
|        - | 5003 | `							 * non-union variadic path below has the same limitation;` |
|        - | 5004 | `							 * fixing both wants a separate counter for elements` |
|        - | 5005 | `							 * already packed into the variadic array. */` |
|     1551 | 5006 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 | 5007 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 | 5008 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 | 5009 | `									bCallIsStrict);` |
|       16 | 5010 | `								if( rcU != SXRET_OK ){` |
|        - | 5011 | `									const char *zGiven;` |
|        3 | 5012 | `									const char *zExpected = "union";` |
|        - | 5013 | `									char zBuf[128];` |
|        - | 5014 | `									char zTypeBuf[128];` |
|        3 | 5015 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 5016 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 | 5017 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 5018 | `										zGiven = "null";` |
|      ! 0 | 5019 | `									}else{` |
|        3 | 5020 | `										zGiven = ph7_type_name(pArg);` |
|        - | 5021 | `									}` |
|        3 | 5022 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 | 5023 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 | 5024 | `									}` |
|        4 | 5025 | `									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|        2 | 5026 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 | 5027 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 5028 | `										goto Abort;` |
|        - | 5029 | `									}` |
|        3 | 5030 | `									PH7_MemObjRelease(pTos);` |
|        3 | 5031 | `									pTos = &pTos[-nCallArgs];` |
|        3 | 5032 | `									pFrameStack = 0;` |
|        3 | 5033 | `									rc = PH7_EXCEPTION;` |
|        3 | 5034 | `									goto SkipFuncBody;` |
|        - | 5035 | `								}` |
|       14 | 5036 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 | 5037 | `								pArg++;` |
|       14 | 5038 | `								continue;` |
|        - | 5039 | `							}` |
|        - | 5040 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - | 5041 | `							 * Nullable types (?type) allow null through without coercion. */` |
|     1532 | 5042 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 | 5043 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       44 | 5044 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 | 5045 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - | 5046 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 | 5047 | `									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|      ! 0 | 5048 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 | 5049 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 5050 | `										goto Abort;` |
|        - | 5051 | `									}` |
|        - | 5052 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 | 5053 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 | 5054 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 | 5055 | `									pFrameStack = 0;` |
|      ! 0 | 5056 | `									rc = PH7_EXCEPTION;` |
|      ! 0 | 5057 | `									goto SkipFuncBody;` |
|       13 | 5058 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 5059 | `									char zTypeBuf[128];` |
|      ! 0 | 5060 | `									rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|      ! 0 | 5061 | `										&aFormalArg[n].sName,` |
|      ! 0 | 5062 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 | 5063 | `										ph7_type_name(pArg));` |
|      ! 0 | 5064 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 5065 | `										goto Abort;` |
|        - | 5066 | `									}` |
|      ! 0 | 5067 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 | 5068 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 | 5069 | `									pFrameStack = 0;` |
|      ! 0 | 5070 | `									rc = PH7_EXCEPTION;` |
|      ! 0 | 5071 | `									goto SkipFuncBody;` |
|        - | 5072 | `								}` |
|        6 | 5073 | `							}` |
|     1537 | 5074 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|     1537 | 5075 | `							pArg++;` |
|        5 | 5076 | `						}` |
|        - | 5077 | `					}` |
|      203 | 5078 | `					sArg.nIdx = nVariadicIdx; /* pObj may be stale here (aMemObj realloc) — use the saved index */` |
|      203 | 5079 | `					sArg.pUserData = 0;` |
|      203 | 5080 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       99 | 5081 | `				}` |
|      203 | 5082 | `				break; /* All remaining args consumed */` |
|        - | 5083 | `			}` |
|   167221 | 5084 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 5085 | `				/* An explicit null is NOT redirected to the default (PHP applies a` |
|        - | 5086 | `				 * default only for an omitted arg); it falls through to the type check` |
|        - | 5087 | `				 * below — TypeError for a non-nullable typed param, kept as null for a` |
|        - | 5088 | ``				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE`` |
|        - | 5089 | `				 * at compile time so its check accepts null. */` |
|        - | 5090 | `				/* Union type: dispatch to the shared coercion helper. */` |
|   166699 | 5091 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|      128 | 5092 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       82 | 5093 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       41 | 5094 | `						bCallIsStrict);` |
|       87 | 5095 | `					if( rcU != SXRET_OK ){` |
|        - | 5096 | `						const char *zGiven;` |
|       26 | 5097 | `						const char *zExpected = "union";` |
|        - | 5098 | `						char zBuf[128];` |
|        - | 5099 | `						char zTypeBuf[128];` |
|       26 | 5100 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|       14 | 5101 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       21 | 5102 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|       10 | 5103 | `							zGiven = "null";` |
|        6 | 5104 | `						}else{` |
|        6 | 5105 | `							zGiven = ph7_type_name(pArg);` |
|        - | 5106 | `						}` |
|       26 | 5107 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       26 | 5108 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|       11 | 5109 | `						}` |
|       37 | 5110 | `						rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|       22 | 5111 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       26 | 5112 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 5113 | `							goto Abort;` |
|        - | 5114 | `						}` |
|       26 | 5115 | `						PH7_MemObjRelease(pTos);` |
|       26 | 5116 | `						pTos = &pTos[-nCallArgs];` |
|       26 | 5117 | `						pFrameStack = 0;` |
|       26 | 5118 | `						rc = PH7_EXCEPTION;` |
|       26 | 5119 | `						goto SkipFuncBody;` |
|        - | 5120 | `					}` |
|       33 | 5121 | `				}else` |
|        - | 5122 | `				/* Make sure the given arguments are of the correct type.` |
|        - | 5123 | `				 * Nullable types (?type) allow null through without coercion. */` |
|   166612 | 5124 | `				if( aFormalArg[n].nType > 0` |
|    90550 | 5125 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|    14031 | 5126 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - | 5127 | `						/* Argument must be a class instance [i.e: object] */` |
|      433 | 5128 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - | 5129 | `						ph7_class *pClass;` |
|      433 | 5130 | `						int rcPseudo = VmCheckPseudoType(&(*pVm),pArg,pName);` |
|      433 | 5131 | `						if( rcPseudo == 0 ){` |
|        - | 5132 | `							/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - | 5133 | `							char zTypeBuf[128],zGivenBuf[128];` |
|        7 | 5134 | `							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|        4 | 5135 | `								&aFormalArg[n].sName,` |
|        2 | 5136 | `								VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 | 5137 | `								VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf)));` |
|        5 | 5138 | `							if( rc == PH7_ABORT ) goto Abort;` |
|        5 | 5139 | `							PH7_MemObjRelease(pTos);` |
|        5 | 5140 | `							pTos = &pTos[-nCallArgs];` |
|        5 | 5141 | `							pFrameStack = 0;` |
|        5 | 5142 | `							rc = PH7_EXCEPTION;` |
|        5 | 5143 | `							goto SkipFuncBody;` |
|        - | 5144 | `						}` |
|        - | 5145 | `						/* rcPseudo==1 accepts a pseudo-type; -1 real class. Resolve via` |
|        - | 5146 | `						 * VmResolveTypeClass (self/parent + interface/abstract, iLoadable=FALSE)` |
|        - | 5147 | `						 * and throw a catchable TypeError on mismatch — matching PHP — instead of` |
|        - | 5148 | `						 * the legacy warn + NULL-coerce. (Symmetric with the positional path.) */` |
|      429 | 5149 | `						pClass = (rcPseudo == 1) ? 0 : VmResolveTypeClass(&(*pVm),pName,pSelfHint);` |
|      429 | 5150 | `						if( pClass ){` |
|        - | 5151 | `							/* Reaching here means the param is non-nullable (the guard above` |
|        - | 5152 | ``							 * skips nullable+null; a `Type $x = null` default is marked`` |
|        - | 5153 | `							 * implicitly nullable at compile time). So ANY non-object —` |
|        - | 5154 | `							 * including an explicit null — is a TypeError, matching PHP` |
|        - | 5155 | `							 * (&& below short-circuits so instanceof only derefs an object). */` |
|      423 | 5156 | `							int bBad = !((pArg->iFlags & MEMOBJ_OBJ)` |
|      209 | 5157 | `								&& PH7_VmInstanceOf(((ph7_class_instance *)pArg->x.pOther)->pClass,pClass));` |
|      223 | 5158 | `							if( bBad ){` |
|        - | 5159 | `								char zTypeBuf[128],zGivenBuf[128];` |
|       45 | 5160 | `								rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|       28 | 5161 | `									&aFormalArg[n].sName,` |
|       28 | 5162 | `									VmSyStringToCStr(&pClass->sName,zTypeBuf,sizeof(zTypeBuf)),` |
|       14 | 5163 | `									VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf)));` |
|       31 | 5164 | `								if( rc == PH7_ABORT ) goto Abort;` |
|       31 | 5165 | `								PH7_MemObjRelease(pTos);` |
|       31 | 5166 | `								pTos = &pTos[-nCallArgs];` |
|       31 | 5167 | `								pFrameStack = 0;` |
|       31 | 5168 | `								rc = PH7_EXCEPTION;` |
|       31 | 5169 | `								goto SkipFuncBody;` |
|        - | 5170 | `							}` |
|      100 | 5171 | `						}` |
|    13801 | 5172 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       74 | 5173 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - | 5174 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 | 5175 | `							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|       10 | 5176 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 | 5177 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 5178 | `								goto Abort;` |
|        - | 5179 | `							}` |
|        - | 5180 | `							/* Skip function body, route through normal cleanup */` |
|       11 | 5181 | `							PH7_MemObjRelease(pTos);` |
|       11 | 5182 | `							pTos = &pTos[-nCallArgs];` |
|       11 | 5183 | `							pFrameStack = 0;` |
|       11 | 5184 | `							rc = PH7_EXCEPTION;` |
|       11 | 5185 | `							goto SkipFuncBody;` |
|       64 | 5186 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 5187 | `							char zTypeBuf[128];` |
|       55 | 5188 | `							rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pVmFunc,n+1,` |
|       34 | 5189 | `								&aFormalArg[n].sName,` |
|       34 | 5190 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|       17 | 5191 | `								ph7_type_name(pArg));` |
|       38 | 5192 | `							if( rc == PH7_ABORT ){` |
|        6 | 5193 | `								goto Abort;` |
|        - | 5194 | `							}` |
|       33 | 5195 | `							PH7_MemObjRelease(pTos);` |
|       33 | 5196 | `							pTos = &pTos[-nCallArgs];` |
|       33 | 5197 | `							pFrameStack = 0;` |
|       33 | 5198 | `							rc = PH7_EXCEPTION;` |
|       33 | 5199 | `							goto SkipFuncBody;` |
|        - | 5200 | `						}` |
|       13 | 5201 | `					}` |
|     6975 | 5202 | `				}` |
|   166601 | 5203 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 5204 | `					/* Pass by reference */` |
|      243 | 5205 | `					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){` |
|        - | 5206 | `						/* php 8.1: $GLOBALS cannot be passed by reference —` |
|        - | 5207 | `						 * a catchable Error with php's exact wording. */` |
|        - | 5208 | `						SyBlob sMsg;` |
|        3 | 5209 | `						SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 5210 | `						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|        2 | 5211 | `							&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        3 | 5212 | `						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 5213 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 5214 | `							goto Abort;` |
|        - | 5215 | `						}` |
|        3 | 5216 | `						PH7_MemObjRelease(pTos);` |
|        3 | 5217 | `						pTos = &pTos[-nCallArgs];` |
|        3 | 5218 | `						pFrameStack = 0;` |
|        3 | 5219 | `						rc = PH7_EXCEPTION;` |
|        3 | 5220 | `						goto SkipFuncBody;` |
|        - | 5221 | `					}` |
|      241 | 5222 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        2 | 5223 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|        3 | 5224 | `						 && (pArg->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){` |
|        - | 5225 | `							/* php: a non-lvalue bound to a by-ref parameter is a catchable Error.` |
|        - | 5226 | `							 * PH7 warned and silently passed by value (same site as the other` |
|        - | 5227 | `							 * binder above). call_user_func()'s deliberate copy is exempt. */` |
|        - | 5228 | `							SyBlob sMsg;` |
|        - | 5229 | `							sxi32 rcRef;` |
|      ! 0 | 5230 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 5231 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 5232 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 5233 | `							rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|      ! 0 | 5234 | `								SyBlobLength(&sMsg));` |
|      ! 0 | 5235 | `							SyBlobRelease(&sMsg);` |
|      ! 0 | 5236 | `							return (rcRef == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 5237 | `						}` |
|        - | 5238 | `						/* Switch to pass by value */` |
|        3 | 5239 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        2 | 5240 | `					}else{` |
|        - | 5241 | `						SyHashEntry *pRefEntry;` |
|        - | 5242 | `						/* Install the referenced variable in the private function frame */` |
|      239 | 5243 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|      239 | 5244 | `						if( pRefEntry == 0 ){` |
|      357 | 5245 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|      236 | 5246 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|      239 | 5247 | `							sArg.nIdx = pArg->nIdx;` |
|      239 | 5248 | `							sArg.pUserData = 0;` |
|      239 | 5249 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      118 | 5250 | `						}` |
|      239 | 5251 | `						pObj = 0;` |
|        - | 5252 | `					}` |
|      122 | 5253 | `				}else{` |
|        - | 5254 | `					/* Pass by value,make a copy of the given argument */` |
|   166361 | 5255 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 5256 | `				}` |
|    83511 | 5257 | `			}else{` |
|        - | 5258 | `				char zName[32];` |
|        - | 5259 | `				SyString sArgName;` |
|        - | 5260 | `				/* Set a dummy name */` |
|      527 | 5261 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      527 | 5262 | `				sArgName.zString = zName;` |
|        - | 5263 | `				/* Annonymous argument */` |
|      527 | 5264 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 5265 | `			}` |
|   167121 | 5266 | `			if( pObj ){` |
|   166885 | 5267 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 5268 | `				/* Insert argument index  */` |
|   166885 | 5269 | `				sArg.nIdx = pObj->nIdx;` |
|   166885 | 5270 | `				sArg.pUserData = 0;` |
|   166885 | 5271 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    83649 | 5272 | `			}` |
|   167121 | 5273 | `			PH7_MemObjRelease(pArg);` |
|   167121 | 5274 | `			pArg++;` |
|   167121 | 5275 | `			++n;` |
|        5 | 5276 | `		}` |
|        - | 5277 | `		} /* end named vs positional branch */` |
|        - | 5278 | `		/* Set up closure environment */` |
|   106200 | 5279 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 5280 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 5281 | `			ph7_value *pValue;` |
|        - | 5282 | `			sxu32 iEnv;` |
|     1793 | 5283 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|     3927 | 5284 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|     2139 | 5285 | `				pEnv = &aEnv[iEnv];` |
|     2139 | 5286 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 5287 | `					/* Do not install null value */` |
|     1731 | 5288 | `					continue;` |
|        - | 5289 | `				}` |
|      408 | 5290 | `				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       11 | 5291 | `				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|        - | 5292 | `					/* The Closure instance carries an explicit bound $this` |
|        - | 5293 | `					 * (bindTo/bind/call): it wins over the creation-time` |
|        - | 5294 | `					 * captured $this, php-exact. */` |
|        5 | 5295 | `					continue;` |
|        - | 5296 | `				}` |
|      409 | 5297 | `				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|        - | 5298 | `					/* Captured by reference: link the name to the shared slot` |
|        - | 5299 | `					 * (no copy), mirroring the by-ref argument install above. */` |
|      163 | 5300 | `					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|      243 | 5301 | `						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),` |
|      160 | 5302 | `							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|       80 | 5303 | `					}` |
|      163 | 5304 | `					continue;` |
|        - | 5305 | `				}` |
|      249 | 5306 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|      249 | 5307 | `				if( pValue == 0 ){` |
|      ! 0 | 5308 | `					continue;` |
|        - | 5309 | `				}` |
|        - | 5310 | `				/* Invalidate any prior representation */` |
|      249 | 5311 | `				PH7_MemObjRelease(pValue);` |
|        - | 5312 | `				/* Duplicate bound variable value */` |
|      249 | 5313 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|      127 | 5314 | `			}` |
|      894 | 5315 | `		}` |
|        - | 5316 | `		/* Too-few-arguments check, placed AFTER the passed arguments were` |
|        - | 5317 | `		 * installed and type-checked: php's RECV order means a type error on` |
|        - | 5318 | ``		 * a PASSED argument beats the count error (`f(int $x,$y)` called`` |
|        - | 5319 | `		 * f("str") is a TypeError, not ArgumentCountError). The passed args` |
|        - | 5320 | `		 * were already released by the install loop, so the standard throw` |
|        - | 5321 | `		 * exit leaks nothing. The named path never fires this (its per-hole` |
|        - | 5322 | `		 * check ran in-loop; n == nNonVariadic >= nRequired here). Hosted` |
|        - | 5323 | `		 * builtin FUNCTIONS (VM_FUNC_INTERNAL) are exempt — their PHL` |
|        - | 5324 | `		 * signatures don't always mirror php's true arity and their in-body` |
|        - | 5325 | `		 * self-checks own php's wording (stage-2 family); hosted-class` |
|        - | 5326 | `		 * METHODS get php's ZPP wording via VmThrowBuiltinTooFewArgs. */` |
|   106195 | 5327 | `		if( n < SySetUsed(&pVmFunc->aArgs)` |
|    56783 | 5328 | `		 && (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){` |
|        - | 5329 | `			sxu32 nNonVar,nReq;` |
|     5909 | 5330 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     5909 | 5331 | `			if( n < nReq ){` |
|       25 | 5332 | `				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;` |
|       25 | 5333 | `				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|        4 | 5334 | `					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        1 | 5335 | `						nPassed,nReq,nNonVar);` |
|        2 | 5336 | `				}else{` |
|       33 | 5337 | `					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       10 | 5338 | `						nPassed,nReq,nNonVar,TRUE);` |
|        - | 5339 | `				}` |
|       25 | 5340 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 5341 | `					goto Abort;` |
|        - | 5342 | `				}` |
|       25 | 5343 | `				PH7_MemObjRelease(pTos);` |
|       25 | 5344 | `				pTos = &pTos[-nCallArgs];` |
|       25 | 5345 | `				pFrameStack = 0;` |
|       25 | 5346 | `				rc = PH7_EXCEPTION;` |
|       25 | 5347 | `				goto SkipFuncBody;` |
|        - | 5348 | `			}` |
|     2941 | 5349 | `		}` |
|        - | 5350 | `		/* Process default values for remaining formal parameters */` |
|   118196 | 5351 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    12267 | 5352 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 5353 | `				/* Variadic parameter with no extra args — create empty array */` |
|      249 | 5354 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      249 | 5355 | `				if( pObj ){` |
|      249 | 5356 | `					PH7_MemObjToHashmap(pObj);` |
|      249 | 5357 | `					sArg.nIdx = pObj->nIdx;` |
|      249 | 5358 | `					sArg.pUserData = 0;` |
|      249 | 5359 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      122 | 5360 | `				}` |
|      249 | 5361 | `				n++;` |
|      249 | 5362 | `				break; /* Variadic is always last */` |
|        - | 5363 | `			}` |
|    12023 | 5364 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|    12021 | 5365 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|    12021 | 5366 | `				if( pObj ){` |
|        - | 5367 | `					/* Evaluate the default value and extract it's result */` |
|    12021 | 5368 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|    12021 | 5369 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 5370 | `						goto Abort;` |
|        - | 5371 | `					}` |
|        - | 5372 | `					/* Insert argument index */` |
|    12021 | 5373 | `					sArg.nIdx = pObj->nIdx;` |
|    12021 | 5374 | `					sArg.pUserData = 0;` |
|    12021 | 5375 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 5376 | `					/* Make sure the default argument is of the correct type.` |
|        - | 5377 | ``					 * A null default on an implicitly-nullable param (`int $x = null`)`` |
|        - | 5378 | `					 * must stay null — casting it to 0/""/false would diverge from PHP` |
|        - | 5379 | `					 * and contradict the explicit-null path, which now keeps it null. */` |
|    12016 | 5380 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     3960 | 5381 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)` |
|     1986 | 5382 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 5383 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 5384 | `						/* Cast to the desired type */` |
|      ! 0 | 5385 | `						xCast(pObj);` |
|      ! 0 | 5386 | `					}` |
|     6008 | 5387 | `				}` |
|     6008 | 5388 | `			}` |
|    12023 | 5389 | `			++n;` |
|        5 | 5390 | `		}` |
|        - | 5391 | `		} /* end VmCallArgMap scope */` |
|        - | 5392 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 5393 | `		 * does not return anything.` |
|        - | 5394 | `		 */` |
|   106178 | 5395 | `		PH7_MemObjRelease(pTos);` |
|   106178 | 5396 | `		pTos = &pTos[-nCallArgs];` |
|        - | 5397 | `		/* Allocate an operand stack (via the recycling allocator) and evaluate the` |
|        - | 5398 | `		 * function body. Size it to a tight static bound when the body is statically` |
|        - | 5399 | `		 * modelable (BYTECODE.md stage 7) — the big memory win for deep recursion,` |
|        - | 5400 | `		 * where one such stack lives per frame — falling back to the safe` |
|        - | 5401 | `		 * instruction-count bound otherwise.` |
|        - | 5402 | `		 *` |
|        - | 5403 | `		 * The bound is computed LAZILY on the first call and cached on the func` |
|        - | 5404 | `		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:` |
|        - | 5405 | `		 * self-computing on first use is fail-safe against any body-creation path` |
|        - | 5406 | `		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),` |
|        - | 5407 | `		 * where undersizing is a heap overflow; the amortized cost is one analysis` |
|        - | 5408 | `		 * per function. */` |
|        - | 5409 | `		{` |
|   106178 | 5410 | `			sxu32 nSlots = pVmFunc->nMaxStack;` |
|   106178 | 5411 | `			if( nSlots == 0 ){` |
|     8297 | 5412 | `				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);` |
|    12443 | 5413 | `				sxu32 nTight = VmComputeMaxStack(&(*pVm),` |
|     8292 | 5414 | `					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);` |
|     8297 | 5415 | `				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;` |
|     8297 | 5416 | `				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */` |
|     8297 | 5417 | `				pVmFunc->nMaxStack = nSlots;` |
|     4146 | 5418 | `			}` |
|   106178 | 5419 | `			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);` |
|        - | 5420 | `		}` |
|   106178 | 5421 | `		if( pFrameStack == 0 ){` |
|        - | 5422 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 5423 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 5424 | `				&pVmFunc->sName);` |
|      ! 0 | 5425 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 5426 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 5427 | `			}` |
|      ! 0 | 5428 | `			break;` |
|        - | 5429 | `		}` |
|    52982 | 5430 | `SkipFuncBody:` |
|   106312 | 5431 | `		if( pSelf ){` |
|        - | 5432 | `			/* Push class name */` |
|    25743 | 5433 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|    12869 | 5434 | `		}` |
|        - | 5435 | `		/* Increment nesting level */` |
|   106312 | 5436 | `		pVm->nRecursionDepth++;` |
|   106312 | 5437 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 5438 | `			/* Arg-binding threw: there is no body to run — finish the call` |
|        - | 5439 | `			 * immediately (no record is pushed). */` |
|        - | 5440 | `			VmCallRecord sCallee;` |
|      139 | 5441 | `			sCallee.pVmFunc = pVmFunc;` |
|      139 | 5442 | `			sCallee.pFrame = pFrame;` |
|      139 | 5443 | `			sCallee.pFrameStack = pFrameStack;` |
|      139 | 5444 | `			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|      139 | 5445 | `			sCallee.nLastRef = SXU32_HIGH;` |
|      139 | 5446 | `			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|      139 | 5447 | `			sState.pTos = pTos;` |
|      139 | 5448 | `			sState.pc = pc;` |
|      139 | 5449 | `			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);` |
|      139 | 5450 | `			pTos = sState.pTos;` |
|      139 | 5451 | `			pc = sState.pc;` |
|      139 | 5452 | `			if( rc == PH7_ABORT ){` |
|        - | 5453 | `				/* Abort processing immeditaley */` |
|      ! 0 | 5454 | `				goto Abort;` |
|      139 | 5455 | `			}else if( rc == PH7_SUSPEND ){` |
|      ! 0 | 5456 | `				goto Suspend;` |
|      139 | 5457 | `			}else if( rc == PH7_EXCEPTION ){` |
|       31 | 5458 | `				goto Exception;` |
|        - | 5459 | `			}` |
|       57 | 5460 | `		}else{` |
|        - | 5461 | `			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a` |
|        - | 5462 | `			 * call record (caller activation + in-flight call) and switch the` |
|        - | 5463 | `			 * loop's locals to the callee — a PHP->PHP call no longer grows` |
|        - | 5464 | `			 * the native stack. The record node is pool-allocated so` |
|        - | 5465 | `			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */` |
|   106178 | 5466 | `			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   106178 | 5467 | `			if( pRec ){` |
|   103532 | 5468 | `				pVm->pIdleCallFrames = (void *)pRec->pPrev;` |
|    51873 | 5469 | `			}else{` |
|     2651 | 5470 | `				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));` |
|        - | 5471 | `			}` |
|   106178 | 5472 | `			if( pRec == 0 ){` |
|        - | 5473 | `				/* OOM: undo the push-time accounting, tear the call down and` |
|        - | 5474 | `				 * raise the non-catchable fatal (the §3.1 OOM convention —` |
|        - | 5475 | `				 * never a silent NULL). */` |
|      ! 0 | 5476 | `				pVm->nRecursionDepth--;` |
|      ! 0 | 5477 | `				if( pSelf ){` |
|      ! 0 | 5478 | `					(void)SySetPop(&pVm->aSelf);` |
|      ! 0 | 5479 | `				}` |
|      ! 0 | 5480 | `				SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|      ! 0 | 5481 | `				VmLeaveFrame(&(*pVm));` |
|      ! 0 | 5482 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 5483 | `				goto Abort;` |
|        - | 5484 | `			}` |
|   106178 | 5485 | `			sState.pTos = pTos;` |
|   106178 | 5486 | `			sState.pc = pc;` |
|   106178 | 5487 | `			pRec->sCaller = sState;` |
|   106178 | 5488 | `			pRec->sCall.pVmFunc = pVmFunc;` |
|   106178 | 5489 | `			pRec->sCall.pFrame = pFrame;` |
|   106178 | 5490 | `			pRec->sCall.pFrameStack = pFrameStack;` |
|   106178 | 5491 | `			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|   106178 | 5492 | `			pRec->sCall.nLastRef = SXU32_HIGH;` |
|   106178 | 5493 | `			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|   106178 | 5494 | `			pRec->pPrev = pCallTop;` |
|   106178 | 5495 | `			pCallTop = pRec;` |
|        - | 5496 | `			/* Switch to the callee activation (what the recursive` |
|        - | 5497 | `			 * VmByteCodeExec entry used to set up). */` |
|   106178 | 5498 | `			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);` |
|   106178 | 5499 | `			pStack = pFrameStack;` |
|   106178 | 5500 | `			pTos = &pStack[-1];` |
|   106178 | 5501 | `			pc = 0;` |
|   106178 | 5502 | `			sState.aInstr = aInstr;` |
|   106178 | 5503 | `			sState.pStack = pStack;` |
|   106178 | 5504 | `			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */` |
|   106178 | 5505 | `			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */` |
|   106178 | 5506 | `			sState.pTos = pTos;` |
|   106178 | 5507 | `			sState.pc = 0;` |
|   106178 | 5508 | `			sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|   106178 | 5509 | `			sState.pEntryFrame = pVm->pFrame;` |
|   106178 | 5510 | `			sState.pResult = pRec->sCaller.pTos;` |
|   106178 | 5511 | `			sState.pLastRef = &pRec->sCall.nLastRef;` |
|   106178 | 5512 | `			sState.pEnforceRetFunc = VmFuncHasReturnType(pVmFunc) ? pVmFunc : 0;` |
|   106178 | 5513 | `			sState.is_callback = 0;` |
|   106178 | 5514 | `			sState.bReturnPropagates = 0;` |
|   106178 | 5515 | `			goto VmLoopFetch;` |
|        - | 5516 | `		}` |
|       57 | 5517 | `	}else{` |
|        - | 5518 | `		ph7_user_func *pFunc;` |
|        - | 5519 | `		ph7_context sCtx;` |
|        - | 5520 | `		ph7_value sRet;` |
|        - | 5521 | `		/* Look for an installed foreign function.` |
|        - | 5522 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 5523 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 5524 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 5525 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   939566 | 5526 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 5527 | `		{` |
|   939566 | 5528 | `		VmCallArgMap *pCallMap2 = pEffCallMap;` |
|   939566 | 5529 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 5530 | `			/* Compiler-qualified: try short name as global fallback */` |
|       32 | 5531 | `			const char *zShort = sName.zString;` |
|        - | 5532 | `			sxu32 i;` |
|      518 | 5533 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      490 | 5534 | `				if( sName.zString[i] == '\\' ){` |
|       46 | 5535 | `					zShort = &sName.zString[i + 1];` |
|       21 | 5536 | `				}` |
|      247 | 5537 | `			}` |
|       32 | 5538 | `			if( zShort != sName.zString ){` |
|       32 | 5539 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       32 | 5540 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       14 | 5541 | `			}` |
|       14 | 5542 | `		}` |
|        - | 5543 | `		} /* end VmCallArgMap namespace scope */` |
|   939566 | 5544 | `		if( pEntry == 0 ){` |
|        - | 5545 | `			/* php accepts the "Class::method" STATIC-callable string everywhere a` |
|        - | 5546 | `			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).` |
|        - | 5547 | `			 * Split on the first "::" and route through the shared array-callable` |
|        - | 5548 | `			 * machinery ([class-name, method-name]) instead of warning undefined. */` |
|        - | 5549 | `			sxu32 iSep;` |
|       19 | 5550 | `			int bScoped = 0;` |
|      159 | 5551 | `			for( iSep = 1 ; iSep + 2 < sName.nByte ; ++iSep ){` |
|      155 | 5552 | `				if( sName.zString[iSep] == ':' && sName.zString[iSep+1] == ':' ){` |
|       13 | 5553 | `					bScoped = 1;` |
|       13 | 5554 | `					break;` |
|        - | 5555 | `				}` |
|       73 | 5556 | `			}` |
|       19 | 5557 | `			if( bScoped ){` |
|       13 | 5558 | `				ph7_hashmap *pCbMap = PH7_NewHashmap(&(*pVm),0,0);` |
|       13 | 5559 | `				if( pCbMap ){` |
|        - | 5560 | `					ph7_value sCallable,sElem,sResult;` |
|        - | 5561 | `					sxi32 rcSm;` |
|       19 | 5562 | `					pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|       12 | 5563 | `						nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|       13 | 5564 | `					SySetReset(&aArg);` |
|       31 | 5565 | `					while( pArg < pTos ){` |
|       19 | 5566 | `						SySetPut(&aArg,(const void *)&pArg);` |
|       19 | 5567 | `						pArg++;` |
|        1 | 5568 | `					}` |
|       13 | 5569 | `					PH7_MemObjInit(pVm,&sElem);` |
|       13 | 5570 | `					PH7_MemObjStringAppend(&sElem,sName.zString,iSep);` |
|       13 | 5571 | `					PH7_HashmapInsert(pCbMap,0,&sElem);` |
|       13 | 5572 | `					PH7_MemObjRelease(&sElem);` |
|       13 | 5573 | `					PH7_MemObjInit(pVm,&sElem);` |
|       13 | 5574 | `					PH7_MemObjStringAppend(&sElem,&sName.zString[iSep+2],sName.nByte-(iSep+2));` |
|       13 | 5575 | `					PH7_HashmapInsert(pCbMap,0,&sElem);` |
|       13 | 5576 | `					PH7_MemObjRelease(&sElem);` |
|       13 | 5577 | `					PH7_MemObjInit(pVm,&sCallable);` |
|       13 | 5578 | `					sCallable.x.pOther = pCbMap;` |
|       13 | 5579 | `					MemObjSetType(&sCallable,MEMOBJ_HASHMAP);` |
|       13 | 5580 | `					PH7_MemObjInit(pVm,&sResult);` |
|       19 | 5581 | `					rcSm = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,(int)SySetUsed(&aArg),` |
|       12 | 5582 | `						(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|       13 | 5583 | `					SySetReset(&aArg);` |
|       13 | 5584 | `					PH7_MemObjRelease(&sCallable);` |
|       13 | 5585 | `					if( nCallArgs > 0 ){` |
|       13 | 5586 | `						VmPopOperand(&pTos,nCallArgs);` |
|        6 | 5587 | `					}` |
|       13 | 5588 | `					if( rcSm == PH7_ABORT ){` |
|      ! 0 | 5589 | `						PH7_MemObjRelease(&sResult);` |
|      ! 0 | 5590 | `						goto Abort;` |
|        - | 5591 | `					}` |
|       13 | 5592 | `					if( rcSm == PH7_EXCEPTION ){` |
|        - | 5593 | `						sxi32 iResumePc;` |
|      ! 0 | 5594 | `						PH7_MemObjRelease(&sResult);` |
|      ! 0 | 5595 | `						if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 5596 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 5597 | `							pc = iResumePc;` |
|      ! 0 | 5598 | `							break;` |
|        - | 5599 | `						}` |
|      ! 0 | 5600 | `						goto Exception;` |
|        - | 5601 | `					}` |
|       13 | 5602 | `					PH7_MemObjStore(&sResult,pTos);` |
|       13 | 5603 | `					PH7_MemObjRelease(&sResult);` |
|       13 | 5604 | `					break;` |
|        - | 5605 | `				}` |
|      ! 0 | 5606 | `			}` |
|        - | 5607 | `			/* Call to an undefined function is a catchable Error in php 8 — it does` |
|        - | 5608 | `			 * NOT warn and hand back null and carry on, which is what PH7 did (and` |
|        - | 5609 | `			 * which quietly turned a typo into a null-propagating program). */` |
|        - | 5610 | `			{` |
|        - | 5611 | `			SyBlob sMsg;` |
|        6 | 5612 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        6 | 5613 | `			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);` |
|        - | 5614 | `			/* Consume this call's captured spread runs so they don't leak into a` |
|        - | 5615 | `			 * later call (this path never reaches VmBuildEffectiveArgMap). */` |
|        6 | 5616 | `			if( pInstr->iP2 ){` |
|      ! 0 | 5617 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 5618 | `			}` |
|        - | 5619 | `			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the` |
|        - | 5620 | `			 * compile-time arg count on the stack, and this early exit skips the` |
|        - | 5621 | `			 * arg-building loop that the normal path uses, so popping only iP1 would` |
|        - | 5622 | `			 * strand the expanded elements and corrupt the enclosing expression. */` |
|        6 | 5623 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 5624 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 5625 | `			}` |
|        6 | 5626 | `			PH7_MemObjRelease(pTos);` |
|        8 | 5627 | `			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|        2 | 5628 | `				SyBlobLength(&sMsg));` |
|        6 | 5629 | `			SyBlobRelease(&sMsg);` |
|        6 | 5630 | `			if( rc == SXERR_ABORT ){` |
|        6 | 5631 | `				goto Abort;` |
|        - | 5632 | `			}` |
|      ! 0 | 5633 | `			goto Exception;` |
|        - | 5634 | `			}` |
|        - | 5635 | `		}` |
|   939550 | 5636 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 5637 | `		/* Host function (builtin): build the effective spread-key map so the` |
|        - | 5638 | `		 * name-forwarding builtins (call_user_func & friends) relay string keys as` |
|        - | 5639 | `		 * named args, and — critically — so this call's captured runs are consumed.` |
|        - | 5640 | `		 * pArg is the top base here (a builtin call pops no method-name slot). */` |
|  1409693 | 5641 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   939545 | 5642 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 5643 | `		/* Start collecting function arguments */` |
|   939550 | 5644 | `		SySetReset(&aArg);` |
|  2539142 | 5645 | `		while( pArg < pTos ){` |
|  1599597 | 5646 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1599597 | 5647 | `			pArg++;` |
|        5 | 5648 | `		}` |
|        - | 5649 | `		/* Assume a null return value */` |
|   939550 | 5650 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 5651 | `		/* Init the call context */` |
|   939550 | 5652 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 5653 | `		/* Hand the call-site named-argument map to the builtin so name-forwarding` |
|        - | 5654 | `		 * helpers (call_user_func & friends) can relay name: arguments — and the` |
|        - | 5655 | `		 * caller's strict_types mode — to the inner callback. Forwarded whole (not` |
|        - | 5656 | `		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even` |
|        - | 5657 | `		 * when its own call site is purely positional; only the two forwarding` |
|        - | 5658 | `		 * builtins read pArgMap, so this is inert for every other host function. */` |
|   939550 | 5659 | `		sCtx.pArgMap = pEffCallMap;` |
|        - | 5660 | `		{` |
|   939550 | 5661 | `		int nGiven = (int)SySetUsed(&aArg);` |
|        - | 5662 | `		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum` |
|        - | 5663 | `		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError` |
|        - | 5664 | `		 * before the C routine runs when called with too few arguments — instead` |
|        - | 5665 | `		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""` |
|        - | 5666 | `		 * return. The message wording matches php's ZPP output byte-for-byte. */` |
|   939550 | 5667 | `		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){` |
|      701 | 5668 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 5669 | `				"%z() expects %s %d argument%s, %d given",` |
|      232 | 5670 | `				&pFunc->sName,` |
|      464 | 5671 | `				pFunc->bAtLeast ? "at least" : "exactly",` |
|      464 | 5672 | `				(int)pFunc->nMinArg,` |
|      464 | 5673 | `				pFunc->nMinArg == 1 ? "" : "s",` |
|      232 | 5674 | `				nGiven);` |
|   939318 | 5675 | `		}else if( pFunc->bHasMaxArg && nGiven > (int)pFunc->nMaxArg ){` |
|        - | 5676 | `			/* php enforces the MAXIMUM as well, and PHL only did so where a` |
|        - | 5677 | `			 * builtin happened to hand-roll the check (51 of ~650), so` |
|        - | 5678 | ``			 * `microtime(1,2)`, `strlen("a","b")` and friends silently ignored`` |
|        - | 5679 | `			 * the extras. The count comes from the same signature table as the` |
|        - | 5680 | `			 * minimum; a variadic tail leaves nMaxArg at -1 and is exempt.` |
|        - | 5681 | `			 * php says "exactly" when the bounds coincide, "at most" otherwise. */` |
|      125 | 5682 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 5683 | `				"%z() expects %s %d argument%s, %d given",` |
|       40 | 5684 | `				&pFunc->sName,` |
|       68 | 5685 | `				((int)pFunc->nMinArg == (int)pFunc->nMaxArg && !pFunc->bAtLeast) ? "exactly" : "at most",` |
|       80 | 5686 | `				(int)pFunc->nMaxArg,` |
|       80 | 5687 | `				pFunc->nMaxArg == 1 ? "" : "s",` |
|       40 | 5688 | `				nGiven);` |
|  1408917 | 5689 | `		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,` |
|   939001 | 5690 | `			(ph7_value **)SySetBasePtr(&aArg))) ){` |
|        - | 5691 | `			/* TypeError thrown: rc carries the caught/uncaught status */` |
|      112 | 5692 | `		}else{` |
|        - | 5693 | `			/* Call the foreign function */` |
|   938792 | 5694 | `			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));` |
|        - | 5695 | `		}` |
|        - | 5696 | `		}` |
|        - | 5697 | `		/* Release the call context */` |
|   939550 | 5698 | `		VmReleaseCallContext(&sCtx);` |
|   939550 | 5699 | `		if( rc == PH7_ABORT ){` |
|        - | 5700 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 5701 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 5702 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      578 | 5703 | `			PH7_MemObjRelease(&sRet);` |
|      578 | 5704 | `			goto Abort;` |
|        - | 5705 | `		}` |
|   938976 | 5706 | `		if( rc != PH7_SUSPEND && pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 5707 | `			/* A throw raised inside this host function — directly` |
|        - | 5708 | `			 * (PH7_VmThrowException) or by a PHP callback it invoked — was` |
|        - | 5709 | `			 * caught by an INLINE try (generator body) THIS exec owns.` |
|        - | 5710 | `			 * VmThrowInline records only a pc-redirect: a direct builtin throw` |
|        - | 5711 | `			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION` |
|        - | 5712 | `			 * with the redirect pending, so the rc branches below never land` |
|        - | 5713 | `			 * it (pre-existing hole: explode("") or a throwing usort` |
|        - | 5714 | `			 * comparator inside a generator's try lost the catch AND the` |
|        - | 5715 | `			 * yield). Land at the redirect now — its drain to the try's` |
|        - | 5716 | `			 * operand base subsumes the args + name pops. */` |
|        5 | 5717 | `			PH7_MemObjRelease(&sRet);` |
|       17 | 5718 | `			PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 5719 | `		}` |
|   938972 | 5720 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 5721 | `			/* A callback invoked by this host function threw. If an in-place catch` |
|        - | 5722 | `			 * recorded a resume target owned by THIS body, resume at its landing pad` |
|        - | 5723 | `			 * (consuming the target); otherwise the exception was caught by an outer` |
|        - | 5724 | `			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW` |
|        - | 5725 | `			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which` |
|        - | 5726 | `			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */` |
|        - | 5727 | `			sxi32 iResumePc;` |
|     1043 | 5728 | `			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 5729 | `				/* Caught by an outer exec, or not caught here: propagate. */` |
|      203 | 5730 | `				goto Exception;` |
|        - | 5731 | `			}` |
|        - | 5732 | `			/* Exception was caught in place by THIS body's try: pop args and the` |
|        - | 5733 | `			 * result slot to restore the pre-try stack, then resume. */` |
|      845 | 5734 | `			PH7_MemObjRelease(&sRet);` |
|      845 | 5735 | `			if( nCallArgs > 0 ){` |
|      555 | 5736 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      275 | 5737 | `			}` |
|      845 | 5738 | `			VmPopOperand(&pTos,1);` |
|      845 | 5739 | `			pc = iResumePc;` |
|      845 | 5740 | `			break;` |
|        - | 5741 | `		}` |
|   937934 | 5742 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 5743 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 5744 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 5745 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 5746 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 5747 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 5748 | `			 * body), the user-function path above will handle re-saving. */` |
|      359 | 5749 | `			PH7_MemObjRelease(&sRet);` |
|      359 | 5750 | `			if( nCallArgs > 0 ){` |
|      359 | 5751 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      177 | 5752 | `			}` |
|        - | 5753 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 5754 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|      359 | 5755 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|      359 | 5756 | `			goto Suspend;` |
|        - | 5757 | `		}` |
|   937580 | 5758 | `		if( nCallArgs > 0 ){` |
|        - | 5759 | `			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack` |
|        - | 5760 | `			 * expanded the compile-time arg count on the stack, so popping iP1` |
|        - | 5761 | `			 * strands the extra elements (or, for an unpack that expanded to fewer` |
|        - | 5762 | `			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,` |
|        - | 5763 | `			 * reading a bogus value whose stray flags sent MemObjStore into the` |
|        - | 5764 | `			 * hashmap-release path and hung). Mirrors every other CALL exit. The` |
|        - | 5765 | `			 * function-name slot (pTos) receives the return value below. */` |
|   916806 | 5766 | `			VmPopOperand(&pTos,nCallArgs);` |
|   458771 | 5767 | `		}` |
|        - | 5768 | `		/* Save foreign function return value into the (now top) function-name slot */` |
|   937580 | 5769 | `		PH7_MemObjStore(&sRet,pTos);` |
|   937580 | 5770 | `		PH7_MemObjRelease(&sRet);` |
|        - | 5771 | `	}` |
|   937684 | 5772 | `	break;` |
|        - | 5773 | `				  }` |
|        - | 5774 | `/*` |
|        - | 5775 | ` * OP_CONSUME: P1 * *` |
|        - | 5776 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 5777 | ` */` |
|    26070 | 5778 | `case PH7_OP_CONSUME: {` |
|        - | 5779 | `	VmOpRc rcOp;` |
|    52145 | 5780 | `	sState.pTos = pTos;` |
|    52145 | 5781 | `	sState.pc = pc;` |
|    52145 | 5782 | `	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);` |
|    52145 | 5783 | `	pTos = sState.pTos;` |
|    52145 | 5784 | `	pc = sState.pc;` |
|    52145 | 5785 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5786 | `		goto Abort;` |
|    52145 | 5787 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5788 | `		goto Exception;` |
|        - | 5789 | `	}` |
|    52140 | 5790 | `	break;` |
|        - | 5791 | `					  }` |
|        - | 5792 |  |
|        - | 5793 | `		} /* Switch() */` |
| 16849012 | 5794 | `		pc++; /* Next instruction in the stream */` |
|        5 | 5795 | `	} /* For(;;) */` |
|   131356 | 5796 | `Done:` |
|        - | 5797 | `	/* A stacked callee completing lands here too (its result is already in` |
|        - | 5798 | `	 * sState.pResult — the caller's operand slot); Unwind's first iteration` |
|        - | 5799 | `	 * bottoms out identically for the record-less case. */` |
|   262926 | 5800 | `	rc = SXRET_OK;` |
|   262926 | 5801 | `	goto Unwind;` |
|      823 | 5802 | `Suspend:` |
|     1651 | 5803 | `	rc = PH7_SUSPEND;` |
|     1651 | 5804 | `	if( pCallTop != 0 ){` |
|        - | 5805 | `		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment` |
|        - | 5806 | `		 * instead of the lossy unwind. pc/nTos of the innermost activation were` |
|        - | 5807 | `		 * already saved into the ctx by VmSuspendCtx; capture the rest (the` |
|        - | 5808 | `		 * record chain, the innermost activation, the suspend-time top frame)` |
|        - | 5809 | `		 * so resume re-enters HERE, inside the innermost callee, like php. The` |
|        - | 5810 | `		 * records / frames / operand stacks stay alive — nothing is freed. Only` |
|        - | 5811 | `		 * fibers reach this (generators yield only at their body level, pCallTop` |
|        - | 5812 | `		 * == 0); a suspend inside a C->PHP callback was already rejected with a` |
|        - | 5813 | `		 * FiberError before it could arrive here. */` |
|      359 | 5814 | `		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));` |
|      359 | 5815 | `		if( pSeg == 0 ){` |
|        - | 5816 | `			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST` |
|        - | 5817 | `			 * pc/nTos into the ctx, so the lossy body-level fallback would resume` |
|        - | 5818 | `			 * the callee's pc against the body stack — silent corruption, exactly` |
|        - | 5819 | `			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the` |
|        - | 5820 | `			 * §3.1 convention shared with the stage-2 record-alloc OOM site). */` |
|      ! 0 | 5821 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 5822 | `			rc = PH7_ABORT;` |
|      ! 0 | 5823 | `			goto Unwind;` |
|        - | 5824 | `		}` |
|      359 | 5825 | `		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */` |
|      359 | 5826 | `		pSeg->sState = sState;` |
|      359 | 5827 | `		pSeg->pCallTop = pCallTop;` |
|      359 | 5828 | `		pSeg->pTopFrame = pVm->pFrame;` |
|      359 | 5829 | `		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;` |
|        - | 5830 | `		{` |
|        - | 5831 | `			VmCallFrame *pRec;` |
|      359 | 5832 | `			pSeg->nRecords = 0;` |
|     1015 | 5833 | `			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){` |
|      661 | 5834 | `				pSeg->nRecords++;` |
|      333 | 5835 | `			}` |
|        - | 5836 | `		}` |
|      359 | 5837 | `		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;` |
|        - | 5838 | `		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */` |
|      359 | 5839 | `		SySetRelease(&aArg);` |
|      359 | 5840 | `		return PH7_SUSPEND;` |
|        - | 5841 | `	}` |
|     1297 | 5842 | `	goto Unwind;` |
|      375 | 5843 | `Abort:` |
|      754 | 5844 | `	rc = PH7_ABORT;` |
|      754 | 5845 | `	goto Unwind;` |
|      404 | 5846 | `Exception:` |
|      813 | 5847 | `	rc = PH7_EXCEPTION;` |
|      808 | 5848 | `	goto Unwind;` |
|   132781 | 5849 | `Unwind:` |
|        - | 5850 | `	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration` |
|        - | 5851 | `	 * finishes the top record exactly as the old per-level native return did:` |
|        - | 5852 | `	 * for ABORT/EXCEPTION, first run what the popped activation's own` |
|        - | 5853 | `	 * Abort/Exception label used to do (clear its pending return, release its` |
|        - | 5854 | `	 * operands — a stacked activation never has bReturnPropagates set), then` |
|        - | 5855 | `	 * VmCallFinish routes in the restored caller (an in-place catch there` |
|        - | 5856 | `	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —` |
|        - | 5857 | `	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible` |
|        - | 5858 | `	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).` |
|        - | 5859 | `	 * At the bottom, VmExecFinalize hands the status to the native caller. */` |
|   133373 | 5860 | `	for(;;){` |
|   266542 | 5861 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|        - | 5862 | `			/* Drop any pending hook-RMW write-backs this activation armed — its` |
|        - | 5863 | `			 * statement is abandoned (only the innermost activation at throw time` |
|        - | 5864 | `			 * can own entries: the armed window spans exactly one instruction, so` |
|        - | 5865 | `			 * no OP_CALL record ever intervenes). */` |
|     2328 | 5866 | `			while( SySetUsed(&pVm->aHookRmw) > 0` |
|     2329 | 5867 | `			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){` |
|      ! 0 | 5868 | `				VmHookRmwDropTop(&(*pVm));` |
|      ! 0 | 5869 | `			}` |
|     1162 | 5870 | `		}` |
|   266542 | 5871 | `		if( pCallTop == 0 ){` |
|   160779 | 5872 | `			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);` |
|        - | 5873 | `		}` |
|   105768 | 5874 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|     1277 | 5875 | `			VmClearFrameReturn(sState.pEntryFrame);` |
|     2591 | 5876 | `			while( pTos >= pStack ){` |
|     1319 | 5877 | `				PH7_MemObjRelease(pTos);` |
|     1319 | 5878 | `				pTos--;` |
|        5 | 5879 | `			}` |
|      636 | 5880 | `		}` |
|        - | 5881 | `		{` |
|   105768 | 5882 | `			VmCallFrame *pRec = pCallTop;` |
|   105768 | 5883 | `			sState = pRec->sCaller;` |
|   105768 | 5884 | `			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);` |
|   105768 | 5885 | `			pCallTop = pRec->pPrev;` |
|   105768 | 5886 | `			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   105768 | 5887 | `			pVm->pIdleCallFrames = (void *)pRec;` |
|   105768 | 5888 | `			aInstr = sState.aInstr;` |
|   105768 | 5889 | `			pStack = sState.pStack;` |
|   105768 | 5890 | `			pTos = sState.pTos;` |
|   105768 | 5891 | `			pc = sState.pc;` |
|        - | 5892 | `		}` |
|   105768 | 5893 | `		if( rc == PH7_OK ){` |
|   105002 | 5894 | `			pc++; /* the loop-bottom increment this OP_CALL missed */` |
|   105002 | 5895 | `			goto VmLoopFetch;` |
|        - | 5896 | `		}` |
|        5 | 5897 | `	}` |
|    80569 | 5898 | `}` |
|        - | 5899 |  |
