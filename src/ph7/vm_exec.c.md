# src/ph7/vm_exec.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4007/4474 lines (89.56%)

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
|   104991 |   45 | `static int VmGrowOperandStack(ph7_vm *pVm, sxu32 nNeed,` |
|        - |   46 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |   47 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        5 |   48 | `{` |
|   104996 |   49 | `	ph7_value *pOld = *ppStack;` |
|   104996 |   50 | `	sxu32 nOldCap = pState->nStackCap;` |
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
|   104996 |   64 | `	nReq = nNeed + pState->nStackOrig;` |
|   104996 |   65 | `	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */` |
|      ! 0 |   66 | `		nReq = SXU32_HIGH;` |
|      ! 0 |   67 | `	}` |
|        - |   68 | `	/* SyMemBackendRealloc's size argument is sxu32, so the byte count` |
|        - |   69 | `	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack` |
|        - |   70 | `	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and` |
|        - |   71 | `	 * the init loop below would run off it. If the true requirement exceeds the` |
|        - |   72 | `	 * representable cap, treat it as OOM (the caller raises the guard error). */` |
|   104996 |   73 | `	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);` |
|   104996 |   74 | `	if( nReq > nMaxCap ){` |
|      ! 0 |   75 | `		return 0;` |
|        - |   76 | `	}` |
|   104996 |   77 | `	if( nReq <= nOldCap ){` |
|   100619 |   78 | `		return 1; /* already fits — no growth needed */` |
|        - |   79 | `	}` |
|     4382 |   80 | `	nNewCap = nReq;` |
|        - |   81 | `	/* Amortize repeated spreads in one argument list: at least double, but never` |
|        - |   82 | `	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't` |
|        - |   83 | `	 * itself overflow.) */` |
|     4382 |   84 | `	if( nNewCap < nOldCap * 2 ){` |
|     4382 |   85 | `		sxu32 nDbl = nOldCap * 2;` |
|     4382 |   86 | `		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }` |
|     4382 |   87 | `		if( nNewCap < nDbl ){ nNewCap = nDbl; }` |
|     2141 |   88 | `	}` |
|     6523 |   89 | `	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,` |
|     2141 |   90 | `		nNewCap * sizeof(ph7_value));` |
|     4382 |   91 | `	if( pNew == 0 ){` |
|      ! 0 |   92 | `		return 0; /* OOM: caller keeps pOld and raises the guard error */` |
|        - |   93 | `	}` |
|        - |   94 | `	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */` |
|   161321 |   95 | `	for( i = nOldCap; i < nNewCap; i++ ){` |
|   156944 |   96 | `		PH7_MemObjInit(pVm, &pNew[i]);` |
|   156944 |   97 | `		pNew[i].nIdx = SXU32_HIGH;` |
|    77097 |   98 | `	}` |
|        - |   99 | `	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */` |
|     4382 |  100 | `	*ppTos = pNew + (*ppTos - pOld);` |
|     4382 |  101 | `	*ppStack = pNew;` |
|     4382 |  102 | `	pState->pStack = pNew + (pState->pStack - pOld);` |
|     4382 |  103 | `	pState->pTos = pNew + (pState->pTos - pOld);` |
|     4382 |  104 | `	pState->nStackCap = nNewCap;` |
|     4382 |  105 | `	if( pCallTop ){` |
|        - |  106 | `		/* This activation is a trampoline callee: its record owns the buffer. */` |
|     4273 |  107 | `		pCallTop->sCall.pFrameStack = pNew;` |
|     4273 |  108 | `		pCallTop->sCall.nStackCap = nNewCap;` |
|     2091 |  109 | `	}else{` |
|        - |  110 | `		/* Base activation: the native entry frees *ppBaseOwner (and, for a` |
|        - |  111 | `		 * resumable coroutine, persists *pnBaseCap across suspend/resume). */` |
|      113 |  112 | `		if( ppBaseOwner ){ *ppBaseOwner = pNew; }` |
|      113 |  113 | `		if( pnBaseCap ){ *pnBaseCap = nNewCap; }` |
|        - |  114 | `	}` |
|        - |  115 | `	/* Re-anchor this activation's captured spread runs (pStart into the old` |
|        - |  116 | `	 * buffer). Enclosing-activation runs live in other buffers — leave them. */` |
|     4382 |  117 | `	nRun = SySetUsed(&pVm->aSpreadRun);` |
|     4382 |  118 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|     4382 |  119 | `	for( i = 0; i < nRun; i++ ){` |
|      ! 0 |  120 | `		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){` |
|      ! 0 |  121 | `			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);` |
|      ! 0 |  122 | `		}` |
|      ! 0 |  123 | `	}` |
|     4382 |  124 | `	return 1;` |
|    52453 |  125 | `}` |
|        - |  126 | `/*` |
|        - |  127 | ` * Ensure the running activation's operand stack can hold a spread of nEntry` |
|        - |  128 | ` * elements pushed at *ppTos (the source slot becomes the first element, so the` |
|        - |  129 | ` * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.` |
|        - |  130 | ` * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.` |
|        - |  131 | ` */` |
|     1305 |  132 | `static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,` |
|        - |  133 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |  134 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        5 |  135 | `{` |
|        - |  136 | `	sxu32 nNeed;` |
|     1310 |  137 | `	if( nEntry == 0 ){` |
|      215 |  138 | `		return 1; /* empty spread never grows the stack */` |
|        - |  139 | `	}` |
|     1098 |  140 | `	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);` |
|     1597 |  141 | `	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,` |
|      499 |  142 | `		pCallTop, ppBaseOwner, pnBaseCap);` |
|      610 |  143 | `}` |
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
|  4530945 |  164 | `static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)` |
|        5 |  165 | `{` |
|  4530950 |  166 | `	if( rcTerm != PH7_SUSPEND ){` |
|  4529266 |  167 | `		VmDiscardFinallyActions(&(*pVm),pState->nFinallyActBase);` |
|  2264442 |  168 | `	}` |
|  4530950 |  169 | `	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){` |
|  3064185 |  170 | `		VmClearFramePending(pState->pEntryFrame);` |
|  1531929 |  171 | `	}` |
|  4530950 |  172 | `	SySetRelease(pArg);` |
|  4530950 |  173 | `	if( rcTerm == PH7_ABORT \|\| rcTerm == PH7_EXCEPTION ){` |
|   615819 |  174 | `		while( pTos >= pState->pStack ){` |
|   309268 |  175 | `			PH7_MemObjRelease(pTos);` |
|   309268 |  176 | `			pTos--;` |
|        5 |  177 | `		}` |
|   153277 |  178 | `	}` |
|  4530950 |  179 | `	return rcTerm;` |
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
|  5336603 |  195 | `static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase)` |
|        5 |  196 | `{` |
|  5336620 |  197 | `	while( SySetUsed(&pVm->aFinallyAction) > nBase ){` |
|       15 |  198 | `		VmFinallyAction *pAct = (VmFinallyAction *)SySetPeek(&pVm->aFinallyAction);` |
|       15 |  199 | `		if( pAct->eKind == PH7_FA_RETHROW && pAct->pExc ){` |
|        9 |  200 | `			PH7_ClassInstanceUnref(pAct->pExc);` |
|       10 |  201 | `		}else if( pAct->eKind == PH7_FA_RETURN ){` |
|        3 |  202 | `			PH7_MemObjRelease(&pAct->sRet);` |
|        1 |  203 | `		}` |
|       15 |  204 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|        3 |  205 | `	}` |
|  5336608 |  206 | `}` |
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
|   811906 |  219 | `static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)` |
|        5 |  220 | `{` |
|        - |  221 | `	/* Decrement nesting level */` |
|   811911 |  222 | `	pVm->nRecursionDepth--;` |
|   811911 |  223 | `	if( pCallee->bSelfPushed ){` |
|        - |  224 | `		/* Pop class name */` |
|   511837 |  225 | `		(void)SySetPop(&pVm->aSelf);` |
|   255916 |  226 | `	}` |
|   811911 |  227 | `	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  228 | `` 		/* Return by reference: the caller binds to the very slot the `return` `` |
|        - |  229 | `		 * named. php keeps that slot alive because the reference holds it, and` |
|        - |  230 | `		 * the slot may well be one the frame about to be torn down owns -- a` |
|        - |  231 | `		 * local, a parameter, or an element of a local array. PIN it here, the` |
|        - |  232 | `		 * way a by-reference closure capture is pinned (VmPinMemObjSlot's own` |
|        - |  233 | `		 * comment records the same trade: php frees by refcount, PHL by a` |
|        - |  234 | `		 * script-lifetime pin). Before this the engine DROPPED the reference for` |
|        - |  235 | `		 * a local (behind two notices php does not have) and left the caller` |
|        - |  236 | `		 * aimed at an element the frame had already released, so` |
|        - |  237 | ``		 * `function &f($x){ $a = [$x]; return $a[0]; }` handed back NULL.`` |
|        - |  238 | `		 * A return with nothing to bind is reported at the RETURN instead, in` |
|        - |  239 | `		 * php's own words -- see the terminal OP_DONE.` |
|        - |  240 | `		 * The pin is what a refcount would be: it never comes back, so a by-ref` |
|        - |  241 | `		 * return naming a fresh frame slot retains it for the run (~0.6 KB a call;` |
|        - |  242 | `		 * PLAN.md 7.1 measures it). A slot that already outlives the frame -- a` |
|        - |  243 | `		 * static, a global, a property -- is pinned harmlessly: the flag only stops` |
|        - |  244 | ``		 * its INDEX being recycled, and `unset($GLOBALS['G'])` still answers php's. */`` |
|       93 |  245 | `		if( pCallee->nLastRef != SXU32_HIGH ){` |
|       86 |  246 | `			VmPinMemObjSlot(&(*pVm),pCallee->nLastRef);` |
|       41 |  247 | `		}` |
|       93 |  248 | `		pCaller->pTos->nIdx = pCallee->nLastRef;` |
|        - |  249 | `		/* The callee PROMISED a reference, whether or not it had one to give:` |
|        - |  250 | `		 * php says nothing further at the call site either way. */` |
|       93 |  251 | `		pCaller->pTos->iFlags \|= MEMOBJ_AUX_REFRET;` |
|       49 |  252 | `	}else{` |
|        - |  253 | `		/* A by-VALUE return is a TEMPORARY — php's IS_TMP_VAR — and must not look like` |
|        - |  254 | `		 * an lvalue. The result lands in the slot the call's first ARGUMENT occupied,` |
|        - |  255 | `		 * which still carried that argument's variable index, so the returned value` |
|        - |  256 | ``		 * inherited it: `f(id($z))` with `function f(&$x)` aliased and overwrote `$z`,`` |
|        - |  257 | `		 * a variable neither function was given by reference. Every call form was` |
|        - |  258 | `		 * affected (function, method, static, closure, nested) and every one of them` |
|        - |  259 | `		 * silently. Clearing it here also lets the call site see the temporary for what` |
|        - |  260 | `		 * it is, which is what php's "Only variables should be passed by reference"` |
|        - |  261 | `		 * notice is raised on. */` |
|   811823 |  262 | `		pCaller->pTos->nIdx = SXU32_HIGH;` |
|        - |  263 | `	}` |
|   811911 |  264 | `	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  265 | `		/* The callee threw (or its finally threw past it). If an in-place catch` |
|        - |  266 | `		 * recorded a resume target owned by THIS caller's body, resume there and` |
|        - |  267 | `		 * consume the target (VmRecordedResume); when the catcher is an outer exec` |
|        - |  268 | `		 * — or this is a callback with no bytecode to resume into — propagate so` |
|        - |  269 | `		 * the owning exec lands. This replaces the old "is the caller's parent a` |
|        - |  270 | `		 * resumable try frame" test, which resumed at the caller's OWN try even` |
|        - |  271 | `		 * when the finally's throw was caught further out, losing that catch's` |
|        - |  272 | `		 * return (ROOT B, face c). */` |
|        - |  273 | `		sxi32 iResumePc;` |
|   611171 |  274 | `		VmFrame *pParentFrame = pCallee->pFrame->pParent;` |
|   611171 |  275 | `		if( !pCaller->is_callback && VmInlineOwnedBy(pVm,pCaller->aInstr,pCaller->pEntryFrame) ){` |
|        - |  276 | `			/* ROOT C: the callee's throw was caught by an inline try in THIS caller` |
|        - |  277 | `			 * (generator body). Drain the operand stack (incl. the unwritten result` |
|        - |  278 | `			 * slot) to the try's base and land at its catch/finally. */` |
|      ! 0 |  279 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iInlineDrain ){` |
|      ! 0 |  280 | `				PH7_MemObjRelease(pCaller->pTos);` |
|      ! 0 |  281 | `				pCaller->pTos--;` |
|      ! 0 |  282 | `			}` |
|      ! 0 |  283 | `			pCaller->pc = (sxi32)pVm->iInlinePc - 1;` |
|      ! 0 |  284 | `			pVm->pInlineInstr = 0;` |
|      ! 0 |  285 | `			pVm->pInlineFrame = 0;` |
|      ! 0 |  286 | `			rc = PH7_OK;` |
|   611171 |  287 | `		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){` |
|        - |  288 | `			/* Pop the result, then drain any abandoned outer-expression operands` |
|        - |  289 | `			 * to the catching try's base (like the inline branch above) — the` |
|        - |  290 | `			 * ops that would have consumed them were abandoned by the throw, and` |
|        - |  291 | `` 			 * leaving them leaks one slot per caught throw (`try { $a = 1 + f(); }` `` |
|        - |  292 | `			 * in a loop overflowed the operand stack). */` |
|   310122 |  293 | `			VmPopOperand(&pCaller->pTos,1);` |
|  1014686 |  294 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iResumeStackDepth ){` |
|   704569 |  295 | `				PH7_MemObjRelease(pCaller->pTos);` |
|   704569 |  296 | `				pCaller->pTos--;` |
|        5 |  297 | `			}` |
|   310122 |  298 | `			pCaller->pc = iResumePc;` |
|   310122 |  299 | `			rc = PH7_OK;` |
|   155038 |  300 | `		}else{` |
|   301054 |  301 | `			if( pParentFrame->pParent ){` |
|   301050 |  302 | `				rc = PH7_EXCEPTION;` |
|   150526 |  303 | `			}else{` |
|        - |  304 | `				/* Continue normal execution */` |
|        6 |  305 | `				rc = PH7_OK;` |
|        - |  306 | `			}` |
|        - |  307 | `		}` |
|   305556 |  308 | `	}` |
|        - |  309 | `	/* Recycle the operand stack for the next same-size call (BYTECODE stage 7),` |
|        - |  310 | `	 * or free it if the pool is full. Its allocated size is tracked in` |
|        - |  311 | `	 * pCallee->nStackCap (nMaxStack + VM_STACK_GUARD, or larger if an OP_SPREAD grew` |
|        - |  312 | `	 * it) — exactly what the buffer holds. (NULL when the function body was skipped.)` |
|        - |  313 | `	 *` |
|        - |  314 | `	 * Never on rc == PH7_SUSPEND: that path (unreachable in the stage-4 model,` |
|        - |  315 | `	 * where a deep suspend parks its whole record segment before reaching here)` |
|        - |  316 | `	 * would leave the callee stack owned by the suspended ctx, so recycling it` |
|        - |  317 | `	 * would hand a live fiber's operand stack to the next call. The guard keeps` |
|        - |  318 | `	 * that invariant explicit and robust to future coroutine changes. */` |
|   811911 |  319 | `	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){` |
|        - |  320 | `		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee` |
|        - |  321 | `		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly` |
|        - |  322 | `		 * the allocated slot count either way. */` |
|   807347 |  323 | `		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap,pCallee->nLiveTos);` |
|   403535 |  324 | `	}` |
|        - |  325 | `	/* Leave the frame. A throw that left this callee through a try it had OPEN` |
|        - |  326 | `	 * never reached that try's OP_POP_EXCEPTION, so the try's transparent wrapper` |
|        - |  327 | `	 * frames are still stacked ON TOP of the callee's own frame — the shape` |
|        - |  328 | ``	 * `function f(){ try { g(); } catch (NoMatch $e) {} }` leaves behind for every`` |
|        - |  329 | `	 * throw g() raises. Popping once here then tore the WRAPPER down and left the` |
|        - |  330 | `	 * callee's real frame on the chain for good: its locals were never released,` |
|        - |  331 | `	 * every frame above it was attributed to the wrong activation, and a generator` |
|        - |  332 | `	 * body that ended this way failed VmFinishCtxRun's identity test — so its ctx` |
|        - |  333 | `	 * frame was freed while the leftover still pointed at it, and the next frame` |
|        - |  334 | `	 * the pool handed out at that address closed pParent into a CYCLE that hung` |
|        - |  335 | `	 * every later walk of the chain. Drop the wrappers first, exactly as the` |
|        - |  336 | `	 * coroutine suspend/finish paths do (VmFreeSuspendedExceptionFrames): they are` |
|        - |  337 | `	 * transient, and OP_LOAD_EXCEPTION builds a fresh one when the try is next` |
|        - |  338 | `	 * entered. */` |
|        - |  339 | `	{` |
|        - |  340 | `		VmFrame *pW;` |
|        - |  341 | `		/* Only when every frame between the top and the callee's own is such a` |
|        - |  342 | `		 * wrapper: anything else means this callee's frame is already gone and the` |
|        - |  343 | `		 * chain above belongs to somebody else — leave it alone. */` |
|  1122190 |  344 | `		for( pW = pVm->pFrame ; pW && pW != pCallee->pFrame ; pW = pW->pParent ){` |
|   620399 |  345 | `			if( (pW->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|   310120 |  346 | `				break;` |
|        - |  347 | `			}` |
|   155119 |  348 | `		}` |
|   811911 |  349 | `		if( pW == pCallee->pFrame ){` |
|   501956 |  350 | `			while( pVm->pFrame != pCallee->pFrame ){` |
|      162 |  351 | `				VmLeaveFrame(&(*pVm));` |
|        2 |  352 | `			}` |
|   250785 |  353 | `		}` |
|        - |  354 | `	}` |
|   811911 |  355 | `	VmLeaveFrame(&(*pVm));` |
|   811911 |  356 | `	if( rc == PH7_ABORT ){` |
|      415 |  357 | `		return PH7_ABORT;` |
|        - |  358 | `	}` |
|   811501 |  359 | `	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  360 | `		/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  361 | `		 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  362 | `		 * overwriting the state saved by the inner level.` |
|        - |  363 | `		 * pTos points to the result slot (not yet written).` |
|        - |  364 | `		 * Save nTos one below so resume pushes at the result slot. */` |
|      ! 0 |  365 | `		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);` |
|      ! 0 |  366 | `		return PH7_SUSPEND;` |
|        - |  367 | `	}` |
|   811501 |  368 | `	if( rc == PH7_EXCEPTION ){` |
|   301050 |  369 | `		return PH7_EXCEPTION;` |
|        - |  370 | `	}` |
|   510456 |  371 | `	return PH7_OK;` |
|   405822 |  372 | `}` |
|        - |  373 | `/*` |
|        - |  374 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  375 | ` *` |
|        - |  376 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  377 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  378 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  379 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  380 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  381 | ` * then the program execution is halted.` |
|        - |  382 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  383 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  384 | ` * or to reset the VM to it's initial state.` |
|        - |  385 | ` */` |
|        - |  386 | `static sxi32 VmByteCodeExecBody(ph7_vm *pVm,VmInstr *aInstr,ph7_value *pStack,int nTos,` |
|        - |  387 | `	ph7_value *pResult,sxu32 *pLastRef,int is_callback,sxi32 nPc,` |
|        - |  388 | `	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,VmParkedSegment *pAdoptSegment,` |
|        - |  389 | `	ph7_value **ppBaseOwner,sxu32 *pnBaseCap,sxu32 nStackOrig);` |
|        - |  390 | `/*` |
|        - |  391 | ` * Native-nesting guard around the executor. PHP->PHP calls run iteratively` |
|        - |  392 | ` * (the stage-2 trampoline), but every OTHER (re-)entry — mini-programs,` |
|        - |  393 | ` * C->PHP callbacks, ctx start/resume, eval/include — is still one real C` |
|        - |  394 | ` * activation of VmByteCodeExecBody. nMaxDepth no longer bounds them (it is` |
|        - |  395 | ` * PHP call depth, raisable to memory-bound values since the clamp removal),` |
|        - |  396 | ` * so this counter is what actually protects the C stack: recursive` |
|        - |  397 | ` * eval/include towers, nested coroutine-resume chains and self-recursive` |
|        - |  398 | ` * C-callback compositions hit a clean fatal instead of overflowing. The limit` |
|        - |  399 | ` * lives in pVm->nMaxNativeDepth — a per-platform default (256 host / 16 small-` |
|        - |  400 | ` * stack embedders, VmInit) overridable via PH7_VM_CONFIG_NATIVE_DEPTH. This is` |
|        - |  401 | ` * still a coarse frame-count net rather than php's stack-byte measurement, so` |
|        - |  402 | ` * the host default is conservative — well below the old config clamp's <1024` |
|        - |  403 | ` * ceiling so it holds on the fattest frames (the callback path drags in` |
|        - |  404 | ` * usort/mergesort/trampoline C frames per re-entry, and instrumented builds` |
|        - |  405 | ` * inflate every frame), while far beyond any realistic eval/include/callback` |
|        - |  406 | ` * nesting.` |
|        - |  407 | ` */` |
|  4531259 |  408 | `PH7_PRIVATE sxi32 VmByteCodeExec(` |
|        - |  409 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  410 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  411 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  412 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  413 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  414 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  415 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  416 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  417 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  418 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|        - |  419 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|        - |  420 | `	ph7_value **ppBaseOwner, /* Storage slot the native entry frees for this invocation's BASE (pCallTop==0) operand stack — a local, pVm->aOps or pCtx->pStack. An OP_SPREAD that grows the base stack writes the new pointer here so the entry frees the right buffer. */` |
|        - |  421 | `	sxu32 *pnBaseCap, /* Storage for the base stack's capacity (resumable coroutines persist it across suspend/resume); updated alongside *ppBaseOwner on base-stack growth. Also the initial capacity read at entry. */` |
|        - |  422 | `	sxu32 nStackOrig /* The base stack's ORIGINAL (ungrown) allocation size. Unlike *pnBaseCap (which is the CURRENT, possibly-grown capacity on a coroutine resume), this is fixed, so OP_SPREAD growth headroom stays bounded across resumes. */` |
|        - |  423 | `	)` |
|        5 |  424 | `{` |
|        - |  425 | `	sxi32 rc;` |
|        - |  426 | `	sxi32 nSavedBrc;` |
|        - |  427 | `	sxu32 nSavedLine;` |
|  4531264 |  428 | `	if( VmNativeNestingExceeded(pVm) ){` |
|        6 |  429 | `		return VmNativeNestingFatal(pVm);` |
|        - |  430 | `	}` |
|        - |  431 | `	/* A fresh native exec entered while a C-boundary throw is parked` |
|        - |  432 | `	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the` |
|        - |  433 | `	 * very opcode that swallowed the throw) must run CLEAN: the parked status` |
|        - |  434 | `	 * belongs to the interrupted outer exec's fetch-point router, not to this` |
|        - |  435 | `	 * one. Save+clear on entry, merge back on exit — the outer status is` |
|        - |  436 | `	 * restored unless this exec parked its own unconsumed (newer) one, with` |
|        - |  437 | `	 * PH7_ABORT dominating either way. */` |
|  4531260 |  438 | `	nSavedBrc = pVm->nBoundaryRc;` |
|  4531260 |  439 | `	pVm->nBoundaryRc = 0;` |
|        - |  440 | `	/* The executing source line belongs to the ACTIVATION. A nested body -- a called` |
|        - |  441 | `	 * function, but equally an attribute-default or default-argument mini-program --` |
|        - |  442 | `	 * runs its own bytecode with its own lines, so it must not leave the caller` |
|        - |  443 | ``	 * reporting the callee's position: `new Exception` stamped line 1 because the`` |
|        - |  444 | ``	 * class's `protected $message = '';` default ran (from the embedded chunk) between`` |
|        - |  445 | `	 * OP_NEW and the stamp. Save on entry, restore on exit. */` |
|  4531260 |  446 | `	nSavedLine = pVm->nCurLine;` |
|  4531260 |  447 | `	pVm->nVmExecDepth++;` |
|  6796703 |  448 | `	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,` |
|  2265443 |  449 | `		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);` |
|  4531268 |  450 | `	pVm->nVmExecDepth--;` |
|  4531268 |  451 | `	pVm->nCurLine = nSavedLine;` |
|  4531268 |  452 | `	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT \|\| pVm->nBoundaryRc == 0) ){` |
|       43 |  453 | `		pVm->nBoundaryRc = nSavedBrc;` |
|       19 |  454 | `	}` |
|  4531268 |  455 | `	return rc;` |
|  2265450 |  456 | `}` |
|        - |  457 | `/*` |
|        - |  458 | ` * D1 commit 2: allocate a captured lvalue path for a deferred element/property call arg.` |
|        - |  459 | ` * eRoot picks the root container: 0 = a real aMemObj slot (nRootIdx), 1 = an undefined` |
|        - |  460 | ` * variable to vivify by name at resolve time (pName, a VM-lifetime bytecode string, borrowed),` |
|        - |  461 | ` * 2 = a string base (subscripting a string -> php refuses a by-ref bind). The struct owns its` |
|        - |  462 | ` * step array and each step's key/name; PH7_MemObjRelease frees it via VmFreeDeferredPath.` |
|        - |  463 | ` */` |
|    51098 |  464 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName)` |
|        5 |  465 | `{` |
|    51103 |  466 | `	VmDeferredPath *pPath = (VmDeferredPath *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDeferredPath));` |
|    51103 |  467 | `	if( pPath == 0 ){` |
|      ! 0 |  468 | `		return 0;` |
|        - |  469 | `	}` |
|    51103 |  470 | `	SyZero(pPath,sizeof(VmDeferredPath));` |
|    51103 |  471 | `	pPath->pAlloc = &pVm->sAllocator;` |
|    51103 |  472 | `	pPath->eRoot = eRoot;` |
|    51103 |  473 | `	pPath->nRootIdx = nRootIdx;` |
|    51103 |  474 | `	if( eRoot == 1 && pName ){` |
|        8 |  475 | `		pPath->sRootName = *pName; /* borrowed VM-lifetime bytes, not copied */` |
|        3 |  476 | `	}` |
|    51103 |  477 | `	return pPath;` |
|    25857 |  478 | `}` |
|        - |  479 | `/*` |
|        - |  480 | ` * A carrier for a fetch that ALREADY HAPPENED: an overloaded container answered with a` |
|        - |  481 | ` * value, and only the by-ref verdict is still pending (VM_DEFER_ROOT_PREFETCH). Takes a` |
|        - |  482 | ` * copy of the value; the caller keeps its own.` |
|        - |  483 | ` */` |
|      188 |  484 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNewPrefetch(ph7_vm *pVm,int nKind,ph7_class *pClass,` |
|        - |  485 | `	const SyString *pName,ph7_value *pVal)` |
|        5 |  486 | `{` |
|      193 |  487 | `	VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),VM_DEFER_ROOT_PREFETCH,SXU32_HIGH,0);` |
|      193 |  488 | `	if( pPath == 0 ){` |
|      ! 0 |  489 | `		return 0;` |
|        - |  490 | `	}` |
|      193 |  491 | `	pPath->nOverKind = (sxu8)nKind;` |
|      193 |  492 | `	pPath->pOverClass = pClass;` |
|      193 |  493 | `	if( pName && pName->nByte > 0 ){` |
|       76 |  494 | `		pPath->zOverName = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|       76 |  495 | `		if( pPath->zOverName == 0 ){` |
|      ! 0 |  496 | `			VmFreeDeferredPath(pPath);` |
|      ! 0 |  497 | `			return 0;` |
|        - |  498 | `		}` |
|       76 |  499 | `		SyStringInitFromBuf(&pPath->sOverName,pPath->zOverName,pName->nByte);` |
|       36 |  500 | `	}` |
|      193 |  501 | `	PH7_MemObjInit(&(*pVm),&pPath->sPrefetch);` |
|      193 |  502 | `	PH7_MemObjStore(pVal,&pPath->sPrefetch);` |
|      193 |  503 | `	return pPath;` |
|       99 |  504 | `}` |
|        - |  505 | `/*` |
|        - |  506 | ` * The verdict a prefetched value gets when the callee turns out to want it BY REFERENCE:` |
|        - |  507 | ` * php asked the object for something to modify and it could only answer with a value.` |
|        - |  508 | ` * Two of the three are notices php carries on from; a HOOKED property is the one php` |
|        - |  509 | ` * refuses outright. All three stay silent for a by-VALUE parameter, which is the whole` |
|        - |  510 | ` * reason the fetch could not decide them itself.` |
|        - |  511 | ` */` |
|       46 |  512 | `static sxi32 VmPrefetchByRefVerdict(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pVal)` |
|        1 |  513 | `{` |
|       47 |  514 | `	if( pPath->nOverKind == VM_OVER_PROP ){` |
|       17 |  515 | `		PH7_VmOverloadedPropNotice(&(*pVm),pPath->pOverClass,&pPath->sOverName,pVal);` |
|       17 |  516 | `		return SXRET_OK;` |
|        - |  517 | `	}` |
|       31 |  518 | `	if( pPath->nOverKind == VM_OVER_HOOK ){` |
|        - |  519 | `		SyBlob sErrMsg;` |
|        - |  520 | `		sxi32 rcH;` |
|        9 |  521 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        9 |  522 | `		SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|        8 |  523 | `			&pPath->pOverClass->sName,&pPath->sOverName);` |
|        9 |  524 | `		rcH = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg);` |
|        9 |  525 | `		SyBlobRelease(&sErrMsg);` |
|        9 |  526 | `		return (rcH == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  527 | `	}` |
|       23 |  528 | `	PH7_VmOverloadedElemNotice(&(*pVm),pPath->pOverClass,pVal);` |
|       23 |  529 | `	return SXRET_OK;` |
|       24 |  530 | `}` |
|    50946 |  531 | `static VmDeferStep * VmDeferPathGrow(VmDeferredPath *pPath)` |
|        5 |  532 | `{` |
|    50951 |  533 | `	if( pPath->nStep >= pPath->nAlloc ){` |
|    50939 |  534 | `		sxu32 nNew = pPath->nAlloc ? pPath->nAlloc * 2 : 4;` |
|    76709 |  535 | `		VmDeferStep *aNew = (VmDeferStep *)SyMemBackendRealloc(pPath->pAlloc,pPath->aStep,` |
|    25770 |  536 | `			nNew * sizeof(VmDeferStep));` |
|    50939 |  537 | `		if( aNew == 0 ){` |
|      ! 0 |  538 | `			return 0;` |
|        - |  539 | `		}` |
|    50939 |  540 | `		pPath->aStep = aNew;` |
|    50939 |  541 | `		pPath->nAlloc = nNew;` |
|    25770 |  542 | `	}` |
|    50951 |  543 | `	return &pPath->aStep[pPath->nStep];` |
|    25781 |  544 | `}` |
|        - |  545 | `/* Append an array-element step, deep-copying the index value (the caller releases pKey). */` |
|    50780 |  546 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey)` |
|        5 |  547 | `{` |
|    50785 |  548 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|    50785 |  549 | `	if( pStep == 0 ){` |
|      ! 0 |  550 | `		return SXERR_MEM;` |
|        - |  551 | `	}` |
|    50785 |  552 | `	pStep->isProp = 0;` |
|    50785 |  553 | `	pStep->bAppend = 0;` |
|    50785 |  554 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|    50785 |  555 | `	PH7_MemObjInit(pKey->pVm,&pStep->sKey);` |
|    50785 |  556 | `	PH7_MemObjStore(pKey,&pStep->sKey);` |
|    50785 |  557 | `	pPath->nStep++;` |
|    50785 |  558 | `	return SXRET_OK;` |
|    25698 |  559 | `}` |
|        - |  560 | ``/* Append a KEYLESS element step — the `[]` of `f($a[])`. It carries no index at all,`` |
|        - |  561 | ` * so the two resolvers differ on it: by reference it creates the next element (php` |
|        - |  562 | `` * binds the parameter to it), by value it is php's `Cannot use [] for reading`. */`` |
|       12 |  563 | `PH7_PRIVATE sxi32 VmDeferPathPushAppend(VmDeferredPath *pPath)` |
|        1 |  564 | `{` |
|       13 |  565 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|       13 |  566 | `	if( pStep == 0 ){` |
|      ! 0 |  567 | `		return SXERR_MEM;` |
|        - |  568 | `	}` |
|       13 |  569 | `	pStep->isProp = 0;` |
|       13 |  570 | `	pStep->bAppend = 1;` |
|       13 |  571 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|       13 |  572 | `	SyZero((void *)&pStep->sKey,sizeof(ph7_value));` |
|       13 |  573 | `	pPath->nStep++;` |
|       13 |  574 | `	return SXRET_OK;` |
|        7 |  575 | `}` |
|        - |  576 | `/* Append an object-property step, owning a private copy of the name bytes. */` |
|      154 |  577 | `PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName)` |
|        3 |  578 | `{` |
|      157 |  579 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|        - |  580 | `	char *zCopy;` |
|      157 |  581 | `	if( pStep == 0 ){` |
|      ! 0 |  582 | `		return SXERR_MEM;` |
|        - |  583 | `	}` |
|      157 |  584 | `	zCopy = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|      157 |  585 | `	if( zCopy == 0 ){` |
|      ! 0 |  586 | `		return SXERR_MEM;` |
|        - |  587 | `	}` |
|      157 |  588 | `	pStep->isProp = 1;` |
|      157 |  589 | `	pStep->bAppend = 0;` |
|      157 |  590 | `	pStep->zProp = zCopy;` |
|      157 |  591 | `	SyStringInitFromBuf(&pStep->sProp,zCopy,pName->nByte);` |
|      157 |  592 | `	pPath->nStep++;` |
|      157 |  593 | `	return SXRET_OK;` |
|       80 |  594 | `}` |
|        - |  595 | `/* Release a captured lvalue path and everything it owns (element keys, property names). */` |
|        - |  596 | `/*` |
|        - |  597 | `` * The pending offset of a `$s[k] ??= v`: a heap copy of the RAW key, owned by the`` |
|        - |  598 | ` * peek's MEMOBJ_AUX_COALSTROFF result on the operand stack. One carrier per` |
|        - |  599 | `` * pending ??=, so `$s[9] ??= ($t[9] ??= "q")` nests — a single VM-wide slot could`` |
|        - |  600 | ` * not (the inner peek overwrote the outer's offset, and the outer store then` |
|        - |  601 | ` * replaced the whole string).` |
|        - |  602 | ` */` |
|       54 |  603 | `PH7_PRIVATE VmCoalStrOff * VmCoalStrOffNew(ph7_vm *pVm,ph7_value *pKey)` |
|        1 |  604 | `{` |
|       55 |  605 | `	VmCoalStrOff *pCoal = (VmCoalStrOff *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmCoalStrOff));` |
|       55 |  606 | `	if( pCoal == 0 ){` |
|      ! 0 |  607 | `		return 0;` |
|        - |  608 | `	}` |
|       55 |  609 | `	pCoal->pAlloc = &pVm->sAllocator;` |
|       55 |  610 | `	PH7_MemObjInit(&(*pVm),&pCoal->sKey);` |
|       55 |  611 | `	if( pKey ){` |
|       55 |  612 | `		PH7_MemObjStore(pKey,&pCoal->sKey);` |
|       27 |  613 | `	}` |
|       55 |  614 | `	return pCoal;` |
|       28 |  615 | `}` |
|       90 |  616 | `PH7_PRIVATE void VmFreeCoalStrOff(VmCoalStrOff *pCoal)` |
|        3 |  617 | `{` |
|        - |  618 | `	SyMemBackend *pAlloc;` |
|       93 |  619 | `	if( pCoal == 0 ){` |
|       39 |  620 | `		return;` |
|        - |  621 | `	}` |
|       55 |  622 | `	pAlloc = pCoal->pAlloc;` |
|       55 |  623 | `	PH7_MemObjRelease(&pCoal->sKey);` |
|       55 |  624 | `	SyMemBackendFree(pAlloc,pCoal);` |
|       48 |  625 | `}` |
|        - |  626 | `/*` |
|        - |  627 | ` * Build the pending __call/__callStatic routing OP_MEMBER hands to the OP_CALL that` |
|        - |  628 | ` * follows it, and hang it off the marked carrier slot. One record per routed call, so a` |
|        - |  629 | ` * routed call evaluated inside another routed call's ARGUMENT LIST — which is where they` |
|        - |  630 | ` * now sit, php's order — keeps its own {receiver, class, name}. Takes the receiver` |
|        - |  631 | ` * reference; the record owns it from here.` |
|        - |  632 | ` */` |
|      126 |  633 | `PH7_PRIVATE VmMagicCall * VmMagicCallNew(ph7_vm *pVm,ph7_class_instance *pRecv,` |
|        - |  634 | `	ph7_class *pClass,const SyString *pName)` |
|        3 |  635 | `{` |
|      129 |  636 | `	VmMagicCall *pPend = (VmMagicCall *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmMagicCall));` |
|      129 |  637 | `	if( pPend == 0 ){` |
|      ! 0 |  638 | `		return 0;` |
|        - |  639 | `	}` |
|      129 |  640 | `	pPend->pAlloc = &pVm->sAllocator;` |
|      129 |  641 | `	pPend->pRecv = pRecv;` |
|      129 |  642 | `	pPend->pClass = pClass;` |
|      129 |  643 | `	SyBlobInit(&pPend->sName,&pVm->sAllocator);` |
|      129 |  644 | `	if( pName && pName->nByte > 0 ){` |
|      129 |  645 | `		SyBlobAppend(&pPend->sName,(const void *)pName->zString,pName->nByte);` |
|       63 |  646 | `	}` |
|      129 |  647 | `	if( pRecv ){` |
|       87 |  648 | `		pRecv->iRef++;` |
|       42 |  649 | `	}` |
|      129 |  650 | `	return pPend;` |
|       66 |  651 | `}` |
|      126 |  652 | `PH7_PRIVATE void VmFreeMagicCall(VmMagicCall *pPend)` |
|        3 |  653 | `{` |
|        - |  654 | `	SyMemBackend *pAlloc;` |
|      129 |  655 | `	if( pPend == 0 ){` |
|      ! 0 |  656 | `		return;` |
|        - |  657 | `	}` |
|      129 |  658 | `	pAlloc = pPend->pAlloc;` |
|      129 |  659 | `	if( pPend->pRecv ){` |
|      ! 0 |  660 | `		PH7_ClassInstanceUnref(pPend->pRecv);` |
|      ! 0 |  661 | `	}` |
|      129 |  662 | `	SyBlobRelease(&pPend->sName);` |
|      129 |  663 | `	SyMemBackendFree(pAlloc,pPend);` |
|       66 |  664 | `}` |
|    51098 |  665 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath)` |
|        5 |  666 | `{` |
|        - |  667 | `	sxu32 i;` |
|    51103 |  668 | `	if( pPath == 0 ){` |
|      ! 0 |  669 | `		return;` |
|        - |  670 | `	}` |
|   102049 |  671 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|    50951 |  672 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|    50951 |  673 | `		if( pStep->isProp ){` |
|      157 |  674 | `			if( pStep->zProp ){` |
|      157 |  675 | `				SyMemBackendFree(pPath->pAlloc,pStep->zProp);` |
|       80 |  676 | `			}` |
|    50874 |  677 | `		}else if( !pStep->bAppend ){` |
|        - |  678 | `			/* An append step holds no key at all — its sKey was never initialized. */` |
|    50785 |  679 | `			PH7_MemObjRelease(&pStep->sKey);` |
|    25693 |  680 | `		}` |
|    25781 |  681 | `	}` |
|    51103 |  682 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|      193 |  683 | `		PH7_MemObjRelease(&pPath->sPrefetch);` |
|      193 |  684 | `		if( pPath->zOverName ){` |
|       76 |  685 | `			SyMemBackendFree(pPath->pAlloc,pPath->zOverName);` |
|       36 |  686 | `		}` |
|       94 |  687 | `	}` |
|    51103 |  688 | `	if( pPath->aStep ){` |
|    50939 |  689 | `		SyMemBackendFree(pPath->pAlloc,pPath->aStep);` |
|    25770 |  690 | `	}` |
|    51103 |  691 | `	SyMemBackendFree(pPath->pAlloc,pPath);` |
|    25857 |  692 | `}` |
|        - |  693 | `/* Map an op-handler VmOpRc into the main-loop rc convention used by VmByteCodeExecBody. */` |
|    50848 |  694 | `static sxi32 VmOpRcToExecRc(VmOpRc rcOp)` |
|        5 |  695 | `{` |
|    50853 |  696 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 |  697 | `		return PH7_ABORT;` |
|        - |  698 | `	}` |
|    50851 |  699 | `	if( rcOp == VM_OP_EXCEPTION ){` |
|       22 |  700 | `		return PH7_EXCEPTION;` |
|        - |  701 | `	}` |
|    50831 |  702 | `	return SXRET_OK;` |
|    25732 |  703 | `}` |
|        - |  704 | `/*` |
|        - |  705 | ` * D1 commit 2: re-drive ONE captured lvalue step by invoking the real LOAD_IDX / MEMBER` |
|        - |  706 | ` * handler on a synthetic 2-slot stack. This reuses the proven COW / vivify / warning / magic` |
|        - |  707 | ` * machinery instead of hand-walking it. pBase carries the current container (its nIdx must be` |
|        - |  708 | ` * a real aMemObj slot for a by-ref write to vivify in place). iP2 selects the mode:` |
|        - |  709 | ` * LOAD_IDX 1=write(vivify,by-ref) / 0=read(by-value); MEMBER PH7_MEMBER_READ=by-value read.` |
|        - |  710 | ` * On success pOut receives the result value and its nIdx (the aliasable element slot for a` |
|        - |  711 | ` * vivified by-ref element).` |
|        - |  712 | ` */` |
|    50848 |  713 | `static sxi32 VmReDriveStep(ph7_vm *pVm,sxi32 iOp,sxu32 iP2,ph7_value *pBase,ph7_value *pKey,ph7_value *pOut)` |
|        5 |  714 | `{` |
|        - |  715 | `	ph7_value mini[2];` |
|        - |  716 | `	VmInstr aI[2];` |
|        - |  717 | `	VmExecState st;` |
|        - |  718 | `	VmOpRc rcOp;` |
|        - |  719 | ``	/* A NULL key is the APPEND form (`$a[]`): LOAD_IDX takes no index operand, so the`` |
|        - |  720 | `	 * base is the whole stack and iP1 says so. */` |
|    50853 |  721 | `	int bAppend = (pKey == 0);` |
|    50853 |  722 | `	PH7_MemObjInit(pVm,&mini[0]);` |
|    50853 |  723 | `	PH7_MemObjInit(pVm,&mini[1]);` |
|    50853 |  724 | `	PH7_MemObjLoad(pBase,&mini[0]);` |
|    50853 |  725 | `	mini[0].nIdx = pBase->nIdx;` |
|    50853 |  726 | `	if( !bAppend ){` |
|    50843 |  727 | `		PH7_MemObjStore(pKey,&mini[1]);` |
|    25722 |  728 | `	}` |
|    50853 |  729 | `	SyZero((void *)aI,sizeof(aI));` |
|        - |  730 | `	/* LOAD_IDX: iP1=1 means "an index is present". MEMBER: iP1=0 means an INSTANCE member` |
|        - |  731 | ``	 * (iP1=1 would be a static `::` access). */`` |
|    50853 |  732 | `	aI[0].iOp = (sxu8)iOp; aI[0].iP1 = (iOp == PH7_OP_LOAD_IDX && !bAppend) ? 1 : 0; aI[0].iP2 = iP2;` |
|    50853 |  733 | `	SyZero((void *)&st,sizeof(st));` |
|    50853 |  734 | `	st.pStack = mini; st.pTos = bAppend ? &mini[0] : &mini[1]; st.aInstr = aI; st.pc = 0;` |
|    50853 |  735 | `	if( iOp == PH7_OP_LOAD_IDX ){` |
|    50779 |  736 | `		rcOp = VmExecOpLoadIdx(&(*pVm),&st,&aI[0]);` |
|    25695 |  737 | `	}else{` |
|       76 |  738 | `		rcOp = VmExecOpMember(&(*pVm),&st,&aI[0]);` |
|        - |  739 | `	}` |
|        - |  740 | `	/* The handler popped the key/name and left the result in the (now top) base slot.` |
|        - |  741 | `	 * DEEP-COPY it into pOut (MemObjStore, not MemObjLoad): the result is chained as the next` |
|        - |  742 | `	 * step's base and must OWN its buffer — mini[0] is released immediately below, and a` |
|        - |  743 | `	 * read-only view (MemObjLoad) would leave pOut dangling into freed memory. */` |
|    50853 |  744 | `	PH7_MemObjStore(st.pTos,pOut);` |
|    50853 |  745 | `	pOut->nIdx = st.pTos->nIdx;` |
|    50853 |  746 | `	PH7_MemObjRelease(&mini[0]);` |
|    50853 |  747 | `	return VmOpRcToExecRc(rcOp);` |
|        5 |  748 | `}` |
|        - |  749 | `/*` |
|        - |  750 | ` * D1 commit 2: resolve an object property as a by-ref target. Given the object's aMemObj` |
|        - |  751 | ` * slot, return the property value's slot index in *pnOut so the by-ref binder can alias it.` |
|        - |  752 | ` * A present property binds directly; a missing one is created (recreate a declared+unset` |
|        - |  753 | ` * property, or a dynamic property on a dynamic-allowing class); a magic __get/__set property` |
|        - |  754 | ` * emits php's Notice and does NOT bind (*pbNoBind), handing __get's value back through` |
|        - |  755 | ` * pValOut (optional) for the by-VALUE pass php makes instead. Mirrors VmExecOpMember's` |
|        - |  756 | ` * write-create.` |
|        - |  757 | ` */` |
|       78 |  758 | `static sxi32 VmBindPropByRef(ph7_vm *pVm,ph7_value *pObj,const SyString *pName,sxu32 *pnOut,int *pbNoBind,ph7_value *pValOut)` |
|        3 |  759 | `{` |
|        - |  760 | `	ph7_class_instance *pThis;` |
|        - |  761 | `	ph7_class *pClass;` |
|        - |  762 | `	SyHashEntry *pEntry;` |
|       81 |  763 | `	VmClassAttr *pAttr = 0;` |
|       81 |  764 | `	*pbNoBind = 0;` |
|       81 |  765 | `	if( pObj == 0 ){` |
|        - |  766 | `		/* The root slot is gone (a detached frame): nothing to bind and nothing to say. */` |
|      ! 0 |  767 | `		*pbNoBind = 1;` |
|      ! 0 |  768 | `		return SXRET_OK;` |
|        - |  769 | `	}` |
|       81 |  770 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  771 | `		/* A non-object base has no property to alias, and php does not pass NULL and carry` |
|        - |  772 | `		 * on: asking one for something to MODIFY is its catchable Error, the same one every` |
|        - |  773 | `		 * other write shape through a null/int/string base raises. PHL warned about a READ` |
|        - |  774 | ``		 * it never performed and handed the by-ref parameter a NULL, so `f($u->p)` with`` |
|        - |  775 | ``		 * `function f(&$x)` wrote into nothing on a statement php stops the script for. The`` |
|        - |  776 | `		 * by-VALUE binding still takes the read warning — that half is php-exact — and is` |
|        - |  777 | `		 * what the value re-drive next door produces. */` |
|        - |  778 | `		SyBlob sErrM;` |
|        - |  779 | `		sxi32 rcErr;` |
|       21 |  780 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       21 |  781 | `		SyBlobFormat(&sErrM,"Attempt to modify property \"%z\" on %s",` |
|       10 |  782 | `			pName,VmArithValueName(pObj));` |
|       31 |  783 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       10 |  784 | `			SyBlobLength(&sErrM));` |
|       21 |  785 | `		SyBlobRelease(&sErrM);` |
|       21 |  786 | `		*pbNoBind = 1;` |
|       21 |  787 | `		return (rcErr == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  788 | `	}` |
|       61 |  789 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|       61 |  790 | `	pClass = pThis->pClass;` |
|       61 |  791 | `	if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|        - |  792 | `		/* A by-reference argument asks the incomplete object for something to` |
|        - |  793 | `		 * MODIFY: php's catchable Error, the same one every direct write raises. */` |
|        - |  794 | `		SyBlob sIncErr;` |
|        - |  795 | `		sxi32 rcInc;` |
|        3 |  796 | `		SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|        3 |  797 | `		PH7_VmIncompleteMsg(&(*pVm),pThis,"modify a property",&sIncErr);` |
|        4 |  798 | `		rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|        1 |  799 | `			SyBlobLength(&sIncErr));` |
|        3 |  800 | `		SyBlobRelease(&sIncErr);` |
|        3 |  801 | `		*pbNoBind = 1;` |
|        3 |  802 | `		return (rcInc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  803 | `	}` |
|       59 |  804 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|       59 |  805 | `	if( pEntry ){` |
|       16 |  806 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       16 |  807 | `		if( (pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       13 |  808 | `			if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|        - |  809 | `				/* A by-reference argument asks for something to MODIFY, and this` |
|        - |  810 | `				 * class's handler refuses every write: php's catchable Error, the` |
|        - |  811 | `				 * same sentence a plain store gets. */` |
|      ! 0 |  812 | `				*pbNoBind = 1;` |
|      ! 0 |  813 | `				return VmThrowNativeNoWrite(&(*pVm),pAttr->pOwner,pAttr->pAttr);` |
|        - |  814 | `			}` |
|       13 |  815 | `			if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|        - |  816 | `				/* A native class's property is a field of php's own C struct, not` |
|        - |  817 | `				 * storage a script may alias: php has no ptr_ptr handler for one, so` |
|        - |  818 | ``				 * `g($i->s)` with `function g(&$x)` passes the VALUE and the callee's`` |
|        - |  819 | `				 * write is lost — in SILENCE, unlike the overloaded case below, which` |
|        - |  820 | ``				 * php has a notice for. `preg_match($p,$s,$i->s)` is the same answer. */`` |
|      ! 0 |  821 | `				ph7_value *pCur = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|      ! 0 |  822 | `				*pbNoBind = 1;` |
|      ! 0 |  823 | `				if( pValOut && pCur ){` |
|      ! 0 |  824 | `					PH7_MemObjStore(pCur,pValOut);` |
|      ! 0 |  825 | `				}` |
|      ! 0 |  826 | `				return SXRET_OK;` |
|        - |  827 | `			}` |
|       12 |  828 | `			if( (pAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|       13 |  829 | `			 && (pAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|        - |  830 | `				/* php's rule for a REFERENCE fetch of an uninitialized typed property:` |
|        - |  831 | `				 * seed NULL and bind when the declared type admits one, refuse with` |
|        - |  832 | `` 				 * `Cannot access uninitialized non-nullable property ... by reference` `` |
|        - |  833 | `				 * when it does not. A by-VALUE argument takes the ordinary read Error,` |
|        - |  834 | `				 * which the value re-drive next door produces. */` |
|       19 |  835 | `				sxi32 rcRs = VmRefUninitTypedProperty(&(*pVm),pClass,pAttr,` |
|       12 |  836 | `					(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx));` |
|       13 |  837 | `				if( rcRs != SXRET_OK ){` |
|       13 |  838 | `					*pbNoBind = 1;` |
|       13 |  839 | `					return (rcRs == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  840 | `				}` |
|      ! 0 |  841 | `			}` |
|      ! 0 |  842 | `			*pnOut = pAttr->nIdx;` |
|      ! 0 |  843 | `			return SXRET_OK;` |
|        - |  844 | `		}` |
|        - |  845 | `		/* A static property is the CLASS's: php does not find it through an` |
|        - |  846 | ``		 * instance, so binding `f($o->s)` by reference must not hand out the`` |
|        - |  847 | `		 * class slot — that let a by-ref callee overwrite shared class state` |
|        - |  848 | `		 * through an object, and with no diagnostic at all (the value pass that` |
|        - |  849 | `		 * carries the notice at the fetch site never runs for a by-ref arg).` |
|        - |  850 | `		 * Notice here and fall through to the missing-property handling. */` |
|        4 |  851 | `		if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->pAttr->sName,` |
|        2 |  852 | `			pAttr->pAttr->iProtection,FALSE) ){` |
|        4 |  853 | `			VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  854 | `				"Accessing static property %z::$%z as non static",` |
|        1 |  855 | `				&pClass->sName,pName);` |
|        1 |  856 | `		}` |
|        3 |  857 | `		pAttr = 0;` |
|        1 |  858 | `	}` |
|       47 |  859 | `	if( PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|        - |  860 | `		/* Overloaded (magic) property: php passes it by-value with a Notice and drops the` |
|        - |  861 | `		 * write-back — "has no effect". The value it passes is __get's, which the caller` |
|        - |  862 | `		 * takes through pValOut; leaving the argument NULL instead turned` |
|        - |  863 | ``		 * `sort($o->magic)` — a statement php performs on a temporary — into`` |
|        - |  864 | ``		 * `sort(): Argument #1 ($array) must be of type array, null given`.`` |
|        - |  865 | `		 *` |
|        - |  866 | ``		 * `__get` ALONE decides it, which is php's own test`` |
|        - |  867 | ``		 * (zend_std_get_property_ptr_ptr consults `ce->__get` and nothing else): a`` |
|        - |  868 | ``		 * class carrying only `__set` has no way to ANSWER the fetch, so php falls`` |
|        - |  869 | `		 * through and creates an ordinary dynamic property instead — which is §10's` |
|        - |  870 | `		 * refusal here, not this notice.` |
|        - |  871 | `		 *` |
|        - |  872 | `		 * The notice is the OVERLOAD's, though, not the fetch's: a native class's` |
|        - |  873 | `		 * VIRTUAL property is a declared name php answers from a handler, and php's` |
|        - |  874 | `` 		 * own get_property_ptr_ptr declines it in SILENCE — `$r = &$doc->formatOutput` `` |
|        - |  875 | `		 * binds a temporary there and says nothing. */` |
|      ! 0 |  876 | `		ph7_class_attr *pVirt = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);` |
|      ! 0 |  877 | `		if( pVirt == 0 \|\| (pVirt->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) == 0 ){` |
|      ! 0 |  878 | `			VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  879 | `				"Indirect modification of overloaded property %z::$%z has no effect",` |
|      ! 0 |  880 | `				&pClass->sName,pName);` |
|      ! 0 |  881 | `		}` |
|      ! 0 |  882 | `		*pbNoBind = 1;` |
|      ! 0 |  883 | `		if( pValOut && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|      ! 0 |  884 | `		 && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g') ){` |
|      ! 0 |  885 | `			VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|      ! 0 |  886 | `			PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pValOut);` |
|      ! 0 |  887 | `			VmMagicGuardPop(pVm);` |
|      ! 0 |  888 | `		}` |
|      ! 0 |  889 | `		return SXRET_OK;` |
|        - |  890 | `	}` |
|        - |  891 | `	{` |
|       47 |  892 | `		ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);` |
|       47 |  893 | `		if( pDecl && PH7_ATTR_LAZY_ABSENT(pDecl,pThis) ){` |
|      ! 0 |  894 | `			if( pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|        - |  895 | `				/* Refused with or without a struct behind it. */` |
|      ! 0 |  896 | `				*pbNoBind = 1;` |
|      ! 0 |  897 | `				return VmThrowNativeNoWrite(&(*pVm),pClass,pDecl);` |
|        - |  898 | `			}` |
|      ! 0 |  899 | `			pDecl = 0;   /* never held: php creates a dynamic property, PHL refuses (§10) */` |
|      ! 0 |  900 | `		}` |
|       47 |  901 | `		if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      ! 0 |  902 | `			VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pAttr);` |
|       47 |  903 | `		}else if( VmClassAllowsDynamicProps(&(*pVm),pClass) ){` |
|       39 |  904 | `			PH7_VmCreateDynamicAttr(&(*pVm),pThis,pName->zString,pName->nByte,&pAttr);` |
|       21 |  905 | `		}else{` |
|        - |  906 | `			SyBlob sMsg;` |
|        - |  907 | `			sxi32 rcT;` |
|       10 |  908 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       10 |  909 | `			SyBlobFormat(&sMsg,"Cannot create dynamic property %z::$%z",&pClass->sName,pName);` |
|       10 |  910 | `			rcT = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       10 |  911 | `			SyBlobRelease(&sMsg);` |
|       10 |  912 | `			*pbNoBind = 1;` |
|       10 |  913 | `			return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  914 | `		}` |
|       39 |  915 | `		if( pAttr ){` |
|       39 |  916 | `			*pnOut = pAttr->nIdx;` |
|       21 |  917 | `		}else{` |
|      ! 0 |  918 | `			*pbNoBind = 1;` |
|        - |  919 | `		}` |
|        - |  920 | `	}` |
|       39 |  921 | `	return SXRET_OK;` |
|       42 |  922 | `}` |
|        - |  923 | `/*` |
|        - |  924 | ` * Walk a captured path's steps over SLOTS: each step vivifies in place and answers the` |
|        - |  925 | ` * next one, so the terminal slot is what the by-ref binder aliases. Split out of the` |
|        - |  926 | ` * resolver below because the VALUE walk hands control back to it — an accessor that` |
|        - |  927 | ` * answered with an OBJECT is a handle, not a temporary, and everything under it is` |
|        - |  928 | ` * addressable again.` |
|        - |  929 | ` */` |
|      144 |  930 | `static sxi32 VmWalkStepsFromSlot(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,sxu32 nCur,` |
|        - |  931 | `	ph7_value *pSlot)` |
|        3 |  932 | `{` |
|        - |  933 | `	sxu32 i;` |
|        - |  934 | `	sxi32 rc;` |
|      251 |  935 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|      151 |  936 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|      151 |  937 | `		if( pStep->isProp ){` |
|       79 |  938 | `			sxu32 nOut = SXU32_HIGH;` |
|       79 |  939 | `			int bNoBind = 0;` |
|        - |  940 | `			ph7_value sMagicVal;` |
|       79 |  941 | `			int bLastStep = (i + 1 == pPath->nStep);` |
|       79 |  942 | `			PH7_MemObjInit(&(*pVm),&sMagicVal);` |
|      155 |  943 | `			rc = VmBindPropByRef(&(*pVm),(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nCur),` |
|       76 |  944 | `				&pStep->sProp,&nOut,&bNoBind,bLastStep ? &sMagicVal : 0);` |
|       79 |  945 | `			if( rc != SXRET_OK ){` |
|       45 |  946 | `				PH7_MemObjRelease(&sMagicVal);` |
|       45 |  947 | `				return rc;` |
|        - |  948 | `			}` |
|       37 |  949 | `			if( bNoBind ){` |
|        - |  950 | `				/* magic/non-object: nothing to alias, so the argument is passed BY` |
|        - |  951 | `				 * VALUE — which for an overloaded property is what __get answered,` |
|        - |  952 | `				 * not the NULL this used to leave behind. */` |
|      ! 0 |  953 | `				if( bLastStep ){` |
|      ! 0 |  954 | `					PH7_MemObjStore(&sMagicVal,pSlot);` |
|      ! 0 |  955 | `					pSlot->nIdx = SXU32_HIGH;` |
|      ! 0 |  956 | `				}` |
|      ! 0 |  957 | `				PH7_MemObjRelease(&sMagicVal);` |
|      ! 0 |  958 | `				return SXRET_OK;` |
|        - |  959 | `			}` |
|       37 |  960 | `			PH7_MemObjRelease(&sMagicVal);` |
|       37 |  961 | `			nCur = nOut;` |
|       20 |  962 | `		}else{` |
|        - |  963 | `			ph7_value out;` |
|       74 |  964 | `			ph7_value *pContainer = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nCur);` |
|       74 |  965 | `			if( pContainer == 0 ){` |
|      ! 0 |  966 | `				return SXRET_OK;` |
|        - |  967 | `			}` |
|       74 |  968 | `			PH7_MemObjInit(&(*pVm),&out);` |
|        - |  969 | ``			/* `f($a[])` bound to a by-reference parameter: php CREATES the next element`` |
|        - |  970 | `			 * and aliases the parameter to it. */` |
|      110 |  971 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,1,pContainer,` |
|       72 |  972 | `				pStep->bAppend ? 0 : &pStep->sKey,&out);` |
|       74 |  973 | `			nCur = out.nIdx;` |
|       74 |  974 | `			PH7_MemObjRelease(&out);` |
|       74 |  975 | `			if( rc != SXRET_OK ){` |
|        3 |  976 | `				return rc;` |
|        - |  977 | `			}` |
|       72 |  978 | `			if( nCur == SXU32_HIGH ){` |
|      ! 0 |  979 | `				return SXRET_OK; /* no aliasable slot (e.g. a non-lvalue container): pass by value */` |
|        - |  980 | `			}` |
|        - |  981 | `		}` |
|       55 |  982 | `	}` |
|        - |  983 | `	{` |
|        - |  984 | `		/* The terminal slot is what the by-ref binder aliases, but the argument also has` |
|        - |  985 | `		 * to CARRY the element's value: a builtin reads what it is handed and writes back` |
|        - |  986 | `		 * through the slot. That was invisible while a path was only ever captured on a` |
|        - |  987 | `		 * MISS — the vivified element is NULL and so was the carrier — and stopped being` |
|        - |  988 | `		 * true when a WRITABLE container's existing element started riding one` |
|        - |  989 | ``		 * (`sort($ao['a'])` reached sort() as NULL). */`` |
|      103 |  990 | `		ph7_value *pFinal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nCur);` |
|      103 |  991 | `		if( pFinal ){` |
|      103 |  992 | `			PH7_MemObjLoad(pFinal,pSlot);` |
|       50 |  993 | `		}` |
|        - |  994 | `	}` |
|      103 |  995 | `	pSlot->nIdx = nCur;` |
|      103 |  996 | `	return SXRET_OK;` |
|       75 |  997 | `}` |
|        - |  998 | `/*` |
|        - |  999 | ` * Walk a captured path's remaining steps over a VALUE rather than a slot — the` |
|        - | 1000 | ` * continuation both resolvers need once the chain has left addressable storage: an` |
|        - | 1001 | ` * overloaded container's answer is a temporary, and everything subscripted off it is a` |
|        - | 1002 | ` * temporary too. bWrite picks php's fetch mode for those steps: a by-REFERENCE argument` |
|        - | 1003 | ` * makes them W fetches, which vivify inside the temporary in SILENCE (php's` |
|        - | 1004 | `` * `f($o['a']['zz'])` says only its notice), while a by-VALUE one reads and warns about a`` |
|        - | 1005 | ` * key that is not there.` |
|        - | 1006 | ` */` |
|    50932 | 1007 | `static sxi32 VmWalkStepsOverValue(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,` |
|        - | 1008 | `	ph7_value *pCur,int bWrite,ph7_value *pSlot)` |
|        5 | 1009 | `{` |
|    50937 | 1010 | `	sxi32 rc = SXRET_OK;` |
|        - | 1011 | `	sxu32 i;` |
|   101693 | 1012 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|    50785 | 1013 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|        - | 1014 | `		ph7_value out;` |
|    50785 | 1015 | `		PH7_MemObjInit(&(*pVm),&out);` |
|    50785 | 1016 | `		if( pStep->isProp && bWrite && (pCur->iFlags & MEMOBJ_OBJ) ){` |
|        - | 1017 | `			/* php's "indirect" only ever describes a VALUE: an object is a HANDLE, so a` |
|        - | 1018 | `			 * write through one lands however the handle was obtained — which is also why` |
|        - | 1019 | ``			 * the notice above stays silent for an object. `f($o->magic->p)` with`` |
|        - | 1020 | ``			 * `function f(&$x)` really does create and write `p` on the object __get`` |
|        - | 1021 | `			 * answered with. Bind the property and let the slot walk finish the chain. */` |
|        3 | 1022 | `			sxu32 nOut = SXU32_HIGH;` |
|        3 | 1023 | `			int bNoBind = 0;` |
|        3 | 1024 | `			int bLastStep = (i + 1 == pPath->nStep);` |
|        - | 1025 | `			ph7_value sMagicVal;` |
|        3 | 1026 | `			PH7_MemObjRelease(&out);` |
|        3 | 1027 | `			PH7_MemObjInit(&(*pVm),&sMagicVal);` |
|        4 | 1028 | `			rc = VmBindPropByRef(&(*pVm),pCur,&pStep->sProp,&nOut,&bNoBind,` |
|        1 | 1029 | `				bLastStep ? &sMagicVal : 0);` |
|        3 | 1030 | `			if( rc != SXRET_OK \|\| bNoBind ){` |
|      ! 0 | 1031 | `				if( rc == SXRET_OK && bLastStep ){` |
|        - | 1032 | `					/* An overloaded property one level down: its own notice has been` |
|        - | 1033 | `					 * raised and what __get answered is what php passes. */` |
|      ! 0 | 1034 | `					PH7_MemObjStore(&sMagicVal,pSlot);` |
|      ! 0 | 1035 | `					pSlot->nIdx = SXU32_HIGH;` |
|      ! 0 | 1036 | `				}` |
|      ! 0 | 1037 | `				PH7_MemObjRelease(&sMagicVal);` |
|      ! 0 | 1038 | `				return rc;` |
|        - | 1039 | `			}` |
|        3 | 1040 | `			PH7_MemObjRelease(&sMagicVal);` |
|        3 | 1041 | `			return VmWalkStepsFromSlot(&(*pVm),pPath,i + 1,nOut,pSlot);` |
|        - | 1042 | `		}` |
|    50783 | 1043 | `		if( pStep->isProp ){` |
|        - | 1044 | `			ph7_value nameVal;` |
|       76 | 1045 | `			PH7_MemObjInitFromString(&(*pVm),&nameVal,&pStep->sProp);` |
|       76 | 1046 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_MEMBER,PH7_MEMBER_READ,pCur,&nameVal,&out);` |
|       76 | 1047 | `			PH7_MemObjRelease(&nameVal);` |
|    50746 | 1048 | `		}else if( pStep->bAppend ){` |
|        - | 1049 | ``			/* `f($o['a'][])`: the append lands in the temporary either way. A by-VALUE`` |
|        - | 1050 | ``			 * binding is php's runtime `Cannot use [] for reading`, the same Error the`` |
|        - | 1051 | `			 * slot-based walk raises for it. */` |
|        - | 1052 | `			sxi32 rcAp;` |
|        3 | 1053 | `			if( bWrite ){` |
|        - | 1054 | `				/* The appended element is a fresh NULL that nothing else can see —` |
|        - | 1055 | `				 * php binds the parameter to it and the temporary is dropped. */` |
|      ! 0 | 1056 | `				PH7_MemObjRelease(pCur);` |
|      ! 0 | 1057 | `				*pCur = out;` |
|      ! 0 | 1058 | `				continue;` |
|        - | 1059 | `			}` |
|        3 | 1060 | `			PH7_MemObjRelease(&out);` |
|        3 | 1061 | `			rcAp = VmThrowFromVm(&(*pVm),"Error","Cannot use [] for reading",` |
|        - | 1062 | `				sizeof("Cannot use [] for reading")-1);` |
|        3 | 1063 | `			return (rcAp == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 1064 | `		}else{` |
|    50707 | 1065 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,bWrite ? 1 : 0,pCur,&pStep->sKey,&out);` |
|        - | 1066 | `		}` |
|    50781 | 1067 | `		PH7_MemObjRelease(pCur);` |
|    50781 | 1068 | `		*pCur = out;` |
|    50781 | 1069 | `		if( rc != SXRET_OK ){` |
|       22 | 1070 | `			return rc;` |
|        - | 1071 | `		}` |
|    25686 | 1072 | `	}` |
|    50913 | 1073 | `	PH7_MemObjStore(pCur,pSlot);` |
|    50913 | 1074 | `	pSlot->nIdx = SXU32_HIGH;` |
|    50913 | 1075 | `	return SXRET_OK;` |
|    25774 | 1076 | `}` |
|        - | 1077 | `/* Resolve a captured lvalue path as a BY-REF target: vivify the whole chain in place and` |
|        - | 1078 | ` * leave pSlot->nIdx pointing at the terminal (aliasable) slot for the by-ref binder. */` |
|      202 | 1079 | `static sxi32 VmResolvePathByRef(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        4 | 1080 | `{` |
|        - | 1081 | `	sxu32 nCur;` |
|        - | 1082 | `	sxi32 rc;` |
|      206 | 1083 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|        - | 1084 | `		/* An overloaded container was asked for something to MODIFY and could only hand` |
|        - | 1085 | `		 * back a value: php notices that the write has no effect and carries on with the` |
|        - | 1086 | `		 * temporary. The notice is raised HERE — the fetch itself cannot know whether the` |
|        - | 1087 | `		 * parameter it feeds is by-reference, and a by-VALUE one is silent. */` |
|        - | 1088 | `		ph7_value cur;` |
|       47 | 1089 | `		PH7_MemObjInit(&(*pVm),&cur);` |
|       47 | 1090 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|       47 | 1091 | `		rc = VmPrefetchByRefVerdict(&(*pVm),pPath,&cur);` |
|       47 | 1092 | `		if( rc == SXRET_OK ){` |
|       39 | 1093 | `			rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,TRUE,pSlot);` |
|       19 | 1094 | `		}` |
|       47 | 1095 | `		PH7_MemObjRelease(&cur);` |
|       47 | 1096 | `		return rc;` |
|        - | 1097 | `	}` |
|      160 | 1098 | `	if( pPath->eRoot == 2 ){` |
|        - | 1099 | `		/* Subscripting a string: php refuses a by-ref bind to a string offset — but` |
|        - | 1100 | ``		 * it applies its OFFSET rules first, so `f($s["p"])` is the offset TypeError`` |
|        - | 1101 | ``		 * and `f($s[1.5])` warns about the cast before this Error is raised. */`` |
|        - | 1102 | `		sxi32 rcT;` |
|       17 | 1103 | `		if( pPath->nStep > 0 && !pPath->aStep[0].isProp ){` |
|        - | 1104 | `			SyBlob sTypeMsg;` |
|       17 | 1105 | `			sxi64 iOfft = 0;` |
|       14 | 1106 | `			if( VmStringOffsetResolve(&(*pVm),&pPath->aStep[0].sKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg)` |
|       10 | 1107 | `				== VM_STROFF_REJECT ){` |
|        3 | 1108 | `				rcT = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|        3 | 1109 | `				return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1110 | `			}` |
|        6 | 1111 | `		}` |
|        - | 1112 | `		{` |
|        - | 1113 | `			/* WHICH refusal is php's depends on what the argument fetch was reaching` |
|        - | 1114 | ``			 * for: the offset ITSELF (`f($s[1])`, one step) cannot be referenced, while`` |
|        - | 1115 | `			 * a step INTO it is the same reach-inside every other consumer gets, named` |
|        - | 1116 | ``			 * for the STEP — `f($s[0][1])` is `... as an array` and `f($s[0]->p)` is`` |
|        - | 1117 | ``			 * `... as an object`. php derives all three from the opcode that consumes`` |
|        - | 1118 | `			 * the fetch; here the step count and the last step's kind are that. */` |
|       21 | 1119 | `			const char *zSoMsg = (pPath->nStep > 1)` |
|        4 | 1120 | `				? (pPath->aStep[pPath->nStep-1].isProp` |
|        - | 1121 | `					? "Cannot use string offset as an object"` |
|        - | 1122 | `					: "Cannot use string offset as an array")` |
|        6 | 1123 | `				: "Cannot create references to/from string offsets";` |
|       15 | 1124 | `			rcT = VmThrowFromVm(&(*pVm),"Error",zSoMsg,SyStrlen(zSoMsg));` |
|        - | 1125 | `		}` |
|       15 | 1126 | `		return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1127 | `	}` |
|      145 | 1128 | `	if( pPath->eRoot == 1 ){` |
|        8 | 1129 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,TRUE); /* vivify $a */` |
|        8 | 1130 | `		if( pRoot == 0 ){` |
|      ! 0 | 1131 | `			return SXRET_OK;` |
|        - | 1132 | `		}` |
|        8 | 1133 | `		nCur = pRoot->nIdx;` |
|        5 | 1134 | `	}else{` |
|      139 | 1135 | `		nCur = pPath->nRootIdx;` |
|        - | 1136 | `	}` |
|      145 | 1137 | `	return VmWalkStepsFromSlot(&(*pVm),pPath,0,nCur,pSlot);` |
|      105 | 1138 | `}` |
|        - | 1139 | `/* Resolve a captured lvalue path as a BY-VALUE argument: read the chain (emitting php's` |
|        - | 1140 | ` * undefined-key/property/offset warnings) WITHOUT vivifying, leaving the terminal value in` |
|        - | 1141 | ` * pSlot. Re-drives the read handlers so the warning sequence matches php exactly. */` |
|    50894 | 1142 | `static sxi32 VmResolvePathByValue(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        5 | 1143 | `{` |
|        - | 1144 | `	ph7_value cur;` |
|        - | 1145 | `	sxi32 rc;` |
|    50899 | 1146 | `	PH7_MemObjInit(&(*pVm),&cur);` |
|    50899 | 1147 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|        - | 1148 | `		/* The accessor already ran, where php runs it: a by-VALUE argument simply takes` |
|        - | 1149 | `		 * what it answered, in silence. */` |
|      145 | 1150 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|    50829 | 1151 | `	}else if( pPath->eRoot == 1 ){` |
|      ! 0 | 1152 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,FALSE);` |
|      ! 0 | 1153 | `		if( pRoot == 0 ){` |
|      ! 0 | 1154 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&pPath->sRootName);` |
|      ! 0 | 1155 | `		}else{` |
|      ! 0 | 1156 | `			PH7_MemObjLoad(pRoot,&cur);` |
|      ! 0 | 1157 | `			cur.nIdx = pRoot->nIdx;` |
|        - | 1158 | `		}` |
|      ! 0 | 1159 | `	}else{` |
|    50759 | 1160 | `		ph7_value *pRoot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pPath->nRootIdx);` |
|    50759 | 1161 | `		if( pRoot ){` |
|    50759 | 1162 | `			PH7_MemObjLoad(pRoot,&cur);` |
|    50759 | 1163 | `			cur.nIdx = pRoot->nIdx;` |
|    25680 | 1164 | `		}` |
|        - | 1165 | `	}` |
|    50899 | 1166 | `	rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,FALSE,pSlot);` |
|    50899 | 1167 | `	PH7_MemObjRelease(&cur);` |
|    50899 | 1168 | `	return rc;` |
|        5 | 1169 | `}` |
|        - | 1170 | `/*` |
|        - | 1171 | ` * Is this actual argument REFUSED by a by-reference parameter?` |
|        - | 1172 | ` *` |
|        - | 1173 | ` * php answers from the argument's compile-time SHAPE, which the call site carries in` |
|        - | 1174 | ` * VmCallArgMap.nNonLvalMask (GenStateArgShape, compile.c): a literal, an operator or` |
|        - | 1175 | `` * cast result, a class constant, `@$x`, `$o?->p` or an assignment is a hard non-lvalue`` |
|        - | 1176 | `` * and binding one is `Argument #N ($p) could not be passed by reference`.`` |
|        - | 1177 | ` *` |
|        - | 1178 | ` * nPos is the argument's position on the operand stack, which is the position the` |
|        - | 1179 | ` * compiler classified — named arguments change which FORMAL a slot binds to, not the` |
|        - | 1180 | ` * slot's index, so both binders index the mask the same way.` |
|        - | 1181 | ` *` |
|        - | 1182 | ` * Without a shape mask (a SPREAD call, an engine-synthesized call, an indirect dispatch` |
|        - | 1183 | ` * through call_user_func or an array callable) this falls back to the runtime test the` |
|        - | 1184 | ` * binders used before: no slot to write back through, and not one of the values PH7 has` |
|        - | 1185 | ` * always passed by value instead. That test cannot tell a literal from a call RESULT —` |
|        - | 1186 | ` * php accepts the latter — which is exactly why the mask exists.` |
|        - | 1187 | ` */` |
|     7785 | 1188 | `PH7_PRIVATE int PH7_VmArgRefusedByRef(VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1189 | `{` |
|     7790 | 1190 | `	if( pMap && pMap->bArgShapes && nPos < 31 ){` |
|     7512 | 1191 | `		return (pMap->nNonLvalMask & (1u << nPos)) != 0;` |
|        - | 1192 | `	}` |
|      282 | 1193 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|      220 | 1194 | `		return 0;` |
|        - | 1195 | `	}` |
|       93 | 1196 | `	return (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|       62 | 1197 | `	    && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0;` |
|     3894 | 1198 | `}` |
|        - | 1199 | `/*` |
|        - | 1200 | `` * The same call site's OTHER answer: the argument is the RESULT of a call or of `new`.`` |
|        - | 1201 | ` *` |
|        - | 1202 | ` * php cannot know at compile time whether the callee returns a reference, so it defers` |
|        - | 1203 | ` * to the value: one that arrived WITH a reference binds silently, and one without gets` |
|        - | 1204 | ` * php's E_NOTICE and the callee then operates on the temporary. Emitting it is all this` |
|        - | 1205 | ` * does — a temp-call argument is never refused.` |
|        - | 1206 | ` */` |
|        - | 1207 | `/*` |
|        - | 1208 | ` * A typed by-REFERENCE parameter's coercion belongs to the CALLER's variable. php` |
|        - | 1209 | ` * converts the actual in weak mode and the REFERENCE then holds the conversion, so` |
|        - | 1210 | `` * `$v = 1.0; f($v);` with `function f(int &$x)` leaves both views int(1). PHL ran the`` |
|        - | 1211 | ` * declared-type check on the operand-stack COPY while the binder aliases the caller's` |
|        - | 1212 | ` * slot by index, so the conversion reached neither the callee (which reads through the` |
|        - | 1213 | ` * alias) nor the caller: both stayed float, and every other pair did the same` |
|        - | 1214 | `` * (`float &$y` given an int, `string &$s` given an int, `bool &$b` given an int).`` |
|        - | 1215 | ` *` |
|        - | 1216 | ` * Writes back only when the check actually changed the value's TYPE — an untyped` |
|        - | 1217 | ` * parameter, or one the actual already satisfies, copies nothing.` |
|        - | 1218 | ` */` |
|     3599 | 1219 | `PH7_PRIVATE void PH7_VmByRefArgWriteBack(ph7_vm *pVm,ph7_value *pArg,sxi32 iPreFlags)` |
|        5 | 1220 | `{` |
|        - | 1221 | `	ph7_value *pSlot;` |
|     3599 | 1222 | `	if( pArg->nIdx == SXU32_HIGH` |
|     3604 | 1223 | `	 \|\| (pArg->iFlags & MEMOBJ_ALL) == (iPreFlags & MEMOBJ_ALL) ){` |
|     3580 | 1224 | `		return;` |
|        - | 1225 | `	}` |
|       25 | 1226 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArg->nIdx);` |
|       25 | 1227 | `	if( pSlot && pSlot != pArg ){` |
|       25 | 1228 | `		PH7_MemObjStore(pArg,pSlot);` |
|       12 | 1229 | `	}` |
|     1801 | 1230 | `}` |
|    40217 | 1231 | `PH7_PRIVATE void PH7_VmArgTempCallNotice(ph7_vm *pVm,VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1232 | `{` |
|    40222 | 1233 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| nPos >= 31 ){` |
|      282 | 1234 | `		return;` |
|        - | 1235 | `	}` |
|    39944 | 1236 | `	if( (pMap->nTempCallMask & (1u << nPos)) == 0 ){` |
|    39904 | 1237 | `		return;` |
|        - | 1238 | `	}` |
|       43 | 1239 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|        3 | 1240 | `		return; /* a by-reference RETURN: php is silent and binds it */` |
|        - | 1241 | `	}` |
|       41 | 1242 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Only variables should be passed by reference");` |
|    20099 | 1243 | `}` |
|        - | 1244 | `/*` |
|        - | 1245 | `` * A GENERATOR's arguments are bound at the `g(...)` that BUILDS the Generator object,`` |
|        - | 1246 | ` * before any resume — php's rule, and where php also refuses a by-reference parameter` |
|        - | 1247 | ` * handed a non-variable. That branch collects its actuals into a vector of its own (and` |
|        - | 1248 | ` * reorders it for named arguments), so neither of the two OP_CALL binders ever sees them` |
|        - | 1249 | `` * and `function g(&$x){ yield; } g(1 + 1);` built a Generator in silence.`` |
|        - | 1250 | ` *` |
|        - | 1251 | ` * Answers PH7_EXCEPTION (or PH7_ABORT) for the first refused position, having raised the` |
|        - | 1252 | ` * throw; SXRET_OK otherwise, with php's temp-call notice emitted along the way. Named` |
|        - | 1253 | ` * arguments are resolved by NAME against the formals here rather than through the` |
|        - | 1254 | ` * branch's own mapping, which is built later and freed inside its block.` |
|        - | 1255 | ` */` |
|      140 | 1256 | `static sxi32 VmScreenGenByRefArgs(ph7_vm *pVm,ph7_vm_func *pFunc,VmCallArgMap *pMap,` |
|        - | 1257 | `	ph7_value *pArg,sxu32 nActual,ph7_class *pSelfHint)` |
|        4 | 1258 | `{` |
|      144 | 1259 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      144 | 1260 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|        - | 1261 | `	sxu32 i;` |
|      294 | 1262 | `	for( i = 0 ; i < nActual ; ++i ){` |
|      160 | 1263 | `		sxu32 n = i;` |
|      160 | 1264 | `		if( pMap && pMap->bHasNamed && i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       34 | 1265 | `			for( n = 0 ; n < nFormal ; ++n ){` |
|       30 | 1266 | `				if( pMap->aNames[i].nByte == SyStringLength(&aFormal[n].sName)` |
|       29 | 1267 | `				 && SyMemcmp(pMap->aNames[i].zString,SyStringData(&aFormal[n].sName),` |
|       36 | 1268 | `					pMap->aNames[i].nByte) == 0 ){` |
|       22 | 1269 | `					break;` |
|        - | 1270 | `				}` |
|        7 | 1271 | `			}` |
|       11 | 1272 | `		}` |
|      160 | 1273 | `		if( n >= nFormal \|\| (aFormal[n].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|      118 | 1274 | `			continue;` |
|        - | 1275 | `		}` |
|       44 | 1276 | `		if( PH7_VmArgRefusedByRef(pMap,i,&pArg[i]) ){` |
|       10 | 1277 | `			sxi32 rcT = VmThrowByRefRefusal(&(*pVm),` |
|        6 | 1278 | `				(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|        6 | 1279 | `				&pFunc->sName,pFunc,n + 1,&aFormal[n].sName);` |
|        7 | 1280 | `			return (rcT == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1281 | `		}` |
|       38 | 1282 | `		PH7_VmArgTempCallNotice(&(*pVm),pMap,i,&pArg[i]);` |
|       20 | 1283 | `	}` |
|      138 | 1284 | `	return SXRET_OK;` |
|       74 | 1285 | `}` |
|        - | 1286 | `/*` |
|        - | 1287 | ` * D1: resolve deferred call arguments in [pArg, pTos) before the callee consumes them.` |
|        - | 1288 | ` *` |
|        - | 1289 | `` * A plain `$var` call argument whose callee signature is unknown at compile time is`` |
|        - | 1290 | ` * emitted as a DEFERRED load (OP_LOAD iP1=1,iP2=3): when the variable does not exist it` |
|        - | 1291 | ` * yields a NULL slot tagged MEMOBJ_AUX_DEFERRED that carries the variable name in x.pOther` |
|        - | 1292 | ` * (a VM-lifetime bytecode string). This helper runs at each OP_CALL dispatch branch, while` |
|        - | 1293 | ` * pVm->pFrame is still the CALLER frame, once the callee's by-ref shape is known:` |
|        - | 1294 | ` *` |
|        - | 1295 | ` *   by-ref position  -> create the variable in the caller frame now and give the slot its` |
|        - | 1296 | ` *                       real nIdx so the ordinary by-ref binder aliases it (silent, as php).` |
|        - | 1297 | ` *   by-value position-> raise php's "Undefined variable $x" and pass a clean NULL WITHOUT` |
|        - | 1298 | ` *                       creating the variable in the caller.` |
|        - | 1299 | ` *` |
|        - | 1300 | ` * The by-ref decision for positional argument n comes from, in priority order:` |
|        - | 1301 | ` *   bAllByValue  -> everything by-value (an unresolvable/erroring callee);` |
|        - | 1302 | ` *   bAllByRef    -> everything by-ref (the indirect array-callable/__invoke dispatch paths,` |
|        - | 1303 | ` *                   which historically over-vivified every plain-var arg — preserved here` |
|        - | 1304 | ` *                   rather than regressed; their by-value refinement is a later slice);` |
|        - | 1305 | ` *   pFormal      -> a user function's formal-argument array (honoring a trailing variadic);` |
|        - | 1306 | ` *   nByRefMask   -> a builtin by-ref position bitmask (used when pFormal == 0).` |
|        - | 1307 | ` *` |
|        - | 1308 | ` * A DEFINED variable never carries the marker (it loads with its real nIdx), so this is a` |
|        - | 1309 | ` * no-op for it; a call with no deferred args pays only one flag test per slot.` |
|        - | 1310 | ` */` |
|  8387233 | 1311 | `PH7_PRIVATE sxi32 PH7_VmResolveDeferredArgs(` |
|        - | 1312 | `	ph7_vm *pVm,` |
|        - | 1313 | `	ph7_value *pArg,` |
|        - | 1314 | `	ph7_value *pTos,` |
|        - | 1315 | `	ph7_vm_func_arg *pFormal,` |
|        - | 1316 | `	sxu32 nFormal,` |
|        - | 1317 | `	sxu32 nByRefMask,` |
|        - | 1318 | `	int bAllByRef,` |
|        - | 1319 | `	int bAllByValue,` |
|        - | 1320 | `	VmCallArgMap *pCallMap)` |
|        5 | 1321 | `{` |
|        - | 1322 | `	ph7_value *p;` |
|  8387238 | 1323 | `	sxu32 n = 0;` |
| 19510790 | 1324 | `	for( p = pArg ; p < pTos ; ++p, ++n ){` |
| 11123663 | 1325 | `		int bByRef = 0;` |
| 11123663 | 1326 | `		int bDeferred = (p->iFlags & (MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH)) != 0;` |
|        - | 1327 | `		SyString sName;` |
| 11123658 | 1328 | `		if( !bDeferred` |
| 11098346 | 1329 | `		 && (bAllByValue \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0) ){` |
|        - | 1330 | `			/* Nothing deferred in this position, and no slot in the VM could refuse` |
|        - | 1331 | `			 * being aliased -- the ordinary case, out through one test. */` |
|  6533638 | 1332 | `			continue;` |
|        - | 1333 | `		}` |
|  9179328 | 1334 | `		if( bAllByValue ){` |
|      ! 0 | 1335 | `			bByRef = 0;` |
|  9179328 | 1336 | `		}else if( bAllByRef ){` |
|        - | 1337 | `			/* bAllByRef comes ONLY from the array-callable-value and __invoke dispatch paths,` |
|        - | 1338 | `			 * which cannot expose the target's per-parameter by-ref flags here. Commit 1 chose` |
|        - | 1339 | `			 * to over-vivify every deferred PLAIN-VAR arg on those paths (harmless: it just` |
|        - | 1340 | `			 * materializes the caller variable), and that is preserved. But a deferred` |
|        - | 1341 | `			 * ELEMENT/PROPERTY (MEMOBJ_AUX_DEFPATH) must NOT be blanket-vivified there: a missing` |
|        - | 1342 | `			 * property would fatal ("Cannot create dynamic property") and a missing element would` |
|        - | 1343 | `			 * silently vivify + swallow php's "Undefined array key" warning. Resolve those` |
|        - | 1344 | `			 * by-value (warn + pass NULL), which matches php for a by-VALUE __invoke/callable —` |
|        - | 1345 | `			 * the genuine by-ref-out-param-into-an-element case stays unsupported here, exactly` |
|        - | 1346 | `			 * as it was before this slice. */` |
|      ! 0 | 1347 | `			bByRef = (p->iFlags & MEMOBJ_AUX_DEFPATH) ? 0 : 1;` |
|  9179328 | 1348 | `		}else if( pFormal ){` |
|   107850 | 1349 | `			sxu32 idx = n;` |
|   107845 | 1350 | `			if( pCallMap && pCallMap->bHasNamed && n < pCallMap->nTotal` |
|     1341 | 1351 | `			 && pCallMap->aNames[n].nByte > 0 ){` |
|        - | 1352 | `				/* A NAMED actual binds to the formal its NAME picks, not to the one at its` |
|        - | 1353 | ``				 * stack position: `r(x: $a["k"])` is argument #1 on the stack and parameter`` |
|        - | 1354 | `				 * $x in the declaration. Reading the by-ref-ness positionally consulted the` |
|        - | 1355 | `				 * wrong formal, so a by-reference named argument naming a missing element` |
|        - | 1356 | ``				 * warned `Undefined array key` and passed NULL where php creates it. */`` |
|        - | 1357 | `				sxu32 f;` |
|      648 | 1358 | `				idx = SXU32_HIGH;` |
|     1200 | 1359 | `				for( f = 0 ; f < nFormal ; ++f ){` |
|     1008 | 1360 | `					if( pCallMap->aNames[n].nByte == SyStringLength(&pFormal[f].sName)` |
|      922 | 1361 | `					 && SyMemcmp(pCallMap->aNames[n].zString,` |
|     1242 | 1362 | `						SyStringData(&pFormal[f].sName),pCallMap->aNames[n].nByte) == 0 ){` |
|      460 | 1363 | `						idx = f;` |
|      460 | 1364 | `						break;` |
|        - | 1365 | `					}` |
|      280 | 1366 | `				}` |
|   107527 | 1367 | `			}else if( idx >= nFormal ){` |
|        - | 1368 | `				/* Beyond the declared formals: a trailing variadic absorbs the tail` |
|        - | 1369 | `				 * (and dictates its by-ref-ness); otherwise the extra arg is by-value. */` |
|    10533 | 1370 | `				idx = (nFormal > 0 && (pFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC))` |
|     7967 | 1371 | `					? nFormal - 1 : SXU32_HIGH;` |
|     3476 | 1372 | `			}` |
|   107850 | 1373 | `			if( idx != SXU32_HIGH ){` |
|   102490 | 1374 | `				bByRef = (pFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|    51196 | 1375 | `			}` |
|    53856 | 1376 | `		}else{` |
|  9071483 | 1377 | `			bByRef = (n < 31 && (nByRefMask & (1u << n))) ? 1 : 0;` |
|        - | 1378 | `		}` |
|  9179328 | 1379 | `		if( !bDeferred ){` |
|        - | 1380 | `			/* An argument that ALREADY resolved to a slot, in a by-reference` |
|        - | 1381 | `			 * position: php screens the property behind it here, because binding` |
|        - | 1382 | `			 * the callee's parameter to it is an INDIRECT modification -- the` |
|        - | 1383 | `			 * callee's write would reach a readonly property, or a native one whose` |
|        - | 1384 | `			 * handler refuses every write, with nothing in the way. This is the one` |
|        - | 1385 | `			 * place both kinds of callee agree on: a user function's formals and a` |
|        - | 1386 | `			 * builtin's signature-derived mask both arrive here. */` |
|  9128088 | 1387 | `			if( bByRef ){` |
|    38956 | 1388 | `				sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),p->nIdx);` |
|    38956 | 1389 | `				if( rcInd != SXRET_OK ){` |
|       63 | 1390 | `					return rcInd;` |
|        - | 1391 | `				}` |
|    19459 | 1392 | `			}` |
|  9128070 | 1393 | `			continue;` |
|        - | 1394 | `		}` |
|    51245 | 1395 | `		if( p->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|        - | 1396 | `			/* D1 commit 2: a deferred array-element/property lvalue. Detach the descriptor` |
|        - | 1397 | `			 * FIRST (so an exception mid-resolve, or a later stack release, cannot double-free` |
|        - | 1398 | `			 * it) then re-walk it in the chosen mode. */` |
|    51101 | 1399 | `			VmDeferredPath *pPath = (VmDeferredPath *)p->x.pOther;` |
|        - | 1400 | `			sxi32 rc;` |
|    51101 | 1401 | `			p->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|    51101 | 1402 | `			p->x.pOther = 0;` |
|    51101 | 1403 | `			MemObjSetType(p,MEMOBJ_NULL);` |
|    51101 | 1404 | `			p->nIdx = SXU32_HIGH;` |
|    51101 | 1405 | `			if( bByRef ){` |
|      206 | 1406 | `				rc = VmResolvePathByRef(&(*pVm),pPath,p);` |
|      105 | 1407 | `			}else{` |
|    50899 | 1408 | `				rc = VmResolvePathByValue(&(*pVm),pPath,p);` |
|        - | 1409 | `			}` |
|    51101 | 1410 | `			VmFreeDeferredPath(pPath);` |
|    51101 | 1411 | `			if( rc != SXRET_OK ){` |
|       92 | 1412 | `				return rc;` |
|        - | 1413 | `			}` |
|    51013 | 1414 | `			continue;` |
|        - | 1415 | `		}` |
|        - | 1416 | `		/* Recover the deferred variable name and drop the marker + carrier. */` |
|      147 | 1417 | `		SyStringInitFromBuf(&sName,(const char *)p->x.pOther,` |
|        - | 1418 | `			p->x.pOther ? SyStrlen((const char *)p->x.pOther) : 0);` |
|      147 | 1419 | `		p->iFlags &= ~MEMOBJ_AUX_DEFERRED;` |
|      147 | 1420 | `		p->x.pOther = 0;` |
|      147 | 1421 | `		if( bByRef ){` |
|        - | 1422 | `			/* Materialize in the caller frame; the value stays NULL, only the slot` |
|        - | 1423 | `			 * back-reference (nIdx) matters for the by-ref binder / write-back. */` |
|      135 | 1424 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      135 | 1425 | `			if( pObj ){` |
|      135 | 1426 | `				p->nIdx = pObj->nIdx;` |
|       66 | 1427 | `			}` |
|       69 | 1428 | `		}else{` |
|        - | 1429 | `			/* php warns and passes NULL without creating the variable; the slot is` |
|        - | 1430 | `			 * already a clean NULL with nIdx == SXU32_HIGH from the deferred load. */` |
|       14 | 1431 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        - | 1432 | `		}` |
|       75 | 1433 | `	}` |
|  8387132 | 1434 | `	return SXRET_OK;` |
|  4193369 | 1435 | `}` |
|        - | 1436 | `/*` |
|        - | 1437 | ` * Did resolving a class NAME raise?` |
|        - | 1438 | ` *` |
|        - | 1439 | ` * The lookup can run an AUTOLOADER, and that autoloader can throw. The boundary rail` |
|        - | 1440 | ` * either parks the status in nBoundaryRc or — when a try caught it in place — records a` |
|        - | 1441 | ` * resume frame; either way the throw is already the engine's to land. A call site that` |
|        - | 1442 | `` * sees the class "missing" and piles its own `Class "X" not found` Error on top reports a`` |
|        - | 1443 | ` * failure php never reports, and that second Error belongs to nobody: it came back` |
|        - | 1444 | ` * UNCAUGHT and killed the script right after the real exception had been handled.` |
|        - | 1445 | ` *` |
|        - | 1446 | ` * Snapshot (nBoundaryRc, pResumeFrame) before the lookup and pass them here after.` |
|        - | 1447 | ` */` |
|      220 | 1448 | `PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore)` |
|        4 | 1449 | `{` |
|      224 | 1450 | `	return pVm->nBoundaryRc != nBrcBefore \|\| (const void *)pVm->pResumeFrame != pResumeBefore;` |
|        4 | 1451 | `}` |
|        - | 1452 | `/*` |
|        - | 1453 | `` * Name php's error for a class+method callable that the DIRECT `$cb()` dispatch cannot`` |
|        - | 1454 | ` * call, or return 0 when it resolves.` |
|        - | 1455 | ` *` |
|        - | 1456 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|        - | 1457 | ` * result for an unresolvable pair — silence a caller cannot detect — so the direct call` |
|        - | 1458 | ` * site has to decide for itself. It used to do that only for the ARRAY form; the` |
|        - | 1459 | ``  * `"Class::method"` STRING form went straight to the dispatcher, and `$cb='C::nosuch'` `` |
|        - | 1460 | ` * evaluated to NULL with no diagnostic at all where php throws.` |
|        - | 1461 | ` *` |
|        - | 1462 | ` * pClass is the resolved target class (0 when the name named nothing); zCls/nCls is the` |
|        - | 1463 | ` * class name AS WRITTEN, which is what php's not-found message quotes. bStaticForm says the` |
|        - | 1464 | ` * target was a class NAME rather than an object. Messages that interpolate a name are built` |
|        - | 1465 | ` * into zBuf.` |
|        - | 1466 | ` *` |
|        - | 1467 | ` * Visibility is NOT decided here: an inaccessible method is diagnosed downstream by the` |
|        - | 1468 | ` * dispatch itself ("Call to private method C::p() from global scope"), php-exact already —` |
|        - | 1469 | ` * and php reports visibility BEFORE staticness, so the static rule below has to stay quiet` |
|        - | 1470 | ` * for a method this scope could not reach anyway.` |
|        - | 1471 | ` */` |
|   200278 | 1472 | `static const char * VmCallableClassMethodError(` |
|        - | 1473 | `	ph7_vm *pVm,` |
|        - | 1474 | `	ph7_class *pClass,             /* Resolved target class, or 0 */` |
|        - | 1475 | `	const char *zCls,sxu32 nCls,   /* Its name as the callable wrote it */` |
|        - | 1476 | `	const char *zMeth,sxu32 nMeth, /* The method name */` |
|        - | 1477 | `	int bStaticForm,               /* TRUE when the target is a class NAME, not an object */` |
|        - | 1478 | `	char *zBuf,int nBuf            /* Scratch for the messages that quote a name */` |
|        - | 1479 | `	)` |
|        4 | 1480 | `{` |
|        - | 1481 | `	ph7_class_method *pMethod;` |
|        - | 1482 | `	SyString sMeth;` |
|        - | 1483 | `` 	/* php's fallback for a name this class cannot reach: when the CALLER holds a `$this` `` |
|        - | 1484 | `	 * that is an instance of it, the name resolves to the __call TRAMPOLINE rather than to` |
|        - | 1485 | `	 * __callStatic — and a trampoline is a NON-STATIC function, so this dispatch, which` |
|        - | 1486 | `	 * carries no object, refuses it exactly as it refuses any other non-static method named` |
|        - | 1487 | `	 * through a class. The callback spellings bind that receiver and run (php's` |
|        - | 1488 | `	 * direct-vs-callback asymmetry, one rule apart). The message names the class the` |
|        - | 1489 | `	 * callable WROTE and the name as written, even when the name is a declared static` |
|        - | 1490 | `	 * method: it is the trampoline being refused, not the method. */` |
|   200282 | 1491 | `	int bFallback = bStaticForm && PH7_VmStaticFallbackThis(&(*pVm),pClass) != 0;` |
|   200282 | 1492 | `	if( pClass == 0 ){` |
|       49 | 1493 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|       49 | 1494 | `		return zBuf;` |
|        - | 1495 | `	}` |
|   200235 | 1496 | `	pMethod = PH7_ClassExtractMethod(pClass,zMeth,nMeth);` |
|   200235 | 1497 | `	if( pMethod == 0 ){` |
|       85 | 1498 | `		if( bFallback ){` |
|        7 | 1499 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|        2 | 1500 | `				&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1501 | `			return zBuf;` |
|        - | 1502 | `		}` |
|        - | 1503 | `		/* A class that answers for unknown names through the catch-all has nothing to` |
|        - | 1504 | `		 * report: php runs __callStatic (class-name target) / __call (object target) for` |
|        - | 1505 | `		 * ANY method name, and the dispatcher below routes it. */` |
|       81 | 1506 | `		const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       81 | 1507 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|       53 | 1508 | `			return 0;` |
|        - | 1509 | `		}` |
|       43 | 1510 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",` |
|       14 | 1511 | `			&pClass->sName,(int)nMeth,zMeth);` |
|       29 | 1512 | `		return zBuf;` |
|        - | 1513 | `	}` |
|        - | 1514 | `	/* An ABSTRACT method (an interface's included) has no body to call — the same message` |
|        - | 1515 | ``	 * the `C::m()` SYNTAX raises in vm_ops_oo.c, which this dispatch reached only as a`` |
|        - | 1516 | `	 * mangled internal function name. */` |
|   200151 | 1517 | `	SyStringInitFromBuf(&sMeth,zMeth,nMeth);` |
|   200151 | 1518 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        7 | 1519 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%z()",&pClass->sName,&sMeth);` |
|        7 | 1520 | `		return zBuf;` |
|        - | 1521 | `	}` |
|        - | 1522 | `	/* Named through a class NAME, a non-static method is never callable: php refuses even` |
|        - | 1523 | `	 * when the CALLER has a compatible $this (unlike call_user_func, which binds it). The` |
|        - | 1524 | `	 * message names the OWNING class and the method's declared spelling. */` |
|        - | 1525 | `	{` |
|        - | 1526 | `		SyString sDecl;` |
|        - | 1527 | `		int bAccessible;` |
|   200145 | 1528 | `		SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1529 | `			SyStringLength(&pMethod->sFunc.sName));` |
|   300235 | 1530 | `		bAccessible = pMethod->iProtection == PH7_CLASS_PROT_PUBLIC` |
|   200142 | 1531 | `			\|\| PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       19 | 1532 | `				&sDecl,pMethod->iProtection,FALSE);` |
|   200145 | 1533 | `		if( !bAccessible && bFallback ){` |
|        - | 1534 | `			/* Inaccessible goes the same way as missing: php never reports the visibility,` |
|        - | 1535 | `			 * because the name resolved to the trampoline before visibility could matter. */` |
|       13 | 1536 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|        4 | 1537 | `				&pClass->sName,(int)nMeth,zMeth);` |
|       17 | 1538 | `			return zBuf;` |
|        - | 1539 | `		}` |
|   200137 | 1540 | `		if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 && bAccessible ){` |
|        - | 1541 | `			/* Deciding class vs NAMED class: a trait is php's compile-time construct, so` |
|        - | 1542 | `			 * every message names the class that composed it (PH7_VmMethodScopeName). */` |
|       25 | 1543 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|       16 | 1544 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|       17 | 1545 | `			return zBuf;` |
|        - | 1546 | `		}` |
|        - | 1547 | `	}` |
|   200121 | 1548 | `	return 0;` |
|   100143 | 1549 | `}` |
|        - | 1550 | `/*` |
|        - | 1551 | ` * php's visibility refusal for a method call, worded once: "Call to private A::m() from` |
|        - | 1552 | ` * scope S" (or "from global scope"). Two sites raise it — OP_CALL's screen and the` |
|        - | 1553 | ` * first-class-callable one below — and php names the DECLARING class, not the class the` |
|        - | 1554 | ` * lookup went through.` |
|        - | 1555 | ` */` |
|       24 | 1556 | `static const char * VmMethodVisibilityMsg(ph7_vm *pVm,ph7_class *pDecl,` |
|        - | 1557 | `	const char *zMeth,sxu32 nMeth,sxi32 iProtection,char *zBuf,int nBuf)` |
|        2 | 1558 | `{` |
|       26 | 1559 | `	const char *zVis = iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       26 | 1560 | `	ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|       26 | 1561 | `	if( pScope ){` |
|        4 | 1562 | `		SyBufferFormat(zBuf,nBuf,"Call to %s method %z::%.*s() from scope %z",` |
|        1 | 1563 | `			zVis,&pDecl->sName,(int)nMeth,zMeth,&pScope->sName);` |
|        2 | 1564 | `	}else{` |
|       35 | 1565 | `		SyBufferFormat(zBuf,nBuf,"Call to %s method %z::%.*s() from global scope",` |
|       11 | 1566 | `			zVis,&pDecl->sName,(int)nMeth,zMeth);` |
|        - | 1567 | `	}` |
|       26 | 1568 | `	return zBuf;` |
|        2 | 1569 | `}` |
|        - | 1570 | `/*` |
|        - | 1571 | ` * Is this class+method pair one a call would reach DIRECTLY from here — a real method (not` |
|        - | 1572 | ` * abstract, not a name only the catch-all answers) that the current scope may call? php` |
|        - | 1573 | ` * decides exactly this when it BUILDS a method Closure, and stores the resolved function; the` |
|        - | 1574 | ` * answer is what the VM_INSTANCE_FCC_SCREENED mark records, so the invocation never asks again.` |
|        - | 1575 | ` * A pair that answers FALSE here is the __call/__callStatic trampoline's, and its closure must` |
|        - | 1576 | ` * keep routing there.` |
|        - | 1577 | ` */` |
|      228 | 1578 | `PH7_PRIVATE int PH7_VmFccMethodIsDirect(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName)` |
|        5 | 1579 | `{` |
|        - | 1580 | `	ph7_class_method *pMethod;` |
|        - | 1581 | `	SyString sDecl;` |
|      233 | 1582 | `	if( pClass == 0 \|\| nName < 1 ){` |
|      ! 0 | 1583 | `		return 0;` |
|        - | 1584 | `	}` |
|      233 | 1585 | `	pMethod = PH7_ClassExtractMethod(pClass,zName,nName);` |
|      233 | 1586 | `	if( pMethod == 0 \|\| (pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       27 | 1587 | `		return 0;` |
|        - | 1588 | `	}` |
|      207 | 1589 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|      138 | 1590 | `		return 1;` |
|        - | 1591 | `	}` |
|       72 | 1592 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1593 | `		SyStringLength(&pMethod->sFunc.sName));` |
|        - | 1594 | `	/* The OWNING class decides (a trait method is owned by the class that composed it) — the` |
|        - | 1595 | `	 * same argument every other visibility site passes. */` |
|      106 | 1596 | `	return PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       68 | 1597 | `		&sDecl,pMethod->iProtection,FALSE) ? 1 : 0;` |
|      119 | 1598 | `}` |
|        - | 1599 | `/*` |
|        - | 1600 | `` * Resolve `$o->m(...)` / `C::m(...)` the way php resolves the CALL it stands for, and say`` |
|        - | 1601 | ` * why when it cannot. php builds a first-class callable through the same member lookup a` |
|        - | 1602 | ` * real call goes through, so every refusal a call would raise happens HERE, at creation:` |
|        - | 1603 | ` * an undefined method, an inaccessible one, an abstract one, and a non-static one named` |
|        - | 1604 | ` * through a class with no receiver to run on. PHL created a Closure for all four and only` |
|        - | 1605 | ` * discovered the problem when (and if) it was invoked — a closure that is built and dropped` |
|        - | 1606 | ` * reported nothing at all.` |
|        - | 1607 | ` *` |
|        - | 1608 | ` * The receiver is the other half of the same lookup. php's ZEND_INIT_STATIC_METHOD_CALL` |
|        - | 1609 | `` * binds the CALLING frame's `$this` when the resolved method is non-static and that object`` |
|        - | 1610 | `` * is an instance of the named class, which is what makes `self::m(...)` inside an instance`` |
|        - | 1611 | ` * method a working callable rather than a static one; PHL bound only the scope, so the` |
|        - | 1612 | ` * closure could never run. *ppRecv is that object, or 0 for a genuinely static callable.` |
|        - | 1613 | ` *` |
|        - | 1614 | ` * Answers 0 when the callable is valid. Messages that quote a name are built into zBuf.` |
|        - | 1615 | ` */` |
|      180 | 1616 | `static const char * VmFccMemberError(ph7_vm *pVm,ph7_class *pClass,` |
|        - | 1617 | `	const char *zCls,sxu32 nCls,const char *zMeth,sxu32 nMeth,int bStaticForm,` |
|        - | 1618 | `	ph7_class_instance **ppRecv,char *zBuf,int nBuf)` |
|        5 | 1619 | `{` |
|        - | 1620 | `	ph7_class_method *pMethod;` |
|        - | 1621 | `	SyString sDecl;` |
|      185 | 1622 | `	*ppRecv = 0;` |
|      185 | 1623 | `	if( pClass == 0 ){` |
|        3 | 1624 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|        3 | 1625 | `		return zBuf;` |
|        - | 1626 | `	}` |
|      183 | 1627 | `	pMethod = nMeth > 0 ? PH7_ClassExtractMethod(pClass,zMeth,nMeth) : 0;` |
|      183 | 1628 | `	if( pMethod == 0 ){` |
|        - | 1629 | `		/* A name the class answers through the catch-all is callable, and the catch-all` |
|        - | 1630 | `		 * the STATIC spelling reaches depends on the receiver, exactly as it does for a` |
|        - | 1631 | `		 * call (PH7_VmStaticFallbackThis). */` |
|       17 | 1632 | `		if( bStaticForm ){` |
|       11 | 1633 | `			*ppRecv = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|       10 | 1634 | `			if( *ppRecv` |
|       10 | 1635 | `			 \|\| PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1) ){` |
|        9 | 1636 | `				return 0;` |
|        1 | 1637 | `			}` |
|        8 | 1638 | `		}else if( PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) ){` |
|        5 | 1639 | `			return 0;` |
|        - | 1640 | `		}` |
|        7 | 1641 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",` |
|        2 | 1642 | `			&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1643 | `		return zBuf;` |
|        - | 1644 | `	}` |
|      167 | 1645 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1646 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      167 | 1647 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1648 | `		/* Named through the CLASS only: an instance of an abstract class cannot exist, so` |
|        - | 1649 | `		 * the object spelling never reaches an abstract body. */` |
|        7 | 1650 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%.*s()",` |
|        2 | 1651 | `			&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1652 | `		return zBuf;` |
|        - | 1653 | `	}` |
|      158 | 1654 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      118 | 1655 | `	 && !PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       34 | 1656 | `			&sDecl,pMethod->iProtection,FALSE) ){` |
|        - | 1657 | `		/* Inaccessible: the catch-all answers for it, on the same receiver a call would use. */` |
|       12 | 1658 | `		*ppRecv = bStaticForm ? PH7_VmStaticFallbackThis(&(*pVm),pClass) : 0;` |
|       12 | 1659 | `		if( *ppRecv ){` |
|      ! 0 | 1660 | `			return 0;` |
|        - | 1661 | `		}` |
|       12 | 1662 | `		if( !bStaticForm && PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) ){` |
|        3 | 1663 | `			return 0;` |
|        - | 1664 | `		}` |
|        - | 1665 | `		/* The DECIDING class is the declaring one (its trait grants live there); the class` |
|        - | 1666 | `		 * php NAMES is the composing one — a trait has no runtime existence in php. */` |
|       14 | 1667 | `		return VmMethodVisibilityMsg(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|        4 | 1668 | `			zMeth,nMeth,pMethod->iProtection,zBuf,nBuf);` |
|        - | 1669 | `	}` |
|      153 | 1670 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|       15 | 1671 | `		*ppRecv = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|       15 | 1672 | `		if( *ppRecv == 0 ){` |
|        7 | 1673 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|        4 | 1674 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|        5 | 1675 | `			return zBuf;` |
|        - | 1676 | `		}` |
|        5 | 1677 | `	}` |
|      149 | 1678 | `	return 0;` |
|       95 | 1679 | `}` |
|        - | 1680 | `/*` |
|        - | 1681 | ` * The same check for the ARRAY form, whose two members carry php's own shape messages` |
|        - | 1682 | ` * before anything is resolved: the target must be an object or a class-name string, the` |
|        - | 1683 | `` * method must be a string. php probes them in that order (`[5,5]` names the FIRST member,`` |
|        - | 1684 | `` * `['NoSuch',5]` the SECOND — the member shape decides before the class is looked up).`` |
|        - | 1685 | ` */` |
|   100200 | 1686 | `static const char * VmDirectArrayCallableError(ph7_vm *pVm,ph7_value *pTarget,ph7_value *pMethod,` |
|        - | 1687 | `	char *zBuf,int nBuf)` |
|        4 | 1688 | `{` |
|        - | 1689 | `	ph7_class *pClass;` |
|   100204 | 1690 | `	if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|       11 | 1691 | `		return "First array member is not a valid class name or object";` |
|        - | 1692 | `	}` |
|   100194 | 1693 | `	if( (pMethod->iFlags & MEMOBJ_STRING) == 0 ){` |
|        9 | 1694 | `		return "Second array member is not a valid method";` |
|        - | 1695 | `	}` |
|   100186 | 1696 | `	pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   150277 | 1697 | `	return VmCallableClassMethodError(&(*pVm),pClass,` |
|   100182 | 1698 | `		(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|   100182 | 1699 | `		(const char *)SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob),` |
|        - | 1700 | `		/* An OBJECT target carries its own $this; only a class NAME is the static form. */` |
|   100182 | 1701 | `		(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|    50091 | 1702 | `		zBuf,nBuf);` |
|    50104 | 1703 | `}` |
|        - | 1704 | `/*` |
|        - | 1705 | ` * The by-reference SHAPE of the callee an INDIRECT dispatch is about to reach — an` |
|        - | 1706 | `` * array callable VALUE (`$cb = [$o,'m']; $cb($a['k']);`) and an __invoke object.`` |
|        - | 1707 | ` * Both go through a shared helper that hides the target from OP_CALL, so the two` |
|        - | 1708 | ` * sites used to materialize EVERY deferred plain-var argument by reference and every` |
|        - | 1709 | ` * deferred element/property by value: a genuine by-ref out-param into an element was` |
|        - | 1710 | `` * unsupported (`$cb($a['new'])` warned `Undefined array key` and handed the callee a`` |
|        - | 1711 | ` * NULL where php creates the element and writes it), and a by-VALUE parameter` |
|        - | 1712 | `` * swallowed php's `Undefined variable` and CREATED the caller's variable.`` |
|        - | 1713 | ` *` |
|        - | 1714 | ` * The target is knowable here: the pair resolves to a class and a method, an object to` |
|        - | 1715 | ` * its __invoke. Answers 0 when nothing resolves — a name routed through` |
|        - | 1716 | ` * __call/__callStatic (php packs those into an ARRAY, so they are by-value anyway) or` |
|        - | 1717 | ` * a pair the screen above is about to refuse.` |
|        - | 1718 | ` */` |
|   100264 | 1719 | `static ph7_vm_func * VmIndirectCalleeFunc(ph7_vm *pVm,ph7_value *pCallable)` |
|        3 | 1720 | `{` |
|   100267 | 1721 | `	ph7_class_method *pMeth = 0;` |
|   100267 | 1722 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|   100267 | 1723 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|   100267 | 1724 | `		ph7_value *pTarget = 0,*pName = 0;` |
|        - | 1725 | `		ph7_class *pClass;` |
|   100264 | 1726 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|   100264 | 1727 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|   100267 | 1728 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1729 | `			return 0;` |
|        - | 1730 | `		}` |
|   100267 | 1731 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   100267 | 1732 | `		if( pClass == 0 ){` |
|      ! 0 | 1733 | `			return 0;` |
|        - | 1734 | `		}` |
|   150399 | 1735 | `		pMeth = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|   100264 | 1736 | `			SyBlobLength(&pName->sBlob));` |
|    50132 | 1737 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1738 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 1739 | `		if( pThis == 0 ){` |
|      ! 0 | 1740 | `			return 0;` |
|        - | 1741 | `		}` |
|      ! 0 | 1742 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 | 1743 | `	}` |
|   100267 | 1744 | `	return pMeth ? &pMeth->sFunc : 0;` |
|    50135 | 1745 | `}` |
|        - | 1746 | `/*` |
|        - | 1747 | ` * Materialize an indirect dispatch's deferred arguments against that callee — the same` |
|        - | 1748 | ` * split OP_CALL makes for a direct one: a native method's by-ref positions come from its` |
|        - | 1749 | ` * signature mask, a PHP one's from its compiled formals, and an unresolved callee binds` |
|        - | 1750 | ` * everything by value (php's answer for the magic route it is about to take).` |
|        - | 1751 | ` */` |
|   100264 | 1752 | `static sxi32 VmResolveIndirectArgs(ph7_vm *pVm,ph7_value *pCallable,ph7_value *pArg,ph7_value *pTos,` |
|        - | 1753 | `	VmCallArgMap *pCallMap)` |
|        3 | 1754 | `{` |
|   100267 | 1755 | `	ph7_vm_func *pFn = VmIndirectCalleeFunc(&(*pVm),pCallable);` |
|   100267 | 1756 | `	if( pFn == 0 ){` |
|       45 | 1757 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pCallMap);` |
|        - | 1758 | `	}` |
|   100223 | 1759 | `	if( pFn->iFlags & VM_FUNC_NATIVE ){` |
|        9 | 1760 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|        8 | 1761 | `			pFn->pNative ? pFn->pNative->nByRefMask : 0,0,0,pCallMap);` |
|        - | 1762 | `	}` |
|   150321 | 1763 | `	return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   100212 | 1764 | `		(ph7_vm_func_arg *)SySetBasePtr(&pFn->aArgs),SySetUsed(&pFn->aArgs),0,0,0,pCallMap);` |
|    50135 | 1765 | `}` |
|        - | 1766 | `/*` |
|        - | 1767 | `` * Why a VALUE cannot be made into a first-class callable. php answers `($v)(...)` with`` |
|        - | 1768 | `` * exactly what it answers `($v)()` — the taxonomy is the DIRECT dispatch's, word for word —`` |
|        - | 1769 | ` * so this walks the same three shapes the OP_CALL sites do and reuses their builders. PHL` |
|        - | 1770 | `` * left a non-callable value STANDING instead: `$x = 5; $f = ($x)(...);` evaluated to int(5),`` |
|        - | 1771 | ` * an array to the array, a misspelled function name to its own string — a value that is not` |
|        - | 1772 | ` * a Closure where php throws, silently, on every shape.` |
|        - | 1773 | ` *` |
|        - | 1774 | ` * Returns 0 when the value IS callable (unreachable through the FCC caller, which asks only` |
|        - | 1775 | ` * after the wrap declined, but it keeps the helper honest for a direct reader).` |
|        - | 1776 | ` */` |
|       82 | 1777 | `static const char * VmFccValueError(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|        4 | 1778 | `{` |
|       86 | 1779 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       23 | 1780 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       23 | 1781 | `		ph7_value *pTarget = 0,*pMeth = 0;` |
|        - | 1782 | `		const char *zWhy;` |
|       23 | 1783 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|        9 | 1784 | `			return "Array callback must have exactly two elements";` |
|        - | 1785 | `		}` |
|       15 | 1786 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pMeth) ){` |
|        3 | 1787 | `			return "Array callback has to contain indices 0 and 1";` |
|        - | 1788 | `		}` |
|       13 | 1789 | `		zWhy = VmDirectArrayCallableError(&(*pVm),pTarget,pMeth,zBuf,nBuf);` |
|       13 | 1790 | `		if( zWhy ){` |
|        9 | 1791 | `			return zWhy;` |
|        - | 1792 | `		}` |
|        - | 1793 | `		/* That check deliberately leaves VISIBILITY to OP_CALL's own screen, which raises it` |
|        - | 1794 | `		 * when the pair is finally called — and a first-class callable never gets there: php` |
|        - | 1795 | ``		 * refuses `[$o,'priv'](...)` at the creation, with the direct dispatch's wording.`` |
|        - | 1796 | `		 * Reached only for a pair PH7_VmIsCallable already declined, so a class routing the` |
|        - | 1797 | `		 * name through __call (which makes it callable) cannot arrive here. */` |
|        5 | 1798 | `		if( (pMeth->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMeth->sBlob) > 0 ){` |
|        5 | 1799 | `			ph7_class *pCbCls = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|        5 | 1800 | `			const char *zM = (const char *)SyBlobData(&pMeth->sBlob);` |
|        5 | 1801 | `			sxu32 nM = SyBlobLength(&pMeth->sBlob);` |
|        5 | 1802 | `			ph7_class_method *pCbMeth = pCbCls ? PH7_ClassExtractMethod(pCbCls,zM,nM) : 0;` |
|        4 | 1803 | `			if( pCbMeth && pCbMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|        5 | 1804 | `			 && !PH7_VmFccMethodIsDirect(&(*pVm),pCbCls,zM,nM) ){` |
|        7 | 1805 | `				return VmMethodVisibilityMsg(&(*pVm),` |
|        2 | 1806 | `					PH7_VmMethodScopeName(&(*pVm),pCbCls,pCbMeth),` |
|        2 | 1807 | `					zM,nM,pCbMeth->iProtection,zBuf,nBuf);` |
|        - | 1808 | `			}` |
|      ! 0 | 1809 | `		}` |
|      ! 0 | 1810 | `		return 0;` |
|        - | 1811 | `	}` |
|       64 | 1812 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       19 | 1813 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|       19 | 1814 | `		if( pObj == 0 ){` |
|      ! 0 | 1815 | `			return "Value of type object is not callable";` |
|        - | 1816 | `		}` |
|       19 | 1817 | `		if( PH7_ClassExtractMethod(pObj->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|      ! 0 | 1818 | `			return 0;` |
|        - | 1819 | `		}` |
|       19 | 1820 | `		SyBufferFormat(zBuf,nBuf,"Object of type %z is not callable",&pObj->pClass->sName);` |
|       19 | 1821 | `		return zBuf;` |
|        - | 1822 | `	}` |
|       46 | 1823 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       32 | 1824 | `		const char *zCls = 0,*zMeth = 0;` |
|       32 | 1825 | `		sxu32 nCls = 0,nMeth = 0;` |
|        - | 1826 | `		SyString sName;` |
|       32 | 1827 | `		SyStringInitFromBuf(&sName,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|        - | 1828 | `		/* A leading backslash only anchors the name to the global namespace. */` |
|       32 | 1829 | `		if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|      ! 0 | 1830 | `			sName.zString++;` |
|      ! 0 | 1831 | `			sName.nByte--;` |
|      ! 0 | 1832 | `		}` |
|       32 | 1833 | `		if( PH7_VmCallableStringParts(sName.zString,sName.nByte,&zCls,&nCls,&zMeth,&nMeth) ){` |
|        - | 1834 | `			/* "Class::method" carries the class/method taxonomy, not the function one. */` |
|        7 | 1835 | `			return VmCallableClassMethodError(&(*pVm),` |
|        2 | 1836 | `				PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0),` |
|        2 | 1837 | `				zCls,nCls,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|        - | 1838 | `		}` |
|       28 | 1839 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined function %z()",&sName);` |
|       28 | 1840 | `		return zBuf;` |
|        - | 1841 | `	}` |
|       16 | 1842 | `	SyBufferFormat(zBuf,nBuf,"Value of type %s is not callable",VmArithTypeName(pValue));` |
|       16 | 1843 | `	return zBuf;` |
|       45 | 1844 | `}` |
|        - | 1845 | `/*` |
|        - | 1846 | `` * Split a `"Class::method"` callable string. php scans for the LAST "::" (zend_memrchr),`` |
|        - | 1847 | `` * so `"C::s::x"` names the class `C::s` — not `C` — and reports it not found. Returns TRUE`` |
|        - | 1848 | `` * and the two halves (either may be empty: `"C::"` and `"::s"` are shapes php accepts here`` |
|        - | 1849 | ` * and rejects further down), FALSE when the string carries no "::" at all.` |
|        - | 1850 | ` */` |
|  1666575 | 1851 | `PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,` |
|        - | 1852 | `	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth)` |
|        5 | 1853 | `{` |
|        - | 1854 | `	sxu32 i;` |
| 11274962 | 1855 | `	for( i = nName ; i >= 2 ; --i ){` |
|  9808585 | 1856 | `		if( zName[i-2] == ':' && zName[i-1] == ':' ){` |
|   200202 | 1857 | `			*pzCls = zName;` |
|   200202 | 1858 | `			*pnCls = i - 2;` |
|   200202 | 1859 | `			*pzMeth = &zName[i];` |
|   200202 | 1860 | `			*pnMeth = nName - i;` |
|   200202 | 1861 | `			return TRUE;` |
|        - | 1862 | `		}` |
|  4800785 | 1863 | `	}` |
|  1466382 | 1864 | `	return FALSE;` |
|   832798 | 1865 | `}` |
|        - | 1866 | `/*` |
|        - | 1867 | ` * A parameter DEFAULT is a mini-program run in the callee's frame before the body` |
|        - | 1868 | `` * starts, and `self::K` inside one means the class that DECLARED the method -- for`` |
|        - | 1869 | ` * a method composed from a trait, the class that composed it. That answer is not` |
|        - | 1870 | ` * reachable by the ordinary route here: the call's self stack is not pushed until` |
|        - | 1871 | `` * the body begins, so the trait rule has nothing to walk from and `self` was left`` |
|        - | 1872 | `` * unresolved -- read as a class of that name, and thrown as `Class "self" not`` |
|        - | 1873 | `` * found` for a default php evaluates without a word.`` |
|        - | 1874 | ` *` |
|        - | 1875 | ` * Mark the declaring class explicitly for the duration, the way a member` |
|        - | 1876 | ` * initializer's evaluation does (PH7_VmPeekDeclaringClass reads the frame-keyed` |
|        - | 1877 | ` * pair). A non-trait method resolves to the class it already did, so nothing else` |
|        - | 1878 | ` * moves. The pair is saved and restored around each default because one default` |
|        - | 1879 | ` * may CALL something that evaluates defaults of its own.` |
|        - | 1880 | ` */` |
|        - | 1881 | `typedef struct VmDefaultScope VmDefaultScope;` |
|        - | 1882 | `struct VmDefaultScope { ph7_class *pClass; void *pFrame; };` |
|    17289 | 1883 | `static void VmDefaultScopeEnter(ph7_vm *pVm,VmFrame *pFrame,ph7_vm_func *pVmFunc,` |
|        - | 1884 | `	ph7_class *pSelf,VmDefaultScope *pSave)` |
|        5 | 1885 | `{` |
|    17294 | 1886 | `	pSave->pClass = pVm->pConstEvalClass;` |
|    17294 | 1887 | `	pSave->pFrame = pVm->pConstEvalFrame;` |
|    17294 | 1888 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pVmFunc->pUserData ){` |
|      373 | 1889 | `		pVm->pConstEvalClass = PH7_VmMemberOwnerClass((ph7_class *)pVmFunc->pUserData,` |
|      123 | 1890 | `			pSelf ? pSelf : (ph7_class *)pVmFunc->pUserData);` |
|      250 | 1891 | `		pVm->pConstEvalFrame = (void *)pFrame;` |
|      123 | 1892 | `	}` |
|    17294 | 1893 | `}` |
|    17289 | 1894 | `static void VmDefaultScopeLeave(ph7_vm *pVm,VmDefaultScope *pSave)` |
|        5 | 1895 | `{` |
|    17294 | 1896 | `	pVm->pConstEvalClass = pSave->pClass;` |
|    17294 | 1897 | `	pVm->pConstEvalFrame = pSave->pFrame;` |
|    17294 | 1898 | `}` |
|  4531247 | 1899 | `static sxi32 VmByteCodeExecBody(` |
|        - | 1900 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 1901 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - | 1902 | `	ph7_value *pStack,   /* Operand stack */` |
|        - | 1903 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - | 1904 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - | 1905 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - | 1906 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - | 1907 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - | 1908 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - | 1909 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|        - | 1910 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|        - | 1911 | `	ph7_value **ppBaseOwner, /* Base (pCallTop==0) operand-stack owner slot the native entry frees; OP_SPREAD growth of the base stack writes the new pointer here (see VmGrowOperandStack). */` |
|        - | 1912 | `	sxu32 *pnBaseCap, /* Base stack capacity (persisted across coroutine suspend/resume); read for the initial capacity and updated on base-stack growth. */` |
|        - | 1913 | `	sxu32 nStackOrig /* Base stack's ORIGINAL (ungrown) allocation size — the fixed headroom reference for OP_SPREAD growth (see nStackOrig in VmExecState). */` |
|        - | 1914 | `	)` |
|        5 | 1915 | `{` |
|        - | 1916 | `	VmInstr *pInstr;` |
|        - | 1917 | `	ph7_value *pTos;` |
|        - | 1918 | `	/* The activation's operand-stack WATERMARK: the deepest pTos has been at any` |
|        - | 1919 | `	 * instruction boundary. A local for the same reason pTos and pc are -- it is` |
|        - | 1920 | `	 * touched once per instruction -- and synced into sState at the same boundaries.` |
|        - | 1921 | `	 * Its whole purpose is the teardown sweep: nothing above it was ever written, so` |
|        - | 1922 | `	 * VmOperandStackRecycle walks to it instead of walking the whole buffer. */` |
|        - | 1923 | `	ph7_value *pHigh;` |
|        - | 1924 | `	SySet aArg;` |
|  4531252 | 1925 | `	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record` |
|        - | 1926 | `	                            * stack (BYTECODE stage 2); NULL = executing the` |
|        - | 1927 | `	                            * bottom activation. */` |
|        - | 1928 | `	VmExecState sState; /* This activation's boundary state (BYTECODE.md stage 1):` |
|        - | 1929 | `	                     * everything a suspended/nested activation must restore.` |
|        - | 1930 | `	                     * pc/pTos stay in locals for the hot loop and are synced` |
|        - | 1931 | `	                     * into sState only around the call epilogue (stage 2 turns` |
|        - | 1932 | `	                     * that boundary into an explicit record push/pop). */` |
|        - | 1933 | `	sxi32 pc;` |
|        - | 1934 | `	sxi32 rc;` |
|  4531252 | 1935 | `	int bCallInitStamp = 0; /* PH7_OP_CALL_INIT: may this site's verdict be remembered? */` |
|  4531252 | 1936 | `	sState.aInstr = aInstr;` |
|  4531252 | 1937 | `	sState.pStack = pStack;` |
|  4531252 | 1938 | `	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */` |
|  4531252 | 1939 | `	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */` |
|  4531252 | 1940 | `	sState.pResult = pResult;` |
|  4531252 | 1941 | `	sState.pLastRef = pLastRef;` |
|  4531252 | 1942 | `	sState.pEnforceRetFunc = pEnforceRetFunc;` |
|  4531252 | 1943 | `	sState.is_callback = (sxu8)(is_callback ? 1 : 0);` |
|  4531252 | 1944 | `	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);` |
|        - | 1945 | `	/* Argument container */` |
|  4531252 | 1946 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|  4531252 | 1947 | `	if( nTos < 0 ){` |
|  1523205 | 1948 | `		pTos = &pStack[-1];` |
|   761560 | 1949 | `	}else{` |
|  3008052 | 1950 | `		pTos = &pStack[nTos];` |
|        - | 1951 | `	}` |
|  4531252 | 1952 | `	sState.pTos = pTos;` |
|  4531252 | 1953 | `	pHigh = pTos;` |
|  4531252 | 1954 | `	sState.pHigh = pHigh;` |
|  4531252 | 1955 | `	sState.pc = nPc;` |
|        - | 1956 | `	/* Finally-drain base. For a resumed generator/fiber TOP-LEVEL body, its own` |
|        - | 1957 | `	 * exception handlers were just re-published above the caller depth` |
|        - | 1958 | `	 * (VmRestoreCtxState), so the live SySetUsed over-counts; take the` |
|        - | 1959 | `	 * caller-depth base recorded on the ctx instead.` |
|        - | 1960 | `	 *` |
|        - | 1961 | `	 * The discriminator is the OPERAND STACK, not the frame: the body exec runs on` |
|        - | 1962 | `	 * the ctx's own pStack, whereas every nested frame-less mini-program run within` |
|        - | 1963 | `	 * it — a default-argument / constructor trampoline, or a catch/finally body via` |
|        - | 1964 | `	 * VmLocalExec — runs on a FRESH operand stack while SHARING pVm->pFrame (those` |
|        - | 1965 | `	 * mini-programs do not push a VM frame). A pFrame-only guard therefore misfires` |
|        - | 1966 | `	 * for such a mini-program and hands it the generator's low caller-base; its` |
|        - | 1967 | `	 * terminal OP_DONE then drains VmDrainFinally down to that base, tearing down a` |
|        - | 1968 | ``	 * live try that the surrounding finally had just opened (e.g. `finally { try {`` |
|        - | 1969 | ``	 * throw new E(); } catch (E) {} }` in a generator: the `new E()` trampoline's`` |
|        - | 1970 | `	 * OP_DONE popped the inner try before its OP_THROW ran, so the throw escaped` |
|        - | 1971 | `	 * uncaught). Requiring pStack == pCtx->pStack pins the override to the resumed` |
|        - | 1972 | `	 * body itself; nested mini-programs fall through to the correct live depth. */` |
|  4531247 | 1973 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame` |
|     3204 | 1974 | `	 && pStack == pVm->pActiveCtx->pStack ){` |
|     2601 | 1975 | `		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;` |
|     2601 | 1976 | `		sState.nFinallyActBase = pVm->pActiveCtx->nFinallyBase;` |
|     1303 | 1977 | `	}else{` |
|  4528656 | 1978 | `		sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|  4528656 | 1979 | `		sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|        - | 1980 | `	}` |
|  4531252 | 1981 | `	sState.pEntryFrame = pVm->pFrame;` |
|  4531252 | 1982 | `	pc = nPc;` |
|        - | 1983 | `	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call` |
|        - | 1984 | `	 * adopts its parked record segment here. VmResumeCtx hands the segment in` |
|        - | 1985 | `	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a` |
|        - | 1986 | `	 * nested mini-program/callback run inside the resumed body — after it rebased` |
|        - | 1987 | `	 * the segment's exception floors and pushed the resume value into the innermost` |
|        - | 1988 | `	 * stack. Locals switch to the innermost activation so the dispatch loop` |
|        - | 1989 | `	 * continues inside the callee; the record chain is restored so its completion` |
|        - | 1990 | `	 * unwinds back through the body. */` |
|  4531252 | 1991 | `	if( pAdoptSegment ){` |
|      123 | 1992 | `		VmParkedSegment *pSeg = pAdoptSegment;` |
|      123 | 1993 | `		pCallTop = pSeg->pCallTop;` |
|      123 | 1994 | `		sState = pSeg->sState;` |
|      123 | 1995 | `		aInstr = sState.aInstr;` |
|      123 | 1996 | `		pStack = sState.pStack;` |
|        - | 1997 | `		/* pc is already nPc (== pCtx->pc, the innermost's post-suspend pc) from the` |
|        - | 1998 | `		 * init above; only the stack/top move to the innermost activation. */` |
|      123 | 1999 | `		pTos = &pStack[nTos];   /* nTos == pCtx->nTos — innermost, resume value pushed */` |
|        - | 2000 | `		/* The parked state carries the innermost activation's watermark; the resume` |
|        - | 2001 | `		 * value was pushed above it, so take whichever is higher. */` |
|      123 | 2002 | `		pHigh = ( sState.pHigh > pTos ) ? sState.pHigh : pTos;` |
|      123 | 2003 | `		SyMemBackendFree(&pVm->sAllocator,pSeg); /* holder only; its contents are now live */` |
|       59 | 2004 | `	}` |
|        - | 2005 | `/*` |
|        - | 2006 | ` * Route an enforcement helper's (or yield-from delegate's) return code from inside` |
|        - | 2007 | ` * the main switch: proceed on SXRET_OK, abort on PH7_ABORT, and on PH7_EXCEPTION` |
|        - | 2008 | ` * resume at the landing pad of the body that actually caught the exception in place` |
|        - | 2009 | ` * (VmRecordedResume) or, when it was caught by an outer exec, unwind out of the VM` |
|        - | 2010 | ` * loop. Replaces the old "jump to pVm->pFrame's nearest iExceptionJump" which ran` |
|        - | 2011 | ` * the statement after the try even when the catch was at an enclosing frame (ROOT B,` |
|        - | 2012 | `` * face b — `yield from` over a throwing sub-generator).`` |
|        - | 2013 | ` */` |
|        - | 2014 | `/* Dispatch-routing macros: bodies live in vm_dispatch.h, written against the` |
|        - | 2015 | ` * VM_EXIT_* primitives. Here they bind to the loop's own control flow. */` |
|        - | 2016 | `#define VM_EXIT_BREAK break` |
|        - | 2017 | `#define VM_EXIT_ABORT goto Abort` |
|        - | 2018 | `#define VM_EXIT_EXCEPTION goto Exception` |
|        - | 2019 | `#include "vm_dispatch.h"` |
|        - | 2020 | `	/* Generator::throw() inject-at-yield: when this invocation is a resumed generator/fiber` |
|        - | 2021 | `	 * body carrying a pending injected exception, raise it HERE — once, before the dispatch` |
|        - | 2022 | `	 * loop (pc is already at the resume point) — so the existing OP_THROW route` |
|        - | 2023 | `	 * (VmThrowException + VmRecordedResume) lands it at the generator's own try/catch landing` |
|        - | 2024 | `	 * pad WITHOUT reconstructing the suspended exception frame on pVm->pFrame. Because` |
|        - | 2025 | `	 * pVm->pFrame stays the body, the return/finally/sRet subsystem is untouched (this is why` |
|        - | 2026 | `	 * the reverted frame-reconstruction approach's regression cannot recur). Behaviorally` |
|        - | 2027 | ``	 * identical to a `throw` executed at the yield point: if the generator's own try catches`` |
|        - | 2028 | `	 * it we resume after the try; otherwise it propagates to the throw() caller (ROOT B lands` |
|        - | 2029 | `	 * the caller's handler) and the ctx closes. pInjected is one-shot and only meaningful at` |
|        - | 2030 | `	 * the resume pc, so this is checked once at entry — NOT per-instruction — keeping the hot` |
|        - | 2031 | `	 * dispatch loop untouched for all normal code. The pFrame gate keeps a nested call / catch` |
|        - | 2032 | `	 * mini-program sharing the ctx (a separate VmByteCodeExec entry) from re-firing.` |
|        - | 2033 | `	 *` |
|        - | 2034 | ``	 * Exception: when this body is suspended mid `yield from` over an inner Generator`` |
|        - | 2035 | `	 * (iDelegateState==3), a Generator::throw() on the OUTER generator must be forwarded` |
|        - | 2036 | `	 * INTO the delegate (PHP yield-from transparency), not raised here. Leave pInjected` |
|        - | 2037 | `	 * set and skip; the resume pc is that OP_YIELD_FROM, which consumes and forwards it. */` |
|        - | 2038 | `	/* pAdoptSegment says the same thing for a DEEP suspend: this invocation IS the` |
|        - | 2039 | `	 * resume, but the adopt above moved sState to the innermost parked activation,` |
|        - | 2040 | `	 * so its entry frame is that callee's and no longer the body's. Fiber::throw()` |
|        - | 2041 | `	 * on a fiber suspended inside a nested call has to raise THERE -- at the` |
|        - | 2042 | ``	 * `Fiber::suspend()` the callee is parked on -- which is where pVm->pFrame and`` |
|        - | 2043 | `	 * the adopted aInstr already point. */` |
|  4531247 | 2044 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected` |
|     1707 | 2045 | `	 && (pAdoptSegment != 0 \|\| pVm->pActiveCtx->pFrame == sState.pEntryFrame)` |
|       69 | 2046 | `	 && pVm->pActiveCtx->iDelegateState != 3 ){` |
|       64 | 2047 | `		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;` |
|        - | 2048 | `		VmFrame *pThrowFrame;` |
|        - | 2049 | `		sxi32 iResumePc;` |
|       64 | 2050 | `		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */` |
|        - | 2051 | `		/* Raising the injection at the yield-from point abandons any array/Iterator` |
|        - | 2052 | `		 * delegation in progress (state 1/2 — state 3 was forwarded, not raised here).` |
|        - | 2053 | `		 * Tear the delegate down so that if the generator's own try catches this and` |
|        - | 2054 | ``		 * runs on to a LATER `yield from`, that opcode classifies its operand fresh`` |
|        - | 2055 | `		 * instead of resuming this now-stale delegate cursor. */` |
|       64 | 2056 | `		if( pVm->pActiveCtx->iDelegateState != 0 ){` |
|        3 | 2057 | `			PH7_MemObjRelease(&pVm->pActiveCtx->sDelegate);` |
|        3 | 2058 | `			pVm->pActiveCtx->pDelegateNode = 0;` |
|        3 | 2059 | `			pVm->pActiveCtx->iDelegateState = 0;` |
|        1 | 2060 | `		}` |
|       64 | 2061 | `		pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       64 | 2062 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|       64 | 2063 | `		rc = VmThrowException(&(*pVm),pInj);` |
|       64 | 2064 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2065 | `			goto Abort;` |
|        - | 2066 | `		}` |
|       64 | 2067 | `		if( VmInlineOwnedBy(pVm,aInstr,sState.pEntryFrame) ){` |
|        - | 2068 | `			/* ROOT C: the inject was caught by an inline try in THIS generator. Drain the` |
|        - | 2069 | `			 * abandoned mid-expression operands and land at the catch/finally body. This is` |
|        - | 2070 | `			 * the pre-loop path (the first fetch uses pc directly), so no -1 adjustment. */` |
|       92 | 2071 | `			while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|       48 | 2072 | `				PH7_MemObjRelease(pTos);` |
|       48 | 2073 | `				pTos--;` |
|        4 | 2074 | `			}` |
|       48 | 2075 | `			pc = (sxi32)pVm->iInlinePc;` |
|       48 | 2076 | `			pVm->pInlineInstr = 0;` |
|       48 | 2077 | `			pVm->pInlineFrame = 0;` |
|       40 | 2078 | `		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 2079 | `			/* Caught by THIS generator's own try. (VmRecordedResume returns FALSE unless a` |
|        - | 2080 | `			 * catch recorded a resume target for this exec, so rc need not be pre-checked;` |
|        - | 2081 | `			 * unlike OP_THROW there is no lexical-try fallthrough here — the else just` |
|        - | 2082 | `			 * propagates.) Drain the abandoned mid-expression operand slots back to the` |
|        - | 2083 | `			 * catching try's base, then land at its pad (iResumePc is landing-1 for the` |
|        - | 2084 | `			 * dispatcher's trailing pc++; the loop below fetches at pc with no leading pc++,` |
|        - | 2085 | `			 * so add 1 to land on the pad itself). */` |
|        3 | 2086 | `			while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      ! 0 | 2087 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 | 2088 | `				pTos--;` |
|      ! 0 | 2089 | `			}` |
|        3 | 2090 | `			pc = iResumePc + 1;` |
|        2 | 2091 | `		}else{` |
|        - | 2092 | `			/* Not caught in this generator (no match, or caught by an outer/caller frame` |
|        - | 2093 | `			 * that ROOT B will land at its own OP_CALL site): propagate out so the ctx` |
|        - | 2094 | `			 * closes and the caller sees the exception. */` |
|       16 | 2095 | `			goto Exception;` |
|        - | 2096 | `		}` |
|       23 | 2097 | `	}` |
|        - | 2098 | `	/* Force-close entry (VmCloseCtx): a suspended generator being destroyed runs its` |
|        - | 2099 | ``	 * pending `finally` blocks. Instead of resuming at the yield, redirect straight`` |
|        - | 2100 | ``	 * into the innermost open try's finally as if a `return` had crossed every`` |
|        - | 2101 | `	 * enclosing finally (mirrors OP_SET_FINALLY_RET; OP_END_FINALLY then threads the` |
|        - | 2102 | `	 * PH7_FA_RETURN out through the whole chain and completes the body). Gate on the` |
|        - | 2103 | `	 * BODY bytecode (aInstr == the function program) so nested mini-programs sharing` |
|        - | 2104 | `	 * this ctx/frame never re-fire it; bClosing stays set so OP_YIELD can reject a` |
|        - | 2105 | `	 * yield reached inside one of these finallys. */` |
|  4531291 | 2106 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing` |
|     1777 | 2107 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|      223 | 2108 | `	 && aInstr == (VmInstr *)SySetBasePtr(&pVm->pActiveCtx->pFunc->aByteCode) ){` |
|        - | 2109 | `		VmFinallyAction sAct;` |
|      223 | 2110 | `		sxu32 iFpc = 0;` |
|      223 | 2111 | `		int nCross = -1; /* cross every enclosing finally of this body */` |
|      223 | 2112 | `		SyZero(&sAct,sizeof(sAct));` |
|      223 | 2113 | `		sAct.eKind = PH7_FA_RETURN;` |
|      223 | 2114 | `		sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|      223 | 2115 | `		PH7_MemObjInit(pVm,&sAct.sRet); /* discarded return; getReturn() is moot post-close */` |
|      223 | 2116 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|       21 | 2117 | `			sAct.nCross = nCross;` |
|       21 | 2118 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       21 | 2119 | `			pc = (sxi32)iFpc; /* pre-loop: the first fetch uses pc directly (no -1) */` |
|       11 | 2120 | `		}else{` |
|        - | 2121 | `			/* No open try had a finally: nothing to run, complete the body. */` |
|      203 | 2122 | `			PH7_MemObjRelease(&sAct.sRet);` |
|      203 | 2123 | `			goto Done;` |
|        - | 2124 | `		}` |
|       10 | 2125 | `	}` |
|        - | 2126 | `	/* Execute as much as we can */` |
| 37508572 | 2127 | `	for(;;){` |
|      ! 0 | 2128 | `VmLoopFetch:` |
|        - | 2129 | `		/* C-boundary throw routing (band A #1): a PHP callee invoked from a C` |
|        - | 2130 | `		 * site with no status channel — a __toString/__toInt cast, __get/__set/` |
|        - | 2131 | `		 * offsetGet/offsetSet, __clone, __destruct, a user callback inside a` |
|        - | 2132 | `		 * builtin — raised, and the site continued with a fallback value; the` |
|        - | 2133 | `		 * invocation boundary parked the status here (VmBoundaryPark). Route it` |
|        - | 2134 | `		 * exactly as the throw site's dispatch macro would have: abort, land at` |
|        - | 2135 | `		 * an inline-try redirect, resume at a recorded in-place catch (draining` |
|        - | 2136 | `		 * the abandoned mid-expression operands to the catching try's base), or` |
|        - | 2137 | `		 * propagate out of this exec. Checked at the fetch point, so a swallowed` |
|        - | 2138 | `		 * throw outlives at most the C remainder of ONE opcode instead of` |
|        - | 2139 | `		 * silently resuming the surrounding PHP code with a bogus value.` |
|        - | 2140 | `		 * The pending write-back sweep shares this one guard so the hot` |
|        - | 2141 | `		 * no-hooks path pays a single predicted branch per fetch. */` |
| 76304356 | 2142 | `		if( pVm->nBoundaryRc != 0 \|\| SySetUsed(&pVm->aHookRmw) > 0` |
| 76296770 | 2143 | `		 \|\| PH7_PcntlAsyncPending \|\| pVm->sAllocator.nMemTried != 0` |
| 76296528 | 2144 | `		 \|\| pVm->bGcWanted \|\| pVm->bClosurePurge ){` |
|    18152 | 2145 | `			if( pVm->bClosurePurge ){` |
|        - | 2146 | `				/* Run-time closures whose last holder went. Freed HERE and not at the` |
|        - | 2147 | `				 * drop, because the drop is usually a dispatch releasing the Closure` |
|        - | 2148 | `				 * object it has just unwrapped and is about to look up by name. */` |
|    16789 | 2149 | `				PH7_VmPurgeDeadClosures(&(*pVm));` |
|     8272 | 2150 | `			}` |
|    18152 | 2151 | `			if( pVm->bGcWanted ){` |
|        - | 2152 | `				/* The cycle collector's root buffer filled. HERE is the only place` |
|        - | 2153 | `				 * it may run: between two instructions, with the operand stack` |
|        - | 2154 | `				 * consistent and no C builtin holding a raw ph7_value* across it.` |
|        - | 2155 | `				 * The drop that buffered the root was in the middle of an opcode's` |
|        - | 2156 | `				 * C body, which is no place to be running destructors. */` |
|       42 | 2157 | `				PH7_GcCollect(&(*pVm));` |
|       20 | 2158 | `			}` |
|    18152 | 2159 | `			if( pVm->sAllocator.nMemTried != 0 ){` |
|        - | 2160 | `				/* memory_limit: an allocation asked for more than the script's` |
|        - | 2161 | `				 * remaining budget and was refused. The allocator cannot raise` |
|        - | 2162 | `				 * anything itself -- it has no VM and no unwind -- so it recorded` |
|        - | 2163 | `				 * the size and disarmed the ceiling, and HERE, at the same safe` |
|        - | 2164 | `				 * point the pcntl handlers and the C-boundary throws use, it` |
|        - | 2165 | `				 * becomes php's fatal.` |
|        - | 2166 | `				 *` |
|        - | 2167 | `				 * php's text exactly, including the two byte counts: this is the` |
|        - | 2168 | `				 * message every framework's OOM triage greps for. Severity 256 is` |
|        - | 2169 | `				 * what makes the label read "Fatal error" -- PHL's label table maps` |
|        - | 2170 | `				 * E_ERROR(1) to its own "Error", and this diagnostic is one users` |
|        - | 2171 | `				 * match against php's, not against the engine's house style.` |
|        - | 2172 | `				 *` |
|        - | 2173 | `				 * The refused allocation has already returned NULL into whatever` |
|        - | 2174 | `				 * asked for it, so the abort is not optional: the caller is holding` |
|        - | 2175 | `				 * a failure it may not check, and the next instruction must not run.` |
|        - | 2176 | `				 */` |
|      ! 0 | 2177 | `				sxu32 nLimit = pVm->sAllocator.nMemLimitHit;` |
|      ! 0 | 2178 | `				sxu32 nTried = pVm->sAllocator.nMemTried;` |
|      ! 0 | 2179 | `				pVm->sAllocator.nMemTried = 0;` |
|      ! 0 | 2180 | `				VmErrorFormat(&(*pVm),256,` |
|        - | 2181 | `					"Allowed memory size of %u bytes exhausted (tried to allocate %u bytes)",` |
|      ! 0 | 2182 | `					nLimit,nTried);` |
|      ! 0 | 2183 | `				goto Abort;` |
|        - | 2184 | `			}` |
|    18152 | 2185 | `			if( PH7_PcntlAsyncPending && pVm->nBoundaryRc == 0 ){` |
|        - | 2186 | `				/* ext/pcntl with pcntl_async_signals(true): a C signal handler` |
|        - | 2187 | `				 * recorded a delivery and raised this flag, and HERE is the` |
|        - | 2188 | `				 * safe point php's EG(vm_interrupt) picks too -- between two` |
|        - | 2189 | `				 * instructions, with the operand stack consistent. The PHP` |
|        - | 2190 | `				 * handlers run now; a throw from one of them is parked in` |
|        - | 2191 | `				 * nBoundaryRc by the callback dispatcher and routed by the` |
|        - | 2192 | `				 * very next branch, exactly as any other C-boundary throw.` |
|        - | 2193 | `				 * A throw that is ALREADY parked wins: running a handler on` |
|        - | 2194 | `				 * top of somebody else's unwind is not a safe point at all,` |
|        - | 2195 | `				 * so the flag is left standing for the next fetch. */` |
|        2 | 2196 | `				PH7_PcntlDrainAsync(&(*pVm));` |
|      ! 0 | 2197 | `			}` |
|    18152 | 2198 | `			if( pVm->nBoundaryRc != 0 ){` |
|      839 | 2199 | `				sxi32 rcBr = pVm->nBoundaryRc;` |
|      839 | 2200 | `				pVm->nBoundaryRc = 0;` |
|      839 | 2201 | `				if( rcBr == PH7_ABORT ){` |
|       13 | 2202 | `					goto Abort;` |
|        - | 2203 | `				}` |
|      829 | 2204 | `				if( VmInlineOwnedBy(pVm,aInstr,sState.pEntryFrame) ){` |
|        - | 2205 | `					/* Caught by an inline try (generator body) THIS exec owns: drain` |
|        - | 2206 | `					 * and land (pre-fetch path: pc is used directly, no trailing ++). */` |
|      ! 0 | 2207 | `					while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|      ! 0 | 2208 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 | 2209 | `						pTos--;` |
|      ! 0 | 2210 | `					}` |
|      ! 0 | 2211 | `					pc = (sxi32)pVm->iInlinePc;` |
|      ! 0 | 2212 | `					pVm->pInlineInstr = 0;` |
|      ! 0 | 2213 | `					pVm->pInlineFrame = 0;` |
|      ! 0 | 2214 | `				}else{` |
|        - | 2215 | `					sxi32 iBrPc;` |
|      829 | 2216 | `					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){` |
|        - | 2217 | `						/* Caught in place by a try THIS exec owns: drain the abandoned` |
|        - | 2218 | `						 * operands to the catching try's base and land at its pad` |
|        - | 2219 | `						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;` |
|        - | 2220 | `						 * pre-fetch here, so +1 lands on the pad itself). */` |
|      689 | 2221 | `						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      329 | 2222 | `							PH7_MemObjRelease(pTos);` |
|      329 | 2223 | `							pTos--;` |
|        5 | 2224 | `						}` |
|      365 | 2225 | `						pc = iBrPc + 1;` |
|      184 | 2226 | `					}else{` |
|        - | 2227 | `						/* Caught by an outer exec (or uncaught-with-report pending):` |
|        - | 2228 | `						 * unwind out of this exec; the owner's macros/router land it. */` |
|      469 | 2229 | `						goto Exception;` |
|        - | 2230 | `					}` |
|        - | 2231 | `				}` |
|      179 | 2232 | `			}` |
|        - | 2233 | `			/* Stale write-back sweep: a pending entry whose OWNING activation is` |
|        - | 2234 | `			 * fetching any pc outside its armed window [nJmpPc, nPc] is dead — for` |
|        - | 2235 | `			 * an RMW entry (window = the modify op alone) a routed throw abandoned` |
|        - | 2236 | `			 * the arming statement mid-flight; for a ??= entry either a throw` |
|        - | 2237 | `			 * abandoned the RHS or the short-circuit jump landed past the` |
|        - | 2238 | `			 * OP_NULLC_STORE (the assign is skipped). Drop it — no set dispatch` |
|        - | 2239 | `			 * (php: the throw/skip discards the write). A nested exec (even a` |
|        - | 2240 | `			 * recursive one over the same bytecode) has a different operand-stack` |
|        - | 2241 | `			 * base and leaves enclosing entries alone; entries below a live top` |
|        - | 2242 | `			 * are reached as the drops expose them. */` |
|    17690 | 2243 | `			while( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      492 | 2244 | `				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      490 | 2245 | `				if( pTopRmw->pOwnerStack != (void *)pStack \|\| pTopRmw->pInstrs != (void *)aInstr` |
|      184 | 2246 | `				 \|\| ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){` |
|      241 | 2247 | `					break; /* not ours, or legitimately in flight */` |
|        - | 2248 | `				}` |
|       13 | 2249 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 2250 | `			}` |
|     8715 | 2251 | `		}` |
| 76303887 | 2252 | `		if( pTos > pHigh ){` |
|  2335601 | 2253 | `			pHigh = pTos;   /* the activation's high-water mark; see pHigh's declaration */` |
|  1166755 | 2254 | `		}` |
|        - | 2255 | `		/* Fetch the instruction to execute */` |
| 76303887 | 2256 | `		pInstr = &aInstr[pc];` |
| 76303887 | 2257 | `		if( pInstr->nLine ){` |
|        - | 2258 | `			/* Publish the source position for diagnostics, debug_backtrace() and` |
|        - | 2259 | `			 * Throwable. Instructions the compiler could not attribute (nLine 0)` |
|        - | 2260 | `			 * leave the last known line standing rather than reporting line 0.` |
|        - | 2261 | `			 * The compilation unit's strict_types mode rides the same gate: a` |
|        - | 2262 | `			 * SYNTHETIC instruction (nLine 0) must not claim to speak for a file. */` |
| 70510120 | 2263 | `			pVm->nCurLine = pInstr->nLine;` |
| 70510120 | 2264 | `			pVm->bCurStrict = pInstr->bStrict;` |
| 35271506 | 2265 | `		}` |
| 76303887 | 2266 | `		rc = SXRET_OK;` |
|        - | 2267 | `/*` |
|        - | 2268 | ` * What follows here is a massive switch statement where each case implements a` |
|        - | 2269 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - | 2270 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - | 2271 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - | 2272 | ` * the switch statement will break with convention and be flush-left.` |
|        - | 2273 | ` */` |
| 76303887 | 2274 | `		switch(pInstr->iOp){` |
|        - | 2275 | `/*` |
|        - | 2276 | ` * DONE: P1 * *` |
|        - | 2277 | ` *` |
|        - | 2278 | ` * Program execution completed: Clean up the mess left behind` |
|        - | 2279 | ` * and return immediately.` |
|        - | 2280 | ` */` |
|  2199730 | 2281 | `case PH7_OP_DONE:` |
|  4398867 | 2282 | `	if( pInstr->iP2 && sState.bReturnPropagates ){` |
|        - | 2283 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - | 2284 | `		 * onto the body frame this catch/finally returns from (skip the transparent` |
|        - | 2285 | `		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION` |
|        - | 2286 | `		 * materializes it into sState.pResult. Drain any finally opened within this body` |
|        - | 2287 | `		 * first (nested try/finally inside the catch), which may overwrite the same` |
|        - | 2288 | `		 * frame's slot (finally-over-catch). */` |
|    24013 | 2289 | `		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|    24013 | 2290 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    23983 | 2291 | `			PH7_MemObjStore(pTos,&pTgt->sRet);` |
|    23983 | 2292 | `			VmPopOperand(&pTos,1);` |
|    11994 | 2293 | `		}else{` |
|       32 | 2294 | ``			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */`` |
|        - | 2295 | `		}` |
|    24013 | 2296 | `		pTgt->bHasRet = 1;` |
|    24013 | 2297 | `		pTgt->nRetGen++;` |
|    24013 | 2298 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    24013 | 2299 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2300 | `			goto Abort;` |
|        - | 2301 | `		}` |
|    24013 | 2302 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 2303 | `			/* A drained finally threw past itself — it discards this return. */` |
|      ! 0 | 2304 | `			goto Exception;` |
|        - | 2305 | `		}` |
|    24013 | 2306 | `		goto Done;` |
|        - | 2307 | `	}` |
|        - | 2308 | ``	/* php's `Only variable references should be returned by reference`, raised at`` |
|        - | 2309 | ``	 * the RETURN and nowhere else: a function DECLARED `&` whose return expression`` |
|        - | 2310 | `	 * is not a variable has nothing to bind, and php says so whether the caller` |
|        - | 2311 | `	 * went on to take the answer by reference or by value. Falling off the end and` |
|        - | 2312 | ``	 * a bare `return;` count too -- both leave it with no variable. The`` |
|        - | 2313 | `	 * VM_FRAME_THROW guard is the one the return-type check below uses: a function` |
|        - | 2314 | `	 * unwinding through this terminal OP_DONE never returned anything. */` |
|  4374854 | 2315 | `	if( sState.pEnforceRetFunc` |
|  2200030 | 2316 | `	 && (sState.pEnforceRetFunc->iFlags & VM_FUNC_REF_RETURN)` |
|    12493 | 2317 | `	 && (sState.pEnforceRetFunc->iFlags & VM_FUNC_GENERATOR) == 0` |
|       88 | 2318 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW)` |
|       93 | 2319 | `	 && ( !(pInstr->iP1 && pTos >= pStack) \|\| pTos->nIdx == SXU32_HIGH ) ){` |
|        - | 2320 | ``		/* A `return` reports on its own line; falling off the END of the body has no`` |
|        - | 2321 | `		 * return to report on, and php names the closing brace there. */` |
|        7 | 2322 | `		sxu32 nSavedLine = pVm->nCurLine;` |
|        7 | 2323 | `		if( !(pInstr->iP1 && pTos >= pStack) && sState.pEnforceRetFunc->nEndLine > 0 ){` |
|        3 | 2324 | `			pVm->nCurLine = sState.pEnforceRetFunc->nEndLine;` |
|        1 | 2325 | `		}` |
|        7 | 2326 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - | 2327 | `			"Only variable references should be returned by reference");` |
|        7 | 2328 | `		pVm->nCurLine = nSavedLine;` |
|        3 | 2329 | `	}` |
|        - | 2330 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - | 2331 | `	 * the fiber start/resume paths) set sState.pEnforceRetFunc, so this branch is` |
|        - | 2332 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - | 2333 | `	 * callback trampolines, and the main script. */` |
|  4374854 | 2334 | `	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)` |
|    25312 | 2335 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - | 2336 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - | 2337 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - | 2338 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - | 2339 | `		 * value the function never actually returned, so enforcing here would` |
|        - | 2340 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - | 2341 | `		 * exception. */` |
|    25268 | 2342 | `		ph7_value *pRetVal = 0;` |
|    25268 | 2343 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    20416 | 2344 | `			pRetVal = pTos;` |
|    10122 | 2345 | `		}` |
|    25268 | 2346 | `		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);` |
|    25268 | 2347 | `		if( rc == PH7_ABORT ) goto Abort;` |
|    25264 | 2348 | `		if( rc == PH7_EXCEPTION ){` |
|      149 | 2349 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      115 | 2350 | `				PH7_MemObjRelease(pTos);` |
|      115 | 2351 | `				pTos--;` |
|       55 | 2352 | `			}` |
|      149 | 2353 | `			goto Exception;` |
|        - | 2354 | `		}` |
|        - | 2355 | `		/* Don't enforce twice if the function loops through multiple` |
|        - | 2356 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - | 2357 | `		 * defensively we clear the pointer after a successful check). */` |
|    25120 | 2358 | `		sState.pEnforceRetFunc = 0;` |
|    12331 | 2359 | `	}` |
|  4374711 | 2360 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|  2889355 | 2361 | `		if( sState.pLastRef ){` |
|   155379 | 2362 | `			*sState.pLastRef = pTos->nIdx;` |
|    77723 | 2363 | `		}` |
|  2889355 | 2364 | `		if( sState.pResult ){` |
|        - | 2365 | `			/* Execution result */` |
|  1399627 | 2366 | `			PH7_MemObjStore(pTos,sState.pResult);` |
|   699733 | 2367 | `		}` |
|  2889355 | 2368 | `		VmPopOperand(&pTos,1);` |
|  1444566 | 2369 | `	}else{` |
|  1485361 | 2370 | `		if( pInstr->iP1 == 0 && pInstr->iP2 && sState.pResult ){` |
|        - | 2371 | ``			/* An EXPLICIT `return;` with no value answers NULL. It reads as a`` |
|        - | 2372 | `			 * no-op for a function (whose result slot starts out null anyway)` |
|        - | 2373 | `			 * and matters for an included CHUNK, whose slot is seeded with the 1` |
|        - | 2374 | ``			 * a file that returns nothing answers: `<?php return;` is php's`` |
|        - | 2375 | `			 * NULL, not that 1. */` |
|      102 | 2376 | `			PH7_MemObjRelease(sState.pResult);` |
|       49 | 2377 | `		}` |
|        - | 2378 | `		/* Nothing referenced — also the throw-unwind path: the compiler routes` |
|        - | 2379 | `		 * an uncaught exception to this terminal OP_DONE with iP1 set but an` |
|        - | 2380 | `		 * empty operand stack (pTos == pStack-1), so there is no return value to` |
|        - | 2381 | `		 * store. Guarding on pTos >= pStack (matching the sibling branch above)` |
|        - | 2382 | `		 * avoids the below-base read that crashed under glibc/ASan. */` |
|  1485361 | 2383 | `		if( sState.pLastRef ){` |
|    21007 | 2384 | `			*sState.pLastRef = SXU32_HIGH;` |
|    10356 | 2385 | `		}` |
|        - | 2386 | `	}` |
|        - | 2387 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - | 2388 | `	 * this execution. When 'return' is used inside a try block,` |
|        - | 2389 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - | 2390 | `	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering` |
|        - | 2391 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - | 2392 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - | 2393 | `	 * block can override it (the finally writes this body frame's sRet slot,` |
|        - | 2394 | `	 * materialized below).` |
|        - | 2395 | `	 */` |
|  4374711 | 2396 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|  4374711 | 2397 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2398 | `		goto Abort;` |
|        - | 2399 | `	}` |
|  4374711 | 2400 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 2401 | `		/* A drained finally threw past itself, discarding the value this OP_DONE` |
|        - | 2402 | `		 * stored into sState.pResult. If an enclosing try IN THIS function caught the new` |
|        - | 2403 | `		 * exception in place, resume at its landing pad; otherwise unwind (the` |
|        - | 2404 | `		 * caller's exception-resume pops the stored result). */` |
|        - | 2405 | `		sxi32 iResumePc;` |
|        5 | 2406 | `		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 2407 | `			pc = iResumePc;` |
|        3 | 2408 | `			break;` |
|        - | 2409 | `		}` |
|        3 | 2410 | `		goto Exception;` |
|        - | 2411 | `	}` |
|  4374707 | 2412 | `	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){` |
|        - | 2413 | `		/* A catch/finally issued a 'return' targeting THIS body. If the body is` |
|        - | 2414 | `		 * actually unwinding because an exception escaped it (terminal OP_DONE on` |
|        - | 2415 | `		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type` |
|        - | 2416 | `		 * enforcement above), that exception supersedes the return: discard it.` |
|        - | 2417 | `		 * Otherwise materialize it as this function's result. */` |
|       12 | 2418 | `		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){` |
|      ! 0 | 2419 | `			VmClearFramePending(sState.pEntryFrame);` |
|      ! 0 | 2420 | `		}else{` |
|       12 | 2421 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|        - | 2422 | `		}` |
|        4 | 2423 | `	}` |
|  4374707 | 2424 | `	goto Done;` |
|        - | 2425 | `/*` |
|        - | 2426 | ` * HALT: P1 * *` |
|        - | 2427 | ` *` |
|        - | 2428 | ` * Program execution aborted: Clean up the mess left behind` |
|        - | 2429 | ` * and abort immediately.` |
|        - | 2430 | ` */` |
|       53 | 2431 | `case PH7_OP_HALT:` |
|      117 | 2432 | `	if( pInstr->iP1 ){` |
|        - | 2433 | `#ifdef UNTRUST` |
|        - | 2434 | `		if( pTos < pStack ){` |
|        - | 2435 | `			goto Abort;` |
|        - | 2436 | `		}` |
|        - | 2437 | `#endif` |
|      117 | 2438 | `		if( sState.pLastRef ){` |
|       65 | 2439 | `			*sState.pLastRef = pTos->nIdx;` |
|       31 | 2440 | `		}` |
|      117 | 2441 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       79 | 2442 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - | 2443 | `				/* Output the exit message */` |
|      120 | 2444 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|       41 | 2445 | `					pVm->sVmConsumer.pUserData);` |
|       79 | 2446 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|       45 | 2447 | `			}` |
|       82 | 2448 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - | 2449 | `			/* Record exit status */` |
|       41 | 2450 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|       19 | 2451 | `		}` |
|      117 | 2452 | `		VmPopOperand(&pTos,1);` |
|       60 | 2453 | `	}else if( sState.pLastRef ){` |
|        - | 2454 | `		/* Nothing referenced */` |
|      ! 0 | 2455 | `		*sState.pLastRef = SXU32_HIGH;` |
|      ! 0 | 2456 | `	}` |
|        - | 2457 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - | 2458 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - | 2459 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - | 2460 | `	 */` |
|      117 | 2461 | `	pVm->bHaltRequested = 1;` |
|      117 | 2462 | `	goto Abort;` |
|        - | 2463 | `/*` |
|        - | 2464 | ` * JMP: * P2 *` |
|        - | 2465 | ` *` |
|        - | 2466 | ` * Unconditional jump: The next instruction executed will be` |
|        - | 2467 | ` * the one at index P2 from the beginning of the program.` |
|        - | 2468 | ` */` |
|   882269 | 2469 | `case PH7_OP_JMP:` |
|  1764835 | 2470 | `	pc = pInstr->iP2 - 1;` |
|  1764835 | 2471 | `	break;` |
|        - | 2472 | `/*` |
|        - | 2473 | ` * JZ: P1 P2 *` |
|        - | 2474 | ` *` |
|        - | 2475 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - | 2476 | ` * entry in the stack if P1 is zero.` |
|        - | 2477 | ` */` |
|  2142488 | 2478 | `case PH7_OP_JZ:` |
|        - | 2479 | `#ifdef UNTRUST` |
|        - | 2480 | `	if( pTos < pStack ){` |
|        - | 2481 | `		goto Abort;` |
|        - | 2482 | `	}` |
|        - | 2483 | `#endif` |
|        - | 2484 | `	/* Get a boolean value */` |
|  4290661 | 2485 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    35891 | 2486 | `		PH7_MemObjToBool(pTos);` |
|    17937 | 2487 | `	}` |
|  4290661 | 2488 | `	if( !pTos->x.iVal ){` |
|        - | 2489 | `		/* Take the jump */` |
|  2374995 | 2490 | `		pc = pInstr->iP2 - 1;` |
|  1189202 | 2491 | `	}` |
|  4290661 | 2492 | `	if( !pInstr->iP1 ){` |
|  3754974 | 2493 | `		VmPopOperand(&pTos,1);` |
|  1879991 | 2494 | `	}` |
|  4290661 | 2495 | `	break;` |
|        - | 2496 | `/*` |
|        - | 2497 | ` * JNZ: P1 P2 *` |
|        - | 2498 | ` *` |
|        - | 2499 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - | 2500 | ` * entry in the stack if P1 is zero.` |
|        - | 2501 | ` */` |
|   162791 | 2502 | `case PH7_OP_JNZ:` |
|        - | 2503 | `#ifdef UNTRUST` |
|        - | 2504 | `	if( pTos < pStack ){` |
|        - | 2505 | `		goto Abort;` |
|        - | 2506 | `	}` |
|        - | 2507 | `#endif` |
|        - | 2508 | `	/* Get a boolean value */` |
|   326214 | 2509 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 | 2510 | `		PH7_MemObjToBool(pTos);` |
|        1 | 2511 | `	}` |
|   326214 | 2512 | `	if( pTos->x.iVal ){` |
|        - | 2513 | `		/* Take the jump */` |
|    11207 | 2514 | `		pc = pInstr->iP2 - 1;` |
|     5603 | 2515 | `	}` |
|   326214 | 2516 | `	if( !pInstr->iP1 ){` |
|       37 | 2517 | `		VmPopOperand(&pTos,1);` |
|       19 | 2518 | `	}` |
|   326214 | 2519 | `	break;` |
|        - | 2520 | `/*` |
|        - | 2521 | ` * NOOP: * * *` |
|        - | 2522 | ` *` |
|        - | 2523 | ` * Do nothing. This instruction is often useful as a jump` |
|        - | 2524 | ` * destination.` |
|        - | 2525 | ` */` |
|        3 | 2526 | `case PH7_OP_NOOP:` |
|        6 | 2527 | `	break;` |
|        - | 2528 | `/*` |
|        - | 2529 | ` * POP: P1 * *` |
|        - | 2530 | ` *` |
|        - | 2531 | ` * Pop P1 elements from the operand stack.` |
|        - | 2532 | ` */` |
|  1817975 | 2533 | `case PH7_OP_POP: {` |
|  3638583 | 2534 | `	sxi32 n = pInstr->iP1;` |
|  3638583 | 2535 | `	if( &pTos[-n+1] < pStack ){` |
|        - | 2536 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 | 2537 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 | 2538 | `	}` |
|  3638583 | 2539 | `	VmPopOperand(&pTos,n);` |
|  3638583 | 2540 | `	break;` |
|        - | 2541 | `				 }` |
|        - | 2542 | `/*` |
|        - | 2543 | ` * DUP: * * *` |
|        - | 2544 | ` *` |
|        - | 2545 | ` * Duplicate the top of the stack.` |
|        - | 2546 | ` */` |
|      207 | 2547 | `case PH7_OP_DUP:` |
|        - | 2548 | `#ifdef UNTRUST` |
|        - | 2549 | `	if( pTos < pStack ){` |
|        - | 2550 | `		goto Abort;` |
|        - | 2551 | `	}` |
|        - | 2552 | `#endif` |
|      419 | 2553 | `	pTos++;` |
|      419 | 2554 | `	PH7_MemObjInit(pVm,pTos);` |
|      419 | 2555 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|      419 | 2556 | `	break;` |
|        - | 2557 | `/*` |
|        - | 2558 | ` * OP_FUNC_DECL * * P3` |
|        - | 2559 | ` *` |
|        - | 2560 | ` * Bind the function p3 names, here, where the declaration STATEMENT sits. Only a` |
|        - | 2561 | ` * conditional declaration takes this route: a top-level one is bound while its unit` |
|        - | 2562 | ` * compiles, exactly as php early-binds it.` |
|        - | 2563 | ` */` |
|       65 | 2564 | `case PH7_OP_FUNC_DECL: {` |
|      134 | 2565 | `	ph7_vm_func *pDeclFunc = (ph7_vm_func *)pInstr->p3;` |
|      134 | 2566 | `	if( pDeclFunc ){` |
|      199 | 2567 | `		SyHashEntry *pDeclEntry = SyHashGet(&pVm->hFunction,` |
|      130 | 2568 | `			SyStringData(&pDeclFunc->sName),SyStringLength(&pDeclFunc->sName));` |
|      134 | 2569 | `		if( pDeclEntry && pDeclEntry->pUserData != (void *)pDeclFunc ){` |
|        - | 2570 | `			/* php's runtime redeclaration fatal -- the same sentence the compiler` |
|        - | 2571 | `			 * raises for two top-level declarations of one name. A BUILTIN of that` |
|        - | 2572 | `			 * name is not in this table, so a polyfill body that reaches here beside` |
|        - | 2573 | ``			 * one is the `function_exists()` guard having answered false. */`` |
|      ! 0 | 2574 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot redeclare %z()",&pDeclFunc->sName);` |
|      ! 0 | 2575 | `			pVm->iExitStatus = 255;` |
|      ! 0 | 2576 | `			pVm->bHaltRequested = 1;` |
|      ! 0 | 2577 | `			goto Abort;` |
|        - | 2578 | `		}` |
|      134 | 2579 | `		if( pDeclEntry == 0 ){` |
|      134 | 2580 | `			PH7_VmInstallUserFunction(&(*pVm),pDeclFunc,0);` |
|       65 | 2581 | `		}` |
|       65 | 2582 | `	}` |
|      134 | 2583 | `	break;` |
|        - | 2584 | `				}` |
|        - | 2585 | `/*` |
|        - | 2586 | ` * CLASS_DEFER: * * P3` |
|        - | 2587 | ` *` |
|        - | 2588 | ` * Execute a class declaration whose parent/interface/trait could not be` |
|        - | 2589 | ` * resolved when the enclosing file compiled (its autoloader had not RUN yet).` |
|        - | 2590 | ` * P3 is the VmDeferredClass record captured by the compiler. A dependency` |
|        - | 2591 | `` * that is STILL missing throws php's catchable `... not found` Error; a`` |
|        - | 2592 | ` * failed re-compile aborts (its errors were already reported). See the` |
|        - | 2593 | ` * deferral block comment in compile_class.c.` |
|        - | 2594 | ` */` |
|       71 | 2595 | `case PH7_OP_CLASS_DEFER: {` |
|      147 | 2596 | `	VmDeferredClass *pDefer = (VmDeferredClass *)pInstr->p3;` |
|      147 | 2597 | `	VmDeferredReq *pMissing = 0;` |
|      147 | 2598 | `	sxi32 rcDecl = SXRET_OK;` |
|      147 | 2599 | `	if( pDefer ){` |
|      147 | 2600 | `		rcDecl = VmExecDeferredClass(&(*pVm),pDefer,&pMissing);` |
|       71 | 2601 | `	}` |
|      147 | 2602 | `	if( pMissing ){` |
|        - | 2603 | `		static const char *azDeferKind[] = { "Class", "Interface", "Trait" };` |
|        - | 2604 | `		char zDeclMsg[520];` |
|       17 | 2605 | `		sxu32 nDeclMsg = SyBufferFormat(zDeclMsg,sizeof(zDeclMsg),"%s \"%.*s\" not found",` |
|        5 | 2606 | `			azDeferKind[pMissing->cKind < 3 ? pMissing->cKind : 0],` |
|       10 | 2607 | `			(int)pMissing->sName.nByte,pMissing->sName.zString);` |
|       12 | 2608 | `		rc = VmThrowFromVm(&(*pVm),"Error",zDeclMsg,nDeclMsg);` |
|       12 | 2609 | `		if( rc == SXERR_ABORT ){` |
|        3 | 2610 | `			goto Abort;` |
|        - | 2611 | `		}` |
|        9 | 2612 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2613 | `	}` |
|      137 | 2614 | `	if( rcDecl == SXERR_ABORT ){` |
|      ! 0 | 2615 | `		goto Abort;` |
|        - | 2616 | `	}` |
|      137 | 2617 | `	break;` |
|        - | 2618 | `				}` |
|        - | 2619 | `/*` |
|        - | 2620 | ` * CVT_INT: * * *` |
|        - | 2621 | ` *` |
|        - | 2622 | ` * Force the top of the stack to be an integer.` |
|        - | 2623 | ` */` |
|     1358 | 2624 | `case PH7_OP_CVT_INT:` |
|        - | 2625 | `#ifdef UNTRUST` |
|        - | 2626 | `	if( pTos < pStack ){` |
|        - | 2627 | `		goto Abort;` |
|        - | 2628 | `	}` |
|        - | 2629 | `#endif` |
|     2688 | 2630 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|        - | 2631 | `		/* php warns from the conversion itself when no int can hold the float` |
|        - | 2632 | ``		 * (`(int)1e19`); the value it then answers is the modular wrap. */`` |
|     1890 | 2633 | `		PH7_MemObjWarnIntCast(pTos);` |
|     1890 | 2634 | `		PH7_MemObjToInteger(pTos);` |
|      926 | 2635 | `	}` |
|        - | 2636 | `	/* Invalidate any prior representation */` |
|     2688 | 2637 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|     2688 | 2638 | `	break;` |
|        - | 2639 | `/*` |
|        - | 2640 | ` * CVT_REAL: * * *` |
|        - | 2641 | ` *` |
|        - | 2642 | ` * Force the top of the stack to be a real.` |
|        - | 2643 | ` */` |
|       58 | 2644 | `case PH7_OP_CVT_REAL:` |
|        - | 2645 | `#ifdef UNTRUST` |
|        - | 2646 | `	if( pTos < pStack ){` |
|        - | 2647 | `		goto Abort;` |
|        - | 2648 | `	}` |
|        - | 2649 | `#endif` |
|      119 | 2650 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      119 | 2651 | `		PH7_MemObjToReal(pTos);` |
|       58 | 2652 | `	}` |
|        - | 2653 | `	/* Invalidate any prior representation */` |
|      119 | 2654 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|      119 | 2655 | `	break;` |
|        - | 2656 | `/*` |
|        - | 2657 | ` * CVT_STR: * * *` |
|        - | 2658 | ` *` |
|        - | 2659 | ` * Force the top of the stack to be a string.` |
|        - | 2660 | ` */` |
|     3238 | 2661 | `case PH7_OP_CVT_STR:` |
|        - | 2662 | `#ifdef UNTRUST` |
|        - | 2663 | `	if( pTos < pStack ){` |
|        - | 2664 | `		goto Abort;` |
|        - | 2665 | `	}` |
|        - | 2666 | `#endif` |
|        - | 2667 | `	/* (string) cast + string interpolation "$arr": php's user-visible` |
|        - | 2668 | `	 * array->string warning site, and the not-stringable-object throw (§2). */` |
|        - | 2669 | `	{` |
|     6478 | 2670 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     6480 | 2671 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2672 | `	}` |
|     6390 | 2673 | `	break;` |
|        - | 2674 | `/*` |
|        - | 2675 | ` * CVT_BOOL: * * *` |
|        - | 2676 | ` *` |
|        - | 2677 | ` * Force the top of the stack to be a boolean.` |
|        - | 2678 | ` */` |
|       66 | 2679 | `case PH7_OP_CVT_BOOL:` |
|        - | 2680 | `#ifdef UNTRUST` |
|        - | 2681 | `	if( pTos < pStack ){` |
|        - | 2682 | `		goto Abort;` |
|        - | 2683 | `	}` |
|        - | 2684 | `#endif` |
|      137 | 2685 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      129 | 2686 | `		PH7_MemObjToBool(pTos);` |
|       62 | 2687 | `	}` |
|      137 | 2688 | `	break;` |
|        - | 2689 | `/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it` |
|        - | 2690 | ` * always raises "The (unset) cast is no longer supported" (php 8 removed` |
|        - | 2691 | ` * the cast), so a program containing it never compiles. The switch has no` |
|        - | 2692 | ` * default arm, so abort loudly rather than fall through as a silent no-op` |
|        - | 2693 | ` * if a future emitter ever produces one without the compile error. */` |
|      ! 0 | 2694 | `case PH7_OP_CVT_NULL:` |
|      ! 0 | 2695 | `	goto Abort;` |
|        - | 2696 | `/*` |
|        - | 2697 | ` * CVT_NUMC: * * *` |
|        - | 2698 | ` *` |
|        - | 2699 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - | 2700 | ` */` |
|      ! 0 | 2701 | `case PH7_OP_CVT_NUMC:` |
|        - | 2702 | `#ifdef UNTRUST` |
|        - | 2703 | `	if( pTos < pStack ){` |
|        - | 2704 | `		goto Abort;` |
|        - | 2705 | `	}` |
|        - | 2706 | `#endif` |
|        - | 2707 | `	/* Force a numeric cast */` |
|      ! 0 | 2708 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 | 2709 | `	break;` |
|        - | 2710 | `/*` |
|        - | 2711 | ` * CVT_ARRAY: * * *` |
|        - | 2712 | ` *` |
|        - | 2713 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - | 2714 | ` */` |
|      371 | 2715 | `case PH7_OP_CVT_ARRAY:` |
|        - | 2716 | `#ifdef UNTRUST` |
|        - | 2717 | `	if( pTos < pStack ){` |
|        - | 2718 | `		goto Abort;` |
|        - | 2719 | `	}` |
|        - | 2720 | `#endif` |
|        - | 2721 | `	/* Force a hashmap cast */` |
|      746 | 2722 | `	rc = PH7_MemObjToHashmap(pTos);` |
|      746 | 2723 | `	if( rc != SXRET_OK ){` |
|        - | 2724 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 | 2725 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - | 2726 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 | 2727 | `	}` |
|      746 | 2728 | `	break;` |
|        - | 2729 | `/*` |
|        - | 2730 | ` * CVT_OBJ: * * *` |
|        - | 2731 | ` *` |
|        - | 2732 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - | 2733 | ` */` |
|       32 | 2734 | `case PH7_OP_CVT_OBJ:` |
|        - | 2735 | `#ifdef UNTRUST` |
|        - | 2736 | `	if( pTos < pStack ){` |
|        - | 2737 | `		goto Abort;` |
|        - | 2738 | `	}` |
|        - | 2739 | `#endif` |
|       68 | 2740 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 2741 | `		/* Force a 'stdClass()' cast */` |
|       68 | 2742 | `		PH7_MemObjToObject(pTos);` |
|       32 | 2743 | `	}` |
|       68 | 2744 | `	break;` |
|        - | 2745 | `/*` |
|        - | 2746 | ` * ERR_CTRL * * *` |
|        - | 2747 | ` *` |
|        - | 2748 | ` * Error control operator.` |
|        - | 2749 | ` */` |
|     4387 | 2750 | `case PH7_OP_UNSET_VAR: {` |
|        - | 2751 | `	VmOpRc rcOp;` |
|     8777 | 2752 | `	sState.pTos = pTos;` |
|     8777 | 2753 | `	sState.pc = pc;` |
|     8777 | 2754 | `	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);` |
|     8777 | 2755 | `	pTos = sState.pTos;` |
|     8777 | 2756 | `	pc = sState.pc;` |
|     8777 | 2757 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 2758 | `		goto Abort;` |
|     8775 | 2759 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 2760 | `		goto Exception;` |
|        - | 2761 | `	}` |
|     8773 | 2762 | `	break;` |
|        - | 2763 | `					  }` |
|    55704 | 2764 | `case PH7_OP_ERR_CTRL:` |
|        - | 2765 | `	/*` |
|        - | 2766 | `	 * Error-control operator '@'. Emitted as a PAIR around the suppressed` |
|        - | 2767 | `	 * expression: iP1=1 opens the window (before the operand is evaluated),` |
|        - | 2768 | `	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),` |
|        - | 2769 | `	 * which went unnoticed only because the engine raised so few diagnostics;` |
|        - | 2770 | `	 * every warning/notice/deprecation the parity work added leaked straight` |
|        - | 2771 | ``	 * through `@`. The window nests, and php 8 does NOT let '@' swallow`` |
|        - | 2772 | `	 * fatals or exceptions — those unwind past the closing instruction, so` |
|        - | 2773 | `	 * the depth is reset on the exception path rather than decremented here.` |
|        - | 2774 | `	 */` |
|   111321 | 2775 | `	if( pInstr->iP1 ){` |
|    55759 | 2776 | `		pVm->nErrSuppress++;` |
|    83421 | 2777 | `	}else if( pVm->nErrSuppress > 0 ){` |
|    55567 | 2778 | `		pVm->nErrSuppress--;` |
|    27758 | 2779 | `	}` |
|   111321 | 2780 | `	break;` |
|        - | 2781 | `/*` |
|        - | 2782 | ` * IS_A * * *` |
|        - | 2783 | ` *` |
|        - | 2784 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - | 2785 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - | 2786 | ` * holding a class name or an object).` |
|        - | 2787 | ` * Push TRUE on success. FALSE otherwise.` |
|        - | 2788 | ` */` |
|      584 | 2789 | `case PH7_OP_IS_A:{` |
|     1173 | 2790 | `	ph7_value *pNos = &pTos[-1];` |
|     1173 | 2791 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - | 2792 | `#ifdef UNTRUST` |
|        - | 2793 | `	if( pNos < pStack ){` |
|        - | 2794 | `		goto Abort;` |
|        - | 2795 | `	}` |
|        - | 2796 | `#endif` |
|        - | 2797 | `	/* php screens the CLASS operand before it looks at the subject: zend_fetch_class` |
|        - | 2798 | `	 * takes an object or a string and refuses everything else with` |
|        - | 2799 | ``	 * `Class name must be a valid object or a string`, whatever is on the left --`` |
|        - | 2800 | ``	 * `5 instanceof $arr` and `$obj instanceof $arr` are the same Error. PHL answered`` |
|        - | 2801 | `	 * FALSE for every one of them, so a typo'd or wrongly-typed class expression read` |
|        - | 2802 | `` 	 * as a clean "no" and the program carried on. The same refusal already guards `::` `` |
|        - | 2803 | ``	 * (VmExecOpMember) and now `new` (VmExecOpNew); this is the third door. */`` |
|     1173 | 2804 | `	if( (pTos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|       17 | 2805 | `		VmPopOperand(&pTos,1);` |
|       17 | 2806 | `		PH7_MemObjRelease(pTos);` |
|       17 | 2807 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       17 | 2808 | `		pTos->nIdx = SXU32_HIGH;` |
|       17 | 2809 | `		rc = VmThrowFromVm(&(*pVm),"Error","Class name must be a valid object or a string",` |
|        - | 2810 | `			sizeof("Class name must be a valid object or a string")-1);` |
|       17 | 2811 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2812 | `			goto Abort;` |
|        - | 2813 | `		}` |
|       17 | 2814 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2815 | `	}` |
|     1157 | 2816 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      953 | 2817 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      953 | 2818 | `		ph7_class *pClass = 0;` |
|        - | 2819 | `		/* Extract the target class */` |
|      953 | 2820 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - | 2821 | `			/* Instance already loaded */` |
|        3 | 2822 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      952 | 2823 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - | 2824 | `			/* self/static/parent, through the shared rail (PH7_VmResolveScopeName), and` |
|        - | 2825 | `			 * an ordinary name through the same non-autoloading extract as before.` |
|        - | 2826 | `			 *` |
|        - | 2827 | ``			 * This handler open-coded the three keywords and answered `self` with the`` |
|        - | 2828 | `			 * DECLARING class, which for a trait method is the TRAIT — and no object is` |
|        - | 2829 | ``			 * ever an instance of a trait, so `$x instanceof self` inside a trait method`` |
|        - | 2830 | `			 * was FALSE for every $x, including one of the very class that used it.` |
|        - | 2831 | ``			 * php composes a trait into the class, so `self` there is the USING class`` |
|        - | 2832 | ``			 * (PH7_VmPeekSelfClass states the rule once, and `self::`/`__CLASS__`/`` |
|        - | 2833 | ``			 * `new self` all already asked it). The same silent false answered inside a`` |
|        - | 2834 | `			 * CLOSURE declared in a trait method, whose stamped scope is the trait too.` |
|        - | 2835 | `			 *` |
|        - | 2836 | `			 * phpstan is where it showed: JustNullableTypeTrait::isSuperTypeOf() begins` |
|        - | 2837 | ``			 * `if ($type instanceof self)`, and with that never taken the trait fell`` |
|        - | 2838 | ``			 * through to `$type->isSubTypeOf($this)` — a mutual recursion with`` |
|        - | 2839 | `			 * IntegerRangeType::isSubTypeOf() that php terminates on the first line. */` |
|     1424 | 2840 | `			pClass = PH7_VmResolveScopeName(&(*pVm),` |
|      946 | 2841 | `				(const char *)SyBlobData(&pTos->sBlob),(sxu32)SyBlobLength(&pTos->sBlob));` |
|      473 | 2842 | `		}` |
|      953 | 2843 | `		if( pClass ){` |
|        - | 2844 | `			/* Perform the query */` |
|      951 | 2845 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|      473 | 2846 | `		}` |
|      474 | 2847 | `	}` |
|        - | 2848 | `	/* Push result */` |
|     1157 | 2849 | `	VmPopOperand(&pTos,1);` |
|     1157 | 2850 | `	PH7_MemObjRelease(pTos);` |
|     1157 | 2851 | `	pTos->x.iVal = iRes;` |
|     1157 | 2852 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     1157 | 2853 | `	break;` |
|        - | 2854 | `				 }` |
|        - | 2855 |  |
|        - | 2856 | `/*` |
|        - | 2857 | ` * LOADC P1 P2 *` |
|        - | 2858 | ` *` |
|        - | 2859 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - | 2860 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - | 2861 | ` */` |
|  7413470 | 2862 | `case PH7_OP_LOADC: {` |
|        - | 2863 | `	ph7_value *pObj;` |
|        - | 2864 | `	/* Reserve a room */` |
| 14826502 | 2865 | `	pTos++;` |
| 14826502 | 2866 | `	if( pInstr->iP1 & PH7_LOADC_NOKEY ){` |
|        - | 2867 | `		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */` |
|   771563 | 2868 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|   771563 | 2869 | `		SyBlobReset(&pTos->sBlob);` |
|   771563 | 2870 | `		pTos->iFlags \|= MEMOBJ_AUX_NOKEY;` |
|   771563 | 2871 | `		pTos->nIdx = SXU32_HIGH;` |
|   771563 | 2872 | `		break;` |
|        - | 2873 | `	}` |
| 14054944 | 2874 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
| 14054944 | 2875 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - | 2876 | `			SyHashEntry *pEntry;` |
|        - | 2877 | `			/* The name php looks up FIRST for an unqualified constant is decided at` |
|        - | 2878 | ``			 * COMPILE time and travels in p3: a `use const` import's FQN, or`` |
|        - | 2879 | ``			 * `current-namespace\NAME`. Absent (global scope, or an already-qualified`` |
|        - | 2880 | `			 * literal), the bare literal is the only name there is. Resolving it here` |
|        - | 2881 | `			 * rather than against the RUNTIME namespace is what makes a function keep` |
|        - | 2882 | `			 * its own namespace when it is called from another one, and what lets a` |
|        - | 2883 | `			 * namespaced constant shadow a global one of the same short name. */` |
|   149911 | 2884 | `			const char *zCand = (const char *)pInstr->p3;` |
|   149911 | 2885 | `			const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|   149911 | 2886 | `			sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - | 2887 | `			/* Both names are fixed by the instruction, so which one wins and what it` |
|        - | 2888 | `			 * resolves to can only change when hConstant does. A site that has already` |
|        - | 2889 | `			 * answered under the current generation answers again without hashing` |
|        - | 2890 | `			 * either -- see PH7_VmConstSiteAnswer for what that was costing. */` |
|   149911 | 2891 | `			pEntry = PH7_VmConstSiteAnswer(&(*pVm),pInstr);` |
|   149911 | 2892 | `			if( pEntry == 0 ){` |
|     9768 | 2893 | `				if( zCand ){` |
|       61 | 2894 | `					pEntry = SyHashGet(&pVm->hConstant,zCand,SyStrlen(zCand));` |
|       28 | 2895 | `				}` |
|        - | 2896 | `				/* The GLOBAL step — skipped when the candidate came from an import,` |
|        - | 2897 | `				 * which php resolves without any fallback. */` |
|     9768 | 2898 | `				if( pEntry == 0 && (pInstr->iP1 & PH7_LOADC_NOGLOBAL) == 0 ){` |
|     9718 | 2899 | `					pEntry = SyHashGet(&pVm->hConstant,(const void *)zLit,nLit);` |
|     4774 | 2900 | `				}` |
|     9768 | 2901 | `				PH7_VmConstSiteRecord(&(*pVm),pInstr,pEntry);` |
|     4799 | 2902 | `			}` |
|   149911 | 2903 | `			if( pEntry ){` |
|   149733 | 2904 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - | 2905 | `				/* Set a NULL default value */` |
|   149733 | 2906 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|   149733 | 2907 | `				SyBlobReset(&pTos->sBlob);` |
|        - | 2908 | `				/* Invoke the callback and deal with the expanded value */` |
|   149733 | 2909 | `				VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|        - | 2910 | `				/* Mark as constant */` |
|   149733 | 2911 | `				pTos->nIdx = SXU32_HIGH;` |
|   149733 | 2912 | `				break;` |
|        - | 2913 | `			}` |
|        - | 2914 | `			{` |
|        - | 2915 | `				/*` |
|        - | 2916 | `				 * php 8 has no bare-word fallback: an unresolved constant is a catchable` |
|        - | 2917 | `				 * Error, not its own name as a string. PH7 answered "X" for an unknown` |
|        - | 2918 | `				 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —` |
|        - | 2919 | `				 * silently became a string and flowed on. php names the name it looked` |
|        - | 2920 | `				 * for FIRST, so the message reports the candidate when there was one` |
|        - | 2921 | ``				 * ("Undefined constant \"B\NOPE\"" inside `namespace B;`).`` |
|        - | 2922 | `				 *` |
|        - | 2923 | `				 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call` |
|        - | 2924 | ``				 * boundary, so neither a bare `break` nor `goto Exception` is correct`` |
|        - | 2925 | `				 * here (see the macro).` |
|        - | 2926 | `				 */` |
|        - | 2927 | `				SyBlob sMsg;` |
|      183 | 2928 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      183 | 2929 | `				if( zCand ){` |
|        8 | 2930 | `					SyBlobFormat(&sMsg,"Undefined constant \"%s\"",zCand);` |
|        5 | 2931 | `				}else{` |
|      177 | 2932 | `					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);` |
|        - | 2933 | `				}` |
|      183 | 2934 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      183 | 2935 | `				SyBlobReset(&pTos->sBlob);` |
|      183 | 2936 | `				pTos->nIdx = SXU32_HIGH;` |
|      272 | 2937 | `				rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|       89 | 2938 | `					SyBlobLength(&sMsg));` |
|      183 | 2939 | `				SyBlobRelease(&sMsg);` |
|      183 | 2940 | `				if( rc == SXERR_ABORT ){` |
|       51 | 2941 | `					goto Abort;` |
|        - | 2942 | `				}` |
|      147 | 2943 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2944 | `			}` |
|        - | 2945 | `		}` |
| 13905038 | 2946 | `		PH7_MemObjLoad(pObj,pTos);` |
|  6952729 | 2947 | `	}else{` |
|        - | 2948 | `		/* Set a NULL value */` |
|      ! 0 | 2949 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 2950 | `	}` |
|        - | 2951 | `	/* Mark as constant */` |
| 13905038 | 2952 | `	pTos->nIdx = SXU32_HIGH;` |
| 13905038 | 2953 | `	break;` |
|        - | 2954 | `				  }` |
|        - | 2955 | `/*` |
|        - | 2956 | ` * LOAD: P1 * P3` |
|        - | 2957 | ` *` |
|        - | 2958 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - | 2959 | ` * from the P3 operand.` |
|        - | 2960 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - | 2961 | ` * the variable if non existent and push the NULL constant instead.` |
|        - | 2962 | ` */` |
|  6878301 | 2963 | `case PH7_OP_LOAD:{` |
|        - | 2964 | `	ph7_value *pObj;` |
|        - | 2965 | `	SyString sName;` |
| 13770482 | 2966 | `	if( pInstr->p3 == 0 ){` |
|        - | 2967 | `		/* Take the variable name from the top of the stack */` |
|        - | 2968 | `#ifdef UNTRUST` |
|        - | 2969 | `		if( pTos < pStack ){` |
|        - | 2970 | `			goto Abort;` |
|        - | 2971 | `		}` |
|        - | 2972 | `#endif` |
|        - | 2973 | `		/* Force a string cast — variable-variable name $$arr (user-visible, §2) */` |
|        - | 2974 | `		{` |
|       42 | 2975 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|  6878342 | 2976 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2977 | `		}` |
|       40 | 2978 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       22 | 2979 | `	}else{` |
| 13770444 | 2980 | `		if( pInstr->nAux == 0 ){` |
|        - | 2981 | `			/* Measured once: the name is a compile-time buffer that never changes.` |
|        - | 2982 | `			 * VmNumberLocals fills this in for a body it walks; this is the door for` |
|        - | 2983 | `			 * one it does not (an include, an eval, a mini-program). */` |
|    55147 | 2984 | `			pInstr->nAux = (sxu32)SyStrlen((const char *)pInstr->p3);` |
|    27565 | 2985 | `		}` |
| 13770444 | 2986 | `		SyStringInitFromBuf(&sName,pInstr->p3,pInstr->nAux);` |
|        - | 2987 | `		/* Reserve a room for the target object */` |
| 13770444 | 2988 | `		pTos++;` |
|        - | 2989 | `	}` |
|        - | 2990 | `	{` |
|        - | 2991 | `	/* A name the compiler put in the instruction has a NUMBER in this body, so the` |
|        - | 2992 | `	 * frame answers it out of an array (PH7_VmExtractVarSlot); a variable-variable's` |
|        - | 2993 | `	 * name is a string on the stack and gets the plain door. */` |
| 20742996 | 2994 | `	int bThis = ( sName.nByte == sizeof("this")-1` |
|  6985279 | 2995 | `	           && SyMemcmp(sName.zString,"this",sizeof("this")-1) == 0` |
| 13863579 | 2996 | `	           && pInstr->iP2 != 1 /* isset()/empty() ask a QUESTION -- see below */ );` |
| 13770480 | 2997 | `	if( bThis \|\| pInstr->iP2 == 2 ){` |
|        - | 2998 | ``		/* One PEEK (no create) answers both questions below -- `$this` in a frame`` |
|        - | 2999 | `		 * with no receiver, and php's read-before-write warning -- where two` |
|        - | 3000 | `		 * separate lookups used to ask the same table the same thing twice for` |
|        - | 3001 | ``		 * every `$this` in every method body. */`` |
|  2035687 | 3002 | `		ph7_value *pPeek = pInstr->p3` |
|  1357439 | 3003 | `			? PH7_VmExtractVarSlot(&(*pVm),&sName,FALSE,pInstr->nSite,aInstr)` |
|   678243 | 3004 | `			: VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE);` |
|  1357444 | 3005 | `		if( pPeek == 0 ){` |
|       22 | 3006 | `			if( bThis ){` |
|        - | 3007 | ``				/* `$this` is not a variable: a frame with no receiver answers a READ`` |
|        - | 3008 | `				 * of it with php's catchable Error, not the undefined-variable` |
|        - | 3009 | ``				 * warning and a NULL. The null was the hazard -- `$this->m()` in a`` |
|        - | 3010 | `				 * plain function became "Call to a member function m() on null",` |
|        - | 3011 | ``				 * `var_dump($this)` printed NULL, and `f($this)` passed one on --`` |
|        - | 3012 | `				 * each a wrong ANSWER a few frames from the mistake. Asked BEFORE the` |
|        - | 3013 | ``				 * extract so the vivifying contexts (`$this ?? 'd'`, which php throws`` |
|        - | 3014 | `				 * for even though it swallows an ordinary undefined variable) reach it` |
|        - | 3015 | ``				 * too; `isset($this)`/`empty($this)` are php's one exemption, and they`` |
|        - | 3016 | `				 * answer false/true in silence. */` |
|       13 | 3017 | `				rc = VmThrowFromVm(&(*pVm),"Error","Using $this when not in object context",` |
|        - | 3018 | `					sizeof("Using $this when not in object context")-1);` |
|       13 | 3019 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3020 | `					goto Abort;` |
|        - | 3021 | `				}` |
|       29 | 3022 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      ! 0 | 3023 | `			}else{` |
|        - | 3024 | ``				/* Read-modify-write target (`$x++`, `$x .= 'a'`): php reads the`` |
|        - | 3025 | `				 * variable before writing, so it warns when it does not exist and` |
|        - | 3026 | `				 * THEN seeds it. The load below still creates the slot the operator` |
|        - | 3027 | ``				 * needs. A plain `=` never gets here -- it writes without reading,`` |
|        - | 3028 | `				 * and stays silent, as php does. */` |
|       10 | 3029 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        - | 3030 | `			}` |
|        4 | 3031 | `		}` |
|   679190 | 3032 | `	}` |
|        - | 3033 | `	/* Extract the requested memory object */` |
| 20648762 | 3034 | `	pObj = pInstr->p3` |
| 13770427 | 3035 | `		? PH7_VmExtractVarSlot(&(*pVm),&sName,pInstr->iP1 != 1,pInstr->nSite,aInstr)` |
|  6878312 | 3036 | `		: VmExtractMemObj(&(*pVm),&sName,TRUE,pInstr->iP1 != 1);` |
|        - | 3037 | `	}` |
| 13770468 | 3038 | `	if( pObj == 0 ){` |
|      285 | 3039 | `		if( pInstr->iP1 ){` |
|        - | 3040 | `			/* Reading a variable that does not exist. php raises E_WARNING` |
|        - | 3041 | `			 * "Undefined variable $x" and evaluates it as NULL; PHL used to` |
|        - | 3042 | `			 * yield NULL silently, which hid typo'd names. iP2 marks the reads` |
|        - | 3043 | `			 * that must stay quiet (isset/empty — see PH7_CompileVariable);` |
|        - | 3044 | `			 * vivifying contexts never get here because they pass iP1 = 0. */` |
|      285 | 3045 | `			if( pInstr->iP2 == 0 ){` |
|       51 | 3046 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|       23 | 3047 | `			}` |
|        - | 3048 | `			/* Variable not found,load NULL */` |
|      285 | 3049 | `			if( !pInstr->p3 ){` |
|       10 | 3050 | `				PH7_MemObjRelease(pTos);` |
|        6 | 3051 | `			}else{` |
|      277 | 3052 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 3053 | `			}` |
|      285 | 3054 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|      285 | 3055 | `			if( pInstr->iP2 == 3 ){` |
|        - | 3056 | `				/* D1 deferred call argument: the variable does not exist, but we do not` |
|        - | 3057 | `				 * know yet whether the callee wants it by-ref (materialize + bind) or` |
|        - | 3058 | `				 * by-value (warn + pass NULL). Tag the slot and stash the variable name` |
|        - | 3059 | `				 * (a VM-lifetime bytecode string, nothing to free) so OP_CALL's` |
|        - | 3060 | `				 * PH7_VmResolveDeferredArgs can decide once the callee is resolved. */` |
|      153 | 3061 | `				pTos->iFlags \|= MEMOBJ_AUX_DEFERRED;` |
|      153 | 3062 | `				pTos->x.pOther = pInstr->p3;` |
|       75 | 3063 | `			}` |
|      285 | 3064 | `			break;` |
|      ! 0 | 3065 | `		}else{` |
|        - | 3066 | `			/* Fatal error */` |
|      ! 0 | 3067 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 3068 | `			goto Abort;` |
|        - | 3069 | `		}` |
|        - | 3070 | `	}` |
|        - | 3071 | `	/* Load variable contents */` |
| 13770188 | 3072 | `	PH7_MemObjLoad(pObj,pTos);` |
| 13770188 | 3073 | `	pTos->nIdx = pObj->nIdx;` |
| 13770188 | 3074 | `	break;` |
|        - | 3075 | `				   }` |
|        - | 3076 | `/*` |
|        - | 3077 | ` * LOAD_MAP P1 * *` |
|        - | 3078 | ` *` |
|        - | 3079 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - | 3080 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - | 3081 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - | 3082 | ` */` |
|    66614 | 3083 | `case PH7_OP_LOAD_MAP: {` |
|        - | 3084 | `	VmOpRc rcOp;` |
|   133095 | 3085 | `	sState.pTos = pTos;` |
|   133095 | 3086 | `	sState.pc = pc;` |
|   133095 | 3087 | `	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);` |
|   133095 | 3088 | `	pTos = sState.pTos;` |
|   133095 | 3089 | `	pc = sState.pc;` |
|   133095 | 3090 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3091 | `		goto Abort;` |
|   133095 | 3092 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       45 | 3093 | `		goto Exception;` |
|        - | 3094 | `	}` |
|   133053 | 3095 | `	break;` |
|        - | 3096 | `					  }` |
|        - | 3097 | `/*` |
|        - | 3098 | ` * LOAD_LIST: P1 * *` |
|        - | 3099 | ` *` |
|        - | 3100 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - | 3101 | ` * This is the VM implementation of the list() PHP construct.` |
|        - | 3102 | ` * Caveats:` |
|        - | 3103 | ` *  This implementation support only a single nesting level.` |
|        - | 3104 | ` */` |
|   196453 | 3105 | `case PH7_OP_LOAD_LIST: {` |
|        - | 3106 | `	VmOpRc rcOp;` |
|   392904 | 3107 | `	sState.pTos = pTos;` |
|   392904 | 3108 | `	sState.pc = pc;` |
|   392904 | 3109 | `	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);` |
|   392904 | 3110 | `	pTos = sState.pTos;` |
|   392904 | 3111 | `	pc = sState.pc;` |
|   392904 | 3112 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3113 | `		goto Abort;` |
|   392904 | 3114 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 3115 | `		goto Exception;` |
|        - | 3116 | `	}` |
|   392898 | 3117 | `	break;` |
|        - | 3118 | `					  }` |
|        - | 3119 | `/*` |
|        - | 3120 | ` * LOAD_IDX: P1 P2 *` |
|        - | 3121 | ` *` |
|        - | 3122 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - | 3123 | ` * from the stack.` |
|        - | 3124 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - | 3125 | ` * instead.` |
|        - | 3126 | ` */` |
|   602718 | 3127 | `case PH7_OP_LOAD_IDX: {` |
|        - | 3128 | `	VmOpRc rcOp;` |
|  1208717 | 3129 | `	sState.pTos = pTos;` |
|  1208717 | 3130 | `	sState.pc = pc;` |
|  1208717 | 3131 | `	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);` |
|  1208717 | 3132 | `	pTos = sState.pTos;` |
|  1208717 | 3133 | `	pc = sState.pc;` |
|  1208717 | 3134 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3135 | `		goto Abort;` |
|  1208717 | 3136 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       99 | 3137 | `		goto Exception;` |
|        - | 3138 | `	}` |
|  1208621 | 3139 | `	break;` |
|        - | 3140 | `					  }` |
|        - | 3141 | `/*` |
|        - | 3142 | ` * LOAD_CLOSURE * * P3` |
|        - | 3143 | ` *` |
|        - | 3144 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - | 3145 | ` * name in the stack.` |
|        - | 3146 | ` */` |
|     9399 | 3147 | `case PH7_OP_LOAD_CLOSURE: {` |
|        - | 3148 | `	VmOpRc rcOp;` |
|    18560 | 3149 | `	sState.pTos = pTos;` |
|    18560 | 3150 | `	sState.pc = pc;` |
|    18560 | 3151 | `	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);` |
|    18560 | 3152 | `	pTos = sState.pTos;` |
|    18560 | 3153 | `	pc = sState.pc;` |
|    18560 | 3154 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3155 | `		goto Abort;` |
|    18560 | 3156 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3157 | `		goto Exception;` |
|        - | 3158 | `	}` |
|    18560 | 3159 | `	break;` |
|        - | 3160 | `					  }` |
|        - | 3161 | `/*` |
|        - | 3162 | ` * LOAD_FCC P1 * *` |
|        - | 3163 | ` *` |
|        - | 3164 | ` * First-class callable: wrap the callee in a Closure object instead of calling it.` |
|        - | 3165 | ` *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS` |
|        - | 3166 | ` *           (from the callee's OP_LOADC). Replace it in place with a Closure whose` |
|        - | 3167 | ` *           $__fn is that name; the existing string-callable dispatch resolves it.` |
|        - | 3168 | ` *           (OOM degrades to leaving the name string on the stack — still callable.)` |
|        - | 3169 | ` *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name` |
|        - | 3170 | ` *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds` |
|        - | 3171 | ` *           $this (scope = its class); a class-name-string target is a static callable` |
|        - | 3172 | ` *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —` |
|        - | 3173 | ` *           the popped target leaves no name string to keep.)` |
|        - | 3174 | ` */` |
|      165 | 3175 | `case PH7_OP_LOAD_FCC:{` |
|      335 | 3176 | `	if( pInstr->iP1 == 1 ){` |
|        - | 3177 | ``		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)`` |
|        - | 3178 | `		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —` |
|        - | 3179 | `		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,` |
|        - | 3180 | ``		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a`` |
|        - | 3181 | `		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is` |
|        - | 3182 | `		 * (graceful degradation), and so is the original on OOM — still whatever it was. */` |
|        - | 3183 | `		ph7_class_instance *pCloObj;` |
|        - | 3184 | `		sxi32 nFccBrc;` |
|        - | 3185 | `		const void *pFccRes;` |
|      152 | 3186 | `		if( VmValueIsClosure(pVm, pTos) ){` |
|       16 | 3187 | `			break;` |
|        - | 3188 | `		}` |
|        - | 3189 | `		/* php's global fallback for an UNQUALIFIED function name written inside a` |
|        - | 3190 | `		 * namespace: the current namespace first, the global one after. The compiler` |
|        - | 3191 | `		 * qualified this name and marks the instruction (iP2==1) when it did, so the` |
|        - | 3192 | `		 * fallback happens here — the OP_CALL path does the same thing from its arg map,` |
|        - | 3193 | ``		 * which an FCC has none of. `strlen(...)` in a namespaced file was`` |
|        - | 3194 | ``		 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|      134 | 3195 | `		if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING)` |
|       14 | 3196 | `			&& !PH7_VmIsCallable(pVm,pTos,TRUE) ){` |
|        7 | 3197 | `			const char *zFccName = (const char *)SyBlobData(&pTos->sBlob);` |
|        7 | 3198 | `			sxu32 nFccName = SyBlobLength(&pTos->sBlob);` |
|        7 | 3199 | `			const char *zFccShort = zFccName;` |
|        - | 3200 | `			sxu32 iFccPos;` |
|      165 | 3201 | `			for( iFccPos = 0 ; iFccPos < nFccName ; ++iFccPos ){` |
|      159 | 3202 | `				if( zFccName[iFccPos] == '\\' ){` |
|        7 | 3203 | `					zFccShort = &zFccName[iFccPos + 1];` |
|        3 | 3204 | `				}` |
|       80 | 3205 | `			}` |
|        7 | 3206 | `			if( zFccShort != zFccName ){` |
|        - | 3207 | `				ph7_value sFccShort;` |
|        7 | 3208 | `				PH7_MemObjInit(pVm,&sFccShort);` |
|       10 | 3209 | `				PH7_MemObjStringAppend(&sFccShort,zFccShort,` |
|        6 | 3210 | `					(sxu32)(nFccName - (sxu32)(zFccShort - zFccName)));` |
|        7 | 3211 | `				if( PH7_VmIsCallable(pVm,&sFccShort,TRUE) ){` |
|        5 | 3212 | `					PH7_MemObjStore(&sFccShort,pTos);` |
|        2 | 3213 | `				}` |
|        7 | 3214 | `				PH7_MemObjRelease(&sFccShort);` |
|        3 | 3215 | `			}` |
|        3 | 3216 | `		}` |
|        - | 3217 | `		/* The array shape's class lookup can run an autoloader that throws; php propagates` |
|        - | 3218 | `		 * THAT exception and never reports the callable bad, exactly as at the OP_CALL sites. */` |
|      138 | 3219 | `		nFccBrc = pVm->nBoundaryRc;` |
|      138 | 3220 | `		pFccRes = (const void *)pVm->pResumeFrame;` |
|      138 | 3221 | `		pCloObj = VmFccWrapValue(pVm, pTos);` |
|      138 | 3222 | `		if( pCloObj ){` |
|      108 | 3223 | `			PH7_MemObjRelease(pTos);` |
|        - | 3224 | `			/* The fresh instance's own reference is this stack slot's (see OP_LOAD_CLOSURE) */` |
|      108 | 3225 | `			pTos->x.pOther = pCloObj;` |
|      108 | 3226 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       56 | 3227 | `		}else{` |
|        - | 3228 | `			/* php refuses a non-callable HERE, with the direct dispatch's own wording — the` |
|        - | 3229 | ``			 * `(...)` does not make a bad callable acceptable, it just defers the call. */`` |
|        - | 3230 | `			char zFccMsg[192];` |
|       31 | 3231 | `			const char *zFccBad = VmFccValueError(&(*pVm),pTos,zFccMsg,sizeof(zFccMsg));` |
|       31 | 3232 | `			int bFccRaised = PH7_VmClassLookupRaised(&(*pVm),nFccBrc,pFccRes);` |
|       31 | 3233 | `			if( zFccBad \|\| bFccRaised ){` |
|        - | 3234 | `				sxi32 rcFcc;` |
|       31 | 3235 | `				PH7_MemObjRelease(pTos);` |
|       31 | 3236 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       31 | 3237 | `				pTos->nIdx = SXU32_HIGH;` |
|       31 | 3238 | `				if( bFccRaised ){` |
|      ! 0 | 3239 | `					rcFcc = pVm->nBoundaryRc;` |
|      ! 0 | 3240 | `					pVm->nBoundaryRc = 0;` |
|      ! 0 | 3241 | `					if( rcFcc == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 3242 | `					rc = PH7_EXCEPTION;` |
|       15 | 3243 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3244 | `				}` |
|       31 | 3245 | `				rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccBad,(sxu32)SyStrlen(zFccBad));` |
|       31 | 3246 | `				if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       31 | 3247 | `				rc = rcFcc;` |
|       57 | 3248 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3249 | `			}` |
|        - | 3250 | `		}` |
|       56 | 3251 | `	}else{` |
|        - | 3252 | `		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]` |
|        - | 3253 | `		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its` |
|        - | 3254 | `		 * class); a class-name string target is a static callable (scope = that class). */` |
|      187 | 3255 | `		ph7_value *pTarget = &pTos[-1];` |
|        - | 3256 | `		SyString sName;` |
|        - | 3257 | `		ph7_class_instance *pCloObj;` |
|      187 | 3258 | `		ph7_class *pFccCls = 0;` |
|      187 | 3259 | `		ph7_class_instance *pFccRecv = 0;` |
|      187 | 3260 | `		const char *zFccErr = 0;` |
|        - | 3261 | `		char zFccMsg[192];` |
|      187 | 3262 | `		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));` |
|      187 | 3263 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      109 | 3264 | `			pFccRecv = (ph7_class_instance *)pTarget->x.pOther;` |
|      109 | 3265 | `			pFccCls = pFccRecv->pClass;` |
|      109 | 3266 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pFccCls) ){` |
|        - | 3267 | ``				/* `$inc->m(...)` resolves the method at CREATION, so php's`` |
|        - | 3268 | `				 * incomplete-object call Error is raised here, not at a later` |
|        - | 3269 | `				 * invocation. */` |
|        - | 3270 | `				SyBlob sIncErr;` |
|        - | 3271 | `				sxi32 rcInc;` |
|        3 | 3272 | `				SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|        3 | 3273 | `				PH7_VmIncompleteMsg(&(*pVm),pFccRecv,"call a method",&sIncErr);` |
|        3 | 3274 | `				VmPopOperand(&pTos,1);       /* the method name */` |
|        3 | 3275 | `				PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|        3 | 3276 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 3277 | `				pTos->nIdx = SXU32_HIGH;` |
|        4 | 3278 | `				rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|        1 | 3279 | `					SyBlobLength(&sIncErr));` |
|        3 | 3280 | `				SyBlobRelease(&sIncErr);` |
|        3 | 3281 | `				if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|        3 | 3282 | `				rc = rcInc;` |
|        3 | 3283 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        5 | 3284 | `			}` |
|      133 | 3285 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|        - | 3286 | ``			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class`` |
|        - | 3287 | `			 * now, so the closure binds the concrete scope (matching PHP). */` |
|       82 | 3288 | `			pFccCls = VmFccResolveScope(pVm, pTarget);` |
|       39 | 3289 | `		}` |
|      185 | 3290 | `		if( pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING) ){` |
|        - | 3291 | `			/* php resolves the member HERE, through the same lookup the call would use:` |
|        - | 3292 | `			 * every refusal a call would raise is raised at CREATION, and a non-static` |
|        - | 3293 | `			 * method named through a class binds the calling frame's own $this. */` |
|      185 | 3294 | `			ph7_class_instance *pRecvOut = 0;` |
|      275 | 3295 | `			zFccErr = VmFccMemberError(&(*pVm),pFccCls,` |
|      180 | 3296 | `				(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|       90 | 3297 | `				SyStringData(&sName),SyStringLength(&sName),` |
|      180 | 3298 | `				(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|       90 | 3299 | `				&pRecvOut,zFccMsg,sizeof(zFccMsg));` |
|      185 | 3300 | `			if( pRecvOut ){` |
|       13 | 3301 | ``				pFccRecv = pRecvOut; /* the receiver php binds into a `C::m(...)` callable */`` |
|        6 | 3302 | `			}` |
|       90 | 3303 | `		}` |
|      185 | 3304 | `		if( zFccErr ){` |
|        - | 3305 | `			sxi32 rcFcc;` |
|       24 | 3306 | `			VmPopOperand(&pTos,1);       /* the method name */` |
|       24 | 3307 | `			PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|       24 | 3308 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       24 | 3309 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 | 3310 | `			rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccErr,(sxu32)SyStrlen(zFccErr));` |
|       24 | 3311 | `			if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       24 | 3312 | `			rc = rcFcc;` |
|       26 | 3313 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3314 | `		}` |
|      163 | 3315 | `		if( pFccCls == 0 ){` |
|      ! 0 | 3316 | `			pCloObj = 0;` |
|      163 | 3317 | `		}else if( pFccRecv ){` |
|      109 | 3318 | `			pCloObj = VmCreateClosure(pVm, &sName, pFccRecv, &pFccRecv->pClass->sName);` |
|       57 | 3319 | `		}else{` |
|       58 | 3320 | `			pCloObj = VmCreateClosure(pVm, &sName, 0, &pFccCls->sName);` |
|        - | 3321 | `		}` |
|      163 | 3322 | `		if( pCloObj ){` |
|        - | 3323 | ``			/* `$o->m(...)` / `C::m(...)` names a METHOD, whatever the class turns out to`` |
|        - | 3324 | `			 * declare: the unwrap must not go looking for a FUNCTION of that name, and a` |
|        - | 3325 | `			 * name the class answers only through __call is still a method call. */` |
|      163 | 3326 | `			pCloObj->iFlags \|= VM_INSTANCE_FCC_METHOD;` |
|        - | 3327 | `			/* The screen above already ran, HERE, where php runs it — so record that this` |
|        - | 3328 | `			 * closure's callee is settled and the invocation must not re-decide it. A name` |
|        - | 3329 | `			 * that resolved to the catch-all instead keeps routing there. */` |
|      163 | 3330 | `			if( PH7_VmFccMethodIsDirect(&(*pVm),pFccCls,SyStringData(&sName),SyStringLength(&sName)) ){` |
|      149 | 3331 | `				pCloObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       72 | 3332 | `			}` |
|       79 | 3333 | `		}` |
|        - | 3334 | `		/* Pop the method name and the target, push the Closure. */` |
|      163 | 3335 | `		PH7_MemObjRelease(pTos);` |
|      163 | 3336 | `		pTos--;` |
|      163 | 3337 | `		PH7_MemObjRelease(pTos);` |
|      163 | 3338 | `		if( pCloObj ){` |
|        - | 3339 | `			/* The fresh instance's own reference is this stack slot's (see OP_LOAD_CLOSURE) */` |
|      163 | 3340 | `			pTos->x.pOther = pCloObj;` |
|      163 | 3341 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       84 | 3342 | `		}else{` |
|      ! 0 | 3343 | `			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */` |
|        - | 3344 | `		}` |
|        - | 3345 | `	}` |
|      267 | 3346 | `	break;` |
|        - | 3347 | `					 }` |
|        - | 3348 | `/*` |
|        - | 3349 | ` * STORE * P2 P3` |
|        - | 3350 | ` *` |
|        - | 3351 | ` * Perform a store (Assignment) operation.` |
|        - | 3352 | ` */` |
|   619083 | 3353 | `case PH7_OP_STORE: {` |
|        - | 3354 | `	ph7_value *pObj;` |
|        - | 3355 | `	SyString sName;` |
|        - | 3356 | `#ifdef UNTRUST` |
|        - | 3357 | `	if( pTos < pStack ){` |
|        - | 3358 | `		goto Abort;` |
|        - | 3359 | `	}` |
|        - | 3360 | `#endif` |
|  1240968 | 3361 | `	if( pInstr->iP2 ){` |
|        - | 3362 | `		sxu32 nIdx;` |
|        - | 3363 | `		sxi32 rcT;` |
|        - | 3364 | `		/* Member store operation */` |
|   104083 | 3365 | `		nIdx = pTos->nIdx;` |
|   104083 | 3366 | `		VmPopOperand(&pTos,1);` |
|   104083 | 3367 | `		if( pVm->pMagicSetThis ){` |
|        - | 3368 | `			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain` |
|        - | 3369 | `			 * store to a missing/inaccessible property on a class declaring __set.` |
|        - | 3370 | `			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on` |
|        - | 3371 | `			 * the stack as the assignment expression's result — php's semantics` |
|        - | 3372 | `			 * (no property is created; a throw rides the boundary rail). */` |
|      775 | 3373 | `			ph7_class_instance *pSetThis = pVm->pMagicSetThis;` |
|        - | 3374 | `			SyString sSetName;` |
|      775 | 3375 | `			pVm->pMagicSetThis = 0;` |
|      775 | 3376 | `			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));` |
|      775 | 3377 | `			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);` |
|      775 | 3378 | `			PH7_ClassInstanceUnref(pSetThis);` |
|      775 | 3379 | `			SyBlobReset(&pVm->sMagicSetName);` |
|      775 | 3380 | `			break;` |
|        - | 3381 | `		}` |
|   103311 | 3382 | `		if( pVm->pHookSetThis ){` |
|        - | 3383 | `			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a` |
|        - | 3384 | `			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with` |
|        - | 3385 | `			 * the rvalue (which stays on the stack as the assignment expression's` |
|        - | 3386 | ``			 * result); a `set => expr` hook's return value is stored into the`` |
|        - | 3387 | `			 * BACKING slot with the ordinary typed enforcement. A property with` |
|        - | 3388 | `			 * only a get hook is php's catchable "is read-only" Error. */` |
|       55 | 3389 | `			ph7_class_instance *pHThis = pVm->pHookSetThis;` |
|       55 | 3390 | `			ph7_class_attr *pHAttr = pVm->pHookSetAttr;` |
|       55 | 3391 | `			sxu32 nBackIdx = pVm->nHookSetIdx;` |
|        - | 3392 | `			sxi32 rcHs;` |
|       55 | 3393 | `			pVm->pHookSetThis = 0;` |
|       55 | 3394 | `			pVm->pHookSetAttr = 0;` |
|       55 | 3395 | `			pVm->nHookSetIdx = SXU32_HIGH;` |
|       55 | 3396 | `			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);` |
|       55 | 3397 | `			PH7_ClassInstanceUnref(pHThis);` |
|       55 | 3398 | `			if( rcHs == PH7_ABORT ){` |
|      ! 0 | 3399 | `				goto Abort;` |
|        - | 3400 | `			}` |
|       55 | 3401 | `			break;` |
|        - | 3402 | `		}` |
|   103259 | 3403 | `		if( nIdx == SXU32_HIGH ){` |
|        - | 3404 | `			/* No slot behind the property: the receiver is a TEMPORARY nothing else` |
|        - | 3405 | ``			 * holds (`mk()->p = 5` on a freshly built object, `(new A)->p` where the`` |
|        - | 3406 | `			 * compile-time screen lets it through). php performs the write on the` |
|        - | 3407 | `			 * doomed object and says nothing — the object is gone at the end of the` |
|        - | 3408 | `			 * statement, so the store is unobservable either way. PHL announced it` |
|        - | 3409 | ``			 * with `Cannot perform assignment on a constant class attribute,PH7 is`` |
|        - | 3410 | ``			 * loading NULL`, a diagnostic php has no equivalent of. Every case that`` |
|        - | 3411 | `			 * IS a refusal — a class constant, a hooked or handler-backed property —` |
|        - | 3412 | `			 * is decided before this point now. */` |
|        3 | 3413 | `			pTos->nIdx = SXU32_HIGH;` |
|        2 | 3414 | `		}else{` |
|        - | 3415 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - | 3416 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|   103257 | 3417 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);` |
|   103257 | 3418 | `			if( rcT == PH7_ABORT ){` |
|       13 | 3419 | `				goto Abort;` |
|        - | 3420 | `			}` |
|   103247 | 3421 | `			if( rcT == PH7_EXCEPTION ){` |
|        - | 3422 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - | 3423 | `				 * control to the nearest catch block if any (draining any` |
|        - | 3424 | `				 * abandoned outer-expression operands to the try's base),` |
|        - | 3425 | `				 * otherwise propagate out of the VM loop. */` |
|   100211 | 3426 | `				VmPopOperand(&pTos,1);` |
|        - | 3427 | `				{` |
|        - | 3428 | `					sxi32 iRp;` |
|   100211 | 3429 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|   400121 | 3430 | `						PH7_RESUME_DRAIN()` |
|   100115 | 3431 | `						pc = iRp;` |
|   100115 | 3432 | `						break;` |
|        - | 3433 | `					}` |
|        - | 3434 | `				}` |
|      109 | 3435 | `				goto Exception;` |
|        - | 3436 | `			}` |
|        - | 3437 | `			/* Point to the desired memory object */` |
|     3041 | 3438 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|     3041 | 3439 | `			if( pObj ){` |
|        - | 3440 | `				/* Perform the store operation */` |
|     3041 | 3441 | `				PH7_MemObjStore(pTos,pObj);` |
|     1518 | 3442 | `			}` |
|        - | 3443 | `		}` |
|     3043 | 3444 | `		break;` |
|  1136890 | 3445 | `	}else if( pInstr->p3 == 0 ){` |
|        - | 3446 | `		/* Take the variable name from the next on the stack (user-visible: a` |
|        - | 3447 | `		 * variable-variable NAME $$arr warns on an array, §2) */` |
|        - | 3448 | `		{` |
|       29 | 3449 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       29 | 3450 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3451 | `		}` |
|       26 | 3452 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       26 | 3453 | `		pTos--;` |
|        - | 3454 | `#ifdef UNTRUST` |
|        - | 3455 | `		if( pTos < pStack  ){` |
|        - | 3456 | `			goto Abort;` |
|        - | 3457 | `		}` |
|        - | 3458 | `#endif` |
|       14 | 3459 | `	}else{` |
|  1136864 | 3460 | `		if( pInstr->nAux == 0 ){` |
|        - | 3461 | `			/* Measured once, like OP_LOAD's: the name is a compile-time buffer.` |
|        - | 3462 | `			 * VmNumberLocals fills it in for a body it walks. */` |
|    19064 | 3463 | `			pInstr->nAux = (sxu32)SyStrlen((const char *)pInstr->p3);` |
|     9528 | 3464 | `		}` |
|  1136864 | 3465 | `		SyStringInitFromBuf(&sName,pInstr->p3,pInstr->nAux);` |
|        - | 3466 | `	}` |
|  1136883 | 3467 | `	if( sName.nByte == sizeof("GLOBALS")-1` |
|   572295 | 3468 | `	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){` |
|        6 | 3469 | `		if( pInstr->p3 ){` |
|        - | 3470 | `			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time` |
|        - | 3471 | `			 * fatal there; raised at the store site here with the same` |
|        - | 3472 | `			 * message and the same non-catchable outcome). Element writes` |
|        - | 3473 | `			 * are unaffected. */` |
|        3 | 3474 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3475 | `				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 3476 | `			pVm->iExitStatus = 255;` |
|        3 | 3477 | `			pVm->bHaltRequested = 1;` |
|        3 | 3478 | `			goto Abort;` |
|        - | 3479 | `		}` |
|        - | 3480 | `		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's` |
|        - | 3481 | `		 * compile-time case: php quietly creates an ordinary symbol-table` |
|        - | 3482 | `		 * entry named GLOBALS, leaving the auto-global view intact. */` |
|        3 | 3483 | `		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);` |
|        3 | 3484 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 3485 | `		break;` |
|        - | 3486 | `	}` |
|        - | 3487 | `	/* Extract the desired variable and if not available dynamically create it */` |
|  1703925 | 3488 | `	pObj = pInstr->p3` |
|  1136857 | 3489 | `		? PH7_VmExtractVarSlot(&(*pVm),&sName,TRUE,pInstr->nSite,aInstr)` |
|   567052 | 3490 | `		: VmExtractMemObj(&(*pVm),&sName,TRUE,TRUE);` |
|  1136884 | 3491 | `	if( pObj == 0 ){` |
|      ! 0 | 3492 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 3493 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 3494 | `		goto Abort;` |
|        - | 3495 | `	}` |
|  1136884 | 3496 | `	if( !pInstr->p3 ){` |
|       24 | 3497 | `		PH7_MemObjRelease(&pTos[1]);` |
|       11 | 3498 | `	}` |
|        - | 3499 | ``	/* A plain VARIABLE can BE a typed property's slot: `$r = &$o->n` aliases it,`` |
|        - | 3500 | `	 * and so does a by-ref parameter bound to one. php enforces the declared type` |
|        - | 3501 | ``	 * on this store exactly as on `$o->n = v` -- with a sentence of its own, which`` |
|        - | 3502 | `	 * names the property HOLDING the reference -- and PHL wrote through it` |
|        - | 3503 | `` 	 * unchecked, leaving a string in an `int` property and an array in a `string` `` |
|        - | 3504 | `	 * one. The store filter's table answers in one lookup and is skipped outright` |
|        - | 3505 | `	 * when nothing has registered a slot; measured at noise level on an` |
|        - | 3506 | `	 * object-and-array workload (a loop of nothing but plain stores is where the` |
|        - | 3507 | `	 * lookup shows at all). */` |
|  1136884 | 3508 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) > 0 && pObj->nIdx != SXU32_HIGH ){` |
|   489055 | 3509 | `		sxi32 rcRef = VmEnforcePropertyTypeOnStore(&(*pVm),pObj->nIdx,pTos,` |
|        - | 3510 | `			VM_TYPED_STORE_VIA_REF);` |
|   489055 | 3511 | `		if( rcRef == PH7_ABORT ){` |
|      ! 0 | 3512 | `			goto Abort;` |
|        - | 3513 | `		}` |
|   489055 | 3514 | `		if( rcRef == PH7_EXCEPTION ){` |
|       17 | 3515 | `			VmPopOperand(&pTos,1);` |
|        - | 3516 | `			{` |
|        - | 3517 | `				sxi32 iRp;` |
|       17 | 3518 | `				if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 3519 | `					PH7_RESUME_DRAIN()` |
|      ! 0 | 3520 | `					pc = iRp;` |
|      ! 0 | 3521 | `					break;` |
|        - | 3522 | `				}` |
|        - | 3523 | `			}` |
|       17 | 3524 | `			goto Exception;` |
|        - | 3525 | `		}` |
|   245444 | 3526 | `	}` |
|        - | 3527 | `	/* Perform the store operation */` |
|  1136868 | 3528 | `	PH7_MemObjStore(pTos,pObj);` |
|  1136868 | 3529 | `	break;` |
|        - | 3530 | `				   }` |
|        - | 3531 | `/*` |
|        - | 3532 | ` * STORE_IDX:   P1 * P3` |
|        - | 3533 | ` * STORE_IDX_R: P1 * P3` |
|        - | 3534 | ` *` |
|        - | 3535 | ` * Perfrom a store operation an a hashmap entry.` |
|        - | 3536 | ` */` |
|   258515 | 3537 | `case PH7_OP_STORE_IDX:` |
|        - | 3538 | `case PH7_OP_STORE_IDX_REF: {` |
|        - | 3539 | `	VmOpRc rcOp;` |
|   516841 | 3540 | `	sState.pTos = pTos;` |
|   516841 | 3541 | `	sState.pc = pc;` |
|   516841 | 3542 | `	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);` |
|   516841 | 3543 | `	pTos = sState.pTos;` |
|   516841 | 3544 | `	pc = sState.pc;` |
|   516841 | 3545 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3546 | `		goto Abort;` |
|   516839 | 3547 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       57 | 3548 | `		goto Exception;` |
|        - | 3549 | `	}` |
|   516787 | 3550 | `	break;` |
|        - | 3551 | `					  }` |
|        - | 3552 | `/*` |
|        - | 3553 | ` * INCR: P1 * *` |
|        - | 3554 | ` *` |
|        - | 3555 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - | 3556 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - | 3557 | ` * the stack and increment after that.` |
|        - | 3558 | ` */` |
|   399811 | 3559 | `case PH7_OP_INCR:` |
|        - | 3560 | `#ifdef UNTRUST` |
|        - | 3561 | `	if( pTos < pStack ){` |
|        - | 3562 | `		goto Abort;` |
|        - | 3563 | `	}` |
|        - | 3564 | `#endif` |
|        - | 3565 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3566 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3567 | `	 * — which otherwise skips object/array/resource operands. */` |
|   801137 | 3568 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3569 | `	/* php refuses to increment a string OFFSET, whatever byte it holds. */` |
|   801129 | 3570 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3571 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3572 | `	 * php's TypeError, raised BEFORE any set dispatch (the type guard below` |
|        - | 3573 | `	 * would skip the mutation and the tail write-back would otherwise call` |
|        - | 3574 | `	 * the set hook with the unchanged value). */` |
|   801108 | 3575 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|   401328 | 3576 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        5 | 3577 | `		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        5 | 3578 | `		if( VM_HOOK_PEND_IS_RMW(pTopInc->iKind) && pTopInc->nScratchIdx == pTos->nIdx ){` |
|        - | 3579 | `			SyBlob sErrMsg;` |
|        5 | 3580 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 | 3581 | `			VmIncDecTypeErrorMsg(pTos,TRUE,&sErrMsg);` |
|        5 | 3582 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        5 | 3583 | `			VmHookRmwDropTop(&(*pVm));` |
|        5 | 3584 | `			pTos->nIdx = SXU32_HIGH;` |
|        5 | 3585 | `			break;` |
|        - | 3586 | `		}` |
|      ! 0 | 3587 | `	}` |
|   801109 | 3588 | `	PH7_INCDEC_NATIVE_ARITH("+")` |
|   801107 | 3589 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3590 | `		/* php's ++ operand contract: an array, object or resource is a catchable` |
|        - | 3591 | `		 * TypeError, not the silent no-op this used to be. Settle the operand` |
|        - | 3592 | `		 * (the op's result slot) BEFORE throwing, like the arithmetic sites. */` |
|        - | 3593 | `		SyBlob sIncMsg;` |
|        - | 3594 | `		sxi32 rcInc;` |
|       25 | 3595 | `		SyBlobInit(&sIncMsg,&pVm->sAllocator);` |
|       25 | 3596 | `		VmIncDecTypeErrorMsg(pTos,TRUE,&sIncMsg);` |
|       25 | 3597 | `		PH7_MemObjRelease(pTos);` |
|       25 | 3598 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       25 | 3599 | `		pTos->nIdx = SXU32_HIGH;` |
|       37 | 3600 | `		rcInc = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sIncMsg),` |
|       12 | 3601 | `			SyBlobLength(&sIncMsg));` |
|       25 | 3602 | `		SyBlobRelease(&sIncMsg);` |
|       25 | 3603 | `		if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|       25 | 3604 | `		rc = rcInc;` |
|       25 | 3605 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3606 | `	}` |
|        - | 3607 | ``	/* php 8.3+: `++` on a BOOL is a no-op it warns about (E_WARNING, errno 2 —`` |
|        - | 3608 | ``	 * same shape as the null `--` below), where PH7 coerced true to int 2. */`` |
|   801083 | 3609 | `	if( pTos->iFlags & MEMOBJ_BOOL ){` |
|        7 | 3610 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3611 | `			"Increment on type bool has no effect, this will change in the next major version of PHP");` |
|        3 | 3612 | `	}` |
|   801083 | 3613 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_BOOL)) == 0 ){` |
|   801077 | 3614 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3615 | `			ph7_value *pObj;` |
|   801065 | 3616 | `			if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   801065 | 3617 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3618 | `					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric` |
|        - | 3619 | `					 * string (it points at str_increment() instead); PHL rejects it. */` |
|        - | 3620 | `					SyBlob sErrMsg;` |
|        3 | 3621 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3622 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3623 | `						"Increment on a non-numeric string is not supported, use str_increment() instead",` |
|        - | 3624 | `						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);` |
|        3 | 3625 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3626 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3627 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3628 | `					break;` |
|      ! 0 | 3629 | `				}else{` |
|        - | 3630 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - | 3631 | `					 * original value: pTos may alias pObj's blob via` |
|        - | 3632 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - | 3633 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - | 3634 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - | 3635 | `					 * so its old-value view survives the coercion. */` |
|   801063 | 3636 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 | 3637 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        5 | 3638 | `					}` |
|        - | 3639 | `					/* Force a numeric cast on the variable */` |
|   801063 | 3640 | `					PH7_MemObjToNumeric(pObj);` |
|   801063 | 3641 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        7 | 3642 | `						pObj->rVal++;` |
|        - | 3643 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3644 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3645 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3646 | `						 * integer-valued real. */` |
|        7 | 3647 | `						PH7_MemObjTryInteger(pObj);` |
|        4 | 3648 | `					}else{` |
|        - | 3649 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3650 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3651 | `						sxi64 r;` |
|   801057 | 3652 | `						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3653 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        7 | 3654 | `							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;` |
|        7 | 3655 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3656 | `#else` |
|        - | 3657 | `							pObj->x.iVal = r;` |
|        - | 3658 | `#endif` |
|        4 | 3659 | `						}else{` |
|   801051 | 3660 | `							pObj->x.iVal = r;` |
|        - | 3661 | `						}` |
|        - | 3662 | `					}` |
|   801063 | 3663 | `					if( pInstr->iP1 ){` |
|        - | 3664 | `						/* Pre-increment: result is the new value. */` |
|     2087 | 3665 | `						PH7_MemObjStore(pObj,pTos);` |
|     1043 | 3666 | `					}` |
|        - | 3667 | `					/* Post-increment: pTos retains the old value (a string` |
|        - | 3668 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - | 3669 | `					/* A NATIVE class's property is php's own C struct field, and` |
|        - | 3670 | ``					 * `++` writes it BACK through the write handler there — so the`` |
|        - | 3671 | `					 * conversion runs on the mutated slot. php's own answer, and` |
|        - | 3672 | `					 * the EXPRESSION's value is the unconverted sum either way:` |
|        - | 3673 | ``					 * `$i->f = 1.456008; var_dump(++$i->f, $i->f)` prints`` |
|        - | 3674 | `					 * 2.4560079999999997 then 2.456007. */` |
|   801063 | 3675 | `					PH7_NATIVE_SET_AFTER_MUTATE(pTos->nIdx,pObj)` |
|        - | 3676 | `				}` |
|   401283 | 3677 | `			}` |
|   401288 | 3678 | `		}else{` |
|       13 | 3679 | `			if( pInstr->iP1 ){` |
|      ! 0 | 3680 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 | 3681 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 | 3682 | `				}else{` |
|        - | 3683 | `					/* Force a numeric cast */` |
|      ! 0 | 3684 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3685 | `					/* Pre-increment */` |
|      ! 0 | 3686 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3687 | `						pTos->rVal++;` |
|        - | 3688 | `						/* Try to get an integer representation */` |
|      ! 0 | 3689 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3690 | `					}else{` |
|        - | 3691 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3692 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3693 | `						sxi64 r;` |
|      ! 0 | 3694 | `						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3695 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3696 | `							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;` |
|      ! 0 | 3697 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3698 | `#else` |
|        - | 3699 | `							pTos->x.iVal = r;` |
|        - | 3700 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3701 | `#endif` |
|      ! 0 | 3702 | `						}else{` |
|      ! 0 | 3703 | `							pTos->x.iVal = r;` |
|      ! 0 | 3704 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3705 | `						}` |
|        - | 3706 | `					}` |
|        - | 3707 | `				}` |
|      ! 0 | 3708 | `			}` |
|        - | 3709 | `		}` |
|   401289 | 3710 | `	}` |
|   801081 | 3711 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|   801081 | 3712 | `	break;` |
|        - | 3713 | `/*` |
|        - | 3714 | ` * DECR: P1 * *` |
|        - | 3715 | ` *` |
|        - | 3716 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - | 3717 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - | 3718 | ` * and decrement after that.` |
|        - | 3719 | ` */` |
|       87 | 3720 | `case PH7_OP_DECR:` |
|        - | 3721 | `#ifdef UNTRUST` |
|        - | 3722 | `	if( pTos < pStack ){` |
|        - | 3723 | `		goto Abort;` |
|        - | 3724 | `	}` |
|        - | 3725 | `#endif` |
|        - | 3726 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3727 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3728 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|        - | 3729 | `	 * property currently holding null). */` |
|      181 | 3730 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3731 | `	/* php refuses to decrement a string OFFSET, whatever byte it holds. */` |
|      168 | 3732 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3733 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3734 | `	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —` |
|        - | 3735 | ``	 * `--` on null is php's no-op and its write-back still dispatches set). */`` |
|      160 | 3736 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|       90 | 3737 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 3738 | `		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 3739 | `		if( VM_HOOK_PEND_IS_RMW(pTopDec->iKind) && pTopDec->nScratchIdx == pTos->nIdx ){` |
|        - | 3740 | `			SyBlob sErrMsg;` |
|        3 | 3741 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3742 | `			VmIncDecTypeErrorMsg(pTos,FALSE,&sErrMsg);` |
|        3 | 3743 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3744 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 3745 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 3746 | `			break;` |
|        - | 3747 | `		}` |
|      ! 0 | 3748 | `	}` |
|      162 | 3749 | `	PH7_INCDEC_NATIVE_ARITH("-")` |
|      160 | 3750 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3751 | ``		/* php's `--` operand contract, the mirror of INCR's: an array, object or`` |
|        - | 3752 | `		 * resource is a catchable TypeError, not a silent no-op. */` |
|        - | 3753 | `		SyBlob sDecMsg;` |
|        - | 3754 | `		sxi32 rcDec;` |
|       11 | 3755 | `		SyBlobInit(&sDecMsg,&pVm->sAllocator);` |
|       11 | 3756 | `		VmIncDecTypeErrorMsg(pTos,FALSE,&sDecMsg);` |
|       11 | 3757 | `		PH7_MemObjRelease(pTos);` |
|       11 | 3758 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 3759 | `		pTos->nIdx = SXU32_HIGH;` |
|       16 | 3760 | `		rcDec = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sDecMsg),` |
|        5 | 3761 | `			SyBlobLength(&sDecMsg));` |
|       11 | 3762 | `		SyBlobRelease(&sDecMsg);` |
|       11 | 3763 | `		if( rcDec == SXERR_ABORT ){ goto Abort; }` |
|       11 | 3764 | `		rc = rcDec;` |
|       13 | 3765 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3766 | `	}` |
|        - | 3767 | ``	/* NULL and BOOL stay excluded: PHP leaves `--` on either untouched (no-op) --`` |
|        - | 3768 | `	 * but 8.3 warns about both no-ops, same as the non-numeric-string one below. */` |
|      150 | 3769 | `	if( pTos->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|        - | 3770 | `		/* E_WARNING, not E_DEPRECATED -- php reports these at errno 2. */` |
|       19 | 3771 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3772 | `			"Decrement on type %s has no effect, this will change in the next major version of PHP",` |
|       12 | 3773 | `			(pTos->iFlags & MEMOBJ_NULL) ? "null" : "bool");` |
|        6 | 3774 | `	}` |
|      150 | 3775 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      138 | 3776 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3777 | `			ph7_value *pObj;` |
|      136 | 3778 | `			if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      136 | 3779 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3780 | ``					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string`` |
|        - | 3781 | `					 * (php has no string decrement); PHL rejects it. */` |
|        - | 3782 | `					SyBlob sErrMsg;` |
|        3 | 3783 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3784 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3785 | `						"Decrement on a non-numeric string is not supported",` |
|        - | 3786 | `						sizeof("Decrement on a non-numeric string is not supported")-1);` |
|        3 | 3787 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3788 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3789 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3790 | `					break;` |
|      ! 0 | 3791 | `				}else{` |
|        - | 3792 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - | 3793 | `					 * post-decrement must preserve pTos's original value, which` |
|        - | 3794 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - | 3795 | `					 * Force pTos to own its blob before coercing pObj. */` |
|      133 | 3796 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 | 3797 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 | 3798 | `					}` |
|      133 | 3799 | `					PH7_MemObjToNumeric(pObj);` |
|      133 | 3800 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|       11 | 3801 | `						pObj->rVal--;` |
|        - | 3802 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3803 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3804 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3805 | `						 * integer-valued real. */` |
|       11 | 3806 | `						PH7_MemObjTryInteger(pObj);` |
|        6 | 3807 | `					}else{` |
|        - | 3808 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3809 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3810 | `						sxi64 r;` |
|      123 | 3811 | `						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3812 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        3 | 3813 | `							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;` |
|        3 | 3814 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3815 | `#else` |
|        - | 3816 | `							pObj->x.iVal = r;` |
|        - | 3817 | `#endif` |
|        2 | 3818 | `						}else{` |
|      121 | 3819 | `							pObj->x.iVal = r;` |
|        - | 3820 | `						}` |
|        - | 3821 | `					}` |
|      133 | 3822 | `					if( pInstr->iP1 ){` |
|        - | 3823 | `						/* Pre-decrement: result is the new value. */` |
|        3 | 3824 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 | 3825 | `					}` |
|        - | 3826 | `					/* Post-decrement: pTos retains the old value. */` |
|      133 | 3827 | `					PH7_NATIVE_SET_AFTER_MUTATE(pTos->nIdx,pObj)   /* see OP_INCR */` |
|        - | 3828 | `				}` |
|       64 | 3829 | `			}` |
|       67 | 3830 | `		}else{` |
|        3 | 3831 | `			if( pInstr->iP1 ){` |
|        3 | 3832 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - | 3833 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 | 3834 | `				}else{` |
|        - | 3835 | `					/* Force a numeric cast */` |
|        3 | 3836 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3837 | `					/* Pre-decrement */` |
|        3 | 3838 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3839 | `						pTos->rVal--;` |
|        - | 3840 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 | 3841 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3842 | `					}else{` |
|        - | 3843 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3844 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3845 | `						sxi64 r;` |
|        3 | 3846 | `						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3847 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3848 | `							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;` |
|      ! 0 | 3849 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3850 | `#else` |
|        - | 3851 | `							pTos->x.iVal = r;` |
|        - | 3852 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3853 | `#endif` |
|      ! 0 | 3854 | `						}else{` |
|        3 | 3855 | `							pTos->x.iVal = r;` |
|        3 | 3856 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3857 | `						}` |
|        - | 3858 | `					}` |
|        - | 3859 | `				}` |
|        1 | 3860 | `			}` |
|        - | 3861 | `		}` |
|       65 | 3862 | `	}` |
|      147 | 3863 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|      147 | 3864 | `	break;` |
|        - | 3865 | `/*` |
|        - | 3866 | ` * UMINUS: * * *` |
|        - | 3867 | ` *` |
|        - | 3868 | ` * Perform a unary minus operation.` |
|        - | 3869 | ` */` |
|    51981 | 3870 | `case PH7_OP_UMINUS:` |
|        - | 3871 | `#ifdef UNTRUST` |
|        - | 3872 | `	if( pTos < pStack ){` |
|        - | 3873 | `		goto Abort;` |
|        - | 3874 | `	}` |
|        - | 3875 | `#endif` |
|        - | 3876 | ``	/* php's `$x * -1` operand contract: an array, object, resource or`` |
|        - | 3877 | `	 * non-numeric string is a TypeError, not a warned int(-1). */` |
|   103955 | 3878 | `	PH7_UNARY_ARITH_CONTRACT(-1)` |
|        - | 3879 | `	/* Force a numeric (integer,real or both) cast */` |
|   103927 | 3880 | `	PH7_MemObjToNumeric(pTos);` |
|   103927 | 3881 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      325 | 3882 | `		pTos->rVal = -pTos->rVal;` |
|      160 | 3883 | `	}` |
|   103927 | 3884 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|   103673 | 3885 | `		if( pTos->x.iVal == SMALLEST_INT64 ){` |
|        - | 3886 | `			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a` |
|        - | 3887 | `			 * REAL representation is already present it is the negated` |
|        - | 3888 | `			 * authoritative value, so just drop the now-stale cached int. The` |
|        - | 3889 | `			 * integer-only build has no float type, so it wraps (two's` |
|        - | 3890 | `			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */` |
|        - | 3891 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        7 | 3892 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 | 3893 | `				pTos->rVal = -(ph7_real)pTos->x.iVal;` |
|        7 | 3894 | `				MemObjSetType(pTos,MEMOBJ_REAL);` |
|        4 | 3895 | `			}else{` |
|      ! 0 | 3896 | `				pTos->iFlags &= ~MEMOBJ_INT;` |
|        - | 3897 | `			}` |
|        - | 3898 | `#else` |
|        - | 3899 | `			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);` |
|        - | 3900 | `#endif` |
|        4 | 3901 | `		}else{` |
|   103667 | 3902 | `			pTos->x.iVal = -pTos->x.iVal;` |
|        - | 3903 | `		}` |
|    51828 | 3904 | `	}` |
|   103927 | 3905 | `	break;` |
|        - | 3906 | `/*` |
|        - | 3907 | ` * UPLUS: * * *` |
|        - | 3908 | ` *` |
|        - | 3909 | ` * Perform a unary plus operation.` |
|        - | 3910 | ` */` |
|       24 | 3911 | `case PH7_OP_UPLUS:` |
|        - | 3912 | `#ifdef UNTRUST` |
|        - | 3913 | `	if( pTos < pStack ){` |
|        - | 3914 | `		goto Abort;` |
|        - | 3915 | `	}` |
|        - | 3916 | `#endif` |
|        - | 3917 | ``	/* Unary plus is php's `$x * 1`, so it answers the same TypeError as unary`` |
|        - | 3918 | ``	 * minus -- naming `int` as the other operand, exactly as php does. */`` |
|       49 | 3919 | `	PH7_UNARY_ARITH_CONTRACT(1)` |
|        - | 3920 | `	/* Force a numeric (integer,real or both) cast */` |
|       41 | 3921 | `	PH7_MemObjToNumeric(pTos);` |
|       41 | 3922 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3923 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 | 3924 | `	}` |
|       41 | 3925 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       41 | 3926 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       20 | 3927 | `	}` |
|       41 | 3928 | `	break;` |
|        - | 3929 | `/*` |
|        - | 3930 | ` * OP_LNOT: * * *` |
|        - | 3931 | ` *` |
|        - | 3932 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - | 3933 | ` * with its complement.` |
|        - | 3934 | ` */` |
|    66187 | 3935 | `case PH7_OP_LNOT:` |
|        - | 3936 | `#ifdef UNTRUST` |
|        - | 3937 | `	if( pTos < pStack ){` |
|        - | 3938 | `		goto Abort;` |
|        - | 3939 | `	}` |
|        - | 3940 | `#endif` |
|        - | 3941 | `	/* Force a boolean cast */` |
|   132073 | 3942 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      754 | 3943 | `		PH7_MemObjToBool(pTos);` |
|      370 | 3944 | `	}` |
|   132073 | 3945 | `	pTos->x.iVal = !pTos->x.iVal;` |
|   132073 | 3946 | `	break;` |
|        - | 3947 | `/*` |
|        - | 3948 | ` * OP_BITNOT: * * *` |
|        - | 3949 | ` *` |
|        - | 3950 | ` * Interpret the top of the stack as an value.Replace it` |
|        - | 3951 | ` * with its ones-complement.` |
|        - | 3952 | ` */` |
|      343 | 3953 | `case PH7_OP_BITNOT:` |
|        - | 3954 | `#ifdef UNTRUST` |
|        - | 3955 | `	if( pTos < pStack ){` |
|        - | 3956 | `		goto Abort;` |
|        - | 3957 | `	}` |
|        - | 3958 | `#endif` |
|      690 | 3959 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - | 3960 | ``		/* php's `~` over a STRING is a per-BYTE ones-complement of the same`` |
|        - | 3961 | ``		 * length — a binary string, not an integer operation, so `~"abc"` is`` |
|        - | 3962 | `		 * "\x9e\x9d\x9c" where PHL cast the string to an int and answered` |
|        - | 3963 | `		 * int(-1). The operand's blob can be a READ-ONLY VIEW of the variable's` |
|        - | 3964 | `		 * own buffer (PH7_MemObjLoad), so complement into a fresh blob and swap` |
|        - | 3965 | `		 * it in rather than writing through the view. */` |
|        - | 3966 | `		SyBlob sNotBuf;` |
|       17 | 3967 | `		const unsigned char *zNotIn = (const unsigned char *)SyBlobData(&pTos->sBlob);` |
|       17 | 3968 | `		sxu32 nNotIn = SyBlobLength(&pTos->sBlob), iNot;` |
|       17 | 3969 | `		SyBlobInit(&sNotBuf,&pVm->sAllocator);` |
|       53 | 3970 | `		for( iNot = 0 ; iNot < nNotIn ; ++iNot ){` |
|       37 | 3971 | `			unsigned char cNot = (unsigned char)~zNotIn[iNot];` |
|       37 | 3972 | `			SyBlobAppend(&sNotBuf,(const void *)&cNot,sizeof(char));` |
|       19 | 3973 | `		}` |
|       17 | 3974 | `		PH7_MemObjRelease(pTos);` |
|       17 | 3975 | `		MemObjSetType(pTos,MEMOBJ_STRING);` |
|       17 | 3976 | `		if( SyBlobLength(&sNotBuf) > 0 ){` |
|       15 | 3977 | `			SyBlobAppend(&pTos->sBlob,SyBlobData(&sNotBuf),SyBlobLength(&sNotBuf));` |
|        7 | 3978 | `		}` |
|       17 | 3979 | `		SyBlobRelease(&sNotBuf);` |
|       17 | 3980 | `		break;` |
|        - | 3981 | `	}` |
|      674 | 3982 | `	if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        - | 3983 | `		/* Everything else — null, a bool, an array, an object, a resource — has` |
|        - | 3984 | `		 * no bitwise-not in php at all: a catchable TypeError naming the operand,` |
|        - | 3985 | `		 * where PHL cast it to an int and answered ~0 / ~1. */` |
|        - | 3986 | `		SyBlob sNotMsg;` |
|        - | 3987 | `		sxi32 rcNot;` |
|       27 | 3988 | `		SyBlobInit(&sNotMsg,&pVm->sAllocator);` |
|       27 | 3989 | `		VmBitNotTypeErrorMsg(pTos,&sNotMsg);` |
|       27 | 3990 | `		PH7_MemObjRelease(pTos);` |
|       27 | 3991 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       27 | 3992 | `		pTos->nIdx = SXU32_HIGH;` |
|       40 | 3993 | `		rcNot = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sNotMsg),` |
|       13 | 3994 | `			SyBlobLength(&sNotMsg));` |
|       27 | 3995 | `		SyBlobRelease(&sNotMsg);` |
|       27 | 3996 | `		if( rcNot == SXERR_ABORT ){ goto Abort; }` |
|       27 | 3997 | `		rc = rcNot;` |
|       27 | 3998 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3999 | `	}` |
|        - | 4000 | `	/* Force an integer cast (php deprecates a lossy float here too) */` |
|      648 | 4001 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      648 | 4002 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      648 | 4003 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 4004 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 4005 | `	}` |
|      648 | 4006 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        - | 4007 | `	/* An INTEGRAL float carries a cached MEMOBJ_INT beside MEMOBJ_REAL, so the` |
|        - | 4008 | `` 	 * cast above is skipped and the value kept rendering as a float: `~2.0` `` |
|        - | 4009 | ``	 * answered 2.0 instead of -3. `~` always yields an int. */`` |
|      648 | 4010 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      648 | 4011 | `	break;` |
|        - | 4012 | `/* OP_MUL * * *` |
|        - | 4013 | ` * OP_MUL_STORE * * *` |
|        - | 4014 | ` *` |
|        - | 4015 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - | 4016 | ` * and push the result back onto the stack.` |
|        - | 4017 | ` */` |
|     3707 | 4018 | `case PH7_OP_MUL:` |
|        - | 4019 | `case PH7_OP_MUL_STORE: {` |
|        - | 4020 | `	VmOpRc rcOp;` |
|        - | 4021 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     7405 | 4022 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     7403 | 4023 | `	sState.pTos = pTos;` |
|     7403 | 4024 | `	sState.pc = pc;` |
|     7403 | 4025 | `	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);` |
|     7403 | 4026 | `	pTos = sState.pTos;` |
|     7403 | 4027 | `	pc = sState.pc;` |
|     7403 | 4028 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4029 | `		goto Abort;` |
|     7403 | 4030 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      312 | 4031 | `		goto Exception;` |
|        - | 4032 | `	}` |
|     7093 | 4033 | `	break;` |
|        - | 4034 | `					  }` |
|        - | 4035 | `/* OP_POW * * *` |
|        - | 4036 | ` * OP_POW_STORE * * *` |
|        - | 4037 | ` *` |
|        - | 4038 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - | 4039 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - | 4040 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - | 4041 | ` * fits in sxi64; otherwise the result is a double.` |
|        - | 4042 | ` */` |
|      535 | 4043 | `case PH7_OP_POW:` |
|        - | 4044 | `case PH7_OP_POW_STORE: {` |
|        - | 4045 | `	VmOpRc rcOp;` |
|        - | 4046 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     1072 | 4047 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     1070 | 4048 | `	sState.pTos = pTos;` |
|     1070 | 4049 | `	sState.pc = pc;` |
|     1070 | 4050 | `	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);` |
|     1070 | 4051 | `	pTos = sState.pTos;` |
|     1070 | 4052 | `	pc = sState.pc;` |
|     1070 | 4053 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4054 | `		goto Abort;` |
|     1070 | 4055 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      318 | 4056 | `		goto Exception;` |
|        - | 4057 | `	}` |
|      753 | 4058 | `	break;` |
|        - | 4059 | `					  }` |
|        - | 4060 | `/* OP_ADD * * *` |
|        - | 4061 | ` *` |
|        - | 4062 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 4063 | ` * and push the result back onto the stack.` |
|        - | 4064 | ` */` |
|    19945 | 4065 | `case PH7_OP_ADD:{` |
|    39782 | 4066 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4067 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|        - | 4068 | `	int rcNa;` |
|    39782 | 4069 | `	const char *zArCls = "TypeError";` |
|        - | 4070 | `#ifdef UNTRUST` |
|        - | 4071 | `	if( pNos < pStack ){` |
|        - | 4072 | `		goto Abort;` |
|        - | 4073 | `	}` |
|        - | 4074 | `#endif` |
|        - | 4075 | `	{` |
|        - | 4076 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|        - | 4077 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|        - | 4078 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|        - | 4079 | `		SyBlob sArMsg;` |
|    39782 | 4080 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    39782 | 4081 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"+",pNos,&zArCls,&sArMsg);` |
|    39782 | 4082 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|        - | 4083 | `			sxi32 rcAr;` |
|      182 | 4084 | `			VmPopOperand(&pTos,1);` |
|      182 | 4085 | `			PH7_MemObjRelease(pTos);` |
|      182 | 4086 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      182 | 4087 | `			pTos->nIdx = SXU32_HIGH;` |
|      272 | 4088 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|       90 | 4089 | `				SyBlobLength(&sArMsg));` |
|      182 | 4090 | `			SyBlobRelease(&sArMsg);` |
|      182 | 4091 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      182 | 4092 | `			rc = rcAr;` |
|      182 | 4093 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4094 | `		}` |
|    39602 | 4095 | `		SyBlobRelease(&sArMsg);` |
|        - | 4096 | `	}` |
|        - | 4097 | `	/* Perform the addition (unless a do_operation handler already answered) */` |
|    39602 | 4098 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|    39588 | 4099 | `		PH7_MemObjAdd(pNos,pTos,FALSE);` |
|    19735 | 4100 | `	}` |
|    39602 | 4101 | `	VmPopOperand(&pTos,1);` |
|    39602 | 4102 | `	break;` |
|        - | 4103 | `				}` |
|        - | 4104 | `/*` |
|        - | 4105 | ` * OP_ADD_STORE * * *` |
|        - | 4106 | ` *` |
|        - | 4107 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 4108 | ` * and push the result back onto the stack.` |
|        - | 4109 | ` */` |
|   235786 | 4110 | `case PH7_OP_ADD_STORE:{` |
|   471843 | 4111 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4112 | `	ph7_value *pObj;` |
|        - | 4113 | `	sxu32 nIdx;` |
|        - | 4114 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|        - | 4115 | `	int rcNa;` |
|   471843 | 4116 | `	const char *zArCls = "TypeError";` |
|        - | 4117 | `#ifdef UNTRUST` |
|        - | 4118 | `	if( pNos < pStack ){` |
|        - | 4119 | `		goto Abort;` |
|        - | 4120 | `	}` |
|        - | 4121 | `#endif` |
|        - | 4122 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|   707550 | 4123 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 4124 | `	{` |
|        - | 4125 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 4126 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 4127 | `		SyBlob sArMsg;` |
|   471837 | 4128 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   471837 | 4129 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"+",pTos,&zArCls,&sArMsg);` |
|   471837 | 4130 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|        - | 4131 | `			sxi32 rcAr;` |
|      151 | 4132 | `			VmPopOperand(&pTos,1);` |
|      151 | 4133 | `			PH7_MemObjRelease(pTos);` |
|      151 | 4134 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      151 | 4135 | `			pTos->nIdx = SXU32_HIGH;` |
|      226 | 4136 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|       75 | 4137 | `				SyBlobLength(&sArMsg));` |
|      151 | 4138 | `			SyBlobRelease(&sArMsg);` |
|      151 | 4139 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      151 | 4140 | `			rc = rcAr;` |
|      151 | 4141 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4142 | `		}` |
|   471687 | 4143 | `		SyBlobRelease(&sArMsg);` |
|        - | 4144 | `	}` |
|        - | 4145 | `	/* Perform the addition */` |
|   471687 | 4146 | `	nIdx = pTos->nIdx;` |
|   471687 | 4147 | `	if( nIdx == pVm->nGlobalIdx ){` |
|        - | 4148 | `		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment` |
|        - | 4149 | `		 * (a compile-time fatal in php; raised here, same message). */` |
|        3 | 4150 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 4151 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 4152 | `		pVm->iExitStatus = 255;` |
|        3 | 4153 | `		pVm->bHaltRequested = 1;` |
|        3 | 4154 | `		goto Abort;` |
|        - | 4155 | `	}` |
|   471685 | 4156 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|   471683 | 4157 | `		PH7_MemObjAdd(pTos,pNos,TRUE);` |
|   235972 | 4158 | `	}` |
|        - | 4159 | `	/* Peform the store operation */` |
|   471685 | 4160 | `	if( nIdx == SXU32_HIGH ){` |
|        - | 4161 | ``		/* A read-modify-write THROUGH a temporary (`f()[0] += 5`): php computes it,`` |
|        - | 4162 | `		 * drops it and stays silent. See the OP_STORE member note above. */` |
|   471683 | 4163 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|   471681 | 4164 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|   471677 | 4165 | `		PH7_MemObjStore(pTos,pObj);` |
|   235969 | 4166 | `	}` |
|   471681 | 4167 | `	PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|        - | 4168 | `	/* Ticket 1433-35: Perform a stack dup */` |
|   471681 | 4169 | `	PH7_MemObjStore(pTos,pNos);` |
|   471681 | 4170 | `	VmPopOperand(&pTos,1);` |
|   471681 | 4171 | `	break;` |
|        - | 4172 | `				}` |
|        - | 4173 | `/* OP_SUB * * *` |
|        - | 4174 | ` *` |
|        - | 4175 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 4176 | ` * first (what was next on the stack) from the second (the` |
|        - | 4177 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 4178 | ` */` |
|    30479 | 4179 | `case PH7_OP_SUB: {` |
|        - | 4180 | `	VmOpRc rcOp;` |
|    61569 | 4181 | `	sState.pTos = pTos;` |
|    61569 | 4182 | `	sState.pc = pc;` |
|    61569 | 4183 | `	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);` |
|    61569 | 4184 | `	pTos = sState.pTos;` |
|    61569 | 4185 | `	pc = sState.pc;` |
|    61569 | 4186 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4187 | `		goto Abort;` |
|    61569 | 4188 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      162 | 4189 | `		goto Exception;` |
|        - | 4190 | `	}` |
|    61409 | 4191 | `	break;` |
|        - | 4192 | `					  }` |
|        - | 4193 | `/* OP_SUB_STORE * * *` |
|        - | 4194 | ` *` |
|        - | 4195 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 4196 | ` * first (what was next on the stack) from the second (the` |
|        - | 4197 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 4198 | ` */` |
|      205 | 4199 | `case PH7_OP_SUB_STORE: {` |
|        - | 4200 | `	VmOpRc rcOp;` |
|        - | 4201 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      412 | 4202 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      410 | 4203 | `	sState.pTos = pTos;` |
|      410 | 4204 | `	sState.pc = pc;` |
|      410 | 4205 | `	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);` |
|      410 | 4206 | `	pTos = sState.pTos;` |
|      410 | 4207 | `	pc = sState.pc;` |
|      410 | 4208 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4209 | `		goto Abort;` |
|      410 | 4210 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      153 | 4211 | `		goto Exception;` |
|        - | 4212 | `	}` |
|      258 | 4213 | `	break;` |
|        - | 4214 | `					  }` |
|        - | 4215 |  |
|        - | 4216 | `/*` |
|        - | 4217 | ` * OP_MOD * * *` |
|        - | 4218 | ` *` |
|        - | 4219 | ` * Pop the top two elements from the stack, divide the` |
|        - | 4220 | ` * first (what was next on the stack) from the second (the` |
|        - | 4221 | ` * top of the stack) and push the remainder after division` |
|        - | 4222 | ` * onto the stack.` |
|        - | 4223 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 4224 | ` */` |
|     1166 | 4225 | `case PH7_OP_MOD: {` |
|        - | 4226 | `	VmOpRc rcOp;` |
|     2337 | 4227 | `	sState.pTos = pTos;` |
|     2337 | 4228 | `	sState.pc = pc;` |
|     2337 | 4229 | `	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);` |
|     2337 | 4230 | `	pTos = sState.pTos;` |
|     2337 | 4231 | `	pc = sState.pc;` |
|     2337 | 4232 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4233 | `		goto Abort;` |
|     2337 | 4234 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      248 | 4235 | `		goto Exception;` |
|        - | 4236 | `	}` |
|     2091 | 4237 | `	break;` |
|        - | 4238 | `					  }` |
|        - | 4239 | `/*` |
|        - | 4240 | ` * OP_MOD_STORE * * *` |
|        - | 4241 | ` *` |
|        - | 4242 | ` * Pop the top two elements from the stack, divide the` |
|        - | 4243 | ` * first (what was next on the stack) from the second (the` |
|        - | 4244 | ` * top of the stack) and push the remainder after division` |
|        - | 4245 | ` * onto the stack.` |
|        - | 4246 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 4247 | ` */` |
|      204 | 4248 | `case PH7_OP_MOD_STORE: {` |
|        - | 4249 | `	VmOpRc rcOp;` |
|        - | 4250 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      409 | 4251 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      407 | 4252 | `	sState.pTos = pTos;` |
|      407 | 4253 | `	sState.pc = pc;` |
|      407 | 4254 | `	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);` |
|      407 | 4255 | `	pTos = sState.pTos;` |
|      407 | 4256 | `	pc = sState.pc;` |
|      407 | 4257 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4258 | `		goto Abort;` |
|      407 | 4259 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      221 | 4260 | `		goto Exception;` |
|        - | 4261 | `	}` |
|      187 | 4262 | `	break;` |
|        - | 4263 | `					  }` |
|        - | 4264 | `/*` |
|        - | 4265 | ` * OP_DIV * * *` |
|        - | 4266 | ` *` |
|        - | 4267 | ` * Pop the top two elements from the stack, divide the` |
|        - | 4268 | ` * first (what was next on the stack) from the second (the` |
|        - | 4269 | ` * top of the stack) and push the result onto the stack.` |
|        - | 4270 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 4271 | ` */` |
|      479 | 4272 | `case PH7_OP_DIV: {` |
|        - | 4273 | `	VmOpRc rcOp;` |
|      961 | 4274 | `	sState.pTos = pTos;` |
|      961 | 4275 | `	sState.pc = pc;` |
|      961 | 4276 | `	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);` |
|      961 | 4277 | `	pTos = sState.pTos;` |
|      961 | 4278 | `	pc = sState.pc;` |
|      961 | 4279 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4280 | `		goto Abort;` |
|      961 | 4281 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      233 | 4282 | `		goto Exception;` |
|        - | 4283 | `	}` |
|      729 | 4284 | `	break;` |
|        - | 4285 | `					  }` |
|        - | 4286 | `/*` |
|        - | 4287 | ` * OP_DIV_STORE * * *` |
|        - | 4288 | ` *` |
|        - | 4289 | ` * Pop the top two elements from the stack, divide the` |
|        - | 4290 | ` * first (what was next on the stack) from the second (the` |
|        - | 4291 | ` * top of the stack) and push the result onto the stack.` |
|        - | 4292 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 4293 | ` */` |
|      211 | 4294 | `case PH7_OP_DIV_STORE:{` |
|      423 | 4295 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4296 | `	ph7_value *pObj;` |
|        - | 4297 | `	ph7_real a,b,r;` |
|        - | 4298 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|        - | 4299 | `	int rcNa;` |
|      423 | 4300 | `	const char *zArCls = "TypeError";` |
|        - | 4301 | `#ifdef UNTRUST` |
|        - | 4302 | `	if( pNos < pStack ){` |
|        - | 4303 | `		goto Abort;` |
|        - | 4304 | `	}` |
|        - | 4305 | `#endif` |
|        - | 4306 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      524 | 4307 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 4308 | `	{` |
|        - | 4309 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 4310 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 4311 | `		SyBlob sArMsg;` |
|      421 | 4312 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|      421 | 4313 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"/",pNos,&zArCls,&sArMsg);` |
|      421 | 4314 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|        - | 4315 | `			sxi32 rcAr;` |
|      151 | 4316 | `			VmPopOperand(&pTos,1);` |
|      151 | 4317 | `			PH7_MemObjRelease(pTos);` |
|      151 | 4318 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      151 | 4319 | `			pTos->nIdx = SXU32_HIGH;` |
|      226 | 4320 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|       75 | 4321 | `				SyBlobLength(&sArMsg));` |
|      151 | 4322 | `			SyBlobRelease(&sArMsg);` |
|      151 | 4323 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      151 | 4324 | `			rc = rcAr;` |
|      151 | 4325 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4326 | `		}` |
|      271 | 4327 | `		SyBlobRelease(&sArMsg);` |
|        - | 4328 | `	}` |
|      271 | 4329 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|        - | 4330 | ``		/* php's `/` answers an INT when both operands are ints and the division is`` |
|        - | 4331 | ``		 * exact (`6/3 === 2`, not `2.0`), and `$x /= $y` is that same operator: php`` |
|        - | 4332 | `		 * has one division and the compound form only decides where the answer goes.` |
|        - | 4333 | `		 * OP_DIV grew the rule (§2's int-boundary work) and this arm, a separate copy` |
|        - | 4334 | ``		 * of it, did not — so `$x = 6; $x /= 3;` left a FLOAT where `$x = $x / 3` left`` |
|        - | 4335 | ``		 * an int, visible through `===`, `var_dump`, `json_encode` and `is_int`.`` |
|        - | 4336 | ``		 * The divisor is screened BEFORE `ia % ib`: x86 computes the overflowing`` |
|        - | 4337 | `		 * PHP_INT_MIN/-1 quotient alongside the remainder and traps (OP_DIV and OP_MOD` |
|        - | 4338 | `		 * guard the same hazard the same way). */` |
|      269 | 4339 | `		int bExactDiv = 0;` |
|      269 | 4340 | `		PH7_MemObjToNumeric(pTos);` |
|      269 | 4341 | `		PH7_MemObjToNumeric(pNos);` |
|      269 | 4342 | `		if( ((pTos->iFlags\|pNos->iFlags) & MEMOBJ_REAL) == 0 ){` |
|      269 | 4343 | `			sxi64 ia = pTos->x.iVal;   /* the lvalue: php's dividend */` |
|      269 | 4344 | `			sxi64 ib = pNos->x.iVal;   /* the right operand: the divisor */` |
|      269 | 4345 | `			sxi64 iQuot = 0;` |
|      269 | 4346 | `			if( ib == 0 ){` |
|       71 | 4347 | `				rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|       73 | 4348 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      199 | 4349 | `			}else if( ib == -1 ){` |
|        - | 4350 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - | 4351 | `				iQuot = ( ia != SMALLEST_INT64 ) ? -ia : SMALLEST_INT64;` |
|        - | 4352 | `				bExactDiv = 1;` |
|        - | 4353 | `#else` |
|        7 | 4354 | `				if( ia != SMALLEST_INT64 ){` |
|        3 | 4355 | `					iQuot = -ia;` |
|        3 | 4356 | `					bExactDiv = 1;` |
|        2 | 4357 | `				}` |
|        - | 4358 | `#endif` |
|      196 | 4359 | `			}else if( ia % ib == 0 ){` |
|      109 | 4360 | `				iQuot = ia / ib;` |
|      109 | 4361 | `				bExactDiv = 1;` |
|       54 | 4362 | `			}` |
|      199 | 4363 | `			if( bExactDiv ){` |
|      111 | 4364 | `				pNos->x.iVal = iQuot;` |
|      111 | 4365 | `				MemObjSetType(pNos,MEMOBJ_INT);` |
|       55 | 4366 | `			}` |
|       99 | 4367 | `		}` |
|      199 | 4368 | `		if( !bExactDiv ){` |
|        - | 4369 | `			/* Force the operands to be real */` |
|       89 | 4370 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       89 | 4371 | `				PH7_MemObjToReal(pTos);` |
|       44 | 4372 | `			}` |
|       89 | 4373 | `			if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       89 | 4374 | `				PH7_MemObjToReal(pNos);` |
|       44 | 4375 | `			}` |
|        - | 4376 | `			/* Perform the requested operation */` |
|       89 | 4377 | `			a = pTos->rVal;` |
|       89 | 4378 | `			b = pNos->rVal;` |
|       89 | 4379 | `			if( b == 0 ){` |
|        - | 4380 | `				/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|        - | 4381 | `				 * not the old non-catchable warning that continued with a 0 result. */` |
|      ! 0 | 4382 | `				rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|      ! 0 | 4383 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      ! 0 | 4384 | `			}else{` |
|       89 | 4385 | `				r = a/b;` |
|        - | 4386 | `				/* Push the result */` |
|       89 | 4387 | `				pNos->rVal = r;` |
|       89 | 4388 | `				MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - | 4389 | `			}` |
|       44 | 4390 | `		}` |
|       99 | 4391 | `	}` |
|      201 | 4392 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|        - | 4393 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|      201 | 4394 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      201 | 4395 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|      201 | 4396 | `		PH7_MemObjStore(pNos,pObj);` |
|      100 | 4397 | `	}` |
|      201 | 4398 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|      201 | 4399 | `	VmPopOperand(&pTos,1);` |
|      201 | 4400 | `	break;` |
|        - | 4401 | `				}` |
|        - | 4402 | `/* OP_BAND * * *` |
|        - | 4403 | ` *` |
|        - | 4404 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4405 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 4406 | ` * two elements.` |
|        - | 4407 | `*/` |
|        - | 4408 | `/* OP_BOR * * *` |
|        - | 4409 | ` *` |
|        - | 4410 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4411 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 4412 | ` * two elements.` |
|        - | 4413 | ` */` |
|        - | 4414 | `/* OP_BXOR * * *` |
|        - | 4415 | ` *` |
|        - | 4416 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4417 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 4418 | ` * two elements.` |
|        - | 4419 | ` */` |
|     4337 | 4420 | `case PH7_OP_BAND:` |
|        - | 4421 | `case PH7_OP_BOR:` |
|        - | 4422 | `case PH7_OP_BXOR:{` |
|     8660 | 4423 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4424 | `	sxi64 a,b,r;` |
|        - | 4425 | `	int cBwOp;` |
|        - | 4426 | `#ifdef UNTRUST` |
|        - | 4427 | `	if( pNos < pStack ){` |
|        - | 4428 | `		goto Abort;` |
|        - | 4429 | `	}` |
|        - | 4430 | `#endif` |
|     8660 | 4431 | `	cBwOp = pInstr->iOp == PH7_OP_BOR ? '\|' : (pInstr->iOp == PH7_OP_BXOR ? '^' : '&');` |
|     8660 | 4432 | `	if( (pNos->iFlags & MEMOBJ_STRING) && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 4433 | `		/* TWO strings: php's per-byte string operation, result a string. */` |
|      149 | 4434 | `		PH7_STRING_BITWISE_RESULT(pNos,pNos,pTos,cBwOp)` |
|      149 | 4435 | `		VmPopOperand(&pTos,1);` |
|      149 | 4436 | `		break;` |
|        - | 4437 | `	}` |
|        - | 4438 | `	{` |
|        - | 4439 | `		char zBwOp[2];` |
|     8512 | 4440 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 4441 | `		/* Anything else is php's arithmetic operand contract, named for this` |
|        - | 4442 | ``		 * operator (`array & int`), not a silent integer cast. */`` |
|     8512 | 4443 | `		PH7_BITWISE_ARITH_CONTRACT(pNos,pTos,zBwOp,1)` |
|        - | 4444 | `	}` |
|        - | 4445 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     8088 | 4446 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     8088 | 4447 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     8082 | 4448 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     8082 | 4449 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     8082 | 4450 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      321 | 4451 | `		PH7_MemObjToInteger(pTos);` |
|      160 | 4452 | `	}` |
|     8082 | 4453 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      323 | 4454 | `		PH7_MemObjToInteger(pNos);` |
|      161 | 4455 | `	}` |
|        - | 4456 | `	/* Perform the requested operation */` |
|     8082 | 4457 | `	a = pNos->x.iVal;` |
|     8082 | 4458 | `	b = pTos->x.iVal;` |
|     8082 | 4459 | `	switch(pInstr->iOp){` |
|      595 | 4460 | `	case PH7_OP_BOR_STORE:` |
|     1195 | 4461 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|      216 | 4462 | `	case PH7_OP_BXOR_STORE:` |
|      433 | 4463 | `	case PH7_OP_BXOR: r = a^b; break;` |
|     3236 | 4464 | `	case PH7_OP_BAND_STORE:` |
|     3219 | 4465 | `	case PH7_OP_BAND:` |
|     6460 | 4466 | `	default:          r = a&b; break;` |
|        - | 4467 | `	}` |
|        - | 4468 | `	/* Push the result */` |
|     8082 | 4469 | `	pNos->x.iVal = r;` |
|     8082 | 4470 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     8082 | 4471 | `	VmPopOperand(&pTos,1);` |
|     8082 | 4472 | `	break;` |
|        - | 4473 | `				 }` |
|        - | 4474 | `/* OP_BAND_STORE * * *` |
|        - | 4475 | ` *` |
|        - | 4476 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4477 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 4478 | ` * two elements.` |
|        - | 4479 | `*/` |
|        - | 4480 | `/* OP_BOR_STORE * * *` |
|        - | 4481 | ` *` |
|        - | 4482 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4483 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 4484 | ` * two elements.` |
|        - | 4485 | ` */` |
|        - | 4486 | `/* OP_BXOR_STORE * * *` |
|        - | 4487 | ` *` |
|        - | 4488 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4489 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 4490 | ` * two elements.` |
|        - | 4491 | ` */` |
|      640 | 4492 | `case PH7_OP_BAND_STORE:` |
|        - | 4493 | `case PH7_OP_BOR_STORE:` |
|        - | 4494 | `case PH7_OP_BXOR_STORE:{` |
|     1281 | 4495 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4496 | `	ph7_value *pObj;` |
|        - | 4497 | `	sxi64 a,b,r;` |
|        - | 4498 | `	int cBwOp,bBwStr;` |
|        - | 4499 | `#ifdef UNTRUST` |
|        - | 4500 | `	if( pNos < pStack ){` |
|        - | 4501 | `		goto Abort;` |
|        - | 4502 | `	}` |
|        - | 4503 | `#endif` |
|        - | 4504 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     1281 | 4505 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     1275 | 4506 | `	cBwOp = pInstr->iOp == PH7_OP_BOR_STORE ? '\|' : (pInstr->iOp == PH7_OP_BXOR_STORE ? '^' : '&');` |
|     1275 | 4507 | `	bBwStr = (pNos->iFlags & MEMOBJ_STRING) != 0 && (pTos->iFlags & MEMOBJ_STRING) != 0;` |
|     1275 | 4508 | `	if( !bBwStr ){` |
|        - | 4509 | `		char zBwOp[2];` |
|     1173 | 4510 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 4511 | ``		/* `$x &= v` answers the same contract as `$x & v` (php's compound`` |
|        - | 4512 | `		 * assignment is the operator plus a store), but through its own error` |
|        - | 4513 | `		 * path, which is POSITIONAL: the lvalue is named first even when the` |
|        - | 4514 | ``		 * right operand is the offender (`$x = 1; $x &= $o` is "int & BsocP",`` |
|        - | 4515 | ``		 * where the plain `1 & $o` is "BsocP & int"). */`` |
|     1173 | 4516 | `		PH7_BITWISE_ARITH_CONTRACT(pTos,pNos,zBwOp,0)` |
|        - | 4517 | `		/* Force the operands to be integer (php deprecates a lossy float here) */` |
|      791 | 4518 | `		rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|      791 | 4519 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      791 | 4520 | `		rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      791 | 4521 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      791 | 4522 | `		if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      309 | 4523 | `			PH7_MemObjToInteger(pTos);` |
|      154 | 4524 | `		}` |
|      791 | 4525 | `		if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      309 | 4526 | `			PH7_MemObjToInteger(pNos);` |
|      154 | 4527 | `		}` |
|      395 | 4528 | `	}` |
|      893 | 4529 | `	if( bBwStr ){` |
|        - | 4530 | `		/* TWO strings: php's per-byte string operation, result a string. The` |
|        - | 4531 | `		 * result lands in pNos, which the store tail below writes into the` |
|        - | 4532 | `		 * lvalue's slot exactly like the integer result. */` |
|      103 | 4533 | `		PH7_STRING_BITWISE_RESULT(pNos,pTos,pNos,cBwOp)` |
|       52 | 4534 | `	}else{` |
|        - | 4535 | `	/* Perform the requested operation */` |
|      791 | 4536 | `	a = pTos->x.iVal;` |
|      791 | 4537 | `	b = pNos->x.iVal;` |
|      791 | 4538 | `	switch(pInstr->iOp){` |
|      151 | 4539 | `	case PH7_OP_BOR_STORE:` |
|      303 | 4540 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|      122 | 4541 | `	case PH7_OP_BXOR_STORE:` |
|      245 | 4542 | `	case PH7_OP_BXOR: r = a^b; break;` |
|      122 | 4543 | `	case PH7_OP_BAND_STORE:` |
|      122 | 4544 | `	case PH7_OP_BAND:` |
|      245 | 4545 | `	default:          r = a&b; break;` |
|        - | 4546 | `	}` |
|        - | 4547 | `	/* Push the result */` |
|      791 | 4548 | `	pNos->x.iVal = r;` |
|      791 | 4549 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        - | 4550 | `	}` |
|      893 | 4551 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|        - | 4552 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|      892 | 4553 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      891 | 4554 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|      891 | 4555 | `		PH7_MemObjStore(pNos,pObj);` |
|      445 | 4556 | `	}` |
|      893 | 4557 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|      893 | 4558 | `	VmPopOperand(&pTos,1);` |
|      893 | 4559 | `	break;` |
|        - | 4560 | `				 }` |
|        - | 4561 | `/* OP_SHL * * *` |
|        - | 4562 | ` *` |
|        - | 4563 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4564 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4565 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4566 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4567 | ` */` |
|        - | 4568 | `/* OP_SHR * * *` |
|        - | 4569 | ` *` |
|        - | 4570 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4571 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4572 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4573 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4574 | ` */` |
|     1680 | 4575 | `case PH7_OP_SHL:` |
|        - | 4576 | `case PH7_OP_SHR: {` |
|        - | 4577 | `	VmOpRc rcOp;` |
|     3362 | 4578 | `	sState.pTos = pTos;` |
|     3362 | 4579 | `	sState.pc = pc;` |
|     3362 | 4580 | `	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);` |
|     3362 | 4581 | `	pTos = sState.pTos;` |
|     3362 | 4582 | `	pc = sState.pc;` |
|     3362 | 4583 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4584 | `		goto Abort;` |
|     3362 | 4585 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      394 | 4586 | `		goto Exception;` |
|        - | 4587 | `	}` |
|     2970 | 4588 | `	break;` |
|        - | 4589 | `					  }` |
|        - | 4590 | `/*  OP_SHL_STORE * * *` |
|        - | 4591 | ` *` |
|        - | 4592 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4593 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4594 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4595 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4596 | ` */` |
|        - | 4597 | `/* OP_SHR_STORE * * *` |
|        - | 4598 | ` *` |
|        - | 4599 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4600 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4601 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4602 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4603 | ` */` |
|      414 | 4604 | `case PH7_OP_SHL_STORE:` |
|        - | 4605 | `case PH7_OP_SHR_STORE: {` |
|        - | 4606 | `	VmOpRc rcOp;` |
|        - | 4607 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      829 | 4608 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      825 | 4609 | `	sState.pTos = pTos;` |
|      825 | 4610 | `	sState.pc = pc;` |
|      825 | 4611 | `	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);` |
|      825 | 4612 | `	pTos = sState.pTos;` |
|      825 | 4613 | `	pc = sState.pc;` |
|      825 | 4614 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4615 | `		goto Abort;` |
|      825 | 4616 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      353 | 4617 | `		goto Exception;` |
|        - | 4618 | `	}` |
|      473 | 4619 | `	break;` |
|        - | 4620 | `					  }` |
|        - | 4621 | `/* CAT:  P1 * *` |
|        - | 4622 | ` *` |
|        - | 4623 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - | 4624 | ` * back.` |
|        - | 4625 | ` */` |
|   168815 | 4626 | `case PH7_OP_CAT:{` |
|        - | 4627 | `	ph7_value *pNos,*pCur;` |
|   337514 | 4628 | `	if( pInstr->iP1 < 1 ){` |
|   284851 | 4629 | `		pNos = &pTos[-1];` |
|   142442 | 4630 | `	}else{` |
|    52668 | 4631 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - | 4632 | `	}` |
|        - | 4633 | `#ifdef UNTRUST` |
|        - | 4634 | `	if( pNos < pStack ){` |
|        - | 4635 | `		goto Abort;` |
|        - | 4636 | `	}` |
|        - | 4637 | `#endif` |
|        - | 4638 | `	/* Force a string cast (user-visible: warns on an array operand, §2).` |
|        - | 4639 | `	 * php coerces the operands left to right, so the leftmost not-stringable` |
|        - | 4640 | `	 * object is the one that throws. */` |
|        - | 4641 | `	{` |
|   337514 | 4642 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|   337514 | 4643 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4644 | `	}` |
|   337512 | 4645 | `	pCur = &pNos[1];` |
|        - | 4646 | `	{` |
|        - | 4647 | `		/* rcSv leaves the loop with the coercion status: the routing macro expands` |
|        - | 4648 | ``		 * to a bare `break` here, which inside the while would fall THROUGH to the`` |
|        - | 4649 | ``		 * `pTos = pNos` below with a throw pending. Route it at case level. */`` |
|   337512 | 4650 | `		sxi32 rcSv = SXRET_OK;` |
|   744483 | 4651 | `		while( pCur <= pTos ){` |
|   407420 | 4652 | `			rcSv = PH7_MemObjToStringUV(pCur);` |
|   407420 | 4653 | `			if( rcSv != SXRET_OK ){` |
|      448 | 4654 | `				break;` |
|        - | 4655 | `			}` |
|        - | 4656 | `			/* Perform the concatenation */` |
|   406976 | 4657 | `			if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   405700 | 4658 | `				if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - | 4659 | `					/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 | 4660 | `					PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4661 | `					goto Abort;` |
|        - | 4662 | `				}` |
|   202646 | 4663 | `			}` |
|   406976 | 4664 | `			SyBlobRelease(&pCur->sBlob);` |
|   406976 | 4665 | `			pCur++;` |
|        5 | 4666 | `		}` |
|   338328 | 4667 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4668 | `	}` |
|   337068 | 4669 | `	pTos = pNos;` |
|   337068 | 4670 | `	break;` |
|        - | 4671 | `				}` |
|        - | 4672 | `/*  CAT_STORE: * * *` |
|        - | 4673 | ` *` |
|        - | 4674 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - | 4675 | ` * back.` |
|        - | 4676 | ` */` |
|    36292 | 4677 | `case PH7_OP_CAT_STORE:{` |
|    78280 | 4678 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4679 | `	ph7_value *pObj;` |
|        - | 4680 | `	sxu32 nIdx;` |
|        - | 4681 | `#ifdef UNTRUST` |
|        - | 4682 | `	if( pNos < pStack ){` |
|        - | 4683 | `		goto Abort;` |
|        - | 4684 | `	}` |
|        - | 4685 | `#endif` |
|        - | 4686 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|    78280 | 4687 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 4688 | `	/* The right operand must be a string to append it (user-visible, §2) */` |
|        - | 4689 | `	{` |
|    78272 | 4690 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|    78276 | 4691 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4692 | `	}` |
|    78262 | 4693 | `	nIdx = pTos->nIdx;` |
|        - | 4694 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - | 4695 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - | 4696 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - | 4697 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - | 4698 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - | 4699 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - | 4700 | `	 * the source we copy from — references share the slot index, so one check` |
|        - | 4701 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - | 4702 | `	 * must run before any mutation (left to the slow path).` |
|        - | 4703 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - | 4704 | `	 * and remains O(n^2) by design. */` |
|    78257 | 4705 | `	if( nIdx != SXU32_HIGH` |
|    78255 | 4706 | `	 && nIdx != pNos->nIdx` |
|    71739 | 4707 | `	 && (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx)) != 0` |
|    71740 | 4708 | `	 && !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|        - | 4709 | `		/* e.g. $x = 5; $x .= "a";  ->  "5a" (user-visible: warns if $x is an array,` |
|        - | 4710 | `		 * throws if it is a not-stringable object — and then the lvalue keeps` |
|        - | 4711 | `		 * holding that object, since the throw abandons the coercion) */` |
|        - | 4712 | `		{` |
|    71728 | 4713 | `			sxi32 rcSv = PH7_MemObjToStringUV(pObj);` |
|    71736 | 4714 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4715 | `		}` |
|    71720 | 4716 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|    71520 | 4717 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4718 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - | 4719 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 | 4720 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4721 | `				goto Abort;` |
|        - | 4722 | `			}` |
|    35348 | 4723 | `		}` |
|        - | 4724 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - | 4725 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - | 4726 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - | 4727 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - | 4728 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - | 4729 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - | 4730 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - | 4731 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - | 4732 | `		 * the same slot is appended to again later in the statement` |
|        - | 4733 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - | 4734 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - | 4735 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|    71720 | 4736 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 | 4737 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 | 4738 | `		}` |
|        - | 4739 | `		/* A hooked lvalue's scratch slot was appended in place — dispatch the` |
|        - | 4740 | `		 * set side now (the consume reads the computed value from the slot). */` |
|    71720 | 4741 | `		PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|    71720 | 4742 | `		pNos->nIdx = SXU32_HIGH;` |
|    71720 | 4743 | `		VmPopOperand(&pTos,1);` |
|    71720 | 4744 | `		break;` |
|        - | 4745 | `	}` |
|        - | 4746 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|        - | 4747 | `	/* Force a string cast (user-visible: warns if the lvalue is an array, §2) */` |
|        - | 4748 | `	{` |
|    13050 | 4749 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|    13050 | 4750 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4751 | `	}` |
|        - | 4752 | `	/* Perform the concatenation (Reverse order) */` |
|       26 | 4753 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       26 | 4754 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4755 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - | 4756 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 | 4757 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4758 | `			goto Abort;` |
|        - | 4759 | `		}` |
|       12 | 4760 | `	}` |
|        - | 4761 | `	/* Perform the store operation */` |
|       26 | 4762 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|        - | 4763 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|       24 | 4764 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       26 | 4765 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       17 | 4766 | `		PH7_MemObjStore(pTos,pObj);` |
|        8 | 4767 | `	}` |
|       21 | 4768 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       21 | 4769 | `	PH7_MemObjStore(pTos,pNos);` |
|       21 | 4770 | `	VmPopOperand(&pTos,1);` |
|       21 | 4771 | `	break;` |
|        - | 4772 | `				}` |
|        - | 4773 | `/* OP_AND: * * *` |
|        - | 4774 | ` *` |
|        - | 4775 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - | 4776 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4777 | ` * stack.` |
|        - | 4778 | ` */` |
|        - | 4779 | `/* OP_OR: * * *` |
|        - | 4780 | ` *` |
|        - | 4781 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - | 4782 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4783 | ` * stack.` |
|        - | 4784 | ` */` |
|   196912 | 4785 | `case PH7_OP_LAND:` |
|        - | 4786 | `case PH7_OP_LOR: {` |
|        - | 4787 | `	VmOpRc rcOp;` |
|   394433 | 4788 | `	sState.pTos = pTos;` |
|   394433 | 4789 | `	sState.pc = pc;` |
|   394433 | 4790 | `	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);` |
|   394433 | 4791 | `	pTos = sState.pTos;` |
|   394433 | 4792 | `	pc = sState.pc;` |
|   394433 | 4793 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4794 | `		goto Abort;` |
|   394433 | 4795 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4796 | `		goto Exception;` |
|        - | 4797 | `	}` |
|   394433 | 4798 | `	break;` |
|        - | 4799 | `					  }` |
|        - | 4800 | `/*` |
|        - | 4801 | ` * OP_NULLC: * * *` |
|        - | 4802 | ` * Null coalescing operator '??'.` |
|        - | 4803 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - | 4804 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - | 4805 | ` */` |
|        - | 4806 | `/*` |
|        - | 4807 | ` * OP_NULLC: * P2 *` |
|        - | 4808 | ` * Short-circuit null coalescing '??'.` |
|        - | 4809 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - | 4810 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - | 4811 | ` */` |
|     1789 | 4812 | `case PH7_OP_NULLC: {` |
|        - | 4813 | `#ifdef UNTRUST` |
|        - | 4814 | `	if( pTos < pStack ){` |
|        - | 4815 | `		goto Abort;` |
|        - | 4816 | `	}` |
|        - | 4817 | `#endif` |
|     3583 | 4818 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 4819 | `		/* Left is not null — keep it and skip the RHS */` |
|     2017 | 4820 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|     1011 | 4821 | `	}else{` |
|        - | 4822 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|     1571 | 4823 | `		VmPopOperand(&pTos, 1);` |
|        - | 4824 | `	}` |
|     3583 | 4825 | `	break;` |
|        - | 4826 | `}` |
|        - | 4827 | `/*` |
|        - | 4828 | ` * OP_NULLC_JMP: * P2 *` |
|        - | 4829 | ` * Null coalescing assignment short-circuit.` |
|        - | 4830 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - | 4831 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - | 4832 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - | 4833 | ` */` |
|       89 | 4834 | `case PH7_OP_NULLC_JMP: {` |
|        - | 4835 | `#ifdef UNTRUST` |
|        - | 4836 | `	if( pTos < pStack ){` |
|        - | 4837 | `		goto Abort;` |
|        - | 4838 | `	}` |
|        - | 4839 | `#endif` |
|      181 | 4840 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       56 | 4841 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —` |
|        - | 4842 | `		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is` |
|        - | 4843 | `		 * dropped by the fetch-point sweep (the landing pc is past its` |
|        - | 4844 | `		 * OP_NULLC_STORE window): the skipped assign never dispatches. */` |
|       27 | 4845 | `	}` |
|      181 | 4846 | `	break;` |
|        - | 4847 | `}` |
|        - | 4848 | `/*` |
|        - | 4849 | ` * OP_NULLC_STORE: * * *` |
|        - | 4850 | ` * Null coalescing assignment store.` |
|        - | 4851 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - | 4852 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - | 4853 | ` * expression result.` |
|        - | 4854 | ` */` |
|        - | 4855 | `/*` |
|        - | 4856 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - | 4857 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - | 4858 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - | 4859 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - | 4860 | ` * non-null, fall through without modifying the stack so the following` |
|        - | 4861 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - | 4862 | ` */` |
|      106 | 4863 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - | 4864 | `#ifdef UNTRUST` |
|        - | 4865 | `	if( pTos < pStack ){` |
|        - | 4866 | `		goto Abort;` |
|        - | 4867 | `	}` |
|        - | 4868 | `#endif` |
|      216 | 4869 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - | 4870 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - | 4871 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       76 | 4872 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       37 | 4873 | `	}` |
|      216 | 4874 | `	break;` |
|        - | 4875 | `}` |
|       59 | 4876 | `case PH7_OP_NULLC_STORE: {` |
|        - | 4877 | `	VmOpRc rcOp;` |
|      121 | 4878 | `	sState.pTos = pTos;` |
|      121 | 4879 | `	sState.pc = pc;` |
|      121 | 4880 | `	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);` |
|      121 | 4881 | `	pTos = sState.pTos;` |
|      121 | 4882 | `	pc = sState.pc;` |
|      121 | 4883 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4884 | `		goto Abort;` |
|      121 | 4885 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       11 | 4886 | `		goto Exception;` |
|        - | 4887 | `	}` |
|      111 | 4888 | `	break;` |
|        - | 4889 | `					  }` |
|        - | 4890 | `/*` |
|        - | 4891 | ` * OP_SPREAD: * * *` |
|        - | 4892 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - | 4893 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - | 4894 | ` * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this` |
|        - | 4895 | ` * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —` |
|        - | 4896 | ` * the CALL may not be the next instruction, and may be an inner call whose own` |
|        - | 4897 | ` * spreads must stay scoped to it.` |
|        - | 4898 | ` * The expansion tail is shared between the plain-array and the materialized` |
|        - | 4899 | ` * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.` |
|        - | 4900 | ` */` |
|      744 | 4901 | `case PH7_OP_SPREAD: {` |
|        - | 4902 | `#ifdef UNTRUST` |
|        - | 4903 | `	if( pTos < pStack ){` |
|        - | 4904 | `		goto Abort;` |
|        - | 4905 | `	}` |
|        - | 4906 | `#endif` |
|        - | 4907 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - | 4908 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - | 4909 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - | 4910 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - | 4911 | `	 * be freed immediately. */` |
|     1398 | 4912 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|       55 | 4913 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - | 4914 | `		sxi32 rcW;` |
|       55 | 4915 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|       55 | 4916 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|       55 | 4917 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|       30 | 4918 | `			sxi32 rcRoute = SXRET_OK; /* the throw already happened; route, do not re-raise */` |
|       30 | 4919 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|       30 | 4920 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|        - | 4921 | ``			/* A bare `goto Exception` unwinds the whole invocation, which is right at a`` |
|        - | 4922 | `			 * CALL boundary and wrong here: an argument list is mid-expression, so a` |
|        - | 4923 | `			 * try/catch around the call catches this and execution must RESUME after` |
|        - | 4924 | `			 * the catch. It did not — the catch ran and every statement after it was` |
|        - | 4925 | ``			 * dropped, exit 0 (a throwing rewind()/key() in `f(...$it)` showed it long`` |
|        - | 4926 | `			 * before the key screen made the path ordinary). */` |
|       30 | 4927 | `			PH7_THROW_ROUTE_MIDEXPR(rcRoute)` |
|        - | 4928 | `		}` |
|        - | 4929 | `		/* Grow the operand stack if this expansion would overflow it (no longer a` |
|        - | 4930 | `		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a` |
|        - | 4931 | `		 * single argument — the historical bounded-fallback, now only under OOM. */` |
|       39 | 4932 | `		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,` |
|       12 | 4933 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 4934 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 4935 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 4936 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 4937 | `				pTmpMap->nEntry);` |
|      ! 0 | 4938 | `			break;` |
|        - | 4939 | `		}` |
|        - | 4940 | `		/* The buffer may have MOVED (and grown): re-anchor the watermark at the whole` |
|        - | 4941 | `		 * new capacity rather than carry a pointer into the freed one. Conservative --` |
|        - | 4942 | `		 * this activation's teardown then sweeps everything -- and OP_SPREAD is rare. */` |
|       27 | 4943 | `		pHigh = pStack + sState.nStackCap - 1;` |
|       27 | 4944 | `		VmSpreadExpandMap(pVm, &pTos, pTmpMap, 0/*a Traversable's values are not the caller's slots*/);` |
|       27 | 4945 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|       27 | 4946 | `		break;` |
|        - | 4947 | `	}` |
|     1346 | 4948 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     1286 | 4949 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|     1879 | 4950 | `		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,` |
|      593 | 4951 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 4952 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 4953 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 4954 | `				pMap->nEntry);` |
|      ! 0 | 4955 | `			break;` |
|        - | 4956 | `		}` |
|     1286 | 4957 | `		pHigh = pStack + sState.nStackCap - 1;   /* see the Traversable arm above */` |
|     1286 | 4958 | `		VmSpreadExpandMap(pVm, &pTos, pMap, pInstr->iP1 != 0);` |
|     1286 | 4959 | `		break;` |
|        - | 4960 | `	}` |
|        - | 4961 | `	/* Neither an array nor a Traversable: php refuses the unpack rather than` |
|        - | 4962 | `	 * passing the value as one ordinary argument, which is what this used to do —` |
|        - | 4963 | ``	 * `f(...'str')` bound "str" to the first parameter and `new C(...null)` bound`` |
|        - | 4964 | `	 * null, silently, on source php will not run. The argument site's class is` |
|        - | 4965 | `	 * TypeError for every type (the array-literal site keeps php's plain Error for` |
|        - | 4966 | `	 * a scalar). */` |
|        - | 4967 | `	{` |
|       62 | 4968 | `		sxi32 rcBad = VmThrowSpreadError(&(*pVm),pTos,1);` |
|       62 | 4969 | `		sxi32 rcRoute = SXRET_OK;` |
|       62 | 4970 | `		if( rcBad == PH7_ABORT \|\| rcBad == SXERR_ABORT ){` |
|      ! 0 | 4971 | `			goto Abort;` |
|        - | 4972 | `		}` |
|       62 | 4973 | `		PH7_THROW_ROUTE_MIDEXPR(rcRoute)` |
|        - | 4974 | `	}` |
|        - | 4975 | `}` |
|        - | 4976 | `/*` |
|        - | 4977 | ` * OP_FLAG_SPREAD: * * *` |
|        - | 4978 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - | 4979 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - | 4980 | ` */` |
|      362 | 4981 | `case PH7_OP_FLAG_SPREAD: {` |
|        - | 4982 | `#ifdef UNTRUST` |
|        - | 4983 | `	if( pTos < pStack ){` |
|        - | 4984 | `		goto Abort;` |
|        - | 4985 | `	}` |
|        - | 4986 | `#endif` |
|      728 | 4987 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|      728 | 4988 | `	break;` |
|        - | 4989 | `}` |
|        - | 4990 | `/* OP_LXOR: * * *` |
|        - | 4991 | ` *` |
|        - | 4992 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - | 4993 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4994 | ` * stack.` |
|        - | 4995 | ` * According to the PHP language reference manual:` |
|        - | 4996 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - | 4997 | ` *  TRUE,but not both.` |
|        - | 4998 | ` */` |
|        6 | 4999 | `case PH7_OP_LXOR:{` |
|       13 | 5000 | `	ph7_value *pNos = &pTos[-1];` |
|       13 | 5001 | `	sxi32 v = 0;` |
|        - | 5002 | `#ifdef UNTRUST` |
|        - | 5003 | `	if( pNos < pStack ){` |
|        - | 5004 | `		goto Abort;` |
|        - | 5005 | `	}` |
|        - | 5006 | `#endif` |
|        - | 5007 | `	/* Force a boolean cast */` |
|       13 | 5008 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 5009 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 5010 | `	}` |
|       13 | 5011 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 5012 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 | 5013 | `	}` |
|       13 | 5014 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 | 5015 | `		v = 1;` |
|        3 | 5016 | `	}` |
|       13 | 5017 | `	VmPopOperand(&pTos,1);` |
|       13 | 5018 | `	pTos->x.iVal = v;` |
|       13 | 5019 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       13 | 5020 | `	break;` |
|        - | 5021 | `				 }` |
|        - | 5022 | `/* OP_EQ P1 P2 P3` |
|        - | 5023 | ` *` |
|        - | 5024 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - | 5025 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - | 5026 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5027 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5028 | ` */` |
|        - | 5029 | `/* OP_NEQ P1 P2 P3` |
|        - | 5030 | ` *` |
|        - | 5031 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - | 5032 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 5033 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5034 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5035 | ` */` |
|     8396 | 5036 | `case PH7_OP_EQ:` |
|        - | 5037 | `case PH7_OP_NEQ: {` |
|        - | 5038 | `	VmOpRc rcOp;` |
|    16794 | 5039 | `	sState.pTos = pTos;` |
|    16794 | 5040 | `	sState.pc = pc;` |
|    16794 | 5041 | `	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);` |
|    16794 | 5042 | `	pTos = sState.pTos;` |
|    16794 | 5043 | `	pc = sState.pc;` |
|    16794 | 5044 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5045 | `		goto Abort;` |
|    16794 | 5046 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       17 | 5047 | `		goto Exception;` |
|        - | 5048 | `	}` |
|    16778 | 5049 | `	break;` |
|        - | 5050 | `					  }` |
|        - | 5051 | `/* OP_TEQ P1 P2 *` |
|        - | 5052 | ` *` |
|        - | 5053 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - | 5054 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 5055 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5056 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5057 | ` */` |
|   466880 | 5058 | `case PH7_OP_TEQ: {` |
|        - | 5059 | `	VmOpRc rcOp;` |
|   935121 | 5060 | `	sState.pTos = pTos;` |
|   935121 | 5061 | `	sState.pc = pc;` |
|   935121 | 5062 | `	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);` |
|   935121 | 5063 | `	pTos = sState.pTos;` |
|   935121 | 5064 | `	pc = sState.pc;` |
|   935121 | 5065 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5066 | `		goto Abort;` |
|   935121 | 5067 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5068 | `		goto Exception;` |
|        - | 5069 | `	}` |
|   935121 | 5070 | `	break;` |
|        - | 5071 | `					  }` |
|        - | 5072 | `/* OP_TNE P1 P2 *` |
|        - | 5073 | ` *` |
|        - | 5074 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - | 5075 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - | 5076 | ` * instruction.` |
|        - | 5077 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5078 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5079 | ` *` |
|        - | 5080 | ` */` |
|   606040 | 5081 | `case PH7_OP_TNE: {` |
|        - | 5082 | `	VmOpRc rcOp;` |
|  1212676 | 5083 | `	sState.pTos = pTos;` |
|  1212676 | 5084 | `	sState.pc = pc;` |
|  1212676 | 5085 | `	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);` |
|  1212676 | 5086 | `	pTos = sState.pTos;` |
|  1212676 | 5087 | `	pc = sState.pc;` |
|  1212676 | 5088 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5089 | `		goto Abort;` |
|  1212676 | 5090 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5091 | `		goto Exception;` |
|        - | 5092 | `	}` |
|  1212676 | 5093 | `	break;` |
|        - | 5094 | `					  }` |
|        - | 5095 | `/* OP_LT P1 P2 P3` |
|        - | 5096 | ` *` |
|        - | 5097 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 5098 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 5099 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 5100 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5101 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5102 | ` *` |
|        - | 5103 | ` */` |
|        - | 5104 | `/* OP_LE P1 P2 P3` |
|        - | 5105 | ` *` |
|        - | 5106 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 5107 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 5108 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 5109 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5110 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5111 | ` *` |
|        - | 5112 | ` */` |
|   546102 | 5113 | `case PH7_OP_LT:` |
|        - | 5114 | `case PH7_OP_LE: {` |
|        - | 5115 | `	VmOpRc rcOp;` |
|  1094689 | 5116 | `	sState.pTos = pTos;` |
|  1094689 | 5117 | `	sState.pc = pc;` |
|  1094689 | 5118 | `	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);` |
|  1094689 | 5119 | `	pTos = sState.pTos;` |
|  1094689 | 5120 | `	pc = sState.pc;` |
|  1094689 | 5121 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5122 | `		goto Abort;` |
|  1094689 | 5123 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 5124 | `		goto Exception;` |
|        - | 5125 | `	}` |
|  1094685 | 5126 | `	break;` |
|        - | 5127 | `					  }` |
|        - | 5128 | `/* OP_GT P1 P2 P3` |
|        - | 5129 | ` *` |
|        - | 5130 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 5131 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 5132 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 5133 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5134 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5135 | ` *` |
|        - | 5136 | ` */` |
|        - | 5137 | `/* OP_GE P1 P2 P3` |
|        - | 5138 | ` *` |
|        - | 5139 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 5140 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 5141 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 5142 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5143 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5144 | ` *` |
|        - | 5145 | ` */` |
|   155149 | 5146 | `case PH7_OP_GT:` |
|        - | 5147 | `case PH7_OP_GE: {` |
|        - | 5148 | `	VmOpRc rcOp;` |
|   310948 | 5149 | `	sState.pTos = pTos;` |
|   310948 | 5150 | `	sState.pc = pc;` |
|   310948 | 5151 | `	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);` |
|   310948 | 5152 | `	pTos = sState.pTos;` |
|   310948 | 5153 | `	pc = sState.pc;` |
|   310948 | 5154 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5155 | `		goto Abort;` |
|   310948 | 5156 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5157 | `		goto Exception;` |
|        - | 5158 | `	}` |
|   310948 | 5159 | `	break;` |
|        - | 5160 | `					  }` |
|        - | 5161 | `/* OP_SPACESHIP * * *` |
|        - | 5162 | ` *` |
|        - | 5163 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - | 5164 | ` *   -1 if left < right` |
|        - | 5165 | ` *    0 if left == right` |
|        - | 5166 | ` *    1 if left > right` |
|        - | 5167 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - | 5168 | ` */` |
|      393 | 5169 | `case PH7_OP_SPACESHIP: {` |
|        - | 5170 | `	VmOpRc rcOp;` |
|      790 | 5171 | `	sState.pTos = pTos;` |
|      790 | 5172 | `	sState.pc = pc;` |
|      790 | 5173 | `	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);` |
|      790 | 5174 | `	pTos = sState.pTos;` |
|      790 | 5175 | `	pc = sState.pc;` |
|      790 | 5176 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5177 | `		goto Abort;` |
|      790 | 5178 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 5179 | `		goto Exception;` |
|        - | 5180 | `	}` |
|      784 | 5181 | `	break;` |
|        - | 5182 | `					  }` |
|        - | 5183 | `/*` |
|        - | 5184 | ` * OP_LOAD_REF * * *` |
|        - | 5185 | ` * Push the index of a referenced object on the stack.` |
|        - | 5186 | ` */` |
|       83 | 5187 | `case PH7_OP_LOAD_REF: {` |
|        - | 5188 | `	sxu32 nIdx;` |
|        - | 5189 | `#ifdef UNTRUST` |
|        - | 5190 | `	if( pTos < pStack ){` |
|        - | 5191 | `		goto Abort;` |
|        - | 5192 | `	}` |
|        - | 5193 | `#endif` |
|      168 | 5194 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|        - | 5195 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|        - | 5196 | `		 * out of a string still carries the BASE VARIABLE's slot index, so taking a` |
|        - | 5197 | `` 		 * reference to it aliased the WHOLE STRING — `$x = [&$s[1]]; $x[0] = "Z";` `` |
|        - | 5198 | ``		 * left $s === "Z". Same Error the `=&` and by-ref-argument paths raise. */`` |
|        3 | 5199 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|        - | 5200 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|        3 | 5201 | `		PH7_MemObjRelease(pTos);` |
|        3 | 5202 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 5203 | `		pTos->nIdx = SXU32_HIGH;` |
|        3 | 5204 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|        3 | 5205 | `		break;` |
|        - | 5206 | `	}` |
|        - | 5207 | `	/* Extract memory object index */` |
|      165 | 5208 | `	nIdx = pTos->nIdx;` |
|      165 | 5209 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - | 5210 | `		/* Nullify the object */` |
|      165 | 5211 | `		PH7_MemObjRelease(pTos);` |
|        - | 5212 | `		/* Mark as constant and store the index on the top of the stack */` |
|      165 | 5213 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      165 | 5214 | `		pTos->nIdx = SXU32_HIGH;` |
|      165 | 5215 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       82 | 5216 | `	}` |
|      165 | 5217 | `	break;` |
|        - | 5218 | `					  }` |
|        - | 5219 | `/*` |
|        - | 5220 | ` * OP_STORE_REF * * P3` |
|        - | 5221 | ` * Perform an assignment operation by reference.` |
|        - | 5222 | ` */` |
|     1713 | 5223 | `case PH7_OP_STORE_REF: {` |
|        - | 5224 | `	VmOpRc rcOp;` |
|     3431 | 5225 | `	sState.pTos = pTos;` |
|     3431 | 5226 | `	sState.pc = pc;` |
|     3431 | 5227 | `	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);` |
|     3431 | 5228 | `	pTos = sState.pTos;` |
|     3431 | 5229 | `	pc = sState.pc;` |
|     3431 | 5230 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 5231 | `		goto Abort;` |
|     3428 | 5232 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       22 | 5233 | `		goto Exception;` |
|        - | 5234 | `	}` |
|     3408 | 5235 | `	break;` |
|        - | 5236 | `					  }` |
|        - | 5237 | `/*` |
|        - | 5238 | ` * OP_UPLINK P1 * *` |
|        - | 5239 | ` * Link a variable to the top active VM frame.` |
|        - | 5240 | ` * This is used to implement the 'global' PHP construct.` |
|        - | 5241 | ` */` |
|      635 | 5242 | `case PH7_OP_UPLINK: {` |
|     1271 | 5243 | `	if( pVm->pFrame->pParent ){` |
|     1271 | 5244 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - | 5245 | `		SyString sName;` |
|        - | 5246 | `		/* rcSv carries the coercion status OUT of the loop: the routing macro is a` |
|        - | 5247 | ``		 * bare `break` here, which would only leave the while and then pop the`` |
|        - | 5248 | `		 * operands with a throw pending. */` |
|     1271 | 5249 | `		sxi32 rcSv = SXRET_OK;` |
|        - | 5250 | `		/* Perform the link */` |
|     3187 | 5251 | `		while( pLink <= pTos ){` |
|        - | 5252 | `			/* Force a string cast — global $$arr link name (user-visible, §2) */` |
|     1927 | 5253 | `			rcSv = PH7_MemObjToStringUV(pLink);` |
|     1927 | 5254 | `			if( rcSv != SXRET_OK ){` |
|        7 | 5255 | `				break;` |
|        - | 5256 | `			}` |
|     1921 | 5257 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|     1921 | 5258 | `			if( sName.nByte > 0 ){` |
|     1921 | 5259 | `				VmFrameLink(&(*pVm),&sName);` |
|      956 | 5260 | `			}` |
|     1921 | 5261 | `			pLink++;` |
|        5 | 5262 | `		}` |
|     1271 | 5263 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|      628 | 5264 | `	}` |
|     1265 | 5265 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|     1265 | 5266 | `	break;` |
|        - | 5267 | `					}` |
|        - | 5268 | `/*` |
|        - | 5269 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - | 5270 | ` * Push an exception in the corresponding container so that` |
|        - | 5271 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - | 5272 | ` */` |
|   740611 | 5273 | `case PH7_OP_LOAD_EXCEPTION: {` |
|        - | 5274 | `	VmOpRc rcOp;` |
|  1481009 | 5275 | `	sState.pTos = pTos;` |
|  1481009 | 5276 | `	sState.pc = pc;` |
|  1481009 | 5277 | `	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);` |
|  1481009 | 5278 | `	pTos = sState.pTos;` |
|  1481009 | 5279 | `	pc = sState.pc;` |
|  1481009 | 5280 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5281 | `		goto Abort;` |
|  1481009 | 5282 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5283 | `		goto Exception;` |
|        - | 5284 | `	}` |
|  1481009 | 5285 | `	break;` |
|        - | 5286 | `					  }` |
|        - | 5287 | `/*` |
|        - | 5288 | ` * OP_POP_EXCEPTION * * P3` |
|        - | 5289 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - | 5290 | ` */` |
|   687267 | 5291 | `case PH7_OP_POP_EXCEPTION: {` |
|  1374321 | 5292 | `	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;` |
|        - | 5293 | `	VmFrame *pBodyFrame;` |
|        - | 5294 | `	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on` |
|        - | 5295 | `	 * top (matched by compiled origin). pException == NULL means this try's` |
|        - | 5296 | `	 * activation was already consumed (an in-place catch handled a throw and` |
|        - | 5297 | `	 * ran the finally itself); compiled fields keep coming from p3. */` |
|  1374321 | 5298 | `	ph7_exception *pException = 0;` |
|  1374321 | 5299 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|     9936 | 5300 | `		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|     9936 | 5301 | `		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];` |
|        - | 5302 | `		/* Same compiled origin is NOT enough: under recursion, when THIS` |
|        - | 5303 | `		 * level's activation was already consumed by an in-place catch (the` |
|        - | 5304 | `		 * resume lands right here), the top can be an OUTER level's activation` |
|        - | 5305 | `		 * of the same lexical try — popping it would run that level's finally` |
|        - | 5306 | `		 * early and orphan its handler (probed: recursive try/catch/finally` |
|        - | 5307 | `		 * lost the outer catch entirely). The activation must also belong to` |
|        - | 5308 | `		 * the CURRENT body frame. */` |
|     9931 | 5309 | `		if( VmExcMatches(pTop,pCompiledExc)` |
|     9821 | 5310 | `		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){` |
|     9700 | 5311 | `			pException = pTop;` |
|     9700 | 5312 | `			(void)SySetPop(&pVm->aException);` |
|     4766 | 5313 | `		}` |
|     4884 | 5314 | `	}` |
|  1374321 | 5315 | `	if( pCompiledExc->iInlined ){` |
|        - | 5316 | `		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.` |
|        - | 5317 | `		 * Pop this try's handler off aException so a throw in the finally propagates to` |
|        - | 5318 | `		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the` |
|        - | 5319 | `		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the` |
|        - | 5320 | `		 * compiler-emitted JMP falls into the finally. Without a finally, this is the` |
|        - | 5321 | `		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */` |
|      169 | 5322 | `		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */` |
|      169 | 5323 | `		if( pCompiledExc->iHasFinally ){` |
|        - | 5324 | `			VmFinallyAction sAct;` |
|       15 | 5325 | `			SyZero(&sAct,sizeof(sAct));` |
|       15 | 5326 | `			sAct.eKind = PH7_FA_FALLTHROUGH;` |
|       15 | 5327 | `			sAct.iNextPc = pCompiledExc->iEndCatchPc;` |
|       15 | 5328 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        - | 5329 | `			/* keep the transparent frame for OP_END_FINALLY */` |
|      163 | 5330 | `		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       22 | 5331 | `			VmLeaveFrame(&(*pVm));` |
|        9 | 5332 | `		}` |
|      169 | 5333 | `		break;` |
|        - | 5334 | `	}` |
|        - | 5335 | `	/* Leave the exception frame. It is normally on top here (a try that fell through` |
|        - | 5336 | `	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a` |
|        - | 5337 | `	 * RESUMED generator/fiber body whose yield was inside this try, that exception` |
|        - | 5338 | `	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),` |
|        - | 5339 | `	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the` |
|        - | 5340 | `	 * coroutine (and defeat the bHasRet materialization just below, which must see the` |
|        - | 5341 | `	 * body). Only leave a genuine exception frame. */` |
|  1374157 | 5342 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|  1064038 | 5343 | `		VmLeaveFrame(&(*pVm));` |
|   531933 | 5344 | `	}` |
|        - | 5345 | `	/* Execute the finally block if present and not already executed by the` |
|        - | 5346 | `	 * catch path. No live activation (pException == NULL) means an in-place` |
|        - | 5347 | `	 * catch consumed it — and that path runs the finally itself — so skip. */` |
|  1374157 | 5348 | `	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){` |
|        - | 5349 | `		sxi32 rcFinally;` |
|       69 | 5350 | `		VmExcRelease(&(*pVm),pException);` |
|       69 | 5351 | `		pException = 0;` |
|       69 | 5352 | `		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);` |
|       69 | 5353 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 | 5354 | `			goto Abort;` |
|        - | 5355 | `		}` |
|       69 | 5356 | `		if( rcFinally == PH7_EXCEPTION ){` |
|        - | 5357 | `			/* The finally threw past itself. If an enclosing try IN THIS function` |
|        - | 5358 | `			 * caught the new exception in place, resume at its landing pad;` |
|        - | 5359 | `			 * otherwise it was caught at an outer frame, so unwind this function. */` |
|        - | 5360 | `			sxi32 iResumePc;` |
|        5 | 5361 | `			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 5362 | `				pc = iResumePc;` |
|        3 | 5363 | `				break;` |
|        - | 5364 | `			}` |
|        3 | 5365 | `			goto Exception;` |
|        - | 5366 | `		}` |
|       30 | 5367 | `	}` |
|  1374153 | 5368 | `	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */` |
|  1374153 | 5369 | `	pBodyFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  1374153 | 5370 | `	if( pBodyFrame->bHasRet ){` |
|        - | 5371 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - | 5372 | `		 * function. The return targets the body frame this try belongs to. Drain` |
|        - | 5373 | `		 * outer finally blocks first, then — only in the real function body` |
|        - | 5374 | `		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline` |
|        - | 5375 | `		 * try within a catch/finally) propagate outward so the owning body returns. */` |
|    23988 | 5376 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    23988 | 5377 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 5378 | `			goto Abort;` |
|        - | 5379 | `		}` |
|    23988 | 5380 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 5381 | `			goto Exception;` |
|        - | 5382 | `		}` |
|    23988 | 5383 | `		if( !sState.bReturnPropagates ){` |
|    23982 | 5384 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|    11989 | 5385 | `		}` |
|    23988 | 5386 | `		goto Done;` |
|        - | 5387 | `	}` |
|  1350169 | 5388 | `	if( pBodyFrame->nCatchJmpPc > 0 ){` |
|        - | 5389 | ``		/* A `break`/`continue` inside the catch (OP_CATCH_JMP) parked its loop-exit`` |
|        - | 5390 | `		 * target on this body frame, and this is a landing pad on its way out. */` |
|       88 | 5391 | `		if( pBodyFrame->nCatchJmpLevels > 1 ){` |
|        - | 5392 | `			/* Still one or more detached bodies out from the target's array — this try` |
|        - | 5393 | `			 * was declared INSIDE another catch/finally mini-program. End this one and` |
|        - | 5394 | `			 * let the park travel outward; the next landing pad decrements again. */` |
|        5 | 5395 | `			pBodyFrame->nCatchJmpLevels--;` |
|        5 | 5396 | `			goto Done;` |
|        - | 5397 | `		}` |
|       84 | 5398 | `		if( pBodyFrame->nCatchJmpCross > 0 ){` |
|        - | 5399 | `			/* Enclosing trys between the catch and the loop: the jump lands past their` |
|        - | 5400 | `			 * OP_POP_EXCEPTION, so run their finally (and leave their frames) here. */` |
|        7 | 5401 | `			sxu32 nCross = pBodyFrame->nCatchJmpCross;` |
|        7 | 5402 | `			pBodyFrame->nCatchJmpCross = 0;` |
|        7 | 5403 | `			rc = VmDrainCrossedTrys(&(*pVm),nCross,sState.nExceptionBase);` |
|        7 | 5404 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5405 | `				goto Abort;` |
|        - | 5406 | `			}` |
|        7 | 5407 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 5408 | `				/* A drained finally threw past itself — it supersedes the loop exit. */` |
|      ! 0 | 5409 | `				pBodyFrame->nCatchJmpPc = 0;` |
|      ! 0 | 5410 | `				goto Exception;` |
|        - | 5411 | `			}` |
|        3 | 5412 | `		}` |
|       84 | 5413 | `		pc = (sxi32)pBodyFrame->nCatchJmpPc - 1;` |
|       84 | 5414 | `		pBodyFrame->nCatchJmpPc = 0;` |
|       84 | 5415 | `		break;` |
|        - | 5416 | `	}` |
|  1350085 | 5417 | `	break;` |
|        - | 5418 | `							}` |
|        - | 5419 | `/*` |
|        - | 5420 | ` * OP_CATCH_JMP P1(levels,cross) P2(target pc) *` |
|        - | 5421 | ` * Jump to iP2, leaving the try/catch structures iP1 describes (PH7_CATCH_JMP_P1).` |
|        - | 5422 | ` *` |
|        - | 5423 | ` * CROSS enclosing trys are being left without reaching their OP_POP_EXCEPTION, so this` |
|        - | 5424 | ` * runs their finallys itself. LEVELS says whether the target is even addressable from` |
|        - | 5425 | `` * here: 0 means it is in this same array (a `goto` out of a try body) and the jump is`` |
|        - | 5426 | ` * taken on the spot; above 0 the jump starts inside a DETACHED catch/finally` |
|        - | 5427 | ` * mini-program whose array is not the target's, so the target is PARKED on the owning` |
|        - | 5428 | `` * body's frame and this mini-program ends — mirroring how an explicit `return` in a`` |
|        - | 5429 | ` * catch parks on sRet at OP_DONE above. VmThrowException then runs this try's finally` |
|        - | 5430 | ` * and returns; the resume lands on the try's OP_POP_EXCEPTION, which decrements LEVELS` |
|        - | 5431 | ` * and either re-parks (another mini-program out) or takes the jump.` |
|        - | 5432 | ` */` |
|       47 | 5433 | `case PH7_OP_CATCH_JMP: {` |
|        - | 5434 | `	VmFrame *pTgt;` |
|       98 | 5435 | `	if( PH7_CATCH_JMP_LEVELS(pInstr->iP1) == 0 ){` |
|        - | 5436 | ``		/* No detached body to leave — a `goto` whose target is in THIS array, but which`` |
|        - | 5437 | `		 * jumps out of one or more enclosing trys. Nothing to park: run their finallys` |
|        - | 5438 | `		 * (a bare jump would leave them to fire at teardown) and go. */` |
|       15 | 5439 | `		rc = VmDrainCrossedTrys(&(*pVm),PH7_CATCH_JMP_CROSS(pInstr->iP1),sState.nExceptionBase);` |
|       15 | 5440 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 5441 | `			goto Abort;` |
|        - | 5442 | `		}` |
|       15 | 5443 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 5444 | `			/* A drained finally threw past itself — it supersedes the jump. */` |
|      ! 0 | 5445 | `			goto Exception;` |
|        - | 5446 | `		}` |
|       15 | 5447 | `		pc = (sxi32)pInstr->iP2 - 1;` |
|       15 | 5448 | `		break;` |
|        - | 5449 | `	}` |
|       86 | 5450 | `	pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|       86 | 5451 | `	pTgt->nCatchJmpPc = (sxu32)pInstr->iP2;` |
|       86 | 5452 | `	pTgt->nCatchJmpLevels = PH7_CATCH_JMP_LEVELS(pInstr->iP1);` |
|       86 | 5453 | `	pTgt->nCatchJmpCross = PH7_CATCH_JMP_CROSS(pInstr->iP1);` |
|        - | 5454 | `	/* A try opened INSIDE this catch body that the jump crosses had its finally run by` |
|        - | 5455 | `	 * the compiler-emitted OP_POP_EXCEPTION; drain anything still pending here. */` |
|       86 | 5456 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|       86 | 5457 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 5458 | `		goto Abort;` |
|        - | 5459 | `	}` |
|       86 | 5460 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 5461 | `		/* A drained finally threw past itself — it discards this jump. */` |
|      ! 0 | 5462 | `		pTgt->nCatchJmpPc = 0;` |
|      ! 0 | 5463 | `		goto Exception;` |
|        - | 5464 | `	}` |
|       86 | 5465 | `	goto Done;` |
|        - | 5466 | `					   }` |
|        - | 5467 | `/*` |
|        - | 5468 | ` * OP_CATCH iP1(catch-index) * P3(ph7_exception)` |
|        - | 5469 | ` * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on` |
|        - | 5470 | ` * pException->pInflight by VmThrowInline) into the catch variable, resolved in the` |
|        - | 5471 | ` * enclosing body's scope (PHP: a catch shares the surrounding variable scope).` |
|        - | 5472 | ` */` |
|       41 | 5473 | `case PH7_OP_CATCH: {` |
|        - | 5474 | `	VmOpRc rcOp;` |
|       87 | 5475 | `	sState.pTos = pTos;` |
|       87 | 5476 | `	sState.pc = pc;` |
|       87 | 5477 | `	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);` |
|       87 | 5478 | `	pTos = sState.pTos;` |
|       87 | 5479 | `	pc = sState.pc;` |
|       87 | 5480 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5481 | `		goto Abort;` |
|       87 | 5482 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5483 | `		goto Exception;` |
|        - | 5484 | `	}` |
|       87 | 5485 | `	break;` |
|        - | 5486 | `					  }` |
|        - | 5487 | `/*` |
|        - | 5488 | ` * OP_END_FINALLY * P3(ph7_exception)` |
|        - | 5489 | ` * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch` |
|        - | 5490 | ` * the pending action queued when the finally was entered (fall-through / re-throw /` |
|        - | 5491 | ` * return / break-continue). A return/break threads out through each enclosing finally` |
|        - | 5492 | ` * via pException->iNextFinallyPc.` |
|        - | 5493 | ` */` |
|       26 | 5494 | `case PH7_OP_END_FINALLY: {` |
|       57 | 5495 | `	ph7_exception *pExc = (ph7_exception *)pInstr->p3;` |
|        - | 5496 | `	VmFinallyAction sAct;` |
|       57 | 5497 | `	int eKind = PH7_FA_FALLTHROUGH;` |
|        - | 5498 | `	/* Leave the try's transparent frame kept alive across the finally. */` |
|       57 | 5499 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        8 | 5500 | `		VmLeaveFrame(&(*pVm));` |
|        3 | 5501 | `	}` |
|       57 | 5502 | `	if( SySetUsed(&pVm->aFinallyAction) > 0 ){` |
|       55 | 5503 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);` |
|       55 | 5504 | `		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];` |
|       55 | 5505 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|       55 | 5506 | `		eKind = sAct.eKind;` |
|       30 | 5507 | `	}else{` |
|        3 | 5508 | `		SyZero(&sAct,sizeof(sAct));` |
|        3 | 5509 | `		sAct.iNextPc = pExc->iEndCatchPc;` |
|        - | 5510 | `	}` |
|       57 | 5511 | `	if( eKind == PH7_FA_FALLTHROUGH ){` |
|       16 | 5512 | `		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;` |
|       20 | 5513 | `		break;` |
|       45 | 5514 | `	}else if( eKind == PH7_FA_JMP ){` |
|        - | 5515 | `		/* break/continue: run the remaining crossed finallys, then take the jump. */` |
|        5 | 5516 | `		sxu32 iFpc = 0;` |
|        5 | 5517 | `		int nCross = sAct.nCross;` |
|        5 | 5518 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|      ! 0 | 5519 | `			sAct.nCross = nCross;` |
|      ! 0 | 5520 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      ! 0 | 5521 | `			pc = (sxi32)iFpc - 1;` |
|      ! 0 | 5522 | `			break;` |
|        - | 5523 | `		}` |
|        5 | 5524 | `		pc = (sxi32)sAct.iNextPc - 1;` |
|        5 | 5525 | `		break;` |
|       41 | 5526 | `	}else if( eKind == PH7_FA_RETHROW ){` |
|       11 | 5527 | `		ph7_class_instance *pRe = sAct.pExc;` |
|        - | 5528 | `		sxi32 _iRpE;` |
|       11 | 5529 | `		rc = VmThrowException(&(*pVm),pRe);` |
|       11 | 5530 | `		if( pRe ){ PH7_ClassInstanceUnref(pRe); }` |
|       11 | 5531 | `		if( rc == SXERR_ABORT ){ goto Abort; }` |
|       11 | 5532 | `		PH7_INLINE_RESUME_BREAK()` |
|       11 | 5533 | `		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }` |
|       11 | 5534 | `		goto Exception;` |
|      ! 0 | 5535 | `	}else{ /* PH7_FA_RETURN */` |
|       31 | 5536 | `		sxu32 iFpc = 0;` |
|       31 | 5537 | `		int nCross = sAct.nCross;` |
|       31 | 5538 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        - | 5539 | `			/* Thread the return through the next enclosing finally. */` |
|        6 | 5540 | `			sAct.nCross = nCross;` |
|        6 | 5541 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        6 | 5542 | `			pc = (sxi32)iFpc - 1;` |
|        6 | 5543 | `			break;` |
|        - | 5544 | `		}` |
|        - | 5545 | `		/* No enclosing finally left: materialize the return from this body. */` |
|       27 | 5546 | `		if( sAct.bHasRetVal && sState.pResult ){` |
|        6 | 5547 | `			PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|        2 | 5548 | `		}` |
|       27 | 5549 | `		PH7_MemObjRelease(&sAct.sRet);` |
|       27 | 5550 | `		goto Done;` |
|        - | 5551 | `	}` |
|        - | 5552 | `						 }` |
|        - | 5553 | `/*` |
|        - | 5554 | ` * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)` |
|        - | 5555 | `` * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a`` |
|        - | 5556 | ` * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);` |
|        - | 5557 | ` * OP_END_FINALLY threads it out through the finally chain and then returns.` |
|        - | 5558 | ` */` |
|       30 | 5559 | `case PH7_OP_SET_FINALLY_RET: {` |
|        - | 5560 | `	VmFinallyAction sAct;` |
|       64 | 5561 | `	sxu32 iFpc = 0;` |
|       64 | 5562 | `	int nCross = -1; /* a return crosses every enclosing finally in this function */` |
|       64 | 5563 | `	SyZero(&sAct,sizeof(sAct));` |
|       64 | 5564 | `	sAct.eKind = PH7_FA_RETURN;` |
|       64 | 5565 | `	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       64 | 5566 | `	PH7_MemObjInit(pVm,&sAct.sRet);` |
|       64 | 5567 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|       52 | 5568 | `		PH7_MemObjStore(pTos,&sAct.sRet);` |
|       52 | 5569 | `		sAct.bHasRetVal = 1;` |
|       52 | 5570 | `		VmPopOperand(&pTos,1);` |
|       24 | 5571 | `	}` |
|       64 | 5572 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        9 | 5573 | `		sAct.nCross = nCross;` |
|        9 | 5574 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        9 | 5575 | `		pc = (sxi32)iFpc - 1;` |
|        9 | 5576 | `		break;` |
|        - | 5577 | `	}` |
|        - | 5578 | `	/* No enclosing finally left: return now. */` |
|       58 | 5579 | `	if( sAct.bHasRetVal && sState.pResult ){` |
|       48 | 5580 | `		PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|       22 | 5581 | `	}` |
|       58 | 5582 | `	PH7_MemObjRelease(&sAct.sRet);` |
|       58 | 5583 | `	goto Done;` |
|        - | 5584 | `						 }` |
|        - | 5585 | `/*` |
|        - | 5586 | ` * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)` |
|        - | 5587 | `` * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action`` |
|        - | 5588 | ` * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.` |
|        - | 5589 | ` */` |
|        4 | 5590 | `case PH7_OP_SET_FINALLY_JMP: {` |
|        - | 5591 | `	VmFinallyAction sAct;` |
|       10 | 5592 | `	sxu32 iFpc = 0;` |
|       10 | 5593 | `	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */` |
|       10 | 5594 | `	SyZero(&sAct,sizeof(sAct));` |
|       10 | 5595 | `	sAct.eKind = PH7_FA_JMP;` |
|       10 | 5596 | `	sAct.iNextPc = pInstr->iP2;` |
|       10 | 5597 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        5 | 5598 | `		sAct.nCross = nCross;` |
|        5 | 5599 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        5 | 5600 | `		pc = (sxi32)iFpc - 1;` |
|        5 | 5601 | `		break;` |
|        - | 5602 | `	}` |
|        - | 5603 | `	/* No finally among the crossed trys: just take the break/continue jump. */` |
|        5 | 5604 | `	pc = (sxi32)sAct.iNextPc - 1;` |
|        5 | 5605 | `	break;` |
|        - | 5606 | `						 }` |
|        - | 5607 | `/*` |
|        - | 5608 | ` * OP_THROW * P2 *` |
|        - | 5609 | ` * Throw an user exception.` |
|        - | 5610 | ` */` |
|   500501 | 5611 | `case PH7_OP_THROW: {` |
|        - | 5612 | `	VmOpRc rcOp;` |
|  1001004 | 5613 | `	sState.pTos = pTos;` |
|  1001004 | 5614 | `	sState.pc = pc;` |
|  1001004 | 5615 | `	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);` |
|  1001004 | 5616 | `	pTos = sState.pTos;` |
|  1001004 | 5617 | `	pc = sState.pc;` |
|  1001004 | 5618 | `	if( rcOp == VM_OP_ABORT ){` |
|       47 | 5619 | `		goto Abort;` |
|  1000962 | 5620 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|   600494 | 5621 | `		goto Exception;` |
|        - | 5622 | `	}` |
|   400473 | 5623 | `	break;` |
|        - | 5624 | `					  }` |
|        - | 5625 | `/*` |
|        - | 5626 | ` * OP_FOREACH_INIT * P2 P3` |
|        - | 5627 | ` * Prepare a foreach step.` |
|        - | 5628 | ` */` |
|    20656 | 5629 | `case PH7_OP_FOREACH_INIT: {` |
|        - | 5630 | `	VmOpRc rcOp;` |
|    41253 | 5631 | `	sState.pTos = pTos;` |
|    41253 | 5632 | `	sState.pc = pc;` |
|    41253 | 5633 | `	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);` |
|    41253 | 5634 | `	pTos = sState.pTos;` |
|    41253 | 5635 | `	pc = sState.pc;` |
|    41253 | 5636 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5637 | `		goto Abort;` |
|    41253 | 5638 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       41 | 5639 | `		goto Exception;` |
|        - | 5640 | `	}` |
|    41215 | 5641 | `	break;` |
|        - | 5642 | `					  }` |
|        - | 5643 | `/*` |
|        - | 5644 | ` * OP_FOREACH_STEP * P2 P3` |
|        - | 5645 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - | 5646 | ` */` |
|   304029 | 5647 | `case PH7_OP_FOREACH_STEP: {` |
|        - | 5648 | `	VmOpRc rcOp;` |
|   607249 | 5649 | `	sState.pTos = pTos;` |
|   607249 | 5650 | `	sState.pc = pc;` |
|   607249 | 5651 | `	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);` |
|   607249 | 5652 | `	pTos = sState.pTos;` |
|   607249 | 5653 | `	pc = sState.pc;` |
|   607249 | 5654 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 5655 | `		goto Abort;` |
|   607247 | 5656 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5657 | `		goto Exception;` |
|        - | 5658 | `	}` |
|   607247 | 5659 | `	break;` |
|        - | 5660 | `						  }` |
|        - | 5661 | `/*` |
|        - | 5662 | ` * OP_MEMBER P1 P2` |
|        - | 5663 | ` * Load class attribute/method on the stack.` |
|        - | 5664 | ` */` |
|   192797 | 5665 | `case PH7_OP_MEMBER: {` |
|        - | 5666 | `	VmOpRc rcOp;` |
|   385495 | 5667 | `	sState.pTos = pTos;` |
|   385495 | 5668 | `	sState.pc = pc;` |
|   385495 | 5669 | `	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);` |
|   385495 | 5670 | `	pTos = sState.pTos;` |
|   385495 | 5671 | `	pc = sState.pc;` |
|   385495 | 5672 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 5673 | `		goto Abort;` |
|   385493 | 5674 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      275 | 5675 | `		goto Exception;` |
|        - | 5676 | `	}` |
|   385221 | 5677 | `	break;` |
|        - | 5678 | `					  }` |
|        - | 5679 | `/*` |
|        - | 5680 | ` * OP_NEW P1 * * *` |
|        - | 5681 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - | 5682 | ` */` |
|  1067001 | 5683 | `case PH7_OP_NEW: {` |
|        - | 5684 | `	VmOpRc rcOp;` |
|  2133965 | 5685 | `	sState.pTos = pTos;` |
|  2133965 | 5686 | `	sState.pc = pc;` |
|  2133965 | 5687 | `	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);` |
|  2133965 | 5688 | `	pTos = sState.pTos;` |
|  2133965 | 5689 | `	pc = sState.pc;` |
|  2133965 | 5690 | `	if( rcOp == VM_OP_ABORT ){` |
|       10 | 5691 | `		goto Abort;` |
|  2133957 | 5692 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      327 | 5693 | `		goto Exception;` |
|        - | 5694 | `	}` |
|  2133634 | 5695 | `	break;` |
|        - | 5696 | `					  }` |
|        - | 5697 | `/*` |
|        - | 5698 | ` * OP_CLONE * * *` |
|        - | 5699 | ` * Perfome a clone operation.` |
|        - | 5700 | ` */` |
|      221 | 5701 | `case PH7_OP_CLONE: {` |
|        - | 5702 | `	VmOpRc rcOp;` |
|      447 | 5703 | `	sState.pTos = pTos;` |
|      447 | 5704 | `	sState.pc = pc;` |
|      447 | 5705 | `	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);` |
|      447 | 5706 | `	pTos = sState.pTos;` |
|      447 | 5707 | `	pc = sState.pc;` |
|      447 | 5708 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5709 | `		goto Abort;` |
|      447 | 5710 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       50 | 5711 | `		goto Exception;` |
|        - | 5712 | `	}` |
|      399 | 5713 | `	break;` |
|        - | 5714 | `					  }` |
|        - | 5715 | `/*` |
|        - | 5716 | ` * OP_SWITCH * * P3` |
|        - | 5717 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - | 5718 | ` */` |
|      125 | 5719 | `case PH7_OP_SWITCH: {` |
|        - | 5720 | `	VmOpRc rcOp;` |
|      255 | 5721 | `	sState.pTos = pTos;` |
|      255 | 5722 | `	sState.pc = pc;` |
|      255 | 5723 | `	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);` |
|      255 | 5724 | `	pTos = sState.pTos;` |
|      255 | 5725 | `	pc = sState.pc;` |
|      255 | 5726 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5727 | `		goto Abort;` |
|      255 | 5728 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 5729 | `		goto Exception;` |
|        - | 5730 | `	}` |
|      251 | 5731 | `	break;` |
|        - | 5732 | `					  }` |
|        - | 5733 | `/*` |
|        - | 5734 | ` * OP_MATCH * * P3` |
|        - | 5735 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - | 5736 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - | 5737 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - | 5738 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - | 5739 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - | 5740 | ` */` |
|      112 | 5741 | `case PH7_OP_MATCH: {` |
|        - | 5742 | `	VmOpRc rcOp;` |
|      229 | 5743 | `	sState.pTos = pTos;` |
|      229 | 5744 | `	sState.pc = pc;` |
|      229 | 5745 | `	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);` |
|      229 | 5746 | `	pTos = sState.pTos;` |
|      229 | 5747 | `	pc = sState.pc;` |
|      229 | 5748 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5749 | `		goto Abort;` |
|      229 | 5750 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        6 | 5751 | `		goto Exception;` |
|        - | 5752 | `	}` |
|      224 | 5753 | `	break;` |
|        - | 5754 | `					  }` |
|        - | 5755 | `/*` |
|        - | 5756 | ` * OP_YIELD P1 P2 *` |
|        - | 5757 | ` *  Yield a value from a generator function.` |
|        - | 5758 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - | 5759 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - | 5760 | ` */` |
|      731 | 5761 | `case PH7_OP_YIELD: {` |
|        - | 5762 | `	ph7_generator *pGen;` |
|     1467 | 5763 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5764 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 | 5765 | `		goto Abort;` |
|        - | 5766 | `	}` |
|     1467 | 5767 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5768 | ``		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's`` |
|        - | 5769 | `		 * pending finallys tried to yield — PHP forbids it. */` |
|      ! 0 | 5770 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5771 | `			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5772 | `			goto Abort;` |
|        - | 5773 | `		}` |
|      ! 0 | 5774 | `		goto Exception;` |
|        - | 5775 | `	}` |
|     1467 | 5776 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|     1467 | 5777 | `	if( pInstr->iP2 ){` |
|        - | 5778 | `		/* yield $key => $value: value on top, key below */` |
|        - | 5779 | `#ifdef UNTRUST` |
|        - | 5780 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - | 5781 | `#endif` |
|      166 | 5782 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|      166 | 5783 | `		VmPopOperand(&pTos, 1);` |
|      166 | 5784 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|      166 | 5785 | `		VmPopOperand(&pTos, 1);` |
|        - | 5786 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|      166 | 5787 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|       30 | 5788 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|       30 | 5789 | `			if( nKey >= pGen->iImplicitKey ){` |
|       30 | 5790 | `				pGen->iImplicitKey = nKey + 1;` |
|       13 | 5791 | `			}` |
|       17 | 5792 | `		}` |
|     1386 | 5793 | `	}else if( pInstr->iP1 ){` |
|        - | 5794 | `		/* yield $value */` |
|        - | 5795 | `#ifdef UNTRUST` |
|        - | 5796 | `		if( pTos < pStack ) goto Abort;` |
|        - | 5797 | `#endif` |
|     1303 | 5798 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|     1303 | 5799 | `		VmPopOperand(&pTos, 1);` |
|        - | 5800 | `		/* Auto-increment key */` |
|     1303 | 5801 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|     1303 | 5802 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|     1303 | 5803 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|      654 | 5804 | `	}else{` |
|        - | 5805 | `		/* Bare yield — null value, auto-increment key */` |
|        3 | 5806 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|        3 | 5807 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|        3 | 5808 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|        3 | 5809 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - | 5810 | `	}` |
|        - | 5811 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|     1467 | 5812 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|     1467 | 5813 | `	goto Suspend;` |
|        - | 5814 | `}` |
|        - | 5815 | `/*` |
|        - | 5816 | ` * OP_YIELD_FROM * * *` |
|        - | 5817 | ` *` |
|        - | 5818 | `` * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an`` |
|        - | 5819 | ` * array/Traversable/Generator from the OUTER generator, preserving the inner` |
|        - | 5820 | ` * keys; the expression evaluates to the inner Generator's return value (or NULL).` |
|        - | 5821 | ` *` |
|        - | 5822 | ` * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,` |
|        - | 5823 | ` * advances the per-instance delegate cursor stored on the exec context (never the` |
|        - | 5824 | ` * shared foreach aStep, so independent generator instances cannot clash). The` |
|        - | 5825 | ` * iterable operand is consumed on first entry; the expression result is pushed at` |
|        - | 5826 | ` * exhaustion — net stack effect +1, identical to OP_YIELD.` |
|        - | 5827 | ` */` |
|      111 | 5828 | `case PH7_OP_YIELD_FROM: {` |
|        - | 5829 | `	ph7_generator *pGenFrom;` |
|        - | 5830 | `	ph7_exec_ctx *pCtxFrom;` |
|        - | 5831 | `	ph7_value sKey,sVal;` |
|      227 | 5832 | `	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */` |
|      227 | 5833 | `	int bExhausted = 0;` |
|      227 | 5834 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5835 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");` |
|      ! 0 | 5836 | `		goto Abort;` |
|        - | 5837 | `	}` |
|      227 | 5838 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5839 | ``		/* A `yield from` reached while VmCloseCtx force-drives this destroyed`` |
|        - | 5840 | `		 * generator's pending finallys — PHP forbids it (distinct message from a` |
|        - | 5841 | ``		 * bare `yield`, mirroring the OP_YIELD guard above). */`` |
|      ! 0 | 5842 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5843 | `			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5844 | `			goto Abort;` |
|        - | 5845 | `		}` |
|      ! 0 | 5846 | `		goto Exception;` |
|        - | 5847 | `	}` |
|      227 | 5848 | `	pCtxFrom = pVm->pActiveCtx;` |
|      227 | 5849 | `	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;` |
|      227 | 5850 | `	PH7_MemObjInit(pVm,&sKey);` |
|      227 | 5851 | `	PH7_MemObjInit(pVm,&sVal);` |
|      227 | 5852 | `	if( pCtxFrom->iDelegateState == 0 ){` |
|        - | 5853 | `		/* First entry: classify the iterable on the stack top. */` |
|       97 | 5854 | `		int bIterable = 1;` |
|        - | 5855 | `#ifdef UNTRUST` |
|        - | 5856 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 5857 | `#endif` |
|       97 | 5858 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       35 | 5859 | `			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       35 | 5860 | `			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;` |
|       35 | 5861 | `			pCtxFrom->iDelegateState = 1;` |
|       82 | 5862 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       63 | 5863 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       63 | 5864 | `			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       63 | 5865 | `			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){` |
|       52 | 5866 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       52 | 5867 | `				pCtxFrom->iDelegateState = 3;` |
|       37 | 5868 | `			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){` |
|        9 | 5869 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        9 | 5870 | `				pCtxFrom->iDelegateState = 2;` |
|        6 | 5871 | `			}else{` |
|        6 | 5872 | `				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - | 5873 | `					sizeof("IteratorAggregate")-1,FALSE,0);` |
|        8 | 5874 | `				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){` |
|        - | 5875 | `					/* Delegate to the Iterator returned by getIterator() */` |
|        - | 5876 | `					ph7_value sIt;` |
|        6 | 5877 | `					PH7_MemObjInit(pVm,&sIt);` |
|        6 | 5878 | `					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);` |
|        6 | 5879 | `					if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){` |
|        - | 5880 | `						/* getIterator() threw/aborted: drop it, consume the` |
|        - | 5881 | `						 * operand, and propagate. */` |
|      ! 0 | 5882 | `						PH7_MemObjRelease(&sIt);` |
|      ! 0 | 5883 | `						VmPopOperand(&pTos,1);` |
|      ! 0 | 5884 | `						goto yf_propagate;` |
|        - | 5885 | `					}` |
|        4 | 5886 | `					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls` |
|        6 | 5887 | `						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){` |
|        6 | 5888 | `						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);` |
|        6 | 5889 | `						pCtxFrom->iDelegateState = 2;` |
|        4 | 5890 | `					}else{` |
|      ! 0 | 5891 | `						bIterable = 0;` |
|        - | 5892 | `					}` |
|        6 | 5893 | `					PH7_MemObjRelease(&sIt);` |
|        4 | 5894 | `				}else{` |
|      ! 0 | 5895 | `					bIterable = 0;` |
|        - | 5896 | `				}` |
|        - | 5897 | `			}` |
|       34 | 5898 | `		}else{` |
|        6 | 5899 | `			bIterable = 0;` |
|        - | 5900 | `		}` |
|       97 | 5901 | `		VmPopOperand(&pTos,1); /* Consume the iterable operand */` |
|       97 | 5902 | `		if( !bIterable ){` |
|        - | 5903 | `			/* Non-iterable source: throw a catchable Error (PHP 8.5), then` |
|        - | 5904 | `			 * funnel through the shared teardown/route path. */` |
|        6 | 5905 | `			rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 5906 | `				"Can use \"yield from\" only with arrays and Traversables",` |
|        - | 5907 | `				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);` |
|        6 | 5908 | `			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        6 | 5909 | `			goto yf_propagate;` |
|        - | 5910 | `		}` |
|       93 | 5911 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|        - | 5912 | `			/* A GENERATOR delegate is not rewound: php links it as a child node and` |
|        - | 5913 | ``			 * only INITIALIZES it, so `yield from $g` over a half-consumed generator`` |
|        - | 5914 | `			 * continues from where it stands. One state it refuses outright, with` |
|        - | 5915 | `			 * its own Error rather than the traverse/rewind wording the other entry` |
|        - | 5916 | `			 * points use — a generator that has already run to its end, which PHL` |
|        - | 5917 | `			 * delegated to in silence and yielded NOTHING from. */` |
|       52 | 5918 | `			ph7_class_instance *pDel = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|       52 | 5919 | `			if( PH7_VmGeneratorIsClosed(&(*pVm),pDel) ){` |
|        3 | 5920 | `				rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 5921 | `					"Generator passed to yield from was aborted without proper return and is unable to continue",` |
|        - | 5922 | `					sizeof("Generator passed to yield from was aborted without proper return and is unable to continue")-1);` |
|        3 | 5923 | `				rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        3 | 5924 | `				goto yf_propagate;` |
|        - | 5925 | `			}` |
|       50 | 5926 | `			rcm = PH7_VmGeneratorPrime(&(*pVm),pDel);` |
|       50 | 5927 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       65 | 5928 | `		}else if( pCtxFrom->iDelegateState >= 2 ){` |
|        - | 5929 | `			/* rewind() a plain Iterator delegate */` |
|       13 | 5930 | `			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 5931 | `				"rewind",sizeof("rewind")-1,0);` |
|       13 | 5932 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 5933 | `		}` |
|       45 | 5934 | `	}else{` |
|        - | 5935 | `		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a` |
|        - | 5936 | `		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()` |
|        - | 5937 | `		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)` |
|        - | 5938 | `		 * ignore send() and just advance with next(). */` |
|        - | 5939 | `#ifdef UNTRUST` |
|        - | 5940 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 5941 | `#endif` |
|      135 | 5942 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       74 | 5943 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|        - | 5944 | `			/* A pending Generator::throw() on the outer was parked on pInjected by the` |
|        - | 5945 | `			 * body-entry gate (which skips its own raise for state 3); forward it as a` |
|        - | 5946 | `			 * throw into the delegate, else forward the sent value (pTos, which` |
|        - | 5947 | `			 * VmResumeCtx copies into the inner's own stack). */` |
|       74 | 5948 | `			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;` |
|       74 | 5949 | `			pCtxFrom->pInjected = 0;` |
|       74 | 5950 | `			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       74 | 5951 | `				if( pInjFwd ){` |
|        - | 5952 | `					/* Borrowed ref: the outer's Generator::throw() holds it across this` |
|        - | 5953 | `					 * whole resume, so the inner inject path must not unref it. */` |
|        5 | 5954 | `					pInner->pCtx->pInjected = pInjFwd;` |
|        5 | 5955 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);` |
|        5 | 5956 | `					pInner->pCtx->pInjected = 0;` |
|        3 | 5957 | `				}else{` |
|       70 | 5958 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);` |
|        4 | 5959 | `				}` |
|       35 | 5960 | `			}else if( pInjFwd ){` |
|        - | 5961 | `				/* No live delegate to receive the throw (inner already finished/closed):` |
|        - | 5962 | `				 * raise it at the yield-from in the outer generator's own frame. */` |
|      ! 0 | 5963 | `				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);` |
|      ! 0 | 5964 | `				pTF->iFlags \|= VM_FRAME_THROW;` |
|      ! 0 | 5965 | `				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 5966 | `			}` |
|       74 | 5967 | `			PH7_MemObjRelease(pTos);` |
|       74 | 5968 | `			pTos--;` |
|       74 | 5969 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       36 | 5970 | `		}else{` |
|       65 | 5971 | `			PH7_MemObjRelease(pTos);` |
|       65 | 5972 | `			pTos--;` |
|       65 | 5973 | `			if( pCtxFrom->iDelegateState >= 2 ){` |
|       17 | 5974 | `				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 5975 | `					"next",sizeof("next")-1,0);` |
|       17 | 5976 | `				if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 5977 | `			}` |
|        - | 5978 | `		}` |
|        - | 5979 | `	}` |
|        - | 5980 | `	/* Fetch the current (key,value) of the delegate, or mark exhausted. */` |
|      205 | 5981 | `	if( pCtxFrom->iDelegateState == 1 ){` |
|       81 | 5982 | `		if( pCtxFrom->pDelegateNode == 0 ){` |
|       27 | 5983 | `			bExhausted = 1;` |
|       16 | 5984 | `		}else{` |
|       59 | 5985 | `			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);` |
|       59 | 5986 | `			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);` |
|        - | 5987 | `			/* Forward traversal follows pPrev (the hashmap's "reverse link",` |
|        - | 5988 | `			 * matching PH7_HashmapGetNextEntry). */` |
|       59 | 5989 | `			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;` |
|        - | 5990 | `		}` |
|       43 | 5991 | `	}else{` |
|      129 | 5992 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|        - | 5993 | `		ph7_value sValid;` |
|        - | 5994 | `		int isValid;` |
|      129 | 5995 | `		PH7_MemObjInit(pVm,&sValid);` |
|      129 | 5996 | `		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|      129 | 5997 | `		PH7_MemObjToBool(&sValid);` |
|      129 | 5998 | `		isValid = (sValid.x.iVal != 0);` |
|      129 | 5999 | `		PH7_MemObjRelease(&sValid);` |
|      129 | 6000 | `		if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      129 | 6001 | `		if( !isValid ){` |
|       33 | 6002 | `			bExhausted = 1;` |
|       19 | 6003 | `		}else{` |
|      101 | 6004 | `			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|      101 | 6005 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      101 | 6006 | `			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|      101 | 6007 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        - | 6008 | `		}` |
|        - | 6009 | `	}` |
|      205 | 6010 | `	if( bExhausted ){` |
|        - | 6011 | `		/* Expression value: inner Generator's return value (state 3) or NULL. */` |
|        - | 6012 | `		ph7_value sResult;` |
|       55 | 6013 | `		PH7_MemObjInit(pVm,&sResult);` |
|       55 | 6014 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       28 | 6015 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|       28 | 6016 | `			if( pInner && pInner->pCtx ){` |
|       28 | 6017 | `				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);` |
|       12 | 6018 | `			}` |
|       12 | 6019 | `		}` |
|       55 | 6020 | `		PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       55 | 6021 | `		pCtxFrom->pDelegateNode = 0;` |
|       55 | 6022 | `		pCtxFrom->iDelegateState = 0;` |
|       55 | 6023 | `		pTos++;` |
|       55 | 6024 | `		PH7_MemObjStore(&sResult,pTos);` |
|       55 | 6025 | `		PH7_MemObjRelease(&sResult);` |
|       55 | 6026 | `		PH7_MemObjRelease(&sKey);` |
|       55 | 6027 | `		PH7_MemObjRelease(&sVal);` |
|       55 | 6028 | `		break; /* fall through to pc+1 with the result on the stack top */` |
|        - | 6029 | `	}` |
|        - | 6030 | `	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.` |
|        - | 6031 | `	 * The outer generator's implicit auto-key counter is NOT advanced by the` |
|        - | 6032 | ``	 * delegated keys — PHP keeps it independent across `yield from`, so a later`` |
|        - | 6033 | ``	 * plain `yield` continues from the outer's own counter. */`` |
|      155 | 6034 | `	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);` |
|      155 | 6035 | `	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);` |
|      155 | 6036 | `	PH7_MemObjRelease(&sKey);` |
|      155 | 6037 | `	PH7_MemObjRelease(&sVal);` |
|        - | 6038 | `	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */` |
|      155 | 6039 | `	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));` |
|      155 | 6040 | `	goto Suspend;` |
|       11 | 6041 | `yf_propagate:` |
|        - | 6042 | `	/* A delegate iterator method threw/aborted (or the source was non-iterable):` |
|        - | 6043 | `	 * tear down the delegation, then route via the shared dispatch macro. rcm is` |
|        - | 6044 | `	 * always PH7_EXCEPTION or PH7_ABORT here. */` |
|       26 | 6045 | `	PH7_MemObjRelease(&sKey);` |
|       26 | 6046 | `	PH7_MemObjRelease(&sVal);` |
|       26 | 6047 | `	PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       26 | 6048 | `	pCtxFrom->pDelegateNode = 0;` |
|       26 | 6049 | `	pCtxFrom->iDelegateState = 0;` |
|       26 | 6050 | `	PH7_DISPATCH_ENFORCE_RC(rcm)` |
|      ! 0 | 6051 | `	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */` |
|        - | 6052 | `}` |
|        - | 6053 | `/*` |
|        - | 6054 | ` * OP_CALL P1 * *` |
|        - | 6055 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - | 6056 | ` *  function on the stack.` |
|        - | 6057 | ` */` |
|        - | 6058 | `/*` |
|        - | 6059 | ` * OP_CALL_INIT * P2 *` |
|        - | 6060 | ` *  Screen the callee on TOS where it is WRITTEN — before this call's arguments run.` |
|        - | 6061 | ` *` |
|        - | 6062 | ` *  php resolves a call's target at INIT_FCALL / INIT_FCALL_BY_NAME / INIT_DYNAMIC_CALL` |
|        - | 6063 | `` *  and raises there, so `undefinedFn(s(1))`, `$f(s(1))` over a misspelled name and`` |
|        - | 6064 | `` *  `$v(s(1))` over an int all refuse BEFORE `s(1)` runs. PHL only ever looked at the`` |
|        - | 6065 | ` *  callee inside OP_CALL, one instruction after the whole argument list, so every one` |
|        - | 6066 | ` *  of those programs produced the argument's side effects (or its exception) first and` |
|        - | 6067 | ` *  php's Error second. The messages were already identical; only the order was not.` |
|        - | 6068 | ` *` |
|        - | 6069 | ` *  The verdict is the FIRST-CLASS-CALLABLE creation screen, unchanged and shared: php` |
|        - | 6070 | `` *  gives `f(...)` the direct call's taxonomy word for word, which makes VmFccValueError`` |
|        - | 6071 | ` *  the one builder for both. The value is left exactly as it is — OP_CALL still does its` |
|        - | 6072 | ` *  own resolution — so this adds a refusal and changes nothing that succeeds. P2 == 1` |
|        - | 6073 | ` *  when the compiler namespace-qualified the name, which is the one bit php's` |
|        - | 6074 | `` *  global-function fallback needs (an unqualified `strlen(...)` inside a namespace).`` |
|        - | 6075 | ` *` |
|        - | 6076 | ` *  Not emitted for a callee whose OP_MEMBER already screened it, for a first-class` |
|        - | 6077 | ` *  callable (OP_LOAD_FCC screens it, with nothing running in between), or for a call` |
|        - | 6078 | ` *  with no arguments at all — there the call IS the first thing that happens.` |
|        - | 6079 | ` */` |
|  1913970 | 6080 | `case PH7_OP_CALL_INIT: {` |
|        - | 6081 | `	/* This screen resolves the callee that OP_CALL is about to resolve again -- it is` |
|        - | 6082 | `	 * here only so php's Error lands before the arguments run -- and it was 4.4% of a` |
|        - | 6083 | `	 * phpcs profile. When the callee is a compile-time constant (the push behind this` |
|        - | 6084 | `	 * instruction is an OP_LOADC) the answer can only change if the set of callable` |
|        - | 6085 | `	 * NAMES changes, and that bumps pVm->nCallableGen. So a site that has passed once` |
|        - | 6086 | `	 * passes for free until something is declared. A dynamic callee is never stamped` |
|        - | 6087 | `	 * and is screened on every call, as it must be. */` |
|  3828174 | 6088 | `	if( pInstr->nAux == pVm->nCallableGen ){` |
|  3685870 | 6089 | `		break;` |
|        - | 6090 | `	}` |
|   142309 | 6091 | `	bCallInitStamp = 0;` |
|   142304 | 6092 | `	if( (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_MAGICCALL)) == 0` |
|   142309 | 6093 | `	 && !VmValueIsClosure(pVm,pTos) ){` |
|   135620 | 6094 | `		const char *zInitCls = 0,*zInitMeth = 0;` |
|   135620 | 6095 | `		sxu32 nInitCls = 0,nInitMeth = 0;` |
|        - | 6096 | `		char zInitMsg[192];` |
|   135620 | 6097 | `		const char *zInitBad = 0;` |
|   135620 | 6098 | `		SyString sInitName = { 0, 0 };` |
|   135620 | 6099 | `		int bInitScoped = 0;` |
|   135620 | 6100 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|   133658 | 6101 | `			SyStringInitFromBuf(&sInitName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 6102 | `			/* A leading backslash only anchors the name to the global namespace. */` |
|   133658 | 6103 | `			if( sInitName.nByte > 0 && sInitName.zString[0] == '\\' ){` |
|        3 | 6104 | `				sInitName.zString++;` |
|        3 | 6105 | `				sInitName.nByte--;` |
|        1 | 6106 | `			}` |
|   133658 | 6107 | `			bInitScoped = PH7_VmCallableStringParts(sInitName.zString,sInitName.nByte,` |
|        - | 6108 | `				&zInitCls,&nInitCls,&zInitMeth,&nInitMeth);` |
|        - | 6109 | `			/* A plain function NAME is the one verdict that depends on nothing but the` |
|        - | 6110 | ``			 * callable set. A `Class::method` string is screened for VISIBILITY too,`` |
|        - | 6111 | `			 * and a trait's body can run under more than one class, so that one is` |
|        - | 6112 | `			 * asked every time. */` |
|   133658 | 6113 | `			bCallInitStamp = !bInitScoped;` |
|    66438 | 6114 | `		}` |
|   135620 | 6115 | `		if( bInitScoped ){` |
|        - | 6116 | ``			/* A `"Class::method"` string carries its whole taxonomy in one builder — the`` |
|        - | 6117 | `			 * class, the missing/abstract/inaccessible cases and the catch-all routing —` |
|        - | 6118 | `			 * and answers 0 when the call WILL run. It is asked unconditionally because` |
|        - | 6119 | `			 * the predicate below is not the same question: is_callable() accepts a` |
|        - | 6120 | `			 * non-static method named through a class, which the direct call refuses. */` |
|       28 | 6121 | `			zInitBad = VmCallableClassMethodError(&(*pVm),` |
|        9 | 6122 | `				PH7_VmExtractClass(&(*pVm),zInitCls,nInitCls,FALSE,0),` |
|        9 | 6123 | `				zInitCls,nInitCls,zInitMeth,nInitMeth,TRUE,zInitMsg,sizeof(zInitMsg));` |
|   135611 | 6124 | `		}else if( !PH7_VmIsCallable(&(*pVm),pTos,TRUE) ){` |
|        - | 6125 | `			/* Not callable under the name as WRITTEN. php's global fallback below may` |
|        - | 6126 | `			 * still find it, and that verdict is generation-dependent like any other --` |
|        - | 6127 | `			 * so the stamp is left standing here and only a real refusal retires it,` |
|        - | 6128 | `			 * which it does by throwing before the stamp is written. Most calls in a` |
|        - | 6129 | ``			 * namespaced file (every `count()`, `is_array()`, `trim()` in phpcs) take`` |
|        - | 6130 | `			 * exactly this path, and it is the expensive one: two callability screens` |
|        - | 6131 | `			 * and a temporary value for the shortened name. */` |
|      127 | 6132 | `			int bInitOk = 0;` |
|      127 | 6133 | `			if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 6134 | `				/* php's global fallback for an UNQUALIFIED name written inside a` |
|        - | 6135 | `				 * namespace: the current namespace first, the global one after. OP_CALL` |
|        - | 6136 | `				 * retries the same way from its argument map; this only has to agree` |
|        - | 6137 | `				 * about whether the call WILL resolve, so the shortened name is tested` |
|        - | 6138 | `				 * and thrown away. */` |
|       75 | 6139 | `				const char *zInitShort = sInitName.zString;` |
|        - | 6140 | `				sxu32 iInitPos;` |
|     1147 | 6141 | `				for( iInitPos = 0 ; iInitPos < sInitName.nByte ; ++iInitPos ){` |
|     1077 | 6142 | `					if( sInitName.zString[iInitPos] == '\\' ){` |
|       89 | 6143 | `						zInitShort = &sInitName.zString[iInitPos + 1];` |
|       42 | 6144 | `					}` |
|      541 | 6145 | `				}` |
|       75 | 6146 | `				if( zInitShort != sInitName.zString ){` |
|        - | 6147 | `					ph7_value sInitShort;` |
|       75 | 6148 | `					PH7_MemObjInit(pVm,&sInitShort);` |
|      110 | 6149 | `					PH7_MemObjStringAppend(&sInitShort,zInitShort,` |
|       70 | 6150 | `						(sxu32)(sInitName.nByte - (sxu32)(zInitShort - sInitName.zString)));` |
|       75 | 6151 | `					bInitOk = PH7_VmIsCallable(&(*pVm),&sInitShort,TRUE);` |
|       75 | 6152 | `					PH7_MemObjRelease(&sInitShort);` |
|       35 | 6153 | `				}` |
|       35 | 6154 | `			}` |
|        - | 6155 | `			/* The FIRST-CLASS-CALLABLE creation screen's builder, unchanged and shared:` |
|        - | 6156 | ``			 * php gives `f(...)` the direct call's taxonomy word for word. It assumes the`` |
|        - | 6157 | `			 * predicate has already declined — a pair a class answers through __call is` |
|        - | 6158 | `			 * callable and never arrives here — which is why it sits under that test. */` |
|      127 | 6159 | `			if( !bInitOk ){` |
|       56 | 6160 | `				zInitBad = VmFccValueError(&(*pVm),pTos,zInitMsg,sizeof(zInitMsg));` |
|       26 | 6161 | `			}` |
|       61 | 6162 | `		}` |
|   135620 | 6163 | `		if( zInitBad ){` |
|        - | 6164 | `			sxi32 rcInit;` |
|       62 | 6165 | `			PH7_MemObjRelease(pTos);` |
|       62 | 6166 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       62 | 6167 | `			pTos->nIdx = SXU32_HIGH;` |
|       62 | 6168 | `			rcInit = VmThrowFromVm(&(*pVm),"Error",zInitBad,(sxu32)SyStrlen(zInitBad));` |
|       62 | 6169 | `			if( rcInit == SXERR_ABORT ){ goto Abort; }` |
|       62 | 6170 | `			rc = rcInit;` |
|       78 | 6171 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6172 | `		}` |
|    67390 | 6173 | `	}` |
|   142251 | 6174 | `	if( bCallInitStamp && pc > 0 && aInstr[pc-1].iOp == PH7_OP_LOADC ){` |
|        - | 6175 | `		/* The callee is the same literal every time this site runs, so record that it` |
|        - | 6176 | `` 		 * was screened -- and at WHICH generation, because a later `function f(){}` `` |
|        - | 6177 | `		 * (or a class, or an unregistered host function) can change the answer. */` |
|   132650 | 6178 | `		pInstr->nAux = pVm->nCallableGen;` |
|    65934 | 6179 | `	}` |
|   142251 | 6180 | `	break;` |
|        - | 6181 | `}` |
|        - | 6182 | `/*` |
|        - | 6183 | ` * OP_ROT_CALLEE P1 P2 *` |
|        - | 6184 | ` *  Turn a call's operand region over: [callee][arg0..argN] becomes [arg0..argN][callee],` |
|        - | 6185 | ` *  which is the layout OP_CALL's entire dispatch is written against.` |
|        - | 6186 | ` *` |
|        - | 6187 | ` *  The codegen pushes the callee FIRST because php resolves it where it is written —` |
|        - | 6188 | ` *  before a single argument runs — so an undefined or inaccessible method is refused` |
|        - | 6189 | `` *  ahead of the argument list's side effects, and a `?->` on null skips the arguments`` |
|        - | 6190 | ` *  altogether. Everything downstream of this instruction still sees the historical` |
|        - | 6191 | ` *  stack, so the reordering costs one memory move per call and nothing else.` |
|        - | 6192 | ` *` |
|        - | 6193 | ` *  P1 is the compile-time argument count; P2 carries PH7_ROT_SPREAD (this call unpacks,` |
|        - | 6194 | ` *  so the runtime count is P1 plus its OWN runs' net growth) and PH7_ROT_TWOSLOT (the` |
|        - | 6195 | ` *  callee is a method pair, [receiver][name]). A __call routing collapses that pair to` |
|        - | 6196 | ` *  one marked carrier at run time, which is read off the slot rather than guessed.` |
|        - | 6197 | ` */` |
|  2430210 | 6198 | `case PH7_OP_ROT_CALLEE: {` |
|  9721217 | 6199 | `	sxi32 nRotArgs = pInstr->iP1` |
|  4860606 | 6200 | `		+ ((pInstr->iP2 & PH7_ROT_SPREAD)` |
|        - | 6201 | `			/* One past the last argument is one past the TOP here: the callee sits` |
|        - | 6202 | `			 * BELOW the region, not above it as at OP_CALL. */` |
|  2430798 | 6203 | `			? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,&pTos[1]) : 0);` |
|  4860611 | 6204 | `	if( nRotArgs < 0 ){` |
|        - | 6205 | `		/* Unreachable: an empty unpack subtracts one per compile-time position, so the` |
|        - | 6206 | `		 * net can reach 0 and no lower. Clamped rather than trusted — reading above the` |
|        - | 6207 | `		 * top to find the callee is not a failure mode worth leaving open. */` |
|      ! 0 | 6208 | `		nRotArgs = 0;` |
|      ! 0 | 6209 | `	}` |
|        - | 6210 | `	{` |
|        - | 6211 | `		ph7_value aCallee[2];` |
|  4860611 | 6212 | `		ph7_value *pTopCallee = &pTos[-nRotArgs];` |
|  4860611 | 6213 | `		sxi32 nCallee = (pInstr->iP2 & PH7_ROT_TWOSLOT) ? 2 : 1;` |
|        - | 6214 | `		ph7_value *pBase;` |
|        - | 6215 | `		sxi32 i;` |
|  4860611 | 6216 | `		if( nCallee > 1 && (pTopCallee->iFlags & MEMOBJ_AUX_MAGICCALL) ){` |
|        - | 6217 | `			/* OP_MEMBER routed a missing/inaccessible name to __call: it consumed the` |
|        - | 6218 | `			 * receiver and left ONE carrier slot, so the pair the compiler counted on` |
|        - | 6219 | `			 * is not there. */` |
|       71 | 6220 | `			nCallee = 1;` |
|       34 | 6221 | `		}` |
|  4860611 | 6222 | `		pBase = pTopCallee - (nCallee - 1);` |
|        - | 6223 | `#ifdef UNTRUST` |
|        - | 6224 | `		if( pBase < pStack ){` |
|        - | 6225 | `			goto Abort;` |
|        - | 6226 | `		}` |
|        - | 6227 | `#endif` |
|  4860611 | 6228 | `		if( nRotArgs > 0 ){` |
|  9744063 | 6229 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  4883491 | 6230 | `				aCallee[i] = pBase[i];` |
|  2441828 | 6231 | `			}` |
| 13267916 | 6232 | `			for( i = 0 ; i < nRotArgs ; ++i ){` |
|  8407344 | 6233 | `				pBase[i] = pBase[i + nCallee];` |
|  4203124 | 6234 | `			}` |
|  9744063 | 6235 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  4883491 | 6236 | `				pBase[nRotArgs + i] = aCallee[i];` |
|  2441828 | 6237 | `			}` |
|  2430379 | 6238 | `		}` |
|  4860611 | 6239 | `		if( pInstr->iP2 & PH7_ROT_SPREAD ){` |
|        - | 6240 | `			/* The argument region now ends nCallee slots lower than it did, so this` |
|        - | 6241 | `			 * call's captured unpack runs — the suffix VmSpreadOwnExtra just assigned` |
|        - | 6242 | `			 * to it, all of them anchored inside the region — move with it, and OP_CALL` |
|        - | 6243 | ``			 * re-derives the same count from them. Unconditional: an `f(...[])` unpack`` |
|        - | 6244 | `			 * moves NO argument (its run is zero-width) and still has to be re-anchored,` |
|        - | 6245 | `			 * or the recount reads it as an ordinary slot and eats one slot too many.` |
|        - | 6246 | `			 * An ENCLOSING call's runs sit below the callee and are left alone. */` |
|     1276 | 6247 | `			sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|     1276 | 6248 | `			VmSpreadRun *aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|        - | 6249 | `			sxu32 r;` |
|     2563 | 6250 | `			for( r = pVm->nSpreadCallBase ; r < nRun ; ++r ){` |
|     1292 | 6251 | `				aRun[r].pStart -= nCallee;` |
|      601 | 6252 | `			}` |
|      588 | 6253 | `		}` |
|        - | 6254 | `	}` |
|  4860611 | 6255 | `	break;` |
|        - | 6256 | `}` |
|  3857620 | 6257 | `case PH7_OP_CALL: {` |
|        - | 6258 | `	/* iP2 = hasSpread (compile-time). Count only THIS call's own unpack` |
|        - | 6259 | `	 * expansion (VmSpreadOwnExtra, derived from the captured runs on top of the` |
|        - | 6260 | `	 * stack) — an INNER spread-bearing call evaluated inside this argument list` |
|        - | 6261 | ``	 * (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not`` |
|        - | 6262 | `	 * be conflated, which a single shared accumulator could not express. */` |
|  7714759 | 6263 | `	sxi32 nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|        - | 6264 | `	ph7_value *pArg;` |
|        - | 6265 | `	/* Consume the engine's magic-dispatch latch here, at the head of the ONE call` |
|        - | 6266 | `	 * it was set for, whatever path that call then takes. Left standing it would` |
|        - | 6267 | `	 * describe the next call instead. */` |
|  7714759 | 6268 | `	int bMagicDispatch = pVm->bMagicDispatch;` |
|        - | 6269 | `	/* ...and the member resolution's own verdict, which rides the callee SLOT rather` |
|        - | 6270 | `	 * than the VM: an OP_MEMBER that produced this callee already decided its` |
|        - | 6271 | `	 * visibility against the entry it chose, so the screen below must stand down. */` |
| 15350268 | 6272 | `	int bMemberScreened = (pTos->iFlags & MEMOBJ_AUX_MEMBERCALL) != 0` |
|  7714754 | 6273 | `		\|\| pVm->bClosureScreened;` |
|        - | 6274 | `	/* ...and whether the NAME in that slot is one of the engine's own function-table` |
|        - | 6275 | `	 * keys rather than something the program spelled: an OP_MEMBER method resolution` |
|        - | 6276 | ``	 * pushes the method's `sVmName` (`[__Class@meth_xxxxxxxxxx]`), and so do the two`` |
|        - | 6277 | `	 * synthetic call builders. Read here because the member mark is cleared just` |
|        - | 6278 | `	 * below; the closure branch adds its own case further down. It is what lets` |
|        - | 6279 | `	 * PH7_VmGetUserFunction refuse those keys to a SCRIPT that spells one. */` |
|  7714759 | 6280 | `	int bEngineCallee = (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_ENGINEFN)) != 0;` |
|        - | 6281 | `	/* ...and the internal-callback latch, for the same reason: it describes THIS call` |
|        - | 6282 | `	 * (an internal function invoking a userland callback binds its arguments weakly),` |
|        - | 6283 | `	 * and a call the callback body makes must not inherit it. */` |
|  7714759 | 6284 | `	int bCallbackWeak = pVm->bCallbackWeak;` |
|        - | 6285 | `	/* ...and whether this call's ANSWER is thrown away, which is what a` |
|        - | 6286 | `	 * #[\NoDiscard] callee warns about. The compiled call site says so directly` |
|        - | 6287 | `	 * (bDiscard, stamped where the statement pops the value: php's` |
|        - | 6288 | `	 * !RETURN_VALUE_USED); php also propagates the same bit THROUGH` |
|        - | 6289 | ``	 * `call_user_func()`/`call_user_func_array()`, which is why`` |
|        - | 6290 | ``	 * `call_user_func('f');` warns and `array_map('f', $a);` does not, and a`` |
|        - | 6291 | `	 * synthetic OP_CALL has no call site of its own to carry it -- so the two` |
|        - | 6292 | `	 * builtins hand it over on this latch. Consumed here like the rest. */` |
|  7714759 | 6293 | `	int bResultDropped = pVm->bDiscardCallback ? 1 : (pInstr->bDiscard != 0);` |
|        - | 6294 | `	/* php's forwarding of that bit through call_user_func() is a COMPILE-time` |
|        - | 6295 | ``	 * special case on the literal name, so `$g = "call_user_func"; $g("f");` does`` |
|        - | 6296 | `	 * not forward. The callee slot's nIdx says which spelling this is: a compiled` |
|        - | 6297 | `	 * literal is marked constant, a variable read is not. */` |
|  7714759 | 6298 | `	int bLiteralCallee = (pTos->nIdx == SXU32_HIGH);` |
|  7714759 | 6299 | `	pVm->bDiscardCallback = 0;` |
|  7714759 | 6300 | `	pVm->bMagicDispatch = 0;` |
|  7714759 | 6301 | `	pVm->bClosureScreened = 0;` |
|  7714759 | 6302 | `	pVm->bCallbackWeak = 0;` |
|  7714759 | 6303 | `	pTos->iFlags &= ~MEMOBJ_AUX_MEMBERCALL;` |
|  7714759 | 6304 | `	pArg = &pTos[-nCallArgs];` |
|        - | 6305 | `	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED` |
|        - | 6306 | `	 * arguments, and a spread expanding to !=1 element shifts the actual positions` |
|        - | 6307 | `	 * of any following compile-time named args. The effective per-actual-slot name` |
|        - | 6308 | `	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)` |
|        - | 6309 | `	 * is built PER PATH against that path's finalized arg base — a method call pops` |
|        - | 6310 | `	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch` |
|        - | 6311 | `	 * site rather than built once here. */` |
|        - | 6312 | `	VmCallArgMap sEffMap;` |
|  7714759 | 6313 | `	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;` |
|        - | 6314 | `	SyHashEntry *pEntry;` |
|        - | 6315 | `	/* What this call SITE remembered about its callee, consulted once and used by` |
|        - | 6316 | `	 * whichever of the two dispatch branches the answer belongs to. */` |
|  7714759 | 6317 | `	SyHashEntry *pSiteEntry = 0;` |
|  7714759 | 6318 | `	int bSiteHost = 0;` |
|        - | 6319 | `	SyString sName;` |
|        - | 6320 | `	/* The host-call trio, hoisted out of the foreign-function branch below so the` |
|        - | 6321 | `	 * NativeCall label there can also be reached from the compiled-function branch:` |
|        - | 6322 | `	 * a VM_FUNC_NATIVE method is a C body wearing a method's clothes, and it runs on` |
|        - | 6323 | `	 * exactly the foreign path (no frame, no call record, no operand stack) once` |
|        - | 6324 | `	 * $this and the called class have been resolved the method way. Jumping into` |
|        - | 6325 | `	 * that branch would otherwise cross these declarations. */` |
|        - | 6326 | `	ph7_user_func *pFunc;` |
|        - | 6327 | `	ph7_context sCtx;` |
|        - | 6328 | `	ph7_value sRet;` |
|        - | 6329 | `	/* Native-METHOD call state; all 0 for a plain host function, which is what the` |
|        - | 6330 | `	 * foreign branch's own fallthrough leaves.` |
|        - | 6331 | `	 *   pNativeOwned — the reference the method path took, released at the shared tail` |
|        - | 6332 | `	 *   pNativeRecv  — what the body actually sees as $this (0 for a static method)` |
|        - | 6333 | `	 *   pNativeClass — the late-static-binding target */` |
|  7714759 | 6334 | `	ph7_class_instance *pNativeOwned = 0;` |
|  7714759 | 6335 | `	ph7_class_instance *pNativeRecv = 0;` |
|  7714759 | 6336 | `	ph7_class *pNativeClass = 0;` |
|        - | 6337 | `	/* The engine's own __call/__callStatic routing: the OP_MEMBER immediately below this` |
|        - | 6338 | `	 * call found a missing (or inaccessible) method on a class declaring the magic handler` |
|        - | 6339 | `	 * and MARKED this callee slot, latching {receiver, class, original name} on the VM.` |
|        - | 6340 | `	 * There is no callable here at all — the mark selects the packing body directly, ahead` |
|        - | 6341 | `	 * of every callable decode below, and the record it dispatches carries no PHP name (it` |
|        - | 6342 | `	 * is not in hHostFunction). This is what replaced writing the string` |
|        - | 6343 | `	 * "__phl_magic_call" into the slot and letting the name lookup find a hidden global.` |
|        - | 6344 | ``	 * Everything from `NativeCall` down is shared with an ordinary builtin call, which is`` |
|        - | 6345 | `	 * what this has always been from the executor's point of view. */` |
|  7714759 | 6346 | `	if( pTos->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|        - | 6347 | `		/* Move the routing off the carrier and onto the VM, HERE — one instruction` |
|        - | 6348 | `		 * before the packing body reads it, with nothing in between that could set` |
|        - | 6349 | `		 * another. OP_MEMBER used to publish it directly, which only held while the` |
|        - | 6350 | `		 * arguments ran before it; now they run after, and a routed call inside this` |
|        - | 6351 | `		 * one's argument list has already come and gone. */` |
|      129 | 6352 | `		VmMagicCall *pPend = (VmMagicCall *)pTos->x.pOther;` |
|      129 | 6353 | `		pTos->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|      129 | 6354 | `		pTos->x.pOther = 0;` |
|      129 | 6355 | `		pVm->pMagicCallThis = pPend ? pPend->pRecv : 0;` |
|      129 | 6356 | `		pVm->pMagicCallClass = pPend ? pPend->pClass : 0;` |
|      129 | 6357 | `		SyBlobReset(&pVm->sMagicCallName);` |
|      129 | 6358 | `		if( pPend && SyBlobLength(&pPend->sName) > 0 ){` |
|      192 | 6359 | `			SyBlobAppend(&pVm->sMagicCallName,SyBlobData(&pPend->sName),` |
|       63 | 6360 | `				SyBlobLength(&pPend->sName));` |
|       63 | 6361 | `		}` |
|      129 | 6362 | `		if( pPend ){` |
|        - | 6363 | `			/* The receiver reference the record held is now the VM's, which` |
|        - | 6364 | `			 * VmMagicCallDispatch gives back — so drop the record without unref'ing. */` |
|      129 | 6365 | `			pPend->pRecv = 0;` |
|      129 | 6366 | `			VmFreeMagicCall(pPend);` |
|       63 | 6367 | `		}` |
|      129 | 6368 | `		pFunc = PH7_VmMagicCallFunc(&(*pVm));` |
|      129 | 6369 | `		if( pFunc == 0 ){` |
|      ! 0 | 6370 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 6371 | `			goto Abort;` |
|        - | 6372 | `		}` |
|        - | 6373 | `		/* D1: the packing body declares no by-ref parameter (php hands __call a packed` |
|        - | 6374 | `		 * ARRAY), so every deferred argument materializes by value, exactly as it did` |
|        - | 6375 | `		 * through the named trampoline's zero by-ref mask. */` |
|        - | 6376 | `		{` |
|      129 | 6377 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffCallMap);` |
|      129 | 6378 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6379 | `		}` |
|      192 | 6380 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|      126 | 6381 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|      129 | 6382 | `		goto NativeCall;` |
|        - | 6383 | `	}` |
|        - | 6384 | `	/* A Closure object is callable: unwrap it to its underlying string callable so the` |
|        - | 6385 | `	 * dispatch below handles it (rather than treating it as a generic object and looking` |
|        - | 6386 | `	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure` |
|        - | 6387 | `	 * so a plain __invoke object skips the temp-value work entirely. */` |
|  7714633 | 6388 | `	if( VmValueIsClosure(pVm,pTos) ){` |
|        - | 6389 | `		ph7_value sCallable;` |
|    20890 | 6390 | `		PH7_MemObjInit(pVm,&sCallable);` |
|    20890 | 6391 | `		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){` |
|    20890 | 6392 | `			PH7_MemObjRelease(pTos);` |
|    20890 | 6393 | `			PH7_MemObjStore(&sCallable,pTos);` |
|        - | 6394 | `` 			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]` `` |
|        - | 6395 | `			 * name, which the lookup below refuses to a name a SCRIPT spelled. */` |
|    20890 | 6396 | `			bEngineCallee = 1;` |
|    10283 | 6397 | `		}` |
|    20890 | 6398 | `		PH7_MemObjRelease(&sCallable);` |
|    10283 | 6399 | `	}` |
|        - | 6400 | `	/* Extract function name */` |
|  7714633 | 6401 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|   304271 | 6402 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 6403 | `			ph7_value sResult;` |
|        - | 6404 | `			sxi32 rcArr;` |
|        - | 6405 | `			/* Taken off the VM at the head of the shape check below, not at the dispatch:` |
|        - | 6406 | `			 * everything between the two (the deferred-argument materialization especially)` |
|        - | 6407 | `			 * can throw and jump out of this branch, and a latch left armed would stand the` |
|        - | 6408 | `			 * visibility screen down for whatever call runs next. */` |
|        - | 6409 | `			int bCbScreened;` |
|        - | 6410 | `			{` |
|        - | 6411 | `				/* php validates the SHAPE of an array callable first: it must hold exactly` |
|        - | 6412 | `				 * two elements. PH7 handed any array to the dispatcher, which failed` |
|        - | 6413 | ``				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */`` |
|   100352 | 6414 | `				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - | 6415 | `				char zCbMsg[192];` |
|   100352 | 6416 | `				const char *zCbErr = 0;` |
|   100352 | 6417 | `				int bCbRaised = 0; /* the class lookup ran an autoloader that threw */` |
|        - | 6418 | `				/* A pair the closure UNWRAP just built is not an array the program wrote: its` |
|        - | 6419 | `				 * callee was resolved and screened where the closure was BUILT, the way php` |
|        - | 6420 | `				 * resolves one, and it is a well-formed [target, method] by construction.` |
|        - | 6421 | `				 * Re-deciding it here, against the CALLER, is what refused an escaped` |
|        - | 6422 | ``				 * `$this->priv(...)` php runs. */`` |
|   100352 | 6423 | `				bCbScreened = pVm->bClosureScreened;` |
|   100352 | 6424 | `				pVm->bClosureScreened = 0; /* put back for the one dispatch that reads it */` |
|   100352 | 6425 | `				if( !bCbScreened && pCbMap && pCbMap->nEntry == 2 ){` |
|        - | 6426 | `					/* Shape is right; now check it actually RESOLVES. The shared dispatcher` |
|        - | 6427 | `					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for` |
|        - | 6428 | `					 * an unresolvable [class,method] pair -- silence a caller cannot detect --` |
|        - | 6429 | `					 * and its contract is relied on by call_user_func/usort, so the throw` |
|        - | 6430 | `					 * belongs here at the call site. */` |
|   100202 | 6431 | `					ph7_value *pCbCls = 0;` |
|   100202 | 6432 | `					ph7_value *pCbMeth = 0;` |
|        - | 6433 | `					/* php reads the INTEGER indices 0 and 1, not the first two entries in` |
|        - | 6434 | `					 * insertion order. Two entries at other keys are a SHAPE error of their` |
|        - | 6435 | `					 * own ("has to contain indices 0 and 1"), distinct from the` |
|        - | 6436 | ``					 * wrong-element-COUNT message below; `[1=>'m',0=>'C']` holds both, so`` |
|        - | 6437 | `					 * the target is index 0 -- the reverse of what PH7 walked. */` |
|   100202 | 6438 | `					if( !PH7_VmArrayCallableParts(&(*pVm),pCbMap,&pCbCls,&pCbMeth) ){` |
|       11 | 6439 | `						zCbErr = "Array callback has to contain indices 0 and 1";` |
|        6 | 6440 | `					}else{` |
|        - | 6441 | `						/* The resolution below can run an autoloader that throws; when it does,` |
|        - | 6442 | `						 * php propagates THAT exception and never reports the class missing. */` |
|   100192 | 6443 | `						sxi32 nCbBrc = pVm->nBoundaryRc;` |
|   100192 | 6444 | `						const void *pCbRes = (const void *)pVm->pResumeFrame;` |
|   150286 | 6445 | `						zCbErr = VmDirectArrayCallableError(&(*pVm),pCbCls,pCbMeth,` |
|    50094 | 6446 | `							zCbMsg,sizeof(zCbMsg));` |
|   100192 | 6447 | `						if( zCbErr && PH7_VmClassLookupRaised(&(*pVm),nCbBrc,pCbRes) ){` |
|        6 | 6448 | `							bCbRaised = 1;` |
|        2 | 6449 | `						}` |
|        - | 6450 | `					}` |
|    50099 | 6451 | `				}` |
|   100352 | 6452 | `				if( !bCbScreened && (pCbMap == 0 \|\| pCbMap->nEntry != 2 \|\| zCbErr) ){` |
|        - | 6453 | `					sxi32 rcCb;` |
|       87 | 6454 | `					if( pInstr->iP2 ){` |
|      ! 0 | 6455 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 6456 | `					}` |
|       87 | 6457 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 6458 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6459 | `					}` |
|       87 | 6460 | `					PH7_MemObjRelease(pTos);` |
|       87 | 6461 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       87 | 6462 | `					pTos->nIdx = SXU32_HIGH;` |
|       87 | 6463 | `					if( bCbRaised ){` |
|        - | 6464 | `						/* Land the autoloader's own throw: consume the parked status (an` |
|        - | 6465 | `						 * in-place catch leaves 0 behind plus a recorded resume frame, which` |
|        - | 6466 | `						 * the router below picks up). */` |
|        6 | 6467 | `						rcCb = pVm->nBoundaryRc;` |
|        6 | 6468 | `						pVm->nBoundaryRc = 0;` |
|        6 | 6469 | `						if( rcCb == PH7_ABORT ){ goto Abort; }` |
|        6 | 6470 | `						rc = PH7_EXCEPTION;` |
|       18 | 6471 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6472 | `					}` |
|       82 | 6473 | `					if( zCbErr == 0 ){` |
|       17 | 6474 | `						zCbErr = "Array callback must have exactly two elements";` |
|        8 | 6475 | `					}` |
|       82 | 6476 | `					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));` |
|       82 | 6477 | `					if( rcCb == SXERR_ABORT ){ goto Abort; }` |
|       82 | 6478 | `					rc = rcCb;` |
|        - | 6479 | `					/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 6480 | `					 * (SXRET_OK + recorded resume) FELL THROUGH and kept dispatching` |
|        - | 6481 | `					 * the malformed callable inside the try. Route like OP_THROW. */` |
|      102 | 6482 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6483 | `				}` |
|        - | 6484 | `			}` |
|        - | 6485 | `			/* Build the effective spread-key map (and consume this call's runs)` |
|        - | 6486 | `			 * against this path's arg base (the array-callable slot isn't popped). */` |
|   150399 | 6487 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100264 | 6488 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6489 | `			/* Materialize the deferred arguments against the pair's own method (see` |
|        - | 6490 | `			 * VmIndirectCalleeFunc), not against a blanket by-ref assumption. */` |
|        - | 6491 | `			{` |
|   100267 | 6492 | `				sxi32 rcDA = VmResolveIndirectArgs(&(*pVm),pTos,pArg,pTos,pEffCallMap);` |
|   100267 | 6493 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6494 | `			}` |
|   100267 | 6495 | `			SySetReset(&aArg);` |
|   100463 | 6496 | `			while( pArg < pTos ){` |
|      198 | 6497 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      198 | 6498 | `				pArg++;` |
|        2 | 6499 | `			}` |
|   100267 | 6500 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 6501 | `			/* May be a class instance and it's static method. Forward this call's named-arg map` |
|        - | 6502 | ``			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —`` |
|        - | 6503 | `			 * mirroring the __invoke-object branch below. */` |
|   100267 | 6504 | `			pVm->bClosureScreened = bCbScreened; /* see the capture above */` |
|        - | 6505 | `			/* This call SITE is what decides the answer is dropped, and the` |
|        - | 6506 | `			 * dispatch below builds a synthetic OP_CALL that has no site of its` |
|        - | 6507 | `			 * own — hand the bit over on the latch, so a #[\NoDiscard] callee` |
|        - | 6508 | `			 * reached through an array callable, a "C::m" string or __invoke` |
|        - | 6509 | `			 * warns exactly as a directly-spelled one does. */` |
|   100267 | 6510 | `			pVm->bDiscardCallback = bResultDropped;` |
|   100267 | 6511 | `			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|        - | 6512 | `			/* Both latches are consumed by the method OP_CALL this dispatch builds;` |
|        - | 6513 | `			 * clear them here for the paths that never reach one. */` |
|   100267 | 6514 | `			pVm->bClosureScreened = 0;` |
|   100267 | 6515 | `			pVm->bDiscardCallback = 0;` |
|   100267 | 6516 | `			SySetReset(&aArg);` |
|        - | 6517 | `			/* Pop given arguments */` |
|   100267 | 6518 | `			if( nCallArgs > 0 ){` |
|      172 | 6519 | `				VmPopOperand(&pTos,nCallArgs);` |
|       85 | 6520 | `			}` |
|   100267 | 6521 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 | 6522 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 6523 | `				goto Abort;` |
|        - | 6524 | `			}` |
|   100267 | 6525 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - | 6526 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - | 6527 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - | 6528 | `				sxi32 iResumePc;` |
|   100014 | 6529 | `				PH7_MemObjRelease(&sResult);` |
|   100014 | 6530 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100004 | 6531 | `					PH7_MemObjRelease(pTos);` |
|        - | 6532 | ``					/* Drain the abandoned outer-expression operands (`1 + $cb()`)`` |
|        - | 6533 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 6534 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300004 | 6535 | `					PH7_RESUME_DRAIN()` |
|   100004 | 6536 | `					pc = iResumePc;` |
|   100004 | 6537 | `					break;` |
|        - | 6538 | `				}` |
|       11 | 6539 | `				goto Exception;` |
|        - | 6540 | `			}` |
|        - | 6541 | `			/* Copy result */` |
|      255 | 6542 | `			PH7_MemObjStore(&sResult,pTos);` |
|      255 | 6543 | `			PH7_MemObjRelease(&sResult);` |
|   204049 | 6544 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - | 6545 | ``			/* An __invoke object is a METHOD CALL wearing one slot: `$o(...)` is`` |
|        - | 6546 | ``			 * `$o->__invoke(...)` with the name resolved by the ENGINE rather than`` |
|        - | 6547 | `			 * spelled by the source. So give it the layout OP_MEMBER leaves for a` |
|        - | 6548 | `			 * spelled method call -- [args..., receiver, method vm-name] -- and let` |
|        - | 6549 | `			 * the one dispatch below run it, on the stage-2 trampoline like every` |
|        - | 6550 | `			 * other PHP->PHP call.` |
|        - | 6551 | `			 *` |
|        - | 6552 | `			 * It used to be handed to VmCallObjectInvoke, which builds a synthetic` |
|        - | 6553 | `			 * OP_CALL and a fresh VmByteCodeExec: one real C activation per call,` |
|        - | 6554 | `			 * and two user-visible consequences.` |
|        - | 6555 | `			 *` |
|        - | 6556 | `			 *   - Recursion through an invokable object died at nMaxNativeDepth` |
|        - | 6557 | ``			 *     (256) where php runs to memory: `$i()` calling `($this)()` hit`` |
|        - | 6558 | `			 *     "Maximum native nesting depth reached" at 256 and php reached` |
|        - | 6559 | `			 *     20000. A plain closure, a method and a static call were all` |
|        - | 6560 | `			 *     already iterative; only this shape was not.` |
|        - | 6561 | ``			 *   - A Fiber::suspend() reached through `$o(...)` was refused with`` |
|        - | 6562 | `			 *     "Cannot suspend across an internal call boundary", because the` |
|        - | 6563 | `			 *     fiber body's nBodyExecDepth and the suspend's nVmExecDepth no` |
|        - | 6564 | `			 *     longer matched. The trampoline PARKS a nested PHP call` |
|        - | 6565 | `			 *     (pParkedSegment); a native re-entry it cannot. That is the whole` |
|        - | 6566 | `			 *     of what stopped phpstan's FiberNodeScopeResolver, whose fiber body` |
|        - | 6567 | `			 *     calls a ClassStatementsGatherer OBJECT and suspends inside it.` |
|        - | 6568 | `			 *` |
|        - | 6569 | `			 * call_user_func(), array_map() and the rest of the C-callback doors` |
|        - | 6570 | `			 * still reach __invoke through VmCallObjectInvoke, and those ARE the` |
|        - | 6571 | `			 * internal boundaries php's fibers cross and PHL's do not (PLAN.md,` |
|        - | 6572 | `			 * "Generators / fibers"). This changes only the dispatch a call site` |
|        - | 6573 | ``			 * SPELLS as `$o(...)`. */`` |
|   203907 | 6574 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|   203907 | 6575 | `			ph7_class_method *pInvMeth = pThis` |
|   203902 | 6576 | `				? PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) : 0;` |
|   203907 | 6577 | `			if( pInvMeth ){` |
|        - | 6578 | `				/* The compiler sized this body for a ONE-slot callee here, so the` |
|        - | 6579 | `				 * name slot is one more than it budgeted. Ask for it properly rather` |
|        - | 6580 | `				 * than spend VM_STACK_GUARD's slack: the growth is a no-op whenever` |
|        - | 6581 | `				 * the slack is there (which is every ordinary call), and the slot is` |
|        - | 6582 | `				 * released a few lines below by the method branch's own pop, so at` |
|        - | 6583 | `				 * most one is ever outstanding. */` |
|   103903 | 6584 | `				ph7_value *pInvOldBase = pStack;` |
|   155852 | 6585 | `				if( !VmGrowOperandStack(pVm,(sxu32)(pTos - pStack) + 2,` |
|        - | 6586 | `				                        &pStack,&pTos,&sState,` |
|    51949 | 6587 | `				                        pCallTop,ppBaseOwner,pnBaseCap) ){` |
|      ! 0 | 6588 | `					PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 6589 | `					goto Abort;` |
|        - | 6590 | `				}` |
|   103903 | 6591 | `				if( pStack != pInvOldBase ){` |
|        - | 6592 | `					/* The buffer moved: the watermark is a pointer INTO it. Unlike` |
|        - | 6593 | `					 * OP_SPREAD (which re-anchors at the whole capacity), keep it tight —` |
|        - | 6594 | `					 * nLiveTos is what the recycle sweep walks, and this runs on every` |
|        - | 6595 | ``					 * `$o(...)`. */`` |
|     2101 | 6596 | `					pHigh = pStack + (pHigh - pInvOldBase);` |
|     1939 | 6597 | `				}` |
|   103903 | 6598 | `				pTos++;` |
|   103903 | 6599 | `				if( pTos > pHigh ){` |
|        - | 6600 | `					/* The name slot is above this activation's high-water mark until the` |
|        - | 6601 | `					 * next fetch point raises it, and an exit between here and the pop` |
|        - | 6602 | `					 * below (an unresolvable callee, the recursion cap) would leave its` |
|        - | 6603 | `					 * blob unswept. */` |
|       44 | 6604 | `					pHigh = pTos;` |
|       20 | 6605 | `				}` |
|   103903 | 6606 | `				PH7_MemObjRelease(pTos);` |
|   155852 | 6607 | `				SyBlobAppend(&pTos->sBlob,(const void *)SyStringData(&pInvMeth->sVmName),` |
|    51949 | 6608 | `					SyStringLength(&pInvMeth->sVmName));` |
|   103903 | 6609 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - | 6610 | `				/* The engine's own function-table key, not a name the program spelled --` |
|        - | 6611 | `				 * the mark VmCallClassMethodLsb writes on its synthetic callee slot, and` |
|        - | 6612 | `				 * what lets PH7_VmGetUserFunction answer for it. */` |
|   103903 | 6613 | `				pTos->iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|   103903 | 6614 | `				pTos->nIdx = SXU32_HIGH;` |
|   103903 | 6615 | `				pArg = &pTos[-1-nCallArgs];` |
|   103903 | 6616 | `				bEngineCallee = 1;` |
|        - | 6617 | ``				/* php dispatches a non-public __invoke through `$o()` from any scope`` |
|        - | 6618 | `				 * (it only WARNS at the declaration) -- the second half of what` |
|        - | 6619 | `				 * VmCallObjectInvoke stated, with pVm->bMagicDispatch. */` |
|   103903 | 6620 | `				bMagicDispatch = 1;` |
|   103903 | 6621 | `				goto CalleeByName;` |
|        - | 6622 | `			}` |
|        - | 6623 | `			{` |
|        - | 6624 | `				/* No __invoke: php's catchable Error, named after the class.` |
|        - | 6625 | `				 * Building the effective map is what CONSUMES this call's captured` |
|        - | 6626 | `				 * unpack runs, which a refused callee owes just as a dispatched one` |
|        - | 6627 | `				 * does — the map itself is never read from here. */` |
|   150008 | 6628 | `				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100004 | 6629 | `					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6630 | `				/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable` |
|        - | 6631 | `				 * slot itself (it pops top-down and the callable IS pTos), which for a` |
|        - | 6632 | `				 * temporary like (new Plain())(...) holds the only reference — popping it` |
|        - | 6633 | `				 * would free pThis before VmRaiseNotCallable reads its class name. */` |
|   100006 | 6634 | `				if( pThis ){` |
|   100006 | 6635 | `					pThis->iRef++;` |
|    50002 | 6636 | `				}` |
|   100006 | 6637 | `				if( nCallArgs > 0 ){` |
|      ! 0 | 6638 | `					VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6639 | `				}` |
|   100006 | 6640 | `				PH7_MemObjRelease(pTos);` |
|   100006 | 6641 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|   100006 | 6642 | `				pTos->nIdx = SXU32_HIGH;` |
|   100006 | 6643 | `				if( pThis == 0 ){` |
|        - | 6644 | `					/* An object slot with no instance behind it: nothing to name. */` |
|      ! 0 | 6645 | `					sxi32 rcNi = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 6646 | `						"Value of type object is not callable",` |
|        - | 6647 | `						sizeof("Value of type object is not callable")-1);` |
|      ! 0 | 6648 | `					if( rcNi == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 6649 | `					rc = rcNi;` |
|      ! 0 | 6650 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6651 | `				}` |
|   100006 | 6652 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|   100006 | 6653 | `				PH7_ClassInstanceUnref(pThis);` |
|   100006 | 6654 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 6655 | `					goto Abort;` |
|        - | 6656 | `				}` |
|        - | 6657 | `				{` |
|        - | 6658 | `					sxi32 iRp;` |
|   100006 | 6659 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|        - | 6660 | `						/* Drain the abandoned outer-expression operands` |
|        - | 6661 | ``						 * (`1 + $plainObj()`) to the try's base — one leaked`` |
|        - | 6662 | `						 * slot per caught throw otherwise. */` |
|   300010 | 6663 | `						PH7_RESUME_DRAIN()` |
|   100006 | 6664 | `						pc = iRp;` |
|   100006 | 6665 | `						break;` |
|        - | 6666 | `					}` |
|        - | 6667 | `				}` |
|      ! 0 | 6668 | `				goto Exception;` |
|        - | 6669 | `			}` |
|      ! 0 | 6670 | `		}else{` |
|        - | 6671 | `			/* php: calling a non-callable is a catchable Error naming the type` |
|        - | 6672 | `			 * ("Value of type int is not callable"), or -- for an array -- the shape it` |
|        - | 6673 | `			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so` |
|        - | 6674 | ``			 * `$x()` on a number quietly evaluated to nothing. */`` |
|        - | 6675 | `			sxi32 rcNc;` |
|        - | 6676 | `			char zMsg[128];` |
|       17 | 6677 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 6678 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");` |
|      ! 0 | 6679 | `			}else{` |
|       25 | 6680 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",` |
|        8 | 6681 | `					VmArithTypeName(pTos));` |
|        - | 6682 | `			}` |
|        - | 6683 | `			/* Consume this call's captured spread runs — a non-callable target` |
|        - | 6684 | `			 * (int/float/bool/null) reaches no dispatch build site. */` |
|       17 | 6685 | `			if( pInstr->iP2 ){` |
|      ! 0 | 6686 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 6687 | `			}` |
|        - | 6688 | `			/* Pop given arguments */` |
|       17 | 6689 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 6690 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6691 | `			}` |
|        - | 6692 | `			/* Settle the call's result slot BEFORE throwing. */` |
|       17 | 6693 | `			PH7_MemObjRelease(pTos);` |
|       17 | 6694 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       17 | 6695 | `			pTos->nIdx = SXU32_HIGH;` |
|       17 | 6696 | `			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       17 | 6697 | `			if( rcNc == SXERR_ABORT ){ goto Abort; }` |
|       17 | 6698 | `			rc = rcNc;` |
|        - | 6699 | `			/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 6700 | `			 * (SXRET_OK + recorded resume) FELL THROUGH and resumed the try body` |
|        - | 6701 | `			 * right after the failed call. Route like OP_THROW. */` |
|       27 | 6702 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6703 | `		}` |
|      255 | 6704 | `		break;` |
|        - | 6705 | `	}` |
|        - | 6706 | `	/* The callee is a NAME in pTos, with its arguments (and, for a method, its` |
|        - | 6707 | `	 * receiver) below it. Reached by falling through from the callable decode` |
|        - | 6708 | `	 * above, and jumped to by the __invoke-object branch, which builds exactly` |
|        - | 6709 | `	 * the two-slot method shape and lands here. */` |
|  3705424 | 6710 | `CalleeByName:` |
|  7514265 | 6711 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 6712 | `	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just` |
|        - | 6713 | `	 * anchors it to the global namespace — strip it before resolving so a` |
|        - | 6714 | ``	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */`` |
|  7514265 | 6715 | `	if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|       15 | 6716 | `		sName.zString++;` |
|       15 | 6717 | `		sName.nByte--;` |
|        7 | 6718 | `	}` |
|        - | 6719 | `	/* Ask this call SITE what its callee name meant last time. Resolving it costs up` |
|        - | 6720 | `	 * to four case-insensitive hash lookups -- the user table under the qualified name` |
|        - | 6721 | `	 * and then the global one, then the same pair of the host table -- and on a phpcs` |
|        - | 6722 | `	 * run that was 9.2% of everything the engine did. The site's answer is stamped with` |
|        - | 6723 | `	 * pVm->nCallableGen, so declaring anything retires all of them at once; see` |
|        - | 6724 | `	 * VmCallSite. */` |
|  7514265 | 6725 | `	pSiteEntry = PH7_VmCallSiteAnswer(pVm,pInstr,&sName,bEngineCallee,&bSiteHost);` |
|  7514265 | 6726 | `	if( pSiteEntry ){` |
|  4064575 | 6727 | `		pEntry = bSiteHost ? 0 : pSiteEntry;` |
|  2032856 | 6728 | `	}else` |
|        - | 6729 | `	{` |
|        - | 6730 | `	/* Check for a compiled function first.` |
|        - | 6731 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 6732 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|  3449695 | 6733 | `	pEntry = PH7_VmGetUserFunction(pVm,(const void *)sName.zString,sName.nByte,bEngineCallee);` |
|        - | 6734 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - | 6735 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - | 6736 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - | 6737 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - | 6738 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - | 6739 | `	{` |
|  3449695 | 6740 | `	VmCallArgMap *pCallMap = pEffCallMap;` |
|  3449695 | 6741 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - | 6742 | `		const char *zFunc;` |
|        - | 6743 | `		const char *zEnd;` |
|        - | 6744 | `		const char *z;` |
|        - | 6745 | `		SyString sGlobal;` |
|       89 | 6746 | `		zFunc = sName.zString;` |
|       89 | 6747 | `		zEnd  = zFunc + sName.nByte;` |
|       89 | 6748 | `		z = zEnd;` |
|        - | 6749 | `		/* Find last namespace separator */` |
|      823 | 6750 | `		while( z > zFunc ){` |
|      823 | 6751 | `			if( z[-1] == '\\' ){` |
|       89 | 6752 | `				break;` |
|        - | 6753 | `			}` |
|      739 | 6754 | `			z--;` |
|        5 | 6755 | `		}` |
|       89 | 6756 | `		if( z > zFunc && z < zEnd ){` |
|        - | 6757 | `			/* Retry lookup using the unqualified/global function name */` |
|       89 | 6758 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       89 | 6759 | `			pEntry = PH7_VmGetUserFunction(pVm,(const void *)sGlobal.zString,sGlobal.nByte,bEngineCallee);` |
|       42 | 6760 | `		}` |
|       42 | 6761 | `	}` |
|        - | 6762 | `	} /* end VmCallArgMap namespace scope */` |
|        - | 6763 | `	/* A user-table hit is the whole answer; a MISS is not (the host branch below` |
|        - | 6764 | `	 * resolves the rest), so only the hit is recorded here. */` |
|  3449695 | 6765 | `	PH7_VmCallSiteRecord(pVm,pInstr,&sName,bEngineCallee,0,pEntry);` |
|        - | 6766 | `	} /* end call-site cache miss */` |
|  7514265 | 6767 | `	if( pEntry ){` |
|        - | 6768 | `		ph7_vm_func_arg *aFormalArg;` |
|        - | 6769 | `		ph7_class_instance *pThis;` |
|        - | 6770 | `		ph7_value *pFrameStack;` |
|        - | 6771 | `		ph7_vm_func *pVmFunc;` |
|        - | 6772 | `		ph7_class *pSelf;` |
|        - | 6773 | `		ph7_class *pSelfHint;` |
|        - | 6774 | `		VmFrame *pFrame;` |
|        - | 6775 | `		ph7_value *pObj;` |
|        - | 6776 | `		VmSlot sArg;` |
|        - | 6777 | `		sxu32 n;` |
|  2377719 | 6778 | `		sxi32 iArgPreFlags = 0; /* the actual's type before its declared-type check */` |
|  2377719 | 6779 | `		int bClosureThis = 0;` |
|  2377719 | 6780 | `		ph7_class *pClosureScope = 0;` |
|        - | 6781 | `		/* initialize fields */` |
|  2377719 | 6782 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|  2377719 | 6783 | `		pThis = 0;` |
|  2377719 | 6784 | `		pSelf = 0;` |
|        - | 6785 | `		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it` |
|        - | 6786 | `		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever` |
|        - | 6787 | `		 * set for a bound plain closure, which dispatches as a function, so the method branch below` |
|        - | 6788 | `		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */` |
|  2377719 | 6789 | `		if( pVm->pClosureThis ){` |
|       78 | 6790 | `			pThis = pVm->pClosureThis;` |
|       78 | 6791 | `			pVm->pClosureThis = 0;` |
|       78 | 6792 | `			bClosureThis = 1;` |
|       37 | 6793 | `		}` |
|  2377719 | 6794 | `		if( pVm->pClosureScope ){` |
|        - | 6795 | `			/* May ride alongside a bound $this, or stand alone for a` |
|        - | 6796 | ``			 * scope-only rebind (`bindTo(null, Scope::class)`). */`` |
|       70 | 6797 | `			pClosureScope = pVm->pClosureScope;` |
|       70 | 6798 | `			pVm->pClosureScope = 0;` |
|       33 | 6799 | `		}` |
|  2377719 | 6800 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - | 6801 | `			ph7_class_method *pMeth;` |
|        - | 6802 | `			/* Class method call */` |
|  2076525 | 6803 | `			ph7_value *pTarget = &pTos[-1];` |
|  2076525 | 6804 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - | 6805 | `				/* Extract the 'this' pointer */` |
|  2076525 | 6806 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - | 6807 | `					/* Instance already loaded */` |
|  1973426 | 6808 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|  1973426 | 6809 | `					pThis->iRef++;` |
|  1973426 | 6810 | `					pSelf = pThis->pClass;` |
|   986628 | 6811 | `				}` |
|  2076525 | 6812 | `				if( pSelf == 0 ){` |
|   103104 | 6813 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - | 6814 | `						/* "Late Static Binding" class name */` |
|   154634 | 6815 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|    51538 | 6816 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|    51538 | 6817 | `					}` |
|   103104 | 6818 | `					if( pSelf == 0 ){` |
|       10 | 6819 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        4 | 6820 | `					}` |
|    51542 | 6821 | `				}` |
|  2076525 | 6822 | `				if( pThis == 0  ){` |
|   103104 | 6823 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|   103104 | 6824 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   103104 | 6825 | `					if( pFrameLocal->pParent ){` |
|        - | 6826 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|      909 | 6827 | `						pThis = pFrameLocal->pThis;` |
|      909 | 6828 | `						if( pThis ){` |
|       76 | 6829 | `							pThis->iRef++;` |
|       36 | 6830 | `						}` |
|      452 | 6831 | `					}` |
|    51542 | 6832 | `				}` |
|  2076525 | 6833 | `				VmPopOperand(&pTos,1);` |
|  2076525 | 6834 | `				PH7_MemObjRelease(pTos);` |
|        - | 6835 | `				/* Synchronize pointers. The method-name slot popped above sat BETWEEN` |
|        - | 6836 | `				 * this call's arguments and the (already-removed) target — so only now` |
|        - | 6837 | `				 * is pTos one past the last actual argument. Re-derive the unpack` |
|        - | 6838 | `				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs` |
|        - | 6839 | `				 * from run ends, which the extra target slot would otherwise offset,` |
|        - | 6840 | ``				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */`` |
|  2076525 | 6841 | `				nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);` |
|  2076525 | 6842 | `				pArg = &pTos[-nCallArgs];` |
|        - | 6843 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - | 6844 | `				 * user have already computed the random generated unique class method name` |
|        - | 6845 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - | 6846 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - | 6847 | `				 */` |
|  2076525 | 6848 | `				while( pArg < pStack ){` |
|      ! 0 | 6849 | `					pArg++;` |
|      ! 0 | 6850 | `				}` |
|  2076525 | 6851 | `				if( pSelf && pVm->bReflectBypass ){` |
|        - | 6852 | `					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection` |
|        - | 6853 | `					 * ignores visibility. Consume-once so nested calls made by the` |
|        - | 6854 | `					 * invoked body are checked normally. */` |
|     1438 | 6855 | `					pVm->bReflectBypass = 0;` |
|      720 | 6856 | `				}else` |
|  2075092 | 6857 | `				if( pSelf && bMagicDispatch && PH7_MagicMethodMustBePublic(&pVmFunc->sName) ){` |
|        - | 6858 | `					/* The ENGINE reaching for a magic method php requires to be public.` |
|        - | 6859 | `					 * php only WARNS at such a declaration and then dispatches the method` |
|        - | 6860 | ``					 * regardless -- the engine calling `__get` is not the outside world`` |
|        - | 6861 | `					 * reaching for a private member -- so a non-public` |
|        - | 6862 | ``					 * `__get`/`__call`/`__invoke`/`__sleep`/... still runs, where PHL threw`` |
|        - | 6863 | ``					 * `Call to private method C::__get()` at the ACCESS and killed the`` |
|        - | 6864 | `					 * script.` |
|        - | 6865 | `					 *` |
|        - | 6866 | `					 * The latch is set by PH7_VmCallMagicMethod at the engine's own` |
|        - | 6867 | `					 * dispatch sites. It is NOT inferred from the instruction: a` |
|        - | 6868 | ``					 * first-class `$o->__get(...)` and `$o->__get('x')` reach the same C`` |
|        - | 6869 | `					 * dispatcher, and php denies both. */` |
|    52520 | 6870 | `				}else` |
|  1970062 | 6871 | `				if( pSelf && !bMemberScreened ){ /* Paranoid edition */` |
|        - | 6872 | `					/* Check if the call is allowed. php binds non-public method` |
|        - | 6873 | `					 * access by the DECLARING class (pVmFunc->pUserData — the class` |
|        - | 6874 | `					 * the callee was compiled in), NOT the instance's class: an` |
|        - | 6875 | `					 * inherited base method calling $this->priv() on a child` |
|        - | 6876 | `					 * instance passes, a child's private SHADOW doesn't hijack the` |
|        - | 6877 | `					 * check for a parent callee, and the denial message names the` |
|        - | 6878 | `					 * declaring class like php. */` |
|  1811313 | 6879 | `					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|        - | 6880 | `					ph7_class *pOwnerClass;` |
|  1811313 | 6881 | `					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|  1811313 | 6882 | `					if( pMeth == 0 && pDeclClass != pSelf ){` |
|      ! 0 | 6883 | `						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      ! 0 | 6884 | `					}` |
|  1811313 | 6885 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        - | 6886 | `						/* ...except that a TRAIT is not a class php still has at run time: it` |
|        - | 6887 | `						 * composed the method INTO the using class, so that class owns the` |
|        - | 6888 | `						 * rule and the name. Deciding against the trait refused a protected` |
|        - | 6889 | `						 * trait method to a SUBCLASS of the composing class (which uses no` |
|        - | 6890 | ``						 * trait of its own) — `class Az { use Tz; } class Bz extends Az {`` |
|        - | 6891 | ``						 * $this->pr(); }` was a fatal php runs. Identity for every non-trait`` |
|        - | 6892 | `						 * method. */` |
|       29 | 6893 | `						pOwnerClass = PH7_VmMethodScopeName(&(*pVm),pSelf,pMeth);` |
|       29 | 6894 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - | 6895 | `							/* php throws a CATCHABLE Error here. The old code merely PRINTED an` |
|        - | 6896 | ``							 * uncaught-exception report and aborted, so `try { $o->priv(); }`` |
|        - | 6897 | ``							 * catch (Error $e)` never caught it and the script died. */`` |
|        - | 6898 | `							char zMsg[256];` |
|        - | 6899 | `							sxi32 rcVis;` |
|        - | 6900 | `							/* php NAMES the calling scope when there is one — "from scope C" —` |
|        - | 6901 | `							 * and says "global scope" only outside every class; the wording is` |
|        - | 6902 | `							 * shared with the first-class-callable screen` |
|        - | 6903 | `							 * (VmMethodVisibilityMsg). */` |
|       19 | 6904 | `							VmMethodVisibilityMsg(&(*pVm),pOwnerClass,` |
|        6 | 6905 | `								pVmFunc->sName.zString,pVmFunc->sName.nByte,` |
|        6 | 6906 | `								pMeth->iProtection,zMsg,sizeof(zMsg));` |
|        - | 6907 | `							/* Consume this call's captured spread runs — this visibility` |
|        - | 6908 | `							 * error exits before the pVmFunc build below. */` |
|       13 | 6909 | `							if( pInstr->iP2 ){` |
|      ! 0 | 6910 | `								VmSpreadConsume(pVm);` |
|      ! 0 | 6911 | `							}` |
|        - | 6912 | `							/* Pop given arguments, and leave the call's NULL result behind. */` |
|       13 | 6913 | `							if( nCallArgs > 0 ){` |
|      ! 0 | 6914 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6915 | `							}` |
|       13 | 6916 | `							PH7_MemObjRelease(pTos);` |
|       13 | 6917 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|       13 | 6918 | `							pTos->nIdx = SXU32_HIGH;` |
|       13 | 6919 | `							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       13 | 6920 | `							if( rcVis == SXERR_ABORT ){ goto Abort; }` |
|       13 | 6921 | `							rc = rcVis;` |
|        - | 6922 | `							/* ENFORCE_RC never consulted pResumeFrame, so an in-place` |
|        - | 6923 | `							 * catch (SXRET_OK + recorded resume) FELL THROUGH and` |
|        - | 6924 | `							 * dispatched the DENIED method anyway. Route like OP_THROW. */` |
|       13 | 6925 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6926 | `						}` |
|        8 | 6927 | `					}` |
|   905612 | 6928 | `				}` |
|  1038164 | 6929 | `			}` |
|  1038164 | 6930 | `		}` |
|        - | 6931 | `		/* pArg is now finalized for every pVmFunc callee (a method call popped its` |
|        - | 6932 | `		 * method-name slot above; functions/closures/generators keep the top base).` |
|        - | 6933 | `		 * Build the PHP 8.1 effective spread-key map here so the generator and the` |
|        - | 6934 | `		 * install path below both see it — and so this call's captured runs are` |
|        - | 6935 | `		 * consumed exactly once, against the correct base. */` |
|  3566332 | 6936 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  2377702 | 6937 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6938 | `		/* Check the PHP call-depth cap (the sole site — BYTECODE.md stage 5).` |
|        - | 6939 | `		 * Default is unbounded (heap-bound recursion, decoupled from the C stack` |
|        - | 6940 | `		 * by the stage-2 trampoline); the C stack is guarded separately by` |
|        - | 6941 | `		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and` |
|        - | 6942 | `		 * then raises a clean non-catchable fatal (was: silently set NULL and` |
|        - | 6943 | `		 * continue) and halts. */` |
|  2377707 | 6944 | `		if( VmRecursionExceeded(pVm) ){` |
|        - | 6945 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - | 6946 | `			 * which walks the whole operand stack — don't release them here. */` |
|        3 | 6947 | `			VmRecursionFatal(&(*pVm));` |
|        3 | 6948 | `			goto Abort;` |
|        - | 6949 | `		}` |
|  2377705 | 6950 | `		if( pVmFunc->pNextName ){` |
|        - | 6951 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|       76 | 6952 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       36 | 6953 | `		}` |
|        - | 6954 | ``		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's`` |
|        - | 6955 | ``		 * `Class::method`). Computed after overload resolution so it reflects the selected method.`` |
|        - | 6956 | ``		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed`` |
|        - | 6957 | `		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method` |
|        - | 6958 | ``		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the`` |
|        - | 6959 | `		 * USING class; we don't carry the using class on the shared struct, so fall back to the` |
|        - | 6960 | `		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for` |
|        - | 6961 | `		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */` |
|  2377705 | 6962 | `		pSelfHint = pSelf;` |
|  2377705 | 6963 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|  2076513 | 6964 | `			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;` |
|  2076513 | 6965 | `			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|  2076061 | 6966 | `				pSelfHint = pDecl;` |
|  1037938 | 6967 | `			}` |
|  1038164 | 6968 | `		}` |
|  2377705 | 6969 | `		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 6970 | `			/* Push the closure's called-class as this frame's LSB class so` |
|        - | 6971 | ``			 * `static::` inside the body resolves like php. An explicit`` |
|        - | 6972 | `			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use` |
|        - | 6973 | `			 * the class captured at the closure's creation site. self::/parent::` |
|        - | 6974 | `			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done` |
|        - | 6975 | ``			 * after pSelfHint so a `self`-typed param is unaffected. */`` |
|    35020 | 6976 | `			if( pClosureScope ){` |
|       70 | 6977 | `				pSelf = pClosureScope;` |
|    34987 | 6978 | `			}else if( pVmFunc->pLsbClass ){` |
|      175 | 6979 | `				pSelf = (ph7_class *)pVmFunc->pLsbClass;` |
|       85 | 6980 | `			}` |
|    17270 | 6981 | `		}` |
|  2377705 | 6982 | `		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){` |
|        - | 6983 | `			/* php 8.4 #[\Deprecated] runtime notice — once per call, before` |
|        - | 6984 | `			 * execution (generators: at the g(...) call site, like php). */` |
|      581 | 6985 | `			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,` |
|      384 | 6986 | `				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);` |
|      192 | 6987 | `		}` |
|        - | 6988 | `		/* php 8.5 #[\NoDiscard]: the CALL SITE decided this answer is thrown away` |
|        - | 6989 | ``		 * (the codegen's bDiscard, php's !RETURN_VALUE_USED), and a `(void)` cast`` |
|        - | 6990 | `		 * in front of the statement is what clears it. Raised before the body, so` |
|        - | 6991 | `		 * a callee that throws has already warned — php's order. */` |
|  2377705 | 6992 | `		if( (pVmFunc->iFlags & VM_FUNC_NODISCARD) && bResultDropped ){` |
|        - | 6993 | `			/* php calls it a "method" whenever the callee has a class SCOPE, and a` |
|        - | 6994 | ``			 * closure declared in a class body has one -- `C::{closure:C::go():3}`. */`` |
|       57 | 6995 | `			ph7_class *pNdClass = 0;` |
|       57 | 6996 | `			if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|       21 | 6997 | `				pNdClass = pSelfHint;` |
|       47 | 6998 | `			}else if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        7 | 6999 | `				pNdClass = (ph7_class *)pVmFunc->pLsbClass;` |
|        3 | 7000 | `			}` |
|       57 | 7001 | `			VmNoDiscardWarn(&(*pVm),pVmFunc,pNdClass);` |
|       28 | 7002 | `		}` |
|        - | 7003 | `		/* D1: resolve deferred plain-var arguments against this callee's declaration` |
|        - | 7004 | `		 * now — BEFORE the native/generator splits and VmEnterFrame, while pVm->pFrame is` |
|        - | 7005 | `		 * still the caller. pVmFunc is final here (post-overload). Covers plain functions,` |
|        - | 7006 | `		 * methods, closures, dynamic-name calls, generators and native methods uniformly;` |
|        - | 7007 | `		 * only the SOURCE of the by-ref positions differs between the last one and the` |
|        - | 7008 | `		 * rest (a signature string vs. compiled formal parameters). */` |
|        - | 7009 | `		{` |
|        - | 7010 | `			sxi32 rcDA;` |
|  2377705 | 7011 | `			if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 7012 | `				/* A native method declares no formal parameters to match against —` |
|        - | 7013 | `				 * its by-ref positions come from the same signature-derived mask a` |
|        - | 7014 | `				 * builtin uses, so resolve them the builtin way. Sharing this one` |
|        - | 7015 | `				 * site (rather than repeating it in the branch below) keeps the` |
|        - | 7016 | `				 * throw routing identical for both kinds of callee. */` |
|  2347232 | 7017 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|  1564878 | 7018 | `					pVmFunc->pNative->nByRefMask,0,0,pEffCallMap);` |
|   782354 | 7019 | `			}else{` |
|  1219102 | 7020 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   812822 | 7021 | `					(ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs),SySetUsed(&pVmFunc->aArgs),` |
|   406275 | 7022 | `					0,0,0,pEffCallMap);` |
|        - | 7023 | `			}` |
|  2377713 | 7024 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 7025 | `		}` |
|  2377651 | 7026 | `		if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 7027 | `			/* C-bodied method (VM_FUNC_NATIVE): hand it to the foreign-function path.` |
|        - | 7028 | `			 *` |
|        - | 7029 | `			 * Everything a METHOD needs has already happened above — the sVmName` |
|        - | 7030 | `			 * lookup, overload selection, $this / self / late-static-binding` |
|        - | 7031 | `			 * resolution, the visibility check, the #[\Deprecated] notice, deferred` |
|        - | 7032 | `			 * argument materialization — and everything BELOW is exactly what a C` |
|        - | 7033 | `			 * body has no use for: a frame, an operand stack, a call record.` |
|        - | 7034 | `			 *` |
|        - | 7035 | `			 * The stack shape already matches a builtin's, because the method branch` |
|        - | 7036 | `			 * popped both the method-name slot and the target slot: [pArg,pTos) are` |
|        - | 7037 | `			 * this call's arguments and pTos is the slot that receives the result.` |
|        - | 7038 | `			 * So the jump lands on shared code, not a copy of it. */` |
|  1564883 | 7039 | `			pFunc = pVmFunc->pNative;` |
|  1564883 | 7040 | `			pNativeOwned = pThis; /* borrowed; the shared tail drops this reference */` |
|        - | 7041 | ``			/* A `C::m()` call reaches the method path with no object in the target`` |
|        - | 7042 | `			 * slot, and the path then adopts the CALLER's $this so an instance-context` |
|        - | 7043 | ``			 * `self::m()` still finds one. A native STATIC method must not see that —`` |
|        - | 7044 | `			 * it would read its argument list one slot off — so the receiver handed to` |
|        - | 7045 | `			 * the body is gated on the declaration, not on what the fallback found. */` |
|  1564883 | 7046 | `			pNativeRecv = (pVmFunc->iFlags & VM_FUNC_NATIVE_STATIC) ? 0 : pThis;` |
|  1564883 | 7047 | `			pNativeClass = pSelf;` |
|  1564883 | 7048 | `			goto NativeCall;` |
|        - | 7049 | `		}` |
|   812773 | 7050 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - | 7051 | `			/* Generator function: return a Generator object instead of executing */` |
|        - | 7052 | `			ph7_exec_ctx *pExecCtx;` |
|        - | 7053 | `			ph7_generator *pGenerator;` |
|        - | 7054 | `			ph7_class_instance *pGenObj;` |
|        - | 7055 | `			ph7_value *pCtxAttr;` |
|        - | 7056 | `			SyString sAttrName;` |
|        - | 7057 | `			ph7_value **apCallArgs;` |
|        - | 7058 | `			int nGenArgs, iArg;` |
|        - | 7059 | `			/* Collect arguments from the operand stack */` |
|      657 | 7060 | `			nGenArgs = (int)(pTos - pArg);` |
|      657 | 7061 | `			apCallArgs = 0;` |
|      657 | 7062 | `			if( nGenArgs > 0 ){` |
|        - | 7063 | `				/* php refuses a non-variable in a by-ref position at the CALL, and for` |
|        - | 7064 | `				 * a generator this IS the call. Routed like the branch's other` |
|        - | 7065 | `				 * pre-frame throws below: no callee frame exists yet, so drop the` |
|        - | 7066 | `				 * arguments plus the function-name slot and land the enclosing try. */` |
|      214 | 7067 | `				rc = VmScreenGenByRefArgs(&(*pVm),pVmFunc,pEffCallMap,pArg,` |
|       70 | 7068 | `					(sxu32)nGenArgs,pSelfHint);` |
|      144 | 7069 | `				if( rc != SXRET_OK ){` |
|        7 | 7070 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 7071 | `						goto Abort;` |
|        - | 7072 | `					}` |
|      330 | 7073 | `					PH7_INLINE_RESUME_BREAK()` |
|        7 | 7074 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 7075 | `					{` |
|        - | 7076 | `						sxi32 iRpB;` |
|        7 | 7077 | `						if( VmRecordedResume(pVm,&iRpB,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 7078 | `							pc = iRpB;` |
|      ! 0 | 7079 | `							break;` |
|        - | 7080 | `						}` |
|        - | 7081 | `					}` |
|        7 | 7082 | `					goto Exception;` |
|        - | 7083 | `				}` |
|       67 | 7084 | `			}` |
|      651 | 7085 | `			if( nGenArgs > 0 ){` |
|      205 | 7086 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       67 | 7087 | `					nGenArgs * sizeof(ph7_value *));` |
|      138 | 7088 | `				if( apCallArgs == 0 ){` |
|        - | 7089 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 | 7090 | `					nGenArgs = 0;` |
|      ! 0 | 7091 | `				}else{` |
|      138 | 7092 | `					VmCallArgMap *pGenMap = pEffCallMap;` |
|      138 | 7093 | `					int didReorder = 0;` |
|      138 | 7094 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - | 7095 | `						/* Named-argument reordering for generator */` |
|       14 | 7096 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|       14 | 7097 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|       14 | 7098 | `						sxu32 nNV = nF;` |
|       14 | 7099 | `						sxi32 iVIdx = -1;` |
|        - | 7100 | `						sxi32 *aGSlot;` |
|        - | 7101 | `						sxu8 *aGUsed;` |
|        - | 7102 | `						sxu32 gi;` |
|       32 | 7103 | `						for( gi = 0; gi < nF; gi++ ){` |
|       20 | 7104 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|       11 | 7105 | `						}` |
|       20 | 7106 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       12 | 7107 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|       14 | 7108 | `						if( aGSlot ){` |
|       14 | 7109 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|       20 | 7110 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        6 | 7111 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|       14 | 7112 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 7113 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 7114 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 7115 | `								goto Abort;` |
|        - | 7116 | `							}` |
|       14 | 7117 | `							if( rc == PH7_EXCEPTION ){` |
|        - | 7118 | `								/* A named-argument error is php's catchable \Error.` |
|        - | 7119 | `								 * No callee frame exists yet on this branch (the` |
|        - | 7120 | `								 * generator body never runs and VmEnterFrame is` |
|        - | 7121 | `								 * further down), so route it like the other` |
|        - | 7122 | `								 * pre-frame OP_CALL throws: drop the args + the` |
|        - | 7123 | `								 * function-name slot and land the enclosing try. */` |
|        3 | 7124 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        3 | 7125 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        3 | 7126 | `								PH7_INLINE_RESUME_BREAK()` |
|        3 | 7127 | `								VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 7128 | `								{` |
|        - | 7129 | `									sxi32 iRpN;` |
|        3 | 7130 | `									if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){` |
|        3 | 7131 | `										pc = iRpN;` |
|        3 | 7132 | `										break;` |
|        - | 7133 | `									}` |
|        - | 7134 | `								}` |
|      ! 0 | 7135 | `								goto Exception;` |
|        - | 7136 | `							}` |
|        - | 7137 | `							{` |
|        - | 7138 | `								/* php's named-hole ArgumentCountError, checked BEFORE` |
|        - | 7139 | `								 * hole compaction: compacting first would report the` |
|        - | 7140 | `								 * positional wording with a fictitious count (g(b:2)` |
|        - | 7141 | ``								 * must be `g(): Argument #1 ($a) not passed`, not`` |
|        - | 7142 | `								 * "1 passed"). Implicit-required watermark, like the` |
|        - | 7143 | `								 * plain-call named path; a hole with NOTHING filled` |
|        - | 7144 | `								 * above it keeps php's count wording — fall through` |
|        - | 7145 | `								 * to VmFiberSetupFrame's check (the compacted count` |
|        - | 7146 | `								 * equals php's num_args there). */` |
|       12 | 7147 | `								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;` |
|       12 | 7148 | `								sxi32 iHole = -1;` |
|       12 | 7149 | `								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);` |
|       28 | 7150 | `								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){` |
|       18 | 7151 | `									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){` |
|       14 | 7152 | `										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);` |
|        6 | 7153 | `									}` |
|       10 | 7154 | `								}` |
|       26 | 7155 | `								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){` |
|        - | 7156 | `									sxu32 gj;` |
|       16 | 7157 | `									int bFound = 0;` |
|       22 | 7158 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       22 | 7159 | `										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }` |
|        5 | 7160 | `									}` |
|       16 | 7161 | `									if( !bFound && gHole + 1 <= nMaxFilledG ){` |
|      ! 0 | 7162 | `										iHole = (sxi32)gHole;` |
|      ! 0 | 7163 | `									}` |
|        9 | 7164 | `								}` |
|       12 | 7165 | `								if( iHole >= 0 ){` |
|      ! 0 | 7166 | `									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,pVmFunc,` |
|      ! 0 | 7167 | `										(sxu32)iHole+1,&aFA[iHole].sName);` |
|      ! 0 | 7168 | `									SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 7169 | `									SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 7170 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 7171 | `										goto Abort;` |
|        - | 7172 | `									}` |
|        - | 7173 | `									/* Route like the VmFiberSetupFrame throw below */` |
|      ! 0 | 7174 | `									PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 7175 | `									VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 7176 | `									{` |
|        - | 7177 | `										sxi32 iRpH;` |
|      ! 0 | 7178 | `										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 7179 | `											pc = iRpH;` |
|      ! 0 | 7180 | `											break;` |
|        - | 7181 | `										}` |
|        - | 7182 | `									}` |
|      ! 0 | 7183 | `									goto Exception;` |
|        - | 7184 | `								}` |
|        - | 7185 | `							}` |
|        - | 7186 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - | 7187 | `							 * append overflow (variadic / positional beyond` |
|        - | 7188 | `							 * formals) so downstream sees every argument. */` |
|        - | 7189 | `							{` |
|       12 | 7190 | `								int nOut = 0;` |
|       28 | 7191 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - | 7192 | `									sxu32 gj;` |
|       24 | 7193 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       24 | 7194 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|       18 | 7195 | `											apCallArgs[nOut++] = &pArg[gj];` |
|       18 | 7196 | `											break;` |
|        - | 7197 | `										}` |
|        5 | 7198 | `									}` |
|       10 | 7199 | `								}` |
|       28 | 7200 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|       18 | 7201 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 | 7202 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 | 7203 | `									}` |
|       10 | 7204 | `								}` |
|       12 | 7205 | `								nGenArgs = nOut;` |
|        - | 7206 | `							}` |
|       12 | 7207 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|       12 | 7208 | `							didReorder = 1;` |
|        5 | 7209 | `						}` |
|        - | 7210 | `						/* If aGSlot allocation failed, fall through to` |
|        - | 7211 | `						 * positional fill below — preserves arg order rather` |
|        - | 7212 | `						 * than passing an uninitialized apCallArgs. */` |
|        5 | 7213 | `					}` |
|      136 | 7214 | `					if( !didReorder ){` |
|      256 | 7215 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|      134 | 7216 | `							apCallArgs[iArg] = &pArg[iArg];` |
|       69 | 7217 | `						}` |
|       61 | 7218 | `					}` |
|        - | 7219 | `				}` |
|       66 | 7220 | `			}` |
|        - | 7221 | `			/* Create execution context and generator wrapper */` |
|      649 | 7222 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|      649 | 7223 | `			if( pExecCtx == 0 ){` |
|      ! 0 | 7224 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 7225 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 7226 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 7227 | `				break;` |
|        - | 7228 | `			}` |
|        - | 7229 | `			/* php's called-scope for the generator BODY: its frame is detached and` |
|        - | 7230 | `			 * created before the receiver is known, so stamp it here, where the call` |
|        - | 7231 | `			 * that made the generator still has it. A trait method's executing scope` |
|        - | 7232 | `			 * is found by walking this class (VmTraitScopeFrom), and without it a` |
|        - | 7233 | `			 * generator declared in a trait could not reach the class's protected` |
|        - | 7234 | `			 * members -- the one activation shape the frame walk cannot recover. */` |
|      649 | 7235 | `			pExecCtx->pFrame->pSelfClass = pSelf ? pSelf : (pThis ? pThis->pClass : 0);` |
|        - | 7236 | `` 			/* And the LATE-STATIC-BINDING class, which is a different question: `self::` `` |
|        - | 7237 | ``			 * is the declaring scope above, `static::` is the class the call was made`` |
|        - | 7238 | `			 * THROUGH. The body resumes long after this call returned, so pVm->aSelf no` |
|        - | 7239 | ``			 * longer holds it and `static::class` / `new static` / `static::m()` inside a`` |
|        - | 7240 | ``			 * generator answered `Class "static" not found`. VmStartCtx republishes this`` |
|        - | 7241 | `			 * for the body's duration. An instance call means the receiver's class; a` |
|        - | 7242 | `			 * static one means the class the call named. */` |
|      649 | 7243 | `			pExecCtx->pLsbClass = pThis ? pThis->pClass : (pSelf ? pSelf : 0);` |
|      649 | 7244 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|      649 | 7245 | `			if( pGenerator == 0 ){` |
|      ! 0 | 7246 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 | 7247 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 7248 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 7249 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 7250 | `				break;` |
|        - | 7251 | `			}` |
|        - | 7252 | `			/* Set up the frame with arguments, closure env, $this */` |
|      649 | 7253 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|      649 | 7254 | `			pVm->pFrame = pExecCtx->pFrame;` |
|     1293 | 7255 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,` |
|      644 | 7256 | `				pEffCallMap ? (pEffCallMap->bStrict ? 1 : 0) : (pVm->bCurStrict ? 1 : 0),` |
|      322 | 7257 | `				pSelfHint,` |
|        - | 7258 | `				TRUE/*generator: the g(...) call site is in the message*/,` |
|        - | 7259 | `				TRUE/*a source-level call binds a by-ref parameter to the caller's slot*/);` |
|      649 | 7260 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|      649 | 7261 | `			pExecCtx->pFrame->pParent = 0;` |
|      649 | 7262 | `			if( apCallArgs ){` |
|      136 | 7263 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       66 | 7264 | `			}` |
|      649 | 7265 | `			if( rc != SXRET_OK ){` |
|       20 | 7266 | `				VmReleaseGenerator(pVm, pGenerator);` |
|       20 | 7267 | `				if( pThis ){` |
|        3 | 7268 | `					PH7_ClassInstanceUnref(pThis);` |
|        1 | 7269 | `				}` |
|       20 | 7270 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 7271 | `					goto Abort;` |
|        - | 7272 | `				}` |
|       20 | 7273 | `				if( rc == PH7_EXCEPTION ){` |
|        - | 7274 | `					/* A declared-type TypeError thrown while binding the` |
|        - | 7275 | `					 * generator's arguments (php binds + type-checks eagerly at` |
|        - | 7276 | `					 * the g(...) call site, before any resume — band A #2). If` |
|        - | 7277 | `					 * an inline try THIS exec owns caught it, land at its` |
|        - | 7278 | `					 * redirect (the drain subsumes the operand pops); else pop` |
|        - | 7279 | `					 * the args + function name and route like the other` |
|        - | 7280 | `					 * OP_CALL throw paths. */` |
|       24 | 7281 | `					PH7_INLINE_RESUME_BREAK()` |
|       18 | 7282 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 7283 | `					{` |
|        - | 7284 | `						sxi32 iRpG;` |
|       18 | 7285 | `						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){` |
|       18 | 7286 | `							pc = iRpG;` |
|       18 | 7287 | `							break;` |
|        - | 7288 | `						}` |
|        - | 7289 | `					}` |
|      ! 0 | 7290 | `					goto Exception;` |
|        - | 7291 | `				}` |
|      ! 0 | 7292 | `				break;` |
|        - | 7293 | `			}` |
|        - | 7294 | `			/* Create Generator class instance */` |
|      631 | 7295 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|      631 | 7296 | `			if( pGenObj == 0 ){` |
|      ! 0 | 7297 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 | 7298 | `				break;` |
|        - | 7299 | `			}` |
|        - | 7300 | `			/* Store generator in __ctx attribute */` |
|      631 | 7301 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      631 | 7302 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|      631 | 7303 | `			if( pCtxAttr ){` |
|      631 | 7304 | `				pCtxAttr->x.pOther = pGenerator;` |
|      631 | 7305 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      313 | 7306 | `			}` |
|        - | 7307 | `			/* Pop args and function name, push Generator object. PH7_NewClassInstance` |
|        - | 7308 | `			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do` |
|        - | 7309 | `			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope` |
|        - | 7310 | ``			 * and its __destruct (which runs pending `finally` blocks and frees the`` |
|        - | 7311 | `			 * exec context) never fires. */` |
|      631 | 7312 | `			PH7_MemObjRelease(pTos);` |
|      631 | 7313 | `			pTos = &pTos[-nCallArgs];` |
|      631 | 7314 | `			pTos->x.pOther = pGenObj;` |
|      631 | 7315 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|      631 | 7316 | `			if( pThis ){` |
|       23 | 7317 | `				PH7_ClassInstanceUnref(pThis);` |
|       10 | 7318 | `			}` |
|      631 | 7319 | `			break;` |
|        - | 7320 | `		}` |
|        - | 7321 | `		/* Extract the formal argument set */` |
|   812121 | 7322 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - | 7323 | `		/* Create a new VM frame  */` |
|   812121 | 7324 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|   812121 | 7325 | `		if( rc == SXRET_OK ){` |
|        - | 7326 | `			/* This activation now needs the function it is about to run. For a` |
|        - | 7327 | `			 * run-time closure that is a hold on its per-instantiation copy, so the` |
|        - | 7328 | `			 * copy cannot be freed under a body that is still executing when its` |
|        - | 7329 | `			 * last Closure object dies. VmLeaveFrame / VmFreeDetachedFrame give it` |
|        - | 7330 | `			 * back -- and a coroutine's DETACHED body frame keeps it for exactly as` |
|        - | 7331 | `			 * long as the frame lives, which is what a suspended generator needs. */` |
|   812121 | 7332 | `			PH7_VmClosureFuncRef(pVmFunc);` |
|   405922 | 7333 | `		}` |
|   812121 | 7334 | `		if( rc == SXRET_OK && pFrame && pSelf ){` |
|        - | 7335 | `			/* php's called-scope: the class this call was made THROUGH. Same as the` |
|        - | 7336 | `			 * receiver's for an instance call, and the named class for a static one --` |
|        - | 7337 | `			 * which is the only way to say which class composed a static TRAIT method. */` |
|   511837 | 7338 | `			pFrame->pSelfClass = pSelf;` |
|   255916 | 7339 | `		}` |
|   812121 | 7340 | `		if( rc != SXRET_OK ){` |
|        - | 7341 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 7342 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 7343 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 7344 | `				&pVmFunc->sName);` |
|        - | 7345 | `			/* The frame that would own (and later release) $this never got created; for a bound` |
|        - | 7346 | `			 * plain closure the consumed transient is the object's ONLY ref, so release it here` |
|        - | 7347 | `			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */` |
|      ! 0 | 7348 | `			if( bClosureThis && pThis ){` |
|      ! 0 | 7349 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 | 7350 | `			}` |
|        - | 7351 | `			/* Pop given arguments */` |
|      ! 0 | 7352 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 7353 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7354 | `			}` |
|        - | 7355 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 | 7356 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 7357 | `			break;` |
|        - | 7358 | `		}` |
|   812121 | 7359 | `		if( pClosureScope ){` |
|        - | 7360 | `			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /` |
|        - | 7361 | `			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected` |
|        - | 7362 | `			 * access inside the body resolves against it. */` |
|       68 | 7363 | `			pFrame->pBoundScope = pClosureScope;` |
|       32 | 7364 | `		}` |
|        - | 7365 | `		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band` |
|        - | 7366 | `		 * A #4): sArg over-counts (defaulted params installed, variadic packed` |
|        - | 7367 | `		 * as one entry) so php's answers can't be derived from it. */` |
|   812121 | 7368 | `		pFrame->nActualArgs = (int)(pTos - pArg);` |
|   812121 | 7369 | `		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) \|\| bClosureThis) ){` |
|        - | 7370 | `			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */` |
|        - | 7371 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|   411083 | 7372 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|   411083 | 7373 | `			if( pObj ){` |
|        - | 7374 | `				/* Reflect the change */` |
|   411083 | 7375 | `				pObj->x.pOther = pThis;` |
|   411083 | 7376 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   205539 | 7377 | `			}` |
|   205539 | 7378 | `		}` |
|   812121 | 7379 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - | 7380 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - | 7381 | `			/* Install static variables */` |
|      114 | 7382 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      240 | 7383 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      130 | 7384 | `				pStatic = &aStatic[n];` |
|      130 | 7385 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - | 7386 | `					/* Initialize the static variables */` |
|       60 | 7387 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|       60 | 7388 | `					if( pObj ){` |
|        - | 7389 | `						/* Assume a NULL initialization value */` |
|       60 | 7390 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|       60 | 7391 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - | 7392 | `							/* Evaluate initialization expression (Any complex expression).` |
|        - | 7393 | `							 * A static's initializer is the one initializer php does NOT` |
|        - | 7394 | `							 * treat as a compile-time constant: it runs inside the call,` |
|        - | 7395 | ``							 * so `static $x = static::class;` is LATE-bound to the runtime`` |
|        - | 7396 | `							 * class. The aSelf push that PH7_VmPeekTopClass answers from` |
|        - | 7397 | `							 * happens further down (this frame is still being built), so` |
|        - | 7398 | `							 * stand it up for the eval and take it straight back --` |
|        - | 7399 | ``							 * without it `static::` found no class at all. */`` |
|       43 | 7400 | `							int bSelfPushed = 0;` |
|       43 | 7401 | `							if( pSelf ){` |
|        9 | 7402 | `								SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|        9 | 7403 | `								bSelfPushed = 1;` |
|        4 | 7404 | `							}` |
|       43 | 7405 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|       43 | 7406 | `							if( bSelfPushed ){` |
|        9 | 7407 | `								(void)SySetPop(&pVm->aSelf);` |
|        4 | 7408 | `							}` |
|       20 | 7409 | `						}` |
|       60 | 7410 | `						pObj->nIdx = pStatic->nIdx;` |
|        - | 7411 | `						/* Permanent pin: the storage outlives every call */` |
|       60 | 7412 | `						VmPinMemObjSlot(&(*pVm),pStatic->nIdx);` |
|       32 | 7413 | `					}else{` |
|      ! 0 | 7414 | `						continue;` |
|        - | 7415 | `					}` |
|       28 | 7416 | `				}` |
|        - | 7417 | `				/* Install in the current frame — a REGISTERED binding, and the slot is` |
|        - | 7418 | `				 * PINNED: the static's storage belongs to the function, not to this` |
|        - | 7419 | `				 * call, so neither the frame teardown nor an unset of the NAME may` |
|        - | 7420 | `				 * recycle it. Poking hVar directly left the binding invisible to the` |
|        - | 7421 | ``				 * reference table, so an array element sharing the static (`[&$s]`)`` |
|        - | 7422 | ``				 * did not count as a reference and `unset($s)` destroyed the storage —`` |
|        - | 7423 | `				 * the next call started over from the initializer. The pin is taken ONCE,` |
|        - | 7424 | `				 * where the slot is created (above). */` |
|      193 | 7425 | `				PH7_VmBindVarSlot(&(*pVm),pFrame,SyStringData(&pStatic->sName),` |
|       63 | 7426 | `					SyStringLength(&pStatic->sName),pStatic->nIdx);` |
|       67 | 7427 | `			}` |
|       55 | 7428 | `		}` |
|        - | 7429 | `		/* Push arguments in the local frame */` |
|        - | 7430 | `		{` |
|   812121 | 7431 | `		VmCallArgMap *pCallMap3 = pEffCallMap;` |
|        - | 7432 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - | 7433 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|        - | 7434 | `		/* A compiled call in a strict file always carries a map with bStrict set` |
|        - | 7435 | `		 * (GenStateAttachStrictFlag); with no map at all the call is either a` |
|        - | 7436 | `		 * compiled WEAK one — where bCurStrict is 0 for the same unit — or an` |
|        - | 7437 | `		 * ENGINE-dispatched one, whose synthetic OP_CALL has no map to carry the` |
|        - | 7438 | `		 * caller's mode. php scopes parameter coercion by the CALLING file either` |
|        - | 7439 | `		 * way, and bCurStrict is that file's mode.` |
|        - | 7440 | `		 *` |
|        - | 7441 | `		 * Unless an INTERNAL function is what reached for this callback (bCallbackWeak):` |
|        - | 7442 | `		 * php has no calling file at that boundary and binds weakly, so` |
|        - | 7443 | ``		 * `array_map('takesInt', ["5"])` from a strict file RUNS there — PHL raised a`` |
|        - | 7444 | `		 * TypeError on valid php, because the ambient bCurStrict was still the strict` |
|        - | 7445 | `		 * caller's. call_user_func / call_user_func_array are php's two forwards and` |
|        - | 7446 | `		 * carry the caller's mode on a map instead. */` |
|  1217935 | 7447 | `		int bCallIsStrict = pCallMap3 ? (pCallMap3->bStrict ? 1 : 0)` |
|   736425 | 7448 | `		                   : (bCallbackWeak ? 0 : (pVm->bCurStrict ? 1 : 0));` |
|   812121 | 7449 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - | 7450 | `			/* ============================================================` |
|        - | 7451 | `			 * Named-argument matching path (PHP 8.0)` |
|        - | 7452 | `			 *` |
|        - | 7453 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - | 7454 | `			 * or position, then install them in the frame.` |
|        - | 7455 | `			 * ============================================================ */` |
|      479 | 7456 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      479 | 7457 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|      479 | 7458 | `			sxi32 iVariadicIdx = -1;` |
|        - | 7459 | `			sxu32 nNonVariadic;` |
|        - | 7460 | `			sxi32 *aSlot;` |
|        - | 7461 | `			sxu8  *aUsed;` |
|        - | 7462 | `			sxu32 i;` |
|        - | 7463 | `			/* Find variadic parameter index */` |
|     1295 | 7464 | `			for( i = 0; i < nFormal; i++ ){` |
|      931 | 7465 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|      114 | 7466 | `					iVariadicIdx = (sxi32)i;` |
|      114 | 7467 | `					break;` |
|        - | 7468 | `				}` |
|      413 | 7469 | `			}` |
|      479 | 7470 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - | 7471 | `			/* Allocate mapping arrays */` |
|      716 | 7472 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      474 | 7473 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|      479 | 7474 | `			if( aSlot == 0 ){` |
|      ! 0 | 7475 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 | 7476 | `				goto Abort;` |
|        - | 7477 | `			}` |
|      479 | 7478 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - | 7479 | `			/* Resolve named arguments to formal parameters */` |
|      716 | 7480 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|      237 | 7481 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|      479 | 7482 | `			if( rc == PH7_ABORT ){` |
|        8 | 7483 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 | 7484 | `				goto Abort;` |
|        - | 7485 | `			}` |
|      473 | 7486 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 7487 | `				/* php's catchable \Error for a bad named argument. The callee` |
|        - | 7488 | `				 * frame is already entered but its body must not run: unwind` |
|        - | 7489 | `				 * exactly like the named-hole ArgumentCountError path below` |
|        - | 7490 | `				 * (release the not-yet-installed actuals — Pass 2's release` |
|        - | 7491 | `				 * loop has not run — pop the callee slot, mark that there is no` |
|        - | 7492 | `				 * callee operand stack, and let VmCallFinish route the throw). */` |
|        - | 7493 | `				sxu32 iRel;` |
|       18 | 7494 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       44 | 7495 | `				for( iRel = 0; iRel < nActual; iRel++ ){` |
|       28 | 7496 | `					PH7_MemObjRelease(&pArg[iRel]);` |
|       15 | 7497 | `				}` |
|       18 | 7498 | `				PH7_MemObjRelease(pTos);` |
|       18 | 7499 | `				pTos = &pTos[-nCallArgs];` |
|       18 | 7500 | `				pFrameStack = 0;` |
|       18 | 7501 | `				goto SkipFuncBody;` |
|        - | 7502 | `			}` |
|        - | 7503 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|        - | 7504 | `			{` |
|        - | 7505 | `			/* php's required watermark for the hole check below, plus the` |
|        - | 7506 | `			 * highest formal slot an actual resolved to: php words a hole` |
|        - | 7507 | ``			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole`` |
|        - | 7508 | `			 * with nothing filled above it gets the positional count message` |
|        - | 7509 | `			 * (zend's RECV arg_num > EX(num_args) distinction). */` |
|        - | 7510 | `			sxu32 nReqNamed;` |
|        - | 7511 | `			sxu32 nNVNamed;` |
|      457 | 7512 | `			sxu32 nMaxFilled = 0;` |
|      457 | 7513 | `			nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);` |
|     1505 | 7514 | `			for( i = 0; i < nActual; i++ ){` |
|     1053 | 7515 | `				if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|      471 | 7516 | `					nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|      233 | 7517 | `				}` |
|      529 | 7518 | `			}` |
|     1203 | 7519 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - | 7520 | `				/* Find the stack arg mapped to formal n */` |
|      765 | 7521 | `				sxi32 iSrc = -1;` |
|     1211 | 7522 | `				for( i = 0; i < nActual; i++ ){` |
|      997 | 7523 | `					if( aSlot[i] == (sxi32)n ){` |
|      551 | 7524 | `						iSrc = (sxi32)i;` |
|      551 | 7525 | `						break;` |
|        - | 7526 | `					}` |
|      228 | 7527 | `				}` |
|      765 | 7528 | `				if( iSrc >= 0 ){` |
|        - | 7529 | `					/* Argument was provided — install with type checking */` |
|      551 | 7530 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - | 7531 | `					/* An explicit null is NOT redirected to the default: PHP applies a` |
|        - | 7532 | `					 * default only for an OMITTED argument. An explicit null falls through` |
|        - | 7533 | `					 * to the type check below (TypeError for a non-nullable typed param,` |
|        - | 7534 | ``					 * kept as null for a typeless one). An implicitly-nullable `Type $x =`` |
|        - | 7535 | ``					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */`` |
|        - | 7536 | `					/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 7537 | `					 * coercion and whole-real materialization in place): the shared` |
|        - | 7538 | `					 * per-argument helper — one implementation for both OP_CALL` |
|        - | 7539 | `					 * paths and the generator/fiber binder (§7.1(f) fold). */` |
|      551 | 7540 | `					iArgPreFlags = pVal->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|      551 | 7541 | `					rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pVal,bCallIsStrict,pSelfHint);` |
|      551 | 7542 | `					if( rc != SXRET_OK ){` |
|        7 | 7543 | `						if( rc == PH7_ABORT ) goto Abort;` |
|        7 | 7544 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 | 7545 | `						PH7_MemObjRelease(pTos);` |
|        7 | 7546 | `						pTos = &pTos[-nCallArgs];` |
|        7 | 7547 | `						pFrameStack = 0;` |
|        7 | 7548 | `						rc = PH7_EXCEPTION;` |
|       11 | 7549 | `						goto SkipFuncBody;` |
|        - | 7550 | `					}` |
|        - | 7551 | `					/* Install: by reference or by value */` |
|      545 | 7552 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|       28 | 7553 | `						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){` |
|        - | 7554 | `							/* php 8.1: $GLOBALS cannot be passed by reference */` |
|        - | 7555 | `							SyBlob sMsg;` |
|      ! 0 | 7556 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 7557 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 7558 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 7559 | `							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|      ! 0 | 7560 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 7561 | `								goto Abort;` |
|        - | 7562 | `							}` |
|      ! 0 | 7563 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 7564 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 7565 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 7566 | `							pFrameStack = 0;` |
|      ! 0 | 7567 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 7568 | `							goto SkipFuncBody;` |
|        - | 7569 | `						}` |
|       28 | 7570 | `						if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)iSrc,pVal) ){` |
|        - | 7571 | `							/* php refuses a by-ref argument whose EXPRESSION is not a variable, at` |
|        - | 7572 | `							 * the call and before the callee runs. Deciding it from the VALUE that` |
|        - | 7573 | `							 * arrived was wrong both ways: an operator result carries its LEFT` |
|        - | 7574 | ``							 * operand's slot, so `f($i + 1)` aliased and overwrote `$i`; and a`` |
|        - | 7575 | `							 * literal and a CALL result look alike there, where php accepts the` |
|        - | 7576 | `							 * call. VmArgRefusedByRef reads the call site's compile-time shape mask` |
|        - | 7577 | `							 * and falls back to the old runtime test only when there is none. */` |
|        - | 7578 | `							sxi32 rcRef;` |
|        3 | 7579 | `							rcRef = VmThrowByRefRefusal(&(*pVm),` |
|        2 | 7580 | `								(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|        2 | 7581 | `								&pVmFunc->sName,pVmFunc,(sxu32)(n+1),&aFormalArg[n].sName);` |
|        3 | 7582 | `							if( rcRef == PH7_ABORT ){` |
|      ! 0 | 7583 | `								goto Abort;` |
|        - | 7584 | `							}` |
|        - | 7585 | `							/* Same teardown as the type-check refusal above: free the slot map,` |
|        - | 7586 | `							 * release the result slot and pop the actuals, then let SkipFuncBody` |
|        - | 7587 | `							 * route the throw. */` |
|        3 | 7588 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 7589 | `							PH7_MemObjRelease(pTos);` |
|        3 | 7590 | `							pTos = &pTos[-nCallArgs];` |
|        3 | 7591 | `							pFrameStack = 0;` |
|        3 | 7592 | `							rc = PH7_EXCEPTION;` |
|        3 | 7593 | `							goto SkipFuncBody;` |
|        - | 7594 | `						}` |
|       25 | 7595 | `						PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)iSrc,pVal);` |
|       25 | 7596 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 | 7597 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 7598 | `						}else{` |
|        - | 7599 | `							SyHashEntry *pRefEntry;` |
|        - | 7600 | `							/* The declared type's conversion is what the reference holds. */` |
|       25 | 7601 | `							PH7_VmByRefArgWriteBack(&(*pVm),pVal,iArgPreFlags);` |
|       37 | 7602 | `							pRefEntry = SyHashGet(&pFrame->hVar,` |
|       24 | 7603 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 | 7604 | `							if( pRefEntry == 0 ){` |
|       37 | 7605 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 | 7606 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|       25 | 7607 | `								sArg.nIdx = pVal->nIdx;` |
|       25 | 7608 | `								sArg.pUserData = 0;` |
|       25 | 7609 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 | 7610 | `							}` |
|       25 | 7611 | `							pObj = 0;` |
|        - | 7612 | `						}` |
|       13 | 7613 | `					}else{` |
|      519 | 7614 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 7615 | `					}` |
|      543 | 7616 | `					if( pObj ){` |
|      519 | 7617 | `						PH7_MemObjStore(pVal,pObj);` |
|      519 | 7618 | `						sArg.nIdx = pObj->nIdx;` |
|      519 | 7619 | `						sArg.pUserData = 0;` |
|      519 | 7620 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      257 | 7621 | `					}` |
|      274 | 7622 | `				}else{` |
|        - | 7623 | `					/* Argument was NOT provided — use default or leave unset */` |
|      218 | 7624 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 7625 | `						/* Should not reach here; variadic handled separately below */` |
|      218 | 7626 | `					}else if( n < nReqNamed ){` |
|        - | 7627 | `						/* php's implicit-required rule applies to named calls` |
|        - | 7628 | `						 * too: a hole below the required watermark throws even` |
|        - | 7629 | `						 * when the formal carries a default — f($a,$b=2,$c)` |
|        - | 7630 | ``						 * called as f(a:1,c:3) is `Argument #2 ($b) not`` |
|        - | 7631 | ``						 * passed` (a hole with NO default is always below the`` |
|        - | 7632 | `						 * watermark, so this subsumes the no-default case).` |
|        - | 7633 | `						 * A hole with nothing filled ABOVE it uses php's` |
|        - | 7634 | `						 * positional count wording instead. The passed stack` |
|        - | 7635 | `						 * args were not released yet on this path (that loop` |
|        - | 7636 | `						 * runs after Pass 2) — release them before the exit. */` |
|        7 | 7637 | `						if( n + 1 > nMaxFilled ){` |
|        3 | 7638 | `							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)` |
|      ! 0 | 7639 | `								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 7640 | `									nMaxFilled,nReqNamed,SySetUsed(&pVmFunc->aArgs))` |
|        3 | 7641 | `								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,pVmFunc,` |
|        1 | 7642 | `									nMaxFilled,nReqNamed,nNVNamed,!bCallbackWeak);` |
|        2 | 7643 | `						}else{` |
|        5 | 7644 | `							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,pVmFunc,n+1,&aFormalArg[n].sName);` |
|        - | 7645 | `						}` |
|        7 | 7646 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       15 | 7647 | `						for( i = 0; i < nActual; i++ ){` |
|        9 | 7648 | `							PH7_MemObjRelease(&pArg[i]);` |
|        5 | 7649 | `						}` |
|        7 | 7650 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 7651 | `							goto Abort;` |
|        - | 7652 | `						}` |
|        7 | 7653 | `						PH7_MemObjRelease(pTos);` |
|        7 | 7654 | `						pTos = &pTos[-nCallArgs];` |
|        7 | 7655 | `						pFrameStack = 0;` |
|        7 | 7656 | `						rc = PH7_EXCEPTION;` |
|        7 | 7657 | `						goto SkipFuncBody;` |
|      212 | 7658 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      212 | 7659 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      212 | 7660 | `						if( pObj ){` |
|        - | 7661 | `							VmDefaultScope sDefScope;` |
|      212 | 7662 | `							VmDefaultScopeEnter(&(*pVm),pFrame,pVmFunc,pSelf,&sDefScope);` |
|      212 | 7663 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|      212 | 7664 | `							VmDefaultScopeLeave(&(*pVm),&sDefScope);` |
|      212 | 7665 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      212 | 7666 | `							sArg.nIdx = pObj->nIdx;` |
|      212 | 7667 | `							sArg.pUserData = 0;` |
|      212 | 7668 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 7669 | `							/* A null default on an implicitly-nullable param must stay null` |
|        - | 7670 | `							 * (see the positional-path note above). */` |
|      208 | 7671 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|       42 | 7672 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0` |
|       25 | 7673 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 7674 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 7675 | `								if( xCast ) xCast(pObj);` |
|      ! 0 | 7676 | `							}else{` |
|        - | 7677 | `								/* Mask matched — a const-indirected whole-real default` |
|        - | 7678 | ``								 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|      212 | 7679 | `								VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 7680 | `							}` |
|      104 | 7681 | `						}` |
|      104 | 7682 | `					}` |
|        - | 7683 | `				}` |
|      378 | 7684 | `			}` |
|        - | 7685 | `			} /* end nReqNamed scope */` |
|        - | 7686 | `			/* Handle variadic parameter */` |
|      443 | 7687 | `			if( iVariadicIdx >= 0 ){` |
|      114 | 7688 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|      114 | 7689 | `				if( pObj ){` |
|        - | 7690 | `					/* Capture the slot index now: PH7_HashmapInsert below can` |
|        - | 7691 | `					 * PH7_ReserveMemObj, which used to reallocate pVm->aMemObj and` |
|        - | 7692 | `					 * dangle pObj. Redundant since P1 -- the pool's segments are` |
|        - | 7693 | `					 * fixed, so a slot's address never moves. Left for the harvest` |
|        - | 7694 | `					 * sweep (PERF.md P1). */` |
|        - | 7695 | `					sxu32 nVariadicSlot;` |
|      114 | 7696 | `					PH7_MemObjToHashmap(pObj);` |
|      114 | 7697 | `					nVariadicSlot = pObj->nIdx;` |
|        - | 7698 | `					{` |
|      114 | 7699 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - | 7700 | `						/* php numbers a failing NAMED variadic element as` |
|        - | 7701 | `						 * max(total positional args, declared non-variadic` |
|        - | 7702 | `						 * formals) + 1 — zend's RECV slots always count —` |
|        - | 7703 | `						 * whichever named element fails; a POSITIONAL element` |
|        - | 7704 | `						 * uses its own 1-based call position. */` |
|      114 | 7705 | `						sxu32 nPositional = 0;` |
|      660 | 7706 | `						for( i = 0; i < nActual; i++ ){` |
|      550 | 7707 | `							if( !(i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0) ){` |
|      333 | 7708 | `								nPositional++;` |
|      165 | 7709 | `							}` |
|      277 | 7710 | `						}` |
|      616 | 7711 | `						for( i = 0; i < nActual; i++ ){` |
|      544 | 7712 | `							if( aSlot[i] == -1 ){` |
|      498 | 7713 | `								int bNamed = (i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0);` |
|      498 | 7714 | `								int bRefElem = 0; /* alias this entry to the caller's slot? */` |
|        - | 7715 | `								/* Same per-element type check + weak coercion as the` |
|        - | 7716 | ``								 * positional-only path (shared helper; no `($name)`). */`` |
|      745 | 7717 | `								rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|      494 | 7718 | `									&aFormalArg[iVariadicIdx],&pArg[i],` |
|      341 | 7719 | `									bNamed ? SXMAX(nPositional,nNonVariadic) + 1 : i + 1,bCallIsStrict);` |
|      498 | 7720 | `								if( rc != SXRET_OK ){` |
|       39 | 7721 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 7722 | `										goto Abort;` |
|        - | 7723 | `									}` |
|       39 | 7724 | `									SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       39 | 7725 | `									PH7_MemObjRelease(pTos);` |
|       39 | 7726 | `									pTos = &pTos[-nCallArgs];` |
|       39 | 7727 | `									pFrameStack = 0;` |
|       39 | 7728 | `									rc = PH7_EXCEPTION;` |
|       39 | 7729 | `									goto SkipFuncBody;` |
|        - | 7730 | `								}` |
|      462 | 7731 | `								if( aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7732 | `									/* php screens a by-ref VARIADIC tail per collected element, in its` |
|        - | 7733 | `									 * no-name wording: a variadic has no per-element parameter name, so` |
|        - | 7734 | ``									 * php says `Argument #N could not be passed by reference` and stops`` |
|        - | 7735 | `									 * there. Nothing screened this arm at all — the branch that collects` |
|        - | 7736 | `									 * a variadic runs before the by-ref binder ever sees a formal. */` |
|        8 | 7737 | `									if( PH7_VmArgRefusedByRef(pCallMap3,i,&pArg[i]) ){` |
|        - | 7738 | `										SyBlob sMsgV;` |
|        - | 7739 | `										sxi32 rcV;` |
|        3 | 7740 | `										SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        3 | 7741 | `										SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        2 | 7742 | `											&pVmFunc->sName,(unsigned)(i + 1));` |
|        3 | 7743 | `										rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        3 | 7744 | `										if( rcV == PH7_ABORT ){` |
|      ! 0 | 7745 | `											goto Abort;` |
|        - | 7746 | `										}` |
|        3 | 7747 | `										SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 7748 | `										PH7_MemObjRelease(pTos);` |
|        3 | 7749 | `										pTos = &pTos[-nCallArgs];` |
|        3 | 7750 | `										pFrameStack = 0;` |
|        3 | 7751 | `										rc = PH7_EXCEPTION;` |
|        3 | 7752 | `										goto SkipFuncBody;` |
|        - | 7753 | `									}` |
|        5 | 7754 | `									PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,i,&pArg[i]);` |
|        2 | 7755 | `								}` |
|        - | 7756 | `								/* A by-ref variadic tail ALIASES its actuals, named entries` |
|        - | 7757 | `								 * included (the positional twin below this branch says why). */` |
|      690 | 7758 | `								bRefElem = (aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF)` |
|      456 | 7759 | `									&& pArg[i].nIdx != SXU32_HIGH;` |
|      460 | 7760 | `								if( bNamed ){` |
|        - | 7761 | `									/* Named variadic entry: insert with string key */` |
|        - | 7762 | `									ph7_value sKey;` |
|      158 | 7763 | `									PH7_MemObjInit(pVm, &sKey);` |
|      158 | 7764 | `									PH7_MemObjStringAppend(&sKey,` |
|      154 | 7765 | `										pCallMap3->aNames[i].zString,` |
|      154 | 7766 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|      158 | 7767 | `									if( bRefElem ){` |
|        5 | 7768 | `										PH7_HashmapInsertByRef(pVarMap, &sKey, pArg[i].nIdx);` |
|        3 | 7769 | `									}else{` |
|      154 | 7770 | `										PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|        - | 7771 | `									}` |
|      158 | 7772 | `									PH7_MemObjRelease(&sKey);` |
|      382 | 7773 | `								}else if( bRefElem ){` |
|        - | 7774 | `									/* Positional variadic entry, aliased */` |
|      ! 0 | 7775 | `									PH7_HashmapInsertByRef(pVarMap, 0, pArg[i].nIdx);` |
|      ! 0 | 7776 | `								}else{` |
|        - | 7777 | `									/* Positional variadic entry */` |
|      305 | 7778 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - | 7779 | `								}` |
|      228 | 7780 | `							}` |
|      255 | 7781 | `						}` |
|        - | 7782 | `					}` |
|       76 | 7783 | `					sArg.nIdx = nVariadicSlot; /* the saved index (see above; redundant since P1) */` |
|       76 | 7784 | `					sArg.pUserData = 0;` |
|       76 | 7785 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       36 | 7786 | `				}` |
|       40 | 7787 | `			}else{` |
|        - | 7788 | `				/* No variadic — preserve unresolved positional overflow` |
|        - | 7789 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - | 7790 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - | 7791 | `				 * the positional-only path's behavior. */` |
|      331 | 7792 | `				sxu32 nAnon = nNonVariadic;` |
|      815 | 7793 | `				for( i = 0; i < nActual; i++ ){` |
|      487 | 7794 | `					if( aSlot[i] == -2 ){` |
|        - | 7795 | `						char zAnonBuf[32];` |
|        - | 7796 | `						SyString sAnonName;` |
|      ! 0 | 7797 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 | 7798 | `							"[%u]apArg",nAnon);` |
|      ! 0 | 7799 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 | 7800 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 | 7801 | `						if( pObj ){` |
|      ! 0 | 7802 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 | 7803 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 | 7804 | `							sArg.pUserData = 0;` |
|      ! 0 | 7805 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 | 7806 | `						}` |
|      ! 0 | 7807 | `						nAnon++;` |
|      ! 0 | 7808 | `					}` |
|      245 | 7809 | `				}` |
|        - | 7810 | `			}` |
|        - | 7811 | `			/* Release all stack arguments */` |
|     1361 | 7812 | `			for( i = 0; i < nActual; i++ ){` |
|      961 | 7813 | `				PH7_MemObjRelease(&pArg[i]);` |
|      483 | 7814 | `			}` |
|      405 | 7815 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - | 7816 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|      405 | 7817 | `			n = nFormal;` |
|      205 | 7818 | `		}else{` |
|        - | 7819 | `		/* ============================================================` |
|        - | 7820 | `		 * Positional-only matching path (original)` |
|        - | 7821 | `		 * ============================================================ */` |
|        - | 7822 | `		/* Base of the actual argument stack, for php's per-ELEMENT argument` |
|        - | 7823 | `		 * number in a variadic type-error (php numbers a variadic-collected` |
|        - | 7824 | `		 * element by its overall 1-based call position, not the formal index). */` |
|   811647 | 7825 | `		ph7_value *pArgBase = pArg;` |
|   811647 | 7826 | `		n = 0;` |
|  1135046 | 7827 | `		while( pArg < pTos ){` |
|   328511 | 7828 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - | 7829 | `				/* Variadic parameter: collect all remaining args into an array */` |
|      746 | 7830 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      746 | 7831 | `				if( pObj ){` |
|        - | 7832 | `					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,` |
|        - | 7833 | `					 * which used to reallocate pVm->aMemObj and dangle pObj (a real UAF, masked by` |
|        - | 7834 | `					 * the pool allocator). Redundant since P1 -- the pool's segments are fixed, so` |
|        - | 7835 | `					 * a slot's address never moves. Left for the harvest sweep (PERF.md P1). */` |
|        - | 7836 | `					sxu32 nVariadicIdx;` |
|        - | 7837 | `					/* Initialize as empty array */` |
|      746 | 7838 | `					PH7_MemObjToHashmap(pObj);` |
|      746 | 7839 | `					nVariadicIdx = pObj->nIdx;` |
|        - | 7840 | `					{` |
|      746 | 7841 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     3307 | 7842 | `						while( pArg < pTos ){` |
|        - | 7843 | `							/* Per-element type check + weak coercion (shared helper,` |
|        - | 7844 | `							 * also used by the named-argument path). The argument` |
|        - | 7845 | `							 * number is the element's overall 1-based call position` |
|        - | 7846 | `` 							 * ((pArg - pArgBase) + 1) and, like php, the `($name)` `` |
|        - | 7847 | `							 * clause is omitted. */` |
|     3806 | 7848 | `							rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|     2609 | 7849 | `								&aFormalArg[n],pArg,(sxu32)(pArg - pArgBase) + 1,bCallIsStrict);` |
|     2614 | 7850 | `							if( rc != SXRET_OK ){` |
|       47 | 7851 | `								if( rc == PH7_ABORT ){` |
|      ! 0 | 7852 | `									goto Abort;` |
|        - | 7853 | `								}` |
|        - | 7854 | `								/* Skip function body, route through normal cleanup */` |
|       47 | 7855 | `								PH7_MemObjRelease(pTos);` |
|       47 | 7856 | `								pTos = &pTos[-nCallArgs];` |
|       47 | 7857 | `								pFrameStack = 0;` |
|       47 | 7858 | `								rc = PH7_EXCEPTION;` |
|       47 | 7859 | `								goto SkipFuncBody;` |
|        - | 7860 | `							}` |
|     2572 | 7861 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7862 | `								/* The positional twin of the named path's variadic screen above:` |
|        - | 7863 | `								 * php refuses a non-variable collected into a by-ref variadic tail,` |
|        - | 7864 | `								 * in its no-name wording. */` |
|       52 | 7865 | `								sxu32 nPosV = (sxu32)(pArg - pArgBase);` |
|       52 | 7866 | `								if( PH7_VmArgRefusedByRef(pCallMap3,nPosV,pArg) ){` |
|        - | 7867 | `									SyBlob sMsgV;` |
|        - | 7868 | `									sxi32 rcV;` |
|        7 | 7869 | `									SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        7 | 7870 | `									SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        6 | 7871 | `										&pVmFunc->sName,(unsigned)(nPosV + 1));` |
|        7 | 7872 | `									rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        7 | 7873 | `									if( rcV == PH7_ABORT ){` |
|      ! 0 | 7874 | `										goto Abort;` |
|        - | 7875 | `									}` |
|        7 | 7876 | `									PH7_MemObjRelease(pTos);` |
|        7 | 7877 | `									pTos = &pTos[-nCallArgs];` |
|        7 | 7878 | `									pFrameStack = 0;` |
|        7 | 7879 | `									rc = PH7_EXCEPTION;` |
|        7 | 7880 | `									goto SkipFuncBody;` |
|        - | 7881 | `								}` |
|       46 | 7882 | `								PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,nPosV,pArg);` |
|       46 | 7883 | `								if( pArg->nIdx != SXU32_HIGH ){` |
|        - | 7884 | `									/* php ALIASES each collected element to the caller's slot:` |
|        - | 7885 | ``									 * `function f(&...$xs){ $xs[0] = 'A'; }` writes back, and`` |
|        - | 7886 | ``									 * var_dump($xs) inside the callee shows `&int(1)`. Copying`` |
|        - | 7887 | `									 * them left every actual untouched. The node counts as a` |
|        - | 7888 | `									 * holder of the caller's slot, so the frame teardown that` |
|        - | 7889 | `									 * destroys the variadic array gives the hold back. */` |
|       42 | 7890 | `									PH7_HashmapInsertByRef(pMap, 0, pArg->nIdx);` |
|       42 | 7891 | `									pArg++;` |
|       42 | 7892 | `									continue;` |
|        - | 7893 | `								}` |
|        2 | 7894 | `							}` |
|     2526 | 7895 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|     2526 | 7896 | `							pArg++;` |
|        5 | 7897 | `						}` |
|        - | 7898 | `					}` |
|      698 | 7899 | `					sArg.nIdx = nVariadicIdx; /* the saved index (see above; redundant since P1) */` |
|      698 | 7900 | `					sArg.pUserData = 0;` |
|      698 | 7901 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      299 | 7902 | `				}` |
|      698 | 7903 | `				break; /* All remaining args consumed */` |
|        - | 7904 | `			}` |
|   327770 | 7905 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 7906 | `				/* An explicit null is NOT redirected to the default (PHP applies a` |
|        - | 7907 | `				 * default only for an omitted arg); it falls through to the type check` |
|        - | 7908 | `				 * below — TypeError for a non-nullable typed param, kept as null for a` |
|        - | 7909 | ``				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE`` |
|        - | 7910 | `				 * at compile time so its check accepts null. */` |
|        - | 7911 | `				/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 7912 | `				 * coercion and whole-real materialization in place — nullable` |
|        - | 7913 | `				 * types (?type) let null through): the shared per-argument` |
|        - | 7914 | `				 * helper, one implementation for both OP_CALL paths and the` |
|        - | 7915 | `				 * generator/fiber binder (§7.1(f) fold). */` |
|   315202 | 7916 | `				iArgPreFlags = pArg->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|   315202 | 7917 | `				rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pArg,bCallIsStrict,pSelfHint);` |
|   315202 | 7918 | `				if( rc != SXRET_OK ){` |
|      349 | 7919 | `					if( rc == PH7_ABORT ){` |
|        6 | 7920 | `						goto Abort;` |
|        - | 7921 | `					}` |
|        - | 7922 | `					/* Skip function body, route through normal cleanup */` |
|      345 | 7923 | `					PH7_MemObjRelease(pTos);` |
|      345 | 7924 | `					pTos = &pTos[-nCallArgs];` |
|      345 | 7925 | `					pFrameStack = 0;` |
|      345 | 7926 | `					rc = PH7_EXCEPTION;` |
|      345 | 7927 | `					goto SkipFuncBody;` |
|        - | 7928 | `				}` |
|   314858 | 7929 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7930 | `					/* Pass by reference */` |
|     7668 | 7931 | `					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){` |
|        - | 7932 | `						/* php 8.1: $GLOBALS cannot be passed by reference —` |
|        - | 7933 | `						 * a catchable Error with php's exact wording. */` |
|        - | 7934 | `						SyBlob sMsg;` |
|        3 | 7935 | `						SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 7936 | `						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|        2 | 7937 | `							&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        3 | 7938 | `						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 7939 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 7940 | `							goto Abort;` |
|        - | 7941 | `						}` |
|        3 | 7942 | `						PH7_MemObjRelease(pTos);` |
|        3 | 7943 | `						pTos = &pTos[-nCallArgs];` |
|        3 | 7944 | `						pFrameStack = 0;` |
|        3 | 7945 | `						rc = PH7_EXCEPTION;` |
|        3 | 7946 | `						goto SkipFuncBody;` |
|        - | 7947 | `					}` |
|     7666 | 7948 | `					if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)n,pArg) ){` |
|        - | 7949 | `						/* php's refusal, decided from the argument's compile-time SHAPE (the` |
|        - | 7950 | `						 * companion of the named-argument binder above; see VmArgRefusedByRef). */` |
|        - | 7951 | `						sxi32 rcRef;` |
|     6032 | 7952 | `						rcRef = VmThrowByRefRefusal(&(*pVm),` |
|     4020 | 7953 | `							(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|     4020 | 7954 | `							&pVmFunc->sName,pVmFunc,(sxu32)(n+1),&aFormalArg[n].sName);` |
|     4022 | 7955 | `						if( rcRef == PH7_ABORT ){` |
|      ! 0 | 7956 | `							goto Abort;` |
|        - | 7957 | `						}` |
|        - | 7958 | `						/* Route the throw like every other binder refusal: release the result` |
|        - | 7959 | `						 * slot, pop the actuals and let SkipFuncBody finish the call. Returning` |
|        - | 7960 | `						 * from here walked out of the dispatch loop with the callee's frame and` |
|        - | 7961 | `						 * stack still live, so a CAUGHT refusal silently abandoned every` |
|        - | 7962 | `						 * statement after the catch. */` |
|     4022 | 7963 | `						PH7_MemObjRelease(pTos);` |
|     4022 | 7964 | `						pTos = &pTos[-nCallArgs];` |
|     4022 | 7965 | `						pFrameStack = 0;` |
|     4022 | 7966 | `						rc = PH7_EXCEPTION;` |
|     4022 | 7967 | `						goto SkipFuncBody;` |
|        - | 7968 | `					}` |
|     3646 | 7969 | `					PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)n,pArg);` |
|     3646 | 7970 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - | 7971 | `						/* Nothing to alias: pass by value. */` |
|       99 | 7972 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       51 | 7973 | `					}else{` |
|        - | 7974 | `						SyHashEntry *pRefEntry;` |
|        - | 7975 | `						/* The declared type's conversion is what the reference holds. */` |
|     3550 | 7976 | `						PH7_VmByRefArgWriteBack(&(*pVm),pArg,iArgPreFlags);` |
|        - | 7977 | `						/* Install the referenced variable in the private function frame */` |
|     3550 | 7978 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|     3550 | 7979 | `						if( pRefEntry == 0 ){` |
|     5319 | 7980 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|     3545 | 7981 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|     3550 | 7982 | `							sArg.nIdx = pArg->nIdx;` |
|     3550 | 7983 | `							sArg.pUserData = 0;` |
|     3550 | 7984 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     1769 | 7985 | `						}` |
|     3550 | 7986 | `						pObj = 0;` |
|        - | 7987 | `					}` |
|     1822 | 7988 | `				}else{` |
|        - | 7989 | `					/* Pass by value,make a copy of the given argument */` |
|   307195 | 7990 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 7991 | `				}` |
|   155544 | 7992 | `			}else{` |
|        - | 7993 | `				char zName[32];` |
|        - | 7994 | `				SyString sArgName;` |
|        - | 7995 | `				/* Set a dummy name */` |
|    12573 | 7996 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|    12573 | 7997 | `				sArgName.zString = zName;` |
|        - | 7998 | `				/* Annonymous argument */` |
|    12573 | 7999 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 8000 | `			}` |
|   323404 | 8001 | `			if( pObj ){` |
|   319859 | 8002 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 8003 | `				/* Insert argument index  */` |
|   319859 | 8004 | `				sArg.nIdx = pObj->nIdx;` |
|   319859 | 8005 | `				sArg.pUserData = 0;` |
|   319859 | 8006 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|   160014 | 8007 | `			}` |
|   323404 | 8008 | `			PH7_MemObjRelease(pArg);` |
|   323404 | 8009 | `			pArg++;` |
|   323404 | 8010 | `			++n;` |
|        5 | 8011 | `		}` |
|        - | 8012 | `		} /* end named vs positional branch */` |
|        - | 8013 | `		/* Set up closure environment */` |
|   807633 | 8014 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 8015 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 8016 | `			ph7_value *pValue;` |
|        - | 8017 | `			sxu32 iEnv;` |
|    34784 | 8018 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|    81378 | 8019 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|    46599 | 8020 | `				pEnv = &aEnv[iEnv];` |
|    46599 | 8021 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 8022 | `					/* Do not install null value */` |
|    33848 | 8023 | `					continue;` |
|        - | 8024 | `				}` |
|    12751 | 8025 | `				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       13 | 8026 | `				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|        - | 8027 | `					/* The Closure instance carries an explicit bound $this` |
|        - | 8028 | `					 * (bindTo/bind/call): it wins over the creation-time` |
|        - | 8029 | `					 * captured $this, php-exact. */` |
|        7 | 8030 | `					continue;` |
|        - | 8031 | `				}` |
|    12750 | 8032 | `				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|        - | 8033 | `					/* Captured by reference: link the name to the shared slot` |
|        - | 8034 | `					 * (no copy), mirroring the by-ref argument install above. */` |
|     2730 | 8035 | `					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|     4078 | 8036 | `						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),` |
|     2725 | 8037 | `							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|     1348 | 8038 | `					}` |
|     2730 | 8039 | `					continue;` |
|        - | 8040 | `				}` |
|    10025 | 8041 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|    10025 | 8042 | `				if( pValue == 0 ){` |
|      ! 0 | 8043 | `					continue;` |
|        - | 8044 | `				}` |
|        - | 8045 | `				/* Invalidate any prior representation */` |
|    10025 | 8046 | `				PH7_MemObjRelease(pValue);` |
|        - | 8047 | `				/* Duplicate bound variable value */` |
|    10025 | 8048 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|     4952 | 8049 | `			}` |
|    17152 | 8050 | `		}` |
|        - | 8051 | `		/* Too-few-arguments check, placed AFTER the passed arguments were` |
|        - | 8052 | `		 * installed and type-checked: php's RECV order means a type error on` |
|        - | 8053 | ``		 * a PASSED argument beats the count error (`f(int $x,$y)` called`` |
|        - | 8054 | `		 * f("str") is a TypeError, not ArgumentCountError). The passed args` |
|        - | 8055 | `		 * were already released by the install loop, so the standard throw` |
|        - | 8056 | `		 * exit leaks nothing. The named path never fires this (its per-hole` |
|        - | 8057 | `		 * check ran in-loop; n == nNonVariadic >= nRequired here).` |
|        - | 8058 | `		 * Builtins written as embedded PHP in the prelude (VM_FUNC_INTERNAL,` |
|        - | 8059 | `		 * with or without VM_FUNC_CLASS_METHOD) are NOT exempt: their declared` |
|        - | 8060 | `		 * signatures were swept against the php 8.5 oracle, so the derived` |
|        - | 8061 | `		 * required count and wording are php's, and VmThrowBuiltinTooFewArgs` |
|        - | 8062 | `		 * words them as php words an internal callable. */` |
|   807633 | 8063 | `		if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 8064 | `			sxu32 nNonVar,nReq;` |
|     7308 | 8065 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     7308 | 8066 | `			if( n < nReq ){` |
|       67 | 8067 | `				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;` |
|       67 | 8068 | `				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|       37 | 8069 | `					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       12 | 8070 | `						nPassed,nReq,SySetUsed(&pVmFunc->aArgs));` |
|       13 | 8071 | `				}else{` |
|        - | 8072 | `					/* php names the call SITE only when the caller is user code: an` |
|        - | 8073 | `					 * INTERNAL function reaching for a callback (array_map, usort,` |
|        - | 8074 | `					 * an autoloader) has no calling line to name, which is exactly` |
|        - | 8075 | `					 * what bCallbackWeak already marks. */` |
|       63 | 8076 | `					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,pVmFunc,` |
|       20 | 8077 | `						nPassed,nReq,nNonVar,!bCallbackWeak);` |
|        - | 8078 | `				}` |
|       67 | 8079 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 8080 | `					goto Abort;` |
|        - | 8081 | `				}` |
|       67 | 8082 | `				PH7_MemObjRelease(pTos);` |
|       67 | 8083 | `				pTos = &pTos[-nCallArgs];` |
|       67 | 8084 | `				pFrameStack = 0;` |
|       67 | 8085 | `				rc = PH7_EXCEPTION;` |
|       67 | 8086 | `				goto SkipFuncBody;` |
|        5 | 8087 | `			}` |
|   803895 | 8088 | `		}else if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) == VM_FUNC_INTERNAL ){` |
|        - | 8089 | `			/* Too-MANY arguments, the mirror php enforces for an internal` |
|        - | 8090 | `			 * callable ("count_chars() expects at most 2 arguments, 3 given").` |
|        - | 8091 | `			 * PHL accepted the extras silently — the surplus only ever reached` |
|        - | 8092 | `			 * func_get_args(). Every formal is filled on this branch, so the call` |
|        - | 8093 | `			 * is over the maximum exactly when more actuals arrived than there are` |
|        - | 8094 | `			 * non-variadic formals; a VARIADIC tail means php declares no maximum` |
|        - | 8095 | `			 * at all, so nNonVar below the formal count is exempt. Prelude` |
|        - | 8096 | `			 * FUNCTIONS only: the chunk-backed class METHODS were not swept against` |
|        - | 8097 | `			 * php's signatures (Fiber::start() is declared argless and reads` |
|        - | 8098 | `			 * func_get_args()). */` |
|        - | 8099 | `			sxu32 nNonVar,nReq;` |
|     2287 | 8100 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     2282 | 8101 | `			if( nNonVar == SySetUsed(&pVmFunc->aArgs)` |
|     2281 | 8102 | `			 && pFrame->nActualArgs >= 0` |
|     2285 | 8103 | `			 && (sxu32)pFrame->nActualArgs > nNonVar ){` |
|       34 | 8104 | `				rc = VmThrowBuiltinTooManyArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       22 | 8105 | `					(sxu32)pFrame->nActualArgs,nNonVar,nReq);` |
|       23 | 8106 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 8107 | `					goto Abort;` |
|        - | 8108 | `				}` |
|       23 | 8109 | `				PH7_MemObjRelease(pTos);` |
|       23 | 8110 | `				pTos = &pTos[-nCallArgs];` |
|       23 | 8111 | `				pFrameStack = 0;` |
|       23 | 8112 | `				rc = PH7_EXCEPTION;` |
|       23 | 8113 | `				goto SkipFuncBody;` |
|        - | 8114 | `			}` |
|     1130 | 8115 | `		}` |
|        - | 8116 | `		/* Process default values for remaining formal parameters */` |
|   824628 | 8117 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    17893 | 8118 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 8119 | `				/* Variadic parameter with no extra args — create empty array */` |
|      812 | 8120 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      812 | 8121 | `				if( pObj ){` |
|      812 | 8122 | `					PH7_MemObjToHashmap(pObj);` |
|      812 | 8123 | `					sArg.nIdx = pObj->nIdx;` |
|      812 | 8124 | `					sArg.pUserData = 0;` |
|      812 | 8125 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      356 | 8126 | `				}` |
|      812 | 8127 | `				n++;` |
|      812 | 8128 | `				break; /* Variadic is always last */` |
|        - | 8129 | `			}` |
|    17086 | 8130 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|    17086 | 8131 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|    17086 | 8132 | `				if( pObj ){` |
|        - | 8133 | `					/* Evaluate the default value and extract it's result */` |
|        - | 8134 | `					VmDefaultScope sDefScope;` |
|    17086 | 8135 | `					VmDefaultScopeEnter(&(*pVm),pFrame,pVmFunc,pSelf,&sDefScope);` |
|    17086 | 8136 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|    17086 | 8137 | `					VmDefaultScopeLeave(&(*pVm),&sDefScope);` |
|    17086 | 8138 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 8139 | `						goto Abort;` |
|        - | 8140 | `					}` |
|        - | 8141 | `					/* Insert argument index */` |
|    17086 | 8142 | `					sArg.nIdx = pObj->nIdx;` |
|    17086 | 8143 | `					sArg.pUserData = 0;` |
|    17086 | 8144 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 8145 | `					/* Make sure the default argument is of the correct type.` |
|        - | 8146 | ``					 * A null default on an implicitly-nullable param (`int $x = null`)`` |
|        - | 8147 | `					 * must stay null — casting it to 0/""/false would diverge from PHP` |
|        - | 8148 | `					 * and contradict the explicit-null path, which now keeps it null. */` |
|    17081 | 8149 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     2289 | 8150 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)` |
|     1146 | 8151 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 8152 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 8153 | `						/* Cast to the desired type */` |
|      ! 0 | 8154 | `						xCast(pObj);` |
|      ! 0 | 8155 | `					}else{` |
|        - | 8156 | `						/* Mask matched — a const-indirected whole-real default` |
|        - | 8157 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|    17086 | 8158 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 8159 | `					}` |
|     8532 | 8160 | `				}` |
|     8532 | 8161 | `			}` |
|    17086 | 8162 | `			++n;` |
|        5 | 8163 | `		}` |
|        - | 8164 | `		} /* end VmCallArgMap scope */` |
|        - | 8165 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 8166 | `		 * does not return anything.` |
|        - | 8167 | `		 */` |
|   807547 | 8168 | `		PH7_MemObjRelease(pTos);` |
|   807547 | 8169 | `		pTos = &pTos[-nCallArgs];` |
|        - | 8170 | `		/* Allocate an operand stack (via the recycling allocator) and evaluate the` |
|        - | 8171 | `		 * function body. Size it to a tight static bound when the body is statically` |
|        - | 8172 | `		 * modelable (BYTECODE.md stage 7) — the big memory win for deep recursion,` |
|        - | 8173 | `		 * where one such stack lives per frame — falling back to the safe` |
|        - | 8174 | `		 * instruction-count bound otherwise.` |
|        - | 8175 | `		 *` |
|        - | 8176 | `		 * The bound is computed LAZILY on the first call and cached on the func` |
|        - | 8177 | `		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:` |
|        - | 8178 | `		 * self-computing on first use is fail-safe against any body-creation path` |
|        - | 8179 | `		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),` |
|        - | 8180 | `		 * where undersizing is a heap overflow; the amortized cost is one analysis` |
|        - | 8181 | `		 * per function. */` |
|        - | 8182 | `		{` |
|   807547 | 8183 | `			sxu32 nSlots = pVmFunc->nMaxStack;` |
|   807547 | 8184 | `			if( nSlots == 0 ){` |
|    23055 | 8185 | `				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);` |
|    34454 | 8186 | `				sxu32 nTight = VmComputeMaxStack(&(*pVm),` |
|    23050 | 8187 | `					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);` |
|    23055 | 8188 | `				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;` |
|    23055 | 8189 | `				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */` |
|    23055 | 8190 | `				pVmFunc->nMaxStack = nSlots;` |
|    11399 | 8191 | `			}` |
|   807547 | 8192 | `			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);` |
|        - | 8193 | `		}` |
|   807547 | 8194 | `		if( pFrameStack == 0 ){` |
|        - | 8195 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 8196 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 8197 | `				&pVmFunc->sName);` |
|      ! 0 | 8198 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 8199 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 8200 | `			}` |
|      ! 0 | 8201 | `			break;` |
|        - | 8202 | `		}` |
|   403907 | 8203 | `SkipFuncBody:` |
|   812111 | 8204 | `		if( pSelf ){` |
|        - | 8205 | `			/* Push class name */` |
|   511837 | 8206 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|   255916 | 8207 | `		}` |
|        - | 8208 | `		/* Increment nesting level */` |
|   812111 | 8209 | `		pVm->nRecursionDepth++;` |
|   812111 | 8210 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 8211 | `			/* Arg-binding threw: there is no body to run — finish the call` |
|        - | 8212 | `			 * immediately (no record is pushed). */` |
|        - | 8213 | `			VmCallRecord sCallee;` |
|     4569 | 8214 | `			sCallee.pVmFunc = pVmFunc;` |
|     4569 | 8215 | `			sCallee.pFrame = pFrame;` |
|     4569 | 8216 | `			sCallee.pFrameStack = pFrameStack;` |
|     4569 | 8217 | `			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|        - | 8218 | `			/* The body never ran, so this stack is untouched -- but the path is rare` |
|        - | 8219 | `			 * (an argument's own evaluation threw) and sweeping the whole capacity` |
|        - | 8220 | `			 * costs nothing here, so do that rather than reason about the binder. */` |
|     4569 | 8221 | `			sCallee.nLiveTos = sCallee.nStackCap;` |
|     4569 | 8222 | `			sCallee.nLastRef = SXU32_HIGH;` |
|     4569 | 8223 | `			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|     4569 | 8224 | `			sState.pTos = pTos;` |
|     4569 | 8225 | `			sState.pc = pc;` |
|     4569 | 8226 | `			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);` |
|     4569 | 8227 | `			pTos = sState.pTos;` |
|     4569 | 8228 | `			pc = sState.pc;` |
|     4569 | 8229 | `			if( rc == PH7_ABORT ){` |
|        - | 8230 | `				/* Abort processing immeditaley */` |
|      ! 0 | 8231 | `				goto Abort;` |
|     4569 | 8232 | `			}else if( rc == PH7_SUSPEND ){` |
|      ! 0 | 8233 | `				goto Suspend;` |
|     4569 | 8234 | `			}else if( rc == PH7_EXCEPTION ){` |
|      221 | 8235 | `				goto Exception;` |
|        - | 8236 | `			}` |
|     2179 | 8237 | `		}else{` |
|        - | 8238 | `			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a` |
|        - | 8239 | `			 * call record (caller activation + in-flight call) and switch the` |
|        - | 8240 | `			 * loop's locals to the callee — a PHP->PHP call no longer grows` |
|        - | 8241 | `			 * the native stack. The record node is pool-allocated so` |
|        - | 8242 | `			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */` |
|   807547 | 8243 | `			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   807547 | 8244 | `			if( pRec ){` |
|   802749 | 8245 | `				pVm->pIdleCallFrames = (void *)pRec->pPrev;` |
|   401249 | 8246 | `			}else{` |
|     4803 | 8247 | `				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));` |
|        - | 8248 | `			}` |
|   807547 | 8249 | `			if( pRec == 0 ){` |
|        - | 8250 | `				/* OOM: undo the push-time accounting, tear the call down and` |
|        - | 8251 | `				 * raise the non-catchable fatal (the §3.1 OOM convention —` |
|        - | 8252 | `				 * never a silent NULL). */` |
|      ! 0 | 8253 | `				pVm->nRecursionDepth--;` |
|      ! 0 | 8254 | `				if( pSelf ){` |
|      ! 0 | 8255 | `					(void)SySetPop(&pVm->aSelf);` |
|      ! 0 | 8256 | `				}` |
|      ! 0 | 8257 | `				SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|      ! 0 | 8258 | `				VmLeaveFrame(&(*pVm));` |
|      ! 0 | 8259 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 8260 | `				goto Abort;` |
|        - | 8261 | `			}` |
|   807547 | 8262 | `			sState.pTos = pTos;` |
|   807547 | 8263 | `			sState.pc = pc;` |
|   807547 | 8264 | `			sState.pHigh = pHigh;   /* the caller's watermark rides with its pTos */` |
|   807547 | 8265 | `			pRec->sCaller = sState;` |
|   807547 | 8266 | `			pRec->sCall.pVmFunc = pVmFunc;` |
|   807547 | 8267 | `			pRec->sCall.pFrame = pFrame;` |
|   807547 | 8268 | `			pRec->sCall.pFrameStack = pFrameStack;` |
|   807547 | 8269 | `			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|        - | 8270 | `			/* Overwritten with the callee's real watermark when the call finishes;` |
|        - | 8271 | `			 * the safe default is "sweep everything", so a path that ever reaches` |
|        - | 8272 | `			 * VmCallFinish without going through the unwind above still cleans the` |
|        - | 8273 | `			 * whole buffer rather than parking a live value in the pool. */` |
|   807547 | 8274 | `			pRec->sCall.nLiveTos = pRec->sCall.nStackCap;` |
|   807547 | 8275 | `			pRec->sCall.nLastRef = SXU32_HIGH;` |
|   807547 | 8276 | `			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|   807547 | 8277 | `			pRec->pPrev = pCallTop;` |
|   807547 | 8278 | `			pCallTop = pRec;` |
|        - | 8279 | `			/* Switch to the callee activation (what the recursive` |
|        - | 8280 | `			 * VmByteCodeExec entry used to set up). */` |
|   807547 | 8281 | `			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);` |
|   807547 | 8282 | `			pStack = pFrameStack;` |
|   807547 | 8283 | `			pTos = &pStack[-1];` |
|   807547 | 8284 | `			pc = 0;` |
|   807547 | 8285 | `			sState.aInstr = aInstr;` |
|   807547 | 8286 | `			sState.pStack = pStack;` |
|   807547 | 8287 | `			pHigh = pTos;             /* the callee starts with an empty stack */` |
|   807547 | 8288 | `			sState.pHigh = pHigh;` |
|   807547 | 8289 | `			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */` |
|   807547 | 8290 | `			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */` |
|   807547 | 8291 | `			sState.pTos = pTos;` |
|   807547 | 8292 | `			sState.pc = 0;` |
|   807547 | 8293 | `			sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|   807547 | 8294 | `			sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|   807547 | 8295 | `			sState.pEntryFrame = pVm->pFrame;` |
|   807547 | 8296 | `			sState.pResult = pRec->sCaller.pTos;` |
|   807547 | 8297 | `			sState.pLastRef = &pRec->sCall.nLastRef;` |
|        - | 8298 | `			/* Carries the callee for BOTH terminal-OP_DONE screens: the declared` |
|        - | 8299 | `			 * return type (which re-tests VmFuncHasReturnType) and the by-reference` |
|        - | 8300 | `			 * return above. */` |
|   807547 | 8301 | `			sState.pEnforceRetFunc = ( VmFuncHasReturnType(pVmFunc)` |
|   807542 | 8302 | `				\|\| (pVmFunc->iFlags & VM_FUNC_REF_RETURN) ) ? pVmFunc : 0;` |
|   807547 | 8303 | `			sState.is_callback = 0;` |
|   807547 | 8304 | `			sState.bReturnPropagates = 0;` |
|   807547 | 8305 | `			goto VmLoopFetch;` |
|        - | 8306 | `		}` |
|     2179 | 8307 | `	}else{` |
|        - | 8308 | `		/* Look for an installed foreign function.` |
|        - | 8309 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 8310 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 8311 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 8312 | `		 * global fallback for unqualified function calls in namespaces. */` |
|  5136551 | 8313 | `		if( pSiteEntry && bSiteHost ){` |
|        - | 8314 | `			/* The site already knows which host entry this name means (see the` |
|        - | 8315 | `			 * VmCallSite consult above the user-table lookup). */` |
|  3581995 | 8316 | `			pEntry = pSiteEntry;` |
|  1791360 | 8317 | `		}else{` |
|  1554561 | 8318 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 8319 | `		{` |
|  1554561 | 8320 | `		VmCallArgMap *pCallMap2 = pEffCallMap;` |
|  1554561 | 8321 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 8322 | `			/* Compiler-qualified: try short name as global fallback */` |
|       89 | 8323 | `			const char *zShort = sName.zString;` |
|        - | 8324 | `			sxu32 i;` |
|     1375 | 8325 | `			for( i = 0; i < sName.nByte; i++ ){` |
|     1291 | 8326 | `				if( sName.zString[i] == '\\' ){` |
|      103 | 8327 | `					zShort = &sName.zString[i + 1];` |
|       49 | 8328 | `				}` |
|      648 | 8329 | `			}` |
|       89 | 8330 | `			if( zShort != sName.zString ){` |
|       89 | 8331 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       89 | 8332 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       42 | 8333 | `			}` |
|       42 | 8334 | `		}` |
|        - | 8335 | `		} /* end VmCallArgMap namespace scope */` |
|        - | 8336 | `		/* Every builtin call in a namespaced file lands here, having missed the user` |
|        - | 8337 | `		 * table twice on the way -- this is the answer worth remembering. */` |
|  1554561 | 8338 | `		PH7_VmCallSiteRecord(pVm,pInstr,&sName,bEngineCallee,1,pEntry);` |
|        - | 8339 | `		}` |
|  5136551 | 8340 | `		if( pEntry == 0 ){` |
|        - | 8341 | `			/* php accepts the "Class::method" STATIC-callable string everywhere a` |
|        - | 8342 | `			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).` |
|        - | 8343 | `			 * Split on the LAST "::" (php's own zend_memrchr scan) and route through the` |
|        - | 8344 | `			 * shared array-callable machinery ([class-name, method-name]) instead of` |
|        - | 8345 | `			 * warning undefined. */` |
|   240143 | 8346 | `			const char *zCbCls = 0,*zCbMeth = 0;` |
|   240143 | 8347 | `			sxu32 nCbCls = 0,nCbMeth = 0;` |
|   240143 | 8348 | `			int bScoped = PH7_VmCallableStringParts(sName.zString,sName.nByte,` |
|        - | 8349 | `				&zCbCls,&nCbCls,&zCbMeth,&nCbMeth);` |
|   240143 | 8350 | `			if( bScoped ){` |
|        - | 8351 | `				/* Resolve BEFORE dispatching: the shared dispatcher answers SXRET_OK with` |
|        - | 8352 | `` 				 * a NULL result for a pair it cannot resolve, so `$cb='C::nosuch'; $cb();` `` |
|        - | 8353 | `				 * evaluated to NULL with no diagnostic at all — where php throws the same` |
|        - | 8354 | `				 * Errors the ARRAY form of the same call already raised here. (Its own` |
|        - | 8355 | ``				 * `if( bScoped )` block: this scratch buffer and the dispatch block's`` |
|        - | 8356 | `				 * hashmap are both block-head declarations, and the check runs between` |
|        - | 8357 | `				 * them.) */` |
|        - | 8358 | `				char zSmMsg[192];` |
|   100077 | 8359 | `				sxi32 nSmBrc = pVm->nBoundaryRc;` |
|   100077 | 8360 | `				const void *pSmRes = (const void *)pVm->pResumeFrame;` |
|   150114 | 8361 | `				const char *zSmErr = VmCallableClassMethodError(&(*pVm),` |
|    50037 | 8362 | `					PH7_VmExtractClass(&(*pVm),zCbCls,nCbCls,FALSE,0),` |
|    50037 | 8363 | `					zCbCls,nCbCls,zCbMeth,nCbMeth,TRUE,zSmMsg,sizeof(zSmMsg));` |
|   100077 | 8364 | `				if( zSmErr ){` |
|        - | 8365 | `					sxi32 rcSmErr;` |
|       53 | 8366 | `					int bSmRaised = PH7_VmClassLookupRaised(&(*pVm),nSmBrc,pSmRes);` |
|       53 | 8367 | `					if( pInstr->iP2 ){` |
|      ! 0 | 8368 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 8369 | `					}` |
|       53 | 8370 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 8371 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 8372 | `					}` |
|       53 | 8373 | `					PH7_MemObjRelease(pTos);` |
|       53 | 8374 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       53 | 8375 | `					pTos->nIdx = SXU32_HIGH;` |
|       53 | 8376 | `					if( bSmRaised ){` |
|        - | 8377 | `						/* The class name's autoloader threw: land THAT, exactly as the array` |
|        - | 8378 | `						 * form does — php never reports the class missing in this case. */` |
|        6 | 8379 | `						rcSmErr = pVm->nBoundaryRc;` |
|        6 | 8380 | `						pVm->nBoundaryRc = 0;` |
|        6 | 8381 | `						if( rcSmErr == PH7_ABORT ){` |
|      ! 0 | 8382 | `							goto Abort;` |
|        - | 8383 | `						}` |
|        6 | 8384 | `						rc = PH7_EXCEPTION;` |
|       14 | 8385 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 8386 | `					}` |
|       48 | 8387 | `					rcSmErr = VmThrowFromVm(&(*pVm),"Error",zSmErr,(sxu32)SyStrlen(zSmErr));` |
|       48 | 8388 | `					if( rcSmErr == SXERR_ABORT ){` |
|      ! 0 | 8389 | `						goto Abort;` |
|        - | 8390 | `					}` |
|       48 | 8391 | `					rc = rcSmErr;` |
|       62 | 8392 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 8393 | `				}` |
|    50012 | 8394 | `			}` |
|   240092 | 8395 | `			if( bScoped ){` |
|        - | 8396 | `				/* Resolved: hand the callable STRING itself to the shared dispatcher, which` |
|        - | 8397 | ``				 * decodes `Class::method` the same way (it has to, for the callback-argument`` |
|        - | 8398 | `				 * callers). This used to build a throwaway [class,method] map here and enter` |
|        - | 8399 | `				 * through the hashmap branch — two decoders for one spelling. */` |
|        - | 8400 | `				ph7_value sResult;` |
|        - | 8401 | `				sxi32 rcSm;` |
|   150038 | 8402 | `				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100024 | 8403 | `					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|   100026 | 8404 | `				SySetReset(&aArg);` |
|   100040 | 8405 | `				while( pArg < pTos ){` |
|       15 | 8406 | `					SySetPut(&aArg,(const void *)&pArg);` |
|       15 | 8407 | `					pArg++;` |
|        1 | 8408 | `				}` |
|   100026 | 8409 | `				PH7_MemObjInit(pVm,&sResult);` |
|   100026 | 8410 | `				pVm->bDiscardCallback = bResultDropped;   /* see the sibling site */` |
|   150038 | 8411 | `				rcSm = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),` |
|   100024 | 8412 | `					(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|   100026 | 8413 | `				pVm->bDiscardCallback = 0;` |
|   100026 | 8414 | `				SySetReset(&aArg);` |
|   100026 | 8415 | `				if( nCallArgs > 0 ){` |
|       13 | 8416 | `					VmPopOperand(&pTos,nCallArgs);` |
|        6 | 8417 | `				}` |
|   100026 | 8418 | `				if( rcSm == PH7_ABORT ){` |
|      ! 0 | 8419 | `					PH7_MemObjRelease(&sResult);` |
|      ! 0 | 8420 | `					goto Abort;` |
|        - | 8421 | `				}` |
|   100026 | 8422 | `				if( rcSm == PH7_EXCEPTION ){` |
|        - | 8423 | `					sxi32 iResumePc;` |
|   100004 | 8424 | `					PH7_MemObjRelease(&sResult);` |
|   100004 | 8425 | `					if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100001 | 8426 | `						PH7_MemObjRelease(pTos);` |
|        - | 8427 | `						/* Drain the abandoned outer-expression operands` |
|        - | 8428 | ``						 * (`1 + "C::m"()`) to the try's base — one leaked`` |
|        - | 8429 | `						 * slot per caught throw otherwise. */` |
|   300001 | 8430 | `						PH7_RESUME_DRAIN()` |
|   100001 | 8431 | `						pc = iResumePc;` |
|   100001 | 8432 | `						break;` |
|        - | 8433 | `					}` |
|        3 | 8434 | `					goto Exception;` |
|        - | 8435 | `				}` |
|       24 | 8436 | `				PH7_MemObjStore(&sResult,pTos);` |
|       24 | 8437 | `				PH7_MemObjRelease(&sResult);` |
|       24 | 8438 | `				break;` |
|        - | 8439 | `			}` |
|        - | 8440 | `			/* Call to an undefined function is a catchable Error in php 8 — it does` |
|        - | 8441 | `			 * NOT warn and hand back null and carry on, which is what PH7 did (and` |
|        - | 8442 | `			 * which quietly turned a typo into a null-propagating program). */` |
|        - | 8443 | `			{` |
|        - | 8444 | `			SyBlob sMsg;` |
|   140068 | 8445 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   140068 | 8446 | `			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);` |
|        - | 8447 | `			/* Consume this call's captured spread runs so they don't leak into a` |
|        - | 8448 | `			 * later call (this path never reaches VmBuildEffectiveArgMap). */` |
|   140068 | 8449 | `			if( pInstr->iP2 ){` |
|      ! 0 | 8450 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 8451 | `			}` |
|        - | 8452 | `			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the` |
|        - | 8453 | `			 * compile-time arg count on the stack, and this early exit skips the` |
|        - | 8454 | `			 * arg-building loop that the normal path uses, so popping only iP1 would` |
|        - | 8455 | `			 * strand the expanded elements and corrupt the enclosing expression. */` |
|   140068 | 8456 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 8457 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 8458 | `			}` |
|   140068 | 8459 | `			PH7_MemObjRelease(pTos);` |
|   210100 | 8460 | `			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|    70032 | 8461 | `				SyBlobLength(&sMsg));` |
|   140068 | 8462 | `			SyBlobRelease(&sMsg);` |
|   140068 | 8463 | `			if( rc == SXERR_ABORT ){` |
|        6 | 8464 | `				goto Abort;` |
|        - | 8465 | `			}` |
|        - | 8466 | `			/* A catch may have run IN PLACE inside VmThrowFromVm (SXRET_OK +` |
|        - | 8467 | ``			 * recorded resume). An unconditional `goto Exception` here unwound the`` |
|        - | 8468 | `` 			 * exec ANYWAY, so `try { nosuchfn(); } catch (Error $e) {} rest();` `` |
|        - | 8469 | `			 * ran the catch and then silently dropped the rest of the script` |
|        - | 8470 | `			 * (exit 0). Route like every other in-exec throw site. */` |
|   360093 | 8471 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 8472 | `			}` |
|        - | 8473 | `		}` |
|  4896413 | 8474 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 8475 | `		/* D1: resolve deferred plain-var arguments against the builtin's by-ref position` |
|        - | 8476 | `		 * mask (derived from its signature). A by-ref out-param (preg_match's $matches, …)` |
|        - | 8477 | `		 * is materialized so PH7_VmStoreArgByRef can write back — this is what keeps a` |
|        - | 8478 | ``		 * DYNAMIC-name by-ref builtin (`$f='preg_match'; $f($p,$s,$m)`) working now that the`` |
|        - | 8479 | `		 * compile-time mask no longer sees it. Every other (by-value) arg warns + passes` |
|        - | 8480 | `		 * NULL, which is also what call_user_func & friends want. pVm->pFrame is the caller. */` |
|        - | 8481 | `		{` |
|  4896413 | 8482 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,pFunc->nByRefMask,0,0,pEffCallMap);` |
|  4896431 | 8483 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 8484 | `		}` |
|        - | 8485 | `		/* Host function (builtin): build the effective spread-key map so the` |
|        - | 8486 | `		 * name-forwarding builtins (call_user_func & friends) relay string keys as` |
|        - | 8487 | `		 * named args, and — critically — so this call's captured runs are consumed.` |
|        - | 8488 | `		 * pArg is the top base here (a builtin call pops no method-name slot). */` |
|  7344525 | 8489 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  4896358 | 8490 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|  3230788 | 8491 | `NativeCall:` |
|        - | 8492 | `		/* A VM_FUNC_NATIVE method joins here, having done the two steps above for` |
|        - | 8493 | `		 * itself: its by-ref mask comes from the same signature machinery, and its` |
|        - | 8494 | `		 * effective arg map was already built (and this call's spread runs already` |
|        - | 8495 | `		 * consumed) on the method path — building it a second time here would` |
|        - | 8496 | `		 * consume them twice. Everything from this point down is shared verbatim:` |
|        - | 8497 | `		 * a native method IS a host call that happens to carry a receiver. */` |
|        - | 8498 | `		/* php raises a deprecated callee's E_DEPRECATED at the CALL, before the` |
|        - | 8499 | ``		 * body and before every screen under it: `curl_close()` with no argument`` |
|        - | 8500 | `		 * warns first and throws the ArgumentCountError second. A native method` |
|        - | 8501 | `		 * joins this block too, and its notice names the DECLARING class. */` |
|  6461367 | 8502 | `		if( pFunc->pDeprecated ){` |
|      148 | 8503 | `			PH7_VmDeprecatedCallNotice(&(*pVm),pFunc->pDeprecated);` |
|       73 | 8504 | `		}` |
|        - | 8505 | `		/* Start collecting function arguments */` |
|  6461367 | 8506 | `		SySetReset(&aArg);` |
| 16237728 | 8507 | `		while( pArg < pTos ){` |
|  9776366 | 8508 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  9776366 | 8509 | `			pArg++;` |
|        5 | 8510 | `		}` |
|        - | 8511 | `		/* Assume a null return value */` |
|  6461367 | 8512 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 8513 | `		/* Init the call context */` |
|  6461367 | 8514 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 8515 | `		/* Hand the call-site named-argument map to the builtin so name-forwarding` |
|        - | 8516 | `		 * helpers (call_user_func & friends) can relay name: arguments — and the` |
|        - | 8517 | `		 * caller's strict_types mode — to the inner callback. Forwarded whole (not` |
|        - | 8518 | `		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even` |
|        - | 8519 | `		 * when its own call site is purely positional; only the two forwarding` |
|        - | 8520 | `		 * builtins read pArgMap, so this is inert for every other host function. */` |
|  6461367 | 8521 | `		sCtx.pArgMap = pEffCallMap;` |
|        - | 8522 | `		/* Receiver + late-static-binding class for a native METHOD. Both stay 0 for a` |
|        - | 8523 | `		 * plain host function (the locals are initialized once per OP_CALL), so this` |
|        - | 8524 | `		 * is inert on the builtin path. The reference on pNativeOwned belongs to the` |
|        - | 8525 | `		 * caller for the span of the call — the native body borrows it and must not` |
|        - | 8526 | `		 * unref, exactly as a bytecode method's frame $this is borrowed. */` |
|  6461367 | 8527 | `		sCtx.pThis = pNativeRecv;` |
|  6461367 | 8528 | `		sCtx.pCalledClass = pNativeClass;` |
|        - | 8529 | `		{` |
|  6461367 | 8530 | `		int nGiven = (int)SySetUsed(&aArg);` |
|        - | 8531 | ``		/* Bind `name:` arguments to the callee's declared POSITIONS before anything`` |
|        - | 8532 | `		 * reads the vector — the arity screen, the ZPP screen and the C body all take` |
|        - | 8533 | `		 * it positionally. A host function has no compiled parameter records for` |
|        - | 8534 | `		 * VmResolveNamedArgs to walk, so its signature string is the source of names` |
|        - | 8535 | `		 * and defaults (PH7_VmBindNamedArgsToSig). Without this every named argument` |
|        - | 8536 | `		 * simply stayed where it was WRITTEN. */` |
|  6461367 | 8537 | `		if( pEffCallMap && pEffCallMap->bHasNamed && nGiven > 0 ){` |
|      156 | 8538 | `			rc = PH7_VmBindNamedArgsToSig(&sCtx,pFunc,pEffCallMap,&nGiven,` |
|      102 | 8539 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|      105 | 8540 | `			if( rc != SXRET_OK ){` |
|        9 | 8541 | `				goto NativeCallDone;` |
|        - | 8542 | `			}` |
|       47 | 8543 | `		}` |
|        - | 8544 | `		/* php binds a by-reference argument at the CALL, before the callee runs, so a` |
|        - | 8545 | ``		 * non-variable in a `&` position is refused ahead of every ZPP check — and`` |
|        - | 8546 | ``		 * ahead of the too-MANY-arguments one (`array_pop([1,2],5)` is the reference`` |
|        - | 8547 | `		 * Error in php, not an ArgumentCountError). With no argument at all there is` |
|        - | 8548 | `		 * nothing to refuse, which is why the too-FEW check below still speaks first` |
|        - | 8549 | ``		 * for `array_pop()`. */`` |
|  9691929 | 8550 | `		rc = PH7_VmScreenByRefArgShapes(&sCtx,pFunc,pEffCallMap,nGiven,` |
|  6461354 | 8551 | `			(ph7_value **)SySetBasePtr(&aArg));` |
|  6461359 | 8552 | `		if( rc != SXRET_OK ){` |
|       51 | 8553 | `			goto NativeCallDone;` |
|        - | 8554 | `		}` |
|        - | 8555 | `		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum` |
|        - | 8556 | `		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError` |
|        - | 8557 | `		 * before the C routine runs when called with too few arguments — instead` |
|        - | 8558 | `		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""` |
|        - | 8559 | `		 * return. The message wording matches php's ZPP output byte-for-byte. */` |
|  9691864 | 8560 | `		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){` |
|      968 | 8561 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 8562 | `				"%z() expects %s %d argument%s, %d given",` |
|      321 | 8563 | `				&pFunc->sName,` |
|      642 | 8564 | `				pFunc->bAtLeast ? "at least" : "exactly",` |
|      642 | 8565 | `				(int)pFunc->nMinArg,` |
|      642 | 8566 | `				pFunc->nMinArg == 1 ? "" : "s",` |
|      321 | 8567 | `				nGiven);` |
|  6460992 | 8568 | `		}else if( pFunc->bHasMaxArg && nGiven > (int)pFunc->nMaxArg ){` |
|        - | 8569 | `			/* php enforces the MAXIMUM as well, and PHL only did so where a` |
|        - | 8570 | `			 * builtin happened to hand-roll the check (51 of ~650), so` |
|        - | 8571 | ``			 * `microtime(1,2)`, `strlen("a","b")` and friends silently ignored`` |
|        - | 8572 | `			 * the extras. The count comes from the same signature table as the` |
|        - | 8573 | `			 * minimum; a variadic tail leaves nMaxArg at -1 and is exempt.` |
|        - | 8574 | `			 * php says "exactly" when the bounds coincide, "at most" otherwise. */` |
|      323 | 8575 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 8576 | `				"%z() expects %s %d argument%s, %d given",` |
|      106 | 8577 | `				&pFunc->sName,` |
|      176 | 8578 | `				((int)pFunc->nMinArg == (int)pFunc->nMaxArg && !pFunc->bAtLeast) ? "exactly" : "at most",` |
|      212 | 8579 | `				(int)pFunc->nMaxArg,` |
|      212 | 8580 | `				pFunc->nMaxArg == 1 ? "" : "s",` |
|      106 | 8581 | `				nGiven);` |
|  9690689 | 8582 | `		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,` |
|  6460454 | 8583 | `			(ph7_value **)SySetBasePtr(&aArg))) ){` |
|        - | 8584 | `			/* TypeError thrown: rc carries the caught/uncaught status */` |
|      615 | 8585 | `		}else{` |
|        - | 8586 | `			/* The name of the builtin that is RUNNING, for the few diagnostics` |
|        - | 8587 | `			 * raised so deep inside the engine that no ph7_context reaches them` |
|        - | 8588 | `			 * (a stream filter's, from inside a device read) and which php still` |
|        - | 8589 | `			 * prefixes with the caller. Saved and restored: a builtin can call` |
|        - | 8590 | `			 * back into php and reach this line again. */` |
|  6459233 | 8591 | `			SyString *pSavedCallee = pVm->pCalleeName;` |
|        - | 8592 | `			/* php's two callback FORWARDS pass "the answer is being dropped" on to` |
|        - | 8593 | `			 * the callback they drive; every other builtin ignores this. Saved and` |
|        - | 8594 | `			 * restored for the same reason the callee name is. */` |
|  6459233 | 8595 | `			int bSavedHostDiscard = pVm->bHostDiscard;` |
|  6459233 | 8596 | `			pVm->pCalleeName = &pFunc->sName;` |
|  6459233 | 8597 | `			pVm->bHostDiscard = bResultDropped && bLiteralCallee;` |
|        - | 8598 | `			/* Call the foreign function */` |
|  6459233 | 8599 | `			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));` |
|  6459241 | 8600 | `			pVm->bHostDiscard = bSavedHostDiscard;` |
|  6459241 | 8601 | `			pVm->pCalleeName = pSavedCallee;` |
|  6459241 | 8602 | `			if( PH7_CmpRefusalPending(pVm) ){` |
|        - | 8603 | `				/* A native compare handler refused a pair this builtin compared` |
|        - | 8604 | `				 * (in_array, sort, max and switch all drive the same comparator,` |
|        - | 8605 | `				 * which has no throw boundary of its own and only recorded it).` |
|        - | 8606 | `				 * php raises out of the comparison and the builtin never finishes;` |
|        - | 8607 | `				 * this one finishes first and then throws, the way every builtin` |
|        - | 8608 | `				 * whose failure is predicted rather than raised in flight does.` |
|        - | 8609 | `				 * Reported on the call context, so VmHostFuncThrowRc below lands` |
|        - | 8610 | `				 * it exactly as the builtin's own throws are landed -- unless the` |
|        - | 8611 | `				 * builtin ALREADY raised, in which case the first throw wins and` |
|        - | 8612 | `				 * the record is only dropped. */` |
|       16 | 8613 | `				if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND` |
|       17 | 8614 | `				 \|\| sCtx.nThrowRc != 0 ){` |
|      ! 0 | 8615 | `					PH7_CmpRefusalClear(&(*pVm));` |
|      ! 0 | 8616 | `				}else{` |
|       17 | 8617 | `					PH7_CmpRefusalRaiseCtx(&sCtx);` |
|        - | 8618 | `				}` |
|        8 | 8619 | `			}` |
|        - | 8620 | `			/* A host function that RAISED a catchable throw (PH7_VmThrowException)` |
|        - | 8621 | `			 * and still returned PH7_OK reports it here — the throw's catch has` |
|        - | 8622 | `			 * already run in place, so treating the call as a normal return would` |
|        - | 8623 | `			 * resume execution INSIDE the try body the throw abandoned (and, when` |
|        - | 8624 | `			 * uncaught, run on past the reported fatal). The status is recorded on` |
|        - | 8625 | `			 * the call context, so this covers the shared validation helpers whose` |
|        - | 8626 | `			 * callers have no channel to thread a status back. */` |
|  6459241 | 8627 | `			rc = VmHostFuncThrowRc(&sCtx,rc);` |
|        - | 8628 | `		}` |
|  3230796 | 8629 | `NativeCallDone:` |
|  3230574 | 8630 | `		(void)nGiven; /* the named-arg binder's early exit lands here */` |
|        - | 8631 | `		}` |
|        - | 8632 | `		/* Release the call context */` |
|  6461375 | 8633 | `		VmReleaseCallContext(&sCtx);` |
|  6461375 | 8634 | `		if( pNativeOwned ){` |
|        - | 8635 | `			/* Drop the receiver reference the method branch took (pThis->iRef++ before` |
|        - | 8636 | `			 * the target slot was released). A bytecode method hands this to its frame` |
|        - | 8637 | `			 * and lets the teardown do it; a native method has no frame, so it is` |
|        - | 8638 | `			 * dropped here — the one point EVERY route out of the call passes through,` |
|        - | 8639 | `			 * including the throw/abort/suspend ones below. Ordered after the context` |
|        - | 8640 | `			 * release because sCtx.sThis aliases the instance. Always 0 for a plain` |
|        - | 8641 | `			 * host function. */` |
|  1562468 | 8642 | `			PH7_ClassInstanceUnref(pNativeOwned);` |
|  1562468 | 8643 | `			pNativeOwned = 0;` |
|  1562468 | 8644 | `			pNativeRecv = 0;` |
|   781149 | 8645 | `		}` |
|  6461375 | 8646 | `		if( rc == PH7_ABORT ){` |
|        - | 8647 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 8648 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 8649 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      689 | 8650 | `			PH7_MemObjRelease(&sRet);` |
|      689 | 8651 | `			goto Abort;` |
|        - | 8652 | `		}` |
|  6460691 | 8653 | `		if( rc != PH7_SUSPEND && VmInlineOwnedBy(pVm,aInstr,sState.pEntryFrame) ){` |
|        - | 8654 | `			/* A throw raised inside this host function — directly` |
|        - | 8655 | `			 * (PH7_VmThrowException) or by a PHP callback it invoked — was` |
|        - | 8656 | `			 * caught by an INLINE try (generator body) THIS exec owns.` |
|        - | 8657 | `			 * VmThrowInline records only a pc-redirect: a direct builtin throw` |
|        - | 8658 | `			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION` |
|        - | 8659 | `			 * with the redirect pending, so the rc branches below never land` |
|        - | 8660 | `			 * it (pre-existing hole: explode("") or a throwing usort` |
|        - | 8661 | `			 * comparator inside a generator's try lost the catch AND the` |
|        - | 8662 | `			 * yield). Land at the redirect now — its drain to the try's` |
|        - | 8663 | `			 * operand base subsumes the args + name pops. */` |
|       10 | 8664 | `			PH7_MemObjRelease(&sRet);` |
|       32 | 8665 | `			PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 8666 | `		}` |
|  6460683 | 8667 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 8668 | `			/* A callback invoked by this host function threw. If an in-place catch` |
|        - | 8669 | `			 * recorded a resume target owned by THIS body, resume at its landing pad` |
|        - | 8670 | `			 * (consuming the target); otherwise the exception was caught by an outer` |
|        - | 8671 | `			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW` |
|        - | 8672 | `			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which` |
|        - | 8673 | `			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */` |
|        - | 8674 | `			sxi32 iResumePc;` |
|    16388 | 8675 | `			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 8676 | `				/* Caught by an outer exec, or not caught here: propagate. */` |
|     4430 | 8677 | `				goto Exception;` |
|        - | 8678 | `			}` |
|        - | 8679 | `			/* Exception was caught in place by THIS body's try: pop args and the` |
|        - | 8680 | `			 * result slot, then drain any abandoned outer-expression operands to` |
|        - | 8681 | `			 * the try's base and resume. */` |
|    11963 | 8682 | `			PH7_MemObjRelease(&sRet);` |
|    11963 | 8683 | `			if( nCallArgs > 0 ){` |
|    11381 | 8684 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|     5688 | 8685 | `			}` |
|    11963 | 8686 | `			VmPopOperand(&pTos,1);` |
|    16507 | 8687 | `			PH7_RESUME_DRAIN()` |
|    11963 | 8688 | `			pc = iResumePc;` |
|    11963 | 8689 | `			break;` |
|        - | 8690 | `		}` |
|  6444300 | 8691 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 8692 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 8693 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 8694 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 8695 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 8696 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 8697 | `			 * body), the user-function path above will handle re-saving. */` |
|      395 | 8698 | `			PH7_MemObjRelease(&sRet);` |
|      395 | 8699 | `			if( nCallArgs > 0 ){` |
|      387 | 8700 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      191 | 8701 | `			}` |
|        - | 8702 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 8703 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|      395 | 8704 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|      395 | 8705 | `			goto Suspend;` |
|        - | 8706 | `		}` |
|  6443910 | 8707 | `		if( nCallArgs > 0 ){` |
|        - | 8708 | `			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack` |
|        - | 8709 | `			 * expanded the compile-time arg count on the stack, so popping iP1` |
|        - | 8710 | `			 * strands the extra elements (or, for an unpack that expanded to fewer` |
|        - | 8711 | `			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,` |
|        - | 8712 | `			 * reading a bogus value whose stray flags sent MemObjStore into the` |
|        - | 8713 | `			 * hashmap-release path and hung). Mirrors every other CALL exit. The` |
|        - | 8714 | `			 * function-name slot (pTos) receives the return value below. */` |
|  6349894 | 8715 | `			VmPopOperand(&pTos,nCallArgs);` |
|  3174955 | 8716 | `		}` |
|        - | 8717 | `		/* Save foreign function return value into the (now top) function-name slot */` |
|  6443910 | 8718 | `		PH7_MemObjStore(&sRet,pTos);` |
|        - | 8719 | `		/* ...and clear that slot's index. It is one of the call's own argument slots,` |
|        - | 8720 | `		 * still carrying the variable index the argument was loaded with, and` |
|        - | 8721 | `		 * PH7_MemObjStore does not touch nIdx — so a builtin's return value came back` |
|        - | 8722 | ``		 * looking like an lvalue for the caller's variable (`f(strtoupper($b))` with`` |
|        - | 8723 | ``		 * `function f(&$x)` overwrote `$b`). No host function returns by reference. */`` |
|  6443910 | 8724 | `		pTos->nIdx = SXU32_HIGH;` |
|  6443910 | 8725 | `		PH7_MemObjRelease(&sRet);` |
|        - | 8726 | `	}` |
|  6448258 | 8727 | `	break;` |
|        - | 8728 | `				  }` |
|        - | 8729 | `/*` |
|        - | 8730 | ` * OP_CONSUME: P1 * *` |
|        - | 8731 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 8732 | ` */` |
|    91481 | 8733 | `case PH7_OP_CONSUME: {` |
|        - | 8734 | `	VmOpRc rcOp;` |
|   182808 | 8735 | `	sState.pTos = pTos;` |
|   182808 | 8736 | `	sState.pc = pc;` |
|   182808 | 8737 | `	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);` |
|   182808 | 8738 | `	pTos = sState.pTos;` |
|   182808 | 8739 | `	pc = sState.pc;` |
|   182808 | 8740 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 8741 | `		goto Abort;` |
|   182806 | 8742 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       14 | 8743 | `		goto Exception;` |
|        - | 8744 | `	}` |
|   182789 | 8745 | `	break;` |
|        - | 8746 | `					  }` |
|        - | 8747 |  |
|        - | 8748 | `		} /* Switch() */` |
| 70453173 | 8749 | `		pc++; /* Next instruction in the stream */` |
|        5 | 8750 | `	} /* For(;;) */` |
|  2211827 | 8751 | `Done:` |
|        - | 8752 | `	/* A stacked callee completing lands here too (its result is already in` |
|        - | 8753 | `	 * sState.pResult — the caller's operand slot); Unwind's first iteration` |
|        - | 8754 | `	 * bottoms out identically for the record-less case. */` |
|  4423061 | 8755 | `	rc = SXRET_OK;` |
|  4423061 | 8756 | `	goto Unwind;` |
|     1001 | 8757 | `Suspend:` |
|     2007 | 8758 | `	rc = PH7_SUSPEND;` |
|     2007 | 8759 | `	if( pCallTop != 0 ){` |
|        - | 8760 | `		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment` |
|        - | 8761 | `		 * instead of the lossy unwind. pc/nTos of the innermost activation were` |
|        - | 8762 | `		 * already saved into the ctx by VmSuspendCtx; capture the rest (the` |
|        - | 8763 | `		 * record chain, the innermost activation, the suspend-time top frame)` |
|        - | 8764 | `		 * so resume re-enters HERE, inside the innermost callee, like php. The` |
|        - | 8765 | `		 * records / frames / operand stacks stay alive — nothing is freed. Only` |
|        - | 8766 | `		 * fibers reach this (generators yield only at their body level, pCallTop` |
|        - | 8767 | `		 * == 0); a suspend inside a C->PHP callback was already rejected with a` |
|        - | 8768 | `		 * FiberError before it could arrive here. */` |
|      323 | 8769 | `		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));` |
|      323 | 8770 | `		if( pSeg == 0 ){` |
|        - | 8771 | `			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST` |
|        - | 8772 | `			 * pc/nTos into the ctx, so the lossy body-level fallback would resume` |
|        - | 8773 | `			 * the callee's pc against the body stack — silent corruption, exactly` |
|        - | 8774 | `			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the` |
|        - | 8775 | `			 * §3.1 convention shared with the stage-2 record-alloc OOM site). */` |
|      ! 0 | 8776 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 8777 | `			rc = PH7_ABORT;` |
|      ! 0 | 8778 | `			goto Unwind;` |
|        - | 8779 | `		}` |
|      323 | 8780 | `		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */` |
|      323 | 8781 | `		sState.pHigh = ( pHigh > pTos ) ? pHigh : pTos;` |
|      323 | 8782 | `		pSeg->sState = sState;` |
|      323 | 8783 | `		pSeg->pCallTop = pCallTop;` |
|      323 | 8784 | `		pSeg->pTopFrame = pVm->pFrame;` |
|      323 | 8785 | `		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;` |
|      323 | 8786 | `		pSeg->nOldFinBase = pVm->pActiveCtx ? pVm->pActiveCtx->nFinallyBase : 0;` |
|        - | 8787 | `		{` |
|        - | 8788 | `			VmCallFrame *pRec;` |
|      323 | 8789 | `			pSeg->nRecords = 0;` |
|      643 | 8790 | `			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){` |
|      325 | 8791 | `				pSeg->nRecords++;` |
|      165 | 8792 | `			}` |
|        - | 8793 | `		}` |
|      323 | 8794 | `		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;` |
|        - | 8795 | `		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */` |
|      323 | 8796 | `		SySetRelease(&aArg);` |
|      323 | 8797 | `		return PH7_SUSPEND;` |
|        - | 8798 | `	}` |
|     1689 | 8799 | `	goto Unwind;` |
|      511 | 8800 | `Abort:` |
|     1026 | 8801 | `	rc = PH7_ABORT;` |
|     1026 | 8802 | `	goto Unwind;` |
|   305667 | 8803 | `Exception:` |
|   611284 | 8804 | `	rc = PH7_EXCEPTION;` |
|   611279 | 8805 | `	goto Unwind;` |
|  2518847 | 8806 | `Unwind:` |
|        - | 8807 | `	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration` |
|        - | 8808 | `	 * finishes the top record exactly as the old per-level native return did:` |
|        - | 8809 | `	 * for ABORT/EXCEPTION, first run what the popped activation's own` |
|        - | 8810 | `	 * Abort/Exception label used to do (clear its pending return, release its` |
|        - | 8811 | `	 * operands — a stacked activation never has bReturnPropagates set), then` |
|        - | 8812 | `	 * VmCallFinish routes in the restored caller (an in-place catch there` |
|        - | 8813 | `	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —` |
|        - | 8814 | `	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible` |
|        - | 8815 | `	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).` |
|        - | 8816 | `	 * At the bottom, VmExecFinalize hands the status to the native caller. */` |
|  2662361 | 8817 | `	for(;;){` |
|  5331834 | 8818 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|        - | 8819 | `			/* Drop any pending hook-RMW write-backs this activation armed — its` |
|        - | 8820 | `			 * statement is abandoned (only the innermost activation at throw time` |
|        - | 8821 | `			 * can own entries: the armed window spans exactly one instruction, so` |
|        - | 8822 | `			 * no OP_CALL record ever intervenes). */` |
|   913553 | 8823 | `			while( SySetUsed(&pVm->aHookRmw) > 0` |
|   913554 | 8824 | `			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){` |
|        3 | 8825 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 8826 | `			}` |
|   456748 | 8827 | `		}` |
|  5343304 | 8828 | `		if( pCallTop == 0 ){` |
|  4535962 | 8829 | `			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);` |
|        - | 8830 | `		}` |
|   807347 | 8831 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|   607001 | 8832 | `			VmClearFramePending(sState.pEntryFrame);` |
|   619343 | 8833 | `			while( pTos >= pStack ){` |
|    12347 | 8834 | `				PH7_MemObjRelease(pTos);` |
|    12347 | 8835 | `				pTos--;` |
|        5 | 8836 | `			}` |
|   303471 | 8837 | `		}` |
|   807347 | 8838 | `		if( rc != PH7_SUSPEND ){` |
|        - | 8839 | `			/* The finishing callee's own leaked finally actions must not survive` |
|        - | 8840 | `			 * into the caller (whose next OP_END_FINALLY would mis-pop them). */` |
|   807347 | 8841 | `			VmDiscardFinallyActions(&(*pVm),sState.nFinallyActBase);` |
|   403535 | 8842 | `		}` |
|        - | 8843 | `		{` |
|   807347 | 8844 | `			VmCallFrame *pRec = pCallTop;` |
|        - | 8845 | `			/* What the finishing callee ever touched of its own operand stack. The` |
|        - | 8846 | `			 * live top is in it too: an op handler that pushed and then threw hands` |
|        - | 8847 | `			 * control to the drain above without another instruction fetch, so pTos` |
|        - | 8848 | `			 * can be above the last sampled watermark. VmCallFinish's recycle sweeps` |
|        - | 8849 | `			 * exactly this much and leaves the rest of the buffer alone. */` |
|   807347 | 8850 | `			if( pTos > pHigh ){` |
|      ! 0 | 8851 | `				pHigh = pTos;` |
|      ! 0 | 8852 | `			}` |
|   807347 | 8853 | `			pRec->sCall.nLiveTos = (pHigh >= pStack) ? (sxu32)(pHigh - pStack) + 1 : 0;` |
|   807347 | 8854 | `			sState = pRec->sCaller;` |
|   807347 | 8855 | `			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);` |
|   807347 | 8856 | `			pCallTop = pRec->pPrev;` |
|   807347 | 8857 | `			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   807347 | 8858 | `			pVm->pIdleCallFrames = (void *)pRec;` |
|   807347 | 8859 | `			aInstr = sState.aInstr;` |
|   807347 | 8860 | `			pStack = sState.pStack;` |
|   807347 | 8861 | `			pTos = sState.pTos;` |
|   807347 | 8862 | `			pHigh = sState.pHigh;` |
|   807347 | 8863 | `			pc = sState.pc;` |
|        - | 8864 | `		}` |
|   807347 | 8865 | `		if( rc == PH7_OK ){` |
|   512558 | 8866 | `			pc++; /* the loop-bottom increment this OP_CALL missed */` |
|   512558 | 8867 | `			goto VmLoopFetch;` |
|        - | 8868 | `		}` |
|        5 | 8869 | `	}` |
|  2270460 | 8870 | `}` |
|        - | 8871 |  |
