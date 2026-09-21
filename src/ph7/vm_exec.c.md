# src/ph7/vm_exec.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2811/3303 lines (85.10%)

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
|      266 |   45 | `static int VmGrowOperandStack(ph7_vm *pVm, sxu32 nNeed,` |
|        - |   46 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |   47 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        4 |   48 | `{` |
|      270 |   49 | `	ph7_value *pOld = *ppStack;` |
|      270 |   50 | `	sxu32 nOldCap = pState->nStackCap;` |
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
|      270 |   64 | `	nReq = nNeed + pState->nStackOrig;` |
|      270 |   65 | `	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */` |
|      ! 0 |   66 | `		nReq = SXU32_HIGH;` |
|      ! 0 |   67 | `	}` |
|        - |   68 | `	/* SyMemBackendRealloc's size argument is sxu32, so the byte count` |
|        - |   69 | `	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack` |
|        - |   70 | `	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and` |
|        - |   71 | `	 * the init loop below would run off it. If the true requirement exceeds the` |
|        - |   72 | `	 * representable cap, treat it as OOM (the caller raises the guard error). */` |
|      270 |   73 | `	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);` |
|      270 |   74 | `	if( nReq > nMaxCap ){` |
|      ! 0 |   75 | `		return 0;` |
|        - |   76 | `	}` |
|      270 |   77 | `	if( nReq <= nOldCap ){` |
|      161 |   78 | `		return 1; /* already fits — no growth needed */` |
|        - |   79 | `	}` |
|      112 |   80 | `	nNewCap = nReq;` |
|        - |   81 | `	/* Amortize repeated spreads in one argument list: at least double, but never` |
|        - |   82 | `	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't` |
|        - |   83 | `	 * itself overflow.) */` |
|      112 |   84 | `	if( nNewCap < nOldCap * 2 ){` |
|      112 |   85 | `		sxu32 nDbl = nOldCap * 2;` |
|      112 |   86 | `		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }` |
|      112 |   87 | `		if( nNewCap < nDbl ){ nNewCap = nDbl; }` |
|       54 |   88 | `	}` |
|      166 |   89 | `	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,` |
|       54 |   90 | `		nNewCap * sizeof(ph7_value));` |
|      112 |   91 | `	if( pNew == 0 ){` |
|      ! 0 |   92 | `		return 0; /* OOM: caller keeps pOld and raises the guard error */` |
|        - |   93 | `	}` |
|        - |   94 | `	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */` |
|     7056 |   95 | `	for( i = nOldCap; i < nNewCap; i++ ){` |
|     6948 |   96 | `		PH7_MemObjInit(pVm, &pNew[i]);` |
|     6948 |   97 | `		pNew[i].nIdx = SXU32_HIGH;` |
|     3476 |   98 | `	}` |
|        - |   99 | `	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */` |
|      112 |  100 | `	*ppTos = pNew + (*ppTos - pOld);` |
|      112 |  101 | `	*ppStack = pNew;` |
|      112 |  102 | `	pState->pStack = pNew + (pState->pStack - pOld);` |
|      112 |  103 | `	pState->pTos = pNew + (pState->pTos - pOld);` |
|      112 |  104 | `	pState->nStackCap = nNewCap;` |
|      112 |  105 | `	if( pCallTop ){` |
|        - |  106 | `		/* This activation is a trampoline callee: its record owns the buffer. */` |
|       80 |  107 | `		pCallTop->sCall.pFrameStack = pNew;` |
|       80 |  108 | `		pCallTop->sCall.nStackCap = nNewCap;` |
|       41 |  109 | `	}else{` |
|        - |  110 | `		/* Base activation: the native entry frees *ppBaseOwner (and, for a` |
|        - |  111 | `		 * resumable coroutine, persists *pnBaseCap across suspend/resume). */` |
|       33 |  112 | `		if( ppBaseOwner ){ *ppBaseOwner = pNew; }` |
|       33 |  113 | `		if( pnBaseCap ){ *pnBaseCap = nNewCap; }` |
|        - |  114 | `	}` |
|        - |  115 | `	/* Re-anchor this activation's captured spread runs (pStart into the old` |
|        - |  116 | `	 * buffer). Enclosing-activation runs live in other buffers — leave them. */` |
|      112 |  117 | `	nRun = SySetUsed(&pVm->aSpreadRun);` |
|      112 |  118 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      112 |  119 | `	for( i = 0; i < nRun; i++ ){` |
|      ! 0 |  120 | `		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){` |
|      ! 0 |  121 | `			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);` |
|      ! 0 |  122 | `		}` |
|      ! 0 |  123 | `	}` |
|      112 |  124 | `	return 1;` |
|      137 |  125 | `}` |
|        - |  126 | `/*` |
|        - |  127 | ` * Ensure the running activation's operand stack can hold a spread of nEntry` |
|        - |  128 | ` * elements pushed at *ppTos (the source slot becomes the first element, so the` |
|        - |  129 | ` * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.` |
|        - |  130 | ` * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.` |
|        - |  131 | ` */` |
|      302 |  132 | `static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,` |
|        - |  133 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |  134 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        4 |  135 | `{` |
|        - |  136 | `	sxu32 nNeed;` |
|      306 |  137 | `	if( nEntry == 0 ){` |
|       39 |  138 | `		return 1; /* empty spread never grows the stack */` |
|        - |  139 | `	}` |
|      270 |  140 | `	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);` |
|      403 |  141 | `	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,` |
|      133 |  142 | `		pCallTop, ppBaseOwner, pnBaseCap);` |
|      155 |  143 | `}` |
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
|        - |  163 | `static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase);` |
|  9342780 |  164 | `static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)` |
|        5 |  165 | `{` |
|  9342785 |  166 | `	if( rcTerm != PH7_SUSPEND ){` |
|  9341473 |  167 | `		VmDiscardFinallyActions(&(*pVm),pState->nFinallyActBase);` |
|  4670734 |  168 | `	}` |
|  9342785 |  169 | `	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){` |
|  7894419 |  170 | `		VmClearFrameReturn(pState->pEntryFrame);` |
|  3947207 |  171 | `	}` |
|  9342785 |  172 | `	SySetRelease(pArg);` |
|  9342785 |  173 | `	if( rcTerm == PH7_ABORT \|\| rcTerm == PH7_EXCEPTION ){` |
|   802903 |  174 | `		while( pTos >= pState->pStack ){` |
|   401831 |  175 | `			PH7_MemObjRelease(pTos);` |
|   401831 |  176 | `			pTos--;` |
|        5 |  177 | `		}` |
|   200536 |  178 | `	}` |
|  9342785 |  179 | `	return rcTerm;` |
|        5 |  180 | `}` |
|        - |  181 | `/*` |
|        - |  182 | ` * Discard the pending finally ACTIONS an activation queued but never consumed,` |
|        - |  183 | ` * releasing what they own (an FA_RETHROW's held exception ref; an FA_RETURN's` |
|        - |  184 | `` * value). A `return` inside a finally that was ENTERED VIA THE THROW REDIRECT`` |
|        - |  185 | ` * (VmThrowInline queued an FA_RETHROW and jumped into the finally body)` |
|        - |  186 | ` * short-circuits that finally's OP_END_FINALLY — OP_SET_FINALLY_RET finds no` |
|        - |  187 | ` * remaining handler and completes the body directly — so the queued action was` |
|        - |  188 | ` * ORPHANED on pVm->aFinallyAction. Left there, an ENCLOSING function's next` |
|        - |  189 | ` * OP_END_FINALLY pops the orphan instead of its own action (re-raising a` |
|        - |  190 | ` * swallowed exception / hijacking control), and on a coroutine body it leaks` |
|        - |  191 | ` * into the resumer's scope. Called at every activation end (record pop and` |
|        - |  192 | ` * exec finalize), never on SUSPEND (a suspended body's pending actions are` |
|        - |  193 | ` * parked base-relative by VmParkCtxState and must survive).` |
|        - |  194 | ` */` |
| 11521435 |  195 | `static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase)` |
|        5 |  196 | `{` |
| 11521450 |  197 | `	while( SySetUsed(&pVm->aFinallyAction) > nBase ){` |
|       13 |  198 | `		VmFinallyAction *pAct = (VmFinallyAction *)SySetPeek(&pVm->aFinallyAction);` |
|       13 |  199 | `		if( pAct->eKind == PH7_FA_RETHROW && pAct->pExc ){` |
|        7 |  200 | `			PH7_ClassInstanceUnref(pAct->pExc);` |
|        9 |  201 | `		}else if( pAct->eKind == PH7_FA_RETURN ){` |
|        3 |  202 | `			PH7_MemObjRelease(&pAct->sRet);` |
|        1 |  203 | `		}` |
|       13 |  204 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|        3 |  205 | `	}` |
| 11521440 |  206 | `}` |
|        - |  207 | `/*` |
|        - |  208 | ` * Finish one user-function call at the "pop" boundary of the callee's` |
|        - |  209 | ` * activation: pop-time accounting (recursion depth, aSelf), by-ref-return` |
|        - |  210 | ` * fixup, callee-threw routing (inline resume / recorded resume / propagate),` |
|        - |  211 | ` * operand-stack free and frame teardown. Extracted verbatim from the OP_CALL` |
|        - |  212 | ` * epilogue (BYTECODE.md stage 1) so the stage-2 trampoline can run the same` |
|        - |  213 | ` * code when a record is popped at OP_DONE instead of after a native return.` |
|        - |  214 | ` * pCaller->pc / pCaller->pTos are authoritative across this boundary; the` |
|        - |  215 | ` * dispatch loop syncs its locals around the call. Returns PH7_OK (continue` |
|        - |  216 | ` * the caller, possibly at a redirected pc), PH7_ABORT, PH7_SUSPEND (the ctx` |
|        - |  217 | ` * state was re-saved at the caller's level) or PH7_EXCEPTION.` |
|        - |  218 | ` */` |
|  2180205 |  219 | `static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)` |
|        5 |  220 | `{` |
|        - |  221 | `	ph7_value *pObj;` |
|        - |  222 | `	/* Decrement nesting level */` |
|  2180210 |  223 | `	pVm->nRecursionDepth--;` |
|  2180210 |  224 | `	if( pCallee->bSelfPushed ){` |
|        - |  225 | `		/* Pop class name */` |
|  1971245 |  226 | `		(void)SySetPop(&pVm->aSelf);` |
|   985620 |  227 | `	}` |
|  2180210 |  228 | `	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  229 | `		/* Return by reference,reflect that */` |
|       52 |  230 | `		if( pCallee->nLastRef != SXU32_HIGH ){` |
|       52 |  231 | `			VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pCallee->pFrame->sLocal);` |
|        - |  232 | `			sxu32 i;` |
|        - |  233 | `			/* Make sure the referenced object is not a local variable */` |
|       98 |  234 | `			for( i = 0 ; i < SySetUsed(&pCallee->pFrame->sLocal) ; ++i ){` |
|       48 |  235 | `				if( pCallee->nLastRef == aSlot[i].nIdx ){` |
|      ! 0 |  236 | `					pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pCallee->nLastRef);` |
|      ! 0 |  237 | `					if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  238 | `						VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  239 | `							"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  240 | `							&pCallee->pVmFunc->sName);` |
|      ! 0 |  241 | `					}` |
|      ! 0 |  242 | `					pCallee->nLastRef = SXU32_HIGH;` |
|      ! 0 |  243 | `					break;` |
|        - |  244 | `				}` |
|       25 |  245 | `			}` |
|       27 |  246 | `		}else{` |
|      ! 0 |  247 | `			if( (pCaller->pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  248 | `				VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  249 | `					"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  250 | `					&pCallee->pVmFunc->sName);` |
|      ! 0 |  251 | `			}` |
|        - |  252 | `		}` |
|       52 |  253 | `		pCaller->pTos->nIdx = pCallee->nLastRef;` |
|       25 |  254 | `	}` |
|  2180210 |  255 | `	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  256 | `		/* The callee threw (or its finally threw past it). If an in-place catch` |
|        - |  257 | `		 * recorded a resume target owned by THIS caller's body, resume there and` |
|        - |  258 | `		 * consume the target (VmRecordedResume); when the catcher is an outer exec` |
|        - |  259 | `		 * — or this is a callback with no bytecode to resume into — propagate so` |
|        - |  260 | `		 * the owning exec lands. This replaces the old "is the caller's parent a` |
|        - |  261 | `		 * resumable try frame" test, which resumed at the caller's OWN try even` |
|        - |  262 | `		 * when the finally's throw was caught further out, losing that catch's` |
|        - |  263 | `		 * return (ROOT B, face c). */` |
|        - |  264 | `		sxi32 iResumePc;` |
|   601429 |  265 | `		VmFrame *pParentFrame = pCallee->pFrame->pParent;` |
|   601429 |  266 | `		if( !pCaller->is_callback && pVm->pInlineInstr == (void *)pCaller->aInstr ){` |
|        - |  267 | `			/* ROOT C: the callee's throw was caught by an inline try in THIS caller` |
|        - |  268 | `			 * (generator body). Drain the operand stack (incl. the unwritten result` |
|        - |  269 | `			 * slot) to the try's base and land at its catch/finally. */` |
|        5 |  270 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iInlineDrain ){` |
|        3 |  271 | `				PH7_MemObjRelease(pCaller->pTos);` |
|        3 |  272 | `				pCaller->pTos--;` |
|        1 |  273 | `			}` |
|        3 |  274 | `			pCaller->pc = (sxi32)pVm->iInlinePc - 1;` |
|        3 |  275 | `			pVm->pInlineInstr = 0;` |
|        3 |  276 | `			rc = PH7_OK;` |
|   601428 |  277 | `		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){` |
|        - |  278 | `			/* Pop the result, then drain any abandoned outer-expression operands` |
|        - |  279 | `			 * to the catching try's base (like the inline branch above) — the` |
|        - |  280 | `			 * ops that would have consumed them were abandoned by the throw, and` |
|        - |  281 | `` 			 * leaving them leaks one slot per caught throw (`try { $a = 1 + f(); }` `` |
|        - |  282 | `			 * in a loop overflowed the operand stack). */` |
|   200921 |  283 | `			VmPopOperand(&pCaller->pTos,1);` |
|   800945 |  284 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iResumeStackDepth ){` |
|   600027 |  285 | `				PH7_MemObjRelease(pCaller->pTos);` |
|   600027 |  286 | `				pCaller->pTos--;` |
|        3 |  287 | `			}` |
|   200921 |  288 | `			pCaller->pc = iResumePc;` |
|   200921 |  289 | `			rc = PH7_OK;` |
|   100463 |  290 | `		}else{` |
|   400511 |  291 | `			if( pParentFrame->pParent ){` |
|   400507 |  292 | `				rc = PH7_EXCEPTION;` |
|   200256 |  293 | `			}else{` |
|        - |  294 | `				/* Continue normal execution */` |
|        6 |  295 | `				rc = PH7_OK;` |
|        - |  296 | `			}` |
|        - |  297 | `		}` |
|   300712 |  298 | `	}` |
|        - |  299 | `	/* Recycle the operand stack for the next same-size call (BYTECODE stage 7),` |
|        - |  300 | `	 * or free it if the pool is full. Its allocated size is tracked in` |
|        - |  301 | `	 * pCallee->nStackCap (nMaxStack + VM_STACK_GUARD, or larger if an OP_SPREAD grew` |
|        - |  302 | `	 * it) — exactly what the buffer holds. (NULL when the function body was skipped.)` |
|        - |  303 | `	 *` |
|        - |  304 | `	 * Never on rc == PH7_SUSPEND: that path (unreachable in the stage-4 model,` |
|        - |  305 | `	 * where a deep suspend parks its whole record segment before reaching here)` |
|        - |  306 | `	 * would leave the callee stack owned by the suspended ctx, so recycling it` |
|        - |  307 | `	 * would hand a live fiber's operand stack to the next call. The guard keeps` |
|        - |  308 | `	 * that invariant explicit and robust to future coroutine changes. */` |
|  2180210 |  309 | `	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){` |
|        - |  310 | `		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee` |
|        - |  311 | `		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly` |
|        - |  312 | `		 * the allocated slot count either way. */` |
|  2179972 |  313 | `		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap);` |
|  1090146 |  314 | `	}` |
|        - |  315 | `	/* Leave the frame */` |
|  2180210 |  316 | `	VmLeaveFrame(&(*pVm));` |
|  2180210 |  317 | `	if( rc == PH7_ABORT ){` |
|      336 |  318 | `		return PH7_ABORT;` |
|        - |  319 | `	}` |
|  2179878 |  320 | `	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  321 | `		/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  322 | `		 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  323 | `		 * overwriting the state saved by the inner level.` |
|        - |  324 | `		 * pTos points to the result slot (not yet written).` |
|        - |  325 | `		 * Save nTos one below so resume pushes at the result slot. */` |
|      ! 0 |  326 | `		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);` |
|      ! 0 |  327 | `		return PH7_SUSPEND;` |
|        - |  328 | `	}` |
|  2179878 |  329 | `	if( rc == PH7_EXCEPTION ){` |
|   400507 |  330 | `		return PH7_EXCEPTION;` |
|        - |  331 | `	}` |
|  1779376 |  332 | `	return PH7_OK;` |
|  1090270 |  333 | `}` |
|        - |  334 | `/*` |
|        - |  335 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  336 | ` *` |
|        - |  337 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  338 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  339 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  340 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  341 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  342 | ` * then the program execution is halted.` |
|        - |  343 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  344 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  345 | ` * or to reset the VM to it's initial state.` |
|        - |  346 | ` */` |
|        - |  347 | `static sxi32 VmByteCodeExecBody(ph7_vm *pVm,VmInstr *aInstr,ph7_value *pStack,int nTos,` |
|        - |  348 | `	ph7_value *pResult,sxu32 *pLastRef,int is_callback,sxi32 nPc,` |
|        - |  349 | `	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,VmParkedSegment *pAdoptSegment,` |
|        - |  350 | `	ph7_value **ppBaseOwner,sxu32 *pnBaseCap,sxu32 nStackOrig);` |
|        - |  351 | `/*` |
|        - |  352 | ` * Native-nesting guard around the executor. PHP->PHP calls run iteratively` |
|        - |  353 | ` * (the stage-2 trampoline), but every OTHER (re-)entry — mini-programs,` |
|        - |  354 | ` * C->PHP callbacks, ctx start/resume, eval/include — is still one real C` |
|        - |  355 | ` * activation of VmByteCodeExecBody. nMaxDepth no longer bounds them (it is` |
|        - |  356 | ` * PHP call depth, raisable to memory-bound values since the clamp removal),` |
|        - |  357 | ` * so this counter is what actually protects the C stack: recursive` |
|        - |  358 | ` * eval/include towers, nested coroutine-resume chains and self-recursive` |
|        - |  359 | ` * C-callback compositions hit a clean fatal instead of overflowing. The limit` |
|        - |  360 | ` * lives in pVm->nMaxNativeDepth — a per-platform default (256 host / 16 small-` |
|        - |  361 | ` * stack embedders, VmInit) overridable via PH7_VM_CONFIG_NATIVE_DEPTH. This is` |
|        - |  362 | ` * still a coarse frame-count net rather than php's stack-byte measurement, so` |
|        - |  363 | ` * the host default is conservative — well below the old config clamp's <1024` |
|        - |  364 | ` * ceiling so it holds on the fattest frames (the callback path drags in` |
|        - |  365 | ` * usort/mergesort/trampoline C frames per re-entry, and instrumented builds` |
|        - |  366 | ` * inflate every frame), while far beyond any realistic eval/include/callback` |
|        - |  367 | ` * nesting.` |
|        - |  368 | ` */` |
|  9343138 |  369 | `PH7_PRIVATE sxi32 VmByteCodeExec(` |
|        - |  370 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  371 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  372 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  373 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  374 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  375 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  376 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  377 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  378 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  379 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|        - |  380 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|        - |  381 | `	ph7_value **ppBaseOwner, /* Storage slot the native entry frees for this invocation's BASE (pCallTop==0) operand stack — a local, pVm->aOps or pCtx->pStack. An OP_SPREAD that grows the base stack writes the new pointer here so the entry frees the right buffer. */` |
|        - |  382 | `	sxu32 *pnBaseCap, /* Storage for the base stack's capacity (resumable coroutines persist it across suspend/resume); updated alongside *ppBaseOwner on base-stack growth. Also the initial capacity read at entry. */` |
|        - |  383 | `	sxu32 nStackOrig /* The base stack's ORIGINAL (ungrown) allocation size. Unlike *pnBaseCap (which is the CURRENT, possibly-grown capacity on a coroutine resume), this is fixed, so OP_SPREAD growth headroom stays bounded across resumes. */` |
|        - |  384 | `	)` |
|        5 |  385 | `{` |
|        - |  386 | `	sxi32 rc;` |
|        - |  387 | `	sxi32 nSavedBrc;` |
|        - |  388 | `	sxu32 nSavedLine;` |
|  9343143 |  389 | `	if( VmNativeNestingExceeded(pVm) ){` |
|        5 |  390 | `		return VmNativeNestingFatal(pVm);` |
|        - |  391 | `	}` |
|        - |  392 | `	/* A fresh native exec entered while a C-boundary throw is parked` |
|        - |  393 | `	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the` |
|        - |  394 | `	 * very opcode that swallowed the throw) must run CLEAN: the parked status` |
|        - |  395 | `	 * belongs to the interrupted outer exec's fetch-point router, not to this` |
|        - |  396 | `	 * one. Save+clear on entry, merge back on exit — the outer status is` |
|        - |  397 | `	 * restored unless this exec parked its own unconsumed (newer) one, with` |
|        - |  398 | `	 * PH7_ABORT dominating either way. */` |
|  9343139 |  399 | `	nSavedBrc = pVm->nBoundaryRc;` |
|  9343139 |  400 | `	pVm->nBoundaryRc = 0;` |
|        - |  401 | `	/* The executing source line belongs to the ACTIVATION. A nested body -- a called` |
|        - |  402 | `	 * function, but equally an attribute-default or default-argument mini-program --` |
|        - |  403 | `	 * runs its own bytecode with its own lines, so it must not leave the caller` |
|        - |  404 | ``	 * reporting the callee's position: `new Exception` stamped line 1 because the`` |
|        - |  405 | ``	 * class's `protected $message = '';` default ran (from the embedded chunk) between`` |
|        - |  406 | `	 * OP_NEW and the stamp. Save on entry, restore on exit. */` |
|  9343139 |  407 | `	nSavedLine = pVm->nCurLine;` |
|  9343139 |  408 | `	pVm->nVmExecDepth++;` |
| 14014706 |  409 | `	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,` |
|  4671567 |  410 | `		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);` |
|  9343139 |  411 | `	pVm->nVmExecDepth--;` |
|  9343139 |  412 | `	pVm->nCurLine = nSavedLine;` |
|  9343139 |  413 | `	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT \|\| pVm->nBoundaryRc == 0) ){` |
|       17 |  414 | `		pVm->nBoundaryRc = nSavedBrc;` |
|        7 |  415 | `	}` |
|  9343139 |  416 | `	return rc;` |
|  4671574 |  417 | `}` |
|        - |  418 | `/*` |
|        - |  419 | ` * D1 commit 2: allocate a captured lvalue path for a deferred element/property call arg.` |
|        - |  420 | ` * eRoot picks the root container: 0 = a real aMemObj slot (nRootIdx), 1 = an undefined` |
|        - |  421 | ` * variable to vivify by name at resolve time (pName, a VM-lifetime bytecode string, borrowed),` |
|        - |  422 | ` * 2 = a string base (subscripting a string -> php refuses a by-ref bind). The struct owns its` |
|        - |  423 | ` * step array and each step's key/name; PH7_MemObjRelease frees it via VmFreeDeferredPath.` |
|        - |  424 | ` */` |
|    23524 |  425 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName)` |
|        5 |  426 | `{` |
|    23529 |  427 | `	VmDeferredPath *pPath = (VmDeferredPath *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDeferredPath));` |
|    23529 |  428 | `	if( pPath == 0 ){` |
|      ! 0 |  429 | `		return 0;` |
|        - |  430 | `	}` |
|    23529 |  431 | `	SyZero(pPath,sizeof(VmDeferredPath));` |
|    23529 |  432 | `	pPath->pAlloc = &pVm->sAllocator;` |
|    23529 |  433 | `	pPath->eRoot = eRoot;` |
|    23529 |  434 | `	pPath->nRootIdx = nRootIdx;` |
|    23529 |  435 | `	if( eRoot == 1 && pName ){` |
|        3 |  436 | `		pPath->sRootName = *pName; /* borrowed VM-lifetime bytes, not copied */` |
|        1 |  437 | `	}` |
|    23529 |  438 | `	return pPath;` |
|    11882 |  439 | `}` |
|    23542 |  440 | `static VmDeferStep * VmDeferPathGrow(VmDeferredPath *pPath)` |
|        5 |  441 | `{` |
|    23547 |  442 | `	if( pPath->nStep >= pPath->nAlloc ){` |
|    23529 |  443 | `		sxu32 nNew = pPath->nAlloc ? pPath->nAlloc * 2 : 4;` |
|    35406 |  444 | `		VmDeferStep *aNew = (VmDeferStep *)SyMemBackendRealloc(pPath->pAlloc,pPath->aStep,` |
|    11877 |  445 | `			nNew * sizeof(VmDeferStep));` |
|    23529 |  446 | `		if( aNew == 0 ){` |
|      ! 0 |  447 | `			return 0;` |
|        - |  448 | `		}` |
|    23529 |  449 | `		pPath->aStep = aNew;` |
|    23529 |  450 | `		pPath->nAlloc = nNew;` |
|    11877 |  451 | `	}` |
|    23547 |  452 | `	return &pPath->aStep[pPath->nStep];` |
|    11891 |  453 | `}` |
|        - |  454 | `/* Append an array-element step, deep-copying the index value (the caller releases pKey). */` |
|    23450 |  455 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey)` |
|        5 |  456 | `{` |
|    23455 |  457 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|    23455 |  458 | `	if( pStep == 0 ){` |
|      ! 0 |  459 | `		return SXERR_MEM;` |
|        - |  460 | `	}` |
|    23455 |  461 | `	pStep->isProp = 0;` |
|    23455 |  462 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|    23455 |  463 | `	PH7_MemObjInit(pKey->pVm,&pStep->sKey);` |
|    23455 |  464 | `	PH7_MemObjStore(pKey,&pStep->sKey);` |
|    23455 |  465 | `	pPath->nStep++;` |
|    23455 |  466 | `	return SXRET_OK;` |
|    11845 |  467 | `}` |
|        - |  468 | `/* Append an object-property step, owning a private copy of the name bytes. */` |
|       92 |  469 | `PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName)` |
|        2 |  470 | `{` |
|       94 |  471 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|        - |  472 | `	char *zCopy;` |
|       94 |  473 | `	if( pStep == 0 ){` |
|      ! 0 |  474 | `		return SXERR_MEM;` |
|        - |  475 | `	}` |
|       94 |  476 | `	zCopy = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|       94 |  477 | `	if( zCopy == 0 ){` |
|      ! 0 |  478 | `		return SXERR_MEM;` |
|        - |  479 | `	}` |
|       94 |  480 | `	pStep->isProp = 1;` |
|       94 |  481 | `	pStep->zProp = zCopy;` |
|       94 |  482 | `	SyStringInitFromBuf(&pStep->sProp,zCopy,pName->nByte);` |
|       94 |  483 | `	pPath->nStep++;` |
|       94 |  484 | `	return SXRET_OK;` |
|       48 |  485 | `}` |
|        - |  486 | `/* Release a captured lvalue path and everything it owns (element keys, property names). */` |
|    23524 |  487 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath)` |
|        5 |  488 | `{` |
|        - |  489 | `	sxu32 i;` |
|    23529 |  490 | `	if( pPath == 0 ){` |
|      ! 0 |  491 | `		return;` |
|        - |  492 | `	}` |
|    47071 |  493 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|    23547 |  494 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|    23547 |  495 | `		if( pStep->isProp ){` |
|       94 |  496 | `			if( pStep->zProp ){` |
|       94 |  497 | `				SyMemBackendFree(pPath->pAlloc,pStep->zProp);` |
|       46 |  498 | `			}` |
|       48 |  499 | `		}else{` |
|    23455 |  500 | `			PH7_MemObjRelease(&pStep->sKey);` |
|        - |  501 | `		}` |
|    11891 |  502 | `	}` |
|    23529 |  503 | `	if( pPath->aStep ){` |
|    23529 |  504 | `		SyMemBackendFree(pPath->pAlloc,pPath->aStep);` |
|    11877 |  505 | `	}` |
|    23529 |  506 | `	SyMemBackendFree(pPath->pAlloc,pPath);` |
|    11882 |  507 | `}` |
|        - |  508 | `/* Map an op-handler VmOpRc into the main-loop rc convention used by VmByteCodeExecBody. */` |
|    23540 |  509 | `static sxi32 VmOpRcToExecRc(VmOpRc rcOp)` |
|        5 |  510 | `{` |
|    23545 |  511 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 |  512 | `		return PH7_ABORT;` |
|        - |  513 | `	}` |
|    23545 |  514 | `	if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 |  515 | `		return PH7_EXCEPTION;` |
|        - |  516 | `	}` |
|    23545 |  517 | `	return SXRET_OK;` |
|    11890 |  518 | `}` |
|        - |  519 | `/*` |
|        - |  520 | ` * D1 commit 2: re-drive ONE captured lvalue step by invoking the real LOAD_IDX / MEMBER` |
|        - |  521 | ` * handler on a synthetic 2-slot stack. This reuses the proven COW / vivify / warning / magic` |
|        - |  522 | ` * machinery instead of hand-walking it. pBase carries the current container (its nIdx must be` |
|        - |  523 | ` * a real aMemObj slot for a by-ref write to vivify in place). iP2 selects the mode:` |
|        - |  524 | ` * LOAD_IDX 1=write(vivify,by-ref) / 0=read(by-value); MEMBER PH7_MEMBER_READ=by-value read.` |
|        - |  525 | ` * On success pOut receives the result value and its nIdx (the aliasable element slot for a` |
|        - |  526 | ` * vivified by-ref element).` |
|        - |  527 | ` */` |
|    23540 |  528 | `static sxi32 VmReDriveStep(ph7_vm *pVm,sxi32 iOp,sxu32 iP2,ph7_value *pBase,ph7_value *pKey,ph7_value *pOut)` |
|        5 |  529 | `{` |
|        - |  530 | `	ph7_value mini[2];` |
|        - |  531 | `	VmInstr aI[2];` |
|        - |  532 | `	VmExecState st;` |
|        - |  533 | `	VmOpRc rcOp;` |
|    23545 |  534 | `	PH7_MemObjInit(pVm,&mini[0]);` |
|    23545 |  535 | `	PH7_MemObjInit(pVm,&mini[1]);` |
|    23545 |  536 | `	PH7_MemObjLoad(pBase,&mini[0]);` |
|    23545 |  537 | `	mini[0].nIdx = pBase->nIdx;` |
|    23545 |  538 | `	PH7_MemObjStore(pKey,&mini[1]);` |
|    23545 |  539 | `	SyZero((void *)aI,sizeof(aI));` |
|        - |  540 | `	/* LOAD_IDX: iP1=1 means "an index is present". MEMBER: iP1=0 means an INSTANCE member` |
|        - |  541 | ``	 * (iP1=1 would be a static `::` access). */`` |
|    23545 |  542 | `	aI[0].iOp = (sxu8)iOp; aI[0].iP1 = (iOp == PH7_OP_LOAD_IDX) ? 1 : 0; aI[0].iP2 = iP2;` |
|    23545 |  543 | `	SyZero((void *)&st,sizeof(st));` |
|    23545 |  544 | `	st.pStack = mini; st.pTos = &mini[1]; st.aInstr = aI; st.pc = 0;` |
|    23545 |  545 | `	if( iOp == PH7_OP_LOAD_IDX ){` |
|    23455 |  546 | `		rcOp = VmExecOpLoadIdx(&(*pVm),&st,&aI[0]);` |
|    11845 |  547 | `	}else{` |
|       92 |  548 | `		rcOp = VmExecOpMember(&(*pVm),&st,&aI[0]);` |
|        - |  549 | `	}` |
|        - |  550 | `	/* The handler popped the key/name and left the result in the (now top) base slot.` |
|        - |  551 | `	 * DEEP-COPY it into pOut (MemObjStore, not MemObjLoad): the result is chained as the next` |
|        - |  552 | `	 * step's base and must OWN its buffer — mini[0] is released immediately below, and a` |
|        - |  553 | `	 * read-only view (MemObjLoad) would leave pOut dangling into freed memory. */` |
|    23545 |  554 | `	PH7_MemObjStore(st.pTos,pOut);` |
|    23545 |  555 | `	pOut->nIdx = st.pTos->nIdx;` |
|    23545 |  556 | `	PH7_MemObjRelease(&mini[0]);` |
|    23545 |  557 | `	return VmOpRcToExecRc(rcOp);` |
|        5 |  558 | `}` |
|        - |  559 | `/*` |
|        - |  560 | ` * D1 commit 2: resolve an object property as a by-ref target. Given the object's aMemObj` |
|        - |  561 | ` * slot, return the property value's slot index in *pnOut so the by-ref binder can alias it.` |
|        - |  562 | ` * A present property binds directly; a missing one is created (recreate a declared+unset` |
|        - |  563 | ` * property, or a dynamic property on a dynamic-allowing class); a magic __get/__set property` |
|        - |  564 | ` * emits php's Notice and does NOT bind (*pbNoBind). Mirrors VmExecOpMember's write-create.` |
|        - |  565 | ` */` |
|        2 |  566 | `static sxi32 VmBindPropByRef(ph7_vm *pVm,sxu32 nObjIdx,const SyString *pName,sxu32 *pnOut,int *pbNoBind)` |
|        1 |  567 | `{` |
|        3 |  568 | `	ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|        - |  569 | `	ph7_class_instance *pThis;` |
|        - |  570 | `	ph7_class *pClass;` |
|        - |  571 | `	SyHashEntry *pEntry;` |
|        3 |  572 | `	VmClassAttr *pAttr = 0;` |
|        3 |  573 | `	*pbNoBind = 0;` |
|        3 |  574 | `	if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  575 | `		/* Base is not an object (e.g. a NULL intermediate): cannot bind a property by ref. */` |
|      ! 0 |  576 | `		*pbNoBind = 1;` |
|      ! 0 |  577 | `		return SXRET_OK;` |
|        - |  578 | `	}` |
|        3 |  579 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|        3 |  580 | `	pClass = pThis->pClass;` |
|        3 |  581 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|        3 |  582 | `	if( pEntry ){` |
|      ! 0 |  583 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      ! 0 |  584 | `		*pnOut = pAttr->nIdx;` |
|      ! 0 |  585 | `		return SXRET_OK;` |
|        - |  586 | `	}` |
|        2 |  587 | `	if( PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|        3 |  588 | `	 \|\| PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) ){` |
|        - |  589 | `		/* Overloaded (magic) property: php passes it by-value with a Notice and drops the` |
|        - |  590 | `		 * write-back — "has no effect". */` |
|      ! 0 |  591 | `		VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  592 | `			"Indirect modification of overloaded property %z::$%z has no effect",` |
|      ! 0 |  593 | `			&pClass->sName,pName);` |
|      ! 0 |  594 | `		*pbNoBind = 1;` |
|      ! 0 |  595 | `		return SXRET_OK;` |
|        - |  596 | `	}` |
|        - |  597 | `	{` |
|        3 |  598 | `		ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);` |
|        3 |  599 | `		if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      ! 0 |  600 | `			VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pAttr);` |
|        3 |  601 | `		}else if( VmClassAllowsDynamicProps(&(*pVm),pClass) ){` |
|        3 |  602 | `			PH7_VmCreateDynamicAttr(&(*pVm),pThis,pName->zString,pName->nByte,&pAttr);` |
|        2 |  603 | `		}else{` |
|        - |  604 | `			SyBlob sMsg;` |
|        - |  605 | `			sxi32 rcT;` |
|      ! 0 |  606 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 |  607 | `			SyBlobFormat(&sMsg,"Cannot create dynamic property %z::$%z",&pClass->sName,pName);` |
|      ! 0 |  608 | `			rcT = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      ! 0 |  609 | `			SyBlobRelease(&sMsg);` |
|      ! 0 |  610 | `			*pbNoBind = 1;` |
|      ! 0 |  611 | `			return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  612 | `		}` |
|        3 |  613 | `		if( pAttr ){` |
|        3 |  614 | `			*pnOut = pAttr->nIdx;` |
|        2 |  615 | `		}else{` |
|      ! 0 |  616 | `			*pbNoBind = 1;` |
|        - |  617 | `		}` |
|        - |  618 | `	}` |
|        3 |  619 | `	return SXRET_OK;` |
|        2 |  620 | `}` |
|        - |  621 | `/* Resolve a captured lvalue path as a BY-REF target: vivify the whole chain in place and` |
|        - |  622 | ` * leave pSlot->nIdx pointing at the terminal (aliasable) slot for the by-ref binder. */` |
|       16 |  623 | `static sxi32 VmResolvePathByRef(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        1 |  624 | `{` |
|        - |  625 | `	sxu32 nCur;` |
|        - |  626 | `	sxu32 i;` |
|        - |  627 | `	sxi32 rc;` |
|       17 |  628 | `	if( pPath->eRoot == 2 ){` |
|        - |  629 | `		/* Subscripting a string: php refuses a by-ref bind to a string offset. */` |
|      ! 0 |  630 | `		sxi32 rcT = VmThrowFromVm(&(*pVm),"Error",` |
|        - |  631 | `			"Cannot create references to/from string offsets",` |
|        - |  632 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|      ! 0 |  633 | `		return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  634 | `	}` |
|       17 |  635 | `	if( pPath->eRoot == 1 ){` |
|        3 |  636 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,TRUE); /* vivify $a */` |
|        3 |  637 | `		if( pRoot == 0 ){` |
|      ! 0 |  638 | `			return SXRET_OK;` |
|        - |  639 | `		}` |
|        3 |  640 | `		nCur = pRoot->nIdx;` |
|        2 |  641 | `	}else{` |
|       15 |  642 | `		nCur = pPath->nRootIdx;` |
|        - |  643 | `	}` |
|       35 |  644 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|       19 |  645 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|       19 |  646 | `		if( pStep->isProp ){` |
|        3 |  647 | `			sxu32 nOut = SXU32_HIGH;` |
|        3 |  648 | `			int bNoBind = 0;` |
|        3 |  649 | `			rc = VmBindPropByRef(&(*pVm),nCur,&pStep->sProp,&nOut,&bNoBind);` |
|        3 |  650 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  651 | `				return rc;` |
|        - |  652 | `			}` |
|        3 |  653 | `			if( bNoBind ){` |
|      ! 0 |  654 | `				return SXRET_OK; /* magic/non-object: leave the slot a clean NULL, pass by value */` |
|        - |  655 | `			}` |
|        3 |  656 | `			nCur = nOut;` |
|        2 |  657 | `		}else{` |
|        - |  658 | `			ph7_value out;` |
|       17 |  659 | `			ph7_value *pContainer = (ph7_value *)SySetAt(&pVm->aMemObj,nCur);` |
|       17 |  660 | `			if( pContainer == 0 ){` |
|      ! 0 |  661 | `				return SXRET_OK;` |
|        - |  662 | `			}` |
|       17 |  663 | `			PH7_MemObjInit(&(*pVm),&out);` |
|       17 |  664 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,1,pContainer,&pStep->sKey,&out);` |
|       17 |  665 | `			nCur = out.nIdx;` |
|       17 |  666 | `			PH7_MemObjRelease(&out);` |
|       17 |  667 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  668 | `				return rc;` |
|        - |  669 | `			}` |
|       17 |  670 | `			if( nCur == SXU32_HIGH ){` |
|      ! 0 |  671 | `				return SXRET_OK; /* no aliasable slot (e.g. a non-lvalue container): pass by value */` |
|        - |  672 | `			}` |
|        - |  673 | `		}` |
|       10 |  674 | `	}` |
|       17 |  675 | `	pSlot->nIdx = nCur;` |
|       17 |  676 | `	return SXRET_OK;` |
|        9 |  677 | `}` |
|        - |  678 | `/* Resolve a captured lvalue path as a BY-VALUE argument: read the chain (emitting php's` |
|        - |  679 | ` * undefined-key/property/offset warnings) WITHOUT vivifying, leaving the terminal value in` |
|        - |  680 | ` * pSlot. Re-drives the read handlers so the warning sequence matches php exactly. */` |
|    23508 |  681 | `static sxi32 VmResolvePathByValue(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        5 |  682 | `{` |
|        - |  683 | `	ph7_value cur;` |
|        - |  684 | `	sxu32 i;` |
|    23513 |  685 | `	sxi32 rc = SXRET_OK;` |
|    23513 |  686 | `	PH7_MemObjInit(&(*pVm),&cur);` |
|    23513 |  687 | `	if( pPath->eRoot == 1 ){` |
|      ! 0 |  688 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,FALSE);` |
|      ! 0 |  689 | `		if( pRoot == 0 ){` |
|      ! 0 |  690 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&pPath->sRootName);` |
|      ! 0 |  691 | `		}else{` |
|      ! 0 |  692 | `			PH7_MemObjLoad(pRoot,&cur);` |
|      ! 0 |  693 | `			cur.nIdx = pRoot->nIdx;` |
|        - |  694 | `		}` |
|      ! 0 |  695 | `	}else{` |
|    23513 |  696 | `		ph7_value *pRoot = (ph7_value *)SySetAt(&pVm->aMemObj,pPath->nRootIdx);` |
|    23513 |  697 | `		if( pRoot ){` |
|    23513 |  698 | `			PH7_MemObjLoad(pRoot,&cur);` |
|    23513 |  699 | `			cur.nIdx = pRoot->nIdx;` |
|    11869 |  700 | `		}` |
|        - |  701 | `	}` |
|    47037 |  702 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|    23529 |  703 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|        - |  704 | `		ph7_value out;` |
|    23529 |  705 | `		PH7_MemObjInit(&(*pVm),&out);` |
|    23529 |  706 | `		if( pStep->isProp ){` |
|        - |  707 | `			ph7_value nameVal;` |
|       92 |  708 | `			PH7_MemObjInitFromString(&(*pVm),&nameVal,&pStep->sProp);` |
|       92 |  709 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_MEMBER,PH7_MEMBER_READ,&cur,&nameVal,&out);` |
|       92 |  710 | `			PH7_MemObjRelease(&nameVal);` |
|       47 |  711 | `		}else{` |
|    23439 |  712 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,0,&cur,&pStep->sKey,&out);` |
|        - |  713 | `		}` |
|    23529 |  714 | `		PH7_MemObjRelease(&cur);` |
|    23529 |  715 | `		cur = out;` |
|    23529 |  716 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  717 | `			PH7_MemObjRelease(&cur);` |
|      ! 0 |  718 | `			return rc;` |
|        - |  719 | `		}` |
|    11882 |  720 | `	}` |
|    23513 |  721 | `	PH7_MemObjStore(&cur,pSlot);` |
|    23513 |  722 | `	pSlot->nIdx = SXU32_HIGH;` |
|    23513 |  723 | `	PH7_MemObjRelease(&cur);` |
|    23513 |  724 | `	return SXRET_OK;` |
|    11874 |  725 | `}` |
|        - |  726 | `/*` |
|        - |  727 | ` * D1: resolve deferred call arguments in [pArg, pTos) before the callee consumes them.` |
|        - |  728 | ` *` |
|        - |  729 | `` * A plain `$var` call argument whose callee signature is unknown at compile time is`` |
|        - |  730 | ` * emitted as a DEFERRED load (OP_LOAD iP1=1,iP2=3): when the variable does not exist it` |
|        - |  731 | ` * yields a NULL slot tagged MEMOBJ_AUX_DEFERRED that carries the variable name in x.pOther` |
|        - |  732 | ` * (a VM-lifetime bytecode string). This helper runs at each OP_CALL dispatch branch, while` |
|        - |  733 | ` * pVm->pFrame is still the CALLER frame, once the callee's by-ref shape is known:` |
|        - |  734 | ` *` |
|        - |  735 | ` *   by-ref position  -> create the variable in the caller frame now and give the slot its` |
|        - |  736 | ` *                       real nIdx so the ordinary by-ref binder aliases it (silent, as php).` |
|        - |  737 | ` *   by-value position-> raise php's "Undefined variable $x" and pass a clean NULL WITHOUT` |
|        - |  738 | ` *                       creating the variable in the caller.` |
|        - |  739 | ` *` |
|        - |  740 | ` * The by-ref decision for positional argument n comes from, in priority order:` |
|        - |  741 | ` *   bAllByValue  -> everything by-value (an unresolvable/erroring callee);` |
|        - |  742 | ` *   bAllByRef    -> everything by-ref (the indirect array-callable/__invoke dispatch paths,` |
|        - |  743 | ` *                   which historically over-vivified every plain-var arg — preserved here` |
|        - |  744 | ` *                   rather than regressed; their by-value refinement is a later slice);` |
|        - |  745 | ` *   pFormal      -> a user function's formal-argument array (honoring a trailing variadic);` |
|        - |  746 | ` *   nByRefMask   -> a builtin by-ref position bitmask (used when pFormal == 0).` |
|        - |  747 | ` *` |
|        - |  748 | ` * A DEFINED variable never carries the marker (it loads with its real nIdx), so this is a` |
|        - |  749 | ` * no-op for it; a call with no deferred args pays only one flag test per slot.` |
|        - |  750 | ` */` |
|  6418240 |  751 | `static sxi32 VmResolveDeferredArgs(` |
|        - |  752 | `	ph7_vm *pVm,` |
|        - |  753 | `	ph7_value *pArg,` |
|        - |  754 | `	ph7_value *pTos,` |
|        - |  755 | `	ph7_vm_func_arg *pFormal,` |
|        - |  756 | `	sxu32 nFormal,` |
|        - |  757 | `	sxu32 nByRefMask,` |
|        - |  758 | `	int bAllByRef,` |
|        - |  759 | `	int bAllByValue)` |
|        5 |  760 | `{` |
|        - |  761 | `	ph7_value *p;` |
|  6418245 |  762 | `	sxu32 n = 0;` |
| 12732635 |  763 | `	for( p = pArg ; p < pTos ; ++p, ++n ){` |
|  6314395 |  764 | `		int bByRef = 0;` |
|        - |  765 | `		SyString sName;` |
|  6314395 |  766 | `		if( (p->iFlags & (MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH)) == 0 ){` |
|  6302510 |  767 | `			continue;` |
|        - |  768 | `		}` |
|    23537 |  769 | `		if( bAllByValue ){` |
|      ! 0 |  770 | `			bByRef = 0;` |
|    23537 |  771 | `		}else if( bAllByRef ){` |
|        - |  772 | `			/* bAllByRef comes ONLY from the array-callable-value and __invoke dispatch paths,` |
|        - |  773 | `			 * which cannot expose the target's per-parameter by-ref flags here. Commit 1 chose` |
|        - |  774 | `			 * to over-vivify every deferred PLAIN-VAR arg on those paths (harmless: it just` |
|        - |  775 | `			 * materializes the caller variable), and that is preserved. But a deferred` |
|        - |  776 | `			 * ELEMENT/PROPERTY (MEMOBJ_AUX_DEFPATH) must NOT be blanket-vivified there: a missing` |
|        - |  777 | `			 * property would fatal ("Cannot create dynamic property") and a missing element would` |
|        - |  778 | `			 * silently vivify + swallow php's "Undefined array key" warning. Resolve those` |
|        - |  779 | `			 * by-value (warn + pass NULL), which matches php for a by-VALUE __invoke/callable —` |
|        - |  780 | `			 * the genuine by-ref-out-param-into-an-element case stays unsupported here, exactly` |
|        - |  781 | `			 * as it was before this slice. */` |
|      ! 0 |  782 | `			bByRef = (p->iFlags & MEMOBJ_AUX_DEFPATH) ? 0 : 1;` |
|    23537 |  783 | `		}else if( pFormal ){` |
|       32 |  784 | `			sxu32 idx = n;` |
|       32 |  785 | `			if( idx >= nFormal ){` |
|        - |  786 | `				/* Beyond the declared formals: a trailing variadic absorbs the tail` |
|        - |  787 | `				 * (and dictates its by-ref-ness); otherwise the extra arg is by-value. */` |
|      ! 0 |  788 | `				idx = (nFormal > 0 && (pFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC))` |
|      ! 0 |  789 | `					? nFormal - 1 : SXU32_HIGH;` |
|      ! 0 |  790 | `			}` |
|       32 |  791 | `			if( idx != SXU32_HIGH ){` |
|       32 |  792 | `				bByRef = (pFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|       15 |  793 | `			}` |
|       17 |  794 | `		}else{` |
|    23507 |  795 | `			bByRef = (n < 31 && (nByRefMask & (1u << n))) ? 1 : 0;` |
|        - |  796 | `		}` |
|    23537 |  797 | `		if( p->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|        - |  798 | `			/* D1 commit 2: a deferred array-element/property lvalue. Detach the descriptor` |
|        - |  799 | `			 * FIRST (so an exception mid-resolve, or a later stack release, cannot double-free` |
|        - |  800 | `			 * it) then re-walk it in the chosen mode. */` |
|    23529 |  801 | `			VmDeferredPath *pPath = (VmDeferredPath *)p->x.pOther;` |
|        - |  802 | `			sxi32 rc;` |
|    23529 |  803 | `			p->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|    23529 |  804 | `			p->x.pOther = 0;` |
|    23529 |  805 | `			MemObjSetType(p,MEMOBJ_NULL);` |
|    23529 |  806 | `			p->nIdx = SXU32_HIGH;` |
|    23529 |  807 | `			if( bByRef ){` |
|       17 |  808 | `				rc = VmResolvePathByRef(&(*pVm),pPath,p);` |
|        9 |  809 | `			}else{` |
|    23513 |  810 | `				rc = VmResolvePathByValue(&(*pVm),pPath,p);` |
|        - |  811 | `			}` |
|    23529 |  812 | `			VmFreeDeferredPath(pPath);` |
|    23529 |  813 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  814 | `				return rc;` |
|        - |  815 | `			}` |
|    23529 |  816 | `			continue;` |
|        - |  817 | `		}` |
|        - |  818 | `		/* Recover the deferred variable name and drop the marker + carrier. */` |
|       10 |  819 | `		SyStringInitFromBuf(&sName,(const char *)p->x.pOther,` |
|        - |  820 | `			p->x.pOther ? SyStrlen((const char *)p->x.pOther) : 0);` |
|       10 |  821 | `		p->iFlags &= ~MEMOBJ_AUX_DEFERRED;` |
|       10 |  822 | `		p->x.pOther = 0;` |
|       10 |  823 | `		if( bByRef ){` |
|        - |  824 | `			/* Materialize in the caller frame; the value stays NULL, only the slot` |
|        - |  825 | `			 * back-reference (nIdx) matters for the by-ref binder / write-back. */` |
|        7 |  826 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|        7 |  827 | `			if( pObj ){` |
|        7 |  828 | `				p->nIdx = pObj->nIdx;` |
|        3 |  829 | `			}` |
|        4 |  830 | `		}else{` |
|        - |  831 | `			/* php warns and passes NULL without creating the variable; the slot is` |
|        - |  832 | `			 * already a clean NULL with nIdx == SXU32_HIGH from the deferred load. */` |
|        3 |  833 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        - |  834 | `		}` |
|        6 |  835 | `	}` |
|  6418245 |  836 | `	return SXRET_OK;` |
|  3209890 |  837 | `}` |
|  9343134 |  838 | `static sxi32 VmByteCodeExecBody(` |
|        - |  839 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  840 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  841 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  842 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  843 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  844 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  845 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  846 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  847 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  848 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|        - |  849 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|        - |  850 | `	ph7_value **ppBaseOwner, /* Base (pCallTop==0) operand-stack owner slot the native entry frees; OP_SPREAD growth of the base stack writes the new pointer here (see VmGrowOperandStack). */` |
|        - |  851 | `	sxu32 *pnBaseCap, /* Base stack capacity (persisted across coroutine suspend/resume); read for the initial capacity and updated on base-stack growth. */` |
|        - |  852 | `	sxu32 nStackOrig /* Base stack's ORIGINAL (ungrown) allocation size — the fixed headroom reference for OP_SPREAD growth (see nStackOrig in VmExecState). */` |
|        - |  853 | `	)` |
|        5 |  854 | `{` |
|        - |  855 | `	VmInstr *pInstr;` |
|        - |  856 | `	ph7_value *pTos;` |
|        - |  857 | `	SySet aArg;` |
|  9343139 |  858 | `	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record` |
|        - |  859 | `	                            * stack (BYTECODE stage 2); NULL = executing the` |
|        - |  860 | `	                            * bottom activation. */` |
|        - |  861 | `	VmExecState sState; /* This activation's boundary state (BYTECODE.md stage 1):` |
|        - |  862 | `	                     * everything a suspended/nested activation must restore.` |
|        - |  863 | `	                     * pc/pTos stay in locals for the hot loop and are synced` |
|        - |  864 | `	                     * into sState only around the call epilogue (stage 2 turns` |
|        - |  865 | `	                     * that boundary into an explicit record push/pop). */` |
|        - |  866 | `	sxi32 pc;` |
|        - |  867 | `	sxi32 rc;` |
|  9343139 |  868 | `	sState.aInstr = aInstr;` |
|  9343139 |  869 | `	sState.pStack = pStack;` |
|  9343139 |  870 | `	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */` |
|  9343139 |  871 | `	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */` |
|  9343139 |  872 | `	sState.pResult = pResult;` |
|  9343139 |  873 | `	sState.pLastRef = pLastRef;` |
|  9343139 |  874 | `	sState.pEnforceRetFunc = pEnforceRetFunc;` |
|  9343139 |  875 | `	sState.is_callback = (sxu8)(is_callback ? 1 : 0);` |
|  9343139 |  876 | `	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);` |
|        - |  877 | `	/* Argument container */` |
|  9343139 |  878 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|  9343139 |  879 | `	if( nTos < 0 ){` |
|  7481675 |  880 | `		pTos = &pStack[-1];` |
|  3740840 |  881 | `	}else{` |
|  1861469 |  882 | `		pTos = &pStack[nTos];` |
|        - |  883 | `	}` |
|  9343139 |  884 | `	sState.pTos = pTos;` |
|  9343139 |  885 | `	sState.pc = nPc;` |
|        - |  886 | `	/* Finally-drain base. For a resumed generator/fiber TOP-LEVEL body, its own` |
|        - |  887 | `	 * exception handlers were just re-published above the caller depth` |
|        - |  888 | `	 * (VmRestoreCtxState), so the live SySetUsed over-counts; take the` |
|        - |  889 | `	 * caller-depth base recorded on the ctx instead.` |
|        - |  890 | `	 *` |
|        - |  891 | `	 * The discriminator is the OPERAND STACK, not the frame: the body exec runs on` |
|        - |  892 | `	 * the ctx's own pStack, whereas every nested frame-less mini-program run within` |
|        - |  893 | `	 * it — a default-argument / constructor trampoline, or a catch/finally body via` |
|        - |  894 | `	 * VmLocalExec — runs on a FRESH operand stack while SHARING pVm->pFrame (those` |
|        - |  895 | `	 * mini-programs do not push a VM frame). A pFrame-only guard therefore misfires` |
|        - |  896 | `	 * for such a mini-program and hands it the generator's low caller-base; its` |
|        - |  897 | `	 * terminal OP_DONE then drains VmDrainFinally down to that base, tearing down a` |
|        - |  898 | ``	 * live try that the surrounding finally had just opened (e.g. `finally { try {`` |
|        - |  899 | ``	 * throw new E(); } catch (E) {} }` in a generator: the `new E()` trampoline's`` |
|        - |  900 | `	 * OP_DONE popped the inner try before its OP_THROW ran, so the throw escaped` |
|        - |  901 | `	 * uncaught). Requiring pStack == pCtx->pStack pins the override to the resumed` |
|        - |  902 | `	 * body itself; nested mini-programs fall through to the correct live depth. */` |
|  9343134 |  903 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame` |
|     2883 |  904 | `	 && pStack == pVm->pActiveCtx->pStack ){` |
|     1845 |  905 | `		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;` |
|     1845 |  906 | `		sState.nFinallyActBase = pVm->pActiveCtx->nFinallyBase;` |
|      925 |  907 | `	}else{` |
|  9341299 |  908 | `		sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|  9341299 |  909 | `		sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|        - |  910 | `	}` |
|  9343139 |  911 | `	sState.pEntryFrame = pVm->pFrame;` |
|  9343139 |  912 | `	pc = nPc;` |
|        - |  913 | `	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call` |
|        - |  914 | `	 * adopts its parked record segment here. VmResumeCtx hands the segment in` |
|        - |  915 | `	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a` |
|        - |  916 | `	 * nested mini-program/callback run inside the resumed body — after it rebased` |
|        - |  917 | `	 * the segment's exception floors and pushed the resume value into the innermost` |
|        - |  918 | `	 * stack. Locals switch to the innermost activation so the dispatch loop` |
|        - |  919 | `	 * continues inside the callee; the record chain is restored so its completion` |
|        - |  920 | `	 * unwinds back through the body. */` |
|  9343139 |  921 | `	if( pAdoptSegment ){` |
|      149 |  922 | `		VmParkedSegment *pSeg = pAdoptSegment;` |
|      149 |  923 | `		pCallTop = pSeg->pCallTop;` |
|      149 |  924 | `		sState = pSeg->sState;` |
|      149 |  925 | `		aInstr = sState.aInstr;` |
|      149 |  926 | `		pStack = sState.pStack;` |
|        - |  927 | `		/* pc is already nPc (== pCtx->pc, the innermost's post-suspend pc) from the` |
|        - |  928 | `		 * init above; only the stack/top move to the innermost activation. */` |
|      149 |  929 | `		pTos = &pStack[nTos];   /* nTos == pCtx->nTos — innermost, resume value pushed */` |
|      149 |  930 | `		SyMemBackendFree(&pVm->sAllocator,pSeg); /* holder only; its contents are now live */` |
|       72 |  931 | `	}` |
|        - |  932 | `/*` |
|        - |  933 | ` * Route an enforcement helper's (or yield-from delegate's) return code from inside` |
|        - |  934 | ` * the main switch: proceed on SXRET_OK, abort on PH7_ABORT, and on PH7_EXCEPTION` |
|        - |  935 | ` * resume at the landing pad of the body that actually caught the exception in place` |
|        - |  936 | ` * (VmRecordedResume) or, when it was caught by an outer exec, unwind out of the VM` |
|        - |  937 | ` * loop. Replaces the old "jump to pVm->pFrame's nearest iExceptionJump" which ran` |
|        - |  938 | ` * the statement after the try even when the catch was at an enclosing frame (ROOT B,` |
|        - |  939 | `` * face b — `yield from` over a throwing sub-generator).`` |
|        - |  940 | ` */` |
|        - |  941 | `/* Dispatch-routing macros: bodies live in vm_dispatch.h, written against the` |
|        - |  942 | ` * VM_EXIT_* primitives. Here they bind to the loop's own control flow. */` |
|        - |  943 | `#define VM_EXIT_BREAK break` |
|        - |  944 | `#define VM_EXIT_ABORT goto Abort` |
|        - |  945 | `#define VM_EXIT_EXCEPTION goto Exception` |
|        - |  946 | `#include "vm_dispatch.h"` |
|        - |  947 | `	/* Generator::throw() inject-at-yield: when this invocation is a resumed generator/fiber` |
|        - |  948 | `	 * body carrying a pending injected exception, raise it HERE — once, before the dispatch` |
|        - |  949 | `	 * loop (pc is already at the resume point) — so the existing OP_THROW route` |
|        - |  950 | `	 * (VmThrowException + VmRecordedResume) lands it at the generator's own try/catch landing` |
|        - |  951 | `	 * pad WITHOUT reconstructing the suspended exception frame on pVm->pFrame. Because` |
|        - |  952 | `	 * pVm->pFrame stays the body, the return/finally/sRet subsystem is untouched (this is why` |
|        - |  953 | `	 * the reverted frame-reconstruction approach's regression cannot recur). Behaviorally` |
|        - |  954 | ``	 * identical to a `throw` executed at the yield point: if the generator's own try catches`` |
|        - |  955 | `	 * it we resume after the try; otherwise it propagates to the throw() caller (ROOT B lands` |
|        - |  956 | `	 * the caller's handler) and the ctx closes. pInjected is one-shot and only meaningful at` |
|        - |  957 | `	 * the resume pc, so this is checked once at entry — NOT per-instruction — keeping the hot` |
|        - |  958 | `	 * dispatch loop untouched for all normal code. The pFrame gate keeps a nested call / catch` |
|        - |  959 | `	 * mini-program sharing the ctx (a separate VmByteCodeExec entry) from re-firing.` |
|        - |  960 | `	 *` |
|        - |  961 | ``	 * Exception: when this body is suspended mid `yield from` over an inner Generator`` |
|        - |  962 | `	 * (iDelegateState==3), a Generator::throw() on the OUTER generator must be forwarded` |
|        - |  963 | `	 * INTO the delegate (PHP yield-from transparency), not raised here. Leave pInjected` |
|        - |  964 | `	 * set and skip; the resume pc is that OP_YIELD_FROM, which consumes and forwards it. */` |
|  9343134 |  965 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected` |
|     1603 |  966 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       63 |  967 | `	 && pVm->pActiveCtx->iDelegateState != 3 ){` |
|       58 |  968 | `		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;` |
|        - |  969 | `		VmFrame *pThrowFrame;` |
|        - |  970 | `		sxi32 iResumePc;` |
|       58 |  971 | `		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */` |
|        - |  972 | `		/* Raising the injection at the yield-from point abandons any array/Iterator` |
|        - |  973 | `		 * delegation in progress (state 1/2 — state 3 was forwarded, not raised here).` |
|        - |  974 | `		 * Tear the delegate down so that if the generator's own try catches this and` |
|        - |  975 | ``		 * runs on to a LATER `yield from`, that opcode classifies its operand fresh`` |
|        - |  976 | `		 * instead of resuming this now-stale delegate cursor. */` |
|       58 |  977 | `		if( pVm->pActiveCtx->iDelegateState != 0 ){` |
|        3 |  978 | `			PH7_MemObjRelease(&pVm->pActiveCtx->sDelegate);` |
|        3 |  979 | `			pVm->pActiveCtx->pDelegateNode = 0;` |
|        3 |  980 | `			pVm->pActiveCtx->iDelegateState = 0;` |
|        1 |  981 | `		}` |
|       58 |  982 | `		pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       58 |  983 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|       58 |  984 | `		rc = VmThrowException(&(*pVm),pInj);` |
|       58 |  985 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  986 | `			goto Abort;` |
|        - |  987 | `		}` |
|       58 |  988 | `		if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - |  989 | `			/* ROOT C: the inject was caught by an inline try in THIS generator. Drain the` |
|        - |  990 | `			 * abandoned mid-expression operands and land at the catch/finally body. This is` |
|        - |  991 | `			 * the pre-loop path (the first fetch uses pc directly), so no -1 adjustment. */` |
|       92 |  992 | `			while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|       48 |  993 | `				PH7_MemObjRelease(pTos);` |
|       48 |  994 | `				pTos--;` |
|        4 |  995 | `			}` |
|       48 |  996 | `			pc = (sxi32)pVm->iInlinePc;` |
|       48 |  997 | `			pVm->pInlineInstr = 0;` |
|       35 |  998 | `		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - |  999 | `			/* Caught by THIS generator's own try. (VmRecordedResume returns FALSE unless a` |
|        - | 1000 | `			 * catch recorded a resume target for this exec, so rc need not be pre-checked;` |
|        - | 1001 | `			 * unlike OP_THROW there is no lexical-try fallthrough here — the else just` |
|        - | 1002 | `			 * propagates.) Drain the abandoned mid-expression operand slots back to the` |
|        - | 1003 | `			 * catching try's base, then land at its pad (iResumePc is landing-1 for the` |
|        - | 1004 | `			 * dispatcher's trailing pc++; the loop below fetches at pc with no leading pc++,` |
|        - | 1005 | `			 * so add 1 to land on the pad itself). */` |
|      ! 0 | 1006 | `			while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      ! 0 | 1007 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 | 1008 | `				pTos--;` |
|      ! 0 | 1009 | `			}` |
|      ! 0 | 1010 | `			pc = iResumePc + 1;` |
|      ! 0 | 1011 | `		}else{` |
|        - | 1012 | `			/* Not caught in this generator (no match, or caught by an outer/caller frame` |
|        - | 1013 | `			 * that ROOT B will land at its own OP_CALL site): propagate out so the ctx` |
|        - | 1014 | `			 * closes and the caller sees the exception. */` |
|       13 | 1015 | `			goto Exception;` |
|        - | 1016 | `		}` |
|       22 | 1017 | `	}` |
|        - | 1018 | `	/* Force-close entry (VmCloseCtx): a suspended generator being destroyed runs its` |
|        - | 1019 | ``	 * pending `finally` blocks. Instead of resuming at the yield, redirect straight`` |
|        - | 1020 | ``	 * into the innermost open try's finally as if a `return` had crossed every`` |
|        - | 1021 | `	 * enclosing finally (mirrors OP_SET_FINALLY_RET; OP_END_FINALLY then threads the` |
|        - | 1022 | `	 * PH7_FA_RETURN out through the whole chain and completes the body). Gate on the` |
|        - | 1023 | `	 * BODY bytecode (aInstr == the function program) so nested mini-programs sharing` |
|        - | 1024 | `	 * this ctx/frame never re-fire it; bClosing stays set so OP_YIELD can reject a` |
|        - | 1025 | `	 * yield reached inside one of these finallys. */` |
|  9343124 | 1026 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing` |
|     1588 | 1027 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       43 | 1028 | `	 && aInstr == (VmInstr *)SySetBasePtr(&pVm->pActiveCtx->pFunc->aByteCode) ){` |
|        - | 1029 | `		VmFinallyAction sAct;` |
|       42 | 1030 | `		sxu32 iFpc = 0;` |
|       42 | 1031 | `		int nCross = -1; /* cross every enclosing finally of this body */` |
|       42 | 1032 | `		SyZero(&sAct,sizeof(sAct));` |
|       42 | 1033 | `		sAct.eKind = PH7_FA_RETURN;` |
|       42 | 1034 | `		sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       42 | 1035 | `		PH7_MemObjInit(pVm,&sAct.sRet); /* discarded return; getReturn() is moot post-close */` |
|       42 | 1036 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|       21 | 1037 | `			sAct.nCross = nCross;` |
|       21 | 1038 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       21 | 1039 | `			pc = (sxi32)iFpc; /* pre-loop: the first fetch uses pc directly (no -1) */` |
|       11 | 1040 | `		}else{` |
|        - | 1041 | `			/* No open try had a finally: nothing to run, complete the body. */` |
|       21 | 1042 | `			PH7_MemObjRelease(&sAct.sRet);` |
|       21 | 1043 | `			goto Done;` |
|        - | 1044 | `		}` |
|       10 | 1045 | `	}` |
|        - | 1046 | `	/* Execute as much as we can */` |
| 37085870 | 1047 | `	for(;;){` |
|      ! 0 | 1048 | `VmLoopFetch:` |
|        - | 1049 | `		/* C-boundary throw routing (band A #1): a PHP callee invoked from a C` |
|        - | 1050 | `		 * site with no status channel — a __toString/__toInt cast, __get/__set/` |
|        - | 1051 | `		 * offsetGet/offsetSet, __clone, __destruct, a user callback inside a` |
|        - | 1052 | `		 * builtin — raised, and the site continued with a fallback value; the` |
|        - | 1053 | `		 * invocation boundary parked the status here (VmBoundaryPark). Route it` |
|        - | 1054 | `		 * exactly as the throw site's dispatch macro would have: abort, land at` |
|        - | 1055 | `		 * an inline-try redirect, resume at a recorded in-place catch (draining` |
|        - | 1056 | `		 * the abandoned mid-expression operands to the catching try's base), or` |
|        - | 1057 | `		 * propagate out of this exec. Checked at the fetch point, so a swallowed` |
|        - | 1058 | `		 * throw outlives at most the C remainder of ONE opcode instead of` |
|        - | 1059 | `		 * silently resuming the surrounding PHP code with a bogus value.` |
|        - | 1060 | `		 * The pending write-back sweep shares this one guard so the hot` |
|        - | 1061 | `		 * no-hooks path pays a single predicted branch per fetch. */` |
| 78111330 | 1062 | `		if( pVm->nBoundaryRc != 0 \|\| SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      773 | 1063 | `			if( pVm->nBoundaryRc != 0 ){` |
|      205 | 1064 | `				sxi32 rcBr = pVm->nBoundaryRc;` |
|      205 | 1065 | `				pVm->nBoundaryRc = 0;` |
|      205 | 1066 | `				if( rcBr == PH7_ABORT ){` |
|       68 | 1067 | `					goto Abort;` |
|        - | 1068 | `				}` |
|      139 | 1069 | `				if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 1070 | `					/* Caught by an inline try (generator body) THIS exec owns: drain` |
|        - | 1071 | `					 * and land (pre-fetch path: pc is used directly, no trailing ++). */` |
|        5 | 1072 | `					while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|        3 | 1073 | `						PH7_MemObjRelease(pTos);` |
|        3 | 1074 | `						pTos--;` |
|        1 | 1075 | `					}` |
|        3 | 1076 | `					pc = (sxi32)pVm->iInlinePc;` |
|        3 | 1077 | `					pVm->pInlineInstr = 0;` |
|        2 | 1078 | `				}else{` |
|        - | 1079 | `					sxi32 iBrPc;` |
|      137 | 1080 | `					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){` |
|        - | 1081 | `						/* Caught in place by a try THIS exec owns: drain the abandoned` |
|        - | 1082 | `						 * operands to the catching try's base and land at its pad` |
|        - | 1083 | `						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;` |
|        - | 1084 | `						 * pre-fetch here, so +1 lands on the pad itself). */` |
|      184 | 1085 | `						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|       92 | 1086 | `							PH7_MemObjRelease(pTos);` |
|       92 | 1087 | `							pTos--;` |
|        4 | 1088 | `						}` |
|       96 | 1089 | `						pc = iBrPc + 1;` |
|       50 | 1090 | `					}else{` |
|        - | 1091 | `						/* Caught by an outer exec (or uncaught-with-report pending):` |
|        - | 1092 | `						 * unwind out of this exec; the owner's macros/router land it. */` |
|       44 | 1093 | `						goto Exception;` |
|        - | 1094 | `					}` |
|        - | 1095 | `				}` |
|       47 | 1096 | `			}` |
|        - | 1097 | `			/* Stale write-back sweep: a pending entry whose OWNING activation is` |
|        - | 1098 | `			 * fetching any pc outside its armed window [nJmpPc, nPc] is dead — for` |
|        - | 1099 | `			 * an RMW entry (window = the modify op alone) a routed throw abandoned` |
|        - | 1100 | `			 * the arming statement mid-flight; for a ??= entry either a throw` |
|        - | 1101 | `			 * abandoned the RHS or the short-circuit jump landed past the` |
|        - | 1102 | `			 * OP_NULLC_STORE (the assign is skipped). Drop it — no set dispatch` |
|        - | 1103 | `			 * (php: the throw/skip discards the write). A nested exec (even a` |
|        - | 1104 | `			 * recursive one over the same bytecode) has a different operand-stack` |
|        - | 1105 | `			 * base and leaves enclosing entries alone; entries below a live top` |
|        - | 1106 | `			 * are reached as the drops expose them. */` |
|      674 | 1107 | `			while( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      569 | 1108 | `				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      568 | 1109 | `				if( pTopRmw->pOwnerStack != (void *)pStack \|\| pTopRmw->pInstrs != (void *)aInstr` |
|       89 | 1110 | `				 \|\| ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){` |
|      281 | 1111 | `					break; /* not ours, or legitimately in flight */` |
|        - | 1112 | `				}` |
|        9 | 1113 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 1114 | `			}` |
|      331 | 1115 | `		}` |
|        - | 1116 | `		/* Fetch the instruction to execute */` |
| 78111224 | 1117 | `		pInstr = &aInstr[pc];` |
| 78111224 | 1118 | `		if( pInstr->nLine ){` |
|        - | 1119 | `			/* Publish the source position for diagnostics, debug_backtrace() and` |
|        - | 1120 | `			 * Throwable. Instructions the compiler could not attribute (nLine 0)` |
|        - | 1121 | `			 * leave the last known line standing rather than reporting line 0. */` |
| 77885728 | 1122 | `			pVm->nCurLine = pInstr->nLine;` |
| 38953081 | 1123 | `		}` |
| 78111224 | 1124 | `		rc = SXRET_OK;` |
|        - | 1125 | `/*` |
|        - | 1126 | ` * What follows here is a massive switch statement where each case implements a` |
|        - | 1127 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - | 1128 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - | 1129 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - | 1130 | ` * the switch statement will break with convention and be flush-left.` |
|        - | 1131 | ` */` |
| 78111224 | 1132 | `		switch(pInstr->iOp){` |
|        - | 1133 | `/*` |
|        - | 1134 | ` * DONE: P1 * *` |
|        - | 1135 | ` *` |
|        - | 1136 | ` * Program execution completed: Clean up the mess left behind` |
|        - | 1137 | ` * and return immediately.` |
|        - | 1138 | ` */` |
|  5249152 | 1139 | `case PH7_OP_DONE:` |
| 10498634 | 1140 | `	if( pInstr->iP2 && sState.bReturnPropagates ){` |
|        - | 1141 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - | 1142 | `		 * onto the body frame this catch/finally returns from (skip the transparent` |
|        - | 1143 | `		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION` |
|        - | 1144 | `		 * materializes it into sState.pResult. Drain any finally opened within this body` |
|        - | 1145 | `		 * first (nested try/finally inside the catch), which may overwrite the same` |
|        - | 1146 | `		 * frame's slot (finally-over-catch). */` |
|    20211 | 1147 | `		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|    20211 | 1148 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    20209 | 1149 | `			PH7_MemObjStore(pTos,&pTgt->sRet);` |
|    20209 | 1150 | `			VmPopOperand(&pTos,1);` |
|    10107 | 1151 | `		}else{` |
|        3 | 1152 | ``			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */`` |
|        - | 1153 | `		}` |
|    20211 | 1154 | `		pTgt->bHasRet = 1;` |
|    20211 | 1155 | `		pTgt->nRetGen++;` |
|    20211 | 1156 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    20211 | 1157 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1158 | `			goto Abort;` |
|        - | 1159 | `		}` |
|    20211 | 1160 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 1161 | `			/* A drained finally threw past itself — it discards this return. */` |
|      ! 0 | 1162 | `			goto Exception;` |
|        - | 1163 | `		}` |
|    20211 | 1164 | `		goto Done;` |
|        - | 1165 | `	}` |
|        - | 1166 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - | 1167 | `	 * the fiber start/resume paths) set sState.pEnforceRetFunc, so this branch is` |
|        - | 1168 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - | 1169 | `	 * callback trampolines, and the main script. */` |
| 10478423 | 1170 | `	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)` |
|     9289 | 1171 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - | 1172 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - | 1173 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - | 1174 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - | 1175 | `		 * value the function never actually returned, so enforcing here would` |
|        - | 1176 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - | 1177 | `		 * exception. */` |
|     9289 | 1178 | `		ph7_value *pRetVal = 0;` |
|     9289 | 1179 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|     9173 | 1180 | `			pRetVal = pTos;` |
|     4584 | 1181 | `		}` |
|     9289 | 1182 | `		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);` |
|     9289 | 1183 | `		if( rc == PH7_ABORT ) goto Abort;` |
|     9283 | 1184 | `		if( rc == PH7_EXCEPTION ){` |
|       32 | 1185 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|       30 | 1186 | `				PH7_MemObjRelease(pTos);` |
|       30 | 1187 | `				pTos--;` |
|       13 | 1188 | `			}` |
|       32 | 1189 | `			goto Exception;` |
|        - | 1190 | `		}` |
|        - | 1191 | `		/* Don't enforce twice if the function loops through multiple` |
|        - | 1192 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - | 1193 | `		 * defensively we clear the pointer after a successful check). */` |
|     9255 | 1194 | `		sState.pEnforceRetFunc = 0;` |
|     4625 | 1195 | `	}` |
| 10478394 | 1196 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|  7498142 | 1197 | `		if( sState.pLastRef ){` |
|   104210 | 1198 | `			*sState.pLastRef = pTos->nIdx;` |
|    52265 | 1199 | `		}` |
|  7498142 | 1200 | `		if( sState.pResult ){` |
|        - | 1201 | `			/* Execution result */` |
|  6046280 | 1202 | `			PH7_MemObjStore(pTos,sState.pResult);` |
|  3023300 | 1203 | `		}` |
|  7498142 | 1204 | `		VmPopOperand(&pTos,1);` |
|  6729488 | 1205 | `	}else if( sState.pLastRef ){` |
|        - | 1206 | `		/* Nothing referenced — also the throw-unwind path: the compiler routes` |
|        - | 1207 | `		 * an uncaught exception to this terminal OP_DONE with iP1 set but an` |
|        - | 1208 | `		 * empty operand stack (pTos == pStack-1), so there is no return value to` |
|        - | 1209 | `		 * store. Guarding on pTos >= pStack (matching the two sibling branches` |
|        - | 1210 | `		 * above) avoids the below-base read that crashed under glibc/ASan. */` |
|  1454079 | 1211 | `		*sState.pLastRef = SXU32_HIGH;` |
|   727037 | 1212 | `	}` |
|        - | 1213 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - | 1214 | `	 * this execution. When 'return' is used inside a try block,` |
|        - | 1215 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - | 1216 | `	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering` |
|        - | 1217 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - | 1218 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - | 1219 | `	 * block can override it (the finally writes this body frame's sRet slot,` |
|        - | 1220 | `	 * materialized below).` |
|        - | 1221 | `	 */` |
| 10478394 | 1222 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
| 10478394 | 1223 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1224 | `		goto Abort;` |
|        - | 1225 | `	}` |
| 10478394 | 1226 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 1227 | `		/* A drained finally threw past itself, discarding the value this OP_DONE` |
|        - | 1228 | `		 * stored into sState.pResult. If an enclosing try IN THIS function caught the new` |
|        - | 1229 | `		 * exception in place, resume at its landing pad; otherwise unwind (the` |
|        - | 1230 | `		 * caller's exception-resume pops the stored result). */` |
|        - | 1231 | `		sxi32 iResumePc;` |
|        5 | 1232 | `		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 1233 | `			pc = iResumePc;` |
|        3 | 1234 | `			break;` |
|        - | 1235 | `		}` |
|        3 | 1236 | `		goto Exception;` |
|        - | 1237 | `	}` |
| 10478390 | 1238 | `	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){` |
|        - | 1239 | `		/* A catch/finally issued a 'return' targeting THIS body. If the body is` |
|        - | 1240 | `		 * actually unwinding because an exception escaped it (terminal OP_DONE on` |
|        - | 1241 | `		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type` |
|        - | 1242 | `		 * enforcement above), that exception supersedes the return: discard it.` |
|        - | 1243 | `		 * Otherwise materialize it as this function's result. */` |
|        9 | 1244 | `		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){` |
|      ! 0 | 1245 | `			VmClearFrameReturn(sState.pEntryFrame);` |
|      ! 0 | 1246 | `		}else{` |
|        9 | 1247 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|        - | 1248 | `		}` |
|        3 | 1249 | `	}` |
| 10478390 | 1250 | `	goto Done;` |
|        - | 1251 | `/*` |
|        - | 1252 | ` * HALT: P1 * *` |
|        - | 1253 | ` *` |
|        - | 1254 | ` * Program execution aborted: Clean up the mess left behind` |
|        - | 1255 | ` * and abort immediately.` |
|        - | 1256 | ` */` |
|       12 | 1257 | `case PH7_OP_HALT:` |
|       28 | 1258 | `	if( pInstr->iP1 ){` |
|        - | 1259 | `#ifdef UNTRUST` |
|        - | 1260 | `		if( pTos < pStack ){` |
|        - | 1261 | `			goto Abort;` |
|        - | 1262 | `		}` |
|        - | 1263 | `#endif` |
|       28 | 1264 | `		if( sState.pLastRef ){` |
|        6 | 1265 | `			*sState.pLastRef = pTos->nIdx;` |
|        2 | 1266 | `		}` |
|       28 | 1267 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       16 | 1268 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - | 1269 | `				/* Output the exit message */` |
|       22 | 1270 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        6 | 1271 | `					pVm->sVmConsumer.pUserData);` |
|       16 | 1272 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|       10 | 1273 | `			}` |
|       22 | 1274 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - | 1275 | `			/* Record exit status */` |
|       16 | 1276 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        6 | 1277 | `		}` |
|       28 | 1278 | `		VmPopOperand(&pTos,1);` |
|       12 | 1279 | `	}else if( sState.pLastRef ){` |
|        - | 1280 | `		/* Nothing referenced */` |
|      ! 0 | 1281 | `		*sState.pLastRef = SXU32_HIGH;` |
|      ! 0 | 1282 | `	}` |
|        - | 1283 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - | 1284 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - | 1285 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - | 1286 | `	 */` |
|       28 | 1287 | `	pVm->bHaltRequested = 1;` |
|       28 | 1288 | `	goto Abort;` |
|        - | 1289 | `/*` |
|        - | 1290 | ` * JMP: * P2 *` |
|        - | 1291 | ` *` |
|        - | 1292 | ` * Unconditional jump: The next instruction executed will be` |
|        - | 1293 | ` * the one at index P2 from the beginning of the program.` |
|        - | 1294 | ` */` |
|   379526 | 1295 | `case PH7_OP_JMP:` |
|   759390 | 1296 | `	pc = pInstr->iP2 - 1;` |
|   759390 | 1297 | `	break;` |
|        - | 1298 | `/*` |
|        - | 1299 | ` * JZ: P1 P2 *` |
|        - | 1300 | ` *` |
|        - | 1301 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - | 1302 | ` * entry in the stack if P1 is zero.` |
|        - | 1303 | ` */` |
|  2415407 | 1304 | `case PH7_OP_JZ:` |
|        - | 1305 | `#ifdef UNTRUST` |
|        - | 1306 | `	if( pTos < pStack ){` |
|        - | 1307 | `		goto Abort;` |
|        - | 1308 | `	}` |
|        - | 1309 | `#endif` |
|        - | 1310 | `	/* Get a boolean value */` |
|  4832698 | 1311 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     1040 | 1312 | `		PH7_MemObjToBool(pTos);` |
|      518 | 1313 | `	}` |
|  4832698 | 1314 | `	if( !pTos->x.iVal ){` |
|        - | 1315 | `		/* Take the jump */` |
|  2466889 | 1316 | `		pc = pInstr->iP2 - 1;` |
|  1233890 | 1317 | `	}` |
|  4832698 | 1318 | `	if( !pInstr->iP1 ){` |
|  4572036 | 1319 | `		VmPopOperand(&pTos,1);` |
|  2286955 | 1320 | `	}` |
|  4832698 | 1321 | `	break;` |
|        - | 1322 | `/*` |
|        - | 1323 | ` * JNZ: P1 P2 *` |
|        - | 1324 | ` *` |
|        - | 1325 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - | 1326 | ` * entry in the stack if P1 is zero.` |
|        - | 1327 | ` */` |
|    99860 | 1328 | `case PH7_OP_JNZ:` |
|        - | 1329 | `#ifdef UNTRUST` |
|        - | 1330 | `	if( pTos < pStack ){` |
|        - | 1331 | `		goto Abort;` |
|        - | 1332 | `	}` |
|        - | 1333 | `#endif` |
|        - | 1334 | `	/* Get a boolean value */` |
|   200058 | 1335 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 1336 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 1337 | `	}` |
|   200058 | 1338 | `	if( pTos->x.iVal ){` |
|        - | 1339 | `		/* Take the jump */` |
|     9943 | 1340 | `		pc = pInstr->iP2 - 1;` |
|     4969 | 1341 | `	}` |
|   200058 | 1342 | `	if( !pInstr->iP1 ){` |
|        3 | 1343 | `		VmPopOperand(&pTos,1);` |
|        1 | 1344 | `	}` |
|   200058 | 1345 | `	break;` |
|        - | 1346 | `/*` |
|        - | 1347 | ` * NOOP: * * *` |
|        - | 1348 | ` *` |
|        - | 1349 | ` * Do nothing. This instruction is often useful as a jump` |
|        - | 1350 | ` * destination.` |
|        - | 1351 | ` */` |
|      ! 0 | 1352 | `case PH7_OP_NOOP:` |
|      ! 0 | 1353 | `	break;` |
|        - | 1354 | `/*` |
|        - | 1355 | ` * POP: P1 * *` |
|        - | 1356 | ` *` |
|        - | 1357 | ` * Pop P1 elements from the operand stack.` |
|        - | 1358 | ` */` |
|  2289499 | 1359 | `case PH7_OP_POP: {` |
|  4580644 | 1360 | `	sxi32 n = pInstr->iP1;` |
|  4580644 | 1361 | `	if( &pTos[-n+1] < pStack ){` |
|        - | 1362 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       79 | 1363 | `		n = (sxi32)(pTos - pStack);` |
|       38 | 1364 | `	}` |
|  4580644 | 1365 | `	VmPopOperand(&pTos,n);` |
|  4580644 | 1366 | `	break;` |
|        - | 1367 | `				 }` |
|        - | 1368 | `/*` |
|        - | 1369 | ` * DUP: * * *` |
|        - | 1370 | ` *` |
|        - | 1371 | ` * Duplicate the top of the stack.` |
|        - | 1372 | ` */` |
|       67 | 1373 | `case PH7_OP_DUP:` |
|        - | 1374 | `#ifdef UNTRUST` |
|        - | 1375 | `	if( pTos < pStack ){` |
|        - | 1376 | `		goto Abort;` |
|        - | 1377 | `	}` |
|        - | 1378 | `#endif` |
|      138 | 1379 | `	pTos++;` |
|      138 | 1380 | `	PH7_MemObjInit(pVm,pTos);` |
|      138 | 1381 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|      138 | 1382 | `	break;` |
|        - | 1383 | `/*` |
|        - | 1384 | ` * NSSWITCH: * * P3` |
|        - | 1385 | ` *` |
|        - | 1386 | ` * Switch the active namespace at runtime.` |
|        - | 1387 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - | 1388 | ` */` |
|    52091 | 1389 | `case PH7_OP_NSSWITCH:` |
|   104187 | 1390 | `	SyBlobReset(&pVm->sNamespace);` |
|   104187 | 1391 | `	if( pInstr->p3 ){` |
|     4277 | 1392 | `		const char *zNs = (const char *)pInstr->p3;` |
|     4277 | 1393 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|     2136 | 1394 | `	}` |
|        - | 1395 | `	/* Clear namespace-scoped use-const imports */` |
|   104187 | 1396 | `	SyHashRelease(&pVm->hUseConstImports);` |
|   104187 | 1397 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|   104187 | 1398 | `	break;` |
|        - | 1399 | `/* OP_USECONST P1 * P3` |
|        - | 1400 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - | 1401 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - | 1402 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - | 1403 | ` */` |
|        7 | 1404 | `case PH7_OP_USECONST: {` |
|       16 | 1405 | `	char **azPair = (char **)pInstr->p3;` |
|       16 | 1406 | `	if( azPair ){` |
|       16 | 1407 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 | 1408 | `	}` |
|       16 | 1409 | `	break;` |
|        - | 1410 | `				}` |
|        - | 1411 | `/*` |
|        - | 1412 | ` * CLASS_DEFER: * * P3` |
|        - | 1413 | ` *` |
|        - | 1414 | ` * Execute a class declaration whose parent/interface/trait could not be` |
|        - | 1415 | ` * resolved when the enclosing file compiled (its autoloader had not RUN yet).` |
|        - | 1416 | ` * P3 is the VmDeferredClass record captured by the compiler. A dependency` |
|        - | 1417 | `` * that is STILL missing throws php's catchable `... not found` Error; a`` |
|        - | 1418 | ` * failed re-compile aborts (its errors were already reported). See the` |
|        - | 1419 | ` * deferral block comment in compile_class.c.` |
|        - | 1420 | ` */` |
|       15 | 1421 | `case PH7_OP_CLASS_DEFER: {` |
|       32 | 1422 | `	VmDeferredClass *pDefer = (VmDeferredClass *)pInstr->p3;` |
|       32 | 1423 | `	VmDeferredReq *pMissing = 0;` |
|       32 | 1424 | `	sxi32 rcDecl = SXRET_OK;` |
|       32 | 1425 | `	if( pDefer ){` |
|       32 | 1426 | `		rcDecl = VmExecDeferredClass(&(*pVm),pDefer,&pMissing);` |
|       15 | 1427 | `	}` |
|       32 | 1428 | `	if( pMissing ){` |
|        - | 1429 | `		static const char *azDeferKind[] = { "Class", "Interface", "Trait" };` |
|        - | 1430 | `		char zDeclMsg[520];` |
|       16 | 1431 | `		sxu32 nDeclMsg = SyBufferFormat(zDeclMsg,sizeof(zDeclMsg),"%s \"%.*s\" not found",` |
|        5 | 1432 | `			azDeferKind[pMissing->cKind < 3 ? pMissing->cKind : 0],` |
|       10 | 1433 | `			(int)pMissing->sName.nByte,pMissing->sName.zString);` |
|       11 | 1434 | `		rc = VmThrowFromVm(&(*pVm),"Error",zDeclMsg,nDeclMsg);` |
|       11 | 1435 | `		if( rc == SXERR_ABORT ){` |
|        3 | 1436 | `			goto Abort;` |
|        - | 1437 | `		}` |
|        9 | 1438 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 1439 | `	}` |
|       22 | 1440 | `	if( rcDecl == SXERR_ABORT ){` |
|      ! 0 | 1441 | `		goto Abort;` |
|        - | 1442 | `	}` |
|       22 | 1443 | `	break;` |
|        - | 1444 | `				}` |
|        - | 1445 | `/*` |
|        - | 1446 | ` * CVT_INT: * * *` |
|        - | 1447 | ` *` |
|        - | 1448 | ` * Force the top of the stack to be an integer.` |
|        - | 1449 | ` */` |
|     1040 | 1450 | `case PH7_OP_CVT_INT:` |
|        - | 1451 | `#ifdef UNTRUST` |
|        - | 1452 | `	if( pTos < pStack ){` |
|        - | 1453 | `		goto Abort;` |
|        - | 1454 | `	}` |
|        - | 1455 | `#endif` |
|     2085 | 1456 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      925 | 1457 | `		PH7_MemObjToInteger(pTos);` |
|      460 | 1458 | `	}` |
|        - | 1459 | `	/* Invalidate any prior representation */` |
|     2085 | 1460 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|     2085 | 1461 | `	break;` |
|        - | 1462 | `/*` |
|        - | 1463 | ` * CVT_REAL: * * *` |
|        - | 1464 | ` *` |
|        - | 1465 | ` * Force the top of the stack to be a real.` |
|        - | 1466 | ` */` |
|       51 | 1467 | `case PH7_OP_CVT_REAL:` |
|        - | 1468 | `#ifdef UNTRUST` |
|        - | 1469 | `	if( pTos < pStack ){` |
|        - | 1470 | `		goto Abort;` |
|        - | 1471 | `	}` |
|        - | 1472 | `#endif` |
|      105 | 1473 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       53 | 1474 | `		PH7_MemObjToReal(pTos);` |
|       25 | 1475 | `	}` |
|        - | 1476 | `	/* Invalidate any prior representation */` |
|      105 | 1477 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|      105 | 1478 | `	break;` |
|        - | 1479 | `/*` |
|        - | 1480 | ` * CVT_STR: * * *` |
|        - | 1481 | ` *` |
|        - | 1482 | ` * Force the top of the stack to be a string.` |
|        - | 1483 | ` */` |
|     2899 | 1484 | `case PH7_OP_CVT_STR:` |
|        - | 1485 | `#ifdef UNTRUST` |
|        - | 1486 | `	if( pTos < pStack ){` |
|        - | 1487 | `		goto Abort;` |
|        - | 1488 | `	}` |
|        - | 1489 | `#endif` |
|        - | 1490 | `	/* (string) cast + string interpolation "$arr": php's user-visible` |
|        - | 1491 | `	 * array->string warning site (§2). */` |
|     5803 | 1492 | `	PH7_MemObjToStringUV(pTos);` |
|     5803 | 1493 | `	break;` |
|        - | 1494 | `/*` |
|        - | 1495 | ` * CVT_BOOL: * * *` |
|        - | 1496 | ` *` |
|        - | 1497 | ` * Force the top of the stack to be a boolean.` |
|        - | 1498 | ` */` |
|       65 | 1499 | `case PH7_OP_CVT_BOOL:` |
|        - | 1500 | `#ifdef UNTRUST` |
|        - | 1501 | `	if( pTos < pStack ){` |
|        - | 1502 | `		goto Abort;` |
|        - | 1503 | `	}` |
|        - | 1504 | `#endif` |
|      131 | 1505 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       39 | 1506 | `		PH7_MemObjToBool(pTos);` |
|       19 | 1507 | `	}` |
|      131 | 1508 | `	break;` |
|        - | 1509 | `/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it` |
|        - | 1510 | ` * always raises "The (unset) cast is no longer supported" (php 8 removed` |
|        - | 1511 | ` * the cast), so a program containing it never compiles. The switch has no` |
|        - | 1512 | ` * default arm, so abort loudly rather than fall through as a silent no-op` |
|        - | 1513 | ` * if a future emitter ever produces one without the compile error. */` |
|      ! 0 | 1514 | `case PH7_OP_CVT_NULL:` |
|      ! 0 | 1515 | `	goto Abort;` |
|        - | 1516 | `/*` |
|        - | 1517 | ` * CVT_NUMC: * * *` |
|        - | 1518 | ` *` |
|        - | 1519 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - | 1520 | ` */` |
|      ! 0 | 1521 | `case PH7_OP_CVT_NUMC:` |
|        - | 1522 | `#ifdef UNTRUST` |
|        - | 1523 | `	if( pTos < pStack ){` |
|        - | 1524 | `		goto Abort;` |
|        - | 1525 | `	}` |
|        - | 1526 | `#endif` |
|        - | 1527 | `	/* Force a numeric cast */` |
|      ! 0 | 1528 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 | 1529 | `	break;` |
|        - | 1530 | `/*` |
|        - | 1531 | ` * CVT_ARRAY: * * *` |
|        - | 1532 | ` *` |
|        - | 1533 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - | 1534 | ` */` |
|       22 | 1535 | `case PH7_OP_CVT_ARRAY:` |
|        - | 1536 | `#ifdef UNTRUST` |
|        - | 1537 | `	if( pTos < pStack ){` |
|        - | 1538 | `		goto Abort;` |
|        - | 1539 | `	}` |
|        - | 1540 | `#endif` |
|        - | 1541 | `	/* Force a hashmap cast */` |
|       48 | 1542 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       48 | 1543 | `	if( rc != SXRET_OK ){` |
|        - | 1544 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 | 1545 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - | 1546 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 | 1547 | `	}` |
|       48 | 1548 | `	break;` |
|        - | 1549 | `/*` |
|        - | 1550 | ` * CVT_OBJ: * * *` |
|        - | 1551 | ` *` |
|        - | 1552 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - | 1553 | ` */` |
|       20 | 1554 | `case PH7_OP_CVT_OBJ:` |
|        - | 1555 | `#ifdef UNTRUST` |
|        - | 1556 | `	if( pTos < pStack ){` |
|        - | 1557 | `		goto Abort;` |
|        - | 1558 | `	}` |
|        - | 1559 | `#endif` |
|       42 | 1560 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1561 | `		/* Force a 'stdClass()' cast */` |
|       42 | 1562 | `		PH7_MemObjToObject(pTos);` |
|       20 | 1563 | `	}` |
|       42 | 1564 | `	break;` |
|        - | 1565 | `/*` |
|        - | 1566 | ` * ERR_CTRL * * *` |
|        - | 1567 | ` *` |
|        - | 1568 | ` * Error control operator.` |
|        - | 1569 | ` */` |
|     3452 | 1570 | `case PH7_OP_UNSET_VAR: {` |
|        - | 1571 | `	VmOpRc rcOp;` |
|     6909 | 1572 | `	sState.pTos = pTos;` |
|     6909 | 1573 | `	sState.pc = pc;` |
|     6909 | 1574 | `	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);` |
|     6909 | 1575 | `	pTos = sState.pTos;` |
|     6909 | 1576 | `	pc = sState.pc;` |
|     6909 | 1577 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 1578 | `		goto Abort;` |
|     6907 | 1579 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1580 | `		goto Exception;` |
|        - | 1581 | `	}` |
|     6907 | 1582 | `	break;` |
|        - | 1583 | `					  }` |
|    34759 | 1584 | `case PH7_OP_ERR_CTRL:` |
|        - | 1585 | `	/*` |
|        - | 1586 | `	 * Error-control operator '@'. Emitted as a PAIR around the suppressed` |
|        - | 1587 | `	 * expression: iP1=1 opens the window (before the operand is evaluated),` |
|        - | 1588 | `	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),` |
|        - | 1589 | `	 * which went unnoticed only because the engine raised so few diagnostics;` |
|        - | 1590 | `	 * every warning/notice/deprecation the parity work added leaked straight` |
|        - | 1591 | ``	 * through `@`. The window nests, and php 8 does NOT let '@' swallow`` |
|        - | 1592 | `	 * fatals or exceptions — those unwind past the closing instruction, so` |
|        - | 1593 | `	 * the depth is reset on the exception path rather than decremented here.` |
|        - | 1594 | `	 */` |
|    69523 | 1595 | `	if( pInstr->iP1 ){` |
|    34765 | 1596 | `		pVm->nErrSuppress++;` |
|    52143 | 1597 | `	}else if( pVm->nErrSuppress > 0 ){` |
|    34763 | 1598 | `		pVm->nErrSuppress--;` |
|    17379 | 1599 | `	}` |
|    69523 | 1600 | `	break;` |
|        - | 1601 | `/*` |
|        - | 1602 | ` * IS_A * * *` |
|        - | 1603 | ` *` |
|        - | 1604 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - | 1605 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - | 1606 | ` * holding a class name or an object).` |
|        - | 1607 | ` * Push TRUE on success. FALSE otherwise.` |
|        - | 1608 | ` */` |
|      397 | 1609 | `case PH7_OP_IS_A:{` |
|      799 | 1610 | `	ph7_value *pNos = &pTos[-1];` |
|      799 | 1611 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - | 1612 | `#ifdef UNTRUST` |
|        - | 1613 | `	if( pNos < pStack ){` |
|        - | 1614 | `		goto Abort;` |
|        - | 1615 | `	}` |
|        - | 1616 | `#endif` |
|      799 | 1617 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      635 | 1618 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      635 | 1619 | `		ph7_class *pClass = 0;` |
|        - | 1620 | `		/* Extract the target class */` |
|      635 | 1621 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - | 1622 | `			/* Instance already loaded */` |
|      ! 0 | 1623 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      635 | 1624 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      635 | 1625 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      635 | 1626 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - | 1627 | `			/* Handle self/static/parent keywords */` |
|      635 | 1628 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        6 | 1629 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      633 | 1630 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 | 1631 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      630 | 1632 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        6 | 1633 | `				pClass = PH7_VmResolveParentClass(&(*pVm));` |
|        4 | 1634 | `			}else{` |
|      625 | 1635 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - | 1636 | `			}` |
|      315 | 1637 | `		}` |
|      635 | 1638 | `		if( pClass ){` |
|        - | 1639 | `			/* Perform the query */` |
|      635 | 1640 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|      315 | 1641 | `		}` |
|      315 | 1642 | `	}` |
|        - | 1643 | `	/* Push result */` |
|      799 | 1644 | `	VmPopOperand(&pTos,1);` |
|      799 | 1645 | `	PH7_MemObjRelease(pTos);` |
|      799 | 1646 | `	pTos->x.iVal = iRes;` |
|      799 | 1647 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      799 | 1648 | `	break;` |
|        - | 1649 | `				 }` |
|        - | 1650 |  |
|        - | 1651 | `/*` |
|        - | 1652 | ` * LOADC P1 P2 *` |
|        - | 1653 | ` *` |
|        - | 1654 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - | 1655 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - | 1656 | ` */` |
|  9982708 | 1657 | `case PH7_OP_LOADC: {` |
|        - | 1658 | `	ph7_value *pObj;` |
|        - | 1659 | `	/* Reserve a room */` |
| 19968489 | 1660 | `	pTos++;` |
| 19968489 | 1661 | `	if( pInstr->iP1 & PH7_LOADC_NOKEY ){` |
|        - | 1662 | `		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */` |
|   720699 | 1663 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|   720699 | 1664 | `		SyBlobReset(&pTos->sBlob);` |
|   720699 | 1665 | `		pTos->iFlags \|= MEMOBJ_AUX_NOKEY;` |
|   720699 | 1666 | `		pTos->nIdx = SXU32_HIGH;` |
|   720699 | 1667 | `		break;` |
|        - | 1668 | `	}` |
| 19247795 | 1669 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
| 19247795 | 1670 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - | 1671 | `			SyHashEntry *pEntry;` |
|        - | 1672 | `			/* Check use const imports first — imports take precedence */` |
|        - | 1673 | `			{` |
|        - | 1674 | `				SyHashEntry *pConstImport;` |
|   187436 | 1675 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|   124954 | 1676 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|   124959 | 1677 | `				if( pConstImport ){` |
|       11 | 1678 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 | 1679 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 | 1680 | `					if( pEntry ){` |
|       11 | 1681 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 | 1682 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 1683 | `						SyBlobReset(&pTos->sBlob);` |
|       11 | 1684 | `						VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|       11 | 1685 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 | 1686 | `						break;` |
|        - | 1687 | `					}` |
|        - | 1688 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 | 1689 | `				}` |
|        - | 1690 | `			}` |
|        - | 1691 | `			/* Candidate for expansion via user defined callbacks */` |
|   124949 | 1692 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|   124949 | 1693 | `			if( pEntry ){` |
|   124937 | 1694 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - | 1695 | `				/* Set a NULL default value */` |
|   124937 | 1696 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|   124937 | 1697 | `				SyBlobReset(&pTos->sBlob);` |
|        - | 1698 | `				/* Invoke the callback and deal with the expanded value */` |
|   124937 | 1699 | `				VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|        - | 1700 | `				/* Mark as constant */` |
|   124937 | 1701 | `				pTos->nIdx = SXU32_HIGH;` |
|   124937 | 1702 | `				break;` |
|        - | 1703 | `			}` |
|        - | 1704 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - | 1705 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - | 1706 | `			 * use-const imports → current NS → global → string fallback).` |
|        - | 1707 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - | 1708 | `			{` |
|       15 | 1709 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|       15 | 1710 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - | 1711 | `				sxu32 j;` |
|       15 | 1712 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|      119 | 1713 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|      106 | 1714 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       54 | 1715 | `				}` |
|       15 | 1716 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - | 1717 | `					/* Try current_namespace\name */` |
|      ! 0 | 1718 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 | 1719 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 | 1720 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 | 1721 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 | 1722 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 | 1723 | `					if( pEntry ){` |
|      ! 0 | 1724 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 | 1725 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 1726 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 | 1727 | `						VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|      ! 0 | 1728 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 1729 | `						break;` |
|        - | 1730 | `					}` |
|        - | 1731 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 | 1732 | `				}` |
|        - | 1733 | `				{` |
|        - | 1734 | `					/*` |
|        - | 1735 | `					 * php 8 has no bare-word fallback: an unresolved constant is a catchable` |
|        - | 1736 | `					 * Error, not its own name as a string. PH7 answered "X" for an unknown` |
|        - | 1737 | `					 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —` |
|        - | 1738 | `					 * silently became a string and flowed on.` |
|        - | 1739 | `					 *` |
|        - | 1740 | `					 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call` |
|        - | 1741 | ``					 * boundary, so neither a bare `break` nor `goto Exception` is correct`` |
|        - | 1742 | `					 * here (see the macro).` |
|        - | 1743 | `					 */` |
|        - | 1744 | `					SyBlob sMsg;` |
|       15 | 1745 | `					SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       15 | 1746 | `					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);` |
|       15 | 1747 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       15 | 1748 | `					SyBlobReset(&pTos->sBlob);` |
|       15 | 1749 | `					pTos->nIdx = SXU32_HIGH;` |
|       21 | 1750 | `					rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|        6 | 1751 | `						SyBlobLength(&sMsg));` |
|       15 | 1752 | `					SyBlobRelease(&sMsg);` |
|       15 | 1753 | `					if( rc == SXERR_ABORT ){` |
|        3 | 1754 | `						goto Abort;` |
|        - | 1755 | `					}` |
|       18 | 1756 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 1757 | `				}` |
|        - | 1758 | `			}` |
|        - | 1759 | `		}` |
| 19122841 | 1760 | `		PH7_MemObjLoad(pObj,pTos);` |
|  9562957 | 1761 | `	}else{` |
|        - | 1762 | `		/* Set a NULL value */` |
|      ! 0 | 1763 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 1764 | `	}` |
|        - | 1765 | `	/* Mark as constant */` |
| 19122841 | 1766 | `	pTos->nIdx = SXU32_HIGH;` |
| 19122841 | 1767 | `	break;` |
|        - | 1768 | `				  }` |
|        - | 1769 | `/*` |
|        - | 1770 | ` * LOAD: P1 * P3` |
|        - | 1771 | ` *` |
|        - | 1772 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - | 1773 | ` * from the P3 operand.` |
|        - | 1774 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - | 1775 | ` * the variable if non existent and push the NULL constant instead.` |
|        - | 1776 | ` */` |
|  7176498 | 1777 | `case PH7_OP_LOAD:{` |
|        - | 1778 | `	ph7_value *pObj;` |
|        - | 1779 | `	SyString sName;` |
| 14359407 | 1780 | `	if( pInstr->p3 == 0 ){` |
|        - | 1781 | `		/* Take the variable name from the top of the stack */` |
|        - | 1782 | `#ifdef UNTRUST` |
|        - | 1783 | `		if( pTos < pStack ){` |
|        - | 1784 | `			goto Abort;` |
|        - | 1785 | `		}` |
|        - | 1786 | `#endif` |
|        - | 1787 | `		/* Force a string cast — variable-variable name $$arr (user-visible, §2) */` |
|       33 | 1788 | `		PH7_MemObjToStringUV(pTos);` |
|       33 | 1789 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       18 | 1790 | `	}else{` |
| 14359377 | 1791 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 1792 | `		/* Reserve a room for the target object */` |
| 14359377 | 1793 | `		pTos++;` |
|        - | 1794 | `	}` |
| 14359407 | 1795 | `	if( pInstr->iP2 == 2 ){` |
|        - | 1796 | ``		/* Read-modify-write target (`$x++`, `$x .= 'a'`): php reads the variable`` |
|        - | 1797 | `		 * before writing, so it warns when it does not exist and THEN seeds it.` |
|        - | 1798 | `		 * Peek first (no create) purely to raise that warning; the load below` |
|        - | 1799 | ``		 * still creates the slot the operator needs. A plain `=` never gets here`` |
|        - | 1800 | `		 * — it writes without reading, and stays silent, as php does. */` |
|   578828 | 1801 | `		if( VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE) == 0 ){` |
|        7 | 1802 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        3 | 1803 | `		}` |
|   289582 | 1804 | `	}` |
|        - | 1805 | `	/* Extract the requested memory object */` |
| 14359407 | 1806 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
| 14359407 | 1807 | `	if( pObj == 0 ){` |
|      118 | 1808 | `		if( pInstr->iP1 ){` |
|        - | 1809 | `			/* Reading a variable that does not exist. php raises E_WARNING` |
|        - | 1810 | `			 * "Undefined variable $x" and evaluates it as NULL; PHL used to` |
|        - | 1811 | `			 * yield NULL silently, which hid typo'd names. iP2 marks the reads` |
|        - | 1812 | `			 * that must stay quiet (isset/empty — see PH7_CompileVariable);` |
|        - | 1813 | `			 * vivifying contexts never get here because they pass iP1 = 0. */` |
|      118 | 1814 | `			if( pInstr->iP2 == 0 ){` |
|       35 | 1815 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|       16 | 1816 | `			}` |
|        - | 1817 | `			/* Variable not found,load NULL */` |
|      118 | 1818 | `			if( !pInstr->p3 ){` |
|       10 | 1819 | `				PH7_MemObjRelease(pTos);` |
|        6 | 1820 | `			}else{` |
|      110 | 1821 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 1822 | `			}` |
|      118 | 1823 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|      118 | 1824 | `			if( pInstr->iP2 == 3 ){` |
|        - | 1825 | `				/* D1 deferred call argument: the variable does not exist, but we do not` |
|        - | 1826 | `				 * know yet whether the callee wants it by-ref (materialize + bind) or` |
|        - | 1827 | `				 * by-value (warn + pass NULL). Tag the slot and stash the variable name` |
|        - | 1828 | `				 * (a VM-lifetime bytecode string, nothing to free) so OP_CALL's` |
|        - | 1829 | `				 * VmResolveDeferredArgs can decide once the callee is resolved. */` |
|       12 | 1830 | `				pTos->iFlags \|= MEMOBJ_AUX_DEFERRED;` |
|       12 | 1831 | `				pTos->x.pOther = pInstr->p3;` |
|        5 | 1832 | `			}` |
|  7176559 | 1833 | `			break;` |
|      ! 0 | 1834 | `		}else{` |
|        - | 1835 | `			/* Fatal error */` |
|      ! 0 | 1836 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 1837 | `			goto Abort;` |
|        - | 1838 | `		}` |
|        - | 1839 | `	}` |
|        - | 1840 | `	/* Load variable contents */` |
| 14359293 | 1841 | `	PH7_MemObjLoad(pObj,pTos);` |
| 14359293 | 1842 | `	pTos->nIdx = pObj->nIdx;` |
| 14359293 | 1843 | `	break;` |
|        - | 1844 | `				   }` |
|        - | 1845 | `/*` |
|        - | 1846 | ` * LOAD_MAP P1 * *` |
|        - | 1847 | ` *` |
|        - | 1848 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - | 1849 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - | 1850 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - | 1851 | ` */` |
|    39013 | 1852 | `case PH7_OP_LOAD_MAP: {` |
|        - | 1853 | `	VmOpRc rcOp;` |
|    78031 | 1854 | `	sState.pTos = pTos;` |
|    78031 | 1855 | `	sState.pc = pc;` |
|    78031 | 1856 | `	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);` |
|    78031 | 1857 | `	pTos = sState.pTos;` |
|    78031 | 1858 | `	pc = sState.pc;` |
|    78031 | 1859 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1860 | `		goto Abort;` |
|    78031 | 1861 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       15 | 1862 | `		goto Exception;` |
|        - | 1863 | `	}` |
|    78017 | 1864 | `	break;` |
|        - | 1865 | `					  }` |
|        - | 1866 | `/*` |
|        - | 1867 | ` * LOAD_LIST: P1 * *` |
|        - | 1868 | ` *` |
|        - | 1869 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - | 1870 | ` * This is the VM implementation of the list() PHP construct.` |
|        - | 1871 | ` * Caveats:` |
|        - | 1872 | ` *  This implementation support only a single nesting level.` |
|        - | 1873 | ` */` |
|      185 | 1874 | `case PH7_OP_LOAD_LIST: {` |
|        - | 1875 | `	VmOpRc rcOp;` |
|      375 | 1876 | `	sState.pTos = pTos;` |
|      375 | 1877 | `	sState.pc = pc;` |
|      375 | 1878 | `	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);` |
|      375 | 1879 | `	pTos = sState.pTos;` |
|      375 | 1880 | `	pc = sState.pc;` |
|      375 | 1881 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1882 | `		goto Abort;` |
|      375 | 1883 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1884 | `		goto Exception;` |
|        - | 1885 | `	}` |
|      375 | 1886 | `	break;` |
|        - | 1887 | `					  }` |
|        - | 1888 | `/*` |
|        - | 1889 | ` * LOAD_IDX: P1 P2 *` |
|        - | 1890 | ` *` |
|        - | 1891 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - | 1892 | ` * from the stack.` |
|        - | 1893 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - | 1894 | ` * instead.` |
|        - | 1895 | ` */` |
|   363980 | 1896 | `case PH7_OP_LOAD_IDX: {` |
|        - | 1897 | `	VmOpRc rcOp;` |
|   729194 | 1898 | `	sState.pTos = pTos;` |
|   729194 | 1899 | `	sState.pc = pc;` |
|   729194 | 1900 | `	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);` |
|   729194 | 1901 | `	pTos = sState.pTos;` |
|   729194 | 1902 | `	pc = sState.pc;` |
|   729194 | 1903 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1904 | `		goto Abort;` |
|   729194 | 1905 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1906 | `		goto Exception;` |
|        - | 1907 | `	}` |
|   729194 | 1908 | `	break;` |
|        - | 1909 | `					  }` |
|        - | 1910 | `/*` |
|        - | 1911 | ` * LOAD_CLOSURE * * P3` |
|        - | 1912 | ` *` |
|        - | 1913 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - | 1914 | ` * name in the stack.` |
|        - | 1915 | ` */` |
|      765 | 1916 | `case PH7_OP_LOAD_CLOSURE: {` |
|        - | 1917 | `	VmOpRc rcOp;` |
|     1535 | 1918 | `	sState.pTos = pTos;` |
|     1535 | 1919 | `	sState.pc = pc;` |
|     1535 | 1920 | `	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);` |
|     1535 | 1921 | `	pTos = sState.pTos;` |
|     1535 | 1922 | `	pc = sState.pc;` |
|     1535 | 1923 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 1924 | `		goto Abort;` |
|     1535 | 1925 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1926 | `		goto Exception;` |
|        - | 1927 | `	}` |
|     1535 | 1928 | `	break;` |
|        - | 1929 | `					  }` |
|        - | 1930 | `/*` |
|        - | 1931 | ` * LOAD_FCC P1 * *` |
|        - | 1932 | ` *` |
|        - | 1933 | ` * First-class callable: wrap the callee in a Closure object instead of calling it.` |
|        - | 1934 | ` *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS` |
|        - | 1935 | ` *           (from the callee's OP_LOADC). Replace it in place with a Closure whose` |
|        - | 1936 | ` *           $__fn is that name; the existing string-callable dispatch resolves it.` |
|        - | 1937 | ` *           (OOM degrades to leaving the name string on the stack — still callable.)` |
|        - | 1938 | ` *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name` |
|        - | 1939 | ` *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds` |
|        - | 1940 | ` *           $this (scope = its class); a class-name-string target is a static callable` |
|        - | 1941 | ` *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —` |
|        - | 1942 | ` *           the popped target leaves no name string to keep.)` |
|        - | 1943 | ` */` |
|       46 | 1944 | `case PH7_OP_LOAD_FCC:{` |
|       95 | 1945 | `	if( pInstr->iP1 == 1 ){` |
|        - | 1946 | ``		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)`` |
|        - | 1947 | `		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —` |
|        - | 1948 | `		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,` |
|        - | 1949 | ``		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a`` |
|        - | 1950 | `		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is` |
|        - | 1951 | `		 * (graceful degradation), and so is the original on OOM — still whatever it was. */` |
|        - | 1952 | `		ph7_class_instance *pCloObj;` |
|       55 | 1953 | `		if( VmValueIsClosure(pVm, pTos) ){` |
|        3 | 1954 | `			break;` |
|        - | 1955 | `		}` |
|       53 | 1956 | `		pCloObj = VmFccWrapValue(pVm, pTos);` |
|       53 | 1957 | `		if( pCloObj ){` |
|       53 | 1958 | `			PH7_MemObjRelease(pTos);` |
|       53 | 1959 | `			pCloObj->iRef++;` |
|       53 | 1960 | `			pTos->x.pOther = pCloObj;` |
|       53 | 1961 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       25 | 1962 | `		}` |
|       28 | 1963 | `	}else{` |
|        - | 1964 | `		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]` |
|        - | 1965 | `		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its` |
|        - | 1966 | `		 * class); a class-name string target is a static callable (scope = that class). */` |
|       41 | 1967 | `		ph7_value *pTarget = &pTos[-1];` |
|        - | 1968 | `		SyString sName;` |
|        - | 1969 | `		ph7_class_instance *pCloObj;` |
|       41 | 1970 | `		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));` |
|       41 | 1971 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|       21 | 1972 | `			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;` |
|       21 | 1973 | `			pCloObj = VmCreateClosure(pVm, &sName, pBoundThis, &pBoundThis->pClass->sName);` |
|       31 | 1974 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|        - | 1975 | ``			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class`` |
|        - | 1976 | `			 * now, so the closure binds the concrete scope (matching PHP). */` |
|       21 | 1977 | `			ph7_class *pScopeCls = VmFccResolveScope(pVm, pTarget);` |
|       21 | 1978 | `			pCloObj = pScopeCls ? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;` |
|       11 | 1979 | `		}else{` |
|      ! 0 | 1980 | `			pCloObj = 0;` |
|        - | 1981 | `		}` |
|        - | 1982 | `		/* Pop the method name and the target, push the Closure. */` |
|       41 | 1983 | `		PH7_MemObjRelease(pTos);` |
|       41 | 1984 | `		pTos--;` |
|       41 | 1985 | `		PH7_MemObjRelease(pTos);` |
|       41 | 1986 | `		if( pCloObj ){` |
|       41 | 1987 | `			pCloObj->iRef++;` |
|       41 | 1988 | `			pTos->x.pOther = pCloObj;` |
|       41 | 1989 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       21 | 1990 | `		}else{` |
|      ! 0 | 1991 | `			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */` |
|        - | 1992 | `		}` |
|        - | 1993 | `	}` |
|       93 | 1994 | `	break;` |
|        - | 1995 | `					 }` |
|        - | 1996 | `/*` |
|        - | 1997 | ` * STORE * P2 P3` |
|        - | 1998 | ` *` |
|        - | 1999 | ` * Perform a store (Assignment) operation.` |
|        - | 2000 | ` */` |
|  1856420 | 2001 | `case PH7_OP_STORE: {` |
|        - | 2002 | `	ph7_value *pObj;` |
|        - | 2003 | `	SyString sName;` |
|        - | 2004 | `#ifdef UNTRUST` |
|        - | 2005 | `	if( pTos < pStack ){` |
|        - | 2006 | `		goto Abort;` |
|        - | 2007 | `	}` |
|        - | 2008 | `#endif` |
|  3714145 | 2009 | `	if( pInstr->iP2 ){` |
|        - | 2010 | `		sxu32 nIdx;` |
|        - | 2011 | `		sxi32 rcT;` |
|        - | 2012 | `		/* Member store operation */` |
|  3001287 | 2013 | `		nIdx = pTos->nIdx;` |
|  3001287 | 2014 | `		VmPopOperand(&pTos,1);` |
|  3001287 | 2015 | `		if( pVm->pMagicSetThis ){` |
|        - | 2016 | `			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain` |
|        - | 2017 | `			 * store to a missing/inaccessible property on a class declaring __set.` |
|        - | 2018 | `			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on` |
|        - | 2019 | `			 * the stack as the assignment expression's result — php's semantics` |
|        - | 2020 | `			 * (no property is created; a throw rides the boundary rail). */` |
|       11 | 2021 | `			ph7_class_instance *pSetThis = pVm->pMagicSetThis;` |
|        - | 2022 | `			SyString sSetName;` |
|       11 | 2023 | `			pVm->pMagicSetThis = 0;` |
|       11 | 2024 | `			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));` |
|       11 | 2025 | `			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);` |
|       11 | 2026 | `			PH7_ClassInstanceUnref(pSetThis);` |
|       11 | 2027 | `			SyBlobReset(&pVm->sMagicSetName);` |
|       11 | 2028 | `			break;` |
|        - | 2029 | `		}` |
|  3001277 | 2030 | `		if( pVm->pHookSetThis ){` |
|        - | 2031 | `			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a` |
|        - | 2032 | `			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with` |
|        - | 2033 | `			 * the rvalue (which stays on the stack as the assignment expression's` |
|        - | 2034 | ``			 * result); a `set => expr` hook's return value is stored into the`` |
|        - | 2035 | `			 * BACKING slot with the ordinary typed enforcement. A property with` |
|        - | 2036 | `			 * only a get hook is php's catchable "is read-only" Error. */` |
|       31 | 2037 | `			ph7_class_instance *pHThis = pVm->pHookSetThis;` |
|       31 | 2038 | `			ph7_class_attr *pHAttr = pVm->pHookSetAttr;` |
|       31 | 2039 | `			sxu32 nBackIdx = pVm->nHookSetIdx;` |
|        - | 2040 | `			sxi32 rcHs;` |
|       31 | 2041 | `			pVm->pHookSetThis = 0;` |
|       31 | 2042 | `			pVm->pHookSetAttr = 0;` |
|       31 | 2043 | `			pVm->nHookSetIdx = SXU32_HIGH;` |
|       31 | 2044 | `			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);` |
|       31 | 2045 | `			PH7_ClassInstanceUnref(pHThis);` |
|       31 | 2046 | `			if( rcHs == PH7_ABORT ){` |
|      ! 0 | 2047 | `				goto Abort;` |
|        - | 2048 | `			}` |
|       31 | 2049 | `			break;` |
|        - | 2050 | `		}` |
|  3001247 | 2051 | `		if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 2052 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 2053 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|      ! 0 | 2054 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 2055 | `		}else{` |
|        - | 2056 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - | 2057 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|  3001247 | 2058 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);` |
|  3001247 | 2059 | `			if( rcT == PH7_ABORT ){` |
|       13 | 2060 | `				goto Abort;` |
|        - | 2061 | `			}` |
|  3001237 | 2062 | `			if( rcT == PH7_EXCEPTION ){` |
|        - | 2063 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - | 2064 | `				 * control to the nearest catch block if any (draining any` |
|        - | 2065 | `				 * abandoned outer-expression operands to the try's base),` |
|        - | 2066 | `				 * otherwise propagate out of the VM loop. */` |
|   100087 | 2067 | `				VmPopOperand(&pTos,1);` |
|        - | 2068 | `				{` |
|        - | 2069 | `					sxi32 iRp;` |
|   100087 | 2070 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|   400085 | 2071 | `						PH7_RESUME_DRAIN()` |
|   100083 | 2072 | `						pc = iRp;` |
|   100083 | 2073 | `						break;` |
|        - | 2074 | `					}` |
|        - | 2075 | `				}` |
|        5 | 2076 | `				goto Exception;` |
|        - | 2077 | `			}` |
|        - | 2078 | `			/* Point to the desired memory object */` |
|  2901155 | 2079 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2901155 | 2080 | `			if( pObj ){` |
|        - | 2081 | `				/* Perform the store operation */` |
|  2901155 | 2082 | `				PH7_MemObjStore(pTos,pObj);` |
|  1450575 | 2083 | `			}` |
|        - | 2084 | `		}` |
|  2901155 | 2085 | `		break;` |
|   712863 | 2086 | `	}else if( pInstr->p3 == 0 ){` |
|        - | 2087 | `		/* Take the variable name from the next on the stack (user-visible: a` |
|        - | 2088 | `		 * variable-variable NAME $$arr warns on an array, §2) */` |
|       18 | 2089 | `		PH7_MemObjToStringUV(pTos);` |
|       18 | 2090 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       18 | 2091 | `		pTos--;` |
|        - | 2092 | `#ifdef UNTRUST` |
|        - | 2093 | `		if( pTos < pStack  ){` |
|        - | 2094 | `			goto Abort;` |
|        - | 2095 | `		}` |
|        - | 2096 | `#endif` |
|       10 | 2097 | `	}else{` |
|   712847 | 2098 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 2099 | `	}` |
|   712858 | 2100 | `	if( sName.nByte == sizeof("GLOBALS")-1` |
|   358484 | 2101 | `	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){` |
|        6 | 2102 | `		if( pInstr->p3 ){` |
|        - | 2103 | `			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time` |
|        - | 2104 | `			 * fatal there; raised at the store site here with the same` |
|        - | 2105 | `			 * message and the same non-catchable outcome). Element writes` |
|        - | 2106 | `			 * are unaffected. */` |
|        3 | 2107 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 2108 | `				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 2109 | `			pVm->iExitStatus = 255;` |
|        3 | 2110 | `			pVm->bHaltRequested = 1;` |
|        3 | 2111 | `			goto Abort;` |
|        - | 2112 | `		}` |
|        - | 2113 | `		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's` |
|        - | 2114 | `		 * compile-time case: php quietly creates an ordinary symbol-table` |
|        - | 2115 | `		 * entry named GLOBALS, leaving the auto-global view intact. */` |
|        3 | 2116 | `		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);` |
|        3 | 2117 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 2118 | `		break;` |
|        - | 2119 | `	}` |
|        - | 2120 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   712859 | 2121 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   712859 | 2122 | `	if( pObj == 0 ){` |
|      ! 0 | 2123 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 2124 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 2125 | `		goto Abort;` |
|        - | 2126 | `	}` |
|   712859 | 2127 | `	if( !pInstr->p3 ){` |
|       16 | 2128 | `		PH7_MemObjRelease(&pTos[1]);` |
|        7 | 2129 | `	}` |
|        - | 2130 | `	/* Perform the store operation */` |
|   712859 | 2131 | `	PH7_MemObjStore(pTos,pObj);` |
|   712859 | 2132 | `	break;` |
|        - | 2133 | `				   }` |
|        - | 2134 | `/*` |
|        - | 2135 | ` * STORE_IDX:   P1 * P3` |
|        - | 2136 | ` * STORE_IDX_R: P1 * P3` |
|        - | 2137 | ` *` |
|        - | 2138 | ` * Perfrom a store operation an a hashmap entry.` |
|        - | 2139 | ` */` |
|   133825 | 2140 | `case PH7_OP_STORE_IDX:` |
|        - | 2141 | `case PH7_OP_STORE_IDX_REF: {` |
|        - | 2142 | `	VmOpRc rcOp;` |
|   267655 | 2143 | `	sState.pTos = pTos;` |
|   267655 | 2144 | `	sState.pc = pc;` |
|   267655 | 2145 | `	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);` |
|   267655 | 2146 | `	pTos = sState.pTos;` |
|   267655 | 2147 | `	pc = sState.pc;` |
|   267655 | 2148 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 2149 | `		goto Abort;` |
|   267653 | 2150 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       15 | 2151 | `		goto Exception;` |
|        - | 2152 | `	}` |
|   267641 | 2153 | `	break;` |
|        - | 2154 | `					  }` |
|        - | 2155 | `/*` |
|        - | 2156 | ` * INCR: P1 * *` |
|        - | 2157 | ` *` |
|        - | 2158 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - | 2159 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - | 2160 | ` * the stack and increment after that.` |
|        - | 2161 | ` */` |
|   270801 | 2162 | `case PH7_OP_INCR:` |
|        - | 2163 | `#ifdef UNTRUST` |
|        - | 2164 | `	if( pTos < pStack ){` |
|        - | 2165 | `		goto Abort;` |
|        - | 2166 | `	}` |
|        - | 2167 | `#endif` |
|        - | 2168 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|        - | 2169 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 2170 | `	 * — which otherwise skips object/array/resource operands. */` |
|   541954 | 2171 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 2172 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 2173 | `	 * php's TypeError, raised BEFORE any set dispatch (the type guard below` |
|        - | 2174 | `	 * would skip the mutation and the tail write-back would otherwise call` |
|        - | 2175 | `	 * the set hook with the unchanged value). */` |
|   541937 | 2176 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|   271145 | 2177 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 2178 | `		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 2179 | `		if( pTopInc->iKind == VM_HOOK_PEND_RMW && pTopInc->nScratchIdx == pTos->nIdx ){` |
|        - | 2180 | `			SyBlob sErrMsg;` |
|        3 | 2181 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 2182 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        3 | 2183 | `				SyBlobAppend(&sErrMsg,"Cannot increment array",sizeof("Cannot increment array")-1);` |
|        1 | 2184 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2185 | `				SyBlobFormat(&sErrMsg,"Cannot increment %z",` |
|      ! 0 | 2186 | `					&((ph7_class_instance *)pTos->x.pOther)->pClass->sName);` |
|      ! 0 | 2187 | `			}else{` |
|      ! 0 | 2188 | `				SyBlobAppend(&sErrMsg,"Cannot increment resource",sizeof("Cannot increment resource")-1);` |
|        - | 2189 | `			}` |
|        3 | 2190 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 2191 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 2192 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 2193 | `			break;` |
|        - | 2194 | `		}` |
|      ! 0 | 2195 | `	}` |
|   541940 | 2196 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   541940 | 2197 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 2198 | `			ph7_value *pObj;` |
|   541940 | 2199 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   541940 | 2200 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 2201 | `					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric` |
|        - | 2202 | `					 * string (it points at str_increment() instead); PHL rejects it. */` |
|        - | 2203 | `					SyBlob sErrMsg;` |
|        3 | 2204 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 2205 | `					SyBlobAppend(&sErrMsg,` |
|        - | 2206 | `						"Increment on a non-numeric string is not supported, use str_increment() instead",` |
|        - | 2207 | `						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);` |
|        3 | 2208 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 2209 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 2210 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 2211 | `					break;` |
|      ! 0 | 2212 | `				}else{` |
|        - | 2213 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - | 2214 | `					 * original value: pTos may alias pObj's blob via` |
|        - | 2215 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - | 2216 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - | 2217 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - | 2218 | `					 * so its old-value view survives the coercion. */` |
|   541938 | 2219 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        9 | 2220 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        3 | 2221 | `					}` |
|        - | 2222 | `					/* Force a numeric cast on the variable */` |
|   541938 | 2223 | `					PH7_MemObjToNumeric(pObj);` |
|   541938 | 2224 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        5 | 2225 | `						pObj->rVal++;` |
|        - | 2226 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 2227 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 2228 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 2229 | `						 * integer-valued real. */` |
|        5 | 2230 | `						PH7_MemObjTryInteger(pObj);` |
|        3 | 2231 | `					}else{` |
|        - | 2232 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 2233 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 2234 | `						sxi64 r;` |
|   541934 | 2235 | `						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 2236 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        5 | 2237 | `							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;` |
|        5 | 2238 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 2239 | `#else` |
|        - | 2240 | `							pObj->x.iVal = r;` |
|        - | 2241 | `#endif` |
|        3 | 2242 | `						}else{` |
|   541930 | 2243 | `							pObj->x.iVal = r;` |
|        - | 2244 | `						}` |
|        - | 2245 | `					}` |
|   541938 | 2246 | `					if( pInstr->iP1 ){` |
|        - | 2247 | `						/* Pre-increment: result is the new value. */` |
|      159 | 2248 | `						PH7_MemObjStore(pObj,pTos);` |
|       79 | 2249 | `					}` |
|        - | 2250 | `					/* Post-increment: pTos retains the old value (a string` |
|        - | 2251 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - | 2252 | `				}` |
|   271137 | 2253 | `			}` |
|   271142 | 2254 | `		}else{` |
|      ! 0 | 2255 | `			if( pInstr->iP1 ){` |
|      ! 0 | 2256 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 | 2257 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 | 2258 | `				}else{` |
|        - | 2259 | `					/* Force a numeric cast */` |
|      ! 0 | 2260 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 2261 | `					/* Pre-increment */` |
|      ! 0 | 2262 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 2263 | `						pTos->rVal++;` |
|        - | 2264 | `						/* Try to get an integer representation */` |
|      ! 0 | 2265 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 2266 | `					}else{` |
|        - | 2267 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 2268 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 2269 | `						sxi64 r;` |
|      ! 0 | 2270 | `						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 2271 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 2272 | `							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;` |
|      ! 0 | 2273 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 2274 | `#else` |
|        - | 2275 | `							pTos->x.iVal = r;` |
|        - | 2276 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 2277 | `#endif` |
|      ! 0 | 2278 | `						}else{` |
|      ! 0 | 2279 | `							pTos->x.iVal = r;` |
|      ! 0 | 2280 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 2281 | `						}` |
|        - | 2282 | `					}` |
|        - | 2283 | `				}` |
|      ! 0 | 2284 | `			}` |
|        - | 2285 | `		}` |
|   271137 | 2286 | `	}` |
|   541938 | 2287 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|   541938 | 2288 | `	break;` |
|        - | 2289 | `/*` |
|        - | 2290 | ` * DECR: P1 * *` |
|        - | 2291 | ` *` |
|        - | 2292 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - | 2293 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - | 2294 | ` * and decrement after that.` |
|        - | 2295 | ` */` |
|      137 | 2296 | `case PH7_OP_DECR:` |
|        - | 2297 | `#ifdef UNTRUST` |
|        - | 2298 | `	if( pTos < pStack ){` |
|        - | 2299 | `		goto Abort;` |
|        - | 2300 | `	}` |
|        - | 2301 | `#endif` |
|        - | 2302 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|        - | 2303 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 2304 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|        - | 2305 | `	 * property currently holding null). */` |
|      281 | 2306 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 2307 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 2308 | `	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —` |
|        - | 2309 | ``	 * `--` on null is php's no-op and its write-back still dispatches set). */`` |
|      268 | 2310 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|      138 | 2311 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 2312 | `		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 2313 | `		if( pTopDec->iKind == VM_HOOK_PEND_RMW && pTopDec->nScratchIdx == pTos->nIdx ){` |
|        - | 2314 | `			SyBlob sErrMsg;` |
|        3 | 2315 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 2316 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        3 | 2317 | `				SyBlobAppend(&sErrMsg,"Cannot decrement array",sizeof("Cannot decrement array")-1);` |
|        1 | 2318 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2319 | `				SyBlobFormat(&sErrMsg,"Cannot decrement %z",` |
|      ! 0 | 2320 | `					&((ph7_class_instance *)pTos->x.pOther)->pClass->sName);` |
|      ! 0 | 2321 | `			}else{` |
|      ! 0 | 2322 | `				SyBlobAppend(&sErrMsg,"Cannot decrement resource",sizeof("Cannot decrement resource")-1);` |
|        - | 2323 | `			}` |
|        3 | 2324 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 2325 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 2326 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 2327 | `			break;` |
|        - | 2328 | `		}` |
|      ! 0 | 2329 | `	}` |
|        - | 2330 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op) -- but 8.3`` |
|        - | 2331 | `	 * deprecates that no-op, same as the non-numeric-string one below. */` |
|      269 | 2332 | `	if( pTos->iFlags & MEMOBJ_NULL ){` |
|        - | 2333 | `		/* E_WARNING, not E_DEPRECATED -- php reports this one at errno 2. */` |
|      ! 0 | 2334 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 2335 | `			"Decrement on type null has no effect, this will change in the next major version of PHP");` |
|      ! 0 | 2336 | `	}` |
|      269 | 2337 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      269 | 2338 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 2339 | `			ph7_value *pObj;` |
|      269 | 2340 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      269 | 2341 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 2342 | ``					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string`` |
|        - | 2343 | `					 * (php has no string decrement); PHL rejects it. */` |
|        - | 2344 | `					SyBlob sErrMsg;` |
|        3 | 2345 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 2346 | `					SyBlobAppend(&sErrMsg,` |
|        - | 2347 | `						"Decrement on a non-numeric string is not supported",` |
|        - | 2348 | `						sizeof("Decrement on a non-numeric string is not supported")-1);` |
|        3 | 2349 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 2350 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 2351 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 2352 | `					break;` |
|      ! 0 | 2353 | `				}else{` |
|        - | 2354 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - | 2355 | `					 * post-decrement must preserve pTos's original value, which` |
|        - | 2356 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - | 2357 | `					 * Force pTos to own its blob before coercing pObj. */` |
|      267 | 2358 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 | 2359 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 | 2360 | `					}` |
|      267 | 2361 | `					PH7_MemObjToNumeric(pObj);` |
|      267 | 2362 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 | 2363 | `						pObj->rVal--;` |
|        - | 2364 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 2365 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 2366 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 2367 | `						 * integer-valued real. */` |
|        9 | 2368 | `						PH7_MemObjTryInteger(pObj);` |
|        5 | 2369 | `					}else{` |
|        - | 2370 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 2371 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 2372 | `						sxi64 r;` |
|      259 | 2373 | `						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 2374 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        3 | 2375 | `							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;` |
|        3 | 2376 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 2377 | `#else` |
|        - | 2378 | `							pObj->x.iVal = r;` |
|        - | 2379 | `#endif` |
|        2 | 2380 | `						}else{` |
|      257 | 2381 | `							pObj->x.iVal = r;` |
|        - | 2382 | `						}` |
|        - | 2383 | `					}` |
|      267 | 2384 | `					if( pInstr->iP1 ){` |
|        - | 2385 | `						/* Pre-decrement: result is the new value. */` |
|        3 | 2386 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 | 2387 | `					}` |
|        - | 2388 | `					/* Post-decrement: pTos retains the old value. */` |
|        - | 2389 | `				}` |
|      132 | 2390 | `			}` |
|      135 | 2391 | `		}else{` |
|      ! 0 | 2392 | `			if( pInstr->iP1 ){` |
|      ! 0 | 2393 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - | 2394 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 | 2395 | `				}else{` |
|        - | 2396 | `					/* Force a numeric cast */` |
|      ! 0 | 2397 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 2398 | `					/* Pre-decrement */` |
|      ! 0 | 2399 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 2400 | `						pTos->rVal--;` |
|        - | 2401 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 | 2402 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 2403 | `					}else{` |
|        - | 2404 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 2405 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 2406 | `						sxi64 r;` |
|      ! 0 | 2407 | `						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 2408 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 2409 | `							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;` |
|      ! 0 | 2410 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 2411 | `#else` |
|        - | 2412 | `							pTos->x.iVal = r;` |
|        - | 2413 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 2414 | `#endif` |
|      ! 0 | 2415 | `						}else{` |
|      ! 0 | 2416 | `							pTos->x.iVal = r;` |
|      ! 0 | 2417 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 2418 | `						}` |
|        - | 2419 | `					}` |
|        - | 2420 | `				}` |
|      ! 0 | 2421 | `			}` |
|        - | 2422 | `		}` |
|      132 | 2423 | `	}` |
|      267 | 2424 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|      267 | 2425 | `	break;` |
|        - | 2426 | `/*` |
|        - | 2427 | ` * UMINUS: * * *` |
|        - | 2428 | ` *` |
|        - | 2429 | ` * Perform a unary minus operation.` |
|        - | 2430 | ` */` |
|    36831 | 2431 | `case PH7_OP_UMINUS:` |
|        - | 2432 | `#ifdef UNTRUST` |
|        - | 2433 | `	if( pTos < pStack ){` |
|        - | 2434 | `		goto Abort;` |
|        - | 2435 | `	}` |
|        - | 2436 | `#endif` |
|        - | 2437 | `	/* Force a numeric (integer,real or both) cast */` |
|    73667 | 2438 | `	PH7_MemObjToNumeric(pTos);` |
|    73667 | 2439 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      121 | 2440 | `		pTos->rVal = -pTos->rVal;` |
|       59 | 2441 | `	}` |
|    73667 | 2442 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    73573 | 2443 | `		if( pTos->x.iVal == SMALLEST_INT64 ){` |
|        - | 2444 | `			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a` |
|        - | 2445 | `			 * REAL representation is already present it is the negated` |
|        - | 2446 | `			 * authoritative value, so just drop the now-stale cached int. The` |
|        - | 2447 | `			 * integer-only build has no float type, so it wraps (two's` |
|        - | 2448 | `			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */` |
|        - | 2449 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        5 | 2450 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 | 2451 | `				pTos->rVal = -(ph7_real)pTos->x.iVal;` |
|        5 | 2452 | `				MemObjSetType(pTos,MEMOBJ_REAL);` |
|        3 | 2453 | `			}else{` |
|      ! 0 | 2454 | `				pTos->iFlags &= ~MEMOBJ_INT;` |
|        - | 2455 | `			}` |
|        - | 2456 | `#else` |
|        - | 2457 | `			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);` |
|        - | 2458 | `#endif` |
|        3 | 2459 | `		}else{` |
|    73569 | 2460 | `			pTos->x.iVal = -pTos->x.iVal;` |
|        - | 2461 | `		}` |
|    36784 | 2462 | `	}` |
|    73667 | 2463 | `	break;` |
|        - | 2464 | `/*` |
|        - | 2465 | ` * UPLUS: * * *` |
|        - | 2466 | ` *` |
|        - | 2467 | ` * Perform a unary plus operation.` |
|        - | 2468 | ` */` |
|       18 | 2469 | `case PH7_OP_UPLUS:` |
|        - | 2470 | `#ifdef UNTRUST` |
|        - | 2471 | `	if( pTos < pStack ){` |
|        - | 2472 | `		goto Abort;` |
|        - | 2473 | `	}` |
|        - | 2474 | `#endif` |
|        - | 2475 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 | 2476 | `	PH7_MemObjToNumeric(pTos);` |
|       37 | 2477 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 2478 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 | 2479 | `	}` |
|       37 | 2480 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 | 2481 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 | 2482 | `	}` |
|       37 | 2483 | `	break;` |
|        - | 2484 | `/*` |
|        - | 2485 | ` * OP_LNOT: * * *` |
|        - | 2486 | ` *` |
|        - | 2487 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - | 2488 | ` * with its complement.` |
|        - | 2489 | ` */` |
|    27022 | 2490 | `case PH7_OP_LNOT:` |
|        - | 2491 | `#ifdef UNTRUST` |
|        - | 2492 | `	if( pTos < pStack ){` |
|        - | 2493 | `		goto Abort;` |
|        - | 2494 | `	}` |
|        - | 2495 | `#endif` |
|        - | 2496 | `	/* Force a boolean cast */` |
|    54049 | 2497 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      159 | 2498 | `		PH7_MemObjToBool(pTos);` |
|       77 | 2499 | `	}` |
|    54049 | 2500 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    54049 | 2501 | `	break;` |
|        - | 2502 | `/*` |
|        - | 2503 | ` * OP_BITNOT: * * *` |
|        - | 2504 | ` *` |
|        - | 2505 | ` * Interpret the top of the stack as an value.Replace it` |
|        - | 2506 | ` * with its ones-complement.` |
|        - | 2507 | ` */` |
|       17 | 2508 | `case PH7_OP_BITNOT:` |
|        - | 2509 | `#ifdef UNTRUST` |
|        - | 2510 | `	if( pTos < pStack ){` |
|        - | 2511 | `		goto Abort;` |
|        - | 2512 | `	}` |
|        - | 2513 | `#endif` |
|        - | 2514 | `	/* Force an integer cast (php deprecates a lossy float here too) */` |
|       36 | 2515 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       36 | 2516 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|       36 | 2517 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2518 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 2519 | `	}` |
|       36 | 2520 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       36 | 2521 | `	break;` |
|        - | 2522 | `/* OP_MUL * * *` |
|        - | 2523 | ` * OP_MUL_STORE * * *` |
|        - | 2524 | ` *` |
|        - | 2525 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - | 2526 | ` * and push the result back onto the stack.` |
|        - | 2527 | ` */` |
|     1545 | 2528 | `case PH7_OP_MUL:` |
|        - | 2529 | `case PH7_OP_MUL_STORE: {` |
|        - | 2530 | `	VmOpRc rcOp;` |
|     3095 | 2531 | `	sState.pTos = pTos;` |
|     3095 | 2532 | `	sState.pc = pc;` |
|     3095 | 2533 | `	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);` |
|     3095 | 2534 | `	pTos = sState.pTos;` |
|     3095 | 2535 | `	pc = sState.pc;` |
|     3095 | 2536 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2537 | `		goto Abort;` |
|     3095 | 2538 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2539 | `		goto Exception;` |
|        - | 2540 | `	}` |
|     3095 | 2541 | `	break;` |
|        - | 2542 | `					  }` |
|        - | 2543 | `/* OP_POW * * *` |
|        - | 2544 | ` * OP_POW_STORE * * *` |
|        - | 2545 | ` *` |
|        - | 2546 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - | 2547 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - | 2548 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - | 2549 | ` * fits in sxi64; otherwise the result is a double.` |
|        - | 2550 | ` */` |
|       68 | 2551 | `case PH7_OP_POW:` |
|        - | 2552 | `case PH7_OP_POW_STORE: {` |
|        - | 2553 | `	VmOpRc rcOp;` |
|      137 | 2554 | `	sState.pTos = pTos;` |
|      137 | 2555 | `	sState.pc = pc;` |
|      137 | 2556 | `	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);` |
|      137 | 2557 | `	pTos = sState.pTos;` |
|      137 | 2558 | `	pc = sState.pc;` |
|      137 | 2559 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2560 | `		goto Abort;` |
|      137 | 2561 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 2562 | `		goto Exception;` |
|        - | 2563 | `	}` |
|      135 | 2564 | `	break;` |
|        - | 2565 | `					  }` |
|        - | 2566 | `/* OP_ADD * * *` |
|        - | 2567 | ` *` |
|        - | 2568 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 2569 | ` * and push the result back onto the stack.` |
|        - | 2570 | ` */` |
|     8586 | 2571 | `case PH7_OP_ADD:{` |
|    17177 | 2572 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2573 | `#ifdef UNTRUST` |
|        - | 2574 | `	if( pNos < pStack ){` |
|        - | 2575 | `		goto Abort;` |
|        - | 2576 | `	}` |
|        - | 2577 | `#endif` |
|        - | 2578 | `	{` |
|        - | 2579 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|        - | 2580 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|        - | 2581 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|        - | 2582 | `		SyBlob sArMsg;` |
|    17177 | 2583 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    17177 | 2584 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 2585 | `			sxi32 rcAr;` |
|        9 | 2586 | `			VmPopOperand(&pTos,1);` |
|        9 | 2587 | `			PH7_MemObjRelease(pTos);` |
|        9 | 2588 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        9 | 2589 | `			pTos->nIdx = SXU32_HIGH;` |
|       13 | 2590 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|        4 | 2591 | `				SyBlobLength(&sArMsg));` |
|        9 | 2592 | `			SyBlobRelease(&sArMsg);` |
|        9 | 2593 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|        9 | 2594 | `			rc = rcAr;` |
|        9 | 2595 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2596 | `		}` |
|    17169 | 2597 | `		SyBlobRelease(&sArMsg);` |
|        - | 2598 | `	}` |
|        - | 2599 | `	/* Perform the addition */` |
|    17169 | 2600 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|    17169 | 2601 | `	VmPopOperand(&pTos,1);` |
|    17169 | 2602 | `	break;` |
|        - | 2603 | `				}` |
|        - | 2604 | `/*` |
|        - | 2605 | ` * OP_ADD_STORE * * *` |
|        - | 2606 | ` *` |
|        - | 2607 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 2608 | ` * and push the result back onto the stack.` |
|        - | 2609 | ` */` |
|     2980 | 2610 | `case PH7_OP_ADD_STORE:{` |
|     5965 | 2611 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2612 | `	ph7_value *pObj;` |
|        - | 2613 | `	sxu32 nIdx;` |
|        - | 2614 | `#ifdef UNTRUST` |
|        - | 2615 | `	if( pNos < pStack ){` |
|        - | 2616 | `		goto Abort;` |
|        - | 2617 | `	}` |
|        - | 2618 | `#endif` |
|        - | 2619 | `	{` |
|        - | 2620 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 2621 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 2622 | `		SyBlob sArMsg;` |
|     5965 | 2623 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     5965 | 2624 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 2625 | `			sxi32 rcAr;` |
|      ! 0 | 2626 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 2627 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 2628 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 2629 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 2630 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 2631 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 2632 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 2633 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 2634 | `			rc = rcAr;` |
|      ! 0 | 2635 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2636 | `		}` |
|     5965 | 2637 | `		SyBlobRelease(&sArMsg);` |
|        - | 2638 | `	}` |
|        - | 2639 | `	/* Perform the addition */` |
|     5965 | 2640 | `	nIdx = pTos->nIdx;` |
|     5965 | 2641 | `	if( nIdx == pVm->nGlobalIdx ){` |
|        - | 2642 | `		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment` |
|        - | 2643 | `		 * (a compile-time fatal in php; raised here, same message). */` |
|        3 | 2644 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 2645 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 2646 | `		pVm->iExitStatus = 255;` |
|        3 | 2647 | `		pVm->bHaltRequested = 1;` |
|        3 | 2648 | `		goto Abort;` |
|        - | 2649 | `	}` |
|     5963 | 2650 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - | 2651 | `	/* Peform the store operation */` |
|     5963 | 2652 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 2653 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     5963 | 2654 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     5963 | 2655 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     5963 | 2656 | `		PH7_MemObjStore(pTos,pObj);` |
|     2979 | 2657 | `	}` |
|     5963 | 2658 | `	PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|        - | 2659 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     5963 | 2660 | `	PH7_MemObjStore(pTos,pNos);` |
|     5963 | 2661 | `	VmPopOperand(&pTos,1);` |
|     5963 | 2662 | `	break;` |
|        - | 2663 | `				}` |
|        - | 2664 | `/* OP_SUB * * *` |
|        - | 2665 | ` *` |
|        - | 2666 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 2667 | ` * first (what was next on the stack) from the second (the` |
|        - | 2668 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 2669 | ` */` |
|    15120 | 2670 | `case PH7_OP_SUB: {` |
|        - | 2671 | `	VmOpRc rcOp;` |
|    30475 | 2672 | `	sState.pTos = pTos;` |
|    30475 | 2673 | `	sState.pc = pc;` |
|    30475 | 2674 | `	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);` |
|    30475 | 2675 | `	pTos = sState.pTos;` |
|    30475 | 2676 | `	pc = sState.pc;` |
|    30475 | 2677 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2678 | `		goto Abort;` |
|    30475 | 2679 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2680 | `		goto Exception;` |
|        - | 2681 | `	}` |
|    30475 | 2682 | `	break;` |
|        - | 2683 | `					  }` |
|        - | 2684 | `/* OP_SUB_STORE * * *` |
|        - | 2685 | ` *` |
|        - | 2686 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 2687 | ` * first (what was next on the stack) from the second (the` |
|        - | 2688 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 2689 | ` */` |
|        6 | 2690 | `case PH7_OP_SUB_STORE: {` |
|        - | 2691 | `	VmOpRc rcOp;` |
|       14 | 2692 | `	sState.pTos = pTos;` |
|       14 | 2693 | `	sState.pc = pc;` |
|       14 | 2694 | `	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);` |
|       14 | 2695 | `	pTos = sState.pTos;` |
|       14 | 2696 | `	pc = sState.pc;` |
|       14 | 2697 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2698 | `		goto Abort;` |
|       14 | 2699 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 2700 | `		goto Exception;` |
|        - | 2701 | `	}` |
|       12 | 2702 | `	break;` |
|        - | 2703 | `					  }` |
|        - | 2704 |  |
|        - | 2705 | `/*` |
|        - | 2706 | ` * OP_MOD * * *` |
|        - | 2707 | ` *` |
|        - | 2708 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2709 | ` * first (what was next on the stack) from the second (the` |
|        - | 2710 | ` * top of the stack) and push the remainder after division` |
|        - | 2711 | ` * onto the stack.` |
|        - | 2712 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 2713 | ` */` |
|      560 | 2714 | `case PH7_OP_MOD: {` |
|        - | 2715 | `	VmOpRc rcOp;` |
|     1125 | 2716 | `	sState.pTos = pTos;` |
|     1125 | 2717 | `	sState.pc = pc;` |
|     1125 | 2718 | `	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);` |
|     1125 | 2719 | `	pTos = sState.pTos;` |
|     1125 | 2720 | `	pc = sState.pc;` |
|     1125 | 2721 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2722 | `		goto Abort;` |
|     1125 | 2723 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 2724 | `		goto Exception;` |
|        - | 2725 | `	}` |
|     1121 | 2726 | `	break;` |
|        - | 2727 | `					  }` |
|        - | 2728 | `/*` |
|        - | 2729 | ` * OP_MOD_STORE * * *` |
|        - | 2730 | ` *` |
|        - | 2731 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2732 | ` * first (what was next on the stack) from the second (the` |
|        - | 2733 | ` * top of the stack) and push the remainder after division` |
|        - | 2734 | ` * onto the stack.` |
|        - | 2735 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 2736 | ` */` |
|        4 | 2737 | `case PH7_OP_MOD_STORE: {` |
|        - | 2738 | `	VmOpRc rcOp;` |
|        9 | 2739 | `	sState.pTos = pTos;` |
|        9 | 2740 | `	sState.pc = pc;` |
|        9 | 2741 | `	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);` |
|        9 | 2742 | `	pTos = sState.pTos;` |
|        9 | 2743 | `	pc = sState.pc;` |
|        9 | 2744 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2745 | `		goto Abort;` |
|        9 | 2746 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 2747 | `		goto Exception;` |
|        - | 2748 | `	}` |
|        5 | 2749 | `	break;` |
|        - | 2750 | `					  }` |
|        - | 2751 | `/*` |
|        - | 2752 | ` * OP_DIV * * *` |
|        - | 2753 | ` *` |
|        - | 2754 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2755 | ` * first (what was next on the stack) from the second (the` |
|        - | 2756 | ` * top of the stack) and push the result onto the stack.` |
|        - | 2757 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 2758 | ` */` |
|       50 | 2759 | `case PH7_OP_DIV: {` |
|        - | 2760 | `	VmOpRc rcOp;` |
|      104 | 2761 | `	sState.pTos = pTos;` |
|      104 | 2762 | `	sState.pc = pc;` |
|      104 | 2763 | `	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);` |
|      104 | 2764 | `	pTos = sState.pTos;` |
|      104 | 2765 | `	pc = sState.pc;` |
|      104 | 2766 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2767 | `		goto Abort;` |
|      104 | 2768 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 2769 | `		goto Exception;` |
|        - | 2770 | `	}` |
|       98 | 2771 | `	break;` |
|        - | 2772 | `					  }` |
|        - | 2773 | `/*` |
|        - | 2774 | ` * OP_DIV_STORE * * *` |
|        - | 2775 | ` *` |
|        - | 2776 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2777 | ` * first (what was next on the stack) from the second (the` |
|        - | 2778 | ` * top of the stack) and push the result onto the stack.` |
|        - | 2779 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 2780 | ` */` |
|        4 | 2781 | `case PH7_OP_DIV_STORE:{` |
|        9 | 2782 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2783 | `	ph7_value *pObj;` |
|        - | 2784 | `	ph7_real a,b,r;` |
|        - | 2785 | `#ifdef UNTRUST` |
|        - | 2786 | `	if( pNos < pStack ){` |
|        - | 2787 | `		goto Abort;` |
|        - | 2788 | `	}` |
|        - | 2789 | `#endif` |
|        - | 2790 | `	{` |
|        - | 2791 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 2792 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 2793 | `		SyBlob sArMsg;` |
|        9 | 2794 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|        9 | 2795 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"/",&sArMsg) != SXRET_OK ){` |
|        - | 2796 | `			sxi32 rcAr;` |
|      ! 0 | 2797 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 2798 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 2799 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 2800 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 2801 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 2802 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 2803 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 2804 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 2805 | `			rc = rcAr;` |
|      ! 0 | 2806 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2807 | `		}` |
|        9 | 2808 | `		SyBlobRelease(&sArMsg);` |
|        - | 2809 | `	}` |
|        - | 2810 | `	/* Force the operands to be real */` |
|        9 | 2811 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 | 2812 | `		PH7_MemObjToReal(pTos);` |
|        4 | 2813 | `	}` |
|        9 | 2814 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 | 2815 | `		PH7_MemObjToReal(pNos);` |
|        4 | 2816 | `	}` |
|        - | 2817 | `	/* Perform the requested operation */` |
|        9 | 2818 | `	a = pTos->rVal;` |
|        9 | 2819 | `	b = pNos->rVal;` |
|        9 | 2820 | `	if( b == 0 ){` |
|        - | 2821 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|        - | 2822 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|        3 | 2823 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|        3 | 2824 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      ! 0 | 2825 | `	}else{` |
|        7 | 2826 | `		r = a/b;` |
|        - | 2827 | `		/* Push the result */` |
|        7 | 2828 | `		pNos->rVal = r;` |
|        7 | 2829 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - | 2830 | `		/* Try to get an integer representation */` |
|        7 | 2831 | `		PH7_MemObjTryInteger(pNos);` |
|        - | 2832 | `	}` |
|        7 | 2833 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 2834 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        7 | 2835 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        7 | 2836 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        7 | 2837 | `		PH7_MemObjStore(pNos,pObj);` |
|        3 | 2838 | `	}` |
|        7 | 2839 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|        7 | 2840 | `	VmPopOperand(&pTos,1);` |
|        7 | 2841 | `	break;` |
|        - | 2842 | `				}` |
|        - | 2843 | `/* OP_BAND * * *` |
|        - | 2844 | ` *` |
|        - | 2845 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2846 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 2847 | ` * two elements.` |
|        - | 2848 | `*/` |
|        - | 2849 | `/* OP_BOR * * *` |
|        - | 2850 | ` *` |
|        - | 2851 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2852 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 2853 | ` * two elements.` |
|        - | 2854 | ` */` |
|        - | 2855 | `/* OP_BXOR * * *` |
|        - | 2856 | ` *` |
|        - | 2857 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2858 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 2859 | ` * two elements.` |
|        - | 2860 | ` */` |
|      657 | 2861 | `case PH7_OP_BAND:` |
|        - | 2862 | `case PH7_OP_BOR:` |
|        - | 2863 | `case PH7_OP_BXOR:{` |
|     1319 | 2864 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2865 | `	sxi64 a,b,r;` |
|        - | 2866 | `#ifdef UNTRUST` |
|        - | 2867 | `	if( pNos < pStack ){` |
|        - | 2868 | `		goto Abort;` |
|        - | 2869 | `	}` |
|        - | 2870 | `#endif` |
|        - | 2871 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     1319 | 2872 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     1319 | 2873 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     1319 | 2874 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     1319 | 2875 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     1319 | 2876 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2877 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 2878 | `	}` |
|     1319 | 2879 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2880 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 | 2881 | `	}` |
|        - | 2882 | `	/* Perform the requested operation */` |
|     1319 | 2883 | `	a = pNos->x.iVal;` |
|     1319 | 2884 | `	b = pTos->x.iVal;` |
|     1319 | 2885 | `	switch(pInstr->iOp){` |
|      138 | 2886 | `	case PH7_OP_BOR_STORE:` |
|      281 | 2887 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 | 2888 | `	case PH7_OP_BXOR_STORE:` |
|       15 | 2889 | `	case PH7_OP_BXOR: r = a^b; break;` |
|      512 | 2890 | `	case PH7_OP_BAND_STORE:` |
|      512 | 2891 | `	case PH7_OP_BAND:` |
|     1028 | 2892 | `	default:          r = a&b; break;` |
|        - | 2893 | `	}` |
|        - | 2894 | `	/* Push the result */` |
|     1319 | 2895 | `	pNos->x.iVal = r;` |
|     1319 | 2896 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     1319 | 2897 | `	VmPopOperand(&pTos,1);` |
|     1319 | 2898 | `	break;` |
|        - | 2899 | `				 }` |
|        - | 2900 | `/* OP_BAND_STORE * * *` |
|        - | 2901 | ` *` |
|        - | 2902 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2903 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 2904 | ` * two elements.` |
|        - | 2905 | `*/` |
|        - | 2906 | `/* OP_BOR_STORE * * *` |
|        - | 2907 | ` *` |
|        - | 2908 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2909 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 2910 | ` * two elements.` |
|        - | 2911 | ` */` |
|        - | 2912 | `/* OP_BXOR_STORE * * *` |
|        - | 2913 | ` *` |
|        - | 2914 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2915 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 2916 | ` * two elements.` |
|        - | 2917 | ` */` |
|       45 | 2918 | `case PH7_OP_BAND_STORE:` |
|        - | 2919 | `case PH7_OP_BOR_STORE:` |
|        - | 2920 | `case PH7_OP_BXOR_STORE:{` |
|       91 | 2921 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2922 | `	ph7_value *pObj;` |
|        - | 2923 | `	sxi64 a,b,r;` |
|        - | 2924 | `#ifdef UNTRUST` |
|        - | 2925 | `	if( pNos < pStack ){` |
|        - | 2926 | `		goto Abort;` |
|        - | 2927 | `	}` |
|        - | 2928 | `#endif` |
|        - | 2929 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|       91 | 2930 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|       91 | 2931 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|       91 | 2932 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       91 | 2933 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|       91 | 2934 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2935 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 2936 | `	}` |
|       91 | 2937 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2938 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 | 2939 | `	}` |
|        - | 2940 | `	/* Perform the requested operation */` |
|       91 | 2941 | `	a = pTos->x.iVal;` |
|       91 | 2942 | `	b = pNos->x.iVal;` |
|       91 | 2943 | `	switch(pInstr->iOp){` |
|       38 | 2944 | `	case PH7_OP_BOR_STORE:` |
|       77 | 2945 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 | 2946 | `	case PH7_OP_BXOR_STORE:` |
|        9 | 2947 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 | 2948 | `	case PH7_OP_BAND_STORE:` |
|        3 | 2949 | `	case PH7_OP_BAND:` |
|        7 | 2950 | `	default:          r = a&b; break;` |
|        - | 2951 | `	}` |
|        - | 2952 | `	/* Push the result */` |
|       91 | 2953 | `	pNos->x.iVal = r;` |
|       91 | 2954 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       91 | 2955 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 2956 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       91 | 2957 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       91 | 2958 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       91 | 2959 | `		PH7_MemObjStore(pNos,pObj);` |
|       45 | 2960 | `	}` |
|       91 | 2961 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       91 | 2962 | `	VmPopOperand(&pTos,1);` |
|       91 | 2963 | `	break;` |
|        - | 2964 | `				 }` |
|        - | 2965 | `/* OP_SHL * * *` |
|        - | 2966 | ` *` |
|        - | 2967 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2968 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 2969 | ` * left by N bits where N is the top element on the stack.` |
|        - | 2970 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 2971 | ` */` |
|        - | 2972 | `/* OP_SHR * * *` |
|        - | 2973 | ` *` |
|        - | 2974 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2975 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 2976 | ` * right by N bits where N is the top element on the stack.` |
|        - | 2977 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 2978 | ` */` |
|       29 | 2979 | `case PH7_OP_SHL:` |
|        - | 2980 | `case PH7_OP_SHR: {` |
|        - | 2981 | `	VmOpRc rcOp;` |
|       60 | 2982 | `	sState.pTos = pTos;` |
|       60 | 2983 | `	sState.pc = pc;` |
|       60 | 2984 | `	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);` |
|       60 | 2985 | `	pTos = sState.pTos;` |
|       60 | 2986 | `	pc = sState.pc;` |
|       60 | 2987 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2988 | `		goto Abort;` |
|       60 | 2989 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2990 | `		goto Exception;` |
|        - | 2991 | `	}` |
|       60 | 2992 | `	break;` |
|        - | 2993 | `					  }` |
|        - | 2994 | `/*  OP_SHL_STORE * * *` |
|        - | 2995 | ` *` |
|        - | 2996 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 2997 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 2998 | ` * left by N bits where N is the top element on the stack.` |
|        - | 2999 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 3000 | ` */` |
|        - | 3001 | `/* OP_SHR_STORE * * *` |
|        - | 3002 | ` *` |
|        - | 3003 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3004 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 3005 | ` * right by N bits where N is the top element on the stack.` |
|        - | 3006 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 3007 | ` */` |
|        9 | 3008 | `case PH7_OP_SHL_STORE:` |
|        - | 3009 | `case PH7_OP_SHR_STORE: {` |
|        - | 3010 | `	VmOpRc rcOp;` |
|       19 | 3011 | `	sState.pTos = pTos;` |
|       19 | 3012 | `	sState.pc = pc;` |
|       19 | 3013 | `	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);` |
|       19 | 3014 | `	pTos = sState.pTos;` |
|       19 | 3015 | `	pc = sState.pc;` |
|       19 | 3016 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3017 | `		goto Abort;` |
|       19 | 3018 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3019 | `		goto Exception;` |
|        - | 3020 | `	}` |
|       19 | 3021 | `	break;` |
|        - | 3022 | `					  }` |
|        - | 3023 | `/* CAT:  P1 * *` |
|        - | 3024 | ` *` |
|        - | 3025 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - | 3026 | ` * back.` |
|        - | 3027 | ` */` |
|    90772 | 3028 | `case PH7_OP_CAT:{` |
|        - | 3029 | `	ph7_value *pNos,*pCur;` |
|   181549 | 3030 | `	if( pInstr->iP1 < 1 ){` |
|   152451 | 3031 | `		pNos = &pTos[-1];` |
|    76228 | 3032 | `	}else{` |
|    29103 | 3033 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - | 3034 | `	}` |
|        - | 3035 | `#ifdef UNTRUST` |
|        - | 3036 | `	if( pNos < pStack ){` |
|        - | 3037 | `		goto Abort;` |
|        - | 3038 | `	}` |
|        - | 3039 | `#endif` |
|        - | 3040 | `	/* Force a string cast (user-visible: warns on an array operand, §2) */` |
|   181549 | 3041 | `	PH7_MemObjToStringUV(pNos);` |
|   181549 | 3042 | `	pCur = &pNos[1];` |
|   367205 | 3043 | `	while( pCur <= pTos ){` |
|   185661 | 3044 | `		PH7_MemObjToStringUV(pCur);` |
|        - | 3045 | `		/* Perform the concatenation */` |
|   185661 | 3046 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   185473 | 3047 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - | 3048 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 | 3049 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 3050 | `				goto Abort;` |
|        - | 3051 | `			}` |
|    92734 | 3052 | `		}` |
|   185661 | 3053 | `		SyBlobRelease(&pCur->sBlob);` |
|   185661 | 3054 | `		pCur++;` |
|        5 | 3055 | `	}` |
|   181549 | 3056 | `	pTos = pNos;` |
|   181549 | 3057 | `	break;` |
|        - | 3058 | `				}` |
|        - | 3059 | `/*  CAT_STORE: * * *` |
|        - | 3060 | ` *` |
|        - | 3061 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - | 3062 | ` * back.` |
|        - | 3063 | ` */` |
|    15228 | 3064 | `case PH7_OP_CAT_STORE:{` |
|    30461 | 3065 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3066 | `	ph7_value *pObj;` |
|        - | 3067 | `	sxu32 nIdx;` |
|        - | 3068 | `#ifdef UNTRUST` |
|        - | 3069 | `	if( pNos < pStack ){` |
|        - | 3070 | `		goto Abort;` |
|        - | 3071 | `	}` |
|        - | 3072 | `#endif` |
|        - | 3073 | `	/* The right operand must be a string to append it (user-visible, §2) */` |
|    30461 | 3074 | `	PH7_MemObjToStringUV(pNos);` |
|    30461 | 3075 | `	nIdx = pTos->nIdx;` |
|        - | 3076 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - | 3077 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - | 3078 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - | 3079 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - | 3080 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - | 3081 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - | 3082 | `	 * the source we copy from — references share the slot index, so one check` |
|        - | 3083 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - | 3084 | `	 * must run before any mutation (left to the slow path).` |
|        - | 3085 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - | 3086 | `	 * and remains O(n^2) by design. */` |
|    30456 | 3087 | `	if( nIdx != SXU32_HIGH` |
|    30456 | 3088 | `	 && nIdx != pNos->nIdx` |
|    30452 | 3089 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|    30453 | 3090 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|    17838 | 3091 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|        - | 3092 | `		/* e.g. $x = 5; $x .= "a";  ->  "5a" (user-visible: warns if $x is an array) */` |
|    30447 | 3093 | `		PH7_MemObjToStringUV(pObj);` |
|    30447 | 3094 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|    30443 | 3095 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 3096 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - | 3097 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 | 3098 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 3099 | `				goto Abort;` |
|        - | 3100 | `			}` |
|    15219 | 3101 | `		}` |
|        - | 3102 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - | 3103 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - | 3104 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - | 3105 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - | 3106 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - | 3107 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - | 3108 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - | 3109 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - | 3110 | `		 * the same slot is appended to again later in the statement` |
|        - | 3111 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - | 3112 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - | 3113 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|    30447 | 3114 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 | 3115 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 | 3116 | `		}` |
|        - | 3117 | `		/* A hooked lvalue's scratch slot was appended in place — dispatch the` |
|        - | 3118 | `		 * set side now (the consume reads the computed value from the slot). */` |
|    30447 | 3119 | `		PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|    30447 | 3120 | `		pNos->nIdx = SXU32_HIGH;` |
|    30447 | 3121 | `		VmPopOperand(&pTos,1);` |
|    30454 | 3122 | `		break;` |
|        - | 3123 | `	}` |
|        - | 3124 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|        - | 3125 | `	/* Force a string cast (user-visible: warns if the lvalue is an array, §2) */` |
|       16 | 3126 | `	PH7_MemObjToStringUV(pTos);` |
|        - | 3127 | `	/* Perform the concatenation (Reverse order) */` |
|       16 | 3128 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 | 3129 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 3130 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - | 3131 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 | 3132 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 3133 | `			goto Abort;` |
|        - | 3134 | `		}` |
|        7 | 3135 | `	}` |
|        - | 3136 | `	/* Perform the store operation */` |
|       16 | 3137 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 3138 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 | 3139 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       24 | 3140 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 | 3141 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 | 3142 | `	}` |
|       11 | 3143 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       11 | 3144 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 | 3145 | `	VmPopOperand(&pTos,1);` |
|       11 | 3146 | `	break;` |
|        - | 3147 | `				}` |
|        - | 3148 | `/* OP_AND: * * *` |
|        - | 3149 | ` *` |
|        - | 3150 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - | 3151 | ` * two values and push the resulting boolean value back onto the` |
|        - | 3152 | ` * stack.` |
|        - | 3153 | ` */` |
|        - | 3154 | `/* OP_OR: * * *` |
|        - | 3155 | ` *` |
|        - | 3156 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - | 3157 | ` * two values and push the resulting boolean value back onto the` |
|        - | 3158 | ` * stack.` |
|        - | 3159 | ` */` |
|   122752 | 3160 | `case PH7_OP_LAND:` |
|        - | 3161 | `case PH7_OP_LOR: {` |
|        - | 3162 | `	VmOpRc rcOp;` |
|   245842 | 3163 | `	sState.pTos = pTos;` |
|   245842 | 3164 | `	sState.pc = pc;` |
|   245842 | 3165 | `	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);` |
|   245842 | 3166 | `	pTos = sState.pTos;` |
|   245842 | 3167 | `	pc = sState.pc;` |
|   245842 | 3168 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3169 | `		goto Abort;` |
|   245842 | 3170 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3171 | `		goto Exception;` |
|        - | 3172 | `	}` |
|   245842 | 3173 | `	break;` |
|        - | 3174 | `					  }` |
|        - | 3175 | `/*` |
|        - | 3176 | ` * OP_NULLC: * * *` |
|        - | 3177 | ` * Null coalescing operator '??'.` |
|        - | 3178 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - | 3179 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - | 3180 | ` */` |
|        - | 3181 | `/*` |
|        - | 3182 | ` * OP_NULLC: * P2 *` |
|        - | 3183 | ` * Short-circuit null coalescing '??'.` |
|        - | 3184 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - | 3185 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - | 3186 | ` */` |
|      387 | 3187 | `case PH7_OP_NULLC: {` |
|        - | 3188 | `#ifdef UNTRUST` |
|        - | 3189 | `	if( pTos < pStack ){` |
|        - | 3190 | `		goto Abort;` |
|        - | 3191 | `	}` |
|        - | 3192 | `#endif` |
|      779 | 3193 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 3194 | `		/* Left is not null — keep it and skip the RHS */` |
|      447 | 3195 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|      226 | 3196 | `	}else{` |
|        - | 3197 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|      337 | 3198 | `		VmPopOperand(&pTos, 1);` |
|        - | 3199 | `	}` |
|      779 | 3200 | `	break;` |
|        - | 3201 | `}` |
|        - | 3202 | `/*` |
|        - | 3203 | ` * OP_NULLC_JMP: * P2 *` |
|        - | 3204 | ` * Null coalescing assignment short-circuit.` |
|        - | 3205 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - | 3206 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - | 3207 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - | 3208 | ` */` |
|       44 | 3209 | `case PH7_OP_NULLC_JMP: {` |
|        - | 3210 | `#ifdef UNTRUST` |
|        - | 3211 | `	if( pTos < pStack ){` |
|        - | 3212 | `		goto Abort;` |
|        - | 3213 | `	}` |
|        - | 3214 | `#endif` |
|       91 | 3215 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       30 | 3216 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —` |
|        - | 3217 | `		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is` |
|        - | 3218 | `		 * dropped by the fetch-point sweep (the landing pc is past its` |
|        - | 3219 | `		 * OP_NULLC_STORE window): the skipped assign never dispatches. */` |
|       14 | 3220 | `	}` |
|       91 | 3221 | `	break;` |
|        - | 3222 | `}` |
|        - | 3223 | `/*` |
|        - | 3224 | ` * OP_NULLC_STORE: * * *` |
|        - | 3225 | ` * Null coalescing assignment store.` |
|        - | 3226 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - | 3227 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - | 3228 | ` * expression result.` |
|        - | 3229 | ` */` |
|        - | 3230 | `/*` |
|        - | 3231 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - | 3232 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - | 3233 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - | 3234 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - | 3235 | ` * non-null, fall through without modifying the stack so the following` |
|        - | 3236 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - | 3237 | ` */` |
|       54 | 3238 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - | 3239 | `#ifdef UNTRUST` |
|        - | 3240 | `	if( pTos < pStack ){` |
|        - | 3241 | `		goto Abort;` |
|        - | 3242 | `	}` |
|        - | 3243 | `#endif` |
|      110 | 3244 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - | 3245 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - | 3246 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       44 | 3247 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       21 | 3248 | `	}` |
|      110 | 3249 | `	break;` |
|        - | 3250 | `}` |
|       27 | 3251 | `case PH7_OP_NULLC_STORE: {` |
|        - | 3252 | `	VmOpRc rcOp;` |
|       57 | 3253 | `	sState.pTos = pTos;` |
|       57 | 3254 | `	sState.pc = pc;` |
|       57 | 3255 | `	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);` |
|       57 | 3256 | `	pTos = sState.pTos;` |
|       57 | 3257 | `	pc = sState.pc;` |
|       57 | 3258 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3259 | `		goto Abort;` |
|       57 | 3260 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3261 | `		goto Exception;` |
|        - | 3262 | `	}` |
|       57 | 3263 | `	break;` |
|        - | 3264 | `					  }` |
|        - | 3265 | `/*` |
|        - | 3266 | ` * OP_SPREAD: * * *` |
|        - | 3267 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - | 3268 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - | 3269 | ` * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this` |
|        - | 3270 | ` * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —` |
|        - | 3271 | ` * the CALL may not be the next instruction, and may be an inner call whose own` |
|        - | 3272 | ` * spreads must stay scoped to it.` |
|        - | 3273 | ` * The expansion tail is shared between the plain-array and the materialized` |
|        - | 3274 | ` * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.` |
|        - | 3275 | ` */` |
|      151 | 3276 | `case PH7_OP_SPREAD: {` |
|        - | 3277 | `#ifdef UNTRUST` |
|        - | 3278 | `	if( pTos < pStack ){` |
|        - | 3279 | `		goto Abort;` |
|        - | 3280 | `	}` |
|        - | 3281 | `#endif` |
|        - | 3282 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - | 3283 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - | 3284 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - | 3285 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - | 3286 | `	 * be freed immediately. */` |
|      306 | 3287 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 | 3288 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - | 3289 | `		sxi32 rcW;` |
|        3 | 3290 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 | 3291 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 | 3292 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 | 3293 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 3294 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 3295 | `			goto Exception;` |
|        - | 3296 | `		}` |
|        - | 3297 | `		/* Grow the operand stack if this expansion would overflow it (no longer a` |
|        - | 3298 | `		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a` |
|        - | 3299 | `		 * single argument — the historical bounded-fallback, now only under OOM. */` |
|        4 | 3300 | `		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,` |
|        1 | 3301 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 3302 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 3303 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 3304 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 3305 | `				pTmpMap->nEntry);` |
|      ! 0 | 3306 | `			break;` |
|        - | 3307 | `		}` |
|        3 | 3308 | `		VmSpreadExpandMap(pVm, &pTos, pTmpMap);` |
|        3 | 3309 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 | 3310 | `		break;` |
|        - | 3311 | `	}` |
|      304 | 3312 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      304 | 3313 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      454 | 3314 | `		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,` |
|      150 | 3315 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 3316 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 3317 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 3318 | `				pMap->nEntry);` |
|      ! 0 | 3319 | `			break;` |
|        - | 3320 | `		}` |
|      304 | 3321 | `		VmSpreadExpandMap(pVm, &pTos, pMap);` |
|      150 | 3322 | `	}` |
|        - | 3323 | `	/* else: not an array — leave as-is (single arg) */` |
|      304 | 3324 | `	break;` |
|        - | 3325 | `}` |
|        - | 3326 | `/*` |
|        - | 3327 | ` * OP_FLAG_SPREAD: * * *` |
|        - | 3328 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - | 3329 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - | 3330 | ` */` |
|      340 | 3331 | `case PH7_OP_FLAG_SPREAD: {` |
|        - | 3332 | `#ifdef UNTRUST` |
|        - | 3333 | `	if( pTos < pStack ){` |
|        - | 3334 | `		goto Abort;` |
|        - | 3335 | `	}` |
|        - | 3336 | `#endif` |
|      683 | 3337 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|      683 | 3338 | `	break;` |
|        - | 3339 | `}` |
|        - | 3340 | `/* OP_LXOR: * * *` |
|        - | 3341 | ` *` |
|        - | 3342 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - | 3343 | ` * two values and push the resulting boolean value back onto the` |
|        - | 3344 | ` * stack.` |
|        - | 3345 | ` * According to the PHP language reference manual:` |
|        - | 3346 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - | 3347 | ` *  TRUE,but not both.` |
|        - | 3348 | ` */` |
|        6 | 3349 | `case PH7_OP_LXOR:{` |
|       13 | 3350 | `	ph7_value *pNos = &pTos[-1];` |
|       13 | 3351 | `	sxi32 v = 0;` |
|        - | 3352 | `#ifdef UNTRUST` |
|        - | 3353 | `	if( pNos < pStack ){` |
|        - | 3354 | `		goto Abort;` |
|        - | 3355 | `	}` |
|        - | 3356 | `#endif` |
|        - | 3357 | `	/* Force a boolean cast */` |
|       13 | 3358 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 3359 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 3360 | `	}` |
|       13 | 3361 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 3362 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 | 3363 | `	}` |
|       13 | 3364 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 | 3365 | `		v = 1;` |
|        3 | 3366 | `	}` |
|       13 | 3367 | `	VmPopOperand(&pTos,1);` |
|       13 | 3368 | `	pTos->x.iVal = v;` |
|       13 | 3369 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       13 | 3370 | `	break;` |
|        - | 3371 | `				 }` |
|        - | 3372 | `/* OP_EQ P1 P2 P3` |
|        - | 3373 | ` *` |
|        - | 3374 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - | 3375 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - | 3376 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3377 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3378 | ` */` |
|        - | 3379 | `/* OP_NEQ P1 P2 P3` |
|        - | 3380 | ` *` |
|        - | 3381 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - | 3382 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 3383 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3384 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3385 | ` */` |
|     5753 | 3386 | `case PH7_OP_EQ:` |
|        - | 3387 | `case PH7_OP_NEQ: {` |
|        - | 3388 | `	VmOpRc rcOp;` |
|    11511 | 3389 | `	sState.pTos = pTos;` |
|    11511 | 3390 | `	sState.pc = pc;` |
|    11511 | 3391 | `	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);` |
|    11511 | 3392 | `	pTos = sState.pTos;` |
|    11511 | 3393 | `	pc = sState.pc;` |
|    11511 | 3394 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3395 | `		goto Abort;` |
|    11511 | 3396 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3397 | `		goto Exception;` |
|        - | 3398 | `	}` |
|    11511 | 3399 | `	break;` |
|        - | 3400 | `					  }` |
|        - | 3401 | `/* OP_TEQ P1 P2 *` |
|        - | 3402 | ` *` |
|        - | 3403 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - | 3404 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 3405 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3406 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3407 | ` */` |
|   246851 | 3408 | `case PH7_OP_TEQ: {` |
|        - | 3409 | `	VmOpRc rcOp;` |
|   494040 | 3410 | `	sState.pTos = pTos;` |
|   494040 | 3411 | `	sState.pc = pc;` |
|   494040 | 3412 | `	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);` |
|   494040 | 3413 | `	pTos = sState.pTos;` |
|   494040 | 3414 | `	pc = sState.pc;` |
|   494040 | 3415 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3416 | `		goto Abort;` |
|   494040 | 3417 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3418 | `		goto Exception;` |
|        - | 3419 | `	}` |
|   494040 | 3420 | `	break;` |
|        - | 3421 | `					  }` |
|        - | 3422 | `/* OP_TNE P1 P2 *` |
|        - | 3423 | ` *` |
|        - | 3424 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - | 3425 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - | 3426 | ` * instruction.` |
|        - | 3427 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3428 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3429 | ` *` |
|        - | 3430 | ` */` |
|   212742 | 3431 | `case PH7_OP_TNE: {` |
|        - | 3432 | `	VmOpRc rcOp;` |
|   425822 | 3433 | `	sState.pTos = pTos;` |
|   425822 | 3434 | `	sState.pc = pc;` |
|   425822 | 3435 | `	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);` |
|   425822 | 3436 | `	pTos = sState.pTos;` |
|   425822 | 3437 | `	pc = sState.pc;` |
|   425822 | 3438 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3439 | `		goto Abort;` |
|   425822 | 3440 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3441 | `		goto Exception;` |
|        - | 3442 | `	}` |
|   425822 | 3443 | `	break;` |
|        - | 3444 | `					  }` |
|        - | 3445 | `/* OP_LT P1 P2 P3` |
|        - | 3446 | ` *` |
|        - | 3447 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 3448 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 3449 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 3450 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3451 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3452 | ` *` |
|        - | 3453 | ` */` |
|        - | 3454 | `/* OP_LE P1 P2 P3` |
|        - | 3455 | ` *` |
|        - | 3456 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 3457 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 3458 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 3459 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3460 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3461 | ` *` |
|        - | 3462 | ` */` |
|   232976 | 3463 | `case PH7_OP_LT:` |
|        - | 3464 | `case PH7_OP_LE: {` |
|        - | 3465 | `	VmOpRc rcOp;` |
|   466615 | 3466 | `	sState.pTos = pTos;` |
|   466615 | 3467 | `	sState.pc = pc;` |
|   466615 | 3468 | `	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);` |
|   466615 | 3469 | `	pTos = sState.pTos;` |
|   466615 | 3470 | `	pc = sState.pc;` |
|   466615 | 3471 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3472 | `		goto Abort;` |
|   466615 | 3473 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3474 | `		goto Exception;` |
|        - | 3475 | `	}` |
|   466615 | 3476 | `	break;` |
|        - | 3477 | `					  }` |
|        - | 3478 | `/* OP_GT P1 P2 P3` |
|        - | 3479 | ` *` |
|        - | 3480 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 3481 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 3482 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 3483 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3484 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3485 | ` *` |
|        - | 3486 | ` */` |
|        - | 3487 | `/* OP_GE P1 P2 P3` |
|        - | 3488 | ` *` |
|        - | 3489 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 3490 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 3491 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 3492 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3493 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3494 | ` *` |
|        - | 3495 | ` */` |
|    96981 | 3496 | `case PH7_OP_GT:` |
|        - | 3497 | `case PH7_OP_GE: {` |
|        - | 3498 | `	VmOpRc rcOp;` |
|   194300 | 3499 | `	sState.pTos = pTos;` |
|   194300 | 3500 | `	sState.pc = pc;` |
|   194300 | 3501 | `	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);` |
|   194300 | 3502 | `	pTos = sState.pTos;` |
|   194300 | 3503 | `	pc = sState.pc;` |
|   194300 | 3504 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3505 | `		goto Abort;` |
|   194300 | 3506 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3507 | `		goto Exception;` |
|        - | 3508 | `	}` |
|   194300 | 3509 | `	break;` |
|        - | 3510 | `					  }` |
|        - | 3511 | `/* OP_SPACESHIP * * *` |
|        - | 3512 | ` *` |
|        - | 3513 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - | 3514 | ` *   -1 if left < right` |
|        - | 3515 | ` *    0 if left == right` |
|        - | 3516 | ` *    1 if left > right` |
|        - | 3517 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - | 3518 | ` */` |
|      216 | 3519 | `case PH7_OP_SPACESHIP: {` |
|        - | 3520 | `	VmOpRc rcOp;` |
|      433 | 3521 | `	sState.pTos = pTos;` |
|      433 | 3522 | `	sState.pc = pc;` |
|      433 | 3523 | `	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);` |
|      433 | 3524 | `	pTos = sState.pTos;` |
|      433 | 3525 | `	pc = sState.pc;` |
|      433 | 3526 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3527 | `		goto Abort;` |
|      433 | 3528 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3529 | `		goto Exception;` |
|        - | 3530 | `	}` |
|      433 | 3531 | `	break;` |
|        - | 3532 | `					  }` |
|        - | 3533 | `/*` |
|        - | 3534 | ` * OP_LOAD_REF * * *` |
|        - | 3535 | ` * Push the index of a referenced object on the stack.` |
|        - | 3536 | ` */` |
|       60 | 3537 | `case PH7_OP_LOAD_REF: {` |
|        - | 3538 | `	sxu32 nIdx;` |
|        - | 3539 | `#ifdef UNTRUST` |
|        - | 3540 | `	if( pTos < pStack ){` |
|        - | 3541 | `		goto Abort;` |
|        - | 3542 | `	}` |
|        - | 3543 | `#endif` |
|        - | 3544 | `	/* Extract memory object index */` |
|      121 | 3545 | `	nIdx = pTos->nIdx;` |
|      121 | 3546 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - | 3547 | `		/* Nullify the object */` |
|      121 | 3548 | `		PH7_MemObjRelease(pTos);` |
|        - | 3549 | `		/* Mark as constant and store the index on the top of the stack */` |
|      121 | 3550 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      121 | 3551 | `		pTos->nIdx = SXU32_HIGH;` |
|      121 | 3552 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       60 | 3553 | `	}` |
|      121 | 3554 | `	break;` |
|        - | 3555 | `					  }` |
|        - | 3556 | `/*` |
|        - | 3557 | ` * OP_STORE_REF * * P3` |
|        - | 3558 | ` * Perform an assignment operation by reference.` |
|        - | 3559 | ` */` |
|       26 | 3560 | `case PH7_OP_STORE_REF: {` |
|        - | 3561 | `	VmOpRc rcOp;` |
|       56 | 3562 | `	sState.pTos = pTos;` |
|       56 | 3563 | `	sState.pc = pc;` |
|       56 | 3564 | `	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);` |
|       56 | 3565 | `	pTos = sState.pTos;` |
|       56 | 3566 | `	pc = sState.pc;` |
|       56 | 3567 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3568 | `		goto Abort;` |
|       53 | 3569 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3570 | `		goto Exception;` |
|        - | 3571 | `	}` |
|       53 | 3572 | `	break;` |
|        - | 3573 | `					  }` |
|        - | 3574 | `/*` |
|        - | 3575 | ` * OP_UPLINK P1 * *` |
|        - | 3576 | ` * Link a variable to the top active VM frame.` |
|        - | 3577 | ` * This is used to implement the 'global' PHP construct.` |
|        - | 3578 | ` */` |
|       29 | 3579 | `case PH7_OP_UPLINK: {` |
|       63 | 3580 | `	if( pVm->pFrame->pParent ){` |
|       63 | 3581 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - | 3582 | `		SyString sName;` |
|        - | 3583 | `		/* Perform the link */` |
|      131 | 3584 | `		while( pLink <= pTos ){` |
|        - | 3585 | `			/* Force a string cast — global $$arr link name (user-visible, §2) */` |
|       73 | 3586 | `			PH7_MemObjToStringUV(pLink);` |
|       73 | 3587 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       73 | 3588 | `			if( sName.nByte > 0 ){` |
|       73 | 3589 | `				VmFrameLink(&(*pVm),&sName);` |
|       34 | 3590 | `			}` |
|       73 | 3591 | `			pLink++;` |
|        5 | 3592 | `		}` |
|       29 | 3593 | `	}` |
|       63 | 3594 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       63 | 3595 | `	break;` |
|        - | 3596 | `					}` |
|        - | 3597 | `/*` |
|        - | 3598 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - | 3599 | ` * Push an exception in the corresponding container so that` |
|        - | 3600 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - | 3601 | ` */` |
|   723752 | 3602 | `case PH7_OP_LOAD_EXCEPTION: {` |
|        - | 3603 | `	VmOpRc rcOp;` |
|  1447509 | 3604 | `	sState.pTos = pTos;` |
|  1447509 | 3605 | `	sState.pc = pc;` |
|  1447509 | 3606 | `	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);` |
|  1447509 | 3607 | `	pTos = sState.pTos;` |
|  1447509 | 3608 | `	pc = sState.pc;` |
|  1447509 | 3609 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3610 | `		goto Abort;` |
|  1447509 | 3611 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3612 | `		goto Exception;` |
|        - | 3613 | `	}` |
|  1447509 | 3614 | `	break;` |
|        - | 3615 | `					  }` |
|        - | 3616 | `/*` |
|        - | 3617 | ` * OP_POP_EXCEPTION * * P3` |
|        - | 3618 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - | 3619 | ` */` |
|   673574 | 3620 | `case PH7_OP_POP_EXCEPTION: {` |
|  1347153 | 3621 | `	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;` |
|        - | 3622 | `	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on` |
|        - | 3623 | `	 * top (matched by compiled origin). pException == NULL means this try's` |
|        - | 3624 | `	 * activation was already consumed (an in-place catch handled a throw and` |
|        - | 3625 | `	 * ran the finally itself); compiled fields keep coming from p3. */` |
|  1347153 | 3626 | `	ph7_exception *pException = 0;` |
|  1347153 | 3627 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|      501 | 3628 | `		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      501 | 3629 | `		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];` |
|        - | 3630 | `		/* Same compiled origin is NOT enough: under recursion, when THIS` |
|        - | 3631 | `		 * level's activation was already consumed by an in-place catch (the` |
|        - | 3632 | `		 * resume lands right here), the top can be an OUTER level's activation` |
|        - | 3633 | `		 * of the same lexical try — popping it would run that level's finally` |
|        - | 3634 | `		 * early and orphan its handler (probed: recursive try/catch/finally` |
|        - | 3635 | `		 * lost the outer catch entirely). The activation must also belong to` |
|        - | 3636 | `		 * the CURRENT body frame. */` |
|      496 | 3637 | `		if( VmExcMatches(pTop,pCompiledExc)` |
|      494 | 3638 | `		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){` |
|      481 | 3639 | `			pException = pTop;` |
|      481 | 3640 | `			(void)SySetPop(&pVm->aException);` |
|      238 | 3641 | `		}` |
|      248 | 3642 | `	}` |
|  1347153 | 3643 | `	if( pCompiledExc->iInlined ){` |
|        - | 3644 | `		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.` |
|        - | 3645 | `		 * Pop this try's handler off aException so a throw in the finally propagates to` |
|        - | 3646 | `		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the` |
|        - | 3647 | `		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the` |
|        - | 3648 | `		 * compiler-emitted JMP falls into the finally. Without a finally, this is the` |
|        - | 3649 | `		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */` |
|      165 | 3650 | `		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */` |
|      165 | 3651 | `		if( pCompiledExc->iHasFinally ){` |
|        - | 3652 | `			VmFinallyAction sAct;` |
|       15 | 3653 | `			SyZero(&sAct,sizeof(sAct));` |
|       15 | 3654 | `			sAct.eKind = PH7_FA_FALLTHROUGH;` |
|       15 | 3655 | `			sAct.iNextPc = pCompiledExc->iEndCatchPc;` |
|       15 | 3656 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        - | 3657 | `			/* keep the transparent frame for OP_END_FINALLY */` |
|      159 | 3658 | `		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       20 | 3659 | `			VmLeaveFrame(&(*pVm));` |
|        8 | 3660 | `		}` |
|      165 | 3661 | `		break;` |
|        - | 3662 | `	}` |
|        - | 3663 | `	/* Leave the exception frame. It is normally on top here (a try that fell through` |
|        - | 3664 | `	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a` |
|        - | 3665 | `	 * RESUMED generator/fiber body whose yield was inside this try, that exception` |
|        - | 3666 | `	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),` |
|        - | 3667 | `	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the` |
|        - | 3668 | `	 * coroutine (and defeat the bHasRet materialization just below, which must see the` |
|        - | 3669 | `	 * body). Only leave a genuine exception frame. */` |
|  1346993 | 3670 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|  1146077 | 3671 | `		VmLeaveFrame(&(*pVm));` |
|   573036 | 3672 | `	}` |
|        - | 3673 | `	/* Execute the finally block if present and not already executed by the` |
|        - | 3674 | `	 * catch path. No live activation (pException == NULL) means an in-place` |
|        - | 3675 | `	 * catch consumed it — and that path runs the finally itself — so skip. */` |
|  1346993 | 3676 | `	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){` |
|        - | 3677 | `		sxi32 rcFinally;` |
|       45 | 3678 | `		VmExcRelease(&(*pVm),pException);` |
|       45 | 3679 | `		pException = 0;` |
|       45 | 3680 | `		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);` |
|       45 | 3681 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 | 3682 | `			goto Abort;` |
|        - | 3683 | `		}` |
|       45 | 3684 | `		if( rcFinally == PH7_EXCEPTION ){` |
|        - | 3685 | `			/* The finally threw past itself. If an enclosing try IN THIS function` |
|        - | 3686 | `			 * caught the new exception in place, resume at its landing pad;` |
|        - | 3687 | `			 * otherwise it was caught at an outer frame, so unwind this function. */` |
|        - | 3688 | `			sxi32 iResumePc;` |
|        5 | 3689 | `			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 3690 | `				pc = iResumePc;` |
|        3 | 3691 | `				break;` |
|        - | 3692 | `			}` |
|        3 | 3693 | `			goto Exception;` |
|        - | 3694 | `		}` |
|       18 | 3695 | `	}` |
|  1346989 | 3696 | `	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */` |
|  1346989 | 3697 | `	if( VmSkipExceptionFrames(pVm->pFrame)->bHasRet ){` |
|        - | 3698 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - | 3699 | `		 * function. The return targets the body frame this try belongs to. Drain` |
|        - | 3700 | `		 * outer finally blocks first, then — only in the real function body` |
|        - | 3701 | `		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline` |
|        - | 3702 | `		 * try within a catch/finally) propagate outward so the owning body returns. */` |
|    20188 | 3703 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    20188 | 3704 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3705 | `			goto Abort;` |
|        - | 3706 | `		}` |
|    20188 | 3707 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 3708 | `			goto Exception;` |
|        - | 3709 | `		}` |
|    20188 | 3710 | `		if( !sState.bReturnPropagates ){` |
|    20182 | 3711 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|    10089 | 3712 | `		}` |
|    20188 | 3713 | `		goto Done;` |
|        - | 3714 | `	}` |
|  1326805 | 3715 | `	break;` |
|        - | 3716 | `							}` |
|        - | 3717 | `/*` |
|        - | 3718 | ` * OP_CATCH iP1(catch-index) * P3(ph7_exception)` |
|        - | 3719 | ` * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on` |
|        - | 3720 | ` * pException->pInflight by VmThrowInline) into the catch variable, resolved in the` |
|        - | 3721 | ` * enclosing body's scope (PHP: a catch shares the surrounding variable scope).` |
|        - | 3722 | ` */` |
|       35 | 3723 | `case PH7_OP_CATCH: {` |
|        - | 3724 | `	VmOpRc rcOp;` |
|       75 | 3725 | `	sState.pTos = pTos;` |
|       75 | 3726 | `	sState.pc = pc;` |
|       75 | 3727 | `	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);` |
|       75 | 3728 | `	pTos = sState.pTos;` |
|       75 | 3729 | `	pc = sState.pc;` |
|       75 | 3730 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3731 | `		goto Abort;` |
|       75 | 3732 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3733 | `		goto Exception;` |
|        - | 3734 | `	}` |
|       75 | 3735 | `	break;` |
|        - | 3736 | `					  }` |
|        - | 3737 | `/*` |
|        - | 3738 | ` * OP_END_FINALLY * P3(ph7_exception)` |
|        - | 3739 | ` * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch` |
|        - | 3740 | ` * the pending action queued when the finally was entered (fall-through / re-throw /` |
|        - | 3741 | ` * return / break-continue). A return/break threads out through each enclosing finally` |
|        - | 3742 | ` * via pException->iNextFinallyPc.` |
|        - | 3743 | ` */` |
|       23 | 3744 | `case PH7_OP_END_FINALLY: {` |
|       50 | 3745 | `	ph7_exception *pExc = (ph7_exception *)pInstr->p3;` |
|        - | 3746 | `	VmFinallyAction sAct;` |
|       50 | 3747 | `	int eKind = PH7_FA_FALLTHROUGH;` |
|        - | 3748 | `	/* Leave the try's transparent frame kept alive across the finally. */` |
|       50 | 3749 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        3 | 3750 | `		VmLeaveFrame(&(*pVm));` |
|        1 | 3751 | `	}` |
|       50 | 3752 | `	if( SySetUsed(&pVm->aFinallyAction) > 0 ){` |
|       50 | 3753 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);` |
|       50 | 3754 | `		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];` |
|       50 | 3755 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|       50 | 3756 | `		eKind = sAct.eKind;` |
|       27 | 3757 | `	}else{` |
|      ! 0 | 3758 | `		SyZero(&sAct,sizeof(sAct));` |
|      ! 0 | 3759 | `		sAct.iNextPc = pExc->iEndCatchPc;` |
|        - | 3760 | `	}` |
|       50 | 3761 | `	if( eKind == PH7_FA_FALLTHROUGH ){` |
|       13 | 3762 | `		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;` |
|       16 | 3763 | `		break;` |
|       40 | 3764 | `	}else if( eKind == PH7_FA_JMP ){` |
|        - | 3765 | `		/* break/continue: run the remaining crossed finallys, then take the jump. */` |
|        3 | 3766 | `		sxu32 iFpc = 0;` |
|        3 | 3767 | `		int nCross = sAct.nCross;` |
|        3 | 3768 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|      ! 0 | 3769 | `			sAct.nCross = nCross;` |
|      ! 0 | 3770 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      ! 0 | 3771 | `			pc = (sxi32)iFpc - 1;` |
|      ! 0 | 3772 | `			break;` |
|        - | 3773 | `		}` |
|        3 | 3774 | `		pc = (sxi32)sAct.iNextPc - 1;` |
|        3 | 3775 | `		break;` |
|       38 | 3776 | `	}else if( eKind == PH7_FA_RETHROW ){` |
|        8 | 3777 | `		ph7_class_instance *pRe = sAct.pExc;` |
|        - | 3778 | `		sxi32 _iRpE;` |
|        8 | 3779 | `		rc = VmThrowException(&(*pVm),pRe);` |
|        8 | 3780 | `		if( pRe ){ PH7_ClassInstanceUnref(pRe); }` |
|        8 | 3781 | `		if( rc == SXERR_ABORT ){ goto Abort; }` |
|        8 | 3782 | `		PH7_INLINE_RESUME_BREAK()` |
|        8 | 3783 | `		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }` |
|        8 | 3784 | `		goto Exception;` |
|      ! 0 | 3785 | `	}else{ /* PH7_FA_RETURN */` |
|       31 | 3786 | `		sxu32 iFpc = 0;` |
|       31 | 3787 | `		int nCross = sAct.nCross;` |
|       31 | 3788 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        - | 3789 | `			/* Thread the return through the next enclosing finally. */` |
|        6 | 3790 | `			sAct.nCross = nCross;` |
|        6 | 3791 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        6 | 3792 | `			pc = (sxi32)iFpc - 1;` |
|        6 | 3793 | `			break;` |
|        - | 3794 | `		}` |
|        - | 3795 | `		/* No enclosing finally left: materialize the return from this body. */` |
|       27 | 3796 | `		if( sAct.bHasRetVal && sState.pResult ){` |
|        6 | 3797 | `			PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|        2 | 3798 | `		}` |
|       27 | 3799 | `		PH7_MemObjRelease(&sAct.sRet);` |
|       27 | 3800 | `		goto Done;` |
|        - | 3801 | `	}` |
|        - | 3802 | `						 }` |
|        - | 3803 | `/*` |
|        - | 3804 | ` * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)` |
|        - | 3805 | `` * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a`` |
|        - | 3806 | ` * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);` |
|        - | 3807 | ` * OP_END_FINALLY threads it out through the finally chain and then returns.` |
|        - | 3808 | ` */` |
|       19 | 3809 | `case PH7_OP_SET_FINALLY_RET: {` |
|        - | 3810 | `	VmFinallyAction sAct;` |
|       42 | 3811 | `	sxu32 iFpc = 0;` |
|       42 | 3812 | `	int nCross = -1; /* a return crosses every enclosing finally in this function */` |
|       42 | 3813 | `	SyZero(&sAct,sizeof(sAct));` |
|       42 | 3814 | `	sAct.eKind = PH7_FA_RETURN;` |
|       42 | 3815 | `	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       42 | 3816 | `	PH7_MemObjInit(pVm,&sAct.sRet);` |
|       42 | 3817 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|       32 | 3818 | `		PH7_MemObjStore(pTos,&sAct.sRet);` |
|       32 | 3819 | `		sAct.bHasRetVal = 1;` |
|       32 | 3820 | `		VmPopOperand(&pTos,1);` |
|       14 | 3821 | `	}` |
|       42 | 3822 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        9 | 3823 | `		sAct.nCross = nCross;` |
|        9 | 3824 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        9 | 3825 | `		pc = (sxi32)iFpc - 1;` |
|        9 | 3826 | `		break;` |
|        - | 3827 | `	}` |
|        - | 3828 | `	/* No enclosing finally left: return now. */` |
|       36 | 3829 | `	if( sAct.bHasRetVal && sState.pResult ){` |
|       28 | 3830 | `		PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|       12 | 3831 | `	}` |
|       36 | 3832 | `	PH7_MemObjRelease(&sAct.sRet);` |
|       36 | 3833 | `	goto Done;` |
|        - | 3834 | `						 }` |
|        - | 3835 | `/*` |
|        - | 3836 | ` * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)` |
|        - | 3837 | `` * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action`` |
|        - | 3838 | ` * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.` |
|        - | 3839 | ` */` |
|        1 | 3840 | `case PH7_OP_SET_FINALLY_JMP: {` |
|        - | 3841 | `	VmFinallyAction sAct;` |
|        3 | 3842 | `	sxu32 iFpc = 0;` |
|        3 | 3843 | `	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */` |
|        3 | 3844 | `	SyZero(&sAct,sizeof(sAct));` |
|        3 | 3845 | `	sAct.eKind = PH7_FA_JMP;` |
|        3 | 3846 | `	sAct.iNextPc = pInstr->iP2;` |
|        3 | 3847 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        3 | 3848 | `		sAct.nCross = nCross;` |
|        3 | 3849 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        3 | 3850 | `		pc = (sxi32)iFpc - 1;` |
|        3 | 3851 | `		break;` |
|        - | 3852 | `	}` |
|        - | 3853 | `	/* No finally among the crossed trys: just take the break/continue jump. */` |
|      ! 0 | 3854 | `	pc = (sxi32)sAct.iNextPc - 1;` |
|      ! 0 | 3855 | `	break;` |
|        - | 3856 | `						 }` |
|        - | 3857 | `/*` |
|        - | 3858 | ` * OP_THROW * P2 *` |
|        - | 3859 | ` * Throw an user exception.` |
|        - | 3860 | ` */` |
|   500410 | 3861 | `case PH7_OP_THROW: {` |
|        - | 3862 | `	VmOpRc rcOp;` |
|  1000825 | 3863 | `	sState.pTos = pTos;` |
|  1000825 | 3864 | `	sState.pc = pc;` |
|  1000825 | 3865 | `	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);` |
|  1000825 | 3866 | `	pTos = sState.pTos;` |
|  1000825 | 3867 | `	pc = sState.pc;` |
|  1000825 | 3868 | `	if( rcOp == VM_OP_ABORT ){` |
|       34 | 3869 | `		goto Abort;` |
|  1000795 | 3870 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|   600463 | 3871 | `		goto Exception;` |
|        - | 3872 | `	}` |
|   400337 | 3873 | `	break;` |
|        - | 3874 | `					  }` |
|        - | 3875 | `/*` |
|        - | 3876 | ` * OP_FOREACH_INIT * P2 P3` |
|        - | 3877 | ` * Prepare a foreach step.` |
|        - | 3878 | ` */` |
|    12726 | 3879 | `case PH7_OP_FOREACH_INIT: {` |
|        - | 3880 | `	VmOpRc rcOp;` |
|    25457 | 3881 | `	sState.pTos = pTos;` |
|    25457 | 3882 | `	sState.pc = pc;` |
|    25457 | 3883 | `	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);` |
|    25457 | 3884 | `	pTos = sState.pTos;` |
|    25457 | 3885 | `	pc = sState.pc;` |
|    25457 | 3886 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3887 | `		goto Abort;` |
|    25457 | 3888 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3889 | `		goto Exception;` |
|        - | 3890 | `	}` |
|    25457 | 3891 | `	break;` |
|        - | 3892 | `					  }` |
|        - | 3893 | `/*` |
|        - | 3894 | ` * OP_FOREACH_STEP * P2 P3` |
|        - | 3895 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - | 3896 | ` */` |
|   134281 | 3897 | `case PH7_OP_FOREACH_STEP: {` |
|        - | 3898 | `	VmOpRc rcOp;` |
|   268567 | 3899 | `	sState.pTos = pTos;` |
|   268567 | 3900 | `	sState.pc = pc;` |
|   268567 | 3901 | `	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);` |
|   268567 | 3902 | `	pTos = sState.pTos;` |
|   268567 | 3903 | `	pc = sState.pc;` |
|   268567 | 3904 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3905 | `		goto Abort;` |
|   268565 | 3906 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3907 | `		goto Exception;` |
|        - | 3908 | `	}` |
|   268565 | 3909 | `	break;` |
|        - | 3910 | `						  }` |
|        - | 3911 | `/*` |
|        - | 3912 | ` * OP_MEMBER P1 P2` |
|        - | 3913 | ` * Load class attribute/method on the stack.` |
|        - | 3914 | ` */` |
|  1618177 | 3915 | `case PH7_OP_MEMBER: {` |
|        - | 3916 | `	VmOpRc rcOp;` |
|  3236359 | 3917 | `	sState.pTos = pTos;` |
|  3236359 | 3918 | `	sState.pc = pc;` |
|  3236359 | 3919 | `	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);` |
|  3236359 | 3920 | `	pTos = sState.pTos;` |
|  3236359 | 3921 | `	pc = sState.pc;` |
|  3236359 | 3922 | `	if( rcOp == VM_OP_ABORT ){` |
|        8 | 3923 | `		goto Abort;` |
|  3236353 | 3924 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       11 | 3925 | `		goto Exception;` |
|        - | 3926 | `	}` |
|  3236343 | 3927 | `	break;` |
|        - | 3928 | `					  }` |
|        - | 3929 | `/*` |
|        - | 3930 | ` * OP_NEW P1 * * *` |
|        - | 3931 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - | 3932 | ` */` |
|   552596 | 3933 | `case PH7_OP_NEW: {` |
|        - | 3934 | `	VmOpRc rcOp;` |
|  1105197 | 3935 | `	sState.pTos = pTos;` |
|  1105197 | 3936 | `	sState.pc = pc;` |
|  1105197 | 3937 | `	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);` |
|  1105197 | 3938 | `	pTos = sState.pTos;` |
|  1105197 | 3939 | `	pc = sState.pc;` |
|  1105197 | 3940 | `	if( rcOp == VM_OP_ABORT ){` |
|        5 | 3941 | `		goto Abort;` |
|  1105193 | 3942 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 3943 | `		goto Exception;` |
|        - | 3944 | `	}` |
|  1105185 | 3945 | `	break;` |
|        - | 3946 | `					  }` |
|        - | 3947 | `/*` |
|        - | 3948 | ` * OP_CLONE * * *` |
|        - | 3949 | ` * Perfome a clone operation.` |
|        - | 3950 | ` */` |
|      108 | 3951 | `case PH7_OP_CLONE: {` |
|        - | 3952 | `	VmOpRc rcOp;` |
|      220 | 3953 | `	sState.pTos = pTos;` |
|      220 | 3954 | `	sState.pc = pc;` |
|      220 | 3955 | `	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);` |
|      220 | 3956 | `	pTos = sState.pTos;` |
|      220 | 3957 | `	pc = sState.pc;` |
|      220 | 3958 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3959 | `		goto Abort;` |
|      220 | 3960 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 3961 | `		goto Exception;` |
|        - | 3962 | `	}` |
|      218 | 3963 | `	break;` |
|        - | 3964 | `					  }` |
|        - | 3965 | `/*` |
|        - | 3966 | ` * OP_CLONE_APPLY * * *` |
|        - | 3967 | ` *  Apply the PHP 8.5 clone($obj, $withProperties) property updates. The updates` |
|        - | 3968 | ` *  array is on the stack top and the freshly-cloned object (from OP_CLONE) is` |
|        - | 3969 | ` *  directly below it. Each entry is applied as a scope-aware property write` |
|        - | 3970 | ` *  (AFTER __clone() has already run); the array is then popped, leaving the` |
|        - | 3971 | ` *  clone as the result.` |
|        - | 3972 | ` */` |
|        8 | 3973 | `case PH7_OP_CLONE_APPLY: {` |
|        - | 3974 | `	VmOpRc rcOp;` |
|       17 | 3975 | `	sState.pTos = pTos;` |
|       17 | 3976 | `	sState.pc = pc;` |
|       17 | 3977 | `	rcOp = VmExecOpCloneApply(&(*pVm),&sState,pInstr);` |
|       17 | 3978 | `	pTos = sState.pTos;` |
|       17 | 3979 | `	pc = sState.pc;` |
|       17 | 3980 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3981 | `		goto Abort;` |
|       17 | 3982 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3983 | `		goto Exception;` |
|        - | 3984 | `	}` |
|       17 | 3985 | `	break;` |
|        - | 3986 | `					  }` |
|        - | 3987 | `/*` |
|        - | 3988 | ` * OP_SWITCH * * P3` |
|        - | 3989 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - | 3990 | ` */` |
|      164 | 3991 | `case PH7_OP_SWITCH: {` |
|        - | 3992 | `	VmOpRc rcOp;` |
|      333 | 3993 | `	sState.pTos = pTos;` |
|      333 | 3994 | `	sState.pc = pc;` |
|      333 | 3995 | `	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);` |
|      333 | 3996 | `	pTos = sState.pTos;` |
|      333 | 3997 | `	pc = sState.pc;` |
|      333 | 3998 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3999 | `		goto Abort;` |
|      333 | 4000 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4001 | `		goto Exception;` |
|        - | 4002 | `	}` |
|      333 | 4003 | `	break;` |
|        - | 4004 | `					  }` |
|        - | 4005 | `/*` |
|        - | 4006 | ` * OP_MATCH * * P3` |
|        - | 4007 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - | 4008 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - | 4009 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - | 4010 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - | 4011 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - | 4012 | ` */` |
|       62 | 4013 | `case PH7_OP_MATCH: {` |
|        - | 4014 | `	VmOpRc rcOp;` |
|      127 | 4015 | `	sState.pTos = pTos;` |
|      127 | 4016 | `	sState.pc = pc;` |
|      127 | 4017 | `	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);` |
|      127 | 4018 | `	pTos = sState.pTos;` |
|      127 | 4019 | `	pc = sState.pc;` |
|      127 | 4020 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4021 | `		goto Abort;` |
|      127 | 4022 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        6 | 4023 | `		goto Exception;` |
|        - | 4024 | `	}` |
|      123 | 4025 | `	break;` |
|        - | 4026 | `					  }` |
|        - | 4027 | `/*` |
|        - | 4028 | ` * OP_YIELD P1 P2 *` |
|        - | 4029 | ` *  Yield a value from a generator function.` |
|        - | 4030 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - | 4031 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - | 4032 | ` */` |
|      590 | 4033 | `case PH7_OP_YIELD: {` |
|        - | 4034 | `	ph7_generator *pGen;` |
|     1185 | 4035 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 4036 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 | 4037 | `		goto Abort;` |
|        - | 4038 | `	}` |
|     1185 | 4039 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 4040 | ``		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's`` |
|        - | 4041 | `		 * pending finallys tried to yield — PHP forbids it. */` |
|      ! 0 | 4042 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 4043 | `			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 4044 | `			goto Abort;` |
|        - | 4045 | `		}` |
|      ! 0 | 4046 | `		goto Exception;` |
|        - | 4047 | `	}` |
|     1185 | 4048 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|     1185 | 4049 | `	if( pInstr->iP2 ){` |
|        - | 4050 | `		/* yield $key => $value: value on top, key below */` |
|        - | 4051 | `#ifdef UNTRUST` |
|        - | 4052 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - | 4053 | `#endif` |
|       70 | 4054 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       70 | 4055 | `		VmPopOperand(&pTos, 1);` |
|       70 | 4056 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       70 | 4057 | `		VmPopOperand(&pTos, 1);` |
|        - | 4058 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       70 | 4059 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|       47 | 4060 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|       47 | 4061 | `			if( nKey >= pGen->iImplicitKey ){` |
|       47 | 4062 | `				pGen->iImplicitKey = nKey + 1;` |
|       23 | 4063 | `			}` |
|       25 | 4064 | `		}` |
|     1151 | 4065 | `	}else if( pInstr->iP1 ){` |
|        - | 4066 | `		/* yield $value */` |
|        - | 4067 | `#ifdef UNTRUST` |
|        - | 4068 | `		if( pTos < pStack ) goto Abort;` |
|        - | 4069 | `#endif` |
|     1117 | 4070 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|     1117 | 4071 | `		VmPopOperand(&pTos, 1);` |
|        - | 4072 | `		/* Auto-increment key */` |
|     1117 | 4073 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|     1117 | 4074 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|     1117 | 4075 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|      561 | 4076 | `	}else{` |
|        - | 4077 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 | 4078 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 4079 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 4080 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 | 4081 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - | 4082 | `	}` |
|        - | 4083 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|     1185 | 4084 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|     1185 | 4085 | `	goto Suspend;` |
|        - | 4086 | `}` |
|        - | 4087 | `/*` |
|        - | 4088 | ` * OP_YIELD_FROM * * *` |
|        - | 4089 | ` *` |
|        - | 4090 | `` * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an`` |
|        - | 4091 | ` * array/Traversable/Generator from the OUTER generator, preserving the inner` |
|        - | 4092 | ` * keys; the expression evaluates to the inner Generator's return value (or NULL).` |
|        - | 4093 | ` *` |
|        - | 4094 | ` * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,` |
|        - | 4095 | ` * advances the per-instance delegate cursor stored on the exec context (never the` |
|        - | 4096 | ` * shared foreach aStep, so independent generator instances cannot clash). The` |
|        - | 4097 | ` * iterable operand is consumed on first entry; the expression result is pushed at` |
|        - | 4098 | ` * exhaustion — net stack effect +1, identical to OP_YIELD.` |
|        - | 4099 | ` */` |
|       93 | 4100 | `case PH7_OP_YIELD_FROM: {` |
|        - | 4101 | `	ph7_generator *pGenFrom;` |
|        - | 4102 | `	ph7_exec_ctx *pCtxFrom;` |
|        - | 4103 | `	ph7_value sKey,sVal;` |
|      191 | 4104 | `	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */` |
|      191 | 4105 | `	int bExhausted = 0;` |
|      191 | 4106 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 4107 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");` |
|      ! 0 | 4108 | `		goto Abort;` |
|        - | 4109 | `	}` |
|      191 | 4110 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 4111 | ``		/* A `yield from` reached while VmCloseCtx force-drives this destroyed`` |
|        - | 4112 | `		 * generator's pending finallys — PHP forbids it (distinct message from a` |
|        - | 4113 | ``		 * bare `yield`, mirroring the OP_YIELD guard above). */`` |
|      ! 0 | 4114 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 4115 | `			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 4116 | `			goto Abort;` |
|        - | 4117 | `		}` |
|      ! 0 | 4118 | `		goto Exception;` |
|        - | 4119 | `	}` |
|      191 | 4120 | `	pCtxFrom = pVm->pActiveCtx;` |
|      191 | 4121 | `	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;` |
|      191 | 4122 | `	PH7_MemObjInit(pVm,&sKey);` |
|      191 | 4123 | `	PH7_MemObjInit(pVm,&sVal);` |
|      191 | 4124 | `	if( pCtxFrom->iDelegateState == 0 ){` |
|        - | 4125 | `		/* First entry: classify the iterable on the stack top. */` |
|       79 | 4126 | `		int bIterable = 1;` |
|        - | 4127 | `#ifdef UNTRUST` |
|        - | 4128 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 4129 | `#endif` |
|       79 | 4130 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       29 | 4131 | `			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       29 | 4132 | `			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;` |
|       29 | 4133 | `			pCtxFrom->iDelegateState = 1;` |
|       67 | 4134 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       51 | 4135 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       51 | 4136 | `			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       51 | 4137 | `			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){` |
|       41 | 4138 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       41 | 4139 | `				pCtxFrom->iDelegateState = 3;` |
|       31 | 4140 | `			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){` |
|        9 | 4141 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        9 | 4142 | `				pCtxFrom->iDelegateState = 2;` |
|        6 | 4143 | `			}else{` |
|        6 | 4144 | `				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - | 4145 | `					sizeof("IteratorAggregate")-1,FALSE,0);` |
|        8 | 4146 | `				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){` |
|        - | 4147 | `					/* Delegate to the Iterator returned by getIterator() */` |
|        - | 4148 | `					ph7_value sIt;` |
|        6 | 4149 | `					PH7_MemObjInit(pVm,&sIt);` |
|        6 | 4150 | `					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);` |
|        6 | 4151 | `					if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){` |
|        - | 4152 | `						/* getIterator() threw/aborted: drop it, consume the` |
|        - | 4153 | `						 * operand, and propagate. */` |
|      ! 0 | 4154 | `						PH7_MemObjRelease(&sIt);` |
|      ! 0 | 4155 | `						VmPopOperand(&pTos,1);` |
|      ! 0 | 4156 | `						goto yf_propagate;` |
|        - | 4157 | `					}` |
|        4 | 4158 | `					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls` |
|        6 | 4159 | `						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){` |
|        6 | 4160 | `						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);` |
|        6 | 4161 | `						pCtxFrom->iDelegateState = 2;` |
|        4 | 4162 | `					}else{` |
|      ! 0 | 4163 | `						bIterable = 0;` |
|        - | 4164 | `					}` |
|        6 | 4165 | `					PH7_MemObjRelease(&sIt);` |
|        4 | 4166 | `				}else{` |
|      ! 0 | 4167 | `					bIterable = 0;` |
|        - | 4168 | `				}` |
|        - | 4169 | `			}` |
|       28 | 4170 | `		}else{` |
|        6 | 4171 | `			bIterable = 0;` |
|        - | 4172 | `		}` |
|       79 | 4173 | `		VmPopOperand(&pTos,1); /* Consume the iterable operand */` |
|       79 | 4174 | `		if( !bIterable ){` |
|        - | 4175 | `			/* Non-iterable source: throw a catchable Error (PHP 8.5), then` |
|        - | 4176 | `			 * funnel through the shared teardown/route path. */` |
|        6 | 4177 | `			rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 4178 | `				"Can use \"yield from\" only with arrays and Traversables",` |
|        - | 4179 | `				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);` |
|        6 | 4180 | `			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        6 | 4181 | `			goto yf_propagate;` |
|        - | 4182 | `		}` |
|       75 | 4183 | `		if( pCtxFrom->iDelegateState >= 2 ){` |
|        - | 4184 | `			/* rewind() the delegate (also starts a fresh generator) */` |
|       51 | 4185 | `			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 4186 | `				"rewind",sizeof("rewind")-1,0);` |
|       51 | 4187 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       23 | 4188 | `		}` |
|       40 | 4189 | `	}else{` |
|        - | 4190 | `		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a` |
|        - | 4191 | `		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()` |
|        - | 4192 | `		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)` |
|        - | 4193 | `		 * ignore send() and just advance with next(). */` |
|        - | 4194 | `#ifdef UNTRUST` |
|        - | 4195 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 4196 | `#endif` |
|      117 | 4197 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       68 | 4198 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|        - | 4199 | `			/* A pending Generator::throw() on the outer was parked on pInjected by the` |
|        - | 4200 | `			 * body-entry gate (which skips its own raise for state 3); forward it as a` |
|        - | 4201 | `			 * throw into the delegate, else forward the sent value (pTos, which` |
|        - | 4202 | `			 * VmResumeCtx copies into the inner's own stack). */` |
|       68 | 4203 | `			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;` |
|       68 | 4204 | `			pCtxFrom->pInjected = 0;` |
|       68 | 4205 | `			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       68 | 4206 | `				if( pInjFwd ){` |
|        - | 4207 | `					/* Borrowed ref: the outer's Generator::throw() holds it across this` |
|        - | 4208 | `					 * whole resume, so the inner inject path must not unref it. */` |
|        5 | 4209 | `					pInner->pCtx->pInjected = pInjFwd;` |
|        5 | 4210 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);` |
|        5 | 4211 | `					pInner->pCtx->pInjected = 0;` |
|        3 | 4212 | `				}else{` |
|       64 | 4213 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);` |
|        4 | 4214 | `				}` |
|       32 | 4215 | `			}else if( pInjFwd ){` |
|        - | 4216 | `				/* No live delegate to receive the throw (inner already finished/closed):` |
|        - | 4217 | `				 * raise it at the yield-from in the outer generator's own frame. */` |
|      ! 0 | 4218 | `				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);` |
|      ! 0 | 4219 | `				pTF->iFlags \|= VM_FRAME_THROW;` |
|      ! 0 | 4220 | `				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 4221 | `			}` |
|       68 | 4222 | `			PH7_MemObjRelease(pTos);` |
|       68 | 4223 | `			pTos--;` |
|       68 | 4224 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       32 | 4225 | `		}else{` |
|       53 | 4226 | `			PH7_MemObjRelease(pTos);` |
|       53 | 4227 | `			pTos--;` |
|       53 | 4228 | `			if( pCtxFrom->iDelegateState >= 2 ){` |
|       17 | 4229 | `				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 4230 | `					"next",sizeof("next")-1,0);` |
|       17 | 4231 | `				if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 4232 | `			}` |
|        - | 4233 | `		}` |
|        - | 4234 | `	}` |
|        - | 4235 | `	/* Fetch the current (key,value) of the delegate, or mark exhausted. */` |
|      177 | 4236 | `	if( pCtxFrom->iDelegateState == 1 ){` |
|       63 | 4237 | `		if( pCtxFrom->pDelegateNode == 0 ){` |
|       21 | 4238 | `			bExhausted = 1;` |
|       13 | 4239 | `		}else{` |
|       47 | 4240 | `			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);` |
|       47 | 4241 | `			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);` |
|        - | 4242 | `			/* Forward traversal follows pPrev (the hashmap's "reverse link",` |
|        - | 4243 | `			 * matching PH7_HashmapGetNextEntry). */` |
|       47 | 4244 | `			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;` |
|        - | 4245 | `		}` |
|       34 | 4246 | `	}else{` |
|      119 | 4247 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|        - | 4248 | `		ph7_value sValid;` |
|        - | 4249 | `		int isValid;` |
|      119 | 4250 | `		PH7_MemObjInit(pVm,&sValid);` |
|      119 | 4251 | `		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|      119 | 4252 | `		PH7_MemObjToBool(&sValid);` |
|      119 | 4253 | `		isValid = (sValid.x.iVal != 0);` |
|      119 | 4254 | `		PH7_MemObjRelease(&sValid);` |
|      119 | 4255 | `		if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      119 | 4256 | `		if( !isValid ){` |
|       28 | 4257 | `			bExhausted = 1;` |
|       16 | 4258 | `		}else{` |
|       95 | 4259 | `			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|       95 | 4260 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       95 | 4261 | `			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       95 | 4262 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        - | 4263 | `		}` |
|        - | 4264 | `	}` |
|      177 | 4265 | `	if( bExhausted ){` |
|        - | 4266 | `		/* Expression value: inner Generator's return value (state 3) or NULL. */` |
|        - | 4267 | `		ph7_value sResult;` |
|       45 | 4268 | `		PH7_MemObjInit(pVm,&sResult);` |
|       45 | 4269 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       23 | 4270 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|       23 | 4271 | `			if( pInner && pInner->pCtx ){` |
|       23 | 4272 | `				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);` |
|       10 | 4273 | `			}` |
|       10 | 4274 | `		}` |
|       45 | 4275 | `		PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       45 | 4276 | `		pCtxFrom->pDelegateNode = 0;` |
|       45 | 4277 | `		pCtxFrom->iDelegateState = 0;` |
|       45 | 4278 | `		pTos++;` |
|       45 | 4279 | `		PH7_MemObjStore(&sResult,pTos);` |
|       45 | 4280 | `		PH7_MemObjRelease(&sResult);` |
|       45 | 4281 | `		PH7_MemObjRelease(&sKey);` |
|       45 | 4282 | `		PH7_MemObjRelease(&sVal);` |
|       45 | 4283 | `		break; /* fall through to pc+1 with the result on the stack top */` |
|        - | 4284 | `	}` |
|        - | 4285 | `	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.` |
|        - | 4286 | `	 * The outer generator's implicit auto-key counter is NOT advanced by the` |
|        - | 4287 | ``	 * delegated keys — PHP keeps it independent across `yield from`, so a later`` |
|        - | 4288 | ``	 * plain `yield` continues from the outer's own counter. */`` |
|      137 | 4289 | `	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);` |
|      137 | 4290 | `	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);` |
|      137 | 4291 | `	PH7_MemObjRelease(&sKey);` |
|      137 | 4292 | `	PH7_MemObjRelease(&sVal);` |
|        - | 4293 | `	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */` |
|      137 | 4294 | `	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));` |
|      137 | 4295 | `	goto Suspend;` |
|        7 | 4296 | `yf_propagate:` |
|        - | 4297 | `	/* A delegate iterator method threw/aborted (or the source was non-iterable):` |
|        - | 4298 | `	 * tear down the delegation, then route via the shared dispatch macro. rcm is` |
|        - | 4299 | `	 * always PH7_EXCEPTION or PH7_ABORT here. */` |
|       18 | 4300 | `	PH7_MemObjRelease(&sKey);` |
|       18 | 4301 | `	PH7_MemObjRelease(&sVal);` |
|       18 | 4302 | `	PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       18 | 4303 | `	pCtxFrom->pDelegateNode = 0;` |
|       18 | 4304 | `	pCtxFrom->iDelegateState = 0;` |
|       18 | 4305 | `	PH7_DISPATCH_ENFORCE_RC(rcm)` |
|      ! 0 | 4306 | `	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */` |
|        - | 4307 | `}` |
|        - | 4308 | `/*` |
|        - | 4309 | ` * OP_CALL P1 * *` |
|        - | 4310 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - | 4311 | ` *  function on the stack.` |
|        - | 4312 | ` */` |
|  3328402 | 4313 | `case PH7_OP_CALL: {` |
|        - | 4314 | `	/* iP2 = hasSpread (compile-time). Count only THIS call's own unpack` |
|        - | 4315 | `	 * expansion (VmSpreadOwnExtra, derived from the captured runs on top of the` |
|        - | 4316 | `	 * stack) — an INNER spread-bearing call evaluated inside this argument list` |
|        - | 4317 | ``	 * (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not`` |
|        - | 4318 | `	 * be conflated, which a single shared accumulator could not express. */` |
|  6658339 | 4319 | `	sxi32 nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|        - | 4320 | `	ph7_value *pArg;` |
|  6658339 | 4321 | `	pArg = &pTos[-nCallArgs];` |
|        - | 4322 | `	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED` |
|        - | 4323 | `	 * arguments, and a spread expanding to !=1 element shifts the actual positions` |
|        - | 4324 | `	 * of any following compile-time named args. The effective per-actual-slot name` |
|        - | 4325 | `	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)` |
|        - | 4326 | `	 * is built PER PATH against that path's finalized arg base — a method call pops` |
|        - | 4327 | `	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch` |
|        - | 4328 | `	 * site rather than built once here. */` |
|        - | 4329 | `	VmCallArgMap sEffMap;` |
|  6658339 | 4330 | `	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;` |
|        - | 4331 | `	SyHashEntry *pEntry;` |
|        - | 4332 | `	SyString sName;` |
|        - | 4333 | `	/* A Closure object is callable: unwrap it to its underlying string callable so the` |
|        - | 4334 | `	 * dispatch below handles it (rather than treating it as a generic object and looking` |
|        - | 4335 | `	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure` |
|        - | 4336 | `	 * so a plain __invoke object skips the temp-value work entirely. */` |
|  6658339 | 4337 | `	if( VmValueIsClosure(pVm,pTos) ){` |
|        - | 4338 | `		ph7_value sCallable;` |
|     1541 | 4339 | `		PH7_MemObjInit(pVm,&sCallable);` |
|     1541 | 4340 | `		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){` |
|     1541 | 4341 | `			PH7_MemObjRelease(pTos);` |
|     1541 | 4342 | `			PH7_MemObjStore(&sCallable,pTos);` |
|      768 | 4343 | `		}` |
|     1541 | 4344 | `		PH7_MemObjRelease(&sCallable);` |
|      768 | 4345 | `	}` |
|        - | 4346 | `	/* Extract function name */` |
|  6658339 | 4347 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|   300193 | 4348 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 4349 | `			ph7_value sResult;` |
|        - | 4350 | `			sxi32 rcArr;` |
|        - | 4351 | `			{` |
|        - | 4352 | `				/* php validates the SHAPE of an array callable first: it must hold exactly` |
|        - | 4353 | `				 * two elements. PH7 handed any array to the dispatcher, which failed` |
|        - | 4354 | ``				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */`` |
|   100098 | 4355 | `				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - | 4356 | `				char zCbMsg[192];` |
|   100098 | 4357 | `				const char *zCbErr = 0;` |
|   100098 | 4358 | `				if( pCbMap && pCbMap->nEntry == 2 ){` |
|        - | 4359 | `					/* Shape is right; now check it actually RESOLVES. The shared dispatcher` |
|        - | 4360 | `					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for` |
|        - | 4361 | `					 * an unresolvable [class,method] pair -- silence a caller cannot detect --` |
|        - | 4362 | `					 * and its contract is relied on by call_user_func/usort, so the throw` |
|        - | 4363 | `					 * belongs here at the call site. */` |
|   100090 | 4364 | `					ph7_value *pCbCls = (ph7_value *)SySetAt(&pVm->aMemObj,pCbMap->pFirst->nValIdx);` |
|   100090 | 4365 | `					ph7_value *pCbMeth = (ph7_value *)SySetAt(&pVm->aMemObj,pCbMap->pFirst->pPrev->nValIdx);` |
|   100090 | 4366 | `					ph7_class *pCbClass = pCbCls ? PH7_VmExtractClassFromValue(&(*pVm),pCbCls) : 0;` |
|   100090 | 4367 | `					if( pCbClass == 0 ){` |
|      ! 0 | 4368 | `						SyBufferFormat(zCbMsg,sizeof(zCbMsg),"Class \"%.*s\" not found",` |
|      ! 0 | 4369 | `							pCbCls ? (int)SyBlobLength(&pCbCls->sBlob) : 0,` |
|      ! 0 | 4370 | `							pCbCls ? (const char *)SyBlobData(&pCbCls->sBlob) : "");` |
|      ! 0 | 4371 | `						zCbErr = zCbMsg;` |
|   100088 | 4372 | `					}else if( pCbMeth == 0 \|\| (pCbMeth->iFlags & MEMOBJ_STRING) == 0` |
|   100090 | 4373 | `						\|\| PH7_ClassExtractMethod(pCbClass,(const char *)SyBlobData(&pCbMeth->sBlob),` |
|   100088 | 4374 | `							SyBlobLength(&pCbMeth->sBlob)) == 0 ){` |
|       13 | 4375 | `						SyBufferFormat(zCbMsg,sizeof(zCbMsg),"Call to undefined method %z::%.*s()",` |
|        3 | 4376 | `							&pCbClass->sName,` |
|        6 | 4377 | `							pCbMeth ? (int)SyBlobLength(&pCbMeth->sBlob) : 0,` |
|        3 | 4378 | `							pCbMeth ? (const char *)SyBlobData(&pCbMeth->sBlob) : "");` |
|        7 | 4379 | `						zCbErr = zCbMsg;` |
|        3 | 4380 | `					}` |
|    50044 | 4381 | `				}` |
|   100098 | 4382 | `				if( pCbMap == 0 \|\| pCbMap->nEntry != 2 \|\| zCbErr ){` |
|        - | 4383 | `					sxi32 rcCb;` |
|       15 | 4384 | `					if( pInstr->iP2 ){` |
|      ! 0 | 4385 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 4386 | `					}` |
|       15 | 4387 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 4388 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 4389 | `					}` |
|       15 | 4390 | `					PH7_MemObjRelease(pTos);` |
|       15 | 4391 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       15 | 4392 | `					pTos->nIdx = SXU32_HIGH;` |
|       15 | 4393 | `					if( zCbErr == 0 ){` |
|        9 | 4394 | `						zCbErr = "Array callback must have exactly two elements";` |
|        4 | 4395 | `					}` |
|       15 | 4396 | `					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));` |
|       15 | 4397 | `					if( rcCb == SXERR_ABORT ){ goto Abort; }` |
|       15 | 4398 | `					rc = rcCb;` |
|        - | 4399 | `					/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 4400 | `					 * (SXRET_OK + recorded resume) FELL THROUGH and kept dispatching` |
|        - | 4401 | `					 * the malformed callable inside the try. Route like OP_THROW. */` |
|       25 | 4402 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4403 | `				}` |
|        - | 4404 | `			}` |
|        - | 4405 | `			/* Build the effective spread-key map (and consume this call's runs)` |
|        - | 4406 | `			 * against this path's arg base (the array-callable slot isn't popped). */` |
|   150125 | 4407 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100082 | 4408 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 4409 | `			/* D1: an array callable dispatches through the shared helper below, which does not` |
|        - | 4410 | `			 * expose the target's per-parameter by-ref flags here. This path over-vivified every` |
|        - | 4411 | ``			 * plain-var argument before D1 (so `[$o,'m'](&$x)` out-params worked); preserve that`` |
|        - | 4412 | `			 * by materializing every deferred arg as by-ref. Refining these to precise by-value` |
|        - | 4413 | `			 * semantics is a later slice. */` |
|        - | 4414 | `			{` |
|   100084 | 4415 | `				sxi32 rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,/*bAllByRef*/1,0);` |
|   100084 | 4416 | `				if( rcDA == PH7_ABORT ){` |
|      ! 0 | 4417 | `					goto Abort;` |
|   100084 | 4418 | `				}else if( rcDA == PH7_EXCEPTION ){` |
|      ! 0 | 4419 | `					goto Exception;` |
|        - | 4420 | `				}` |
|        - | 4421 | `			}` |
|   100084 | 4422 | `			SySetReset(&aArg);` |
|   100162 | 4423 | `			while( pArg < pTos ){` |
|       79 | 4424 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       79 | 4425 | `				pArg++;` |
|        1 | 4426 | `			}` |
|   100084 | 4427 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 4428 | `			/* May be a class instance and it's static method. Forward this call's named-arg map` |
|        - | 4429 | ``			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —`` |
|        - | 4430 | `			 * mirroring the __invoke-object branch below. */` |
|   100084 | 4431 | `			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|   100084 | 4432 | `			SySetReset(&aArg);` |
|        - | 4433 | `			/* Pop given arguments */` |
|   100084 | 4434 | `			if( nCallArgs > 0 ){` |
|       63 | 4435 | `				VmPopOperand(&pTos,nCallArgs);` |
|       31 | 4436 | `			}` |
|   100084 | 4437 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 | 4438 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 4439 | `				goto Abort;` |
|        - | 4440 | `			}` |
|   100084 | 4441 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - | 4442 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - | 4443 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - | 4444 | `				sxi32 iResumePc;` |
|   100004 | 4445 | `				PH7_MemObjRelease(&sResult);` |
|   100004 | 4446 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100004 | 4447 | `					PH7_MemObjRelease(pTos);` |
|        - | 4448 | ``					/* Drain the abandoned outer-expression operands (`1 + $cb()`)`` |
|        - | 4449 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 4450 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300006 | 4451 | `					PH7_RESUME_DRAIN()` |
|   100004 | 4452 | `					pc = iResumePc;` |
|   100004 | 4453 | `					break;` |
|        - | 4454 | `				}` |
|      ! 0 | 4455 | `				goto Exception;` |
|        - | 4456 | `			}` |
|        - | 4457 | `			/* Copy result */` |
|       81 | 4458 | `			PH7_MemObjStore(&sResult,pTos);` |
|       81 | 4459 | `			PH7_MemObjRelease(&sResult);` |
|   200137 | 4460 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|   200089 | 4461 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - | 4462 | `			ph7_value sResult;` |
|        - | 4463 | `			sxi32 rcInv;` |
|        - | 4464 | `			/* __invoke object callable: the object slot isn't popped, so pArg is` |
|        - | 4465 | `			 * already this call's arg base — build the map + consume the runs. */` |
|   300132 | 4466 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   200086 | 4467 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 4468 | `			/* D1: like the array-callable path above, __invoke dispatches through a shared` |
|        - | 4469 | `			 * helper that hides the target's by-ref flags here. Preserve the pre-D1` |
|        - | 4470 | ``			 * over-vivification (so `$o(&$x)` out-params keep working) by materializing`` |
|        - | 4471 | `			 * every deferred arg as by-ref. */` |
|        - | 4472 | `			{` |
|   200089 | 4473 | `				sxi32 rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,/*bAllByRef*/1,0);` |
|   200089 | 4474 | `				if( rcDA == PH7_ABORT ){` |
|      ! 0 | 4475 | `					goto Abort;` |
|   200089 | 4476 | `				}else if( rcDA == PH7_EXCEPTION ){` |
|      ! 0 | 4477 | `					goto Exception;` |
|        - | 4478 | `				}` |
|        - | 4479 | `			}` |
|   200089 | 4480 | `			SySetReset(&aArg);` |
|   200207 | 4481 | `			while( pArg < pTos ){` |
|      120 | 4482 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      120 | 4483 | `				pArg++;` |
|        2 | 4484 | `			}` |
|   200089 | 4485 | `			PH7_MemObjInit(pVm,&sResult);` |
|   300132 | 4486 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|   200086 | 4487 | `				(int)SySetUsed(&aArg),` |
|   200086 | 4488 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - | 4489 | `				&sResult,` |
|   100043 | 4490 | `				pEffCallMap);` |
|   200089 | 4491 | `			SySetReset(&aArg);` |
|        - | 4492 | `			/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable` |
|        - | 4493 | `			 * slot itself (it pops top-down and the callable IS pTos), which for a` |
|        - | 4494 | `			 * temporary like (new Plain())(...) holds the only reference — popping it` |
|        - | 4495 | `			 * would free pThis before VmRaiseNotCallable reads its class name below.` |
|        - | 4496 | `			 * Only the not-callable branch dereferences pThis afterwards, so pin just` |
|        - | 4497 | `			 * for that case; the matching PH7_ClassInstanceUnref drops it (destroying` |
|        - | 4498 | `			 * the temporary). The other branches let the pop free the temp as before. */` |
|   200089 | 4499 | `			if( rcInv == SXERR_INVALID ){` |
|   100016 | 4500 | `				pThis->iRef++;` |
|    50007 | 4501 | `			}` |
|   200089 | 4502 | `			if( nCallArgs > 0 ){` |
|       78 | 4503 | `				VmPopOperand(&pTos,nCallArgs);` |
|       38 | 4504 | `			}` |
|   200089 | 4505 | `			if( rcInv == SXERR_INVALID ){` |
|        - | 4506 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - | 4507 | `				 * sResult was already released by VmCallObjectInvoke. */` |
|   100016 | 4508 | `				PH7_MemObjRelease(pTos);` |
|   100016 | 4509 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|   100016 | 4510 | `				PH7_ClassInstanceUnref(pThis);` |
|   100016 | 4511 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4512 | `					goto Abort;` |
|        - | 4513 | `				}` |
|        - | 4514 | `				{` |
|        - | 4515 | `					sxi32 iRp;` |
|   100016 | 4516 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|        - | 4517 | `						/* Drain the abandoned outer-expression operands` |
|        - | 4518 | ``						 * (`1 + $plainObj()`) to the try's base — one leaked`` |
|        - | 4519 | `						 * slot per caught throw otherwise. */` |
|   300030 | 4520 | `						PH7_RESUME_DRAIN()` |
|   100016 | 4521 | `						pc = iRp;` |
|   100016 | 4522 | `						break;` |
|        - | 4523 | `					}` |
|        - | 4524 | `				}` |
|      ! 0 | 4525 | `				goto Exception;` |
|        - | 4526 | `			}` |
|   100075 | 4527 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 | 4528 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 4529 | `				goto Abort;` |
|        - | 4530 | `			}` |
|   100075 | 4531 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - | 4532 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - | 4533 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - | 4534 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - | 4535 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - | 4536 | `				sxi32 iResumePc;` |
|   100008 | 4537 | `				PH7_MemObjRelease(&sResult);` |
|   100008 | 4538 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100006 | 4539 | `					PH7_MemObjRelease(pTos);` |
|        - | 4540 | ``					/* Drain the abandoned outer-expression operands (`1 + $inv()`)`` |
|        - | 4541 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 4542 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300010 | 4543 | `					PH7_RESUME_DRAIN()` |
|   100006 | 4544 | `					pc = iResumePc;` |
|   100006 | 4545 | `					break;` |
|        - | 4546 | `				}` |
|        3 | 4547 | `				goto Exception;` |
|        - | 4548 | `			}` |
|       68 | 4549 | `			PH7_MemObjStore(&sResult,pTos);` |
|       68 | 4550 | `			PH7_MemObjRelease(&sResult);` |
|       35 | 4551 | `		}else{` |
|        - | 4552 | `			/* php: calling a non-callable is a catchable Error naming the type` |
|        - | 4553 | `			 * ("Value of type int is not callable"), or -- for an array -- the shape it` |
|        - | 4554 | `			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so` |
|        - | 4555 | ``			 * `$x()` on a number quietly evaluated to nothing. */`` |
|        - | 4556 | `			sxi32 rcNc;` |
|        - | 4557 | `			char zMsg[128];` |
|        9 | 4558 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 4559 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");` |
|      ! 0 | 4560 | `			}else{` |
|       13 | 4561 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",` |
|        4 | 4562 | `					VmArithTypeName(pTos));` |
|        - | 4563 | `			}` |
|        - | 4564 | `			/* Consume this call's captured spread runs — a non-callable target` |
|        - | 4565 | `			 * (int/float/bool/null) reaches no dispatch build site. */` |
|        9 | 4566 | `			if( pInstr->iP2 ){` |
|      ! 0 | 4567 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 4568 | `			}` |
|        - | 4569 | `			/* Pop given arguments */` |
|        9 | 4570 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 4571 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 4572 | `			}` |
|        - | 4573 | `			/* Settle the call's result slot BEFORE throwing. */` |
|        9 | 4574 | `			PH7_MemObjRelease(pTos);` |
|        9 | 4575 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        9 | 4576 | `			pTos->nIdx = SXU32_HIGH;` |
|        9 | 4577 | `			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|        9 | 4578 | `			if( rcNc == SXERR_ABORT ){ goto Abort; }` |
|        9 | 4579 | `			rc = rcNc;` |
|        - | 4580 | `			/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 4581 | `			 * (SXRET_OK + recorded resume) FELL THROUGH and resumed the try body` |
|        - | 4582 | `			 * right after the failed call. Route like OP_THROW. */` |
|       15 | 4583 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4584 | `		}` |
|      148 | 4585 | `		break;` |
|        - | 4586 | `	}` |
|  6358149 | 4587 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 4588 | `	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just` |
|        - | 4589 | `	 * anchors it to the global namespace — strip it before resolving so a` |
|        - | 4590 | ``	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */`` |
|  6358149 | 4591 | `	if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|       13 | 4592 | `		sName.zString++;` |
|       13 | 4593 | `		sName.nByte--;` |
|        6 | 4594 | `	}` |
|        - | 4595 | `	/* Check for a compiled function first.` |
|        - | 4596 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 4597 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|  6358149 | 4598 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 4599 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - | 4600 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - | 4601 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - | 4602 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - | 4603 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - | 4604 | `	{` |
|  6358149 | 4605 | `	VmCallArgMap *pCallMap = pEffCallMap;` |
|  6358149 | 4606 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - | 4607 | `		const char *zFunc;` |
|        - | 4608 | `		const char *zEnd;` |
|        - | 4609 | `		const char *z;` |
|        - | 4610 | `		SyString sGlobal;` |
|       32 | 4611 | `		zFunc = sName.zString;` |
|       32 | 4612 | `		zEnd  = zFunc + sName.nByte;` |
|       32 | 4613 | `		z = zEnd;` |
|        - | 4614 | `		/* Find last namespace separator */` |
|      286 | 4615 | `		while( z > zFunc ){` |
|      286 | 4616 | `			if( z[-1] == '\\' ){` |
|       32 | 4617 | `				break;` |
|        - | 4618 | `			}` |
|      258 | 4619 | `			z--;` |
|        4 | 4620 | `		}` |
|       32 | 4621 | `		if( z > zFunc && z < zEnd ){` |
|        - | 4622 | `			/* Retry lookup using the unqualified/global function name */` |
|       32 | 4623 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       32 | 4624 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       14 | 4625 | `		}` |
|       14 | 4626 | `	}` |
|        - | 4627 | `	} /* end VmCallArgMap namespace scope */` |
|  6358149 | 4628 | `	if( pEntry ){` |
|        - | 4629 | `		ph7_vm_func_arg *aFormalArg;` |
|        - | 4630 | `		ph7_class_instance *pThis;` |
|        - | 4631 | `		ph7_value *pFrameStack;` |
|        - | 4632 | `		ph7_vm_func *pVmFunc;` |
|        - | 4633 | `		ph7_class *pSelf;` |
|        - | 4634 | `		ph7_class *pSelfHint;` |
|        - | 4635 | `		VmFrame *pFrame;` |
|        - | 4636 | `		ph7_value *pObj;` |
|        - | 4637 | `		VmSlot sArg;` |
|        - | 4638 | `		sxu32 n;` |
|  2181006 | 4639 | `		int bClosureThis = 0;` |
|  2181006 | 4640 | `		ph7_class *pClosureScope = 0;` |
|        - | 4641 | `		/* initialize fields */` |
|  2181006 | 4642 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|  2181006 | 4643 | `		pThis = 0;` |
|  2181006 | 4644 | `		pSelf = 0;` |
|        - | 4645 | `		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it` |
|        - | 4646 | `		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever` |
|        - | 4647 | `		 * set for a bound plain closure, which dispatches as a function, so the method branch below` |
|        - | 4648 | `		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */` |
|  2181006 | 4649 | `		if( pVm->pClosureThis ){` |
|       33 | 4650 | `			pThis = pVm->pClosureThis;` |
|       33 | 4651 | `			pVm->pClosureThis = 0;` |
|       33 | 4652 | `			bClosureThis = 1;` |
|       16 | 4653 | `		}` |
|  2181006 | 4654 | `		if( pVm->pClosureScope ){` |
|        - | 4655 | `			/* May ride alongside a bound $this, or stand alone for a` |
|        - | 4656 | ``			 * scope-only rebind (`bindTo(null, Scope::class)`). */`` |
|       29 | 4657 | `			pClosureScope = pVm->pClosureScope;` |
|       29 | 4658 | `			pVm->pClosureScope = 0;` |
|       14 | 4659 | `		}` |
|  2181006 | 4660 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - | 4661 | `			ph7_class_method *pMeth;` |
|        - | 4662 | `			/* Class method call */` |
|  1971387 | 4663 | `			ph7_value *pTarget = &pTos[-1];` |
|  1971387 | 4664 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - | 4665 | `				/* Extract the 'this' pointer */` |
|  1971387 | 4666 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - | 4667 | `					/* Instance already loaded */` |
|  1870331 | 4668 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|  1870331 | 4669 | `					pThis->iRef++;` |
|  1870331 | 4670 | `					pSelf = pThis->pClass;` |
|   935163 | 4671 | `				}` |
|  1971387 | 4672 | `				if( pSelf == 0 ){` |
|   101061 | 4673 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - | 4674 | `						/* "Late Static Binding" class name */` |
|     1496 | 4675 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|      497 | 4676 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|      497 | 4677 | `					}` |
|   101061 | 4678 | `					if( pSelf == 0 ){` |
|   100065 | 4679 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|    50031 | 4680 | `					}` |
|    50528 | 4681 | `				}` |
|  1971387 | 4682 | `				if( pThis == 0  ){` |
|   101061 | 4683 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|   101061 | 4684 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   101061 | 4685 | `					if( pFrameLocal->pParent ){` |
|        - | 4686 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|      713 | 4687 | `						pThis = pFrameLocal->pThis;` |
|      713 | 4688 | `						if( pThis ){` |
|      182 | 4689 | `							pThis->iRef++;` |
|       90 | 4690 | `						}` |
|      354 | 4691 | `					}` |
|    50528 | 4692 | `				}` |
|  1971387 | 4693 | `				VmPopOperand(&pTos,1);` |
|  1971387 | 4694 | `				PH7_MemObjRelease(pTos);` |
|        - | 4695 | `				/* Synchronize pointers. The method-name slot popped above sat BETWEEN` |
|        - | 4696 | `				 * this call's arguments and the (already-removed) target — so only now` |
|        - | 4697 | `				 * is pTos one past the last actual argument. Re-derive the unpack` |
|        - | 4698 | `				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs` |
|        - | 4699 | `				 * from run ends, which the extra target slot would otherwise offset,` |
|        - | 4700 | ``				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */`` |
|  1971387 | 4701 | `				nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);` |
|  1971387 | 4702 | `				pArg = &pTos[-nCallArgs];` |
|        - | 4703 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - | 4704 | `				 * user have already computed the random generated unique class method name` |
|        - | 4705 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - | 4706 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - | 4707 | `				 */` |
|  1971387 | 4708 | `				while( pArg < pStack ){` |
|      ! 0 | 4709 | `					pArg++;` |
|      ! 0 | 4710 | `				}` |
|  1971387 | 4711 | `				if( pSelf && pVm->bReflectBypass ){` |
|        - | 4712 | `					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection` |
|        - | 4713 | `					 * ignores visibility. Consume-once so nested calls made by the` |
|        - | 4714 | `					 * invoked body are checked normally. */` |
|       11 | 4715 | `					pVm->bReflectBypass = 0;` |
|        6 | 4716 | `				}else` |
|  1971377 | 4717 | `				if( pSelf ){ /* Paranoid edition */` |
|        - | 4718 | `					/* Check if the call is allowed. php binds non-public method` |
|        - | 4719 | `					 * access by the DECLARING class (pVmFunc->pUserData — the class` |
|        - | 4720 | `					 * the callee was compiled in), NOT the instance's class: an` |
|        - | 4721 | `					 * inherited base method calling $this->priv() on a child` |
|        - | 4722 | `					 * instance passes, a child's private SHADOW doesn't hijack the` |
|        - | 4723 | `					 * check for a parent callee, and the denial message names the` |
|        - | 4724 | `					 * declaring class like php. */` |
|  1971377 | 4725 | `					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|  1971377 | 4726 | `					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|  1971377 | 4727 | `					if( pMeth == 0 && pDeclClass != pSelf ){` |
|      ! 0 | 4728 | `						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      ! 0 | 4729 | `					}` |
|  1971377 | 4730 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     2647 | 4731 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pDeclClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - | 4732 | `							/* php throws a CATCHABLE Error here. The old code merely PRINTED an` |
|        - | 4733 | ``							 * uncaught-exception report and aborted, so `try { $o->priv(); }`` |
|        - | 4734 | ``							 * catch (Error $e)` never caught it and the script died. */`` |
|        - | 4735 | `							char zMsg[256];` |
|        - | 4736 | `							sxi32 rcVis;` |
|       12 | 4737 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       17 | 4738 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|       10 | 4739 | `								zVis,(int)pDeclClass->sName.nByte,pDeclClass->sName.zString,` |
|       10 | 4740 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - | 4741 | `							/* Consume this call's captured spread runs — this visibility` |
|        - | 4742 | `							 * error exits before the pVmFunc build below. */` |
|       12 | 4743 | `							if( pInstr->iP2 ){` |
|      ! 0 | 4744 | `								VmSpreadConsume(pVm);` |
|      ! 0 | 4745 | `							}` |
|        - | 4746 | `							/* Pop given arguments, and leave the call's NULL result behind. */` |
|       12 | 4747 | `							if( nCallArgs > 0 ){` |
|      ! 0 | 4748 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 4749 | `							}` |
|       12 | 4750 | `							PH7_MemObjRelease(pTos);` |
|       12 | 4751 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|       12 | 4752 | `							pTos->nIdx = SXU32_HIGH;` |
|       12 | 4753 | `							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       12 | 4754 | `							if( rcVis == SXERR_ABORT ){ goto Abort; }` |
|       12 | 4755 | `							rc = rcVis;` |
|        - | 4756 | `							/* ENFORCE_RC never consulted pResumeFrame, so an in-place` |
|        - | 4757 | `							 * catch (SXRET_OK + recorded resume) FELL THROUGH and` |
|        - | 4758 | `							 * dispatched the DENIED method anyway. Route like OP_THROW. */` |
|       16 | 4759 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4760 | `						}` |
|     1316 | 4761 | `					}` |
|   985681 | 4762 | `				}` |
|   985686 | 4763 | `			}` |
|   985686 | 4764 | `		}` |
|        - | 4765 | `		/* pArg is now finalized for every pVmFunc callee (a method call popped its` |
|        - | 4766 | `		 * method-name slot above; functions/closures/generators keep the top base).` |
|        - | 4767 | `		 * Build the PHP 8.1 effective spread-key map here so the generator and the` |
|        - | 4768 | `		 * install path below both see it — and so this call's captured runs are` |
|        - | 4769 | `		 * consumed exactly once, against the correct base. */` |
|  3271654 | 4770 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  2180991 | 4771 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 4772 | `		/* Check the PHP call-depth cap (the sole site — BYTECODE.md stage 5).` |
|        - | 4773 | `		 * Default is unbounded (heap-bound recursion, decoupled from the C stack` |
|        - | 4774 | `		 * by the stage-2 trampoline); the C stack is guarded separately by` |
|        - | 4775 | `		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and` |
|        - | 4776 | `		 * then raises a clean non-catchable fatal (was: silently set NULL and` |
|        - | 4777 | `		 * continue) and halts. */` |
|  2180996 | 4778 | `		if( VmRecursionExceeded(pVm) ){` |
|        - | 4779 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - | 4780 | `			 * which walks the whole operand stack — don't release them here. */` |
|        3 | 4781 | `			VmRecursionFatal(&(*pVm));` |
|        3 | 4782 | `			goto Abort;` |
|        - | 4783 | `		}` |
|  2180994 | 4784 | `		if( pVmFunc->pNextName ){` |
|        - | 4785 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      268 | 4786 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|      132 | 4787 | `		}` |
|        - | 4788 | ``		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's`` |
|        - | 4789 | ``		 * `Class::method`). Computed after overload resolution so it reflects the selected method.`` |
|        - | 4790 | ``		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed`` |
|        - | 4791 | `		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method` |
|        - | 4792 | ``		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the`` |
|        - | 4793 | `		 * USING class; we don't carry the using class on the shared struct, so fall back to the` |
|        - | 4794 | `		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for` |
|        - | 4795 | `		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */` |
|  2180994 | 4796 | `		pSelfHint = pSelf;` |
|  2180994 | 4797 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|  1971377 | 4798 | `			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;` |
|  1971377 | 4799 | `			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|  1970275 | 4800 | `				pSelfHint = pDecl;` |
|   985135 | 4801 | `			}` |
|   985686 | 4802 | `		}` |
|  2180994 | 4803 | `		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 4804 | `			/* Push the closure's called-class as this frame's LSB class so` |
|        - | 4805 | ``			 * `static::` inside the body resolves like php. An explicit`` |
|        - | 4806 | `			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use` |
|        - | 4807 | `			 * the class captured at the closure's creation site. self::/parent::` |
|        - | 4808 | `			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done` |
|        - | 4809 | ``			 * after pSelfHint so a `self`-typed param is unaffected. */`` |
|     2395 | 4810 | `			if( pClosureScope ){` |
|       29 | 4811 | `				pSelf = pClosureScope;` |
|     2381 | 4812 | `			}else if( pVmFunc->pLsbClass ){` |
|       87 | 4813 | `				pSelf = (ph7_class *)pVmFunc->pLsbClass;` |
|       41 | 4814 | `			}` |
|     1195 | 4815 | `		}` |
|  2180994 | 4816 | `		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){` |
|        - | 4817 | `			/* php 8.4 #[\Deprecated] runtime notice — once per call, before` |
|        - | 4818 | `			 * execution (generators: at the g(...) call site, like php). */` |
|      173 | 4819 | `			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,` |
|      112 | 4820 | `				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);` |
|       56 | 4821 | `		}` |
|        - | 4822 | `		/* D1: resolve deferred plain-var arguments against this callee's formal parameters` |
|        - | 4823 | `		 * now — BEFORE the generator split and VmEnterFrame, while pVm->pFrame is still the` |
|        - | 4824 | `		 * caller. pVmFunc is final here (post-overload). Covers plain functions, methods,` |
|        - | 4825 | `		 * closures, dynamic-name calls and generators uniformly. */` |
|        - | 4826 | `		{` |
|  3271651 | 4827 | `			sxi32 rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|  2180989 | 4828 | `				(ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs),SySetUsed(&pVmFunc->aArgs),` |
|        - | 4829 | `				0,0,0);` |
|  2180994 | 4830 | `			if( rcDA == PH7_ABORT ){` |
|      ! 0 | 4831 | `				goto Abort;` |
|  2180994 | 4832 | `			}else if( rcDA == PH7_EXCEPTION ){` |
|      ! 0 | 4833 | `				goto Exception;` |
|        - | 4834 | `			}` |
|        - | 4835 | `		}` |
|  2180994 | 4836 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - | 4837 | `			/* Generator function: return a Generator object instead of executing */` |
|        - | 4838 | `			ph7_exec_ctx *pExecCtx;` |
|        - | 4839 | `			ph7_generator *pGenerator;` |
|        - | 4840 | `			ph7_class_instance *pGenObj;` |
|        - | 4841 | `			ph7_value *pCtxAttr;` |
|        - | 4842 | `			SyString sAttrName;` |
|        - | 4843 | `			ph7_value **apCallArgs;` |
|        - | 4844 | `			int nGenArgs, iArg;` |
|        - | 4845 | `			/* Collect arguments from the operand stack */` |
|      369 | 4846 | `			nGenArgs = (int)(pTos - pArg);` |
|      369 | 4847 | `			apCallArgs = 0;` |
|      369 | 4848 | `			if( nGenArgs > 0 ){` |
|      118 | 4849 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       38 | 4850 | `					nGenArgs * sizeof(ph7_value *));` |
|       80 | 4851 | `				if( apCallArgs == 0 ){` |
|        - | 4852 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 | 4853 | `					nGenArgs = 0;` |
|      ! 0 | 4854 | `				}else{` |
|       80 | 4855 | `					VmCallArgMap *pGenMap = pEffCallMap;` |
|       80 | 4856 | `					int didReorder = 0;` |
|       80 | 4857 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - | 4858 | `						/* Named-argument reordering for generator */` |
|       10 | 4859 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|       10 | 4860 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|       10 | 4861 | `						sxu32 nNV = nF;` |
|       10 | 4862 | `						sxi32 iVIdx = -1;` |
|        - | 4863 | `						sxi32 *aGSlot;` |
|        - | 4864 | `						sxu8 *aGUsed;` |
|        - | 4865 | `						sxu32 gi;` |
|       22 | 4866 | `						for( gi = 0; gi < nF; gi++ ){` |
|       14 | 4867 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        8 | 4868 | `						}` |
|       14 | 4869 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        8 | 4870 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|       10 | 4871 | `						if( aGSlot ){` |
|       10 | 4872 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|       14 | 4873 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        4 | 4874 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|       10 | 4875 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 4876 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 4877 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4878 | `								goto Abort;` |
|        - | 4879 | `							}` |
|       10 | 4880 | `							if( rc == PH7_EXCEPTION ){` |
|        - | 4881 | `								/* A named-argument error is php's catchable \Error.` |
|        - | 4882 | `								 * No callee frame exists yet on this branch (the` |
|        - | 4883 | `								 * generator body never runs and VmEnterFrame is` |
|        - | 4884 | `								 * further down), so route it like the other` |
|        - | 4885 | `								 * pre-frame OP_CALL throws: drop the args + the` |
|        - | 4886 | `								 * function-name slot and land the enclosing try. */` |
|        3 | 4887 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        3 | 4888 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      185 | 4889 | `								PH7_INLINE_RESUME_BREAK()` |
|        3 | 4890 | `								VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 4891 | `								{` |
|        - | 4892 | `									sxi32 iRpN;` |
|        3 | 4893 | `									if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){` |
|        3 | 4894 | `										pc = iRpN;` |
|        3 | 4895 | `										break;` |
|        - | 4896 | `									}` |
|        - | 4897 | `								}` |
|      ! 0 | 4898 | `								goto Exception;` |
|        - | 4899 | `							}` |
|        8 | 4900 | `							if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){` |
|        - | 4901 | `								/* php's named-hole ArgumentCountError, checked BEFORE` |
|        - | 4902 | `								 * hole compaction: compacting first would report the` |
|        - | 4903 | `								 * positional wording with a fictitious count (g(b:2)` |
|        - | 4904 | ``								 * must be `g(): Argument #1 ($a) not passed`, not`` |
|        - | 4905 | `								 * "1 passed"). Implicit-required watermark, like the` |
|        - | 4906 | `								 * plain-call named path; a hole with NOTHING filled` |
|        - | 4907 | `								 * above it keeps php's count wording — fall through` |
|        - | 4908 | `								 * to VmFiberSetupFrame's check (the compacted count` |
|        - | 4909 | `								 * equals php's num_args there). */` |
|        8 | 4910 | `								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;` |
|        8 | 4911 | `								sxi32 iHole = -1;` |
|        8 | 4912 | `								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);` |
|       18 | 4913 | `								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){` |
|       12 | 4914 | `									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){` |
|       10 | 4915 | `										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);` |
|        4 | 4916 | `									}` |
|        7 | 4917 | `								}` |
|       18 | 4918 | `								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){` |
|        - | 4919 | `									sxu32 gj;` |
|       12 | 4920 | `									int bFound = 0;` |
|       16 | 4921 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       16 | 4922 | `										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }` |
|        3 | 4923 | `									}` |
|       12 | 4924 | `									if( !bFound && gHole + 1 <= nMaxFilledG ){` |
|      ! 0 | 4925 | `										iHole = (sxi32)gHole;` |
|      ! 0 | 4926 | `									}` |
|        7 | 4927 | `								}` |
|        8 | 4928 | `								if( iHole >= 0 ){` |
|      ! 0 | 4929 | `									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 4930 | `										(sxu32)iHole+1,&aFA[iHole].sName);` |
|      ! 0 | 4931 | `									SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 4932 | `									SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4933 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 4934 | `										goto Abort;` |
|        - | 4935 | `									}` |
|        - | 4936 | `									/* Route like the VmFiberSetupFrame throw below */` |
|      ! 0 | 4937 | `									PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 4938 | `									VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 4939 | `									{` |
|        - | 4940 | `										sxi32 iRpH;` |
|      ! 0 | 4941 | `										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 4942 | `											pc = iRpH;` |
|      ! 0 | 4943 | `											break;` |
|        - | 4944 | `										}` |
|        - | 4945 | `									}` |
|      ! 0 | 4946 | `									goto Exception;` |
|        - | 4947 | `								}` |
|        3 | 4948 | `							}` |
|        - | 4949 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - | 4950 | `							 * append overflow (variadic / positional beyond` |
|        - | 4951 | `							 * formals) so downstream sees every argument. */` |
|        - | 4952 | `							{` |
|        8 | 4953 | `								int nOut = 0;` |
|       18 | 4954 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - | 4955 | `									sxu32 gj;` |
|       16 | 4956 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       16 | 4957 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|       12 | 4958 | `											apCallArgs[nOut++] = &pArg[gj];` |
|       12 | 4959 | `											break;` |
|        - | 4960 | `										}` |
|        3 | 4961 | `									}` |
|        7 | 4962 | `								}` |
|       18 | 4963 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|       12 | 4964 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 | 4965 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 | 4966 | `									}` |
|        7 | 4967 | `								}` |
|        8 | 4968 | `								nGenArgs = nOut;` |
|        - | 4969 | `							}` |
|        8 | 4970 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        8 | 4971 | `							didReorder = 1;` |
|        3 | 4972 | `						}` |
|        - | 4973 | `						/* If aGSlot allocation failed, fall through to` |
|        - | 4974 | `						 * positional fill below — preserves arg order rather` |
|        - | 4975 | `						 * than passing an uninitialized apCallArgs. */` |
|        3 | 4976 | `					}` |
|       78 | 4977 | `					if( !didReorder ){` |
|      146 | 4978 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|       78 | 4979 | `							apCallArgs[iArg] = &pArg[iArg];` |
|       41 | 4980 | `						}` |
|       34 | 4981 | `					}` |
|        - | 4982 | `				}` |
|       37 | 4983 | `			}` |
|        - | 4984 | `			/* Create execution context and generator wrapper */` |
|      367 | 4985 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|      367 | 4986 | `			if( pExecCtx == 0 ){` |
|      ! 0 | 4987 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4988 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 4989 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 4990 | `				break;` |
|        - | 4991 | `			}` |
|      367 | 4992 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|      367 | 4993 | `			if( pGenerator == 0 ){` |
|      ! 0 | 4994 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 | 4995 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 4996 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 4997 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 4998 | `				break;` |
|        - | 4999 | `			}` |
|        - | 5000 | `			/* Set up the frame with arguments, closure env, $this */` |
|      367 | 5001 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|      367 | 5002 | `			pVm->pFrame = pExecCtx->pFrame;` |
|      373 | 5003 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,` |
|      184 | 5004 | `				(pEffCallMap && pEffCallMap->bStrict) ? 1 : 0, pSelfHint,` |
|        - | 5005 | `				TRUE/*generator: the g(...) call site is in the message*/);` |
|      367 | 5006 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|      367 | 5007 | `			pExecCtx->pFrame->pParent = 0;` |
|      367 | 5008 | `			if( apCallArgs ){` |
|       78 | 5009 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       37 | 5010 | `			}` |
|      367 | 5011 | `			if( rc != SXRET_OK ){` |
|       18 | 5012 | `				VmReleaseGenerator(pVm, pGenerator);` |
|       18 | 5013 | `				if( pThis ){` |
|        3 | 5014 | `					PH7_ClassInstanceUnref(pThis);` |
|        1 | 5015 | `				}` |
|       18 | 5016 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 5017 | `					goto Abort;` |
|        - | 5018 | `				}` |
|       18 | 5019 | `				if( rc == PH7_EXCEPTION ){` |
|        - | 5020 | `					/* A declared-type TypeError thrown while binding the` |
|        - | 5021 | `					 * generator's arguments (php binds + type-checks eagerly at` |
|        - | 5022 | `					 * the g(...) call site, before any resume — band A #2). If` |
|        - | 5023 | `					 * an inline try THIS exec owns caught it, land at its` |
|        - | 5024 | `					 * redirect (the drain subsumes the operand pops); else pop` |
|        - | 5025 | `					 * the args + function name and route like the other` |
|        - | 5026 | `					 * OP_CALL throw paths. */` |
|       22 | 5027 | `					PH7_INLINE_RESUME_BREAK()` |
|       16 | 5028 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 5029 | `					{` |
|        - | 5030 | `						sxi32 iRpG;` |
|       16 | 5031 | `						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){` |
|       16 | 5032 | `							pc = iRpG;` |
|       16 | 5033 | `							break;` |
|        - | 5034 | `						}` |
|        - | 5035 | `					}` |
|      ! 0 | 5036 | `					goto Exception;` |
|        - | 5037 | `				}` |
|      ! 0 | 5038 | `				break;` |
|        - | 5039 | `			}` |
|        - | 5040 | `			/* Create Generator class instance */` |
|      351 | 5041 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|      351 | 5042 | `			if( pGenObj == 0 ){` |
|      ! 0 | 5043 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 | 5044 | `				break;` |
|        - | 5045 | `			}` |
|        - | 5046 | `			/* Store generator in __ctx attribute */` |
|      351 | 5047 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      351 | 5048 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|      351 | 5049 | `			if( pCtxAttr ){` |
|      351 | 5050 | `				pCtxAttr->x.pOther = pGenerator;` |
|      351 | 5051 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      173 | 5052 | `			}` |
|        - | 5053 | `			/* Pop args and function name, push Generator object. PH7_NewClassInstance` |
|        - | 5054 | `			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do` |
|        - | 5055 | `			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope` |
|        - | 5056 | ``			 * and its __destruct (which runs pending `finally` blocks and frees the`` |
|        - | 5057 | `			 * exec context) never fires. */` |
|      351 | 5058 | `			PH7_MemObjRelease(pTos);` |
|      351 | 5059 | `			pTos = &pTos[-nCallArgs];` |
|      351 | 5060 | `			pTos->x.pOther = pGenObj;` |
|      351 | 5061 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|      351 | 5062 | `			if( pThis ){` |
|       29 | 5063 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 | 5064 | `			}` |
|      351 | 5065 | `			break;` |
|        - | 5066 | `		}` |
|        - | 5067 | `		/* Extract the formal argument set */` |
|  2180630 | 5068 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - | 5069 | `		/* Create a new VM frame  */` |
|  2180630 | 5070 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|  2180630 | 5071 | `		if( rc != SXRET_OK ){` |
|        - | 5072 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 5073 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 5074 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 5075 | `				&pVmFunc->sName);` |
|        - | 5076 | `			/* The frame that would own (and later release) $this never got created; for a bound` |
|        - | 5077 | `			 * plain closure the consumed transient is the object's ONLY ref, so release it here` |
|        - | 5078 | `			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */` |
|      ! 0 | 5079 | `			if( bClosureThis && pThis ){` |
|      ! 0 | 5080 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 | 5081 | `			}` |
|        - | 5082 | `			/* Pop given arguments */` |
|      ! 0 | 5083 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 5084 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 5085 | `			}` |
|        - | 5086 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 | 5087 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 5088 | `			break;` |
|        - | 5089 | `		}` |
|  2180630 | 5090 | `		if( pClosureScope ){` |
|        - | 5091 | `			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /` |
|        - | 5092 | `			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected` |
|        - | 5093 | `			 * access inside the body resolves against it. */` |
|       27 | 5094 | `			pFrame->pBoundScope = pClosureScope;` |
|       13 | 5095 | `		}` |
|        - | 5096 | `		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band` |
|        - | 5097 | `		 * A #4): sArg over-counts (defaulted params installed, variadic packed` |
|        - | 5098 | `		 * as one entry) so php's answers can't be derived from it. */` |
|  2180630 | 5099 | `		pFrame->nActualArgs = (int)(pTos - pArg);` |
|  2180630 | 5100 | `		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) \|\| bClosureThis) ){` |
|        - | 5101 | `			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */` |
|        - | 5102 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|  1870505 | 5103 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|  1870505 | 5104 | `			if( pObj ){` |
|        - | 5105 | `				/* Reflect the change */` |
|  1870505 | 5106 | `				pObj->x.pOther = pThis;` |
|  1870505 | 5107 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   935250 | 5108 | `			}` |
|   935250 | 5109 | `		}` |
|  2180630 | 5110 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - | 5111 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - | 5112 | `			/* Install static variables */` |
|     1170 | 5113 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|     2336 | 5114 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|     1170 | 5115 | `				pStatic = &aStatic[n];` |
|     1170 | 5116 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - | 5117 | `					/* Initialize the static variables */` |
|       40 | 5118 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|       40 | 5119 | `					if( pObj ){` |
|        - | 5120 | `						/* Assume a NULL initialization value */` |
|       40 | 5121 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|       40 | 5122 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - | 5123 | `							/* Evaluate initialization expression (Any complex expression) */` |
|       40 | 5124 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|       18 | 5125 | `						}` |
|       40 | 5126 | `						pObj->nIdx = pStatic->nIdx;` |
|       22 | 5127 | `					}else{` |
|      ! 0 | 5128 | `						continue;` |
|        - | 5129 | `					}` |
|       18 | 5130 | `				}` |
|        - | 5131 | `				/* Install in the current frame */` |
|     1753 | 5132 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|     1166 | 5133 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      587 | 5134 | `			}` |
|      583 | 5135 | `		}` |
|        - | 5136 | `		/* Push arguments in the local frame */` |
|        - | 5137 | `		{` |
|  2180630 | 5138 | `		VmCallArgMap *pCallMap3 = pEffCallMap;` |
|        - | 5139 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - | 5140 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|  2180630 | 5141 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|  2180630 | 5142 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - | 5143 | `			/* ============================================================` |
|        - | 5144 | `			 * Named-argument matching path (PHP 8.0)` |
|        - | 5145 | `			 *` |
|        - | 5146 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - | 5147 | `			 * or position, then install them in the frame.` |
|        - | 5148 | `			 * ============================================================ */` |
|      326 | 5149 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      326 | 5150 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|      326 | 5151 | `			sxi32 iVariadicIdx = -1;` |
|        - | 5152 | `			sxu32 nNonVariadic;` |
|        - | 5153 | `			sxi32 *aSlot;` |
|        - | 5154 | `			sxu8  *aUsed;` |
|        - | 5155 | `			sxu32 i;` |
|        - | 5156 | `			/* Find variadic parameter index */` |
|      844 | 5157 | `			for( i = 0; i < nFormal; i++ ){` |
|      620 | 5158 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|      102 | 5159 | `					iVariadicIdx = (sxi32)i;` |
|      102 | 5160 | `					break;` |
|        - | 5161 | `				}` |
|      263 | 5162 | `			}` |
|      326 | 5163 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - | 5164 | `			/* Allocate mapping arrays */` |
|      487 | 5165 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      322 | 5166 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|      326 | 5167 | `			if( aSlot == 0 ){` |
|      ! 0 | 5168 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 | 5169 | `				goto Abort;` |
|        - | 5170 | `			}` |
|      326 | 5171 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - | 5172 | `			/* Resolve named arguments to formal parameters */` |
|      487 | 5173 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|      161 | 5174 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|      326 | 5175 | `			if( rc == PH7_ABORT ){` |
|        8 | 5176 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 | 5177 | `				goto Abort;` |
|        - | 5178 | `			}` |
|      320 | 5179 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 5180 | `				/* php's catchable \Error for a bad named argument. The callee` |
|        - | 5181 | `				 * frame is already entered but its body must not run: unwind` |
|        - | 5182 | `				 * exactly like the named-hole ArgumentCountError path below` |
|        - | 5183 | `				 * (release the not-yet-installed actuals — Pass 2's release` |
|        - | 5184 | `				 * loop has not run — pop the callee slot, mark that there is no` |
|        - | 5185 | `				 * callee operand stack, and let VmCallFinish route the throw). */` |
|        - | 5186 | `				sxu32 iRel;` |
|        5 | 5187 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       11 | 5188 | `				for( iRel = 0; iRel < nActual; iRel++ ){` |
|        7 | 5189 | `					PH7_MemObjRelease(&pArg[iRel]);` |
|        4 | 5190 | `				}` |
|        5 | 5191 | `				PH7_MemObjRelease(pTos);` |
|        5 | 5192 | `				pTos = &pTos[-nCallArgs];` |
|        5 | 5193 | `				pFrameStack = 0;` |
|        5 | 5194 | `				goto SkipFuncBody;` |
|        - | 5195 | `			}` |
|        - | 5196 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|        - | 5197 | `			{` |
|        - | 5198 | `			/* php's required watermark for the hole check below (0 disables it` |
|        - | 5199 | `			 * for hosted builtin FUNCTIONS, which self-manage — hosted-class` |
|        - | 5200 | `			 * methods and all user code get php's named-hole error), plus the` |
|        - | 5201 | `			 * highest formal slot an actual resolved to: php words a hole` |
|        - | 5202 | ``			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole`` |
|        - | 5203 | `			 * with nothing filled above it gets the positional count message` |
|        - | 5204 | `			 * (zend's RECV arg_num > EX(num_args) distinction). */` |
|      316 | 5205 | `			sxu32 nReqNamed = 0;` |
|      316 | 5206 | `			sxu32 nNVNamed = 0;` |
|      316 | 5207 | `			sxu32 nMaxFilled = 0;` |
|      316 | 5208 | `			if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){` |
|      316 | 5209 | `				nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);` |
|     1170 | 5210 | `				for( i = 0; i < nActual; i++ ){` |
|      858 | 5211 | `					if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|      323 | 5212 | `						nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|      160 | 5213 | `					}` |
|      431 | 5214 | `				}` |
|      156 | 5215 | `			}` |
|      806 | 5216 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - | 5217 | `				/* Find the stack arg mapped to formal n */` |
|      503 | 5218 | `				sxi32 iSrc = -1;` |
|      805 | 5219 | `				for( i = 0; i < nActual; i++ ){` |
|      699 | 5220 | `					if( aSlot[i] == (sxi32)n ){` |
|      397 | 5221 | `						iSrc = (sxi32)i;` |
|      397 | 5222 | `						break;` |
|        - | 5223 | `					}` |
|      153 | 5224 | `				}` |
|      503 | 5225 | `				if( iSrc >= 0 ){` |
|        - | 5226 | `					/* Argument was provided — install with type checking */` |
|      397 | 5227 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - | 5228 | `					/* An explicit null is NOT redirected to the default: PHP applies a` |
|        - | 5229 | `					 * default only for an OMITTED argument. An explicit null falls through` |
|        - | 5230 | `					 * to the type check below (TypeError for a non-nullable typed param,` |
|        - | 5231 | ``					 * kept as null for a typeless one). An implicitly-nullable `Type $x =`` |
|        - | 5232 | ``					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */`` |
|        - | 5233 | `					/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 5234 | `					 * coercion and whole-real materialization in place): the shared` |
|        - | 5235 | `					 * per-argument helper — one implementation for both OP_CALL` |
|        - | 5236 | `					 * paths and the generator/fiber binder (§7.1(f) fold). */` |
|      397 | 5237 | `					rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pVal,bCallIsStrict,pSelfHint);` |
|      397 | 5238 | `					if( rc != SXRET_OK ){` |
|        7 | 5239 | `						if( rc == PH7_ABORT ) goto Abort;` |
|        7 | 5240 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 | 5241 | `						PH7_MemObjRelease(pTos);` |
|        7 | 5242 | `						pTos = &pTos[-nCallArgs];` |
|        7 | 5243 | `						pFrameStack = 0;` |
|        7 | 5244 | `						rc = PH7_EXCEPTION;` |
|        9 | 5245 | `						goto SkipFuncBody;` |
|        - | 5246 | `					}` |
|        - | 5247 | `					/* Install: by reference or by value */` |
|      391 | 5248 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 | 5249 | `						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){` |
|        - | 5250 | `							/* php 8.1: $GLOBALS cannot be passed by reference */` |
|        - | 5251 | `							SyBlob sMsg;` |
|      ! 0 | 5252 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 5253 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 5254 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 5255 | `							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|      ! 0 | 5256 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 5257 | `								goto Abort;` |
|        - | 5258 | `							}` |
|      ! 0 | 5259 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 5260 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 5261 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 5262 | `							pFrameStack = 0;` |
|      ! 0 | 5263 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 5264 | `							goto SkipFuncBody;` |
|        - | 5265 | `						}` |
|        5 | 5266 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 | 5267 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|      ! 0 | 5268 | `							 && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){` |
|        - | 5269 | `								/* A non-lvalue bound to a by-ref parameter is a catchable Error in` |
|        - | 5270 | `								 * php — f(5) where f(&$x). PH7 only warned and quietly passed by` |
|        - | 5271 | `								 * value, so the call ran with a copy and the caller never knew.` |
|        - | 5272 | `								 * The one legitimate copy is call_user_func()'s (MEMOBJ_AUX_CUFVAL),` |
|        - | 5273 | `								 * which php also permits, with its own warning. */` |
|        - | 5274 | `								SyBlob sMsg;` |
|        - | 5275 | `								sxi32 rcRef;` |
|      ! 0 | 5276 | `								SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 5277 | `								SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 5278 | `									&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 5279 | `								rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|      ! 0 | 5280 | `									SyBlobLength(&sMsg));` |
|      ! 0 | 5281 | `								SyBlobRelease(&sMsg);` |
|      ! 0 | 5282 | `								if( rcRef == SXERR_ABORT ){` |
|      ! 0 | 5283 | `									pFrameStack = 0;` |
|      ! 0 | 5284 | `									rc = PH7_ABORT;` |
|      ! 0 | 5285 | `									goto SkipFuncBody;` |
|        - | 5286 | `								}` |
|      ! 0 | 5287 | `								pFrameStack = 0;` |
|      ! 0 | 5288 | `								rc = PH7_EXCEPTION;` |
|      ! 0 | 5289 | `								goto SkipFuncBody;` |
|        - | 5290 | `							}` |
|      ! 0 | 5291 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 5292 | `						}else{` |
|        7 | 5293 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 | 5294 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 | 5295 | `							if( pRefEntry == 0 ){` |
|        7 | 5296 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 | 5297 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 | 5298 | `								sArg.nIdx = pVal->nIdx;` |
|        5 | 5299 | `								sArg.pUserData = 0;` |
|        5 | 5300 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 | 5301 | `							}` |
|        5 | 5302 | `							pObj = 0;` |
|        - | 5303 | `						}` |
|        3 | 5304 | `					}else{` |
|      387 | 5305 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 5306 | `					}` |
|      391 | 5307 | `					if( pObj ){` |
|      387 | 5308 | `						PH7_MemObjStore(pVal,pObj);` |
|      387 | 5309 | `						sArg.nIdx = pObj->nIdx;` |
|      387 | 5310 | `						sArg.pUserData = 0;` |
|      387 | 5311 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      192 | 5312 | `					}` |
|      197 | 5313 | `				}else{` |
|        - | 5314 | `					/* Argument was NOT provided — use default or leave unset */` |
|      108 | 5315 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 5316 | `						/* Should not reach here; variadic handled separately below */` |
|      108 | 5317 | `					}else if( n < nReqNamed ){` |
|        - | 5318 | `						/* php's implicit-required rule applies to named calls` |
|        - | 5319 | `						 * too: a hole below the required watermark throws even` |
|        - | 5320 | `						 * when the formal carries a default — f($a,$b=2,$c)` |
|        - | 5321 | ``						 * called as f(a:1,c:3) is `Argument #2 ($b) not`` |
|        - | 5322 | ``						 * passed` (a hole with NO default is always below the`` |
|        - | 5323 | `						 * watermark, so this subsumes the no-default case).` |
|        - | 5324 | `						 * A hole with nothing filled ABOVE it uses php's` |
|        - | 5325 | `						 * positional count wording instead. The passed stack` |
|        - | 5326 | `						 * args were not released yet on this path (that loop` |
|        - | 5327 | `						 * runs after Pass 2) — release them before the exit. */` |
|        5 | 5328 | `						if( n + 1 > nMaxFilled ){` |
|        3 | 5329 | `							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)` |
|      ! 0 | 5330 | `								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 5331 | `									nMaxFilled,nReqNamed,nNVNamed)` |
|        3 | 5332 | `								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        1 | 5333 | `									nMaxFilled,nReqNamed,nNVNamed,TRUE);` |
|        2 | 5334 | `						}else{` |
|        3 | 5335 | `							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        - | 5336 | `						}` |
|        5 | 5337 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        9 | 5338 | `						for( i = 0; i < nActual; i++ ){` |
|        5 | 5339 | `							PH7_MemObjRelease(&pArg[i]);` |
|        3 | 5340 | `						}` |
|        5 | 5341 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 5342 | `							goto Abort;` |
|        - | 5343 | `						}` |
|        5 | 5344 | `						PH7_MemObjRelease(pTos);` |
|        5 | 5345 | `						pTos = &pTos[-nCallArgs];` |
|        5 | 5346 | `						pFrameStack = 0;` |
|        5 | 5347 | `						rc = PH7_EXCEPTION;` |
|        5 | 5348 | `						goto SkipFuncBody;` |
|      104 | 5349 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      104 | 5350 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      104 | 5351 | `						if( pObj ){` |
|      104 | 5352 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|      104 | 5353 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      104 | 5354 | `							sArg.nIdx = pObj->nIdx;` |
|      104 | 5355 | `							sArg.pUserData = 0;` |
|      104 | 5356 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 5357 | `							/* A null default on an implicitly-nullable param must stay null` |
|        - | 5358 | `							 * (see the positional-path note above). */` |
|      102 | 5359 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|       42 | 5360 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0` |
|       23 | 5361 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 5362 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 5363 | `								if( xCast ) xCast(pObj);` |
|      ! 0 | 5364 | `							}else{` |
|        - | 5365 | `								/* Mask matched — a const-indirected whole-real default` |
|        - | 5366 | ``								 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|      104 | 5367 | `								VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 5368 | `							}` |
|       51 | 5369 | `						}` |
|       51 | 5370 | `					}` |
|        - | 5371 | `				}` |
|      248 | 5372 | `			}` |
|        - | 5373 | `			} /* end nReqNamed scope */` |
|        - | 5374 | `			/* Handle variadic parameter */` |
|      306 | 5375 | `			if( iVariadicIdx >= 0 ){` |
|      102 | 5376 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|      102 | 5377 | `				if( pObj ){` |
|        - | 5378 | `					/* Capture the slot index now: PH7_HashmapInsert below can` |
|        - | 5379 | `					 * PH7_ReserveMemObj, reallocating pVm->aMemObj and dangling pObj` |
|        - | 5380 | `					 * (same latent UAF the positional path guards against). */` |
|        - | 5381 | `					sxu32 nVariadicSlot;` |
|      102 | 5382 | `					PH7_MemObjToHashmap(pObj);` |
|      102 | 5383 | `					nVariadicSlot = pObj->nIdx;` |
|        - | 5384 | `					{` |
|      102 | 5385 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - | 5386 | `						/* php numbers a failing NAMED variadic element as` |
|        - | 5387 | `						 * max(total positional args, declared non-variadic` |
|        - | 5388 | `						 * formals) + 1 — zend's RECV slots always count —` |
|        - | 5389 | `						 * whichever named element fails; a POSITIONAL element` |
|        - | 5390 | `						 * uses its own 1-based call position. */` |
|      102 | 5391 | `						sxu32 nPositional = 0;` |
|      606 | 5392 | `						for( i = 0; i < nActual; i++ ){` |
|      508 | 5393 | `							if( !(i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0) ){` |
|      333 | 5394 | `								nPositional++;` |
|      165 | 5395 | `							}` |
|      256 | 5396 | `						}` |
|      564 | 5397 | `						for( i = 0; i < nActual; i++ ){` |
|      502 | 5398 | `							if( aSlot[i] == -1 ){` |
|      458 | 5399 | `								int bNamed = (i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0);` |
|        - | 5400 | `								/* Same per-element type check + weak coercion as the` |
|        - | 5401 | ``								 * positional-only path (shared helper; no `($name)`). */`` |
|      685 | 5402 | `								rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|      454 | 5403 | `									&aFormalArg[iVariadicIdx],&pArg[i],` |
|      301 | 5404 | `									bNamed ? SXMAX(nPositional,nNonVariadic) + 1 : i + 1,bCallIsStrict);` |
|      458 | 5405 | `								if( rc != SXRET_OK ){` |
|       39 | 5406 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 5407 | `										goto Abort;` |
|        - | 5408 | `									}` |
|       39 | 5409 | `									SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       39 | 5410 | `									PH7_MemObjRelease(pTos);` |
|       39 | 5411 | `									pTos = &pTos[-nCallArgs];` |
|       39 | 5412 | `									pFrameStack = 0;` |
|       39 | 5413 | `									rc = PH7_EXCEPTION;` |
|       39 | 5414 | `									goto SkipFuncBody;` |
|        - | 5415 | `								}` |
|      422 | 5416 | `								if( bNamed ){` |
|        - | 5417 | `									/* Named variadic entry: insert with string key */` |
|        - | 5418 | `									ph7_value sKey;` |
|      120 | 5419 | `									PH7_MemObjInit(pVm, &sKey);` |
|      120 | 5420 | `									PH7_MemObjStringAppend(&sKey,` |
|      116 | 5421 | `										pCallMap3->aNames[i].zString,` |
|      116 | 5422 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|      120 | 5423 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|      120 | 5424 | `									PH7_MemObjRelease(&sKey);` |
|       62 | 5425 | `								}else{` |
|        - | 5426 | `									/* Positional variadic entry */` |
|      305 | 5427 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - | 5428 | `								}` |
|      209 | 5429 | `							}` |
|      235 | 5430 | `						}` |
|        - | 5431 | `					}` |
|       66 | 5432 | `					sArg.nIdx = nVariadicSlot; /* pObj may be stale here (aMemObj realloc) */` |
|       66 | 5433 | `					sArg.pUserData = 0;` |
|       66 | 5434 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       31 | 5435 | `				}` |
|       35 | 5436 | `			}else{` |
|        - | 5437 | `				/* No variadic — preserve unresolved positional overflow` |
|        - | 5438 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - | 5439 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - | 5440 | `				 * the positional-only path's behavior. */` |
|      207 | 5441 | `				sxu32 nAnon = nNonVariadic;` |
|      547 | 5442 | `				for( i = 0; i < nActual; i++ ){` |
|      343 | 5443 | `					if( aSlot[i] == -2 ){` |
|        - | 5444 | `						char zAnonBuf[32];` |
|        - | 5445 | `						SyString sAnonName;` |
|      ! 0 | 5446 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 | 5447 | `							"[%u]apArg",nAnon);` |
|      ! 0 | 5448 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 | 5449 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 | 5450 | `						if( pObj ){` |
|      ! 0 | 5451 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 | 5452 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 | 5453 | `							sArg.pUserData = 0;` |
|      ! 0 | 5454 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 | 5455 | `						}` |
|      ! 0 | 5456 | `						nAnon++;` |
|      ! 0 | 5457 | `					}` |
|      173 | 5458 | `				}` |
|        - | 5459 | `			}` |
|        - | 5460 | `			/* Release all stack arguments */` |
|     1042 | 5461 | `			for( i = 0; i < nActual; i++ ){` |
|      776 | 5462 | `				PH7_MemObjRelease(&pArg[i]);` |
|      390 | 5463 | `			}` |
|      270 | 5464 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - | 5465 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|      270 | 5466 | `			n = nFormal;` |
|      137 | 5467 | `		}else{` |
|        - | 5468 | `		/* ============================================================` |
|        - | 5469 | `		 * Positional-only matching path (original)` |
|        - | 5470 | `		 * ============================================================ */` |
|        - | 5471 | `		/* Base of the actual argument stack, for php's per-ELEMENT argument` |
|        - | 5472 | `		 * number in a variadic type-error (php numbers a variadic-collected` |
|        - | 5473 | `		 * element by its overall 1-based call position, not the formal index). */` |
|  2180308 | 5474 | `		ph7_value *pArgBase = pArg;` |
|  2180308 | 5475 | `		n = 0;` |
|  3812932 | 5476 | `		while( pArg < pTos ){` |
|  1633005 | 5477 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - | 5478 | `				/* Variadic parameter: collect all remaining args into an array */` |
|      243 | 5479 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      243 | 5480 | `				if( pObj ){` |
|        - | 5481 | `					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,` |
|        - | 5482 | `					 * reallocating pVm->aMemObj and dangling pObj — so don't read pObj->nIdx after` |
|        - | 5483 | `					 * the packing loop (pre-existing UAF, masked by the pool allocator). pMap is a` |
|        - | 5484 | `					 * separately-allocated hashmap and stays valid across the realloc. */` |
|        - | 5485 | `					sxu32 nVariadicIdx;` |
|        - | 5486 | `					/* Initialize as empty array */` |
|      243 | 5487 | `					PH7_MemObjToHashmap(pObj);` |
|      243 | 5488 | `					nVariadicIdx = pObj->nIdx;` |
|        - | 5489 | `					{` |
|      243 | 5490 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     1833 | 5491 | `						while( pArg < pTos ){` |
|        - | 5492 | `							/* Per-element type check + weak coercion (shared helper,` |
|        - | 5493 | `							 * also used by the named-argument path). The argument` |
|        - | 5494 | `							 * number is the element's overall 1-based call position` |
|        - | 5495 | `` 							 * ((pArg - pArgBase) + 1) and, like php, the `($name)` `` |
|        - | 5496 | `							 * clause is omitted. */` |
|     2438 | 5497 | `							rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|     1622 | 5498 | `								&aFormalArg[n],pArg,(sxu32)(pArg - pArgBase) + 1,bCallIsStrict);` |
|     1627 | 5499 | `							if( rc != SXRET_OK ){` |
|       35 | 5500 | `								if( rc == PH7_ABORT ){` |
|      ! 0 | 5501 | `									goto Abort;` |
|        - | 5502 | `								}` |
|        - | 5503 | `								/* Skip function body, route through normal cleanup */` |
|       35 | 5504 | `								PH7_MemObjRelease(pTos);` |
|       35 | 5505 | `								pTos = &pTos[-nCallArgs];` |
|       35 | 5506 | `								pFrameStack = 0;` |
|       35 | 5507 | `								rc = PH7_EXCEPTION;` |
|       35 | 5508 | `								goto SkipFuncBody;` |
|        - | 5509 | `							}` |
|     1595 | 5510 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|     1595 | 5511 | `							pArg++;` |
|        5 | 5512 | `						}` |
|        - | 5513 | `					}` |
|      211 | 5514 | `					sArg.nIdx = nVariadicIdx; /* pObj may be stale here (aMemObj realloc) — use the saved index */` |
|      211 | 5515 | `					sArg.pUserData = 0;` |
|      211 | 5516 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      103 | 5517 | `				}` |
|      211 | 5518 | `				break; /* All remaining args consumed */` |
|        - | 5519 | `			}` |
|  1632767 | 5520 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 5521 | `				/* An explicit null is NOT redirected to the default (PHP applies a` |
|        - | 5522 | `				 * default only for an omitted arg); it falls through to the type check` |
|        - | 5523 | `				 * below — TypeError for a non-nullable typed param, kept as null for a` |
|        - | 5524 | ``				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE`` |
|        - | 5525 | `				 * at compile time so its check accepts null. */` |
|        - | 5526 | `				/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 5527 | `				 * coercion and whole-real materialization in place — nullable` |
|        - | 5528 | `				 * types (?type) let null through): the shared per-argument` |
|        - | 5529 | `				 * helper, one implementation for both OP_CALL paths and the` |
|        - | 5530 | `				 * generator/fiber binder (§7.1(f) fold). */` |
|  1631873 | 5531 | `				rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pArg,bCallIsStrict,pSelfHint);` |
|  1631873 | 5532 | `				if( rc != SXRET_OK ){` |
|      141 | 5533 | `					if( rc == PH7_ABORT ){` |
|        6 | 5534 | `						goto Abort;` |
|        - | 5535 | `					}` |
|        - | 5536 | `					/* Skip function body, route through normal cleanup */` |
|      137 | 5537 | `					PH7_MemObjRelease(pTos);` |
|      137 | 5538 | `					pTos = &pTos[-nCallArgs];` |
|      137 | 5539 | `					pFrameStack = 0;` |
|      137 | 5540 | `					rc = PH7_EXCEPTION;` |
|      137 | 5541 | `					goto SkipFuncBody;` |
|        - | 5542 | `				}` |
|  1631737 | 5543 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 5544 | `					/* Pass by reference */` |
|     2315 | 5545 | `					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){` |
|        - | 5546 | `						/* php 8.1: $GLOBALS cannot be passed by reference —` |
|        - | 5547 | `						 * a catchable Error with php's exact wording. */` |
|        - | 5548 | `						SyBlob sMsg;` |
|        3 | 5549 | `						SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 5550 | `						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|        2 | 5551 | `							&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        3 | 5552 | `						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 5553 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 5554 | `							goto Abort;` |
|        - | 5555 | `						}` |
|        3 | 5556 | `						PH7_MemObjRelease(pTos);` |
|        3 | 5557 | `						pTos = &pTos[-nCallArgs];` |
|        3 | 5558 | `						pFrameStack = 0;` |
|        3 | 5559 | `						rc = PH7_EXCEPTION;` |
|        3 | 5560 | `						goto SkipFuncBody;` |
|        - | 5561 | `					}` |
|     2313 | 5562 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        2 | 5563 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|        3 | 5564 | `						 && (pArg->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){` |
|        - | 5565 | `							/* php: a non-lvalue bound to a by-ref parameter is a catchable Error.` |
|        - | 5566 | `							 * PH7 warned and silently passed by value (same site as the other` |
|        - | 5567 | `							 * binder above). call_user_func()'s deliberate copy is exempt. */` |
|        - | 5568 | `							SyBlob sMsg;` |
|        - | 5569 | `							sxi32 rcRef;` |
|      ! 0 | 5570 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 5571 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 5572 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 5573 | `							rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|      ! 0 | 5574 | `								SyBlobLength(&sMsg));` |
|      ! 0 | 5575 | `							SyBlobRelease(&sMsg);` |
|      ! 0 | 5576 | `							return (rcRef == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 5577 | `						}` |
|        - | 5578 | `						/* Switch to pass by value */` |
|        3 | 5579 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        2 | 5580 | `					}else{` |
|        - | 5581 | `						SyHashEntry *pRefEntry;` |
|        - | 5582 | `						/* Install the referenced variable in the private function frame */` |
|     2311 | 5583 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|     2311 | 5584 | `						if( pRefEntry == 0 ){` |
|     3464 | 5585 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|     2306 | 5586 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|     2311 | 5587 | `							sArg.nIdx = pArg->nIdx;` |
|     2311 | 5588 | `							sArg.pUserData = 0;` |
|     2311 | 5589 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     1153 | 5590 | `						}` |
|     2311 | 5591 | `						pObj = 0;` |
|        - | 5592 | `					}` |
|     1159 | 5593 | `				}else{` |
|        - | 5594 | `					/* Pass by value,make a copy of the given argument */` |
|  1629427 | 5595 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 5596 | `				}` |
|   816195 | 5597 | `			}else{` |
|        - | 5598 | `				char zName[32];` |
|        - | 5599 | `				SyString sArgName;` |
|        - | 5600 | `				/* Set a dummy name */` |
|      899 | 5601 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      899 | 5602 | `				sArgName.zString = zName;` |
|        - | 5603 | `				/* Annonymous argument */` |
|      899 | 5604 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 5605 | `			}` |
|  1632629 | 5606 | `			if( pObj ){` |
|  1630323 | 5607 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 5608 | `				/* Insert argument index  */` |
|  1630323 | 5609 | `				sArg.nIdx = pObj->nIdx;` |
|  1630323 | 5610 | `				sArg.pUserData = 0;` |
|  1630323 | 5611 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|   815484 | 5612 | `			}` |
|  1632629 | 5613 | `			PH7_MemObjRelease(pArg);` |
|  1632629 | 5614 | `			pArg++;` |
|  1632629 | 5615 | `			++n;` |
|        5 | 5616 | `		}` |
|        - | 5617 | `		} /* end named vs positional branch */` |
|        - | 5618 | `		/* Set up closure environment */` |
|  2180404 | 5619 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 5620 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 5621 | `			ph7_value *pValue;` |
|        - | 5622 | `			sxu32 iEnv;` |
|     2377 | 5623 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|     5187 | 5624 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|     2815 | 5625 | `				pEnv = &aEnv[iEnv];` |
|     2815 | 5626 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 5627 | `					/* Do not install null value */` |
|     2275 | 5628 | `					continue;` |
|        - | 5629 | `				}` |
|      540 | 5630 | `				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       11 | 5631 | `				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|        - | 5632 | `					/* The Closure instance carries an explicit bound $this` |
|        - | 5633 | `					 * (bindTo/bind/call): it wins over the creation-time` |
|        - | 5634 | `					 * captured $this, php-exact. */` |
|        5 | 5635 | `					continue;` |
|        - | 5636 | `				}` |
|      541 | 5637 | `				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|        - | 5638 | `					/* Captured by reference: link the name to the shared slot` |
|        - | 5639 | `					 * (no copy), mirroring the by-ref argument install above. */` |
|      163 | 5640 | `					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|      243 | 5641 | `						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),` |
|      160 | 5642 | `							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|       80 | 5643 | `					}` |
|      163 | 5644 | `					continue;` |
|        - | 5645 | `				}` |
|      380 | 5646 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|      380 | 5647 | `				if( pValue == 0 ){` |
|      ! 0 | 5648 | `					continue;` |
|        - | 5649 | `				}` |
|        - | 5650 | `				/* Invalidate any prior representation */` |
|      380 | 5651 | `				PH7_MemObjRelease(pValue);` |
|        - | 5652 | `				/* Duplicate bound variable value */` |
|      380 | 5653 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|      192 | 5654 | `			}` |
|     1186 | 5655 | `		}` |
|        - | 5656 | `		/* Too-few-arguments check, placed AFTER the passed arguments were` |
|        - | 5657 | `		 * installed and type-checked: php's RECV order means a type error on` |
|        - | 5658 | ``		 * a PASSED argument beats the count error (`f(int $x,$y)` called`` |
|        - | 5659 | `		 * f("str") is a TypeError, not ArgumentCountError). The passed args` |
|        - | 5660 | `		 * were already released by the install loop, so the standard throw` |
|        - | 5661 | `		 * exit leaks nothing. The named path never fires this (its per-hole` |
|        - | 5662 | `		 * check ran in-loop; n == nNonVariadic >= nRequired here). Hosted` |
|        - | 5663 | `		 * builtin FUNCTIONS (VM_FUNC_INTERNAL) are exempt — their PHL` |
|        - | 5664 | `		 * signatures don't always mirror php's true arity and their in-body` |
|        - | 5665 | `		 * self-checks own php's wording (stage-2 family); hosted-class` |
|        - | 5666 | `		 * METHODS get php's ZPP wording via VmThrowBuiltinTooFewArgs. */` |
|  2180399 | 5667 | `		if( n < SySetUsed(&pVmFunc->aArgs)` |
|  1816395 | 5668 | `		 && (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){` |
|        - | 5669 | `			sxu32 nNonVar,nReq;` |
|  1450743 | 5670 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|  1450743 | 5671 | `			if( n < nReq ){` |
|       25 | 5672 | `				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;` |
|       25 | 5673 | `				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|        4 | 5674 | `					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        1 | 5675 | `						nPassed,nReq,nNonVar);` |
|        2 | 5676 | `				}else{` |
|       33 | 5677 | `					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       10 | 5678 | `						nPassed,nReq,nNonVar,TRUE);` |
|        - | 5679 | `				}` |
|       25 | 5680 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 5681 | `					goto Abort;` |
|        - | 5682 | `				}` |
|       25 | 5683 | `				PH7_MemObjRelease(pTos);` |
|       25 | 5684 | `				pTos = &pTos[-nCallArgs];` |
|       25 | 5685 | `				pFrameStack = 0;` |
|       25 | 5686 | `				rc = PH7_EXCEPTION;` |
|       25 | 5687 | `				goto SkipFuncBody;` |
|        - | 5688 | `			}` |
|   725358 | 5689 | `		}` |
|        - | 5690 | `		/* Process default values for remaining formal parameters */` |
|  5086228 | 5691 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|  2906103 | 5692 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 5693 | `				/* Variadic parameter with no extra args — create empty array */` |
|      257 | 5694 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      257 | 5695 | `				if( pObj ){` |
|      257 | 5696 | `					PH7_MemObjToHashmap(pObj);` |
|      257 | 5697 | `					sArg.nIdx = pObj->nIdx;` |
|      257 | 5698 | `					sArg.pUserData = 0;` |
|      257 | 5699 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      126 | 5700 | `				}` |
|      257 | 5701 | `				n++;` |
|      257 | 5702 | `				break; /* Variadic is always last */` |
|        - | 5703 | `			}` |
|  2905851 | 5704 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|  2905849 | 5705 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|  2905849 | 5706 | `				if( pObj ){` |
|        - | 5707 | `					/* Evaluate the default value and extract it's result */` |
|  2905849 | 5708 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|  2905849 | 5709 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 5710 | `						goto Abort;` |
|        - | 5711 | `					}` |
|        - | 5712 | `					/* Insert argument index */` |
|  2905849 | 5713 | `					sArg.nIdx = pObj->nIdx;` |
|  2905849 | 5714 | `					sArg.pUserData = 0;` |
|  2905849 | 5715 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 5716 | `					/* Make sure the default argument is of the correct type.` |
|        - | 5717 | ``					 * A null default on an implicitly-nullable param (`int $x = null`)`` |
|        - | 5718 | `					 * must stay null — casting it to 0/""/false would diverge from PHP` |
|        - | 5719 | `					 * and contradict the explicit-null path, which now keeps it null. */` |
|  2905844 | 5720 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|  1448646 | 5721 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)` |
|   724332 | 5722 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 5723 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 5724 | `						/* Cast to the desired type */` |
|      ! 0 | 5725 | `						xCast(pObj);` |
|      ! 0 | 5726 | `					}else{` |
|        - | 5727 | `						/* Mask matched — a const-indirected whole-real default` |
|        - | 5728 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|  2905849 | 5729 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 5730 | `					}` |
|  1452922 | 5731 | `				}` |
|  1452922 | 5732 | `			}` |
|  2905851 | 5733 | `			++n;` |
|        5 | 5734 | `		}` |
|        - | 5735 | `		} /* end VmCallArgMap scope */` |
|        - | 5736 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 5737 | `		 * does not return anything.` |
|        - | 5738 | `		 */` |
|  2180382 | 5739 | `		PH7_MemObjRelease(pTos);` |
|  2180382 | 5740 | `		pTos = &pTos[-nCallArgs];` |
|        - | 5741 | `		/* Allocate an operand stack (via the recycling allocator) and evaluate the` |
|        - | 5742 | `		 * function body. Size it to a tight static bound when the body is statically` |
|        - | 5743 | `		 * modelable (BYTECODE.md stage 7) — the big memory win for deep recursion,` |
|        - | 5744 | `		 * where one such stack lives per frame — falling back to the safe` |
|        - | 5745 | `		 * instruction-count bound otherwise.` |
|        - | 5746 | `		 *` |
|        - | 5747 | `		 * The bound is computed LAZILY on the first call and cached on the func` |
|        - | 5748 | `		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:` |
|        - | 5749 | `		 * self-computing on first use is fail-safe against any body-creation path` |
|        - | 5750 | `		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),` |
|        - | 5751 | `		 * where undersizing is a heap overflow; the amortized cost is one analysis` |
|        - | 5752 | `		 * per function. */` |
|        - | 5753 | `		{` |
|  2180382 | 5754 | `			sxu32 nSlots = pVmFunc->nMaxStack;` |
|  2180382 | 5755 | `			if( nSlots == 0 ){` |
|     9249 | 5756 | `				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);` |
|    13871 | 5757 | `				sxu32 nTight = VmComputeMaxStack(&(*pVm),` |
|     9244 | 5758 | `					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);` |
|     9249 | 5759 | `				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;` |
|     9249 | 5760 | `				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */` |
|     9249 | 5761 | `				pVmFunc->nMaxStack = nSlots;` |
|     4622 | 5762 | `			}` |
|  2180382 | 5763 | `			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);` |
|        - | 5764 | `		}` |
|  2180382 | 5765 | `		if( pFrameStack == 0 ){` |
|        - | 5766 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 5767 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 5768 | `				&pVmFunc->sName);` |
|      ! 0 | 5769 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 5770 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 5771 | `			}` |
|      ! 0 | 5772 | `			break;` |
|        - | 5773 | `		}` |
|  1090026 | 5774 | `SkipFuncBody:` |
|  2180620 | 5775 | `		if( pSelf ){` |
|        - | 5776 | `			/* Push class name */` |
|  1971455 | 5777 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|   985725 | 5778 | `		}` |
|        - | 5779 | `		/* Increment nesting level */` |
|  2180620 | 5780 | `		pVm->nRecursionDepth++;` |
|  2180620 | 5781 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 5782 | `			/* Arg-binding threw: there is no body to run — finish the call` |
|        - | 5783 | `			 * immediately (no record is pushed). */` |
|        - | 5784 | `			VmCallRecord sCallee;` |
|      243 | 5785 | `			sCallee.pVmFunc = pVmFunc;` |
|      243 | 5786 | `			sCallee.pFrame = pFrame;` |
|      243 | 5787 | `			sCallee.pFrameStack = pFrameStack;` |
|      243 | 5788 | `			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|      243 | 5789 | `			sCallee.nLastRef = SXU32_HIGH;` |
|      243 | 5790 | `			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|      243 | 5791 | `			sState.pTos = pTos;` |
|      243 | 5792 | `			sState.pc = pc;` |
|      243 | 5793 | `			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);` |
|      243 | 5794 | `			pTos = sState.pTos;` |
|      243 | 5795 | `			pc = sState.pc;` |
|      243 | 5796 | `			if( rc == PH7_ABORT ){` |
|        - | 5797 | `				/* Abort processing immeditaley */` |
|      ! 0 | 5798 | `				goto Abort;` |
|      243 | 5799 | `			}else if( rc == PH7_SUSPEND ){` |
|      ! 0 | 5800 | `				goto Suspend;` |
|      243 | 5801 | `			}else if( rc == PH7_EXCEPTION ){` |
|       31 | 5802 | `				goto Exception;` |
|        - | 5803 | `			}` |
|      109 | 5804 | `		}else{` |
|        - | 5805 | `			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a` |
|        - | 5806 | `			 * call record (caller activation + in-flight call) and switch the` |
|        - | 5807 | `			 * loop's locals to the callee — a PHP->PHP call no longer grows` |
|        - | 5808 | `			 * the native stack. The record node is pool-allocated so` |
|        - | 5809 | `			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */` |
|  2180382 | 5810 | `			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;` |
|  2180382 | 5811 | `			if( pRec ){` |
|  2177524 | 5812 | `				pVm->pIdleCallFrames = (void *)pRec->pPrev;` |
|  1088927 | 5813 | `			}else{` |
|     2863 | 5814 | `				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));` |
|        - | 5815 | `			}` |
|  2180382 | 5816 | `			if( pRec == 0 ){` |
|        - | 5817 | `				/* OOM: undo the push-time accounting, tear the call down and` |
|        - | 5818 | `				 * raise the non-catchable fatal (the §3.1 OOM convention —` |
|        - | 5819 | `				 * never a silent NULL). */` |
|      ! 0 | 5820 | `				pVm->nRecursionDepth--;` |
|      ! 0 | 5821 | `				if( pSelf ){` |
|      ! 0 | 5822 | `					(void)SySetPop(&pVm->aSelf);` |
|      ! 0 | 5823 | `				}` |
|      ! 0 | 5824 | `				SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|      ! 0 | 5825 | `				VmLeaveFrame(&(*pVm));` |
|      ! 0 | 5826 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 5827 | `				goto Abort;` |
|        - | 5828 | `			}` |
|  2180382 | 5829 | `			sState.pTos = pTos;` |
|  2180382 | 5830 | `			sState.pc = pc;` |
|  2180382 | 5831 | `			pRec->sCaller = sState;` |
|  2180382 | 5832 | `			pRec->sCall.pVmFunc = pVmFunc;` |
|  2180382 | 5833 | `			pRec->sCall.pFrame = pFrame;` |
|  2180382 | 5834 | `			pRec->sCall.pFrameStack = pFrameStack;` |
|  2180382 | 5835 | `			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|  2180382 | 5836 | `			pRec->sCall.nLastRef = SXU32_HIGH;` |
|  2180382 | 5837 | `			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|  2180382 | 5838 | `			pRec->pPrev = pCallTop;` |
|  2180382 | 5839 | `			pCallTop = pRec;` |
|        - | 5840 | `			/* Switch to the callee activation (what the recursive` |
|        - | 5841 | `			 * VmByteCodeExec entry used to set up). */` |
|  2180382 | 5842 | `			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);` |
|  2180382 | 5843 | `			pStack = pFrameStack;` |
|  2180382 | 5844 | `			pTos = &pStack[-1];` |
|  2180382 | 5845 | `			pc = 0;` |
|  2180382 | 5846 | `			sState.aInstr = aInstr;` |
|  2180382 | 5847 | `			sState.pStack = pStack;` |
|  2180382 | 5848 | `			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */` |
|  2180382 | 5849 | `			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */` |
|  2180382 | 5850 | `			sState.pTos = pTos;` |
|  2180382 | 5851 | `			sState.pc = 0;` |
|  2180382 | 5852 | `			sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|  2180382 | 5853 | `			sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|  2180382 | 5854 | `			sState.pEntryFrame = pVm->pFrame;` |
|  2180382 | 5855 | `			sState.pResult = pRec->sCaller.pTos;` |
|  2180382 | 5856 | `			sState.pLastRef = &pRec->sCall.nLastRef;` |
|  2180382 | 5857 | `			sState.pEnforceRetFunc = VmFuncHasReturnType(pVmFunc) ? pVmFunc : 0;` |
|  2180382 | 5858 | `			sState.is_callback = 0;` |
|  2180382 | 5859 | `			sState.bReturnPropagates = 0;` |
|  2180382 | 5860 | `			goto VmLoopFetch;` |
|        - | 5861 | `		}` |
|      109 | 5862 | `	}else{` |
|        - | 5863 | `		ph7_user_func *pFunc;` |
|        - | 5864 | `		ph7_context sCtx;` |
|        - | 5865 | `		ph7_value sRet;` |
|        - | 5866 | `		/* Look for an installed foreign function.` |
|        - | 5867 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 5868 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 5869 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 5870 | `		 * global fallback for unqualified function calls in namespaces. */` |
|  4177148 | 5871 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 5872 | `		{` |
|  4177148 | 5873 | `		VmCallArgMap *pCallMap2 = pEffCallMap;` |
|  4177148 | 5874 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 5875 | `			/* Compiler-qualified: try short name as global fallback */` |
|       32 | 5876 | `			const char *zShort = sName.zString;` |
|        - | 5877 | `			sxu32 i;` |
|      518 | 5878 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      490 | 5879 | `				if( sName.zString[i] == '\\' ){` |
|       46 | 5880 | `					zShort = &sName.zString[i + 1];` |
|       21 | 5881 | `				}` |
|      247 | 5882 | `			}` |
|       32 | 5883 | `			if( zShort != sName.zString ){` |
|       32 | 5884 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       32 | 5885 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       14 | 5886 | `			}` |
|       14 | 5887 | `		}` |
|        - | 5888 | `		} /* end VmCallArgMap namespace scope */` |
|  4177148 | 5889 | `		if( pEntry == 0 ){` |
|        - | 5890 | `			/* php accepts the "Class::method" STATIC-callable string everywhere a` |
|        - | 5891 | `			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).` |
|        - | 5892 | `			 * Split on the first "::" and route through the shared array-callable` |
|        - | 5893 | `			 * machinery ([class-name, method-name]) instead of warning undefined. */` |
|        - | 5894 | `			sxu32 iSep;` |
|   240064 | 5895 | `			int bScoped = 0;` |
|  2800700 | 5896 | `			for( iSep = 1 ; iSep + 2 < sName.nByte ; ++iSep ){` |
|  2660652 | 5897 | `				if( sName.zString[iSep] == ':' && sName.zString[iSep+1] == ':' ){` |
|   100014 | 5898 | `					bScoped = 1;` |
|   100014 | 5899 | `					break;` |
|        - | 5900 | `				}` |
|  1280322 | 5901 | `			}` |
|   240064 | 5902 | `			if( bScoped ){` |
|   100014 | 5903 | `				ph7_hashmap *pCbMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   100014 | 5904 | `				if( pCbMap ){` |
|        - | 5905 | `					ph7_value sCallable,sElem,sResult;` |
|        - | 5906 | `					sxi32 rcSm;` |
|   150020 | 5907 | `					pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100012 | 5908 | `						nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|   100014 | 5909 | `					SySetReset(&aArg);` |
|   100032 | 5910 | `					while( pArg < pTos ){` |
|       19 | 5911 | `						SySetPut(&aArg,(const void *)&pArg);` |
|       19 | 5912 | `						pArg++;` |
|        1 | 5913 | `					}` |
|   100014 | 5914 | `					PH7_MemObjInit(pVm,&sElem);` |
|   100014 | 5915 | `					PH7_MemObjStringAppend(&sElem,sName.zString,iSep);` |
|   100014 | 5916 | `					PH7_HashmapInsert(pCbMap,0,&sElem);` |
|   100014 | 5917 | `					PH7_MemObjRelease(&sElem);` |
|   100014 | 5918 | `					PH7_MemObjInit(pVm,&sElem);` |
|   100014 | 5919 | `					PH7_MemObjStringAppend(&sElem,&sName.zString[iSep+2],sName.nByte-(iSep+2));` |
|   100014 | 5920 | `					PH7_HashmapInsert(pCbMap,0,&sElem);` |
|   100014 | 5921 | `					PH7_MemObjRelease(&sElem);` |
|   100014 | 5922 | `					PH7_MemObjInit(pVm,&sCallable);` |
|   100014 | 5923 | `					sCallable.x.pOther = pCbMap;` |
|   100014 | 5924 | `					MemObjSetType(&sCallable,MEMOBJ_HASHMAP);` |
|   100014 | 5925 | `					PH7_MemObjInit(pVm,&sResult);` |
|   150020 | 5926 | `					rcSm = PH7_VmCallUserFunctionWithMap(pVm,&sCallable,(int)SySetUsed(&aArg),` |
|   100012 | 5927 | `						(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|   100014 | 5928 | `					SySetReset(&aArg);` |
|   100014 | 5929 | `					PH7_MemObjRelease(&sCallable);` |
|   100014 | 5930 | `					if( nCallArgs > 0 ){` |
|       13 | 5931 | `						VmPopOperand(&pTos,nCallArgs);` |
|        6 | 5932 | `					}` |
|   100014 | 5933 | `					if( rcSm == PH7_ABORT ){` |
|      ! 0 | 5934 | `						PH7_MemObjRelease(&sResult);` |
|      ! 0 | 5935 | `						goto Abort;` |
|        - | 5936 | `					}` |
|   100014 | 5937 | `					if( rcSm == PH7_EXCEPTION ){` |
|        - | 5938 | `						sxi32 iResumePc;` |
|   100001 | 5939 | `						PH7_MemObjRelease(&sResult);` |
|   100001 | 5940 | `						if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100001 | 5941 | `							PH7_MemObjRelease(pTos);` |
|        - | 5942 | `							/* Drain the abandoned outer-expression operands` |
|        - | 5943 | ``							 * (`1 + "C::m"()`) to the try's base — one leaked`` |
|        - | 5944 | `							 * slot per caught throw otherwise. */` |
|   300001 | 5945 | `							PH7_RESUME_DRAIN()` |
|   100001 | 5946 | `							pc = iResumePc;` |
|   100001 | 5947 | `							break;` |
|        - | 5948 | `						}` |
|      ! 0 | 5949 | `						goto Exception;` |
|        - | 5950 | `					}` |
|       13 | 5951 | `					PH7_MemObjStore(&sResult,pTos);` |
|       13 | 5952 | `					PH7_MemObjRelease(&sResult);` |
|       13 | 5953 | `					break;` |
|        - | 5954 | `				}` |
|      ! 0 | 5955 | `			}` |
|        - | 5956 | `			/* Call to an undefined function is a catchable Error in php 8 — it does` |
|        - | 5957 | `			 * NOT warn and hand back null and carry on, which is what PH7 did (and` |
|        - | 5958 | `			 * which quietly turned a typo into a null-propagating program). */` |
|        - | 5959 | `			{` |
|        - | 5960 | `			SyBlob sMsg;` |
|   140052 | 5961 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   140052 | 5962 | `			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);` |
|        - | 5963 | `			/* Consume this call's captured spread runs so they don't leak into a` |
|        - | 5964 | `			 * later call (this path never reaches VmBuildEffectiveArgMap). */` |
|   140052 | 5965 | `			if( pInstr->iP2 ){` |
|        3 | 5966 | `				VmSpreadConsume(pVm);` |
|        1 | 5967 | `			}` |
|        - | 5968 | `			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the` |
|        - | 5969 | `			 * compile-time arg count on the stack, and this early exit skips the` |
|        - | 5970 | `			 * arg-building loop that the normal path uses, so popping only iP1 would` |
|        - | 5971 | `			 * strand the expanded elements and corrupt the enclosing expression. */` |
|   140052 | 5972 | `			if( nCallArgs > 0 ){` |
|        5 | 5973 | `				VmPopOperand(&pTos,nCallArgs);` |
|        2 | 5974 | `			}` |
|   140052 | 5975 | `			PH7_MemObjRelease(pTos);` |
|   210076 | 5976 | `			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|    70024 | 5977 | `				SyBlobLength(&sMsg));` |
|   140052 | 5978 | `			SyBlobRelease(&sMsg);` |
|   140052 | 5979 | `			if( rc == SXERR_ABORT ){` |
|        6 | 5980 | `				goto Abort;` |
|        - | 5981 | `			}` |
|        - | 5982 | `			/* A catch may have run IN PLACE inside VmThrowFromVm (SXRET_OK +` |
|        - | 5983 | ``			 * recorded resume). An unconditional `goto Exception` here unwound the`` |
|        - | 5984 | `` 			 * exec ANYWAY, so `try { nosuchfn(); } catch (Error $e) {} rest();` `` |
|        - | 5985 | `			 * ran the catch and then silently dropped the rest of the script` |
|        - | 5986 | `			 * (exit 0). Route like every other in-exec throw site. */` |
|   380088 | 5987 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5988 | `			}` |
|        - | 5989 | `		}` |
|  3937088 | 5990 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 5991 | `		/* D1: resolve deferred plain-var arguments against the builtin's by-ref position` |
|        - | 5992 | `		 * mask (derived from its signature). A by-ref out-param (preg_match's $matches, …)` |
|        - | 5993 | `		 * is materialized so PH7_VmStoreArgByRef can write back — this is what keeps a` |
|        - | 5994 | ``		 * DYNAMIC-name by-ref builtin (`$f='preg_match'; $f($p,$s,$m)`) working now that the`` |
|        - | 5995 | `		 * compile-time mask no longer sees it. Every other (by-value) arg warns + passes` |
|        - | 5996 | `		 * NULL, which is also what call_user_func & friends want. pVm->pFrame is the caller. */` |
|        - | 5997 | `		{` |
|  3937088 | 5998 | `			sxi32 rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,pFunc->nByRefMask,0,0);` |
|  3937088 | 5999 | `			if( rcDA == PH7_ABORT ){` |
|      ! 0 | 6000 | `				goto Abort;` |
|  3937088 | 6001 | `			}else if( rcDA == PH7_EXCEPTION ){` |
|      ! 0 | 6002 | `				goto Exception;` |
|        - | 6003 | `			}` |
|        - | 6004 | `		}` |
|        - | 6005 | `		/* Host function (builtin): build the effective spread-key map so the` |
|        - | 6006 | `		 * name-forwarding builtins (call_user_func & friends) relay string keys as` |
|        - | 6007 | `		 * named args, and — critically — so this call's captured runs are consumed.` |
|        - | 6008 | `		 * pArg is the top base here (a builtin call pops no method-name slot). */` |
|  5906232 | 6009 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  3937083 | 6010 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6011 | `		/* Start collecting function arguments */` |
|  3937088 | 6012 | `		SySetReset(&aArg);` |
|  8615942 | 6013 | `		while( pArg < pTos ){` |
|  4678859 | 6014 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  4678859 | 6015 | `			pArg++;` |
|        5 | 6016 | `		}` |
|        - | 6017 | `		/* Assume a null return value */` |
|  3937088 | 6018 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 6019 | `		/* Init the call context */` |
|  3937088 | 6020 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 6021 | `		/* Hand the call-site named-argument map to the builtin so name-forwarding` |
|        - | 6022 | `		 * helpers (call_user_func & friends) can relay name: arguments — and the` |
|        - | 6023 | `		 * caller's strict_types mode — to the inner callback. Forwarded whole (not` |
|        - | 6024 | `		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even` |
|        - | 6025 | `		 * when its own call site is purely positional; only the two forwarding` |
|        - | 6026 | `		 * builtins read pArgMap, so this is inert for every other host function. */` |
|  3937088 | 6027 | `		sCtx.pArgMap = pEffCallMap;` |
|        - | 6028 | `		{` |
|  3937088 | 6029 | `		int nGiven = (int)SySetUsed(&aArg);` |
|        - | 6030 | `		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum` |
|        - | 6031 | `		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError` |
|        - | 6032 | `		 * before the C routine runs when called with too few arguments — instead` |
|        - | 6033 | `		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""` |
|        - | 6034 | `		 * return. The message wording matches php's ZPP output byte-for-byte. */` |
|  3937088 | 6035 | `		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){` |
|      704 | 6036 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 6037 | `				"%z() expects %s %d argument%s, %d given",` |
|      233 | 6038 | `				&pFunc->sName,` |
|      466 | 6039 | `				pFunc->bAtLeast ? "at least" : "exactly",` |
|      466 | 6040 | `				(int)pFunc->nMinArg,` |
|      466 | 6041 | `				pFunc->nMinArg == 1 ? "" : "s",` |
|      233 | 6042 | `				nGiven);` |
|  3936855 | 6043 | `		}else if( pFunc->bHasMaxArg && nGiven > (int)pFunc->nMaxArg ){` |
|        - | 6044 | `			/* php enforces the MAXIMUM as well, and PHL only did so where a` |
|        - | 6045 | `			 * builtin happened to hand-roll the check (51 of ~650), so` |
|        - | 6046 | ``			 * `microtime(1,2)`, `strlen("a","b")` and friends silently ignored`` |
|        - | 6047 | `			 * the extras. The count comes from the same signature table as the` |
|        - | 6048 | `			 * minimum; a variadic tail leaves nMaxArg at -1 and is exempt.` |
|        - | 6049 | `			 * php says "exactly" when the bounds coincide, "at most" otherwise. */` |
|      125 | 6050 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 6051 | `				"%z() expects %s %d argument%s, %d given",` |
|       40 | 6052 | `				&pFunc->sName,` |
|       68 | 6053 | `				((int)pFunc->nMinArg == (int)pFunc->nMaxArg && !pFunc->bAtLeast) ? "exactly" : "at most",` |
|       80 | 6054 | `				(int)pFunc->nMaxArg,` |
|       80 | 6055 | `				pFunc->nMaxArg == 1 ? "" : "s",` |
|       40 | 6056 | `				nGiven);` |
|  5905453 | 6057 | `		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,` |
|  3936537 | 6058 | `			(ph7_value **)SySetBasePtr(&aArg))) ){` |
|        - | 6059 | `			/* TypeError thrown: rc carries the caught/uncaught status */` |
|      114 | 6060 | `		}else{` |
|        - | 6061 | `			/* Call the foreign function */` |
|  3936324 | 6062 | `			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));` |
|        - | 6063 | `			/* A host function that RAISED a catchable throw (PH7_VmThrowException)` |
|        - | 6064 | `			 * and still returned PH7_OK reports it here — the throw's catch has` |
|        - | 6065 | `			 * already run in place, so treating the call as a normal return would` |
|        - | 6066 | `			 * resume execution INSIDE the try body the throw abandoned (and, when` |
|        - | 6067 | `			 * uncaught, run on past the reported fatal). The status is recorded on` |
|        - | 6068 | `			 * the call context, so this covers the shared validation helpers whose` |
|        - | 6069 | `			 * callers have no channel to thread a status back. */` |
|  3936324 | 6070 | `			rc = VmHostFuncThrowRc(&sCtx,rc);` |
|        - | 6071 | `		}` |
|        - | 6072 | `		}` |
|        - | 6073 | `		/* Release the call context */` |
|  3937088 | 6074 | `		VmReleaseCallContext(&sCtx);` |
|  3937088 | 6075 | `		if( rc == PH7_ABORT ){` |
|        - | 6076 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 6077 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 6078 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      588 | 6079 | `			PH7_MemObjRelease(&sRet);` |
|      588 | 6080 | `			goto Abort;` |
|        - | 6081 | `		}` |
|  3936504 | 6082 | `		if( rc != PH7_SUSPEND && pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 6083 | `			/* A throw raised inside this host function — directly` |
|        - | 6084 | `			 * (PH7_VmThrowException) or by a PHP callback it invoked — was` |
|        - | 6085 | `			 * caught by an INLINE try (generator body) THIS exec owns.` |
|        - | 6086 | `			 * VmThrowInline records only a pc-redirect: a direct builtin throw` |
|        - | 6087 | `			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION` |
|        - | 6088 | `			 * with the redirect pending, so the rc branches below never land` |
|        - | 6089 | `			 * it (pre-existing hole: explode("") or a throwing usort` |
|        - | 6090 | `			 * comparator inside a generator's try lost the catch AND the` |
|        - | 6091 | `			 * yield). Land at the redirect now — its drain to the try's` |
|        - | 6092 | `			 * operand base subsumes the args + name pops. */` |
|        7 | 6093 | `			PH7_MemObjRelease(&sRet);` |
|       25 | 6094 | `			PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 6095 | `		}` |
|  3936498 | 6096 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 6097 | `			/* A callback invoked by this host function threw. If an in-place catch` |
|        - | 6098 | `			 * recorded a resume target owned by THIS body, resume at its landing pad` |
|        - | 6099 | `			 * (consuming the target); otherwise the exception was caught by an outer` |
|        - | 6100 | `			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW` |
|        - | 6101 | `			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which` |
|        - | 6102 | `			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */` |
|        - | 6103 | `			sxi32 iResumePc;` |
|     5349 | 6104 | `			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 6105 | `				/* Caught by an outer exec, or not caught here: propagate. */` |
|      331 | 6106 | `				goto Exception;` |
|        - | 6107 | `			}` |
|        - | 6108 | `			/* Exception was caught in place by THIS body's try: pop args and the` |
|        - | 6109 | `			 * result slot, then drain any abandoned outer-expression operands to` |
|        - | 6110 | `			 * the try's base and resume. */` |
|     5023 | 6111 | `			PH7_MemObjRelease(&sRet);` |
|     5023 | 6112 | `			if( nCallArgs > 0 ){` |
|     4733 | 6113 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|     2364 | 6114 | `			}` |
|     5023 | 6115 | `			VmPopOperand(&pTos,1);` |
|     9023 | 6116 | `			PH7_RESUME_DRAIN()` |
|     5023 | 6117 | `			pc = iResumePc;` |
|     5023 | 6118 | `			break;` |
|        - | 6119 | `		}` |
|  3931154 | 6120 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 6121 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 6122 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 6123 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 6124 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 6125 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 6126 | `			 * body), the user-function path above will handle re-saving. */` |
|      359 | 6127 | `			PH7_MemObjRelease(&sRet);` |
|      359 | 6128 | `			if( nCallArgs > 0 ){` |
|      359 | 6129 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      177 | 6130 | `			}` |
|        - | 6131 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 6132 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|      359 | 6133 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|      359 | 6134 | `			goto Suspend;` |
|        - | 6135 | `		}` |
|  3930800 | 6136 | `		if( nCallArgs > 0 ){` |
|        - | 6137 | `			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack` |
|        - | 6138 | `			 * expanded the compile-time arg count on the stack, so popping iP1` |
|        - | 6139 | `			 * strands the extra elements (or, for an unpack that expanded to fewer` |
|        - | 6140 | `			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,` |
|        - | 6141 | `			 * reading a bogus value whose stray flags sent MemObjStore into the` |
|        - | 6142 | `			 * hashmap-release path and hung). Mirrors every other CALL exit. The` |
|        - | 6143 | `			 * function-name slot (pTos) receives the return value below. */` |
|  3909354 | 6144 | `			VmPopOperand(&pTos,nCallArgs);` |
|  1955277 | 6145 | `		}` |
|        - | 6146 | `		/* Save foreign function return value into the (now top) function-name slot */` |
|  3930800 | 6147 | `		PH7_MemObjStore(&sRet,pTos);` |
|  3930800 | 6148 | `		PH7_MemObjRelease(&sRet);` |
|        - | 6149 | `	}` |
|  3931008 | 6150 | `	break;` |
|        - | 6151 | `				  }` |
|        - | 6152 | `/*` |
|        - | 6153 | ` * OP_CONSUME: P1 * *` |
|        - | 6154 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 6155 | ` */` |
|    28621 | 6156 | `case PH7_OP_CONSUME: {` |
|        - | 6157 | `	VmOpRc rcOp;` |
|    57247 | 6158 | `	sState.pTos = pTos;` |
|    57247 | 6159 | `	sState.pc = pc;` |
|    57247 | 6160 | `	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);` |
|    57247 | 6161 | `	pTos = sState.pTos;` |
|    57247 | 6162 | `	pc = sState.pc;` |
|    57247 | 6163 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 6164 | `		goto Abort;` |
|    57247 | 6165 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 6166 | `		goto Exception;` |
|        - | 6167 | `	}` |
|    57242 | 6168 | `	break;` |
|        - | 6169 | `					  }` |
|        - | 6170 |  |
|        - | 6171 | `		} /* Switch() */` |
| 64808684 | 6172 | `		pc++; /* Next instruction in the stream */` |
|        5 | 6173 | `	} /* For(;;) */` |
|  5259262 | 6174 | `Done:` |
|        - | 6175 | `	/* A stacked callee completing lands here too (its result is already in` |
|        - | 6176 | `	 * sState.pResult — the caller's operand slot); Unwind's first iteration` |
|        - | 6177 | `	 * bottoms out identically for the record-less case. */` |
| 10518854 | 6178 | `	rc = SXRET_OK;` |
| 10518854 | 6179 | `	goto Unwind;` |
|      833 | 6180 | `Suspend:` |
|     1671 | 6181 | `	rc = PH7_SUSPEND;` |
|     1671 | 6182 | `	if( pCallTop != 0 ){` |
|        - | 6183 | `		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment` |
|        - | 6184 | `		 * instead of the lossy unwind. pc/nTos of the innermost activation were` |
|        - | 6185 | `		 * already saved into the ctx by VmSuspendCtx; capture the rest (the` |
|        - | 6186 | `		 * record chain, the innermost activation, the suspend-time top frame)` |
|        - | 6187 | `		 * so resume re-enters HERE, inside the innermost callee, like php. The` |
|        - | 6188 | `		 * records / frames / operand stacks stay alive — nothing is freed. Only` |
|        - | 6189 | `		 * fibers reach this (generators yield only at their body level, pCallTop` |
|        - | 6190 | `		 * == 0); a suspend inside a C->PHP callback was already rejected with a` |
|        - | 6191 | `		 * FiberError before it could arrive here. */` |
|      359 | 6192 | `		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));` |
|      359 | 6193 | `		if( pSeg == 0 ){` |
|        - | 6194 | `			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST` |
|        - | 6195 | `			 * pc/nTos into the ctx, so the lossy body-level fallback would resume` |
|        - | 6196 | `			 * the callee's pc against the body stack — silent corruption, exactly` |
|        - | 6197 | `			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the` |
|        - | 6198 | `			 * §3.1 convention shared with the stage-2 record-alloc OOM site). */` |
|      ! 0 | 6199 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 6200 | `			rc = PH7_ABORT;` |
|      ! 0 | 6201 | `			goto Unwind;` |
|        - | 6202 | `		}` |
|      359 | 6203 | `		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */` |
|      359 | 6204 | `		pSeg->sState = sState;` |
|      359 | 6205 | `		pSeg->pCallTop = pCallTop;` |
|      359 | 6206 | `		pSeg->pTopFrame = pVm->pFrame;` |
|      359 | 6207 | `		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;` |
|      359 | 6208 | `		pSeg->nOldFinBase = pVm->pActiveCtx ? pVm->pActiveCtx->nFinallyBase : 0;` |
|        - | 6209 | `		{` |
|        - | 6210 | `			VmCallFrame *pRec;` |
|      359 | 6211 | `			pSeg->nRecords = 0;` |
|     1015 | 6212 | `			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){` |
|      661 | 6213 | `				pSeg->nRecords++;` |
|      333 | 6214 | `			}` |
|        - | 6215 | `		}` |
|      359 | 6216 | `		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;` |
|        - | 6217 | `		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */` |
|      359 | 6218 | `		SySetRelease(&aArg);` |
|      359 | 6219 | `		return PH7_SUSPEND;` |
|        - | 6220 | `	}` |
|     1317 | 6221 | `	goto Unwind;` |
|      382 | 6222 | `Abort:` |
|      768 | 6223 | `	rc = PH7_ABORT;` |
|      768 | 6224 | `	goto Unwind;` |
|   300509 | 6225 | `Exception:` |
|   601023 | 6226 | `	rc = PH7_EXCEPTION;` |
|   601018 | 6227 | `	goto Unwind;` |
|  5560809 | 6228 | `Unwind:` |
|        - | 6229 | `	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration` |
|        - | 6230 | `	 * finishes the top record exactly as the old per-level native return did:` |
|        - | 6231 | `	 * for ABORT/EXCEPTION, first run what the popped activation's own` |
|        - | 6232 | `	 * Abort/Exception label used to do (clear its pending return, release its` |
|        - | 6233 | `	 * operands — a stacked activation never has bReturnPropagates set), then` |
|        - | 6234 | `	 * VmCallFinish routes in the restored caller (an in-place catch there` |
|        - | 6235 | `	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —` |
|        - | 6236 | `	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible` |
|        - | 6237 | `	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).` |
|        - | 6238 | `	 * At the bottom, VmExecFinalize hands the status to the native caller. */` |
|  5761536 | 6239 | `	for(;;){` |
| 11522752 | 6240 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|        - | 6241 | `			/* Drop any pending hook-RMW write-backs this activation armed — its` |
|        - | 6242 | `			 * statement is abandoned (only the innermost activation at throw time` |
|        - | 6243 | `			 * can own entries: the armed window spans exactly one instruction, so` |
|        - | 6244 | `			 * no OP_CALL record ever intervenes). */` |
|  1002590 | 6245 | `			while( SySetUsed(&pVm->aHookRmw) > 0` |
|  1002591 | 6246 | `			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){` |
|      ! 0 | 6247 | `				VmHookRmwDropTop(&(*pVm));` |
|      ! 0 | 6248 | `			}` |
|   501293 | 6249 | `		}` |
| 11522752 | 6250 | `		if( pCallTop == 0 ){` |
|  9342785 | 6251 | `			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);` |
|        - | 6252 | `		}` |
|  2179972 | 6253 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|   601519 | 6254 | `			VmClearFrameReturn(sState.pEntryFrame);` |
|   603379 | 6255 | `			while( pTos >= pStack ){` |
|     1865 | 6256 | `				PH7_MemObjRelease(pTos);` |
|     1865 | 6257 | `				pTos--;` |
|        5 | 6258 | `			}` |
|   300757 | 6259 | `		}` |
|  2179972 | 6260 | `		if( rc != PH7_SUSPEND ){` |
|        - | 6261 | `			/* The finishing callee's own leaked finally actions must not survive` |
|        - | 6262 | `			 * into the caller (whose next OP_END_FINALLY would mis-pop them). */` |
|  2179972 | 6263 | `			VmDiscardFinallyActions(&(*pVm),sState.nFinallyActBase);` |
|  1090146 | 6264 | `		}` |
|        - | 6265 | `		{` |
|  2179972 | 6266 | `			VmCallFrame *pRec = pCallTop;` |
|  2179972 | 6267 | `			sState = pRec->sCaller;` |
|  2179972 | 6268 | `			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);` |
|  2179972 | 6269 | `			pCallTop = pRec->pPrev;` |
|  2179972 | 6270 | `			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;` |
|  2179972 | 6271 | `			pVm->pIdleCallFrames = (void *)pRec;` |
|  2179972 | 6272 | `			aInstr = sState.aInstr;` |
|  2179972 | 6273 | `			pStack = sState.pStack;` |
|  2179972 | 6274 | `			pTos = sState.pTos;` |
|  2179972 | 6275 | `			pc = sState.pc;` |
|        - | 6276 | `		}` |
|  2179972 | 6277 | `		if( rc == PH7_OK ){` |
|  1779168 | 6278 | `			pc++; /* the loop-bottom increment this OP_CALL missed */` |
|  1779168 | 6279 | `			goto VmLoopFetch;` |
|        - | 6280 | `		}` |
|        5 | 6281 | `	}` |
|  4671572 | 6282 | `}` |
|        - | 6283 |  |
