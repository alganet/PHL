# src/ph7/vm_exec.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3720/4214 lines (88.28%)

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
|      213 |   78 | `		return 1; /* already fits — no growth needed */` |
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
|       49 |  112 | `		if( ppBaseOwner ){ *ppBaseOwner = pNew; }` |
|       49 |  113 | `		if( pnBaseCap ){ *pnBaseCap = nNewCap; }` |
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
|  3400995 |  164 | `static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)` |
|        5 |  165 | `{` |
|  3401000 |  166 | `	if( rcTerm != PH7_SUSPEND ){` |
|  3399618 |  167 | `		VmDiscardFinallyActions(&(*pVm),pState->nFinallyActBase);` |
|  1699797 |  168 | `	}` |
|  3401000 |  169 | `	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){` |
|  1942811 |  170 | `		VmClearFramePending(pState->pEntryFrame);` |
|   971394 |  171 | `	}` |
|  3401000 |  172 | `	SySetRelease(pArg);` |
|  3401000 |  173 | `	if( rcTerm == PH7_ABORT \|\| rcTerm == PH7_EXCEPTION ){` |
|   806127 |  174 | `		while( pTos >= pState->pStack ){` |
|   403946 |  175 | `			PH7_MemObjRelease(pTos);` |
|   403946 |  176 | `			pTos--;` |
|        5 |  177 | `		}` |
|   201090 |  178 | `	}` |
|  3401000 |  179 | `	return rcTerm;` |
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
|  4165639 |  195 | `static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase)` |
|        5 |  196 | `{` |
|  4165654 |  197 | `	while( SySetUsed(&pVm->aFinallyAction) > nBase ){` |
|       13 |  198 | `		VmFinallyAction *pAct = (VmFinallyAction *)SySetPeek(&pVm->aFinallyAction);` |
|       13 |  199 | `		if( pAct->eKind == PH7_FA_RETHROW && pAct->pExc ){` |
|        7 |  200 | `			PH7_ClassInstanceUnref(pAct->pExc);` |
|        9 |  201 | `		}else if( pAct->eKind == PH7_FA_RETURN ){` |
|        3 |  202 | `			PH7_MemObjRelease(&pAct->sRet);` |
|        1 |  203 | `		}` |
|       13 |  204 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|        3 |  205 | `	}` |
|  4165644 |  206 | `}` |
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
|   770504 |  219 | `static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)` |
|        5 |  220 | `{` |
|        - |  221 | `	ph7_value *pObj;` |
|        - |  222 | `	/* Decrement nesting level */` |
|   770509 |  223 | `	pVm->nRecursionDepth--;` |
|   770509 |  224 | `	if( pCallee->bSelfPushed ){` |
|        - |  225 | `		/* Pop class name */` |
|   505793 |  226 | `		(void)SySetPop(&pVm->aSelf);` |
|   252894 |  227 | `	}` |
|   770509 |  228 | `	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
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
|   770447 |  264 | `		pCaller->pTos->nIdx = SXU32_HIGH;` |
|        - |  265 | `	}` |
|   770509 |  266 | `	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  267 | `		/* The callee threw (or its finally threw past it). If an in-place catch` |
|        - |  268 | `		 * recorded a resume target owned by THIS caller's body, resume there and` |
|        - |  269 | `		 * consume the target (VmRecordedResume); when the catcher is an outer exec` |
|        - |  270 | `		 * — or this is a callback with no bytecode to resume into — propagate so` |
|        - |  271 | `		 * the owning exec lands. This replaces the old "is the caller's parent a` |
|        - |  272 | `		 * resumable try frame" test, which resumed at the caller's OWN try even` |
|        - |  273 | `		 * when the finally's throw was caught further out, losing that catch's` |
|        - |  274 | `		 * return (ROOT B, face c). */` |
|        - |  275 | `		sxi32 iResumePc;` |
|   608569 |  276 | `		VmFrame *pParentFrame = pCallee->pFrame->pParent;` |
|   608569 |  277 | `		if( !pCaller->is_callback && pVm->pInlineInstr == (void *)pCaller->aInstr ){` |
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
|   608569 |  288 | `		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){` |
|        - |  289 | `			/* Pop the result, then drain any abandoned outer-expression operands` |
|        - |  290 | `			 * to the catching try's base (like the inline branch above) — the` |
|        - |  291 | `			 * ops that would have consumed them were abandoned by the throw, and` |
|        - |  292 | `` 			 * leaving them leaks one slot per caught throw (`try { $a = 1 + f(); }` `` |
|        - |  293 | `			 * in a loop overflowed the operand stack). */` |
|   207841 |  294 | `			VmPopOperand(&pCaller->pTos,1);` |
|   813653 |  295 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iResumeStackDepth ){` |
|   605817 |  296 | `				PH7_MemObjRelease(pCaller->pTos);` |
|   605817 |  297 | `				pCaller->pTos--;` |
|        5 |  298 | `			}` |
|   207841 |  299 | `			pCaller->pc = iResumePc;` |
|   207841 |  300 | `			rc = PH7_OK;` |
|   103923 |  301 | `		}else{` |
|   400733 |  302 | `			if( pParentFrame->pParent ){` |
|   400729 |  303 | `				rc = PH7_EXCEPTION;` |
|   200367 |  304 | `			}else{` |
|        - |  305 | `				/* Continue normal execution */` |
|        6 |  306 | `				rc = PH7_OK;` |
|        - |  307 | `			}` |
|        - |  308 | `		}` |
|   304282 |  309 | `	}` |
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
|   770509 |  320 | `	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){` |
|        - |  321 | `		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee` |
|        - |  322 | `		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly` |
|        - |  323 | `		 * the allocated slot count either way. */` |
|   766031 |  324 | `		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap);` |
|   383230 |  325 | `	}` |
|        - |  326 | `	/* Leave the frame */` |
|   770509 |  327 | `	VmLeaveFrame(&(*pVm));` |
|   770509 |  328 | `	if( rc == PH7_ABORT ){` |
|      397 |  329 | `		return PH7_ABORT;` |
|        - |  330 | `	}` |
|   770117 |  331 | `	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  332 | `		/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  333 | `		 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  334 | `		 * overwriting the state saved by the inner level.` |
|        - |  335 | `		 * pTos points to the result slot (not yet written).` |
|        - |  336 | `		 * Save nTos one below so resume pushes at the result slot. */` |
|      ! 0 |  337 | `		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);` |
|      ! 0 |  338 | `		return PH7_SUSPEND;` |
|        - |  339 | `	}` |
|   770117 |  340 | `	if( rc == PH7_EXCEPTION ){` |
|   400729 |  341 | `		return PH7_EXCEPTION;` |
|        - |  342 | `	}` |
|   369393 |  343 | `	return PH7_OK;` |
|   385474 |  344 | `}` |
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
|  3401301 |  380 | `PH7_PRIVATE sxi32 VmByteCodeExec(` |
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
|  3401306 |  400 | `	if( VmNativeNestingExceeded(pVm) ){` |
|        5 |  401 | `		return VmNativeNestingFatal(pVm);` |
|        - |  402 | `	}` |
|        - |  403 | `	/* A fresh native exec entered while a C-boundary throw is parked` |
|        - |  404 | `	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the` |
|        - |  405 | `	 * very opcode that swallowed the throw) must run CLEAN: the parked status` |
|        - |  406 | `	 * belongs to the interrupted outer exec's fetch-point router, not to this` |
|        - |  407 | `	 * one. Save+clear on entry, merge back on exit — the outer status is` |
|        - |  408 | `	 * restored unless this exec parked its own unconsumed (newer) one, with` |
|        - |  409 | `	 * PH7_ABORT dominating either way. */` |
|  3401302 |  410 | `	nSavedBrc = pVm->nBoundaryRc;` |
|  3401302 |  411 | `	pVm->nBoundaryRc = 0;` |
|        - |  412 | `	/* The executing source line belongs to the ACTIVATION. A nested body -- a called` |
|        - |  413 | `	 * function, but equally an attribute-default or default-argument mini-program --` |
|        - |  414 | `	 * runs its own bytecode with its own lines, so it must not leave the caller` |
|        - |  415 | ``	 * reporting the callee's position: `new Exception` stamped line 1 because the`` |
|        - |  416 | ``	 * class's `protected $message = '';` default ran (from the embedded chunk) between`` |
|        - |  417 | `	 * OP_NEW and the stamp. Save on entry, restore on exit. */` |
|  3401302 |  418 | `	nSavedLine = pVm->nCurLine;` |
|  3401302 |  419 | `	pVm->nVmExecDepth++;` |
|  5101941 |  420 | `	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,` |
|  1700639 |  421 | `		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);` |
|  3401302 |  422 | `	pVm->nVmExecDepth--;` |
|  3401302 |  423 | `	pVm->nCurLine = nSavedLine;` |
|  3401302 |  424 | `	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT \|\| pVm->nBoundaryRc == 0) ){` |
|       38 |  425 | `		pVm->nBoundaryRc = nSavedBrc;` |
|       17 |  426 | `	}` |
|  3401302 |  427 | `	return rc;` |
|  1700646 |  428 | `}` |
|        - |  429 | `/*` |
|        - |  430 | ` * D1 commit 2: allocate a captured lvalue path for a deferred element/property call arg.` |
|        - |  431 | ` * eRoot picks the root container: 0 = a real aMemObj slot (nRootIdx), 1 = an undefined` |
|        - |  432 | ` * variable to vivify by name at resolve time (pName, a VM-lifetime bytecode string, borrowed),` |
|        - |  433 | ` * 2 = a string base (subscripting a string -> php refuses a by-ref bind). The struct owns its` |
|        - |  434 | ` * step array and each step's key/name; PH7_MemObjRelease frees it via VmFreeDeferredPath.` |
|        - |  435 | ` */` |
|    47358 |  436 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName)` |
|        5 |  437 | `{` |
|    47363 |  438 | `	VmDeferredPath *pPath = (VmDeferredPath *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDeferredPath));` |
|    47363 |  439 | `	if( pPath == 0 ){` |
|      ! 0 |  440 | `		return 0;` |
|        - |  441 | `	}` |
|    47363 |  442 | `	SyZero(pPath,sizeof(VmDeferredPath));` |
|    47363 |  443 | `	pPath->pAlloc = &pVm->sAllocator;` |
|    47363 |  444 | `	pPath->eRoot = eRoot;` |
|    47363 |  445 | `	pPath->nRootIdx = nRootIdx;` |
|    47363 |  446 | `	if( eRoot == 1 && pName ){` |
|        6 |  447 | `		pPath->sRootName = *pName; /* borrowed VM-lifetime bytes, not copied */` |
|        2 |  448 | `	}` |
|    47363 |  449 | `	return pPath;` |
|    23853 |  450 | `}` |
|        - |  451 | `/*` |
|        - |  452 | ` * A carrier for a fetch that ALREADY HAPPENED: an overloaded container answered with a` |
|        - |  453 | ` * value, and only the by-ref verdict is still pending (VM_DEFER_ROOT_PREFETCH). Takes a` |
|        - |  454 | ` * copy of the value; the caller keeps its own.` |
|        - |  455 | ` */` |
|     2304 |  456 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNewPrefetch(ph7_vm *pVm,int nKind,ph7_class *pClass,` |
|        - |  457 | `	const SyString *pName,ph7_value *pVal)` |
|        4 |  458 | `{` |
|     2308 |  459 | `	VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),VM_DEFER_ROOT_PREFETCH,SXU32_HIGH,0);` |
|     2308 |  460 | `	if( pPath == 0 ){` |
|      ! 0 |  461 | `		return 0;` |
|        - |  462 | `	}` |
|     2308 |  463 | `	pPath->nOverKind = (sxu8)nKind;` |
|     2308 |  464 | `	pPath->pOverClass = pClass;` |
|     2308 |  465 | `	if( pName && pName->nByte > 0 ){` |
|     2242 |  466 | `		pPath->zOverName = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|     2242 |  467 | `		if( pPath->zOverName == 0 ){` |
|      ! 0 |  468 | `			VmFreeDeferredPath(pPath);` |
|      ! 0 |  469 | `			return 0;` |
|        - |  470 | `		}` |
|     2242 |  471 | `		SyStringInitFromBuf(&pPath->sOverName,pPath->zOverName,pName->nByte);` |
|     1119 |  472 | `	}` |
|     2308 |  473 | `	PH7_MemObjInit(&(*pVm),&pPath->sPrefetch);` |
|     2308 |  474 | `	PH7_MemObjStore(pVal,&pPath->sPrefetch);` |
|     2308 |  475 | `	return pPath;` |
|     1156 |  476 | `}` |
|        - |  477 | `/*` |
|        - |  478 | ` * The verdict a prefetched value gets when the callee turns out to want it BY REFERENCE:` |
|        - |  479 | ` * php asked the object for something to modify and it could only answer with a value.` |
|        - |  480 | ` * Two of the three are notices php carries on from; a HOOKED property is the one php` |
|        - |  481 | ` * refuses outright. All three stay silent for a by-VALUE parameter, which is the whole` |
|        - |  482 | ` * reason the fetch could not decide them itself.` |
|        - |  483 | ` */` |
|       40 |  484 | `static sxi32 VmPrefetchByRefVerdict(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pVal)` |
|        1 |  485 | `{` |
|       41 |  486 | `	if( pPath->nOverKind == VM_OVER_PROP ){` |
|       15 |  487 | `		PH7_VmOverloadedPropNotice(&(*pVm),pPath->pOverClass,&pPath->sOverName,pVal);` |
|       15 |  488 | `		return SXRET_OK;` |
|        - |  489 | `	}` |
|       27 |  490 | `	if( pPath->nOverKind == VM_OVER_HOOK ){` |
|        - |  491 | `		SyBlob sErrMsg;` |
|        - |  492 | `		sxi32 rcH;` |
|        5 |  493 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 |  494 | `		SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|        4 |  495 | `			&pPath->pOverClass->sName,&pPath->sOverName);` |
|        5 |  496 | `		rcH = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg);` |
|        5 |  497 | `		SyBlobRelease(&sErrMsg);` |
|        5 |  498 | `		return (rcH == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  499 | `	}` |
|       23 |  500 | `	PH7_VmOverloadedElemNotice(&(*pVm),pPath->pOverClass,pVal);` |
|       23 |  501 | `	return SXRET_OK;` |
|       21 |  502 | `}` |
|    45346 |  503 | `static VmDeferStep * VmDeferPathGrow(VmDeferredPath *pPath)` |
|        5 |  504 | `{` |
|    45351 |  505 | `	if( pPath->nStep >= pPath->nAlloc ){` |
|    45307 |  506 | `		sxu32 nNew = pPath->nAlloc ? pPath->nAlloc * 2 : 4;` |
|    68127 |  507 | `		VmDeferStep *aNew = (VmDeferStep *)SyMemBackendRealloc(pPath->pAlloc,pPath->aStep,` |
|    22820 |  508 | `			nNew * sizeof(VmDeferStep));` |
|    45307 |  509 | `		if( aNew == 0 ){` |
|      ! 0 |  510 | `			return 0;` |
|        - |  511 | `		}` |
|    45307 |  512 | `		pPath->aStep = aNew;` |
|    45307 |  513 | `		pPath->nAlloc = nNew;` |
|    22820 |  514 | `	}` |
|    45351 |  515 | `	return &pPath->aStep[pPath->nStep];` |
|    22847 |  516 | `}` |
|        - |  517 | `/* Append an array-element step, deep-copying the index value (the caller releases pKey). */` |
|    45030 |  518 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey)` |
|        5 |  519 | `{` |
|    45035 |  520 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|    45035 |  521 | `	if( pStep == 0 ){` |
|      ! 0 |  522 | `		return SXERR_MEM;` |
|        - |  523 | `	}` |
|    45035 |  524 | `	pStep->isProp = 0;` |
|    45035 |  525 | `	pStep->bAppend = 0;` |
|    45035 |  526 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|    45035 |  527 | `	PH7_MemObjInit(pKey->pVm,&pStep->sKey);` |
|    45035 |  528 | `	PH7_MemObjStore(pKey,&pStep->sKey);` |
|    45035 |  529 | `	pPath->nStep++;` |
|    45035 |  530 | `	return SXRET_OK;` |
|    22689 |  531 | `}` |
|        - |  532 | ``/* Append a KEYLESS element step — the `[]` of `f($a[])`. It carries no index at all,`` |
|        - |  533 | ` * so the two resolvers differ on it: by reference it creates the next element (php` |
|        - |  534 | `` * binds the parameter to it), by value it is php's `Cannot use [] for reading`. */`` |
|       10 |  535 | `PH7_PRIVATE sxi32 VmDeferPathPushAppend(VmDeferredPath *pPath)` |
|        1 |  536 | `{` |
|       11 |  537 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|       11 |  538 | `	if( pStep == 0 ){` |
|      ! 0 |  539 | `		return SXERR_MEM;` |
|        - |  540 | `	}` |
|       11 |  541 | `	pStep->isProp = 0;` |
|       11 |  542 | `	pStep->bAppend = 1;` |
|       11 |  543 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|       11 |  544 | `	SyZero((void *)&pStep->sKey,sizeof(ph7_value));` |
|       11 |  545 | `	pPath->nStep++;` |
|       11 |  546 | `	return SXRET_OK;` |
|        6 |  547 | `}` |
|        - |  548 | `/* Append an object-property step, owning a private copy of the name bytes. */` |
|      306 |  549 | `PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName)` |
|        4 |  550 | `{` |
|      310 |  551 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|        - |  552 | `	char *zCopy;` |
|      310 |  553 | `	if( pStep == 0 ){` |
|      ! 0 |  554 | `		return SXERR_MEM;` |
|        - |  555 | `	}` |
|      310 |  556 | `	zCopy = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|      310 |  557 | `	if( zCopy == 0 ){` |
|      ! 0 |  558 | `		return SXERR_MEM;` |
|        - |  559 | `	}` |
|      310 |  560 | `	pStep->isProp = 1;` |
|      310 |  561 | `	pStep->bAppend = 0;` |
|      310 |  562 | `	pStep->zProp = zCopy;` |
|      310 |  563 | `	SyStringInitFromBuf(&pStep->sProp,zCopy,pName->nByte);` |
|      310 |  564 | `	pPath->nStep++;` |
|      310 |  565 | `	return SXRET_OK;` |
|      157 |  566 | `}` |
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
|       87 |  620 | `		pRecv->iRef++;` |
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
|    47358 |  637 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath)` |
|        5 |  638 | `{` |
|        - |  639 | `	sxu32 i;` |
|    47363 |  640 | `	if( pPath == 0 ){` |
|      ! 0 |  641 | `		return;` |
|        - |  642 | `	}` |
|    92709 |  643 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|    45351 |  644 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|    45351 |  645 | `		if( pStep->isProp ){` |
|      310 |  646 | `			if( pStep->zProp ){` |
|      310 |  647 | `				SyMemBackendFree(pPath->pAlloc,pStep->zProp);` |
|      157 |  648 | `			}` |
|    45198 |  649 | `		}else if( !pStep->bAppend ){` |
|        - |  650 | `			/* An append step holds no key at all — its sKey was never initialized. */` |
|    45035 |  651 | `			PH7_MemObjRelease(&pStep->sKey);` |
|    22684 |  652 | `		}` |
|    22847 |  653 | `	}` |
|    47363 |  654 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|     2308 |  655 | `		PH7_MemObjRelease(&pPath->sPrefetch);` |
|     2308 |  656 | `		if( pPath->zOverName ){` |
|     2242 |  657 | `			SyMemBackendFree(pPath->pAlloc,pPath->zOverName);` |
|     1119 |  658 | `		}` |
|     1152 |  659 | `	}` |
|    47363 |  660 | `	if( pPath->aStep ){` |
|    45307 |  661 | `		SyMemBackendFree(pPath->pAlloc,pPath->aStep);` |
|    22820 |  662 | `	}` |
|    47363 |  663 | `	SyMemBackendFree(pPath->pAlloc,pPath);` |
|    23853 |  664 | `}` |
|        - |  665 | `/* Map an op-handler VmOpRc into the main-loop rc convention used by VmByteCodeExecBody. */` |
|    45316 |  666 | `static sxi32 VmOpRcToExecRc(VmOpRc rcOp)` |
|        5 |  667 | `{` |
|    45321 |  668 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 |  669 | `		return PH7_ABORT;` |
|        - |  670 | `	}` |
|    45319 |  671 | `	if( rcOp == VM_OP_EXCEPTION ){` |
|       15 |  672 | `		return PH7_EXCEPTION;` |
|        - |  673 | `	}` |
|    45307 |  674 | `	return SXRET_OK;` |
|    22832 |  675 | `}` |
|        - |  676 | `/*` |
|        - |  677 | ` * D1 commit 2: re-drive ONE captured lvalue step by invoking the real LOAD_IDX / MEMBER` |
|        - |  678 | ` * handler on a synthetic 2-slot stack. This reuses the proven COW / vivify / warning / magic` |
|        - |  679 | ` * machinery instead of hand-walking it. pBase carries the current container (its nIdx must be` |
|        - |  680 | ` * a real aMemObj slot for a by-ref write to vivify in place). iP2 selects the mode:` |
|        - |  681 | ` * LOAD_IDX 1=write(vivify,by-ref) / 0=read(by-value); MEMBER PH7_MEMBER_READ=by-value read.` |
|        - |  682 | ` * On success pOut receives the result value and its nIdx (the aliasable element slot for a` |
|        - |  683 | ` * vivified by-ref element).` |
|        - |  684 | ` */` |
|    45316 |  685 | `static sxi32 VmReDriveStep(ph7_vm *pVm,sxi32 iOp,sxu32 iP2,ph7_value *pBase,ph7_value *pKey,ph7_value *pOut)` |
|        5 |  686 | `{` |
|        - |  687 | `	ph7_value mini[2];` |
|        - |  688 | `	VmInstr aI[2];` |
|        - |  689 | `	VmExecState st;` |
|        - |  690 | `	VmOpRc rcOp;` |
|        - |  691 | ``	/* A NULL key is the APPEND form (`$a[]`): LOAD_IDX takes no index operand, so the`` |
|        - |  692 | `	 * base is the whole stack and iP1 says so. */` |
|    45321 |  693 | `	int bAppend = (pKey == 0);` |
|    45321 |  694 | `	PH7_MemObjInit(pVm,&mini[0]);` |
|    45321 |  695 | `	PH7_MemObjInit(pVm,&mini[1]);` |
|    45321 |  696 | `	PH7_MemObjLoad(pBase,&mini[0]);` |
|    45321 |  697 | `	mini[0].nIdx = pBase->nIdx;` |
|    45321 |  698 | `	if( !bAppend ){` |
|    45313 |  699 | `		PH7_MemObjStore(pKey,&mini[1]);` |
|    22823 |  700 | `	}` |
|    45321 |  701 | `	SyZero((void *)aI,sizeof(aI));` |
|        - |  702 | `	/* LOAD_IDX: iP1=1 means "an index is present". MEMBER: iP1=0 means an INSTANCE member` |
|        - |  703 | ``	 * (iP1=1 would be a static `::` access). */`` |
|    45321 |  704 | `	aI[0].iOp = (sxu8)iOp; aI[0].iP1 = (iOp == PH7_OP_LOAD_IDX && !bAppend) ? 1 : 0; aI[0].iP2 = iP2;` |
|    45321 |  705 | `	SyZero((void *)&st,sizeof(st));` |
|    45321 |  706 | `	st.pStack = mini; st.pTos = bAppend ? &mini[0] : &mini[1]; st.aInstr = aI; st.pc = 0;` |
|    45321 |  707 | `	if( iOp == PH7_OP_LOAD_IDX ){` |
|    45035 |  708 | `		rcOp = VmExecOpLoadIdx(&(*pVm),&st,&aI[0]);` |
|    22689 |  709 | `	}else{` |
|      290 |  710 | `		rcOp = VmExecOpMember(&(*pVm),&st,&aI[0]);` |
|        - |  711 | `	}` |
|        - |  712 | `	/* The handler popped the key/name and left the result in the (now top) base slot.` |
|        - |  713 | `	 * DEEP-COPY it into pOut (MemObjStore, not MemObjLoad): the result is chained as the next` |
|        - |  714 | `	 * step's base and must OWN its buffer — mini[0] is released immediately below, and a` |
|        - |  715 | `	 * read-only view (MemObjLoad) would leave pOut dangling into freed memory. */` |
|    45321 |  716 | `	PH7_MemObjStore(st.pTos,pOut);` |
|    45321 |  717 | `	pOut->nIdx = st.pTos->nIdx;` |
|    45321 |  718 | `	PH7_MemObjRelease(&mini[0]);` |
|    45321 |  719 | `	return VmOpRcToExecRc(rcOp);` |
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
|      ! 0 |  780 | `			if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|        - |  781 | `				/* A native class's property is a field of php's own C struct, not` |
|        - |  782 | `				 * storage a script may alias: php has no ptr_ptr handler for one, so` |
|        - |  783 | ``				 * `g($i->s)` with `function g(&$x)` passes the VALUE and the callee's`` |
|        - |  784 | `				 * write is lost — in SILENCE, unlike the overloaded case below, which` |
|        - |  785 | ``				 * php has a notice for. `preg_match($p,$s,$i->s)` is the same answer. */`` |
|      ! 0 |  786 | `				ph7_value *pCur = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|      ! 0 |  787 | `				*pbNoBind = 1;` |
|      ! 0 |  788 | `				if( pValOut && pCur ){` |
|      ! 0 |  789 | `					PH7_MemObjStore(pCur,pValOut);` |
|      ! 0 |  790 | `				}` |
|      ! 0 |  791 | `				return SXRET_OK;` |
|        - |  792 | `			}` |
|      ! 0 |  793 | `			*pnOut = pAttr->nIdx;` |
|      ! 0 |  794 | `			return SXRET_OK;` |
|        - |  795 | `		}` |
|        - |  796 | `		/* A static property is the CLASS's: php does not find it through an` |
|        - |  797 | ``		 * instance, so binding `f($o->s)` by reference must not hand out the`` |
|        - |  798 | `		 * class slot — that let a by-ref callee overwrite shared class state` |
|        - |  799 | `		 * through an object, and with no diagnostic at all (the value pass that` |
|        - |  800 | `		 * carries the notice at the fetch site never runs for a by-ref arg).` |
|        - |  801 | `		 * Notice here and fall through to the missing-property handling. */` |
|        4 |  802 | `		if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->pAttr->sName,` |
|        2 |  803 | `			pAttr->pAttr->iProtection,FALSE) ){` |
|        4 |  804 | `			VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  805 | `				"Accessing static property %z::$%z as non static",` |
|        1 |  806 | `				&pClass->sName,pName);` |
|        1 |  807 | `		}` |
|        3 |  808 | `		pAttr = 0;` |
|        1 |  809 | `	}` |
|        6 |  810 | `	if( PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|        9 |  811 | `	 \|\| PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) ){` |
|        - |  812 | `		/* Overloaded (magic) property: php passes it by-value with a Notice and drops the` |
|        - |  813 | `		 * write-back — "has no effect". The value it passes is __get's, which the caller` |
|        - |  814 | `		 * takes through pValOut; leaving the argument NULL instead turned` |
|        - |  815 | ``		 * `sort($o->magic)` — a statement php performs on a temporary — into`` |
|        - |  816 | ``		 * `sort(): Argument #1 ($array) must be of type array, null given`. */`` |
|      ! 0 |  817 | `		VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  818 | `			"Indirect modification of overloaded property %z::$%z has no effect",` |
|      ! 0 |  819 | `			&pClass->sName,pName);` |
|      ! 0 |  820 | `		*pbNoBind = 1;` |
|      ! 0 |  821 | `		if( pValOut && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|      ! 0 |  822 | `		 && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g') ){` |
|      ! 0 |  823 | `			VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|      ! 0 |  824 | `			PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pValOut);` |
|      ! 0 |  825 | `			VmMagicGuardPop(pVm);` |
|      ! 0 |  826 | `		}` |
|      ! 0 |  827 | `		return SXRET_OK;` |
|        - |  828 | `	}` |
|        - |  829 | `	{` |
|        9 |  830 | `		ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);` |
|        9 |  831 | `		if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      ! 0 |  832 | `			VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pAttr);` |
|        9 |  833 | `		}else if( VmClassAllowsDynamicProps(&(*pVm),pClass) ){` |
|        6 |  834 | `			PH7_VmCreateDynamicAttr(&(*pVm),pThis,pName->zString,pName->nByte,&pAttr);` |
|        4 |  835 | `		}else{` |
|        - |  836 | `			SyBlob sMsg;` |
|        - |  837 | `			sxi32 rcT;` |
|        3 |  838 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 |  839 | `			SyBlobFormat(&sMsg,"Cannot create dynamic property %z::$%z",&pClass->sName,pName);` |
|        3 |  840 | `			rcT = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        3 |  841 | `			SyBlobRelease(&sMsg);` |
|        3 |  842 | `			*pbNoBind = 1;` |
|        3 |  843 | `			return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  844 | `		}` |
|        6 |  845 | `		if( pAttr ){` |
|        6 |  846 | `			*pnOut = pAttr->nIdx;` |
|        4 |  847 | `		}else{` |
|      ! 0 |  848 | `			*pbNoBind = 1;` |
|        - |  849 | `		}` |
|        - |  850 | `	}` |
|        6 |  851 | `	return SXRET_OK;` |
|       13 |  852 | `}` |
|        - |  853 | `/*` |
|        - |  854 | ` * Walk a captured path's steps over SLOTS: each step vivifies in place and answers the` |
|        - |  855 | ` * next one, so the terminal slot is what the by-ref binder aliases. Split out of the` |
|        - |  856 | ` * resolver below because the VALUE walk hands control back to it — an accessor that` |
|        - |  857 | ` * answered with an OBJECT is a handle, not a temporary, and everything under it is` |
|        - |  858 | ` * addressable again.` |
|        - |  859 | ` */` |
|       82 |  860 | `static sxi32 VmWalkStepsFromSlot(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,sxu32 nCur,` |
|        - |  861 | `	ph7_value *pSlot)` |
|        3 |  862 | `{` |
|        - |  863 | `	sxu32 i;` |
|        - |  864 | `	sxi32 rc;` |
|      151 |  865 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|       87 |  866 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|       87 |  867 | `		if( pStep->isProp ){` |
|       21 |  868 | `			sxu32 nOut = SXU32_HIGH;` |
|       21 |  869 | `			int bNoBind = 0;` |
|        - |  870 | `			ph7_value sMagicVal;` |
|       21 |  871 | `			int bLastStep = (i + 1 == pPath->nStep);` |
|       21 |  872 | `			PH7_MemObjInit(&(*pVm),&sMagicVal);` |
|       39 |  873 | `			rc = VmBindPropByRef(&(*pVm),(ph7_value *)SySetAt(&pVm->aMemObj,nCur),` |
|       18 |  874 | `				&pStep->sProp,&nOut,&bNoBind,bLastStep ? &sMagicVal : 0);` |
|       21 |  875 | `			if( rc != SXRET_OK ){` |
|       18 |  876 | `				PH7_MemObjRelease(&sMagicVal);` |
|       18 |  877 | `				return rc;` |
|        - |  878 | `			}` |
|        3 |  879 | `			if( bNoBind ){` |
|        - |  880 | `				/* magic/non-object: nothing to alias, so the argument is passed BY` |
|        - |  881 | `				 * VALUE — which for an overloaded property is what __get answered,` |
|        - |  882 | `				 * not the NULL this used to leave behind. */` |
|      ! 0 |  883 | `				if( bLastStep ){` |
|      ! 0 |  884 | `					PH7_MemObjStore(&sMagicVal,pSlot);` |
|      ! 0 |  885 | `					pSlot->nIdx = SXU32_HIGH;` |
|      ! 0 |  886 | `				}` |
|      ! 0 |  887 | `				PH7_MemObjRelease(&sMagicVal);` |
|      ! 0 |  888 | `				return SXRET_OK;` |
|        - |  889 | `			}` |
|        3 |  890 | `			PH7_MemObjRelease(&sMagicVal);` |
|        3 |  891 | `			nCur = nOut;` |
|        2 |  892 | `		}else{` |
|        - |  893 | `			ph7_value out;` |
|       68 |  894 | `			ph7_value *pContainer = (ph7_value *)SySetAt(&pVm->aMemObj,nCur);` |
|       68 |  895 | `			if( pContainer == 0 ){` |
|      ! 0 |  896 | `				return SXRET_OK;` |
|        - |  897 | `			}` |
|       68 |  898 | `			PH7_MemObjInit(&(*pVm),&out);` |
|        - |  899 | ``			/* `f($a[])` bound to a by-reference parameter: php CREATES the next element`` |
|        - |  900 | `			 * and aliases the parameter to it. */` |
|      101 |  901 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,1,pContainer,` |
|       66 |  902 | `				pStep->bAppend ? 0 : &pStep->sKey,&out);` |
|       68 |  903 | `			nCur = out.nIdx;` |
|       68 |  904 | `			PH7_MemObjRelease(&out);` |
|       68 |  905 | `			if( rc != SXRET_OK ){` |
|        3 |  906 | `				return rc;` |
|        - |  907 | `			}` |
|       66 |  908 | `			if( nCur == SXU32_HIGH ){` |
|      ! 0 |  909 | `				return SXRET_OK; /* no aliasable slot (e.g. a non-lvalue container): pass by value */` |
|        - |  910 | `			}` |
|        - |  911 | `		}` |
|       35 |  912 | `	}` |
|        - |  913 | `	{` |
|        - |  914 | `		/* The terminal slot is what the by-ref binder aliases, but the argument also has` |
|        - |  915 | `		 * to CARRY the element's value: a builtin reads what it is handed and writes back` |
|        - |  916 | `		 * through the slot. That was invisible while a path was only ever captured on a` |
|        - |  917 | `		 * MISS — the vivified element is NULL and so was the carrier — and stopped being` |
|        - |  918 | `		 * true when a WRITABLE container's existing element started riding one` |
|        - |  919 | ``		 * (`sort($ao['a'])` reached sort() as NULL). */`` |
|       66 |  920 | `		ph7_value *pFinal = (ph7_value *)SySetAt(&pVm->aMemObj,nCur);` |
|       66 |  921 | `		if( pFinal ){` |
|       66 |  922 | `			PH7_MemObjLoad(pFinal,pSlot);` |
|       32 |  923 | `		}` |
|        - |  924 | `	}` |
|       66 |  925 | `	pSlot->nIdx = nCur;` |
|       66 |  926 | `	return SXRET_OK;` |
|       44 |  927 | `}` |
|        - |  928 | `/*` |
|        - |  929 | ` * Walk a captured path's remaining steps over a VALUE rather than a slot — the` |
|        - |  930 | ` * continuation both resolvers need once the chain has left addressable storage: an` |
|        - |  931 | ` * overloaded container's answer is a temporary, and everything subscripted off it is a` |
|        - |  932 | ` * temporary too. bWrite picks php's fetch mode for those steps: a by-REFERENCE argument` |
|        - |  933 | ` * makes them W fetches, which vivify inside the temporary in SILENCE (php's` |
|        - |  934 | `` * `f($o['a']['zz'])` says only its notice), while a by-VALUE one reads and warns about a`` |
|        - |  935 | ` * key that is not there.` |
|        - |  936 | ` */` |
|    47264 |  937 | `static sxi32 VmWalkStepsOverValue(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,` |
|        - |  938 | `	ph7_value *pCur,int bWrite,ph7_value *pSlot)` |
|        5 |  939 | `{` |
|    47269 |  940 | `	sxi32 rc = SXRET_OK;` |
|        - |  941 | `	sxu32 i;` |
|    92507 |  942 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|    45259 |  943 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|        - |  944 | `		ph7_value out;` |
|    45259 |  945 | `		PH7_MemObjInit(&(*pVm),&out);` |
|    45259 |  946 | `		if( pStep->isProp && bWrite && (pCur->iFlags & MEMOBJ_OBJ) ){` |
|        - |  947 | `			/* php's "indirect" only ever describes a VALUE: an object is a HANDLE, so a` |
|        - |  948 | `			 * write through one lands however the handle was obtained — which is also why` |
|        - |  949 | ``			 * the notice above stays silent for an object. `f($o->magic->p)` with`` |
|        - |  950 | ``			 * `function f(&$x)` really does create and write `p` on the object __get`` |
|        - |  951 | `			 * answered with. Bind the property and let the slot walk finish the chain. */` |
|        3 |  952 | `			sxu32 nOut = SXU32_HIGH;` |
|        3 |  953 | `			int bNoBind = 0;` |
|        3 |  954 | `			int bLastStep = (i + 1 == pPath->nStep);` |
|        - |  955 | `			ph7_value sMagicVal;` |
|        3 |  956 | `			PH7_MemObjRelease(&out);` |
|        3 |  957 | `			PH7_MemObjInit(&(*pVm),&sMagicVal);` |
|        4 |  958 | `			rc = VmBindPropByRef(&(*pVm),pCur,&pStep->sProp,&nOut,&bNoBind,` |
|        1 |  959 | `				bLastStep ? &sMagicVal : 0);` |
|        3 |  960 | `			if( rc != SXRET_OK \|\| bNoBind ){` |
|      ! 0 |  961 | `				if( rc == SXRET_OK && bLastStep ){` |
|        - |  962 | `					/* An overloaded property one level down: its own notice has been` |
|        - |  963 | `					 * raised and what __get answered is what php passes. */` |
|      ! 0 |  964 | `					PH7_MemObjStore(&sMagicVal,pSlot);` |
|      ! 0 |  965 | `					pSlot->nIdx = SXU32_HIGH;` |
|      ! 0 |  966 | `				}` |
|      ! 0 |  967 | `				PH7_MemObjRelease(&sMagicVal);` |
|      ! 0 |  968 | `				return rc;` |
|        - |  969 | `			}` |
|        3 |  970 | `			PH7_MemObjRelease(&sMagicVal);` |
|        3 |  971 | `			return VmWalkStepsFromSlot(&(*pVm),pPath,i + 1,nOut,pSlot);` |
|        - |  972 | `		}` |
|    45257 |  973 | `		if( pStep->isProp ){` |
|        - |  974 | `			ph7_value nameVal;` |
|      290 |  975 | `			PH7_MemObjInitFromString(&(*pVm),&nameVal,&pStep->sProp);` |
|      290 |  976 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_MEMBER,PH7_MEMBER_READ,pCur,&nameVal,&out);` |
|      290 |  977 | `			PH7_MemObjRelease(&nameVal);` |
|    45114 |  978 | `		}else if( pStep->bAppend ){` |
|        - |  979 | ``			/* `f($o['a'][])`: the append lands in the temporary either way. A by-VALUE`` |
|        - |  980 | ``			 * binding is php's runtime `Cannot use [] for reading`, the same Error the`` |
|        - |  981 | `			 * slot-based walk raises for it. */` |
|        - |  982 | `			sxi32 rcAp;` |
|        3 |  983 | `			if( bWrite ){` |
|        - |  984 | `				/* The appended element is a fresh NULL that nothing else can see —` |
|        - |  985 | `				 * php binds the parameter to it and the temporary is dropped. */` |
|      ! 0 |  986 | `				PH7_MemObjRelease(pCur);` |
|      ! 0 |  987 | `				*pCur = out;` |
|      ! 0 |  988 | `				continue;` |
|        - |  989 | `			}` |
|        3 |  990 | `			PH7_MemObjRelease(&out);` |
|        3 |  991 | `			rcAp = VmThrowFromVm(&(*pVm),"Error","Cannot use [] for reading",` |
|        - |  992 | `				sizeof("Cannot use [] for reading")-1);` |
|        3 |  993 | `			return (rcAp == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 |  994 | `		}else{` |
|    44969 |  995 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,bWrite ? 1 : 0,pCur,&pStep->sKey,&out);` |
|        - |  996 | `		}` |
|    45255 |  997 | `		PH7_MemObjRelease(pCur);` |
|    45255 |  998 | `		*pCur = out;` |
|    45255 |  999 | `		if( rc != SXRET_OK ){` |
|       14 | 1000 | `			return rc;` |
|        - | 1001 | `		}` |
|    22793 | 1002 | `	}` |
|    47253 | 1003 | `	PH7_MemObjStore(pCur,pSlot);` |
|    47253 | 1004 | `	pSlot->nIdx = SXU32_HIGH;` |
|    47253 | 1005 | `	return SXRET_OK;` |
|    23806 | 1006 | `}` |
|        - | 1007 | `/* Resolve a captured lvalue path as a BY-REF target: vivify the whole chain in place and` |
|        - | 1008 | ` * leave pSlot->nIdx pointing at the terminal (aliasable) slot for the by-ref binder. */` |
|      128 | 1009 | `static sxi32 VmResolvePathByRef(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        4 | 1010 | `{` |
|        - | 1011 | `	sxu32 nCur;` |
|        - | 1012 | `	sxi32 rc;` |
|      132 | 1013 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|        - | 1014 | `		/* An overloaded container was asked for something to MODIFY and could only hand` |
|        - | 1015 | `		 * back a value: php notices that the write has no effect and carries on with the` |
|        - | 1016 | `		 * temporary. The notice is raised HERE — the fetch itself cannot know whether the` |
|        - | 1017 | `		 * parameter it feeds is by-reference, and a by-VALUE one is silent. */` |
|        - | 1018 | `		ph7_value cur;` |
|       41 | 1019 | `		PH7_MemObjInit(&(*pVm),&cur);` |
|       41 | 1020 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|       41 | 1021 | `		rc = VmPrefetchByRefVerdict(&(*pVm),pPath,&cur);` |
|       41 | 1022 | `		if( rc == SXRET_OK ){` |
|       37 | 1023 | `			rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,TRUE,pSlot);` |
|       18 | 1024 | `		}` |
|       41 | 1025 | `		PH7_MemObjRelease(&cur);` |
|       41 | 1026 | `		return rc;` |
|        - | 1027 | `	}` |
|       92 | 1028 | `	if( pPath->eRoot == 2 ){` |
|        - | 1029 | `		/* Subscripting a string: php refuses a by-ref bind to a string offset — but` |
|        - | 1030 | ``		 * it applies its OFFSET rules first, so `f($s["p"])` is the offset TypeError`` |
|        - | 1031 | ``		 * and `f($s[1.5])` warns about the cast before this Error is raised. */`` |
|        - | 1032 | `		sxi32 rcT;` |
|       11 | 1033 | `		if( pPath->nStep > 0 && !pPath->aStep[0].isProp ){` |
|        - | 1034 | `			SyBlob sTypeMsg;` |
|       11 | 1035 | `			sxi64 iOfft = 0;` |
|        8 | 1036 | `			if( VmStringOffsetResolve(&(*pVm),&pPath->aStep[0].sKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg)` |
|        7 | 1037 | `				== VM_STROFF_REJECT ){` |
|        3 | 1038 | `				rcT = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|        3 | 1039 | `				return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1040 | `			}` |
|        3 | 1041 | `		}` |
|        9 | 1042 | `		rcT = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 1043 | `			"Cannot create references to/from string offsets",` |
|        - | 1044 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|        9 | 1045 | `		return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1046 | `	}` |
|       83 | 1047 | `	if( pPath->eRoot == 1 ){` |
|        6 | 1048 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,TRUE); /* vivify $a */` |
|        6 | 1049 | `		if( pRoot == 0 ){` |
|      ! 0 | 1050 | `			return SXRET_OK;` |
|        - | 1051 | `		}` |
|        6 | 1052 | `		nCur = pRoot->nIdx;` |
|        4 | 1053 | `	}else{` |
|       79 | 1054 | `		nCur = pPath->nRootIdx;` |
|        - | 1055 | `	}` |
|       83 | 1056 | `	return VmWalkStepsFromSlot(&(*pVm),pPath,0,nCur,pSlot);` |
|       68 | 1057 | `}` |
|        - | 1058 | `/* Resolve a captured lvalue path as a BY-VALUE argument: read the chain (emitting php's` |
|        - | 1059 | ` * undefined-key/property/offset warnings) WITHOUT vivifying, leaving the terminal value in` |
|        - | 1060 | ` * pSlot. Re-drives the read handlers so the warning sequence matches php exactly. */` |
|    47228 | 1061 | `static sxi32 VmResolvePathByValue(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        5 | 1062 | `{` |
|        - | 1063 | `	ph7_value cur;` |
|        - | 1064 | `	sxi32 rc;` |
|    47233 | 1065 | `	PH7_MemObjInit(&(*pVm),&cur);` |
|    47233 | 1066 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|        - | 1067 | `		/* The accessor already ran, where php runs it: a by-VALUE argument simply takes` |
|        - | 1068 | `		 * what it answered, in silence. */` |
|     2266 | 1069 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|    46102 | 1070 | `	}else if( pPath->eRoot == 1 ){` |
|      ! 0 | 1071 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,FALSE);` |
|      ! 0 | 1072 | `		if( pRoot == 0 ){` |
|      ! 0 | 1073 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&pPath->sRootName);` |
|      ! 0 | 1074 | `		}else{` |
|      ! 0 | 1075 | `			PH7_MemObjLoad(pRoot,&cur);` |
|      ! 0 | 1076 | `			cur.nIdx = pRoot->nIdx;` |
|        - | 1077 | `		}` |
|      ! 0 | 1078 | `	}else{` |
|    44971 | 1079 | `		ph7_value *pRoot = (ph7_value *)SySetAt(&pVm->aMemObj,pPath->nRootIdx);` |
|    44971 | 1080 | `		if( pRoot ){` |
|    44971 | 1081 | `			PH7_MemObjLoad(pRoot,&cur);` |
|    44971 | 1082 | `			cur.nIdx = pRoot->nIdx;` |
|    22652 | 1083 | `		}` |
|        - | 1084 | `	}` |
|    47233 | 1085 | `	rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,FALSE,pSlot);` |
|    47233 | 1086 | `	PH7_MemObjRelease(&cur);` |
|    47233 | 1087 | `	return rc;` |
|        5 | 1088 | `}` |
|        - | 1089 | `/*` |
|        - | 1090 | ` * Is this actual argument REFUSED by a by-reference parameter?` |
|        - | 1091 | ` *` |
|        - | 1092 | ` * php answers from the argument's compile-time SHAPE, which the call site carries in` |
|        - | 1093 | ` * VmCallArgMap.nNonLvalMask (GenStateArgShape, compile.c): a literal, an operator or` |
|        - | 1094 | `` * cast result, a class constant, `@$x`, `$o?->p` or an assignment is a hard non-lvalue`` |
|        - | 1095 | `` * and binding one is `Argument #N ($p) could not be passed by reference`.`` |
|        - | 1096 | ` *` |
|        - | 1097 | ` * nPos is the argument's position on the operand stack, which is the position the` |
|        - | 1098 | ` * compiler classified — named arguments change which FORMAL a slot binds to, not the` |
|        - | 1099 | ` * slot's index, so both binders index the mask the same way.` |
|        - | 1100 | ` *` |
|        - | 1101 | ` * Without a shape mask (a SPREAD call, an engine-synthesized call, an indirect dispatch` |
|        - | 1102 | ` * through call_user_func or an array callable) this falls back to the runtime test the` |
|        - | 1103 | ` * binders used before: no slot to write back through, and not one of the values PH7 has` |
|        - | 1104 | ` * always passed by value instead. That test cannot tell a literal from a call RESULT —` |
|        - | 1105 | ` * php accepts the latter — which is exactly why the mask exists.` |
|        - | 1106 | ` */` |
|     7162 | 1107 | `PH7_PRIVATE int PH7_VmArgRefusedByRef(VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1108 | `{` |
|     7167 | 1109 | `	if( pMap && pMap->bArgShapes && nPos < 31 ){` |
|     6947 | 1110 | `		return (pMap->nNonLvalMask & (1u << nPos)) != 0;` |
|        - | 1111 | `	}` |
|      225 | 1112 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|      163 | 1113 | `		return 0;` |
|        - | 1114 | `	}` |
|       93 | 1115 | `	return (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|       62 | 1116 | `	    && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0;` |
|     3586 | 1117 | `}` |
|        - | 1118 | `/*` |
|        - | 1119 | `` * The same call site's OTHER answer: the argument is the RESULT of a call or of `new`.`` |
|        - | 1120 | ` *` |
|        - | 1121 | ` * php cannot know at compile time whether the callee returns a reference, so it defers` |
|        - | 1122 | ` * to the value: one that arrived WITH a reference binds silently, and one without gets` |
|        - | 1123 | ` * php's E_NOTICE and the callee then operates on the temporary. Emitting it is all this` |
|        - | 1124 | ` * does — a temp-call argument is never refused.` |
|        - | 1125 | ` */` |
|        - | 1126 | `/*` |
|        - | 1127 | ` * A typed by-REFERENCE parameter's coercion belongs to the CALLER's variable. php` |
|        - | 1128 | ` * converts the actual in weak mode and the REFERENCE then holds the conversion, so` |
|        - | 1129 | `` * `$v = 1.0; f($v);` with `function f(int &$x)` leaves both views int(1). PHL ran the`` |
|        - | 1130 | ` * declared-type check on the operand-stack COPY while the binder aliases the caller's` |
|        - | 1131 | ` * slot by index, so the conversion reached neither the callee (which reads through the` |
|        - | 1132 | ` * alias) nor the caller: both stayed float, and every other pair did the same` |
|        - | 1133 | `` * (`float &$y` given an int, `string &$s` given an int, `bool &$b` given an int).`` |
|        - | 1134 | ` *` |
|        - | 1135 | ` * Writes back only when the check actually changed the value's TYPE — an untyped` |
|        - | 1136 | ` * parameter, or one the actual already satisfies, copies nothing.` |
|        - | 1137 | ` */` |
|     2978 | 1138 | `PH7_PRIVATE void PH7_VmByRefArgWriteBack(ph7_vm *pVm,ph7_value *pArg,sxi32 iPreFlags)` |
|        5 | 1139 | `{` |
|        - | 1140 | `	ph7_value *pSlot;` |
|     2978 | 1141 | `	if( pArg->nIdx == SXU32_HIGH` |
|     2983 | 1142 | `	 \|\| (pArg->iFlags & MEMOBJ_ALL) == (iPreFlags & MEMOBJ_ALL) ){` |
|     2959 | 1143 | `		return;` |
|        - | 1144 | `	}` |
|       25 | 1145 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|       25 | 1146 | `	if( pSlot && pSlot != pArg ){` |
|       25 | 1147 | `		PH7_MemObjStore(pArg,pSlot);` |
|       12 | 1148 | `	}` |
|     1494 | 1149 | `}` |
|     4518 | 1150 | `PH7_PRIVATE void PH7_VmArgTempCallNotice(ph7_vm *pVm,VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1151 | `{` |
|     4523 | 1152 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| nPos >= 31 ){` |
|      225 | 1153 | `		return;` |
|        - | 1154 | `	}` |
|     4303 | 1155 | `	if( (pMap->nTempCallMask & (1u << nPos)) == 0 ){` |
|     4263 | 1156 | `		return;` |
|        - | 1157 | `	}` |
|       43 | 1158 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|        3 | 1159 | `		return; /* a by-reference RETURN: php is silent and binds it */` |
|        - | 1160 | `	}` |
|       41 | 1161 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Only variables should be passed by reference");` |
|     2264 | 1162 | `}` |
|        - | 1163 | `/*` |
|        - | 1164 | `` * A GENERATOR's arguments are bound at the `g(...)` that BUILDS the Generator object,`` |
|        - | 1165 | ` * before any resume — php's rule, and where php also refuses a by-reference parameter` |
|        - | 1166 | ` * handed a non-variable. That branch collects its actuals into a vector of its own (and` |
|        - | 1167 | ` * reorders it for named arguments), so neither of the two OP_CALL binders ever sees them` |
|        - | 1168 | `` * and `function g(&$x){ yield; } g(1 + 1);` built a Generator in silence.`` |
|        - | 1169 | ` *` |
|        - | 1170 | ` * Answers PH7_EXCEPTION (or PH7_ABORT) for the first refused position, having raised the` |
|        - | 1171 | ` * throw; SXRET_OK otherwise, with php's temp-call notice emitted along the way. Named` |
|        - | 1172 | ` * arguments are resolved by NAME against the formals here rather than through the` |
|        - | 1173 | ` * branch's own mapping, which is built later and freed inside its block.` |
|        - | 1174 | ` */` |
|      118 | 1175 | `static sxi32 VmScreenGenByRefArgs(ph7_vm *pVm,ph7_vm_func *pFunc,VmCallArgMap *pMap,` |
|        - | 1176 | `	ph7_value *pArg,sxu32 nActual,ph7_class *pSelfHint)` |
|        4 | 1177 | `{` |
|      122 | 1178 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      122 | 1179 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|        - | 1180 | `	sxu32 i;` |
|      248 | 1181 | `	for( i = 0 ; i < nActual ; ++i ){` |
|      136 | 1182 | `		sxu32 n = i;` |
|      136 | 1183 | `		if( pMap && pMap->bHasNamed && i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       35 | 1184 | `			for( n = 0 ; n < nFormal ; ++n ){` |
|       30 | 1185 | `				if( pMap->aNames[i].nByte == SyStringLength(&aFormal[n].sName)` |
|       30 | 1186 | `				 && SyMemcmp(pMap->aNames[i].zString,SyStringData(&aFormal[n].sName),` |
|       36 | 1187 | `					pMap->aNames[i].nByte) == 0 ){` |
|       23 | 1188 | `					break;` |
|        - | 1189 | `				}` |
|        8 | 1190 | `			}` |
|       11 | 1191 | `		}` |
|      136 | 1192 | `		if( n >= nFormal \|\| (aFormal[n].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|       94 | 1193 | `			continue;` |
|        - | 1194 | `		}` |
|       44 | 1195 | `		if( PH7_VmArgRefusedByRef(pMap,i,&pArg[i]) ){` |
|       10 | 1196 | `			sxi32 rcT = VmThrowByRefRefusal(&(*pVm),` |
|        6 | 1197 | `				(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|        6 | 1198 | `				&pFunc->sName,n + 1,&aFormal[n].sName);` |
|        7 | 1199 | `			return (rcT == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1200 | `		}` |
|       38 | 1201 | `		PH7_VmArgTempCallNotice(&(*pVm),pMap,i,&pArg[i]);` |
|       20 | 1202 | `	}` |
|      116 | 1203 | `	return SXRET_OK;` |
|       63 | 1204 | `}` |
|        - | 1205 | `/*` |
|        - | 1206 | ` * D1: resolve deferred call arguments in [pArg, pTos) before the callee consumes them.` |
|        - | 1207 | ` *` |
|        - | 1208 | `` * A plain `$var` call argument whose callee signature is unknown at compile time is`` |
|        - | 1209 | ` * emitted as a DEFERRED load (OP_LOAD iP1=1,iP2=3): when the variable does not exist it` |
|        - | 1210 | ` * yields a NULL slot tagged MEMOBJ_AUX_DEFERRED that carries the variable name in x.pOther` |
|        - | 1211 | ` * (a VM-lifetime bytecode string). This helper runs at each OP_CALL dispatch branch, while` |
|        - | 1212 | ` * pVm->pFrame is still the CALLER frame, once the callee's by-ref shape is known:` |
|        - | 1213 | ` *` |
|        - | 1214 | ` *   by-ref position  -> create the variable in the caller frame now and give the slot its` |
|        - | 1215 | ` *                       real nIdx so the ordinary by-ref binder aliases it (silent, as php).` |
|        - | 1216 | ` *   by-value position-> raise php's "Undefined variable $x" and pass a clean NULL WITHOUT` |
|        - | 1217 | ` *                       creating the variable in the caller.` |
|        - | 1218 | ` *` |
|        - | 1219 | ` * The by-ref decision for positional argument n comes from, in priority order:` |
|        - | 1220 | ` *   bAllByValue  -> everything by-value (an unresolvable/erroring callee);` |
|        - | 1221 | ` *   bAllByRef    -> everything by-ref (the indirect array-callable/__invoke dispatch paths,` |
|        - | 1222 | ` *                   which historically over-vivified every plain-var arg — preserved here` |
|        - | 1223 | ` *                   rather than regressed; their by-value refinement is a later slice);` |
|        - | 1224 | ` *   pFormal      -> a user function's formal-argument array (honoring a trailing variadic);` |
|        - | 1225 | ` *   nByRefMask   -> a builtin by-ref position bitmask (used when pFormal == 0).` |
|        - | 1226 | ` *` |
|        - | 1227 | ` * A DEFINED variable never carries the marker (it loads with its real nIdx), so this is a` |
|        - | 1228 | ` * no-op for it; a call with no deferred args pays only one flag test per slot.` |
|        - | 1229 | ` */` |
|  5047092 | 1230 | `PH7_PRIVATE sxi32 PH7_VmResolveDeferredArgs(` |
|        - | 1231 | `	ph7_vm *pVm,` |
|        - | 1232 | `	ph7_value *pArg,` |
|        - | 1233 | `	ph7_value *pTos,` |
|        - | 1234 | `	ph7_vm_func_arg *pFormal,` |
|        - | 1235 | `	sxu32 nFormal,` |
|        - | 1236 | `	sxu32 nByRefMask,` |
|        - | 1237 | `	int bAllByRef,` |
|        - | 1238 | `	int bAllByValue,` |
|        - | 1239 | `	VmCallArgMap *pCallMap)` |
|        5 | 1240 | `{` |
|        - | 1241 | `	ph7_value *p;` |
|  5047097 | 1242 | `	sxu32 n = 0;` |
| 10409190 | 1243 | `	for( p = pArg ; p < pTos ; ++p, ++n ){` |
|  5362142 | 1244 | `		int bByRef = 0;` |
|        - | 1245 | `		SyString sName;` |
|  5362142 | 1246 | `		if( (p->iFlags & (MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH)) == 0 ){` |
|  5338215 | 1247 | `			continue;` |
|        - | 1248 | `		}` |
|    47419 | 1249 | `		if( bAllByValue ){` |
|      ! 0 | 1250 | `			bByRef = 0;` |
|    47419 | 1251 | `		}else if( bAllByRef ){` |
|        - | 1252 | `			/* bAllByRef comes ONLY from the array-callable-value and __invoke dispatch paths,` |
|        - | 1253 | `			 * which cannot expose the target's per-parameter by-ref flags here. Commit 1 chose` |
|        - | 1254 | `			 * to over-vivify every deferred PLAIN-VAR arg on those paths (harmless: it just` |
|        - | 1255 | `			 * materializes the caller variable), and that is preserved. But a deferred` |
|        - | 1256 | `			 * ELEMENT/PROPERTY (MEMOBJ_AUX_DEFPATH) must NOT be blanket-vivified there: a missing` |
|        - | 1257 | `			 * property would fatal ("Cannot create dynamic property") and a missing element would` |
|        - | 1258 | `			 * silently vivify + swallow php's "Undefined array key" warning. Resolve those` |
|        - | 1259 | `			 * by-value (warn + pass NULL), which matches php for a by-VALUE __invoke/callable —` |
|        - | 1260 | `			 * the genuine by-ref-out-param-into-an-element case stays unsupported here, exactly` |
|        - | 1261 | `			 * as it was before this slice. */` |
|      ! 0 | 1262 | `			bByRef = (p->iFlags & MEMOBJ_AUX_DEFPATH) ? 0 : 1;` |
|    47419 | 1263 | `		}else if( pFormal ){` |
|      216 | 1264 | `			sxu32 idx = n;` |
|      212 | 1265 | `			if( pCallMap && pCallMap->bHasNamed && n < pCallMap->nTotal` |
|       22 | 1266 | `			 && pCallMap->aNames[n].nByte > 0 ){` |
|        - | 1267 | `				/* A NAMED actual binds to the formal its NAME picks, not to the one at its` |
|        - | 1268 | ``				 * stack position: `r(x: $a["k"])` is argument #1 on the stack and parameter`` |
|        - | 1269 | `				 * $x in the declaration. Reading the by-ref-ness positionally consulted the` |
|        - | 1270 | `				 * wrong formal, so a by-reference named argument naming a missing element` |
|        - | 1271 | ``				 * warned `Undefined array key` and passed NULL where php creates it. */`` |
|        - | 1272 | `				sxu32 f;` |
|       13 | 1273 | `				idx = SXU32_HIGH;` |
|       25 | 1274 | `				for( f = 0 ; f < nFormal ; ++f ){` |
|       24 | 1275 | `					if( pCallMap->aNames[n].nByte == SyStringLength(&pFormal[f].sName)` |
|       25 | 1276 | `					 && SyMemcmp(pCallMap->aNames[n].zString,` |
|       36 | 1277 | `						SyStringData(&pFormal[f].sName),pCallMap->aNames[n].nByte) == 0 ){` |
|       13 | 1278 | `						idx = f;` |
|       13 | 1279 | `						break;` |
|        - | 1280 | `					}` |
|        7 | 1281 | `				}` |
|      207 | 1282 | `			}else if( idx >= nFormal ){` |
|        - | 1283 | `				/* Beyond the declared formals: a trailing variadic absorbs the tail` |
|        - | 1284 | `				 * (and dictates its by-ref-ness); otherwise the extra arg is by-value. */` |
|      ! 0 | 1285 | `				idx = (nFormal > 0 && (pFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC))` |
|      ! 0 | 1286 | `					? nFormal - 1 : SXU32_HIGH;` |
|      ! 0 | 1287 | `			}` |
|      216 | 1288 | `			if( idx != SXU32_HIGH ){` |
|      216 | 1289 | `				bByRef = (pFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|      106 | 1290 | `			}` |
|      110 | 1291 | `		}else{` |
|    47207 | 1292 | `			bByRef = (n < 31 && (nByRefMask & (1u << n))) ? 1 : 0;` |
|        - | 1293 | `		}` |
|    47419 | 1294 | `		if( p->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|        - | 1295 | `			/* D1 commit 2: a deferred array-element/property lvalue. Detach the descriptor` |
|        - | 1296 | `			 * FIRST (so an exception mid-resolve, or a later stack release, cannot double-free` |
|        - | 1297 | `			 * it) then re-walk it in the chosen mode. */` |
|    47361 | 1298 | `			VmDeferredPath *pPath = (VmDeferredPath *)p->x.pOther;` |
|        - | 1299 | `			sxi32 rc;` |
|    47361 | 1300 | `			p->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|    47361 | 1301 | `			p->x.pOther = 0;` |
|    47361 | 1302 | `			MemObjSetType(p,MEMOBJ_NULL);` |
|    47361 | 1303 | `			p->nIdx = SXU32_HIGH;` |
|    47361 | 1304 | `			if( bByRef ){` |
|      132 | 1305 | `				rc = VmResolvePathByRef(&(*pVm),pPath,p);` |
|       68 | 1306 | `			}else{` |
|    47233 | 1307 | `				rc = VmResolvePathByValue(&(*pVm),pPath,p);` |
|        - | 1308 | `			}` |
|    47361 | 1309 | `			VmFreeDeferredPath(pPath);` |
|    47361 | 1310 | `			if( rc != SXRET_OK ){` |
|       48 | 1311 | `				return rc;` |
|        - | 1312 | `			}` |
|    47317 | 1313 | `			continue;` |
|        - | 1314 | `		}` |
|        - | 1315 | `		/* Recover the deferred variable name and drop the marker + carrier. */` |
|       60 | 1316 | `		SyStringInitFromBuf(&sName,(const char *)p->x.pOther,` |
|        - | 1317 | `			p->x.pOther ? SyStrlen((const char *)p->x.pOther) : 0);` |
|       60 | 1318 | `		p->iFlags &= ~MEMOBJ_AUX_DEFERRED;` |
|       60 | 1319 | `		p->x.pOther = 0;` |
|       60 | 1320 | `		if( bByRef ){` |
|        - | 1321 | `			/* Materialize in the caller frame; the value stays NULL, only the slot` |
|        - | 1322 | `			 * back-reference (nIdx) matters for the by-ref binder / write-back. */` |
|       47 | 1323 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|       47 | 1324 | `			if( pObj ){` |
|       47 | 1325 | `				p->nIdx = pObj->nIdx;` |
|       23 | 1326 | `			}` |
|       24 | 1327 | `		}else{` |
|        - | 1328 | `			/* php warns and passes NULL without creating the variable; the slot is` |
|        - | 1329 | `			 * already a clean NULL with nIdx == SXU32_HIGH from the deferred load. */` |
|       14 | 1330 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        - | 1331 | `		}` |
|       31 | 1332 | `	}` |
|  5047053 | 1333 | `	return SXRET_OK;` |
|  2524587 | 1334 | `}` |
|        - | 1335 | `/*` |
|        - | 1336 | ` * Did resolving a class NAME raise?` |
|        - | 1337 | ` *` |
|        - | 1338 | ` * The lookup can run an AUTOLOADER, and that autoloader can throw. The boundary rail` |
|        - | 1339 | ` * either parks the status in nBoundaryRc or — when a try caught it in place — records a` |
|        - | 1340 | ` * resume frame; either way the throw is already the engine's to land. A call site that` |
|        - | 1341 | `` * sees the class "missing" and piles its own `Class "X" not found` Error on top reports a`` |
|        - | 1342 | ` * failure php never reports, and that second Error belongs to nobody: it came back` |
|        - | 1343 | ` * UNCAUGHT and killed the script right after the real exception had been handled.` |
|        - | 1344 | ` *` |
|        - | 1345 | ` * Snapshot (nBoundaryRc, pResumeFrame) before the lookup and pass them here after.` |
|        - | 1346 | ` */` |
|      190 | 1347 | `PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore)` |
|        4 | 1348 | `{` |
|      194 | 1349 | `	return pVm->nBoundaryRc != nBrcBefore \|\| (const void *)pVm->pResumeFrame != pResumeBefore;` |
|        4 | 1350 | `}` |
|        - | 1351 | `/*` |
|        - | 1352 | `` * Name php's error for a class+method callable that the DIRECT `$cb()` dispatch cannot`` |
|        - | 1353 | ` * call, or return 0 when it resolves.` |
|        - | 1354 | ` *` |
|        - | 1355 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|        - | 1356 | ` * result for an unresolvable pair — silence a caller cannot detect — so the direct call` |
|        - | 1357 | ` * site has to decide for itself. It used to do that only for the ARRAY form; the` |
|        - | 1358 | ``  * `"Class::method"` STRING form went straight to the dispatcher, and `$cb='C::nosuch'` `` |
|        - | 1359 | ` * evaluated to NULL with no diagnostic at all where php throws.` |
|        - | 1360 | ` *` |
|        - | 1361 | ` * pClass is the resolved target class (0 when the name named nothing); zCls/nCls is the` |
|        - | 1362 | ` * class name AS WRITTEN, which is what php's not-found message quotes. bStaticForm says the` |
|        - | 1363 | ` * target was a class NAME rather than an object. Messages that interpolate a name are built` |
|        - | 1364 | ` * into zBuf.` |
|        - | 1365 | ` *` |
|        - | 1366 | ` * Visibility is NOT decided here: an inaccessible method is diagnosed downstream by the` |
|        - | 1367 | ` * dispatch itself ("Call to private method C::p() from global scope"), php-exact already —` |
|        - | 1368 | ` * and php reports visibility BEFORE staticness, so the static rule below has to stay quiet` |
|        - | 1369 | ` * for a method this scope could not reach anyway.` |
|        - | 1370 | ` */` |
|   200266 | 1371 | `static const char * VmCallableClassMethodError(` |
|        - | 1372 | `	ph7_vm *pVm,` |
|        - | 1373 | `	ph7_class *pClass,             /* Resolved target class, or 0 */` |
|        - | 1374 | `	const char *zCls,sxu32 nCls,   /* Its name as the callable wrote it */` |
|        - | 1375 | `	const char *zMeth,sxu32 nMeth, /* The method name */` |
|        - | 1376 | `	int bStaticForm,               /* TRUE when the target is a class NAME, not an object */` |
|        - | 1377 | `	char *zBuf,int nBuf            /* Scratch for the messages that quote a name */` |
|        - | 1378 | `	)` |
|        4 | 1379 | `{` |
|        - | 1380 | `	ph7_class_method *pMethod;` |
|        - | 1381 | `	SyString sMeth;` |
|        - | 1382 | `` 	/* php's fallback for a name this class cannot reach: when the CALLER holds a `$this` `` |
|        - | 1383 | `	 * that is an instance of it, the name resolves to the __call TRAMPOLINE rather than to` |
|        - | 1384 | `	 * __callStatic — and a trampoline is a NON-STATIC function, so this dispatch, which` |
|        - | 1385 | `	 * carries no object, refuses it exactly as it refuses any other non-static method named` |
|        - | 1386 | `	 * through a class. The callback spellings bind that receiver and run (php's` |
|        - | 1387 | `	 * direct-vs-callback asymmetry, one rule apart). The message names the class the` |
|        - | 1388 | `	 * callable WROTE and the name as written, even when the name is a declared static` |
|        - | 1389 | `	 * method: it is the trampoline being refused, not the method. */` |
|   200270 | 1390 | `	int bFallback = bStaticForm && PH7_VmStaticFallbackThis(&(*pVm),pClass) != 0;` |
|   200270 | 1391 | `	if( pClass == 0 ){` |
|       49 | 1392 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|       49 | 1393 | `		return zBuf;` |
|        - | 1394 | `	}` |
|   200222 | 1395 | `	pMethod = PH7_ClassExtractMethod(pClass,zMeth,nMeth);` |
|   200222 | 1396 | `	if( pMethod == 0 ){` |
|       79 | 1397 | `		if( bFallback ){` |
|        7 | 1398 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|        2 | 1399 | `				&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1400 | `			return zBuf;` |
|        - | 1401 | `		}` |
|        - | 1402 | `		/* A class that answers for unknown names through the catch-all has nothing to` |
|        - | 1403 | `		 * report: php runs __callStatic (class-name target) / __call (object target) for` |
|        - | 1404 | `		 * ANY method name, and the dispatcher below routes it. */` |
|       75 | 1405 | `		const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       75 | 1406 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|       47 | 1407 | `			return 0;` |
|        - | 1408 | `		}` |
|       43 | 1409 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",` |
|       14 | 1410 | `			&pClass->sName,(int)nMeth,zMeth);` |
|       29 | 1411 | `		return zBuf;` |
|        - | 1412 | `	}` |
|        - | 1413 | `	/* An ABSTRACT method (an interface's included) has no body to call — the same message` |
|        - | 1414 | ``	 * the `C::m()` SYNTAX raises in vm_ops_oo.c, which this dispatch reached only as a`` |
|        - | 1415 | `	 * mangled internal function name. */` |
|   200144 | 1416 | `	SyStringInitFromBuf(&sMeth,zMeth,nMeth);` |
|   200144 | 1417 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        7 | 1418 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%z()",&pClass->sName,&sMeth);` |
|        7 | 1419 | `		return zBuf;` |
|        - | 1420 | `	}` |
|        - | 1421 | `	/* Named through a class NAME, a non-static method is never callable: php refuses even` |
|        - | 1422 | `	 * when the CALLER has a compatible $this (unlike call_user_func, which binds it). The` |
|        - | 1423 | `	 * message names the OWNING class and the method's declared spelling. */` |
|        - | 1424 | `	{` |
|        - | 1425 | `		SyString sDecl;` |
|        - | 1426 | `		int bAccessible;` |
|   200138 | 1427 | `		SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1428 | `			SyStringLength(&pMethod->sFunc.sName));` |
|   300225 | 1429 | `		bAccessible = pMethod->iProtection == PH7_CLASS_PROT_PUBLIC` |
|   200136 | 1430 | `			\|\| PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       19 | 1431 | `				&sDecl,pMethod->iProtection,FALSE);` |
|   200138 | 1432 | `		if( !bAccessible && bFallback ){` |
|        - | 1433 | `			/* Inaccessible goes the same way as missing: php never reports the visibility,` |
|        - | 1434 | `			 * because the name resolved to the trampoline before visibility could matter. */` |
|       13 | 1435 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|        4 | 1436 | `				&pClass->sName,(int)nMeth,zMeth);` |
|       17 | 1437 | `			return zBuf;` |
|        - | 1438 | `		}` |
|   200130 | 1439 | `		if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 && bAccessible ){` |
|        - | 1440 | `			/* Deciding class vs NAMED class: a trait is php's compile-time construct, so` |
|        - | 1441 | `			 * every message names the class that composed it (PH7_VmMethodScopeName). */` |
|       25 | 1442 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|       16 | 1443 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|       17 | 1444 | `			return zBuf;` |
|        - | 1445 | `		}` |
|        - | 1446 | `	}` |
|   200114 | 1447 | `	return 0;` |
|   100137 | 1448 | `}` |
|        - | 1449 | `/*` |
|        - | 1450 | ` * php's visibility refusal for a method call, worded once: "Call to private A::m() from` |
|        - | 1451 | ` * scope S" (or "from global scope"). Two sites raise it — OP_CALL's screen and the` |
|        - | 1452 | ` * first-class-callable one below — and php names the DECLARING class, not the class the` |
|        - | 1453 | ` * lookup went through.` |
|        - | 1454 | ` */` |
|       24 | 1455 | `static const char * VmMethodVisibilityMsg(ph7_vm *pVm,ph7_class *pDecl,` |
|        - | 1456 | `	const char *zMeth,sxu32 nMeth,sxi32 iProtection,char *zBuf,int nBuf)` |
|        2 | 1457 | `{` |
|       26 | 1458 | `	const char *zVis = iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       26 | 1459 | `	ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));` |
|       26 | 1460 | `	if( pScope ){` |
|        4 | 1461 | `		SyBufferFormat(zBuf,nBuf,"Call to %s method %z::%.*s() from scope %z",` |
|        1 | 1462 | `			zVis,&pDecl->sName,(int)nMeth,zMeth,&pScope->sName);` |
|        2 | 1463 | `	}else{` |
|       35 | 1464 | `		SyBufferFormat(zBuf,nBuf,"Call to %s method %z::%.*s() from global scope",` |
|       11 | 1465 | `			zVis,&pDecl->sName,(int)nMeth,zMeth);` |
|        - | 1466 | `	}` |
|       26 | 1467 | `	return zBuf;` |
|        2 | 1468 | `}` |
|        - | 1469 | `/*` |
|        - | 1470 | ` * Is this class+method pair one a call would reach DIRECTLY from here — a real method (not` |
|        - | 1471 | ` * abstract, not a name only the catch-all answers) that the current scope may call? php` |
|        - | 1472 | ` * decides exactly this when it BUILDS a method Closure, and stores the resolved function; the` |
|        - | 1473 | ` * answer is what the VM_INSTANCE_FCC_SCREENED mark records, so the invocation never asks again.` |
|        - | 1474 | ` * A pair that answers FALSE here is the __call/__callStatic trampoline's, and its closure must` |
|        - | 1475 | ` * keep routing there.` |
|        - | 1476 | ` */` |
|      206 | 1477 | `PH7_PRIVATE int PH7_VmFccMethodIsDirect(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName)` |
|        5 | 1478 | `{` |
|        - | 1479 | `	ph7_class_method *pMethod;` |
|        - | 1480 | `	SyString sDecl;` |
|      211 | 1481 | `	if( pClass == 0 \|\| nName < 1 ){` |
|      ! 0 | 1482 | `		return 0;` |
|        - | 1483 | `	}` |
|      211 | 1484 | `	pMethod = PH7_ClassExtractMethod(pClass,zName,nName);` |
|      211 | 1485 | `	if( pMethod == 0 \|\| (pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       17 | 1486 | `		return 0;` |
|        - | 1487 | `	}` |
|      195 | 1488 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|      126 | 1489 | `		return 1;` |
|        - | 1490 | `	}` |
|       72 | 1491 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1492 | `		SyStringLength(&pMethod->sFunc.sName));` |
|        - | 1493 | `	/* The OWNING class decides (a trait method is owned by the class that composed it) — the` |
|        - | 1494 | `	 * same argument every other visibility site passes. */` |
|      106 | 1495 | `	return PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       68 | 1496 | `		&sDecl,pMethod->iProtection,FALSE) ? 1 : 0;` |
|      108 | 1497 | `}` |
|        - | 1498 | `/*` |
|        - | 1499 | `` * Resolve `$o->m(...)` / `C::m(...)` the way php resolves the CALL it stands for, and say`` |
|        - | 1500 | ` * why when it cannot. php builds a first-class callable through the same member lookup a` |
|        - | 1501 | ` * real call goes through, so every refusal a call would raise happens HERE, at creation:` |
|        - | 1502 | ` * an undefined method, an inaccessible one, an abstract one, and a non-static one named` |
|        - | 1503 | ` * through a class with no receiver to run on. PHL created a Closure for all four and only` |
|        - | 1504 | ` * discovered the problem when (and if) it was invoked — a closure that is built and dropped` |
|        - | 1505 | ` * reported nothing at all.` |
|        - | 1506 | ` *` |
|        - | 1507 | ` * The receiver is the other half of the same lookup. php's ZEND_INIT_STATIC_METHOD_CALL` |
|        - | 1508 | `` * binds the CALLING frame's `$this` when the resolved method is non-static and that object`` |
|        - | 1509 | `` * is an instance of the named class, which is what makes `self::m(...)` inside an instance`` |
|        - | 1510 | ` * method a working callable rather than a static one; PHL bound only the scope, so the` |
|        - | 1511 | ` * closure could never run. *ppRecv is that object, or 0 for a genuinely static callable.` |
|        - | 1512 | ` *` |
|        - | 1513 | ` * Answers 0 when the callable is valid. Messages that quote a name are built into zBuf.` |
|        - | 1514 | ` */` |
|      178 | 1515 | `static const char * VmFccMemberError(ph7_vm *pVm,ph7_class *pClass,` |
|        - | 1516 | `	const char *zCls,sxu32 nCls,const char *zMeth,sxu32 nMeth,int bStaticForm,` |
|        - | 1517 | `	ph7_class_instance **ppRecv,char *zBuf,int nBuf)` |
|        5 | 1518 | `{` |
|        - | 1519 | `	ph7_class_method *pMethod;` |
|        - | 1520 | `	SyString sDecl;` |
|      183 | 1521 | `	*ppRecv = 0;` |
|      183 | 1522 | `	if( pClass == 0 ){` |
|        3 | 1523 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|        3 | 1524 | `		return zBuf;` |
|        - | 1525 | `	}` |
|      181 | 1526 | `	pMethod = nMeth > 0 ? PH7_ClassExtractMethod(pClass,zMeth,nMeth) : 0;` |
|      181 | 1527 | `	if( pMethod == 0 ){` |
|        - | 1528 | `		/* A name the class answers through the catch-all is callable, and the catch-all` |
|        - | 1529 | `		 * the STATIC spelling reaches depends on the receiver, exactly as it does for a` |
|        - | 1530 | `		 * call (PH7_VmStaticFallbackThis). */` |
|       17 | 1531 | `		if( bStaticForm ){` |
|       11 | 1532 | `			*ppRecv = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|       10 | 1533 | `			if( *ppRecv` |
|       10 | 1534 | `			 \|\| PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1) ){` |
|        9 | 1535 | `				return 0;` |
|        1 | 1536 | `			}` |
|        8 | 1537 | `		}else if( PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) ){` |
|        5 | 1538 | `			return 0;` |
|        - | 1539 | `		}` |
|        7 | 1540 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",` |
|        2 | 1541 | `			&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1542 | `		return zBuf;` |
|        - | 1543 | `	}` |
|      165 | 1544 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1545 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      165 | 1546 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1547 | `		/* Named through the CLASS only: an instance of an abstract class cannot exist, so` |
|        - | 1548 | `		 * the object spelling never reaches an abstract body. */` |
|        7 | 1549 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%.*s()",` |
|        2 | 1550 | `			&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1551 | `		return zBuf;` |
|        - | 1552 | `	}` |
|      156 | 1553 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      117 | 1554 | `	 && !PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       34 | 1555 | `			&sDecl,pMethod->iProtection,FALSE) ){` |
|        - | 1556 | `		/* Inaccessible: the catch-all answers for it, on the same receiver a call would use. */` |
|       12 | 1557 | `		*ppRecv = bStaticForm ? PH7_VmStaticFallbackThis(&(*pVm),pClass) : 0;` |
|       12 | 1558 | `		if( *ppRecv ){` |
|      ! 0 | 1559 | `			return 0;` |
|        - | 1560 | `		}` |
|       12 | 1561 | `		if( !bStaticForm && PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) ){` |
|        3 | 1562 | `			return 0;` |
|        - | 1563 | `		}` |
|        - | 1564 | `		/* The DECIDING class is the declaring one (its trait grants live there); the class` |
|        - | 1565 | `		 * php NAMES is the composing one — a trait has no runtime existence in php. */` |
|       14 | 1566 | `		return VmMethodVisibilityMsg(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|        4 | 1567 | `			zMeth,nMeth,pMethod->iProtection,zBuf,nBuf);` |
|        - | 1568 | `	}` |
|      151 | 1569 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|       15 | 1570 | `		*ppRecv = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|       15 | 1571 | `		if( *ppRecv == 0 ){` |
|        7 | 1572 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|        4 | 1573 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|        5 | 1574 | `			return zBuf;` |
|        - | 1575 | `		}` |
|        5 | 1576 | `	}` |
|      147 | 1577 | `	return 0;` |
|       94 | 1578 | `}` |
|        - | 1579 | `/*` |
|        - | 1580 | ` * The same check for the ARRAY form, whose two members carry php's own shape messages` |
|        - | 1581 | ` * before anything is resolved: the target must be an object or a class-name string, the` |
|        - | 1582 | `` * method must be a string. php probes them in that order (`[5,5]` names the FIRST member,`` |
|        - | 1583 | `` * `['NoSuch',5]` the SECOND — the member shape decides before the class is looked up).`` |
|        - | 1584 | ` */` |
|   100186 | 1585 | `static const char * VmDirectArrayCallableError(ph7_vm *pVm,ph7_value *pTarget,ph7_value *pMethod,` |
|        - | 1586 | `	char *zBuf,int nBuf)` |
|        4 | 1587 | `{` |
|        - | 1588 | `	ph7_class *pClass;` |
|   100190 | 1589 | `	if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|       11 | 1590 | `		return "First array member is not a valid class name or object";` |
|        - | 1591 | `	}` |
|   100180 | 1592 | `	if( (pMethod->iFlags & MEMOBJ_STRING) == 0 ){` |
|        9 | 1593 | `		return "Second array member is not a valid method";` |
|        - | 1594 | `	}` |
|   100172 | 1595 | `	pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   150256 | 1596 | `	return VmCallableClassMethodError(&(*pVm),pClass,` |
|   100168 | 1597 | `		(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|   100168 | 1598 | `		(const char *)SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob),` |
|        - | 1599 | `		/* An OBJECT target carries its own $this; only a class NAME is the static form. */` |
|   100168 | 1600 | `		(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|    50084 | 1601 | `		zBuf,nBuf);` |
|    50097 | 1602 | `}` |
|        - | 1603 | `/*` |
|        - | 1604 | ` * The by-reference SHAPE of the callee an INDIRECT dispatch is about to reach — an` |
|        - | 1605 | `` * array callable VALUE (`$cb = [$o,'m']; $cb($a['k']);`) and an __invoke object.`` |
|        - | 1606 | ` * Both go through a shared helper that hides the target from OP_CALL, so the two` |
|        - | 1607 | ` * sites used to materialize EVERY deferred plain-var argument by reference and every` |
|        - | 1608 | ` * deferred element/property by value: a genuine by-ref out-param into an element was` |
|        - | 1609 | `` * unsupported (`$cb($a['new'])` warned `Undefined array key` and handed the callee a`` |
|        - | 1610 | ` * NULL where php creates the element and writes it), and a by-VALUE parameter` |
|        - | 1611 | `` * swallowed php's `Undefined variable` and CREATED the caller's variable.`` |
|        - | 1612 | ` *` |
|        - | 1613 | ` * The target is knowable here: the pair resolves to a class and a method, an object to` |
|        - | 1614 | ` * its __invoke. Answers 0 when nothing resolves — a name routed through` |
|        - | 1615 | ` * __call/__callStatic (php packs those into an ARRAY, so they are by-value anyway) or` |
|        - | 1616 | ` * a pair the screen above is about to refuse.` |
|        - | 1617 | ` */` |
|   300330 | 1618 | `static ph7_vm_func * VmIndirectCalleeFunc(ph7_vm *pVm,ph7_value *pCallable)` |
|        4 | 1619 | `{` |
|   300334 | 1620 | `	ph7_class_method *pMeth = 0;` |
|   300334 | 1621 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|   100245 | 1622 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|   100245 | 1623 | `		ph7_value *pTarget = 0,*pName = 0;` |
|        - | 1624 | `		ph7_class *pClass;` |
|   100242 | 1625 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|   100242 | 1626 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|   100245 | 1627 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1628 | `			return 0;` |
|        - | 1629 | `		}` |
|   100245 | 1630 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   100245 | 1631 | `		if( pClass == 0 ){` |
|      ! 0 | 1632 | `			return 0;` |
|        - | 1633 | `		}` |
|   150366 | 1634 | `		pMeth = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|   100242 | 1635 | `			SyBlobLength(&pName->sBlob));` |
|   250213 | 1636 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|   200092 | 1637 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|   200092 | 1638 | `		if( pThis == 0 ){` |
|      ! 0 | 1639 | `			return 0;` |
|        - | 1640 | `		}` |
|   200092 | 1641 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|   100044 | 1642 | `	}` |
|   300334 | 1643 | `	return pMeth ? &pMeth->sFunc : 0;` |
|   150169 | 1644 | `}` |
|        - | 1645 | `/*` |
|        - | 1646 | ` * Materialize an indirect dispatch's deferred arguments against that callee — the same` |
|        - | 1647 | ` * split OP_CALL makes for a direct one: a native method's by-ref positions come from its` |
|        - | 1648 | ` * signature mask, a PHP one's from its compiled formals, and an unresolved callee binds` |
|        - | 1649 | ` * everything by value (php's answer for the magic route it is about to take).` |
|        - | 1650 | ` */` |
|   300330 | 1651 | `static sxi32 VmResolveIndirectArgs(ph7_vm *pVm,ph7_value *pCallable,ph7_value *pArg,ph7_value *pTos,` |
|        - | 1652 | `	VmCallArgMap *pCallMap)` |
|        4 | 1653 | `{` |
|   300334 | 1654 | `	ph7_vm_func *pFn = VmIndirectCalleeFunc(&(*pVm),pCallable);` |
|   300334 | 1655 | `	if( pFn == 0 ){` |
|   100042 | 1656 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pCallMap);` |
|        - | 1657 | `	}` |
|   200294 | 1658 | `	if( pFn->iFlags & VM_FUNC_NATIVE ){` |
|        7 | 1659 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|        6 | 1660 | `			pFn->pNative ? pFn->pNative->nByRefMask : 0,0,0,pCallMap);` |
|        - | 1661 | `	}` |
|   300430 | 1662 | `	return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   200284 | 1663 | `		(ph7_vm_func_arg *)SySetBasePtr(&pFn->aArgs),SySetUsed(&pFn->aArgs),0,0,0,pCallMap);` |
|   150169 | 1664 | `}` |
|        - | 1665 | `/*` |
|        - | 1666 | `` * Why a VALUE cannot be made into a first-class callable. php answers `($v)(...)` with`` |
|        - | 1667 | `` * exactly what it answers `($v)()` — the taxonomy is the DIRECT dispatch's, word for word —`` |
|        - | 1668 | ` * so this walks the same three shapes the OP_CALL sites do and reuses their builders. PHL` |
|        - | 1669 | `` * left a non-callable value STANDING instead: `$x = 5; $f = ($x)(...);` evaluated to int(5),`` |
|        - | 1670 | ` * an array to the array, a misspelled function name to its own string — a value that is not` |
|        - | 1671 | ` * a Closure where php throws, silently, on every shape.` |
|        - | 1672 | ` *` |
|        - | 1673 | ` * Returns 0 when the value IS callable (unreachable through the FCC caller, which asks only` |
|        - | 1674 | ` * after the wrap declined, but it keeps the helper honest for a direct reader).` |
|        - | 1675 | ` */` |
|       74 | 1676 | `static const char * VmFccValueError(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|        4 | 1677 | `{` |
|       78 | 1678 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       23 | 1679 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       23 | 1680 | `		ph7_value *pTarget = 0,*pMeth = 0;` |
|        - | 1681 | `		const char *zWhy;` |
|       23 | 1682 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|        9 | 1683 | `			return "Array callback must have exactly two elements";` |
|        - | 1684 | `		}` |
|       15 | 1685 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pMeth) ){` |
|        3 | 1686 | `			return "Array callback has to contain indices 0 and 1";` |
|        - | 1687 | `		}` |
|       13 | 1688 | `		zWhy = VmDirectArrayCallableError(&(*pVm),pTarget,pMeth,zBuf,nBuf);` |
|       13 | 1689 | `		if( zWhy ){` |
|        9 | 1690 | `			return zWhy;` |
|        - | 1691 | `		}` |
|        - | 1692 | `		/* That check deliberately leaves VISIBILITY to OP_CALL's own screen, which raises it` |
|        - | 1693 | `		 * when the pair is finally called — and a first-class callable never gets there: php` |
|        - | 1694 | ``		 * refuses `[$o,'priv'](...)` at the creation, with the direct dispatch's wording.`` |
|        - | 1695 | `		 * Reached only for a pair PH7_VmIsCallable already declined, so a class routing the` |
|        - | 1696 | `		 * name through __call (which makes it callable) cannot arrive here. */` |
|        5 | 1697 | `		if( (pMeth->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMeth->sBlob) > 0 ){` |
|        5 | 1698 | `			ph7_class *pCbCls = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|        5 | 1699 | `			const char *zM = (const char *)SyBlobData(&pMeth->sBlob);` |
|        5 | 1700 | `			sxu32 nM = SyBlobLength(&pMeth->sBlob);` |
|        5 | 1701 | `			ph7_class_method *pCbMeth = pCbCls ? PH7_ClassExtractMethod(pCbCls,zM,nM) : 0;` |
|        4 | 1702 | `			if( pCbMeth && pCbMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|        5 | 1703 | `			 && !PH7_VmFccMethodIsDirect(&(*pVm),pCbCls,zM,nM) ){` |
|        7 | 1704 | `				return VmMethodVisibilityMsg(&(*pVm),` |
|        2 | 1705 | `					PH7_VmMethodScopeName(&(*pVm),pCbCls,pCbMeth),` |
|        2 | 1706 | `					zM,nM,pCbMeth->iProtection,zBuf,nBuf);` |
|        - | 1707 | `			}` |
|      ! 0 | 1708 | `		}` |
|      ! 0 | 1709 | `		return 0;` |
|        - | 1710 | `	}` |
|       56 | 1711 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       15 | 1712 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|       15 | 1713 | `		if( pObj == 0 ){` |
|      ! 0 | 1714 | `			return "Value of type object is not callable";` |
|        - | 1715 | `		}` |
|       15 | 1716 | `		if( PH7_ClassExtractMethod(pObj->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|      ! 0 | 1717 | `			return 0;` |
|        - | 1718 | `		}` |
|       15 | 1719 | `		SyBufferFormat(zBuf,nBuf,"Object of type %z is not callable",&pObj->pClass->sName);` |
|       15 | 1720 | `		return zBuf;` |
|        - | 1721 | `	}` |
|       42 | 1722 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       30 | 1723 | `		const char *zCls = 0,*zMeth = 0;` |
|       30 | 1724 | `		sxu32 nCls = 0,nMeth = 0;` |
|        - | 1725 | `		SyString sName;` |
|       30 | 1726 | `		SyStringInitFromBuf(&sName,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|        - | 1727 | `		/* A leading backslash only anchors the name to the global namespace. */` |
|       30 | 1728 | `		if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|      ! 0 | 1729 | `			sName.zString++;` |
|      ! 0 | 1730 | `			sName.nByte--;` |
|      ! 0 | 1731 | `		}` |
|       30 | 1732 | `		if( PH7_VmCallableStringParts(sName.zString,sName.nByte,&zCls,&nCls,&zMeth,&nMeth) ){` |
|        - | 1733 | `			/* "Class::method" carries the class/method taxonomy, not the function one. */` |
|        7 | 1734 | `			return VmCallableClassMethodError(&(*pVm),` |
|        2 | 1735 | `				PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0),` |
|        2 | 1736 | `				zCls,nCls,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|        - | 1737 | `		}` |
|       26 | 1738 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined function %z()",&sName);` |
|       26 | 1739 | `		return zBuf;` |
|        - | 1740 | `	}` |
|       13 | 1741 | `	SyBufferFormat(zBuf,nBuf,"Value of type %s is not callable",VmArithTypeName(pValue));` |
|       13 | 1742 | `	return zBuf;` |
|       41 | 1743 | `}` |
|        - | 1744 | `/*` |
|        - | 1745 | `` * Split a `"Class::method"` callable string. php scans for the LAST "::" (zend_memrchr),`` |
|        - | 1746 | `` * so `"C::s::x"` names the class `C::s` — not `C` — and reports it not found. Returns TRUE`` |
|        - | 1747 | `` * and the two halves (either may be empty: `"C::"` and `"::s"` are shapes php accepts here`` |
|        - | 1748 | ` * and rejects further down), FALSE when the string carries no "::" at all.` |
|        - | 1749 | ` */` |
|  1891952 | 1750 | `PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,` |
|        - | 1751 | `	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth)` |
|        5 | 1752 | `{` |
|        - | 1753 | `	sxu32 i;` |
| 14979017 | 1754 | `	for( i = nName ; i >= 2 ; --i ){` |
| 13287261 | 1755 | `		if( zName[i-2] == ':' && zName[i-1] == ':' ){` |
|   200200 | 1756 | `			*pzCls = zName;` |
|   200200 | 1757 | `			*pnCls = i - 2;` |
|   200200 | 1758 | `			*pzMeth = &zName[i];` |
|   200200 | 1759 | `			*pnMeth = nName - i;` |
|   200200 | 1760 | `			return TRUE;` |
|        - | 1761 | `		}` |
|  6552867 | 1762 | `	}` |
|  1691761 | 1763 | `	return FALSE;` |
|   947017 | 1764 | `}` |
|  3401297 | 1765 | `static sxi32 VmByteCodeExecBody(` |
|        - | 1766 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 1767 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - | 1768 | `	ph7_value *pStack,   /* Operand stack */` |
|        - | 1769 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - | 1770 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - | 1771 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - | 1772 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - | 1773 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - | 1774 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - | 1775 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|        - | 1776 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|        - | 1777 | `	ph7_value **ppBaseOwner, /* Base (pCallTop==0) operand-stack owner slot the native entry frees; OP_SPREAD growth of the base stack writes the new pointer here (see VmGrowOperandStack). */` |
|        - | 1778 | `	sxu32 *pnBaseCap, /* Base stack capacity (persisted across coroutine suspend/resume); read for the initial capacity and updated on base-stack growth. */` |
|        - | 1779 | `	sxu32 nStackOrig /* Base stack's ORIGINAL (ungrown) allocation size — the fixed headroom reference for OP_SPREAD growth (see nStackOrig in VmExecState). */` |
|        - | 1780 | `	)` |
|        5 | 1781 | `{` |
|        - | 1782 | `	VmInstr *pInstr;` |
|        - | 1783 | `	ph7_value *pTos;` |
|        - | 1784 | `	SySet aArg;` |
|  3401302 | 1785 | `	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record` |
|        - | 1786 | `	                            * stack (BYTECODE stage 2); NULL = executing the` |
|        - | 1787 | `	                            * bottom activation. */` |
|        - | 1788 | `	VmExecState sState; /* This activation's boundary state (BYTECODE.md stage 1):` |
|        - | 1789 | `	                     * everything a suspended/nested activation must restore.` |
|        - | 1790 | `	                     * pc/pTos stay in locals for the hot loop and are synced` |
|        - | 1791 | `	                     * into sState only around the call epilogue (stage 2 turns` |
|        - | 1792 | `	                     * that boundary into an explicit record push/pop). */` |
|        - | 1793 | `	sxi32 pc;` |
|        - | 1794 | `	sxi32 rc;` |
|  3401302 | 1795 | `	sState.aInstr = aInstr;` |
|  3401302 | 1796 | `	sState.pStack = pStack;` |
|  3401302 | 1797 | `	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */` |
|  3401302 | 1798 | `	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */` |
|  3401302 | 1799 | `	sState.pResult = pResult;` |
|  3401302 | 1800 | `	sState.pLastRef = pLastRef;` |
|  3401302 | 1801 | `	sState.pEnforceRetFunc = pEnforceRetFunc;` |
|  3401302 | 1802 | `	sState.is_callback = (sxu8)(is_callback ? 1 : 0);` |
|  3401302 | 1803 | `	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);` |
|        - | 1804 | `	/* Argument container */` |
|  3401302 | 1805 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|  3401302 | 1806 | `	if( nTos < 0 ){` |
|  1497176 | 1807 | `		pTos = &pStack[-1];` |
|   748588 | 1808 | `	}else{` |
|  1904131 | 1809 | `		pTos = &pStack[nTos];` |
|        - | 1810 | `	}` |
|  3401302 | 1811 | `	sState.pTos = pTos;` |
|  3401302 | 1812 | `	sState.pc = nPc;` |
|        - | 1813 | `	/* Finally-drain base. For a resumed generator/fiber TOP-LEVEL body, its own` |
|        - | 1814 | `	 * exception handlers were just re-published above the caller depth` |
|        - | 1815 | `	 * (VmRestoreCtxState), so the live SySetUsed over-counts; take the` |
|        - | 1816 | `	 * caller-depth base recorded on the ctx instead.` |
|        - | 1817 | `	 *` |
|        - | 1818 | `	 * The discriminator is the OPERAND STACK, not the frame: the body exec runs on` |
|        - | 1819 | `	 * the ctx's own pStack, whereas every nested frame-less mini-program run within` |
|        - | 1820 | `	 * it — a default-argument / constructor trampoline, or a catch/finally body via` |
|        - | 1821 | `	 * VmLocalExec — runs on a FRESH operand stack while SHARING pVm->pFrame (those` |
|        - | 1822 | `	 * mini-programs do not push a VM frame). A pFrame-only guard therefore misfires` |
|        - | 1823 | `	 * for such a mini-program and hands it the generator's low caller-base; its` |
|        - | 1824 | `	 * terminal OP_DONE then drains VmDrainFinally down to that base, tearing down a` |
|        - | 1825 | ``	 * live try that the surrounding finally had just opened (e.g. `finally { try {`` |
|        - | 1826 | ``	 * throw new E(); } catch (E) {} }` in a generator: the `new E()` trampoline's`` |
|        - | 1827 | `	 * OP_DONE popped the inner try before its OP_THROW ran, so the throw escaped` |
|        - | 1828 | `	 * uncaught). Requiring pStack == pCtx->pStack pins the override to the resumed` |
|        - | 1829 | `	 * body itself; nested mini-programs fall through to the correct live depth. */` |
|  3401297 | 1830 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame` |
|     2468 | 1831 | `	 && pStack == pVm->pActiveCtx->pStack ){` |
|     1951 | 1832 | `		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;` |
|     1951 | 1833 | `		sState.nFinallyActBase = pVm->pActiveCtx->nFinallyBase;` |
|      978 | 1834 | `	}else{` |
|  3399356 | 1835 | `		sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|  3399356 | 1836 | `		sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|        - | 1837 | `	}` |
|  3401302 | 1838 | `	sState.pEntryFrame = pVm->pFrame;` |
|  3401302 | 1839 | `	pc = nPc;` |
|        - | 1840 | `	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call` |
|        - | 1841 | `	 * adopts its parked record segment here. VmResumeCtx hands the segment in` |
|        - | 1842 | `	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a` |
|        - | 1843 | `	 * nested mini-program/callback run inside the resumed body — after it rebased` |
|        - | 1844 | `	 * the segment's exception floors and pushed the resume value into the innermost` |
|        - | 1845 | `	 * stack. Locals switch to the innermost activation so the dispatch loop` |
|        - | 1846 | `	 * continues inside the callee; the record chain is restored so its completion` |
|        - | 1847 | `	 * unwinds back through the body. */` |
|  3401302 | 1848 | `	if( pAdoptSegment ){` |
|      106 | 1849 | `		VmParkedSegment *pSeg = pAdoptSegment;` |
|      106 | 1850 | `		pCallTop = pSeg->pCallTop;` |
|      106 | 1851 | `		sState = pSeg->sState;` |
|      106 | 1852 | `		aInstr = sState.aInstr;` |
|      106 | 1853 | `		pStack = sState.pStack;` |
|        - | 1854 | `		/* pc is already nPc (== pCtx->pc, the innermost's post-suspend pc) from the` |
|        - | 1855 | `		 * init above; only the stack/top move to the innermost activation. */` |
|      106 | 1856 | `		pTos = &pStack[nTos];   /* nTos == pCtx->nTos — innermost, resume value pushed */` |
|      106 | 1857 | `		SyMemBackendFree(&pVm->sAllocator,pSeg); /* holder only; its contents are now live */` |
|       51 | 1858 | `	}` |
|        - | 1859 | `/*` |
|        - | 1860 | ` * Route an enforcement helper's (or yield-from delegate's) return code from inside` |
|        - | 1861 | ` * the main switch: proceed on SXRET_OK, abort on PH7_ABORT, and on PH7_EXCEPTION` |
|        - | 1862 | ` * resume at the landing pad of the body that actually caught the exception in place` |
|        - | 1863 | ` * (VmRecordedResume) or, when it was caught by an outer exec, unwind out of the VM` |
|        - | 1864 | ` * loop. Replaces the old "jump to pVm->pFrame's nearest iExceptionJump" which ran` |
|        - | 1865 | ` * the statement after the try even when the catch was at an enclosing frame (ROOT B,` |
|        - | 1866 | `` * face b — `yield from` over a throwing sub-generator).`` |
|        - | 1867 | ` */` |
|        - | 1868 | `/* Dispatch-routing macros: bodies live in vm_dispatch.h, written against the` |
|        - | 1869 | ` * VM_EXIT_* primitives. Here they bind to the loop's own control flow. */` |
|        - | 1870 | `#define VM_EXIT_BREAK break` |
|        - | 1871 | `#define VM_EXIT_ABORT goto Abort` |
|        - | 1872 | `#define VM_EXIT_EXCEPTION goto Exception` |
|        - | 1873 | `#include "vm_dispatch.h"` |
|        - | 1874 | `	/* Generator::throw() inject-at-yield: when this invocation is a resumed generator/fiber` |
|        - | 1875 | `	 * body carrying a pending injected exception, raise it HERE — once, before the dispatch` |
|        - | 1876 | `	 * loop (pc is already at the resume point) — so the existing OP_THROW route` |
|        - | 1877 | `	 * (VmThrowException + VmRecordedResume) lands it at the generator's own try/catch landing` |
|        - | 1878 | `	 * pad WITHOUT reconstructing the suspended exception frame on pVm->pFrame. Because` |
|        - | 1879 | `	 * pVm->pFrame stays the body, the return/finally/sRet subsystem is untouched (this is why` |
|        - | 1880 | `	 * the reverted frame-reconstruction approach's regression cannot recur). Behaviorally` |
|        - | 1881 | ``	 * identical to a `throw` executed at the yield point: if the generator's own try catches`` |
|        - | 1882 | `	 * it we resume after the try; otherwise it propagates to the throw() caller (ROOT B lands` |
|        - | 1883 | `	 * the caller's handler) and the ctx closes. pInjected is one-shot and only meaningful at` |
|        - | 1884 | `	 * the resume pc, so this is checked once at entry — NOT per-instruction — keeping the hot` |
|        - | 1885 | `	 * dispatch loop untouched for all normal code. The pFrame gate keeps a nested call / catch` |
|        - | 1886 | `	 * mini-program sharing the ctx (a separate VmByteCodeExec entry) from re-firing.` |
|        - | 1887 | `	 *` |
|        - | 1888 | ``	 * Exception: when this body is suspended mid `yield from` over an inner Generator`` |
|        - | 1889 | `	 * (iDelegateState==3), a Generator::throw() on the OUTER generator must be forwarded` |
|        - | 1890 | `	 * INTO the delegate (PHP yield-from transparency), not raised here. Leave pInjected` |
|        - | 1891 | `	 * set and skip; the resume pc is that OP_YIELD_FROM, which consumes and forwards it. */` |
|  3401297 | 1892 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected` |
|     1320 | 1893 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       63 | 1894 | `	 && pVm->pActiveCtx->iDelegateState != 3 ){` |
|       58 | 1895 | `		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;` |
|        - | 1896 | `		VmFrame *pThrowFrame;` |
|        - | 1897 | `		sxi32 iResumePc;` |
|       58 | 1898 | `		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */` |
|        - | 1899 | `		/* Raising the injection at the yield-from point abandons any array/Iterator` |
|        - | 1900 | `		 * delegation in progress (state 1/2 — state 3 was forwarded, not raised here).` |
|        - | 1901 | `		 * Tear the delegate down so that if the generator's own try catches this and` |
|        - | 1902 | ``		 * runs on to a LATER `yield from`, that opcode classifies its operand fresh`` |
|        - | 1903 | `		 * instead of resuming this now-stale delegate cursor. */` |
|       58 | 1904 | `		if( pVm->pActiveCtx->iDelegateState != 0 ){` |
|        3 | 1905 | `			PH7_MemObjRelease(&pVm->pActiveCtx->sDelegate);` |
|        3 | 1906 | `			pVm->pActiveCtx->pDelegateNode = 0;` |
|        3 | 1907 | `			pVm->pActiveCtx->iDelegateState = 0;` |
|        1 | 1908 | `		}` |
|       58 | 1909 | `		pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       58 | 1910 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|       58 | 1911 | `		rc = VmThrowException(&(*pVm),pInj);` |
|       58 | 1912 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1913 | `			goto Abort;` |
|        - | 1914 | `		}` |
|       58 | 1915 | `		if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 1916 | `			/* ROOT C: the inject was caught by an inline try in THIS generator. Drain the` |
|        - | 1917 | `			 * abandoned mid-expression operands and land at the catch/finally body. This is` |
|        - | 1918 | `			 * the pre-loop path (the first fetch uses pc directly), so no -1 adjustment. */` |
|       92 | 1919 | `			while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|       48 | 1920 | `				PH7_MemObjRelease(pTos);` |
|       48 | 1921 | `				pTos--;` |
|        4 | 1922 | `			}` |
|       48 | 1923 | `			pc = (sxi32)pVm->iInlinePc;` |
|       48 | 1924 | `			pVm->pInlineInstr = 0;` |
|       34 | 1925 | `		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 1926 | `			/* Caught by THIS generator's own try. (VmRecordedResume returns FALSE unless a` |
|        - | 1927 | `			 * catch recorded a resume target for this exec, so rc need not be pre-checked;` |
|        - | 1928 | `			 * unlike OP_THROW there is no lexical-try fallthrough here — the else just` |
|        - | 1929 | `			 * propagates.) Drain the abandoned mid-expression operand slots back to the` |
|        - | 1930 | `			 * catching try's base, then land at its pad (iResumePc is landing-1 for the` |
|        - | 1931 | `			 * dispatcher's trailing pc++; the loop below fetches at pc with no leading pc++,` |
|        - | 1932 | `			 * so add 1 to land on the pad itself). */` |
|      ! 0 | 1933 | `			while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      ! 0 | 1934 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 | 1935 | `				pTos--;` |
|      ! 0 | 1936 | `			}` |
|      ! 0 | 1937 | `			pc = iResumePc + 1;` |
|      ! 0 | 1938 | `		}else{` |
|        - | 1939 | `			/* Not caught in this generator (no match, or caught by an outer/caller frame` |
|        - | 1940 | `			 * that ROOT B will land at its own OP_CALL site): propagate out so the ctx` |
|        - | 1941 | `			 * closes and the caller sees the exception. */` |
|       12 | 1942 | `			goto Exception;` |
|        - | 1943 | `		}` |
|       22 | 1944 | `	}` |
|        - | 1945 | `	/* Force-close entry (VmCloseCtx): a suspended generator being destroyed runs its` |
|        - | 1946 | ``	 * pending `finally` blocks. Instead of resuming at the yield, redirect straight`` |
|        - | 1947 | ``	 * into the innermost open try's finally as if a `return` had crossed every`` |
|        - | 1948 | `	 * enclosing finally (mirrors OP_SET_FINALLY_RET; OP_END_FINALLY then threads the` |
|        - | 1949 | `	 * PH7_FA_RETURN out through the whole chain and completes the body). Gate on the` |
|        - | 1950 | `	 * BODY bytecode (aInstr == the function program) so nested mini-programs sharing` |
|        - | 1951 | `	 * this ctx/frame never re-fire it; bClosing stays set so OP_YIELD can reject a` |
|        - | 1952 | `	 * yield reached inside one of these finallys. */` |
|  3401287 | 1953 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing` |
|     1314 | 1954 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       61 | 1955 | `	 && aInstr == (VmInstr *)SySetBasePtr(&pVm->pActiveCtx->pFunc->aByteCode) ){` |
|        - | 1956 | `		VmFinallyAction sAct;` |
|       60 | 1957 | `		sxu32 iFpc = 0;` |
|       60 | 1958 | `		int nCross = -1; /* cross every enclosing finally of this body */` |
|       60 | 1959 | `		SyZero(&sAct,sizeof(sAct));` |
|       60 | 1960 | `		sAct.eKind = PH7_FA_RETURN;` |
|       60 | 1961 | `		sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       60 | 1962 | `		PH7_MemObjInit(pVm,&sAct.sRet); /* discarded return; getReturn() is moot post-close */` |
|       60 | 1963 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|       21 | 1964 | `			sAct.nCross = nCross;` |
|       21 | 1965 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       21 | 1966 | `			pc = (sxi32)iFpc; /* pre-loop: the first fetch uses pc directly (no -1) */` |
|       11 | 1967 | `		}else{` |
|        - | 1968 | `			/* No open try had a finally: nothing to run, complete the body. */` |
|       39 | 1969 | `			PH7_MemObjRelease(&sAct.sRet);` |
|       39 | 1970 | `			goto Done;` |
|        - | 1971 | `		}` |
|       10 | 1972 | `	}` |
|        - | 1973 | `	/* Execute as much as we can */` |
| 23687600 | 1974 | `	for(;;){` |
|      ! 0 | 1975 | `VmLoopFetch:` |
|        - | 1976 | `		/* C-boundary throw routing (band A #1): a PHP callee invoked from a C` |
|        - | 1977 | `		 * site with no status channel — a __toString/__toInt cast, __get/__set/` |
|        - | 1978 | `		 * offsetGet/offsetSet, __clone, __destruct, a user callback inside a` |
|        - | 1979 | `		 * builtin — raised, and the site continued with a fallback value; the` |
|        - | 1980 | `		 * invocation boundary parked the status here (VmBoundaryPark). Route it` |
|        - | 1981 | `		 * exactly as the throw site's dispatch macro would have: abort, land at` |
|        - | 1982 | `		 * an inline-try redirect, resume at a recorded in-place catch (draining` |
|        - | 1983 | `		 * the abandoned mid-expression operands to the catching try's base), or` |
|        - | 1984 | `		 * propagate out of this exec. Checked at the fetch point, so a swallowed` |
|        - | 1985 | `		 * throw outlives at most the C remainder of ONE opcode instead of` |
|        - | 1986 | `		 * silently resuming the surrounding PHP code with a bogus value.` |
|        - | 1987 | `		 * The pending write-back sweep shares this one guard so the hot` |
|        - | 1988 | `		 * no-hooks path pays a single predicted branch per fetch. */` |
| 48466547 | 1989 | `		if( pVm->nBoundaryRc != 0 \|\| SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      908 | 1990 | `			if( pVm->nBoundaryRc != 0 ){` |
|      436 | 1991 | `				sxi32 rcBr = pVm->nBoundaryRc;` |
|      436 | 1992 | `				pVm->nBoundaryRc = 0;` |
|      436 | 1993 | `				if( rcBr == PH7_ABORT ){` |
|        3 | 1994 | `					goto Abort;` |
|        - | 1995 | `				}` |
|      434 | 1996 | `				if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 1997 | `					/* Caught by an inline try (generator body) THIS exec owns: drain` |
|        - | 1998 | `					 * and land (pre-fetch path: pc is used directly, no trailing ++). */` |
|      ! 0 | 1999 | `					while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|      ! 0 | 2000 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 | 2001 | `						pTos--;` |
|      ! 0 | 2002 | `					}` |
|      ! 0 | 2003 | `					pc = (sxi32)pVm->iInlinePc;` |
|      ! 0 | 2004 | `					pVm->pInlineInstr = 0;` |
|      ! 0 | 2005 | `				}else{` |
|        - | 2006 | `					sxi32 iBrPc;` |
|      434 | 2007 | `					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){` |
|        - | 2008 | `						/* Caught in place by a try THIS exec owns: drain the abandoned` |
|        - | 2009 | `						 * operands to the catching try's base and land at its pad` |
|        - | 2010 | `						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;` |
|        - | 2011 | `						 * pre-fetch here, so +1 lands on the pad itself). */` |
|      519 | 2012 | `						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      278 | 2013 | `							PH7_MemObjRelease(pTos);` |
|      278 | 2014 | `							pTos--;` |
|        5 | 2015 | `						}` |
|      246 | 2016 | `						pc = iBrPc + 1;` |
|      125 | 2017 | `					}else{` |
|        - | 2018 | `						/* Caught by an outer exec (or uncaught-with-report pending):` |
|        - | 2019 | `						 * unwind out of this exec; the owner's macros/router land it. */` |
|      193 | 2020 | `						goto Exception;` |
|        - | 2021 | `					}` |
|        - | 2022 | `				}` |
|      120 | 2023 | `			}` |
|        - | 2024 | `			/* Stale write-back sweep: a pending entry whose OWNING activation is` |
|        - | 2025 | `			 * fetching any pc outside its armed window [nJmpPc, nPc] is dead — for` |
|        - | 2026 | `			 * an RMW entry (window = the modify op alone) a routed throw abandoned` |
|        - | 2027 | `			 * the arming statement mid-flight; for a ??= entry either a throw` |
|        - | 2028 | `			 * abandoned the RHS or the short-circuit jump landed past the` |
|        - | 2029 | `			 * OP_NULLC_STORE (the assign is skipped). Drop it — no set dispatch` |
|        - | 2030 | `			 * (php: the throw/skip discards the write). A nested exec (even a` |
|        - | 2031 | `			 * recursive one over the same bytecode) has a different operand-stack` |
|        - | 2032 | `			 * base and leaves enclosing entries alone; entries below a live top` |
|        - | 2033 | `			 * are reached as the drops expose them. */` |
|      730 | 2034 | `			while( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      473 | 2035 | `				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      472 | 2036 | `				if( pTopRmw->pOwnerStack != (void *)pStack \|\| pTopRmw->pInstrs != (void *)aInstr` |
|      169 | 2037 | `				 \|\| ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){` |
|      231 | 2038 | `					break; /* not ours, or legitimately in flight */` |
|        - | 2039 | `				}` |
|       13 | 2040 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 2041 | `			}` |
|      356 | 2042 | `		}` |
|        - | 2043 | `		/* Fetch the instruction to execute */` |
| 48466357 | 2044 | `		pInstr = &aInstr[pc];` |
| 48466357 | 2045 | `		if( pInstr->nLine ){` |
|        - | 2046 | `			/* Publish the source position for diagnostics, debug_backtrace() and` |
|        - | 2047 | `			 * Throwable. Instructions the compiler could not attribute (nLine 0)` |
|        - | 2048 | `			 * leave the last known line standing rather than reporting line 0.` |
|        - | 2049 | `			 * The compilation unit's strict_types mode rides the same gate: a` |
|        - | 2050 | `			 * SYNTHETIC instruction (nLine 0) must not claim to speak for a file. */` |
| 45012054 | 2051 | `			pVm->nCurLine = pInstr->nLine;` |
| 45012054 | 2052 | `			pVm->bCurStrict = pInstr->bStrict;` |
| 22526448 | 2053 | `		}` |
| 48466357 | 2054 | `		rc = SXRET_OK;` |
|        - | 2055 | `/*` |
|        - | 2056 | ` * What follows here is a massive switch statement where each case implements a` |
|        - | 2057 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - | 2058 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - | 2059 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - | 2060 | ` * the switch statement will break with convention and be flush-left.` |
|        - | 2061 | ` */` |
| 48466357 | 2062 | `		switch(pInstr->iOp){` |
|        - | 2063 | `/*` |
|        - | 2064 | ` * DONE: P1 * *` |
|        - | 2065 | ` *` |
|        - | 2066 | ` * Program execution completed: Clean up the mess left behind` |
|        - | 2067 | ` * and return immediately.` |
|        - | 2068 | ` */` |
|  1569065 | 2069 | `case PH7_OP_DONE:` |
|  3138551 | 2070 | `	if( pInstr->iP2 && sState.bReturnPropagates ){` |
|        - | 2071 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - | 2072 | `		 * onto the body frame this catch/finally returns from (skip the transparent` |
|        - | 2073 | `		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION` |
|        - | 2074 | `		 * materializes it into sState.pResult. Drain any finally opened within this body` |
|        - | 2075 | `		 * first (nested try/finally inside the catch), which may overwrite the same` |
|        - | 2076 | `		 * frame's slot (finally-over-catch). */` |
|    20441 | 2077 | `		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|    20441 | 2078 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    20435 | 2079 | `			PH7_MemObjStore(pTos,&pTgt->sRet);` |
|    20435 | 2080 | `			VmPopOperand(&pTos,1);` |
|    10220 | 2081 | `		}else{` |
|        8 | 2082 | ``			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */`` |
|        - | 2083 | `		}` |
|    20441 | 2084 | `		pTgt->bHasRet = 1;` |
|    20441 | 2085 | `		pTgt->nRetGen++;` |
|    20441 | 2086 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    20441 | 2087 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2088 | `			goto Abort;` |
|        - | 2089 | `		}` |
|    20441 | 2090 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 2091 | `			/* A drained finally threw past itself — it discards this return. */` |
|      ! 0 | 2092 | `			goto Exception;` |
|        - | 2093 | `		}` |
|    20441 | 2094 | `		goto Done;` |
|        - | 2095 | `	}` |
|        - | 2096 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - | 2097 | `	 * the fiber start/resume paths) set sState.pEnforceRetFunc, so this branch is` |
|        - | 2098 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - | 2099 | `	 * callback trampolines, and the main script. */` |
|  3118110 | 2100 | `	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)` |
|    12707 | 2101 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - | 2102 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - | 2103 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - | 2104 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - | 2105 | `		 * value the function never actually returned, so enforcing here would` |
|        - | 2106 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - | 2107 | `		 * exception. */` |
|    12707 | 2108 | `		ph7_value *pRetVal = 0;` |
|    12707 | 2109 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    10729 | 2110 | `			pRetVal = pTos;` |
|     5359 | 2111 | `		}` |
|    12707 | 2112 | `		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);` |
|    12707 | 2113 | `		if( rc == PH7_ABORT ) goto Abort;` |
|    12703 | 2114 | `		if( rc == PH7_EXCEPTION ){` |
|      147 | 2115 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      113 | 2116 | `				PH7_MemObjRelease(pTos);` |
|      113 | 2117 | `				pTos--;` |
|       54 | 2118 | `			}` |
|      147 | 2119 | `			goto Exception;` |
|        - | 2120 | `		}` |
|        - | 2121 | `		/* Don't enforce twice if the function loops through multiple` |
|        - | 2122 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - | 2123 | `		 * defensively we clear the pointer after a successful check). */` |
|    12561 | 2124 | `		sState.pEnforceRetFunc = 0;` |
|     6275 | 2125 | `	}` |
|  3117969 | 2126 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|  1650112 | 2127 | `		if( sState.pLastRef ){` |
|   129515 | 2128 | `			*sState.pLastRef = pTos->nIdx;` |
|    64972 | 2129 | `		}` |
|  1650112 | 2130 | `		if( sState.pResult ){` |
|        - | 2131 | `			/* Execution result */` |
|   179990 | 2132 | `			PH7_MemObjStore(pTos,sState.pResult);` |
|    90201 | 2133 | `		}` |
|  1650112 | 2134 | `		VmPopOperand(&pTos,1);` |
|   825267 | 2135 | `	}else{` |
|  1467862 | 2136 | `		if( pInstr->iP1 == 0 && pInstr->iP2 && sState.pResult ){` |
|        - | 2137 | ``			/* An EXPLICIT `return;` with no value answers NULL. It reads as a`` |
|        - | 2138 | `			 * no-op for a function (whose result slot starts out null anyway)` |
|        - | 2139 | `			 * and matters for an included CHUNK, whose slot is seeded with the 1` |
|        - | 2140 | ``			 * a file that returns nothing answers: `<?php return;` is php's`` |
|        - | 2141 | `			 * NULL, not that 1. */` |
|       45 | 2142 | `			PH7_MemObjRelease(sState.pResult);` |
|       21 | 2143 | `		}` |
|        - | 2144 | `		/* Nothing referenced — also the throw-unwind path: the compiler routes` |
|        - | 2145 | `		 * an uncaught exception to this terminal OP_DONE with iP1 set but an` |
|        - | 2146 | `		 * empty operand stack (pTos == pStack-1), so there is no return value to` |
|        - | 2147 | `		 * store. Guarding on pTos >= pStack (matching the sibling branch above)` |
|        - | 2148 | `		 * avoids the below-base read that crashed under glibc/ASan. */` |
|  1467862 | 2149 | `		if( sState.pLastRef ){` |
|    11653 | 2150 | `			*sState.pLastRef = SXU32_HIGH;` |
|     5824 | 2151 | `		}` |
|        - | 2152 | `	}` |
|        - | 2153 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - | 2154 | `	 * this execution. When 'return' is used inside a try block,` |
|        - | 2155 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - | 2156 | `	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering` |
|        - | 2157 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - | 2158 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - | 2159 | `	 * block can override it (the finally writes this body frame's sRet slot,` |
|        - | 2160 | `	 * materialized below).` |
|        - | 2161 | `	 */` |
|  3117969 | 2162 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|  3117969 | 2163 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2164 | `		goto Abort;` |
|        - | 2165 | `	}` |
|  3117969 | 2166 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 2167 | `		/* A drained finally threw past itself, discarding the value this OP_DONE` |
|        - | 2168 | `		 * stored into sState.pResult. If an enclosing try IN THIS function caught the new` |
|        - | 2169 | `		 * exception in place, resume at its landing pad; otherwise unwind (the` |
|        - | 2170 | `		 * caller's exception-resume pops the stored result). */` |
|        - | 2171 | `		sxi32 iResumePc;` |
|        5 | 2172 | `		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 2173 | `			pc = iResumePc;` |
|        3 | 2174 | `			break;` |
|        - | 2175 | `		}` |
|        3 | 2176 | `		goto Exception;` |
|        - | 2177 | `	}` |
|  3117965 | 2178 | `	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){` |
|        - | 2179 | `		/* A catch/finally issued a 'return' targeting THIS body. If the body is` |
|        - | 2180 | `		 * actually unwinding because an exception escaped it (terminal OP_DONE on` |
|        - | 2181 | `		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type` |
|        - | 2182 | `		 * enforcement above), that exception supersedes the return: discard it.` |
|        - | 2183 | `		 * Otherwise materialize it as this function's result. */` |
|       14 | 2184 | `		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){` |
|      ! 0 | 2185 | `			VmClearFramePending(sState.pEntryFrame);` |
|      ! 0 | 2186 | `		}else{` |
|       14 | 2187 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|        - | 2188 | `		}` |
|        5 | 2189 | `	}` |
|  3117965 | 2190 | `	goto Done;` |
|        - | 2191 | `/*` |
|        - | 2192 | ` * HALT: P1 * *` |
|        - | 2193 | ` *` |
|        - | 2194 | ` * Program execution aborted: Clean up the mess left behind` |
|        - | 2195 | ` * and abort immediately.` |
|        - | 2196 | ` */` |
|       41 | 2197 | `case PH7_OP_HALT:` |
|       86 | 2198 | `	if( pInstr->iP1 ){` |
|        - | 2199 | `#ifdef UNTRUST` |
|        - | 2200 | `		if( pTos < pStack ){` |
|        - | 2201 | `			goto Abort;` |
|        - | 2202 | `		}` |
|        - | 2203 | `#endif` |
|       86 | 2204 | `		if( sState.pLastRef ){` |
|       62 | 2205 | `			*sState.pLastRef = pTos->nIdx;` |
|       30 | 2206 | `		}` |
|       86 | 2207 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       71 | 2208 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - | 2209 | `				/* Output the exit message */` |
|      105 | 2210 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|       34 | 2211 | `					pVm->sVmConsumer.pUserData);` |
|       71 | 2212 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|       37 | 2213 | `			}` |
|       50 | 2214 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - | 2215 | `			/* Record exit status */` |
|       16 | 2216 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        7 | 2217 | `		}` |
|       86 | 2218 | `		VmPopOperand(&pTos,1);` |
|       41 | 2219 | `	}else if( sState.pLastRef ){` |
|        - | 2220 | `		/* Nothing referenced */` |
|      ! 0 | 2221 | `		*sState.pLastRef = SXU32_HIGH;` |
|      ! 0 | 2222 | `	}` |
|        - | 2223 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - | 2224 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - | 2225 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - | 2226 | `	 */` |
|       86 | 2227 | `	pVm->bHaltRequested = 1;` |
|       86 | 2228 | `	goto Abort;` |
|        - | 2229 | `/*` |
|        - | 2230 | ` * JMP: * P2 *` |
|        - | 2231 | ` *` |
|        - | 2232 | ` * Unconditional jump: The next instruction executed will be` |
|        - | 2233 | ` * the one at index P2 from the beginning of the program.` |
|        - | 2234 | ` */` |
|   508542 | 2235 | `case PH7_OP_JMP:` |
|  1017935 | 2236 | `	pc = pInstr->iP2 - 1;` |
|  1017935 | 2237 | `	break;` |
|        - | 2238 | `/*` |
|        - | 2239 | ` * JZ: P1 P2 *` |
|        - | 2240 | ` *` |
|        - | 2241 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - | 2242 | ` * entry in the stack if P1 is zero.` |
|        - | 2243 | ` */` |
|  1352706 | 2244 | `case PH7_OP_JZ:` |
|        - | 2245 | `#ifdef UNTRUST` |
|        - | 2246 | `	if( pTos < pStack ){` |
|        - | 2247 | `		goto Abort;` |
|        - | 2248 | `	}` |
|        - | 2249 | `#endif` |
|        - | 2250 | `	/* Get a boolean value */` |
|  2709577 | 2251 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     1137 | 2252 | `		PH7_MemObjToBool(pTos);` |
|      566 | 2253 | `	}` |
|  2709577 | 2254 | `	if( !pTos->x.iVal ){` |
|        - | 2255 | `		/* Take the jump */` |
|  1484568 | 2256 | `		pc = pInstr->iP2 - 1;` |
|   743490 | 2257 | `	}` |
|  2709577 | 2258 | `	if( !pInstr->iP1 ){` |
|  2317527 | 2259 | `		VmPopOperand(&pTos,1);` |
|  1160642 | 2260 | `	}` |
|  2709577 | 2261 | `	break;` |
|        - | 2262 | `/*` |
|        - | 2263 | ` * JNZ: P1 P2 *` |
|        - | 2264 | ` *` |
|        - | 2265 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - | 2266 | ` * entry in the stack if P1 is zero.` |
|        - | 2267 | ` */` |
|   146268 | 2268 | `case PH7_OP_JNZ:` |
|        - | 2269 | `#ifdef UNTRUST` |
|        - | 2270 | `	if( pTos < pStack ){` |
|        - | 2271 | `		goto Abort;` |
|        - | 2272 | `	}` |
|        - | 2273 | `#endif` |
|        - | 2274 | `	/* Get a boolean value */` |
|   292977 | 2275 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 | 2276 | `		PH7_MemObjToBool(pTos);` |
|        1 | 2277 | `	}` |
|   292977 | 2278 | `	if( pTos->x.iVal ){` |
|        - | 2279 | `		/* Take the jump */` |
|     9854 | 2280 | `		pc = pInstr->iP2 - 1;` |
|     4925 | 2281 | `	}` |
|   292977 | 2282 | `	if( !pInstr->iP1 ){` |
|        8 | 2283 | `		VmPopOperand(&pTos,1);` |
|        3 | 2284 | `	}` |
|   292977 | 2285 | `	break;` |
|        - | 2286 | `/*` |
|        - | 2287 | ` * NOOP: * * *` |
|        - | 2288 | ` *` |
|        - | 2289 | ` * Do nothing. This instruction is often useful as a jump` |
|        - | 2290 | ` * destination.` |
|        - | 2291 | ` */` |
|      ! 0 | 2292 | `case PH7_OP_NOOP:` |
|      ! 0 | 2293 | `	break;` |
|        - | 2294 | `/*` |
|        - | 2295 | ` * POP: P1 * *` |
|        - | 2296 | ` *` |
|        - | 2297 | ` * Pop P1 elements from the operand stack.` |
|        - | 2298 | ` */` |
|  1109324 | 2299 | `case PH7_OP_POP: {` |
|  2221652 | 2300 | `	sxi32 n = pInstr->iP1;` |
|  2221652 | 2301 | `	if( &pTos[-n+1] < pStack ){` |
|        - | 2302 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      416 | 2303 | `		n = (sxi32)(pTos - pStack);` |
|      206 | 2304 | `	}` |
|  2221652 | 2305 | `	VmPopOperand(&pTos,n);` |
|  2221652 | 2306 | `	break;` |
|        - | 2307 | `				 }` |
|        - | 2308 | `/*` |
|        - | 2309 | ` * DUP: * * *` |
|        - | 2310 | ` *` |
|        - | 2311 | ` * Duplicate the top of the stack.` |
|        - | 2312 | ` */` |
|      117 | 2313 | `case PH7_OP_DUP:` |
|        - | 2314 | `#ifdef UNTRUST` |
|        - | 2315 | `	if( pTos < pStack ){` |
|        - | 2316 | `		goto Abort;` |
|        - | 2317 | `	}` |
|        - | 2318 | `#endif` |
|      238 | 2319 | `	pTos++;` |
|      238 | 2320 | `	PH7_MemObjInit(pVm,pTos);` |
|      238 | 2321 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|      238 | 2322 | `	break;` |
|        - | 2323 | `/*` |
|        - | 2324 | ` * CLASS_DEFER: * * P3` |
|        - | 2325 | ` *` |
|        - | 2326 | ` * Execute a class declaration whose parent/interface/trait could not be` |
|        - | 2327 | ` * resolved when the enclosing file compiled (its autoloader had not RUN yet).` |
|        - | 2328 | ` * P3 is the VmDeferredClass record captured by the compiler. A dependency` |
|        - | 2329 | `` * that is STILL missing throws php's catchable `... not found` Error; a`` |
|        - | 2330 | ` * failed re-compile aborts (its errors were already reported). See the` |
|        - | 2331 | ` * deferral block comment in compile_class.c.` |
|        - | 2332 | ` */` |
|       15 | 2333 | `case PH7_OP_CLASS_DEFER: {` |
|       33 | 2334 | `	VmDeferredClass *pDefer = (VmDeferredClass *)pInstr->p3;` |
|       33 | 2335 | `	VmDeferredReq *pMissing = 0;` |
|       33 | 2336 | `	sxi32 rcDecl = SXRET_OK;` |
|       33 | 2337 | `	if( pDefer ){` |
|       33 | 2338 | `		rcDecl = VmExecDeferredClass(&(*pVm),pDefer,&pMissing);` |
|       15 | 2339 | `	}` |
|       33 | 2340 | `	if( pMissing ){` |
|        - | 2341 | `		static const char *azDeferKind[] = { "Class", "Interface", "Trait" };` |
|        - | 2342 | `		char zDeclMsg[520];` |
|       17 | 2343 | `		sxu32 nDeclMsg = SyBufferFormat(zDeclMsg,sizeof(zDeclMsg),"%s \"%.*s\" not found",` |
|        5 | 2344 | `			azDeferKind[pMissing->cKind < 3 ? pMissing->cKind : 0],` |
|       10 | 2345 | `			(int)pMissing->sName.nByte,pMissing->sName.zString);` |
|       12 | 2346 | `		rc = VmThrowFromVm(&(*pVm),"Error",zDeclMsg,nDeclMsg);` |
|       12 | 2347 | `		if( rc == SXERR_ABORT ){` |
|        3 | 2348 | `			goto Abort;` |
|        - | 2349 | `		}` |
|        9 | 2350 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2351 | `	}` |
|       22 | 2352 | `	if( rcDecl == SXERR_ABORT ){` |
|      ! 0 | 2353 | `		goto Abort;` |
|        - | 2354 | `	}` |
|       22 | 2355 | `	break;` |
|        - | 2356 | `				}` |
|        - | 2357 | `/*` |
|        - | 2358 | ` * CVT_INT: * * *` |
|        - | 2359 | ` *` |
|        - | 2360 | ` * Force the top of the stack to be an integer.` |
|        - | 2361 | ` */` |
|      517 | 2362 | `case PH7_OP_CVT_INT:` |
|        - | 2363 | `#ifdef UNTRUST` |
|        - | 2364 | `	if( pTos < pStack ){` |
|        - | 2365 | `		goto Abort;` |
|        - | 2366 | `	}` |
|        - | 2367 | `#endif` |
|     1039 | 2368 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|        - | 2369 | `		/* php warns from the conversion itself when no int can hold the float` |
|        - | 2370 | ``		 * (`(int)1e19`); the value it then answers is the modular wrap. */`` |
|      869 | 2371 | `		PH7_MemObjWarnIntCast(pTos);` |
|      869 | 2372 | `		PH7_MemObjToInteger(pTos);` |
|      432 | 2373 | `	}` |
|        - | 2374 | `	/* Invalidate any prior representation */` |
|     1039 | 2375 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|     1039 | 2376 | `	break;` |
|        - | 2377 | `/*` |
|        - | 2378 | ` * CVT_REAL: * * *` |
|        - | 2379 | ` *` |
|        - | 2380 | ` * Force the top of the stack to be a real.` |
|        - | 2381 | ` */` |
|       45 | 2382 | `case PH7_OP_CVT_REAL:` |
|        - | 2383 | `#ifdef UNTRUST` |
|        - | 2384 | `	if( pTos < pStack ){` |
|        - | 2385 | `		goto Abort;` |
|        - | 2386 | `	}` |
|        - | 2387 | `#endif` |
|       93 | 2388 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 | 2389 | `		PH7_MemObjToReal(pTos);` |
|       26 | 2390 | `	}` |
|        - | 2391 | `	/* Invalidate any prior representation */` |
|       93 | 2392 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       93 | 2393 | `	break;` |
|        - | 2394 | `/*` |
|        - | 2395 | ` * CVT_STR: * * *` |
|        - | 2396 | ` *` |
|        - | 2397 | ` * Force the top of the stack to be a string.` |
|        - | 2398 | ` */` |
|     2301 | 2399 | `case PH7_OP_CVT_STR:` |
|        - | 2400 | `#ifdef UNTRUST` |
|        - | 2401 | `	if( pTos < pStack ){` |
|        - | 2402 | `		goto Abort;` |
|        - | 2403 | `	}` |
|        - | 2404 | `#endif` |
|        - | 2405 | `	/* (string) cast + string interpolation "$arr": php's user-visible` |
|        - | 2406 | `	 * array->string warning site, and the not-stringable-object throw (§2). */` |
|        - | 2407 | `	{` |
|     4607 | 2408 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     4609 | 2409 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2410 | `	}` |
|     4527 | 2411 | `	break;` |
|        - | 2412 | `/*` |
|        - | 2413 | ` * CVT_BOOL: * * *` |
|        - | 2414 | ` *` |
|        - | 2415 | ` * Force the top of the stack to be a boolean.` |
|        - | 2416 | ` */` |
|       41 | 2417 | `case PH7_OP_CVT_BOOL:` |
|        - | 2418 | `#ifdef UNTRUST` |
|        - | 2419 | `	if( pTos < pStack ){` |
|        - | 2420 | `		goto Abort;` |
|        - | 2421 | `	}` |
|        - | 2422 | `#endif` |
|       86 | 2423 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       84 | 2424 | `		PH7_MemObjToBool(pTos);` |
|       40 | 2425 | `	}` |
|       86 | 2426 | `	break;` |
|        - | 2427 | `/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it` |
|        - | 2428 | ` * always raises "The (unset) cast is no longer supported" (php 8 removed` |
|        - | 2429 | ` * the cast), so a program containing it never compiles. The switch has no` |
|        - | 2430 | ` * default arm, so abort loudly rather than fall through as a silent no-op` |
|        - | 2431 | ` * if a future emitter ever produces one without the compile error. */` |
|      ! 0 | 2432 | `case PH7_OP_CVT_NULL:` |
|      ! 0 | 2433 | `	goto Abort;` |
|        - | 2434 | `/*` |
|        - | 2435 | ` * CVT_NUMC: * * *` |
|        - | 2436 | ` *` |
|        - | 2437 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - | 2438 | ` */` |
|      ! 0 | 2439 | `case PH7_OP_CVT_NUMC:` |
|        - | 2440 | `#ifdef UNTRUST` |
|        - | 2441 | `	if( pTos < pStack ){` |
|        - | 2442 | `		goto Abort;` |
|        - | 2443 | `	}` |
|        - | 2444 | `#endif` |
|        - | 2445 | `	/* Force a numeric cast */` |
|      ! 0 | 2446 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 | 2447 | `	break;` |
|        - | 2448 | `/*` |
|        - | 2449 | ` * CVT_ARRAY: * * *` |
|        - | 2450 | ` *` |
|        - | 2451 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - | 2452 | ` */` |
|       72 | 2453 | `case PH7_OP_CVT_ARRAY:` |
|        - | 2454 | `#ifdef UNTRUST` |
|        - | 2455 | `	if( pTos < pStack ){` |
|        - | 2456 | `		goto Abort;` |
|        - | 2457 | `	}` |
|        - | 2458 | `#endif` |
|        - | 2459 | `	/* Force a hashmap cast */` |
|      149 | 2460 | `	rc = PH7_MemObjToHashmap(pTos);` |
|      149 | 2461 | `	if( rc != SXRET_OK ){` |
|        - | 2462 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 | 2463 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - | 2464 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 | 2465 | `	}` |
|      149 | 2466 | `	break;` |
|        - | 2467 | `/*` |
|        - | 2468 | ` * CVT_OBJ: * * *` |
|        - | 2469 | ` *` |
|        - | 2470 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - | 2471 | ` */` |
|       26 | 2472 | `case PH7_OP_CVT_OBJ:` |
|        - | 2473 | `#ifdef UNTRUST` |
|        - | 2474 | `	if( pTos < pStack ){` |
|        - | 2475 | `		goto Abort;` |
|        - | 2476 | `	}` |
|        - | 2477 | `#endif` |
|       54 | 2478 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 2479 | `		/* Force a 'stdClass()' cast */` |
|       54 | 2480 | `		PH7_MemObjToObject(pTos);` |
|       26 | 2481 | `	}` |
|       54 | 2482 | `	break;` |
|        - | 2483 | `/*` |
|        - | 2484 | ` * ERR_CTRL * * *` |
|        - | 2485 | ` *` |
|        - | 2486 | ` * Error control operator.` |
|        - | 2487 | ` */` |
|     3902 | 2488 | `case PH7_OP_UNSET_VAR: {` |
|        - | 2489 | `	VmOpRc rcOp;` |
|     7809 | 2490 | `	sState.pTos = pTos;` |
|     7809 | 2491 | `	sState.pc = pc;` |
|     7809 | 2492 | `	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);` |
|     7809 | 2493 | `	pTos = sState.pTos;` |
|     7809 | 2494 | `	pc = sState.pc;` |
|     7809 | 2495 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 2496 | `		goto Abort;` |
|     7807 | 2497 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2498 | `		goto Exception;` |
|        - | 2499 | `	}` |
|     7807 | 2500 | `	break;` |
|        - | 2501 | `					  }` |
|    43491 | 2502 | `case PH7_OP_ERR_CTRL:` |
|        - | 2503 | `	/*` |
|        - | 2504 | `	 * Error-control operator '@'. Emitted as a PAIR around the suppressed` |
|        - | 2505 | `	 * expression: iP1=1 opens the window (before the operand is evaluated),` |
|        - | 2506 | `	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),` |
|        - | 2507 | `	 * which went unnoticed only because the engine raised so few diagnostics;` |
|        - | 2508 | `	 * every warning/notice/deprecation the parity work added leaked straight` |
|        - | 2509 | ``	 * through `@`. The window nests, and php 8 does NOT let '@' swallow`` |
|        - | 2510 | `	 * fatals or exceptions — those unwind past the closing instruction, so` |
|        - | 2511 | `	 * the depth is reset on the exception path rather than decremented here.` |
|        - | 2512 | `	 */` |
|    86987 | 2513 | `	if( pInstr->iP1 ){` |
|    43585 | 2514 | `		pVm->nErrSuppress++;` |
|    65197 | 2515 | `	}else if( pVm->nErrSuppress > 0 ){` |
|    43407 | 2516 | `		pVm->nErrSuppress--;` |
|    21701 | 2517 | `	}` |
|    86987 | 2518 | `	break;` |
|        - | 2519 | `/*` |
|        - | 2520 | ` * IS_A * * *` |
|        - | 2521 | ` *` |
|        - | 2522 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - | 2523 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - | 2524 | ` * holding a class name or an object).` |
|        - | 2525 | ` * Push TRUE on success. FALSE otherwise.` |
|        - | 2526 | ` */` |
|      430 | 2527 | `case PH7_OP_IS_A:{` |
|      865 | 2528 | `	ph7_value *pNos = &pTos[-1];` |
|      865 | 2529 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - | 2530 | `#ifdef UNTRUST` |
|        - | 2531 | `	if( pNos < pStack ){` |
|        - | 2532 | `		goto Abort;` |
|        - | 2533 | `	}` |
|        - | 2534 | `#endif` |
|      865 | 2535 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      727 | 2536 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      727 | 2537 | `		ph7_class *pClass = 0;` |
|        - | 2538 | `		/* Extract the target class */` |
|      727 | 2539 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - | 2540 | `			/* Instance already loaded */` |
|      ! 0 | 2541 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      727 | 2542 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      727 | 2543 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      727 | 2544 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - | 2545 | `			/* Handle self/static/parent keywords */` |
|      727 | 2546 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        6 | 2547 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      725 | 2548 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 | 2549 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      722 | 2550 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        6 | 2551 | `				pClass = PH7_VmResolveParentClass(&(*pVm));` |
|        4 | 2552 | `			}else{` |
|      717 | 2553 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - | 2554 | `			}` |
|      361 | 2555 | `		}` |
|      727 | 2556 | `		if( pClass ){` |
|        - | 2557 | `			/* Perform the query */` |
|      725 | 2558 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|      360 | 2559 | `		}` |
|      361 | 2560 | `	}` |
|        - | 2561 | `	/* Push result */` |
|      865 | 2562 | `	VmPopOperand(&pTos,1);` |
|      865 | 2563 | `	PH7_MemObjRelease(pTos);` |
|      865 | 2564 | `	pTos->x.iVal = iRes;` |
|      865 | 2565 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      865 | 2566 | `	break;` |
|        - | 2567 | `				 }` |
|        - | 2568 |  |
|        - | 2569 | `/*` |
|        - | 2570 | ` * LOADC P1 P2 *` |
|        - | 2571 | ` *` |
|        - | 2572 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - | 2573 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - | 2574 | ` */` |
|  4852029 | 2575 | `case PH7_OP_LOADC: {` |
|        - | 2576 | `	ph7_value *pObj;` |
|        - | 2577 | `	/* Reserve a room */` |
|  9709014 | 2578 | `	pTos++;` |
|  9709014 | 2579 | `	if( pInstr->iP1 & PH7_LOADC_NOKEY ){` |
|        - | 2580 | `		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */` |
|   738075 | 2581 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|   738075 | 2582 | `		SyBlobReset(&pTos->sBlob);` |
|   738075 | 2583 | `		pTos->iFlags \|= MEMOBJ_AUX_NOKEY;` |
|   738075 | 2584 | `		pTos->nIdx = SXU32_HIGH;` |
|   738075 | 2585 | `		break;` |
|        - | 2586 | `	}` |
|  8970944 | 2587 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  8970944 | 2588 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - | 2589 | `			SyHashEntry *pEntry;` |
|        - | 2590 | `			/* The name php looks up FIRST for an unqualified constant is decided at` |
|        - | 2591 | ``			 * COMPILE time and travels in p3: a `use const` import's FQN, or`` |
|        - | 2592 | ``			 * `current-namespace\NAME`. Absent (global scope, or an already-qualified`` |
|        - | 2593 | `			 * literal), the bare literal is the only name there is. Resolving it here` |
|        - | 2594 | `			 * rather than against the RUNTIME namespace is what makes a function keep` |
|        - | 2595 | `			 * its own namespace when it is called from another one, and what lets a` |
|        - | 2596 | `			 * namespaced constant shadow a global one of the same short name. */` |
|   134413 | 2597 | `			const char *zCand = (const char *)pInstr->p3;` |
|   134413 | 2598 | `			const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|   134413 | 2599 | `			sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|   134413 | 2600 | `			if( zCand ){` |
|       55 | 2601 | `				pEntry = SyHashGet(&pVm->hConstant,zCand,SyStrlen(zCand));` |
|       55 | 2602 | `				if( pEntry ){` |
|       47 | 2603 | `					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       47 | 2604 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       47 | 2605 | `					SyBlobReset(&pTos->sBlob);` |
|       47 | 2606 | `					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|       47 | 2607 | `					pTos->nIdx = SXU32_HIGH;` |
|       47 | 2608 | `					break;` |
|        - | 2609 | `				}` |
|        4 | 2610 | `			}` |
|        - | 2611 | `			/* The GLOBAL step — skipped when the candidate came from an import, which` |
|        - | 2612 | `			 * php resolves without any fallback. */` |
|   134371 | 2613 | `			if( (pInstr->iP1 & PH7_LOADC_NOGLOBAL) == 0 ){` |
|   134369 | 2614 | `				pEntry = SyHashGet(&pVm->hConstant,(const void *)zLit,nLit);` |
|   134369 | 2615 | `				if( pEntry ){` |
|   134197 | 2616 | `					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - | 2617 | `					/* Set a NULL default value */` |
|   134197 | 2618 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|   134197 | 2619 | `					SyBlobReset(&pTos->sBlob);` |
|        - | 2620 | `					/* Invoke the callback and deal with the expanded value */` |
|   134197 | 2621 | `					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|        - | 2622 | `					/* Mark as constant */` |
|   134197 | 2623 | `					pTos->nIdx = SXU32_HIGH;` |
|   134197 | 2624 | `					break;` |
|        - | 2625 | `				}` |
|       86 | 2626 | `			}` |
|        - | 2627 | `			{` |
|        - | 2628 | `				/*` |
|        - | 2629 | `				 * php 8 has no bare-word fallback: an unresolved constant is a catchable` |
|        - | 2630 | `				 * Error, not its own name as a string. PH7 answered "X" for an unknown` |
|        - | 2631 | `				 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —` |
|        - | 2632 | `				 * silently became a string and flowed on. php names the name it looked` |
|        - | 2633 | `				 * for FIRST, so the message reports the candidate when there was one` |
|        - | 2634 | ``				 * ("Undefined constant \"B\NOPE\"" inside `namespace B;`).`` |
|        - | 2635 | `				 *` |
|        - | 2636 | `				 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call` |
|        - | 2637 | ``				 * boundary, so neither a bare `break` nor `goto Exception` is correct`` |
|        - | 2638 | `				 * here (see the macro).` |
|        - | 2639 | `				 */` |
|        - | 2640 | `				SyBlob sMsg;` |
|      179 | 2641 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      179 | 2642 | `				if( zCand ){` |
|        8 | 2643 | `					SyBlobFormat(&sMsg,"Undefined constant \"%s\"",zCand);` |
|        5 | 2644 | `				}else{` |
|      173 | 2645 | `					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);` |
|        - | 2646 | `				}` |
|      179 | 2647 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      179 | 2648 | `				SyBlobReset(&pTos->sBlob);` |
|      179 | 2649 | `				pTos->nIdx = SXU32_HIGH;` |
|      266 | 2650 | `				rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|       87 | 2651 | `					SyBlobLength(&sMsg));` |
|      179 | 2652 | `				SyBlobRelease(&sMsg);` |
|      179 | 2653 | `				if( rc == SXERR_ABORT ){` |
|       49 | 2654 | `					goto Abort;` |
|        - | 2655 | `				}` |
|      157 | 2656 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2657 | `			}` |
|        - | 2658 | `		}` |
|  8836536 | 2659 | `		PH7_MemObjLoad(pObj,pTos);` |
|  4420749 | 2660 | `	}else{` |
|        - | 2661 | `		/* Set a NULL value */` |
|      ! 0 | 2662 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 2663 | `	}` |
|        - | 2664 | `	/* Mark as constant */` |
|  8836536 | 2665 | `	pTos->nIdx = SXU32_HIGH;` |
|  8836536 | 2666 | `	break;` |
|        - | 2667 | `				  }` |
|        - | 2668 | `/*` |
|        - | 2669 | ` * LOAD: P1 * P3` |
|        - | 2670 | ` *` |
|        - | 2671 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - | 2672 | ` * from the P3 operand.` |
|        - | 2673 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - | 2674 | ` * the variable if non existent and push the NULL constant instead.` |
|        - | 2675 | ` */` |
|  3834168 | 2676 | `case PH7_OP_LOAD:{` |
|        - | 2677 | `	ph7_value *pObj;` |
|        - | 2678 | `	SyString sName;` |
|  7679829 | 2679 | `	if( pInstr->p3 == 0 ){` |
|        - | 2680 | `		/* Take the variable name from the top of the stack */` |
|        - | 2681 | `#ifdef UNTRUST` |
|        - | 2682 | `		if( pTos < pStack ){` |
|        - | 2683 | `			goto Abort;` |
|        - | 2684 | `		}` |
|        - | 2685 | `#endif` |
|        - | 2686 | `		/* Force a string cast — variable-variable name $$arr (user-visible, §2) */` |
|        - | 2687 | `		{` |
|       40 | 2688 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|  3834207 | 2689 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2690 | `		}` |
|       38 | 2691 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       21 | 2692 | `	}else{` |
|  7679793 | 2693 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 2694 | `		/* Reserve a room for the target object */` |
|  7679793 | 2695 | `		pTos++;` |
|        - | 2696 | `	}` |
|  7679827 | 2697 | `	if( pInstr->iP2 == 2 ){` |
|        - | 2698 | ``		/* Read-modify-write target (`$x++`, `$x .= 'a'`): php reads the variable`` |
|        - | 2699 | `		 * before writing, so it warns when it does not exist and THEN seeds it.` |
|        - | 2700 | `		 * Peek first (no create) purely to raise that warning; the load below` |
|        - | 2701 | ``		 * still creates the slot the operator needs. A plain `=` never gets here`` |
|        - | 2702 | `		 * — it writes without reading, and stays silent, as php does. */` |
|   744998 | 2703 | `		if( VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE) == 0 ){` |
|        7 | 2704 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        3 | 2705 | `		}` |
|   372926 | 2706 | `	}` |
|        - | 2707 | `	/* Extract the requested memory object */` |
|  7679827 | 2708 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  7679827 | 2709 | `	if( pObj == 0 ){` |
|      187 | 2710 | `		if( pInstr->iP1 ){` |
|        - | 2711 | `			/* Reading a variable that does not exist. php raises E_WARNING` |
|        - | 2712 | `			 * "Undefined variable $x" and evaluates it as NULL; PHL used to` |
|        - | 2713 | `			 * yield NULL silently, which hid typo'd names. iP2 marks the reads` |
|        - | 2714 | `			 * that must stay quiet (isset/empty — see PH7_CompileVariable);` |
|        - | 2715 | `			 * vivifying contexts never get here because they pass iP1 = 0. */` |
|      187 | 2716 | `			if( pInstr->iP2 == 0 ){` |
|       45 | 2717 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|       20 | 2718 | `			}` |
|        - | 2719 | `			/* Variable not found,load NULL */` |
|      187 | 2720 | `			if( !pInstr->p3 ){` |
|       10 | 2721 | `				PH7_MemObjRelease(pTos);` |
|        6 | 2722 | `			}else{` |
|      179 | 2723 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 2724 | `			}` |
|      187 | 2725 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|      187 | 2726 | `			if( pInstr->iP2 == 3 ){` |
|        - | 2727 | `				/* D1 deferred call argument: the variable does not exist, but we do not` |
|        - | 2728 | `				 * know yet whether the callee wants it by-ref (materialize + bind) or` |
|        - | 2729 | `				 * by-value (warn + pass NULL). Tag the slot and stash the variable name` |
|        - | 2730 | `				 * (a VM-lifetime bytecode string, nothing to free) so OP_CALL's` |
|        - | 2731 | `				 * PH7_VmResolveDeferredArgs can decide once the callee is resolved. */` |
|       64 | 2732 | `				pTos->iFlags \|= MEMOBJ_AUX_DEFERRED;` |
|       64 | 2733 | `				pTos->x.pOther = pInstr->p3;` |
|       31 | 2734 | `			}` |
|      187 | 2735 | `			break;` |
|      ! 0 | 2736 | `		}else{` |
|        - | 2737 | `			/* Fatal error */` |
|      ! 0 | 2738 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 2739 | `			goto Abort;` |
|        - | 2740 | `		}` |
|        - | 2741 | `	}` |
|        - | 2742 | `	/* Load variable contents */` |
|  7679645 | 2743 | `	PH7_MemObjLoad(pObj,pTos);` |
|  7679645 | 2744 | `	pTos->nIdx = pObj->nIdx;` |
|  7679645 | 2745 | `	break;` |
|        - | 2746 | `				   }` |
|        - | 2747 | `/*` |
|        - | 2748 | ` * LOAD_MAP P1 * *` |
|        - | 2749 | ` *` |
|        - | 2750 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - | 2751 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - | 2752 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - | 2753 | ` */` |
|    52239 | 2754 | `case PH7_OP_LOAD_MAP: {` |
|        - | 2755 | `	VmOpRc rcOp;` |
|   104481 | 2756 | `	sState.pTos = pTos;` |
|   104481 | 2757 | `	sState.pc = pc;` |
|   104481 | 2758 | `	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);` |
|   104481 | 2759 | `	pTos = sState.pTos;` |
|   104481 | 2760 | `	pc = sState.pc;` |
|   104481 | 2761 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2762 | `		goto Abort;` |
|   104481 | 2763 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       15 | 2764 | `		goto Exception;` |
|        - | 2765 | `	}` |
|   104467 | 2766 | `	break;` |
|        - | 2767 | `					  }` |
|        - | 2768 | `/*` |
|        - | 2769 | ` * LOAD_LIST: P1 * *` |
|        - | 2770 | ` *` |
|        - | 2771 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - | 2772 | ` * This is the VM implementation of the list() PHP construct.` |
|        - | 2773 | ` * Caveats:` |
|        - | 2774 | ` *  This implementation support only a single nesting level.` |
|        - | 2775 | ` */` |
|      728 | 2776 | `case PH7_OP_LOAD_LIST: {` |
|        - | 2777 | `	VmOpRc rcOp;` |
|     1461 | 2778 | `	sState.pTos = pTos;` |
|     1461 | 2779 | `	sState.pc = pc;` |
|     1461 | 2780 | `	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);` |
|     1461 | 2781 | `	pTos = sState.pTos;` |
|     1461 | 2782 | `	pc = sState.pc;` |
|     1461 | 2783 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2784 | `		goto Abort;` |
|     1461 | 2785 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 2786 | `		goto Exception;` |
|        - | 2787 | `	}` |
|     1459 | 2788 | `	break;` |
|        - | 2789 | `					  }` |
|        - | 2790 | `/*` |
|        - | 2791 | ` * LOAD_IDX: P1 P2 *` |
|        - | 2792 | ` *` |
|        - | 2793 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - | 2794 | ` * from the stack.` |
|        - | 2795 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - | 2796 | ` * instead.` |
|        - | 2797 | ` */` |
|   514998 | 2798 | `case PH7_OP_LOAD_IDX: {` |
|        - | 2799 | `	VmOpRc rcOp;` |
|  1032106 | 2800 | `	sState.pTos = pTos;` |
|  1032106 | 2801 | `	sState.pc = pc;` |
|  1032106 | 2802 | `	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);` |
|  1032106 | 2803 | `	pTos = sState.pTos;` |
|  1032106 | 2804 | `	pc = sState.pc;` |
|  1032106 | 2805 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2806 | `		goto Abort;` |
|  1032106 | 2807 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       76 | 2808 | `		goto Exception;` |
|        - | 2809 | `	}` |
|  1032034 | 2810 | `	break;` |
|        - | 2811 | `					  }` |
|        - | 2812 | `/*` |
|        - | 2813 | ` * LOAD_CLOSURE * * P3` |
|        - | 2814 | ` *` |
|        - | 2815 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - | 2816 | ` * name in the stack.` |
|        - | 2817 | ` */` |
|     5027 | 2818 | `case PH7_OP_LOAD_CLOSURE: {` |
|        - | 2819 | `	VmOpRc rcOp;` |
|    10059 | 2820 | `	sState.pTos = pTos;` |
|    10059 | 2821 | `	sState.pc = pc;` |
|    10059 | 2822 | `	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);` |
|    10059 | 2823 | `	pTos = sState.pTos;` |
|    10059 | 2824 | `	pc = sState.pc;` |
|    10059 | 2825 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2826 | `		goto Abort;` |
|    10059 | 2827 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2828 | `		goto Exception;` |
|        - | 2829 | `	}` |
|    10059 | 2830 | `	break;` |
|        - | 2831 | `					  }` |
|        - | 2832 | `/*` |
|        - | 2833 | ` * LOAD_FCC P1 * *` |
|        - | 2834 | ` *` |
|        - | 2835 | ` * First-class callable: wrap the callee in a Closure object instead of calling it.` |
|        - | 2836 | ` *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS` |
|        - | 2837 | ` *           (from the callee's OP_LOADC). Replace it in place with a Closure whose` |
|        - | 2838 | ` *           $__fn is that name; the existing string-callable dispatch resolves it.` |
|        - | 2839 | ` *           (OOM degrades to leaving the name string on the stack — still callable.)` |
|        - | 2840 | ` *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name` |
|        - | 2841 | ` *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds` |
|        - | 2842 | ` *           $this (scope = its class); a class-name-string target is a static callable` |
|        - | 2843 | ` *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —` |
|        - | 2844 | ` *           the popped target leaves no name string to keep.)` |
|        - | 2845 | ` */` |
|      160 | 2846 | `case PH7_OP_LOAD_FCC:{` |
|      325 | 2847 | `	if( pInstr->iP1 == 1 ){` |
|        - | 2848 | ``		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)`` |
|        - | 2849 | `		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —` |
|        - | 2850 | `		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,` |
|        - | 2851 | ``		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a`` |
|        - | 2852 | `		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is` |
|        - | 2853 | `		 * (graceful degradation), and so is the original on OOM — still whatever it was. */` |
|        - | 2854 | `		ph7_class_instance *pCloObj;` |
|        - | 2855 | `		sxi32 nFccBrc;` |
|        - | 2856 | `		const void *pFccRes;` |
|      143 | 2857 | `		if( VmValueIsClosure(pVm, pTos) ){` |
|       11 | 2858 | `			break;` |
|        - | 2859 | `		}` |
|        - | 2860 | `		/* php's global fallback for an UNQUALIFIED function name written inside a` |
|        - | 2861 | `		 * namespace: the current namespace first, the global one after. The compiler` |
|        - | 2862 | `		 * qualified this name and marks the instruction (iP2==1) when it did, so the` |
|        - | 2863 | `		 * fallback happens here — the OP_CALL path does the same thing from its arg map,` |
|        - | 2864 | ``		 * which an FCC has none of. `strlen(...)` in a namespaced file was`` |
|        - | 2865 | ``		 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|      130 | 2866 | `		if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING)` |
|       13 | 2867 | `			&& !PH7_VmIsCallable(pVm,pTos,TRUE) ){` |
|        7 | 2868 | `			const char *zFccName = (const char *)SyBlobData(&pTos->sBlob);` |
|        7 | 2869 | `			sxu32 nFccName = SyBlobLength(&pTos->sBlob);` |
|        7 | 2870 | `			const char *zFccShort = zFccName;` |
|        - | 2871 | `			sxu32 iFccPos;` |
|      165 | 2872 | `			for( iFccPos = 0 ; iFccPos < nFccName ; ++iFccPos ){` |
|      159 | 2873 | `				if( zFccName[iFccPos] == '\\' ){` |
|        7 | 2874 | `					zFccShort = &zFccName[iFccPos + 1];` |
|        3 | 2875 | `				}` |
|       80 | 2876 | `			}` |
|        7 | 2877 | `			if( zFccShort != zFccName ){` |
|        - | 2878 | `				ph7_value sFccShort;` |
|        7 | 2879 | `				PH7_MemObjInit(pVm,&sFccShort);` |
|       10 | 2880 | `				PH7_MemObjStringAppend(&sFccShort,zFccShort,` |
|        6 | 2881 | `					(sxu32)(nFccName - (sxu32)(zFccShort - zFccName)));` |
|        7 | 2882 | `				if( PH7_VmIsCallable(pVm,&sFccShort,TRUE) ){` |
|        5 | 2883 | `					PH7_MemObjStore(&sFccShort,pTos);` |
|        2 | 2884 | `				}` |
|        7 | 2885 | `				PH7_MemObjRelease(&sFccShort);` |
|        3 | 2886 | `			}` |
|        3 | 2887 | `		}` |
|        - | 2888 | `		/* The array shape's class lookup can run an autoloader that throws; php propagates` |
|        - | 2889 | `		 * THAT exception and never reports the callable bad, exactly as at the OP_CALL sites. */` |
|      133 | 2890 | `		nFccBrc = pVm->nBoundaryRc;` |
|      133 | 2891 | `		pFccRes = (const void *)pVm->pResumeFrame;` |
|      133 | 2892 | `		pCloObj = VmFccWrapValue(pVm, pTos);` |
|      133 | 2893 | `		if( pCloObj ){` |
|      103 | 2894 | `			PH7_MemObjRelease(pTos);` |
|      103 | 2895 | `			pCloObj->iRef++;` |
|      103 | 2896 | `			pTos->x.pOther = pCloObj;` |
|      103 | 2897 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       53 | 2898 | `		}else{` |
|        - | 2899 | `			/* php refuses a non-callable HERE, with the direct dispatch's own wording — the` |
|        - | 2900 | ``			 * `(...)` does not make a bad callable acceptable, it just defers the call. */`` |
|        - | 2901 | `			char zFccMsg[192];` |
|       31 | 2902 | `			const char *zFccBad = VmFccValueError(&(*pVm),pTos,zFccMsg,sizeof(zFccMsg));` |
|       31 | 2903 | `			int bFccRaised = PH7_VmClassLookupRaised(&(*pVm),nFccBrc,pFccRes);` |
|       31 | 2904 | `			if( zFccBad \|\| bFccRaised ){` |
|        - | 2905 | `				sxi32 rcFcc;` |
|       31 | 2906 | `				PH7_MemObjRelease(pTos);` |
|       31 | 2907 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       31 | 2908 | `				pTos->nIdx = SXU32_HIGH;` |
|       31 | 2909 | `				if( bFccRaised ){` |
|      ! 0 | 2910 | `					rcFcc = pVm->nBoundaryRc;` |
|      ! 0 | 2911 | `					pVm->nBoundaryRc = 0;` |
|      ! 0 | 2912 | `					if( rcFcc == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 2913 | `					rc = PH7_EXCEPTION;` |
|       15 | 2914 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2915 | `				}` |
|       31 | 2916 | `				rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccBad,(sxu32)SyStrlen(zFccBad));` |
|       31 | 2917 | `				if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       31 | 2918 | `				rc = rcFcc;` |
|       61 | 2919 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2920 | `			}` |
|        - | 2921 | `		}` |
|       53 | 2922 | `	}else{` |
|        - | 2923 | `		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]` |
|        - | 2924 | `		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its` |
|        - | 2925 | `		 * class); a class-name string target is a static callable (scope = that class). */` |
|      185 | 2926 | `		ph7_value *pTarget = &pTos[-1];` |
|        - | 2927 | `		SyString sName;` |
|        - | 2928 | `		ph7_class_instance *pCloObj;` |
|      185 | 2929 | `		ph7_class *pFccCls = 0;` |
|      185 | 2930 | `		ph7_class_instance *pFccRecv = 0;` |
|      185 | 2931 | `		const char *zFccErr = 0;` |
|        - | 2932 | `		char zFccMsg[192];` |
|      185 | 2933 | `		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));` |
|      185 | 2934 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      109 | 2935 | `			pFccRecv = (ph7_class_instance *)pTarget->x.pOther;` |
|      109 | 2936 | `			pFccCls = pFccRecv->pClass;` |
|      109 | 2937 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pFccCls) ){` |
|        - | 2938 | ``				/* `$inc->m(...)` resolves the method at CREATION, so php's`` |
|        - | 2939 | `				 * incomplete-object call Error is raised here, not at a later` |
|        - | 2940 | `				 * invocation. */` |
|        - | 2941 | `				SyBlob sIncErr;` |
|        - | 2942 | `				sxi32 rcInc;` |
|        3 | 2943 | `				SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|        3 | 2944 | `				PH7_VmIncompleteMsg(&(*pVm),pFccRecv,"call a method",&sIncErr);` |
|        3 | 2945 | `				VmPopOperand(&pTos,1);       /* the method name */` |
|        3 | 2946 | `				PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|        3 | 2947 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 2948 | `				pTos->nIdx = SXU32_HIGH;` |
|        4 | 2949 | `				rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|        1 | 2950 | `					SyBlobLength(&sIncErr));` |
|        3 | 2951 | `				SyBlobRelease(&sIncErr);` |
|        3 | 2952 | `				if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|        3 | 2953 | `				rc = rcInc;` |
|        3 | 2954 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        5 | 2955 | `			}` |
|      131 | 2956 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|        - | 2957 | ``			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class`` |
|        - | 2958 | `			 * now, so the closure binds the concrete scope (matching PHP). */` |
|       80 | 2959 | `			pFccCls = VmFccResolveScope(pVm, pTarget);` |
|       38 | 2960 | `		}` |
|      183 | 2961 | `		if( pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING) ){` |
|        - | 2962 | `			/* php resolves the member HERE, through the same lookup the call would use:` |
|        - | 2963 | `			 * every refusal a call would raise is raised at CREATION, and a non-static` |
|        - | 2964 | `			 * method named through a class binds the calling frame's own $this. */` |
|      183 | 2965 | `			ph7_class_instance *pRecvOut = 0;` |
|      272 | 2966 | `			zFccErr = VmFccMemberError(&(*pVm),pFccCls,` |
|      178 | 2967 | `				(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|       89 | 2968 | `				SyStringData(&sName),SyStringLength(&sName),` |
|      178 | 2969 | `				(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|       89 | 2970 | `				&pRecvOut,zFccMsg,sizeof(zFccMsg));` |
|      183 | 2971 | `			if( pRecvOut ){` |
|       13 | 2972 | ``				pFccRecv = pRecvOut; /* the receiver php binds into a `C::m(...)` callable */`` |
|        6 | 2973 | `			}` |
|       89 | 2974 | `		}` |
|      183 | 2975 | `		if( zFccErr ){` |
|        - | 2976 | `			sxi32 rcFcc;` |
|       24 | 2977 | `			VmPopOperand(&pTos,1);       /* the method name */` |
|       24 | 2978 | `			PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|       24 | 2979 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       24 | 2980 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 | 2981 | `			rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccErr,(sxu32)SyStrlen(zFccErr));` |
|       24 | 2982 | `			if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       24 | 2983 | `			rc = rcFcc;` |
|       30 | 2984 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2985 | `		}` |
|      161 | 2986 | `		if( pFccCls == 0 ){` |
|      ! 0 | 2987 | `			pCloObj = 0;` |
|      161 | 2988 | `		}else if( pFccRecv ){` |
|      109 | 2989 | `			pCloObj = VmCreateClosure(pVm, &sName, pFccRecv, &pFccRecv->pClass->sName);` |
|       57 | 2990 | `		}else{` |
|       56 | 2991 | `			pCloObj = VmCreateClosure(pVm, &sName, 0, &pFccCls->sName);` |
|        - | 2992 | `		}` |
|      161 | 2993 | `		if( pCloObj ){` |
|        - | 2994 | ``			/* `$o->m(...)` / `C::m(...)` names a METHOD, whatever the class turns out to`` |
|        - | 2995 | `			 * declare: the unwrap must not go looking for a FUNCTION of that name, and a` |
|        - | 2996 | `			 * name the class answers only through __call is still a method call. */` |
|      161 | 2997 | `			pCloObj->iFlags \|= VM_INSTANCE_FCC_METHOD;` |
|        - | 2998 | `			/* The screen above already ran, HERE, where php runs it — so record that this` |
|        - | 2999 | `			 * closure's callee is settled and the invocation must not re-decide it. A name` |
|        - | 3000 | `			 * that resolved to the catch-all instead keeps routing there. */` |
|      161 | 3001 | `			if( PH7_VmFccMethodIsDirect(&(*pVm),pFccCls,SyStringData(&sName),SyStringLength(&sName)) ){` |
|      147 | 3002 | `				pCloObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       71 | 3003 | `			}` |
|       78 | 3004 | `		}` |
|        - | 3005 | `		/* Pop the method name and the target, push the Closure. */` |
|      161 | 3006 | `		PH7_MemObjRelease(pTos);` |
|      161 | 3007 | `		pTos--;` |
|      161 | 3008 | `		PH7_MemObjRelease(pTos);` |
|      161 | 3009 | `		if( pCloObj ){` |
|      161 | 3010 | `			pCloObj->iRef++;` |
|      161 | 3011 | `			pTos->x.pOther = pCloObj;` |
|      161 | 3012 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       83 | 3013 | `		}else{` |
|      ! 0 | 3014 | `			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */` |
|        - | 3015 | `		}` |
|        - | 3016 | `	}` |
|      261 | 3017 | `	break;` |
|        - | 3018 | `					 }` |
|        - | 3019 | `/*` |
|        - | 3020 | ` * STORE * P2 P3` |
|        - | 3021 | ` *` |
|        - | 3022 | ` * Perform a store (Assignment) operation.` |
|        - | 3023 | ` */` |
|   509580 | 3024 | `case PH7_OP_STORE: {` |
|        - | 3025 | `	ph7_value *pObj;` |
|        - | 3026 | `	SyString sName;` |
|        - | 3027 | `#ifdef UNTRUST` |
|        - | 3028 | `	if( pTos < pStack ){` |
|        - | 3029 | `		goto Abort;` |
|        - | 3030 | `	}` |
|        - | 3031 | `#endif` |
|  1021321 | 3032 | `	if( pInstr->iP2 ){` |
|        - | 3033 | `		sxu32 nIdx;` |
|        - | 3034 | `		sxi32 rcT;` |
|        - | 3035 | `		/* Member store operation */` |
|   103227 | 3036 | `		nIdx = pTos->nIdx;` |
|   103227 | 3037 | `		VmPopOperand(&pTos,1);` |
|   103227 | 3038 | `		if( pVm->pMagicSetThis ){` |
|        - | 3039 | `			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain` |
|        - | 3040 | `			 * store to a missing/inaccessible property on a class declaring __set.` |
|        - | 3041 | `			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on` |
|        - | 3042 | `			 * the stack as the assignment expression's result — php's semantics` |
|        - | 3043 | `			 * (no property is created; a throw rides the boundary rail). */` |
|      377 | 3044 | `			ph7_class_instance *pSetThis = pVm->pMagicSetThis;` |
|        - | 3045 | `			SyString sSetName;` |
|      377 | 3046 | `			pVm->pMagicSetThis = 0;` |
|      377 | 3047 | `			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));` |
|      377 | 3048 | `			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);` |
|      377 | 3049 | `			PH7_ClassInstanceUnref(pSetThis);` |
|      377 | 3050 | `			SyBlobReset(&pVm->sMagicSetName);` |
|      377 | 3051 | `			break;` |
|        - | 3052 | `		}` |
|   102853 | 3053 | `		if( pVm->pHookSetThis ){` |
|        - | 3054 | `			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a` |
|        - | 3055 | `			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with` |
|        - | 3056 | `			 * the rvalue (which stays on the stack as the assignment expression's` |
|        - | 3057 | ``			 * result); a `set => expr` hook's return value is stored into the`` |
|        - | 3058 | `			 * BACKING slot with the ordinary typed enforcement. A property with` |
|        - | 3059 | `			 * only a get hook is php's catchable "is read-only" Error. */` |
|       56 | 3060 | `			ph7_class_instance *pHThis = pVm->pHookSetThis;` |
|       56 | 3061 | `			ph7_class_attr *pHAttr = pVm->pHookSetAttr;` |
|       56 | 3062 | `			sxu32 nBackIdx = pVm->nHookSetIdx;` |
|        - | 3063 | `			sxi32 rcHs;` |
|       56 | 3064 | `			pVm->pHookSetThis = 0;` |
|       56 | 3065 | `			pVm->pHookSetAttr = 0;` |
|       56 | 3066 | `			pVm->nHookSetIdx = SXU32_HIGH;` |
|       56 | 3067 | `			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);` |
|       56 | 3068 | `			PH7_ClassInstanceUnref(pHThis);` |
|       56 | 3069 | `			if( rcHs == PH7_ABORT ){` |
|      ! 0 | 3070 | `				goto Abort;` |
|        - | 3071 | `			}` |
|       56 | 3072 | `			break;` |
|        - | 3073 | `		}` |
|   102801 | 3074 | `		if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 3075 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3076 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|      ! 0 | 3077 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 3078 | `		}else{` |
|        - | 3079 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - | 3080 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|   102801 | 3081 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);` |
|   102801 | 3082 | `			if( rcT == PH7_ABORT ){` |
|       13 | 3083 | `				goto Abort;` |
|        - | 3084 | `			}` |
|   102791 | 3085 | `			if( rcT == PH7_EXCEPTION ){` |
|        - | 3086 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - | 3087 | `				 * control to the nearest catch block if any (draining any` |
|        - | 3088 | `				 * abandoned outer-expression operands to the try's base),` |
|        - | 3089 | `				 * otherwise propagate out of the VM loop. */` |
|   100175 | 3090 | `				VmPopOperand(&pTos,1);` |
|        - | 3091 | `				{` |
|        - | 3092 | `					sxi32 iRp;` |
|   100175 | 3093 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|   400101 | 3094 | `						PH7_RESUME_DRAIN()` |
|   100099 | 3095 | `						pc = iRp;` |
|   100099 | 3096 | `						break;` |
|        - | 3097 | `					}` |
|        - | 3098 | `				}` |
|       81 | 3099 | `				goto Exception;` |
|        - | 3100 | `			}` |
|        - | 3101 | `			/* Point to the desired memory object */` |
|     2621 | 3102 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2621 | 3103 | `			if( pObj ){` |
|        - | 3104 | `				/* Perform the store operation */` |
|     2621 | 3105 | `				PH7_MemObjStore(pTos,pObj);` |
|     1308 | 3106 | `			}` |
|        - | 3107 | `		}` |
|     2621 | 3108 | `		break;` |
|   918099 | 3109 | `	}else if( pInstr->p3 == 0 ){` |
|        - | 3110 | `		/* Take the variable name from the next on the stack (user-visible: a` |
|        - | 3111 | `		 * variable-variable NAME $$arr warns on an array, §2) */` |
|        - | 3112 | `		{` |
|       27 | 3113 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       27 | 3114 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3115 | `		}` |
|       24 | 3116 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       24 | 3117 | `		pTos--;` |
|        - | 3118 | `#ifdef UNTRUST` |
|        - | 3119 | `		if( pTos < pStack  ){` |
|        - | 3120 | `			goto Abort;` |
|        - | 3121 | `		}` |
|        - | 3122 | `#endif` |
|       13 | 3123 | `	}else{` |
|   918075 | 3124 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 3125 | `	}` |
|   918092 | 3126 | `	if( sName.nByte == sizeof("GLOBALS")-1` |
|   461776 | 3127 | `	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){` |
|        6 | 3128 | `		if( pInstr->p3 ){` |
|        - | 3129 | `			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time` |
|        - | 3130 | `			 * fatal there; raised at the store site here with the same` |
|        - | 3131 | `			 * message and the same non-catchable outcome). Element writes` |
|        - | 3132 | `			 * are unaffected. */` |
|        3 | 3133 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3134 | `				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 3135 | `			pVm->iExitStatus = 255;` |
|        3 | 3136 | `			pVm->bHaltRequested = 1;` |
|        3 | 3137 | `			goto Abort;` |
|        - | 3138 | `		}` |
|        - | 3139 | `		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's` |
|        - | 3140 | `		 * compile-time case: php quietly creates an ordinary symbol-table` |
|        - | 3141 | `		 * entry named GLOBALS, leaving the auto-global view intact. */` |
|        3 | 3142 | `		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);` |
|        3 | 3143 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 3144 | `		break;` |
|        - | 3145 | `	}` |
|        - | 3146 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   918093 | 3147 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   918093 | 3148 | `	if( pObj == 0 ){` |
|      ! 0 | 3149 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 3150 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 3151 | `		goto Abort;` |
|        - | 3152 | `	}` |
|   918093 | 3153 | `	if( !pInstr->p3 ){` |
|       22 | 3154 | `		PH7_MemObjRelease(&pTos[1]);` |
|       10 | 3155 | `	}` |
|        - | 3156 | `	/* Perform the store operation */` |
|   918093 | 3157 | `	PH7_MemObjStore(pTos,pObj);` |
|   918093 | 3158 | `	break;` |
|        - | 3159 | `				   }` |
|        - | 3160 | `/*` |
|        - | 3161 | ` * STORE_IDX:   P1 * P3` |
|        - | 3162 | ` * STORE_IDX_R: P1 * P3` |
|        - | 3163 | ` *` |
|        - | 3164 | ` * Perfrom a store operation an a hashmap entry.` |
|        - | 3165 | ` */` |
|   195441 | 3166 | `case PH7_OP_STORE_IDX:` |
|        - | 3167 | `case PH7_OP_STORE_IDX_REF: {` |
|        - | 3168 | `	VmOpRc rcOp;` |
|   390864 | 3169 | `	sState.pTos = pTos;` |
|   390864 | 3170 | `	sState.pc = pc;` |
|   390864 | 3171 | `	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);` |
|   390864 | 3172 | `	pTos = sState.pTos;` |
|   390864 | 3173 | `	pc = sState.pc;` |
|   390864 | 3174 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3175 | `		goto Abort;` |
|   390862 | 3176 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       43 | 3177 | `		goto Exception;` |
|        - | 3178 | `	}` |
|   390824 | 3179 | `	break;` |
|        - | 3180 | `					  }` |
|        - | 3181 | `/*` |
|        - | 3182 | ` * INCR: P1 * *` |
|        - | 3183 | ` *` |
|        - | 3184 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - | 3185 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - | 3186 | ` * the stack and increment after that.` |
|        - | 3187 | ` */` |
|   348288 | 3188 | `case PH7_OP_INCR:` |
|        - | 3189 | `#ifdef UNTRUST` |
|        - | 3190 | `	if( pTos < pStack ){` |
|        - | 3191 | `		goto Abort;` |
|        - | 3192 | `	}` |
|        - | 3193 | `#endif` |
|        - | 3194 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3195 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3196 | `	 * — which otherwise skips object/array/resource operands. */` |
|   697446 | 3197 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3198 | `	/* php refuses to increment a string OFFSET, whatever byte it holds. */` |
|   697436 | 3199 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3200 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3201 | `	 * php's TypeError, raised BEFORE any set dispatch (the type guard below` |
|        - | 3202 | `	 * would skip the mutation and the tail write-back would otherwise call` |
|        - | 3203 | `	 * the set hook with the unchanged value). */` |
|   697417 | 3204 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|   349156 | 3205 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        5 | 3206 | `		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        5 | 3207 | `		if( VM_HOOK_PEND_IS_RMW(pTopInc->iKind) && pTopInc->nScratchIdx == pTos->nIdx ){` |
|        - | 3208 | `			SyBlob sErrMsg;` |
|        5 | 3209 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 | 3210 | `			VmIncDecTypeErrorMsg(pTos,TRUE,&sErrMsg);` |
|        5 | 3211 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        5 | 3212 | `			VmHookRmwDropTop(&(*pVm));` |
|        5 | 3213 | `			pTos->nIdx = SXU32_HIGH;` |
|        5 | 3214 | `			break;` |
|        - | 3215 | `		}` |
|      ! 0 | 3216 | `	}` |
|   697418 | 3217 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3218 | `		/* php's ++ operand contract: an array, object or resource is a catchable` |
|        - | 3219 | `		 * TypeError, not the silent no-op this used to be. Settle the operand` |
|        - | 3220 | `		 * (the op's result slot) BEFORE throwing, like the arithmetic sites. */` |
|        - | 3221 | `		SyBlob sIncMsg;` |
|        - | 3222 | `		sxi32 rcInc;` |
|       23 | 3223 | `		SyBlobInit(&sIncMsg,&pVm->sAllocator);` |
|       23 | 3224 | `		VmIncDecTypeErrorMsg(pTos,TRUE,&sIncMsg);` |
|       23 | 3225 | `		PH7_MemObjRelease(pTos);` |
|       23 | 3226 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       23 | 3227 | `		pTos->nIdx = SXU32_HIGH;` |
|       34 | 3228 | `		rcInc = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sIncMsg),` |
|       11 | 3229 | `			SyBlobLength(&sIncMsg));` |
|       23 | 3230 | `		SyBlobRelease(&sIncMsg);` |
|       23 | 3231 | `		if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|       23 | 3232 | `		rc = rcInc;` |
|       25 | 3233 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3234 | `	}` |
|        - | 3235 | ``	/* php 8.3+: `++` on a BOOL is a no-op it warns about (E_WARNING, errno 2 —`` |
|        - | 3236 | ``	 * same shape as the null `--` below), where PH7 coerced true to int 2. */`` |
|   697396 | 3237 | `	if( pTos->iFlags & MEMOBJ_BOOL ){` |
|        7 | 3238 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3239 | `			"Increment on type bool has no effect, this will change in the next major version of PHP");` |
|        3 | 3240 | `	}` |
|   697396 | 3241 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_BOOL)) == 0 ){` |
|   697390 | 3242 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3243 | `			ph7_value *pObj;` |
|   697382 | 3244 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   697382 | 3245 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3246 | `					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric` |
|        - | 3247 | `					 * string (it points at str_increment() instead); PHL rejects it. */` |
|        - | 3248 | `					SyBlob sErrMsg;` |
|        3 | 3249 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3250 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3251 | `						"Increment on a non-numeric string is not supported, use str_increment() instead",` |
|        - | 3252 | `						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);` |
|        3 | 3253 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3254 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3255 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3256 | `					break;` |
|      ! 0 | 3257 | `				}else{` |
|        - | 3258 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - | 3259 | `					 * original value: pTos may alias pObj's blob via` |
|        - | 3260 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - | 3261 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - | 3262 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - | 3263 | `					 * so its old-value view survives the coercion. */` |
|   697380 | 3264 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       12 | 3265 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        5 | 3266 | `					}` |
|        - | 3267 | `					/* Force a numeric cast on the variable */` |
|   697380 | 3268 | `					PH7_MemObjToNumeric(pObj);` |
|   697380 | 3269 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        7 | 3270 | `						pObj->rVal++;` |
|        - | 3271 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3272 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3273 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3274 | `						 * integer-valued real. */` |
|        7 | 3275 | `						PH7_MemObjTryInteger(pObj);` |
|        4 | 3276 | `					}else{` |
|        - | 3277 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3278 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3279 | `						sxi64 r;` |
|   697374 | 3280 | `						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3281 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        7 | 3282 | `							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;` |
|        7 | 3283 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3284 | `#else` |
|        - | 3285 | `							pObj->x.iVal = r;` |
|        - | 3286 | `#endif` |
|        4 | 3287 | `						}else{` |
|   697368 | 3288 | `							pObj->x.iVal = r;` |
|        - | 3289 | `						}` |
|        - | 3290 | `					}` |
|   697380 | 3291 | `					if( pInstr->iP1 ){` |
|        - | 3292 | `						/* Pre-increment: result is the new value. */` |
|       71 | 3293 | `						PH7_MemObjStore(pObj,pTos);` |
|       35 | 3294 | `					}` |
|        - | 3295 | `					/* Post-increment: pTos retains the old value (a string` |
|        - | 3296 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - | 3297 | `					/* A NATIVE class's property is php's own C struct field, and` |
|        - | 3298 | ``					 * `++` writes it BACK through the write handler there — so the`` |
|        - | 3299 | `					 * conversion runs on the mutated slot. php's own answer, and` |
|        - | 3300 | `					 * the EXPRESSION's value is the unconverted sum either way:` |
|        - | 3301 | ``					 * `$i->f = 1.456008; var_dump(++$i->f, $i->f)` prints`` |
|        - | 3302 | `					 * 2.4560079999999997 then 2.456007. */` |
|   697380 | 3303 | `					PH7_NATIVE_SET_AFTER_MUTATE(pTos->nIdx,pObj)` |
|        - | 3304 | `				}` |
|   349117 | 3305 | `			}` |
|   349122 | 3306 | `		}else{` |
|        9 | 3307 | `			if( pInstr->iP1 ){` |
|      ! 0 | 3308 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 | 3309 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 | 3310 | `				}else{` |
|        - | 3311 | `					/* Force a numeric cast */` |
|      ! 0 | 3312 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3313 | `					/* Pre-increment */` |
|      ! 0 | 3314 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3315 | `						pTos->rVal++;` |
|        - | 3316 | `						/* Try to get an integer representation */` |
|      ! 0 | 3317 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3318 | `					}else{` |
|        - | 3319 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3320 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3321 | `						sxi64 r;` |
|      ! 0 | 3322 | `						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3323 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3324 | `							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;` |
|      ! 0 | 3325 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3326 | `#else` |
|        - | 3327 | `							pTos->x.iVal = r;` |
|        - | 3328 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3329 | `#endif` |
|      ! 0 | 3330 | `						}else{` |
|      ! 0 | 3331 | `							pTos->x.iVal = r;` |
|      ! 0 | 3332 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3333 | `						}` |
|        - | 3334 | `					}` |
|        - | 3335 | `				}` |
|      ! 0 | 3336 | `			}` |
|        - | 3337 | `		}` |
|   349121 | 3338 | `	}` |
|   697394 | 3339 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|   697394 | 3340 | `	break;` |
|        - | 3341 | `/*` |
|        - | 3342 | ` * DECR: P1 * *` |
|        - | 3343 | ` *` |
|        - | 3344 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - | 3345 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - | 3346 | ` * and decrement after that.` |
|        - | 3347 | ` */` |
|       72 | 3348 | `case PH7_OP_DECR:` |
|        - | 3349 | `#ifdef UNTRUST` |
|        - | 3350 | `	if( pTos < pStack ){` |
|        - | 3351 | `		goto Abort;` |
|        - | 3352 | `	}` |
|        - | 3353 | `#endif` |
|        - | 3354 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3355 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3356 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|        - | 3357 | `	 * property currently holding null). */` |
|      152 | 3358 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3359 | `	/* php refuses to decrement a string OFFSET, whatever byte it holds. */` |
|      141 | 3360 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3361 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3362 | `	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —` |
|        - | 3363 | ``	 * `--` on null is php's no-op and its write-back still dispatches set). */`` |
|      134 | 3364 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|       76 | 3365 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 3366 | `		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 3367 | `		if( VM_HOOK_PEND_IS_RMW(pTopDec->iKind) && pTopDec->nScratchIdx == pTos->nIdx ){` |
|        - | 3368 | `			SyBlob sErrMsg;` |
|        3 | 3369 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3370 | `			VmIncDecTypeErrorMsg(pTos,FALSE,&sErrMsg);` |
|        3 | 3371 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3372 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 3373 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 3374 | `			break;` |
|        - | 3375 | `		}` |
|      ! 0 | 3376 | `	}` |
|      135 | 3377 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3378 | ``		/* php's `--` operand contract, the mirror of INCR's: an array, object or`` |
|        - | 3379 | `		 * resource is a catchable TypeError, not a silent no-op. */` |
|        - | 3380 | `		SyBlob sDecMsg;` |
|        - | 3381 | `		sxi32 rcDec;` |
|       11 | 3382 | `		SyBlobInit(&sDecMsg,&pVm->sAllocator);` |
|       11 | 3383 | `		VmIncDecTypeErrorMsg(pTos,FALSE,&sDecMsg);` |
|       11 | 3384 | `		PH7_MemObjRelease(pTos);` |
|       11 | 3385 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 3386 | `		pTos->nIdx = SXU32_HIGH;` |
|       16 | 3387 | `		rcDec = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sDecMsg),` |
|        5 | 3388 | `			SyBlobLength(&sDecMsg));` |
|       11 | 3389 | `		SyBlobRelease(&sDecMsg);` |
|       11 | 3390 | `		if( rcDec == SXERR_ABORT ){ goto Abort; }` |
|       11 | 3391 | `		rc = rcDec;` |
|       13 | 3392 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3393 | `	}` |
|        - | 3394 | ``	/* NULL and BOOL stay excluded: PHP leaves `--` on either untouched (no-op) --`` |
|        - | 3395 | `	 * but 8.3 warns about both no-ops, same as the non-numeric-string one below. */` |
|      125 | 3396 | `	if( pTos->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|        - | 3397 | `		/* E_WARNING, not E_DEPRECATED -- php reports these at errno 2. */` |
|       16 | 3398 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3399 | `			"Decrement on type %s has no effect, this will change in the next major version of PHP",` |
|       10 | 3400 | `			(pTos->iFlags & MEMOBJ_NULL) ? "null" : "bool");` |
|        5 | 3401 | `	}` |
|      125 | 3402 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      115 | 3403 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3404 | `			ph7_value *pObj;` |
|      115 | 3405 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      115 | 3406 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3407 | ``					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string`` |
|        - | 3408 | `					 * (php has no string decrement); PHL rejects it. */` |
|        - | 3409 | `					SyBlob sErrMsg;` |
|        3 | 3410 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3411 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3412 | `						"Decrement on a non-numeric string is not supported",` |
|        - | 3413 | `						sizeof("Decrement on a non-numeric string is not supported")-1);` |
|        3 | 3414 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3415 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3416 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3417 | `					break;` |
|      ! 0 | 3418 | `				}else{` |
|        - | 3419 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - | 3420 | `					 * post-decrement must preserve pTos's original value, which` |
|        - | 3421 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - | 3422 | `					 * Force pTos to own its blob before coercing pObj. */` |
|      112 | 3423 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 | 3424 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 | 3425 | `					}` |
|      112 | 3426 | `					PH7_MemObjToNumeric(pObj);` |
|      112 | 3427 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|       11 | 3428 | `						pObj->rVal--;` |
|        - | 3429 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3430 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3431 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3432 | `						 * integer-valued real. */` |
|       11 | 3433 | `						PH7_MemObjTryInteger(pObj);` |
|        6 | 3434 | `					}else{` |
|        - | 3435 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3436 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3437 | `						sxi64 r;` |
|      102 | 3438 | `						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3439 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        3 | 3440 | `							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;` |
|        3 | 3441 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3442 | `#else` |
|        - | 3443 | `							pObj->x.iVal = r;` |
|        - | 3444 | `#endif` |
|        2 | 3445 | `						}else{` |
|      100 | 3446 | `							pObj->x.iVal = r;` |
|        - | 3447 | `						}` |
|        - | 3448 | `					}` |
|      112 | 3449 | `					if( pInstr->iP1 ){` |
|        - | 3450 | `						/* Pre-decrement: result is the new value. */` |
|        3 | 3451 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 | 3452 | `					}` |
|        - | 3453 | `					/* Post-decrement: pTos retains the old value. */` |
|      112 | 3454 | `					PH7_NATIVE_SET_AFTER_MUTATE(pTos->nIdx,pObj)   /* see OP_INCR */` |
|        - | 3455 | `				}` |
|       55 | 3456 | `			}` |
|       57 | 3457 | `		}else{` |
|      ! 0 | 3458 | `			if( pInstr->iP1 ){` |
|      ! 0 | 3459 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - | 3460 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 | 3461 | `				}else{` |
|        - | 3462 | `					/* Force a numeric cast */` |
|      ! 0 | 3463 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3464 | `					/* Pre-decrement */` |
|      ! 0 | 3465 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3466 | `						pTos->rVal--;` |
|        - | 3467 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 | 3468 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3469 | `					}else{` |
|        - | 3470 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3471 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3472 | `						sxi64 r;` |
|      ! 0 | 3473 | `						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3474 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3475 | `							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;` |
|      ! 0 | 3476 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3477 | `#else` |
|        - | 3478 | `							pTos->x.iVal = r;` |
|        - | 3479 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3480 | `#endif` |
|      ! 0 | 3481 | `						}else{` |
|      ! 0 | 3482 | `							pTos->x.iVal = r;` |
|      ! 0 | 3483 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3484 | `						}` |
|        - | 3485 | `					}` |
|        - | 3486 | `				}` |
|      ! 0 | 3487 | `			}` |
|        - | 3488 | `		}` |
|       55 | 3489 | `	}` |
|      122 | 3490 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|      122 | 3491 | `	break;` |
|        - | 3492 | `/*` |
|        - | 3493 | ` * UMINUS: * * *` |
|        - | 3494 | ` *` |
|        - | 3495 | ` * Perform a unary minus operation.` |
|        - | 3496 | ` */` |
|    44362 | 3497 | `case PH7_OP_UMINUS:` |
|        - | 3498 | `#ifdef UNTRUST` |
|        - | 3499 | `	if( pTos < pStack ){` |
|        - | 3500 | `		goto Abort;` |
|        - | 3501 | `	}` |
|        - | 3502 | `#endif` |
|        - | 3503 | ``	/* php's `$x * -1` operand contract: an array, object, resource or`` |
|        - | 3504 | `	 * non-numeric string is a TypeError, not a warned int(-1). */` |
|    88731 | 3505 | `	PH7_UNARY_ARITH_CONTRACT()` |
|        - | 3506 | `	/* Force a numeric (integer,real or both) cast */` |
|    88703 | 3507 | `	PH7_MemObjToNumeric(pTos);` |
|    88703 | 3508 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      269 | 3509 | `		pTos->rVal = -pTos->rVal;` |
|      132 | 3510 | `	}` |
|    88703 | 3511 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    88487 | 3512 | `		if( pTos->x.iVal == SMALLEST_INT64 ){` |
|        - | 3513 | `			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a` |
|        - | 3514 | `			 * REAL representation is already present it is the negated` |
|        - | 3515 | `			 * authoritative value, so just drop the now-stale cached int. The` |
|        - | 3516 | `			 * integer-only build has no float type, so it wraps (two's` |
|        - | 3517 | `			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */` |
|        - | 3518 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        7 | 3519 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 | 3520 | `				pTos->rVal = -(ph7_real)pTos->x.iVal;` |
|        7 | 3521 | `				MemObjSetType(pTos,MEMOBJ_REAL);` |
|        4 | 3522 | `			}else{` |
|      ! 0 | 3523 | `				pTos->iFlags &= ~MEMOBJ_INT;` |
|        - | 3524 | `			}` |
|        - | 3525 | `#else` |
|        - | 3526 | `			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);` |
|        - | 3527 | `#endif` |
|        4 | 3528 | `		}else{` |
|    88481 | 3529 | `			pTos->x.iVal = -pTos->x.iVal;` |
|        - | 3530 | `		}` |
|    44241 | 3531 | `	}` |
|    88703 | 3532 | `	break;` |
|        - | 3533 | `/*` |
|        - | 3534 | ` * UPLUS: * * *` |
|        - | 3535 | ` *` |
|        - | 3536 | ` * Perform a unary plus operation.` |
|        - | 3537 | ` */` |
|       22 | 3538 | `case PH7_OP_UPLUS:` |
|        - | 3539 | `#ifdef UNTRUST` |
|        - | 3540 | `	if( pTos < pStack ){` |
|        - | 3541 | `		goto Abort;` |
|        - | 3542 | `	}` |
|        - | 3543 | `#endif` |
|        - | 3544 | ``	/* Unary plus is php's `$x * 1`, so it answers the same TypeError as unary`` |
|        - | 3545 | ``	 * minus -- naming `int` as the other operand, exactly as php does. */`` |
|       45 | 3546 | `	PH7_UNARY_ARITH_CONTRACT()` |
|        - | 3547 | `	/* Force a numeric (integer,real or both) cast */` |
|       39 | 3548 | `	PH7_MemObjToNumeric(pTos);` |
|       39 | 3549 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3550 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 | 3551 | `	}` |
|       39 | 3552 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       39 | 3553 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       19 | 3554 | `	}` |
|       39 | 3555 | `	break;` |
|        - | 3556 | `/*` |
|        - | 3557 | ` * OP_LNOT: * * *` |
|        - | 3558 | ` *` |
|        - | 3559 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - | 3560 | ` * with its complement.` |
|        - | 3561 | ` */` |
|    35468 | 3562 | `case PH7_OP_LNOT:` |
|        - | 3563 | `#ifdef UNTRUST` |
|        - | 3564 | `	if( pTos < pStack ){` |
|        - | 3565 | `		goto Abort;` |
|        - | 3566 | `	}` |
|        - | 3567 | `#endif` |
|        - | 3568 | `	/* Force a boolean cast */` |
|    70934 | 3569 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      371 | 3570 | `		PH7_MemObjToBool(pTos);` |
|      183 | 3571 | `	}` |
|    70934 | 3572 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    70934 | 3573 | `	break;` |
|        - | 3574 | `/*` |
|        - | 3575 | ` * OP_BITNOT: * * *` |
|        - | 3576 | ` *` |
|        - | 3577 | ` * Interpret the top of the stack as an value.Replace it` |
|        - | 3578 | ` * with its ones-complement.` |
|        - | 3579 | ` */` |
|       94 | 3580 | `case PH7_OP_BITNOT:` |
|        - | 3581 | `#ifdef UNTRUST` |
|        - | 3582 | `	if( pTos < pStack ){` |
|        - | 3583 | `		goto Abort;` |
|        - | 3584 | `	}` |
|        - | 3585 | `#endif` |
|      193 | 3586 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - | 3587 | ``		/* php's `~` over a STRING is a per-BYTE ones-complement of the same`` |
|        - | 3588 | ``		 * length — a binary string, not an integer operation, so `~"abc"` is`` |
|        - | 3589 | `		 * "\x9e\x9d\x9c" where PHL cast the string to an int and answered` |
|        - | 3590 | `		 * int(-1). The operand's blob can be a READ-ONLY VIEW of the variable's` |
|        - | 3591 | `		 * own buffer (PH7_MemObjLoad), so complement into a fresh blob and swap` |
|        - | 3592 | `		 * it in rather than writing through the view. */` |
|        - | 3593 | `		SyBlob sNotBuf;` |
|       17 | 3594 | `		const unsigned char *zNotIn = (const unsigned char *)SyBlobData(&pTos->sBlob);` |
|       17 | 3595 | `		sxu32 nNotIn = SyBlobLength(&pTos->sBlob), iNot;` |
|       17 | 3596 | `		SyBlobInit(&sNotBuf,&pVm->sAllocator);` |
|       53 | 3597 | `		for( iNot = 0 ; iNot < nNotIn ; ++iNot ){` |
|       37 | 3598 | `			unsigned char cNot = (unsigned char)~zNotIn[iNot];` |
|       37 | 3599 | `			SyBlobAppend(&sNotBuf,(const void *)&cNot,sizeof(char));` |
|       19 | 3600 | `		}` |
|       17 | 3601 | `		PH7_MemObjRelease(pTos);` |
|       17 | 3602 | `		MemObjSetType(pTos,MEMOBJ_STRING);` |
|       17 | 3603 | `		if( SyBlobLength(&sNotBuf) > 0 ){` |
|       15 | 3604 | `			SyBlobAppend(&pTos->sBlob,SyBlobData(&sNotBuf),SyBlobLength(&sNotBuf));` |
|        7 | 3605 | `		}` |
|       17 | 3606 | `		SyBlobRelease(&sNotBuf);` |
|       17 | 3607 | `		break;` |
|        - | 3608 | `	}` |
|      177 | 3609 | `	if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        - | 3610 | `		/* Everything else — null, a bool, an array, an object, a resource — has` |
|        - | 3611 | `		 * no bitwise-not in php at all: a catchable TypeError naming the operand,` |
|        - | 3612 | `		 * where PHL cast it to an int and answered ~0 / ~1. */` |
|        - | 3613 | `		SyBlob sNotMsg;` |
|        - | 3614 | `		sxi32 rcNot;` |
|       25 | 3615 | `		SyBlobInit(&sNotMsg,&pVm->sAllocator);` |
|       25 | 3616 | `		VmBitNotTypeErrorMsg(pTos,&sNotMsg);` |
|       25 | 3617 | `		PH7_MemObjRelease(pTos);` |
|       25 | 3618 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       25 | 3619 | `		pTos->nIdx = SXU32_HIGH;` |
|       37 | 3620 | `		rcNot = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sNotMsg),` |
|       12 | 3621 | `			SyBlobLength(&sNotMsg));` |
|       25 | 3622 | `		SyBlobRelease(&sNotMsg);` |
|       25 | 3623 | `		if( rcNot == SXERR_ABORT ){ goto Abort; }` |
|       25 | 3624 | `		rc = rcNot;` |
|       27 | 3625 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3626 | `	}` |
|        - | 3627 | `	/* Force an integer cast (php deprecates a lossy float here too) */` |
|      153 | 3628 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      153 | 3629 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      153 | 3630 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 3631 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 3632 | `	}` |
|      153 | 3633 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        - | 3634 | `	/* An INTEGRAL float carries a cached MEMOBJ_INT beside MEMOBJ_REAL, so the` |
|        - | 3635 | `` 	 * cast above is skipped and the value kept rendering as a float: `~2.0` `` |
|        - | 3636 | ``	 * answered 2.0 instead of -3. `~` always yields an int. */`` |
|      153 | 3637 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      153 | 3638 | `	break;` |
|        - | 3639 | `/* OP_MUL * * *` |
|        - | 3640 | ` * OP_MUL_STORE * * *` |
|        - | 3641 | ` *` |
|        - | 3642 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - | 3643 | ` * and push the result back onto the stack.` |
|        - | 3644 | ` */` |
|     1531 | 3645 | `case PH7_OP_MUL:` |
|        - | 3646 | `case PH7_OP_MUL_STORE: {` |
|        - | 3647 | `	VmOpRc rcOp;` |
|        - | 3648 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     3067 | 3649 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     3065 | 3650 | `	sState.pTos = pTos;` |
|     3065 | 3651 | `	sState.pc = pc;` |
|     3065 | 3652 | `	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);` |
|     3065 | 3653 | `	pTos = sState.pTos;` |
|     3065 | 3654 | `	pc = sState.pc;` |
|     3065 | 3655 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3656 | `		goto Abort;` |
|     3065 | 3657 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3658 | `		goto Exception;` |
|        - | 3659 | `	}` |
|     3065 | 3660 | `	break;` |
|        - | 3661 | `					  }` |
|        - | 3662 | `/* OP_POW * * *` |
|        - | 3663 | ` * OP_POW_STORE * * *` |
|        - | 3664 | ` *` |
|        - | 3665 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - | 3666 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - | 3667 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - | 3668 | ` * fits in sxi64; otherwise the result is a double.` |
|        - | 3669 | ` */` |
|       94 | 3670 | `case PH7_OP_POW:` |
|        - | 3671 | `case PH7_OP_POW_STORE: {` |
|        - | 3672 | `	VmOpRc rcOp;` |
|        - | 3673 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      189 | 3674 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      187 | 3675 | `	sState.pTos = pTos;` |
|      187 | 3676 | `	sState.pc = pc;` |
|      187 | 3677 | `	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);` |
|      187 | 3678 | `	pTos = sState.pTos;` |
|      187 | 3679 | `	pc = sState.pc;` |
|      187 | 3680 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3681 | `		goto Abort;` |
|      187 | 3682 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 3683 | `		goto Exception;` |
|        - | 3684 | `	}` |
|      183 | 3685 | `	break;` |
|        - | 3686 | `					  }` |
|        - | 3687 | `/* OP_ADD * * *` |
|        - | 3688 | ` *` |
|        - | 3689 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 3690 | ` * and push the result back onto the stack.` |
|        - | 3691 | ` */` |
|    10257 | 3692 | `case PH7_OP_ADD:{` |
|    20519 | 3693 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3694 | `#ifdef UNTRUST` |
|        - | 3695 | `	if( pNos < pStack ){` |
|        - | 3696 | `		goto Abort;` |
|        - | 3697 | `	}` |
|        - | 3698 | `#endif` |
|        - | 3699 | `	{` |
|        - | 3700 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|        - | 3701 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|        - | 3702 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|        - | 3703 | `		SyBlob sArMsg;` |
|    20519 | 3704 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    20519 | 3705 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 3706 | `			sxi32 rcAr;` |
|       11 | 3707 | `			VmPopOperand(&pTos,1);` |
|       11 | 3708 | `			PH7_MemObjRelease(pTos);` |
|       11 | 3709 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 3710 | `			pTos->nIdx = SXU32_HIGH;` |
|       16 | 3711 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|        5 | 3712 | `				SyBlobLength(&sArMsg));` |
|       11 | 3713 | `			SyBlobRelease(&sArMsg);` |
|       11 | 3714 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|       11 | 3715 | `			rc = rcAr;` |
|       11 | 3716 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3717 | `		}` |
|    20509 | 3718 | `		SyBlobRelease(&sArMsg);` |
|        - | 3719 | `	}` |
|        - | 3720 | `	/* Perform the addition */` |
|    20509 | 3721 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|    20509 | 3722 | `	VmPopOperand(&pTos,1);` |
|    20509 | 3723 | `	break;` |
|        - | 3724 | `				}` |
|        - | 3725 | `/*` |
|        - | 3726 | ` * OP_ADD_STORE * * *` |
|        - | 3727 | ` *` |
|        - | 3728 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 3729 | ` * and push the result back onto the stack.` |
|        - | 3730 | ` */` |
|     3319 | 3731 | `case PH7_OP_ADD_STORE:{` |
|     6643 | 3732 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3733 | `	ph7_value *pObj;` |
|        - | 3734 | `	sxu32 nIdx;` |
|        - | 3735 | `#ifdef UNTRUST` |
|        - | 3736 | `	if( pNos < pStack ){` |
|        - | 3737 | `		goto Abort;` |
|        - | 3738 | `	}` |
|        - | 3739 | `#endif` |
|        - | 3740 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     6645 | 3741 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 3742 | `	{` |
|        - | 3743 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 3744 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 3745 | `		SyBlob sArMsg;` |
|     6637 | 3746 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     6637 | 3747 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 3748 | `			sxi32 rcAr;` |
|      ! 0 | 3749 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 3750 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 3751 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 3752 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 3753 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 3754 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 3755 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 3756 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 3757 | `			rc = rcAr;` |
|      ! 0 | 3758 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3759 | `		}` |
|     6637 | 3760 | `		SyBlobRelease(&sArMsg);` |
|        - | 3761 | `	}` |
|        - | 3762 | `	/* Perform the addition */` |
|     6637 | 3763 | `	nIdx = pTos->nIdx;` |
|     6637 | 3764 | `	if( nIdx == pVm->nGlobalIdx ){` |
|        - | 3765 | `		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment` |
|        - | 3766 | `		 * (a compile-time fatal in php; raised here, same message). */` |
|        3 | 3767 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3768 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 3769 | `		pVm->iExitStatus = 255;` |
|        3 | 3770 | `		pVm->bHaltRequested = 1;` |
|        3 | 3771 | `		goto Abort;` |
|        - | 3772 | `	}` |
|     6635 | 3773 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - | 3774 | `	/* Peform the store operation */` |
|     6635 | 3775 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 3776 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6635 | 3777 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     6635 | 3778 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     6633 | 3779 | `		PH7_MemObjStore(pTos,pObj);` |
|     3314 | 3780 | `	}` |
|     6633 | 3781 | `	PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|        - | 3782 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     6633 | 3783 | `	PH7_MemObjStore(pTos,pNos);` |
|     6633 | 3784 | `	VmPopOperand(&pTos,1);` |
|     6633 | 3785 | `	break;` |
|        - | 3786 | `				}` |
|        - | 3787 | `/* OP_SUB * * *` |
|        - | 3788 | ` *` |
|        - | 3789 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 3790 | ` * first (what was next on the stack) from the second (the` |
|        - | 3791 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 3792 | ` */` |
|    25072 | 3793 | `case PH7_OP_SUB: {` |
|        - | 3794 | `	VmOpRc rcOp;` |
|    50486 | 3795 | `	sState.pTos = pTos;` |
|    50486 | 3796 | `	sState.pc = pc;` |
|    50486 | 3797 | `	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);` |
|    50486 | 3798 | `	pTos = sState.pTos;` |
|    50486 | 3799 | `	pc = sState.pc;` |
|    50486 | 3800 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3801 | `		goto Abort;` |
|    50486 | 3802 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3803 | `		goto Exception;` |
|        - | 3804 | `	}` |
|    50486 | 3805 | `	break;` |
|        - | 3806 | `					  }` |
|        - | 3807 | `/* OP_SUB_STORE * * *` |
|        - | 3808 | ` *` |
|        - | 3809 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 3810 | ` * first (what was next on the stack) from the second (the` |
|        - | 3811 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 3812 | ` */` |
|        8 | 3813 | `case PH7_OP_SUB_STORE: {` |
|        - | 3814 | `	VmOpRc rcOp;` |
|        - | 3815 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       18 | 3816 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       16 | 3817 | `	sState.pTos = pTos;` |
|       16 | 3818 | `	sState.pc = pc;` |
|       16 | 3819 | `	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);` |
|       16 | 3820 | `	pTos = sState.pTos;` |
|       16 | 3821 | `	pc = sState.pc;` |
|       16 | 3822 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3823 | `		goto Abort;` |
|       16 | 3824 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 3825 | `		goto Exception;` |
|        - | 3826 | `	}` |
|       14 | 3827 | `	break;` |
|        - | 3828 | `					  }` |
|        - | 3829 |  |
|        - | 3830 | `/*` |
|        - | 3831 | ` * OP_MOD * * *` |
|        - | 3832 | ` *` |
|        - | 3833 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3834 | ` * first (what was next on the stack) from the second (the` |
|        - | 3835 | ` * top of the stack) and push the remainder after division` |
|        - | 3836 | ` * onto the stack.` |
|        - | 3837 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 3838 | ` */` |
|      528 | 3839 | `case PH7_OP_MOD: {` |
|        - | 3840 | `	VmOpRc rcOp;` |
|     1061 | 3841 | `	sState.pTos = pTos;` |
|     1061 | 3842 | `	sState.pc = pc;` |
|     1061 | 3843 | `	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);` |
|     1061 | 3844 | `	pTos = sState.pTos;` |
|     1061 | 3845 | `	pc = sState.pc;` |
|     1061 | 3846 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3847 | `		goto Abort;` |
|     1061 | 3848 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       22 | 3849 | `		goto Exception;` |
|        - | 3850 | `	}` |
|     1041 | 3851 | `	break;` |
|        - | 3852 | `					  }` |
|        - | 3853 | `/*` |
|        - | 3854 | ` * OP_MOD_STORE * * *` |
|        - | 3855 | ` *` |
|        - | 3856 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3857 | ` * first (what was next on the stack) from the second (the` |
|        - | 3858 | ` * top of the stack) and push the remainder after division` |
|        - | 3859 | ` * onto the stack.` |
|        - | 3860 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 3861 | ` */` |
|        6 | 3862 | `case PH7_OP_MOD_STORE: {` |
|        - | 3863 | `	VmOpRc rcOp;` |
|        - | 3864 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       13 | 3865 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       11 | 3866 | `	sState.pTos = pTos;` |
|       11 | 3867 | `	sState.pc = pc;` |
|       11 | 3868 | `	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);` |
|       11 | 3869 | `	pTos = sState.pTos;` |
|       11 | 3870 | `	pc = sState.pc;` |
|       11 | 3871 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3872 | `		goto Abort;` |
|       11 | 3873 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 3874 | `		goto Exception;` |
|        - | 3875 | `	}` |
|        7 | 3876 | `	break;` |
|        - | 3877 | `					  }` |
|        - | 3878 | `/*` |
|        - | 3879 | ` * OP_DIV * * *` |
|        - | 3880 | ` *` |
|        - | 3881 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3882 | ` * first (what was next on the stack) from the second (the` |
|        - | 3883 | ` * top of the stack) and push the result onto the stack.` |
|        - | 3884 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 3885 | ` */` |
|      271 | 3886 | `case PH7_OP_DIV: {` |
|        - | 3887 | `	VmOpRc rcOp;` |
|      545 | 3888 | `	sState.pTos = pTos;` |
|      545 | 3889 | `	sState.pc = pc;` |
|      545 | 3890 | `	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);` |
|      545 | 3891 | `	pTos = sState.pTos;` |
|      545 | 3892 | `	pc = sState.pc;` |
|      545 | 3893 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3894 | `		goto Abort;` |
|      545 | 3895 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 3896 | `		goto Exception;` |
|        - | 3897 | `	}` |
|      539 | 3898 | `	break;` |
|        - | 3899 | `					  }` |
|        - | 3900 | `/*` |
|        - | 3901 | ` * OP_DIV_STORE * * *` |
|        - | 3902 | ` *` |
|        - | 3903 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3904 | ` * first (what was next on the stack) from the second (the` |
|        - | 3905 | ` * top of the stack) and push the result onto the stack.` |
|        - | 3906 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 3907 | ` */` |
|        8 | 3908 | `case PH7_OP_DIV_STORE:{` |
|       17 | 3909 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3910 | `	ph7_value *pObj;` |
|        - | 3911 | `	ph7_real a,b,r;` |
|        - | 3912 | `#ifdef UNTRUST` |
|        - | 3913 | `	if( pNos < pStack ){` |
|        - | 3914 | `		goto Abort;` |
|        - | 3915 | `	}` |
|        - | 3916 | `#endif` |
|        - | 3917 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       17 | 3918 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 3919 | `	{` |
|        - | 3920 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 3921 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 3922 | `		SyBlob sArMsg;` |
|       15 | 3923 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|       15 | 3924 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"/",&sArMsg) != SXRET_OK ){` |
|        - | 3925 | `			sxi32 rcAr;` |
|      ! 0 | 3926 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 3927 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 3928 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 3929 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 3930 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 3931 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 3932 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 3933 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 3934 | `			rc = rcAr;` |
|      ! 0 | 3935 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3936 | `		}` |
|       15 | 3937 | `		SyBlobRelease(&sArMsg);` |
|        - | 3938 | `	}` |
|        - | 3939 | `	/* Force the operands to be real */` |
|       15 | 3940 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       15 | 3941 | `		PH7_MemObjToReal(pTos);` |
|        7 | 3942 | `	}` |
|       15 | 3943 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       15 | 3944 | `		PH7_MemObjToReal(pNos);` |
|        7 | 3945 | `	}` |
|        - | 3946 | `	/* Perform the requested operation */` |
|       15 | 3947 | `	a = pTos->rVal;` |
|       15 | 3948 | `	b = pNos->rVal;` |
|       15 | 3949 | `	if( b == 0 ){` |
|        - | 3950 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|        - | 3951 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|        5 | 3952 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|        9 | 3953 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      ! 0 | 3954 | `	}else{` |
|       11 | 3955 | `		r = a/b;` |
|        - | 3956 | `		/* Push the result */` |
|       11 | 3957 | `		pNos->rVal = r;` |
|       11 | 3958 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - | 3959 | `		/* Try to get an integer representation */` |
|       11 | 3960 | `		PH7_MemObjTryInteger(pNos);` |
|        - | 3961 | `	}` |
|       11 | 3962 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 3963 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       11 | 3964 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       11 | 3965 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       11 | 3966 | `		PH7_MemObjStore(pNos,pObj);` |
|        5 | 3967 | `	}` |
|       11 | 3968 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       11 | 3969 | `	VmPopOperand(&pTos,1);` |
|       11 | 3970 | `	break;` |
|        - | 3971 | `				}` |
|        - | 3972 | `/* OP_BAND * * *` |
|        - | 3973 | ` *` |
|        - | 3974 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3975 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 3976 | ` * two elements.` |
|        - | 3977 | `*/` |
|        - | 3978 | `/* OP_BOR * * *` |
|        - | 3979 | ` *` |
|        - | 3980 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3981 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 3982 | ` * two elements.` |
|        - | 3983 | ` */` |
|        - | 3984 | `/* OP_BXOR * * *` |
|        - | 3985 | ` *` |
|        - | 3986 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3987 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 3988 | ` * two elements.` |
|        - | 3989 | ` */` |
|      827 | 3990 | `case PH7_OP_BAND:` |
|        - | 3991 | `case PH7_OP_BOR:` |
|        - | 3992 | `case PH7_OP_BXOR:{` |
|     1656 | 3993 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3994 | `	sxi64 a,b,r;` |
|        - | 3995 | `	int cBwOp;` |
|        - | 3996 | `#ifdef UNTRUST` |
|        - | 3997 | `	if( pNos < pStack ){` |
|        - | 3998 | `		goto Abort;` |
|        - | 3999 | `	}` |
|        - | 4000 | `#endif` |
|     1656 | 4001 | `	cBwOp = pInstr->iOp == PH7_OP_BOR ? '\|' : (pInstr->iOp == PH7_OP_BXOR ? '^' : '&');` |
|     1656 | 4002 | `	if( (pNos->iFlags & MEMOBJ_STRING) && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 4003 | `		/* TWO strings: php's per-byte string operation, result a string. */` |
|       35 | 4004 | `		PH7_STRING_BITWISE_RESULT(pNos,pNos,pTos,cBwOp)` |
|       35 | 4005 | `		VmPopOperand(&pTos,1);` |
|       35 | 4006 | `		break;` |
|        - | 4007 | `	}` |
|        - | 4008 | `	{` |
|        - | 4009 | `		char zBwOp[2];` |
|     1622 | 4010 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 4011 | `		/* Anything else is php's arithmetic operand contract, named for this` |
|        - | 4012 | ``		 * operator (`array & int`), not a silent integer cast. */`` |
|     1624 | 4013 | `		PH7_BITWISE_ARITH_CONTRACT(pNos,pTos,zBwOp,1)` |
|        - | 4014 | `	}` |
|        - | 4015 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     1590 | 4016 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     1590 | 4017 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     1584 | 4018 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     1584 | 4019 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     1584 | 4020 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|        9 | 4021 | `		PH7_MemObjToInteger(pTos);` |
|        4 | 4022 | `	}` |
|     1584 | 4023 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|       11 | 4024 | `		PH7_MemObjToInteger(pNos);` |
|        5 | 4025 | `	}` |
|        - | 4026 | `	/* Perform the requested operation */` |
|     1584 | 4027 | `	a = pNos->x.iVal;` |
|     1584 | 4028 | `	b = pTos->x.iVal;` |
|     1584 | 4029 | `	switch(pInstr->iOp){` |
|      260 | 4030 | `	case PH7_OP_BOR_STORE:` |
|      525 | 4031 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        8 | 4032 | `	case PH7_OP_BXOR_STORE:` |
|       17 | 4033 | `	case PH7_OP_BXOR: r = a^b; break;` |
|      522 | 4034 | `	case PH7_OP_BAND_STORE:` |
|      521 | 4035 | `	case PH7_OP_BAND:` |
|     1048 | 4036 | `	default:          r = a&b; break;` |
|        - | 4037 | `	}` |
|        - | 4038 | `	/* Push the result */` |
|     1584 | 4039 | `	pNos->x.iVal = r;` |
|     1584 | 4040 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     1584 | 4041 | `	VmPopOperand(&pTos,1);` |
|     1584 | 4042 | `	break;` |
|        - | 4043 | `				 }` |
|        - | 4044 | `/* OP_BAND_STORE * * *` |
|        - | 4045 | ` *` |
|        - | 4046 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4047 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 4048 | ` * two elements.` |
|        - | 4049 | `*/` |
|        - | 4050 | `/* OP_BOR_STORE * * *` |
|        - | 4051 | ` *` |
|        - | 4052 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4053 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 4054 | ` * two elements.` |
|        - | 4055 | ` */` |
|        - | 4056 | `/* OP_BXOR_STORE * * *` |
|        - | 4057 | ` *` |
|        - | 4058 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4059 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 4060 | ` * two elements.` |
|        - | 4061 | ` */` |
|       50 | 4062 | `case PH7_OP_BAND_STORE:` |
|        - | 4063 | `case PH7_OP_BOR_STORE:` |
|        - | 4064 | `case PH7_OP_BXOR_STORE:{` |
|      101 | 4065 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4066 | `	ph7_value *pObj;` |
|        - | 4067 | `	sxi64 a,b,r;` |
|        - | 4068 | `	int cBwOp,bBwStr;` |
|        - | 4069 | `#ifdef UNTRUST` |
|        - | 4070 | `	if( pNos < pStack ){` |
|        - | 4071 | `		goto Abort;` |
|        - | 4072 | `	}` |
|        - | 4073 | `#endif` |
|        - | 4074 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      101 | 4075 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       95 | 4076 | `	cBwOp = pInstr->iOp == PH7_OP_BOR_STORE ? '\|' : (pInstr->iOp == PH7_OP_BXOR_STORE ? '^' : '&');` |
|       95 | 4077 | `	bBwStr = (pNos->iFlags & MEMOBJ_STRING) != 0 && (pTos->iFlags & MEMOBJ_STRING) != 0;` |
|       95 | 4078 | `	if( !bBwStr ){` |
|        - | 4079 | `		char zBwOp[2];` |
|       89 | 4080 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 4081 | ``		/* `$x &= v` answers the same contract as `$x & v` (php's compound`` |
|        - | 4082 | `		 * assignment is the operator plus a store), but through its own error` |
|        - | 4083 | `		 * path, which is POSITIONAL: the lvalue is named first even when the` |
|        - | 4084 | ``		 * right operand is the offender (`$x = 1; $x &= $o` is "int & BsocP",`` |
|        - | 4085 | ``		 * where the plain `1 & $o` is "BsocP & int"). */`` |
|       89 | 4086 | `		PH7_BITWISE_ARITH_CONTRACT(pTos,pNos,zBwOp,0)` |
|        - | 4087 | `		/* Force the operands to be integer (php deprecates a lossy float here) */` |
|       85 | 4088 | `		rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|       85 | 4089 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|       85 | 4090 | `		rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       85 | 4091 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|       85 | 4092 | `		if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 4093 | `			PH7_MemObjToInteger(pTos);` |
|      ! 0 | 4094 | `		}` |
|       85 | 4095 | `		if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        3 | 4096 | `			PH7_MemObjToInteger(pNos);` |
|        1 | 4097 | `		}` |
|       42 | 4098 | `	}` |
|       91 | 4099 | `	if( bBwStr ){` |
|        - | 4100 | `		/* TWO strings: php's per-byte string operation, result a string. The` |
|        - | 4101 | `		 * result lands in pNos, which the store tail below writes into the` |
|        - | 4102 | `		 * lvalue's slot exactly like the integer result. */` |
|        7 | 4103 | `		PH7_STRING_BITWISE_RESULT(pNos,pTos,pNos,cBwOp)` |
|        4 | 4104 | `	}else{` |
|        - | 4105 | `	/* Perform the requested operation */` |
|       85 | 4106 | `	a = pTos->x.iVal;` |
|       85 | 4107 | `	b = pNos->x.iVal;` |
|       85 | 4108 | `	switch(pInstr->iOp){` |
|       32 | 4109 | `	case PH7_OP_BOR_STORE:` |
|       65 | 4110 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        5 | 4111 | `	case PH7_OP_BXOR_STORE:` |
|       11 | 4112 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        5 | 4113 | `	case PH7_OP_BAND_STORE:` |
|        5 | 4114 | `	case PH7_OP_BAND:` |
|       11 | 4115 | `	default:          r = a&b; break;` |
|        - | 4116 | `	}` |
|        - | 4117 | `	/* Push the result */` |
|       85 | 4118 | `	pNos->x.iVal = r;` |
|       85 | 4119 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        - | 4120 | `	}` |
|       91 | 4121 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 4122 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       91 | 4123 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       91 | 4124 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       91 | 4125 | `		PH7_MemObjStore(pNos,pObj);` |
|       45 | 4126 | `	}` |
|       91 | 4127 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       91 | 4128 | `	VmPopOperand(&pTos,1);` |
|       91 | 4129 | `	break;` |
|        - | 4130 | `				 }` |
|        - | 4131 | `/* OP_SHL * * *` |
|        - | 4132 | ` *` |
|        - | 4133 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4134 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4135 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4136 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4137 | ` */` |
|        - | 4138 | `/* OP_SHR * * *` |
|        - | 4139 | ` *` |
|        - | 4140 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4141 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4142 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4143 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4144 | ` */` |
|       55 | 4145 | `case PH7_OP_SHL:` |
|        - | 4146 | `case PH7_OP_SHR: {` |
|        - | 4147 | `	VmOpRc rcOp;` |
|      112 | 4148 | `	sState.pTos = pTos;` |
|      112 | 4149 | `	sState.pc = pc;` |
|      112 | 4150 | `	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);` |
|      112 | 4151 | `	pTos = sState.pTos;` |
|      112 | 4152 | `	pc = sState.pc;` |
|      112 | 4153 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4154 | `		goto Abort;` |
|      112 | 4155 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       34 | 4156 | `		goto Exception;` |
|        - | 4157 | `	}` |
|       80 | 4158 | `	break;` |
|        - | 4159 | `					  }` |
|        - | 4160 | `/*  OP_SHL_STORE * * *` |
|        - | 4161 | ` *` |
|        - | 4162 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4163 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4164 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4165 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4166 | ` */` |
|        - | 4167 | `/* OP_SHR_STORE * * *` |
|        - | 4168 | ` *` |
|        - | 4169 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4170 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4171 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4172 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4173 | ` */` |
|       20 | 4174 | `case PH7_OP_SHL_STORE:` |
|        - | 4175 | `case PH7_OP_SHR_STORE: {` |
|        - | 4176 | `	VmOpRc rcOp;` |
|        - | 4177 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       41 | 4178 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       37 | 4179 | `	sState.pTos = pTos;` |
|       37 | 4180 | `	sState.pc = pc;` |
|       37 | 4181 | `	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);` |
|       37 | 4182 | `	pTos = sState.pTos;` |
|       37 | 4183 | `	pc = sState.pc;` |
|       37 | 4184 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4185 | `		goto Abort;` |
|       37 | 4186 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 4187 | `		goto Exception;` |
|        - | 4188 | `	}` |
|       29 | 4189 | `	break;` |
|        - | 4190 | `					  }` |
|        - | 4191 | `/* CAT:  P1 * *` |
|        - | 4192 | ` *` |
|        - | 4193 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - | 4194 | ` * back.` |
|        - | 4195 | ` */` |
|   112092 | 4196 | `case PH7_OP_CAT:{` |
|        - | 4197 | `	ph7_value *pNos,*pCur;` |
|   224189 | 4198 | `	if( pInstr->iP1 < 1 ){` |
|   188215 | 4199 | `		pNos = &pTos[-1];` |
|    94110 | 4200 | `	}else{` |
|    35979 | 4201 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - | 4202 | `	}` |
|        - | 4203 | `#ifdef UNTRUST` |
|        - | 4204 | `	if( pNos < pStack ){` |
|        - | 4205 | `		goto Abort;` |
|        - | 4206 | `	}` |
|        - | 4207 | `#endif` |
|        - | 4208 | `	/* Force a string cast (user-visible: warns on an array operand, §2).` |
|        - | 4209 | `	 * php coerces the operands left to right, so the leftmost not-stringable` |
|        - | 4210 | `	 * object is the one that throws. */` |
|        - | 4211 | `	{` |
|   224189 | 4212 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|   224189 | 4213 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4214 | `	}` |
|   224187 | 4215 | `	pCur = &pNos[1];` |
|        - | 4216 | `	{` |
|        - | 4217 | `		/* rcSv leaves the loop with the coercion status: the routing macro expands` |
|        - | 4218 | ``		 * to a bare `break` here, which inside the while would fall THROUGH to the`` |
|        - | 4219 | ``		 * `pTos = pNos` below with a throw pending. Route it at case level. */`` |
|   224187 | 4220 | `		sxi32 rcSv = SXRET_OK;` |
|   460375 | 4221 | `		while( pCur <= pTos ){` |
|   236637 | 4222 | `			rcSv = PH7_MemObjToStringUV(pCur);` |
|   236637 | 4223 | `			if( rcSv != SXRET_OK ){` |
|      447 | 4224 | `				break;` |
|        - | 4225 | `			}` |
|        - | 4226 | `			/* Perform the concatenation */` |
|   236193 | 4227 | `			if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   235643 | 4228 | `				if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - | 4229 | `					/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 | 4230 | `					PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4231 | `					goto Abort;` |
|        - | 4232 | `				}` |
|   117819 | 4233 | `			}` |
|   236193 | 4234 | `			SyBlobRelease(&pCur->sBlob);` |
|   236193 | 4235 | `			pCur++;` |
|        5 | 4236 | `		}` |
|   225009 | 4237 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4238 | `	}` |
|   223743 | 4239 | `	pTos = pNos;` |
|   223743 | 4240 | `	break;` |
|        - | 4241 | `				}` |
|        - | 4242 | `/*  CAT_STORE: * * *` |
|        - | 4243 | ` *` |
|        - | 4244 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - | 4245 | ` * back.` |
|        - | 4246 | ` */` |
|    20255 | 4247 | `case PH7_OP_CAT_STORE:{` |
|    40515 | 4248 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4249 | `	ph7_value *pObj;` |
|        - | 4250 | `	sxu32 nIdx;` |
|        - | 4251 | `#ifdef UNTRUST` |
|        - | 4252 | `	if( pNos < pStack ){` |
|        - | 4253 | `		goto Abort;` |
|        - | 4254 | `	}` |
|        - | 4255 | `#endif` |
|        - | 4256 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|    60764 | 4257 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 4258 | `	/* The right operand must be a string to append it (user-visible, §2) */` |
|        - | 4259 | `	{` |
|    40513 | 4260 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|    40517 | 4261 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4262 | `	}` |
|    40503 | 4263 | `	nIdx = pTos->nIdx;` |
|        - | 4264 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - | 4265 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - | 4266 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - | 4267 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - | 4268 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - | 4269 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - | 4270 | `	 * the source we copy from — references share the slot index, so one check` |
|        - | 4271 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - | 4272 | `	 * must run before any mutation (left to the slow path).` |
|        - | 4273 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - | 4274 | `	 * and remains O(n^2) by design. */` |
|    40498 | 4275 | `	if( nIdx != SXU32_HIGH` |
|    40498 | 4276 | `	 && nIdx != pNos->nIdx` |
|    40494 | 4277 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|    40495 | 4278 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|    20439 | 4279 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|        - | 4280 | `		/* e.g. $x = 5; $x .= "a";  ->  "5a" (user-visible: warns if $x is an array,` |
|        - | 4281 | `		 * throws if it is a not-stringable object — and then the lvalue keeps` |
|        - | 4282 | `		 * holding that object, since the throw abandons the coercion) */` |
|        - | 4283 | `		{` |
|    40487 | 4284 | `			sxi32 rcSv = PH7_MemObjToStringUV(pObj);` |
|    40495 | 4285 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4286 | `		}` |
|    40481 | 4287 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|    40423 | 4288 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4289 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - | 4290 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 | 4291 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4292 | `				goto Abort;` |
|        - | 4293 | `			}` |
|    20209 | 4294 | `		}` |
|        - | 4295 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - | 4296 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - | 4297 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - | 4298 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - | 4299 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - | 4300 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - | 4301 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - | 4302 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - | 4303 | `		 * the same slot is appended to again later in the statement` |
|        - | 4304 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - | 4305 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - | 4306 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|    40481 | 4307 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 | 4308 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 | 4309 | `		}` |
|        - | 4310 | `		/* A hooked lvalue's scratch slot was appended in place — dispatch the` |
|        - | 4311 | `		 * set side now (the consume reads the computed value from the slot). */` |
|    40481 | 4312 | `		PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|    40481 | 4313 | `		pNos->nIdx = SXU32_HIGH;` |
|    40481 | 4314 | `		VmPopOperand(&pTos,1);` |
|    40481 | 4315 | `		break;` |
|        - | 4316 | `	}` |
|        - | 4317 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|        - | 4318 | `	/* Force a string cast (user-visible: warns if the lvalue is an array, §2) */` |
|        - | 4319 | `	{` |
|       18 | 4320 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       18 | 4321 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4322 | `	}` |
|        - | 4323 | `	/* Perform the concatenation (Reverse order) */` |
|       18 | 4324 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       18 | 4325 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4326 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - | 4327 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 | 4328 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4329 | `			goto Abort;` |
|        - | 4330 | `		}` |
|        8 | 4331 | `	}` |
|        - | 4332 | `	/* Perform the store operation */` |
|       18 | 4333 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 4334 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       18 | 4335 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       26 | 4336 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       13 | 4337 | `		PH7_MemObjStore(pTos,pObj);` |
|        6 | 4338 | `	}` |
|       13 | 4339 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       13 | 4340 | `	PH7_MemObjStore(pTos,pNos);` |
|       13 | 4341 | `	VmPopOperand(&pTos,1);` |
|       13 | 4342 | `	break;` |
|        - | 4343 | `				}` |
|        - | 4344 | `/* OP_AND: * * *` |
|        - | 4345 | ` *` |
|        - | 4346 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - | 4347 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4348 | ` * stack.` |
|        - | 4349 | ` */` |
|        - | 4350 | `/* OP_OR: * * *` |
|        - | 4351 | ` *` |
|        - | 4352 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - | 4353 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4354 | ` * stack.` |
|        - | 4355 | ` */` |
|   172522 | 4356 | `case PH7_OP_LAND:` |
|        - | 4357 | `case PH7_OP_LOR: {` |
|        - | 4358 | `	VmOpRc rcOp;` |
|   345486 | 4359 | `	sState.pTos = pTos;` |
|   345486 | 4360 | `	sState.pc = pc;` |
|   345486 | 4361 | `	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);` |
|   345486 | 4362 | `	pTos = sState.pTos;` |
|   345486 | 4363 | `	pc = sState.pc;` |
|   345486 | 4364 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4365 | `		goto Abort;` |
|   345486 | 4366 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4367 | `		goto Exception;` |
|        - | 4368 | `	}` |
|   345486 | 4369 | `	break;` |
|        - | 4370 | `					  }` |
|        - | 4371 | `/*` |
|        - | 4372 | ` * OP_NULLC: * * *` |
|        - | 4373 | ` * Null coalescing operator '??'.` |
|        - | 4374 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - | 4375 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - | 4376 | ` */` |
|        - | 4377 | `/*` |
|        - | 4378 | ` * OP_NULLC: * P2 *` |
|        - | 4379 | ` * Short-circuit null coalescing '??'.` |
|        - | 4380 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - | 4381 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - | 4382 | ` */` |
|      606 | 4383 | `case PH7_OP_NULLC: {` |
|        - | 4384 | `#ifdef UNTRUST` |
|        - | 4385 | `	if( pTos < pStack ){` |
|        - | 4386 | `		goto Abort;` |
|        - | 4387 | `	}` |
|        - | 4388 | `#endif` |
|     1217 | 4389 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 4390 | `		/* Left is not null — keep it and skip the RHS */` |
|      935 | 4391 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|      470 | 4392 | `	}else{` |
|        - | 4393 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|      287 | 4394 | `		VmPopOperand(&pTos, 1);` |
|        - | 4395 | `	}` |
|     1217 | 4396 | `	break;` |
|        - | 4397 | `}` |
|        - | 4398 | `/*` |
|        - | 4399 | ` * OP_NULLC_JMP: * P2 *` |
|        - | 4400 | ` * Null coalescing assignment short-circuit.` |
|        - | 4401 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - | 4402 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - | 4403 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - | 4404 | ` */` |
|       82 | 4405 | `case PH7_OP_NULLC_JMP: {` |
|        - | 4406 | `#ifdef UNTRUST` |
|        - | 4407 | `	if( pTos < pStack ){` |
|        - | 4408 | `		goto Abort;` |
|        - | 4409 | `	}` |
|        - | 4410 | `#endif` |
|      167 | 4411 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       52 | 4412 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —` |
|        - | 4413 | `		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is` |
|        - | 4414 | `		 * dropped by the fetch-point sweep (the landing pc is past its` |
|        - | 4415 | `		 * OP_NULLC_STORE window): the skipped assign never dispatches. */` |
|       25 | 4416 | `	}` |
|      167 | 4417 | `	break;` |
|        - | 4418 | `}` |
|        - | 4419 | `/*` |
|        - | 4420 | ` * OP_NULLC_STORE: * * *` |
|        - | 4421 | ` * Null coalescing assignment store.` |
|        - | 4422 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - | 4423 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - | 4424 | ` * expression result.` |
|        - | 4425 | ` */` |
|        - | 4426 | `/*` |
|        - | 4427 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - | 4428 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - | 4429 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - | 4430 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - | 4431 | ` * non-null, fall through without modifying the stack so the following` |
|        - | 4432 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - | 4433 | ` */` |
|       73 | 4434 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - | 4435 | `#ifdef UNTRUST` |
|        - | 4436 | `	if( pTos < pStack ){` |
|        - | 4437 | `		goto Abort;` |
|        - | 4438 | `	}` |
|        - | 4439 | `#endif` |
|      150 | 4440 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - | 4441 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - | 4442 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       66 | 4443 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       32 | 4444 | `	}` |
|      150 | 4445 | `	break;` |
|        - | 4446 | `}` |
|       54 | 4447 | `case PH7_OP_NULLC_STORE: {` |
|        - | 4448 | `	VmOpRc rcOp;` |
|      111 | 4449 | `	sState.pTos = pTos;` |
|      111 | 4450 | `	sState.pc = pc;` |
|      111 | 4451 | `	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);` |
|      111 | 4452 | `	pTos = sState.pTos;` |
|      111 | 4453 | `	pc = sState.pc;` |
|      111 | 4454 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4455 | `		goto Abort;` |
|      111 | 4456 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 4457 | `		goto Exception;` |
|        - | 4458 | `	}` |
|      103 | 4459 | `	break;` |
|        - | 4460 | `					  }` |
|        - | 4461 | `/*` |
|        - | 4462 | ` * OP_SPREAD: * * *` |
|        - | 4463 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - | 4464 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - | 4465 | ` * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this` |
|        - | 4466 | ` * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —` |
|        - | 4467 | ` * the CALL may not be the next instruction, and may be an inner call whose own` |
|        - | 4468 | ` * spreads must stay scoped to it.` |
|        - | 4469 | ` * The expansion tail is shared between the plain-array and the materialized` |
|        - | 4470 | ` * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.` |
|        - | 4471 | ` */` |
|      306 | 4472 | `case PH7_OP_SPREAD: {` |
|        - | 4473 | `#ifdef UNTRUST` |
|        - | 4474 | `	if( pTos < pStack ){` |
|        - | 4475 | `		goto Abort;` |
|        - | 4476 | `	}` |
|        - | 4477 | `#endif` |
|        - | 4478 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - | 4479 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - | 4480 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - | 4481 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - | 4482 | `	 * be freed immediately. */` |
|      616 | 4483 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 | 4484 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - | 4485 | `		sxi32 rcW;` |
|        3 | 4486 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 | 4487 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 | 4488 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 | 4489 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 4490 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 4491 | `			goto Exception;` |
|        - | 4492 | `		}` |
|        - | 4493 | `		/* Grow the operand stack if this expansion would overflow it (no longer a` |
|        - | 4494 | `		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a` |
|        - | 4495 | `		 * single argument — the historical bounded-fallback, now only under OOM. */` |
|        4 | 4496 | `		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,` |
|        1 | 4497 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 4498 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 4499 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 4500 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 4501 | `				pTmpMap->nEntry);` |
|      ! 0 | 4502 | `			break;` |
|        - | 4503 | `		}` |
|        3 | 4504 | `		VmSpreadExpandMap(pVm, &pTos, pTmpMap, 0/*a Traversable's values are not the caller's slots*/);` |
|        3 | 4505 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 | 4506 | `		break;` |
|        - | 4507 | `	}` |
|      614 | 4508 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      614 | 4509 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      919 | 4510 | `		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,` |
|      305 | 4511 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 4512 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 4513 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 4514 | `				pMap->nEntry);` |
|      ! 0 | 4515 | `			break;` |
|        - | 4516 | `		}` |
|      614 | 4517 | `		VmSpreadExpandMap(pVm, &pTos, pMap, pInstr->iP1 != 0);` |
|      305 | 4518 | `	}` |
|        - | 4519 | `	/* else: not an array — leave as-is (single arg) */` |
|      614 | 4520 | `	break;` |
|        - | 4521 | `}` |
|        - | 4522 | `/*` |
|        - | 4523 | ` * OP_FLAG_SPREAD: * * *` |
|        - | 4524 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - | 4525 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - | 4526 | ` */` |
|      342 | 4527 | `case PH7_OP_FLAG_SPREAD: {` |
|        - | 4528 | `#ifdef UNTRUST` |
|        - | 4529 | `	if( pTos < pStack ){` |
|        - | 4530 | `		goto Abort;` |
|        - | 4531 | `	}` |
|        - | 4532 | `#endif` |
|      687 | 4533 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|      687 | 4534 | `	break;` |
|        - | 4535 | `}` |
|        - | 4536 | `/* OP_LXOR: * * *` |
|        - | 4537 | ` *` |
|        - | 4538 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - | 4539 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4540 | ` * stack.` |
|        - | 4541 | ` * According to the PHP language reference manual:` |
|        - | 4542 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - | 4543 | ` *  TRUE,but not both.` |
|        - | 4544 | ` */` |
|        6 | 4545 | `case PH7_OP_LXOR:{` |
|       13 | 4546 | `	ph7_value *pNos = &pTos[-1];` |
|       13 | 4547 | `	sxi32 v = 0;` |
|        - | 4548 | `#ifdef UNTRUST` |
|        - | 4549 | `	if( pNos < pStack ){` |
|        - | 4550 | `		goto Abort;` |
|        - | 4551 | `	}` |
|        - | 4552 | `#endif` |
|        - | 4553 | `	/* Force a boolean cast */` |
|       13 | 4554 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 4555 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 4556 | `	}` |
|       13 | 4557 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 4558 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 | 4559 | `	}` |
|       13 | 4560 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 | 4561 | `		v = 1;` |
|        3 | 4562 | `	}` |
|       13 | 4563 | `	VmPopOperand(&pTos,1);` |
|       13 | 4564 | `	pTos->x.iVal = v;` |
|       13 | 4565 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       13 | 4566 | `	break;` |
|        - | 4567 | `				 }` |
|        - | 4568 | `/* OP_EQ P1 P2 P3` |
|        - | 4569 | ` *` |
|        - | 4570 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - | 4571 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - | 4572 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4573 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4574 | ` */` |
|        - | 4575 | `/* OP_NEQ P1 P2 P3` |
|        - | 4576 | ` *` |
|        - | 4577 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - | 4578 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 4579 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4580 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4581 | ` */` |
|     6796 | 4582 | `case PH7_OP_EQ:` |
|        - | 4583 | `case PH7_OP_NEQ: {` |
|        - | 4584 | `	VmOpRc rcOp;` |
|    13591 | 4585 | `	sState.pTos = pTos;` |
|    13591 | 4586 | `	sState.pc = pc;` |
|    13591 | 4587 | `	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);` |
|    13591 | 4588 | `	pTos = sState.pTos;` |
|    13591 | 4589 | `	pc = sState.pc;` |
|    13591 | 4590 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4591 | `		goto Abort;` |
|    13591 | 4592 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4593 | `		goto Exception;` |
|        - | 4594 | `	}` |
|    13591 | 4595 | `	break;` |
|        - | 4596 | `					  }` |
|        - | 4597 | `/* OP_TEQ P1 P2 *` |
|        - | 4598 | ` *` |
|        - | 4599 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - | 4600 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 4601 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4602 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4603 | ` */` |
|   363822 | 4604 | `case PH7_OP_TEQ: {` |
|        - | 4605 | `	VmOpRc rcOp;` |
|   728883 | 4606 | `	sState.pTos = pTos;` |
|   728883 | 4607 | `	sState.pc = pc;` |
|   728883 | 4608 | `	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);` |
|   728883 | 4609 | `	pTos = sState.pTos;` |
|   728883 | 4610 | `	pc = sState.pc;` |
|   728883 | 4611 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4612 | `		goto Abort;` |
|   728883 | 4613 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4614 | `		goto Exception;` |
|        - | 4615 | `	}` |
|   728883 | 4616 | `	break;` |
|        - | 4617 | `					  }` |
|        - | 4618 | `/* OP_TNE P1 P2 *` |
|        - | 4619 | ` *` |
|        - | 4620 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - | 4621 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - | 4622 | ` * instruction.` |
|        - | 4623 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4624 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4625 | ` *` |
|        - | 4626 | ` */` |
|   320974 | 4627 | `case PH7_OP_TNE: {` |
|        - | 4628 | `	VmOpRc rcOp;` |
|   642398 | 4629 | `	sState.pTos = pTos;` |
|   642398 | 4630 | `	sState.pc = pc;` |
|   642398 | 4631 | `	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);` |
|   642398 | 4632 | `	pTos = sState.pTos;` |
|   642398 | 4633 | `	pc = sState.pc;` |
|   642398 | 4634 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4635 | `		goto Abort;` |
|   642398 | 4636 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4637 | `		goto Exception;` |
|        - | 4638 | `	}` |
|   642398 | 4639 | `	break;` |
|        - | 4640 | `					  }` |
|        - | 4641 | `/* OP_LT P1 P2 P3` |
|        - | 4642 | ` *` |
|        - | 4643 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4644 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 4645 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4646 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4647 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4648 | ` *` |
|        - | 4649 | ` */` |
|        - | 4650 | `/* OP_LE P1 P2 P3` |
|        - | 4651 | ` *` |
|        - | 4652 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4653 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 4654 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4655 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4656 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4657 | ` *` |
|        - | 4658 | ` */` |
|   286854 | 4659 | `case PH7_OP_LT:` |
|        - | 4660 | `case PH7_OP_LE: {` |
|        - | 4661 | `	VmOpRc rcOp;` |
|   574998 | 4662 | `	sState.pTos = pTos;` |
|   574998 | 4663 | `	sState.pc = pc;` |
|   574998 | 4664 | `	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);` |
|   574998 | 4665 | `	pTos = sState.pTos;` |
|   574998 | 4666 | `	pc = sState.pc;` |
|   574998 | 4667 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4668 | `		goto Abort;` |
|   574998 | 4669 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4670 | `		goto Exception;` |
|        - | 4671 | `	}` |
|   574998 | 4672 | `	break;` |
|        - | 4673 | `					  }` |
|        - | 4674 | `/* OP_GT P1 P2 P3` |
|        - | 4675 | ` *` |
|        - | 4676 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4677 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 4678 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4679 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4680 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4681 | ` *` |
|        - | 4682 | ` */` |
|        - | 4683 | `/* OP_GE P1 P2 P3` |
|        - | 4684 | ` *` |
|        - | 4685 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4686 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 4687 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4688 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4689 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4690 | ` *` |
|        - | 4691 | ` */` |
|   136047 | 4692 | `case PH7_OP_GT:` |
|        - | 4693 | `case PH7_OP_GE: {` |
|        - | 4694 | `	VmOpRc rcOp;` |
|   272549 | 4695 | `	sState.pTos = pTos;` |
|   272549 | 4696 | `	sState.pc = pc;` |
|   272549 | 4697 | `	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);` |
|   272549 | 4698 | `	pTos = sState.pTos;` |
|   272549 | 4699 | `	pc = sState.pc;` |
|   272549 | 4700 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4701 | `		goto Abort;` |
|   272549 | 4702 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4703 | `		goto Exception;` |
|        - | 4704 | `	}` |
|   272549 | 4705 | `	break;` |
|        - | 4706 | `					  }` |
|        - | 4707 | `/* OP_SPACESHIP * * *` |
|        - | 4708 | ` *` |
|        - | 4709 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - | 4710 | ` *   -1 if left < right` |
|        - | 4711 | ` *    0 if left == right` |
|        - | 4712 | ` *    1 if left > right` |
|        - | 4713 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - | 4714 | ` */` |
|      357 | 4715 | `case PH7_OP_SPACESHIP: {` |
|        - | 4716 | `	VmOpRc rcOp;` |
|      718 | 4717 | `	sState.pTos = pTos;` |
|      718 | 4718 | `	sState.pc = pc;` |
|      718 | 4719 | `	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);` |
|      718 | 4720 | `	pTos = sState.pTos;` |
|      718 | 4721 | `	pc = sState.pc;` |
|      718 | 4722 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4723 | `		goto Abort;` |
|      718 | 4724 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4725 | `		goto Exception;` |
|        - | 4726 | `	}` |
|      718 | 4727 | `	break;` |
|        - | 4728 | `					  }` |
|        - | 4729 | `/*` |
|        - | 4730 | ` * OP_LOAD_REF * * *` |
|        - | 4731 | ` * Push the index of a referenced object on the stack.` |
|        - | 4732 | ` */` |
|       82 | 4733 | `case PH7_OP_LOAD_REF: {` |
|        - | 4734 | `	sxu32 nIdx;` |
|        - | 4735 | `#ifdef UNTRUST` |
|        - | 4736 | `	if( pTos < pStack ){` |
|        - | 4737 | `		goto Abort;` |
|        - | 4738 | `	}` |
|        - | 4739 | `#endif` |
|      166 | 4740 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|        - | 4741 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|        - | 4742 | `		 * out of a string still carries the BASE VARIABLE's slot index, so taking a` |
|        - | 4743 | `` 		 * reference to it aliased the WHOLE STRING — `$x = [&$s[1]]; $x[0] = "Z";` `` |
|        - | 4744 | ``		 * left $s === "Z". Same Error the `=&` and by-ref-argument paths raise. */`` |
|        3 | 4745 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|        - | 4746 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|        3 | 4747 | `		PH7_MemObjRelease(pTos);` |
|        3 | 4748 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 4749 | `		pTos->nIdx = SXU32_HIGH;` |
|        3 | 4750 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|        3 | 4751 | `		break;` |
|        - | 4752 | `	}` |
|        - | 4753 | `	/* Extract memory object index */` |
|      163 | 4754 | `	nIdx = pTos->nIdx;` |
|      163 | 4755 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - | 4756 | `		/* Nullify the object */` |
|      163 | 4757 | `		PH7_MemObjRelease(pTos);` |
|        - | 4758 | `		/* Mark as constant and store the index on the top of the stack */` |
|      163 | 4759 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      163 | 4760 | `		pTos->nIdx = SXU32_HIGH;` |
|      163 | 4761 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       81 | 4762 | `	}` |
|      163 | 4763 | `	break;` |
|        - | 4764 | `					  }` |
|        - | 4765 | `/*` |
|        - | 4766 | ` * OP_STORE_REF * * P3` |
|        - | 4767 | ` * Perform an assignment operation by reference.` |
|        - | 4768 | ` */` |
|     1607 | 4769 | `case PH7_OP_STORE_REF: {` |
|        - | 4770 | `	VmOpRc rcOp;` |
|     3218 | 4771 | `	sState.pTos = pTos;` |
|     3218 | 4772 | `	sState.pc = pc;` |
|     3218 | 4773 | `	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);` |
|     3218 | 4774 | `	pTos = sState.pTos;` |
|     3218 | 4775 | `	pc = sState.pc;` |
|     3218 | 4776 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 4777 | `		goto Abort;` |
|     3216 | 4778 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 4779 | `		goto Exception;` |
|        - | 4780 | `	}` |
|     3208 | 4781 | `	break;` |
|        - | 4782 | `					  }` |
|        - | 4783 | `/*` |
|        - | 4784 | ` * OP_UPLINK P1 * *` |
|        - | 4785 | ` * Link a variable to the top active VM frame.` |
|        - | 4786 | ` * This is used to implement the 'global' PHP construct.` |
|        - | 4787 | ` */` |
|      152 | 4788 | `case PH7_OP_UPLINK: {` |
|      309 | 4789 | `	if( pVm->pFrame->pParent ){` |
|      309 | 4790 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - | 4791 | `		SyString sName;` |
|        - | 4792 | `		/* rcSv carries the coercion status OUT of the loop: the routing macro is a` |
|        - | 4793 | ``		 * bare `break` here, which would only leave the while and then pop the`` |
|        - | 4794 | `		 * operands with a throw pending. */` |
|      309 | 4795 | `		sxi32 rcSv = SXRET_OK;` |
|        - | 4796 | `		/* Perform the link */` |
|      625 | 4797 | `		while( pLink <= pTos ){` |
|        - | 4798 | `			/* Force a string cast — global $$arr link name (user-visible, §2) */` |
|      327 | 4799 | `			rcSv = PH7_MemObjToStringUV(pLink);` |
|      327 | 4800 | `			if( rcSv != SXRET_OK ){` |
|        7 | 4801 | `				break;` |
|        - | 4802 | `			}` |
|      321 | 4803 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|      321 | 4804 | `			if( sName.nByte > 0 ){` |
|      321 | 4805 | `				VmFrameLink(&(*pVm),&sName);` |
|      158 | 4806 | `			}` |
|      321 | 4807 | `			pLink++;` |
|        5 | 4808 | `		}` |
|      309 | 4809 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|      149 | 4810 | `	}` |
|      303 | 4811 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|      303 | 4812 | `	break;` |
|        - | 4813 | `					}` |
|        - | 4814 | `/*` |
|        - | 4815 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - | 4816 | ` * Push an exception in the corresponding container so that` |
|        - | 4817 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - | 4818 | ` */` |
|   730804 | 4819 | `case PH7_OP_LOAD_EXCEPTION: {` |
|        - | 4820 | `	VmOpRc rcOp;` |
|  1461613 | 4821 | `	sState.pTos = pTos;` |
|  1461613 | 4822 | `	sState.pc = pc;` |
|  1461613 | 4823 | `	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);` |
|  1461613 | 4824 | `	pTos = sState.pTos;` |
|  1461613 | 4825 | `	pc = sState.pc;` |
|  1461613 | 4826 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4827 | `		goto Abort;` |
|  1461613 | 4828 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4829 | `		goto Exception;` |
|        - | 4830 | `	}` |
|  1461613 | 4831 | `	break;` |
|        - | 4832 | `					  }` |
|        - | 4833 | `/*` |
|        - | 4834 | ` * OP_POP_EXCEPTION * * P3` |
|        - | 4835 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - | 4836 | ` */` |
|   680489 | 4837 | `case PH7_OP_POP_EXCEPTION: {` |
|  1360983 | 4838 | `	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;` |
|        - | 4839 | `	VmFrame *pBodyFrame;` |
|        - | 4840 | `	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on` |
|        - | 4841 | `	 * top (matched by compiled origin). pException == NULL means this try's` |
|        - | 4842 | `	 * activation was already consumed (an in-place catch handled a throw and` |
|        - | 4843 | `	 * ran the finally itself); compiled fields keep coming from p3. */` |
|  1360983 | 4844 | `	ph7_exception *pException = 0;` |
|  1360983 | 4845 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|     4730 | 4846 | `		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|     4730 | 4847 | `		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];` |
|        - | 4848 | `		/* Same compiled origin is NOT enough: under recursion, when THIS` |
|        - | 4849 | `		 * level's activation was already consumed by an in-place catch (the` |
|        - | 4850 | `		 * resume lands right here), the top can be an OUTER level's activation` |
|        - | 4851 | `		 * of the same lexical try — popping it would run that level's finally` |
|        - | 4852 | `		 * early and orphan its handler (probed: recursive try/catch/finally` |
|        - | 4853 | `		 * lost the outer catch entirely). The activation must also belong to` |
|        - | 4854 | `		 * the CURRENT body frame. */` |
|     4725 | 4855 | `		if( VmExcMatches(pTop,pCompiledExc)` |
|     4672 | 4856 | `		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){` |
|     4608 | 4857 | `			pException = pTop;` |
|     4608 | 4858 | `			(void)SySetPop(&pVm->aException);` |
|     2302 | 4859 | `		}` |
|     2363 | 4860 | `	}` |
|  1360983 | 4861 | `	if( pCompiledExc->iInlined ){` |
|        - | 4862 | `		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.` |
|        - | 4863 | `		 * Pop this try's handler off aException so a throw in the finally propagates to` |
|        - | 4864 | `		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the` |
|        - | 4865 | `		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the` |
|        - | 4866 | `		 * compiler-emitted JMP falls into the finally. Without a finally, this is the` |
|        - | 4867 | `		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */` |
|      167 | 4868 | `		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */` |
|      167 | 4869 | `		if( pCompiledExc->iHasFinally ){` |
|        - | 4870 | `			VmFinallyAction sAct;` |
|       14 | 4871 | `			SyZero(&sAct,sizeof(sAct));` |
|       14 | 4872 | `			sAct.eKind = PH7_FA_FALLTHROUGH;` |
|       14 | 4873 | `			sAct.iNextPc = pCompiledExc->iEndCatchPc;` |
|       14 | 4874 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        - | 4875 | `			/* keep the transparent frame for OP_END_FINALLY */` |
|      161 | 4876 | `		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       20 | 4877 | `			VmLeaveFrame(&(*pVm));` |
|        8 | 4878 | `		}` |
|      167 | 4879 | `		break;` |
|        - | 4880 | `	}` |
|        - | 4881 | `	/* Leave the exception frame. It is normally on top here (a try that fell through` |
|        - | 4882 | `	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a` |
|        - | 4883 | `	 * RESUMED generator/fiber body whose yield was inside this try, that exception` |
|        - | 4884 | `	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),` |
|        - | 4885 | `	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the` |
|        - | 4886 | `	 * coroutine (and defeat the bHasRet materialization just below, which must see the` |
|        - | 4887 | `	 * body). Only leave a genuine exception frame. */` |
|  1360821 | 4888 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|  1152983 | 4889 | `		VmLeaveFrame(&(*pVm));` |
|   576489 | 4890 | `	}` |
|        - | 4891 | `	/* Execute the finally block if present and not already executed by the` |
|        - | 4892 | `	 * catch path. No live activation (pException == NULL) means an in-place` |
|        - | 4893 | `	 * catch consumed it — and that path runs the finally itself — so skip. */` |
|  1360821 | 4894 | `	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){` |
|        - | 4895 | `		sxi32 rcFinally;` |
|       57 | 4896 | `		VmExcRelease(&(*pVm),pException);` |
|       57 | 4897 | `		pException = 0;` |
|       57 | 4898 | `		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);` |
|       57 | 4899 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 | 4900 | `			goto Abort;` |
|        - | 4901 | `		}` |
|       57 | 4902 | `		if( rcFinally == PH7_EXCEPTION ){` |
|        - | 4903 | `			/* The finally threw past itself. If an enclosing try IN THIS function` |
|        - | 4904 | `			 * caught the new exception in place, resume at its landing pad;` |
|        - | 4905 | `			 * otherwise it was caught at an outer frame, so unwind this function. */` |
|        - | 4906 | `			sxi32 iResumePc;` |
|        5 | 4907 | `			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 4908 | `				pc = iResumePc;` |
|        3 | 4909 | `				break;` |
|        - | 4910 | `			}` |
|        3 | 4911 | `			goto Exception;` |
|        - | 4912 | `		}` |
|       24 | 4913 | `	}` |
|  1360817 | 4914 | `	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */` |
|  1360817 | 4915 | `	pBodyFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  1360817 | 4916 | `	if( pBodyFrame->bHasRet ){` |
|        - | 4917 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - | 4918 | `		 * function. The return targets the body frame this try belongs to. Drain` |
|        - | 4919 | `		 * outer finally blocks first, then — only in the real function body` |
|        - | 4920 | `		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline` |
|        - | 4921 | `		 * try within a catch/finally) propagate outward so the owning body returns. */` |
|    20415 | 4922 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    20415 | 4923 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4924 | `			goto Abort;` |
|        - | 4925 | `		}` |
|    20415 | 4926 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 4927 | `			goto Exception;` |
|        - | 4928 | `		}` |
|    20415 | 4929 | `		if( !sState.bReturnPropagates ){` |
|    20409 | 4930 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|    10202 | 4931 | `		}` |
|    20415 | 4932 | `		goto Done;` |
|        - | 4933 | `	}` |
|  1340407 | 4934 | `	if( pBodyFrame->nCatchJmpPc > 0 ){` |
|        - | 4935 | ``		/* A `break`/`continue` inside the catch (OP_CATCH_JMP) parked its loop-exit`` |
|        - | 4936 | `		 * target on this body frame, and this is a landing pad on its way out. */` |
|       88 | 4937 | `		if( pBodyFrame->nCatchJmpLevels > 1 ){` |
|        - | 4938 | `			/* Still one or more detached bodies out from the target's array — this try` |
|        - | 4939 | `			 * was declared INSIDE another catch/finally mini-program. End this one and` |
|        - | 4940 | `			 * let the park travel outward; the next landing pad decrements again. */` |
|        6 | 4941 | `			pBodyFrame->nCatchJmpLevels--;` |
|        6 | 4942 | `			goto Done;` |
|        - | 4943 | `		}` |
|       84 | 4944 | `		if( pBodyFrame->nCatchJmpCross > 0 ){` |
|        - | 4945 | `			/* Enclosing trys between the catch and the loop: the jump lands past their` |
|        - | 4946 | `			 * OP_POP_EXCEPTION, so run their finally (and leave their frames) here. */` |
|        8 | 4947 | `			sxu32 nCross = pBodyFrame->nCatchJmpCross;` |
|        8 | 4948 | `			pBodyFrame->nCatchJmpCross = 0;` |
|        8 | 4949 | `			rc = VmDrainCrossedTrys(&(*pVm),nCross,sState.nExceptionBase);` |
|        8 | 4950 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4951 | `				goto Abort;` |
|        - | 4952 | `			}` |
|        8 | 4953 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 4954 | `				/* A drained finally threw past itself — it supersedes the loop exit. */` |
|      ! 0 | 4955 | `				pBodyFrame->nCatchJmpPc = 0;` |
|      ! 0 | 4956 | `				goto Exception;` |
|        - | 4957 | `			}` |
|        3 | 4958 | `		}` |
|       84 | 4959 | `		pc = (sxi32)pBodyFrame->nCatchJmpPc - 1;` |
|       84 | 4960 | `		pBodyFrame->nCatchJmpPc = 0;` |
|       84 | 4961 | `		break;` |
|        - | 4962 | `	}` |
|  1340323 | 4963 | `	break;` |
|        - | 4964 | `							}` |
|        - | 4965 | `/*` |
|        - | 4966 | ` * OP_CATCH_JMP P1(levels,cross) P2(target pc) *` |
|        - | 4967 | ` * Jump to iP2, leaving the try/catch structures iP1 describes (PH7_CATCH_JMP_P1).` |
|        - | 4968 | ` *` |
|        - | 4969 | ` * CROSS enclosing trys are being left without reaching their OP_POP_EXCEPTION, so this` |
|        - | 4970 | ` * runs their finallys itself. LEVELS says whether the target is even addressable from` |
|        - | 4971 | `` * here: 0 means it is in this same array (a `goto` out of a try body) and the jump is`` |
|        - | 4972 | ` * taken on the spot; above 0 the jump starts inside a DETACHED catch/finally` |
|        - | 4973 | ` * mini-program whose array is not the target's, so the target is PARKED on the owning` |
|        - | 4974 | `` * body's frame and this mini-program ends — mirroring how an explicit `return` in a`` |
|        - | 4975 | ` * catch parks on sRet at OP_DONE above. VmThrowException then runs this try's finally` |
|        - | 4976 | ` * and returns; the resume lands on the try's OP_POP_EXCEPTION, which decrements LEVELS` |
|        - | 4977 | ` * and either re-parks (another mini-program out) or takes the jump.` |
|        - | 4978 | ` */` |
|       47 | 4979 | `case PH7_OP_CATCH_JMP: {` |
|        - | 4980 | `	VmFrame *pTgt;` |
|       98 | 4981 | `	if( PH7_CATCH_JMP_LEVELS(pInstr->iP1) == 0 ){` |
|        - | 4982 | ``		/* No detached body to leave — a `goto` whose target is in THIS array, but which`` |
|        - | 4983 | `		 * jumps out of one or more enclosing trys. Nothing to park: run their finallys` |
|        - | 4984 | `		 * (a bare jump would leave them to fire at teardown) and go. */` |
|       14 | 4985 | `		rc = VmDrainCrossedTrys(&(*pVm),PH7_CATCH_JMP_CROSS(pInstr->iP1),sState.nExceptionBase);` |
|       14 | 4986 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4987 | `			goto Abort;` |
|        - | 4988 | `		}` |
|       14 | 4989 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 4990 | `			/* A drained finally threw past itself — it supersedes the jump. */` |
|      ! 0 | 4991 | `			goto Exception;` |
|        - | 4992 | `		}` |
|       14 | 4993 | `		pc = (sxi32)pInstr->iP2 - 1;` |
|       14 | 4994 | `		break;` |
|        - | 4995 | `	}` |
|       86 | 4996 | `	pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|       86 | 4997 | `	pTgt->nCatchJmpPc = (sxu32)pInstr->iP2;` |
|       86 | 4998 | `	pTgt->nCatchJmpLevels = PH7_CATCH_JMP_LEVELS(pInstr->iP1);` |
|       86 | 4999 | `	pTgt->nCatchJmpCross = PH7_CATCH_JMP_CROSS(pInstr->iP1);` |
|        - | 5000 | `	/* A try opened INSIDE this catch body that the jump crosses had its finally run by` |
|        - | 5001 | `	 * the compiler-emitted OP_POP_EXCEPTION; drain anything still pending here. */` |
|       86 | 5002 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|       86 | 5003 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 5004 | `		goto Abort;` |
|        - | 5005 | `	}` |
|       86 | 5006 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 5007 | `		/* A drained finally threw past itself — it discards this jump. */` |
|      ! 0 | 5008 | `		pTgt->nCatchJmpPc = 0;` |
|      ! 0 | 5009 | `		goto Exception;` |
|        - | 5010 | `	}` |
|       86 | 5011 | `	goto Done;` |
|        - | 5012 | `					   }` |
|        - | 5013 | `/*` |
|        - | 5014 | ` * OP_CATCH iP1(catch-index) * P3(ph7_exception)` |
|        - | 5015 | ` * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on` |
|        - | 5016 | ` * pException->pInflight by VmThrowInline) into the catch variable, resolved in the` |
|        - | 5017 | ` * enclosing body's scope (PHP: a catch shares the surrounding variable scope).` |
|        - | 5018 | ` */` |
|       38 | 5019 | `case PH7_OP_CATCH: {` |
|        - | 5020 | `	VmOpRc rcOp;` |
|       81 | 5021 | `	sState.pTos = pTos;` |
|       81 | 5022 | `	sState.pc = pc;` |
|       81 | 5023 | `	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);` |
|       81 | 5024 | `	pTos = sState.pTos;` |
|       81 | 5025 | `	pc = sState.pc;` |
|       81 | 5026 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5027 | `		goto Abort;` |
|       81 | 5028 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5029 | `		goto Exception;` |
|        - | 5030 | `	}` |
|       81 | 5031 | `	break;` |
|        - | 5032 | `					  }` |
|        - | 5033 | `/*` |
|        - | 5034 | ` * OP_END_FINALLY * P3(ph7_exception)` |
|        - | 5035 | ` * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch` |
|        - | 5036 | ` * the pending action queued when the finally was entered (fall-through / re-throw /` |
|        - | 5037 | ` * return / break-continue). A return/break threads out through each enclosing finally` |
|        - | 5038 | ` * via pException->iNextFinallyPc.` |
|        - | 5039 | ` */` |
|       24 | 5040 | `case PH7_OP_END_FINALLY: {` |
|       52 | 5041 | `	ph7_exception *pExc = (ph7_exception *)pInstr->p3;` |
|        - | 5042 | `	VmFinallyAction sAct;` |
|       52 | 5043 | `	int eKind = PH7_FA_FALLTHROUGH;` |
|        - | 5044 | `	/* Leave the try's transparent frame kept alive across the finally. */` |
|       52 | 5045 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        3 | 5046 | `		VmLeaveFrame(&(*pVm));` |
|        1 | 5047 | `	}` |
|       52 | 5048 | `	if( SySetUsed(&pVm->aFinallyAction) > 0 ){` |
|       52 | 5049 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);` |
|       52 | 5050 | `		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];` |
|       52 | 5051 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|       52 | 5052 | `		eKind = sAct.eKind;` |
|       28 | 5053 | `	}else{` |
|      ! 0 | 5054 | `		SyZero(&sAct,sizeof(sAct));` |
|      ! 0 | 5055 | `		sAct.iNextPc = pExc->iEndCatchPc;` |
|        - | 5056 | `	}` |
|       52 | 5057 | `	if( eKind == PH7_FA_FALLTHROUGH ){` |
|       12 | 5058 | `		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;` |
|       16 | 5059 | `		break;` |
|       42 | 5060 | `	}else if( eKind == PH7_FA_JMP ){` |
|        - | 5061 | `		/* break/continue: run the remaining crossed finallys, then take the jump. */` |
|        5 | 5062 | `		sxu32 iFpc = 0;` |
|        5 | 5063 | `		int nCross = sAct.nCross;` |
|        5 | 5064 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|      ! 0 | 5065 | `			sAct.nCross = nCross;` |
|      ! 0 | 5066 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      ! 0 | 5067 | `			pc = (sxi32)iFpc - 1;` |
|      ! 0 | 5068 | `			break;` |
|        - | 5069 | `		}` |
|        5 | 5070 | `		pc = (sxi32)sAct.iNextPc - 1;` |
|        5 | 5071 | `		break;` |
|       38 | 5072 | `	}else if( eKind == PH7_FA_RETHROW ){` |
|        8 | 5073 | `		ph7_class_instance *pRe = sAct.pExc;` |
|        - | 5074 | `		sxi32 _iRpE;` |
|        8 | 5075 | `		rc = VmThrowException(&(*pVm),pRe);` |
|        8 | 5076 | `		if( pRe ){ PH7_ClassInstanceUnref(pRe); }` |
|        8 | 5077 | `		if( rc == SXERR_ABORT ){ goto Abort; }` |
|        8 | 5078 | `		PH7_INLINE_RESUME_BREAK()` |
|        8 | 5079 | `		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }` |
|        8 | 5080 | `		goto Exception;` |
|      ! 0 | 5081 | `	}else{ /* PH7_FA_RETURN */` |
|       31 | 5082 | `		sxu32 iFpc = 0;` |
|       31 | 5083 | `		int nCross = sAct.nCross;` |
|       31 | 5084 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        - | 5085 | `			/* Thread the return through the next enclosing finally. */` |
|        6 | 5086 | `			sAct.nCross = nCross;` |
|        6 | 5087 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        6 | 5088 | `			pc = (sxi32)iFpc - 1;` |
|        6 | 5089 | `			break;` |
|        - | 5090 | `		}` |
|        - | 5091 | `		/* No enclosing finally left: materialize the return from this body. */` |
|       27 | 5092 | `		if( sAct.bHasRetVal && sState.pResult ){` |
|        6 | 5093 | `			PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|        2 | 5094 | `		}` |
|       27 | 5095 | `		PH7_MemObjRelease(&sAct.sRet);` |
|       27 | 5096 | `		goto Done;` |
|        - | 5097 | `	}` |
|        - | 5098 | `						 }` |
|        - | 5099 | `/*` |
|        - | 5100 | ` * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)` |
|        - | 5101 | `` * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a`` |
|        - | 5102 | ` * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);` |
|        - | 5103 | ` * OP_END_FINALLY threads it out through the finally chain and then returns.` |
|        - | 5104 | ` */` |
|       22 | 5105 | `case PH7_OP_SET_FINALLY_RET: {` |
|        - | 5106 | `	VmFinallyAction sAct;` |
|       49 | 5107 | `	sxu32 iFpc = 0;` |
|       49 | 5108 | `	int nCross = -1; /* a return crosses every enclosing finally in this function */` |
|       49 | 5109 | `	SyZero(&sAct,sizeof(sAct));` |
|       49 | 5110 | `	sAct.eKind = PH7_FA_RETURN;` |
|       49 | 5111 | `	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       49 | 5112 | `	PH7_MemObjInit(pVm,&sAct.sRet);` |
|       49 | 5113 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|       37 | 5114 | `		PH7_MemObjStore(pTos,&sAct.sRet);` |
|       37 | 5115 | `		sAct.bHasRetVal = 1;` |
|       37 | 5116 | `		VmPopOperand(&pTos,1);` |
|       16 | 5117 | `	}` |
|       49 | 5118 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        9 | 5119 | `		sAct.nCross = nCross;` |
|        9 | 5120 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        9 | 5121 | `		pc = (sxi32)iFpc - 1;` |
|        9 | 5122 | `		break;` |
|        - | 5123 | `	}` |
|        - | 5124 | `	/* No enclosing finally left: return now. */` |
|       43 | 5125 | `	if( sAct.bHasRetVal && sState.pResult ){` |
|       33 | 5126 | `		PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|       14 | 5127 | `	}` |
|       43 | 5128 | `	PH7_MemObjRelease(&sAct.sRet);` |
|       43 | 5129 | `	goto Done;` |
|        - | 5130 | `						 }` |
|        - | 5131 | `/*` |
|        - | 5132 | ` * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)` |
|        - | 5133 | `` * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action`` |
|        - | 5134 | ` * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.` |
|        - | 5135 | ` */` |
|        4 | 5136 | `case PH7_OP_SET_FINALLY_JMP: {` |
|        - | 5137 | `	VmFinallyAction sAct;` |
|       11 | 5138 | `	sxu32 iFpc = 0;` |
|       11 | 5139 | `	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */` |
|       11 | 5140 | `	SyZero(&sAct,sizeof(sAct));` |
|       11 | 5141 | `	sAct.eKind = PH7_FA_JMP;` |
|       11 | 5142 | `	sAct.iNextPc = pInstr->iP2;` |
|       11 | 5143 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        5 | 5144 | `		sAct.nCross = nCross;` |
|        5 | 5145 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        5 | 5146 | `		pc = (sxi32)iFpc - 1;` |
|        5 | 5147 | `		break;` |
|        - | 5148 | `	}` |
|        - | 5149 | `	/* No finally among the crossed trys: just take the break/continue jump. */` |
|        6 | 5150 | `	pc = (sxi32)sAct.iNextPc - 1;` |
|        6 | 5151 | `	break;` |
|        - | 5152 | `						 }` |
|        - | 5153 | `/*` |
|        - | 5154 | ` * OP_THROW * P2 *` |
|        - | 5155 | ` * Throw an user exception.` |
|        - | 5156 | ` */` |
|   500429 | 5157 | `case PH7_OP_THROW: {` |
|        - | 5158 | `	VmOpRc rcOp;` |
|  1000863 | 5159 | `	sState.pTos = pTos;` |
|  1000863 | 5160 | `	sState.pc = pc;` |
|  1000863 | 5161 | `	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);` |
|  1000863 | 5162 | `	pTos = sState.pTos;` |
|  1000863 | 5163 | `	pc = sState.pc;` |
|  1000863 | 5164 | `	if( rcOp == VM_OP_ABORT ){` |
|       39 | 5165 | `		goto Abort;` |
|  1000829 | 5166 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|   600381 | 5167 | `		goto Exception;` |
|        - | 5168 | `	}` |
|   400453 | 5169 | `	break;` |
|        - | 5170 | `					  }` |
|        - | 5171 | `/*` |
|        - | 5172 | ` * OP_FOREACH_INIT * P2 P3` |
|        - | 5173 | ` * Prepare a foreach step.` |
|        - | 5174 | ` */` |
|    16077 | 5175 | `case PH7_OP_FOREACH_INIT: {` |
|        - | 5176 | `	VmOpRc rcOp;` |
|    32159 | 5177 | `	sState.pTos = pTos;` |
|    32159 | 5178 | `	sState.pc = pc;` |
|    32159 | 5179 | `	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);` |
|    32159 | 5180 | `	pTos = sState.pTos;` |
|    32159 | 5181 | `	pc = sState.pc;` |
|    32159 | 5182 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5183 | `		goto Abort;` |
|    32159 | 5184 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 5185 | `		goto Exception;` |
|        - | 5186 | `	}` |
|    32155 | 5187 | `	break;` |
|        - | 5188 | `					  }` |
|        - | 5189 | `/*` |
|        - | 5190 | ` * OP_FOREACH_STEP * P2 P3` |
|        - | 5191 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - | 5192 | ` */` |
|   206828 | 5193 | `case PH7_OP_FOREACH_STEP: {` |
|        - | 5194 | `	VmOpRc rcOp;` |
|   413669 | 5195 | `	sState.pTos = pTos;` |
|   413669 | 5196 | `	sState.pc = pc;` |
|   413669 | 5197 | `	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);` |
|   413669 | 5198 | `	pTos = sState.pTos;` |
|   413669 | 5199 | `	pc = sState.pc;` |
|   413669 | 5200 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 5201 | `		goto Abort;` |
|   413667 | 5202 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5203 | `		goto Exception;` |
|        - | 5204 | `	}` |
|   413667 | 5205 | `	break;` |
|        - | 5206 | `						  }` |
|        - | 5207 | `/*` |
|        - | 5208 | ` * OP_MEMBER P1 P2` |
|        - | 5209 | ` * Load class attribute/method on the stack.` |
|        - | 5210 | ` */` |
|   172998 | 5211 | `case PH7_OP_MEMBER: {` |
|        - | 5212 | `	VmOpRc rcOp;` |
|   346004 | 5213 | `	sState.pTos = pTos;` |
|   346004 | 5214 | `	sState.pc = pc;` |
|   346004 | 5215 | `	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);` |
|   346004 | 5216 | `	pTos = sState.pTos;` |
|   346004 | 5217 | `	pc = sState.pc;` |
|   346004 | 5218 | `	if( rcOp == VM_OP_ABORT ){` |
|        8 | 5219 | `		goto Abort;` |
|   345998 | 5220 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      169 | 5221 | `		goto Exception;` |
|        - | 5222 | `	}` |
|   345832 | 5223 | `	break;` |
|        - | 5224 | `					  }` |
|        - | 5225 | `/*` |
|        - | 5226 | ` * OP_NEW P1 * * *` |
|        - | 5227 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - | 5228 | ` */` |
|  1058607 | 5229 | `case PH7_OP_NEW: {` |
|        - | 5230 | `	VmOpRc rcOp;` |
|  2117219 | 5231 | `	sState.pTos = pTos;` |
|  2117219 | 5232 | `	sState.pc = pc;` |
|  2117219 | 5233 | `	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);` |
|  2117219 | 5234 | `	pTos = sState.pTos;` |
|  2117219 | 5235 | `	pc = sState.pc;` |
|  2117219 | 5236 | `	if( rcOp == VM_OP_ABORT ){` |
|        5 | 5237 | `		goto Abort;` |
|  2117215 | 5238 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      216 | 5239 | `		goto Exception;` |
|        - | 5240 | `	}` |
|  2117003 | 5241 | `	break;` |
|        - | 5242 | `					  }` |
|        - | 5243 | `/*` |
|        - | 5244 | ` * OP_CLONE * * *` |
|        - | 5245 | ` * Perfome a clone operation.` |
|        - | 5246 | ` */` |
|      152 | 5247 | `case PH7_OP_CLONE: {` |
|        - | 5248 | `	VmOpRc rcOp;` |
|      309 | 5249 | `	sState.pTos = pTos;` |
|      309 | 5250 | `	sState.pc = pc;` |
|      309 | 5251 | `	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);` |
|      309 | 5252 | `	pTos = sState.pTos;` |
|      309 | 5253 | `	pc = sState.pc;` |
|      309 | 5254 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5255 | `		goto Abort;` |
|      309 | 5256 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       21 | 5257 | `		goto Exception;` |
|        - | 5258 | `	}` |
|      289 | 5259 | `	break;` |
|        - | 5260 | `					  }` |
|        - | 5261 | `/*` |
|        - | 5262 | ` * OP_SWITCH * * P3` |
|        - | 5263 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - | 5264 | ` */` |
|       64 | 5265 | `case PH7_OP_SWITCH: {` |
|        - | 5266 | `	VmOpRc rcOp;` |
|      133 | 5267 | `	sState.pTos = pTos;` |
|      133 | 5268 | `	sState.pc = pc;` |
|      133 | 5269 | `	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);` |
|      133 | 5270 | `	pTos = sState.pTos;` |
|      133 | 5271 | `	pc = sState.pc;` |
|      133 | 5272 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5273 | `		goto Abort;` |
|      133 | 5274 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5275 | `		goto Exception;` |
|        - | 5276 | `	}` |
|      133 | 5277 | `	break;` |
|        - | 5278 | `					  }` |
|        - | 5279 | `/*` |
|        - | 5280 | ` * OP_MATCH * * P3` |
|        - | 5281 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - | 5282 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - | 5283 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - | 5284 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - | 5285 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - | 5286 | ` */` |
|       86 | 5287 | `case PH7_OP_MATCH: {` |
|        - | 5288 | `	VmOpRc rcOp;` |
|      177 | 5289 | `	sState.pTos = pTos;` |
|      177 | 5290 | `	sState.pc = pc;` |
|      177 | 5291 | `	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);` |
|      177 | 5292 | `	pTos = sState.pTos;` |
|      177 | 5293 | `	pc = sState.pc;` |
|      177 | 5294 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5295 | `		goto Abort;` |
|      177 | 5296 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        6 | 5297 | `		goto Exception;` |
|        - | 5298 | `	}` |
|      173 | 5299 | `	break;` |
|        - | 5300 | `					  }` |
|        - | 5301 | `/*` |
|        - | 5302 | ` * OP_YIELD P1 P2 *` |
|        - | 5303 | ` *  Yield a value from a generator function.` |
|        - | 5304 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - | 5305 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - | 5306 | ` */` |
|      595 | 5307 | `case PH7_OP_YIELD: {` |
|        - | 5308 | `	ph7_generator *pGen;` |
|     1195 | 5309 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5310 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 | 5311 | `		goto Abort;` |
|        - | 5312 | `	}` |
|     1195 | 5313 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5314 | ``		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's`` |
|        - | 5315 | `		 * pending finallys tried to yield — PHP forbids it. */` |
|      ! 0 | 5316 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5317 | `			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5318 | `			goto Abort;` |
|        - | 5319 | `		}` |
|      ! 0 | 5320 | `		goto Exception;` |
|        - | 5321 | `	}` |
|     1195 | 5322 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|     1195 | 5323 | `	if( pInstr->iP2 ){` |
|        - | 5324 | `		/* yield $key => $value: value on top, key below */` |
|        - | 5325 | `#ifdef UNTRUST` |
|        - | 5326 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - | 5327 | `#endif` |
|       20 | 5328 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       20 | 5329 | `		VmPopOperand(&pTos, 1);` |
|       20 | 5330 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       20 | 5331 | `		VmPopOperand(&pTos, 1);` |
|        - | 5332 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       20 | 5333 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 | 5334 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 | 5335 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 | 5336 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 | 5337 | `			}` |
|        2 | 5338 | `		}` |
|     1186 | 5339 | `	}else if( pInstr->iP1 ){` |
|        - | 5340 | `		/* yield $value */` |
|        - | 5341 | `#ifdef UNTRUST` |
|        - | 5342 | `		if( pTos < pStack ) goto Abort;` |
|        - | 5343 | `#endif` |
|     1177 | 5344 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|     1177 | 5345 | `		VmPopOperand(&pTos, 1);` |
|        - | 5346 | `		/* Auto-increment key */` |
|     1177 | 5347 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|     1177 | 5348 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|     1177 | 5349 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|      591 | 5350 | `	}else{` |
|        - | 5351 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 | 5352 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 5353 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 5354 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 | 5355 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - | 5356 | `	}` |
|        - | 5357 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|     1195 | 5358 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|     1195 | 5359 | `	goto Suspend;` |
|        - | 5360 | `}` |
|        - | 5361 | `/*` |
|        - | 5362 | ` * OP_YIELD_FROM * * *` |
|        - | 5363 | ` *` |
|        - | 5364 | `` * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an`` |
|        - | 5365 | ` * array/Traversable/Generator from the OUTER generator, preserving the inner` |
|        - | 5366 | ` * keys; the expression evaluates to the inner Generator's return value (or NULL).` |
|        - | 5367 | ` *` |
|        - | 5368 | ` * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,` |
|        - | 5369 | ` * advances the per-instance delegate cursor stored on the exec context (never the` |
|        - | 5370 | ` * shared foreach aStep, so independent generator instances cannot clash). The` |
|        - | 5371 | ` * iterable operand is consumed on first entry; the expression result is pushed at` |
|        - | 5372 | ` * exhaustion — net stack effect +1, identical to OP_YIELD.` |
|        - | 5373 | ` */` |
|       93 | 5374 | `case PH7_OP_YIELD_FROM: {` |
|        - | 5375 | `	ph7_generator *pGenFrom;` |
|        - | 5376 | `	ph7_exec_ctx *pCtxFrom;` |
|        - | 5377 | `	ph7_value sKey,sVal;` |
|      191 | 5378 | `	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */` |
|      191 | 5379 | `	int bExhausted = 0;` |
|      191 | 5380 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5381 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");` |
|      ! 0 | 5382 | `		goto Abort;` |
|        - | 5383 | `	}` |
|      191 | 5384 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5385 | ``		/* A `yield from` reached while VmCloseCtx force-drives this destroyed`` |
|        - | 5386 | `		 * generator's pending finallys — PHP forbids it (distinct message from a` |
|        - | 5387 | ``		 * bare `yield`, mirroring the OP_YIELD guard above). */`` |
|      ! 0 | 5388 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5389 | `			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5390 | `			goto Abort;` |
|        - | 5391 | `		}` |
|      ! 0 | 5392 | `		goto Exception;` |
|        - | 5393 | `	}` |
|      191 | 5394 | `	pCtxFrom = pVm->pActiveCtx;` |
|      191 | 5395 | `	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;` |
|      191 | 5396 | `	PH7_MemObjInit(pVm,&sKey);` |
|      191 | 5397 | `	PH7_MemObjInit(pVm,&sVal);` |
|      191 | 5398 | `	if( pCtxFrom->iDelegateState == 0 ){` |
|        - | 5399 | `		/* First entry: classify the iterable on the stack top. */` |
|       79 | 5400 | `		int bIterable = 1;` |
|        - | 5401 | `#ifdef UNTRUST` |
|        - | 5402 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 5403 | `#endif` |
|       79 | 5404 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       29 | 5405 | `			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       29 | 5406 | `			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;` |
|       29 | 5407 | `			pCtxFrom->iDelegateState = 1;` |
|       67 | 5408 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       51 | 5409 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       51 | 5410 | `			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       51 | 5411 | `			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){` |
|       41 | 5412 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       41 | 5413 | `				pCtxFrom->iDelegateState = 3;` |
|       31 | 5414 | `			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){` |
|        9 | 5415 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        9 | 5416 | `				pCtxFrom->iDelegateState = 2;` |
|        6 | 5417 | `			}else{` |
|        5 | 5418 | `				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - | 5419 | `					sizeof("IteratorAggregate")-1,FALSE,0);` |
|        7 | 5420 | `				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){` |
|        - | 5421 | `					/* Delegate to the Iterator returned by getIterator() */` |
|        - | 5422 | `					ph7_value sIt;` |
|        5 | 5423 | `					PH7_MemObjInit(pVm,&sIt);` |
|        5 | 5424 | `					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);` |
|        5 | 5425 | `					if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){` |
|        - | 5426 | `						/* getIterator() threw/aborted: drop it, consume the` |
|        - | 5427 | `						 * operand, and propagate. */` |
|      ! 0 | 5428 | `						PH7_MemObjRelease(&sIt);` |
|      ! 0 | 5429 | `						VmPopOperand(&pTos,1);` |
|      ! 0 | 5430 | `						goto yf_propagate;` |
|        - | 5431 | `					}` |
|        4 | 5432 | `					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls` |
|        5 | 5433 | `						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){` |
|        5 | 5434 | `						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);` |
|        5 | 5435 | `						pCtxFrom->iDelegateState = 2;` |
|        3 | 5436 | `					}else{` |
|      ! 0 | 5437 | `						bIterable = 0;` |
|        - | 5438 | `					}` |
|        5 | 5439 | `					PH7_MemObjRelease(&sIt);` |
|        3 | 5440 | `				}else{` |
|      ! 0 | 5441 | `					bIterable = 0;` |
|        - | 5442 | `				}` |
|        - | 5443 | `			}` |
|       28 | 5444 | `		}else{` |
|        6 | 5445 | `			bIterable = 0;` |
|        - | 5446 | `		}` |
|       79 | 5447 | `		VmPopOperand(&pTos,1); /* Consume the iterable operand */` |
|       79 | 5448 | `		if( !bIterable ){` |
|        - | 5449 | `			/* Non-iterable source: throw a catchable Error (PHP 8.5), then` |
|        - | 5450 | `			 * funnel through the shared teardown/route path. */` |
|        6 | 5451 | `			rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 5452 | `				"Can use \"yield from\" only with arrays and Traversables",` |
|        - | 5453 | `				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);` |
|        6 | 5454 | `			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        6 | 5455 | `			goto yf_propagate;` |
|        - | 5456 | `		}` |
|       75 | 5457 | `		if( pCtxFrom->iDelegateState >= 2 ){` |
|        - | 5458 | `			/* rewind() the delegate (also starts a fresh generator) */` |
|       51 | 5459 | `			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 5460 | `				"rewind",sizeof("rewind")-1,0);` |
|       51 | 5461 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       23 | 5462 | `		}` |
|       40 | 5463 | `	}else{` |
|        - | 5464 | `		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a` |
|        - | 5465 | `		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()` |
|        - | 5466 | `		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)` |
|        - | 5467 | `		 * ignore send() and just advance with next(). */` |
|        - | 5468 | `#ifdef UNTRUST` |
|        - | 5469 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 5470 | `#endif` |
|      117 | 5471 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       67 | 5472 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|        - | 5473 | `			/* A pending Generator::throw() on the outer was parked on pInjected by the` |
|        - | 5474 | `			 * body-entry gate (which skips its own raise for state 3); forward it as a` |
|        - | 5475 | `			 * throw into the delegate, else forward the sent value (pTos, which` |
|        - | 5476 | `			 * VmResumeCtx copies into the inner's own stack). */` |
|       67 | 5477 | `			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;` |
|       67 | 5478 | `			pCtxFrom->pInjected = 0;` |
|       67 | 5479 | `			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       67 | 5480 | `				if( pInjFwd ){` |
|        - | 5481 | `					/* Borrowed ref: the outer's Generator::throw() holds it across this` |
|        - | 5482 | `					 * whole resume, so the inner inject path must not unref it. */` |
|        5 | 5483 | `					pInner->pCtx->pInjected = pInjFwd;` |
|        5 | 5484 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);` |
|        5 | 5485 | `					pInner->pCtx->pInjected = 0;` |
|        3 | 5486 | `				}else{` |
|       63 | 5487 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);` |
|        3 | 5488 | `				}` |
|       32 | 5489 | `			}else if( pInjFwd ){` |
|        - | 5490 | `				/* No live delegate to receive the throw (inner already finished/closed):` |
|        - | 5491 | `				 * raise it at the yield-from in the outer generator's own frame. */` |
|      ! 0 | 5492 | `				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);` |
|      ! 0 | 5493 | `				pTF->iFlags \|= VM_FRAME_THROW;` |
|      ! 0 | 5494 | `				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 5495 | `			}` |
|       67 | 5496 | `			PH7_MemObjRelease(pTos);` |
|       67 | 5497 | `			pTos--;` |
|       67 | 5498 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       32 | 5499 | `		}else{` |
|       53 | 5500 | `			PH7_MemObjRelease(pTos);` |
|       53 | 5501 | `			pTos--;` |
|       53 | 5502 | `			if( pCtxFrom->iDelegateState >= 2 ){` |
|       17 | 5503 | `				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 5504 | `					"next",sizeof("next")-1,0);` |
|       17 | 5505 | `				if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 5506 | `			}` |
|        - | 5507 | `		}` |
|        - | 5508 | `	}` |
|        - | 5509 | `	/* Fetch the current (key,value) of the delegate, or mark exhausted. */` |
|      177 | 5510 | `	if( pCtxFrom->iDelegateState == 1 ){` |
|       63 | 5511 | `		if( pCtxFrom->pDelegateNode == 0 ){` |
|       21 | 5512 | `			bExhausted = 1;` |
|       13 | 5513 | `		}else{` |
|       47 | 5514 | `			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);` |
|       47 | 5515 | `			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);` |
|        - | 5516 | `			/* Forward traversal follows pPrev (the hashmap's "reverse link",` |
|        - | 5517 | `			 * matching PH7_HashmapGetNextEntry). */` |
|       47 | 5518 | `			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;` |
|        - | 5519 | `		}` |
|       34 | 5520 | `	}else{` |
|      119 | 5521 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|        - | 5522 | `		ph7_value sValid;` |
|        - | 5523 | `		int isValid;` |
|      119 | 5524 | `		PH7_MemObjInit(pVm,&sValid);` |
|      119 | 5525 | `		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|      119 | 5526 | `		PH7_MemObjToBool(&sValid);` |
|      119 | 5527 | `		isValid = (sValid.x.iVal != 0);` |
|      119 | 5528 | `		PH7_MemObjRelease(&sValid);` |
|      119 | 5529 | `		if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      119 | 5530 | `		if( !isValid ){` |
|       28 | 5531 | `			bExhausted = 1;` |
|       16 | 5532 | `		}else{` |
|       95 | 5533 | `			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|       95 | 5534 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       95 | 5535 | `			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       95 | 5536 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        - | 5537 | `		}` |
|        - | 5538 | `	}` |
|      177 | 5539 | `	if( bExhausted ){` |
|        - | 5540 | `		/* Expression value: inner Generator's return value (state 3) or NULL. */` |
|        - | 5541 | `		ph7_value sResult;` |
|       45 | 5542 | `		PH7_MemObjInit(pVm,&sResult);` |
|       45 | 5543 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       23 | 5544 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|       23 | 5545 | `			if( pInner && pInner->pCtx ){` |
|       23 | 5546 | `				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);` |
|       10 | 5547 | `			}` |
|       10 | 5548 | `		}` |
|       45 | 5549 | `		PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       45 | 5550 | `		pCtxFrom->pDelegateNode = 0;` |
|       45 | 5551 | `		pCtxFrom->iDelegateState = 0;` |
|       45 | 5552 | `		pTos++;` |
|       45 | 5553 | `		PH7_MemObjStore(&sResult,pTos);` |
|       45 | 5554 | `		PH7_MemObjRelease(&sResult);` |
|       45 | 5555 | `		PH7_MemObjRelease(&sKey);` |
|       45 | 5556 | `		PH7_MemObjRelease(&sVal);` |
|       45 | 5557 | `		break; /* fall through to pc+1 with the result on the stack top */` |
|        - | 5558 | `	}` |
|        - | 5559 | `	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.` |
|        - | 5560 | `	 * The outer generator's implicit auto-key counter is NOT advanced by the` |
|        - | 5561 | ``	 * delegated keys — PHP keeps it independent across `yield from`, so a later`` |
|        - | 5562 | ``	 * plain `yield` continues from the outer's own counter. */`` |
|      137 | 5563 | `	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);` |
|      137 | 5564 | `	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);` |
|      137 | 5565 | `	PH7_MemObjRelease(&sKey);` |
|      137 | 5566 | `	PH7_MemObjRelease(&sVal);` |
|        - | 5567 | `	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */` |
|      137 | 5568 | `	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));` |
|      137 | 5569 | `	goto Suspend;` |
|        7 | 5570 | `yf_propagate:` |
|        - | 5571 | `	/* A delegate iterator method threw/aborted (or the source was non-iterable):` |
|        - | 5572 | `	 * tear down the delegation, then route via the shared dispatch macro. rcm is` |
|        - | 5573 | `	 * always PH7_EXCEPTION or PH7_ABORT here. */` |
|       17 | 5574 | `	PH7_MemObjRelease(&sKey);` |
|       17 | 5575 | `	PH7_MemObjRelease(&sVal);` |
|       17 | 5576 | `	PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       17 | 5577 | `	pCtxFrom->pDelegateNode = 0;` |
|       17 | 5578 | `	pCtxFrom->iDelegateState = 0;` |
|       17 | 5579 | `	PH7_DISPATCH_ENFORCE_RC(rcm)` |
|      ! 0 | 5580 | `	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */` |
|        - | 5581 | `}` |
|        - | 5582 | `/*` |
|        - | 5583 | ` * OP_CALL P1 * *` |
|        - | 5584 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - | 5585 | ` *  function on the stack.` |
|        - | 5586 | ` */` |
|        - | 5587 | `/*` |
|        - | 5588 | ` * OP_CALL_INIT * P2 *` |
|        - | 5589 | ` *  Screen the callee on TOS where it is WRITTEN — before this call's arguments run.` |
|        - | 5590 | ` *` |
|        - | 5591 | ` *  php resolves a call's target at INIT_FCALL / INIT_FCALL_BY_NAME / INIT_DYNAMIC_CALL` |
|        - | 5592 | `` *  and raises there, so `undefinedFn(s(1))`, `$f(s(1))` over a misspelled name and`` |
|        - | 5593 | `` *  `$v(s(1))` over an int all refuse BEFORE `s(1)` runs. PHL only ever looked at the`` |
|        - | 5594 | ` *  callee inside OP_CALL, one instruction after the whole argument list, so every one` |
|        - | 5595 | ` *  of those programs produced the argument's side effects (or its exception) first and` |
|        - | 5596 | ` *  php's Error second. The messages were already identical; only the order was not.` |
|        - | 5597 | ` *` |
|        - | 5598 | ` *  The verdict is the FIRST-CLASS-CALLABLE creation screen, unchanged and shared: php` |
|        - | 5599 | `` *  gives `f(...)` the direct call's taxonomy word for word, which makes VmFccValueError`` |
|        - | 5600 | ` *  the one builder for both. The value is left exactly as it is — OP_CALL still does its` |
|        - | 5601 | ` *  own resolution — so this adds a refusal and changes nothing that succeeds. P2 == 1` |
|        - | 5602 | ` *  when the compiler namespace-qualified the name, which is the one bit php's` |
|        - | 5603 | `` *  global-function fallback needs (an unqualified `strlen(...)` inside a namespace).`` |
|        - | 5604 | ` *` |
|        - | 5605 | ` *  Not emitted for a callee whose OP_MEMBER already screened it, for a first-class` |
|        - | 5606 | ` *  callable (OP_LOAD_FCC screens it, with nothing running in between), or for a call` |
|        - | 5607 | ` *  with no arguments at all — there the call IS the first thing that happens.` |
|        - | 5608 | ` */` |
|   770734 | 5609 | `case PH7_OP_CALL_INIT: {` |
|  1543554 | 5610 | `	if( (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_MAGICCALL)) == 0` |
|  1543559 | 5611 | `	 && !VmValueIsClosure(pVm,pTos) ){` |
|  1539733 | 5612 | `		const char *zInitCls = 0,*zInitMeth = 0;` |
|  1539733 | 5613 | `		sxu32 nInitCls = 0,nInitMeth = 0;` |
|        - | 5614 | `		char zInitMsg[192];` |
|  1539733 | 5615 | `		const char *zInitBad = 0;` |
|  1539733 | 5616 | `		SyString sInitName = { 0, 0 };` |
|  1539733 | 5617 | `		int bInitScoped = 0;` |
|  1539733 | 5618 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|  1539583 | 5619 | `			SyStringInitFromBuf(&sInitName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 5620 | `			/* A leading backslash only anchors the name to the global namespace. */` |
|  1539583 | 5621 | `			if( sInitName.nByte > 0 && sInitName.zString[0] == '\\' ){` |
|        3 | 5622 | `				sInitName.zString++;` |
|        3 | 5623 | `				sInitName.nByte--;` |
|        1 | 5624 | `			}` |
|  1539583 | 5625 | `			bInitScoped = PH7_VmCallableStringParts(sInitName.zString,sInitName.nByte,` |
|        - | 5626 | `				&zInitCls,&nInitCls,&zInitMeth,&nInitMeth);` |
|   770832 | 5627 | `		}` |
|  1539733 | 5628 | `		if( bInitScoped ){` |
|        - | 5629 | ``			/* A `"Class::method"` string carries its whole taxonomy in one builder — the`` |
|        - | 5630 | `			 * class, the missing/abstract/inaccessible cases and the catch-all routing —` |
|        - | 5631 | `			 * and answers 0 when the call WILL run. It is asked unconditionally because` |
|        - | 5632 | `			 * the predicate below is not the same question: is_callable() accepts a` |
|        - | 5633 | `			 * non-static method named through a class, which the direct call refuses. */` |
|       28 | 5634 | `			zInitBad = VmCallableClassMethodError(&(*pVm),` |
|        9 | 5635 | `				PH7_VmExtractClass(&(*pVm),zInitCls,nInitCls,FALSE,0),` |
|        9 | 5636 | `				zInitCls,nInitCls,zInitMeth,nInitMeth,TRUE,zInitMsg,sizeof(zInitMsg));` |
|  1539724 | 5637 | `		}else if( !PH7_VmIsCallable(&(*pVm),pTos,TRUE) ){` |
|      101 | 5638 | `			int bInitOk = 0;` |
|      101 | 5639 | `			if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 5640 | `				/* php's global fallback for an UNQUALIFIED name written inside a` |
|        - | 5641 | `				 * namespace: the current namespace first, the global one after. OP_CALL` |
|        - | 5642 | `				 * retries the same way from its argument map; this only has to agree` |
|        - | 5643 | `				 * about whether the call WILL resolve, so the shortened name is tested` |
|        - | 5644 | `				 * and thrown away. */` |
|       57 | 5645 | `				const char *zInitShort = sInitName.zString;` |
|        - | 5646 | `				sxu32 iInitPos;` |
|      857 | 5647 | `				for( iInitPos = 0 ; iInitPos < sInitName.nByte ; ++iInitPos ){` |
|      805 | 5648 | `					if( sInitName.zString[iInitPos] == '\\' ){` |
|       71 | 5649 | `						zInitShort = &sInitName.zString[iInitPos + 1];` |
|       33 | 5650 | `					}` |
|      405 | 5651 | `				}` |
|       57 | 5652 | `				if( zInitShort != sInitName.zString ){` |
|        - | 5653 | `					ph7_value sInitShort;` |
|       57 | 5654 | `					PH7_MemObjInit(pVm,&sInitShort);` |
|       83 | 5655 | `					PH7_MemObjStringAppend(&sInitShort,zInitShort,` |
|       52 | 5656 | `						(sxu32)(sInitName.nByte - (sxu32)(zInitShort - sInitName.zString)));` |
|       57 | 5657 | `					bInitOk = PH7_VmIsCallable(&(*pVm),&sInitShort,TRUE);` |
|       57 | 5658 | `					PH7_MemObjRelease(&sInitShort);` |
|       26 | 5659 | `				}` |
|       26 | 5660 | `			}` |
|        - | 5661 | `			/* The FIRST-CLASS-CALLABLE creation screen's builder, unchanged and shared:` |
|        - | 5662 | ``			 * php gives `f(...)` the direct call's taxonomy word for word. It assumes the`` |
|        - | 5663 | `			 * predicate has already declined — a pair a class answers through __call is` |
|        - | 5664 | `			 * callable and never arrives here — which is why it sits under that test. */` |
|      101 | 5665 | `			if( !bInitOk ){` |
|       48 | 5666 | `				zInitBad = VmFccValueError(&(*pVm),pTos,zInitMsg,sizeof(zInitMsg));` |
|       22 | 5667 | `			}` |
|       48 | 5668 | `		}` |
|  1539733 | 5669 | `		if( zInitBad ){` |
|        - | 5670 | `			sxi32 rcInit;` |
|       54 | 5671 | `			PH7_MemObjRelease(pTos);` |
|       54 | 5672 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       54 | 5673 | `			pTos->nIdx = SXU32_HIGH;` |
|       54 | 5674 | `			rcInit = VmThrowFromVm(&(*pVm),"Error",zInitBad,(sxu32)SyStrlen(zInitBad));` |
|       54 | 5675 | `			if( rcInit == SXERR_ABORT ){ goto Abort; }` |
|       54 | 5676 | `			rc = rcInit;` |
|       80 | 5677 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5678 | `		}` |
|   770882 | 5679 | `	}` |
|  1543509 | 5680 | `	break;` |
|        - | 5681 | `}` |
|        - | 5682 | `/*` |
|        - | 5683 | ` * OP_ROT_CALLEE P1 P2 *` |
|        - | 5684 | ` *  Turn a call's operand region over: [callee][arg0..argN] becomes [arg0..argN][callee],` |
|        - | 5685 | ` *  which is the layout OP_CALL's entire dispatch is written against.` |
|        - | 5686 | ` *` |
|        - | 5687 | ` *  The codegen pushes the callee FIRST because php resolves it where it is written —` |
|        - | 5688 | ` *  before a single argument runs — so an undefined or inaccessible method is refused` |
|        - | 5689 | `` *  ahead of the argument list's side effects, and a `?->` on null skips the arguments`` |
|        - | 5690 | ` *  altogether. Everything downstream of this instruction still sees the historical` |
|        - | 5691 | ` *  stack, so the reordering costs one memory move per call and nothing else.` |
|        - | 5692 | ` *` |
|        - | 5693 | ` *  P1 is the compile-time argument count; P2 carries PH7_ROT_SPREAD (this call unpacks,` |
|        - | 5694 | ` *  so the runtime count is P1 plus its OWN runs' net growth) and PH7_ROT_TWOSLOT (the` |
|        - | 5695 | ` *  callee is a method pair, [receiver][name]). A __call routing collapses that pair to` |
|        - | 5696 | ` *  one marked carrier at run time, which is read off the slot rather than guessed.` |
|        - | 5697 | ` */` |
|  1278064 | 5698 | `case PH7_OP_ROT_CALLEE: {` |
|  5116433 | 5699 | `	sxi32 nRotArgs = pInstr->iP1` |
|  2558214 | 5700 | `		+ ((pInstr->iP2 & PH7_ROT_SPREAD)` |
|        - | 5701 | `			/* One past the last argument is one past the TOP here: the callee sits` |
|        - | 5702 | `			 * BELOW the region, not above it as at OP_CALL. */` |
|  1278363 | 5703 | `			? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,&pTos[1]) : 0);` |
|  2558219 | 5704 | `	if( nRotArgs < 0 ){` |
|        - | 5705 | `		/* Unreachable: an empty unpack subtracts one per compile-time position, so the` |
|        - | 5706 | `		 * net can reach 0 and no lower. Clamped rather than trusted — reading above the` |
|        - | 5707 | `		 * top to find the callee is not a failure mode worth leaving open. */` |
|      ! 0 | 5708 | `		nRotArgs = 0;` |
|      ! 0 | 5709 | `	}` |
|        - | 5710 | `	{` |
|        - | 5711 | `		ph7_value aCallee[2];` |
|  2558219 | 5712 | `		ph7_value *pTopCallee = &pTos[-nRotArgs];` |
|  2558219 | 5713 | `		sxi32 nCallee = (pInstr->iP2 & PH7_ROT_TWOSLOT) ? 2 : 1;` |
|        - | 5714 | `		ph7_value *pBase;` |
|        - | 5715 | `		sxi32 i;` |
|  2558219 | 5716 | `		if( nCallee > 1 && (pTopCallee->iFlags & MEMOBJ_AUX_MAGICCALL) ){` |
|        - | 5717 | `			/* OP_MEMBER routed a missing/inaccessible name to __call: it consumed the` |
|        - | 5718 | `			 * receiver and left ONE carrier slot, so the pair the compiler counted on` |
|        - | 5719 | `			 * is not there. */` |
|       71 | 5720 | `			nCallee = 1;` |
|       34 | 5721 | `		}` |
|  2558219 | 5722 | `		pBase = pTopCallee - (nCallee - 1);` |
|        - | 5723 | `#ifdef UNTRUST` |
|        - | 5724 | `		if( pBase < pStack ){` |
|        - | 5725 | `			goto Abort;` |
|        - | 5726 | `		}` |
|        - | 5727 | `#endif` |
|  2558219 | 5728 | `		if( nRotArgs > 0 ){` |
|  5127651 | 5729 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  2569459 | 5730 | `				aCallee[i] = pBase[i];` |
|  1285775 | 5731 | `			}` |
|  6411737 | 5732 | `			for( i = 0 ; i < nRotArgs ; ++i ){` |
|  3853545 | 5733 | `				pBase[i] = pBase[i + nCallee];` |
|  1928266 | 5734 | `			}` |
|  5127651 | 5735 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  2569459 | 5736 | `				pBase[nRotArgs + i] = aCallee[i];` |
|  1285775 | 5737 | `			}` |
|  1280139 | 5738 | `		}` |
|  2558219 | 5739 | `		if( pInstr->iP2 & PH7_ROT_SPREAD ){` |
|        - | 5740 | `			/* The argument region now ends nCallee slots lower than it did, so this` |
|        - | 5741 | `			 * call's captured unpack runs — the suffix VmSpreadOwnExtra just assigned` |
|        - | 5742 | `			 * to it, all of them anchored inside the region — move with it, and OP_CALL` |
|        - | 5743 | ``			 * re-derives the same count from them. Unconditional: an `f(...[])` unpack`` |
|        - | 5744 | `			 * moves NO argument (its run is zero-width) and still has to be re-anchored,` |
|        - | 5745 | `			 * or the recount reads it as an ordinary slot and eats one slot too many.` |
|        - | 5746 | `			 * An ENCLOSING call's runs sit below the callee and are left alone. */` |
|      602 | 5747 | `			sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      602 | 5748 | `			VmSpreadRun *aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|        - | 5749 | `			sxu32 r;` |
|     1214 | 5750 | `			for( r = pVm->nSpreadCallBase ; r < nRun ; ++r ){` |
|      616 | 5751 | `				aRun[r].pStart -= nCallee;` |
|      310 | 5752 | `			}` |
|      299 | 5753 | `		}` |
|        - | 5754 | `	}` |
|  2558219 | 5755 | `	break;` |
|        - | 5756 | `}` |
|  2140020 | 5757 | `case PH7_OP_CALL: {` |
|        - | 5758 | `	/* iP2 = hasSpread (compile-time). Count only THIS call's own unpack` |
|        - | 5759 | `	 * expansion (VmSpreadOwnExtra, derived from the captured runs on top of the` |
|        - | 5760 | `	 * stack) — an INNER spread-bearing call evaluated inside this argument list` |
|        - | 5761 | ``	 * (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not`` |
|        - | 5762 | `	 * be conflated, which a single shared accumulator could not express. */` |
|  4282117 | 5763 | `	sxi32 nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|        - | 5764 | `	ph7_value *pArg;` |
|        - | 5765 | `	/* Consume the engine's magic-dispatch latch here, at the head of the ONE call` |
|        - | 5766 | `	 * it was set for, whatever path that call then takes. Left standing it would` |
|        - | 5767 | `	 * describe the next call instead. */` |
|  4282117 | 5768 | `	int bMagicDispatch = pVm->bMagicDispatch;` |
|        - | 5769 | `	/* ...and the member resolution's own verdict, which rides the callee SLOT rather` |
|        - | 5770 | `	 * than the VM: an OP_MEMBER that produced this callee already decided its` |
|        - | 5771 | `	 * visibility against the entry it chose, so the screen below must stand down. */` |
|  8499441 | 5772 | `	int bMemberScreened = (pTos->iFlags & MEMOBJ_AUX_MEMBERCALL) != 0` |
|  4282112 | 5773 | `		\|\| pVm->bClosureScreened;` |
|        - | 5774 | `	/* ...and whether the NAME in that slot is one of the engine's own function-table` |
|        - | 5775 | `	 * keys rather than something the program spelled: an OP_MEMBER method resolution` |
|        - | 5776 | ``	 * pushes the method's `sVmName` (`[__Class@meth_xxxxxxxxxx]`), and so do the two`` |
|        - | 5777 | `	 * synthetic call builders. Read here because the member mark is cleared just` |
|        - | 5778 | `	 * below; the closure branch adds its own case further down. It is what lets` |
|        - | 5779 | `	 * PH7_VmGetUserFunction refuse those keys to a SCRIPT that spells one. */` |
|  4282117 | 5780 | `	int bEngineCallee = (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_ENGINEFN)) != 0;` |
|        - | 5781 | `	/* ...and the internal-callback latch, for the same reason: it describes THIS call` |
|        - | 5782 | `	 * (an internal function invoking a userland callback binds its arguments weakly),` |
|        - | 5783 | `	 * and a call the callback body makes must not inherit it. */` |
|  4282117 | 5784 | `	int bCallbackWeak = pVm->bCallbackWeak;` |
|  4282117 | 5785 | `	pVm->bMagicDispatch = 0;` |
|  4282117 | 5786 | `	pVm->bClosureScreened = 0;` |
|  4282117 | 5787 | `	pVm->bCallbackWeak = 0;` |
|  4282117 | 5788 | `	pTos->iFlags &= ~MEMOBJ_AUX_MEMBERCALL;` |
|  4282117 | 5789 | `	pArg = &pTos[-nCallArgs];` |
|        - | 5790 | `	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED` |
|        - | 5791 | `	 * arguments, and a spread expanding to !=1 element shifts the actual positions` |
|        - | 5792 | `	 * of any following compile-time named args. The effective per-actual-slot name` |
|        - | 5793 | `	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)` |
|        - | 5794 | `	 * is built PER PATH against that path's finalized arg base — a method call pops` |
|        - | 5795 | `	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch` |
|        - | 5796 | `	 * site rather than built once here. */` |
|        - | 5797 | `	VmCallArgMap sEffMap;` |
|  4282117 | 5798 | `	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;` |
|        - | 5799 | `	SyHashEntry *pEntry;` |
|        - | 5800 | `	SyString sName;` |
|        - | 5801 | `	/* The host-call trio, hoisted out of the foreign-function branch below so the` |
|        - | 5802 | `	 * NativeCall label there can also be reached from the compiled-function branch:` |
|        - | 5803 | `	 * a VM_FUNC_NATIVE method is a C body wearing a method's clothes, and it runs on` |
|        - | 5804 | `	 * exactly the foreign path (no frame, no call record, no operand stack) once` |
|        - | 5805 | `	 * $this and the called class have been resolved the method way. Jumping into` |
|        - | 5806 | `	 * that branch would otherwise cross these declarations. */` |
|        - | 5807 | `	ph7_user_func *pFunc;` |
|        - | 5808 | `	ph7_context sCtx;` |
|        - | 5809 | `	ph7_value sRet;` |
|        - | 5810 | `	/* Native-METHOD call state; all 0 for a plain host function, which is what the` |
|        - | 5811 | `	 * foreign branch's own fallthrough leaves.` |
|        - | 5812 | `	 *   pNativeOwned — the reference the method path took, released at the shared tail` |
|        - | 5813 | `	 *   pNativeRecv  — what the body actually sees as $this (0 for a static method)` |
|        - | 5814 | `	 *   pNativeClass — the late-static-binding target */` |
|  4282117 | 5815 | `	ph7_class_instance *pNativeOwned = 0;` |
|  4282117 | 5816 | `	ph7_class_instance *pNativeRecv = 0;` |
|  4282117 | 5817 | `	ph7_class *pNativeClass = 0;` |
|        - | 5818 | `	/* The engine's own __call/__callStatic routing: the OP_MEMBER immediately below this` |
|        - | 5819 | `	 * call found a missing (or inaccessible) method on a class declaring the magic handler` |
|        - | 5820 | `	 * and MARKED this callee slot, latching {receiver, class, original name} on the VM.` |
|        - | 5821 | `	 * There is no callable here at all — the mark selects the packing body directly, ahead` |
|        - | 5822 | `	 * of every callable decode below, and the record it dispatches carries no PHP name (it` |
|        - | 5823 | `	 * is not in hHostFunction). This is what replaced writing the string` |
|        - | 5824 | `	 * "__phl_magic_call" into the slot and letting the name lookup find a hidden global.` |
|        - | 5825 | ``	 * Everything from `NativeCall` down is shared with an ordinary builtin call, which is`` |
|        - | 5826 | `	 * what this has always been from the executor's point of view. */` |
|  4282117 | 5827 | `	if( pTos->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|        - | 5828 | `		/* Move the routing off the carrier and onto the VM, HERE — one instruction` |
|        - | 5829 | `		 * before the packing body reads it, with nothing in between that could set` |
|        - | 5830 | `		 * another. OP_MEMBER used to publish it directly, which only held while the` |
|        - | 5831 | `		 * arguments ran before it; now they run after, and a routed call inside this` |
|        - | 5832 | `		 * one's argument list has already come and gone. */` |
|      130 | 5833 | `		VmMagicCall *pPend = (VmMagicCall *)pTos->x.pOther;` |
|      130 | 5834 | `		pTos->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|      130 | 5835 | `		pTos->x.pOther = 0;` |
|      130 | 5836 | `		pVm->pMagicCallThis = pPend ? pPend->pRecv : 0;` |
|      130 | 5837 | `		pVm->pMagicCallClass = pPend ? pPend->pClass : 0;` |
|      130 | 5838 | `		SyBlobReset(&pVm->sMagicCallName);` |
|      130 | 5839 | `		if( pPend && SyBlobLength(&pPend->sName) > 0 ){` |
|      193 | 5840 | `			SyBlobAppend(&pVm->sMagicCallName,SyBlobData(&pPend->sName),` |
|       63 | 5841 | `				SyBlobLength(&pPend->sName));` |
|       63 | 5842 | `		}` |
|      130 | 5843 | `		if( pPend ){` |
|        - | 5844 | `			/* The receiver reference the record held is now the VM's, which` |
|        - | 5845 | `			 * VmMagicCallDispatch gives back — so drop the record without unref'ing. */` |
|      130 | 5846 | `			pPend->pRecv = 0;` |
|      130 | 5847 | `			VmFreeMagicCall(pPend);` |
|       63 | 5848 | `		}` |
|      130 | 5849 | `		pFunc = PH7_VmMagicCallFunc(&(*pVm));` |
|      130 | 5850 | `		if( pFunc == 0 ){` |
|      ! 0 | 5851 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 5852 | `			goto Abort;` |
|        - | 5853 | `		}` |
|        - | 5854 | `		/* D1: the packing body declares no by-ref parameter (php hands __call a packed` |
|        - | 5855 | `		 * ARRAY), so every deferred argument materializes by value, exactly as it did` |
|        - | 5856 | `		 * through the named trampoline's zero by-ref mask. */` |
|        - | 5857 | `		{` |
|      130 | 5858 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffCallMap);` |
|      130 | 5859 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 5860 | `		}` |
|      193 | 5861 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|      126 | 5862 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|      130 | 5863 | `		goto NativeCall;` |
|        - | 5864 | `	}` |
|        - | 5865 | `	/* A Closure object is callable: unwrap it to its underlying string callable so the` |
|        - | 5866 | `	 * dispatch below handles it (rather than treating it as a generic object and looking` |
|        - | 5867 | `	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure` |
|        - | 5868 | `	 * so a plain __invoke object skips the temp-value work entirely. */` |
|  4281991 | 5869 | `	if( VmValueIsClosure(pVm,pTos) ){` |
|        - | 5870 | `		ph7_value sCallable;` |
|    12045 | 5871 | `		PH7_MemObjInit(pVm,&sCallable);` |
|    12045 | 5872 | `		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){` |
|    12045 | 5873 | `			PH7_MemObjRelease(pTos);` |
|    12045 | 5874 | `			PH7_MemObjStore(&sCallable,pTos);` |
|        - | 5875 | `` 			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]` `` |
|        - | 5876 | `			 * name, which the lookup below refuses to a name a SCRIPT spelled. */` |
|    12045 | 5877 | `			bEngineCallee = 1;` |
|     6020 | 5878 | `		}` |
|    12045 | 5879 | `		PH7_MemObjRelease(&sCallable);` |
|     6020 | 5880 | `	}` |
|        - | 5881 | `	/* Extract function name */` |
|  4281991 | 5882 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|   300435 | 5883 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 5884 | `			ph7_value sResult;` |
|        - | 5885 | `			sxi32 rcArr;` |
|        - | 5886 | `			/* Taken off the VM at the head of the shape check below, not at the dispatch:` |
|        - | 5887 | `			 * everything between the two (the deferred-argument materialization especially)` |
|        - | 5888 | `			 * can throw and jump out of this branch, and a latch left armed would stand the` |
|        - | 5889 | `			 * visibility screen down for whatever call runs next. */` |
|        - | 5890 | `			int bCbScreened;` |
|        - | 5891 | `			{` |
|        - | 5892 | `				/* php validates the SHAPE of an array callable first: it must hold exactly` |
|        - | 5893 | `				 * two elements. PH7 handed any array to the dispatcher, which failed` |
|        - | 5894 | ``				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */`` |
|   100330 | 5895 | `				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - | 5896 | `				char zCbMsg[192];` |
|   100330 | 5897 | `				const char *zCbErr = 0;` |
|   100330 | 5898 | `				int bCbRaised = 0; /* the class lookup ran an autoloader that threw */` |
|        - | 5899 | `				/* A pair the closure UNWRAP just built is not an array the program wrote: its` |
|        - | 5900 | `				 * callee was resolved and screened where the closure was BUILT, the way php` |
|        - | 5901 | `				 * resolves one, and it is a well-formed [target, method] by construction.` |
|        - | 5902 | `				 * Re-deciding it here, against the CALLER, is what refused an escaped` |
|        - | 5903 | ``				 * `$this->priv(...)` php runs. */`` |
|   100330 | 5904 | `				bCbScreened = pVm->bClosureScreened;` |
|   100330 | 5905 | `				pVm->bClosureScreened = 0; /* put back for the one dispatch that reads it */` |
|   100330 | 5906 | `				if( !bCbScreened && pCbMap && pCbMap->nEntry == 2 ){` |
|        - | 5907 | `					/* Shape is right; now check it actually RESOLVES. The shared dispatcher` |
|        - | 5908 | `					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for` |
|        - | 5909 | `					 * an unresolvable [class,method] pair -- silence a caller cannot detect --` |
|        - | 5910 | `					 * and its contract is relied on by call_user_func/usort, so the throw` |
|        - | 5911 | `					 * belongs here at the call site. */` |
|   100188 | 5912 | `					ph7_value *pCbCls = 0;` |
|   100188 | 5913 | `					ph7_value *pCbMeth = 0;` |
|        - | 5914 | `					/* php reads the INTEGER indices 0 and 1, not the first two entries in` |
|        - | 5915 | `					 * insertion order. Two entries at other keys are a SHAPE error of their` |
|        - | 5916 | `					 * own ("has to contain indices 0 and 1"), distinct from the` |
|        - | 5917 | ``					 * wrong-element-COUNT message below; `[1=>'m',0=>'C']` holds both, so`` |
|        - | 5918 | `					 * the target is index 0 -- the reverse of what PH7 walked. */` |
|   100188 | 5919 | `					if( !PH7_VmArrayCallableParts(&(*pVm),pCbMap,&pCbCls,&pCbMeth) ){` |
|       11 | 5920 | `						zCbErr = "Array callback has to contain indices 0 and 1";` |
|        6 | 5921 | `					}else{` |
|        - | 5922 | `						/* The resolution below can run an autoloader that throws; when it does,` |
|        - | 5923 | `						 * php propagates THAT exception and never reports the class missing. */` |
|   100178 | 5924 | `						sxi32 nCbBrc = pVm->nBoundaryRc;` |
|   100178 | 5925 | `						const void *pCbRes = (const void *)pVm->pResumeFrame;` |
|   150265 | 5926 | `						zCbErr = VmDirectArrayCallableError(&(*pVm),pCbCls,pCbMeth,` |
|    50087 | 5927 | `							zCbMsg,sizeof(zCbMsg));` |
|   100178 | 5928 | `						if( zCbErr && PH7_VmClassLookupRaised(&(*pVm),nCbBrc,pCbRes) ){` |
|        6 | 5929 | `							bCbRaised = 1;` |
|        2 | 5930 | `						}` |
|        - | 5931 | `					}` |
|    50092 | 5932 | `				}` |
|   100330 | 5933 | `				if( !bCbScreened && (pCbMap == 0 \|\| pCbMap->nEntry != 2 \|\| zCbErr) ){` |
|        - | 5934 | `					sxi32 rcCb;` |
|       87 | 5935 | `					if( pInstr->iP2 ){` |
|      ! 0 | 5936 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 5937 | `					}` |
|       87 | 5938 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 5939 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 5940 | `					}` |
|       87 | 5941 | `					PH7_MemObjRelease(pTos);` |
|       87 | 5942 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       87 | 5943 | `					pTos->nIdx = SXU32_HIGH;` |
|       87 | 5944 | `					if( bCbRaised ){` |
|        - | 5945 | `						/* Land the autoloader's own throw: consume the parked status (an` |
|        - | 5946 | `						 * in-place catch leaves 0 behind plus a recorded resume frame, which` |
|        - | 5947 | `						 * the router below picks up). */` |
|        6 | 5948 | `						rcCb = pVm->nBoundaryRc;` |
|        6 | 5949 | `						pVm->nBoundaryRc = 0;` |
|        6 | 5950 | `						if( rcCb == PH7_ABORT ){ goto Abort; }` |
|        6 | 5951 | `						rc = PH7_EXCEPTION;` |
|       18 | 5952 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5953 | `					}` |
|       82 | 5954 | `					if( zCbErr == 0 ){` |
|       17 | 5955 | `						zCbErr = "Array callback must have exactly two elements";` |
|        8 | 5956 | `					}` |
|       82 | 5957 | `					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));` |
|       82 | 5958 | `					if( rcCb == SXERR_ABORT ){ goto Abort; }` |
|       82 | 5959 | `					rc = rcCb;` |
|        - | 5960 | `					/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 5961 | `					 * (SXRET_OK + recorded resume) FELL THROUGH and kept dispatching` |
|        - | 5962 | `					 * the malformed callable inside the try. Route like OP_THROW. */` |
|      106 | 5963 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5964 | `				}` |
|        - | 5965 | `			}` |
|        - | 5966 | `			/* Build the effective spread-key map (and consume this call's runs)` |
|        - | 5967 | `			 * against this path's arg base (the array-callable slot isn't popped). */` |
|   150366 | 5968 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100242 | 5969 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 5970 | `			/* Materialize the deferred arguments against the pair's own method (see` |
|        - | 5971 | `			 * VmIndirectCalleeFunc), not against a blanket by-ref assumption. */` |
|        - | 5972 | `			{` |
|   100245 | 5973 | `				sxi32 rcDA = VmResolveIndirectArgs(&(*pVm),pTos,pArg,pTos,pEffCallMap);` |
|   100245 | 5974 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 5975 | `			}` |
|   100245 | 5976 | `			SySetReset(&aArg);` |
|   100423 | 5977 | `			while( pArg < pTos ){` |
|      181 | 5978 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      181 | 5979 | `				pArg++;` |
|        3 | 5980 | `			}` |
|   100245 | 5981 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 5982 | `			/* May be a class instance and it's static method. Forward this call's named-arg map` |
|        - | 5983 | ``			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —`` |
|        - | 5984 | `			 * mirroring the __invoke-object branch below. */` |
|   100245 | 5985 | `			pVm->bClosureScreened = bCbScreened; /* see the capture above */` |
|   100245 | 5986 | `			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|        - | 5987 | `			/* The latch is consumed by the method OP_CALL this dispatch builds; clear it here` |
|        - | 5988 | `			 * for the paths that never reach one. */` |
|   100245 | 5989 | `			pVm->bClosureScreened = 0;` |
|   100245 | 5990 | `			SySetReset(&aArg);` |
|        - | 5991 | `			/* Pop given arguments */` |
|   100245 | 5992 | `			if( nCallArgs > 0 ){` |
|      155 | 5993 | `				VmPopOperand(&pTos,nCallArgs);` |
|       76 | 5994 | `			}` |
|   100245 | 5995 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 | 5996 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 5997 | `				goto Abort;` |
|        - | 5998 | `			}` |
|   100245 | 5999 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - | 6000 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - | 6001 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - | 6002 | `				sxi32 iResumePc;` |
|   100014 | 6003 | `				PH7_MemObjRelease(&sResult);` |
|   100014 | 6004 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100004 | 6005 | `					PH7_MemObjRelease(pTos);` |
|        - | 6006 | ``					/* Drain the abandoned outer-expression operands (`1 + $cb()`)`` |
|        - | 6007 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 6008 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300006 | 6009 | `					PH7_RESUME_DRAIN()` |
|   100004 | 6010 | `					pc = iResumePc;` |
|   100004 | 6011 | `					break;` |
|        - | 6012 | `				}` |
|       11 | 6013 | `				goto Exception;` |
|        - | 6014 | `			}` |
|        - | 6015 | `			/* Copy result */` |
|      233 | 6016 | `			PH7_MemObjStore(&sResult,pTos);` |
|      233 | 6017 | `			PH7_MemObjRelease(&sResult);` |
|   200223 | 6018 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|   200092 | 6019 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - | 6020 | `			ph7_value sResult;` |
|        - | 6021 | `			sxi32 rcInv;` |
|        - | 6022 | `			/* __invoke object callable: the object slot isn't popped, so pArg is` |
|        - | 6023 | `			 * already this call's arg base — build the map + consume the runs. */` |
|   300136 | 6024 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   200088 | 6025 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6026 | `			/* Materialize the deferred arguments against this object's __invoke, the` |
|        - | 6027 | `			 * array-callable path's rule one shape over. */` |
|        - | 6028 | `			{` |
|   200092 | 6029 | `				sxi32 rcDA = VmResolveIndirectArgs(&(*pVm),pTos,pArg,pTos,pEffCallMap);` |
|   200092 | 6030 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6031 | `			}` |
|   200092 | 6032 | `			SySetReset(&aArg);` |
|   200186 | 6033 | `			while( pArg < pTos ){` |
|       98 | 6034 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       98 | 6035 | `				pArg++;` |
|        4 | 6036 | `			}` |
|   200092 | 6037 | `			PH7_MemObjInit(pVm,&sResult);` |
|   300136 | 6038 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|   200088 | 6039 | `				(int)SySetUsed(&aArg),` |
|   200088 | 6040 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - | 6041 | `				&sResult,` |
|   100044 | 6042 | `				pEffCallMap);` |
|   200092 | 6043 | `			SySetReset(&aArg);` |
|        - | 6044 | `			/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable` |
|        - | 6045 | `			 * slot itself (it pops top-down and the callable IS pTos), which for a` |
|        - | 6046 | `			 * temporary like (new Plain())(...) holds the only reference — popping it` |
|        - | 6047 | `			 * would free pThis before VmRaiseNotCallable reads its class name below.` |
|        - | 6048 | `			 * Only the not-callable branch dereferences pThis afterwards, so pin just` |
|        - | 6049 | `			 * for that case; the matching PH7_ClassInstanceUnref drops it (destroying` |
|        - | 6050 | `			 * the temporary). The other branches let the pop free the temp as before. */` |
|   200092 | 6051 | `			if( rcInv == SXERR_INVALID ){` |
|   100006 | 6052 | `				pThis->iRef++;` |
|    50002 | 6053 | `			}` |
|   200092 | 6054 | `			if( nCallArgs > 0 ){` |
|       78 | 6055 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 | 6056 | `			}` |
|   200092 | 6057 | `			if( rcInv == SXERR_INVALID ){` |
|        - | 6058 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - | 6059 | `				 * sResult was already released by VmCallObjectInvoke. */` |
|   100006 | 6060 | `				PH7_MemObjRelease(pTos);` |
|   100006 | 6061 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|   100006 | 6062 | `				PH7_ClassInstanceUnref(pThis);` |
|   100006 | 6063 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 6064 | `					goto Abort;` |
|        - | 6065 | `				}` |
|        - | 6066 | `				{` |
|        - | 6067 | `					sxi32 iRp;` |
|   100006 | 6068 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|        - | 6069 | `						/* Drain the abandoned outer-expression operands` |
|        - | 6070 | ``						 * (`1 + $plainObj()`) to the try's base — one leaked`` |
|        - | 6071 | `						 * slot per caught throw otherwise. */` |
|   300010 | 6072 | `						PH7_RESUME_DRAIN()` |
|   100006 | 6073 | `						pc = iRp;` |
|   100006 | 6074 | `						break;` |
|        - | 6075 | `					}` |
|        - | 6076 | `				}` |
|      ! 0 | 6077 | `				goto Exception;` |
|        - | 6078 | `			}` |
|   100088 | 6079 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 | 6080 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 6081 | `				goto Abort;` |
|        - | 6082 | `			}` |
|   100088 | 6083 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - | 6084 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - | 6085 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - | 6086 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - | 6087 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - | 6088 | `				sxi32 iResumePc;` |
|   100008 | 6089 | `				PH7_MemObjRelease(&sResult);` |
|   100008 | 6090 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100006 | 6091 | `					PH7_MemObjRelease(pTos);` |
|        - | 6092 | ``					/* Drain the abandoned outer-expression operands (`1 + $inv()`)`` |
|        - | 6093 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 6094 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300010 | 6095 | `					PH7_RESUME_DRAIN()` |
|   100006 | 6096 | `					pc = iResumePc;` |
|   100006 | 6097 | `					break;` |
|        - | 6098 | `				}` |
|        3 | 6099 | `				goto Exception;` |
|        - | 6100 | `			}` |
|       82 | 6101 | `			PH7_MemObjStore(&sResult,pTos);` |
|       82 | 6102 | `			PH7_MemObjRelease(&sResult);` |
|       43 | 6103 | `		}else{` |
|        - | 6104 | `			/* php: calling a non-callable is a catchable Error naming the type` |
|        - | 6105 | `			 * ("Value of type int is not callable"), or -- for an array -- the shape it` |
|        - | 6106 | `			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so` |
|        - | 6107 | ``			 * `$x()` on a number quietly evaluated to nothing. */`` |
|        - | 6108 | `			sxi32 rcNc;` |
|        - | 6109 | `			char zMsg[128];` |
|       17 | 6110 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 6111 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");` |
|      ! 0 | 6112 | `			}else{` |
|       25 | 6113 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",` |
|        8 | 6114 | `					VmArithTypeName(pTos));` |
|        - | 6115 | `			}` |
|        - | 6116 | `			/* Consume this call's captured spread runs — a non-callable target` |
|        - | 6117 | `			 * (int/float/bool/null) reaches no dispatch build site. */` |
|       17 | 6118 | `			if( pInstr->iP2 ){` |
|      ! 0 | 6119 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 6120 | `			}` |
|        - | 6121 | `			/* Pop given arguments */` |
|       17 | 6122 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 6123 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6124 | `			}` |
|        - | 6125 | `			/* Settle the call's result slot BEFORE throwing. */` |
|       17 | 6126 | `			PH7_MemObjRelease(pTos);` |
|       17 | 6127 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       17 | 6128 | `			pTos->nIdx = SXU32_HIGH;` |
|       17 | 6129 | `			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       17 | 6130 | `			if( rcNc == SXERR_ABORT ){ goto Abort; }` |
|       17 | 6131 | `			rc = rcNc;` |
|        - | 6132 | `			/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 6133 | `			 * (SXRET_OK + recorded resume) FELL THROUGH and resumed the try body` |
|        - | 6134 | `			 * right after the failed call. Route like OP_THROW. */` |
|       31 | 6135 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6136 | `		}` |
|      312 | 6137 | `		break;` |
|        - | 6138 | `	}` |
|  3981561 | 6139 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 6140 | `	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just` |
|        - | 6141 | `	 * anchors it to the global namespace — strip it before resolving so a` |
|        - | 6142 | ``	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */`` |
|  3981561 | 6143 | `	if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|       15 | 6144 | `		sName.zString++;` |
|       15 | 6145 | `		sName.nByte--;` |
|        7 | 6146 | `	}` |
|        - | 6147 | `	/* Check for a compiled function first.` |
|        - | 6148 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 6149 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|  3981561 | 6150 | `	pEntry = PH7_VmGetUserFunction(pVm,(const void *)sName.zString,sName.nByte,bEngineCallee);` |
|        - | 6151 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - | 6152 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - | 6153 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - | 6154 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - | 6155 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - | 6156 | `	{` |
|  3981561 | 6157 | `	VmCallArgMap *pCallMap = pEffCallMap;` |
|  3981561 | 6158 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - | 6159 | `		const char *zFunc;` |
|        - | 6160 | `		const char *zEnd;` |
|        - | 6161 | `		const char *z;` |
|        - | 6162 | `		SyString sGlobal;` |
|       57 | 6163 | `		zFunc = sName.zString;` |
|       57 | 6164 | `		zEnd  = zFunc + sName.nByte;` |
|       57 | 6165 | `		z = zEnd;` |
|        - | 6166 | `		/* Find last namespace separator */` |
|      529 | 6167 | `		while( z > zFunc ){` |
|      529 | 6168 | `			if( z[-1] == '\\' ){` |
|       57 | 6169 | `				break;` |
|        - | 6170 | `			}` |
|      477 | 6171 | `			z--;` |
|        5 | 6172 | `		}` |
|       57 | 6173 | `		if( z > zFunc && z < zEnd ){` |
|        - | 6174 | `			/* Retry lookup using the unqualified/global function name */` |
|       57 | 6175 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       57 | 6176 | `			pEntry = PH7_VmGetUserFunction(pVm,(const void *)sGlobal.zString,sGlobal.nByte,bEngineCallee);` |
|       26 | 6177 | `		}` |
|       26 | 6178 | `	}` |
|        - | 6179 | `	} /* end VmCallArgMap namespace scope */` |
|  3981561 | 6180 | `	if( pEntry ){` |
|        - | 6181 | `		ph7_vm_func_arg *aFormalArg;` |
|        - | 6182 | `		ph7_class_instance *pThis;` |
|        - | 6183 | `		ph7_value *pFrameStack;` |
|        - | 6184 | `		ph7_vm_func *pVmFunc;` |
|        - | 6185 | `		ph7_class *pSelf;` |
|        - | 6186 | `		ph7_class *pSelfHint;` |
|        - | 6187 | `		VmFrame *pFrame;` |
|        - | 6188 | `		ph7_value *pObj;` |
|        - | 6189 | `		VmSlot sArg;` |
|        - | 6190 | `		sxu32 n;` |
|  2286097 | 6191 | `		sxi32 iArgPreFlags = 0; /* the actual's type before its declared-type check */` |
|  2286097 | 6192 | `		int bClosureThis = 0;` |
|  2286097 | 6193 | `		ph7_class *pClosureScope = 0;` |
|        - | 6194 | `		/* initialize fields */` |
|  2286097 | 6195 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|  2286097 | 6196 | `		pThis = 0;` |
|  2286097 | 6197 | `		pSelf = 0;` |
|        - | 6198 | `		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it` |
|        - | 6199 | `		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever` |
|        - | 6200 | `		 * set for a bound plain closure, which dispatches as a function, so the method branch below` |
|        - | 6201 | `		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */` |
|  2286097 | 6202 | `		if( pVm->pClosureThis ){` |
|       70 | 6203 | `			pThis = pVm->pClosureThis;` |
|       70 | 6204 | `			pVm->pClosureThis = 0;` |
|       70 | 6205 | `			bClosureThis = 1;` |
|       33 | 6206 | `		}` |
|  2286097 | 6207 | `		if( pVm->pClosureScope ){` |
|        - | 6208 | `			/* May ride alongside a bound $this, or stand alone for a` |
|        - | 6209 | ``			 * scope-only rebind (`bindTo(null, Scope::class)`). */`` |
|       60 | 6210 | `			pClosureScope = pVm->pClosureScope;` |
|       60 | 6211 | `			pVm->pClosureScope = 0;` |
|       28 | 6212 | `		}` |
|  2286097 | 6213 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - | 6214 | `			ph7_class_method *pMeth;` |
|        - | 6215 | `			/* Class method call */` |
|  2020561 | 6216 | `			ph7_value *pTarget = &pTos[-1];` |
|  2020561 | 6217 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - | 6218 | `				/* Extract the 'this' pointer */` |
|  2020561 | 6219 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - | 6220 | `					/* Instance already loaded */` |
|  1918931 | 6221 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|  1918931 | 6222 | `					pThis->iRef++;` |
|  1918931 | 6223 | `					pSelf = pThis->pClass;` |
|   959464 | 6224 | `				}` |
|  2020561 | 6225 | `				if( pSelf == 0 ){` |
|   101635 | 6226 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - | 6227 | `						/* "Late Static Binding" class name */` |
|   152441 | 6228 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|    50812 | 6229 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|    50812 | 6230 | `					}` |
|   101635 | 6231 | `					if( pSelf == 0 ){` |
|        7 | 6232 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 | 6233 | `					}` |
|    50815 | 6234 | `				}` |
|  2020561 | 6235 | `				if( pThis == 0  ){` |
|   101635 | 6236 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|   101635 | 6237 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   101635 | 6238 | `					if( pFrameLocal->pParent ){` |
|        - | 6239 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|      765 | 6240 | `						pThis = pFrameLocal->pThis;` |
|      765 | 6241 | `						if( pThis ){` |
|       62 | 6242 | `							pThis->iRef++;` |
|       30 | 6243 | `						}` |
|      380 | 6244 | `					}` |
|    50815 | 6245 | `				}` |
|  2020561 | 6246 | `				VmPopOperand(&pTos,1);` |
|  2020561 | 6247 | `				PH7_MemObjRelease(pTos);` |
|        - | 6248 | `				/* Synchronize pointers. The method-name slot popped above sat BETWEEN` |
|        - | 6249 | `				 * this call's arguments and the (already-removed) target — so only now` |
|        - | 6250 | `				 * is pTos one past the last actual argument. Re-derive the unpack` |
|        - | 6251 | `				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs` |
|        - | 6252 | `				 * from run ends, which the extra target slot would otherwise offset,` |
|        - | 6253 | ``				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */`` |
|  2020561 | 6254 | `				nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);` |
|  2020561 | 6255 | `				pArg = &pTos[-nCallArgs];` |
|        - | 6256 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - | 6257 | `				 * user have already computed the random generated unique class method name` |
|        - | 6258 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - | 6259 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - | 6260 | `				 */` |
|  2020561 | 6261 | `				while( pArg < pStack ){` |
|      ! 0 | 6262 | `					pArg++;` |
|      ! 0 | 6263 | `				}` |
|  2020561 | 6264 | `				if( pSelf && pVm->bReflectBypass ){` |
|        - | 6265 | `					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection` |
|        - | 6266 | `					 * ignores visibility. Consume-once so nested calls made by the` |
|        - | 6267 | `					 * invoked body are checked normally. */` |
|      219 | 6268 | `					pVm->bReflectBypass = 0;` |
|      110 | 6269 | `				}else` |
|  2020343 | 6270 | `				if( pSelf && bMagicDispatch && PH7_MagicMethodMustBePublic(&pVmFunc->sName) ){` |
|        - | 6271 | `					/* The ENGINE reaching for a magic method php requires to be public.` |
|        - | 6272 | `					 * php only WARNS at such a declaration and then dispatches the method` |
|        - | 6273 | ``					 * regardless -- the engine calling `__get` is not the outside world`` |
|        - | 6274 | `					 * reaching for a private member -- so a non-public` |
|        - | 6275 | ``					 * `__get`/`__call`/`__invoke`/`__sleep`/... still runs, where PHL threw`` |
|        - | 6276 | ``					 * `Call to private method C::__get()` at the ACCESS and killed the`` |
|        - | 6277 | `					 * script.` |
|        - | 6278 | `					 *` |
|        - | 6279 | `					 * The latch is set by PH7_VmCallMagicMethod at the engine's own` |
|        - | 6280 | `					 * dispatch sites. It is NOT inferred from the instruction: a` |
|        - | 6281 | ``					 * first-class `$o->__get(...)` and `$o->__get('x')` reach the same C`` |
|        - | 6282 | `					 * dispatcher, and php denies both. */` |
|    53616 | 6283 | `				}else` |
|  1913122 | 6284 | `				if( pSelf && !bMemberScreened ){ /* Paranoid edition */` |
|        - | 6285 | `					/* Check if the call is allowed. php binds non-public method` |
|        - | 6286 | `					 * access by the DECLARING class (pVmFunc->pUserData — the class` |
|        - | 6287 | `					 * the callee was compiled in), NOT the instance's class: an` |
|        - | 6288 | `					 * inherited base method calling $this->priv() on a child` |
|        - | 6289 | `					 * instance passes, a child's private SHADOW doesn't hijack the` |
|        - | 6290 | `					 * check for a parent callee, and the denial message names the` |
|        - | 6291 | `					 * declaring class like php. */` |
|  1783402 | 6292 | `					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|        - | 6293 | `					ph7_class *pOwnerClass;` |
|  1783402 | 6294 | `					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|  1783402 | 6295 | `					if( pMeth == 0 && pDeclClass != pSelf ){` |
|      ! 0 | 6296 | `						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      ! 0 | 6297 | `					}` |
|  1783402 | 6298 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        - | 6299 | `						/* ...except that a TRAIT is not a class php still has at run time: it` |
|        - | 6300 | `						 * composed the method INTO the using class, so that class owns the` |
|        - | 6301 | `						 * rule and the name. Deciding against the trait refused a protected` |
|        - | 6302 | `						 * trait method to a SUBCLASS of the composing class (which uses no` |
|        - | 6303 | ``						 * trait of its own) — `class Az { use Tz; } class Bz extends Az {`` |
|        - | 6304 | ``						 * $this->pr(); }` was a fatal php runs. Identity for every non-trait`` |
|        - | 6305 | `						 * method. */` |
|       27 | 6306 | `						pOwnerClass = PH7_VmMethodScopeName(&(*pVm),pSelf,pMeth);` |
|       27 | 6307 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - | 6308 | `							/* php throws a CATCHABLE Error here. The old code merely PRINTED an` |
|        - | 6309 | ``							 * uncaught-exception report and aborted, so `try { $o->priv(); }`` |
|        - | 6310 | ``							 * catch (Error $e)` never caught it and the script died. */`` |
|        - | 6311 | `							char zMsg[256];` |
|        - | 6312 | `							sxi32 rcVis;` |
|        - | 6313 | `							/* php NAMES the calling scope when there is one — "from scope C" —` |
|        - | 6314 | `							 * and says "global scope" only outside every class; the wording is` |
|        - | 6315 | `							 * shared with the first-class-callable screen` |
|        - | 6316 | `							 * (VmMethodVisibilityMsg). */` |
|       19 | 6317 | `							VmMethodVisibilityMsg(&(*pVm),pOwnerClass,` |
|        6 | 6318 | `								pVmFunc->sName.zString,pVmFunc->sName.nByte,` |
|        6 | 6319 | `								pMeth->iProtection,zMsg,sizeof(zMsg));` |
|        - | 6320 | `							/* Consume this call's captured spread runs — this visibility` |
|        - | 6321 | `							 * error exits before the pVmFunc build below. */` |
|       13 | 6322 | `							if( pInstr->iP2 ){` |
|      ! 0 | 6323 | `								VmSpreadConsume(pVm);` |
|      ! 0 | 6324 | `							}` |
|        - | 6325 | `							/* Pop given arguments, and leave the call's NULL result behind. */` |
|       13 | 6326 | `							if( nCallArgs > 0 ){` |
|      ! 0 | 6327 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6328 | `							}` |
|       13 | 6329 | `							PH7_MemObjRelease(pTos);` |
|       13 | 6330 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|       13 | 6331 | `							pTos->nIdx = SXU32_HIGH;` |
|       13 | 6332 | `							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       13 | 6333 | `							if( rcVis == SXERR_ABORT ){ goto Abort; }` |
|       13 | 6334 | `							rc = rcVis;` |
|        - | 6335 | `							/* ENFORCE_RC never consulted pResumeFrame, so an in-place` |
|        - | 6336 | `							 * catch (SXRET_OK + recorded resume) FELL THROUGH and` |
|        - | 6337 | `							 * dispatched the DENIED method anyway. Route like OP_THROW. */` |
|       13 | 6338 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6339 | `						}` |
|        7 | 6340 | `					}` |
|   891692 | 6341 | `				}` |
|  1010273 | 6342 | `			}` |
|  1010273 | 6343 | `		}` |
|        - | 6344 | `		/* pArg is now finalized for every pVmFunc callee (a method call popped its` |
|        - | 6345 | `		 * method-name slot above; functions/closures/generators keep the top base).` |
|        - | 6346 | `		 * Build the PHP 8.1 effective spread-key map here so the generator and the` |
|        - | 6347 | `		 * install path below both see it — and so this call's captured runs are` |
|        - | 6348 | `		 * consumed exactly once, against the correct base. */` |
|  3429343 | 6349 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  2286080 | 6350 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6351 | `		/* Check the PHP call-depth cap (the sole site — BYTECODE.md stage 5).` |
|        - | 6352 | `		 * Default is unbounded (heap-bound recursion, decoupled from the C stack` |
|        - | 6353 | `		 * by the stage-2 trampoline); the C stack is guarded separately by` |
|        - | 6354 | `		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and` |
|        - | 6355 | `		 * then raises a clean non-catchable fatal (was: silently set NULL and` |
|        - | 6356 | `		 * continue) and halts. */` |
|  2286085 | 6357 | `		if( VmRecursionExceeded(pVm) ){` |
|        - | 6358 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - | 6359 | `			 * which walks the whole operand stack — don't release them here. */` |
|        3 | 6360 | `			VmRecursionFatal(&(*pVm));` |
|        3 | 6361 | `			goto Abort;` |
|        - | 6362 | `		}` |
|  2286083 | 6363 | `		if( pVmFunc->pNextName ){` |
|        - | 6364 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      289 | 6365 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|      142 | 6366 | `		}` |
|        - | 6367 | ``		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's`` |
|        - | 6368 | ``		 * `Class::method`). Computed after overload resolution so it reflects the selected method.`` |
|        - | 6369 | ``		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed`` |
|        - | 6370 | `		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method` |
|        - | 6371 | ``		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the`` |
|        - | 6372 | `		 * USING class; we don't carry the using class on the shared struct, so fall back to the` |
|        - | 6373 | `		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for` |
|        - | 6374 | `		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */` |
|  2286083 | 6375 | `		pSelfHint = pSelf;` |
|  2286083 | 6376 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|  2020549 | 6377 | `			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;` |
|  2020549 | 6378 | `			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|  2020243 | 6379 | `				pSelfHint = pDecl;` |
|  1010120 | 6380 | `			}` |
|  1010273 | 6381 | `		}` |
|  2286083 | 6382 | `		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 6383 | `			/* Push the closure's called-class as this frame's LSB class so` |
|        - | 6384 | ``			 * `static::` inside the body resolves like php. An explicit`` |
|        - | 6385 | `			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use` |
|        - | 6386 | `			 * the class captured at the closure's creation site. self::/parent::` |
|        - | 6387 | `			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done` |
|        - | 6388 | ``			 * after pSelfHint so a `self`-typed param is unaffected. */`` |
|    18724 | 6389 | `			if( pClosureScope ){` |
|       60 | 6390 | `				pSelf = pClosureScope;` |
|    18696 | 6391 | `			}else if( pVmFunc->pLsbClass ){` |
|      133 | 6392 | `				pSelf = (ph7_class *)pVmFunc->pLsbClass;` |
|       64 | 6393 | `			}` |
|     9355 | 6394 | `		}` |
|  2286083 | 6395 | `		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){` |
|        - | 6396 | `			/* php 8.4 #[\Deprecated] runtime notice — once per call, before` |
|        - | 6397 | `			 * execution (generators: at the g(...) call site, like php). */` |
|      284 | 6398 | `			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,` |
|      186 | 6399 | `				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);` |
|       93 | 6400 | `		}` |
|        - | 6401 | `		/* D1: resolve deferred plain-var arguments against this callee's declaration` |
|        - | 6402 | `		 * now — BEFORE the native/generator splits and VmEnterFrame, while pVm->pFrame is` |
|        - | 6403 | `		 * still the caller. pVmFunc is final here (post-overload). Covers plain functions,` |
|        - | 6404 | `		 * methods, closures, dynamic-name calls, generators and native methods uniformly;` |
|        - | 6405 | `		 * only the SOURCE of the by-ref positions differs between the last one and the` |
|        - | 6406 | `		 * rest (a signature string vs. compiled formal parameters). */` |
|        - | 6407 | `		{` |
|        - | 6408 | `			sxi32 rcDA;` |
|  2286083 | 6409 | `			if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 6410 | `				/* A native method declares no formal parameters to match against —` |
|        - | 6411 | `				 * its by-ref positions come from the same signature-derived mask a` |
|        - | 6412 | `				 * builtin uses, so resolve them the builtin way. Sharing this one` |
|        - | 6413 | `				 * site (rather than repeating it in the branch below) keeps the` |
|        - | 6414 | `				 * throw routing identical for both kinds of callee. */` |
|  2272386 | 6415 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|  1514920 | 6416 | `					pVmFunc->pNative->nByRefMask,0,0,pEffCallMap);` |
|   757466 | 6417 | `			}else{` |
|  1156959 | 6418 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   771158 | 6419 | `					(ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs),SySetUsed(&pVmFunc->aArgs),` |
|   385796 | 6420 | `					0,0,0,pEffCallMap);` |
|        - | 6421 | `			}` |
|  2286095 | 6422 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6423 | `		}` |
|  2286055 | 6424 | `		if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 6425 | `			/* C-bodied method (VM_FUNC_NATIVE): hand it to the foreign-function path.` |
|        - | 6426 | `			 *` |
|        - | 6427 | `			 * Everything a METHOD needs has already happened above — the sVmName` |
|        - | 6428 | `			 * lookup, overload selection, $this / self / late-static-binding` |
|        - | 6429 | `			 * resolution, the visibility check, the #[\Deprecated] notice, deferred` |
|        - | 6430 | `			 * argument materialization — and everything BELOW is exactly what a C` |
|        - | 6431 | `			 * body has no use for: a frame, an operand stack, a call record.` |
|        - | 6432 | `			 *` |
|        - | 6433 | `			 * The stack shape already matches a builtin's, because the method branch` |
|        - | 6434 | `			 * popped both the method-name slot and the target slot: [pArg,pTos) are` |
|        - | 6435 | `			 * this call's arguments and pTos is the slot that receives the result.` |
|        - | 6436 | `			 * So the jump lands on shared code, not a copy of it. */` |
|  1514925 | 6437 | `			pFunc = pVmFunc->pNative;` |
|  1514925 | 6438 | `			pNativeOwned = pThis; /* borrowed; the shared tail drops this reference */` |
|        - | 6439 | ``			/* A `C::m()` call reaches the method path with no object in the target`` |
|        - | 6440 | `			 * slot, and the path then adopts the CALLER's $this so an instance-context` |
|        - | 6441 | ``			 * `self::m()` still finds one. A native STATIC method must not see that —`` |
|        - | 6442 | `			 * it would read its argument list one slot off — so the receiver handed to` |
|        - | 6443 | `			 * the body is gated on the declaration, not on what the fallback found. */` |
|  1514925 | 6444 | `			pNativeRecv = (pVmFunc->iFlags & VM_FUNC_NATIVE_STATIC) ? 0 : pThis;` |
|  1514925 | 6445 | `			pNativeClass = pSelf;` |
|  1514925 | 6446 | `			goto NativeCall;` |
|        - | 6447 | `		}` |
|   771135 | 6448 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - | 6449 | `			/* Generator function: return a Generator object instead of executing */` |
|        - | 6450 | `			ph7_exec_ctx *pExecCtx;` |
|        - | 6451 | `			ph7_generator *pGenerator;` |
|        - | 6452 | `			ph7_class_instance *pGenObj;` |
|        - | 6453 | `			ph7_value *pCtxAttr;` |
|        - | 6454 | `			SyString sAttrName;` |
|        - | 6455 | `			ph7_value **apCallArgs;` |
|        - | 6456 | `			int nGenArgs, iArg;` |
|        - | 6457 | `			/* Collect arguments from the operand stack */` |
|      421 | 6458 | `			nGenArgs = (int)(pTos - pArg);` |
|      421 | 6459 | `			apCallArgs = 0;` |
|      421 | 6460 | `			if( nGenArgs > 0 ){` |
|        - | 6461 | `				/* php refuses a non-variable in a by-ref position at the CALL, and for` |
|        - | 6462 | `				 * a generator this IS the call. Routed like the branch's other` |
|        - | 6463 | `				 * pre-frame throws below: no callee frame exists yet, so drop the` |
|        - | 6464 | `				 * arguments plus the function-name slot and land the enclosing try. */` |
|      181 | 6465 | `				rc = VmScreenGenByRefArgs(&(*pVm),pVmFunc,pEffCallMap,pArg,` |
|       59 | 6466 | `					(sxu32)nGenArgs,pSelfHint);` |
|      122 | 6467 | `				if( rc != SXRET_OK ){` |
|        7 | 6468 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 6469 | `						goto Abort;` |
|        - | 6470 | `					}` |
|      212 | 6471 | `					PH7_INLINE_RESUME_BREAK()` |
|        7 | 6472 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6473 | `					{` |
|        - | 6474 | `						sxi32 iRpB;` |
|        7 | 6475 | `						if( VmRecordedResume(pVm,&iRpB,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 6476 | `							pc = iRpB;` |
|      ! 0 | 6477 | `							break;` |
|        - | 6478 | `						}` |
|        - | 6479 | `					}` |
|        7 | 6480 | `					goto Exception;` |
|        - | 6481 | `				}` |
|       56 | 6482 | `			}` |
|      415 | 6483 | `			if( nGenArgs > 0 ){` |
|      172 | 6484 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       56 | 6485 | `					nGenArgs * sizeof(ph7_value *));` |
|      116 | 6486 | `				if( apCallArgs == 0 ){` |
|        - | 6487 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 | 6488 | `					nGenArgs = 0;` |
|      ! 0 | 6489 | `				}else{` |
|      116 | 6490 | `					VmCallArgMap *pGenMap = pEffCallMap;` |
|      116 | 6491 | `					int didReorder = 0;` |
|      116 | 6492 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - | 6493 | `						/* Named-argument reordering for generator */` |
|       15 | 6494 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|       15 | 6495 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|       15 | 6496 | `						sxu32 nNV = nF;` |
|       15 | 6497 | `						sxi32 iVIdx = -1;` |
|        - | 6498 | `						sxi32 *aGSlot;` |
|        - | 6499 | `						sxu8 *aGUsed;` |
|        - | 6500 | `						sxu32 gi;` |
|       33 | 6501 | `						for( gi = 0; gi < nF; gi++ ){` |
|       21 | 6502 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|       12 | 6503 | `						}` |
|       21 | 6504 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       12 | 6505 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|       15 | 6506 | `						if( aGSlot ){` |
|       15 | 6507 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|       21 | 6508 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        6 | 6509 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|       15 | 6510 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 6511 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 6512 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6513 | `								goto Abort;` |
|        - | 6514 | `							}` |
|       15 | 6515 | `							if( rc == PH7_EXCEPTION ){` |
|        - | 6516 | `								/* A named-argument error is php's catchable \Error.` |
|        - | 6517 | `								 * No callee frame exists yet on this branch (the` |
|        - | 6518 | `								 * generator body never runs and VmEnterFrame is` |
|        - | 6519 | `								 * further down), so route it like the other` |
|        - | 6520 | `								 * pre-frame OP_CALL throws: drop the args + the` |
|        - | 6521 | `								 * function-name slot and land the enclosing try. */` |
|        3 | 6522 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        3 | 6523 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        3 | 6524 | `								PH7_INLINE_RESUME_BREAK()` |
|        3 | 6525 | `								VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6526 | `								{` |
|        - | 6527 | `									sxi32 iRpN;` |
|        3 | 6528 | `									if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){` |
|        3 | 6529 | `										pc = iRpN;` |
|        3 | 6530 | `										break;` |
|        - | 6531 | `									}` |
|        - | 6532 | `								}` |
|      ! 0 | 6533 | `								goto Exception;` |
|        - | 6534 | `							}` |
|        - | 6535 | `							{` |
|        - | 6536 | `								/* php's named-hole ArgumentCountError, checked BEFORE` |
|        - | 6537 | `								 * hole compaction: compacting first would report the` |
|        - | 6538 | `								 * positional wording with a fictitious count (g(b:2)` |
|        - | 6539 | ``								 * must be `g(): Argument #1 ($a) not passed`, not`` |
|        - | 6540 | `								 * "1 passed"). Implicit-required watermark, like the` |
|        - | 6541 | `								 * plain-call named path; a hole with NOTHING filled` |
|        - | 6542 | `								 * above it keeps php's count wording — fall through` |
|        - | 6543 | `								 * to VmFiberSetupFrame's check (the compacted count` |
|        - | 6544 | `								 * equals php's num_args there). */` |
|       13 | 6545 | `								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;` |
|       13 | 6546 | `								sxi32 iHole = -1;` |
|       13 | 6547 | `								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);` |
|       29 | 6548 | `								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){` |
|       19 | 6549 | `									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){` |
|       15 | 6550 | `										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);` |
|        6 | 6551 | `									}` |
|       11 | 6552 | `								}` |
|       27 | 6553 | `								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){` |
|        - | 6554 | `									sxu32 gj;` |
|       17 | 6555 | `									int bFound = 0;` |
|       23 | 6556 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       23 | 6557 | `										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }` |
|        5 | 6558 | `									}` |
|       17 | 6559 | `									if( !bFound && gHole + 1 <= nMaxFilledG ){` |
|      ! 0 | 6560 | `										iHole = (sxi32)gHole;` |
|      ! 0 | 6561 | `									}` |
|       10 | 6562 | `								}` |
|       13 | 6563 | `								if( iHole >= 0 ){` |
|      ! 0 | 6564 | `									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 6565 | `										(sxu32)iHole+1,&aFA[iHole].sName);` |
|      ! 0 | 6566 | `									SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 6567 | `									SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6568 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 6569 | `										goto Abort;` |
|        - | 6570 | `									}` |
|        - | 6571 | `									/* Route like the VmFiberSetupFrame throw below */` |
|      ! 0 | 6572 | `									PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 6573 | `									VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6574 | `									{` |
|        - | 6575 | `										sxi32 iRpH;` |
|      ! 0 | 6576 | `										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 6577 | `											pc = iRpH;` |
|      ! 0 | 6578 | `											break;` |
|        - | 6579 | `										}` |
|        - | 6580 | `									}` |
|      ! 0 | 6581 | `									goto Exception;` |
|        - | 6582 | `								}` |
|        - | 6583 | `							}` |
|        - | 6584 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - | 6585 | `							 * append overflow (variadic / positional beyond` |
|        - | 6586 | `							 * formals) so downstream sees every argument. */` |
|        - | 6587 | `							{` |
|       13 | 6588 | `								int nOut = 0;` |
|       29 | 6589 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - | 6590 | `									sxu32 gj;` |
|       25 | 6591 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       25 | 6592 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|       19 | 6593 | `											apCallArgs[nOut++] = &pArg[gj];` |
|       19 | 6594 | `											break;` |
|        - | 6595 | `										}` |
|        5 | 6596 | `									}` |
|       11 | 6597 | `								}` |
|       29 | 6598 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|       19 | 6599 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 | 6600 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 | 6601 | `									}` |
|       11 | 6602 | `								}` |
|       13 | 6603 | `								nGenArgs = nOut;` |
|        - | 6604 | `							}` |
|       13 | 6605 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|       13 | 6606 | `							didReorder = 1;` |
|        5 | 6607 | `						}` |
|        - | 6608 | `						/* If aGSlot allocation failed, fall through to` |
|        - | 6609 | `						 * positional fill below — preserves arg order rather` |
|        - | 6610 | `						 * than passing an uninitialized apCallArgs. */` |
|        5 | 6611 | `					}` |
|      114 | 6612 | `					if( !didReorder ){` |
|      210 | 6613 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|      110 | 6614 | `							apCallArgs[iArg] = &pArg[iArg];` |
|       57 | 6615 | `						}` |
|       50 | 6616 | `					}` |
|        - | 6617 | `				}` |
|       55 | 6618 | `			}` |
|        - | 6619 | `			/* Create execution context and generator wrapper */` |
|      413 | 6620 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|      413 | 6621 | `			if( pExecCtx == 0 ){` |
|      ! 0 | 6622 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6623 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 6624 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 6625 | `				break;` |
|        - | 6626 | `			}` |
|      413 | 6627 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|      413 | 6628 | `			if( pGenerator == 0 ){` |
|      ! 0 | 6629 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 | 6630 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6631 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 6632 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 6633 | `				break;` |
|        - | 6634 | `			}` |
|        - | 6635 | `			/* Set up the frame with arguments, closure env, $this */` |
|      413 | 6636 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|      413 | 6637 | `			pVm->pFrame = pExecCtx->pFrame;` |
|      821 | 6638 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,` |
|      408 | 6639 | `				pEffCallMap ? (pEffCallMap->bStrict ? 1 : 0) : (pVm->bCurStrict ? 1 : 0),` |
|      204 | 6640 | `				pSelfHint,` |
|        - | 6641 | `				TRUE/*generator: the g(...) call site is in the message*/,` |
|        - | 6642 | `				TRUE/*a source-level call binds a by-ref parameter to the caller's slot*/);` |
|      413 | 6643 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|      413 | 6644 | `			pExecCtx->pFrame->pParent = 0;` |
|      413 | 6645 | `			if( apCallArgs ){` |
|      114 | 6646 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       55 | 6647 | `			}` |
|      413 | 6648 | `			if( rc != SXRET_OK ){` |
|       18 | 6649 | `				VmReleaseGenerator(pVm, pGenerator);` |
|       18 | 6650 | `				if( pThis ){` |
|        3 | 6651 | `					PH7_ClassInstanceUnref(pThis);` |
|        1 | 6652 | `				}` |
|       18 | 6653 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 6654 | `					goto Abort;` |
|        - | 6655 | `				}` |
|       18 | 6656 | `				if( rc == PH7_EXCEPTION ){` |
|        - | 6657 | `					/* A declared-type TypeError thrown while binding the` |
|        - | 6658 | `					 * generator's arguments (php binds + type-checks eagerly at` |
|        - | 6659 | `					 * the g(...) call site, before any resume — band A #2). If` |
|        - | 6660 | `					 * an inline try THIS exec owns caught it, land at its` |
|        - | 6661 | `					 * redirect (the drain subsumes the operand pops); else pop` |
|        - | 6662 | `					 * the args + function name and route like the other` |
|        - | 6663 | `					 * OP_CALL throw paths. */` |
|       22 | 6664 | `					PH7_INLINE_RESUME_BREAK()` |
|       16 | 6665 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6666 | `					{` |
|        - | 6667 | `						sxi32 iRpG;` |
|       16 | 6668 | `						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){` |
|       16 | 6669 | `							pc = iRpG;` |
|       16 | 6670 | `							break;` |
|        - | 6671 | `						}` |
|        - | 6672 | `					}` |
|      ! 0 | 6673 | `					goto Exception;` |
|        - | 6674 | `				}` |
|      ! 0 | 6675 | `				break;` |
|        - | 6676 | `			}` |
|        - | 6677 | `			/* Create Generator class instance */` |
|      397 | 6678 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|      397 | 6679 | `			if( pGenObj == 0 ){` |
|      ! 0 | 6680 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 | 6681 | `				break;` |
|        - | 6682 | `			}` |
|        - | 6683 | `			/* Store generator in __ctx attribute */` |
|      397 | 6684 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      397 | 6685 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|      397 | 6686 | `			if( pCtxAttr ){` |
|      397 | 6687 | `				pCtxAttr->x.pOther = pGenerator;` |
|      397 | 6688 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      196 | 6689 | `			}` |
|        - | 6690 | `			/* Pop args and function name, push Generator object. PH7_NewClassInstance` |
|        - | 6691 | `			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do` |
|        - | 6692 | `			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope` |
|        - | 6693 | ``			 * and its __destruct (which runs pending `finally` blocks and frees the`` |
|        - | 6694 | `			 * exec context) never fires. */` |
|      397 | 6695 | `			PH7_MemObjRelease(pTos);` |
|      397 | 6696 | `			pTos = &pTos[-nCallArgs];` |
|      397 | 6697 | `			pTos->x.pOther = pGenObj;` |
|      397 | 6698 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|      397 | 6699 | `			if( pThis ){` |
|       16 | 6700 | `				PH7_ClassInstanceUnref(pThis);` |
|        6 | 6701 | `			}` |
|      397 | 6702 | `			break;` |
|        - | 6703 | `		}` |
|        - | 6704 | `		/* Extract the formal argument set */` |
|   770719 | 6705 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - | 6706 | `		/* Create a new VM frame  */` |
|   770719 | 6707 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|   770719 | 6708 | `		if( rc != SXRET_OK ){` |
|        - | 6709 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 6710 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 6711 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 6712 | `				&pVmFunc->sName);` |
|        - | 6713 | `			/* The frame that would own (and later release) $this never got created; for a bound` |
|        - | 6714 | `			 * plain closure the consumed transient is the object's ONLY ref, so release it here` |
|        - | 6715 | `			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */` |
|      ! 0 | 6716 | `			if( bClosureThis && pThis ){` |
|      ! 0 | 6717 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 | 6718 | `			}` |
|        - | 6719 | `			/* Pop given arguments */` |
|      ! 0 | 6720 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 6721 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6722 | `			}` |
|        - | 6723 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 | 6724 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 6725 | `			break;` |
|        - | 6726 | `		}` |
|   770719 | 6727 | `		if( pClosureScope ){` |
|        - | 6728 | `			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /` |
|        - | 6729 | `			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected` |
|        - | 6730 | `			 * access inside the body resolves against it. */` |
|       58 | 6731 | `			pFrame->pBoundScope = pClosureScope;` |
|       27 | 6732 | `		}` |
|        - | 6733 | `		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band` |
|        - | 6734 | `		 * A #4): sArg over-counts (defaulted params installed, variadic packed` |
|        - | 6735 | `		 * as one entry) so php's answers can't be derived from it. */` |
|   770719 | 6736 | `		pFrame->nActualArgs = (int)(pTos - pArg);` |
|   770719 | 6737 | `		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) \|\| bClosureThis) ){` |
|        - | 6738 | `			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */` |
|        - | 6739 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|   405167 | 6740 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|   405167 | 6741 | `			if( pObj ){` |
|        - | 6742 | `				/* Reflect the change */` |
|   405167 | 6743 | `				pObj->x.pOther = pThis;` |
|   405167 | 6744 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   202581 | 6745 | `			}` |
|   202581 | 6746 | `		}` |
|   770719 | 6747 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - | 6748 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - | 6749 | `			/* Install static variables */` |
|       47 | 6750 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       91 | 6751 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|       47 | 6752 | `				pStatic = &aStatic[n];` |
|       47 | 6753 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - | 6754 | `					/* Initialize the static variables */` |
|       27 | 6755 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|       27 | 6756 | `					if( pObj ){` |
|        - | 6757 | `						/* Assume a NULL initialization value */` |
|       27 | 6758 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|       27 | 6759 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - | 6760 | `							/* Evaluate initialization expression (Any complex expression) */` |
|       27 | 6761 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|       12 | 6762 | `						}` |
|       27 | 6763 | `						pObj->nIdx = pStatic->nIdx;` |
|        - | 6764 | `						/* Permanent pin: the storage outlives every call */` |
|       27 | 6765 | `						VmPinMemObjSlot(&(*pVm),pStatic->nIdx);` |
|       15 | 6766 | `					}else{` |
|      ! 0 | 6767 | `						continue;` |
|        - | 6768 | `					}` |
|       12 | 6769 | `				}` |
|        - | 6770 | `				/* Install in the current frame — a REGISTERED binding, and the slot is` |
|        - | 6771 | `				 * PINNED: the static's storage belongs to the function, not to this` |
|        - | 6772 | `				 * call, so neither the frame teardown nor an unset of the NAME may` |
|        - | 6773 | `				 * recycle it. Poking hVar directly left the binding invisible to the` |
|        - | 6774 | ``				 * reference table, so an array element sharing the static (`[&$s]`)`` |
|        - | 6775 | ``				 * did not count as a reference and `unset($s)` destroyed the storage —`` |
|        - | 6776 | `				 * the next call started over from the initializer. The pin is taken ONCE,` |
|        - | 6777 | `				 * where the slot is created (above). */` |
|       69 | 6778 | `				PH7_VmBindVarSlot(&(*pVm),pFrame,SyStringData(&pStatic->sName),` |
|       22 | 6779 | `					SyStringLength(&pStatic->sName),pStatic->nIdx);` |
|       25 | 6780 | `			}` |
|       22 | 6781 | `		}` |
|        - | 6782 | `		/* Push arguments in the local frame */` |
|        - | 6783 | `		{` |
|   770719 | 6784 | `		VmCallArgMap *pCallMap3 = pEffCallMap;` |
|        - | 6785 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - | 6786 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|        - | 6787 | `		/* A compiled call in a strict file always carries a map with bStrict set` |
|        - | 6788 | `		 * (GenStateAttachStrictFlag); with no map at all the call is either a` |
|        - | 6789 | `		 * compiled WEAK one — where bCurStrict is 0 for the same unit — or an` |
|        - | 6790 | `		 * ENGINE-dispatched one, whose synthetic OP_CALL has no map to carry the` |
|        - | 6791 | `		 * caller's mode. php scopes parameter coercion by the CALLING file either` |
|        - | 6792 | `		 * way, and bCurStrict is that file's mode.` |
|        - | 6793 | `		 *` |
|        - | 6794 | `		 * Unless an INTERNAL function is what reached for this callback (bCallbackWeak):` |
|        - | 6795 | `		 * php has no calling file at that boundary and binds weakly, so` |
|        - | 6796 | ``		 * `array_map('takesInt', ["5"])` from a strict file RUNS there — PHL raised a`` |
|        - | 6797 | `		 * TypeError on valid php, because the ambient bCurStrict was still the strict` |
|        - | 6798 | `		 * caller's. call_user_func / call_user_func_array are php's two forwards and` |
|        - | 6799 | `		 * carry the caller's mode on a map instead. */` |
|  1155848 | 6800 | `		int bCallIsStrict = pCallMap3 ? (pCallMap3->bStrict ? 1 : 0)` |
|   706468 | 6801 | `		                   : (bCallbackWeak ? 0 : (pVm->bCurStrict ? 1 : 0));` |
|   770719 | 6802 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - | 6803 | `			/* ============================================================` |
|        - | 6804 | `			 * Named-argument matching path (PHP 8.0)` |
|        - | 6805 | `			 *` |
|        - | 6806 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - | 6807 | `			 * or position, then install them in the frame.` |
|        - | 6808 | `			 * ============================================================ */` |
|      385 | 6809 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      385 | 6810 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|      385 | 6811 | `			sxi32 iVariadicIdx = -1;` |
|        - | 6812 | `			sxu32 nNonVariadic;` |
|        - | 6813 | `			sxi32 *aSlot;` |
|        - | 6814 | `			sxu8  *aUsed;` |
|        - | 6815 | `			sxu32 i;` |
|        - | 6816 | `			/* Find variadic parameter index */` |
|      993 | 6817 | `			for( i = 0; i < nFormal; i++ ){` |
|      719 | 6818 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|      110 | 6819 | `					iVariadicIdx = (sxi32)i;` |
|      110 | 6820 | `					break;` |
|        - | 6821 | `				}` |
|      309 | 6822 | `			}` |
|      385 | 6823 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - | 6824 | `			/* Allocate mapping arrays */` |
|      575 | 6825 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      380 | 6826 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|      385 | 6827 | `			if( aSlot == 0 ){` |
|      ! 0 | 6828 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 | 6829 | `				goto Abort;` |
|        - | 6830 | `			}` |
|      385 | 6831 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - | 6832 | `			/* Resolve named arguments to formal parameters */` |
|      575 | 6833 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|      190 | 6834 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|      385 | 6835 | `			if( rc == PH7_ABORT ){` |
|        8 | 6836 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 | 6837 | `				goto Abort;` |
|        - | 6838 | `			}` |
|      379 | 6839 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 6840 | `				/* php's catchable \Error for a bad named argument. The callee` |
|        - | 6841 | `				 * frame is already entered but its body must not run: unwind` |
|        - | 6842 | `				 * exactly like the named-hole ArgumentCountError path below` |
|        - | 6843 | `				 * (release the not-yet-installed actuals — Pass 2's release` |
|        - | 6844 | `				 * loop has not run — pop the callee slot, mark that there is no` |
|        - | 6845 | `				 * callee operand stack, and let VmCallFinish route the throw). */` |
|        - | 6846 | `				sxu32 iRel;` |
|        5 | 6847 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       11 | 6848 | `				for( iRel = 0; iRel < nActual; iRel++ ){` |
|        7 | 6849 | `					PH7_MemObjRelease(&pArg[iRel]);` |
|        4 | 6850 | `				}` |
|        5 | 6851 | `				PH7_MemObjRelease(pTos);` |
|        5 | 6852 | `				pTos = &pTos[-nCallArgs];` |
|        5 | 6853 | `				pFrameStack = 0;` |
|        5 | 6854 | `				goto SkipFuncBody;` |
|        - | 6855 | `			}` |
|        - | 6856 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|        - | 6857 | `			{` |
|        - | 6858 | `			/* php's required watermark for the hole check below, plus the` |
|        - | 6859 | `			 * highest formal slot an actual resolved to: php words a hole` |
|        - | 6860 | ``			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole`` |
|        - | 6861 | `			 * with nothing filled above it gets the positional count message` |
|        - | 6862 | `			 * (zend's RECV arg_num > EX(num_args) distinction). */` |
|        - | 6863 | `			sxu32 nReqNamed;` |
|        - | 6864 | `			sxu32 nNVNamed;` |
|      375 | 6865 | `			sxu32 nMaxFilled = 0;` |
|      375 | 6866 | `			nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);` |
|     1299 | 6867 | `			for( i = 0; i < nActual; i++ ){` |
|      929 | 6868 | `				if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|      386 | 6869 | `					nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|      191 | 6870 | `				}` |
|      467 | 6871 | `			}` |
|      953 | 6872 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - | 6873 | `				/* Find the stack arg mapped to formal n */` |
|      594 | 6874 | `				sxi32 iSrc = -1;` |
|      940 | 6875 | `				for( i = 0; i < nActual; i++ ){` |
|      808 | 6876 | `					if( aSlot[i] == (sxi32)n ){` |
|      462 | 6877 | `						iSrc = (sxi32)i;` |
|      462 | 6878 | `						break;` |
|        - | 6879 | `					}` |
|      177 | 6880 | `				}` |
|      594 | 6881 | `				if( iSrc >= 0 ){` |
|        - | 6882 | `					/* Argument was provided — install with type checking */` |
|      462 | 6883 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - | 6884 | `					/* An explicit null is NOT redirected to the default: PHP applies a` |
|        - | 6885 | `					 * default only for an OMITTED argument. An explicit null falls through` |
|        - | 6886 | `					 * to the type check below (TypeError for a non-nullable typed param,` |
|        - | 6887 | ``					 * kept as null for a typeless one). An implicitly-nullable `Type $x =`` |
|        - | 6888 | ``					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */`` |
|        - | 6889 | `					/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 6890 | `					 * coercion and whole-real materialization in place): the shared` |
|        - | 6891 | `					 * per-argument helper — one implementation for both OP_CALL` |
|        - | 6892 | `					 * paths and the generator/fiber binder (§7.1(f) fold). */` |
|      462 | 6893 | `					iArgPreFlags = pVal->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|      462 | 6894 | `					rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pVal,bCallIsStrict,pSelfHint);` |
|      462 | 6895 | `					if( rc != SXRET_OK ){` |
|        7 | 6896 | `						if( rc == PH7_ABORT ) goto Abort;` |
|        7 | 6897 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 | 6898 | `						PH7_MemObjRelease(pTos);` |
|        7 | 6899 | `						pTos = &pTos[-nCallArgs];` |
|        7 | 6900 | `						pFrameStack = 0;` |
|        7 | 6901 | `						rc = PH7_EXCEPTION;` |
|       10 | 6902 | `						goto SkipFuncBody;` |
|        - | 6903 | `					}` |
|        - | 6904 | `					/* Install: by reference or by value */` |
|      456 | 6905 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|       28 | 6906 | `						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){` |
|        - | 6907 | `							/* php 8.1: $GLOBALS cannot be passed by reference */` |
|        - | 6908 | `							SyBlob sMsg;` |
|      ! 0 | 6909 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 6910 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 6911 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 6912 | `							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|      ! 0 | 6913 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 6914 | `								goto Abort;` |
|        - | 6915 | `							}` |
|      ! 0 | 6916 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 6917 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 6918 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 6919 | `							pFrameStack = 0;` |
|      ! 0 | 6920 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 6921 | `							goto SkipFuncBody;` |
|        - | 6922 | `						}` |
|       28 | 6923 | `						if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)iSrc,pVal) ){` |
|        - | 6924 | `							/* php refuses a by-ref argument whose EXPRESSION is not a variable, at` |
|        - | 6925 | `							 * the call and before the callee runs. Deciding it from the VALUE that` |
|        - | 6926 | `							 * arrived was wrong both ways: an operator result carries its LEFT` |
|        - | 6927 | ``							 * operand's slot, so `f($i + 1)` aliased and overwrote `$i`; and a`` |
|        - | 6928 | `							 * literal and a CALL result look alike there, where php accepts the` |
|        - | 6929 | `							 * call. VmArgRefusedByRef reads the call site's compile-time shape mask` |
|        - | 6930 | `							 * and falls back to the old runtime test only when there is none. */` |
|        - | 6931 | `							sxi32 rcRef;` |
|        3 | 6932 | `							rcRef = VmThrowByRefRefusal(&(*pVm),` |
|        2 | 6933 | `								(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|        2 | 6934 | `								&pVmFunc->sName,(sxu32)(n+1),&aFormalArg[n].sName);` |
|        3 | 6935 | `							if( rcRef == PH7_ABORT ){` |
|      ! 0 | 6936 | `								goto Abort;` |
|        - | 6937 | `							}` |
|        - | 6938 | `							/* Same teardown as the type-check refusal above: free the slot map,` |
|        - | 6939 | `							 * release the result slot and pop the actuals, then let SkipFuncBody` |
|        - | 6940 | `							 * route the throw. */` |
|        3 | 6941 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 6942 | `							PH7_MemObjRelease(pTos);` |
|        3 | 6943 | `							pTos = &pTos[-nCallArgs];` |
|        3 | 6944 | `							pFrameStack = 0;` |
|        3 | 6945 | `							rc = PH7_EXCEPTION;` |
|        3 | 6946 | `							goto SkipFuncBody;` |
|        - | 6947 | `						}` |
|       25 | 6948 | `						PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)iSrc,pVal);` |
|       25 | 6949 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 | 6950 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 6951 | `						}else{` |
|        - | 6952 | `							SyHashEntry *pRefEntry;` |
|        - | 6953 | `							/* The declared type's conversion is what the reference holds. */` |
|       25 | 6954 | `							PH7_VmByRefArgWriteBack(&(*pVm),pVal,iArgPreFlags);` |
|       37 | 6955 | `							pRefEntry = SyHashGet(&pFrame->hVar,` |
|       24 | 6956 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 | 6957 | `							if( pRefEntry == 0 ){` |
|       37 | 6958 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 | 6959 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|       25 | 6960 | `								sArg.nIdx = pVal->nIdx;` |
|       25 | 6961 | `								sArg.pUserData = 0;` |
|       25 | 6962 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 | 6963 | `							}` |
|       25 | 6964 | `							pObj = 0;` |
|        - | 6965 | `						}` |
|       13 | 6966 | `					}else{` |
|      430 | 6967 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 6968 | `					}` |
|      454 | 6969 | `					if( pObj ){` |
|      430 | 6970 | `						PH7_MemObjStore(pVal,pObj);` |
|      430 | 6971 | `						sArg.nIdx = pObj->nIdx;` |
|      430 | 6972 | `						sArg.pUserData = 0;` |
|      430 | 6973 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      213 | 6974 | `					}` |
|      229 | 6975 | `				}else{` |
|        - | 6976 | `					/* Argument was NOT provided — use default or leave unset */` |
|      135 | 6977 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 6978 | `						/* Should not reach here; variadic handled separately below */` |
|      135 | 6979 | `					}else if( n < nReqNamed ){` |
|        - | 6980 | `						/* php's implicit-required rule applies to named calls` |
|        - | 6981 | `						 * too: a hole below the required watermark throws even` |
|        - | 6982 | `						 * when the formal carries a default — f($a,$b=2,$c)` |
|        - | 6983 | ``						 * called as f(a:1,c:3) is `Argument #2 ($b) not`` |
|        - | 6984 | ``						 * passed` (a hole with NO default is always below the`` |
|        - | 6985 | `						 * watermark, so this subsumes the no-default case).` |
|        - | 6986 | `						 * A hole with nothing filled ABOVE it uses php's` |
|        - | 6987 | `						 * positional count wording instead. The passed stack` |
|        - | 6988 | `						 * args were not released yet on this path (that loop` |
|        - | 6989 | `						 * runs after Pass 2) — release them before the exit. */` |
|        5 | 6990 | `						if( n + 1 > nMaxFilled ){` |
|        3 | 6991 | `							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)` |
|      ! 0 | 6992 | `								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 6993 | `									nMaxFilled,nReqNamed,SySetUsed(&pVmFunc->aArgs))` |
|        3 | 6994 | `								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        1 | 6995 | `									nMaxFilled,nReqNamed,nNVNamed,TRUE);` |
|        2 | 6996 | `						}else{` |
|        3 | 6997 | `							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        - | 6998 | `						}` |
|        5 | 6999 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        9 | 7000 | `						for( i = 0; i < nActual; i++ ){` |
|        5 | 7001 | `							PH7_MemObjRelease(&pArg[i]);` |
|        3 | 7002 | `						}` |
|        5 | 7003 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 7004 | `							goto Abort;` |
|        - | 7005 | `						}` |
|        5 | 7006 | `						PH7_MemObjRelease(pTos);` |
|        5 | 7007 | `						pTos = &pTos[-nCallArgs];` |
|        5 | 7008 | `						pFrameStack = 0;` |
|        5 | 7009 | `						rc = PH7_EXCEPTION;` |
|        5 | 7010 | `						goto SkipFuncBody;` |
|      131 | 7011 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      131 | 7012 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      131 | 7013 | `						if( pObj ){` |
|      131 | 7014 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|      131 | 7015 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      131 | 7016 | `							sArg.nIdx = pObj->nIdx;` |
|      131 | 7017 | `							sArg.pUserData = 0;` |
|      131 | 7018 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 7019 | `							/* A null default on an implicitly-nullable param must stay null` |
|        - | 7020 | `							 * (see the positional-path note above). */` |
|      128 | 7021 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|       42 | 7022 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0` |
|       24 | 7023 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 7024 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 7025 | `								if( xCast ) xCast(pObj);` |
|      ! 0 | 7026 | `							}else{` |
|        - | 7027 | `								/* Mask matched — a const-indirected whole-real default` |
|        - | 7028 | ``								 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|      131 | 7029 | `								VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 7030 | `							}` |
|       64 | 7031 | `						}` |
|       64 | 7032 | `					}` |
|        - | 7033 | `				}` |
|      293 | 7034 | `			}` |
|        - | 7035 | `			} /* end nReqNamed scope */` |
|        - | 7036 | `			/* Handle variadic parameter */` |
|      362 | 7037 | `			if( iVariadicIdx >= 0 ){` |
|      110 | 7038 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|      110 | 7039 | `				if( pObj ){` |
|        - | 7040 | `					/* Capture the slot index now: PH7_HashmapInsert below can` |
|        - | 7041 | `					 * PH7_ReserveMemObj, reallocating pVm->aMemObj and dangling pObj` |
|        - | 7042 | `					 * (same latent UAF the positional path guards against). */` |
|        - | 7043 | `					sxu32 nVariadicSlot;` |
|      110 | 7044 | `					PH7_MemObjToHashmap(pObj);` |
|      110 | 7045 | `					nVariadicSlot = pObj->nIdx;` |
|        - | 7046 | `					{` |
|      110 | 7047 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - | 7048 | `						/* php numbers a failing NAMED variadic element as` |
|        - | 7049 | `						 * max(total positional args, declared non-variadic` |
|        - | 7050 | `						 * formals) + 1 — zend's RECV slots always count —` |
|        - | 7051 | `						 * whichever named element fails; a POSITIONAL element` |
|        - | 7052 | `						 * uses its own 1-based call position. */` |
|      110 | 7053 | `						sxu32 nPositional = 0;` |
|      622 | 7054 | `						for( i = 0; i < nActual; i++ ){` |
|      516 | 7055 | `							if( !(i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0) ){` |
|      333 | 7056 | `								nPositional++;` |
|      165 | 7057 | `							}` |
|      260 | 7058 | `						}` |
|      578 | 7059 | `						for( i = 0; i < nActual; i++ ){` |
|      510 | 7060 | `							if( aSlot[i] == -1 ){` |
|      464 | 7061 | `								int bNamed = (i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0);` |
|      464 | 7062 | `								int bRefElem = 0; /* alias this entry to the caller's slot? */` |
|        - | 7063 | `								/* Same per-element type check + weak coercion as the` |
|        - | 7064 | ``								 * positional-only path (shared helper; no `($name)`). */`` |
|      694 | 7065 | `								rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|      460 | 7066 | `									&aFormalArg[iVariadicIdx],&pArg[i],` |
|      307 | 7067 | `									bNamed ? SXMAX(nPositional,nNonVariadic) + 1 : i + 1,bCallIsStrict);` |
|      464 | 7068 | `								if( rc != SXRET_OK ){` |
|       39 | 7069 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 7070 | `										goto Abort;` |
|        - | 7071 | `									}` |
|       39 | 7072 | `									SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       39 | 7073 | `									PH7_MemObjRelease(pTos);` |
|       39 | 7074 | `									pTos = &pTos[-nCallArgs];` |
|       39 | 7075 | `									pFrameStack = 0;` |
|       39 | 7076 | `									rc = PH7_EXCEPTION;` |
|       39 | 7077 | `									goto SkipFuncBody;` |
|        - | 7078 | `								}` |
|      428 | 7079 | `								if( aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7080 | `									/* php screens a by-ref VARIADIC tail per collected element, in its` |
|        - | 7081 | `									 * no-name wording: a variadic has no per-element parameter name, so` |
|        - | 7082 | ``									 * php says `Argument #N could not be passed by reference` and stops`` |
|        - | 7083 | `									 * there. Nothing screened this arm at all — the branch that collects` |
|        - | 7084 | `									 * a variadic runs before the by-ref binder ever sees a formal. */` |
|        8 | 7085 | `									if( PH7_VmArgRefusedByRef(pCallMap3,i,&pArg[i]) ){` |
|        - | 7086 | `										SyBlob sMsgV;` |
|        - | 7087 | `										sxi32 rcV;` |
|        3 | 7088 | `										SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        3 | 7089 | `										SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        2 | 7090 | `											&pVmFunc->sName,(unsigned)(i + 1));` |
|        3 | 7091 | `										rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        3 | 7092 | `										if( rcV == PH7_ABORT ){` |
|      ! 0 | 7093 | `											goto Abort;` |
|        - | 7094 | `										}` |
|        3 | 7095 | `										SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 7096 | `										PH7_MemObjRelease(pTos);` |
|        3 | 7097 | `										pTos = &pTos[-nCallArgs];` |
|        3 | 7098 | `										pFrameStack = 0;` |
|        3 | 7099 | `										rc = PH7_EXCEPTION;` |
|        3 | 7100 | `										goto SkipFuncBody;` |
|        - | 7101 | `									}` |
|        5 | 7102 | `									PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,i,&pArg[i]);` |
|        2 | 7103 | `								}` |
|        - | 7104 | `								/* A by-ref variadic tail ALIASES its actuals, named entries` |
|        - | 7105 | `								 * included (the positional twin below this branch says why). */` |
|      639 | 7106 | `								bRefElem = (aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF)` |
|      422 | 7107 | `									&& pArg[i].nIdx != SXU32_HIGH;` |
|      426 | 7108 | `								if( bNamed ){` |
|        - | 7109 | `									/* Named variadic entry: insert with string key */` |
|        - | 7110 | `									ph7_value sKey;` |
|      124 | 7111 | `									PH7_MemObjInit(pVm, &sKey);` |
|      124 | 7112 | `									PH7_MemObjStringAppend(&sKey,` |
|      120 | 7113 | `										pCallMap3->aNames[i].zString,` |
|      120 | 7114 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|      124 | 7115 | `									if( bRefElem ){` |
|        5 | 7116 | `										PH7_HashmapInsertByRef(pVarMap, &sKey, pArg[i].nIdx);` |
|        3 | 7117 | `									}else{` |
|      120 | 7118 | `										PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|        - | 7119 | `									}` |
|      124 | 7120 | `									PH7_MemObjRelease(&sKey);` |
|      365 | 7121 | `								}else if( bRefElem ){` |
|        - | 7122 | `									/* Positional variadic entry, aliased */` |
|      ! 0 | 7123 | `									PH7_HashmapInsertByRef(pVarMap, 0, pArg[i].nIdx);` |
|      ! 0 | 7124 | `								}else{` |
|        - | 7125 | `									/* Positional variadic entry */` |
|      305 | 7126 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - | 7127 | `								}` |
|      211 | 7128 | `							}` |
|      238 | 7129 | `						}` |
|        - | 7130 | `					}` |
|       72 | 7131 | `					sArg.nIdx = nVariadicSlot; /* pObj may be stale here (aMemObj realloc) */` |
|       72 | 7132 | `					sArg.pUserData = 0;` |
|       72 | 7133 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       34 | 7134 | `				}` |
|       38 | 7135 | `			}else{` |
|        - | 7136 | `				/* No variadic — preserve unresolved positional overflow` |
|        - | 7137 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - | 7138 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - | 7139 | `				 * the positional-only path's behavior. */` |
|      254 | 7140 | `				sxu32 nAnon = nNonVariadic;` |
|      652 | 7141 | `				for( i = 0; i < nActual; i++ ){` |
|      400 | 7142 | `					if( aSlot[i] == -2 ){` |
|        - | 7143 | `						char zAnonBuf[32];` |
|        - | 7144 | `						SyString sAnonName;` |
|      ! 0 | 7145 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 | 7146 | `							"[%u]apArg",nAnon);` |
|      ! 0 | 7147 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 | 7148 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 | 7149 | `						if( pObj ){` |
|      ! 0 | 7150 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 | 7151 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 | 7152 | `							sArg.pUserData = 0;` |
|      ! 0 | 7153 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 | 7154 | `						}` |
|      ! 0 | 7155 | `						nAnon++;` |
|      ! 0 | 7156 | `					}` |
|      201 | 7157 | `				}` |
|        - | 7158 | `			}` |
|        - | 7159 | `			/* Release all stack arguments */` |
|     1160 | 7160 | `			for( i = 0; i < nActual; i++ ){` |
|      840 | 7161 | `				PH7_MemObjRelease(&pArg[i]);` |
|      422 | 7162 | `			}` |
|      324 | 7163 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - | 7164 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|      324 | 7165 | `			n = nFormal;` |
|      164 | 7166 | `		}else{` |
|        - | 7167 | `		/* ============================================================` |
|        - | 7168 | `		 * Positional-only matching path (original)` |
|        - | 7169 | `		 * ============================================================ */` |
|        - | 7170 | `		/* Base of the actual argument stack, for php's per-ELEMENT argument` |
|        - | 7171 | `		 * number in a variadic type-error (php numbers a variadic-collected` |
|        - | 7172 | `		 * element by its overall 1-based call position, not the formal index). */` |
|   770339 | 7173 | `		ph7_value *pArgBase = pArg;` |
|   770339 | 7174 | `		n = 0;` |
|  1039571 | 7175 | `		while( pArg < pTos ){` |
|   274165 | 7176 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - | 7177 | `				/* Variadic parameter: collect all remaining args into an array */` |
|      631 | 7178 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      631 | 7179 | `				if( pObj ){` |
|        - | 7180 | `					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,` |
|        - | 7181 | `					 * reallocating pVm->aMemObj and dangling pObj — so don't read pObj->nIdx after` |
|        - | 7182 | `					 * the packing loop (pre-existing UAF, masked by the pool allocator). pMap is a` |
|        - | 7183 | `					 * separately-allocated hashmap and stays valid across the realloc. */` |
|        - | 7184 | `					sxu32 nVariadicIdx;` |
|        - | 7185 | `					/* Initialize as empty array */` |
|      631 | 7186 | `					PH7_MemObjToHashmap(pObj);` |
|      631 | 7187 | `					nVariadicIdx = pObj->nIdx;` |
|        - | 7188 | `					{` |
|      631 | 7189 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     2893 | 7190 | `						while( pArg < pTos ){` |
|        - | 7191 | `							/* Per-element type check + weak coercion (shared helper,` |
|        - | 7192 | `							 * also used by the named-argument path). The argument` |
|        - | 7193 | `							 * number is the element's overall 1-based call position` |
|        - | 7194 | `` 							 * ((pArg - pArgBase) + 1) and, like php, the `($name)` `` |
|        - | 7195 | `							 * clause is omitted. */` |
|     3467 | 7196 | `							rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|     2308 | 7197 | `								&aFormalArg[n],pArg,(sxu32)(pArg - pArgBase) + 1,bCallIsStrict);` |
|     2313 | 7198 | `							if( rc != SXRET_OK ){` |
|       44 | 7199 | `								if( rc == PH7_ABORT ){` |
|      ! 0 | 7200 | `									goto Abort;` |
|        - | 7201 | `								}` |
|        - | 7202 | `								/* Skip function body, route through normal cleanup */` |
|       44 | 7203 | `								PH7_MemObjRelease(pTos);` |
|       44 | 7204 | `								pTos = &pTos[-nCallArgs];` |
|       44 | 7205 | `								pFrameStack = 0;` |
|       44 | 7206 | `								rc = PH7_EXCEPTION;` |
|       44 | 7207 | `								goto SkipFuncBody;` |
|        - | 7208 | `							}` |
|     2273 | 7209 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7210 | `								/* The positional twin of the named path's variadic screen above:` |
|        - | 7211 | `								 * php refuses a non-variable collected into a by-ref variadic tail,` |
|        - | 7212 | `								 * in its no-name wording. */` |
|       52 | 7213 | `								sxu32 nPosV = (sxu32)(pArg - pArgBase);` |
|       52 | 7214 | `								if( PH7_VmArgRefusedByRef(pCallMap3,nPosV,pArg) ){` |
|        - | 7215 | `									SyBlob sMsgV;` |
|        - | 7216 | `									sxi32 rcV;` |
|        7 | 7217 | `									SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        7 | 7218 | `									SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        6 | 7219 | `										&pVmFunc->sName,(unsigned)(nPosV + 1));` |
|        7 | 7220 | `									rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        7 | 7221 | `									if( rcV == PH7_ABORT ){` |
|      ! 0 | 7222 | `										goto Abort;` |
|        - | 7223 | `									}` |
|        7 | 7224 | `									PH7_MemObjRelease(pTos);` |
|        7 | 7225 | `									pTos = &pTos[-nCallArgs];` |
|        7 | 7226 | `									pFrameStack = 0;` |
|        7 | 7227 | `									rc = PH7_EXCEPTION;` |
|        7 | 7228 | `									goto SkipFuncBody;` |
|        - | 7229 | `								}` |
|       46 | 7230 | `								PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,nPosV,pArg);` |
|       46 | 7231 | `								if( pArg->nIdx != SXU32_HIGH ){` |
|        - | 7232 | `									/* php ALIASES each collected element to the caller's slot:` |
|        - | 7233 | ``									 * `function f(&...$xs){ $xs[0] = 'A'; }` writes back, and`` |
|        - | 7234 | ``									 * var_dump($xs) inside the callee shows `&int(1)`. Copying`` |
|        - | 7235 | `									 * them left every actual untouched. The node counts as a` |
|        - | 7236 | `									 * holder of the caller's slot, so the frame teardown that` |
|        - | 7237 | `									 * destroys the variadic array gives the hold back. */` |
|       42 | 7238 | `									PH7_HashmapInsertByRef(pMap, 0, pArg->nIdx);` |
|       42 | 7239 | `									pArg++;` |
|       42 | 7240 | `									continue;` |
|        - | 7241 | `								}` |
|        2 | 7242 | `							}` |
|     2227 | 7243 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|     2227 | 7244 | `							pArg++;` |
|        5 | 7245 | `						}` |
|        - | 7246 | `					}` |
|      585 | 7247 | `					sArg.nIdx = nVariadicIdx; /* pObj may be stale here (aMemObj realloc) — use the saved index */` |
|      585 | 7248 | `					sArg.pUserData = 0;` |
|      585 | 7249 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      290 | 7250 | `				}` |
|      585 | 7251 | `				break; /* All remaining args consumed */` |
|        - | 7252 | `			}` |
|   273539 | 7253 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 7254 | `				/* An explicit null is NOT redirected to the default (PHP applies a` |
|        - | 7255 | `				 * default only for an omitted arg); it falls through to the type check` |
|        - | 7256 | `				 * below — TypeError for a non-nullable typed param, kept as null for a` |
|        - | 7257 | ``				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE`` |
|        - | 7258 | `				 * at compile time so its check accepts null. */` |
|        - | 7259 | `				/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 7260 | `				 * coercion and whole-real materialization in place — nullable` |
|        - | 7261 | `				 * types (?type) let null through): the shared per-argument` |
|        - | 7262 | `				 * helper, one implementation for both OP_CALL paths and the` |
|        - | 7263 | `				 * generator/fiber binder (§7.1(f) fold). */` |
|   269387 | 7264 | `				iArgPreFlags = pArg->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|   269387 | 7265 | `				rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pArg,bCallIsStrict,pSelfHint);` |
|   269387 | 7266 | `				if( rc != SXRET_OK ){` |
|      287 | 7267 | `					if( rc == PH7_ABORT ){` |
|        6 | 7268 | `						goto Abort;` |
|        - | 7269 | `					}` |
|        - | 7270 | `					/* Skip function body, route through normal cleanup */` |
|      283 | 7271 | `					PH7_MemObjRelease(pTos);` |
|      283 | 7272 | `					pTos = &pTos[-nCallArgs];` |
|      283 | 7273 | `					pFrameStack = 0;` |
|      283 | 7274 | `					rc = PH7_EXCEPTION;` |
|      283 | 7275 | `					goto SkipFuncBody;` |
|        - | 7276 | `				}` |
|   269105 | 7277 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7278 | `					/* Pass by reference */` |
|     7045 | 7279 | `					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){` |
|        - | 7280 | `						/* php 8.1: $GLOBALS cannot be passed by reference —` |
|        - | 7281 | `						 * a catchable Error with php's exact wording. */` |
|        - | 7282 | `						SyBlob sMsg;` |
|        3 | 7283 | `						SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 7284 | `						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|        2 | 7285 | `							&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        3 | 7286 | `						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 7287 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 7288 | `							goto Abort;` |
|        - | 7289 | `						}` |
|        3 | 7290 | `						PH7_MemObjRelease(pTos);` |
|        3 | 7291 | `						pTos = &pTos[-nCallArgs];` |
|        3 | 7292 | `						pFrameStack = 0;` |
|        3 | 7293 | `						rc = PH7_EXCEPTION;` |
|        3 | 7294 | `						goto SkipFuncBody;` |
|        - | 7295 | `					}` |
|     7043 | 7296 | `					if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)n,pArg) ){` |
|        - | 7297 | `						/* php's refusal, decided from the argument's compile-time SHAPE (the` |
|        - | 7298 | `						 * companion of the named-argument binder above; see VmArgRefusedByRef). */` |
|        - | 7299 | `						sxi32 rcRef;` |
|     6028 | 7300 | `						rcRef = VmThrowByRefRefusal(&(*pVm),` |
|     4018 | 7301 | `							(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|     4018 | 7302 | `							&pVmFunc->sName,(sxu32)(n+1),&aFormalArg[n].sName);` |
|     4019 | 7303 | `						if( rcRef == PH7_ABORT ){` |
|      ! 0 | 7304 | `							goto Abort;` |
|        - | 7305 | `						}` |
|        - | 7306 | `						/* Route the throw like every other binder refusal: release the result` |
|        - | 7307 | `						 * slot, pop the actuals and let SkipFuncBody finish the call. Returning` |
|        - | 7308 | `						 * from here walked out of the dispatch loop with the callee's frame and` |
|        - | 7309 | `						 * stack still live, so a CAUGHT refusal silently abandoned every` |
|        - | 7310 | `						 * statement after the catch. */` |
|     4019 | 7311 | `						PH7_MemObjRelease(pTos);` |
|     4019 | 7312 | `						pTos = &pTos[-nCallArgs];` |
|     4019 | 7313 | `						pFrameStack = 0;` |
|     4019 | 7314 | `						rc = PH7_EXCEPTION;` |
|     4019 | 7315 | `						goto SkipFuncBody;` |
|        - | 7316 | `					}` |
|     3025 | 7317 | `					PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)n,pArg);` |
|     3025 | 7318 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - | 7319 | `						/* Nothing to alias: pass by value. */` |
|       99 | 7320 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       51 | 7321 | `					}else{` |
|        - | 7322 | `						SyHashEntry *pRefEntry;` |
|        - | 7323 | `						/* The declared type's conversion is what the reference holds. */` |
|     2929 | 7324 | `						PH7_VmByRefArgWriteBack(&(*pVm),pArg,iArgPreFlags);` |
|        - | 7325 | `						/* Install the referenced variable in the private function frame */` |
|     2929 | 7326 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|     2929 | 7327 | `						if( pRefEntry == 0 ){` |
|     4391 | 7328 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|     2924 | 7329 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|     2929 | 7330 | `							sArg.nIdx = pArg->nIdx;` |
|     2929 | 7331 | `							sArg.pUserData = 0;` |
|     2929 | 7332 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     1462 | 7333 | `						}` |
|     2929 | 7334 | `						pObj = 0;` |
|        - | 7335 | `					}` |
|     1515 | 7336 | `				}else{` |
|        - | 7337 | `					/* Pass by value,make a copy of the given argument */` |
|   262065 | 7338 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 7339 | `				}` |
|   132980 | 7340 | `			}else{` |
|        - | 7341 | `				char zName[32];` |
|        - | 7342 | `				SyString sArgName;` |
|        - | 7343 | `				/* Set a dummy name */` |
|     4157 | 7344 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|     4157 | 7345 | `				sArgName.zString = zName;` |
|        - | 7346 | `				/* Annonymous argument */` |
|     4157 | 7347 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 7348 | `			}` |
|   269237 | 7349 | `			if( pObj ){` |
|   266313 | 7350 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 7351 | `				/* Insert argument index  */` |
|   266313 | 7352 | `				sArg.nIdx = pObj->nIdx;` |
|   266313 | 7353 | `				sArg.pUserData = 0;` |
|   266313 | 7354 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|   133580 | 7355 | `			}` |
|   269237 | 7356 | `			PH7_MemObjRelease(pArg);` |
|   269237 | 7357 | `			pArg++;` |
|   269237 | 7358 | `			++n;` |
|        5 | 7359 | `		}` |
|        - | 7360 | `		} /* end named vs positional branch */` |
|        - | 7361 | `		/* Set up closure environment */` |
|   766311 | 7362 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 7363 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 7364 | `			ph7_value *pValue;` |
|        - | 7365 | `			sxu32 iEnv;` |
|    18668 | 7366 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|    42002 | 7367 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|    23339 | 7368 | `				pEnv = &aEnv[iEnv];` |
|    23339 | 7369 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 7370 | `					/* Do not install null value */` |
|    18124 | 7371 | `					continue;` |
|        - | 7372 | `				}` |
|     5215 | 7373 | `				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       13 | 7374 | `				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|        - | 7375 | `					/* The Closure instance carries an explicit bound $this` |
|        - | 7376 | `					 * (bindTo/bind/call): it wins over the creation-time` |
|        - | 7377 | `					 * captured $this, php-exact. */` |
|        7 | 7378 | `					continue;` |
|        - | 7379 | `				}` |
|     5214 | 7380 | `				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|        - | 7381 | `					/* Captured by reference: link the name to the shared slot` |
|        - | 7382 | `					 * (no copy), mirroring the by-ref argument install above. */` |
|      970 | 7383 | `					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|     1448 | 7384 | `						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),` |
|      965 | 7385 | `							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|      478 | 7386 | `					}` |
|      970 | 7387 | `					continue;` |
|        - | 7388 | `				}` |
|     4249 | 7389 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|     4249 | 7390 | `				if( pValue == 0 ){` |
|      ! 0 | 7391 | `					continue;` |
|        - | 7392 | `				}` |
|        - | 7393 | `				/* Invalidate any prior representation */` |
|     4249 | 7394 | `				PH7_MemObjRelease(pValue);` |
|        - | 7395 | `				/* Duplicate bound variable value */` |
|     4249 | 7396 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|     2127 | 7397 | `			}` |
|     9327 | 7398 | `		}` |
|        - | 7399 | `		/* Too-few-arguments check, placed AFTER the passed arguments were` |
|        - | 7400 | `		 * installed and type-checked: php's RECV order means a type error on` |
|        - | 7401 | ``		 * a PASSED argument beats the count error (`f(int $x,$y)` called`` |
|        - | 7402 | `		 * f("str") is a TypeError, not ArgumentCountError). The passed args` |
|        - | 7403 | `		 * were already released by the install loop, so the standard throw` |
|        - | 7404 | `		 * exit leaks nothing. The named path never fires this (its per-hole` |
|        - | 7405 | `		 * check ran in-loop; n == nNonVariadic >= nRequired here).` |
|        - | 7406 | `		 * Builtins written as embedded PHP in the prelude (VM_FUNC_INTERNAL,` |
|        - | 7407 | `		 * with or without VM_FUNC_CLASS_METHOD) are NOT exempt: their declared` |
|        - | 7408 | `		 * signatures were swept against the php 8.5 oracle, so the derived` |
|        - | 7409 | `		 * required count and wording are php's, and VmThrowBuiltinTooFewArgs` |
|        - | 7410 | `		 * words them as php words an internal callable. */` |
|   766311 | 7411 | `		if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 7412 | `			sxu32 nNonVar,nReq;` |
|     5881 | 7413 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     5881 | 7414 | `			if( n < nReq ){` |
|       57 | 7415 | `				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;` |
|       57 | 7416 | `				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|       52 | 7417 | `					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       17 | 7418 | `						nPassed,nReq,SySetUsed(&pVmFunc->aArgs));` |
|       18 | 7419 | `				}else{` |
|       33 | 7420 | `					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       10 | 7421 | `						nPassed,nReq,nNonVar,TRUE);` |
|        - | 7422 | `				}` |
|       57 | 7423 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 7424 | `					goto Abort;` |
|        - | 7425 | `				}` |
|       57 | 7426 | `				PH7_MemObjRelease(pTos);` |
|       57 | 7427 | `				pTos = &pTos[-nCallArgs];` |
|       57 | 7428 | `				pFrameStack = 0;` |
|       57 | 7429 | `				rc = PH7_EXCEPTION;` |
|       57 | 7430 | `				goto SkipFuncBody;` |
|        5 | 7431 | `			}` |
|   763345 | 7432 | `		}else if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) == VM_FUNC_INTERNAL ){` |
|        - | 7433 | `			/* Too-MANY arguments, the mirror php enforces for an internal` |
|        - | 7434 | `			 * callable ("count_chars() expects at most 2 arguments, 3 given").` |
|        - | 7435 | `			 * PHL accepted the extras silently — the surplus only ever reached` |
|        - | 7436 | `			 * func_get_args(). Every formal is filled on this branch, so the call` |
|        - | 7437 | `			 * is over the maximum exactly when more actuals arrived than there are` |
|        - | 7438 | `			 * non-variadic formals; a VARIADIC tail means php declares no maximum` |
|        - | 7439 | `			 * at all, so nNonVar below the formal count is exempt. Prelude` |
|        - | 7440 | `			 * FUNCTIONS only: the chunk-backed class METHODS were not swept against` |
|        - | 7441 | `			 * php's signatures (Fiber::start() is declared argless and reads` |
|        - | 7442 | `			 * func_get_args()). */` |
|        - | 7443 | `			sxu32 nNonVar,nReq;` |
|      573 | 7444 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|      568 | 7445 | `			if( nNonVar == SySetUsed(&pVmFunc->aArgs)` |
|      567 | 7446 | `			 && pFrame->nActualArgs >= 0` |
|      571 | 7447 | `			 && (sxu32)pFrame->nActualArgs > nNonVar ){` |
|       40 | 7448 | `				rc = VmThrowBuiltinTooManyArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       26 | 7449 | `					(sxu32)pFrame->nActualArgs,nNonVar,nReq);` |
|       27 | 7450 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 7451 | `					goto Abort;` |
|        - | 7452 | `				}` |
|       27 | 7453 | `				PH7_MemObjRelease(pTos);` |
|       27 | 7454 | `				pTos = &pTos[-nCallArgs];` |
|       27 | 7455 | `				pFrameStack = 0;` |
|       27 | 7456 | `				rc = PH7_EXCEPTION;` |
|       27 | 7457 | `				goto SkipFuncBody;` |
|        - | 7458 | `			}` |
|      271 | 7459 | `		}` |
|        - | 7460 | `		/* Process default values for remaining formal parameters */` |
|   780747 | 7461 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    15167 | 7462 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 7463 | `				/* Variadic parameter with no extra args — create empty array */` |
|      651 | 7464 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      651 | 7465 | `				if( pObj ){` |
|      651 | 7466 | `					PH7_MemObjToHashmap(pObj);` |
|      651 | 7467 | `					sArg.nIdx = pObj->nIdx;` |
|      651 | 7468 | `					sArg.pUserData = 0;` |
|      651 | 7469 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      323 | 7470 | `				}` |
|      651 | 7471 | `				n++;` |
|      651 | 7472 | `				break; /* Variadic is always last */` |
|        - | 7473 | `			}` |
|    14521 | 7474 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|    14521 | 7475 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|    14521 | 7476 | `				if( pObj ){` |
|        - | 7477 | `					/* Evaluate the default value and extract it's result */` |
|    14521 | 7478 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|    14521 | 7479 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 7480 | `						goto Abort;` |
|        - | 7481 | `					}` |
|        - | 7482 | `					/* Insert argument index */` |
|    14521 | 7483 | `					sArg.nIdx = pObj->nIdx;` |
|    14521 | 7484 | `					sArg.pUserData = 0;` |
|    14521 | 7485 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 7486 | `					/* Make sure the default argument is of the correct type.` |
|        - | 7487 | ``					 * A null default on an implicitly-nullable param (`int $x = null`)`` |
|        - | 7488 | `					 * must stay null — casting it to 0/""/false would diverge from PHP` |
|        - | 7489 | `					 * and contradict the explicit-null path, which now keeps it null. */` |
|    14516 | 7490 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1828 | 7491 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)` |
|      921 | 7492 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 7493 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 7494 | `						/* Cast to the desired type */` |
|      ! 0 | 7495 | `						xCast(pObj);` |
|      ! 0 | 7496 | `					}else{` |
|        - | 7497 | `						/* Mask matched — a const-indirected whole-real default` |
|        - | 7498 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|    14521 | 7499 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 7500 | `					}` |
|     7256 | 7501 | `				}` |
|     7256 | 7502 | `			}` |
|    14521 | 7503 | `			++n;` |
|        5 | 7504 | `		}` |
|        - | 7505 | `		} /* end VmCallArgMap scope */` |
|        - | 7506 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 7507 | `		 * does not return anything.` |
|        - | 7508 | `		 */` |
|   766231 | 7509 | `		PH7_MemObjRelease(pTos);` |
|   766231 | 7510 | `		pTos = &pTos[-nCallArgs];` |
|        - | 7511 | `		/* Allocate an operand stack (via the recycling allocator) and evaluate the` |
|        - | 7512 | `		 * function body. Size it to a tight static bound when the body is statically` |
|        - | 7513 | `		 * modelable (BYTECODE.md stage 7) — the big memory win for deep recursion,` |
|        - | 7514 | `		 * where one such stack lives per frame — falling back to the safe` |
|        - | 7515 | `		 * instruction-count bound otherwise.` |
|        - | 7516 | `		 *` |
|        - | 7517 | `		 * The bound is computed LAZILY on the first call and cached on the func` |
|        - | 7518 | `		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:` |
|        - | 7519 | `		 * self-computing on first use is fail-safe against any body-creation path` |
|        - | 7520 | `		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),` |
|        - | 7521 | `		 * where undersizing is a heap overflow; the amortized cost is one analysis` |
|        - | 7522 | `		 * per function. */` |
|        - | 7523 | `		{` |
|   766231 | 7524 | `			sxu32 nSlots = pVmFunc->nMaxStack;` |
|   766231 | 7525 | `			if( nSlots == 0 ){` |
|    14343 | 7526 | `				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);` |
|    21511 | 7527 | `				sxu32 nTight = VmComputeMaxStack(&(*pVm),` |
|    14338 | 7528 | `					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);` |
|    14343 | 7529 | `				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;` |
|    14343 | 7530 | `				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */` |
|    14343 | 7531 | `				pVmFunc->nMaxStack = nSlots;` |
|     7168 | 7532 | `			}` |
|   766231 | 7533 | `			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);` |
|        - | 7534 | `		}` |
|   766231 | 7535 | `		if( pFrameStack == 0 ){` |
|        - | 7536 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 7537 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 7538 | `				&pVmFunc->sName);` |
|      ! 0 | 7539 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 7540 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7541 | `			}` |
|      ! 0 | 7542 | `			break;` |
|        - | 7543 | `		}` |
|   382896 | 7544 | `SkipFuncBody:` |
|   770709 | 7545 | `		if( pSelf ){` |
|        - | 7546 | `			/* Push class name */` |
|   505793 | 7547 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|   252894 | 7548 | `		}` |
|        - | 7549 | `		/* Increment nesting level */` |
|   770709 | 7550 | `		pVm->nRecursionDepth++;` |
|   770709 | 7551 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 7552 | `			/* Arg-binding threw: there is no body to run — finish the call` |
|        - | 7553 | `			 * immediately (no record is pushed). */` |
|        - | 7554 | `			VmCallRecord sCallee;` |
|     4483 | 7555 | `			sCallee.pVmFunc = pVmFunc;` |
|     4483 | 7556 | `			sCallee.pFrame = pFrame;` |
|     4483 | 7557 | `			sCallee.pFrameStack = pFrameStack;` |
|     4483 | 7558 | `			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|     4483 | 7559 | `			sCallee.nLastRef = SXU32_HIGH;` |
|     4483 | 7560 | `			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|     4483 | 7561 | `			sState.pTos = pTos;` |
|     4483 | 7562 | `			sState.pc = pc;` |
|     4483 | 7563 | `			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);` |
|     4483 | 7564 | `			pTos = sState.pTos;` |
|     4483 | 7565 | `			pc = sState.pc;` |
|     4483 | 7566 | `			if( rc == PH7_ABORT ){` |
|        - | 7567 | `				/* Abort processing immeditaley */` |
|      ! 0 | 7568 | `				goto Abort;` |
|     4483 | 7569 | `			}else if( rc == PH7_SUSPEND ){` |
|      ! 0 | 7570 | `				goto Suspend;` |
|     4483 | 7571 | `			}else if( rc == PH7_EXCEPTION ){` |
|      165 | 7572 | `				goto Exception;` |
|        - | 7573 | `			}` |
|     2164 | 7574 | `		}else{` |
|        - | 7575 | `			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a` |
|        - | 7576 | `			 * call record (caller activation + in-flight call) and switch the` |
|        - | 7577 | `			 * loop's locals to the callee — a PHP->PHP call no longer grows` |
|        - | 7578 | `			 * the native stack. The record node is pool-allocated so` |
|        - | 7579 | `			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */` |
|   766231 | 7580 | `			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   766231 | 7581 | `			if( pRec ){` |
|   764042 | 7582 | `				pVm->pIdleCallFrames = (void *)pRec->pPrev;` |
|   382241 | 7583 | `			}else{` |
|     2194 | 7584 | `				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));` |
|        - | 7585 | `			}` |
|   766231 | 7586 | `			if( pRec == 0 ){` |
|        - | 7587 | `				/* OOM: undo the push-time accounting, tear the call down and` |
|        - | 7588 | `				 * raise the non-catchable fatal (the §3.1 OOM convention —` |
|        - | 7589 | `				 * never a silent NULL). */` |
|      ! 0 | 7590 | `				pVm->nRecursionDepth--;` |
|      ! 0 | 7591 | `				if( pSelf ){` |
|      ! 0 | 7592 | `					(void)SySetPop(&pVm->aSelf);` |
|      ! 0 | 7593 | `				}` |
|      ! 0 | 7594 | `				SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|      ! 0 | 7595 | `				VmLeaveFrame(&(*pVm));` |
|      ! 0 | 7596 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 7597 | `				goto Abort;` |
|        - | 7598 | `			}` |
|   766231 | 7599 | `			sState.pTos = pTos;` |
|   766231 | 7600 | `			sState.pc = pc;` |
|   766231 | 7601 | `			pRec->sCaller = sState;` |
|   766231 | 7602 | `			pRec->sCall.pVmFunc = pVmFunc;` |
|   766231 | 7603 | `			pRec->sCall.pFrame = pFrame;` |
|   766231 | 7604 | `			pRec->sCall.pFrameStack = pFrameStack;` |
|   766231 | 7605 | `			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|   766231 | 7606 | `			pRec->sCall.nLastRef = SXU32_HIGH;` |
|   766231 | 7607 | `			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|   766231 | 7608 | `			pRec->pPrev = pCallTop;` |
|   766231 | 7609 | `			pCallTop = pRec;` |
|        - | 7610 | `			/* Switch to the callee activation (what the recursive` |
|        - | 7611 | `			 * VmByteCodeExec entry used to set up). */` |
|   766231 | 7612 | `			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);` |
|   766231 | 7613 | `			pStack = pFrameStack;` |
|   766231 | 7614 | `			pTos = &pStack[-1];` |
|   766231 | 7615 | `			pc = 0;` |
|   766231 | 7616 | `			sState.aInstr = aInstr;` |
|   766231 | 7617 | `			sState.pStack = pStack;` |
|   766231 | 7618 | `			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */` |
|   766231 | 7619 | `			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */` |
|   766231 | 7620 | `			sState.pTos = pTos;` |
|   766231 | 7621 | `			sState.pc = 0;` |
|   766231 | 7622 | `			sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|   766231 | 7623 | `			sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|   766231 | 7624 | `			sState.pEntryFrame = pVm->pFrame;` |
|   766231 | 7625 | `			sState.pResult = pRec->sCaller.pTos;` |
|   766231 | 7626 | `			sState.pLastRef = &pRec->sCall.nLastRef;` |
|   766231 | 7627 | `			sState.pEnforceRetFunc = VmFuncHasReturnType(pVmFunc) ? pVmFunc : 0;` |
|   766231 | 7628 | `			sState.is_callback = 0;` |
|   766231 | 7629 | `			sState.bReturnPropagates = 0;` |
|   766231 | 7630 | `			goto VmLoopFetch;` |
|        - | 7631 | `		}` |
|     2164 | 7632 | `	}else{` |
|        - | 7633 | `		/* Look for an installed foreign function.` |
|        - | 7634 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 7635 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 7636 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 7637 | `		 * global fallback for unqualified function calls in namespaces. */` |
|  1695469 | 7638 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 7639 | `		{` |
|  1695469 | 7640 | `		VmCallArgMap *pCallMap2 = pEffCallMap;` |
|  1695469 | 7641 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 7642 | `			/* Compiler-qualified: try short name as global fallback */` |
|       57 | 7643 | `			const char *zShort = sName.zString;` |
|        - | 7644 | `			sxu32 i;` |
|      857 | 7645 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      805 | 7646 | `				if( sName.zString[i] == '\\' ){` |
|       71 | 7647 | `					zShort = &sName.zString[i + 1];` |
|       33 | 7648 | `				}` |
|      405 | 7649 | `			}` |
|       57 | 7650 | `			if( zShort != sName.zString ){` |
|       57 | 7651 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       57 | 7652 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       26 | 7653 | `			}` |
|       26 | 7654 | `		}` |
|        - | 7655 | `		} /* end VmCallArgMap namespace scope */` |
|  1695469 | 7656 | `		if( pEntry == 0 ){` |
|        - | 7657 | `			/* php accepts the "Class::method" STATIC-callable string everywhere a` |
|        - | 7658 | `			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).` |
|        - | 7659 | `			 * Split on the LAST "::" (php's own zend_memrchr scan) and route through the` |
|        - | 7660 | `			 * shared array-callable machinery ([class-name, method-name]) instead of` |
|        - | 7661 | `			 * warning undefined. */` |
|   240140 | 7662 | `			const char *zCbCls = 0,*zCbMeth = 0;` |
|   240140 | 7663 | `			sxu32 nCbCls = 0,nCbMeth = 0;` |
|   240140 | 7664 | `			int bScoped = PH7_VmCallableStringParts(sName.zString,sName.nByte,` |
|        - | 7665 | `				&zCbCls,&nCbCls,&zCbMeth,&nCbMeth);` |
|   240140 | 7666 | `			if( bScoped ){` |
|        - | 7667 | `				/* Resolve BEFORE dispatching: the shared dispatcher answers SXRET_OK with` |
|        - | 7668 | `` 				 * a NULL result for a pair it cannot resolve, so `$cb='C::nosuch'; $cb();` `` |
|        - | 7669 | `				 * evaluated to NULL with no diagnostic at all — where php throws the same` |
|        - | 7670 | `				 * Errors the ARRAY form of the same call already raised here. (Its own` |
|        - | 7671 | ``				 * `if( bScoped )` block: this scratch buffer and the dispatch block's`` |
|        - | 7672 | `				 * hashmap are both block-head declarations, and the check runs between` |
|        - | 7673 | `				 * them.) */` |
|        - | 7674 | `				char zSmMsg[192];` |
|   100080 | 7675 | `				sxi32 nSmBrc = pVm->nBoundaryRc;` |
|   100080 | 7676 | `				const void *pSmRes = (const void *)pVm->pResumeFrame;` |
|   150118 | 7677 | `				const char *zSmErr = VmCallableClassMethodError(&(*pVm),` |
|    50038 | 7678 | `					PH7_VmExtractClass(&(*pVm),zCbCls,nCbCls,FALSE,0),` |
|    50038 | 7679 | `					zCbCls,nCbCls,zCbMeth,nCbMeth,TRUE,zSmMsg,sizeof(zSmMsg));` |
|   100080 | 7680 | `				if( zSmErr ){` |
|        - | 7681 | `					sxi32 rcSmErr;` |
|       53 | 7682 | `					int bSmRaised = PH7_VmClassLookupRaised(&(*pVm),nSmBrc,pSmRes);` |
|       53 | 7683 | `					if( pInstr->iP2 ){` |
|      ! 0 | 7684 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 7685 | `					}` |
|       53 | 7686 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 7687 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7688 | `					}` |
|       53 | 7689 | `					PH7_MemObjRelease(pTos);` |
|       53 | 7690 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       53 | 7691 | `					pTos->nIdx = SXU32_HIGH;` |
|       53 | 7692 | `					if( bSmRaised ){` |
|        - | 7693 | `						/* The class name's autoloader threw: land THAT, exactly as the array` |
|        - | 7694 | `						 * form does — php never reports the class missing in this case. */` |
|        6 | 7695 | `						rcSmErr = pVm->nBoundaryRc;` |
|        6 | 7696 | `						pVm->nBoundaryRc = 0;` |
|        6 | 7697 | `						if( rcSmErr == PH7_ABORT ){` |
|      ! 0 | 7698 | `							goto Abort;` |
|        - | 7699 | `						}` |
|        6 | 7700 | `						rc = PH7_EXCEPTION;` |
|       14 | 7701 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7702 | `					}` |
|       48 | 7703 | `					rcSmErr = VmThrowFromVm(&(*pVm),"Error",zSmErr,(sxu32)SyStrlen(zSmErr));` |
|       48 | 7704 | `					if( rcSmErr == SXERR_ABORT ){` |
|      ! 0 | 7705 | `						goto Abort;` |
|        - | 7706 | `					}` |
|       48 | 7707 | `					rc = rcSmErr;` |
|       64 | 7708 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7709 | `				}` |
|    50013 | 7710 | `			}` |
|   240090 | 7711 | `			if( bScoped ){` |
|        - | 7712 | `				/* Resolved: hand the callable STRING itself to the shared dispatcher, which` |
|        - | 7713 | ``				 * decodes `Class::method` the same way (it has to, for the callback-argument`` |
|        - | 7714 | `				 * callers). This used to build a throwaway [class,method] map here and enter` |
|        - | 7715 | `				 * through the hashmap branch — two decoders for one spelling. */` |
|        - | 7716 | `				ph7_value sResult;` |
|        - | 7717 | `				sxi32 rcSm;` |
|   150041 | 7718 | `				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100026 | 7719 | `					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|   100028 | 7720 | `				SySetReset(&aArg);` |
|   100044 | 7721 | `				while( pArg < pTos ){` |
|       17 | 7722 | `					SySetPut(&aArg,(const void *)&pArg);` |
|       17 | 7723 | `					pArg++;` |
|        1 | 7724 | `				}` |
|   100028 | 7725 | `				PH7_MemObjInit(pVm,&sResult);` |
|   150041 | 7726 | `				rcSm = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),` |
|   100026 | 7727 | `					(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|   100028 | 7728 | `				SySetReset(&aArg);` |
|   100028 | 7729 | `				if( nCallArgs > 0 ){` |
|       15 | 7730 | `					VmPopOperand(&pTos,nCallArgs);` |
|        7 | 7731 | `				}` |
|   100028 | 7732 | `				if( rcSm == PH7_ABORT ){` |
|      ! 0 | 7733 | `					PH7_MemObjRelease(&sResult);` |
|      ! 0 | 7734 | `					goto Abort;` |
|        - | 7735 | `				}` |
|   100028 | 7736 | `				if( rcSm == PH7_EXCEPTION ){` |
|        - | 7737 | `					sxi32 iResumePc;` |
|   100004 | 7738 | `					PH7_MemObjRelease(&sResult);` |
|   100004 | 7739 | `					if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100001 | 7740 | `						PH7_MemObjRelease(pTos);` |
|        - | 7741 | `						/* Drain the abandoned outer-expression operands` |
|        - | 7742 | ``						 * (`1 + "C::m"()`) to the try's base — one leaked`` |
|        - | 7743 | `						 * slot per caught throw otherwise. */` |
|   300001 | 7744 | `						PH7_RESUME_DRAIN()` |
|   100001 | 7745 | `						pc = iResumePc;` |
|   100001 | 7746 | `						break;` |
|        - | 7747 | `					}` |
|        3 | 7748 | `					goto Exception;` |
|        - | 7749 | `				}` |
|       25 | 7750 | `				PH7_MemObjStore(&sResult,pTos);` |
|       25 | 7751 | `				PH7_MemObjRelease(&sResult);` |
|       25 | 7752 | `				break;` |
|        - | 7753 | `			}` |
|        - | 7754 | `			/* Call to an undefined function is a catchable Error in php 8 — it does` |
|        - | 7755 | `			 * NOT warn and hand back null and carry on, which is what PH7 did (and` |
|        - | 7756 | `			 * which quietly turned a typo into a null-propagating program). */` |
|        - | 7757 | `			{` |
|        - | 7758 | `			SyBlob sMsg;` |
|   140064 | 7759 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   140064 | 7760 | `			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);` |
|        - | 7761 | `			/* Consume this call's captured spread runs so they don't leak into a` |
|        - | 7762 | `			 * later call (this path never reaches VmBuildEffectiveArgMap). */` |
|   140064 | 7763 | `			if( pInstr->iP2 ){` |
|      ! 0 | 7764 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 7765 | `			}` |
|        - | 7766 | `			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the` |
|        - | 7767 | `			 * compile-time arg count on the stack, and this early exit skips the` |
|        - | 7768 | `			 * arg-building loop that the normal path uses, so popping only iP1 would` |
|        - | 7769 | `			 * strand the expanded elements and corrupt the enclosing expression. */` |
|   140064 | 7770 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 7771 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7772 | `			}` |
|   140064 | 7773 | `			PH7_MemObjRelease(pTos);` |
|   210094 | 7774 | `			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|    70030 | 7775 | `				SyBlobLength(&sMsg));` |
|   140064 | 7776 | `			SyBlobRelease(&sMsg);` |
|   140064 | 7777 | `			if( rc == SXERR_ABORT ){` |
|        6 | 7778 | `				goto Abort;` |
|        - | 7779 | `			}` |
|        - | 7780 | `			/* A catch may have run IN PLACE inside VmThrowFromVm (SXRET_OK +` |
|        - | 7781 | ``			 * recorded resume). An unconditional `goto Exception` here unwound the`` |
|        - | 7782 | `` 			 * exec ANYWAY, so `try { nosuchfn(); } catch (Error $e) {} rest();` `` |
|        - | 7783 | `			 * ran the catch and then silently dropped the rest of the script` |
|        - | 7784 | `			 * (exit 0). Route like every other in-exec throw site. */` |
|   380111 | 7785 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7786 | `			}` |
|        - | 7787 | `		}` |
|  1455333 | 7788 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 7789 | `		/* D1: resolve deferred plain-var arguments against the builtin's by-ref position` |
|        - | 7790 | `		 * mask (derived from its signature). A by-ref out-param (preg_match's $matches, …)` |
|        - | 7791 | `		 * is materialized so PH7_VmStoreArgByRef can write back — this is what keeps a` |
|        - | 7792 | ``		 * DYNAMIC-name by-ref builtin (`$f='preg_match'; $f($p,$s,$m)`) working now that the`` |
|        - | 7793 | `		 * compile-time mask no longer sees it. Every other (by-value) arg warns + passes` |
|        - | 7794 | `		 * NULL, which is also what call_user_func & friends want. pVm->pFrame is the caller. */` |
|        - | 7795 | `		{` |
|  1455333 | 7796 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,pFunc->nByRefMask,0,0,pEffCallMap);` |
|  1455349 | 7797 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 7798 | `		}` |
|        - | 7799 | `		/* Host function (builtin): build the effective spread-key map so the` |
|        - | 7800 | `		 * name-forwarding builtins (call_user_func & friends) relay string keys as` |
|        - | 7801 | `		 * named args, and — critically — so this call's captured runs are consumed.` |
|        - | 7802 | `		 * pArg is the top base here (a builtin call pops no method-name slot). */` |
|  2183794 | 7803 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  1455314 | 7804 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|  1484361 | 7805 | `NativeCall:` |
|        - | 7806 | `		/* A VM_FUNC_NATIVE method joins here, having done the two steps above for` |
|        - | 7807 | `		 * itself: its by-ref mask comes from the same signature machinery, and its` |
|        - | 7808 | `		 * effective arg map was already built (and this call's spread runs already` |
|        - | 7809 | `		 * consumed) on the method path — building it a second time here would` |
|        - | 7810 | `		 * consume them twice. Everything from this point down is shared verbatim:` |
|        - | 7811 | `		 * a native method IS a host call that happens to carry a receiver. */` |
|        - | 7812 | `		/* Start collecting function arguments */` |
|  2970365 | 7813 | `		SySetReset(&aArg);` |
|  7048724 | 7814 | `		while( pArg < pTos ){` |
|  4078364 | 7815 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  4078364 | 7816 | `			pArg++;` |
|        5 | 7817 | `		}` |
|        - | 7818 | `		/* Assume a null return value */` |
|  2970365 | 7819 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 7820 | `		/* Init the call context */` |
|  2970365 | 7821 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 7822 | `		/* Hand the call-site named-argument map to the builtin so name-forwarding` |
|        - | 7823 | `		 * helpers (call_user_func & friends) can relay name: arguments — and the` |
|        - | 7824 | `		 * caller's strict_types mode — to the inner callback. Forwarded whole (not` |
|        - | 7825 | `		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even` |
|        - | 7826 | `		 * when its own call site is purely positional; only the two forwarding` |
|        - | 7827 | `		 * builtins read pArgMap, so this is inert for every other host function. */` |
|  2970365 | 7828 | `		sCtx.pArgMap = pEffCallMap;` |
|        - | 7829 | `		/* Receiver + late-static-binding class for a native METHOD. Both stay 0 for a` |
|        - | 7830 | `		 * plain host function (the locals are initialized once per OP_CALL), so this` |
|        - | 7831 | `		 * is inert on the builtin path. The reference on pNativeOwned belongs to the` |
|        - | 7832 | `		 * caller for the span of the call — the native body borrows it and must not` |
|        - | 7833 | `		 * unref, exactly as a bytecode method's frame $this is borrowed. */` |
|  2970365 | 7834 | `		sCtx.pThis = pNativeRecv;` |
|  2970365 | 7835 | `		sCtx.pCalledClass = pNativeClass;` |
|        - | 7836 | `		{` |
|  2970365 | 7837 | `		int nGiven = (int)SySetUsed(&aArg);` |
|        - | 7838 | ``		/* Bind `name:` arguments to the callee's declared POSITIONS before anything`` |
|        - | 7839 | `		 * reads the vector — the arity screen, the ZPP screen and the C body all take` |
|        - | 7840 | `		 * it positionally. A host function has no compiled parameter records for` |
|        - | 7841 | `		 * VmResolveNamedArgs to walk, so its signature string is the source of names` |
|        - | 7842 | `		 * and defaults (PH7_VmBindNamedArgsToSig). Without this every named argument` |
|        - | 7843 | `		 * simply stayed where it was WRITTEN. */` |
|  2970365 | 7844 | `		if( pEffCallMap && pEffCallMap->bHasNamed && nGiven > 0 ){` |
|      143 | 7845 | `			rc = PH7_VmBindNamedArgsToSig(&sCtx,pFunc,pEffCallMap,&nGiven,` |
|       94 | 7846 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|       96 | 7847 | `			if( rc != SXRET_OK ){` |
|        9 | 7848 | `				goto NativeCallDone;` |
|        - | 7849 | `			}` |
|       43 | 7850 | `		}` |
|        - | 7851 | `		/* php binds a by-reference argument at the CALL, before the callee runs, so a` |
|        - | 7852 | ``		 * non-variable in a `&` position is refused ahead of every ZPP check — and`` |
|        - | 7853 | ``		 * ahead of the too-MANY-arguments one (`array_pop([1,2],5)` is the reference`` |
|        - | 7854 | `		 * Error in php, not an ArgumentCountError). With no argument at all there is` |
|        - | 7855 | `		 * nothing to refuse, which is why the too-FEW check below still speaks first` |
|        - | 7856 | ``		 * for `array_pop()`. */`` |
|  4456352 | 7857 | `		rc = PH7_VmScreenByRefArgShapes(&sCtx,pFunc,pEffCallMap,nGiven,` |
|  2970352 | 7858 | `			(ph7_value **)SySetBasePtr(&aArg));` |
|  2970357 | 7859 | `		if( rc != SXRET_OK ){` |
|       55 | 7860 | `			goto NativeCallDone;` |
|        - | 7861 | `		}` |
|        - | 7862 | `		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum` |
|        - | 7863 | `		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError` |
|        - | 7864 | `		 * before the C routine runs when called with too few arguments — instead` |
|        - | 7865 | `		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""` |
|        - | 7866 | `		 * return. The message wording matches php's ZPP output byte-for-byte. */` |
|  4456277 | 7867 | `		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){` |
|      881 | 7868 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 7869 | `				"%z() expects %s %d argument%s, %d given",` |
|      292 | 7870 | `				&pFunc->sName,` |
|      584 | 7871 | `				pFunc->bAtLeast ? "at least" : "exactly",` |
|      584 | 7872 | `				(int)pFunc->nMinArg,` |
|      584 | 7873 | `				pFunc->nMinArg == 1 ? "" : "s",` |
|      292 | 7874 | `				nGiven);` |
|  2970015 | 7875 | `		}else if( pFunc->bHasMaxArg && nGiven > (int)pFunc->nMaxArg ){` |
|        - | 7876 | `			/* php enforces the MAXIMUM as well, and PHL only did so where a` |
|        - | 7877 | `			 * builtin happened to hand-roll the check (51 of ~650), so` |
|        - | 7878 | ``			 * `microtime(1,2)`, `strlen("a","b")` and friends silently ignored`` |
|        - | 7879 | `			 * the extras. The count comes from the same signature table as the` |
|        - | 7880 | `			 * minimum; a variadic tail leaves nMaxArg at -1 and is exempt.` |
|        - | 7881 | `			 * php says "exactly" when the bounds coincide, "at most" otherwise. */` |
|      230 | 7882 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 7883 | `				"%z() expects %s %d argument%s, %d given",` |
|       75 | 7884 | `				&pFunc->sName,` |
|      124 | 7885 | `				((int)pFunc->nMinArg == (int)pFunc->nMaxArg && !pFunc->bAtLeast) ? "exactly" : "at most",` |
|      150 | 7886 | `				(int)pFunc->nMaxArg,` |
|      150 | 7887 | `				pFunc->nMaxArg == 1 ? "" : "s",` |
|       75 | 7888 | `				nGiven);` |
|  4455251 | 7889 | `		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,` |
|  2969568 | 7890 | `			(ph7_value **)SySetBasePtr(&aArg))) ){` |
|        - | 7891 | `			/* TypeError thrown: rc carries the caught/uncaught status */` |
|      518 | 7892 | `		}else{` |
|        - | 7893 | `			/* The name of the builtin that is RUNNING, for the few diagnostics` |
|        - | 7894 | `			 * raised so deep inside the engine that no ph7_context reaches them` |
|        - | 7895 | `			 * (a stream filter's, from inside a device read) and which php still` |
|        - | 7896 | `			 * prefixes with the caller. Saved and restored: a builtin can call` |
|        - | 7897 | `			 * back into php and reach this line again. */` |
|  2968547 | 7898 | `			SyString *pSavedCallee = pVm->pCalleeName;` |
|  2968547 | 7899 | `			pVm->pCalleeName = &pFunc->sName;` |
|        - | 7900 | `			/* Call the foreign function */` |
|  2968547 | 7901 | `			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));` |
|  2968547 | 7902 | `			pVm->pCalleeName = pSavedCallee;` |
|        - | 7903 | `			/* A host function that RAISED a catchable throw (PH7_VmThrowException)` |
|        - | 7904 | `			 * and still returned PH7_OK reports it here — the throw's catch has` |
|        - | 7905 | `			 * already run in place, so treating the call as a normal return would` |
|        - | 7906 | `			 * resume execution INSIDE the try body the throw abandoned (and, when` |
|        - | 7907 | `			 * uncaught, run on past the reported fatal). The status is recorded on` |
|        - | 7908 | `			 * the call context, so this covers the shared validation helpers whose` |
|        - | 7909 | `			 * callers have no channel to thread a status back. */` |
|  2968547 | 7910 | `			rc = VmHostFuncThrowRc(&sCtx,rc);` |
|        - | 7911 | `		}` |
|  1484361 | 7912 | `NativeCallDone:` |
|  1485999 | 7913 | `		(void)nGiven; /* the named-arg binder's early exit lands here */` |
|        - | 7914 | `		}` |
|        - | 7915 | `		/* Release the call context */` |
|  2970365 | 7916 | `		VmReleaseCallContext(&sCtx);` |
|  2970365 | 7917 | `		if( pNativeOwned ){` |
|        - | 7918 | `			/* Drop the receiver reference the method branch took (pThis->iRef++ before` |
|        - | 7919 | `			 * the target slot was released). A bytecode method hands this to its frame` |
|        - | 7920 | `			 * and lets the teardown do it; a native method has no frame, so it is` |
|        - | 7921 | `			 * dropped here — the one point EVERY route out of the call passes through,` |
|        - | 7922 | `			 * including the throw/abort/suspend ones below. Ordered after the context` |
|        - | 7923 | `			 * release because sCtx.sThis aliases the instance. Always 0 for a plain` |
|        - | 7924 | `			 * host function. */` |
|  1513877 | 7925 | `			PH7_ClassInstanceUnref(pNativeOwned);` |
|  1513877 | 7926 | `			pNativeOwned = 0;` |
|  1513877 | 7927 | `			pNativeRecv = 0;` |
|   756937 | 7928 | `		}` |
|  2970365 | 7929 | `		if( rc == PH7_ABORT ){` |
|        - | 7930 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 7931 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 7932 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      677 | 7933 | `			PH7_MemObjRelease(&sRet);` |
|      677 | 7934 | `			goto Abort;` |
|        - | 7935 | `		}` |
|  2969693 | 7936 | `		if( rc != PH7_SUSPEND && pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 7937 | `			/* A throw raised inside this host function — directly` |
|        - | 7938 | `			 * (PH7_VmThrowException) or by a PHP callback it invoked — was` |
|        - | 7939 | `			 * caught by an INLINE try (generator body) THIS exec owns.` |
|        - | 7940 | `			 * VmThrowInline records only a pc-redirect: a direct builtin throw` |
|        - | 7941 | `			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION` |
|        - | 7942 | `			 * with the redirect pending, so the rc branches below never land` |
|        - | 7943 | `			 * it (pre-existing hole: explode("") or a throwing usort` |
|        - | 7944 | `			 * comparator inside a generator's try lost the catch AND the` |
|        - | 7945 | `			 * yield). Land at the redirect now — its drain to the try's` |
|        - | 7946 | `			 * operand base subsumes the args + name pops. */` |
|       10 | 7947 | `			PH7_MemObjRelease(&sRet);` |
|       32 | 7948 | `			PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 7949 | `		}` |
|  2969685 | 7950 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 7951 | `			/* A callback invoked by this host function threw. If an in-place catch` |
|        - | 7952 | `			 * recorded a resume target owned by THIS body, resume at its landing pad` |
|        - | 7953 | `			 * (consuming the target); otherwise the exception was caught by an outer` |
|        - | 7954 | `			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW` |
|        - | 7955 | `			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which` |
|        - | 7956 | `			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */` |
|        - | 7957 | `			sxi32 iResumePc;` |
|     9276 | 7958 | `			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 7959 | `				/* Caught by an outer exec, or not caught here: propagate. */` |
|     2572 | 7960 | `				goto Exception;` |
|        - | 7961 | `			}` |
|        - | 7962 | `			/* Exception was caught in place by THIS body's try: pop args and the` |
|        - | 7963 | `			 * result slot, then drain any abandoned outer-expression operands to` |
|        - | 7964 | `			 * the try's base and resume. */` |
|     6709 | 7965 | `			PH7_MemObjRelease(&sRet);` |
|     6709 | 7966 | `			if( nCallArgs > 0 ){` |
|     6231 | 7967 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|     3113 | 7968 | `			}` |
|     6709 | 7969 | `			VmPopOperand(&pTos,1);` |
|    10935 | 7970 | `			PH7_RESUME_DRAIN()` |
|     6709 | 7971 | `			pc = iResumePc;` |
|     6709 | 7972 | `			break;` |
|        - | 7973 | `		}` |
|  2960414 | 7974 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 7975 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 7976 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 7977 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 7978 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 7979 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 7980 | `			 * body), the user-function path above will handle re-saving. */` |
|      367 | 7981 | `			PH7_MemObjRelease(&sRet);` |
|      367 | 7982 | `			if( nCallArgs > 0 ){` |
|      359 | 7983 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      177 | 7984 | `			}` |
|        - | 7985 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 7986 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|      367 | 7987 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|      367 | 7988 | `			goto Suspend;` |
|        - | 7989 | `		}` |
|  2960052 | 7990 | `		if( nCallArgs > 0 ){` |
|        - | 7991 | `			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack` |
|        - | 7992 | `			 * expanded the compile-time arg count on the stack, so popping iP1` |
|        - | 7993 | `			 * strands the extra elements (or, for an unpack that expanded to fewer` |
|        - | 7994 | `			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,` |
|        - | 7995 | `			 * reading a bogus value whose stray flags sent MemObjStore into the` |
|        - | 7996 | `			 * hashmap-release path and hung). Mirrors every other CALL exit. The` |
|        - | 7997 | `			 * function-name slot (pTos) receives the return value below. */` |
|  2900172 | 7998 | `			VmPopOperand(&pTos,nCallArgs);` |
|  1450903 | 7999 | `		}` |
|        - | 8000 | `		/* Save foreign function return value into the (now top) function-name slot */` |
|  2960052 | 8001 | `		PH7_MemObjStore(&sRet,pTos);` |
|        - | 8002 | `		/* ...and clear that slot's index. It is one of the call's own argument slots,` |
|        - | 8003 | `		 * still carrying the variable index the argument was loaded with, and` |
|        - | 8004 | `		 * PH7_MemObjStore does not touch nIdx — so a builtin's return value came back` |
|        - | 8005 | ``		 * looking like an lvalue for the caller's variable (`f(strtoupper($b))` with`` |
|        - | 8006 | ``		 * `function f(&$x)` overwrote `$b`). No host function returns by reference. */`` |
|  2960052 | 8007 | `		pTos->nIdx = SXU32_HIGH;` |
|  2960052 | 8008 | `		PH7_MemObjRelease(&sRet);` |
|        - | 8009 | `	}` |
|  2964370 | 8010 | `	break;` |
|        - | 8011 | `				  }` |
|        - | 8012 | `/*` |
|        - | 8013 | ` * OP_CONSUME: P1 * *` |
|        - | 8014 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 8015 | ` */` |
|    61938 | 8016 | `case PH7_OP_CONSUME: {` |
|        - | 8017 | `	VmOpRc rcOp;` |
|   123881 | 8018 | `	sState.pTos = pTos;` |
|   123881 | 8019 | `	sState.pc = pc;` |
|   123881 | 8020 | `	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);` |
|   123881 | 8021 | `	pTos = sState.pTos;` |
|   123881 | 8022 | `	pc = sState.pc;` |
|   123881 | 8023 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 8024 | `		goto Abort;` |
|   123879 | 8025 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       14 | 8026 | `		goto Exception;` |
|        - | 8027 | `	}` |
|   123862 | 8028 | `	break;` |
|        - | 8029 | `					  }` |
|        - | 8030 |  |
|        - | 8031 | `		} /* Switch() */` |
| 43934002 | 8032 | `		pc++; /* Next instruction in the stream */` |
|        5 | 8033 | `	} /* For(;;) */` |
|  1579287 | 8034 | `Done:` |
|        - | 8035 | `	/* A stacked callee completing lands here too (its result is already in` |
|        - | 8036 | `	 * sState.pResult — the caller's operand slot); Unwind's first iteration` |
|        - | 8037 | `	 * bottoms out identically for the record-less case. */` |
|  3158995 | 8038 | `	rc = SXRET_OK;` |
|  3158995 | 8039 | `	goto Unwind;` |
|      842 | 8040 | `Suspend:` |
|     1689 | 8041 | `	rc = PH7_SUSPEND;` |
|     1689 | 8042 | `	if( pCallTop != 0 ){` |
|        - | 8043 | `		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment` |
|        - | 8044 | `		 * instead of the lossy unwind. pc/nTos of the innermost activation were` |
|        - | 8045 | `		 * already saved into the ctx by VmSuspendCtx; capture the rest (the` |
|        - | 8046 | `		 * record chain, the innermost activation, the suspend-time top frame)` |
|        - | 8047 | `		 * so resume re-enters HERE, inside the innermost callee, like php. The` |
|        - | 8048 | `		 * records / frames / operand stacks stay alive — nothing is freed. Only` |
|        - | 8049 | `		 * fibers reach this (generators yield only at their body level, pCallTop` |
|        - | 8050 | `		 * == 0); a suspend inside a C->PHP callback was already rejected with a` |
|        - | 8051 | `		 * FiberError before it could arrive here. */` |
|      306 | 8052 | `		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));` |
|      306 | 8053 | `		if( pSeg == 0 ){` |
|        - | 8054 | `			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST` |
|        - | 8055 | `			 * pc/nTos into the ctx, so the lossy body-level fallback would resume` |
|        - | 8056 | `			 * the callee's pc against the body stack — silent corruption, exactly` |
|        - | 8057 | `			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the` |
|        - | 8058 | `			 * §3.1 convention shared with the stage-2 record-alloc OOM site). */` |
|      ! 0 | 8059 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 8060 | `			rc = PH7_ABORT;` |
|      ! 0 | 8061 | `			goto Unwind;` |
|        - | 8062 | `		}` |
|      306 | 8063 | `		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */` |
|      306 | 8064 | `		pSeg->sState = sState;` |
|      306 | 8065 | `		pSeg->pCallTop = pCallTop;` |
|      306 | 8066 | `		pSeg->pTopFrame = pVm->pFrame;` |
|      306 | 8067 | `		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;` |
|      306 | 8068 | `		pSeg->nOldFinBase = pVm->pActiveCtx ? pVm->pActiveCtx->nFinallyBase : 0;` |
|        - | 8069 | `		{` |
|        - | 8070 | `			VmCallFrame *pRec;` |
|      306 | 8071 | `			pSeg->nRecords = 0;` |
|      608 | 8072 | `			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){` |
|      306 | 8073 | `				pSeg->nRecords++;` |
|      155 | 8074 | `			}` |
|        - | 8075 | `		}` |
|      306 | 8076 | `		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;` |
|        - | 8077 | `		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */` |
|      306 | 8078 | `		SySetRelease(&aArg);` |
|      306 | 8079 | `		return PH7_SUSPEND;` |
|        - | 8080 | `	}` |
|     1387 | 8081 | `	goto Unwind;` |
|      480 | 8082 | `Abort:` |
|      965 | 8083 | `	rc = PH7_ABORT;` |
|      965 | 8084 | `	goto Unwind;` |
|   302367 | 8085 | `Exception:` |
|   604738 | 8086 | `	rc = PH7_EXCEPTION;` |
|   604733 | 8087 | `	goto Unwind;` |
|  1882825 | 8088 | `Unwind:` |
|        - | 8089 | `	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration` |
|        - | 8090 | `	 * finishes the top record exactly as the old per-level native return did:` |
|        - | 8091 | `	 * for ABORT/EXCEPTION, first run what the popped activation's own` |
|        - | 8092 | `	 * Abort/Exception label used to do (clear its pending return, release its` |
|        - | 8093 | `	 * operands — a stacked activation never has bReturnPropagates set), then` |
|        - | 8094 | `	 * VmCallFinish routes in the restored caller (an in-place catch there` |
|        - | 8095 | `	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —` |
|        - | 8096 | `	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible` |
|        - | 8097 | `	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).` |
|        - | 8098 | `	 * At the bottom, VmExecFinalize hands the status to the native caller. */` |
|  2083718 | 8099 | `	for(;;){` |
|  4167026 | 8100 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|        - | 8101 | `			/* Drop any pending hook-RMW write-backs this activation armed — its` |
|        - | 8102 | `			 * statement is abandoned (only the innermost activation at throw time` |
|        - | 8103 | `			 * can own entries: the armed window spans exactly one instruction, so` |
|        - | 8104 | `			 * no OP_CALL record ever intervenes). */` |
|  1006655 | 8105 | `			while( SySetUsed(&pVm->aHookRmw) > 0` |
|  1006656 | 8106 | `			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){` |
|        3 | 8107 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 8108 | `			}` |
|   503324 | 8109 | `		}` |
|  4167026 | 8110 | `		if( pCallTop == 0 ){` |
|  3400998 | 8111 | `			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);` |
|        - | 8112 | `		}` |
|   766033 | 8113 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|   604473 | 8114 | `			VmClearFramePending(sState.pEntryFrame);` |
|   612095 | 8115 | `			while( pTos >= pStack ){` |
|     7627 | 8116 | `				PH7_MemObjRelease(pTos);` |
|     7627 | 8117 | `				pTos--;` |
|        5 | 8118 | `			}` |
|   302234 | 8119 | `		}` |
|   766033 | 8120 | `		if( rc != PH7_SUSPEND ){` |
|        - | 8121 | `			/* The finishing callee's own leaked finally actions must not survive` |
|        - | 8122 | `			 * into the caller (whose next OP_END_FINALLY would mis-pop them). */` |
|   766031 | 8123 | `			VmDiscardFinallyActions(&(*pVm),sState.nFinallyActBase);` |
|   383230 | 8124 | `		}` |
|        - | 8125 | `		{` |
|   766033 | 8126 | `			VmCallFrame *pRec = pCallTop;` |
|   766033 | 8127 | `			sState = pRec->sCaller;` |
|   766033 | 8128 | `			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);` |
|   766033 | 8129 | `			pCallTop = pRec->pPrev;` |
|   766033 | 8130 | `			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   766033 | 8131 | `			pVm->pIdleCallFrames = (void *)pRec;` |
|   766033 | 8132 | `			aInstr = sState.aInstr;` |
|   766033 | 8133 | `			pStack = sState.pStack;` |
|   766033 | 8134 | `			pTos = sState.pTos;` |
|   766033 | 8135 | `			pc = sState.pc;` |
|        - | 8136 | `		}` |
|   766033 | 8137 | `		if( rc == PH7_OK ){` |
|   365073 | 8138 | `			pc++; /* the loop-bottom increment this OP_CALL missed */` |
|   365073 | 8139 | `			goto VmLoopFetch;` |
|        - | 8140 | `		}` |
|        5 | 8141 | `	}` |
|  1700642 | 8142 | `}` |
|        - | 8143 |  |
