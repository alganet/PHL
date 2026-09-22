# src/ph7/vm_exec.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3086/3536 lines (87.27%)

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
|        3 |   48 | `{` |
|      269 |   49 | `	ph7_value *pOld = *ppStack;` |
|      269 |   50 | `	sxu32 nOldCap = pState->nStackCap;` |
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
|      269 |   64 | `	nReq = nNeed + pState->nStackOrig;` |
|      269 |   65 | `	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */` |
|      ! 0 |   66 | `		nReq = SXU32_HIGH;` |
|      ! 0 |   67 | `	}` |
|        - |   68 | `	/* SyMemBackendRealloc's size argument is sxu32, so the byte count` |
|        - |   69 | `	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack` |
|        - |   70 | `	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and` |
|        - |   71 | `	 * the init loop below would run off it. If the true requirement exceeds the` |
|        - |   72 | `	 * representable cap, treat it as OOM (the caller raises the guard error). */` |
|      269 |   73 | `	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);` |
|      269 |   74 | `	if( nReq > nMaxCap ){` |
|      ! 0 |   75 | `		return 0;` |
|        - |   76 | `	}` |
|      269 |   77 | `	if( nReq <= nOldCap ){` |
|      161 |   78 | `		return 1; /* already fits — no growth needed */` |
|        - |   79 | `	}` |
|      111 |   80 | `	nNewCap = nReq;` |
|        - |   81 | `	/* Amortize repeated spreads in one argument list: at least double, but never` |
|        - |   82 | `	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't` |
|        - |   83 | `	 * itself overflow.) */` |
|      111 |   84 | `	if( nNewCap < nOldCap * 2 ){` |
|      111 |   85 | `		sxu32 nDbl = nOldCap * 2;` |
|      111 |   86 | `		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }` |
|      111 |   87 | `		if( nNewCap < nDbl ){ nNewCap = nDbl; }` |
|       54 |   88 | `	}` |
|      165 |   89 | `	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,` |
|       54 |   90 | `		nNewCap * sizeof(ph7_value));` |
|      111 |   91 | `	if( pNew == 0 ){` |
|      ! 0 |   92 | `		return 0; /* OOM: caller keeps pOld and raises the guard error */` |
|        - |   93 | `	}` |
|        - |   94 | `	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */` |
|     7031 |   95 | `	for( i = nOldCap; i < nNewCap; i++ ){` |
|     6923 |   96 | `		PH7_MemObjInit(pVm, &pNew[i]);` |
|     6923 |   97 | `		pNew[i].nIdx = SXU32_HIGH;` |
|     3463 |   98 | `	}` |
|        - |   99 | `	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */` |
|      111 |  100 | `	*ppTos = pNew + (*ppTos - pOld);` |
|      111 |  101 | `	*ppStack = pNew;` |
|      111 |  102 | `	pState->pStack = pNew + (pState->pStack - pOld);` |
|      111 |  103 | `	pState->pTos = pNew + (pState->pTos - pOld);` |
|      111 |  104 | `	pState->nStackCap = nNewCap;` |
|      111 |  105 | `	if( pCallTop ){` |
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
|      111 |  117 | `	nRun = SySetUsed(&pVm->aSpreadRun);` |
|      111 |  118 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      111 |  119 | `	for( i = 0; i < nRun; i++ ){` |
|      ! 0 |  120 | `		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){` |
|      ! 0 |  121 | `			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);` |
|      ! 0 |  122 | `		}` |
|      ! 0 |  123 | `	}` |
|      111 |  124 | `	return 1;` |
|      136 |  125 | `}` |
|        - |  126 | `/*` |
|        - |  127 | ` * Ensure the running activation's operand stack can hold a spread of nEntry` |
|        - |  128 | ` * elements pushed at *ppTos (the source slot becomes the first element, so the` |
|        - |  129 | ` * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.` |
|        - |  130 | ` * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.` |
|        - |  131 | ` */` |
|      304 |  132 | `static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,` |
|        - |  133 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |  134 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        3 |  135 | `{` |
|        - |  136 | `	sxu32 nNeed;` |
|      307 |  137 | `	if( nEntry == 0 ){` |
|       41 |  138 | `		return 1; /* empty spread never grows the stack */` |
|        - |  139 | `	}` |
|      269 |  140 | `	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);` |
|      402 |  141 | `	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,` |
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
|  9371268 |  164 | `static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)` |
|        5 |  165 | `{` |
|  9371273 |  166 | `	if( rcTerm != PH7_SUSPEND ){` |
|  9369883 |  167 | `		VmDiscardFinallyActions(&(*pVm),pState->nFinallyActBase);` |
|  4684939 |  168 | `	}` |
|  9371273 |  169 | `	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){` |
|  7920867 |  170 | `		VmClearFramePending(pState->pEntryFrame);` |
|  3960431 |  171 | `	}` |
|  9371273 |  172 | `	SySetRelease(pArg);` |
|  9371273 |  173 | `	if( rcTerm == PH7_ABORT \|\| rcTerm == PH7_EXCEPTION ){` |
|   803679 |  174 | `		while( pTos >= pState->pStack ){` |
|   402225 |  175 | `			PH7_MemObjRelease(pTos);` |
|   402225 |  176 | `			pTos--;` |
|        5 |  177 | `		}` |
|   200727 |  178 | `	}` |
|  9371273 |  179 | `	return rcTerm;` |
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
| 11571464 |  195 | `static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase)` |
|        5 |  196 | `{` |
| 11571479 |  197 | `	while( SySetUsed(&pVm->aFinallyAction) > nBase ){` |
|       13 |  198 | `		VmFinallyAction *pAct = (VmFinallyAction *)SySetPeek(&pVm->aFinallyAction);` |
|       13 |  199 | `		if( pAct->eKind == PH7_FA_RETHROW && pAct->pExc ){` |
|        7 |  200 | `			PH7_ClassInstanceUnref(pAct->pExc);` |
|        9 |  201 | `		}else if( pAct->eKind == PH7_FA_RETURN ){` |
|        3 |  202 | `			PH7_MemObjRelease(&pAct->sRet);` |
|        1 |  203 | `		}` |
|       13 |  204 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|        3 |  205 | `	}` |
| 11571469 |  206 | `}` |
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
|  2202060 |  219 | `static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)` |
|        5 |  220 | `{` |
|        - |  221 | `	ph7_value *pObj;` |
|        - |  222 | `	/* Decrement nesting level */` |
|  2202065 |  223 | `	pVm->nRecursionDepth--;` |
|  2202065 |  224 | `	if( pCallee->bSelfPushed ){` |
|        - |  225 | `		/* Pop class name */` |
|  1971227 |  226 | `		(void)SySetPop(&pVm->aSelf);` |
|   985611 |  227 | `	}` |
|  2202065 |  228 | `	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  229 | `		/* Return by reference,reflect that */` |
|       55 |  230 | `		if( pCallee->nLastRef != SXU32_HIGH ){` |
|       55 |  231 | `			VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pCallee->pFrame->sLocal);` |
|        - |  232 | `			sxu32 i;` |
|        - |  233 | `			/* Make sure the referenced object is not a local variable */` |
|      105 |  234 | `			for( i = 0 ; i < SySetUsed(&pCallee->pFrame->sLocal) ; ++i ){` |
|       53 |  235 | `				if( pCallee->nLastRef == aSlot[i].nIdx ){` |
|      ! 0 |  236 | `					pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pCallee->nLastRef);` |
|      ! 0 |  237 | `					if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  238 | `						VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  239 | `							"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  240 | `							&pCallee->pVmFunc->sName);` |
|      ! 0 |  241 | `					}` |
|      ! 0 |  242 | `					pCallee->nLastRef = SXU32_HIGH;` |
|      ! 0 |  243 | `					break;` |
|        - |  244 | `				}` |
|       28 |  245 | `			}` |
|       29 |  246 | `		}else{` |
|      ! 0 |  247 | `			if( (pCaller->pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  248 | `				VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  249 | `					"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  250 | `					&pCallee->pVmFunc->sName);` |
|      ! 0 |  251 | `			}` |
|        - |  252 | `		}` |
|       55 |  253 | `		pCaller->pTos->nIdx = pCallee->nLastRef;` |
|       26 |  254 | `	}` |
|  2202065 |  255 | `	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  256 | `		/* The callee threw (or its finally threw past it). If an in-place catch` |
|        - |  257 | `		 * recorded a resume target owned by THIS caller's body, resume there and` |
|        - |  258 | `		 * consume the target (VmRecordedResume); when the catcher is an outer exec` |
|        - |  259 | `		 * — or this is a callback with no bytecode to resume into — propagate so` |
|        - |  260 | `		 * the owning exec lands. This replaces the old "is the caller's parent a` |
|        - |  261 | `		 * resumable try frame" test, which resumed at the caller's OWN try even` |
|        - |  262 | `		 * when the finally's throw was caught further out, losing that catch's` |
|        - |  263 | `		 * return (ROOT B, face c). */` |
|        - |  264 | `		sxi32 iResumePc;` |
|   602719 |  265 | `		VmFrame *pParentFrame = pCallee->pFrame->pParent;` |
|   602719 |  266 | `		if( !pCaller->is_callback && pVm->pInlineInstr == (void *)pCaller->aInstr ){` |
|        - |  267 | `			/* ROOT C: the callee's throw was caught by an inline try in THIS caller` |
|        - |  268 | `			 * (generator body). Drain the operand stack (incl. the unwritten result` |
|        - |  269 | `			 * slot) to the try's base and land at its catch/finally. */` |
|      ! 0 |  270 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iInlineDrain ){` |
|      ! 0 |  271 | `				PH7_MemObjRelease(pCaller->pTos);` |
|      ! 0 |  272 | `				pCaller->pTos--;` |
|      ! 0 |  273 | `			}` |
|      ! 0 |  274 | `			pCaller->pc = (sxi32)pVm->iInlinePc - 1;` |
|      ! 0 |  275 | `			pVm->pInlineInstr = 0;` |
|      ! 0 |  276 | `			rc = PH7_OK;` |
|   602719 |  277 | `		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){` |
|        - |  278 | `			/* Pop the result, then drain any abandoned outer-expression operands` |
|        - |  279 | `			 * to the catching try's base (like the inline branch above) — the` |
|        - |  280 | `			 * ops that would have consumed them were abandoned by the throw, and` |
|        - |  281 | `` 			 * leaving them leaks one slot per caught throw (`try { $a = 1 + f(); }` `` |
|        - |  282 | `			 * in a loop overflowed the operand stack). */` |
|   201873 |  283 | `			VmPopOperand(&pCaller->pTos,1);` |
|   802049 |  284 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iResumeStackDepth ){` |
|   600181 |  285 | `				PH7_MemObjRelease(pCaller->pTos);` |
|   600181 |  286 | `				pCaller->pTos--;` |
|        5 |  287 | `			}` |
|   201873 |  288 | `			pCaller->pc = iResumePc;` |
|   201873 |  289 | `			rc = PH7_OK;` |
|   100939 |  290 | `		}else{` |
|   400851 |  291 | `			if( pParentFrame->pParent ){` |
|   400847 |  292 | `				rc = PH7_EXCEPTION;` |
|   200426 |  293 | `			}else{` |
|        - |  294 | `				/* Continue normal execution */` |
|        6 |  295 | `				rc = PH7_OK;` |
|        - |  296 | `			}` |
|        - |  297 | `		}` |
|   301357 |  298 | `	}` |
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
|  2202065 |  309 | `	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){` |
|        - |  310 | `		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee` |
|        - |  311 | `		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly` |
|        - |  312 | `		 * the allocated slot count either way. */` |
|  2201591 |  313 | `		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap);` |
|  1100991 |  314 | `	}` |
|        - |  315 | `	/* Leave the frame */` |
|  2202065 |  316 | `	VmLeaveFrame(&(*pVm));` |
|  2202065 |  317 | `	if( rc == PH7_ABORT ){` |
|      330 |  318 | `		return PH7_ABORT;` |
|        - |  319 | `	}` |
|  2201739 |  320 | `	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  321 | `		/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  322 | `		 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  323 | `		 * overwriting the state saved by the inner level.` |
|        - |  324 | `		 * pTos points to the result slot (not yet written).` |
|        - |  325 | `		 * Save nTos one below so resume pushes at the result slot. */` |
|      ! 0 |  326 | `		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);` |
|      ! 0 |  327 | `		return PH7_SUSPEND;` |
|        - |  328 | `	}` |
|  2201739 |  329 | `	if( rc == PH7_EXCEPTION ){` |
|   400847 |  330 | `		return PH7_EXCEPTION;` |
|        - |  331 | `	}` |
|  1800897 |  332 | `	return PH7_OK;` |
|  1101233 |  333 | `}` |
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
|  9371574 |  369 | `PH7_PRIVATE sxi32 VmByteCodeExec(` |
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
|  9371579 |  389 | `	if( VmNativeNestingExceeded(pVm) ){` |
|        6 |  390 | `		return VmNativeNestingFatal(pVm);` |
|        - |  391 | `	}` |
|        - |  392 | `	/* A fresh native exec entered while a C-boundary throw is parked` |
|        - |  393 | `	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the` |
|        - |  394 | `	 * very opcode that swallowed the throw) must run CLEAN: the parked status` |
|        - |  395 | `	 * belongs to the interrupted outer exec's fetch-point router, not to this` |
|        - |  396 | `	 * one. Save+clear on entry, merge back on exit — the outer status is` |
|        - |  397 | `	 * restored unless this exec parked its own unconsumed (newer) one, with` |
|        - |  398 | `	 * PH7_ABORT dominating either way. */` |
|  9371575 |  399 | `	nSavedBrc = pVm->nBoundaryRc;` |
|  9371575 |  400 | `	pVm->nBoundaryRc = 0;` |
|        - |  401 | `	/* The executing source line belongs to the ACTIVATION. A nested body -- a called` |
|        - |  402 | `	 * function, but equally an attribute-default or default-argument mini-program --` |
|        - |  403 | `	 * runs its own bytecode with its own lines, so it must not leave the caller` |
|        - |  404 | ``	 * reporting the callee's position: `new Exception` stamped line 1 because the`` |
|        - |  405 | ``	 * class's `protected $message = '';` default ran (from the embedded chunk) between`` |
|        - |  406 | `	 * OP_NEW and the stamp. Save on entry, restore on exit. */` |
|  9371575 |  407 | `	nSavedLine = pVm->nCurLine;` |
|  9371575 |  408 | `	pVm->nVmExecDepth++;` |
| 14057360 |  409 | `	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,` |
|  4685785 |  410 | `		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);` |
|  9371575 |  411 | `	pVm->nVmExecDepth--;` |
|  9371575 |  412 | `	pVm->nCurLine = nSavedLine;` |
|  9371575 |  413 | `	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT \|\| pVm->nBoundaryRc == 0) ){` |
|       24 |  414 | `		pVm->nBoundaryRc = nSavedBrc;` |
|       10 |  415 | `	}` |
|  9371575 |  416 | `	return rc;` |
|  4685792 |  417 | `}` |
|        - |  418 | `/*` |
|        - |  419 | ` * D1 commit 2: allocate a captured lvalue path for a deferred element/property call arg.` |
|        - |  420 | ` * eRoot picks the root container: 0 = a real aMemObj slot (nRootIdx), 1 = an undefined` |
|        - |  421 | ` * variable to vivify by name at resolve time (pName, a VM-lifetime bytecode string, borrowed),` |
|        - |  422 | ` * 2 = a string base (subscripting a string -> php refuses a by-ref bind). The struct owns its` |
|        - |  423 | ` * step array and each step's key/name; PH7_MemObjRelease frees it via VmFreeDeferredPath.` |
|        - |  424 | ` */` |
|    38515 |  425 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName)` |
|        5 |  426 | `{` |
|    38520 |  427 | `	VmDeferredPath *pPath = (VmDeferredPath *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDeferredPath));` |
|    38520 |  428 | `	if( pPath == 0 ){` |
|      ! 0 |  429 | `		return 0;` |
|        - |  430 | `	}` |
|    38520 |  431 | `	SyZero(pPath,sizeof(VmDeferredPath));` |
|    38520 |  432 | `	pPath->pAlloc = &pVm->sAllocator;` |
|    38520 |  433 | `	pPath->eRoot = eRoot;` |
|    38520 |  434 | `	pPath->nRootIdx = nRootIdx;` |
|    38520 |  435 | `	if( eRoot == 1 && pName ){` |
|        3 |  436 | `		pPath->sRootName = *pName; /* borrowed VM-lifetime bytes, not copied */` |
|        1 |  437 | `	}` |
|    38520 |  438 | `	return pPath;` |
|    19413 |  439 | `}` |
|    38533 |  440 | `static VmDeferStep * VmDeferPathGrow(VmDeferredPath *pPath)` |
|        5 |  441 | `{` |
|    38538 |  442 | `	if( pPath->nStep >= pPath->nAlloc ){` |
|    38520 |  443 | `		sxu32 nNew = pPath->nAlloc ? pPath->nAlloc * 2 : 4;` |
|    57928 |  444 | `		VmDeferStep *aNew = (VmDeferStep *)SyMemBackendRealloc(pPath->pAlloc,pPath->aStep,` |
|    19408 |  445 | `			nNew * sizeof(VmDeferStep));` |
|    38520 |  446 | `		if( aNew == 0 ){` |
|      ! 0 |  447 | `			return 0;` |
|        - |  448 | `		}` |
|    38520 |  449 | `		pPath->aStep = aNew;` |
|    38520 |  450 | `		pPath->nAlloc = nNew;` |
|    19408 |  451 | `	}` |
|    38538 |  452 | `	return &pPath->aStep[pPath->nStep];` |
|    19422 |  453 | `}` |
|        - |  454 | `/* Append an array-element step, deep-copying the index value (the caller releases pKey). */` |
|    38433 |  455 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey)` |
|        5 |  456 | `{` |
|    38438 |  457 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|    38438 |  458 | `	if( pStep == 0 ){` |
|      ! 0 |  459 | `		return SXERR_MEM;` |
|        - |  460 | `	}` |
|    38438 |  461 | `	pStep->isProp = 0;` |
|    38438 |  462 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|    38438 |  463 | `	PH7_MemObjInit(pKey->pVm,&pStep->sKey);` |
|    38438 |  464 | `	PH7_MemObjStore(pKey,&pStep->sKey);` |
|    38438 |  465 | `	pPath->nStep++;` |
|    38438 |  466 | `	return SXRET_OK;` |
|    19372 |  467 | `}` |
|        - |  468 | `/* Append an object-property step, owning a private copy of the name bytes. */` |
|      100 |  469 | `PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName)` |
|        3 |  470 | `{` |
|      103 |  471 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|        - |  472 | `	char *zCopy;` |
|      103 |  473 | `	if( pStep == 0 ){` |
|      ! 0 |  474 | `		return SXERR_MEM;` |
|        - |  475 | `	}` |
|      103 |  476 | `	zCopy = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|      103 |  477 | `	if( zCopy == 0 ){` |
|      ! 0 |  478 | `		return SXERR_MEM;` |
|        - |  479 | `	}` |
|      103 |  480 | `	pStep->isProp = 1;` |
|      103 |  481 | `	pStep->zProp = zCopy;` |
|      103 |  482 | `	SyStringInitFromBuf(&pStep->sProp,zCopy,pName->nByte);` |
|      103 |  483 | `	pPath->nStep++;` |
|      103 |  484 | `	return SXRET_OK;` |
|       53 |  485 | `}` |
|        - |  486 | `/* Release a captured lvalue path and everything it owns (element keys, property names). */` |
|        - |  487 | `/*` |
|        - |  488 | `` * The pending offset of a `$s[k] ??= v`: a heap copy of the RAW key, owned by the`` |
|        - |  489 | ` * peek's MEMOBJ_AUX_COALSTROFF result on the operand stack. One carrier per` |
|        - |  490 | `` * pending ??=, so `$s[9] ??= ($t[9] ??= "q")` nests — a single VM-wide slot could`` |
|        - |  491 | ` * not (the inner peek overwrote the outer's offset, and the outer store then` |
|        - |  492 | ` * replaced the whole string).` |
|        - |  493 | ` */` |
|       52 |  494 | `PH7_PRIVATE VmCoalStrOff * VmCoalStrOffNew(ph7_vm *pVm,ph7_value *pKey)` |
|        1 |  495 | `{` |
|       53 |  496 | `	VmCoalStrOff *pCoal = (VmCoalStrOff *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmCoalStrOff));` |
|       53 |  497 | `	if( pCoal == 0 ){` |
|      ! 0 |  498 | `		return 0;` |
|        - |  499 | `	}` |
|       53 |  500 | `	pCoal->pAlloc = &pVm->sAllocator;` |
|       53 |  501 | `	PH7_MemObjInit(&(*pVm),&pCoal->sKey);` |
|       53 |  502 | `	if( pKey ){` |
|       53 |  503 | `		PH7_MemObjStore(pKey,&pCoal->sKey);` |
|       26 |  504 | `	}` |
|       53 |  505 | `	return pCoal;` |
|       27 |  506 | `}` |
|       86 |  507 | `PH7_PRIVATE void VmFreeCoalStrOff(VmCoalStrOff *pCoal)` |
|        4 |  508 | `{` |
|        - |  509 | `	SyMemBackend *pAlloc;` |
|       90 |  510 | `	if( pCoal == 0 ){` |
|       38 |  511 | `		return;` |
|        - |  512 | `	}` |
|       53 |  513 | `	pAlloc = pCoal->pAlloc;` |
|       53 |  514 | `	PH7_MemObjRelease(&pCoal->sKey);` |
|       53 |  515 | `	SyMemBackendFree(pAlloc,pCoal);` |
|       47 |  516 | `}` |
|    38515 |  517 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath)` |
|        5 |  518 | `{` |
|        - |  519 | `	sxu32 i;` |
|    38520 |  520 | `	if( pPath == 0 ){` |
|      ! 0 |  521 | `		return;` |
|        - |  522 | `	}` |
|    77053 |  523 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|    38538 |  524 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|    38538 |  525 | `		if( pStep->isProp ){` |
|      103 |  526 | `			if( pStep->zProp ){` |
|      103 |  527 | `				SyMemBackendFree(pPath->pAlloc,pStep->zProp);` |
|       50 |  528 | `			}` |
|       53 |  529 | `		}else{` |
|    38438 |  530 | `			PH7_MemObjRelease(&pStep->sKey);` |
|        - |  531 | `		}` |
|    19422 |  532 | `	}` |
|    38520 |  533 | `	if( pPath->aStep ){` |
|    38520 |  534 | `		SyMemBackendFree(pPath->pAlloc,pPath->aStep);` |
|    19408 |  535 | `	}` |
|    38520 |  536 | `	SyMemBackendFree(pPath->pAlloc,pPath);` |
|    19413 |  537 | `}` |
|        - |  538 | `/* Map an op-handler VmOpRc into the main-loop rc convention used by VmByteCodeExecBody. */` |
|    38523 |  539 | `static sxi32 VmOpRcToExecRc(VmOpRc rcOp)` |
|        5 |  540 | `{` |
|    38528 |  541 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 |  542 | `		return PH7_ABORT;` |
|        - |  543 | `	}` |
|    38526 |  544 | `	if( rcOp == VM_OP_EXCEPTION ){` |
|       12 |  545 | `		return PH7_EXCEPTION;` |
|        - |  546 | `	}` |
|    38516 |  547 | `	return SXRET_OK;` |
|    19417 |  548 | `}` |
|        - |  549 | `/*` |
|        - |  550 | ` * D1 commit 2: re-drive ONE captured lvalue step by invoking the real LOAD_IDX / MEMBER` |
|        - |  551 | ` * handler on a synthetic 2-slot stack. This reuses the proven COW / vivify / warning / magic` |
|        - |  552 | ` * machinery instead of hand-walking it. pBase carries the current container (its nIdx must be` |
|        - |  553 | ` * a real aMemObj slot for a by-ref write to vivify in place). iP2 selects the mode:` |
|        - |  554 | ` * LOAD_IDX 1=write(vivify,by-ref) / 0=read(by-value); MEMBER PH7_MEMBER_READ=by-value read.` |
|        - |  555 | ` * On success pOut receives the result value and its nIdx (the aliasable element slot for a` |
|        - |  556 | ` * vivified by-ref element).` |
|        - |  557 | ` */` |
|    38523 |  558 | `static sxi32 VmReDriveStep(ph7_vm *pVm,sxi32 iOp,sxu32 iP2,ph7_value *pBase,ph7_value *pKey,ph7_value *pOut)` |
|        5 |  559 | `{` |
|        - |  560 | `	ph7_value mini[2];` |
|        - |  561 | `	VmInstr aI[2];` |
|        - |  562 | `	VmExecState st;` |
|        - |  563 | `	VmOpRc rcOp;` |
|    38528 |  564 | `	PH7_MemObjInit(pVm,&mini[0]);` |
|    38528 |  565 | `	PH7_MemObjInit(pVm,&mini[1]);` |
|    38528 |  566 | `	PH7_MemObjLoad(pBase,&mini[0]);` |
|    38528 |  567 | `	mini[0].nIdx = pBase->nIdx;` |
|    38528 |  568 | `	PH7_MemObjStore(pKey,&mini[1]);` |
|    38528 |  569 | `	SyZero((void *)aI,sizeof(aI));` |
|        - |  570 | `	/* LOAD_IDX: iP1=1 means "an index is present". MEMBER: iP1=0 means an INSTANCE member` |
|        - |  571 | ``	 * (iP1=1 would be a static `::` access). */`` |
|    38528 |  572 | `	aI[0].iOp = (sxu8)iOp; aI[0].iP1 = (iOp == PH7_OP_LOAD_IDX) ? 1 : 0; aI[0].iP2 = iP2;` |
|    38528 |  573 | `	SyZero((void *)&st,sizeof(st));` |
|    38528 |  574 | `	st.pStack = mini; st.pTos = &mini[1]; st.aInstr = aI; st.pc = 0;` |
|    38528 |  575 | `	if( iOp == PH7_OP_LOAD_IDX ){` |
|    38432 |  576 | `		rcOp = VmExecOpLoadIdx(&(*pVm),&st,&aI[0]);` |
|    19369 |  577 | `	}else{` |
|       99 |  578 | `		rcOp = VmExecOpMember(&(*pVm),&st,&aI[0]);` |
|        - |  579 | `	}` |
|        - |  580 | `	/* The handler popped the key/name and left the result in the (now top) base slot.` |
|        - |  581 | `	 * DEEP-COPY it into pOut (MemObjStore, not MemObjLoad): the result is chained as the next` |
|        - |  582 | `	 * step's base and must OWN its buffer — mini[0] is released immediately below, and a` |
|        - |  583 | `	 * read-only view (MemObjLoad) would leave pOut dangling into freed memory. */` |
|    38528 |  584 | `	PH7_MemObjStore(st.pTos,pOut);` |
|    38528 |  585 | `	pOut->nIdx = st.pTos->nIdx;` |
|    38528 |  586 | `	PH7_MemObjRelease(&mini[0]);` |
|    38528 |  587 | `	return VmOpRcToExecRc(rcOp);` |
|        5 |  588 | `}` |
|        - |  589 | `/*` |
|        - |  590 | ` * D1 commit 2: resolve an object property as a by-ref target. Given the object's aMemObj` |
|        - |  591 | ` * slot, return the property value's slot index in *pnOut so the by-ref binder can alias it.` |
|        - |  592 | ` * A present property binds directly; a missing one is created (recreate a declared+unset` |
|        - |  593 | ` * property, or a dynamic property on a dynamic-allowing class); a magic __get/__set property` |
|        - |  594 | ` * emits php's Notice and does NOT bind (*pbNoBind). Mirrors VmExecOpMember's write-create.` |
|        - |  595 | ` */` |
|        4 |  596 | `static sxi32 VmBindPropByRef(ph7_vm *pVm,sxu32 nObjIdx,const SyString *pName,sxu32 *pnOut,int *pbNoBind)` |
|        2 |  597 | `{` |
|        6 |  598 | `	ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|        - |  599 | `	ph7_class_instance *pThis;` |
|        - |  600 | `	ph7_class *pClass;` |
|        - |  601 | `	SyHashEntry *pEntry;` |
|        6 |  602 | `	VmClassAttr *pAttr = 0;` |
|        6 |  603 | `	*pbNoBind = 0;` |
|        6 |  604 | `	if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  605 | `		/* Base is not an object (e.g. a NULL intermediate): cannot bind a property by ref. */` |
|      ! 0 |  606 | `		*pbNoBind = 1;` |
|      ! 0 |  607 | `		return SXRET_OK;` |
|        - |  608 | `	}` |
|        6 |  609 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|        6 |  610 | `	pClass = pThis->pClass;` |
|        6 |  611 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|        6 |  612 | `	if( pEntry ){` |
|        3 |  613 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        3 |  614 | `		if( (pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      ! 0 |  615 | `			*pnOut = pAttr->nIdx;` |
|      ! 0 |  616 | `			return SXRET_OK;` |
|        - |  617 | `		}` |
|        - |  618 | `		/* A static property is the CLASS's: php does not find it through an` |
|        - |  619 | ``		 * instance, so binding `f($o->s)` by reference must not hand out the`` |
|        - |  620 | `		 * class slot — that let a by-ref callee overwrite shared class state` |
|        - |  621 | `		 * through an object, and with no diagnostic at all (the value pass that` |
|        - |  622 | `		 * carries the notice at the fetch site never runs for a by-ref arg).` |
|        - |  623 | `		 * Notice here and fall through to the missing-property handling. */` |
|        4 |  624 | `		if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->pAttr->sName,` |
|        2 |  625 | `			pAttr->pAttr->iProtection,FALSE) ){` |
|        4 |  626 | `			VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  627 | `				"Accessing static property %z::$%z as non static",` |
|        1 |  628 | `				&pClass->sName,pName);` |
|        1 |  629 | `		}` |
|        3 |  630 | `		pAttr = 0;` |
|        1 |  631 | `	}` |
|        4 |  632 | `	if( PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|        6 |  633 | `	 \|\| PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) ){` |
|        - |  634 | `		/* Overloaded (magic) property: php passes it by-value with a Notice and drops the` |
|        - |  635 | `		 * write-back — "has no effect". */` |
|      ! 0 |  636 | `		VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  637 | `			"Indirect modification of overloaded property %z::$%z has no effect",` |
|      ! 0 |  638 | `			&pClass->sName,pName);` |
|      ! 0 |  639 | `		*pbNoBind = 1;` |
|      ! 0 |  640 | `		return SXRET_OK;` |
|        - |  641 | `	}` |
|        - |  642 | `	{` |
|        6 |  643 | `		ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);` |
|        6 |  644 | `		if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      ! 0 |  645 | `			VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pAttr);` |
|        6 |  646 | `		}else if( VmClassAllowsDynamicProps(&(*pVm),pClass) ){` |
|        3 |  647 | `			PH7_VmCreateDynamicAttr(&(*pVm),pThis,pName->zString,pName->nByte,&pAttr);` |
|        2 |  648 | `		}else{` |
|        - |  649 | `			SyBlob sMsg;` |
|        - |  650 | `			sxi32 rcT;` |
|        3 |  651 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 |  652 | `			SyBlobFormat(&sMsg,"Cannot create dynamic property %z::$%z",&pClass->sName,pName);` |
|        3 |  653 | `			rcT = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        3 |  654 | `			SyBlobRelease(&sMsg);` |
|        3 |  655 | `			*pbNoBind = 1;` |
|        3 |  656 | `			return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  657 | `		}` |
|        3 |  658 | `		if( pAttr ){` |
|        3 |  659 | `			*pnOut = pAttr->nIdx;` |
|        2 |  660 | `		}else{` |
|      ! 0 |  661 | `			*pbNoBind = 1;` |
|        - |  662 | `		}` |
|        - |  663 | `	}` |
|        3 |  664 | `	return SXRET_OK;` |
|        4 |  665 | `}` |
|        - |  666 | `/* Resolve a captured lvalue path as a BY-REF target: vivify the whole chain in place and` |
|        - |  667 | ` * leave pSlot->nIdx pointing at the terminal (aliasable) slot for the by-ref binder. */` |
|       24 |  668 | `static sxi32 VmResolvePathByRef(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        3 |  669 | `{` |
|        - |  670 | `	sxu32 nCur;` |
|        - |  671 | `	sxu32 i;` |
|        - |  672 | `	sxi32 rc;` |
|       27 |  673 | `	if( pPath->eRoot == 2 ){` |
|        - |  674 | `		/* Subscripting a string: php refuses a by-ref bind to a string offset — but` |
|        - |  675 | ``		 * it applies its OFFSET rules first, so `f($s["p"])` is the offset TypeError`` |
|        - |  676 | ``		 * and `f($s[1.5])` warns about the cast before this Error is raised. */`` |
|        - |  677 | `		sxi32 rcT;` |
|        8 |  678 | `		if( pPath->nStep > 0 && !pPath->aStep[0].isProp ){` |
|        - |  679 | `			SyBlob sTypeMsg;` |
|        8 |  680 | `			sxi64 iOfft = 0;` |
|        6 |  681 | `			if( VmStringOffsetResolve(&(*pVm),&pPath->aStep[0].sKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg)` |
|        5 |  682 | `				== VM_STROFF_REJECT ){` |
|        3 |  683 | `				rcT = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|        3 |  684 | `				return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  685 | `			}` |
|        2 |  686 | `		}` |
|        6 |  687 | `		rcT = VmThrowFromVm(&(*pVm),"Error",` |
|        - |  688 | `			"Cannot create references to/from string offsets",` |
|        - |  689 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|        6 |  690 | `		return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  691 | `	}` |
|       20 |  692 | `	if( pPath->eRoot == 1 ){` |
|        3 |  693 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,TRUE); /* vivify $a */` |
|        3 |  694 | `		if( pRoot == 0 ){` |
|      ! 0 |  695 | `			return SXRET_OK;` |
|        - |  696 | `		}` |
|        3 |  697 | `		nCur = pRoot->nIdx;` |
|        2 |  698 | `	}else{` |
|       18 |  699 | `		nCur = pPath->nRootIdx;` |
|        - |  700 | `	}` |
|       38 |  701 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|       22 |  702 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|       22 |  703 | `		if( pStep->isProp ){` |
|        6 |  704 | `			sxu32 nOut = SXU32_HIGH;` |
|        6 |  705 | `			int bNoBind = 0;` |
|        6 |  706 | `			rc = VmBindPropByRef(&(*pVm),nCur,&pStep->sProp,&nOut,&bNoBind);` |
|        6 |  707 | `			if( rc != SXRET_OK ){` |
|        3 |  708 | `				return rc;` |
|        - |  709 | `			}` |
|        3 |  710 | `			if( bNoBind ){` |
|      ! 0 |  711 | `				return SXRET_OK; /* magic/non-object: leave the slot a clean NULL, pass by value */` |
|        - |  712 | `			}` |
|        3 |  713 | `			nCur = nOut;` |
|        2 |  714 | `		}else{` |
|        - |  715 | `			ph7_value out;` |
|       17 |  716 | `			ph7_value *pContainer = (ph7_value *)SySetAt(&pVm->aMemObj,nCur);` |
|       17 |  717 | `			if( pContainer == 0 ){` |
|      ! 0 |  718 | `				return SXRET_OK;` |
|        - |  719 | `			}` |
|       17 |  720 | `			PH7_MemObjInit(&(*pVm),&out);` |
|       17 |  721 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,1,pContainer,&pStep->sKey,&out);` |
|       17 |  722 | `			nCur = out.nIdx;` |
|       17 |  723 | `			PH7_MemObjRelease(&out);` |
|       17 |  724 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  725 | `				return rc;` |
|        - |  726 | `			}` |
|       17 |  727 | `			if( nCur == SXU32_HIGH ){` |
|      ! 0 |  728 | `				return SXRET_OK; /* no aliasable slot (e.g. a non-lvalue container): pass by value */` |
|        - |  729 | `			}` |
|        - |  730 | `		}` |
|       10 |  731 | `	}` |
|       17 |  732 | `	pSlot->nIdx = nCur;` |
|       17 |  733 | `	return SXRET_OK;` |
|       15 |  734 | `}` |
|        - |  735 | `/* Resolve a captured lvalue path as a BY-VALUE argument: read the chain (emitting php's` |
|        - |  736 | ` * undefined-key/property/offset warnings) WITHOUT vivifying, leaving the terminal value in` |
|        - |  737 | ` * pSlot. Re-drives the read handlers so the warning sequence matches php exactly. */` |
|    38491 |  738 | `static sxi32 VmResolvePathByValue(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        5 |  739 | `{` |
|        - |  740 | `	ph7_value cur;` |
|        - |  741 | `	sxu32 i;` |
|    38496 |  742 | `	sxi32 rc = SXRET_OK;` |
|    38496 |  743 | `	PH7_MemObjInit(&(*pVm),&cur);` |
|    38496 |  744 | `	if( pPath->eRoot == 1 ){` |
|      ! 0 |  745 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,FALSE);` |
|      ! 0 |  746 | `		if( pRoot == 0 ){` |
|      ! 0 |  747 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&pPath->sRootName);` |
|      ! 0 |  748 | `		}else{` |
|      ! 0 |  749 | `			PH7_MemObjLoad(pRoot,&cur);` |
|      ! 0 |  750 | `			cur.nIdx = pRoot->nIdx;` |
|        - |  751 | `		}` |
|      ! 0 |  752 | `	}else{` |
|    38496 |  753 | `		ph7_value *pRoot = (ph7_value *)SySetAt(&pVm->aMemObj,pPath->nRootIdx);` |
|    38496 |  754 | `		if( pRoot ){` |
|    38496 |  755 | `			PH7_MemObjLoad(pRoot,&cur);` |
|    38496 |  756 | `			cur.nIdx = pRoot->nIdx;` |
|    19396 |  757 | `		}` |
|        - |  758 | `	}` |
|    76991 |  759 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|    38512 |  760 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|        - |  761 | `		ph7_value out;` |
|    38512 |  762 | `		PH7_MemObjInit(&(*pVm),&out);` |
|    38512 |  763 | `		if( pStep->isProp ){` |
|        - |  764 | `			ph7_value nameVal;` |
|       99 |  765 | `			PH7_MemObjInitFromString(&(*pVm),&nameVal,&pStep->sProp);` |
|       99 |  766 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_MEMBER,PH7_MEMBER_READ,&cur,&nameVal,&out);` |
|       99 |  767 | `			PH7_MemObjRelease(&nameVal);` |
|       51 |  768 | `		}else{` |
|    38416 |  769 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,0,&cur,&pStep->sKey,&out);` |
|        - |  770 | `		}` |
|    38512 |  771 | `		PH7_MemObjRelease(&cur);` |
|    38512 |  772 | `		cur = out;` |
|    38512 |  773 | `		if( rc != SXRET_OK ){` |
|       14 |  774 | `			PH7_MemObjRelease(&cur);` |
|       14 |  775 | `			return rc;` |
|        - |  776 | `		}` |
|    19403 |  777 | `	}` |
|    38484 |  778 | `	PH7_MemObjStore(&cur,pSlot);` |
|    38484 |  779 | `	pSlot->nIdx = SXU32_HIGH;` |
|    38484 |  780 | `	PH7_MemObjRelease(&cur);` |
|    38484 |  781 | `	return SXRET_OK;` |
|    19401 |  782 | `}` |
|        - |  783 | `/*` |
|        - |  784 | ` * D1: resolve deferred call arguments in [pArg, pTos) before the callee consumes them.` |
|        - |  785 | ` *` |
|        - |  786 | `` * A plain `$var` call argument whose callee signature is unknown at compile time is`` |
|        - |  787 | ` * emitted as a DEFERRED load (OP_LOAD iP1=1,iP2=3): when the variable does not exist it` |
|        - |  788 | ` * yields a NULL slot tagged MEMOBJ_AUX_DEFERRED that carries the variable name in x.pOther` |
|        - |  789 | ` * (a VM-lifetime bytecode string). This helper runs at each OP_CALL dispatch branch, while` |
|        - |  790 | ` * pVm->pFrame is still the CALLER frame, once the callee's by-ref shape is known:` |
|        - |  791 | ` *` |
|        - |  792 | ` *   by-ref position  -> create the variable in the caller frame now and give the slot its` |
|        - |  793 | ` *                       real nIdx so the ordinary by-ref binder aliases it (silent, as php).` |
|        - |  794 | ` *   by-value position-> raise php's "Undefined variable $x" and pass a clean NULL WITHOUT` |
|        - |  795 | ` *                       creating the variable in the caller.` |
|        - |  796 | ` *` |
|        - |  797 | ` * The by-ref decision for positional argument n comes from, in priority order:` |
|        - |  798 | ` *   bAllByValue  -> everything by-value (an unresolvable/erroring callee);` |
|        - |  799 | ` *   bAllByRef    -> everything by-ref (the indirect array-callable/__invoke dispatch paths,` |
|        - |  800 | ` *                   which historically over-vivified every plain-var arg — preserved here` |
|        - |  801 | ` *                   rather than regressed; their by-value refinement is a later slice);` |
|        - |  802 | ` *   pFormal      -> a user function's formal-argument array (honoring a trailing variadic);` |
|        - |  803 | ` *   nByRefMask   -> a builtin by-ref position bitmask (used when pFormal == 0).` |
|        - |  804 | ` *` |
|        - |  805 | ` * A DEFINED variable never carries the marker (it loads with its real nIdx), so this is a` |
|        - |  806 | ` * no-op for it; a call with no deferred args pays only one flag test per slot.` |
|        - |  807 | ` */` |
|  6574483 |  808 | `static sxi32 VmResolveDeferredArgs(` |
|        - |  809 | `	ph7_vm *pVm,` |
|        - |  810 | `	ph7_value *pArg,` |
|        - |  811 | `	ph7_value *pTos,` |
|        - |  812 | `	ph7_vm_func_arg *pFormal,` |
|        - |  813 | `	sxu32 nFormal,` |
|        - |  814 | `	sxu32 nByRefMask,` |
|        - |  815 | `	int bAllByRef,` |
|        - |  816 | `	int bAllByValue)` |
|        5 |  817 | `{` |
|        - |  818 | `	ph7_value *p;` |
|  6574488 |  819 | `	sxu32 n = 0;` |
| 13143542 |  820 | `	for( p = pArg ; p < pTos ; ++p, ++n ){` |
|  6569079 |  821 | `		int bByRef = 0;` |
|        - |  822 | `		SyString sName;` |
|  6569079 |  823 | `		if( (p->iFlags & (MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH)) == 0 ){` |
|  6549653 |  824 | `			continue;` |
|        - |  825 | `		}` |
|    38528 |  826 | `		if( bAllByValue ){` |
|      ! 0 |  827 | `			bByRef = 0;` |
|    38528 |  828 | `		}else if( bAllByRef ){` |
|        - |  829 | `			/* bAllByRef comes ONLY from the array-callable-value and __invoke dispatch paths,` |
|        - |  830 | `			 * which cannot expose the target's per-parameter by-ref flags here. Commit 1 chose` |
|        - |  831 | `			 * to over-vivify every deferred PLAIN-VAR arg on those paths (harmless: it just` |
|        - |  832 | `			 * materializes the caller variable), and that is preserved. But a deferred` |
|        - |  833 | `			 * ELEMENT/PROPERTY (MEMOBJ_AUX_DEFPATH) must NOT be blanket-vivified there: a missing` |
|        - |  834 | `			 * property would fatal ("Cannot create dynamic property") and a missing element would` |
|        - |  835 | `			 * silently vivify + swallow php's "Undefined array key" warning. Resolve those` |
|        - |  836 | `			 * by-value (warn + pass NULL), which matches php for a by-VALUE __invoke/callable —` |
|        - |  837 | `			 * the genuine by-ref-out-param-into-an-element case stays unsupported here, exactly` |
|        - |  838 | `			 * as it was before this slice. */` |
|      ! 0 |  839 | `			bByRef = (p->iFlags & MEMOBJ_AUX_DEFPATH) ? 0 : 1;` |
|    38528 |  840 | `		}else if( pFormal ){` |
|       44 |  841 | `			sxu32 idx = n;` |
|       44 |  842 | `			if( idx >= nFormal ){` |
|        - |  843 | `				/* Beyond the declared formals: a trailing variadic absorbs the tail` |
|        - |  844 | `				 * (and dictates its by-ref-ness); otherwise the extra arg is by-value. */` |
|      ! 0 |  845 | `				idx = (nFormal > 0 && (pFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC))` |
|      ! 0 |  846 | `					? nFormal - 1 : SXU32_HIGH;` |
|      ! 0 |  847 | `			}` |
|       44 |  848 | `			if( idx != SXU32_HIGH ){` |
|       44 |  849 | `				bByRef = (pFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|       20 |  850 | `			}` |
|       24 |  851 | `		}else{` |
|    38488 |  852 | `			bByRef = (n < 31 && (nByRefMask & (1u << n))) ? 1 : 0;` |
|        - |  853 | `		}` |
|    38528 |  854 | `		if( p->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|        - |  855 | `			/* D1 commit 2: a deferred array-element/property lvalue. Detach the descriptor` |
|        - |  856 | `			 * FIRST (so an exception mid-resolve, or a later stack release, cannot double-free` |
|        - |  857 | `			 * it) then re-walk it in the chosen mode. */` |
|    38520 |  858 | `			VmDeferredPath *pPath = (VmDeferredPath *)p->x.pOther;` |
|        - |  859 | `			sxi32 rc;` |
|    38520 |  860 | `			p->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|    38520 |  861 | `			p->x.pOther = 0;` |
|    38520 |  862 | `			MemObjSetType(p,MEMOBJ_NULL);` |
|    38520 |  863 | `			p->nIdx = SXU32_HIGH;` |
|    38520 |  864 | `			if( bByRef ){` |
|       27 |  865 | `				rc = VmResolvePathByRef(&(*pVm),pPath,p);` |
|       15 |  866 | `			}else{` |
|    38496 |  867 | `				rc = VmResolvePathByValue(&(*pVm),pPath,p);` |
|        - |  868 | `			}` |
|    38520 |  869 | `			VmFreeDeferredPath(pPath);` |
|    38520 |  870 | `			if( rc != SXRET_OK ){` |
|       23 |  871 | `				return rc;` |
|        - |  872 | `			}` |
|    38500 |  873 | `			continue;` |
|        - |  874 | `		}` |
|        - |  875 | `		/* Recover the deferred variable name and drop the marker + carrier. */` |
|       10 |  876 | `		SyStringInitFromBuf(&sName,(const char *)p->x.pOther,` |
|        - |  877 | `			p->x.pOther ? SyStrlen((const char *)p->x.pOther) : 0);` |
|       10 |  878 | `		p->iFlags &= ~MEMOBJ_AUX_DEFERRED;` |
|       10 |  879 | `		p->x.pOther = 0;` |
|       10 |  880 | `		if( bByRef ){` |
|        - |  881 | `			/* Materialize in the caller frame; the value stays NULL, only the slot` |
|        - |  882 | `			 * back-reference (nIdx) matters for the by-ref binder / write-back. */` |
|        7 |  883 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|        7 |  884 | `			if( pObj ){` |
|        7 |  885 | `				p->nIdx = pObj->nIdx;` |
|        3 |  886 | `			}` |
|        4 |  887 | `		}else{` |
|        - |  888 | `			/* php warns and passes NULL without creating the variable; the slot is` |
|        - |  889 | `			 * already a clean NULL with nIdx == SXU32_HIGH from the deferred load. */` |
|        3 |  890 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        - |  891 | `		}` |
|        6 |  892 | `	}` |
|  6574468 |  893 | `	return SXRET_OK;` |
|  3288188 |  894 | `}` |
|        - |  895 | `/*` |
|        - |  896 | ` * Did resolving a class NAME raise?` |
|        - |  897 | ` *` |
|        - |  898 | ` * The lookup can run an AUTOLOADER, and that autoloader can throw. The boundary rail` |
|        - |  899 | ` * either parks the status in nBoundaryRc or — when a try caught it in place — records a` |
|        - |  900 | ` * resume frame; either way the throw is already the engine's to land. A call site that` |
|        - |  901 | `` * sees the class "missing" and piles its own `Class "X" not found` Error on top reports a`` |
|        - |  902 | ` * failure php never reports, and that second Error belongs to nobody: it came back` |
|        - |  903 | ` * UNCAUGHT and killed the script right after the real exception had been handled.` |
|        - |  904 | ` *` |
|        - |  905 | ` * Snapshot (nBoundaryRc, pResumeFrame) before the lookup and pass them here after.` |
|        - |  906 | ` */` |
|      120 |  907 | `PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore)` |
|        5 |  908 | `{` |
|      125 |  909 | `	return pVm->nBoundaryRc != nBrcBefore \|\| (const void *)pVm->pResumeFrame != pResumeBefore;` |
|        5 |  910 | `}` |
|        - |  911 | `/*` |
|        - |  912 | `` * Name php's error for a class+method callable that the DIRECT `$cb()` dispatch cannot`` |
|        - |  913 | ` * call, or return 0 when it resolves.` |
|        - |  914 | ` *` |
|        - |  915 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|        - |  916 | ` * result for an unresolvable pair — silence a caller cannot detect — so the direct call` |
|        - |  917 | ` * site has to decide for itself. It used to do that only for the ARRAY form; the` |
|        - |  918 | ``  * `"Class::method"` STRING form went straight to the dispatcher, and `$cb='C::nosuch'` `` |
|        - |  919 | ` * evaluated to NULL with no diagnostic at all where php throws.` |
|        - |  920 | ` *` |
|        - |  921 | ` * pClass is the resolved target class (0 when the name named nothing); zCls/nCls is the` |
|        - |  922 | ` * class name AS WRITTEN, which is what php's not-found message quotes. bStaticForm says the` |
|        - |  923 | ` * target was a class NAME rather than an object. Messages that interpolate a name are built` |
|        - |  924 | ` * into zBuf.` |
|        - |  925 | ` *` |
|        - |  926 | ` * Visibility is NOT decided here: an inaccessible method is diagnosed downstream by the` |
|        - |  927 | ` * dispatch itself ("Call to private method C::p() from global scope"), php-exact already —` |
|        - |  928 | ` * and php reports visibility BEFORE staticness, so the static rule below has to stay quiet` |
|        - |  929 | ` * for a method this scope could not reach anyway.` |
|        - |  930 | ` */` |
|   200206 |  931 | `static const char * VmCallableClassMethodError(` |
|        - |  932 | `	ph7_vm *pVm,` |
|        - |  933 | `	ph7_class *pClass,             /* Resolved target class, or 0 */` |
|        - |  934 | `	const char *zCls,sxu32 nCls,   /* Its name as the callable wrote it */` |
|        - |  935 | `	const char *zMeth,sxu32 nMeth, /* The method name */` |
|        - |  936 | `	int bStaticForm,               /* TRUE when the target is a class NAME, not an object */` |
|        - |  937 | `	char *zBuf,int nBuf            /* Scratch for the messages that quote a name */` |
|        - |  938 | `	)` |
|        4 |  939 | `{` |
|        - |  940 | `	ph7_class_method *pMethod;` |
|        - |  941 | `	ph7_class *pDecl;` |
|        - |  942 | `	SyString sMeth;` |
|   200210 |  943 | `	if( pClass == 0 ){` |
|       43 |  944 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|       43 |  945 | `		return zBuf;` |
|        - |  946 | `	}` |
|   200170 |  947 | `	pMethod = PH7_ClassExtractMethod(pClass,zMeth,nMeth);` |
|   200170 |  948 | `	if( pMethod == 0 ){` |
|        - |  949 | `		/* A class that answers for unknown names through the catch-all has nothing to` |
|        - |  950 | `		 * report: php runs __callStatic (class-name target) / __call (object target) for` |
|        - |  951 | `		 * ANY method name, and the dispatcher below routes it. */` |
|       25 |  952 | `		const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       25 |  953 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|        7 |  954 | `			return 0;` |
|        - |  955 | `		}` |
|       28 |  956 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",` |
|        9 |  957 | `			&pClass->sName,(int)nMeth,zMeth);` |
|       19 |  958 | `		return zBuf;` |
|        - |  959 | `	}` |
|        - |  960 | `	/* An ABSTRACT method (an interface's included) has no body to call — the same message` |
|        - |  961 | ``	 * the `C::m()` SYNTAX raises in vm_ops_oo.c, which this dispatch reached only as a`` |
|        - |  962 | `	 * mangled internal function name. */` |
|   200146 |  963 | `	SyStringInitFromBuf(&sMeth,zMeth,nMeth);` |
|   200146 |  964 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        7 |  965 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%z()",&pClass->sName,&sMeth);` |
|        7 |  966 | `		return zBuf;` |
|        - |  967 | `	}` |
|        - |  968 | `	/* Named through a class NAME, a non-static method is never callable: php refuses even` |
|        - |  969 | `	 * when the CALLER has a compatible $this (unlike call_user_func, which binds it). The` |
|        - |  970 | `	 * message names the DECLARING class and the method's declared spelling. */` |
|   200140 |  971 | `	pDecl = pMethod->sFunc.pUserData ? (ph7_class *)pMethod->sFunc.pUserData : pClass;` |
|   200140 |  972 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|        - |  973 | `		SyString sDecl;` |
|       17 |  974 | `		SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - |  975 | `			SyStringLength(&pMethod->sFunc.sName));` |
|       16 |  976 | `		if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC` |
|       11 |  977 | `		 \|\| PH7_VmClassMemberAccess(&(*pVm),pDecl,&sDecl,pMethod->iProtection,FALSE) ){` |
|       19 |  978 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|        6 |  979 | `				&pDecl->sName,&sDecl);` |
|       13 |  980 | `			return zBuf;` |
|        - |  981 | `		}` |
|        2 |  982 | `	}` |
|   200128 |  983 | `	return 0;` |
|   100107 |  984 | `}` |
|        - |  985 | `/*` |
|        - |  986 | ` * The same check for the ARRAY form, whose two members carry php's own shape messages` |
|        - |  987 | ` * before anything is resolved: the target must be an object or a class-name string, the` |
|        - |  988 | `` * method must be a string. php probes them in that order (`[5,5]` names the FIRST member,`` |
|        - |  989 | `` * `['NoSuch',5]` the SECOND — the member shape decides before the class is looked up).`` |
|        - |  990 | ` */` |
|   100164 |  991 | `static const char * VmDirectArrayCallableError(ph7_vm *pVm,ph7_value *pTarget,ph7_value *pMethod,` |
|        - |  992 | `	char *zBuf,int nBuf)` |
|        4 |  993 | `{` |
|        - |  994 | `	ph7_class *pClass;` |
|   100168 |  995 | `	if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|        7 |  996 | `		return "First array member is not a valid class name or object";` |
|        - |  997 | `	}` |
|   100162 |  998 | `	if( (pMethod->iFlags & MEMOBJ_STRING) == 0 ){` |
|        9 |  999 | `		return "Second array member is not a valid method";` |
|        - | 1000 | `	}` |
|   100154 | 1001 | `	pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   150229 | 1002 | `	return VmCallableClassMethodError(&(*pVm),pClass,` |
|   100150 | 1003 | `		(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|   100150 | 1004 | `		(const char *)SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob),` |
|        - | 1005 | `		/* An OBJECT target carries its own $this; only a class NAME is the static form. */` |
|   100150 | 1006 | `		(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|    50075 | 1007 | `		zBuf,nBuf);` |
|    50086 | 1008 | `}` |
|        - | 1009 | `/*` |
|        - | 1010 | `` * Split a `"Class::method"` callable string. php scans for the LAST "::" (zend_memrchr),`` |
|        - | 1011 | `` * so `"C::s::x"` names the class `C::s` — not `C` — and reports it not found. Returns TRUE`` |
|        - | 1012 | `` * and the two halves (either may be empty: `"C::"` and `"::s"` are shapes php accepts here`` |
|        - | 1013 | ` * and rejects further down), FALSE when the string carries no "::" at all.` |
|        - | 1014 | ` */` |
|   342406 | 1015 | `PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,` |
|        - | 1016 | `	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth)` |
|        5 | 1017 | `{` |
|        - | 1018 | `	sxu32 i;` |
|  2903071 | 1019 | `	for( i = nName ; i >= 2 ; --i ){` |
|  2760781 | 1020 | `		if( zName[i-2] == ':' && zName[i-1] == ':' ){` |
|   200119 | 1021 | `			*pzCls = zName;` |
|   200119 | 1022 | `			*pnCls = i - 2;` |
|   200119 | 1023 | `			*pzMeth = &zName[i];` |
|   200119 | 1024 | `			*pnMeth = nName - i;` |
|   200119 | 1025 | `			return TRUE;` |
|        - | 1026 | `		}` |
|  1280335 | 1027 | `	}` |
|   142295 | 1028 | `	return FALSE;` |
|   171208 | 1029 | `}` |
|  9371570 | 1030 | `static sxi32 VmByteCodeExecBody(` |
|        - | 1031 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 1032 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - | 1033 | `	ph7_value *pStack,   /* Operand stack */` |
|        - | 1034 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - | 1035 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - | 1036 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - | 1037 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - | 1038 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - | 1039 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - | 1040 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|        - | 1041 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|        - | 1042 | `	ph7_value **ppBaseOwner, /* Base (pCallTop==0) operand-stack owner slot the native entry frees; OP_SPREAD growth of the base stack writes the new pointer here (see VmGrowOperandStack). */` |
|        - | 1043 | `	sxu32 *pnBaseCap, /* Base stack capacity (persisted across coroutine suspend/resume); read for the initial capacity and updated on base-stack growth. */` |
|        - | 1044 | `	sxu32 nStackOrig /* Base stack's ORIGINAL (ungrown) allocation size — the fixed headroom reference for OP_SPREAD growth (see nStackOrig in VmExecState). */` |
|        - | 1045 | `	)` |
|        5 | 1046 | `{` |
|        - | 1047 | `	VmInstr *pInstr;` |
|        - | 1048 | `	ph7_value *pTos;` |
|        - | 1049 | `	SySet aArg;` |
|  9371575 | 1050 | `	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record` |
|        - | 1051 | `	                            * stack (BYTECODE stage 2); NULL = executing the` |
|        - | 1052 | `	                            * bottom activation. */` |
|        - | 1053 | `	VmExecState sState; /* This activation's boundary state (BYTECODE.md stage 1):` |
|        - | 1054 | `	                     * everything a suspended/nested activation must restore.` |
|        - | 1055 | `	                     * pc/pTos stay in locals for the hot loop and are synced` |
|        - | 1056 | `	                     * into sState only around the call epilogue (stage 2 turns` |
|        - | 1057 | `	                     * that boundary into an explicit record push/pop). */` |
|        - | 1058 | `	sxi32 pc;` |
|        - | 1059 | `	sxi32 rc;` |
|  9371575 | 1060 | `	sState.aInstr = aInstr;` |
|  9371575 | 1061 | `	sState.pStack = pStack;` |
|  9371575 | 1062 | `	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */` |
|  9371575 | 1063 | `	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */` |
|  9371575 | 1064 | `	sState.pResult = pResult;` |
|  9371575 | 1065 | `	sState.pLastRef = pLastRef;` |
|  9371575 | 1066 | `	sState.pEnforceRetFunc = pEnforceRetFunc;` |
|  9371575 | 1067 | `	sState.is_callback = (sxu8)(is_callback ? 1 : 0);` |
|  9371575 | 1068 | `	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);` |
|        - | 1069 | `	/* Argument container */` |
|  9371575 | 1070 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|  9371575 | 1071 | `	if( nTos < 0 ){` |
|  7506371 | 1072 | `		pTos = &pStack[-1];` |
|  3753188 | 1073 | `	}else{` |
|  1865209 | 1074 | `		pTos = &pStack[nTos];` |
|        - | 1075 | `	}` |
|  9371575 | 1076 | `	sState.pTos = pTos;` |
|  9371575 | 1077 | `	sState.pc = nPc;` |
|        - | 1078 | `	/* Finally-drain base. For a resumed generator/fiber TOP-LEVEL body, its own` |
|        - | 1079 | `	 * exception handlers were just re-published above the caller depth` |
|        - | 1080 | `	 * (VmRestoreCtxState), so the live SySetUsed over-counts; take the` |
|        - | 1081 | `	 * caller-depth base recorded on the ctx instead.` |
|        - | 1082 | `	 *` |
|        - | 1083 | `	 * The discriminator is the OPERAND STACK, not the frame: the body exec runs on` |
|        - | 1084 | `	 * the ctx's own pStack, whereas every nested frame-less mini-program run within` |
|        - | 1085 | `	 * it — a default-argument / constructor trampoline, or a catch/finally body via` |
|        - | 1086 | `	 * VmLocalExec — runs on a FRESH operand stack while SHARING pVm->pFrame (those` |
|        - | 1087 | `	 * mini-programs do not push a VM frame). A pFrame-only guard therefore misfires` |
|        - | 1088 | `	 * for such a mini-program and hands it the generator's low caller-base; its` |
|        - | 1089 | `	 * terminal OP_DONE then drains VmDrainFinally down to that base, tearing down a` |
|        - | 1090 | ``	 * live try that the surrounding finally had just opened (e.g. `finally { try {`` |
|        - | 1091 | ``	 * throw new E(); } catch (E) {} }` in a generator: the `new E()` trampoline's`` |
|        - | 1092 | `	 * OP_DONE popped the inner try before its OP_THROW ran, so the throw escaped` |
|        - | 1093 | `	 * uncaught). Requiring pStack == pCtx->pStack pins the override to the resumed` |
|        - | 1094 | `	 * body itself; nested mini-programs fall through to the correct live depth. */` |
|  9371570 | 1095 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame` |
|     2966 | 1096 | `	 && pStack == pVm->pActiveCtx->pStack ){` |
|     1933 | 1097 | `		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;` |
|     1933 | 1098 | `		sState.nFinallyActBase = pVm->pActiveCtx->nFinallyBase;` |
|      969 | 1099 | `	}else{` |
|  9369647 | 1100 | `		sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|  9369647 | 1101 | `		sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|        - | 1102 | `	}` |
|  9371575 | 1103 | `	sState.pEntryFrame = pVm->pFrame;` |
|  9371575 | 1104 | `	pc = nPc;` |
|        - | 1105 | `	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call` |
|        - | 1106 | `	 * adopts its parked record segment here. VmResumeCtx hands the segment in` |
|        - | 1107 | `	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a` |
|        - | 1108 | `	 * nested mini-program/callback run inside the resumed body — after it rebased` |
|        - | 1109 | `	 * the segment's exception floors and pushed the resume value into the innermost` |
|        - | 1110 | `	 * stack. Locals switch to the innermost activation so the dispatch loop` |
|        - | 1111 | `	 * continues inside the callee; the record chain is restored so its completion` |
|        - | 1112 | `	 * unwinds back through the body. */` |
|  9371575 | 1113 | `	if( pAdoptSegment ){` |
|      106 | 1114 | `		VmParkedSegment *pSeg = pAdoptSegment;` |
|      106 | 1115 | `		pCallTop = pSeg->pCallTop;` |
|      106 | 1116 | `		sState = pSeg->sState;` |
|      106 | 1117 | `		aInstr = sState.aInstr;` |
|      106 | 1118 | `		pStack = sState.pStack;` |
|        - | 1119 | `		/* pc is already nPc (== pCtx->pc, the innermost's post-suspend pc) from the` |
|        - | 1120 | `		 * init above; only the stack/top move to the innermost activation. */` |
|      106 | 1121 | `		pTos = &pStack[nTos];   /* nTos == pCtx->nTos — innermost, resume value pushed */` |
|      106 | 1122 | `		SyMemBackendFree(&pVm->sAllocator,pSeg); /* holder only; its contents are now live */` |
|       51 | 1123 | `	}` |
|        - | 1124 | `/*` |
|        - | 1125 | ` * Route an enforcement helper's (or yield-from delegate's) return code from inside` |
|        - | 1126 | ` * the main switch: proceed on SXRET_OK, abort on PH7_ABORT, and on PH7_EXCEPTION` |
|        - | 1127 | ` * resume at the landing pad of the body that actually caught the exception in place` |
|        - | 1128 | ` * (VmRecordedResume) or, when it was caught by an outer exec, unwind out of the VM` |
|        - | 1129 | ` * loop. Replaces the old "jump to pVm->pFrame's nearest iExceptionJump" which ran` |
|        - | 1130 | ` * the statement after the try even when the catch was at an enclosing frame (ROOT B,` |
|        - | 1131 | `` * face b — `yield from` over a throwing sub-generator).`` |
|        - | 1132 | ` */` |
|        - | 1133 | `/* Dispatch-routing macros: bodies live in vm_dispatch.h, written against the` |
|        - | 1134 | ` * VM_EXIT_* primitives. Here they bind to the loop's own control flow. */` |
|        - | 1135 | `#define VM_EXIT_BREAK break` |
|        - | 1136 | `#define VM_EXIT_ABORT goto Abort` |
|        - | 1137 | `#define VM_EXIT_EXCEPTION goto Exception` |
|        - | 1138 | `#include "vm_dispatch.h"` |
|        - | 1139 | `	/* Generator::throw() inject-at-yield: when this invocation is a resumed generator/fiber` |
|        - | 1140 | `	 * body carrying a pending injected exception, raise it HERE — once, before the dispatch` |
|        - | 1141 | `	 * loop (pc is already at the resume point) — so the existing OP_THROW route` |
|        - | 1142 | `	 * (VmThrowException + VmRecordedResume) lands it at the generator's own try/catch landing` |
|        - | 1143 | `	 * pad WITHOUT reconstructing the suspended exception frame on pVm->pFrame. Because` |
|        - | 1144 | `	 * pVm->pFrame stays the body, the return/finally/sRet subsystem is untouched (this is why` |
|        - | 1145 | `	 * the reverted frame-reconstruction approach's regression cannot recur). Behaviorally` |
|        - | 1146 | ``	 * identical to a `throw` executed at the yield point: if the generator's own try catches`` |
|        - | 1147 | `	 * it we resume after the try; otherwise it propagates to the throw() caller (ROOT B lands` |
|        - | 1148 | `	 * the caller's handler) and the ctx closes. pInjected is one-shot and only meaningful at` |
|        - | 1149 | `	 * the resume pc, so this is checked once at entry — NOT per-instruction — keeping the hot` |
|        - | 1150 | `	 * dispatch loop untouched for all normal code. The pFrame gate keeps a nested call / catch` |
|        - | 1151 | `	 * mini-program sharing the ctx (a separate VmByteCodeExec entry) from re-firing.` |
|        - | 1152 | `	 *` |
|        - | 1153 | ``	 * Exception: when this body is suspended mid `yield from` over an inner Generator`` |
|        - | 1154 | `	 * (iDelegateState==3), a Generator::throw() on the OUTER generator must be forwarded` |
|        - | 1155 | `	 * INTO the delegate (PHP yield-from transparency), not raised here. Leave pInjected` |
|        - | 1156 | `	 * set and skip; the resume pc is that OP_YIELD_FROM, which consumes and forwards it. */` |
|  9371570 | 1157 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected` |
|     1639 | 1158 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       63 | 1159 | `	 && pVm->pActiveCtx->iDelegateState != 3 ){` |
|       58 | 1160 | `		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;` |
|        - | 1161 | `		VmFrame *pThrowFrame;` |
|        - | 1162 | `		sxi32 iResumePc;` |
|       58 | 1163 | `		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */` |
|        - | 1164 | `		/* Raising the injection at the yield-from point abandons any array/Iterator` |
|        - | 1165 | `		 * delegation in progress (state 1/2 — state 3 was forwarded, not raised here).` |
|        - | 1166 | `		 * Tear the delegate down so that if the generator's own try catches this and` |
|        - | 1167 | ``		 * runs on to a LATER `yield from`, that opcode classifies its operand fresh`` |
|        - | 1168 | `		 * instead of resuming this now-stale delegate cursor. */` |
|       58 | 1169 | `		if( pVm->pActiveCtx->iDelegateState != 0 ){` |
|        3 | 1170 | `			PH7_MemObjRelease(&pVm->pActiveCtx->sDelegate);` |
|        3 | 1171 | `			pVm->pActiveCtx->pDelegateNode = 0;` |
|        3 | 1172 | `			pVm->pActiveCtx->iDelegateState = 0;` |
|        1 | 1173 | `		}` |
|       58 | 1174 | `		pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       58 | 1175 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|       58 | 1176 | `		rc = VmThrowException(&(*pVm),pInj);` |
|       58 | 1177 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1178 | `			goto Abort;` |
|        - | 1179 | `		}` |
|       58 | 1180 | `		if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 1181 | `			/* ROOT C: the inject was caught by an inline try in THIS generator. Drain the` |
|        - | 1182 | `			 * abandoned mid-expression operands and land at the catch/finally body. This is` |
|        - | 1183 | `			 * the pre-loop path (the first fetch uses pc directly), so no -1 adjustment. */` |
|       92 | 1184 | `			while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|       48 | 1185 | `				PH7_MemObjRelease(pTos);` |
|       48 | 1186 | `				pTos--;` |
|        4 | 1187 | `			}` |
|       48 | 1188 | `			pc = (sxi32)pVm->iInlinePc;` |
|       48 | 1189 | `			pVm->pInlineInstr = 0;` |
|       34 | 1190 | `		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 1191 | `			/* Caught by THIS generator's own try. (VmRecordedResume returns FALSE unless a` |
|        - | 1192 | `			 * catch recorded a resume target for this exec, so rc need not be pre-checked;` |
|        - | 1193 | `			 * unlike OP_THROW there is no lexical-try fallthrough here — the else just` |
|        - | 1194 | `			 * propagates.) Drain the abandoned mid-expression operand slots back to the` |
|        - | 1195 | `			 * catching try's base, then land at its pad (iResumePc is landing-1 for the` |
|        - | 1196 | `			 * dispatcher's trailing pc++; the loop below fetches at pc with no leading pc++,` |
|        - | 1197 | `			 * so add 1 to land on the pad itself). */` |
|      ! 0 | 1198 | `			while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      ! 0 | 1199 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 | 1200 | `				pTos--;` |
|      ! 0 | 1201 | `			}` |
|      ! 0 | 1202 | `			pc = iResumePc + 1;` |
|      ! 0 | 1203 | `		}else{` |
|        - | 1204 | `			/* Not caught in this generator (no match, or caught by an outer/caller frame` |
|        - | 1205 | `			 * that ROOT B will land at its own OP_CALL site): propagate out so the ctx` |
|        - | 1206 | `			 * closes and the caller sees the exception. */` |
|       12 | 1207 | `			goto Exception;` |
|        - | 1208 | `		}` |
|       22 | 1209 | `	}` |
|        - | 1210 | `	/* Force-close entry (VmCloseCtx): a suspended generator being destroyed runs its` |
|        - | 1211 | ``	 * pending `finally` blocks. Instead of resuming at the yield, redirect straight`` |
|        - | 1212 | ``	 * into the innermost open try's finally as if a `return` had crossed every`` |
|        - | 1213 | `	 * enclosing finally (mirrors OP_SET_FINALLY_RET; OP_END_FINALLY then threads the` |
|        - | 1214 | `	 * PH7_FA_RETURN out through the whole chain and completes the body). Gate on the` |
|        - | 1215 | `	 * BODY bytecode (aInstr == the function program) so nested mini-programs sharing` |
|        - | 1216 | `	 * this ctx/frame never re-fire it; bClosing stays set so OP_YIELD can reject a` |
|        - | 1217 | `	 * yield reached inside one of these finallys. */` |
|  9371560 | 1218 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing` |
|     1624 | 1219 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       43 | 1220 | `	 && aInstr == (VmInstr *)SySetBasePtr(&pVm->pActiveCtx->pFunc->aByteCode) ){` |
|        - | 1221 | `		VmFinallyAction sAct;` |
|       42 | 1222 | `		sxu32 iFpc = 0;` |
|       42 | 1223 | `		int nCross = -1; /* cross every enclosing finally of this body */` |
|       42 | 1224 | `		SyZero(&sAct,sizeof(sAct));` |
|       42 | 1225 | `		sAct.eKind = PH7_FA_RETURN;` |
|       42 | 1226 | `		sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       42 | 1227 | `		PH7_MemObjInit(pVm,&sAct.sRet); /* discarded return; getReturn() is moot post-close */` |
|       42 | 1228 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|       21 | 1229 | `			sAct.nCross = nCross;` |
|       21 | 1230 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       21 | 1231 | `			pc = (sxi32)iFpc; /* pre-loop: the first fetch uses pc directly (no -1) */` |
|       11 | 1232 | `		}else{` |
|        - | 1233 | `			/* No open try had a finally: nothing to run, complete the body. */` |
|       21 | 1234 | `			PH7_MemObjRelease(&sAct.sRet);` |
|       21 | 1235 | `			goto Done;` |
|        - | 1236 | `		}` |
|       10 | 1237 | `	}` |
|        - | 1238 | `	/* Execute as much as we can */` |
| 38630210 | 1239 | `	for(;;){` |
|      ! 0 | 1240 | `VmLoopFetch:` |
|        - | 1241 | `		/* C-boundary throw routing (band A #1): a PHP callee invoked from a C` |
|        - | 1242 | `		 * site with no status channel — a __toString/__toInt cast, __get/__set/` |
|        - | 1243 | `		 * offsetGet/offsetSet, __clone, __destruct, a user callback inside a` |
|        - | 1244 | `		 * builtin — raised, and the site continued with a fallback value; the` |
|        - | 1245 | `		 * invocation boundary parked the status here (VmBoundaryPark). Route it` |
|        - | 1246 | `		 * exactly as the throw site's dispatch macro would have: abort, land at` |
|        - | 1247 | `		 * an inline-try redirect, resume at a recorded in-place catch (draining` |
|        - | 1248 | `		 * the abandoned mid-expression operands to the catching try's base), or` |
|        - | 1249 | `		 * propagate out of this exec. Checked at the fetch point, so a swallowed` |
|        - | 1250 | `		 * throw outlives at most the C remainder of ONE opcode instead of` |
|        - | 1251 | `		 * silently resuming the surrounding PHP code with a bogus value.` |
|        - | 1252 | `		 * The pending write-back sweep shares this one guard so the hot` |
|        - | 1253 | `		 * no-hooks path pays a single predicted branch per fetch. */` |
| 81238376 | 1254 | `		if( pVm->nBoundaryRc != 0 \|\| SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      763 | 1255 | `			if( pVm->nBoundaryRc != 0 ){` |
|      193 | 1256 | `				sxi32 rcBr = pVm->nBoundaryRc;` |
|      193 | 1257 | `				pVm->nBoundaryRc = 0;` |
|      193 | 1258 | `				if( rcBr == PH7_ABORT ){` |
|        3 | 1259 | `					goto Abort;` |
|        - | 1260 | `				}` |
|      191 | 1261 | `				if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 1262 | `					/* Caught by an inline try (generator body) THIS exec owns: drain` |
|        - | 1263 | `					 * and land (pre-fetch path: pc is used directly, no trailing ++). */` |
|      ! 0 | 1264 | `					while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|      ! 0 | 1265 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 | 1266 | `						pTos--;` |
|      ! 0 | 1267 | `					}` |
|      ! 0 | 1268 | `					pc = (sxi32)pVm->iInlinePc;` |
|      ! 0 | 1269 | `					pVm->pInlineInstr = 0;` |
|      ! 0 | 1270 | `				}else{` |
|        - | 1271 | `					sxi32 iBrPc;` |
|      191 | 1272 | `					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){` |
|        - | 1273 | `						/* Caught in place by a try THIS exec owns: drain the abandoned` |
|        - | 1274 | `						 * operands to the catching try's base and land at its pad` |
|        - | 1275 | `						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;` |
|        - | 1276 | `						 * pre-fetch here, so +1 lands on the pad itself). */` |
|      259 | 1277 | `						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      131 | 1278 | `							PH7_MemObjRelease(pTos);` |
|      131 | 1279 | `							pTos--;` |
|        5 | 1280 | `						}` |
|      133 | 1281 | `						pc = iBrPc + 1;` |
|       69 | 1282 | `					}else{` |
|        - | 1283 | `						/* Caught by an outer exec (or uncaught-with-report pending):` |
|        - | 1284 | `						 * unwind out of this exec; the owner's macros/router land it. */` |
|       62 | 1285 | `						goto Exception;` |
|        - | 1286 | `					}` |
|        - | 1287 | `				}` |
|       64 | 1288 | `			}` |
|        - | 1289 | `			/* Stale write-back sweep: a pending entry whose OWNING activation is` |
|        - | 1290 | `			 * fetching any pc outside its armed window [nJmpPc, nPc] is dead — for` |
|        - | 1291 | `			 * an RMW entry (window = the modify op alone) a routed throw abandoned` |
|        - | 1292 | `			 * the arming statement mid-flight; for a ??= entry either a throw` |
|        - | 1293 | `			 * abandoned the RHS or the short-circuit jump landed past the` |
|        - | 1294 | `			 * OP_NULLC_STORE (the assign is skipped). Drop it — no set dispatch` |
|        - | 1295 | `			 * (php: the throw/skip discards the write). A nested exec (even a` |
|        - | 1296 | `			 * recursive one over the same bytecode) has a different operand-stack` |
|        - | 1297 | `			 * base and leaves enclosing entries alone; entries below a live top` |
|        - | 1298 | `			 * are reached as the drops expose them. */` |
|      713 | 1299 | `			while( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      571 | 1300 | `				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      570 | 1301 | `				if( pTopRmw->pOwnerStack != (void *)pStack \|\| pTopRmw->pInstrs != (void *)aInstr` |
|       91 | 1302 | `				 \|\| ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){` |
|      281 | 1303 | `					break; /* not ours, or legitimately in flight */` |
|        - | 1304 | `				}` |
|       11 | 1305 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 1306 | `			}` |
|      349 | 1307 | `		}` |
|        - | 1308 | `		/* Fetch the instruction to execute */` |
| 81238316 | 1309 | `		pInstr = &aInstr[pc];` |
| 81238316 | 1310 | `		if( pInstr->nLine ){` |
|        - | 1311 | `			/* Publish the source position for diagnostics, debug_backtrace() and` |
|        - | 1312 | `			 * Throwable. Instructions the compiler could not attribute (nLine 0)` |
|        - | 1313 | `			 * leave the last known line standing rather than reporting line 0.` |
|        - | 1314 | `			 * The compilation unit's strict_types mode rides the same gate: a` |
|        - | 1315 | `			 * SYNTHETIC instruction (nLine 0) must not claim to speak for a file. */` |
| 77776746 | 1316 | `			pVm->nCurLine = pInstr->nLine;` |
| 77776746 | 1317 | `			pVm->bCurStrict = pInstr->bStrict;` |
| 38900974 | 1318 | `		}` |
| 81238316 | 1319 | `		rc = SXRET_OK;` |
|        - | 1320 | `/*` |
|        - | 1321 | ` * What follows here is a massive switch statement where each case implements a` |
|        - | 1322 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - | 1323 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - | 1324 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - | 1325 | ` * the switch statement will break with convention and be flush-left.` |
|        - | 1326 | ` */` |
| 81238316 | 1327 | `		switch(pInstr->iOp){` |
|        - | 1328 | `/*` |
|        - | 1329 | ` * DONE: P1 * *` |
|        - | 1330 | ` *` |
|        - | 1331 | ` * Program execution completed: Clean up the mess left behind` |
|        - | 1332 | ` * and return immediately.` |
|        - | 1333 | ` */` |
|  5273422 | 1334 | `case PH7_OP_DONE:` |
| 10547245 | 1335 | `	if( pInstr->iP2 && sState.bReturnPropagates ){` |
|        - | 1336 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - | 1337 | `		 * onto the body frame this catch/finally returns from (skip the transparent` |
|        - | 1338 | `		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION` |
|        - | 1339 | `		 * materializes it into sState.pResult. Drain any finally opened within this body` |
|        - | 1340 | `		 * first (nested try/finally inside the catch), which may overwrite the same` |
|        - | 1341 | `		 * frame's slot (finally-over-catch). */` |
|    20221 | 1342 | `		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|    20221 | 1343 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    20217 | 1344 | `			PH7_MemObjStore(pTos,&pTgt->sRet);` |
|    20217 | 1345 | `			VmPopOperand(&pTos,1);` |
|    10111 | 1346 | `		}else{` |
|        6 | 1347 | ``			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */`` |
|        - | 1348 | `		}` |
|    20221 | 1349 | `		pTgt->bHasRet = 1;` |
|    20221 | 1350 | `		pTgt->nRetGen++;` |
|    20221 | 1351 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    20221 | 1352 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1353 | `			goto Abort;` |
|        - | 1354 | `		}` |
|    20221 | 1355 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 1356 | `			/* A drained finally threw past itself — it discards this return. */` |
|      ! 0 | 1357 | `			goto Exception;` |
|        - | 1358 | `		}` |
|    20221 | 1359 | `		goto Done;` |
|        - | 1360 | `	}` |
|        - | 1361 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - | 1362 | `	 * the fiber start/resume paths) set sState.pEnforceRetFunc, so this branch is` |
|        - | 1363 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - | 1364 | `	 * callback trampolines, and the main script. */` |
| 10527024 | 1365 | `	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)` |
|    10851 | 1366 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - | 1367 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - | 1368 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - | 1369 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - | 1370 | `		 * value the function never actually returned, so enforcing here would` |
|        - | 1371 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - | 1372 | `		 * exception. */` |
|    10851 | 1373 | `		ph7_value *pRetVal = 0;` |
|    10851 | 1374 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|     9829 | 1375 | `			pRetVal = pTos;` |
|     4912 | 1376 | `		}` |
|    10851 | 1377 | `		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);` |
|    10851 | 1378 | `		if( rc == PH7_ABORT ) goto Abort;` |
|    10847 | 1379 | `		if( rc == PH7_EXCEPTION ){` |
|      145 | 1380 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      111 | 1381 | `				PH7_MemObjRelease(pTos);` |
|      111 | 1382 | `				pTos--;` |
|       53 | 1383 | `			}` |
|      145 | 1384 | `			goto Exception;` |
|        - | 1385 | `		}` |
|        - | 1386 | `		/* Don't enforce twice if the function loops through multiple` |
|        - | 1387 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - | 1388 | `		 * defensively we clear the pointer after a successful check). */` |
|    10707 | 1389 | `		sState.pEnforceRetFunc = 0;` |
|     5351 | 1390 | `	}` |
| 10526885 | 1391 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|  7532275 | 1392 | `		if( sState.pLastRef ){` |
|   120991 | 1393 | `			*sState.pLastRef = pTos->nIdx;` |
|    60691 | 1394 | `		}` |
|  7532275 | 1395 | `		if( sState.pResult ){` |
|        - | 1396 | `			/* Execution result */` |
|  6077839 | 1397 | `			PH7_MemObjStore(pTos,sState.pResult);` |
|  3039115 | 1398 | `		}` |
|  7532275 | 1399 | `		VmPopOperand(&pTos,1);` |
|  6760948 | 1400 | `	}else if( sState.pLastRef ){` |
|        - | 1401 | `		/* Nothing referenced — also the throw-unwind path: the compiler routes` |
|        - | 1402 | `		 * an uncaught exception to this terminal OP_DONE with iP1 set but an` |
|        - | 1403 | `		 * empty operand stack (pTos == pStack-1), so there is no return value to` |
|        - | 1404 | `		 * store. Guarding on pTos >= pStack (matching the two sibling branches` |
|        - | 1405 | `		 * above) avoids the below-base read that crashed under glibc/ASan. */` |
|  1457865 | 1406 | `		*sState.pLastRef = SXU32_HIGH;` |
|   728930 | 1407 | `	}` |
|        - | 1408 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - | 1409 | `	 * this execution. When 'return' is used inside a try block,` |
|        - | 1410 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - | 1411 | `	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering` |
|        - | 1412 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - | 1413 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - | 1414 | `	 * block can override it (the finally writes this body frame's sRet slot,` |
|        - | 1415 | `	 * materialized below).` |
|        - | 1416 | `	 */` |
| 10526885 | 1417 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
| 10526885 | 1418 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1419 | `		goto Abort;` |
|        - | 1420 | `	}` |
| 10526885 | 1421 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 1422 | `		/* A drained finally threw past itself, discarding the value this OP_DONE` |
|        - | 1423 | `		 * stored into sState.pResult. If an enclosing try IN THIS function caught the new` |
|        - | 1424 | `		 * exception in place, resume at its landing pad; otherwise unwind (the` |
|        - | 1425 | `		 * caller's exception-resume pops the stored result). */` |
|        - | 1426 | `		sxi32 iResumePc;` |
|        5 | 1427 | `		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 1428 | `			pc = iResumePc;` |
|        3 | 1429 | `			break;` |
|        - | 1430 | `		}` |
|        3 | 1431 | `		goto Exception;` |
|        - | 1432 | `	}` |
| 10526881 | 1433 | `	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){` |
|        - | 1434 | `		/* A catch/finally issued a 'return' targeting THIS body. If the body is` |
|        - | 1435 | `		 * actually unwinding because an exception escaped it (terminal OP_DONE on` |
|        - | 1436 | `		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type` |
|        - | 1437 | `		 * enforcement above), that exception supersedes the return: discard it.` |
|        - | 1438 | `		 * Otherwise materialize it as this function's result. */` |
|       11 | 1439 | `		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){` |
|      ! 0 | 1440 | `			VmClearFramePending(sState.pEntryFrame);` |
|      ! 0 | 1441 | `		}else{` |
|       11 | 1442 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|        - | 1443 | `		}` |
|        4 | 1444 | `	}` |
| 10526881 | 1445 | `	goto Done;` |
|        - | 1446 | `/*` |
|        - | 1447 | ` * HALT: P1 * *` |
|        - | 1448 | ` *` |
|        - | 1449 | ` * Program execution aborted: Clean up the mess left behind` |
|        - | 1450 | ` * and abort immediately.` |
|        - | 1451 | ` */` |
|       12 | 1452 | `case PH7_OP_HALT:` |
|       28 | 1453 | `	if( pInstr->iP1 ){` |
|        - | 1454 | `#ifdef UNTRUST` |
|        - | 1455 | `		if( pTos < pStack ){` |
|        - | 1456 | `			goto Abort;` |
|        - | 1457 | `		}` |
|        - | 1458 | `#endif` |
|       28 | 1459 | `		if( sState.pLastRef ){` |
|        6 | 1460 | `			*sState.pLastRef = pTos->nIdx;` |
|        2 | 1461 | `		}` |
|       28 | 1462 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       16 | 1463 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - | 1464 | `				/* Output the exit message */` |
|       22 | 1465 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        6 | 1466 | `					pVm->sVmConsumer.pUserData);` |
|       16 | 1467 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|       10 | 1468 | `			}` |
|       20 | 1469 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - | 1470 | `			/* Record exit status */` |
|       14 | 1471 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        6 | 1472 | `		}` |
|       28 | 1473 | `		VmPopOperand(&pTos,1);` |
|       12 | 1474 | `	}else if( sState.pLastRef ){` |
|        - | 1475 | `		/* Nothing referenced */` |
|      ! 0 | 1476 | `		*sState.pLastRef = SXU32_HIGH;` |
|      ! 0 | 1477 | `	}` |
|        - | 1478 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - | 1479 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - | 1480 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - | 1481 | `	 */` |
|       28 | 1482 | `	pVm->bHaltRequested = 1;` |
|       28 | 1483 | `	goto Abort;` |
|        - | 1484 | `/*` |
|        - | 1485 | ` * JMP: * P2 *` |
|        - | 1486 | ` *` |
|        - | 1487 | ` * Unconditional jump: The next instruction executed will be` |
|        - | 1488 | ` * the one at index P2 from the beginning of the program.` |
|        - | 1489 | ` */` |
|   426394 | 1490 | `case PH7_OP_JMP:` |
|   853196 | 1491 | `	pc = pInstr->iP2 - 1;` |
|   853196 | 1492 | `	break;` |
|        - | 1493 | `/*` |
|        - | 1494 | ` * JZ: P1 P2 *` |
|        - | 1495 | ` *` |
|        - | 1496 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - | 1497 | ` * entry in the stack if P1 is zero.` |
|        - | 1498 | ` */` |
|  2574533 | 1499 | `case PH7_OP_JZ:` |
|        - | 1500 | `#ifdef UNTRUST` |
|        - | 1501 | `	if( pTos < pStack ){` |
|        - | 1502 | `		goto Abort;` |
|        - | 1503 | `	}` |
|        - | 1504 | `#endif` |
|        - | 1505 | `	/* Get a boolean value */` |
|  5151375 | 1506 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     1065 | 1507 | `		PH7_MemObjToBool(pTos);` |
|      530 | 1508 | `	}` |
|  5151375 | 1509 | `	if( !pTos->x.iVal ){` |
|        - | 1510 | `		/* Take the jump */` |
|  2656262 | 1511 | `		pc = pInstr->iP2 - 1;` |
|  1328683 | 1512 | `	}` |
|  5151375 | 1513 | `	if( !pInstr->iP1 ){` |
|  4862467 | 1514 | `		VmPopOperand(&pTos,1);` |
|  2432383 | 1515 | `	}` |
|  5151375 | 1516 | `	break;` |
|        - | 1517 | `/*` |
|        - | 1518 | ` * JNZ: P1 P2 *` |
|        - | 1519 | ` *` |
|        - | 1520 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - | 1521 | ` * entry in the stack if P1 is zero.` |
|        - | 1522 | ` */` |
|   127292 | 1523 | `case PH7_OP_JNZ:` |
|        - | 1524 | `#ifdef UNTRUST` |
|        - | 1525 | `	if( pTos < pStack ){` |
|        - | 1526 | `		goto Abort;` |
|        - | 1527 | `	}` |
|        - | 1528 | `#endif` |
|        - | 1529 | `	/* Get a boolean value */` |
|   254993 | 1530 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 1531 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 1532 | `	}` |
|   254993 | 1533 | `	if( pTos->x.iVal ){` |
|        - | 1534 | `		/* Take the jump */` |
|    10679 | 1535 | `		pc = pInstr->iP2 - 1;` |
|     5337 | 1536 | `	}` |
|   254993 | 1537 | `	if( !pInstr->iP1 ){` |
|        8 | 1538 | `		VmPopOperand(&pTos,1);` |
|        3 | 1539 | `	}` |
|   254993 | 1540 | `	break;` |
|        - | 1541 | `/*` |
|        - | 1542 | ` * NOOP: * * *` |
|        - | 1543 | ` *` |
|        - | 1544 | ` * Do nothing. This instruction is often useful as a jump` |
|        - | 1545 | ` * destination.` |
|        - | 1546 | ` */` |
|      ! 0 | 1547 | `case PH7_OP_NOOP:` |
|      ! 0 | 1548 | `	break;` |
|        - | 1549 | `/*` |
|        - | 1550 | ` * POP: P1 * *` |
|        - | 1551 | ` *` |
|        - | 1552 | ` * Pop P1 elements from the operand stack.` |
|        - | 1553 | ` */` |
|  2407277 | 1554 | `case PH7_OP_POP: {` |
|  4816554 | 1555 | `	sxi32 n = pInstr->iP1;` |
|  4816554 | 1556 | `	if( &pTos[-n+1] < pStack ){` |
|        - | 1557 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       79 | 1558 | `		n = (sxi32)(pTos - pStack);` |
|       38 | 1559 | `	}` |
|  4816554 | 1560 | `	VmPopOperand(&pTos,n);` |
|  4816554 | 1561 | `	break;` |
|        - | 1562 | `				 }` |
|        - | 1563 | `/*` |
|        - | 1564 | ` * DUP: * * *` |
|        - | 1565 | ` *` |
|        - | 1566 | ` * Duplicate the top of the stack.` |
|        - | 1567 | ` */` |
|       67 | 1568 | `case PH7_OP_DUP:` |
|        - | 1569 | `#ifdef UNTRUST` |
|        - | 1570 | `	if( pTos < pStack ){` |
|        - | 1571 | `		goto Abort;` |
|        - | 1572 | `	}` |
|        - | 1573 | `#endif` |
|      138 | 1574 | `	pTos++;` |
|      138 | 1575 | `	PH7_MemObjInit(pVm,pTos);` |
|      138 | 1576 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|      138 | 1577 | `	break;` |
|        - | 1578 | `/*` |
|        - | 1579 | ` * CLASS_DEFER: * * P3` |
|        - | 1580 | ` *` |
|        - | 1581 | ` * Execute a class declaration whose parent/interface/trait could not be` |
|        - | 1582 | ` * resolved when the enclosing file compiled (its autoloader had not RUN yet).` |
|        - | 1583 | ` * P3 is the VmDeferredClass record captured by the compiler. A dependency` |
|        - | 1584 | `` * that is STILL missing throws php's catchable `... not found` Error; a`` |
|        - | 1585 | ` * failed re-compile aborts (its errors were already reported). See the` |
|        - | 1586 | ` * deferral block comment in compile_class.c.` |
|        - | 1587 | ` */` |
|       15 | 1588 | `case PH7_OP_CLASS_DEFER: {` |
|       32 | 1589 | `	VmDeferredClass *pDefer = (VmDeferredClass *)pInstr->p3;` |
|       32 | 1590 | `	VmDeferredReq *pMissing = 0;` |
|       32 | 1591 | `	sxi32 rcDecl = SXRET_OK;` |
|       32 | 1592 | `	if( pDefer ){` |
|       32 | 1593 | `		rcDecl = VmExecDeferredClass(&(*pVm),pDefer,&pMissing);` |
|       15 | 1594 | `	}` |
|       32 | 1595 | `	if( pMissing ){` |
|        - | 1596 | `		static const char *azDeferKind[] = { "Class", "Interface", "Trait" };` |
|        - | 1597 | `		char zDeclMsg[520];` |
|       17 | 1598 | `		sxu32 nDeclMsg = SyBufferFormat(zDeclMsg,sizeof(zDeclMsg),"%s \"%.*s\" not found",` |
|        5 | 1599 | `			azDeferKind[pMissing->cKind < 3 ? pMissing->cKind : 0],` |
|       10 | 1600 | `			(int)pMissing->sName.nByte,pMissing->sName.zString);` |
|       12 | 1601 | `		rc = VmThrowFromVm(&(*pVm),"Error",zDeclMsg,nDeclMsg);` |
|       12 | 1602 | `		if( rc == SXERR_ABORT ){` |
|        3 | 1603 | `			goto Abort;` |
|        - | 1604 | `		}` |
|        9 | 1605 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 1606 | `	}` |
|       22 | 1607 | `	if( rcDecl == SXERR_ABORT ){` |
|      ! 0 | 1608 | `		goto Abort;` |
|        - | 1609 | `	}` |
|       22 | 1610 | `	break;` |
|        - | 1611 | `				}` |
|        - | 1612 | `/*` |
|        - | 1613 | ` * CVT_INT: * * *` |
|        - | 1614 | ` *` |
|        - | 1615 | ` * Force the top of the stack to be an integer.` |
|        - | 1616 | ` */` |
|     1100 | 1617 | `case PH7_OP_CVT_INT:` |
|        - | 1618 | `#ifdef UNTRUST` |
|        - | 1619 | `	if( pTos < pStack ){` |
|        - | 1620 | `		goto Abort;` |
|        - | 1621 | `	}` |
|        - | 1622 | `#endif` |
|     2205 | 1623 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      983 | 1624 | `		PH7_MemObjToInteger(pTos);` |
|      489 | 1625 | `	}` |
|        - | 1626 | `	/* Invalidate any prior representation */` |
|     2205 | 1627 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|     2205 | 1628 | `	break;` |
|        - | 1629 | `/*` |
|        - | 1630 | ` * CVT_REAL: * * *` |
|        - | 1631 | ` *` |
|        - | 1632 | ` * Force the top of the stack to be a real.` |
|        - | 1633 | ` */` |
|       88 | 1634 | `case PH7_OP_CVT_REAL:` |
|        - | 1635 | `#ifdef UNTRUST` |
|        - | 1636 | `	if( pTos < pStack ){` |
|        - | 1637 | `		goto Abort;` |
|        - | 1638 | `	}` |
|        - | 1639 | `#endif` |
|      181 | 1640 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       90 | 1641 | `		PH7_MemObjToReal(pTos);` |
|       43 | 1642 | `	}` |
|        - | 1643 | `	/* Invalidate any prior representation */` |
|      181 | 1644 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|      181 | 1645 | `	break;` |
|        - | 1646 | `/*` |
|        - | 1647 | ` * CVT_STR: * * *` |
|        - | 1648 | ` *` |
|        - | 1649 | ` * Force the top of the stack to be a string.` |
|        - | 1650 | ` */` |
|     3116 | 1651 | `case PH7_OP_CVT_STR:` |
|        - | 1652 | `#ifdef UNTRUST` |
|        - | 1653 | `	if( pTos < pStack ){` |
|        - | 1654 | `		goto Abort;` |
|        - | 1655 | `	}` |
|        - | 1656 | `#endif` |
|        - | 1657 | `	/* (string) cast + string interpolation "$arr": php's user-visible` |
|        - | 1658 | `	 * array->string warning site, and the not-stringable-object throw (§2). */` |
|        - | 1659 | `	{` |
|     6237 | 1660 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     6239 | 1661 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 1662 | `	}` |
|     6161 | 1663 | `	break;` |
|        - | 1664 | `/*` |
|        - | 1665 | ` * CVT_BOOL: * * *` |
|        - | 1666 | ` *` |
|        - | 1667 | ` * Force the top of the stack to be a boolean.` |
|        - | 1668 | ` */` |
|       65 | 1669 | `case PH7_OP_CVT_BOOL:` |
|        - | 1670 | `#ifdef UNTRUST` |
|        - | 1671 | `	if( pTos < pStack ){` |
|        - | 1672 | `		goto Abort;` |
|        - | 1673 | `	}` |
|        - | 1674 | `#endif` |
|      131 | 1675 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       39 | 1676 | `		PH7_MemObjToBool(pTos);` |
|       19 | 1677 | `	}` |
|      131 | 1678 | `	break;` |
|        - | 1679 | `/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it` |
|        - | 1680 | ` * always raises "The (unset) cast is no longer supported" (php 8 removed` |
|        - | 1681 | ` * the cast), so a program containing it never compiles. The switch has no` |
|        - | 1682 | ` * default arm, so abort loudly rather than fall through as a silent no-op` |
|        - | 1683 | ` * if a future emitter ever produces one without the compile error. */` |
|      ! 0 | 1684 | `case PH7_OP_CVT_NULL:` |
|      ! 0 | 1685 | `	goto Abort;` |
|        - | 1686 | `/*` |
|        - | 1687 | ` * CVT_NUMC: * * *` |
|        - | 1688 | ` *` |
|        - | 1689 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - | 1690 | ` */` |
|      ! 0 | 1691 | `case PH7_OP_CVT_NUMC:` |
|        - | 1692 | `#ifdef UNTRUST` |
|        - | 1693 | `	if( pTos < pStack ){` |
|        - | 1694 | `		goto Abort;` |
|        - | 1695 | `	}` |
|        - | 1696 | `#endif` |
|        - | 1697 | `	/* Force a numeric cast */` |
|      ! 0 | 1698 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 | 1699 | `	break;` |
|        - | 1700 | `/*` |
|        - | 1701 | ` * CVT_ARRAY: * * *` |
|        - | 1702 | ` *` |
|        - | 1703 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - | 1704 | ` */` |
|       23 | 1705 | `case PH7_OP_CVT_ARRAY:` |
|        - | 1706 | `#ifdef UNTRUST` |
|        - | 1707 | `	if( pTos < pStack ){` |
|        - | 1708 | `		goto Abort;` |
|        - | 1709 | `	}` |
|        - | 1710 | `#endif` |
|        - | 1711 | `	/* Force a hashmap cast */` |
|       51 | 1712 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       51 | 1713 | `	if( rc != SXRET_OK ){` |
|        - | 1714 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 | 1715 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - | 1716 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 | 1717 | `	}` |
|       51 | 1718 | `	break;` |
|        - | 1719 | `/*` |
|        - | 1720 | ` * CVT_OBJ: * * *` |
|        - | 1721 | ` *` |
|        - | 1722 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - | 1723 | ` */` |
|       23 | 1724 | `case PH7_OP_CVT_OBJ:` |
|        - | 1725 | `#ifdef UNTRUST` |
|        - | 1726 | `	if( pTos < pStack ){` |
|        - | 1727 | `		goto Abort;` |
|        - | 1728 | `	}` |
|        - | 1729 | `#endif` |
|       48 | 1730 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1731 | `		/* Force a 'stdClass()' cast */` |
|       48 | 1732 | `		PH7_MemObjToObject(pTos);` |
|       23 | 1733 | `	}` |
|       48 | 1734 | `	break;` |
|        - | 1735 | `/*` |
|        - | 1736 | ` * ERR_CTRL * * *` |
|        - | 1737 | ` *` |
|        - | 1738 | ` * Error control operator.` |
|        - | 1739 | ` */` |
|     3497 | 1740 | `case PH7_OP_UNSET_VAR: {` |
|        - | 1741 | `	VmOpRc rcOp;` |
|     6999 | 1742 | `	sState.pTos = pTos;` |
|     6999 | 1743 | `	sState.pc = pc;` |
|     6999 | 1744 | `	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);` |
|     6999 | 1745 | `	pTos = sState.pTos;` |
|     6999 | 1746 | `	pc = sState.pc;` |
|     6999 | 1747 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 1748 | `		goto Abort;` |
|     6997 | 1749 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 1750 | `		goto Exception;` |
|        - | 1751 | `	}` |
|     6997 | 1752 | `	break;` |
|        - | 1753 | `					  }` |
|    36474 | 1754 | `case PH7_OP_ERR_CTRL:` |
|        - | 1755 | `	/*` |
|        - | 1756 | `	 * Error-control operator '@'. Emitted as a PAIR around the suppressed` |
|        - | 1757 | `	 * expression: iP1=1 opens the window (before the operand is evaluated),` |
|        - | 1758 | `	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),` |
|        - | 1759 | `	 * which went unnoticed only because the engine raised so few diagnostics;` |
|        - | 1760 | `	 * every warning/notice/deprecation the parity work added leaked straight` |
|        - | 1761 | ``	 * through `@`. The window nests, and php 8 does NOT let '@' swallow`` |
|        - | 1762 | `	 * fatals or exceptions — those unwind past the closing instruction, so` |
|        - | 1763 | `	 * the depth is reset on the exception path rather than decremented here.` |
|        - | 1764 | `	 */` |
|    72953 | 1765 | `	if( pInstr->iP1 ){` |
|    36533 | 1766 | `		pVm->nErrSuppress++;` |
|    54689 | 1767 | `	}else if( pVm->nErrSuppress > 0 ){` |
|    36425 | 1768 | `		pVm->nErrSuppress--;` |
|    18210 | 1769 | `	}` |
|    72953 | 1770 | `	break;` |
|        - | 1771 | `/*` |
|        - | 1772 | ` * IS_A * * *` |
|        - | 1773 | ` *` |
|        - | 1774 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - | 1775 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - | 1776 | ` * holding a class name or an object).` |
|        - | 1777 | ` * Push TRUE on success. FALSE otherwise.` |
|        - | 1778 | ` */` |
|      533 | 1779 | `case PH7_OP_IS_A:{` |
|     1071 | 1780 | `	ph7_value *pNos = &pTos[-1];` |
|     1071 | 1781 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - | 1782 | `#ifdef UNTRUST` |
|        - | 1783 | `	if( pNos < pStack ){` |
|        - | 1784 | `		goto Abort;` |
|        - | 1785 | `	}` |
|        - | 1786 | `#endif` |
|     1071 | 1787 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      657 | 1788 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      657 | 1789 | `		ph7_class *pClass = 0;` |
|        - | 1790 | `		/* Extract the target class */` |
|      657 | 1791 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - | 1792 | `			/* Instance already loaded */` |
|      ! 0 | 1793 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      657 | 1794 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      657 | 1795 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      657 | 1796 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - | 1797 | `			/* Handle self/static/parent keywords */` |
|      657 | 1798 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        6 | 1799 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      655 | 1800 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 | 1801 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      652 | 1802 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        6 | 1803 | `				pClass = PH7_VmResolveParentClass(&(*pVm));` |
|        4 | 1804 | `			}else{` |
|      647 | 1805 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - | 1806 | `			}` |
|      326 | 1807 | `		}` |
|      657 | 1808 | `		if( pClass ){` |
|        - | 1809 | `			/* Perform the query */` |
|      655 | 1810 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|      325 | 1811 | `		}` |
|      326 | 1812 | `	}` |
|        - | 1813 | `	/* Push result */` |
|     1071 | 1814 | `	VmPopOperand(&pTos,1);` |
|     1071 | 1815 | `	PH7_MemObjRelease(pTos);` |
|     1071 | 1816 | `	pTos->x.iVal = iRes;` |
|     1071 | 1817 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     1071 | 1818 | `	break;` |
|        - | 1819 | `				 }` |
|        - | 1820 |  |
|        - | 1821 | `/*` |
|        - | 1822 | ` * LOADC P1 P2 *` |
|        - | 1823 | ` *` |
|        - | 1824 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - | 1825 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - | 1826 | ` */` |
| 10213673 | 1827 | `case PH7_OP_LOADC: {` |
|        - | 1828 | `	ph7_value *pObj;` |
|        - | 1829 | `	/* Reserve a room */` |
| 20431126 | 1830 | `	pTos++;` |
| 20431126 | 1831 | `	if( pInstr->iP1 & PH7_LOADC_NOKEY ){` |
|        - | 1832 | `		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */` |
|   723665 | 1833 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|   723665 | 1834 | `		SyBlobReset(&pTos->sBlob);` |
|   723665 | 1835 | `		pTos->iFlags \|= MEMOBJ_AUX_NOKEY;` |
|   723665 | 1836 | `		pTos->nIdx = SXU32_HIGH;` |
|   723665 | 1837 | `		break;` |
|        - | 1838 | `	}` |
| 19707466 | 1839 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
| 19707466 | 1840 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - | 1841 | `			SyHashEntry *pEntry;` |
|        - | 1842 | `			/* The name php looks up FIRST for an unqualified constant is decided at` |
|        - | 1843 | ``			 * COMPILE time and travels in p3: a `use const` import's FQN, or`` |
|        - | 1844 | ``			 * `current-namespace\NAME`. Absent (global scope, or an already-qualified`` |
|        - | 1845 | `			 * literal), the bare literal is the only name there is. Resolving it here` |
|        - | 1846 | `			 * rather than against the RUNTIME namespace is what makes a function keep` |
|        - | 1847 | `			 * its own namespace when it is called from another one, and what lets a` |
|        - | 1848 | `			 * namespaced constant shadow a global one of the same short name. */` |
|   126635 | 1849 | `			const char *zCand = (const char *)pInstr->p3;` |
|   126635 | 1850 | `			const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|   126635 | 1851 | `			sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|   126635 | 1852 | `			if( zCand ){` |
|       55 | 1853 | `				pEntry = SyHashGet(&pVm->hConstant,zCand,SyStrlen(zCand));` |
|       55 | 1854 | `				if( pEntry ){` |
|       47 | 1855 | `					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       47 | 1856 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       47 | 1857 | `					SyBlobReset(&pTos->sBlob);` |
|       47 | 1858 | `					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|       47 | 1859 | `					pTos->nIdx = SXU32_HIGH;` |
|       47 | 1860 | `					break;` |
|        - | 1861 | `				}` |
|        4 | 1862 | `			}` |
|        - | 1863 | `			/* The GLOBAL step — skipped when the candidate came from an import, which` |
|        - | 1864 | `			 * php resolves without any fallback. */` |
|   126593 | 1865 | `			if( (pInstr->iP1 & PH7_LOADC_NOGLOBAL) == 0 ){` |
|   126591 | 1866 | `				pEntry = SyHashGet(&pVm->hConstant,(const void *)zLit,nLit);` |
|   126591 | 1867 | `				if( pEntry ){` |
|   126423 | 1868 | `					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - | 1869 | `					/* Set a NULL default value */` |
|   126423 | 1870 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|   126423 | 1871 | `					SyBlobReset(&pTos->sBlob);` |
|        - | 1872 | `					/* Invoke the callback and deal with the expanded value */` |
|   126423 | 1873 | `					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|        - | 1874 | `					/* Mark as constant */` |
|   126423 | 1875 | `					pTos->nIdx = SXU32_HIGH;` |
|   126423 | 1876 | `					break;` |
|        - | 1877 | `				}` |
|       84 | 1878 | `			}` |
|        - | 1879 | `			{` |
|        - | 1880 | `				/*` |
|        - | 1881 | `				 * php 8 has no bare-word fallback: an unresolved constant is a catchable` |
|        - | 1882 | `				 * Error, not its own name as a string. PH7 answered "X" for an unknown` |
|        - | 1883 | `				 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —` |
|        - | 1884 | `				 * silently became a string and flowed on. php names the name it looked` |
|        - | 1885 | `				 * for FIRST, so the message reports the candidate when there was one` |
|        - | 1886 | ``				 * ("Undefined constant \"B\NOPE\"" inside `namespace B;`).`` |
|        - | 1887 | `				 *` |
|        - | 1888 | `				 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call` |
|        - | 1889 | ``				 * boundary, so neither a bare `break` nor `goto Exception` is correct`` |
|        - | 1890 | `				 * here (see the macro).` |
|        - | 1891 | `				 */` |
|        - | 1892 | `				SyBlob sMsg;` |
|      174 | 1893 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      174 | 1894 | `				if( zCand ){` |
|        8 | 1895 | `					SyBlobFormat(&sMsg,"Undefined constant \"%s\"",zCand);` |
|        5 | 1896 | `				}else{` |
|      168 | 1897 | `					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);` |
|        - | 1898 | `				}` |
|      174 | 1899 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      174 | 1900 | `				SyBlobReset(&pTos->sBlob);` |
|      174 | 1901 | `				pTos->nIdx = SXU32_HIGH;` |
|      259 | 1902 | `				rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|       85 | 1903 | `					SyBlobLength(&sMsg));` |
|      174 | 1904 | `				SyBlobRelease(&sMsg);` |
|      174 | 1905 | `				if( rc == SXERR_ABORT ){` |
|       49 | 1906 | `					goto Abort;` |
|        - | 1907 | `				}` |
|      148 | 1908 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 1909 | `			}` |
|        - | 1910 | `		}` |
| 19580836 | 1911 | `		PH7_MemObjLoad(pObj,pTos);` |
|  9792308 | 1912 | `	}else{` |
|        - | 1913 | `		/* Set a NULL value */` |
|      ! 0 | 1914 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 1915 | `	}` |
|        - | 1916 | `	/* Mark as constant */` |
| 19580836 | 1917 | `	pTos->nIdx = SXU32_HIGH;` |
| 19580836 | 1918 | `	break;` |
|        - | 1919 | `				  }` |
|        - | 1920 | `/*` |
|        - | 1921 | ` * LOAD: P1 * P3` |
|        - | 1922 | ` *` |
|        - | 1923 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - | 1924 | ` * from the P3 operand.` |
|        - | 1925 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - | 1926 | ` * the variable if non existent and push the NULL constant instead.` |
|        - | 1927 | ` */` |
|  7666271 | 1928 | `case PH7_OP_LOAD:{` |
|        - | 1929 | `	ph7_value *pObj;` |
|        - | 1930 | `	SyString sName;` |
| 15340370 | 1931 | `	if( pInstr->p3 == 0 ){` |
|        - | 1932 | `		/* Take the variable name from the top of the stack */` |
|        - | 1933 | `#ifdef UNTRUST` |
|        - | 1934 | `		if( pTos < pStack ){` |
|        - | 1935 | `			goto Abort;` |
|        - | 1936 | `		}` |
|        - | 1937 | `#endif` |
|        - | 1938 | `		/* Force a string cast — variable-variable name $$arr (user-visible, §2) */` |
|        - | 1939 | `		{` |
|       35 | 1940 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|  7666305 | 1941 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 1942 | `		}` |
|       33 | 1943 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       18 | 1944 | `	}else{` |
| 15340338 | 1945 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 1946 | `		/* Reserve a room for the target object */` |
| 15340338 | 1947 | `		pTos++;` |
|        - | 1948 | `	}` |
| 15340368 | 1949 | `	if( pInstr->iP2 == 2 ){` |
|        - | 1950 | ``		/* Read-modify-write target (`$x++`, `$x .= 'a'`): php reads the variable`` |
|        - | 1951 | `		 * before writing, so it warns when it does not exist and THEN seeds it.` |
|        - | 1952 | `		 * Peek first (no create) purely to raise that warning; the load below` |
|        - | 1953 | ``		 * still creates the slot the operator needs. A plain `=` never gets here`` |
|        - | 1954 | `		 * — it writes without reading, and stays silent, as php does. */` |
|   681480 | 1955 | `		if( VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE) == 0 ){` |
|        7 | 1956 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        3 | 1957 | `		}` |
|   340943 | 1958 | `	}` |
|        - | 1959 | `	/* Extract the requested memory object */` |
| 15340368 | 1960 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
| 15340368 | 1961 | `	if( pObj == 0 ){` |
|      123 | 1962 | `		if( pInstr->iP1 ){` |
|        - | 1963 | `			/* Reading a variable that does not exist. php raises E_WARNING` |
|        - | 1964 | `			 * "Undefined variable $x" and evaluates it as NULL; PHL used to` |
|        - | 1965 | `			 * yield NULL silently, which hid typo'd names. iP2 marks the reads` |
|        - | 1966 | `			 * that must stay quiet (isset/empty — see PH7_CompileVariable);` |
|        - | 1967 | `			 * vivifying contexts never get here because they pass iP1 = 0. */` |
|      123 | 1968 | `			if( pInstr->iP2 == 0 ){` |
|       38 | 1969 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|       17 | 1970 | `			}` |
|        - | 1971 | `			/* Variable not found,load NULL */` |
|      123 | 1972 | `			if( !pInstr->p3 ){` |
|       10 | 1973 | `				PH7_MemObjRelease(pTos);` |
|        6 | 1974 | `			}else{` |
|      115 | 1975 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 1976 | `			}` |
|      123 | 1977 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|      123 | 1978 | `			if( pInstr->iP2 == 3 ){` |
|        - | 1979 | `				/* D1 deferred call argument: the variable does not exist, but we do not` |
|        - | 1980 | `				 * know yet whether the callee wants it by-ref (materialize + bind) or` |
|        - | 1981 | `				 * by-value (warn + pass NULL). Tag the slot and stash the variable name` |
|        - | 1982 | `				 * (a VM-lifetime bytecode string, nothing to free) so OP_CALL's` |
|        - | 1983 | `				 * VmResolveDeferredArgs can decide once the callee is resolved. */` |
|       12 | 1984 | `				pTos->iFlags \|= MEMOBJ_AUX_DEFERRED;` |
|       12 | 1985 | `				pTos->x.pOther = pInstr->p3;` |
|        5 | 1986 | `			}` |
|      123 | 1987 | `			break;` |
|      ! 0 | 1988 | `		}else{` |
|        - | 1989 | `			/* Fatal error */` |
|      ! 0 | 1990 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 1991 | `			goto Abort;` |
|        - | 1992 | `		}` |
|        - | 1993 | `	}` |
|        - | 1994 | `	/* Load variable contents */` |
| 15340250 | 1995 | `	PH7_MemObjLoad(pObj,pTos);` |
| 15340250 | 1996 | `	pTos->nIdx = pObj->nIdx;` |
| 15340250 | 1997 | `	break;` |
|        - | 1998 | `				   }` |
|        - | 1999 | `/*` |
|        - | 2000 | ` * LOAD_MAP P1 * *` |
|        - | 2001 | ` *` |
|        - | 2002 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - | 2003 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - | 2004 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - | 2005 | ` */` |
|    41843 | 2006 | `case PH7_OP_LOAD_MAP: {` |
|        - | 2007 | `	VmOpRc rcOp;` |
|    83691 | 2008 | `	sState.pTos = pTos;` |
|    83691 | 2009 | `	sState.pc = pc;` |
|    83691 | 2010 | `	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);` |
|    83691 | 2011 | `	pTos = sState.pTos;` |
|    83691 | 2012 | `	pc = sState.pc;` |
|    83691 | 2013 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2014 | `		goto Abort;` |
|    83691 | 2015 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       15 | 2016 | `		goto Exception;` |
|        - | 2017 | `	}` |
|    83677 | 2018 | `	break;` |
|        - | 2019 | `					  }` |
|        - | 2020 | `/*` |
|        - | 2021 | ` * LOAD_LIST: P1 * *` |
|        - | 2022 | ` *` |
|        - | 2023 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - | 2024 | ` * This is the VM implementation of the list() PHP construct.` |
|        - | 2025 | ` * Caveats:` |
|        - | 2026 | ` *  This implementation support only a single nesting level.` |
|        - | 2027 | ` */` |
|      242 | 2028 | `case PH7_OP_LOAD_LIST: {` |
|        - | 2029 | `	VmOpRc rcOp;` |
|      489 | 2030 | `	sState.pTos = pTos;` |
|      489 | 2031 | `	sState.pc = pc;` |
|      489 | 2032 | `	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);` |
|      489 | 2033 | `	pTos = sState.pTos;` |
|      489 | 2034 | `	pc = sState.pc;` |
|      489 | 2035 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2036 | `		goto Abort;` |
|      489 | 2037 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2038 | `		goto Exception;` |
|        - | 2039 | `	}` |
|      489 | 2040 | `	break;` |
|        - | 2041 | `					  }` |
|        - | 2042 | `/*` |
|        - | 2043 | ` * LOAD_IDX: P1 P2 *` |
|        - | 2044 | ` *` |
|        - | 2045 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - | 2046 | ` * from the stack.` |
|        - | 2047 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - | 2048 | ` * instead.` |
|        - | 2049 | ` */` |
|   455892 | 2050 | `case PH7_OP_LOAD_IDX: {` |
|        - | 2051 | `	VmOpRc rcOp;` |
|   913302 | 2052 | `	sState.pTos = pTos;` |
|   913302 | 2053 | `	sState.pc = pc;` |
|   913302 | 2054 | `	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);` |
|   913302 | 2055 | `	pTos = sState.pTos;` |
|   913302 | 2056 | `	pc = sState.pc;` |
|   913302 | 2057 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2058 | `		goto Abort;` |
|   913302 | 2059 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       59 | 2060 | `		goto Exception;` |
|        - | 2061 | `	}` |
|   913246 | 2062 | `	break;` |
|        - | 2063 | `					  }` |
|        - | 2064 | `/*` |
|        - | 2065 | ` * LOAD_CLOSURE * * P3` |
|        - | 2066 | ` *` |
|        - | 2067 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - | 2068 | ` * name in the stack.` |
|        - | 2069 | ` */` |
|     1755 | 2070 | `case PH7_OP_LOAD_CLOSURE: {` |
|        - | 2071 | `	VmOpRc rcOp;` |
|     3515 | 2072 | `	sState.pTos = pTos;` |
|     3515 | 2073 | `	sState.pc = pc;` |
|     3515 | 2074 | `	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);` |
|     3515 | 2075 | `	pTos = sState.pTos;` |
|     3515 | 2076 | `	pc = sState.pc;` |
|     3515 | 2077 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2078 | `		goto Abort;` |
|     3515 | 2079 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2080 | `		goto Exception;` |
|        - | 2081 | `	}` |
|     3515 | 2082 | `	break;` |
|        - | 2083 | `					  }` |
|        - | 2084 | `/*` |
|        - | 2085 | ` * LOAD_FCC P1 * *` |
|        - | 2086 | ` *` |
|        - | 2087 | ` * First-class callable: wrap the callee in a Closure object instead of calling it.` |
|        - | 2088 | ` *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS` |
|        - | 2089 | ` *           (from the callee's OP_LOADC). Replace it in place with a Closure whose` |
|        - | 2090 | ` *           $__fn is that name; the existing string-callable dispatch resolves it.` |
|        - | 2091 | ` *           (OOM degrades to leaving the name string on the stack — still callable.)` |
|        - | 2092 | ` *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name` |
|        - | 2093 | ` *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds` |
|        - | 2094 | ` *           $this (scope = its class); a class-name-string target is a static callable` |
|        - | 2095 | ` *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —` |
|        - | 2096 | ` *           the popped target leaves no name string to keep.)` |
|        - | 2097 | ` */` |
|       52 | 2098 | `case PH7_OP_LOAD_FCC:{` |
|      107 | 2099 | `	if( pInstr->iP1 == 1 ){` |
|        - | 2100 | ``		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)`` |
|        - | 2101 | `		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —` |
|        - | 2102 | `		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,` |
|        - | 2103 | ``		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a`` |
|        - | 2104 | `		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is` |
|        - | 2105 | `		 * (graceful degradation), and so is the original on OOM — still whatever it was. */` |
|        - | 2106 | `		ph7_class_instance *pCloObj;` |
|       65 | 2107 | `		if( VmValueIsClosure(pVm, pTos) ){` |
|        3 | 2108 | `			break;` |
|        - | 2109 | `		}` |
|       63 | 2110 | `		pCloObj = VmFccWrapValue(pVm, pTos);` |
|       63 | 2111 | `		if( pCloObj ){` |
|       63 | 2112 | `			PH7_MemObjRelease(pTos);` |
|       63 | 2113 | `			pCloObj->iRef++;` |
|       63 | 2114 | `			pTos->x.pOther = pCloObj;` |
|       63 | 2115 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       30 | 2116 | `		}` |
|       33 | 2117 | `	}else{` |
|        - | 2118 | `		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]` |
|        - | 2119 | `		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its` |
|        - | 2120 | `		 * class); a class-name string target is a static callable (scope = that class). */` |
|       44 | 2121 | `		ph7_value *pTarget = &pTos[-1];` |
|        - | 2122 | `		SyString sName;` |
|        - | 2123 | `		ph7_class_instance *pCloObj;` |
|       44 | 2124 | `		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));` |
|       44 | 2125 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|       24 | 2126 | `			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;` |
|       24 | 2127 | `			pCloObj = VmCreateClosure(pVm, &sName, pBoundThis, &pBoundThis->pClass->sName);` |
|       32 | 2128 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|        - | 2129 | ``			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class`` |
|        - | 2130 | `			 * now, so the closure binds the concrete scope (matching PHP). */` |
|       21 | 2131 | `			ph7_class *pScopeCls = VmFccResolveScope(pVm, pTarget);` |
|       21 | 2132 | `			pCloObj = pScopeCls ? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;` |
|       11 | 2133 | `		}else{` |
|      ! 0 | 2134 | `			pCloObj = 0;` |
|        - | 2135 | `		}` |
|        - | 2136 | `		/* Pop the method name and the target, push the Closure. */` |
|       44 | 2137 | `		PH7_MemObjRelease(pTos);` |
|       44 | 2138 | `		pTos--;` |
|       44 | 2139 | `		PH7_MemObjRelease(pTos);` |
|       44 | 2140 | `		if( pCloObj ){` |
|       44 | 2141 | `			pCloObj->iRef++;` |
|       44 | 2142 | `			pTos->x.pOther = pCloObj;` |
|       44 | 2143 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       23 | 2144 | `		}else{` |
|      ! 0 | 2145 | `			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */` |
|        - | 2146 | `		}` |
|        - | 2147 | `	}` |
|      105 | 2148 | `	break;` |
|        - | 2149 | `					 }` |
|        - | 2150 | `/*` |
|        - | 2151 | ` * STORE * P2 P3` |
|        - | 2152 | ` *` |
|        - | 2153 | ` * Perform a store (Assignment) operation.` |
|        - | 2154 | ` */` |
|  1906959 | 2155 | `case PH7_OP_STORE: {` |
|        - | 2156 | `	ph7_value *pObj;` |
|        - | 2157 | `	SyString sName;` |
|        - | 2158 | `#ifdef UNTRUST` |
|        - | 2159 | `	if( pTos < pStack ){` |
|        - | 2160 | `		goto Abort;` |
|        - | 2161 | `	}` |
|        - | 2162 | `#endif` |
|  3815507 | 2163 | `	if( pInstr->iP2 ){` |
|        - | 2164 | `		sxu32 nIdx;` |
|        - | 2165 | `		sxi32 rcT;` |
|        - | 2166 | `		/* Member store operation */` |
|  3006747 | 2167 | `		nIdx = pTos->nIdx;` |
|  3006747 | 2168 | `		VmPopOperand(&pTos,1);` |
|  3006747 | 2169 | `		if( pVm->pMagicSetThis ){` |
|        - | 2170 | `			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain` |
|        - | 2171 | `			 * store to a missing/inaccessible property on a class declaring __set.` |
|        - | 2172 | `			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on` |
|        - | 2173 | `			 * the stack as the assignment expression's result — php's semantics` |
|        - | 2174 | `			 * (no property is created; a throw rides the boundary rail). */` |
|       17 | 2175 | `			ph7_class_instance *pSetThis = pVm->pMagicSetThis;` |
|        - | 2176 | `			SyString sSetName;` |
|       17 | 2177 | `			pVm->pMagicSetThis = 0;` |
|       17 | 2178 | `			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));` |
|       17 | 2179 | `			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);` |
|       17 | 2180 | `			PH7_ClassInstanceUnref(pSetThis);` |
|       17 | 2181 | `			SyBlobReset(&pVm->sMagicSetName);` |
|       17 | 2182 | `			break;` |
|        - | 2183 | `		}` |
|  3006733 | 2184 | `		if( pVm->pHookSetThis ){` |
|        - | 2185 | `			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a` |
|        - | 2186 | `			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with` |
|        - | 2187 | `			 * the rvalue (which stays on the stack as the assignment expression's` |
|        - | 2188 | ``			 * result); a `set => expr` hook's return value is stored into the`` |
|        - | 2189 | `			 * BACKING slot with the ordinary typed enforcement. A property with` |
|        - | 2190 | `			 * only a get hook is php's catchable "is read-only" Error. */` |
|       55 | 2191 | `			ph7_class_instance *pHThis = pVm->pHookSetThis;` |
|       55 | 2192 | `			ph7_class_attr *pHAttr = pVm->pHookSetAttr;` |
|       55 | 2193 | `			sxu32 nBackIdx = pVm->nHookSetIdx;` |
|        - | 2194 | `			sxi32 rcHs;` |
|       55 | 2195 | `			pVm->pHookSetThis = 0;` |
|       55 | 2196 | `			pVm->pHookSetAttr = 0;` |
|       55 | 2197 | `			pVm->nHookSetIdx = SXU32_HIGH;` |
|       55 | 2198 | `			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);` |
|       55 | 2199 | `			PH7_ClassInstanceUnref(pHThis);` |
|       55 | 2200 | `			if( rcHs == PH7_ABORT ){` |
|      ! 0 | 2201 | `				goto Abort;` |
|        - | 2202 | `			}` |
|       55 | 2203 | `			break;` |
|        - | 2204 | `		}` |
|  3006681 | 2205 | `		if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 2206 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 2207 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|      ! 0 | 2208 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 2209 | `		}else{` |
|        - | 2210 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - | 2211 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|  3006681 | 2212 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);` |
|  3006681 | 2213 | `			if( rcT == PH7_ABORT ){` |
|       13 | 2214 | `				goto Abort;` |
|        - | 2215 | `			}` |
|  3006671 | 2216 | `			if( rcT == PH7_EXCEPTION ){` |
|        - | 2217 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - | 2218 | `				 * control to the nearest catch block if any (draining any` |
|        - | 2219 | `				 * abandoned outer-expression operands to the try's base),` |
|        - | 2220 | `				 * otherwise propagate out of the VM loop. */` |
|   100103 | 2221 | `				VmPopOperand(&pTos,1);` |
|        - | 2222 | `				{` |
|        - | 2223 | `					sxi32 iRp;` |
|   100103 | 2224 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|   400085 | 2225 | `						PH7_RESUME_DRAIN()` |
|   100083 | 2226 | `						pc = iRp;` |
|   100083 | 2227 | `						break;` |
|        - | 2228 | `					}` |
|        - | 2229 | `				}` |
|       24 | 2230 | `				goto Exception;` |
|        - | 2231 | `			}` |
|        - | 2232 | `			/* Point to the desired memory object */` |
|  2906573 | 2233 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2906573 | 2234 | `			if( pObj ){` |
|        - | 2235 | `				/* Perform the store operation */` |
|  2906573 | 2236 | `				PH7_MemObjStore(pTos,pObj);` |
|  1453284 | 2237 | `			}` |
|        - | 2238 | `		}` |
|  2906573 | 2239 | `		break;` |
|   808765 | 2240 | `	}else if( pInstr->p3 == 0 ){` |
|        - | 2241 | `		/* Take the variable name from the next on the stack (user-visible: a` |
|        - | 2242 | `		 * variable-variable NAME $$arr warns on an array, §2) */` |
|        - | 2243 | `		{` |
|       20 | 2244 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       20 | 2245 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2246 | `		}` |
|       18 | 2247 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       18 | 2248 | `		pTos--;` |
|        - | 2249 | `#ifdef UNTRUST` |
|        - | 2250 | `		if( pTos < pStack  ){` |
|        - | 2251 | `			goto Abort;` |
|        - | 2252 | `		}` |
|        - | 2253 | `#endif` |
|       10 | 2254 | `	}else{` |
|   808747 | 2255 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 2256 | `	}` |
|   808758 | 2257 | `	if( sName.nByte == sizeof("GLOBALS")-1` |
|   406645 | 2258 | `	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){` |
|        6 | 2259 | `		if( pInstr->p3 ){` |
|        - | 2260 | `			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time` |
|        - | 2261 | `			 * fatal there; raised at the store site here with the same` |
|        - | 2262 | `			 * message and the same non-catchable outcome). Element writes` |
|        - | 2263 | `			 * are unaffected. */` |
|        3 | 2264 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 2265 | `				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 2266 | `			pVm->iExitStatus = 255;` |
|        3 | 2267 | `			pVm->bHaltRequested = 1;` |
|        3 | 2268 | `			goto Abort;` |
|        - | 2269 | `		}` |
|        - | 2270 | `		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's` |
|        - | 2271 | `		 * compile-time case: php quietly creates an ordinary symbol-table` |
|        - | 2272 | `		 * entry named GLOBALS, leaving the auto-global view intact. */` |
|        3 | 2273 | `		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);` |
|        3 | 2274 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 2275 | `		break;` |
|        - | 2276 | `	}` |
|        - | 2277 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   808759 | 2278 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   808759 | 2279 | `	if( pObj == 0 ){` |
|      ! 0 | 2280 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 2281 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 2282 | `		goto Abort;` |
|        - | 2283 | `	}` |
|   808759 | 2284 | `	if( !pInstr->p3 ){` |
|       16 | 2285 | `		PH7_MemObjRelease(&pTos[1]);` |
|        7 | 2286 | `	}` |
|        - | 2287 | `	/* Perform the store operation */` |
|   808759 | 2288 | `	PH7_MemObjStore(pTos,pObj);` |
|   808759 | 2289 | `	break;` |
|        - | 2290 | `				   }` |
|        - | 2291 | `/*` |
|        - | 2292 | ` * STORE_IDX:   P1 * P3` |
|        - | 2293 | ` * STORE_IDX_R: P1 * P3` |
|        - | 2294 | ` *` |
|        - | 2295 | ` * Perfrom a store operation an a hashmap entry.` |
|        - | 2296 | ` */` |
|   146597 | 2297 | `case PH7_OP_STORE_IDX:` |
|        - | 2298 | `case PH7_OP_STORE_IDX_REF: {` |
|        - | 2299 | `	VmOpRc rcOp;` |
|   293199 | 2300 | `	sState.pTos = pTos;` |
|   293199 | 2301 | `	sState.pc = pc;` |
|   293199 | 2302 | `	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);` |
|   293199 | 2303 | `	pTos = sState.pTos;` |
|   293199 | 2304 | `	pc = sState.pc;` |
|   293199 | 2305 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 2306 | `		goto Abort;` |
|   293197 | 2307 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       36 | 2308 | `		goto Exception;` |
|        - | 2309 | `	}` |
|   293165 | 2310 | `	break;` |
|        - | 2311 | `					  }` |
|        - | 2312 | `/*` |
|        - | 2313 | ` * INCR: P1 * *` |
|        - | 2314 | ` *` |
|        - | 2315 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - | 2316 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - | 2317 | ` * the stack and increment after that.` |
|        - | 2318 | ` */` |
|   318470 | 2319 | `case PH7_OP_INCR:` |
|        - | 2320 | `#ifdef UNTRUST` |
|        - | 2321 | `	if( pTos < pStack ){` |
|        - | 2322 | `		goto Abort;` |
|        - | 2323 | `	}` |
|        - | 2324 | `#endif` |
|        - | 2325 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|        - | 2326 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 2327 | `	 * — which otherwise skips object/array/resource operands. */` |
|   637363 | 2328 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 2329 | `	/* php refuses to increment a string OFFSET, whatever byte it holds. */` |
|   637353 | 2330 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 2331 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 2332 | `	 * php's TypeError, raised BEFORE any set dispatch (the type guard below` |
|        - | 2333 | `	 * would skip the mutation and the tail write-back would otherwise call` |
|        - | 2334 | `	 * the set hook with the unchanged value). */` |
|   637334 | 2335 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|   318889 | 2336 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 2337 | `		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 2338 | `		if( pTopInc->iKind == VM_HOOK_PEND_RMW && pTopInc->nScratchIdx == pTos->nIdx ){` |
|        - | 2339 | `			SyBlob sErrMsg;` |
|        3 | 2340 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 2341 | `			VmIncDecTypeErrorMsg(pTos,TRUE,&sErrMsg);` |
|        3 | 2342 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 2343 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 2344 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 2345 | `			break;` |
|        - | 2346 | `		}` |
|      ! 0 | 2347 | `	}` |
|   637337 | 2348 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 2349 | `		/* php's ++ operand contract: an array, object or resource is a catchable` |
|        - | 2350 | `		 * TypeError, not the silent no-op this used to be. Settle the operand` |
|        - | 2351 | `		 * (the op's result slot) BEFORE throwing, like the arithmetic sites. */` |
|        - | 2352 | `		SyBlob sIncMsg;` |
|        - | 2353 | `		sxi32 rcInc;` |
|       21 | 2354 | `		SyBlobInit(&sIncMsg,&pVm->sAllocator);` |
|       21 | 2355 | `		VmIncDecTypeErrorMsg(pTos,TRUE,&sIncMsg);` |
|       21 | 2356 | `		PH7_MemObjRelease(pTos);` |
|       21 | 2357 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       21 | 2358 | `		pTos->nIdx = SXU32_HIGH;` |
|       31 | 2359 | `		rcInc = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sIncMsg),` |
|       10 | 2360 | `			SyBlobLength(&sIncMsg));` |
|       21 | 2361 | `		SyBlobRelease(&sIncMsg);` |
|       21 | 2362 | `		if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|       21 | 2363 | `		rc = rcInc;` |
|       23 | 2364 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2365 | `	}` |
|        - | 2366 | ``	/* php 8.3+: `++` on a BOOL is a no-op it warns about (E_WARNING, errno 2 —`` |
|        - | 2367 | ``	 * same shape as the null `--` below), where PH7 coerced true to int 2. */`` |
|   637317 | 2368 | `	if( pTos->iFlags & MEMOBJ_BOOL ){` |
|        7 | 2369 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 2370 | `			"Increment on type bool has no effect, this will change in the next major version of PHP");` |
|        3 | 2371 | `	}` |
|   637317 | 2372 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_BOOL)) == 0 ){` |
|   637311 | 2373 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 2374 | `			ph7_value *pObj;` |
|   637311 | 2375 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   637311 | 2376 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 2377 | `					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric` |
|        - | 2378 | `					 * string (it points at str_increment() instead); PHL rejects it. */` |
|        - | 2379 | `					SyBlob sErrMsg;` |
|        3 | 2380 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 2381 | `					SyBlobAppend(&sErrMsg,` |
|        - | 2382 | `						"Increment on a non-numeric string is not supported, use str_increment() instead",` |
|        - | 2383 | `						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);` |
|        3 | 2384 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 2385 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 2386 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 2387 | `					break;` |
|      ! 0 | 2388 | `				}else{` |
|        - | 2389 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - | 2390 | `					 * original value: pTos may alias pObj's blob via` |
|        - | 2391 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - | 2392 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - | 2393 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - | 2394 | `					 * so its old-value view survives the coercion. */` |
|   637309 | 2395 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 | 2396 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        5 | 2397 | `					}` |
|        - | 2398 | `					/* Force a numeric cast on the variable */` |
|   637309 | 2399 | `					PH7_MemObjToNumeric(pObj);` |
|   637309 | 2400 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        5 | 2401 | `						pObj->rVal++;` |
|        - | 2402 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 2403 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 2404 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 2405 | `						 * integer-valued real. */` |
|        5 | 2406 | `						PH7_MemObjTryInteger(pObj);` |
|        3 | 2407 | `					}else{` |
|        - | 2408 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 2409 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 2410 | `						sxi64 r;` |
|   637305 | 2411 | `						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 2412 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        5 | 2413 | `							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;` |
|        5 | 2414 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 2415 | `#else` |
|        - | 2416 | `							pObj->x.iVal = r;` |
|        - | 2417 | `#endif` |
|        3 | 2418 | `						}else{` |
|   637301 | 2419 | `							pObj->x.iVal = r;` |
|        - | 2420 | `						}` |
|        - | 2421 | `					}` |
|   637309 | 2422 | `					if( pInstr->iP1 ){` |
|        - | 2423 | `						/* Pre-increment: result is the new value. */` |
|      176 | 2424 | `						PH7_MemObjStore(pObj,pTos);` |
|       87 | 2425 | `					}` |
|        - | 2426 | `					/* Post-increment: pTos retains the old value (a string` |
|        - | 2427 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - | 2428 | `				}` |
|   318858 | 2429 | `			}` |
|   318863 | 2430 | `		}else{` |
|      ! 0 | 2431 | `			if( pInstr->iP1 ){` |
|      ! 0 | 2432 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 | 2433 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 | 2434 | `				}else{` |
|        - | 2435 | `					/* Force a numeric cast */` |
|      ! 0 | 2436 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 2437 | `					/* Pre-increment */` |
|      ! 0 | 2438 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 2439 | `						pTos->rVal++;` |
|        - | 2440 | `						/* Try to get an integer representation */` |
|      ! 0 | 2441 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 2442 | `					}else{` |
|        - | 2443 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 2444 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 2445 | `						sxi64 r;` |
|      ! 0 | 2446 | `						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 2447 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 2448 | `							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;` |
|      ! 0 | 2449 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 2450 | `#else` |
|        - | 2451 | `							pTos->x.iVal = r;` |
|        - | 2452 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 2453 | `#endif` |
|      ! 0 | 2454 | `						}else{` |
|      ! 0 | 2455 | `							pTos->x.iVal = r;` |
|      ! 0 | 2456 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 2457 | `						}` |
|        - | 2458 | `					}` |
|        - | 2459 | `				}` |
|      ! 0 | 2460 | `			}` |
|        - | 2461 | `		}` |
|   318858 | 2462 | `	}` |
|   637315 | 2463 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|   637315 | 2464 | `	break;` |
|        - | 2465 | `/*` |
|        - | 2466 | ` * DECR: P1 * *` |
|        - | 2467 | ` *` |
|        - | 2468 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - | 2469 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - | 2470 | ` * and decrement after that.` |
|        - | 2471 | ` */` |
|      218 | 2472 | `case PH7_OP_DECR:` |
|        - | 2473 | `#ifdef UNTRUST` |
|        - | 2474 | `	if( pTos < pStack ){` |
|        - | 2475 | `		goto Abort;` |
|        - | 2476 | `	}` |
|        - | 2477 | `#endif` |
|        - | 2478 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|        - | 2479 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 2480 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|        - | 2481 | `	 * property currently holding null). */` |
|      445 | 2482 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 2483 | `	/* php refuses to decrement a string OFFSET, whatever byte it holds. */` |
|      435 | 2484 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 2485 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 2486 | `	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —` |
|        - | 2487 | ``	 * `--` on null is php's no-op and its write-back still dispatches set). */`` |
|      426 | 2488 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|      224 | 2489 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 2490 | `		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 2491 | `		if( pTopDec->iKind == VM_HOOK_PEND_RMW && pTopDec->nScratchIdx == pTos->nIdx ){` |
|        - | 2492 | `			SyBlob sErrMsg;` |
|        3 | 2493 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 2494 | `			VmIncDecTypeErrorMsg(pTos,FALSE,&sErrMsg);` |
|        3 | 2495 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 2496 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 2497 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 2498 | `			break;` |
|        - | 2499 | `		}` |
|      ! 0 | 2500 | `	}` |
|      429 | 2501 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 2502 | ``		/* php's `--` operand contract, the mirror of INCR's: an array, object or`` |
|        - | 2503 | `		 * resource is a catchable TypeError, not a silent no-op. */` |
|        - | 2504 | `		SyBlob sDecMsg;` |
|        - | 2505 | `		sxi32 rcDec;` |
|       11 | 2506 | `		SyBlobInit(&sDecMsg,&pVm->sAllocator);` |
|       11 | 2507 | `		VmIncDecTypeErrorMsg(pTos,FALSE,&sDecMsg);` |
|       11 | 2508 | `		PH7_MemObjRelease(pTos);` |
|       11 | 2509 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 2510 | `		pTos->nIdx = SXU32_HIGH;` |
|       16 | 2511 | `		rcDec = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sDecMsg),` |
|        5 | 2512 | `			SyBlobLength(&sDecMsg));` |
|       11 | 2513 | `		SyBlobRelease(&sDecMsg);` |
|       11 | 2514 | `		if( rcDec == SXERR_ABORT ){ goto Abort; }` |
|       11 | 2515 | `		rc = rcDec;` |
|       13 | 2516 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2517 | `	}` |
|        - | 2518 | ``	/* NULL and BOOL stay excluded: PHP leaves `--` on either untouched (no-op) --`` |
|        - | 2519 | `	 * but 8.3 warns about both no-ops, same as the non-numeric-string one below. */` |
|      419 | 2520 | `	if( pTos->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|        - | 2521 | `		/* E_WARNING, not E_DEPRECATED -- php reports these at errno 2. */` |
|       13 | 2522 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 2523 | `			"Decrement on type %s has no effect, this will change in the next major version of PHP",` |
|        8 | 2524 | `			(pTos->iFlags & MEMOBJ_NULL) ? "null" : "bool");` |
|        4 | 2525 | `	}` |
|      419 | 2526 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      411 | 2527 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 2528 | `			ph7_value *pObj;` |
|      411 | 2529 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      411 | 2530 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 2531 | ``					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string`` |
|        - | 2532 | `					 * (php has no string decrement); PHL rejects it. */` |
|        - | 2533 | `					SyBlob sErrMsg;` |
|        3 | 2534 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 2535 | `					SyBlobAppend(&sErrMsg,` |
|        - | 2536 | `						"Decrement on a non-numeric string is not supported",` |
|        - | 2537 | `						sizeof("Decrement on a non-numeric string is not supported")-1);` |
|        3 | 2538 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 2539 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 2540 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 2541 | `					break;` |
|      ! 0 | 2542 | `				}else{` |
|        - | 2543 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - | 2544 | `					 * post-decrement must preserve pTos's original value, which` |
|        - | 2545 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - | 2546 | `					 * Force pTos to own its blob before coercing pObj. */` |
|      408 | 2547 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 | 2548 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 | 2549 | `					}` |
|      408 | 2550 | `					PH7_MemObjToNumeric(pObj);` |
|      408 | 2551 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|       11 | 2552 | `						pObj->rVal--;` |
|        - | 2553 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 2554 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 2555 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 2556 | `						 * integer-valued real. */` |
|       11 | 2557 | `						PH7_MemObjTryInteger(pObj);` |
|        6 | 2558 | `					}else{` |
|        - | 2559 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 2560 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 2561 | `						sxi64 r;` |
|      398 | 2562 | `						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 2563 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        3 | 2564 | `							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;` |
|        3 | 2565 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 2566 | `#else` |
|        - | 2567 | `							pObj->x.iVal = r;` |
|        - | 2568 | `#endif` |
|        2 | 2569 | `						}else{` |
|      396 | 2570 | `							pObj->x.iVal = r;` |
|        - | 2571 | `						}` |
|        - | 2572 | `					}` |
|      408 | 2573 | `					if( pInstr->iP1 ){` |
|        - | 2574 | `						/* Pre-decrement: result is the new value. */` |
|        3 | 2575 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 | 2576 | `					}` |
|        - | 2577 | `					/* Post-decrement: pTos retains the old value. */` |
|        - | 2578 | `				}` |
|      202 | 2579 | `			}` |
|      206 | 2580 | `		}else{` |
|      ! 0 | 2581 | `			if( pInstr->iP1 ){` |
|      ! 0 | 2582 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - | 2583 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 | 2584 | `				}else{` |
|        - | 2585 | `					/* Force a numeric cast */` |
|      ! 0 | 2586 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 2587 | `					/* Pre-decrement */` |
|      ! 0 | 2588 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 2589 | `						pTos->rVal--;` |
|        - | 2590 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 | 2591 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 2592 | `					}else{` |
|        - | 2593 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 2594 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 2595 | `						sxi64 r;` |
|      ! 0 | 2596 | `						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 2597 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 2598 | `							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;` |
|      ! 0 | 2599 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 2600 | `#else` |
|        - | 2601 | `							pTos->x.iVal = r;` |
|        - | 2602 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 2603 | `#endif` |
|      ! 0 | 2604 | `						}else{` |
|      ! 0 | 2605 | `							pTos->x.iVal = r;` |
|      ! 0 | 2606 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 2607 | `						}` |
|        - | 2608 | `					}` |
|        - | 2609 | `				}` |
|      ! 0 | 2610 | `			}` |
|        - | 2611 | `		}` |
|      202 | 2612 | `	}` |
|      416 | 2613 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|      416 | 2614 | `	break;` |
|        - | 2615 | `/*` |
|        - | 2616 | ` * UMINUS: * * *` |
|        - | 2617 | ` *` |
|        - | 2618 | ` * Perform a unary minus operation.` |
|        - | 2619 | ` */` |
|    38629 | 2620 | `case PH7_OP_UMINUS:` |
|        - | 2621 | `#ifdef UNTRUST` |
|        - | 2622 | `	if( pTos < pStack ){` |
|        - | 2623 | `		goto Abort;` |
|        - | 2624 | `	}` |
|        - | 2625 | `#endif` |
|        - | 2626 | ``	/* php's `$x * -1` operand contract: an array, object, resource or`` |
|        - | 2627 | `	 * non-numeric string is a TypeError, not a warned int(-1). */` |
|    77265 | 2628 | `	PH7_UNARY_ARITH_CONTRACT()` |
|        - | 2629 | `	/* Force a numeric (integer,real or both) cast */` |
|    77237 | 2630 | `	PH7_MemObjToNumeric(pTos);` |
|    77237 | 2631 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      176 | 2632 | `		pTos->rVal = -pTos->rVal;` |
|       86 | 2633 | `	}` |
|    77237 | 2634 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    77095 | 2635 | `		if( pTos->x.iVal == SMALLEST_INT64 ){` |
|        - | 2636 | `			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a` |
|        - | 2637 | `			 * REAL representation is already present it is the negated` |
|        - | 2638 | `			 * authoritative value, so just drop the now-stale cached int. The` |
|        - | 2639 | `			 * integer-only build has no float type, so it wraps (two's` |
|        - | 2640 | `			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */` |
|        - | 2641 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        7 | 2642 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 | 2643 | `				pTos->rVal = -(ph7_real)pTos->x.iVal;` |
|        7 | 2644 | `				MemObjSetType(pTos,MEMOBJ_REAL);` |
|        4 | 2645 | `			}else{` |
|      ! 0 | 2646 | `				pTos->iFlags &= ~MEMOBJ_INT;` |
|        - | 2647 | `			}` |
|        - | 2648 | `#else` |
|        - | 2649 | `			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);` |
|        - | 2650 | `#endif` |
|        4 | 2651 | `		}else{` |
|    77089 | 2652 | `			pTos->x.iVal = -pTos->x.iVal;` |
|        - | 2653 | `		}` |
|    38545 | 2654 | `	}` |
|    77237 | 2655 | `	break;` |
|        - | 2656 | `/*` |
|        - | 2657 | ` * UPLUS: * * *` |
|        - | 2658 | ` *` |
|        - | 2659 | ` * Perform a unary plus operation.` |
|        - | 2660 | ` */` |
|       22 | 2661 | `case PH7_OP_UPLUS:` |
|        - | 2662 | `#ifdef UNTRUST` |
|        - | 2663 | `	if( pTos < pStack ){` |
|        - | 2664 | `		goto Abort;` |
|        - | 2665 | `	}` |
|        - | 2666 | `#endif` |
|        - | 2667 | ``	/* Unary plus is php's `$x * 1`, so it answers the same TypeError as unary`` |
|        - | 2668 | ``	 * minus -- naming `int` as the other operand, exactly as php does. */`` |
|       45 | 2669 | `	PH7_UNARY_ARITH_CONTRACT()` |
|        - | 2670 | `	/* Force a numeric (integer,real or both) cast */` |
|       39 | 2671 | `	PH7_MemObjToNumeric(pTos);` |
|       39 | 2672 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 2673 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 | 2674 | `	}` |
|       39 | 2675 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       39 | 2676 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       19 | 2677 | `	}` |
|       39 | 2678 | `	break;` |
|        - | 2679 | `/*` |
|        - | 2680 | ` * OP_LNOT: * * *` |
|        - | 2681 | ` *` |
|        - | 2682 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - | 2683 | ` * with its complement.` |
|        - | 2684 | ` */` |
|    30030 | 2685 | `case PH7_OP_LNOT:` |
|        - | 2686 | `#ifdef UNTRUST` |
|        - | 2687 | `	if( pTos < pStack ){` |
|        - | 2688 | `		goto Abort;` |
|        - | 2689 | `	}` |
|        - | 2690 | `#endif` |
|        - | 2691 | `	/* Force a boolean cast */` |
|    60064 | 2692 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      173 | 2693 | `		PH7_MemObjToBool(pTos);` |
|       84 | 2694 | `	}` |
|    60064 | 2695 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    60064 | 2696 | `	break;` |
|        - | 2697 | `/*` |
|        - | 2698 | ` * OP_BITNOT: * * *` |
|        - | 2699 | ` *` |
|        - | 2700 | ` * Interpret the top of the stack as an value.Replace it` |
|        - | 2701 | ` * with its ones-complement.` |
|        - | 2702 | ` */` |
|       45 | 2703 | `case PH7_OP_BITNOT:` |
|        - | 2704 | `#ifdef UNTRUST` |
|        - | 2705 | `	if( pTos < pStack ){` |
|        - | 2706 | `		goto Abort;` |
|        - | 2707 | `	}` |
|        - | 2708 | `#endif` |
|       92 | 2709 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - | 2710 | ``		/* php's `~` over a STRING is a per-BYTE ones-complement of the same`` |
|        - | 2711 | ``		 * length — a binary string, not an integer operation, so `~"abc"` is`` |
|        - | 2712 | `		 * "\x9e\x9d\x9c" where PHL cast the string to an int and answered` |
|        - | 2713 | `		 * int(-1). The operand's blob can be a READ-ONLY VIEW of the variable's` |
|        - | 2714 | `		 * own buffer (PH7_MemObjLoad), so complement into a fresh blob and swap` |
|        - | 2715 | `		 * it in rather than writing through the view. */` |
|        - | 2716 | `		SyBlob sNotBuf;` |
|       17 | 2717 | `		const unsigned char *zNotIn = (const unsigned char *)SyBlobData(&pTos->sBlob);` |
|       17 | 2718 | `		sxu32 nNotIn = SyBlobLength(&pTos->sBlob), iNot;` |
|       17 | 2719 | `		SyBlobInit(&sNotBuf,&pVm->sAllocator);` |
|       53 | 2720 | `		for( iNot = 0 ; iNot < nNotIn ; ++iNot ){` |
|       37 | 2721 | `			unsigned char cNot = (unsigned char)~zNotIn[iNot];` |
|       37 | 2722 | `			SyBlobAppend(&sNotBuf,(const void *)&cNot,sizeof(char));` |
|       19 | 2723 | `		}` |
|       17 | 2724 | `		PH7_MemObjRelease(pTos);` |
|       17 | 2725 | `		MemObjSetType(pTos,MEMOBJ_STRING);` |
|       17 | 2726 | `		if( SyBlobLength(&sNotBuf) > 0 ){` |
|       15 | 2727 | `			SyBlobAppend(&pTos->sBlob,SyBlobData(&sNotBuf),SyBlobLength(&sNotBuf));` |
|        7 | 2728 | `		}` |
|       17 | 2729 | `		SyBlobRelease(&sNotBuf);` |
|       17 | 2730 | `		break;` |
|        - | 2731 | `	}` |
|       76 | 2732 | `	if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        - | 2733 | `		/* Everything else — null, a bool, an array, an object, a resource — has` |
|        - | 2734 | `		 * no bitwise-not in php at all: a catchable TypeError naming the operand,` |
|        - | 2735 | `		 * where PHL cast it to an int and answered ~0 / ~1. */` |
|        - | 2736 | `		SyBlob sNotMsg;` |
|        - | 2737 | `		sxi32 rcNot;` |
|       25 | 2738 | `		SyBlobInit(&sNotMsg,&pVm->sAllocator);` |
|       25 | 2739 | `		VmBitNotTypeErrorMsg(pTos,&sNotMsg);` |
|       25 | 2740 | `		PH7_MemObjRelease(pTos);` |
|       25 | 2741 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       25 | 2742 | `		pTos->nIdx = SXU32_HIGH;` |
|       37 | 2743 | `		rcNot = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sNotMsg),` |
|       12 | 2744 | `			SyBlobLength(&sNotMsg));` |
|       25 | 2745 | `		SyBlobRelease(&sNotMsg);` |
|       25 | 2746 | `		if( rcNot == SXERR_ABORT ){ goto Abort; }` |
|       25 | 2747 | `		rc = rcNot;` |
|       27 | 2748 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2749 | `	}` |
|        - | 2750 | `	/* Force an integer cast (php deprecates a lossy float here too) */` |
|       52 | 2751 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       52 | 2752 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|       52 | 2753 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 2754 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 2755 | `	}` |
|       52 | 2756 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        - | 2757 | `	/* An INTEGRAL float carries a cached MEMOBJ_INT beside MEMOBJ_REAL, so the` |
|        - | 2758 | `` 	 * cast above is skipped and the value kept rendering as a float: `~2.0` `` |
|        - | 2759 | ``	 * answered 2.0 instead of -3. `~` always yields an int. */`` |
|       52 | 2760 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       52 | 2761 | `	break;` |
|        - | 2762 | `/* OP_MUL * * *` |
|        - | 2763 | ` * OP_MUL_STORE * * *` |
|        - | 2764 | ` *` |
|        - | 2765 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - | 2766 | ` * and push the result back onto the stack.` |
|        - | 2767 | ` */` |
|     1566 | 2768 | `case PH7_OP_MUL:` |
|        - | 2769 | `case PH7_OP_MUL_STORE: {` |
|        - | 2770 | `	VmOpRc rcOp;` |
|        - | 2771 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     3137 | 2772 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     3135 | 2773 | `	sState.pTos = pTos;` |
|     3135 | 2774 | `	sState.pc = pc;` |
|     3135 | 2775 | `	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);` |
|     3135 | 2776 | `	pTos = sState.pTos;` |
|     3135 | 2777 | `	pc = sState.pc;` |
|     3135 | 2778 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2779 | `		goto Abort;` |
|     3135 | 2780 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2781 | `		goto Exception;` |
|        - | 2782 | `	}` |
|     3135 | 2783 | `	break;` |
|        - | 2784 | `					  }` |
|        - | 2785 | `/* OP_POW * * *` |
|        - | 2786 | ` * OP_POW_STORE * * *` |
|        - | 2787 | ` *` |
|        - | 2788 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - | 2789 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - | 2790 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - | 2791 | ` * fits in sxi64; otherwise the result is a double.` |
|        - | 2792 | ` */` |
|       77 | 2793 | `case PH7_OP_POW:` |
|        - | 2794 | `case PH7_OP_POW_STORE: {` |
|        - | 2795 | `	VmOpRc rcOp;` |
|        - | 2796 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      155 | 2797 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      153 | 2798 | `	sState.pTos = pTos;` |
|      153 | 2799 | `	sState.pc = pc;` |
|      153 | 2800 | `	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);` |
|      153 | 2801 | `	pTos = sState.pTos;` |
|      153 | 2802 | `	pc = sState.pc;` |
|      153 | 2803 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2804 | `		goto Abort;` |
|      153 | 2805 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 2806 | `		goto Exception;` |
|        - | 2807 | `	}` |
|      151 | 2808 | `	break;` |
|        - | 2809 | `					  }` |
|        - | 2810 | `/* OP_ADD * * *` |
|        - | 2811 | ` *` |
|        - | 2812 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 2813 | ` * and push the result back onto the stack.` |
|        - | 2814 | ` */` |
|     9008 | 2815 | `case PH7_OP_ADD:{` |
|    18021 | 2816 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2817 | `#ifdef UNTRUST` |
|        - | 2818 | `	if( pNos < pStack ){` |
|        - | 2819 | `		goto Abort;` |
|        - | 2820 | `	}` |
|        - | 2821 | `#endif` |
|        - | 2822 | `	{` |
|        - | 2823 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|        - | 2824 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|        - | 2825 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|        - | 2826 | `		SyBlob sArMsg;` |
|    18021 | 2827 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    18021 | 2828 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 2829 | `			sxi32 rcAr;` |
|        9 | 2830 | `			VmPopOperand(&pTos,1);` |
|        9 | 2831 | `			PH7_MemObjRelease(pTos);` |
|        9 | 2832 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        9 | 2833 | `			pTos->nIdx = SXU32_HIGH;` |
|       13 | 2834 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|        4 | 2835 | `				SyBlobLength(&sArMsg));` |
|        9 | 2836 | `			SyBlobRelease(&sArMsg);` |
|        9 | 2837 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|        9 | 2838 | `			rc = rcAr;` |
|        9 | 2839 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2840 | `		}` |
|    18013 | 2841 | `		SyBlobRelease(&sArMsg);` |
|        - | 2842 | `	}` |
|        - | 2843 | `	/* Perform the addition */` |
|    18013 | 2844 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|    18013 | 2845 | `	VmPopOperand(&pTos,1);` |
|    18013 | 2846 | `	break;` |
|        - | 2847 | `				}` |
|        - | 2848 | `/*` |
|        - | 2849 | ` * OP_ADD_STORE * * *` |
|        - | 2850 | ` *` |
|        - | 2851 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 2852 | ` * and push the result back onto the stack.` |
|        - | 2853 | ` */` |
|     2987 | 2854 | `case PH7_OP_ADD_STORE:{` |
|     5979 | 2855 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 2856 | `	ph7_value *pObj;` |
|        - | 2857 | `	sxu32 nIdx;` |
|        - | 2858 | `#ifdef UNTRUST` |
|        - | 2859 | `	if( pNos < pStack ){` |
|        - | 2860 | `		goto Abort;` |
|        - | 2861 | `	}` |
|        - | 2862 | `#endif` |
|        - | 2863 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     5981 | 2864 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 2865 | `	{` |
|        - | 2866 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 2867 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 2868 | `		SyBlob sArMsg;` |
|     5973 | 2869 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     5973 | 2870 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 2871 | `			sxi32 rcAr;` |
|      ! 0 | 2872 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 2873 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 2874 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 2875 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 2876 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 2877 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 2878 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 2879 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 2880 | `			rc = rcAr;` |
|      ! 0 | 2881 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2882 | `		}` |
|     5973 | 2883 | `		SyBlobRelease(&sArMsg);` |
|        - | 2884 | `	}` |
|        - | 2885 | `	/* Perform the addition */` |
|     5973 | 2886 | `	nIdx = pTos->nIdx;` |
|     5973 | 2887 | `	if( nIdx == pVm->nGlobalIdx ){` |
|        - | 2888 | `		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment` |
|        - | 2889 | `		 * (a compile-time fatal in php; raised here, same message). */` |
|        3 | 2890 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 2891 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 2892 | `		pVm->iExitStatus = 255;` |
|        3 | 2893 | `		pVm->bHaltRequested = 1;` |
|        3 | 2894 | `		goto Abort;` |
|        - | 2895 | `	}` |
|     5971 | 2896 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - | 2897 | `	/* Peform the store operation */` |
|     5971 | 2898 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 2899 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     5971 | 2900 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     5971 | 2901 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     5971 | 2902 | `		PH7_MemObjStore(pTos,pObj);` |
|     2983 | 2903 | `	}` |
|     5971 | 2904 | `	PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|        - | 2905 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     5971 | 2906 | `	PH7_MemObjStore(pTos,pNos);` |
|     5971 | 2907 | `	VmPopOperand(&pTos,1);` |
|     5971 | 2908 | `	break;` |
|        - | 2909 | `				}` |
|        - | 2910 | `/* OP_SUB * * *` |
|        - | 2911 | ` *` |
|        - | 2912 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 2913 | ` * first (what was next on the stack) from the second (the` |
|        - | 2914 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 2915 | ` */` |
|    22364 | 2916 | `case PH7_OP_SUB: {` |
|        - | 2917 | `	VmOpRc rcOp;` |
|    45034 | 2918 | `	sState.pTos = pTos;` |
|    45034 | 2919 | `	sState.pc = pc;` |
|    45034 | 2920 | `	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);` |
|    45034 | 2921 | `	pTos = sState.pTos;` |
|    45034 | 2922 | `	pc = sState.pc;` |
|    45034 | 2923 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2924 | `		goto Abort;` |
|    45034 | 2925 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2926 | `		goto Exception;` |
|        - | 2927 | `	}` |
|    45034 | 2928 | `	break;` |
|        - | 2929 | `					  }` |
|        - | 2930 | `/* OP_SUB_STORE * * *` |
|        - | 2931 | ` *` |
|        - | 2932 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 2933 | ` * first (what was next on the stack) from the second (the` |
|        - | 2934 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 2935 | ` */` |
|        7 | 2936 | `case PH7_OP_SUB_STORE: {` |
|        - | 2937 | `	VmOpRc rcOp;` |
|        - | 2938 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       16 | 2939 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       14 | 2940 | `	sState.pTos = pTos;` |
|       14 | 2941 | `	sState.pc = pc;` |
|       14 | 2942 | `	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);` |
|       14 | 2943 | `	pTos = sState.pTos;` |
|       14 | 2944 | `	pc = sState.pc;` |
|       14 | 2945 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2946 | `		goto Abort;` |
|       14 | 2947 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 2948 | `		goto Exception;` |
|        - | 2949 | `	}` |
|       12 | 2950 | `	break;` |
|        - | 2951 | `					  }` |
|        - | 2952 |  |
|        - | 2953 | `/*` |
|        - | 2954 | ` * OP_MOD * * *` |
|        - | 2955 | ` *` |
|        - | 2956 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2957 | ` * first (what was next on the stack) from the second (the` |
|        - | 2958 | ` * top of the stack) and push the remainder after division` |
|        - | 2959 | ` * onto the stack.` |
|        - | 2960 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 2961 | ` */` |
|      633 | 2962 | `case PH7_OP_MOD: {` |
|        - | 2963 | `	VmOpRc rcOp;` |
|     1271 | 2964 | `	sState.pTos = pTos;` |
|     1271 | 2965 | `	sState.pc = pc;` |
|     1271 | 2966 | `	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);` |
|     1271 | 2967 | `	pTos = sState.pTos;` |
|     1271 | 2968 | `	pc = sState.pc;` |
|     1271 | 2969 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2970 | `		goto Abort;` |
|     1271 | 2971 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 2972 | `		goto Exception;` |
|        - | 2973 | `	}` |
|     1267 | 2974 | `	break;` |
|        - | 2975 | `					  }` |
|        - | 2976 | `/*` |
|        - | 2977 | ` * OP_MOD_STORE * * *` |
|        - | 2978 | ` *` |
|        - | 2979 | ` * Pop the top two elements from the stack, divide the` |
|        - | 2980 | ` * first (what was next on the stack) from the second (the` |
|        - | 2981 | ` * top of the stack) and push the remainder after division` |
|        - | 2982 | ` * onto the stack.` |
|        - | 2983 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 2984 | ` */` |
|        5 | 2985 | `case PH7_OP_MOD_STORE: {` |
|        - | 2986 | `	VmOpRc rcOp;` |
|        - | 2987 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       11 | 2988 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        9 | 2989 | `	sState.pTos = pTos;` |
|        9 | 2990 | `	sState.pc = pc;` |
|        9 | 2991 | `	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);` |
|        9 | 2992 | `	pTos = sState.pTos;` |
|        9 | 2993 | `	pc = sState.pc;` |
|        9 | 2994 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2995 | `		goto Abort;` |
|        9 | 2996 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 2997 | `		goto Exception;` |
|        - | 2998 | `	}` |
|        5 | 2999 | `	break;` |
|        - | 3000 | `					  }` |
|        - | 3001 | `/*` |
|        - | 3002 | ` * OP_DIV * * *` |
|        - | 3003 | ` *` |
|        - | 3004 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3005 | ` * first (what was next on the stack) from the second (the` |
|        - | 3006 | ` * top of the stack) and push the result onto the stack.` |
|        - | 3007 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 3008 | ` */` |
|       68 | 3009 | `case PH7_OP_DIV: {` |
|        - | 3010 | `	VmOpRc rcOp;` |
|      140 | 3011 | `	sState.pTos = pTos;` |
|      140 | 3012 | `	sState.pc = pc;` |
|      140 | 3013 | `	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);` |
|      140 | 3014 | `	pTos = sState.pTos;` |
|      140 | 3015 | `	pc = sState.pc;` |
|      140 | 3016 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3017 | `		goto Abort;` |
|      140 | 3018 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 3019 | `		goto Exception;` |
|        - | 3020 | `	}` |
|      134 | 3021 | `	break;` |
|        - | 3022 | `					  }` |
|        - | 3023 | `/*` |
|        - | 3024 | ` * OP_DIV_STORE * * *` |
|        - | 3025 | ` *` |
|        - | 3026 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3027 | ` * first (what was next on the stack) from the second (the` |
|        - | 3028 | ` * top of the stack) and push the result onto the stack.` |
|        - | 3029 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 3030 | ` */` |
|        6 | 3031 | `case PH7_OP_DIV_STORE:{` |
|       13 | 3032 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3033 | `	ph7_value *pObj;` |
|        - | 3034 | `	ph7_real a,b,r;` |
|        - | 3035 | `#ifdef UNTRUST` |
|        - | 3036 | `	if( pNos < pStack ){` |
|        - | 3037 | `		goto Abort;` |
|        - | 3038 | `	}` |
|        - | 3039 | `#endif` |
|        - | 3040 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       13 | 3041 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 3042 | `	{` |
|        - | 3043 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 3044 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 3045 | `		SyBlob sArMsg;` |
|       11 | 3046 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|       11 | 3047 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"/",&sArMsg) != SXRET_OK ){` |
|        - | 3048 | `			sxi32 rcAr;` |
|      ! 0 | 3049 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 3050 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 3051 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 3052 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 3053 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 3054 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 3055 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 3056 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 3057 | `			rc = rcAr;` |
|      ! 0 | 3058 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3059 | `		}` |
|       11 | 3060 | `		SyBlobRelease(&sArMsg);` |
|        - | 3061 | `	}` |
|        - | 3062 | `	/* Force the operands to be real */` |
|       11 | 3063 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 3064 | `		PH7_MemObjToReal(pTos);` |
|        5 | 3065 | `	}` |
|       11 | 3066 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 3067 | `		PH7_MemObjToReal(pNos);` |
|        5 | 3068 | `	}` |
|        - | 3069 | `	/* Perform the requested operation */` |
|       11 | 3070 | `	a = pTos->rVal;` |
|       11 | 3071 | `	b = pNos->rVal;` |
|       11 | 3072 | `	if( b == 0 ){` |
|        - | 3073 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|        - | 3074 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|        3 | 3075 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|        3 | 3076 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      ! 0 | 3077 | `	}else{` |
|        9 | 3078 | `		r = a/b;` |
|        - | 3079 | `		/* Push the result */` |
|        9 | 3080 | `		pNos->rVal = r;` |
|        9 | 3081 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - | 3082 | `		/* Try to get an integer representation */` |
|        9 | 3083 | `		PH7_MemObjTryInteger(pNos);` |
|        - | 3084 | `	}` |
|        9 | 3085 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 3086 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        9 | 3087 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        9 | 3088 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        9 | 3089 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 | 3090 | `	}` |
|        9 | 3091 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|        9 | 3092 | `	VmPopOperand(&pTos,1);` |
|        9 | 3093 | `	break;` |
|        - | 3094 | `				}` |
|        - | 3095 | `/* OP_BAND * * *` |
|        - | 3096 | ` *` |
|        - | 3097 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3098 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 3099 | ` * two elements.` |
|        - | 3100 | `*/` |
|        - | 3101 | `/* OP_BOR * * *` |
|        - | 3102 | ` *` |
|        - | 3103 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3104 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 3105 | ` * two elements.` |
|        - | 3106 | ` */` |
|        - | 3107 | `/* OP_BXOR * * *` |
|        - | 3108 | ` *` |
|        - | 3109 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3110 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 3111 | ` * two elements.` |
|        - | 3112 | ` */` |
|      727 | 3113 | `case PH7_OP_BAND:` |
|        - | 3114 | `case PH7_OP_BOR:` |
|        - | 3115 | `case PH7_OP_BXOR:{` |
|     1457 | 3116 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3117 | `	sxi64 a,b,r;` |
|        - | 3118 | `	int cBwOp;` |
|        - | 3119 | `#ifdef UNTRUST` |
|        - | 3120 | `	if( pNos < pStack ){` |
|        - | 3121 | `		goto Abort;` |
|        - | 3122 | `	}` |
|        - | 3123 | `#endif` |
|     1457 | 3124 | `	cBwOp = pInstr->iOp == PH7_OP_BOR ? '\|' : (pInstr->iOp == PH7_OP_BXOR ? '^' : '&');` |
|     1457 | 3125 | `	if( (pNos->iFlags & MEMOBJ_STRING) && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 3126 | `		/* TWO strings: php's per-byte string operation, result a string. */` |
|       35 | 3127 | `		PH7_STRING_BITWISE_RESULT(pNos,pNos,pTos,cBwOp)` |
|       35 | 3128 | `		VmPopOperand(&pTos,1);` |
|       35 | 3129 | `		break;` |
|        - | 3130 | `	}` |
|        - | 3131 | `	{` |
|        - | 3132 | `		char zBwOp[2];` |
|     1423 | 3133 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 3134 | `		/* Anything else is php's arithmetic operand contract, named for this` |
|        - | 3135 | ``		 * operator (`array & int`), not a silent integer cast. */`` |
|     1425 | 3136 | `		PH7_BITWISE_ARITH_CONTRACT(pNos,pTos,zBwOp,1)` |
|        - | 3137 | `	}` |
|        - | 3138 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     1391 | 3139 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     1391 | 3140 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     1391 | 3141 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     1391 | 3142 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     1391 | 3143 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|        9 | 3144 | `		PH7_MemObjToInteger(pTos);` |
|        4 | 3145 | `	}` |
|     1391 | 3146 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|       11 | 3147 | `		PH7_MemObjToInteger(pNos);` |
|        5 | 3148 | `	}` |
|        - | 3149 | `	/* Perform the requested operation */` |
|     1391 | 3150 | `	a = pNos->x.iVal;` |
|     1391 | 3151 | `	b = pTos->x.iVal;` |
|     1391 | 3152 | `	switch(pInstr->iOp){` |
|      152 | 3153 | `	case PH7_OP_BOR_STORE:` |
|      307 | 3154 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        8 | 3155 | `	case PH7_OP_BXOR_STORE:` |
|       17 | 3156 | `	case PH7_OP_BXOR: r = a^b; break;` |
|      533 | 3157 | `	case PH7_OP_BAND_STORE:` |
|      533 | 3158 | `	case PH7_OP_BAND:` |
|     1071 | 3159 | `	default:          r = a&b; break;` |
|        - | 3160 | `	}` |
|        - | 3161 | `	/* Push the result */` |
|     1391 | 3162 | `	pNos->x.iVal = r;` |
|     1391 | 3163 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     1391 | 3164 | `	VmPopOperand(&pTos,1);` |
|     1391 | 3165 | `	break;` |
|        - | 3166 | `				 }` |
|        - | 3167 | `/* OP_BAND_STORE * * *` |
|        - | 3168 | ` *` |
|        - | 3169 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3170 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 3171 | ` * two elements.` |
|        - | 3172 | `*/` |
|        - | 3173 | `/* OP_BOR_STORE * * *` |
|        - | 3174 | ` *` |
|        - | 3175 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3176 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 3177 | ` * two elements.` |
|        - | 3178 | ` */` |
|        - | 3179 | `/* OP_BXOR_STORE * * *` |
|        - | 3180 | ` *` |
|        - | 3181 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3182 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 3183 | ` * two elements.` |
|        - | 3184 | ` */` |
|       54 | 3185 | `case PH7_OP_BAND_STORE:` |
|        - | 3186 | `case PH7_OP_BOR_STORE:` |
|        - | 3187 | `case PH7_OP_BXOR_STORE:{` |
|      109 | 3188 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3189 | `	ph7_value *pObj;` |
|        - | 3190 | `	sxi64 a,b,r;` |
|        - | 3191 | `	int cBwOp,bBwStr;` |
|        - | 3192 | `#ifdef UNTRUST` |
|        - | 3193 | `	if( pNos < pStack ){` |
|        - | 3194 | `		goto Abort;` |
|        - | 3195 | `	}` |
|        - | 3196 | `#endif` |
|        - | 3197 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      109 | 3198 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      103 | 3199 | `	cBwOp = pInstr->iOp == PH7_OP_BOR_STORE ? '\|' : (pInstr->iOp == PH7_OP_BXOR_STORE ? '^' : '&');` |
|      103 | 3200 | `	bBwStr = (pNos->iFlags & MEMOBJ_STRING) != 0 && (pTos->iFlags & MEMOBJ_STRING) != 0;` |
|      103 | 3201 | `	if( !bBwStr ){` |
|        - | 3202 | `		char zBwOp[2];` |
|       97 | 3203 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 3204 | ``		/* `$x &= v` answers the same contract as `$x & v` (php's compound`` |
|        - | 3205 | `		 * assignment is the operator plus a store), but through its own error` |
|        - | 3206 | `		 * path, which is POSITIONAL: the lvalue is named first even when the` |
|        - | 3207 | ``		 * right operand is the offender (`$x = 1; $x &= $o` is "int & BsocP",`` |
|        - | 3208 | ``		 * where the plain `1 & $o` is "BsocP & int"). */`` |
|       97 | 3209 | `		PH7_BITWISE_ARITH_CONTRACT(pTos,pNos,zBwOp,0)` |
|        - | 3210 | `		/* Force the operands to be integer (php deprecates a lossy float here) */` |
|       93 | 3211 | `		rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|       93 | 3212 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|       93 | 3213 | `		rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       93 | 3214 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|       93 | 3215 | `		if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 3216 | `			PH7_MemObjToInteger(pTos);` |
|      ! 0 | 3217 | `		}` |
|       93 | 3218 | `		if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        3 | 3219 | `			PH7_MemObjToInteger(pNos);` |
|        1 | 3220 | `		}` |
|       46 | 3221 | `	}` |
|       99 | 3222 | `	if( bBwStr ){` |
|        - | 3223 | `		/* TWO strings: php's per-byte string operation, result a string. The` |
|        - | 3224 | `		 * result lands in pNos, which the store tail below writes into the` |
|        - | 3225 | `		 * lvalue's slot exactly like the integer result. */` |
|        7 | 3226 | `		PH7_STRING_BITWISE_RESULT(pNos,pTos,pNos,cBwOp)` |
|        4 | 3227 | `	}else{` |
|        - | 3228 | `	/* Perform the requested operation */` |
|       93 | 3229 | `	a = pTos->x.iVal;` |
|       93 | 3230 | `	b = pNos->x.iVal;` |
|       93 | 3231 | `	switch(pInstr->iOp){` |
|       38 | 3232 | `	case PH7_OP_BOR_STORE:` |
|       77 | 3233 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 | 3234 | `	case PH7_OP_BXOR_STORE:` |
|        9 | 3235 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        4 | 3236 | `	case PH7_OP_BAND_STORE:` |
|        4 | 3237 | `	case PH7_OP_BAND:` |
|        9 | 3238 | `	default:          r = a&b; break;` |
|        - | 3239 | `	}` |
|        - | 3240 | `	/* Push the result */` |
|       93 | 3241 | `	pNos->x.iVal = r;` |
|       93 | 3242 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        - | 3243 | `	}` |
|       99 | 3244 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 3245 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       99 | 3246 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       99 | 3247 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       99 | 3248 | `		PH7_MemObjStore(pNos,pObj);` |
|       49 | 3249 | `	}` |
|       99 | 3250 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       99 | 3251 | `	VmPopOperand(&pTos,1);` |
|       99 | 3252 | `	break;` |
|        - | 3253 | `				 }` |
|        - | 3254 | `/* OP_SHL * * *` |
|        - | 3255 | ` *` |
|        - | 3256 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3257 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 3258 | ` * left by N bits where N is the top element on the stack.` |
|        - | 3259 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 3260 | ` */` |
|        - | 3261 | `/* OP_SHR * * *` |
|        - | 3262 | ` *` |
|        - | 3263 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3264 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 3265 | ` * right by N bits where N is the top element on the stack.` |
|        - | 3266 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 3267 | ` */` |
|       67 | 3268 | `case PH7_OP_SHL:` |
|        - | 3269 | `case PH7_OP_SHR: {` |
|        - | 3270 | `	VmOpRc rcOp;` |
|      136 | 3271 | `	sState.pTos = pTos;` |
|      136 | 3272 | `	sState.pc = pc;` |
|      136 | 3273 | `	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);` |
|      136 | 3274 | `	pTos = sState.pTos;` |
|      136 | 3275 | `	pc = sState.pc;` |
|      136 | 3276 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3277 | `		goto Abort;` |
|      136 | 3278 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       29 | 3279 | `		goto Exception;` |
|        - | 3280 | `	}` |
|      108 | 3281 | `	break;` |
|        - | 3282 | `					  }` |
|        - | 3283 | `/*  OP_SHL_STORE * * *` |
|        - | 3284 | ` *` |
|        - | 3285 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3286 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 3287 | ` * left by N bits where N is the top element on the stack.` |
|        - | 3288 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 3289 | ` */` |
|        - | 3290 | `/* OP_SHR_STORE * * *` |
|        - | 3291 | ` *` |
|        - | 3292 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3293 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 3294 | ` * right by N bits where N is the top element on the stack.` |
|        - | 3295 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 3296 | ` */` |
|       18 | 3297 | `case PH7_OP_SHL_STORE:` |
|        - | 3298 | `case PH7_OP_SHR_STORE: {` |
|        - | 3299 | `	VmOpRc rcOp;` |
|        - | 3300 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       37 | 3301 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       33 | 3302 | `	sState.pTos = pTos;` |
|       33 | 3303 | `	sState.pc = pc;` |
|       33 | 3304 | `	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);` |
|       33 | 3305 | `	pTos = sState.pTos;` |
|       33 | 3306 | `	pc = sState.pc;` |
|       33 | 3307 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3308 | `		goto Abort;` |
|       33 | 3309 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 3310 | `		goto Exception;` |
|        - | 3311 | `	}` |
|       25 | 3312 | `	break;` |
|        - | 3313 | `					  }` |
|        - | 3314 | `/* CAT:  P1 * *` |
|        - | 3315 | ` *` |
|        - | 3316 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - | 3317 | ` * back.` |
|        - | 3318 | ` */` |
|    96066 | 3319 | `case PH7_OP_CAT:{` |
|        - | 3320 | `	ph7_value *pNos,*pCur;` |
|   192137 | 3321 | `	if( pInstr->iP1 < 1 ){` |
|   161991 | 3322 | `		pNos = &pTos[-1];` |
|    80998 | 3323 | `	}else{` |
|    30151 | 3324 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - | 3325 | `	}` |
|        - | 3326 | `#ifdef UNTRUST` |
|        - | 3327 | `	if( pNos < pStack ){` |
|        - | 3328 | `		goto Abort;` |
|        - | 3329 | `	}` |
|        - | 3330 | `#endif` |
|        - | 3331 | `	/* Force a string cast (user-visible: warns on an array operand, §2).` |
|        - | 3332 | `	 * php coerces the operands left to right, so the leftmost not-stringable` |
|        - | 3333 | `	 * object is the one that throws. */` |
|        - | 3334 | `	{` |
|   192137 | 3335 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|   192137 | 3336 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3337 | `	}` |
|   192135 | 3338 | `	pCur = &pNos[1];` |
|        - | 3339 | `	{` |
|        - | 3340 | `		/* rcSv leaves the loop with the coercion status: the routing macro expands` |
|        - | 3341 | ``		 * to a bare `break` here, which inside the while would fall THROUGH to the`` |
|        - | 3342 | ``		 * `pTos = pNos` below with a throw pending. Route it at case level. */`` |
|   192135 | 3343 | `		sxi32 rcSv = SXRET_OK;` |
|   388643 | 3344 | `		while( pCur <= pTos ){` |
|   196957 | 3345 | `			rcSv = PH7_MemObjToStringUV(pCur);` |
|   196957 | 3346 | `			if( rcSv != SXRET_OK ){` |
|      448 | 3347 | `				break;` |
|        - | 3348 | `			}` |
|        - | 3349 | `			/* Perform the concatenation */` |
|   196513 | 3350 | `			if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   196269 | 3351 | `				if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - | 3352 | `					/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 | 3353 | `					PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 3354 | `					goto Abort;` |
|        - | 3355 | `				}` |
|    98132 | 3356 | `			}` |
|   196513 | 3357 | `			SyBlobRelease(&pCur->sBlob);` |
|   196513 | 3358 | `			pCur++;` |
|        5 | 3359 | `		}` |
|   192955 | 3360 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3361 | `	}` |
|   191691 | 3362 | `	pTos = pNos;` |
|   191691 | 3363 | `	break;` |
|        - | 3364 | `				}` |
|        - | 3365 | `/*  CAT_STORE: * * *` |
|        - | 3366 | ` *` |
|        - | 3367 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - | 3368 | ` * back.` |
|        - | 3369 | ` */` |
|    18738 | 3370 | `case PH7_OP_CAT_STORE:{` |
|    37480 | 3371 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3372 | `	ph7_value *pObj;` |
|        - | 3373 | `	sxu32 nIdx;` |
|        - | 3374 | `#ifdef UNTRUST` |
|        - | 3375 | `	if( pNos < pStack ){` |
|        - | 3376 | `		goto Abort;` |
|        - | 3377 | `	}` |
|        - | 3378 | `#endif` |
|        - | 3379 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|    56213 | 3380 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 3381 | `	/* The right operand must be a string to append it (user-visible, §2) */` |
|        - | 3382 | `	{` |
|    37478 | 3383 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|    37482 | 3384 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3385 | `	}` |
|    37468 | 3386 | `	nIdx = pTos->nIdx;` |
|        - | 3387 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - | 3388 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - | 3389 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - | 3390 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - | 3391 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - | 3392 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - | 3393 | `	 * the source we copy from — references share the slot index, so one check` |
|        - | 3394 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - | 3395 | `	 * must run before any mutation (left to the slow path).` |
|        - | 3396 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - | 3397 | `	 * and remains O(n^2) by design. */` |
|    37463 | 3398 | `	if( nIdx != SXU32_HIGH` |
|    37463 | 3399 | `	 && nIdx != pNos->nIdx` |
|    37459 | 3400 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|    37460 | 3401 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|    21319 | 3402 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|        - | 3403 | `		/* e.g. $x = 5; $x .= "a";  ->  "5a" (user-visible: warns if $x is an array,` |
|        - | 3404 | `		 * throws if it is a not-stringable object — and then the lvalue keeps` |
|        - | 3405 | `		 * holding that object, since the throw abandons the coercion) */` |
|        - | 3406 | `		{` |
|    37454 | 3407 | `			sxi32 rcSv = PH7_MemObjToStringUV(pObj);` |
|    37462 | 3408 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3409 | `		}` |
|    37450 | 3410 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|    37446 | 3411 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 3412 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - | 3413 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 | 3414 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 3415 | `				goto Abort;` |
|        - | 3416 | `			}` |
|    18720 | 3417 | `		}` |
|        - | 3418 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - | 3419 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - | 3420 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - | 3421 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - | 3422 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - | 3423 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - | 3424 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - | 3425 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - | 3426 | `		 * the same slot is appended to again later in the statement` |
|        - | 3427 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - | 3428 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - | 3429 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|    37450 | 3430 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 | 3431 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 | 3432 | `		}` |
|        - | 3433 | `		/* A hooked lvalue's scratch slot was appended in place — dispatch the` |
|        - | 3434 | `		 * set side now (the consume reads the computed value from the slot). */` |
|    37450 | 3435 | `		PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|    37450 | 3436 | `		pNos->nIdx = SXU32_HIGH;` |
|    37450 | 3437 | `		VmPopOperand(&pTos,1);` |
|    37450 | 3438 | `		break;` |
|        - | 3439 | `	}` |
|        - | 3440 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|        - | 3441 | `	/* Force a string cast (user-visible: warns if the lvalue is an array, §2) */` |
|        - | 3442 | `	{` |
|       16 | 3443 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       16 | 3444 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3445 | `	}` |
|        - | 3446 | `	/* Perform the concatenation (Reverse order) */` |
|       16 | 3447 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 | 3448 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 3449 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - | 3450 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 | 3451 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 3452 | `			goto Abort;` |
|        - | 3453 | `		}` |
|        7 | 3454 | `	}` |
|        - | 3455 | `	/* Perform the store operation */` |
|       16 | 3456 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 3457 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 | 3458 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       24 | 3459 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 | 3460 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 | 3461 | `	}` |
|       11 | 3462 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       11 | 3463 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 | 3464 | `	VmPopOperand(&pTos,1);` |
|       11 | 3465 | `	break;` |
|        - | 3466 | `				}` |
|        - | 3467 | `/* OP_AND: * * *` |
|        - | 3468 | ` *` |
|        - | 3469 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - | 3470 | ` * two values and push the resulting boolean value back onto the` |
|        - | 3471 | ` * stack.` |
|        - | 3472 | ` */` |
|        - | 3473 | `/* OP_OR: * * *` |
|        - | 3474 | ` *` |
|        - | 3475 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - | 3476 | ` * two values and push the resulting boolean value back onto the` |
|        - | 3477 | ` * stack.` |
|        - | 3478 | ` */` |
|   151438 | 3479 | `case PH7_OP_LAND:` |
|        - | 3480 | `case PH7_OP_LOR: {` |
|        - | 3481 | `	VmOpRc rcOp;` |
|   303285 | 3482 | `	sState.pTos = pTos;` |
|   303285 | 3483 | `	sState.pc = pc;` |
|   303285 | 3484 | `	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);` |
|   303285 | 3485 | `	pTos = sState.pTos;` |
|   303285 | 3486 | `	pc = sState.pc;` |
|   303285 | 3487 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3488 | `		goto Abort;` |
|   303285 | 3489 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3490 | `		goto Exception;` |
|        - | 3491 | `	}` |
|   303285 | 3492 | `	break;` |
|        - | 3493 | `					  }` |
|        - | 3494 | `/*` |
|        - | 3495 | ` * OP_NULLC: * * *` |
|        - | 3496 | ` * Null coalescing operator '??'.` |
|        - | 3497 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - | 3498 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - | 3499 | ` */` |
|        - | 3500 | `/*` |
|        - | 3501 | ` * OP_NULLC: * P2 *` |
|        - | 3502 | ` * Short-circuit null coalescing '??'.` |
|        - | 3503 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - | 3504 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - | 3505 | ` */` |
|      420 | 3506 | `case PH7_OP_NULLC: {` |
|        - | 3507 | `#ifdef UNTRUST` |
|        - | 3508 | `	if( pTos < pStack ){` |
|        - | 3509 | `		goto Abort;` |
|        - | 3510 | `	}` |
|        - | 3511 | `#endif` |
|      845 | 3512 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 3513 | `		/* Left is not null — keep it and skip the RHS */` |
|      495 | 3514 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|      250 | 3515 | `	}else{` |
|        - | 3516 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|      355 | 3517 | `		VmPopOperand(&pTos, 1);` |
|        - | 3518 | `	}` |
|      845 | 3519 | `	break;` |
|        - | 3520 | `}` |
|        - | 3521 | `/*` |
|        - | 3522 | ` * OP_NULLC_JMP: * P2 *` |
|        - | 3523 | ` * Null coalescing assignment short-circuit.` |
|        - | 3524 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - | 3525 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - | 3526 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - | 3527 | ` */` |
|       71 | 3528 | `case PH7_OP_NULLC_JMP: {` |
|        - | 3529 | `#ifdef UNTRUST` |
|        - | 3530 | `	if( pTos < pStack ){` |
|        - | 3531 | `		goto Abort;` |
|        - | 3532 | `	}` |
|        - | 3533 | `#endif` |
|      145 | 3534 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       38 | 3535 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —` |
|        - | 3536 | `		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is` |
|        - | 3537 | `		 * dropped by the fetch-point sweep (the landing pc is past its` |
|        - | 3538 | `		 * OP_NULLC_STORE window): the skipped assign never dispatches. */` |
|       18 | 3539 | `	}` |
|      145 | 3540 | `	break;` |
|        - | 3541 | `}` |
|        - | 3542 | `/*` |
|        - | 3543 | ` * OP_NULLC_STORE: * * *` |
|        - | 3544 | ` * Null coalescing assignment store.` |
|        - | 3545 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - | 3546 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - | 3547 | ` * expression result.` |
|        - | 3548 | ` */` |
|        - | 3549 | `/*` |
|        - | 3550 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - | 3551 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - | 3552 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - | 3553 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - | 3554 | ` * non-null, fall through without modifying the stack so the following` |
|        - | 3555 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - | 3556 | ` */` |
|       56 | 3557 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - | 3558 | `#ifdef UNTRUST` |
|        - | 3559 | `	if( pTos < pStack ){` |
|        - | 3560 | `		goto Abort;` |
|        - | 3561 | `	}` |
|        - | 3562 | `#endif` |
|      115 | 3563 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - | 3564 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - | 3565 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       44 | 3566 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       21 | 3567 | `	}` |
|      115 | 3568 | `	break;` |
|        - | 3569 | `}` |
|       50 | 3570 | `case PH7_OP_NULLC_STORE: {` |
|        - | 3571 | `	VmOpRc rcOp;` |
|      103 | 3572 | `	sState.pTos = pTos;` |
|      103 | 3573 | `	sState.pc = pc;` |
|      103 | 3574 | `	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);` |
|      103 | 3575 | `	pTos = sState.pTos;` |
|      103 | 3576 | `	pc = sState.pc;` |
|      103 | 3577 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3578 | `		goto Abort;` |
|      103 | 3579 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 3580 | `		goto Exception;` |
|        - | 3581 | `	}` |
|       97 | 3582 | `	break;` |
|        - | 3583 | `					  }` |
|        - | 3584 | `/*` |
|        - | 3585 | ` * OP_SPREAD: * * *` |
|        - | 3586 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - | 3587 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - | 3588 | ` * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this` |
|        - | 3589 | ` * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —` |
|        - | 3590 | ` * the CALL may not be the next instruction, and may be an inner call whose own` |
|        - | 3591 | ` * spreads must stay scoped to it.` |
|        - | 3592 | ` * The expansion tail is shared between the plain-array and the materialized` |
|        - | 3593 | ` * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.` |
|        - | 3594 | ` */` |
|      152 | 3595 | `case PH7_OP_SPREAD: {` |
|        - | 3596 | `#ifdef UNTRUST` |
|        - | 3597 | `	if( pTos < pStack ){` |
|        - | 3598 | `		goto Abort;` |
|        - | 3599 | `	}` |
|        - | 3600 | `#endif` |
|        - | 3601 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - | 3602 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - | 3603 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - | 3604 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - | 3605 | `	 * be freed immediately. */` |
|      307 | 3606 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 | 3607 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - | 3608 | `		sxi32 rcW;` |
|        3 | 3609 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 | 3610 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 | 3611 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 | 3612 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 3613 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 3614 | `			goto Exception;` |
|        - | 3615 | `		}` |
|        - | 3616 | `		/* Grow the operand stack if this expansion would overflow it (no longer a` |
|        - | 3617 | `		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a` |
|        - | 3618 | `		 * single argument — the historical bounded-fallback, now only under OOM. */` |
|        4 | 3619 | `		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,` |
|        1 | 3620 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 3621 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 3622 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 3623 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 3624 | `				pTmpMap->nEntry);` |
|      ! 0 | 3625 | `			break;` |
|        - | 3626 | `		}` |
|        3 | 3627 | `		VmSpreadExpandMap(pVm, &pTos, pTmpMap);` |
|        3 | 3628 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 | 3629 | `		break;` |
|        - | 3630 | `	}` |
|      305 | 3631 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      305 | 3632 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      456 | 3633 | `		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,` |
|      151 | 3634 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 3635 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 3636 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 3637 | `				pMap->nEntry);` |
|      ! 0 | 3638 | `			break;` |
|        - | 3639 | `		}` |
|      305 | 3640 | `		VmSpreadExpandMap(pVm, &pTos, pMap);` |
|      151 | 3641 | `	}` |
|        - | 3642 | `	/* else: not an array — leave as-is (single arg) */` |
|      305 | 3643 | `	break;` |
|        - | 3644 | `}` |
|        - | 3645 | `/*` |
|        - | 3646 | ` * OP_FLAG_SPREAD: * * *` |
|        - | 3647 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - | 3648 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - | 3649 | ` */` |
|      340 | 3650 | `case PH7_OP_FLAG_SPREAD: {` |
|        - | 3651 | `#ifdef UNTRUST` |
|        - | 3652 | `	if( pTos < pStack ){` |
|        - | 3653 | `		goto Abort;` |
|        - | 3654 | `	}` |
|        - | 3655 | `#endif` |
|      683 | 3656 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|      683 | 3657 | `	break;` |
|        - | 3658 | `}` |
|        - | 3659 | `/* OP_LXOR: * * *` |
|        - | 3660 | ` *` |
|        - | 3661 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - | 3662 | ` * two values and push the resulting boolean value back onto the` |
|        - | 3663 | ` * stack.` |
|        - | 3664 | ` * According to the PHP language reference manual:` |
|        - | 3665 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - | 3666 | ` *  TRUE,but not both.` |
|        - | 3667 | ` */` |
|        6 | 3668 | `case PH7_OP_LXOR:{` |
|       13 | 3669 | `	ph7_value *pNos = &pTos[-1];` |
|       13 | 3670 | `	sxi32 v = 0;` |
|        - | 3671 | `#ifdef UNTRUST` |
|        - | 3672 | `	if( pNos < pStack ){` |
|        - | 3673 | `		goto Abort;` |
|        - | 3674 | `	}` |
|        - | 3675 | `#endif` |
|        - | 3676 | `	/* Force a boolean cast */` |
|       13 | 3677 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 3678 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 3679 | `	}` |
|       13 | 3680 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 3681 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 | 3682 | `	}` |
|       13 | 3683 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 | 3684 | `		v = 1;` |
|        3 | 3685 | `	}` |
|       13 | 3686 | `	VmPopOperand(&pTos,1);` |
|       13 | 3687 | `	pTos->x.iVal = v;` |
|       13 | 3688 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       13 | 3689 | `	break;` |
|        - | 3690 | `				 }` |
|        - | 3691 | `/* OP_EQ P1 P2 P3` |
|        - | 3692 | ` *` |
|        - | 3693 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - | 3694 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - | 3695 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3696 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3697 | ` */` |
|        - | 3698 | `/* OP_NEQ P1 P2 P3` |
|        - | 3699 | ` *` |
|        - | 3700 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - | 3701 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 3702 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3703 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3704 | ` */` |
|     6023 | 3705 | `case PH7_OP_EQ:` |
|        - | 3706 | `case PH7_OP_NEQ: {` |
|        - | 3707 | `	VmOpRc rcOp;` |
|    12051 | 3708 | `	sState.pTos = pTos;` |
|    12051 | 3709 | `	sState.pc = pc;` |
|    12051 | 3710 | `	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);` |
|    12051 | 3711 | `	pTos = sState.pTos;` |
|    12051 | 3712 | `	pc = sState.pc;` |
|    12051 | 3713 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3714 | `		goto Abort;` |
|    12051 | 3715 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3716 | `		goto Exception;` |
|        - | 3717 | `	}` |
|    12051 | 3718 | `	break;` |
|        - | 3719 | `					  }` |
|        - | 3720 | `/* OP_TEQ P1 P2 *` |
|        - | 3721 | ` *` |
|        - | 3722 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - | 3723 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 3724 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3725 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3726 | ` */` |
|   291613 | 3727 | `case PH7_OP_TEQ: {` |
|        - | 3728 | `	VmOpRc rcOp;` |
|   583635 | 3729 | `	sState.pTos = pTos;` |
|   583635 | 3730 | `	sState.pc = pc;` |
|   583635 | 3731 | `	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);` |
|   583635 | 3732 | `	pTos = sState.pTos;` |
|   583635 | 3733 | `	pc = sState.pc;` |
|   583635 | 3734 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3735 | `		goto Abort;` |
|   583635 | 3736 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3737 | `		goto Exception;` |
|        - | 3738 | `	}` |
|   583635 | 3739 | `	break;` |
|        - | 3740 | `					  }` |
|        - | 3741 | `/* OP_TNE P1 P2 *` |
|        - | 3742 | ` *` |
|        - | 3743 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - | 3744 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - | 3745 | ` * instruction.` |
|        - | 3746 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3747 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3748 | ` *` |
|        - | 3749 | ` */` |
|   252942 | 3750 | `case PH7_OP_TNE: {` |
|        - | 3751 | `	VmOpRc rcOp;` |
|   506293 | 3752 | `	sState.pTos = pTos;` |
|   506293 | 3753 | `	sState.pc = pc;` |
|   506293 | 3754 | `	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);` |
|   506293 | 3755 | `	pTos = sState.pTos;` |
|   506293 | 3756 | `	pc = sState.pc;` |
|   506293 | 3757 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3758 | `		goto Abort;` |
|   506293 | 3759 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3760 | `		goto Exception;` |
|        - | 3761 | `	}` |
|   506293 | 3762 | `	break;` |
|        - | 3763 | `					  }` |
|        - | 3764 | `/* OP_LT P1 P2 P3` |
|        - | 3765 | ` *` |
|        - | 3766 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 3767 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 3768 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 3769 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3770 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3771 | ` *` |
|        - | 3772 | ` */` |
|        - | 3773 | `/* OP_LE P1 P2 P3` |
|        - | 3774 | ` *` |
|        - | 3775 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 3776 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 3777 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 3778 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3779 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3780 | ` *` |
|        - | 3781 | ` */` |
|   268517 | 3782 | `case PH7_OP_LT:` |
|        - | 3783 | `case PH7_OP_LE: {` |
|        - | 3784 | `	VmOpRc rcOp;` |
|   537839 | 3785 | `	sState.pTos = pTos;` |
|   537839 | 3786 | `	sState.pc = pc;` |
|   537839 | 3787 | `	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);` |
|   537839 | 3788 | `	pTos = sState.pTos;` |
|   537839 | 3789 | `	pc = sState.pc;` |
|   537839 | 3790 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3791 | `		goto Abort;` |
|   537839 | 3792 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3793 | `		goto Exception;` |
|        - | 3794 | `	}` |
|   537839 | 3795 | `	break;` |
|        - | 3796 | `					  }` |
|        - | 3797 | `/* OP_GT P1 P2 P3` |
|        - | 3798 | ` *` |
|        - | 3799 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 3800 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 3801 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 3802 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3803 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3804 | ` *` |
|        - | 3805 | ` */` |
|        - | 3806 | `/* OP_GE P1 P2 P3` |
|        - | 3807 | ` *` |
|        - | 3808 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 3809 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 3810 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 3811 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 3812 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 3813 | ` *` |
|        - | 3814 | ` */` |
|   124122 | 3815 | `case PH7_OP_GT:` |
|        - | 3816 | `case PH7_OP_GE: {` |
|        - | 3817 | `	VmOpRc rcOp;` |
|   248653 | 3818 | `	sState.pTos = pTos;` |
|   248653 | 3819 | `	sState.pc = pc;` |
|   248653 | 3820 | `	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);` |
|   248653 | 3821 | `	pTos = sState.pTos;` |
|   248653 | 3822 | `	pc = sState.pc;` |
|   248653 | 3823 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3824 | `		goto Abort;` |
|   248653 | 3825 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3826 | `		goto Exception;` |
|        - | 3827 | `	}` |
|   248653 | 3828 | `	break;` |
|        - | 3829 | `					  }` |
|        - | 3830 | `/* OP_SPACESHIP * * *` |
|        - | 3831 | ` *` |
|        - | 3832 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - | 3833 | ` *   -1 if left < right` |
|        - | 3834 | ` *    0 if left == right` |
|        - | 3835 | ` *    1 if left > right` |
|        - | 3836 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - | 3837 | ` */` |
|      268 | 3838 | `case PH7_OP_SPACESHIP: {` |
|        - | 3839 | `	VmOpRc rcOp;` |
|      539 | 3840 | `	sState.pTos = pTos;` |
|      539 | 3841 | `	sState.pc = pc;` |
|      539 | 3842 | `	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);` |
|      539 | 3843 | `	pTos = sState.pTos;` |
|      539 | 3844 | `	pc = sState.pc;` |
|      539 | 3845 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3846 | `		goto Abort;` |
|      539 | 3847 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3848 | `		goto Exception;` |
|        - | 3849 | `	}` |
|      539 | 3850 | `	break;` |
|        - | 3851 | `					  }` |
|        - | 3852 | `/*` |
|        - | 3853 | ` * OP_LOAD_REF * * *` |
|        - | 3854 | ` * Push the index of a referenced object on the stack.` |
|        - | 3855 | ` */` |
|       61 | 3856 | `case PH7_OP_LOAD_REF: {` |
|        - | 3857 | `	sxu32 nIdx;` |
|        - | 3858 | `#ifdef UNTRUST` |
|        - | 3859 | `	if( pTos < pStack ){` |
|        - | 3860 | `		goto Abort;` |
|        - | 3861 | `	}` |
|        - | 3862 | `#endif` |
|      124 | 3863 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|        - | 3864 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|        - | 3865 | `		 * out of a string still carries the BASE VARIABLE's slot index, so taking a` |
|        - | 3866 | `` 		 * reference to it aliased the WHOLE STRING — `$x = [&$s[1]]; $x[0] = "Z";` `` |
|        - | 3867 | ``		 * left $s === "Z". Same Error the `=&` and by-ref-argument paths raise. */`` |
|        3 | 3868 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|        - | 3869 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|        3 | 3870 | `		PH7_MemObjRelease(pTos);` |
|        3 | 3871 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 3872 | `		pTos->nIdx = SXU32_HIGH;` |
|        3 | 3873 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|        3 | 3874 | `		break;` |
|        - | 3875 | `	}` |
|        - | 3876 | `	/* Extract memory object index */` |
|      121 | 3877 | `	nIdx = pTos->nIdx;` |
|      121 | 3878 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - | 3879 | `		/* Nullify the object */` |
|      121 | 3880 | `		PH7_MemObjRelease(pTos);` |
|        - | 3881 | `		/* Mark as constant and store the index on the top of the stack */` |
|      121 | 3882 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      121 | 3883 | `		pTos->nIdx = SXU32_HIGH;` |
|      121 | 3884 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       60 | 3885 | `	}` |
|      121 | 3886 | `	break;` |
|        - | 3887 | `					  }` |
|        - | 3888 | `/*` |
|        - | 3889 | ` * OP_STORE_REF * * P3` |
|        - | 3890 | ` * Perform an assignment operation by reference.` |
|        - | 3891 | ` */` |
|       33 | 3892 | `case PH7_OP_STORE_REF: {` |
|        - | 3893 | `	VmOpRc rcOp;` |
|       71 | 3894 | `	sState.pTos = pTos;` |
|       71 | 3895 | `	sState.pc = pc;` |
|       71 | 3896 | `	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);` |
|       71 | 3897 | `	pTos = sState.pTos;` |
|       71 | 3898 | `	pc = sState.pc;` |
|       71 | 3899 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3900 | `		goto Abort;` |
|       68 | 3901 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 3902 | `		goto Exception;` |
|        - | 3903 | `	}` |
|       60 | 3904 | `	break;` |
|        - | 3905 | `					  }` |
|        - | 3906 | `/*` |
|        - | 3907 | ` * OP_UPLINK P1 * *` |
|        - | 3908 | ` * Link a variable to the top active VM frame.` |
|        - | 3909 | ` * This is used to implement the 'global' PHP construct.` |
|        - | 3910 | ` */` |
|       32 | 3911 | `case PH7_OP_UPLINK: {` |
|       69 | 3912 | `	if( pVm->pFrame->pParent ){` |
|       69 | 3913 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - | 3914 | `		SyString sName;` |
|        - | 3915 | `		/* rcSv carries the coercion status OUT of the loop: the routing macro is a` |
|        - | 3916 | ``		 * bare `break` here, which would only leave the while and then pop the`` |
|        - | 3917 | `		 * operands with a throw pending. */` |
|       69 | 3918 | `		sxi32 rcSv = SXRET_OK;` |
|        - | 3919 | `		/* Perform the link */` |
|      139 | 3920 | `		while( pLink <= pTos ){` |
|        - | 3921 | `			/* Force a string cast — global $$arr link name (user-visible, §2) */` |
|       81 | 3922 | `			rcSv = PH7_MemObjToStringUV(pLink);` |
|       81 | 3923 | `			if( rcSv != SXRET_OK ){` |
|        7 | 3924 | `				break;` |
|        - | 3925 | `			}` |
|       75 | 3926 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       75 | 3927 | `			if( sName.nByte > 0 ){` |
|       75 | 3928 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 | 3929 | `			}` |
|       75 | 3930 | `			pLink++;` |
|        5 | 3931 | `		}` |
|       69 | 3932 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|       29 | 3933 | `	}` |
|       63 | 3934 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       63 | 3935 | `	break;` |
|        - | 3936 | `					}` |
|        - | 3937 | `/*` |
|        - | 3938 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - | 3939 | ` * Push an exception in the corresponding container so that` |
|        - | 3940 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - | 3941 | ` */` |
|   725224 | 3942 | `case PH7_OP_LOAD_EXCEPTION: {` |
|        - | 3943 | `	VmOpRc rcOp;` |
|  1450453 | 3944 | `	sState.pTos = pTos;` |
|  1450453 | 3945 | `	sState.pc = pc;` |
|  1450453 | 3946 | `	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);` |
|  1450453 | 3947 | `	pTos = sState.pTos;` |
|  1450453 | 3948 | `	pc = sState.pc;` |
|  1450453 | 3949 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3950 | `		goto Abort;` |
|  1450453 | 3951 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3952 | `		goto Exception;` |
|        - | 3953 | `	}` |
|  1450453 | 3954 | `	break;` |
|        - | 3955 | `					  }` |
|        - | 3956 | `/*` |
|        - | 3957 | ` * OP_POP_EXCEPTION * * P3` |
|        - | 3958 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - | 3959 | ` */` |
|   675031 | 3960 | `case PH7_OP_POP_EXCEPTION: {` |
|  1350067 | 3961 | `	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;` |
|        - | 3962 | `	VmFrame *pBodyFrame;` |
|        - | 3963 | `	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on` |
|        - | 3964 | `	 * top (matched by compiled origin). pException == NULL means this try's` |
|        - | 3965 | `	 * activation was already consumed (an in-place catch handled a throw and` |
|        - | 3966 | `	 * ran the finally itself); compiled fields keep coming from p3. */` |
|  1350067 | 3967 | `	ph7_exception *pException = 0;` |
|  1350067 | 3968 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|     1507 | 3969 | `		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|     1507 | 3970 | `		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];` |
|        - | 3971 | `		/* Same compiled origin is NOT enough: under recursion, when THIS` |
|        - | 3972 | `		 * level's activation was already consumed by an in-place catch (the` |
|        - | 3973 | `		 * resume lands right here), the top can be an OUTER level's activation` |
|        - | 3974 | `		 * of the same lexical try — popping it would run that level's finally` |
|        - | 3975 | `		 * early and orphan its handler (probed: recursive try/catch/finally` |
|        - | 3976 | `		 * lost the outer catch entirely). The activation must also belong to` |
|        - | 3977 | `		 * the CURRENT body frame. */` |
|     1502 | 3978 | `		if( VmExcMatches(pTop,pCompiledExc)` |
|     1495 | 3979 | `		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){` |
|     1477 | 3980 | `			pException = pTop;` |
|     1477 | 3981 | `			(void)SySetPop(&pVm->aException);` |
|      736 | 3982 | `		}` |
|      751 | 3983 | `	}` |
|  1350067 | 3984 | `	if( pCompiledExc->iInlined ){` |
|        - | 3985 | `		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.` |
|        - | 3986 | `		 * Pop this try's handler off aException so a throw in the finally propagates to` |
|        - | 3987 | `		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the` |
|        - | 3988 | `		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the` |
|        - | 3989 | `		 * compiler-emitted JMP falls into the finally. Without a finally, this is the` |
|        - | 3990 | `		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */` |
|      167 | 3991 | `		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */` |
|      167 | 3992 | `		if( pCompiledExc->iHasFinally ){` |
|        - | 3993 | `			VmFinallyAction sAct;` |
|       14 | 3994 | `			SyZero(&sAct,sizeof(sAct));` |
|       14 | 3995 | `			sAct.eKind = PH7_FA_FALLTHROUGH;` |
|       14 | 3996 | `			sAct.iNextPc = pCompiledExc->iEndCatchPc;` |
|       14 | 3997 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        - | 3998 | `			/* keep the transparent frame for OP_END_FINALLY */` |
|      161 | 3999 | `		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       20 | 4000 | `			VmLeaveFrame(&(*pVm));` |
|        8 | 4001 | `		}` |
|      167 | 4002 | `		break;` |
|        - | 4003 | `	}` |
|        - | 4004 | `	/* Leave the exception frame. It is normally on top here (a try that fell through` |
|        - | 4005 | `	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a` |
|        - | 4006 | `	 * RESUMED generator/fiber body whose yield was inside this try, that exception` |
|        - | 4007 | `	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),` |
|        - | 4008 | `	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the` |
|        - | 4009 | `	 * coroutine (and defeat the bHasRet materialization just below, which must see the` |
|        - | 4010 | `	 * body). Only leave a genuine exception frame. */` |
|  1349905 | 4011 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|  1148035 | 4012 | `		VmLeaveFrame(&(*pVm));` |
|   574015 | 4013 | `	}` |
|        - | 4014 | `	/* Execute the finally block if present and not already executed by the` |
|        - | 4015 | `	 * catch path. No live activation (pException == NULL) means an in-place` |
|        - | 4016 | `	 * catch consumed it — and that path runs the finally itself — so skip. */` |
|  1349905 | 4017 | `	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){` |
|        - | 4018 | `		sxi32 rcFinally;` |
|       57 | 4019 | `		VmExcRelease(&(*pVm),pException);` |
|       57 | 4020 | `		pException = 0;` |
|       57 | 4021 | `		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);` |
|       57 | 4022 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 | 4023 | `			goto Abort;` |
|        - | 4024 | `		}` |
|       57 | 4025 | `		if( rcFinally == PH7_EXCEPTION ){` |
|        - | 4026 | `			/* The finally threw past itself. If an enclosing try IN THIS function` |
|        - | 4027 | `			 * caught the new exception in place, resume at its landing pad;` |
|        - | 4028 | `			 * otherwise it was caught at an outer frame, so unwind this function. */` |
|        - | 4029 | `			sxi32 iResumePc;` |
|        5 | 4030 | `			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 4031 | `				pc = iResumePc;` |
|        3 | 4032 | `				break;` |
|        - | 4033 | `			}` |
|        3 | 4034 | `			goto Exception;` |
|        - | 4035 | `		}` |
|       24 | 4036 | `	}` |
|  1349901 | 4037 | `	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */` |
|  1349901 | 4038 | `	pBodyFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  1349901 | 4039 | `	if( pBodyFrame->bHasRet ){` |
|        - | 4040 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - | 4041 | `		 * function. The return targets the body frame this try belongs to. Drain` |
|        - | 4042 | `		 * outer finally blocks first, then — only in the real function body` |
|        - | 4043 | `		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline` |
|        - | 4044 | `		 * try within a catch/finally) propagate outward so the owning body returns. */` |
|    20197 | 4045 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    20197 | 4046 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4047 | `			goto Abort;` |
|        - | 4048 | `		}` |
|    20197 | 4049 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 4050 | `			goto Exception;` |
|        - | 4051 | `		}` |
|    20197 | 4052 | `		if( !sState.bReturnPropagates ){` |
|    20191 | 4053 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|    10093 | 4054 | `		}` |
|    20197 | 4055 | `		goto Done;` |
|        - | 4056 | `	}` |
|  1329709 | 4057 | `	if( pBodyFrame->nCatchJmpPc > 0 ){` |
|        - | 4058 | ``		/* A `break`/`continue` inside the catch (OP_CATCH_JMP) parked its loop-exit`` |
|        - | 4059 | `		 * target on this body frame, and this is a landing pad on its way out. */` |
|       88 | 4060 | `		if( pBodyFrame->nCatchJmpLevels > 1 ){` |
|        - | 4061 | `			/* Still one or more detached bodies out from the target's array — this try` |
|        - | 4062 | `			 * was declared INSIDE another catch/finally mini-program. End this one and` |
|        - | 4063 | `			 * let the park travel outward; the next landing pad decrements again. */` |
|        6 | 4064 | `			pBodyFrame->nCatchJmpLevels--;` |
|        6 | 4065 | `			goto Done;` |
|        - | 4066 | `		}` |
|       84 | 4067 | `		if( pBodyFrame->nCatchJmpCross > 0 ){` |
|        - | 4068 | `			/* Enclosing trys between the catch and the loop: the jump lands past their` |
|        - | 4069 | `			 * OP_POP_EXCEPTION, so run their finally (and leave their frames) here. */` |
|        8 | 4070 | `			sxu32 nCross = pBodyFrame->nCatchJmpCross;` |
|        8 | 4071 | `			pBodyFrame->nCatchJmpCross = 0;` |
|        8 | 4072 | `			rc = VmDrainCrossedTrys(&(*pVm),nCross,sState.nExceptionBase);` |
|        8 | 4073 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4074 | `				goto Abort;` |
|        - | 4075 | `			}` |
|        8 | 4076 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 4077 | `				/* A drained finally threw past itself — it supersedes the loop exit. */` |
|      ! 0 | 4078 | `				pBodyFrame->nCatchJmpPc = 0;` |
|      ! 0 | 4079 | `				goto Exception;` |
|        - | 4080 | `			}` |
|        3 | 4081 | `		}` |
|       84 | 4082 | `		pc = (sxi32)pBodyFrame->nCatchJmpPc - 1;` |
|       84 | 4083 | `		pBodyFrame->nCatchJmpPc = 0;` |
|       84 | 4084 | `		break;` |
|        - | 4085 | `	}` |
|  1329625 | 4086 | `	break;` |
|        - | 4087 | `							}` |
|        - | 4088 | `/*` |
|        - | 4089 | ` * OP_CATCH_JMP P1(levels,cross) P2(target pc) *` |
|        - | 4090 | ` * Jump to iP2, leaving the try/catch structures iP1 describes (PH7_CATCH_JMP_P1).` |
|        - | 4091 | ` *` |
|        - | 4092 | ` * CROSS enclosing trys are being left without reaching their OP_POP_EXCEPTION, so this` |
|        - | 4093 | ` * runs their finallys itself. LEVELS says whether the target is even addressable from` |
|        - | 4094 | `` * here: 0 means it is in this same array (a `goto` out of a try body) and the jump is`` |
|        - | 4095 | ` * taken on the spot; above 0 the jump starts inside a DETACHED catch/finally` |
|        - | 4096 | ` * mini-program whose array is not the target's, so the target is PARKED on the owning` |
|        - | 4097 | `` * body's frame and this mini-program ends — mirroring how an explicit `return` in a`` |
|        - | 4098 | ` * catch parks on sRet at OP_DONE above. VmThrowException then runs this try's finally` |
|        - | 4099 | ` * and returns; the resume lands on the try's OP_POP_EXCEPTION, which decrements LEVELS` |
|        - | 4100 | ` * and either re-parks (another mini-program out) or takes the jump.` |
|        - | 4101 | ` */` |
|       47 | 4102 | `case PH7_OP_CATCH_JMP: {` |
|        - | 4103 | `	VmFrame *pTgt;` |
|       98 | 4104 | `	if( PH7_CATCH_JMP_LEVELS(pInstr->iP1) == 0 ){` |
|        - | 4105 | ``		/* No detached body to leave — a `goto` whose target is in THIS array, but which`` |
|        - | 4106 | `		 * jumps out of one or more enclosing trys. Nothing to park: run their finallys` |
|        - | 4107 | `		 * (a bare jump would leave them to fire at teardown) and go. */` |
|       15 | 4108 | `		rc = VmDrainCrossedTrys(&(*pVm),PH7_CATCH_JMP_CROSS(pInstr->iP1),sState.nExceptionBase);` |
|       15 | 4109 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4110 | `			goto Abort;` |
|        - | 4111 | `		}` |
|       15 | 4112 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 4113 | `			/* A drained finally threw past itself — it supersedes the jump. */` |
|      ! 0 | 4114 | `			goto Exception;` |
|        - | 4115 | `		}` |
|       15 | 4116 | `		pc = (sxi32)pInstr->iP2 - 1;` |
|       15 | 4117 | `		break;` |
|        - | 4118 | `	}` |
|       86 | 4119 | `	pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|       86 | 4120 | `	pTgt->nCatchJmpPc = (sxu32)pInstr->iP2;` |
|       86 | 4121 | `	pTgt->nCatchJmpLevels = PH7_CATCH_JMP_LEVELS(pInstr->iP1);` |
|       86 | 4122 | `	pTgt->nCatchJmpCross = PH7_CATCH_JMP_CROSS(pInstr->iP1);` |
|        - | 4123 | `	/* A try opened INSIDE this catch body that the jump crosses had its finally run by` |
|        - | 4124 | `	 * the compiler-emitted OP_POP_EXCEPTION; drain anything still pending here. */` |
|       86 | 4125 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|       86 | 4126 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4127 | `		goto Abort;` |
|        - | 4128 | `	}` |
|       86 | 4129 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 4130 | `		/* A drained finally threw past itself — it discards this jump. */` |
|      ! 0 | 4131 | `		pTgt->nCatchJmpPc = 0;` |
|      ! 0 | 4132 | `		goto Exception;` |
|        - | 4133 | `	}` |
|       86 | 4134 | `	goto Done;` |
|        - | 4135 | `					   }` |
|        - | 4136 | `/*` |
|        - | 4137 | ` * OP_CATCH iP1(catch-index) * P3(ph7_exception)` |
|        - | 4138 | ` * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on` |
|        - | 4139 | ` * pException->pInflight by VmThrowInline) into the catch variable, resolved in the` |
|        - | 4140 | ` * enclosing body's scope (PHP: a catch shares the surrounding variable scope).` |
|        - | 4141 | ` */` |
|       38 | 4142 | `case PH7_OP_CATCH: {` |
|        - | 4143 | `	VmOpRc rcOp;` |
|       81 | 4144 | `	sState.pTos = pTos;` |
|       81 | 4145 | `	sState.pc = pc;` |
|       81 | 4146 | `	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);` |
|       81 | 4147 | `	pTos = sState.pTos;` |
|       81 | 4148 | `	pc = sState.pc;` |
|       81 | 4149 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4150 | `		goto Abort;` |
|       81 | 4151 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4152 | `		goto Exception;` |
|        - | 4153 | `	}` |
|       81 | 4154 | `	break;` |
|        - | 4155 | `					  }` |
|        - | 4156 | `/*` |
|        - | 4157 | ` * OP_END_FINALLY * P3(ph7_exception)` |
|        - | 4158 | ` * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch` |
|        - | 4159 | ` * the pending action queued when the finally was entered (fall-through / re-throw /` |
|        - | 4160 | ` * return / break-continue). A return/break threads out through each enclosing finally` |
|        - | 4161 | ` * via pException->iNextFinallyPc.` |
|        - | 4162 | ` */` |
|       24 | 4163 | `case PH7_OP_END_FINALLY: {` |
|       52 | 4164 | `	ph7_exception *pExc = (ph7_exception *)pInstr->p3;` |
|        - | 4165 | `	VmFinallyAction sAct;` |
|       52 | 4166 | `	int eKind = PH7_FA_FALLTHROUGH;` |
|        - | 4167 | `	/* Leave the try's transparent frame kept alive across the finally. */` |
|       52 | 4168 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        3 | 4169 | `		VmLeaveFrame(&(*pVm));` |
|        1 | 4170 | `	}` |
|       52 | 4171 | `	if( SySetUsed(&pVm->aFinallyAction) > 0 ){` |
|       52 | 4172 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);` |
|       52 | 4173 | `		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];` |
|       52 | 4174 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|       52 | 4175 | `		eKind = sAct.eKind;` |
|       28 | 4176 | `	}else{` |
|      ! 0 | 4177 | `		SyZero(&sAct,sizeof(sAct));` |
|      ! 0 | 4178 | `		sAct.iNextPc = pExc->iEndCatchPc;` |
|        - | 4179 | `	}` |
|       52 | 4180 | `	if( eKind == PH7_FA_FALLTHROUGH ){` |
|       12 | 4181 | `		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;` |
|       16 | 4182 | `		break;` |
|       42 | 4183 | `	}else if( eKind == PH7_FA_JMP ){` |
|        - | 4184 | `		/* break/continue: run the remaining crossed finallys, then take the jump. */` |
|        5 | 4185 | `		sxu32 iFpc = 0;` |
|        5 | 4186 | `		int nCross = sAct.nCross;` |
|        5 | 4187 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|      ! 0 | 4188 | `			sAct.nCross = nCross;` |
|      ! 0 | 4189 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      ! 0 | 4190 | `			pc = (sxi32)iFpc - 1;` |
|      ! 0 | 4191 | `			break;` |
|        - | 4192 | `		}` |
|        5 | 4193 | `		pc = (sxi32)sAct.iNextPc - 1;` |
|        5 | 4194 | `		break;` |
|       38 | 4195 | `	}else if( eKind == PH7_FA_RETHROW ){` |
|        8 | 4196 | `		ph7_class_instance *pRe = sAct.pExc;` |
|        - | 4197 | `		sxi32 _iRpE;` |
|        8 | 4198 | `		rc = VmThrowException(&(*pVm),pRe);` |
|        8 | 4199 | `		if( pRe ){ PH7_ClassInstanceUnref(pRe); }` |
|        8 | 4200 | `		if( rc == SXERR_ABORT ){ goto Abort; }` |
|        8 | 4201 | `		PH7_INLINE_RESUME_BREAK()` |
|        8 | 4202 | `		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }` |
|        8 | 4203 | `		goto Exception;` |
|      ! 0 | 4204 | `	}else{ /* PH7_FA_RETURN */` |
|       31 | 4205 | `		sxu32 iFpc = 0;` |
|       31 | 4206 | `		int nCross = sAct.nCross;` |
|       31 | 4207 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        - | 4208 | `			/* Thread the return through the next enclosing finally. */` |
|        6 | 4209 | `			sAct.nCross = nCross;` |
|        6 | 4210 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        6 | 4211 | `			pc = (sxi32)iFpc - 1;` |
|        6 | 4212 | `			break;` |
|        - | 4213 | `		}` |
|        - | 4214 | `		/* No enclosing finally left: materialize the return from this body. */` |
|       27 | 4215 | `		if( sAct.bHasRetVal && sState.pResult ){` |
|        6 | 4216 | `			PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|        2 | 4217 | `		}` |
|       27 | 4218 | `		PH7_MemObjRelease(&sAct.sRet);` |
|       27 | 4219 | `		goto Done;` |
|        - | 4220 | `	}` |
|        - | 4221 | `						 }` |
|        - | 4222 | `/*` |
|        - | 4223 | ` * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)` |
|        - | 4224 | `` * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a`` |
|        - | 4225 | ` * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);` |
|        - | 4226 | ` * OP_END_FINALLY threads it out through the finally chain and then returns.` |
|        - | 4227 | ` */` |
|       23 | 4228 | `case PH7_OP_SET_FINALLY_RET: {` |
|        - | 4229 | `	VmFinallyAction sAct;` |
|       51 | 4230 | `	sxu32 iFpc = 0;` |
|       51 | 4231 | `	int nCross = -1; /* a return crosses every enclosing finally in this function */` |
|       51 | 4232 | `	SyZero(&sAct,sizeof(sAct));` |
|       51 | 4233 | `	sAct.eKind = PH7_FA_RETURN;` |
|       51 | 4234 | `	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       51 | 4235 | `	PH7_MemObjInit(pVm,&sAct.sRet);` |
|       51 | 4236 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|       37 | 4237 | `		PH7_MemObjStore(pTos,&sAct.sRet);` |
|       37 | 4238 | `		sAct.bHasRetVal = 1;` |
|       37 | 4239 | `		VmPopOperand(&pTos,1);` |
|       16 | 4240 | `	}` |
|       51 | 4241 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        9 | 4242 | `		sAct.nCross = nCross;` |
|        9 | 4243 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        9 | 4244 | `		pc = (sxi32)iFpc - 1;` |
|        9 | 4245 | `		break;` |
|        - | 4246 | `	}` |
|        - | 4247 | `	/* No enclosing finally left: return now. */` |
|       45 | 4248 | `	if( sAct.bHasRetVal && sState.pResult ){` |
|       33 | 4249 | `		PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|       14 | 4250 | `	}` |
|       45 | 4251 | `	PH7_MemObjRelease(&sAct.sRet);` |
|       45 | 4252 | `	goto Done;` |
|        - | 4253 | `						 }` |
|        - | 4254 | `/*` |
|        - | 4255 | ` * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)` |
|        - | 4256 | `` * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action`` |
|        - | 4257 | ` * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.` |
|        - | 4258 | ` */` |
|        4 | 4259 | `case PH7_OP_SET_FINALLY_JMP: {` |
|        - | 4260 | `	VmFinallyAction sAct;` |
|       11 | 4261 | `	sxu32 iFpc = 0;` |
|       11 | 4262 | `	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */` |
|       11 | 4263 | `	SyZero(&sAct,sizeof(sAct));` |
|       11 | 4264 | `	sAct.eKind = PH7_FA_JMP;` |
|       11 | 4265 | `	sAct.iNextPc = pInstr->iP2;` |
|       11 | 4266 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        5 | 4267 | `		sAct.nCross = nCross;` |
|        5 | 4268 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        5 | 4269 | `		pc = (sxi32)iFpc - 1;` |
|        5 | 4270 | `		break;` |
|        - | 4271 | `	}` |
|        - | 4272 | `	/* No finally among the crossed trys: just take the break/continue jump. */` |
|        6 | 4273 | `	pc = (sxi32)sAct.iNextPc - 1;` |
|        6 | 4274 | `	break;` |
|        - | 4275 | `						 }` |
|        - | 4276 | `/*` |
|        - | 4277 | ` * OP_THROW * P2 *` |
|        - | 4278 | ` * Throw an user exception.` |
|        - | 4279 | ` */` |
|   500509 | 4280 | `case PH7_OP_THROW: {` |
|        - | 4281 | `	VmOpRc rcOp;` |
|  1001023 | 4282 | `	sState.pTos = pTos;` |
|  1001023 | 4283 | `	sState.pc = pc;` |
|  1001023 | 4284 | `	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);` |
|  1001023 | 4285 | `	pTos = sState.pTos;` |
|  1001023 | 4286 | `	pc = sState.pc;` |
|  1001023 | 4287 | `	if( rcOp == VM_OP_ABORT ){` |
|       32 | 4288 | `		goto Abort;` |
|  1000995 | 4289 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|   600555 | 4290 | `		goto Exception;` |
|        - | 4291 | `	}` |
|   400445 | 4292 | `	break;` |
|        - | 4293 | `					  }` |
|        - | 4294 | `/*` |
|        - | 4295 | ` * OP_FOREACH_INIT * P2 P3` |
|        - | 4296 | ` * Prepare a foreach step.` |
|        - | 4297 | ` */` |
|    13760 | 4298 | `case PH7_OP_FOREACH_INIT: {` |
|        - | 4299 | `	VmOpRc rcOp;` |
|    27525 | 4300 | `	sState.pTos = pTos;` |
|    27525 | 4301 | `	sState.pc = pc;` |
|    27525 | 4302 | `	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);` |
|    27525 | 4303 | `	pTos = sState.pTos;` |
|    27525 | 4304 | `	pc = sState.pc;` |
|    27525 | 4305 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4306 | `		goto Abort;` |
|    27525 | 4307 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4308 | `		goto Exception;` |
|        - | 4309 | `	}` |
|    27525 | 4310 | `	break;` |
|        - | 4311 | `					  }` |
|        - | 4312 | `/*` |
|        - | 4313 | ` * OP_FOREACH_STEP * P2 P3` |
|        - | 4314 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - | 4315 | ` */` |
|   149316 | 4316 | `case PH7_OP_FOREACH_STEP: {` |
|        - | 4317 | `	VmOpRc rcOp;` |
|   298637 | 4318 | `	sState.pTos = pTos;` |
|   298637 | 4319 | `	sState.pc = pc;` |
|   298637 | 4320 | `	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);` |
|   298637 | 4321 | `	pTos = sState.pTos;` |
|   298637 | 4322 | `	pc = sState.pc;` |
|   298637 | 4323 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 4324 | `		goto Abort;` |
|   298635 | 4325 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4326 | `		goto Exception;` |
|        - | 4327 | `	}` |
|   298635 | 4328 | `	break;` |
|        - | 4329 | `						  }` |
|        - | 4330 | `/*` |
|        - | 4331 | ` * OP_MEMBER P1 P2` |
|        - | 4332 | ` * Load class attribute/method on the stack.` |
|        - | 4333 | ` */` |
|  1623479 | 4334 | `case PH7_OP_MEMBER: {` |
|        - | 4335 | `	VmOpRc rcOp;` |
|  3246963 | 4336 | `	sState.pTos = pTos;` |
|  3246963 | 4337 | `	sState.pc = pc;` |
|  3246963 | 4338 | `	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);` |
|  3246963 | 4339 | `	pTos = sState.pTos;` |
|  3246963 | 4340 | `	pc = sState.pc;` |
|  3246963 | 4341 | `	if( rcOp == VM_OP_ABORT ){` |
|        8 | 4342 | `		goto Abort;` |
|  3246957 | 4343 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       27 | 4344 | `		goto Exception;` |
|        - | 4345 | `	}` |
|  3246933 | 4346 | `	break;` |
|        - | 4347 | `					  }` |
|        - | 4348 | `/*` |
|        - | 4349 | ` * OP_NEW P1 * * *` |
|        - | 4350 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - | 4351 | ` */` |
|   553638 | 4352 | `case PH7_OP_NEW: {` |
|        - | 4353 | `	VmOpRc rcOp;` |
|  1107281 | 4354 | `	sState.pTos = pTos;` |
|  1107281 | 4355 | `	sState.pc = pc;` |
|  1107281 | 4356 | `	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);` |
|  1107281 | 4357 | `	pTos = sState.pTos;` |
|  1107281 | 4358 | `	pc = sState.pc;` |
|  1107281 | 4359 | `	if( rcOp == VM_OP_ABORT ){` |
|        5 | 4360 | `		goto Abort;` |
|  1107277 | 4361 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       18 | 4362 | `		goto Exception;` |
|        - | 4363 | `	}` |
|  1107261 | 4364 | `	break;` |
|        - | 4365 | `					  }` |
|        - | 4366 | `/*` |
|        - | 4367 | ` * OP_CLONE * * *` |
|        - | 4368 | ` * Perfome a clone operation.` |
|        - | 4369 | ` */` |
|      110 | 4370 | `case PH7_OP_CLONE: {` |
|        - | 4371 | `	VmOpRc rcOp;` |
|      225 | 4372 | `	sState.pTos = pTos;` |
|      225 | 4373 | `	sState.pc = pc;` |
|      225 | 4374 | `	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);` |
|      225 | 4375 | `	pTos = sState.pTos;` |
|      225 | 4376 | `	pc = sState.pc;` |
|      225 | 4377 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4378 | `		goto Abort;` |
|      225 | 4379 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 4380 | `		goto Exception;` |
|        - | 4381 | `	}` |
|      223 | 4382 | `	break;` |
|        - | 4383 | `					  }` |
|        - | 4384 | `/*` |
|        - | 4385 | ` * OP_CLONE_APPLY * * *` |
|        - | 4386 | ` *  Apply the PHP 8.5 clone($obj, $withProperties) property updates. The updates` |
|        - | 4387 | ` *  array is on the stack top and the freshly-cloned object (from OP_CLONE) is` |
|        - | 4388 | ` *  directly below it. Each entry is applied as a scope-aware property write` |
|        - | 4389 | ` *  (AFTER __clone() has already run); the array is then popped, leaving the` |
|        - | 4390 | ` *  clone as the result.` |
|        - | 4391 | ` */` |
|        8 | 4392 | `case PH7_OP_CLONE_APPLY: {` |
|        - | 4393 | `	VmOpRc rcOp;` |
|       17 | 4394 | `	sState.pTos = pTos;` |
|       17 | 4395 | `	sState.pc = pc;` |
|       17 | 4396 | `	rcOp = VmExecOpCloneApply(&(*pVm),&sState,pInstr);` |
|       17 | 4397 | `	pTos = sState.pTos;` |
|       17 | 4398 | `	pc = sState.pc;` |
|       17 | 4399 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4400 | `		goto Abort;` |
|       17 | 4401 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4402 | `		goto Exception;` |
|        - | 4403 | `	}` |
|       17 | 4404 | `	break;` |
|        - | 4405 | `					  }` |
|        - | 4406 | `/*` |
|        - | 4407 | ` * OP_SWITCH * * P3` |
|        - | 4408 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - | 4409 | ` */` |
|      172 | 4410 | `case PH7_OP_SWITCH: {` |
|        - | 4411 | `	VmOpRc rcOp;` |
|      349 | 4412 | `	sState.pTos = pTos;` |
|      349 | 4413 | `	sState.pc = pc;` |
|      349 | 4414 | `	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);` |
|      349 | 4415 | `	pTos = sState.pTos;` |
|      349 | 4416 | `	pc = sState.pc;` |
|      349 | 4417 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4418 | `		goto Abort;` |
|      349 | 4419 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4420 | `		goto Exception;` |
|        - | 4421 | `	}` |
|      349 | 4422 | `	break;` |
|        - | 4423 | `					  }` |
|        - | 4424 | `/*` |
|        - | 4425 | ` * OP_MATCH * * P3` |
|        - | 4426 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - | 4427 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - | 4428 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - | 4429 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - | 4430 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - | 4431 | ` */` |
|       73 | 4432 | `case PH7_OP_MATCH: {` |
|        - | 4433 | `	VmOpRc rcOp;` |
|      151 | 4434 | `	sState.pTos = pTos;` |
|      151 | 4435 | `	sState.pc = pc;` |
|      151 | 4436 | `	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);` |
|      151 | 4437 | `	pTos = sState.pTos;` |
|      151 | 4438 | `	pc = sState.pc;` |
|      151 | 4439 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4440 | `		goto Abort;` |
|      151 | 4441 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        6 | 4442 | `		goto Exception;` |
|        - | 4443 | `	}` |
|      146 | 4444 | `	break;` |
|        - | 4445 | `					  }` |
|        - | 4446 | `/*` |
|        - | 4447 | ` * OP_YIELD P1 P2 *` |
|        - | 4448 | ` *  Yield a value from a generator function.` |
|        - | 4449 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - | 4450 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - | 4451 | ` */` |
|      602 | 4452 | `case PH7_OP_YIELD: {` |
|        - | 4453 | `	ph7_generator *pGen;` |
|     1209 | 4454 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 4455 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 | 4456 | `		goto Abort;` |
|        - | 4457 | `	}` |
|     1209 | 4458 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 4459 | ``		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's`` |
|        - | 4460 | `		 * pending finallys tried to yield — PHP forbids it. */` |
|      ! 0 | 4461 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 4462 | `			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 4463 | `			goto Abort;` |
|        - | 4464 | `		}` |
|      ! 0 | 4465 | `		goto Exception;` |
|        - | 4466 | `	}` |
|     1209 | 4467 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|     1209 | 4468 | `	if( pInstr->iP2 ){` |
|        - | 4469 | `		/* yield $key => $value: value on top, key below */` |
|        - | 4470 | `#ifdef UNTRUST` |
|        - | 4471 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - | 4472 | `#endif` |
|       70 | 4473 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       70 | 4474 | `		VmPopOperand(&pTos, 1);` |
|       70 | 4475 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       70 | 4476 | `		VmPopOperand(&pTos, 1);` |
|        - | 4477 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       70 | 4478 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|       47 | 4479 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|       47 | 4480 | `			if( nKey >= pGen->iImplicitKey ){` |
|       47 | 4481 | `				pGen->iImplicitKey = nKey + 1;` |
|       23 | 4482 | `			}` |
|       25 | 4483 | `		}` |
|     1175 | 4484 | `	}else if( pInstr->iP1 ){` |
|        - | 4485 | `		/* yield $value */` |
|        - | 4486 | `#ifdef UNTRUST` |
|        - | 4487 | `		if( pTos < pStack ) goto Abort;` |
|        - | 4488 | `#endif` |
|     1141 | 4489 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|     1141 | 4490 | `		VmPopOperand(&pTos, 1);` |
|        - | 4491 | `		/* Auto-increment key */` |
|     1141 | 4492 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|     1141 | 4493 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|     1141 | 4494 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|      573 | 4495 | `	}else{` |
|        - | 4496 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 | 4497 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 4498 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 4499 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 | 4500 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - | 4501 | `	}` |
|        - | 4502 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|     1209 | 4503 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|     1209 | 4504 | `	goto Suspend;` |
|        - | 4505 | `}` |
|        - | 4506 | `/*` |
|        - | 4507 | ` * OP_YIELD_FROM * * *` |
|        - | 4508 | ` *` |
|        - | 4509 | `` * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an`` |
|        - | 4510 | ` * array/Traversable/Generator from the OUTER generator, preserving the inner` |
|        - | 4511 | ` * keys; the expression evaluates to the inner Generator's return value (or NULL).` |
|        - | 4512 | ` *` |
|        - | 4513 | ` * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,` |
|        - | 4514 | ` * advances the per-instance delegate cursor stored on the exec context (never the` |
|        - | 4515 | ` * shared foreach aStep, so independent generator instances cannot clash). The` |
|        - | 4516 | ` * iterable operand is consumed on first entry; the expression result is pushed at` |
|        - | 4517 | ` * exhaustion — net stack effect +1, identical to OP_YIELD.` |
|        - | 4518 | ` */` |
|       93 | 4519 | `case PH7_OP_YIELD_FROM: {` |
|        - | 4520 | `	ph7_generator *pGenFrom;` |
|        - | 4521 | `	ph7_exec_ctx *pCtxFrom;` |
|        - | 4522 | `	ph7_value sKey,sVal;` |
|      191 | 4523 | `	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */` |
|      191 | 4524 | `	int bExhausted = 0;` |
|      191 | 4525 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 4526 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");` |
|      ! 0 | 4527 | `		goto Abort;` |
|        - | 4528 | `	}` |
|      191 | 4529 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 4530 | ``		/* A `yield from` reached while VmCloseCtx force-drives this destroyed`` |
|        - | 4531 | `		 * generator's pending finallys — PHP forbids it (distinct message from a` |
|        - | 4532 | ``		 * bare `yield`, mirroring the OP_YIELD guard above). */`` |
|      ! 0 | 4533 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 4534 | `			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 4535 | `			goto Abort;` |
|        - | 4536 | `		}` |
|      ! 0 | 4537 | `		goto Exception;` |
|        - | 4538 | `	}` |
|      191 | 4539 | `	pCtxFrom = pVm->pActiveCtx;` |
|      191 | 4540 | `	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;` |
|      191 | 4541 | `	PH7_MemObjInit(pVm,&sKey);` |
|      191 | 4542 | `	PH7_MemObjInit(pVm,&sVal);` |
|      191 | 4543 | `	if( pCtxFrom->iDelegateState == 0 ){` |
|        - | 4544 | `		/* First entry: classify the iterable on the stack top. */` |
|       79 | 4545 | `		int bIterable = 1;` |
|        - | 4546 | `#ifdef UNTRUST` |
|        - | 4547 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 4548 | `#endif` |
|       79 | 4549 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       29 | 4550 | `			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       29 | 4551 | `			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;` |
|       29 | 4552 | `			pCtxFrom->iDelegateState = 1;` |
|       67 | 4553 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       51 | 4554 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       51 | 4555 | `			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       51 | 4556 | `			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){` |
|       41 | 4557 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       41 | 4558 | `				pCtxFrom->iDelegateState = 3;` |
|       31 | 4559 | `			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){` |
|        9 | 4560 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        9 | 4561 | `				pCtxFrom->iDelegateState = 2;` |
|        6 | 4562 | `			}else{` |
|        5 | 4563 | `				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - | 4564 | `					sizeof("IteratorAggregate")-1,FALSE,0);` |
|        7 | 4565 | `				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){` |
|        - | 4566 | `					/* Delegate to the Iterator returned by getIterator() */` |
|        - | 4567 | `					ph7_value sIt;` |
|        5 | 4568 | `					PH7_MemObjInit(pVm,&sIt);` |
|        5 | 4569 | `					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);` |
|        5 | 4570 | `					if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){` |
|        - | 4571 | `						/* getIterator() threw/aborted: drop it, consume the` |
|        - | 4572 | `						 * operand, and propagate. */` |
|      ! 0 | 4573 | `						PH7_MemObjRelease(&sIt);` |
|      ! 0 | 4574 | `						VmPopOperand(&pTos,1);` |
|      ! 0 | 4575 | `						goto yf_propagate;` |
|        - | 4576 | `					}` |
|        4 | 4577 | `					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls` |
|        5 | 4578 | `						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){` |
|        5 | 4579 | `						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);` |
|        5 | 4580 | `						pCtxFrom->iDelegateState = 2;` |
|        3 | 4581 | `					}else{` |
|      ! 0 | 4582 | `						bIterable = 0;` |
|        - | 4583 | `					}` |
|        5 | 4584 | `					PH7_MemObjRelease(&sIt);` |
|        3 | 4585 | `				}else{` |
|      ! 0 | 4586 | `					bIterable = 0;` |
|        - | 4587 | `				}` |
|        - | 4588 | `			}` |
|       28 | 4589 | `		}else{` |
|        6 | 4590 | `			bIterable = 0;` |
|        - | 4591 | `		}` |
|       79 | 4592 | `		VmPopOperand(&pTos,1); /* Consume the iterable operand */` |
|       79 | 4593 | `		if( !bIterable ){` |
|        - | 4594 | `			/* Non-iterable source: throw a catchable Error (PHP 8.5), then` |
|        - | 4595 | `			 * funnel through the shared teardown/route path. */` |
|        6 | 4596 | `			rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 4597 | `				"Can use \"yield from\" only with arrays and Traversables",` |
|        - | 4598 | `				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);` |
|        6 | 4599 | `			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        6 | 4600 | `			goto yf_propagate;` |
|        - | 4601 | `		}` |
|       75 | 4602 | `		if( pCtxFrom->iDelegateState >= 2 ){` |
|        - | 4603 | `			/* rewind() the delegate (also starts a fresh generator) */` |
|       51 | 4604 | `			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 4605 | `				"rewind",sizeof("rewind")-1,0);` |
|       51 | 4606 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       23 | 4607 | `		}` |
|       40 | 4608 | `	}else{` |
|        - | 4609 | `		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a` |
|        - | 4610 | `		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()` |
|        - | 4611 | `		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)` |
|        - | 4612 | `		 * ignore send() and just advance with next(). */` |
|        - | 4613 | `#ifdef UNTRUST` |
|        - | 4614 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 4615 | `#endif` |
|      117 | 4616 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       68 | 4617 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|        - | 4618 | `			/* A pending Generator::throw() on the outer was parked on pInjected by the` |
|        - | 4619 | `			 * body-entry gate (which skips its own raise for state 3); forward it as a` |
|        - | 4620 | `			 * throw into the delegate, else forward the sent value (pTos, which` |
|        - | 4621 | `			 * VmResumeCtx copies into the inner's own stack). */` |
|       68 | 4622 | `			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;` |
|       68 | 4623 | `			pCtxFrom->pInjected = 0;` |
|       68 | 4624 | `			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       68 | 4625 | `				if( pInjFwd ){` |
|        - | 4626 | `					/* Borrowed ref: the outer's Generator::throw() holds it across this` |
|        - | 4627 | `					 * whole resume, so the inner inject path must not unref it. */` |
|        5 | 4628 | `					pInner->pCtx->pInjected = pInjFwd;` |
|        5 | 4629 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);` |
|        5 | 4630 | `					pInner->pCtx->pInjected = 0;` |
|        3 | 4631 | `				}else{` |
|       64 | 4632 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);` |
|        4 | 4633 | `				}` |
|       32 | 4634 | `			}else if( pInjFwd ){` |
|        - | 4635 | `				/* No live delegate to receive the throw (inner already finished/closed):` |
|        - | 4636 | `				 * raise it at the yield-from in the outer generator's own frame. */` |
|      ! 0 | 4637 | `				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);` |
|      ! 0 | 4638 | `				pTF->iFlags \|= VM_FRAME_THROW;` |
|      ! 0 | 4639 | `				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 4640 | `			}` |
|       68 | 4641 | `			PH7_MemObjRelease(pTos);` |
|       68 | 4642 | `			pTos--;` |
|       68 | 4643 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       33 | 4644 | `		}else{` |
|       53 | 4645 | `			PH7_MemObjRelease(pTos);` |
|       53 | 4646 | `			pTos--;` |
|       53 | 4647 | `			if( pCtxFrom->iDelegateState >= 2 ){` |
|       17 | 4648 | `				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 4649 | `					"next",sizeof("next")-1,0);` |
|       17 | 4650 | `				if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 4651 | `			}` |
|        - | 4652 | `		}` |
|        - | 4653 | `	}` |
|        - | 4654 | `	/* Fetch the current (key,value) of the delegate, or mark exhausted. */` |
|      177 | 4655 | `	if( pCtxFrom->iDelegateState == 1 ){` |
|       63 | 4656 | `		if( pCtxFrom->pDelegateNode == 0 ){` |
|       21 | 4657 | `			bExhausted = 1;` |
|       13 | 4658 | `		}else{` |
|       47 | 4659 | `			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);` |
|       47 | 4660 | `			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);` |
|        - | 4661 | `			/* Forward traversal follows pPrev (the hashmap's "reverse link",` |
|        - | 4662 | `			 * matching PH7_HashmapGetNextEntry). */` |
|       47 | 4663 | `			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;` |
|        - | 4664 | `		}` |
|       34 | 4665 | `	}else{` |
|      119 | 4666 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|        - | 4667 | `		ph7_value sValid;` |
|        - | 4668 | `		int isValid;` |
|      119 | 4669 | `		PH7_MemObjInit(pVm,&sValid);` |
|      119 | 4670 | `		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|      119 | 4671 | `		PH7_MemObjToBool(&sValid);` |
|      119 | 4672 | `		isValid = (sValid.x.iVal != 0);` |
|      119 | 4673 | `		PH7_MemObjRelease(&sValid);` |
|      119 | 4674 | `		if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      119 | 4675 | `		if( !isValid ){` |
|       28 | 4676 | `			bExhausted = 1;` |
|       16 | 4677 | `		}else{` |
|       95 | 4678 | `			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|       95 | 4679 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       95 | 4680 | `			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       95 | 4681 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        - | 4682 | `		}` |
|        - | 4683 | `	}` |
|      177 | 4684 | `	if( bExhausted ){` |
|        - | 4685 | `		/* Expression value: inner Generator's return value (state 3) or NULL. */` |
|        - | 4686 | `		ph7_value sResult;` |
|       45 | 4687 | `		PH7_MemObjInit(pVm,&sResult);` |
|       45 | 4688 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       24 | 4689 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|       24 | 4690 | `			if( pInner && pInner->pCtx ){` |
|       24 | 4691 | `				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);` |
|       10 | 4692 | `			}` |
|       10 | 4693 | `		}` |
|       45 | 4694 | `		PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       45 | 4695 | `		pCtxFrom->pDelegateNode = 0;` |
|       45 | 4696 | `		pCtxFrom->iDelegateState = 0;` |
|       45 | 4697 | `		pTos++;` |
|       45 | 4698 | `		PH7_MemObjStore(&sResult,pTos);` |
|       45 | 4699 | `		PH7_MemObjRelease(&sResult);` |
|       45 | 4700 | `		PH7_MemObjRelease(&sKey);` |
|       45 | 4701 | `		PH7_MemObjRelease(&sVal);` |
|       45 | 4702 | `		break; /* fall through to pc+1 with the result on the stack top */` |
|        - | 4703 | `	}` |
|        - | 4704 | `	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.` |
|        - | 4705 | `	 * The outer generator's implicit auto-key counter is NOT advanced by the` |
|        - | 4706 | ``	 * delegated keys — PHP keeps it independent across `yield from`, so a later`` |
|        - | 4707 | ``	 * plain `yield` continues from the outer's own counter. */`` |
|      137 | 4708 | `	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);` |
|      137 | 4709 | `	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);` |
|      137 | 4710 | `	PH7_MemObjRelease(&sKey);` |
|      137 | 4711 | `	PH7_MemObjRelease(&sVal);` |
|        - | 4712 | `	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */` |
|      137 | 4713 | `	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));` |
|      137 | 4714 | `	goto Suspend;` |
|        7 | 4715 | `yf_propagate:` |
|        - | 4716 | `	/* A delegate iterator method threw/aborted (or the source was non-iterable):` |
|        - | 4717 | `	 * tear down the delegation, then route via the shared dispatch macro. rcm is` |
|        - | 4718 | `	 * always PH7_EXCEPTION or PH7_ABORT here. */` |
|       17 | 4719 | `	PH7_MemObjRelease(&sKey);` |
|       17 | 4720 | `	PH7_MemObjRelease(&sVal);` |
|       17 | 4721 | `	PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       17 | 4722 | `	pCtxFrom->pDelegateNode = 0;` |
|       17 | 4723 | `	pCtxFrom->iDelegateState = 0;` |
|       17 | 4724 | `	PH7_DISPATCH_ENFORCE_RC(rcm)` |
|      ! 0 | 4725 | `	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */` |
|        - | 4726 | `}` |
|        - | 4727 | `/*` |
|        - | 4728 | ` * OP_CALL P1 * *` |
|        - | 4729 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - | 4730 | ` *  function on the stack.` |
|        - | 4731 | ` */` |
|  3406406 | 4732 | `case PH7_OP_CALL: {` |
|        - | 4733 | `	/* iP2 = hasSpread (compile-time). Count only THIS call's own unpack` |
|        - | 4734 | `	 * expansion (VmSpreadOwnExtra, derived from the captured runs on top of the` |
|        - | 4735 | `	 * stack) — an INNER spread-bearing call evaluated inside this argument list` |
|        - | 4736 | ``	 * (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not`` |
|        - | 4737 | `	 * be conflated, which a single shared accumulator could not express. */` |
|  6814700 | 4738 | `	sxi32 nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|        - | 4739 | `	ph7_value *pArg;` |
|        - | 4740 | `	/* Consume the engine's magic-dispatch latch here, at the head of the ONE call` |
|        - | 4741 | `	 * it was set for, whatever path that call then takes. Left standing it would` |
|        - | 4742 | `	 * describe the next call instead. */` |
|  6814700 | 4743 | `	int bMagicDispatch = pVm->bMagicDispatch;` |
|  6814700 | 4744 | `	pVm->bMagicDispatch = 0;` |
|  6814700 | 4745 | `	pArg = &pTos[-nCallArgs];` |
|        - | 4746 | `	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED` |
|        - | 4747 | `	 * arguments, and a spread expanding to !=1 element shifts the actual positions` |
|        - | 4748 | `	 * of any following compile-time named args. The effective per-actual-slot name` |
|        - | 4749 | `	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)` |
|        - | 4750 | `	 * is built PER PATH against that path's finalized arg base — a method call pops` |
|        - | 4751 | `	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch` |
|        - | 4752 | `	 * site rather than built once here. */` |
|        - | 4753 | `	VmCallArgMap sEffMap;` |
|  6814700 | 4754 | `	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;` |
|        - | 4755 | `	SyHashEntry *pEntry;` |
|        - | 4756 | `	SyString sName;` |
|        - | 4757 | `	/* The host-call trio, hoisted out of the foreign-function branch below so the` |
|        - | 4758 | `	 * NativeCall label there can also be reached from the compiled-function branch:` |
|        - | 4759 | `	 * a VM_FUNC_NATIVE method is a C body wearing a method's clothes, and it runs on` |
|        - | 4760 | `	 * exactly the foreign path (no frame, no call record, no operand stack) once` |
|        - | 4761 | `	 * $this and the called class have been resolved the method way. Jumping into` |
|        - | 4762 | `	 * that branch would otherwise cross these declarations. */` |
|        - | 4763 | `	ph7_user_func *pFunc;` |
|        - | 4764 | `	ph7_context sCtx;` |
|        - | 4765 | `	ph7_value sRet;` |
|        - | 4766 | `	/* Native-METHOD call state; all 0 for a plain host function, which is what the` |
|        - | 4767 | `	 * foreign branch's own fallthrough leaves.` |
|        - | 4768 | `	 *   pNativeOwned — the reference the method path took, released at the shared tail` |
|        - | 4769 | `	 *   pNativeRecv  — what the body actually sees as $this (0 for a static method)` |
|        - | 4770 | `	 *   pNativeClass — the late-static-binding target */` |
|  6814700 | 4771 | `	ph7_class_instance *pNativeOwned = 0;` |
|  6814700 | 4772 | `	ph7_class_instance *pNativeRecv = 0;` |
|  6814700 | 4773 | `	ph7_class *pNativeClass = 0;` |
|        - | 4774 | `	/* A Closure object is callable: unwrap it to its underlying string callable so the` |
|        - | 4775 | `	 * dispatch below handles it (rather than treating it as a generic object and looking` |
|        - | 4776 | `	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure` |
|        - | 4777 | `	 * so a plain __invoke object skips the temp-value work entirely. */` |
|  6814700 | 4778 | `	if( VmValueIsClosure(pVm,pTos) ){` |
|        - | 4779 | `		ph7_value sCallable;` |
|     3549 | 4780 | `		PH7_MemObjInit(pVm,&sCallable);` |
|     3549 | 4781 | `		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){` |
|     3549 | 4782 | `			PH7_MemObjRelease(pTos);` |
|     3549 | 4783 | `			PH7_MemObjStore(&sCallable,pTos);` |
|     1772 | 4784 | `		}` |
|     3549 | 4785 | `		PH7_MemObjRelease(&sCallable);` |
|     1772 | 4786 | `	}` |
|        - | 4787 | `	/* Extract function name */` |
|  6814700 | 4788 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|   300287 | 4789 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 4790 | `			ph7_value sResult;` |
|        - | 4791 | `			sxi32 rcArr;` |
|        - | 4792 | `			{` |
|        - | 4793 | `				/* php validates the SHAPE of an array callable first: it must hold exactly` |
|        - | 4794 | `				 * two elements. PH7 handed any array to the dispatcher, which failed` |
|        - | 4795 | ``				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */`` |
|   100188 | 4796 | `				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - | 4797 | `				char zCbMsg[192];` |
|   100188 | 4798 | `				const char *zCbErr = 0;` |
|   100188 | 4799 | `				int bCbRaised = 0; /* the class lookup ran an autoloader that threw */` |
|   100188 | 4800 | `				if( pCbMap && pCbMap->nEntry == 2 ){` |
|        - | 4801 | `					/* Shape is right; now check it actually RESOLVES. The shared dispatcher` |
|        - | 4802 | `					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for` |
|        - | 4803 | `					 * an unresolvable [class,method] pair -- silence a caller cannot detect --` |
|        - | 4804 | `					 * and its contract is relied on by call_user_func/usort, so the throw` |
|        - | 4805 | `					 * belongs here at the call site. */` |
|   100176 | 4806 | `					ph7_value *pCbCls = 0;` |
|   100176 | 4807 | `					ph7_value *pCbMeth = 0;` |
|        - | 4808 | `					/* php reads the INTEGER indices 0 and 1, not the first two entries in` |
|        - | 4809 | `					 * insertion order. Two entries at other keys are a SHAPE error of their` |
|        - | 4810 | `					 * own ("has to contain indices 0 and 1"), distinct from the` |
|        - | 4811 | ``					 * wrong-element-COUNT message below; `[1=>'m',0=>'C']` holds both, so`` |
|        - | 4812 | `					 * the target is index 0 -- the reverse of what PH7 walked. */` |
|   100176 | 4813 | `					if( !PH7_VmArrayCallableParts(&(*pVm),pCbMap,&pCbCls,&pCbMeth) ){` |
|        9 | 4814 | `						zCbErr = "Array callback has to contain indices 0 and 1";` |
|        5 | 4815 | `					}else{` |
|        - | 4816 | `						/* The resolution below can run an autoloader that throws; when it does,` |
|        - | 4817 | `						 * php propagates THAT exception and never reports the class missing. */` |
|   100168 | 4818 | `						sxi32 nCbBrc = pVm->nBoundaryRc;` |
|   100168 | 4819 | `						const void *pCbRes = (const void *)pVm->pResumeFrame;` |
|   150250 | 4820 | `						zCbErr = VmDirectArrayCallableError(&(*pVm),pCbCls,pCbMeth,` |
|    50082 | 4821 | `							zCbMsg,sizeof(zCbMsg));` |
|   100168 | 4822 | `						if( zCbErr && PH7_VmClassLookupRaised(&(*pVm),nCbBrc,pCbRes) ){` |
|        6 | 4823 | `							bCbRaised = 1;` |
|        2 | 4824 | `						}` |
|        - | 4825 | `					}` |
|    50086 | 4826 | `				}` |
|   100188 | 4827 | `				if( pCbMap == 0 \|\| pCbMap->nEntry != 2 \|\| zCbErr ){` |
|        - | 4828 | `					sxi32 rcCb;` |
|       73 | 4829 | `					if( pInstr->iP2 ){` |
|      ! 0 | 4830 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 4831 | `					}` |
|       73 | 4832 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 4833 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 4834 | `					}` |
|       73 | 4835 | `					PH7_MemObjRelease(pTos);` |
|       73 | 4836 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       73 | 4837 | `					pTos->nIdx = SXU32_HIGH;` |
|       73 | 4838 | `					if( bCbRaised ){` |
|        - | 4839 | `						/* Land the autoloader's own throw: consume the parked status (an` |
|        - | 4840 | `						 * in-place catch leaves 0 behind plus a recorded resume frame, which` |
|        - | 4841 | `						 * the router below picks up). */` |
|        6 | 4842 | `						rcCb = pVm->nBoundaryRc;` |
|        6 | 4843 | `						pVm->nBoundaryRc = 0;` |
|        6 | 4844 | `						if( rcCb == PH7_ABORT ){ goto Abort; }` |
|        6 | 4845 | `						rc = PH7_EXCEPTION;` |
|       14 | 4846 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4847 | `					}` |
|       68 | 4848 | `					if( zCbErr == 0 ){` |
|       13 | 4849 | `						zCbErr = "Array callback must have exactly two elements";` |
|        6 | 4850 | `					}` |
|       68 | 4851 | `					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));` |
|       68 | 4852 | `					if( rcCb == SXERR_ABORT ){ goto Abort; }` |
|       68 | 4853 | `					rc = rcCb;` |
|        - | 4854 | `					/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 4855 | `					 * (SXRET_OK + recorded resume) FELL THROUGH and kept dispatching` |
|        - | 4856 | `					 * the malformed callable inside the try. Route like OP_THROW. */` |
|       84 | 4857 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4858 | `				}` |
|        - | 4859 | `			}` |
|        - | 4860 | `			/* Build the effective spread-key map (and consume this call's runs)` |
|        - | 4861 | `			 * against this path's arg base (the array-callable slot isn't popped). */` |
|   150175 | 4862 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100114 | 4863 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 4864 | `			/* D1: an array callable dispatches through the shared helper below, which does not` |
|        - | 4865 | `			 * expose the target's per-parameter by-ref flags here. This path over-vivified every` |
|        - | 4866 | ``			 * plain-var argument before D1 (so `[$o,'m'](&$x)` out-params worked); preserve that`` |
|        - | 4867 | `			 * by materializing every deferred arg as by-ref. Refining these to precise by-value` |
|        - | 4868 | `			 * semantics is a later slice. */` |
|        - | 4869 | `			{` |
|   100118 | 4870 | `				sxi32 rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,/*bAllByRef*/1,0);` |
|   100118 | 4871 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 4872 | `			}` |
|   100118 | 4873 | `			SySetReset(&aArg);` |
|   100216 | 4874 | `			while( pArg < pTos ){` |
|      101 | 4875 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      101 | 4876 | `				pArg++;` |
|        3 | 4877 | `			}` |
|   100118 | 4878 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 4879 | `			/* May be a class instance and it's static method. Forward this call's named-arg map` |
|        - | 4880 | ``			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —`` |
|        - | 4881 | `			 * mirroring the __invoke-object branch below. */` |
|   100118 | 4882 | `			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|   100118 | 4883 | `			SySetReset(&aArg);` |
|        - | 4884 | `			/* Pop given arguments */` |
|   100118 | 4885 | `			if( nCallArgs > 0 ){` |
|       81 | 4886 | `				VmPopOperand(&pTos,nCallArgs);` |
|       39 | 4887 | `			}` |
|   100118 | 4888 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 | 4889 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 4890 | `				goto Abort;` |
|        - | 4891 | `			}` |
|   100118 | 4892 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - | 4893 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - | 4894 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - | 4895 | `				sxi32 iResumePc;` |
|   100013 | 4896 | `				PH7_MemObjRelease(&sResult);` |
|   100013 | 4897 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100007 | 4898 | `					PH7_MemObjRelease(pTos);` |
|        - | 4899 | ``					/* Drain the abandoned outer-expression operands (`1 + $cb()`)`` |
|        - | 4900 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 4901 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300011 | 4902 | `					PH7_RESUME_DRAIN()` |
|   100007 | 4903 | `					pc = iResumePc;` |
|   100007 | 4904 | `					break;` |
|        - | 4905 | `				}` |
|        7 | 4906 | `				goto Exception;` |
|        - | 4907 | `			}` |
|        - | 4908 | `			/* Copy result */` |
|      106 | 4909 | `			PH7_MemObjStore(&sResult,pTos);` |
|      106 | 4910 | `			PH7_MemObjRelease(&sResult);` |
|   200154 | 4911 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|   200094 | 4912 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - | 4913 | `			ph7_value sResult;` |
|        - | 4914 | `			sxi32 rcInv;` |
|        - | 4915 | `			/* __invoke object callable: the object slot isn't popped, so pArg is` |
|        - | 4916 | `			 * already this call's arg base — build the map + consume the runs. */` |
|   300139 | 4917 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   200090 | 4918 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 4919 | `			/* D1: like the array-callable path above, __invoke dispatches through a shared` |
|        - | 4920 | `			 * helper that hides the target's by-ref flags here. Preserve the pre-D1` |
|        - | 4921 | ``			 * over-vivification (so `$o(&$x)` out-params keep working) by materializing`` |
|        - | 4922 | `			 * every deferred arg as by-ref. */` |
|        - | 4923 | `			{` |
|   200094 | 4924 | `				sxi32 rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,/*bAllByRef*/1,0);` |
|   200094 | 4925 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 4926 | `			}` |
|   200094 | 4927 | `			SySetReset(&aArg);` |
|   200218 | 4928 | `			while( pArg < pTos ){` |
|      128 | 4929 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      128 | 4930 | `				pArg++;` |
|        4 | 4931 | `			}` |
|   200094 | 4932 | `			PH7_MemObjInit(pVm,&sResult);` |
|   300139 | 4933 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|   200090 | 4934 | `				(int)SySetUsed(&aArg),` |
|   200090 | 4935 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - | 4936 | `				&sResult,` |
|   100045 | 4937 | `				pEffCallMap);` |
|   200094 | 4938 | `			SySetReset(&aArg);` |
|        - | 4939 | `			/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable` |
|        - | 4940 | `			 * slot itself (it pops top-down and the callable IS pTos), which for a` |
|        - | 4941 | `			 * temporary like (new Plain())(...) holds the only reference — popping it` |
|        - | 4942 | `			 * would free pThis before VmRaiseNotCallable reads its class name below.` |
|        - | 4943 | `			 * Only the not-callable branch dereferences pThis afterwards, so pin just` |
|        - | 4944 | `			 * for that case; the matching PH7_ClassInstanceUnref drops it (destroying` |
|        - | 4945 | `			 * the temporary). The other branches let the pop free the temp as before. */` |
|   200094 | 4946 | `			if( rcInv == SXERR_INVALID ){` |
|   100016 | 4947 | `				pThis->iRef++;` |
|    50007 | 4948 | `			}` |
|   200094 | 4949 | `			if( nCallArgs > 0 ){` |
|       84 | 4950 | `				VmPopOperand(&pTos,nCallArgs);` |
|       40 | 4951 | `			}` |
|   200094 | 4952 | `			if( rcInv == SXERR_INVALID ){` |
|        - | 4953 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - | 4954 | `				 * sResult was already released by VmCallObjectInvoke. */` |
|   100016 | 4955 | `				PH7_MemObjRelease(pTos);` |
|   100016 | 4956 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|   100016 | 4957 | `				PH7_ClassInstanceUnref(pThis);` |
|   100016 | 4958 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4959 | `					goto Abort;` |
|        - | 4960 | `				}` |
|        - | 4961 | `				{` |
|        - | 4962 | `					sxi32 iRp;` |
|   100016 | 4963 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|        - | 4964 | `						/* Drain the abandoned outer-expression operands` |
|        - | 4965 | ``						 * (`1 + $plainObj()`) to the try's base — one leaked`` |
|        - | 4966 | `						 * slot per caught throw otherwise. */` |
|   300030 | 4967 | `						PH7_RESUME_DRAIN()` |
|   100016 | 4968 | `						pc = iRp;` |
|   100016 | 4969 | `						break;` |
|        - | 4970 | `					}` |
|        - | 4971 | `				}` |
|      ! 0 | 4972 | `				goto Exception;` |
|        - | 4973 | `			}` |
|   100080 | 4974 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 | 4975 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 4976 | `				goto Abort;` |
|        - | 4977 | `			}` |
|   100080 | 4978 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - | 4979 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - | 4980 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - | 4981 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - | 4982 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - | 4983 | `				sxi32 iResumePc;` |
|   100008 | 4984 | `				PH7_MemObjRelease(&sResult);` |
|   100008 | 4985 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100006 | 4986 | `					PH7_MemObjRelease(pTos);` |
|        - | 4987 | ``					/* Drain the abandoned outer-expression operands (`1 + $inv()`)`` |
|        - | 4988 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 4989 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300010 | 4990 | `					PH7_RESUME_DRAIN()` |
|   100006 | 4991 | `					pc = iResumePc;` |
|   100006 | 4992 | `					break;` |
|        - | 4993 | `				}` |
|        3 | 4994 | `				goto Exception;` |
|        - | 4995 | `			}` |
|       74 | 4996 | `			PH7_MemObjStore(&sResult,pTos);` |
|       74 | 4997 | `			PH7_MemObjRelease(&sResult);` |
|       39 | 4998 | `		}else{` |
|        - | 4999 | `			/* php: calling a non-callable is a catchable Error naming the type` |
|        - | 5000 | `			 * ("Value of type int is not callable"), or -- for an array -- the shape it` |
|        - | 5001 | `			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so` |
|        - | 5002 | ``			 * `$x()` on a number quietly evaluated to nothing. */`` |
|        - | 5003 | `			sxi32 rcNc;` |
|        - | 5004 | `			char zMsg[128];` |
|        9 | 5005 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 5006 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");` |
|      ! 0 | 5007 | `			}else{` |
|       13 | 5008 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",` |
|        4 | 5009 | `					VmArithTypeName(pTos));` |
|        - | 5010 | `			}` |
|        - | 5011 | `			/* Consume this call's captured spread runs — a non-callable target` |
|        - | 5012 | `			 * (int/float/bool/null) reaches no dispatch build site. */` |
|        9 | 5013 | `			if( pInstr->iP2 ){` |
|      ! 0 | 5014 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 5015 | `			}` |
|        - | 5016 | `			/* Pop given arguments */` |
|        9 | 5017 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 5018 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 5019 | `			}` |
|        - | 5020 | `			/* Settle the call's result slot BEFORE throwing. */` |
|        9 | 5021 | `			PH7_MemObjRelease(pTos);` |
|        9 | 5022 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        9 | 5023 | `			pTos->nIdx = SXU32_HIGH;` |
|        9 | 5024 | `			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|        9 | 5025 | `			if( rcNc == SXERR_ABORT ){ goto Abort; }` |
|        9 | 5026 | `			rc = rcNc;` |
|        - | 5027 | `			/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 5028 | `			 * (SXRET_OK + recorded resume) FELL THROUGH and resumed the try body` |
|        - | 5029 | `			 * right after the failed call. Route like OP_THROW. */` |
|       15 | 5030 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5031 | `		}` |
|      178 | 5032 | `		break;` |
|        - | 5033 | `	}` |
|  6514418 | 5034 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 5035 | `	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just` |
|        - | 5036 | `	 * anchors it to the global namespace — strip it before resolving so a` |
|        - | 5037 | ``	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */`` |
|  6514418 | 5038 | `	if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|       15 | 5039 | `		sName.zString++;` |
|       15 | 5040 | `		sName.nByte--;` |
|        7 | 5041 | `	}` |
|        - | 5042 | `	/* Check for a compiled function first.` |
|        - | 5043 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 5044 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|  6514418 | 5045 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 5046 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - | 5047 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - | 5048 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - | 5049 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - | 5050 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - | 5051 | `	{` |
|  6514418 | 5052 | `	VmCallArgMap *pCallMap = pEffCallMap;` |
|  6514418 | 5053 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - | 5054 | `		const char *zFunc;` |
|        - | 5055 | `		const char *zEnd;` |
|        - | 5056 | `		const char *z;` |
|        - | 5057 | `		SyString sGlobal;` |
|       53 | 5058 | `		zFunc = sName.zString;` |
|       53 | 5059 | `		zEnd  = zFunc + sName.nByte;` |
|       53 | 5060 | `		z = zEnd;` |
|        - | 5061 | `		/* Find last namespace separator */` |
|      493 | 5062 | `		while( z > zFunc ){` |
|      493 | 5063 | `			if( z[-1] == '\\' ){` |
|       53 | 5064 | `				break;` |
|        - | 5065 | `			}` |
|      445 | 5066 | `			z--;` |
|        5 | 5067 | `		}` |
|       53 | 5068 | `		if( z > zFunc && z < zEnd ){` |
|        - | 5069 | `			/* Retry lookup using the unqualified/global function name */` |
|       53 | 5070 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       53 | 5071 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       24 | 5072 | `		}` |
|       24 | 5073 | `	}` |
|        - | 5074 | `	} /* end VmCallArgMap namespace scope */` |
|  6514418 | 5075 | `	if( pEntry ){` |
|        - | 5076 | `		ph7_vm_func_arg *aFormalArg;` |
|        - | 5077 | `		ph7_class_instance *pThis;` |
|        - | 5078 | `		ph7_value *pFrameStack;` |
|        - | 5079 | `		ph7_vm_func *pVmFunc;` |
|        - | 5080 | `		ph7_class *pSelf;` |
|        - | 5081 | `		ph7_class *pSelfHint;` |
|        - | 5082 | `		VmFrame *pFrame;` |
|        - | 5083 | `		ph7_value *pObj;` |
|        - | 5084 | `		VmSlot sArg;` |
|        - | 5085 | `		sxu32 n;` |
|  2208591 | 5086 | `		int bClosureThis = 0;` |
|  2208591 | 5087 | `		ph7_class *pClosureScope = 0;` |
|        - | 5088 | `		/* initialize fields */` |
|  2208591 | 5089 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|  2208591 | 5090 | `		pThis = 0;` |
|  2208591 | 5091 | `		pSelf = 0;` |
|        - | 5092 | `		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it` |
|        - | 5093 | `		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever` |
|        - | 5094 | `		 * set for a bound plain closure, which dispatches as a function, so the method branch below` |
|        - | 5095 | `		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */` |
|  2208591 | 5096 | `		if( pVm->pClosureThis ){` |
|       41 | 5097 | `			pThis = pVm->pClosureThis;` |
|       41 | 5098 | `			pVm->pClosureThis = 0;` |
|       41 | 5099 | `			bClosureThis = 1;` |
|       20 | 5100 | `		}` |
|  2208591 | 5101 | `		if( pVm->pClosureScope ){` |
|        - | 5102 | `			/* May ride alongside a bound $this, or stand alone for a` |
|        - | 5103 | ``			 * scope-only rebind (`bindTo(null, Scope::class)`). */`` |
|       37 | 5104 | `			pClosureScope = pVm->pClosureScope;` |
|       37 | 5105 | `			pVm->pClosureScope = 0;` |
|       18 | 5106 | `		}` |
|  2208591 | 5107 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - | 5108 | `			ph7_class_method *pMeth;` |
|        - | 5109 | `			/* Class method call */` |
|  1977065 | 5110 | `			ph7_value *pTarget = &pTos[-1];` |
|  1977065 | 5111 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - | 5112 | `				/* Extract the 'this' pointer */` |
|  1977065 | 5113 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - | 5114 | `					/* Instance already loaded */` |
|  1875875 | 5115 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|  1875875 | 5116 | `					pThis->iRef++;` |
|  1875875 | 5117 | `					pSelf = pThis->pClass;` |
|   937935 | 5118 | `				}` |
|  1977065 | 5119 | `				if( pSelf == 0 ){` |
|   101195 | 5120 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - | 5121 | `						/* "Late Static Binding" class name */` |
|     1571 | 5122 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|      522 | 5123 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|      522 | 5124 | `					}` |
|   101195 | 5125 | `					if( pSelf == 0 ){` |
|   100149 | 5126 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|    50073 | 5127 | `					}` |
|    50595 | 5128 | `				}` |
|  1977065 | 5129 | `				if( pThis == 0  ){` |
|   101195 | 5130 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|   101195 | 5131 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   101195 | 5132 | `					if( pFrameLocal->pParent ){` |
|        - | 5133 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|      807 | 5134 | `						pThis = pFrameLocal->pThis;` |
|      807 | 5135 | `						if( pThis ){` |
|      215 | 5136 | `							pThis->iRef++;` |
|      106 | 5137 | `						}` |
|      401 | 5138 | `					}` |
|    50595 | 5139 | `				}` |
|  1977065 | 5140 | `				VmPopOperand(&pTos,1);` |
|  1977065 | 5141 | `				PH7_MemObjRelease(pTos);` |
|        - | 5142 | `				/* Synchronize pointers. The method-name slot popped above sat BETWEEN` |
|        - | 5143 | `				 * this call's arguments and the (already-removed) target — so only now` |
|        - | 5144 | `				 * is pTos one past the last actual argument. Re-derive the unpack` |
|        - | 5145 | `				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs` |
|        - | 5146 | `				 * from run ends, which the extra target slot would otherwise offset,` |
|        - | 5147 | ``				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */`` |
|  1977065 | 5148 | `				nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);` |
|  1977065 | 5149 | `				pArg = &pTos[-nCallArgs];` |
|        - | 5150 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - | 5151 | `				 * user have already computed the random generated unique class method name` |
|        - | 5152 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - | 5153 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - | 5154 | `				 */` |
|  1977065 | 5155 | `				while( pArg < pStack ){` |
|      ! 0 | 5156 | `					pArg++;` |
|      ! 0 | 5157 | `				}` |
|  1977065 | 5158 | `				if( pSelf && pVm->bReflectBypass ){` |
|        - | 5159 | `					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection` |
|        - | 5160 | `					 * ignores visibility. Consume-once so nested calls made by the` |
|        - | 5161 | `					 * invoked body are checked normally. */` |
|       11 | 5162 | `					pVm->bReflectBypass = 0;` |
|        6 | 5163 | `				}else` |
|  1977055 | 5164 | `				if( pSelf && bMagicDispatch && PH7_MagicMethodMustBePublic(&pVmFunc->sName) ){` |
|        - | 5165 | `					/* The ENGINE reaching for a magic method php requires to be public.` |
|        - | 5166 | `					 * php only WARNS at such a declaration and then dispatches the method` |
|        - | 5167 | ``					 * regardless -- the engine calling `__get` is not the outside world`` |
|        - | 5168 | `					 * reaching for a private member -- so a non-public` |
|        - | 5169 | ``					 * `__get`/`__call`/`__invoke`/`__sleep`/... still runs, where PHL threw`` |
|        - | 5170 | ``					 * `Call to private method C::__get()` at the ACCESS and killed the`` |
|        - | 5171 | `					 * script.` |
|        - | 5172 | `					 *` |
|        - | 5173 | `					 * The latch is set by PH7_VmCallMagicMethod at the engine's own` |
|        - | 5174 | `					 * dispatch sites. It is NOT inferred from the instruction: a` |
|        - | 5175 | ``					 * first-class `$o->__get(...)` and `$o->__get('x')` reach the same C`` |
|        - | 5176 | `					 * dispatcher, and php denies both. */` |
|    50317 | 5177 | `				}else` |
|  1876431 | 5178 | `				if( pSelf ){ /* Paranoid edition */` |
|        - | 5179 | `					/* Check if the call is allowed. php binds non-public method` |
|        - | 5180 | `					 * access by the DECLARING class (pVmFunc->pUserData — the class` |
|        - | 5181 | `					 * the callee was compiled in), NOT the instance's class: an` |
|        - | 5182 | `					 * inherited base method calling $this->priv() on a child` |
|        - | 5183 | `					 * instance passes, a child's private SHADOW doesn't hijack the` |
|        - | 5184 | `					 * check for a parent callee, and the denial message names the` |
|        - | 5185 | `					 * declaring class like php. */` |
|  1876431 | 5186 | `					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|  1876431 | 5187 | `					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|  1876431 | 5188 | `					if( pMeth == 0 && pDeclClass != pSelf ){` |
|      ! 0 | 5189 | `						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      ! 0 | 5190 | `					}` |
|  1876431 | 5191 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     3191 | 5192 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pDeclClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - | 5193 | `							/* php throws a CATCHABLE Error here. The old code merely PRINTED an` |
|        - | 5194 | ``							 * uncaught-exception report and aborted, so `try { $o->priv(); }`` |
|        - | 5195 | ``							 * catch (Error $e)` never caught it and the script died. */`` |
|        - | 5196 | `							char zMsg[256];` |
|        - | 5197 | `							sxi32 rcVis;` |
|       24 | 5198 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       35 | 5199 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|       22 | 5200 | `								zVis,(int)pDeclClass->sName.nByte,pDeclClass->sName.zString,` |
|       22 | 5201 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - | 5202 | `							/* Consume this call's captured spread runs — this visibility` |
|        - | 5203 | `							 * error exits before the pVmFunc build below. */` |
|       24 | 5204 | `							if( pInstr->iP2 ){` |
|      ! 0 | 5205 | `								VmSpreadConsume(pVm);` |
|      ! 0 | 5206 | `							}` |
|        - | 5207 | `							/* Pop given arguments, and leave the call's NULL result behind. */` |
|       24 | 5208 | `							if( nCallArgs > 0 ){` |
|        5 | 5209 | `								VmPopOperand(&pTos,nCallArgs);` |
|        2 | 5210 | `							}` |
|       24 | 5211 | `							PH7_MemObjRelease(pTos);` |
|       24 | 5212 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|       24 | 5213 | `							pTos->nIdx = SXU32_HIGH;` |
|       24 | 5214 | `							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       24 | 5215 | `							if( rcVis == SXERR_ABORT ){ goto Abort; }` |
|       24 | 5216 | `							rc = rcVis;` |
|        - | 5217 | `							/* ENFORCE_RC never consulted pResumeFrame, so an in-place` |
|        - | 5218 | `							 * catch (SXRET_OK + recorded resume) FELL THROUGH and` |
|        - | 5219 | `							 * dispatched the DENIED method anyway. Route like OP_THROW. */` |
|       30 | 5220 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5221 | `						}` |
|     1582 | 5222 | `					}` |
|   938202 | 5223 | `				}` |
|   988519 | 5224 | `			}` |
|   988519 | 5225 | `		}` |
|        - | 5226 | `		/* pArg is now finalized for every pVmFunc callee (a method call popped its` |
|        - | 5227 | `		 * method-name slot above; functions/closures/generators keep the top base).` |
|        - | 5228 | `		 * Build the PHP 8.1 effective spread-key map here so the generator and the` |
|        - | 5229 | `		 * install path below both see it — and so this call's captured runs are` |
|        - | 5230 | `		 * consumed exactly once, against the correct base. */` |
|  3313049 | 5231 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  2208564 | 5232 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 5233 | `		/* Check the PHP call-depth cap (the sole site — BYTECODE.md stage 5).` |
|        - | 5234 | `		 * Default is unbounded (heap-bound recursion, decoupled from the C stack` |
|        - | 5235 | `		 * by the stage-2 trampoline); the C stack is guarded separately by` |
|        - | 5236 | `		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and` |
|        - | 5237 | `		 * then raises a clean non-catchable fatal (was: silently set NULL and` |
|        - | 5238 | `		 * continue) and halts. */` |
|  2208569 | 5239 | `		if( VmRecursionExceeded(pVm) ){` |
|        - | 5240 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - | 5241 | `			 * which walks the whole operand stack — don't release them here. */` |
|        3 | 5242 | `			VmRecursionFatal(&(*pVm));` |
|        3 | 5243 | `			goto Abort;` |
|        - | 5244 | `		}` |
|  2208567 | 5245 | `		if( pVmFunc->pNextName ){` |
|        - | 5246 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      268 | 5247 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|      132 | 5248 | `		}` |
|        - | 5249 | ``		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's`` |
|        - | 5250 | ``		 * `Class::method`). Computed after overload resolution so it reflects the selected method.`` |
|        - | 5251 | ``		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed`` |
|        - | 5252 | `		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method` |
|        - | 5253 | ``		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the`` |
|        - | 5254 | `		 * USING class; we don't carry the using class on the shared struct, so fall back to the` |
|        - | 5255 | `		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for` |
|        - | 5256 | `		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */` |
|  2208567 | 5257 | `		pSelfHint = pSelf;` |
|  2208567 | 5258 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|  1977043 | 5259 | `			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;` |
|  1977043 | 5260 | `			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|  1975893 | 5261 | `				pSelfHint = pDecl;` |
|   987944 | 5262 | `			}` |
|   988519 | 5263 | `		}` |
|  2208567 | 5264 | `		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 5265 | `			/* Push the closure's called-class as this frame's LSB class so` |
|        - | 5266 | ``			 * `static::` inside the body resolves like php. An explicit`` |
|        - | 5267 | `			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use` |
|        - | 5268 | `			 * the class captured at the closure's creation site. self::/parent::` |
|        - | 5269 | `			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done` |
|        - | 5270 | ``			 * after pSelfHint so a `self`-typed param is unaffected. */`` |
|     4743 | 5271 | `			if( pClosureScope ){` |
|       37 | 5272 | `				pSelf = pClosureScope;` |
|     4725 | 5273 | `			}else if( pVmFunc->pLsbClass ){` |
|       82 | 5274 | `				pSelf = (ph7_class *)pVmFunc->pLsbClass;` |
|       39 | 5275 | `			}` |
|     2369 | 5276 | `		}` |
|  2208567 | 5277 | `		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){` |
|        - | 5278 | `			/* php 8.4 #[\Deprecated] runtime notice — once per call, before` |
|        - | 5279 | `			 * execution (generators: at the g(...) call site, like php). */` |
|      173 | 5280 | `			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,` |
|      112 | 5281 | `				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);` |
|       56 | 5282 | `		}` |
|        - | 5283 | `		/* D1: resolve deferred plain-var arguments against this callee's declaration` |
|        - | 5284 | `		 * now — BEFORE the native/generator splits and VmEnterFrame, while pVm->pFrame is` |
|        - | 5285 | `		 * still the caller. pVmFunc is final here (post-overload). Covers plain functions,` |
|        - | 5286 | `		 * methods, closures, dynamic-name calls, generators and native methods uniformly;` |
|        - | 5287 | `		 * only the SOURCE of the by-ref positions differs between the last one and the` |
|        - | 5288 | `		 * rest (a signature string vs. compiled formal parameters). */` |
|        - | 5289 | `		{` |
|        - | 5290 | `			sxi32 rcDA;` |
|  2208567 | 5291 | `			if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 5292 | `				/* A native method declares no formal parameters to match against —` |
|        - | 5293 | `				 * its by-ref positions come from the same signature-derived mask a` |
|        - | 5294 | `				 * builtin uses, so resolve them the builtin way. Sharing this one` |
|        - | 5295 | `				 * site (rather than repeating it in the branch below) keeps the` |
|        - | 5296 | `				 * throw routing identical for both kinds of callee. */` |
|     8849 | 5297 | `				rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|     5896 | 5298 | `					pVmFunc->pNative->nByRefMask,0,0);` |
|     2953 | 5299 | `			}else{` |
|  3304202 | 5300 | `				rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|  2202666 | 5301 | `					(ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs),SySetUsed(&pVmFunc->aArgs),` |
|        - | 5302 | `					0,0,0);` |
|        - | 5303 | `			}` |
|  2208575 | 5304 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 5305 | `		}` |
|  2208557 | 5306 | `		if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 5307 | `			/* C-bodied method (VM_FUNC_NATIVE): hand it to the foreign-function path.` |
|        - | 5308 | `			 *` |
|        - | 5309 | `			 * Everything a METHOD needs has already happened above — the sVmName` |
|        - | 5310 | `			 * lookup, overload selection, $this / self / late-static-binding` |
|        - | 5311 | `			 * resolution, the visibility check, the #[\Deprecated] notice, deferred` |
|        - | 5312 | `			 * argument materialization — and everything BELOW is exactly what a C` |
|        - | 5313 | `			 * body has no use for: a frame, an operand stack, a call record.` |
|        - | 5314 | `			 *` |
|        - | 5315 | `			 * The stack shape already matches a builtin's, because the method branch` |
|        - | 5316 | `			 * popped both the method-name slot and the target slot: [pArg,pTos) are` |
|        - | 5317 | `			 * this call's arguments and pTos is the slot that receives the result.` |
|        - | 5318 | `			 * So the jump lands on shared code, not a copy of it. */` |
|     5901 | 5319 | `			pFunc = pVmFunc->pNative;` |
|     5901 | 5320 | `			pNativeOwned = pThis; /* borrowed; the shared tail drops this reference */` |
|        - | 5321 | ``			/* A `C::m()` call reaches the method path with no object in the target`` |
|        - | 5322 | `			 * slot, and the path then adopts the CALLER's $this so an instance-context` |
|        - | 5323 | ``			 * `self::m()` still finds one. A native STATIC method must not see that —`` |
|        - | 5324 | `			 * it would read its argument list one slot off — so the receiver handed to` |
|        - | 5325 | `			 * the body is gated on the declaration, not on what the fallback found. */` |
|     5901 | 5326 | `			pNativeRecv = (pVmFunc->iFlags & VM_FUNC_NATIVE_STATIC) ? 0 : pThis;` |
|     5901 | 5327 | `			pNativeClass = pSelf;` |
|     5901 | 5328 | `			goto NativeCall;` |
|        - | 5329 | `		}` |
|  2202661 | 5330 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - | 5331 | `			/* Generator function: return a Generator object instead of executing */` |
|        - | 5332 | `			ph7_exec_ctx *pExecCtx;` |
|        - | 5333 | `			ph7_generator *pGenerator;` |
|        - | 5334 | `			ph7_class_instance *pGenObj;` |
|        - | 5335 | `			ph7_value *pCtxAttr;` |
|        - | 5336 | `			SyString sAttrName;` |
|        - | 5337 | `			ph7_value **apCallArgs;` |
|        - | 5338 | `			int nGenArgs, iArg;` |
|        - | 5339 | `			/* Collect arguments from the operand stack */` |
|      389 | 5340 | `			nGenArgs = (int)(pTos - pArg);` |
|      389 | 5341 | `			apCallArgs = 0;` |
|      389 | 5342 | `			if( nGenArgs > 0 ){` |
|      118 | 5343 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       38 | 5344 | `					nGenArgs * sizeof(ph7_value *));` |
|       80 | 5345 | `				if( apCallArgs == 0 ){` |
|        - | 5346 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 | 5347 | `					nGenArgs = 0;` |
|      ! 0 | 5348 | `				}else{` |
|       80 | 5349 | `					VmCallArgMap *pGenMap = pEffCallMap;` |
|       80 | 5350 | `					int didReorder = 0;` |
|       80 | 5351 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - | 5352 | `						/* Named-argument reordering for generator */` |
|       10 | 5353 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|       10 | 5354 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|       10 | 5355 | `						sxu32 nNV = nF;` |
|       10 | 5356 | `						sxi32 iVIdx = -1;` |
|        - | 5357 | `						sxi32 *aGSlot;` |
|        - | 5358 | `						sxu8 *aGUsed;` |
|        - | 5359 | `						sxu32 gi;` |
|       22 | 5360 | `						for( gi = 0; gi < nF; gi++ ){` |
|       14 | 5361 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        8 | 5362 | `						}` |
|       14 | 5363 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        8 | 5364 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|       10 | 5365 | `						if( aGSlot ){` |
|       10 | 5366 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|       14 | 5367 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        4 | 5368 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|       10 | 5369 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 5370 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 5371 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 5372 | `								goto Abort;` |
|        - | 5373 | `							}` |
|       10 | 5374 | `							if( rc == PH7_EXCEPTION ){` |
|        - | 5375 | `								/* A named-argument error is php's catchable \Error.` |
|        - | 5376 | `								 * No callee frame exists yet on this branch (the` |
|        - | 5377 | `								 * generator body never runs and VmEnterFrame is` |
|        - | 5378 | `								 * further down), so route it like the other` |
|        - | 5379 | `								 * pre-frame OP_CALL throws: drop the args + the` |
|        - | 5380 | `								 * function-name slot and land the enclosing try. */` |
|        3 | 5381 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        3 | 5382 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      195 | 5383 | `								PH7_INLINE_RESUME_BREAK()` |
|        3 | 5384 | `								VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 5385 | `								{` |
|        - | 5386 | `									sxi32 iRpN;` |
|        3 | 5387 | `									if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){` |
|        3 | 5388 | `										pc = iRpN;` |
|        3 | 5389 | `										break;` |
|        - | 5390 | `									}` |
|        - | 5391 | `								}` |
|      ! 0 | 5392 | `								goto Exception;` |
|        - | 5393 | `							}` |
|        - | 5394 | `							{` |
|        - | 5395 | `								/* php's named-hole ArgumentCountError, checked BEFORE` |
|        - | 5396 | `								 * hole compaction: compacting first would report the` |
|        - | 5397 | `								 * positional wording with a fictitious count (g(b:2)` |
|        - | 5398 | ``								 * must be `g(): Argument #1 ($a) not passed`, not`` |
|        - | 5399 | `								 * "1 passed"). Implicit-required watermark, like the` |
|        - | 5400 | `								 * plain-call named path; a hole with NOTHING filled` |
|        - | 5401 | `								 * above it keeps php's count wording — fall through` |
|        - | 5402 | `								 * to VmFiberSetupFrame's check (the compacted count` |
|        - | 5403 | `								 * equals php's num_args there). */` |
|        8 | 5404 | `								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;` |
|        8 | 5405 | `								sxi32 iHole = -1;` |
|        8 | 5406 | `								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);` |
|       18 | 5407 | `								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){` |
|       12 | 5408 | `									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){` |
|       10 | 5409 | `										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);` |
|        4 | 5410 | `									}` |
|        7 | 5411 | `								}` |
|       18 | 5412 | `								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){` |
|        - | 5413 | `									sxu32 gj;` |
|       12 | 5414 | `									int bFound = 0;` |
|       16 | 5415 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       16 | 5416 | `										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }` |
|        3 | 5417 | `									}` |
|       12 | 5418 | `									if( !bFound && gHole + 1 <= nMaxFilledG ){` |
|      ! 0 | 5419 | `										iHole = (sxi32)gHole;` |
|      ! 0 | 5420 | `									}` |
|        7 | 5421 | `								}` |
|        8 | 5422 | `								if( iHole >= 0 ){` |
|      ! 0 | 5423 | `									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 5424 | `										(sxu32)iHole+1,&aFA[iHole].sName);` |
|      ! 0 | 5425 | `									SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 5426 | `									SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 5427 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 5428 | `										goto Abort;` |
|        - | 5429 | `									}` |
|        - | 5430 | `									/* Route like the VmFiberSetupFrame throw below */` |
|      ! 0 | 5431 | `									PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 5432 | `									VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 5433 | `									{` |
|        - | 5434 | `										sxi32 iRpH;` |
|      ! 0 | 5435 | `										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 5436 | `											pc = iRpH;` |
|      ! 0 | 5437 | `											break;` |
|        - | 5438 | `										}` |
|        - | 5439 | `									}` |
|      ! 0 | 5440 | `									goto Exception;` |
|        - | 5441 | `								}` |
|        - | 5442 | `							}` |
|        - | 5443 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - | 5444 | `							 * append overflow (variadic / positional beyond` |
|        - | 5445 | `							 * formals) so downstream sees every argument. */` |
|        - | 5446 | `							{` |
|        8 | 5447 | `								int nOut = 0;` |
|       18 | 5448 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - | 5449 | `									sxu32 gj;` |
|       16 | 5450 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       16 | 5451 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|       12 | 5452 | `											apCallArgs[nOut++] = &pArg[gj];` |
|       12 | 5453 | `											break;` |
|        - | 5454 | `										}` |
|        3 | 5455 | `									}` |
|        7 | 5456 | `								}` |
|       18 | 5457 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|       12 | 5458 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 | 5459 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 | 5460 | `									}` |
|        7 | 5461 | `								}` |
|        8 | 5462 | `								nGenArgs = nOut;` |
|        - | 5463 | `							}` |
|        8 | 5464 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        8 | 5465 | `							didReorder = 1;` |
|        3 | 5466 | `						}` |
|        - | 5467 | `						/* If aGSlot allocation failed, fall through to` |
|        - | 5468 | `						 * positional fill below — preserves arg order rather` |
|        - | 5469 | `						 * than passing an uninitialized apCallArgs. */` |
|        3 | 5470 | `					}` |
|       78 | 5471 | `					if( !didReorder ){` |
|      146 | 5472 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|       78 | 5473 | `							apCallArgs[iArg] = &pArg[iArg];` |
|       41 | 5474 | `						}` |
|       34 | 5475 | `					}` |
|        - | 5476 | `				}` |
|       37 | 5477 | `			}` |
|        - | 5478 | `			/* Create execution context and generator wrapper */` |
|      387 | 5479 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|      387 | 5480 | `			if( pExecCtx == 0 ){` |
|      ! 0 | 5481 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 5482 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 5483 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 5484 | `				break;` |
|        - | 5485 | `			}` |
|      387 | 5486 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|      387 | 5487 | `			if( pGenerator == 0 ){` |
|      ! 0 | 5488 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 | 5489 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 5490 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 5491 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 5492 | `				break;` |
|        - | 5493 | `			}` |
|        - | 5494 | `			/* Set up the frame with arguments, closure env, $this */` |
|      387 | 5495 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|      387 | 5496 | `			pVm->pFrame = pExecCtx->pFrame;` |
|      769 | 5497 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,` |
|      382 | 5498 | `				pEffCallMap ? (pEffCallMap->bStrict ? 1 : 0) : (pVm->bCurStrict ? 1 : 0),` |
|      191 | 5499 | `				pSelfHint,` |
|        - | 5500 | `				TRUE/*generator: the g(...) call site is in the message*/);` |
|      387 | 5501 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|      387 | 5502 | `			pExecCtx->pFrame->pParent = 0;` |
|      387 | 5503 | `			if( apCallArgs ){` |
|       78 | 5504 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       37 | 5505 | `			}` |
|      387 | 5506 | `			if( rc != SXRET_OK ){` |
|       18 | 5507 | `				VmReleaseGenerator(pVm, pGenerator);` |
|       18 | 5508 | `				if( pThis ){` |
|        3 | 5509 | `					PH7_ClassInstanceUnref(pThis);` |
|        1 | 5510 | `				}` |
|       18 | 5511 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 5512 | `					goto Abort;` |
|        - | 5513 | `				}` |
|       18 | 5514 | `				if( rc == PH7_EXCEPTION ){` |
|        - | 5515 | `					/* A declared-type TypeError thrown while binding the` |
|        - | 5516 | `					 * generator's arguments (php binds + type-checks eagerly at` |
|        - | 5517 | `					 * the g(...) call site, before any resume — band A #2). If` |
|        - | 5518 | `					 * an inline try THIS exec owns caught it, land at its` |
|        - | 5519 | `					 * redirect (the drain subsumes the operand pops); else pop` |
|        - | 5520 | `					 * the args + function name and route like the other` |
|        - | 5521 | `					 * OP_CALL throw paths. */` |
|       22 | 5522 | `					PH7_INLINE_RESUME_BREAK()` |
|       16 | 5523 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 5524 | `					{` |
|        - | 5525 | `						sxi32 iRpG;` |
|       16 | 5526 | `						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){` |
|       16 | 5527 | `							pc = iRpG;` |
|       16 | 5528 | `							break;` |
|        - | 5529 | `						}` |
|        - | 5530 | `					}` |
|      ! 0 | 5531 | `					goto Exception;` |
|        - | 5532 | `				}` |
|      ! 0 | 5533 | `				break;` |
|        - | 5534 | `			}` |
|        - | 5535 | `			/* Create Generator class instance */` |
|      371 | 5536 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|      371 | 5537 | `			if( pGenObj == 0 ){` |
|      ! 0 | 5538 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 | 5539 | `				break;` |
|        - | 5540 | `			}` |
|        - | 5541 | `			/* Store generator in __ctx attribute */` |
|      371 | 5542 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      371 | 5543 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|      371 | 5544 | `			if( pCtxAttr ){` |
|      371 | 5545 | `				pCtxAttr->x.pOther = pGenerator;` |
|      371 | 5546 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      183 | 5547 | `			}` |
|        - | 5548 | `			/* Pop args and function name, push Generator object. PH7_NewClassInstance` |
|        - | 5549 | `			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do` |
|        - | 5550 | `			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope` |
|        - | 5551 | ``			 * and its __destruct (which runs pending `finally` blocks and frees the`` |
|        - | 5552 | `			 * exec context) never fires. */` |
|      371 | 5553 | `			PH7_MemObjRelease(pTos);` |
|      371 | 5554 | `			pTos = &pTos[-nCallArgs];` |
|      371 | 5555 | `			pTos->x.pOther = pGenObj;` |
|      371 | 5556 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|      371 | 5557 | `			if( pThis ){` |
|       32 | 5558 | `				PH7_ClassInstanceUnref(pThis);` |
|       14 | 5559 | `			}` |
|      371 | 5560 | `			break;` |
|        - | 5561 | `		}` |
|        - | 5562 | `		/* Extract the formal argument set */` |
|  2202277 | 5563 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - | 5564 | `		/* Create a new VM frame  */` |
|  2202277 | 5565 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|  2202277 | 5566 | `		if( rc != SXRET_OK ){` |
|        - | 5567 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 5568 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 5569 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 5570 | `				&pVmFunc->sName);` |
|        - | 5571 | `			/* The frame that would own (and later release) $this never got created; for a bound` |
|        - | 5572 | `			 * plain closure the consumed transient is the object's ONLY ref, so release it here` |
|        - | 5573 | `			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */` |
|      ! 0 | 5574 | `			if( bClosureThis && pThis ){` |
|      ! 0 | 5575 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 | 5576 | `			}` |
|        - | 5577 | `			/* Pop given arguments */` |
|      ! 0 | 5578 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 5579 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 5580 | `			}` |
|        - | 5581 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 | 5582 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 5583 | `			break;` |
|        - | 5584 | `		}` |
|  2202277 | 5585 | `		if( pClosureScope ){` |
|        - | 5586 | `			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /` |
|        - | 5587 | `			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected` |
|        - | 5588 | `			 * access inside the body resolves against it. */` |
|       35 | 5589 | `			pFrame->pBoundScope = pClosureScope;` |
|       17 | 5590 | `		}` |
|        - | 5591 | `		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band` |
|        - | 5592 | `		 * A #4): sArg over-counts (defaulted params installed, variadic packed` |
|        - | 5593 | `		 * as one entry) so php's answers can't be derived from it. */` |
|  2202277 | 5594 | `		pFrame->nActualArgs = (int)(pTos - pArg);` |
|  2202277 | 5595 | `		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) \|\| bClosureThis) ){` |
|        - | 5596 | `			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */` |
|        - | 5597 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|  1870593 | 5598 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|  1870593 | 5599 | `			if( pObj ){` |
|        - | 5600 | `				/* Reflect the change */` |
|  1870593 | 5601 | `				pObj->x.pOther = pThis;` |
|  1870593 | 5602 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   935294 | 5603 | `			}` |
|   935294 | 5604 | `		}` |
|  2202277 | 5605 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - | 5606 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - | 5607 | `			/* Install static variables */` |
|     1283 | 5608 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|     2561 | 5609 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|     1283 | 5610 | `				pStatic = &aStatic[n];` |
|     1283 | 5611 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - | 5612 | `					/* Initialize the static variables */` |
|       53 | 5613 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|       53 | 5614 | `					if( pObj ){` |
|        - | 5615 | `						/* Assume a NULL initialization value */` |
|       53 | 5616 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|       53 | 5617 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - | 5618 | `							/* Evaluate initialization expression (Any complex expression) */` |
|       53 | 5619 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|       24 | 5620 | `						}` |
|       53 | 5621 | `						pObj->nIdx = pStatic->nIdx;` |
|       29 | 5622 | `					}else{` |
|      ! 0 | 5623 | `						continue;` |
|        - | 5624 | `					}` |
|       24 | 5625 | `				}` |
|        - | 5626 | `				/* Install in the current frame */` |
|     1922 | 5627 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|     1278 | 5628 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      644 | 5629 | `			}` |
|      639 | 5630 | `		}` |
|        - | 5631 | `		/* Push arguments in the local frame */` |
|        - | 5632 | `		{` |
|  2202277 | 5633 | `		VmCallArgMap *pCallMap3 = pEffCallMap;` |
|        - | 5634 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - | 5635 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|        - | 5636 | `		/* A compiled call in a strict file always carries a map with bStrict set` |
|        - | 5637 | `		 * (GenStateAttachStrictFlag); with no map at all the call is either a` |
|        - | 5638 | `		 * compiled WEAK one — where bCurStrict is 0 for the same unit — or an` |
|        - | 5639 | `		 * ENGINE-dispatched one, whose synthetic OP_CALL has no map to carry the` |
|        - | 5640 | `		 * caller's mode. php scopes parameter coercion by the CALLING file either` |
|        - | 5641 | `		 * way, and bCurStrict is that file's mode. */` |
|  2202503 | 5642 | `		int bCallIsStrict = pCallMap3 ? (pCallMap3->bStrict ? 1 : 0)` |
|  2202046 | 5643 | `		                              : (pVm->bCurStrict ? 1 : 0);` |
|  2202277 | 5644 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - | 5645 | `			/* ============================================================` |
|        - | 5646 | `			 * Named-argument matching path (PHP 8.0)` |
|        - | 5647 | `			 *` |
|        - | 5648 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - | 5649 | `			 * or position, then install them in the frame.` |
|        - | 5650 | `			 * ============================================================ */` |
|      357 | 5651 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      357 | 5652 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|      357 | 5653 | `			sxi32 iVariadicIdx = -1;` |
|        - | 5654 | `			sxu32 nNonVariadic;` |
|        - | 5655 | `			sxi32 *aSlot;` |
|        - | 5656 | `			sxu8  *aUsed;` |
|        - | 5657 | `			sxu32 i;` |
|        - | 5658 | `			/* Find variadic parameter index */` |
|      927 | 5659 | `			for( i = 0; i < nFormal; i++ ){` |
|      675 | 5660 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|      104 | 5661 | `					iVariadicIdx = (sxi32)i;` |
|      104 | 5662 | `					break;` |
|        - | 5663 | `				}` |
|      290 | 5664 | `			}` |
|      357 | 5665 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - | 5666 | `			/* Allocate mapping arrays */` |
|      533 | 5667 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      352 | 5668 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|      357 | 5669 | `			if( aSlot == 0 ){` |
|      ! 0 | 5670 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 | 5671 | `				goto Abort;` |
|        - | 5672 | `			}` |
|      357 | 5673 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - | 5674 | `			/* Resolve named arguments to formal parameters */` |
|      533 | 5675 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|      176 | 5676 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|      357 | 5677 | `			if( rc == PH7_ABORT ){` |
|        8 | 5678 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 | 5679 | `				goto Abort;` |
|        - | 5680 | `			}` |
|      350 | 5681 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 5682 | `				/* php's catchable \Error for a bad named argument. The callee` |
|        - | 5683 | `				 * frame is already entered but its body must not run: unwind` |
|        - | 5684 | `				 * exactly like the named-hole ArgumentCountError path below` |
|        - | 5685 | `				 * (release the not-yet-installed actuals — Pass 2's release` |
|        - | 5686 | `				 * loop has not run — pop the callee slot, mark that there is no` |
|        - | 5687 | `				 * callee operand stack, and let VmCallFinish route the throw). */` |
|        - | 5688 | `				sxu32 iRel;` |
|        5 | 5689 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       11 | 5690 | `				for( iRel = 0; iRel < nActual; iRel++ ){` |
|        7 | 5691 | `					PH7_MemObjRelease(&pArg[iRel]);` |
|        4 | 5692 | `				}` |
|        5 | 5693 | `				PH7_MemObjRelease(pTos);` |
|        5 | 5694 | `				pTos = &pTos[-nCallArgs];` |
|        5 | 5695 | `				pFrameStack = 0;` |
|        5 | 5696 | `				goto SkipFuncBody;` |
|        - | 5697 | `			}` |
|        - | 5698 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|        - | 5699 | `			{` |
|        - | 5700 | `			/* php's required watermark for the hole check below, plus the` |
|        - | 5701 | `			 * highest formal slot an actual resolved to: php words a hole` |
|        - | 5702 | ``			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole`` |
|        - | 5703 | `			 * with nothing filled above it gets the positional count message` |
|        - | 5704 | `			 * (zend's RECV arg_num > EX(num_args) distinction). */` |
|        - | 5705 | `			sxu32 nReqNamed;` |
|        - | 5706 | `			sxu32 nNVNamed;` |
|      346 | 5707 | `			sxu32 nMaxFilled = 0;` |
|      346 | 5708 | `			nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);` |
|     1246 | 5709 | `			for( i = 0; i < nActual; i++ ){` |
|      904 | 5710 | `				if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|      370 | 5711 | `					nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|      183 | 5712 | `				}` |
|      454 | 5713 | `			}` |
|      888 | 5714 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - | 5715 | `				/* Find the stack arg mapped to formal n */` |
|      556 | 5716 | `				sxi32 iSrc = -1;` |
|      892 | 5717 | `				for( i = 0; i < nActual; i++ ){` |
|      780 | 5718 | `					if( aSlot[i] == (sxi32)n ){` |
|      444 | 5719 | `						iSrc = (sxi32)i;` |
|      444 | 5720 | `						break;` |
|        - | 5721 | `					}` |
|      171 | 5722 | `				}` |
|      556 | 5723 | `				if( iSrc >= 0 ){` |
|        - | 5724 | `					/* Argument was provided — install with type checking */` |
|      444 | 5725 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - | 5726 | `					/* An explicit null is NOT redirected to the default: PHP applies a` |
|        - | 5727 | `					 * default only for an OMITTED argument. An explicit null falls through` |
|        - | 5728 | `					 * to the type check below (TypeError for a non-nullable typed param,` |
|        - | 5729 | ``					 * kept as null for a typeless one). An implicitly-nullable `Type $x =`` |
|        - | 5730 | ``					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */`` |
|        - | 5731 | `					/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 5732 | `					 * coercion and whole-real materialization in place): the shared` |
|        - | 5733 | `					 * per-argument helper — one implementation for both OP_CALL` |
|        - | 5734 | `					 * paths and the generator/fiber binder (§7.1(f) fold). */` |
|      444 | 5735 | `					rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pVal,bCallIsStrict,pSelfHint);` |
|      444 | 5736 | `					if( rc != SXRET_OK ){` |
|        7 | 5737 | `						if( rc == PH7_ABORT ) goto Abort;` |
|        7 | 5738 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 | 5739 | `						PH7_MemObjRelease(pTos);` |
|        7 | 5740 | `						pTos = &pTos[-nCallArgs];` |
|        7 | 5741 | `						pFrameStack = 0;` |
|        7 | 5742 | `						rc = PH7_EXCEPTION;` |
|        9 | 5743 | `						goto SkipFuncBody;` |
|        - | 5744 | `					}` |
|        - | 5745 | `					/* Install: by reference or by value */` |
|      438 | 5746 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 | 5747 | `						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){` |
|        - | 5748 | `							/* php 8.1: $GLOBALS cannot be passed by reference */` |
|        - | 5749 | `							SyBlob sMsg;` |
|      ! 0 | 5750 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 5751 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 5752 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 5753 | `							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|      ! 0 | 5754 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 5755 | `								goto Abort;` |
|        - | 5756 | `							}` |
|      ! 0 | 5757 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 5758 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 5759 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 5760 | `							pFrameStack = 0;` |
|      ! 0 | 5761 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 5762 | `							goto SkipFuncBody;` |
|        - | 5763 | `						}` |
|        5 | 5764 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 | 5765 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|      ! 0 | 5766 | `							 && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){` |
|        - | 5767 | `								/* A non-lvalue bound to a by-ref parameter is a catchable Error in` |
|        - | 5768 | `								 * php — f(5) where f(&$x). PH7 only warned and quietly passed by` |
|        - | 5769 | `								 * value, so the call ran with a copy and the caller never knew.` |
|        - | 5770 | `								 * The one legitimate copy is call_user_func()'s (MEMOBJ_AUX_CUFVAL),` |
|        - | 5771 | `								 * which php also permits, with its own warning. */` |
|        - | 5772 | `								SyBlob sMsg;` |
|        - | 5773 | `								sxi32 rcRef;` |
|      ! 0 | 5774 | `								SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 5775 | `								SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 5776 | `									&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 5777 | `								rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|      ! 0 | 5778 | `									SyBlobLength(&sMsg));` |
|      ! 0 | 5779 | `								SyBlobRelease(&sMsg);` |
|      ! 0 | 5780 | `								if( rcRef == SXERR_ABORT ){` |
|      ! 0 | 5781 | `									pFrameStack = 0;` |
|      ! 0 | 5782 | `									rc = PH7_ABORT;` |
|      ! 0 | 5783 | `									goto SkipFuncBody;` |
|        - | 5784 | `								}` |
|      ! 0 | 5785 | `								pFrameStack = 0;` |
|      ! 0 | 5786 | `								rc = PH7_EXCEPTION;` |
|      ! 0 | 5787 | `								goto SkipFuncBody;` |
|        - | 5788 | `							}` |
|      ! 0 | 5789 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 5790 | `						}else{` |
|        7 | 5791 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 | 5792 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 | 5793 | `							if( pRefEntry == 0 ){` |
|        7 | 5794 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 | 5795 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 | 5796 | `								sArg.nIdx = pVal->nIdx;` |
|        5 | 5797 | `								sArg.pUserData = 0;` |
|        5 | 5798 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 | 5799 | `							}` |
|        5 | 5800 | `							pObj = 0;` |
|        - | 5801 | `						}` |
|        3 | 5802 | `					}else{` |
|      434 | 5803 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 5804 | `					}` |
|      438 | 5805 | `					if( pObj ){` |
|      434 | 5806 | `						PH7_MemObjStore(pVal,pObj);` |
|      434 | 5807 | `						sArg.nIdx = pObj->nIdx;` |
|      434 | 5808 | `						sArg.pUserData = 0;` |
|      434 | 5809 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      215 | 5810 | `					}` |
|      221 | 5811 | `				}else{` |
|        - | 5812 | `					/* Argument was NOT provided — use default or leave unset */` |
|      115 | 5813 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 5814 | `						/* Should not reach here; variadic handled separately below */` |
|      115 | 5815 | `					}else if( n < nReqNamed ){` |
|        - | 5816 | `						/* php's implicit-required rule applies to named calls` |
|        - | 5817 | `						 * too: a hole below the required watermark throws even` |
|        - | 5818 | `						 * when the formal carries a default — f($a,$b=2,$c)` |
|        - | 5819 | ``						 * called as f(a:1,c:3) is `Argument #2 ($b) not`` |
|        - | 5820 | ``						 * passed` (a hole with NO default is always below the`` |
|        - | 5821 | `						 * watermark, so this subsumes the no-default case).` |
|        - | 5822 | `						 * A hole with nothing filled ABOVE it uses php's` |
|        - | 5823 | `						 * positional count wording instead. The passed stack` |
|        - | 5824 | `						 * args were not released yet on this path (that loop` |
|        - | 5825 | `						 * runs after Pass 2) — release them before the exit. */` |
|        5 | 5826 | `						if( n + 1 > nMaxFilled ){` |
|        3 | 5827 | `							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)` |
|      ! 0 | 5828 | `								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 5829 | `									nMaxFilled,nReqNamed,SySetUsed(&pVmFunc->aArgs))` |
|        3 | 5830 | `								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        1 | 5831 | `									nMaxFilled,nReqNamed,nNVNamed,TRUE);` |
|        2 | 5832 | `						}else{` |
|        3 | 5833 | `							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        - | 5834 | `						}` |
|        5 | 5835 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        9 | 5836 | `						for( i = 0; i < nActual; i++ ){` |
|        5 | 5837 | `							PH7_MemObjRelease(&pArg[i]);` |
|        3 | 5838 | `						}` |
|        5 | 5839 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 5840 | `							goto Abort;` |
|        - | 5841 | `						}` |
|        5 | 5842 | `						PH7_MemObjRelease(pTos);` |
|        5 | 5843 | `						pTos = &pTos[-nCallArgs];` |
|        5 | 5844 | `						pFrameStack = 0;` |
|        5 | 5845 | `						rc = PH7_EXCEPTION;` |
|        5 | 5846 | `						goto SkipFuncBody;` |
|      111 | 5847 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      111 | 5848 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      111 | 5849 | `						if( pObj ){` |
|      111 | 5850 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|      111 | 5851 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      111 | 5852 | `							sArg.nIdx = pObj->nIdx;` |
|      111 | 5853 | `							sArg.pUserData = 0;` |
|      111 | 5854 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 5855 | `							/* A null default on an implicitly-nullable param must stay null` |
|        - | 5856 | `							 * (see the positional-path note above). */` |
|      108 | 5857 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|       42 | 5858 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0` |
|       24 | 5859 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 5860 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 5861 | `								if( xCast ) xCast(pObj);` |
|      ! 0 | 5862 | `							}else{` |
|        - | 5863 | `								/* Mask matched — a const-indirected whole-real default` |
|        - | 5864 | ``								 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|      111 | 5865 | `								VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 5866 | `							}` |
|       54 | 5867 | `						}` |
|       54 | 5868 | `					}` |
|        - | 5869 | `				}` |
|      275 | 5870 | `			}` |
|        - | 5871 | `			} /* end nReqNamed scope */` |
|        - | 5872 | `			/* Handle variadic parameter */` |
|      336 | 5873 | `			if( iVariadicIdx >= 0 ){` |
|      104 | 5874 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|      104 | 5875 | `				if( pObj ){` |
|        - | 5876 | `					/* Capture the slot index now: PH7_HashmapInsert below can` |
|        - | 5877 | `					 * PH7_ReserveMemObj, reallocating pVm->aMemObj and dangling pObj` |
|        - | 5878 | `					 * (same latent UAF the positional path guards against). */` |
|        - | 5879 | `					sxu32 nVariadicSlot;` |
|      104 | 5880 | `					PH7_MemObjToHashmap(pObj);` |
|      104 | 5881 | `					nVariadicSlot = pObj->nIdx;` |
|        - | 5882 | `					{` |
|      104 | 5883 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - | 5884 | `						/* php numbers a failing NAMED variadic element as` |
|        - | 5885 | `						 * max(total positional args, declared non-variadic` |
|        - | 5886 | `						 * formals) + 1 — zend's RECV slots always count —` |
|        - | 5887 | `						 * whichever named element fails; a POSITIONAL element` |
|        - | 5888 | `						 * uses its own 1-based call position. */` |
|      104 | 5889 | `						sxu32 nPositional = 0;` |
|      610 | 5890 | `						for( i = 0; i < nActual; i++ ){` |
|      510 | 5891 | `							if( !(i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0) ){` |
|      333 | 5892 | `								nPositional++;` |
|      165 | 5893 | `							}` |
|      257 | 5894 | `						}` |
|      568 | 5895 | `						for( i = 0; i < nActual; i++ ){` |
|      504 | 5896 | `							if( aSlot[i] == -1 ){` |
|      458 | 5897 | `								int bNamed = (i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0);` |
|        - | 5898 | `								/* Same per-element type check + weak coercion as the` |
|        - | 5899 | ``								 * positional-only path (shared helper; no `($name)`). */`` |
|      685 | 5900 | `								rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|      454 | 5901 | `									&aFormalArg[iVariadicIdx],&pArg[i],` |
|      301 | 5902 | `									bNamed ? SXMAX(nPositional,nNonVariadic) + 1 : i + 1,bCallIsStrict);` |
|      458 | 5903 | `								if( rc != SXRET_OK ){` |
|       39 | 5904 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 5905 | `										goto Abort;` |
|        - | 5906 | `									}` |
|       39 | 5907 | `									SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       39 | 5908 | `									PH7_MemObjRelease(pTos);` |
|       39 | 5909 | `									pTos = &pTos[-nCallArgs];` |
|       39 | 5910 | `									pFrameStack = 0;` |
|       39 | 5911 | `									rc = PH7_EXCEPTION;` |
|       39 | 5912 | `									goto SkipFuncBody;` |
|        - | 5913 | `								}` |
|      422 | 5914 | `								if( bNamed ){` |
|        - | 5915 | `									/* Named variadic entry: insert with string key */` |
|        - | 5916 | `									ph7_value sKey;` |
|      120 | 5917 | `									PH7_MemObjInit(pVm, &sKey);` |
|      120 | 5918 | `									PH7_MemObjStringAppend(&sKey,` |
|      116 | 5919 | `										pCallMap3->aNames[i].zString,` |
|      116 | 5920 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|      120 | 5921 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|      120 | 5922 | `									PH7_MemObjRelease(&sKey);` |
|       62 | 5923 | `								}else{` |
|        - | 5924 | `									/* Positional variadic entry */` |
|      305 | 5925 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - | 5926 | `								}` |
|      209 | 5927 | `							}` |
|      236 | 5928 | `						}` |
|        - | 5929 | `					}` |
|       68 | 5930 | `					sArg.nIdx = nVariadicSlot; /* pObj may be stale here (aMemObj realloc) */` |
|       68 | 5931 | `					sArg.pUserData = 0;` |
|       68 | 5932 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       32 | 5933 | `				}` |
|       36 | 5934 | `			}else{` |
|        - | 5935 | `				/* No variadic — preserve unresolved positional overflow` |
|        - | 5936 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - | 5937 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - | 5938 | `				 * the positional-only path's behavior. */` |
|      234 | 5939 | `				sxu32 nAnon = nNonVariadic;` |
|      618 | 5940 | `				for( i = 0; i < nActual; i++ ){` |
|      386 | 5941 | `					if( aSlot[i] == -2 ){` |
|        - | 5942 | `						char zAnonBuf[32];` |
|        - | 5943 | `						SyString sAnonName;` |
|      ! 0 | 5944 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 | 5945 | `							"[%u]apArg",nAnon);` |
|      ! 0 | 5946 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 | 5947 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 | 5948 | `						if( pObj ){` |
|      ! 0 | 5949 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 | 5950 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 | 5951 | `							sArg.pUserData = 0;` |
|      ! 0 | 5952 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 | 5953 | `						}` |
|      ! 0 | 5954 | `						nAnon++;` |
|      ! 0 | 5955 | `					}` |
|      194 | 5956 | `				}` |
|        - | 5957 | `			}` |
|        - | 5958 | `			/* Release all stack arguments */` |
|     1118 | 5959 | `			for( i = 0; i < nActual; i++ ){` |
|      822 | 5960 | `				PH7_MemObjRelease(&pArg[i]);` |
|      413 | 5961 | `			}` |
|      300 | 5962 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - | 5963 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|      300 | 5964 | `			n = nFormal;` |
|      152 | 5965 | `		}else{` |
|        - | 5966 | `		/* ============================================================` |
|        - | 5967 | `		 * Positional-only matching path (original)` |
|        - | 5968 | `		 * ============================================================ */` |
|        - | 5969 | `		/* Base of the actual argument stack, for php's per-ELEMENT argument` |
|        - | 5970 | `		 * number in a variadic type-error (php numbers a variadic-collected` |
|        - | 5971 | `		 * element by its overall 1-based call position, not the formal index). */` |
|  2201925 | 5972 | `		ph7_value *pArgBase = pArg;` |
|  2201925 | 5973 | `		n = 0;` |
|  3877037 | 5974 | `		while( pArg < pTos ){` |
|  1675793 | 5975 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - | 5976 | `				/* Variadic parameter: collect all remaining args into an array */` |
|      413 | 5977 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      413 | 5978 | `				if( pObj ){` |
|        - | 5979 | `					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,` |
|        - | 5980 | `					 * reallocating pVm->aMemObj and dangling pObj — so don't read pObj->nIdx after` |
|        - | 5981 | `					 * the packing loop (pre-existing UAF, masked by the pool allocator). pMap is a` |
|        - | 5982 | `					 * separately-allocated hashmap and stays valid across the realloc. */` |
|        - | 5983 | `					sxu32 nVariadicIdx;` |
|        - | 5984 | `					/* Initialize as empty array */` |
|      413 | 5985 | `					PH7_MemObjToHashmap(pObj);` |
|      413 | 5986 | `					nVariadicIdx = pObj->nIdx;` |
|        - | 5987 | `					{` |
|      413 | 5988 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     2229 | 5989 | `						while( pArg < pTos ){` |
|        - | 5990 | `							/* Per-element type check + weak coercion (shared helper,` |
|        - | 5991 | `							 * also used by the named-argument path). The argument` |
|        - | 5992 | `							 * number is the element's overall 1-based call position` |
|        - | 5993 | `` 							 * ((pArg - pArgBase) + 1) and, like php, the `($name)` `` |
|        - | 5994 | `							 * clause is omitted. */` |
|     2789 | 5995 | `							rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|     1856 | 5996 | `								&aFormalArg[n],pArg,(sxu32)(pArg - pArgBase) + 1,bCallIsStrict);` |
|     1861 | 5997 | `							if( rc != SXRET_OK ){` |
|       44 | 5998 | `								if( rc == PH7_ABORT ){` |
|      ! 0 | 5999 | `									goto Abort;` |
|        - | 6000 | `								}` |
|        - | 6001 | `								/* Skip function body, route through normal cleanup */` |
|       44 | 6002 | `								PH7_MemObjRelease(pTos);` |
|       44 | 6003 | `								pTos = &pTos[-nCallArgs];` |
|       44 | 6004 | `								pFrameStack = 0;` |
|       44 | 6005 | `								rc = PH7_EXCEPTION;` |
|       44 | 6006 | `								goto SkipFuncBody;` |
|        - | 6007 | `							}` |
|     1821 | 6008 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|     1821 | 6009 | `							pArg++;` |
|        5 | 6010 | `						}` |
|        - | 6011 | `					}` |
|      373 | 6012 | `					sArg.nIdx = nVariadicIdx; /* pObj may be stale here (aMemObj realloc) — use the saved index */` |
|      373 | 6013 | `					sArg.pUserData = 0;` |
|      373 | 6014 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      184 | 6015 | `				}` |
|      373 | 6016 | `				break; /* All remaining args consumed */` |
|        - | 6017 | `			}` |
|  1675385 | 6018 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 6019 | `				/* An explicit null is NOT redirected to the default (PHP applies a` |
|        - | 6020 | `				 * default only for an omitted arg); it falls through to the type check` |
|        - | 6021 | `				 * below — TypeError for a non-nullable typed param, kept as null for a` |
|        - | 6022 | ``				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE`` |
|        - | 6023 | `				 * at compile time so its check accepts null. */` |
|        - | 6024 | `				/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 6025 | `				 * coercion and whole-real materialization in place — nullable` |
|        - | 6026 | `				 * types (?type) let null through): the shared per-argument` |
|        - | 6027 | `				 * helper, one implementation for both OP_CALL paths and the` |
|        - | 6028 | `				 * generator/fiber binder (§7.1(f) fold). */` |
|  1674317 | 6029 | `				rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pArg,bCallIsStrict,pSelfHint);` |
|  1674317 | 6030 | `				if( rc != SXRET_OK ){` |
|      271 | 6031 | `					if( rc == PH7_ABORT ){` |
|        6 | 6032 | `						goto Abort;` |
|        - | 6033 | `					}` |
|        - | 6034 | `					/* Skip function body, route through normal cleanup */` |
|      267 | 6035 | `					PH7_MemObjRelease(pTos);` |
|      267 | 6036 | `					pTos = &pTos[-nCallArgs];` |
|      267 | 6037 | `					pFrameStack = 0;` |
|      267 | 6038 | `					rc = PH7_EXCEPTION;` |
|      267 | 6039 | `					goto SkipFuncBody;` |
|        - | 6040 | `				}` |
|  1674051 | 6041 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 6042 | `					/* Pass by reference */` |
|     2569 | 6043 | `					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){` |
|        - | 6044 | `						/* php 8.1: $GLOBALS cannot be passed by reference —` |
|        - | 6045 | `						 * a catchable Error with php's exact wording. */` |
|        - | 6046 | `						SyBlob sMsg;` |
|        3 | 6047 | `						SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 6048 | `						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|        2 | 6049 | `							&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        3 | 6050 | `						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 6051 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 6052 | `							goto Abort;` |
|        - | 6053 | `						}` |
|        3 | 6054 | `						PH7_MemObjRelease(pTos);` |
|        3 | 6055 | `						pTos = &pTos[-nCallArgs];` |
|        3 | 6056 | `						pFrameStack = 0;` |
|        3 | 6057 | `						rc = PH7_EXCEPTION;` |
|        3 | 6058 | `						goto SkipFuncBody;` |
|        - | 6059 | `					}` |
|     2567 | 6060 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        2 | 6061 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|        3 | 6062 | `						 && (pArg->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){` |
|        - | 6063 | `							/* php: a non-lvalue bound to a by-ref parameter is a catchable Error.` |
|        - | 6064 | `							 * PH7 warned and silently passed by value (same site as the other` |
|        - | 6065 | `							 * binder above). call_user_func()'s deliberate copy is exempt. */` |
|        - | 6066 | `							SyBlob sMsg;` |
|        - | 6067 | `							sxi32 rcRef;` |
|      ! 0 | 6068 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 6069 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 6070 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 6071 | `							rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|      ! 0 | 6072 | `								SyBlobLength(&sMsg));` |
|      ! 0 | 6073 | `							SyBlobRelease(&sMsg);` |
|      ! 0 | 6074 | `							return (rcRef == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 6075 | `						}` |
|        - | 6076 | `						/* Switch to pass by value */` |
|        3 | 6077 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        2 | 6078 | `					}else{` |
|        - | 6079 | `						SyHashEntry *pRefEntry;` |
|        - | 6080 | `						/* Install the referenced variable in the private function frame */` |
|     2565 | 6081 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|     2565 | 6082 | `						if( pRefEntry == 0 ){` |
|     3845 | 6083 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|     2560 | 6084 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|     2565 | 6085 | `							sArg.nIdx = pArg->nIdx;` |
|     2565 | 6086 | `							sArg.pUserData = 0;` |
|     2565 | 6087 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     1280 | 6088 | `						}` |
|     2565 | 6089 | `						pObj = 0;` |
|        - | 6090 | `					}` |
|     1286 | 6091 | `				}else{` |
|        - | 6092 | `					/* Pass by value,make a copy of the given argument */` |
|  1671487 | 6093 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 6094 | `				}` |
|   837423 | 6095 | `			}else{` |
|        - | 6096 | `				char zName[32];` |
|        - | 6097 | `				SyString sArgName;` |
|        - | 6098 | `				/* Set a dummy name */` |
|     1073 | 6099 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|     1073 | 6100 | `				sArgName.zString = zName;` |
|        - | 6101 | `				/* Annonymous argument */` |
|     1073 | 6102 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 6103 | `			}` |
|  1675117 | 6104 | `			if( pObj ){` |
|  1672557 | 6105 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 6106 | `				/* Insert argument index  */` |
|  1672557 | 6107 | `				sArg.nIdx = pObj->nIdx;` |
|  1672557 | 6108 | `				sArg.pUserData = 0;` |
|  1672557 | 6109 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|   836672 | 6110 | `			}` |
|  1675117 | 6111 | `			PH7_MemObjRelease(pArg);` |
|  1675117 | 6112 | `			pArg++;` |
|  1675117 | 6113 | `			++n;` |
|        5 | 6114 | `		}` |
|        - | 6115 | `		} /* end named vs positional branch */` |
|        - | 6116 | `		/* Set up closure environment */` |
|  2201913 | 6117 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 6118 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 6119 | `			ph7_value *pValue;` |
|        - | 6120 | `			sxu32 iEnv;` |
|     4693 | 6121 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|    10151 | 6122 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|     5463 | 6123 | `				pEnv = &aEnv[iEnv];` |
|     5463 | 6124 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 6125 | `					/* Do not install null value */` |
|     4589 | 6126 | `					continue;` |
|        - | 6127 | `				}` |
|      874 | 6128 | `				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       13 | 6129 | `				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|        - | 6130 | `					/* The Closure instance carries an explicit bound $this` |
|        - | 6131 | `					 * (bindTo/bind/call): it wins over the creation-time` |
|        - | 6132 | `					 * captured $this, php-exact. */` |
|        7 | 6133 | `					continue;` |
|        - | 6134 | `				}` |
|      873 | 6135 | `				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|        - | 6136 | `					/* Captured by reference: link the name to the shared slot` |
|        - | 6137 | `					 * (no copy), mirroring the by-ref argument install above. */` |
|      220 | 6138 | `					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|      329 | 6139 | `						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),` |
|      218 | 6140 | `							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|      109 | 6141 | `					}` |
|      220 | 6142 | `					continue;` |
|        - | 6143 | `				}` |
|      655 | 6144 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|      655 | 6145 | `				if( pValue == 0 ){` |
|      ! 0 | 6146 | `					continue;` |
|        - | 6147 | `				}` |
|        - | 6148 | `				/* Invalidate any prior representation */` |
|      655 | 6149 | `				PH7_MemObjRelease(pValue);` |
|        - | 6150 | `				/* Duplicate bound variable value */` |
|      655 | 6151 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|      330 | 6152 | `			}` |
|     2344 | 6153 | `		}` |
|        - | 6154 | `		/* Too-few-arguments check, placed AFTER the passed arguments were` |
|        - | 6155 | `		 * installed and type-checked: php's RECV order means a type error on` |
|        - | 6156 | ``		 * a PASSED argument beats the count error (`f(int $x,$y)` called`` |
|        - | 6157 | `		 * f("str") is a TypeError, not ArgumentCountError). The passed args` |
|        - | 6158 | `		 * were already released by the install loop, so the standard throw` |
|        - | 6159 | `		 * exit leaks nothing. The named path never fires this (its per-hole` |
|        - | 6160 | `		 * check ran in-loop; n == nNonVariadic >= nRequired here).` |
|        - | 6161 | `		 * Builtins written as embedded PHP in the prelude (VM_FUNC_INTERNAL,` |
|        - | 6162 | `		 * with or without VM_FUNC_CLASS_METHOD) are NOT exempt: their declared` |
|        - | 6163 | `		 * signatures were swept against the php 8.5 oracle, so the derived` |
|        - | 6164 | `		 * required count and wording are php's, and VmThrowBuiltinTooFewArgs` |
|        - | 6165 | `		 * words them as php words an internal callable. */` |
|  2201913 | 6166 | `		if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 6167 | `			sxu32 nNonVar,nReq;` |
|  1454545 | 6168 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|  1454545 | 6169 | `			if( n < nReq ){` |
|       89 | 6170 | `				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;` |
|       89 | 6171 | `				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|      101 | 6172 | `					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       33 | 6173 | `						nPassed,nReq,SySetUsed(&pVmFunc->aArgs));` |
|       35 | 6174 | `				}else{` |
|       33 | 6175 | `					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       10 | 6176 | `						nPassed,nReq,nNonVar,TRUE);` |
|        - | 6177 | `				}` |
|       89 | 6178 | `				if( rc == PH7_ABORT ){` |
|        3 | 6179 | `					goto Abort;` |
|        - | 6180 | `				}` |
|       87 | 6181 | `				PH7_MemObjRelease(pTos);` |
|       87 | 6182 | `				pTos = &pTos[-nCallArgs];` |
|       87 | 6183 | `				pFrameStack = 0;` |
|       87 | 6184 | `				rc = PH7_EXCEPTION;` |
|       87 | 6185 | `				goto SkipFuncBody;` |
|        5 | 6186 | `			}` |
|  1474600 | 6187 | `		}else if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) == VM_FUNC_INTERNAL ){` |
|        - | 6188 | `			/* Too-MANY arguments, the mirror php enforces for an internal` |
|        - | 6189 | `			 * callable ("count_chars() expects at most 2 arguments, 3 given").` |
|        - | 6190 | `			 * PHL accepted the extras silently — the surplus only ever reached` |
|        - | 6191 | `			 * func_get_args(). Every formal is filled on this branch, so the call` |
|        - | 6192 | `			 * is over the maximum exactly when more actuals arrived than there are` |
|        - | 6193 | `			 * non-variadic formals; a VARIADIC tail means php declares no maximum` |
|        - | 6194 | `			 * at all, so nNonVar below the formal count is exempt. Prelude` |
|        - | 6195 | `			 * FUNCTIONS only: the chunk-backed class METHODS were not swept against` |
|        - | 6196 | `			 * php's signatures (Fiber::start() is declared argless and reads` |
|        - | 6197 | `			 * func_get_args()). */` |
|        - | 6198 | `			sxu32 nNonVar,nReq;` |
|     4667 | 6199 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     4662 | 6200 | `			if( nNonVar == SySetUsed(&pVmFunc->aArgs)` |
|     4661 | 6201 | `			 && pFrame->nActualArgs >= 0` |
|     4665 | 6202 | `			 && (sxu32)pFrame->nActualArgs > nNonVar ){` |
|       55 | 6203 | `				rc = VmThrowBuiltinTooManyArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       36 | 6204 | `					(sxu32)pFrame->nActualArgs,nNonVar,nReq);` |
|       37 | 6205 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 6206 | `					goto Abort;` |
|        - | 6207 | `				}` |
|       37 | 6208 | `				PH7_MemObjRelease(pTos);` |
|       37 | 6209 | `				pTos = &pTos[-nCallArgs];` |
|       37 | 6210 | `				pFrameStack = 0;` |
|       37 | 6211 | `				rc = PH7_EXCEPTION;` |
|       37 | 6212 | `				goto SkipFuncBody;` |
|        - | 6213 | `			}` |
|     2313 | 6214 | `		}` |
|        - | 6215 | `		/* Process default values for remaining formal parameters */` |
|  5113511 | 6216 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|  2912187 | 6217 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 6218 | `				/* Variadic parameter with no extra args — create empty array */` |
|      467 | 6219 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      467 | 6220 | `				if( pObj ){` |
|      467 | 6221 | `					PH7_MemObjToHashmap(pObj);` |
|      467 | 6222 | `					sArg.nIdx = pObj->nIdx;` |
|      467 | 6223 | `					sArg.pUserData = 0;` |
|      467 | 6224 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      231 | 6225 | `				}` |
|      467 | 6226 | `				n++;` |
|      467 | 6227 | `				break; /* Variadic is always last */` |
|        - | 6228 | `			}` |
|  2911725 | 6229 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|  2911725 | 6230 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|  2911725 | 6231 | `				if( pObj ){` |
|        - | 6232 | `					/* Evaluate the default value and extract it's result */` |
|  2911725 | 6233 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|  2911725 | 6234 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 6235 | `						goto Abort;` |
|        - | 6236 | `					}` |
|        - | 6237 | `					/* Insert argument index */` |
|  2911725 | 6238 | `					sArg.nIdx = pObj->nIdx;` |
|  2911725 | 6239 | `					sArg.pUserData = 0;` |
|  2911725 | 6240 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 6241 | `					/* Make sure the default argument is of the correct type.` |
|        - | 6242 | ``					 * A null default on an implicitly-nullable param (`int $x = null`)`` |
|        - | 6243 | `					 * must stay null — casting it to 0/""/false would diverge from PHP` |
|        - | 6244 | `					 * and contradict the explicit-null path, which now keeps it null. */` |
|  2911720 | 6245 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|  1450634 | 6246 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)` |
|   725326 | 6247 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 6248 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 6249 | `						/* Cast to the desired type */` |
|      ! 0 | 6250 | `						xCast(pObj);` |
|      ! 0 | 6251 | `					}else{` |
|        - | 6252 | `						/* Mask matched — a const-indirected whole-real default` |
|        - | 6253 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|  2911725 | 6254 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 6255 | `					}` |
|  1455860 | 6256 | `				}` |
|  1455860 | 6257 | `			}` |
|  2911725 | 6258 | `			++n;` |
|        5 | 6259 | `		}` |
|        - | 6260 | `		} /* end VmCallArgMap scope */` |
|        - | 6261 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 6262 | `		 * does not return anything.` |
|        - | 6263 | `		 */` |
|  2201791 | 6264 | `		PH7_MemObjRelease(pTos);` |
|  2201791 | 6265 | `		pTos = &pTos[-nCallArgs];` |
|        - | 6266 | `		/* Allocate an operand stack (via the recycling allocator) and evaluate the` |
|        - | 6267 | `		 * function body. Size it to a tight static bound when the body is statically` |
|        - | 6268 | `		 * modelable (BYTECODE.md stage 7) — the big memory win for deep recursion,` |
|        - | 6269 | `		 * where one such stack lives per frame — falling back to the safe` |
|        - | 6270 | `		 * instruction-count bound otherwise.` |
|        - | 6271 | `		 *` |
|        - | 6272 | `		 * The bound is computed LAZILY on the first call and cached on the func` |
|        - | 6273 | `		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:` |
|        - | 6274 | `		 * self-computing on first use is fail-safe against any body-creation path` |
|        - | 6275 | `		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),` |
|        - | 6276 | `		 * where undersizing is a heap overflow; the amortized cost is one analysis` |
|        - | 6277 | `		 * per function. */` |
|        - | 6278 | `		{` |
|  2201791 | 6279 | `			sxu32 nSlots = pVmFunc->nMaxStack;` |
|  2201791 | 6280 | `			if( nSlots == 0 ){` |
|    11473 | 6281 | `				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);` |
|    17207 | 6282 | `				sxu32 nTight = VmComputeMaxStack(&(*pVm),` |
|    11468 | 6283 | `					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);` |
|    11473 | 6284 | `				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;` |
|    11473 | 6285 | `				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */` |
|    11473 | 6286 | `				pVmFunc->nMaxStack = nSlots;` |
|     5734 | 6287 | `			}` |
|  2201791 | 6288 | `			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);` |
|        - | 6289 | `		}` |
|  2201791 | 6290 | `		if( pFrameStack == 0 ){` |
|        - | 6291 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 6292 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 6293 | `				&pVmFunc->sName);` |
|      ! 0 | 6294 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 6295 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6296 | `			}` |
|      ! 0 | 6297 | `			break;` |
|        - | 6298 | `		}` |
|  1100695 | 6299 | `SkipFuncBody:` |
|  2202265 | 6300 | `		if( pSelf ){` |
|        - | 6301 | `			/* Push class name */` |
|  1971227 | 6302 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|   985611 | 6303 | `		}` |
|        - | 6304 | `		/* Increment nesting level */` |
|  2202265 | 6305 | `		pVm->nRecursionDepth++;` |
|  2202265 | 6306 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 6307 | `			/* Arg-binding threw: there is no body to run — finish the call` |
|        - | 6308 | `			 * immediately (no record is pushed). */` |
|        - | 6309 | `			VmCallRecord sCallee;` |
|      479 | 6310 | `			sCallee.pVmFunc = pVmFunc;` |
|      479 | 6311 | `			sCallee.pFrame = pFrame;` |
|      479 | 6312 | `			sCallee.pFrameStack = pFrameStack;` |
|      479 | 6313 | `			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|      479 | 6314 | `			sCallee.nLastRef = SXU32_HIGH;` |
|      479 | 6315 | `			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|      479 | 6316 | `			sState.pTos = pTos;` |
|      479 | 6317 | `			sState.pc = pc;` |
|      479 | 6318 | `			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);` |
|      479 | 6319 | `			pTos = sState.pTos;` |
|      479 | 6320 | `			pc = sState.pc;` |
|      479 | 6321 | `			if( rc == PH7_ABORT ){` |
|        - | 6322 | `				/* Abort processing immeditaley */` |
|      ! 0 | 6323 | `				goto Abort;` |
|      479 | 6324 | `			}else if( rc == PH7_SUSPEND ){` |
|      ! 0 | 6325 | `				goto Suspend;` |
|      479 | 6326 | `			}else if( rc == PH7_EXCEPTION ){` |
|      169 | 6327 | `				goto Exception;` |
|        - | 6328 | `			}` |
|      160 | 6329 | `		}else{` |
|        - | 6330 | `			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a` |
|        - | 6331 | `			 * call record (caller activation + in-flight call) and switch the` |
|        - | 6332 | `			 * loop's locals to the callee — a PHP->PHP call no longer grows` |
|        - | 6333 | `			 * the native stack. The record node is pool-allocated so` |
|        - | 6334 | `			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */` |
|  2201791 | 6335 | `			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;` |
|  2201791 | 6336 | `			if( pRec ){` |
|  2198969 | 6337 | `				pVm->pIdleCallFrames = (void *)pRec->pPrev;` |
|  1099685 | 6338 | `			}else{` |
|     2827 | 6339 | `				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));` |
|        - | 6340 | `			}` |
|  2201791 | 6341 | `			if( pRec == 0 ){` |
|        - | 6342 | `				/* OOM: undo the push-time accounting, tear the call down and` |
|        - | 6343 | `				 * raise the non-catchable fatal (the §3.1 OOM convention —` |
|        - | 6344 | `				 * never a silent NULL). */` |
|      ! 0 | 6345 | `				pVm->nRecursionDepth--;` |
|      ! 0 | 6346 | `				if( pSelf ){` |
|      ! 0 | 6347 | `					(void)SySetPop(&pVm->aSelf);` |
|      ! 0 | 6348 | `				}` |
|      ! 0 | 6349 | `				SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|      ! 0 | 6350 | `				VmLeaveFrame(&(*pVm));` |
|      ! 0 | 6351 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 6352 | `				goto Abort;` |
|        - | 6353 | `			}` |
|  2201791 | 6354 | `			sState.pTos = pTos;` |
|  2201791 | 6355 | `			sState.pc = pc;` |
|  2201791 | 6356 | `			pRec->sCaller = sState;` |
|  2201791 | 6357 | `			pRec->sCall.pVmFunc = pVmFunc;` |
|  2201791 | 6358 | `			pRec->sCall.pFrame = pFrame;` |
|  2201791 | 6359 | `			pRec->sCall.pFrameStack = pFrameStack;` |
|  2201791 | 6360 | `			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|  2201791 | 6361 | `			pRec->sCall.nLastRef = SXU32_HIGH;` |
|  2201791 | 6362 | `			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|  2201791 | 6363 | `			pRec->pPrev = pCallTop;` |
|  2201791 | 6364 | `			pCallTop = pRec;` |
|        - | 6365 | `			/* Switch to the callee activation (what the recursive` |
|        - | 6366 | `			 * VmByteCodeExec entry used to set up). */` |
|  2201791 | 6367 | `			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);` |
|  2201791 | 6368 | `			pStack = pFrameStack;` |
|  2201791 | 6369 | `			pTos = &pStack[-1];` |
|  2201791 | 6370 | `			pc = 0;` |
|  2201791 | 6371 | `			sState.aInstr = aInstr;` |
|  2201791 | 6372 | `			sState.pStack = pStack;` |
|  2201791 | 6373 | `			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */` |
|  2201791 | 6374 | `			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */` |
|  2201791 | 6375 | `			sState.pTos = pTos;` |
|  2201791 | 6376 | `			sState.pc = 0;` |
|  2201791 | 6377 | `			sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|  2201791 | 6378 | `			sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|  2201791 | 6379 | `			sState.pEntryFrame = pVm->pFrame;` |
|  2201791 | 6380 | `			sState.pResult = pRec->sCaller.pTos;` |
|  2201791 | 6381 | `			sState.pLastRef = &pRec->sCall.nLastRef;` |
|  2201791 | 6382 | `			sState.pEnforceRetFunc = VmFuncHasReturnType(pVmFunc) ? pVmFunc : 0;` |
|  2201791 | 6383 | `			sState.is_callback = 0;` |
|  2201791 | 6384 | `			sState.bReturnPropagates = 0;` |
|  2201791 | 6385 | `			goto VmLoopFetch;` |
|        - | 6386 | `		}` |
|      160 | 6387 | `	}else{` |
|        - | 6388 | `		/* Look for an installed foreign function.` |
|        - | 6389 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 6390 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 6391 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 6392 | `		 * global fallback for unqualified function calls in namespaces. */` |
|  4305832 | 6393 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 6394 | `		{` |
|  4305832 | 6395 | `		VmCallArgMap *pCallMap2 = pEffCallMap;` |
|  4305832 | 6396 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 6397 | `			/* Compiler-qualified: try short name as global fallback */` |
|       53 | 6398 | `			const char *zShort = sName.zString;` |
|        - | 6399 | `			sxu32 i;` |
|      765 | 6400 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      717 | 6401 | `				if( sName.zString[i] == '\\' ){` |
|       67 | 6402 | `					zShort = &sName.zString[i + 1];` |
|       31 | 6403 | `				}` |
|      361 | 6404 | `			}` |
|       53 | 6405 | `			if( zShort != sName.zString ){` |
|       53 | 6406 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       53 | 6407 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       24 | 6408 | `			}` |
|       24 | 6409 | `		}` |
|        - | 6410 | `		} /* end VmCallArgMap namespace scope */` |
|  4305832 | 6411 | `		if( pEntry == 0 ){` |
|        - | 6412 | `			/* php accepts the "Class::method" STATIC-callable string everywhere a` |
|        - | 6413 | `			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).` |
|        - | 6414 | `			 * Split on the LAST "::" (php's own zend_memrchr scan) and route through the` |
|        - | 6415 | `			 * shared array-callable machinery ([class-name, method-name]) instead of` |
|        - | 6416 | `			 * warning undefined. */` |
|   240114 | 6417 | `			const char *zCbCls = 0,*zCbMeth = 0;` |
|   240114 | 6418 | `			sxu32 nCbCls = 0,nCbMeth = 0;` |
|   240114 | 6419 | `			int bScoped = PH7_VmCallableStringParts(sName.zString,sName.nByte,` |
|        - | 6420 | `				&zCbCls,&nCbCls,&zCbMeth,&nCbMeth);` |
|   240114 | 6421 | `			if( bScoped ){` |
|        - | 6422 | `				/* Resolve BEFORE dispatching: the shared dispatcher answers SXRET_OK with` |
|        - | 6423 | `` 				 * a NULL result for a pair it cannot resolve, so `$cb='C::nosuch'; $cb();` `` |
|        - | 6424 | `				 * evaluated to NULL with no diagnostic at all — where php throws the same` |
|        - | 6425 | `				 * Errors the ARRAY form of the same call already raised here. (Its own` |
|        - | 6426 | ``				 * `if( bScoped )` block: this scratch buffer and the dispatch block's`` |
|        - | 6427 | `				 * hashmap are both block-head declarations, and the check runs between` |
|        - | 6428 | `				 * them.) */` |
|        - | 6429 | `				char zSmMsg[192];` |
|   100059 | 6430 | `				sxi32 nSmBrc = pVm->nBoundaryRc;` |
|   100059 | 6431 | `				const void *pSmRes = (const void *)pVm->pResumeFrame;` |
|   150087 | 6432 | `				const char *zSmErr = VmCallableClassMethodError(&(*pVm),` |
|    50028 | 6433 | `					PH7_VmExtractClass(&(*pVm),zCbCls,nCbCls,FALSE,0),` |
|    50028 | 6434 | `					zCbCls,nCbCls,zCbMeth,nCbMeth,TRUE,zSmMsg,sizeof(zSmMsg));` |
|   100059 | 6435 | `				if( zSmErr ){` |
|        - | 6436 | `					sxi32 rcSmErr;` |
|       43 | 6437 | `					int bSmRaised = PH7_VmClassLookupRaised(&(*pVm),nSmBrc,pSmRes);` |
|       43 | 6438 | `					if( pInstr->iP2 ){` |
|      ! 0 | 6439 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 6440 | `					}` |
|       43 | 6441 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 6442 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6443 | `					}` |
|       43 | 6444 | `					PH7_MemObjRelease(pTos);` |
|       43 | 6445 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       43 | 6446 | `					pTos->nIdx = SXU32_HIGH;` |
|       43 | 6447 | `					if( bSmRaised ){` |
|        - | 6448 | `						/* The class name's autoloader threw: land THAT, exactly as the array` |
|        - | 6449 | `						 * form does — php never reports the class missing in this case. */` |
|        6 | 6450 | `						rcSmErr = pVm->nBoundaryRc;` |
|        6 | 6451 | `						pVm->nBoundaryRc = 0;` |
|        6 | 6452 | `						if( rcSmErr == PH7_ABORT ){` |
|      ! 0 | 6453 | `							goto Abort;` |
|        - | 6454 | `						}` |
|        6 | 6455 | `						rc = PH7_EXCEPTION;` |
|       12 | 6456 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6457 | `					}` |
|       38 | 6458 | `					rcSmErr = VmThrowFromVm(&(*pVm),"Error",zSmErr,(sxu32)SyStrlen(zSmErr));` |
|       38 | 6459 | `					if( rcSmErr == SXERR_ABORT ){` |
|      ! 0 | 6460 | `						goto Abort;` |
|        - | 6461 | `					}` |
|       38 | 6462 | `					rc = rcSmErr;` |
|       50 | 6463 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6464 | `				}` |
|    50008 | 6465 | `			}` |
|   240074 | 6466 | `			if( bScoped ){` |
|        - | 6467 | `				/* Resolved: hand the callable STRING itself to the shared dispatcher, which` |
|        - | 6468 | ``				 * decodes `Class::method` the same way (it has to, for the callback-argument`` |
|        - | 6469 | `				 * callers). This used to build a throwaway [class,method] map here and enter` |
|        - | 6470 | `				 * through the hashmap branch — two decoders for one spelling. */` |
|        - | 6471 | `				ph7_value sResult;` |
|        - | 6472 | `				sxi32 rcSm;` |
|   150026 | 6473 | `				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100016 | 6474 | `					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|   100018 | 6475 | `				SySetReset(&aArg);` |
|   100028 | 6476 | `				while( pArg < pTos ){` |
|       11 | 6477 | `					SySetPut(&aArg,(const void *)&pArg);` |
|       11 | 6478 | `					pArg++;` |
|        1 | 6479 | `				}` |
|   100018 | 6480 | `				PH7_MemObjInit(pVm,&sResult);` |
|   150026 | 6481 | `				rcSm = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),` |
|   100016 | 6482 | `					(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|   100018 | 6483 | `				SySetReset(&aArg);` |
|   100018 | 6484 | `				if( nCallArgs > 0 ){` |
|        9 | 6485 | `					VmPopOperand(&pTos,nCallArgs);` |
|        4 | 6486 | `				}` |
|   100018 | 6487 | `				if( rcSm == PH7_ABORT ){` |
|      ! 0 | 6488 | `					PH7_MemObjRelease(&sResult);` |
|      ! 0 | 6489 | `					goto Abort;` |
|        - | 6490 | `				}` |
|   100018 | 6491 | `				if( rcSm == PH7_EXCEPTION ){` |
|        - | 6492 | `					sxi32 iResumePc;` |
|   100004 | 6493 | `					PH7_MemObjRelease(&sResult);` |
|   100004 | 6494 | `					if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100001 | 6495 | `						PH7_MemObjRelease(pTos);` |
|        - | 6496 | `						/* Drain the abandoned outer-expression operands` |
|        - | 6497 | ``						 * (`1 + "C::m"()`) to the try's base — one leaked`` |
|        - | 6498 | `						 * slot per caught throw otherwise. */` |
|   300001 | 6499 | `						PH7_RESUME_DRAIN()` |
|   100001 | 6500 | `						pc = iResumePc;` |
|   100001 | 6501 | `						break;` |
|        - | 6502 | `					}` |
|        3 | 6503 | `					goto Exception;` |
|        - | 6504 | `				}` |
|       15 | 6505 | `				PH7_MemObjStore(&sResult,pTos);` |
|       15 | 6506 | `				PH7_MemObjRelease(&sResult);` |
|       15 | 6507 | `				break;` |
|        - | 6508 | `			}` |
|        - | 6509 | `			/* Call to an undefined function is a catchable Error in php 8 — it does` |
|        - | 6510 | `			 * NOT warn and hand back null and carry on, which is what PH7 did (and` |
|        - | 6511 | `			 * which quietly turned a typo into a null-propagating program). */` |
|        - | 6512 | `			{` |
|        - | 6513 | `			SyBlob sMsg;` |
|   140058 | 6514 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   140058 | 6515 | `			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);` |
|        - | 6516 | `			/* Consume this call's captured spread runs so they don't leak into a` |
|        - | 6517 | `			 * later call (this path never reaches VmBuildEffectiveArgMap). */` |
|   140058 | 6518 | `			if( pInstr->iP2 ){` |
|        3 | 6519 | `				VmSpreadConsume(pVm);` |
|        1 | 6520 | `			}` |
|        - | 6521 | `			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the` |
|        - | 6522 | `			 * compile-time arg count on the stack, and this early exit skips the` |
|        - | 6523 | `			 * arg-building loop that the normal path uses, so popping only iP1 would` |
|        - | 6524 | `			 * strand the expanded elements and corrupt the enclosing expression. */` |
|   140058 | 6525 | `			if( nCallArgs > 0 ){` |
|        8 | 6526 | `				VmPopOperand(&pTos,nCallArgs);` |
|        3 | 6527 | `			}` |
|   140058 | 6528 | `			PH7_MemObjRelease(pTos);` |
|   210085 | 6529 | `			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|    70027 | 6530 | `				SyBlobLength(&sMsg));` |
|   140058 | 6531 | `			SyBlobRelease(&sMsg);` |
|   140058 | 6532 | `			if( rc == SXERR_ABORT ){` |
|        6 | 6533 | `				goto Abort;` |
|        - | 6534 | `			}` |
|        - | 6535 | `			/* A catch may have run IN PLACE inside VmThrowFromVm (SXRET_OK +` |
|        - | 6536 | ``			 * recorded resume). An unconditional `goto Exception` here unwound the`` |
|        - | 6537 | `` 			 * exec ANYWAY, so `try { nosuchfn(); } catch (Error $e) {} rest();` `` |
|        - | 6538 | `			 * ran the catch and then silently dropped the rest of the script` |
|        - | 6539 | `			 * (exit 0). Route like every other in-exec throw site. */` |
|   380098 | 6540 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6541 | `			}` |
|        - | 6542 | `		}` |
|  4065722 | 6543 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 6544 | `		/* D1: resolve deferred plain-var arguments against the builtin's by-ref position` |
|        - | 6545 | `		 * mask (derived from its signature). A by-ref out-param (preg_match's $matches, …)` |
|        - | 6546 | `		 * is materialized so PH7_VmStoreArgByRef can write back — this is what keeps a` |
|        - | 6547 | ``		 * DYNAMIC-name by-ref builtin (`$f='preg_match'; $f($p,$s,$m)`) working now that the`` |
|        - | 6548 | `		 * compile-time mask no longer sees it. Every other (by-value) arg warns + passes` |
|        - | 6549 | `		 * NULL, which is also what call_user_func & friends want. pVm->pFrame is the caller. */` |
|        - | 6550 | `		{` |
|  4065722 | 6551 | `			sxi32 rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,pFunc->nByRefMask,0,0);` |
|  4065738 | 6552 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6553 | `		}` |
|        - | 6554 | `		/* Host function (builtin): build the effective spread-key map so the` |
|        - | 6555 | `		 * name-forwarding builtins (call_user_func & friends) relay string keys as` |
|        - | 6556 | `		 * named args, and — critically — so this call's captured runs are consumed.` |
|        - | 6557 | `		 * pArg is the top base here (a builtin call pops no method-name slot). */` |
|  6099309 | 6558 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  4065707 | 6559 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|  2035058 | 6560 | `NativeCall:` |
|        - | 6561 | `		/* A VM_FUNC_NATIVE method joins here, having done the two steps above for` |
|        - | 6562 | `		 * itself: its by-ref mask comes from the same signature machinery, and its` |
|        - | 6563 | `		 * effective arg map was already built (and this call's spread runs already` |
|        - | 6564 | `		 * consumed) on the method path — building it a second time here would` |
|        - | 6565 | `		 * consume them twice. Everything from this point down is shared verbatim:` |
|        - | 6566 | `		 * a native method IS a host call that happens to carry a receiver. */` |
|        - | 6567 | `		/* Start collecting function arguments */` |
|  4071608 | 6568 | `		SySetReset(&aArg);` |
|  8962202 | 6569 | `		while( pArg < pTos ){` |
|  4890599 | 6570 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  4890599 | 6571 | `			pArg++;` |
|        5 | 6572 | `		}` |
|        - | 6573 | `		/* Assume a null return value */` |
|  4071608 | 6574 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 6575 | `		/* Init the call context */` |
|  4071608 | 6576 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 6577 | `		/* Hand the call-site named-argument map to the builtin so name-forwarding` |
|        - | 6578 | `		 * helpers (call_user_func & friends) can relay name: arguments — and the` |
|        - | 6579 | `		 * caller's strict_types mode — to the inner callback. Forwarded whole (not` |
|        - | 6580 | `		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even` |
|        - | 6581 | `		 * when its own call site is purely positional; only the two forwarding` |
|        - | 6582 | `		 * builtins read pArgMap, so this is inert for every other host function. */` |
|  4071608 | 6583 | `		sCtx.pArgMap = pEffCallMap;` |
|        - | 6584 | `		/* Receiver + late-static-binding class for a native METHOD. Both stay 0 for a` |
|        - | 6585 | `		 * plain host function (the locals are initialized once per OP_CALL), so this` |
|        - | 6586 | `		 * is inert on the builtin path. The reference on pNativeOwned belongs to the` |
|        - | 6587 | `		 * caller for the span of the call — the native body borrows it and must not` |
|        - | 6588 | `		 * unref, exactly as a bytecode method's frame $this is borrowed. */` |
|  4071608 | 6589 | `		sCtx.pThis = pNativeRecv;` |
|  4071608 | 6590 | `		sCtx.pCalledClass = pNativeClass;` |
|        - | 6591 | `		{` |
|  4071608 | 6592 | `		int nGiven = (int)SySetUsed(&aArg);` |
|        - | 6593 | `		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum` |
|        - | 6594 | `		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError` |
|        - | 6595 | `		 * before the C routine runs when called with too few arguments — instead` |
|        - | 6596 | `		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""` |
|        - | 6597 | `		 * return. The message wording matches php's ZPP output byte-for-byte. */` |
|  4071608 | 6598 | `		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){` |
|      728 | 6599 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 6600 | `				"%z() expects %s %d argument%s, %d given",` |
|      241 | 6601 | `				&pFunc->sName,` |
|      482 | 6602 | `				pFunc->bAtLeast ? "at least" : "exactly",` |
|      482 | 6603 | `				(int)pFunc->nMinArg,` |
|      482 | 6604 | `				pFunc->nMinArg == 1 ? "" : "s",` |
|      241 | 6605 | `				nGiven);` |
|  4071367 | 6606 | `		}else if( pFunc->bHasMaxArg && nGiven > (int)pFunc->nMaxArg ){` |
|        - | 6607 | `			/* php enforces the MAXIMUM as well, and PHL only did so where a` |
|        - | 6608 | `			 * builtin happened to hand-roll the check (51 of ~650), so` |
|        - | 6609 | ``			 * `microtime(1,2)`, `strlen("a","b")` and friends silently ignored`` |
|        - | 6610 | `			 * the extras. The count comes from the same signature table as the` |
|        - | 6611 | `			 * minimum; a variadic tail leaves nMaxArg at -1 and is exempt.` |
|        - | 6612 | `			 * php says "exactly" when the bounds coincide, "at most" otherwise. */` |
|      143 | 6613 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 6614 | `				"%z() expects %s %d argument%s, %d given",` |
|       46 | 6615 | `				&pFunc->sName,` |
|       78 | 6616 | `				((int)pFunc->nMinArg == (int)pFunc->nMaxArg && !pFunc->bAtLeast) ? "exactly" : "at most",` |
|       92 | 6617 | `				(int)pFunc->nMaxArg,` |
|       92 | 6618 | `				pFunc->nMaxArg == 1 ? "" : "s",` |
|       46 | 6619 | `				nGiven);` |
|  6107338 | 6620 | `		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,` |
|  4071029 | 6621 | `			(ph7_value **)SySetBasePtr(&aArg))) ){` |
|        - | 6622 | `			/* TypeError thrown: rc carries the caught/uncaught status */` |
|      118 | 6623 | `		}else{` |
|        - | 6624 | `			/* Call the foreign function */` |
|  4070808 | 6625 | `			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));` |
|        - | 6626 | `			/* A host function that RAISED a catchable throw (PH7_VmThrowException)` |
|        - | 6627 | `			 * and still returned PH7_OK reports it here — the throw's catch has` |
|        - | 6628 | `			 * already run in place, so treating the call as a normal return would` |
|        - | 6629 | `			 * resume execution INSIDE the try body the throw abandoned (and, when` |
|        - | 6630 | `			 * uncaught, run on past the reported fatal). The status is recorded on` |
|        - | 6631 | `			 * the call context, so this covers the shared validation helpers whose` |
|        - | 6632 | `			 * callers have no channel to thread a status back. */` |
|  4070808 | 6633 | `			rc = VmHostFuncThrowRc(&sCtx,rc);` |
|        - | 6634 | `		}` |
|        - | 6635 | `		}` |
|        - | 6636 | `		/* Release the call context */` |
|  4071608 | 6637 | `		VmReleaseCallContext(&sCtx);` |
|  4071608 | 6638 | `		if( pNativeOwned ){` |
|        - | 6639 | `			/* Drop the receiver reference the method branch took (pThis->iRef++ before` |
|        - | 6640 | `			 * the target slot was released). A bytecode method hands this to its frame` |
|        - | 6641 | `			 * and lets the teardown do it; a native method has no frame, so it is` |
|        - | 6642 | `			 * dropped here — the one point EVERY route out of the call passes through,` |
|        - | 6643 | `			 * including the throw/abort/suspend ones below. Ordered after the context` |
|        - | 6644 | `			 * release because sCtx.sThis aliases the instance. Always 0 for a plain` |
|        - | 6645 | `			 * host function. */` |
|     5493 | 6646 | `			PH7_ClassInstanceUnref(pNativeOwned);` |
|     5493 | 6647 | `			pNativeOwned = 0;` |
|     5493 | 6648 | `			pNativeRecv = 0;` |
|     2744 | 6649 | `		}` |
|  4071608 | 6650 | `		if( rc == PH7_ABORT ){` |
|        - | 6651 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 6652 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 6653 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      588 | 6654 | `			PH7_MemObjRelease(&sRet);` |
|      588 | 6655 | `			goto Abort;` |
|        - | 6656 | `		}` |
|  4071024 | 6657 | `		if( rc != PH7_SUSPEND && pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 6658 | `			/* A throw raised inside this host function — directly` |
|        - | 6659 | `			 * (PH7_VmThrowException) or by a PHP callback it invoked — was` |
|        - | 6660 | `			 * caught by an INLINE try (generator body) THIS exec owns.` |
|        - | 6661 | `			 * VmThrowInline records only a pc-redirect: a direct builtin throw` |
|        - | 6662 | `			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION` |
|        - | 6663 | `			 * with the redirect pending, so the rc branches below never land` |
|        - | 6664 | `			 * it (pre-existing hole: explode("") or a throwing usort` |
|        - | 6665 | `			 * comparator inside a generator's try lost the catch AND the` |
|        - | 6666 | `			 * yield). Land at the redirect now — its drain to the try's` |
|        - | 6667 | `			 * operand base subsumes the args + name pops. */` |
|       10 | 6668 | `			PH7_MemObjRelease(&sRet);` |
|       32 | 6669 | `			PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 6670 | `		}` |
|  4071016 | 6671 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 6672 | `			/* A callback invoked by this host function threw. If an in-place catch` |
|        - | 6673 | `			 * recorded a resume target owned by THIS body, resume at its landing pad` |
|        - | 6674 | `			 * (consuming the target); otherwise the exception was caught by an outer` |
|        - | 6675 | `			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW` |
|        - | 6676 | `			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which` |
|        - | 6677 | `			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */` |
|        - | 6678 | `			sxi32 iResumePc;` |
|     5837 | 6679 | `			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 6680 | `				/* Caught by an outer exec, or not caught here: propagate. */` |
|      555 | 6681 | `				goto Exception;` |
|        - | 6682 | `			}` |
|        - | 6683 | `			/* Exception was caught in place by THIS body's try: pop args and the` |
|        - | 6684 | `			 * result slot, then drain any abandoned outer-expression operands to` |
|        - | 6685 | `			 * the try's base and resume. */` |
|     5287 | 6686 | `			PH7_MemObjRelease(&sRet);` |
|     5287 | 6687 | `			if( nCallArgs > 0 ){` |
|     4983 | 6688 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|     2489 | 6689 | `			}` |
|     5287 | 6690 | `			VmPopOperand(&pTos,1);` |
|     9287 | 6691 | `			PH7_RESUME_DRAIN()` |
|     5287 | 6692 | `			pc = iResumePc;` |
|     5287 | 6693 | `			break;` |
|        - | 6694 | `		}` |
|  4065184 | 6695 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 6696 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 6697 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 6698 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 6699 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 6700 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 6701 | `			 * body), the user-function path above will handle re-saving. */` |
|      361 | 6702 | `			PH7_MemObjRelease(&sRet);` |
|      361 | 6703 | `			if( nCallArgs > 0 ){` |
|      355 | 6704 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      175 | 6705 | `			}` |
|        - | 6706 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 6707 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|      361 | 6708 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|      361 | 6709 | `			goto Suspend;` |
|        - | 6710 | `		}` |
|  4064828 | 6711 | `		if( nCallArgs > 0 ){` |
|        - | 6712 | `			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack` |
|        - | 6713 | `			 * expanded the compile-time arg count on the stack, so popping iP1` |
|        - | 6714 | `			 * strands the extra elements (or, for an unpack that expanded to fewer` |
|        - | 6715 | `			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,` |
|        - | 6716 | `			 * reading a bogus value whose stray flags sent MemObjStore into the` |
|        - | 6717 | `			 * hashmap-release path and hung). Mirrors every other CALL exit. The` |
|        - | 6718 | `			 * function-name slot (pTos) receives the return value below. */` |
|  4039406 | 6719 | `			VmPopOperand(&pTos,nCallArgs);` |
|  2020444 | 6720 | `		}` |
|        - | 6721 | `		/* Save foreign function return value into the (now top) function-name slot */` |
|  4064828 | 6722 | `		PH7_MemObjStore(&sRet,pTos);` |
|  4064828 | 6723 | `		PH7_MemObjRelease(&sRet);` |
|        - | 6724 | `	}` |
|  4065138 | 6725 | `	break;` |
|        - | 6726 | `				  }` |
|        - | 6727 | `/*` |
|        - | 6728 | ` * OP_CONSUME: P1 * *` |
|        - | 6729 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 6730 | ` */` |
|    34832 | 6731 | `case PH7_OP_CONSUME: {` |
|        - | 6732 | `	VmOpRc rcOp;` |
|    69669 | 6733 | `	sState.pTos = pTos;` |
|    69669 | 6734 | `	sState.pc = pc;` |
|    69669 | 6735 | `	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);` |
|    69669 | 6736 | `	pTos = sState.pTos;` |
|    69669 | 6737 | `	pc = sState.pc;` |
|    69669 | 6738 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 6739 | `		goto Abort;` |
|    69667 | 6740 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       14 | 6741 | `		goto Exception;` |
|        - | 6742 | `	}` |
|    69650 | 6743 | `	break;` |
|        - | 6744 | `					  }` |
|        - | 6745 |  |
|        - | 6746 | `		} /* Switch() */` |
| 67864468 | 6747 | `		pc++; /* Next instruction in the stream */` |
|        5 | 6748 | `	} /* For(;;) */` |
|  5283528 | 6749 | `Done:` |
|        - | 6750 | `	/* A stacked callee completing lands here too (its result is already in` |
|        - | 6751 | `	 * sState.pResult — the caller's operand slot); Unwind's first iteration` |
|        - | 6752 | `	 * bottoms out identically for the record-less case. */` |
| 10567457 | 6753 | `	rc = SXRET_OK;` |
| 10567457 | 6754 | `	goto Unwind;` |
|      846 | 6755 | `Suspend:` |
|     1697 | 6756 | `	rc = PH7_SUSPEND;` |
|     1697 | 6757 | `	if( pCallTop != 0 ){` |
|        - | 6758 | `		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment` |
|        - | 6759 | `		 * instead of the lossy unwind. pc/nTos of the innermost activation were` |
|        - | 6760 | `		 * already saved into the ctx by VmSuspendCtx; capture the rest (the` |
|        - | 6761 | `		 * record chain, the innermost activation, the suspend-time top frame)` |
|        - | 6762 | `		 * so resume re-enters HERE, inside the innermost callee, like php. The` |
|        - | 6763 | `		 * records / frames / operand stacks stay alive — nothing is freed. Only` |
|        - | 6764 | `		 * fibers reach this (generators yield only at their body level, pCallTop` |
|        - | 6765 | `		 * == 0); a suspend inside a C->PHP callback was already rejected with a` |
|        - | 6766 | `		 * FiberError before it could arrive here. */` |
|      306 | 6767 | `		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));` |
|      306 | 6768 | `		if( pSeg == 0 ){` |
|        - | 6769 | `			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST` |
|        - | 6770 | `			 * pc/nTos into the ctx, so the lossy body-level fallback would resume` |
|        - | 6771 | `			 * the callee's pc against the body stack — silent corruption, exactly` |
|        - | 6772 | `			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the` |
|        - | 6773 | `			 * §3.1 convention shared with the stage-2 record-alloc OOM site). */` |
|      ! 0 | 6774 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 6775 | `			rc = PH7_ABORT;` |
|      ! 0 | 6776 | `			goto Unwind;` |
|        - | 6777 | `		}` |
|      306 | 6778 | `		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */` |
|      306 | 6779 | `		pSeg->sState = sState;` |
|      306 | 6780 | `		pSeg->pCallTop = pCallTop;` |
|      306 | 6781 | `		pSeg->pTopFrame = pVm->pFrame;` |
|      306 | 6782 | `		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;` |
|      306 | 6783 | `		pSeg->nOldFinBase = pVm->pActiveCtx ? pVm->pActiveCtx->nFinallyBase : 0;` |
|        - | 6784 | `		{` |
|        - | 6785 | `			VmCallFrame *pRec;` |
|      306 | 6786 | `			pSeg->nRecords = 0;` |
|      608 | 6787 | `			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){` |
|      306 | 6788 | `				pSeg->nRecords++;` |
|      155 | 6789 | `			}` |
|        - | 6790 | `		}` |
|      306 | 6791 | `		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;` |
|        - | 6792 | `		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */` |
|      306 | 6793 | `		SySetRelease(&aArg);` |
|      306 | 6794 | `		return PH7_SUSPEND;` |
|        - | 6795 | `	}` |
|     1395 | 6796 | `	goto Unwind;` |
|      405 | 6797 | `Abort:` |
|      814 | 6798 | `	rc = PH7_ABORT;` |
|      814 | 6799 | `	goto Unwind;` |
|   301099 | 6800 | `Exception:` |
|   602203 | 6801 | `	rc = PH7_EXCEPTION;` |
|   602198 | 6802 | `	goto Unwind;` |
|  5585727 | 6803 | `Unwind:` |
|        - | 6804 | `	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration` |
|        - | 6805 | `	 * finishes the top record exactly as the old per-level native return did:` |
|        - | 6806 | `	 * for ABORT/EXCEPTION, first run what the popped activation's own` |
|        - | 6807 | `	 * Abort/Exception label used to do (clear its pending return, release its` |
|        - | 6808 | `	 * operands — a stacked activation never has bReturnPropagates set), then` |
|        - | 6809 | `	 * VmCallFinish routes in the restored caller (an in-place catch there` |
|        - | 6810 | `	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —` |
|        - | 6811 | `	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible` |
|        - | 6812 | `	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).` |
|        - | 6813 | `	 * At the bottom, VmExecFinalize hands the status to the native caller. */` |
|  5786625 | 6814 | `	for(;;){` |
| 11572859 | 6815 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|        - | 6816 | `			/* Drop any pending hook-RMW write-backs this activation armed — its` |
|        - | 6817 | `			 * statement is abandoned (only the innermost activation at throw time` |
|        - | 6818 | `			 * can own entries: the armed window spans exactly one instruction, so` |
|        - | 6819 | `			 * no OP_CALL record ever intervenes). */` |
|  1004016 | 6820 | `			while( SySetUsed(&pVm->aHookRmw) > 0` |
|  1004017 | 6821 | `			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){` |
|      ! 0 | 6822 | `				VmHookRmwDropTop(&(*pVm));` |
|      ! 0 | 6823 | `			}` |
|   502006 | 6824 | `		}` |
| 11572859 | 6825 | `		if( pCallTop == 0 ){` |
|  9371271 | 6826 | `			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);` |
|        - | 6827 | `		}` |
|  2201593 | 6828 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|   602563 | 6829 | `			VmClearFramePending(sState.pEntryFrame);` |
|   605657 | 6830 | `			while( pTos >= pStack ){` |
|     3099 | 6831 | `				PH7_MemObjRelease(pTos);` |
|     3099 | 6832 | `				pTos--;` |
|        5 | 6833 | `			}` |
|   301279 | 6834 | `		}` |
|  2201593 | 6835 | `		if( rc != PH7_SUSPEND ){` |
|        - | 6836 | `			/* The finishing callee's own leaked finally actions must not survive` |
|        - | 6837 | `			 * into the caller (whose next OP_END_FINALLY would mis-pop them). */` |
|  2201591 | 6838 | `			VmDiscardFinallyActions(&(*pVm),sState.nFinallyActBase);` |
|  1100991 | 6839 | `		}` |
|        - | 6840 | `		{` |
|  2201593 | 6841 | `			VmCallFrame *pRec = pCallTop;` |
|  2201593 | 6842 | `			sState = pRec->sCaller;` |
|  2201593 | 6843 | `			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);` |
|  2201593 | 6844 | `			pCallTop = pRec->pPrev;` |
|  2201593 | 6845 | `			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;` |
|  2201593 | 6846 | `			pVm->pIdleCallFrames = (void *)pRec;` |
|  2201593 | 6847 | `			aInstr = sState.aInstr;` |
|  2201593 | 6848 | `			pStack = sState.pStack;` |
|  2201593 | 6849 | `			pTos = sState.pTos;` |
|  2201593 | 6850 | `			pc = sState.pc;` |
|        - | 6851 | `		}` |
|  2201593 | 6852 | `		if( rc == PH7_OK ){` |
|  1800585 | 6853 | `			pc++; /* the loop-bottom increment this OP_CALL missed */` |
|  1800585 | 6854 | `			goto VmLoopFetch;` |
|        - | 6855 | `		}` |
|        5 | 6856 | `	}` |
|  4685788 | 6857 | `}` |
|        - | 6858 |  |
