# src/ph7/vm_exec.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4621/5098 lines (90.64%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits |  Line | Source |
| --------: | ----: | :--- |
|         - |     1 | `/**` |
|         - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |     5 | ` */` |
|         - |     6 | `#include "ph7int.h"` |
|         - |     7 | `#include <math.h>` |
|         - |     8 | `/*` |
|         - |     9 | ` * Section:` |
|         - |    10 | ` *    The bytecode interpreter: VmByteCodeExec, its dispatch loop` |
|         - |    11 | ` *    (VmByteCodeExecBody) and the operand-stack growth + trampoline` |
|         - |    12 | ` *    call-finish machinery the loop is welded to. Split from vm.c after the` |
|         - |    13 | ` *    vm_ops_* extractions shrank the loop enough to fit its own unit; the` |
|         - |    14 | ` *    remaining inline arms (CALL, YIELD/YIELD_FROM, the finally family,` |
|         - |    15 | ` *    SPREAD, and the hot loads/stores/arith) share loop-invocation state` |
|         - |    16 | ` *    (pCallTop, ppBaseOwner/pnBaseCap, Suspend/SkipFuncBody/Done labels)` |
|         - |    17 | ` *    that must stay inside one function.` |
|         - |    18 | ` * Status:` |
|         - |    19 | ` *    Stable.` |
|         - |    20 | ` */` |
|         - |    21 | `/*` |
|         - |    22 | ` * OP_SPREAD stack growth (removes the old VM_STACK_GUARD expansion cap).` |
|         - |    23 | ` *` |
|         - |    24 | ` * The operand stack of the CURRENTLY-RUNNING activation is about to receive more` |
|         - |    25 | ` * spread elements than its remaining slack holds. Realloc the buffer so the` |
|         - |    26 | ` * expansion — and the rest of the body's normal pushes — fit, then fix up every` |
|         - |    27 | ` * pointer that aimed into the old buffer. Returns 1 on success (the caller may` |
|         - |    28 | ` * proceed with the expansion), 0 on OOM (the caller keeps the old buffer and` |
|         - |    29 | ` * raises the historical guard error, preserving pre-growth soundness).` |
|         - |    30 | ` *` |
|         - |    31 | ` * nNeed is the live-slot count the stack must hold after the pending expansion;` |
|         - |    32 | ` * the grown capacity adds VM_STACK_GUARD back on top so the remainder of the body` |
|         - |    33 | ` * keeps its slack. ONLY this activation's references move, and they are all here:` |
|         - |    34 | ` *   - the dispatch locals pStack/pTos (via *ppStack / *ppTos) and the boundary` |
|         - |    35 | ` *     copies in sState (pState->pStack/pTos + the tracked capacity nStackCap)` |
|         - |    36 | ` *   - the owner slot: for a trampoline callee, its record's pFrameStack and the` |
|         - |    37 | ` *     recycle capacity nStackCap; for the base activation (top-level, mini-program,` |
|         - |    38 | ` *     coroutine body, callback), *ppBaseOwner / *pnBaseCap — whatever storage the` |
|         - |    39 | ` *     native entry frees (a local, pVm->aOps, or pCtx->pStack/nStackCap)` |
|         - |    40 | ` *   - aSpreadRun[].pStart entries anchored in the OLD buffer (this call's earlier` |
|         - |    41 | ` *     spreads) — an unfixed pStart would desync PHP 8.1 named-arg replay after the` |
|         - |    42 | ` *     realloc. Enclosing activations own DISTINCT buffers, so their runs (pStart` |
|         - |    43 | ` *     outside [pOld, pOld+nOldCap)) are deliberately left untouched.` |
|         - |    44 | ` */` |
|    105279 |    45 | `static int VmGrowOperandStack(ph7_vm *pVm, sxu32 nNeed,` |
|         - |    46 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|         - |    47 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|         5 |    48 | `{` |
|    105284 |    49 | `	ph7_value *pOld = *ppStack;` |
|    105284 |    50 | `	sxu32 nOldCap = pState->nStackCap;` |
|         - |    51 | `	sxu32 nNewCap, nReq, nMaxCap, i, nRun;` |
|         - |    52 | `	ph7_value *pNew;` |
|         - |    53 | `	VmSpreadRun *aRun;` |
|         - |    54 | `	/* Size the grown buffer to the post-expansion live depth (nNeed) PLUS the` |
|         - |    55 | `	 * activation's original full budget (nStackOrig = nMaxStack + VM_STACK_GUARD) as` |
|         - |    56 | `	 * headroom. That headroom is essential: only OP_SPREAD re-checks capacity, so any` |
|         - |    57 | `	 * ordinary arg pushes that FOLLOW this spread in the same call (e.g.` |
|         - |    58 | ``	 * `foo(...$big, a1..aN)`) must fit — and the rest of the body adds at most`` |
|         - |    59 | `	 * nMaxStack above the current point. Crucially the headroom is relative to the` |
|         - |    60 | `	 * ORIGINAL capacity, NOT the grown nOldCap: basing it on nOldCap would ratchet` |
|         - |    61 | `	 * capacity up on every spread (nOldCap already includes prior growth), leaking` |
|         - |    62 | `	 * without bound across statements that share one operand stack until it pins at` |
|         - |    63 | `	 * nMaxCap. nNeed resets between statements (the stack pops back), so this does not. */` |
|    105284 |    64 | `	nReq = nNeed + pState->nStackOrig;` |
|    105284 |    65 | `	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */` |
|       ! 0 |    66 | `		nReq = SXU32_HIGH;` |
|       ! 0 |    67 | `	}` |
|         - |    68 | `	/* SyMemBackendRealloc's size argument is sxu32, so the byte count` |
|         - |    69 | `	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack` |
|         - |    70 | `	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and` |
|         - |    71 | `	 * the init loop below would run off it. If the true requirement exceeds the` |
|         - |    72 | `	 * representable cap, treat it as OOM (the caller raises the guard error). */` |
|    105284 |    73 | `	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);` |
|    105284 |    74 | `	if( nReq > nMaxCap ){` |
|       ! 0 |    75 | `		return 0;` |
|         - |    76 | `	}` |
|    105284 |    77 | `	if( nReq <= nOldCap ){` |
|    100699 |    78 | `		return 1; /* already fits — no growth needed */` |
|         - |    79 | `	}` |
|      4590 |    80 | `	nNewCap = nReq;` |
|         - |    81 | `	/* Amortize repeated spreads in one argument list: at least double, but never` |
|         - |    82 | `	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't` |
|         - |    83 | `	 * itself overflow.) */` |
|      4590 |    84 | `	if( nNewCap < nOldCap * 2 ){` |
|      4590 |    85 | `		sxu32 nDbl = nOldCap * 2;` |
|      4590 |    86 | `		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }` |
|      4590 |    87 | `		if( nNewCap < nDbl ){ nNewCap = nDbl; }` |
|      2245 |    88 | `	}` |
|      6835 |    89 | `	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,` |
|      2245 |    90 | `		nNewCap * sizeof(ph7_value));` |
|      4590 |    91 | `	if( pNew == 0 ){` |
|       ! 0 |    92 | `		return 0; /* OOM: caller keeps pOld and raises the guard error */` |
|         - |    93 | `	}` |
|         - |    94 | `	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */` |
|    171219 |    95 | `	for( i = nOldCap; i < nNewCap; i++ ){` |
|    166634 |    96 | `		PH7_MemObjInit(pVm, &pNew[i]);` |
|    166634 |    97 | `		pNew[i].nIdx = SXU32_HIGH;` |
|     81942 |    98 | `	}` |
|         - |    99 | `	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */` |
|      4590 |   100 | `	*ppTos = pNew + (*ppTos - pOld);` |
|      4590 |   101 | `	*ppStack = pNew;` |
|      4590 |   102 | `	pState->pStack = pNew + (pState->pStack - pOld);` |
|      4590 |   103 | `	pState->pTos = pNew + (pState->pTos - pOld);` |
|      4590 |   104 | `	pState->nStackCap = nNewCap;` |
|      4590 |   105 | `	if( pCallTop ){` |
|         - |   106 | `		/* This activation is a trampoline callee: its record owns the buffer. */` |
|      4468 |   107 | `		pCallTop->sCall.pFrameStack = pNew;` |
|      4468 |   108 | `		pCallTop->sCall.nStackCap = nNewCap;` |
|      2189 |   109 | `	}else{` |
|         - |   110 | `		/* Base activation: the native entry frees *ppBaseOwner (and, for a` |
|         - |   111 | `		 * resumable coroutine, persists *pnBaseCap across suspend/resume). */` |
|       127 |   112 | `		if( ppBaseOwner ){ *ppBaseOwner = pNew; }` |
|       127 |   113 | `		if( pnBaseCap ){ *pnBaseCap = nNewCap; }` |
|         - |   114 | `	}` |
|         - |   115 | `	/* Re-anchor this activation's captured spread runs (pStart into the old` |
|         - |   116 | `	 * buffer). Enclosing-activation runs live in other buffers — leave them. */` |
|      4590 |   117 | `	nRun = SySetUsed(&pVm->aSpreadRun);` |
|      4590 |   118 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      4590 |   119 | `	for( i = 0; i < nRun; i++ ){` |
|       ! 0 |   120 | `		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){` |
|       ! 0 |   121 | `			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);` |
|       ! 0 |   122 | `		}` |
|       ! 0 |   123 | `	}` |
|      4590 |   124 | `	return 1;` |
|     52597 |   125 | `}` |
|         - |   126 | `/*` |
|         - |   127 | ` * Ensure the running activation's operand stack can hold a spread of nEntry` |
|         - |   128 | ` * elements pushed at *ppTos (the source slot becomes the first element, so the` |
|         - |   129 | ` * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.` |
|         - |   130 | ` * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.` |
|         - |   131 | ` */` |
|      1633 |   132 | `static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,` |
|         - |   133 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|         - |   134 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|         5 |   135 | `{` |
|         - |   136 | `	sxu32 nNeed;` |
|      1638 |   137 | `	if( nEntry == 0 ){` |
|       262 |   138 | `		return 1; /* empty spread never grows the stack */` |
|         - |   139 | `	}` |
|      1380 |   140 | `	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);` |
|      2020 |   141 | `	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,` |
|       640 |   142 | `		pCallTop, ppBaseOwner, pnBaseCap);` |
|       774 |   143 | `}` |
|         - |   144 | `/*` |
|         - |   145 | ` * Terminal teardown of one VmByteCodeExec activation — the former` |
|         - |   146 | ` * Done/Suspend/Abort/Exception label bodies, one home.` |
|         - |   147 | ` *` |
|         - |   148 | ` * SXRET_OK (Done): whenever the REAL body returns, its pending-return slot` |
|         - |   149 | ` * must be empty — the materialize at OP_DONE/OP_POP_EXCEPTION already moved` |
|         - |   150 | ` * the value into pResult and cleared bHasRet, so the clear is normally a` |
|         - |   151 | ` * no-op; it only fires on a path that reached Done with a stale slot,` |
|         - |   152 | ` * preventing a leak. The !bReturnPropagates guard is essential: a` |
|         - |   153 | ` * catch/finally MINI-PROGRAM runs in its body's own frame (VmLocalExec adds` |
|         - |   154 | ` * no frame), so pEntryFrame is that body — wiping its slot would destroy the` |
|         - |   155 | ` * return the body is about to take.` |
|         - |   156 | ` * PH7_SUSPEND: a generator/fiber body never suspends mid-completion of a` |
|         - |   157 | ` * catch/finally return, so its frame's slot is empty (nothing to clear) and` |
|         - |   158 | ` * its operand stack is preserved in place — the ctx owns it.` |
|         - |   159 | ` * PH7_ABORT / PH7_EXCEPTION: abnormal unwind — discard the body's pending` |
|         - |   160 | ` * return (an escaping exception supersedes it, per PHP) and release every` |
|         - |   161 | ` * live operand slot down to the activation's stack base.` |
|         - |   162 | ` */` |
|         - |   163 | `static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase);` |
|   4594417 |   164 | `static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)` |
|         5 |   165 | `{` |
|   4594422 |   166 | `	if( rcTerm != PH7_SUSPEND ){` |
|   4592650 |   167 | `		VmDiscardFinallyActions(&(*pVm),pState->nFinallyActBase);` |
|   2296062 |   168 | `	}` |
|   4594422 |   169 | `	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){` |
|   3123483 |   170 | `		VmClearFramePending(pState->pEntryFrame);` |
|   1561506 |   171 | `	}` |
|   4594422 |   172 | `	SySetRelease(pArg);` |
|   4594422 |   173 | `	if( rcTerm == PH7_ABORT \|\| rcTerm == PH7_EXCEPTION ){` |
|    617831 |   174 | `		while( pTos >= pState->pStack ){` |
|    310392 |   175 | `			PH7_MemObjRelease(pTos);` |
|    310392 |   176 | `			pTos--;` |
|         5 |   177 | `		}` |
|    153721 |   178 | `	}` |
|   4594422 |   179 | `	return rcTerm;` |
|         5 |   180 | `}` |
|         - |   181 | `/*` |
|         - |   182 | ` * Discard the pending finally ACTIONS an activation queued but never consumed,` |
|         - |   183 | ` * releasing what they own (an FA_RETHROW's held exception ref; an FA_RETURN's` |
|         - |   184 | `` * value). A `return` inside a finally that was ENTERED VIA THE THROW REDIRECT`` |
|         - |   185 | ` * (VmThrowInline queued an FA_RETHROW and jumped into the finally body)` |
|         - |   186 | ` * short-circuits that finally's OP_END_FINALLY — OP_SET_FINALLY_RET finds no` |
|         - |   187 | ` * remaining handler and completes the body directly — so the queued action was` |
|         - |   188 | ` * ORPHANED on pVm->aFinallyAction. Left there, an ENCLOSING function's next` |
|         - |   189 | ` * OP_END_FINALLY pops the orphan instead of its own action (re-raising a` |
|         - |   190 | ` * swallowed exception / hijacking control), and on a coroutine body it leaks` |
|         - |   191 | ` * into the resumer's scope. Called at every activation end (record pop and` |
|         - |   192 | ` * exec finalize), never on SUSPEND (a suspended body's pending actions are` |
|         - |   193 | ` * parked base-relative by VmParkCtxState and must survive).` |
|         - |   194 | ` */` |
|   5478003 |   195 | `static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase)` |
|         5 |   196 | `{` |
|   5478020 |   197 | `	while( SySetUsed(&pVm->aFinallyAction) > nBase ){` |
|        15 |   198 | `		VmFinallyAction *pAct = (VmFinallyAction *)SySetPeek(&pVm->aFinallyAction);` |
|        15 |   199 | `		if( pAct->eKind == PH7_FA_RETHROW && pAct->pExc ){` |
|         9 |   200 | `			PH7_ClassInstanceUnref(pAct->pExc);` |
|        10 |   201 | `		}else if( pAct->eKind == PH7_FA_RETURN ){` |
|         3 |   202 | `			PH7_MemObjRelease(&pAct->sRet);` |
|         1 |   203 | `		}` |
|        15 |   204 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|         3 |   205 | `	}` |
|   5478008 |   206 | `}` |
|         - |   207 | `/*` |
|         - |   208 | ` * Finish one user-function call at the "pop" boundary of the callee's` |
|         - |   209 | ` * activation: pop-time accounting (recursion depth, aSelf), by-ref-return` |
|         - |   210 | ` * fixup, callee-threw routing (inline resume / recorded resume / propagate),` |
|         - |   211 | ` * operand-stack free and frame teardown. Extracted verbatim from the OP_CALL` |
|         - |   212 | ` * epilogue so the trampoline that pops records can run the same` |
|         - |   213 | ` * code when a record is popped at OP_DONE instead of after a native return.` |
|         - |   214 | ` * pCaller->pc / pCaller->pTos are authoritative across this boundary; the` |
|         - |   215 | ` * dispatch loop syncs its locals around the call. Returns PH7_OK (continue` |
|         - |   216 | ` * the caller, possibly at a redirected pc), PH7_ABORT, PH7_SUSPEND (the ctx` |
|         - |   217 | ` * state was re-saved at the caller's level) or PH7_EXCEPTION.` |
|         - |   218 | ` */` |
|    890188 |   219 | `static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)` |
|         5 |   220 | `{` |
|         - |   221 | `	/* Decrement nesting level */` |
|    890193 |   222 | `	pVm->nRecursionDepth--;` |
|    890193 |   223 | `	if( pCallee->bSelfPushed ){` |
|         - |   224 | `		/* Pop class name */` |
|    514549 |   225 | `		(void)SySetPop(&pVm->aSelf);` |
|    257272 |   226 | `	}` |
|    890193 |   227 | `	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|         - |   228 | `` 		/* Return by reference: the caller binds to the very slot the `return` `` |
|         - |   229 | `		 * named. php keeps that slot alive because the reference holds it, and` |
|         - |   230 | `		 * the slot may well be one the frame about to be torn down owns -- a` |
|         - |   231 | `		 * local, a parameter, or an element of a local array. PIN it here, the` |
|         - |   232 | `		 * way a by-reference closure capture is pinned (VmPinMemObjSlot's own` |
|         - |   233 | `		 * comment records the same trade: php frees by refcount, PHL by a` |
|         - |   234 | `		 * script-lifetime pin). Before this the engine DROPPED the reference for` |
|         - |   235 | `		 * a local (behind two notices php does not have) and left the caller` |
|         - |   236 | `		 * aimed at an element the frame had already released, so` |
|         - |   237 | ``		 * `function &f($x){ $a = [$x]; return $a[0]; }` handed back NULL.`` |
|         - |   238 | `		 * A return with nothing to bind is reported at the RETURN instead, in` |
|         - |   239 | `		 * php's own words -- see the terminal OP_DONE.` |
|         - |   240 | `		 * The pin is what a refcount would be: it never comes back, so a by-ref` |
|         - |   241 | `		 * return naming a fresh frame slot retains it for the run (~0.6 KB a call;` |
|         - |   242 | `		 * measured at ~0.6 KB). A slot that already outlives the frame -- a` |
|         - |   243 | `		 * static, a global, a property -- is pinned harmlessly: the flag only stops` |
|         - |   244 | ``		 * its INDEX being recycled, and `unset($GLOBALS['G'])` still answers php's. */`` |
|        95 |   245 | `		if( pCallee->nLastRef != SXU32_HIGH ){` |
|        89 |   246 | `			VmPinMemObjSlot(&(*pVm),pCallee->nLastRef);` |
|        42 |   247 | `		}` |
|        95 |   248 | `		pCaller->pTos->nIdx = pCallee->nLastRef;` |
|         - |   249 | `		/* The callee PROMISED a reference, whether or not it had one to give:` |
|         - |   250 | `		 * php says nothing further at the call site either way. */` |
|        95 |   251 | `		pCaller->pTos->iFlags \|= MEMOBJ_AUX_REFRET;` |
|        50 |   252 | `	}else{` |
|         - |   253 | `		/* A by-VALUE return is a TEMPORARY — php's IS_TMP_VAR — and must not look like` |
|         - |   254 | `		 * an lvalue. The result lands in the slot the call's first ARGUMENT occupied,` |
|         - |   255 | `		 * which still carried that argument's variable index, so the returned value` |
|         - |   256 | ``		 * inherited it: `f(id($z))` with `function f(&$x)` aliased and overwrote `$z`,`` |
|         - |   257 | `		 * a variable neither function was given by reference. Every call form was` |
|         - |   258 | `		 * affected (function, method, static, closure, nested) and every one of them` |
|         - |   259 | `		 * silently. Clearing it here also lets the call site see the temporary for what` |
|         - |   260 | `		 * it is, which is what php's "Only variables should be passed by reference"` |
|         - |   261 | `		 * notice is raised on. */` |
|    890103 |   262 | `		pCaller->pTos->nIdx = SXU32_HIGH;` |
|         - |   263 | `	}` |
|    890193 |   264 | `	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|         - |   265 | `		/* The callee threw (or its finally threw past it). If an in-place catch` |
|         - |   266 | `		 * recorded a resume target owned by THIS caller's body, resume there and` |
|         - |   267 | `		 * consume the target (VmRecordedResume); when the catcher is an outer exec` |
|         - |   268 | `		 * — or this is a callback with no bytecode to resume into — propagate so` |
|         - |   269 | `		 * the owning exec lands. This replaces the old "is the caller's parent a` |
|         - |   270 | `		 * resumable try frame" test, which resumed at the caller's OWN try even` |
|         - |   271 | `		 * when the finally's throw was caught further out, losing that catch's` |
|         - |   272 | `		 * return (ROOT B, face c). */` |
|         - |   273 | `		sxi32 iResumePc;` |
|    613713 |   274 | `		VmFrame *pParentFrame = pCallee->pFrame->pParent;` |
|    613713 |   275 | `		if( !pCaller->is_callback && VmInlineOwnedBy(pVm,pCaller->aInstr,pCaller->pEntryFrame) ){` |
|         - |   276 | `			/* ROOT C: the callee's throw was caught by an inline try in THIS caller` |
|         - |   277 | `			 * (generator body). Drain the operand stack (incl. the unwritten result` |
|         - |   278 | `			 * slot) to the try's base and land at its catch/finally. */` |
|         5 |   279 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iInlineDrain ){` |
|         3 |   280 | `				PH7_MemObjRelease(pCaller->pTos);` |
|         3 |   281 | `				pCaller->pTos--;` |
|         1 |   282 | `			}` |
|         3 |   283 | `			pCaller->pc = (sxi32)pVm->iInlinePc - 1;` |
|         3 |   284 | `			pVm->pInlineInstr = 0;` |
|         3 |   285 | `			pVm->pInlineFrame = 0;` |
|         3 |   286 | `			rc = PH7_OK;` |
|    613712 |   287 | `		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){` |
|         - |   288 | `			/* Pop the result, then drain any abandoned outer-expression operands` |
|         - |   289 | `			 * to the catching try's base (like the inline branch above) — the` |
|         - |   290 | `			 * ops that would have consumed them were abandoned by the throw, and` |
|         - |   291 | `` 			 * leaving them leaks one slot per caught throw (`try { $a = 1 + f(); }` `` |
|         - |   292 | `			 * in a loop overflowed the operand stack). */` |
|    312056 |   293 | `			VmPopOperand(&pCaller->pTos,1);` |
|   1016876 |   294 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iResumeStackDepth ){` |
|    704825 |   295 | `				PH7_MemObjRelease(pCaller->pTos);` |
|    704825 |   296 | `				pCaller->pTos--;` |
|         5 |   297 | `			}` |
|    312056 |   298 | `			pCaller->pc = iResumePc;` |
|    312056 |   299 | `			rc = PH7_OK;` |
|    156005 |   300 | `		}else{` |
|    301660 |   301 | `			if( pParentFrame->pParent ){` |
|    301650 |   302 | `				rc = PH7_EXCEPTION;` |
|    150826 |   303 | `			}else{` |
|         - |   304 | `				/* Continue normal execution */` |
|        13 |   305 | `				rc = PH7_OK;` |
|         - |   306 | `			}` |
|         - |   307 | `		}` |
|    306827 |   308 | `	}` |
|         - |   309 | `	/* Recycle the operand stack for the next same-size call (BYTECODE stage 7),` |
|         - |   310 | `	 * or free it if the pool is full. Its allocated size is tracked in` |
|         - |   311 | `	 * pCallee->nStackCap (nMaxStack + VM_STACK_GUARD, or larger if an OP_SPREAD grew` |
|         - |   312 | `	 * it) — exactly what the buffer holds. (NULL when the function body was skipped.)` |
|         - |   313 | `	 *` |
|         - |   314 | `	 * Never on rc == PH7_SUSPEND: that path (unreachable in the stage-4 model,` |
|         - |   315 | `	 * where a deep suspend parks its whole record segment before reaching here)` |
|         - |   316 | `	 * would leave the callee stack owned by the suspended ctx, so recycling it` |
|         - |   317 | `	 * would hand a live fiber's operand stack to the next call. The guard keeps` |
|         - |   318 | `	 * that invariant explicit and robust to future coroutine changes. */` |
|    890193 |   319 | `	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){` |
|         - |   320 | `		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee` |
|         - |   321 | `		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly` |
|         - |   322 | `		 * the allocated slot count either way. */` |
|    885363 |   323 | `		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap,pCallee->nLiveTos);` |
|    442871 |   324 | `	}` |
|         - |   325 | `	/* Leave the frame. A throw that left this callee through a try it had OPEN` |
|         - |   326 | `	 * never reached that try's OP_POP_EXCEPTION, so the try's transparent wrapper` |
|         - |   327 | `	 * frames are still stacked ON TOP of the callee's own frame — the shape` |
|         - |   328 | ``	 * `function f(){ try { g(); } catch (NoMatch $e) {} }` leaves behind for every`` |
|         - |   329 | `	 * throw g() raises. Popping once here then tore the WRAPPER down and left the` |
|         - |   330 | `	 * callee's real frame on the chain for good: its locals were never released,` |
|         - |   331 | `	 * every frame above it was attributed to the wrong activation, and a generator` |
|         - |   332 | `	 * body that ended this way failed VmFinishCtxRun's identity test — so its ctx` |
|         - |   333 | `	 * frame was freed while the leftover still pointed at it, and the next frame` |
|         - |   334 | `	 * the pool handed out at that address closed pParent into a CYCLE that hung` |
|         - |   335 | `	 * every later walk of the chain. Drop the wrappers first, exactly as the` |
|         - |   336 | `	 * coroutine suspend/finish paths do (VmFreeSuspendedExceptionFrames): they are` |
|         - |   337 | `	 * transient, and OP_LOAD_EXCEPTION builds a fresh one when the try is next` |
|         - |   338 | `	 * entered. */` |
|         - |   339 | `	{` |
|         - |   340 | `		VmFrame *pW;` |
|         - |   341 | `		/* Only when every frame between the top and the callee's own is such a` |
|         - |   342 | `		 * wrapper: anything else means this callee's frame is already gone and the` |
|         - |   343 | `		 * chain above belongs to somebody else — leave it alone. */` |
|   1202418 |   344 | `		for( pW = pVm->pFrame ; pW && pW != pCallee->pFrame ; pW = pW->pParent ){` |
|    624279 |   345 | `			if( (pW->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|    312054 |   346 | `				break;` |
|         - |   347 | `			}` |
|    156092 |   348 | `		}` |
|    890193 |   349 | `		if( pW == pCallee->pFrame ){` |
|    578306 |   350 | `			while( pVm->pFrame != pCallee->pFrame ){` |
|       166 |   351 | `				VmLeaveFrame(&(*pVm));` |
|         4 |   352 | `			}` |
|    289287 |   353 | `		}` |
|         - |   354 | `	}` |
|    890193 |   355 | `	VmLeaveFrame(&(*pVm));` |
|    890193 |   356 | `	if( rc == PH7_ABORT ){` |
|       629 |   357 | `		return PH7_ABORT;` |
|         - |   358 | `	}` |
|    889569 |   359 | `	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|         - |   360 | `		/* A Fiber::suspend() was called somewhere inside this function.` |
|         - |   361 | `		 * Re-save the fiber's state at THIS level (the fiber's body),` |
|         - |   362 | `		 * overwriting the state saved by the inner level.` |
|         - |   363 | `		 * pTos points to the result slot (not yet written).` |
|         - |   364 | `		 * Save nTos one below so resume pushes at the result slot. */` |
|       ! 0 |   365 | `		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);` |
|       ! 0 |   366 | `		return PH7_SUSPEND;` |
|         - |   367 | `	}` |
|    889569 |   368 | `	if( rc == PH7_EXCEPTION ){` |
|    301650 |   369 | `		return PH7_EXCEPTION;` |
|         - |   370 | `	}` |
|    587924 |   371 | `	return PH7_OK;` |
|    445291 |   372 | `}` |
|         - |   373 | `/*` |
|         - |   374 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|         - |   375 | ` *` |
|         - |   376 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|         - |   377 | ` * close the program with a final OP_DONE and to set up the default` |
|         - |   378 | ` * consumer routines and other stuff. Refer to the implementation` |
|         - |   379 | ` * of [PH7_VmMakeReady()] for additional information.` |
|         - |   380 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|         - |   381 | ` * then the program execution is halted.` |
|         - |   382 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|         - |   383 | ` * should be used respectively to clean up the mess that was left behind` |
|         - |   384 | ` * or to reset the VM to it's initial state.` |
|         - |   385 | ` */` |
|         - |   386 | `static sxi32 VmByteCodeExecBody(ph7_vm *pVm,VmInstr *aInstr,ph7_value *pStack,int nTos,` |
|         - |   387 | `	ph7_value *pResult,sxu32 *pLastRef,int is_callback,sxi32 nPc,` |
|         - |   388 | `	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,VmParkedSegment *pAdoptSegment,` |
|         - |   389 | `	ph7_value **ppBaseOwner,sxu32 *pnBaseCap,sxu32 nStackOrig);` |
|         - |   390 | `/*` |
|         - |   391 | ` * Native-nesting guard around the executor. PHP->PHP calls run iteratively` |
|         - |   392 | ` * (the stage-2 trampoline), but every OTHER (re-)entry — mini-programs,` |
|         - |   393 | ` * C->PHP callbacks, ctx start/resume, eval/include — is still one real C` |
|         - |   394 | ` * activation of VmByteCodeExecBody. nMaxDepth no longer bounds them (it is` |
|         - |   395 | ` * PHP call depth, raisable to memory-bound values since the clamp removal),` |
|         - |   396 | ` * so this counter is what actually protects the C stack: recursive` |
|         - |   397 | ` * eval/include towers, nested coroutine-resume chains and self-recursive` |
|         - |   398 | ` * C-callback compositions hit a clean fatal instead of overflowing. The limit` |
|         - |   399 | ` * lives in pVm->nMaxNativeDepth — a per-platform default (256 host / 16 small-` |
|         - |   400 | ` * stack embedders, VmInit) overridable via PH7_VM_CONFIG_NATIVE_DEPTH. This is` |
|         - |   401 | ` * still a coarse frame-count net rather than php's stack-byte measurement, so` |
|         - |   402 | ` * the host default is conservative — well below the old config clamp's <1024` |
|         - |   403 | ` * ceiling so it holds on the fattest frames (the callback path drags in` |
|         - |   404 | ` * usort/mergesort/trampoline C frames per re-entry, and instrumented builds` |
|         - |   405 | ` * inflate every frame), while far beyond any realistic eval/include/callback` |
|         - |   406 | ` * nesting.` |
|         - |   407 | ` */` |
|   4594413 |   408 | `PH7_PRIVATE sxi32 VmByteCodeExec(` |
|         - |   409 | `	ph7_vm *pVm,         /* Target VM */` |
|         - |   410 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|         - |   411 | `	ph7_value *pStack,   /* Operand stack */` |
|         - |   412 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|         - |   413 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|         - |   414 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|         - |   415 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|         - |   416 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|         - |   417 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|         - |   418 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|         - |   419 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|         - |   420 | `	ph7_value **ppBaseOwner, /* Storage slot the native entry frees for this invocation's BASE (pCallTop==0) operand stack — a local, pVm->aOps or pCtx->pStack. An OP_SPREAD that grows the base stack writes the new pointer here so the entry frees the right buffer. */` |
|         - |   421 | `	sxu32 *pnBaseCap, /* Storage for the base stack's capacity (resumable coroutines persist it across suspend/resume); updated alongside *ppBaseOwner on base-stack growth. Also the initial capacity read at entry. */` |
|         - |   422 | `	sxu32 nStackOrig /* The base stack's ORIGINAL (ungrown) allocation size. Unlike *pnBaseCap (which is the CURRENT, possibly-grown capacity on a coroutine resume), this is fixed, so OP_SPREAD growth headroom stays bounded across resumes. */` |
|         - |   423 | `	)` |
|         5 |   424 | `{` |
|         - |   425 | `	sxi32 rc;` |
|         - |   426 | `	sxi32 nSavedBrc;` |
|         - |   427 | `	sxu32 nSavedLine;` |
|   4594418 |   428 | `	if( VmNativeNestingExceeded(pVm) ){` |
|         5 |   429 | `		return VmNativeNestingFatal(pVm);` |
|         - |   430 | `	}` |
|         - |   431 | `	/* A fresh native exec entered while a C-boundary throw is parked` |
|         - |   432 | `	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the` |
|         - |   433 | `	 * very opcode that swallowed the throw) must run CLEAN: the parked status` |
|         - |   434 | `	 * belongs to the interrupted outer exec's fetch-point router, not to this` |
|         - |   435 | `	 * one. Save+clear on entry, merge back on exit — the outer status is` |
|         - |   436 | `	 * restored unless this exec parked its own unconsumed (newer) one, with` |
|         - |   437 | `	 * PH7_ABORT dominating either way. */` |
|   4594414 |   438 | `	nSavedBrc = pVm->nBoundaryRc;` |
|   4594414 |   439 | `	pVm->nBoundaryRc = 0;` |
|         - |   440 | `	/* The executing source line belongs to the ACTIVATION. A nested body -- a called` |
|         - |   441 | `	 * function, but equally an attribute-default or default-argument mini-program --` |
|         - |   442 | `	 * runs its own bytecode with its own lines, so it must not leave the caller` |
|         - |   443 | ``	 * reporting the callee's position: `new Exception` stamped line 1 because the`` |
|         - |   444 | ``	 * class's `protected $message = '';` default ran (from the embedded chunk) between`` |
|         - |   445 | `	 * OP_NEW and the stamp. Save on entry, restore on exit. */` |
|   4594414 |   446 | `	nSavedLine = pVm->nCurLine;` |
|   4594414 |   447 | `	pVm->nVmExecDepth++;` |
|   6891362 |   448 | `	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,` |
|   2296948 |   449 | `		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);` |
|   4594422 |   450 | `	pVm->nVmExecDepth--;` |
|   4594422 |   451 | `	pVm->nCurLine = nSavedLine;` |
|   4594422 |   452 | `	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT \|\| pVm->nBoundaryRc == 0) ){` |
|        49 |   453 | `		pVm->nBoundaryRc = nSavedBrc;` |
|        22 |   454 | `	}` |
|   4594422 |   455 | `	return rc;` |
|   2296955 |   456 | `}` |
|         - |   457 | `/*` |
|         - |   458 | ` * D1 commit 2: allocate a captured lvalue path for a deferred element/property call arg.` |
|         - |   459 | ` * eRoot picks the root container: 0 = a real aMemObj slot (nRootIdx), 1 = an undefined` |
|         - |   460 | ` * variable to vivify by name at resolve time (pName, a VM-lifetime bytecode string, borrowed),` |
|         - |   461 | ` * 2 = a string base (subscripting a string -> php refuses a by-ref bind). The struct owns its` |
|         - |   462 | ` * step array and each step's key/name; PH7_MemObjRelease frees it via VmFreeDeferredPath.` |
|         - |   463 | ` */` |
|     86670 |   464 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName)` |
|         5 |   465 | `{` |
|     86675 |   466 | `	VmDeferredPath *pPath = (VmDeferredPath *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDeferredPath));` |
|     86675 |   467 | `	if( pPath == 0 ){` |
|       ! 0 |   468 | `		return 0;` |
|         - |   469 | `	}` |
|     86675 |   470 | `	SyZero(pPath,sizeof(VmDeferredPath));` |
|     86675 |   471 | `	pPath->pAlloc = &pVm->sAllocator;` |
|     86675 |   472 | `	pPath->eRoot = eRoot;` |
|     86675 |   473 | `	pPath->nRootIdx = nRootIdx;` |
|     86675 |   474 | `	if( eRoot == 1 && pName ){` |
|        16 |   475 | `		pPath->sRootName = *pName; /* borrowed VM-lifetime bytes, not copied */` |
|         6 |   476 | `	}` |
|     86675 |   477 | `	return pPath;` |
|     44004 |   478 | `}` |
|         - |   479 | `/*` |
|         - |   480 | ` * A carrier for a fetch that ALREADY HAPPENED: an overloaded container answered with a` |
|         - |   481 | ` * value, and only the by-ref verdict is still pending (VM_DEFER_ROOT_PREFETCH). Takes a` |
|         - |   482 | ` * copy of the value; the caller keeps its own.` |
|         - |   483 | ` */` |
|      3624 |   484 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNewPrefetch(ph7_vm *pVm,int nKind,ph7_class *pClass,` |
|         - |   485 | `	const SyString *pName,ph7_value *pVal)` |
|         5 |   486 | `{` |
|      3629 |   487 | `	VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),VM_DEFER_ROOT_PREFETCH,SXU32_HIGH,0);` |
|      3629 |   488 | `	if( pPath == 0 ){` |
|       ! 0 |   489 | `		return 0;` |
|         - |   490 | `	}` |
|      3629 |   491 | `	pPath->nOverKind = (sxu8)nKind;` |
|      3629 |   492 | `	pPath->pOverClass = pClass;` |
|      3629 |   493 | `	if( pName && pName->nByte > 0 ){` |
|       107 |   494 | `		pPath->zOverName = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|       107 |   495 | `		if( pPath->zOverName == 0 ){` |
|       ! 0 |   496 | `			VmFreeDeferredPath(pPath);` |
|       ! 0 |   497 | `			return 0;` |
|         - |   498 | `		}` |
|       107 |   499 | `		SyStringInitFromBuf(&pPath->sOverName,pPath->zOverName,pName->nByte);` |
|        51 |   500 | `	}` |
|      3629 |   501 | `	PH7_MemObjInit(&(*pVm),&pPath->sPrefetch);` |
|      3629 |   502 | `	PH7_MemObjStore(pVal,&pPath->sPrefetch);` |
|      3629 |   503 | `	return pPath;` |
|      1817 |   504 | `}` |
|         - |   505 | `/*` |
|         - |   506 | ` * The verdict a prefetched value gets when the callee turns out to want it BY REFERENCE:` |
|         - |   507 | ` * php asked the object for something to modify and it could only answer with a value.` |
|         - |   508 | ` * Two of the three are notices php carries on from; a HOOKED property is the one php` |
|         - |   509 | ` * refuses outright. All three stay silent for a by-VALUE parameter, which is the whole` |
|         - |   510 | ` * reason the fetch could not decide them itself.` |
|         - |   511 | ` */` |
|        46 |   512 | `static sxi32 VmPrefetchByRefVerdict(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pVal)` |
|         1 |   513 | `{` |
|        47 |   514 | `	if( pPath->nOverKind == VM_OVER_PROP ){` |
|        17 |   515 | `		PH7_VmOverloadedPropNotice(&(*pVm),pPath->pOverClass,&pPath->sOverName,pVal);` |
|        17 |   516 | `		return SXRET_OK;` |
|         - |   517 | `	}` |
|        31 |   518 | `	if( pPath->nOverKind == VM_OVER_HOOK ){` |
|         - |   519 | `		SyBlob sErrMsg;` |
|         - |   520 | `		sxi32 rcH;` |
|         9 |   521 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         9 |   522 | `		SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|         8 |   523 | `			&pPath->pOverClass->sDisp,&pPath->sOverName);` |
|         9 |   524 | `		rcH = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg);` |
|         9 |   525 | `		SyBlobRelease(&sErrMsg);` |
|         9 |   526 | `		return (rcH == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - |   527 | `	}` |
|        23 |   528 | `	PH7_VmOverloadedElemNotice(&(*pVm),pPath->pOverClass,pVal);` |
|        23 |   529 | `	return SXRET_OK;` |
|        24 |   530 | `}` |
|     86474 |   531 | `static VmDeferStep * VmDeferPathGrow(VmDeferredPath *pPath)` |
|         5 |   532 | `{` |
|     86479 |   533 | `	if( pPath->nStep >= pPath->nAlloc ){` |
|     86463 |   534 | `		sxu32 nNew = pPath->nAlloc ? pPath->nAlloc * 2 : 4;` |
|    130356 |   535 | `		VmDeferStep *aNew = (VmDeferStep *)SyMemBackendRealloc(pPath->pAlloc,pPath->aStep,` |
|     43893 |   536 | `			nNew * sizeof(VmDeferStep));` |
|     86463 |   537 | `		if( aNew == 0 ){` |
|       ! 0 |   538 | `			return 0;` |
|         - |   539 | `		}` |
|     86463 |   540 | `		pPath->aStep = aNew;` |
|     86463 |   541 | `		pPath->nAlloc = nNew;` |
|     43893 |   542 | `	}` |
|     86479 |   543 | `	return &pPath->aStep[pPath->nStep];` |
|     43906 |   544 | `}` |
|         - |   545 | `/* Append an array-element step, deep-copying the index value (the caller releases pKey). */` |
|     82914 |   546 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey)` |
|         5 |   547 | `{` |
|     82919 |   548 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|     82919 |   549 | `	if( pStep == 0 ){` |
|       ! 0 |   550 | `		return SXERR_MEM;` |
|         - |   551 | `	}` |
|     82919 |   552 | `	pStep->isProp = 0;` |
|     82919 |   553 | `	pStep->bAppend = 0;` |
|     82919 |   554 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|     82919 |   555 | `	PH7_MemObjInit(pKey->pVm,&pStep->sKey);` |
|     82919 |   556 | `	PH7_MemObjStore(pKey,&pStep->sKey);` |
|     82919 |   557 | `	pPath->nStep++;` |
|     82919 |   558 | `	return SXRET_OK;` |
|     42126 |   559 | `}` |
|         - |   560 | ``/* Append a KEYLESS element step — the `[]` of `f($a[])`. It carries no index at all,`` |
|         - |   561 | ` * so the two resolvers differ on it: by reference it creates the next element (php` |
|         - |   562 | `` * binds the parameter to it), by value it is php's `Cannot use [] for reading`. */`` |
|        12 |   563 | `PH7_PRIVATE sxi32 VmDeferPathPushAppend(VmDeferredPath *pPath)` |
|         1 |   564 | `{` |
|        13 |   565 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|        13 |   566 | `	if( pStep == 0 ){` |
|       ! 0 |   567 | `		return SXERR_MEM;` |
|         - |   568 | `	}` |
|        13 |   569 | `	pStep->isProp = 0;` |
|        13 |   570 | `	pStep->bAppend = 1;` |
|        13 |   571 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|        13 |   572 | `	SyZero((void *)&pStep->sKey,sizeof(ph7_value));` |
|        13 |   573 | `	pPath->nStep++;` |
|        13 |   574 | `	return SXRET_OK;` |
|         7 |   575 | `}` |
|         - |   576 | `/* Append an object-property step, owning a private copy of the name bytes. */` |
|      3548 |   577 | `PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName)` |
|         5 |   578 | `{` |
|      3553 |   579 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|         - |   580 | `	char *zCopy;` |
|      3553 |   581 | `	if( pStep == 0 ){` |
|       ! 0 |   582 | `		return SXERR_MEM;` |
|         - |   583 | `	}` |
|      3553 |   584 | `	zCopy = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|      3553 |   585 | `	if( zCopy == 0 ){` |
|       ! 0 |   586 | `		return SXERR_MEM;` |
|         - |   587 | `	}` |
|      3553 |   588 | `	pStep->isProp = 1;` |
|      3553 |   589 | `	pStep->bAppend = 0;` |
|      3553 |   590 | `	pStep->zProp = zCopy;` |
|      3553 |   591 | `	SyStringInitFromBuf(&pStep->sProp,zCopy,pName->nByte);` |
|      3553 |   592 | `	pPath->nStep++;` |
|      3553 |   593 | `	return SXRET_OK;` |
|      1779 |   594 | `}` |
|         - |   595 | `/* Release a captured lvalue path and everything it owns (element keys, property names). */` |
|         - |   596 | `/*` |
|         - |   597 | `` * The pending offset of a `$s[k] ??= v`: a heap copy of the RAW key, owned by the`` |
|         - |   598 | ` * peek's MEMOBJ_AUX_COALSTROFF result on the operand stack. One carrier per` |
|         - |   599 | `` * pending ??=, so `$s[9] ??= ($t[9] ??= "q")` nests — a single VM-wide slot could`` |
|         - |   600 | ` * not (the inner peek overwrote the outer's offset, and the outer store then` |
|         - |   601 | ` * replaced the whole string).` |
|         - |   602 | ` */` |
|        54 |   603 | `PH7_PRIVATE VmCoalStrOff * VmCoalStrOffNew(ph7_vm *pVm,ph7_value *pKey)` |
|         1 |   604 | `{` |
|        55 |   605 | `	VmCoalStrOff *pCoal = (VmCoalStrOff *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmCoalStrOff));` |
|        55 |   606 | `	if( pCoal == 0 ){` |
|       ! 0 |   607 | `		return 0;` |
|         - |   608 | `	}` |
|        55 |   609 | `	pCoal->pAlloc = &pVm->sAllocator;` |
|        55 |   610 | `	PH7_MemObjInit(&(*pVm),&pCoal->sKey);` |
|        55 |   611 | `	if( pKey ){` |
|        55 |   612 | `		PH7_MemObjStore(pKey,&pCoal->sKey);` |
|        27 |   613 | `	}` |
|        55 |   614 | `	return pCoal;` |
|        28 |   615 | `}` |
|        90 |   616 | `PH7_PRIVATE void VmFreeCoalStrOff(VmCoalStrOff *pCoal)` |
|         3 |   617 | `{` |
|         - |   618 | `	SyMemBackend *pAlloc;` |
|        93 |   619 | `	if( pCoal == 0 ){` |
|        39 |   620 | `		return;` |
|         - |   621 | `	}` |
|        55 |   622 | `	pAlloc = pCoal->pAlloc;` |
|        55 |   623 | `	PH7_MemObjRelease(&pCoal->sKey);` |
|        55 |   624 | `	SyMemBackendFree(pAlloc,pCoal);` |
|        48 |   625 | `}` |
|         - |   626 | `/*` |
|         - |   627 | ` * Build the pending __call/__callStatic routing OP_MEMBER hands to the OP_CALL that` |
|         - |   628 | ` * follows it, and hang it off the marked carrier slot. One record per routed call, so a` |
|         - |   629 | ` * routed call evaluated inside another routed call's ARGUMENT LIST — which is where they` |
|         - |   630 | ` * now sit, php's order — keeps its own {receiver, class, name}. Takes the receiver` |
|         - |   631 | ` * reference; the record owns it from here.` |
|         - |   632 | ` */` |
|       162 |   633 | `PH7_PRIVATE VmMagicCall * VmMagicCallNew(ph7_vm *pVm,ph7_class_instance *pRecv,` |
|         - |   634 | `	ph7_class *pClass,const SyString *pName)` |
|         4 |   635 | `{` |
|       166 |   636 | `	VmMagicCall *pPend = (VmMagicCall *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmMagicCall));` |
|       166 |   637 | `	if( pPend == 0 ){` |
|       ! 0 |   638 | `		return 0;` |
|         - |   639 | `	}` |
|       166 |   640 | `	pPend->pAlloc = &pVm->sAllocator;` |
|       166 |   641 | `	pPend->pRecv = pRecv;` |
|       166 |   642 | `	pPend->pClass = pClass;` |
|       166 |   643 | `	pPend->pLsb = 0;` |
|       166 |   644 | `	SyBlobInit(&pPend->sName,&pVm->sAllocator);` |
|       166 |   645 | `	if( pName && pName->nByte > 0 ){` |
|       166 |   646 | `		SyBlobAppend(&pPend->sName,(const void *)pName->zString,pName->nByte);` |
|        81 |   647 | `	}` |
|       166 |   648 | `	if( pRecv ){` |
|        90 |   649 | `		pRecv->iRef++;` |
|        43 |   650 | `	}` |
|       166 |   651 | `	return pPend;` |
|        85 |   652 | `}` |
|       162 |   653 | `PH7_PRIVATE void VmFreeMagicCall(VmMagicCall *pPend)` |
|         4 |   654 | `{` |
|         - |   655 | `	SyMemBackend *pAlloc;` |
|       166 |   656 | `	if( pPend == 0 ){` |
|       ! 0 |   657 | `		return;` |
|         - |   658 | `	}` |
|       166 |   659 | `	pAlloc = pPend->pAlloc;` |
|       166 |   660 | `	if( pPend->pRecv ){` |
|       ! 0 |   661 | `		PH7_ClassInstanceUnref(pPend->pRecv);` |
|       ! 0 |   662 | `	}` |
|       166 |   663 | `	SyBlobRelease(&pPend->sName);` |
|       166 |   664 | `	SyMemBackendFree(pAlloc,pPend);` |
|        85 |   665 | `}` |
|     86670 |   666 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath)` |
|         5 |   667 | `{` |
|         - |   668 | `	sxu32 i;` |
|     86675 |   669 | `	if( pPath == 0 ){` |
|       ! 0 |   670 | `		return;` |
|         - |   671 | `	}` |
|    173149 |   672 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|     86479 |   673 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|     86479 |   674 | `		if( pStep->isProp ){` |
|      3553 |   675 | `			if( pStep->zProp ){` |
|      3553 |   676 | `				SyMemBackendFree(pPath->pAlloc,pStep->zProp);` |
|      1779 |   677 | `			}` |
|     84705 |   678 | `		}else if( !pStep->bAppend ){` |
|         - |   679 | `			/* An append step holds no key at all — its sKey was never initialized. */` |
|     82919 |   680 | `			PH7_MemObjRelease(&pStep->sKey);` |
|     42121 |   681 | `		}` |
|     43906 |   682 | `	}` |
|     86675 |   683 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|      3629 |   684 | `		PH7_MemObjRelease(&pPath->sPrefetch);` |
|      3629 |   685 | `		if( pPath->zOverName ){` |
|       107 |   686 | `			SyMemBackendFree(pPath->pAlloc,pPath->zOverName);` |
|        51 |   687 | `		}` |
|      1812 |   688 | `	}` |
|     86675 |   689 | `	if( pPath->aStep ){` |
|     86463 |   690 | `		SyMemBackendFree(pPath->pAlloc,pPath->aStep);` |
|     43893 |   691 | `	}` |
|     86675 |   692 | `	SyMemBackendFree(pPath->pAlloc,pPath);` |
|     44004 |   693 | `}` |
|         - |   694 | `/* Map an op-handler VmOpRc into the main-loop rc convention used by VmByteCodeExecBody. */` |
|     86368 |   695 | `static sxi32 VmOpRcToExecRc(VmOpRc rcOp)` |
|         5 |   696 | `{` |
|     86373 |   697 | `	if( rcOp == VM_OP_ABORT ){` |
|         3 |   698 | `		return PH7_ABORT;` |
|         - |   699 | `	}` |
|     86371 |   700 | `	if( rcOp == VM_OP_EXCEPTION ){` |
|        23 |   701 | `		return PH7_EXCEPTION;` |
|         - |   702 | `	}` |
|     86351 |   703 | `	return SXRET_OK;` |
|     43853 |   704 | `}` |
|         - |   705 | `/*` |
|         - |   706 | ` * D1 commit 2: re-drive ONE captured lvalue step by invoking the real LOAD_IDX / MEMBER` |
|         - |   707 | ` * handler on a synthetic 2-slot stack. This reuses the proven COW / vivify / warning / magic` |
|         - |   708 | ` * machinery instead of hand-walking it. pBase carries the current container (its nIdx must be` |
|         - |   709 | ` * a real aMemObj slot for a by-ref write to vivify in place). iP2 selects the mode:` |
|         - |   710 | ` * LOAD_IDX 1=write(vivify,by-ref) / 0=read(by-value); MEMBER PH7_MEMBER_READ=by-value read.` |
|         - |   711 | ` * On success pOut receives the result value and its nIdx (the aliasable element slot for a` |
|         - |   712 | ` * vivified by-ref element).` |
|         - |   713 | ` */` |
|     86368 |   714 | `static sxi32 VmReDriveStep(ph7_vm *pVm,sxi32 iOp,sxu32 iP2,ph7_value *pBase,ph7_value *pKey,ph7_value *pOut)` |
|         5 |   715 | `{` |
|         - |   716 | `	ph7_value mini[2];` |
|         - |   717 | `	VmInstr aI[2];` |
|         - |   718 | `	VmExecState st;` |
|         - |   719 | `	VmOpRc rcOp;` |
|         - |   720 | ``	/* A NULL key is the APPEND form (`$a[]`): LOAD_IDX takes no index operand, so the`` |
|         - |   721 | `	 * base is the whole stack and iP1 says so. */` |
|     86373 |   722 | `	int bAppend = (pKey == 0);` |
|     86373 |   723 | `	PH7_MemObjInit(pVm,&mini[0]);` |
|     86373 |   724 | `	PH7_MemObjInit(pVm,&mini[1]);` |
|     86373 |   725 | `	PH7_MemObjLoad(pBase,&mini[0]);` |
|     86373 |   726 | `	mini[0].nIdx = pBase->nIdx;` |
|     86373 |   727 | `	if( !bAppend ){` |
|     86363 |   728 | `		PH7_MemObjStore(pKey,&mini[1]);` |
|     43843 |   729 | `	}` |
|     86373 |   730 | `	SyZero((void *)aI,sizeof(aI));` |
|         - |   731 | `	/* LOAD_IDX: iP1=1 means "an index is present". MEMBER: iP1=0 means an INSTANCE member` |
|         - |   732 | ``	 * (iP1=1 would be a static `::` access). */`` |
|     86373 |   733 | `	aI[0].iOp = (sxu8)iOp; aI[0].iP1 = (iOp == PH7_OP_LOAD_IDX && !bAppend) ? 1 : 0; aI[0].iP2 = iP2;` |
|     86373 |   734 | `	SyZero((void *)&st,sizeof(st));` |
|     86373 |   735 | `	st.pStack = mini; st.pTos = bAppend ? &mini[0] : &mini[1]; st.aInstr = aI; st.pc = 0;` |
|     86373 |   736 | `	if( iOp == PH7_OP_LOAD_IDX ){` |
|     82907 |   737 | `		rcOp = VmExecOpLoadIdx(&(*pVm),&st,&aI[0]);` |
|     42120 |   738 | `	}else{` |
|      3471 |   739 | `		rcOp = VmExecOpMember(&(*pVm),&st,&aI[0]);` |
|         - |   740 | `	}` |
|         - |   741 | `	/* The handler popped the key/name and left the result in the (now top) base slot.` |
|         - |   742 | `	 * DEEP-COPY it into pOut (MemObjStore, not MemObjLoad): the result is chained as the next` |
|         - |   743 | `	 * step's base and must OWN its buffer — mini[0] is released immediately below, and a` |
|         - |   744 | `	 * read-only view (MemObjLoad) would leave pOut dangling into freed memory. */` |
|     86373 |   745 | `	PH7_MemObjStore(st.pTos,pOut);` |
|     86373 |   746 | `	pOut->nIdx = st.pTos->nIdx;` |
|     86373 |   747 | `	PH7_MemObjRelease(&mini[0]);` |
|     86373 |   748 | `	return VmOpRcToExecRc(rcOp);` |
|         5 |   749 | `}` |
|         - |   750 | `/*` |
|         - |   751 | ` * D1 commit 2: resolve an object property as a by-ref target. Given the object's aMemObj` |
|         - |   752 | ` * slot, return the property value's slot index in *pnOut so the by-ref binder can alias it.` |
|         - |   753 | ` * A present property binds directly; a missing one is created (recreate a declared+unset` |
|         - |   754 | ` * property, or a dynamic property on a dynamic-allowing class); a magic __get/__set property` |
|         - |   755 | ` * emits php's Notice and does NOT bind (*pbNoBind), handing __get's value back through` |
|         - |   756 | ` * pValOut (optional) for the by-VALUE pass php makes instead. Mirrors VmExecOpMember's` |
|         - |   757 | ` * write-create.` |
|         - |   758 | ` */` |
|        78 |   759 | `static sxi32 VmBindPropByRef(ph7_vm *pVm,ph7_value *pObj,const SyString *pName,sxu32 *pnOut,int *pbNoBind,ph7_value *pValOut)` |
|         4 |   760 | `{` |
|         - |   761 | `	ph7_class_instance *pThis;` |
|         - |   762 | `	ph7_class *pClass;` |
|         - |   763 | `	SyHashEntry *pEntry;` |
|        82 |   764 | `	VmClassAttr *pAttr = 0;` |
|        82 |   765 | `	*pbNoBind = 0;` |
|        82 |   766 | `	if( pObj == 0 ){` |
|         - |   767 | `		/* The root slot is gone (a detached frame): nothing to bind and nothing to say. */` |
|       ! 0 |   768 | `		*pbNoBind = 1;` |
|       ! 0 |   769 | `		return SXRET_OK;` |
|         - |   770 | `	}` |
|        82 |   771 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - |   772 | `		/* A non-object base has no property to alias, and php does not pass NULL and carry` |
|         - |   773 | `		 * on: asking one for something to MODIFY is its catchable Error, the same one every` |
|         - |   774 | `		 * other write shape through a null/int/string base raises. PHL warned about a READ` |
|         - |   775 | ``		 * it never performed and handed the by-ref parameter a NULL, so `f($u->p)` with`` |
|         - |   776 | ``		 * `function f(&$x)` wrote into nothing on a statement php stops the script for. The`` |
|         - |   777 | `		 * by-VALUE binding still takes the read warning — that half is php-exact — and is` |
|         - |   778 | `		 * what the value re-drive next door produces. */` |
|         - |   779 | `		SyBlob sErrM;` |
|         - |   780 | `		sxi32 rcErr;` |
|        21 |   781 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|        21 |   782 | `		SyBlobFormat(&sErrM,"Attempt to modify property \"%z\" on %s",` |
|        10 |   783 | `			pName,VmArithValueName(pObj));` |
|        31 |   784 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|        10 |   785 | `			SyBlobLength(&sErrM));` |
|        21 |   786 | `		SyBlobRelease(&sErrM);` |
|        21 |   787 | `		*pbNoBind = 1;` |
|        21 |   788 | `		return (rcErr == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - |   789 | `	}` |
|        62 |   790 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|        62 |   791 | `	pClass = pThis->pClass;` |
|        62 |   792 | `	if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|         - |   793 | `		/* A by-reference argument asks the incomplete object for something to` |
|         - |   794 | `		 * MODIFY: php's catchable Error, the same one every direct write raises. */` |
|         - |   795 | `		SyBlob sIncErr;` |
|         - |   796 | `		sxi32 rcInc;` |
|         3 |   797 | `		SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|         3 |   798 | `		PH7_VmIncompleteMsg(&(*pVm),pThis,"modify a property",&sIncErr);` |
|         4 |   799 | `		rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|         1 |   800 | `			SyBlobLength(&sIncErr));` |
|         3 |   801 | `		SyBlobRelease(&sIncErr);` |
|         3 |   802 | `		*pbNoBind = 1;` |
|         3 |   803 | `		return (rcInc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - |   804 | `	}` |
|        60 |   805 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|        60 |   806 | `	if( pEntry ){` |
|        16 |   807 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        16 |   808 | `		if( (pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        13 |   809 | `			if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|         - |   810 | `				/* A by-reference argument asks for something to MODIFY, and this` |
|         - |   811 | `				 * class's handler refuses every write: php's catchable Error, the` |
|         - |   812 | `				 * same sentence a plain store gets. */` |
|       ! 0 |   813 | `				*pbNoBind = 1;` |
|       ! 0 |   814 | `				return VmThrowNativeNoWrite(&(*pVm),PH7_VmAttrOwner(pAttr),pAttr->pAttr);` |
|         - |   815 | `			}` |
|        13 |   816 | `			if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|         - |   817 | `				/* A native class's property is a field of php's own C struct, not` |
|         - |   818 | `				 * storage a script may alias: php has no ptr_ptr handler for one, so` |
|         - |   819 | ``				 * `g($i->s)` with `function g(&$x)` passes the VALUE and the callee's`` |
|         - |   820 | `				 * write is lost — in SILENCE, unlike the overloaded case below, which` |
|         - |   821 | ``				 * php has a notice for. `preg_match($p,$s,$i->s)` is the same answer. */`` |
|       ! 0 |   822 | `				ph7_value *pCur = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|       ! 0 |   823 | `				*pbNoBind = 1;` |
|       ! 0 |   824 | `				if( pValOut && pCur ){` |
|       ! 0 |   825 | `					PH7_MemObjStore(pCur,pValOut);` |
|       ! 0 |   826 | `				}` |
|       ! 0 |   827 | `				return SXRET_OK;` |
|         - |   828 | `			}` |
|        12 |   829 | `			if( (pAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|        13 |   830 | `			 && (pAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|         - |   831 | `				/* php's rule for a REFERENCE fetch of an uninitialized typed property:` |
|         - |   832 | `				 * seed NULL and bind when the declared type admits one, refuse with` |
|         - |   833 | `` 				 * `Cannot access uninitialized non-nullable property ... by reference` `` |
|         - |   834 | `				 * when it does not. A by-VALUE argument takes the ordinary read Error,` |
|         - |   835 | `				 * which the value re-drive next door produces. */` |
|        19 |   836 | `				sxi32 rcRs = VmRefUninitTypedProperty(&(*pVm),pClass,pAttr,` |
|        12 |   837 | `					(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx));` |
|        13 |   838 | `				if( rcRs != SXRET_OK ){` |
|        13 |   839 | `					*pbNoBind = 1;` |
|        13 |   840 | `					return (rcRs == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - |   841 | `				}` |
|       ! 0 |   842 | `			}` |
|       ! 0 |   843 | `			*pnOut = pAttr->nIdx;` |
|       ! 0 |   844 | `			return SXRET_OK;` |
|         - |   845 | `		}` |
|         - |   846 | `		/* A static property is the CLASS's: php does not find it through an` |
|         - |   847 | ``		 * instance, so binding `f($o->s)` by reference must not hand out the`` |
|         - |   848 | `		 * class slot — that let a by-ref callee overwrite shared class state` |
|         - |   849 | `		 * through an object, and with no diagnostic at all (the value pass that` |
|         - |   850 | `		 * carries the notice at the fetch site never runs for a by-ref arg).` |
|         - |   851 | `		 * Notice here and fall through to the missing-property handling. */` |
|         4 |   852 | `		if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->pAttr->sName,` |
|         2 |   853 | `			pAttr->pAttr->iProtection,FALSE) ){` |
|         4 |   854 | `			VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|         - |   855 | `				"Accessing static property %z::$%z as non static",` |
|         1 |   856 | `				&pClass->sDisp,pName);` |
|         1 |   857 | `		}` |
|         3 |   858 | `		pAttr = 0;` |
|         1 |   859 | `	}` |
|        48 |   860 | `	if( PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|         - |   861 | `		/* Overloaded (magic) property: php passes it by-value with a Notice and drops the` |
|         - |   862 | `		 * write-back — "has no effect". The value it passes is __get's, which the caller` |
|         - |   863 | `		 * takes through pValOut; leaving the argument NULL instead turned` |
|         - |   864 | ``		 * `sort($o->magic)` — a statement php performs on a temporary — into`` |
|         - |   865 | ``		 * `sort(): Argument #1 ($array) must be of type array, null given`.`` |
|         - |   866 | `		 *` |
|         - |   867 | ``		 * `__get` ALONE decides it, which is php's own test`` |
|         - |   868 | ``		 * (zend_std_get_property_ptr_ptr consults `ce->__get` and nothing else): a`` |
|         - |   869 | ``		 * class carrying only `__set` has no way to ANSWER the fetch, so php falls`` |
|         - |   870 | `		 * through and creates an ordinary dynamic property instead — which is the scope policy's` |
|         - |   871 | `		 * refusal here, not this notice.` |
|         - |   872 | `		 *` |
|         - |   873 | `		 * The notice is the OVERLOAD's, though, not the fetch's: a native class's` |
|         - |   874 | `		 * VIRTUAL property is a declared name php answers from a handler, and php's` |
|         - |   875 | `` 		 * own get_property_ptr_ptr declines it in SILENCE — `$r = &$doc->formatOutput` `` |
|         - |   876 | `		 * binds a temporary there and says nothing. */` |
|       ! 0 |   877 | `		ph7_class_attr *pVirt = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);` |
|       ! 0 |   878 | `		if( pVirt == 0 \|\| (pVirt->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) == 0 ){` |
|       ! 0 |   879 | `			VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|         - |   880 | `				"Indirect modification of overloaded property %z::$%z has no effect",` |
|       ! 0 |   881 | `				&pClass->sDisp,pName);` |
|       ! 0 |   882 | `		}` |
|       ! 0 |   883 | `		*pbNoBind = 1;` |
|       ! 0 |   884 | `		if( pValOut && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|       ! 0 |   885 | `		 && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g') ){` |
|       ! 0 |   886 | `			VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|       ! 0 |   887 | `			PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pValOut);` |
|       ! 0 |   888 | `			VmMagicGuardPop(pVm);` |
|       ! 0 |   889 | `		}` |
|       ! 0 |   890 | `		return SXRET_OK;` |
|         - |   891 | `	}` |
|         - |   892 | `	{` |
|        48 |   893 | `		ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);` |
|        48 |   894 | `		if( pDecl && PH7_ATTR_LAZY_ABSENT(pDecl,pThis) ){` |
|       ! 0 |   895 | `			if( pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|         - |   896 | `				/* Refused with or without a struct behind it. */` |
|       ! 0 |   897 | `				*pbNoBind = 1;` |
|       ! 0 |   898 | `				return VmThrowNativeNoWrite(&(*pVm),pClass,pDecl);` |
|         - |   899 | `			}` |
|       ! 0 |   900 | `			pDecl = 0;   /* never held: php creates a dynamic property, PHL refuses (the scope policy) */` |
|       ! 0 |   901 | `		}` |
|        48 |   902 | `		if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       ! 0 |   903 | `			VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pAttr);` |
|        48 |   904 | `		}else if( VmClassAllowsDynamicProps(&(*pVm),pClass) ){` |
|        39 |   905 | `			PH7_VmCreateDynamicAttr(&(*pVm),pThis,pName->zString,pName->nByte,&pAttr);` |
|        21 |   906 | `		}else{` |
|         - |   907 | `			SyBlob sMsg;` |
|         - |   908 | `			sxi32 rcT;` |
|        10 |   909 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        10 |   910 | `			SyBlobFormat(&sMsg,"Cannot create dynamic property %z::$%z",&pClass->sDisp,pName);` |
|        10 |   911 | `			rcT = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        10 |   912 | `			SyBlobRelease(&sMsg);` |
|        10 |   913 | `			*pbNoBind = 1;` |
|        10 |   914 | `			return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - |   915 | `		}` |
|        39 |   916 | `		if( pAttr ){` |
|        39 |   917 | `			*pnOut = pAttr->nIdx;` |
|        21 |   918 | `		}else{` |
|       ! 0 |   919 | `			*pbNoBind = 1;` |
|         - |   920 | `		}` |
|         - |   921 | `	}` |
|        39 |   922 | `	return SXRET_OK;` |
|        43 |   923 | `}` |
|         - |   924 | `/*` |
|         - |   925 | ` * Walk a captured path's steps over SLOTS: each step vivifies in place and answers the` |
|         - |   926 | ` * next one, so the terminal slot is what the by-ref binder aliases. Split out of the` |
|         - |   927 | ` * resolver below because the VALUE walk hands control back to it — an accessor that` |
|         - |   928 | ` * answered with an OBJECT is a handle, not a temporary, and everything under it is` |
|         - |   929 | ` * addressable again.` |
|         - |   930 | ` */` |
|       148 |   931 | `static sxi32 VmWalkStepsFromSlot(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,sxu32 nCur,` |
|         - |   932 | `	ph7_value *pSlot)` |
|         5 |   933 | `{` |
|         - |   934 | `	sxu32 i;` |
|         - |   935 | `	sxi32 rc;` |
|       261 |   936 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|       157 |   937 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|       157 |   938 | `		if( pStep->isProp ){` |
|        80 |   939 | `			sxu32 nOut = SXU32_HIGH;` |
|        80 |   940 | `			int bNoBind = 0;` |
|         - |   941 | `			ph7_value sMagicVal;` |
|        80 |   942 | `			int bLastStep = (i + 1 == pPath->nStep);` |
|        80 |   943 | `			PH7_MemObjInit(&(*pVm),&sMagicVal);` |
|       156 |   944 | `			rc = VmBindPropByRef(&(*pVm),(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nCur),` |
|        76 |   945 | `				&pStep->sProp,&nOut,&bNoBind,bLastStep ? &sMagicVal : 0);` |
|        80 |   946 | `			if( rc != SXRET_OK ){` |
|        45 |   947 | `				PH7_MemObjRelease(&sMagicVal);` |
|        45 |   948 | `				return rc;` |
|         - |   949 | `			}` |
|        37 |   950 | `			if( bNoBind ){` |
|         - |   951 | `				/* magic/non-object: nothing to alias, so the argument is passed BY` |
|         - |   952 | `				 * VALUE — which for an overloaded property is what __get answered,` |
|         - |   953 | `				 * not the NULL this used to leave behind. */` |
|       ! 0 |   954 | `				if( bLastStep ){` |
|       ! 0 |   955 | `					PH7_MemObjStore(&sMagicVal,pSlot);` |
|       ! 0 |   956 | `					pSlot->nIdx = SXU32_HIGH;` |
|       ! 0 |   957 | `				}` |
|       ! 0 |   958 | `				PH7_MemObjRelease(&sMagicVal);` |
|       ! 0 |   959 | `				return SXRET_OK;` |
|         - |   960 | `			}` |
|        37 |   961 | `			PH7_MemObjRelease(&sMagicVal);` |
|        37 |   962 | `			nCur = nOut;` |
|        20 |   963 | `		}else{` |
|         - |   964 | `			ph7_value out;` |
|        79 |   965 | `			ph7_value *pContainer = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nCur);` |
|        79 |   966 | `			if( pContainer == 0 ){` |
|       ! 0 |   967 | `				return SXRET_OK;` |
|         - |   968 | `			}` |
|        79 |   969 | `			PH7_MemObjInit(&(*pVm),&out);` |
|         - |   970 | ``			/* `f($a[])` bound to a by-reference parameter: php CREATES the next element`` |
|         - |   971 | `			 * and aliases the parameter to it. */` |
|       117 |   972 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,1,pContainer,` |
|        76 |   973 | `				pStep->bAppend ? 0 : &pStep->sKey,&out);` |
|        79 |   974 | `			nCur = out.nIdx;` |
|        79 |   975 | `			PH7_MemObjRelease(&out);` |
|        79 |   976 | `			if( rc != SXRET_OK ){` |
|         3 |   977 | `				return rc;` |
|         - |   978 | `			}` |
|        77 |   979 | `			if( nCur == SXU32_HIGH ){` |
|       ! 0 |   980 | `				return SXRET_OK; /* no aliasable slot (e.g. a non-lvalue container): pass by value */` |
|         - |   981 | `			}` |
|         - |   982 | `		}` |
|        58 |   983 | `	}` |
|         - |   984 | `	{` |
|         - |   985 | `		/* The terminal slot is what the by-ref binder aliases, but the argument also has` |
|         - |   986 | `		 * to CARRY the element's value: a builtin reads what it is handed and writes back` |
|         - |   987 | `		 * through the slot. That was invisible while a path was only ever captured on a` |
|         - |   988 | `		 * MISS — the vivified element is NULL and so was the carrier — and stopped being` |
|         - |   989 | `		 * true when a WRITABLE container's existing element started riding one` |
|         - |   990 | ``		 * (`sort($ao['a'])` reached sort() as NULL). */`` |
|       108 |   991 | `		ph7_value *pFinal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nCur);` |
|       108 |   992 | `		if( pFinal ){` |
|       108 |   993 | `			PH7_MemObjLoad(pFinal,pSlot);` |
|        52 |   994 | `		}` |
|         - |   995 | `	}` |
|       108 |   996 | `	pSlot->nIdx = nCur;` |
|       108 |   997 | `	return SXRET_OK;` |
|        79 |   998 | `}` |
|         - |   999 | `/*` |
|         - |  1000 | ` * Walk a captured path's remaining steps over a VALUE rather than a slot — the` |
|         - |  1001 | ` * continuation both resolvers need once the chain has left addressable storage: an` |
|         - |  1002 | ` * overloaded container's answer is a temporary, and everything subscripted off it is a` |
|         - |  1003 | ` * temporary too. bWrite picks php's fetch mode for those steps: a by-REFERENCE argument` |
|         - |  1004 | ` * makes them W fetches, which vivify inside the temporary in SILENCE (php's` |
|         - |  1005 | `` * `f($o['a']['zz'])` says only its notice), while a by-VALUE one reads and warns about a`` |
|         - |  1006 | ` * key that is not there.` |
|         - |  1007 | ` */` |
|     86496 |  1008 | `static sxi32 VmWalkStepsOverValue(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,` |
|         - |  1009 | `	ph7_value *pCur,int bWrite,ph7_value *pSlot)` |
|         5 |  1010 | `{` |
|     86501 |  1011 | `	sxi32 rc = SXRET_OK;` |
|     86501 |  1012 | `	sxi32 nBrc = pVm->nBoundaryRc;` |
|     86501 |  1013 | `	const void *pResume = (const void *)pVm->pResumeFrame;` |
|         - |  1014 | `	sxu32 i;` |
|    172773 |  1015 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|     86305 |  1016 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|         - |  1017 | `		ph7_value out;` |
|     86305 |  1018 | `		if( PH7_VmClassLookupRaised(&(*pVm),nBrc,pResume) ){` |
|         - |  1019 | `			/* The previous step's diagnostic reached an error handler that threw: php's` |
|         - |  1020 | `			 * fetch ends there, and no later step runs (or warns). The resolver hands` |
|         - |  1021 | `			 * the throw on. */` |
|        17 |  1022 | `			return SXRET_OK;` |
|         - |  1023 | `		}` |
|     86301 |  1024 | `		PH7_MemObjInit(&(*pVm),&out);` |
|     86301 |  1025 | `		if( pStep->isProp && bWrite && (pCur->iFlags & MEMOBJ_OBJ) ){` |
|         - |  1026 | `			/* php's "indirect" only ever describes a VALUE: an object is a HANDLE, so a` |
|         - |  1027 | `			 * write through one lands however the handle was obtained — which is also why` |
|         - |  1028 | ``			 * the notice above stays silent for an object. `f($o->magic->p)` with`` |
|         - |  1029 | ``			 * `function f(&$x)` really does create and write `p` on the object __get`` |
|         - |  1030 | `			 * answered with. Bind the property and let the slot walk finish the chain. */` |
|         3 |  1031 | `			sxu32 nOut = SXU32_HIGH;` |
|         3 |  1032 | `			int bNoBind = 0;` |
|         3 |  1033 | `			int bLastStep = (i + 1 == pPath->nStep);` |
|         - |  1034 | `			ph7_value sMagicVal;` |
|         3 |  1035 | `			PH7_MemObjRelease(&out);` |
|         3 |  1036 | `			PH7_MemObjInit(&(*pVm),&sMagicVal);` |
|         4 |  1037 | `			rc = VmBindPropByRef(&(*pVm),pCur,&pStep->sProp,&nOut,&bNoBind,` |
|         1 |  1038 | `				bLastStep ? &sMagicVal : 0);` |
|         3 |  1039 | `			if( rc != SXRET_OK \|\| bNoBind ){` |
|       ! 0 |  1040 | `				if( rc == SXRET_OK && bLastStep ){` |
|         - |  1041 | `					/* An overloaded property one level down: its own notice has been` |
|         - |  1042 | `					 * raised and what __get answered is what php passes. */` |
|       ! 0 |  1043 | `					PH7_MemObjStore(&sMagicVal,pSlot);` |
|       ! 0 |  1044 | `					pSlot->nIdx = SXU32_HIGH;` |
|       ! 0 |  1045 | `				}` |
|       ! 0 |  1046 | `				PH7_MemObjRelease(&sMagicVal);` |
|       ! 0 |  1047 | `				return rc;` |
|         - |  1048 | `			}` |
|         3 |  1049 | `			PH7_MemObjRelease(&sMagicVal);` |
|         3 |  1050 | `			return VmWalkStepsFromSlot(&(*pVm),pPath,i + 1,nOut,pSlot);` |
|         - |  1051 | `		}` |
|     86299 |  1052 | `		if( pStep->isProp ){` |
|         - |  1053 | `			ph7_value nameVal;` |
|      3471 |  1054 | `			PH7_MemObjInitFromString(&(*pVm),&nameVal,&pStep->sProp);` |
|      3471 |  1055 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_MEMBER,PH7_MEMBER_READ,pCur,&nameVal,&out);` |
|      3471 |  1056 | `			PH7_MemObjRelease(&nameVal);` |
|     84566 |  1057 | `		}else if( pStep->bAppend ){` |
|         - |  1058 | ``			/* `f($o['a'][])`: the append lands in the temporary either way. A by-VALUE`` |
|         - |  1059 | ``			 * binding is php's runtime `Cannot use [] for reading`, the same Error the`` |
|         - |  1060 | `			 * slot-based walk raises for it. */` |
|         - |  1061 | `			sxi32 rcAp;` |
|         3 |  1062 | `			if( bWrite ){` |
|         - |  1063 | `				/* The appended element is a fresh NULL that nothing else can see —` |
|         - |  1064 | `				 * php binds the parameter to it and the temporary is dropped. */` |
|       ! 0 |  1065 | `				PH7_MemObjRelease(pCur);` |
|       ! 0 |  1066 | `				*pCur = out;` |
|       ! 0 |  1067 | `				continue;` |
|         - |  1068 | `			}` |
|         3 |  1069 | `			PH7_MemObjRelease(&out);` |
|         3 |  1070 | `			rcAp = VmThrowFromVm(&(*pVm),"Error","Cannot use [] for reading",` |
|         - |  1071 | `				sizeof("Cannot use [] for reading")-1);` |
|         3 |  1072 | `			return (rcAp == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       ! 0 |  1073 | `		}else{` |
|     82831 |  1074 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,bWrite ? 1 : 0,pCur,&pStep->sKey,&out);` |
|         - |  1075 | `		}` |
|     86297 |  1076 | `		PH7_MemObjRelease(pCur);` |
|     86297 |  1077 | `		*pCur = out;` |
|     86297 |  1078 | `		if( rc != SXRET_OK ){` |
|        23 |  1079 | `			return rc;` |
|         - |  1080 | `		}` |
|     43805 |  1081 | `	}` |
|     86473 |  1082 | `	PH7_MemObjStore(pCur,pSlot);` |
|     86473 |  1083 | `	pSlot->nIdx = SXU32_HIGH;` |
|     86473 |  1084 | `	return SXRET_OK;` |
|     43917 |  1085 | `}` |
|         - |  1086 | `/* Resolve a captured lvalue path as a BY-REF target: vivify the whole chain in place and` |
|         - |  1087 | ` * leave pSlot->nIdx pointing at the terminal (aliasable) slot for the by-ref binder. */` |
|       206 |  1088 | `static sxi32 VmResolvePathByRef(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|         5 |  1089 | `{` |
|         - |  1090 | `	sxu32 nCur;` |
|         - |  1091 | `	sxi32 rc;` |
|       211 |  1092 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|         - |  1093 | `		/* An overloaded container was asked for something to MODIFY and could only hand` |
|         - |  1094 | `		 * back a value: php notices that the write has no effect and carries on with the` |
|         - |  1095 | `		 * temporary. The notice is raised HERE — the fetch itself cannot know whether the` |
|         - |  1096 | `		 * parameter it feeds is by-reference, and a by-VALUE one is silent. */` |
|         - |  1097 | `		ph7_value cur;` |
|        47 |  1098 | `		PH7_MemObjInit(&(*pVm),&cur);` |
|        47 |  1099 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|        47 |  1100 | `		rc = VmPrefetchByRefVerdict(&(*pVm),pPath,&cur);` |
|        47 |  1101 | `		if( rc == SXRET_OK ){` |
|        39 |  1102 | `			rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,TRUE,pSlot);` |
|        19 |  1103 | `		}` |
|        47 |  1104 | `		PH7_MemObjRelease(&cur);` |
|        47 |  1105 | `		return rc;` |
|         - |  1106 | `	}` |
|       165 |  1107 | `	if( pPath->eRoot == 2 ){` |
|         - |  1108 | `		/* Subscripting a string: php refuses a by-ref bind to a string offset — but` |
|         - |  1109 | ``		 * it applies its OFFSET rules first, so `f($s["p"])` is the offset TypeError`` |
|         - |  1110 | ``		 * and `f($s[1.5])` warns about the cast before this Error is raised. */`` |
|         - |  1111 | `		sxi32 rcT;` |
|        17 |  1112 | `		if( pPath->nStep > 0 && !pPath->aStep[0].isProp ){` |
|         - |  1113 | `			SyBlob sTypeMsg;` |
|        17 |  1114 | `			sxi64 iOfft = 0;` |
|        14 |  1115 | `			if( VmStringOffsetResolve(&(*pVm),&pPath->aStep[0].sKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg)` |
|        10 |  1116 | `				== VM_STROFF_REJECT ){` |
|         3 |  1117 | `				rcT = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|         3 |  1118 | `				return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - |  1119 | `			}` |
|         6 |  1120 | `		}` |
|         - |  1121 | `		{` |
|         - |  1122 | `			/* WHICH refusal is php's depends on what the argument fetch was reaching` |
|         - |  1123 | ``			 * for: the offset ITSELF (`f($s[1])`, one step) cannot be referenced, while`` |
|         - |  1124 | `			 * a step INTO it is the same reach-inside every other consumer gets, named` |
|         - |  1125 | ``			 * for the STEP — `f($s[0][1])` is `... as an array` and `f($s[0]->p)` is`` |
|         - |  1126 | ``			 * `... as an object`. php derives all three from the opcode that consumes`` |
|         - |  1127 | `			 * the fetch; here the step count and the last step's kind are that. */` |
|        21 |  1128 | `			const char *zSoMsg = (pPath->nStep > 1)` |
|         4 |  1129 | `				? (pPath->aStep[pPath->nStep-1].isProp` |
|         - |  1130 | `					? "Cannot use string offset as an object"` |
|         - |  1131 | `					: "Cannot use string offset as an array")` |
|         6 |  1132 | `				: "Cannot create references to/from string offsets";` |
|        15 |  1133 | `			rcT = VmThrowFromVm(&(*pVm),"Error",zSoMsg,SyStrlen(zSoMsg));` |
|         - |  1134 | `		}` |
|        15 |  1135 | `		return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - |  1136 | `	}` |
|       151 |  1137 | `	if( pPath->eRoot == 1 ){` |
|        11 |  1138 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,TRUE); /* vivify $a */` |
|        11 |  1139 | `		if( pRoot == 0 ){` |
|       ! 0 |  1140 | `			return SXRET_OK;` |
|         - |  1141 | `		}` |
|        11 |  1142 | `		nCur = pRoot->nIdx;` |
|         7 |  1143 | `	}else{` |
|       142 |  1144 | `		nCur = pPath->nRootIdx;` |
|         - |  1145 | `	}` |
|       151 |  1146 | `	return VmWalkStepsFromSlot(&(*pVm),pPath,0,nCur,pSlot);` |
|       108 |  1147 | `}` |
|         - |  1148 | `/* Resolve a captured lvalue path as a BY-VALUE argument: read the chain (emitting php's` |
|         - |  1149 | ` * undefined-key/property/offset warnings) WITHOUT vivifying, leaving the terminal value in` |
|         - |  1150 | ` * pSlot. Re-drives the read handlers so the warning sequence matches php exactly. */` |
|     86462 |  1151 | `static sxi32 VmResolvePathByValue(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|         5 |  1152 | `{` |
|         - |  1153 | `	ph7_value cur;` |
|         - |  1154 | `	sxi32 rc;` |
|     86467 |  1155 | `	PH7_MemObjInit(&(*pVm),&cur);` |
|     86467 |  1156 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|         - |  1157 | `		/* The accessor already ran, where php runs it: a by-VALUE argument simply takes` |
|         - |  1158 | `		 * what it answered, in silence. */` |
|      3581 |  1159 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|     84679 |  1160 | `	}else if( pPath->eRoot == 1 ){` |
|         5 |  1161 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,FALSE);` |
|         5 |  1162 | `		if( pRoot == 0 ){` |
|         5 |  1163 | `			sxi32 nBrc = pVm->nBoundaryRc;` |
|         5 |  1164 | `			const void *pResume = (const void *)pVm->pResumeFrame;` |
|         5 |  1165 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&pPath->sRootName);` |
|         5 |  1166 | `			if( PH7_VmClassLookupRaised(&(*pVm),nBrc,pResume) ){` |
|         - |  1167 | `				/* The error handler threw: php's fetch ends there, and the steps` |
|         - |  1168 | `				 * after the root never run (or warn) at all. */` |
|         5 |  1169 | `				return SXRET_OK;` |
|         - |  1170 | `			}` |
|       ! 0 |  1171 | `		}else{` |
|       ! 0 |  1172 | `			PH7_MemObjLoad(pRoot,&cur);` |
|       ! 0 |  1173 | `			cur.nIdx = pRoot->nIdx;` |
|         - |  1174 | `		}` |
|       ! 0 |  1175 | `	}else{` |
|     82887 |  1176 | `		ph7_value *pRoot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pPath->nRootIdx);` |
|     82887 |  1177 | `		if( pRoot ){` |
|     82887 |  1178 | `			PH7_MemObjLoad(pRoot,&cur);` |
|     82887 |  1179 | `			cur.nIdx = pRoot->nIdx;` |
|     42105 |  1180 | `		}` |
|         - |  1181 | `	}` |
|     86463 |  1182 | `	rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,FALSE,pSlot);` |
|     86463 |  1183 | `	PH7_MemObjRelease(&cur);` |
|     86463 |  1184 | `	return rc;` |
|     43900 |  1185 | `}` |
|         - |  1186 | `/*` |
|         - |  1187 | ` * Is this actual argument REFUSED by a by-reference parameter?` |
|         - |  1188 | ` *` |
|         - |  1189 | ` * php answers from the argument's compile-time SHAPE, which the call site carries in` |
|         - |  1190 | ` * VmCallArgMap.nNonLvalMask (GenStateArgShape, compile.c): a literal, an operator or` |
|         - |  1191 | `` * cast result, a class constant, `@$x`, `$o?->p` or an assignment is a hard non-lvalue`` |
|         - |  1192 | `` * and binding one is `Argument #N ($p) could not be passed by reference`.`` |
|         - |  1193 | ` *` |
|         - |  1194 | ` * nPos is the argument's position on the operand stack, which is the position the` |
|         - |  1195 | ` * compiler classified — named arguments change which FORMAL a slot binds to, not the` |
|         - |  1196 | ` * slot's index, so both binders index the mask the same way.` |
|         - |  1197 | ` *` |
|         - |  1198 | ` * Without a shape mask (a SPREAD call, an engine-synthesized call, an indirect dispatch` |
|         - |  1199 | ` * through call_user_func or an array callable) this falls back to the runtime test the` |
|         - |  1200 | ` * binders used before: no slot to write back through, and not one of the values PH7 has` |
|         - |  1201 | ` * always passed by value instead. That test cannot tell a literal from a call RESULT —` |
|         - |  1202 | ` * php accepts the latter — which is exactly why the mask exists.` |
|         - |  1203 | ` */` |
|      8636 |  1204 | `PH7_PRIVATE int PH7_VmArgRefusedByRef(VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|         5 |  1205 | `{` |
|      8641 |  1206 | `	if( pMap && pMap->bArgShapes && nPos < 31 ){` |
|      8265 |  1207 | `		return (pMap->nNonLvalMask & (1u << nPos)) != 0;` |
|         - |  1208 | `	}` |
|       381 |  1209 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|       255 |  1210 | `		return 0;` |
|         - |  1211 | `	}` |
|       190 |  1212 | `	return (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|       126 |  1213 | `	    && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0;` |
|      4319 |  1214 | `}` |
|         - |  1215 | `/*` |
|         - |  1216 | `` * The same call site's OTHER answer: the argument is the RESULT of a call or of `new`.`` |
|         - |  1217 | ` *` |
|         - |  1218 | ` * php cannot know at compile time whether the callee returns a reference, so it defers` |
|         - |  1219 | ` * to the value: one that arrived WITH a reference binds silently, and one without gets` |
|         - |  1220 | ` * php's E_NOTICE and the callee then operates on the temporary. Emitting it is all this` |
|         - |  1221 | ` * does — a temp-call argument is never refused.` |
|         - |  1222 | ` */` |
|         - |  1223 | `/*` |
|         - |  1224 | ` * A typed by-REFERENCE parameter's coercion belongs to the CALLER's variable. php` |
|         - |  1225 | ` * converts the actual in weak mode and the REFERENCE then holds the conversion, so` |
|         - |  1226 | `` * `$v = 1.0; f($v);` with `function f(int &$x)` leaves both views int(1). PHL ran the`` |
|         - |  1227 | ` * declared-type check on the operand-stack COPY while the binder aliases the caller's` |
|         - |  1228 | ` * slot by index, so the conversion reached neither the callee (which reads through the` |
|         - |  1229 | ` * alias) nor the caller: both stayed float, and every other pair did the same` |
|         - |  1230 | `` * (`float &$y` given an int, `string &$s` given an int, `bool &$b` given an int).`` |
|         - |  1231 | ` *` |
|         - |  1232 | ` * Writes back only when the check actually changed the value's TYPE — an untyped` |
|         - |  1233 | ` * parameter, or one the actual already satisfies, copies nothing.` |
|         - |  1234 | ` */` |
|      4384 |  1235 | `PH7_PRIVATE void PH7_VmByRefArgWriteBack(ph7_vm *pVm,ph7_value *pArg,sxi32 iPreFlags)` |
|         5 |  1236 | `{` |
|         - |  1237 | `	ph7_value *pSlot;` |
|      4384 |  1238 | `	if( pArg->nIdx == SXU32_HIGH` |
|      4389 |  1239 | `	 \|\| (pArg->iFlags & MEMOBJ_ALL) == (iPreFlags & MEMOBJ_ALL) ){` |
|      4365 |  1240 | `		return;` |
|         - |  1241 | `	}` |
|        25 |  1242 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArg->nIdx);` |
|        25 |  1243 | `	if( pSlot && pSlot != pArg ){` |
|        25 |  1244 | `		PH7_MemObjStore(pArg,pSlot);` |
|        12 |  1245 | `	}` |
|      2193 |  1246 | `}` |
|     42609 |  1247 | `PH7_PRIVATE void PH7_VmArgTempCallNotice(ph7_vm *pVm,VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|         5 |  1248 | `{` |
|     42614 |  1249 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| nPos >= 31 ){` |
|       381 |  1250 | `		return;` |
|         - |  1251 | `	}` |
|     42238 |  1252 | `	if( (pMap->nTempCallMask & (1u << nPos)) == 0 ){` |
|     42198 |  1253 | `		return;` |
|         - |  1254 | `	}` |
|        44 |  1255 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|         3 |  1256 | `		return; /* a by-reference RETURN: php is silent and binds it */` |
|         - |  1257 | `	}` |
|        42 |  1258 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Only variables should be passed by reference");` |
|     21292 |  1259 | `}` |
|         - |  1260 | `/*` |
|         - |  1261 | `` * A GENERATOR's arguments are bound at the `g(...)` that BUILDS the Generator object,`` |
|         - |  1262 | ` * before any resume — php's rule, and where php also refuses a by-reference parameter` |
|         - |  1263 | ` * handed a non-variable. That branch collects its actuals into a vector of its own (and` |
|         - |  1264 | ` * reorders it for named arguments), so neither of the two OP_CALL binders ever sees them` |
|         - |  1265 | `` * and `function g(&$x){ yield; } g(1 + 1);` built a Generator in silence.`` |
|         - |  1266 | ` *` |
|         - |  1267 | ` * Answers PH7_EXCEPTION (or PH7_ABORT) for the first refused position, having raised the` |
|         - |  1268 | ` * throw; SXRET_OK otherwise, with php's temp-call notice emitted along the way. Named` |
|         - |  1269 | ` * arguments are resolved by NAME against the formals here rather than through the` |
|         - |  1270 | ` * branch's own mapping, which is built later and freed inside its block.` |
|         - |  1271 | ` */` |
|       208 |  1272 | `static sxi32 VmScreenGenByRefArgs(ph7_vm *pVm,ph7_vm_func *pFunc,VmCallArgMap *pMap,` |
|         - |  1273 | `	ph7_value *pArg,sxu32 nActual,ph7_class *pSelfHint)` |
|         5 |  1274 | `{` |
|       213 |  1275 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       213 |  1276 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|         - |  1277 | `	sxu32 i;` |
|       477 |  1278 | `	for( i = 0 ; i < nActual ; ++i ){` |
|       275 |  1279 | `		sxu32 n = i;` |
|       275 |  1280 | `		if( pMap && pMap->bHasNamed && i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       199 |  1281 | `			for( n = 0 ; n < nFormal ; ++n ){` |
|       180 |  1282 | `				if( pMap->aNames[i].nByte == SyStringLength(&aFormal[n].sName)` |
|       171 |  1283 | `				 && SyMemcmp(pMap->aNames[i].zString,SyStringData(&aFormal[n].sName),` |
|       228 |  1284 | `					pMap->aNames[i].nByte) == 0 ){` |
|        84 |  1285 | `					break;` |
|         - |  1286 | `				}` |
|        55 |  1287 | `			}` |
|        47 |  1288 | `		}` |
|       275 |  1289 | `		if( n >= nFormal \|\| (aFormal[n].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|       231 |  1290 | `			continue;` |
|         - |  1291 | `		}` |
|        46 |  1292 | `		if( PH7_VmArgRefusedByRef(pMap,i,&pArg[i]) ){` |
|        10 |  1293 | `			sxi32 rcT = VmThrowByRefRefusal(&(*pVm),` |
|         6 |  1294 | `				(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|         6 |  1295 | `				&pFunc->sName,pFunc,n + 1,&aFormal[n].sName);` |
|         7 |  1296 | `			return (rcT == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - |  1297 | `		}` |
|        40 |  1298 | `		PH7_VmArgTempCallNotice(&(*pVm),pMap,i,&pArg[i]);` |
|        21 |  1299 | `	}` |
|       207 |  1300 | `	return SXRET_OK;` |
|       109 |  1301 | `}` |
|         - |  1302 | `/*` |
|         - |  1303 | ` * The status a resolver answers once a diagnostic it raised reached an error handler that` |
|         - |  1304 | ` * threw: the parked C-boundary status, taken over (the caller routes it now, so the fetch` |
|         - |  1305 | ` * point must not fire it again), or PH7_EXCEPTION when a try caught it in place and only` |
|         - |  1306 | ` * the resume record says so.` |
|         - |  1307 | ` */` |
|        20 |  1308 | `static sxi32 VmResolveRaisedRc(ph7_vm *pVm,sxi32 nBrcBefore)` |
|         1 |  1309 | `{` |
|        21 |  1310 | `	sxi32 rc = pVm->nBoundaryRc;` |
|        21 |  1311 | `	if( rc != nBrcBefore && rc != 0 ){` |
|        21 |  1312 | `		pVm->nBoundaryRc = nBrcBefore;` |
|        21 |  1313 | `		return rc;` |
|         - |  1314 | `	}` |
|       ! 0 |  1315 | `	return PH7_EXCEPTION;` |
|        11 |  1316 | `}` |
|         - |  1317 | `/*` |
|         - |  1318 | ` * D1: resolve deferred call arguments in [pArg, pTos) before the callee consumes them.` |
|         - |  1319 | ` *` |
|         - |  1320 | `` * A plain `$var` call argument whose callee signature is unknown at compile time is`` |
|         - |  1321 | ` * emitted as a DEFERRED load (OP_LOAD iP1=1,iP2=3): when the variable does not exist it` |
|         - |  1322 | ` * yields a NULL slot tagged MEMOBJ_AUX_DEFERRED that carries the variable name in x.pOther` |
|         - |  1323 | ` * (a VM-lifetime bytecode string). This helper runs at each OP_CALL dispatch branch, while` |
|         - |  1324 | ` * pVm->pFrame is still the CALLER frame, once the callee's by-ref shape is known:` |
|         - |  1325 | ` *` |
|         - |  1326 | ` *   by-ref position  -> create the variable in the caller frame now and give the slot its` |
|         - |  1327 | ` *                       real nIdx so the ordinary by-ref binder aliases it (silent, as php).` |
|         - |  1328 | ` *   by-value position-> raise php's "Undefined variable $x" and pass a clean NULL WITHOUT` |
|         - |  1329 | ` *                       creating the variable in the caller.` |
|         - |  1330 | ` *` |
|         - |  1331 | ` * The by-ref decision for positional argument n comes from, in priority order:` |
|         - |  1332 | ` *   bAllByValue  -> everything by-value (an unresolvable/erroring callee);` |
|         - |  1333 | ` *   bAllByRef    -> everything by-ref (the indirect array-callable/__invoke dispatch paths,` |
|         - |  1334 | ` *                   which historically over-vivified every plain-var arg — preserved here` |
|         - |  1335 | ` *                   rather than regressed; their by-value refinement is a later slice);` |
|         - |  1336 | ` *   pFormal      -> a user function's formal-argument array (honoring a trailing variadic);` |
|         - |  1337 | ` *   nByRefMask   -> a builtin by-ref position bitmask (used when pFormal == 0).` |
|         - |  1338 | ` *` |
|         - |  1339 | ` * A DEFINED variable never carries the marker (it loads with its real nIdx), so this is a` |
|         - |  1340 | ` * no-op for it; a call with no deferred args pays only one flag test per slot.` |
|         - |  1341 | ` */` |
|  21229997 |  1342 | `PH7_PRIVATE sxi32 PH7_VmResolveDeferredArgs(` |
|         - |  1343 | `	ph7_vm *pVm,` |
|         - |  1344 | `	ph7_value *pArg,` |
|         - |  1345 | `	ph7_value *pTos,` |
|         - |  1346 | `	ph7_vm_func_arg *pFormal,` |
|         - |  1347 | `	sxu32 nFormal,` |
|         - |  1348 | `	sxu32 nByRefMask,` |
|         - |  1349 | `	int bAllByRef,` |
|         - |  1350 | `	int bAllByValue,` |
|         - |  1351 | `	VmCallArgMap *pCallMap)` |
|         5 |  1352 | `{` |
|         - |  1353 | `	ph7_value *p;` |
|  21230002 |  1354 | `	sxu32 n = 0;` |
|  21230002 |  1355 | `	sxi32 nBrc = pVm->nBoundaryRc;` |
|  21230002 |  1356 | `	const void *pResume = (const void *)pVm->pResumeFrame;` |
|  52634880 |  1357 | `	for( p = pArg ; p < pTos ; ++p, ++n ){` |
|  31405009 |  1358 | `		int bByRef = 0;` |
|  31405009 |  1359 | `		int bDeferred = (p->iFlags & (MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH)) != 0;` |
|         - |  1360 | `		SyString sName;` |
|  31405004 |  1361 | `		if( !bDeferred` |
|  31305468 |  1362 | `		 && (bAllByValue \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0) ){` |
|         - |  1363 | `			/* Nothing deferred in this position, and no slot in the VM could refuse` |
|         - |  1364 | `			 * being aliased -- the ordinary case, out through one test. */` |
|  16996062 |  1365 | `			continue;` |
|         - |  1366 | `		}` |
|  28816434 |  1367 | `		if( bAllByValue ){` |
|        15 |  1368 | `			bByRef = 0;` |
|  28816428 |  1369 | `		}else if( bAllByRef ){` |
|         - |  1370 | `			/* bAllByRef comes ONLY from the array-callable-value and __invoke dispatch paths,` |
|         - |  1371 | `			 * which cannot expose the target's per-parameter by-ref flags here. Commit 1 chose` |
|         - |  1372 | `			 * to over-vivify every deferred PLAIN-VAR arg on those paths (harmless: it just` |
|         - |  1373 | `			 * materializes the caller variable), and that is preserved. But a deferred` |
|         - |  1374 | `			 * ELEMENT/PROPERTY (MEMOBJ_AUX_DEFPATH) must NOT be blanket-vivified there: a missing` |
|         - |  1375 | `			 * property would fatal ("Cannot create dynamic property") and a missing element would` |
|         - |  1376 | `			 * silently vivify + swallow php's "Undefined array key" warning. Resolve those` |
|         - |  1377 | `			 * by-value (warn + pass NULL), which matches php for a by-VALUE __invoke/callable —` |
|         - |  1378 | `			 * the genuine by-ref-out-param-into-an-element case stays unsupported here, exactly` |
|         - |  1379 | `			 * as it was before this slice. */` |
|       ! 0 |  1380 | `			bByRef = (p->iFlags & MEMOBJ_AUX_DEFPATH) ? 0 : 1;` |
|  28816422 |  1381 | `		}else if( pFormal ){` |
|    156128 |  1382 | `			sxu32 idx = n;` |
|    156123 |  1383 | `			if( pCallMap && pCallMap->bHasNamed && n < pCallMap->nTotal` |
|      1977 |  1384 | `			 && pCallMap->aNames[n].nByte > 0 ){` |
|         - |  1385 | `				/* A NAMED actual binds to the formal its NAME picks, not to the one at its` |
|         - |  1386 | ``				 * stack position: `r(x: $a["k"])` is argument #1 on the stack and parameter`` |
|         - |  1387 | `				 * $x in the declaration. Reading the by-ref-ness positionally consulted the` |
|         - |  1388 | `				 * wrong formal, so a by-reference named argument naming a missing element` |
|         - |  1389 | ``				 * warned `Undefined array key` and passed NULL where php creates it. */`` |
|         - |  1390 | `				sxu32 f;` |
|       981 |  1391 | `				idx = SXU32_HIGH;` |
|      1899 |  1392 | `				for( f = 0 ; f < nFormal ; ++f ){` |
|      1570 |  1393 | `					if( pCallMap->aNames[n].nByte == SyStringLength(&pFormal[f].sName)` |
|      1424 |  1394 | `					 && SyMemcmp(pCallMap->aNames[n].zString,` |
|      1902 |  1395 | `						SyStringData(&pFormal[f].sName),pCallMap->aNames[n].nByte) == 0 ){` |
|       657 |  1396 | `						idx = f;` |
|       657 |  1397 | `						break;` |
|         - |  1398 | `					}` |
|       464 |  1399 | `				}` |
|    155640 |  1400 | `			}else if( idx >= nFormal ){` |
|         - |  1401 | `				/* Beyond the declared formals: a trailing variadic absorbs the tail` |
|         - |  1402 | `				 * (and dictates its by-ref-ness); otherwise the extra arg is by-value. */` |
|     13323 |  1403 | `				idx = (nFormal > 0 && (pFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC))` |
|      9898 |  1404 | `					? nFormal - 1 : SXU32_HIGH;` |
|      4402 |  1405 | `			}` |
|    156128 |  1406 | `			if( idx != SXU32_HIGH ){` |
|    148908 |  1407 | `				bByRef = (pFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|     74409 |  1408 | `			}` |
|     77996 |  1409 | `		}else{` |
|  28660299 |  1410 | `			bByRef = (n < 31 && (nByRefMask & (1u << n))) ? 1 : 0;` |
|         - |  1411 | `		}` |
|  28816434 |  1412 | `		if( !bDeferred ){` |
|         - |  1413 | `			/* An argument that ALREADY resolved to a slot, in a by-reference` |
|         - |  1414 | `			 * position: php screens the property behind it here, because binding` |
|         - |  1415 | `			 * the callee's parameter to it is an INDIRECT modification -- the` |
|         - |  1416 | `			 * callee's write would reach a readonly property, or a native one whose` |
|         - |  1417 | `			 * handler refuses every write, with nothing in the way. This is the one` |
|         - |  1418 | `			 * place both kinds of callee agree on: a user function's formals and a` |
|         - |  1419 | `			 * builtin's signature-derived mask both arrive here. */` |
|  28616164 |  1420 | `			if( bByRef ){` |
|     39551 |  1421 | `				sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),p->nIdx);` |
|     39551 |  1422 | `				if( rcInd != SXRET_OK ){` |
|        73 |  1423 | `					return rcInd;` |
|         - |  1424 | `				}` |
|     19756 |  1425 | `			}` |
|  28616146 |  1426 | `			continue;` |
|         - |  1427 | `		}` |
|    200275 |  1428 | `		if( p->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|         - |  1429 | `			/* D1 commit 2: a deferred array-element/property lvalue. Detach the descriptor` |
|         - |  1430 | `			 * FIRST (so an exception mid-resolve, or a later stack release, cannot double-free` |
|         - |  1431 | `			 * it) then re-walk it in the chosen mode. */` |
|     86673 |  1432 | `			VmDeferredPath *pPath = (VmDeferredPath *)p->x.pOther;` |
|         - |  1433 | `			sxi32 rc;` |
|     86673 |  1434 | `			p->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|     86673 |  1435 | `			p->x.pOther = 0;` |
|     86673 |  1436 | `			MemObjSetType(p,MEMOBJ_NULL);` |
|     86673 |  1437 | `			p->nIdx = SXU32_HIGH;` |
|     86673 |  1438 | `			if( bByRef ){` |
|       211 |  1439 | `				rc = VmResolvePathByRef(&(*pVm),pPath,p);` |
|       108 |  1440 | `			}else{` |
|     86467 |  1441 | `				rc = VmResolvePathByValue(&(*pVm),pPath,p);` |
|         - |  1442 | `			}` |
|     86673 |  1443 | `			VmFreeDeferredPath(pPath);` |
|     86673 |  1444 | `			if( rc != SXRET_OK ){` |
|        93 |  1445 | `				return rc;` |
|         - |  1446 | `			}` |
|     86585 |  1447 | `			if( PH7_VmClassLookupRaised(&(*pVm),nBrc,pResume) ){` |
|         - |  1448 | `				/* A diagnostic of the walk reached an error handler that threw. php` |
|         - |  1449 | `				 * stops at that SEND: no later operand is read, and the call is not` |
|         - |  1450 | `				 * made. The throw is the caller's to route, as if this had raised it. */` |
|         9 |  1451 | `				return VmResolveRaisedRc(&(*pVm),nBrc);` |
|         - |  1452 | `			}` |
|     86577 |  1453 | `			continue;` |
|         - |  1454 | `		}` |
|         - |  1455 | `		/* Recover the deferred variable name and drop the marker + carrier. */` |
|    113607 |  1456 | `		SyStringInitFromBuf(&sName,(const char *)p->x.pOther,` |
|         - |  1457 | `			p->x.pOther ? SyStrlen((const char *)p->x.pOther) : 0);` |
|    113607 |  1458 | `		p->iFlags &= ~MEMOBJ_AUX_DEFERRED;` |
|    113607 |  1459 | `		p->x.pOther = 0;` |
|    113607 |  1460 | `		if( bByRef ){` |
|         - |  1461 | `			/* Materialize in the caller frame; the value stays NULL, only the slot` |
|         - |  1462 | `			 * back-reference (nIdx) matters for the by-ref binder / write-back. */` |
|       173 |  1463 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|       173 |  1464 | `			if( pObj ){` |
|       173 |  1465 | `				p->nIdx = pObj->nIdx;` |
|        84 |  1466 | `			}` |
|        89 |  1467 | `		}else{` |
|         - |  1468 | `			/* A variable a frameless call's operand names may exist by now -- it was` |
|         - |  1469 | `			 * left unread on purpose (OP_LOAD iP2 = 5), or a later argument created` |
|         - |  1470 | `			 * it -- and is read here. */` |
|    113439 |  1471 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE);` |
|    113439 |  1472 | `			if( pObj ){` |
|    113369 |  1473 | `				PH7_MemObjLoad(pObj,p);` |
|    113369 |  1474 | `				p->nIdx = pObj->nIdx;` |
|    113369 |  1475 | `				continue;` |
|         - |  1476 | `			}` |
|         - |  1477 | `			/* php warns and passes NULL without creating the variable; the slot is` |
|         - |  1478 | `			 * already a clean NULL with nIdx == SXU32_HIGH from the deferred load. */` |
|        75 |  1479 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        75 |  1480 | `			if( PH7_VmClassLookupRaised(&(*pVm),nBrc,pResume) ){` |
|        13 |  1481 | `				return VmResolveRaisedRc(&(*pVm),nBrc); /* likewise */` |
|         - |  1482 | `			}` |
|         - |  1483 | `		}` |
|       118 |  1484 | `	}` |
|  21229876 |  1485 | `	return SXRET_OK;` |
|  10616386 |  1486 | `}` |
|         - |  1487 | `/*` |
|         - |  1488 | ` * Did resolving a class NAME raise?` |
|         - |  1489 | ` *` |
|         - |  1490 | ` * The lookup can run an AUTOLOADER, and that autoloader can throw. The boundary rail` |
|         - |  1491 | ` * either parks the status in nBoundaryRc or — when a try caught it in place — records a` |
|         - |  1492 | ` * resume frame; either way the throw is already the engine's to land. A call site that` |
|         - |  1493 | `` * sees the class "missing" and piles its own `Class "X" not found` Error on top reports a`` |
|         - |  1494 | ` * failure php never reports, and that second Error belongs to nobody: it came back` |
|         - |  1495 | ` * UNCAUGHT and killed the script right after the real exception had been handled.` |
|         - |  1496 | ` *` |
|         - |  1497 | ` * Snapshot (nBoundaryRc, pResumeFrame) before the lookup and pass them here after.` |
|         - |  1498 | ` */` |
|    173348 |  1499 | `PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore)` |
|         5 |  1500 | `{` |
|    173353 |  1501 | `	return pVm->nBoundaryRc != nBrcBefore \|\| (const void *)pVm->pResumeFrame != pResumeBefore;` |
|         5 |  1502 | `}` |
|         - |  1503 | `/*` |
|         - |  1504 | `` * Name php's error for a class+method callable that the DIRECT `$cb()` dispatch cannot`` |
|         - |  1505 | ` * call, or return 0 when it resolves.` |
|         - |  1506 | ` *` |
|         - |  1507 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|         - |  1508 | ` * result for an unresolvable pair — silence a caller cannot detect — so the direct call` |
|         - |  1509 | ` * site has to decide for itself. It used to do that only for the ARRAY form; the` |
|         - |  1510 | ``  * `"Class::method"` STRING form went straight to the dispatcher, and `$cb='C::nosuch'` `` |
|         - |  1511 | ` * evaluated to NULL with no diagnostic at all where php throws.` |
|         - |  1512 | ` *` |
|         - |  1513 | ` * pClass is the resolved target class (0 when the name named nothing); zCls/nCls is the` |
|         - |  1514 | ` * class name AS WRITTEN, which is what php's not-found message quotes. bStaticForm says the` |
|         - |  1515 | ` * target was a class NAME rather than an object. Messages that interpolate a name are built` |
|         - |  1516 | ` * into zBuf.` |
|         - |  1517 | ` *` |
|         - |  1518 | ` * Visibility is NOT decided here: an inaccessible method is diagnosed downstream by the` |
|         - |  1519 | ` * dispatch itself ("Call to private method C::p() from global scope"), php-exact already —` |
|         - |  1520 | ` * and php reports visibility BEFORE staticness, so the static rule below has to stay quiet` |
|         - |  1521 | ` * for a method this scope could not reach anyway.` |
|         - |  1522 | ` */` |
|    200348 |  1523 | `static const char * VmCallableClassMethodError(` |
|         - |  1524 | `	ph7_vm *pVm,` |
|         - |  1525 | `	ph7_class *pClass,             /* Resolved target class, or 0 */` |
|         - |  1526 | `	const char *zCls,sxu32 nCls,   /* Its name as the callable wrote it */` |
|         - |  1527 | `	const char *zMeth,sxu32 nMeth, /* The method name */` |
|         - |  1528 | `	int bStaticForm,               /* TRUE when the target is a class NAME, not an object */` |
|         - |  1529 | `	char *zBuf,int nBuf            /* Scratch for the messages that quote a name */` |
|         - |  1530 | `	)` |
|         5 |  1531 | `{` |
|         - |  1532 | `	ph7_class_method *pMethod;` |
|         - |  1533 | `	SyString sMeth;` |
|         - |  1534 | `` 	/* php's fallback for a name this class cannot reach: when the CALLER holds a `$this` `` |
|         - |  1535 | `	 * that is an instance of it, the name resolves to the __call TRAMPOLINE rather than to` |
|         - |  1536 | `	 * __callStatic — and a trampoline is a NON-STATIC function, so this dispatch, which` |
|         - |  1537 | `	 * carries no object, refuses it exactly as it refuses any other non-static method named` |
|         - |  1538 | `	 * through a class. The callback spellings bind that receiver and run (php's` |
|         - |  1539 | `	 * direct-vs-callback asymmetry, one rule apart). The message names the class the` |
|         - |  1540 | `	 * callable WROTE and the name as written, even when the name is a declared static` |
|         - |  1541 | `	 * method: it is the trampoline being refused, not the method. */` |
|    200353 |  1542 | `	int bFallback = bStaticForm && PH7_VmStaticFallbackThis(&(*pVm),pClass) != 0;` |
|    200353 |  1543 | `	if( pClass == 0 ){` |
|        51 |  1544 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|        51 |  1545 | `		return zBuf;` |
|         - |  1546 | `	}` |
|    200305 |  1547 | `	pMethod = PH7_ClassExtractMethod(pClass,zMeth,nMeth);` |
|    200305 |  1548 | `	if( pMethod == 0 ){` |
|       121 |  1549 | `		if( bFallback ){` |
|         7 |  1550 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|         2 |  1551 | `				&pClass->sDisp,(int)nMeth,zMeth);` |
|         5 |  1552 | `			return zBuf;` |
|         - |  1553 | `		}` |
|         - |  1554 | `		/* A class that answers for unknown names through the catch-all has nothing to` |
|         - |  1555 | `		 * report: php runs __callStatic (class-name target) / __call (object target) for` |
|         - |  1556 | `		 * ANY method name, and the dispatcher below routes it. */` |
|       117 |  1557 | `		const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       117 |  1558 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|        89 |  1559 | `			return 0;` |
|         - |  1560 | `		}` |
|        43 |  1561 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",` |
|        14 |  1562 | `			&pClass->sDisp,(int)nMeth,zMeth);` |
|        29 |  1563 | `		return zBuf;` |
|         - |  1564 | `	}` |
|         - |  1565 | `	/* An ABSTRACT method (an interface's included) has no body to call — the same message` |
|         - |  1566 | ``	 * the `C::m()` SYNTAX raises in vm_ops_oo.c, which this dispatch reached only as a`` |
|         - |  1567 | `	 * mangled internal function name. */` |
|    200186 |  1568 | `	SyStringInitFromBuf(&sMeth,zMeth,nMeth);` |
|    200186 |  1569 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         7 |  1570 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%z()",&pClass->sDisp,&sMeth);` |
|         7 |  1571 | `		return zBuf;` |
|         - |  1572 | `	}` |
|         - |  1573 | `	/* Named through a class NAME, a non-static method is never callable: php refuses even` |
|         - |  1574 | `	 * when the CALLER has a compatible $this (unlike call_user_func, which binds it). The` |
|         - |  1575 | `	 * message names the OWNING class and the method's declared spelling. */` |
|         - |  1576 | `	{` |
|         - |  1577 | `		SyString sDecl;` |
|         - |  1578 | `		int bAccessible;` |
|    200180 |  1579 | `		SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|         - |  1580 | `			SyStringLength(&pMethod->sFunc.sName));` |
|    300291 |  1581 | `		bAccessible = pMethod->iProtection == PH7_CLASS_PROT_PUBLIC` |
|    200176 |  1582 | `			\|\| PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|        23 |  1583 | `				&sDecl,pMethod->iProtection,FALSE);` |
|    200180 |  1584 | `		if( !bAccessible && bFallback ){` |
|         - |  1585 | `			/* Inaccessible goes the same way as missing: php never reports the visibility,` |
|         - |  1586 | `			 * because the name resolved to the trampoline before visibility could matter. */` |
|        13 |  1587 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|         4 |  1588 | `				&pClass->sDisp,(int)nMeth,zMeth);` |
|        17 |  1589 | `			return zBuf;` |
|         - |  1590 | `		}` |
|    200172 |  1591 | `		if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 && bAccessible ){` |
|         - |  1592 | `			/* Deciding class vs NAMED class: a trait is php's compile-time construct, so` |
|         - |  1593 | `			 * every message names the class that composed it (PH7_VmMethodScopeName). */` |
|        25 |  1594 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|        16 |  1595 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|        17 |  1596 | `			return zBuf;` |
|         - |  1597 | `		}` |
|         - |  1598 | `	}` |
|    200156 |  1599 | `	return 0;` |
|    100179 |  1600 | `}` |
|         - |  1601 | `/*` |
|         - |  1602 | ` * php's visibility refusal for a method call, worded once: "Call to private A::m() from` |
|         - |  1603 | ` * scope S" (or "from global scope"). Two sites raise it — OP_CALL's screen and the` |
|         - |  1604 | ` * first-class-callable one below — and php names the DECLARING class, not the class the` |
|         - |  1605 | ` * lookup went through.` |
|         - |  1606 | ` */` |
|        32 |  1607 | `static const char * VmMethodVisibilityMsg(ph7_vm *pVm,ph7_class *pDecl,` |
|         - |  1608 | `	const char *zMeth,sxu32 nMeth,sxi32 iProtection,char *zBuf,int nBuf)` |
|         4 |  1609 | `{` |
|        36 |  1610 | `	const char *zVis = iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        36 |  1611 | `	ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|        36 |  1612 | `	if( pScope ){` |
|         4 |  1613 | `		SyBufferFormat(zBuf,nBuf,"Call to %s method %z::%.*s() from scope %z",` |
|         1 |  1614 | `			zVis,&pDecl->sDisp,(int)nMeth,zMeth,&pScope->sDisp);` |
|         2 |  1615 | `	}else{` |
|        49 |  1616 | `		SyBufferFormat(zBuf,nBuf,"Call to %s method %z::%.*s() from global scope",` |
|        15 |  1617 | `			zVis,&pDecl->sDisp,(int)nMeth,zMeth);` |
|         - |  1618 | `	}` |
|        36 |  1619 | `	return zBuf;` |
|         4 |  1620 | `}` |
|         - |  1621 | `/*` |
|         - |  1622 | ` * Is this class+method pair one a call would reach DIRECTLY from here — a real method (not` |
|         - |  1623 | ` * abstract, not a name only the catch-all answers) that the current scope may call? php` |
|         - |  1624 | ` * decides exactly this when it BUILDS a method Closure, and stores the resolved function; the` |
|         - |  1625 | ` * answer is what the VM_INSTANCE_FCC_SCREENED mark records, so the invocation never asks again.` |
|         - |  1626 | ` * A pair that answers FALSE here is the __call/__callStatic trampoline's, and its closure must` |
|         - |  1627 | ` * keep routing there.` |
|         - |  1628 | ` */` |
|       652 |  1629 | `PH7_PRIVATE int PH7_VmFccMethodIsDirect(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName)` |
|         5 |  1630 | `{` |
|         - |  1631 | `	ph7_class_method *pMethod;` |
|         - |  1632 | `	SyString sDecl;` |
|       657 |  1633 | `	if( pClass == 0 \|\| nName < 1 ){` |
|       ! 0 |  1634 | `		return 0;` |
|         - |  1635 | `	}` |
|       657 |  1636 | `	pMethod = PH7_ClassExtractMethod(pClass,zName,nName);` |
|       657 |  1637 | `	if( pMethod == 0 \|\| (pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       174 |  1638 | `		return 0;` |
|         - |  1639 | `	}` |
|       487 |  1640 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|       369 |  1641 | `		return 1;` |
|         - |  1642 | `	}` |
|       123 |  1643 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|         - |  1644 | `		SyStringLength(&pMethod->sFunc.sName));` |
|         - |  1645 | `	/* The OWNING class decides (a trait method is owned by the class that composed it) — the` |
|         - |  1646 | `	 * same argument every other visibility site passes. */` |
|       182 |  1647 | `	return PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       118 |  1648 | `		&sDecl,pMethod->iProtection,FALSE) ? 1 : 0;` |
|       331 |  1649 | `}` |
|         - |  1650 | `/*` |
|         - |  1651 | `` * Resolve `$o->m(...)` / `C::m(...)` the way php resolves the CALL it stands for, and say`` |
|         - |  1652 | ` * why when it cannot. php builds a first-class callable through the same member lookup a` |
|         - |  1653 | ` * real call goes through, so every refusal a call would raise happens HERE, at creation:` |
|         - |  1654 | ` * an undefined method, an inaccessible one, an abstract one, and a non-static one named` |
|         - |  1655 | ` * through a class with no receiver to run on. PHL created a Closure for all four and only` |
|         - |  1656 | ` * discovered the problem when (and if) it was invoked — a closure that is built and dropped` |
|         - |  1657 | ` * reported nothing at all.` |
|         - |  1658 | ` *` |
|         - |  1659 | ` * The receiver is the other half of the same lookup. php's ZEND_INIT_STATIC_METHOD_CALL` |
|         - |  1660 | `` * binds the CALLING frame's `$this` when the resolved method is non-static and that object`` |
|         - |  1661 | `` * is an instance of the named class, which is what makes `self::m(...)` inside an instance`` |
|         - |  1662 | ` * method a working callable rather than a static one; PHL bound only the scope, so the` |
|         - |  1663 | ` * closure could never run. *ppRecv is that object, or 0 for a genuinely static callable.` |
|         - |  1664 | ` *` |
|         - |  1665 | ` * Answers 0 when the callable is valid. Messages that quote a name are built into zBuf.` |
|         - |  1666 | ` */` |
|       380 |  1667 | `static const char * VmFccMemberError(ph7_vm *pVm,ph7_class *pClass,` |
|         - |  1668 | `	const char *zCls,sxu32 nCls,const char *zMeth,sxu32 nMeth,int bStaticForm,` |
|         - |  1669 | `	ph7_class_instance **ppRecv,char *zBuf,int nBuf)` |
|         5 |  1670 | `{` |
|         - |  1671 | `	ph7_class_method *pMethod;` |
|         - |  1672 | `	SyString sDecl;` |
|       385 |  1673 | `	*ppRecv = 0;` |
|       385 |  1674 | `	if( pClass == 0 ){` |
|         9 |  1675 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|         9 |  1676 | `		return zBuf;` |
|         - |  1677 | `	}` |
|       377 |  1678 | `	pMethod = nMeth > 0 ? PH7_ClassExtractMethod(pClass,zMeth,nMeth) : 0;` |
|       377 |  1679 | `	if( pMethod == 0 ){` |
|         - |  1680 | `		/* A name the class answers through the catch-all is callable, and the catch-all` |
|         - |  1681 | `		 * the STATIC spelling reaches depends on the receiver, exactly as it does for a` |
|         - |  1682 | `		 * call (PH7_VmStaticFallbackThis). */` |
|        80 |  1683 | `		if( bStaticForm ){` |
|        56 |  1684 | `			*ppRecv = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|        52 |  1685 | `			if( *ppRecv` |
|        51 |  1686 | `			 \|\| PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1) ){` |
|        52 |  1687 | `				return 0;` |
|         1 |  1688 | `			}` |
|        30 |  1689 | `		}else if( PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) ){` |
|        26 |  1690 | `			return 0;` |
|         - |  1691 | `		}` |
|        10 |  1692 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",` |
|         3 |  1693 | `			&pClass->sDisp,(int)nMeth,zMeth);` |
|         7 |  1694 | `		return zBuf;` |
|         - |  1695 | `	}` |
|       301 |  1696 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|         - |  1697 | `		SyStringLength(&pMethod->sFunc.sName));` |
|       301 |  1698 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  1699 | `		/* Named through the CLASS only: an instance of an abstract class cannot exist, so` |
|         - |  1700 | `		 * the object spelling never reaches an abstract body. */` |
|         7 |  1701 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%.*s()",` |
|         2 |  1702 | `			&pClass->sDisp,(int)nMeth,zMeth);` |
|         5 |  1703 | `		return zBuf;` |
|         - |  1704 | `	}` |
|       292 |  1705 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|       194 |  1706 | `	 && !PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|        43 |  1707 | `			&sDecl,pMethod->iProtection,FALSE) ){` |
|         - |  1708 | `		/* Inaccessible: the catch-all answers for it, on the same receiver a call would use. */` |
|        31 |  1709 | `		*ppRecv = bStaticForm ? PH7_VmStaticFallbackThis(&(*pVm),pClass) : 0;` |
|        31 |  1710 | `		if( *ppRecv ){` |
|         5 |  1711 | `			return 0;` |
|         - |  1712 | `		}` |
|        36 |  1713 | `		if( bStaticForm` |
|        17 |  1714 | `			? PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1) != 0` |
|        10 |  1715 | `			: PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) != 0 ){` |
|        14 |  1716 | `			return 0;` |
|         - |  1717 | `		}` |
|         - |  1718 | `		/* The DECIDING class is the declaring one (its trait grants live there); the class` |
|         - |  1719 | `		 * php NAMES is the composing one — a trait has no runtime existence in php. */` |
|        21 |  1720 | `		return VmMethodVisibilityMsg(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|         6 |  1721 | `			zMeth,nMeth,pMethod->iProtection,zBuf,nBuf);` |
|         - |  1722 | `	}` |
|       269 |  1723 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|        54 |  1724 | `		*ppRecv = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|        54 |  1725 | `		if( *ppRecv == 0 ){` |
|         7 |  1726 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|         4 |  1727 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|         5 |  1728 | `			return zBuf;` |
|         - |  1729 | `		}` |
|        24 |  1730 | `	}` |
|       265 |  1731 | `	return 0;` |
|       195 |  1732 | `}` |
|         - |  1733 | `/*` |
|         - |  1734 | `` * The receiver a callable VALUE naming a method through a class (`'C::m'`, `['C','m']`)`` |
|         - |  1735 | ` * binds when Closure::fromCallable() turns it into a closure: the same one the syntactic` |
|         - |  1736 | `` * `C::m(...)` binds, so the closure keeps the caller's `$this` after it leaves the method`` |
|         - |  1737 | ` * that made it. 0 for a static method, or when there is no compatible receiver.` |
|         - |  1738 | ` */` |
|        70 |  1739 | `PH7_PRIVATE ph7_class_instance * PH7_VmFccClassReceiver(ph7_vm *pVm,ph7_class *pClass,` |
|         - |  1740 | `	const char *zMeth,sxu32 nMeth)` |
|         1 |  1741 | `{` |
|        71 |  1742 | `	ph7_class_instance *pRecv = 0;` |
|         - |  1743 | `	char zBuf[160];` |
|        71 |  1744 | `	if( pClass == 0 \|\| VmFccMemberError(&(*pVm),pClass,0,0,zMeth,nMeth,TRUE,` |
|        70 |  1745 | `			&pRecv,zBuf,(int)sizeof(zBuf)) != 0 ){` |
|       ! 0 |  1746 | `		return 0;` |
|         - |  1747 | `	}` |
|        71 |  1748 | `	return pRecv;` |
|        36 |  1749 | `}` |
|         - |  1750 | `/*` |
|         - |  1751 | `` * The same resolution for a LITERAL `X::__construct(...)`, which php's compiler turns into a`` |
|         - |  1752 | ` * request for the class's constructor rather than a method lookup -- the call's own rule` |
|         - |  1753 | ` * (vm_ops_oo.c, the static OP_MEMBER): a class with none is "Cannot call constructor" with` |
|         - |  1754 | ``  * no catch-all, the only visibility refusal is a PRIVATE constructor reached with a `$this` `` |
|         - |  1755 | ` * whose class is not the constructor's own, and anything else needs a receiver that is an` |
|         - |  1756 | ` * instance of the class, or it is the non-static Error.` |
|         - |  1757 | ` */` |
|        26 |  1758 | `static const char * VmFccCtorError(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance **ppRecv,` |
|         - |  1759 | `	char *zBuf,int nBuf)` |
|         1 |  1760 | `{` |
|        27 |  1761 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        27 |  1762 | `	ph7_class_instance *pRawThis = PH7_VmCallerThis(&(*pVm));` |
|        27 |  1763 | `	*ppRecv = 0;` |
|        27 |  1764 | `	if( pMethod == 0 ){` |
|         5 |  1765 | `		return "Cannot call constructor";` |
|         - |  1766 | `	}` |
|        22 |  1767 | `	if( pRawThis && pMethod->iProtection == PH7_CLASS_PROT_PRIVATE` |
|        12 |  1768 | `	 && pRawThis->pClass != PH7_VmMethodScopeName(&(*pVm),pClass,pMethod) ){` |
|         5 |  1769 | `		SyBufferFormat(zBuf,nBuf,"Cannot call private %z::__construct()",&pClass->sDisp);` |
|         5 |  1770 | `		return zBuf;` |
|         - |  1771 | `	}` |
|        19 |  1772 | `	*ppRecv = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|        19 |  1773 | `	if( *ppRecv == 0 ){` |
|        10 |  1774 | `		SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|         6 |  1775 | `			&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&pMethod->sFunc.sName);` |
|         7 |  1776 | `		return zBuf;` |
|         - |  1777 | `	}` |
|        13 |  1778 | `	return 0;` |
|        14 |  1779 | `}` |
|         - |  1780 | `/*` |
|         - |  1781 | `` * A static `self::sf(...)` / `parent::sf(...)` is a FORWARDING call in php: it keeps the`` |
|         - |  1782 | `` * caller's late-static-binding class, so `static::` inside the closure (and`` |
|         - |  1783 | ` * getClosureCalledClass) answers that class rather than the one the keyword resolved to. An` |
|         - |  1784 | `` * explicit class name does not forward, and `static::` already resolved to the caller's.`` |
|         - |  1785 | ` * Recorded beside $__scope, which stays the class the callee was resolved in.` |
|         - |  1786 | ` */` |
|       114 |  1787 | `static void VmFccForwardCalled(ph7_vm *pVm,ph7_class_instance *pCloObj,ph7_class *pFccCls,` |
|         - |  1788 | `	ph7_value *pTarget,const SyString *pName)` |
|         5 |  1789 | `{` |
|       119 |  1790 | `	const char *zKw = (const char *)SyBlobData(&pTarget->sBlob);` |
|       119 |  1791 | `	sxu32 nKw = SyBlobLength(&pTarget->sBlob);` |
|         - |  1792 | `	ph7_class_method *pMethod;` |
|         - |  1793 | `	ph7_class *pCalled;` |
|         - |  1794 | `	ph7_value *pAttr;` |
|         - |  1795 | `	SyString sAttr;` |
|       119 |  1796 | `	if( !((nKw == 4 && SyMemcmp(zKw,"self",4) == 0) \|\| (nKw == 6 && SyMemcmp(zKw,"parent",6) == 0)) ){` |
|        98 |  1797 | `		return;` |
|         - |  1798 | `	}` |
|        45 |  1799 | `	pMethod = PH7_ClassExtractMethod(pFccCls,SyStringData(pName),SyStringLength(pName));` |
|        45 |  1800 | `	if( !PH7_VmFccMethodIsDirect(&(*pVm),pFccCls,SyStringData(pName),SyStringLength(pName)) ){` |
|         - |  1801 | `		/* A name only __callStatic answers is php's static trampoline, and it forwards` |
|         - |  1802 | ``		 * like any static method; the __call route on the caller's `$this` does not. */`` |
|        12 |  1803 | `		if( PH7_VmStaticFallbackThis(&(*pVm),pFccCls) != 0 ){` |
|       ! 0 |  1804 | `			return;` |
|         2 |  1805 | `		}` |
|        39 |  1806 | `	}else if( (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|       ! 0 |  1807 | `		return;` |
|         - |  1808 | `	}` |
|        45 |  1809 | `	pCalled = PH7_VmPeekTopClass(&(*pVm));` |
|        45 |  1810 | `	if( pCalled == 0 \|\| pCalled == pFccCls \|\| !PH7_VmInstanceOf(pCalled,pFccCls) ){` |
|        17 |  1811 | `		return;` |
|         - |  1812 | `	}` |
|        29 |  1813 | `	SyStringInitFromBuf(&sAttr,"__called",8);` |
|        29 |  1814 | `	pAttr = PH7_ClassInstanceFetchAttr(pCloObj,&sAttr);` |
|        29 |  1815 | `	if( pAttr ){` |
|        29 |  1816 | `		PH7_MemObjStringAppend(pAttr,SyStringData(&pCalled->sName),SyStringLength(&pCalled->sName));` |
|        14 |  1817 | `	}` |
|        69 |  1818 | `}` |
|         - |  1819 | `/*` |
|         - |  1820 | ` * The same check for the ARRAY form, whose two members carry php's own shape messages` |
|         - |  1821 | ` * before anything is resolved: the target must be an object or a class-name string, the` |
|         - |  1822 | `` * method must be a string. php probes them in that order (`[5,5]` names the FIRST member,`` |
|         - |  1823 | `` * `['NoSuch',5]` the SECOND — the member shape decides before the class is looked up).`` |
|         - |  1824 | ` */` |
|    100252 |  1825 | `static const char * VmDirectArrayCallableError(ph7_vm *pVm,ph7_value *pTarget,ph7_value *pMethod,` |
|         - |  1826 | `	char *zBuf,int nBuf)` |
|         5 |  1827 | `{` |
|         - |  1828 | `	ph7_class *pClass;` |
|    100257 |  1829 | `	if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|        11 |  1830 | `		return "First array member is not a valid class name or object";` |
|         - |  1831 | `	}` |
|    100247 |  1832 | `	if( (pMethod->iFlags & MEMOBJ_STRING) == 0 ){` |
|         9 |  1833 | `		return "Second array member is not a valid method";` |
|         - |  1834 | `	}` |
|    100239 |  1835 | `	pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|    150356 |  1836 | `	return VmCallableClassMethodError(&(*pVm),pClass,` |
|    100234 |  1837 | `		(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|    100234 |  1838 | `		(const char *)SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob),` |
|         - |  1839 | `		/* An OBJECT target carries its own $this; only a class NAME is the static form. */` |
|    100234 |  1840 | `		(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|     50117 |  1841 | `		zBuf,nBuf);` |
|     50131 |  1842 | `}` |
|         - |  1843 | `/*` |
|         - |  1844 | ` * The by-reference SHAPE of the callee an INDIRECT dispatch is about to reach — an` |
|         - |  1845 | `` * array callable VALUE (`$cb = [$o,'m']; $cb($a['k']);`) and an __invoke object.`` |
|         - |  1846 | ` * Both go through a shared helper that hides the target from OP_CALL, so the two` |
|         - |  1847 | ` * sites used to materialize EVERY deferred plain-var argument by reference and every` |
|         - |  1848 | ` * deferred element/property by value: a genuine by-ref out-param into an element was` |
|         - |  1849 | `` * unsupported (`$cb($a['new'])` warned `Undefined array key` and handed the callee a`` |
|         - |  1850 | ` * NULL where php creates the element and writes it), and a by-VALUE parameter` |
|         - |  1851 | `` * swallowed php's `Undefined variable` and CREATED the caller's variable.`` |
|         - |  1852 | ` *` |
|         - |  1853 | ` * The target is knowable here: the pair resolves to a class and a method, an object to` |
|         - |  1854 | ` * its __invoke. Answers 0 when nothing resolves — a name routed through` |
|         - |  1855 | ` * __call/__callStatic (php packs those into an ARRAY, so they are by-value anyway) or` |
|         - |  1856 | ` * a pair the screen above is about to refuse.` |
|         - |  1857 | ` */` |
|    100612 |  1858 | `static ph7_vm_func * VmIndirectCalleeFunc(ph7_vm *pVm,ph7_value *pCallable)` |
|         5 |  1859 | `{` |
|    100617 |  1860 | `	ph7_class_method *pMeth = 0;` |
|    100617 |  1861 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|    100617 |  1862 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|    100617 |  1863 | `		ph7_value *pTarget = 0,*pName = 0;` |
|         - |  1864 | `		ph7_class *pClass;` |
|    100612 |  1865 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|    100612 |  1866 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|    100617 |  1867 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 |  1868 | `			return 0;` |
|         - |  1869 | `		}` |
|    100617 |  1870 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|    100617 |  1871 | `		if( pClass == 0 ){` |
|       ! 0 |  1872 | `			return 0;` |
|         - |  1873 | `		}` |
|    150923 |  1874 | `		pMeth = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|    100612 |  1875 | `			SyBlobLength(&pName->sBlob));` |
|     50306 |  1876 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 |  1877 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|       ! 0 |  1878 | `		if( pThis == 0 ){` |
|       ! 0 |  1879 | `			return 0;` |
|         - |  1880 | `		}` |
|       ! 0 |  1881 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|       ! 0 |  1882 | `	}` |
|    100617 |  1883 | `	return pMeth ? &pMeth->sFunc : 0;` |
|     50311 |  1884 | `}` |
|         - |  1885 | `/*` |
|         - |  1886 | ` * Materialize an indirect dispatch's deferred arguments against that callee — the same` |
|         - |  1887 | ` * split OP_CALL makes for a direct one: a native method's by-ref positions come from its` |
|         - |  1888 | ` * signature mask, a PHP one's from its compiled formals, and an unresolved callee binds` |
|         - |  1889 | ` * everything by value (php's answer for the magic route it is about to take).` |
|         - |  1890 | ` */` |
|    100612 |  1891 | `static sxi32 VmResolveIndirectArgs(ph7_vm *pVm,ph7_value *pCallable,ph7_value *pArg,ph7_value *pTos,` |
|         - |  1892 | `	VmCallArgMap *pCallMap)` |
|         5 |  1893 | `{` |
|    100617 |  1894 | `	ph7_vm_func *pFn = VmIndirectCalleeFunc(&(*pVm),pCallable);` |
|    100617 |  1895 | `	if( pFn == 0 ){` |
|       185 |  1896 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pCallMap);` |
|         - |  1897 | `	}` |
|    100434 |  1898 | `	if( pFn->iFlags & VM_FUNC_NATIVE ){` |
|         9 |  1899 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|         8 |  1900 | `			pFn->pNative ? pFn->pNative->nByRefMask : 0,0,0,pCallMap);` |
|         - |  1901 | `	}` |
|    150637 |  1902 | `	return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|    100422 |  1903 | `		(ph7_vm_func_arg *)SySetBasePtr(&pFn->aArgs),SySetUsed(&pFn->aArgs),0,0,0,pCallMap);` |
|     50311 |  1904 | `}` |
|         - |  1905 | `/*` |
|         - |  1906 | ` * The METHOD a callable value will run, for OP_NAMED_SEND to screen a name against: an` |
|         - |  1907 | `` * array pair, a `"Class::method"` string, an __invoke object, and a Closure bound to a`` |
|         - |  1908 | ` * method -- the shapes whose callee OP_CALL finds through a class rather than by a name` |
|         - |  1909 | ` * the function tables answer. Nothing here changes a thing: OP_CALL_INIT has already` |
|         - |  1910 | ` * refused (and autoloaded for) every one of them php refuses before its arguments.` |
|         - |  1911 | ` *` |
|         - |  1912 | ` * Answers 0 whenever the call will not land on that method as found -- a name only` |
|         - |  1913 | ` * __call/__callStatic answers (php packs those into an array), one the calling scope` |
|         - |  1914 | ` * cannot reach (OP_CALL's own refusal, or the catch-all again), an abstract one, a` |
|         - |  1915 | `` * `parent::m` spelled inside a pair -- and for a Closure whose `$__fn` is a FUNCTION,`` |
|         - |  1916 | ` * which the plain-closure screen reads instead.` |
|         - |  1917 | ` */` |
|    114862 |  1918 | `static ph7_vm_func * VmNamedSendMethod(ph7_vm *pVm,ph7_value *pCallee)` |
|         5 |  1919 | `{` |
|    114867 |  1920 | `	ph7_class *pClass = 0;` |
|         - |  1921 | `	ph7_class_method *pMeth;` |
|    114867 |  1922 | `	const char *zMeth = 0;` |
|    114867 |  1923 | `	sxu32 nMeth = 0;` |
|    114867 |  1924 | `	int bScreened = 0;` |
|    114867 |  1925 | `	if( pCallee->iFlags & MEMOBJ_HASHMAP ){` |
|        28 |  1926 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallee->x.pOther;` |
|        28 |  1927 | `		ph7_value *pTarget = 0,*pName = 0;` |
|        26 |  1928 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|        26 |  1929 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|        28 |  1930 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 |  1931 | `			return 0;` |
|         - |  1932 | `		}` |
|        28 |  1933 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|        28 |  1934 | `		zMeth = (const char *)SyBlobData(&pName->sBlob);` |
|        28 |  1935 | `		nMeth = SyBlobLength(&pName->sBlob);` |
|    114854 |  1936 | `	}else if( pCallee->iFlags & MEMOBJ_STRING ){` |
|    114713 |  1937 | `		const char *zCls = 0;` |
|    114713 |  1938 | `		sxu32 nCls = 0;` |
|         - |  1939 | `		SyString sName;` |
|    114713 |  1940 | `		if( pCallee->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_ENGINEFN) ){` |
|     57557 |  1941 | `			return 0; /* the engine's own key, which the function tables answer */` |
|         - |  1942 | `		}` |
|    114445 |  1943 | `		SyStringInitFromBuf(&sName,SyBlobData(&pCallee->sBlob),SyBlobLength(&pCallee->sBlob));` |
|    114445 |  1944 | `		if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|         5 |  1945 | `			sName.zString++;` |
|         5 |  1946 | `			sName.nByte--;` |
|         2 |  1947 | `		}` |
|    114445 |  1948 | `		if( !PH7_VmCallableStringParts(sName.zString,sName.nByte,&zCls,&nCls,&zMeth,&nMeth) ){` |
|    114433 |  1949 | `			return 0;` |
|         - |  1950 | `		}` |
|        13 |  1951 | `		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       137 |  1952 | `	}else if( VmValueIsClosure(pVm,pCallee) ){` |
|         - |  1953 | `		/* VmClosureUnwrap's own split, read without the latches it arms for a dispatch: a` |
|         - |  1954 | `		 * bound or static Closure calls a METHOD when it was built from one or its class` |
|         - |  1955 | ``		 * answers `$__fn`, and a method Closure's `$__scope` names the class its callee was`` |
|         - |  1956 | ``		 * resolved in (`parent::m(...)`). Its visibility was decided where it was built. */`` |
|       115 |  1957 | `		ph7_class_instance *pClo = (ph7_class_instance *)pCallee->x.pOther;` |
|         - |  1958 | `		ph7_value *pFn,*pBound,*pScope;` |
|       115 |  1959 | `		ph7_class *pScopeCls = 0;` |
|         - |  1960 | `		SyString sAttr;` |
|       115 |  1961 | `		if( (pClo->iFlags & VM_INSTANCE_FCC_BOUND) == 0 ){` |
|        47 |  1962 | `			return 0;` |
|         - |  1963 | `		}` |
|        72 |  1964 | `		SyStringInitFromBuf(&sAttr,"__fn",4);` |
|        72 |  1965 | `		pFn = PH7_ClassInstanceFetchAttr(pClo,&sAttr);` |
|        72 |  1966 | `		SyStringInitFromBuf(&sAttr,"__this",6);` |
|        72 |  1967 | `		pBound = PH7_ClassInstanceFetchAttr(pClo,&sAttr);` |
|        72 |  1968 | `		SyStringInitFromBuf(&sAttr,"__scope",7);` |
|        72 |  1969 | `		pScope = PH7_ClassInstanceFetchAttr(pClo,&sAttr);` |
|        72 |  1970 | `		if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|       ! 0 |  1971 | `			return 0;` |
|         - |  1972 | `		}` |
|        72 |  1973 | `		zMeth = (const char *)SyBlobData(&pFn->sBlob);` |
|        72 |  1974 | `		nMeth = SyBlobLength(&pFn->sBlob);` |
|        72 |  1975 | `		if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|       104 |  1976 | `			pScopeCls = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pScope->sBlob),` |
|        34 |  1977 | `				SyBlobLength(&pScope->sBlob),FALSE,0);` |
|        34 |  1978 | `		}` |
|        72 |  1979 | `		if( pBound && (pBound->iFlags & MEMOBJ_OBJ) ){` |
|        48 |  1980 | `			pClass = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|        46 |  1981 | `			if( (pClo->iFlags & VM_INSTANCE_FCC_METHOD) && pScopeCls && pScopeCls != pClass` |
|        24 |  1982 | `			 && PH7_VmInstanceOf(pClass,pScopeCls) && PH7_ClassExtractMethod(pScopeCls,zMeth,nMeth) ){` |
|         8 |  1983 | `				pClass = pScopeCls;` |
|         3 |  1984 | `			}` |
|        25 |  1985 | `		}else{` |
|        26 |  1986 | `			pClass = pScopeCls;` |
|         - |  1987 | `		}` |
|        70 |  1988 | `		if( pClass && (pClo->iFlags & VM_INSTANCE_FCC_METHOD) == 0` |
|        42 |  1989 | `		 && PH7_ClassExtractMethod(pClass,zMeth,nMeth) == 0 ){` |
|         5 |  1990 | ``			return 0; /* a bound plain closure: `$__fn` is a function */`` |
|         - |  1991 | `		}` |
|        68 |  1992 | `		bScreened = 1;` |
|        51 |  1993 | `	}else if( pCallee->iFlags & MEMOBJ_OBJ ){` |
|         - |  1994 | ``		/* `$o(...)` dispatches __invoke from any scope, whatever its visibility. */`` |
|        18 |  1995 | `		ph7_class_instance *pObj = (ph7_class_instance *)pCallee->x.pOther;` |
|        18 |  1996 | `		pMeth = pObj ? PH7_ClassExtractMethod(pObj->pClass,"__invoke",sizeof("__invoke")-1) : 0;` |
|        18 |  1997 | `		return pMeth ? &pMeth->sFunc : 0;` |
|         - |  1998 | `	}` |
|       106 |  1999 | `	if( pClass == 0 \|\| nMeth < 1 ){` |
|       ! 0 |  2000 | `		return 0;` |
|         - |  2001 | `	}` |
|       106 |  2002 | `	pMeth = PH7_ClassExtractMethod(pClass,zMeth,nMeth);` |
|       104 |  2003 | `	if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|        68 |  2004 | `	 \|\| (!bScreened && !PH7_VmFccMethodIsDirect(&(*pVm),pClass,zMeth,nMeth)) ){` |
|        40 |  2005 | `		return 0;` |
|         - |  2006 | `	}` |
|        68 |  2007 | `	return &pMeth->sFunc;` |
|     57366 |  2008 | `}` |
|         - |  2009 | `/*` |
|         - |  2010 | `` * Why a VALUE cannot be made into a first-class callable. php answers `($v)(...)` with`` |
|         - |  2011 | `` * exactly what it answers `($v)()` — the taxonomy is the DIRECT dispatch's, word for word —`` |
|         - |  2012 | ` * so this walks the same three shapes the OP_CALL sites do and reuses their builders. PHL` |
|         - |  2013 | `` * left a non-callable value STANDING instead: `$x = 5; $f = ($x)(...);` evaluated to int(5),`` |
|         - |  2014 | ` * an array to the array, a misspelled function name to its own string — a value that is not` |
|         - |  2015 | ` * a Closure where php throws, silently, on every shape.` |
|         - |  2016 | ` *` |
|         - |  2017 | ` * Returns 0 when the value IS callable (unreachable through the FCC caller, which asks only` |
|         - |  2018 | ` * after the wrap declined, but it keeps the helper honest for a direct reader).` |
|         - |  2019 | ` */` |
|       106 |  2020 | `static const char * VmFccValueError(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|         4 |  2021 | `{` |
|       110 |  2022 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        28 |  2023 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        28 |  2024 | `		ph7_value *pTarget = 0,*pMeth = 0;` |
|         - |  2025 | `		const char *zWhy;` |
|        28 |  2026 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|         9 |  2027 | `			return "Array callback must have exactly two elements";` |
|         - |  2028 | `		}` |
|        20 |  2029 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pMeth) ){` |
|         3 |  2030 | `			return "Array callback has to contain indices 0 and 1";` |
|         - |  2031 | `		}` |
|        18 |  2032 | `		zWhy = VmDirectArrayCallableError(&(*pVm),pTarget,pMeth,zBuf,nBuf);` |
|        18 |  2033 | `		if( zWhy ){` |
|         9 |  2034 | `			return zWhy;` |
|         - |  2035 | `		}` |
|         - |  2036 | `		/* That check deliberately leaves VISIBILITY to OP_CALL's own screen, which raises it` |
|         - |  2037 | `		 * when the pair is finally called — and a first-class callable never gets there: php` |
|         - |  2038 | ``		 * refuses `[$o,'priv'](...)` at the creation, with the direct dispatch's wording.`` |
|         - |  2039 | `		 * Reached only for a pair PH7_VmIsCallable already declined, so a class routing the` |
|         - |  2040 | `		 * name through __call (which makes it callable) cannot arrive here. */` |
|        10 |  2041 | `		if( (pMeth->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMeth->sBlob) > 0 ){` |
|        10 |  2042 | `			ph7_class *pCbCls = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|        10 |  2043 | `			const char *zM = (const char *)SyBlobData(&pMeth->sBlob);` |
|        10 |  2044 | `			sxu32 nM = SyBlobLength(&pMeth->sBlob);` |
|        10 |  2045 | `			ph7_class_method *pCbMeth = pCbCls ? PH7_ClassExtractMethod(pCbCls,zM,nM) : 0;` |
|         8 |  2046 | `			if( pCbMeth && pCbMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|        10 |  2047 | `			 && !PH7_VmFccMethodIsDirect(&(*pVm),pCbCls,zM,nM) ){` |
|        14 |  2048 | `				return VmMethodVisibilityMsg(&(*pVm),` |
|         4 |  2049 | `					PH7_VmMethodScopeName(&(*pVm),pCbCls,pCbMeth),` |
|         4 |  2050 | `					zM,nM,pCbMeth->iProtection,zBuf,nBuf);` |
|         - |  2051 | `			}` |
|       ! 0 |  2052 | `		}` |
|       ! 0 |  2053 | `		return 0;` |
|         - |  2054 | `	}` |
|        84 |  2055 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        19 |  2056 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|        19 |  2057 | `		if( pObj == 0 ){` |
|       ! 0 |  2058 | `			return "Value of type object is not callable";` |
|         - |  2059 | `		}` |
|        19 |  2060 | `		if( PH7_ClassExtractMethod(pObj->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       ! 0 |  2061 | `			return 0;` |
|         - |  2062 | `		}` |
|        19 |  2063 | `		SyBufferFormat(zBuf,nBuf,"Object of type %z is not callable",&pObj->pClass->sDisp);` |
|        19 |  2064 | `		return zBuf;` |
|         - |  2065 | `	}` |
|        66 |  2066 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|        52 |  2067 | `		const char *zCls = 0,*zMeth = 0;` |
|        52 |  2068 | `		sxu32 nCls = 0,nMeth = 0;` |
|         - |  2069 | `		SyString sName;` |
|        52 |  2070 | `		SyStringInitFromBuf(&sName,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|         - |  2071 | `		/* A leading backslash only anchors the name to the global namespace. */` |
|        52 |  2072 | `		if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|       ! 0 |  2073 | `			sName.zString++;` |
|       ! 0 |  2074 | `			sName.nByte--;` |
|       ! 0 |  2075 | `		}` |
|        52 |  2076 | `		if( PH7_VmCallableStringParts(sName.zString,sName.nByte,&zCls,&nCls,&zMeth,&nMeth) ){` |
|         - |  2077 | `			/* "Class::method" carries the class/method taxonomy, not the function one. */` |
|         7 |  2078 | `			return VmCallableClassMethodError(&(*pVm),` |
|         2 |  2079 | `				PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0),` |
|         2 |  2080 | `				zCls,nCls,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|         - |  2081 | `		}` |
|        48 |  2082 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined function %z()",&sName);` |
|        48 |  2083 | `		return zBuf;` |
|         - |  2084 | `	}` |
|        16 |  2085 | `	SyBufferFormat(zBuf,nBuf,"Value of type %s is not callable",VmArithTypeName(pValue));` |
|        16 |  2086 | `	return zBuf;` |
|        57 |  2087 | `}` |
|         - |  2088 | `/*` |
|         - |  2089 | `` * Split a `"Class::method"` callable string. php scans for the LAST "::" (zend_memrchr),`` |
|         - |  2090 | `` * so `"C::s::x"` names the class `C::s` — not `C` — and reports it not found. Returns TRUE`` |
|         - |  2091 | `` * and the two halves (either may be empty: `"C::"` and `"::s"` are shapes php accepts here`` |
|         - |  2092 | ` * and rejects further down), FALSE when the string carries no "::" at all.` |
|         - |  2093 | ` */` |
|   2187912 |  2094 | `PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,` |
|         - |  2095 | `	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth)` |
|         5 |  2096 | `{` |
|         - |  2097 | `	sxu32 i;` |
|  14583116 |  2098 | `	for( i = nName ; i >= 2 ; --i ){` |
|  12596232 |  2099 | `		if( zName[i-2] == ':' && zName[i-1] == ':' ){` |
|    201033 |  2100 | `			*pzCls = zName;` |
|    201033 |  2101 | `			*pnCls = i - 2;` |
|    201033 |  2102 | `			*pzMeth = &zName[i];` |
|    201033 |  2103 | `			*pnMeth = nName - i;` |
|    201033 |  2104 | `			return TRUE;` |
|         - |  2105 | `		}` |
|   6193170 |  2106 | `	}` |
|   1986889 |  2107 | `	return FALSE;` |
|   1093321 |  2108 | `}` |
|         - |  2109 | `/*` |
|         - |  2110 | ` * php 8.4's FRAMELESS functions, and the arities their stubs list (a bit per argument` |
|         - |  2111 | ` * count, read back from the oracle's own opcode dump): a direct call of one of these at a` |
|         - |  2112 | ` * listed arity pushes no frame of its own. Asked by the call dispatch and by the compiler,` |
|         - |  2113 | ` * which needs the same answer for the argument list of an unqualified namespaced call.` |
|         - |  2114 | ` */` |
|  17401893 |  2115 | `PH7_PRIVATE int PH7_VmFramelessArity(const SyString *pName,int nArgs)` |
|         5 |  2116 | `{` |
|         - |  2117 | `	static const struct { const char *zName; int nLen; int mArity; } aFrameless[] = {` |
|         - |  2118 | `		{ "implode",         sizeof("implode")-1,         0x6 },` |
|         - |  2119 | `		{ "in_array",        sizeof("in_array")-1,        0xC },` |
|         - |  2120 | `		{ "str_replace",     sizeof("str_replace")-1,     0x8 },` |
|         - |  2121 | `		{ "strtr",           sizeof("strtr")-1,           0xC },` |
|         - |  2122 | `		{ "trim",            sizeof("trim")-1,            0x6 },` |
|         - |  2123 | `		{ "substr",          sizeof("substr")-1,          0xC },` |
|         - |  2124 | `		{ "strpos",          sizeof("strpos")-1,          0xC },` |
|         - |  2125 | `		{ "strstr",          sizeof("strstr")-1,          0xC },` |
|         - |  2126 | `		{ "str_contains",    sizeof("str_contains")-1,    0x4 },` |
|         - |  2127 | `		{ "str_starts_with", sizeof("str_starts_with")-1, 0x4 },` |
|         - |  2128 | `		{ "dirname",         sizeof("dirname")-1,         0x6 },` |
|         - |  2129 | `		{ "preg_match",      sizeof("preg_match")-1,      0x4 },` |
|         - |  2130 | `		{ "preg_replace",    sizeof("preg_replace")-1,    0x8 },` |
|         - |  2131 | `		{ "min",             sizeof("min")-1,             0x4 },` |
|         - |  2132 | `		{ "max",             sizeof("max")-1,             0x4 },` |
|         - |  2133 | `		{ "dechex",          sizeof("dechex")-1,          0x2 },` |
|         - |  2134 | `		{ "is_numeric",      sizeof("is_numeric")-1,      0x2 },` |
|         - |  2135 | `		{ "class_exists",    sizeof("class_exists")-1,    0x6 },` |
|         - |  2136 | `		{ "property_exists", sizeof("property_exists")-1, 0x4 },` |
|         - |  2137 | `	};` |
|         - |  2138 | `	sxu32 iF;` |
|  17401898 |  2139 | `	if( nArgs < 1 \|\| nArgs > 3 ){` |
|     28565 |  2140 | `		return 0;` |
|         - |  2141 | `	}` |
| 330202284 |  2142 | `	for( iF = 0 ; iF < SX_ARRAYSIZE(aFrameless) ; ++iF ){` |
| 314015873 |  2143 | `		if( (int)pName->nByte == aFrameless[iF].nLen` |
| 184594887 |  2144 | `		 && (aFrameless[iF].mArity & (1 << nArgs)) != 0` |
|  42299776 |  2145 | `		 && SyStrnicmp(pName->zString,aFrameless[iF].zName,(sxu32)aFrameless[iF].nLen) == 0 ){` |
|   1186932 |  2146 | `			return 1;` |
|         - |  2147 | `		}` |
| 156417953 |  2148 | `	}` |
|  16186411 |  2149 | `	return 0;` |
|   8701338 |  2150 | `}` |
|         - |  2151 | `/*` |
|         - |  2152 | `` * php 8.4's compiler rewrites `sprintf()` into string concatenation when it can read the`` |
|         - |  2153 | `` * format: a string LITERAL under 256 bytes whose every `%` is `%s`, `%d` or `%%`, with`` |
|         - |  2154 | ` * exactly one argument per placeholder. That call is no call at all -- a __toString it` |
|         - |  2155 | ` * reaches, or a conversion warning it raises, has the caller's frame under it and no` |
|         - |  2156 | `` * `sprintf` one. The format's literal-ness is the compiler's to say (nConstStrMask).`` |
|         - |  2157 | ` */` |
|      3158 |  2158 | `static int VmSprintfFolds(const VmInstr *pInstr,const ph7_value *pArg,int nCallArgs)` |
|         4 |  2159 | `{` |
|      3162 |  2160 | `	const VmCallArgMap *pMap = (const VmCallArgMap *)pInstr->p3;` |
|         - |  2161 | `	const char *zIn,*zEnd;` |
|      3162 |  2162 | `	int nHole = 0;` |
|      3158 |  2163 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| (pMap->nConstStrMask & 1) == 0` |
|      2816 |  2164 | `	 \|\| (pArg[0].iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pArg[0].sBlob) >= 256 ){` |
|       696 |  2165 | `		return 0;` |
|         - |  2166 | `	}` |
|      2470 |  2167 | `	zIn = (const char *)SyBlobData(&pArg[0].sBlob);` |
|      2470 |  2168 | `	zEnd = &zIn[SyBlobLength(&pArg[0].sBlob)];` |
|     10410 |  2169 | `	for( ; zIn < zEnd ; zIn++ ){` |
|      9880 |  2170 | `		if( zIn[0] != '%' ){` |
|      6286 |  2171 | `			continue;` |
|         - |  2172 | `		}` |
|      3598 |  2173 | `		if( ++zIn >= zEnd ){` |
|       ! 0 |  2174 | `			return 0;` |
|         - |  2175 | `		}` |
|      3598 |  2176 | `		if( zIn[0] == 's' \|\| zIn[0] == 'd' ){` |
|      1652 |  2177 | `			nHole++;` |
|      2768 |  2178 | `		}else if( zIn[0] != '%' ){` |
|      1940 |  2179 | `			return 0;` |
|         - |  2180 | `		}` |
|       827 |  2181 | `	}` |
|       534 |  2182 | `	return nHole == nCallArgs - 1;` |
|      1580 |  2183 | `}` |
|         - |  2184 | `/*` |
|         - |  2185 | ` * A parameter DEFAULT is a mini-program run in the callee's frame before the body` |
|         - |  2186 | `` * starts, and `self::K` inside one means the class that DECLARED the method -- for`` |
|         - |  2187 | ` * a method composed from a trait, the class that composed it. That answer is not` |
|         - |  2188 | ` * reachable by the ordinary route here: the call's self stack is not pushed until` |
|         - |  2189 | `` * the body begins, so the trait rule has nothing to walk from and `self` was left`` |
|         - |  2190 | `` * unresolved -- read as a class of that name, and thrown as `Class "self" not`` |
|         - |  2191 | `` * found` for a default php evaluates without a word.`` |
|         - |  2192 | ` *` |
|         - |  2193 | ` * Mark the declaring class explicitly for the duration, the way a member` |
|         - |  2194 | ` * initializer's evaluation does (PH7_VmPeekDeclaringClass reads the frame-keyed` |
|         - |  2195 | ` * pair). A non-trait method resolves to the class it already did, so nothing else` |
|         - |  2196 | ` * moves. The pair is saved and restored around each default because one default` |
|         - |  2197 | ` * may CALL something that evaluates defaults of its own.` |
|         - |  2198 | ` */` |
|         - |  2199 | `typedef struct VmDefaultScope VmDefaultScope;` |
|         - |  2200 | `struct VmDefaultScope { ph7_class *pClass; void *pFrame; };` |
|     18887 |  2201 | `static void VmDefaultScopeEnter(ph7_vm *pVm,VmFrame *pFrame,ph7_vm_func *pVmFunc,` |
|         - |  2202 | `	ph7_class *pSelf,VmDefaultScope *pSave)` |
|         5 |  2203 | `{` |
|     18892 |  2204 | `	pSave->pClass = pVm->pConstEvalClass;` |
|     18892 |  2205 | `	pSave->pFrame = pVm->pConstEvalFrame;` |
|     18892 |  2206 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pVmFunc->pUserData ){` |
|       476 |  2207 | `		pVm->pConstEvalClass = PH7_VmMemberOwnerClass((ph7_class *)pVmFunc->pUserData,` |
|       157 |  2208 | `			pSelf ? pSelf : (ph7_class *)pVmFunc->pUserData);` |
|       319 |  2209 | `		pVm->pConstEvalFrame = (void *)pFrame;` |
|       157 |  2210 | `	}` |
|     18892 |  2211 | `}` |
|     18887 |  2212 | `static void VmDefaultScopeLeave(ph7_vm *pVm,VmDefaultScope *pSave)` |
|         5 |  2213 | `{` |
|     18892 |  2214 | `	pVm->pConstEvalClass = pSave->pClass;` |
|     18892 |  2215 | `	pVm->pConstEvalFrame = pSave->pFrame;` |
|     18892 |  2216 | `}` |
|   4594401 |  2217 | `static sxi32 VmByteCodeExecBody(` |
|         - |  2218 | `	ph7_vm *pVm,         /* Target VM */` |
|         - |  2219 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|         - |  2220 | `	ph7_value *pStack,   /* Operand stack */` |
|         - |  2221 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|         - |  2222 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|         - |  2223 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|         - |  2224 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|         - |  2225 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|         - |  2226 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|         - |  2227 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|         - |  2228 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|         - |  2229 | `	ph7_value **ppBaseOwner, /* Base (pCallTop==0) operand-stack owner slot the native entry frees; OP_SPREAD growth of the base stack writes the new pointer here (see VmGrowOperandStack). */` |
|         - |  2230 | `	sxu32 *pnBaseCap, /* Base stack capacity (persisted across coroutine suspend/resume); read for the initial capacity and updated on base-stack growth. */` |
|         - |  2231 | `	sxu32 nStackOrig /* Base stack's ORIGINAL (ungrown) allocation size — the fixed headroom reference for OP_SPREAD growth (see nStackOrig in VmExecState). */` |
|         - |  2232 | `	)` |
|         5 |  2233 | `{` |
|         - |  2234 | `	VmInstr *pInstr;` |
|         - |  2235 | `	ph7_value *pTos;` |
|         - |  2236 | `	/* The activation's operand-stack WATERMARK: the deepest pTos has been at any` |
|         - |  2237 | `	 * instruction boundary. A local for the same reason pTos and pc are -- it is` |
|         - |  2238 | `	 * touched once per instruction -- and synced into sState at the same boundaries.` |
|         - |  2239 | `	 * Its whole purpose is the teardown sweep: nothing above it was ever written, so` |
|         - |  2240 | `	 * VmOperandStackRecycle walks to it instead of walking the whole buffer. */` |
|         - |  2241 | `	ph7_value *pHigh;` |
|         - |  2242 | `	SySet aArg;` |
|   4594406 |  2243 | `	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record` |
|         - |  2244 | `	                            * stack (BYTECODE stage 2); NULL = executing the` |
|         - |  2245 | `	                            * bottom activation. */` |
|         - |  2246 | `	VmExecState sState; /* This activation's boundary state:` |
|         - |  2247 | `	                     * everything a suspended/nested activation must restore.` |
|         - |  2248 | `	                     * pc/pTos stay in locals for the hot loop and are synced` |
|         - |  2249 | `	                     * into sState only around the call epilogue (stage 2 turns` |
|         - |  2250 | `	                     * that boundary into an explicit record push/pop). */` |
|         - |  2251 | `	sxi32 pc;` |
|         - |  2252 | `	sxi32 rc;` |
|   4594406 |  2253 | `	int bCallInitStamp = 0; /* PH7_OP_CALL_INIT: may this site's verdict be remembered? */` |
|   4594406 |  2254 | `	sState.aInstr = aInstr;` |
|   4594406 |  2255 | `	sState.pStack = pStack;` |
|   4594406 |  2256 | `	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */` |
|   4594406 |  2257 | `	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */` |
|   4594406 |  2258 | `	sState.pResult = pResult;` |
|   4594406 |  2259 | `	sState.pLastRef = pLastRef;` |
|   4594406 |  2260 | `	sState.pEnforceRetFunc = pEnforceRetFunc;` |
|   4594406 |  2261 | `	sState.is_callback = (sxu8)(is_callback ? 1 : 0);` |
|   4594406 |  2262 | `	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);` |
|         - |  2263 | `	/* Argument container */` |
|   4594406 |  2264 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|   4594406 |  2265 | `	if( nTos < 0 ){` |
|   1560250 |  2266 | `		pTos = &pStack[-1];` |
|    780057 |  2267 | `	}else{` |
|   3034161 |  2268 | `		pTos = &pStack[nTos];` |
|         - |  2269 | `	}` |
|   4594406 |  2270 | `	sState.pTos = pTos;` |
|   4594406 |  2271 | `	pHigh = pTos;` |
|   4594406 |  2272 | `	sState.pHigh = pHigh;` |
|   4594406 |  2273 | `	sState.pc = nPc;` |
|         - |  2274 | `	/* Finally-drain base. For a resumed generator/fiber TOP-LEVEL body, its own` |
|         - |  2275 | `	 * exception handlers were just re-published above the caller depth` |
|         - |  2276 | `	 * (VmRestoreCtxState), so the live SySetUsed over-counts; take the` |
|         - |  2277 | `	 * caller-depth base recorded on the ctx instead.` |
|         - |  2278 | `	 *` |
|         - |  2279 | `	 * The discriminator is the OPERAND STACK, not the frame: the body exec runs on` |
|         - |  2280 | `	 * the ctx's own pStack, whereas every nested frame-less mini-program run within` |
|         - |  2281 | `	 * it — a default-argument / constructor trampoline, or a catch/finally body via` |
|         - |  2282 | `	 * VmLocalExec — runs on a FRESH operand stack while SHARING pVm->pFrame (those` |
|         - |  2283 | `	 * mini-programs do not push a VM frame). A pFrame-only guard therefore misfires` |
|         - |  2284 | `	 * for such a mini-program and hands it the generator's low caller-base; its` |
|         - |  2285 | `	 * terminal OP_DONE then drains VmDrainFinally down to that base, tearing down a` |
|         - |  2286 | ``	 * live try that the surrounding finally had just opened (e.g. `finally { try {`` |
|         - |  2287 | ``	 * throw new E(); } catch (E) {} }` in a generator: the `new E()` trampoline's`` |
|         - |  2288 | `	 * OP_DONE popped the inner try before its OP_THROW ran, so the throw escaped` |
|         - |  2289 | `	 * uncaught). Requiring pStack == pCtx->pStack pins the override to the resumed` |
|         - |  2290 | `	 * body itself; nested mini-programs fall through to the correct live depth. */` |
|   4594401 |  2291 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame` |
|      3605 |  2292 | `	 && pStack == pVm->pActiveCtx->pStack ){` |
|      2883 |  2293 | `		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;` |
|      2883 |  2294 | `		sState.nFinallyActBase = pVm->pActiveCtx->nFinallyBase;` |
|      1444 |  2295 | `	}else{` |
|   4591528 |  2296 | `		sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|   4591528 |  2297 | `		sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|         - |  2298 | `	}` |
|   4594406 |  2299 | `	sState.pEntryFrame = pVm->pFrame;` |
|   4594406 |  2300 | `	pc = nPc;` |
|         - |  2301 | `	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call` |
|         - |  2302 | `	 * adopts its parked record segment here. VmResumeCtx hands the segment in` |
|         - |  2303 | `	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a` |
|         - |  2304 | `	 * nested mini-program/callback run inside the resumed body — after it rebased` |
|         - |  2305 | `	 * the segment's exception floors and pushed the resume value into the innermost` |
|         - |  2306 | `	 * stack. Locals switch to the innermost activation so the dispatch loop` |
|         - |  2307 | `	 * continues inside the callee; the record chain is restored so its completion` |
|         - |  2308 | `	 * unwinds back through the body. */` |
|   4594406 |  2309 | `	if( pAdoptSegment ){` |
|       ! 0 |  2310 | `		VmParkedSegment *pSeg = pAdoptSegment;` |
|       ! 0 |  2311 | `		pCallTop = pSeg->pCallTop;` |
|       ! 0 |  2312 | `		sState = pSeg->sState;` |
|       ! 0 |  2313 | `		aInstr = sState.aInstr;` |
|       ! 0 |  2314 | `		pStack = sState.pStack;` |
|         - |  2315 | `		/* pc is already nPc (== pCtx->pc, the innermost's post-suspend pc) from the` |
|         - |  2316 | `		 * init above; only the stack/top move to the innermost activation. */` |
|       ! 0 |  2317 | `		pTos = &pStack[nTos];   /* nTos == pCtx->nTos — innermost, resume value pushed */` |
|         - |  2318 | `		/* The parked state carries the innermost activation's watermark; the resume` |
|         - |  2319 | `		 * value was pushed above it, so take whichever is higher. */` |
|       ! 0 |  2320 | `		pHigh = ( sState.pHigh > pTos ) ? sState.pHigh : pTos;` |
|       ! 0 |  2321 | `		SyMemBackendFree(&pVm->sAllocator,pSeg); /* holder only; its contents are now live */` |
|       ! 0 |  2322 | `	}` |
|         - |  2323 | `/*` |
|         - |  2324 | ` * Route an enforcement helper's (or yield-from delegate's) return code from inside` |
|         - |  2325 | ` * the main switch: proceed on SXRET_OK, abort on PH7_ABORT, and on PH7_EXCEPTION` |
|         - |  2326 | ` * resume at the landing pad of the body that actually caught the exception in place` |
|         - |  2327 | ` * (VmRecordedResume) or, when it was caught by an outer exec, unwind out of the VM` |
|         - |  2328 | ` * loop. Replaces the old "jump to pVm->pFrame's nearest iExceptionJump" which ran` |
|         - |  2329 | ` * the statement after the try even when the catch was at an enclosing frame (ROOT B,` |
|         - |  2330 | `` * face b — `yield from` over a throwing sub-generator).`` |
|         - |  2331 | ` */` |
|         - |  2332 | `/* Dispatch-routing macros: bodies live in vm_dispatch.h, written against the` |
|         - |  2333 | ` * VM_EXIT_* primitives. Here they bind to the loop's own control flow. */` |
|         - |  2334 | `#define VM_EXIT_BREAK break` |
|         - |  2335 | `#define VM_EXIT_ABORT goto Abort` |
|         - |  2336 | `#define VM_EXIT_EXCEPTION goto Exception` |
|         - |  2337 | `#include "vm_dispatch.h"` |
|         - |  2338 | `	/* Generator::throw() inject-at-yield: when this invocation is a resumed generator/fiber` |
|         - |  2339 | `	 * body carrying a pending injected exception, raise it HERE — once, before the dispatch` |
|         - |  2340 | `	 * loop (pc is already at the resume point) — so the existing OP_THROW route` |
|         - |  2341 | `	 * (VmThrowException + VmRecordedResume) lands it at the generator's own try/catch landing` |
|         - |  2342 | `	 * pad WITHOUT reconstructing the suspended exception frame on pVm->pFrame. Because` |
|         - |  2343 | `	 * pVm->pFrame stays the body, the return/finally/sRet subsystem is untouched (this is why` |
|         - |  2344 | `	 * the reverted frame-reconstruction approach's regression cannot recur). Behaviorally` |
|         - |  2345 | ``	 * identical to a `throw` executed at the yield point: if the generator's own try catches`` |
|         - |  2346 | `	 * it we resume after the try; otherwise it propagates to the throw() caller (ROOT B lands` |
|         - |  2347 | `	 * the caller's handler) and the ctx closes. pInjected is one-shot and only meaningful at` |
|         - |  2348 | `	 * the resume pc, so this is checked once at entry — NOT per-instruction — keeping the hot` |
|         - |  2349 | `	 * dispatch loop untouched for all normal code. The pFrame gate keeps a nested call / catch` |
|         - |  2350 | `	 * mini-program sharing the ctx (a separate VmByteCodeExec entry) from re-firing.` |
|         - |  2351 | `	 *` |
|         - |  2352 | ``	 * Exception: when this body is suspended mid `yield from` over an inner Generator`` |
|         - |  2353 | `	 * (iDelegateState==3), a Generator::throw() on the OUTER generator must be forwarded` |
|         - |  2354 | `	 * INTO the delegate (PHP yield-from transparency), not raised here. Leave pInjected` |
|         - |  2355 | `	 * set and skip; the resume pc is that OP_YIELD_FROM, which consumes and forwards it. */` |
|         - |  2356 | `	/* pAdoptSegment says the same thing for a DEEP suspend: this invocation IS the` |
|         - |  2357 | `	 * resume, but the adopt above moved sState to the innermost parked activation,` |
|         - |  2358 | `	 * so its entry frame is that callee's and no longer the body's. Fiber::throw()` |
|         - |  2359 | `	 * on a fiber suspended inside a nested call has to raise THERE -- at the` |
|         - |  2360 | ``	 * `Fiber::suspend()` the callee is parked on -- which is where pVm->pFrame and`` |
|         - |  2361 | `	 * the adopted aInstr already point. */` |
|   4594401 |  2362 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected` |
|      1892 |  2363 | `	 && (pAdoptSegment != 0 \|\| pVm->pActiveCtx->pFrame == sState.pEntryFrame)` |
|        65 |  2364 | `	 && pVm->pActiveCtx->iDelegateState != 3 ){` |
|        60 |  2365 | `		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;` |
|         - |  2366 | `		VmFrame *pThrowFrame;` |
|         - |  2367 | `		sxi32 iResumePc;` |
|        60 |  2368 | `		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */` |
|         - |  2369 | `		/* Raising the injection at the yield-from point abandons any array/Iterator` |
|         - |  2370 | `		 * delegation in progress (state 1/2 — state 3 was forwarded, not raised here).` |
|         - |  2371 | `		 * Tear the delegate down so that if the generator's own try catches this and` |
|         - |  2372 | ``		 * runs on to a LATER `yield from`, that opcode classifies its operand fresh`` |
|         - |  2373 | `		 * instead of resuming this now-stale delegate cursor. */` |
|        60 |  2374 | `		if( pVm->pActiveCtx->iDelegateState != 0 ){` |
|         3 |  2375 | `			PH7_MemObjRelease(&pVm->pActiveCtx->sDelegate);` |
|         3 |  2376 | `			pVm->pActiveCtx->pDelegateNode = 0;` |
|         3 |  2377 | `			pVm->pActiveCtx->iDelegateState = 0;` |
|         1 |  2378 | `		}` |
|        60 |  2379 | `		pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|        60 |  2380 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        60 |  2381 | `		rc = VmThrowException(&(*pVm),pInj);` |
|        60 |  2382 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2383 | `			goto Abort;` |
|         - |  2384 | `		}` |
|        60 |  2385 | `		if( VmInlineOwnedBy(pVm,aInstr,sState.pEntryFrame) ){` |
|         - |  2386 | `			/* ROOT C: the inject was caught by an inline try in THIS generator. Drain the` |
|         - |  2387 | `			 * abandoned mid-expression operands and land at the catch/finally body. This is` |
|         - |  2388 | `			 * the pre-loop path (the first fetch uses pc directly), so no -1 adjustment. */` |
|        96 |  2389 | `			while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|        50 |  2390 | `				PH7_MemObjRelease(pTos);` |
|        50 |  2391 | `				pTos--;` |
|         4 |  2392 | `			}` |
|        50 |  2393 | `			pc = (sxi32)pVm->iInlinePc;` |
|        50 |  2394 | `			pVm->pInlineInstr = 0;` |
|        50 |  2395 | `			pVm->pInlineFrame = 0;` |
|        35 |  2396 | `		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|         - |  2397 | `			/* Caught by THIS generator's own try. (VmRecordedResume returns FALSE unless a` |
|         - |  2398 | `			 * catch recorded a resume target for this exec, so rc need not be pre-checked;` |
|         - |  2399 | `			 * unlike OP_THROW there is no lexical-try fallthrough here — the else just` |
|         - |  2400 | `			 * propagates.) Drain the abandoned mid-expression operand slots back to the` |
|         - |  2401 | `			 * catching try's base, then land at its pad (iResumePc is landing-1 for the` |
|         - |  2402 | `			 * dispatcher's trailing pc++; the loop below fetches at pc with no leading pc++,` |
|         - |  2403 | `			 * so add 1 to land on the pad itself). */` |
|       ! 0 |  2404 | `			while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|       ! 0 |  2405 | `				PH7_MemObjRelease(pTos);` |
|       ! 0 |  2406 | `				pTos--;` |
|       ! 0 |  2407 | `			}` |
|       ! 0 |  2408 | `			pc = iResumePc + 1;` |
|       ! 0 |  2409 | `		}else{` |
|         - |  2410 | `			/* Not caught in this generator (no match, or caught by an outer/caller frame` |
|         - |  2411 | `			 * that ROOT B will land at its own OP_CALL site): propagate out so the ctx` |
|         - |  2412 | `			 * closes and the caller sees the exception. */` |
|        12 |  2413 | `			goto Exception;` |
|         - |  2414 | `		}` |
|        23 |  2415 | `	}` |
|         - |  2416 | `	/* Force-close entry (VmCloseCtx): a suspended generator being destroyed runs its` |
|         - |  2417 | ``	 * pending `finally` blocks. Instead of resuming at the yield, redirect straight`` |
|         - |  2418 | ``	 * into the innermost open try's finally as if a `return` had crossed every`` |
|         - |  2419 | `	 * enclosing finally (mirrors OP_SET_FINALLY_RET; OP_END_FINALLY then threads the` |
|         - |  2420 | `	 * PH7_FA_RETURN out through the whole chain and completes the body). Gate on the` |
|         - |  2421 | `	 * BODY bytecode (aInstr == the function program) so nested mini-programs sharing` |
|         - |  2422 | `	 * this ctx/frame never re-fire it; bClosing stays set so OP_YIELD can reject a` |
|         - |  2423 | `	 * yield reached inside one of these finallys. */` |
|   4594447 |  2424 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing` |
|      1980 |  2425 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       250 |  2426 | `	 && aInstr == (VmInstr *)SySetBasePtr(&pVm->pActiveCtx->pFunc->aByteCode) ){` |
|         - |  2427 | `		VmFinallyAction sAct;` |
|       249 |  2428 | `		sxu32 iFpc = 0;` |
|       249 |  2429 | `		int nCross = -1; /* cross every enclosing finally of this body */` |
|       249 |  2430 | `		SyZero(&sAct,sizeof(sAct));` |
|       249 |  2431 | `		sAct.eKind = PH7_FA_RETURN;` |
|       249 |  2432 | `		sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       249 |  2433 | `		PH7_MemObjInit(pVm,&sAct.sRet); /* discarded return; getReturn() is moot post-close */` |
|       249 |  2434 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        24 |  2435 | `			sAct.nCross = nCross;` |
|        24 |  2436 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        24 |  2437 | `			pc = (sxi32)iFpc; /* pre-loop: the first fetch uses pc directly (no -1) */` |
|        13 |  2438 | `		}else{` |
|         - |  2439 | `			/* No open try had a finally: nothing to run, complete the body. */` |
|       227 |  2440 | `			PH7_MemObjRelease(&sAct.sRet);` |
|       227 |  2441 | `			goto Done;` |
|         - |  2442 | `		}` |
|        11 |  2443 | `	}` |
|         - |  2444 | `	/* Execute as much as we can */` |
| 207309678 |  2445 | `	for(;;){` |
|       ! 0 |  2446 | `VmLoopFetch:` |
|         - |  2447 | `		/* C-boundary throw routing (band A #1): a PHP callee invoked from a C` |
|         - |  2448 | `		 * site with no status channel — a __toString/__toInt cast, __get/__set/` |
|         - |  2449 | `		 * offsetGet/offsetSet, __clone, __destruct, a user callback inside a` |
|         - |  2450 | `		 * builtin — raised, and the site continued with a fallback value; the` |
|         - |  2451 | `		 * invocation boundary parked the status here (VmBoundaryPark). Route it` |
|         - |  2452 | `		 * exactly as the throw site's dispatch macro would have: abort, land at` |
|         - |  2453 | `		 * an inline-try redirect, resume at a recorded in-place catch (draining` |
|         - |  2454 | `		 * the abandoned mid-expression operands to the catching try's base), or` |
|         - |  2455 | `		 * propagate out of this exec. Checked at the fetch point, so a swallowed` |
|         - |  2456 | `		 * throw outlives at most the C remainder of ONE opcode instead of` |
|         - |  2457 | `		 * silently resuming the surrounding PHP code with a bogus value.` |
|         - |  2458 | `		 * The pending write-back sweep shares this one guard so the hot` |
|         - |  2459 | `		 * no-hooks path pays a single predicted branch per fetch. */` |
| 423380912 |  2460 | `		if( pVm->nBoundaryRc != 0 \|\| SySetUsed(&pVm->aHookRmw) > 0` |
| 416002370 |  2461 | `		 \|\| PH7_PcntlAsyncPending \|\| pVm->sAllocator.nMemTried != 0` |
| 416002128 |  2462 | `		 \|\| pVm->bGcWanted \|\| pVm->bClosurePurge ){` |
|  14777754 |  2463 | `			if( pVm->bClosurePurge ){` |
|         - |  2464 | `				/* Run-time closures whose last holder went. Freed HERE and not at the` |
|         - |  2465 | `				 * drop, because the drop is usually a dispatch releasing the Closure` |
|         - |  2466 | `				 * object it has just unwrapped and is about to look up by name. */` |
|     20617 |  2467 | `				PH7_VmPurgeDeadClosures(&(*pVm));` |
|     10186 |  2468 | `			}` |
|     22234 |  2469 | `			if( pVm->bGcWanted ){` |
|         - |  2470 | `				/* The cycle collector's root buffer filled. HERE is the only place` |
|         - |  2471 | `				 * it may run: between two instructions, with the operand stack` |
|         - |  2472 | `				 * consistent and no C builtin holding a raw ph7_value* across it.` |
|         - |  2473 | `				 * The drop that buffered the root was in the middle of an opcode's` |
|         - |  2474 | `				 * C body, which is no place to be running destructors. */` |
|        56 |  2475 | `				PH7_GcCollect(&(*pVm));` |
|        27 |  2476 | `			}` |
|     22234 |  2477 | `			if( pVm->sAllocator.nMemTried != 0 ){` |
|         - |  2478 | `				/* memory_limit: an allocation asked for more than the script's` |
|         - |  2479 | `				 * remaining budget and was refused. The allocator cannot raise` |
|         - |  2480 | `				 * anything itself -- it has no VM and no unwind -- so it recorded` |
|         - |  2481 | `				 * the size and disarmed the ceiling, and HERE, at the same safe` |
|         - |  2482 | `				 * point the pcntl handlers and the C-boundary throws use, it` |
|         - |  2483 | `				 * becomes php's fatal.` |
|         - |  2484 | `				 *` |
|         - |  2485 | `				 * php's text exactly, including the two byte counts: this is the` |
|         - |  2486 | `				 * message every framework's OOM triage greps for. Severity 256 is` |
|         - |  2487 | `				 * what makes the label read "Fatal error" -- PHL's label table maps` |
|         - |  2488 | `				 * E_ERROR(1) to its own "Error", and this diagnostic is one users` |
|         - |  2489 | `				 * match against php's, not against the engine's house style.` |
|         - |  2490 | `				 *` |
|         - |  2491 | `				 * The refused allocation has already returned NULL into whatever` |
|         - |  2492 | `				 * asked for it, so the abort is not optional: the caller is holding` |
|         - |  2493 | `				 * a failure it may not check, and the next instruction must not run.` |
|         - |  2494 | `				 */` |
|       ! 0 |  2495 | `				sxu32 nLimit = pVm->sAllocator.nMemLimitHit;` |
|       ! 0 |  2496 | `				sxu32 nTried = pVm->sAllocator.nMemTried;` |
|       ! 0 |  2497 | `				pVm->sAllocator.nMemTried = 0;` |
|       ! 0 |  2498 | `				VmErrorFormat(&(*pVm),256,` |
|         - |  2499 | `					"Allowed memory size of %u bytes exhausted (tried to allocate %u bytes)",` |
|       ! 0 |  2500 | `					nLimit,nTried);` |
|       ! 0 |  2501 | `				goto Abort;` |
|         - |  2502 | `			}` |
|     22234 |  2503 | `			if( PH7_PcntlAsyncPending && pVm->nBoundaryRc == 0 ){` |
|         - |  2504 | `				/* ext/pcntl with pcntl_async_signals(true): a C signal handler` |
|         - |  2505 | `				 * recorded a delivery and raised this flag, and HERE is the` |
|         - |  2506 | `				 * safe point php's EG(vm_interrupt) picks too -- between two` |
|         - |  2507 | `				 * instructions, with the operand stack consistent. The PHP` |
|         - |  2508 | `				 * handlers run now; a throw from one of them is parked in` |
|         - |  2509 | `				 * nBoundaryRc by the callback dispatcher and routed by the` |
|         - |  2510 | `				 * very next branch, exactly as any other C-boundary throw.` |
|         - |  2511 | `				 * A throw that is ALREADY parked wins: running a handler on` |
|         - |  2512 | `				 * top of somebody else's unwind is not a safe point at all,` |
|         - |  2513 | `				 * so the flag is left standing for the next fetch. */` |
|         2 |  2514 | `				PH7_PcntlDrainAsync(&(*pVm));` |
|       ! 0 |  2515 | `			}` |
|     22234 |  2516 | `			if( pVm->nBoundaryRc != 0 ){` |
|      1079 |  2517 | `				sxi32 rcBr = pVm->nBoundaryRc;` |
|      1079 |  2518 | `				pVm->nBoundaryRc = 0;` |
|      1079 |  2519 | `				if( rcBr == PH7_ABORT ){` |
|        12 |  2520 | `					goto Abort;` |
|         - |  2521 | `				}` |
|      1069 |  2522 | `				if( VmInlineOwnedBy(pVm,aInstr,sState.pEntryFrame) ){` |
|         - |  2523 | `					/* Caught by an inline try (generator body) THIS exec owns: drain` |
|         - |  2524 | `					 * and land (pre-fetch path: pc is used directly, no trailing ++). */` |
|       ! 0 |  2525 | `					while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|       ! 0 |  2526 | `						PH7_MemObjRelease(pTos);` |
|       ! 0 |  2527 | `						pTos--;` |
|       ! 0 |  2528 | `					}` |
|       ! 0 |  2529 | `					pc = (sxi32)pVm->iInlinePc;` |
|       ! 0 |  2530 | `					pVm->pInlineInstr = 0;` |
|       ! 0 |  2531 | `					pVm->pInlineFrame = 0;` |
|       ! 0 |  2532 | `				}else{` |
|         - |  2533 | `					sxi32 iBrPc;` |
|      1069 |  2534 | `					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){` |
|         - |  2535 | `						/* Caught in place by a try THIS exec owns: drain the abandoned` |
|         - |  2536 | `						 * operands to the catching try's base and land at its pad` |
|         - |  2537 | `						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;` |
|         - |  2538 | `						 * pre-fetch here, so +1 lands on the pad itself). */` |
|       779 |  2539 | `						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|       367 |  2540 | `							PH7_MemObjRelease(pTos);` |
|       367 |  2541 | `							pTos--;` |
|         5 |  2542 | `						}` |
|       417 |  2543 | `						pc = iBrPc + 1;` |
|       210 |  2544 | `					}else{` |
|         - |  2545 | `						/* Caught by an outer exec (or uncaught-with-report pending):` |
|         - |  2546 | `						 * unwind out of this exec; the owner's macros/router land it. */` |
|       657 |  2547 | `						goto Exception;` |
|         - |  2548 | `					}` |
|         - |  2549 | `				}` |
|       205 |  2550 | `			}` |
|         - |  2551 | `			/* Stale write-back sweep: a pending entry whose OWNING activation is` |
|         - |  2552 | `			 * fetching any pc outside its armed window [nJmpPc, nPc] is dead — for` |
|         - |  2553 | `			 * an RMW entry (window = the modify op alone) a routed throw abandoned` |
|         - |  2554 | `			 * the arming statement mid-flight; for a ??= entry either a throw` |
|         - |  2555 | `			 * abandoned the RHS or the short-circuit jump landed past the` |
|         - |  2556 | `			 * OP_NULLC_STORE (the assign is skipped). Drop it — no set dispatch` |
|         - |  2557 | `			 * (php: the throw/skip discards the write). A nested exec (even a` |
|         - |  2558 | `			 * recursive one over the same bytecode) has a different operand-stack` |
|         - |  2559 | `			 * base and leaves enclosing entries alone; entries below a live top` |
|         - |  2560 | `			 * are reached as the drops expose them. */` |
|     21584 |  2561 | `			while( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|       492 |  2562 | `				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|       490 |  2563 | `				if( pTopRmw->pOwnerStack != (void *)pStack \|\| pTopRmw->pInstrs != (void *)aInstr` |
|       184 |  2564 | `				 \|\| ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){` |
|       241 |  2565 | `					break; /* not ours, or legitimately in flight */` |
|         - |  2566 | `				}` |
|        13 |  2567 | `				VmHookRmwDropTop(&(*pVm));` |
|         1 |  2568 | `			}` |
|     10662 |  2569 | `		}` |
| 423401697 |  2570 | `		if( pTos > pHigh ){` |
|   2704433 |  2571 | `			pHigh = pTos;   /* the activation's high-water mark; see pHigh's declaration */` |
|   1352501 |  2572 | `		}` |
|         - |  2573 | `		/* Fetch the instruction to execute */` |
| 423401697 |  2574 | `		pInstr = &aInstr[pc];` |
| 423401697 |  2575 | `		if( pInstr->nLine ){` |
|         - |  2576 | `			/* Publish the source position for diagnostics, debug_backtrace() and` |
|         - |  2577 | `			 * Throwable. Instructions the compiler could not attribute (nLine 0)` |
|         - |  2578 | `			 * leave the last known line standing rather than reporting line 0.` |
|         - |  2579 | `			 * The compilation unit's strict_types mode rides the same gate: a` |
|         - |  2580 | `			 * SYNTHETIC instruction (nLine 0) must not claim to speak for a file. */` |
| 410157750 |  2581 | `			pVm->nCurLine = pInstr->nLine;` |
| 410157750 |  2582 | `			pVm->bCurStrict = pInstr->bStrict;` |
| 205121903 |  2583 | `		}` |
| 423401697 |  2584 | `		rc = SXRET_OK;` |
|         - |  2585 | `/*` |
|         - |  2586 | ` * What follows here is a massive switch statement where each case implements a` |
|         - |  2587 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|         - |  2588 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|         - |  2589 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|         - |  2590 | ` * the switch statement will break with convention and be flush-left.` |
|         - |  2591 | ` */` |
| 423401697 |  2592 | `		switch(pInstr->iOp){` |
|         - |  2593 | `/*` |
|         - |  2594 | ` * DONE: P1 * *` |
|         - |  2595 | ` *` |
|         - |  2596 | ` * Program execution completed: Clean up the mess left behind` |
|         - |  2597 | ` * and return immediately.` |
|         - |  2598 | ` */` |
|   2268323 |  2599 | `case PH7_OP_DONE:` |
|   4536565 |  2600 | `	if( pInstr->iP2 && sState.bReturnPropagates ){` |
|         - |  2601 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|         - |  2602 | `		 * onto the body frame this catch/finally returns from (skip the transparent` |
|         - |  2603 | `		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION` |
|         - |  2604 | `		 * materializes it into sState.pResult. Drain any finally opened within this body` |
|         - |  2605 | `		 * first (nested try/finally inside the catch), which may overwrite the same` |
|         - |  2606 | `		 * frame's slot (finally-over-catch). */` |
|     24323 |  2607 | `		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|     24323 |  2608 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|     24267 |  2609 | `			PH7_MemObjStore(pTos,&pTgt->sRet);` |
|     24267 |  2610 | `			VmPopOperand(&pTos,1);` |
|     12136 |  2611 | `		}else{` |
|        59 |  2612 | ``			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */`` |
|         - |  2613 | `		}` |
|     24323 |  2614 | `		pTgt->bHasRet = 1;` |
|     24323 |  2615 | `		pTgt->nRetGen++;` |
|     24323 |  2616 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|     24323 |  2617 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2618 | `			goto Abort;` |
|         - |  2619 | `		}` |
|     24323 |  2620 | `		if( rc == PH7_EXCEPTION ){` |
|         - |  2621 | `			/* A drained finally threw past itself — it discards this return. */` |
|       ! 0 |  2622 | `			goto Exception;` |
|         - |  2623 | `		}` |
|     24323 |  2624 | `		goto Done;` |
|         - |  2625 | `	}` |
|         - |  2626 | ``	/* php's `Only variable references should be returned by reference`, raised at`` |
|         - |  2627 | ``	 * the RETURN and nowhere else: a function DECLARED `&` whose return expression`` |
|         - |  2628 | `	 * is not a variable has nothing to bind, and php says so whether the caller` |
|         - |  2629 | `	 * went on to take the answer by reference or by value. Falling off the end and` |
|         - |  2630 | ``	 * a bare `return;` count too -- both leave it with no variable. The`` |
|         - |  2631 | `	 * VM_FRAME_THROW guard is the one the return-type check below uses: a function` |
|         - |  2632 | `	 * unwinding through this terminal OP_DONE never returned anything. */` |
|   4512242 |  2633 | `	if( sState.pEnforceRetFunc` |
|   2270216 |  2634 | `	 && (sState.pEnforceRetFunc->iFlags & VM_FUNC_REF_RETURN)` |
|     13765 |  2635 | `	 && (sState.pEnforceRetFunc->iFlags & VM_FUNC_GENERATOR) == 0` |
|        90 |  2636 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW)` |
|        95 |  2637 | `	 && ( !(pInstr->iP1 && pTos >= pStack) \|\| pTos->nIdx == SXU32_HIGH ) ){` |
|         - |  2638 | ``		/* A `return` reports on its own line; falling off the END of the body has no`` |
|         - |  2639 | `		 * return to report on, and php names the closing brace there. */` |
|         7 |  2640 | `		sxu32 nSavedLine = pVm->nCurLine;` |
|         7 |  2641 | `		if( !(pInstr->iP1 && pTos >= pStack) && sState.pEnforceRetFunc->nEndLine > 0 ){` |
|         3 |  2642 | `			pVm->nCurLine = sState.pEnforceRetFunc->nEndLine;` |
|         1 |  2643 | `		}` |
|         7 |  2644 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|         - |  2645 | `			"Only variable references should be returned by reference");` |
|         7 |  2646 | `		pVm->nCurLine = nSavedLine;` |
|         3 |  2647 | `	}` |
|         - |  2648 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|         - |  2649 | `	 * the fiber start/resume paths) set sState.pEnforceRetFunc, so this branch is` |
|         - |  2650 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|         - |  2651 | `	 * callback trampolines, and the main script. */` |
|   4512242 |  2652 | `	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)` |
|     27818 |  2653 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|         - |  2654 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|         - |  2655 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|         - |  2656 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|         - |  2657 | `		 * value the function never actually returned, so enforcing here would` |
|         - |  2658 | `		 * raise a spurious "Return value must be of type X" over the real` |
|         - |  2659 | `		 * exception. */` |
|     27773 |  2660 | `		ph7_value *pRetVal = 0;` |
|     27773 |  2661 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|     21739 |  2662 | `			pRetVal = pTos;` |
|     10801 |  2663 | `		}` |
|     27773 |  2664 | `		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);` |
|     27773 |  2665 | `		if( rc == PH7_ABORT ) goto Abort;` |
|     27769 |  2666 | `		if( rc == PH7_EXCEPTION ){` |
|       159 |  2667 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|       125 |  2668 | `				PH7_MemObjRelease(pTos);` |
|       125 |  2669 | `				pTos--;` |
|        60 |  2670 | `			}` |
|       159 |  2671 | `			goto Exception;` |
|         - |  2672 | `		}` |
|         - |  2673 | `		/* Don't enforce twice if the function loops through multiple` |
|         - |  2674 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|         - |  2675 | `		 * defensively we clear the pointer after a successful check). */` |
|     27615 |  2676 | `		sState.pEnforceRetFunc = 0;` |
|     13596 |  2677 | `	}` |
|   4512089 |  2678 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|   3012529 |  2679 | `		if( sState.pLastRef ){` |
|    223889 |  2680 | `			*sState.pLastRef = pTos->nIdx;` |
|    112306 |  2681 | `		}` |
|   3012529 |  2682 | `		if( sState.pResult ){` |
|         - |  2683 | `			/* Execution result */` |
|   1511565 |  2684 | `			PH7_MemObjStore(pTos,sState.pResult);` |
|    755960 |  2685 | `		}` |
|   3012529 |  2686 | `		VmPopOperand(&pTos,1);` |
|   1506411 |  2687 | `	}else{` |
|   1499565 |  2688 | `		if( pInstr->iP1 == 0 && pInstr->iP2 && sState.pResult ){` |
|         - |  2689 | ``			/* An EXPLICIT `return;` with no value answers NULL. It reads as a`` |
|         - |  2690 | `			 * no-op for a function (whose result slot starts out null anyway)` |
|         - |  2691 | `			 * and matters for an included CHUNK, whose slot is seeded with the 1` |
|         - |  2692 | ``			 * a file that returns nothing answers: `<?php return;` is php's`` |
|         - |  2693 | `			 * NULL, not that 1. */` |
|       325 |  2694 | `			PH7_MemObjRelease(sState.pResult);` |
|       160 |  2695 | `		}` |
|         - |  2696 | `		/* Nothing referenced — also the throw-unwind path: the compiler routes` |
|         - |  2697 | `		 * an uncaught exception to this terminal OP_DONE with iP1 set but an` |
|         - |  2698 | `		 * empty operand stack (pTos == pStack-1), so there is no return value to` |
|         - |  2699 | `		 * store. Guarding on pTos >= pStack (matching the sibling branch above)` |
|         - |  2700 | `		 * avoids the below-base read that crashed under glibc/ASan. */` |
|   1499565 |  2701 | `		if( sState.pLastRef ){` |
|     27717 |  2702 | `			*sState.pLastRef = SXU32_HIGH;` |
|     13711 |  2703 | `		}` |
|         - |  2704 | `	}` |
|         - |  2705 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|         - |  2706 | `	 * this execution. When 'return' is used inside a try block,` |
|         - |  2707 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|         - |  2708 | `	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering` |
|         - |  2709 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|         - |  2710 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|         - |  2711 | `	 * block can override it (the finally writes this body frame's sRet slot,` |
|         - |  2712 | `	 * materialized below).` |
|         - |  2713 | `	 */` |
|   4512089 |  2714 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|   4512089 |  2715 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2716 | `		goto Abort;` |
|         - |  2717 | `	}` |
|   4512089 |  2718 | `	if( rc == PH7_EXCEPTION ){` |
|         - |  2719 | `		/* A drained finally threw past itself, discarding the value this OP_DONE` |
|         - |  2720 | `		 * stored into sState.pResult. If an enclosing try IN THIS function caught the new` |
|         - |  2721 | `		 * exception in place, resume at its landing pad; otherwise unwind (the` |
|         - |  2722 | `		 * caller's exception-resume pops the stored result). */` |
|         - |  2723 | `		sxi32 iResumePc;` |
|         5 |  2724 | `		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|         3 |  2725 | `			pc = iResumePc;` |
|         3 |  2726 | `			break;` |
|         - |  2727 | `		}` |
|         3 |  2728 | `		goto Exception;` |
|         - |  2729 | `	}` |
|   4512085 |  2730 | `	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){` |
|         - |  2731 | `		/* A catch/finally issued a 'return' targeting THIS body. If the body is` |
|         - |  2732 | `		 * actually unwinding because an exception escaped it (terminal OP_DONE on` |
|         - |  2733 | `		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type` |
|         - |  2734 | `		 * enforcement above), that exception supersedes the return: discard it.` |
|         - |  2735 | `		 * Otherwise materialize it as this function's result. */` |
|        11 |  2736 | `		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){` |
|       ! 0 |  2737 | `			VmClearFramePending(sState.pEntryFrame);` |
|       ! 0 |  2738 | `		}else{` |
|        11 |  2739 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|         - |  2740 | `		}` |
|         4 |  2741 | `	}` |
|   4512085 |  2742 | `	goto Done;` |
|         - |  2743 | `/*` |
|         - |  2744 | ` * HALT: P1 * *` |
|         - |  2745 | ` *` |
|         - |  2746 | ` * Program execution aborted: Clean up the mess left behind` |
|         - |  2747 | ` * and abort immediately.` |
|         - |  2748 | ` */` |
|        54 |  2749 | `case PH7_OP_HALT:` |
|       119 |  2750 | `	if( pInstr->iP1 ){` |
|         - |  2751 | `#ifdef UNTRUST` |
|         - |  2752 | `		if( pTos < pStack ){` |
|         - |  2753 | `			goto Abort;` |
|         - |  2754 | `		}` |
|         - |  2755 | `#endif` |
|       119 |  2756 | `		if( sState.pLastRef ){` |
|        68 |  2757 | `			*sState.pLastRef = pTos->nIdx;` |
|        32 |  2758 | `		}` |
|       119 |  2759 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        81 |  2760 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|         - |  2761 | `				/* Output the exit message */` |
|       123 |  2762 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        42 |  2763 | `					pVm->sVmConsumer.pUserData);` |
|        81 |  2764 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        46 |  2765 | `			}` |
|        82 |  2766 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|         - |  2767 | `			/* Record exit status */` |
|        40 |  2768 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        19 |  2769 | `		}` |
|       119 |  2770 | `		VmPopOperand(&pTos,1);` |
|        61 |  2771 | `	}else if( sState.pLastRef ){` |
|         - |  2772 | `		/* Nothing referenced */` |
|       ! 0 |  2773 | `		*sState.pLastRef = SXU32_HIGH;` |
|       ! 0 |  2774 | `	}` |
|         - |  2775 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|         - |  2776 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|         - |  2777 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|         - |  2778 | `	 */` |
|       119 |  2779 | `	pVm->bHaltRequested = 1;` |
|       119 |  2780 | `	goto Abort;` |
|         - |  2781 | `/*` |
|         - |  2782 | ` * JMP: * P2 *` |
|         - |  2783 | ` *` |
|         - |  2784 | ` * Unconditional jump: The next instruction executed will be` |
|         - |  2785 | ` * the one at index P2 from the beginning of the program.` |
|         - |  2786 | ` */` |
|   8079716 |  2787 | `case PH7_OP_JMP:` |
|  16160369 |  2788 | `	pc = pInstr->iP2 - 1;` |
|  16160369 |  2789 | `	break;` |
|         - |  2790 | `/*` |
|         - |  2791 | ` * JZ: P1 P2 *` |
|         - |  2792 | ` *` |
|         - |  2793 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|         - |  2794 | ` * entry in the stack if P1 is zero.` |
|         - |  2795 | ` */` |
|  16553484 |  2796 | `case PH7_OP_JZ:` |
|         - |  2797 | `#ifdef UNTRUST` |
|         - |  2798 | `	if( pTos < pStack ){` |
|         - |  2799 | `		goto Abort;` |
|         - |  2800 | `	}` |
|         - |  2801 | `#endif` |
|         - |  2802 | `	/* Get a boolean value */` |
|  33116848 |  2803 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     38461 |  2804 | `		PH7_MemObjToBool(pTos);` |
|     19222 |  2805 | `	}` |
|  33116848 |  2806 | `	if( !pTos->x.iVal ){` |
|         - |  2807 | `		/* Take the jump */` |
|   5253409 |  2808 | `		pc = pInstr->iP2 - 1;` |
|   2629457 |  2809 | `	}` |
|  33116848 |  2810 | `	if( !pInstr->iP1 ){` |
|  25809768 |  2811 | `		VmPopOperand(&pTos,1);` |
|  12909488 |  2812 | `	}` |
|  33116848 |  2813 | `	break;` |
|         - |  2814 | `/*` |
|         - |  2815 | ` * JNZ: P1 P2 *` |
|         - |  2816 | ` *` |
|         - |  2817 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|         - |  2818 | ` * entry in the stack if P1 is zero.` |
|         - |  2819 | ` */` |
|   3758195 |  2820 | `case PH7_OP_JNZ:` |
|         - |  2821 | `#ifdef UNTRUST` |
|         - |  2822 | `	if( pTos < pStack ){` |
|         - |  2823 | `		goto Abort;` |
|         - |  2824 | `	}` |
|         - |  2825 | `#endif` |
|         - |  2826 | `	/* Get a boolean value */` |
|   7517740 |  2827 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        58 |  2828 | `		PH7_MemObjToBool(pTos);` |
|        28 |  2829 | `	}` |
|   7517740 |  2830 | `	if( pTos->x.iVal ){` |
|         - |  2831 | `		/* Take the jump */` |
|    675172 |  2832 | `		pc = pInstr->iP2 - 1;` |
|    337585 |  2833 | `	}` |
|   7517740 |  2834 | `	if( !pInstr->iP1 ){` |
|        90 |  2835 | `		VmPopOperand(&pTos,1);` |
|        45 |  2836 | `	}` |
|   7517740 |  2837 | `	break;` |
|         - |  2838 | `/*` |
|         - |  2839 | ` * NOOP: * * *` |
|         - |  2840 | ` *` |
|         - |  2841 | ` * Do nothing. This instruction is often useful as a jump` |
|         - |  2842 | ` * destination.` |
|         - |  2843 | ` */` |
|         3 |  2844 | `case PH7_OP_NOOP:` |
|         6 |  2845 | `	break;` |
|         - |  2846 | `/*` |
|         - |  2847 | ` * SNAPSHOT: P1 * *` |
|         - |  2848 | ` *` |
|         - |  2849 | ` * Give the top P1 stack slots their own copy of the string bytes they are borrowing` |
|         - |  2850 | ` * (P1 = 0 means the top slot alone).` |
|         - |  2851 | ` *` |
|         - |  2852 | ` * A value copy aliases the source's buffer rather than duplicating it, so a value pushed` |
|         - |  2853 | `` * from `$x` is a pointer into `$x` plus the length `$x` had at the push. Anything that`` |
|         - |  2854 | `` * runs before the push is consumed and assigns to `$x` overwrites those bytes in place,`` |
|         - |  2855 | ` * and the pushed value then reads the NEW content through the OLD length --` |
|         - |  2856 | `` * `show($x, $x = 'second')` printed "secon" for an argument php had already copied as`` |
|         - |  2857 | `` * "first", and `[$x, $x = 'second']` built the same truncated element. The compiler emits`` |
|         - |  2858 | ` * this only where something that can run code still sits between a push and its consumer,` |
|         - |  2859 | ` * so ordinary code pays for it exactly where php's own SEND_VAR pays for a reference.` |
|         - |  2860 | ` */` |
|    880482 |  2861 | `case PH7_OP_SNAPSHOT: {` |
|   1762009 |  2862 | `	sxi32 nSnap = pInstr->iP1 > 0 ? pInstr->iP1 : 1;` |
|         - |  2863 | `	ph7_value *pSnap;` |
|         - |  2864 | `#ifdef UNTRUST` |
|         - |  2865 | `	if( pTos < pStack ){` |
|         - |  2866 | `		goto Abort;` |
|         - |  2867 | `	}` |
|         - |  2868 | `#endif` |
|   1762009 |  2869 | `	if( &pTos[-nSnap+1] < pStack ){` |
|       ! 0 |  2870 | `		nSnap = (sxi32)(pTos - pStack) + 1;` |
|       ! 0 |  2871 | `	}` |
|   3560897 |  2872 | `	for( pSnap = pTos ; nSnap > 0 ; --nSnap, --pSnap ){` |
|   1798893 |  2873 | `		SyBlobMakePrivate(&pSnap->sBlob);` |
|    899938 |  2874 | `	}` |
|   1762009 |  2875 | `	break;` |
|         - |  2876 | `					   }` |
|         - |  2877 | `/*` |
|         - |  2878 | ` * PICK: P1 * *` |
|         - |  2879 | ` *` |
|         - |  2880 | ` * Push a copy of the stack slot P1 below the top (P1 = 0 duplicates the top, as DUP does).` |
|         - |  2881 | ` *` |
|         - |  2882 | ` * An assignment target's dynamic subscript and property NAMES are evaluated before the` |
|         - |  2883 | ` * assigned value in php and the fetches they belong to run after it, so the compiler pushes` |
|         - |  2884 | ` * those names first, puts the value on top of them, and emits the access chain last. Each` |
|         - |  2885 | ` * access then reads its name back from the slot it was parked in -- which is at a depth the` |
|         - |  2886 | ` * compiler knows exactly, since every level of the chain consumes a container and a name and` |
|         - |  2887 | ` * leaves one element behind it.` |
|         - |  2888 | ` */` |
|      6626 |  2889 | `case PH7_OP_PICK: {` |
|         - |  2890 | `	ph7_value *pPick;` |
|         - |  2891 | `#ifdef UNTRUST` |
|         - |  2892 | `	if( &pTos[-pInstr->iP1] < pStack ){` |
|         - |  2893 | `		goto Abort;` |
|         - |  2894 | `	}` |
|         - |  2895 | `#endif` |
|     13256 |  2896 | `	pPick = &pTos[-pInstr->iP1];` |
|     13256 |  2897 | `	pTos++;` |
|     13256 |  2898 | `	PH7_MemObjInit(pVm,pTos);` |
|     13256 |  2899 | `	PH7_MemObjStore(pPick,pTos);` |
|     13256 |  2900 | `	break;` |
|         - |  2901 | `				 }` |
|         - |  2902 | `/*` |
|         - |  2903 | ` * POP: P1 * *` |
|         - |  2904 | ` *` |
|         - |  2905 | ` * Pop P1 elements from the operand stack.` |
|         - |  2906 | ` */` |
|  11830284 |  2907 | `case PH7_OP_POP: {` |
|  23666511 |  2908 | `	sxi32 n = pInstr->iP1;` |
|  23666511 |  2909 | `	if( &pTos[-n+1] < pStack ){` |
|         - |  2910 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       ! 0 |  2911 | `		n = (sxi32)(pTos - pStack);` |
|       ! 0 |  2912 | `	}` |
|  23666511 |  2913 | `	VmPopOperand(&pTos,n);` |
|  23666511 |  2914 | `	break;` |
|         - |  2915 | `				 }` |
|         - |  2916 | `/*` |
|         - |  2917 | ` * DUP: * * *` |
|         - |  2918 | ` *` |
|         - |  2919 | ` * Duplicate the top of the stack.` |
|         - |  2920 | ` */` |
|       426 |  2921 | `case PH7_OP_DUP:` |
|         - |  2922 | `#ifdef UNTRUST` |
|         - |  2923 | `	if( pTos < pStack ){` |
|         - |  2924 | `		goto Abort;` |
|         - |  2925 | `	}` |
|         - |  2926 | `#endif` |
|       857 |  2927 | `	pTos++;` |
|       857 |  2928 | `	PH7_MemObjInit(pVm,pTos);` |
|       857 |  2929 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       857 |  2930 | `	break;` |
|         - |  2931 | `/*` |
|         - |  2932 | ` * SWAP: * * *` |
|         - |  2933 | ` *` |
|         - |  2934 | ` * Exchange the top two stack slots.` |
|         - |  2935 | ` *` |
|         - |  2936 | `` * Emitted for one shape only: a binary operator whose LEFT operand is a plain `$var` and`` |
|         - |  2937 | ` * whose RIGHT operand can run code. php never materializes a plain variable operand -- it` |
|         - |  2938 | `` * reads the compiled variable AT the operator -- so `$n = 1; $n - ($n = 5)` is 0 there and`` |
|         - |  2939 | ` * the assignment's value is what BOTH sides see. Copying the left operand would answer the` |
|         - |  2940 | ` * pre-assignment 1; the load has to MOVE instead, which puts it on the stack above the` |
|         - |  2941 | ` * right operand and needs the two put back in the operator's order. The whole ph7_value` |
|         - |  2942 | ` * moves, pVm included -- it is the same pointer in both -- and no jump can land between` |
|         - |  2943 | ` * the two pushes and this, so nothing is holding either slot's address.` |
|         - |  2944 | ` */` |
|   3397511 |  2945 | `case PH7_OP_SWAP: {` |
|         - |  2946 | `	ph7_value sSwap;` |
|         - |  2947 | `#ifdef UNTRUST` |
|         - |  2948 | `	if( pTos < &pStack[1] ){` |
|         - |  2949 | `		goto Abort;` |
|         - |  2950 | `	}` |
|         - |  2951 | `#endif` |
|   6795264 |  2952 | `	sSwap  = pTos[0];` |
|   6795264 |  2953 | `	pTos[0] = pTos[-1];` |
|   6795264 |  2954 | `	pTos[-1] = sSwap;` |
|   6795264 |  2955 | `	break;` |
|         - |  2956 | `				  }` |
|         - |  2957 | `/*` |
|         - |  2958 | ` * OP_FUNC_DECL * * P3` |
|         - |  2959 | ` *` |
|         - |  2960 | ` * Bind the function p3 names, here, where the declaration STATEMENT sits. Only a` |
|         - |  2961 | ` * conditional declaration takes this route: a top-level one is bound while its unit` |
|         - |  2962 | ` * compiles, exactly as php early-binds it.` |
|         - |  2963 | ` */` |
|        98 |  2964 | `case PH7_OP_FUNC_DECL: {` |
|       201 |  2965 | `	ph7_vm_func *pDeclFunc = (ph7_vm_func *)pInstr->p3;` |
|       201 |  2966 | `	if( pDeclFunc ){` |
|       299 |  2967 | `		SyHashEntry *pDeclEntry = SyHashGet(&pVm->hFunction,` |
|       196 |  2968 | `			SyStringData(&pDeclFunc->sName),SyStringLength(&pDeclFunc->sName));` |
|       196 |  2969 | `		if( pInstr->iP1 == 0` |
|       141 |  2970 | `		 && PH7_VmNameIsInternalFunc(pVm,SyStringData(&pDeclFunc->sName),` |
|        38 |  2971 | `				SyStringLength(&pDeclFunc->sName)) ){` |
|         - |  2972 | `			/* An INTERNAL function of this name. php refuses a conditional` |
|         - |  2973 | `			 * declaration of one exactly as it refuses a top-level one -- being` |
|         - |  2974 | `			 * reached at run time buys it nothing -- and names no previous` |
|         - |  2975 | `			 * declaration, an internal function having no file and no line.` |
|         - |  2976 | `			 *` |
|         - |  2977 | `			 * A polyfill does not land here: it asks function_exists() first, and` |
|         - |  2978 | `			 * for a name this engine carries that answers true and the body is` |
|         - |  2979 | `			 * never reached. Where it answers false the name is not internal and` |
|         - |  2980 | `			 * this test does not fire. */` |
|         3 |  2981 | `			PH7_VmFatalError(&(*pVm),"Cannot redeclare function %z()",&pDeclFunc->sName);` |
|         3 |  2982 | `			pVm->iExitStatus = 255;` |
|         3 |  2983 | `			pVm->bHaltRequested = 1;` |
|         3 |  2984 | `			goto Abort;` |
|         - |  2985 | `		}` |
|       199 |  2986 | `		if( pDeclEntry && pInstr->iP1 == 0 ){` |
|         - |  2987 | `			/* php's runtime redeclaration fatal -- the same sentence the compiler` |
|         - |  2988 | `			 * raises for two top-level declarations of one name, and now in the same` |
|         - |  2989 | ``			 * SHAPE: `PHP Fatal error:  ` and a `Stack trace:` block, which the plain`` |
|         - |  2990 | `			 * diagnostic printer this used to go through gives neither of. A BUILTIN` |
|         - |  2991 | `			 * of that name is not in this table, so a polyfill body that reaches here` |
|         - |  2992 | ``			 * beside one is the `function_exists()` guard having answered false.`` |
|         - |  2993 | `			 *` |
|         - |  2994 | `			 * The name is taken whenever this statement RUNS, so running the same` |
|         - |  2995 | ``			 * declaration a second time collides with itself -- `function mk(){`` |
|         - |  2996 | ``			 * function h(){} } mk(); mk();` is php's fatal, naming one line as both`` |
|         - |  2997 | `			 * the refusal and the previous declaration. Testing the installed entry` |
|         - |  2998 | `			 * for IDENTITY instead made the second run a silent no-op. */` |
|         3 |  2999 | `			ph7_vm_func *pPrevDecl = (ph7_vm_func *)pDeclEntry->pUserData;` |
|         3 |  3000 | `			if( pPrevDecl && pPrevDecl->sFile.nByte > 0 ){` |
|         4 |  3001 | `				PH7_VmFatalError(&(*pVm),` |
|         - |  3002 | `					"Cannot redeclare function %z() (previously declared in %.*s:%u)",` |
|         1 |  3003 | `					&pDeclFunc->sName,pPrevDecl->sFile.nByte,pPrevDecl->sFile.zString,` |
|         1 |  3004 | `					pPrevDecl->nLine);` |
|         2 |  3005 | `			}else{` |
|       ! 0 |  3006 | `				PH7_VmFatalError(&(*pVm),"Cannot redeclare function %z()",&pDeclFunc->sName);` |
|         - |  3007 | `			}` |
|         3 |  3008 | `			pVm->iExitStatus = 255;` |
|         3 |  3009 | `			pVm->bHaltRequested = 1;` |
|         3 |  3010 | `			goto Abort;` |
|         - |  3011 | `		}` |
|       197 |  3012 | `		if( pDeclEntry == 0 \|\| pDeclEntry->pUserData != (void *)pDeclFunc ){` |
|       173 |  3013 | `			PH7_VmInstallUserFunction(&(*pVm),pDeclFunc,0);` |
|        84 |  3014 | `		}` |
|        96 |  3015 | `	}` |
|       197 |  3016 | `	break;` |
|         - |  3017 | `				}` |
|         - |  3018 | `/*` |
|         - |  3019 | ` * CONST_DECL * * P3` |
|         - |  3020 | ` *` |
|         - |  3021 | ` * Bind the global constant p3 (VmConstDecl) names to the value on top of the stack,` |
|         - |  3022 | `` * here, where its `const` statement runs, and pop it. A name already taken is the`` |
|         - |  3023 | `` * warning define() raises, blamed on the `const` keyword's line, and the first value`` |
|         - |  3024 | ` * stays; the value just computed is dropped.` |
|         - |  3025 | ` */` |
|       122 |  3026 | `case PH7_OP_CONST_DECL: {` |
|       249 |  3027 | `	VmConstDecl *pDecl = (VmConstDecl *)pInstr->p3;` |
|         - |  3028 | `#ifdef UNTRUST` |
|         - |  3029 | `	if( pTos < pStack ){` |
|         - |  3030 | `		goto Abort;` |
|         - |  3031 | `	}` |
|         - |  3032 | `#endif` |
|       249 |  3033 | `	if( pDecl ){` |
|       249 |  3034 | `		sxu32 nSavedLine = pVm->nCurLine;` |
|       249 |  3035 | `		pVm->nCurLine = pDecl->nLine;` |
|       249 |  3036 | `		if( !PH7_VmConstantNameTaken(&(*pVm),pDecl->sName.zString,pDecl->sName.nByte) ){` |
|       237 |  3037 | `			ph7_value *pKeep = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|       237 |  3038 | `			if( pKeep ){` |
|       237 |  3039 | `				PH7_MemObjInit(&(*pVm),pKeep);` |
|       237 |  3040 | `				PH7_MemObjStore(pTos,pKeep);` |
|       348 |  3041 | `				if( PH7_VmRegisterConstantEx(&(*pVm),&pDecl->sName,VmExpandUserConstant,pKeep,` |
|       237 |  3042 | `						pDecl->sFile.nByte > 0 ? &pDecl->sFile : 0,pDecl->nLine,1) == SXRET_OK ){` |
|       237 |  3043 | `					if( SySetUsed(&pDecl->aAttrs) > 0 ){` |
|        19 |  3044 | `						SyHashEntry *pEntry = PH7_VmConstantFetch(&(*pVm),pDecl->sName.zString,` |
|         6 |  3045 | `							pDecl->sName.nByte,0);` |
|        13 |  3046 | `						if( pEntry ){` |
|        13 |  3047 | `							ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        13 |  3048 | `							ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pDecl->aAttrs);` |
|         - |  3049 | `							sxu32 n;` |
|        27 |  3050 | `							for( n = 0 ; n < SySetUsed(&pDecl->aAttrs) ; ++n ){` |
|        15 |  3051 | `								SySetPut(&pCons->aAttrs,(const void *)&aAttr[n]);` |
|         8 |  3052 | `							}` |
|         6 |  3053 | `						}` |
|         6 |  3054 | `					}` |
|       121 |  3055 | `				}else{` |
|       ! 0 |  3056 | `					PH7_MemObjRelease(pKeep);` |
|       ! 0 |  3057 | `					SyMemBackendPoolFree(&pVm->sAllocator,pKeep);` |
|         - |  3058 | `				}` |
|       116 |  3059 | `			}` |
|       116 |  3060 | `		}` |
|       249 |  3061 | `		pVm->nCurLine = nSavedLine;` |
|       122 |  3062 | `	}` |
|       249 |  3063 | `	VmPopOperand(&pTos,1);` |
|       249 |  3064 | `	break;` |
|         - |  3065 | `				}` |
|         - |  3066 | `/*` |
|         - |  3067 | ` * CLASS_DEFER: * * P3` |
|         - |  3068 | ` *` |
|         - |  3069 | ` * Execute a class declaration whose parent/interface/trait could not be` |
|         - |  3070 | ` * resolved when the enclosing file compiled (its autoloader had not RUN yet).` |
|         - |  3071 | ` * P3 is the VmDeferredClass record captured by the compiler. A dependency` |
|         - |  3072 | `` * that is STILL missing throws php's catchable `... not found` Error; a`` |
|         - |  3073 | ` * failed re-compile aborts (its errors were already reported). See the` |
|         - |  3074 | ` * deferral block comment in compile_class.c.` |
|         - |  3075 | ` */` |
|        90 |  3076 | `case PH7_OP_CLASS_DEFER: {` |
|       185 |  3077 | `	VmDeferredClass *pDefer = (VmDeferredClass *)pInstr->p3;` |
|       185 |  3078 | `	VmDeferredReq *pMissing = 0;` |
|       185 |  3079 | `	sxi32 rcDecl = SXRET_OK;` |
|       185 |  3080 | `	if( pDefer ){` |
|       185 |  3081 | `		rcDecl = VmExecDeferredClass(&(*pVm),pDefer,&pMissing);` |
|        90 |  3082 | `	}` |
|       185 |  3083 | `	if( pMissing ){` |
|         - |  3084 | `		static const char *azDeferKind[] = { "Class", "Interface", "Trait" };` |
|         - |  3085 | `		char zDeclMsg[520];` |
|        20 |  3086 | `		sxu32 nDeclMsg = SyBufferFormat(zDeclMsg,sizeof(zDeclMsg),"%s \"%.*s\" not found",` |
|         6 |  3087 | `			azDeferKind[pMissing->cKind < 3 ? pMissing->cKind : 0],` |
|        12 |  3088 | `			(int)pMissing->sName.nByte,pMissing->sName.zString);` |
|        14 |  3089 | `		rc = VmThrowFromVm(&(*pVm),"Error",zDeclMsg,nDeclMsg);` |
|        14 |  3090 | `		if( rc == SXERR_ABORT ){` |
|         3 |  3091 | `			goto Abort;` |
|         - |  3092 | `		}` |
|        11 |  3093 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  3094 | `	}` |
|       173 |  3095 | `	if( rcDecl == SXERR_ABORT ){` |
|         5 |  3096 | `		goto Abort;` |
|         - |  3097 | `	}` |
|       169 |  3098 | `	break;` |
|         - |  3099 | `				}` |
|         - |  3100 | `/*` |
|         - |  3101 | ` * CLASS_OBLIGE: * * P3` |
|         - |  3102 | ` *` |
|         - |  3103 | ` * Settle the variance pairs a class's compile-time link left unresolved (P3, a` |
|         - |  3104 | ` * VmClassObligeSet): autoload what they name, check them again, and halt on a` |
|         - |  3105 | ` * refusal -- php's compile fatal, raised where the declaration runs.` |
|         - |  3106 | ` */` |
|        15 |  3107 | `case PH7_OP_CLASS_OBLIGE:` |
|        34 |  3108 | `	if( pInstr->p3 && PH7_ClassSettleObligations(&(*pVm),(VmClassObligeSet *)pInstr->p3) == SXERR_ABORT ){` |
|        24 |  3109 | `		goto Abort;` |
|         - |  3110 | `	}` |
|        11 |  3111 | `	break;` |
|         - |  3112 | `/*` |
|         - |  3113 | ` * CLASS_DECLARE: * * P3` |
|         - |  3114 | ` *` |
|         - |  3115 | ` * Declare P3, a top-level class php does not early-bind, where its statement runs.` |
|         - |  3116 | ` */` |
|       619 |  3117 | `case PH7_OP_CLASS_DECLARE:` |
|      1243 |  3118 | `	if( pInstr->p3 && PH7_VmDeclareHiddenClass(&(*pVm),(ph7_class *)pInstr->p3) == SXERR_ABORT ){` |
|         3 |  3119 | `		goto Abort;` |
|         - |  3120 | `	}` |
|      1241 |  3121 | `	break;` |
|         - |  3122 | `/*` |
|         - |  3123 | ` * CVT_INT: * * *` |
|         - |  3124 | ` *` |
|         - |  3125 | ` * Force the top of the stack to be an integer.` |
|         - |  3126 | ` */` |
|      1455 |  3127 | `case PH7_OP_CVT_INT:` |
|         - |  3128 | `#ifdef UNTRUST` |
|         - |  3129 | `	if( pTos < pStack ){` |
|         - |  3130 | `		goto Abort;` |
|         - |  3131 | `	}` |
|         - |  3132 | `#endif` |
|      2882 |  3133 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  3134 | `		/* php warns from the conversion itself when no int can hold the float` |
|         - |  3135 | ``		 * (`(int)1e19`); the value it then answers is the modular wrap. */`` |
|      2084 |  3136 | `		PH7_MemObjWarnIntCast(pTos);` |
|      2084 |  3137 | `		PH7_MemObjToInteger(pTos);` |
|      1023 |  3138 | `	}` |
|         - |  3139 | `	/* Invalidate any prior representation */` |
|      2882 |  3140 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      2882 |  3141 | `	break;` |
|         - |  3142 | `/*` |
|         - |  3143 | ` * CVT_REAL: * * *` |
|         - |  3144 | ` *` |
|         - |  3145 | ` * Force the top of the stack to be a real.` |
|         - |  3146 | ` */` |
|       134 |  3147 | `case PH7_OP_CVT_REAL:` |
|         - |  3148 | `#ifdef UNTRUST` |
|         - |  3149 | `	if( pTos < pStack ){` |
|         - |  3150 | `		goto Abort;` |
|         - |  3151 | `	}` |
|         - |  3152 | `#endif` |
|       273 |  3153 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       273 |  3154 | `		PH7_MemObjToReal(pTos);` |
|       134 |  3155 | `	}` |
|         - |  3156 | `	/* Invalidate any prior representation */` |
|       273 |  3157 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       273 |  3158 | `	break;` |
|         - |  3159 | `/*` |
|         - |  3160 | ` * CVT_STR: * * *` |
|         - |  3161 | ` *` |
|         - |  3162 | ` * Force the top of the stack to be a string.` |
|         - |  3163 | ` */` |
|      3601 |  3164 | `case PH7_OP_CVT_STR:` |
|         - |  3165 | `#ifdef UNTRUST` |
|         - |  3166 | `	if( pTos < pStack ){` |
|         - |  3167 | `		goto Abort;` |
|         - |  3168 | `	}` |
|         - |  3169 | `#endif` |
|         - |  3170 | `	/* (string) cast + string interpolation "$arr": php's user-visible` |
|         - |  3171 | `	 * array->string warning site, and the not-stringable-object throw. */` |
|         - |  3172 | `	{` |
|      7204 |  3173 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|      7206 |  3174 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|         - |  3175 | `	}` |
|      7114 |  3176 | `	break;` |
|         - |  3177 | `/*` |
|         - |  3178 | ` * CVT_BOOL: * * *` |
|         - |  3179 | ` *` |
|         - |  3180 | ` * Force the top of the stack to be a boolean.` |
|         - |  3181 | ` */` |
|        74 |  3182 | `case PH7_OP_CVT_BOOL:` |
|         - |  3183 | `#ifdef UNTRUST` |
|         - |  3184 | `	if( pTos < pStack ){` |
|         - |  3185 | `		goto Abort;` |
|         - |  3186 | `	}` |
|         - |  3187 | `#endif` |
|       153 |  3188 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       145 |  3189 | `		PH7_MemObjToBool(pTos);` |
|        70 |  3190 | `	}` |
|       153 |  3191 | `	break;` |
|         - |  3192 | `/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it` |
|         - |  3193 | ` * always raises "The (unset) cast is no longer supported" (php 8 removed` |
|         - |  3194 | ` * the cast), so a program containing it never compiles. The switch has no` |
|         - |  3195 | ` * default arm, so abort loudly rather than fall through as a silent no-op` |
|         - |  3196 | ` * if a future emitter ever produces one without the compile error. */` |
|       ! 0 |  3197 | `case PH7_OP_CVT_NULL:` |
|       ! 0 |  3198 | `	goto Abort;` |
|         - |  3199 | `/*` |
|         - |  3200 | ` * CVT_NUMC: * * *` |
|         - |  3201 | ` *` |
|         - |  3202 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|         - |  3203 | ` */` |
|       ! 0 |  3204 | `case PH7_OP_CVT_NUMC:` |
|         - |  3205 | `#ifdef UNTRUST` |
|         - |  3206 | `	if( pTos < pStack ){` |
|         - |  3207 | `		goto Abort;` |
|         - |  3208 | `	}` |
|         - |  3209 | `#endif` |
|         - |  3210 | `	/* Force a numeric cast */` |
|       ! 0 |  3211 | `	PH7_MemObjToNumeric(pTos);` |
|       ! 0 |  3212 | `	break;` |
|         - |  3213 | `/*` |
|         - |  3214 | ` * CVT_ARRAY: * * *` |
|         - |  3215 | ` *` |
|         - |  3216 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|         - |  3217 | ` */` |
|       437 |  3218 | `case PH7_OP_CVT_ARRAY:` |
|         - |  3219 | `#ifdef UNTRUST` |
|         - |  3220 | `	if( pTos < pStack ){` |
|         - |  3221 | `		goto Abort;` |
|         - |  3222 | `	}` |
|         - |  3223 | `#endif` |
|         - |  3224 | `	/* Force a hashmap cast */` |
|       879 |  3225 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       879 |  3226 | `	if( rc != SXRET_OK ){` |
|         - |  3227 | `		/* Not so fatal,emit a simple warning */` |
|       ! 0 |  3228 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|         - |  3229 | `			"PH7 engine is running out of memory while performing an array cast");` |
|       ! 0 |  3230 | `	}` |
|       879 |  3231 | `	break;` |
|         - |  3232 | `/*` |
|         - |  3233 | ` * CVT_OBJ: * * *` |
|         - |  3234 | ` *` |
|         - |  3235 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|         - |  3236 | ` */` |
|        32 |  3237 | `case PH7_OP_CVT_OBJ:` |
|         - |  3238 | `#ifdef UNTRUST` |
|         - |  3239 | `	if( pTos < pStack ){` |
|         - |  3240 | `		goto Abort;` |
|         - |  3241 | `	}` |
|         - |  3242 | `#endif` |
|        68 |  3243 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - |  3244 | `		/* Force a 'stdClass()' cast */` |
|        68 |  3245 | `		PH7_MemObjToObject(pTos);` |
|        32 |  3246 | `	}` |
|        68 |  3247 | `	break;` |
|         - |  3248 | `/*` |
|         - |  3249 | ` * ERR_CTRL * * *` |
|         - |  3250 | ` *` |
|         - |  3251 | ` * Error control operator.` |
|         - |  3252 | ` */` |
|      4510 |  3253 | `case PH7_OP_UNSET_VAR: {` |
|         - |  3254 | `	VmOpRc rcOp;` |
|      9012 |  3255 | `	sState.pTos = pTos;` |
|      9012 |  3256 | `	sState.pc = pc;` |
|      9012 |  3257 | `	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);` |
|      9012 |  3258 | `	pTos = sState.pTos;` |
|      9012 |  3259 | `	pc = sState.pc;` |
|      9012 |  3260 | `	if( rcOp == VM_OP_ABORT ){` |
|         3 |  3261 | `		goto Abort;` |
|      9010 |  3262 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|         3 |  3263 | `		goto Exception;` |
|         - |  3264 | `	}` |
|      9008 |  3265 | `	break;` |
|         - |  3266 | `					  }` |
|     62144 |  3267 | `case PH7_OP_ERR_CTRL:` |
|         - |  3268 | `	/*` |
|         - |  3269 | `	 * Error-control operator '@'. Emitted as a PAIR around the suppressed` |
|         - |  3270 | `	 * expression: iP1=1 opens the window (before the operand is evaluated),` |
|         - |  3271 | `	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),` |
|         - |  3272 | `	 * which went unnoticed only because the engine raised so few diagnostics;` |
|         - |  3273 | `	 * every warning/notice/deprecation the parity work added leaked straight` |
|         - |  3274 | ``	 * through `@`. The window nests, and php 8 does NOT let '@' swallow`` |
|         - |  3275 | `	 * fatals or exceptions — those unwind past the closing instruction, so` |
|         - |  3276 | `	 * the depth is reset on the exception path rather than decremented here.` |
|         - |  3277 | `	 */` |
|    124195 |  3278 | `	if( pInstr->iP1 ){` |
|     62204 |  3279 | `		pVm->nErrSuppress++;` |
|     93071 |  3280 | `	}else if( pVm->nErrSuppress > 0 ){` |
|     61996 |  3281 | `		pVm->nErrSuppress--;` |
|     30971 |  3282 | `	}` |
|    124195 |  3283 | `	break;` |
|         - |  3284 | `/*` |
|         - |  3285 | ` * IS_A * * *` |
|         - |  3286 | ` *` |
|         - |  3287 | ` * Pop the top two operands from the stack and check whether the first operand` |
|         - |  3288 | ` * is an object and is an instance of the second operand (which must be a string` |
|         - |  3289 | ` * holding a class name or an object).` |
|         - |  3290 | ` * Push TRUE on success. FALSE otherwise.` |
|         - |  3291 | ` */` |
|       651 |  3292 | `case PH7_OP_IS_A:{` |
|      1307 |  3293 | `	ph7_value *pNos = &pTos[-1];` |
|      1307 |  3294 | `	sxi32 iRes = 0; /* assume false by default */` |
|         - |  3295 | `#ifdef UNTRUST` |
|         - |  3296 | `	if( pNos < pStack ){` |
|         - |  3297 | `		goto Abort;` |
|         - |  3298 | `	}` |
|         - |  3299 | `#endif` |
|         - |  3300 | `	/* php screens the CLASS operand before it looks at the subject: zend_fetch_class` |
|         - |  3301 | `	 * takes an object or a string and refuses everything else with` |
|         - |  3302 | ``	 * `Class name must be a valid object or a string`, whatever is on the left --`` |
|         - |  3303 | ``	 * `5 instanceof $arr` and `$obj instanceof $arr` are the same Error. PHL answered`` |
|         - |  3304 | `	 * FALSE for every one of them, so a typo'd or wrongly-typed class expression read` |
|         - |  3305 | `` 	 * as a clean "no" and the program carried on. The same refusal already guards `::` `` |
|         - |  3306 | ``	 * (VmExecOpMember) and now `new` (VmExecOpNew); this is the third door. */`` |
|      1307 |  3307 | `	if( (pTos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|        17 |  3308 | `		VmPopOperand(&pTos,1);` |
|        17 |  3309 | `		PH7_MemObjRelease(pTos);` |
|        17 |  3310 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        17 |  3311 | `		pTos->nIdx = SXU32_HIGH;` |
|        17 |  3312 | `		rc = VmThrowFromVm(&(*pVm),"Error","Class name must be a valid object or a string",` |
|         - |  3313 | `			sizeof("Class name must be a valid object or a string")-1);` |
|        17 |  3314 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3315 | `			goto Abort;` |
|         - |  3316 | `		}` |
|        17 |  3317 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  3318 | `	}` |
|      1291 |  3319 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      1075 |  3320 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      1075 |  3321 | `		ph7_class *pClass = 0;` |
|         - |  3322 | `		/* Extract the target class */` |
|      1075 |  3323 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|         - |  3324 | `			/* Instance already loaded */` |
|         3 |  3325 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      1074 |  3326 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|         - |  3327 | `			/* self/static/parent, through the shared rail (PH7_VmResolveScopeName), and` |
|         - |  3328 | `			 * an ordinary name through the same non-autoloading extract as before.` |
|         - |  3329 | `			 *` |
|         - |  3330 | ``			 * This handler open-coded the three keywords and answered `self` with the`` |
|         - |  3331 | `			 * DECLARING class, which for a trait method is the TRAIT — and no object is` |
|         - |  3332 | ``			 * ever an instance of a trait, so `$x instanceof self` inside a trait method`` |
|         - |  3333 | `			 * was FALSE for every $x, including one of the very class that used it.` |
|         - |  3334 | ``			 * php composes a trait into the class, so `self` there is the USING class`` |
|         - |  3335 | ``			 * (PH7_VmPeekSelfClass states the rule once, and `self::`/`__CLASS__`/`` |
|         - |  3336 | ``			 * `new self` all already asked it). The same silent false answered inside a`` |
|         - |  3337 | `			 * CLOSURE declared in a trait method, whose stamped scope is the trait too.` |
|         - |  3338 | `			 *` |
|         - |  3339 | `			 * phpstan is where it showed: JustNullableTypeTrait::isSuperTypeOf() begins` |
|         - |  3340 | ``			 * `if ($type instanceof self)`, and with that never taken the trait fell`` |
|         - |  3341 | ``			 * through to `$type->isSubTypeOf($this)` — a mutual recursion with`` |
|         - |  3342 | `			 * IntegerRangeType::isSubTypeOf() that php terminates on the first line. */` |
|         - |  3343 | ``			/* Only a WRITTEN keyword (iP1) is one: `$x instanceof $c` over`` |
|         - |  3344 | ``			 * `$c = 'self'` names a class nothing can be called, and answers false. */`` |
|      1073 |  3345 | `			pClass = pInstr->iP1` |
|        69 |  3346 | `				? PH7_VmResolveScopeName(&(*pVm),` |
|        46 |  3347 | `					(const char *)SyBlobData(&pTos->sBlob),(sxu32)SyBlobLength(&pTos->sBlob))` |
|      1556 |  3348 | `				: PH7_VmExtractClass(&(*pVm),` |
|      1022 |  3349 | `					(const char *)SyBlobData(&pTos->sBlob),(sxu32)SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       534 |  3350 | `		}` |
|      1075 |  3351 | `		if( pClass ){` |
|         - |  3352 | `			/* Perform the query */` |
|      1065 |  3353 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       530 |  3354 | `		}` |
|       535 |  3355 | `	}` |
|         - |  3356 | `	/* Push result */` |
|      1291 |  3357 | `	VmPopOperand(&pTos,1);` |
|      1291 |  3358 | `	PH7_MemObjRelease(pTos);` |
|      1291 |  3359 | `	pTos->x.iVal = iRes;` |
|      1291 |  3360 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      1291 |  3361 | `	break;` |
|         - |  3362 | `				 }` |
|         - |  3363 |  |
|         - |  3364 | `/*` |
|         - |  3365 | ` * LOADC P1 P2 *` |
|         - |  3366 | ` *` |
|         - |  3367 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|         - |  3368 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|         - |  3369 | ` */` |
|  38925571 |  3370 | `case PH7_OP_LOADC: {` |
|         - |  3371 | `	ph7_value *pObj;` |
|         - |  3372 | `	/* Reserve a room */` |
|  77857125 |  3373 | `	pTos++;` |
|  77857125 |  3374 | `	if( pInstr->iP1 & PH7_LOADC_NOKEY ){` |
|         - |  3375 | `		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */` |
|    788245 |  3376 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|    788245 |  3377 | `		SyBlobReset(&pTos->sBlob);` |
|    788245 |  3378 | `		pTos->iFlags \|= MEMOBJ_AUX_NOKEY;` |
|    788245 |  3379 | `		pTos->nIdx = SXU32_HIGH;` |
|    788245 |  3380 | `		break;` |
|         - |  3381 | `	}` |
|  77068885 |  3382 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  77068885 |  3383 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|         - |  3384 | `			SyHashEntry *pEntry;` |
|         - |  3385 | `			/* The name php looks up FIRST for an unqualified constant is decided at` |
|         - |  3386 | ``			 * COMPILE time and travels in p3: a `use const` import's FQN, or`` |
|         - |  3387 | ``			 * `current-namespace\NAME`. Absent (global scope, or an already-qualified`` |
|         - |  3388 | `			 * literal), the bare literal is the only name there is. Resolving it here` |
|         - |  3389 | `			 * rather than against the RUNTIME namespace is what makes a function keep` |
|         - |  3390 | `			 * its own namespace when it is called from another one, and what lets a` |
|         - |  3391 | `			 * namespaced constant shadow a global one of the same short name. */` |
|    179185 |  3392 | `			const char *zCand = (const char *)pInstr->p3;` |
|    179185 |  3393 | `			const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|    179185 |  3394 | `			sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|         - |  3395 | `			/* Both names are fixed by the instruction, so which one wins and what it` |
|         - |  3396 | `			 * resolves to can only change when hConstant does. A site that has already` |
|         - |  3397 | `			 * answered under the current generation answers again without hashing` |
|         - |  3398 | `			 * either -- see PH7_VmConstSiteAnswer for what that was costing. */` |
|    179185 |  3399 | `			pEntry = PH7_VmConstSiteAnswer(&(*pVm),pInstr);` |
|    179185 |  3400 | `			if( pEntry == 0 ){` |
|     11060 |  3401 | `				if( zCand ){` |
|        86 |  3402 | `					pEntry = PH7_VmConstantFetch(&(*pVm),zCand,SyStrlen(zCand),1);` |
|        41 |  3403 | `				}` |
|         - |  3404 | `				/* The GLOBAL step — skipped when the candidate came from an import,` |
|         - |  3405 | `				 * which php resolves without any fallback. */` |
|     11060 |  3406 | `				if( pEntry == 0 && (pInstr->iP1 & PH7_LOADC_NOGLOBAL) == 0 ){` |
|     10992 |  3407 | `					pEntry = PH7_VmConstantFetch(&(*pVm),zLit,nLit,1);` |
|      5405 |  3408 | `				}` |
|     11060 |  3409 | `				PH7_VmConstSiteRecord(&(*pVm),pInstr,pEntry);` |
|      5439 |  3410 | `			}` |
|    179185 |  3411 | `			if( pEntry ){` |
|    178979 |  3412 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|         - |  3413 | `				/* Set a NULL default value */` |
|    178979 |  3414 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    178979 |  3415 | `				SyBlobReset(&pTos->sBlob);` |
|         - |  3416 | `				/* Invoke the callback and deal with the expanded value */` |
|    178979 |  3417 | `				VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|         - |  3418 | `				/* Mark as constant */` |
|    178979 |  3419 | `				pTos->nIdx = SXU32_HIGH;` |
|    178979 |  3420 | `				break;` |
|         - |  3421 | `			}` |
|         - |  3422 | `			{` |
|         - |  3423 | `				/*` |
|         - |  3424 | `				 * php 8 has no bare-word fallback: an unresolved constant is a catchable` |
|         - |  3425 | `				 * Error, not its own name as a string. PH7 answered "X" for an unknown` |
|         - |  3426 | `				 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —` |
|         - |  3427 | `				 * silently became a string and flowed on. php names the name it looked` |
|         - |  3428 | `				 * for FIRST, so the message reports the candidate when there was one` |
|         - |  3429 | ``				 * ("Undefined constant \"B\NOPE\"" inside `namespace B;`).`` |
|         - |  3430 | `				 *` |
|         - |  3431 | `				 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call` |
|         - |  3432 | ``				 * boundary, so neither a bare `break` nor `goto Exception` is correct`` |
|         - |  3433 | `				 * here (see the macro).` |
|         - |  3434 | `				 */` |
|         - |  3435 | `				SyBlob sMsg;` |
|       211 |  3436 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       211 |  3437 | `				if( zCand ){` |
|        11 |  3438 | `					SyBlobFormat(&sMsg,"Undefined constant \"%s\"",zCand);` |
|         6 |  3439 | `				}else{` |
|       201 |  3440 | `					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);` |
|         - |  3441 | `				}` |
|       211 |  3442 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       211 |  3443 | `				SyBlobReset(&pTos->sBlob);` |
|       211 |  3444 | `				pTos->nIdx = SXU32_HIGH;` |
|       314 |  3445 | `				rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|       103 |  3446 | `					SyBlobLength(&sMsg));` |
|       211 |  3447 | `				SyBlobRelease(&sMsg);` |
|       211 |  3448 | `				if( rc == SXERR_ABORT ){` |
|        84 |  3449 | `					goto Abort;` |
|         - |  3450 | `				}` |
|       143 |  3451 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  3452 | `			}` |
|         - |  3453 | `		}` |
|  76889705 |  3454 | `		PH7_MemObjLoad(pObj,pTos);` |
|  38448283 |  3455 | `	}else{` |
|         - |  3456 | `		/* Set a NULL value */` |
|       ! 0 |  3457 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|         - |  3458 | `	}` |
|         - |  3459 | `	/* Mark as constant */` |
|  76889705 |  3460 | `	pTos->nIdx = SXU32_HIGH;` |
|  76889705 |  3461 | `	break;` |
|         - |  3462 | `				  }` |
|         - |  3463 | `/*` |
|         - |  3464 | ` * LOAD: P1 * P3` |
|         - |  3465 | ` *` |
|         - |  3466 | ` * Load a variable where it's name is taken from the top of the stack or` |
|         - |  3467 | ` * from the P3 operand.` |
|         - |  3468 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|         - |  3469 | ` * the variable if non existent and push the NULL constant instead.` |
|         - |  3470 | ` */` |
|  42159638 |  3471 | `case PH7_OP_LOAD:{` |
|         - |  3472 | `	ph7_value *pObj;` |
|         - |  3473 | `	SyString sName;` |
|  84347180 |  3474 | `	if( pInstr->p3 == 0 ){` |
|         - |  3475 | `		/* Take the variable name from the top of the stack */` |
|         - |  3476 | `#ifdef UNTRUST` |
|         - |  3477 | `		if( pTos < pStack ){` |
|         - |  3478 | `			goto Abort;` |
|         - |  3479 | `		}` |
|         - |  3480 | `#endif` |
|         - |  3481 | `		/* Force a string cast — variable-variable name $$arr (user-visible) */` |
|         - |  3482 | `		{` |
|        46 |  3483 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|  42159683 |  3484 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|         - |  3485 | `		}` |
|        44 |  3486 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        24 |  3487 | `	}else{` |
|  84347138 |  3488 | `		if( pInstr->nAux == 0 ){` |
|         - |  3489 | `			/* Measured once: the name is a compile-time buffer that never changes.` |
|         - |  3490 | `			 * VmNumberLocals fills this in for a body it walks; this is the door for` |
|         - |  3491 | `			 * one it does not (an include, an eval, a mini-program). */` |
|     62756 |  3492 | `			pInstr->nAux = (sxu32)SyStrlen((const char *)pInstr->p3);` |
|     31370 |  3493 | `		}` |
|  84347138 |  3494 | `		SyStringInitFromBuf(&sName,pInstr->p3,pInstr->nAux);` |
|         - |  3495 | `		/* Reserve a room for the target object */` |
|  84347138 |  3496 | `		pTos++;` |
|  84347138 |  3497 | `		if( pInstr->iP2 == 5 ){` |
|         - |  3498 | `			/* An operand php's frameless instruction reads itself, at the call, after` |
|         - |  3499 | `			 * the arguments that follow it ran: leave it unread whether or not the` |
|         - |  3500 | `			 * variable exists yet, for PH7_VmResolveDeferredArgs to read then. */` |
|    113375 |  3501 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    113375 |  3502 | `			pTos->nIdx = SXU32_HIGH;` |
|    113375 |  3503 | `			pTos->iFlags \|= MEMOBJ_AUX_DEFERRED;` |
|    113375 |  3504 | `			pTos->x.pOther = pInstr->p3;` |
|    113375 |  3505 | `			break;` |
|         - |  3506 | `		}` |
|         - |  3507 | `	}` |
|         - |  3508 | `	{` |
|         - |  3509 | `	/* A name the compiler put in the instruction has a NUMBER in this body, so the` |
|         - |  3510 | `	 * frame answers it out of an array (PH7_VmExtractVarSlot); a variable-variable's` |
|         - |  3511 | `	 * name is a string on the stack and gets the plain door. */` |
| 129853226 |  3512 | `	int bThis = ( sName.nByte == sizeof("this")-1` |
|  45646396 |  3513 | `	           && SyMemcmp(sName.zString,"this",sizeof("this")-1) == 0` |
|  87749278 |  3514 | `	           && pInstr->iP2 != 1 /* isset()/empty() ask a QUESTION -- see below */ );` |
|  84233808 |  3515 | `	if( bThis \|\| pInstr->iP2 == 2 ){` |
|         - |  3516 | ``		/* One PEEK (no create) answers both questions below -- `$this` in a frame`` |
|         - |  3517 | `		 * with no receiver, and php's read-before-write warning -- where two` |
|         - |  3518 | `		 * separate lookups used to ask the same table the same thing twice for` |
|         - |  3519 | ``		 * every `$this` in every method body. */`` |
|  31255555 |  3520 | `		ph7_value *pPeek = pInstr->p3` |
|  20837562 |  3521 | `			? PH7_VmExtractVarSlot(&(*pVm),&sName,FALSE,pInstr->nSite,aInstr)` |
|  10417988 |  3522 | `			: VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE);` |
|  20837567 |  3523 | `		if( pPeek == 0 ){` |
|        22 |  3524 | `			if( bThis ){` |
|         - |  3525 | ``				/* `$this` is not a variable: a frame with no receiver answers a READ`` |
|         - |  3526 | `				 * of it with php's catchable Error, not the undefined-variable` |
|         - |  3527 | ``				 * warning and a NULL. The null was the hazard -- `$this->m()` in a`` |
|         - |  3528 | `				 * plain function became "Call to a member function m() on null",` |
|         - |  3529 | ``				 * `var_dump($this)` printed NULL, and `f($this)` passed one on --`` |
|         - |  3530 | `				 * each a wrong ANSWER a few frames from the mistake. Asked BEFORE the` |
|         - |  3531 | ``				 * extract so the vivifying contexts (`$this ?? 'd'`, which php throws`` |
|         - |  3532 | `				 * for even though it swallows an ordinary undefined variable) reach it` |
|         - |  3533 | ``				 * too; `isset($this)`/`empty($this)` are php's one exemption, and they`` |
|         - |  3534 | `				 * answer false/true in silence. */` |
|        13 |  3535 | `				rc = VmThrowFromVm(&(*pVm),"Error","Using $this when not in object context",` |
|         - |  3536 | `					sizeof("Using $this when not in object context")-1);` |
|        13 |  3537 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3538 | `					goto Abort;` |
|         - |  3539 | `				}` |
|        29 |  3540 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       ! 0 |  3541 | `			}else{` |
|         - |  3542 | ``				/* Read-modify-write target (`$x++`, `$x .= 'a'`): php reads the`` |
|         - |  3543 | `				 * variable before writing, so it warns when it does not exist and` |
|         - |  3544 | `				 * THEN seeds it. The load below still creates the slot the operator` |
|         - |  3545 | ``				 * needs. A plain `=` never gets here -- it writes without reading,`` |
|         - |  3546 | `				 * and stays silent, as php does. */` |
|        10 |  3547 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|         - |  3548 | `			}` |
|         4 |  3549 | `		}` |
|  10419568 |  3550 | `	}` |
|         - |  3551 | `	/* Extract the requested memory object */` |
| 126336672 |  3552 | `	pObj = pInstr->p3` |
|  84233751 |  3553 | `		? PH7_VmExtractVarSlot(&(*pVm),&sName,pInstr->iP1 != 1,pInstr->nSite,aInstr)` |
|  42102896 |  3554 | `		: VmExtractMemObj(&(*pVm),&sName,TRUE,pInstr->iP1 != 1);` |
|         - |  3555 | `	}` |
|  84233796 |  3556 | `	if( pObj == 0 ){` |
|       695 |  3557 | `		if( pInstr->iP1 ){` |
|         - |  3558 | `			/* Reading a variable that does not exist. php raises E_WARNING` |
|         - |  3559 | `			 * "Undefined variable $x" and evaluates it as NULL; PHL used to` |
|         - |  3560 | `			 * yield NULL silently, which hid typo'd names. iP2 marks the reads` |
|         - |  3561 | `			 * that must stay quiet (isset/empty — see PH7_CompileVariable);` |
|         - |  3562 | `			 * vivifying contexts never get here because they pass iP1 = 0. */` |
|       695 |  3563 | `			if( pInstr->iP2 == 0 ){` |
|       215 |  3564 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|       105 |  3565 | `			}` |
|         - |  3566 | `			/* Variable not found,load NULL */` |
|       695 |  3567 | `			if( !pInstr->p3 ){` |
|        10 |  3568 | `				PH7_MemObjRelease(pTos);` |
|         6 |  3569 | `			}else{` |
|       687 |  3570 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|         - |  3571 | `			}` |
|       695 |  3572 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|       695 |  3573 | `			if( pInstr->iP2 == 3 ){` |
|         - |  3574 | `				/* D1 deferred call argument: the variable does not exist, but we do not` |
|         - |  3575 | `				 * know yet whether the callee wants it by-ref (materialize + bind) or` |
|         - |  3576 | `				 * by-value (warn + pass NULL). Tag the slot and stash the variable name` |
|         - |  3577 | `				 * (a VM-lifetime bytecode string, nothing to free) so OP_CALL's` |
|         - |  3578 | `				 * PH7_VmResolveDeferredArgs can decide once the callee is resolved. */` |
|       379 |  3579 | `				pTos->iFlags \|= MEMOBJ_AUX_DEFERRED;` |
|       379 |  3580 | `				pTos->x.pOther = pInstr->p3;` |
|       187 |  3581 | `			}` |
|       695 |  3582 | `			break;` |
|       ! 0 |  3583 | `		}else{` |
|         - |  3584 | `			/* Fatal error */` |
|       ! 0 |  3585 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|       ! 0 |  3586 | `			goto Abort;` |
|         - |  3587 | `		}` |
|         - |  3588 | `	}` |
|         - |  3589 | `	/* Load variable contents */` |
|  84233106 |  3590 | `	PH7_MemObjLoad(pObj,pTos);` |
|  84233106 |  3591 | `	pTos->nIdx = pObj->nIdx;` |
|  84233106 |  3592 | `	break;` |
|         - |  3593 | `				   }` |
|         - |  3594 | `/*` |
|         - |  3595 | ` * LOAD_MAP P1 * *` |
|         - |  3596 | ` *` |
|         - |  3597 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|         - |  3598 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|         - |  3599 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|         - |  3600 | ` */` |
|    156123 |  3601 | `case PH7_OP_LOAD_MAP: {` |
|         - |  3602 | `	VmOpRc rcOp;` |
|    312107 |  3603 | `	sState.pTos = pTos;` |
|    312107 |  3604 | `	sState.pc = pc;` |
|    312107 |  3605 | `	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);` |
|    312107 |  3606 | `	pTos = sState.pTos;` |
|    312107 |  3607 | `	pc = sState.pc;` |
|    312107 |  3608 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  3609 | `		goto Abort;` |
|    312107 |  3610 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        45 |  3611 | `		goto Exception;` |
|         - |  3612 | `	}` |
|    312065 |  3613 | `	break;` |
|         - |  3614 | `					  }` |
|         - |  3615 | `/*` |
|         - |  3616 | ` * LOAD_LIST: P1 * *` |
|         - |  3617 | ` *` |
|         - |  3618 | ` * Assign hashmap entries values to the top P1 entries.` |
|         - |  3619 | ` * This is the VM implementation of the list() PHP construct.` |
|         - |  3620 | ` * Caveats:` |
|         - |  3621 | ` *  This implementation support only a single nesting level.` |
|         - |  3622 | ` */` |
|    196768 |  3623 | `case PH7_OP_LOAD_LIST: {` |
|         - |  3624 | `	VmOpRc rcOp;` |
|    393534 |  3625 | `	sState.pTos = pTos;` |
|    393534 |  3626 | `	sState.pc = pc;` |
|    393534 |  3627 | `	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);` |
|    393534 |  3628 | `	pTos = sState.pTos;` |
|    393534 |  3629 | `	pc = sState.pc;` |
|    393534 |  3630 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  3631 | `		goto Abort;` |
|    393534 |  3632 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|         7 |  3633 | `		goto Exception;` |
|         - |  3634 | `	}` |
|    393528 |  3635 | `	break;` |
|         - |  3636 | `					  }` |
|         - |  3637 | `/*` |
|         - |  3638 | ` * LOAD_IDX: P1 P2 *` |
|         - |  3639 | ` *` |
|         - |  3640 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|         - |  3641 | ` * from the stack.` |
|         - |  3642 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|         - |  3643 | ` * instead.` |
|         - |  3644 | ` */` |
|    802775 |  3645 | `case PH7_OP_LOAD_IDX: {` |
|         - |  3646 | `	VmOpRc rcOp;` |
|   1611726 |  3647 | `	sState.pTos = pTos;` |
|   1611726 |  3648 | `	sState.pc = pc;` |
|   1611726 |  3649 | `	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);` |
|   1611726 |  3650 | `	pTos = sState.pTos;` |
|   1611726 |  3651 | `	pc = sState.pc;` |
|   1611726 |  3652 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  3653 | `		goto Abort;` |
|   1611726 |  3654 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       101 |  3655 | `		goto Exception;` |
|         - |  3656 | `	}` |
|   1611628 |  3657 | `	break;` |
|         - |  3658 | `					  }` |
|         - |  3659 | `/*` |
|         - |  3660 | ` * LOAD_CLOSURE * * P3` |
|         - |  3661 | ` *` |
|         - |  3662 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|         - |  3663 | ` * name in the stack.` |
|         - |  3664 | ` */` |
|     11882 |  3665 | `case PH7_OP_LOAD_CLOSURE: {` |
|         - |  3666 | `	VmOpRc rcOp;` |
|     23526 |  3667 | `	sState.pTos = pTos;` |
|     23526 |  3668 | `	sState.pc = pc;` |
|     23526 |  3669 | `	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);` |
|     23526 |  3670 | `	pTos = sState.pTos;` |
|     23526 |  3671 | `	pc = sState.pc;` |
|     23526 |  3672 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  3673 | `		goto Abort;` |
|     23526 |  3674 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       ! 0 |  3675 | `		goto Exception;` |
|         - |  3676 | `	}` |
|     23526 |  3677 | `	break;` |
|         - |  3678 | `					  }` |
|         - |  3679 | `/*` |
|         - |  3680 | ` * LOAD_FCC P1 * *` |
|         - |  3681 | ` *` |
|         - |  3682 | ` * First-class callable: wrap the callee in a Closure object instead of calling it.` |
|         - |  3683 | ` *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS` |
|         - |  3684 | ` *           (from the callee's OP_LOADC). Replace it in place with a Closure whose` |
|         - |  3685 | ` *           $__fn is that name; the existing string-callable dispatch resolves it.` |
|         - |  3686 | ` *           (OOM degrades to leaving the name string on the stack — still callable.)` |
|         - |  3687 | ` *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name` |
|         - |  3688 | ` *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds` |
|         - |  3689 | ` *           $this (scope = its class); a class-name-string target is a static callable` |
|         - |  3690 | ` *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —` |
|         - |  3691 | ` *           the popped target leaves no name string to keep.)` |
|         - |  3692 | ` */` |
|       276 |  3693 | `case PH7_OP_LOAD_FCC:{` |
|       557 |  3694 | `	if( pInstr->iP1 == 1 ){` |
|         - |  3695 | ``		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)`` |
|         - |  3696 | `		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —` |
|         - |  3697 | `		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,` |
|         - |  3698 | ``		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a`` |
|         - |  3699 | `		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is` |
|         - |  3700 | `		 * (graceful degradation), and so is the original on OOM — still whatever it was. */` |
|         - |  3701 | `		ph7_class_instance *pCloObj;` |
|         - |  3702 | `		sxi32 nFccBrc;` |
|         - |  3703 | `		const void *pFccRes;` |
|       219 |  3704 | `		if( VmValueIsClosure(pVm, pTos) ){` |
|        16 |  3705 | `			break;` |
|         - |  3706 | `		}` |
|         - |  3707 | `		/* php's global fallback for an UNQUALIFIED function name written inside a` |
|         - |  3708 | `		 * namespace: the current namespace first, the global one after. The compiler` |
|         - |  3709 | `		 * qualified this name and marks the instruction (iP2==1) when it did, so the` |
|         - |  3710 | `		 * fallback happens here — the OP_CALL path does the same thing from its arg map,` |
|         - |  3711 | ``		 * which an FCC has none of. `strlen(...)` in a namespaced file was`` |
|         - |  3712 | ``		 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|       200 |  3713 | `		if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING)` |
|        23 |  3714 | `			&& !PH7_VmIsCallable(pVm,pTos,TRUE) ){` |
|        13 |  3715 | `			const char *zFccName = (const char *)SyBlobData(&pTos->sBlob);` |
|        13 |  3716 | `			sxu32 nFccName = SyBlobLength(&pTos->sBlob);` |
|        13 |  3717 | `			const char *zFccShort = zFccName;` |
|         - |  3718 | `			sxu32 iFccPos;` |
|       209 |  3719 | `			for( iFccPos = 0 ; iFccPos < nFccName ; ++iFccPos ){` |
|       199 |  3720 | `				if( zFccName[iFccPos] == '\\' ){` |
|        13 |  3721 | `					zFccShort = &zFccName[iFccPos + 1];` |
|         5 |  3722 | `				}` |
|       101 |  3723 | `			}` |
|        13 |  3724 | `			if( zFccShort != zFccName ){` |
|         - |  3725 | `				ph7_value sFccShort;` |
|        13 |  3726 | `				PH7_MemObjInit(pVm,&sFccShort);` |
|        18 |  3727 | `				PH7_MemObjStringAppend(&sFccShort,zFccShort,` |
|        10 |  3728 | `					(sxu32)(nFccName - (sxu32)(zFccShort - zFccName)));` |
|        13 |  3729 | `				if( PH7_VmIsCallable(pVm,&sFccShort,TRUE) ){` |
|        11 |  3730 | `					PH7_MemObjStore(&sFccShort,pTos);` |
|         4 |  3731 | `				}` |
|        13 |  3732 | `				PH7_MemObjRelease(&sFccShort);` |
|         5 |  3733 | `			}` |
|         5 |  3734 | `		}` |
|         - |  3735 | `		/* The array shape's class lookup can run an autoloader that throws; php propagates` |
|         - |  3736 | `		 * THAT exception and never reports the callable bad, exactly as at the OP_CALL sites. */` |
|       205 |  3737 | `		nFccBrc = pVm->nBoundaryRc;` |
|       205 |  3738 | `		pFccRes = (const void *)pVm->pResumeFrame;` |
|       205 |  3739 | `		pCloObj = VmFccWrapValue(pVm, pTos, FALSE);` |
|       205 |  3740 | `		if( pCloObj ){` |
|       175 |  3741 | `			pCloObj->iFlags \|= VM_INSTANCE_FCC_SYNTAX;` |
|       175 |  3742 | `			PH7_MemObjRelease(pTos);` |
|         - |  3743 | `			/* The fresh instance's own reference is this stack slot's (see OP_LOAD_CLOSURE) */` |
|       175 |  3744 | `			pTos->x.pOther = pCloObj;` |
|       175 |  3745 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|        90 |  3746 | `		}else{` |
|         - |  3747 | `			/* php refuses a non-callable HERE, with the direct dispatch's own wording — the` |
|         - |  3748 | ``			 * `(...)` does not make a bad callable acceptable, it just defers the call. */`` |
|         - |  3749 | `			char zFccMsg[192];` |
|        31 |  3750 | `			const char *zFccBad = VmFccValueError(&(*pVm),pTos,zFccMsg,sizeof(zFccMsg));` |
|        31 |  3751 | `			int bFccRaised = PH7_VmClassLookupRaised(&(*pVm),nFccBrc,pFccRes);` |
|        31 |  3752 | `			if( zFccBad \|\| bFccRaised ){` |
|         - |  3753 | `				sxi32 rcFcc;` |
|        31 |  3754 | `				PH7_MemObjRelease(pTos);` |
|        31 |  3755 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        31 |  3756 | `				pTos->nIdx = SXU32_HIGH;` |
|        31 |  3757 | `				if( bFccRaised ){` |
|       ! 0 |  3758 | `					rcFcc = pVm->nBoundaryRc;` |
|       ! 0 |  3759 | `					pVm->nBoundaryRc = 0;` |
|       ! 0 |  3760 | `					if( rcFcc == PH7_ABORT ){ goto Abort; }` |
|       ! 0 |  3761 | `					rc = PH7_EXCEPTION;` |
|        15 |  3762 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  3763 | `				}` |
|        31 |  3764 | `				rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccBad,(sxu32)SyStrlen(zFccBad));` |
|        31 |  3765 | `				if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|        31 |  3766 | `				rc = rcFcc;` |
|        57 |  3767 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  3768 | `			}` |
|         - |  3769 | `		}` |
|        90 |  3770 | `	}else{` |
|         - |  3771 | `		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]` |
|         - |  3772 | `		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its` |
|         - |  3773 | `		 * class); a class-name string target is a static callable (scope = that class). */` |
|       343 |  3774 | `		ph7_value *pTarget = &pTos[-1];` |
|         - |  3775 | `		SyString sName;` |
|         - |  3776 | `		ph7_class_instance *pCloObj;` |
|       343 |  3777 | `		ph7_class *pFccCls = 0;` |
|       343 |  3778 | `		ph7_class_instance *pFccRecv = 0;` |
|       343 |  3779 | `		const char *zFccErr = 0;` |
|         - |  3780 | `		char zFccMsg[192];` |
|       343 |  3781 | `		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));` |
|       343 |  3782 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|       135 |  3783 | `			pFccRecv = (ph7_class_instance *)pTarget->x.pOther;` |
|       135 |  3784 | `			pFccCls = pFccRecv->pClass;` |
|       135 |  3785 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pFccCls) ){` |
|         - |  3786 | ``				/* `$inc->m(...)` resolves the method at CREATION, so php's`` |
|         - |  3787 | `				 * incomplete-object call Error is raised here, not at a later` |
|         - |  3788 | `				 * invocation. */` |
|         - |  3789 | `				SyBlob sIncErr;` |
|         - |  3790 | `				sxi32 rcInc;` |
|         3 |  3791 | `				SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|         3 |  3792 | `				PH7_VmIncompleteMsg(&(*pVm),pFccRecv,"call a method",&sIncErr);` |
|         3 |  3793 | `				VmPopOperand(&pTos,1);       /* the method name */` |
|         3 |  3794 | `				PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|         3 |  3795 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|         3 |  3796 | `				pTos->nIdx = SXU32_HIGH;` |
|         4 |  3797 | `				rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|         1 |  3798 | `					SyBlobLength(&sIncErr));` |
|         3 |  3799 | `				SyBlobRelease(&sIncErr);` |
|         3 |  3800 | `				if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|         3 |  3801 | `				rc = rcInc;` |
|         3 |  3802 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         5 |  3803 | `			}` |
|       277 |  3804 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|         - |  3805 | ``			/* Static `T::m(...)`: resolve T (incl. a WRITTEN self/static/parent) to the`` |
|         - |  3806 | `			 * real class now, so the closure binds the concrete scope (matching PHP). */` |
|       213 |  3807 | `			pFccCls = pInstr->bDiscard ? VmFccResolveScope(pVm, pTarget)` |
|       220 |  3808 | `				: PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       116 |  3809 | `					(sxu32)SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       104 |  3810 | `		}` |
|       341 |  3811 | `		if( pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING) ){` |
|         - |  3812 | `			/* php resolves the member HERE, through the same lookup the call would use:` |
|         - |  3813 | `			 * every refusal a call would raise is raised at CREATION, and a non-static` |
|         - |  3814 | `			 * method named through a class binds the calling frame's own $this. */` |
|       341 |  3815 | `			ph7_class_instance *pRecvOut = 0;` |
|       341 |  3816 | `			if( pInstr->iP2 == 2 && pFccCls ){` |
|         - |  3817 | ``				/* A LITERAL `X::__construct(...)`: the class's constructor, as for the call`` |
|         - |  3818 | `				 * (vm_ops_oo.c) -- not a method lookup. */` |
|        27 |  3819 | `				zFccErr = VmFccCtorError(&(*pVm),pFccCls,&pRecvOut,zFccMsg,sizeof(zFccMsg));` |
|        14 |  3820 | `			}else{` |
|       470 |  3821 | `				zFccErr = VmFccMemberError(&(*pVm),pFccCls,` |
|       310 |  3822 | `					(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|       155 |  3823 | `					SyStringData(&sName),SyStringLength(&sName),` |
|       310 |  3824 | `					(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|       155 |  3825 | `					&pRecvOut,zFccMsg,sizeof(zFccMsg));` |
|         - |  3826 | `			}` |
|       341 |  3827 | `			if( pRecvOut ){` |
|        45 |  3828 | ``				pFccRecv = pRecvOut; /* the receiver php binds into a `C::m(...)` callable */`` |
|        21 |  3829 | `			}` |
|       168 |  3830 | `		}` |
|       341 |  3831 | `		if( zFccErr ){` |
|         - |  3832 | `			sxi32 rcFcc;` |
|        51 |  3833 | `			VmPopOperand(&pTos,1);       /* the method name */` |
|        51 |  3834 | `			PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|        51 |  3835 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        51 |  3836 | `			pTos->nIdx = SXU32_HIGH;` |
|        51 |  3837 | `			rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccErr,(sxu32)SyStrlen(zFccErr));` |
|        51 |  3838 | `			if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|        51 |  3839 | `			rc = rcFcc;` |
|        53 |  3840 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  3841 | `		}` |
|       293 |  3842 | `		if( pFccCls == 0 ){` |
|       ! 0 |  3843 | `			pCloObj = 0;` |
|       293 |  3844 | `		}else if( pFccRecv ){` |
|         - |  3845 | `` 			/* The scope is the class the member was RESOLVED in, which for `parent::m(...)` `` |
|         - |  3846 | ``			 * or `A::m(...)` is not the receiver's: php keeps that function, and the unwrap`` |
|         - |  3847 | `			 * reads this to call it rather than the receiver's override (VmClosureUnwrap). */` |
|       165 |  3848 | `			pCloObj = VmCreateClosure(pVm, &sName, pFccRecv, &pFccCls->sName);` |
|        85 |  3849 | `		}else{` |
|       133 |  3850 | `			pCloObj = VmCreateClosure(pVm, &sName, 0, &pFccCls->sName);` |
|       133 |  3851 | `			if( pCloObj && (pTarget->iFlags & MEMOBJ_STRING) ){` |
|       133 |  3852 | `				VmFccForwardCalled(&(*pVm),pCloObj,pFccCls,pTarget,&sName);` |
|        64 |  3853 | `			}` |
|         - |  3854 | `		}` |
|       293 |  3855 | `		if( pCloObj ){` |
|         - |  3856 | ``			/* `$o->m(...)` / `C::m(...)` names a METHOD, whatever the class turns out to`` |
|         - |  3857 | `			 * declare: the unwrap must not go looking for a FUNCTION of that name, and a` |
|         - |  3858 | `			 * name the class answers only through __call is still a method call. */` |
|       293 |  3859 | `			pCloObj->iFlags \|= VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SYNTAX;` |
|         - |  3860 | `			/* The screen above already ran, HERE, where php runs it — so record that this` |
|         - |  3861 | `			 * closure's callee is settled and the invocation must not re-decide it. A name` |
|         - |  3862 | `			 * that resolved to the catch-all instead keeps routing there. */` |
|       293 |  3863 | `			if( PH7_VmFccMethodIsDirect(&(*pVm),pFccCls,SyStringData(&sName),SyStringLength(&sName)) ){` |
|       211 |  3864 | `				pCloObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       103 |  3865 | `			}` |
|       144 |  3866 | `		}` |
|         - |  3867 | `		/* Pop the method name and the target, push the Closure. */` |
|       293 |  3868 | `		PH7_MemObjRelease(pTos);` |
|       293 |  3869 | `		pTos--;` |
|       293 |  3870 | `		PH7_MemObjRelease(pTos);` |
|       293 |  3871 | `		if( pCloObj ){` |
|         - |  3872 | `			/* The fresh instance's own reference is this stack slot's (see OP_LOAD_CLOSURE) */` |
|       293 |  3873 | `			pTos->x.pOther = pCloObj;` |
|       293 |  3874 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       149 |  3875 | `		}else{` |
|       ! 0 |  3876 | `			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */` |
|         - |  3877 | `		}` |
|         - |  3878 | `	}` |
|       463 |  3879 | `	break;` |
|         - |  3880 | `					 }` |
|         - |  3881 | `/*` |
|         - |  3882 | ` * STORE * P2 P3` |
|         - |  3883 | ` *` |
|         - |  3884 | ` * Perform a store (Assignment) operation.` |
|         - |  3885 | ` */` |
|    793781 |  3886 | `case PH7_OP_STORE: {` |
|         - |  3887 | `	ph7_value *pObj;` |
|         - |  3888 | `	SyString sName;` |
|         - |  3889 | `#ifdef UNTRUST` |
|         - |  3890 | `	if( pTos < pStack ){` |
|         - |  3891 | `		goto Abort;` |
|         - |  3892 | `	}` |
|         - |  3893 | `#endif` |
|   1593064 |  3894 | `	if( pInstr->iP2 ){` |
|         - |  3895 | `		sxu32 nIdx;` |
|         - |  3896 | `		sxi32 rcT;` |
|         - |  3897 | `		/* Member store operation */` |
|    105607 |  3898 | `		nIdx = pTos->nIdx;` |
|    105607 |  3899 | `		VmPopOperand(&pTos,1);` |
|    105607 |  3900 | `		if( pVm->pMagicSetThis ){` |
|         - |  3901 | `			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain` |
|         - |  3902 | `			 * store to a missing/inaccessible property on a class declaring __set.` |
|         - |  3903 | `			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on` |
|         - |  3904 | `			 * the stack as the assignment expression's result — php's semantics` |
|         - |  3905 | `			 * (no property is created; a throw rides the boundary rail). */` |
|      1127 |  3906 | `			ph7_class_instance *pSetThis = pVm->pMagicSetThis;` |
|         - |  3907 | `			SyString sSetName;` |
|      1127 |  3908 | `			pVm->pMagicSetThis = 0;` |
|      1127 |  3909 | `			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));` |
|      1127 |  3910 | `			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);` |
|      1127 |  3911 | `			PH7_ClassInstanceUnref(pSetThis);` |
|      1127 |  3912 | `			SyBlobReset(&pVm->sMagicSetName);` |
|      1127 |  3913 | `			break;` |
|         - |  3914 | `		}` |
|    104485 |  3915 | `		if( pVm->pHookSetThis ){` |
|         - |  3916 | `			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a` |
|         - |  3917 | `			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with` |
|         - |  3918 | `			 * the rvalue (which stays on the stack as the assignment expression's` |
|         - |  3919 | ``			 * result); a `set => expr` hook's return value is stored into the`` |
|         - |  3920 | `			 * BACKING slot with the ordinary typed enforcement. A property with` |
|         - |  3921 | `			 * only a get hook is php's catchable "is read-only" Error. */` |
|        67 |  3922 | `			ph7_class_instance *pHThis = pVm->pHookSetThis;` |
|        67 |  3923 | `			ph7_class_attr *pHAttr = pVm->pHookSetAttr;` |
|        67 |  3924 | `			sxu32 nBackIdx = pVm->nHookSetIdx;` |
|         - |  3925 | `			sxi32 rcHs;` |
|        67 |  3926 | `			pVm->pHookSetThis = 0;` |
|        67 |  3927 | `			pVm->pHookSetAttr = 0;` |
|        67 |  3928 | `			pVm->nHookSetIdx = SXU32_HIGH;` |
|        67 |  3929 | `			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);` |
|        67 |  3930 | `			PH7_ClassInstanceUnref(pHThis);` |
|        67 |  3931 | `			if( rcHs == PH7_ABORT ){` |
|       ! 0 |  3932 | `				goto Abort;` |
|         - |  3933 | `			}` |
|        67 |  3934 | `			break;` |
|         - |  3935 | `		}` |
|    104421 |  3936 | `		if( nIdx == SXU32_HIGH ){` |
|         - |  3937 | `			/* No slot behind the property: the receiver is a TEMPORARY nothing else` |
|         - |  3938 | ``			 * holds (`mk()->p = 5` on a freshly built object, `(new A)->p` where the`` |
|         - |  3939 | `			 * compile-time screen lets it through). php performs the write on the` |
|         - |  3940 | `			 * doomed object and says nothing — the object is gone at the end of the` |
|         - |  3941 | `			 * statement, so the store is unobservable either way. PHL announced it` |
|         - |  3942 | ``			 * with `Cannot perform assignment on a constant class attribute,PH7 is`` |
|         - |  3943 | ``			 * loading NULL`, a diagnostic php has no equivalent of. Every case that`` |
|         - |  3944 | `			 * IS a refusal — a class constant, a hooked or handler-backed property —` |
|         - |  3945 | `			 * is decided before this point now. */` |
|         3 |  3946 | `			pTos->nIdx = SXU32_HIGH;` |
|         2 |  3947 | `		}else{` |
|         - |  3948 | `			/* Enforce typed property declaration if any. May coerce the` |
|         - |  3949 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|    104419 |  3950 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);` |
|    104419 |  3951 | `			if( rcT == PH7_ABORT ){` |
|        13 |  3952 | `				goto Abort;` |
|         - |  3953 | `			}` |
|    104409 |  3954 | `			if( rcT == PH7_EXCEPTION ){` |
|         - |  3955 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|         - |  3956 | `				 * control to the nearest catch block if any (draining any` |
|         - |  3957 | `				 * abandoned outer-expression operands to the try's base),` |
|         - |  3958 | `				 * otherwise propagate out of the VM loop. */` |
|    100221 |  3959 | `				VmPopOperand(&pTos,1);` |
|         - |  3960 | `				{` |
|         - |  3961 | `					sxi32 iRp;` |
|    100221 |  3962 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|    400131 |  3963 | `						PH7_RESUME_DRAIN()` |
|    100125 |  3964 | `						pc = iRp;` |
|    100125 |  3965 | `						break;` |
|         - |  3966 | `					}` |
|         - |  3967 | `				}` |
|       108 |  3968 | `				goto Exception;` |
|         - |  3969 | `			}` |
|         - |  3970 | `			/* Point to the desired memory object */` |
|      4193 |  3971 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|      4193 |  3972 | `			if( pObj ){` |
|         - |  3973 | `				/* Perform the store operation */` |
|      4193 |  3974 | `				PH7_MemObjStore(pTos,pObj);` |
|      2094 |  3975 | `			}` |
|         - |  3976 | `		}` |
|      4195 |  3977 | `		break;` |
|   1487462 |  3978 | `	}else if( pInstr->p3 == 0 ){` |
|         - |  3979 | `		/* Take the variable name from the next on the stack (user-visible: a` |
|         - |  3980 | `		 * variable-variable NAME $$arr warns on an array) */` |
|         - |  3981 | `		{` |
|        29 |  3982 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|        29 |  3983 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|         - |  3984 | `		}` |
|        26 |  3985 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        26 |  3986 | `		pTos--;` |
|         - |  3987 | `#ifdef UNTRUST` |
|         - |  3988 | `		if( pTos < pStack  ){` |
|         - |  3989 | `			goto Abort;` |
|         - |  3990 | `		}` |
|         - |  3991 | `#endif` |
|        14 |  3992 | `	}else{` |
|   1487436 |  3993 | `		if( pInstr->nAux == 0 ){` |
|         - |  3994 | `			/* Measured once, like OP_LOAD's: the name is a compile-time buffer.` |
|         - |  3995 | `			 * VmNumberLocals fills it in for a body it walks. */` |
|     20710 |  3996 | `			pInstr->nAux = (sxu32)SyStrlen((const char *)pInstr->p3);` |
|     10351 |  3997 | `		}` |
|   1487436 |  3998 | `		SyStringInitFromBuf(&sName,pInstr->p3,pInstr->nAux);` |
|         - |  3999 | `	}` |
|   1487455 |  4000 | `	if( sName.nByte == sizeof("GLOBALS")-1` |
|    749829 |  4001 | `	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){` |
|         6 |  4002 | `		if( pInstr->p3 ){` |
|         - |  4003 | `			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time` |
|         - |  4004 | `			 * fatal there; raised at the store site here with the same` |
|         - |  4005 | `			 * message and the same non-catchable outcome). Element writes` |
|         - |  4006 | `			 * are unaffected. */` |
|         3 |  4007 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|         - |  4008 | `				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|         3 |  4009 | `			pVm->iExitStatus = 255;` |
|         3 |  4010 | `			pVm->bHaltRequested = 1;` |
|         3 |  4011 | `			goto Abort;` |
|         - |  4012 | `		}` |
|         - |  4013 | `		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's` |
|         - |  4014 | `		 * compile-time case: php quietly creates an ordinary symbol-table` |
|         - |  4015 | `		 * entry named GLOBALS, leaving the auto-global view intact. */` |
|         3 |  4016 | `		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);` |
|         3 |  4017 | `		PH7_MemObjRelease(&pTos[1]);` |
|         3 |  4018 | `		break;` |
|         - |  4019 | `	}` |
|         - |  4020 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   2228433 |  4021 | `	pObj = pInstr->p3` |
|   1487429 |  4022 | `		? PH7_VmExtractVarSlot(&(*pVm),&sName,TRUE,pInstr->nSite,aInstr)` |
|    740988 |  4023 | `		: VmExtractMemObj(&(*pVm),&sName,TRUE,TRUE);` |
|   1487456 |  4024 | `	if( pObj == 0 ){` |
|       ! 0 |  4025 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|         - |  4026 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|       ! 0 |  4027 | `		goto Abort;` |
|         - |  4028 | `	}` |
|   1487456 |  4029 | `	if( !pInstr->p3 ){` |
|        24 |  4030 | `		PH7_MemObjRelease(&pTos[1]);` |
|        11 |  4031 | `	}` |
|         - |  4032 | ``	/* A plain VARIABLE can BE a typed property's slot: `$r = &$o->n` aliases it,`` |
|         - |  4033 | `	 * and so does a by-ref parameter bound to one. php enforces the declared type` |
|         - |  4034 | ``	 * on this store exactly as on `$o->n = v` -- with a sentence of its own, which`` |
|         - |  4035 | `	 * names the property HOLDING the reference -- and PHL wrote through it` |
|         - |  4036 | `` 	 * unchecked, leaving a string in an `int` property and an array in a `string` `` |
|         - |  4037 | `	 * one. The store filter's table answers in one lookup and is skipped outright` |
|         - |  4038 | `	 * when nothing has registered a slot; measured at noise level on an` |
|         - |  4039 | `	 * object-and-array workload (a loop of nothing but plain stores is where the` |
|         - |  4040 | `	 * lookup shows at all). */` |
|   1487456 |  4041 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) > 0 && pObj->nIdx != SXU32_HIGH ){` |
|    538793 |  4042 | `		sxi32 rcRef = VmEnforcePropertyTypeOnStore(&(*pVm),pObj->nIdx,pTos,` |
|         - |  4043 | `			VM_TYPED_STORE_VIA_REF);` |
|    538793 |  4044 | `		if( rcRef == PH7_ABORT ){` |
|       ! 0 |  4045 | `			goto Abort;` |
|         - |  4046 | `		}` |
|    538793 |  4047 | `		if( rcRef == PH7_EXCEPTION ){` |
|        17 |  4048 | `			VmPopOperand(&pTos,1);` |
|         - |  4049 | `			{` |
|         - |  4050 | `				sxi32 iRp;` |
|        17 |  4051 | `				if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|       ! 0 |  4052 | `					PH7_RESUME_DRAIN()` |
|       ! 0 |  4053 | `					pc = iRp;` |
|       ! 0 |  4054 | `					break;` |
|         - |  4055 | `				}` |
|         - |  4056 | `			}` |
|        17 |  4057 | `			goto Exception;` |
|         - |  4058 | `		}` |
|    270325 |  4059 | `	}` |
|         - |  4060 | `	/* Perform the store operation */` |
|   1487440 |  4061 | `	PH7_MemObjStore(pTos,pObj);` |
|   1487440 |  4062 | `	break;` |
|         - |  4063 | `				   }` |
|         - |  4064 | `/*` |
|         - |  4065 | ` * STORE_IDX:   P1 * P3` |
|         - |  4066 | ` * STORE_IDX_R: P1 * P3` |
|         - |  4067 | ` *` |
|         - |  4068 | ` * Perfrom a store operation an a hashmap entry.` |
|         - |  4069 | ` */` |
|    307895 |  4070 | `case PH7_OP_STORE_IDX:` |
|         - |  4071 | `case PH7_OP_STORE_IDX_REF: {` |
|         - |  4072 | `	VmOpRc rcOp;` |
|    615570 |  4073 | `	sState.pTos = pTos;` |
|    615570 |  4074 | `	sState.pc = pc;` |
|    615570 |  4075 | `	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);` |
|    615570 |  4076 | `	pTos = sState.pTos;` |
|    615570 |  4077 | `	pc = sState.pc;` |
|    615570 |  4078 | `	if( rcOp == VM_OP_ABORT ){` |
|         3 |  4079 | `		goto Abort;` |
|    615568 |  4080 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        56 |  4081 | `		goto Exception;` |
|         - |  4082 | `	}` |
|    615516 |  4083 | `	break;` |
|         - |  4084 | `					  }` |
|         - |  4085 | `/*` |
|         - |  4086 | ` * INCR: P1 * *` |
|         - |  4087 | ` *` |
|         - |  4088 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|         - |  4089 | ` * If the P1 operand is set then perform a duplication of the top of` |
|         - |  4090 | ` * the stack and increment after that.` |
|         - |  4091 | ` */` |
|   6430289 |  4092 | `case PH7_OP_INCR:` |
|         - |  4093 | `#ifdef UNTRUST` |
|         - |  4094 | `	if( pTos < pStack ){` |
|         - |  4095 | `		goto Abort;` |
|         - |  4096 | `	}` |
|         - |  4097 | `#endif` |
|         - |  4098 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|         - |  4099 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|         - |  4100 | `	 * — which otherwise skips object/array/resource operands. */` |
|  12862745 |  4101 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|         - |  4102 | `	/* php refuses to increment a string OFFSET, whatever byte it holds. */` |
|  12862737 |  4103 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|         - |  4104 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|         - |  4105 | `	 * php's TypeError, raised BEFORE any set dispatch (the type guard below` |
|         - |  4106 | `	 * would skip the mutation and the tail write-back would otherwise call` |
|         - |  4107 | `	 * the set hook with the unchanged value). */` |
|  12862716 |  4108 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|   6432458 |  4109 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|         5 |  4110 | `		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|         5 |  4111 | `		if( VM_HOOK_PEND_IS_RMW(pTopInc->iKind) && pTopInc->nScratchIdx == pTos->nIdx ){` |
|         - |  4112 | `			SyBlob sErrMsg;` |
|         5 |  4113 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         5 |  4114 | `			VmIncDecTypeErrorMsg(pTos,TRUE,&sErrMsg);` |
|         5 |  4115 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|         5 |  4116 | `			VmHookRmwDropTop(&(*pVm));` |
|         5 |  4117 | `			pTos->nIdx = SXU32_HIGH;` |
|         5 |  4118 | `			break;` |
|         - |  4119 | `		}` |
|       ! 0 |  4120 | `	}` |
|  12862717 |  4121 | `	PH7_INCDEC_NATIVE_ARITH("+")` |
|  12862715 |  4122 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|         - |  4123 | `		/* php's ++ operand contract: an array, object or resource is a catchable` |
|         - |  4124 | `		 * TypeError, not the silent no-op this used to be. Settle the operand` |
|         - |  4125 | `		 * (the op's result slot) BEFORE throwing, like the arithmetic sites. */` |
|         - |  4126 | `		SyBlob sIncMsg;` |
|         - |  4127 | `		sxi32 rcInc;` |
|        25 |  4128 | `		SyBlobInit(&sIncMsg,&pVm->sAllocator);` |
|        25 |  4129 | `		VmIncDecTypeErrorMsg(pTos,TRUE,&sIncMsg);` |
|        25 |  4130 | `		PH7_MemObjRelease(pTos);` |
|        25 |  4131 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        25 |  4132 | `		pTos->nIdx = SXU32_HIGH;` |
|        37 |  4133 | `		rcInc = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sIncMsg),` |
|        12 |  4134 | `			SyBlobLength(&sIncMsg));` |
|        25 |  4135 | `		SyBlobRelease(&sIncMsg);` |
|        25 |  4136 | `		if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|        25 |  4137 | `		rc = rcInc;` |
|        25 |  4138 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  4139 | `	}` |
|         - |  4140 | ``	/* php 8.3+: `++` on a BOOL is a no-op it warns about (E_WARNING, errno 2 —`` |
|         - |  4141 | ``	 * same shape as the null `--` below), where PH7 coerced true to int 2. */`` |
|  12862691 |  4142 | `	if( pTos->iFlags & MEMOBJ_BOOL ){` |
|         7 |  4143 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|         - |  4144 | `			"Increment on type bool has no effect, this will change in the next major version of PHP");` |
|         3 |  4145 | `	}` |
|  12862691 |  4146 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_BOOL)) == 0 ){` |
|  12862685 |  4147 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|         - |  4148 | `			ph7_value *pObj;` |
|  12862673 |  4149 | `			if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|  12862673 |  4150 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|         - |  4151 | `					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric` |
|         - |  4152 | `					 * string (it points at str_increment() instead); PHL rejects it. */` |
|         - |  4153 | `					SyBlob sErrMsg;` |
|         3 |  4154 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         3 |  4155 | `					SyBlobAppend(&sErrMsg,` |
|         - |  4156 | `						"Increment on a non-numeric string is not supported, use str_increment() instead",` |
|         - |  4157 | `						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);` |
|         3 |  4158 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|         3 |  4159 | `					VmHookRmwDropTop(&(*pVm));` |
|         3 |  4160 | `					pTos->nIdx = SXU32_HIGH;` |
|         3 |  4161 | `					break;` |
|       ! 0 |  4162 | `				}else{` |
|         - |  4163 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|         - |  4164 | `					 * original value: pTos may alias pObj's blob via` |
|         - |  4165 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|         - |  4166 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|         - |  4167 | `					 * pObj. Force pTos to take ownership of its blob first` |
|         - |  4168 | `					 * so its old-value view survives the coercion. */` |
|  12862671 |  4169 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        13 |  4170 | `						SyBlobNullAppend(&pTos->sBlob);` |
|         5 |  4171 | `					}` |
|         - |  4172 | `					/* Force a numeric cast on the variable */` |
|  12862671 |  4173 | `					PH7_MemObjToNumeric(pObj);` |
|  12862671 |  4174 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|         7 |  4175 | `						pObj->rVal++;` |
|         - |  4176 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|         - |  4177 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|         - |  4178 | `						 * ===, intdiv() etc. read a stale int for an` |
|         - |  4179 | `						 * integer-valued real. */` |
|         7 |  4180 | `						PH7_MemObjTryInteger(pObj);` |
|         4 |  4181 | `					}else{` |
|         - |  4182 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|         - |  4183 | `						 * build wraps like OP_POW's OMIT path. */` |
|         - |  4184 | `						sxi64 r;` |
|  12862665 |  4185 | `						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|         - |  4186 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         7 |  4187 | `							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;` |
|         7 |  4188 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|         - |  4189 | `#else` |
|         - |  4190 | `							pObj->x.iVal = r;` |
|         - |  4191 | `#endif` |
|         4 |  4192 | `						}else{` |
|  12862659 |  4193 | `							pObj->x.iVal = r;` |
|         - |  4194 | `						}` |
|         - |  4195 | `					}` |
|  12862671 |  4196 | `					if( pInstr->iP1 ){` |
|         - |  4197 | `						/* Pre-increment: result is the new value. */` |
|      2186 |  4198 | `						PH7_MemObjStore(pObj,pTos);` |
|      1091 |  4199 | `					}` |
|         - |  4200 | `					/* Post-increment: pTos retains the old value (a string` |
|         - |  4201 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|         - |  4202 | `					/* A NATIVE class's property is php's own C struct field, and` |
|         - |  4203 | ``					 * `++` writes it BACK through the write handler there — so the`` |
|         - |  4204 | `					 * conversion runs on the mutated slot. php's own answer, and` |
|         - |  4205 | `					 * the EXPRESSION's value is the unconverted sum either way:` |
|         - |  4206 | ``					 * `$i->f = 1.456008; var_dump(++$i->f, $i->f)` prints`` |
|         - |  4207 | `					 * 2.4560079999999997 then 2.456007. */` |
|  12862671 |  4208 | `					PH7_NATIVE_SET_AFTER_MUTATE(pTos->nIdx,pObj)` |
|         - |  4209 | `				}` |
|   6432413 |  4210 | `			}` |
|   6432418 |  4211 | `		}else{` |
|        13 |  4212 | `			if( pInstr->iP1 ){` |
|       ! 0 |  4213 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|       ! 0 |  4214 | `					PH7_MemObjStringIncrement(pTos);` |
|       ! 0 |  4215 | `				}else{` |
|         - |  4216 | `					/* Force a numeric cast */` |
|       ! 0 |  4217 | `					PH7_MemObjToNumeric(pTos);` |
|         - |  4218 | `					/* Pre-increment */` |
|       ! 0 |  4219 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|       ! 0 |  4220 | `						pTos->rVal++;` |
|         - |  4221 | `						/* Try to get an integer representation */` |
|       ! 0 |  4222 | `						PH7_MemObjTryInteger(pTos);` |
|       ! 0 |  4223 | `					}else{` |
|         - |  4224 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|         - |  4225 | `						 * build wraps like OP_POW's OMIT path. */` |
|         - |  4226 | `						sxi64 r;` |
|       ! 0 |  4227 | `						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|         - |  4228 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       ! 0 |  4229 | `							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;` |
|       ! 0 |  4230 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|         - |  4231 | `#else` |
|         - |  4232 | `							pTos->x.iVal = r;` |
|         - |  4233 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|         - |  4234 | `#endif` |
|       ! 0 |  4235 | `						}else{` |
|       ! 0 |  4236 | `							pTos->x.iVal = r;` |
|       ! 0 |  4237 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|         - |  4238 | `						}` |
|         - |  4239 | `					}` |
|         - |  4240 | `				}` |
|       ! 0 |  4241 | `			}` |
|         - |  4242 | `		}` |
|   6432419 |  4243 | `	}` |
|  12862689 |  4244 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|  12862689 |  4245 | `	break;` |
|         - |  4246 | `/*` |
|         - |  4247 | ` * DECR: P1 * *` |
|         - |  4248 | ` *` |
|         - |  4249 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|         - |  4250 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|         - |  4251 | ` * and decrement after that.` |
|         - |  4252 | ` */` |
|        88 |  4253 | `case PH7_OP_DECR:` |
|         - |  4254 | `#ifdef UNTRUST` |
|         - |  4255 | `	if( pTos < pStack ){` |
|         - |  4256 | `		goto Abort;` |
|         - |  4257 | `	}` |
|         - |  4258 | `#endif` |
|         - |  4259 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|         - |  4260 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|         - |  4261 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|         - |  4262 | `	 * property currently holding null). */` |
|       181 |  4263 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|         - |  4264 | `	/* php refuses to decrement a string OFFSET, whatever byte it holds. */` |
|       169 |  4265 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|         - |  4266 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|         - |  4267 | `	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —` |
|         - |  4268 | ``	 * `--` on null is php's no-op and its write-back still dispatches set). */`` |
|       162 |  4269 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|        90 |  4270 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|         3 |  4271 | `		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|         3 |  4272 | `		if( VM_HOOK_PEND_IS_RMW(pTopDec->iKind) && pTopDec->nScratchIdx == pTos->nIdx ){` |
|         - |  4273 | `			SyBlob sErrMsg;` |
|         3 |  4274 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         3 |  4275 | `			VmIncDecTypeErrorMsg(pTos,FALSE,&sErrMsg);` |
|         3 |  4276 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|         3 |  4277 | `			VmHookRmwDropTop(&(*pVm));` |
|         3 |  4278 | `			pTos->nIdx = SXU32_HIGH;` |
|         3 |  4279 | `			break;` |
|         - |  4280 | `		}` |
|       ! 0 |  4281 | `	}` |
|       163 |  4282 | `	PH7_INCDEC_NATIVE_ARITH("-")` |
|       161 |  4283 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|         - |  4284 | ``		/* php's `--` operand contract, the mirror of INCR's: an array, object or`` |
|         - |  4285 | `		 * resource is a catchable TypeError, not a silent no-op. */` |
|         - |  4286 | `		SyBlob sDecMsg;` |
|         - |  4287 | `		sxi32 rcDec;` |
|        11 |  4288 | `		SyBlobInit(&sDecMsg,&pVm->sAllocator);` |
|        11 |  4289 | `		VmIncDecTypeErrorMsg(pTos,FALSE,&sDecMsg);` |
|        11 |  4290 | `		PH7_MemObjRelease(pTos);` |
|        11 |  4291 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        11 |  4292 | `		pTos->nIdx = SXU32_HIGH;` |
|        16 |  4293 | `		rcDec = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sDecMsg),` |
|         5 |  4294 | `			SyBlobLength(&sDecMsg));` |
|        11 |  4295 | `		SyBlobRelease(&sDecMsg);` |
|        11 |  4296 | `		if( rcDec == SXERR_ABORT ){ goto Abort; }` |
|        11 |  4297 | `		rc = rcDec;` |
|        13 |  4298 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  4299 | `	}` |
|         - |  4300 | ``	/* NULL and BOOL stay excluded: PHP leaves `--` on either untouched (no-op) --`` |
|         - |  4301 | `	 * but 8.3 warns about both no-ops, same as the non-numeric-string one below. */` |
|       151 |  4302 | `	if( pTos->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|         - |  4303 | `		/* E_WARNING, not E_DEPRECATED -- php reports these at errno 2. */` |
|        19 |  4304 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|         - |  4305 | `			"Decrement on type %s has no effect, this will change in the next major version of PHP",` |
|        12 |  4306 | `			(pTos->iFlags & MEMOBJ_NULL) ? "null" : "bool");` |
|         6 |  4307 | `	}` |
|       151 |  4308 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|       139 |  4309 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|         - |  4310 | `			ph7_value *pObj;` |
|       137 |  4311 | `			if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       137 |  4312 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|         - |  4313 | ``					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string`` |
|         - |  4314 | `					 * (php has no string decrement); PHL rejects it. */` |
|         - |  4315 | `					SyBlob sErrMsg;` |
|         3 |  4316 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         3 |  4317 | `					SyBlobAppend(&sErrMsg,` |
|         - |  4318 | `						"Decrement on a non-numeric string is not supported",` |
|         - |  4319 | `						sizeof("Decrement on a non-numeric string is not supported")-1);` |
|         3 |  4320 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|         3 |  4321 | `					VmHookRmwDropTop(&(*pVm));` |
|         3 |  4322 | `					pTos->nIdx = SXU32_HIGH;` |
|         3 |  4323 | `					break;` |
|       ! 0 |  4324 | `				}else{` |
|         - |  4325 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|         - |  4326 | `					 * post-decrement must preserve pTos's original value, which` |
|         - |  4327 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|         - |  4328 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       135 |  4329 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|         5 |  4330 | `						SyBlobNullAppend(&pTos->sBlob);` |
|         2 |  4331 | `					}` |
|       135 |  4332 | `					PH7_MemObjToNumeric(pObj);` |
|       135 |  4333 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        11 |  4334 | `						pObj->rVal--;` |
|         - |  4335 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|         - |  4336 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|         - |  4337 | `						 * ===, intdiv() etc. read a stale int for an` |
|         - |  4338 | `						 * integer-valued real. */` |
|        11 |  4339 | `						PH7_MemObjTryInteger(pObj);` |
|         6 |  4340 | `					}else{` |
|         - |  4341 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|         - |  4342 | `						 * build wraps like OP_POW's OMIT path. */` |
|         - |  4343 | `						sxi64 r;` |
|       125 |  4344 | `						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|         - |  4345 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         3 |  4346 | `							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;` |
|         3 |  4347 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|         - |  4348 | `#else` |
|         - |  4349 | `							pObj->x.iVal = r;` |
|         - |  4350 | `#endif` |
|         2 |  4351 | `						}else{` |
|       123 |  4352 | `							pObj->x.iVal = r;` |
|         - |  4353 | `						}` |
|         - |  4354 | `					}` |
|       135 |  4355 | `					if( pInstr->iP1 ){` |
|         - |  4356 | `						/* Pre-decrement: result is the new value. */` |
|         3 |  4357 | `						PH7_MemObjStore(pObj,pTos);` |
|         1 |  4358 | `					}` |
|         - |  4359 | `					/* Post-decrement: pTos retains the old value. */` |
|       135 |  4360 | `					PH7_NATIVE_SET_AFTER_MUTATE(pTos->nIdx,pObj)   /* see OP_INCR */` |
|         - |  4361 | `				}` |
|        65 |  4362 | `			}` |
|        68 |  4363 | `		}else{` |
|         3 |  4364 | `			if( pInstr->iP1 ){` |
|         3 |  4365 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|         - |  4366 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|       ! 0 |  4367 | `				}else{` |
|         - |  4368 | `					/* Force a numeric cast */` |
|         3 |  4369 | `					PH7_MemObjToNumeric(pTos);` |
|         - |  4370 | `					/* Pre-decrement */` |
|         3 |  4371 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|       ! 0 |  4372 | `						pTos->rVal--;` |
|         - |  4373 | `						/* Keep the cached int consistent with the new rVal. */` |
|       ! 0 |  4374 | `						PH7_MemObjTryInteger(pTos);` |
|       ! 0 |  4375 | `					}else{` |
|         - |  4376 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|         - |  4377 | `						 * build wraps like OP_POW's OMIT path. */` |
|         - |  4378 | `						sxi64 r;` |
|         3 |  4379 | `						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|         - |  4380 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       ! 0 |  4381 | `							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;` |
|       ! 0 |  4382 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|         - |  4383 | `#else` |
|         - |  4384 | `							pTos->x.iVal = r;` |
|         - |  4385 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|         - |  4386 | `#endif` |
|       ! 0 |  4387 | `						}else{` |
|         3 |  4388 | `							pTos->x.iVal = r;` |
|         3 |  4389 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|         - |  4390 | `						}` |
|         - |  4391 | `					}` |
|         - |  4392 | `				}` |
|         1 |  4393 | `			}` |
|         - |  4394 | `		}` |
|        66 |  4395 | `	}` |
|       149 |  4396 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|       149 |  4397 | `	break;` |
|         - |  4398 | `/*` |
|         - |  4399 | ` * UMINUS: * * *` |
|         - |  4400 | ` *` |
|         - |  4401 | ` * Perform a unary minus operation.` |
|         - |  4402 | ` */` |
|     56598 |  4403 | `case PH7_OP_UMINUS:` |
|         - |  4404 | `#ifdef UNTRUST` |
|         - |  4405 | `	if( pTos < pStack ){` |
|         - |  4406 | `		goto Abort;` |
|         - |  4407 | `	}` |
|         - |  4408 | `#endif` |
|         - |  4409 | ``	/* php's `$x * -1` operand contract: an array, object, resource or`` |
|         - |  4410 | `	 * non-numeric string is a TypeError, not a warned int(-1). */` |
|    113189 |  4411 | `	PH7_UNARY_ARITH_CONTRACT(-1)` |
|         - |  4412 | `	/* Force a numeric (integer,real or both) cast */` |
|    113161 |  4413 | `	PH7_MemObjToNumeric(pTos);` |
|    113161 |  4414 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       345 |  4415 | `		pTos->rVal = -pTos->rVal;` |
|       170 |  4416 | `	}` |
|    113161 |  4417 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    112891 |  4418 | `		if( pTos->x.iVal == SMALLEST_INT64 ){` |
|         - |  4419 | `			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a` |
|         - |  4420 | `			 * REAL representation is already present it is the negated` |
|         - |  4421 | `			 * authoritative value, so just drop the now-stale cached int. The` |
|         - |  4422 | `			 * integer-only build has no float type, so it wraps (two's` |
|         - |  4423 | `			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */` |
|         - |  4424 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         7 |  4425 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|         7 |  4426 | `				pTos->rVal = -(ph7_real)pTos->x.iVal;` |
|         7 |  4427 | `				MemObjSetType(pTos,MEMOBJ_REAL);` |
|         4 |  4428 | `			}else{` |
|       ! 0 |  4429 | `				pTos->iFlags &= ~MEMOBJ_INT;` |
|         - |  4430 | `			}` |
|         - |  4431 | `#else` |
|         - |  4432 | `			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);` |
|         - |  4433 | `#endif` |
|         4 |  4434 | `		}else{` |
|    112885 |  4435 | `			pTos->x.iVal = -pTos->x.iVal;` |
|         - |  4436 | `		}` |
|     56437 |  4437 | `	}` |
|    113161 |  4438 | `	break;` |
|         - |  4439 | `/*` |
|         - |  4440 | ` * UPLUS: * * *` |
|         - |  4441 | ` *` |
|         - |  4442 | ` * Perform a unary plus operation.` |
|         - |  4443 | ` */` |
|        24 |  4444 | `case PH7_OP_UPLUS:` |
|         - |  4445 | `#ifdef UNTRUST` |
|         - |  4446 | `	if( pTos < pStack ){` |
|         - |  4447 | `		goto Abort;` |
|         - |  4448 | `	}` |
|         - |  4449 | `#endif` |
|         - |  4450 | ``	/* Unary plus is php's `$x * 1`, so it answers the same TypeError as unary`` |
|         - |  4451 | ``	 * minus -- naming `int` as the other operand, exactly as php does. */`` |
|        49 |  4452 | `	PH7_UNARY_ARITH_CONTRACT(1)` |
|         - |  4453 | `	/* Force a numeric (integer,real or both) cast */` |
|        41 |  4454 | `	PH7_MemObjToNumeric(pTos);` |
|        41 |  4455 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       ! 0 |  4456 | `		pTos->rVal = +pTos->rVal;` |
|       ! 0 |  4457 | `	}` |
|        41 |  4458 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|        41 |  4459 | `		pTos->x.iVal = +pTos->x.iVal;` |
|        20 |  4460 | `	}` |
|        41 |  4461 | `	break;` |
|         - |  4462 | `/*` |
|         - |  4463 | ` * OP_LNOT: * * *` |
|         - |  4464 | ` *` |
|         - |  4465 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|         - |  4466 | ` * with its complement.` |
|         - |  4467 | ` */` |
|     85417 |  4468 | `case PH7_OP_LNOT:` |
|         - |  4469 | `#ifdef UNTRUST` |
|         - |  4470 | `	if( pTos < pStack ){` |
|         - |  4471 | `		goto Abort;` |
|         - |  4472 | `	}` |
|         - |  4473 | `#endif` |
|         - |  4474 | `	/* Force a boolean cast */` |
|    170515 |  4475 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       878 |  4476 | `		PH7_MemObjToBool(pTos);` |
|       432 |  4477 | `	}` |
|    170515 |  4478 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    170515 |  4479 | `	break;` |
|         - |  4480 | `/*` |
|         - |  4481 | ` * OP_BITNOT: * * *` |
|         - |  4482 | ` *` |
|         - |  4483 | ` * Interpret the top of the stack as an value.Replace it` |
|         - |  4484 | ` * with its ones-complement.` |
|         - |  4485 | ` */` |
|       389 |  4486 | `case PH7_OP_BITNOT:` |
|         - |  4487 | `#ifdef UNTRUST` |
|         - |  4488 | `	if( pTos < pStack ){` |
|         - |  4489 | `		goto Abort;` |
|         - |  4490 | `	}` |
|         - |  4491 | `#endif` |
|       782 |  4492 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|         - |  4493 | ``		/* php's `~` over a STRING is a per-BYTE ones-complement of the same`` |
|         - |  4494 | ``		 * length — a binary string, not an integer operation, so `~"abc"` is`` |
|         - |  4495 | `		 * "\x9e\x9d\x9c" where PHL cast the string to an int and answered` |
|         - |  4496 | `		 * int(-1). The operand's blob can be a READ-ONLY VIEW of the variable's` |
|         - |  4497 | `		 * own buffer (PH7_MemObjLoad), so complement into a fresh blob and swap` |
|         - |  4498 | `		 * it in rather than writing through the view. */` |
|         - |  4499 | `		SyBlob sNotBuf;` |
|        17 |  4500 | `		const unsigned char *zNotIn = (const unsigned char *)SyBlobData(&pTos->sBlob);` |
|        17 |  4501 | `		sxu32 nNotIn = SyBlobLength(&pTos->sBlob), iNot;` |
|        17 |  4502 | `		SyBlobInit(&sNotBuf,&pVm->sAllocator);` |
|        53 |  4503 | `		for( iNot = 0 ; iNot < nNotIn ; ++iNot ){` |
|        37 |  4504 | `			unsigned char cNot = (unsigned char)~zNotIn[iNot];` |
|        37 |  4505 | `			SyBlobAppend(&sNotBuf,(const void *)&cNot,sizeof(char));` |
|        19 |  4506 | `		}` |
|        17 |  4507 | `		PH7_MemObjRelease(pTos);` |
|        17 |  4508 | `		MemObjSetType(pTos,MEMOBJ_STRING);` |
|        17 |  4509 | `		if( SyBlobLength(&sNotBuf) > 0 ){` |
|        15 |  4510 | `			SyBlobAppend(&pTos->sBlob,SyBlobData(&sNotBuf),SyBlobLength(&sNotBuf));` |
|         7 |  4511 | `		}` |
|        17 |  4512 | `		SyBlobRelease(&sNotBuf);` |
|        17 |  4513 | `		break;` |
|         - |  4514 | `	}` |
|       766 |  4515 | `	if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|         - |  4516 | `		/* Everything else — null, a bool, an array, an object, a resource — has` |
|         - |  4517 | `		 * no bitwise-not in php at all: a catchable TypeError naming the operand,` |
|         - |  4518 | `		 * where PHL cast it to an int and answered ~0 / ~1. */` |
|         - |  4519 | `		SyBlob sNotMsg;` |
|         - |  4520 | `		sxi32 rcNot;` |
|        27 |  4521 | `		SyBlobInit(&sNotMsg,&pVm->sAllocator);` |
|        27 |  4522 | `		VmBitNotTypeErrorMsg(pTos,&sNotMsg);` |
|        27 |  4523 | `		PH7_MemObjRelease(pTos);` |
|        27 |  4524 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        27 |  4525 | `		pTos->nIdx = SXU32_HIGH;` |
|        40 |  4526 | `		rcNot = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sNotMsg),` |
|        13 |  4527 | `			SyBlobLength(&sNotMsg));` |
|        27 |  4528 | `		SyBlobRelease(&sNotMsg);` |
|        27 |  4529 | `		if( rcNot == SXERR_ABORT ){ goto Abort; }` |
|        27 |  4530 | `		rc = rcNot;` |
|        27 |  4531 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  4532 | `	}` |
|         - |  4533 | `	/* Force an integer cast (php deprecates a lossy float here too) */` |
|       740 |  4534 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       740 |  4535 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|       740 |  4536 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 |  4537 | `		PH7_MemObjToInteger(pTos);` |
|       ! 0 |  4538 | `	}` |
|       740 |  4539 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|         - |  4540 | `	/* An INTEGRAL float carries a cached MEMOBJ_INT beside MEMOBJ_REAL, so the` |
|         - |  4541 | `` 	 * cast above is skipped and the value kept rendering as a float: `~2.0` `` |
|         - |  4542 | ``	 * answered 2.0 instead of -3. `~` always yields an int. */`` |
|       740 |  4543 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       740 |  4544 | `	break;` |
|         - |  4545 | `/* OP_MUL * * *` |
|         - |  4546 | ` * OP_MUL_STORE * * *` |
|         - |  4547 | ` *` |
|         - |  4548 | ` * Pop the top two elements from the stack, multiply them together,` |
|         - |  4549 | ` * and push the result back onto the stack.` |
|         - |  4550 | ` */` |
|      3817 |  4551 | `case PH7_OP_MUL:` |
|         - |  4552 | `case PH7_OP_MUL_STORE: {` |
|         - |  4553 | `	VmOpRc rcOp;` |
|         - |  4554 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      7625 |  4555 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      7623 |  4556 | `	sState.pTos = pTos;` |
|      7623 |  4557 | `	sState.pc = pc;` |
|      7623 |  4558 | `	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);` |
|      7623 |  4559 | `	pTos = sState.pTos;` |
|      7623 |  4560 | `	pc = sState.pc;` |
|      7623 |  4561 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  4562 | `		goto Abort;` |
|      7623 |  4563 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       312 |  4564 | `		goto Exception;` |
|         - |  4565 | `	}` |
|      7313 |  4566 | `	break;` |
|         - |  4567 | `					  }` |
|         - |  4568 | `/* OP_POW * * *` |
|         - |  4569 | ` * OP_POW_STORE * * *` |
|         - |  4570 | ` *` |
|         - |  4571 | ` * Pop the top two elements from the stack, raise the second to the` |
|         - |  4572 | ` * power of the first, and push the result. PHP semantics: int**int` |
|         - |  4573 | ` * stays integer iff the exponent is non-negative and the exact result` |
|         - |  4574 | ` * fits in sxi64; otherwise the result is a double.` |
|         - |  4575 | ` */` |
|       535 |  4576 | `case PH7_OP_POW:` |
|         - |  4577 | `case PH7_OP_POW_STORE: {` |
|         - |  4578 | `	VmOpRc rcOp;` |
|         - |  4579 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      1072 |  4580 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      1070 |  4581 | `	sState.pTos = pTos;` |
|      1070 |  4582 | `	sState.pc = pc;` |
|      1070 |  4583 | `	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);` |
|      1070 |  4584 | `	pTos = sState.pTos;` |
|      1070 |  4585 | `	pc = sState.pc;` |
|      1070 |  4586 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  4587 | `		goto Abort;` |
|      1070 |  4588 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       318 |  4589 | `		goto Exception;` |
|         - |  4590 | `	}` |
|       753 |  4591 | `	break;` |
|         - |  4592 | `					  }` |
|         - |  4593 | `/* OP_ADD * * *` |
|         - |  4594 | ` *` |
|         - |  4595 | ` * Pop the top two elements from the stack, add them together,` |
|         - |  4596 | ` * and push the result back onto the stack.` |
|         - |  4597 | ` */` |
|   3363238 |  4598 | `case PH7_OP_ADD:{` |
|   6726366 |  4599 | `	ph7_value *pNos = &pTos[-1];` |
|         - |  4600 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|         - |  4601 | `	int rcNa;` |
|   6726366 |  4602 | `	const char *zArCls = "TypeError";` |
|         - |  4603 | `#ifdef UNTRUST` |
|         - |  4604 | `	if( pNos < pStack ){` |
|         - |  4605 | `		goto Abort;` |
|         - |  4606 | `	}` |
|         - |  4607 | `#endif` |
|         - |  4608 | `	{` |
|         - |  4609 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|         - |  4610 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|         - |  4611 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|         - |  4612 | `		SyBlob sArMsg;` |
|   6726366 |  4613 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   6726366 |  4614 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"+",pNos,&zArCls,&sArMsg);` |
|   6726366 |  4615 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|         - |  4616 | `			sxi32 rcAr;` |
|       182 |  4617 | `			VmPopOperand(&pTos,1);` |
|       182 |  4618 | `			PH7_MemObjRelease(pTos);` |
|       182 |  4619 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       182 |  4620 | `			pTos->nIdx = SXU32_HIGH;` |
|       272 |  4621 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|        90 |  4622 | `				SyBlobLength(&sArMsg));` |
|       182 |  4623 | `			SyBlobRelease(&sArMsg);` |
|       182 |  4624 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|       182 |  4625 | `			rc = rcAr;` |
|       182 |  4626 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  4627 | `		}` |
|   6726186 |  4628 | `		SyBlobRelease(&sArMsg);` |
|         - |  4629 | `	}` |
|         - |  4630 | `	/* Perform the addition (unless a do_operation handler already answered) */` |
|   6726186 |  4631 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|   6726172 |  4632 | `		PH7_MemObjAdd(pNos,pTos,FALSE);` |
|   3363026 |  4633 | `	}` |
|   6726186 |  4634 | `	VmPopOperand(&pTos,1);` |
|   6726186 |  4635 | `	break;` |
|         - |  4636 | `				}` |
|         - |  4637 | `/*` |
|         - |  4638 | ` * OP_ADD_STORE * * *` |
|         - |  4639 | ` *` |
|         - |  4640 | ` * Pop the top two elements from the stack, add them together,` |
|         - |  4641 | ` * and push the result back onto the stack.` |
|         - |  4642 | ` */` |
|    237344 |  4643 | `case PH7_OP_ADD_STORE:{` |
|    474957 |  4644 | `	ph7_value *pNos = &pTos[-1];` |
|         - |  4645 | `	ph7_value *pObj;` |
|         - |  4646 | `	sxu32 nIdx;` |
|         - |  4647 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|         - |  4648 | `	int rcNa;` |
|    474957 |  4649 | `	const char *zArCls = "TypeError";` |
|         - |  4650 | `#ifdef UNTRUST` |
|         - |  4651 | `	if( pNos < pStack ){` |
|         - |  4652 | `		goto Abort;` |
|         - |  4653 | `	}` |
|         - |  4654 | `#endif` |
|         - |  4655 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|    712222 |  4656 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|         - |  4657 | `	{` |
|         - |  4658 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|         - |  4659 | `		 * array, object or resource operand is a TypeError too. */` |
|         - |  4660 | `		SyBlob sArMsg;` |
|    474951 |  4661 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    474951 |  4662 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"+",pTos,&zArCls,&sArMsg);` |
|    474951 |  4663 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|         - |  4664 | `			sxi32 rcAr;` |
|       151 |  4665 | `			VmPopOperand(&pTos,1);` |
|       151 |  4666 | `			PH7_MemObjRelease(pTos);` |
|       151 |  4667 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       151 |  4668 | `			pTos->nIdx = SXU32_HIGH;` |
|       226 |  4669 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|        75 |  4670 | `				SyBlobLength(&sArMsg));` |
|       151 |  4671 | `			SyBlobRelease(&sArMsg);` |
|       151 |  4672 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|       151 |  4673 | `			rc = rcAr;` |
|       151 |  4674 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  4675 | `		}` |
|    474801 |  4676 | `		SyBlobRelease(&sArMsg);` |
|         - |  4677 | `	}` |
|         - |  4678 | `	/* Perform the addition */` |
|    474801 |  4679 | `	nIdx = pTos->nIdx;` |
|    474801 |  4680 | `	if( nIdx == pVm->nGlobalIdx ){` |
|         - |  4681 | `		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment` |
|         - |  4682 | `		 * (a compile-time fatal in php; raised here, same message). */` |
|         3 |  4683 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|         - |  4684 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|         3 |  4685 | `		pVm->iExitStatus = 255;` |
|         3 |  4686 | `		pVm->bHaltRequested = 1;` |
|         3 |  4687 | `		goto Abort;` |
|         - |  4688 | `	}` |
|    474799 |  4689 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|    474797 |  4690 | `		PH7_MemObjAdd(pTos,pNos,TRUE);` |
|    237528 |  4691 | `	}` |
|         - |  4692 | `	/* Peform the store operation */` |
|    474799 |  4693 | `	if( nIdx == SXU32_HIGH ){` |
|         - |  4694 | ``		/* A read-modify-write THROUGH a temporary (`f()[0] += 5`): php computes it,`` |
|         - |  4695 | `		 * drops it and stays silent. See the OP_STORE member note above. */` |
|    474797 |  4696 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|    474795 |  4697 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|    474791 |  4698 | `		PH7_MemObjStore(pTos,pObj);` |
|    237525 |  4699 | `	}` |
|    474795 |  4700 | `	PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|         - |  4701 | `	/* Ticket 1433-35: Perform a stack dup */` |
|    474795 |  4702 | `	PH7_MemObjStore(pTos,pNos);` |
|    474795 |  4703 | `	VmPopOperand(&pTos,1);` |
|    474795 |  4704 | `	break;` |
|         - |  4705 | `				}` |
|         - |  4706 | `/* OP_SUB * * *` |
|         - |  4707 | ` *` |
|         - |  4708 | ` * Pop the top two elements from the stack, subtract the` |
|         - |  4709 | ` * first (what was next on the stack) from the second (the` |
|         - |  4710 | ` * top of the stack) and push the result back onto the stack.` |
|         - |  4711 | ` */` |
|     46026 |  4712 | `case PH7_OP_SUB: {` |
|         - |  4713 | `	VmOpRc rcOp;` |
|     93379 |  4714 | `	sState.pTos = pTos;` |
|     93379 |  4715 | `	sState.pc = pc;` |
|     93379 |  4716 | `	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);` |
|     93379 |  4717 | `	pTos = sState.pTos;` |
|     93379 |  4718 | `	pc = sState.pc;` |
|     93379 |  4719 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  4720 | `		goto Abort;` |
|     93379 |  4721 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       162 |  4722 | `		goto Exception;` |
|         - |  4723 | `	}` |
|     93219 |  4724 | `	break;` |
|         - |  4725 | `					  }` |
|         - |  4726 | `/* OP_SUB_STORE * * *` |
|         - |  4727 | ` *` |
|         - |  4728 | ` * Pop the top two elements from the stack, subtract the` |
|         - |  4729 | ` * first (what was next on the stack) from the second (the` |
|         - |  4730 | ` * top of the stack) and push the result back onto the stack.` |
|         - |  4731 | ` */` |
|       205 |  4732 | `case PH7_OP_SUB_STORE: {` |
|         - |  4733 | `	VmOpRc rcOp;` |
|         - |  4734 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       412 |  4735 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       410 |  4736 | `	sState.pTos = pTos;` |
|       410 |  4737 | `	sState.pc = pc;` |
|       410 |  4738 | `	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);` |
|       410 |  4739 | `	pTos = sState.pTos;` |
|       410 |  4740 | `	pc = sState.pc;` |
|       410 |  4741 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  4742 | `		goto Abort;` |
|       410 |  4743 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       153 |  4744 | `		goto Exception;` |
|         - |  4745 | `	}` |
|       258 |  4746 | `	break;` |
|         - |  4747 | `					  }` |
|         - |  4748 |  |
|         - |  4749 | `/*` |
|         - |  4750 | ` * OP_MOD * * *` |
|         - |  4751 | ` *` |
|         - |  4752 | ` * Pop the top two elements from the stack, divide the` |
|         - |  4753 | ` * first (what was next on the stack) from the second (the` |
|         - |  4754 | ` * top of the stack) and push the remainder after division` |
|         - |  4755 | ` * onto the stack.` |
|         - |  4756 | ` * Note: Only integer arithemtic is allowed.` |
|         - |  4757 | ` */` |
|      1412 |  4758 | `case PH7_OP_MOD: {` |
|         - |  4759 | `	VmOpRc rcOp;` |
|      2829 |  4760 | `	sState.pTos = pTos;` |
|      2829 |  4761 | `	sState.pc = pc;` |
|      2829 |  4762 | `	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);` |
|      2829 |  4763 | `	pTos = sState.pTos;` |
|      2829 |  4764 | `	pc = sState.pc;` |
|      2829 |  4765 | `	if( rcOp == VM_OP_ABORT ){` |
|         3 |  4766 | `		goto Abort;` |
|      2827 |  4767 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       248 |  4768 | `		goto Exception;` |
|         - |  4769 | `	}` |
|      2581 |  4770 | `	break;` |
|         - |  4771 | `					  }` |
|         - |  4772 | `/*` |
|         - |  4773 | ` * OP_MOD_STORE * * *` |
|         - |  4774 | ` *` |
|         - |  4775 | ` * Pop the top two elements from the stack, divide the` |
|         - |  4776 | ` * first (what was next on the stack) from the second (the` |
|         - |  4777 | ` * top of the stack) and push the remainder after division` |
|         - |  4778 | ` * onto the stack.` |
|         - |  4779 | ` * Note: Only integer arithemtic is allowed.` |
|         - |  4780 | ` */` |
|       204 |  4781 | `case PH7_OP_MOD_STORE: {` |
|         - |  4782 | `	VmOpRc rcOp;` |
|         - |  4783 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       409 |  4784 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       407 |  4785 | `	sState.pTos = pTos;` |
|       407 |  4786 | `	sState.pc = pc;` |
|       407 |  4787 | `	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);` |
|       407 |  4788 | `	pTos = sState.pTos;` |
|       407 |  4789 | `	pc = sState.pc;` |
|       407 |  4790 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  4791 | `		goto Abort;` |
|       407 |  4792 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       221 |  4793 | `		goto Exception;` |
|         - |  4794 | `	}` |
|       187 |  4795 | `	break;` |
|         - |  4796 | `					  }` |
|         - |  4797 | `/*` |
|         - |  4798 | ` * OP_DIV * * *` |
|         - |  4799 | ` *` |
|         - |  4800 | ` * Pop the top two elements from the stack, divide the` |
|         - |  4801 | ` * first (what was next on the stack) from the second (the` |
|         - |  4802 | ` * top of the stack) and push the result onto the stack.` |
|         - |  4803 | ` * Note: Only floating point arithemtic is allowed.` |
|         - |  4804 | ` */` |
|       494 |  4805 | `case PH7_OP_DIV: {` |
|         - |  4806 | `	VmOpRc rcOp;` |
|       993 |  4807 | `	sState.pTos = pTos;` |
|       993 |  4808 | `	sState.pc = pc;` |
|       993 |  4809 | `	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);` |
|       993 |  4810 | `	pTos = sState.pTos;` |
|       993 |  4811 | `	pc = sState.pc;` |
|       993 |  4812 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  4813 | `		goto Abort;` |
|       993 |  4814 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       233 |  4815 | `		goto Exception;` |
|         - |  4816 | `	}` |
|       761 |  4817 | `	break;` |
|         - |  4818 | `					  }` |
|         - |  4819 | `/*` |
|         - |  4820 | ` * OP_DIV_STORE * * *` |
|         - |  4821 | ` *` |
|         - |  4822 | ` * Pop the top two elements from the stack, divide the` |
|         - |  4823 | ` * first (what was next on the stack) from the second (the` |
|         - |  4824 | ` * top of the stack) and push the result onto the stack.` |
|         - |  4825 | ` * Note: Only floating point arithemtic is allowed.` |
|         - |  4826 | ` */` |
|       211 |  4827 | `case PH7_OP_DIV_STORE:{` |
|       423 |  4828 | `	ph7_value *pNos = &pTos[-1];` |
|         - |  4829 | `	ph7_value *pObj;` |
|         - |  4830 | `	ph7_real a,b,r;` |
|         - |  4831 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|         - |  4832 | `	int rcNa;` |
|       423 |  4833 | `	const char *zArCls = "TypeError";` |
|         - |  4834 | `#ifdef UNTRUST` |
|         - |  4835 | `	if( pNos < pStack ){` |
|         - |  4836 | `		goto Abort;` |
|         - |  4837 | `	}` |
|         - |  4838 | `#endif` |
|         - |  4839 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       524 |  4840 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|         - |  4841 | `	{` |
|         - |  4842 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|         - |  4843 | `		 * array, object or resource operand is a TypeError too. */` |
|         - |  4844 | `		SyBlob sArMsg;` |
|       421 |  4845 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|       421 |  4846 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"/",pNos,&zArCls,&sArMsg);` |
|       421 |  4847 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|         - |  4848 | `			sxi32 rcAr;` |
|       151 |  4849 | `			VmPopOperand(&pTos,1);` |
|       151 |  4850 | `			PH7_MemObjRelease(pTos);` |
|       151 |  4851 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       151 |  4852 | `			pTos->nIdx = SXU32_HIGH;` |
|       226 |  4853 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|        75 |  4854 | `				SyBlobLength(&sArMsg));` |
|       151 |  4855 | `			SyBlobRelease(&sArMsg);` |
|       151 |  4856 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|       151 |  4857 | `			rc = rcAr;` |
|       151 |  4858 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  4859 | `		}` |
|       271 |  4860 | `		SyBlobRelease(&sArMsg);` |
|         - |  4861 | `	}` |
|       271 |  4862 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|         - |  4863 | ``		/* php's `/` answers an INT when both operands are ints and the division is`` |
|         - |  4864 | ``		 * exact (`6/3 === 2`, not `2.0`), and `$x /= $y` is that same operator: php`` |
|         - |  4865 | `		 * has one division and the compound form only decides where the answer goes.` |
|         - |  4866 | `		 * OP_DIV grew the rule (the int-boundary work) and this arm, a separate copy` |
|         - |  4867 | ``		 * of it, did not — so `$x = 6; $x /= 3;` left a FLOAT where `$x = $x / 3` left`` |
|         - |  4868 | ``		 * an int, visible through `===`, `var_dump`, `json_encode` and `is_int`.`` |
|         - |  4869 | ``		 * The divisor is screened BEFORE `ia % ib`: x86 computes the overflowing`` |
|         - |  4870 | `		 * PHP_INT_MIN/-1 quotient alongside the remainder and traps (OP_DIV and OP_MOD` |
|         - |  4871 | `		 * guard the same hazard the same way). */` |
|       269 |  4872 | `		int bExactDiv = 0;` |
|       269 |  4873 | `		PH7_MemObjToNumeric(pTos);` |
|       269 |  4874 | `		PH7_MemObjToNumeric(pNos);` |
|       269 |  4875 | `		if( ((pTos->iFlags\|pNos->iFlags) & MEMOBJ_REAL) == 0 ){` |
|       269 |  4876 | `			sxi64 ia = pTos->x.iVal;   /* the lvalue: php's dividend */` |
|       269 |  4877 | `			sxi64 ib = pNos->x.iVal;   /* the right operand: the divisor */` |
|       269 |  4878 | `			sxi64 iQuot = 0;` |
|       269 |  4879 | `			if( ib == 0 ){` |
|        71 |  4880 | `				rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|        73 |  4881 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|       199 |  4882 | `			}else if( ib == -1 ){` |
|         - |  4883 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  4884 | `				iQuot = ( ia != SMALLEST_INT64 ) ? -ia : SMALLEST_INT64;` |
|         - |  4885 | `				bExactDiv = 1;` |
|         - |  4886 | `#else` |
|         7 |  4887 | `				if( ia != SMALLEST_INT64 ){` |
|         3 |  4888 | `					iQuot = -ia;` |
|         3 |  4889 | `					bExactDiv = 1;` |
|         2 |  4890 | `				}` |
|         - |  4891 | `#endif` |
|       196 |  4892 | `			}else if( ia % ib == 0 ){` |
|       109 |  4893 | `				iQuot = ia / ib;` |
|       109 |  4894 | `				bExactDiv = 1;` |
|        54 |  4895 | `			}` |
|       199 |  4896 | `			if( bExactDiv ){` |
|       111 |  4897 | `				pNos->x.iVal = iQuot;` |
|       111 |  4898 | `				MemObjSetType(pNos,MEMOBJ_INT);` |
|        55 |  4899 | `			}` |
|        99 |  4900 | `		}` |
|       199 |  4901 | `		if( !bExactDiv ){` |
|         - |  4902 | `			/* Force the operands to be real */` |
|        89 |  4903 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        89 |  4904 | `				PH7_MemObjToReal(pTos);` |
|        44 |  4905 | `			}` |
|        89 |  4906 | `			if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        89 |  4907 | `				PH7_MemObjToReal(pNos);` |
|        44 |  4908 | `			}` |
|         - |  4909 | `			/* Perform the requested operation */` |
|        89 |  4910 | `			a = pTos->rVal;` |
|        89 |  4911 | `			b = pNos->rVal;` |
|        89 |  4912 | `			if( b == 0 ){` |
|         - |  4913 | `				/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|         - |  4914 | `				 * not the old non-catchable warning that continued with a 0 result. */` |
|       ! 0 |  4915 | `				rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|       ! 0 |  4916 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|       ! 0 |  4917 | `			}else{` |
|        89 |  4918 | `				r = a/b;` |
|         - |  4919 | `				/* Push the result */` |
|        89 |  4920 | `				pNos->rVal = r;` |
|        89 |  4921 | `				MemObjSetType(pNos,MEMOBJ_REAL);` |
|         - |  4922 | `			}` |
|        44 |  4923 | `		}` |
|        99 |  4924 | `	}` |
|       201 |  4925 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|         - |  4926 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|       201 |  4927 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       201 |  4928 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       201 |  4929 | `		PH7_MemObjStore(pNos,pObj);` |
|       100 |  4930 | `	}` |
|       201 |  4931 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       201 |  4932 | `	VmPopOperand(&pTos,1);` |
|       201 |  4933 | `	break;` |
|         - |  4934 | `				}` |
|         - |  4935 | `/* OP_BAND * * *` |
|         - |  4936 | ` *` |
|         - |  4937 | ` * Pop the top two elements from the stack.  Convert both elements` |
|         - |  4938 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|         - |  4939 | ` * two elements.` |
|         - |  4940 | `*/` |
|         - |  4941 | `/* OP_BOR * * *` |
|         - |  4942 | ` *` |
|         - |  4943 | ` * Pop the top two elements from the stack.  Convert both elements` |
|         - |  4944 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|         - |  4945 | ` * two elements.` |
|         - |  4946 | ` */` |
|         - |  4947 | `/* OP_BXOR * * *` |
|         - |  4948 | ` *` |
|         - |  4949 | ` * Pop the top two elements from the stack.  Convert both elements` |
|         - |  4950 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|         - |  4951 | ` * two elements.` |
|         - |  4952 | ` */` |
|      4626 |  4953 | `case PH7_OP_BAND:` |
|         - |  4954 | `case PH7_OP_BOR:` |
|         - |  4955 | `case PH7_OP_BXOR:{` |
|      9239 |  4956 | `	ph7_value *pNos = &pTos[-1];` |
|         - |  4957 | `	sxi64 a,b,r;` |
|         - |  4958 | `	int cBwOp;` |
|         - |  4959 | `#ifdef UNTRUST` |
|         - |  4960 | `	if( pNos < pStack ){` |
|         - |  4961 | `		goto Abort;` |
|         - |  4962 | `	}` |
|         - |  4963 | `#endif` |
|      9239 |  4964 | `	cBwOp = pInstr->iOp == PH7_OP_BOR ? '\|' : (pInstr->iOp == PH7_OP_BXOR ? '^' : '&');` |
|      9239 |  4965 | `	if( (pNos->iFlags & MEMOBJ_STRING) && (pTos->iFlags & MEMOBJ_STRING) ){` |
|         - |  4966 | `		/* TWO strings: php's per-byte string operation, result a string. */` |
|       149 |  4967 | `		PH7_STRING_BITWISE_RESULT(pNos,pNos,pTos,cBwOp)` |
|       149 |  4968 | `		VmPopOperand(&pTos,1);` |
|       149 |  4969 | `		break;` |
|         - |  4970 | `	}` |
|         - |  4971 | `	{` |
|         - |  4972 | `		char zBwOp[2];` |
|      9091 |  4973 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|         - |  4974 | `		/* Anything else is php's arithmetic operand contract, named for this` |
|         - |  4975 | ``		 * operator (`array & int`), not a silent integer cast. */`` |
|      9091 |  4976 | `		PH7_BITWISE_ARITH_CONTRACT(pNos,pTos,zBwOp,1)` |
|         - |  4977 | `	}` |
|         - |  4978 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|      8667 |  4979 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|      8667 |  4980 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      8661 |  4981 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      8661 |  4982 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      8661 |  4983 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       321 |  4984 | `		PH7_MemObjToInteger(pTos);` |
|       160 |  4985 | `	}` |
|      8661 |  4986 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|       323 |  4987 | `		PH7_MemObjToInteger(pNos);` |
|       161 |  4988 | `	}` |
|         - |  4989 | `	/* Perform the requested operation */` |
|      8661 |  4990 | `	a = pNos->x.iVal;` |
|      8661 |  4991 | `	b = pTos->x.iVal;` |
|      8661 |  4992 | `	switch(pInstr->iOp){` |
|       636 |  4993 | `	case PH7_OP_BOR_STORE:` |
|      1277 |  4994 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|       216 |  4995 | `	case PH7_OP_BXOR_STORE:` |
|       433 |  4996 | `	case PH7_OP_BXOR: r = a^b; break;` |
|      3484 |  4997 | `	case PH7_OP_BAND_STORE:` |
|      3468 |  4998 | `	case PH7_OP_BAND:` |
|      6957 |  4999 | `	default:          r = a&b; break;` |
|         - |  5000 | `	}` |
|         - |  5001 | `	/* Push the result */` |
|      8661 |  5002 | `	pNos->x.iVal = r;` |
|      8661 |  5003 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      8661 |  5004 | `	VmPopOperand(&pTos,1);` |
|      8661 |  5005 | `	break;` |
|         - |  5006 | `				 }` |
|         - |  5007 | `/* OP_BAND_STORE * * *` |
|         - |  5008 | ` *` |
|         - |  5009 | ` * Pop the top two elements from the stack.  Convert both elements` |
|         - |  5010 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|         - |  5011 | ` * two elements.` |
|         - |  5012 | `*/` |
|         - |  5013 | `/* OP_BOR_STORE * * *` |
|         - |  5014 | ` *` |
|         - |  5015 | ` * Pop the top two elements from the stack.  Convert both elements` |
|         - |  5016 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|         - |  5017 | ` * two elements.` |
|         - |  5018 | ` */` |
|         - |  5019 | `/* OP_BXOR_STORE * * *` |
|         - |  5020 | ` *` |
|         - |  5021 | ` * Pop the top two elements from the stack.  Convert both elements` |
|         - |  5022 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|         - |  5023 | ` * two elements.` |
|         - |  5024 | ` */` |
|       640 |  5025 | `case PH7_OP_BAND_STORE:` |
|         - |  5026 | `case PH7_OP_BOR_STORE:` |
|         - |  5027 | `case PH7_OP_BXOR_STORE:{` |
|      1281 |  5028 | `	ph7_value *pNos = &pTos[-1];` |
|         - |  5029 | `	ph7_value *pObj;` |
|         - |  5030 | `	sxi64 a,b,r;` |
|         - |  5031 | `	int cBwOp,bBwStr;` |
|         - |  5032 | `#ifdef UNTRUST` |
|         - |  5033 | `	if( pNos < pStack ){` |
|         - |  5034 | `		goto Abort;` |
|         - |  5035 | `	}` |
|         - |  5036 | `#endif` |
|         - |  5037 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      1281 |  5038 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      1275 |  5039 | `	cBwOp = pInstr->iOp == PH7_OP_BOR_STORE ? '\|' : (pInstr->iOp == PH7_OP_BXOR_STORE ? '^' : '&');` |
|      1275 |  5040 | `	bBwStr = (pNos->iFlags & MEMOBJ_STRING) != 0 && (pTos->iFlags & MEMOBJ_STRING) != 0;` |
|      1275 |  5041 | `	if( !bBwStr ){` |
|         - |  5042 | `		char zBwOp[2];` |
|      1173 |  5043 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|         - |  5044 | ``		/* `$x &= v` answers the same contract as `$x & v` (php's compound`` |
|         - |  5045 | `		 * assignment is the operator plus a store), but through its own error` |
|         - |  5046 | `		 * path, which is POSITIONAL: the lvalue is named first even when the` |
|         - |  5047 | ``		 * right operand is the offender (`$x = 1; $x &= $o` is "int & BsocP",`` |
|         - |  5048 | ``		 * where the plain `1 & $o` is "BsocP & int"). */`` |
|      1173 |  5049 | `		PH7_BITWISE_ARITH_CONTRACT(pTos,pNos,zBwOp,0)` |
|         - |  5050 | `		/* Force the operands to be integer (php deprecates a lossy float here) */` |
|       791 |  5051 | `		rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|       791 |  5052 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|       791 |  5053 | `		rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       791 |  5054 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|       791 |  5055 | `		if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       309 |  5056 | `			PH7_MemObjToInteger(pTos);` |
|       154 |  5057 | `		}` |
|       791 |  5058 | `		if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|       309 |  5059 | `			PH7_MemObjToInteger(pNos);` |
|       154 |  5060 | `		}` |
|       395 |  5061 | `	}` |
|       893 |  5062 | `	if( bBwStr ){` |
|         - |  5063 | `		/* TWO strings: php's per-byte string operation, result a string. The` |
|         - |  5064 | `		 * result lands in pNos, which the store tail below writes into the` |
|         - |  5065 | `		 * lvalue's slot exactly like the integer result. */` |
|       103 |  5066 | `		PH7_STRING_BITWISE_RESULT(pNos,pTos,pNos,cBwOp)` |
|        52 |  5067 | `	}else{` |
|         - |  5068 | `	/* Perform the requested operation */` |
|       791 |  5069 | `	a = pTos->x.iVal;` |
|       791 |  5070 | `	b = pNos->x.iVal;` |
|       791 |  5071 | `	switch(pInstr->iOp){` |
|       151 |  5072 | `	case PH7_OP_BOR_STORE:` |
|       303 |  5073 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|       122 |  5074 | `	case PH7_OP_BXOR_STORE:` |
|       245 |  5075 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       122 |  5076 | `	case PH7_OP_BAND_STORE:` |
|       122 |  5077 | `	case PH7_OP_BAND:` |
|       245 |  5078 | `	default:          r = a&b; break;` |
|         - |  5079 | `	}` |
|         - |  5080 | `	/* Push the result */` |
|       791 |  5081 | `	pNos->x.iVal = r;` |
|       791 |  5082 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|         - |  5083 | `	}` |
|       893 |  5084 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|         - |  5085 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|       892 |  5086 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       891 |  5087 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       891 |  5088 | `		PH7_MemObjStore(pNos,pObj);` |
|       445 |  5089 | `	}` |
|       893 |  5090 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       893 |  5091 | `	VmPopOperand(&pTos,1);` |
|       893 |  5092 | `	break;` |
|         - |  5093 | `				 }` |
|         - |  5094 | `/* OP_SHL * * *` |
|         - |  5095 | ` *` |
|         - |  5096 | ` * Pop the top two elements from the stack.  Convert both elements` |
|         - |  5097 | ` * to integers.  Push back onto the stack the second element shifted` |
|         - |  5098 | ` * left by N bits where N is the top element on the stack.` |
|         - |  5099 | ` * Note: Only integer arithmetic is allowed.` |
|         - |  5100 | ` */` |
|         - |  5101 | `/* OP_SHR * * *` |
|         - |  5102 | ` *` |
|         - |  5103 | ` * Pop the top two elements from the stack.  Convert both elements` |
|         - |  5104 | ` * to integers.  Push back onto the stack the second element shifted` |
|         - |  5105 | ` * right by N bits where N is the top element on the stack.` |
|         - |  5106 | ` * Note: Only integer arithmetic is allowed.` |
|         - |  5107 | ` */` |
|      1686 |  5108 | `case PH7_OP_SHL:` |
|         - |  5109 | `case PH7_OP_SHR: {` |
|         - |  5110 | `	VmOpRc rcOp;` |
|      3375 |  5111 | `	sState.pTos = pTos;` |
|      3375 |  5112 | `	sState.pc = pc;` |
|      3375 |  5113 | `	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);` |
|      3375 |  5114 | `	pTos = sState.pTos;` |
|      3375 |  5115 | `	pc = sState.pc;` |
|      3375 |  5116 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  5117 | `		goto Abort;` |
|      3375 |  5118 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       394 |  5119 | `		goto Exception;` |
|         - |  5120 | `	}` |
|      2982 |  5121 | `	break;` |
|         - |  5122 | `					  }` |
|         - |  5123 | `/*  OP_SHL_STORE * * *` |
|         - |  5124 | ` *` |
|         - |  5125 | ` * Pop the top two elements from the stack.  Convert both elements` |
|         - |  5126 | ` * to integers.  Push back onto the stack the second element shifted` |
|         - |  5127 | ` * left by N bits where N is the top element on the stack.` |
|         - |  5128 | ` * Note: Only integer arithmetic is allowed.` |
|         - |  5129 | ` */` |
|         - |  5130 | `/* OP_SHR_STORE * * *` |
|         - |  5131 | ` *` |
|         - |  5132 | ` * Pop the top two elements from the stack.  Convert both elements` |
|         - |  5133 | ` * to integers.  Push back onto the stack the second element shifted` |
|         - |  5134 | ` * right by N bits where N is the top element on the stack.` |
|         - |  5135 | ` * Note: Only integer arithmetic is allowed.` |
|         - |  5136 | ` */` |
|       414 |  5137 | `case PH7_OP_SHL_STORE:` |
|         - |  5138 | `case PH7_OP_SHR_STORE: {` |
|         - |  5139 | `	VmOpRc rcOp;` |
|         - |  5140 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       829 |  5141 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       825 |  5142 | `	sState.pTos = pTos;` |
|       825 |  5143 | `	sState.pc = pc;` |
|       825 |  5144 | `	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);` |
|       825 |  5145 | `	pTos = sState.pTos;` |
|       825 |  5146 | `	pc = sState.pc;` |
|       825 |  5147 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  5148 | `		goto Abort;` |
|       825 |  5149 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       353 |  5150 | `		goto Exception;` |
|         - |  5151 | `	}` |
|       473 |  5152 | `	break;` |
|         - |  5153 | `					  }` |
|         - |  5154 | `/* CAT:  P1 * *` |
|         - |  5155 | ` *` |
|         - |  5156 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|         - |  5157 | ` * back.` |
|         - |  5158 | ` */` |
|   4085383 |  5159 | `case PH7_OP_CAT:{` |
|         - |  5160 | `	ph7_value *pNos,*pCur;` |
|   8170625 |  5161 | `	if( pInstr->iP1 < 1 ){` |
|   8105296 |  5162 | `		pNos = &pTos[-1];` |
|   4052652 |  5163 | `	}else{` |
|     65334 |  5164 | `		pNos = &pTos[-pInstr->iP1+1];` |
|         - |  5165 | `	}` |
|         - |  5166 | `#ifdef UNTRUST` |
|         - |  5167 | `	if( pNos < pStack ){` |
|         - |  5168 | `		goto Abort;` |
|         - |  5169 | `	}` |
|         - |  5170 | `#endif` |
|         - |  5171 | `	/* Force a string cast (user-visible: warns on an array operand).` |
|         - |  5172 | `	 * php coerces the operands left to right, so the leftmost not-stringable` |
|         - |  5173 | `	 * object is the one that throws. */` |
|         - |  5174 | `	{` |
|   8170625 |  5175 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|   8170625 |  5176 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|         - |  5177 | `	}` |
|   8170623 |  5178 | `	pCur = &pNos[1];` |
|         - |  5179 | `	{` |
|         - |  5180 | `		/* rcSv leaves the loop with the coercion status: the routing macro expands` |
|         - |  5181 | ``		 * to a bare `break` here, which inside the while would fall THROUGH to the`` |
|         - |  5182 | ``		 * `pTos = pNos` below with a throw pending. Route it at case level. */`` |
|   8170623 |  5183 | `		sxi32 rcSv = SXRET_OK;` |
|  16424397 |  5184 | `		while( pCur <= pTos ){` |
|   8254223 |  5185 | `			rcSv = PH7_MemObjToStringUV(pCur);` |
|   8254223 |  5186 | `			if( rcSv != SXRET_OK ){` |
|       448 |  5187 | `				break;` |
|         - |  5188 | `			}` |
|         - |  5189 | `			/* Perform the concatenation */` |
|   8253779 |  5190 | `			if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   8252051 |  5191 | `				if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|         - |  5192 | `					/* Allocation failure: raise a fatal instead of a truncated concat */` |
|       ! 0 |  5193 | `					PH7_VmMemoryError(&(*pVm));` |
|       ! 0 |  5194 | `					goto Abort;` |
|         - |  5195 | `				}` |
|   4125809 |  5196 | `			}` |
|   8253779 |  5197 | `			SyBlobRelease(&pCur->sBlob);` |
|   8253779 |  5198 | `			pCur++;` |
|         5 |  5199 | `		}` |
|   8171439 |  5200 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|         - |  5201 | `	}` |
|   8170179 |  5202 | `	pTos = pNos;` |
|   8170179 |  5203 | `	break;` |
|         - |  5204 | `				}` |
|         - |  5205 | `/*  CAT_STORE: * * *` |
|         - |  5206 | ` *` |
|         - |  5207 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|         - |  5208 | ` * back.` |
|         - |  5209 | ` */` |
|   3743738 |  5210 | `case PH7_OP_CAT_STORE:{` |
|  14885313 |  5211 | `	ph7_value *pNos = &pTos[-1];` |
|         - |  5212 | `	ph7_value *pObj;` |
|         - |  5213 | `	sxu32 nIdx;` |
|         - |  5214 | `#ifdef UNTRUST` |
|         - |  5215 | `	if( pNos < pStack ){` |
|         - |  5216 | `		goto Abort;` |
|         - |  5217 | `	}` |
|         - |  5218 | `#endif` |
|         - |  5219 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|  14885313 |  5220 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|         - |  5221 | `	/* The right operand must be a string to append it (user-visible) */` |
|         - |  5222 | `	{` |
|  14885305 |  5223 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|  14885309 |  5224 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|         - |  5225 | `	}` |
|  14885295 |  5226 | `	nIdx = pTos->nIdx;` |
|         - |  5227 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|         - |  5228 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|         - |  5229 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|         - |  5230 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|         - |  5231 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|         - |  5232 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|         - |  5233 | `	 * the source we copy from — references share the slot index, so one check` |
|         - |  5234 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|         - |  5235 | `	 * must run before any mutation (left to the slow path).` |
|         - |  5236 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|         - |  5237 | `	 * and remains O(n^2) by design. */` |
|  14885290 |  5238 | `	if( nIdx != SXU32_HIGH` |
|  14885288 |  5239 | `	 && nIdx != pNos->nIdx` |
|   7486614 |  5240 | `	 && (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx)) != 0` |
|   7486615 |  5241 | `	 && !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|         - |  5242 | `		/* e.g. $x = 5; $x .= "a";  ->  "5a" (user-visible: warns if $x is an array,` |
|         - |  5243 | `		 * throws if it is a not-stringable object — and then the lvalue keeps` |
|         - |  5244 | `		 * holding that object, since the throw abandons the coercion) */` |
|         - |  5245 | `		{` |
|   7486603 |  5246 | `			sxi32 rcSv = PH7_MemObjToStringUV(pObj);` |
|   7486611 |  5247 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|         - |  5248 | `		}` |
|   7486595 |  5249 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|   7485839 |  5250 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|         - |  5251 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|         - |  5252 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|       ! 0 |  5253 | `				PH7_VmMemoryError(&(*pVm));` |
|       ! 0 |  5254 | `				goto Abort;` |
|         - |  5255 | `			}` |
|   3742499 |  5256 | `		}` |
|         - |  5257 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|         - |  5258 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|         - |  5259 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|         - |  5260 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|         - |  5261 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|         - |  5262 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|         - |  5263 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|         - |  5264 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|         - |  5265 | `		 * the same slot is appended to again later in the statement` |
|         - |  5266 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|         - |  5267 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|         - |  5268 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|   7486595 |  5269 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        18 |  5270 | `			PH7_MemObjStore(pObj,pNos);` |
|         8 |  5271 | `		}` |
|         - |  5272 | `		/* A hooked lvalue's scratch slot was appended in place — dispatch the` |
|         - |  5273 | `		 * set side now (the consume reads the computed value from the slot). */` |
|   7486595 |  5274 | `		PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|   7486595 |  5275 | `		pNos->nIdx = SXU32_HIGH;` |
|   7486595 |  5276 | `		VmPopOperand(&pTos,1);` |
|   7486595 |  5277 | `		break;` |
|         - |  5278 | `	}` |
|         - |  5279 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|         - |  5280 | `	/* Force a string cast (user-visible: warns if the lvalue is an array) */` |
|         - |  5281 | `	{` |
|  14797366 |  5282 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|  14797366 |  5283 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|         - |  5284 | `	}` |
|         - |  5285 | `	/* Perform the concatenation (Reverse order) */` |
|        26 |  5286 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|        26 |  5287 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|         - |  5288 | `			/* Allocation failure: raise a fatal before committing the store so` |
|         - |  5289 | `			 * no partially-concatenated value is written to the lvalue. */` |
|       ! 0 |  5290 | `			PH7_VmMemoryError(&(*pVm));` |
|       ! 0 |  5291 | `			goto Abort;` |
|         - |  5292 | `		}` |
|        12 |  5293 | `	}` |
|         - |  5294 | `	/* Perform the store operation */` |
|        26 |  5295 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|         - |  5296 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|        24 |  5297 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        26 |  5298 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|        17 |  5299 | `		PH7_MemObjStore(pTos,pObj);` |
|         8 |  5300 | `	}` |
|        21 |  5301 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|        21 |  5302 | `	PH7_MemObjStore(pTos,pNos);` |
|        21 |  5303 | `	VmPopOperand(&pTos,1);` |
|        21 |  5304 | `	break;` |
|         - |  5305 | `				}` |
|         - |  5306 | `/* OP_AND: * * *` |
|         - |  5307 | ` *` |
|         - |  5308 | ` * Pop two values off the stack.  Take the logical AND of the` |
|         - |  5309 | ` * two values and push the resulting boolean value back onto the` |
|         - |  5310 | ` * stack.` |
|         - |  5311 | ` */` |
|         - |  5312 | `/* OP_OR: * * *` |
|         - |  5313 | ` *` |
|         - |  5314 | ` * Pop two values off the stack.  Take the logical OR of the` |
|         - |  5315 | ` * two values and push the resulting boolean value back onto the` |
|         - |  5316 | ` * stack.` |
|         - |  5317 | ` */` |
|   6805940 |  5318 | `case PH7_OP_LAND:` |
|         - |  5319 | `case PH7_OP_LOR: {` |
|         - |  5320 | `	VmOpRc rcOp;` |
|  13613207 |  5321 | `	sState.pTos = pTos;` |
|  13613207 |  5322 | `	sState.pc = pc;` |
|  13613207 |  5323 | `	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);` |
|  13613207 |  5324 | `	pTos = sState.pTos;` |
|  13613207 |  5325 | `	pc = sState.pc;` |
|  13613207 |  5326 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  5327 | `		goto Abort;` |
|  13613207 |  5328 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       ! 0 |  5329 | `		goto Exception;` |
|         - |  5330 | `	}` |
|  13613207 |  5331 | `	break;` |
|         - |  5332 | `					  }` |
|         - |  5333 | `/*` |
|         - |  5334 | ` * OP_NULLC: * * *` |
|         - |  5335 | ` * Null coalescing operator '??'.` |
|         - |  5336 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|         - |  5337 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|         - |  5338 | ` */` |
|         - |  5339 | `/*` |
|         - |  5340 | ` * OP_NULLC: * P2 *` |
|         - |  5341 | ` * Short-circuit null coalescing '??'.` |
|         - |  5342 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|         - |  5343 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|         - |  5344 | ` */` |
|      2679 |  5345 | `case PH7_OP_NULLC: {` |
|         - |  5346 | `#ifdef UNTRUST` |
|         - |  5347 | `	if( pTos < pStack ){` |
|         - |  5348 | `		goto Abort;` |
|         - |  5349 | `	}` |
|         - |  5350 | `#endif` |
|      5363 |  5351 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - |  5352 | `		/* Left is not null — keep it and skip the RHS */` |
|      3139 |  5353 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|      1572 |  5354 | `	}else{` |
|         - |  5355 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|      2229 |  5356 | `		VmPopOperand(&pTos, 1);` |
|         - |  5357 | `	}` |
|      5363 |  5358 | `	break;` |
|         - |  5359 | `}` |
|         - |  5360 | `/*` |
|         - |  5361 | ` * OP_NULLC_JMP: * P2 *` |
|         - |  5362 | ` * Null coalescing assignment short-circuit.` |
|         - |  5363 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|         - |  5364 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|         - |  5365 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|         - |  5366 | ` */` |
|       102 |  5367 | `case PH7_OP_NULLC_JMP: {` |
|         - |  5368 | `#ifdef UNTRUST` |
|         - |  5369 | `	if( pTos < pStack ){` |
|         - |  5370 | `		goto Abort;` |
|         - |  5371 | `	}` |
|         - |  5372 | `#endif` |
|       208 |  5373 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        73 |  5374 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —` |
|         - |  5375 | `		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is` |
|         - |  5376 | `		 * dropped by the fetch-point sweep (the landing pc is past its` |
|         - |  5377 | `		 * OP_NULLC_STORE window): the skipped assign never dispatches. */` |
|        35 |  5378 | `	}` |
|       208 |  5379 | `	break;` |
|         - |  5380 | `}` |
|         - |  5381 | `/*` |
|         - |  5382 | ` * OP_NULLC_STORE: * * *` |
|         - |  5383 | ` * Null coalescing assignment store.` |
|         - |  5384 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|         - |  5385 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|         - |  5386 | ` * expression result.` |
|         - |  5387 | ` */` |
|         - |  5388 | `/*` |
|         - |  5389 | ` * OP_NULLSAFE_JMP: * P2 *` |
|         - |  5390 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|         - |  5391 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|         - |  5392 | ` * on the stack as the result of the entire containing postfix chain. If` |
|         - |  5393 | ` * non-null, fall through without modifying the stack so the following` |
|         - |  5394 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|         - |  5395 | ` */` |
|       270 |  5396 | `case PH7_OP_NULLSAFE_JMP: {` |
|         - |  5397 | `#ifdef UNTRUST` |
|         - |  5398 | `	if( pTos < pStack ){` |
|         - |  5399 | `		goto Abort;` |
|         - |  5400 | `	}` |
|         - |  5401 | `#endif` |
|       545 |  5402 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|         - |  5403 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|         - |  5404 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       114 |  5405 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        56 |  5406 | `	}` |
|       545 |  5407 | `	break;` |
|         - |  5408 | `}` |
|        64 |  5409 | `case PH7_OP_NULLC_STORE: {` |
|         - |  5410 | `	VmOpRc rcOp;` |
|       132 |  5411 | `	sState.pTos = pTos;` |
|       132 |  5412 | `	sState.pc = pc;` |
|       132 |  5413 | `	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);` |
|       132 |  5414 | `	pTos = sState.pTos;` |
|       132 |  5415 | `	pc = sState.pc;` |
|       132 |  5416 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  5417 | `		goto Abort;` |
|       132 |  5418 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        11 |  5419 | `		goto Exception;` |
|         - |  5420 | `	}` |
|       122 |  5421 | `	break;` |
|         - |  5422 | `					  }` |
|         - |  5423 | `/*` |
|         - |  5424 | ` * OP_SPREAD: * * *` |
|         - |  5425 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|         - |  5426 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|         - |  5427 | ` * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this` |
|         - |  5428 | ` * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —` |
|         - |  5429 | ` * the CALL may not be the next instruction, and may be an inner call whose own` |
|         - |  5430 | ` * spreads must stay scoped to it.` |
|         - |  5431 | ` * The expansion tail is shared between the plain-array and the materialized` |
|         - |  5432 | ` * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.` |
|         - |  5433 | ` */` |
|       908 |  5434 | `case PH7_OP_SPREAD: {` |
|         - |  5435 | `#ifdef UNTRUST` |
|         - |  5436 | `	if( pTos < pStack ){` |
|         - |  5437 | `		goto Abort;` |
|         - |  5438 | `	}` |
|         - |  5439 | `#endif` |
|         - |  5440 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|         - |  5441 | `	 * temp array (positional values), then expand it onto the operand stack` |
|         - |  5442 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|         - |  5443 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|         - |  5444 | `	 * be freed immediately. */` |
|      1726 |  5445 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        60 |  5446 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|         - |  5447 | `		sxi32 rcW;` |
|        60 |  5448 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        60 |  5449 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        60 |  5450 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|        31 |  5451 | `			sxi32 rcRoute = SXRET_OK; /* the throw already happened; route, do not re-raise */` |
|        31 |  5452 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|        31 |  5453 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|         - |  5454 | ``			/* A bare `goto Exception` unwinds the whole invocation, which is right at a`` |
|         - |  5455 | `			 * CALL boundary and wrong here: an argument list is mid-expression, so a` |
|         - |  5456 | `			 * try/catch around the call catches this and execution must RESUME after` |
|         - |  5457 | `			 * the catch. It did not — the catch ran and every statement after it was` |
|         - |  5458 | ``			 * dropped, exit 0 (a throwing rewind()/key() in `f(...$it)` showed it long`` |
|         - |  5459 | `			 * before the key screen made the path ordinary). */` |
|        31 |  5460 | `			PH7_THROW_ROUTE_MIDEXPR(rcRoute)` |
|         - |  5461 | `		}` |
|         - |  5462 | `		/* Grow the operand stack if this expansion would overflow it (no longer a` |
|         - |  5463 | `		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a` |
|         - |  5464 | `		 * single argument — the historical bounded-fallback, now only under OOM. */` |
|        45 |  5465 | `		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,` |
|        14 |  5466 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|       ! 0 |  5467 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|       ! 0 |  5468 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|         - |  5469 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|       ! 0 |  5470 | `				pTmpMap->nEntry);` |
|       ! 0 |  5471 | `			break;` |
|         - |  5472 | `		}` |
|         - |  5473 | `		/* The buffer may have MOVED (and grown): re-anchor the watermark at the whole` |
|         - |  5474 | `		 * new capacity rather than carry a pointer into the freed one. Conservative --` |
|         - |  5475 | `		 * this activation's teardown then sweeps everything -- and OP_SPREAD is rare. */` |
|        31 |  5476 | `		pHigh = pStack + sState.nStackCap - 1;` |
|        31 |  5477 | `		VmSpreadExpandMap(pVm, &pTos, pTmpMap, 0/*a Traversable's values are not the caller's slots*/);` |
|        31 |  5478 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        31 |  5479 | `		break;` |
|         - |  5480 | `	}` |
|      1670 |  5481 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      1610 |  5482 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      2365 |  5483 | `		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,` |
|       755 |  5484 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|       ! 0 |  5485 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|         - |  5486 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|       ! 0 |  5487 | `				pMap->nEntry);` |
|       ! 0 |  5488 | `			break;` |
|         - |  5489 | `		}` |
|      1610 |  5490 | `		pHigh = pStack + sState.nStackCap - 1;   /* see the Traversable arm above */` |
|      1610 |  5491 | `		VmSpreadExpandMap(pVm, &pTos, pMap, pInstr->iP1 != 0);` |
|      1610 |  5492 | `		break;` |
|         - |  5493 | `	}` |
|         - |  5494 | `	/* Neither an array nor a Traversable: php refuses the unpack rather than` |
|         - |  5495 | `	 * passing the value as one ordinary argument, which is what this used to do —` |
|         - |  5496 | ``	 * `f(...'str')` bound "str" to the first parameter and `new C(...null)` bound`` |
|         - |  5497 | `	 * null, silently, on source php will not run. The argument site's class is` |
|         - |  5498 | `	 * TypeError for every type (the array-literal site keeps php's plain Error for` |
|         - |  5499 | `	 * a scalar). */` |
|         - |  5500 | `	{` |
|        63 |  5501 | `		sxi32 rcBad = VmThrowSpreadError(&(*pVm),pTos,1);` |
|        63 |  5502 | `		sxi32 rcRoute = SXRET_OK;` |
|        63 |  5503 | `		if( rcBad == PH7_ABORT \|\| rcBad == SXERR_ABORT ){` |
|       ! 0 |  5504 | `			goto Abort;` |
|         - |  5505 | `		}` |
|        63 |  5506 | `		PH7_THROW_ROUTE_MIDEXPR(rcRoute)` |
|         - |  5507 | `	}` |
|         - |  5508 | `}` |
|         - |  5509 | `/*` |
|         - |  5510 | ` * OP_FLAG_SPREAD: * * *` |
|         - |  5511 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|         - |  5512 | ` * Used by array literal unpacking '[...$arr]'.` |
|         - |  5513 | ` */` |
|       375 |  5514 | `case PH7_OP_FLAG_SPREAD: {` |
|         - |  5515 | `#ifdef UNTRUST` |
|         - |  5516 | `	if( pTos < pStack ){` |
|         - |  5517 | `		goto Abort;` |
|         - |  5518 | `	}` |
|         - |  5519 | `#endif` |
|       754 |  5520 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       754 |  5521 | `	break;` |
|         - |  5522 | `}` |
|         - |  5523 | `/* OP_LXOR: * * *` |
|         - |  5524 | ` *` |
|         - |  5525 | ` * Pop two values off the stack. Take the logical XOR of the` |
|         - |  5526 | ` * two values and push the resulting boolean value back onto the` |
|         - |  5527 | ` * stack.` |
|         - |  5528 | ` * According to the PHP language reference manual:` |
|         - |  5529 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|         - |  5530 | ` *  TRUE,but not both.` |
|         - |  5531 | ` */` |
|         6 |  5532 | `case PH7_OP_LXOR:{` |
|        13 |  5533 | `	ph7_value *pNos = &pTos[-1];` |
|        13 |  5534 | `	sxi32 v = 0;` |
|         - |  5535 | `#ifdef UNTRUST` |
|         - |  5536 | `	if( pNos < pStack ){` |
|         - |  5537 | `		goto Abort;` |
|         - |  5538 | `	}` |
|         - |  5539 | `#endif` |
|         - |  5540 | `	/* Force a boolean cast */` |
|        13 |  5541 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       ! 0 |  5542 | `		PH7_MemObjToBool(pTos);` |
|       ! 0 |  5543 | `	}` |
|        13 |  5544 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       ! 0 |  5545 | `		PH7_MemObjToBool(pNos);` |
|       ! 0 |  5546 | `	}` |
|        13 |  5547 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|         7 |  5548 | `		v = 1;` |
|         3 |  5549 | `	}` |
|        13 |  5550 | `	VmPopOperand(&pTos,1);` |
|        13 |  5551 | `	pTos->x.iVal = v;` |
|        13 |  5552 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|        13 |  5553 | `	break;` |
|         - |  5554 | `				 }` |
|         - |  5555 | `/* OP_EQ P1 P2 P3` |
|         - |  5556 | ` *` |
|         - |  5557 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|         - |  5558 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|         - |  5559 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|         - |  5560 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|         - |  5561 | ` */` |
|         - |  5562 | `/* OP_NEQ P1 P2 P3` |
|         - |  5563 | ` *` |
|         - |  5564 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|         - |  5565 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|         - |  5566 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|         - |  5567 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|         - |  5568 | ` */` |
|      9028 |  5569 | `case PH7_OP_EQ:` |
|         - |  5570 | `case PH7_OP_NEQ: {` |
|         - |  5571 | `	VmOpRc rcOp;` |
|     18046 |  5572 | `	sState.pTos = pTos;` |
|     18046 |  5573 | `	sState.pc = pc;` |
|     18046 |  5574 | `	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);` |
|     18046 |  5575 | `	pTos = sState.pTos;` |
|     18046 |  5576 | `	pc = sState.pc;` |
|     18046 |  5577 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  5578 | `		goto Abort;` |
|     18046 |  5579 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        17 |  5580 | `		goto Exception;` |
|         - |  5581 | `	}` |
|     18030 |  5582 | `	break;` |
|         - |  5583 | `					  }` |
|         - |  5584 | `/* OP_TEQ P1 P2 *` |
|         - |  5585 | ` *` |
|         - |  5586 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|         - |  5587 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|         - |  5588 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|         - |  5589 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|         - |  5590 | ` */` |
|   3940816 |  5591 | `case PH7_OP_TEQ: {` |
|         - |  5592 | `	VmOpRc rcOp;` |
|   7883736 |  5593 | `	sState.pTos = pTos;` |
|   7883736 |  5594 | `	sState.pc = pc;` |
|   7883736 |  5595 | `	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);` |
|   7883736 |  5596 | `	pTos = sState.pTos;` |
|   7883736 |  5597 | `	pc = sState.pc;` |
|   7883736 |  5598 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  5599 | `		goto Abort;` |
|   7883736 |  5600 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       ! 0 |  5601 | `		goto Exception;` |
|         - |  5602 | `	}` |
|   7883736 |  5603 | `	break;` |
|         - |  5604 | `					  }` |
|         - |  5605 | `/* OP_TNE P1 P2 *` |
|         - |  5606 | ` *` |
|         - |  5607 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|         - |  5608 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|         - |  5609 | ` * instruction.` |
|         - |  5610 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|         - |  5611 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|         - |  5612 | ` *` |
|         - |  5613 | ` */` |
|    721618 |  5614 | `case PH7_OP_TNE: {` |
|         - |  5615 | `	VmOpRc rcOp;` |
|   1444393 |  5616 | `	sState.pTos = pTos;` |
|   1444393 |  5617 | `	sState.pc = pc;` |
|   1444393 |  5618 | `	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);` |
|   1444393 |  5619 | `	pTos = sState.pTos;` |
|   1444393 |  5620 | `	pc = sState.pc;` |
|   1444393 |  5621 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  5622 | `		goto Abort;` |
|   1444393 |  5623 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       ! 0 |  5624 | `		goto Exception;` |
|         - |  5625 | `	}` |
|   1444393 |  5626 | `	break;` |
|         - |  5627 | `					  }` |
|         - |  5628 | `/* OP_LT P1 P2 P3` |
|         - |  5629 | ` *` |
|         - |  5630 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|         - |  5631 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|         - |  5632 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|         - |  5633 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|         - |  5634 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|         - |  5635 | ` *` |
|         - |  5636 | ` */` |
|         - |  5637 | `/* OP_LE P1 P2 P3` |
|         - |  5638 | ` *` |
|         - |  5639 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|         - |  5640 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|         - |  5641 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|         - |  5642 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|         - |  5643 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|         - |  5644 | ` *` |
|         - |  5645 | ` */` |
|  11260708 |  5646 | `case PH7_OP_LT:` |
|         - |  5647 | `case PH7_OP_LE: {` |
|         - |  5648 | `	VmOpRc rcOp;` |
|  22525273 |  5649 | `	sState.pTos = pTos;` |
|  22525273 |  5650 | `	sState.pc = pc;` |
|  22525273 |  5651 | `	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);` |
|  22525273 |  5652 | `	pTos = sState.pTos;` |
|  22525273 |  5653 | `	pc = sState.pc;` |
|  22525273 |  5654 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  5655 | `		goto Abort;` |
|  22525273 |  5656 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|         5 |  5657 | `		goto Exception;` |
|         - |  5658 | `	}` |
|  22525269 |  5659 | `	break;` |
|         - |  5660 | `					  }` |
|         - |  5661 | `/* OP_GT P1 P2 P3` |
|         - |  5662 | ` *` |
|         - |  5663 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|         - |  5664 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|         - |  5665 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|         - |  5666 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|         - |  5667 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|         - |  5668 | ` *` |
|         - |  5669 | ` */` |
|         - |  5670 | `/* OP_GE P1 P2 P3` |
|         - |  5671 | ` *` |
|         - |  5672 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|         - |  5673 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|         - |  5674 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|         - |  5675 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|         - |  5676 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|         - |  5677 | ` *` |
|         - |  5678 | ` */` |
|   3418338 |  5679 | `case PH7_OP_GT:` |
|         - |  5680 | `case PH7_OP_GE: {` |
|         - |  5681 | `	VmOpRc rcOp;` |
|   6838050 |  5682 | `	sState.pTos = pTos;` |
|   6838050 |  5683 | `	sState.pc = pc;` |
|   6838050 |  5684 | `	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);` |
|   6838050 |  5685 | `	pTos = sState.pTos;` |
|   6838050 |  5686 | `	pc = sState.pc;` |
|   6838050 |  5687 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  5688 | `		goto Abort;` |
|   6838050 |  5689 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       ! 0 |  5690 | `		goto Exception;` |
|         - |  5691 | `	}` |
|   6838050 |  5692 | `	break;` |
|         - |  5693 | `					  }` |
|         - |  5694 | `/* OP_SPACESHIP * * *` |
|         - |  5695 | ` *` |
|         - |  5696 | ` * Pop the top two elements from the stack. Push an integer result:` |
|         - |  5697 | ` *   -1 if left < right` |
|         - |  5698 | ` *    0 if left == right` |
|         - |  5699 | ` *    1 if left > right` |
|         - |  5700 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|         - |  5701 | ` */` |
|       961 |  5702 | `case PH7_OP_SPACESHIP: {` |
|         - |  5703 | `	VmOpRc rcOp;` |
|      1927 |  5704 | `	sState.pTos = pTos;` |
|      1927 |  5705 | `	sState.pc = pc;` |
|      1927 |  5706 | `	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);` |
|      1927 |  5707 | `	pTos = sState.pTos;` |
|      1927 |  5708 | `	pc = sState.pc;` |
|      1927 |  5709 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  5710 | `		goto Abort;` |
|      1927 |  5711 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|         7 |  5712 | `		goto Exception;` |
|         - |  5713 | `	}` |
|      1921 |  5714 | `	break;` |
|         - |  5715 | `					  }` |
|         - |  5716 | `/*` |
|         - |  5717 | ` * OP_LOAD_REF * * *` |
|         - |  5718 | ` * Push the index of a referenced object on the stack.` |
|         - |  5719 | ` */` |
|       102 |  5720 | `case PH7_OP_LOAD_REF: {` |
|         - |  5721 | `	sxu32 nIdx;` |
|         - |  5722 | `#ifdef UNTRUST` |
|         - |  5723 | `	if( pTos < pStack ){` |
|         - |  5724 | `		goto Abort;` |
|         - |  5725 | `	}` |
|         - |  5726 | `#endif` |
|       207 |  5727 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|         - |  5728 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|         - |  5729 | `		 * out of a string still carries the BASE VARIABLE's slot index, so taking a` |
|         - |  5730 | `` 		 * reference to it aliased the WHOLE STRING — `$x = [&$s[1]]; $x[0] = "Z";` `` |
|         - |  5731 | ``		 * left $s === "Z". Same Error the `=&` and by-ref-argument paths raise. */`` |
|         3 |  5732 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|         - |  5733 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|         3 |  5734 | `		PH7_MemObjRelease(pTos);` |
|         3 |  5735 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|         3 |  5736 | `		pTos->nIdx = SXU32_HIGH;` |
|         3 |  5737 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|         3 |  5738 | `		break;` |
|         - |  5739 | `	}` |
|         - |  5740 | `	/* Extract memory object index */` |
|       204 |  5741 | `	nIdx = pTos->nIdx;` |
|       204 |  5742 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|         - |  5743 | `		/* Nullify the object */` |
|       204 |  5744 | `		PH7_MemObjRelease(pTos);` |
|         - |  5745 | `		/* Mark as constant and store the index on the top of the stack */` |
|       204 |  5746 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       204 |  5747 | `		pTos->nIdx = SXU32_HIGH;` |
|       204 |  5748 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       101 |  5749 | `	}` |
|       204 |  5750 | `	break;` |
|         - |  5751 | `					  }` |
|         - |  5752 | `/*` |
|         - |  5753 | ` * OP_STORE_REF * * P3` |
|         - |  5754 | ` * Perform an assignment operation by reference.` |
|         - |  5755 | ` */` |
|      2535 |  5756 | `case PH7_OP_STORE_REF: {` |
|         - |  5757 | `	VmOpRc rcOp;` |
|      5075 |  5758 | `	sState.pTos = pTos;` |
|      5075 |  5759 | `	sState.pc = pc;` |
|      5075 |  5760 | `	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);` |
|      5075 |  5761 | `	pTos = sState.pTos;` |
|      5075 |  5762 | `	pc = sState.pc;` |
|      5075 |  5763 | `	if( rcOp == VM_OP_ABORT ){` |
|         3 |  5764 | `		goto Abort;` |
|      5073 |  5765 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        22 |  5766 | `		goto Exception;` |
|         - |  5767 | `	}` |
|      5053 |  5768 | `	break;` |
|         - |  5769 | `					  }` |
|         - |  5770 | `/*` |
|         - |  5771 | ` * OP_UPLINK P1 * *` |
|         - |  5772 | ` * Link a variable to the top active VM frame.` |
|         - |  5773 | ` * This is used to implement the 'global' PHP construct.` |
|         - |  5774 | ` */` |
|       890 |  5775 | `case PH7_OP_UPLINK: {` |
|      1781 |  5776 | `	if( pVm->pFrame->pParent ){` |
|      1781 |  5777 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|         - |  5778 | `		SyString sName;` |
|         - |  5779 | `		/* rcSv carries the coercion status OUT of the loop: the routing macro is a` |
|         - |  5780 | ``		 * bare `break` here, which would only leave the while and then pop the`` |
|         - |  5781 | `		 * operands with a throw pending. */` |
|      1781 |  5782 | `		sxi32 rcSv = SXRET_OK;` |
|         - |  5783 | `		/* Perform the link */` |
|      4421 |  5784 | `		while( pLink <= pTos ){` |
|         - |  5785 | `			/* Force a string cast — global $$arr link name (user-visible) */` |
|      2651 |  5786 | `			rcSv = PH7_MemObjToStringUV(pLink);` |
|      2651 |  5787 | `			if( rcSv != SXRET_OK ){` |
|         7 |  5788 | `				break;` |
|         - |  5789 | `			}` |
|      2645 |  5790 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|      2645 |  5791 | `			if( sName.nByte > 0 ){` |
|      2645 |  5792 | `				VmFrameLink(&(*pVm),&sName);` |
|      1318 |  5793 | `			}` |
|      2645 |  5794 | `			pLink++;` |
|         5 |  5795 | `		}` |
|      1781 |  5796 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|       883 |  5797 | `	}` |
|      1775 |  5798 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|      1775 |  5799 | `	break;` |
|         - |  5800 | `					}` |
|         - |  5801 | `/*` |
|         - |  5802 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|         - |  5803 | ` * Push an exception in the corresponding container so that` |
|         - |  5804 | ` * it can be thrown later by the OP_THROW instruction.` |
|         - |  5805 | ` */` |
|    744284 |  5806 | `case PH7_OP_LOAD_EXCEPTION: {` |
|         - |  5807 | `	VmOpRc rcOp;` |
|   1488355 |  5808 | `	sState.pTos = pTos;` |
|   1488355 |  5809 | `	sState.pc = pc;` |
|   1488355 |  5810 | `	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);` |
|   1488355 |  5811 | `	pTos = sState.pTos;` |
|   1488355 |  5812 | `	pc = sState.pc;` |
|   1488355 |  5813 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  5814 | `		goto Abort;` |
|   1488355 |  5815 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       ! 0 |  5816 | `		goto Exception;` |
|         - |  5817 | `	}` |
|   1488355 |  5818 | `	break;` |
|         - |  5819 | `					  }` |
|         - |  5820 | `/*` |
|         - |  5821 | ` * OP_POP_EXCEPTION * * P3` |
|         - |  5822 | ` * Pop a previously pushed exception from the corresponding container.` |
|         - |  5823 | ` */` |
|    690929 |  5824 | `case PH7_OP_POP_EXCEPTION: {` |
|   1381645 |  5825 | `	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;` |
|         - |  5826 | `	VmFrame *pBodyFrame;` |
|         - |  5827 | `	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on` |
|         - |  5828 | `	 * top (matched by compiled origin). pException == NULL means this try's` |
|         - |  5829 | `	 * activation was already consumed (an in-place catch handled a throw and` |
|         - |  5830 | `	 * ran the finally itself); compiled fields keep coming from p3. */` |
|   1381645 |  5831 | `	ph7_exception *pException = 0;` |
|   1381645 |  5832 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|     13212 |  5833 | `		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|     13212 |  5834 | `		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];` |
|         - |  5835 | `		/* Same compiled origin is NOT enough: under recursion, when THIS` |
|         - |  5836 | `		 * level's activation was already consumed by an in-place catch (the` |
|         - |  5837 | `		 * resume lands right here), the top can be an OUTER level's activation` |
|         - |  5838 | `		 * of the same lexical try — popping it would run that level's finally` |
|         - |  5839 | `		 * early and orphan its handler (probed: recursive try/catch/finally` |
|         - |  5840 | `		 * lost the outer catch entirely). The activation must also belong to` |
|         - |  5841 | `		 * the CURRENT body frame. */` |
|     13207 |  5842 | `		if( VmExcMatches(pTop,pCompiledExc)` |
|     13086 |  5843 | `		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){` |
|     12954 |  5844 | `			pException = pTop;` |
|     12954 |  5845 | `			(void)SySetPop(&pVm->aException);` |
|      6393 |  5846 | `		}` |
|      6522 |  5847 | `	}` |
|   1381645 |  5848 | `	if( pCompiledExc->iInlined ){` |
|         - |  5849 | `		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.` |
|         - |  5850 | `		 * Pop this try's handler off aException so a throw in the finally propagates to` |
|         - |  5851 | `		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the` |
|         - |  5852 | `		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the` |
|         - |  5853 | `		 * compiler-emitted JMP falls into the finally. Without a finally, this is the` |
|         - |  5854 | `		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */` |
|       175 |  5855 | `		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */` |
|       175 |  5856 | `		if( pCompiledExc->iHasFinally ){` |
|         - |  5857 | `			VmFinallyAction sAct;` |
|        15 |  5858 | `			SyZero(&sAct,sizeof(sAct));` |
|        15 |  5859 | `			sAct.eKind = PH7_FA_FALLTHROUGH;` |
|        15 |  5860 | `			sAct.iNextPc = pCompiledExc->iEndCatchPc;` |
|        15 |  5861 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|         - |  5862 | `			/* keep the transparent frame for OP_END_FINALLY */` |
|       169 |  5863 | `		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        23 |  5864 | `			VmLeaveFrame(&(*pVm));` |
|         9 |  5865 | `		}` |
|       175 |  5866 | `		break;` |
|         - |  5867 | `	}` |
|         - |  5868 | `	/* Leave the exception frame. It is normally on top here (a try that fell through` |
|         - |  5869 | `	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a` |
|         - |  5870 | `	 * RESUMED generator/fiber body whose yield was inside this try, that exception` |
|         - |  5871 | `	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),` |
|         - |  5872 | `	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the` |
|         - |  5873 | `	 * coroutine (and defeat the bHasRet materialization just below, which must see the` |
|         - |  5874 | `	 * body). Only leave a genuine exception frame. */` |
|         - |  5875 | `	/* ...and never THIS exec's own entry frame. A catch body runs as a` |
|         - |  5876 | `	 * mini-program entered on the transparent catch frame, which carries` |
|         - |  5877 | `	 * VM_FRAME_EXCEPTION too; a try nested inside that catch body whose own` |
|         - |  5878 | `	 * exception frame is already gone then popped the mini-program's entry` |
|         - |  5879 | `	 * frame instead, and this body's terminal OP_DONE read it back (bHasRet)` |
|         - |  5880 | `	 * after it had been freed. An exec never owns the teardown of the frame it` |
|         - |  5881 | `	 * was entered on -- whoever entered it does. */` |
|   1381475 |  5882 | `	if( (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) && pVm->pFrame != sState.pEntryFrame ){` |
|   1069426 |  5883 | `		VmLeaveFrame(&(*pVm));` |
|    534627 |  5884 | `	}` |
|         - |  5885 | `	/* Execute the finally block if present and not already executed by the` |
|         - |  5886 | `	 * catch path. No live activation (pException == NULL) means an in-place` |
|         - |  5887 | `	 * catch consumed it — and that path runs the finally itself — so skip. */` |
|   1381475 |  5888 | `	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){` |
|         - |  5889 | `		sxi32 rcFinally;` |
|        69 |  5890 | `		VmExcRelease(&(*pVm),pException);` |
|        69 |  5891 | `		pException = 0;` |
|        69 |  5892 | `		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);` |
|        69 |  5893 | `		if( rcFinally == SXERR_ABORT ){` |
|       ! 0 |  5894 | `			goto Abort;` |
|         - |  5895 | `		}` |
|        69 |  5896 | `		if( rcFinally == PH7_EXCEPTION ){` |
|         - |  5897 | `			/* The finally threw past itself. If an enclosing try IN THIS function` |
|         - |  5898 | `			 * caught the new exception in place, resume at its landing pad;` |
|         - |  5899 | `			 * otherwise it was caught at an outer frame, so unwind this function. */` |
|         - |  5900 | `			sxi32 iResumePc;` |
|         3 |  5901 | `			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|         3 |  5902 | `				pc = iResumePc;` |
|         3 |  5903 | `				break;` |
|         - |  5904 | `			}` |
|       ! 0 |  5905 | `			goto Exception;` |
|         - |  5906 | `		}` |
|        31 |  5907 | `	}` |
|   1381473 |  5908 | `	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */` |
|   1381473 |  5909 | `	pBodyFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   1381473 |  5910 | `	if( pBodyFrame->bHasRet ){` |
|         - |  5911 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|         - |  5912 | `		 * function. The return targets the body frame this try belongs to. Drain` |
|         - |  5913 | `		 * outer finally blocks first, then — only in the real function body` |
|         - |  5914 | `		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline` |
|         - |  5915 | `		 * try within a catch/finally) propagate outward so the owning body returns. */` |
|     24299 |  5916 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|     24299 |  5917 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5918 | `			goto Abort;` |
|         - |  5919 | `		}` |
|     24299 |  5920 | `		if( rc == PH7_EXCEPTION ){` |
|       ! 0 |  5921 | `			goto Exception;` |
|         - |  5922 | `		}` |
|     24299 |  5923 | `		if( !sState.bReturnPropagates ){` |
|     24293 |  5924 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|     12144 |  5925 | `		}` |
|     24299 |  5926 | `		goto Done;` |
|         - |  5927 | `	}` |
|   1357179 |  5928 | `	if( pBodyFrame->nCatchJmpPc > 0 ){` |
|         - |  5929 | ``		/* A `break`/`continue` inside the catch (OP_CATCH_JMP) parked its loop-exit`` |
|         - |  5930 | `		 * target on this body frame, and this is a landing pad on its way out. */` |
|        88 |  5931 | `		if( pBodyFrame->nCatchJmpLevels > 1 ){` |
|         - |  5932 | `			/* Still one or more detached bodies out from the target's array — this try` |
|         - |  5933 | `			 * was declared INSIDE another catch/finally mini-program. End this one and` |
|         - |  5934 | `			 * let the park travel outward; the next landing pad decrements again. */` |
|         6 |  5935 | `			pBodyFrame->nCatchJmpLevels--;` |
|         6 |  5936 | `			goto Done;` |
|         - |  5937 | `		}` |
|        84 |  5938 | `		if( pBodyFrame->nCatchJmpCross > 0 ){` |
|         - |  5939 | `			/* Enclosing trys between the catch and the loop: the jump lands past their` |
|         - |  5940 | `			 * OP_POP_EXCEPTION, so run their finally (and leave their frames) here. */` |
|         8 |  5941 | `			sxu32 nCross = pBodyFrame->nCatchJmpCross;` |
|         8 |  5942 | `			pBodyFrame->nCatchJmpCross = 0;` |
|         8 |  5943 | `			rc = VmDrainCrossedTrys(&(*pVm),nCross,sState.nExceptionBase);` |
|         8 |  5944 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5945 | `				goto Abort;` |
|         - |  5946 | `			}` |
|         8 |  5947 | `			if( rc == PH7_EXCEPTION ){` |
|         - |  5948 | `				/* A drained finally threw past itself — it supersedes the loop exit. */` |
|       ! 0 |  5949 | `				pBodyFrame->nCatchJmpPc = 0;` |
|       ! 0 |  5950 | `				goto Exception;` |
|         - |  5951 | `			}` |
|         3 |  5952 | `		}` |
|        84 |  5953 | `		pc = (sxi32)pBodyFrame->nCatchJmpPc - 1;` |
|        84 |  5954 | `		pBodyFrame->nCatchJmpPc = 0;` |
|        84 |  5955 | `		break;` |
|         - |  5956 | `	}` |
|   1357095 |  5957 | `	break;` |
|         - |  5958 | `							}` |
|         - |  5959 | `/*` |
|         - |  5960 | ` * OP_CATCH_JMP P1(levels,cross) P2(target pc) *` |
|         - |  5961 | ` * Jump to iP2, leaving the try/catch structures iP1 describes (PH7_CATCH_JMP_P1).` |
|         - |  5962 | ` *` |
|         - |  5963 | ` * CROSS enclosing trys are being left without reaching their OP_POP_EXCEPTION, so this` |
|         - |  5964 | ` * runs their finallys itself. LEVELS says whether the target is even addressable from` |
|         - |  5965 | `` * here: 0 means it is in this same array (a `goto` out of a try body) and the jump is`` |
|         - |  5966 | ` * taken on the spot; above 0 the jump starts inside a DETACHED catch/finally` |
|         - |  5967 | ` * mini-program whose array is not the target's, so the target is PARKED on the owning` |
|         - |  5968 | `` * body's frame and this mini-program ends — mirroring how an explicit `return` in a`` |
|         - |  5969 | ` * catch parks on sRet at OP_DONE above. VmThrowException then runs this try's finally` |
|         - |  5970 | ` * and returns; the resume lands on the try's OP_POP_EXCEPTION, which decrements LEVELS` |
|         - |  5971 | ` * and either re-parks (another mini-program out) or takes the jump.` |
|         - |  5972 | ` */` |
|        47 |  5973 | `case PH7_OP_CATCH_JMP: {` |
|         - |  5974 | `	VmFrame *pTgt;` |
|        98 |  5975 | `	if( PH7_CATCH_JMP_LEVELS(pInstr->iP1) == 0 ){` |
|         - |  5976 | ``		/* No detached body to leave — a `goto` whose target is in THIS array, but which`` |
|         - |  5977 | `		 * jumps out of one or more enclosing trys. Nothing to park: run their finallys` |
|         - |  5978 | `		 * (a bare jump would leave them to fire at teardown) and go. */` |
|        15 |  5979 | `		rc = VmDrainCrossedTrys(&(*pVm),PH7_CATCH_JMP_CROSS(pInstr->iP1),sState.nExceptionBase);` |
|        15 |  5980 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5981 | `			goto Abort;` |
|         - |  5982 | `		}` |
|        15 |  5983 | `		if( rc == PH7_EXCEPTION ){` |
|         - |  5984 | `			/* A drained finally threw past itself — it supersedes the jump. */` |
|       ! 0 |  5985 | `			goto Exception;` |
|         - |  5986 | `		}` |
|        15 |  5987 | `		pc = (sxi32)pInstr->iP2 - 1;` |
|        15 |  5988 | `		break;` |
|         - |  5989 | `	}` |
|        86 |  5990 | `	pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|        86 |  5991 | `	pTgt->nCatchJmpPc = (sxu32)pInstr->iP2;` |
|        86 |  5992 | `	pTgt->nCatchJmpLevels = PH7_CATCH_JMP_LEVELS(pInstr->iP1);` |
|        86 |  5993 | `	pTgt->nCatchJmpCross = PH7_CATCH_JMP_CROSS(pInstr->iP1);` |
|         - |  5994 | `	/* A try opened INSIDE this catch body that the jump crosses had its finally run by` |
|         - |  5995 | `	 * the compiler-emitted OP_POP_EXCEPTION; drain anything still pending here. */` |
|        86 |  5996 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|        86 |  5997 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  5998 | `		goto Abort;` |
|         - |  5999 | `	}` |
|        86 |  6000 | `	if( rc == PH7_EXCEPTION ){` |
|         - |  6001 | `		/* A drained finally threw past itself — it discards this jump. */` |
|       ! 0 |  6002 | `		pTgt->nCatchJmpPc = 0;` |
|       ! 0 |  6003 | `		goto Exception;` |
|         - |  6004 | `	}` |
|        86 |  6005 | `	goto Done;` |
|         - |  6006 | `					   }` |
|         - |  6007 | `/*` |
|         - |  6008 | ` * OP_CATCH iP1(catch-index) * P3(ph7_exception)` |
|         - |  6009 | ` * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on` |
|         - |  6010 | ` * pException->pInflight by VmThrowInline) into the catch variable, resolved in the` |
|         - |  6011 | ` * enclosing body's scope (PHP: a catch shares the surrounding variable scope).` |
|         - |  6012 | ` */` |
|        44 |  6013 | `case PH7_OP_CATCH: {` |
|         - |  6014 | `	VmOpRc rcOp;` |
|        93 |  6015 | `	sState.pTos = pTos;` |
|        93 |  6016 | `	sState.pc = pc;` |
|        93 |  6017 | `	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);` |
|        93 |  6018 | `	pTos = sState.pTos;` |
|        93 |  6019 | `	pc = sState.pc;` |
|        93 |  6020 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  6021 | `		goto Abort;` |
|        93 |  6022 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       ! 0 |  6023 | `		goto Exception;` |
|         - |  6024 | `	}` |
|        93 |  6025 | `	break;` |
|         - |  6026 | `					  }` |
|         - |  6027 | `/*` |
|         - |  6028 | ` * OP_END_FINALLY * P3(ph7_exception)` |
|         - |  6029 | ` * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch` |
|         - |  6030 | ` * the pending action queued when the finally was entered (fall-through / re-throw /` |
|         - |  6031 | ` * return / break-continue). A return/break threads out through each enclosing finally` |
|         - |  6032 | ` * via pException->iNextFinallyPc.` |
|         - |  6033 | ` */` |
|        27 |  6034 | `case PH7_OP_END_FINALLY: {` |
|        59 |  6035 | `	ph7_exception *pExc = (ph7_exception *)pInstr->p3;` |
|         - |  6036 | `	VmFinallyAction sAct;` |
|        59 |  6037 | `	int eKind = PH7_FA_FALLTHROUGH;` |
|         - |  6038 | `	/* Leave the try's transparent frame kept alive across the finally. */` |
|        59 |  6039 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|         8 |  6040 | `		VmLeaveFrame(&(*pVm));` |
|         3 |  6041 | `	}` |
|        59 |  6042 | `	if( SySetUsed(&pVm->aFinallyAction) > 0 ){` |
|        57 |  6043 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);` |
|        57 |  6044 | `		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];` |
|        57 |  6045 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|        57 |  6046 | `		eKind = sAct.eKind;` |
|        31 |  6047 | `	}else{` |
|         3 |  6048 | `		SyZero(&sAct,sizeof(sAct));` |
|         3 |  6049 | `		sAct.iNextPc = pExc->iEndCatchPc;` |
|         - |  6050 | `	}` |
|        59 |  6051 | `	if( eKind == PH7_FA_FALLTHROUGH ){` |
|        15 |  6052 | `		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;` |
|        19 |  6053 | `		break;` |
|        47 |  6054 | `	}else if( eKind == PH7_FA_JMP ){` |
|         - |  6055 | `		/* break/continue: run the remaining crossed finallys, then take the jump. */` |
|         6 |  6056 | `		sxu32 iFpc = 0;` |
|         6 |  6057 | `		int nCross = sAct.nCross;` |
|         6 |  6058 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|       ! 0 |  6059 | `			sAct.nCross = nCross;` |
|       ! 0 |  6060 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       ! 0 |  6061 | `			pc = (sxi32)iFpc - 1;` |
|       ! 0 |  6062 | `			break;` |
|         - |  6063 | `		}` |
|         6 |  6064 | `		pc = (sxi32)sAct.iNextPc - 1;` |
|         6 |  6065 | `		break;` |
|        43 |  6066 | `	}else if( eKind == PH7_FA_RETHROW ){` |
|        12 |  6067 | `		ph7_class_instance *pRe = sAct.pExc;` |
|         - |  6068 | `		sxi32 _iRpE;` |
|        12 |  6069 | `		rc = VmThrowException(&(*pVm),pRe);` |
|        12 |  6070 | `		if( pRe ){ PH7_ClassInstanceUnref(pRe); }` |
|        12 |  6071 | `		if( rc == SXERR_ABORT ){ goto Abort; }` |
|        12 |  6072 | `		PH7_INLINE_RESUME_BREAK()` |
|        12 |  6073 | `		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }` |
|        12 |  6074 | `		goto Exception;` |
|       ! 0 |  6075 | `	}else{ /* PH7_FA_RETURN */` |
|        33 |  6076 | `		sxu32 iFpc = 0;` |
|        33 |  6077 | `		int nCross = sAct.nCross;` |
|        33 |  6078 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|         - |  6079 | `			/* Thread the return through the next enclosing finally. */` |
|         5 |  6080 | `			sAct.nCross = nCross;` |
|         5 |  6081 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|         5 |  6082 | `			pc = (sxi32)iFpc - 1;` |
|         5 |  6083 | `			break;` |
|         - |  6084 | `		}` |
|         - |  6085 | `		/* No enclosing finally left: materialize the return from this body. */` |
|        29 |  6086 | `		if( sAct.bHasRetVal && sState.pResult ){` |
|         6 |  6087 | `			PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|         2 |  6088 | `		}` |
|        29 |  6089 | `		PH7_MemObjRelease(&sAct.sRet);` |
|        29 |  6090 | `		goto Done;` |
|         - |  6091 | `	}` |
|         - |  6092 | `						 }` |
|         - |  6093 | `/*` |
|         - |  6094 | ` * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)` |
|         - |  6095 | `` * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a`` |
|         - |  6096 | ` * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);` |
|         - |  6097 | ` * OP_END_FINALLY threads it out through the finally chain and then returns.` |
|         - |  6098 | ` */` |
|        30 |  6099 | `case PH7_OP_SET_FINALLY_RET: {` |
|         - |  6100 | `	VmFinallyAction sAct;` |
|        64 |  6101 | `	sxu32 iFpc = 0;` |
|        64 |  6102 | `	int nCross = -1; /* a return crosses every enclosing finally in this function */` |
|        64 |  6103 | `	SyZero(&sAct,sizeof(sAct));` |
|        64 |  6104 | `	sAct.eKind = PH7_FA_RETURN;` |
|        64 |  6105 | `	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        64 |  6106 | `	PH7_MemObjInit(pVm,&sAct.sRet);` |
|        64 |  6107 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|        52 |  6108 | `		PH7_MemObjStore(pTos,&sAct.sRet);` |
|        52 |  6109 | `		sAct.bHasRetVal = 1;` |
|        52 |  6110 | `		VmPopOperand(&pTos,1);` |
|        24 |  6111 | `	}` |
|        64 |  6112 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|         8 |  6113 | `		sAct.nCross = nCross;` |
|         8 |  6114 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|         8 |  6115 | `		pc = (sxi32)iFpc - 1;` |
|         8 |  6116 | `		break;` |
|         - |  6117 | `	}` |
|         - |  6118 | `	/* No enclosing finally left: return now. */` |
|        58 |  6119 | `	if( sAct.bHasRetVal && sState.pResult ){` |
|        48 |  6120 | `		PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|        22 |  6121 | `	}` |
|        58 |  6122 | `	PH7_MemObjRelease(&sAct.sRet);` |
|        58 |  6123 | `	goto Done;` |
|         - |  6124 | `						 }` |
|         - |  6125 | `/*` |
|         - |  6126 | ` * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)` |
|         - |  6127 | `` * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action`` |
|         - |  6128 | ` * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.` |
|         - |  6129 | ` */` |
|         4 |  6130 | `case PH7_OP_SET_FINALLY_JMP: {` |
|         - |  6131 | `	VmFinallyAction sAct;` |
|        11 |  6132 | `	sxu32 iFpc = 0;` |
|        11 |  6133 | `	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */` |
|        11 |  6134 | `	SyZero(&sAct,sizeof(sAct));` |
|        11 |  6135 | `	sAct.eKind = PH7_FA_JMP;` |
|        11 |  6136 | `	sAct.iNextPc = pInstr->iP2;` |
|        11 |  6137 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|         6 |  6138 | `		sAct.nCross = nCross;` |
|         6 |  6139 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|         6 |  6140 | `		pc = (sxi32)iFpc - 1;` |
|         6 |  6141 | `		break;` |
|         - |  6142 | `	}` |
|         - |  6143 | `	/* No finally among the crossed trys: just take the break/continue jump. */` |
|         6 |  6144 | `	pc = (sxi32)sAct.iNextPc - 1;` |
|         6 |  6145 | `	break;` |
|         - |  6146 | `						 }` |
|         - |  6147 | `/*` |
|         - |  6148 | ` * OP_THROW * P2 *` |
|         - |  6149 | ` * Throw an user exception.` |
|         - |  6150 | ` */` |
|    500653 |  6151 | `case PH7_OP_THROW: {` |
|         - |  6152 | `	VmOpRc rcOp;` |
|   1001308 |  6153 | `	sState.pTos = pTos;` |
|   1001308 |  6154 | `	sState.pc = pc;` |
|   1001308 |  6155 | `	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);` |
|   1001308 |  6156 | `	pTos = sState.pTos;` |
|   1001308 |  6157 | `	pc = sState.pc;` |
|   1001308 |  6158 | `	if( rcOp == VM_OP_ABORT ){` |
|        51 |  6159 | `		goto Abort;` |
|   1001262 |  6160 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|    600756 |  6161 | `		goto Exception;` |
|         - |  6162 | `	}` |
|    400511 |  6163 | `	break;` |
|         - |  6164 | `					  }` |
|         - |  6165 | `/*` |
|         - |  6166 | ` * OP_FOREACH_INIT * P2 P3` |
|         - |  6167 | ` * Prepare a foreach step.` |
|         - |  6168 | ` */` |
|     24179 |  6169 | `case PH7_OP_FOREACH_INIT: {` |
|         - |  6170 | `	VmOpRc rcOp;` |
|     48294 |  6171 | `	sState.pTos = pTos;` |
|     48294 |  6172 | `	sState.pc = pc;` |
|     48294 |  6173 | `	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);` |
|     48294 |  6174 | `	pTos = sState.pTos;` |
|     48294 |  6175 | `	pc = sState.pc;` |
|     48294 |  6176 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  6177 | `		goto Abort;` |
|     48294 |  6178 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        45 |  6179 | `		goto Exception;` |
|         - |  6180 | `	}` |
|     48252 |  6181 | `	break;` |
|         - |  6182 | `					  }` |
|         - |  6183 | `/*` |
|         - |  6184 | ` * OP_FOREACH_STEP * P2 P3` |
|         - |  6185 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|         - |  6186 | ` */` |
|   2582828 |  6187 | `case PH7_OP_FOREACH_STEP: {` |
|         - |  6188 | `	VmOpRc rcOp;` |
|   5164833 |  6189 | `	sState.pTos = pTos;` |
|   5164833 |  6190 | `	sState.pc = pc;` |
|   5164833 |  6191 | `	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);` |
|   5164833 |  6192 | `	pTos = sState.pTos;` |
|   5164833 |  6193 | `	pc = sState.pc;` |
|   5164833 |  6194 | `	if( rcOp == VM_OP_ABORT ){` |
|         3 |  6195 | `		goto Abort;` |
|   5164831 |  6196 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|         6 |  6197 | `		goto Exception;` |
|         - |  6198 | `	}` |
|   5164827 |  6199 | `	break;` |
|         - |  6200 | `						  }` |
|         - |  6201 | `/*` |
|         - |  6202 | ` * OP_MEMBER P1 P2` |
|         - |  6203 | ` * Load class attribute/method on the stack.` |
|         - |  6204 | ` */` |
|    231300 |  6205 | `case PH7_OP_MEMBER: {` |
|         - |  6206 | `	VmOpRc rcOp;` |
|    462501 |  6207 | `	sState.pTos = pTos;` |
|    462501 |  6208 | `	sState.pc = pc;` |
|    462501 |  6209 | `	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);` |
|    462501 |  6210 | `	pTos = sState.pTos;` |
|    462501 |  6211 | `	pc = sState.pc;` |
|    462501 |  6212 | `	if( rcOp == VM_OP_ABORT ){` |
|         3 |  6213 | `		goto Abort;` |
|    462499 |  6214 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       376 |  6215 | `		goto Exception;` |
|         - |  6216 | `	}` |
|    462127 |  6217 | `	break;` |
|         - |  6218 | `					  }` |
|         - |  6219 | `/*` |
|         - |  6220 | ` * OP_NEW P1 * * *` |
|         - |  6221 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|         - |  6222 | ` */` |
|   1072020 |  6223 | `case PH7_OP_NEW: {` |
|         - |  6224 | `	VmOpRc rcOp;` |
|   2144003 |  6225 | `	sState.pTos = pTos;` |
|   2144003 |  6226 | `	sState.pc = pc;` |
|   2144003 |  6227 | `	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);` |
|   2144003 |  6228 | `	pTos = sState.pTos;` |
|   2144003 |  6229 | `	pc = sState.pc;` |
|   2144003 |  6230 | `	if( rcOp == VM_OP_ABORT ){` |
|        17 |  6231 | `		goto Abort;` |
|   2143989 |  6232 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       404 |  6233 | `		goto Exception;` |
|         - |  6234 | `	}` |
|   2143590 |  6235 | `	break;` |
|         - |  6236 | `					  }` |
|         - |  6237 | `/*` |
|         - |  6238 | ` * OP_CLONE * * *` |
|         - |  6239 | ` * Perfome a clone operation.` |
|         - |  6240 | ` */` |
|       309 |  6241 | `case PH7_OP_CLONE: {` |
|         - |  6242 | `	VmOpRc rcOp;` |
|       623 |  6243 | `	sState.pTos = pTos;` |
|       623 |  6244 | `	sState.pc = pc;` |
|       623 |  6245 | `	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);` |
|       623 |  6246 | `	pTos = sState.pTos;` |
|       623 |  6247 | `	pc = sState.pc;` |
|       623 |  6248 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  6249 | `		goto Abort;` |
|       623 |  6250 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        50 |  6251 | `		goto Exception;` |
|         - |  6252 | `	}` |
|       575 |  6253 | `	break;` |
|         - |  6254 | `					  }` |
|         - |  6255 | `/*` |
|         - |  6256 | ` * OP_SWITCH * * P3` |
|         - |  6257 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|         - |  6258 | ` */` |
|       150 |  6259 | `case PH7_OP_SWITCH: {` |
|         - |  6260 | `	VmOpRc rcOp;` |
|       305 |  6261 | `	sState.pTos = pTos;` |
|       305 |  6262 | `	sState.pc = pc;` |
|       305 |  6263 | `	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);` |
|       305 |  6264 | `	pTos = sState.pTos;` |
|       305 |  6265 | `	pc = sState.pc;` |
|       305 |  6266 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  6267 | `		goto Abort;` |
|       305 |  6268 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|         5 |  6269 | `		goto Exception;` |
|         - |  6270 | `	}` |
|       301 |  6271 | `	break;` |
|         - |  6272 | `					  }` |
|         - |  6273 | `/*` |
|         - |  6274 | ` * OP_MATCH * * P3` |
|         - |  6275 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|         - |  6276 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|         - |  6277 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|         - |  6278 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|         - |  6279 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|         - |  6280 | ` */` |
|       136 |  6281 | `case PH7_OP_MATCH: {` |
|         - |  6282 | `	VmOpRc rcOp;` |
|       277 |  6283 | `	sState.pTos = pTos;` |
|       277 |  6284 | `	sState.pc = pc;` |
|       277 |  6285 | `	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);` |
|       277 |  6286 | `	pTos = sState.pTos;` |
|       277 |  6287 | `	pc = sState.pc;` |
|       277 |  6288 | `	if( rcOp == VM_OP_ABORT ){` |
|       ! 0 |  6289 | `		goto Abort;` |
|       277 |  6290 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|         6 |  6291 | `		goto Exception;` |
|         - |  6292 | `	}` |
|       272 |  6293 | `	break;` |
|         - |  6294 | `					  }` |
|         - |  6295 | `/*` |
|         - |  6296 | ` * OP_YIELD P1 P2 *` |
|         - |  6297 | ` *  Yield a value from a generator function.` |
|         - |  6298 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|         - |  6299 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|         - |  6300 | ` */` |
|       808 |  6301 | `case PH7_OP_YIELD: {` |
|         - |  6302 | `	ph7_generator *pGen;` |
|      1621 |  6303 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|       ! 0 |  6304 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|       ! 0 |  6305 | `		goto Abort;` |
|         - |  6306 | `	}` |
|      1621 |  6307 | `	if( pVm->pActiveCtx->bClosing ){` |
|         - |  6308 | ``		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's`` |
|         - |  6309 | `		 * pending finallys tried to yield — PHP forbids it. */` |
|       ! 0 |  6310 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|       ! 0 |  6311 | `			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){` |
|       ! 0 |  6312 | `			goto Abort;` |
|         - |  6313 | `		}` |
|       ! 0 |  6314 | `		goto Exception;` |
|         - |  6315 | `	}` |
|      1621 |  6316 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|      1621 |  6317 | `	if( pInstr->iP2 ){` |
|         - |  6318 | `		/* yield $key => $value: value on top, key below */` |
|         - |  6319 | `#ifdef UNTRUST` |
|         - |  6320 | `		if( pTos < &pStack[1] ) goto Abort;` |
|         - |  6321 | `#endif` |
|       171 |  6322 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       171 |  6323 | `		VmPopOperand(&pTos, 1);` |
|       171 |  6324 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       171 |  6325 | `		VmPopOperand(&pTos, 1);` |
|         - |  6326 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       171 |  6327 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|        31 |  6328 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|        31 |  6329 | `			if( nKey >= pGen->iImplicitKey ){` |
|        31 |  6330 | `				pGen->iImplicitKey = nKey + 1;` |
|        14 |  6331 | `			}` |
|        19 |  6332 | `		}` |
|      1538 |  6333 | `	}else if( pInstr->iP1 ){` |
|         - |  6334 | `		/* yield $value */` |
|         - |  6335 | `#ifdef UNTRUST` |
|         - |  6336 | `		if( pTos < pStack ) goto Abort;` |
|         - |  6337 | `#endif` |
|      1453 |  6338 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|      1453 |  6339 | `		VmPopOperand(&pTos, 1);` |
|         - |  6340 | `		/* Auto-increment key */` |
|      1453 |  6341 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      1453 |  6342 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      1453 |  6343 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       729 |  6344 | `	}else{` |
|         - |  6345 | `		/* Bare yield — null value, auto-increment key */` |
|         3 |  6346 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|         3 |  6347 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|         3 |  6348 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|         3 |  6349 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|         - |  6350 | `	}` |
|         - |  6351 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|      1621 |  6352 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|      1621 |  6353 | `	goto Suspend;` |
|         - |  6354 | `}` |
|         - |  6355 | `/*` |
|         - |  6356 | ` * OP_YIELD_FROM * * *` |
|         - |  6357 | ` *` |
|         - |  6358 | `` * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an`` |
|         - |  6359 | ` * array/Traversable/Generator from the OUTER generator, preserving the inner` |
|         - |  6360 | ` * keys; the expression evaluates to the inner Generator's return value (or NULL).` |
|         - |  6361 | ` *` |
|         - |  6362 | ` * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,` |
|         - |  6363 | ` * advances the per-instance delegate cursor stored on the exec context (never the` |
|         - |  6364 | ` * shared foreach aStep, so independent generator instances cannot clash). The` |
|         - |  6365 | ` * iterable operand is consumed on first entry; the expression result is pushed at` |
|         - |  6366 | ` * exhaustion — net stack effect +1, identical to OP_YIELD.` |
|         - |  6367 | ` */` |
|       115 |  6368 | `case PH7_OP_YIELD_FROM: {` |
|         - |  6369 | `	ph7_generator *pGenFrom;` |
|         - |  6370 | `	ph7_exec_ctx *pCtxFrom;` |
|         - |  6371 | `	ph7_value sKey,sVal;` |
|       235 |  6372 | `	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */` |
|       235 |  6373 | `	int bExhausted = 0;` |
|       235 |  6374 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|       ! 0 |  6375 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");` |
|       ! 0 |  6376 | `		goto Abort;` |
|         - |  6377 | `	}` |
|       235 |  6378 | `	if( pVm->pActiveCtx->bClosing ){` |
|         - |  6379 | ``		/* A `yield from` reached while VmCloseCtx force-drives this destroyed`` |
|         - |  6380 | `		 * generator's pending finallys — PHP forbids it (distinct message from a` |
|         - |  6381 | ``		 * bare `yield`, mirroring the OP_YIELD guard above). */`` |
|       ! 0 |  6382 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|       ! 0 |  6383 | `			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){` |
|       ! 0 |  6384 | `			goto Abort;` |
|         - |  6385 | `		}` |
|       ! 0 |  6386 | `		goto Exception;` |
|         - |  6387 | `	}` |
|       235 |  6388 | `	pCtxFrom = pVm->pActiveCtx;` |
|       235 |  6389 | `	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;` |
|       235 |  6390 | `	PH7_MemObjInit(pVm,&sKey);` |
|       235 |  6391 | `	PH7_MemObjInit(pVm,&sVal);` |
|       235 |  6392 | `	if( pCtxFrom->iDelegateState == 0 ){` |
|         - |  6393 | `		/* First entry: classify the iterable on the stack top. */` |
|       101 |  6394 | `		int bIterable = 1;` |
|         - |  6395 | `#ifdef UNTRUST` |
|         - |  6396 | `		if( pTos < pStack ){ goto Abort; }` |
|         - |  6397 | `#endif` |
|       101 |  6398 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        35 |  6399 | `			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        35 |  6400 | `			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;` |
|        35 |  6401 | `			pCtxFrom->iDelegateState = 1;` |
|        86 |  6402 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        67 |  6403 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        67 |  6404 | `			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|        67 |  6405 | `			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){` |
|        57 |  6406 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        57 |  6407 | `				pCtxFrom->iDelegateState = 3;` |
|        39 |  6408 | `			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){` |
|         9 |  6409 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|         9 |  6410 | `				pCtxFrom->iDelegateState = 2;` |
|         6 |  6411 | `			}else{` |
|         5 |  6412 | `				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|         - |  6413 | `					sizeof("IteratorAggregate")-1,FALSE,0);` |
|         7 |  6414 | `				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){` |
|         - |  6415 | `					/* Delegate to the Iterator returned by getIterator() */` |
|         - |  6416 | `					ph7_value sIt;` |
|         5 |  6417 | `					PH7_MemObjInit(pVm,&sIt);` |
|         5 |  6418 | `					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);` |
|         5 |  6419 | `					if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){` |
|         - |  6420 | `						/* getIterator() threw/aborted: drop it, consume the` |
|         - |  6421 | `						 * operand, and propagate. */` |
|       ! 0 |  6422 | `						PH7_MemObjRelease(&sIt);` |
|       ! 0 |  6423 | `						VmPopOperand(&pTos,1);` |
|       ! 0 |  6424 | `						goto yf_propagate;` |
|         - |  6425 | `					}` |
|         4 |  6426 | `					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls` |
|         5 |  6427 | `						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){` |
|         5 |  6428 | `						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);` |
|         5 |  6429 | `						pCtxFrom->iDelegateState = 2;` |
|         3 |  6430 | `					}else{` |
|       ! 0 |  6431 | `						bIterable = 0;` |
|         - |  6432 | `					}` |
|         5 |  6433 | `					PH7_MemObjRelease(&sIt);` |
|         3 |  6434 | `				}else{` |
|       ! 0 |  6435 | `					bIterable = 0;` |
|         - |  6436 | `				}` |
|         - |  6437 | `			}` |
|        36 |  6438 | `		}else{` |
|         6 |  6439 | `			bIterable = 0;` |
|         - |  6440 | `		}` |
|       101 |  6441 | `		VmPopOperand(&pTos,1); /* Consume the iterable operand */` |
|       101 |  6442 | `		if( !bIterable ){` |
|         - |  6443 | `			/* Non-iterable source: throw a catchable Error (PHP 8.5), then` |
|         - |  6444 | `			 * funnel through the shared teardown/route path. */` |
|         6 |  6445 | `			rc = VmThrowFromVm(&(*pVm),"Error",` |
|         - |  6446 | `				"Can use \"yield from\" only with arrays and Traversables",` |
|         - |  6447 | `				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);` |
|         6 |  6448 | `			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         6 |  6449 | `			goto yf_propagate;` |
|         - |  6450 | `		}` |
|        97 |  6451 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|         - |  6452 | `			/* A GENERATOR delegate is not rewound: php links it as a child node and` |
|         - |  6453 | ``			 * only INITIALIZES it, so `yield from $g` over a half-consumed generator`` |
|         - |  6454 | `			 * continues from where it stands. One state it refuses outright, with` |
|         - |  6455 | `			 * its own Error rather than the traverse/rewind wording the other entry` |
|         - |  6456 | `			 * points use — a generator that has already run to its end, which PHL` |
|         - |  6457 | `			 * delegated to in silence and yielded NOTHING from. */` |
|        57 |  6458 | `			ph7_class_instance *pDel = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|        57 |  6459 | `			if( PH7_VmGeneratorIsClosed(&(*pVm),pDel) ){` |
|         3 |  6460 | `				rc = VmThrowFromVm(&(*pVm),"Error",` |
|         - |  6461 | `					"Generator passed to yield from was aborted without proper return and is unable to continue",` |
|         - |  6462 | `					sizeof("Generator passed to yield from was aborted without proper return and is unable to continue")-1);` |
|         3 |  6463 | `				rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         3 |  6464 | `				goto yf_propagate;` |
|         - |  6465 | `			}` |
|        55 |  6466 | `			rcm = PH7_VmGeneratorPrime(&(*pVm),pDel);` |
|        55 |  6467 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        67 |  6468 | `		}else if( pCtxFrom->iDelegateState >= 2 ){` |
|         - |  6469 | `			/* rewind() a plain Iterator delegate */` |
|        13 |  6470 | `			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|         - |  6471 | `				"rewind",sizeof("rewind")-1,0);` |
|        13 |  6472 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|         5 |  6473 | `		}` |
|        47 |  6474 | `	}else{` |
|         - |  6475 | `		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a` |
|         - |  6476 | `		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()` |
|         - |  6477 | `		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)` |
|         - |  6478 | `		 * ignore send() and just advance with next(). */` |
|         - |  6479 | `#ifdef UNTRUST` |
|         - |  6480 | `		if( pTos < pStack ){ goto Abort; }` |
|         - |  6481 | `#endif` |
|       139 |  6482 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|        78 |  6483 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|         - |  6484 | `			/* A pending Generator::throw() on the outer was parked on pInjected by the` |
|         - |  6485 | `			 * body-entry gate (which skips its own raise for state 3); forward it as a` |
|         - |  6486 | `			 * throw into the delegate, else forward the sent value (pTos, which` |
|         - |  6487 | `			 * VmResumeCtx copies into the inner's own stack). */` |
|        78 |  6488 | `			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;` |
|        78 |  6489 | `			pCtxFrom->pInjected = 0;` |
|        78 |  6490 | `			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        78 |  6491 | `				if( pInjFwd ){` |
|         - |  6492 | `					/* Borrowed ref: the outer's Generator::throw() holds it across this` |
|         - |  6493 | `					 * whole resume, so the inner inject path must not unref it. */` |
|         5 |  6494 | `					pInner->pCtx->pInjected = pInjFwd;` |
|         5 |  6495 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);` |
|         5 |  6496 | `					pInner->pCtx->pInjected = 0;` |
|         3 |  6497 | `				}else{` |
|        74 |  6498 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);` |
|         4 |  6499 | `				}` |
|        37 |  6500 | `			}else if( pInjFwd ){` |
|         - |  6501 | `				/* No live delegate to receive the throw (inner already finished/closed):` |
|         - |  6502 | `				 * raise it at the yield-from in the outer generator's own frame. */` |
|       ! 0 |  6503 | `				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);` |
|       ! 0 |  6504 | `				pTF->iFlags \|= VM_FRAME_THROW;` |
|       ! 0 |  6505 | `				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       ! 0 |  6506 | `			}` |
|        78 |  6507 | `			PH7_MemObjRelease(pTos);` |
|        78 |  6508 | `			pTos--;` |
|        78 |  6509 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        38 |  6510 | `		}else{` |
|        65 |  6511 | `			PH7_MemObjRelease(pTos);` |
|        65 |  6512 | `			pTos--;` |
|        65 |  6513 | `			if( pCtxFrom->iDelegateState >= 2 ){` |
|        17 |  6514 | `				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|         - |  6515 | `					"next",sizeof("next")-1,0);` |
|        17 |  6516 | `				if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|         5 |  6517 | `			}` |
|         - |  6518 | `		}` |
|         - |  6519 | `	}` |
|         - |  6520 | `	/* Fetch the current (key,value) of the delegate, or mark exhausted. */` |
|       213 |  6521 | `	if( pCtxFrom->iDelegateState == 1 ){` |
|        81 |  6522 | `		if( pCtxFrom->pDelegateNode == 0 ){` |
|        27 |  6523 | `			bExhausted = 1;` |
|        16 |  6524 | `		}else{` |
|        59 |  6525 | `			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);` |
|        59 |  6526 | `			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);` |
|         - |  6527 | `			/* Forward traversal follows pPrev (the hashmap's "reverse link",` |
|         - |  6528 | `			 * matching PH7_HashmapGetNextEntry). */` |
|        59 |  6529 | `			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;` |
|         - |  6530 | `		}` |
|        43 |  6531 | `	}else{` |
|       137 |  6532 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|         - |  6533 | `		ph7_value sValid;` |
|         - |  6534 | `		int isValid;` |
|       137 |  6535 | `		PH7_MemObjInit(pVm,&sValid);` |
|       137 |  6536 | `		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|       137 |  6537 | `		PH7_MemObjToBool(&sValid);` |
|       137 |  6538 | `		isValid = (sValid.x.iVal != 0);` |
|       137 |  6539 | `		PH7_MemObjRelease(&sValid);` |
|       137 |  6540 | `		if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       137 |  6541 | `		if( !isValid ){` |
|        35 |  6542 | `			bExhausted = 1;` |
|        20 |  6543 | `		}else{` |
|       107 |  6544 | `			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|       107 |  6545 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       107 |  6546 | `			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       107 |  6547 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|         - |  6548 | `		}` |
|         - |  6549 | `	}` |
|       213 |  6550 | `	if( bExhausted ){` |
|         - |  6551 | `		/* Expression value: inner Generator's return value (state 3) or NULL. */` |
|         - |  6552 | `		ph7_value sResult;` |
|        57 |  6553 | `		PH7_MemObjInit(pVm,&sResult);` |
|        57 |  6554 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|        30 |  6555 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|        30 |  6556 | `			if( pInner && pInner->pCtx ){` |
|        30 |  6557 | `				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);` |
|        13 |  6558 | `			}` |
|        13 |  6559 | `		}` |
|        57 |  6560 | `		PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|        57 |  6561 | `		pCtxFrom->pDelegateNode = 0;` |
|        57 |  6562 | `		pCtxFrom->iDelegateState = 0;` |
|        57 |  6563 | `		pTos++;` |
|        57 |  6564 | `		PH7_MemObjStore(&sResult,pTos);` |
|        57 |  6565 | `		PH7_MemObjRelease(&sResult);` |
|        57 |  6566 | `		PH7_MemObjRelease(&sKey);` |
|        57 |  6567 | `		PH7_MemObjRelease(&sVal);` |
|        57 |  6568 | `		break; /* fall through to pc+1 with the result on the stack top */` |
|         - |  6569 | `	}` |
|         - |  6570 | `	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.` |
|         - |  6571 | `	 * The outer generator's implicit auto-key counter is NOT advanced by the` |
|         - |  6572 | ``	 * delegated keys — PHP keeps it independent across `yield from`, so a later`` |
|         - |  6573 | ``	 * plain `yield` continues from the outer's own counter. */`` |
|       161 |  6574 | `	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);` |
|       161 |  6575 | `	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);` |
|       161 |  6576 | `	PH7_MemObjRelease(&sKey);` |
|       161 |  6577 | `	PH7_MemObjRelease(&sVal);` |
|         - |  6578 | `	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */` |
|       161 |  6579 | `	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));` |
|       161 |  6580 | `	goto Suspend;` |
|        11 |  6581 | `yf_propagate:` |
|         - |  6582 | `	/* A delegate iterator method threw/aborted (or the source was non-iterable):` |
|         - |  6583 | `	 * tear down the delegation, then route via the shared dispatch macro. rcm is` |
|         - |  6584 | `	 * always PH7_EXCEPTION or PH7_ABORT here. */` |
|        26 |  6585 | `	PH7_MemObjRelease(&sKey);` |
|        26 |  6586 | `	PH7_MemObjRelease(&sVal);` |
|        26 |  6587 | `	PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|        26 |  6588 | `	pCtxFrom->pDelegateNode = 0;` |
|        26 |  6589 | `	pCtxFrom->iDelegateState = 0;` |
|        26 |  6590 | `	PH7_DISPATCH_ENFORCE_RC(rcm)` |
|       ! 0 |  6591 | `	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */` |
|         - |  6592 | `}` |
|         - |  6593 | `/*` |
|         - |  6594 | ` * OP_CALL P1 * *` |
|         - |  6595 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|         - |  6596 | ` *  function on the stack.` |
|         - |  6597 | ` */` |
|         - |  6598 | `/*` |
|         - |  6599 | ` * OP_CALL_INIT * P2 *` |
|         - |  6600 | ` *  Screen the callee on TOS where it is WRITTEN — before this call's arguments run.` |
|         - |  6601 | ` *` |
|         - |  6602 | ` *  php resolves a call's target at INIT_FCALL / INIT_FCALL_BY_NAME / INIT_DYNAMIC_CALL` |
|         - |  6603 | `` *  and raises there, so `undefinedFn(s(1))`, `$f(s(1))` over a misspelled name and`` |
|         - |  6604 | `` *  `$v(s(1))` over an int all refuse BEFORE `s(1)` runs. PHL only ever looked at the`` |
|         - |  6605 | ` *  callee inside OP_CALL, one instruction after the whole argument list, so every one` |
|         - |  6606 | ` *  of those programs produced the argument's side effects (or its exception) first and` |
|         - |  6607 | ` *  php's Error second. The messages were already identical; only the order was not.` |
|         - |  6608 | ` *` |
|         - |  6609 | ` *  The verdict is the FIRST-CLASS-CALLABLE creation screen, unchanged and shared: php` |
|         - |  6610 | `` *  gives `f(...)` the direct call's taxonomy word for word, which makes VmFccValueError`` |
|         - |  6611 | ` *  the one builder for both. The value is left exactly as it is — OP_CALL still does its` |
|         - |  6612 | ` *  own resolution — so this adds a refusal and changes nothing that succeeds. P2 == 1` |
|         - |  6613 | ` *  when the compiler namespace-qualified the name, which is the one bit php's` |
|         - |  6614 | `` *  global-function fallback needs (an unqualified `strlen(...)` inside a namespace).`` |
|         - |  6615 | ` *` |
|         - |  6616 | ` *  Not emitted for a callee whose OP_MEMBER already screened it, for a first-class` |
|         - |  6617 | ` *  callable (OP_LOAD_FCC screens it, with nothing running in between), or for a call` |
|         - |  6618 | ` *  with no arguments at all — there the call IS the first thing that happens.` |
|         - |  6619 | ` */` |
|   8293136 |  6620 | `case PH7_OP_CALL_INIT: {` |
|         - |  6621 | `	/* This screen resolves the callee that OP_CALL is about to resolve again -- it is` |
|         - |  6622 | `	 * here only so php's Error lands before the arguments run -- and it was 4.4% of a` |
|         - |  6623 | `	 * phpcs profile. When the callee is a compile-time constant (the push behind this` |
|         - |  6624 | `	 * instruction is an OP_LOADC) the answer can only change if the set of callable` |
|         - |  6625 | `	 * NAMES changes, and that bumps pVm->nCallableGen. So a site that has passed once` |
|         - |  6626 | `	 * passes for free until something is declared. A dynamic callee is never stamped` |
|         - |  6627 | `	 * and is screened on every call, as it must be. */` |
|  16589895 |  6628 | `	if( pInstr->nAux == pVm->nCallableGen ){` |
|  16169148 |  6629 | `		break;` |
|         - |  6630 | `	}` |
|    420752 |  6631 | `	if( pInstr->iP2 & PH7_CALLINIT_CONSTRUCT ){` |
|         - |  6632 | `		/* A language construct's call: its callee is a host function hidden from every` |
|         - |  6633 | `		 * name a script can spell, so the callability screen below would refuse the` |
|         - |  6634 | `		 * engine's own dispatch. OP_CALL resolves it through the same mark. */` |
|    262273 |  6635 | `		break;` |
|         - |  6636 | `	}` |
|    158484 |  6637 | `	bCallInitStamp = 0;` |
|    158479 |  6638 | `	if( (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_MAGICCALL)) == 0` |
|    158484 |  6639 | `	 && !VmValueIsClosure(pVm,pTos) ){` |
|    133405 |  6640 | `		const char *zInitCls = 0,*zInitMeth = 0;` |
|    133405 |  6641 | `		sxu32 nInitCls = 0,nInitMeth = 0;` |
|         - |  6642 | `		char zInitMsg[192];` |
|    133405 |  6643 | `		const char *zInitBad = 0;` |
|    133405 |  6644 | `		SyString sInitName = { 0, 0 };` |
|    133405 |  6645 | `		int bInitScoped = 0;` |
|    133405 |  6646 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|    131413 |  6647 | `			SyStringInitFromBuf(&sInitName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|         - |  6648 | `			/* A leading backslash only anchors the name to the global namespace. */` |
|    131413 |  6649 | `			if( sInitName.nByte > 0 && sInitName.zString[0] == '\\' ){` |
|        17 |  6650 | `				sInitName.zString++;` |
|        17 |  6651 | `				sInitName.nByte--;` |
|         7 |  6652 | `			}` |
|    131413 |  6653 | `			bInitScoped = PH7_VmCallableStringParts(sInitName.zString,sInitName.nByte,` |
|         - |  6654 | `				&zInitCls,&nInitCls,&zInitMeth,&nInitMeth);` |
|         - |  6655 | `			/* A plain function NAME is the one verdict that depends on nothing but the` |
|         - |  6656 | ``			 * callable set. A `Class::method` string is screened for VISIBILITY too,`` |
|         - |  6657 | `			 * and a trait's body can run under more than one class, so that one is` |
|         - |  6658 | `			 * asked every time. */` |
|    131413 |  6659 | `			bCallInitStamp = !bInitScoped;` |
|     65292 |  6660 | `		}` |
|    133405 |  6661 | `		if( bInitScoped ){` |
|         - |  6662 | ``			/* A `"Class::method"` string carries its whole taxonomy in one builder — the`` |
|         - |  6663 | `			 * class, the missing/abstract/inaccessible cases and the catch-all routing —` |
|         - |  6664 | `			 * and answers 0 when the call WILL run. It is asked unconditionally because` |
|         - |  6665 | `			 * the predicate below is not the same question: is_callable() accepts a` |
|         - |  6666 | `			 * non-static method named through a class, which the direct call refuses. */` |
|        44 |  6667 | `			zInitBad = VmCallableClassMethodError(&(*pVm),` |
|        14 |  6668 | `				PH7_VmExtractClass(&(*pVm),zInitCls,nInitCls,FALSE,0),` |
|        14 |  6669 | `				zInitCls,nInitCls,zInitMeth,nInitMeth,TRUE,zInitMsg,sizeof(zInitMsg));` |
|    133391 |  6670 | `		}else if( !PH7_VmIsCallable(&(*pVm),pTos,TRUE) ){` |
|         - |  6671 | `			/* Not callable under the name as WRITTEN. php's global fallback below may` |
|         - |  6672 | `			 * still find it, and that verdict is generation-dependent like any other --` |
|         - |  6673 | `			 * so the stamp is left standing here and only a real refusal retires it,` |
|         - |  6674 | `			 * which it does by throwing before the stamp is written. Most calls in a` |
|         - |  6675 | ``			 * namespaced file (every `count()`, `is_array()`, `trim()` in phpcs) take`` |
|         - |  6676 | `			 * exactly this path, and it is the expensive one: two callability screens` |
|         - |  6677 | `			 * and a temporary value for the shortened name. */` |
|       401 |  6678 | `			int bInitOk = 0;` |
|       401 |  6679 | `			if( (pInstr->iP2 & PH7_CALLINIT_NAMESPACED) && (pTos->iFlags & MEMOBJ_STRING) ){` |
|         - |  6680 | `				/* php's global fallback for an UNQUALIFIED name written inside a` |
|         - |  6681 | `				 * namespace: the current namespace first, the global one after. OP_CALL` |
|         - |  6682 | `				 * retries the same way from its argument map; this only has to agree` |
|         - |  6683 | `				 * about whether the call WILL resolve, so the shortened name is tested` |
|         - |  6684 | `				 * and thrown away. */` |
|       325 |  6685 | `				const char *zInitShort = sInitName.zString;` |
|         - |  6686 | `				sxu32 iInitPos;` |
|      4491 |  6687 | `				for( iInitPos = 0 ; iInitPos < sInitName.nByte ; ++iInitPos ){` |
|      4171 |  6688 | `					if( sInitName.zString[iInitPos] == '\\' ){` |
|       341 |  6689 | `						zInitShort = &sInitName.zString[iInitPos + 1];` |
|       168 |  6690 | `					}` |
|      2088 |  6691 | `				}` |
|       325 |  6692 | `				if( zInitShort != sInitName.zString ){` |
|         - |  6693 | `					ph7_value sInitShort;` |
|       325 |  6694 | `					PH7_MemObjInit(pVm,&sInitShort);` |
|       485 |  6695 | `					PH7_MemObjStringAppend(&sInitShort,zInitShort,` |
|       320 |  6696 | `						(sxu32)(sInitName.nByte - (sxu32)(zInitShort - sInitName.zString)));` |
|       325 |  6697 | `					bInitOk = PH7_VmIsCallable(&(*pVm),&sInitShort,TRUE);` |
|       325 |  6698 | `					PH7_MemObjRelease(&sInitShort);` |
|       160 |  6699 | `				}` |
|       160 |  6700 | `			}` |
|         - |  6701 | `			/* The FIRST-CLASS-CALLABLE creation screen's builder, unchanged and shared:` |
|         - |  6702 | ``			 * php gives `f(...)` the direct call's taxonomy word for word. It assumes the`` |
|         - |  6703 | `			 * predicate has already declined — a pair a class answers through __call is` |
|         - |  6704 | `			 * callable and never arrives here — which is why it sits under that test. */` |
|       401 |  6705 | `			if( !bInitOk ){` |
|        80 |  6706 | `				zInitBad = VmFccValueError(&(*pVm),pTos,zInitMsg,sizeof(zInitMsg));` |
|        38 |  6707 | `			}` |
|       198 |  6708 | `		}` |
|    133405 |  6709 | `		if( zInitBad ){` |
|         - |  6710 | `			sxi32 rcInit;` |
|        86 |  6711 | `			PH7_MemObjRelease(pTos);` |
|        86 |  6712 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        86 |  6713 | `			pTos->nIdx = SXU32_HIGH;` |
|        86 |  6714 | `			rcInit = VmThrowFromVm(&(*pVm),"Error",zInitBad,(sxu32)SyStrlen(zInitBad));` |
|        86 |  6715 | `			if( rcInit == SXERR_ABORT ){ goto Abort; }` |
|        86 |  6716 | `			rc = rcInit;` |
|       120 |  6717 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  6718 | `		}` |
|     66247 |  6719 | `	}` |
|    158402 |  6720 | `	if( bCallInitStamp && pc > 0 && aInstr[pc-1].iOp == PH7_OP_LOADC ){` |
|         - |  6721 | `		/* The callee is the same literal every time this site runs, so record that it` |
|         - |  6722 | `` 		 * was screened -- and at WHICH generation, because a later `function f(){}` `` |
|         - |  6723 | `		 * (or a class, or an unregistered host function) can change the answer. */` |
|    130259 |  6724 | `		pInstr->nAux = pVm->nCallableGen;` |
|     64715 |  6725 | `	}` |
|    158402 |  6726 | `	break;` |
|         - |  6727 | `}` |
|         - |  6728 | `/*` |
|         - |  6729 | ` * OP_NAMED_SEND P1 P2 P3` |
|         - |  6730 | ` *  Screen the NAMED argument just pushed against the callee still waiting below the` |
|         - |  6731 | `` *  argument region, the way php's SEND resolves a name: `Unknown named parameter`, or`` |
|         - |  6732 | `` *  `Named parameter $x overwrites previous argument` for a formal a positional argument`` |
|         - |  6733 | ` *  already filled, is thrown HERE -- before a later argument runs, and before a plain` |
|         - |  6734 | `` *  `$var` operand (a deferred load, still unread) says `Undefined variable`. The`` |
|         - |  6735 | ` *  arguments sent before it have been sent in php, so their deferred reads are settled` |
|         - |  6736 | ` *  first, each against the formal it binds to.` |
|         - |  6737 | ` *` |
|         - |  6738 | ` *  Only a callee whose target is known here is screened: a function name (with the` |
|         - |  6739 | ` *  namespace's global fallback) or an OP_MEMBER method key, from the compiled-function` |
|         - |  6740 | ` *  table or -- by its signature -- the host-function table or a native method's C body,` |
|         - |  6741 | `` *  a plain Closure through the function its `$__fn` names, a `new`'s class operand`` |
|         - |  6742 | ` *  through its constructor (none declared takes no name at all), and an array pair, a` |
|         - |  6743 | `` *  `"C::m"` string, an __invoke object or a method Closure through the method`` |
|         - |  6744 | ` *  VmNamedSendMethod finds. A __call routing and an unreachable method leave the name to` |
|         - |  6745 | ` *  OP_CALL's own resolution.` |
|         - |  6746 | ` *` |
|         - |  6747 | ` *  The same instruction is the SEND of a deferred operand that a later argument could` |
|         - |  6748 | `` *  run code past (PH7_ROT_READ): php reads `$u` in `f($u, s())` at its own position, so`` |
|         - |  6749 | `` *  its `Undefined variable` (or `Undefined array key`, or the by-reference parameter's`` |
|         - |  6750 | `` *  vivification) comes before `s()` runs, not at the call. A POSITIONAL argument has no`` |
|         - |  6751 | ` *  name to screen and is only read; a named one is read once its name has passed. A` |
|         - |  6752 | ` *  callee not known here leaves the read to OP_CALL, as it always did.` |
|         - |  6753 | ` *` |
|         - |  6754 | ` *  P1 = the argument's compile-time position, P2 = PH7_ROT_SPREAD when an unpack precedes` |
|         - |  6755 | `` *  it, \| PH7_ROT_NEW when the list is a `new`'s (\| PH7_ROT_ANON for an anonymous class,`` |
|         - |  6756 | ` *  whose class the map names), \| PH7_ROT_READ / PH7_ROT_POSITIONAL as above, P3 = the` |
|         - |  6757 | ` *  call's VmCallArgMap (none for a positional read in a list without names).` |
|         - |  6758 | ` */` |
|    167060 |  6759 | `case PH7_OP_NAMED_SEND: {` |
|    335020 |  6760 | `	VmCallArgMap *pSendMap = (VmCallArgMap *)pInstr->p3;` |
|         - |  6761 | `	sxi32 nSendPushed;` |
|         - |  6762 | `	ph7_value *pSendCallee;` |
|         - |  6763 | `	SyString sSendFn, *pSendName;` |
|    335020 |  6764 | `	SyHashEntry *pSendEntry = 0;` |
|    335020 |  6765 | `	ph7_vm_func *pSendFunc = 0;` |
|    335020 |  6766 | `	ph7_user_func *pSendHost = 0;` |
|    335020 |  6767 | `	ph7_vm_func_arg *aSendFormal = 0;` |
|    335020 |  6768 | `	sxu32 nSendFormal = 0, nSendNonVar, k;` |
|    335020 |  6769 | `	sxi32 iSendVar = -1;` |
|    335020 |  6770 | `	int bSendEngine = 0, bSendFallback = 0, iSendPass;` |
|    335020 |  6771 | `	int bSendNew = (pInstr->iP2 & PH7_ROT_NEW) != 0, bSendNoCtor = 0;` |
|    335020 |  6772 | `	int bSendPositional = (pInstr->iP2 & PH7_ROT_POSITIONAL) != 0;` |
|         - |  6773 | `	char zSendErr[160];` |
|    335020 |  6774 | `	if( bSendPositional ){` |
|    333496 |  6775 | `		if( (pTos->iFlags & (MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH)) == 0 ){` |
|    277455 |  6776 | `			break; /* a defined variable: read at its push, nothing left to send */` |
|         - |  6777 | `		}` |
|    113537 |  6778 | `		pSendName = 0;` |
|     56701 |  6779 | `	}else{` |
|      1529 |  6780 | `		if( pSendMap == 0 \|\| (sxu32)pInstr->iP1 >= pSendMap->nTotal ){` |
|       ! 0 |  6781 | `			break;` |
|         - |  6782 | `		}` |
|      1529 |  6783 | `		pSendName = &pSendMap->aNames[pInstr->iP1];` |
|         - |  6784 | `	}` |
|    230117 |  6785 | `	nSendPushed = pInstr->iP1 + 1` |
|    115056 |  6786 | `		+ ((pInstr->iP2 & PH7_ROT_SPREAD) ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1 + 1,&pTos[1]) : 0);` |
|    115061 |  6787 | `	pSendCallee = &pTos[-nSendPushed];` |
|    115056 |  6788 | `	if( bSendPositional && nSendPushed >= 1 && pSendCallee >= pStack` |
|    113537 |  6789 | `	 && (pInstr->iP2 & PH7_ROT_NEW) == 0 && (pSendCallee->iFlags & MEMOBJ_AUX_MAGICCALL) ){` |
|         - |  6790 | `		/* A __call / __callStatic routing: php packs the list into an array, so every` |
|         - |  6791 | `		 * operand is sent by value. */` |
|         5 |  6792 | `		sxi32 rcMagic = PH7_VmResolveDeferredArgs(&(*pVm),pTos,&pTos[1],0,0,0,0,1,0);` |
|         7 |  6793 | `		PH7_DISPATCH_ENFORCE_RC(rcMagic)` |
|         5 |  6794 | `		break;` |
|         - |  6795 | `	}` |
|    115052 |  6796 | `	if( (pSendName && pSendName->nByte == 0) \|\| nSendPushed < 1` |
|    172487 |  6797 | `	 \|\| ( (pInstr->iP2 & PH7_ROT_ANON) == 0` |
|    115026 |  6798 | `	   && (pSendCallee < pStack \|\| (pSendCallee->iFlags & MEMOBJ_AUX_MAGICCALL)) ) ){` |
|         7 |  6799 | `		break;` |
|         - |  6800 | `	}` |
|    115045 |  6801 | `	SyStringInitFromBuf(&sSendFn,"",0);` |
|    115045 |  6802 | `	if( bSendNew ){` |
|         - |  6803 | ``		/* A `new`'s list: its screen pass (OP_NEW, iP1 -1) resolved the class, refused`` |
|         - |  6804 | ``		 * every `new` php refuses before the arguments run, and left the operand -- a`` |
|         - |  6805 | ``		 * name, `self`/`static`/`parent` among them, or an object -- standing here. A`` |
|         - |  6806 | `		 * class that declares no constructor takes no name at all. An anonymous class` |
|         - |  6807 | `		 * leaves nothing there: its screen (OP_NEW, iP1 -2) declared it, and the map` |
|         - |  6808 | `		 * names it. */` |
|       183 |  6809 | `		ph7_class *pSendClass = 0;` |
|         - |  6810 | `		ph7_class_method *pSendCons;` |
|       183 |  6811 | `		if( pInstr->iP2 & PH7_ROT_ANON ){` |
|        55 |  6812 | `			if( pSendMap->sNewAnon.nByte > 0 ){` |
|        81 |  6813 | `				pSendClass = PH7_VmExtractClass(&(*pVm),pSendMap->sNewAnon.zString,` |
|        26 |  6814 | `					pSendMap->sNewAnon.nByte,FALSE,0);` |
|        29 |  6815 | `			}` |
|       156 |  6816 | `		}else if( (pSendCallee->iFlags & MEMOBJ_STRING) && SyBlobLength(&pSendCallee->sBlob) > 0 ){` |
|       190 |  6817 | `			pSendClass = PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pSendCallee->sBlob),` |
|        62 |  6818 | `				SyBlobLength(&pSendCallee->sBlob));` |
|        65 |  6819 | `		}else if( pSendCallee->iFlags & MEMOBJ_OBJ ){` |
|         3 |  6820 | `			pSendClass = ((ph7_class_instance *)pSendCallee->x.pOther)->pClass;` |
|         1 |  6821 | `		}` |
|       183 |  6822 | `		if( pSendClass == 0 ){` |
|       ! 0 |  6823 | `			break;` |
|         - |  6824 | `		}` |
|       183 |  6825 | `		pSendCons = PH7_ClassExtractMethod(pSendClass,"__construct",sizeof("__construct")-1);` |
|       183 |  6826 | `		if( pSendCons ){` |
|       161 |  6827 | `			pSendFunc = &pSendCons->sFunc;` |
|        83 |  6828 | `		}else{` |
|        25 |  6829 | `			bSendNoCtor = 1;` |
|         5 |  6830 | `		}` |
|    114956 |  6831 | `	}else if( (pSendFunc = VmNamedSendMethod(&(*pVm),pSendCallee)) != 0 ){` |
|         - |  6832 | ``		/* A pair, a `"C::m"` string, an __invoke object or a method Closure: the`` |
|         - |  6833 | `		 * method OP_CALL will land on, found through its class. */` |
|    114826 |  6834 | `	}else if( pSendCallee->iFlags & MEMOBJ_STRING ){` |
|    114705 |  6835 | `		bSendEngine = (pSendCallee->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_ENGINEFN)) != 0;` |
|    114705 |  6836 | `		bSendFallback = pSendMap && pSendMap->bIsNamespaced;` |
|    114705 |  6837 | `		SyStringInitFromBuf(&sSendFn,SyBlobData(&pSendCallee->sBlob),SyBlobLength(&pSendCallee->sBlob));` |
|    114705 |  6838 | `		if( sSendFn.nByte > 0 && sSendFn.zString[0] == '\\' ){` |
|       ! 0 |  6839 | `			sSendFn.zString++;` |
|       ! 0 |  6840 | `			sSendFn.nByte--;` |
|       ! 0 |  6841 | `		}` |
|     57365 |  6842 | `	}else if( VmValueIsClosure(pVm,pSendCallee)` |
|        79 |  6843 | `	 && (((ph7_class_instance *)pSendCallee->x.pOther)->iFlags & VM_INSTANCE_FCC_METHOD) == 0 ){` |
|         - |  6844 | ``		/* A plain Closure -- an anonymous function, `f(...)`, `strlen(...)`, or one bound`` |
|         - |  6845 | ``		 * to an object or a scope -- calls the function its `$__fn` names, an`` |
|         - |  6846 | `		 * already-resolved key the engine wrote. One built from a method names a METHOD` |
|         - |  6847 | `		 * there, which a function lookup would mistake; the branch above answered it, or` |
|         - |  6848 | `		 * it is a trampoline OP_CALL routes. */` |
|         - |  6849 | `		ph7_value *pSendFn;` |
|         - |  6850 | `		SyString sSendAttr;` |
|        49 |  6851 | `		SyStringInitFromBuf(&sSendAttr,"__fn",4);` |
|        49 |  6852 | `		pSendFn = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pSendCallee->x.pOther,&sSendAttr);` |
|        49 |  6853 | `		if( pSendFn == 0 \|\| (pSendFn->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 |  6854 | `			break;` |
|         - |  6855 | `		}` |
|        49 |  6856 | `		bSendEngine = 1;` |
|        49 |  6857 | `		bSendFallback = 0;` |
|        49 |  6858 | `		SyStringInitFromBuf(&sSendFn,SyBlobData(&pSendFn->sBlob),SyBlobLength(&pSendFn->sBlob));` |
|        26 |  6859 | `	}else{` |
|        19 |  6860 | `		break;` |
|         - |  6861 | `	}` |
|    229785 |  6862 | `	for( iSendPass = 0 ; iSendPass < 2 && pSendFunc == 0 && pSendHost == 0 && !bSendNew ; ++iSendPass ){` |
|    114783 |  6863 | `		SyString sSendTry = sSendFn;` |
|    114783 |  6864 | `		if( iSendPass == 1 ){` |
|         - |  6865 | `			/* OP_CALL's global fallback for an unqualified name written in a namespace. */` |
|        35 |  6866 | `			const char *zSendEnd = sSendFn.zString + sSendFn.nByte;` |
|        35 |  6867 | `			const char *zSendShort = zSendEnd;` |
|        35 |  6868 | `			if( !bSendFallback ){` |
|         5 |  6869 | `				break;` |
|         - |  6870 | `			}` |
|       256 |  6871 | `			while( zSendShort > sSendFn.zString && zSendShort[-1] != '\\' ){` |
|       228 |  6872 | `				zSendShort--;` |
|         2 |  6873 | `			}` |
|        30 |  6874 | `			if( zSendShort == sSendFn.zString \|\| zSendShort == zSendEnd ){` |
|       ! 0 |  6875 | `				break;` |
|         - |  6876 | `			}` |
|        30 |  6877 | `			SyStringInitFromBuf(&sSendTry,zSendShort,(sxu32)(zSendEnd - zSendShort));` |
|        14 |  6878 | `		}` |
|    114779 |  6879 | `		pSendEntry = PH7_VmGetUserFunction(pVm,(const void *)sSendTry.zString,sSendTry.nByte,bSendEngine);` |
|    114779 |  6880 | `		if( pSendEntry ){` |
|       955 |  6881 | `			pSendFunc = (ph7_vm_func *)pSendEntry->pUserData;` |
|       480 |  6882 | `		}else{` |
|    113829 |  6883 | `			pSendEntry = PH7_VmGetHostFunction(pVm,(const void *)sSendTry.zString,sSendTry.nByte,bSendEngine);` |
|    113829 |  6884 | `			if( pSendEntry ){` |
|    113797 |  6885 | `				pSendHost = (ph7_user_func *)pSendEntry->pUserData;` |
|     56826 |  6886 | `			}` |
|         - |  6887 | `		}` |
|     57322 |  6888 | `	}` |
|    115011 |  6889 | `	if( pSendFunc && (pSendFunc->iFlags & VM_FUNC_NATIVE) ){` |
|         - |  6890 | `		/* A C-bodied method: no compiled formals, and OP_CALL binds its names by the` |
|         - |  6891 | `		 * signature its foreign body carries, exactly as it does a builtin's. */` |
|       257 |  6892 | `		pSendHost = pSendFunc->pNative;` |
|       257 |  6893 | `		pSendFunc = 0;` |
|       126 |  6894 | `	}` |
|    115011 |  6895 | `	if( bSendPositional ){` |
|         - |  6896 | `		/* This one slot alone, against the formal at its position: an earlier operand a` |
|         - |  6897 | `		 * frameless call reads itself is still unread below it, and stays so. */` |
|    113533 |  6898 | `		sxu32 iSendAt = (sxu32)pInstr->iP1;` |
|         - |  6899 | `		sxi32 rcRead;` |
|    113533 |  6900 | `		if( pSendFunc ){` |
|        47 |  6901 | `			ph7_vm_func_arg *aAt = (ph7_vm_func_arg *)SySetBasePtr(&pSendFunc->aArgs);` |
|        47 |  6902 | `			sxu32 nAt = SySetUsed(&pSendFunc->aArgs);` |
|        47 |  6903 | `			if( iSendAt < nAt ){` |
|        45 |  6904 | `				aAt += iSendAt;` |
|        45 |  6905 | `				nAt -= iSendAt;` |
|        24 |  6906 | `			}else if( nAt > 0 && (aAt[nAt-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|         3 |  6907 | `				aAt += nAt - 1; /* the variadic absorbs the tail */` |
|         3 |  6908 | `				nAt = 1;` |
|         2 |  6909 | `			}else{` |
|       ! 0 |  6910 | `				nAt = 0; /* an extra argument binds by value */` |
|         - |  6911 | `			}` |
|        47 |  6912 | `			rcRead = PH7_VmResolveDeferredArgs(&(*pVm),pTos,&pTos[1],aAt,nAt,0,0,0,0);` |
|    113511 |  6913 | `		}else if( pSendHost && (pInstr->iP2 & PH7_ROT_FRAMELESS) == 0 ){` |
|       233 |  6914 | `			rcRead = PH7_VmResolveDeferredArgs(&(*pVm),pTos,&pTos[1],0,0,` |
|       114 |  6915 | `				iSendAt < 31 ? (pSendHost->nByRefMask >> iSendAt) & 1u : 0,0,0,0);` |
|    113432 |  6916 | `		}else if( bSendNoCtor ){` |
|         - |  6917 | `			/* No constructor: php still sends the list, by value, to nothing. */` |
|         3 |  6918 | `			rcRead = PH7_VmResolveDeferredArgs(&(*pVm),pTos,&pTos[1],0,0,0,0,1,0);` |
|         2 |  6919 | `		}else{` |
|    113373 |  6920 | `			break;` |
|         - |  6921 | `		}` |
|       167 |  6922 | `		PH7_DISPATCH_ENFORCE_RC(rcRead)` |
|       153 |  6923 | `		break;` |
|         - |  6924 | `	}` |
|      1483 |  6925 | `	if( pSendFunc ){` |
|       897 |  6926 | `		aSendFormal = (ph7_vm_func_arg *)SySetBasePtr(&pSendFunc->aArgs);` |
|       897 |  6927 | `		nSendFormal = SySetUsed(&pSendFunc->aArgs);` |
|      2387 |  6928 | `		for( k = 0 ; k < nSendFormal ; ++k ){` |
|      1759 |  6929 | `			if( aSendFormal[k].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       269 |  6930 | `				iSendVar = (sxi32)k;` |
|       269 |  6931 | `				break;` |
|         - |  6932 | `			}` |
|       750 |  6933 | `		}` |
|       897 |  6934 | `		nSendNonVar = iSendVar >= 0 ? (sxu32)iSendVar : nSendFormal;` |
|      1515 |  6935 | `		for( k = 0 ; k < nSendNonVar ; ++k ){` |
|      1198 |  6936 | `			if( SyStringLength(&aSendFormal[k].sName) == pSendName->nByte` |
|      1092 |  6937 | `			 && SyMemcmp(SyStringData(&aSendFormal[k].sName),pSendName->zString,pSendName->nByte) == 0 ){` |
|       585 |  6938 | `				break;` |
|         - |  6939 | `			}` |
|       314 |  6940 | `		}` |
|      1037 |  6941 | `	}else if( pSendHost ){` |
|         - |  6942 | `		/* A host function has no compiled formals: its signature names the parameters,` |
|         - |  6943 | `		 * the same string PH7_VmBindNamedArgsToSig binds by at the call. */` |
|         - |  6944 | `		int bSendVariadic;` |
|       567 |  6945 | `		int iSendPos = PH7_VmSigNamedParam(pSendHost->zSig,pSendName,&bSendVariadic);` |
|       567 |  6946 | `		if( iSendPos == -2 ){` |
|        19 |  6947 | `			break;` |
|         - |  6948 | `		}` |
|       549 |  6949 | `		iSendVar = bSendVariadic ? 0 : -1;` |
|       549 |  6950 | `		nSendNonVar = (sxu32)(iSendPos + 1);` |
|       549 |  6951 | `		k = iSendPos >= 0 ? (sxu32)iSendPos : nSendNonVar;` |
|       299 |  6952 | `	}else if( bSendNoCtor ){` |
|        22 |  6953 | `		nSendNonVar = 0;` |
|        22 |  6954 | `		k = 0;` |
|        12 |  6955 | `	}else{` |
|         5 |  6956 | `		break;` |
|         - |  6957 | `	}` |
|      1461 |  6958 | `	zSendErr[0] = 0;` |
|      1461 |  6959 | `	if( k < nSendNonVar ){` |
|         - |  6960 | `		/* A formal an earlier POSITIONAL argument filled. Only asked when no unpack` |
|         - |  6961 | `		 * precedes: an unpack's own string keys are names OP_CALL has to weigh. */` |
|       857 |  6962 | `		sxu32 nSendPos = 0;` |
|       857 |  6963 | `		if( (pInstr->iP2 & PH7_ROT_SPREAD) == 0 ){` |
|       977 |  6964 | `			while( nSendPos < (sxu32)pInstr->iP1 && pSendMap->aNames[nSendPos].nByte == 0 ){` |
|       135 |  6965 | `				nSendPos++;` |
|         5 |  6966 | `			}` |
|       847 |  6967 | `			if( k < nSendPos ){` |
|        74 |  6968 | `				SyBufferFormat(zSendErr,sizeof(zSendErr),` |
|         - |  6969 | `					"Named parameter $%.*s overwrites previous argument",` |
|        46 |  6970 | `					(int)pSendName->nByte,pSendName->zString);` |
|        23 |  6971 | `			}` |
|       426 |  6972 | `		}` |
|      1035 |  6973 | `	}else if( iSendVar < 0 ){` |
|       251 |  6974 | `		SyBufferFormat(zSendErr,sizeof(zSendErr),"Unknown named parameter $%.*s",` |
|       164 |  6975 | `			(int)pSendName->nByte,pSendName->zString);` |
|        82 |  6976 | `	}` |
|      1461 |  6977 | `	if( zSendErr[0] ){` |
|         - |  6978 | `		sxi32 rcSend;` |
|       215 |  6979 | `		if( (pInstr->iP2 & PH7_ROT_SPREAD) == 0 && pInstr->iP1 > 0 ){` |
|         - |  6980 | ``			/* The arguments before this one were SENT in php: a deferred `$var` read`` |
|         - |  6981 | `			 * warns (or is created, for a by-reference formal) now. */` |
|       110 |  6982 | `			rcSend = PH7_VmResolveDeferredArgs(&(*pVm),&pTos[-pInstr->iP1],pTos,` |
|        35 |  6983 | `				aSendFormal,nSendFormal,pSendHost ? pSendHost->nByRefMask : 0,0,0,pSendMap);` |
|        75 |  6984 | `			PH7_DISPATCH_ENFORCE_RC(rcSend)` |
|        35 |  6985 | `		}` |
|       215 |  6986 | `		if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|         - |  6987 | `			/* A subscript or property operand has been fetched in php by now -- only a` |
|         - |  6988 | `			 * plain variable is read by the SEND itself. */` |
|         8 |  6989 | `			rcSend = PH7_VmResolveDeferredArgs(&(*pVm),pTos,&pTos[1],0,0,0,0,1,0);` |
|         8 |  6990 | `			PH7_DISPATCH_ENFORCE_RC(rcSend)` |
|         3 |  6991 | `		}` |
|       215 |  6992 | `		rcSend = VmThrowNamedArgError(&(*pVm),zSendErr,(sxu32)SyStrlen(zSendErr));` |
|       215 |  6993 | `		if( rcSend == PH7_ABORT ){ goto Abort; }` |
|       211 |  6994 | `		rc = rcSend;` |
|       223 |  6995 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  6996 | `	}` |
|      1251 |  6997 | `	if( (pInstr->iP2 & (PH7_ROT_READ\|PH7_ROT_SPREAD)) == PH7_ROT_READ && !bSendNoCtor ){` |
|         - |  6998 | `		/* The name passed, and a later argument can run code: read the operand now. */` |
|         8 |  6999 | `		sxi32 rcRead = PH7_VmResolveDeferredArgs(&(*pVm),&pTos[-pInstr->iP1],&pTos[1],` |
|         2 |  7000 | `			aSendFormal,nSendFormal,pSendHost ? pSendHost->nByRefMask : 0,0,0,pSendMap);` |
|         6 |  7001 | `		PH7_DISPATCH_ENFORCE_RC(rcRead)` |
|         2 |  7002 | `	}` |
|      1251 |  7003 | `	break;` |
|         - |  7004 | `}` |
|         - |  7005 | `/*` |
|         - |  7006 | ` * OP_ROT_CALLEE P1 P2 *` |
|         - |  7007 | ` *  Turn a call's operand region over: [callee][arg0..argN] becomes [arg0..argN][callee],` |
|         - |  7008 | ` *  which is the layout OP_CALL's entire dispatch is written against.` |
|         - |  7009 | ` *` |
|         - |  7010 | ` *  The codegen pushes the callee FIRST because php resolves it where it is written —` |
|         - |  7011 | ` *  before a single argument runs — so an undefined or inaccessible method is refused` |
|         - |  7012 | `` *  ahead of the argument list's side effects, and a `?->` on null skips the arguments`` |
|         - |  7013 | ` *  altogether. Everything downstream of this instruction still sees the historical` |
|         - |  7014 | ` *  stack, so the reordering costs one memory move per call and nothing else.` |
|         - |  7015 | ` *` |
|         - |  7016 | ` *  P1 is the compile-time argument count; P2 carries PH7_ROT_SPREAD (this call unpacks,` |
|         - |  7017 | ` *  so the runtime count is P1 plus its OWN runs' net growth) and PH7_ROT_TWOSLOT (the` |
|         - |  7018 | ` *  callee is a method pair, [receiver][name]). A __call routing collapses that pair to` |
|         - |  7019 | ` *  one marked carrier at run time, which is read off the slot rather than guessed.` |
|         - |  7020 | ` */` |
|   8825448 |  7021 | `case PH7_OP_ROT_CALLEE: {` |
|  35308947 |  7022 | `	sxi32 nRotArgs = pInstr->iP1` |
|  17654471 |  7023 | `		+ ((pInstr->iP2 & PH7_ROT_SPREAD)` |
|         - |  7024 | `			/* One past the last argument is one past the TOP here: the callee sits` |
|         - |  7025 | `			 * BELOW the region, not above it as at OP_CALL. */` |
|   8826169 |  7026 | `			? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,&pTos[1]) : 0);` |
|  17654476 |  7027 | `	if( nRotArgs < 0 ){` |
|         - |  7028 | `		/* Unreachable: an empty unpack subtracts one per compile-time position, so the` |
|         - |  7029 | `		 * net can reach 0 and no lower. Clamped rather than trusted — reading above the` |
|         - |  7030 | `		 * top to find the callee is not a failure mode worth leaving open. */` |
|       ! 0 |  7031 | `		nRotArgs = 0;` |
|       ! 0 |  7032 | `	}` |
|         - |  7033 | `	{` |
|         - |  7034 | `		ph7_value aCallee[2];` |
|  17654476 |  7035 | `		ph7_value *pTopCallee = &pTos[-nRotArgs];` |
|  17654476 |  7036 | `		sxi32 nCallee = (pInstr->iP2 & PH7_ROT_TWOSLOT) ? 2 : 1;` |
|         - |  7037 | `		ph7_value *pBase;` |
|         - |  7038 | `		sxi32 i;` |
|  17654476 |  7039 | `		if( nCallee > 1 && (pTopCallee->iFlags & MEMOBJ_AUX_MAGICCALL) ){` |
|         - |  7040 | `			/* OP_MEMBER routed a missing/inaccessible name to __call: it consumed the` |
|         - |  7041 | `			 * receiver and left ONE carrier slot, so the pair the compiler counted on` |
|         - |  7042 | `			 * is not there. */` |
|       108 |  7043 | `			nCallee = 1;` |
|        52 |  7044 | `		}` |
|  17654476 |  7045 | `		pBase = pTopCallee - (nCallee - 1);` |
|         - |  7046 | `#ifdef UNTRUST` |
|         - |  7047 | `		if( pBase < pStack ){` |
|         - |  7048 | `			goto Abort;` |
|         - |  7049 | `		}` |
|         - |  7050 | `#endif` |
|  17654476 |  7051 | `		if( nRotArgs > 0 ){` |
|  35360991 |  7052 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  17706592 |  7053 | `				aCallee[i] = pBase[i];` |
|   8855073 |  7054 | `			}` |
|  46312851 |  7055 | `			for( i = 0 ; i < nRotArgs ; ++i ){` |
|  28658452 |  7056 | `				pBase[i] = pBase[i + nCallee];` |
|  14331021 |  7057 | `			}` |
|  35360991 |  7058 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  17706592 |  7059 | `				pBase[nRotArgs + i] = aCallee[i];` |
|   8855073 |  7060 | `			}` |
|   8828987 |  7061 | `		}` |
|  17654476 |  7062 | `		if( pInstr->iP2 & PH7_ROT_SPREAD ){` |
|         - |  7063 | `			/* The argument region now ends nCallee slots lower than it did, so this` |
|         - |  7064 | `			 * call's captured unpack runs — the suffix VmSpreadOwnExtra just assigned` |
|         - |  7065 | `			 * to it, all of them anchored inside the region — move with it, and OP_CALL` |
|         - |  7066 | ``			 * re-derives the same count from them. Unconditional: an `f(...[])` unpack`` |
|         - |  7067 | `			 * moves NO argument (its run is zero-width) and still has to be re-anchored,` |
|         - |  7068 | `			 * or the recount reads it as an ordinary slot and eats one slot too many.` |
|         - |  7069 | `			 * An ENCLOSING call's runs sit below the callee and are left alone. */` |
|      1542 |  7070 | `			sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      1542 |  7071 | `			VmSpreadRun *aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|         - |  7072 | `			sxu32 r;` |
|      3153 |  7073 | `			for( r = pVm->nSpreadCallBase ; r < nRun ; ++r ){` |
|      1616 |  7074 | `				aRun[r].pStart -= nCallee;` |
|       763 |  7075 | `			}` |
|       721 |  7076 | `		}` |
|         - |  7077 | `	}` |
|  17654476 |  7078 | `	break;` |
|         - |  7079 | `}` |
|  10275391 |  7080 | `case PH7_OP_CALL: {` |
|         - |  7081 | `	/* iP2 is a compile-time bit SET (PH7_CALL_*). PH7_CALL_SPREAD: count only THIS` |
|         - |  7082 | `	 * call's own unpack expansion (VmSpreadOwnExtra, derived from the captured runs on` |
|         - |  7083 | `	 * top of the stack) — an INNER spread-bearing call evaluated inside this argument` |
|         - |  7084 | ``	 * list (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not`` |
|         - |  7085 | `	 * be conflated, which a single shared accumulator could not express. */` |
|  41107137 |  7086 | `	sxi32 nCallArgs = pInstr->iP1` |
|  20553566 |  7087 | `		+ ((pInstr->iP2 & PH7_CALL_SPREAD) ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|         - |  7088 | `	ph7_value *pArg;` |
|         - |  7089 | `	/* Consume the engine's magic-dispatch latch here, at the head of the ONE call` |
|         - |  7090 | `	 * it was set for, whatever path that call then takes. Left standing it would` |
|         - |  7091 | `	 * describe the next call instead. */` |
|  20553571 |  7092 | `	int bMagicDispatch = pVm->bMagicDispatch;` |
|         - |  7093 | `	/* ...and the member resolution's own verdict, which rides the callee SLOT rather` |
|         - |  7094 | `	 * than the VM: an OP_MEMBER that produced this callee already decided its` |
|         - |  7095 | `	 * visibility against the entry it chose, so the screen below must stand down. */` |
|  41006298 |  7096 | `	int bMemberScreened = (pTos->iFlags & MEMOBJ_AUX_MEMBERCALL) != 0` |
|  20553566 |  7097 | `		\|\| pVm->bClosureScreened;` |
|         - |  7098 | `	/* ...and whether the NAME in that slot is one of the engine's own function-table` |
|         - |  7099 | `	 * keys rather than something the program spelled: an OP_MEMBER method resolution` |
|         - |  7100 | ``	 * pushes the method's `sVmName` (`[__Class@meth_xxxxxxxxxx]`), and so do the two`` |
|         - |  7101 | `	 * synthetic call builders. Read here because the member mark is cleared just` |
|         - |  7102 | `	 * below; the closure branch adds its own case further down. It is what lets` |
|         - |  7103 | `	 * PH7_VmGetUserFunction refuse those keys to a SCRIPT that spells one. */` |
|  20553571 |  7104 | `	int bEngineCallee = (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_ENGINEFN)) != 0;` |
|  20553571 |  7105 | `	int bViaClosure = 0; /* the target was a Closure, unwrapped below: not a DIRECT spelling */` |
|         - |  7106 | `	/* ...and the internal-callback latch, for the same reason: it describes THIS call` |
|         - |  7107 | `	 * (an internal function invoking a userland callback binds its arguments weakly),` |
|         - |  7108 | `	 * and a call the callback body makes must not inherit it. */` |
|  20553571 |  7109 | `	int bCallbackWeak = pVm->bCallbackWeak;` |
|         - |  7110 | `	/* ...and the autoloader's trace-only one (VM_FRAME_NATIVE_TRACE): an autoloader` |
|         - |  7111 | `	 * that is itself a builtin has no frame to mark, and whatever the file it` |
|         - |  7112 | `	 * includes calls first must not be marked in its place. */` |
|  20553571 |  7113 | `	SyString *pNativeTrace = pVm->pNativeTraceName;` |
|         - |  7114 | `	/* ...and whether this call's ANSWER is thrown away, which is what a` |
|         - |  7115 | `	 * #[\NoDiscard] callee warns about. The compiled call site says so directly` |
|         - |  7116 | `	 * (bDiscard, stamped where the statement pops the value: php's` |
|         - |  7117 | `	 * !RETURN_VALUE_USED); php also propagates the same bit THROUGH` |
|         - |  7118 | ``	 * `call_user_func()`/`call_user_func_array()`, which is why`` |
|         - |  7119 | ``	 * `call_user_func('f');` warns and `array_map('f', $a);` does not, and a`` |
|         - |  7120 | `	 * synthetic OP_CALL has no call site of its own to carry it -- so the two` |
|         - |  7121 | `	 * builtins hand it over on this latch. Consumed here like the rest. */` |
|  20553571 |  7122 | `	int bResultDropped = pVm->bDiscardCallback ? 1 : (pInstr->bDiscard != 0);` |
|         - |  7123 | `	/* php's forwarding of that bit through call_user_func() is a COMPILE-time` |
|         - |  7124 | ``	 * special case on the literal name, so `$g = "call_user_func"; $g("f");` does`` |
|         - |  7125 | `	 * not forward. The callee slot's nIdx says which spelling this is: a compiled` |
|         - |  7126 | `	 * literal is marked constant, a variable read is not. */` |
|  20553571 |  7127 | `	int bLiteralCallee = (pTos->nIdx == SXU32_HIGH);` |
|         - |  7128 | ``	/* php ELIDES a `call_user_func()`/`call_user_func_array()` frame only when its`` |
|         - |  7129 | `	 * compiler can prove the name is the global function -- written in the global` |
|         - |  7130 | `	 * namespace, or fully qualified, or imported. An UNQUALIFIED call inside a` |
|         - |  7131 | `	 * namespace could still resolve to a namespace-local function, so php cannot` |
|         - |  7132 | `	 * fold it and emits the real internal frame: the callback then has no userland` |
|         - |  7133 | ``	 * caller, exactly as under array_map. `bIsNamespaced` is that compile-time`` |
|         - |  7134 | `	 * question, already recorded on the call's argument map for the global-fallback` |
|         - |  7135 | `	 * resolution. (Every probe behind this rule had been written in the global` |
|         - |  7136 | `	 * namespace, which is why the forwards looked unconditionally elided; pest calls` |
|         - |  7137 | ``	 * one from inside `namespace Pest\Concerns`.) */`` |
|  20553571 |  7138 | `	int bNsCallee = (pInstr->p3 && ((VmCallArgMap *)pInstr->p3)->bIsNamespaced) ? 1 : 0;` |
|         - |  7139 | `	/* php's ZEND_CALL_DYNAMIC, decided the way php's compiler decides it: a compiled` |
|         - |  7140 | ``	 * call is dynamic when its callee is not a literal name (`$n()`, `$c()`, an array`` |
|         - |  7141 | `	 * pair; the Closure branch below adds its own case, a temporary holds no slot` |
|         - |  7142 | `	 * index to read). A SYNTHETIC call is one the C dispatcher built, and php's` |
|         - |  7143 | `	 * zend_call_function always marks those -- except for the one forward its` |
|         - |  7144 | ``	 * compiler folded into a direct call (`call_user_func('f', ...)` on a literal,`` |
|         - |  7145 | `	 * global name), which reaches here with NEITHER latch armed. Read by the host` |
|         - |  7146 | `	 * function's context (PH7_CTX_CALL_DYNAMIC): the six functions that answer from` |
|         - |  7147 | `	 * their caller's frame refuse it. */` |
|  32344965 |  7148 | `	int bDynamicCall = (pInstr->nLine == 0)` |
|   3032386 |  7149 | `		? (bCallbackWeak \|\| pVm->pNativeFrameName != 0 \|\| pVm->bDynamicForward)` |
|  20553946 |  7150 | `		: !bLiteralCallee;` |
|  20553571 |  7151 | `	pVm->bDynamicForward = 0;` |
|  20553571 |  7152 | `	pVm->bDiscardCallback = 0;` |
|  20553571 |  7153 | `	pVm->bMagicDispatch = 0;` |
|  20553571 |  7154 | `	pVm->bClosureScreened = 0;` |
|  20553571 |  7155 | `	pVm->bClosureStaticTramp = 0;` |
|  20553571 |  7156 | `	pVm->bClosureNoNamed = 0;` |
|  20553571 |  7157 | `	pVm->pClosureMethodCls = 0;` |
|  20553571 |  7158 | `	pVm->bCallbackWeak = 0;` |
|  20553571 |  7159 | `	pVm->pNativeTraceName = 0;` |
|  20553571 |  7160 | `	pTos->iFlags &= ~MEMOBJ_AUX_MEMBERCALL;` |
|  20553571 |  7161 | `	pArg = &pTos[-nCallArgs];` |
|         - |  7162 | `	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED` |
|         - |  7163 | `	 * arguments, and a spread expanding to !=1 element shifts the actual positions` |
|         - |  7164 | `	 * of any following compile-time named args. The effective per-actual-slot name` |
|         - |  7165 | `	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)` |
|         - |  7166 | `	 * is built PER PATH against that path's finalized arg base — a method call pops` |
|         - |  7167 | `	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch` |
|         - |  7168 | `	 * site rather than built once here. */` |
|         - |  7169 | `	VmCallArgMap sEffMap;` |
|  20553571 |  7170 | `	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;` |
|         - |  7171 | `	SyHashEntry *pEntry;` |
|         - |  7172 | `	/* What this call SITE remembered about its callee, consulted once and used by` |
|         - |  7173 | `	 * whichever of the two dispatch branches the answer belongs to. */` |
|  20553571 |  7174 | `	SyHashEntry *pSiteEntry = 0;` |
|  20553571 |  7175 | `	int bSiteHost = 0;` |
|         - |  7176 | `	SyString sName;` |
|         - |  7177 | `	/* The host-call trio, hoisted out of the foreign-function branch below so the` |
|         - |  7178 | `	 * NativeCall label there can also be reached from the compiled-function branch:` |
|         - |  7179 | `	 * a VM_FUNC_NATIVE method is a C body wearing a method's clothes, and it runs on` |
|         - |  7180 | `	 * exactly the foreign path (no frame, no call record, no operand stack) once` |
|         - |  7181 | `	 * $this and the called class have been resolved the method way. Jumping into` |
|         - |  7182 | `	 * that branch would otherwise cross these declarations. */` |
|         - |  7183 | `	ph7_user_func *pFunc;` |
|         - |  7184 | `	ph7_context sCtx;` |
|         - |  7185 | `	ph7_value sRet;` |
|         - |  7186 | `	/* Native-METHOD call state; all 0 for a plain host function, which is what the` |
|         - |  7187 | `	 * foreign branch's own fallthrough leaves.` |
|         - |  7188 | `	 *   pNativeOwned — the reference the method path took, released at the shared tail` |
|         - |  7189 | `	 *   pNativeRecv  — what the body actually sees as $this (0 for a static method)` |
|         - |  7190 | `	 *   pNativeClass — the late-static-binding target */` |
|  20553571 |  7191 | `	ph7_class_instance *pNativeOwned = 0;` |
|  20553571 |  7192 | `	ph7_class_instance *pNativeRecv = 0;` |
|  20553571 |  7193 | `	ph7_class *pNativeClass = 0;` |
|         - |  7194 | `	/*   pNativeMethod / pNativeDeclClass — the method record and the class that` |
|         - |  7195 | ``	 *   DECLARES it, for the `class`/`type` keys of the trace frame php gives an`` |
|         - |  7196 | `	 *   internal call. Left 0 by the plain-builtin fallthrough, whose frame carries` |
|         - |  7197 | `	 *   neither key. */` |
|  20553571 |  7198 | `	ph7_vm_func *pNativeMethod = 0;` |
|  20553571 |  7199 | `	ph7_class *pNativeDeclClass = 0;` |
|         - |  7200 | `	/* php's compiler REWRITES a handful of calls into dedicated opcodes, and a` |
|         - |  7201 | ``	 * refusal raised by the opcode has no internal frame to name: `strlen($a)` on an`` |
|         - |  7202 | ``	 * array reports the CALLER as frame #0 where `str_repeat($a,2)` reports`` |
|         - |  7203 | `	 * str_repeat. Which names fold is php's own list (ZEND_STRLEN, ZEND_COUNT,` |
|         - |  7204 | `	 * ZEND_ARRAY_KEY_EXISTS, ZEND_GET_CLASS); the fold needs the exact arity and a` |
|         - |  7205 | ``	 * literal callee, so a dynamic name, a spread or a `name:` argument all take the`` |
|         - |  7206 | `	 * ordinary path and DO carry the frame. */` |
|  20553571 |  7207 | `	int bFoldedCallee = 0;` |
|         - |  7208 | `	/* ...and whether php's compiler turns it into a FRAMELESS call instead (see` |
|         - |  7209 | `	 * VmNativeCall.bFrameless): it keeps its trace frame, but pushes no frame of its own. */` |
|  20553571 |  7210 | `	int bFramelessCallee = 0;` |
|         - |  7211 | `	/* The engine's own __call/__callStatic routing: the OP_MEMBER immediately below this` |
|         - |  7212 | `	 * call found a missing (or inaccessible) method on a class declaring the magic handler` |
|         - |  7213 | `	 * and MARKED this callee slot, latching {receiver, class, original name} on the VM.` |
|         - |  7214 | `	 * There is no callable here at all — the mark selects the packing body directly, ahead` |
|         - |  7215 | `	 * of every callable decode below, and the record it dispatches carries no PHP name (it` |
|         - |  7216 | `	 * is not in hHostFunction). This is what replaced writing the string` |
|         - |  7217 | `	 * "__phl_magic_call" into the slot and letting the name lookup find a hidden global.` |
|         - |  7218 | ``	 * Everything from `NativeCall` down is shared with an ordinary builtin call, which is`` |
|         - |  7219 | `	 * what this has always been from the executor's point of view. */` |
|  20553571 |  7220 | `	if( pTos->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|         - |  7221 | `		/* Move the routing off the carrier and onto the VM, HERE — one instruction` |
|         - |  7222 | `		 * before the packing body reads it, with nothing in between that could set` |
|         - |  7223 | `		 * another. OP_MEMBER used to publish it directly, which only held while the` |
|         - |  7224 | `		 * arguments ran before it; now they run after, and a routed call inside this` |
|         - |  7225 | `		 * one's argument list has already come and gone. */` |
|       166 |  7226 | `		VmMagicCall *pPend = (VmMagicCall *)pTos->x.pOther;` |
|       166 |  7227 | `		pTos->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|       166 |  7228 | `		pTos->x.pOther = 0;` |
|       166 |  7229 | `		pVm->pMagicCallThis = pPend ? pPend->pRecv : 0;` |
|       166 |  7230 | `		pVm->pMagicCallClass = pPend ? pPend->pClass : 0;` |
|       166 |  7231 | `		pVm->pMagicCallLsb = pPend ? pPend->pLsb : 0;` |
|       166 |  7232 | `		SyBlobReset(&pVm->sMagicCallName);` |
|       166 |  7233 | `		if( pPend && SyBlobLength(&pPend->sName) > 0 ){` |
|       247 |  7234 | `			SyBlobAppend(&pVm->sMagicCallName,SyBlobData(&pPend->sName),` |
|        81 |  7235 | `				SyBlobLength(&pPend->sName));` |
|        81 |  7236 | `		}` |
|       166 |  7237 | `		if( pPend ){` |
|         - |  7238 | `			/* The receiver reference the record held is now the VM's, which` |
|         - |  7239 | `			 * VmMagicCallDispatch gives back — so drop the record without unref'ing. */` |
|       166 |  7240 | `			pPend->pRecv = 0;` |
|       166 |  7241 | `			VmFreeMagicCall(pPend);` |
|        81 |  7242 | `		}` |
|       166 |  7243 | `		pFunc = PH7_VmMagicCallFunc(&(*pVm));` |
|       166 |  7244 | `		if( pFunc == 0 ){` |
|       ! 0 |  7245 | `			PH7_VmMemoryError(&(*pVm));` |
|       ! 0 |  7246 | `			goto Abort;` |
|         - |  7247 | `		}` |
|         - |  7248 | `		/* D1: the packing body declares no by-ref parameter (php hands __call a packed` |
|         - |  7249 | `		 * ARRAY), so every deferred argument materializes by value, exactly as it did` |
|         - |  7250 | `		 * through the named trampoline's zero by-ref mask. */` |
|         - |  7251 | `		{` |
|       166 |  7252 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffCallMap);` |
|       166 |  7253 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|         - |  7254 | `		}` |
|       247 |  7255 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|       162 |  7256 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|       166 |  7257 | `		goto NativeCall;` |
|         - |  7258 | `	}` |
|         - |  7259 | `	/* A Closure object is callable: unwrap it to its underlying string callable so the` |
|         - |  7260 | `	 * dispatch below handles it (rather than treating it as a generic object and looking` |
|         - |  7261 | `	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure` |
|         - |  7262 | `	 * so a plain __invoke object skips the temp-value work entirely. */` |
|  20553409 |  7263 | `	if( VmValueIsClosure(pVm,pTos) ){` |
|         - |  7264 | `		ph7_value sCallable;` |
|     43712 |  7265 | `		PH7_MemObjInit(pVm,&sCallable);` |
|     43712 |  7266 | `		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){` |
|     43712 |  7267 | `			PH7_MemObjRelease(pTos);` |
|     43712 |  7268 | `			PH7_MemObjStore(&sCallable,pTos);` |
|         - |  7269 | `` 			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]` `` |
|         - |  7270 | `			 * name, which the lookup below refuses to a name a SCRIPT spelled. */` |
|     43712 |  7271 | `			bEngineCallee = 1;` |
|         - |  7272 | `			/* ...and a call THROUGH a Closure is dynamic whatever it wraps: php marks` |
|         - |  7273 | ``			 * a fake closure's invocation too (`compact(...)` then `$c('a')`). */`` |
|     43712 |  7274 | `			bDynamicCall = 1;` |
|     43712 |  7275 | `			bViaClosure = 1;` |
|     21694 |  7276 | `		}` |
|     43712 |  7277 | `		PH7_MemObjRelease(&sCallable);` |
|     21694 |  7278 | `	}` |
|         - |  7279 | `	/* Extract function name */` |
|  20553409 |  7280 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|    304625 |  7281 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|         - |  7282 | `			ph7_value sResult;` |
|         - |  7283 | `			sxi32 rcArr;` |
|         - |  7284 | `			/* Taken off the VM at the head of the shape check below, not at the dispatch:` |
|         - |  7285 | `			 * everything between the two (the deferred-argument materialization especially)` |
|         - |  7286 | `			 * can throw and jump out of this branch, and a latch left armed would stand the` |
|         - |  7287 | `			 * visibility screen down for whatever call runs next. */` |
|         - |  7288 | `			int bCbScreened;` |
|         - |  7289 | `			ph7_class *pCbFromCls; /* ...and the class its method was resolved in, same lifetime */` |
|         - |  7290 | `			int bCbTramp;          /* ...and whether it is the __callStatic trampoline, same lifetime */` |
|         - |  7291 | `			int bCbNoNamed;        /* ...and whether it is a fromCallable() trampoline, same lifetime */` |
|         - |  7292 | `			{` |
|         - |  7293 | `				/* php validates the SHAPE of an array callable first: it must hold exactly` |
|         - |  7294 | `				 * two elements. PH7 handed any array to the dispatcher, which failed` |
|         - |  7295 | ``				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */`` |
|    100701 |  7296 | `				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;` |
|         - |  7297 | `				char zCbMsg[192];` |
|    100701 |  7298 | `				const char *zCbErr = 0;` |
|    100701 |  7299 | `				int bCbRaised = 0; /* the class lookup ran an autoloader that threw */` |
|         - |  7300 | `				/* A pair the closure UNWRAP just built is not an array the program wrote: its` |
|         - |  7301 | `				 * callee was resolved and screened where the closure was BUILT, the way php` |
|         - |  7302 | `				 * resolves one, and it is a well-formed [target, method] by construction.` |
|         - |  7303 | `				 * Re-deciding it here, against the CALLER, is what refused an escaped` |
|         - |  7304 | ``				 * `$this->priv(...)` php runs. */`` |
|    100701 |  7305 | `				bCbScreened = pVm->bClosureScreened;` |
|    100701 |  7306 | `				pVm->bClosureScreened = 0; /* put back for the one dispatch that reads it */` |
|    100701 |  7307 | `				pCbFromCls = pVm->pClosureMethodCls;` |
|    100701 |  7308 | `				pVm->pClosureMethodCls = 0;` |
|    100701 |  7309 | `				bCbTramp = pVm->bClosureStaticTramp;` |
|    100701 |  7310 | `				pVm->bClosureStaticTramp = 0;` |
|    100701 |  7311 | `				bCbNoNamed = pVm->bClosureNoNamed;` |
|    100701 |  7312 | `				pVm->bClosureNoNamed = 0;` |
|         - |  7313 | `				/* The trampoline's pair is the unwrap's too, and the direct spelling's refusal` |
|         - |  7314 | ``				 * below would re-ask the caller's `$this` and call the name non-static. */`` |
|    100701 |  7315 | `				if( !bCbScreened && !bCbTramp && pCbMap && pCbMap->nEntry == 2 ){` |
|         - |  7316 | `					/* Shape is right; now check it actually RESOLVES. The shared dispatcher` |
|         - |  7317 | `					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for` |
|         - |  7318 | `					 * an unresolvable [class,method] pair -- silence a caller cannot detect --` |
|         - |  7319 | `					 * and its contract is relied on by call_user_func/usort, so the throw` |
|         - |  7320 | `					 * belongs here at the call site. */` |
|    100251 |  7321 | `					ph7_value *pCbCls = 0;` |
|    100251 |  7322 | `					ph7_value *pCbMeth = 0;` |
|         - |  7323 | `					/* php reads the INTEGER indices 0 and 1, not the first two entries in` |
|         - |  7324 | `					 * insertion order. Two entries at other keys are a SHAPE error of their` |
|         - |  7325 | `					 * own ("has to contain indices 0 and 1"), distinct from the` |
|         - |  7326 | ``					 * wrong-element-COUNT message below; `[1=>'m',0=>'C']` holds both, so`` |
|         - |  7327 | `					 * the target is index 0 -- the reverse of what PH7 walked. */` |
|    100251 |  7328 | `					if( !PH7_VmArrayCallableParts(&(*pVm),pCbMap,&pCbCls,&pCbMeth) ){` |
|        11 |  7329 | `						zCbErr = "Array callback has to contain indices 0 and 1";` |
|         6 |  7330 | `					}else{` |
|         - |  7331 | `						/* The resolution below can run an autoloader that throws; when it does,` |
|         - |  7332 | `						 * php propagates THAT exception and never reports the class missing. */` |
|    100241 |  7333 | `						sxi32 nCbBrc = pVm->nBoundaryRc;` |
|    100241 |  7334 | `						const void *pCbRes = (const void *)pVm->pResumeFrame;` |
|    150359 |  7335 | `						zCbErr = VmDirectArrayCallableError(&(*pVm),pCbCls,pCbMeth,` |
|     50118 |  7336 | `							zCbMsg,sizeof(zCbMsg));` |
|    100241 |  7337 | `						if( zCbErr && PH7_VmClassLookupRaised(&(*pVm),nCbBrc,pCbRes) ){` |
|         6 |  7338 | `							bCbRaised = 1;` |
|         2 |  7339 | `						}` |
|         - |  7340 | `					}` |
|     50123 |  7341 | `				}` |
|    100701 |  7342 | `				if( !bCbScreened && (pCbMap == 0 \|\| pCbMap->nEntry != 2 \|\| zCbErr) ){` |
|         - |  7343 | `					sxi32 rcCb;` |
|        87 |  7344 | `					if( pInstr->iP2 & PH7_CALL_SPREAD ){` |
|       ! 0 |  7345 | `						VmSpreadConsume(pVm);` |
|       ! 0 |  7346 | `					}` |
|        87 |  7347 | `					if( nCallArgs > 0 ){` |
|       ! 0 |  7348 | `						VmPopOperand(&pTos,nCallArgs);` |
|       ! 0 |  7349 | `					}` |
|        87 |  7350 | `					PH7_MemObjRelease(pTos);` |
|        87 |  7351 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        87 |  7352 | `					pTos->nIdx = SXU32_HIGH;` |
|        87 |  7353 | `					if( bCbRaised ){` |
|         - |  7354 | `						/* Land the autoloader's own throw: consume the parked status (an` |
|         - |  7355 | `						 * in-place catch leaves 0 behind plus a recorded resume frame, which` |
|         - |  7356 | `						 * the router below picks up). */` |
|         6 |  7357 | `						rcCb = pVm->nBoundaryRc;` |
|         6 |  7358 | `						pVm->nBoundaryRc = 0;` |
|         6 |  7359 | `						if( rcCb == PH7_ABORT ){ goto Abort; }` |
|         6 |  7360 | `						rc = PH7_EXCEPTION;` |
|        18 |  7361 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  7362 | `					}` |
|        82 |  7363 | `					if( zCbErr == 0 ){` |
|        17 |  7364 | `						zCbErr = "Array callback must have exactly two elements";` |
|         8 |  7365 | `					}` |
|        82 |  7366 | `					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));` |
|        82 |  7367 | `					if( rcCb == SXERR_ABORT ){ goto Abort; }` |
|        82 |  7368 | `					rc = rcCb;` |
|         - |  7369 | `					/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|         - |  7370 | `					 * (SXRET_OK + recorded resume) FELL THROUGH and kept dispatching` |
|         - |  7371 | `					 * the malformed callable inside the try. Route like OP_THROW. */` |
|       108 |  7372 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  7373 | `				}` |
|         - |  7374 | `			}` |
|         - |  7375 | `			/* Build the effective spread-key map (and consume this call's runs)` |
|         - |  7376 | `			 * against this path's arg base (the array-callable slot isn't popped). */` |
|    150923 |  7377 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|    100612 |  7378 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|         - |  7379 | `			/* Materialize the deferred arguments against the pair's own method (see` |
|         - |  7380 | `			 * VmIndirectCalleeFunc), not against a blanket by-ref assumption. */` |
|         - |  7381 | `			{` |
|    100617 |  7382 | `				sxi32 rcDA = VmResolveIndirectArgs(&(*pVm),pTos,pArg,pTos,pEffCallMap);` |
|    100617 |  7383 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|         - |  7384 | `			}` |
|    100617 |  7385 | `			SySetReset(&aArg);` |
|    101007 |  7386 | `			while( pArg < pTos ){` |
|       393 |  7387 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       393 |  7388 | `				pArg++;` |
|         3 |  7389 | `			}` |
|    100617 |  7390 | `			PH7_MemObjInit(pVm,&sResult);` |
|         - |  7391 | `			/* May be a class instance and it's static method. Forward this call's named-arg map` |
|         - |  7392 | ``			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —`` |
|         - |  7393 | `			 * mirroring the __invoke-object branch below. */` |
|    100617 |  7394 | `			pVm->bClosureScreened = bCbScreened; /* see the capture above */` |
|    100617 |  7395 | `			pVm->pClosureMethodCls = pCbFromCls;` |
|    100617 |  7396 | `			pVm->bClosureStaticTramp = bCbTramp;` |
|    100617 |  7397 | `			pVm->bClosureNoNamed = bCbNoNamed;` |
|         - |  7398 | `			/* This call SITE is what decides the answer is dropped, and the` |
|         - |  7399 | `			 * dispatch below builds a synthetic OP_CALL that has no site of its` |
|         - |  7400 | `			 * own — hand the bit over on the latch, so a #[\NoDiscard] callee` |
|         - |  7401 | `			 * reached through an array callable, a "C::m" string or __invoke` |
|         - |  7402 | `			 * warns exactly as a directly-spelled one does. */` |
|    100617 |  7403 | `			pVm->bDiscardCallback = bResultDropped;` |
|    100617 |  7404 | `			pVm->bDirectCallable = !bViaClosure;` |
|    100617 |  7405 | `			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|         - |  7406 | `			/* Both latches are consumed by the method OP_CALL this dispatch builds;` |
|         - |  7407 | `			 * clear them here for the paths that never reach one. */` |
|    100617 |  7408 | `			pVm->bClosureScreened = 0;` |
|    100617 |  7409 | `			pVm->bClosureStaticTramp = 0;` |
|    100617 |  7410 | `			pVm->bClosureNoNamed = 0;` |
|    100617 |  7411 | `			pVm->pClosureMethodCls = 0;` |
|    100617 |  7412 | `			pVm->bDiscardCallback = 0;` |
|    100617 |  7413 | `			SySetReset(&aArg);` |
|         - |  7414 | `			/* Pop given arguments */` |
|    100617 |  7415 | `			if( nCallArgs > 0 ){` |
|       311 |  7416 | `				VmPopOperand(&pTos,nCallArgs);` |
|       154 |  7417 | `			}` |
|    100617 |  7418 | `			if( rcArr == PH7_ABORT ){` |
|       ! 0 |  7419 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 |  7420 | `				goto Abort;` |
|         - |  7421 | `			}` |
|    100617 |  7422 | `			if( rcArr == PH7_EXCEPTION ){` |
|         - |  7423 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|         - |  7424 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|         - |  7425 | `				sxi32 iResumePc;` |
|    100049 |  7426 | `				PH7_MemObjRelease(&sResult);` |
|    100049 |  7427 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|    100007 |  7428 | `					PH7_MemObjRelease(pTos);` |
|         - |  7429 | ``					/* Drain the abandoned outer-expression operands (`1 + $cb()`)`` |
|         - |  7430 | `					 * to the try's base — one leaked slot per caught throw` |
|         - |  7431 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|    300007 |  7432 | `					PH7_RESUME_DRAIN()` |
|    100007 |  7433 | `					pc = iResumePc;` |
|    100007 |  7434 | `					break;` |
|         - |  7435 | `				}` |
|        43 |  7436 | `				goto Exception;` |
|         - |  7437 | `			}` |
|         - |  7438 | `			/* Copy result */` |
|       570 |  7439 | `			PH7_MemObjStore(&sResult,pTos);` |
|       570 |  7440 | `			PH7_MemObjRelease(&sResult);` |
|    204211 |  7441 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|         - |  7442 | ``			/* An __invoke object is a METHOD CALL wearing one slot: `$o(...)` is`` |
|         - |  7443 | ``			 * `$o->__invoke(...)` with the name resolved by the ENGINE rather than`` |
|         - |  7444 | `			 * spelled by the source. So give it the layout OP_MEMBER leaves for a` |
|         - |  7445 | `			 * spelled method call -- [args..., receiver, method vm-name] -- and let` |
|         - |  7446 | `			 * the one dispatch below run it, on the stage-2 trampoline like every` |
|         - |  7447 | `			 * other PHP->PHP call.` |
|         - |  7448 | `			 *` |
|         - |  7449 | `			 * It used to be handed to VmCallObjectInvoke, which builds a synthetic` |
|         - |  7450 | `			 * OP_CALL and a fresh VmByteCodeExec: one real C activation per call,` |
|         - |  7451 | `			 * and two user-visible consequences.` |
|         - |  7452 | `			 *` |
|         - |  7453 | `			 *   - Recursion through an invokable object died at nMaxNativeDepth` |
|         - |  7454 | ``			 *     (256) where php runs to memory: `$i()` calling `($this)()` hit`` |
|         - |  7455 | `			 *     "Maximum native nesting depth reached" at 256 and php reached` |
|         - |  7456 | `			 *     20000. A plain closure, a method and a static call were all` |
|         - |  7457 | `			 *     already iterative; only this shape was not.` |
|         - |  7458 | ``			 *   - A Fiber::suspend() reached through `$o(...)` was refused with`` |
|         - |  7459 | `			 *     "Cannot suspend across an internal call boundary", because the` |
|         - |  7460 | `			 *     fiber body's nBodyExecDepth and the suspend's nVmExecDepth no` |
|         - |  7461 | `			 *     longer matched. The trampoline PARKS a nested PHP call` |
|         - |  7462 | `			 *     (pParkedSegment); a native re-entry it cannot. That is the whole` |
|         - |  7463 | `			 *     of what stopped phpstan's FiberNodeScopeResolver, whose fiber body` |
|         - |  7464 | `			 *     calls a ClassStatementsGatherer OBJECT and suspends inside it.` |
|         - |  7465 | `			 *` |
|         - |  7466 | `			 * call_user_func(), array_map() and the rest of the C-callback doors` |
|         - |  7467 | `			 * still reach __invoke through VmCallObjectInvoke, and those ARE the` |
|         - |  7468 | `			 * internal boundaries php's fibers cross and PHL's do not (a recorded` |
|         - |  7469 | `			 * generator/fiber divergence). This changes only the dispatch a call site` |
|         - |  7470 | ``			 * SPELLS as `$o(...)`. */`` |
|    203912 |  7471 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|    203912 |  7472 | `			ph7_class_method *pInvMeth = pThis` |
|    203908 |  7473 | `				? PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) : 0;` |
|    203912 |  7474 | `			if( pInvMeth ){` |
|         - |  7475 | `				/* The compiler sized this body for a ONE-slot callee here, so the` |
|         - |  7476 | `				 * name slot is one more than it budgeted. Ask for it properly rather` |
|         - |  7477 | `				 * than spend VM_STACK_GUARD's slack: the growth is a no-op whenever` |
|         - |  7478 | `				 * the slack is there (which is every ordinary call), and the slot is` |
|         - |  7479 | `				 * released a few lines below by the method branch's own pop, so at` |
|         - |  7480 | `				 * most one is ever outstanding. */` |
|    103908 |  7481 | `				ph7_value *pInvOldBase = pStack;` |
|    155860 |  7482 | `				if( !VmGrowOperandStack(pVm,(sxu32)(pTos - pStack) + 2,` |
|         - |  7483 | `				                        &pStack,&pTos,&sState,` |
|     51952 |  7484 | `				                        pCallTop,ppBaseOwner,pnBaseCap) ){` |
|       ! 0 |  7485 | `					PH7_VmMemoryError(&(*pVm));` |
|       ! 0 |  7486 | `					goto Abort;` |
|         - |  7487 | `				}` |
|    103908 |  7488 | `				if( pStack != pInvOldBase ){` |
|         - |  7489 | `					/* The buffer moved: the watermark is a pointer INTO it. Unlike` |
|         - |  7490 | `					 * OP_SPREAD (which re-anchors at the whole capacity), keep it tight —` |
|         - |  7491 | `					 * nLiveTos is what the recycle sweep walks, and this runs on every` |
|         - |  7492 | ``					 * `$o(...)`. */`` |
|      2119 |  7493 | `					pHigh = pStack + (pHigh - pInvOldBase);` |
|      1942 |  7494 | `				}` |
|    103908 |  7495 | `				pTos++;` |
|    103908 |  7496 | `				if( pTos > pHigh ){` |
|         - |  7497 | `					/* The name slot is above this activation's high-water mark until the` |
|         - |  7498 | `					 * next fetch point raises it, and an exit between here and the pop` |
|         - |  7499 | `					 * below (an unresolvable callee, the recursion cap) would leave its` |
|         - |  7500 | `					 * blob unswept. */` |
|        47 |  7501 | `					pHigh = pTos;` |
|        22 |  7502 | `				}` |
|    103908 |  7503 | `				PH7_MemObjRelease(pTos);` |
|    155860 |  7504 | `				SyBlobAppend(&pTos->sBlob,(const void *)SyStringData(&pInvMeth->sVmName),` |
|     51952 |  7505 | `					SyStringLength(&pInvMeth->sVmName));` |
|    103908 |  7506 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|         - |  7507 | `				/* The engine's own function-table key, not a name the program spelled --` |
|         - |  7508 | `				 * the mark VmCallClassMethodLsb writes on its synthetic callee slot, and` |
|         - |  7509 | `				 * what lets PH7_VmGetUserFunction answer for it. */` |
|    103908 |  7510 | `				pTos->iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|    103908 |  7511 | `				pTos->nIdx = SXU32_HIGH;` |
|    103908 |  7512 | `				pArg = &pTos[-1-nCallArgs];` |
|    103908 |  7513 | `				bEngineCallee = 1;` |
|         - |  7514 | ``				/* php dispatches a non-public __invoke through `$o()` from any scope`` |
|         - |  7515 | `				 * (it only WARNS at the declaration) -- the second half of what` |
|         - |  7516 | `				 * VmCallObjectInvoke stated, with pVm->bMagicDispatch. */` |
|    103908 |  7517 | `				bMagicDispatch = 1;` |
|    103908 |  7518 | `				goto CalleeByName;` |
|         - |  7519 | `			}` |
|         - |  7520 | `			{` |
|         - |  7521 | `				/* No __invoke: php's catchable Error, named after the class.` |
|         - |  7522 | `				 * Building the effective map is what CONSUMES this call's captured` |
|         - |  7523 | `				 * unpack runs, which a refused callee owes just as a dispatched one` |
|         - |  7524 | `				 * does — the map itself is never read from here. */` |
|    150008 |  7525 | `				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|    100004 |  7526 | `					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|         - |  7527 | `				/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable` |
|         - |  7528 | `				 * slot itself (it pops top-down and the callable IS pTos), which for a` |
|         - |  7529 | `				 * temporary like (new Plain())(...) holds the only reference — popping it` |
|         - |  7530 | `				 * would free pThis before VmRaiseNotCallable reads its class name. */` |
|    100006 |  7531 | `				if( pThis ){` |
|    100006 |  7532 | `					pThis->iRef++;` |
|     50002 |  7533 | `				}` |
|    100006 |  7534 | `				if( nCallArgs > 0 ){` |
|       ! 0 |  7535 | `					VmPopOperand(&pTos,nCallArgs);` |
|       ! 0 |  7536 | `				}` |
|    100006 |  7537 | `				PH7_MemObjRelease(pTos);` |
|    100006 |  7538 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    100006 |  7539 | `				pTos->nIdx = SXU32_HIGH;` |
|    100006 |  7540 | `				if( pThis == 0 ){` |
|         - |  7541 | `					/* An object slot with no instance behind it: nothing to name. */` |
|       ! 0 |  7542 | `					sxi32 rcNi = VmThrowFromVm(&(*pVm),"Error",` |
|         - |  7543 | `						"Value of type object is not callable",` |
|         - |  7544 | `						sizeof("Value of type object is not callable")-1);` |
|       ! 0 |  7545 | `					if( rcNi == SXERR_ABORT ){ goto Abort; }` |
|       ! 0 |  7546 | `					rc = rcNi;` |
|       ! 0 |  7547 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  7548 | `				}` |
|    100006 |  7549 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|    100006 |  7550 | `				PH7_ClassInstanceUnref(pThis);` |
|    100006 |  7551 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  7552 | `					goto Abort;` |
|         - |  7553 | `				}` |
|         - |  7554 | `				{` |
|         - |  7555 | `					sxi32 iRp;` |
|    100006 |  7556 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|         - |  7557 | `						/* Drain the abandoned outer-expression operands` |
|         - |  7558 | ``						 * (`1 + $plainObj()`) to the try's base — one leaked`` |
|         - |  7559 | `						 * slot per caught throw otherwise. */` |
|    300010 |  7560 | `						PH7_RESUME_DRAIN()` |
|    100006 |  7561 | `						pc = iRp;` |
|    100006 |  7562 | `						break;` |
|         - |  7563 | `					}` |
|         - |  7564 | `				}` |
|       ! 0 |  7565 | `				goto Exception;` |
|         - |  7566 | `			}` |
|       ! 0 |  7567 | `		}else{` |
|         - |  7568 | `			/* php: calling a non-callable is a catchable Error naming the type` |
|         - |  7569 | `			 * ("Value of type int is not callable"), or -- for an array -- the shape it` |
|         - |  7570 | `			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so` |
|         - |  7571 | ``			 * `$x()` on a number quietly evaluated to nothing. */`` |
|         - |  7572 | `			sxi32 rcNc;` |
|         - |  7573 | `			char zMsg[128];` |
|        17 |  7574 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       ! 0 |  7575 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");` |
|       ! 0 |  7576 | `			}else{` |
|        25 |  7577 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",` |
|         8 |  7578 | `					VmArithTypeName(pTos));` |
|         - |  7579 | `			}` |
|         - |  7580 | `			/* Consume this call's captured spread runs — a non-callable target` |
|         - |  7581 | `			 * (int/float/bool/null) reaches no dispatch build site. */` |
|        17 |  7582 | `			if( pInstr->iP2 & PH7_CALL_SPREAD ){` |
|       ! 0 |  7583 | `				VmSpreadConsume(pVm);` |
|       ! 0 |  7584 | `			}` |
|         - |  7585 | `			/* Pop given arguments */` |
|        17 |  7586 | `			if( nCallArgs > 0 ){` |
|       ! 0 |  7587 | `				VmPopOperand(&pTos,nCallArgs);` |
|       ! 0 |  7588 | `			}` |
|         - |  7589 | `			/* Settle the call's result slot BEFORE throwing. */` |
|        17 |  7590 | `			PH7_MemObjRelease(pTos);` |
|        17 |  7591 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        17 |  7592 | `			pTos->nIdx = SXU32_HIGH;` |
|        17 |  7593 | `			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|        17 |  7594 | `			if( rcNc == SXERR_ABORT ){ goto Abort; }` |
|        17 |  7595 | `			rc = rcNc;` |
|         - |  7596 | `			/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|         - |  7597 | `			 * (SXRET_OK + recorded resume) FELL THROUGH and resumed the try body` |
|         - |  7598 | `			 * right after the failed call. Route like OP_THROW. */` |
|        27 |  7599 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  7600 | `		}` |
|       570 |  7601 | `		break;` |
|         - |  7602 | `	}` |
|         - |  7603 | `	/* The callee is a NAME in pTos, with its arguments (and, for a method, its` |
|         - |  7604 | `	 * receiver) below it. Reached by falling through from the callable decode` |
|         - |  7605 | `	 * above, and jumped to by the __invoke-object branch, which builds exactly` |
|         - |  7606 | `	 * the two-slot method shape and lands here. */` |
|  10123000 |  7607 | `CalleeByName:` |
|  20352693 |  7608 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|         - |  7609 | `	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just` |
|         - |  7610 | `	 * anchors it to the global namespace — strip it before resolving so a` |
|         - |  7611 | ``	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */`` |
|  20352693 |  7612 | `	if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|        24 |  7613 | `		sName.zString++;` |
|        24 |  7614 | `		sName.nByte--;` |
|        11 |  7615 | `	}` |
|         - |  7616 | `	/* Ask this call SITE what its callee name meant last time. Resolving it costs up` |
|         - |  7617 | `	 * to four case-insensitive hash lookups -- the user table under the qualified name` |
|         - |  7618 | `	 * and then the global one, then the same pair of the host table -- and on a phpcs` |
|         - |  7619 | `	 * run that was 9.2% of everything the engine did. The site's answer is stamped with` |
|         - |  7620 | `	 * pVm->nCallableGen, so declaring anything retires all of them at once; see` |
|         - |  7621 | `	 * VmCallSite. */` |
|  20352693 |  7622 | `	pSiteEntry = PH7_VmCallSiteAnswer(pVm,pInstr,&sName,bEngineCallee,&bSiteHost);` |
|  20352693 |  7623 | `	if( pSiteEntry ){` |
|  16831318 |  7624 | `		pEntry = bSiteHost ? 0 : pSiteEntry;` |
|   8417939 |  7625 | `	}else` |
|         - |  7626 | `	{` |
|         - |  7627 | `	/* Check for a compiled function first.` |
|         - |  7628 | `	 * Static names are already namespace-qualified by the compiler.` |
|         - |  7629 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   3521380 |  7630 | `	pEntry = PH7_VmGetUserFunction(pVm,(const void *)sName.zString,sName.nByte,bEngineCallee);` |
|         - |  7631 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|         - |  7632 | `	 * function is not found, retry with the global name (strip the namespace` |
|         - |  7633 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|         - |  7634 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|         - |  7635 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|         - |  7636 | `	{` |
|   3521380 |  7637 | `	VmCallArgMap *pCallMap = pEffCallMap;` |
|   3521380 |  7638 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|         - |  7639 | `		const char *zFunc;` |
|         - |  7640 | `		const char *zEnd;` |
|         - |  7641 | `		const char *z;` |
|         - |  7642 | `		SyString sGlobal;` |
|       379 |  7643 | `		zFunc = sName.zString;` |
|       379 |  7644 | `		zEnd  = zFunc + sName.nByte;` |
|       379 |  7645 | `		z = zEnd;` |
|         - |  7646 | `		/* Find last namespace separator */` |
|      4077 |  7647 | `		while( z > zFunc ){` |
|      4077 |  7648 | `			if( z[-1] == '\\' ){` |
|       379 |  7649 | `				break;` |
|         - |  7650 | `			}` |
|      3703 |  7651 | `			z--;` |
|         5 |  7652 | `		}` |
|       379 |  7653 | `		if( z > zFunc && z < zEnd ){` |
|         - |  7654 | `			/* Retry lookup using the unqualified/global function name */` |
|       379 |  7655 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       379 |  7656 | `			pEntry = PH7_VmGetUserFunction(pVm,(const void *)sGlobal.zString,sGlobal.nByte,bEngineCallee);` |
|       187 |  7657 | `		}` |
|       187 |  7658 | `	}` |
|         - |  7659 | `	} /* end VmCallArgMap namespace scope */` |
|         - |  7660 | `	/* A user-table hit is the whole answer; a MISS is not (the host branch below` |
|         - |  7661 | `	 * resolves the rest), so only the hit is recorded here. */` |
|   3521380 |  7662 | `	PH7_VmCallSiteRecord(pVm,pInstr,&sName,bEngineCallee,0,pEntry);` |
|         - |  7663 | `	} /* end call-site cache miss */` |
|  20352693 |  7664 | `	if( pEntry ){` |
|         - |  7665 | `		ph7_vm_func_arg *aFormalArg;` |
|         - |  7666 | `		ph7_class_instance *pThis;` |
|         - |  7667 | `		ph7_value *pFrameStack;` |
|         - |  7668 | `		ph7_vm_func *pVmFunc;` |
|         - |  7669 | `		ph7_class *pSelf;` |
|         - |  7670 | `		ph7_class *pSelfHint;` |
|         - |  7671 | `		VmFrame *pFrame;` |
|         - |  7672 | `		ph7_value *pObj;` |
|         - |  7673 | `		VmSlot sArg;` |
|         - |  7674 | `		sxu32 n;` |
|   2513663 |  7675 | `		sxi32 iArgPreFlags = 0; /* the actual's type before its declared-type check */` |
|   2513663 |  7676 | `		int bClosureThis = 0;` |
|         - |  7677 | `		int bClosureUnbound;` |
|   2513663 |  7678 | `		ph7_class *pClosureScope = 0;` |
|         - |  7679 | `		/* initialize fields */` |
|   2513663 |  7680 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|   2513663 |  7681 | `		pThis = 0;` |
|   2513663 |  7682 | `		pSelf = 0;` |
|         - |  7683 | `		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it` |
|         - |  7684 | `		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever` |
|         - |  7685 | `		 * set for a bound plain closure, which dispatches as a function, so the method branch below` |
|         - |  7686 | `		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */` |
|   2513663 |  7687 | `		if( pVm->pClosureThis ){` |
|       142 |  7688 | `			pThis = pVm->pClosureThis;` |
|       142 |  7689 | `			pVm->pClosureThis = 0;` |
|       142 |  7690 | `			bClosureThis = 1;` |
|        69 |  7691 | `		}` |
|   2513663 |  7692 | `		if( pVm->pClosureScope ){` |
|         - |  7693 | `			/* May ride alongside a bound $this, or stand alone for a` |
|         - |  7694 | ``			 * scope-only rebind (`bindTo(null, Scope::class)`). */`` |
|       182 |  7695 | `			pClosureScope = pVm->pClosureScope;` |
|       182 |  7696 | `			pVm->pClosureScope = 0;` |
|        89 |  7697 | `		}` |
|         - |  7698 | ``		/* ...and a rebind that left it with NO `$this` rides alongside both (VmClosureUnwrap). */`` |
|   2513663 |  7699 | `		bClosureUnbound = pVm->bClosureUnbound;` |
|   2513663 |  7700 | `		pVm->bClosureUnbound = 0;` |
|   2513663 |  7701 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|         - |  7702 | `			ph7_class_method *pMeth;` |
|         - |  7703 | `			/* Class method call */` |
|   2136207 |  7704 | `			ph7_value *pTarget = &pTos[-1];` |
|   2136207 |  7705 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|         - |  7706 | `				/* Extract the 'this' pointer */` |
|   2136207 |  7707 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|         - |  7708 | `					/* Instance already loaded */` |
|   2016962 |  7709 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|   2016962 |  7710 | `					pThis->iRef++;` |
|   2016962 |  7711 | `					pSelf = pThis->pClass;` |
|   1008396 |  7712 | `				}` |
|   2136207 |  7713 | `				if( pSelf == 0 ){` |
|    119250 |  7714 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|         - |  7715 | `						/* "Late Static Binding" class name */` |
|    178832 |  7716 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     59604 |  7717 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|     59604 |  7718 | `					}` |
|    119250 |  7719 | `					if( pSelf == 0 ){` |
|        24 |  7720 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        11 |  7721 | `					}` |
|     59615 |  7722 | `				}` |
|   2136207 |  7723 | `				if( pThis == 0  ){` |
|    119250 |  7724 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|    119250 |  7725 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|    119250 |  7726 | `					if( pFrameLocal->pParent ){` |
|         - |  7727 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|     14733 |  7728 | `						pThis = pFrameLocal->pThis;` |
|     14733 |  7729 | `						if( pThis ){` |
|         - |  7730 | `` 							/* ...but only a NON-static method runs on it. `self::run()` `` |
|         - |  7731 | `							 * from an instance method reaches a static run() with no` |
|         - |  7732 | ``							 * `$this` in php: isset($this) is false there, and a`` |
|         - |  7733 | `							 * callback it builds ('parent::im') has no receiver. */` |
|       315 |  7734 | `							ph7_class *pDecl = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|       471 |  7735 | `							ph7_class_method *pCallee = PH7_ClassExtractMethod(pDecl,` |
|       156 |  7736 | `								pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|       315 |  7737 | `							if( pCallee == 0 && pDecl != pSelf ){` |
|       ! 0 |  7738 | `								pCallee = PH7_ClassExtractMethod(pSelf,` |
|       ! 0 |  7739 | `									pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|       ! 0 |  7740 | `							}` |
|       312 |  7741 | `							if( pCallee && &pCallee->sFunc == pVmFunc` |
|       315 |  7742 | `							 && (pCallee->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|       289 |  7743 | `								pThis = 0;` |
|       146 |  7744 | `							}else{` |
|        27 |  7745 | `								pThis->iRef++;` |
|         - |  7746 | `							}` |
|       156 |  7747 | `						}` |
|      7364 |  7748 | `					}` |
|     59615 |  7749 | `				}` |
|   2136207 |  7750 | `				VmPopOperand(&pTos,1);` |
|   2136207 |  7751 | `				PH7_MemObjRelease(pTos);` |
|         - |  7752 | `				/* Synchronize pointers. The method-name slot popped above sat BETWEEN` |
|         - |  7753 | `				 * this call's arguments and the (already-removed) target — so only now` |
|         - |  7754 | `				 * is pTos one past the last actual argument. Re-derive the unpack` |
|         - |  7755 | `				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs` |
|         - |  7756 | `				 * from run ends, which the extra target slot would otherwise offset,` |
|         - |  7757 | ``				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */`` |
|   3204398 |  7758 | `				nCallArgs = pInstr->iP1 + ((pInstr->iP2 & PH7_CALL_SPREAD)` |
|   1068458 |  7759 | `					? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);` |
|   2136207 |  7760 | `				pArg = &pTos[-nCallArgs];` |
|         - |  7761 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|         - |  7762 | `				 * user have already computed the random generated unique class method name` |
|         - |  7763 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|         - |  7764 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|         - |  7765 | `				 */` |
|   2136207 |  7766 | `				while( pArg < pStack ){` |
|       ! 0 |  7767 | `					pArg++;` |
|       ! 0 |  7768 | `				}` |
|   2136207 |  7769 | `				if( pSelf && pVm->bReflectBypass ){` |
|         - |  7770 | `					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection` |
|         - |  7771 | `					 * ignores visibility. Consume-once so nested calls made by the` |
|         - |  7772 | `					 * invoked body are checked normally. */` |
|      1736 |  7773 | `					pVm->bReflectBypass = 0;` |
|       869 |  7774 | `				}else` |
|   2134476 |  7775 | `				if( pSelf && bMagicDispatch && PH7_MagicMethodMustBePublic(&pVmFunc->sName) ){` |
|         - |  7776 | `					/* The ENGINE reaching for a magic method php requires to be public.` |
|         - |  7777 | `					 * php only WARNS at such a declaration and then dispatches the method` |
|         - |  7778 | ``					 * regardless -- the engine calling `__get` is not the outside world`` |
|         - |  7779 | `					 * reaching for a private member -- so a non-public` |
|         - |  7780 | ``					 * `__get`/`__call`/`__invoke`/`__sleep`/... still runs, where PHL threw`` |
|         - |  7781 | ``					 * `Call to private method C::__get()` at the ACCESS and killed the`` |
|         - |  7782 | `					 * script.` |
|         - |  7783 | `					 *` |
|         - |  7784 | `					 * The latch is set by PH7_VmCallMagicMethod at the engine's own` |
|         - |  7785 | `					 * dispatch sites. It is NOT inferred from the instruction: a` |
|         - |  7786 | ``					 * first-class `$o->__get(...)` and `$o->__get('x')` reach the same C`` |
|         - |  7787 | `					 * dispatcher, and php denies both. */` |
|     52701 |  7788 | `				}else` |
|   2029084 |  7789 | `				if( pSelf && !bMemberScreened ){ /* Paranoid edition */` |
|         - |  7790 | `					/* Check if the call is allowed. php binds non-public method` |
|         - |  7791 | `					 * access by the DECLARING class (pVmFunc->pUserData — the class` |
|         - |  7792 | `					 * the callee was compiled in), NOT the instance's class: an` |
|         - |  7793 | `					 * inherited base method calling $this->priv() on a child` |
|         - |  7794 | `					 * instance passes, a child's private SHADOW doesn't hijack the` |
|         - |  7795 | `					 * check for a parent callee, and the denial message names the` |
|         - |  7796 | `					 * declaring class like php. */` |
|   1826937 |  7797 | `					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|         - |  7798 | `					ph7_class *pOwnerClass;` |
|   1826937 |  7799 | `					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|   1826937 |  7800 | `					if( pMeth == 0 && pDeclClass != pSelf ){` |
|       ! 0 |  7801 | `						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|       ! 0 |  7802 | `					}` |
|   1826937 |  7803 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         - |  7804 | `						/* ...except that a TRAIT is not a class php still has at run time: it` |
|         - |  7805 | `						 * composed the method INTO the using class, so that class owns the` |
|         - |  7806 | `						 * rule and the name. Deciding against the trait refused a protected` |
|         - |  7807 | `						 * trait method to a SUBCLASS of the composing class (which uses no` |
|         - |  7808 | ``						 * trait of its own) — `class Az { use Tz; } class Bz extends Az {`` |
|         - |  7809 | ``						 * $this->pr(); }` was a fatal php runs. Identity for every non-trait`` |
|         - |  7810 | `						 * method. */` |
|        43 |  7811 | `						pOwnerClass = PH7_VmMethodScopeName(&(*pVm),pSelf,pMeth);` |
|        43 |  7812 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|         - |  7813 | `							/* php throws a CATCHABLE Error here. The old code merely PRINTED an` |
|         - |  7814 | ``							 * uncaught-exception report and aborted, so `try { $o->priv(); }`` |
|         - |  7815 | ``							 * catch (Error $e)` never caught it and the script died. */`` |
|         - |  7816 | `							char zMsg[256];` |
|         - |  7817 | `							sxi32 rcVis;` |
|         - |  7818 | `							/* php NAMES the calling scope when there is one — "from scope C" —` |
|         - |  7819 | `							 * and says "global scope" only outside every class; the wording is` |
|         - |  7820 | `							 * shared with the first-class-callable screen` |
|         - |  7821 | `							 * (VmMethodVisibilityMsg). */` |
|        19 |  7822 | `							VmMethodVisibilityMsg(&(*pVm),pOwnerClass,` |
|         6 |  7823 | `								pVmFunc->sName.zString,pVmFunc->sName.nByte,` |
|         6 |  7824 | `								pMeth->iProtection,zMsg,sizeof(zMsg));` |
|         - |  7825 | `							/* Consume this call's captured spread runs — this visibility` |
|         - |  7826 | `							 * error exits before the pVmFunc build below. */` |
|        13 |  7827 | `							if( pInstr->iP2 & PH7_CALL_SPREAD ){` |
|       ! 0 |  7828 | `								VmSpreadConsume(pVm);` |
|       ! 0 |  7829 | `							}` |
|         - |  7830 | `							/* Pop given arguments, and leave the call's NULL result behind. */` |
|        13 |  7831 | `							if( nCallArgs > 0 ){` |
|       ! 0 |  7832 | `								VmPopOperand(&pTos,nCallArgs);` |
|       ! 0 |  7833 | `							}` |
|        13 |  7834 | `							PH7_MemObjRelease(pTos);` |
|        13 |  7835 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|        13 |  7836 | `							pTos->nIdx = SXU32_HIGH;` |
|        13 |  7837 | `							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|        13 |  7838 | `							if( rcVis == SXERR_ABORT ){ goto Abort; }` |
|        13 |  7839 | `							rc = rcVis;` |
|         - |  7840 | `							/* ENFORCE_RC never consulted pResumeFrame, so an in-place` |
|         - |  7841 | `							 * catch (SXRET_OK + recorded resume) FELL THROUGH and` |
|         - |  7842 | `							 * dispatched the DENIED method anyway. Route like OP_THROW. */` |
|        13 |  7843 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  7844 | `						}` |
|        14 |  7845 | `					}` |
|    913424 |  7846 | `				}` |
|   1068005 |  7847 | `			}` |
|   1068005 |  7848 | `		}` |
|         - |  7849 | `		/* pArg is now finalized for every pVmFunc callee (a method call popped its` |
|         - |  7850 | `		 * method-name slot above; functions/closures/generators keep the top base).` |
|         - |  7851 | `		 * Build the PHP 8.1 effective spread-key map here so the generator and the` |
|         - |  7852 | `		 * install path below both see it — and so this call's captured runs are` |
|         - |  7853 | `		 * consumed exactly once, against the correct base. */` |
|   3770576 |  7854 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   2513646 |  7855 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|         - |  7856 | `		/* Check the PHP call-depth cap (the sole site).` |
|         - |  7857 | `		 * Default is unbounded (heap-bound recursion, decoupled from the C stack` |
|         - |  7858 | `		 * by the stage-2 trampoline); the C stack is guarded separately by` |
|         - |  7859 | `		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and` |
|         - |  7860 | `		 * then raises a clean non-catchable fatal (was: silently set NULL and` |
|         - |  7861 | `		 * continue) and halts. */` |
|   2513651 |  7862 | `		if( VmRecursionExceeded(pVm) ){` |
|         - |  7863 | `			/* Args and the function-name slot are released by the Abort label,` |
|         - |  7864 | `			 * which walks the whole operand stack — don't release them here. */` |
|         3 |  7865 | `			VmRecursionFatal(&(*pVm));` |
|         3 |  7866 | `			goto Abort;` |
|         - |  7867 | `		}` |
|   2513649 |  7868 | `		if( pVmFunc->pNextName ){` |
|         - |  7869 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|        76 |  7870 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|        36 |  7871 | `		}` |
|         - |  7872 | ``		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's`` |
|         - |  7873 | ``		 * `Class::method`). Computed after overload resolution so it reflects the selected method.`` |
|         - |  7874 | ``		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed`` |
|         - |  7875 | `		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method` |
|         - |  7876 | ``		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the`` |
|         - |  7877 | `		 * USING class; we don't carry the using class on the shared struct, so fall back to the` |
|         - |  7878 | `		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for` |
|         - |  7879 | `		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */` |
|   2513649 |  7880 | `		pSelfHint = pSelf;` |
|   2513649 |  7881 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|   2136195 |  7882 | `			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;` |
|   2136195 |  7883 | `			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|   2135713 |  7884 | `				pSelfHint = pDecl;` |
|   1067764 |  7885 | `			}` |
|   1068005 |  7886 | `		}` |
|   2513649 |  7887 | `		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|         - |  7888 | `			/* Push the closure's called-class as this frame's LSB class so` |
|         - |  7889 | ``			 * `static::` inside the body resolves like php. An explicit`` |
|         - |  7890 | `			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use` |
|         - |  7891 | `			 * the class captured at the closure's creation site. self::/parent::` |
|         - |  7892 | `			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done` |
|         - |  7893 | ``			 * after pSelfHint so a `self`-typed param is unaffected. */`` |
|     65028 |  7894 | `			if( bClosureThis && pThis ){` |
|         - |  7895 | `				/* A bound receiver IS php's called scope, whatever scope rides with it. */` |
|       142 |  7896 | `				pSelf = pThis->pClass;` |
|     64959 |  7897 | `			}else if( pClosureScope ){` |
|        43 |  7898 | `				pSelf = pClosureScope;` |
|     64870 |  7899 | `			}else if( pVmFunc->pLsbClass && bClosureUnbound != PH7_CLOSURE_UNSCOPED ){` |
|       817 |  7900 | `				pSelf = (ph7_class *)pVmFunc->pLsbClass;` |
|       406 |  7901 | `			}` |
|     32226 |  7902 | `		}` |
|   2513649 |  7903 | `		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){` |
|         - |  7904 | `			/* php 8.4 #[\Deprecated] runtime notice — once per call, before` |
|         - |  7905 | `			 * execution (generators: at the g(...) call site, like php). */` |
|       590 |  7906 | `			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,` |
|       390 |  7907 | `				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);` |
|       195 |  7908 | `		}` |
|         - |  7909 | `		/* php 8.5 #[\NoDiscard]: the CALL SITE decided this answer is thrown away` |
|         - |  7910 | ``		 * (the codegen's bDiscard, php's !RETURN_VALUE_USED), and a `(void)` cast`` |
|         - |  7911 | `		 * in front of the statement is what clears it. Raised before the body, so` |
|         - |  7912 | `		 * a callee that throws has already warned — php's order. */` |
|   2513649 |  7913 | `		if( (pVmFunc->iFlags & VM_FUNC_NODISCARD) && bResultDropped ){` |
|         - |  7914 | `			/* php calls it a "method" whenever the callee has a class SCOPE, and a` |
|         - |  7915 | ``			 * closure declared in a class body has one -- `C::{closure:C::go():3}`. */`` |
|        60 |  7916 | `			ph7_class *pNdClass = 0;` |
|        60 |  7917 | `			if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        21 |  7918 | `				pNdClass = pSelfHint;` |
|        50 |  7919 | `			}else if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|         7 |  7920 | `				pNdClass = (ph7_class *)pVmFunc->pLsbClass;` |
|         3 |  7921 | `			}` |
|        60 |  7922 | `			VmNoDiscardWarn(&(*pVm),pVmFunc,pNdClass);` |
|        29 |  7923 | `		}` |
|         - |  7924 | `		/* D1: resolve deferred plain-var arguments against this callee's declaration` |
|         - |  7925 | `		 * now — BEFORE the native/generator splits and VmEnterFrame, while pVm->pFrame is` |
|         - |  7926 | `		 * still the caller. pVmFunc is final here (post-overload). Covers plain functions,` |
|         - |  7927 | `		 * methods, closures, dynamic-name calls, generators and native methods uniformly;` |
|         - |  7928 | `		 * only the SOURCE of the by-ref positions differs between the last one and the` |
|         - |  7929 | `		 * rest (a signature string vs. compiled formal parameters). */` |
|         - |  7930 | `		{` |
|         - |  7931 | `			sxi32 rcDA;` |
|   2513649 |  7932 | `			if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|         - |  7933 | `				/* A native method declares no formal parameters to match against —` |
|         - |  7934 | `				 * its by-ref positions come from the same signature-derived mask a` |
|         - |  7935 | `				 * builtin uses, so resolve them the builtin way. Sharing this one` |
|         - |  7936 | `				 * site (rather than repeating it in the branch below) keeps the` |
|         - |  7937 | `				 * throw routing identical for both kinds of callee. */` |
|   2433776 |  7938 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|   1622574 |  7939 | `					pVmFunc->pNative->nByRefMask,0,0,pEffCallMap);` |
|    811202 |  7940 | `			}else{` |
|   1336802 |  7941 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|    891070 |  7942 | `					(ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs),SySetUsed(&pVmFunc->aArgs),` |
|    445727 |  7943 | `					0,0,0,pEffCallMap);` |
|         - |  7944 | `			}` |
|   2513657 |  7945 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|         - |  7946 | `		}` |
|   2513587 |  7947 | `		if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|         - |  7948 | `			/* C-bodied method (VM_FUNC_NATIVE): hand it to the foreign-function path.` |
|         - |  7949 | `			 *` |
|         - |  7950 | `			 * Everything a METHOD needs has already happened above — the sVmName` |
|         - |  7951 | `			 * lookup, overload selection, $this / self / late-static-binding` |
|         - |  7952 | `			 * resolution, the visibility check, the #[\Deprecated] notice, deferred` |
|         - |  7953 | `			 * argument materialization — and everything BELOW is exactly what a C` |
|         - |  7954 | `			 * body has no use for: a frame, an operand stack, a call record.` |
|         - |  7955 | `			 *` |
|         - |  7956 | `			 * The stack shape already matches a builtin's, because the method branch` |
|         - |  7957 | `			 * popped both the method-name slot and the target slot: [pArg,pTos) are` |
|         - |  7958 | `			 * this call's arguments and pTos is the slot that receives the result.` |
|         - |  7959 | `			 * So the jump lands on shared code, not a copy of it. */` |
|   1622579 |  7960 | `			pFunc = pVmFunc->pNative;` |
|   1622579 |  7961 | `			pNativeOwned = pThis; /* borrowed; the shared tail drops this reference */` |
|         - |  7962 | ``			/* A `C::m()` call reaches the method path with no object in the target`` |
|         - |  7963 | `			 * slot, and the path then adopts the CALLER's $this so an instance-context` |
|         - |  7964 | ``			 * `self::m()` still finds one. A native STATIC method must not see that —`` |
|         - |  7965 | `			 * it would read its argument list one slot off — so the receiver handed to` |
|         - |  7966 | `			 * the body is gated on the declaration, not on what the fallback found. */` |
|   1622579 |  7967 | `			pNativeRecv = (pVmFunc->iFlags & VM_FUNC_NATIVE_STATIC) ? 0 : pThis;` |
|   1622579 |  7968 | `			pNativeClass = pSelf;` |
|   1622579 |  7969 | `			pNativeMethod = pVmFunc;` |
|         - |  7970 | ``			/* php's `class` key is the DECLARING class, which oo.c parks on the`` |
|         - |  7971 | `			 * method's pUserData -- an inherited native method reports the base,` |
|         - |  7972 | `			 * not the receiver's class (SplTempFileObject->setMaxLineLen() is` |
|         - |  7973 | ``			 * `SplFileObject->setMaxLineLen` in php's trace). */`` |
|   2433956 |  7974 | `			pNativeDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData` |
|    811377 |  7975 | `				: (pThis ? pThis->pClass : pSelf);` |
|   1622579 |  7976 | `			goto NativeCall;` |
|         - |  7977 | `		}` |
|    891013 |  7978 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  7979 | `			/* Generator function: return a Generator object instead of executing */` |
|         - |  7980 | `			ph7_exec_ctx *pExecCtx;` |
|         - |  7981 | `			ph7_generator *pGenerator;` |
|         - |  7982 | `			ph7_class_instance *pGenObj;` |
|         - |  7983 | `			ph7_value *pCtxAttr;` |
|         - |  7984 | `			SyString sAttrName;` |
|         - |  7985 | `			ph7_value **apCallArgs;` |
|         - |  7986 | `			SyString *aGenArgName; /* the named extras' names, inside apCallArgs's block */` |
|         - |  7987 | `			int nGenArgs, iArg;` |
|       819 |  7988 | `			sxi32 iGenHole = -1; /* a named call's unbound required formal */` |
|         - |  7989 | `			/* Collect arguments from the operand stack */` |
|       819 |  7990 | `			nGenArgs = (int)(pTos - pArg);` |
|       819 |  7991 | `			apCallArgs = 0;` |
|       819 |  7992 | `			aGenArgName = 0;` |
|       819 |  7993 | `			if( nGenArgs > 0 ){` |
|         - |  7994 | `				/* php refuses a non-variable in a by-ref position at the CALL, and for` |
|         - |  7995 | `				 * a generator this IS the call. Routed like the branch's other` |
|         - |  7996 | `				 * pre-frame throws below: no callee frame exists yet, so drop the` |
|         - |  7997 | `				 * arguments plus the function-name slot and land the enclosing try. */` |
|       317 |  7998 | `				rc = VmScreenGenByRefArgs(&(*pVm),pVmFunc,pEffCallMap,pArg,` |
|       104 |  7999 | `					(sxu32)nGenArgs,pSelfHint);` |
|       213 |  8000 | `				if( rc != SXRET_OK ){` |
|         7 |  8001 | `					if( rc == PH7_ABORT ){` |
|       ! 0 |  8002 | `						goto Abort;` |
|         - |  8003 | `					}` |
|       393 |  8004 | `					PH7_INLINE_RESUME_BREAK()` |
|         7 |  8005 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|         - |  8006 | `					{` |
|         - |  8007 | `						sxi32 iRpB;` |
|         7 |  8008 | `						if( VmRecordedResume(pVm,&iRpB,sState.pEntryFrame,aInstr) ){` |
|       ! 0 |  8009 | `							pc = iRpB;` |
|       ! 0 |  8010 | `							break;` |
|         - |  8011 | `						}` |
|         - |  8012 | `					}` |
|        25 |  8013 | `					goto Exception;` |
|         - |  8014 | `				}` |
|       101 |  8015 | `			}` |
|       813 |  8016 | `			if( nGenArgs > 0 ){` |
|         - |  8017 | `				/* A named call binds each actual to the formal it NAMES and leaves the` |
|         - |  8018 | `				 * formals BETWEEN two named ones on their defaults, so the reordered` |
|         - |  8019 | `				 * list carries one entry per formal and can be LONGER than the number` |
|         - |  8020 | `				 * of actuals. */` |
|       207 |  8021 | `				sxu32 nSpan = (sxu32)nGenArgs + SySetUsed(&pVmFunc->aArgs);` |
|         - |  8022 | `				/* One block: the laid-out list, the actuals it is laid out from, and` |
|         - |  8023 | `				 * the names of the extras the variadic keys (see VmCtxBindNamedArgs). */` |
|       308 |  8024 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       202 |  8025 | `					(nSpan + (sxu32)nGenArgs) * sizeof(ph7_value *) + nSpan * sizeof(SyString));` |
|       207 |  8026 | `				if( apCallArgs == 0 ){` |
|         - |  8027 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|       ! 0 |  8028 | `					nGenArgs = 0;` |
|       ! 0 |  8029 | `				}else{` |
|       207 |  8030 | `					ph7_value **apActual = &apCallArgs[nSpan];` |
|       207 |  8031 | `					int didReorder = 0;` |
|       469 |  8032 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|       267 |  8033 | `						apActual[iArg] = &pArg[iArg];` |
|       136 |  8034 | `					}` |
|       207 |  8035 | `					if( pEffCallMap && pEffCallMap->bHasNamed ){` |
|        63 |  8036 | `						int nBound = 0;` |
|        63 |  8037 | `						aGenArgName = (SyString *)&apActual[nGenArgs];` |
|        92 |  8038 | `						rc = VmCtxBindNamedArgs(&(*pVm),pVmFunc,pEffCallMap,` |
|        29 |  8039 | `							(sxu32)nGenArgs,apActual,apCallArgs,aGenArgName,&nBound,&iGenHole);` |
|        63 |  8040 | `						if( rc == PH7_ABORT ){` |
|       ! 0 |  8041 | `							SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       ! 0 |  8042 | `							goto Abort;` |
|         - |  8043 | `						}` |
|        63 |  8044 | `						if( rc == PH7_EXCEPTION ){` |
|         - |  8045 | `							/* A named-argument error is php's catchable \Error (a hole's` |
|         - |  8046 | `							 * ArgumentCountError is the body's, thrown on its frame` |
|         - |  8047 | `							 * below). No callee frame exists yet on this` |
|         - |  8048 | `							 * branch (the generator body never runs and VmEnterFrame is` |
|         - |  8049 | `							 * further down), so route it like the other pre-frame` |
|         - |  8050 | `							 * OP_CALL throws: drop the args + the function-name slot and` |
|         - |  8051 | `							 * land the enclosing try. */` |
|       ! 0 |  8052 | `							SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       ! 0 |  8053 | `							PH7_INLINE_RESUME_BREAK()` |
|       ! 0 |  8054 | `							VmPopOperand(&pTos,nCallArgs + 1);` |
|         - |  8055 | `							{` |
|         - |  8056 | `								sxi32 iRpN;` |
|       ! 0 |  8057 | `								if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){` |
|       ! 0 |  8058 | `									pc = iRpN;` |
|       ! 0 |  8059 | `									break;` |
|         - |  8060 | `								}` |
|         - |  8061 | `							}` |
|       ! 0 |  8062 | `							goto Exception;` |
|         - |  8063 | `						}` |
|        63 |  8064 | `						if( rc == SXRET_OK ){` |
|        63 |  8065 | `							nGenArgs = nBound;` |
|        63 |  8066 | `							didReorder = 1;` |
|        34 |  8067 | `						}else{` |
|         - |  8068 | `							/* Out of memory: keep the positional order rather than` |
|         - |  8069 | `							 * pass an unfilled list. */` |
|       ! 0 |  8070 | `							aGenArgName = 0;` |
|         - |  8071 | `						}` |
|        29 |  8072 | `					}` |
|       207 |  8073 | `					if( !didReorder ){` |
|       307 |  8074 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|       163 |  8075 | `							apCallArgs[iArg] = apActual[iArg];` |
|        84 |  8076 | `						}` |
|        72 |  8077 | `					}` |
|         - |  8078 | `				}` |
|       101 |  8079 | `			}` |
|         - |  8080 | `			/* Create execution context and generator wrapper */` |
|       813 |  8081 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       813 |  8082 | `			if( pExecCtx == 0 ){` |
|       ! 0 |  8083 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       ! 0 |  8084 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|       ! 0 |  8085 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|       ! 0 |  8086 | `				break;` |
|         - |  8087 | `			}` |
|         - |  8088 | `			/* php's called-scope for the generator BODY: its frame is detached and` |
|         - |  8089 | `			 * created before the receiver is known, so stamp it here, where the call` |
|         - |  8090 | `			 * that made the generator still has it. A trait method's executing scope` |
|         - |  8091 | `			 * is found by walking this class (VmTraitScopeFrom), and without it a` |
|         - |  8092 | `			 * generator declared in a trait could not reach the class's protected` |
|         - |  8093 | `			 * members -- the one activation shape the frame walk cannot recover. */` |
|       813 |  8094 | `			pExecCtx->pFrame->pSelfClass = pSelf ? pSelf : (pThis ? pThis->pClass : 0);` |
|         - |  8095 | `` 			/* And the LATE-STATIC-BINDING class, which is a different question: `self::` `` |
|         - |  8096 | ``			 * is the declaring scope above, `static::` is the class the call was made`` |
|         - |  8097 | `			 * THROUGH. The body resumes long after this call returned, so pVm->aSelf no` |
|         - |  8098 | ``			 * longer holds it and `static::class` / `new static` / `static::m()` inside a`` |
|         - |  8099 | ``			 * generator answered `Class "static" not found`. VmStartCtx republishes this`` |
|         - |  8100 | `			 * for the body's duration. An instance call means the receiver's class; a` |
|         - |  8101 | `			 * static one means the class the call named. */` |
|       813 |  8102 | `			pExecCtx->pLsbClass = pThis ? pThis->pClass : (pSelf ? pSelf : 0);` |
|       813 |  8103 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       813 |  8104 | `			if( pGenerator == 0 ){` |
|       ! 0 |  8105 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|       ! 0 |  8106 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       ! 0 |  8107 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|       ! 0 |  8108 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|       ! 0 |  8109 | `				break;` |
|         - |  8110 | `			}` |
|         - |  8111 | `			/* Set up the frame with arguments, closure env, $this. A refusal raised` |
|         - |  8112 | `			 * while they bind shows this frame in its trace, called from THIS line;` |
|         - |  8113 | `			 * the detached frame had none, and printed line 1. */` |
|       813 |  8114 | `			VmStampCoroutineCallSite(pVm, pExecCtx);` |
|       813 |  8115 | `			pExecCtx->bUnboundThis = (sxu8)bClosureUnbound;` |
|       813 |  8116 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       813 |  8117 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       813 |  8118 | `			if( iGenHole >= 0 ){` |
|         - |  8119 | `				/* A named hole is refused from inside the body too, so on this frame. */` |
|        16 |  8120 | `				rc = VmCtxThrowNamedHole(pVm, pVmFunc, pSelfHint, iGenHole);` |
|         9 |  8121 | `			}else{` |
|      1593 |  8122 | `				rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs, aGenArgName,` |
|       794 |  8123 | `					pEffCallMap ? (pEffCallMap->bStrict ? 1 : 0) : (pVm->bCurStrict ? 1 : 0),` |
|       397 |  8124 | `					pSelfHint,` |
|         - |  8125 | `					TRUE/*generator: the g(...) call site is in the message*/,` |
|         - |  8126 | `					TRUE/*a source-level call binds a by-ref parameter to the caller's slot*/);` |
|         - |  8127 | `			}` |
|       813 |  8128 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       813 |  8129 | `			pExecCtx->pFrame->pParent = 0;` |
|       813 |  8130 | `			if( apCallArgs ){` |
|       207 |  8131 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       101 |  8132 | `			}` |
|       813 |  8133 | `			if( rc != SXRET_OK ){` |
|        60 |  8134 | `				VmReleaseGenerator(pVm, pGenerator);` |
|        60 |  8135 | `				if( pThis ){` |
|         9 |  8136 | `					PH7_ClassInstanceUnref(pThis);` |
|         3 |  8137 | `				}` |
|        60 |  8138 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8139 | `					goto Abort;` |
|         - |  8140 | `				}` |
|        60 |  8141 | `				if( rc == PH7_EXCEPTION ){` |
|         - |  8142 | `					/* A declared-type TypeError thrown while binding the` |
|         - |  8143 | `					 * generator's arguments (php binds + type-checks eagerly at` |
|         - |  8144 | `					 * the g(...) call site, before any resume — band A #2). If` |
|         - |  8145 | `					 * an inline try THIS exec owns caught it, land at its` |
|         - |  8146 | `					 * redirect (the drain subsumes the operand pops); else pop` |
|         - |  8147 | `					 * the args + function name and route like the other` |
|         - |  8148 | `					 * OP_CALL throw paths. */` |
|        64 |  8149 | `					PH7_INLINE_RESUME_BREAK()` |
|        58 |  8150 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|         - |  8151 | `					{` |
|         - |  8152 | `						sxi32 iRpG;` |
|        58 |  8153 | `						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){` |
|        21 |  8154 | `							pc = iRpG;` |
|        21 |  8155 | `							break;` |
|         - |  8156 | `						}` |
|         - |  8157 | `					}` |
|        40 |  8158 | `					goto Exception;` |
|         - |  8159 | `				}` |
|       ! 0 |  8160 | `				break;` |
|         - |  8161 | `			}` |
|         - |  8162 | `			/* Create Generator class instance */` |
|       757 |  8163 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       757 |  8164 | `			if( pGenObj == 0 ){` |
|       ! 0 |  8165 | `				VmReleaseGenerator(pVm, pGenerator);` |
|       ! 0 |  8166 | `				break;` |
|         - |  8167 | `			}` |
|         - |  8168 | `			/* Store generator in __ctx attribute */` |
|       757 |  8169 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       757 |  8170 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       757 |  8171 | `			if( pCtxAttr ){` |
|       757 |  8172 | `				pCtxAttr->x.pOther = pGenerator;` |
|       757 |  8173 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       376 |  8174 | `			}` |
|         - |  8175 | `			/* Pop args and function name, push Generator object. PH7_NewClassInstance` |
|         - |  8176 | `			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do` |
|         - |  8177 | `			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope` |
|         - |  8178 | ``			 * and its __destruct (which runs pending `finally` blocks and frees the`` |
|         - |  8179 | `			 * exec context) never fires. */` |
|       757 |  8180 | `			PH7_MemObjRelease(pTos);` |
|       757 |  8181 | `			pTos = &pTos[-nCallArgs];` |
|       757 |  8182 | `			pTos->x.pOther = pGenObj;` |
|       757 |  8183 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       757 |  8184 | `			if( pThis ){` |
|        31 |  8185 | `				PH7_ClassInstanceUnref(pThis);` |
|        13 |  8186 | `			}` |
|       757 |  8187 | `			break;` |
|         - |  8188 | `		}` |
|         - |  8189 | `		/* Extract the formal argument set */` |
|    890199 |  8190 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|         - |  8191 | `		/* Create a new VM frame  */` |
|    890199 |  8192 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    890199 |  8193 | `		if( rc == SXRET_OK && pFrame && (bCallbackWeak \|\| pVm->pNativeFrameName) ){` |
|         - |  8194 | `			/* An INTERNAL function reached for this callback, so the frame above it` |
|         - |  8195 | `			 * is that builtin's and not user code: php names no call site in an` |
|         - |  8196 | `			 * argument diagnostic raised here. The same latch already decides the` |
|         - |  8197 | `			 * binding mode and the too-few wording; this records it on the frame,` |
|         - |  8198 | `			 * because the type error is raised further down the argument-binding` |
|         - |  8199 | `			 * path than the latch survives. */` |
|     22971 |  8200 | `			pFrame->iFlags \|= VM_FRAME_NATIVE_CALLER;` |
|         - |  8201 | `			/* ...and WHICH builtin, because php gives it a backtrace frame of its own` |
|         - |  8202 | `			 * (the callback's carries no file/line instead). pCalleeName is the host` |
|         - |  8203 | `			 * function currently running, saved/restored around every dispatch. */` |
|     34580 |  8204 | `			pFrame->pNativeCaller = pVm->pNativeFrameName` |
|     22966 |  8205 | `				? pVm->pNativeFrameName : pVm->pCalleeName;` |
|         - |  8206 | `			/* Consume: the latch describes ONE call, and a call the callback body` |
|         - |  8207 | `			 * makes must not inherit it. */` |
|     22971 |  8208 | `			pVm->pNativeFrameName = 0;` |
|    878590 |  8209 | `		}else if( rc == SXRET_OK && pFrame && pNativeTrace ){` |
|         - |  8210 | `			/* An autoloader a builtin triggered: that builtin's trace frame, and` |
|         - |  8211 | `			 * nothing else of the callback shape (VM_FRAME_NATIVE_TRACE). */` |
|       519 |  8212 | `			pFrame->iFlags \|= VM_FRAME_NATIVE_TRACE;` |
|       519 |  8213 | `			pFrame->pNativeCaller = pNativeTrace;` |
|       257 |  8214 | `		}` |
|    890199 |  8215 | `		if( rc == SXRET_OK ){` |
|         - |  8216 | `			/* This activation now needs the function it is about to run. For a` |
|         - |  8217 | `			 * run-time closure that is a hold on its per-instantiation copy, so the` |
|         - |  8218 | `			 * copy cannot be freed under a body that is still executing when its` |
|         - |  8219 | `			 * last Closure object dies. VmLeaveFrame / VmFreeDetachedFrame give it` |
|         - |  8220 | `			 * back -- and a coroutine's DETACHED body frame keeps it for exactly as` |
|         - |  8221 | `			 * long as the frame lives, which is what a suspended generator needs. */` |
|    890199 |  8222 | `			PH7_VmClosureFuncRef(pVmFunc);` |
|    445289 |  8223 | `		}` |
|    890199 |  8224 | `		if( rc == SXRET_OK && pFrame && pSelf ){` |
|         - |  8225 | `			/* php's called-scope: the class this call was made THROUGH. Same as the` |
|         - |  8226 | `			 * receiver's for an instance call, and the named class for a static one --` |
|         - |  8227 | `			 * which is the only way to say which class composed a static TRAIT method. */` |
|    514549 |  8228 | `			pFrame->pSelfClass = pSelf;` |
|    257272 |  8229 | `		}` |
|    890199 |  8230 | `		if( rc != SXRET_OK ){` |
|         - |  8231 | `			/* Raise exception: Out of memory */` |
|       ! 0 |  8232 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|         - |  8233 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|       ! 0 |  8234 | `				&pVmFunc->sName);` |
|         - |  8235 | `			/* The frame that would own (and later release) $this never got created; for a bound` |
|         - |  8236 | `			 * plain closure the consumed transient is the object's ONLY ref, so release it here` |
|         - |  8237 | `			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */` |
|       ! 0 |  8238 | `			if( bClosureThis && pThis ){` |
|       ! 0 |  8239 | `				PH7_ClassInstanceUnref(pThis);` |
|       ! 0 |  8240 | `			}` |
|         - |  8241 | `			/* Pop given arguments */` |
|       ! 0 |  8242 | `			if( nCallArgs > 0 ){` |
|       ! 0 |  8243 | `				VmPopOperand(&pTos,nCallArgs);` |
|       ! 0 |  8244 | `			}` |
|         - |  8245 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|       ! 0 |  8246 | `			PH7_MemObjRelease(pTos);` |
|       ! 0 |  8247 | `			break;` |
|         - |  8248 | `		}` |
|    890199 |  8249 | `		if( pClosureScope ){` |
|         - |  8250 | `			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /` |
|         - |  8251 | `			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected` |
|         - |  8252 | `			 * access inside the body resolves against it. */` |
|       178 |  8253 | `			pFrame->pBoundScope = pClosureScope;` |
|    890112 |  8254 | `		}else if( bClosureUnbound == PH7_CLOSURE_UNSCOPED ){` |
|        24 |  8255 | `			pFrame->iFlags \|= VM_FRAME_UNSCOPED;` |
|        10 |  8256 | `		}` |
|         - |  8257 | `		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band` |
|         - |  8258 | `		 * A #4): sArg over-counts (defaulted params installed, variadic packed` |
|         - |  8259 | `		 * as one entry) so php's answers can't be derived from it. */` |
|    890199 |  8260 | `		pFrame->nActualArgs = (int)(pTos - pArg);` |
|   1096306 |  8261 | `		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) \|\| bClosureThis) ){` |
|         - |  8262 | `			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */` |
|         - |  8263 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|    412219 |  8264 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|    412219 |  8265 | `			if( pObj ){` |
|         - |  8266 | `				/* Reflect the change */` |
|    412219 |  8267 | `				pObj->x.pOther = pThis;` |
|    412219 |  8268 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|    206112 |  8269 | `			}` |
|    684092 |  8270 | `		}else if( (pVmFunc->iFlags & VM_FUNC_CLOSURE) && !bClosureThis && !bClosureUnbound ){` |
|         - |  8271 | `			/* A closure made in a method runs on the receiver it captured, and php has it` |
|         - |  8272 | `			 * from the start of the call: an argument refusal's trace names the frame` |
|         - |  8273 | ``			 * `C->{closure:…}()`. The rest of the captured environment is installed after`` |
|         - |  8274 | ``			 * the arguments bind, below; `$this` alone comes first, where a method's does.`` |
|         - |  8275 | `			 * Not once a rebind has dropped it: that clone shares this function, capture and` |
|         - |  8276 | `			 * all, with the closure it was made from. */` |
|     64592 |  8277 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|     85704 |  8278 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aClosureEnv) ; ++n ){` |
|     83989 |  8279 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     74271 |  8280 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName),"this",sizeof("this")-1) == 0 ){` |
|     62877 |  8281 | `					if( (aEnv[n].iFlags & VM_FUNC_ARG_IGNORE) == 0` |
|     62882 |  8282 | `					 \|\| (aEnv[n].sValue.iFlags & MEMOBJ_NULL) == 0 ){` |
|       574 |  8283 | `						pObj = VmExtractMemObj(&(*pVm),&aEnv[n].sName,FALSE,TRUE);` |
|       574 |  8284 | `						if( pObj ){` |
|       574 |  8285 | `							PH7_MemObjRelease(pObj);` |
|       574 |  8286 | `							PH7_MemObjStore(&aEnv[n].sValue,pObj);` |
|       285 |  8287 | `						}` |
|       285 |  8288 | `					}` |
|     62882 |  8289 | `					break;` |
|         - |  8290 | `				}` |
|     10482 |  8291 | `			}` |
|     32008 |  8292 | `		}` |
|    890199 |  8293 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|         - |  8294 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|         - |  8295 | `			/* Install static variables */` |
|       114 |  8296 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       240 |  8297 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|       130 |  8298 | `				pStatic = &aStatic[n];` |
|       130 |  8299 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|         - |  8300 | `					/* Initialize the static variables */` |
|        60 |  8301 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|        60 |  8302 | `					if( pObj ){` |
|         - |  8303 | `						/* Assume a NULL initialization value */` |
|        60 |  8304 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|        60 |  8305 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|         - |  8306 | `							/* Evaluate initialization expression (Any complex expression).` |
|         - |  8307 | `							 * A static's initializer is the one initializer php does NOT` |
|         - |  8308 | `							 * treat as a compile-time constant: it runs inside the call,` |
|         - |  8309 | ``							 * so `static $x = static::class;` is LATE-bound to the runtime`` |
|         - |  8310 | `							 * class. The aSelf push that PH7_VmPeekTopClass answers from` |
|         - |  8311 | `							 * happens further down (this frame is still being built), so` |
|         - |  8312 | `							 * stand it up for the eval and take it straight back --` |
|         - |  8313 | ``							 * without it `static::` found no class at all. */`` |
|        43 |  8314 | `							int bSelfPushed = 0;` |
|        43 |  8315 | `							if( pSelf ){` |
|         9 |  8316 | `								SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|         9 |  8317 | `								bSelfPushed = 1;` |
|         4 |  8318 | `							}` |
|        43 |  8319 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|        43 |  8320 | `							if( bSelfPushed ){` |
|         9 |  8321 | `								(void)SySetPop(&pVm->aSelf);` |
|         4 |  8322 | `							}` |
|        20 |  8323 | `						}` |
|        60 |  8324 | `						pObj->nIdx = pStatic->nIdx;` |
|         - |  8325 | `						/* Permanent pin: the storage outlives every call */` |
|        60 |  8326 | `						VmPinMemObjSlot(&(*pVm),pStatic->nIdx);` |
|        32 |  8327 | `					}else{` |
|       ! 0 |  8328 | `						continue;` |
|         - |  8329 | `					}` |
|        28 |  8330 | `				}` |
|         - |  8331 | `				/* Install in the current frame — a REGISTERED binding, and the slot is` |
|         - |  8332 | `				 * PINNED: the static's storage belongs to the function, not to this` |
|         - |  8333 | `				 * call, so neither the frame teardown nor an unset of the NAME may` |
|         - |  8334 | `				 * recycle it. Poking hVar directly left the binding invisible to the` |
|         - |  8335 | ``				 * reference table, so an array element sharing the static (`[&$s]`)`` |
|         - |  8336 | ``				 * did not count as a reference and `unset($s)` destroyed the storage —`` |
|         - |  8337 | `				 * the next call started over from the initializer. The pin is taken ONCE,` |
|         - |  8338 | `				 * where the slot is created (above). */` |
|       193 |  8339 | `				PH7_VmBindVarSlot(&(*pVm),pFrame,SyStringData(&pStatic->sName),` |
|        63 |  8340 | `					SyStringLength(&pStatic->sName),pStatic->nIdx);` |
|        67 |  8341 | `			}` |
|        55 |  8342 | `		}` |
|         - |  8343 | `		/* Push arguments in the local frame */` |
|         - |  8344 | `		{` |
|    890199 |  8345 | `		VmCallArgMap *pCallMap3 = pEffCallMap;` |
|         - |  8346 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|         - |  8347 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|         - |  8348 | `		/* A compiled call in a strict file always carries a map with bStrict set` |
|         - |  8349 | `		 * (GenStateAttachStrictFlag); with no map at all the call is either a` |
|         - |  8350 | `		 * compiled WEAK one — where bCurStrict is 0 for the same unit — or an` |
|         - |  8351 | `		 * ENGINE-dispatched one, whose synthetic OP_CALL has no map to carry the` |
|         - |  8352 | `		 * caller's mode. php scopes parameter coercion by the CALLING file either` |
|         - |  8353 | `		 * way, and bCurStrict is that file's mode.` |
|         - |  8354 | `		 *` |
|         - |  8355 | `		 * Unless an INTERNAL function is what reached for this callback (bCallbackWeak):` |
|         - |  8356 | `		 * php has no calling file at that boundary and binds weakly, so` |
|         - |  8357 | ``		 * `array_map('takesInt', ["5"])` from a strict file RUNS there — PHL raised a`` |
|         - |  8358 | `		 * TypeError on valid php, because the ambient bCurStrict was still the strict` |
|         - |  8359 | `		 * caller's. call_user_func / call_user_func_array are php's two forwards and` |
|         - |  8360 | `		 * carry the caller's mode on a map instead. */` |
|   1334624 |  8361 | `		int bCallIsStrict = pCallMap3 ? (pCallMap3->bStrict ? 1 : 0)` |
|    782689 |  8362 | `		                   : (bCallbackWeak ? 0 : (pVm->bCurStrict ? 1 : 0));` |
|    890199 |  8363 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|         - |  8364 | `			/* ============================================================` |
|         - |  8365 | `			 * Named-argument matching path (PHP 8.0)` |
|         - |  8366 | `			 *` |
|         - |  8367 | `			 * Resolve each actual argument to its formal parameter by name` |
|         - |  8368 | `			 * or position, then install them in the frame.` |
|         - |  8369 | `			 * ============================================================ */` |
|       737 |  8370 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       737 |  8371 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       737 |  8372 | `			sxi32 iVariadicIdx = -1;` |
|         - |  8373 | `			sxu32 nNonVariadic;` |
|         - |  8374 | `			sxi32 *aSlot;` |
|         - |  8375 | `			sxu8  *aUsed;` |
|         - |  8376 | `			sxu32 i;` |
|         - |  8377 | `			/* Find variadic parameter index */` |
|      1957 |  8378 | `			for( i = 0; i < nFormal; i++ ){` |
|      1467 |  8379 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       247 |  8380 | `					iVariadicIdx = (sxi32)i;` |
|       247 |  8381 | `					break;` |
|         - |  8382 | `				}` |
|       615 |  8383 | `			}` |
|       737 |  8384 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|         - |  8385 | `			/* Allocate mapping arrays */` |
|      1103 |  8386 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       732 |  8387 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       737 |  8388 | `			if( aSlot == 0 ){` |
|       ! 0 |  8389 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|       ! 0 |  8390 | `				goto Abort;` |
|         - |  8391 | `			}` |
|       737 |  8392 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|         - |  8393 | `			/* Resolve named arguments to formal parameters. php raises every error` |
|         - |  8394 | `			 * this can throw at the CALL SITE, before the callee is entered, so its` |
|         - |  8395 | `			 * trace has no frame for the function being called; the frame is already` |
|         - |  8396 | `			 * entered here, so the resolve runs from the caller's. */` |
|         - |  8397 | `			{` |
|       737 |  8398 | `			VmFrame *pEntered = pVm->pFrame;` |
|       737 |  8399 | `			pVm->pFrame = pEntered->pParent;` |
|      1103 |  8400 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       366 |  8401 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       737 |  8402 | `			pVm->pFrame = pEntered;` |
|         - |  8403 | `			}` |
|       737 |  8404 | `			if( rc == PH7_ABORT ){` |
|         3 |  8405 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|         3 |  8406 | `				goto Abort;` |
|         - |  8407 | `			}` |
|       735 |  8408 | `			if( rc == PH7_EXCEPTION ){` |
|         - |  8409 | `				/* php's catchable \Error for a bad named argument. The callee` |
|         - |  8410 | `				 * frame is already entered but its body must not run: unwind` |
|         - |  8411 | `				 * exactly like the named-hole ArgumentCountError path below` |
|         - |  8412 | `				 * (release the not-yet-installed actuals — Pass 2's release` |
|         - |  8413 | `				 * loop has not run — pop the callee slot, mark that there is no` |
|         - |  8414 | `				 * callee operand stack, and let VmCallFinish route the throw). */` |
|         - |  8415 | `				sxu32 iRel;` |
|        53 |  8416 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       153 |  8417 | `				for( iRel = 0; iRel < nActual; iRel++ ){` |
|       103 |  8418 | `					PH7_MemObjRelease(&pArg[iRel]);` |
|        53 |  8419 | `				}` |
|        53 |  8420 | `				PH7_MemObjRelease(pTos);` |
|        53 |  8421 | `				pTos = &pTos[-nCallArgs];` |
|        53 |  8422 | `				pFrameStack = 0;` |
|        53 |  8423 | `				goto SkipFuncBody;` |
|         - |  8424 | `			}` |
|         - |  8425 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|         - |  8426 | `			{` |
|         - |  8427 | `			/* php's required watermark for the hole check below, plus the` |
|         - |  8428 | `			 * highest formal slot an actual resolved to: php words a hole` |
|         - |  8429 | ``			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole`` |
|         - |  8430 | `			 * with nothing filled above it gets the positional count message` |
|         - |  8431 | `			 * (zend's RECV arg_num > EX(num_args) distinction). */` |
|         - |  8432 | `			sxu32 nReqNamed;` |
|         - |  8433 | `			sxu32 nNVNamed;` |
|       685 |  8434 | `			sxu32 nMaxFilled = 0;` |
|       685 |  8435 | `			nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);` |
|      2139 |  8436 | `			for( i = 0; i < nActual; i++ ){` |
|      1459 |  8437 | `				if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|       703 |  8438 | `					nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|       349 |  8439 | `				}` |
|       732 |  8440 | `			}` |
|         - |  8441 | `			/* php's call arity runs up to the highest formal a name binds, and` |
|         - |  8442 | `			 * a hole below it -- filled by its default -- is an argument there:` |
|         - |  8443 | ``			 * `f(b: 3)` on f($a = 1, $b = 2) is func_num_args() 2 and`` |
|         - |  8444 | `			 * func_get_args() [1, 3]. The stamp counted operands only. */` |
|      1703 |  8445 | `			for( n = 0; n < nMaxFilled; n++ ){` |
|      1023 |  8446 | `				if( !aUsed[n] ){` |
|       213 |  8447 | `					pFrame->nActualArgs++;` |
|       104 |  8448 | `				}` |
|       514 |  8449 | `			}` |
|       685 |  8450 | `			if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|         - |  8451 | `				/* An INTERNAL callee's variadic does not collect a name it has no` |
|         - |  8452 | `				 * parameter for -- not even its own variadic's name -- unless it is` |
|         - |  8453 | `				 * one of php's forwards: its parameter parsing refuses the extra, at` |
|         - |  8454 | `				 * the moment PH7_VmBuiltinExtraNamedRule records (the host-function` |
|         - |  8455 | `				 * door's rule). Every builtin-chunk variadic is refused after its` |
|         - |  8456 | `				 * arity screen and before its type screens, so a REFUSE row would` |
|         - |  8457 | `				 * need the refusal moved past the loop below. */` |
|        46 |  8458 | `				int bExtraNamed = 0;` |
|         - |  8459 | `				int iRule;` |
|        96 |  8460 | `				for( i = 0; i < nActual && i < pCallMap3->nTotal; i++ ){` |
|        74 |  8461 | `					if( aSlot[i] == -1 && pCallMap3->aNames[i].nByte > 0 ){` |
|        23 |  8462 | `						bExtraNamed = 1;` |
|        23 |  8463 | `						break;` |
|         - |  8464 | `					}` |
|        27 |  8465 | `				}` |
|        46 |  8466 | `				iRule = bExtraNamed ? PH7_VmBuiltinExtraNamedRule(&pVmFunc->sName) : VM_XNAMED_TAKE;` |
|        46 |  8467 | `				if( iRule != VM_XNAMED_TAKE ){` |
|        23 |  8468 | `					int bArityMet = 1;` |
|        43 |  8469 | `					for( n = 0; n < nReqNamed && n < nNonVariadic; n++ ){` |
|        23 |  8470 | `						if( !aUsed[n] ){` |
|         3 |  8471 | `							bArityMet = 0;` |
|         3 |  8472 | `							break;` |
|         - |  8473 | `						}` |
|        11 |  8474 | `					}` |
|        23 |  8475 | `					if( bArityMet \|\| iRule == VM_XNAMED_BEFORE_ARITY ){` |
|        21 |  8476 | `						rc = VmThrowBuiltinExtraNamed(&(*pVm),pSelfHint,&pVmFunc->sName);` |
|        21 |  8477 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        65 |  8478 | `						for( i = 0; i < nActual; i++ ){` |
|        45 |  8479 | `							PH7_MemObjRelease(&pArg[i]);` |
|        23 |  8480 | `						}` |
|        21 |  8481 | `						if( rc == PH7_ABORT ){` |
|       ! 0 |  8482 | `							goto Abort;` |
|         - |  8483 | `						}` |
|        21 |  8484 | `						PH7_MemObjRelease(pTos);` |
|        21 |  8485 | `						pTos = &pTos[-nCallArgs];` |
|        21 |  8486 | `						pFrameStack = 0;` |
|        21 |  8487 | `						rc = PH7_EXCEPTION;` |
|        38 |  8488 | `						goto SkipFuncBody;` |
|         - |  8489 | `					}` |
|         1 |  8490 | `				}` |
|        12 |  8491 | `			}` |
|      1719 |  8492 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|         - |  8493 | `				/* Find the stack arg mapped to formal n */` |
|      1093 |  8494 | `				sxi32 iSrc = -1;` |
|      1761 |  8495 | `				for( i = 0; i < nActual; i++ ){` |
|      1447 |  8496 | `					if( aSlot[i] == (sxi32)n ){` |
|       779 |  8497 | `						iSrc = (sxi32)i;` |
|       779 |  8498 | `						break;` |
|         - |  8499 | `					}` |
|       339 |  8500 | `				}` |
|      1093 |  8501 | `				if( iSrc >= 0 ){` |
|         - |  8502 | `					/* Argument was provided — install with type checking */` |
|       779 |  8503 | `					ph7_value *pVal = &pArg[iSrc];` |
|         - |  8504 | `					/* An explicit null is NOT redirected to the default: PHP applies a` |
|         - |  8505 | `					 * default only for an OMITTED argument. An explicit null falls through` |
|         - |  8506 | `					 * to the type check below (TypeError for a non-nullable typed param,` |
|         - |  8507 | ``					 * kept as null for a typeless one). An implicitly-nullable `Type $x =`` |
|         - |  8508 | ``					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */`` |
|         - |  8509 | `					/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|         - |  8510 | `					 * coercion and whole-real materialization in place): the shared` |
|         - |  8511 | `					 * per-argument helper — one implementation for both OP_CALL` |
|         - |  8512 | `					 * paths and the generator/fiber binder (a recorded fold). */` |
|       779 |  8513 | `					iArgPreFlags = pVal->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|       779 |  8514 | `					rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pVal,bCallIsStrict,pSelfHint);` |
|       779 |  8515 | `					if( rc != SXRET_OK ){` |
|        10 |  8516 | `						if( rc == PH7_ABORT ) goto Abort;` |
|        10 |  8517 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        10 |  8518 | `						PH7_MemObjRelease(pTos);` |
|        10 |  8519 | `						pTos = &pTos[-nCallArgs];` |
|        10 |  8520 | `						pFrameStack = 0;` |
|        10 |  8521 | `						rc = PH7_EXCEPTION;` |
|        10 |  8522 | `						goto SkipFuncBody;` |
|         - |  8523 | `					}` |
|         - |  8524 | `					/* Install: by reference or by value */` |
|       771 |  8525 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        45 |  8526 | `						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){` |
|         - |  8527 | `							/* php 8.1: $GLOBALS cannot be passed by reference */` |
|         - |  8528 | `							SyBlob sMsg;` |
|       ! 0 |  8529 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       ! 0 |  8530 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|       ! 0 |  8531 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|       ! 0 |  8532 | `							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|       ! 0 |  8533 | `							if( rc == PH7_ABORT ){` |
|       ! 0 |  8534 | `								goto Abort;` |
|         - |  8535 | `							}` |
|       ! 0 |  8536 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       ! 0 |  8537 | `							PH7_MemObjRelease(pTos);` |
|       ! 0 |  8538 | `							pTos = &pTos[-nCallArgs];` |
|       ! 0 |  8539 | `							pFrameStack = 0;` |
|       ! 0 |  8540 | `							rc = PH7_EXCEPTION;` |
|       ! 0 |  8541 | `							goto SkipFuncBody;` |
|         - |  8542 | `						}` |
|        45 |  8543 | `						if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)iSrc,pVal) ){` |
|         - |  8544 | `							/* php refuses a by-ref argument whose EXPRESSION is not a variable, at` |
|         - |  8545 | `							 * the call and before the callee runs. Deciding it from the VALUE that` |
|         - |  8546 | `							 * arrived was wrong both ways: an operator result carries its LEFT` |
|         - |  8547 | ``							 * operand's slot, so `f($i + 1)` aliased and overwrote `$i`; and a`` |
|         - |  8548 | `							 * literal and a CALL result look alike there, where php accepts the` |
|         - |  8549 | `							 * call. VmArgRefusedByRef reads the call site's compile-time shape mask` |
|         - |  8550 | `							 * and falls back to the old runtime test only when there is none. */` |
|         - |  8551 | `							sxi32 rcRef;` |
|         3 |  8552 | `							rcRef = VmThrowByRefRefusal(&(*pVm),` |
|         2 |  8553 | `								(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|         2 |  8554 | `								&pVmFunc->sName,pVmFunc,(sxu32)(n+1),&aFormalArg[n].sName);` |
|         3 |  8555 | `							if( rcRef == PH7_ABORT ){` |
|       ! 0 |  8556 | `								goto Abort;` |
|         - |  8557 | `							}` |
|         - |  8558 | `							/* Same teardown as the type-check refusal above: free the slot map,` |
|         - |  8559 | `							 * release the result slot and pop the actuals, then let SkipFuncBody` |
|         - |  8560 | `							 * route the throw. */` |
|         3 |  8561 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|         3 |  8562 | `							PH7_MemObjRelease(pTos);` |
|         3 |  8563 | `							pTos = &pTos[-nCallArgs];` |
|         3 |  8564 | `							pFrameStack = 0;` |
|         3 |  8565 | `							rc = PH7_EXCEPTION;` |
|         3 |  8566 | `							goto SkipFuncBody;` |
|         - |  8567 | `						}` |
|        43 |  8568 | `						PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)iSrc,pVal);` |
|        43 |  8569 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|         8 |  8570 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|         5 |  8571 | `						}else{` |
|         - |  8572 | `							SyHashEntry *pRefEntry;` |
|         - |  8573 | `							/* The declared type's conversion is what the reference holds. */` |
|        37 |  8574 | `							PH7_VmByRefArgWriteBack(&(*pVm),pVal,iArgPreFlags);` |
|        54 |  8575 | `							pRefEntry = SyHashGet(&pFrame->hVar,` |
|        34 |  8576 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        37 |  8577 | `							if( pRefEntry == 0 ){` |
|        54 |  8578 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        34 |  8579 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        37 |  8580 | `								sArg.nIdx = pVal->nIdx;` |
|        37 |  8581 | `								sArg.pUserData = 0;` |
|        37 |  8582 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        17 |  8583 | `							}` |
|        37 |  8584 | `							pObj = 0;` |
|         - |  8585 | `						}` |
|        23 |  8586 | `					}else{` |
|       729 |  8587 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|         - |  8588 | `					}` |
|       769 |  8589 | `					if( pObj ){` |
|       735 |  8590 | `						PH7_MemObjStore(pVal,pObj);` |
|       735 |  8591 | `						sArg.nIdx = pObj->nIdx;` |
|       735 |  8592 | `						sArg.pUserData = 0;` |
|       735 |  8593 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       365 |  8594 | `					}` |
|       387 |  8595 | `				}else{` |
|         - |  8596 | `					/* Argument was NOT provided — use default or leave unset */` |
|       319 |  8597 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         - |  8598 | `						/* Should not reach here; variadic handled separately below */` |
|       319 |  8599 | `					}else if( n < nReqNamed ){` |
|         - |  8600 | `						/* php's implicit-required rule applies to named calls` |
|         - |  8601 | `						 * too: a hole below the required watermark throws even` |
|         - |  8602 | `						 * when the formal carries a default — f($a,$b=2,$c)` |
|         - |  8603 | ``						 * called as f(a:1,c:3) is `Argument #2 ($b) not`` |
|         - |  8604 | ``						 * passed` (a hole with NO default is always below the`` |
|         - |  8605 | `						 * watermark, so this subsumes the no-default case).` |
|         - |  8606 | `						 * A hole with nothing filled ABOVE it uses php's` |
|         - |  8607 | `						 * positional count wording instead. The passed stack` |
|         - |  8608 | `						 * args were not released yet on this path (that loop` |
|         - |  8609 | `						 * runs after Pass 2) — release them before the exit. */` |
|        25 |  8610 | `						if( n + 1 > nMaxFilled ){` |
|         7 |  8611 | `							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)` |
|         3 |  8612 | `								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|         1 |  8613 | `									nMaxFilled,nReqNamed,SySetUsed(&pVmFunc->aArgs))` |
|         7 |  8614 | `								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,pVmFunc,` |
|         4 |  8615 | `									nMaxFilled,nReqNamed,nNVNamed,(pFrame->iFlags & VM_FRAME_NATIVE_CALLER) == 0);` |
|         4 |  8616 | `						}else{` |
|        19 |  8617 | `							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,pVmFunc,n+1,&aFormalArg[n].sName);` |
|         - |  8618 | `						}` |
|        25 |  8619 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        51 |  8620 | `						for( i = 0; i < nActual; i++ ){` |
|        29 |  8621 | `							PH7_MemObjRelease(&pArg[i]);` |
|        16 |  8622 | `						}` |
|        25 |  8623 | `						if( rc == PH7_ABORT ){` |
|       ! 0 |  8624 | `							goto Abort;` |
|         - |  8625 | `						}` |
|        25 |  8626 | `						PH7_MemObjRelease(pTos);` |
|        25 |  8627 | `						pTos = &pTos[-nCallArgs];` |
|        25 |  8628 | `						pFrameStack = 0;` |
|        25 |  8629 | `						rc = PH7_EXCEPTION;` |
|        25 |  8630 | `						goto SkipFuncBody;` |
|       297 |  8631 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       297 |  8632 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       297 |  8633 | `						if( pObj ){` |
|         - |  8634 | `							VmDefaultScope sDefScope;` |
|       297 |  8635 | `							VmDefaultScopeEnter(&(*pVm),pFrame,pVmFunc,pSelf,&sDefScope);` |
|       297 |  8636 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|       297 |  8637 | `							VmDefaultScopeLeave(&(*pVm),&sDefScope);` |
|       297 |  8638 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       297 |  8639 | `							sArg.nIdx = pObj->nIdx;` |
|       297 |  8640 | `							sArg.pUserData = 0;` |
|       297 |  8641 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|         - |  8642 | `							/* The default is held to the type like a passed argument (see` |
|         - |  8643 | `							 * the positional-path note below). */` |
|       297 |  8644 | `							rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pObj,bCallIsStrict,pSelfHint);` |
|       297 |  8645 | `							if( rc != SXRET_OK ){` |
|         3 |  8646 | `								if( rc == PH7_ABORT ) goto Abort;` |
|         3 |  8647 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|         5 |  8648 | `								for( i = 0; i < nActual; i++ ){` |
|         3 |  8649 | `									PH7_MemObjRelease(&pArg[i]);` |
|         2 |  8650 | `								}` |
|         3 |  8651 | `								PH7_MemObjRelease(pTos);` |
|         3 |  8652 | `								pTos = &pTos[-nCallArgs];` |
|         3 |  8653 | `								pFrameStack = 0;` |
|         3 |  8654 | `								rc = PH7_EXCEPTION;` |
|         3 |  8655 | `								goto SkipFuncBody;` |
|         - |  8656 | `							}` |
|       145 |  8657 | `						}` |
|       145 |  8658 | `					}` |
|         - |  8659 | `				}` |
|       532 |  8660 | `			}` |
|         - |  8661 | `			} /* end nReqNamed scope */` |
|         - |  8662 | `			/* Handle variadic parameter */` |
|       631 |  8663 | `			if( iVariadicIdx >= 0 ){` |
|       197 |  8664 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|       197 |  8665 | `				if( pObj ){` |
|         - |  8666 | `					/* Capture the slot index now: PH7_HashmapInsert below can` |
|         - |  8667 | `					 * PH7_ReserveMemObj, which used to reallocate pVm->aMemObj and` |
|         - |  8668 | `					 * dangle pObj. Redundant since P1 -- the pool's segments are` |
|         - |  8669 | `					 * fixed, so a slot's address never moves. Left for the harvest` |
|         - |  8670 | `					 * sweep. */` |
|         - |  8671 | `					sxu32 nVariadicSlot;` |
|       197 |  8672 | `					PH7_MemObjToHashmap(pObj);` |
|       197 |  8673 | `					nVariadicSlot = pObj->nIdx;` |
|         - |  8674 | `					{` |
|       197 |  8675 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|         - |  8676 | `						/* php numbers a failing NAMED variadic element as` |
|         - |  8677 | `						 * max(total positional args, declared non-variadic` |
|         - |  8678 | `						 * formals) + 1 — zend's RECV slots always count —` |
|         - |  8679 | `						 * whichever named element fails; a POSITIONAL element` |
|         - |  8680 | `						 * uses its own 1-based call position. */` |
|       197 |  8681 | `						sxu32 nPositional = 0;` |
|       917 |  8682 | `						for( i = 0; i < nActual; i++ ){` |
|       725 |  8683 | `							if( !(i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0) ){` |
|       399 |  8684 | `								nPositional++;` |
|       197 |  8685 | `							}` |
|       365 |  8686 | `						}` |
|         - |  8687 | `						/* php's variadic holds every positional element before any named` |
|         - |  8688 | `						 * one -- a later unpack's positional follows an earlier unpack's` |
|         - |  8689 | `						 * name -- so the names are a second pass. A positional element is` |
|         - |  8690 | `						 * numbered where it BOUND: after the highest formal a name filled. */` |
|       197 |  8691 | `						sxu32 iPass, nArgPos = 0, nNamedHigh = 0, nElem;` |
|       535 |  8692 | `						for( iPass = 0; iPass < 2; iPass++ )` |
|      1761 |  8693 | `						for( i = 0; i < nActual; i++ ){` |
|      1423 |  8694 | `							int bNamed = (i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0);` |
|      1423 |  8695 | `							if( iPass == 0 ){` |
|       721 |  8696 | `								if( bNamed ){` |
|       327 |  8697 | `									if( aSlot[i] >= 0 && (sxu32)aSlot[i] + 1 > nNamedHigh ){` |
|        52 |  8698 | `										nNamedHigh = (sxu32)aSlot[i] + 1;` |
|        24 |  8699 | `									}` |
|       327 |  8700 | `									continue;` |
|         - |  8701 | `								}` |
|       399 |  8702 | `								if( nArgPos < nNamedHigh ){` |
|       ! 0 |  8703 | `									nArgPos = nNamedHigh;` |
|       ! 0 |  8704 | `								}` |
|       399 |  8705 | `								nArgPos++;` |
|       904 |  8706 | `							}else if( !bNamed ){` |
|       389 |  8707 | `								continue;` |
|         - |  8708 | `							}` |
|       717 |  8709 | `							nElem = bNamed ? SXMAX(nPositional,nNonVariadic) + 1 : nArgPos;` |
|       717 |  8710 | `							if( aSlot[i] == -1 ){` |
|       605 |  8711 | `								int bRefElem = 0; /* alias this entry to the caller's slot? */` |
|         - |  8712 | `								/* Same per-element type check + weak coercion as the` |
|         - |  8713 | ``								 * positional-only path (shared helper; no `($name)`). */`` |
|       905 |  8714 | `								rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|       600 |  8715 | `									&aFormalArg[iVariadicIdx],&pArg[i],` |
|       300 |  8716 | `									nElem,bCallIsStrict);` |
|       605 |  8717 | `								if( rc != SXRET_OK ){` |
|        41 |  8718 | `									if( rc == PH7_ABORT ){` |
|       ! 0 |  8719 | `										goto Abort;` |
|         - |  8720 | `									}` |
|        41 |  8721 | `									SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        41 |  8722 | `									PH7_MemObjRelease(pTos);` |
|        41 |  8723 | `									pTos = &pTos[-nCallArgs];` |
|        41 |  8724 | `									pFrameStack = 0;` |
|        41 |  8725 | `									rc = PH7_EXCEPTION;` |
|        41 |  8726 | `									goto SkipFuncBody;` |
|         - |  8727 | `								}` |
|       567 |  8728 | `								if( aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|         - |  8729 | `									/* php screens a by-ref VARIADIC tail per collected element, in its` |
|         - |  8730 | `									 * no-name wording: a variadic has no per-element parameter name, so` |
|         - |  8731 | ``									 * php says `Argument #N could not be passed by reference` and stops`` |
|         - |  8732 | `									 * there. Nothing screened this arm at all — the branch that collects` |
|         - |  8733 | `									 * a variadic runs before the by-ref binder ever sees a formal. */` |
|         8 |  8734 | `									if( PH7_VmArgRefusedByRef(pCallMap3,i,&pArg[i]) ){` |
|         - |  8735 | `										SyBlob sMsgV;` |
|         - |  8736 | `										sxi32 rcV;` |
|         3 |  8737 | `										SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|         3 |  8738 | `										SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|         1 |  8739 | `											&pVmFunc->sName,(unsigned)nElem);` |
|         3 |  8740 | `										rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|         3 |  8741 | `										if( rcV == PH7_ABORT ){` |
|       ! 0 |  8742 | `											goto Abort;` |
|         - |  8743 | `										}` |
|         3 |  8744 | `										SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|         3 |  8745 | `										PH7_MemObjRelease(pTos);` |
|         3 |  8746 | `										pTos = &pTos[-nCallArgs];` |
|         3 |  8747 | `										pFrameStack = 0;` |
|         3 |  8748 | `										rc = PH7_EXCEPTION;` |
|         3 |  8749 | `										goto SkipFuncBody;` |
|         - |  8750 | `									}` |
|         5 |  8751 | `									PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,i,&pArg[i]);` |
|         2 |  8752 | `								}` |
|         - |  8753 | `								/* A by-ref variadic tail ALIASES its actuals, named entries` |
|         - |  8754 | `								 * included (the positional twin below this branch says why). */` |
|       847 |  8755 | `								bRefElem = (aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF)` |
|       560 |  8756 | `									&& pArg[i].nIdx != SXU32_HIGH;` |
|       565 |  8757 | `								if( bNamed ){` |
|         - |  8758 | `									/* Named variadic entry: insert with string key */` |
|         - |  8759 | `									ph7_value sKey;` |
|       233 |  8760 | `									PH7_MemObjInit(pVm, &sKey);` |
|       233 |  8761 | `									PH7_MemObjStringAppend(&sKey,` |
|       228 |  8762 | `										pCallMap3->aNames[i].zString,` |
|       228 |  8763 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       233 |  8764 | `									if( bRefElem ){` |
|         5 |  8765 | `										PH7_HashmapInsertByRef(pVarMap, &sKey, pArg[i].nIdx);` |
|         3 |  8766 | `									}else{` |
|       229 |  8767 | `										PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|         - |  8768 | `									}` |
|       233 |  8769 | `									PH7_MemObjRelease(&sKey);` |
|       449 |  8770 | `								}else if( bRefElem ){` |
|         - |  8771 | `									/* Positional variadic entry, aliased */` |
|       ! 0 |  8772 | `									PH7_HashmapInsertByRef(pVarMap, 0, pArg[i].nIdx);` |
|       ! 0 |  8773 | `								}else{` |
|         - |  8774 | `									/* Positional variadic entry */` |
|       335 |  8775 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|         - |  8776 | `								}` |
|       280 |  8777 | `							}` |
|       510 |  8778 | `						}` |
|         - |  8779 | `					}` |
|       157 |  8780 | `					sArg.nIdx = nVariadicSlot; /* the saved index (see above; redundant since P1) */` |
|       157 |  8781 | `					sArg.pUserData = 0;` |
|       157 |  8782 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        76 |  8783 | `				}` |
|        81 |  8784 | `			}else{` |
|         - |  8785 | `				/* No variadic — preserve unresolved positional overflow` |
|         - |  8786 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|         - |  8787 | `				 * func_get_args() / func_num_args() still see them, matching` |
|         - |  8788 | `				 * the positional-only path's behavior. */` |
|       439 |  8789 | `				sxu32 nAnon = nNonVariadic;` |
|      1089 |  8790 | `				for( i = 0; i < nActual; i++ ){` |
|       655 |  8791 | `					if( aSlot[i] == -2 ){` |
|         - |  8792 | `						char zAnonBuf[32];` |
|         - |  8793 | `						SyString sAnonName;` |
|        13 |  8794 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|         4 |  8795 | `							"[%u]apArg",nAnon);` |
|         9 |  8796 | `						sAnonName.zString = zAnonBuf;` |
|         9 |  8797 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|         9 |  8798 | `						if( pObj ){` |
|         9 |  8799 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|         9 |  8800 | `							sArg.nIdx = pObj->nIdx;` |
|         9 |  8801 | `							sArg.pUserData = 0;` |
|         9 |  8802 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|         4 |  8803 | `						}` |
|         9 |  8804 | `						nAnon++;` |
|         4 |  8805 | `					}` |
|       330 |  8806 | `				}` |
|         - |  8807 | `			}` |
|         - |  8808 | `			/* Release all stack arguments */` |
|      1881 |  8809 | `			for( i = 0; i < nActual; i++ ){` |
|      1295 |  8810 | `				PH7_MemObjRelease(&pArg[i]);` |
|       650 |  8811 | `			}` |
|       591 |  8812 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|         - |  8813 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       591 |  8814 | `			n = nFormal;` |
|       298 |  8815 | `		}else{` |
|         - |  8816 | `		/* ============================================================` |
|         - |  8817 | `		 * Positional-only matching path (original)` |
|         - |  8818 | `		 * ============================================================ */` |
|         - |  8819 | `		/* Base of the actual argument stack, for php's per-ELEMENT argument` |
|         - |  8820 | `		 * number in a variadic type-error (php numbers a variadic-collected` |
|         - |  8821 | `		 * element by its overall 1-based call position, not the formal index). */` |
|    889467 |  8822 | `		ph7_value *pArgBase = pArg;` |
|    889467 |  8823 | `		n = 0;` |
|   1350484 |  8824 | `		while( pArg < pTos ){` |
|    466397 |  8825 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|         - |  8826 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       880 |  8827 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       880 |  8828 | `				if( pObj ){` |
|         - |  8829 | `					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,` |
|         - |  8830 | `					 * which used to reallocate pVm->aMemObj and dangle pObj (a real UAF, masked by` |
|         - |  8831 | `					 * the pool allocator). Redundant since P1 -- the pool's segments are fixed, so` |
|         - |  8832 | `					 * a slot's address never moves. Left for the harvest sweep. */` |
|         - |  8833 | `					sxu32 nVariadicIdx;` |
|         - |  8834 | `					/* Initialize as empty array */` |
|       880 |  8835 | `					PH7_MemObjToHashmap(pObj);` |
|       880 |  8836 | `					nVariadicIdx = pObj->nIdx;` |
|         - |  8837 | `					{` |
|       880 |  8838 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      3743 |  8839 | `						while( pArg < pTos ){` |
|         - |  8840 | `							/* Per-element type check + weak coercion (shared helper,` |
|         - |  8841 | `							 * also used by the named-argument path). The argument` |
|         - |  8842 | `							 * number is the element's overall 1-based call position` |
|         - |  8843 | `` 							 * ((pArg - pArgBase) + 1) and, like php, the `($name)` `` |
|         - |  8844 | `							 * clause is omitted. */` |
|      4262 |  8845 | `							rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|      2913 |  8846 | `								&aFormalArg[n],pArg,(sxu32)(pArg - pArgBase) + 1,bCallIsStrict);` |
|      2918 |  8847 | `							if( rc != SXRET_OK ){` |
|        49 |  8848 | `								if( rc == PH7_ABORT ){` |
|       ! 0 |  8849 | `									goto Abort;` |
|         - |  8850 | `								}` |
|         - |  8851 | `								/* Skip function body, route through normal cleanup */` |
|        49 |  8852 | `								PH7_MemObjRelease(pTos);` |
|        49 |  8853 | `								pTos = &pTos[-nCallArgs];` |
|        49 |  8854 | `								pFrameStack = 0;` |
|        49 |  8855 | `								rc = PH7_EXCEPTION;` |
|        49 |  8856 | `								goto SkipFuncBody;` |
|         - |  8857 | `							}` |
|      2874 |  8858 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|         - |  8859 | `								/* The positional twin of the named path's variadic screen above:` |
|         - |  8860 | `								 * php refuses a non-variable collected into a by-ref variadic tail,` |
|         - |  8861 | `								 * in its no-name wording. */` |
|        61 |  8862 | `								sxu32 nPosV = (sxu32)(pArg - pArgBase);` |
|        61 |  8863 | `								if( PH7_VmArgRefusedByRef(pCallMap3,nPosV,pArg) ){` |
|         - |  8864 | `									SyBlob sMsgV;` |
|         - |  8865 | `									sxi32 rcV;` |
|         7 |  8866 | `									SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|         7 |  8867 | `									SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|         6 |  8868 | `										&pVmFunc->sName,(unsigned)(nPosV + 1));` |
|         7 |  8869 | `									rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|         7 |  8870 | `									if( rcV == PH7_ABORT ){` |
|       ! 0 |  8871 | `										goto Abort;` |
|         - |  8872 | `									}` |
|         7 |  8873 | `									PH7_MemObjRelease(pTos);` |
|         7 |  8874 | `									pTos = &pTos[-nCallArgs];` |
|         7 |  8875 | `									pFrameStack = 0;` |
|         7 |  8876 | `									rc = PH7_EXCEPTION;` |
|         7 |  8877 | `									goto SkipFuncBody;` |
|         - |  8878 | `								}` |
|        55 |  8879 | `								PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,nPosV,pArg);` |
|        55 |  8880 | `								if( pArg->nIdx != SXU32_HIGH ){` |
|         - |  8881 | `									/* php ALIASES each collected element to the caller's slot:` |
|         - |  8882 | ``									 * `function f(&...$xs){ $xs[0] = 'A'; }` writes back, and`` |
|         - |  8883 | ``									 * var_dump($xs) inside the callee shows `&int(1)`. Copying`` |
|         - |  8884 | `									 * them left every actual untouched. The node counts as a` |
|         - |  8885 | `									 * holder of the caller's slot, so the frame teardown that` |
|         - |  8886 | `									 * destroys the variadic array gives the hold back. */` |
|        42 |  8887 | `									PH7_HashmapInsertByRef(pMap, 0, pArg->nIdx);` |
|        42 |  8888 | `									pArg++;` |
|        42 |  8889 | `									continue;` |
|         - |  8890 | `								}` |
|         6 |  8891 | `							}` |
|      2828 |  8892 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      2828 |  8893 | `							pArg++;` |
|         5 |  8894 | `						}` |
|         - |  8895 | `					}` |
|       830 |  8896 | `					sArg.nIdx = nVariadicIdx; /* the saved index (see above; redundant since P1) */` |
|       830 |  8897 | `					sArg.pUserData = 0;` |
|       830 |  8898 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       365 |  8899 | `				}` |
|       830 |  8900 | `				break; /* All remaining args consumed */` |
|         - |  8901 | `			}` |
|    465522 |  8902 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|         - |  8903 | `				/* An explicit null is NOT redirected to the default (PHP applies a` |
|         - |  8904 | `				 * default only for an omitted arg); it falls through to the type check` |
|         - |  8905 | `				 * below — TypeError for a non-nullable typed param, kept as null for a` |
|         - |  8906 | ``				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE`` |
|         - |  8907 | `				 * at compile time so its check accepts null. */` |
|         - |  8908 | `				/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|         - |  8909 | `				 * coercion and whole-real materialization in place — nullable` |
|         - |  8910 | `				 * types (?type) let null through): the shared per-argument` |
|         - |  8911 | `				 * helper, one implementation for both OP_CALL paths and the` |
|         - |  8912 | `				 * generator/fiber binder (a recorded fold). */` |
|    448916 |  8913 | `				iArgPreFlags = pArg->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|    448916 |  8914 | `				rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pArg,bCallIsStrict,pSelfHint);` |
|    448916 |  8915 | `				if( rc != SXRET_OK ){` |
|       481 |  8916 | `					if( rc == PH7_ABORT ){` |
|         6 |  8917 | `						goto Abort;` |
|         - |  8918 | `					}` |
|         - |  8919 | `					/* Skip function body, route through normal cleanup */` |
|       477 |  8920 | `					PH7_MemObjRelease(pTos);` |
|       477 |  8921 | `					pTos = &pTos[-nCallArgs];` |
|       477 |  8922 | `					pFrameStack = 0;` |
|       477 |  8923 | `					rc = PH7_EXCEPTION;` |
|       477 |  8924 | `					goto SkipFuncBody;` |
|         - |  8925 | `				}` |
|    448440 |  8926 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|         - |  8927 | `					/* Pass by reference */` |
|      8493 |  8928 | `					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){` |
|         - |  8929 | `						/* php 8.1: $GLOBALS cannot be passed by reference —` |
|         - |  8930 | `						 * a catchable Error with php's exact wording. */` |
|         - |  8931 | `						SyBlob sMsg;` |
|         3 |  8932 | `						SyBlobInit(&sMsg,&pVm->sAllocator);` |
|         3 |  8933 | `						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|         2 |  8934 | `							&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|         3 |  8935 | `						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|         3 |  8936 | `						if( rc == PH7_ABORT ){` |
|       ! 0 |  8937 | `							goto Abort;` |
|         - |  8938 | `						}` |
|         3 |  8939 | `						PH7_MemObjRelease(pTos);` |
|         3 |  8940 | `						pTos = &pTos[-nCallArgs];` |
|         3 |  8941 | `						pFrameStack = 0;` |
|         3 |  8942 | `						rc = PH7_EXCEPTION;` |
|         3 |  8943 | `						goto SkipFuncBody;` |
|         - |  8944 | `					}` |
|      8491 |  8945 | `					if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)n,pArg) ){` |
|         - |  8946 | `						/* php's refusal, decided from the argument's compile-time SHAPE (the` |
|         - |  8947 | `						 * companion of the named-argument binder above; see VmArgRefusedByRef). */` |
|         - |  8948 | `						sxi32 rcRef;` |
|      6036 |  8949 | `						rcRef = VmThrowByRefRefusal(&(*pVm),` |
|      4022 |  8950 | `							(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|      4022 |  8951 | `							&pVmFunc->sName,pVmFunc,(sxu32)(n+1),&aFormalArg[n].sName);` |
|      4025 |  8952 | `						if( rcRef == PH7_ABORT ){` |
|       ! 0 |  8953 | `							goto Abort;` |
|         - |  8954 | `						}` |
|         - |  8955 | `						/* Route the throw like every other binder refusal: release the result` |
|         - |  8956 | `						 * slot, pop the actuals and let SkipFuncBody finish the call. Returning` |
|         - |  8957 | `						 * from here walked out of the dispatch loop with the callee's frame and` |
|         - |  8958 | `						 * stack still live, so a CAUGHT refusal silently abandoned every` |
|         - |  8959 | `						 * statement after the catch. */` |
|      4025 |  8960 | `						PH7_MemObjRelease(pTos);` |
|      4025 |  8961 | `						pTos = &pTos[-nCallArgs];` |
|      4025 |  8962 | `						pFrameStack = 0;` |
|      4025 |  8963 | `						rc = PH7_EXCEPTION;` |
|      4025 |  8964 | `						goto SkipFuncBody;` |
|         - |  8965 | `					}` |
|      4469 |  8966 | `					PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)n,pArg);` |
|      4469 |  8967 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|         - |  8968 | `						/* Nothing to alias: pass by value. */` |
|       150 |  8969 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        77 |  8970 | `					}else{` |
|         - |  8971 | `						SyHashEntry *pRefEntry;` |
|         - |  8972 | `						/* The declared type's conversion is what the reference holds. */` |
|      4323 |  8973 | `						PH7_VmByRefArgWriteBack(&(*pVm),pArg,iArgPreFlags);` |
|         - |  8974 | `						/* Install the referenced variable in the private function frame */` |
|      4323 |  8975 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|      4323 |  8976 | `						if( pRefEntry == 0 ){` |
|      6478 |  8977 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|      4318 |  8978 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|      4323 |  8979 | `							sArg.nIdx = pArg->nIdx;` |
|      4323 |  8980 | `							sArg.pUserData = 0;` |
|      4323 |  8981 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      2155 |  8982 | `						}` |
|      4323 |  8983 | `						pObj = 0;` |
|         - |  8984 | `					}` |
|      2233 |  8985 | `				}else{` |
|         - |  8986 | `					/* Pass by value,make a copy of the given argument */` |
|    439952 |  8987 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|         - |  8988 | `				}` |
|    223030 |  8989 | `			}else{` |
|         - |  8990 | `				char zName[32];` |
|         - |  8991 | `				SyString sArgName;` |
|         - |  8992 | `				/* Set a dummy name */` |
|     16611 |  8993 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|     16611 |  8994 | `				sArgName.zString = zName;` |
|         - |  8995 | `				/* Annonymous argument */` |
|     16611 |  8996 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|         - |  8997 | `			}` |
|    461022 |  8998 | `			if( pObj ){` |
|    456704 |  8999 | `				PH7_MemObjStore(pArg,pObj);` |
|         - |  9000 | `				/* Insert argument index  */` |
|    456704 |  9001 | `				sArg.nIdx = pObj->nIdx;` |
|    456704 |  9002 | `				sArg.pUserData = 0;` |
|    456704 |  9003 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    229130 |  9004 | `			}` |
|    461022 |  9005 | `			PH7_MemObjRelease(pArg);` |
|    461022 |  9006 | `			pArg++;` |
|    461022 |  9007 | `			++n;` |
|         5 |  9008 | `		}` |
|         - |  9009 | `		} /* end named vs positional branch */` |
|         - |  9010 | `		/* Set up closure environment */` |
|    885503 |  9011 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|         - |  9012 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|         - |  9013 | `			ph7_value *pValue;` |
|         - |  9014 | `			sxu32 iEnv;` |
|     64664 |  9015 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|    148717 |  9016 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|     84058 |  9017 | `				pEnv = &aEnv[iEnv];` |
|     84058 |  9018 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|         - |  9019 | `					/* Do not install null value */` |
|     62342 |  9020 | `					continue;` |
|         - |  9021 | `				}` |
|     21716 |  9022 | `				if( SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|     11998 |  9023 | `				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|         - |  9024 | `					/* Installed with the frame, above -- or, when the Closure instance` |
|         - |  9025 | `					 * carries an explicit bound $this (bindTo/bind/call), that one wins` |
|         - |  9026 | `					 * over the creation-time captured $this, php-exact. */` |
|       606 |  9027 | `					continue;` |
|         - |  9028 | `				}` |
|     21119 |  9029 | `				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|         - |  9030 | `					/* Captured by reference: link the name to the shared slot` |
|         - |  9031 | `					 * (no copy), mirroring the by-ref argument install above. */` |
|      4889 |  9032 | `					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|      7315 |  9033 | `						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),` |
|      4884 |  9034 | `							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|      2426 |  9035 | `					}` |
|      4889 |  9036 | `					continue;` |
|         - |  9037 | `				}` |
|     16235 |  9038 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|     16235 |  9039 | `				if( pValue == 0 ){` |
|       ! 0 |  9040 | `					continue;` |
|         - |  9041 | `				}` |
|         - |  9042 | `				/* Invalidate any prior representation */` |
|     16235 |  9043 | `				PH7_MemObjRelease(pValue);` |
|         - |  9044 | `				/* Duplicate bound variable value */` |
|     16235 |  9045 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|      8057 |  9046 | `			}` |
|     32044 |  9047 | `		}` |
|         - |  9048 | `		/* Too-few-arguments check, placed AFTER the passed arguments were` |
|         - |  9049 | `		 * installed and type-checked: php's RECV order means a type error on` |
|         - |  9050 | ``		 * a PASSED argument beats the count error (`f(int $x,$y)` called`` |
|         - |  9051 | `		 * f("str") is a TypeError, not ArgumentCountError). The passed args` |
|         - |  9052 | `		 * were already released by the install loop, so the standard throw` |
|         - |  9053 | `		 * exit leaks nothing. The named path never fires this (its per-hole` |
|         - |  9054 | `		 * check ran in-loop; n == nNonVariadic >= nRequired here).` |
|         - |  9055 | `		 * Builtins written as embedded PHP in the prelude (VM_FUNC_INTERNAL,` |
|         - |  9056 | `		 * with or without VM_FUNC_CLASS_METHOD) are NOT exempt: their declared` |
|         - |  9057 | `		 * signatures were swept against the php 8.5 oracle, so the derived` |
|         - |  9058 | `		 * required count and wording are php's, and VmThrowBuiltinTooFewArgs` |
|         - |  9059 | `		 * words them as php words an internal callable. */` |
|    885503 |  9060 | `		if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|         - |  9061 | `			sxu32 nNonVar,nReq;` |
|      8461 |  9062 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|      8461 |  9063 | `			if( n < nReq ){` |
|       107 |  9064 | `				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;` |
|       107 |  9065 | `				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|        37 |  9066 | `					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        12 |  9067 | `						nPassed,nReq,SySetUsed(&pVmFunc->aArgs));` |
|        13 |  9068 | `				}else{` |
|         - |  9069 | `					/* php names the call SITE only when the caller is user code: an` |
|         - |  9070 | `					 * INTERNAL function reaching for a callback (array_map, usort,` |
|         - |  9071 | `					 * a forward php did not fold) has no calling line to name, which` |
|         - |  9072 | `					 * is exactly what the frame's native-caller mark records. */` |
|       122 |  9073 | `					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,pVmFunc,` |
|        78 |  9074 | `						nPassed,nReq,nNonVar,(pFrame->iFlags & VM_FRAME_NATIVE_CALLER) == 0);` |
|         - |  9075 | `				}` |
|       107 |  9076 | `				if( rc == PH7_ABORT ){` |
|       ! 0 |  9077 | `					goto Abort;` |
|         - |  9078 | `				}` |
|       107 |  9079 | `				PH7_MemObjRelease(pTos);` |
|       107 |  9080 | `				pTos = &pTos[-nCallArgs];` |
|       107 |  9081 | `				pFrameStack = 0;` |
|       107 |  9082 | `				rc = PH7_EXCEPTION;` |
|       107 |  9083 | `				goto SkipFuncBody;` |
|         5 |  9084 | `			}` |
|    881167 |  9085 | `		}else if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) == VM_FUNC_INTERNAL ){` |
|         - |  9086 | `			/* Too-MANY arguments, the mirror php enforces for an internal` |
|         - |  9087 | `			 * callable ("count_chars() expects at most 2 arguments, 3 given").` |
|         - |  9088 | `			 * PHL accepted the extras silently — the surplus only ever reached` |
|         - |  9089 | `			 * func_get_args(). Every formal is filled on this branch, so the call` |
|         - |  9090 | `			 * is over the maximum exactly when more actuals arrived than there are` |
|         - |  9091 | `			 * non-variadic formals; a VARIADIC tail means php declares no maximum` |
|         - |  9092 | `			 * at all, so nNonVar below the formal count is exempt. Prelude` |
|         - |  9093 | `			 * FUNCTIONS only: the chunk-backed class METHODS were not swept against` |
|         - |  9094 | `			 * php's signatures (Fiber::start() is declared argless and reads` |
|         - |  9095 | `			 * func_get_args()). */` |
|         - |  9096 | `			sxu32 nNonVar,nReq;` |
|      2759 |  9097 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|      2754 |  9098 | `			if( nNonVar == SySetUsed(&pVmFunc->aArgs)` |
|      2752 |  9099 | `			 && pFrame->nActualArgs >= 0` |
|      2755 |  9100 | `			 && (sxu32)pFrame->nActualArgs > nNonVar ){` |
|        34 |  9101 | `				rc = VmThrowBuiltinTooManyArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        22 |  9102 | `					(sxu32)pFrame->nActualArgs,nNonVar,nReq);` |
|        23 |  9103 | `				if( rc == PH7_ABORT ){` |
|       ! 0 |  9104 | `					goto Abort;` |
|         - |  9105 | `				}` |
|        23 |  9106 | `				PH7_MemObjRelease(pTos);` |
|        23 |  9107 | `				pTos = &pTos[-nCallArgs];` |
|        23 |  9108 | `				pFrameStack = 0;` |
|        23 |  9109 | `				rc = PH7_EXCEPTION;` |
|        23 |  9110 | `				goto SkipFuncBody;` |
|         - |  9111 | `			}` |
|      1365 |  9112 | `		}` |
|         - |  9113 | `		/* Process default values for remaining formal parameters */` |
|    903958 |  9114 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     19613 |  9115 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         - |  9116 | `				/* Variadic parameter with no extra args — create empty array */` |
|      1018 |  9117 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      1018 |  9118 | `				if( pObj ){` |
|      1018 |  9119 | `					PH7_MemObjToHashmap(pObj);` |
|      1018 |  9120 | `					sArg.nIdx = pObj->nIdx;` |
|      1018 |  9121 | `					sArg.pUserData = 0;` |
|      1018 |  9122 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       459 |  9123 | `				}` |
|      1018 |  9124 | `				n++;` |
|      1018 |  9125 | `				break; /* Variadic is always last */` |
|         - |  9126 | `			}` |
|     18600 |  9127 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     18600 |  9128 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     18600 |  9129 | `				if( pObj ){` |
|         - |  9130 | `					/* Evaluate the default value and extract it's result */` |
|         - |  9131 | `					VmDefaultScope sDefScope;` |
|     18600 |  9132 | `					VmDefaultScopeEnter(&(*pVm),pFrame,pVmFunc,pSelf,&sDefScope);` |
|     18600 |  9133 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|     18600 |  9134 | `					VmDefaultScopeLeave(&(*pVm),&sDefScope);` |
|     18600 |  9135 | `					if( rc == PH7_ABORT ){` |
|       ! 0 |  9136 | `						goto Abort;` |
|         - |  9137 | `					}` |
|         - |  9138 | `					/* Insert argument index */` |
|     18600 |  9139 | `					sArg.nIdx = pObj->nIdx;` |
|     18600 |  9140 | `					sArg.pUserData = 0;` |
|     18600 |  9141 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|         - |  9142 | `					/* php's RECV_INIT holds the default to the parameter's type` |
|         - |  9143 | `					 * exactly as it holds a passed argument, in the CALLER's mode:` |
|         - |  9144 | ``					 * one the compiler could not fold (`int $x = C`, `= new X`) and`` |
|         - |  9145 | ``					 * the type refuses is the ordinary TypeError, `called in` the`` |
|         - |  9146 | `					 * call's line. A blind cast here turned "a" into 0 silently and` |
|         - |  9147 | `					 * an object into a conversion warning. An implicitly-nullable` |
|         - |  9148 | ``					 * `int $x = null` carries VM_FUNC_ARG_NULLABLE, so null stays. */`` |
|     18600 |  9149 | `					rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pObj,bCallIsStrict,pSelfHint);` |
|     18600 |  9150 | `					if( rc != SXRET_OK ){` |
|        18 |  9151 | `						if( rc == PH7_ABORT ){` |
|       ! 0 |  9152 | `							goto Abort;` |
|         - |  9153 | `						}` |
|        18 |  9154 | `						PH7_MemObjRelease(pTos);` |
|        18 |  9155 | `						pTos = &pTos[-nCallArgs];` |
|        18 |  9156 | `						pFrameStack = 0;` |
|        18 |  9157 | `						rc = PH7_EXCEPTION;` |
|        18 |  9158 | `						goto SkipFuncBody;` |
|         - |  9159 | `					}` |
|      9275 |  9160 | `				}` |
|      9275 |  9161 | `			}` |
|     18584 |  9162 | `			++n;` |
|         5 |  9163 | `		}` |
|         - |  9164 | `		} /* end VmCallArgMap scope */` |
|         - |  9165 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|         - |  9166 | `		 * does not return anything.` |
|         - |  9167 | `		 */` |
|    885363 |  9168 | `		PH7_MemObjRelease(pTos);` |
|    885363 |  9169 | `		pTos = &pTos[-nCallArgs];` |
|         - |  9170 | `		/* Allocate an operand stack (via the recycling allocator) and evaluate the` |
|         - |  9171 | `		 * function body. Size it to a tight static bound when the body is statically` |
|         - |  9172 | `		 * modelable — the big memory win for deep recursion,` |
|         - |  9173 | `		 * where one such stack lives per frame — falling back to the safe` |
|         - |  9174 | `		 * instruction-count bound otherwise.` |
|         - |  9175 | `		 *` |
|         - |  9176 | `		 * The bound is computed LAZILY on the first call and cached on the func` |
|         - |  9177 | `		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:` |
|         - |  9178 | `		 * self-computing on first use is fail-safe against any body-creation path` |
|         - |  9179 | `		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),` |
|         - |  9180 | `		 * where undersizing is a heap overflow; the amortized cost is one analysis` |
|         - |  9181 | `		 * per function. */` |
|         - |  9182 | `		{` |
|    885363 |  9183 | `			sxu32 nSlots = pVmFunc->nMaxStack;` |
|    885363 |  9184 | `			if( nSlots == 0 ){` |
|     29316 |  9185 | `				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);` |
|     43843 |  9186 | `				sxu32 nTight = VmComputeMaxStack(&(*pVm),` |
|     29311 |  9187 | `					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);` |
|     29316 |  9188 | `				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;` |
|     29316 |  9189 | `				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */` |
|     29316 |  9190 | `				pVmFunc->nMaxStack = nSlots;` |
|     14527 |  9191 | `			}` |
|    885363 |  9192 | `			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);` |
|         - |  9193 | `		}` |
|    885363 |  9194 | `		if( pFrameStack == 0 ){` |
|         - |  9195 | `			/* Raise exception: Out of memory */` |
|       ! 0 |  9196 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|       ! 0 |  9197 | `				&pVmFunc->sName);` |
|       ! 0 |  9198 | `			if( nCallArgs > 0 ){` |
|       ! 0 |  9199 | `				VmPopOperand(&pTos,nCallArgs);` |
|       ! 0 |  9200 | `			}` |
|       ! 0 |  9201 | `			break;` |
|         - |  9202 | `		}` |
|    442487 |  9203 | `SkipFuncBody:` |
|    890193 |  9204 | `		if( pSelf ){` |
|         - |  9205 | `			/* Push class name */` |
|    514549 |  9206 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|    257272 |  9207 | `		}` |
|         - |  9208 | `		/* Increment nesting level */` |
|    890193 |  9209 | `		pVm->nRecursionDepth++;` |
|    890193 |  9210 | `		if( rc == PH7_EXCEPTION ){` |
|         - |  9211 | `			/* Arg-binding threw: there is no body to run — finish the call` |
|         - |  9212 | `			 * immediately (no record is pushed). */` |
|         - |  9213 | `			VmCallRecord sCallee;` |
|      4835 |  9214 | `			sCallee.pVmFunc = pVmFunc;` |
|      4835 |  9215 | `			sCallee.pFrame = pFrame;` |
|      4835 |  9216 | `			sCallee.pFrameStack = pFrameStack;` |
|      4835 |  9217 | `			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|         - |  9218 | `			/* The body never ran, so this stack is untouched -- but the path is rare` |
|         - |  9219 | `			 * (an argument's own evaluation threw) and sweeping the whole capacity` |
|         - |  9220 | `			 * costs nothing here, so do that rather than reason about the binder. */` |
|      4835 |  9221 | `			sCallee.nLiveTos = sCallee.nStackCap;` |
|      4835 |  9222 | `			sCallee.nLastRef = SXU32_HIGH;` |
|      4835 |  9223 | `			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|      4835 |  9224 | `			sState.pTos = pTos;` |
|      4835 |  9225 | `			sState.pc = pc;` |
|      4835 |  9226 | `			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);` |
|      4835 |  9227 | `			pTos = sState.pTos;` |
|      4835 |  9228 | `			pc = sState.pc;` |
|      4835 |  9229 | `			if( rc == PH7_ABORT ){` |
|         - |  9230 | `				/* Abort processing immeditaley */` |
|       ! 0 |  9231 | `				goto Abort;` |
|      4835 |  9232 | `			}else if( rc == PH7_SUSPEND ){` |
|       ! 0 |  9233 | `				goto Suspend;` |
|      4835 |  9234 | `			}else if( rc == PH7_EXCEPTION ){` |
|       485 |  9235 | `				goto Exception;` |
|         - |  9236 | `			}` |
|      2180 |  9237 | `		}else{` |
|         - |  9238 | `			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a` |
|         - |  9239 | `			 * call record (caller activation + in-flight call) and switch the` |
|         - |  9240 | `			 * loop's locals to the callee — a PHP->PHP call no longer grows` |
|         - |  9241 | `			 * the native stack. The record node is pool-allocated so` |
|         - |  9242 | `			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */` |
|    885363 |  9243 | `			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;` |
|    885363 |  9244 | `			if( pRec ){` |
|    880089 |  9245 | `				pVm->pIdleCallFrames = (void *)pRec->pPrev;` |
|    440249 |  9246 | `			}else{` |
|      5279 |  9247 | `				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));` |
|         - |  9248 | `			}` |
|    885363 |  9249 | `			if( pRec == 0 ){` |
|         - |  9250 | `				/* OOM: undo the push-time accounting, tear the call down and` |
|         - |  9251 | `				 * raise the non-catchable fatal (the OOM convention —` |
|         - |  9252 | `				 * never a silent NULL). */` |
|       ! 0 |  9253 | `				pVm->nRecursionDepth--;` |
|       ! 0 |  9254 | `				if( pSelf ){` |
|       ! 0 |  9255 | `					(void)SySetPop(&pVm->aSelf);` |
|       ! 0 |  9256 | `				}` |
|       ! 0 |  9257 | `				SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|       ! 0 |  9258 | `				VmLeaveFrame(&(*pVm));` |
|       ! 0 |  9259 | `				PH7_VmMemoryError(&(*pVm));` |
|       ! 0 |  9260 | `				goto Abort;` |
|         - |  9261 | `			}` |
|    885363 |  9262 | `			sState.pTos = pTos;` |
|    885363 |  9263 | `			sState.pc = pc;` |
|    885363 |  9264 | `			sState.pHigh = pHigh;   /* the caller's watermark rides with its pTos */` |
|    885363 |  9265 | `			pRec->sCaller = sState;` |
|    885363 |  9266 | `			pRec->sCall.pVmFunc = pVmFunc;` |
|    885363 |  9267 | `			pRec->sCall.pFrame = pFrame;` |
|    885363 |  9268 | `			pRec->sCall.pFrameStack = pFrameStack;` |
|    885363 |  9269 | `			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|         - |  9270 | `			/* Overwritten with the callee's real watermark when the call finishes;` |
|         - |  9271 | `			 * the safe default is "sweep everything", so a path that ever reaches` |
|         - |  9272 | `			 * VmCallFinish without going through the unwind above still cleans the` |
|         - |  9273 | `			 * whole buffer rather than parking a live value in the pool. */` |
|    885363 |  9274 | `			pRec->sCall.nLiveTos = pRec->sCall.nStackCap;` |
|    885363 |  9275 | `			pRec->sCall.nLastRef = SXU32_HIGH;` |
|    885363 |  9276 | `			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|    885363 |  9277 | `			pRec->pPrev = pCallTop;` |
|    885363 |  9278 | `			pCallTop = pRec;` |
|         - |  9279 | `			/* Switch to the callee activation (what the recursive` |
|         - |  9280 | `			 * VmByteCodeExec entry used to set up). */` |
|    885363 |  9281 | `			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);` |
|    885363 |  9282 | `			pStack = pFrameStack;` |
|    885363 |  9283 | `			pTos = &pStack[-1];` |
|    885363 |  9284 | `			pc = 0;` |
|    885363 |  9285 | `			sState.aInstr = aInstr;` |
|    885363 |  9286 | `			sState.pStack = pStack;` |
|    885363 |  9287 | `			pHigh = pTos;             /* the callee starts with an empty stack */` |
|    885363 |  9288 | `			sState.pHigh = pHigh;` |
|    885363 |  9289 | `			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */` |
|    885363 |  9290 | `			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */` |
|    885363 |  9291 | `			sState.pTos = pTos;` |
|    885363 |  9292 | `			sState.pc = 0;` |
|    885363 |  9293 | `			sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|    885363 |  9294 | `			sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|    885363 |  9295 | `			sState.pEntryFrame = pVm->pFrame;` |
|    885363 |  9296 | `			sState.pResult = pRec->sCaller.pTos;` |
|    885363 |  9297 | `			sState.pLastRef = &pRec->sCall.nLastRef;` |
|         - |  9298 | `			/* Carries the callee for BOTH terminal-OP_DONE screens: the declared` |
|         - |  9299 | `			 * return type (which re-tests VmFuncHasReturnType) and the by-reference` |
|         - |  9300 | `			 * return above. */` |
|    885363 |  9301 | `			sState.pEnforceRetFunc = ( VmFuncHasReturnType(pVmFunc)` |
|    885358 |  9302 | `				\|\| (pVmFunc->iFlags & VM_FUNC_REF_RETURN) ) ? pVmFunc : 0;` |
|    885363 |  9303 | `			sState.is_callback = 0;` |
|    885363 |  9304 | `			sState.bReturnPropagates = 0;` |
|    885363 |  9305 | `			goto VmLoopFetch;` |
|         - |  9306 | `		}` |
|      2180 |  9307 | `	}else{` |
|         - |  9308 | `		/* Look for an installed foreign function.` |
|         - |  9309 | `		 * Host functions are registered with short names (strlen, etc.).` |
|         - |  9310 | `		 * If the compiler namespace-qualified the name, extract the short` |
|         - |  9311 | `		 * name (last component after \) and try that. This implements PHP's` |
|         - |  9312 | `		 * global fallback for unqualified function calls in namespaces. */` |
|  17839035 |  9313 | `		if( pSiteEntry && bSiteHost ){` |
|         - |  9314 | `			/* The site already knows which host entry this name means (see the` |
|         - |  9315 | `			 * VmCallSite consult above the user-table lookup). */` |
|  16258157 |  9316 | `			pEntry = pSiteEntry;` |
|   8130772 |  9317 | `		}else{` |
|         - |  9318 | ``		/* bConstruct: `isset`/`empty`/`unset`/`eval`/`print`/`include*`/`require*` are`` |
|         - |  9319 | `		 * registered host functions here and are no function at all in php, so the` |
|         - |  9320 | `		 * registration answers only the call site the CONSTRUCT's codegen emitted` |
|         - |  9321 | ``		 * (PH7_CALL_CONSTRUCT). A program's own `$f = 'include'; $f($p);` misses and`` |
|         - |  9322 | ``		 * gets php's `Call to undefined function include()`. */`` |
|   1580883 |  9323 | `		int bConstructSite = (pInstr->iP2 & PH7_CALL_CONSTRUCT) != 0;` |
|   1580883 |  9324 | `		pEntry = PH7_VmGetHostFunction(pVm,(const void *)sName.zString,sName.nByte,bConstructSite);` |
|         - |  9325 | `		{` |
|   1580883 |  9326 | `		VmCallArgMap *pCallMap2 = pEffCallMap;` |
|   1580883 |  9327 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|         - |  9328 | `			/* Compiler-qualified: try short name as global fallback */` |
|       379 |  9329 | `			const char *zShort = sName.zString;` |
|         - |  9330 | `			sxu32 i;` |
|      5409 |  9331 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      5035 |  9332 | `				if( sName.zString[i] == '\\' ){` |
|       395 |  9333 | `					zShort = &sName.zString[i + 1];` |
|       195 |  9334 | `				}` |
|      2520 |  9335 | `			}` |
|       379 |  9336 | `			if( zShort != sName.zString ){` |
|       379 |  9337 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       379 |  9338 | `				pEntry = PH7_VmGetHostFunction(pVm,(const void *)zShort,nShort,bConstructSite);` |
|       187 |  9339 | `			}` |
|       187 |  9340 | `		}` |
|         - |  9341 | `		} /* end VmCallArgMap namespace scope */` |
|         - |  9342 | `		/* Every builtin call in a namespaced file lands here, having missed the user` |
|         - |  9343 | `		 * table twice on the way -- this is the answer worth remembering. */` |
|   1580883 |  9344 | `		PH7_VmCallSiteRecord(pVm,pInstr,&sName,bEngineCallee,1,pEntry);` |
|         - |  9345 | `		}` |
|  17839035 |  9346 | `		if( pEntry == 0 ){` |
|         - |  9347 | `			/* php accepts the "Class::method" STATIC-callable string everywhere a` |
|         - |  9348 | `			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).` |
|         - |  9349 | `			 * Split on the LAST "::" (php's own zend_memrchr scan) and route through the` |
|         - |  9350 | `			 * shared array-callable machinery ([class-name, method-name]) instead of` |
|         - |  9351 | `			 * warning undefined. */` |
|    240151 |  9352 | `			const char *zCbCls = 0,*zCbMeth = 0;` |
|    240151 |  9353 | `			sxu32 nCbCls = 0,nCbMeth = 0;` |
|    240151 |  9354 | `			int bScoped = PH7_VmCallableStringParts(sName.zString,sName.nByte,` |
|         - |  9355 | `				&zCbCls,&nCbCls,&zCbMeth,&nCbMeth);` |
|    240151 |  9356 | `			if( bScoped ){` |
|         - |  9357 | `				/* Resolve BEFORE dispatching: the shared dispatcher answers SXRET_OK with` |
|         - |  9358 | `` 				 * a NULL result for a pair it cannot resolve, so `$cb='C::nosuch'; $cb();` `` |
|         - |  9359 | `				 * evaluated to NULL with no diagnostic at all — where php throws the same` |
|         - |  9360 | `				 * Errors the ARRAY form of the same call already raised here. (Its own` |
|         - |  9361 | ``				 * `if( bScoped )` block: this scratch buffer and the dispatch block's`` |
|         - |  9362 | `				 * hashmap are both block-head declarations, and the check runs between` |
|         - |  9363 | `				 * them.) */` |
|         - |  9364 | `				char zSmMsg[192];` |
|    100086 |  9365 | `				sxi32 nSmBrc = pVm->nBoundaryRc;` |
|    100086 |  9366 | `				const void *pSmRes = (const void *)pVm->pResumeFrame;` |
|    150127 |  9367 | `				const char *zSmErr = VmCallableClassMethodError(&(*pVm),` |
|     50041 |  9368 | `					PH7_VmExtractClass(&(*pVm),zCbCls,nCbCls,FALSE,0),` |
|     50041 |  9369 | `					zCbCls,nCbCls,zCbMeth,nCbMeth,TRUE,zSmMsg,sizeof(zSmMsg));` |
|    100086 |  9370 | `				if( zSmErr ){` |
|         - |  9371 | `					sxi32 rcSmErr;` |
|        55 |  9372 | `					int bSmRaised = PH7_VmClassLookupRaised(&(*pVm),nSmBrc,pSmRes);` |
|        55 |  9373 | `					if( pInstr->iP2 & PH7_CALL_SPREAD ){` |
|       ! 0 |  9374 | `						VmSpreadConsume(pVm);` |
|       ! 0 |  9375 | `					}` |
|        55 |  9376 | `					if( nCallArgs > 0 ){` |
|       ! 0 |  9377 | `						VmPopOperand(&pTos,nCallArgs);` |
|       ! 0 |  9378 | `					}` |
|        55 |  9379 | `					PH7_MemObjRelease(pTos);` |
|        55 |  9380 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        55 |  9381 | `					pTos->nIdx = SXU32_HIGH;` |
|        55 |  9382 | `					if( bSmRaised ){` |
|         - |  9383 | `						/* The class name's autoloader threw: land THAT, exactly as the array` |
|         - |  9384 | `						 * form does — php never reports the class missing in this case. */` |
|         6 |  9385 | `						rcSmErr = pVm->nBoundaryRc;` |
|         6 |  9386 | `						pVm->nBoundaryRc = 0;` |
|         6 |  9387 | `						if( rcSmErr == PH7_ABORT ){` |
|       ! 0 |  9388 | `							goto Abort;` |
|         - |  9389 | `						}` |
|         6 |  9390 | `						rc = PH7_EXCEPTION;` |
|        15 |  9391 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  9392 | `					}` |
|        50 |  9393 | `					rcSmErr = VmThrowFromVm(&(*pVm),"Error",zSmErr,(sxu32)SyStrlen(zSmErr));` |
|        50 |  9394 | `					if( rcSmErr == SXERR_ABORT ){` |
|       ! 0 |  9395 | `						goto Abort;` |
|         - |  9396 | `					}` |
|        50 |  9397 | `					rc = rcSmErr;` |
|        76 |  9398 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  9399 | `				}` |
|     50015 |  9400 | `			}` |
|    240099 |  9401 | `			if( bScoped ){` |
|         - |  9402 | `				/* Resolved: hand the callable STRING itself to the shared dispatcher, which` |
|         - |  9403 | ``				 * decodes `Class::method` the same way (it has to, for the callback-argument`` |
|         - |  9404 | `				 * callers). This used to build a throwaway [class,method] map here and enter` |
|         - |  9405 | `				 * through the hashmap branch — two decoders for one spelling. */` |
|         - |  9406 | `				ph7_value sResult;` |
|         - |  9407 | `				sxi32 rcSm;` |
|    150048 |  9408 | `				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|    100030 |  9409 | `					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|    100033 |  9410 | `				SySetReset(&aArg);` |
|    100053 |  9411 | `				while( pArg < pTos ){` |
|        22 |  9412 | `					SySetPut(&aArg,(const void *)&pArg);` |
|        22 |  9413 | `					pArg++;` |
|         2 |  9414 | `				}` |
|    100033 |  9415 | `				PH7_MemObjInit(pVm,&sResult);` |
|    100033 |  9416 | `				pVm->bDiscardCallback = bResultDropped;   /* see the sibling site */` |
|    100033 |  9417 | `				pVm->bDirectCallable = !bViaClosure;` |
|    150048 |  9418 | `				rcSm = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),` |
|    100030 |  9419 | `					(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|    100033 |  9420 | `				pVm->bDiscardCallback = 0;` |
|    100033 |  9421 | `				SySetReset(&aArg);` |
|    100033 |  9422 | `				if( nCallArgs > 0 ){` |
|        18 |  9423 | `					VmPopOperand(&pTos,nCallArgs);` |
|         8 |  9424 | `				}` |
|    100033 |  9425 | `				if( rcSm == PH7_ABORT ){` |
|       ! 0 |  9426 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 |  9427 | `					goto Abort;` |
|         - |  9428 | `				}` |
|    100033 |  9429 | `				if( rcSm == PH7_EXCEPTION ){` |
|         - |  9430 | `					sxi32 iResumePc;` |
|    100004 |  9431 | `					PH7_MemObjRelease(&sResult);` |
|    100004 |  9432 | `					if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|    100001 |  9433 | `						PH7_MemObjRelease(pTos);` |
|         - |  9434 | `						/* Drain the abandoned outer-expression operands` |
|         - |  9435 | ``						 * (`1 + "C::m"()`) to the try's base — one leaked`` |
|         - |  9436 | `						 * slot per caught throw otherwise. */` |
|    300001 |  9437 | `						PH7_RESUME_DRAIN()` |
|    100001 |  9438 | `						pc = iResumePc;` |
|    100001 |  9439 | `						break;` |
|         - |  9440 | `					}` |
|         3 |  9441 | `					goto Exception;` |
|         - |  9442 | `				}` |
|        30 |  9443 | `				PH7_MemObjStore(&sResult,pTos);` |
|        30 |  9444 | `				PH7_MemObjRelease(&sResult);` |
|        30 |  9445 | `				break;` |
|         - |  9446 | `			}` |
|         - |  9447 | `			/* Call to an undefined function is a catchable Error in php 8 — it does` |
|         - |  9448 | `			 * NOT warn and hand back null and carry on, which is what PH7 did (and` |
|         - |  9449 | `			 * which quietly turned a typo into a null-propagating program). */` |
|         - |  9450 | `			{` |
|         - |  9451 | `			SyBlob sMsg;` |
|    140069 |  9452 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    140069 |  9453 | `			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);` |
|         - |  9454 | `			/* Consume this call's captured spread runs so they don't leak into a` |
|         - |  9455 | `			 * later call (this path never reaches VmBuildEffectiveArgMap). */` |
|    140069 |  9456 | `			if( pInstr->iP2 & PH7_CALL_SPREAD ){` |
|       ! 0 |  9457 | `				VmSpreadConsume(pVm);` |
|       ! 0 |  9458 | `			}` |
|         - |  9459 | `			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the` |
|         - |  9460 | `			 * compile-time arg count on the stack, and this early exit skips the` |
|         - |  9461 | `			 * arg-building loop that the normal path uses, so popping only iP1 would` |
|         - |  9462 | `			 * strand the expanded elements and corrupt the enclosing expression. */` |
|    140069 |  9463 | `			if( nCallArgs > 0 ){` |
|       ! 0 |  9464 | `				VmPopOperand(&pTos,nCallArgs);` |
|       ! 0 |  9465 | `			}` |
|    140069 |  9466 | `			PH7_MemObjRelease(pTos);` |
|    210101 |  9467 | `			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|     70032 |  9468 | `				SyBlobLength(&sMsg));` |
|    140069 |  9469 | `			SyBlobRelease(&sMsg);` |
|    140069 |  9470 | `			if( rc == SXERR_ABORT ){` |
|         6 |  9471 | `				goto Abort;` |
|         - |  9472 | `			}` |
|         - |  9473 | `			/* A catch may have run IN PLACE inside VmThrowFromVm (SXRET_OK +` |
|         - |  9474 | ``			 * recorded resume). An unconditional `goto Exception` here unwound the`` |
|         - |  9475 | `` 			 * exec ANYWAY, so `try { nosuchfn(); } catch (Error $e) {} rest();` `` |
|         - |  9476 | `			 * ran the catch and then silently dropped the rest of the script` |
|         - |  9477 | `			 * (exit 0). Route like every other in-exec throw site. */` |
|    360094 |  9478 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|         - |  9479 | `			}` |
|         - |  9480 | `		}` |
|  17598889 |  9481 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|         - |  9482 | `		/* D1: resolve deferred plain-var arguments against the builtin's by-ref position` |
|         - |  9483 | `		 * mask (derived from its signature). A by-ref out-param (preg_match's $matches, …)` |
|         - |  9484 | `		 * is materialized so PH7_VmStoreArgByRef can write back — this is what keeps a` |
|         - |  9485 | ``		 * DYNAMIC-name by-ref builtin (`$f='preg_match'; $f($p,$s,$m)`) working now that the`` |
|         - |  9486 | `		 * compile-time mask no longer sees it. Every other (by-value) arg warns + passes` |
|         - |  9487 | `		 * NULL, which is also what call_user_func & friends want. pVm->pFrame is the caller. */` |
|         - |  9488 | `		{` |
|  17598889 |  9489 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,pFunc->nByRefMask,0,0,pEffCallMap);` |
|  17598907 |  9490 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|         - |  9491 | `		}` |
|         - |  9492 | `		/* Host function (builtin): build the effective spread-key map so the` |
|         - |  9493 | `		 * name-forwarding builtins (call_user_func & friends) relay string keys as` |
|         - |  9494 | `		 * named args, and — critically — so this call's captured runs are consumed.` |
|         - |  9495 | `		 * pArg is the top base here (a builtin call pops no method-name slot). */` |
|  26399546 |  9496 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  17598834 |  9497 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|         - |  9498 | `		/* Does php's compiler rewrite THIS call into an opcode of its own? Only a` |
|         - |  9499 | `		 * literal, unambiguous global name at the exact arity the rewrite covers --` |
|         - |  9500 | `		 * the same three disqualifiers the call_user_func fold has (an unqualified` |
|         - |  9501 | `		 * name inside a namespace, a name read from a variable, a spread), plus a` |
|         - |  9502 | ``		 * `name:` argument, which the rewrite cannot reorder. A call that never came`` |
|         - |  9503 | `		 * from the compiler is not one: the C dispatcher's synthetic call (array_map,` |
|         - |  9504 | ``		 * usort, Reflection's invoke) and a Closure's unwrap (`strlen(...)`) are real`` |
|         - |  9505 | `		 * calls, with a frame. See bFoldedCallee. */` |
|  17598834 |  9506 | `		if( pInstr->nLine != 0 && !bViaClosure && bLiteralCallee && !bNsCallee && (pInstr->iP2 & PH7_CALL_SPREAD) == 0` |
|  16420083 |  9507 | `		 && (pEffCallMap == 0 \|\| !pEffCallMap->bHasNamed) ){` |
|         - |  9508 | `			static const struct { const char *zName; int nLen; int nArg; } aFolded[] = {` |
|         - |  9509 | `				{ "strlen",           sizeof("strlen")-1,           1 },` |
|         - |  9510 | `				{ "count",            sizeof("count")-1,            1 },` |
|         - |  9511 | `				{ "sizeof",           sizeof("sizeof")-1,           1 },` |
|         - |  9512 | `				{ "array_key_exists", sizeof("array_key_exists")-1, 2 },` |
|         - |  9513 | `				{ "get_class",        sizeof("get_class")-1,        1 },` |
|         - |  9514 | `				{ "get_class",        sizeof("get_class")-1,        0 },` |
|         - |  9515 | `				{ "get_called_class", sizeof("get_called_class")-1, 0 },` |
|         - |  9516 | ``				/* the casts: `strval($a)` warns and runs __toString with no strval frame */`` |
|         - |  9517 | `				{ "strval",           sizeof("strval")-1,           1 },` |
|         - |  9518 | `				{ "intval",           sizeof("intval")-1,           1 },` |
|         - |  9519 | `				{ "floatval",         sizeof("floatval")-1,         1 },` |
|         - |  9520 | `				{ "doubleval",        sizeof("doubleval")-1,        1 },` |
|         - |  9521 | `				{ "boolval",          sizeof("boolval")-1,          1 },` |
|         - |  9522 | `			};` |
|         - |  9523 | `			sxu32 iF;` |
| 156782547 |  9524 | `			for( iF = 0 ; iF < SX_ARRAYSIZE(aFolded) ; ++iF ){` |
| 145091550 |  9525 | `				if( (int)pFunc->sName.nByte == aFolded[iF].nLen` |
|  92034434 |  9526 | `				 && nCallArgs == aFolded[iF].nArg` |
|  22328822 |  9527 | `				 && SyStrnicmp(pFunc->sName.zString,aFolded[iF].zName,(sxu32)aFolded[iF].nLen) == 0 ){` |
|   4728665 |  9528 | `					bFoldedCallee = 1;` |
|   4728665 |  9529 | `					break;` |
|         - |  9530 | `				}` |
|  70182840 |  9531 | `			}` |
|  16419652 |  9532 | `			if( !bFoldedCallee && nCallArgs >= 1 && pFunc->sName.nByte == sizeof("sprintf")-1` |
|   6099660 |  9533 | `			 && SyStrnicmp(pFunc->sName.zString,"sprintf",sizeof("sprintf")-1) == 0 ){` |
|      3162 |  9534 | `				bFoldedCallee = VmSprintfFolds(pInstr,pArg,nCallArgs);` |
|      1576 |  9535 | `			}` |
|   8211190 |  9536 | `		}` |
|         - |  9537 | `		/* php 8.4's FRAMELESS calls: the same disqualifiers, except that an unqualified` |
|         - |  9538 | `		 * name inside a namespace still qualifies -- php compiles both branches and picks` |
|         - |  9539 | `		 * the frameless one at run time when no namespace function answers, which is the` |
|         - |  9540 | `		 * only way this dispatch reaches a builtin. Each function is frameless at the` |
|         - |  9541 | `		 * arities its stub lists (a bit per argument count), and at no other. */` |
|  17598834 |  9542 | `		if( pInstr->nLine != 0 && !bViaClosure && bLiteralCallee && (pInstr->iP2 & PH7_CALL_SPREAD) == 0` |
|  25188534 |  9543 | `		 && (pEffCallMap == 0 \|\| !pEffCallMap->bHasNamed) && nCallArgs >= 1 && nCallArgs <= 3 ){` |
|  25045693 |  9544 | `			bFramelessCallee = PH7_VmFramelessArity(&pFunc->sName,nCallArgs)` |
|         - |  9545 | `				/* ...unless its compiler never made it one: nested in the arguments of` |
|         - |  9546 | `				 * another namespaced frameless call (VmCallArgMap.bNotFrameless). */` |
|  16354611 |  9547 | `				&& !(bNsCallee && ((VmCallArgMap *)pInstr->p3)->bNotFrameless);` |
|   8178748 |  9548 | `		}` |
|    622264 |  9549 | `NativeCall:` |
|         - |  9550 | `		/* A VM_FUNC_NATIVE method joins here, having done the two steps above for` |
|         - |  9551 | `		 * itself: its by-ref mask comes from the same signature machinery, and its` |
|         - |  9552 | `		 * effective arg map was already built (and this call's spread runs already` |
|         - |  9553 | `		 * consumed) on the method path — building it a second time here would` |
|         - |  9554 | `		 * consume them twice. Everything from this point down is shared verbatim:` |
|         - |  9555 | `		 * a native method IS a host call that happens to carry a receiver. */` |
|         - |  9556 | `		/* php raises a deprecated callee's E_DEPRECATED at the CALL, before the` |
|         - |  9557 | ``		 * body and before every screen under it: `curl_close()` with no argument`` |
|         - |  9558 | `		 * warns first and throws the ArgumentCountError second. A native method` |
|         - |  9559 | `		 * joins this block too, and its notice names the DECLARING class. */` |
|  35561475 |  9560 | `		if( pFunc->pDeprecated ){` |
|       275 |  9561 | `			PH7_VmDeprecatedCallNotice(&(*pVm),pFunc->pDeprecated);` |
|       136 |  9562 | `		}` |
|         - |  9563 | `		/* Start collecting function arguments */` |
|  35561475 |  9564 | `		SySetReset(&aArg);` |
|  65475798 |  9565 | `		while( pArg < pTos ){` |
|  29914328 |  9566 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  29914328 |  9567 | `			pArg++;` |
|         5 |  9568 | `		}` |
|         - |  9569 | `		/* Assume a null return value */` |
|  35561475 |  9570 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|         - |  9571 | `		/* Init the call context */` |
|  43823606 |  9572 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,(bDynamicCall ? PH7_CTX_CALL_DYNAMIC : 0)` |
|  42423113 |  9573 | `			\| ((pInstr->nLine != 0 && bLiteralCallee && !bNsCallee && pNativeMethod == 0` |
|  16469999 |  9574 | `			    && (pInstr->iP2 & PH7_CALL_SPREAD) == 0` |
|  26080813 |  9575 | `			    && (pEffCallMap == 0 \|\| !pEffCallMap->bHasNamed)) ? PH7_CTX_CALL_CT_BOUND : 0)` |
|  35561470 |  9576 | `			\| (bFoldedCallee ? PH7_CTX_CALL_FOLDED : 0));` |
|         - |  9577 | `		/* Hand the call-site named-argument map to the builtin so name-forwarding` |
|         - |  9578 | `		 * helpers (call_user_func & friends) can relay name: arguments — and the` |
|         - |  9579 | `		 * caller's strict_types mode — to the inner callback. Forwarded whole (not` |
|         - |  9580 | `		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even` |
|         - |  9581 | `		 * when its own call site is purely positional; only the two forwarding` |
|         - |  9582 | `		 * builtins read pArgMap, so this is inert for every other host function. */` |
|  35561475 |  9583 | `		sCtx.pArgMap = pEffCallMap;` |
|         - |  9584 | `		/* Receiver + late-static-binding class for a native METHOD. Both stay 0 for a` |
|         - |  9585 | `		 * plain host function (the locals are initialized once per OP_CALL), so this` |
|         - |  9586 | `		 * is inert on the builtin path. The reference on pNativeOwned belongs to the` |
|         - |  9587 | `		 * caller for the span of the call — the native body borrows it and must not` |
|         - |  9588 | `		 * unref, exactly as a bytecode method's frame $this is borrowed. */` |
|  35561475 |  9589 | `		sCtx.pThis = pNativeRecv;` |
|  35561475 |  9590 | `		sCtx.pCalledClass = pNativeClass;` |
|         - |  9591 | `		{` |
|  35561475 |  9592 | `		int nGiven = (int)SySetUsed(&aArg);` |
|         - |  9593 | `		/* Trailing actuals the binder collected as unknown-name EXTRAS past a variadic` |
|         - |  9594 | `		 * signature's declared parameters, and the map naming them (aNames is owned` |
|         - |  9595 | `		 * here, released at NativeCallDone). php keeps them out of the argument count` |
|         - |  9596 | `		 * its ZPP screens, so they are subtracted from every screen below and only the` |
|         - |  9597 | `		 * C body sees them. */` |
|  35561475 |  9598 | `		int nExtraNamed = 0;` |
|  35561475 |  9599 | `		int iExtraRule = VM_XNAMED_TAKE;` |
|         - |  9600 | `		VmCallArgMap sTailMap;` |
|         - |  9601 | `		/* The trace frame php gives this internal call. Linked in below, AFTER the` |
|         - |  9602 | `		 * two screens php answers from the caller's own frame (a named argument it` |
|         - |  9603 | `		 * cannot bind, and a non-variable in a by-reference position -- neither` |
|         - |  9604 | `		 * leaves an internal frame in php's trace), and unlinked unconditionally at` |
|         - |  9605 | `		 * NativeCallDone: pPrev is seeded here so the restore is a no-op on the` |
|         - |  9606 | `		 * paths that jump there before the link. */` |
|         - |  9607 | `		VmNativeCall sNativeCall;` |
|  35561475 |  9608 | `		sNativeCall.pName = &pFunc->sName;` |
|  35561475 |  9609 | `		sNativeCall.pClass = 0;` |
|  35561475 |  9610 | `		sNativeCall.bStatic = 0;` |
|  35561475 |  9611 | `		sNativeCall.nLine = pVm->nCurLine;` |
|  35561475 |  9612 | `		sNativeCall.pFrame = (void *)pVm->pFrame;` |
|  35561475 |  9613 | `		sNativeCall.nIncDepth = SySetUsed(&pVm->aIncFrame);` |
|  35561475 |  9614 | `		sNativeCall.bElided = pVm->bElideNativeCall;` |
|  35561475 |  9615 | `		sNativeCall.bFrameless = bFramelessCallee;` |
|  35561475 |  9616 | `		pVm->bElideNativeCall = 0;` |
|  35561475 |  9617 | `		sNativeCall.pPrev = pVm->pNativeCall;` |
|  35561475 |  9618 | `		sNativeCall.apArg = 0;` |
|  35561475 |  9619 | `		sNativeCall.nArg = 0;` |
|         - |  9620 | ``		/* Bind `name:` arguments to the callee's declared POSITIONS before anything`` |
|         - |  9621 | `		 * reads the vector — the arity screen, the ZPP screen and the C body all take` |
|         - |  9622 | `		 * it positionally. A host function has no compiled parameter records for` |
|         - |  9623 | `		 * VmResolveNamedArgs to walk, so its signature string is the source of names` |
|         - |  9624 | `		 * and defaults (PH7_VmBindNamedArgsToSig). Without this every named argument` |
|         - |  9625 | `		 * simply stayed where it was WRITTEN. */` |
|  35561475 |  9626 | `		sTailMap.aNames = 0;` |
|  35561475 |  9627 | `		if( pEffCallMap && pEffCallMap->bHasNamed && nGiven > 0 ){` |
|       413 |  9628 | `			rc = PH7_VmBindNamedArgsToSig(&sCtx,pFunc,pEffCallMap,&aArg,&nGiven,` |
|         - |  9629 | `				&nExtraNamed,&sTailMap);` |
|       413 |  9630 | `			if( rc != SXRET_OK ){` |
|        10 |  9631 | `				goto NativeCallDone;` |
|         - |  9632 | `			}` |
|       405 |  9633 | `			if( nExtraNamed > 0 && sTailMap.aNames ){` |
|       231 |  9634 | `				sCtx.pArgMap = &sTailMap;` |
|       113 |  9635 | `			}` |
|       200 |  9636 | `		}` |
|         - |  9637 | `		/* php binds a by-reference argument at the CALL, before the callee runs, so a` |
|         - |  9638 | ``		 * non-variable in a `&` position is refused ahead of every ZPP check — and`` |
|         - |  9639 | ``		 * ahead of the too-MANY-arguments one (`array_pop([1,2],5)` is the reference`` |
|         - |  9640 | `		 * Error in php, not an ArgumentCountError). With no argument at all there is` |
|         - |  9641 | `		 * nothing to refuse, which is why the too-FEW check below still speaks first` |
|         - |  9642 | ``		 * for `array_pop()`. */`` |
|  61513348 |  9643 | `		rc = PH7_VmScreenByRefArgShapes(&sCtx,pFunc,pEffCallMap,nGiven,` |
|  35561462 |  9644 | `			(ph7_value **)SySetBasePtr(&aArg));` |
|  35561467 |  9645 | `		if( rc != SXRET_OK ){` |
|  16339951 |  9646 | `			goto NativeCallDone;` |
|         - |  9647 | `		}` |
|         - |  9648 | `		/* From here down every refusal is one php raises from INSIDE the callee --` |
|         - |  9649 | `		 * the arity screens, the argument-type screen and the C body itself -- so` |
|         - |  9650 | `		 * the internal frame is on the trace for all of them. A native METHOD names` |
|         - |  9651 | ``		 * its DECLARING class, which is what php's `class` key holds (an inherited`` |
|         - |  9652 | `		 * one reports the base, not the receiver's class). */` |
|  19221521 |  9653 | `		if( pNativeMethod ){` |
|         - |  9654 | ``			/* php's `function` key is the BARE method name, with the class in its own`` |
|         - |  9655 | ``			 * key -- the qualified `Class::method` spelling belongs to the diagnostic`` |
|         - |  9656 | `			 * text, and is what the host-function record carries. The method record` |
|         - |  9657 | `			 * holds the name as it was declared. */` |
|   1622577 |  9658 | `			sNativeCall.pName = &pNativeMethod->sName;` |
|   1622577 |  9659 | `			sNativeCall.pClass = pNativeDeclClass;` |
|   1622577 |  9660 | `			sNativeCall.bStatic = (pNativeMethod->iFlags & VM_FUNC_NATIVE_STATIC) != 0;` |
|    811196 |  9661 | `		}` |
|         - |  9662 | `		/* A php LANGUAGE CONSTRUCT is dispatched here as a host function but is not a` |
|         - |  9663 | ``		 * call in php at all: `print`, `isset`, `unset` and `empty` are opcodes with`` |
|         - |  9664 | `		 * no frame, and include/require/eval have a frame of their own SHAPE, built` |
|         - |  9665 | `		 * from the include stack further down the walk. Recording one here emitted` |
|         - |  9666 | `		 * that frame twice -- an exception created at the top level of an included` |
|         - |  9667 | ``		 * unit listed `include()` once for the unit it was thrown in and once for`` |
|         - |  9668 | `		 * the file that loaded it. */` |
|  19221521 |  9669 | `		if( !bFoldedCallee && !pFunc->bConstruct ){` |
|  14216653 |  9670 | `			pVm->pNativeCall = &sNativeCall;` |
|   7108325 |  9671 | `		}` |
|         - |  9672 | `		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum` |
|         - |  9673 | `		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError` |
|         - |  9674 | `		 * before the C routine runs when called with too few arguments — instead` |
|         - |  9675 | `		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""` |
|         - |  9676 | `		 * return. The message wording matches php's ZPP output byte-for-byte. */` |
|         - |  9677 | `		/* An unknown-name extra is refused from the callee's own parameter parsing,` |
|         - |  9678 | `		 * at a moment that depends on how php's C code parses (see` |
|         - |  9679 | `		 * PH7_VmBuiltinExtraNamedRule); the forwards take it instead. */` |
|  19221521 |  9680 | `		iExtraRule = nExtraNamed > 0 ? PH7_VmBuiltinExtraNamedRule(&pFunc->sName) : VM_XNAMED_TAKE;` |
|  28833483 |  9681 | `		if( iExtraRule == VM_XNAMED_BEFORE_ARITY ){` |
|         9 |  9682 | `			rc = PH7_VmRefuseExtraNamed(&sCtx,pFunc);` |
|  19221517 |  9683 | `		}else if( pFunc->nMinArg > 0 && nGiven - nExtraNamed < pFunc->nMinArg ){` |
|      1046 |  9684 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|         - |  9685 | `				"%z() expects %s %d argument%s, %d given",` |
|       347 |  9686 | `				&pFunc->sName,` |
|       694 |  9687 | `				pFunc->bAtLeast ? "at least" : "exactly",` |
|       694 |  9688 | `				(int)pFunc->nMinArg,` |
|       694 |  9689 | `				pFunc->nMinArg == 1 ? "" : "s",` |
|       347 |  9690 | `				nGiven - nExtraNamed);` |
|  19221166 |  9691 | `		}else if( pFunc->bHasMaxArg && nGiven > (int)pFunc->nMaxArg ){` |
|         - |  9692 | `			/* php enforces the MAXIMUM as well, and PHL only did so where a` |
|         - |  9693 | `			 * builtin happened to hand-roll the check (51 of ~650), so` |
|         - |  9694 | ``			 * `microtime(1,2)`, `strlen("a","b")` and friends silently ignored`` |
|         - |  9695 | `			 * the extras. The count comes from the same signature table as the` |
|         - |  9696 | `			 * minimum; a variadic tail leaves nMaxArg at -1 and is exempt.` |
|         - |  9697 | `			 * php says "exactly" when the bounds coincide, "at most" otherwise. */` |
|       368 |  9698 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|         - |  9699 | `				"%z() expects %s %d argument%s, %d given",` |
|       121 |  9700 | `				&pFunc->sName,` |
|       204 |  9701 | `				((int)pFunc->nMinArg == (int)pFunc->nMaxArg && !pFunc->bAtLeast) ? "exactly" : "at most",` |
|       242 |  9702 | `				(int)pFunc->nMaxArg,` |
|       242 |  9703 | `				pFunc->nMaxArg == 1 ? "" : "s",` |
|       121 |  9704 | `				nGiven);` |
|  19220698 |  9705 | `		}else if( iExtraRule == VM_XNAMED_BEFORE_TYPES ){` |
|         5 |  9706 | `			rc = PH7_VmRefuseExtraNamed(&sCtx,pFunc);` |
|  28832063 |  9707 | `		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven - nExtraNamed,` |
|  19220568 |  9708 | `			(ph7_value **)SySetBasePtr(&aArg))) ){` |
|         - |  9709 | `			/* TypeError thrown: rc carries the caught/uncaught status */` |
|  19219901 |  9710 | `		}else if( iExtraRule == VM_XNAMED_REFUSE ){` |
|        30 |  9711 | `			rc = PH7_VmRefuseExtraNamed(&sCtx,pFunc);` |
|        16 |  9712 | `		}else{` |
|         - |  9713 | `			/* The name of the builtin that is RUNNING, for the few diagnostics` |
|         - |  9714 | `			 * raised so deep inside the engine that no ph7_context reaches them` |
|         - |  9715 | `			 * (a stream filter's, from inside a device read) and which php still` |
|         - |  9716 | `			 * prefixes with the caller. Saved and restored: a builtin can call` |
|         - |  9717 | `			 * back into php and reach this line again. */` |
|  19219207 |  9718 | `			SyString *pSavedCallee = pVm->pCalleeName;` |
|         - |  9719 | `			/* php's two callback FORWARDS pass "the answer is being dropped" on to` |
|         - |  9720 | `			 * the callback they drive; every other builtin ignores this. Saved and` |
|         - |  9721 | `			 * restored for the same reason the callee name is. */` |
|  19219207 |  9722 | `			int bSavedHostDiscard = pVm->bHostDiscard;` |
|  19219207 |  9723 | `			SyString *pSavedNativeFrame = pVm->pNativeFrameName;` |
|  19219207 |  9724 | `			pVm->pCalleeName = &pFunc->sName;` |
|  19219207 |  9725 | `			pVm->bHostDiscard = bResultDropped && bLiteralCallee;` |
|         - |  9726 | `			/* A forward php could not elide invokes its callback the way any other` |
|         - |  9727 | `			 * internal function does: the callback's frame gets no file or line, and` |
|         - |  9728 | `			 * this builtin gets a frame of its own. Only the FRAME shape is affected --` |
|         - |  9729 | `			 * the argument BINDING mode still travels the forward's own map, which is` |
|         - |  9730 | `			 * php's rule and a separate latch (bCallbackWeak). A FRAMELESS call` |
|         - |  9731 | `			 * in a namespace is no such shape: php picks its frameless branch at` |
|         - |  9732 | `			 * run time, and what it calls back is called from the user frame. */` |
|  19219207 |  9733 | `			if( !bFramelessCallee && (bNsCallee \|\| !bLiteralCallee \|\| (pInstr->iP2 & PH7_CALL_SPREAD)) ){` |
|         - |  9734 | `				/* ...and two more shapes php cannot fold, for the same compile-time` |
|         - |  9735 | `				 * reason. A name that is not a literal at all` |
|         - |  9736 | ``				 * (`$n = 'call_user_func'; $n($c)`), and an argument list carrying a`` |
|         - |  9737 | ``				 * SPREAD (`call_user_func_array(...$pair)`) -- the fold rewrites the`` |
|         - |  9738 | `				 * call into a direct one and needs the arity at compile time, which a` |
|         - |  9739 | `				 * runtime unpack does not give it. */` |
|      2894 |  9740 | `				pVm->pNativeFrameName = &pFunc->sName;` |
|      1397 |  9741 | `			}` |
|         - |  9742 | `			/* Only the C body can reach for a callback, so only it can be asked for` |
|         - |  9743 | `			 * a trace that shows its arguments. An unknown-name extra is no positional` |
|         - |  9744 | `			 * argument (php's ZPP never counts it). */` |
|  19219207 |  9745 | `			sNativeCall.apArg = (ph7_value **)SySetBasePtr(&aArg);` |
|  19219207 |  9746 | `			sNativeCall.nArg = nGiven - nExtraNamed;` |
|         - |  9747 | `			/* Call the foreign function */` |
|  19219207 |  9748 | `			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));` |
|  19219215 |  9749 | `			pVm->bHostDiscard = bSavedHostDiscard;` |
|  19219215 |  9750 | `			pVm->pCalleeName = pSavedCallee;` |
|  19219215 |  9751 | `			pVm->pNativeFrameName = pSavedNativeFrame;` |
|  19219215 |  9752 | `			if( PH7_CmpRefusalPending(pVm) ){` |
|         - |  9753 | `				/* A native compare handler refused a pair this builtin compared` |
|         - |  9754 | `				 * (in_array, sort, max and switch all drive the same comparator,` |
|         - |  9755 | `				 * which has no throw boundary of its own and only recorded it).` |
|         - |  9756 | `				 * php raises out of the comparison and the builtin never finishes;` |
|         - |  9757 | `				 * this one finishes first and then throws, the way every builtin` |
|         - |  9758 | `				 * whose failure is predicted rather than raised in flight does.` |
|         - |  9759 | `				 * Reported on the call context, so VmHostFuncThrowRc below lands` |
|         - |  9760 | `				 * it exactly as the builtin's own throws are landed -- unless the` |
|         - |  9761 | `				 * builtin ALREADY raised, in which case the first throw wins and` |
|         - |  9762 | `				 * the record is only dropped. */` |
|        18 |  9763 | `				if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND` |
|        20 |  9764 | `				 \|\| sCtx.nThrowRc != 0 ){` |
|       ! 0 |  9765 | `					PH7_CmpRefusalClear(&(*pVm));` |
|       ! 0 |  9766 | `				}else{` |
|        20 |  9767 | `					PH7_CmpRefusalRaiseCtx(&sCtx);` |
|         - |  9768 | `				}` |
|         9 |  9769 | `			}` |
|         - |  9770 | `			/* A host function that RAISED a catchable throw (PH7_VmThrowException)` |
|         - |  9771 | `			 * and still returned PH7_OK reports it here — the throw's catch has` |
|         - |  9772 | `			 * already run in place, so treating the call as a normal return would` |
|         - |  9773 | `			 * resume execution INSIDE the try body the throw abandoned (and, when` |
|         - |  9774 | `			 * uncaught, run on past the reported fatal). The status is recorded on` |
|         - |  9775 | `			 * the call context, so this covers the shared validation helpers whose` |
|         - |  9776 | `			 * callers have no channel to thread a status back. */` |
|  19219215 |  9777 | `			rc = VmHostFuncThrowRc(&sCtx,rc);` |
|         - |  9778 | `		}` |
|   9609593 |  9779 | `NativeCallDone:` |
|  25951885 |  9780 | `		(void)nGiven; /* the named-arg binder's early exit lands here */` |
|  35561483 |  9781 | `		pVm->pNativeCall = sNativeCall.pPrev;` |
|  35561483 |  9782 | `		if( sTailMap.aNames ){` |
|       231 |  9783 | `			SyMemBackendFree(&pVm->sAllocator,sTailMap.aNames);` |
|       113 |  9784 | `		}` |
|         - |  9785 | `		}` |
|         - |  9786 | `		/* Release the call context */` |
|  35561483 |  9787 | `		VmReleaseCallContext(&sCtx);` |
|  35561483 |  9788 | `		if( pNativeOwned ){` |
|         - |  9789 | `			/* Drop the receiver reference the method branch took (pThis->iRef++ before` |
|         - |  9790 | `			 * the target slot was released). A bytecode method hands this to its frame` |
|         - |  9791 | `			 * and lets the teardown do it; a native method has no frame, so it is` |
|         - |  9792 | `			 * dropped here — the one point EVERY route out of the call passes through,` |
|         - |  9793 | `			 * including the throw/abort/suspend ones below. Ordered after the context` |
|         - |  9794 | `			 * release because sCtx.sThis aliases the instance. Always 0 for a plain` |
|         - |  9795 | `			 * host function. */` |
|   1604876 |  9796 | `			PH7_ClassInstanceUnref(pNativeOwned);` |
|   1604876 |  9797 | `			pNativeOwned = 0;` |
|   1604876 |  9798 | `			pNativeRecv = 0;` |
|    802353 |  9799 | `		}` |
|  35561483 |  9800 | `		if( rc == PH7_ABORT ){` |
|         - |  9801 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|         - |  9802 | `			 * the Abort: label only frees the operand stack, not this local` |
|         - |  9803 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|  16340817 |  9804 | `			PH7_MemObjRelease(&sRet);` |
|  16340817 |  9805 | `			goto Abort;` |
|         - |  9806 | `		}` |
|  19220671 |  9807 | `		if( rc != PH7_SUSPEND && VmInlineOwnedBy(pVm,aInstr,sState.pEntryFrame) ){` |
|         - |  9808 | `			/* A throw raised inside this host function — directly` |
|         - |  9809 | `			 * (PH7_VmThrowException) or by a PHP callback it invoked — was` |
|         - |  9810 | `			 * caught by an INLINE try (generator body) THIS exec owns.` |
|         - |  9811 | `			 * VmThrowInline records only a pc-redirect: a direct builtin throw` |
|         - |  9812 | `			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION` |
|         - |  9813 | `			 * with the redirect pending, so the rc branches below never land` |
|         - |  9814 | `			 * it (pre-existing hole: explode("") or a throwing usort` |
|         - |  9815 | `			 * comparator inside a generator's try lost the catch AND the` |
|         - |  9816 | `			 * yield). Land at the redirect now — its drain to the try's` |
|         - |  9817 | `			 * operand base subsumes the args + name pops. */` |
|        12 |  9818 | `			PH7_MemObjRelease(&sRet);` |
|        38 |  9819 | `			PH7_INLINE_RESUME_BREAK()` |
|       ! 0 |  9820 | `		}` |
|  19220661 |  9821 | `		if( rc == PH7_EXCEPTION ){` |
|         - |  9822 | `			/* A callback invoked by this host function threw. If an in-place catch` |
|         - |  9823 | `			 * recorded a resume target owned by THIS body, resume at its landing pad` |
|         - |  9824 | `			 * (consuming the target); otherwise the exception was caught by an outer` |
|         - |  9825 | `			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW` |
|         - |  9826 | `			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which` |
|         - |  9827 | `			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */` |
|         - |  9828 | `			sxi32 iResumePc;` |
|     19482 |  9829 | `			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|         - |  9830 | `				/* Caught by an outer exec, or not caught here: propagate. */` |
|      5732 |  9831 | `				goto Exception;` |
|         - |  9832 | `			}` |
|         - |  9833 | `			/* Exception was caught in place by THIS body's try: pop args and the` |
|         - |  9834 | `			 * result slot, then drain any abandoned outer-expression operands to` |
|         - |  9835 | `			 * the try's base and resume. */` |
|     13755 |  9836 | `			PH7_MemObjRelease(&sRet);` |
|     13755 |  9837 | `			if( nCallArgs > 0 ){` |
|     13165 |  9838 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      6580 |  9839 | `			}` |
|     13755 |  9840 | `			VmPopOperand(&pTos,1);` |
|     18315 |  9841 | `			PH7_RESUME_DRAIN()` |
|     13755 |  9842 | `			pc = iResumePc;` |
|     13755 |  9843 | `			break;` |
|         - |  9844 | `		}` |
|  19201184 |  9845 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|         - |  9846 | `			/* Fiber::suspend() was called from within a fiber.` |
|         - |  9847 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|         - |  9848 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|         - |  9849 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|         - |  9850 | `			 * and we need to save state here. If it's a nested call (method` |
|         - |  9851 | `			 * body), the user-function path above will handle re-saving. */` |
|       ! 0 |  9852 | `			PH7_MemObjRelease(&sRet);` |
|       ! 0 |  9853 | `			if( nCallArgs > 0 ){` |
|       ! 0 |  9854 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|       ! 0 |  9855 | `			}` |
|         - |  9856 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|         - |  9857 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       ! 0 |  9858 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       ! 0 |  9859 | `			goto Suspend;` |
|         - |  9860 | `		}` |
|  19201184 |  9861 | `		if( nCallArgs > 0 ){` |
|         - |  9862 | `			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack` |
|         - |  9863 | `			 * expanded the compile-time arg count on the stack, so popping iP1` |
|         - |  9864 | `			 * strands the extra elements (or, for an unpack that expanded to fewer` |
|         - |  9865 | `			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,` |
|         - |  9866 | `			 * reading a bogus value whose stray flags sent MemObjStore into the` |
|         - |  9867 | `			 * hashmap-release path and hung). Mirrors every other CALL exit. The` |
|         - |  9868 | `			 * function-name slot (pTos) receives the return value below. */` |
|  19084784 |  9869 | `			VmPopOperand(&pTos,nCallArgs);` |
|   9543718 |  9870 | `		}` |
|         - |  9871 | `		/* Save foreign function return value into the (now top) function-name slot */` |
|  19201184 |  9872 | `		PH7_MemObjStore(&sRet,pTos);` |
|         - |  9873 | `		/* ...and clear that slot's index. It is one of the call's own argument slots,` |
|         - |  9874 | `		 * still carrying the variable index the argument was loaded with, and` |
|         - |  9875 | `		 * PH7_MemObjStore does not touch nIdx — so a builtin's return value came back` |
|         - |  9876 | ``		 * looking like an lvalue for the caller's variable (`f(strtoupper($b))` with`` |
|         - |  9877 | ``		 * `function f(&$x)` overwrote `$b`). No host function returns by reference. */`` |
|  19201184 |  9878 | `		pTos->nIdx = SXU32_HIGH;` |
|  19201184 |  9879 | `		PH7_MemObjRelease(&sRet);` |
|         - |  9880 | `	}` |
|  19205534 |  9881 | `	break;` |
|         - |  9882 | `				  }` |
|         - |  9883 | `/*` |
|         - |  9884 | ` * OP_CONSUME: P1 * *` |
|         - |  9885 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|         - |  9886 | ` */` |
|    118075 |  9887 | `case PH7_OP_CONSUME: {` |
|         - |  9888 | `	VmOpRc rcOp;` |
|    235994 |  9889 | `	sState.pTos = pTos;` |
|    235994 |  9890 | `	sState.pc = pc;` |
|    235994 |  9891 | `	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);` |
|    235994 |  9892 | `	pTos = sState.pTos;` |
|    235994 |  9893 | `	pc = sState.pc;` |
|    235994 |  9894 | `	if( rcOp == VM_OP_ABORT ){` |
|         3 |  9895 | `		goto Abort;` |
|    235992 |  9896 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        14 |  9897 | `		goto Exception;` |
|         - |  9898 | `	}` |
|    235975 |  9899 | `	break;` |
|         - |  9900 | `					  }` |
|         - |  9901 |  |
|         - |  9902 | `		} /* Switch() */` |
| 409940597 |  9903 | `		pc++; /* Next instruction in the stream */` |
|         5 |  9904 | `	} /* For(;;) */` |
|   2280583 |  9905 | `Done:` |
|         - |  9906 | `	/* A stacked callee completing lands here too (its result is already in` |
|         - |  9907 | `	 * sState.pResult — the caller's operand slot); Unwind's first iteration` |
|         - |  9908 | `	 * bottoms out identically for the record-less case. */` |
|   4561085 |  9909 | `	rc = SXRET_OK;` |
|   4561085 |  9910 | `	goto Unwind;` |
|       886 |  9911 | `Suspend:` |
|      1777 |  9912 | `	rc = PH7_SUSPEND;` |
|      1777 |  9913 | `	if( pCallTop != 0 ){` |
|         - |  9914 | `		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment` |
|         - |  9915 | `		 * instead of the lossy unwind. pc/nTos of the innermost activation were` |
|         - |  9916 | `		 * already saved into the ctx by VmSuspendCtx; capture the rest (the` |
|         - |  9917 | `		 * record chain, the innermost activation, the suspend-time top frame)` |
|         - |  9918 | `		 * so resume re-enters HERE, inside the innermost callee, like php. The` |
|         - |  9919 | `		 * records / frames / operand stacks stay alive — nothing is freed. Only` |
|         - |  9920 | `		 * fibers reach this (generators yield only at their body level, pCallTop` |
|         - |  9921 | `		 * == 0); a suspend inside a C->PHP callback was already rejected with a` |
|         - |  9922 | `		 * FiberError before it could arrive here. */` |
|       ! 0 |  9923 | `		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));` |
|       ! 0 |  9924 | `		if( pSeg == 0 ){` |
|         - |  9925 | `			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST` |
|         - |  9926 | `			 * pc/nTos into the ctx, so the lossy body-level fallback would resume` |
|         - |  9927 | `			 * the callee's pc against the body stack — silent corruption, exactly` |
|         - |  9928 | `			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the` |
|         - |  9929 | `			 * the convention shared with the stage-2 record-alloc OOM site). */` |
|       ! 0 |  9930 | `			PH7_VmMemoryError(&(*pVm));` |
|       ! 0 |  9931 | `			rc = PH7_ABORT;` |
|       ! 0 |  9932 | `			goto Unwind;` |
|         - |  9933 | `		}` |
|       ! 0 |  9934 | `		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */` |
|       ! 0 |  9935 | `		sState.pHigh = ( pHigh > pTos ) ? pHigh : pTos;` |
|       ! 0 |  9936 | `		pSeg->sState = sState;` |
|       ! 0 |  9937 | `		pSeg->pCallTop = pCallTop;` |
|       ! 0 |  9938 | `		pSeg->pTopFrame = pVm->pFrame;` |
|       ! 0 |  9939 | `		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;` |
|       ! 0 |  9940 | `		pSeg->nOldFinBase = pVm->pActiveCtx ? pVm->pActiveCtx->nFinallyBase : 0;` |
|         - |  9941 | `		{` |
|         - |  9942 | `			VmCallFrame *pRec;` |
|       ! 0 |  9943 | `			pSeg->nRecords = 0;` |
|       ! 0 |  9944 | `			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){` |
|       ! 0 |  9945 | `				pSeg->nRecords++;` |
|       ! 0 |  9946 | `			}` |
|         - |  9947 | `		}` |
|       ! 0 |  9948 | `		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;` |
|         - |  9949 | `		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */` |
|       ! 0 |  9950 | `		SySetRelease(&aArg);` |
|       ! 0 |  9951 | `		return PH7_SUSPEND;` |
|         - |  9952 | `	}` |
|      1777 |  9953 | `	goto Unwind;` |
|       663 |  9954 | `Abort:` |
|  16341230 |  9955 | `	rc = PH7_ABORT;` |
|  16341230 |  9956 | `	goto Unwind;` |
|    306928 |  9957 | `Exception:` |
|    613806 |  9958 | `	rc = PH7_EXCEPTION;` |
|    613801 |  9959 | `	goto Unwind;` |
|   2589060 |  9960 | `Unwind:` |
|         - |  9961 | `	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration` |
|         - |  9962 | `	 * finishes the top record exactly as the old per-level native return did:` |
|         - |  9963 | `	 * for ABORT/EXCEPTION, first run what the popped activation's own` |
|         - |  9964 | `	 * Abort/Exception label used to do (clear its pending return, release its` |
|         - |  9965 | `	 * operands — a stacked activation never has bReturnPropagates set), then` |
|         - |  9966 | `	 * VmCallFinish routes in the restored caller (an in-place catch there` |
|         - |  9967 | `	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —` |
|         - |  9968 | `	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible` |
|         - |  9969 | `	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).` |
|         - |  9970 | `	 * At the bottom, VmExecFinalize hands the status to the native caller. */` |
|  26155093 |  9971 | `	for(;;){` |
|  28895054 |  9972 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|         - |  9973 | `			/* Drop any pending hook-RMW write-backs this activation armed — its` |
|         - |  9974 | `			 * statement is abandoned (only the innermost activation at throw time` |
|         - |  9975 | `			 * can own entries: the armed window spans exactly one instruction, so` |
|         - |  9976 | `			 * no OP_CALL record ever intervenes). */` |
|  46832569 |  9977 | `			while( SySetUsed(&pVm->aHookRmw) > 0` |
|    916930 |  9978 | `			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){` |
|         3 |  9979 | `				VmHookRmwDropTop(&(*pVm));` |
|         1 |  9980 | `			}` |
|    458436 |  9981 | `		}` |
|  23417380 |  9982 | `		if( pCallTop == 0 ){` |
|  22532022 |  9983 | `			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);` |
|         - |  9984 | `		}` |
|    885363 |  9985 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|    609489 |  9986 | `			VmClearFramePending(sState.pEntryFrame);` |
|    626261 |  9987 | `			while( pTos >= pStack ){` |
|     16777 |  9988 | `				PH7_MemObjRelease(pTos);` |
|     16777 |  9989 | `				pTos--;` |
|         5 |  9990 | `			}` |
|    304715 |  9991 | `		}` |
|    885363 |  9992 | `		if( rc != PH7_SUSPEND ){` |
|         - |  9993 | `			/* The finishing callee's own leaked finally actions must not survive` |
|         - |  9994 | `			 * into the caller (whose next OP_END_FINALLY would mis-pop them). */` |
|    885363 |  9995 | `			VmDiscardFinallyActions(&(*pVm),sState.nFinallyActBase);` |
|    442871 |  9996 | `		}` |
|         - |  9997 | `		{` |
|    885363 |  9998 | `			VmCallFrame *pRec = pCallTop;` |
|         - |  9999 | `			/* What the finishing callee ever touched of its own operand stack. The` |
|         - | 10000 | `			 * live top is in it too: an op handler that pushed and then threw hands` |
|         - | 10001 | `			 * control to the drain above without another instruction fetch, so pTos` |
|         - | 10002 | `			 * can be above the last sampled watermark. VmCallFinish's recycle sweeps` |
|         - | 10003 | `			 * exactly this much and leaves the rest of the buffer alone. */` |
|    885363 | 10004 | `			if( pTos > pHigh ){` |
|       ! 0 | 10005 | `				pHigh = pTos;` |
|       ! 0 | 10006 | `			}` |
|    885363 | 10007 | `			pRec->sCall.nLiveTos = (pHigh >= pStack) ? (sxu32)(pHigh - pStack) + 1 : 0;` |
|    885363 | 10008 | `			sState = pRec->sCaller;` |
|    885363 | 10009 | `			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);` |
|    885363 | 10010 | `			pCallTop = pRec->pPrev;` |
|    885363 | 10011 | `			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;` |
|    885363 | 10012 | `			pVm->pIdleCallFrames = (void *)pRec;` |
|    885363 | 10013 | `			aInstr = sState.aInstr;` |
|    885363 | 10014 | `			pStack = sState.pStack;` |
|    885363 | 10015 | `			pTos = sState.pTos;` |
|    885363 | 10016 | `			pHigh = sState.pHigh;` |
|    885363 | 10017 | `			pc = sState.pc;` |
|         - | 10018 | `		}` |
|    885363 | 10019 | `		if( rc == PH7_OK ){` |
|   7960742 | 10020 | `			pc++; /* the loop-bottom increment this OP_CALL missed */` |
|   7960742 | 10021 | `			goto VmLoopFetch;` |
|         - | 10022 | `		}` |
|         5 | 10023 | `	}` |
|  20234553 | 10024 | `}` |
|         - | 10025 |  |
