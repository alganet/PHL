# src/ph7/vm_exec.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3711/4204 lines (88.27%)

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
|      546 |   45 | `static int VmGrowOperandStack(ph7_vm *pVm, sxu32 nNeed,` |
|        - |   46 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |   47 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        4 |   48 | `{` |
|      550 |   49 | `	ph7_value *pOld = *ppStack;` |
|      550 |   50 | `	sxu32 nOldCap = pState->nStackCap;` |
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
|      550 |   64 | `	nReq = nNeed + pState->nStackOrig;` |
|      550 |   65 | `	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */` |
|      ! 0 |   66 | `		nReq = SXU32_HIGH;` |
|      ! 0 |   67 | `	}` |
|        - |   68 | `	/* SyMemBackendRealloc's size argument is sxu32, so the byte count` |
|        - |   69 | `	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack` |
|        - |   70 | `	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and` |
|        - |   71 | `	 * the init loop below would run off it. If the true requirement exceeds the` |
|        - |   72 | `	 * representable cap, treat it as OOM (the caller raises the guard error). */` |
|      550 |   73 | `	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);` |
|      550 |   74 | `	if( nReq > nMaxCap ){` |
|      ! 0 |   75 | `		return 0;` |
|        - |   76 | `	}` |
|      550 |   77 | `	if( nReq <= nOldCap ){` |
|      214 |   78 | `		return 1; /* already fits — no growth needed */` |
|        - |   79 | `	}` |
|      340 |   80 | `	nNewCap = nReq;` |
|        - |   81 | `	/* Amortize repeated spreads in one argument list: at least double, but never` |
|        - |   82 | `	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't` |
|        - |   83 | `	 * itself overflow.) */` |
|      340 |   84 | `	if( nNewCap < nOldCap * 2 ){` |
|      340 |   85 | `		sxu32 nDbl = nOldCap * 2;` |
|      340 |   86 | `		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }` |
|      340 |   87 | `		if( nNewCap < nDbl ){ nNewCap = nDbl; }` |
|      168 |   88 | `	}` |
|      508 |   89 | `	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,` |
|      168 |   90 | `		nNewCap * sizeof(ph7_value));` |
|      340 |   91 | `	if( pNew == 0 ){` |
|      ! 0 |   92 | `		return 0; /* OOM: caller keeps pOld and raises the guard error */` |
|        - |   93 | `	}` |
|        - |   94 | `	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */` |
|    20560 |   95 | `	for( i = nOldCap; i < nNewCap; i++ ){` |
|    20224 |   96 | `		PH7_MemObjInit(pVm, &pNew[i]);` |
|    20224 |   97 | `		pNew[i].nIdx = SXU32_HIGH;` |
|    10114 |   98 | `	}` |
|        - |   99 | `	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */` |
|      340 |  100 | `	*ppTos = pNew + (*ppTos - pOld);` |
|      340 |  101 | `	*ppStack = pNew;` |
|      340 |  102 | `	pState->pStack = pNew + (pState->pStack - pOld);` |
|      340 |  103 | `	pState->pTos = pNew + (pState->pTos - pOld);` |
|      340 |  104 | `	pState->nStackCap = nNewCap;` |
|      340 |  105 | `	if( pCallTop ){` |
|        - |  106 | `		/* This activation is a trampoline callee: its record owns the buffer. */` |
|      292 |  107 | `		pCallTop->sCall.pFrameStack = pNew;` |
|      292 |  108 | `		pCallTop->sCall.nStackCap = nNewCap;` |
|      147 |  109 | `	}else{` |
|        - |  110 | `		/* Base activation: the native entry frees *ppBaseOwner (and, for a` |
|        - |  111 | `		 * resumable coroutine, persists *pnBaseCap across suspend/resume). */` |
|       50 |  112 | `		if( ppBaseOwner ){ *ppBaseOwner = pNew; }` |
|       50 |  113 | `		if( pnBaseCap ){ *pnBaseCap = nNewCap; }` |
|        - |  114 | `	}` |
|        - |  115 | `	/* Re-anchor this activation's captured spread runs (pStart into the old` |
|        - |  116 | `	 * buffer). Enclosing-activation runs live in other buffers — leave them. */` |
|      340 |  117 | `	nRun = SySetUsed(&pVm->aSpreadRun);` |
|      340 |  118 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      340 |  119 | `	for( i = 0; i < nRun; i++ ){` |
|      ! 0 |  120 | `		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){` |
|      ! 0 |  121 | `			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);` |
|      ! 0 |  122 | `		}` |
|      ! 0 |  123 | `	}` |
|      340 |  124 | `	return 1;` |
|      277 |  125 | `}` |
|        - |  126 | `/*` |
|        - |  127 | ` * Ensure the running activation's operand stack can hold a spread of nEntry` |
|        - |  128 | ` * elements pushed at *ppTos (the source slot becomes the first element, so the` |
|        - |  129 | ` * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.` |
|        - |  130 | ` * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.` |
|        - |  131 | ` */` |
|      612 |  132 | `static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,` |
|        - |  133 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |  134 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        4 |  135 | `{` |
|        - |  136 | `	sxu32 nNeed;` |
|      616 |  137 | `	if( nEntry == 0 ){` |
|       69 |  138 | `		return 1; /* empty spread never grows the stack */` |
|        - |  139 | `	}` |
|      550 |  140 | `	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);` |
|      823 |  141 | `	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,` |
|      273 |  142 | `		pCallTop, ppBaseOwner, pnBaseCap);` |
|      310 |  143 | `}` |
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
|  3384517 |  164 | `static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)` |
|        5 |  165 | `{` |
|  3384522 |  166 | `	if( rcTerm != PH7_SUSPEND ){` |
|  3383140 |  167 | `		VmDiscardFinallyActions(&(*pVm),pState->nFinallyActBase);` |
|  1691565 |  168 | `	}` |
|  3384522 |  169 | `	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){` |
|  1927588 |  170 | `		VmClearFramePending(pState->pEntryFrame);` |
|   963789 |  171 | `	}` |
|  3384522 |  172 | `	SySetRelease(pArg);` |
|  3384522 |  173 | `	if( rcTerm == PH7_ABORT \|\| rcTerm == PH7_EXCEPTION ){` |
|   805181 |  174 | `		while( pTos >= pState->pStack ){` |
|   403265 |  175 | `			PH7_MemObjRelease(pTos);` |
|   403265 |  176 | `			pTos--;` |
|        5 |  177 | `		}` |
|   200958 |  178 | `	}` |
|  3384522 |  179 | `	return rcTerm;` |
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
|  4141971 |  195 | `static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase)` |
|        5 |  196 | `{` |
|  4141986 |  197 | `	while( SySetUsed(&pVm->aFinallyAction) > nBase ){` |
|       13 |  198 | `		VmFinallyAction *pAct = (VmFinallyAction *)SySetPeek(&pVm->aFinallyAction);` |
|       13 |  199 | `		if( pAct->eKind == PH7_FA_RETHROW && pAct->pExc ){` |
|        7 |  200 | `			PH7_ClassInstanceUnref(pAct->pExc);` |
|        9 |  201 | `		}else if( pAct->eKind == PH7_FA_RETURN ){` |
|        3 |  202 | `			PH7_MemObjRelease(&pAct->sRet);` |
|        1 |  203 | `		}` |
|       13 |  204 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|        3 |  205 | `	}` |
|  4141976 |  206 | `}` |
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
|   763314 |  219 | `static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)` |
|        5 |  220 | `{` |
|        - |  221 | `	ph7_value *pObj;` |
|        - |  222 | `	/* Decrement nesting level */` |
|   763319 |  223 | `	pVm->nRecursionDepth--;` |
|   763319 |  224 | `	if( pCallee->bSelfPushed ){` |
|        - |  225 | `		/* Pop class name */` |
|   505649 |  226 | `		(void)SySetPop(&pVm->aSelf);` |
|   252822 |  227 | `	}` |
|   763319 |  228 | `	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  229 | `		/* Return by reference,reflect that */` |
|       65 |  230 | `		if( pCallee->nLastRef != SXU32_HIGH ){` |
|       65 |  231 | `			VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pCallee->pFrame->sLocal);` |
|        - |  232 | `			sxu32 i;` |
|        - |  233 | `			/* Make sure the referenced object is not a local variable */` |
|      119 |  234 | `			for( i = 0 ; i < SySetUsed(&pCallee->pFrame->sLocal) ; ++i ){` |
|       57 |  235 | `				if( pCallee->nLastRef == aSlot[i].nIdx ){` |
|      ! 0 |  236 | `					pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pCallee->nLastRef);` |
|      ! 0 |  237 | `					if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  238 | `						VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  239 | `							"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  240 | `							&pCallee->pVmFunc->sName);` |
|      ! 0 |  241 | `					}` |
|      ! 0 |  242 | `					pCallee->nLastRef = SXU32_HIGH;` |
|      ! 0 |  243 | `					break;` |
|        - |  244 | `				}` |
|       30 |  245 | `			}` |
|       34 |  246 | `		}else{` |
|      ! 0 |  247 | `			if( (pCaller->pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  248 | `				VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  249 | `					"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  250 | `					&pCallee->pVmFunc->sName);` |
|      ! 0 |  251 | `			}` |
|        - |  252 | `		}` |
|       65 |  253 | `		pCaller->pTos->nIdx = pCallee->nLastRef;` |
|       34 |  254 | `	}else{` |
|        - |  255 | `		/* A by-VALUE return is a TEMPORARY — php's IS_TMP_VAR — and must not look like` |
|        - |  256 | `		 * an lvalue. The result lands in the slot the call's first ARGUMENT occupied,` |
|        - |  257 | `		 * which still carried that argument's variable index, so the returned value` |
|        - |  258 | ``		 * inherited it: `f(id($z))` with `function f(&$x)` aliased and overwrote `$z`,`` |
|        - |  259 | `		 * a variable neither function was given by reference. Every call form was` |
|        - |  260 | `		 * affected (function, method, static, closure, nested) and every one of them` |
|        - |  261 | `		 * silently. Clearing it here also lets the call site see the temporary for what` |
|        - |  262 | `		 * it is, which is what php's "Only variables should be passed by reference"` |
|        - |  263 | `		 * notice is raised on. */` |
|   763257 |  264 | `		pCaller->pTos->nIdx = SXU32_HIGH;` |
|        - |  265 | `	}` |
|   763319 |  266 | `	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  267 | `		/* The callee threw (or its finally threw past it). If an in-place catch` |
|        - |  268 | `		 * recorded a resume target owned by THIS caller's body, resume there and` |
|        - |  269 | `		 * consume the target (VmRecordedResume); when the catcher is an outer exec` |
|        - |  270 | `		 * — or this is a callback with no bytecode to resume into — propagate so` |
|        - |  271 | `		 * the owning exec lands. This replaces the old "is the caller's parent a` |
|        - |  272 | `		 * resumable try frame" test, which resumed at the caller's OWN try even` |
|        - |  273 | `		 * when the finally's throw was caught further out, losing that catch's` |
|        - |  274 | `		 * return (ROOT B, face c). */` |
|        - |  275 | `		sxi32 iResumePc;` |
|   607965 |  276 | `		VmFrame *pParentFrame = pCallee->pFrame->pParent;` |
|   607965 |  277 | `		if( !pCaller->is_callback && pVm->pInlineInstr == (void *)pCaller->aInstr ){` |
|        - |  278 | `			/* ROOT C: the callee's throw was caught by an inline try in THIS caller` |
|        - |  279 | `			 * (generator body). Drain the operand stack (incl. the unwritten result` |
|        - |  280 | `			 * slot) to the try's base and land at its catch/finally. */` |
|      ! 0 |  281 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iInlineDrain ){` |
|      ! 0 |  282 | `				PH7_MemObjRelease(pCaller->pTos);` |
|      ! 0 |  283 | `				pCaller->pTos--;` |
|      ! 0 |  284 | `			}` |
|      ! 0 |  285 | `			pCaller->pc = (sxi32)pVm->iInlinePc - 1;` |
|      ! 0 |  286 | `			pVm->pInlineInstr = 0;` |
|      ! 0 |  287 | `			rc = PH7_OK;` |
|   607965 |  288 | `		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){` |
|        - |  289 | `			/* Pop the result, then drain any abandoned outer-expression operands` |
|        - |  290 | `			 * to the catching try's base (like the inline branch above) — the` |
|        - |  291 | `			 * ops that would have consumed them were abandoned by the throw, and` |
|        - |  292 | `` 			 * leaving them leaks one slot per caught throw (`try { $a = 1 + f(); }` `` |
|        - |  293 | `			 * in a loop overflowed the operand stack). */` |
|   207247 |  294 | `			VmPopOperand(&pCaller->pTos,1);` |
|   812911 |  295 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iResumeStackDepth ){` |
|   605669 |  296 | `				PH7_MemObjRelease(pCaller->pTos);` |
|   605669 |  297 | `				pCaller->pTos--;` |
|        5 |  298 | `			}` |
|   207247 |  299 | `			pCaller->pc = iResumePc;` |
|   207247 |  300 | `			rc = PH7_OK;` |
|   103626 |  301 | `		}else{` |
|   400723 |  302 | `			if( pParentFrame->pParent ){` |
|   400719 |  303 | `				rc = PH7_EXCEPTION;` |
|   200362 |  304 | `			}else{` |
|        - |  305 | `				/* Continue normal execution */` |
|        6 |  306 | `				rc = PH7_OK;` |
|        - |  307 | `			}` |
|        - |  308 | `		}` |
|   303980 |  309 | `	}` |
|        - |  310 | `	/* Recycle the operand stack for the next same-size call (BYTECODE stage 7),` |
|        - |  311 | `	 * or free it if the pool is full. Its allocated size is tracked in` |
|        - |  312 | `	 * pCallee->nStackCap (nMaxStack + VM_STACK_GUARD, or larger if an OP_SPREAD grew` |
|        - |  313 | `	 * it) — exactly what the buffer holds. (NULL when the function body was skipped.)` |
|        - |  314 | `	 *` |
|        - |  315 | `	 * Never on rc == PH7_SUSPEND: that path (unreachable in the stage-4 model,` |
|        - |  316 | `	 * where a deep suspend parks its whole record segment before reaching here)` |
|        - |  317 | `	 * would leave the callee stack owned by the suspended ctx, so recycling it` |
|        - |  318 | `	 * would hand a live fiber's operand stack to the next call. The guard keeps` |
|        - |  319 | `	 * that invariant explicit and robust to future coroutine changes. */` |
|   763319 |  320 | `	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){` |
|        - |  321 | `		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee` |
|        - |  322 | `		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly` |
|        - |  323 | `		 * the allocated slot count either way. */` |
|   758841 |  324 | `		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap);` |
|   379640 |  325 | `	}` |
|        - |  326 | `	/* Leave the frame */` |
|   763319 |  327 | `	VmLeaveFrame(&(*pVm));` |
|   763319 |  328 | `	if( rc == PH7_ABORT ){` |
|      397 |  329 | `		return PH7_ABORT;` |
|        - |  330 | `	}` |
|   762927 |  331 | `	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  332 | `		/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  333 | `		 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  334 | `		 * overwriting the state saved by the inner level.` |
|        - |  335 | `		 * pTos points to the result slot (not yet written).` |
|        - |  336 | `		 * Save nTos one below so resume pushes at the result slot. */` |
|      ! 0 |  337 | `		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);` |
|      ! 0 |  338 | `		return PH7_SUSPEND;` |
|        - |  339 | `	}` |
|   762927 |  340 | `	if( rc == PH7_EXCEPTION ){` |
|   400719 |  341 | `		return PH7_EXCEPTION;` |
|        - |  342 | `	}` |
|   362213 |  343 | `	return PH7_OK;` |
|   381884 |  344 | `}` |
|        - |  345 | `/*` |
|        - |  346 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  347 | ` *` |
|        - |  348 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  349 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  350 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  351 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  352 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  353 | ` * then the program execution is halted.` |
|        - |  354 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  355 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  356 | ` * or to reset the VM to it's initial state.` |
|        - |  357 | ` */` |
|        - |  358 | `static sxi32 VmByteCodeExecBody(ph7_vm *pVm,VmInstr *aInstr,ph7_value *pStack,int nTos,` |
|        - |  359 | `	ph7_value *pResult,sxu32 *pLastRef,int is_callback,sxi32 nPc,` |
|        - |  360 | `	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,VmParkedSegment *pAdoptSegment,` |
|        - |  361 | `	ph7_value **ppBaseOwner,sxu32 *pnBaseCap,sxu32 nStackOrig);` |
|        - |  362 | `/*` |
|        - |  363 | ` * Native-nesting guard around the executor. PHP->PHP calls run iteratively` |
|        - |  364 | ` * (the stage-2 trampoline), but every OTHER (re-)entry — mini-programs,` |
|        - |  365 | ` * C->PHP callbacks, ctx start/resume, eval/include — is still one real C` |
|        - |  366 | ` * activation of VmByteCodeExecBody. nMaxDepth no longer bounds them (it is` |
|        - |  367 | ` * PHP call depth, raisable to memory-bound values since the clamp removal),` |
|        - |  368 | ` * so this counter is what actually protects the C stack: recursive` |
|        - |  369 | ` * eval/include towers, nested coroutine-resume chains and self-recursive` |
|        - |  370 | ` * C-callback compositions hit a clean fatal instead of overflowing. The limit` |
|        - |  371 | ` * lives in pVm->nMaxNativeDepth — a per-platform default (256 host / 16 small-` |
|        - |  372 | ` * stack embedders, VmInit) overridable via PH7_VM_CONFIG_NATIVE_DEPTH. This is` |
|        - |  373 | ` * still a coarse frame-count net rather than php's stack-byte measurement, so` |
|        - |  374 | ` * the host default is conservative — well below the old config clamp's <1024` |
|        - |  375 | ` * ceiling so it holds on the fattest frames (the callback path drags in` |
|        - |  376 | ` * usort/mergesort/trampoline C frames per re-entry, and instrumented builds` |
|        - |  377 | ` * inflate every frame), while far beyond any realistic eval/include/callback` |
|        - |  378 | ` * nesting.` |
|        - |  379 | ` */` |
|  3384823 |  380 | `PH7_PRIVATE sxi32 VmByteCodeExec(` |
|        - |  381 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  382 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  383 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  384 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  385 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  386 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  387 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  388 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  389 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  390 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|        - |  391 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|        - |  392 | `	ph7_value **ppBaseOwner, /* Storage slot the native entry frees for this invocation's BASE (pCallTop==0) operand stack — a local, pVm->aOps or pCtx->pStack. An OP_SPREAD that grows the base stack writes the new pointer here so the entry frees the right buffer. */` |
|        - |  393 | `	sxu32 *pnBaseCap, /* Storage for the base stack's capacity (resumable coroutines persist it across suspend/resume); updated alongside *ppBaseOwner on base-stack growth. Also the initial capacity read at entry. */` |
|        - |  394 | `	sxu32 nStackOrig /* The base stack's ORIGINAL (ungrown) allocation size. Unlike *pnBaseCap (which is the CURRENT, possibly-grown capacity on a coroutine resume), this is fixed, so OP_SPREAD growth headroom stays bounded across resumes. */` |
|        - |  395 | `	)` |
|        5 |  396 | `{` |
|        - |  397 | `	sxi32 rc;` |
|        - |  398 | `	sxi32 nSavedBrc;` |
|        - |  399 | `	sxu32 nSavedLine;` |
|  3384828 |  400 | `	if( VmNativeNestingExceeded(pVm) ){` |
|        5 |  401 | `		return VmNativeNestingFatal(pVm);` |
|        - |  402 | `	}` |
|        - |  403 | `	/* A fresh native exec entered while a C-boundary throw is parked` |
|        - |  404 | `	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the` |
|        - |  405 | `	 * very opcode that swallowed the throw) must run CLEAN: the parked status` |
|        - |  406 | `	 * belongs to the interrupted outer exec's fetch-point router, not to this` |
|        - |  407 | `	 * one. Save+clear on entry, merge back on exit — the outer status is` |
|        - |  408 | `	 * restored unless this exec parked its own unconsumed (newer) one, with` |
|        - |  409 | `	 * PH7_ABORT dominating either way. */` |
|  3384824 |  410 | `	nSavedBrc = pVm->nBoundaryRc;` |
|  3384824 |  411 | `	pVm->nBoundaryRc = 0;` |
|        - |  412 | `	/* The executing source line belongs to the ACTIVATION. A nested body -- a called` |
|        - |  413 | `	 * function, but equally an attribute-default or default-argument mini-program --` |
|        - |  414 | `	 * runs its own bytecode with its own lines, so it must not leave the caller` |
|        - |  415 | ``	 * reporting the callee's position: `new Exception` stamped line 1 because the`` |
|        - |  416 | ``	 * class's `protected $message = '';` default ran (from the embedded chunk) between`` |
|        - |  417 | `	 * OP_NEW and the stamp. Save on entry, restore on exit. */` |
|  3384824 |  418 | `	nSavedLine = pVm->nCurLine;` |
|  3384824 |  419 | `	pVm->nVmExecDepth++;` |
|  5077231 |  420 | `	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,` |
|  1692407 |  421 | `		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);` |
|  3384824 |  422 | `	pVm->nVmExecDepth--;` |
|  3384824 |  423 | `	pVm->nCurLine = nSavedLine;` |
|  3384824 |  424 | `	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT \|\| pVm->nBoundaryRc == 0) ){` |
|       38 |  425 | `		pVm->nBoundaryRc = nSavedBrc;` |
|       17 |  426 | `	}` |
|  3384824 |  427 | `	return rc;` |
|  1692414 |  428 | `}` |
|        - |  429 | `/*` |
|        - |  430 | ` * D1 commit 2: allocate a captured lvalue path for a deferred element/property call arg.` |
|        - |  431 | ` * eRoot picks the root container: 0 = a real aMemObj slot (nRootIdx), 1 = an undefined` |
|        - |  432 | ` * variable to vivify by name at resolve time (pName, a VM-lifetime bytecode string, borrowed),` |
|        - |  433 | ` * 2 = a string base (subscripting a string -> php refuses a by-ref bind). The struct owns its` |
|        - |  434 | ` * step array and each step's key/name; PH7_MemObjRelease frees it via VmFreeDeferredPath.` |
|        - |  435 | ` */` |
|    45264 |  436 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName)` |
|        5 |  437 | `{` |
|    45269 |  438 | `	VmDeferredPath *pPath = (VmDeferredPath *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDeferredPath));` |
|    45269 |  439 | `	if( pPath == 0 ){` |
|      ! 0 |  440 | `		return 0;` |
|        - |  441 | `	}` |
|    45269 |  442 | `	SyZero(pPath,sizeof(VmDeferredPath));` |
|    45269 |  443 | `	pPath->pAlloc = &pVm->sAllocator;` |
|    45269 |  444 | `	pPath->eRoot = eRoot;` |
|    45269 |  445 | `	pPath->nRootIdx = nRootIdx;` |
|    45269 |  446 | `	if( eRoot == 1 && pName ){` |
|        6 |  447 | `		pPath->sRootName = *pName; /* borrowed VM-lifetime bytes, not copied */` |
|        2 |  448 | `	}` |
|    45269 |  449 | `	return pPath;` |
|    22806 |  450 | `}` |
|        - |  451 | `/*` |
|        - |  452 | ` * A carrier for a fetch that ALREADY HAPPENED: an overloaded container answered with a` |
|        - |  453 | ` * value, and only the by-ref verdict is still pending (VM_DEFER_ROOT_PREFETCH). Takes a` |
|        - |  454 | ` * copy of the value; the caller keeps its own.` |
|        - |  455 | ` */` |
|      216 |  456 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNewPrefetch(ph7_vm *pVm,int nKind,ph7_class *pClass,` |
|        - |  457 | `	const SyString *pName,ph7_value *pVal)` |
|        4 |  458 | `{` |
|      220 |  459 | `	VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),VM_DEFER_ROOT_PREFETCH,SXU32_HIGH,0);` |
|      220 |  460 | `	if( pPath == 0 ){` |
|      ! 0 |  461 | `		return 0;` |
|        - |  462 | `	}` |
|      220 |  463 | `	pPath->nOverKind = (sxu8)nKind;` |
|      220 |  464 | `	pPath->pOverClass = pClass;` |
|      220 |  465 | `	if( pName && pName->nByte > 0 ){` |
|      174 |  466 | `		pPath->zOverName = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|      174 |  467 | `		if( pPath->zOverName == 0 ){` |
|      ! 0 |  468 | `			VmFreeDeferredPath(pPath);` |
|      ! 0 |  469 | `			return 0;` |
|        - |  470 | `		}` |
|      174 |  471 | `		SyStringInitFromBuf(&pPath->sOverName,pPath->zOverName,pName->nByte);` |
|       85 |  472 | `	}` |
|      220 |  473 | `	PH7_MemObjInit(&(*pVm),&pPath->sPrefetch);` |
|      220 |  474 | `	PH7_MemObjStore(pVal,&pPath->sPrefetch);` |
|      220 |  475 | `	return pPath;` |
|      112 |  476 | `}` |
|        - |  477 | `/*` |
|        - |  478 | ` * The verdict a prefetched value gets when the callee turns out to want it BY REFERENCE:` |
|        - |  479 | ` * php asked the object for something to modify and it could only answer with a value.` |
|        - |  480 | ` * Two of the three are notices php carries on from; a HOOKED property is the one php` |
|        - |  481 | ` * refuses outright. All three stay silent for a by-VALUE parameter, which is the whole` |
|        - |  482 | ` * reason the fetch could not decide them itself.` |
|        - |  483 | ` */` |
|       34 |  484 | `static sxi32 VmPrefetchByRefVerdict(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pVal)` |
|        1 |  485 | `{` |
|       35 |  486 | `	if( pPath->nOverKind == VM_OVER_PROP ){` |
|       15 |  487 | `		PH7_VmOverloadedPropNotice(&(*pVm),pPath->pOverClass,&pPath->sOverName,pVal);` |
|       15 |  488 | `		return SXRET_OK;` |
|        - |  489 | `	}` |
|       21 |  490 | `	if( pPath->nOverKind == VM_OVER_HOOK ){` |
|        - |  491 | `		SyBlob sErrMsg;` |
|        - |  492 | `		sxi32 rcH;` |
|        5 |  493 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 |  494 | `		SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|        4 |  495 | `			&pPath->pOverClass->sName,&pPath->sOverName);` |
|        5 |  496 | `		rcH = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg);` |
|        5 |  497 | `		SyBlobRelease(&sErrMsg);` |
|        5 |  498 | `		return (rcH == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  499 | `	}` |
|       17 |  500 | `	PH7_VmOverloadedElemNotice(&(*pVm),pPath->pOverClass,pVal);` |
|       17 |  501 | `	return SXRET_OK;` |
|       18 |  502 | `}` |
|    45086 |  503 | `static VmDeferStep * VmDeferPathGrow(VmDeferredPath *pPath)` |
|        5 |  504 | `{` |
|    45091 |  505 | `	if( pPath->nStep >= pPath->nAlloc ){` |
|    45083 |  506 | `		sxu32 nNew = pPath->nAlloc ? pPath->nAlloc * 2 : 4;` |
|    67791 |  507 | `		VmDeferStep *aNew = (VmDeferStep *)SyMemBackendRealloc(pPath->pAlloc,pPath->aStep,` |
|    22708 |  508 | `			nNew * sizeof(VmDeferStep));` |
|    45083 |  509 | `		if( aNew == 0 ){` |
|      ! 0 |  510 | `			return 0;` |
|        - |  511 | `		}` |
|    45083 |  512 | `		pPath->aStep = aNew;` |
|    45083 |  513 | `		pPath->nAlloc = nNew;` |
|    22708 |  514 | `	}` |
|    45091 |  515 | `	return &pPath->aStep[pPath->nStep];` |
|    22717 |  516 | `}` |
|        - |  517 | `/* Append an array-element step, deep-copying the index value (the caller releases pKey). */` |
|    45018 |  518 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey)` |
|        5 |  519 | `{` |
|    45023 |  520 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|    45023 |  521 | `	if( pStep == 0 ){` |
|      ! 0 |  522 | `		return SXERR_MEM;` |
|        - |  523 | `	}` |
|    45023 |  524 | `	pStep->isProp = 0;` |
|    45023 |  525 | `	pStep->bAppend = 0;` |
|    45023 |  526 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|    45023 |  527 | `	PH7_MemObjInit(pKey->pVm,&pStep->sKey);` |
|    45023 |  528 | `	PH7_MemObjStore(pKey,&pStep->sKey);` |
|    45023 |  529 | `	pPath->nStep++;` |
|    45023 |  530 | `	return SXRET_OK;` |
|    22683 |  531 | `}` |
|        - |  532 | ``/* Append a KEYLESS element step — the `[]` of `f($a[])`. It carries no index at all,`` |
|        - |  533 | ` * so the two resolvers differ on it: by reference it creates the next element (php` |
|        - |  534 | `` * binds the parameter to it), by value it is php's `Cannot use [] for reading`. */`` |
|        8 |  535 | `PH7_PRIVATE sxi32 VmDeferPathPushAppend(VmDeferredPath *pPath)` |
|        1 |  536 | `{` |
|        9 |  537 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|        9 |  538 | `	if( pStep == 0 ){` |
|      ! 0 |  539 | `		return SXERR_MEM;` |
|        - |  540 | `	}` |
|        9 |  541 | `	pStep->isProp = 0;` |
|        9 |  542 | `	pStep->bAppend = 1;` |
|        9 |  543 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|        9 |  544 | `	SyZero((void *)&pStep->sKey,sizeof(ph7_value));` |
|        9 |  545 | `	pPath->nStep++;` |
|        9 |  546 | `	return SXRET_OK;` |
|        5 |  547 | `}` |
|        - |  548 | `/* Append an object-property step, owning a private copy of the name bytes. */` |
|       60 |  549 | `PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName)` |
|        3 |  550 | `{` |
|       63 |  551 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|        - |  552 | `	char *zCopy;` |
|       63 |  553 | `	if( pStep == 0 ){` |
|      ! 0 |  554 | `		return SXERR_MEM;` |
|        - |  555 | `	}` |
|       63 |  556 | `	zCopy = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|       63 |  557 | `	if( zCopy == 0 ){` |
|      ! 0 |  558 | `		return SXERR_MEM;` |
|        - |  559 | `	}` |
|       63 |  560 | `	pStep->isProp = 1;` |
|       63 |  561 | `	pStep->bAppend = 0;` |
|       63 |  562 | `	pStep->zProp = zCopy;` |
|       63 |  563 | `	SyStringInitFromBuf(&pStep->sProp,zCopy,pName->nByte);` |
|       63 |  564 | `	pPath->nStep++;` |
|       63 |  565 | `	return SXRET_OK;` |
|       33 |  566 | `}` |
|        - |  567 | `/* Release a captured lvalue path and everything it owns (element keys, property names). */` |
|        - |  568 | `/*` |
|        - |  569 | `` * The pending offset of a `$s[k] ??= v`: a heap copy of the RAW key, owned by the`` |
|        - |  570 | ` * peek's MEMOBJ_AUX_COALSTROFF result on the operand stack. One carrier per` |
|        - |  571 | `` * pending ??=, so `$s[9] ??= ($t[9] ??= "q")` nests — a single VM-wide slot could`` |
|        - |  572 | ` * not (the inner peek overwrote the outer's offset, and the outer store then` |
|        - |  573 | ` * replaced the whole string).` |
|        - |  574 | ` */` |
|       52 |  575 | `PH7_PRIVATE VmCoalStrOff * VmCoalStrOffNew(ph7_vm *pVm,ph7_value *pKey)` |
|        1 |  576 | `{` |
|       53 |  577 | `	VmCoalStrOff *pCoal = (VmCoalStrOff *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmCoalStrOff));` |
|       53 |  578 | `	if( pCoal == 0 ){` |
|      ! 0 |  579 | `		return 0;` |
|        - |  580 | `	}` |
|       53 |  581 | `	pCoal->pAlloc = &pVm->sAllocator;` |
|       53 |  582 | `	PH7_MemObjInit(&(*pVm),&pCoal->sKey);` |
|       53 |  583 | `	if( pKey ){` |
|       53 |  584 | `		PH7_MemObjStore(pKey,&pCoal->sKey);` |
|       26 |  585 | `	}` |
|       53 |  586 | `	return pCoal;` |
|       27 |  587 | `}` |
|       86 |  588 | `PH7_PRIVATE void VmFreeCoalStrOff(VmCoalStrOff *pCoal)` |
|        4 |  589 | `{` |
|        - |  590 | `	SyMemBackend *pAlloc;` |
|       90 |  591 | `	if( pCoal == 0 ){` |
|       38 |  592 | `		return;` |
|        - |  593 | `	}` |
|       53 |  594 | `	pAlloc = pCoal->pAlloc;` |
|       53 |  595 | `	PH7_MemObjRelease(&pCoal->sKey);` |
|       53 |  596 | `	SyMemBackendFree(pAlloc,pCoal);` |
|       47 |  597 | `}` |
|        - |  598 | `/*` |
|        - |  599 | ` * Build the pending __call/__callStatic routing OP_MEMBER hands to the OP_CALL that` |
|        - |  600 | ` * follows it, and hang it off the marked carrier slot. One record per routed call, so a` |
|        - |  601 | ` * routed call evaluated inside another routed call's ARGUMENT LIST — which is where they` |
|        - |  602 | ` * now sit, php's order — keeps its own {receiver, class, name}. Takes the receiver` |
|        - |  603 | ` * reference; the record owns it from here.` |
|        - |  604 | ` */` |
|      126 |  605 | `PH7_PRIVATE VmMagicCall * VmMagicCallNew(ph7_vm *pVm,ph7_class_instance *pRecv,` |
|        - |  606 | `	ph7_class *pClass,const SyString *pName)` |
|        4 |  607 | `{` |
|      130 |  608 | `	VmMagicCall *pPend = (VmMagicCall *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmMagicCall));` |
|      130 |  609 | `	if( pPend == 0 ){` |
|      ! 0 |  610 | `		return 0;` |
|        - |  611 | `	}` |
|      130 |  612 | `	pPend->pAlloc = &pVm->sAllocator;` |
|      130 |  613 | `	pPend->pRecv = pRecv;` |
|      130 |  614 | `	pPend->pClass = pClass;` |
|      130 |  615 | `	SyBlobInit(&pPend->sName,&pVm->sAllocator);` |
|      130 |  616 | `	if( pName && pName->nByte > 0 ){` |
|      130 |  617 | `		SyBlobAppend(&pPend->sName,(const void *)pName->zString,pName->nByte);` |
|       63 |  618 | `	}` |
|      130 |  619 | `	if( pRecv ){` |
|       88 |  620 | `		pRecv->iRef++;` |
|       42 |  621 | `	}` |
|      130 |  622 | `	return pPend;` |
|       67 |  623 | `}` |
|      126 |  624 | `PH7_PRIVATE void VmFreeMagicCall(VmMagicCall *pPend)` |
|        4 |  625 | `{` |
|        - |  626 | `	SyMemBackend *pAlloc;` |
|      130 |  627 | `	if( pPend == 0 ){` |
|      ! 0 |  628 | `		return;` |
|        - |  629 | `	}` |
|      130 |  630 | `	pAlloc = pPend->pAlloc;` |
|      130 |  631 | `	if( pPend->pRecv ){` |
|      ! 0 |  632 | `		PH7_ClassInstanceUnref(pPend->pRecv);` |
|      ! 0 |  633 | `	}` |
|      130 |  634 | `	SyBlobRelease(&pPend->sName);` |
|      130 |  635 | `	SyMemBackendFree(pAlloc,pPend);` |
|       67 |  636 | `}` |
|    45264 |  637 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath)` |
|        5 |  638 | `{` |
|        - |  639 | `	sxu32 i;` |
|    45269 |  640 | `	if( pPath == 0 ){` |
|      ! 0 |  641 | `		return;` |
|        - |  642 | `	}` |
|    90355 |  643 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|    45091 |  644 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|    45091 |  645 | `		if( pStep->isProp ){` |
|       63 |  646 | `			if( pStep->zProp ){` |
|       63 |  647 | `				SyMemBackendFree(pPath->pAlloc,pStep->zProp);` |
|       33 |  648 | `			}` |
|    45061 |  649 | `		}else if( !pStep->bAppend ){` |
|        - |  650 | `			/* An append step holds no key at all — its sKey was never initialized. */` |
|    45023 |  651 | `			PH7_MemObjRelease(&pStep->sKey);` |
|    22678 |  652 | `		}` |
|    22717 |  653 | `	}` |
|    45269 |  654 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|      220 |  655 | `		PH7_MemObjRelease(&pPath->sPrefetch);` |
|      220 |  656 | `		if( pPath->zOverName ){` |
|      174 |  657 | `			SyMemBackendFree(pPath->pAlloc,pPath->zOverName);` |
|       85 |  658 | `		}` |
|      108 |  659 | `	}` |
|    45269 |  660 | `	if( pPath->aStep ){` |
|    45083 |  661 | `		SyMemBackendFree(pPath->pAlloc,pPath->aStep);` |
|    22708 |  662 | `	}` |
|    45269 |  663 | `	SyMemBackendFree(pPath->pAlloc,pPath);` |
|    22806 |  664 | `}` |
|        - |  665 | `/* Map an op-handler VmOpRc into the main-loop rc convention used by VmByteCodeExecBody. */` |
|    45056 |  666 | `static sxi32 VmOpRcToExecRc(VmOpRc rcOp)` |
|        5 |  667 | `{` |
|    45061 |  668 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 |  669 | `		return PH7_ABORT;` |
|        - |  670 | `	}` |
|    45059 |  671 | `	if( rcOp == VM_OP_EXCEPTION ){` |
|       12 |  672 | `		return PH7_EXCEPTION;` |
|        - |  673 | `	}` |
|    45049 |  674 | `	return SXRET_OK;` |
|    22702 |  675 | `}` |
|        - |  676 | `/*` |
|        - |  677 | ` * D1 commit 2: re-drive ONE captured lvalue step by invoking the real LOAD_IDX / MEMBER` |
|        - |  678 | ` * handler on a synthetic 2-slot stack. This reuses the proven COW / vivify / warning / magic` |
|        - |  679 | ` * machinery instead of hand-walking it. pBase carries the current container (its nIdx must be` |
|        - |  680 | ` * a real aMemObj slot for a by-ref write to vivify in place). iP2 selects the mode:` |
|        - |  681 | ` * LOAD_IDX 1=write(vivify,by-ref) / 0=read(by-value); MEMBER PH7_MEMBER_READ=by-value read.` |
|        - |  682 | ` * On success pOut receives the result value and its nIdx (the aliasable element slot for a` |
|        - |  683 | ` * vivified by-ref element).` |
|        - |  684 | ` */` |
|    45056 |  685 | `static sxi32 VmReDriveStep(ph7_vm *pVm,sxi32 iOp,sxu32 iP2,ph7_value *pBase,ph7_value *pKey,ph7_value *pOut)` |
|        5 |  686 | `{` |
|        - |  687 | `	ph7_value mini[2];` |
|        - |  688 | `	VmInstr aI[2];` |
|        - |  689 | `	VmExecState st;` |
|        - |  690 | `	VmOpRc rcOp;` |
|        - |  691 | ``	/* A NULL key is the APPEND form (`$a[]`): LOAD_IDX takes no index operand, so the`` |
|        - |  692 | `	 * base is the whole stack and iP1 says so. */` |
|    45061 |  693 | `	int bAppend = (pKey == 0);` |
|    45061 |  694 | `	PH7_MemObjInit(pVm,&mini[0]);` |
|    45061 |  695 | `	PH7_MemObjInit(pVm,&mini[1]);` |
|    45061 |  696 | `	PH7_MemObjLoad(pBase,&mini[0]);` |
|    45061 |  697 | `	mini[0].nIdx = pBase->nIdx;` |
|    45061 |  698 | `	if( !bAppend ){` |
|    45055 |  699 | `		PH7_MemObjStore(pKey,&mini[1]);` |
|    22694 |  700 | `	}` |
|    45061 |  701 | `	SyZero((void *)aI,sizeof(aI));` |
|        - |  702 | `	/* LOAD_IDX: iP1=1 means "an index is present". MEMBER: iP1=0 means an INSTANCE member` |
|        - |  703 | ``	 * (iP1=1 would be a static `::` access). */`` |
|    45061 |  704 | `	aI[0].iOp = (sxu8)iOp; aI[0].iP1 = (iOp == PH7_OP_LOAD_IDX && !bAppend) ? 1 : 0; aI[0].iP2 = iP2;` |
|    45061 |  705 | `	SyZero((void *)&st,sizeof(st));` |
|    45061 |  706 | `	st.pStack = mini; st.pTos = bAppend ? &mini[0] : &mini[1]; st.aInstr = aI; st.pc = 0;` |
|    45061 |  707 | `	if( iOp == PH7_OP_LOAD_IDX ){` |
|    45021 |  708 | `		rcOp = VmExecOpLoadIdx(&(*pVm),&st,&aI[0]);` |
|    22682 |  709 | `	}else{` |
|       43 |  710 | `		rcOp = VmExecOpMember(&(*pVm),&st,&aI[0]);` |
|        - |  711 | `	}` |
|        - |  712 | `	/* The handler popped the key/name and left the result in the (now top) base slot.` |
|        - |  713 | `	 * DEEP-COPY it into pOut (MemObjStore, not MemObjLoad): the result is chained as the next` |
|        - |  714 | `	 * step's base and must OWN its buffer — mini[0] is released immediately below, and a` |
|        - |  715 | `	 * read-only view (MemObjLoad) would leave pOut dangling into freed memory. */` |
|    45061 |  716 | `	PH7_MemObjStore(st.pTos,pOut);` |
|    45061 |  717 | `	pOut->nIdx = st.pTos->nIdx;` |
|    45061 |  718 | `	PH7_MemObjRelease(&mini[0]);` |
|    45061 |  719 | `	return VmOpRcToExecRc(rcOp);` |
|        5 |  720 | `}` |
|        - |  721 | `/*` |
|        - |  722 | ` * D1 commit 2: resolve an object property as a by-ref target. Given the object's aMemObj` |
|        - |  723 | ` * slot, return the property value's slot index in *pnOut so the by-ref binder can alias it.` |
|        - |  724 | ` * A present property binds directly; a missing one is created (recreate a declared+unset` |
|        - |  725 | ` * property, or a dynamic property on a dynamic-allowing class); a magic __get/__set property` |
|        - |  726 | ` * emits php's Notice and does NOT bind (*pbNoBind), handing __get's value back through` |
|        - |  727 | ` * pValOut (optional) for the by-VALUE pass php makes instead. Mirrors VmExecOpMember's` |
|        - |  728 | ` * write-create.` |
|        - |  729 | ` */` |
|       20 |  730 | `static sxi32 VmBindPropByRef(ph7_vm *pVm,ph7_value *pObj,const SyString *pName,sxu32 *pnOut,int *pbNoBind,ph7_value *pValOut)` |
|        3 |  731 | `{` |
|        - |  732 | `	ph7_class_instance *pThis;` |
|        - |  733 | `	ph7_class *pClass;` |
|        - |  734 | `	SyHashEntry *pEntry;` |
|       23 |  735 | `	VmClassAttr *pAttr = 0;` |
|       23 |  736 | `	*pbNoBind = 0;` |
|       23 |  737 | `	if( pObj == 0 ){` |
|        - |  738 | `		/* The root slot is gone (a detached frame): nothing to bind and nothing to say. */` |
|      ! 0 |  739 | `		*pbNoBind = 1;` |
|      ! 0 |  740 | `		return SXRET_OK;` |
|        - |  741 | `	}` |
|       23 |  742 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  743 | `		/* A non-object base has no property to alias, and php does not pass NULL and carry` |
|        - |  744 | `		 * on: asking one for something to MODIFY is its catchable Error, the same one every` |
|        - |  745 | `		 * other write shape through a null/int/string base raises. PHL warned about a READ` |
|        - |  746 | ``		 * it never performed and handed the by-ref parameter a NULL, so `f($u->p)` with`` |
|        - |  747 | ``		 * `function f(&$x)` wrote into nothing on a statement php stops the script for. The`` |
|        - |  748 | `		 * by-VALUE binding still takes the read warning — that half is php-exact — and is` |
|        - |  749 | `		 * what the value re-drive next door produces. */` |
|        - |  750 | `		SyBlob sErrM;` |
|        - |  751 | `		sxi32 rcErr;` |
|       13 |  752 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       13 |  753 | `		SyBlobFormat(&sErrM,"Attempt to modify property \"%z\" on %s",` |
|        6 |  754 | `			pName,VmArithValueName(pObj));` |
|       19 |  755 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|        6 |  756 | `			SyBlobLength(&sErrM));` |
|       13 |  757 | `		SyBlobRelease(&sErrM);` |
|       13 |  758 | `		*pbNoBind = 1;` |
|       13 |  759 | `		return (rcErr == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  760 | `	}` |
|       11 |  761 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|       11 |  762 | `	pClass = pThis->pClass;` |
|       11 |  763 | `	if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|        - |  764 | `		/* A by-reference argument asks the incomplete object for something to` |
|        - |  765 | `		 * MODIFY: php's catchable Error, the same one every direct write raises. */` |
|        - |  766 | `		SyBlob sIncErr;` |
|        - |  767 | `		sxi32 rcInc;` |
|        3 |  768 | `		SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|        3 |  769 | `		PH7_VmIncompleteMsg(&(*pVm),pThis,"modify a property",&sIncErr);` |
|        4 |  770 | `		rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|        1 |  771 | `			SyBlobLength(&sIncErr));` |
|        3 |  772 | `		SyBlobRelease(&sIncErr);` |
|        3 |  773 | `		*pbNoBind = 1;` |
|        3 |  774 | `		return (rcInc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  775 | `	}` |
|        9 |  776 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|        9 |  777 | `	if( pEntry ){` |
|        3 |  778 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        3 |  779 | `		if( (pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      ! 0 |  780 | `			*pnOut = pAttr->nIdx;` |
|      ! 0 |  781 | `			return SXRET_OK;` |
|        - |  782 | `		}` |
|        - |  783 | `		/* A static property is the CLASS's: php does not find it through an` |
|        - |  784 | ``		 * instance, so binding `f($o->s)` by reference must not hand out the`` |
|        - |  785 | `		 * class slot — that let a by-ref callee overwrite shared class state` |
|        - |  786 | `		 * through an object, and with no diagnostic at all (the value pass that` |
|        - |  787 | `		 * carries the notice at the fetch site never runs for a by-ref arg).` |
|        - |  788 | `		 * Notice here and fall through to the missing-property handling. */` |
|        4 |  789 | `		if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->pAttr->sName,` |
|        2 |  790 | `			pAttr->pAttr->iProtection,FALSE) ){` |
|        4 |  791 | `			VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  792 | `				"Accessing static property %z::$%z as non static",` |
|        1 |  793 | `				&pClass->sName,pName);` |
|        1 |  794 | `		}` |
|        3 |  795 | `		pAttr = 0;` |
|        1 |  796 | `	}` |
|        6 |  797 | `	if( PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|        9 |  798 | `	 \|\| PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) ){` |
|        - |  799 | `		/* Overloaded (magic) property: php passes it by-value with a Notice and drops the` |
|        - |  800 | `		 * write-back — "has no effect". The value it passes is __get's, which the caller` |
|        - |  801 | `		 * takes through pValOut; leaving the argument NULL instead turned` |
|        - |  802 | ``		 * `sort($o->magic)` — a statement php performs on a temporary — into`` |
|        - |  803 | ``		 * `sort(): Argument #1 ($array) must be of type array, null given`. */`` |
|      ! 0 |  804 | `		VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  805 | `			"Indirect modification of overloaded property %z::$%z has no effect",` |
|      ! 0 |  806 | `			&pClass->sName,pName);` |
|      ! 0 |  807 | `		*pbNoBind = 1;` |
|      ! 0 |  808 | `		if( pValOut && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|      ! 0 |  809 | `		 && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g') ){` |
|      ! 0 |  810 | `			VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|      ! 0 |  811 | `			PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pValOut);` |
|      ! 0 |  812 | `			VmMagicGuardPop(pVm);` |
|      ! 0 |  813 | `		}` |
|      ! 0 |  814 | `		return SXRET_OK;` |
|        - |  815 | `	}` |
|        - |  816 | `	{` |
|        9 |  817 | `		ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);` |
|        9 |  818 | `		if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      ! 0 |  819 | `			VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pAttr);` |
|        9 |  820 | `		}else if( VmClassAllowsDynamicProps(&(*pVm),pClass) ){` |
|        6 |  821 | `			PH7_VmCreateDynamicAttr(&(*pVm),pThis,pName->zString,pName->nByte,&pAttr);` |
|        4 |  822 | `		}else{` |
|        - |  823 | `			SyBlob sMsg;` |
|        - |  824 | `			sxi32 rcT;` |
|        3 |  825 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 |  826 | `			SyBlobFormat(&sMsg,"Cannot create dynamic property %z::$%z",&pClass->sName,pName);` |
|        3 |  827 | `			rcT = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        3 |  828 | `			SyBlobRelease(&sMsg);` |
|        3 |  829 | `			*pbNoBind = 1;` |
|        3 |  830 | `			return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  831 | `		}` |
|        6 |  832 | `		if( pAttr ){` |
|        6 |  833 | `			*pnOut = pAttr->nIdx;` |
|        4 |  834 | `		}else{` |
|      ! 0 |  835 | `			*pbNoBind = 1;` |
|        - |  836 | `		}` |
|        - |  837 | `	}` |
|        6 |  838 | `	return SXRET_OK;` |
|       13 |  839 | `}` |
|        - |  840 | `/*` |
|        - |  841 | ` * Walk a captured path's steps over SLOTS: each step vivifies in place and answers the` |
|        - |  842 | ` * next one, so the terminal slot is what the by-ref binder aliases. Split out of the` |
|        - |  843 | ` * resolver below because the VALUE walk hands control back to it — an accessor that` |
|        - |  844 | ` * answered with an OBJECT is a handle, not a temporary, and everything under it is` |
|        - |  845 | ` * addressable again.` |
|        - |  846 | ` */` |
|       80 |  847 | `static sxi32 VmWalkStepsFromSlot(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,sxu32 nCur,` |
|        - |  848 | `	ph7_value *pSlot)` |
|        3 |  849 | `{` |
|        - |  850 | `	sxu32 i;` |
|        - |  851 | `	sxi32 rc;` |
|      149 |  852 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|       85 |  853 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|       85 |  854 | `		if( pStep->isProp ){` |
|       21 |  855 | `			sxu32 nOut = SXU32_HIGH;` |
|       21 |  856 | `			int bNoBind = 0;` |
|        - |  857 | `			ph7_value sMagicVal;` |
|       21 |  858 | `			int bLastStep = (i + 1 == pPath->nStep);` |
|       21 |  859 | `			PH7_MemObjInit(&(*pVm),&sMagicVal);` |
|       39 |  860 | `			rc = VmBindPropByRef(&(*pVm),(ph7_value *)SySetAt(&pVm->aMemObj,nCur),` |
|       18 |  861 | `				&pStep->sProp,&nOut,&bNoBind,bLastStep ? &sMagicVal : 0);` |
|       21 |  862 | `			if( rc != SXRET_OK ){` |
|       18 |  863 | `				PH7_MemObjRelease(&sMagicVal);` |
|       18 |  864 | `				return rc;` |
|        - |  865 | `			}` |
|        3 |  866 | `			if( bNoBind ){` |
|        - |  867 | `				/* magic/non-object: nothing to alias, so the argument is passed BY` |
|        - |  868 | `				 * VALUE — which for an overloaded property is what __get answered,` |
|        - |  869 | `				 * not the NULL this used to leave behind. */` |
|      ! 0 |  870 | `				if( bLastStep ){` |
|      ! 0 |  871 | `					PH7_MemObjStore(&sMagicVal,pSlot);` |
|      ! 0 |  872 | `					pSlot->nIdx = SXU32_HIGH;` |
|      ! 0 |  873 | `				}` |
|      ! 0 |  874 | `				PH7_MemObjRelease(&sMagicVal);` |
|      ! 0 |  875 | `				return SXRET_OK;` |
|        - |  876 | `			}` |
|        3 |  877 | `			PH7_MemObjRelease(&sMagicVal);` |
|        3 |  878 | `			nCur = nOut;` |
|        2 |  879 | `		}else{` |
|        - |  880 | `			ph7_value out;` |
|       66 |  881 | `			ph7_value *pContainer = (ph7_value *)SySetAt(&pVm->aMemObj,nCur);` |
|       66 |  882 | `			if( pContainer == 0 ){` |
|      ! 0 |  883 | `				return SXRET_OK;` |
|        - |  884 | `			}` |
|       66 |  885 | `			PH7_MemObjInit(&(*pVm),&out);` |
|        - |  886 | ``			/* `f($a[])` bound to a by-reference parameter: php CREATES the next element`` |
|        - |  887 | `			 * and aliases the parameter to it. */` |
|       98 |  888 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,1,pContainer,` |
|       64 |  889 | `				pStep->bAppend ? 0 : &pStep->sKey,&out);` |
|       66 |  890 | `			nCur = out.nIdx;` |
|       66 |  891 | `			PH7_MemObjRelease(&out);` |
|       66 |  892 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  893 | `				return rc;` |
|        - |  894 | `			}` |
|       66 |  895 | `			if( nCur == SXU32_HIGH ){` |
|      ! 0 |  896 | `				return SXRET_OK; /* no aliasable slot (e.g. a non-lvalue container): pass by value */` |
|        - |  897 | `			}` |
|        - |  898 | `		}` |
|       35 |  899 | `	}` |
|        - |  900 | `	{` |
|        - |  901 | `		/* The terminal slot is what the by-ref binder aliases, but the argument also has` |
|        - |  902 | `		 * to CARRY the element's value: a builtin reads what it is handed and writes back` |
|        - |  903 | `		 * through the slot. That was invisible while a path was only ever captured on a` |
|        - |  904 | `		 * MISS — the vivified element is NULL and so was the carrier — and stopped being` |
|        - |  905 | `		 * true when a WRITABLE container's existing element started riding one` |
|        - |  906 | ``		 * (`sort($ao['a'])` reached sort() as NULL). */`` |
|       66 |  907 | `		ph7_value *pFinal = (ph7_value *)SySetAt(&pVm->aMemObj,nCur);` |
|       66 |  908 | `		if( pFinal ){` |
|       66 |  909 | `			PH7_MemObjLoad(pFinal,pSlot);` |
|       32 |  910 | `		}` |
|        - |  911 | `	}` |
|       66 |  912 | `	pSlot->nIdx = nCur;` |
|       66 |  913 | `	return SXRET_OK;` |
|       43 |  914 | `}` |
|        - |  915 | `/*` |
|        - |  916 | ` * Walk a captured path's remaining steps over a VALUE rather than a slot — the` |
|        - |  917 | ` * continuation both resolvers need once the chain has left addressable storage: an` |
|        - |  918 | ` * overloaded container's answer is a temporary, and everything subscripted off it is a` |
|        - |  919 | ` * temporary too. bWrite picks php's fetch mode for those steps: a by-REFERENCE argument` |
|        - |  920 | ` * makes them W fetches, which vivify inside the temporary in SILENCE (php's` |
|        - |  921 | `` * `f($o['a']['zz'])` says only its notice), while a by-VALUE one reads and warns about a`` |
|        - |  922 | ` * key that is not there.` |
|        - |  923 | ` */` |
|    45172 |  924 | `static sxi32 VmWalkStepsOverValue(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,` |
|        - |  925 | `	ph7_value *pCur,int bWrite,ph7_value *pSlot)` |
|        5 |  926 | `{` |
|    45177 |  927 | `	sxi32 rc = SXRET_OK;` |
|        - |  928 | `	sxu32 i;` |
|    90157 |  929 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|    45001 |  930 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|        - |  931 | `		ph7_value out;` |
|    45001 |  932 | `		PH7_MemObjInit(&(*pVm),&out);` |
|    45001 |  933 | `		if( pStep->isProp && bWrite && (pCur->iFlags & MEMOBJ_OBJ) ){` |
|        - |  934 | `			/* php's "indirect" only ever describes a VALUE: an object is a HANDLE, so a` |
|        - |  935 | `			 * write through one lands however the handle was obtained — which is also why` |
|        - |  936 | ``			 * the notice above stays silent for an object. `f($o->magic->p)` with`` |
|        - |  937 | ``			 * `function f(&$x)` really does create and write `p` on the object __get`` |
|        - |  938 | `			 * answered with. Bind the property and let the slot walk finish the chain. */` |
|        3 |  939 | `			sxu32 nOut = SXU32_HIGH;` |
|        3 |  940 | `			int bNoBind = 0;` |
|        3 |  941 | `			int bLastStep = (i + 1 == pPath->nStep);` |
|        - |  942 | `			ph7_value sMagicVal;` |
|        3 |  943 | `			PH7_MemObjRelease(&out);` |
|        3 |  944 | `			PH7_MemObjInit(&(*pVm),&sMagicVal);` |
|        4 |  945 | `			rc = VmBindPropByRef(&(*pVm),pCur,&pStep->sProp,&nOut,&bNoBind,` |
|        1 |  946 | `				bLastStep ? &sMagicVal : 0);` |
|        3 |  947 | `			if( rc != SXRET_OK \|\| bNoBind ){` |
|      ! 0 |  948 | `				if( rc == SXRET_OK && bLastStep ){` |
|        - |  949 | `					/* An overloaded property one level down: its own notice has been` |
|        - |  950 | `					 * raised and what __get answered is what php passes. */` |
|      ! 0 |  951 | `					PH7_MemObjStore(&sMagicVal,pSlot);` |
|      ! 0 |  952 | `					pSlot->nIdx = SXU32_HIGH;` |
|      ! 0 |  953 | `				}` |
|      ! 0 |  954 | `				PH7_MemObjRelease(&sMagicVal);` |
|      ! 0 |  955 | `				return rc;` |
|        - |  956 | `			}` |
|        3 |  957 | `			PH7_MemObjRelease(&sMagicVal);` |
|        3 |  958 | `			return VmWalkStepsFromSlot(&(*pVm),pPath,i + 1,nOut,pSlot);` |
|        - |  959 | `		}` |
|    44999 |  960 | `		if( pStep->isProp ){` |
|        - |  961 | `			ph7_value nameVal;` |
|       43 |  962 | `			PH7_MemObjInitFromString(&(*pVm),&nameVal,&pStep->sProp);` |
|       43 |  963 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_MEMBER,PH7_MEMBER_READ,pCur,&nameVal,&out);` |
|       43 |  964 | `			PH7_MemObjRelease(&nameVal);` |
|    44979 |  965 | `		}else if( pStep->bAppend ){` |
|        - |  966 | ``			/* `f($o['a'][])`: the append lands in the temporary either way. A by-VALUE`` |
|        - |  967 | ``			 * binding is php's runtime `Cannot use [] for reading`, the same Error the`` |
|        - |  968 | `			 * slot-based walk raises for it. */` |
|        - |  969 | `			sxi32 rcAp;` |
|        3 |  970 | `			if( bWrite ){` |
|        - |  971 | `				/* The appended element is a fresh NULL that nothing else can see —` |
|        - |  972 | `				 * php binds the parameter to it and the temporary is dropped. */` |
|      ! 0 |  973 | `				PH7_MemObjRelease(pCur);` |
|      ! 0 |  974 | `				*pCur = out;` |
|      ! 0 |  975 | `				continue;` |
|        - |  976 | `			}` |
|        3 |  977 | `			PH7_MemObjRelease(&out);` |
|        3 |  978 | `			rcAp = VmThrowFromVm(&(*pVm),"Error","Cannot use [] for reading",` |
|        - |  979 | `				sizeof("Cannot use [] for reading")-1);` |
|        3 |  980 | `			return (rcAp == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 |  981 | `		}else{` |
|    44957 |  982 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,bWrite ? 1 : 0,pCur,&pStep->sKey,&out);` |
|        - |  983 | `		}` |
|    44997 |  984 | `		PH7_MemObjRelease(pCur);` |
|    44997 |  985 | `		*pCur = out;` |
|    44997 |  986 | `		if( rc != SXRET_OK ){` |
|       14 |  987 | `			return rc;` |
|        - |  988 | `		}` |
|    22664 |  989 | `	}` |
|    45161 |  990 | `	PH7_MemObjStore(pCur,pSlot);` |
|    45161 |  991 | `	pSlot->nIdx = SXU32_HIGH;` |
|    45161 |  992 | `	return SXRET_OK;` |
|    22760 |  993 | `}` |
|        - |  994 | `/* Resolve a captured lvalue path as a BY-REF target: vivify the whole chain in place and` |
|        - |  995 | ` * leave pSlot->nIdx pointing at the terminal (aliasable) slot for the by-ref binder. */` |
|      120 |  996 | `static sxi32 VmResolvePathByRef(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        4 |  997 | `{` |
|        - |  998 | `	sxu32 nCur;` |
|        - |  999 | `	sxi32 rc;` |
|      124 | 1000 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|        - | 1001 | `		/* An overloaded container was asked for something to MODIFY and could only hand` |
|        - | 1002 | `		 * back a value: php notices that the write has no effect and carries on with the` |
|        - | 1003 | `		 * temporary. The notice is raised HERE — the fetch itself cannot know whether the` |
|        - | 1004 | `		 * parameter it feeds is by-reference, and a by-VALUE one is silent. */` |
|        - | 1005 | `		ph7_value cur;` |
|       35 | 1006 | `		PH7_MemObjInit(&(*pVm),&cur);` |
|       35 | 1007 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|       35 | 1008 | `		rc = VmPrefetchByRefVerdict(&(*pVm),pPath,&cur);` |
|       35 | 1009 | `		if( rc == SXRET_OK ){` |
|       31 | 1010 | `			rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,TRUE,pSlot);` |
|       15 | 1011 | `		}` |
|       35 | 1012 | `		PH7_MemObjRelease(&cur);` |
|       35 | 1013 | `		return rc;` |
|        - | 1014 | `	}` |
|       90 | 1015 | `	if( pPath->eRoot == 2 ){` |
|        - | 1016 | `		/* Subscripting a string: php refuses a by-ref bind to a string offset — but` |
|        - | 1017 | ``		 * it applies its OFFSET rules first, so `f($s["p"])` is the offset TypeError`` |
|        - | 1018 | ``		 * and `f($s[1.5])` warns about the cast before this Error is raised. */`` |
|        - | 1019 | `		sxi32 rcT;` |
|       11 | 1020 | `		if( pPath->nStep > 0 && !pPath->aStep[0].isProp ){` |
|        - | 1021 | `			SyBlob sTypeMsg;` |
|       11 | 1022 | `			sxi64 iOfft = 0;` |
|        8 | 1023 | `			if( VmStringOffsetResolve(&(*pVm),&pPath->aStep[0].sKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg)` |
|        7 | 1024 | `				== VM_STROFF_REJECT ){` |
|        3 | 1025 | `				rcT = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|        3 | 1026 | `				return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1027 | `			}` |
|        3 | 1028 | `		}` |
|        9 | 1029 | `		rcT = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 1030 | `			"Cannot create references to/from string offsets",` |
|        - | 1031 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|        9 | 1032 | `		return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1033 | `	}` |
|       81 | 1034 | `	if( pPath->eRoot == 1 ){` |
|        6 | 1035 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,TRUE); /* vivify $a */` |
|        6 | 1036 | `		if( pRoot == 0 ){` |
|      ! 0 | 1037 | `			return SXRET_OK;` |
|        - | 1038 | `		}` |
|        6 | 1039 | `		nCur = pRoot->nIdx;` |
|        4 | 1040 | `	}else{` |
|       77 | 1041 | `		nCur = pPath->nRootIdx;` |
|        - | 1042 | `	}` |
|       81 | 1043 | `	return VmWalkStepsFromSlot(&(*pVm),pPath,0,nCur,pSlot);` |
|       64 | 1044 | `}` |
|        - | 1045 | `/* Resolve a captured lvalue path as a BY-VALUE argument: read the chain (emitting php's` |
|        - | 1046 | ` * undefined-key/property/offset warnings) WITHOUT vivifying, leaving the terminal value in` |
|        - | 1047 | ` * pSlot. Re-drives the read handlers so the warning sequence matches php exactly. */` |
|    45142 | 1048 | `static sxi32 VmResolvePathByValue(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        5 | 1049 | `{` |
|        - | 1050 | `	ph7_value cur;` |
|        - | 1051 | `	sxi32 rc;` |
|    45147 | 1052 | `	PH7_MemObjInit(&(*pVm),&cur);` |
|    45147 | 1053 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|        - | 1054 | `		/* The accessor already ran, where php runs it: a by-VALUE argument simply takes` |
|        - | 1055 | `		 * what it answered, in silence. */` |
|      184 | 1056 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|    45057 | 1057 | `	}else if( pPath->eRoot == 1 ){` |
|      ! 0 | 1058 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,FALSE);` |
|      ! 0 | 1059 | `		if( pRoot == 0 ){` |
|      ! 0 | 1060 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&pPath->sRootName);` |
|      ! 0 | 1061 | `		}else{` |
|      ! 0 | 1062 | `			PH7_MemObjLoad(pRoot,&cur);` |
|      ! 0 | 1063 | `			cur.nIdx = pRoot->nIdx;` |
|        - | 1064 | `		}` |
|      ! 0 | 1065 | `	}else{` |
|    44967 | 1066 | `		ph7_value *pRoot = (ph7_value *)SySetAt(&pVm->aMemObj,pPath->nRootIdx);` |
|    44967 | 1067 | `		if( pRoot ){` |
|    44967 | 1068 | `			PH7_MemObjLoad(pRoot,&cur);` |
|    44967 | 1069 | `			cur.nIdx = pRoot->nIdx;` |
|    22650 | 1070 | `		}` |
|        - | 1071 | `	}` |
|    45147 | 1072 | `	rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,FALSE,pSlot);` |
|    45147 | 1073 | `	PH7_MemObjRelease(&cur);` |
|    45147 | 1074 | `	return rc;` |
|        5 | 1075 | `}` |
|        - | 1076 | `/*` |
|        - | 1077 | ` * Is this actual argument REFUSED by a by-reference parameter?` |
|        - | 1078 | ` *` |
|        - | 1079 | ` * php answers from the argument's compile-time SHAPE, which the call site carries in` |
|        - | 1080 | ` * VmCallArgMap.nNonLvalMask (GenStateArgShape, compile.c): a literal, an operator or` |
|        - | 1081 | `` * cast result, a class constant, `@$x`, `$o?->p` or an assignment is a hard non-lvalue`` |
|        - | 1082 | `` * and binding one is `Argument #N ($p) could not be passed by reference`.`` |
|        - | 1083 | ` *` |
|        - | 1084 | ` * nPos is the argument's position on the operand stack, which is the position the` |
|        - | 1085 | ` * compiler classified — named arguments change which FORMAL a slot binds to, not the` |
|        - | 1086 | ` * slot's index, so both binders index the mask the same way.` |
|        - | 1087 | ` *` |
|        - | 1088 | ` * Without a shape mask (a SPREAD call, an engine-synthesized call, an indirect dispatch` |
|        - | 1089 | ` * through call_user_func or an array callable) this falls back to the runtime test the` |
|        - | 1090 | ` * binders used before: no slot to write back through, and not one of the values PH7 has` |
|        - | 1091 | ` * always passed by value instead. That test cannot tell a literal from a call RESULT —` |
|        - | 1092 | ` * php accepts the latter — which is exactly why the mask exists.` |
|        - | 1093 | ` */` |
|     7118 | 1094 | `PH7_PRIVATE int PH7_VmArgRefusedByRef(VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1095 | `{` |
|     7123 | 1096 | `	if( pMap && pMap->bArgShapes && nPos < 31 ){` |
|     6903 | 1097 | `		return (pMap->nNonLvalMask & (1u << nPos)) != 0;` |
|        - | 1098 | `	}` |
|      224 | 1099 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|      162 | 1100 | `		return 0;` |
|        - | 1101 | `	}` |
|       93 | 1102 | `	return (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|       62 | 1103 | `	    && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0;` |
|     3564 | 1104 | `}` |
|        - | 1105 | `/*` |
|        - | 1106 | `` * The same call site's OTHER answer: the argument is the RESULT of a call or of `new`.`` |
|        - | 1107 | ` *` |
|        - | 1108 | ` * php cannot know at compile time whether the callee returns a reference, so it defers` |
|        - | 1109 | ` * to the value: one that arrived WITH a reference binds silently, and one without gets` |
|        - | 1110 | ` * php's E_NOTICE and the callee then operates on the temporary. Emitting it is all this` |
|        - | 1111 | ` * does — a temp-call argument is never refused.` |
|        - | 1112 | ` */` |
|        - | 1113 | `/*` |
|        - | 1114 | ` * A typed by-REFERENCE parameter's coercion belongs to the CALLER's variable. php` |
|        - | 1115 | ` * converts the actual in weak mode and the REFERENCE then holds the conversion, so` |
|        - | 1116 | `` * `$v = 1.0; f($v);` with `function f(int &$x)` leaves both views int(1). PHL ran the`` |
|        - | 1117 | ` * declared-type check on the operand-stack COPY while the binder aliases the caller's` |
|        - | 1118 | ` * slot by index, so the conversion reached neither the callee (which reads through the` |
|        - | 1119 | ` * alias) nor the caller: both stayed float, and every other pair did the same` |
|        - | 1120 | `` * (`float &$y` given an int, `string &$s` given an int, `bool &$b` given an int).`` |
|        - | 1121 | ` *` |
|        - | 1122 | ` * Writes back only when the check actually changed the value's TYPE — an untyped` |
|        - | 1123 | ` * parameter, or one the actual already satisfies, copies nothing.` |
|        - | 1124 | ` */` |
|     2940 | 1125 | `PH7_PRIVATE void PH7_VmByRefArgWriteBack(ph7_vm *pVm,ph7_value *pArg,sxi32 iPreFlags)` |
|        5 | 1126 | `{` |
|        - | 1127 | `	ph7_value *pSlot;` |
|     2940 | 1128 | `	if( pArg->nIdx == SXU32_HIGH` |
|     2945 | 1129 | `	 \|\| (pArg->iFlags & MEMOBJ_ALL) == (iPreFlags & MEMOBJ_ALL) ){` |
|     2921 | 1130 | `		return;` |
|        - | 1131 | `	}` |
|       25 | 1132 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|       25 | 1133 | `	if( pSlot && pSlot != pArg ){` |
|       25 | 1134 | `		PH7_MemObjStore(pArg,pSlot);` |
|       12 | 1135 | `	}` |
|     1475 | 1136 | `}` |
|     4380 | 1137 | `PH7_PRIVATE void PH7_VmArgTempCallNotice(ph7_vm *pVm,VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1138 | `{` |
|     4385 | 1139 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| nPos >= 31 ){` |
|      224 | 1140 | `		return;` |
|        - | 1141 | `	}` |
|     4165 | 1142 | `	if( (pMap->nTempCallMask & (1u << nPos)) == 0 ){` |
|     4125 | 1143 | `		return;` |
|        - | 1144 | `	}` |
|       43 | 1145 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|        3 | 1146 | `		return; /* a by-reference RETURN: php is silent and binds it */` |
|        - | 1147 | `	}` |
|       41 | 1148 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Only variables should be passed by reference");` |
|     2195 | 1149 | `}` |
|        - | 1150 | `/*` |
|        - | 1151 | `` * A GENERATOR's arguments are bound at the `g(...)` that BUILDS the Generator object,`` |
|        - | 1152 | ` * before any resume — php's rule, and where php also refuses a by-reference parameter` |
|        - | 1153 | ` * handed a non-variable. That branch collects its actuals into a vector of its own (and` |
|        - | 1154 | ` * reorders it for named arguments), so neither of the two OP_CALL binders ever sees them` |
|        - | 1155 | `` * and `function g(&$x){ yield; } g(1 + 1);` built a Generator in silence.`` |
|        - | 1156 | ` *` |
|        - | 1157 | ` * Answers PH7_EXCEPTION (or PH7_ABORT) for the first refused position, having raised the` |
|        - | 1158 | ` * throw; SXRET_OK otherwise, with php's temp-call notice emitted along the way. Named` |
|        - | 1159 | ` * arguments are resolved by NAME against the formals here rather than through the` |
|        - | 1160 | ` * branch's own mapping, which is built later and freed inside its block.` |
|        - | 1161 | ` */` |
|      118 | 1162 | `static sxi32 VmScreenGenByRefArgs(ph7_vm *pVm,ph7_vm_func *pFunc,VmCallArgMap *pMap,` |
|        - | 1163 | `	ph7_value *pArg,sxu32 nActual,ph7_class *pSelfHint)` |
|        5 | 1164 | `{` |
|      123 | 1165 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      123 | 1166 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|        - | 1167 | `	sxu32 i;` |
|      249 | 1168 | `	for( i = 0 ; i < nActual ; ++i ){` |
|      137 | 1169 | `		sxu32 n = i;` |
|      137 | 1170 | `		if( pMap && pMap->bHasNamed && i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       35 | 1171 | `			for( n = 0 ; n < nFormal ; ++n ){` |
|       30 | 1172 | `				if( pMap->aNames[i].nByte == SyStringLength(&aFormal[n].sName)` |
|       30 | 1173 | `				 && SyMemcmp(pMap->aNames[i].zString,SyStringData(&aFormal[n].sName),` |
|       36 | 1174 | `					pMap->aNames[i].nByte) == 0 ){` |
|       23 | 1175 | `					break;` |
|        - | 1176 | `				}` |
|        8 | 1177 | `			}` |
|       11 | 1178 | `		}` |
|      137 | 1179 | `		if( n >= nFormal \|\| (aFormal[n].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|       95 | 1180 | `			continue;` |
|        - | 1181 | `		}` |
|       44 | 1182 | `		if( PH7_VmArgRefusedByRef(pMap,i,&pArg[i]) ){` |
|       10 | 1183 | `			sxi32 rcT = VmThrowByRefRefusal(&(*pVm),` |
|        6 | 1184 | `				(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|        6 | 1185 | `				&pFunc->sName,n + 1,&aFormal[n].sName);` |
|        7 | 1186 | `			return (rcT == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1187 | `		}` |
|       38 | 1188 | `		PH7_VmArgTempCallNotice(&(*pVm),pMap,i,&pArg[i]);` |
|       20 | 1189 | `	}` |
|      117 | 1190 | `	return SXRET_OK;` |
|       64 | 1191 | `}` |
|        - | 1192 | `/*` |
|        - | 1193 | ` * D1: resolve deferred call arguments in [pArg, pTos) before the callee consumes them.` |
|        - | 1194 | ` *` |
|        - | 1195 | `` * A plain `$var` call argument whose callee signature is unknown at compile time is`` |
|        - | 1196 | ` * emitted as a DEFERRED load (OP_LOAD iP1=1,iP2=3): when the variable does not exist it` |
|        - | 1197 | ` * yields a NULL slot tagged MEMOBJ_AUX_DEFERRED that carries the variable name in x.pOther` |
|        - | 1198 | ` * (a VM-lifetime bytecode string). This helper runs at each OP_CALL dispatch branch, while` |
|        - | 1199 | ` * pVm->pFrame is still the CALLER frame, once the callee's by-ref shape is known:` |
|        - | 1200 | ` *` |
|        - | 1201 | ` *   by-ref position  -> create the variable in the caller frame now and give the slot its` |
|        - | 1202 | ` *                       real nIdx so the ordinary by-ref binder aliases it (silent, as php).` |
|        - | 1203 | ` *   by-value position-> raise php's "Undefined variable $x" and pass a clean NULL WITHOUT` |
|        - | 1204 | ` *                       creating the variable in the caller.` |
|        - | 1205 | ` *` |
|        - | 1206 | ` * The by-ref decision for positional argument n comes from, in priority order:` |
|        - | 1207 | ` *   bAllByValue  -> everything by-value (an unresolvable/erroring callee);` |
|        - | 1208 | ` *   bAllByRef    -> everything by-ref (the indirect array-callable/__invoke dispatch paths,` |
|        - | 1209 | ` *                   which historically over-vivified every plain-var arg — preserved here` |
|        - | 1210 | ` *                   rather than regressed; their by-value refinement is a later slice);` |
|        - | 1211 | ` *   pFormal      -> a user function's formal-argument array (honoring a trailing variadic);` |
|        - | 1212 | ` *   nByRefMask   -> a builtin by-ref position bitmask (used when pFormal == 0).` |
|        - | 1213 | ` *` |
|        - | 1214 | ` * A DEFINED variable never carries the marker (it loads with its real nIdx), so this is a` |
|        - | 1215 | ` * no-op for it; a call with no deferred args pays only one flag test per slot.` |
|        - | 1216 | ` */` |
|  4935418 | 1217 | `PH7_PRIVATE sxi32 PH7_VmResolveDeferredArgs(` |
|        - | 1218 | `	ph7_vm *pVm,` |
|        - | 1219 | `	ph7_value *pArg,` |
|        - | 1220 | `	ph7_value *pTos,` |
|        - | 1221 | `	ph7_vm_func_arg *pFormal,` |
|        - | 1222 | `	sxu32 nFormal,` |
|        - | 1223 | `	sxu32 nByRefMask,` |
|        - | 1224 | `	int bAllByRef,` |
|        - | 1225 | `	int bAllByValue,` |
|        - | 1226 | `	VmCallArgMap *pCallMap)` |
|        5 | 1227 | `{` |
|        - | 1228 | `	ph7_value *p;` |
|  4935423 | 1229 | `	sxu32 n = 0;` |
| 10088681 | 1230 | `	for( p = pArg ; p < pTos ; ++p, ++n ){` |
|  5153305 | 1231 | `		int bByRef = 0;` |
|        - | 1232 | `		SyString sName;` |
|  5153305 | 1233 | `		if( (p->iFlags & (MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH)) == 0 ){` |
|  5130462 | 1234 | `			continue;` |
|        - | 1235 | `		}` |
|    45289 | 1236 | `		if( bAllByValue ){` |
|      ! 0 | 1237 | `			bByRef = 0;` |
|    45289 | 1238 | `		}else if( bAllByRef ){` |
|        - | 1239 | `			/* bAllByRef comes ONLY from the array-callable-value and __invoke dispatch paths,` |
|        - | 1240 | `			 * which cannot expose the target's per-parameter by-ref flags here. Commit 1 chose` |
|        - | 1241 | `			 * to over-vivify every deferred PLAIN-VAR arg on those paths (harmless: it just` |
|        - | 1242 | `			 * materializes the caller variable), and that is preserved. But a deferred` |
|        - | 1243 | `			 * ELEMENT/PROPERTY (MEMOBJ_AUX_DEFPATH) must NOT be blanket-vivified there: a missing` |
|        - | 1244 | `			 * property would fatal ("Cannot create dynamic property") and a missing element would` |
|        - | 1245 | `			 * silently vivify + swallow php's "Undefined array key" warning. Resolve those` |
|        - | 1246 | `			 * by-value (warn + pass NULL), which matches php for a by-VALUE __invoke/callable —` |
|        - | 1247 | `			 * the genuine by-ref-out-param-into-an-element case stays unsupported here, exactly` |
|        - | 1248 | `			 * as it was before this slice. */` |
|      ! 0 | 1249 | `			bByRef = (p->iFlags & MEMOBJ_AUX_DEFPATH) ? 0 : 1;` |
|    45289 | 1250 | `		}else if( pFormal ){` |
|      209 | 1251 | `			sxu32 idx = n;` |
|      204 | 1252 | `			if( pCallMap && pCallMap->bHasNamed && n < pCallMap->nTotal` |
|       23 | 1253 | `			 && pCallMap->aNames[n].nByte > 0 ){` |
|        - | 1254 | `				/* A NAMED actual binds to the formal its NAME picks, not to the one at its` |
|        - | 1255 | ``				 * stack position: `r(x: $a["k"])` is argument #1 on the stack and parameter`` |
|        - | 1256 | `				 * $x in the declaration. Reading the by-ref-ness positionally consulted the` |
|        - | 1257 | `				 * wrong formal, so a by-reference named argument naming a missing element` |
|        - | 1258 | ``				 * warned `Undefined array key` and passed NULL where php creates it. */`` |
|        - | 1259 | `				sxu32 f;` |
|       13 | 1260 | `				idx = SXU32_HIGH;` |
|       25 | 1261 | `				for( f = 0 ; f < nFormal ; ++f ){` |
|       24 | 1262 | `					if( pCallMap->aNames[n].nByte == SyStringLength(&pFormal[f].sName)` |
|       25 | 1263 | `					 && SyMemcmp(pCallMap->aNames[n].zString,` |
|       36 | 1264 | `						SyStringData(&pFormal[f].sName),pCallMap->aNames[n].nByte) == 0 ){` |
|       13 | 1265 | `						idx = f;` |
|       13 | 1266 | `						break;` |
|        - | 1267 | `					}` |
|        7 | 1268 | `				}` |
|      199 | 1269 | `			}else if( idx >= nFormal ){` |
|        - | 1270 | `				/* Beyond the declared formals: a trailing variadic absorbs the tail` |
|        - | 1271 | `				 * (and dictates its by-ref-ness); otherwise the extra arg is by-value. */` |
|      ! 0 | 1272 | `				idx = (nFormal > 0 && (pFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC))` |
|      ! 0 | 1273 | `					? nFormal - 1 : SXU32_HIGH;` |
|      ! 0 | 1274 | `			}` |
|      209 | 1275 | `			if( idx != SXU32_HIGH ){` |
|      209 | 1276 | `				bByRef = (pFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|      102 | 1277 | `			}` |
|      107 | 1278 | `		}else{` |
|    45085 | 1279 | `			bByRef = (n < 31 && (nByRefMask & (1u << n))) ? 1 : 0;` |
|        - | 1280 | `		}` |
|    45289 | 1281 | `		if( p->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|        - | 1282 | `			/* D1 commit 2: a deferred array-element/property lvalue. Detach the descriptor` |
|        - | 1283 | `			 * FIRST (so an exception mid-resolve, or a later stack release, cannot double-free` |
|        - | 1284 | `			 * it) then re-walk it in the chosen mode. */` |
|    45267 | 1285 | `			VmDeferredPath *pPath = (VmDeferredPath *)p->x.pOther;` |
|        - | 1286 | `			sxi32 rc;` |
|    45267 | 1287 | `			p->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|    45267 | 1288 | `			p->x.pOther = 0;` |
|    45267 | 1289 | `			MemObjSetType(p,MEMOBJ_NULL);` |
|    45267 | 1290 | `			p->nIdx = SXU32_HIGH;` |
|    45267 | 1291 | `			if( bByRef ){` |
|      124 | 1292 | `				rc = VmResolvePathByRef(&(*pVm),pPath,p);` |
|       64 | 1293 | `			}else{` |
|    45147 | 1294 | `				rc = VmResolvePathByValue(&(*pVm),pPath,p);` |
|        - | 1295 | `			}` |
|    45267 | 1296 | `			VmFreeDeferredPath(pPath);` |
|    45267 | 1297 | `			if( rc != SXRET_OK ){` |
|       46 | 1298 | `				return rc;` |
|        - | 1299 | `			}` |
|    45225 | 1300 | `			continue;` |
|        - | 1301 | `		}` |
|        - | 1302 | `		/* Recover the deferred variable name and drop the marker + carrier. */` |
|       24 | 1303 | `		SyStringInitFromBuf(&sName,(const char *)p->x.pOther,` |
|        - | 1304 | `			p->x.pOther ? SyStrlen((const char *)p->x.pOther) : 0);` |
|       24 | 1305 | `		p->iFlags &= ~MEMOBJ_AUX_DEFERRED;` |
|       24 | 1306 | `		p->x.pOther = 0;` |
|       24 | 1307 | `		if( bByRef ){` |
|        - | 1308 | `			/* Materialize in the caller frame; the value stays NULL, only the slot` |
|        - | 1309 | `			 * back-reference (nIdx) matters for the by-ref binder / write-back. */` |
|       11 | 1310 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|       11 | 1311 | `			if( pObj ){` |
|       11 | 1312 | `				p->nIdx = pObj->nIdx;` |
|        5 | 1313 | `			}` |
|        6 | 1314 | `		}else{` |
|        - | 1315 | `			/* php warns and passes NULL without creating the variable; the slot is` |
|        - | 1316 | `			 * already a clean NULL with nIdx == SXU32_HIGH from the deferred load. */` |
|       14 | 1317 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        - | 1318 | `		}` |
|       13 | 1319 | `	}` |
|  4935381 | 1320 | `	return SXRET_OK;` |
|  2468761 | 1321 | `}` |
|        - | 1322 | `/*` |
|        - | 1323 | ` * Did resolving a class NAME raise?` |
|        - | 1324 | ` *` |
|        - | 1325 | ` * The lookup can run an AUTOLOADER, and that autoloader can throw. The boundary rail` |
|        - | 1326 | ` * either parks the status in nBoundaryRc or — when a try caught it in place — records a` |
|        - | 1327 | ` * resume frame; either way the throw is already the engine's to land. A call site that` |
|        - | 1328 | `` * sees the class "missing" and piles its own `Class "X" not found` Error on top reports a`` |
|        - | 1329 | ` * failure php never reports, and that second Error belongs to nobody: it came back` |
|        - | 1330 | ` * UNCAUGHT and killed the script right after the real exception had been handled.` |
|        - | 1331 | ` *` |
|        - | 1332 | ` * Snapshot (nBoundaryRc, pResumeFrame) before the lookup and pass them here after.` |
|        - | 1333 | ` */` |
|      190 | 1334 | `PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore)` |
|        4 | 1335 | `{` |
|      194 | 1336 | `	return pVm->nBoundaryRc != nBrcBefore \|\| (const void *)pVm->pResumeFrame != pResumeBefore;` |
|        4 | 1337 | `}` |
|        - | 1338 | `/*` |
|        - | 1339 | `` * Name php's error for a class+method callable that the DIRECT `$cb()` dispatch cannot`` |
|        - | 1340 | ` * call, or return 0 when it resolves.` |
|        - | 1341 | ` *` |
|        - | 1342 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|        - | 1343 | ` * result for an unresolvable pair — silence a caller cannot detect — so the direct call` |
|        - | 1344 | ` * site has to decide for itself. It used to do that only for the ARRAY form; the` |
|        - | 1345 | ``  * `"Class::method"` STRING form went straight to the dispatcher, and `$cb='C::nosuch'` `` |
|        - | 1346 | ` * evaluated to NULL with no diagnostic at all where php throws.` |
|        - | 1347 | ` *` |
|        - | 1348 | ` * pClass is the resolved target class (0 when the name named nothing); zCls/nCls is the` |
|        - | 1349 | ` * class name AS WRITTEN, which is what php's not-found message quotes. bStaticForm says the` |
|        - | 1350 | ` * target was a class NAME rather than an object. Messages that interpolate a name are built` |
|        - | 1351 | ` * into zBuf.` |
|        - | 1352 | ` *` |
|        - | 1353 | ` * Visibility is NOT decided here: an inaccessible method is diagnosed downstream by the` |
|        - | 1354 | ` * dispatch itself ("Call to private method C::p() from global scope"), php-exact already —` |
|        - | 1355 | ` * and php reports visibility BEFORE staticness, so the static rule below has to stay quiet` |
|        - | 1356 | ` * for a method this scope could not reach anyway.` |
|        - | 1357 | ` */` |
|   200266 | 1358 | `static const char * VmCallableClassMethodError(` |
|        - | 1359 | `	ph7_vm *pVm,` |
|        - | 1360 | `	ph7_class *pClass,             /* Resolved target class, or 0 */` |
|        - | 1361 | `	const char *zCls,sxu32 nCls,   /* Its name as the callable wrote it */` |
|        - | 1362 | `	const char *zMeth,sxu32 nMeth, /* The method name */` |
|        - | 1363 | `	int bStaticForm,               /* TRUE when the target is a class NAME, not an object */` |
|        - | 1364 | `	char *zBuf,int nBuf            /* Scratch for the messages that quote a name */` |
|        - | 1365 | `	)` |
|        4 | 1366 | `{` |
|        - | 1367 | `	ph7_class_method *pMethod;` |
|        - | 1368 | `	SyString sMeth;` |
|        - | 1369 | `` 	/* php's fallback for a name this class cannot reach: when the CALLER holds a `$this` `` |
|        - | 1370 | `	 * that is an instance of it, the name resolves to the __call TRAMPOLINE rather than to` |
|        - | 1371 | `	 * __callStatic — and a trampoline is a NON-STATIC function, so this dispatch, which` |
|        - | 1372 | `	 * carries no object, refuses it exactly as it refuses any other non-static method named` |
|        - | 1373 | `	 * through a class. The callback spellings bind that receiver and run (php's` |
|        - | 1374 | `	 * direct-vs-callback asymmetry, one rule apart). The message names the class the` |
|        - | 1375 | `	 * callable WROTE and the name as written, even when the name is a declared static` |
|        - | 1376 | `	 * method: it is the trampoline being refused, not the method. */` |
|   200270 | 1377 | `	int bFallback = bStaticForm && PH7_VmStaticFallbackThis(&(*pVm),pClass) != 0;` |
|   200270 | 1378 | `	if( pClass == 0 ){` |
|       49 | 1379 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|       49 | 1380 | `		return zBuf;` |
|        - | 1381 | `	}` |
|   200223 | 1382 | `	pMethod = PH7_ClassExtractMethod(pClass,zMeth,nMeth);` |
|   200223 | 1383 | `	if( pMethod == 0 ){` |
|       79 | 1384 | `		if( bFallback ){` |
|        7 | 1385 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|        2 | 1386 | `				&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1387 | `			return zBuf;` |
|        - | 1388 | `		}` |
|        - | 1389 | `		/* A class that answers for unknown names through the catch-all has nothing to` |
|        - | 1390 | `		 * report: php runs __callStatic (class-name target) / __call (object target) for` |
|        - | 1391 | `		 * ANY method name, and the dispatcher below routes it. */` |
|       75 | 1392 | `		const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       75 | 1393 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|       47 | 1394 | `			return 0;` |
|        - | 1395 | `		}` |
|       43 | 1396 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",` |
|       14 | 1397 | `			&pClass->sName,(int)nMeth,zMeth);` |
|       29 | 1398 | `		return zBuf;` |
|        - | 1399 | `	}` |
|        - | 1400 | `	/* An ABSTRACT method (an interface's included) has no body to call — the same message` |
|        - | 1401 | ``	 * the `C::m()` SYNTAX raises in vm_ops_oo.c, which this dispatch reached only as a`` |
|        - | 1402 | `	 * mangled internal function name. */` |
|   200145 | 1403 | `	SyStringInitFromBuf(&sMeth,zMeth,nMeth);` |
|   200145 | 1404 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        7 | 1405 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%z()",&pClass->sName,&sMeth);` |
|        7 | 1406 | `		return zBuf;` |
|        - | 1407 | `	}` |
|        - | 1408 | `	/* Named through a class NAME, a non-static method is never callable: php refuses even` |
|        - | 1409 | `	 * when the CALLER has a compatible $this (unlike call_user_func, which binds it). The` |
|        - | 1410 | `	 * message names the OWNING class and the method's declared spelling. */` |
|        - | 1411 | `	{` |
|        - | 1412 | `		SyString sDecl;` |
|        - | 1413 | `		int bAccessible;` |
|   200139 | 1414 | `		SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1415 | `			SyStringLength(&pMethod->sFunc.sName));` |
|   300226 | 1416 | `		bAccessible = pMethod->iProtection == PH7_CLASS_PROT_PUBLIC` |
|   200136 | 1417 | `			\|\| PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       19 | 1418 | `				&sDecl,pMethod->iProtection,FALSE);` |
|   200139 | 1419 | `		if( !bAccessible && bFallback ){` |
|        - | 1420 | `			/* Inaccessible goes the same way as missing: php never reports the visibility,` |
|        - | 1421 | `			 * because the name resolved to the trampoline before visibility could matter. */` |
|       13 | 1422 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|        4 | 1423 | `				&pClass->sName,(int)nMeth,zMeth);` |
|       17 | 1424 | `			return zBuf;` |
|        - | 1425 | `		}` |
|   200131 | 1426 | `		if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 && bAccessible ){` |
|        - | 1427 | `			/* Deciding class vs NAMED class: a trait is php's compile-time construct, so` |
|        - | 1428 | `			 * every message names the class that composed it (PH7_VmMethodScopeName). */` |
|       25 | 1429 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|       16 | 1430 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|       17 | 1431 | `			return zBuf;` |
|        - | 1432 | `		}` |
|        - | 1433 | `	}` |
|   200115 | 1434 | `	return 0;` |
|   100137 | 1435 | `}` |
|        - | 1436 | `/*` |
|        - | 1437 | ` * php's visibility refusal for a method call, worded once: "Call to private A::m() from` |
|        - | 1438 | ` * scope S" (or "from global scope"). Two sites raise it — OP_CALL's screen and the` |
|        - | 1439 | ` * first-class-callable one below — and php names the DECLARING class, not the class the` |
|        - | 1440 | ` * lookup went through.` |
|        - | 1441 | ` */` |
|       24 | 1442 | `static const char * VmMethodVisibilityMsg(ph7_vm *pVm,ph7_class *pDecl,` |
|        - | 1443 | `	const char *zMeth,sxu32 nMeth,sxi32 iProtection,char *zBuf,int nBuf)` |
|        2 | 1444 | `{` |
|       26 | 1445 | `	const char *zVis = iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       26 | 1446 | `	ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));` |
|       26 | 1447 | `	if( pScope ){` |
|        4 | 1448 | `		SyBufferFormat(zBuf,nBuf,"Call to %s method %z::%.*s() from scope %z",` |
|        1 | 1449 | `			zVis,&pDecl->sName,(int)nMeth,zMeth,&pScope->sName);` |
|        2 | 1450 | `	}else{` |
|       35 | 1451 | `		SyBufferFormat(zBuf,nBuf,"Call to %s method %z::%.*s() from global scope",` |
|       11 | 1452 | `			zVis,&pDecl->sName,(int)nMeth,zMeth);` |
|        - | 1453 | `	}` |
|       26 | 1454 | `	return zBuf;` |
|        2 | 1455 | `}` |
|        - | 1456 | `/*` |
|        - | 1457 | ` * Is this class+method pair one a call would reach DIRECTLY from here — a real method (not` |
|        - | 1458 | ` * abstract, not a name only the catch-all answers) that the current scope may call? php` |
|        - | 1459 | ` * decides exactly this when it BUILDS a method Closure, and stores the resolved function; the` |
|        - | 1460 | ` * answer is what the VM_INSTANCE_FCC_SCREENED mark records, so the invocation never asks again.` |
|        - | 1461 | ` * A pair that answers FALSE here is the __call/__callStatic trampoline's, and its closure must` |
|        - | 1462 | ` * keep routing there.` |
|        - | 1463 | ` */` |
|      206 | 1464 | `PH7_PRIVATE int PH7_VmFccMethodIsDirect(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName)` |
|        4 | 1465 | `{` |
|        - | 1466 | `	ph7_class_method *pMethod;` |
|        - | 1467 | `	SyString sDecl;` |
|      210 | 1468 | `	if( pClass == 0 \|\| nName < 1 ){` |
|      ! 0 | 1469 | `		return 0;` |
|        - | 1470 | `	}` |
|      210 | 1471 | `	pMethod = PH7_ClassExtractMethod(pClass,zName,nName);` |
|      210 | 1472 | `	if( pMethod == 0 \|\| (pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       17 | 1473 | `		return 0;` |
|        - | 1474 | `	}` |
|      194 | 1475 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|      125 | 1476 | `		return 1;` |
|        - | 1477 | `	}` |
|       72 | 1478 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1479 | `		SyStringLength(&pMethod->sFunc.sName));` |
|        - | 1480 | `	/* The OWNING class decides (a trait method is owned by the class that composed it) — the` |
|        - | 1481 | `	 * same argument every other visibility site passes. */` |
|      106 | 1482 | `	return PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       68 | 1483 | `		&sDecl,pMethod->iProtection,FALSE) ? 1 : 0;` |
|      107 | 1484 | `}` |
|        - | 1485 | `/*` |
|        - | 1486 | `` * Resolve `$o->m(...)` / `C::m(...)` the way php resolves the CALL it stands for, and say`` |
|        - | 1487 | ` * why when it cannot. php builds a first-class callable through the same member lookup a` |
|        - | 1488 | ` * real call goes through, so every refusal a call would raise happens HERE, at creation:` |
|        - | 1489 | ` * an undefined method, an inaccessible one, an abstract one, and a non-static one named` |
|        - | 1490 | ` * through a class with no receiver to run on. PHL created a Closure for all four and only` |
|        - | 1491 | ` * discovered the problem when (and if) it was invoked — a closure that is built and dropped` |
|        - | 1492 | ` * reported nothing at all.` |
|        - | 1493 | ` *` |
|        - | 1494 | ` * The receiver is the other half of the same lookup. php's ZEND_INIT_STATIC_METHOD_CALL` |
|        - | 1495 | `` * binds the CALLING frame's `$this` when the resolved method is non-static and that object`` |
|        - | 1496 | `` * is an instance of the named class, which is what makes `self::m(...)` inside an instance`` |
|        - | 1497 | ` * method a working callable rather than a static one; PHL bound only the scope, so the` |
|        - | 1498 | ` * closure could never run. *ppRecv is that object, or 0 for a genuinely static callable.` |
|        - | 1499 | ` *` |
|        - | 1500 | ` * Answers 0 when the callable is valid. Messages that quote a name are built into zBuf.` |
|        - | 1501 | ` */` |
|      178 | 1502 | `static const char * VmFccMemberError(ph7_vm *pVm,ph7_class *pClass,` |
|        - | 1503 | `	const char *zCls,sxu32 nCls,const char *zMeth,sxu32 nMeth,int bStaticForm,` |
|        - | 1504 | `	ph7_class_instance **ppRecv,char *zBuf,int nBuf)` |
|        4 | 1505 | `{` |
|        - | 1506 | `	ph7_class_method *pMethod;` |
|        - | 1507 | `	SyString sDecl;` |
|      182 | 1508 | `	*ppRecv = 0;` |
|      182 | 1509 | `	if( pClass == 0 ){` |
|        3 | 1510 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|        3 | 1511 | `		return zBuf;` |
|        - | 1512 | `	}` |
|      180 | 1513 | `	pMethod = nMeth > 0 ? PH7_ClassExtractMethod(pClass,zMeth,nMeth) : 0;` |
|      180 | 1514 | `	if( pMethod == 0 ){` |
|        - | 1515 | `		/* A name the class answers through the catch-all is callable, and the catch-all` |
|        - | 1516 | `		 * the STATIC spelling reaches depends on the receiver, exactly as it does for a` |
|        - | 1517 | `		 * call (PH7_VmStaticFallbackThis). */` |
|       17 | 1518 | `		if( bStaticForm ){` |
|       11 | 1519 | `			*ppRecv = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|       10 | 1520 | `			if( *ppRecv` |
|       10 | 1521 | `			 \|\| PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1) ){` |
|        9 | 1522 | `				return 0;` |
|        1 | 1523 | `			}` |
|        8 | 1524 | `		}else if( PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) ){` |
|        5 | 1525 | `			return 0;` |
|        - | 1526 | `		}` |
|        7 | 1527 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",` |
|        2 | 1528 | `			&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1529 | `		return zBuf;` |
|        - | 1530 | `	}` |
|      164 | 1531 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1532 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      164 | 1533 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1534 | `		/* Named through the CLASS only: an instance of an abstract class cannot exist, so` |
|        - | 1535 | `		 * the object spelling never reaches an abstract body. */` |
|        7 | 1536 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%.*s()",` |
|        2 | 1537 | `			&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1538 | `		return zBuf;` |
|        - | 1539 | `	}` |
|      156 | 1540 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      116 | 1541 | `	 && !PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       34 | 1542 | `			&sDecl,pMethod->iProtection,FALSE) ){` |
|        - | 1543 | `		/* Inaccessible: the catch-all answers for it, on the same receiver a call would use. */` |
|       12 | 1544 | `		*ppRecv = bStaticForm ? PH7_VmStaticFallbackThis(&(*pVm),pClass) : 0;` |
|       12 | 1545 | `		if( *ppRecv ){` |
|      ! 0 | 1546 | `			return 0;` |
|        - | 1547 | `		}` |
|       12 | 1548 | `		if( !bStaticForm && PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) ){` |
|        3 | 1549 | `			return 0;` |
|        - | 1550 | `		}` |
|        - | 1551 | `		/* The DECIDING class is the declaring one (its trait grants live there); the class` |
|        - | 1552 | `		 * php NAMES is the composing one — a trait has no runtime existence in php. */` |
|       14 | 1553 | `		return VmMethodVisibilityMsg(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|        4 | 1554 | `			zMeth,nMeth,pMethod->iProtection,zBuf,nBuf);` |
|        - | 1555 | `	}` |
|      150 | 1556 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|       15 | 1557 | `		*ppRecv = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|       15 | 1558 | `		if( *ppRecv == 0 ){` |
|        7 | 1559 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|        4 | 1560 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|        5 | 1561 | `			return zBuf;` |
|        - | 1562 | `		}` |
|        5 | 1563 | `	}` |
|      146 | 1564 | `	return 0;` |
|       93 | 1565 | `}` |
|        - | 1566 | `/*` |
|        - | 1567 | ` * The same check for the ARRAY form, whose two members carry php's own shape messages` |
|        - | 1568 | ` * before anything is resolved: the target must be an object or a class-name string, the` |
|        - | 1569 | `` * method must be a string. php probes them in that order (`[5,5]` names the FIRST member,`` |
|        - | 1570 | `` * `['NoSuch',5]` the SECOND — the member shape decides before the class is looked up).`` |
|        - | 1571 | ` */` |
|   100186 | 1572 | `static const char * VmDirectArrayCallableError(ph7_vm *pVm,ph7_value *pTarget,ph7_value *pMethod,` |
|        - | 1573 | `	char *zBuf,int nBuf)` |
|        4 | 1574 | `{` |
|        - | 1575 | `	ph7_class *pClass;` |
|   100190 | 1576 | `	if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|       11 | 1577 | `		return "First array member is not a valid class name or object";` |
|        - | 1578 | `	}` |
|   100180 | 1579 | `	if( (pMethod->iFlags & MEMOBJ_STRING) == 0 ){` |
|        9 | 1580 | `		return "Second array member is not a valid method";` |
|        - | 1581 | `	}` |
|   100172 | 1582 | `	pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   150256 | 1583 | `	return VmCallableClassMethodError(&(*pVm),pClass,` |
|   100168 | 1584 | `		(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|   100168 | 1585 | `		(const char *)SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob),` |
|        - | 1586 | `		/* An OBJECT target carries its own $this; only a class NAME is the static form. */` |
|   100168 | 1587 | `		(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|    50084 | 1588 | `		zBuf,nBuf);` |
|    50097 | 1589 | `}` |
|        - | 1590 | `/*` |
|        - | 1591 | ` * The by-reference SHAPE of the callee an INDIRECT dispatch is about to reach — an` |
|        - | 1592 | `` * array callable VALUE (`$cb = [$o,'m']; $cb($a['k']);`) and an __invoke object.`` |
|        - | 1593 | ` * Both go through a shared helper that hides the target from OP_CALL, so the two` |
|        - | 1594 | ` * sites used to materialize EVERY deferred plain-var argument by reference and every` |
|        - | 1595 | ` * deferred element/property by value: a genuine by-ref out-param into an element was` |
|        - | 1596 | `` * unsupported (`$cb($a['new'])` warned `Undefined array key` and handed the callee a`` |
|        - | 1597 | ` * NULL where php creates the element and writes it), and a by-VALUE parameter` |
|        - | 1598 | `` * swallowed php's `Undefined variable` and CREATED the caller's variable.`` |
|        - | 1599 | ` *` |
|        - | 1600 | ` * The target is knowable here: the pair resolves to a class and a method, an object to` |
|        - | 1601 | ` * its __invoke. Answers 0 when nothing resolves — a name routed through` |
|        - | 1602 | ` * __call/__callStatic (php packs those into an ARRAY, so they are by-value anyway) or` |
|        - | 1603 | ` * a pair the screen above is about to refuse.` |
|        - | 1604 | ` */` |
|   300330 | 1605 | `static ph7_vm_func * VmIndirectCalleeFunc(ph7_vm *pVm,ph7_value *pCallable)` |
|        5 | 1606 | `{` |
|   300335 | 1607 | `	ph7_class_method *pMeth = 0;` |
|   300335 | 1608 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|   100246 | 1609 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|   100246 | 1610 | `		ph7_value *pTarget = 0,*pName = 0;` |
|        - | 1611 | `		ph7_class *pClass;` |
|   100242 | 1612 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|   100242 | 1613 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|   100246 | 1614 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1615 | `			return 0;` |
|        - | 1616 | `		}` |
|   100246 | 1617 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   100246 | 1618 | `		if( pClass == 0 ){` |
|      ! 0 | 1619 | `			return 0;` |
|        - | 1620 | `		}` |
|   150367 | 1621 | `		pMeth = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|   100242 | 1622 | `			SyBlobLength(&pName->sBlob));` |
|   250213 | 1623 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|   200092 | 1624 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|   200092 | 1625 | `		if( pThis == 0 ){` |
|      ! 0 | 1626 | `			return 0;` |
|        - | 1627 | `		}` |
|   200092 | 1628 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|   100044 | 1629 | `	}` |
|   300335 | 1630 | `	return pMeth ? &pMeth->sFunc : 0;` |
|   150170 | 1631 | `}` |
|        - | 1632 | `/*` |
|        - | 1633 | ` * Materialize an indirect dispatch's deferred arguments against that callee — the same` |
|        - | 1634 | ` * split OP_CALL makes for a direct one: a native method's by-ref positions come from its` |
|        - | 1635 | ` * signature mask, a PHP one's from its compiled formals, and an unresolved callee binds` |
|        - | 1636 | ` * everything by value (php's answer for the magic route it is about to take).` |
|        - | 1637 | ` */` |
|   300330 | 1638 | `static sxi32 VmResolveIndirectArgs(ph7_vm *pVm,ph7_value *pCallable,ph7_value *pArg,ph7_value *pTos,` |
|        - | 1639 | `	VmCallArgMap *pCallMap)` |
|        5 | 1640 | `{` |
|   300335 | 1641 | `	ph7_vm_func *pFn = VmIndirectCalleeFunc(&(*pVm),pCallable);` |
|   300335 | 1642 | `	if( pFn == 0 ){` |
|   100042 | 1643 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pCallMap);` |
|        - | 1644 | `	}` |
|   200295 | 1645 | `	if( pFn->iFlags & VM_FUNC_NATIVE ){` |
|        7 | 1646 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|        6 | 1647 | `			pFn->pNative ? pFn->pNative->nByRefMask : 0,0,0,pCallMap);` |
|        - | 1648 | `	}` |
|   300431 | 1649 | `	return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   200284 | 1650 | `		(ph7_vm_func_arg *)SySetBasePtr(&pFn->aArgs),SySetUsed(&pFn->aArgs),0,0,0,pCallMap);` |
|   150170 | 1651 | `}` |
|        - | 1652 | `/*` |
|        - | 1653 | `` * Why a VALUE cannot be made into a first-class callable. php answers `($v)(...)` with`` |
|        - | 1654 | `` * exactly what it answers `($v)()` — the taxonomy is the DIRECT dispatch's, word for word —`` |
|        - | 1655 | ` * so this walks the same three shapes the OP_CALL sites do and reuses their builders. PHL` |
|        - | 1656 | `` * left a non-callable value STANDING instead: `$x = 5; $f = ($x)(...);` evaluated to int(5),`` |
|        - | 1657 | ` * an array to the array, a misspelled function name to its own string — a value that is not` |
|        - | 1658 | ` * a Closure where php throws, silently, on every shape.` |
|        - | 1659 | ` *` |
|        - | 1660 | ` * Returns 0 when the value IS callable (unreachable through the FCC caller, which asks only` |
|        - | 1661 | ` * after the wrap declined, but it keeps the helper honest for a direct reader).` |
|        - | 1662 | ` */` |
|       66 | 1663 | `static const char * VmFccValueError(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|        2 | 1664 | `{` |
|       68 | 1665 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       23 | 1666 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       23 | 1667 | `		ph7_value *pTarget = 0,*pMeth = 0;` |
|        - | 1668 | `		const char *zWhy;` |
|       23 | 1669 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|        9 | 1670 | `			return "Array callback must have exactly two elements";` |
|        - | 1671 | `		}` |
|       15 | 1672 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pMeth) ){` |
|        3 | 1673 | `			return "Array callback has to contain indices 0 and 1";` |
|        - | 1674 | `		}` |
|       13 | 1675 | `		zWhy = VmDirectArrayCallableError(&(*pVm),pTarget,pMeth,zBuf,nBuf);` |
|       13 | 1676 | `		if( zWhy ){` |
|        9 | 1677 | `			return zWhy;` |
|        - | 1678 | `		}` |
|        - | 1679 | `		/* That check deliberately leaves VISIBILITY to OP_CALL's own screen, which raises it` |
|        - | 1680 | `		 * when the pair is finally called — and a first-class callable never gets there: php` |
|        - | 1681 | ``		 * refuses `[$o,'priv'](...)` at the creation, with the direct dispatch's wording.`` |
|        - | 1682 | `		 * Reached only for a pair PH7_VmIsCallable already declined, so a class routing the` |
|        - | 1683 | `		 * name through __call (which makes it callable) cannot arrive here. */` |
|        5 | 1684 | `		if( (pMeth->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMeth->sBlob) > 0 ){` |
|        5 | 1685 | `			ph7_class *pCbCls = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|        5 | 1686 | `			const char *zM = (const char *)SyBlobData(&pMeth->sBlob);` |
|        5 | 1687 | `			sxu32 nM = SyBlobLength(&pMeth->sBlob);` |
|        5 | 1688 | `			ph7_class_method *pCbMeth = pCbCls ? PH7_ClassExtractMethod(pCbCls,zM,nM) : 0;` |
|        4 | 1689 | `			if( pCbMeth && pCbMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|        5 | 1690 | `			 && !PH7_VmFccMethodIsDirect(&(*pVm),pCbCls,zM,nM) ){` |
|        7 | 1691 | `				return VmMethodVisibilityMsg(&(*pVm),` |
|        2 | 1692 | `					PH7_VmMethodScopeName(&(*pVm),pCbCls,pCbMeth),` |
|        2 | 1693 | `					zM,nM,pCbMeth->iProtection,zBuf,nBuf);` |
|        - | 1694 | `			}` |
|      ! 0 | 1695 | `		}` |
|      ! 0 | 1696 | `		return 0;` |
|        - | 1697 | `	}` |
|       46 | 1698 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       15 | 1699 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|       15 | 1700 | `		if( pObj == 0 ){` |
|      ! 0 | 1701 | `			return "Value of type object is not callable";` |
|        - | 1702 | `		}` |
|       15 | 1703 | `		if( PH7_ClassExtractMethod(pObj->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|      ! 0 | 1704 | `			return 0;` |
|        - | 1705 | `		}` |
|       15 | 1706 | `		SyBufferFormat(zBuf,nBuf,"Object of type %z is not callable",&pObj->pClass->sName);` |
|       15 | 1707 | `		return zBuf;` |
|        - | 1708 | `	}` |
|       32 | 1709 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       20 | 1710 | `		const char *zCls = 0,*zMeth = 0;` |
|       20 | 1711 | `		sxu32 nCls = 0,nMeth = 0;` |
|        - | 1712 | `		SyString sName;` |
|       20 | 1713 | `		SyStringInitFromBuf(&sName,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|        - | 1714 | `		/* A leading backslash only anchors the name to the global namespace. */` |
|       20 | 1715 | `		if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|      ! 0 | 1716 | `			sName.zString++;` |
|      ! 0 | 1717 | `			sName.nByte--;` |
|      ! 0 | 1718 | `		}` |
|       20 | 1719 | `		if( PH7_VmCallableStringParts(sName.zString,sName.nByte,&zCls,&nCls,&zMeth,&nMeth) ){` |
|        - | 1720 | `			/* "Class::method" carries the class/method taxonomy, not the function one. */` |
|        7 | 1721 | `			return VmCallableClassMethodError(&(*pVm),` |
|        2 | 1722 | `				PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0),` |
|        2 | 1723 | `				zCls,nCls,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|        - | 1724 | `		}` |
|       16 | 1725 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined function %z()",&sName);` |
|       16 | 1726 | `		return zBuf;` |
|        - | 1727 | `	}` |
|       13 | 1728 | `	SyBufferFormat(zBuf,nBuf,"Value of type %s is not callable",VmArithTypeName(pValue));` |
|       13 | 1729 | `	return zBuf;` |
|       35 | 1730 | `}` |
|        - | 1731 | `/*` |
|        - | 1732 | `` * Split a `"Class::method"` callable string. php scans for the LAST "::" (zend_memrchr),`` |
|        - | 1733 | `` * so `"C::s::x"` names the class `C::s` — not `C` — and reports it not found. Returns TRUE`` |
|        - | 1734 | `` * and the two halves (either may be empty: `"C::"` and `"::s"` are shapes php accepts here`` |
|        - | 1735 | ` * and rejects further down), FALSE when the string carries no "::" at all.` |
|        - | 1736 | ` */` |
|  1810255 | 1737 | `PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,` |
|        - | 1738 | `	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth)` |
|        5 | 1739 | `{` |
|        - | 1740 | `	sxu32 i;` |
| 14376525 | 1741 | `	for( i = nName ; i >= 2 ; --i ){` |
| 12766466 | 1742 | `		if( zName[i-2] == ':' && zName[i-1] == ':' ){` |
|   200201 | 1743 | `			*pzCls = zName;` |
|   200201 | 1744 | `			*pnCls = i - 2;` |
|   200201 | 1745 | `			*pzMeth = &zName[i];` |
|   200201 | 1746 | `			*pnMeth = nName - i;` |
|   200201 | 1747 | `			return TRUE;` |
|        - | 1748 | `		}` |
|  6292544 | 1749 | `	}` |
|  1610064 | 1750 | `	return FALSE;` |
|   906178 | 1751 | `}` |
|  3384819 | 1752 | `static sxi32 VmByteCodeExecBody(` |
|        - | 1753 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 1754 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - | 1755 | `	ph7_value *pStack,   /* Operand stack */` |
|        - | 1756 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - | 1757 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - | 1758 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - | 1759 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - | 1760 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - | 1761 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - | 1762 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|        - | 1763 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|        - | 1764 | `	ph7_value **ppBaseOwner, /* Base (pCallTop==0) operand-stack owner slot the native entry frees; OP_SPREAD growth of the base stack writes the new pointer here (see VmGrowOperandStack). */` |
|        - | 1765 | `	sxu32 *pnBaseCap, /* Base stack capacity (persisted across coroutine suspend/resume); read for the initial capacity and updated on base-stack growth. */` |
|        - | 1766 | `	sxu32 nStackOrig /* Base stack's ORIGINAL (ungrown) allocation size — the fixed headroom reference for OP_SPREAD growth (see nStackOrig in VmExecState). */` |
|        - | 1767 | `	)` |
|        5 | 1768 | `{` |
|        - | 1769 | `	VmInstr *pInstr;` |
|        - | 1770 | `	ph7_value *pTos;` |
|        - | 1771 | `	SySet aArg;` |
|  3384824 | 1772 | `	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record` |
|        - | 1773 | `	                            * stack (BYTECODE stage 2); NULL = executing the` |
|        - | 1774 | `	                            * bottom activation. */` |
|        - | 1775 | `	VmExecState sState; /* This activation's boundary state (BYTECODE.md stage 1):` |
|        - | 1776 | `	                     * everything a suspended/nested activation must restore.` |
|        - | 1777 | `	                     * pc/pTos stay in locals for the hot loop and are synced` |
|        - | 1778 | `	                     * into sState only around the call epilogue (stage 2 turns` |
|        - | 1779 | `	                     * that boundary into an explicit record push/pop). */` |
|        - | 1780 | `	sxi32 pc;` |
|        - | 1781 | `	sxi32 rc;` |
|  3384824 | 1782 | `	sState.aInstr = aInstr;` |
|  3384824 | 1783 | `	sState.pStack = pStack;` |
|  3384824 | 1784 | `	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */` |
|  3384824 | 1785 | `	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */` |
|  3384824 | 1786 | `	sState.pResult = pResult;` |
|  3384824 | 1787 | `	sState.pLastRef = pLastRef;` |
|  3384824 | 1788 | `	sState.pEnforceRetFunc = pEnforceRetFunc;` |
|  3384824 | 1789 | `	sState.is_callback = (sxu8)(is_callback ? 1 : 0);` |
|  3384824 | 1790 | `	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);` |
|        - | 1791 | `	/* Argument container */` |
|  3384824 | 1792 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|  3384824 | 1793 | `	if( nTos < 0 ){` |
|  1494085 | 1794 | `		pTos = &pStack[-1];` |
|   747044 | 1795 | `	}else{` |
|  1890744 | 1796 | `		pTos = &pStack[nTos];` |
|        - | 1797 | `	}` |
|  3384824 | 1798 | `	sState.pTos = pTos;` |
|  3384824 | 1799 | `	sState.pc = nPc;` |
|        - | 1800 | `	/* Finally-drain base. For a resumed generator/fiber TOP-LEVEL body, its own` |
|        - | 1801 | `	 * exception handlers were just re-published above the caller depth` |
|        - | 1802 | `	 * (VmRestoreCtxState), so the live SySetUsed over-counts; take the` |
|        - | 1803 | `	 * caller-depth base recorded on the ctx instead.` |
|        - | 1804 | `	 *` |
|        - | 1805 | `	 * The discriminator is the OPERAND STACK, not the frame: the body exec runs on` |
|        - | 1806 | `	 * the ctx's own pStack, whereas every nested frame-less mini-program run within` |
|        - | 1807 | `	 * it — a default-argument / constructor trampoline, or a catch/finally body via` |
|        - | 1808 | `	 * VmLocalExec — runs on a FRESH operand stack while SHARING pVm->pFrame (those` |
|        - | 1809 | `	 * mini-programs do not push a VM frame). A pFrame-only guard therefore misfires` |
|        - | 1810 | `	 * for such a mini-program and hands it the generator's low caller-base; its` |
|        - | 1811 | `	 * terminal OP_DONE then drains VmDrainFinally down to that base, tearing down a` |
|        - | 1812 | ``	 * live try that the surrounding finally had just opened (e.g. `finally { try {`` |
|        - | 1813 | ``	 * throw new E(); } catch (E) {} }` in a generator: the `new E()` trampoline's`` |
|        - | 1814 | `	 * OP_DONE popped the inner try before its OP_THROW ran, so the throw escaped` |
|        - | 1815 | `	 * uncaught). Requiring pStack == pCtx->pStack pins the override to the resumed` |
|        - | 1816 | `	 * body itself; nested mini-programs fall through to the correct live depth. */` |
|  3384819 | 1817 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame` |
|     2464 | 1818 | `	 && pStack == pVm->pActiveCtx->pStack ){` |
|     1947 | 1819 | `		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;` |
|     1947 | 1820 | `		sState.nFinallyActBase = pVm->pActiveCtx->nFinallyBase;` |
|      976 | 1821 | `	}else{` |
|  3382882 | 1822 | `		sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|  3382882 | 1823 | `		sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|        - | 1824 | `	}` |
|  3384824 | 1825 | `	sState.pEntryFrame = pVm->pFrame;` |
|  3384824 | 1826 | `	pc = nPc;` |
|        - | 1827 | `	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call` |
|        - | 1828 | `	 * adopts its parked record segment here. VmResumeCtx hands the segment in` |
|        - | 1829 | `	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a` |
|        - | 1830 | `	 * nested mini-program/callback run inside the resumed body — after it rebased` |
|        - | 1831 | `	 * the segment's exception floors and pushed the resume value into the innermost` |
|        - | 1832 | `	 * stack. Locals switch to the innermost activation so the dispatch loop` |
|        - | 1833 | `	 * continues inside the callee; the record chain is restored so its completion` |
|        - | 1834 | `	 * unwinds back through the body. */` |
|  3384824 | 1835 | `	if( pAdoptSegment ){` |
|      106 | 1836 | `		VmParkedSegment *pSeg = pAdoptSegment;` |
|      106 | 1837 | `		pCallTop = pSeg->pCallTop;` |
|      106 | 1838 | `		sState = pSeg->sState;` |
|      106 | 1839 | `		aInstr = sState.aInstr;` |
|      106 | 1840 | `		pStack = sState.pStack;` |
|        - | 1841 | `		/* pc is already nPc (== pCtx->pc, the innermost's post-suspend pc) from the` |
|        - | 1842 | `		 * init above; only the stack/top move to the innermost activation. */` |
|      106 | 1843 | `		pTos = &pStack[nTos];   /* nTos == pCtx->nTos — innermost, resume value pushed */` |
|      106 | 1844 | `		SyMemBackendFree(&pVm->sAllocator,pSeg); /* holder only; its contents are now live */` |
|       51 | 1845 | `	}` |
|        - | 1846 | `/*` |
|        - | 1847 | ` * Route an enforcement helper's (or yield-from delegate's) return code from inside` |
|        - | 1848 | ` * the main switch: proceed on SXRET_OK, abort on PH7_ABORT, and on PH7_EXCEPTION` |
|        - | 1849 | ` * resume at the landing pad of the body that actually caught the exception in place` |
|        - | 1850 | ` * (VmRecordedResume) or, when it was caught by an outer exec, unwind out of the VM` |
|        - | 1851 | ` * loop. Replaces the old "jump to pVm->pFrame's nearest iExceptionJump" which ran` |
|        - | 1852 | ` * the statement after the try even when the catch was at an enclosing frame (ROOT B,` |
|        - | 1853 | `` * face b — `yield from` over a throwing sub-generator).`` |
|        - | 1854 | ` */` |
|        - | 1855 | `/* Dispatch-routing macros: bodies live in vm_dispatch.h, written against the` |
|        - | 1856 | ` * VM_EXIT_* primitives. Here they bind to the loop's own control flow. */` |
|        - | 1857 | `#define VM_EXIT_BREAK break` |
|        - | 1858 | `#define VM_EXIT_ABORT goto Abort` |
|        - | 1859 | `#define VM_EXIT_EXCEPTION goto Exception` |
|        - | 1860 | `#include "vm_dispatch.h"` |
|        - | 1861 | `	/* Generator::throw() inject-at-yield: when this invocation is a resumed generator/fiber` |
|        - | 1862 | `	 * body carrying a pending injected exception, raise it HERE — once, before the dispatch` |
|        - | 1863 | `	 * loop (pc is already at the resume point) — so the existing OP_THROW route` |
|        - | 1864 | `	 * (VmThrowException + VmRecordedResume) lands it at the generator's own try/catch landing` |
|        - | 1865 | `	 * pad WITHOUT reconstructing the suspended exception frame on pVm->pFrame. Because` |
|        - | 1866 | `	 * pVm->pFrame stays the body, the return/finally/sRet subsystem is untouched (this is why` |
|        - | 1867 | `	 * the reverted frame-reconstruction approach's regression cannot recur). Behaviorally` |
|        - | 1868 | ``	 * identical to a `throw` executed at the yield point: if the generator's own try catches`` |
|        - | 1869 | `	 * it we resume after the try; otherwise it propagates to the throw() caller (ROOT B lands` |
|        - | 1870 | `	 * the caller's handler) and the ctx closes. pInjected is one-shot and only meaningful at` |
|        - | 1871 | `	 * the resume pc, so this is checked once at entry — NOT per-instruction — keeping the hot` |
|        - | 1872 | `	 * dispatch loop untouched for all normal code. The pFrame gate keeps a nested call / catch` |
|        - | 1873 | `	 * mini-program sharing the ctx (a separate VmByteCodeExec entry) from re-firing.` |
|        - | 1874 | `	 *` |
|        - | 1875 | ``	 * Exception: when this body is suspended mid `yield from` over an inner Generator`` |
|        - | 1876 | `	 * (iDelegateState==3), a Generator::throw() on the OUTER generator must be forwarded` |
|        - | 1877 | `	 * INTO the delegate (PHP yield-from transparency), not raised here. Leave pInjected` |
|        - | 1878 | `	 * set and skip; the resume pc is that OP_YIELD_FROM, which consumes and forwards it. */` |
|  3384819 | 1879 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected` |
|     1318 | 1880 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       63 | 1881 | `	 && pVm->pActiveCtx->iDelegateState != 3 ){` |
|       58 | 1882 | `		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;` |
|        - | 1883 | `		VmFrame *pThrowFrame;` |
|        - | 1884 | `		sxi32 iResumePc;` |
|       58 | 1885 | `		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */` |
|        - | 1886 | `		/* Raising the injection at the yield-from point abandons any array/Iterator` |
|        - | 1887 | `		 * delegation in progress (state 1/2 — state 3 was forwarded, not raised here).` |
|        - | 1888 | `		 * Tear the delegate down so that if the generator's own try catches this and` |
|        - | 1889 | ``		 * runs on to a LATER `yield from`, that opcode classifies its operand fresh`` |
|        - | 1890 | `		 * instead of resuming this now-stale delegate cursor. */` |
|       58 | 1891 | `		if( pVm->pActiveCtx->iDelegateState != 0 ){` |
|        3 | 1892 | `			PH7_MemObjRelease(&pVm->pActiveCtx->sDelegate);` |
|        3 | 1893 | `			pVm->pActiveCtx->pDelegateNode = 0;` |
|        3 | 1894 | `			pVm->pActiveCtx->iDelegateState = 0;` |
|        1 | 1895 | `		}` |
|       58 | 1896 | `		pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       58 | 1897 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|       58 | 1898 | `		rc = VmThrowException(&(*pVm),pInj);` |
|       58 | 1899 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1900 | `			goto Abort;` |
|        - | 1901 | `		}` |
|       58 | 1902 | `		if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 1903 | `			/* ROOT C: the inject was caught by an inline try in THIS generator. Drain the` |
|        - | 1904 | `			 * abandoned mid-expression operands and land at the catch/finally body. This is` |
|        - | 1905 | `			 * the pre-loop path (the first fetch uses pc directly), so no -1 adjustment. */` |
|       92 | 1906 | `			while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|       48 | 1907 | `				PH7_MemObjRelease(pTos);` |
|       48 | 1908 | `				pTos--;` |
|        4 | 1909 | `			}` |
|       48 | 1910 | `			pc = (sxi32)pVm->iInlinePc;` |
|       48 | 1911 | `			pVm->pInlineInstr = 0;` |
|       34 | 1912 | `		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 1913 | `			/* Caught by THIS generator's own try. (VmRecordedResume returns FALSE unless a` |
|        - | 1914 | `			 * catch recorded a resume target for this exec, so rc need not be pre-checked;` |
|        - | 1915 | `			 * unlike OP_THROW there is no lexical-try fallthrough here — the else just` |
|        - | 1916 | `			 * propagates.) Drain the abandoned mid-expression operand slots back to the` |
|        - | 1917 | `			 * catching try's base, then land at its pad (iResumePc is landing-1 for the` |
|        - | 1918 | `			 * dispatcher's trailing pc++; the loop below fetches at pc with no leading pc++,` |
|        - | 1919 | `			 * so add 1 to land on the pad itself). */` |
|      ! 0 | 1920 | `			while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      ! 0 | 1921 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 | 1922 | `				pTos--;` |
|      ! 0 | 1923 | `			}` |
|      ! 0 | 1924 | `			pc = iResumePc + 1;` |
|      ! 0 | 1925 | `		}else{` |
|        - | 1926 | `			/* Not caught in this generator (no match, or caught by an outer/caller frame` |
|        - | 1927 | `			 * that ROOT B will land at its own OP_CALL site): propagate out so the ctx` |
|        - | 1928 | `			 * closes and the caller sees the exception. */` |
|       12 | 1929 | `			goto Exception;` |
|        - | 1930 | `		}` |
|       22 | 1931 | `	}` |
|        - | 1932 | `	/* Force-close entry (VmCloseCtx): a suspended generator being destroyed runs its` |
|        - | 1933 | ``	 * pending `finally` blocks. Instead of resuming at the yield, redirect straight`` |
|        - | 1934 | ``	 * into the innermost open try's finally as if a `return` had crossed every`` |
|        - | 1935 | `	 * enclosing finally (mirrors OP_SET_FINALLY_RET; OP_END_FINALLY then threads the` |
|        - | 1936 | `	 * PH7_FA_RETURN out through the whole chain and completes the body). Gate on the` |
|        - | 1937 | `	 * BODY bytecode (aInstr == the function program) so nested mini-programs sharing` |
|        - | 1938 | `	 * this ctx/frame never re-fire it; bClosing stays set so OP_YIELD can reject a` |
|        - | 1939 | `	 * yield reached inside one of these finallys. */` |
|  3384809 | 1940 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing` |
|     1310 | 1941 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       57 | 1942 | `	 && aInstr == (VmInstr *)SySetBasePtr(&pVm->pActiveCtx->pFunc->aByteCode) ){` |
|        - | 1943 | `		VmFinallyAction sAct;` |
|       56 | 1944 | `		sxu32 iFpc = 0;` |
|       56 | 1945 | `		int nCross = -1; /* cross every enclosing finally of this body */` |
|       56 | 1946 | `		SyZero(&sAct,sizeof(sAct));` |
|       56 | 1947 | `		sAct.eKind = PH7_FA_RETURN;` |
|       56 | 1948 | `		sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       56 | 1949 | `		PH7_MemObjInit(pVm,&sAct.sRet); /* discarded return; getReturn() is moot post-close */` |
|       56 | 1950 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|       21 | 1951 | `			sAct.nCross = nCross;` |
|       21 | 1952 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       21 | 1953 | `			pc = (sxi32)iFpc; /* pre-loop: the first fetch uses pc directly (no -1) */` |
|       11 | 1954 | `		}else{` |
|        - | 1955 | `			/* No open try had a finally: nothing to run, complete the body. */` |
|       35 | 1956 | `			PH7_MemObjRelease(&sAct.sRet);` |
|       35 | 1957 | `			goto Done;` |
|        - | 1958 | `		}` |
|       10 | 1959 | `	}` |
|        - | 1960 | `	/* Execute as much as we can */` |
| 23003163 | 1961 | `	for(;;){` |
|      ! 0 | 1962 | `VmLoopFetch:` |
|        - | 1963 | `		/* C-boundary throw routing (band A #1): a PHP callee invoked from a C` |
|        - | 1964 | `		 * site with no status channel — a __toString/__toInt cast, __get/__set/` |
|        - | 1965 | `		 * offsetGet/offsetSet, __clone, __destruct, a user callback inside a` |
|        - | 1966 | `		 * builtin — raised, and the site continued with a fallback value; the` |
|        - | 1967 | `		 * invocation boundary parked the status here (VmBoundaryPark). Route it` |
|        - | 1968 | `		 * exactly as the throw site's dispatch macro would have: abort, land at` |
|        - | 1969 | `		 * an inline-try redirect, resume at a recorded in-place catch (draining` |
|        - | 1970 | `		 * the abandoned mid-expression operands to the catching try's base), or` |
|        - | 1971 | `		 * propagate out of this exec. Checked at the fetch point, so a swallowed` |
|        - | 1972 | `		 * throw outlives at most the C remainder of ONE opcode instead of` |
|        - | 1973 | `		 * silently resuming the surrounding PHP code with a bogus value.` |
|        - | 1974 | `		 * The pending write-back sweep shares this one guard so the hot` |
|        - | 1975 | `		 * no-hooks path pays a single predicted branch per fetch. */` |
| 47083095 | 1976 | `		if( pVm->nBoundaryRc != 0 \|\| SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      703 | 1977 | `			if( pVm->nBoundaryRc != 0 ){` |
|      283 | 1978 | `				sxi32 rcBr = pVm->nBoundaryRc;` |
|      283 | 1979 | `				pVm->nBoundaryRc = 0;` |
|      283 | 1980 | `				if( rcBr == PH7_ABORT ){` |
|        3 | 1981 | `					goto Abort;` |
|        - | 1982 | `				}` |
|      281 | 1983 | `				if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 1984 | `					/* Caught by an inline try (generator body) THIS exec owns: drain` |
|        - | 1985 | `					 * and land (pre-fetch path: pc is used directly, no trailing ++). */` |
|      ! 0 | 1986 | `					while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|      ! 0 | 1987 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 | 1988 | `						pTos--;` |
|      ! 0 | 1989 | `					}` |
|      ! 0 | 1990 | `					pc = (sxi32)pVm->iInlinePc;` |
|      ! 0 | 1991 | `					pVm->pInlineInstr = 0;` |
|      ! 0 | 1992 | `				}else{` |
|        - | 1993 | `					sxi32 iBrPc;` |
|      281 | 1994 | `					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){` |
|        - | 1995 | `						/* Caught in place by a try THIS exec owns: drain the abandoned` |
|        - | 1996 | `						 * operands to the catching try's base and land at its pad` |
|        - | 1997 | `						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;` |
|        - | 1998 | `						 * pre-fetch here, so +1 lands on the pad itself). */` |
|      347 | 1999 | `						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      189 | 2000 | `							PH7_MemObjRelease(pTos);` |
|      189 | 2001 | `							pTos--;` |
|        5 | 2002 | `						}` |
|      163 | 2003 | `						pc = iBrPc + 1;` |
|       84 | 2004 | `					}else{` |
|        - | 2005 | `						/* Caught by an outer exec (or uncaught-with-report pending):` |
|        - | 2006 | `						 * unwind out of this exec; the owner's macros/router land it. */` |
|      123 | 2007 | `						goto Exception;` |
|        - | 2008 | `					}` |
|        - | 2009 | `				}` |
|       79 | 2010 | `			}` |
|        - | 2011 | `			/* Stale write-back sweep: a pending entry whose OWNING activation is` |
|        - | 2012 | `			 * fetching any pc outside its armed window [nJmpPc, nPc] is dead — for` |
|        - | 2013 | `			 * an RMW entry (window = the modify op alone) a routed throw abandoned` |
|        - | 2014 | `			 * the arming statement mid-flight; for a ??= entry either a throw` |
|        - | 2015 | `			 * abandoned the RHS or the short-circuit jump landed past the` |
|        - | 2016 | `			 * OP_NULLC_STORE (the assign is skipped). Drop it — no set dispatch` |
|        - | 2017 | `			 * (php: the throw/skip discards the write). A nested exec (even a` |
|        - | 2018 | `			 * recursive one over the same bytecode) has a different operand-stack` |
|        - | 2019 | `			 * base and leaves enclosing entries alone; entries below a live top` |
|        - | 2020 | `			 * are reached as the drops expose them. */` |
|      595 | 2021 | `			while( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      421 | 2022 | `				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      420 | 2023 | `				if( pTopRmw->pOwnerStack != (void *)pStack \|\| pTopRmw->pInstrs != (void *)aInstr` |
|      161 | 2024 | `				 \|\| ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){` |
|      205 | 2025 | `					break; /* not ours, or legitimately in flight */` |
|        - | 2026 | `				}` |
|       13 | 2027 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 2028 | `			}` |
|      289 | 2029 | `		}` |
|        - | 2030 | `		/* Fetch the instruction to execute */` |
| 47082975 | 2031 | `		pInstr = &aInstr[pc];` |
| 47082975 | 2032 | `		if( pInstr->nLine ){` |
|        - | 2033 | `			/* Publish the source position for diagnostics, debug_backtrace() and` |
|        - | 2034 | `			 * Throwable. Instructions the compiler could not attribute (nLine 0)` |
|        - | 2035 | `			 * leave the last known line standing rather than reporting line 0.` |
|        - | 2036 | `			 * The compilation unit's strict_types mode rides the same gate: a` |
|        - | 2037 | `			 * SYNTHETIC instruction (nLine 0) must not claim to speak for a file. */` |
| 43656505 | 2038 | `			pVm->nCurLine = pInstr->nLine;` |
| 43656505 | 2039 | `			pVm->bCurStrict = pInstr->bStrict;` |
| 21848777 | 2040 | `		}` |
| 47082975 | 2041 | `		rc = SXRET_OK;` |
|        - | 2042 | `/*` |
|        - | 2043 | ` * What follows here is a massive switch statement where each case implements a` |
|        - | 2044 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - | 2045 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - | 2046 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - | 2047 | ` * the switch statement will break with convention and be flush-left.` |
|        - | 2048 | ` */` |
| 47082975 | 2049 | `		switch(pInstr->iOp){` |
|        - | 2050 | `/*` |
|        - | 2051 | ` * DONE: P1 * *` |
|        - | 2052 | ` *` |
|        - | 2053 | ` * Program execution completed: Clean up the mess left behind` |
|        - | 2054 | ` * and return immediately.` |
|        - | 2055 | ` */` |
|  1557733 | 2056 | `case PH7_OP_DONE:` |
|  3115910 | 2057 | `	if( pInstr->iP2 && sState.bReturnPropagates ){` |
|        - | 2058 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - | 2059 | `		 * onto the body frame this catch/finally returns from (skip the transparent` |
|        - | 2060 | `		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION` |
|        - | 2061 | `		 * materializes it into sState.pResult. Drain any finally opened within this body` |
|        - | 2062 | `		 * first (nested try/finally inside the catch), which may overwrite the same` |
|        - | 2063 | `		 * frame's slot (finally-over-catch). */` |
|    20283 | 2064 | `		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|    20283 | 2065 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    20277 | 2066 | `			PH7_MemObjStore(pTos,&pTgt->sRet);` |
|    20277 | 2067 | `			VmPopOperand(&pTos,1);` |
|    10141 | 2068 | `		}else{` |
|        8 | 2069 | ``			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */`` |
|        - | 2070 | `		}` |
|    20283 | 2071 | `		pTgt->bHasRet = 1;` |
|    20283 | 2072 | `		pTgt->nRetGen++;` |
|    20283 | 2073 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    20283 | 2074 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2075 | `			goto Abort;` |
|        - | 2076 | `		}` |
|    20283 | 2077 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 2078 | `			/* A drained finally threw past itself — it discards this return. */` |
|      ! 0 | 2079 | `			goto Exception;` |
|        - | 2080 | `		}` |
|    20283 | 2081 | `		goto Done;` |
|        - | 2082 | `	}` |
|        - | 2083 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - | 2084 | `	 * the fiber start/resume paths) set sState.pEnforceRetFunc, so this branch is` |
|        - | 2085 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - | 2086 | `	 * callback trampolines, and the main script. */` |
|  3095627 | 2087 | `	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)` |
|    11853 | 2088 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - | 2089 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - | 2090 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - | 2091 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - | 2092 | `		 * value the function never actually returned, so enforcing here would` |
|        - | 2093 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - | 2094 | `		 * exception. */` |
|    11853 | 2095 | `		ph7_value *pRetVal = 0;` |
|    11853 | 2096 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    10079 | 2097 | `			pRetVal = pTos;` |
|     5037 | 2098 | `		}` |
|    11853 | 2099 | `		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);` |
|    11853 | 2100 | `		if( rc == PH7_ABORT ) goto Abort;` |
|    11849 | 2101 | `		if( rc == PH7_EXCEPTION ){` |
|      147 | 2102 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      113 | 2103 | `				PH7_MemObjRelease(pTos);` |
|      113 | 2104 | `				pTos--;` |
|       54 | 2105 | `			}` |
|      147 | 2106 | `			goto Exception;` |
|        - | 2107 | `		}` |
|        - | 2108 | `		/* Don't enforce twice if the function loops through multiple` |
|        - | 2109 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - | 2110 | `		 * defensively we clear the pointer after a successful check). */` |
|    11707 | 2111 | `		sState.pEnforceRetFunc = 0;` |
|     5851 | 2112 | `	}` |
|  3095486 | 2113 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|  1630932 | 2114 | `		if( sState.pLastRef ){` |
|   124773 | 2115 | `			*sState.pLastRef = pTos->nIdx;` |
|    62606 | 2116 | `		}` |
|  1630932 | 2117 | `		if( sState.pResult ){` |
|        - | 2118 | `			/* Execution result */` |
|   165318 | 2119 | `			PH7_MemObjStore(pTos,sState.pResult);` |
|    82876 | 2120 | `		}` |
|  1630932 | 2121 | `		VmPopOperand(&pTos,1);` |
|   815688 | 2122 | `	}else{` |
|  1464559 | 2123 | `		if( pInstr->iP1 == 0 && pInstr->iP2 && sState.pResult ){` |
|        - | 2124 | ``			/* An EXPLICIT `return;` with no value answers NULL. It reads as a`` |
|        - | 2125 | `			 * no-op for a function (whose result slot starts out null anyway)` |
|        - | 2126 | `			 * and matters for an included CHUNK, whose slot is seeded with the 1` |
|        - | 2127 | ``			 * a file that returns nothing answers: `<?php return;` is php's`` |
|        - | 2128 | `			 * NULL, not that 1. */` |
|       41 | 2129 | `			PH7_MemObjRelease(sState.pResult);` |
|       19 | 2130 | `		}` |
|        - | 2131 | `		/* Nothing referenced — also the throw-unwind path: the compiler routes` |
|        - | 2132 | `		 * an uncaught exception to this terminal OP_DONE with iP1 set but an` |
|        - | 2133 | `		 * empty operand stack (pTos == pStack-1), so there is no return value to` |
|        - | 2134 | `		 * store. Guarding on pTos >= pStack (matching the sibling branch above)` |
|        - | 2135 | `		 * avoids the below-base read that crashed under glibc/ASan. */` |
|  1464559 | 2136 | `		if( sState.pLastRef ){` |
|     9963 | 2137 | `			*sState.pLastRef = SXU32_HIGH;` |
|     4979 | 2138 | `		}` |
|        - | 2139 | `	}` |
|        - | 2140 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - | 2141 | `	 * this execution. When 'return' is used inside a try block,` |
|        - | 2142 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - | 2143 | `	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering` |
|        - | 2144 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - | 2145 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - | 2146 | `	 * block can override it (the finally writes this body frame's sRet slot,` |
|        - | 2147 | `	 * materialized below).` |
|        - | 2148 | `	 */` |
|  3095486 | 2149 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|  3095486 | 2150 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2151 | `		goto Abort;` |
|        - | 2152 | `	}` |
|  3095486 | 2153 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 2154 | `		/* A drained finally threw past itself, discarding the value this OP_DONE` |
|        - | 2155 | `		 * stored into sState.pResult. If an enclosing try IN THIS function caught the new` |
|        - | 2156 | `		 * exception in place, resume at its landing pad; otherwise unwind (the` |
|        - | 2157 | `		 * caller's exception-resume pops the stored result). */` |
|        - | 2158 | `		sxi32 iResumePc;` |
|        5 | 2159 | `		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 2160 | `			pc = iResumePc;` |
|        3 | 2161 | `			break;` |
|        - | 2162 | `		}` |
|        3 | 2163 | `		goto Exception;` |
|        - | 2164 | `	}` |
|  3095482 | 2165 | `	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){` |
|        - | 2166 | `		/* A catch/finally issued a 'return' targeting THIS body. If the body is` |
|        - | 2167 | `		 * actually unwinding because an exception escaped it (terminal OP_DONE on` |
|        - | 2168 | `		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type` |
|        - | 2169 | `		 * enforcement above), that exception supersedes the return: discard it.` |
|        - | 2170 | `		 * Otherwise materialize it as this function's result. */` |
|       14 | 2171 | `		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){` |
|      ! 0 | 2172 | `			VmClearFramePending(sState.pEntryFrame);` |
|      ! 0 | 2173 | `		}else{` |
|       14 | 2174 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|        - | 2175 | `		}` |
|        5 | 2176 | `	}` |
|  3095482 | 2177 | `	goto Done;` |
|        - | 2178 | `/*` |
|        - | 2179 | ` * HALT: P1 * *` |
|        - | 2180 | ` *` |
|        - | 2181 | ` * Program execution aborted: Clean up the mess left behind` |
|        - | 2182 | ` * and abort immediately.` |
|        - | 2183 | ` */` |
|       41 | 2184 | `case PH7_OP_HALT:` |
|       86 | 2185 | `	if( pInstr->iP1 ){` |
|        - | 2186 | `#ifdef UNTRUST` |
|        - | 2187 | `		if( pTos < pStack ){` |
|        - | 2188 | `			goto Abort;` |
|        - | 2189 | `		}` |
|        - | 2190 | `#endif` |
|       86 | 2191 | `		if( sState.pLastRef ){` |
|       63 | 2192 | `			*sState.pLastRef = pTos->nIdx;` |
|       30 | 2193 | `		}` |
|       86 | 2194 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       72 | 2195 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - | 2196 | `				/* Output the exit message */` |
|      106 | 2197 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|       34 | 2198 | `					pVm->sVmConsumer.pUserData);` |
|       72 | 2199 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|       38 | 2200 | `			}` |
|       50 | 2201 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - | 2202 | `			/* Record exit status */` |
|       16 | 2203 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        7 | 2204 | `		}` |
|       86 | 2205 | `		VmPopOperand(&pTos,1);` |
|       41 | 2206 | `	}else if( sState.pLastRef ){` |
|        - | 2207 | `		/* Nothing referenced */` |
|      ! 0 | 2208 | `		*sState.pLastRef = SXU32_HIGH;` |
|      ! 0 | 2209 | `	}` |
|        - | 2210 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - | 2211 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - | 2212 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - | 2213 | `	 */` |
|       86 | 2214 | `	pVm->bHaltRequested = 1;` |
|       86 | 2215 | `	goto Abort;` |
|        - | 2216 | `/*` |
|        - | 2217 | ` * JMP: * P2 *` |
|        - | 2218 | ` *` |
|        - | 2219 | ` * Unconditional jump: The next instruction executed will be` |
|        - | 2220 | ` * the one at index P2 from the beginning of the program.` |
|        - | 2221 | ` */` |
|   485127 | 2222 | `case PH7_OP_JMP:` |
|   971100 | 2223 | `	pc = pInstr->iP2 - 1;` |
|   971100 | 2224 | `	break;` |
|        - | 2225 | `/*` |
|        - | 2226 | ` * JZ: P1 P2 *` |
|        - | 2227 | ` *` |
|        - | 2228 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - | 2229 | ` * entry in the stack if P1 is zero.` |
|        - | 2230 | ` */` |
|  1291005 | 2231 | `case PH7_OP_JZ:` |
|        - | 2232 | `#ifdef UNTRUST` |
|        - | 2233 | `	if( pTos < pStack ){` |
|        - | 2234 | `		goto Abort;` |
|        - | 2235 | `	}` |
|        - | 2236 | `#endif` |
|        - | 2237 | `	/* Get a boolean value */` |
|  2586178 | 2238 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      931 | 2239 | `		PH7_MemObjToBool(pTos);` |
|      463 | 2240 | `	}` |
|  2586178 | 2241 | `	if( !pTos->x.iVal ){` |
|        - | 2242 | `		/* Take the jump */` |
|  1403340 | 2243 | `		pc = pInstr->iP2 - 1;` |
|   702881 | 2244 | `	}` |
|  2586178 | 2245 | `	if( !pInstr->iP1 ){` |
|  2229380 | 2246 | `		VmPopOperand(&pTos,1);` |
|  1116570 | 2247 | `	}` |
|  2586178 | 2248 | `	break;` |
|        - | 2249 | `/*` |
|        - | 2250 | ` * JNZ: P1 P2 *` |
|        - | 2251 | ` *` |
|        - | 2252 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - | 2253 | ` * entry in the stack if P1 is zero.` |
|        - | 2254 | ` */` |
|   145363 | 2255 | `case PH7_OP_JNZ:` |
|        - | 2256 | `#ifdef UNTRUST` |
|        - | 2257 | `	if( pTos < pStack ){` |
|        - | 2258 | `		goto Abort;` |
|        - | 2259 | `	}` |
|        - | 2260 | `#endif` |
|        - | 2261 | `	/* Get a boolean value */` |
|   291176 | 2262 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 2263 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 2264 | `	}` |
|   291176 | 2265 | `	if( pTos->x.iVal ){` |
|        - | 2266 | `		/* Take the jump */` |
|     9487 | 2267 | `		pc = pInstr->iP2 - 1;` |
|     4741 | 2268 | `	}` |
|   291176 | 2269 | `	if( !pInstr->iP1 ){` |
|        8 | 2270 | `		VmPopOperand(&pTos,1);` |
|        3 | 2271 | `	}` |
|   291176 | 2272 | `	break;` |
|        - | 2273 | `/*` |
|        - | 2274 | ` * NOOP: * * *` |
|        - | 2275 | ` *` |
|        - | 2276 | ` * Do nothing. This instruction is often useful as a jump` |
|        - | 2277 | ` * destination.` |
|        - | 2278 | ` */` |
|      ! 0 | 2279 | `case PH7_OP_NOOP:` |
|      ! 0 | 2280 | `	break;` |
|        - | 2281 | `/*` |
|        - | 2282 | ` * POP: P1 * *` |
|        - | 2283 | ` *` |
|        - | 2284 | ` * Pop P1 elements from the operand stack.` |
|        - | 2285 | ` */` |
|  1070776 | 2286 | `case PH7_OP_POP: {` |
|  2144579 | 2287 | `	sxi32 n = pInstr->iP1;` |
|  2144579 | 2288 | `	if( &pTos[-n+1] < pStack ){` |
|        - | 2289 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      412 | 2290 | `		n = (sxi32)(pTos - pStack);` |
|      204 | 2291 | `	}` |
|  2144579 | 2292 | `	VmPopOperand(&pTos,n);` |
|  2144579 | 2293 | `	break;` |
|        - | 2294 | `				 }` |
|        - | 2295 | `/*` |
|        - | 2296 | ` * DUP: * * *` |
|        - | 2297 | ` *` |
|        - | 2298 | ` * Duplicate the top of the stack.` |
|        - | 2299 | ` */` |
|      112 | 2300 | `case PH7_OP_DUP:` |
|        - | 2301 | `#ifdef UNTRUST` |
|        - | 2302 | `	if( pTos < pStack ){` |
|        - | 2303 | `		goto Abort;` |
|        - | 2304 | `	}` |
|        - | 2305 | `#endif` |
|      229 | 2306 | `	pTos++;` |
|      229 | 2307 | `	PH7_MemObjInit(pVm,pTos);` |
|      229 | 2308 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|      229 | 2309 | `	break;` |
|        - | 2310 | `/*` |
|        - | 2311 | ` * CLASS_DEFER: * * P3` |
|        - | 2312 | ` *` |
|        - | 2313 | ` * Execute a class declaration whose parent/interface/trait could not be` |
|        - | 2314 | ` * resolved when the enclosing file compiled (its autoloader had not RUN yet).` |
|        - | 2315 | ` * P3 is the VmDeferredClass record captured by the compiler. A dependency` |
|        - | 2316 | `` * that is STILL missing throws php's catchable `... not found` Error; a`` |
|        - | 2317 | ` * failed re-compile aborts (its errors were already reported). See the` |
|        - | 2318 | ` * deferral block comment in compile_class.c.` |
|        - | 2319 | ` */` |
|       15 | 2320 | `case PH7_OP_CLASS_DEFER: {` |
|       33 | 2321 | `	VmDeferredClass *pDefer = (VmDeferredClass *)pInstr->p3;` |
|       33 | 2322 | `	VmDeferredReq *pMissing = 0;` |
|       33 | 2323 | `	sxi32 rcDecl = SXRET_OK;` |
|       33 | 2324 | `	if( pDefer ){` |
|       33 | 2325 | `		rcDecl = VmExecDeferredClass(&(*pVm),pDefer,&pMissing);` |
|       15 | 2326 | `	}` |
|       33 | 2327 | `	if( pMissing ){` |
|        - | 2328 | `		static const char *azDeferKind[] = { "Class", "Interface", "Trait" };` |
|        - | 2329 | `		char zDeclMsg[520];` |
|       17 | 2330 | `		sxu32 nDeclMsg = SyBufferFormat(zDeclMsg,sizeof(zDeclMsg),"%s \"%.*s\" not found",` |
|        5 | 2331 | `			azDeferKind[pMissing->cKind < 3 ? pMissing->cKind : 0],` |
|       10 | 2332 | `			(int)pMissing->sName.nByte,pMissing->sName.zString);` |
|       12 | 2333 | `		rc = VmThrowFromVm(&(*pVm),"Error",zDeclMsg,nDeclMsg);` |
|       12 | 2334 | `		if( rc == SXERR_ABORT ){` |
|        3 | 2335 | `			goto Abort;` |
|        - | 2336 | `		}` |
|        9 | 2337 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2338 | `	}` |
|       22 | 2339 | `	if( rcDecl == SXERR_ABORT ){` |
|      ! 0 | 2340 | `		goto Abort;` |
|        - | 2341 | `	}` |
|       22 | 2342 | `	break;` |
|        - | 2343 | `				}` |
|        - | 2344 | `/*` |
|        - | 2345 | ` * CVT_INT: * * *` |
|        - | 2346 | ` *` |
|        - | 2347 | ` * Force the top of the stack to be an integer.` |
|        - | 2348 | ` */` |
|      371 | 2349 | `case PH7_OP_CVT_INT:` |
|        - | 2350 | `#ifdef UNTRUST` |
|        - | 2351 | `	if( pTos < pStack ){` |
|        - | 2352 | `		goto Abort;` |
|        - | 2353 | `	}` |
|        - | 2354 | `#endif` |
|      747 | 2355 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      579 | 2356 | `		PH7_MemObjToInteger(pTos);` |
|      287 | 2357 | `	}` |
|        - | 2358 | `	/* Invalidate any prior representation */` |
|      747 | 2359 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      747 | 2360 | `	break;` |
|        - | 2361 | `/*` |
|        - | 2362 | ` * CVT_REAL: * * *` |
|        - | 2363 | ` *` |
|        - | 2364 | ` * Force the top of the stack to be a real.` |
|        - | 2365 | ` */` |
|       43 | 2366 | `case PH7_OP_CVT_REAL:` |
|        - | 2367 | `#ifdef UNTRUST` |
|        - | 2368 | `	if( pTos < pStack ){` |
|        - | 2369 | `		goto Abort;` |
|        - | 2370 | `	}` |
|        - | 2371 | `#endif` |
|       89 | 2372 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 | 2373 | `		PH7_MemObjToReal(pTos);` |
|       26 | 2374 | `	}` |
|        - | 2375 | `	/* Invalidate any prior representation */` |
|       89 | 2376 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       89 | 2377 | `	break;` |
|        - | 2378 | `/*` |
|        - | 2379 | ` * CVT_STR: * * *` |
|        - | 2380 | ` *` |
|        - | 2381 | ` * Force the top of the stack to be a string.` |
|        - | 2382 | ` */` |
|     1768 | 2383 | `case PH7_OP_CVT_STR:` |
|        - | 2384 | `#ifdef UNTRUST` |
|        - | 2385 | `	if( pTos < pStack ){` |
|        - | 2386 | `		goto Abort;` |
|        - | 2387 | `	}` |
|        - | 2388 | `#endif` |
|        - | 2389 | `	/* (string) cast + string interpolation "$arr": php's user-visible` |
|        - | 2390 | `	 * array->string warning site, and the not-stringable-object throw (§2). */` |
|        - | 2391 | `	{` |
|     3541 | 2392 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     3543 | 2393 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2394 | `	}` |
|     3461 | 2395 | `	break;` |
|        - | 2396 | `/*` |
|        - | 2397 | ` * CVT_BOOL: * * *` |
|        - | 2398 | ` *` |
|        - | 2399 | ` * Force the top of the stack to be a boolean.` |
|        - | 2400 | ` */` |
|       30 | 2401 | `case PH7_OP_CVT_BOOL:` |
|        - | 2402 | `#ifdef UNTRUST` |
|        - | 2403 | `	if( pTos < pStack ){` |
|        - | 2404 | `		goto Abort;` |
|        - | 2405 | `	}` |
|        - | 2406 | `#endif` |
|       64 | 2407 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       62 | 2408 | `		PH7_MemObjToBool(pTos);` |
|       29 | 2409 | `	}` |
|       64 | 2410 | `	break;` |
|        - | 2411 | `/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it` |
|        - | 2412 | ` * always raises "The (unset) cast is no longer supported" (php 8 removed` |
|        - | 2413 | ` * the cast), so a program containing it never compiles. The switch has no` |
|        - | 2414 | ` * default arm, so abort loudly rather than fall through as a silent no-op` |
|        - | 2415 | ` * if a future emitter ever produces one without the compile error. */` |
|      ! 0 | 2416 | `case PH7_OP_CVT_NULL:` |
|      ! 0 | 2417 | `	goto Abort;` |
|        - | 2418 | `/*` |
|        - | 2419 | ` * CVT_NUMC: * * *` |
|        - | 2420 | ` *` |
|        - | 2421 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - | 2422 | ` */` |
|      ! 0 | 2423 | `case PH7_OP_CVT_NUMC:` |
|        - | 2424 | `#ifdef UNTRUST` |
|        - | 2425 | `	if( pTos < pStack ){` |
|        - | 2426 | `		goto Abort;` |
|        - | 2427 | `	}` |
|        - | 2428 | `#endif` |
|        - | 2429 | `	/* Force a numeric cast */` |
|      ! 0 | 2430 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 | 2431 | `	break;` |
|        - | 2432 | `/*` |
|        - | 2433 | ` * CVT_ARRAY: * * *` |
|        - | 2434 | ` *` |
|        - | 2435 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - | 2436 | ` */` |
|       68 | 2437 | `case PH7_OP_CVT_ARRAY:` |
|        - | 2438 | `#ifdef UNTRUST` |
|        - | 2439 | `	if( pTos < pStack ){` |
|        - | 2440 | `		goto Abort;` |
|        - | 2441 | `	}` |
|        - | 2442 | `#endif` |
|        - | 2443 | `	/* Force a hashmap cast */` |
|      141 | 2444 | `	rc = PH7_MemObjToHashmap(pTos);` |
|      141 | 2445 | `	if( rc != SXRET_OK ){` |
|        - | 2446 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 | 2447 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - | 2448 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 | 2449 | `	}` |
|      141 | 2450 | `	break;` |
|        - | 2451 | `/*` |
|        - | 2452 | ` * CVT_OBJ: * * *` |
|        - | 2453 | ` *` |
|        - | 2454 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - | 2455 | ` */` |
|       26 | 2456 | `case PH7_OP_CVT_OBJ:` |
|        - | 2457 | `#ifdef UNTRUST` |
|        - | 2458 | `	if( pTos < pStack ){` |
|        - | 2459 | `		goto Abort;` |
|        - | 2460 | `	}` |
|        - | 2461 | `#endif` |
|       54 | 2462 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 2463 | `		/* Force a 'stdClass()' cast */` |
|       54 | 2464 | `		PH7_MemObjToObject(pTos);` |
|       26 | 2465 | `	}` |
|       54 | 2466 | `	break;` |
|        - | 2467 | `/*` |
|        - | 2468 | ` * ERR_CTRL * * *` |
|        - | 2469 | ` *` |
|        - | 2470 | ` * Error control operator.` |
|        - | 2471 | ` */` |
|     3858 | 2472 | `case PH7_OP_UNSET_VAR: {` |
|        - | 2473 | `	VmOpRc rcOp;` |
|     7721 | 2474 | `	sState.pTos = pTos;` |
|     7721 | 2475 | `	sState.pc = pc;` |
|     7721 | 2476 | `	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);` |
|     7721 | 2477 | `	pTos = sState.pTos;` |
|     7721 | 2478 | `	pc = sState.pc;` |
|     7721 | 2479 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 2480 | `		goto Abort;` |
|     7719 | 2481 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2482 | `		goto Exception;` |
|        - | 2483 | `	}` |
|     7719 | 2484 | `	break;` |
|        - | 2485 | `					  }` |
|    41448 | 2486 | `case PH7_OP_ERR_CTRL:` |
|        - | 2487 | `	/*` |
|        - | 2488 | `	 * Error-control operator '@'. Emitted as a PAIR around the suppressed` |
|        - | 2489 | `	 * expression: iP1=1 opens the window (before the operand is evaluated),` |
|        - | 2490 | `	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),` |
|        - | 2491 | `	 * which went unnoticed only because the engine raised so few diagnostics;` |
|        - | 2492 | `	 * every warning/notice/deprecation the parity work added leaked straight` |
|        - | 2493 | ``	 * through `@`. The window nests, and php 8 does NOT let '@' swallow`` |
|        - | 2494 | `	 * fatals or exceptions — those unwind past the closing instruction, so` |
|        - | 2495 | `	 * the depth is reset on the exception path rather than decremented here.` |
|        - | 2496 | `	 */` |
|    82901 | 2497 | `	if( pInstr->iP1 ){` |
|    41529 | 2498 | `		pVm->nErrSuppress++;` |
|    62139 | 2499 | `	}else if( pVm->nErrSuppress > 0 ){` |
|    41377 | 2500 | `		pVm->nErrSuppress--;` |
|    20686 | 2501 | `	}` |
|    82901 | 2502 | `	break;` |
|        - | 2503 | `/*` |
|        - | 2504 | ` * IS_A * * *` |
|        - | 2505 | ` *` |
|        - | 2506 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - | 2507 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - | 2508 | ` * holding a class name or an object).` |
|        - | 2509 | ` * Push TRUE on success. FALSE otherwise.` |
|        - | 2510 | ` */` |
|      234 | 2511 | `case PH7_OP_IS_A:{` |
|      473 | 2512 | `	ph7_value *pNos = &pTos[-1];` |
|      473 | 2513 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - | 2514 | `#ifdef UNTRUST` |
|        - | 2515 | `	if( pNos < pStack ){` |
|        - | 2516 | `		goto Abort;` |
|        - | 2517 | `	}` |
|        - | 2518 | `#endif` |
|      473 | 2519 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      463 | 2520 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      463 | 2521 | `		ph7_class *pClass = 0;` |
|        - | 2522 | `		/* Extract the target class */` |
|      463 | 2523 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - | 2524 | `			/* Instance already loaded */` |
|      ! 0 | 2525 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      463 | 2526 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      463 | 2527 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      463 | 2528 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - | 2529 | `			/* Handle self/static/parent keywords */` |
|      463 | 2530 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        6 | 2531 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      461 | 2532 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 | 2533 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      458 | 2534 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        6 | 2535 | `				pClass = PH7_VmResolveParentClass(&(*pVm));` |
|        4 | 2536 | `			}else{` |
|      453 | 2537 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - | 2538 | `			}` |
|      229 | 2539 | `		}` |
|      463 | 2540 | `		if( pClass ){` |
|        - | 2541 | `			/* Perform the query */` |
|      461 | 2542 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|      228 | 2543 | `		}` |
|      229 | 2544 | `	}` |
|        - | 2545 | `	/* Push result */` |
|      473 | 2546 | `	VmPopOperand(&pTos,1);` |
|      473 | 2547 | `	PH7_MemObjRelease(pTos);` |
|      473 | 2548 | `	pTos->x.iVal = iRes;` |
|      473 | 2549 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      473 | 2550 | `	break;` |
|        - | 2551 | `				 }` |
|        - | 2552 |  |
|        - | 2553 | `/*` |
|        - | 2554 | ` * LOADC P1 P2 *` |
|        - | 2555 | ` *` |
|        - | 2556 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - | 2557 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - | 2558 | ` */` |
|  4691372 | 2559 | `case PH7_OP_LOADC: {` |
|        - | 2560 | `	ph7_value *pObj;` |
|        - | 2561 | `	/* Reserve a room */` |
|  9387738 | 2562 | `	pTos++;` |
|  9387738 | 2563 | `	if( pInstr->iP1 & PH7_LOADC_NOKEY ){` |
|        - | 2564 | `		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */` |
|   732689 | 2565 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|   732689 | 2566 | `		SyBlobReset(&pTos->sBlob);` |
|   732689 | 2567 | `		pTos->iFlags \|= MEMOBJ_AUX_NOKEY;` |
|   732689 | 2568 | `		pTos->nIdx = SXU32_HIGH;` |
|   732689 | 2569 | `		break;` |
|        - | 2570 | `	}` |
|  8655054 | 2571 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  8655054 | 2572 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - | 2573 | `			SyHashEntry *pEntry;` |
|        - | 2574 | `			/* The name php looks up FIRST for an unqualified constant is decided at` |
|        - | 2575 | ``			 * COMPILE time and travels in p3: a `use const` import's FQN, or`` |
|        - | 2576 | ``			 * `current-namespace\NAME`. Absent (global scope, or an already-qualified`` |
|        - | 2577 | `			 * literal), the bare literal is the only name there is. Resolving it here` |
|        - | 2578 | `			 * rather than against the RUNTIME namespace is what makes a function keep` |
|        - | 2579 | `			 * its own namespace when it is called from another one, and what lets a` |
|        - | 2580 | `			 * namespaced constant shadow a global one of the same short name. */` |
|   133144 | 2581 | `			const char *zCand = (const char *)pInstr->p3;` |
|   133144 | 2582 | `			const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|   133144 | 2583 | `			sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|   133144 | 2584 | `			if( zCand ){` |
|       55 | 2585 | `				pEntry = SyHashGet(&pVm->hConstant,zCand,SyStrlen(zCand));` |
|       55 | 2586 | `				if( pEntry ){` |
|       47 | 2587 | `					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       47 | 2588 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       47 | 2589 | `					SyBlobReset(&pTos->sBlob);` |
|       47 | 2590 | `					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|       47 | 2591 | `					pTos->nIdx = SXU32_HIGH;` |
|       47 | 2592 | `					break;` |
|        - | 2593 | `				}` |
|        4 | 2594 | `			}` |
|        - | 2595 | `			/* The GLOBAL step — skipped when the candidate came from an import, which` |
|        - | 2596 | `			 * php resolves without any fallback. */` |
|   133102 | 2597 | `			if( (pInstr->iP1 & PH7_LOADC_NOGLOBAL) == 0 ){` |
|   133100 | 2598 | `				pEntry = SyHashGet(&pVm->hConstant,(const void *)zLit,nLit);` |
|   133100 | 2599 | `				if( pEntry ){` |
|   132928 | 2600 | `					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - | 2601 | `					/* Set a NULL default value */` |
|   132928 | 2602 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|   132928 | 2603 | `					SyBlobReset(&pTos->sBlob);` |
|        - | 2604 | `					/* Invoke the callback and deal with the expanded value */` |
|   132928 | 2605 | `					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|        - | 2606 | `					/* Mark as constant */` |
|   132928 | 2607 | `					pTos->nIdx = SXU32_HIGH;` |
|   132928 | 2608 | `					break;` |
|        - | 2609 | `				}` |
|       86 | 2610 | `			}` |
|        - | 2611 | `			{` |
|        - | 2612 | `				/*` |
|        - | 2613 | `				 * php 8 has no bare-word fallback: an unresolved constant is a catchable` |
|        - | 2614 | `				 * Error, not its own name as a string. PH7 answered "X" for an unknown` |
|        - | 2615 | `				 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —` |
|        - | 2616 | `				 * silently became a string and flowed on. php names the name it looked` |
|        - | 2617 | `				 * for FIRST, so the message reports the candidate when there was one` |
|        - | 2618 | ``				 * ("Undefined constant \"B\NOPE\"" inside `namespace B;`).`` |
|        - | 2619 | `				 *` |
|        - | 2620 | `				 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call` |
|        - | 2621 | ``				 * boundary, so neither a bare `break` nor `goto Exception` is correct`` |
|        - | 2622 | `				 * here (see the macro).` |
|        - | 2623 | `				 */` |
|        - | 2624 | `				SyBlob sMsg;` |
|      179 | 2625 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      179 | 2626 | `				if( zCand ){` |
|        8 | 2627 | `					SyBlobFormat(&sMsg,"Undefined constant \"%s\"",zCand);` |
|        5 | 2628 | `				}else{` |
|      173 | 2629 | `					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);` |
|        - | 2630 | `				}` |
|      179 | 2631 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      179 | 2632 | `				SyBlobReset(&pTos->sBlob);` |
|      179 | 2633 | `				pTos->nIdx = SXU32_HIGH;` |
|      266 | 2634 | `				rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|       87 | 2635 | `					SyBlobLength(&sMsg));` |
|      179 | 2636 | `				SyBlobRelease(&sMsg);` |
|      179 | 2637 | `				if( rc == SXERR_ABORT ){` |
|       49 | 2638 | `					goto Abort;` |
|        - | 2639 | `				}` |
|      157 | 2640 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2641 | `			}` |
|        - | 2642 | `		}` |
|  8521915 | 2643 | `		PH7_MemObjLoad(pObj,pTos);` |
|  4263456 | 2644 | `	}else{` |
|        - | 2645 | `		/* Set a NULL value */` |
|      ! 0 | 2646 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 2647 | `	}` |
|        - | 2648 | `	/* Mark as constant */` |
|  8521915 | 2649 | `	pTos->nIdx = SXU32_HIGH;` |
|  8521915 | 2650 | `	break;` |
|        - | 2651 | `				  }` |
|        - | 2652 | `/*` |
|        - | 2653 | ` * LOAD: P1 * P3` |
|        - | 2654 | ` *` |
|        - | 2655 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - | 2656 | ` * from the P3 operand.` |
|        - | 2657 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - | 2658 | ` * the variable if non existent and push the NULL constant instead.` |
|        - | 2659 | ` */` |
|  3717548 | 2660 | `case PH7_OP_LOAD:{` |
|        - | 2661 | `	ph7_value *pObj;` |
|        - | 2662 | `	SyString sName;` |
|  7446635 | 2663 | `	if( pInstr->p3 == 0 ){` |
|        - | 2664 | `		/* Take the variable name from the top of the stack */` |
|        - | 2665 | `#ifdef UNTRUST` |
|        - | 2666 | `		if( pTos < pStack ){` |
|        - | 2667 | `			goto Abort;` |
|        - | 2668 | `		}` |
|        - | 2669 | `#endif` |
|        - | 2670 | `		/* Force a string cast — variable-variable name $$arr (user-visible, §2) */` |
|        - | 2671 | `		{` |
|       40 | 2672 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|  3717587 | 2673 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2674 | `		}` |
|       38 | 2675 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       21 | 2676 | `	}else{` |
|  7446599 | 2677 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 2678 | `		/* Reserve a room for the target object */` |
|  7446599 | 2679 | `		pTos++;` |
|        - | 2680 | `	}` |
|  7446633 | 2681 | `	if( pInstr->iP2 == 2 ){` |
|        - | 2682 | ``		/* Read-modify-write target (`$x++`, `$x .= 'a'`): php reads the variable`` |
|        - | 2683 | `		 * before writing, so it warns when it does not exist and THEN seeds it.` |
|        - | 2684 | `		 * Peek first (no create) purely to raise that warning; the load below` |
|        - | 2685 | ``		 * still creates the slot the operator needs. A plain `=` never gets here`` |
|        - | 2686 | `		 * — it writes without reading, and stays silent, as php does. */` |
|   742588 | 2687 | `		if( VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE) == 0 ){` |
|        7 | 2688 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        3 | 2689 | `		}` |
|   371722 | 2690 | `	}` |
|        - | 2691 | `	/* Extract the requested memory object */` |
|  7446633 | 2692 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  7446633 | 2693 | `	if( pObj == 0 ){` |
|      149 | 2694 | `		if( pInstr->iP1 ){` |
|        - | 2695 | `			/* Reading a variable that does not exist. php raises E_WARNING` |
|        - | 2696 | `			 * "Undefined variable $x" and evaluates it as NULL; PHL used to` |
|        - | 2697 | `			 * yield NULL silently, which hid typo'd names. iP2 marks the reads` |
|        - | 2698 | `			 * that must stay quiet (isset/empty — see PH7_CompileVariable);` |
|        - | 2699 | `			 * vivifying contexts never get here because they pass iP1 = 0. */` |
|      149 | 2700 | `			if( pInstr->iP2 == 0 ){` |
|       44 | 2701 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|       20 | 2702 | `			}` |
|        - | 2703 | `			/* Variable not found,load NULL */` |
|      149 | 2704 | `			if( !pInstr->p3 ){` |
|       10 | 2705 | `				PH7_MemObjRelease(pTos);` |
|        6 | 2706 | `			}else{` |
|      141 | 2707 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 2708 | `			}` |
|      149 | 2709 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|      149 | 2710 | `			if( pInstr->iP2 == 3 ){` |
|        - | 2711 | `				/* D1 deferred call argument: the variable does not exist, but we do not` |
|        - | 2712 | `				 * know yet whether the callee wants it by-ref (materialize + bind) or` |
|        - | 2713 | `				 * by-value (warn + pass NULL). Tag the slot and stash the variable name` |
|        - | 2714 | `				 * (a VM-lifetime bytecode string, nothing to free) so OP_CALL's` |
|        - | 2715 | `				 * PH7_VmResolveDeferredArgs can decide once the callee is resolved. */` |
|       28 | 2716 | `				pTos->iFlags \|= MEMOBJ_AUX_DEFERRED;` |
|       28 | 2717 | `				pTos->x.pOther = pInstr->p3;` |
|       13 | 2718 | `			}` |
|      149 | 2719 | `			break;` |
|      ! 0 | 2720 | `		}else{` |
|        - | 2721 | `			/* Fatal error */` |
|      ! 0 | 2722 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 2723 | `			goto Abort;` |
|        - | 2724 | `		}` |
|        - | 2725 | `	}` |
|        - | 2726 | `	/* Load variable contents */` |
|  7446489 | 2727 | `	PH7_MemObjLoad(pObj,pTos);` |
|  7446489 | 2728 | `	pTos->nIdx = pObj->nIdx;` |
|  7446489 | 2729 | `	break;` |
|        - | 2730 | `				   }` |
|        - | 2731 | `/*` |
|        - | 2732 | ` * LOAD_MAP P1 * *` |
|        - | 2733 | ` *` |
|        - | 2734 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - | 2735 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - | 2736 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - | 2737 | ` */` |
|    49645 | 2738 | `case PH7_OP_LOAD_MAP: {` |
|        - | 2739 | `	VmOpRc rcOp;` |
|    99294 | 2740 | `	sState.pTos = pTos;` |
|    99294 | 2741 | `	sState.pc = pc;` |
|    99294 | 2742 | `	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);` |
|    99294 | 2743 | `	pTos = sState.pTos;` |
|    99294 | 2744 | `	pc = sState.pc;` |
|    99294 | 2745 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2746 | `		goto Abort;` |
|    99294 | 2747 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       15 | 2748 | `		goto Exception;` |
|        - | 2749 | `	}` |
|    99280 | 2750 | `	break;` |
|        - | 2751 | `					  }` |
|        - | 2752 | `/*` |
|        - | 2753 | ` * LOAD_LIST: P1 * *` |
|        - | 2754 | ` *` |
|        - | 2755 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - | 2756 | ` * This is the VM implementation of the list() PHP construct.` |
|        - | 2757 | ` * Caveats:` |
|        - | 2758 | ` *  This implementation support only a single nesting level.` |
|        - | 2759 | ` */` |
|      526 | 2760 | `case PH7_OP_LOAD_LIST: {` |
|        - | 2761 | `	VmOpRc rcOp;` |
|     1057 | 2762 | `	sState.pTos = pTos;` |
|     1057 | 2763 | `	sState.pc = pc;` |
|     1057 | 2764 | `	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);` |
|     1057 | 2765 | `	pTos = sState.pTos;` |
|     1057 | 2766 | `	pc = sState.pc;` |
|     1057 | 2767 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2768 | `		goto Abort;` |
|     1057 | 2769 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2770 | `		goto Exception;` |
|        - | 2771 | `	}` |
|     1057 | 2772 | `	break;` |
|        - | 2773 | `					  }` |
|        - | 2774 | `/*` |
|        - | 2775 | ` * LOAD_IDX: P1 P2 *` |
|        - | 2776 | ` *` |
|        - | 2777 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - | 2778 | ` * from the stack.` |
|        - | 2779 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - | 2780 | ` * instead.` |
|        - | 2781 | ` */` |
|   511543 | 2782 | `case PH7_OP_LOAD_IDX: {` |
|        - | 2783 | `	VmOpRc rcOp;` |
|  1025196 | 2784 | `	sState.pTos = pTos;` |
|  1025196 | 2785 | `	sState.pc = pc;` |
|  1025196 | 2786 | `	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);` |
|  1025196 | 2787 | `	pTos = sState.pTos;` |
|  1025196 | 2788 | `	pc = sState.pc;` |
|  1025196 | 2789 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2790 | `		goto Abort;` |
|  1025196 | 2791 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       59 | 2792 | `		goto Exception;` |
|        - | 2793 | `	}` |
|  1025140 | 2794 | `	break;` |
|        - | 2795 | `					  }` |
|        - | 2796 | `/*` |
|        - | 2797 | ` * LOAD_CLOSURE * * P3` |
|        - | 2798 | ` *` |
|        - | 2799 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - | 2800 | ` * name in the stack.` |
|        - | 2801 | ` */` |
|     4155 | 2802 | `case PH7_OP_LOAD_CLOSURE: {` |
|        - | 2803 | `	VmOpRc rcOp;` |
|     8315 | 2804 | `	sState.pTos = pTos;` |
|     8315 | 2805 | `	sState.pc = pc;` |
|     8315 | 2806 | `	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);` |
|     8315 | 2807 | `	pTos = sState.pTos;` |
|     8315 | 2808 | `	pc = sState.pc;` |
|     8315 | 2809 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2810 | `		goto Abort;` |
|     8315 | 2811 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2812 | `		goto Exception;` |
|        - | 2813 | `	}` |
|     8315 | 2814 | `	break;` |
|        - | 2815 | `					  }` |
|        - | 2816 | `/*` |
|        - | 2817 | ` * LOAD_FCC P1 * *` |
|        - | 2818 | ` *` |
|        - | 2819 | ` * First-class callable: wrap the callee in a Closure object instead of calling it.` |
|        - | 2820 | ` *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS` |
|        - | 2821 | ` *           (from the callee's OP_LOADC). Replace it in place with a Closure whose` |
|        - | 2822 | ` *           $__fn is that name; the existing string-callable dispatch resolves it.` |
|        - | 2823 | ` *           (OOM degrades to leaving the name string on the stack — still callable.)` |
|        - | 2824 | ` *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name` |
|        - | 2825 | ` *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds` |
|        - | 2826 | ` *           $this (scope = its class); a class-name-string target is a static callable` |
|        - | 2827 | ` *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —` |
|        - | 2828 | ` *           the popped target leaves no name string to keep.)` |
|        - | 2829 | ` */` |
|      160 | 2830 | `case PH7_OP_LOAD_FCC:{` |
|      325 | 2831 | `	if( pInstr->iP1 == 1 ){` |
|        - | 2832 | ``		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)`` |
|        - | 2833 | `		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —` |
|        - | 2834 | `		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,` |
|        - | 2835 | ``		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a`` |
|        - | 2836 | `		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is` |
|        - | 2837 | `		 * (graceful degradation), and so is the original on OOM — still whatever it was. */` |
|        - | 2838 | `		ph7_class_instance *pCloObj;` |
|        - | 2839 | `		sxi32 nFccBrc;` |
|        - | 2840 | `		const void *pFccRes;` |
|      143 | 2841 | `		if( VmValueIsClosure(pVm, pTos) ){` |
|       11 | 2842 | `			break;` |
|        - | 2843 | `		}` |
|        - | 2844 | `		/* php's global fallback for an UNQUALIFIED function name written inside a` |
|        - | 2845 | `		 * namespace: the current namespace first, the global one after. The compiler` |
|        - | 2846 | `		 * qualified this name and marks the instruction (iP2==1) when it did, so the` |
|        - | 2847 | `		 * fallback happens here — the OP_CALL path does the same thing from its arg map,` |
|        - | 2848 | ``		 * which an FCC has none of. `strlen(...)` in a namespaced file was`` |
|        - | 2849 | ``		 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|      130 | 2850 | `		if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING)` |
|       13 | 2851 | `			&& !PH7_VmIsCallable(pVm,pTos,TRUE) ){` |
|        7 | 2852 | `			const char *zFccName = (const char *)SyBlobData(&pTos->sBlob);` |
|        7 | 2853 | `			sxu32 nFccName = SyBlobLength(&pTos->sBlob);` |
|        7 | 2854 | `			const char *zFccShort = zFccName;` |
|        - | 2855 | `			sxu32 iFccPos;` |
|      165 | 2856 | `			for( iFccPos = 0 ; iFccPos < nFccName ; ++iFccPos ){` |
|      159 | 2857 | `				if( zFccName[iFccPos] == '\\' ){` |
|        7 | 2858 | `					zFccShort = &zFccName[iFccPos + 1];` |
|        3 | 2859 | `				}` |
|       80 | 2860 | `			}` |
|        7 | 2861 | `			if( zFccShort != zFccName ){` |
|        - | 2862 | `				ph7_value sFccShort;` |
|        7 | 2863 | `				PH7_MemObjInit(pVm,&sFccShort);` |
|       10 | 2864 | `				PH7_MemObjStringAppend(&sFccShort,zFccShort,` |
|        6 | 2865 | `					(sxu32)(nFccName - (sxu32)(zFccShort - zFccName)));` |
|        7 | 2866 | `				if( PH7_VmIsCallable(pVm,&sFccShort,TRUE) ){` |
|        5 | 2867 | `					PH7_MemObjStore(&sFccShort,pTos);` |
|        2 | 2868 | `				}` |
|        7 | 2869 | `				PH7_MemObjRelease(&sFccShort);` |
|        3 | 2870 | `			}` |
|        3 | 2871 | `		}` |
|        - | 2872 | `		/* The array shape's class lookup can run an autoloader that throws; php propagates` |
|        - | 2873 | `		 * THAT exception and never reports the callable bad, exactly as at the OP_CALL sites. */` |
|      133 | 2874 | `		nFccBrc = pVm->nBoundaryRc;` |
|      133 | 2875 | `		pFccRes = (const void *)pVm->pResumeFrame;` |
|      133 | 2876 | `		pCloObj = VmFccWrapValue(pVm, pTos);` |
|      133 | 2877 | `		if( pCloObj ){` |
|      103 | 2878 | `			PH7_MemObjRelease(pTos);` |
|      103 | 2879 | `			pCloObj->iRef++;` |
|      103 | 2880 | `			pTos->x.pOther = pCloObj;` |
|      103 | 2881 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       53 | 2882 | `		}else{` |
|        - | 2883 | `			/* php refuses a non-callable HERE, with the direct dispatch's own wording — the` |
|        - | 2884 | ``			 * `(...)` does not make a bad callable acceptable, it just defers the call. */`` |
|        - | 2885 | `			char zFccMsg[192];` |
|       31 | 2886 | `			const char *zFccBad = VmFccValueError(&(*pVm),pTos,zFccMsg,sizeof(zFccMsg));` |
|       31 | 2887 | `			int bFccRaised = PH7_VmClassLookupRaised(&(*pVm),nFccBrc,pFccRes);` |
|       31 | 2888 | `			if( zFccBad \|\| bFccRaised ){` |
|        - | 2889 | `				sxi32 rcFcc;` |
|       31 | 2890 | `				PH7_MemObjRelease(pTos);` |
|       31 | 2891 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       31 | 2892 | `				pTos->nIdx = SXU32_HIGH;` |
|       31 | 2893 | `				if( bFccRaised ){` |
|      ! 0 | 2894 | `					rcFcc = pVm->nBoundaryRc;` |
|      ! 0 | 2895 | `					pVm->nBoundaryRc = 0;` |
|      ! 0 | 2896 | `					if( rcFcc == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 2897 | `					rc = PH7_EXCEPTION;` |
|       15 | 2898 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2899 | `				}` |
|       31 | 2900 | `				rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccBad,(sxu32)SyStrlen(zFccBad));` |
|       31 | 2901 | `				if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       31 | 2902 | `				rc = rcFcc;` |
|       61 | 2903 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2904 | `			}` |
|        - | 2905 | `		}` |
|       53 | 2906 | `	}else{` |
|        - | 2907 | `		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]` |
|        - | 2908 | `		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its` |
|        - | 2909 | `		 * class); a class-name string target is a static callable (scope = that class). */` |
|      184 | 2910 | `		ph7_value *pTarget = &pTos[-1];` |
|        - | 2911 | `		SyString sName;` |
|        - | 2912 | `		ph7_class_instance *pCloObj;` |
|      184 | 2913 | `		ph7_class *pFccCls = 0;` |
|      184 | 2914 | `		ph7_class_instance *pFccRecv = 0;` |
|      184 | 2915 | `		const char *zFccErr = 0;` |
|        - | 2916 | `		char zFccMsg[192];` |
|      184 | 2917 | `		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));` |
|      184 | 2918 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      108 | 2919 | `			pFccRecv = (ph7_class_instance *)pTarget->x.pOther;` |
|      108 | 2920 | `			pFccCls = pFccRecv->pClass;` |
|      108 | 2921 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pFccCls) ){` |
|        - | 2922 | ``				/* `$inc->m(...)` resolves the method at CREATION, so php's`` |
|        - | 2923 | `				 * incomplete-object call Error is raised here, not at a later` |
|        - | 2924 | `				 * invocation. */` |
|        - | 2925 | `				SyBlob sIncErr;` |
|        - | 2926 | `				sxi32 rcInc;` |
|        3 | 2927 | `				SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|        3 | 2928 | `				PH7_VmIncompleteMsg(&(*pVm),pFccRecv,"call a method",&sIncErr);` |
|        3 | 2929 | `				VmPopOperand(&pTos,1);       /* the method name */` |
|        3 | 2930 | `				PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|        3 | 2931 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 2932 | `				pTos->nIdx = SXU32_HIGH;` |
|        4 | 2933 | `				rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|        1 | 2934 | `					SyBlobLength(&sIncErr));` |
|        3 | 2935 | `				SyBlobRelease(&sIncErr);` |
|        3 | 2936 | `				if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|        3 | 2937 | `				rc = rcInc;` |
|        3 | 2938 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        4 | 2939 | `			}` |
|      131 | 2940 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|        - | 2941 | ``			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class`` |
|        - | 2942 | `			 * now, so the closure binds the concrete scope (matching PHP). */` |
|       80 | 2943 | `			pFccCls = VmFccResolveScope(pVm, pTarget);` |
|       38 | 2944 | `		}` |
|      182 | 2945 | `		if( pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING) ){` |
|        - | 2946 | `			/* php resolves the member HERE, through the same lookup the call would use:` |
|        - | 2947 | `			 * every refusal a call would raise is raised at CREATION, and a non-static` |
|        - | 2948 | `			 * method named through a class binds the calling frame's own $this. */` |
|      182 | 2949 | `			ph7_class_instance *pRecvOut = 0;` |
|      271 | 2950 | `			zFccErr = VmFccMemberError(&(*pVm),pFccCls,` |
|      178 | 2951 | `				(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|       89 | 2952 | `				SyStringData(&sName),SyStringLength(&sName),` |
|      178 | 2953 | `				(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|       89 | 2954 | `				&pRecvOut,zFccMsg,sizeof(zFccMsg));` |
|      182 | 2955 | `			if( pRecvOut ){` |
|       13 | 2956 | ``				pFccRecv = pRecvOut; /* the receiver php binds into a `C::m(...)` callable */`` |
|        6 | 2957 | `			}` |
|       89 | 2958 | `		}` |
|      182 | 2959 | `		if( zFccErr ){` |
|        - | 2960 | `			sxi32 rcFcc;` |
|       24 | 2961 | `			VmPopOperand(&pTos,1);       /* the method name */` |
|       24 | 2962 | `			PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|       24 | 2963 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       24 | 2964 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 | 2965 | `			rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccErr,(sxu32)SyStrlen(zFccErr));` |
|       24 | 2966 | `			if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       24 | 2967 | `			rc = rcFcc;` |
|       30 | 2968 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2969 | `		}` |
|      160 | 2970 | `		if( pFccCls == 0 ){` |
|      ! 0 | 2971 | `			pCloObj = 0;` |
|      160 | 2972 | `		}else if( pFccRecv ){` |
|      108 | 2973 | `			pCloObj = VmCreateClosure(pVm, &sName, pFccRecv, &pFccRecv->pClass->sName);` |
|       56 | 2974 | `		}else{` |
|       56 | 2975 | `			pCloObj = VmCreateClosure(pVm, &sName, 0, &pFccCls->sName);` |
|        - | 2976 | `		}` |
|      160 | 2977 | `		if( pCloObj ){` |
|        - | 2978 | ``			/* `$o->m(...)` / `C::m(...)` names a METHOD, whatever the class turns out to`` |
|        - | 2979 | `			 * declare: the unwrap must not go looking for a FUNCTION of that name, and a` |
|        - | 2980 | `			 * name the class answers only through __call is still a method call. */` |
|      160 | 2981 | `			pCloObj->iFlags \|= VM_INSTANCE_FCC_METHOD;` |
|        - | 2982 | `			/* The screen above already ran, HERE, where php runs it — so record that this` |
|        - | 2983 | `			 * closure's callee is settled and the invocation must not re-decide it. A name` |
|        - | 2984 | `			 * that resolved to the catch-all instead keeps routing there. */` |
|      160 | 2985 | `			if( PH7_VmFccMethodIsDirect(&(*pVm),pFccCls,SyStringData(&sName),SyStringLength(&sName)) ){` |
|      146 | 2986 | `				pCloObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       71 | 2987 | `			}` |
|       78 | 2988 | `		}` |
|        - | 2989 | `		/* Pop the method name and the target, push the Closure. */` |
|      160 | 2990 | `		PH7_MemObjRelease(pTos);` |
|      160 | 2991 | `		pTos--;` |
|      160 | 2992 | `		PH7_MemObjRelease(pTos);` |
|      160 | 2993 | `		if( pCloObj ){` |
|      160 | 2994 | `			pCloObj->iRef++;` |
|      160 | 2995 | `			pTos->x.pOther = pCloObj;` |
|      160 | 2996 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       82 | 2997 | `		}else{` |
|      ! 0 | 2998 | `			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */` |
|        - | 2999 | `		}` |
|        - | 3000 | `	}` |
|      261 | 3001 | `	break;` |
|        - | 3002 | `					 }` |
|        - | 3003 | `/*` |
|        - | 3004 | ` * STORE * P2 P3` |
|        - | 3005 | ` *` |
|        - | 3006 | ` * Perform a store (Assignment) operation.` |
|        - | 3007 | ` */` |
|   495274 | 3008 | `case PH7_OP_STORE: {` |
|        - | 3009 | `	ph7_value *pObj;` |
|        - | 3010 | `	SyString sName;` |
|        - | 3011 | `#ifdef UNTRUST` |
|        - | 3012 | `	if( pTos < pStack ){` |
|        - | 3013 | `		goto Abort;` |
|        - | 3014 | `	}` |
|        - | 3015 | `#endif` |
|   992718 | 3016 | `	if( pInstr->iP2 ){` |
|        - | 3017 | `		sxu32 nIdx;` |
|        - | 3018 | `		sxi32 rcT;` |
|        - | 3019 | `		/* Member store operation */` |
|   101939 | 3020 | `		nIdx = pTos->nIdx;` |
|   101939 | 3021 | `		VmPopOperand(&pTos,1);` |
|   101939 | 3022 | `		if( pVm->pMagicSetThis ){` |
|        - | 3023 | `			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain` |
|        - | 3024 | `			 * store to a missing/inaccessible property on a class declaring __set.` |
|        - | 3025 | `			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on` |
|        - | 3026 | `			 * the stack as the assignment expression's result — php's semantics` |
|        - | 3027 | `			 * (no property is created; a throw rides the boundary rail). */` |
|       31 | 3028 | `			ph7_class_instance *pSetThis = pVm->pMagicSetThis;` |
|        - | 3029 | `			SyString sSetName;` |
|       31 | 3030 | `			pVm->pMagicSetThis = 0;` |
|       31 | 3031 | `			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));` |
|       31 | 3032 | `			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);` |
|       31 | 3033 | `			PH7_ClassInstanceUnref(pSetThis);` |
|       31 | 3034 | `			SyBlobReset(&pVm->sMagicSetName);` |
|       31 | 3035 | `			break;` |
|        - | 3036 | `		}` |
|   101911 | 3037 | `		if( pVm->pHookSetThis ){` |
|        - | 3038 | `			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a` |
|        - | 3039 | `			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with` |
|        - | 3040 | `			 * the rvalue (which stays on the stack as the assignment expression's` |
|        - | 3041 | ``			 * result); a `set => expr` hook's return value is stored into the`` |
|        - | 3042 | `			 * BACKING slot with the ordinary typed enforcement. A property with` |
|        - | 3043 | `			 * only a get hook is php's catchable "is read-only" Error. */` |
|       56 | 3044 | `			ph7_class_instance *pHThis = pVm->pHookSetThis;` |
|       56 | 3045 | `			ph7_class_attr *pHAttr = pVm->pHookSetAttr;` |
|       56 | 3046 | `			sxu32 nBackIdx = pVm->nHookSetIdx;` |
|        - | 3047 | `			sxi32 rcHs;` |
|       56 | 3048 | `			pVm->pHookSetThis = 0;` |
|       56 | 3049 | `			pVm->pHookSetAttr = 0;` |
|       56 | 3050 | `			pVm->nHookSetIdx = SXU32_HIGH;` |
|       56 | 3051 | `			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);` |
|       56 | 3052 | `			PH7_ClassInstanceUnref(pHThis);` |
|       56 | 3053 | `			if( rcHs == PH7_ABORT ){` |
|      ! 0 | 3054 | `				goto Abort;` |
|        - | 3055 | `			}` |
|       56 | 3056 | `			break;` |
|        - | 3057 | `		}` |
|   101859 | 3058 | `		if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 3059 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3060 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|      ! 0 | 3061 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 3062 | `		}else{` |
|        - | 3063 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - | 3064 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|   101859 | 3065 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);` |
|   101859 | 3066 | `			if( rcT == PH7_ABORT ){` |
|       13 | 3067 | `				goto Abort;` |
|        - | 3068 | `			}` |
|   101849 | 3069 | `			if( rcT == PH7_EXCEPTION ){` |
|        - | 3070 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - | 3071 | `				 * control to the nearest catch block if any (draining any` |
|        - | 3072 | `				 * abandoned outer-expression operands to the try's base),` |
|        - | 3073 | `				 * otherwise propagate out of the VM loop. */` |
|   100167 | 3074 | `				VmPopOperand(&pTos,1);` |
|        - | 3075 | `				{` |
|        - | 3076 | `					sxi32 iRp;` |
|   100167 | 3077 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|   400093 | 3078 | `						PH7_RESUME_DRAIN()` |
|   100091 | 3079 | `						pc = iRp;` |
|   100091 | 3080 | `						break;` |
|        - | 3081 | `					}` |
|        - | 3082 | `				}` |
|       81 | 3083 | `				goto Exception;` |
|        - | 3084 | `			}` |
|        - | 3085 | `			/* Point to the desired memory object */` |
|     1687 | 3086 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1687 | 3087 | `			if( pObj ){` |
|        - | 3088 | `				/* Perform the store operation */` |
|     1687 | 3089 | `				PH7_MemObjStore(pTos,pObj);` |
|      841 | 3090 | `			}` |
|        - | 3091 | `		}` |
|     1687 | 3092 | `		break;` |
|   890784 | 3093 | `	}else if( pInstr->p3 == 0 ){` |
|        - | 3094 | `		/* Take the variable name from the next on the stack (user-visible: a` |
|        - | 3095 | `		 * variable-variable NAME $$arr warns on an array, §2) */` |
|        - | 3096 | `		{` |
|       27 | 3097 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       27 | 3098 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3099 | `		}` |
|       24 | 3100 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       24 | 3101 | `		pTos--;` |
|        - | 3102 | `#ifdef UNTRUST` |
|        - | 3103 | `		if( pTos < pStack  ){` |
|        - | 3104 | `			goto Abort;` |
|        - | 3105 | `		}` |
|        - | 3106 | `#endif` |
|       13 | 3107 | `	}else{` |
|   890760 | 3108 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 3109 | `	}` |
|   890777 | 3110 | `	if( sName.nByte == sizeof("GLOBALS")-1` |
|   448080 | 3111 | `	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){` |
|        6 | 3112 | `		if( pInstr->p3 ){` |
|        - | 3113 | `			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time` |
|        - | 3114 | `			 * fatal there; raised at the store site here with the same` |
|        - | 3115 | `			 * message and the same non-catchable outcome). Element writes` |
|        - | 3116 | `			 * are unaffected. */` |
|        3 | 3117 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3118 | `				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 3119 | `			pVm->iExitStatus = 255;` |
|        3 | 3120 | `			pVm->bHaltRequested = 1;` |
|        3 | 3121 | `			goto Abort;` |
|        - | 3122 | `		}` |
|        - | 3123 | `		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's` |
|        - | 3124 | `		 * compile-time case: php quietly creates an ordinary symbol-table` |
|        - | 3125 | `		 * entry named GLOBALS, leaving the auto-global view intact. */` |
|        3 | 3126 | `		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);` |
|        3 | 3127 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 3128 | `		break;` |
|        - | 3129 | `	}` |
|        - | 3130 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   890778 | 3131 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   890778 | 3132 | `	if( pObj == 0 ){` |
|      ! 0 | 3133 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 3134 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 3135 | `		goto Abort;` |
|        - | 3136 | `	}` |
|   890778 | 3137 | `	if( !pInstr->p3 ){` |
|       22 | 3138 | `		PH7_MemObjRelease(&pTos[1]);` |
|       10 | 3139 | `	}` |
|        - | 3140 | `	/* Perform the store operation */` |
|   890778 | 3141 | `	PH7_MemObjStore(pTos,pObj);` |
|   890778 | 3142 | `	break;` |
|        - | 3143 | `				   }` |
|        - | 3144 | `/*` |
|        - | 3145 | ` * STORE_IDX:   P1 * P3` |
|        - | 3146 | ` * STORE_IDX_R: P1 * P3` |
|        - | 3147 | ` *` |
|        - | 3148 | ` * Perfrom a store operation an a hashmap entry.` |
|        - | 3149 | ` */` |
|   178638 | 3150 | `case PH7_OP_STORE_IDX:` |
|        - | 3151 | `case PH7_OP_STORE_IDX_REF: {` |
|        - | 3152 | `	VmOpRc rcOp;` |
|   357273 | 3153 | `	sState.pTos = pTos;` |
|   357273 | 3154 | `	sState.pc = pc;` |
|   357273 | 3155 | `	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);` |
|   357273 | 3156 | `	pTos = sState.pTos;` |
|   357273 | 3157 | `	pc = sState.pc;` |
|   357273 | 3158 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3159 | `		goto Abort;` |
|   357271 | 3160 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       37 | 3161 | `		goto Exception;` |
|        - | 3162 | `	}` |
|   357239 | 3163 | `	break;` |
|        - | 3164 | `					  }` |
|        - | 3165 | `/*` |
|        - | 3166 | ` * INCR: P1 * *` |
|        - | 3167 | ` *` |
|        - | 3168 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - | 3169 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - | 3170 | ` * the stack and increment after that.` |
|        - | 3171 | ` */` |
|   347968 | 3172 | `case PH7_OP_INCR:` |
|        - | 3173 | `#ifdef UNTRUST` |
|        - | 3174 | `	if( pTos < pStack ){` |
|        - | 3175 | `		goto Abort;` |
|        - | 3176 | `	}` |
|        - | 3177 | `#endif` |
|        - | 3178 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3179 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3180 | `	 * — which otherwise skips object/array/resource operands. */` |
|   696809 | 3181 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3182 | `	/* php refuses to increment a string OFFSET, whatever byte it holds. */` |
|   696799 | 3183 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3184 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3185 | `	 * php's TypeError, raised BEFORE any set dispatch (the type guard below` |
|        - | 3186 | `	 * would skip the mutation and the tail write-back would otherwise call` |
|        - | 3187 | `	 * the set hook with the unchanged value). */` |
|   696780 | 3188 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|   348838 | 3189 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        5 | 3190 | `		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        5 | 3191 | `		if( VM_HOOK_PEND_IS_RMW(pTopInc->iKind) && pTopInc->nScratchIdx == pTos->nIdx ){` |
|        - | 3192 | `			SyBlob sErrMsg;` |
|        5 | 3193 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 | 3194 | `			VmIncDecTypeErrorMsg(pTos,TRUE,&sErrMsg);` |
|        5 | 3195 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        5 | 3196 | `			VmHookRmwDropTop(&(*pVm));` |
|        5 | 3197 | `			pTos->nIdx = SXU32_HIGH;` |
|        5 | 3198 | `			break;` |
|        - | 3199 | `		}` |
|      ! 0 | 3200 | `	}` |
|   696781 | 3201 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3202 | `		/* php's ++ operand contract: an array, object or resource is a catchable` |
|        - | 3203 | `		 * TypeError, not the silent no-op this used to be. Settle the operand` |
|        - | 3204 | `		 * (the op's result slot) BEFORE throwing, like the arithmetic sites. */` |
|        - | 3205 | `		SyBlob sIncMsg;` |
|        - | 3206 | `		sxi32 rcInc;` |
|       21 | 3207 | `		SyBlobInit(&sIncMsg,&pVm->sAllocator);` |
|       21 | 3208 | `		VmIncDecTypeErrorMsg(pTos,TRUE,&sIncMsg);` |
|       21 | 3209 | `		PH7_MemObjRelease(pTos);` |
|       21 | 3210 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       21 | 3211 | `		pTos->nIdx = SXU32_HIGH;` |
|       31 | 3212 | `		rcInc = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sIncMsg),` |
|       10 | 3213 | `			SyBlobLength(&sIncMsg));` |
|       21 | 3214 | `		SyBlobRelease(&sIncMsg);` |
|       21 | 3215 | `		if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|       21 | 3216 | `		rc = rcInc;` |
|       23 | 3217 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3218 | `	}` |
|        - | 3219 | ``	/* php 8.3+: `++` on a BOOL is a no-op it warns about (E_WARNING, errno 2 —`` |
|        - | 3220 | ``	 * same shape as the null `--` below), where PH7 coerced true to int 2. */`` |
|   696761 | 3221 | `	if( pTos->iFlags & MEMOBJ_BOOL ){` |
|        7 | 3222 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3223 | `			"Increment on type bool has no effect, this will change in the next major version of PHP");` |
|        3 | 3224 | `	}` |
|   696761 | 3225 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_BOOL)) == 0 ){` |
|   696755 | 3226 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3227 | `			ph7_value *pObj;` |
|   696749 | 3228 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   696749 | 3229 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3230 | `					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric` |
|        - | 3231 | `					 * string (it points at str_increment() instead); PHL rejects it. */` |
|        - | 3232 | `					SyBlob sErrMsg;` |
|        3 | 3233 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3234 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3235 | `						"Increment on a non-numeric string is not supported, use str_increment() instead",` |
|        - | 3236 | `						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);` |
|        3 | 3237 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3238 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3239 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3240 | `					break;` |
|      ! 0 | 3241 | `				}else{` |
|        - | 3242 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - | 3243 | `					 * original value: pTos may alias pObj's blob via` |
|        - | 3244 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - | 3245 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - | 3246 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - | 3247 | `					 * so its old-value view survives the coercion. */` |
|   696747 | 3248 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 | 3249 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        5 | 3250 | `					}` |
|        - | 3251 | `					/* Force a numeric cast on the variable */` |
|   696747 | 3252 | `					PH7_MemObjToNumeric(pObj);` |
|   696747 | 3253 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        5 | 3254 | `						pObj->rVal++;` |
|        - | 3255 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3256 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3257 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3258 | `						 * integer-valued real. */` |
|        5 | 3259 | `						PH7_MemObjTryInteger(pObj);` |
|        3 | 3260 | `					}else{` |
|        - | 3261 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3262 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3263 | `						sxi64 r;` |
|   696743 | 3264 | `						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3265 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        5 | 3266 | `							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;` |
|        5 | 3267 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3268 | `#else` |
|        - | 3269 | `							pObj->x.iVal = r;` |
|        - | 3270 | `#endif` |
|        3 | 3271 | `						}else{` |
|   696739 | 3272 | `							pObj->x.iVal = r;` |
|        - | 3273 | `						}` |
|        - | 3274 | `					}` |
|   696747 | 3275 | `					if( pInstr->iP1 ){` |
|        - | 3276 | `						/* Pre-increment: result is the new value. */` |
|       69 | 3277 | `						PH7_MemObjStore(pObj,pTos);` |
|       34 | 3278 | `					}` |
|        - | 3279 | `					/* Post-increment: pTos retains the old value (a string` |
|        - | 3280 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - | 3281 | `				}` |
|   348802 | 3282 | `			}` |
|   348807 | 3283 | `		}else{` |
|        7 | 3284 | `			if( pInstr->iP1 ){` |
|      ! 0 | 3285 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 | 3286 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 | 3287 | `				}else{` |
|        - | 3288 | `					/* Force a numeric cast */` |
|      ! 0 | 3289 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3290 | `					/* Pre-increment */` |
|      ! 0 | 3291 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3292 | `						pTos->rVal++;` |
|        - | 3293 | `						/* Try to get an integer representation */` |
|      ! 0 | 3294 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3295 | `					}else{` |
|        - | 3296 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3297 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3298 | `						sxi64 r;` |
|      ! 0 | 3299 | `						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3300 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3301 | `							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;` |
|      ! 0 | 3302 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3303 | `#else` |
|        - | 3304 | `							pTos->x.iVal = r;` |
|        - | 3305 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3306 | `#endif` |
|      ! 0 | 3307 | `						}else{` |
|      ! 0 | 3308 | `							pTos->x.iVal = r;` |
|      ! 0 | 3309 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3310 | `						}` |
|        - | 3311 | `					}` |
|        - | 3312 | `				}` |
|      ! 0 | 3313 | `			}` |
|        - | 3314 | `		}` |
|   348805 | 3315 | `	}` |
|   696759 | 3316 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|   696759 | 3317 | `	break;` |
|        - | 3318 | `/*` |
|        - | 3319 | ` * DECR: P1 * *` |
|        - | 3320 | ` *` |
|        - | 3321 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - | 3322 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - | 3323 | ` * and decrement after that.` |
|        - | 3324 | ` */` |
|       72 | 3325 | `case PH7_OP_DECR:` |
|        - | 3326 | `#ifdef UNTRUST` |
|        - | 3327 | `	if( pTos < pStack ){` |
|        - | 3328 | `		goto Abort;` |
|        - | 3329 | `	}` |
|        - | 3330 | `#endif` |
|        - | 3331 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3332 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3333 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|        - | 3334 | `	 * property currently holding null). */` |
|      152 | 3335 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3336 | `	/* php refuses to decrement a string OFFSET, whatever byte it holds. */` |
|      141 | 3337 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3338 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3339 | `	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —` |
|        - | 3340 | ``	 * `--` on null is php's no-op and its write-back still dispatches set). */`` |
|      134 | 3341 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|       76 | 3342 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 3343 | `		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 3344 | `		if( VM_HOOK_PEND_IS_RMW(pTopDec->iKind) && pTopDec->nScratchIdx == pTos->nIdx ){` |
|        - | 3345 | `			SyBlob sErrMsg;` |
|        3 | 3346 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3347 | `			VmIncDecTypeErrorMsg(pTos,FALSE,&sErrMsg);` |
|        3 | 3348 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3349 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 3350 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 3351 | `			break;` |
|        - | 3352 | `		}` |
|      ! 0 | 3353 | `	}` |
|      135 | 3354 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3355 | ``		/* php's `--` operand contract, the mirror of INCR's: an array, object or`` |
|        - | 3356 | `		 * resource is a catchable TypeError, not a silent no-op. */` |
|        - | 3357 | `		SyBlob sDecMsg;` |
|        - | 3358 | `		sxi32 rcDec;` |
|       11 | 3359 | `		SyBlobInit(&sDecMsg,&pVm->sAllocator);` |
|       11 | 3360 | `		VmIncDecTypeErrorMsg(pTos,FALSE,&sDecMsg);` |
|       11 | 3361 | `		PH7_MemObjRelease(pTos);` |
|       11 | 3362 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 3363 | `		pTos->nIdx = SXU32_HIGH;` |
|       16 | 3364 | `		rcDec = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sDecMsg),` |
|        5 | 3365 | `			SyBlobLength(&sDecMsg));` |
|       11 | 3366 | `		SyBlobRelease(&sDecMsg);` |
|       11 | 3367 | `		if( rcDec == SXERR_ABORT ){ goto Abort; }` |
|       11 | 3368 | `		rc = rcDec;` |
|       13 | 3369 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3370 | `	}` |
|        - | 3371 | ``	/* NULL and BOOL stay excluded: PHP leaves `--` on either untouched (no-op) --`` |
|        - | 3372 | `	 * but 8.3 warns about both no-ops, same as the non-numeric-string one below. */` |
|      125 | 3373 | `	if( pTos->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|        - | 3374 | `		/* E_WARNING, not E_DEPRECATED -- php reports these at errno 2. */` |
|       16 | 3375 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3376 | `			"Decrement on type %s has no effect, this will change in the next major version of PHP",` |
|       10 | 3377 | `			(pTos->iFlags & MEMOBJ_NULL) ? "null" : "bool");` |
|        5 | 3378 | `	}` |
|      125 | 3379 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      115 | 3380 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3381 | `			ph7_value *pObj;` |
|      115 | 3382 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      115 | 3383 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3384 | ``					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string`` |
|        - | 3385 | `					 * (php has no string decrement); PHL rejects it. */` |
|        - | 3386 | `					SyBlob sErrMsg;` |
|        3 | 3387 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3388 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3389 | `						"Decrement on a non-numeric string is not supported",` |
|        - | 3390 | `						sizeof("Decrement on a non-numeric string is not supported")-1);` |
|        3 | 3391 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3392 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3393 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3394 | `					break;` |
|      ! 0 | 3395 | `				}else{` |
|        - | 3396 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - | 3397 | `					 * post-decrement must preserve pTos's original value, which` |
|        - | 3398 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - | 3399 | `					 * Force pTos to own its blob before coercing pObj. */` |
|      112 | 3400 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 | 3401 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 | 3402 | `					}` |
|      112 | 3403 | `					PH7_MemObjToNumeric(pObj);` |
|      112 | 3404 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|       11 | 3405 | `						pObj->rVal--;` |
|        - | 3406 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3407 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3408 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3409 | `						 * integer-valued real. */` |
|       11 | 3410 | `						PH7_MemObjTryInteger(pObj);` |
|        6 | 3411 | `					}else{` |
|        - | 3412 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3413 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3414 | `						sxi64 r;` |
|      102 | 3415 | `						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3416 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        3 | 3417 | `							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;` |
|        3 | 3418 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3419 | `#else` |
|        - | 3420 | `							pObj->x.iVal = r;` |
|        - | 3421 | `#endif` |
|        2 | 3422 | `						}else{` |
|      100 | 3423 | `							pObj->x.iVal = r;` |
|        - | 3424 | `						}` |
|        - | 3425 | `					}` |
|      112 | 3426 | `					if( pInstr->iP1 ){` |
|        - | 3427 | `						/* Pre-decrement: result is the new value. */` |
|        3 | 3428 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 | 3429 | `					}` |
|        - | 3430 | `					/* Post-decrement: pTos retains the old value. */` |
|        - | 3431 | `				}` |
|       55 | 3432 | `			}` |
|       57 | 3433 | `		}else{` |
|      ! 0 | 3434 | `			if( pInstr->iP1 ){` |
|      ! 0 | 3435 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - | 3436 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 | 3437 | `				}else{` |
|        - | 3438 | `					/* Force a numeric cast */` |
|      ! 0 | 3439 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3440 | `					/* Pre-decrement */` |
|      ! 0 | 3441 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3442 | `						pTos->rVal--;` |
|        - | 3443 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 | 3444 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3445 | `					}else{` |
|        - | 3446 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3447 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3448 | `						sxi64 r;` |
|      ! 0 | 3449 | `						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3450 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3451 | `							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;` |
|      ! 0 | 3452 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3453 | `#else` |
|        - | 3454 | `							pTos->x.iVal = r;` |
|        - | 3455 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3456 | `#endif` |
|      ! 0 | 3457 | `						}else{` |
|      ! 0 | 3458 | `							pTos->x.iVal = r;` |
|      ! 0 | 3459 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3460 | `						}` |
|        - | 3461 | `					}` |
|        - | 3462 | `				}` |
|      ! 0 | 3463 | `			}` |
|        - | 3464 | `		}` |
|       55 | 3465 | `	}` |
|      122 | 3466 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|      122 | 3467 | `	break;` |
|        - | 3468 | `/*` |
|        - | 3469 | ` * UMINUS: * * *` |
|        - | 3470 | ` *` |
|        - | 3471 | ` * Perform a unary minus operation.` |
|        - | 3472 | ` */` |
|    42873 | 3473 | `case PH7_OP_UMINUS:` |
|        - | 3474 | `#ifdef UNTRUST` |
|        - | 3475 | `	if( pTos < pStack ){` |
|        - | 3476 | `		goto Abort;` |
|        - | 3477 | `	}` |
|        - | 3478 | `#endif` |
|        - | 3479 | ``	/* php's `$x * -1` operand contract: an array, object, resource or`` |
|        - | 3480 | `	 * non-numeric string is a TypeError, not a warned int(-1). */` |
|    85753 | 3481 | `	PH7_UNARY_ARITH_CONTRACT()` |
|        - | 3482 | `	/* Force a numeric (integer,real or both) cast */` |
|    85725 | 3483 | `	PH7_MemObjToNumeric(pTos);` |
|    85725 | 3484 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      227 | 3485 | `		pTos->rVal = -pTos->rVal;` |
|      111 | 3486 | `	}` |
|    85725 | 3487 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    85547 | 3488 | `		if( pTos->x.iVal == SMALLEST_INT64 ){` |
|        - | 3489 | `			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a` |
|        - | 3490 | `			 * REAL representation is already present it is the negated` |
|        - | 3491 | `			 * authoritative value, so just drop the now-stale cached int. The` |
|        - | 3492 | `			 * integer-only build has no float type, so it wraps (two's` |
|        - | 3493 | `			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */` |
|        - | 3494 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        7 | 3495 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 | 3496 | `				pTos->rVal = -(ph7_real)pTos->x.iVal;` |
|        7 | 3497 | `				MemObjSetType(pTos,MEMOBJ_REAL);` |
|        4 | 3498 | `			}else{` |
|      ! 0 | 3499 | `				pTos->iFlags &= ~MEMOBJ_INT;` |
|        - | 3500 | `			}` |
|        - | 3501 | `#else` |
|        - | 3502 | `			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);` |
|        - | 3503 | `#endif` |
|        4 | 3504 | `		}else{` |
|    85541 | 3505 | `			pTos->x.iVal = -pTos->x.iVal;` |
|        - | 3506 | `		}` |
|    42771 | 3507 | `	}` |
|    85725 | 3508 | `	break;` |
|        - | 3509 | `/*` |
|        - | 3510 | ` * UPLUS: * * *` |
|        - | 3511 | ` *` |
|        - | 3512 | ` * Perform a unary plus operation.` |
|        - | 3513 | ` */` |
|       22 | 3514 | `case PH7_OP_UPLUS:` |
|        - | 3515 | `#ifdef UNTRUST` |
|        - | 3516 | `	if( pTos < pStack ){` |
|        - | 3517 | `		goto Abort;` |
|        - | 3518 | `	}` |
|        - | 3519 | `#endif` |
|        - | 3520 | ``	/* Unary plus is php's `$x * 1`, so it answers the same TypeError as unary`` |
|        - | 3521 | ``	 * minus -- naming `int` as the other operand, exactly as php does. */`` |
|       45 | 3522 | `	PH7_UNARY_ARITH_CONTRACT()` |
|        - | 3523 | `	/* Force a numeric (integer,real or both) cast */` |
|       39 | 3524 | `	PH7_MemObjToNumeric(pTos);` |
|       39 | 3525 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3526 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 | 3527 | `	}` |
|       39 | 3528 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       39 | 3529 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       19 | 3530 | `	}` |
|       39 | 3531 | `	break;` |
|        - | 3532 | `/*` |
|        - | 3533 | ` * OP_LNOT: * * *` |
|        - | 3534 | ` *` |
|        - | 3535 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - | 3536 | ` * with its complement.` |
|        - | 3537 | ` */` |
|    33474 | 3538 | `case PH7_OP_LNOT:` |
|        - | 3539 | `#ifdef UNTRUST` |
|        - | 3540 | `	if( pTos < pStack ){` |
|        - | 3541 | `		goto Abort;` |
|        - | 3542 | `	}` |
|        - | 3543 | `#endif` |
|        - | 3544 | `	/* Force a boolean cast */` |
|    66948 | 3545 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      183 | 3546 | `		PH7_MemObjToBool(pTos);` |
|       89 | 3547 | `	}` |
|    66948 | 3548 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    66948 | 3549 | `	break;` |
|        - | 3550 | `/*` |
|        - | 3551 | ` * OP_BITNOT: * * *` |
|        - | 3552 | ` *` |
|        - | 3553 | ` * Interpret the top of the stack as an value.Replace it` |
|        - | 3554 | ` * with its ones-complement.` |
|        - | 3555 | ` */` |
|       85 | 3556 | `case PH7_OP_BITNOT:` |
|        - | 3557 | `#ifdef UNTRUST` |
|        - | 3558 | `	if( pTos < pStack ){` |
|        - | 3559 | `		goto Abort;` |
|        - | 3560 | `	}` |
|        - | 3561 | `#endif` |
|      175 | 3562 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - | 3563 | ``		/* php's `~` over a STRING is a per-BYTE ones-complement of the same`` |
|        - | 3564 | ``		 * length — a binary string, not an integer operation, so `~"abc"` is`` |
|        - | 3565 | `		 * "\x9e\x9d\x9c" where PHL cast the string to an int and answered` |
|        - | 3566 | `		 * int(-1). The operand's blob can be a READ-ONLY VIEW of the variable's` |
|        - | 3567 | `		 * own buffer (PH7_MemObjLoad), so complement into a fresh blob and swap` |
|        - | 3568 | `		 * it in rather than writing through the view. */` |
|        - | 3569 | `		SyBlob sNotBuf;` |
|       17 | 3570 | `		const unsigned char *zNotIn = (const unsigned char *)SyBlobData(&pTos->sBlob);` |
|       17 | 3571 | `		sxu32 nNotIn = SyBlobLength(&pTos->sBlob), iNot;` |
|       17 | 3572 | `		SyBlobInit(&sNotBuf,&pVm->sAllocator);` |
|       53 | 3573 | `		for( iNot = 0 ; iNot < nNotIn ; ++iNot ){` |
|       37 | 3574 | `			unsigned char cNot = (unsigned char)~zNotIn[iNot];` |
|       37 | 3575 | `			SyBlobAppend(&sNotBuf,(const void *)&cNot,sizeof(char));` |
|       19 | 3576 | `		}` |
|       17 | 3577 | `		PH7_MemObjRelease(pTos);` |
|       17 | 3578 | `		MemObjSetType(pTos,MEMOBJ_STRING);` |
|       17 | 3579 | `		if( SyBlobLength(&sNotBuf) > 0 ){` |
|       15 | 3580 | `			SyBlobAppend(&pTos->sBlob,SyBlobData(&sNotBuf),SyBlobLength(&sNotBuf));` |
|        7 | 3581 | `		}` |
|       17 | 3582 | `		SyBlobRelease(&sNotBuf);` |
|       17 | 3583 | `		break;` |
|        - | 3584 | `	}` |
|      159 | 3585 | `	if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        - | 3586 | `		/* Everything else — null, a bool, an array, an object, a resource — has` |
|        - | 3587 | `		 * no bitwise-not in php at all: a catchable TypeError naming the operand,` |
|        - | 3588 | `		 * where PHL cast it to an int and answered ~0 / ~1. */` |
|        - | 3589 | `		SyBlob sNotMsg;` |
|        - | 3590 | `		sxi32 rcNot;` |
|       25 | 3591 | `		SyBlobInit(&sNotMsg,&pVm->sAllocator);` |
|       25 | 3592 | `		VmBitNotTypeErrorMsg(pTos,&sNotMsg);` |
|       25 | 3593 | `		PH7_MemObjRelease(pTos);` |
|       25 | 3594 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       25 | 3595 | `		pTos->nIdx = SXU32_HIGH;` |
|       37 | 3596 | `		rcNot = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sNotMsg),` |
|       12 | 3597 | `			SyBlobLength(&sNotMsg));` |
|       25 | 3598 | `		SyBlobRelease(&sNotMsg);` |
|       25 | 3599 | `		if( rcNot == SXERR_ABORT ){ goto Abort; }` |
|       25 | 3600 | `		rc = rcNot;` |
|       27 | 3601 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3602 | `	}` |
|        - | 3603 | `	/* Force an integer cast (php deprecates a lossy float here too) */` |
|      135 | 3604 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      135 | 3605 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      135 | 3606 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 3607 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 3608 | `	}` |
|      135 | 3609 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        - | 3610 | `	/* An INTEGRAL float carries a cached MEMOBJ_INT beside MEMOBJ_REAL, so the` |
|        - | 3611 | `` 	 * cast above is skipped and the value kept rendering as a float: `~2.0` `` |
|        - | 3612 | ``	 * answered 2.0 instead of -3. `~` always yields an int. */`` |
|      135 | 3613 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      135 | 3614 | `	break;` |
|        - | 3615 | `/* OP_MUL * * *` |
|        - | 3616 | ` * OP_MUL_STORE * * *` |
|        - | 3617 | ` *` |
|        - | 3618 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - | 3619 | ` * and push the result back onto the stack.` |
|        - | 3620 | ` */` |
|     1525 | 3621 | `case PH7_OP_MUL:` |
|        - | 3622 | `case PH7_OP_MUL_STORE: {` |
|        - | 3623 | `	VmOpRc rcOp;` |
|        - | 3624 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     3055 | 3625 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     3053 | 3626 | `	sState.pTos = pTos;` |
|     3053 | 3627 | `	sState.pc = pc;` |
|     3053 | 3628 | `	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);` |
|     3053 | 3629 | `	pTos = sState.pTos;` |
|     3053 | 3630 | `	pc = sState.pc;` |
|     3053 | 3631 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3632 | `		goto Abort;` |
|     3053 | 3633 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3634 | `		goto Exception;` |
|        - | 3635 | `	}` |
|     3053 | 3636 | `	break;` |
|        - | 3637 | `					  }` |
|        - | 3638 | `/* OP_POW * * *` |
|        - | 3639 | ` * OP_POW_STORE * * *` |
|        - | 3640 | ` *` |
|        - | 3641 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - | 3642 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - | 3643 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - | 3644 | ` * fits in sxi64; otherwise the result is a double.` |
|        - | 3645 | ` */` |
|       94 | 3646 | `case PH7_OP_POW:` |
|        - | 3647 | `case PH7_OP_POW_STORE: {` |
|        - | 3648 | `	VmOpRc rcOp;` |
|        - | 3649 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      189 | 3650 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      187 | 3651 | `	sState.pTos = pTos;` |
|      187 | 3652 | `	sState.pc = pc;` |
|      187 | 3653 | `	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);` |
|      187 | 3654 | `	pTos = sState.pTos;` |
|      187 | 3655 | `	pc = sState.pc;` |
|      187 | 3656 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3657 | `		goto Abort;` |
|      187 | 3658 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 3659 | `		goto Exception;` |
|        - | 3660 | `	}` |
|      183 | 3661 | `	break;` |
|        - | 3662 | `					  }` |
|        - | 3663 | `/* OP_ADD * * *` |
|        - | 3664 | ` *` |
|        - | 3665 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 3666 | ` * and push the result back onto the stack.` |
|        - | 3667 | ` */` |
|    10179 | 3668 | `case PH7_OP_ADD:{` |
|    20363 | 3669 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3670 | `#ifdef UNTRUST` |
|        - | 3671 | `	if( pNos < pStack ){` |
|        - | 3672 | `		goto Abort;` |
|        - | 3673 | `	}` |
|        - | 3674 | `#endif` |
|        - | 3675 | `	{` |
|        - | 3676 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|        - | 3677 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|        - | 3678 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|        - | 3679 | `		SyBlob sArMsg;` |
|    20363 | 3680 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    20363 | 3681 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 3682 | `			sxi32 rcAr;` |
|       11 | 3683 | `			VmPopOperand(&pTos,1);` |
|       11 | 3684 | `			PH7_MemObjRelease(pTos);` |
|       11 | 3685 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 3686 | `			pTos->nIdx = SXU32_HIGH;` |
|       16 | 3687 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|        5 | 3688 | `				SyBlobLength(&sArMsg));` |
|       11 | 3689 | `			SyBlobRelease(&sArMsg);` |
|       11 | 3690 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|       11 | 3691 | `			rc = rcAr;` |
|       11 | 3692 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3693 | `		}` |
|    20353 | 3694 | `		SyBlobRelease(&sArMsg);` |
|        - | 3695 | `	}` |
|        - | 3696 | `	/* Perform the addition */` |
|    20353 | 3697 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|    20353 | 3698 | `	VmPopOperand(&pTos,1);` |
|    20353 | 3699 | `	break;` |
|        - | 3700 | `				}` |
|        - | 3701 | `/*` |
|        - | 3702 | ` * OP_ADD_STORE * * *` |
|        - | 3703 | ` *` |
|        - | 3704 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 3705 | ` * and push the result back onto the stack.` |
|        - | 3706 | ` */` |
|     3035 | 3707 | `case PH7_OP_ADD_STORE:{` |
|     6075 | 3708 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3709 | `	ph7_value *pObj;` |
|        - | 3710 | `	sxu32 nIdx;` |
|        - | 3711 | `#ifdef UNTRUST` |
|        - | 3712 | `	if( pNos < pStack ){` |
|        - | 3713 | `		goto Abort;` |
|        - | 3714 | `	}` |
|        - | 3715 | `#endif` |
|        - | 3716 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     6077 | 3717 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 3718 | `	{` |
|        - | 3719 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 3720 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 3721 | `		SyBlob sArMsg;` |
|     6069 | 3722 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     6069 | 3723 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 3724 | `			sxi32 rcAr;` |
|      ! 0 | 3725 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 3726 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 3727 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 3728 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 3729 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 3730 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 3731 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 3732 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 3733 | `			rc = rcAr;` |
|      ! 0 | 3734 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3735 | `		}` |
|     6069 | 3736 | `		SyBlobRelease(&sArMsg);` |
|        - | 3737 | `	}` |
|        - | 3738 | `	/* Perform the addition */` |
|     6069 | 3739 | `	nIdx = pTos->nIdx;` |
|     6069 | 3740 | `	if( nIdx == pVm->nGlobalIdx ){` |
|        - | 3741 | `		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment` |
|        - | 3742 | `		 * (a compile-time fatal in php; raised here, same message). */` |
|        3 | 3743 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3744 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 3745 | `		pVm->iExitStatus = 255;` |
|        3 | 3746 | `		pVm->bHaltRequested = 1;` |
|        3 | 3747 | `		goto Abort;` |
|        - | 3748 | `	}` |
|     6067 | 3749 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - | 3750 | `	/* Peform the store operation */` |
|     6067 | 3751 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 3752 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6067 | 3753 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     6067 | 3754 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     6065 | 3755 | `		PH7_MemObjStore(pTos,pObj);` |
|     3030 | 3756 | `	}` |
|     6065 | 3757 | `	PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|        - | 3758 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     6065 | 3759 | `	PH7_MemObjStore(pTos,pNos);` |
|     6065 | 3760 | `	VmPopOperand(&pTos,1);` |
|     6065 | 3761 | `	break;` |
|        - | 3762 | `				}` |
|        - | 3763 | `/* OP_SUB * * *` |
|        - | 3764 | ` *` |
|        - | 3765 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 3766 | ` * first (what was next on the stack) from the second (the` |
|        - | 3767 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 3768 | ` */` |
|    25065 | 3769 | `case PH7_OP_SUB: {` |
|        - | 3770 | `	VmOpRc rcOp;` |
|    50472 | 3771 | `	sState.pTos = pTos;` |
|    50472 | 3772 | `	sState.pc = pc;` |
|    50472 | 3773 | `	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);` |
|    50472 | 3774 | `	pTos = sState.pTos;` |
|    50472 | 3775 | `	pc = sState.pc;` |
|    50472 | 3776 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3777 | `		goto Abort;` |
|    50472 | 3778 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3779 | `		goto Exception;` |
|        - | 3780 | `	}` |
|    50472 | 3781 | `	break;` |
|        - | 3782 | `					  }` |
|        - | 3783 | `/* OP_SUB_STORE * * *` |
|        - | 3784 | ` *` |
|        - | 3785 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 3786 | ` * first (what was next on the stack) from the second (the` |
|        - | 3787 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 3788 | ` */` |
|        8 | 3789 | `case PH7_OP_SUB_STORE: {` |
|        - | 3790 | `	VmOpRc rcOp;` |
|        - | 3791 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       18 | 3792 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       16 | 3793 | `	sState.pTos = pTos;` |
|       16 | 3794 | `	sState.pc = pc;` |
|       16 | 3795 | `	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);` |
|       16 | 3796 | `	pTos = sState.pTos;` |
|       16 | 3797 | `	pc = sState.pc;` |
|       16 | 3798 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3799 | `		goto Abort;` |
|       16 | 3800 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 3801 | `		goto Exception;` |
|        - | 3802 | `	}` |
|       14 | 3803 | `	break;` |
|        - | 3804 | `					  }` |
|        - | 3805 |  |
|        - | 3806 | `/*` |
|        - | 3807 | ` * OP_MOD * * *` |
|        - | 3808 | ` *` |
|        - | 3809 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3810 | ` * first (what was next on the stack) from the second (the` |
|        - | 3811 | ` * top of the stack) and push the remainder after division` |
|        - | 3812 | ` * onto the stack.` |
|        - | 3813 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 3814 | ` */` |
|      502 | 3815 | `case PH7_OP_MOD: {` |
|        - | 3816 | `	VmOpRc rcOp;` |
|     1009 | 3817 | `	sState.pTos = pTos;` |
|     1009 | 3818 | `	sState.pc = pc;` |
|     1009 | 3819 | `	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);` |
|     1009 | 3820 | `	pTos = sState.pTos;` |
|     1009 | 3821 | `	pc = sState.pc;` |
|     1009 | 3822 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3823 | `		goto Abort;` |
|     1009 | 3824 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       22 | 3825 | `		goto Exception;` |
|        - | 3826 | `	}` |
|      989 | 3827 | `	break;` |
|        - | 3828 | `					  }` |
|        - | 3829 | `/*` |
|        - | 3830 | ` * OP_MOD_STORE * * *` |
|        - | 3831 | ` *` |
|        - | 3832 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3833 | ` * first (what was next on the stack) from the second (the` |
|        - | 3834 | ` * top of the stack) and push the remainder after division` |
|        - | 3835 | ` * onto the stack.` |
|        - | 3836 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 3837 | ` */` |
|        6 | 3838 | `case PH7_OP_MOD_STORE: {` |
|        - | 3839 | `	VmOpRc rcOp;` |
|        - | 3840 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       13 | 3841 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       11 | 3842 | `	sState.pTos = pTos;` |
|       11 | 3843 | `	sState.pc = pc;` |
|       11 | 3844 | `	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);` |
|       11 | 3845 | `	pTos = sState.pTos;` |
|       11 | 3846 | `	pc = sState.pc;` |
|       11 | 3847 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3848 | `		goto Abort;` |
|       11 | 3849 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 3850 | `		goto Exception;` |
|        - | 3851 | `	}` |
|        7 | 3852 | `	break;` |
|        - | 3853 | `					  }` |
|        - | 3854 | `/*` |
|        - | 3855 | ` * OP_DIV * * *` |
|        - | 3856 | ` *` |
|        - | 3857 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3858 | ` * first (what was next on the stack) from the second (the` |
|        - | 3859 | ` * top of the stack) and push the result onto the stack.` |
|        - | 3860 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 3861 | ` */` |
|       70 | 3862 | `case PH7_OP_DIV: {` |
|        - | 3863 | `	VmOpRc rcOp;` |
|      143 | 3864 | `	sState.pTos = pTos;` |
|      143 | 3865 | `	sState.pc = pc;` |
|      143 | 3866 | `	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);` |
|      143 | 3867 | `	pTos = sState.pTos;` |
|      143 | 3868 | `	pc = sState.pc;` |
|      143 | 3869 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3870 | `		goto Abort;` |
|      143 | 3871 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 3872 | `		goto Exception;` |
|        - | 3873 | `	}` |
|      137 | 3874 | `	break;` |
|        - | 3875 | `					  }` |
|        - | 3876 | `/*` |
|        - | 3877 | ` * OP_DIV_STORE * * *` |
|        - | 3878 | ` *` |
|        - | 3879 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3880 | ` * first (what was next on the stack) from the second (the` |
|        - | 3881 | ` * top of the stack) and push the result onto the stack.` |
|        - | 3882 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 3883 | ` */` |
|        8 | 3884 | `case PH7_OP_DIV_STORE:{` |
|       17 | 3885 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3886 | `	ph7_value *pObj;` |
|        - | 3887 | `	ph7_real a,b,r;` |
|        - | 3888 | `#ifdef UNTRUST` |
|        - | 3889 | `	if( pNos < pStack ){` |
|        - | 3890 | `		goto Abort;` |
|        - | 3891 | `	}` |
|        - | 3892 | `#endif` |
|        - | 3893 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       17 | 3894 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 3895 | `	{` |
|        - | 3896 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 3897 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 3898 | `		SyBlob sArMsg;` |
|       15 | 3899 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|       15 | 3900 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"/",&sArMsg) != SXRET_OK ){` |
|        - | 3901 | `			sxi32 rcAr;` |
|      ! 0 | 3902 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 3903 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 3904 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 3905 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 3906 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 3907 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 3908 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 3909 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 3910 | `			rc = rcAr;` |
|      ! 0 | 3911 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3912 | `		}` |
|       15 | 3913 | `		SyBlobRelease(&sArMsg);` |
|        - | 3914 | `	}` |
|        - | 3915 | `	/* Force the operands to be real */` |
|       15 | 3916 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       15 | 3917 | `		PH7_MemObjToReal(pTos);` |
|        7 | 3918 | `	}` |
|       15 | 3919 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       15 | 3920 | `		PH7_MemObjToReal(pNos);` |
|        7 | 3921 | `	}` |
|        - | 3922 | `	/* Perform the requested operation */` |
|       15 | 3923 | `	a = pTos->rVal;` |
|       15 | 3924 | `	b = pNos->rVal;` |
|       15 | 3925 | `	if( b == 0 ){` |
|        - | 3926 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|        - | 3927 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|        5 | 3928 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|        9 | 3929 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      ! 0 | 3930 | `	}else{` |
|       11 | 3931 | `		r = a/b;` |
|        - | 3932 | `		/* Push the result */` |
|       11 | 3933 | `		pNos->rVal = r;` |
|       11 | 3934 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - | 3935 | `		/* Try to get an integer representation */` |
|       11 | 3936 | `		PH7_MemObjTryInteger(pNos);` |
|        - | 3937 | `	}` |
|       11 | 3938 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 3939 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       11 | 3940 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       11 | 3941 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       11 | 3942 | `		PH7_MemObjStore(pNos,pObj);` |
|        5 | 3943 | `	}` |
|       11 | 3944 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       11 | 3945 | `	VmPopOperand(&pTos,1);` |
|       11 | 3946 | `	break;` |
|        - | 3947 | `				}` |
|        - | 3948 | `/* OP_BAND * * *` |
|        - | 3949 | ` *` |
|        - | 3950 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3951 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 3952 | ` * two elements.` |
|        - | 3953 | `*/` |
|        - | 3954 | `/* OP_BOR * * *` |
|        - | 3955 | ` *` |
|        - | 3956 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3957 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 3958 | ` * two elements.` |
|        - | 3959 | ` */` |
|        - | 3960 | `/* OP_BXOR * * *` |
|        - | 3961 | ` *` |
|        - | 3962 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3963 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 3964 | ` * two elements.` |
|        - | 3965 | ` */` |
|      673 | 3966 | `case PH7_OP_BAND:` |
|        - | 3967 | `case PH7_OP_BOR:` |
|        - | 3968 | `case PH7_OP_BXOR:{` |
|     1349 | 3969 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3970 | `	sxi64 a,b,r;` |
|        - | 3971 | `	int cBwOp;` |
|        - | 3972 | `#ifdef UNTRUST` |
|        - | 3973 | `	if( pNos < pStack ){` |
|        - | 3974 | `		goto Abort;` |
|        - | 3975 | `	}` |
|        - | 3976 | `#endif` |
|     1349 | 3977 | `	cBwOp = pInstr->iOp == PH7_OP_BOR ? '\|' : (pInstr->iOp == PH7_OP_BXOR ? '^' : '&');` |
|     1349 | 3978 | `	if( (pNos->iFlags & MEMOBJ_STRING) && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 3979 | `		/* TWO strings: php's per-byte string operation, result a string. */` |
|       35 | 3980 | `		PH7_STRING_BITWISE_RESULT(pNos,pNos,pTos,cBwOp)` |
|       35 | 3981 | `		VmPopOperand(&pTos,1);` |
|       35 | 3982 | `		break;` |
|        - | 3983 | `	}` |
|        - | 3984 | `	{` |
|        - | 3985 | `		char zBwOp[2];` |
|     1315 | 3986 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 3987 | `		/* Anything else is php's arithmetic operand contract, named for this` |
|        - | 3988 | ``		 * operator (`array & int`), not a silent integer cast. */`` |
|     1317 | 3989 | `		PH7_BITWISE_ARITH_CONTRACT(pNos,pTos,zBwOp,1)` |
|        - | 3990 | `	}` |
|        - | 3991 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     1283 | 3992 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     1283 | 3993 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     1277 | 3994 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     1277 | 3995 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     1277 | 3996 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|        9 | 3997 | `		PH7_MemObjToInteger(pTos);` |
|        4 | 3998 | `	}` |
|     1277 | 3999 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|       11 | 4000 | `		PH7_MemObjToInteger(pNos);` |
|        5 | 4001 | `	}` |
|        - | 4002 | `	/* Perform the requested operation */` |
|     1277 | 4003 | `	a = pNos->x.iVal;` |
|     1277 | 4004 | `	b = pTos->x.iVal;` |
|     1277 | 4005 | `	switch(pInstr->iOp){` |
|      240 | 4006 | `	case PH7_OP_BOR_STORE:` |
|      485 | 4007 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        8 | 4008 | `	case PH7_OP_BXOR_STORE:` |
|       17 | 4009 | `	case PH7_OP_BXOR: r = a^b; break;` |
|      388 | 4010 | `	case PH7_OP_BAND_STORE:` |
|      388 | 4011 | `	case PH7_OP_BAND:` |
|      781 | 4012 | `	default:          r = a&b; break;` |
|        - | 4013 | `	}` |
|        - | 4014 | `	/* Push the result */` |
|     1277 | 4015 | `	pNos->x.iVal = r;` |
|     1277 | 4016 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     1277 | 4017 | `	VmPopOperand(&pTos,1);` |
|     1277 | 4018 | `	break;` |
|        - | 4019 | `				 }` |
|        - | 4020 | `/* OP_BAND_STORE * * *` |
|        - | 4021 | ` *` |
|        - | 4022 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4023 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 4024 | ` * two elements.` |
|        - | 4025 | `*/` |
|        - | 4026 | `/* OP_BOR_STORE * * *` |
|        - | 4027 | ` *` |
|        - | 4028 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4029 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 4030 | ` * two elements.` |
|        - | 4031 | ` */` |
|        - | 4032 | `/* OP_BXOR_STORE * * *` |
|        - | 4033 | ` *` |
|        - | 4034 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4035 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 4036 | ` * two elements.` |
|        - | 4037 | ` */` |
|       50 | 4038 | `case PH7_OP_BAND_STORE:` |
|        - | 4039 | `case PH7_OP_BOR_STORE:` |
|        - | 4040 | `case PH7_OP_BXOR_STORE:{` |
|      101 | 4041 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4042 | `	ph7_value *pObj;` |
|        - | 4043 | `	sxi64 a,b,r;` |
|        - | 4044 | `	int cBwOp,bBwStr;` |
|        - | 4045 | `#ifdef UNTRUST` |
|        - | 4046 | `	if( pNos < pStack ){` |
|        - | 4047 | `		goto Abort;` |
|        - | 4048 | `	}` |
|        - | 4049 | `#endif` |
|        - | 4050 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      101 | 4051 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       95 | 4052 | `	cBwOp = pInstr->iOp == PH7_OP_BOR_STORE ? '\|' : (pInstr->iOp == PH7_OP_BXOR_STORE ? '^' : '&');` |
|       95 | 4053 | `	bBwStr = (pNos->iFlags & MEMOBJ_STRING) != 0 && (pTos->iFlags & MEMOBJ_STRING) != 0;` |
|       95 | 4054 | `	if( !bBwStr ){` |
|        - | 4055 | `		char zBwOp[2];` |
|       89 | 4056 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 4057 | ``		/* `$x &= v` answers the same contract as `$x & v` (php's compound`` |
|        - | 4058 | `		 * assignment is the operator plus a store), but through its own error` |
|        - | 4059 | `		 * path, which is POSITIONAL: the lvalue is named first even when the` |
|        - | 4060 | ``		 * right operand is the offender (`$x = 1; $x &= $o` is "int & BsocP",`` |
|        - | 4061 | ``		 * where the plain `1 & $o` is "BsocP & int"). */`` |
|       89 | 4062 | `		PH7_BITWISE_ARITH_CONTRACT(pTos,pNos,zBwOp,0)` |
|        - | 4063 | `		/* Force the operands to be integer (php deprecates a lossy float here) */` |
|       85 | 4064 | `		rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|       85 | 4065 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|       85 | 4066 | `		rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       85 | 4067 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|       85 | 4068 | `		if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 4069 | `			PH7_MemObjToInteger(pTos);` |
|      ! 0 | 4070 | `		}` |
|       85 | 4071 | `		if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        3 | 4072 | `			PH7_MemObjToInteger(pNos);` |
|        1 | 4073 | `		}` |
|       42 | 4074 | `	}` |
|       91 | 4075 | `	if( bBwStr ){` |
|        - | 4076 | `		/* TWO strings: php's per-byte string operation, result a string. The` |
|        - | 4077 | `		 * result lands in pNos, which the store tail below writes into the` |
|        - | 4078 | `		 * lvalue's slot exactly like the integer result. */` |
|        7 | 4079 | `		PH7_STRING_BITWISE_RESULT(pNos,pTos,pNos,cBwOp)` |
|        4 | 4080 | `	}else{` |
|        - | 4081 | `	/* Perform the requested operation */` |
|       85 | 4082 | `	a = pTos->x.iVal;` |
|       85 | 4083 | `	b = pNos->x.iVal;` |
|       85 | 4084 | `	switch(pInstr->iOp){` |
|       32 | 4085 | `	case PH7_OP_BOR_STORE:` |
|       65 | 4086 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        5 | 4087 | `	case PH7_OP_BXOR_STORE:` |
|       11 | 4088 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        5 | 4089 | `	case PH7_OP_BAND_STORE:` |
|        5 | 4090 | `	case PH7_OP_BAND:` |
|       11 | 4091 | `	default:          r = a&b; break;` |
|        - | 4092 | `	}` |
|        - | 4093 | `	/* Push the result */` |
|       85 | 4094 | `	pNos->x.iVal = r;` |
|       85 | 4095 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        - | 4096 | `	}` |
|       91 | 4097 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 4098 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       91 | 4099 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       91 | 4100 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       91 | 4101 | `		PH7_MemObjStore(pNos,pObj);` |
|       45 | 4102 | `	}` |
|       91 | 4103 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       91 | 4104 | `	VmPopOperand(&pTos,1);` |
|       91 | 4105 | `	break;` |
|        - | 4106 | `				 }` |
|        - | 4107 | `/* OP_SHL * * *` |
|        - | 4108 | ` *` |
|        - | 4109 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4110 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4111 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4112 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4113 | ` */` |
|        - | 4114 | `/* OP_SHR * * *` |
|        - | 4115 | ` *` |
|        - | 4116 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4117 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4118 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4119 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4120 | ` */` |
|       55 | 4121 | `case PH7_OP_SHL:` |
|        - | 4122 | `case PH7_OP_SHR: {` |
|        - | 4123 | `	VmOpRc rcOp;` |
|      113 | 4124 | `	sState.pTos = pTos;` |
|      113 | 4125 | `	sState.pc = pc;` |
|      113 | 4126 | `	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);` |
|      113 | 4127 | `	pTos = sState.pTos;` |
|      113 | 4128 | `	pc = sState.pc;` |
|      113 | 4129 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4130 | `		goto Abort;` |
|      113 | 4131 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       34 | 4132 | `		goto Exception;` |
|        - | 4133 | `	}` |
|       80 | 4134 | `	break;` |
|        - | 4135 | `					  }` |
|        - | 4136 | `/*  OP_SHL_STORE * * *` |
|        - | 4137 | ` *` |
|        - | 4138 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4139 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4140 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4141 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4142 | ` */` |
|        - | 4143 | `/* OP_SHR_STORE * * *` |
|        - | 4144 | ` *` |
|        - | 4145 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4146 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4147 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4148 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4149 | ` */` |
|       20 | 4150 | `case PH7_OP_SHL_STORE:` |
|        - | 4151 | `case PH7_OP_SHR_STORE: {` |
|        - | 4152 | `	VmOpRc rcOp;` |
|        - | 4153 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       41 | 4154 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       37 | 4155 | `	sState.pTos = pTos;` |
|       37 | 4156 | `	sState.pc = pc;` |
|       37 | 4157 | `	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);` |
|       37 | 4158 | `	pTos = sState.pTos;` |
|       37 | 4159 | `	pc = sState.pc;` |
|       37 | 4160 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4161 | `		goto Abort;` |
|       37 | 4162 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 4163 | `		goto Exception;` |
|        - | 4164 | `	}` |
|       29 | 4165 | `	break;` |
|        - | 4166 | `					  }` |
|        - | 4167 | `/* CAT:  P1 * *` |
|        - | 4168 | ` *` |
|        - | 4169 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - | 4170 | ` * back.` |
|        - | 4171 | ` */` |
|   106443 | 4172 | `case PH7_OP_CAT:{` |
|        - | 4173 | `	ph7_value *pNos,*pCur;` |
|   212891 | 4174 | `	if( pInstr->iP1 < 1 ){` |
|   178577 | 4175 | `		pNos = &pTos[-1];` |
|    89291 | 4176 | `	}else{` |
|    34319 | 4177 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - | 4178 | `	}` |
|        - | 4179 | `#ifdef UNTRUST` |
|        - | 4180 | `	if( pNos < pStack ){` |
|        - | 4181 | `		goto Abort;` |
|        - | 4182 | `	}` |
|        - | 4183 | `#endif` |
|        - | 4184 | `	/* Force a string cast (user-visible: warns on an array operand, §2).` |
|        - | 4185 | `	 * php coerces the operands left to right, so the leftmost not-stringable` |
|        - | 4186 | `	 * object is the one that throws. */` |
|        - | 4187 | `	{` |
|   212891 | 4188 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|   212891 | 4189 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4190 | `	}` |
|   212889 | 4191 | `	pCur = &pNos[1];` |
|        - | 4192 | `	{` |
|        - | 4193 | `		/* rcSv leaves the loop with the coercion status: the routing macro expands` |
|        - | 4194 | ``		 * to a bare `break` here, which inside the while would fall THROUGH to the`` |
|        - | 4195 | ``		 * `pTos = pNos` below with a throw pending. Route it at case level. */`` |
|   212889 | 4196 | `		sxi32 rcSv = SXRET_OK;` |
|   436261 | 4197 | `		while( pCur <= pTos ){` |
|   223821 | 4198 | `			rcSv = PH7_MemObjToStringUV(pCur);` |
|   223821 | 4199 | `			if( rcSv != SXRET_OK ){` |
|      447 | 4200 | `				break;` |
|        - | 4201 | `			}` |
|        - | 4202 | `			/* Perform the concatenation */` |
|   223377 | 4203 | `			if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   223091 | 4204 | `				if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - | 4205 | `					/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 | 4206 | `					PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4207 | `					goto Abort;` |
|        - | 4208 | `				}` |
|   111543 | 4209 | `			}` |
|   223377 | 4210 | `			SyBlobRelease(&pCur->sBlob);` |
|   223377 | 4211 | `			pCur++;` |
|        5 | 4212 | `		}` |
|   213711 | 4213 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4214 | `	}` |
|   212445 | 4215 | `	pTos = pNos;` |
|   212445 | 4216 | `	break;` |
|        - | 4217 | `				}` |
|        - | 4218 | `/*  CAT_STORE: * * *` |
|        - | 4219 | ` *` |
|        - | 4220 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - | 4221 | ` * back.` |
|        - | 4222 | ` */` |
|    19656 | 4223 | `case PH7_OP_CAT_STORE:{` |
|    39316 | 4224 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4225 | `	ph7_value *pObj;` |
|        - | 4226 | `	sxu32 nIdx;` |
|        - | 4227 | `#ifdef UNTRUST` |
|        - | 4228 | `	if( pNos < pStack ){` |
|        - | 4229 | `		goto Abort;` |
|        - | 4230 | `	}` |
|        - | 4231 | `#endif` |
|        - | 4232 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|    58967 | 4233 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 4234 | `	/* The right operand must be a string to append it (user-visible, §2) */` |
|        - | 4235 | `	{` |
|    39314 | 4236 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|    39318 | 4237 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4238 | `	}` |
|    39304 | 4239 | `	nIdx = pTos->nIdx;` |
|        - | 4240 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - | 4241 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - | 4242 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - | 4243 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - | 4244 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - | 4245 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - | 4246 | `	 * the source we copy from — references share the slot index, so one check` |
|        - | 4247 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - | 4248 | `	 * must run before any mutation (left to the slow path).` |
|        - | 4249 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - | 4250 | `	 * and remains O(n^2) by design. */` |
|    39299 | 4251 | `	if( nIdx != SXU32_HIGH` |
|    39299 | 4252 | `	 && nIdx != pNos->nIdx` |
|    39295 | 4253 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|    39296 | 4254 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|    19754 | 4255 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|        - | 4256 | `		/* e.g. $x = 5; $x .= "a";  ->  "5a" (user-visible: warns if $x is an array,` |
|        - | 4257 | `		 * throws if it is a not-stringable object — and then the lvalue keeps` |
|        - | 4258 | `		 * holding that object, since the throw abandons the coercion) */` |
|        - | 4259 | `		{` |
|    39290 | 4260 | `			sxi32 rcSv = PH7_MemObjToStringUV(pObj);` |
|    39298 | 4261 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4262 | `		}` |
|    39286 | 4263 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|    39228 | 4264 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4265 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - | 4266 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 | 4267 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4268 | `				goto Abort;` |
|        - | 4269 | `			}` |
|    19611 | 4270 | `		}` |
|        - | 4271 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - | 4272 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - | 4273 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - | 4274 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - | 4275 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - | 4276 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - | 4277 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - | 4278 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - | 4279 | `		 * the same slot is appended to again later in the statement` |
|        - | 4280 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - | 4281 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - | 4282 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|    39286 | 4283 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 | 4284 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 | 4285 | `		}` |
|        - | 4286 | `		/* A hooked lvalue's scratch slot was appended in place — dispatch the` |
|        - | 4287 | `		 * set side now (the consume reads the computed value from the slot). */` |
|    39286 | 4288 | `		PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|    39286 | 4289 | `		pNos->nIdx = SXU32_HIGH;` |
|    39286 | 4290 | `		VmPopOperand(&pTos,1);` |
|    39286 | 4291 | `		break;` |
|        - | 4292 | `	}` |
|        - | 4293 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|        - | 4294 | `	/* Force a string cast (user-visible: warns if the lvalue is an array, §2) */` |
|        - | 4295 | `	{` |
|       16 | 4296 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       16 | 4297 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4298 | `	}` |
|        - | 4299 | `	/* Perform the concatenation (Reverse order) */` |
|       16 | 4300 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 | 4301 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4302 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - | 4303 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 | 4304 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4305 | `			goto Abort;` |
|        - | 4306 | `		}` |
|        7 | 4307 | `	}` |
|        - | 4308 | `	/* Perform the store operation */` |
|       16 | 4309 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 4310 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 | 4311 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       24 | 4312 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 | 4313 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 | 4314 | `	}` |
|       11 | 4315 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       11 | 4316 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 | 4317 | `	VmPopOperand(&pTos,1);` |
|       11 | 4318 | `	break;` |
|        - | 4319 | `				}` |
|        - | 4320 | `/* OP_AND: * * *` |
|        - | 4321 | ` *` |
|        - | 4322 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - | 4323 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4324 | ` * stack.` |
|        - | 4325 | ` */` |
|        - | 4326 | `/* OP_OR: * * *` |
|        - | 4327 | ` *` |
|        - | 4328 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - | 4329 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4330 | ` * stack.` |
|        - | 4331 | ` */` |
|   170871 | 4332 | `case PH7_OP_LAND:` |
|        - | 4333 | `case PH7_OP_LOR: {` |
|        - | 4334 | `	VmOpRc rcOp;` |
|   342194 | 4335 | `	sState.pTos = pTos;` |
|   342194 | 4336 | `	sState.pc = pc;` |
|   342194 | 4337 | `	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);` |
|   342194 | 4338 | `	pTos = sState.pTos;` |
|   342194 | 4339 | `	pc = sState.pc;` |
|   342194 | 4340 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4341 | `		goto Abort;` |
|   342194 | 4342 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4343 | `		goto Exception;` |
|        - | 4344 | `	}` |
|   342194 | 4345 | `	break;` |
|        - | 4346 | `					  }` |
|        - | 4347 | `/*` |
|        - | 4348 | ` * OP_NULLC: * * *` |
|        - | 4349 | ` * Null coalescing operator '??'.` |
|        - | 4350 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - | 4351 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - | 4352 | ` */` |
|        - | 4353 | `/*` |
|        - | 4354 | ` * OP_NULLC: * P2 *` |
|        - | 4355 | ` * Short-circuit null coalescing '??'.` |
|        - | 4356 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - | 4357 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - | 4358 | ` */` |
|      354 | 4359 | `case PH7_OP_NULLC: {` |
|        - | 4360 | `#ifdef UNTRUST` |
|        - | 4361 | `	if( pTos < pStack ){` |
|        - | 4362 | `		goto Abort;` |
|        - | 4363 | `	}` |
|        - | 4364 | `#endif` |
|      713 | 4365 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 4366 | `		/* Left is not null — keep it and skip the RHS */` |
|      513 | 4367 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|      259 | 4368 | `	}else{` |
|        - | 4369 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|      205 | 4370 | `		VmPopOperand(&pTos, 1);` |
|        - | 4371 | `	}` |
|      713 | 4372 | `	break;` |
|        - | 4373 | `}` |
|        - | 4374 | `/*` |
|        - | 4375 | ` * OP_NULLC_JMP: * P2 *` |
|        - | 4376 | ` * Null coalescing assignment short-circuit.` |
|        - | 4377 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - | 4378 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - | 4379 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - | 4380 | ` */` |
|       72 | 4381 | `case PH7_OP_NULLC_JMP: {` |
|        - | 4382 | `#ifdef UNTRUST` |
|        - | 4383 | `	if( pTos < pStack ){` |
|        - | 4384 | `		goto Abort;` |
|        - | 4385 | `	}` |
|        - | 4386 | `#endif` |
|      147 | 4387 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       38 | 4388 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —` |
|        - | 4389 | `		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is` |
|        - | 4390 | `		 * dropped by the fetch-point sweep (the landing pc is past its` |
|        - | 4391 | `		 * OP_NULLC_STORE window): the skipped assign never dispatches. */` |
|       18 | 4392 | `	}` |
|      147 | 4393 | `	break;` |
|        - | 4394 | `}` |
|        - | 4395 | `/*` |
|        - | 4396 | ` * OP_NULLC_STORE: * * *` |
|        - | 4397 | ` * Null coalescing assignment store.` |
|        - | 4398 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - | 4399 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - | 4400 | ` * expression result.` |
|        - | 4401 | ` */` |
|        - | 4402 | `/*` |
|        - | 4403 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - | 4404 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - | 4405 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - | 4406 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - | 4407 | ` * non-null, fall through without modifying the stack so the following` |
|        - | 4408 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - | 4409 | ` */` |
|       68 | 4410 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - | 4411 | `#ifdef UNTRUST` |
|        - | 4412 | `	if( pTos < pStack ){` |
|        - | 4413 | `		goto Abort;` |
|        - | 4414 | `	}` |
|        - | 4415 | `#endif` |
|      140 | 4416 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - | 4417 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - | 4418 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       58 | 4419 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       28 | 4420 | `	}` |
|      140 | 4421 | `	break;` |
|        - | 4422 | `}` |
|       51 | 4423 | `case PH7_OP_NULLC_STORE: {` |
|        - | 4424 | `	VmOpRc rcOp;` |
|      105 | 4425 | `	sState.pTos = pTos;` |
|      105 | 4426 | `	sState.pc = pc;` |
|      105 | 4427 | `	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);` |
|      105 | 4428 | `	pTos = sState.pTos;` |
|      105 | 4429 | `	pc = sState.pc;` |
|      105 | 4430 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4431 | `		goto Abort;` |
|      105 | 4432 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 4433 | `		goto Exception;` |
|        - | 4434 | `	}` |
|       99 | 4435 | `	break;` |
|        - | 4436 | `					  }` |
|        - | 4437 | `/*` |
|        - | 4438 | ` * OP_SPREAD: * * *` |
|        - | 4439 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - | 4440 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - | 4441 | ` * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this` |
|        - | 4442 | ` * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —` |
|        - | 4443 | ` * the CALL may not be the next instruction, and may be an inner call whose own` |
|        - | 4444 | ` * spreads must stay scoped to it.` |
|        - | 4445 | ` * The expansion tail is shared between the plain-array and the materialized` |
|        - | 4446 | ` * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.` |
|        - | 4447 | ` */` |
|      306 | 4448 | `case PH7_OP_SPREAD: {` |
|        - | 4449 | `#ifdef UNTRUST` |
|        - | 4450 | `	if( pTos < pStack ){` |
|        - | 4451 | `		goto Abort;` |
|        - | 4452 | `	}` |
|        - | 4453 | `#endif` |
|        - | 4454 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - | 4455 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - | 4456 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - | 4457 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - | 4458 | `	 * be freed immediately. */` |
|      616 | 4459 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 | 4460 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - | 4461 | `		sxi32 rcW;` |
|        3 | 4462 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 | 4463 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 | 4464 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 | 4465 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 4466 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 4467 | `			goto Exception;` |
|        - | 4468 | `		}` |
|        - | 4469 | `		/* Grow the operand stack if this expansion would overflow it (no longer a` |
|        - | 4470 | `		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a` |
|        - | 4471 | `		 * single argument — the historical bounded-fallback, now only under OOM. */` |
|        4 | 4472 | `		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,` |
|        1 | 4473 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 4474 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 4475 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 4476 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 4477 | `				pTmpMap->nEntry);` |
|      ! 0 | 4478 | `			break;` |
|        - | 4479 | `		}` |
|        3 | 4480 | `		VmSpreadExpandMap(pVm, &pTos, pTmpMap, 0/*a Traversable's values are not the caller's slots*/);` |
|        3 | 4481 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 | 4482 | `		break;` |
|        - | 4483 | `	}` |
|      614 | 4484 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      614 | 4485 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      919 | 4486 | `		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,` |
|      305 | 4487 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 4488 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 4489 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 4490 | `				pMap->nEntry);` |
|      ! 0 | 4491 | `			break;` |
|        - | 4492 | `		}` |
|      614 | 4493 | `		VmSpreadExpandMap(pVm, &pTos, pMap, pInstr->iP1 != 0);` |
|      305 | 4494 | `	}` |
|        - | 4495 | `	/* else: not an array — leave as-is (single arg) */` |
|      614 | 4496 | `	break;` |
|        - | 4497 | `}` |
|        - | 4498 | `/*` |
|        - | 4499 | ` * OP_FLAG_SPREAD: * * *` |
|        - | 4500 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - | 4501 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - | 4502 | ` */` |
|      342 | 4503 | `case PH7_OP_FLAG_SPREAD: {` |
|        - | 4504 | `#ifdef UNTRUST` |
|        - | 4505 | `	if( pTos < pStack ){` |
|        - | 4506 | `		goto Abort;` |
|        - | 4507 | `	}` |
|        - | 4508 | `#endif` |
|      688 | 4509 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|      688 | 4510 | `	break;` |
|        - | 4511 | `}` |
|        - | 4512 | `/* OP_LXOR: * * *` |
|        - | 4513 | ` *` |
|        - | 4514 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - | 4515 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4516 | ` * stack.` |
|        - | 4517 | ` * According to the PHP language reference manual:` |
|        - | 4518 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - | 4519 | ` *  TRUE,but not both.` |
|        - | 4520 | ` */` |
|        6 | 4521 | `case PH7_OP_LXOR:{` |
|       13 | 4522 | `	ph7_value *pNos = &pTos[-1];` |
|       13 | 4523 | `	sxi32 v = 0;` |
|        - | 4524 | `#ifdef UNTRUST` |
|        - | 4525 | `	if( pNos < pStack ){` |
|        - | 4526 | `		goto Abort;` |
|        - | 4527 | `	}` |
|        - | 4528 | `#endif` |
|        - | 4529 | `	/* Force a boolean cast */` |
|       13 | 4530 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 4531 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 4532 | `	}` |
|       13 | 4533 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 4534 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 | 4535 | `	}` |
|       13 | 4536 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 | 4537 | `		v = 1;` |
|        3 | 4538 | `	}` |
|       13 | 4539 | `	VmPopOperand(&pTos,1);` |
|       13 | 4540 | `	pTos->x.iVal = v;` |
|       13 | 4541 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       13 | 4542 | `	break;` |
|        - | 4543 | `				 }` |
|        - | 4544 | `/* OP_EQ P1 P2 P3` |
|        - | 4545 | ` *` |
|        - | 4546 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - | 4547 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - | 4548 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4549 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4550 | ` */` |
|        - | 4551 | `/* OP_NEQ P1 P2 P3` |
|        - | 4552 | ` *` |
|        - | 4553 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - | 4554 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 4555 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4556 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4557 | ` */` |
|     6529 | 4558 | `case PH7_OP_EQ:` |
|        - | 4559 | `case PH7_OP_NEQ: {` |
|        - | 4560 | `	VmOpRc rcOp;` |
|    13060 | 4561 | `	sState.pTos = pTos;` |
|    13060 | 4562 | `	sState.pc = pc;` |
|    13060 | 4563 | `	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);` |
|    13060 | 4564 | `	pTos = sState.pTos;` |
|    13060 | 4565 | `	pc = sState.pc;` |
|    13060 | 4566 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4567 | `		goto Abort;` |
|    13060 | 4568 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4569 | `		goto Exception;` |
|        - | 4570 | `	}` |
|    13060 | 4571 | `	break;` |
|        - | 4572 | `					  }` |
|        - | 4573 | `/* OP_TEQ P1 P2 *` |
|        - | 4574 | ` *` |
|        - | 4575 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - | 4576 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 4577 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4578 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4579 | ` */` |
|   343714 | 4580 | `case PH7_OP_TEQ: {` |
|        - | 4581 | `	VmOpRc rcOp;` |
|   688674 | 4582 | `	sState.pTos = pTos;` |
|   688674 | 4583 | `	sState.pc = pc;` |
|   688674 | 4584 | `	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);` |
|   688674 | 4585 | `	pTos = sState.pTos;` |
|   688674 | 4586 | `	pc = sState.pc;` |
|   688674 | 4587 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4588 | `		goto Abort;` |
|   688674 | 4589 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4590 | `		goto Exception;` |
|        - | 4591 | `	}` |
|   688674 | 4592 | `	break;` |
|        - | 4593 | `					  }` |
|        - | 4594 | `/* OP_TNE P1 P2 *` |
|        - | 4595 | ` *` |
|        - | 4596 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - | 4597 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - | 4598 | ` * instruction.` |
|        - | 4599 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4600 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4601 | ` *` |
|        - | 4602 | ` */` |
|   302844 | 4603 | `case PH7_OP_TNE: {` |
|        - | 4604 | `	VmOpRc rcOp;` |
|   606144 | 4605 | `	sState.pTos = pTos;` |
|   606144 | 4606 | `	sState.pc = pc;` |
|   606144 | 4607 | `	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);` |
|   606144 | 4608 | `	pTos = sState.pTos;` |
|   606144 | 4609 | `	pc = sState.pc;` |
|   606144 | 4610 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4611 | `		goto Abort;` |
|   606144 | 4612 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4613 | `		goto Exception;` |
|        - | 4614 | `	}` |
|   606144 | 4615 | `	break;` |
|        - | 4616 | `					  }` |
|        - | 4617 | `/* OP_LT P1 P2 P3` |
|        - | 4618 | ` *` |
|        - | 4619 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4620 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 4621 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4622 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4623 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4624 | ` *` |
|        - | 4625 | ` */` |
|        - | 4626 | `/* OP_LE P1 P2 P3` |
|        - | 4627 | ` *` |
|        - | 4628 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4629 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 4630 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4631 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4632 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4633 | ` *` |
|        - | 4634 | ` */` |
|   286441 | 4635 | `case PH7_OP_LT:` |
|        - | 4636 | `case PH7_OP_LE: {` |
|        - | 4637 | `	VmOpRc rcOp;` |
|   574178 | 4638 | `	sState.pTos = pTos;` |
|   574178 | 4639 | `	sState.pc = pc;` |
|   574178 | 4640 | `	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);` |
|   574178 | 4641 | `	pTos = sState.pTos;` |
|   574178 | 4642 | `	pc = sState.pc;` |
|   574178 | 4643 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4644 | `		goto Abort;` |
|   574178 | 4645 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4646 | `		goto Exception;` |
|        - | 4647 | `	}` |
|   574178 | 4648 | `	break;` |
|        - | 4649 | `					  }` |
|        - | 4650 | `/* OP_GT P1 P2 P3` |
|        - | 4651 | ` *` |
|        - | 4652 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4653 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 4654 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4655 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4656 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4657 | ` *` |
|        - | 4658 | ` */` |
|        - | 4659 | `/* OP_GE P1 P2 P3` |
|        - | 4660 | ` *` |
|        - | 4661 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4662 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 4663 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4664 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4665 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4666 | ` *` |
|        - | 4667 | ` */` |
|   135936 | 4668 | `case PH7_OP_GT:` |
|        - | 4669 | `case PH7_OP_GE: {` |
|        - | 4670 | `	VmOpRc rcOp;` |
|   272330 | 4671 | `	sState.pTos = pTos;` |
|   272330 | 4672 | `	sState.pc = pc;` |
|   272330 | 4673 | `	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);` |
|   272330 | 4674 | `	pTos = sState.pTos;` |
|   272330 | 4675 | `	pc = sState.pc;` |
|   272330 | 4676 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4677 | `		goto Abort;` |
|   272330 | 4678 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4679 | `		goto Exception;` |
|        - | 4680 | `	}` |
|   272330 | 4681 | `	break;` |
|        - | 4682 | `					  }` |
|        - | 4683 | `/* OP_SPACESHIP * * *` |
|        - | 4684 | ` *` |
|        - | 4685 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - | 4686 | ` *   -1 if left < right` |
|        - | 4687 | ` *    0 if left == right` |
|        - | 4688 | ` *    1 if left > right` |
|        - | 4689 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - | 4690 | ` */` |
|      283 | 4691 | `case PH7_OP_SPACESHIP: {` |
|        - | 4692 | `	VmOpRc rcOp;` |
|      571 | 4693 | `	sState.pTos = pTos;` |
|      571 | 4694 | `	sState.pc = pc;` |
|      571 | 4695 | `	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);` |
|      571 | 4696 | `	pTos = sState.pTos;` |
|      571 | 4697 | `	pc = sState.pc;` |
|      571 | 4698 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4699 | `		goto Abort;` |
|      571 | 4700 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4701 | `		goto Exception;` |
|        - | 4702 | `	}` |
|      571 | 4703 | `	break;` |
|        - | 4704 | `					  }` |
|        - | 4705 | `/*` |
|        - | 4706 | ` * OP_LOAD_REF * * *` |
|        - | 4707 | ` * Push the index of a referenced object on the stack.` |
|        - | 4708 | ` */` |
|       82 | 4709 | `case PH7_OP_LOAD_REF: {` |
|        - | 4710 | `	sxu32 nIdx;` |
|        - | 4711 | `#ifdef UNTRUST` |
|        - | 4712 | `	if( pTos < pStack ){` |
|        - | 4713 | `		goto Abort;` |
|        - | 4714 | `	}` |
|        - | 4715 | `#endif` |
|      166 | 4716 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|        - | 4717 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|        - | 4718 | `		 * out of a string still carries the BASE VARIABLE's slot index, so taking a` |
|        - | 4719 | `` 		 * reference to it aliased the WHOLE STRING — `$x = [&$s[1]]; $x[0] = "Z";` `` |
|        - | 4720 | ``		 * left $s === "Z". Same Error the `=&` and by-ref-argument paths raise. */`` |
|        3 | 4721 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|        - | 4722 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|        3 | 4723 | `		PH7_MemObjRelease(pTos);` |
|        3 | 4724 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 4725 | `		pTos->nIdx = SXU32_HIGH;` |
|        3 | 4726 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|        3 | 4727 | `		break;` |
|        - | 4728 | `	}` |
|        - | 4729 | `	/* Extract memory object index */` |
|      163 | 4730 | `	nIdx = pTos->nIdx;` |
|      163 | 4731 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - | 4732 | `		/* Nullify the object */` |
|      163 | 4733 | `		PH7_MemObjRelease(pTos);` |
|        - | 4734 | `		/* Mark as constant and store the index on the top of the stack */` |
|      163 | 4735 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      163 | 4736 | `		pTos->nIdx = SXU32_HIGH;` |
|      163 | 4737 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       81 | 4738 | `	}` |
|      163 | 4739 | `	break;` |
|        - | 4740 | `					  }` |
|        - | 4741 | `/*` |
|        - | 4742 | ` * OP_STORE_REF * * P3` |
|        - | 4743 | ` * Perform an assignment operation by reference.` |
|        - | 4744 | ` */` |
|     1606 | 4745 | `case PH7_OP_STORE_REF: {` |
|        - | 4746 | `	VmOpRc rcOp;` |
|     3217 | 4747 | `	sState.pTos = pTos;` |
|     3217 | 4748 | `	sState.pc = pc;` |
|     3217 | 4749 | `	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);` |
|     3217 | 4750 | `	pTos = sState.pTos;` |
|     3217 | 4751 | `	pc = sState.pc;` |
|     3217 | 4752 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 4753 | `		goto Abort;` |
|     3214 | 4754 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 4755 | `		goto Exception;` |
|        - | 4756 | `	}` |
|     3206 | 4757 | `	break;` |
|        - | 4758 | `					  }` |
|        - | 4759 | `/*` |
|        - | 4760 | ` * OP_UPLINK P1 * *` |
|        - | 4761 | ` * Link a variable to the top active VM frame.` |
|        - | 4762 | ` * This is used to implement the 'global' PHP construct.` |
|        - | 4763 | ` */` |
|      109 | 4764 | `case PH7_OP_UPLINK: {` |
|      223 | 4765 | `	if( pVm->pFrame->pParent ){` |
|      223 | 4766 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - | 4767 | `		SyString sName;` |
|        - | 4768 | `		/* rcSv carries the coercion status OUT of the loop: the routing macro is a` |
|        - | 4769 | ``		 * bare `break` here, which would only leave the while and then pop the`` |
|        - | 4770 | `		 * operands with a throw pending. */` |
|      223 | 4771 | `		sxi32 rcSv = SXRET_OK;` |
|        - | 4772 | `		/* Perform the link */` |
|      453 | 4773 | `		while( pLink <= pTos ){` |
|        - | 4774 | `			/* Force a string cast — global $$arr link name (user-visible, §2) */` |
|      241 | 4775 | `			rcSv = PH7_MemObjToStringUV(pLink);` |
|      241 | 4776 | `			if( rcSv != SXRET_OK ){` |
|        7 | 4777 | `				break;` |
|        - | 4778 | `			}` |
|      235 | 4779 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|      235 | 4780 | `			if( sName.nByte > 0 ){` |
|      235 | 4781 | `				VmFrameLink(&(*pVm),&sName);` |
|      115 | 4782 | `			}` |
|      235 | 4783 | `			pLink++;` |
|        5 | 4784 | `		}` |
|      223 | 4785 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|      106 | 4786 | `	}` |
|      217 | 4787 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|      217 | 4788 | `	break;` |
|        - | 4789 | `					}` |
|        - | 4790 | `/*` |
|        - | 4791 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - | 4792 | ` * Push an exception in the corresponding container so that` |
|        - | 4793 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - | 4794 | ` */` |
|   729548 | 4795 | `case PH7_OP_LOAD_EXCEPTION: {` |
|        - | 4796 | `	VmOpRc rcOp;` |
|  1459101 | 4797 | `	sState.pTos = pTos;` |
|  1459101 | 4798 | `	sState.pc = pc;` |
|  1459101 | 4799 | `	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);` |
|  1459101 | 4800 | `	pTos = sState.pTos;` |
|  1459101 | 4801 | `	pc = sState.pc;` |
|  1459101 | 4802 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4803 | `		goto Abort;` |
|  1459101 | 4804 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4805 | `		goto Exception;` |
|        - | 4806 | `	}` |
|  1459101 | 4807 | `	break;` |
|        - | 4808 | `					  }` |
|        - | 4809 | `/*` |
|        - | 4810 | ` * OP_POP_EXCEPTION * * P3` |
|        - | 4811 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - | 4812 | ` */` |
|   679352 | 4813 | `case PH7_OP_POP_EXCEPTION: {` |
|  1358709 | 4814 | `	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;` |
|        - | 4815 | `	VmFrame *pBodyFrame;` |
|        - | 4816 | `	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on` |
|        - | 4817 | `	 * top (matched by compiled origin). pException == NULL means this try's` |
|        - | 4818 | `	 * activation was already consumed (an in-place catch handled a throw and` |
|        - | 4819 | `	 * ran the finally itself); compiled fields keep coming from p3. */` |
|  1358709 | 4820 | `	ph7_exception *pException = 0;` |
|  1358709 | 4821 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|     3691 | 4822 | `		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|     3691 | 4823 | `		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];` |
|        - | 4824 | `		/* Same compiled origin is NOT enough: under recursion, when THIS` |
|        - | 4825 | `		 * level's activation was already consumed by an in-place catch (the` |
|        - | 4826 | `		 * resume lands right here), the top can be an OUTER level's activation` |
|        - | 4827 | `		 * of the same lexical try — popping it would run that level's finally` |
|        - | 4828 | `		 * early and orphan its handler (probed: recursive try/catch/finally` |
|        - | 4829 | `		 * lost the outer catch entirely). The activation must also belong to` |
|        - | 4830 | `		 * the CURRENT body frame. */` |
|     3686 | 4831 | `		if( VmExcMatches(pTop,pCompiledExc)` |
|     3643 | 4832 | `		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){` |
|     3589 | 4833 | `			pException = pTop;` |
|     3589 | 4834 | `			(void)SySetPop(&pVm->aException);` |
|     1792 | 4835 | `		}` |
|     1843 | 4836 | `	}` |
|  1358709 | 4837 | `	if( pCompiledExc->iInlined ){` |
|        - | 4838 | `		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.` |
|        - | 4839 | `		 * Pop this try's handler off aException so a throw in the finally propagates to` |
|        - | 4840 | `		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the` |
|        - | 4841 | `		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the` |
|        - | 4842 | `		 * compiler-emitted JMP falls into the finally. Without a finally, this is the` |
|        - | 4843 | `		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */` |
|      167 | 4844 | `		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */` |
|      167 | 4845 | `		if( pCompiledExc->iHasFinally ){` |
|        - | 4846 | `			VmFinallyAction sAct;` |
|       15 | 4847 | `			SyZero(&sAct,sizeof(sAct));` |
|       15 | 4848 | `			sAct.eKind = PH7_FA_FALLTHROUGH;` |
|       15 | 4849 | `			sAct.iNextPc = pCompiledExc->iEndCatchPc;` |
|       15 | 4850 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        - | 4851 | `			/* keep the transparent frame for OP_END_FINALLY */` |
|      161 | 4852 | `		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       20 | 4853 | `			VmLeaveFrame(&(*pVm));` |
|        8 | 4854 | `		}` |
|      167 | 4855 | `		break;` |
|        - | 4856 | `	}` |
|        - | 4857 | `	/* Leave the exception frame. It is normally on top here (a try that fell through` |
|        - | 4858 | `	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a` |
|        - | 4859 | `	 * RESUMED generator/fiber body whose yield was inside this try, that exception` |
|        - | 4860 | `	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),` |
|        - | 4861 | `	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the` |
|        - | 4862 | `	 * coroutine (and defeat the bHasRet materialization just below, which must see the` |
|        - | 4863 | `	 * body). Only leave a genuine exception frame. */` |
|  1358547 | 4864 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|  1151303 | 4865 | `		VmLeaveFrame(&(*pVm));` |
|   575649 | 4866 | `	}` |
|        - | 4867 | `	/* Execute the finally block if present and not already executed by the` |
|        - | 4868 | `	 * catch path. No live activation (pException == NULL) means an in-place` |
|        - | 4869 | `	 * catch consumed it — and that path runs the finally itself — so skip. */` |
|  1358547 | 4870 | `	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){` |
|        - | 4871 | `		sxi32 rcFinally;` |
|       57 | 4872 | `		VmExcRelease(&(*pVm),pException);` |
|       57 | 4873 | `		pException = 0;` |
|       57 | 4874 | `		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);` |
|       57 | 4875 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 | 4876 | `			goto Abort;` |
|        - | 4877 | `		}` |
|       57 | 4878 | `		if( rcFinally == PH7_EXCEPTION ){` |
|        - | 4879 | `			/* The finally threw past itself. If an enclosing try IN THIS function` |
|        - | 4880 | `			 * caught the new exception in place, resume at its landing pad;` |
|        - | 4881 | `			 * otherwise it was caught at an outer frame, so unwind this function. */` |
|        - | 4882 | `			sxi32 iResumePc;` |
|        5 | 4883 | `			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 4884 | `				pc = iResumePc;` |
|        3 | 4885 | `				break;` |
|        - | 4886 | `			}` |
|        3 | 4887 | `			goto Exception;` |
|        - | 4888 | `		}` |
|       24 | 4889 | `	}` |
|  1358543 | 4890 | `	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */` |
|  1358543 | 4891 | `	pBodyFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  1358543 | 4892 | `	if( pBodyFrame->bHasRet ){` |
|        - | 4893 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - | 4894 | `		 * function. The return targets the body frame this try belongs to. Drain` |
|        - | 4895 | `		 * outer finally blocks first, then — only in the real function body` |
|        - | 4896 | `		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline` |
|        - | 4897 | `		 * try within a catch/finally) propagate outward so the owning body returns. */` |
|    20257 | 4898 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    20257 | 4899 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4900 | `			goto Abort;` |
|        - | 4901 | `		}` |
|    20257 | 4902 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 4903 | `			goto Exception;` |
|        - | 4904 | `		}` |
|    20257 | 4905 | `		if( !sState.bReturnPropagates ){` |
|    20251 | 4906 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|    10123 | 4907 | `		}` |
|    20257 | 4908 | `		goto Done;` |
|        - | 4909 | `	}` |
|  1338291 | 4910 | `	if( pBodyFrame->nCatchJmpPc > 0 ){` |
|        - | 4911 | ``		/* A `break`/`continue` inside the catch (OP_CATCH_JMP) parked its loop-exit`` |
|        - | 4912 | `		 * target on this body frame, and this is a landing pad on its way out. */` |
|       87 | 4913 | `		if( pBodyFrame->nCatchJmpLevels > 1 ){` |
|        - | 4914 | `			/* Still one or more detached bodies out from the target's array — this try` |
|        - | 4915 | `			 * was declared INSIDE another catch/finally mini-program. End this one and` |
|        - | 4916 | `			 * let the park travel outward; the next landing pad decrements again. */` |
|        6 | 4917 | `			pBodyFrame->nCatchJmpLevels--;` |
|        6 | 4918 | `			goto Done;` |
|        - | 4919 | `		}` |
|       83 | 4920 | `		if( pBodyFrame->nCatchJmpCross > 0 ){` |
|        - | 4921 | `			/* Enclosing trys between the catch and the loop: the jump lands past their` |
|        - | 4922 | `			 * OP_POP_EXCEPTION, so run their finally (and leave their frames) here. */` |
|        8 | 4923 | `			sxu32 nCross = pBodyFrame->nCatchJmpCross;` |
|        8 | 4924 | `			pBodyFrame->nCatchJmpCross = 0;` |
|        8 | 4925 | `			rc = VmDrainCrossedTrys(&(*pVm),nCross,sState.nExceptionBase);` |
|        8 | 4926 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4927 | `				goto Abort;` |
|        - | 4928 | `			}` |
|        8 | 4929 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 4930 | `				/* A drained finally threw past itself — it supersedes the loop exit. */` |
|      ! 0 | 4931 | `				pBodyFrame->nCatchJmpPc = 0;` |
|      ! 0 | 4932 | `				goto Exception;` |
|        - | 4933 | `			}` |
|        3 | 4934 | `		}` |
|       83 | 4935 | `		pc = (sxi32)pBodyFrame->nCatchJmpPc - 1;` |
|       83 | 4936 | `		pBodyFrame->nCatchJmpPc = 0;` |
|       83 | 4937 | `		break;` |
|        - | 4938 | `	}` |
|  1338207 | 4939 | `	break;` |
|        - | 4940 | `							}` |
|        - | 4941 | `/*` |
|        - | 4942 | ` * OP_CATCH_JMP P1(levels,cross) P2(target pc) *` |
|        - | 4943 | ` * Jump to iP2, leaving the try/catch structures iP1 describes (PH7_CATCH_JMP_P1).` |
|        - | 4944 | ` *` |
|        - | 4945 | ` * CROSS enclosing trys are being left without reaching their OP_POP_EXCEPTION, so this` |
|        - | 4946 | ` * runs their finallys itself. LEVELS says whether the target is even addressable from` |
|        - | 4947 | `` * here: 0 means it is in this same array (a `goto` out of a try body) and the jump is`` |
|        - | 4948 | ` * taken on the spot; above 0 the jump starts inside a DETACHED catch/finally` |
|        - | 4949 | ` * mini-program whose array is not the target's, so the target is PARKED on the owning` |
|        - | 4950 | `` * body's frame and this mini-program ends — mirroring how an explicit `return` in a`` |
|        - | 4951 | ` * catch parks on sRet at OP_DONE above. VmThrowException then runs this try's finally` |
|        - | 4952 | ` * and returns; the resume lands on the try's OP_POP_EXCEPTION, which decrements LEVELS` |
|        - | 4953 | ` * and either re-parks (another mini-program out) or takes the jump.` |
|        - | 4954 | ` */` |
|       47 | 4955 | `case PH7_OP_CATCH_JMP: {` |
|        - | 4956 | `	VmFrame *pTgt;` |
|       97 | 4957 | `	if( PH7_CATCH_JMP_LEVELS(pInstr->iP1) == 0 ){` |
|        - | 4958 | ``		/* No detached body to leave — a `goto` whose target is in THIS array, but which`` |
|        - | 4959 | `		 * jumps out of one or more enclosing trys. Nothing to park: run their finallys` |
|        - | 4960 | `		 * (a bare jump would leave them to fire at teardown) and go. */` |
|       15 | 4961 | `		rc = VmDrainCrossedTrys(&(*pVm),PH7_CATCH_JMP_CROSS(pInstr->iP1),sState.nExceptionBase);` |
|       15 | 4962 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4963 | `			goto Abort;` |
|        - | 4964 | `		}` |
|       15 | 4965 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 4966 | `			/* A drained finally threw past itself — it supersedes the jump. */` |
|      ! 0 | 4967 | `			goto Exception;` |
|        - | 4968 | `		}` |
|       15 | 4969 | `		pc = (sxi32)pInstr->iP2 - 1;` |
|       15 | 4970 | `		break;` |
|        - | 4971 | `	}` |
|       85 | 4972 | `	pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|       85 | 4973 | `	pTgt->nCatchJmpPc = (sxu32)pInstr->iP2;` |
|       85 | 4974 | `	pTgt->nCatchJmpLevels = PH7_CATCH_JMP_LEVELS(pInstr->iP1);` |
|       85 | 4975 | `	pTgt->nCatchJmpCross = PH7_CATCH_JMP_CROSS(pInstr->iP1);` |
|        - | 4976 | `	/* A try opened INSIDE this catch body that the jump crosses had its finally run by` |
|        - | 4977 | `	 * the compiler-emitted OP_POP_EXCEPTION; drain anything still pending here. */` |
|       85 | 4978 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|       85 | 4979 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4980 | `		goto Abort;` |
|        - | 4981 | `	}` |
|       85 | 4982 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 4983 | `		/* A drained finally threw past itself — it discards this jump. */` |
|      ! 0 | 4984 | `		pTgt->nCatchJmpPc = 0;` |
|      ! 0 | 4985 | `		goto Exception;` |
|        - | 4986 | `	}` |
|       85 | 4987 | `	goto Done;` |
|        - | 4988 | `					   }` |
|        - | 4989 | `/*` |
|        - | 4990 | ` * OP_CATCH iP1(catch-index) * P3(ph7_exception)` |
|        - | 4991 | ` * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on` |
|        - | 4992 | ` * pException->pInflight by VmThrowInline) into the catch variable, resolved in the` |
|        - | 4993 | ` * enclosing body's scope (PHP: a catch shares the surrounding variable scope).` |
|        - | 4994 | ` */` |
|       38 | 4995 | `case PH7_OP_CATCH: {` |
|        - | 4996 | `	VmOpRc rcOp;` |
|       81 | 4997 | `	sState.pTos = pTos;` |
|       81 | 4998 | `	sState.pc = pc;` |
|       81 | 4999 | `	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);` |
|       81 | 5000 | `	pTos = sState.pTos;` |
|       81 | 5001 | `	pc = sState.pc;` |
|       81 | 5002 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5003 | `		goto Abort;` |
|       81 | 5004 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5005 | `		goto Exception;` |
|        - | 5006 | `	}` |
|       81 | 5007 | `	break;` |
|        - | 5008 | `					  }` |
|        - | 5009 | `/*` |
|        - | 5010 | ` * OP_END_FINALLY * P3(ph7_exception)` |
|        - | 5011 | ` * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch` |
|        - | 5012 | ` * the pending action queued when the finally was entered (fall-through / re-throw /` |
|        - | 5013 | ` * return / break-continue). A return/break threads out through each enclosing finally` |
|        - | 5014 | ` * via pException->iNextFinallyPc.` |
|        - | 5015 | ` */` |
|       24 | 5016 | `case PH7_OP_END_FINALLY: {` |
|       52 | 5017 | `	ph7_exception *pExc = (ph7_exception *)pInstr->p3;` |
|        - | 5018 | `	VmFinallyAction sAct;` |
|       52 | 5019 | `	int eKind = PH7_FA_FALLTHROUGH;` |
|        - | 5020 | `	/* Leave the try's transparent frame kept alive across the finally. */` |
|       52 | 5021 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        3 | 5022 | `		VmLeaveFrame(&(*pVm));` |
|        1 | 5023 | `	}` |
|       52 | 5024 | `	if( SySetUsed(&pVm->aFinallyAction) > 0 ){` |
|       52 | 5025 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);` |
|       52 | 5026 | `		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];` |
|       52 | 5027 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|       52 | 5028 | `		eKind = sAct.eKind;` |
|       28 | 5029 | `	}else{` |
|      ! 0 | 5030 | `		SyZero(&sAct,sizeof(sAct));` |
|      ! 0 | 5031 | `		sAct.iNextPc = pExc->iEndCatchPc;` |
|        - | 5032 | `	}` |
|       52 | 5033 | `	if( eKind == PH7_FA_FALLTHROUGH ){` |
|       13 | 5034 | `		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;` |
|       17 | 5035 | `		break;` |
|       42 | 5036 | `	}else if( eKind == PH7_FA_JMP ){` |
|        - | 5037 | `		/* break/continue: run the remaining crossed finallys, then take the jump. */` |
|        5 | 5038 | `		sxu32 iFpc = 0;` |
|        5 | 5039 | `		int nCross = sAct.nCross;` |
|        5 | 5040 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|      ! 0 | 5041 | `			sAct.nCross = nCross;` |
|      ! 0 | 5042 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      ! 0 | 5043 | `			pc = (sxi32)iFpc - 1;` |
|      ! 0 | 5044 | `			break;` |
|        - | 5045 | `		}` |
|        5 | 5046 | `		pc = (sxi32)sAct.iNextPc - 1;` |
|        5 | 5047 | `		break;` |
|       38 | 5048 | `	}else if( eKind == PH7_FA_RETHROW ){` |
|        8 | 5049 | `		ph7_class_instance *pRe = sAct.pExc;` |
|        - | 5050 | `		sxi32 _iRpE;` |
|        8 | 5051 | `		rc = VmThrowException(&(*pVm),pRe);` |
|        8 | 5052 | `		if( pRe ){ PH7_ClassInstanceUnref(pRe); }` |
|        8 | 5053 | `		if( rc == SXERR_ABORT ){ goto Abort; }` |
|        8 | 5054 | `		PH7_INLINE_RESUME_BREAK()` |
|        8 | 5055 | `		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }` |
|        8 | 5056 | `		goto Exception;` |
|      ! 0 | 5057 | `	}else{ /* PH7_FA_RETURN */` |
|       31 | 5058 | `		sxu32 iFpc = 0;` |
|       31 | 5059 | `		int nCross = sAct.nCross;` |
|       31 | 5060 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        - | 5061 | `			/* Thread the return through the next enclosing finally. */` |
|        6 | 5062 | `			sAct.nCross = nCross;` |
|        6 | 5063 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        6 | 5064 | `			pc = (sxi32)iFpc - 1;` |
|        6 | 5065 | `			break;` |
|        - | 5066 | `		}` |
|        - | 5067 | `		/* No enclosing finally left: materialize the return from this body. */` |
|       27 | 5068 | `		if( sAct.bHasRetVal && sState.pResult ){` |
|        6 | 5069 | `			PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|        2 | 5070 | `		}` |
|       27 | 5071 | `		PH7_MemObjRelease(&sAct.sRet);` |
|       27 | 5072 | `		goto Done;` |
|        - | 5073 | `	}` |
|        - | 5074 | `						 }` |
|        - | 5075 | `/*` |
|        - | 5076 | ` * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)` |
|        - | 5077 | `` * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a`` |
|        - | 5078 | ` * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);` |
|        - | 5079 | ` * OP_END_FINALLY threads it out through the finally chain and then returns.` |
|        - | 5080 | ` */` |
|       22 | 5081 | `case PH7_OP_SET_FINALLY_RET: {` |
|        - | 5082 | `	VmFinallyAction sAct;` |
|       49 | 5083 | `	sxu32 iFpc = 0;` |
|       49 | 5084 | `	int nCross = -1; /* a return crosses every enclosing finally in this function */` |
|       49 | 5085 | `	SyZero(&sAct,sizeof(sAct));` |
|       49 | 5086 | `	sAct.eKind = PH7_FA_RETURN;` |
|       49 | 5087 | `	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       49 | 5088 | `	PH7_MemObjInit(pVm,&sAct.sRet);` |
|       49 | 5089 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|       37 | 5090 | `		PH7_MemObjStore(pTos,&sAct.sRet);` |
|       37 | 5091 | `		sAct.bHasRetVal = 1;` |
|       37 | 5092 | `		VmPopOperand(&pTos,1);` |
|       16 | 5093 | `	}` |
|       49 | 5094 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        9 | 5095 | `		sAct.nCross = nCross;` |
|        9 | 5096 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        9 | 5097 | `		pc = (sxi32)iFpc - 1;` |
|        9 | 5098 | `		break;` |
|        - | 5099 | `	}` |
|        - | 5100 | `	/* No enclosing finally left: return now. */` |
|       43 | 5101 | `	if( sAct.bHasRetVal && sState.pResult ){` |
|       33 | 5102 | `		PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|       14 | 5103 | `	}` |
|       43 | 5104 | `	PH7_MemObjRelease(&sAct.sRet);` |
|       43 | 5105 | `	goto Done;` |
|        - | 5106 | `						 }` |
|        - | 5107 | `/*` |
|        - | 5108 | ` * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)` |
|        - | 5109 | `` * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action`` |
|        - | 5110 | ` * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.` |
|        - | 5111 | ` */` |
|        4 | 5112 | `case PH7_OP_SET_FINALLY_JMP: {` |
|        - | 5113 | `	VmFinallyAction sAct;` |
|       10 | 5114 | `	sxu32 iFpc = 0;` |
|       10 | 5115 | `	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */` |
|       10 | 5116 | `	SyZero(&sAct,sizeof(sAct));` |
|       10 | 5117 | `	sAct.eKind = PH7_FA_JMP;` |
|       10 | 5118 | `	sAct.iNextPc = pInstr->iP2;` |
|       10 | 5119 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        5 | 5120 | `		sAct.nCross = nCross;` |
|        5 | 5121 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        5 | 5122 | `		pc = (sxi32)iFpc - 1;` |
|        5 | 5123 | `		break;` |
|        - | 5124 | `	}` |
|        - | 5125 | `	/* No finally among the crossed trys: just take the break/continue jump. */` |
|        6 | 5126 | `	pc = (sxi32)sAct.iNextPc - 1;` |
|        6 | 5127 | `	break;` |
|        - | 5128 | `						 }` |
|        - | 5129 | `/*` |
|        - | 5130 | ` * OP_THROW * P2 *` |
|        - | 5131 | ` * Throw an user exception.` |
|        - | 5132 | ` */` |
|   500423 | 5133 | `case PH7_OP_THROW: {` |
|        - | 5134 | `	VmOpRc rcOp;` |
|  1000851 | 5135 | `	sState.pTos = pTos;` |
|  1000851 | 5136 | `	sState.pc = pc;` |
|  1000851 | 5137 | `	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);` |
|  1000851 | 5138 | `	pTos = sState.pTos;` |
|  1000851 | 5139 | `	pc = sState.pc;` |
|  1000851 | 5140 | `	if( rcOp == VM_OP_ABORT ){` |
|       39 | 5141 | `		goto Abort;` |
|  1000817 | 5142 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|   600371 | 5143 | `		goto Exception;` |
|        - | 5144 | `	}` |
|   400451 | 5145 | `	break;` |
|        - | 5146 | `					  }` |
|        - | 5147 | `/*` |
|        - | 5148 | ` * OP_FOREACH_INIT * P2 P3` |
|        - | 5149 | ` * Prepare a foreach step.` |
|        - | 5150 | ` */` |
|    15256 | 5151 | `case PH7_OP_FOREACH_INIT: {` |
|        - | 5152 | `	VmOpRc rcOp;` |
|    30517 | 5153 | `	sState.pTos = pTos;` |
|    30517 | 5154 | `	sState.pc = pc;` |
|    30517 | 5155 | `	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);` |
|    30517 | 5156 | `	pTos = sState.pTos;` |
|    30517 | 5157 | `	pc = sState.pc;` |
|    30517 | 5158 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5159 | `		goto Abort;` |
|    30517 | 5160 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 5161 | `		goto Exception;` |
|        - | 5162 | `	}` |
|    30513 | 5163 | `	break;` |
|        - | 5164 | `					  }` |
|        - | 5165 | `/*` |
|        - | 5166 | ` * OP_FOREACH_STEP * P2 P3` |
|        - | 5167 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - | 5168 | ` */` |
|   185543 | 5169 | `case PH7_OP_FOREACH_STEP: {` |
|        - | 5170 | `	VmOpRc rcOp;` |
|   371087 | 5171 | `	sState.pTos = pTos;` |
|   371087 | 5172 | `	sState.pc = pc;` |
|   371087 | 5173 | `	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);` |
|   371087 | 5174 | `	pTos = sState.pTos;` |
|   371087 | 5175 | `	pc = sState.pc;` |
|   371087 | 5176 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 5177 | `		goto Abort;` |
|   371085 | 5178 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5179 | `		goto Exception;` |
|        - | 5180 | `	}` |
|   371085 | 5181 | `	break;` |
|        - | 5182 | `						  }` |
|        - | 5183 | `/*` |
|        - | 5184 | ` * OP_MEMBER P1 P2` |
|        - | 5185 | ` * Load class attribute/method on the stack.` |
|        - | 5186 | ` */` |
|   162953 | 5187 | `case PH7_OP_MEMBER: {` |
|        - | 5188 | `	VmOpRc rcOp;` |
|   325914 | 5189 | `	sState.pTos = pTos;` |
|   325914 | 5190 | `	sState.pc = pc;` |
|   325914 | 5191 | `	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);` |
|   325914 | 5192 | `	pTos = sState.pTos;` |
|   325914 | 5193 | `	pc = sState.pc;` |
|   325914 | 5194 | `	if( rcOp == VM_OP_ABORT ){` |
|        8 | 5195 | `		goto Abort;` |
|   325908 | 5196 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      165 | 5197 | `		goto Exception;` |
|        - | 5198 | `	}` |
|   325746 | 5199 | `	break;` |
|        - | 5200 | `					  }` |
|        - | 5201 | `/*` |
|        - | 5202 | ` * OP_NEW P1 * * *` |
|        - | 5203 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - | 5204 | ` */` |
|  1056834 | 5205 | `case PH7_OP_NEW: {` |
|        - | 5206 | `	VmOpRc rcOp;` |
|  2113673 | 5207 | `	sState.pTos = pTos;` |
|  2113673 | 5208 | `	sState.pc = pc;` |
|  2113673 | 5209 | `	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);` |
|  2113673 | 5210 | `	pTos = sState.pTos;` |
|  2113673 | 5211 | `	pc = sState.pc;` |
|  2113673 | 5212 | `	if( rcOp == VM_OP_ABORT ){` |
|        5 | 5213 | `		goto Abort;` |
|  2113669 | 5214 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      140 | 5215 | `		goto Exception;` |
|        - | 5216 | `	}` |
|  2113531 | 5217 | `	break;` |
|        - | 5218 | `					  }` |
|        - | 5219 | `/*` |
|        - | 5220 | ` * OP_CLONE * * *` |
|        - | 5221 | ` * Perfome a clone operation.` |
|        - | 5222 | ` */` |
|      122 | 5223 | `case PH7_OP_CLONE: {` |
|        - | 5224 | `	VmOpRc rcOp;` |
|      249 | 5225 | `	sState.pTos = pTos;` |
|      249 | 5226 | `	sState.pc = pc;` |
|      249 | 5227 | `	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);` |
|      249 | 5228 | `	pTos = sState.pTos;` |
|      249 | 5229 | `	pc = sState.pc;` |
|      249 | 5230 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5231 | `		goto Abort;` |
|      249 | 5232 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       19 | 5233 | `		goto Exception;` |
|        - | 5234 | `	}` |
|      231 | 5235 | `	break;` |
|        - | 5236 | `					  }` |
|        - | 5237 | `/*` |
|        - | 5238 | ` * OP_SWITCH * * P3` |
|        - | 5239 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - | 5240 | ` */` |
|       42 | 5241 | `case PH7_OP_SWITCH: {` |
|        - | 5242 | `	VmOpRc rcOp;` |
|       89 | 5243 | `	sState.pTos = pTos;` |
|       89 | 5244 | `	sState.pc = pc;` |
|       89 | 5245 | `	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);` |
|       89 | 5246 | `	pTos = sState.pTos;` |
|       89 | 5247 | `	pc = sState.pc;` |
|       89 | 5248 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5249 | `		goto Abort;` |
|       89 | 5250 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5251 | `		goto Exception;` |
|        - | 5252 | `	}` |
|       89 | 5253 | `	break;` |
|        - | 5254 | `					  }` |
|        - | 5255 | `/*` |
|        - | 5256 | ` * OP_MATCH * * P3` |
|        - | 5257 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - | 5258 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - | 5259 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - | 5260 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - | 5261 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - | 5262 | ` */` |
|       78 | 5263 | `case PH7_OP_MATCH: {` |
|        - | 5264 | `	VmOpRc rcOp;` |
|      160 | 5265 | `	sState.pTos = pTos;` |
|      160 | 5266 | `	sState.pc = pc;` |
|      160 | 5267 | `	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);` |
|      160 | 5268 | `	pTos = sState.pTos;` |
|      160 | 5269 | `	pc = sState.pc;` |
|      160 | 5270 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5271 | `		goto Abort;` |
|      160 | 5272 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        6 | 5273 | `		goto Exception;` |
|        - | 5274 | `	}` |
|      156 | 5275 | `	break;` |
|        - | 5276 | `					  }` |
|        - | 5277 | `/*` |
|        - | 5278 | ` * OP_YIELD P1 P2 *` |
|        - | 5279 | ` *  Yield a value from a generator function.` |
|        - | 5280 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - | 5281 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - | 5282 | ` */` |
|      595 | 5283 | `case PH7_OP_YIELD: {` |
|        - | 5284 | `	ph7_generator *pGen;` |
|     1195 | 5285 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5286 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 | 5287 | `		goto Abort;` |
|        - | 5288 | `	}` |
|     1195 | 5289 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5290 | ``		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's`` |
|        - | 5291 | `		 * pending finallys tried to yield — PHP forbids it. */` |
|      ! 0 | 5292 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5293 | `			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5294 | `			goto Abort;` |
|        - | 5295 | `		}` |
|      ! 0 | 5296 | `		goto Exception;` |
|        - | 5297 | `	}` |
|     1195 | 5298 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|     1195 | 5299 | `	if( pInstr->iP2 ){` |
|        - | 5300 | `		/* yield $key => $value: value on top, key below */` |
|        - | 5301 | `#ifdef UNTRUST` |
|        - | 5302 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - | 5303 | `#endif` |
|       20 | 5304 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       20 | 5305 | `		VmPopOperand(&pTos, 1);` |
|       20 | 5306 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       20 | 5307 | `		VmPopOperand(&pTos, 1);` |
|        - | 5308 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       20 | 5309 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 | 5310 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 | 5311 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 | 5312 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 | 5313 | `			}` |
|        2 | 5314 | `		}` |
|     1186 | 5315 | `	}else if( pInstr->iP1 ){` |
|        - | 5316 | `		/* yield $value */` |
|        - | 5317 | `#ifdef UNTRUST` |
|        - | 5318 | `		if( pTos < pStack ) goto Abort;` |
|        - | 5319 | `#endif` |
|     1177 | 5320 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|     1177 | 5321 | `		VmPopOperand(&pTos, 1);` |
|        - | 5322 | `		/* Auto-increment key */` |
|     1177 | 5323 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|     1177 | 5324 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|     1177 | 5325 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|      591 | 5326 | `	}else{` |
|        - | 5327 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 | 5328 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 5329 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 5330 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 | 5331 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - | 5332 | `	}` |
|        - | 5333 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|     1195 | 5334 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|     1195 | 5335 | `	goto Suspend;` |
|        - | 5336 | `}` |
|        - | 5337 | `/*` |
|        - | 5338 | ` * OP_YIELD_FROM * * *` |
|        - | 5339 | ` *` |
|        - | 5340 | `` * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an`` |
|        - | 5341 | ` * array/Traversable/Generator from the OUTER generator, preserving the inner` |
|        - | 5342 | ` * keys; the expression evaluates to the inner Generator's return value (or NULL).` |
|        - | 5343 | ` *` |
|        - | 5344 | ` * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,` |
|        - | 5345 | ` * advances the per-instance delegate cursor stored on the exec context (never the` |
|        - | 5346 | ` * shared foreach aStep, so independent generator instances cannot clash). The` |
|        - | 5347 | ` * iterable operand is consumed on first entry; the expression result is pushed at` |
|        - | 5348 | ` * exhaustion — net stack effect +1, identical to OP_YIELD.` |
|        - | 5349 | ` */` |
|       93 | 5350 | `case PH7_OP_YIELD_FROM: {` |
|        - | 5351 | `	ph7_generator *pGenFrom;` |
|        - | 5352 | `	ph7_exec_ctx *pCtxFrom;` |
|        - | 5353 | `	ph7_value sKey,sVal;` |
|      191 | 5354 | `	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */` |
|      191 | 5355 | `	int bExhausted = 0;` |
|      191 | 5356 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5357 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");` |
|      ! 0 | 5358 | `		goto Abort;` |
|        - | 5359 | `	}` |
|      191 | 5360 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5361 | ``		/* A `yield from` reached while VmCloseCtx force-drives this destroyed`` |
|        - | 5362 | `		 * generator's pending finallys — PHP forbids it (distinct message from a` |
|        - | 5363 | ``		 * bare `yield`, mirroring the OP_YIELD guard above). */`` |
|      ! 0 | 5364 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5365 | `			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5366 | `			goto Abort;` |
|        - | 5367 | `		}` |
|      ! 0 | 5368 | `		goto Exception;` |
|        - | 5369 | `	}` |
|      191 | 5370 | `	pCtxFrom = pVm->pActiveCtx;` |
|      191 | 5371 | `	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;` |
|      191 | 5372 | `	PH7_MemObjInit(pVm,&sKey);` |
|      191 | 5373 | `	PH7_MemObjInit(pVm,&sVal);` |
|      191 | 5374 | `	if( pCtxFrom->iDelegateState == 0 ){` |
|        - | 5375 | `		/* First entry: classify the iterable on the stack top. */` |
|       79 | 5376 | `		int bIterable = 1;` |
|        - | 5377 | `#ifdef UNTRUST` |
|        - | 5378 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 5379 | `#endif` |
|       79 | 5380 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       29 | 5381 | `			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       29 | 5382 | `			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;` |
|       29 | 5383 | `			pCtxFrom->iDelegateState = 1;` |
|       67 | 5384 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       51 | 5385 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       51 | 5386 | `			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       51 | 5387 | `			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){` |
|       41 | 5388 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       41 | 5389 | `				pCtxFrom->iDelegateState = 3;` |
|       31 | 5390 | `			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){` |
|        9 | 5391 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        9 | 5392 | `				pCtxFrom->iDelegateState = 2;` |
|        6 | 5393 | `			}else{` |
|        5 | 5394 | `				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - | 5395 | `					sizeof("IteratorAggregate")-1,FALSE,0);` |
|        7 | 5396 | `				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){` |
|        - | 5397 | `					/* Delegate to the Iterator returned by getIterator() */` |
|        - | 5398 | `					ph7_value sIt;` |
|        5 | 5399 | `					PH7_MemObjInit(pVm,&sIt);` |
|        5 | 5400 | `					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);` |
|        5 | 5401 | `					if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){` |
|        - | 5402 | `						/* getIterator() threw/aborted: drop it, consume the` |
|        - | 5403 | `						 * operand, and propagate. */` |
|      ! 0 | 5404 | `						PH7_MemObjRelease(&sIt);` |
|      ! 0 | 5405 | `						VmPopOperand(&pTos,1);` |
|      ! 0 | 5406 | `						goto yf_propagate;` |
|        - | 5407 | `					}` |
|        4 | 5408 | `					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls` |
|        5 | 5409 | `						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){` |
|        5 | 5410 | `						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);` |
|        5 | 5411 | `						pCtxFrom->iDelegateState = 2;` |
|        3 | 5412 | `					}else{` |
|      ! 0 | 5413 | `						bIterable = 0;` |
|        - | 5414 | `					}` |
|        5 | 5415 | `					PH7_MemObjRelease(&sIt);` |
|        3 | 5416 | `				}else{` |
|      ! 0 | 5417 | `					bIterable = 0;` |
|        - | 5418 | `				}` |
|        - | 5419 | `			}` |
|       28 | 5420 | `		}else{` |
|        6 | 5421 | `			bIterable = 0;` |
|        - | 5422 | `		}` |
|       79 | 5423 | `		VmPopOperand(&pTos,1); /* Consume the iterable operand */` |
|       79 | 5424 | `		if( !bIterable ){` |
|        - | 5425 | `			/* Non-iterable source: throw a catchable Error (PHP 8.5), then` |
|        - | 5426 | `			 * funnel through the shared teardown/route path. */` |
|        6 | 5427 | `			rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 5428 | `				"Can use \"yield from\" only with arrays and Traversables",` |
|        - | 5429 | `				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);` |
|        6 | 5430 | `			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        6 | 5431 | `			goto yf_propagate;` |
|        - | 5432 | `		}` |
|       75 | 5433 | `		if( pCtxFrom->iDelegateState >= 2 ){` |
|        - | 5434 | `			/* rewind() the delegate (also starts a fresh generator) */` |
|       51 | 5435 | `			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 5436 | `				"rewind",sizeof("rewind")-1,0);` |
|       51 | 5437 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       23 | 5438 | `		}` |
|       40 | 5439 | `	}else{` |
|        - | 5440 | `		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a` |
|        - | 5441 | `		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()` |
|        - | 5442 | `		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)` |
|        - | 5443 | `		 * ignore send() and just advance with next(). */` |
|        - | 5444 | `#ifdef UNTRUST` |
|        - | 5445 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 5446 | `#endif` |
|      117 | 5447 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       67 | 5448 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|        - | 5449 | `			/* A pending Generator::throw() on the outer was parked on pInjected by the` |
|        - | 5450 | `			 * body-entry gate (which skips its own raise for state 3); forward it as a` |
|        - | 5451 | `			 * throw into the delegate, else forward the sent value (pTos, which` |
|        - | 5452 | `			 * VmResumeCtx copies into the inner's own stack). */` |
|       67 | 5453 | `			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;` |
|       67 | 5454 | `			pCtxFrom->pInjected = 0;` |
|       67 | 5455 | `			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       67 | 5456 | `				if( pInjFwd ){` |
|        - | 5457 | `					/* Borrowed ref: the outer's Generator::throw() holds it across this` |
|        - | 5458 | `					 * whole resume, so the inner inject path must not unref it. */` |
|        5 | 5459 | `					pInner->pCtx->pInjected = pInjFwd;` |
|        5 | 5460 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);` |
|        5 | 5461 | `					pInner->pCtx->pInjected = 0;` |
|        3 | 5462 | `				}else{` |
|       63 | 5463 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);` |
|        3 | 5464 | `				}` |
|       32 | 5465 | `			}else if( pInjFwd ){` |
|        - | 5466 | `				/* No live delegate to receive the throw (inner already finished/closed):` |
|        - | 5467 | `				 * raise it at the yield-from in the outer generator's own frame. */` |
|      ! 0 | 5468 | `				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);` |
|      ! 0 | 5469 | `				pTF->iFlags \|= VM_FRAME_THROW;` |
|      ! 0 | 5470 | `				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 5471 | `			}` |
|       67 | 5472 | `			PH7_MemObjRelease(pTos);` |
|       67 | 5473 | `			pTos--;` |
|       67 | 5474 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       32 | 5475 | `		}else{` |
|       53 | 5476 | `			PH7_MemObjRelease(pTos);` |
|       53 | 5477 | `			pTos--;` |
|       53 | 5478 | `			if( pCtxFrom->iDelegateState >= 2 ){` |
|       17 | 5479 | `				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 5480 | `					"next",sizeof("next")-1,0);` |
|       17 | 5481 | `				if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 5482 | `			}` |
|        - | 5483 | `		}` |
|        - | 5484 | `	}` |
|        - | 5485 | `	/* Fetch the current (key,value) of the delegate, or mark exhausted. */` |
|      177 | 5486 | `	if( pCtxFrom->iDelegateState == 1 ){` |
|       63 | 5487 | `		if( pCtxFrom->pDelegateNode == 0 ){` |
|       21 | 5488 | `			bExhausted = 1;` |
|       13 | 5489 | `		}else{` |
|       47 | 5490 | `			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);` |
|       47 | 5491 | `			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);` |
|        - | 5492 | `			/* Forward traversal follows pPrev (the hashmap's "reverse link",` |
|        - | 5493 | `			 * matching PH7_HashmapGetNextEntry). */` |
|       47 | 5494 | `			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;` |
|        - | 5495 | `		}` |
|       34 | 5496 | `	}else{` |
|      119 | 5497 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|        - | 5498 | `		ph7_value sValid;` |
|        - | 5499 | `		int isValid;` |
|      119 | 5500 | `		PH7_MemObjInit(pVm,&sValid);` |
|      119 | 5501 | `		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|      119 | 5502 | `		PH7_MemObjToBool(&sValid);` |
|      119 | 5503 | `		isValid = (sValid.x.iVal != 0);` |
|      119 | 5504 | `		PH7_MemObjRelease(&sValid);` |
|      119 | 5505 | `		if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      119 | 5506 | `		if( !isValid ){` |
|       28 | 5507 | `			bExhausted = 1;` |
|       16 | 5508 | `		}else{` |
|       95 | 5509 | `			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|       95 | 5510 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       95 | 5511 | `			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       95 | 5512 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        - | 5513 | `		}` |
|        - | 5514 | `	}` |
|      177 | 5515 | `	if( bExhausted ){` |
|        - | 5516 | `		/* Expression value: inner Generator's return value (state 3) or NULL. */` |
|        - | 5517 | `		ph7_value sResult;` |
|       45 | 5518 | `		PH7_MemObjInit(pVm,&sResult);` |
|       45 | 5519 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       23 | 5520 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|       23 | 5521 | `			if( pInner && pInner->pCtx ){` |
|       23 | 5522 | `				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);` |
|       10 | 5523 | `			}` |
|       10 | 5524 | `		}` |
|       45 | 5525 | `		PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       45 | 5526 | `		pCtxFrom->pDelegateNode = 0;` |
|       45 | 5527 | `		pCtxFrom->iDelegateState = 0;` |
|       45 | 5528 | `		pTos++;` |
|       45 | 5529 | `		PH7_MemObjStore(&sResult,pTos);` |
|       45 | 5530 | `		PH7_MemObjRelease(&sResult);` |
|       45 | 5531 | `		PH7_MemObjRelease(&sKey);` |
|       45 | 5532 | `		PH7_MemObjRelease(&sVal);` |
|       45 | 5533 | `		break; /* fall through to pc+1 with the result on the stack top */` |
|        - | 5534 | `	}` |
|        - | 5535 | `	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.` |
|        - | 5536 | `	 * The outer generator's implicit auto-key counter is NOT advanced by the` |
|        - | 5537 | ``	 * delegated keys — PHP keeps it independent across `yield from`, so a later`` |
|        - | 5538 | ``	 * plain `yield` continues from the outer's own counter. */`` |
|      137 | 5539 | `	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);` |
|      137 | 5540 | `	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);` |
|      137 | 5541 | `	PH7_MemObjRelease(&sKey);` |
|      137 | 5542 | `	PH7_MemObjRelease(&sVal);` |
|        - | 5543 | `	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */` |
|      137 | 5544 | `	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));` |
|      137 | 5545 | `	goto Suspend;` |
|        7 | 5546 | `yf_propagate:` |
|        - | 5547 | `	/* A delegate iterator method threw/aborted (or the source was non-iterable):` |
|        - | 5548 | `	 * tear down the delegation, then route via the shared dispatch macro. rcm is` |
|        - | 5549 | `	 * always PH7_EXCEPTION or PH7_ABORT here. */` |
|       17 | 5550 | `	PH7_MemObjRelease(&sKey);` |
|       17 | 5551 | `	PH7_MemObjRelease(&sVal);` |
|       17 | 5552 | `	PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       17 | 5553 | `	pCtxFrom->pDelegateNode = 0;` |
|       17 | 5554 | `	pCtxFrom->iDelegateState = 0;` |
|       17 | 5555 | `	PH7_DISPATCH_ENFORCE_RC(rcm)` |
|      ! 0 | 5556 | `	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */` |
|        - | 5557 | `}` |
|        - | 5558 | `/*` |
|        - | 5559 | ` * OP_CALL P1 * *` |
|        - | 5560 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - | 5561 | ` *  function on the stack.` |
|        - | 5562 | ` */` |
|        - | 5563 | `/*` |
|        - | 5564 | ` * OP_CALL_INIT * P2 *` |
|        - | 5565 | ` *  Screen the callee on TOS where it is WRITTEN — before this call's arguments run.` |
|        - | 5566 | ` *` |
|        - | 5567 | ` *  php resolves a call's target at INIT_FCALL / INIT_FCALL_BY_NAME / INIT_DYNAMIC_CALL` |
|        - | 5568 | `` *  and raises there, so `undefinedFn(s(1))`, `$f(s(1))` over a misspelled name and`` |
|        - | 5569 | `` *  `$v(s(1))` over an int all refuse BEFORE `s(1)` runs. PHL only ever looked at the`` |
|        - | 5570 | ` *  callee inside OP_CALL, one instruction after the whole argument list, so every one` |
|        - | 5571 | ` *  of those programs produced the argument's side effects (or its exception) first and` |
|        - | 5572 | ` *  php's Error second. The messages were already identical; only the order was not.` |
|        - | 5573 | ` *` |
|        - | 5574 | ` *  The verdict is the FIRST-CLASS-CALLABLE creation screen, unchanged and shared: php` |
|        - | 5575 | `` *  gives `f(...)` the direct call's taxonomy word for word, which makes VmFccValueError`` |
|        - | 5576 | ` *  the one builder for both. The value is left exactly as it is — OP_CALL still does its` |
|        - | 5577 | ` *  own resolution — so this adds a refusal and changes nothing that succeeds. P2 == 1` |
|        - | 5578 | ` *  when the compiler namespace-qualified the name, which is the one bit php's` |
|        - | 5579 | `` *  global-function fallback needs (an unqualified `strlen(...)` inside a namespace).`` |
|        - | 5580 | ` *` |
|        - | 5581 | ` *  Not emitted for a callee whose OP_MEMBER already screened it, for a first-class` |
|        - | 5582 | ` *  callable (OP_LOAD_FCC screens it, with nothing running in between), or for a call` |
|        - | 5583 | ` *  with no arguments at all — there the call IS the first thing that happens.` |
|        - | 5584 | ` */` |
|   729699 | 5585 | `case PH7_OP_CALL_INIT: {` |
|  1461492 | 5586 | `	if( (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_MAGICCALL)) == 0` |
|  1461497 | 5587 | `	 && !VmValueIsClosure(pVm,pTos) ){` |
|  1459847 | 5588 | `		const char *zInitCls = 0,*zInitMeth = 0;` |
|  1459847 | 5589 | `		sxu32 nInitCls = 0,nInitMeth = 0;` |
|        - | 5590 | `		char zInitMsg[192];` |
|  1459847 | 5591 | `		const char *zInitBad = 0;` |
|  1459847 | 5592 | `		SyString sInitName = { 0, 0 };` |
|  1459847 | 5593 | `		int bInitScoped = 0;` |
|  1459847 | 5594 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|  1459697 | 5595 | `			SyStringInitFromBuf(&sInitName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 5596 | `			/* A leading backslash only anchors the name to the global namespace. */` |
|  1459697 | 5597 | `			if( sInitName.nByte > 0 && sInitName.zString[0] == '\\' ){` |
|        3 | 5598 | `				sInitName.zString++;` |
|        3 | 5599 | `				sInitName.nByte--;` |
|        1 | 5600 | `			}` |
|  1459697 | 5601 | `			bInitScoped = PH7_VmCallableStringParts(sInitName.zString,sInitName.nByte,` |
|        - | 5602 | `				&zInitCls,&nInitCls,&zInitMeth,&nInitMeth);` |
|   730893 | 5603 | `		}` |
|  1459847 | 5604 | `		if( bInitScoped ){` |
|        - | 5605 | ``			/* A `"Class::method"` string carries its whole taxonomy in one builder — the`` |
|        - | 5606 | `			 * class, the missing/abstract/inaccessible cases and the catch-all routing —` |
|        - | 5607 | `			 * and answers 0 when the call WILL run. It is asked unconditionally because` |
|        - | 5608 | `			 * the predicate below is not the same question: is_callable() accepts a` |
|        - | 5609 | `			 * non-static method named through a class, which the direct call refuses. */` |
|       28 | 5610 | `			zInitBad = VmCallableClassMethodError(&(*pVm),` |
|        9 | 5611 | `				PH7_VmExtractClass(&(*pVm),zInitCls,nInitCls,FALSE,0),` |
|        9 | 5612 | `				zInitCls,nInitCls,zInitMeth,nInitMeth,TRUE,zInitMsg,sizeof(zInitMsg));` |
|  1459838 | 5613 | `		}else if( !PH7_VmIsCallable(&(*pVm),pTos,TRUE) ){` |
|       93 | 5614 | `			int bInitOk = 0;` |
|       93 | 5615 | `			if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 5616 | `				/* php's global fallback for an UNQUALIFIED name written inside a` |
|        - | 5617 | `				 * namespace: the current namespace first, the global one after. OP_CALL` |
|        - | 5618 | `				 * retries the same way from its argument map; this only has to agree` |
|        - | 5619 | `				 * about whether the call WILL resolve, so the shortened name is tested` |
|        - | 5620 | `				 * and thrown away. */` |
|       57 | 5621 | `				const char *zInitShort = sInitName.zString;` |
|        - | 5622 | `				sxu32 iInitPos;` |
|      857 | 5623 | `				for( iInitPos = 0 ; iInitPos < sInitName.nByte ; ++iInitPos ){` |
|      805 | 5624 | `					if( sInitName.zString[iInitPos] == '\\' ){` |
|       71 | 5625 | `						zInitShort = &sInitName.zString[iInitPos + 1];` |
|       33 | 5626 | `					}` |
|      405 | 5627 | `				}` |
|       57 | 5628 | `				if( zInitShort != sInitName.zString ){` |
|        - | 5629 | `					ph7_value sInitShort;` |
|       57 | 5630 | `					PH7_MemObjInit(pVm,&sInitShort);` |
|       83 | 5631 | `					PH7_MemObjStringAppend(&sInitShort,zInitShort,` |
|       52 | 5632 | `						(sxu32)(sInitName.nByte - (sxu32)(zInitShort - sInitName.zString)));` |
|       57 | 5633 | `					bInitOk = PH7_VmIsCallable(&(*pVm),&sInitShort,TRUE);` |
|       57 | 5634 | `					PH7_MemObjRelease(&sInitShort);` |
|       26 | 5635 | `				}` |
|       26 | 5636 | `			}` |
|        - | 5637 | `			/* The FIRST-CLASS-CALLABLE creation screen's builder, unchanged and shared:` |
|        - | 5638 | ``			 * php gives `f(...)` the direct call's taxonomy word for word. It assumes the`` |
|        - | 5639 | `			 * predicate has already declined — a pair a class answers through __call is` |
|        - | 5640 | `			 * callable and never arrives here — which is why it sits under that test. */` |
|       93 | 5641 | `			if( !bInitOk ){` |
|       38 | 5642 | `				zInitBad = VmFccValueError(&(*pVm),pTos,zInitMsg,sizeof(zInitMsg));` |
|       18 | 5643 | `			}` |
|       44 | 5644 | `		}` |
|  1459847 | 5645 | `		if( zInitBad ){` |
|        - | 5646 | `			sxi32 rcInit;` |
|       44 | 5647 | `			PH7_MemObjRelease(pTos);` |
|       44 | 5648 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       44 | 5649 | `			pTos->nIdx = SXU32_HIGH;` |
|       44 | 5650 | `			rcInit = VmThrowFromVm(&(*pVm),"Error",zInitBad,(sxu32)SyStrlen(zInitBad));` |
|       44 | 5651 | `			if( rcInit == SXERR_ABORT ){ goto Abort; }` |
|       44 | 5652 | `			rc = rcInit;` |
|       62 | 5653 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5654 | `		}` |
|   730947 | 5655 | `	}` |
|  1461455 | 5656 | `	break;` |
|        - | 5657 | `}` |
|        - | 5658 | `/*` |
|        - | 5659 | ` * OP_ROT_CALLEE P1 P2 *` |
|        - | 5660 | ` *  Turn a call's operand region over: [callee][arg0..argN] becomes [arg0..argN][callee],` |
|        - | 5661 | ` *  which is the layout OP_CALL's entire dispatch is written against.` |
|        - | 5662 | ` *` |
|        - | 5663 | ` *  The codegen pushes the callee FIRST because php resolves it where it is written —` |
|        - | 5664 | ` *  before a single argument runs — so an undefined or inaccessible method is refused` |
|        - | 5665 | `` *  ahead of the argument list's side effects, and a `?->` on null skips the arguments`` |
|        - | 5666 | ` *  altogether. Everything downstream of this instruction still sees the historical` |
|        - | 5667 | ` *  stack, so the reordering costs one memory move per call and nothing else.` |
|        - | 5668 | ` *` |
|        - | 5669 | ` *  P1 is the compile-time argument count; P2 carries PH7_ROT_SPREAD (this call unpacks,` |
|        - | 5670 | ` *  so the runtime count is P1 plus its OWN runs' net growth) and PH7_ROT_TWOSLOT (the` |
|        - | 5671 | ` *  callee is a method pair, [receiver][name]). A __call routing collapses that pair to` |
|        - | 5672 | ` *  one marked carrier at run time, which is read off the slot rather than guessed.` |
|        - | 5673 | ` */` |
|  1232846 | 5674 | `case PH7_OP_ROT_CALLEE: {` |
|  4935577 | 5675 | `	sxi32 nRotArgs = pInstr->iP1` |
|  2467786 | 5676 | `		+ ((pInstr->iP2 & PH7_ROT_SPREAD)` |
|        - | 5677 | `			/* One past the last argument is one past the TOP here: the callee sits` |
|        - | 5678 | `			 * BELOW the region, not above it as at OP_CALL. */` |
|  1233145 | 5679 | `			? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,&pTos[1]) : 0);` |
|  2467791 | 5680 | `	if( nRotArgs < 0 ){` |
|        - | 5681 | `		/* Unreachable: an empty unpack subtracts one per compile-time position, so the` |
|        - | 5682 | `		 * net can reach 0 and no lower. Clamped rather than trusted — reading above the` |
|        - | 5683 | `		 * top to find the callee is not a failure mode worth leaving open. */` |
|      ! 0 | 5684 | `		nRotArgs = 0;` |
|      ! 0 | 5685 | `	}` |
|        - | 5686 | `	{` |
|        - | 5687 | `		ph7_value aCallee[2];` |
|  2467791 | 5688 | `		ph7_value *pTopCallee = &pTos[-nRotArgs];` |
|  2467791 | 5689 | `		sxi32 nCallee = (pInstr->iP2 & PH7_ROT_TWOSLOT) ? 2 : 1;` |
|        - | 5690 | `		ph7_value *pBase;` |
|        - | 5691 | `		sxi32 i;` |
|  2467791 | 5692 | `		if( nCallee > 1 && (pTopCallee->iFlags & MEMOBJ_AUX_MAGICCALL) ){` |
|        - | 5693 | `			/* OP_MEMBER routed a missing/inaccessible name to __call: it consumed the` |
|        - | 5694 | `			 * receiver and left ONE carrier slot, so the pair the compiler counted on` |
|        - | 5695 | `			 * is not there. */` |
|       71 | 5696 | `			nCallee = 1;` |
|       34 | 5697 | `		}` |
|  2467791 | 5698 | `		pBase = pTopCallee - (nCallee - 1);` |
|        - | 5699 | `#ifdef UNTRUST` |
|        - | 5700 | `		if( pBase < pStack ){` |
|        - | 5701 | `			goto Abort;` |
|        - | 5702 | `		}` |
|        - | 5703 | `#endif` |
|  2467791 | 5704 | `		if( nRotArgs > 0 ){` |
|  4939083 | 5705 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  2471319 | 5706 | `				aCallee[i] = pBase[i];` |
|  1236709 | 5707 | `			}` |
|  6127144 | 5708 | `			for( i = 0 ; i < nRotArgs ; ++i ){` |
|  3659380 | 5709 | `				pBase[i] = pBase[i + nCallee];` |
|  1831181 | 5710 | `			}` |
|  4939083 | 5711 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  2471319 | 5712 | `				pBase[nRotArgs + i] = aCallee[i];` |
|  1236709 | 5713 | `			}` |
|  1234929 | 5714 | `		}` |
|  2467791 | 5715 | `		if( pInstr->iP2 & PH7_ROT_SPREAD ){` |
|        - | 5716 | `			/* The argument region now ends nCallee slots lower than it did, so this` |
|        - | 5717 | `			 * call's captured unpack runs — the suffix VmSpreadOwnExtra just assigned` |
|        - | 5718 | `			 * to it, all of them anchored inside the region — move with it, and OP_CALL` |
|        - | 5719 | ``			 * re-derives the same count from them. Unconditional: an `f(...[])` unpack`` |
|        - | 5720 | `			 * moves NO argument (its run is zero-width) and still has to be re-anchored,` |
|        - | 5721 | `			 * or the recount reads it as an ordinary slot and eats one slot too many.` |
|        - | 5722 | `			 * An ENCLOSING call's runs sit below the callee and are left alone. */` |
|      602 | 5723 | `			sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      602 | 5724 | `			VmSpreadRun *aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|        - | 5725 | `			sxu32 r;` |
|     1214 | 5726 | `			for( r = pVm->nSpreadCallBase ; r < nRun ; ++r ){` |
|      616 | 5727 | `				aRun[r].pStart -= nCallee;` |
|      310 | 5728 | `			}` |
|      299 | 5729 | `		}` |
|        - | 5730 | `	}` |
|  2467791 | 5731 | `	break;` |
|        - | 5732 | `}` |
|  2084601 | 5733 | `case PH7_OP_CALL: {` |
|        - | 5734 | `	/* iP2 = hasSpread (compile-time). Count only THIS call's own unpack` |
|        - | 5735 | `	 * expansion (VmSpreadOwnExtra, derived from the captured runs on top of the` |
|        - | 5736 | `	 * stack) — an INNER spread-bearing call evaluated inside this argument list` |
|        - | 5737 | ``	 * (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not`` |
|        - | 5738 | `	 * be conflated, which a single shared accumulator could not express. */` |
|  4171301 | 5739 | `	sxi32 nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|        - | 5740 | `	ph7_value *pArg;` |
|        - | 5741 | `	/* Consume the engine's magic-dispatch latch here, at the head of the ONE call` |
|        - | 5742 | `	 * it was set for, whatever path that call then takes. Left standing it would` |
|        - | 5743 | `	 * describe the next call instead. */` |
|  4171301 | 5744 | `	int bMagicDispatch = pVm->bMagicDispatch;` |
|        - | 5745 | `	/* ...and the member resolution's own verdict, which rides the callee SLOT rather` |
|        - | 5746 | `	 * than the VM: an OP_MEMBER that produced this callee already decided its` |
|        - | 5747 | `	 * visibility against the entry it chose, so the screen below must stand down. */` |
|  8283806 | 5748 | `	int bMemberScreened = (pTos->iFlags & MEMOBJ_AUX_MEMBERCALL) != 0` |
|  4171296 | 5749 | `		\|\| pVm->bClosureScreened;` |
|        - | 5750 | `	/* ...and whether the NAME in that slot is one of the engine's own function-table` |
|        - | 5751 | `	 * keys rather than something the program spelled: an OP_MEMBER method resolution` |
|        - | 5752 | ``	 * pushes the method's `sVmName` (`[__Class@meth_xxxxxxxxxx]`), and so do the two`` |
|        - | 5753 | `	 * synthetic call builders. Read here because the member mark is cleared just` |
|        - | 5754 | `	 * below; the closure branch adds its own case further down. It is what lets` |
|        - | 5755 | `	 * PH7_VmGetUserFunction refuse those keys to a SCRIPT that spells one. */` |
|  4171301 | 5756 | `	int bEngineCallee = (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_ENGINEFN)) != 0;` |
|        - | 5757 | `	/* ...and the internal-callback latch, for the same reason: it describes THIS call` |
|        - | 5758 | `	 * (an internal function invoking a userland callback binds its arguments weakly),` |
|        - | 5759 | `	 * and a call the callback body makes must not inherit it. */` |
|  4171301 | 5760 | `	int bCallbackWeak = pVm->bCallbackWeak;` |
|  4171301 | 5761 | `	pVm->bMagicDispatch = 0;` |
|  4171301 | 5762 | `	pVm->bClosureScreened = 0;` |
|  4171301 | 5763 | `	pVm->bCallbackWeak = 0;` |
|  4171301 | 5764 | `	pTos->iFlags &= ~MEMOBJ_AUX_MEMBERCALL;` |
|  4171301 | 5765 | `	pArg = &pTos[-nCallArgs];` |
|        - | 5766 | `	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED` |
|        - | 5767 | `	 * arguments, and a spread expanding to !=1 element shifts the actual positions` |
|        - | 5768 | `	 * of any following compile-time named args. The effective per-actual-slot name` |
|        - | 5769 | `	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)` |
|        - | 5770 | `	 * is built PER PATH against that path's finalized arg base — a method call pops` |
|        - | 5771 | `	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch` |
|        - | 5772 | `	 * site rather than built once here. */` |
|        - | 5773 | `	VmCallArgMap sEffMap;` |
|  4171301 | 5774 | `	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;` |
|        - | 5775 | `	SyHashEntry *pEntry;` |
|        - | 5776 | `	SyString sName;` |
|        - | 5777 | `	/* The host-call trio, hoisted out of the foreign-function branch below so the` |
|        - | 5778 | `	 * NativeCall label there can also be reached from the compiled-function branch:` |
|        - | 5779 | `	 * a VM_FUNC_NATIVE method is a C body wearing a method's clothes, and it runs on` |
|        - | 5780 | `	 * exactly the foreign path (no frame, no call record, no operand stack) once` |
|        - | 5781 | `	 * $this and the called class have been resolved the method way. Jumping into` |
|        - | 5782 | `	 * that branch would otherwise cross these declarations. */` |
|        - | 5783 | `	ph7_user_func *pFunc;` |
|        - | 5784 | `	ph7_context sCtx;` |
|        - | 5785 | `	ph7_value sRet;` |
|        - | 5786 | `	/* Native-METHOD call state; all 0 for a plain host function, which is what the` |
|        - | 5787 | `	 * foreign branch's own fallthrough leaves.` |
|        - | 5788 | `	 *   pNativeOwned — the reference the method path took, released at the shared tail` |
|        - | 5789 | `	 *   pNativeRecv  — what the body actually sees as $this (0 for a static method)` |
|        - | 5790 | `	 *   pNativeClass — the late-static-binding target */` |
|  4171301 | 5791 | `	ph7_class_instance *pNativeOwned = 0;` |
|  4171301 | 5792 | `	ph7_class_instance *pNativeRecv = 0;` |
|  4171301 | 5793 | `	ph7_class *pNativeClass = 0;` |
|        - | 5794 | `	/* The engine's own __call/__callStatic routing: the OP_MEMBER immediately below this` |
|        - | 5795 | `	 * call found a missing (or inaccessible) method on a class declaring the magic handler` |
|        - | 5796 | `	 * and MARKED this callee slot, latching {receiver, class, original name} on the VM.` |
|        - | 5797 | `	 * There is no callable here at all — the mark selects the packing body directly, ahead` |
|        - | 5798 | `	 * of every callable decode below, and the record it dispatches carries no PHP name (it` |
|        - | 5799 | `	 * is not in hHostFunction). This is what replaced writing the string` |
|        - | 5800 | `	 * "__phl_magic_call" into the slot and letting the name lookup find a hidden global.` |
|        - | 5801 | ``	 * Everything from `NativeCall` down is shared with an ordinary builtin call, which is`` |
|        - | 5802 | `	 * what this has always been from the executor's point of view. */` |
|  4171301 | 5803 | `	if( pTos->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|        - | 5804 | `		/* Move the routing off the carrier and onto the VM, HERE — one instruction` |
|        - | 5805 | `		 * before the packing body reads it, with nothing in between that could set` |
|        - | 5806 | `		 * another. OP_MEMBER used to publish it directly, which only held while the` |
|        - | 5807 | `		 * arguments ran before it; now they run after, and a routed call inside this` |
|        - | 5808 | `		 * one's argument list has already come and gone. */` |
|      130 | 5809 | `		VmMagicCall *pPend = (VmMagicCall *)pTos->x.pOther;` |
|      130 | 5810 | `		pTos->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|      130 | 5811 | `		pTos->x.pOther = 0;` |
|      130 | 5812 | `		pVm->pMagicCallThis = pPend ? pPend->pRecv : 0;` |
|      130 | 5813 | `		pVm->pMagicCallClass = pPend ? pPend->pClass : 0;` |
|      130 | 5814 | `		SyBlobReset(&pVm->sMagicCallName);` |
|      130 | 5815 | `		if( pPend && SyBlobLength(&pPend->sName) > 0 ){` |
|      193 | 5816 | `			SyBlobAppend(&pVm->sMagicCallName,SyBlobData(&pPend->sName),` |
|       63 | 5817 | `				SyBlobLength(&pPend->sName));` |
|       63 | 5818 | `		}` |
|      130 | 5819 | `		if( pPend ){` |
|        - | 5820 | `			/* The receiver reference the record held is now the VM's, which` |
|        - | 5821 | `			 * VmMagicCallDispatch gives back — so drop the record without unref'ing. */` |
|      130 | 5822 | `			pPend->pRecv = 0;` |
|      130 | 5823 | `			VmFreeMagicCall(pPend);` |
|       63 | 5824 | `		}` |
|      130 | 5825 | `		pFunc = PH7_VmMagicCallFunc(&(*pVm));` |
|      130 | 5826 | `		if( pFunc == 0 ){` |
|      ! 0 | 5827 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 5828 | `			goto Abort;` |
|        - | 5829 | `		}` |
|        - | 5830 | `		/* D1: the packing body declares no by-ref parameter (php hands __call a packed` |
|        - | 5831 | `		 * ARRAY), so every deferred argument materializes by value, exactly as it did` |
|        - | 5832 | `		 * through the named trampoline's zero by-ref mask. */` |
|        - | 5833 | `		{` |
|      130 | 5834 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffCallMap);` |
|      130 | 5835 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 5836 | `		}` |
|      193 | 5837 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|      126 | 5838 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|      130 | 5839 | `		goto NativeCall;` |
|        - | 5840 | `	}` |
|        - | 5841 | `	/* A Closure object is callable: unwrap it to its underlying string callable so the` |
|        - | 5842 | `	 * dispatch below handles it (rather than treating it as a generic object and looking` |
|        - | 5843 | `	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure` |
|        - | 5844 | `	 * so a plain __invoke object skips the temp-value work entirely. */` |
|  4171175 | 5845 | `	if( VmValueIsClosure(pVm,pTos) ){` |
|        - | 5846 | `		ph7_value sCallable;` |
|     8021 | 5847 | `		PH7_MemObjInit(pVm,&sCallable);` |
|     8021 | 5848 | `		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){` |
|     8021 | 5849 | `			PH7_MemObjRelease(pTos);` |
|     8021 | 5850 | `			PH7_MemObjStore(&sCallable,pTos);` |
|        - | 5851 | `` 			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]` `` |
|        - | 5852 | `			 * name, which the lookup below refuses to a name a SCRIPT spelled. */` |
|     8021 | 5853 | `			bEngineCallee = 1;` |
|     4008 | 5854 | `		}` |
|     8021 | 5855 | `		PH7_MemObjRelease(&sCallable);` |
|     4008 | 5856 | `	}` |
|        - | 5857 | `	/* Extract function name */` |
|  4171175 | 5858 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|   300435 | 5859 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 5860 | `			ph7_value sResult;` |
|        - | 5861 | `			sxi32 rcArr;` |
|        - | 5862 | `			/* Taken off the VM at the head of the shape check below, not at the dispatch:` |
|        - | 5863 | `			 * everything between the two (the deferred-argument materialization especially)` |
|        - | 5864 | `			 * can throw and jump out of this branch, and a latch left armed would stand the` |
|        - | 5865 | `			 * visibility screen down for whatever call runs next. */` |
|        - | 5866 | `			int bCbScreened;` |
|        - | 5867 | `			{` |
|        - | 5868 | `				/* php validates the SHAPE of an array callable first: it must hold exactly` |
|        - | 5869 | `				 * two elements. PH7 handed any array to the dispatcher, which failed` |
|        - | 5870 | ``				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */`` |
|   100330 | 5871 | `				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - | 5872 | `				char zCbMsg[192];` |
|   100330 | 5873 | `				const char *zCbErr = 0;` |
|   100330 | 5874 | `				int bCbRaised = 0; /* the class lookup ran an autoloader that threw */` |
|        - | 5875 | `				/* A pair the closure UNWRAP just built is not an array the program wrote: its` |
|        - | 5876 | `				 * callee was resolved and screened where the closure was BUILT, the way php` |
|        - | 5877 | `				 * resolves one, and it is a well-formed [target, method] by construction.` |
|        - | 5878 | `				 * Re-deciding it here, against the CALLER, is what refused an escaped` |
|        - | 5879 | ``				 * `$this->priv(...)` php runs. */`` |
|   100330 | 5880 | `				bCbScreened = pVm->bClosureScreened;` |
|   100330 | 5881 | `				pVm->bClosureScreened = 0; /* put back for the one dispatch that reads it */` |
|   100330 | 5882 | `				if( !bCbScreened && pCbMap && pCbMap->nEntry == 2 ){` |
|        - | 5883 | `					/* Shape is right; now check it actually RESOLVES. The shared dispatcher` |
|        - | 5884 | `					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for` |
|        - | 5885 | `					 * an unresolvable [class,method] pair -- silence a caller cannot detect --` |
|        - | 5886 | `					 * and its contract is relied on by call_user_func/usort, so the throw` |
|        - | 5887 | `					 * belongs here at the call site. */` |
|   100188 | 5888 | `					ph7_value *pCbCls = 0;` |
|   100188 | 5889 | `					ph7_value *pCbMeth = 0;` |
|        - | 5890 | `					/* php reads the INTEGER indices 0 and 1, not the first two entries in` |
|        - | 5891 | `					 * insertion order. Two entries at other keys are a SHAPE error of their` |
|        - | 5892 | `					 * own ("has to contain indices 0 and 1"), distinct from the` |
|        - | 5893 | ``					 * wrong-element-COUNT message below; `[1=>'m',0=>'C']` holds both, so`` |
|        - | 5894 | `					 * the target is index 0 -- the reverse of what PH7 walked. */` |
|   100188 | 5895 | `					if( !PH7_VmArrayCallableParts(&(*pVm),pCbMap,&pCbCls,&pCbMeth) ){` |
|       11 | 5896 | `						zCbErr = "Array callback has to contain indices 0 and 1";` |
|        6 | 5897 | `					}else{` |
|        - | 5898 | `						/* The resolution below can run an autoloader that throws; when it does,` |
|        - | 5899 | `						 * php propagates THAT exception and never reports the class missing. */` |
|   100178 | 5900 | `						sxi32 nCbBrc = pVm->nBoundaryRc;` |
|   100178 | 5901 | `						const void *pCbRes = (const void *)pVm->pResumeFrame;` |
|   150265 | 5902 | `						zCbErr = VmDirectArrayCallableError(&(*pVm),pCbCls,pCbMeth,` |
|    50087 | 5903 | `							zCbMsg,sizeof(zCbMsg));` |
|   100178 | 5904 | `						if( zCbErr && PH7_VmClassLookupRaised(&(*pVm),nCbBrc,pCbRes) ){` |
|        6 | 5905 | `							bCbRaised = 1;` |
|        2 | 5906 | `						}` |
|        - | 5907 | `					}` |
|    50092 | 5908 | `				}` |
|   100330 | 5909 | `				if( !bCbScreened && (pCbMap == 0 \|\| pCbMap->nEntry != 2 \|\| zCbErr) ){` |
|        - | 5910 | `					sxi32 rcCb;` |
|       87 | 5911 | `					if( pInstr->iP2 ){` |
|      ! 0 | 5912 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 5913 | `					}` |
|       87 | 5914 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 5915 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 5916 | `					}` |
|       87 | 5917 | `					PH7_MemObjRelease(pTos);` |
|       87 | 5918 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       87 | 5919 | `					pTos->nIdx = SXU32_HIGH;` |
|       87 | 5920 | `					if( bCbRaised ){` |
|        - | 5921 | `						/* Land the autoloader's own throw: consume the parked status (an` |
|        - | 5922 | `						 * in-place catch leaves 0 behind plus a recorded resume frame, which` |
|        - | 5923 | `						 * the router below picks up). */` |
|        6 | 5924 | `						rcCb = pVm->nBoundaryRc;` |
|        6 | 5925 | `						pVm->nBoundaryRc = 0;` |
|        6 | 5926 | `						if( rcCb == PH7_ABORT ){ goto Abort; }` |
|        6 | 5927 | `						rc = PH7_EXCEPTION;` |
|       18 | 5928 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5929 | `					}` |
|       82 | 5930 | `					if( zCbErr == 0 ){` |
|       17 | 5931 | `						zCbErr = "Array callback must have exactly two elements";` |
|        8 | 5932 | `					}` |
|       82 | 5933 | `					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));` |
|       82 | 5934 | `					if( rcCb == SXERR_ABORT ){ goto Abort; }` |
|       82 | 5935 | `					rc = rcCb;` |
|        - | 5936 | `					/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 5937 | `					 * (SXRET_OK + recorded resume) FELL THROUGH and kept dispatching` |
|        - | 5938 | `					 * the malformed callable inside the try. Route like OP_THROW. */` |
|      106 | 5939 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5940 | `				}` |
|        - | 5941 | `			}` |
|        - | 5942 | `			/* Build the effective spread-key map (and consume this call's runs)` |
|        - | 5943 | `			 * against this path's arg base (the array-callable slot isn't popped). */` |
|   150367 | 5944 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100242 | 5945 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 5946 | `			/* Materialize the deferred arguments against the pair's own method (see` |
|        - | 5947 | `			 * VmIndirectCalleeFunc), not against a blanket by-ref assumption. */` |
|        - | 5948 | `			{` |
|   100246 | 5949 | `				sxi32 rcDA = VmResolveIndirectArgs(&(*pVm),pTos,pArg,pTos,pEffCallMap);` |
|   100246 | 5950 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 5951 | `			}` |
|   100246 | 5952 | `			SySetReset(&aArg);` |
|   100424 | 5953 | `			while( pArg < pTos ){` |
|      181 | 5954 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      181 | 5955 | `				pArg++;` |
|        3 | 5956 | `			}` |
|   100246 | 5957 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 5958 | `			/* May be a class instance and it's static method. Forward this call's named-arg map` |
|        - | 5959 | ``			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —`` |
|        - | 5960 | `			 * mirroring the __invoke-object branch below. */` |
|   100246 | 5961 | `			pVm->bClosureScreened = bCbScreened; /* see the capture above */` |
|   100246 | 5962 | `			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|        - | 5963 | `			/* The latch is consumed by the method OP_CALL this dispatch builds; clear it here` |
|        - | 5964 | `			 * for the paths that never reach one. */` |
|   100246 | 5965 | `			pVm->bClosureScreened = 0;` |
|   100246 | 5966 | `			SySetReset(&aArg);` |
|        - | 5967 | `			/* Pop given arguments */` |
|   100246 | 5968 | `			if( nCallArgs > 0 ){` |
|      155 | 5969 | `				VmPopOperand(&pTos,nCallArgs);` |
|       76 | 5970 | `			}` |
|   100246 | 5971 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 | 5972 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 5973 | `				goto Abort;` |
|        - | 5974 | `			}` |
|   100246 | 5975 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - | 5976 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - | 5977 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - | 5978 | `				sxi32 iResumePc;` |
|   100014 | 5979 | `				PH7_MemObjRelease(&sResult);` |
|   100014 | 5980 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100004 | 5981 | `					PH7_MemObjRelease(pTos);` |
|        - | 5982 | ``					/* Drain the abandoned outer-expression operands (`1 + $cb()`)`` |
|        - | 5983 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 5984 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300006 | 5985 | `					PH7_RESUME_DRAIN()` |
|   100004 | 5986 | `					pc = iResumePc;` |
|   100004 | 5987 | `					break;` |
|        - | 5988 | `				}` |
|       11 | 5989 | `				goto Exception;` |
|        - | 5990 | `			}` |
|        - | 5991 | `			/* Copy result */` |
|      233 | 5992 | `			PH7_MemObjStore(&sResult,pTos);` |
|      233 | 5993 | `			PH7_MemObjRelease(&sResult);` |
|   200223 | 5994 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|   200092 | 5995 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - | 5996 | `			ph7_value sResult;` |
|        - | 5997 | `			sxi32 rcInv;` |
|        - | 5998 | `			/* __invoke object callable: the object slot isn't popped, so pArg is` |
|        - | 5999 | `			 * already this call's arg base — build the map + consume the runs. */` |
|   300136 | 6000 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   200088 | 6001 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6002 | `			/* Materialize the deferred arguments against this object's __invoke, the` |
|        - | 6003 | `			 * array-callable path's rule one shape over. */` |
|        - | 6004 | `			{` |
|   200092 | 6005 | `				sxi32 rcDA = VmResolveIndirectArgs(&(*pVm),pTos,pArg,pTos,pEffCallMap);` |
|   200092 | 6006 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6007 | `			}` |
|   200092 | 6008 | `			SySetReset(&aArg);` |
|   200186 | 6009 | `			while( pArg < pTos ){` |
|       98 | 6010 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       98 | 6011 | `				pArg++;` |
|        4 | 6012 | `			}` |
|   200092 | 6013 | `			PH7_MemObjInit(pVm,&sResult);` |
|   300136 | 6014 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|   200088 | 6015 | `				(int)SySetUsed(&aArg),` |
|   200088 | 6016 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - | 6017 | `				&sResult,` |
|   100044 | 6018 | `				pEffCallMap);` |
|   200092 | 6019 | `			SySetReset(&aArg);` |
|        - | 6020 | `			/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable` |
|        - | 6021 | `			 * slot itself (it pops top-down and the callable IS pTos), which for a` |
|        - | 6022 | `			 * temporary like (new Plain())(...) holds the only reference — popping it` |
|        - | 6023 | `			 * would free pThis before VmRaiseNotCallable reads its class name below.` |
|        - | 6024 | `			 * Only the not-callable branch dereferences pThis afterwards, so pin just` |
|        - | 6025 | `			 * for that case; the matching PH7_ClassInstanceUnref drops it (destroying` |
|        - | 6026 | `			 * the temporary). The other branches let the pop free the temp as before. */` |
|   200092 | 6027 | `			if( rcInv == SXERR_INVALID ){` |
|   100006 | 6028 | `				pThis->iRef++;` |
|    50002 | 6029 | `			}` |
|   200092 | 6030 | `			if( nCallArgs > 0 ){` |
|       78 | 6031 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 | 6032 | `			}` |
|   200092 | 6033 | `			if( rcInv == SXERR_INVALID ){` |
|        - | 6034 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - | 6035 | `				 * sResult was already released by VmCallObjectInvoke. */` |
|   100006 | 6036 | `				PH7_MemObjRelease(pTos);` |
|   100006 | 6037 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|   100006 | 6038 | `				PH7_ClassInstanceUnref(pThis);` |
|   100006 | 6039 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 6040 | `					goto Abort;` |
|        - | 6041 | `				}` |
|        - | 6042 | `				{` |
|        - | 6043 | `					sxi32 iRp;` |
|   100006 | 6044 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|        - | 6045 | `						/* Drain the abandoned outer-expression operands` |
|        - | 6046 | ``						 * (`1 + $plainObj()`) to the try's base — one leaked`` |
|        - | 6047 | `						 * slot per caught throw otherwise. */` |
|   300010 | 6048 | `						PH7_RESUME_DRAIN()` |
|   100006 | 6049 | `						pc = iRp;` |
|   100006 | 6050 | `						break;` |
|        - | 6051 | `					}` |
|        - | 6052 | `				}` |
|      ! 0 | 6053 | `				goto Exception;` |
|        - | 6054 | `			}` |
|   100088 | 6055 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 | 6056 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 6057 | `				goto Abort;` |
|        - | 6058 | `			}` |
|   100088 | 6059 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - | 6060 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - | 6061 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - | 6062 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - | 6063 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - | 6064 | `				sxi32 iResumePc;` |
|   100008 | 6065 | `				PH7_MemObjRelease(&sResult);` |
|   100008 | 6066 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100006 | 6067 | `					PH7_MemObjRelease(pTos);` |
|        - | 6068 | ``					/* Drain the abandoned outer-expression operands (`1 + $inv()`)`` |
|        - | 6069 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 6070 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300010 | 6071 | `					PH7_RESUME_DRAIN()` |
|   100006 | 6072 | `					pc = iResumePc;` |
|   100006 | 6073 | `					break;` |
|        - | 6074 | `				}` |
|        3 | 6075 | `				goto Exception;` |
|        - | 6076 | `			}` |
|       82 | 6077 | `			PH7_MemObjStore(&sResult,pTos);` |
|       82 | 6078 | `			PH7_MemObjRelease(&sResult);` |
|       43 | 6079 | `		}else{` |
|        - | 6080 | `			/* php: calling a non-callable is a catchable Error naming the type` |
|        - | 6081 | `			 * ("Value of type int is not callable"), or -- for an array -- the shape it` |
|        - | 6082 | `			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so` |
|        - | 6083 | ``			 * `$x()` on a number quietly evaluated to nothing. */`` |
|        - | 6084 | `			sxi32 rcNc;` |
|        - | 6085 | `			char zMsg[128];` |
|       17 | 6086 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 6087 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");` |
|      ! 0 | 6088 | `			}else{` |
|       25 | 6089 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",` |
|        8 | 6090 | `					VmArithTypeName(pTos));` |
|        - | 6091 | `			}` |
|        - | 6092 | `			/* Consume this call's captured spread runs — a non-callable target` |
|        - | 6093 | `			 * (int/float/bool/null) reaches no dispatch build site. */` |
|       17 | 6094 | `			if( pInstr->iP2 ){` |
|      ! 0 | 6095 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 6096 | `			}` |
|        - | 6097 | `			/* Pop given arguments */` |
|       17 | 6098 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 6099 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6100 | `			}` |
|        - | 6101 | `			/* Settle the call's result slot BEFORE throwing. */` |
|       17 | 6102 | `			PH7_MemObjRelease(pTos);` |
|       17 | 6103 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       17 | 6104 | `			pTos->nIdx = SXU32_HIGH;` |
|       17 | 6105 | `			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       17 | 6106 | `			if( rcNc == SXERR_ABORT ){ goto Abort; }` |
|       17 | 6107 | `			rc = rcNc;` |
|        - | 6108 | `			/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 6109 | `			 * (SXRET_OK + recorded resume) FELL THROUGH and resumed the try body` |
|        - | 6110 | `			 * right after the failed call. Route like OP_THROW. */` |
|       31 | 6111 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6112 | `		}` |
|      313 | 6113 | `		break;` |
|        - | 6114 | `	}` |
|  3870745 | 6115 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 6116 | `	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just` |
|        - | 6117 | `	 * anchors it to the global namespace — strip it before resolving so a` |
|        - | 6118 | ``	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */`` |
|  3870745 | 6119 | `	if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|       15 | 6120 | `		sName.zString++;` |
|       15 | 6121 | `		sName.nByte--;` |
|        7 | 6122 | `	}` |
|        - | 6123 | `	/* Check for a compiled function first.` |
|        - | 6124 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 6125 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|  3870745 | 6126 | `	pEntry = PH7_VmGetUserFunction(pVm,(const void *)sName.zString,sName.nByte,bEngineCallee);` |
|        - | 6127 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - | 6128 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - | 6129 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - | 6130 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - | 6131 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - | 6132 | `	{` |
|  3870745 | 6133 | `	VmCallArgMap *pCallMap = pEffCallMap;` |
|  3870745 | 6134 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - | 6135 | `		const char *zFunc;` |
|        - | 6136 | `		const char *zEnd;` |
|        - | 6137 | `		const char *z;` |
|        - | 6138 | `		SyString sGlobal;` |
|       57 | 6139 | `		zFunc = sName.zString;` |
|       57 | 6140 | `		zEnd  = zFunc + sName.nByte;` |
|       57 | 6141 | `		z = zEnd;` |
|        - | 6142 | `		/* Find last namespace separator */` |
|      529 | 6143 | `		while( z > zFunc ){` |
|      529 | 6144 | `			if( z[-1] == '\\' ){` |
|       57 | 6145 | `				break;` |
|        - | 6146 | `			}` |
|      477 | 6147 | `			z--;` |
|        5 | 6148 | `		}` |
|       57 | 6149 | `		if( z > zFunc && z < zEnd ){` |
|        - | 6150 | `			/* Retry lookup using the unqualified/global function name */` |
|       57 | 6151 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       57 | 6152 | `			pEntry = PH7_VmGetUserFunction(pVm,(const void *)sGlobal.zString,sGlobal.nByte,bEngineCallee);` |
|       26 | 6153 | `		}` |
|       26 | 6154 | `	}` |
|        - | 6155 | `	} /* end VmCallArgMap namespace scope */` |
|  3870745 | 6156 | `	if( pEntry ){` |
|        - | 6157 | `		ph7_vm_func_arg *aFormalArg;` |
|        - | 6158 | `		ph7_class_instance *pThis;` |
|        - | 6159 | `		ph7_value *pFrameStack;` |
|        - | 6160 | `		ph7_vm_func *pVmFunc;` |
|        - | 6161 | `		ph7_class *pSelf;` |
|        - | 6162 | `		ph7_class *pSelfHint;` |
|        - | 6163 | `		VmFrame *pFrame;` |
|        - | 6164 | `		ph7_value *pObj;` |
|        - | 6165 | `		VmSlot sArg;` |
|        - | 6166 | `		sxu32 n;` |
|  2255462 | 6167 | `		sxi32 iArgPreFlags = 0; /* the actual's type before its declared-type check */` |
|  2255462 | 6168 | `		int bClosureThis = 0;` |
|  2255462 | 6169 | `		ph7_class *pClosureScope = 0;` |
|        - | 6170 | `		/* initialize fields */` |
|  2255462 | 6171 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|  2255462 | 6172 | `		pThis = 0;` |
|  2255462 | 6173 | `		pSelf = 0;` |
|        - | 6174 | `		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it` |
|        - | 6175 | `		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever` |
|        - | 6176 | `		 * set for a bound plain closure, which dispatches as a function, so the method branch below` |
|        - | 6177 | `		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */` |
|  2255462 | 6178 | `		if( pVm->pClosureThis ){` |
|       69 | 6179 | `			pThis = pVm->pClosureThis;` |
|       69 | 6180 | `			pVm->pClosureThis = 0;` |
|       69 | 6181 | `			bClosureThis = 1;` |
|       33 | 6182 | `		}` |
|  2255462 | 6183 | `		if( pVm->pClosureScope ){` |
|        - | 6184 | `			/* May ride alongside a bound $this, or stand alone for a` |
|        - | 6185 | ``			 * scope-only rebind (`bindTo(null, Scope::class)`). */`` |
|       59 | 6186 | `			pClosureScope = pVm->pClosureScope;` |
|       59 | 6187 | `			pVm->pClosureScope = 0;` |
|       28 | 6188 | `		}` |
|  2255462 | 6189 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - | 6190 | `			ph7_class_method *pMeth;` |
|        - | 6191 | `			/* Class method call */` |
|  1996974 | 6192 | `			ph7_value *pTarget = &pTos[-1];` |
|  1996974 | 6193 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - | 6194 | `				/* Extract the 'this' pointer */` |
|  1996974 | 6195 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - | 6196 | `					/* Instance already loaded */` |
|  1895434 | 6197 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|  1895434 | 6198 | `					pThis->iRef++;` |
|  1895434 | 6199 | `					pSelf = pThis->pClass;` |
|   947716 | 6200 | `				}` |
|  1996974 | 6201 | `				if( pSelf == 0 ){` |
|   101545 | 6202 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - | 6203 | `						/* "Late Static Binding" class name */` |
|   152306 | 6204 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|    50767 | 6205 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|    50767 | 6206 | `					}` |
|   101545 | 6207 | `					if( pSelf == 0 ){` |
|        7 | 6208 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 | 6209 | `					}` |
|    50770 | 6210 | `				}` |
|  1996974 | 6211 | `				if( pThis == 0  ){` |
|   101545 | 6212 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|   101545 | 6213 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   101545 | 6214 | `					if( pFrameLocal->pParent ){` |
|        - | 6215 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|      731 | 6216 | `						pThis = pFrameLocal->pThis;` |
|      731 | 6217 | `						if( pThis ){` |
|       62 | 6218 | `							pThis->iRef++;` |
|       30 | 6219 | `						}` |
|      363 | 6220 | `					}` |
|    50770 | 6221 | `				}` |
|  1996974 | 6222 | `				VmPopOperand(&pTos,1);` |
|  1996974 | 6223 | `				PH7_MemObjRelease(pTos);` |
|        - | 6224 | `				/* Synchronize pointers. The method-name slot popped above sat BETWEEN` |
|        - | 6225 | `				 * this call's arguments and the (already-removed) target — so only now` |
|        - | 6226 | `				 * is pTos one past the last actual argument. Re-derive the unpack` |
|        - | 6227 | `				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs` |
|        - | 6228 | `				 * from run ends, which the extra target slot would otherwise offset,` |
|        - | 6229 | ``				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */`` |
|  1996974 | 6230 | `				nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);` |
|  1996974 | 6231 | `				pArg = &pTos[-nCallArgs];` |
|        - | 6232 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - | 6233 | `				 * user have already computed the random generated unique class method name` |
|        - | 6234 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - | 6235 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - | 6236 | `				 */` |
|  1996974 | 6237 | `				while( pArg < pStack ){` |
|      ! 0 | 6238 | `					pArg++;` |
|      ! 0 | 6239 | `				}` |
|  1996974 | 6240 | `				if( pSelf && pVm->bReflectBypass ){` |
|        - | 6241 | `					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection` |
|        - | 6242 | `					 * ignores visibility. Consume-once so nested calls made by the` |
|        - | 6243 | `					 * invoked body are checked normally. */` |
|      219 | 6244 | `					pVm->bReflectBypass = 0;` |
|      110 | 6245 | `				}else` |
|  1996756 | 6246 | `				if( pSelf && bMagicDispatch && PH7_MagicMethodMustBePublic(&pVmFunc->sName) ){` |
|        - | 6247 | `					/* The ENGINE reaching for a magic method php requires to be public.` |
|        - | 6248 | `					 * php only WARNS at such a declaration and then dispatches the method` |
|        - | 6249 | ``					 * regardless -- the engine calling `__get` is not the outside world`` |
|        - | 6250 | `					 * reaching for a private member -- so a non-public` |
|        - | 6251 | ``					 * `__get`/`__call`/`__invoke`/`__sleep`/... still runs, where PHL threw`` |
|        - | 6252 | ``					 * `Call to private method C::__get()` at the ACCESS and killed the`` |
|        - | 6253 | `					 * script.` |
|        - | 6254 | `					 *` |
|        - | 6255 | `					 * The latch is set by PH7_VmCallMagicMethod at the engine's own` |
|        - | 6256 | `					 * dispatch sites. It is NOT inferred from the instruction: a` |
|        - | 6257 | ``					 * first-class `$o->__get(...)` and `$o->__get('x')` reach the same C`` |
|        - | 6258 | `					 * dispatcher, and php denies both. */` |
|    50590 | 6259 | `				}else` |
|  1895586 | 6260 | `				if( pSelf && !bMemberScreened ){ /* Paranoid edition */` |
|        - | 6261 | `					/* Check if the call is allowed. php binds non-public method` |
|        - | 6262 | `					 * access by the DECLARING class (pVmFunc->pUserData — the class` |
|        - | 6263 | `					 * the callee was compiled in), NOT the instance's class: an` |
|        - | 6264 | `					 * inherited base method calling $this->priv() on a child` |
|        - | 6265 | `					 * instance passes, a child's private SHADOW doesn't hijack the` |
|        - | 6266 | `					 * check for a parent callee, and the denial message names the` |
|        - | 6267 | `					 * declaring class like php. */` |
|  1777861 | 6268 | `					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|        - | 6269 | `					ph7_class *pOwnerClass;` |
|  1777861 | 6270 | `					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|  1777861 | 6271 | `					if( pMeth == 0 && pDeclClass != pSelf ){` |
|      ! 0 | 6272 | `						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      ! 0 | 6273 | `					}` |
|  1777861 | 6274 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        - | 6275 | `						/* ...except that a TRAIT is not a class php still has at run time: it` |
|        - | 6276 | `						 * composed the method INTO the using class, so that class owns the` |
|        - | 6277 | `						 * rule and the name. Deciding against the trait refused a protected` |
|        - | 6278 | `						 * trait method to a SUBCLASS of the composing class (which uses no` |
|        - | 6279 | ``						 * trait of its own) — `class Az { use Tz; } class Bz extends Az {`` |
|        - | 6280 | ``						 * $this->pr(); }` was a fatal php runs. Identity for every non-trait`` |
|        - | 6281 | `						 * method. */` |
|       27 | 6282 | `						pOwnerClass = PH7_VmMethodScopeName(&(*pVm),pSelf,pMeth);` |
|       27 | 6283 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - | 6284 | `							/* php throws a CATCHABLE Error here. The old code merely PRINTED an` |
|        - | 6285 | ``							 * uncaught-exception report and aborted, so `try { $o->priv(); }`` |
|        - | 6286 | ``							 * catch (Error $e)` never caught it and the script died. */`` |
|        - | 6287 | `							char zMsg[256];` |
|        - | 6288 | `							sxi32 rcVis;` |
|        - | 6289 | `							/* php NAMES the calling scope when there is one — "from scope C" —` |
|        - | 6290 | `							 * and says "global scope" only outside every class; the wording is` |
|        - | 6291 | `							 * shared with the first-class-callable screen` |
|        - | 6292 | `							 * (VmMethodVisibilityMsg). */` |
|       19 | 6293 | `							VmMethodVisibilityMsg(&(*pVm),pOwnerClass,` |
|        6 | 6294 | `								pVmFunc->sName.zString,pVmFunc->sName.nByte,` |
|        6 | 6295 | `								pMeth->iProtection,zMsg,sizeof(zMsg));` |
|        - | 6296 | `							/* Consume this call's captured spread runs — this visibility` |
|        - | 6297 | `							 * error exits before the pVmFunc build below. */` |
|       13 | 6298 | `							if( pInstr->iP2 ){` |
|      ! 0 | 6299 | `								VmSpreadConsume(pVm);` |
|      ! 0 | 6300 | `							}` |
|        - | 6301 | `							/* Pop given arguments, and leave the call's NULL result behind. */` |
|       13 | 6302 | `							if( nCallArgs > 0 ){` |
|      ! 0 | 6303 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6304 | `							}` |
|       13 | 6305 | `							PH7_MemObjRelease(pTos);` |
|       13 | 6306 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|       13 | 6307 | `							pTos->nIdx = SXU32_HIGH;` |
|       13 | 6308 | `							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       13 | 6309 | `							if( rcVis == SXERR_ABORT ){ goto Abort; }` |
|       13 | 6310 | `							rc = rcVis;` |
|        - | 6311 | `							/* ENFORCE_RC never consulted pResumeFrame, so an in-place` |
|        - | 6312 | `							 * catch (SXRET_OK + recorded resume) FELL THROUGH and` |
|        - | 6313 | `							 * dispatched the DENIED method anyway. Route like OP_THROW. */` |
|       13 | 6314 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6315 | `						}` |
|        7 | 6316 | `					}` |
|   888922 | 6317 | `				}` |
|   998480 | 6318 | `			}` |
|   998480 | 6319 | `		}` |
|        - | 6320 | `		/* pArg is now finalized for every pVmFunc callee (a method call popped its` |
|        - | 6321 | `		 * method-name slot above; functions/closures/generators keep the top base).` |
|        - | 6322 | `		 * Build the PHP 8.1 effective spread-key map here so the generator and the` |
|        - | 6323 | `		 * install path below both see it — and so this call's captured runs are` |
|        - | 6324 | `		 * consumed exactly once, against the correct base. */` |
|  3383396 | 6325 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  2255445 | 6326 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6327 | `		/* Check the PHP call-depth cap (the sole site — BYTECODE.md stage 5).` |
|        - | 6328 | `		 * Default is unbounded (heap-bound recursion, decoupled from the C stack` |
|        - | 6329 | `		 * by the stage-2 trampoline); the C stack is guarded separately by` |
|        - | 6330 | `		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and` |
|        - | 6331 | `		 * then raises a clean non-catchable fatal (was: silently set NULL and` |
|        - | 6332 | `		 * continue) and halts. */` |
|  2255450 | 6333 | `		if( VmRecursionExceeded(pVm) ){` |
|        - | 6334 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - | 6335 | `			 * which walks the whole operand stack — don't release them here. */` |
|        3 | 6336 | `			VmRecursionFatal(&(*pVm));` |
|        3 | 6337 | `			goto Abort;` |
|        - | 6338 | `		}` |
|  2255448 | 6339 | `		if( pVmFunc->pNextName ){` |
|        - | 6340 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      289 | 6341 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|      142 | 6342 | `		}` |
|        - | 6343 | ``		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's`` |
|        - | 6344 | ``		 * `Class::method`). Computed after overload resolution so it reflects the selected method.`` |
|        - | 6345 | ``		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed`` |
|        - | 6346 | `		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method` |
|        - | 6347 | ``		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the`` |
|        - | 6348 | `		 * USING class; we don't carry the using class on the shared struct, so fall back to the` |
|        - | 6349 | `		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for` |
|        - | 6350 | `		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */` |
|  2255448 | 6351 | `		pSelfHint = pSelf;` |
|  2255448 | 6352 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|  1996962 | 6353 | `			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;` |
|  1996962 | 6354 | `			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|  1996656 | 6355 | `				pSelfHint = pDecl;` |
|   998327 | 6356 | `			}` |
|   998480 | 6357 | `		}` |
|  2255448 | 6358 | `		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 6359 | `			/* Push the closure's called-class as this frame's LSB class so` |
|        - | 6360 | ``			 * `static::` inside the body resolves like php. An explicit`` |
|        - | 6361 | `			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use` |
|        - | 6362 | `			 * the class captured at the closure's creation site. self::/parent::` |
|        - | 6363 | `			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done` |
|        - | 6364 | ``			 * after pSelfHint so a `self`-typed param is unaffected. */`` |
|    14367 | 6365 | `			if( pClosureScope ){` |
|       59 | 6366 | `				pSelf = pClosureScope;` |
|    14339 | 6367 | `			}else if( pVmFunc->pLsbClass ){` |
|      133 | 6368 | `				pSelf = (ph7_class *)pVmFunc->pLsbClass;` |
|       64 | 6369 | `			}` |
|     7181 | 6370 | `		}` |
|  2255448 | 6371 | `		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){` |
|        - | 6372 | `			/* php 8.4 #[\Deprecated] runtime notice — once per call, before` |
|        - | 6373 | `			 * execution (generators: at the g(...) call site, like php). */` |
|      284 | 6374 | `			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,` |
|      186 | 6375 | `				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);` |
|       93 | 6376 | `		}` |
|        - | 6377 | `		/* D1: resolve deferred plain-var arguments against this callee's declaration` |
|        - | 6378 | `		 * now — BEFORE the native/generator splits and VmEnterFrame, while pVm->pFrame is` |
|        - | 6379 | `		 * still the caller. pVmFunc is final here (post-overload). Covers plain functions,` |
|        - | 6380 | `		 * methods, closures, dynamic-name calls, generators and native methods uniformly;` |
|        - | 6381 | `		 * only the SOURCE of the by-ref positions differs between the last one and the` |
|        - | 6382 | `		 * rest (a signature string vs. compiled formal parameters). */` |
|        - | 6383 | `		{` |
|        - | 6384 | `			sxi32 rcDA;` |
|  2255448 | 6385 | `			if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 6386 | `				/* A native method declares no formal parameters to match against —` |
|        - | 6387 | `				 * its by-ref positions come from the same signature-derived mask a` |
|        - | 6388 | `				 * builtin uses, so resolve them the builtin way. Sharing this one` |
|        - | 6389 | `				 * site (rather than repeating it in the branch below) keeps the` |
|        - | 6390 | `				 * throw routing identical for both kinds of callee. */` |
|  2237222 | 6391 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|  1491477 | 6392 | `					pVmFunc->pNative->nByRefMask,0,0,pEffCallMap);` |
|   745745 | 6393 | `			}else{` |
|  1146176 | 6394 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   763966 | 6395 | `					(ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs),SySetUsed(&pVmFunc->aArgs),` |
|   382205 | 6396 | `					0,0,0,pEffCallMap);` |
|        - | 6397 | `			}` |
|  2255460 | 6398 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6399 | `		}` |
|  2255422 | 6400 | `		if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 6401 | `			/* C-bodied method (VM_FUNC_NATIVE): hand it to the foreign-function path.` |
|        - | 6402 | `			 *` |
|        - | 6403 | `			 * Everything a METHOD needs has already happened above — the sVmName` |
|        - | 6404 | `			 * lookup, overload selection, $this / self / late-static-binding` |
|        - | 6405 | `			 * resolution, the visibility check, the #[\Deprecated] notice, deferred` |
|        - | 6406 | `			 * argument materialization — and everything BELOW is exactly what a C` |
|        - | 6407 | `			 * body has no use for: a frame, an operand stack, a call record.` |
|        - | 6408 | `			 *` |
|        - | 6409 | `			 * The stack shape already matches a builtin's, because the method branch` |
|        - | 6410 | `			 * popped both the method-name slot and the target slot: [pArg,pTos) are` |
|        - | 6411 | `			 * this call's arguments and pTos is the slot that receives the result.` |
|        - | 6412 | `			 * So the jump lands on shared code, not a copy of it. */` |
|  1491482 | 6413 | `			pFunc = pVmFunc->pNative;` |
|  1491482 | 6414 | `			pNativeOwned = pThis; /* borrowed; the shared tail drops this reference */` |
|        - | 6415 | ``			/* A `C::m()` call reaches the method path with no object in the target`` |
|        - | 6416 | `			 * slot, and the path then adopts the CALLER's $this so an instance-context` |
|        - | 6417 | ``			 * `self::m()` still finds one. A native STATIC method must not see that —`` |
|        - | 6418 | `			 * it would read its argument list one slot off — so the receiver handed to` |
|        - | 6419 | `			 * the body is gated on the declaration, not on what the fallback found. */` |
|  1491482 | 6420 | `			pNativeRecv = (pVmFunc->iFlags & VM_FUNC_NATIVE_STATIC) ? 0 : pThis;` |
|  1491482 | 6421 | `			pNativeClass = pSelf;` |
|  1491482 | 6422 | `			goto NativeCall;` |
|        - | 6423 | `		}` |
|   763945 | 6424 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - | 6425 | `			/* Generator function: return a Generator object instead of executing */` |
|        - | 6426 | `			ph7_exec_ctx *pExecCtx;` |
|        - | 6427 | `			ph7_generator *pGenerator;` |
|        - | 6428 | `			ph7_class_instance *pGenObj;` |
|        - | 6429 | `			ph7_value *pCtxAttr;` |
|        - | 6430 | `			SyString sAttrName;` |
|        - | 6431 | `			ph7_value **apCallArgs;` |
|        - | 6432 | `			int nGenArgs, iArg;` |
|        - | 6433 | `			/* Collect arguments from the operand stack */` |
|      421 | 6434 | `			nGenArgs = (int)(pTos - pArg);` |
|      421 | 6435 | `			apCallArgs = 0;` |
|      421 | 6436 | `			if( nGenArgs > 0 ){` |
|        - | 6437 | `				/* php refuses a non-variable in a by-ref position at the CALL, and for` |
|        - | 6438 | `				 * a generator this IS the call. Routed like the branch's other` |
|        - | 6439 | `				 * pre-frame throws below: no callee frame exists yet, so drop the` |
|        - | 6440 | `				 * arguments plus the function-name slot and land the enclosing try. */` |
|      182 | 6441 | `				rc = VmScreenGenByRefArgs(&(*pVm),pVmFunc,pEffCallMap,pArg,` |
|       59 | 6442 | `					(sxu32)nGenArgs,pSelfHint);` |
|      123 | 6443 | `				if( rc != SXRET_OK ){` |
|        7 | 6444 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 6445 | `						goto Abort;` |
|        - | 6446 | `					}` |
|      212 | 6447 | `					PH7_INLINE_RESUME_BREAK()` |
|        7 | 6448 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6449 | `					{` |
|        - | 6450 | `						sxi32 iRpB;` |
|        7 | 6451 | `						if( VmRecordedResume(pVm,&iRpB,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 6452 | `							pc = iRpB;` |
|      ! 0 | 6453 | `							break;` |
|        - | 6454 | `						}` |
|        - | 6455 | `					}` |
|        7 | 6456 | `					goto Exception;` |
|        - | 6457 | `				}` |
|       56 | 6458 | `			}` |
|      415 | 6459 | `			if( nGenArgs > 0 ){` |
|      173 | 6460 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       56 | 6461 | `					nGenArgs * sizeof(ph7_value *));` |
|      117 | 6462 | `				if( apCallArgs == 0 ){` |
|        - | 6463 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 | 6464 | `					nGenArgs = 0;` |
|      ! 0 | 6465 | `				}else{` |
|      117 | 6466 | `					VmCallArgMap *pGenMap = pEffCallMap;` |
|      117 | 6467 | `					int didReorder = 0;` |
|      117 | 6468 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - | 6469 | `						/* Named-argument reordering for generator */` |
|       15 | 6470 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|       15 | 6471 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|       15 | 6472 | `						sxu32 nNV = nF;` |
|       15 | 6473 | `						sxi32 iVIdx = -1;` |
|        - | 6474 | `						sxi32 *aGSlot;` |
|        - | 6475 | `						sxu8 *aGUsed;` |
|        - | 6476 | `						sxu32 gi;` |
|       33 | 6477 | `						for( gi = 0; gi < nF; gi++ ){` |
|       21 | 6478 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|       12 | 6479 | `						}` |
|       21 | 6480 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       12 | 6481 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|       15 | 6482 | `						if( aGSlot ){` |
|       15 | 6483 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|       21 | 6484 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        6 | 6485 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|       15 | 6486 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 6487 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 6488 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6489 | `								goto Abort;` |
|        - | 6490 | `							}` |
|       15 | 6491 | `							if( rc == PH7_EXCEPTION ){` |
|        - | 6492 | `								/* A named-argument error is php's catchable \Error.` |
|        - | 6493 | `								 * No callee frame exists yet on this branch (the` |
|        - | 6494 | `								 * generator body never runs and VmEnterFrame is` |
|        - | 6495 | `								 * further down), so route it like the other` |
|        - | 6496 | `								 * pre-frame OP_CALL throws: drop the args + the` |
|        - | 6497 | `								 * function-name slot and land the enclosing try. */` |
|        3 | 6498 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        3 | 6499 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        3 | 6500 | `								PH7_INLINE_RESUME_BREAK()` |
|        3 | 6501 | `								VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6502 | `								{` |
|        - | 6503 | `									sxi32 iRpN;` |
|        3 | 6504 | `									if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){` |
|        3 | 6505 | `										pc = iRpN;` |
|        3 | 6506 | `										break;` |
|        - | 6507 | `									}` |
|        - | 6508 | `								}` |
|      ! 0 | 6509 | `								goto Exception;` |
|        - | 6510 | `							}` |
|        - | 6511 | `							{` |
|        - | 6512 | `								/* php's named-hole ArgumentCountError, checked BEFORE` |
|        - | 6513 | `								 * hole compaction: compacting first would report the` |
|        - | 6514 | `								 * positional wording with a fictitious count (g(b:2)` |
|        - | 6515 | ``								 * must be `g(): Argument #1 ($a) not passed`, not`` |
|        - | 6516 | `								 * "1 passed"). Implicit-required watermark, like the` |
|        - | 6517 | `								 * plain-call named path; a hole with NOTHING filled` |
|        - | 6518 | `								 * above it keeps php's count wording — fall through` |
|        - | 6519 | `								 * to VmFiberSetupFrame's check (the compacted count` |
|        - | 6520 | `								 * equals php's num_args there). */` |
|       13 | 6521 | `								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;` |
|       13 | 6522 | `								sxi32 iHole = -1;` |
|       13 | 6523 | `								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);` |
|       29 | 6524 | `								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){` |
|       19 | 6525 | `									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){` |
|       15 | 6526 | `										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);` |
|        6 | 6527 | `									}` |
|       11 | 6528 | `								}` |
|       27 | 6529 | `								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){` |
|        - | 6530 | `									sxu32 gj;` |
|       17 | 6531 | `									int bFound = 0;` |
|       23 | 6532 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       23 | 6533 | `										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }` |
|        5 | 6534 | `									}` |
|       17 | 6535 | `									if( !bFound && gHole + 1 <= nMaxFilledG ){` |
|      ! 0 | 6536 | `										iHole = (sxi32)gHole;` |
|      ! 0 | 6537 | `									}` |
|       10 | 6538 | `								}` |
|       13 | 6539 | `								if( iHole >= 0 ){` |
|      ! 0 | 6540 | `									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 6541 | `										(sxu32)iHole+1,&aFA[iHole].sName);` |
|      ! 0 | 6542 | `									SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 6543 | `									SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6544 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 6545 | `										goto Abort;` |
|        - | 6546 | `									}` |
|        - | 6547 | `									/* Route like the VmFiberSetupFrame throw below */` |
|      ! 0 | 6548 | `									PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 6549 | `									VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6550 | `									{` |
|        - | 6551 | `										sxi32 iRpH;` |
|      ! 0 | 6552 | `										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 6553 | `											pc = iRpH;` |
|      ! 0 | 6554 | `											break;` |
|        - | 6555 | `										}` |
|        - | 6556 | `									}` |
|      ! 0 | 6557 | `									goto Exception;` |
|        - | 6558 | `								}` |
|        - | 6559 | `							}` |
|        - | 6560 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - | 6561 | `							 * append overflow (variadic / positional beyond` |
|        - | 6562 | `							 * formals) so downstream sees every argument. */` |
|        - | 6563 | `							{` |
|       13 | 6564 | `								int nOut = 0;` |
|       29 | 6565 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - | 6566 | `									sxu32 gj;` |
|       25 | 6567 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       25 | 6568 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|       19 | 6569 | `											apCallArgs[nOut++] = &pArg[gj];` |
|       19 | 6570 | `											break;` |
|        - | 6571 | `										}` |
|        5 | 6572 | `									}` |
|       11 | 6573 | `								}` |
|       29 | 6574 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|       19 | 6575 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 | 6576 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 | 6577 | `									}` |
|       11 | 6578 | `								}` |
|       13 | 6579 | `								nGenArgs = nOut;` |
|        - | 6580 | `							}` |
|       13 | 6581 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|       13 | 6582 | `							didReorder = 1;` |
|        5 | 6583 | `						}` |
|        - | 6584 | `						/* If aGSlot allocation failed, fall through to` |
|        - | 6585 | `						 * positional fill below — preserves arg order rather` |
|        - | 6586 | `						 * than passing an uninitialized apCallArgs. */` |
|        5 | 6587 | `					}` |
|      115 | 6588 | `					if( !didReorder ){` |
|      211 | 6589 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|      111 | 6590 | `							apCallArgs[iArg] = &pArg[iArg];` |
|       58 | 6591 | `						}` |
|       50 | 6592 | `					}` |
|        - | 6593 | `				}` |
|       55 | 6594 | `			}` |
|        - | 6595 | `			/* Create execution context and generator wrapper */` |
|      413 | 6596 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|      413 | 6597 | `			if( pExecCtx == 0 ){` |
|      ! 0 | 6598 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6599 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 6600 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 6601 | `				break;` |
|        - | 6602 | `			}` |
|      413 | 6603 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|      413 | 6604 | `			if( pGenerator == 0 ){` |
|      ! 0 | 6605 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 | 6606 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6607 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 6608 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 6609 | `				break;` |
|        - | 6610 | `			}` |
|        - | 6611 | `			/* Set up the frame with arguments, closure env, $this */` |
|      413 | 6612 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|      413 | 6613 | `			pVm->pFrame = pExecCtx->pFrame;` |
|      821 | 6614 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,` |
|      408 | 6615 | `				pEffCallMap ? (pEffCallMap->bStrict ? 1 : 0) : (pVm->bCurStrict ? 1 : 0),` |
|      204 | 6616 | `				pSelfHint,` |
|        - | 6617 | `				TRUE/*generator: the g(...) call site is in the message*/,` |
|        - | 6618 | `				TRUE/*a source-level call binds a by-ref parameter to the caller's slot*/);` |
|      413 | 6619 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|      413 | 6620 | `			pExecCtx->pFrame->pParent = 0;` |
|      413 | 6621 | `			if( apCallArgs ){` |
|      115 | 6622 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       55 | 6623 | `			}` |
|      413 | 6624 | `			if( rc != SXRET_OK ){` |
|       18 | 6625 | `				VmReleaseGenerator(pVm, pGenerator);` |
|       18 | 6626 | `				if( pThis ){` |
|        3 | 6627 | `					PH7_ClassInstanceUnref(pThis);` |
|        1 | 6628 | `				}` |
|       18 | 6629 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 6630 | `					goto Abort;` |
|        - | 6631 | `				}` |
|       18 | 6632 | `				if( rc == PH7_EXCEPTION ){` |
|        - | 6633 | `					/* A declared-type TypeError thrown while binding the` |
|        - | 6634 | `					 * generator's arguments (php binds + type-checks eagerly at` |
|        - | 6635 | `					 * the g(...) call site, before any resume — band A #2). If` |
|        - | 6636 | `					 * an inline try THIS exec owns caught it, land at its` |
|        - | 6637 | `					 * redirect (the drain subsumes the operand pops); else pop` |
|        - | 6638 | `					 * the args + function name and route like the other` |
|        - | 6639 | `					 * OP_CALL throw paths. */` |
|       22 | 6640 | `					PH7_INLINE_RESUME_BREAK()` |
|       16 | 6641 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6642 | `					{` |
|        - | 6643 | `						sxi32 iRpG;` |
|       16 | 6644 | `						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){` |
|       16 | 6645 | `							pc = iRpG;` |
|       16 | 6646 | `							break;` |
|        - | 6647 | `						}` |
|        - | 6648 | `					}` |
|      ! 0 | 6649 | `					goto Exception;` |
|        - | 6650 | `				}` |
|      ! 0 | 6651 | `				break;` |
|        - | 6652 | `			}` |
|        - | 6653 | `			/* Create Generator class instance */` |
|      397 | 6654 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|      397 | 6655 | `			if( pGenObj == 0 ){` |
|      ! 0 | 6656 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 | 6657 | `				break;` |
|        - | 6658 | `			}` |
|        - | 6659 | `			/* Store generator in __ctx attribute */` |
|      397 | 6660 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      397 | 6661 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|      397 | 6662 | `			if( pCtxAttr ){` |
|      397 | 6663 | `				pCtxAttr->x.pOther = pGenerator;` |
|      397 | 6664 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      196 | 6665 | `			}` |
|        - | 6666 | `			/* Pop args and function name, push Generator object. PH7_NewClassInstance` |
|        - | 6667 | `			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do` |
|        - | 6668 | `			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope` |
|        - | 6669 | ``			 * and its __destruct (which runs pending `finally` blocks and frees the`` |
|        - | 6670 | `			 * exec context) never fires. */` |
|      397 | 6671 | `			PH7_MemObjRelease(pTos);` |
|      397 | 6672 | `			pTos = &pTos[-nCallArgs];` |
|      397 | 6673 | `			pTos->x.pOther = pGenObj;` |
|      397 | 6674 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|      397 | 6675 | `			if( pThis ){` |
|       16 | 6676 | `				PH7_ClassInstanceUnref(pThis);` |
|        6 | 6677 | `			}` |
|      397 | 6678 | `			break;` |
|        - | 6679 | `		}` |
|        - | 6680 | `		/* Extract the formal argument set */` |
|   763529 | 6681 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - | 6682 | `		/* Create a new VM frame  */` |
|   763529 | 6683 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|   763529 | 6684 | `		if( rc != SXRET_OK ){` |
|        - | 6685 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 6686 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 6687 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 6688 | `				&pVmFunc->sName);` |
|        - | 6689 | `			/* The frame that would own (and later release) $this never got created; for a bound` |
|        - | 6690 | `			 * plain closure the consumed transient is the object's ONLY ref, so release it here` |
|        - | 6691 | `			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */` |
|      ! 0 | 6692 | `			if( bClosureThis && pThis ){` |
|      ! 0 | 6693 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 | 6694 | `			}` |
|        - | 6695 | `			/* Pop given arguments */` |
|      ! 0 | 6696 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 6697 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6698 | `			}` |
|        - | 6699 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 | 6700 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 6701 | `			break;` |
|        - | 6702 | `		}` |
|   763529 | 6703 | `		if( pClosureScope ){` |
|        - | 6704 | `			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /` |
|        - | 6705 | `			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected` |
|        - | 6706 | `			 * access inside the body resolves against it. */` |
|       57 | 6707 | `			pFrame->pBoundScope = pClosureScope;` |
|       27 | 6708 | `		}` |
|        - | 6709 | `		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band` |
|        - | 6710 | `		 * A #4): sArg over-counts (defaulted params installed, variadic packed` |
|        - | 6711 | `		 * as one entry) so php's answers can't be derived from it. */` |
|   763529 | 6712 | `		pFrame->nActualArgs = (int)(pTos - pArg);` |
|   763529 | 6713 | `		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) \|\| bClosureThis) ){` |
|        - | 6714 | `			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */` |
|        - | 6715 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|   405023 | 6716 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|   405023 | 6717 | `			if( pObj ){` |
|        - | 6718 | `				/* Reflect the change */` |
|   405023 | 6719 | `				pObj->x.pOther = pThis;` |
|   405023 | 6720 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   202509 | 6721 | `			}` |
|   202509 | 6722 | `		}` |
|   763529 | 6723 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - | 6724 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - | 6725 | `			/* Install static variables */` |
|       47 | 6726 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       91 | 6727 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|       47 | 6728 | `				pStatic = &aStatic[n];` |
|       47 | 6729 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - | 6730 | `					/* Initialize the static variables */` |
|       27 | 6731 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|       27 | 6732 | `					if( pObj ){` |
|        - | 6733 | `						/* Assume a NULL initialization value */` |
|       27 | 6734 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|       27 | 6735 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - | 6736 | `							/* Evaluate initialization expression (Any complex expression) */` |
|       27 | 6737 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|       12 | 6738 | `						}` |
|       27 | 6739 | `						pObj->nIdx = pStatic->nIdx;` |
|        - | 6740 | `						/* Permanent pin: the storage outlives every call */` |
|       27 | 6741 | `						VmPinMemObjSlot(&(*pVm),pStatic->nIdx);` |
|       15 | 6742 | `					}else{` |
|      ! 0 | 6743 | `						continue;` |
|        - | 6744 | `					}` |
|       12 | 6745 | `				}` |
|        - | 6746 | `				/* Install in the current frame — a REGISTERED binding, and the slot is` |
|        - | 6747 | `				 * PINNED: the static's storage belongs to the function, not to this` |
|        - | 6748 | `				 * call, so neither the frame teardown nor an unset of the NAME may` |
|        - | 6749 | `				 * recycle it. Poking hVar directly left the binding invisible to the` |
|        - | 6750 | ``				 * reference table, so an array element sharing the static (`[&$s]`)`` |
|        - | 6751 | ``				 * did not count as a reference and `unset($s)` destroyed the storage —`` |
|        - | 6752 | `				 * the next call started over from the initializer. The pin is taken ONCE,` |
|        - | 6753 | `				 * where the slot is created (above). */` |
|       69 | 6754 | `				PH7_VmBindVarSlot(&(*pVm),pFrame,SyStringData(&pStatic->sName),` |
|       22 | 6755 | `					SyStringLength(&pStatic->sName),pStatic->nIdx);` |
|       25 | 6756 | `			}` |
|       22 | 6757 | `		}` |
|        - | 6758 | `		/* Push arguments in the local frame */` |
|        - | 6759 | `		{` |
|   763529 | 6760 | `		VmCallArgMap *pCallMap3 = pEffCallMap;` |
|        - | 6761 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - | 6762 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|        - | 6763 | `		/* A compiled call in a strict file always carries a map with bStrict set` |
|        - | 6764 | `		 * (GenStateAttachStrictFlag); with no map at all the call is either a` |
|        - | 6765 | `		 * compiled WEAK one — where bCurStrict is 0 for the same unit — or an` |
|        - | 6766 | `		 * ENGINE-dispatched one, whose synthetic OP_CALL has no map to carry the` |
|        - | 6767 | `		 * caller's mode. php scopes parameter coercion by the CALLING file either` |
|        - | 6768 | `		 * way, and bCurStrict is that file's mode.` |
|        - | 6769 | `		 *` |
|        - | 6770 | `		 * Unless an INTERNAL function is what reached for this callback (bCallbackWeak):` |
|        - | 6771 | `		 * php has no calling file at that boundary and binds weakly, so` |
|        - | 6772 | ``		 * `array_map('takesInt', ["5"])` from a strict file RUNS there — PHL raised a`` |
|        - | 6773 | `		 * TypeError on valid php, because the ambient bCurStrict was still the strict` |
|        - | 6774 | `		 * caller's. call_user_func / call_user_func_array are php's two forwards and` |
|        - | 6775 | `		 * carry the caller's mode on a map instead. */` |
|  1145068 | 6776 | `		int bCallIsStrict = pCallMap3 ? (pCallMap3->bStrict ? 1 : 0)` |
|   701038 | 6777 | `		                   : (bCallbackWeak ? 0 : (pVm->bCurStrict ? 1 : 0));` |
|   763529 | 6778 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - | 6779 | `			/* ============================================================` |
|        - | 6780 | `			 * Named-argument matching path (PHP 8.0)` |
|        - | 6781 | `			 *` |
|        - | 6782 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - | 6783 | `			 * or position, then install them in the frame.` |
|        - | 6784 | `			 * ============================================================ */` |
|      385 | 6785 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      385 | 6786 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|      385 | 6787 | `			sxi32 iVariadicIdx = -1;` |
|        - | 6788 | `			sxu32 nNonVariadic;` |
|        - | 6789 | `			sxi32 *aSlot;` |
|        - | 6790 | `			sxu8  *aUsed;` |
|        - | 6791 | `			sxu32 i;` |
|        - | 6792 | `			/* Find variadic parameter index */` |
|      993 | 6793 | `			for( i = 0; i < nFormal; i++ ){` |
|      719 | 6794 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|      111 | 6795 | `					iVariadicIdx = (sxi32)i;` |
|      111 | 6796 | `					break;` |
|        - | 6797 | `				}` |
|      308 | 6798 | `			}` |
|      385 | 6799 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - | 6800 | `			/* Allocate mapping arrays */` |
|      575 | 6801 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      380 | 6802 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|      385 | 6803 | `			if( aSlot == 0 ){` |
|      ! 0 | 6804 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 | 6805 | `				goto Abort;` |
|        - | 6806 | `			}` |
|      385 | 6807 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - | 6808 | `			/* Resolve named arguments to formal parameters */` |
|      575 | 6809 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|      190 | 6810 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|      385 | 6811 | `			if( rc == PH7_ABORT ){` |
|        8 | 6812 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 | 6813 | `				goto Abort;` |
|        - | 6814 | `			}` |
|      379 | 6815 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 6816 | `				/* php's catchable \Error for a bad named argument. The callee` |
|        - | 6817 | `				 * frame is already entered but its body must not run: unwind` |
|        - | 6818 | `				 * exactly like the named-hole ArgumentCountError path below` |
|        - | 6819 | `				 * (release the not-yet-installed actuals — Pass 2's release` |
|        - | 6820 | `				 * loop has not run — pop the callee slot, mark that there is no` |
|        - | 6821 | `				 * callee operand stack, and let VmCallFinish route the throw). */` |
|        - | 6822 | `				sxu32 iRel;` |
|        5 | 6823 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       11 | 6824 | `				for( iRel = 0; iRel < nActual; iRel++ ){` |
|        7 | 6825 | `					PH7_MemObjRelease(&pArg[iRel]);` |
|        4 | 6826 | `				}` |
|        5 | 6827 | `				PH7_MemObjRelease(pTos);` |
|        5 | 6828 | `				pTos = &pTos[-nCallArgs];` |
|        5 | 6829 | `				pFrameStack = 0;` |
|        5 | 6830 | `				goto SkipFuncBody;` |
|        - | 6831 | `			}` |
|        - | 6832 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|        - | 6833 | `			{` |
|        - | 6834 | `			/* php's required watermark for the hole check below, plus the` |
|        - | 6835 | `			 * highest formal slot an actual resolved to: php words a hole` |
|        - | 6836 | ``			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole`` |
|        - | 6837 | `			 * with nothing filled above it gets the positional count message` |
|        - | 6838 | `			 * (zend's RECV arg_num > EX(num_args) distinction). */` |
|        - | 6839 | `			sxu32 nReqNamed;` |
|        - | 6840 | `			sxu32 nNVNamed;` |
|      375 | 6841 | `			sxu32 nMaxFilled = 0;` |
|      375 | 6842 | `			nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);` |
|     1299 | 6843 | `			for( i = 0; i < nActual; i++ ){` |
|      929 | 6844 | `				if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|      385 | 6845 | `					nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|      191 | 6846 | `				}` |
|      467 | 6847 | `			}` |
|      953 | 6848 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - | 6849 | `				/* Find the stack arg mapped to formal n */` |
|      593 | 6850 | `				sxi32 iSrc = -1;` |
|      939 | 6851 | `				for( i = 0; i < nActual; i++ ){` |
|      807 | 6852 | `					if( aSlot[i] == (sxi32)n ){` |
|      461 | 6853 | `						iSrc = (sxi32)i;` |
|      461 | 6854 | `						break;` |
|        - | 6855 | `					}` |
|      176 | 6856 | `				}` |
|      593 | 6857 | `				if( iSrc >= 0 ){` |
|        - | 6858 | `					/* Argument was provided — install with type checking */` |
|      461 | 6859 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - | 6860 | `					/* An explicit null is NOT redirected to the default: PHP applies a` |
|        - | 6861 | `					 * default only for an OMITTED argument. An explicit null falls through` |
|        - | 6862 | `					 * to the type check below (TypeError for a non-nullable typed param,` |
|        - | 6863 | ``					 * kept as null for a typeless one). An implicitly-nullable `Type $x =`` |
|        - | 6864 | ``					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */`` |
|        - | 6865 | `					/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 6866 | `					 * coercion and whole-real materialization in place): the shared` |
|        - | 6867 | `					 * per-argument helper — one implementation for both OP_CALL` |
|        - | 6868 | `					 * paths and the generator/fiber binder (§7.1(f) fold). */` |
|      461 | 6869 | `					iArgPreFlags = pVal->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|      461 | 6870 | `					rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pVal,bCallIsStrict,pSelfHint);` |
|      461 | 6871 | `					if( rc != SXRET_OK ){` |
|        7 | 6872 | `						if( rc == PH7_ABORT ) goto Abort;` |
|        7 | 6873 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 | 6874 | `						PH7_MemObjRelease(pTos);` |
|        7 | 6875 | `						pTos = &pTos[-nCallArgs];` |
|        7 | 6876 | `						pFrameStack = 0;` |
|        7 | 6877 | `						rc = PH7_EXCEPTION;` |
|       10 | 6878 | `						goto SkipFuncBody;` |
|        - | 6879 | `					}` |
|        - | 6880 | `					/* Install: by reference or by value */` |
|      455 | 6881 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|       28 | 6882 | `						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){` |
|        - | 6883 | `							/* php 8.1: $GLOBALS cannot be passed by reference */` |
|        - | 6884 | `							SyBlob sMsg;` |
|      ! 0 | 6885 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 6886 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 6887 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 6888 | `							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|      ! 0 | 6889 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 6890 | `								goto Abort;` |
|        - | 6891 | `							}` |
|      ! 0 | 6892 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 6893 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 6894 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 6895 | `							pFrameStack = 0;` |
|      ! 0 | 6896 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 6897 | `							goto SkipFuncBody;` |
|        - | 6898 | `						}` |
|       28 | 6899 | `						if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)iSrc,pVal) ){` |
|        - | 6900 | `							/* php refuses a by-ref argument whose EXPRESSION is not a variable, at` |
|        - | 6901 | `							 * the call and before the callee runs. Deciding it from the VALUE that` |
|        - | 6902 | `							 * arrived was wrong both ways: an operator result carries its LEFT` |
|        - | 6903 | ``							 * operand's slot, so `f($i + 1)` aliased and overwrote `$i`; and a`` |
|        - | 6904 | `							 * literal and a CALL result look alike there, where php accepts the` |
|        - | 6905 | `							 * call. VmArgRefusedByRef reads the call site's compile-time shape mask` |
|        - | 6906 | `							 * and falls back to the old runtime test only when there is none. */` |
|        - | 6907 | `							sxi32 rcRef;` |
|        3 | 6908 | `							rcRef = VmThrowByRefRefusal(&(*pVm),` |
|        2 | 6909 | `								(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|        2 | 6910 | `								&pVmFunc->sName,(sxu32)(n+1),&aFormalArg[n].sName);` |
|        3 | 6911 | `							if( rcRef == PH7_ABORT ){` |
|      ! 0 | 6912 | `								goto Abort;` |
|        - | 6913 | `							}` |
|        - | 6914 | `							/* Same teardown as the type-check refusal above: free the slot map,` |
|        - | 6915 | `							 * release the result slot and pop the actuals, then let SkipFuncBody` |
|        - | 6916 | `							 * route the throw. */` |
|        3 | 6917 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 6918 | `							PH7_MemObjRelease(pTos);` |
|        3 | 6919 | `							pTos = &pTos[-nCallArgs];` |
|        3 | 6920 | `							pFrameStack = 0;` |
|        3 | 6921 | `							rc = PH7_EXCEPTION;` |
|        3 | 6922 | `							goto SkipFuncBody;` |
|        - | 6923 | `						}` |
|       25 | 6924 | `						PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)iSrc,pVal);` |
|       25 | 6925 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 | 6926 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 6927 | `						}else{` |
|        - | 6928 | `							SyHashEntry *pRefEntry;` |
|        - | 6929 | `							/* The declared type's conversion is what the reference holds. */` |
|       25 | 6930 | `							PH7_VmByRefArgWriteBack(&(*pVm),pVal,iArgPreFlags);` |
|       37 | 6931 | `							pRefEntry = SyHashGet(&pFrame->hVar,` |
|       24 | 6932 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 | 6933 | `							if( pRefEntry == 0 ){` |
|       37 | 6934 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 | 6935 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|       25 | 6936 | `								sArg.nIdx = pVal->nIdx;` |
|       25 | 6937 | `								sArg.pUserData = 0;` |
|       25 | 6938 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 | 6939 | `							}` |
|       25 | 6940 | `							pObj = 0;` |
|        - | 6941 | `						}` |
|       13 | 6942 | `					}else{` |
|      429 | 6943 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 6944 | `					}` |
|      453 | 6945 | `					if( pObj ){` |
|      429 | 6946 | `						PH7_MemObjStore(pVal,pObj);` |
|      429 | 6947 | `						sArg.nIdx = pObj->nIdx;` |
|      429 | 6948 | `						sArg.pUserData = 0;` |
|      429 | 6949 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      213 | 6950 | `					}` |
|      228 | 6951 | `				}else{` |
|        - | 6952 | `					/* Argument was NOT provided — use default or leave unset */` |
|      135 | 6953 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 6954 | `						/* Should not reach here; variadic handled separately below */` |
|      135 | 6955 | `					}else if( n < nReqNamed ){` |
|        - | 6956 | `						/* php's implicit-required rule applies to named calls` |
|        - | 6957 | `						 * too: a hole below the required watermark throws even` |
|        - | 6958 | `						 * when the formal carries a default — f($a,$b=2,$c)` |
|        - | 6959 | ``						 * called as f(a:1,c:3) is `Argument #2 ($b) not`` |
|        - | 6960 | ``						 * passed` (a hole with NO default is always below the`` |
|        - | 6961 | `						 * watermark, so this subsumes the no-default case).` |
|        - | 6962 | `						 * A hole with nothing filled ABOVE it uses php's` |
|        - | 6963 | `						 * positional count wording instead. The passed stack` |
|        - | 6964 | `						 * args were not released yet on this path (that loop` |
|        - | 6965 | `						 * runs after Pass 2) — release them before the exit. */` |
|        5 | 6966 | `						if( n + 1 > nMaxFilled ){` |
|        3 | 6967 | `							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)` |
|      ! 0 | 6968 | `								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 6969 | `									nMaxFilled,nReqNamed,SySetUsed(&pVmFunc->aArgs))` |
|        3 | 6970 | `								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        1 | 6971 | `									nMaxFilled,nReqNamed,nNVNamed,TRUE);` |
|        2 | 6972 | `						}else{` |
|        3 | 6973 | `							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        - | 6974 | `						}` |
|        5 | 6975 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        9 | 6976 | `						for( i = 0; i < nActual; i++ ){` |
|        5 | 6977 | `							PH7_MemObjRelease(&pArg[i]);` |
|        3 | 6978 | `						}` |
|        5 | 6979 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 6980 | `							goto Abort;` |
|        - | 6981 | `						}` |
|        5 | 6982 | `						PH7_MemObjRelease(pTos);` |
|        5 | 6983 | `						pTos = &pTos[-nCallArgs];` |
|        5 | 6984 | `						pFrameStack = 0;` |
|        5 | 6985 | `						rc = PH7_EXCEPTION;` |
|        5 | 6986 | `						goto SkipFuncBody;` |
|      131 | 6987 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      131 | 6988 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      131 | 6989 | `						if( pObj ){` |
|      131 | 6990 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|      131 | 6991 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      131 | 6992 | `							sArg.nIdx = pObj->nIdx;` |
|      131 | 6993 | `							sArg.pUserData = 0;` |
|      131 | 6994 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 6995 | `							/* A null default on an implicitly-nullable param must stay null` |
|        - | 6996 | `							 * (see the positional-path note above). */` |
|      128 | 6997 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|       42 | 6998 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0` |
|       24 | 6999 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 7000 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 7001 | `								if( xCast ) xCast(pObj);` |
|      ! 0 | 7002 | `							}else{` |
|        - | 7003 | `								/* Mask matched — a const-indirected whole-real default` |
|        - | 7004 | ``								 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|      131 | 7005 | `								VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 7006 | `							}` |
|       64 | 7007 | `						}` |
|       64 | 7008 | `					}` |
|        - | 7009 | `				}` |
|      292 | 7010 | `			}` |
|        - | 7011 | `			} /* end nReqNamed scope */` |
|        - | 7012 | `			/* Handle variadic parameter */` |
|      363 | 7013 | `			if( iVariadicIdx >= 0 ){` |
|      111 | 7014 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|      111 | 7015 | `				if( pObj ){` |
|        - | 7016 | `					/* Capture the slot index now: PH7_HashmapInsert below can` |
|        - | 7017 | `					 * PH7_ReserveMemObj, reallocating pVm->aMemObj and dangling pObj` |
|        - | 7018 | `					 * (same latent UAF the positional path guards against). */` |
|        - | 7019 | `					sxu32 nVariadicSlot;` |
|      111 | 7020 | `					PH7_MemObjToHashmap(pObj);` |
|      111 | 7021 | `					nVariadicSlot = pObj->nIdx;` |
|        - | 7022 | `					{` |
|      111 | 7023 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - | 7024 | `						/* php numbers a failing NAMED variadic element as` |
|        - | 7025 | `						 * max(total positional args, declared non-variadic` |
|        - | 7026 | `						 * formals) + 1 — zend's RECV slots always count —` |
|        - | 7027 | `						 * whichever named element fails; a POSITIONAL element` |
|        - | 7028 | `						 * uses its own 1-based call position. */` |
|      111 | 7029 | `						sxu32 nPositional = 0;` |
|      623 | 7030 | `						for( i = 0; i < nActual; i++ ){` |
|      517 | 7031 | `							if( !(i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0) ){` |
|      333 | 7032 | `								nPositional++;` |
|      165 | 7033 | `							}` |
|      261 | 7034 | `						}` |
|      579 | 7035 | `						for( i = 0; i < nActual; i++ ){` |
|      511 | 7036 | `							if( aSlot[i] == -1 ){` |
|      465 | 7037 | `								int bNamed = (i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0);` |
|      465 | 7038 | `								int bRefElem = 0; /* alias this entry to the caller's slot? */` |
|        - | 7039 | `								/* Same per-element type check + weak coercion as the` |
|        - | 7040 | ``								 * positional-only path (shared helper; no `($name)`). */`` |
|      695 | 7041 | `								rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|      460 | 7042 | `									&aFormalArg[iVariadicIdx],&pArg[i],` |
|      307 | 7043 | `									bNamed ? SXMAX(nPositional,nNonVariadic) + 1 : i + 1,bCallIsStrict);` |
|      465 | 7044 | `								if( rc != SXRET_OK ){` |
|       39 | 7045 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 7046 | `										goto Abort;` |
|        - | 7047 | `									}` |
|       39 | 7048 | `									SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       39 | 7049 | `									PH7_MemObjRelease(pTos);` |
|       39 | 7050 | `									pTos = &pTos[-nCallArgs];` |
|       39 | 7051 | `									pFrameStack = 0;` |
|       39 | 7052 | `									rc = PH7_EXCEPTION;` |
|       39 | 7053 | `									goto SkipFuncBody;` |
|        - | 7054 | `								}` |
|      429 | 7055 | `								if( aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7056 | `									/* php screens a by-ref VARIADIC tail per collected element, in its` |
|        - | 7057 | `									 * no-name wording: a variadic has no per-element parameter name, so` |
|        - | 7058 | ``									 * php says `Argument #N could not be passed by reference` and stops`` |
|        - | 7059 | `									 * there. Nothing screened this arm at all — the branch that collects` |
|        - | 7060 | `									 * a variadic runs before the by-ref binder ever sees a formal. */` |
|        8 | 7061 | `									if( PH7_VmArgRefusedByRef(pCallMap3,i,&pArg[i]) ){` |
|        - | 7062 | `										SyBlob sMsgV;` |
|        - | 7063 | `										sxi32 rcV;` |
|        3 | 7064 | `										SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        3 | 7065 | `										SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        2 | 7066 | `											&pVmFunc->sName,(unsigned)(i + 1));` |
|        3 | 7067 | `										rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        3 | 7068 | `										if( rcV == PH7_ABORT ){` |
|      ! 0 | 7069 | `											goto Abort;` |
|        - | 7070 | `										}` |
|        3 | 7071 | `										SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 7072 | `										PH7_MemObjRelease(pTos);` |
|        3 | 7073 | `										pTos = &pTos[-nCallArgs];` |
|        3 | 7074 | `										pFrameStack = 0;` |
|        3 | 7075 | `										rc = PH7_EXCEPTION;` |
|        3 | 7076 | `										goto SkipFuncBody;` |
|        - | 7077 | `									}` |
|        5 | 7078 | `									PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,i,&pArg[i]);` |
|        2 | 7079 | `								}` |
|        - | 7080 | `								/* A by-ref variadic tail ALIASES its actuals, named entries` |
|        - | 7081 | `								 * included (the positional twin below this branch says why). */` |
|      639 | 7082 | `								bRefElem = (aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF)` |
|      422 | 7083 | `									&& pArg[i].nIdx != SXU32_HIGH;` |
|      426 | 7084 | `								if( bNamed ){` |
|        - | 7085 | `									/* Named variadic entry: insert with string key */` |
|        - | 7086 | `									ph7_value sKey;` |
|      124 | 7087 | `									PH7_MemObjInit(pVm, &sKey);` |
|      124 | 7088 | `									PH7_MemObjStringAppend(&sKey,` |
|      120 | 7089 | `										pCallMap3->aNames[i].zString,` |
|      120 | 7090 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|      124 | 7091 | `									if( bRefElem ){` |
|        5 | 7092 | `										PH7_HashmapInsertByRef(pVarMap, &sKey, pArg[i].nIdx);` |
|        3 | 7093 | `									}else{` |
|      120 | 7094 | `										PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|        - | 7095 | `									}` |
|      124 | 7096 | `									PH7_MemObjRelease(&sKey);` |
|      365 | 7097 | `								}else if( bRefElem ){` |
|        - | 7098 | `									/* Positional variadic entry, aliased */` |
|      ! 0 | 7099 | `									PH7_HashmapInsertByRef(pVarMap, 0, pArg[i].nIdx);` |
|      ! 0 | 7100 | `								}else{` |
|        - | 7101 | `									/* Positional variadic entry */` |
|      305 | 7102 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - | 7103 | `								}` |
|      211 | 7104 | `							}` |
|      238 | 7105 | `						}` |
|        - | 7106 | `					}` |
|       72 | 7107 | `					sArg.nIdx = nVariadicSlot; /* pObj may be stale here (aMemObj realloc) */` |
|       72 | 7108 | `					sArg.pUserData = 0;` |
|       72 | 7109 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       34 | 7110 | `				}` |
|       38 | 7111 | `			}else{` |
|        - | 7112 | `				/* No variadic — preserve unresolved positional overflow` |
|        - | 7113 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - | 7114 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - | 7115 | `				 * the positional-only path's behavior. */` |
|      254 | 7116 | `				sxu32 nAnon = nNonVariadic;` |
|      652 | 7117 | `				for( i = 0; i < nActual; i++ ){` |
|      400 | 7118 | `					if( aSlot[i] == -2 ){` |
|        - | 7119 | `						char zAnonBuf[32];` |
|        - | 7120 | `						SyString sAnonName;` |
|      ! 0 | 7121 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 | 7122 | `							"[%u]apArg",nAnon);` |
|      ! 0 | 7123 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 | 7124 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 | 7125 | `						if( pObj ){` |
|      ! 0 | 7126 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 | 7127 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 | 7128 | `							sArg.pUserData = 0;` |
|      ! 0 | 7129 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 | 7130 | `						}` |
|      ! 0 | 7131 | `						nAnon++;` |
|      ! 0 | 7132 | `					}` |
|      201 | 7133 | `				}` |
|        - | 7134 | `			}` |
|        - | 7135 | `			/* Release all stack arguments */` |
|     1160 | 7136 | `			for( i = 0; i < nActual; i++ ){` |
|      840 | 7137 | `				PH7_MemObjRelease(&pArg[i]);` |
|      422 | 7138 | `			}` |
|      324 | 7139 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - | 7140 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|      324 | 7141 | `			n = nFormal;` |
|      164 | 7142 | `		}else{` |
|        - | 7143 | `		/* ============================================================` |
|        - | 7144 | `		 * Positional-only matching path (original)` |
|        - | 7145 | `		 * ============================================================ */` |
|        - | 7146 | `		/* Base of the actual argument stack, for php's per-ELEMENT argument` |
|        - | 7147 | `		 * number in a variadic type-error (php numbers a variadic-collected` |
|        - | 7148 | `		 * element by its overall 1-based call position, not the formal index). */` |
|   763149 | 7149 | `		ph7_value *pArgBase = pArg;` |
|   763149 | 7150 | `		n = 0;` |
|  1021828 | 7151 | `		while( pArg < pTos ){` |
|   263578 | 7152 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - | 7153 | `				/* Variadic parameter: collect all remaining args into an array */` |
|      597 | 7154 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      597 | 7155 | `				if( pObj ){` |
|        - | 7156 | `					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,` |
|        - | 7157 | `					 * reallocating pVm->aMemObj and dangling pObj — so don't read pObj->nIdx after` |
|        - | 7158 | `					 * the packing loop (pre-existing UAF, masked by the pool allocator). pMap is a` |
|        - | 7159 | `					 * separately-allocated hashmap and stays valid across the realloc. */` |
|        - | 7160 | `					sxu32 nVariadicIdx;` |
|        - | 7161 | `					/* Initialize as empty array */` |
|      597 | 7162 | `					PH7_MemObjToHashmap(pObj);` |
|      597 | 7163 | `					nVariadicIdx = pObj->nIdx;` |
|        - | 7164 | `					{` |
|      597 | 7165 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     2711 | 7166 | `						while( pArg < pTos ){` |
|        - | 7167 | `							/* Per-element type check + weak coercion (shared helper,` |
|        - | 7168 | `							 * also used by the named-argument path). The argument` |
|        - | 7169 | `							 * number is the element's overall 1-based call position` |
|        - | 7170 | `` 							 * ((pArg - pArgBase) + 1) and, like php, the `($name)` `` |
|        - | 7171 | `							 * clause is omitted. */` |
|     3245 | 7172 | `							rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|     2160 | 7173 | `								&aFormalArg[n],pArg,(sxu32)(pArg - pArgBase) + 1,bCallIsStrict);` |
|     2165 | 7174 | `							if( rc != SXRET_OK ){` |
|       43 | 7175 | `								if( rc == PH7_ABORT ){` |
|      ! 0 | 7176 | `									goto Abort;` |
|        - | 7177 | `								}` |
|        - | 7178 | `								/* Skip function body, route through normal cleanup */` |
|       43 | 7179 | `								PH7_MemObjRelease(pTos);` |
|       43 | 7180 | `								pTos = &pTos[-nCallArgs];` |
|       43 | 7181 | `								pFrameStack = 0;` |
|       43 | 7182 | `								rc = PH7_EXCEPTION;` |
|       43 | 7183 | `								goto SkipFuncBody;` |
|        - | 7184 | `							}` |
|     2125 | 7185 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7186 | `								/* The positional twin of the named path's variadic screen above:` |
|        - | 7187 | `								 * php refuses a non-variable collected into a by-ref variadic tail,` |
|        - | 7188 | `								 * in its no-name wording. */` |
|       52 | 7189 | `								sxu32 nPosV = (sxu32)(pArg - pArgBase);` |
|       52 | 7190 | `								if( PH7_VmArgRefusedByRef(pCallMap3,nPosV,pArg) ){` |
|        - | 7191 | `									SyBlob sMsgV;` |
|        - | 7192 | `									sxi32 rcV;` |
|        7 | 7193 | `									SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        7 | 7194 | `									SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        6 | 7195 | `										&pVmFunc->sName,(unsigned)(nPosV + 1));` |
|        7 | 7196 | `									rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        7 | 7197 | `									if( rcV == PH7_ABORT ){` |
|      ! 0 | 7198 | `										goto Abort;` |
|        - | 7199 | `									}` |
|        7 | 7200 | `									PH7_MemObjRelease(pTos);` |
|        7 | 7201 | `									pTos = &pTos[-nCallArgs];` |
|        7 | 7202 | `									pFrameStack = 0;` |
|        7 | 7203 | `									rc = PH7_EXCEPTION;` |
|        7 | 7204 | `									goto SkipFuncBody;` |
|        - | 7205 | `								}` |
|       46 | 7206 | `								PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,nPosV,pArg);` |
|       46 | 7207 | `								if( pArg->nIdx != SXU32_HIGH ){` |
|        - | 7208 | `									/* php ALIASES each collected element to the caller's slot:` |
|        - | 7209 | ``									 * `function f(&...$xs){ $xs[0] = 'A'; }` writes back, and`` |
|        - | 7210 | ``									 * var_dump($xs) inside the callee shows `&int(1)`. Copying`` |
|        - | 7211 | `									 * them left every actual untouched. The node counts as a` |
|        - | 7212 | `									 * holder of the caller's slot, so the frame teardown that` |
|        - | 7213 | `									 * destroys the variadic array gives the hold back. */` |
|       42 | 7214 | `									PH7_HashmapInsertByRef(pMap, 0, pArg->nIdx);` |
|       42 | 7215 | `									pArg++;` |
|       42 | 7216 | `									continue;` |
|        - | 7217 | `								}` |
|        2 | 7218 | `							}` |
|     2079 | 7219 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|     2079 | 7220 | `							pArg++;` |
|        5 | 7221 | `						}` |
|        - | 7222 | `					}` |
|      551 | 7223 | `					sArg.nIdx = nVariadicIdx; /* pObj may be stale here (aMemObj realloc) — use the saved index */` |
|      551 | 7224 | `					sArg.pUserData = 0;` |
|      551 | 7225 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      273 | 7226 | `				}` |
|      551 | 7227 | `				break; /* All remaining args consumed */` |
|        - | 7228 | `			}` |
|   262986 | 7229 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 7230 | `				/* An explicit null is NOT redirected to the default (PHP applies a` |
|        - | 7231 | `				 * default only for an omitted arg); it falls through to the type check` |
|        - | 7232 | `				 * below — TypeError for a non-nullable typed param, kept as null for a` |
|        - | 7233 | ``				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE`` |
|        - | 7234 | `				 * at compile time so its check accepts null. */` |
|        - | 7235 | `				/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 7236 | `				 * coercion and whole-real materialization in place — nullable` |
|        - | 7237 | `				 * types (?type) let null through): the shared per-argument` |
|        - | 7238 | `				 * helper, one implementation for both OP_CALL paths and the` |
|        - | 7239 | `				 * generator/fiber binder (§7.1(f) fold). */` |
|   260046 | 7240 | `				iArgPreFlags = pArg->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|   260046 | 7241 | `				rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pArg,bCallIsStrict,pSelfHint);` |
|   260046 | 7242 | `				if( rc != SXRET_OK ){` |
|      287 | 7243 | `					if( rc == PH7_ABORT ){` |
|        6 | 7244 | `						goto Abort;` |
|        - | 7245 | `					}` |
|        - | 7246 | `					/* Skip function body, route through normal cleanup */` |
|      283 | 7247 | `					PH7_MemObjRelease(pTos);` |
|      283 | 7248 | `					pTos = &pTos[-nCallArgs];` |
|      283 | 7249 | `					pFrameStack = 0;` |
|      283 | 7250 | `					rc = PH7_EXCEPTION;` |
|      283 | 7251 | `					goto SkipFuncBody;` |
|        - | 7252 | `				}` |
|   259764 | 7253 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7254 | `					/* Pass by reference */` |
|     7001 | 7255 | `					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){` |
|        - | 7256 | `						/* php 8.1: $GLOBALS cannot be passed by reference —` |
|        - | 7257 | `						 * a catchable Error with php's exact wording. */` |
|        - | 7258 | `						SyBlob sMsg;` |
|        3 | 7259 | `						SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 7260 | `						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|        2 | 7261 | `							&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        3 | 7262 | `						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 7263 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 7264 | `							goto Abort;` |
|        - | 7265 | `						}` |
|        3 | 7266 | `						PH7_MemObjRelease(pTos);` |
|        3 | 7267 | `						pTos = &pTos[-nCallArgs];` |
|        3 | 7268 | `						pFrameStack = 0;` |
|        3 | 7269 | `						rc = PH7_EXCEPTION;` |
|        3 | 7270 | `						goto SkipFuncBody;` |
|        - | 7271 | `					}` |
|     6999 | 7272 | `					if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)n,pArg) ){` |
|        - | 7273 | `						/* php's refusal, decided from the argument's compile-time SHAPE (the` |
|        - | 7274 | `						 * companion of the named-argument binder above; see VmArgRefusedByRef). */` |
|        - | 7275 | `						sxi32 rcRef;` |
|     6028 | 7276 | `						rcRef = VmThrowByRefRefusal(&(*pVm),` |
|     4018 | 7277 | `							(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|     4018 | 7278 | `							&pVmFunc->sName,(sxu32)(n+1),&aFormalArg[n].sName);` |
|     4019 | 7279 | `						if( rcRef == PH7_ABORT ){` |
|      ! 0 | 7280 | `							goto Abort;` |
|        - | 7281 | `						}` |
|        - | 7282 | `						/* Route the throw like every other binder refusal: release the result` |
|        - | 7283 | `						 * slot, pop the actuals and let SkipFuncBody finish the call. Returning` |
|        - | 7284 | `						 * from here walked out of the dispatch loop with the callee's frame and` |
|        - | 7285 | `						 * stack still live, so a CAUGHT refusal silently abandoned every` |
|        - | 7286 | `						 * statement after the catch. */` |
|     4019 | 7287 | `						PH7_MemObjRelease(pTos);` |
|     4019 | 7288 | `						pTos = &pTos[-nCallArgs];` |
|     4019 | 7289 | `						pFrameStack = 0;` |
|     4019 | 7290 | `						rc = PH7_EXCEPTION;` |
|     4019 | 7291 | `						goto SkipFuncBody;` |
|        - | 7292 | `					}` |
|     2981 | 7293 | `					PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)n,pArg);` |
|     2981 | 7294 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - | 7295 | `						/* Nothing to alias: pass by value. */` |
|       93 | 7296 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 | 7297 | `					}else{` |
|        - | 7298 | `						SyHashEntry *pRefEntry;` |
|        - | 7299 | `						/* The declared type's conversion is what the reference holds. */` |
|     2891 | 7300 | `						PH7_VmByRefArgWriteBack(&(*pVm),pArg,iArgPreFlags);` |
|        - | 7301 | `						/* Install the referenced variable in the private function frame */` |
|     2891 | 7302 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|     2891 | 7303 | `						if( pRefEntry == 0 ){` |
|     4334 | 7304 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|     2886 | 7305 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|     2891 | 7306 | `							sArg.nIdx = pArg->nIdx;` |
|     2891 | 7307 | `							sArg.pUserData = 0;` |
|     2891 | 7308 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     1443 | 7309 | `						}` |
|     2891 | 7310 | `						pObj = 0;` |
|        - | 7311 | `					}` |
|     1493 | 7312 | `				}else{` |
|        - | 7313 | `					/* Pass by value,make a copy of the given argument */` |
|   252768 | 7314 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 7315 | `				}` |
|   128319 | 7316 | `			}else{` |
|        - | 7317 | `				char zName[32];` |
|        - | 7318 | `				SyString sArgName;` |
|        - | 7319 | `				/* Set a dummy name */` |
|     2945 | 7320 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|     2945 | 7321 | `				sArgName.zString = zName;` |
|        - | 7322 | `				/* Annonymous argument */` |
|     2945 | 7323 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 7324 | `			}` |
|   258684 | 7325 | `			if( pObj ){` |
|   255798 | 7326 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 7327 | `				/* Insert argument index  */` |
|   255798 | 7328 | `				sArg.nIdx = pObj->nIdx;` |
|   255798 | 7329 | `				sArg.pUserData = 0;` |
|   255798 | 7330 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|   128341 | 7331 | `			}` |
|   258684 | 7332 | `			PH7_MemObjRelease(pArg);` |
|   258684 | 7333 | `			pArg++;` |
|   258684 | 7334 | `			++n;` |
|        5 | 7335 | `		}` |
|        - | 7336 | `		} /* end named vs positional branch */` |
|        - | 7337 | `		/* Set up closure environment */` |
|   759121 | 7338 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 7339 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 7340 | `			ph7_value *pValue;` |
|        - | 7341 | `			sxu32 iEnv;` |
|    14313 | 7342 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|    31731 | 7343 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|    17423 | 7344 | `				pEnv = &aEnv[iEnv];` |
|    17423 | 7345 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 7346 | `					/* Do not install null value */` |
|    13985 | 7347 | `					continue;` |
|        - | 7348 | `				}` |
|     3438 | 7349 | `				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       13 | 7350 | `				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|        - | 7351 | `					/* The Closure instance carries an explicit bound $this` |
|        - | 7352 | `					 * (bindTo/bind/call): it wins over the creation-time` |
|        - | 7353 | `					 * captured $this, php-exact. */` |
|        7 | 7354 | `					continue;` |
|        - | 7355 | `				}` |
|     3437 | 7356 | `				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|        - | 7357 | `					/* Captured by reference: link the name to the shared slot` |
|        - | 7358 | `					 * (no copy), mirroring the by-ref argument install above. */` |
|      807 | 7359 | `					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|     1208 | 7360 | `						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),` |
|      802 | 7361 | `							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|      401 | 7362 | `					}` |
|      807 | 7363 | `					continue;` |
|        - | 7364 | `				}` |
|     2635 | 7365 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|     2635 | 7366 | `				if( pValue == 0 ){` |
|      ! 0 | 7367 | `					continue;` |
|        - | 7368 | `				}` |
|        - | 7369 | `				/* Invalidate any prior representation */` |
|     2635 | 7370 | `				PH7_MemObjRelease(pValue);` |
|        - | 7371 | `				/* Duplicate bound variable value */` |
|     2635 | 7372 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|     1320 | 7373 | `			}` |
|     7154 | 7374 | `		}` |
|        - | 7375 | `		/* Too-few-arguments check, placed AFTER the passed arguments were` |
|        - | 7376 | `		 * installed and type-checked: php's RECV order means a type error on` |
|        - | 7377 | ``		 * a PASSED argument beats the count error (`f(int $x,$y)` called`` |
|        - | 7378 | `		 * f("str") is a TypeError, not ArgumentCountError). The passed args` |
|        - | 7379 | `		 * were already released by the install loop, so the standard throw` |
|        - | 7380 | `		 * exit leaks nothing. The named path never fires this (its per-hole` |
|        - | 7381 | `		 * check ran in-loop; n == nNonVariadic >= nRequired here).` |
|        - | 7382 | `		 * Builtins written as embedded PHP in the prelude (VM_FUNC_INTERNAL,` |
|        - | 7383 | `		 * with or without VM_FUNC_CLASS_METHOD) are NOT exempt: their declared` |
|        - | 7384 | `		 * signatures were swept against the php 8.5 oracle, so the derived` |
|        - | 7385 | `		 * required count and wording are php's, and VmThrowBuiltinTooFewArgs` |
|        - | 7386 | `		 * words them as php words an internal callable. */` |
|   759121 | 7387 | `		if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 7388 | `			sxu32 nNonVar,nReq;` |
|     5224 | 7389 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     5224 | 7390 | `			if( n < nReq ){` |
|       57 | 7391 | `				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;` |
|       57 | 7392 | `				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|       52 | 7393 | `					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       17 | 7394 | `						nPassed,nReq,SySetUsed(&pVmFunc->aArgs));` |
|       18 | 7395 | `				}else{` |
|       33 | 7396 | `					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       10 | 7397 | `						nPassed,nReq,nNonVar,TRUE);` |
|        - | 7398 | `				}` |
|       57 | 7399 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 7400 | `					goto Abort;` |
|        - | 7401 | `				}` |
|       57 | 7402 | `				PH7_MemObjRelease(pTos);` |
|       57 | 7403 | `				pTos = &pTos[-nCallArgs];` |
|       57 | 7404 | `				pFrameStack = 0;` |
|       57 | 7405 | `				rc = PH7_EXCEPTION;` |
|       57 | 7406 | `				goto SkipFuncBody;` |
|        5 | 7407 | `			}` |
|   756484 | 7408 | `		}else if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) == VM_FUNC_INTERNAL ){` |
|        - | 7409 | `			/* Too-MANY arguments, the mirror php enforces for an internal` |
|        - | 7410 | `			 * callable ("count_chars() expects at most 2 arguments, 3 given").` |
|        - | 7411 | `			 * PHL accepted the extras silently — the surplus only ever reached` |
|        - | 7412 | `			 * func_get_args(). Every formal is filled on this branch, so the call` |
|        - | 7413 | `			 * is over the maximum exactly when more actuals arrived than there are` |
|        - | 7414 | `			 * non-variadic formals; a VARIADIC tail means php declares no maximum` |
|        - | 7415 | `			 * at all, so nNonVar below the formal count is exempt. Prelude` |
|        - | 7416 | `			 * FUNCTIONS only: the chunk-backed class METHODS were not swept against` |
|        - | 7417 | `			 * php's signatures (Fiber::start() is declared argless and reads` |
|        - | 7418 | `			 * func_get_args()). */` |
|        - | 7419 | `			sxu32 nNonVar,nReq;` |
|      507 | 7420 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|      502 | 7421 | `			if( nNonVar == SySetUsed(&pVmFunc->aArgs)` |
|      501 | 7422 | `			 && pFrame->nActualArgs >= 0` |
|      505 | 7423 | `			 && (sxu32)pFrame->nActualArgs > nNonVar ){` |
|       40 | 7424 | `				rc = VmThrowBuiltinTooManyArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       26 | 7425 | `					(sxu32)pFrame->nActualArgs,nNonVar,nReq);` |
|       27 | 7426 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 7427 | `					goto Abort;` |
|        - | 7428 | `				}` |
|       27 | 7429 | `				PH7_MemObjRelease(pTos);` |
|       27 | 7430 | `				pTos = &pTos[-nCallArgs];` |
|       27 | 7431 | `				pFrameStack = 0;` |
|       27 | 7432 | `				rc = PH7_EXCEPTION;` |
|       27 | 7433 | `				goto SkipFuncBody;` |
|        - | 7434 | `			}` |
|      238 | 7435 | `		}` |
|        - | 7436 | `		/* Process default values for remaining formal parameters */` |
|   772629 | 7437 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    14205 | 7438 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 7439 | `				/* Variadic parameter with no extra args — create empty array */` |
|      617 | 7440 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      617 | 7441 | `				if( pObj ){` |
|      617 | 7442 | `					PH7_MemObjToHashmap(pObj);` |
|      617 | 7443 | `					sArg.nIdx = pObj->nIdx;` |
|      617 | 7444 | `					sArg.pUserData = 0;` |
|      617 | 7445 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      306 | 7446 | `				}` |
|      617 | 7447 | `				n++;` |
|      617 | 7448 | `				break; /* Variadic is always last */` |
|        - | 7449 | `			}` |
|    13593 | 7450 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|    13593 | 7451 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|    13593 | 7452 | `				if( pObj ){` |
|        - | 7453 | `					/* Evaluate the default value and extract it's result */` |
|    13593 | 7454 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|    13593 | 7455 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 7456 | `						goto Abort;` |
|        - | 7457 | `					}` |
|        - | 7458 | `					/* Insert argument index */` |
|    13593 | 7459 | `					sArg.nIdx = pObj->nIdx;` |
|    13593 | 7460 | `					sArg.pUserData = 0;` |
|    13593 | 7461 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 7462 | `					/* Make sure the default argument is of the correct type.` |
|        - | 7463 | ``					 * A null default on an implicitly-nullable param (`int $x = null`)`` |
|        - | 7464 | `					 * must stay null — casting it to 0/""/false would diverge from PHP` |
|        - | 7465 | `					 * and contradict the explicit-null path, which now keeps it null. */` |
|    13588 | 7466 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1563 | 7467 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)` |
|      789 | 7468 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 7469 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 7470 | `						/* Cast to the desired type */` |
|      ! 0 | 7471 | `						xCast(pObj);` |
|      ! 0 | 7472 | `					}else{` |
|        - | 7473 | `						/* Mask matched — a const-indirected whole-real default` |
|        - | 7474 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|    13593 | 7475 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 7476 | `					}` |
|     6793 | 7477 | `				}` |
|     6793 | 7478 | `			}` |
|    13593 | 7479 | `			++n;` |
|        5 | 7480 | `		}` |
|        - | 7481 | `		} /* end VmCallArgMap scope */` |
|        - | 7482 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 7483 | `		 * does not return anything.` |
|        - | 7484 | `		 */` |
|   759041 | 7485 | `		PH7_MemObjRelease(pTos);` |
|   759041 | 7486 | `		pTos = &pTos[-nCallArgs];` |
|        - | 7487 | `		/* Allocate an operand stack (via the recycling allocator) and evaluate the` |
|        - | 7488 | `		 * function body. Size it to a tight static bound when the body is statically` |
|        - | 7489 | `		 * modelable (BYTECODE.md stage 7) — the big memory win for deep recursion,` |
|        - | 7490 | `		 * where one such stack lives per frame — falling back to the safe` |
|        - | 7491 | `		 * instruction-count bound otherwise.` |
|        - | 7492 | `		 *` |
|        - | 7493 | `		 * The bound is computed LAZILY on the first call and cached on the func` |
|        - | 7494 | `		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:` |
|        - | 7495 | `		 * self-computing on first use is fail-safe against any body-creation path` |
|        - | 7496 | `		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),` |
|        - | 7497 | `		 * where undersizing is a heap overflow; the amortized cost is one analysis` |
|        - | 7498 | `		 * per function. */` |
|        - | 7499 | `		{` |
|   759041 | 7500 | `			sxu32 nSlots = pVmFunc->nMaxStack;` |
|   759041 | 7501 | `			if( nSlots == 0 ){` |
|    12508 | 7502 | `				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);` |
|    18759 | 7503 | `				sxu32 nTight = VmComputeMaxStack(&(*pVm),` |
|    12503 | 7504 | `					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);` |
|    12508 | 7505 | `				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;` |
|    12508 | 7506 | `				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */` |
|    12508 | 7507 | `				pVmFunc->nMaxStack = nSlots;` |
|     6251 | 7508 | `			}` |
|   759041 | 7509 | `			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);` |
|        - | 7510 | `		}` |
|   759041 | 7511 | `		if( pFrameStack == 0 ){` |
|        - | 7512 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 7513 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 7514 | `				&pVmFunc->sName);` |
|      ! 0 | 7515 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 7516 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7517 | `			}` |
|      ! 0 | 7518 | `			break;` |
|        - | 7519 | `		}` |
|   379296 | 7520 | `SkipFuncBody:` |
|   763519 | 7521 | `		if( pSelf ){` |
|        - | 7522 | `			/* Push class name */` |
|   505649 | 7523 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|   252822 | 7524 | `		}` |
|        - | 7525 | `		/* Increment nesting level */` |
|   763519 | 7526 | `		pVm->nRecursionDepth++;` |
|   763519 | 7527 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 7528 | `			/* Arg-binding threw: there is no body to run — finish the call` |
|        - | 7529 | `			 * immediately (no record is pushed). */` |
|        - | 7530 | `			VmCallRecord sCallee;` |
|     4483 | 7531 | `			sCallee.pVmFunc = pVmFunc;` |
|     4483 | 7532 | `			sCallee.pFrame = pFrame;` |
|     4483 | 7533 | `			sCallee.pFrameStack = pFrameStack;` |
|     4483 | 7534 | `			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|     4483 | 7535 | `			sCallee.nLastRef = SXU32_HIGH;` |
|     4483 | 7536 | `			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|     4483 | 7537 | `			sState.pTos = pTos;` |
|     4483 | 7538 | `			sState.pc = pc;` |
|     4483 | 7539 | `			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);` |
|     4483 | 7540 | `			pTos = sState.pTos;` |
|     4483 | 7541 | `			pc = sState.pc;` |
|     4483 | 7542 | `			if( rc == PH7_ABORT ){` |
|        - | 7543 | `				/* Abort processing immeditaley */` |
|      ! 0 | 7544 | `				goto Abort;` |
|     4483 | 7545 | `			}else if( rc == PH7_SUSPEND ){` |
|      ! 0 | 7546 | `				goto Suspend;` |
|     4483 | 7547 | `			}else if( rc == PH7_EXCEPTION ){` |
|      165 | 7548 | `				goto Exception;` |
|        - | 7549 | `			}` |
|     2164 | 7550 | `		}else{` |
|        - | 7551 | `			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a` |
|        - | 7552 | `			 * call record (caller activation + in-flight call) and switch the` |
|        - | 7553 | `			 * loop's locals to the callee — a PHP->PHP call no longer grows` |
|        - | 7554 | `			 * the native stack. The record node is pool-allocated so` |
|        - | 7555 | `			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */` |
|   759041 | 7556 | `			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   759041 | 7557 | `			if( pRec ){` |
|   756871 | 7558 | `				pVm->pIdleCallFrames = (void *)pRec->pPrev;` |
|   378660 | 7559 | `			}else{` |
|     2175 | 7560 | `				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));` |
|        - | 7561 | `			}` |
|   759041 | 7562 | `			if( pRec == 0 ){` |
|        - | 7563 | `				/* OOM: undo the push-time accounting, tear the call down and` |
|        - | 7564 | `				 * raise the non-catchable fatal (the §3.1 OOM convention —` |
|        - | 7565 | `				 * never a silent NULL). */` |
|      ! 0 | 7566 | `				pVm->nRecursionDepth--;` |
|      ! 0 | 7567 | `				if( pSelf ){` |
|      ! 0 | 7568 | `					(void)SySetPop(&pVm->aSelf);` |
|      ! 0 | 7569 | `				}` |
|      ! 0 | 7570 | `				SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|      ! 0 | 7571 | `				VmLeaveFrame(&(*pVm));` |
|      ! 0 | 7572 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 7573 | `				goto Abort;` |
|        - | 7574 | `			}` |
|   759041 | 7575 | `			sState.pTos = pTos;` |
|   759041 | 7576 | `			sState.pc = pc;` |
|   759041 | 7577 | `			pRec->sCaller = sState;` |
|   759041 | 7578 | `			pRec->sCall.pVmFunc = pVmFunc;` |
|   759041 | 7579 | `			pRec->sCall.pFrame = pFrame;` |
|   759041 | 7580 | `			pRec->sCall.pFrameStack = pFrameStack;` |
|   759041 | 7581 | `			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|   759041 | 7582 | `			pRec->sCall.nLastRef = SXU32_HIGH;` |
|   759041 | 7583 | `			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|   759041 | 7584 | `			pRec->pPrev = pCallTop;` |
|   759041 | 7585 | `			pCallTop = pRec;` |
|        - | 7586 | `			/* Switch to the callee activation (what the recursive` |
|        - | 7587 | `			 * VmByteCodeExec entry used to set up). */` |
|   759041 | 7588 | `			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);` |
|   759041 | 7589 | `			pStack = pFrameStack;` |
|   759041 | 7590 | `			pTos = &pStack[-1];` |
|   759041 | 7591 | `			pc = 0;` |
|   759041 | 7592 | `			sState.aInstr = aInstr;` |
|   759041 | 7593 | `			sState.pStack = pStack;` |
|   759041 | 7594 | `			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */` |
|   759041 | 7595 | `			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */` |
|   759041 | 7596 | `			sState.pTos = pTos;` |
|   759041 | 7597 | `			sState.pc = 0;` |
|   759041 | 7598 | `			sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|   759041 | 7599 | `			sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|   759041 | 7600 | `			sState.pEntryFrame = pVm->pFrame;` |
|   759041 | 7601 | `			sState.pResult = pRec->sCaller.pTos;` |
|   759041 | 7602 | `			sState.pLastRef = &pRec->sCall.nLastRef;` |
|   759041 | 7603 | `			sState.pEnforceRetFunc = VmFuncHasReturnType(pVmFunc) ? pVmFunc : 0;` |
|   759041 | 7604 | `			sState.is_callback = 0;` |
|   759041 | 7605 | `			sState.bReturnPropagates = 0;` |
|   759041 | 7606 | `			goto VmLoopFetch;` |
|        - | 7607 | `		}` |
|     2164 | 7608 | `	}else{` |
|        - | 7609 | `		/* Look for an installed foreign function.` |
|        - | 7610 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 7611 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 7612 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 7613 | `		 * global fallback for unqualified function calls in namespaces. */` |
|  1615288 | 7614 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 7615 | `		{` |
|  1615288 | 7616 | `		VmCallArgMap *pCallMap2 = pEffCallMap;` |
|  1615288 | 7617 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 7618 | `			/* Compiler-qualified: try short name as global fallback */` |
|       57 | 7619 | `			const char *zShort = sName.zString;` |
|        - | 7620 | `			sxu32 i;` |
|      857 | 7621 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      805 | 7622 | `				if( sName.zString[i] == '\\' ){` |
|       71 | 7623 | `					zShort = &sName.zString[i + 1];` |
|       33 | 7624 | `				}` |
|      405 | 7625 | `			}` |
|       57 | 7626 | `			if( zShort != sName.zString ){` |
|       57 | 7627 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       57 | 7628 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       26 | 7629 | `			}` |
|       26 | 7630 | `		}` |
|        - | 7631 | `		} /* end VmCallArgMap namespace scope */` |
|  1615288 | 7632 | `		if( pEntry == 0 ){` |
|        - | 7633 | `			/* php accepts the "Class::method" STATIC-callable string everywhere a` |
|        - | 7634 | `			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).` |
|        - | 7635 | `			 * Split on the LAST "::" (php's own zend_memrchr scan) and route through the` |
|        - | 7636 | `			 * shared array-callable machinery ([class-name, method-name]) instead of` |
|        - | 7637 | `			 * warning undefined. */` |
|   240140 | 7638 | `			const char *zCbCls = 0,*zCbMeth = 0;` |
|   240140 | 7639 | `			sxu32 nCbCls = 0,nCbMeth = 0;` |
|   240140 | 7640 | `			int bScoped = PH7_VmCallableStringParts(sName.zString,sName.nByte,` |
|        - | 7641 | `				&zCbCls,&nCbCls,&zCbMeth,&nCbMeth);` |
|   240140 | 7642 | `			if( bScoped ){` |
|        - | 7643 | `				/* Resolve BEFORE dispatching: the shared dispatcher answers SXRET_OK with` |
|        - | 7644 | `` 				 * a NULL result for a pair it cannot resolve, so `$cb='C::nosuch'; $cb();` `` |
|        - | 7645 | `				 * evaluated to NULL with no diagnostic at all — where php throws the same` |
|        - | 7646 | `				 * Errors the ARRAY form of the same call already raised here. (Its own` |
|        - | 7647 | ``				 * `if( bScoped )` block: this scratch buffer and the dispatch block's`` |
|        - | 7648 | `				 * hashmap are both block-head declarations, and the check runs between` |
|        - | 7649 | `				 * them.) */` |
|        - | 7650 | `				char zSmMsg[192];` |
|   100079 | 7651 | `				sxi32 nSmBrc = pVm->nBoundaryRc;` |
|   100079 | 7652 | `				const void *pSmRes = (const void *)pVm->pResumeFrame;` |
|   150117 | 7653 | `				const char *zSmErr = VmCallableClassMethodError(&(*pVm),` |
|    50038 | 7654 | `					PH7_VmExtractClass(&(*pVm),zCbCls,nCbCls,FALSE,0),` |
|    50038 | 7655 | `					zCbCls,nCbCls,zCbMeth,nCbMeth,TRUE,zSmMsg,sizeof(zSmMsg));` |
|   100079 | 7656 | `				if( zSmErr ){` |
|        - | 7657 | `					sxi32 rcSmErr;` |
|       53 | 7658 | `					int bSmRaised = PH7_VmClassLookupRaised(&(*pVm),nSmBrc,pSmRes);` |
|       53 | 7659 | `					if( pInstr->iP2 ){` |
|      ! 0 | 7660 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 7661 | `					}` |
|       53 | 7662 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 7663 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7664 | `					}` |
|       53 | 7665 | `					PH7_MemObjRelease(pTos);` |
|       53 | 7666 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       53 | 7667 | `					pTos->nIdx = SXU32_HIGH;` |
|       53 | 7668 | `					if( bSmRaised ){` |
|        - | 7669 | `						/* The class name's autoloader threw: land THAT, exactly as the array` |
|        - | 7670 | `						 * form does — php never reports the class missing in this case. */` |
|        6 | 7671 | `						rcSmErr = pVm->nBoundaryRc;` |
|        6 | 7672 | `						pVm->nBoundaryRc = 0;` |
|        6 | 7673 | `						if( rcSmErr == PH7_ABORT ){` |
|      ! 0 | 7674 | `							goto Abort;` |
|        - | 7675 | `						}` |
|        6 | 7676 | `						rc = PH7_EXCEPTION;` |
|       14 | 7677 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7678 | `					}` |
|       48 | 7679 | `					rcSmErr = VmThrowFromVm(&(*pVm),"Error",zSmErr,(sxu32)SyStrlen(zSmErr));` |
|       48 | 7680 | `					if( rcSmErr == SXERR_ABORT ){` |
|      ! 0 | 7681 | `						goto Abort;` |
|        - | 7682 | `					}` |
|       48 | 7683 | `					rc = rcSmErr;` |
|       64 | 7684 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7685 | `				}` |
|    50013 | 7686 | `			}` |
|   240090 | 7687 | `			if( bScoped ){` |
|        - | 7688 | `				/* Resolved: hand the callable STRING itself to the shared dispatcher, which` |
|        - | 7689 | ``				 * decodes `Class::method` the same way (it has to, for the callback-argument`` |
|        - | 7690 | `				 * callers). This used to build a throwaway [class,method] map here and enter` |
|        - | 7691 | `				 * through the hashmap branch — two decoders for one spelling. */` |
|        - | 7692 | `				ph7_value sResult;` |
|        - | 7693 | `				sxi32 rcSm;` |
|   150041 | 7694 | `				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100026 | 7695 | `					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|   100028 | 7696 | `				SySetReset(&aArg);` |
|   100044 | 7697 | `				while( pArg < pTos ){` |
|       17 | 7698 | `					SySetPut(&aArg,(const void *)&pArg);` |
|       17 | 7699 | `					pArg++;` |
|        1 | 7700 | `				}` |
|   100028 | 7701 | `				PH7_MemObjInit(pVm,&sResult);` |
|   150041 | 7702 | `				rcSm = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),` |
|   100026 | 7703 | `					(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|   100028 | 7704 | `				SySetReset(&aArg);` |
|   100028 | 7705 | `				if( nCallArgs > 0 ){` |
|       15 | 7706 | `					VmPopOperand(&pTos,nCallArgs);` |
|        7 | 7707 | `				}` |
|   100028 | 7708 | `				if( rcSm == PH7_ABORT ){` |
|      ! 0 | 7709 | `					PH7_MemObjRelease(&sResult);` |
|      ! 0 | 7710 | `					goto Abort;` |
|        - | 7711 | `				}` |
|   100028 | 7712 | `				if( rcSm == PH7_EXCEPTION ){` |
|        - | 7713 | `					sxi32 iResumePc;` |
|   100004 | 7714 | `					PH7_MemObjRelease(&sResult);` |
|   100004 | 7715 | `					if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100001 | 7716 | `						PH7_MemObjRelease(pTos);` |
|        - | 7717 | `						/* Drain the abandoned outer-expression operands` |
|        - | 7718 | ``						 * (`1 + "C::m"()`) to the try's base — one leaked`` |
|        - | 7719 | `						 * slot per caught throw otherwise. */` |
|   300001 | 7720 | `						PH7_RESUME_DRAIN()` |
|   100001 | 7721 | `						pc = iResumePc;` |
|   100001 | 7722 | `						break;` |
|        - | 7723 | `					}` |
|        3 | 7724 | `					goto Exception;` |
|        - | 7725 | `				}` |
|       25 | 7726 | `				PH7_MemObjStore(&sResult,pTos);` |
|       25 | 7727 | `				PH7_MemObjRelease(&sResult);` |
|       25 | 7728 | `				break;` |
|        - | 7729 | `			}` |
|        - | 7730 | `			/* Call to an undefined function is a catchable Error in php 8 — it does` |
|        - | 7731 | `			 * NOT warn and hand back null and carry on, which is what PH7 did (and` |
|        - | 7732 | `			 * which quietly turned a typo into a null-propagating program). */` |
|        - | 7733 | `			{` |
|        - | 7734 | `			SyBlob sMsg;` |
|   140064 | 7735 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   140064 | 7736 | `			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);` |
|        - | 7737 | `			/* Consume this call's captured spread runs so they don't leak into a` |
|        - | 7738 | `			 * later call (this path never reaches VmBuildEffectiveArgMap). */` |
|   140064 | 7739 | `			if( pInstr->iP2 ){` |
|      ! 0 | 7740 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 7741 | `			}` |
|        - | 7742 | `			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the` |
|        - | 7743 | `			 * compile-time arg count on the stack, and this early exit skips the` |
|        - | 7744 | `			 * arg-building loop that the normal path uses, so popping only iP1 would` |
|        - | 7745 | `			 * strand the expanded elements and corrupt the enclosing expression. */` |
|   140064 | 7746 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 7747 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7748 | `			}` |
|   140064 | 7749 | `			PH7_MemObjRelease(pTos);` |
|   210094 | 7750 | `			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|    70030 | 7751 | `				SyBlobLength(&sMsg));` |
|   140064 | 7752 | `			SyBlobRelease(&sMsg);` |
|   140064 | 7753 | `			if( rc == SXERR_ABORT ){` |
|        6 | 7754 | `				goto Abort;` |
|        - | 7755 | `			}` |
|        - | 7756 | `			/* A catch may have run IN PLACE inside VmThrowFromVm (SXRET_OK +` |
|        - | 7757 | ``			 * recorded resume). An unconditional `goto Exception` here unwound the`` |
|        - | 7758 | `` 			 * exec ANYWAY, so `try { nosuchfn(); } catch (Error $e) {} rest();` `` |
|        - | 7759 | `			 * ran the catch and then silently dropped the rest of the script` |
|        - | 7760 | `			 * (exit 0). Route like every other in-exec throw site. */` |
|   380111 | 7761 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7762 | `			}` |
|        - | 7763 | `		}` |
|  1375152 | 7764 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 7765 | `		/* D1: resolve deferred plain-var arguments against the builtin's by-ref position` |
|        - | 7766 | `		 * mask (derived from its signature). A by-ref out-param (preg_match's $matches, …)` |
|        - | 7767 | `		 * is materialized so PH7_VmStoreArgByRef can write back — this is what keeps a` |
|        - | 7768 | ``		 * DYNAMIC-name by-ref builtin (`$f='preg_match'; $f($p,$s,$m)`) working now that the`` |
|        - | 7769 | `		 * compile-time mask no longer sees it. Every other (by-value) arg warns + passes` |
|        - | 7770 | `		 * NULL, which is also what call_user_func & friends want. pVm->pFrame is the caller. */` |
|        - | 7771 | `		{` |
|  1375152 | 7772 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,pFunc->nByRefMask,0,0,pEffCallMap);` |
|  1375168 | 7773 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 7774 | `		}` |
|        - | 7775 | `		/* Host function (builtin): build the effective spread-key map so the` |
|        - | 7776 | `		 * name-forwarding builtins (call_user_func & friends) relay string keys as` |
|        - | 7777 | `		 * named args, and — critically — so this call's captured runs are consumed.` |
|        - | 7778 | `		 * pArg is the top base here (a builtin call pops no method-name slot). */` |
|  2063528 | 7779 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  1375133 | 7780 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|  1432543 | 7781 | `NativeCall:` |
|        - | 7782 | `		/* A VM_FUNC_NATIVE method joins here, having done the two steps above for` |
|        - | 7783 | `		 * itself: its by-ref mask comes from the same signature machinery, and its` |
|        - | 7784 | `		 * effective arg map was already built (and this call's spread runs already` |
|        - | 7785 | `		 * consumed) on the method path — building it a second time here would` |
|        - | 7786 | `		 * consume them twice. Everything from this point down is shared verbatim:` |
|        - | 7787 | `		 * a native method IS a host call that happens to carry a receiver. */` |
|        - | 7788 | `		/* Start collecting function arguments */` |
|  2866741 | 7789 | `		SySetReset(&aArg);` |
|  6748104 | 7790 | `		while( pArg < pTos ){` |
|  3881368 | 7791 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  3881368 | 7792 | `			pArg++;` |
|        5 | 7793 | `		}` |
|        - | 7794 | `		/* Assume a null return value */` |
|  2866741 | 7795 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 7796 | `		/* Init the call context */` |
|  2866741 | 7797 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 7798 | `		/* Hand the call-site named-argument map to the builtin so name-forwarding` |
|        - | 7799 | `		 * helpers (call_user_func & friends) can relay name: arguments — and the` |
|        - | 7800 | `		 * caller's strict_types mode — to the inner callback. Forwarded whole (not` |
|        - | 7801 | `		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even` |
|        - | 7802 | `		 * when its own call site is purely positional; only the two forwarding` |
|        - | 7803 | `		 * builtins read pArgMap, so this is inert for every other host function. */` |
|  2866741 | 7804 | `		sCtx.pArgMap = pEffCallMap;` |
|        - | 7805 | `		/* Receiver + late-static-binding class for a native METHOD. Both stay 0 for a` |
|        - | 7806 | `		 * plain host function (the locals are initialized once per OP_CALL), so this` |
|        - | 7807 | `		 * is inert on the builtin path. The reference on pNativeOwned belongs to the` |
|        - | 7808 | `		 * caller for the span of the call — the native body borrows it and must not` |
|        - | 7809 | `		 * unref, exactly as a bytecode method's frame $this is borrowed. */` |
|  2866741 | 7810 | `		sCtx.pThis = pNativeRecv;` |
|  2866741 | 7811 | `		sCtx.pCalledClass = pNativeClass;` |
|        - | 7812 | `		{` |
|  2866741 | 7813 | `		int nGiven = (int)SySetUsed(&aArg);` |
|        - | 7814 | ``		/* Bind `name:` arguments to the callee's declared POSITIONS before anything`` |
|        - | 7815 | `		 * reads the vector — the arity screen, the ZPP screen and the C body all take` |
|        - | 7816 | `		 * it positionally. A host function has no compiled parameter records for` |
|        - | 7817 | `		 * VmResolveNamedArgs to walk, so its signature string is the source of names` |
|        - | 7818 | `		 * and defaults (PH7_VmBindNamedArgsToSig). Without this every named argument` |
|        - | 7819 | `		 * simply stayed where it was WRITTEN. */` |
|  2866741 | 7820 | `		if( pEffCallMap && pEffCallMap->bHasNamed && nGiven > 0 ){` |
|      143 | 7821 | `			rc = PH7_VmBindNamedArgsToSig(&sCtx,pFunc,pEffCallMap,&nGiven,` |
|       94 | 7822 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|       96 | 7823 | `			if( rc != SXRET_OK ){` |
|        9 | 7824 | `				goto NativeCallDone;` |
|        - | 7825 | `			}` |
|       43 | 7826 | `		}` |
|        - | 7827 | `		/* php binds a by-reference argument at the CALL, before the callee runs, so a` |
|        - | 7828 | ``		 * non-variable in a `&` position is refused ahead of every ZPP check — and`` |
|        - | 7829 | ``		 * ahead of the too-MANY-arguments one (`array_pop([1,2],5)` is the reference`` |
|        - | 7830 | `		 * Error in php, not an ArgumentCountError). With no argument at all there is` |
|        - | 7831 | `		 * nothing to refuse, which is why the too-FEW check below still speaks first` |
|        - | 7832 | ``		 * for `array_pop()`. */`` |
|  4300922 | 7833 | `		rc = PH7_VmScreenByRefArgShapes(&sCtx,pFunc,pEffCallMap,nGiven,` |
|  2866728 | 7834 | `			(ph7_value **)SySetBasePtr(&aArg));` |
|  2866733 | 7835 | `		if( rc != SXRET_OK ){` |
|       55 | 7836 | `			goto NativeCallDone;` |
|        - | 7837 | `		}` |
|        - | 7838 | `		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum` |
|        - | 7839 | `		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError` |
|        - | 7840 | `		 * before the C routine runs when called with too few arguments — instead` |
|        - | 7841 | `		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""` |
|        - | 7842 | `		 * return. The message wording matches php's ZPP output byte-for-byte. */` |
|  4300847 | 7843 | `		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){` |
|      857 | 7844 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 7845 | `				"%z() expects %s %d argument%s, %d given",` |
|      284 | 7846 | `				&pFunc->sName,` |
|      568 | 7847 | `				pFunc->bAtLeast ? "at least" : "exactly",` |
|      568 | 7848 | `				(int)pFunc->nMinArg,` |
|      568 | 7849 | `				pFunc->nMinArg == 1 ? "" : "s",` |
|      284 | 7850 | `				nGiven);` |
|  2866399 | 7851 | `		}else if( pFunc->bHasMaxArg && nGiven > (int)pFunc->nMaxArg ){` |
|        - | 7852 | `			/* php enforces the MAXIMUM as well, and PHL only did so where a` |
|        - | 7853 | `			 * builtin happened to hand-roll the check (51 of ~650), so` |
|        - | 7854 | ``			 * `microtime(1,2)`, `strlen("a","b")` and friends silently ignored`` |
|        - | 7855 | `			 * the extras. The count comes from the same signature table as the` |
|        - | 7856 | `			 * minimum; a variadic tail leaves nMaxArg at -1 and is exempt.` |
|        - | 7857 | `			 * php says "exactly" when the bounds coincide, "at most" otherwise. */` |
|      218 | 7858 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 7859 | `				"%z() expects %s %d argument%s, %d given",` |
|       71 | 7860 | `				&pFunc->sName,` |
|      117 | 7861 | `				((int)pFunc->nMinArg == (int)pFunc->nMaxArg && !pFunc->bAtLeast) ? "exactly" : "at most",` |
|      142 | 7862 | `				(int)pFunc->nMaxArg,` |
|      142 | 7863 | `				pFunc->nMaxArg == 1 ? "" : "s",` |
|       71 | 7864 | `				nGiven);` |
|  4299853 | 7865 | `		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,` |
|  2865968 | 7866 | `			(ph7_value **)SySetBasePtr(&aArg))) ){` |
|        - | 7867 | `			/* TypeError thrown: rc carries the caught/uncaught status */` |
|      498 | 7868 | `		}else{` |
|        - | 7869 | `			/* The name of the builtin that is RUNNING, for the few diagnostics` |
|        - | 7870 | `			 * raised so deep inside the engine that no ph7_context reaches them` |
|        - | 7871 | `			 * (a stream filter's, from inside a device read) and which php still` |
|        - | 7872 | `			 * prefixes with the caller. Saved and restored: a builtin can call` |
|        - | 7873 | `			 * back into php and reach this line again. */` |
|  2864987 | 7874 | `			SyString *pSavedCallee = pVm->pCalleeName;` |
|  2864987 | 7875 | `			pVm->pCalleeName = &pFunc->sName;` |
|        - | 7876 | `			/* Call the foreign function */` |
|  2864987 | 7877 | `			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));` |
|  2864987 | 7878 | `			pVm->pCalleeName = pSavedCallee;` |
|        - | 7879 | `			/* A host function that RAISED a catchable throw (PH7_VmThrowException)` |
|        - | 7880 | `			 * and still returned PH7_OK reports it here — the throw's catch has` |
|        - | 7881 | `			 * already run in place, so treating the call as a normal return would` |
|        - | 7882 | `			 * resume execution INSIDE the try body the throw abandoned (and, when` |
|        - | 7883 | `			 * uncaught, run on past the reported fatal). The status is recorded on` |
|        - | 7884 | `			 * the call context, so this covers the shared validation helpers whose` |
|        - | 7885 | `			 * callers have no channel to thread a status back. */` |
|  2864987 | 7886 | `			rc = VmHostFuncThrowRc(&sCtx,rc);` |
|        - | 7887 | `		}` |
|  1432543 | 7888 | `NativeCallDone:` |
|  1434193 | 7889 | `		(void)nGiven; /* the named-arg binder's early exit lands here */` |
|        - | 7890 | `		}` |
|        - | 7891 | `		/* Release the call context */` |
|  2866741 | 7892 | `		VmReleaseCallContext(&sCtx);` |
|  2866741 | 7893 | `		if( pNativeOwned ){` |
|        - | 7894 | `			/* Drop the receiver reference the method branch took (pThis->iRef++ before` |
|        - | 7895 | `			 * the target slot was released). A bytecode method hands this to its frame` |
|        - | 7896 | `			 * and lets the teardown do it; a native method has no frame, so it is` |
|        - | 7897 | `			 * dropped here — the one point EVERY route out of the call passes through,` |
|        - | 7898 | `			 * including the throw/abort/suspend ones below. Ordered after the context` |
|        - | 7899 | `			 * release because sCtx.sThis aliases the instance. Always 0 for a plain` |
|        - | 7900 | `			 * host function. */` |
|  1490524 | 7901 | `			PH7_ClassInstanceUnref(pNativeOwned);` |
|  1490524 | 7902 | `			pNativeOwned = 0;` |
|  1490524 | 7903 | `			pNativeRecv = 0;` |
|   745261 | 7904 | `		}` |
|  2866741 | 7905 | `		if( rc == PH7_ABORT ){` |
|        - | 7906 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 7907 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 7908 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      677 | 7909 | `			PH7_MemObjRelease(&sRet);` |
|      677 | 7910 | `			goto Abort;` |
|        - | 7911 | `		}` |
|  2866069 | 7912 | `		if( rc != PH7_SUSPEND && pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 7913 | `			/* A throw raised inside this host function — directly` |
|        - | 7914 | `			 * (PH7_VmThrowException) or by a PHP callback it invoked — was` |
|        - | 7915 | `			 * caught by an INLINE try (generator body) THIS exec owns.` |
|        - | 7916 | `			 * VmThrowInline records only a pc-redirect: a direct builtin throw` |
|        - | 7917 | `			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION` |
|        - | 7918 | `			 * with the redirect pending, so the rc branches below never land` |
|        - | 7919 | `			 * it (pre-existing hole: explode("") or a throwing usort` |
|        - | 7920 | `			 * comparator inside a generator's try lost the catch AND the` |
|        - | 7921 | `			 * yield). Land at the redirect now — its drain to the try's` |
|        - | 7922 | `			 * operand base subsumes the args + name pops. */` |
|       10 | 7923 | `			PH7_MemObjRelease(&sRet);` |
|       32 | 7924 | `			PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 7925 | `		}` |
|  2866061 | 7926 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 7927 | `			/* A callback invoked by this host function threw. If an in-place catch` |
|        - | 7928 | `			 * recorded a resume target owned by THIS body, resume at its landing pad` |
|        - | 7929 | `			 * (consuming the target); otherwise the exception was caught by an outer` |
|        - | 7930 | `			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW` |
|        - | 7931 | `			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which` |
|        - | 7932 | `			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */` |
|        - | 7933 | `			sxi32 iResumePc;` |
|     8111 | 7934 | `			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 7935 | `				/* Caught by an outer exec, or not caught here: propagate. */` |
|     1909 | 7936 | `				goto Exception;` |
|        - | 7937 | `			}` |
|        - | 7938 | `			/* Exception was caught in place by THIS body's try: pop args and the` |
|        - | 7939 | `			 * result slot, then drain any abandoned outer-expression operands to` |
|        - | 7940 | `			 * the try's base and resume. */` |
|     6207 | 7941 | `			PH7_MemObjRelease(&sRet);` |
|     6207 | 7942 | `			if( nCallArgs > 0 ){` |
|     5751 | 7943 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|     2873 | 7944 | `			}` |
|     6207 | 7945 | `			VmPopOperand(&pTos,1);` |
|    10389 | 7946 | `			PH7_RESUME_DRAIN()` |
|     6207 | 7947 | `			pc = iResumePc;` |
|     6207 | 7948 | `			break;` |
|        - | 7949 | `		}` |
|  2857955 | 7950 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 7951 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 7952 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 7953 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 7954 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 7955 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 7956 | `			 * body), the user-function path above will handle re-saving. */` |
|      367 | 7957 | `			PH7_MemObjRelease(&sRet);` |
|      367 | 7958 | `			if( nCallArgs > 0 ){` |
|      359 | 7959 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      177 | 7960 | `			}` |
|        - | 7961 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 7962 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|      367 | 7963 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|      367 | 7964 | `			goto Suspend;` |
|        - | 7965 | `		}` |
|  2857593 | 7966 | `		if( nCallArgs > 0 ){` |
|        - | 7967 | `			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack` |
|        - | 7968 | `			 * expanded the compile-time arg count on the stack, so popping iP1` |
|        - | 7969 | `			 * strands the extra elements (or, for an unpack that expanded to fewer` |
|        - | 7970 | `			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,` |
|        - | 7971 | `			 * reading a bogus value whose stray flags sent MemObjStore into the` |
|        - | 7972 | `			 * hashmap-release path and hung). Mirrors every other CALL exit. The` |
|        - | 7973 | `			 * function-name slot (pTos) receives the return value below. */` |
|  2806200 | 7974 | `			VmPopOperand(&pTos,nCallArgs);` |
|  1403921 | 7975 | `		}` |
|        - | 7976 | `		/* Save foreign function return value into the (now top) function-name slot */` |
|  2857593 | 7977 | `		PH7_MemObjStore(&sRet,pTos);` |
|        - | 7978 | `		/* ...and clear that slot's index. It is one of the call's own argument slots,` |
|        - | 7979 | `		 * still carrying the variable index the argument was loaded with, and` |
|        - | 7980 | `		 * PH7_MemObjStore does not touch nIdx — so a builtin's return value came back` |
|        - | 7981 | ``		 * looking like an lvalue for the caller's variable (`f(strtoupper($b))` with`` |
|        - | 7982 | ``		 * `function f(&$x)` overwrote `$b`). No host function returns by reference. */`` |
|  2857593 | 7983 | `		pTos->nIdx = SXU32_HIGH;` |
|  2857593 | 7984 | `		PH7_MemObjRelease(&sRet);` |
|        - | 7985 | `	}` |
|  2861911 | 7986 | `	break;` |
|        - | 7987 | `				  }` |
|        - | 7988 | `/*` |
|        - | 7989 | ` * OP_CONSUME: P1 * *` |
|        - | 7990 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 7991 | ` */` |
|    54466 | 7992 | `case PH7_OP_CONSUME: {` |
|        - | 7993 | `	VmOpRc rcOp;` |
|   108937 | 7994 | `	sState.pTos = pTos;` |
|   108937 | 7995 | `	sState.pc = pc;` |
|   108937 | 7996 | `	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);` |
|   108937 | 7997 | `	pTos = sState.pTos;` |
|   108937 | 7998 | `	pc = sState.pc;` |
|   108937 | 7999 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 8000 | `		goto Abort;` |
|   108935 | 8001 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       14 | 8002 | `		goto Exception;` |
|        - | 8003 | `	}` |
|   108918 | 8004 | `	break;` |
|        - | 8005 | `					  }` |
|        - | 8006 |  |
|        - | 8007 | `		} /* Switch() */` |
| 42581394 | 8008 | `		pc++; /* Next instruction in the stream */` |
|        5 | 8009 | `	} /* For(;;) */` |
|  1567874 | 8010 | `Done:` |
|        - | 8011 | `	/* A stacked callee completing lands here too (its result is already in` |
|        - | 8012 | `	 * sState.pResult — the caller's operand slot); Unwind's first iteration` |
|        - | 8013 | `	 * bottoms out identically for the record-less case. */` |
|  3136192 | 8014 | `	rc = SXRET_OK;` |
|  3136192 | 8015 | `	goto Unwind;` |
|      842 | 8016 | `Suspend:` |
|     1689 | 8017 | `	rc = PH7_SUSPEND;` |
|     1689 | 8018 | `	if( pCallTop != 0 ){` |
|        - | 8019 | `		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment` |
|        - | 8020 | `		 * instead of the lossy unwind. pc/nTos of the innermost activation were` |
|        - | 8021 | `		 * already saved into the ctx by VmSuspendCtx; capture the rest (the` |
|        - | 8022 | `		 * record chain, the innermost activation, the suspend-time top frame)` |
|        - | 8023 | `		 * so resume re-enters HERE, inside the innermost callee, like php. The` |
|        - | 8024 | `		 * records / frames / operand stacks stay alive — nothing is freed. Only` |
|        - | 8025 | `		 * fibers reach this (generators yield only at their body level, pCallTop` |
|        - | 8026 | `		 * == 0); a suspend inside a C->PHP callback was already rejected with a` |
|        - | 8027 | `		 * FiberError before it could arrive here. */` |
|      306 | 8028 | `		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));` |
|      306 | 8029 | `		if( pSeg == 0 ){` |
|        - | 8030 | `			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST` |
|        - | 8031 | `			 * pc/nTos into the ctx, so the lossy body-level fallback would resume` |
|        - | 8032 | `			 * the callee's pc against the body stack — silent corruption, exactly` |
|        - | 8033 | `			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the` |
|        - | 8034 | `			 * §3.1 convention shared with the stage-2 record-alloc OOM site). */` |
|      ! 0 | 8035 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 8036 | `			rc = PH7_ABORT;` |
|      ! 0 | 8037 | `			goto Unwind;` |
|        - | 8038 | `		}` |
|      306 | 8039 | `		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */` |
|      306 | 8040 | `		pSeg->sState = sState;` |
|      306 | 8041 | `		pSeg->pCallTop = pCallTop;` |
|      306 | 8042 | `		pSeg->pTopFrame = pVm->pFrame;` |
|      306 | 8043 | `		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;` |
|      306 | 8044 | `		pSeg->nOldFinBase = pVm->pActiveCtx ? pVm->pActiveCtx->nFinallyBase : 0;` |
|        - | 8045 | `		{` |
|        - | 8046 | `			VmCallFrame *pRec;` |
|      306 | 8047 | `			pSeg->nRecords = 0;` |
|      608 | 8048 | `			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){` |
|      306 | 8049 | `				pSeg->nRecords++;` |
|      155 | 8050 | `			}` |
|        - | 8051 | `		}` |
|      306 | 8052 | `		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;` |
|        - | 8053 | `		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */` |
|      306 | 8054 | `		SySetRelease(&aArg);` |
|      306 | 8055 | `		return PH7_SUSPEND;` |
|        - | 8056 | `	}` |
|     1387 | 8057 | `	goto Unwind;` |
|      480 | 8058 | `Abort:` |
|      965 | 8059 | `	rc = PH7_ABORT;` |
|      965 | 8060 | `	goto Unwind;` |
|   301939 | 8061 | `Exception:` |
|   603883 | 8062 | `	rc = PH7_EXCEPTION;` |
|   603878 | 8063 | `	goto Unwind;` |
|  1870984 | 8064 | `Unwind:` |
|        - | 8065 | `	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration` |
|        - | 8066 | `	 * finishes the top record exactly as the old per-level native return did:` |
|        - | 8067 | `	 * for ABORT/EXCEPTION, first run what the popped activation's own` |
|        - | 8068 | `	 * Abort/Exception label used to do (clear its pending return, release its` |
|        - | 8069 | `	 * operands — a stacked activation never has bReturnPropagates set), then` |
|        - | 8070 | `	 * VmCallFinish routes in the restored caller (an in-place catch there` |
|        - | 8071 | `	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —` |
|        - | 8072 | `	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible` |
|        - | 8073 | `	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).` |
|        - | 8074 | `	 * At the bottom, VmExecFinalize hands the status to the native caller. */` |
|  2071896 | 8075 | `	for(;;){` |
|  4143358 | 8076 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|        - | 8077 | `			/* Drop any pending hook-RMW write-backs this activation armed — its` |
|        - | 8078 | `			 * statement is abandoned (only the innermost activation at throw time` |
|        - | 8079 | `			 * can own entries: the armed window spans exactly one instruction, so` |
|        - | 8080 | `			 * no OP_CALL record ever intervenes). */` |
|  1005788 | 8081 | `			while( SySetUsed(&pVm->aHookRmw) > 0` |
|  1005789 | 8082 | `			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){` |
|      ! 0 | 8083 | `				VmHookRmwDropTop(&(*pVm));` |
|      ! 0 | 8084 | `			}` |
|   502892 | 8085 | `		}` |
|  4143358 | 8086 | `		if( pCallTop == 0 ){` |
|  3384520 | 8087 | `			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);` |
|        - | 8088 | `		}` |
|   758843 | 8089 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|   603873 | 8090 | `			VmClearFramePending(sState.pEntryFrame);` |
|   610163 | 8091 | `			while( pTos >= pStack ){` |
|     6295 | 8092 | `				PH7_MemObjRelease(pTos);` |
|     6295 | 8093 | `				pTos--;` |
|        5 | 8094 | `			}` |
|   301934 | 8095 | `		}` |
|   758843 | 8096 | `		if( rc != PH7_SUSPEND ){` |
|        - | 8097 | `			/* The finishing callee's own leaked finally actions must not survive` |
|        - | 8098 | `			 * into the caller (whose next OP_END_FINALLY would mis-pop them). */` |
|   758841 | 8099 | `			VmDiscardFinallyActions(&(*pVm),sState.nFinallyActBase);` |
|   379640 | 8100 | `		}` |
|        - | 8101 | `		{` |
|   758843 | 8102 | `			VmCallFrame *pRec = pCallTop;` |
|   758843 | 8103 | `			sState = pRec->sCaller;` |
|   758843 | 8104 | `			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);` |
|   758843 | 8105 | `			pCallTop = pRec->pPrev;` |
|   758843 | 8106 | `			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   758843 | 8107 | `			pVm->pIdleCallFrames = (void *)pRec;` |
|   758843 | 8108 | `			aInstr = sState.aInstr;` |
|   758843 | 8109 | `			pStack = sState.pStack;` |
|   758843 | 8110 | `			pTos = sState.pTos;` |
|   758843 | 8111 | `			pc = sState.pc;` |
|        - | 8112 | `		}` |
|   758843 | 8113 | `		if( rc == PH7_OK ){` |
|   357893 | 8114 | `			pc++; /* the loop-bottom increment this OP_CALL missed */` |
|   357893 | 8115 | `			goto VmLoopFetch;` |
|        - | 8116 | `		}` |
|        5 | 8117 | `	}` |
|  1692410 | 8118 | `}` |
|        - | 8119 |  |
