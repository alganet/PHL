# src/ph7/vm_exec.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4085/4566 lines (89.47%)

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
|   104999 |   45 | `static int VmGrowOperandStack(ph7_vm *pVm, sxu32 nNeed,` |
|        - |   46 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |   47 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        5 |   48 | `{` |
|   105004 |   49 | `	ph7_value *pOld = *ppStack;` |
|   105004 |   50 | `	sxu32 nOldCap = pState->nStackCap;` |
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
|   105004 |   64 | `	nReq = nNeed + pState->nStackOrig;` |
|   105004 |   65 | `	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */` |
|      ! 0 |   66 | `		nReq = SXU32_HIGH;` |
|      ! 0 |   67 | `	}` |
|        - |   68 | `	/* SyMemBackendRealloc's size argument is sxu32, so the byte count` |
|        - |   69 | `	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack` |
|        - |   70 | `	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and` |
|        - |   71 | `	 * the init loop below would run off it. If the true requirement exceeds the` |
|        - |   72 | `	 * representable cap, treat it as OOM (the caller raises the guard error). */` |
|   105004 |   73 | `	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);` |
|   105004 |   74 | `	if( nReq > nMaxCap ){` |
|      ! 0 |   75 | `		return 0;` |
|        - |   76 | `	}` |
|   105004 |   77 | `	if( nReq <= nOldCap ){` |
|   100618 |   78 | `		return 1; /* already fits — no growth needed */` |
|        - |   79 | `	}` |
|     4390 |   80 | `	nNewCap = nReq;` |
|        - |   81 | `	/* Amortize repeated spreads in one argument list: at least double, but never` |
|        - |   82 | `	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't` |
|        - |   83 | `	 * itself overflow.) */` |
|     4390 |   84 | `	if( nNewCap < nOldCap * 2 ){` |
|     4390 |   85 | `		sxu32 nDbl = nOldCap * 2;` |
|     4390 |   86 | `		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }` |
|     4390 |   87 | `		if( nNewCap < nDbl ){ nNewCap = nDbl; }` |
|     2145 |   88 | `	}` |
|     6535 |   89 | `	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,` |
|     2145 |   90 | `		nNewCap * sizeof(ph7_value));` |
|     4390 |   91 | `	if( pNew == 0 ){` |
|      ! 0 |   92 | `		return 0; /* OOM: caller keeps pOld and raises the guard error */` |
|        - |   93 | `	}` |
|        - |   94 | `	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */` |
|   162651 |   95 | `	for( i = nOldCap; i < nNewCap; i++ ){` |
|   158266 |   96 | `		PH7_MemObjInit(pVm, &pNew[i]);` |
|   158266 |   97 | `		pNew[i].nIdx = SXU32_HIGH;` |
|    77758 |   98 | `	}` |
|        - |   99 | `	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */` |
|     4390 |  100 | `	*ppTos = pNew + (*ppTos - pOld);` |
|     4390 |  101 | `	*ppStack = pNew;` |
|     4390 |  102 | `	pState->pStack = pNew + (pState->pStack - pOld);` |
|     4390 |  103 | `	pState->pTos = pNew + (pState->pTos - pOld);` |
|     4390 |  104 | `	pState->nStackCap = nNewCap;` |
|     4390 |  105 | `	if( pCallTop ){` |
|        - |  106 | `		/* This activation is a trampoline callee: its record owns the buffer. */` |
|     4278 |  107 | `		pCallTop->sCall.pFrameStack = pNew;` |
|     4278 |  108 | `		pCallTop->sCall.nStackCap = nNewCap;` |
|     2094 |  109 | `	}else{` |
|        - |  110 | `		/* Base activation: the native entry frees *ppBaseOwner (and, for a` |
|        - |  111 | `		 * resumable coroutine, persists *pnBaseCap across suspend/resume). */` |
|      117 |  112 | `		if( ppBaseOwner ){ *ppBaseOwner = pNew; }` |
|      117 |  113 | `		if( pnBaseCap ){ *pnBaseCap = nNewCap; }` |
|        - |  114 | `	}` |
|        - |  115 | `	/* Re-anchor this activation's captured spread runs (pStart into the old` |
|        - |  116 | `	 * buffer). Enclosing-activation runs live in other buffers — leave them. */` |
|     4390 |  117 | `	nRun = SySetUsed(&pVm->aSpreadRun);` |
|     4390 |  118 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|     4390 |  119 | `	for( i = 0; i < nRun; i++ ){` |
|      ! 0 |  120 | `		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){` |
|      ! 0 |  121 | `			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);` |
|      ! 0 |  122 | `		}` |
|      ! 0 |  123 | `	}` |
|     4390 |  124 | `	return 1;` |
|    52457 |  125 | `}` |
|        - |  126 | `/*` |
|        - |  127 | ` * Ensure the running activation's operand stack can hold a spread of nEntry` |
|        - |  128 | ` * elements pushed at *ppTos (the source slot becomes the first element, so the` |
|        - |  129 | ` * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.` |
|        - |  130 | ` * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.` |
|        - |  131 | ` */` |
|     1311 |  132 | `static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,` |
|        - |  133 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |  134 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        5 |  135 | `{` |
|        - |  136 | `	sxu32 nNeed;` |
|     1316 |  137 | `	if( nEntry == 0 ){` |
|      214 |  138 | `		return 1; /* empty spread never grows the stack */` |
|        - |  139 | `	}` |
|     1104 |  140 | `	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);` |
|     1606 |  141 | `	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,` |
|      502 |  142 | `		pCallTop, ppBaseOwner, pnBaseCap);` |
|      613 |  143 | `}` |
|        - |  144 | `/*` |
|        - |  145 | ` * Terminal teardown of one VmByteCodeExec activation — the former` |
|        - |  146 | ` * Done/Suspend/Abort/Exception label bodies, one home.` |
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
|  4541569 |  164 | `static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)` |
|        5 |  165 | `{` |
|  4541574 |  166 | `	if( rcTerm != PH7_SUSPEND ){` |
|  4539902 |  167 | `		VmDiscardFinallyActions(&(*pVm),pState->nFinallyActBase);` |
|  2269762 |  168 | `	}` |
|  4541574 |  169 | `	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){` |
|  3074307 |  170 | `		VmClearFramePending(pState->pEntryFrame);` |
|  1536992 |  171 | `	}` |
|  4541574 |  172 | `	SySetRelease(pArg);` |
|  4541574 |  173 | `	if( rcTerm == PH7_ABORT \|\| rcTerm == PH7_EXCEPTION ){` |
|   616621 |  174 | `		while( pTos >= pState->pStack ){` |
|   309720 |  175 | `			PH7_MemObjRelease(pTos);` |
|   309720 |  176 | `			pTos--;` |
|        5 |  177 | `		}` |
|   153452 |  178 | `	}` |
|  4541574 |  179 | `	return rcTerm;` |
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
|  5366920 |  195 | `static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase)` |
|        5 |  196 | `{` |
|  5366937 |  197 | `	while( SySetUsed(&pVm->aFinallyAction) > nBase ){` |
|       15 |  198 | `		VmFinallyAction *pAct = (VmFinallyAction *)SySetPeek(&pVm->aFinallyAction);` |
|       15 |  199 | `		if( pAct->eKind == PH7_FA_RETHROW && pAct->pExc ){` |
|        9 |  200 | `			PH7_ClassInstanceUnref(pAct->pExc);` |
|       10 |  201 | `		}else if( pAct->eKind == PH7_FA_RETURN ){` |
|        3 |  202 | `			PH7_MemObjRelease(&pAct->sRet);` |
|        1 |  203 | `		}` |
|       15 |  204 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|        3 |  205 | `	}` |
|  5366925 |  206 | `}` |
|        - |  207 | `/*` |
|        - |  208 | ` * Finish one user-function call at the "pop" boundary of the callee's` |
|        - |  209 | ` * activation: pop-time accounting (recursion depth, aSelf), by-ref-return` |
|        - |  210 | ` * fixup, callee-threw routing (inline resume / recorded resume / propagate),` |
|        - |  211 | ` * operand-stack free and frame teardown. Extracted verbatim from the OP_CALL` |
|        - |  212 | ` * epilogue so the trampoline that pops records can run the same` |
|        - |  213 | ` * code when a record is popped at OP_DONE instead of after a native return.` |
|        - |  214 | ` * pCaller->pc / pCaller->pTos are authoritative across this boundary; the` |
|        - |  215 | ` * dispatch loop syncs its locals around the call. Returns PH7_OK (continue` |
|        - |  216 | ` * the caller, possibly at a redirected pc), PH7_ABORT, PH7_SUSPEND (the ctx` |
|        - |  217 | ` * state was re-saved at the caller's level) or PH7_EXCEPTION.` |
|        - |  218 | ` */` |
|   831633 |  219 | `static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)` |
|        5 |  220 | `{` |
|        - |  221 | `	/* Decrement nesting level */` |
|   831638 |  222 | `	pVm->nRecursionDepth--;` |
|   831638 |  223 | `	if( pCallee->bSelfPushed ){` |
|        - |  224 | `		/* Pop class name */` |
|   512181 |  225 | `		(void)SySetPop(&pVm->aSelf);` |
|   256088 |  226 | `	}` |
|   831638 |  227 | `	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
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
|        - |  242 | `		 * measured at ~0.6 KB). A slot that already outlives the frame -- a` |
|        - |  243 | `		 * static, a global, a property -- is pinned harmlessly: the flag only stops` |
|        - |  244 | ``		 * its INDEX being recycled, and `unset($GLOBALS['G'])` still answers php's. */`` |
|       92 |  245 | `		if( pCallee->nLastRef != SXU32_HIGH ){` |
|       85 |  246 | `			VmPinMemObjSlot(&(*pVm),pCallee->nLastRef);` |
|       41 |  247 | `		}` |
|       92 |  248 | `		pCaller->pTos->nIdx = pCallee->nLastRef;` |
|        - |  249 | `		/* The callee PROMISED a reference, whether or not it had one to give:` |
|        - |  250 | `		 * php says nothing further at the call site either way. */` |
|       92 |  251 | `		pCaller->pTos->iFlags \|= MEMOBJ_AUX_REFRET;` |
|       48 |  252 | `	}else{` |
|        - |  253 | `		/* A by-VALUE return is a TEMPORARY — php's IS_TMP_VAR — and must not look like` |
|        - |  254 | `		 * an lvalue. The result lands in the slot the call's first ARGUMENT occupied,` |
|        - |  255 | `		 * which still carried that argument's variable index, so the returned value` |
|        - |  256 | ``		 * inherited it: `f(id($z))` with `function f(&$x)` aliased and overwrote `$z`,`` |
|        - |  257 | `		 * a variable neither function was given by reference. Every call form was` |
|        - |  258 | `		 * affected (function, method, static, closure, nested) and every one of them` |
|        - |  259 | `		 * silently. Clearing it here also lets the call site see the temporary for what` |
|        - |  260 | `		 * it is, which is what php's "Only variables should be passed by reference"` |
|        - |  261 | `		 * notice is raised on. */` |
|   831550 |  262 | `		pCaller->pTos->nIdx = SXU32_HIGH;` |
|        - |  263 | `	}` |
|   831638 |  264 | `	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  265 | `		/* The callee threw (or its finally threw past it). If an in-place catch` |
|        - |  266 | `		 * recorded a resume target owned by THIS caller's body, resume there and` |
|        - |  267 | `		 * consume the target (VmRecordedResume); when the catcher is an outer exec` |
|        - |  268 | `		 * — or this is a callback with no bytecode to resume into — propagate so` |
|        - |  269 | `		 * the owning exec lands. This replaces the old "is the caller's parent a` |
|        - |  270 | `		 * resumable try frame" test, which resumed at the caller's OWN try even` |
|        - |  271 | `		 * when the finally's throw was caught further out, losing that catch's` |
|        - |  272 | `		 * return (ROOT B, face c). */` |
|        - |  273 | `		sxi32 iResumePc;` |
|   611611 |  274 | `		VmFrame *pParentFrame = pCallee->pFrame->pParent;` |
|   611611 |  275 | `		if( !pCaller->is_callback && VmInlineOwnedBy(pVm,pCaller->aInstr,pCaller->pEntryFrame) ){` |
|        - |  276 | `			/* ROOT C: the callee's throw was caught by an inline try in THIS caller` |
|        - |  277 | `			 * (generator body). Drain the operand stack (incl. the unwritten result` |
|        - |  278 | `			 * slot) to the try's base and land at its catch/finally. */` |
|        5 |  279 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iInlineDrain ){` |
|        3 |  280 | `				PH7_MemObjRelease(pCaller->pTos);` |
|        3 |  281 | `				pCaller->pTos--;` |
|        1 |  282 | `			}` |
|        3 |  283 | `			pCaller->pc = (sxi32)pVm->iInlinePc - 1;` |
|        3 |  284 | `			pVm->pInlineInstr = 0;` |
|        3 |  285 | `			pVm->pInlineFrame = 0;` |
|        3 |  286 | `			rc = PH7_OK;` |
|   611610 |  287 | `		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){` |
|        - |  288 | `			/* Pop the result, then drain any abandoned outer-expression operands` |
|        - |  289 | `			 * to the catching try's base (like the inline branch above) — the` |
|        - |  290 | `			 * ops that would have consumed them were abandoned by the throw, and` |
|        - |  291 | `` 			 * leaving them leaks one slot per caught throw (`try { $a = 1 + f(); }` `` |
|        - |  292 | `			 * in a loop overflowed the operand stack). */` |
|   310466 |  293 | `			VmPopOperand(&pCaller->pTos,1);` |
|  1015030 |  294 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iResumeStackDepth ){` |
|   704569 |  295 | `				PH7_MemObjRelease(pCaller->pTos);` |
|   704569 |  296 | `				pCaller->pTos--;` |
|        5 |  297 | `			}` |
|   310466 |  298 | `			pCaller->pc = iResumePc;` |
|   310466 |  299 | `			rc = PH7_OK;` |
|   155210 |  300 | `		}else{` |
|   301148 |  301 | `			if( pParentFrame->pParent ){` |
|   301144 |  302 | `				rc = PH7_EXCEPTION;` |
|   150573 |  303 | `			}else{` |
|        - |  304 | `				/* Continue normal execution */` |
|        6 |  305 | `				rc = PH7_OK;` |
|        - |  306 | `			}` |
|        - |  307 | `		}` |
|   305776 |  308 | `	}` |
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
|   831638 |  319 | `	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){` |
|        - |  320 | `		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee` |
|        - |  321 | `		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly` |
|        - |  322 | `		 * the allocated slot count either way. */` |
|   827028 |  323 | `		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap,pCallee->nLiveTos);` |
|   413716 |  324 | `	}` |
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
|  1142269 |  344 | `		for( pW = pVm->pFrame ; pW && pW != pCallee->pFrame ; pW = pW->pParent ){` |
|   621095 |  345 | `			if( (pW->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|   310464 |  346 | `				break;` |
|        - |  347 | `			}` |
|   155295 |  348 | `		}` |
|   831638 |  349 | `		if( pW == pCallee->pFrame ){` |
|   521339 |  350 | `			while( pVm->pFrame != pCallee->pFrame ){` |
|      164 |  351 | `				VmLeaveFrame(&(*pVm));` |
|        4 |  352 | `			}` |
|   260817 |  353 | `		}` |
|        - |  354 | `	}` |
|   831638 |  355 | `	VmLeaveFrame(&(*pVm));` |
|   831638 |  356 | `	if( rc == PH7_ABORT ){` |
|      623 |  357 | `		return PH7_ABORT;` |
|        - |  358 | `	}` |
|   831020 |  359 | `	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  360 | `		/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  361 | `		 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  362 | `		 * overwriting the state saved by the inner level.` |
|        - |  363 | `		 * pTos points to the result slot (not yet written).` |
|        - |  364 | `		 * Save nTos one below so resume pushes at the result slot. */` |
|      ! 0 |  365 | `		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);` |
|      ! 0 |  366 | `		return PH7_SUSPEND;` |
|        - |  367 | `	}` |
|   831020 |  368 | `	if( rc == PH7_EXCEPTION ){` |
|   301144 |  369 | `		return PH7_EXCEPTION;` |
|        - |  370 | `	}` |
|   529881 |  371 | `	return PH7_OK;` |
|   416026 |  372 | `}` |
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
|  4541565 |  408 | `PH7_PRIVATE sxi32 VmByteCodeExec(` |
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
|  4541570 |  428 | `	if( VmNativeNestingExceeded(pVm) ){` |
|        6 |  429 | `		return VmNativeNestingFatal(pVm);` |
|        - |  430 | `	}` |
|        - |  431 | `	/* A fresh native exec entered while a C-boundary throw is parked` |
|        - |  432 | `	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the` |
|        - |  433 | `	 * very opcode that swallowed the throw) must run CLEAN: the parked status` |
|        - |  434 | `	 * belongs to the interrupted outer exec's fetch-point router, not to this` |
|        - |  435 | `	 * one. Save+clear on entry, merge back on exit — the outer status is` |
|        - |  436 | `	 * restored unless this exec parked its own unconsumed (newer) one, with` |
|        - |  437 | `	 * PH7_ABORT dominating either way. */` |
|  4541566 |  438 | `	nSavedBrc = pVm->nBoundaryRc;` |
|  4541566 |  439 | `	pVm->nBoundaryRc = 0;` |
|        - |  440 | `	/* The executing source line belongs to the ACTIVATION. A nested body -- a called` |
|        - |  441 | `	 * function, but equally an attribute-default or default-argument mini-program --` |
|        - |  442 | `	 * runs its own bytecode with its own lines, so it must not leave the caller` |
|        - |  443 | ``	 * reporting the callee's position: `new Exception` stamped line 1 because the`` |
|        - |  444 | ``	 * class's `protected $message = '';` default ran (from the embedded chunk) between`` |
|        - |  445 | `	 * OP_NEW and the stamp. Save on entry, restore on exit. */` |
|  4541566 |  446 | `	nSavedLine = pVm->nCurLine;` |
|  4541566 |  447 | `	pVm->nVmExecDepth++;` |
|  6812164 |  448 | `	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,` |
|  2270598 |  449 | `		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);` |
|  4541574 |  450 | `	pVm->nVmExecDepth--;` |
|  4541574 |  451 | `	pVm->nCurLine = nSavedLine;` |
|  4541574 |  452 | `	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT \|\| pVm->nBoundaryRc == 0) ){` |
|       43 |  453 | `		pVm->nBoundaryRc = nSavedBrc;` |
|       19 |  454 | `	}` |
|  4541574 |  455 | `	return rc;` |
|  2270605 |  456 | `}` |
|        - |  457 | `/*` |
|        - |  458 | ` * D1 commit 2: allocate a captured lvalue path for a deferred element/property call arg.` |
|        - |  459 | ` * eRoot picks the root container: 0 = a real aMemObj slot (nRootIdx), 1 = an undefined` |
|        - |  460 | ` * variable to vivify by name at resolve time (pName, a VM-lifetime bytecode string, borrowed),` |
|        - |  461 | ` * 2 = a string base (subscripting a string -> php refuses a by-ref bind). The struct owns its` |
|        - |  462 | ` * step array and each step's key/name; PH7_MemObjRelease frees it via VmFreeDeferredPath.` |
|        - |  463 | ` */` |
|    63248 |  464 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName)` |
|        5 |  465 | `{` |
|    63253 |  466 | `	VmDeferredPath *pPath = (VmDeferredPath *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDeferredPath));` |
|    63253 |  467 | `	if( pPath == 0 ){` |
|      ! 0 |  468 | `		return 0;` |
|        - |  469 | `	}` |
|    63253 |  470 | `	SyZero(pPath,sizeof(VmDeferredPath));` |
|    63253 |  471 | `	pPath->pAlloc = &pVm->sAllocator;` |
|    63253 |  472 | `	pPath->eRoot = eRoot;` |
|    63253 |  473 | `	pPath->nRootIdx = nRootIdx;` |
|    63253 |  474 | `	if( eRoot == 1 && pName ){` |
|        8 |  475 | `		pPath->sRootName = *pName; /* borrowed VM-lifetime bytes, not copied */` |
|        3 |  476 | `	}` |
|    63253 |  477 | `	return pPath;` |
|    32250 |  478 | `}` |
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
|        8 |  523 | `			&pPath->pOverClass->sDisp,&pPath->sOverName);` |
|        9 |  524 | `		rcH = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg);` |
|        9 |  525 | `		SyBlobRelease(&sErrMsg);` |
|        9 |  526 | `		return (rcH == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  527 | `	}` |
|       23 |  528 | `	PH7_VmOverloadedElemNotice(&(*pVm),pPath->pOverClass,pVal);` |
|       23 |  529 | `	return SXRET_OK;` |
|       24 |  530 | `}` |
|    63096 |  531 | `static VmDeferStep * VmDeferPathGrow(VmDeferredPath *pPath)` |
|        5 |  532 | `{` |
|    63101 |  533 | `	if( pPath->nStep >= pPath->nAlloc ){` |
|    63089 |  534 | `		sxu32 nNew = pPath->nAlloc ? pPath->nAlloc * 2 : 4;` |
|    95252 |  535 | `		VmDeferStep *aNew = (VmDeferStep *)SyMemBackendRealloc(pPath->pAlloc,pPath->aStep,` |
|    32163 |  536 | `			nNew * sizeof(VmDeferStep));` |
|    63089 |  537 | `		if( aNew == 0 ){` |
|      ! 0 |  538 | `			return 0;` |
|        - |  539 | `		}` |
|    63089 |  540 | `		pPath->aStep = aNew;` |
|    63089 |  541 | `		pPath->nAlloc = nNew;` |
|    32163 |  542 | `	}` |
|    63101 |  543 | `	return &pPath->aStep[pPath->nStep];` |
|    32174 |  544 | `}` |
|        - |  545 | `/* Append an array-element step, deep-copying the index value (the caller releases pKey). */` |
|    62930 |  546 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey)` |
|        5 |  547 | `{` |
|    62935 |  548 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|    62935 |  549 | `	if( pStep == 0 ){` |
|      ! 0 |  550 | `		return SXERR_MEM;` |
|        - |  551 | `	}` |
|    62935 |  552 | `	pStep->isProp = 0;` |
|    62935 |  553 | `	pStep->bAppend = 0;` |
|    62935 |  554 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|    62935 |  555 | `	PH7_MemObjInit(pKey->pVm,&pStep->sKey);` |
|    62935 |  556 | `	PH7_MemObjStore(pKey,&pStep->sKey);` |
|    62935 |  557 | `	pPath->nStep++;` |
|    62935 |  558 | `	return SXRET_OK;` |
|    32091 |  559 | `}` |
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
|        4 |  617 | `{` |
|        - |  618 | `	SyMemBackend *pAlloc;` |
|       94 |  619 | `	if( pCoal == 0 ){` |
|       40 |  620 | `		return;` |
|        - |  621 | `	}` |
|       55 |  622 | `	pAlloc = pCoal->pAlloc;` |
|       55 |  623 | `	PH7_MemObjRelease(&pCoal->sKey);` |
|       55 |  624 | `	SyMemBackendFree(pAlloc,pCoal);` |
|       49 |  625 | `}` |
|        - |  626 | `/*` |
|        - |  627 | ` * Build the pending __call/__callStatic routing OP_MEMBER hands to the OP_CALL that` |
|        - |  628 | ` * follows it, and hang it off the marked carrier slot. One record per routed call, so a` |
|        - |  629 | ` * routed call evaluated inside another routed call's ARGUMENT LIST — which is where they` |
|        - |  630 | ` * now sit, php's order — keeps its own {receiver, class, name}. Takes the receiver` |
|        - |  631 | ` * reference; the record owns it from here.` |
|        - |  632 | ` */` |
|      126 |  633 | `PH7_PRIVATE VmMagicCall * VmMagicCallNew(ph7_vm *pVm,ph7_class_instance *pRecv,` |
|        - |  634 | `	ph7_class *pClass,const SyString *pName)` |
|        4 |  635 | `{` |
|      130 |  636 | `	VmMagicCall *pPend = (VmMagicCall *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmMagicCall));` |
|      130 |  637 | `	if( pPend == 0 ){` |
|      ! 0 |  638 | `		return 0;` |
|        - |  639 | `	}` |
|      130 |  640 | `	pPend->pAlloc = &pVm->sAllocator;` |
|      130 |  641 | `	pPend->pRecv = pRecv;` |
|      130 |  642 | `	pPend->pClass = pClass;` |
|      130 |  643 | `	SyBlobInit(&pPend->sName,&pVm->sAllocator);` |
|      130 |  644 | `	if( pName && pName->nByte > 0 ){` |
|      130 |  645 | `		SyBlobAppend(&pPend->sName,(const void *)pName->zString,pName->nByte);` |
|       63 |  646 | `	}` |
|      130 |  647 | `	if( pRecv ){` |
|       88 |  648 | `		pRecv->iRef++;` |
|       42 |  649 | `	}` |
|      130 |  650 | `	return pPend;` |
|       67 |  651 | `}` |
|      126 |  652 | `PH7_PRIVATE void VmFreeMagicCall(VmMagicCall *pPend)` |
|        4 |  653 | `{` |
|        - |  654 | `	SyMemBackend *pAlloc;` |
|      130 |  655 | `	if( pPend == 0 ){` |
|      ! 0 |  656 | `		return;` |
|        - |  657 | `	}` |
|      130 |  658 | `	pAlloc = pPend->pAlloc;` |
|      130 |  659 | `	if( pPend->pRecv ){` |
|      ! 0 |  660 | `		PH7_ClassInstanceUnref(pPend->pRecv);` |
|      ! 0 |  661 | `	}` |
|      130 |  662 | `	SyBlobRelease(&pPend->sName);` |
|      130 |  663 | `	SyMemBackendFree(pAlloc,pPend);` |
|       67 |  664 | `}` |
|    63248 |  665 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath)` |
|        5 |  666 | `{` |
|        - |  667 | `	sxu32 i;` |
|    63253 |  668 | `	if( pPath == 0 ){` |
|      ! 0 |  669 | `		return;` |
|        - |  670 | `	}` |
|   126349 |  671 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|    63101 |  672 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|    63101 |  673 | `		if( pStep->isProp ){` |
|      157 |  674 | `			if( pStep->zProp ){` |
|      157 |  675 | `				SyMemBackendFree(pPath->pAlloc,pStep->zProp);` |
|       80 |  676 | `			}` |
|    63024 |  677 | `		}else if( !pStep->bAppend ){` |
|        - |  678 | `			/* An append step holds no key at all — its sKey was never initialized. */` |
|    62935 |  679 | `			PH7_MemObjRelease(&pStep->sKey);` |
|    32086 |  680 | `		}` |
|    32174 |  681 | `	}` |
|    63253 |  682 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|      193 |  683 | `		PH7_MemObjRelease(&pPath->sPrefetch);` |
|      193 |  684 | `		if( pPath->zOverName ){` |
|       76 |  685 | `			SyMemBackendFree(pPath->pAlloc,pPath->zOverName);` |
|       36 |  686 | `		}` |
|       94 |  687 | `	}` |
|    63253 |  688 | `	if( pPath->aStep ){` |
|    63089 |  689 | `		SyMemBackendFree(pPath->pAlloc,pPath->aStep);` |
|    32163 |  690 | `	}` |
|    63253 |  691 | `	SyMemBackendFree(pPath->pAlloc,pPath);` |
|    32250 |  692 | `}` |
|        - |  693 | `/* Map an op-handler VmOpRc into the main-loop rc convention used by VmByteCodeExecBody. */` |
|    62998 |  694 | `static sxi32 VmOpRcToExecRc(VmOpRc rcOp)` |
|        5 |  695 | `{` |
|    63003 |  696 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 |  697 | `		return PH7_ABORT;` |
|        - |  698 | `	}` |
|    63001 |  699 | `	if( rcOp == VM_OP_EXCEPTION ){` |
|       23 |  700 | `		return PH7_EXCEPTION;` |
|        - |  701 | `	}` |
|    62981 |  702 | `	return SXRET_OK;` |
|    32125 |  703 | `}` |
|        - |  704 | `/*` |
|        - |  705 | ` * D1 commit 2: re-drive ONE captured lvalue step by invoking the real LOAD_IDX / MEMBER` |
|        - |  706 | ` * handler on a synthetic 2-slot stack. This reuses the proven COW / vivify / warning / magic` |
|        - |  707 | ` * machinery instead of hand-walking it. pBase carries the current container (its nIdx must be` |
|        - |  708 | ` * a real aMemObj slot for a by-ref write to vivify in place). iP2 selects the mode:` |
|        - |  709 | ` * LOAD_IDX 1=write(vivify,by-ref) / 0=read(by-value); MEMBER PH7_MEMBER_READ=by-value read.` |
|        - |  710 | ` * On success pOut receives the result value and its nIdx (the aliasable element slot for a` |
|        - |  711 | ` * vivified by-ref element).` |
|        - |  712 | ` */` |
|    62998 |  713 | `static sxi32 VmReDriveStep(ph7_vm *pVm,sxi32 iOp,sxu32 iP2,ph7_value *pBase,ph7_value *pKey,ph7_value *pOut)` |
|        5 |  714 | `{` |
|        - |  715 | `	ph7_value mini[2];` |
|        - |  716 | `	VmInstr aI[2];` |
|        - |  717 | `	VmExecState st;` |
|        - |  718 | `	VmOpRc rcOp;` |
|        - |  719 | ``	/* A NULL key is the APPEND form (`$a[]`): LOAD_IDX takes no index operand, so the`` |
|        - |  720 | `	 * base is the whole stack and iP1 says so. */` |
|    63003 |  721 | `	int bAppend = (pKey == 0);` |
|    63003 |  722 | `	PH7_MemObjInit(pVm,&mini[0]);` |
|    63003 |  723 | `	PH7_MemObjInit(pVm,&mini[1]);` |
|    63003 |  724 | `	PH7_MemObjLoad(pBase,&mini[0]);` |
|    63003 |  725 | `	mini[0].nIdx = pBase->nIdx;` |
|    63003 |  726 | `	if( !bAppend ){` |
|    62993 |  727 | `		PH7_MemObjStore(pKey,&mini[1]);` |
|    32115 |  728 | `	}` |
|    63003 |  729 | `	SyZero((void *)aI,sizeof(aI));` |
|        - |  730 | `	/* LOAD_IDX: iP1=1 means "an index is present". MEMBER: iP1=0 means an INSTANCE member` |
|        - |  731 | ``	 * (iP1=1 would be a static `::` access). */`` |
|    63003 |  732 | `	aI[0].iOp = (sxu8)iOp; aI[0].iP1 = (iOp == PH7_OP_LOAD_IDX && !bAppend) ? 1 : 0; aI[0].iP2 = iP2;` |
|    63003 |  733 | `	SyZero((void *)&st,sizeof(st));` |
|    63003 |  734 | `	st.pStack = mini; st.pTos = bAppend ? &mini[0] : &mini[1]; st.aInstr = aI; st.pc = 0;` |
|    63003 |  735 | `	if( iOp == PH7_OP_LOAD_IDX ){` |
|    62929 |  736 | `		rcOp = VmExecOpLoadIdx(&(*pVm),&st,&aI[0]);` |
|    32088 |  737 | `	}else{` |
|       77 |  738 | `		rcOp = VmExecOpMember(&(*pVm),&st,&aI[0]);` |
|        - |  739 | `	}` |
|        - |  740 | `	/* The handler popped the key/name and left the result in the (now top) base slot.` |
|        - |  741 | `	 * DEEP-COPY it into pOut (MemObjStore, not MemObjLoad): the result is chained as the next` |
|        - |  742 | `	 * step's base and must OWN its buffer — mini[0] is released immediately below, and a` |
|        - |  743 | `	 * read-only view (MemObjLoad) would leave pOut dangling into freed memory. */` |
|    63003 |  744 | `	PH7_MemObjStore(st.pTos,pOut);` |
|    63003 |  745 | `	pOut->nIdx = st.pTos->nIdx;` |
|    63003 |  746 | `	PH7_MemObjRelease(&mini[0]);` |
|    63003 |  747 | `	return VmOpRcToExecRc(rcOp);` |
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
|      ! 0 |  813 | `				return VmThrowNativeNoWrite(&(*pVm),PH7_VmAttrOwner(pAttr),pAttr->pAttr);` |
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
|        1 |  855 | `				&pClass->sDisp,pName);` |
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
|        - |  869 | `		 * through and creates an ordinary dynamic property instead — which is the scope policy's` |
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
|      ! 0 |  880 | `				&pClass->sDisp,pName);` |
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
|      ! 0 |  899 | `			pDecl = 0;   /* never held: php creates a dynamic property, PHL refuses (the scope policy) */` |
|      ! 0 |  900 | `		}` |
|       47 |  901 | `		if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      ! 0 |  902 | `			VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pAttr);` |
|       47 |  903 | `		}else if( VmClassAllowsDynamicProps(&(*pVm),pClass) ){` |
|       39 |  904 | `			PH7_VmCreateDynamicAttr(&(*pVm),pThis,pName->zString,pName->nByte,&pAttr);` |
|       21 |  905 | `		}else{` |
|        - |  906 | `			SyBlob sMsg;` |
|        - |  907 | `			sxi32 rcT;` |
|        9 |  908 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        9 |  909 | `			SyBlobFormat(&sMsg,"Cannot create dynamic property %z::$%z",&pClass->sDisp,pName);` |
|        9 |  910 | `			rcT = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        9 |  911 | `			SyBlobRelease(&sMsg);` |
|        9 |  912 | `			*pbNoBind = 1;` |
|        9 |  913 | `			return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
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
|       44 |  946 | `				PH7_MemObjRelease(&sMagicVal);` |
|       44 |  947 | `				return rc;` |
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
|    63082 | 1007 | `static sxi32 VmWalkStepsOverValue(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,` |
|        - | 1008 | `	ph7_value *pCur,int bWrite,ph7_value *pSlot)` |
|        5 | 1009 | `{` |
|    63087 | 1010 | `	sxi32 rc = SXRET_OK;` |
|        - | 1011 | `	sxu32 i;` |
|   125993 | 1012 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|    62935 | 1013 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|        - | 1014 | `		ph7_value out;` |
|    62935 | 1015 | `		PH7_MemObjInit(&(*pVm),&out);` |
|    62935 | 1016 | `		if( pStep->isProp && bWrite && (pCur->iFlags & MEMOBJ_OBJ) ){` |
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
|    62933 | 1043 | `		if( pStep->isProp ){` |
|        - | 1044 | `			ph7_value nameVal;` |
|       77 | 1045 | `			PH7_MemObjInitFromString(&(*pVm),&nameVal,&pStep->sProp);` |
|       77 | 1046 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_MEMBER,PH7_MEMBER_READ,pCur,&nameVal,&out);` |
|       77 | 1047 | `			PH7_MemObjRelease(&nameVal);` |
|    62896 | 1048 | `		}else if( pStep->bAppend ){` |
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
|    62857 | 1065 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,bWrite ? 1 : 0,pCur,&pStep->sKey,&out);` |
|        - | 1066 | `		}` |
|    62931 | 1067 | `		PH7_MemObjRelease(pCur);` |
|    62931 | 1068 | `		*pCur = out;` |
|    62931 | 1069 | `		if( rc != SXRET_OK ){` |
|       23 | 1070 | `			return rc;` |
|        - | 1071 | `		}` |
|    32079 | 1072 | `	}` |
|    63063 | 1073 | `	PH7_MemObjStore(pCur,pSlot);` |
|    63063 | 1074 | `	pSlot->nIdx = SXU32_HIGH;` |
|    63063 | 1075 | `	return SXRET_OK;` |
|    32167 | 1076 | `}` |
|        - | 1077 | `/* Resolve a captured lvalue path as a BY-REF target: vivify the whole chain in place and` |
|        - | 1078 | ` * leave pSlot->nIdx pointing at the terminal (aliasable) slot for the by-ref binder. */` |
|      202 | 1079 | `static sxi32 VmResolvePathByRef(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        3 | 1080 | `{` |
|        - | 1081 | `	sxu32 nCur;` |
|        - | 1082 | `	sxi32 rc;` |
|      205 | 1083 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
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
|      159 | 1098 | `	if( pPath->eRoot == 2 ){` |
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
|      104 | 1138 | `}` |
|        - | 1139 | `/* Resolve a captured lvalue path as a BY-VALUE argument: read the chain (emitting php's` |
|        - | 1140 | ` * undefined-key/property/offset warnings) WITHOUT vivifying, leaving the terminal value in` |
|        - | 1141 | ` * pSlot. Re-drives the read handlers so the warning sequence matches php exactly. */` |
|    63044 | 1142 | `static sxi32 VmResolvePathByValue(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        5 | 1143 | `{` |
|        - | 1144 | `	ph7_value cur;` |
|        - | 1145 | `	sxi32 rc;` |
|    63049 | 1146 | `	PH7_MemObjInit(&(*pVm),&cur);` |
|    63049 | 1147 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|        - | 1148 | `		/* The accessor already ran, where php runs it: a by-VALUE argument simply takes` |
|        - | 1149 | `		 * what it answered, in silence. */` |
|      145 | 1150 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|    62979 | 1151 | `	}else if( pPath->eRoot == 1 ){` |
|      ! 0 | 1152 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,FALSE);` |
|      ! 0 | 1153 | `		if( pRoot == 0 ){` |
|      ! 0 | 1154 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&pPath->sRootName);` |
|      ! 0 | 1155 | `		}else{` |
|      ! 0 | 1156 | `			PH7_MemObjLoad(pRoot,&cur);` |
|      ! 0 | 1157 | `			cur.nIdx = pRoot->nIdx;` |
|        - | 1158 | `		}` |
|      ! 0 | 1159 | `	}else{` |
|    62909 | 1160 | `		ph7_value *pRoot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pPath->nRootIdx);` |
|    62909 | 1161 | `		if( pRoot ){` |
|    62909 | 1162 | `			PH7_MemObjLoad(pRoot,&cur);` |
|    62909 | 1163 | `			cur.nIdx = pRoot->nIdx;` |
|    32073 | 1164 | `		}` |
|        - | 1165 | `	}` |
|    63049 | 1166 | `	rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,FALSE,pSlot);` |
|    63049 | 1167 | `	PH7_MemObjRelease(&cur);` |
|    63049 | 1168 | `	return rc;` |
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
|     8186 | 1188 | `PH7_PRIVATE int PH7_VmArgRefusedByRef(VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1189 | `{` |
|     8191 | 1190 | `	if( pMap && pMap->bArgShapes && nPos < 31 ){` |
|     7903 | 1191 | `		return (pMap->nNonLvalMask & (1u << nPos)) != 0;` |
|        - | 1192 | `	}` |
|      293 | 1193 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|      229 | 1194 | `		return 0;` |
|        - | 1195 | `	}` |
|       96 | 1196 | `	return (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|       64 | 1197 | `	    && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0;` |
|     4094 | 1198 | `}` |
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
|     3996 | 1219 | `PH7_PRIVATE void PH7_VmByRefArgWriteBack(ph7_vm *pVm,ph7_value *pArg,sxi32 iPreFlags)` |
|        5 | 1220 | `{` |
|        - | 1221 | `	ph7_value *pSlot;` |
|     3996 | 1222 | `	if( pArg->nIdx == SXU32_HIGH` |
|     4001 | 1223 | `	 \|\| (pArg->iFlags & MEMOBJ_ALL) == (iPreFlags & MEMOBJ_ALL) ){` |
|     3977 | 1224 | `		return;` |
|        - | 1225 | `	}` |
|       25 | 1226 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArg->nIdx);` |
|       25 | 1227 | `	if( pSlot && pSlot != pArg ){` |
|       25 | 1228 | `		PH7_MemObjStore(pArg,pSlot);` |
|       12 | 1229 | `	}` |
|     1999 | 1230 | `}` |
|    41931 | 1231 | `PH7_PRIVATE void PH7_VmArgTempCallNotice(ph7_vm *pVm,VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1232 | `{` |
|    41936 | 1233 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| nPos >= 31 ){` |
|      293 | 1234 | `		return;` |
|        - | 1235 | `	}` |
|    41648 | 1236 | `	if( (pMap->nTempCallMask & (1u << nPos)) == 0 ){` |
|    41608 | 1237 | `		return;` |
|        - | 1238 | `	}` |
|       43 | 1239 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|        3 | 1240 | `		return; /* a by-reference RETURN: php is silent and binds it */` |
|        - | 1241 | `	}` |
|       41 | 1242 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Only variables should be passed by reference");` |
|    20956 | 1243 | `}` |
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
|      162 | 1256 | `static sxi32 VmScreenGenByRefArgs(ph7_vm *pVm,ph7_vm_func *pFunc,VmCallArgMap *pMap,` |
|        - | 1257 | `	ph7_value *pArg,sxu32 nActual,ph7_class *pSelfHint)` |
|        5 | 1258 | `{` |
|      167 | 1259 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      167 | 1260 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|        - | 1261 | `	sxu32 i;` |
|      357 | 1262 | `	for( i = 0 ; i < nActual ; ++i ){` |
|      201 | 1263 | `		sxu32 n = i;` |
|      201 | 1264 | `		if( pMap && pMap->bHasNamed && i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|      110 | 1265 | `			for( n = 0 ; n < nFormal ; ++n ){` |
|      104 | 1266 | `				if( pMap->aNames[i].nByte == SyStringLength(&aFormal[n].sName)` |
|      105 | 1267 | `				 && SyMemcmp(pMap->aNames[i].zString,SyStringData(&aFormal[n].sName),` |
|      147 | 1268 | `					pMap->aNames[i].nByte) == 0 ){` |
|       62 | 1269 | `					break;` |
|        - | 1270 | `				}` |
|       27 | 1271 | `			}` |
|       30 | 1272 | `		}` |
|      201 | 1273 | `		if( n >= nFormal \|\| (aFormal[n].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|      157 | 1274 | `			continue;` |
|        - | 1275 | `		}` |
|       47 | 1276 | `		if( PH7_VmArgRefusedByRef(pMap,i,&pArg[i]) ){` |
|       10 | 1277 | `			sxi32 rcT = VmThrowByRefRefusal(&(*pVm),` |
|        6 | 1278 | `				(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|        6 | 1279 | `				&pFunc->sName,pFunc,n + 1,&aFormal[n].sName);` |
|        7 | 1280 | `			return (rcT == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1281 | `		}` |
|       41 | 1282 | `		PH7_VmArgTempCallNotice(&(*pVm),pMap,i,&pArg[i]);` |
|       22 | 1283 | `	}` |
|      161 | 1284 | `	return SXRET_OK;` |
|       86 | 1285 | `}` |
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
|  8685628 | 1311 | `PH7_PRIVATE sxi32 PH7_VmResolveDeferredArgs(` |
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
|  8685633 | 1323 | `	sxu32 n = 0;` |
| 20251803 | 1324 | `	for( p = pArg ; p < pTos ; ++p, ++n ){` |
| 11566281 | 1325 | `		int bByRef = 0;` |
| 11566281 | 1326 | `		int bDeferred = (p->iFlags & (MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH)) != 0;` |
|        - | 1327 | `		SyString sName;` |
| 11566276 | 1328 | `		if( !bDeferred` |
| 11535197 | 1329 | `		 && (bAllByValue \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0) ){` |
|        - | 1330 | `			/* Nothing deferred in this position, and no slot in the VM could refuse` |
|        - | 1331 | `			 * being aliased -- the ordinary case, out through one test. */` |
|  6951818 | 1332 | `			continue;` |
|        - | 1333 | `		}` |
|  9227552 | 1334 | `		if( bAllByValue ){` |
|      ! 0 | 1335 | `			bByRef = 0;` |
|  9227552 | 1336 | `		}else if( bAllByRef ){` |
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
|  9227552 | 1348 | `		}else if( pFormal ){` |
|   109140 | 1349 | `			sxu32 idx = n;` |
|   109135 | 1350 | `			if( pCallMap && pCallMap->bHasNamed && n < pCallMap->nTotal` |
|     1359 | 1351 | `			 && pCallMap->aNames[n].nByte > 0 ){` |
|        - | 1352 | `				/* A NAMED actual binds to the formal its NAME picks, not to the one at its` |
|        - | 1353 | ``				 * stack position: `r(x: $a["k"])` is argument #1 on the stack and parameter`` |
|        - | 1354 | `				 * $x in the declaration. Reading the by-ref-ness positionally consulted the` |
|        - | 1355 | `				 * wrong formal, so a by-reference named argument naming a missing element` |
|        - | 1356 | ``				 * warned `Undefined array key` and passed NULL where php creates it. */`` |
|        - | 1357 | `				sxu32 f;` |
|      661 | 1358 | `				idx = SXU32_HIGH;` |
|     1225 | 1359 | `				for( f = 0 ; f < nFormal ; ++f ){` |
|     1032 | 1360 | `					if( pCallMap->aNames[n].nByte == SyStringLength(&pFormal[f].sName)` |
|      947 | 1361 | `					 && SyMemcmp(pCallMap->aNames[n].zString,` |
|     1278 | 1362 | `						SyStringData(&pFormal[f].sName),pCallMap->aNames[n].nByte) == 0 ){` |
|      472 | 1363 | `						idx = f;` |
|      472 | 1364 | `						break;` |
|        - | 1365 | `					}` |
|      287 | 1366 | `				}` |
|   108812 | 1367 | `			}else if( idx >= nFormal ){` |
|        - | 1368 | `				/* Beyond the declared formals: a trailing variadic absorbs the tail` |
|        - | 1369 | `				 * (and dictates its by-ref-ness); otherwise the extra arg is by-value. */` |
|    10737 | 1370 | `				idx = (nFormal > 0 && (pFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC))` |
|     8103 | 1371 | `					? nFormal - 1 : SXU32_HIGH;` |
|     3544 | 1372 | `			}` |
|   109140 | 1373 | `			if( idx != SXU32_HIGH ){` |
|   103644 | 1374 | `				bByRef = (pFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|    51773 | 1375 | `			}` |
|    54501 | 1376 | `		}else{` |
|  9118417 | 1377 | `			bByRef = (n < 31 && (nByRefMask & (1u << n))) ? 1 : 0;` |
|        - | 1378 | `		}` |
|  9227552 | 1379 | `		if( !bDeferred ){` |
|        - | 1380 | `			/* An argument that ALREADY resolved to a slot, in a by-reference` |
|        - | 1381 | `			 * position: php screens the property behind it here, because binding` |
|        - | 1382 | `			 * the callee's parameter to it is an INDIRECT modification -- the` |
|        - | 1383 | `			 * callee's write would reach a readonly property, or a native one whose` |
|        - | 1384 | `			 * handler refuses every write, with nothing in the way. This is the one` |
|        - | 1385 | `			 * place both kinds of callee agree on: a user function's formals and a` |
|        - | 1386 | `			 * builtin's signature-derived mask both arrive here. */` |
|  9164142 | 1387 | `			if( bByRef ){` |
|    39267 | 1388 | `				sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),p->nIdx);` |
|    39267 | 1389 | `				if( rcInd != SXRET_OK ){` |
|       63 | 1390 | `					return rcInd;` |
|        - | 1391 | `				}` |
|    19614 | 1392 | `			}` |
|  9164124 | 1393 | `			continue;` |
|        - | 1394 | `		}` |
|    63415 | 1395 | `		if( p->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|        - | 1396 | `			/* D1 commit 2: a deferred array-element/property lvalue. Detach the descriptor` |
|        - | 1397 | `			 * FIRST (so an exception mid-resolve, or a later stack release, cannot double-free` |
|        - | 1398 | `			 * it) then re-walk it in the chosen mode. */` |
|    63251 | 1399 | `			VmDeferredPath *pPath = (VmDeferredPath *)p->x.pOther;` |
|        - | 1400 | `			sxi32 rc;` |
|    63251 | 1401 | `			p->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|    63251 | 1402 | `			p->x.pOther = 0;` |
|    63251 | 1403 | `			MemObjSetType(p,MEMOBJ_NULL);` |
|    63251 | 1404 | `			p->nIdx = SXU32_HIGH;` |
|    63251 | 1405 | `			if( bByRef ){` |
|      205 | 1406 | `				rc = VmResolvePathByRef(&(*pVm),pPath,p);` |
|      104 | 1407 | `			}else{` |
|    63049 | 1408 | `				rc = VmResolvePathByValue(&(*pVm),pPath,p);` |
|        - | 1409 | `			}` |
|    63251 | 1410 | `			VmFreeDeferredPath(pPath);` |
|    63251 | 1411 | `			if( rc != SXRET_OK ){` |
|       92 | 1412 | `				return rc;` |
|        - | 1413 | `			}` |
|    63163 | 1414 | `			continue;` |
|        - | 1415 | `		}` |
|        - | 1416 | `		/* Recover the deferred variable name and drop the marker + carrier. */` |
|      168 | 1417 | `		SyStringInitFromBuf(&sName,(const char *)p->x.pOther,` |
|        - | 1418 | `			p->x.pOther ? SyStrlen((const char *)p->x.pOther) : 0);` |
|      168 | 1419 | `		p->iFlags &= ~MEMOBJ_AUX_DEFERRED;` |
|      168 | 1420 | `		p->x.pOther = 0;` |
|      168 | 1421 | `		if( bByRef ){` |
|        - | 1422 | `			/* Materialize in the caller frame; the value stays NULL, only the slot` |
|        - | 1423 | `			 * back-reference (nIdx) matters for the by-ref binder / write-back. */` |
|      155 | 1424 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      155 | 1425 | `			if( pObj ){` |
|      155 | 1426 | `				p->nIdx = pObj->nIdx;` |
|       76 | 1427 | `			}` |
|       79 | 1428 | `		}else{` |
|        - | 1429 | `			/* php warns and passes NULL without creating the variable; the slot is` |
|        - | 1430 | `			 * already a clean NULL with nIdx == SXU32_HIGH from the deferred load. */` |
|       14 | 1431 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        - | 1432 | `		}` |
|       86 | 1433 | `	}` |
|  8685527 | 1434 | `	return SXRET_OK;` |
|  4344096 | 1435 | `}` |
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
|      222 | 1448 | `PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore)` |
|        5 | 1449 | `{` |
|      227 | 1450 | `	return pVm->nBoundaryRc != nBrcBefore \|\| (const void *)pVm->pResumeFrame != pResumeBefore;` |
|        5 | 1451 | `}` |
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
|   200280 | 1472 | `static const char * VmCallableClassMethodError(` |
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
|   200284 | 1491 | `	int bFallback = bStaticForm && PH7_VmStaticFallbackThis(&(*pVm),pClass) != 0;` |
|   200284 | 1492 | `	if( pClass == 0 ){` |
|       49 | 1493 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|       49 | 1494 | `		return zBuf;` |
|        - | 1495 | `	}` |
|   200238 | 1496 | `	pMethod = PH7_ClassExtractMethod(pClass,zMeth,nMeth);` |
|   200238 | 1497 | `	if( pMethod == 0 ){` |
|       85 | 1498 | `		if( bFallback ){` |
|        7 | 1499 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|        2 | 1500 | `				&pClass->sDisp,(int)nMeth,zMeth);` |
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
|       14 | 1511 | `			&pClass->sDisp,(int)nMeth,zMeth);` |
|       29 | 1512 | `		return zBuf;` |
|        - | 1513 | `	}` |
|        - | 1514 | `	/* An ABSTRACT method (an interface's included) has no body to call — the same message` |
|        - | 1515 | ``	 * the `C::m()` SYNTAX raises in vm_ops_oo.c, which this dispatch reached only as a`` |
|        - | 1516 | `	 * mangled internal function name. */` |
|   200154 | 1517 | `	SyStringInitFromBuf(&sMeth,zMeth,nMeth);` |
|   200154 | 1518 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        7 | 1519 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%z()",&pClass->sDisp,&sMeth);` |
|        7 | 1520 | `		return zBuf;` |
|        - | 1521 | `	}` |
|        - | 1522 | `	/* Named through a class NAME, a non-static method is never callable: php refuses even` |
|        - | 1523 | `	 * when the CALLER has a compatible $this (unlike call_user_func, which binds it). The` |
|        - | 1524 | `	 * message names the OWNING class and the method's declared spelling. */` |
|        - | 1525 | `	{` |
|        - | 1526 | `		SyString sDecl;` |
|        - | 1527 | `		int bAccessible;` |
|   200148 | 1528 | `		SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1529 | `			SyStringLength(&pMethod->sFunc.sName));` |
|   300239 | 1530 | `		bAccessible = pMethod->iProtection == PH7_CLASS_PROT_PUBLIC` |
|   200144 | 1531 | `			\|\| PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       19 | 1532 | `				&sDecl,pMethod->iProtection,FALSE);` |
|   200148 | 1533 | `		if( !bAccessible && bFallback ){` |
|        - | 1534 | `			/* Inaccessible goes the same way as missing: php never reports the visibility,` |
|        - | 1535 | `			 * because the name resolved to the trampoline before visibility could matter. */` |
|       13 | 1536 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|        4 | 1537 | `				&pClass->sDisp,(int)nMeth,zMeth);` |
|       17 | 1538 | `			return zBuf;` |
|        - | 1539 | `		}` |
|   200140 | 1540 | `		if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 && bAccessible ){` |
|        - | 1541 | `			/* Deciding class vs NAMED class: a trait is php's compile-time construct, so` |
|        - | 1542 | `			 * every message names the class that composed it (PH7_VmMethodScopeName). */` |
|       25 | 1543 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|       16 | 1544 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|       17 | 1545 | `			return zBuf;` |
|        - | 1546 | `		}` |
|        - | 1547 | `	}` |
|   200124 | 1548 | `	return 0;` |
|   100144 | 1549 | `}` |
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
|        1 | 1563 | `			zVis,&pDecl->sDisp,(int)nMeth,zMeth,&pScope->sDisp);` |
|        2 | 1564 | `	}else{` |
|       35 | 1565 | `		SyBufferFormat(zBuf,nBuf,"Call to %s method %z::%.*s() from global scope",` |
|       11 | 1566 | `			zVis,&pDecl->sDisp,(int)nMeth,zMeth);` |
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
|        2 | 1642 | `			&pClass->sDisp,(int)nMeth,zMeth);` |
|        5 | 1643 | `		return zBuf;` |
|        - | 1644 | `	}` |
|      167 | 1645 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1646 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      167 | 1647 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1648 | `		/* Named through the CLASS only: an instance of an abstract class cannot exist, so` |
|        - | 1649 | `		 * the object spelling never reaches an abstract body. */` |
|        7 | 1650 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%.*s()",` |
|        2 | 1651 | `			&pClass->sDisp,(int)nMeth,zMeth);` |
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
|   100202 | 1686 | `static const char * VmDirectArrayCallableError(ph7_vm *pVm,ph7_value *pTarget,ph7_value *pMethod,` |
|        - | 1687 | `	char *zBuf,int nBuf)` |
|        4 | 1688 | `{` |
|        - | 1689 | `	ph7_class *pClass;` |
|   100206 | 1690 | `	if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|       11 | 1691 | `		return "First array member is not a valid class name or object";` |
|        - | 1692 | `	}` |
|   100196 | 1693 | `	if( (pMethod->iFlags & MEMOBJ_STRING) == 0 ){` |
|        9 | 1694 | `		return "Second array member is not a valid method";` |
|        - | 1695 | `	}` |
|   100188 | 1696 | `	pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   150280 | 1697 | `	return VmCallableClassMethodError(&(*pVm),pClass,` |
|   100184 | 1698 | `		(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|   100184 | 1699 | `		(const char *)SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob),` |
|        - | 1700 | `		/* An OBJECT target carries its own $this; only a class NAME is the static form. */` |
|   100184 | 1701 | `		(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|    50092 | 1702 | `		zBuf,nBuf);` |
|    50105 | 1703 | `}` |
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
|   100266 | 1719 | `static ph7_vm_func * VmIndirectCalleeFunc(ph7_vm *pVm,ph7_value *pCallable)` |
|        5 | 1720 | `{` |
|   100271 | 1721 | `	ph7_class_method *pMeth = 0;` |
|   100271 | 1722 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|   100271 | 1723 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|   100271 | 1724 | `		ph7_value *pTarget = 0,*pName = 0;` |
|        - | 1725 | `		ph7_class *pClass;` |
|   100266 | 1726 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|   100266 | 1727 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|   100271 | 1728 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1729 | `			return 0;` |
|        - | 1730 | `		}` |
|   100271 | 1731 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   100271 | 1732 | `		if( pClass == 0 ){` |
|      ! 0 | 1733 | `			return 0;` |
|        - | 1734 | `		}` |
|   150404 | 1735 | `		pMeth = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|   100266 | 1736 | `			SyBlobLength(&pName->sBlob));` |
|    50133 | 1737 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1738 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 1739 | `		if( pThis == 0 ){` |
|      ! 0 | 1740 | `			return 0;` |
|        - | 1741 | `		}` |
|      ! 0 | 1742 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 | 1743 | `	}` |
|   100271 | 1744 | `	return pMeth ? &pMeth->sFunc : 0;` |
|    50138 | 1745 | `}` |
|        - | 1746 | `/*` |
|        - | 1747 | ` * Materialize an indirect dispatch's deferred arguments against that callee — the same` |
|        - | 1748 | ` * split OP_CALL makes for a direct one: a native method's by-ref positions come from its` |
|        - | 1749 | ` * signature mask, a PHP one's from its compiled formals, and an unresolved callee binds` |
|        - | 1750 | ` * everything by value (php's answer for the magic route it is about to take).` |
|        - | 1751 | ` */` |
|   100266 | 1752 | `static sxi32 VmResolveIndirectArgs(ph7_vm *pVm,ph7_value *pCallable,ph7_value *pArg,ph7_value *pTos,` |
|        - | 1753 | `	VmCallArgMap *pCallMap)` |
|        5 | 1754 | `{` |
|   100271 | 1755 | `	ph7_vm_func *pFn = VmIndirectCalleeFunc(&(*pVm),pCallable);` |
|   100271 | 1756 | `	if( pFn == 0 ){` |
|       45 | 1757 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pCallMap);` |
|        - | 1758 | `	}` |
|   100227 | 1759 | `	if( pFn->iFlags & VM_FUNC_NATIVE ){` |
|        9 | 1760 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|        8 | 1761 | `			pFn->pNative ? pFn->pNative->nByRefMask : 0,0,0,pCallMap);` |
|        - | 1762 | `	}` |
|   150326 | 1763 | `	return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   100214 | 1764 | `		(ph7_vm_func_arg *)SySetBasePtr(&pFn->aArgs),SySetUsed(&pFn->aArgs),0,0,0,pCallMap);` |
|    50138 | 1765 | `}` |
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
|      102 | 1777 | `static const char * VmFccValueError(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|        4 | 1778 | `{` |
|      106 | 1779 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
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
|       84 | 1812 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       19 | 1813 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|       19 | 1814 | `		if( pObj == 0 ){` |
|      ! 0 | 1815 | `			return "Value of type object is not callable";` |
|        - | 1816 | `		}` |
|       19 | 1817 | `		if( PH7_ClassExtractMethod(pObj->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|      ! 0 | 1818 | `			return 0;` |
|        - | 1819 | `		}` |
|       19 | 1820 | `		SyBufferFormat(zBuf,nBuf,"Object of type %z is not callable",&pObj->pClass->sDisp);` |
|       19 | 1821 | `		return zBuf;` |
|        - | 1822 | `	}` |
|       66 | 1823 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       52 | 1824 | `		const char *zCls = 0,*zMeth = 0;` |
|       52 | 1825 | `		sxu32 nCls = 0,nMeth = 0;` |
|        - | 1826 | `		SyString sName;` |
|       52 | 1827 | `		SyStringInitFromBuf(&sName,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|        - | 1828 | `		/* A leading backslash only anchors the name to the global namespace. */` |
|       52 | 1829 | `		if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|      ! 0 | 1830 | `			sName.zString++;` |
|      ! 0 | 1831 | `			sName.nByte--;` |
|      ! 0 | 1832 | `		}` |
|       52 | 1833 | `		if( PH7_VmCallableStringParts(sName.zString,sName.nByte,&zCls,&nCls,&zMeth,&nMeth) ){` |
|        - | 1834 | `			/* "Class::method" carries the class/method taxonomy, not the function one. */` |
|        7 | 1835 | `			return VmCallableClassMethodError(&(*pVm),` |
|        2 | 1836 | `				PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0),` |
|        2 | 1837 | `				zCls,nCls,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|        - | 1838 | `		}` |
|       48 | 1839 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined function %z()",&sName);` |
|       48 | 1840 | `		return zBuf;` |
|        - | 1841 | `	}` |
|       16 | 1842 | `	SyBufferFormat(zBuf,nBuf,"Value of type %s is not callable",VmArithTypeName(pValue));` |
|       16 | 1843 | `	return zBuf;` |
|       55 | 1844 | `}` |
|        - | 1845 | `/*` |
|        - | 1846 | `` * Split a `"Class::method"` callable string. php scans for the LAST "::" (zend_memrchr),`` |
|        - | 1847 | `` * so `"C::s::x"` names the class `C::s` — not `C` — and reports it not found. Returns TRUE`` |
|        - | 1848 | `` * and the two halves (either may be empty: `"C::"` and `"::s"` are shapes php accepts here`` |
|        - | 1849 | ` * and rejects further down), FALSE when the string carries no "::" at all.` |
|        - | 1850 | ` */` |
|  1655648 | 1851 | `PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,` |
|        - | 1852 | `	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth)` |
|        5 | 1853 | `{` |
|        - | 1854 | `	sxu32 i;` |
| 11265204 | 1855 | `	for( i = nName ; i >= 2 ; --i ){` |
|  9809754 | 1856 | `		if( zName[i-2] == ':' && zName[i-1] == ':' ){` |
|   200203 | 1857 | `			*pzCls = zName;` |
|   200203 | 1858 | `			*pnCls = i - 2;` |
|   200203 | 1859 | `			*pzMeth = &zName[i];` |
|   200203 | 1860 | `			*pnMeth = nName - i;` |
|   200203 | 1861 | `			return TRUE;` |
|        - | 1862 | `		}` |
|  4801334 | 1863 | `	}` |
|  1455455 | 1864 | `	return FALSE;` |
|   827321 | 1865 | `}` |
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
|    18167 | 1883 | `static void VmDefaultScopeEnter(ph7_vm *pVm,VmFrame *pFrame,ph7_vm_func *pVmFunc,` |
|        - | 1884 | `	ph7_class *pSelf,VmDefaultScope *pSave)` |
|        5 | 1885 | `{` |
|    18172 | 1886 | `	pSave->pClass = pVm->pConstEvalClass;` |
|    18172 | 1887 | `	pSave->pFrame = pVm->pConstEvalFrame;` |
|    18172 | 1888 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pVmFunc->pUserData ){` |
|      385 | 1889 | `		pVm->pConstEvalClass = PH7_VmMemberOwnerClass((ph7_class *)pVmFunc->pUserData,` |
|      127 | 1890 | `			pSelf ? pSelf : (ph7_class *)pVmFunc->pUserData);` |
|      258 | 1891 | `		pVm->pConstEvalFrame = (void *)pFrame;` |
|      127 | 1892 | `	}` |
|    18172 | 1893 | `}` |
|    18167 | 1894 | `static void VmDefaultScopeLeave(ph7_vm *pVm,VmDefaultScope *pSave)` |
|        5 | 1895 | `{` |
|    18172 | 1896 | `	pVm->pConstEvalClass = pSave->pClass;` |
|    18172 | 1897 | `	pVm->pConstEvalFrame = pSave->pFrame;` |
|    18172 | 1898 | `}` |
|  4541553 | 1899 | `static sxi32 VmByteCodeExecBody(` |
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
|  4541558 | 1925 | `	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record` |
|        - | 1926 | `	                            * stack (BYTECODE stage 2); NULL = executing the` |
|        - | 1927 | `	                            * bottom activation. */` |
|        - | 1928 | `	VmExecState sState; /* This activation's boundary state:` |
|        - | 1929 | `	                     * everything a suspended/nested activation must restore.` |
|        - | 1930 | `	                     * pc/pTos stay in locals for the hot loop and are synced` |
|        - | 1931 | `	                     * into sState only around the call epilogue (stage 2 turns` |
|        - | 1932 | `	                     * that boundary into an explicit record push/pop). */` |
|        - | 1933 | `	sxi32 pc;` |
|        - | 1934 | `	sxi32 rc;` |
|  4541558 | 1935 | `	int bCallInitStamp = 0; /* PH7_OP_CALL_INIT: may this site's verdict be remembered? */` |
|  4541558 | 1936 | `	sState.aInstr = aInstr;` |
|  4541558 | 1937 | `	sState.pStack = pStack;` |
|  4541558 | 1938 | `	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */` |
|  4541558 | 1939 | `	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */` |
|  4541558 | 1940 | `	sState.pResult = pResult;` |
|  4541558 | 1941 | `	sState.pLastRef = pLastRef;` |
|  4541558 | 1942 | `	sState.pEnforceRetFunc = pEnforceRetFunc;` |
|  4541558 | 1943 | `	sState.is_callback = (sxu8)(is_callback ? 1 : 0);` |
|  4541558 | 1944 | `	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);` |
|        - | 1945 | `	/* Argument container */` |
|  4541558 | 1946 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|  4541558 | 1947 | `	if( nTos < 0 ){` |
|  1527255 | 1948 | `		pTos = &pStack[-1];` |
|   763577 | 1949 | `	}else{` |
|  3014308 | 1950 | `		pTos = &pStack[nTos];` |
|        - | 1951 | `	}` |
|  4541558 | 1952 | `	sState.pTos = pTos;` |
|  4541558 | 1953 | `	pHigh = pTos;` |
|  4541558 | 1954 | `	sState.pHigh = pHigh;` |
|  4541558 | 1955 | `	sState.pc = nPc;` |
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
|  4541553 | 1973 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame` |
|     3213 | 1974 | `	 && pStack == pVm->pActiveCtx->pStack ){` |
|     2663 | 1975 | `		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;` |
|     2663 | 1976 | `		sState.nFinallyActBase = pVm->pActiveCtx->nFinallyBase;` |
|     1334 | 1977 | `	}else{` |
|  4538900 | 1978 | `		sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|  4538900 | 1979 | `		sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|        - | 1980 | `	}` |
|  4541558 | 1981 | `	sState.pEntryFrame = pVm->pFrame;` |
|  4541558 | 1982 | `	pc = nPc;` |
|        - | 1983 | `	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call` |
|        - | 1984 | `	 * adopts its parked record segment here. VmResumeCtx hands the segment in` |
|        - | 1985 | `	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a` |
|        - | 1986 | `	 * nested mini-program/callback run inside the resumed body — after it rebased` |
|        - | 1987 | `	 * the segment's exception floors and pushed the resume value into the innermost` |
|        - | 1988 | `	 * stack. Locals switch to the innermost activation so the dispatch loop` |
|        - | 1989 | `	 * continues inside the callee; the record chain is restored so its completion` |
|        - | 1990 | `	 * unwinds back through the body. */` |
|  4541558 | 1991 | `	if( pAdoptSegment ){` |
|      ! 0 | 1992 | `		VmParkedSegment *pSeg = pAdoptSegment;` |
|      ! 0 | 1993 | `		pCallTop = pSeg->pCallTop;` |
|      ! 0 | 1994 | `		sState = pSeg->sState;` |
|      ! 0 | 1995 | `		aInstr = sState.aInstr;` |
|      ! 0 | 1996 | `		pStack = sState.pStack;` |
|        - | 1997 | `		/* pc is already nPc (== pCtx->pc, the innermost's post-suspend pc) from the` |
|        - | 1998 | `		 * init above; only the stack/top move to the innermost activation. */` |
|      ! 0 | 1999 | `		pTos = &pStack[nTos];   /* nTos == pCtx->nTos — innermost, resume value pushed */` |
|        - | 2000 | `		/* The parked state carries the innermost activation's watermark; the resume` |
|        - | 2001 | `		 * value was pushed above it, so take whichever is higher. */` |
|      ! 0 | 2002 | `		pHigh = ( sState.pHigh > pTos ) ? sState.pHigh : pTos;` |
|      ! 0 | 2003 | `		SyMemBackendFree(&pVm->sAllocator,pSeg); /* holder only; its contents are now live */` |
|      ! 0 | 2004 | `	}` |
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
|  4541553 | 2044 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected` |
|     1676 | 2045 | `	 && (pAdoptSegment != 0 \|\| pVm->pActiveCtx->pFrame == sState.pEntryFrame)` |
|       63 | 2046 | `	 && pVm->pActiveCtx->iDelegateState != 3 ){` |
|       58 | 2047 | `		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;` |
|        - | 2048 | `		VmFrame *pThrowFrame;` |
|        - | 2049 | `		sxi32 iResumePc;` |
|       58 | 2050 | `		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */` |
|        - | 2051 | `		/* Raising the injection at the yield-from point abandons any array/Iterator` |
|        - | 2052 | `		 * delegation in progress (state 1/2 — state 3 was forwarded, not raised here).` |
|        - | 2053 | `		 * Tear the delegate down so that if the generator's own try catches this and` |
|        - | 2054 | ``		 * runs on to a LATER `yield from`, that opcode classifies its operand fresh`` |
|        - | 2055 | `		 * instead of resuming this now-stale delegate cursor. */` |
|       58 | 2056 | `		if( pVm->pActiveCtx->iDelegateState != 0 ){` |
|        3 | 2057 | `			PH7_MemObjRelease(&pVm->pActiveCtx->sDelegate);` |
|        3 | 2058 | `			pVm->pActiveCtx->pDelegateNode = 0;` |
|        3 | 2059 | `			pVm->pActiveCtx->iDelegateState = 0;` |
|        1 | 2060 | `		}` |
|       58 | 2061 | `		pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       58 | 2062 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|       58 | 2063 | `		rc = VmThrowException(&(*pVm),pInj);` |
|       58 | 2064 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2065 | `			goto Abort;` |
|        - | 2066 | `		}` |
|       58 | 2067 | `		if( VmInlineOwnedBy(pVm,aInstr,sState.pEntryFrame) ){` |
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
|       35 | 2078 | `		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 2079 | `			/* Caught by THIS generator's own try. (VmRecordedResume returns FALSE unless a` |
|        - | 2080 | `			 * catch recorded a resume target for this exec, so rc need not be pre-checked;` |
|        - | 2081 | `			 * unlike OP_THROW there is no lexical-try fallthrough here — the else just` |
|        - | 2082 | `			 * propagates.) Drain the abandoned mid-expression operand slots back to the` |
|        - | 2083 | `			 * catching try's base, then land at its pad (iResumePc is landing-1 for the` |
|        - | 2084 | `			 * dispatcher's trailing pc++; the loop below fetches at pc with no leading pc++,` |
|        - | 2085 | `			 * so add 1 to land on the pad itself). */` |
|      ! 0 | 2086 | `			while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      ! 0 | 2087 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 | 2088 | `				pTos--;` |
|      ! 0 | 2089 | `			}` |
|      ! 0 | 2090 | `			pc = iResumePc + 1;` |
|      ! 0 | 2091 | `		}else{` |
|        - | 2092 | `			/* Not caught in this generator (no match, or caught by an outer/caller frame` |
|        - | 2093 | `			 * that ROOT B will land at its own OP_CALL site): propagate out so the ctx` |
|        - | 2094 | `			 * closes and the caller sees the exception. */` |
|       13 | 2095 | `			goto Exception;` |
|        - | 2096 | `		}` |
|       22 | 2097 | `	}` |
|        - | 2098 | `	/* Force-close entry (VmCloseCtx): a suspended generator being destroyed runs its` |
|        - | 2099 | ``	 * pending `finally` blocks. Instead of resuming at the yield, redirect straight`` |
|        - | 2100 | ``	 * into the innermost open try's finally as if a `return` had crossed every`` |
|        - | 2101 | `	 * enclosing finally (mirrors OP_SET_FINALLY_RET; OP_END_FINALLY then threads the` |
|        - | 2102 | `	 * PH7_FA_RETURN out through the whole chain and completes the body). Gate on the` |
|        - | 2103 | `	 * BODY bytecode (aInstr == the function program) so nested mini-programs sharing` |
|        - | 2104 | `	 * this ctx/frame never re-fire it; bClosing stays set so OP_YIELD can reject a` |
|        - | 2105 | `	 * yield reached inside one of these finallys. */` |
|  4541597 | 2106 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing` |
|     1751 | 2107 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
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
| 40978623 | 2127 | `	for(;;){` |
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
| 83238075 | 2142 | `		if( pVm->nBoundaryRc != 0 \|\| SySetUsed(&pVm->aHookRmw) > 0` |
| 83230451 | 2143 | `		 \|\| PH7_PcntlAsyncPending \|\| pVm->sAllocator.nMemTried != 0` |
| 83230209 | 2144 | `		 \|\| pVm->bGcWanted \|\| pVm->bClosurePurge ){` |
|    18928 | 2145 | `			if( pVm->bClosurePurge ){` |
|        - | 2146 | `				/* Run-time closures whose last holder went. Freed HERE and not at the` |
|        - | 2147 | `				 * drop, because the drop is usually a dispatch releasing the Closure` |
|        - | 2148 | `				 * object it has just unwrapped and is about to look up by name. */` |
|    17523 | 2149 | `				PH7_VmPurgeDeadClosures(&(*pVm));` |
|     8639 | 2150 | `			}` |
|    18928 | 2151 | `			if( pVm->bGcWanted ){` |
|        - | 2152 | `				/* The cycle collector's root buffer filled. HERE is the only place` |
|        - | 2153 | `				 * it may run: between two instructions, with the operand stack` |
|        - | 2154 | `				 * consistent and no C builtin holding a raw ph7_value* across it.` |
|        - | 2155 | `				 * The drop that buffered the root was in the middle of an opcode's` |
|        - | 2156 | `				 * C body, which is no place to be running destructors. */` |
|       49 | 2157 | `				PH7_GcCollect(&(*pVm));` |
|       23 | 2158 | `			}` |
|    18928 | 2159 | `			if( pVm->sAllocator.nMemTried != 0 ){` |
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
|    18928 | 2185 | `			if( PH7_PcntlAsyncPending && pVm->nBoundaryRc == 0 ){` |
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
|    18928 | 2198 | `			if( pVm->nBoundaryRc != 0 ){` |
|      875 | 2199 | `				sxi32 rcBr = pVm->nBoundaryRc;` |
|      875 | 2200 | `				pVm->nBoundaryRc = 0;` |
|      875 | 2201 | `				if( rcBr == PH7_ABORT ){` |
|       12 | 2202 | `					goto Abort;` |
|        - | 2203 | `				}` |
|      865 | 2204 | `				if( VmInlineOwnedBy(pVm,aInstr,sState.pEntryFrame) ){` |
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
|      865 | 2216 | `					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){` |
|        - | 2217 | `						/* Caught in place by a try THIS exec owns: drain the abandoned` |
|        - | 2218 | `						 * operands to the catching try's base and land at its pad` |
|        - | 2219 | `						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;` |
|        - | 2220 | `						 * pre-fetch here, so +1 lands on the pad itself). */` |
|      693 | 2221 | `						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      333 | 2222 | `							PH7_MemObjRelease(pTos);` |
|      333 | 2223 | `							pTos--;` |
|        5 | 2224 | `						}` |
|      365 | 2225 | `						pc = iBrPc + 1;` |
|      184 | 2226 | `					}else{` |
|        - | 2227 | `						/* Caught by an outer exec (or uncaught-with-report pending):` |
|        - | 2228 | `						 * unwind out of this exec; the owner's macros/router land it. */` |
|      505 | 2229 | `						goto Exception;` |
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
|    18430 | 2243 | `			while( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      492 | 2244 | `				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      490 | 2245 | `				if( pTopRmw->pOwnerStack != (void *)pStack \|\| pTopRmw->pInstrs != (void *)aInstr` |
|      184 | 2246 | `				 \|\| ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){` |
|      241 | 2247 | `					break; /* not ours, or legitimately in flight */` |
|        - | 2248 | `				}` |
|       13 | 2249 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 2250 | `			}` |
|     9085 | 2251 | `		}` |
| 83237570 | 2252 | `		if( pTos > pHigh ){` |
|  2422178 | 2253 | `			pHigh = pTos;   /* the activation's high-water mark; see pHigh's declaration */` |
|  1211380 | 2254 | `		}` |
|        - | 2255 | `		/* Fetch the instruction to execute */` |
| 83237570 | 2256 | `		pInstr = &aInstr[pc];` |
| 83237570 | 2257 | `		if( pInstr->nLine ){` |
|        - | 2258 | `			/* Publish the source position for diagnostics, debug_backtrace() and` |
|        - | 2259 | `			 * Throwable. Instructions the compiler could not attribute (nLine 0)` |
|        - | 2260 | `			 * leave the last known line standing rather than reporting line 0.` |
|        - | 2261 | `			 * The compilation unit's strict_types mode rides the same gate: a` |
|        - | 2262 | `			 * SYNTHETIC instruction (nLine 0) must not claim to speak for a file. */` |
| 77427443 | 2263 | `			pVm->nCurLine = pInstr->nLine;` |
| 77427443 | 2264 | `			pVm->bCurStrict = pInstr->bStrict;` |
| 38753482 | 2265 | `		}` |
| 83237570 | 2266 | `		rc = SXRET_OK;` |
|        - | 2267 | `/*` |
|        - | 2268 | ` * What follows here is a massive switch statement where each case implements a` |
|        - | 2269 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - | 2270 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - | 2271 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - | 2272 | ` * the switch statement will break with convention and be flush-left.` |
|        - | 2273 | ` */` |
| 83237570 | 2274 | `		switch(pInstr->iOp){` |
|        - | 2275 | `/*` |
|        - | 2276 | ` * DONE: P1 * *` |
|        - | 2277 | ` *` |
|        - | 2278 | ` * Program execution completed: Clean up the mess left behind` |
|        - | 2279 | ` * and return immediately.` |
|        - | 2280 | ` */` |
|  2214070 | 2281 | `case PH7_OP_DONE:` |
|  4428232 | 2282 | `	if( pInstr->iP2 && sState.bReturnPropagates ){` |
|        - | 2283 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - | 2284 | `		 * onto the body frame this catch/finally returns from (skip the transparent` |
|        - | 2285 | `		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION` |
|        - | 2286 | `		 * materializes it into sState.pResult. Drain any finally opened within this body` |
|        - | 2287 | `		 * first (nested try/finally inside the catch), which may overwrite the same` |
|        - | 2288 | `		 * frame's slot (finally-over-catch). */` |
|    24017 | 2289 | `		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|    24017 | 2290 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    23987 | 2291 | `			PH7_MemObjStore(pTos,&pTgt->sRet);` |
|    23987 | 2292 | `			VmPopOperand(&pTos,1);` |
|    11996 | 2293 | `		}else{` |
|       32 | 2294 | ``			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */`` |
|        - | 2295 | `		}` |
|    24017 | 2296 | `		pTgt->bHasRet = 1;` |
|    24017 | 2297 | `		pTgt->nRetGen++;` |
|    24017 | 2298 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    24017 | 2299 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2300 | `			goto Abort;` |
|        - | 2301 | `		}` |
|    24017 | 2302 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 2303 | `			/* A drained finally threw past itself — it discards this return. */` |
|      ! 0 | 2304 | `			goto Exception;` |
|        - | 2305 | `		}` |
|    24017 | 2306 | `		goto Done;` |
|        - | 2307 | `	}` |
|        - | 2308 | ``	/* php's `Only variable references should be returned by reference`, raised at`` |
|        - | 2309 | ``	 * the RETURN and nowhere else: a function DECLARED `&` whose return expression`` |
|        - | 2310 | `	 * is not a variable has nothing to bind, and php says so whether the caller` |
|        - | 2311 | `	 * went on to take the answer by reference or by value. Falling off the end and` |
|        - | 2312 | ``	 * a bare `return;` count too -- both leave it with no variable. The`` |
|        - | 2313 | `	 * VM_FRAME_THROW guard is the one the return-type check below uses: a function` |
|        - | 2314 | `	 * unwinding through this terminal OP_DONE never returned anything. */` |
|  4404215 | 2315 | `	if( sState.pEnforceRetFunc` |
|  2215500 | 2316 | `	 && (sState.pEnforceRetFunc->iFlags & VM_FUNC_REF_RETURN)` |
|    12975 | 2317 | `	 && (sState.pEnforceRetFunc->iFlags & VM_FUNC_GENERATOR) == 0` |
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
|  4404215 | 2334 | `	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)` |
|    26241 | 2335 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - | 2336 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - | 2337 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - | 2338 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - | 2339 | `		 * value the function never actually returned, so enforcing here would` |
|        - | 2340 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - | 2341 | `		 * exception. */` |
|    26197 | 2342 | `		ph7_value *pRetVal = 0;` |
|    26197 | 2343 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    20959 | 2344 | `			pRetVal = pTos;` |
|    10411 | 2345 | `		}` |
|    26197 | 2346 | `		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);` |
|    26197 | 2347 | `		if( rc == PH7_ABORT ) goto Abort;` |
|    26193 | 2348 | `		if( rc == PH7_EXCEPTION ){` |
|      151 | 2349 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      117 | 2350 | `				PH7_MemObjRelease(pTos);` |
|      117 | 2351 | `				pTos--;` |
|       56 | 2352 | `			}` |
|      151 | 2353 | `			goto Exception;` |
|        - | 2354 | `		}` |
|        - | 2355 | `		/* Don't enforce twice if the function loops through multiple` |
|        - | 2356 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - | 2357 | `		 * defensively we clear the pointer after a successful check). */` |
|    26047 | 2358 | `		sState.pEnforceRetFunc = 0;` |
|    12812 | 2359 | `	}` |
|  4404070 | 2360 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|  2914132 | 2361 | `		if( sState.pLastRef ){` |
|   172836 | 2362 | `			*sState.pLastRef = pTos->nIdx;` |
|    86792 | 2363 | `		}` |
|  2914132 | 2364 | `		if( sState.pResult ){` |
|        - | 2365 | `			/* Execution result */` |
|  1421234 | 2366 | `			PH7_MemObjStore(pTos,sState.pResult);` |
|   710881 | 2367 | `		}` |
|  2914132 | 2368 | `		VmPopOperand(&pTos,1);` |
|  1457299 | 2369 | `	}else{` |
|  1489943 | 2370 | `		if( pInstr->iP1 == 0 && pInstr->iP2 && sState.pResult ){` |
|        - | 2371 | ``			/* An EXPLICIT `return;` with no value answers NULL. It reads as a`` |
|        - | 2372 | `			 * no-op for a function (whose result slot starts out null anyway)` |
|        - | 2373 | `			 * and matters for an included CHUNK, whose slot is seeded with the 1` |
|        - | 2374 | ``			 * a file that returns nothing answers: `<?php return;` is php's`` |
|        - | 2375 | `			 * NULL, not that 1. */` |
|      195 | 2376 | `			PH7_MemObjRelease(sState.pResult);` |
|       95 | 2377 | `		}` |
|        - | 2378 | `		/* Nothing referenced — also the throw-unwind path: the compiler routes` |
|        - | 2379 | `		 * an uncaught exception to this terminal OP_DONE with iP1 set but an` |
|        - | 2380 | `		 * empty operand stack (pTos == pStack-1), so there is no return value to` |
|        - | 2381 | `		 * store. Guarding on pTos >= pStack (matching the sibling branch above)` |
|        - | 2382 | `		 * avoids the below-base read that crashed under glibc/ASan. */` |
|  1489943 | 2383 | `		if( sState.pLastRef ){` |
|    22629 | 2384 | `			*sState.pLastRef = SXU32_HIGH;` |
|    11167 | 2385 | `		}` |
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
|  4404070 | 2396 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|  4404070 | 2397 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2398 | `		goto Abort;` |
|        - | 2399 | `	}` |
|  4404070 | 2400 | `	if( rc == PH7_EXCEPTION ){` |
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
|  4404066 | 2412 | `	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){` |
|        - | 2413 | `		/* A catch/finally issued a 'return' targeting THIS body. If the body is` |
|        - | 2414 | `		 * actually unwinding because an exception escaped it (terminal OP_DONE on` |
|        - | 2415 | `		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type` |
|        - | 2416 | `		 * enforcement above), that exception supersedes the return: discard it.` |
|        - | 2417 | `		 * Otherwise materialize it as this function's result. */` |
|       11 | 2418 | `		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){` |
|      ! 0 | 2419 | `			VmClearFramePending(sState.pEntryFrame);` |
|      ! 0 | 2420 | `		}else{` |
|       11 | 2421 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|        - | 2422 | `		}` |
|        4 | 2423 | `	}` |
|  4404066 | 2424 | `	goto Done;` |
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
|       64 | 2439 | `			*sState.pLastRef = pTos->nIdx;` |
|       31 | 2440 | `		}` |
|      117 | 2441 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       79 | 2442 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - | 2443 | `				/* Output the exit message */` |
|      120 | 2444 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|       41 | 2445 | `					pVm->sVmConsumer.pUserData);` |
|       79 | 2446 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|       45 | 2447 | `			}` |
|       83 | 2448 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - | 2449 | `			/* Record exit status */` |
|       42 | 2450 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
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
|   973521 | 2469 | `case PH7_OP_JMP:` |
|  1947890 | 2470 | `	pc = pInstr->iP2 - 1;` |
|  1947890 | 2471 | `	break;` |
|        - | 2472 | `/*` |
|        - | 2473 | ` * JZ: P1 P2 *` |
|        - | 2474 | ` *` |
|        - | 2475 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - | 2476 | ` * entry in the stack if P1 is zero.` |
|        - | 2477 | ` */` |
|  2330464 | 2478 | `case PH7_OP_JZ:` |
|        - | 2479 | `#ifdef UNTRUST` |
|        - | 2480 | `	if( pTos < pStack ){` |
|        - | 2481 | `		goto Abort;` |
|        - | 2482 | `	}` |
|        - | 2483 | `#endif` |
|        - | 2484 | `	/* Get a boolean value */` |
|  4670277 | 2485 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    36487 | 2486 | `		PH7_MemObjToBool(pTos);` |
|    18235 | 2487 | `	}` |
|  4670277 | 2488 | `	if( !pTos->x.iVal ){` |
|        - | 2489 | `		/* Take the jump */` |
|  2548405 | 2490 | `		pc = pInstr->iP2 - 1;` |
|  1276829 | 2491 | `	}` |
|  4670277 | 2492 | `	if( !pInstr->iP1 ){` |
|  4104475 | 2493 | `		VmPopOperand(&pTos,1);` |
|  2056576 | 2494 | `	}` |
|  4670277 | 2495 | `	break;` |
|        - | 2496 | `/*` |
|        - | 2497 | ` * JNZ: P1 P2 *` |
|        - | 2498 | ` *` |
|        - | 2499 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - | 2500 | ` * entry in the stack if P1 is zero.` |
|        - | 2501 | ` */` |
|   181025 | 2502 | `case PH7_OP_JNZ:` |
|        - | 2503 | `#ifdef UNTRUST` |
|        - | 2504 | `	if( pTos < pStack ){` |
|        - | 2505 | `		goto Abort;` |
|        - | 2506 | `	}` |
|        - | 2507 | `#endif` |
|        - | 2508 | `	/* Get a boolean value */` |
|   363318 | 2509 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        5 | 2510 | `		PH7_MemObjToBool(pTos);` |
|        2 | 2511 | `	}` |
|   363318 | 2512 | `	if( pTos->x.iVal ){` |
|        - | 2513 | `		/* Take the jump */` |
|    11579 | 2514 | `		pc = pInstr->iP2 - 1;` |
|     5789 | 2515 | `	}` |
|   363318 | 2516 | `	if( !pInstr->iP1 ){` |
|       37 | 2517 | `		VmPopOperand(&pTos,1);` |
|       19 | 2518 | `	}` |
|   363318 | 2519 | `	break;` |
|        - | 2520 | `/*` |
|        - | 2521 | ` * NOOP: * * *` |
|        - | 2522 | ` *` |
|        - | 2523 | ` * Do nothing. This instruction is often useful as a jump` |
|        - | 2524 | ` * destination.` |
|        - | 2525 | ` */` |
|        3 | 2526 | `case PH7_OP_NOOP:` |
|        6 | 2527 | `	break;` |
|        - | 2528 | `/*` |
|        - | 2529 | ` * SNAPSHOT: P1 * *` |
|        - | 2530 | ` *` |
|        - | 2531 | ` * Give the top P1 stack slots their own copy of the string bytes they are borrowing` |
|        - | 2532 | ` * (P1 = 0 means the top slot alone).` |
|        - | 2533 | ` *` |
|        - | 2534 | ` * A value copy aliases the source's buffer rather than duplicating it, so a value pushed` |
|        - | 2535 | `` * from `$x` is a pointer into `$x` plus the length `$x` had at the push. Anything that`` |
|        - | 2536 | `` * runs before the push is consumed and assigns to `$x` overwrites those bytes in place,`` |
|        - | 2537 | ` * and the pushed value then reads the NEW content through the OLD length --` |
|        - | 2538 | `` * `show($x, $x = 'second')` printed "secon" for an argument php had already copied as`` |
|        - | 2539 | `` * "first", and `[$x, $x = 'second']` built the same truncated element. The compiler emits`` |
|        - | 2540 | ` * this only where something that can run code still sits between a push and its consumer,` |
|        - | 2541 | ` * so ordinary code pays for it exactly where php's own SEND_VAR pays for a reference.` |
|        - | 2542 | ` */` |
|   811996 | 2543 | `case PH7_OP_SNAPSHOT: {` |
|  1624855 | 2544 | `	sxi32 nSnap = pInstr->iP1 > 0 ? pInstr->iP1 : 1;` |
|        - | 2545 | `	ph7_value *pSnap;` |
|        - | 2546 | `#ifdef UNTRUST` |
|        - | 2547 | `	if( pTos < pStack ){` |
|        - | 2548 | `		goto Abort;` |
|        - | 2549 | `	}` |
|        - | 2550 | `#endif` |
|  1624855 | 2551 | `	if( &pTos[-nSnap+1] < pStack ){` |
|      ! 0 | 2552 | `		nSnap = (sxi32)(pTos - pStack) + 1;` |
|      ! 0 | 2553 | `	}` |
|  3275873 | 2554 | `	for( pSnap = pTos ; nSnap > 0 ; --nSnap, --pSnap ){` |
|  1651023 | 2555 | `		SyBlobMakePrivate(&pSnap->sBlob);` |
|   825912 | 2556 | `	}` |
|  1624855 | 2557 | `	break;` |
|        - | 2558 | `					   }` |
|        - | 2559 | `/*` |
|        - | 2560 | ` * PICK: P1 * *` |
|        - | 2561 | ` *` |
|        - | 2562 | ` * Push a copy of the stack slot P1 below the top (P1 = 0 duplicates the top, as DUP does).` |
|        - | 2563 | ` *` |
|        - | 2564 | ` * An assignment target's dynamic subscript and property NAMES are evaluated before the` |
|        - | 2565 | ` * assigned value in php and the fetches they belong to run after it, so the compiler pushes` |
|        - | 2566 | ` * those names first, puts the value on top of them, and emits the access chain last. Each` |
|        - | 2567 | ` * access then reads its name back from the slot it was parked in -- which is at a depth the` |
|        - | 2568 | ` * compiler knows exactly, since every level of the chain consumes a container and a name and` |
|        - | 2569 | ` * leaves one element behind it.` |
|        - | 2570 | ` */` |
|     6479 | 2571 | `case PH7_OP_PICK: {` |
|        - | 2572 | `	ph7_value *pPick;` |
|        - | 2573 | `#ifdef UNTRUST` |
|        - | 2574 | `	if( &pTos[-pInstr->iP1] < pStack ){` |
|        - | 2575 | `		goto Abort;` |
|        - | 2576 | `	}` |
|        - | 2577 | `#endif` |
|    12962 | 2578 | `	pPick = &pTos[-pInstr->iP1];` |
|    12962 | 2579 | `	pTos++;` |
|    12962 | 2580 | `	PH7_MemObjInit(pVm,pTos);` |
|    12962 | 2581 | `	PH7_MemObjStore(pPick,pTos);` |
|    12962 | 2582 | `	break;` |
|        - | 2583 | `				 }` |
|        - | 2584 | `/*` |
|        - | 2585 | ` * POP: P1 * *` |
|        - | 2586 | ` *` |
|        - | 2587 | ` * Pop P1 elements from the operand stack.` |
|        - | 2588 | ` */` |
|  2049210 | 2589 | `case PH7_OP_POP: {` |
|  4103931 | 2590 | `	sxi32 n = pInstr->iP1;` |
|  4103931 | 2591 | `	if( &pTos[-n+1] < pStack ){` |
|        - | 2592 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 | 2593 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 | 2594 | `	}` |
|  4103931 | 2595 | `	VmPopOperand(&pTos,n);` |
|  4103931 | 2596 | `	break;` |
|        - | 2597 | `				 }` |
|        - | 2598 | `/*` |
|        - | 2599 | ` * DUP: * * *` |
|        - | 2600 | ` *` |
|        - | 2601 | ` * Duplicate the top of the stack.` |
|        - | 2602 | ` */` |
|      207 | 2603 | `case PH7_OP_DUP:` |
|        - | 2604 | `#ifdef UNTRUST` |
|        - | 2605 | `	if( pTos < pStack ){` |
|        - | 2606 | `		goto Abort;` |
|        - | 2607 | `	}` |
|        - | 2608 | `#endif` |
|      419 | 2609 | `	pTos++;` |
|      419 | 2610 | `	PH7_MemObjInit(pVm,pTos);` |
|      419 | 2611 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|      419 | 2612 | `	break;` |
|        - | 2613 | `/*` |
|        - | 2614 | ` * SWAP: * * *` |
|        - | 2615 | ` *` |
|        - | 2616 | ` * Exchange the top two stack slots.` |
|        - | 2617 | ` *` |
|        - | 2618 | `` * Emitted for one shape only: a binary operator whose LEFT operand is a plain `$var` and`` |
|        - | 2619 | ` * whose RIGHT operand can run code. php never materializes a plain variable operand -- it` |
|        - | 2620 | `` * reads the compiled variable AT the operator -- so `$n = 1; $n - ($n = 5)` is 0 there and`` |
|        - | 2621 | ` * the assignment's value is what BOTH sides see. Copying the left operand would answer the` |
|        - | 2622 | ` * pre-assignment 1; the load has to MOVE instead, which puts it on the stack above the` |
|        - | 2623 | ` * right operand and needs the two put back in the operator's order. The whole ph7_value` |
|        - | 2624 | ` * moves, pVm included -- it is the same pointer in both -- and no jump can land between` |
|        - | 2625 | ` * the two pushes and this, so nothing is holding either slot's address.` |
|        - | 2626 | ` */` |
|    50996 | 2627 | `case PH7_OP_SWAP: {` |
|        - | 2628 | `	ph7_value sSwap;` |
|        - | 2629 | `#ifdef UNTRUST` |
|        - | 2630 | `	if( pTos < &pStack[1] ){` |
|        - | 2631 | `		goto Abort;` |
|        - | 2632 | `	}` |
|        - | 2633 | `#endif` |
|   102239 | 2634 | `	sSwap  = pTos[0];` |
|   102239 | 2635 | `	pTos[0] = pTos[-1];` |
|   102239 | 2636 | `	pTos[-1] = sSwap;` |
|   102239 | 2637 | `	break;` |
|        - | 2638 | `				  }` |
|        - | 2639 | `/*` |
|        - | 2640 | ` * OP_FUNC_DECL * * P3` |
|        - | 2641 | ` *` |
|        - | 2642 | ` * Bind the function p3 names, here, where the declaration STATEMENT sits. Only a` |
|        - | 2643 | ` * conditional declaration takes this route: a top-level one is bound while its unit` |
|        - | 2644 | ` * compiles, exactly as php early-binds it.` |
|        - | 2645 | ` */` |
|       78 | 2646 | `case PH7_OP_FUNC_DECL: {` |
|      161 | 2647 | `	ph7_vm_func *pDeclFunc = (ph7_vm_func *)pInstr->p3;` |
|      161 | 2648 | `	if( pDeclFunc ){` |
|      239 | 2649 | `		SyHashEntry *pDeclEntry = SyHashGet(&pVm->hFunction,` |
|      156 | 2650 | `			SyStringData(&pDeclFunc->sName),SyStringLength(&pDeclFunc->sName));` |
|      156 | 2651 | `		if( pInstr->iP1 == 0` |
|      116 | 2652 | `		 && PH7_VmNameIsInternalFunc(pVm,SyStringData(&pDeclFunc->sName),` |
|       33 | 2653 | `				SyStringLength(&pDeclFunc->sName)) ){` |
|        - | 2654 | `			/* An INTERNAL function of this name. php refuses a conditional` |
|        - | 2655 | `			 * declaration of one exactly as it refuses a top-level one -- being` |
|        - | 2656 | `			 * reached at run time buys it nothing -- and names no previous` |
|        - | 2657 | `			 * declaration, an internal function having no file and no line.` |
|        - | 2658 | `			 *` |
|        - | 2659 | `			 * A polyfill does not land here: it asks function_exists() first, and` |
|        - | 2660 | `			 * for a name this engine carries that answers true and the body is` |
|        - | 2661 | `			 * never reached. Where it answers false the name is not internal and` |
|        - | 2662 | `			 * this test does not fire. */` |
|        3 | 2663 | `			PH7_VmFatalError(&(*pVm),"Cannot redeclare function %z()",&pDeclFunc->sName);` |
|        3 | 2664 | `			pVm->iExitStatus = 255;` |
|        3 | 2665 | `			pVm->bHaltRequested = 1;` |
|        3 | 2666 | `			goto Abort;` |
|        - | 2667 | `		}` |
|      159 | 2668 | `		if( pDeclEntry && pInstr->iP1 == 0 ){` |
|        - | 2669 | `			/* php's runtime redeclaration fatal -- the same sentence the compiler` |
|        - | 2670 | `			 * raises for two top-level declarations of one name, and now in the same` |
|        - | 2671 | ``			 * SHAPE: `PHP Fatal error:  ` and a `Stack trace:` block, which the plain`` |
|        - | 2672 | `			 * diagnostic printer this used to go through gives neither of. A BUILTIN` |
|        - | 2673 | `			 * of that name is not in this table, so a polyfill body that reaches here` |
|        - | 2674 | ``			 * beside one is the `function_exists()` guard having answered false.`` |
|        - | 2675 | `			 *` |
|        - | 2676 | `			 * The name is taken whenever this statement RUNS, so running the same` |
|        - | 2677 | ``			 * declaration a second time collides with itself -- `function mk(){`` |
|        - | 2678 | ``			 * function h(){} } mk(); mk();` is php's fatal, naming one line as both`` |
|        - | 2679 | `			 * the refusal and the previous declaration. Testing the installed entry` |
|        - | 2680 | `			 * for IDENTITY instead made the second run a silent no-op. */` |
|        3 | 2681 | `			ph7_vm_func *pPrevDecl = (ph7_vm_func *)pDeclEntry->pUserData;` |
|        3 | 2682 | `			if( pPrevDecl && pPrevDecl->sFile.nByte > 0 ){` |
|        4 | 2683 | `				PH7_VmFatalError(&(*pVm),` |
|        - | 2684 | `					"Cannot redeclare function %z() (previously declared in %.*s:%u)",` |
|        1 | 2685 | `					&pDeclFunc->sName,pPrevDecl->sFile.nByte,pPrevDecl->sFile.zString,` |
|        1 | 2686 | `					pPrevDecl->nLine);` |
|        2 | 2687 | `			}else{` |
|      ! 0 | 2688 | `				PH7_VmFatalError(&(*pVm),"Cannot redeclare function %z()",&pDeclFunc->sName);` |
|        - | 2689 | `			}` |
|        3 | 2690 | `			pVm->iExitStatus = 255;` |
|        3 | 2691 | `			pVm->bHaltRequested = 1;` |
|        3 | 2692 | `			goto Abort;` |
|        - | 2693 | `		}` |
|      157 | 2694 | `		if( pDeclEntry == 0 \|\| pDeclEntry->pUserData != (void *)pDeclFunc ){` |
|      151 | 2695 | `			PH7_VmInstallUserFunction(&(*pVm),pDeclFunc,0);` |
|       73 | 2696 | `		}` |
|       76 | 2697 | `	}` |
|      157 | 2698 | `	break;` |
|        - | 2699 | `				}` |
|        - | 2700 | `/*` |
|        - | 2701 | ` * CLASS_DEFER: * * P3` |
|        - | 2702 | ` *` |
|        - | 2703 | ` * Execute a class declaration whose parent/interface/trait could not be` |
|        - | 2704 | ` * resolved when the enclosing file compiled (its autoloader had not RUN yet).` |
|        - | 2705 | ` * P3 is the VmDeferredClass record captured by the compiler. A dependency` |
|        - | 2706 | `` * that is STILL missing throws php's catchable `... not found` Error; a`` |
|        - | 2707 | ` * failed re-compile aborts (its errors were already reported). See the` |
|        - | 2708 | ` * deferral block comment in compile_class.c.` |
|        - | 2709 | ` */` |
|       78 | 2710 | `case PH7_OP_CLASS_DEFER: {` |
|      161 | 2711 | `	VmDeferredClass *pDefer = (VmDeferredClass *)pInstr->p3;` |
|      161 | 2712 | `	VmDeferredReq *pMissing = 0;` |
|      161 | 2713 | `	sxi32 rcDecl = SXRET_OK;` |
|      161 | 2714 | `	if( pDefer ){` |
|      161 | 2715 | `		rcDecl = VmExecDeferredClass(&(*pVm),pDefer,&pMissing);` |
|       78 | 2716 | `	}` |
|      161 | 2717 | `	if( pMissing ){` |
|        - | 2718 | `		static const char *azDeferKind[] = { "Class", "Interface", "Trait" };` |
|        - | 2719 | `		char zDeclMsg[520];` |
|       16 | 2720 | `		sxu32 nDeclMsg = SyBufferFormat(zDeclMsg,sizeof(zDeclMsg),"%s \"%.*s\" not found",` |
|        5 | 2721 | `			azDeferKind[pMissing->cKind < 3 ? pMissing->cKind : 0],` |
|       10 | 2722 | `			(int)pMissing->sName.nByte,pMissing->sName.zString);` |
|       11 | 2723 | `		rc = VmThrowFromVm(&(*pVm),"Error",zDeclMsg,nDeclMsg);` |
|       11 | 2724 | `		if( rc == SXERR_ABORT ){` |
|        3 | 2725 | `			goto Abort;` |
|        - | 2726 | `		}` |
|        9 | 2727 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2728 | `	}` |
|      151 | 2729 | `	if( rcDecl == SXERR_ABORT ){` |
|      ! 0 | 2730 | `		goto Abort;` |
|        - | 2731 | `	}` |
|      151 | 2732 | `	break;` |
|        - | 2733 | `				}` |
|        - | 2734 | `/*` |
|        - | 2735 | ` * CVT_INT: * * *` |
|        - | 2736 | ` *` |
|        - | 2737 | ` * Force the top of the stack to be an integer.` |
|        - | 2738 | ` */` |
|     1401 | 2739 | `case PH7_OP_CVT_INT:` |
|        - | 2740 | `#ifdef UNTRUST` |
|        - | 2741 | `	if( pTos < pStack ){` |
|        - | 2742 | `		goto Abort;` |
|        - | 2743 | `	}` |
|        - | 2744 | `#endif` |
|     2774 | 2745 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|        - | 2746 | `		/* php warns from the conversion itself when no int can hold the float` |
|        - | 2747 | ``		 * (`(int)1e19`); the value it then answers is the modular wrap. */`` |
|     1976 | 2748 | `		PH7_MemObjWarnIntCast(pTos);` |
|     1976 | 2749 | `		PH7_MemObjToInteger(pTos);` |
|      969 | 2750 | `	}` |
|        - | 2751 | `	/* Invalidate any prior representation */` |
|     2774 | 2752 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|     2774 | 2753 | `	break;` |
|        - | 2754 | `/*` |
|        - | 2755 | ` * CVT_REAL: * * *` |
|        - | 2756 | ` *` |
|        - | 2757 | ` * Force the top of the stack to be a real.` |
|        - | 2758 | ` */` |
|       64 | 2759 | `case PH7_OP_CVT_REAL:` |
|        - | 2760 | `#ifdef UNTRUST` |
|        - | 2761 | `	if( pTos < pStack ){` |
|        - | 2762 | `		goto Abort;` |
|        - | 2763 | `	}` |
|        - | 2764 | `#endif` |
|      132 | 2765 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      132 | 2766 | `		PH7_MemObjToReal(pTos);` |
|       64 | 2767 | `	}` |
|        - | 2768 | `	/* Invalidate any prior representation */` |
|      132 | 2769 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|      132 | 2770 | `	break;` |
|        - | 2771 | `/*` |
|        - | 2772 | ` * CVT_STR: * * *` |
|        - | 2773 | ` *` |
|        - | 2774 | ` * Force the top of the stack to be a string.` |
|        - | 2775 | ` */` |
|     3442 | 2776 | `case PH7_OP_CVT_STR:` |
|        - | 2777 | `#ifdef UNTRUST` |
|        - | 2778 | `	if( pTos < pStack ){` |
|        - | 2779 | `		goto Abort;` |
|        - | 2780 | `	}` |
|        - | 2781 | `#endif` |
|        - | 2782 | `	/* (string) cast + string interpolation "$arr": php's user-visible` |
|        - | 2783 | `	 * array->string warning site, and the not-stringable-object throw. */` |
|        - | 2784 | `	{` |
|     6886 | 2785 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     6888 | 2786 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2787 | `	}` |
|     6796 | 2788 | `	break;` |
|        - | 2789 | `/*` |
|        - | 2790 | ` * CVT_BOOL: * * *` |
|        - | 2791 | ` *` |
|        - | 2792 | ` * Force the top of the stack to be a boolean.` |
|        - | 2793 | ` */` |
|       74 | 2794 | `case PH7_OP_CVT_BOOL:` |
|        - | 2795 | `#ifdef UNTRUST` |
|        - | 2796 | `	if( pTos < pStack ){` |
|        - | 2797 | `		goto Abort;` |
|        - | 2798 | `	}` |
|        - | 2799 | `#endif` |
|      153 | 2800 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      145 | 2801 | `		PH7_MemObjToBool(pTos);` |
|       70 | 2802 | `	}` |
|      153 | 2803 | `	break;` |
|        - | 2804 | `/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it` |
|        - | 2805 | ` * always raises "The (unset) cast is no longer supported" (php 8 removed` |
|        - | 2806 | ` * the cast), so a program containing it never compiles. The switch has no` |
|        - | 2807 | ` * default arm, so abort loudly rather than fall through as a silent no-op` |
|        - | 2808 | ` * if a future emitter ever produces one without the compile error. */` |
|      ! 0 | 2809 | `case PH7_OP_CVT_NULL:` |
|      ! 0 | 2810 | `	goto Abort;` |
|        - | 2811 | `/*` |
|        - | 2812 | ` * CVT_NUMC: * * *` |
|        - | 2813 | ` *` |
|        - | 2814 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - | 2815 | ` */` |
|      ! 0 | 2816 | `case PH7_OP_CVT_NUMC:` |
|        - | 2817 | `#ifdef UNTRUST` |
|        - | 2818 | `	if( pTos < pStack ){` |
|        - | 2819 | `		goto Abort;` |
|        - | 2820 | `	}` |
|        - | 2821 | `#endif` |
|        - | 2822 | `	/* Force a numeric cast */` |
|      ! 0 | 2823 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 | 2824 | `	break;` |
|        - | 2825 | `/*` |
|        - | 2826 | ` * CVT_ARRAY: * * *` |
|        - | 2827 | ` *` |
|        - | 2828 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - | 2829 | ` */` |
|      431 | 2830 | `case PH7_OP_CVT_ARRAY:` |
|        - | 2831 | `#ifdef UNTRUST` |
|        - | 2832 | `	if( pTos < pStack ){` |
|        - | 2833 | `		goto Abort;` |
|        - | 2834 | `	}` |
|        - | 2835 | `#endif` |
|        - | 2836 | `	/* Force a hashmap cast */` |
|      867 | 2837 | `	rc = PH7_MemObjToHashmap(pTos);` |
|      867 | 2838 | `	if( rc != SXRET_OK ){` |
|        - | 2839 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 | 2840 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - | 2841 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 | 2842 | `	}` |
|      867 | 2843 | `	break;` |
|        - | 2844 | `/*` |
|        - | 2845 | ` * CVT_OBJ: * * *` |
|        - | 2846 | ` *` |
|        - | 2847 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - | 2848 | ` */` |
|       32 | 2849 | `case PH7_OP_CVT_OBJ:` |
|        - | 2850 | `#ifdef UNTRUST` |
|        - | 2851 | `	if( pTos < pStack ){` |
|        - | 2852 | `		goto Abort;` |
|        - | 2853 | `	}` |
|        - | 2854 | `#endif` |
|       67 | 2855 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 2856 | `		/* Force a 'stdClass()' cast */` |
|       67 | 2857 | `		PH7_MemObjToObject(pTos);` |
|       32 | 2858 | `	}` |
|       67 | 2859 | `	break;` |
|        - | 2860 | `/*` |
|        - | 2861 | ` * ERR_CTRL * * *` |
|        - | 2862 | ` *` |
|        - | 2863 | ` * Error control operator.` |
|        - | 2864 | ` */` |
|     4504 | 2865 | `case PH7_OP_UNSET_VAR: {` |
|        - | 2866 | `	VmOpRc rcOp;` |
|     9000 | 2867 | `	sState.pTos = pTos;` |
|     9000 | 2868 | `	sState.pc = pc;` |
|     9000 | 2869 | `	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);` |
|     9000 | 2870 | `	pTos = sState.pTos;` |
|     9000 | 2871 | `	pc = sState.pc;` |
|     9000 | 2872 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 2873 | `		goto Abort;` |
|     8998 | 2874 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 2875 | `		goto Exception;` |
|        - | 2876 | `	}` |
|     8996 | 2877 | `	break;` |
|        - | 2878 | `					  }` |
|    57994 | 2879 | `case PH7_OP_ERR_CTRL:` |
|        - | 2880 | `	/*` |
|        - | 2881 | `	 * Error-control operator '@'. Emitted as a PAIR around the suppressed` |
|        - | 2882 | `	 * expression: iP1=1 opens the window (before the operand is evaluated),` |
|        - | 2883 | `	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),` |
|        - | 2884 | `	 * which went unnoticed only because the engine raised so few diagnostics;` |
|        - | 2885 | `	 * every warning/notice/deprecation the parity work added leaked straight` |
|        - | 2886 | ``	 * through `@`. The window nests, and php 8 does NOT let '@' swallow`` |
|        - | 2887 | `	 * fatals or exceptions — those unwind past the closing instruction, so` |
|        - | 2888 | `	 * the depth is reset on the exception path rather than decremented here.` |
|        - | 2889 | `	 */` |
|   115901 | 2890 | `	if( pInstr->iP1 ){` |
|    58049 | 2891 | `		pVm->nErrSuppress++;` |
|    86856 | 2892 | `	}else if( pVm->nErrSuppress > 0 ){` |
|    57857 | 2893 | `		pVm->nErrSuppress--;` |
|    28903 | 2894 | `	}` |
|   115901 | 2895 | `	break;` |
|        - | 2896 | `/*` |
|        - | 2897 | ` * IS_A * * *` |
|        - | 2898 | ` *` |
|        - | 2899 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - | 2900 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - | 2901 | ` * holding a class name or an object).` |
|        - | 2902 | ` * Push TRUE on success. FALSE otherwise.` |
|        - | 2903 | ` */` |
|      586 | 2904 | `case PH7_OP_IS_A:{` |
|     1177 | 2905 | `	ph7_value *pNos = &pTos[-1];` |
|     1177 | 2906 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - | 2907 | `#ifdef UNTRUST` |
|        - | 2908 | `	if( pNos < pStack ){` |
|        - | 2909 | `		goto Abort;` |
|        - | 2910 | `	}` |
|        - | 2911 | `#endif` |
|        - | 2912 | `	/* php screens the CLASS operand before it looks at the subject: zend_fetch_class` |
|        - | 2913 | `	 * takes an object or a string and refuses everything else with` |
|        - | 2914 | ``	 * `Class name must be a valid object or a string`, whatever is on the left --`` |
|        - | 2915 | ``	 * `5 instanceof $arr` and `$obj instanceof $arr` are the same Error. PHL answered`` |
|        - | 2916 | `	 * FALSE for every one of them, so a typo'd or wrongly-typed class expression read` |
|        - | 2917 | `` 	 * as a clean "no" and the program carried on. The same refusal already guards `::` `` |
|        - | 2918 | ``	 * (VmExecOpMember) and now `new` (VmExecOpNew); this is the third door. */`` |
|     1177 | 2919 | `	if( (pTos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|       17 | 2920 | `		VmPopOperand(&pTos,1);` |
|       17 | 2921 | `		PH7_MemObjRelease(pTos);` |
|       17 | 2922 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       17 | 2923 | `		pTos->nIdx = SXU32_HIGH;` |
|       17 | 2924 | `		rc = VmThrowFromVm(&(*pVm),"Error","Class name must be a valid object or a string",` |
|        - | 2925 | `			sizeof("Class name must be a valid object or a string")-1);` |
|       17 | 2926 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2927 | `			goto Abort;` |
|        - | 2928 | `		}` |
|       17 | 2929 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2930 | `	}` |
|     1161 | 2931 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      957 | 2932 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      957 | 2933 | `		ph7_class *pClass = 0;` |
|        - | 2934 | `		/* Extract the target class */` |
|      957 | 2935 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - | 2936 | `			/* Instance already loaded */` |
|        3 | 2937 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      956 | 2938 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - | 2939 | `			/* self/static/parent, through the shared rail (PH7_VmResolveScopeName), and` |
|        - | 2940 | `			 * an ordinary name through the same non-autoloading extract as before.` |
|        - | 2941 | `			 *` |
|        - | 2942 | ``			 * This handler open-coded the three keywords and answered `self` with the`` |
|        - | 2943 | `			 * DECLARING class, which for a trait method is the TRAIT — and no object is` |
|        - | 2944 | ``			 * ever an instance of a trait, so `$x instanceof self` inside a trait method`` |
|        - | 2945 | `			 * was FALSE for every $x, including one of the very class that used it.` |
|        - | 2946 | ``			 * php composes a trait into the class, so `self` there is the USING class`` |
|        - | 2947 | ``			 * (PH7_VmPeekSelfClass states the rule once, and `self::`/`__CLASS__`/`` |
|        - | 2948 | ``			 * `new self` all already asked it). The same silent false answered inside a`` |
|        - | 2949 | `			 * CLOSURE declared in a trait method, whose stamped scope is the trait too.` |
|        - | 2950 | `			 *` |
|        - | 2951 | `			 * phpstan is where it showed: JustNullableTypeTrait::isSuperTypeOf() begins` |
|        - | 2952 | ``			 * `if ($type instanceof self)`, and with that never taken the trait fell`` |
|        - | 2953 | ``			 * through to `$type->isSubTypeOf($this)` — a mutual recursion with`` |
|        - | 2954 | `			 * IntegerRangeType::isSubTypeOf() that php terminates on the first line. */` |
|     1430 | 2955 | `			pClass = PH7_VmResolveScopeName(&(*pVm),` |
|      950 | 2956 | `				(const char *)SyBlobData(&pTos->sBlob),(sxu32)SyBlobLength(&pTos->sBlob));` |
|      475 | 2957 | `		}` |
|      957 | 2958 | `		if( pClass ){` |
|        - | 2959 | `			/* Perform the query */` |
|      955 | 2960 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|      475 | 2961 | `		}` |
|      476 | 2962 | `	}` |
|        - | 2963 | `	/* Push result */` |
|     1161 | 2964 | `	VmPopOperand(&pTos,1);` |
|     1161 | 2965 | `	PH7_MemObjRelease(pTos);` |
|     1161 | 2966 | `	pTos->x.iVal = iRes;` |
|     1161 | 2967 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     1161 | 2968 | `	break;` |
|        - | 2969 | `				 }` |
|        - | 2970 |  |
|        - | 2971 | `/*` |
|        - | 2972 | ` * LOADC P1 P2 *` |
|        - | 2973 | ` *` |
|        - | 2974 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - | 2975 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - | 2976 | ` */` |
|  7908844 | 2977 | `case PH7_OP_LOADC: {` |
|        - | 2978 | `	ph7_value *pObj;` |
|        - | 2979 | `	/* Reserve a room */` |
| 15823176 | 2980 | `	pTos++;` |
| 15823176 | 2981 | `	if( pInstr->iP1 & PH7_LOADC_NOKEY ){` |
|        - | 2982 | `		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */` |
|   778245 | 2983 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|   778245 | 2984 | `		SyBlobReset(&pTos->sBlob);` |
|   778245 | 2985 | `		pTos->iFlags \|= MEMOBJ_AUX_NOKEY;` |
|   778245 | 2986 | `		pTos->nIdx = SXU32_HIGH;` |
|   778245 | 2987 | `		break;` |
|        - | 2988 | `	}` |
| 15044936 | 2989 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
| 15044936 | 2990 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - | 2991 | `			SyHashEntry *pEntry;` |
|        - | 2992 | `			/* The name php looks up FIRST for an unqualified constant is decided at` |
|        - | 2993 | ``			 * COMPILE time and travels in p3: a `use const` import's FQN, or`` |
|        - | 2994 | ``			 * `current-namespace\NAME`. Absent (global scope, or an already-qualified`` |
|        - | 2995 | `			 * literal), the bare literal is the only name there is. Resolving it here` |
|        - | 2996 | `			 * rather than against the RUNTIME namespace is what makes a function keep` |
|        - | 2997 | `			 * its own namespace when it is called from another one, and what lets a` |
|        - | 2998 | `			 * namespaced constant shadow a global one of the same short name. */` |
|   162825 | 2999 | `			const char *zCand = (const char *)pInstr->p3;` |
|   162825 | 3000 | `			const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|   162825 | 3001 | `			sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - | 3002 | `			/* Both names are fixed by the instruction, so which one wins and what it` |
|        - | 3003 | `			 * resolves to can only change when hConstant does. A site that has already` |
|        - | 3004 | `			 * answered under the current generation answers again without hashing` |
|        - | 3005 | `			 * either -- see PH7_VmConstSiteAnswer for what that was costing. */` |
|   162825 | 3006 | `			pEntry = PH7_VmConstSiteAnswer(&(*pVm),pInstr);` |
|   162825 | 3007 | `			if( pEntry == 0 ){` |
|    10432 | 3008 | `				if( zCand ){` |
|       64 | 3009 | `					pEntry = SyHashGet(&pVm->hConstant,zCand,SyStrlen(zCand));` |
|       30 | 3010 | `				}` |
|        - | 3011 | `				/* The GLOBAL step — skipped when the candidate came from an import,` |
|        - | 3012 | `				 * which php resolves without any fallback. */` |
|    10432 | 3013 | `				if( pEntry == 0 && (pInstr->iP1 & PH7_LOADC_NOGLOBAL) == 0 ){` |
|    10382 | 3014 | `					pEntry = SyHashGet(&pVm->hConstant,(const void *)zLit,nLit);` |
|     5100 | 3015 | `				}` |
|    10432 | 3016 | `				PH7_VmConstSiteRecord(&(*pVm),pInstr,pEntry);` |
|     5125 | 3017 | `			}` |
|   162825 | 3018 | `			if( pEntry ){` |
|   162651 | 3019 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - | 3020 | `				/* Set a NULL default value */` |
|   162651 | 3021 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|   162651 | 3022 | `				SyBlobReset(&pTos->sBlob);` |
|        - | 3023 | `				/* Invoke the callback and deal with the expanded value */` |
|   162651 | 3024 | `				VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|        - | 3025 | `				/* Mark as constant */` |
|   162651 | 3026 | `				pTos->nIdx = SXU32_HIGH;` |
|   162651 | 3027 | `				break;` |
|        - | 3028 | `			}` |
|        - | 3029 | `			{` |
|        - | 3030 | `				/*` |
|        - | 3031 | `				 * php 8 has no bare-word fallback: an unresolved constant is a catchable` |
|        - | 3032 | `				 * Error, not its own name as a string. PH7 answered "X" for an unknown` |
|        - | 3033 | `				 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —` |
|        - | 3034 | `				 * silently became a string and flowed on. php names the name it looked` |
|        - | 3035 | `				 * for FIRST, so the message reports the candidate when there was one` |
|        - | 3036 | ``				 * ("Undefined constant \"B\NOPE\"" inside `namespace B;`).`` |
|        - | 3037 | `				 *` |
|        - | 3038 | `				 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call` |
|        - | 3039 | ``				 * boundary, so neither a bare `break` nor `goto Exception` is correct`` |
|        - | 3040 | `				 * here (see the macro).` |
|        - | 3041 | `				 */` |
|        - | 3042 | `				SyBlob sMsg;` |
|      179 | 3043 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      179 | 3044 | `				if( zCand ){` |
|        7 | 3045 | `					SyBlobFormat(&sMsg,"Undefined constant \"%s\"",zCand);` |
|        4 | 3046 | `				}else{` |
|      173 | 3047 | `					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);` |
|        - | 3048 | `				}` |
|      179 | 3049 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      179 | 3050 | `				SyBlobReset(&pTos->sBlob);` |
|      179 | 3051 | `				pTos->nIdx = SXU32_HIGH;` |
|      266 | 3052 | `				rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|       87 | 3053 | `					SyBlobLength(&sMsg));` |
|      179 | 3054 | `				SyBlobRelease(&sMsg);` |
|      179 | 3055 | `				if( rc == SXERR_ABORT ){` |
|       51 | 3056 | `					goto Abort;` |
|        - | 3057 | `				}` |
|      143 | 3058 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3059 | `			}` |
|        - | 3060 | `		}` |
| 14882116 | 3061 | `		PH7_MemObjLoad(pObj,pTos);` |
|  7444241 | 3062 | `	}else{` |
|        - | 3063 | `		/* Set a NULL value */` |
|      ! 0 | 3064 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 3065 | `	}` |
|        - | 3066 | `	/* Mark as constant */` |
| 14882116 | 3067 | `	pTos->nIdx = SXU32_HIGH;` |
| 14882116 | 3068 | `	break;` |
|        - | 3069 | `				  }` |
|        - | 3070 | `/*` |
|        - | 3071 | ` * LOAD: P1 * P3` |
|        - | 3072 | ` *` |
|        - | 3073 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - | 3074 | ` * from the P3 operand.` |
|        - | 3075 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - | 3076 | ` * the variable if non existent and push the NULL constant instead.` |
|        - | 3077 | ` */` |
|  7390651 | 3078 | `case PH7_OP_LOAD:{` |
|        - | 3079 | `	ph7_value *pObj;` |
|        - | 3080 | `	SyString sName;` |
| 14807564 | 3081 | `	if( pInstr->p3 == 0 ){` |
|        - | 3082 | `		/* Take the variable name from the top of the stack */` |
|        - | 3083 | `#ifdef UNTRUST` |
|        - | 3084 | `		if( pTos < pStack ){` |
|        - | 3085 | `			goto Abort;` |
|        - | 3086 | `		}` |
|        - | 3087 | `#endif` |
|        - | 3088 | `		/* Force a string cast — variable-variable name $$arr (user-visible) */` |
|        - | 3089 | `		{` |
|       46 | 3090 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|  7390696 | 3091 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3092 | `		}` |
|       44 | 3093 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       24 | 3094 | `	}else{` |
| 14807522 | 3095 | `		if( pInstr->nAux == 0 ){` |
|        - | 3096 | `			/* Measured once: the name is a compile-time buffer that never changes.` |
|        - | 3097 | `			 * VmNumberLocals fills this in for a body it walks; this is the door for` |
|        - | 3098 | `			 * one it does not (an include, an eval, a mini-program). */` |
|    56317 | 3099 | `			pInstr->nAux = (sxu32)SyStrlen((const char *)pInstr->p3);` |
|    28150 | 3100 | `		}` |
| 14807522 | 3101 | `		SyStringInitFromBuf(&sName,pInstr->p3,pInstr->nAux);` |
|        - | 3102 | `		/* Reserve a room for the target object */` |
| 14807522 | 3103 | `		pTos++;` |
|        - | 3104 | `	}` |
|        - | 3105 | `	{` |
|        - | 3106 | `	/* A name the compiler put in the instruction has a NUMBER in this body, so the` |
|        - | 3107 | `	 * frame answers it out of an array (PH7_VmExtractVarSlot); a variable-variable's` |
|        - | 3108 | `	 * name is a string on the stack and gets the plain door. */` |
| 22299421 | 3109 | `	int bThis = ( sName.nByte == sizeof("this")-1` |
|  7517051 | 3110 | `	           && SyMemcmp(sName.zString,"this",sizeof("this")-1) == 0` |
| 14907701 | 3111 | `	           && pInstr->iP2 != 1 /* isset()/empty() ask a QUESTION -- see below */ );` |
| 14807562 | 3112 | `	if( bThis \|\| pInstr->iP2 == 2 ){` |
|        - | 3113 | ``		/* One PEEK (no create) answers both questions below -- `$this` in a frame`` |
|        - | 3114 | `		 * with no receiver, and php's read-before-write warning -- where two` |
|        - | 3115 | `		 * separate lookups used to ask the same table the same thing twice for` |
|        - | 3116 | ``		 * every `$this` in every method body. */`` |
|  2289745 | 3117 | `		ph7_value *pPeek = pInstr->p3` |
|  1526995 | 3118 | `			? PH7_VmExtractVarSlot(&(*pVm),&sName,FALSE,pInstr->nSite,aInstr)` |
|   762745 | 3119 | `			: VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE);` |
|  1527000 | 3120 | `		if( pPeek == 0 ){` |
|       22 | 3121 | `			if( bThis ){` |
|        - | 3122 | ``				/* `$this` is not a variable: a frame with no receiver answers a READ`` |
|        - | 3123 | `				 * of it with php's catchable Error, not the undefined-variable` |
|        - | 3124 | ``				 * warning and a NULL. The null was the hazard -- `$this->m()` in a`` |
|        - | 3125 | `				 * plain function became "Call to a member function m() on null",` |
|        - | 3126 | ``				 * `var_dump($this)` printed NULL, and `f($this)` passed one on --`` |
|        - | 3127 | `				 * each a wrong ANSWER a few frames from the mistake. Asked BEFORE the` |
|        - | 3128 | ``				 * extract so the vivifying contexts (`$this ?? 'd'`, which php throws`` |
|        - | 3129 | `				 * for even though it swallows an ordinary undefined variable) reach it` |
|        - | 3130 | ``				 * too; `isset($this)`/`empty($this)` are php's one exemption, and they`` |
|        - | 3131 | `				 * answer false/true in silence. */` |
|       13 | 3132 | `				rc = VmThrowFromVm(&(*pVm),"Error","Using $this when not in object context",` |
|        - | 3133 | `					sizeof("Using $this when not in object context")-1);` |
|       13 | 3134 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3135 | `					goto Abort;` |
|        - | 3136 | `				}` |
|       29 | 3137 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      ! 0 | 3138 | `			}else{` |
|        - | 3139 | ``				/* Read-modify-write target (`$x++`, `$x .= 'a'`): php reads the`` |
|        - | 3140 | `				 * variable before writing, so it warns when it does not exist and` |
|        - | 3141 | `				 * THEN seeds it. The load below still creates the slot the operator` |
|        - | 3142 | ``				 * needs. A plain `=` never gets here -- it writes without reading,`` |
|        - | 3143 | `				 * and stays silent, as php does. */` |
|       10 | 3144 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        - | 3145 | `			}` |
|        4 | 3146 | `		}` |
|   764244 | 3147 | `	}` |
|        - | 3148 | `	/* Extract the requested memory object */` |
| 22198194 | 3149 | `	pObj = pInstr->p3` |
| 14807505 | 3150 | `		? PH7_VmExtractVarSlot(&(*pVm),&sName,pInstr->iP1 != 1,pInstr->nSite,aInstr)` |
|  7390664 | 3151 | `		: VmExtractMemObj(&(*pVm),&sName,TRUE,pInstr->iP1 != 1);` |
|        - | 3152 | `	}` |
| 14807550 | 3153 | `	if( pObj == 0 ){` |
|      451 | 3154 | `		if( pInstr->iP1 ){` |
|        - | 3155 | `			/* Reading a variable that does not exist. php raises E_WARNING` |
|        - | 3156 | `			 * "Undefined variable $x" and evaluates it as NULL; PHL used to` |
|        - | 3157 | `			 * yield NULL silently, which hid typo'd names. iP2 marks the reads` |
|        - | 3158 | `			 * that must stay quiet (isset/empty — see PH7_CompileVariable);` |
|        - | 3159 | `			 * vivifying contexts never get here because they pass iP1 = 0. */` |
|      451 | 3160 | `			if( pInstr->iP2 == 0 ){` |
|      193 | 3161 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|       94 | 3162 | `			}` |
|        - | 3163 | `			/* Variable not found,load NULL */` |
|      451 | 3164 | `			if( !pInstr->p3 ){` |
|       10 | 3165 | `				PH7_MemObjRelease(pTos);` |
|        6 | 3166 | `			}else{` |
|      443 | 3167 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 3168 | `			}` |
|      451 | 3169 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|      451 | 3170 | `			if( pInstr->iP2 == 3 ){` |
|        - | 3171 | `				/* D1 deferred call argument: the variable does not exist, but we do not` |
|        - | 3172 | `				 * know yet whether the callee wants it by-ref (materialize + bind) or` |
|        - | 3173 | `				 * by-value (warn + pass NULL). Tag the slot and stash the variable name` |
|        - | 3174 | `				 * (a VM-lifetime bytecode string, nothing to free) so OP_CALL's` |
|        - | 3175 | `				 * PH7_VmResolveDeferredArgs can decide once the callee is resolved. */` |
|      174 | 3176 | `				pTos->iFlags \|= MEMOBJ_AUX_DEFERRED;` |
|      174 | 3177 | `				pTos->x.pOther = pInstr->p3;` |
|       85 | 3178 | `			}` |
|      451 | 3179 | `			break;` |
|      ! 0 | 3180 | `		}else{` |
|        - | 3181 | `			/* Fatal error */` |
|      ! 0 | 3182 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 3183 | `			goto Abort;` |
|        - | 3184 | `		}` |
|        - | 3185 | `	}` |
|        - | 3186 | `	/* Load variable contents */` |
| 14807104 | 3187 | `	PH7_MemObjLoad(pObj,pTos);` |
| 14807104 | 3188 | `	pTos->nIdx = pObj->nIdx;` |
| 14807104 | 3189 | `	break;` |
|        - | 3190 | `				   }` |
|        - | 3191 | `/*` |
|        - | 3192 | ` * LOAD_MAP P1 * *` |
|        - | 3193 | ` *` |
|        - | 3194 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - | 3195 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - | 3196 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - | 3197 | ` */` |
|   150957 | 3198 | `case PH7_OP_LOAD_MAP: {` |
|        - | 3199 | `	VmOpRc rcOp;` |
|   301775 | 3200 | `	sState.pTos = pTos;` |
|   301775 | 3201 | `	sState.pc = pc;` |
|   301775 | 3202 | `	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);` |
|   301775 | 3203 | `	pTos = sState.pTos;` |
|   301775 | 3204 | `	pc = sState.pc;` |
|   301775 | 3205 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3206 | `		goto Abort;` |
|   301775 | 3207 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       46 | 3208 | `		goto Exception;` |
|        - | 3209 | `	}` |
|   301733 | 3210 | `	break;` |
|        - | 3211 | `					  }` |
|        - | 3212 | `/*` |
|        - | 3213 | ` * LOAD_LIST: P1 * *` |
|        - | 3214 | ` *` |
|        - | 3215 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - | 3216 | ` * This is the VM implementation of the list() PHP construct.` |
|        - | 3217 | ` * Caveats:` |
|        - | 3218 | ` *  This implementation support only a single nesting level.` |
|        - | 3219 | ` */` |
|   196585 | 3220 | `case PH7_OP_LOAD_LIST: {` |
|        - | 3221 | `	VmOpRc rcOp;` |
|   393168 | 3222 | `	sState.pTos = pTos;` |
|   393168 | 3223 | `	sState.pc = pc;` |
|   393168 | 3224 | `	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);` |
|   393168 | 3225 | `	pTos = sState.pTos;` |
|   393168 | 3226 | `	pc = sState.pc;` |
|   393168 | 3227 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3228 | `		goto Abort;` |
|   393168 | 3229 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 3230 | `		goto Exception;` |
|        - | 3231 | `	}` |
|   393162 | 3232 | `	break;` |
|        - | 3233 | `					  }` |
|        - | 3234 | `/*` |
|        - | 3235 | ` * LOAD_IDX: P1 P2 *` |
|        - | 3236 | ` *` |
|        - | 3237 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - | 3238 | ` * from the stack.` |
|        - | 3239 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - | 3240 | ` * instead.` |
|        - | 3241 | ` */` |
|   667632 | 3242 | `case PH7_OP_LOAD_IDX: {` |
|        - | 3243 | `	VmOpRc rcOp;` |
|  1341081 | 3244 | `	sState.pTos = pTos;` |
|  1341081 | 3245 | `	sState.pc = pc;` |
|  1341081 | 3246 | `	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);` |
|  1341081 | 3247 | `	pTos = sState.pTos;` |
|  1341081 | 3248 | `	pc = sState.pc;` |
|  1341081 | 3249 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3250 | `		goto Abort;` |
|  1341081 | 3251 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      102 | 3252 | `		goto Exception;` |
|        - | 3253 | `	}` |
|  1340983 | 3254 | `	break;` |
|        - | 3255 | `					  }` |
|        - | 3256 | `/*` |
|        - | 3257 | ` * LOAD_CLOSURE * * P3` |
|        - | 3258 | ` *` |
|        - | 3259 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - | 3260 | ` * name in the stack.` |
|        - | 3261 | ` */` |
|     9838 | 3262 | `case PH7_OP_LOAD_CLOSURE: {` |
|        - | 3263 | `	VmOpRc rcOp;` |
|    19438 | 3264 | `	sState.pTos = pTos;` |
|    19438 | 3265 | `	sState.pc = pc;` |
|    19438 | 3266 | `	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);` |
|    19438 | 3267 | `	pTos = sState.pTos;` |
|    19438 | 3268 | `	pc = sState.pc;` |
|    19438 | 3269 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3270 | `		goto Abort;` |
|    19438 | 3271 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3272 | `		goto Exception;` |
|        - | 3273 | `	}` |
|    19438 | 3274 | `	break;` |
|        - | 3275 | `					  }` |
|        - | 3276 | `/*` |
|        - | 3277 | ` * LOAD_FCC P1 * *` |
|        - | 3278 | ` *` |
|        - | 3279 | ` * First-class callable: wrap the callee in a Closure object instead of calling it.` |
|        - | 3280 | ` *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS` |
|        - | 3281 | ` *           (from the callee's OP_LOADC). Replace it in place with a Closure whose` |
|        - | 3282 | ` *           $__fn is that name; the existing string-callable dispatch resolves it.` |
|        - | 3283 | ` *           (OOM degrades to leaving the name string on the stack — still callable.)` |
|        - | 3284 | ` *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name` |
|        - | 3285 | ` *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds` |
|        - | 3286 | ` *           $this (scope = its class); a class-name-string target is a static callable` |
|        - | 3287 | ` *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —` |
|        - | 3288 | ` *           the popped target leaves no name string to keep.)` |
|        - | 3289 | ` */` |
|      176 | 3290 | `case PH7_OP_LOAD_FCC:{` |
|      357 | 3291 | `	if( pInstr->iP1 == 1 ){` |
|        - | 3292 | ``		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)`` |
|        - | 3293 | `		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —` |
|        - | 3294 | `		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,` |
|        - | 3295 | ``		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a`` |
|        - | 3296 | `		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is` |
|        - | 3297 | `		 * (graceful degradation), and so is the original on OOM — still whatever it was. */` |
|        - | 3298 | `		ph7_class_instance *pCloObj;` |
|        - | 3299 | `		sxi32 nFccBrc;` |
|        - | 3300 | `		const void *pFccRes;` |
|      175 | 3301 | `		if( VmValueIsClosure(pVm, pTos) ){` |
|       16 | 3302 | `			break;` |
|        - | 3303 | `		}` |
|        - | 3304 | `		/* php's global fallback for an UNQUALIFIED function name written inside a` |
|        - | 3305 | `		 * namespace: the current namespace first, the global one after. The compiler` |
|        - | 3306 | `		 * qualified this name and marks the instruction (iP2==1) when it did, so the` |
|        - | 3307 | `		 * fallback happens here — the OP_CALL path does the same thing from its arg map,` |
|        - | 3308 | ``		 * which an FCC has none of. `strlen(...)` in a namespaced file was`` |
|        - | 3309 | ``		 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|      156 | 3310 | `		if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING)` |
|       14 | 3311 | `			&& !PH7_VmIsCallable(pVm,pTos,TRUE) ){` |
|        7 | 3312 | `			const char *zFccName = (const char *)SyBlobData(&pTos->sBlob);` |
|        7 | 3313 | `			sxu32 nFccName = SyBlobLength(&pTos->sBlob);` |
|        7 | 3314 | `			const char *zFccShort = zFccName;` |
|        - | 3315 | `			sxu32 iFccPos;` |
|      165 | 3316 | `			for( iFccPos = 0 ; iFccPos < nFccName ; ++iFccPos ){` |
|      159 | 3317 | `				if( zFccName[iFccPos] == '\\' ){` |
|        7 | 3318 | `					zFccShort = &zFccName[iFccPos + 1];` |
|        3 | 3319 | `				}` |
|       80 | 3320 | `			}` |
|        7 | 3321 | `			if( zFccShort != zFccName ){` |
|        - | 3322 | `				ph7_value sFccShort;` |
|        7 | 3323 | `				PH7_MemObjInit(pVm,&sFccShort);` |
|       10 | 3324 | `				PH7_MemObjStringAppend(&sFccShort,zFccShort,` |
|        6 | 3325 | `					(sxu32)(nFccName - (sxu32)(zFccShort - zFccName)));` |
|        7 | 3326 | `				if( PH7_VmIsCallable(pVm,&sFccShort,TRUE) ){` |
|        5 | 3327 | `					PH7_MemObjStore(&sFccShort,pTos);` |
|        2 | 3328 | `				}` |
|        7 | 3329 | `				PH7_MemObjRelease(&sFccShort);` |
|        3 | 3330 | `			}` |
|        3 | 3331 | `		}` |
|        - | 3332 | `		/* The array shape's class lookup can run an autoloader that throws; php propagates` |
|        - | 3333 | `		 * THAT exception and never reports the callable bad, exactly as at the OP_CALL sites. */` |
|      160 | 3334 | `		nFccBrc = pVm->nBoundaryRc;` |
|      160 | 3335 | `		pFccRes = (const void *)pVm->pResumeFrame;` |
|      160 | 3336 | `		pCloObj = VmFccWrapValue(pVm, pTos);` |
|      160 | 3337 | `		if( pCloObj ){` |
|      130 | 3338 | `			PH7_MemObjRelease(pTos);` |
|        - | 3339 | `			/* The fresh instance's own reference is this stack slot's (see OP_LOAD_CLOSURE) */` |
|      130 | 3340 | `			pTos->x.pOther = pCloObj;` |
|      130 | 3341 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       67 | 3342 | `		}else{` |
|        - | 3343 | `			/* php refuses a non-callable HERE, with the direct dispatch's own wording — the` |
|        - | 3344 | ``			 * `(...)` does not make a bad callable acceptable, it just defers the call. */`` |
|        - | 3345 | `			char zFccMsg[192];` |
|       31 | 3346 | `			const char *zFccBad = VmFccValueError(&(*pVm),pTos,zFccMsg,sizeof(zFccMsg));` |
|       31 | 3347 | `			int bFccRaised = PH7_VmClassLookupRaised(&(*pVm),nFccBrc,pFccRes);` |
|       31 | 3348 | `			if( zFccBad \|\| bFccRaised ){` |
|        - | 3349 | `				sxi32 rcFcc;` |
|       31 | 3350 | `				PH7_MemObjRelease(pTos);` |
|       31 | 3351 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       31 | 3352 | `				pTos->nIdx = SXU32_HIGH;` |
|       31 | 3353 | `				if( bFccRaised ){` |
|      ! 0 | 3354 | `					rcFcc = pVm->nBoundaryRc;` |
|      ! 0 | 3355 | `					pVm->nBoundaryRc = 0;` |
|      ! 0 | 3356 | `					if( rcFcc == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 3357 | `					rc = PH7_EXCEPTION;` |
|       15 | 3358 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3359 | `				}` |
|       31 | 3360 | `				rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccBad,(sxu32)SyStrlen(zFccBad));` |
|       31 | 3361 | `				if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       31 | 3362 | `				rc = rcFcc;` |
|       57 | 3363 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3364 | `			}` |
|        - | 3365 | `		}` |
|       67 | 3366 | `	}else{` |
|        - | 3367 | `		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]` |
|        - | 3368 | `		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its` |
|        - | 3369 | `		 * class); a class-name string target is a static callable (scope = that class). */` |
|      187 | 3370 | `		ph7_value *pTarget = &pTos[-1];` |
|        - | 3371 | `		SyString sName;` |
|        - | 3372 | `		ph7_class_instance *pCloObj;` |
|      187 | 3373 | `		ph7_class *pFccCls = 0;` |
|      187 | 3374 | `		ph7_class_instance *pFccRecv = 0;` |
|      187 | 3375 | `		const char *zFccErr = 0;` |
|        - | 3376 | `		char zFccMsg[192];` |
|      187 | 3377 | `		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));` |
|      187 | 3378 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      109 | 3379 | `			pFccRecv = (ph7_class_instance *)pTarget->x.pOther;` |
|      109 | 3380 | `			pFccCls = pFccRecv->pClass;` |
|      109 | 3381 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pFccCls) ){` |
|        - | 3382 | ``				/* `$inc->m(...)` resolves the method at CREATION, so php's`` |
|        - | 3383 | `				 * incomplete-object call Error is raised here, not at a later` |
|        - | 3384 | `				 * invocation. */` |
|        - | 3385 | `				SyBlob sIncErr;` |
|        - | 3386 | `				sxi32 rcInc;` |
|        3 | 3387 | `				SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|        3 | 3388 | `				PH7_VmIncompleteMsg(&(*pVm),pFccRecv,"call a method",&sIncErr);` |
|        3 | 3389 | `				VmPopOperand(&pTos,1);       /* the method name */` |
|        3 | 3390 | `				PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|        3 | 3391 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 3392 | `				pTos->nIdx = SXU32_HIGH;` |
|        4 | 3393 | `				rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|        1 | 3394 | `					SyBlobLength(&sIncErr));` |
|        3 | 3395 | `				SyBlobRelease(&sIncErr);` |
|        3 | 3396 | `				if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|        3 | 3397 | `				rc = rcInc;` |
|        3 | 3398 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        5 | 3399 | `			}` |
|      133 | 3400 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|        - | 3401 | ``			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class`` |
|        - | 3402 | `			 * now, so the closure binds the concrete scope (matching PHP). */` |
|       82 | 3403 | `			pFccCls = VmFccResolveScope(pVm, pTarget);` |
|       39 | 3404 | `		}` |
|      185 | 3405 | `		if( pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING) ){` |
|        - | 3406 | `			/* php resolves the member HERE, through the same lookup the call would use:` |
|        - | 3407 | `			 * every refusal a call would raise is raised at CREATION, and a non-static` |
|        - | 3408 | `			 * method named through a class binds the calling frame's own $this. */` |
|      185 | 3409 | `			ph7_class_instance *pRecvOut = 0;` |
|      275 | 3410 | `			zFccErr = VmFccMemberError(&(*pVm),pFccCls,` |
|      180 | 3411 | `				(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|       90 | 3412 | `				SyStringData(&sName),SyStringLength(&sName),` |
|      180 | 3413 | `				(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|       90 | 3414 | `				&pRecvOut,zFccMsg,sizeof(zFccMsg));` |
|      185 | 3415 | `			if( pRecvOut ){` |
|       13 | 3416 | ``				pFccRecv = pRecvOut; /* the receiver php binds into a `C::m(...)` callable */`` |
|        6 | 3417 | `			}` |
|       90 | 3418 | `		}` |
|      185 | 3419 | `		if( zFccErr ){` |
|        - | 3420 | `			sxi32 rcFcc;` |
|       24 | 3421 | `			VmPopOperand(&pTos,1);       /* the method name */` |
|       24 | 3422 | `			PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|       24 | 3423 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       24 | 3424 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 | 3425 | `			rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccErr,(sxu32)SyStrlen(zFccErr));` |
|       24 | 3426 | `			if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       24 | 3427 | `			rc = rcFcc;` |
|       26 | 3428 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3429 | `		}` |
|      163 | 3430 | `		if( pFccCls == 0 ){` |
|      ! 0 | 3431 | `			pCloObj = 0;` |
|      163 | 3432 | `		}else if( pFccRecv ){` |
|      109 | 3433 | `			pCloObj = VmCreateClosure(pVm, &sName, pFccRecv, &pFccRecv->pClass->sName);` |
|       57 | 3434 | `		}else{` |
|       58 | 3435 | `			pCloObj = VmCreateClosure(pVm, &sName, 0, &pFccCls->sName);` |
|        - | 3436 | `		}` |
|      163 | 3437 | `		if( pCloObj ){` |
|        - | 3438 | ``			/* `$o->m(...)` / `C::m(...)` names a METHOD, whatever the class turns out to`` |
|        - | 3439 | `			 * declare: the unwrap must not go looking for a FUNCTION of that name, and a` |
|        - | 3440 | `			 * name the class answers only through __call is still a method call. */` |
|      163 | 3441 | `			pCloObj->iFlags \|= VM_INSTANCE_FCC_METHOD;` |
|        - | 3442 | `			/* The screen above already ran, HERE, where php runs it — so record that this` |
|        - | 3443 | `			 * closure's callee is settled and the invocation must not re-decide it. A name` |
|        - | 3444 | `			 * that resolved to the catch-all instead keeps routing there. */` |
|      163 | 3445 | `			if( PH7_VmFccMethodIsDirect(&(*pVm),pFccCls,SyStringData(&sName),SyStringLength(&sName)) ){` |
|      149 | 3446 | `				pCloObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       72 | 3447 | `			}` |
|       79 | 3448 | `		}` |
|        - | 3449 | `		/* Pop the method name and the target, push the Closure. */` |
|      163 | 3450 | `		PH7_MemObjRelease(pTos);` |
|      163 | 3451 | `		pTos--;` |
|      163 | 3452 | `		PH7_MemObjRelease(pTos);` |
|      163 | 3453 | `		if( pCloObj ){` |
|        - | 3454 | `			/* The fresh instance's own reference is this stack slot's (see OP_LOAD_CLOSURE) */` |
|      163 | 3455 | `			pTos->x.pOther = pCloObj;` |
|      163 | 3456 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       84 | 3457 | `		}else{` |
|      ! 0 | 3458 | `			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */` |
|        - | 3459 | `		}` |
|        - | 3460 | `	}` |
|      289 | 3461 | `	break;` |
|        - | 3462 | `					 }` |
|        - | 3463 | `/*` |
|        - | 3464 | ` * STORE * P2 P3` |
|        - | 3465 | ` *` |
|        - | 3466 | ` * Perform a store (Assignment) operation.` |
|        - | 3467 | ` */` |
|   712184 | 3468 | `case PH7_OP_STORE: {` |
|        - | 3469 | `	ph7_value *pObj;` |
|        - | 3470 | `	SyString sName;` |
|        - | 3471 | `#ifdef UNTRUST` |
|        - | 3472 | `	if( pTos < pStack ){` |
|        - | 3473 | `		goto Abort;` |
|        - | 3474 | `	}` |
|        - | 3475 | `#endif` |
|  1429513 | 3476 | `	if( pInstr->iP2 ){` |
|        - | 3477 | `		sxu32 nIdx;` |
|        - | 3478 | `		sxi32 rcT;` |
|        - | 3479 | `		/* Member store operation */` |
|   105167 | 3480 | `		nIdx = pTos->nIdx;` |
|   105167 | 3481 | `		VmPopOperand(&pTos,1);` |
|   105167 | 3482 | `		if( pVm->pMagicSetThis ){` |
|        - | 3483 | `			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain` |
|        - | 3484 | `			 * store to a missing/inaccessible property on a class declaring __set.` |
|        - | 3485 | `			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on` |
|        - | 3486 | `			 * the stack as the assignment expression's result — php's semantics` |
|        - | 3487 | `			 * (no property is created; a throw rides the boundary rail). */` |
|      786 | 3488 | `			ph7_class_instance *pSetThis = pVm->pMagicSetThis;` |
|        - | 3489 | `			SyString sSetName;` |
|      786 | 3490 | `			pVm->pMagicSetThis = 0;` |
|      786 | 3491 | `			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));` |
|      786 | 3492 | `			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);` |
|      786 | 3493 | `			PH7_ClassInstanceUnref(pSetThis);` |
|      786 | 3494 | `			SyBlobReset(&pVm->sMagicSetName);` |
|      786 | 3495 | `			break;` |
|        - | 3496 | `		}` |
|   104385 | 3497 | `		if( pVm->pHookSetThis ){` |
|        - | 3498 | `			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a` |
|        - | 3499 | `			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with` |
|        - | 3500 | `			 * the rvalue (which stays on the stack as the assignment expression's` |
|        - | 3501 | ``			 * result); a `set => expr` hook's return value is stored into the`` |
|        - | 3502 | `			 * BACKING slot with the ordinary typed enforcement. A property with` |
|        - | 3503 | `			 * only a get hook is php's catchable "is read-only" Error. */` |
|       55 | 3504 | `			ph7_class_instance *pHThis = pVm->pHookSetThis;` |
|       55 | 3505 | `			ph7_class_attr *pHAttr = pVm->pHookSetAttr;` |
|       55 | 3506 | `			sxu32 nBackIdx = pVm->nHookSetIdx;` |
|        - | 3507 | `			sxi32 rcHs;` |
|       55 | 3508 | `			pVm->pHookSetThis = 0;` |
|       55 | 3509 | `			pVm->pHookSetAttr = 0;` |
|       55 | 3510 | `			pVm->nHookSetIdx = SXU32_HIGH;` |
|       55 | 3511 | `			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);` |
|       55 | 3512 | `			PH7_ClassInstanceUnref(pHThis);` |
|       55 | 3513 | `			if( rcHs == PH7_ABORT ){` |
|      ! 0 | 3514 | `				goto Abort;` |
|        - | 3515 | `			}` |
|       55 | 3516 | `			break;` |
|        - | 3517 | `		}` |
|   104333 | 3518 | `		if( nIdx == SXU32_HIGH ){` |
|        - | 3519 | `			/* No slot behind the property: the receiver is a TEMPORARY nothing else` |
|        - | 3520 | ``			 * holds (`mk()->p = 5` on a freshly built object, `(new A)->p` where the`` |
|        - | 3521 | `			 * compile-time screen lets it through). php performs the write on the` |
|        - | 3522 | `			 * doomed object and says nothing — the object is gone at the end of the` |
|        - | 3523 | `			 * statement, so the store is unobservable either way. PHL announced it` |
|        - | 3524 | ``			 * with `Cannot perform assignment on a constant class attribute,PH7 is`` |
|        - | 3525 | ``			 * loading NULL`, a diagnostic php has no equivalent of. Every case that`` |
|        - | 3526 | `			 * IS a refusal — a class constant, a hooked or handler-backed property —` |
|        - | 3527 | `			 * is decided before this point now. */` |
|        3 | 3528 | `			pTos->nIdx = SXU32_HIGH;` |
|        2 | 3529 | `		}else{` |
|        - | 3530 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - | 3531 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|   104331 | 3532 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);` |
|   104331 | 3533 | `			if( rcT == PH7_ABORT ){` |
|       13 | 3534 | `				goto Abort;` |
|        - | 3535 | `			}` |
|   104321 | 3536 | `			if( rcT == PH7_EXCEPTION ){` |
|        - | 3537 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - | 3538 | `				 * control to the nearest catch block if any (draining any` |
|        - | 3539 | `				 * abandoned outer-expression operands to the try's base),` |
|        - | 3540 | `				 * otherwise propagate out of the VM loop. */` |
|   100211 | 3541 | `				VmPopOperand(&pTos,1);` |
|        - | 3542 | `				{` |
|        - | 3543 | `					sxi32 iRp;` |
|   100211 | 3544 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|   400121 | 3545 | `						PH7_RESUME_DRAIN()` |
|   100115 | 3546 | `						pc = iRp;` |
|   100115 | 3547 | `						break;` |
|        - | 3548 | `					}` |
|        - | 3549 | `				}` |
|      108 | 3550 | `				goto Exception;` |
|        - | 3551 | `			}` |
|        - | 3552 | `			/* Point to the desired memory object */` |
|     4115 | 3553 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|     4115 | 3554 | `			if( pObj ){` |
|        - | 3555 | `				/* Perform the store operation */` |
|     4115 | 3556 | `				PH7_MemObjStore(pTos,pObj);` |
|     2055 | 3557 | `			}` |
|        - | 3558 | `		}` |
|     4117 | 3559 | `		break;` |
|  1324351 | 3560 | `	}else if( pInstr->p3 == 0 ){` |
|        - | 3561 | `		/* Take the variable name from the next on the stack (user-visible: a` |
|        - | 3562 | `		 * variable-variable NAME $$arr warns on an array) */` |
|        - | 3563 | `		{` |
|       28 | 3564 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       28 | 3565 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3566 | `		}` |
|       26 | 3567 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       26 | 3568 | `		pTos--;` |
|        - | 3569 | `#ifdef UNTRUST` |
|        - | 3570 | `		if( pTos < pStack  ){` |
|        - | 3571 | `			goto Abort;` |
|        - | 3572 | `		}` |
|        - | 3573 | `#endif` |
|       14 | 3574 | `	}else{` |
|  1324325 | 3575 | `		if( pInstr->nAux == 0 ){` |
|        - | 3576 | `			/* Measured once, like OP_LOAD's: the name is a compile-time buffer.` |
|        - | 3577 | `			 * VmNumberLocals fills it in for a body it walks. */` |
|    19304 | 3578 | `			pInstr->nAux = (sxu32)SyStrlen((const char *)pInstr->p3);` |
|     9648 | 3579 | `		}` |
|  1324325 | 3580 | `		SyStringInitFromBuf(&sName,pInstr->p3,pInstr->nAux);` |
|        - | 3581 | `	}` |
|  1324344 | 3582 | `	if( sName.nByte == sizeof("GLOBALS")-1` |
|   667974 | 3583 | `	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){` |
|        6 | 3584 | `		if( pInstr->p3 ){` |
|        - | 3585 | `			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time` |
|        - | 3586 | `			 * fatal there; raised at the store site here with the same` |
|        - | 3587 | `			 * message and the same non-catchable outcome). Element writes` |
|        - | 3588 | `			 * are unaffected. */` |
|        3 | 3589 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3590 | `				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 3591 | `			pVm->iExitStatus = 255;` |
|        3 | 3592 | `			pVm->bHaltRequested = 1;` |
|        3 | 3593 | `			goto Abort;` |
|        - | 3594 | `		}` |
|        - | 3595 | `		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's` |
|        - | 3596 | `		 * compile-time case: php quietly creates an ordinary symbol-table` |
|        - | 3597 | `		 * entry named GLOBALS, leaving the auto-global view intact. */` |
|        3 | 3598 | `		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);` |
|        3 | 3599 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 3600 | `		break;` |
|        - | 3601 | `	}` |
|        - | 3602 | `	/* Extract the desired variable and if not available dynamically create it */` |
|  1983945 | 3603 | `	pObj = pInstr->p3` |
|  1324318 | 3604 | `		? PH7_VmExtractVarSlot(&(*pVm),&sName,TRUE,pInstr->nSite,aInstr)` |
|   659611 | 3605 | `		: VmExtractMemObj(&(*pVm),&sName,TRUE,TRUE);` |
|  1324345 | 3606 | `	if( pObj == 0 ){` |
|      ! 0 | 3607 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 3608 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 3609 | `		goto Abort;` |
|        - | 3610 | `	}` |
|  1324345 | 3611 | `	if( !pInstr->p3 ){` |
|       24 | 3612 | `		PH7_MemObjRelease(&pTos[1]);` |
|       11 | 3613 | `	}` |
|        - | 3614 | ``	/* A plain VARIABLE can BE a typed property's slot: `$r = &$o->n` aliases it,`` |
|        - | 3615 | `	 * and so does a by-ref parameter bound to one. php enforces the declared type` |
|        - | 3616 | ``	 * on this store exactly as on `$o->n = v` -- with a sentence of its own, which`` |
|        - | 3617 | `	 * names the property HOLDING the reference -- and PHL wrote through it` |
|        - | 3618 | `` 	 * unchecked, leaving a string in an `int` property and an array in a `string` `` |
|        - | 3619 | `	 * one. The store filter's table answers in one lookup and is skipped outright` |
|        - | 3620 | `	 * when nothing has registered a slot; measured at noise level on an` |
|        - | 3621 | `	 * object-and-array workload (a loop of nothing but plain stores is where the` |
|        - | 3622 | `	 * lookup shows at all). */` |
|  1324345 | 3623 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) > 0 && pObj->nIdx != SXU32_HIGH ){` |
|   492189 | 3624 | `		sxi32 rcRef = VmEnforcePropertyTypeOnStore(&(*pVm),pObj->nIdx,pTos,` |
|        - | 3625 | `			VM_TYPED_STORE_VIA_REF);` |
|   492189 | 3626 | `		if( rcRef == PH7_ABORT ){` |
|      ! 0 | 3627 | `			goto Abort;` |
|        - | 3628 | `		}` |
|   492189 | 3629 | `		if( rcRef == PH7_EXCEPTION ){` |
|       17 | 3630 | `			VmPopOperand(&pTos,1);` |
|        - | 3631 | `			{` |
|        - | 3632 | `				sxi32 iRp;` |
|       17 | 3633 | `				if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 3634 | `					PH7_RESUME_DRAIN()` |
|      ! 0 | 3635 | `					pc = iRp;` |
|      ! 0 | 3636 | `					break;` |
|        - | 3637 | `				}` |
|        - | 3638 | `			}` |
|       17 | 3639 | `			goto Exception;` |
|        - | 3640 | `		}` |
|   247011 | 3641 | `	}` |
|        - | 3642 | `	/* Perform the store operation */` |
|  1324329 | 3643 | `	PH7_MemObjStore(pTos,pObj);` |
|  1324329 | 3644 | `	break;` |
|        - | 3645 | `				   }` |
|        - | 3646 | `/*` |
|        - | 3647 | ` * STORE_IDX:   P1 * P3` |
|        - | 3648 | ` * STORE_IDX_R: P1 * P3` |
|        - | 3649 | ` *` |
|        - | 3650 | ` * Perfrom a store operation an a hashmap entry.` |
|        - | 3651 | ` */` |
|   273750 | 3652 | `case PH7_OP_STORE_IDX:` |
|        - | 3653 | `case PH7_OP_STORE_IDX_REF: {` |
|        - | 3654 | `	VmOpRc rcOp;` |
|   547280 | 3655 | `	sState.pTos = pTos;` |
|   547280 | 3656 | `	sState.pc = pc;` |
|   547280 | 3657 | `	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);` |
|   547280 | 3658 | `	pTos = sState.pTos;` |
|   547280 | 3659 | `	pc = sState.pc;` |
|   547280 | 3660 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3661 | `		goto Abort;` |
|   547278 | 3662 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       56 | 3663 | `		goto Exception;` |
|        - | 3664 | `	}` |
|   547226 | 3665 | `	break;` |
|        - | 3666 | `					  }` |
|        - | 3667 | `/*` |
|        - | 3668 | ` * INCR: P1 * *` |
|        - | 3669 | ` *` |
|        - | 3670 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - | 3671 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - | 3672 | ` * the stack and increment after that.` |
|        - | 3673 | ` */` |
|   476487 | 3674 | `case PH7_OP_INCR:` |
|        - | 3675 | `#ifdef UNTRUST` |
|        - | 3676 | `	if( pTos < pStack ){` |
|        - | 3677 | `		goto Abort;` |
|        - | 3678 | `	}` |
|        - | 3679 | `#endif` |
|        - | 3680 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3681 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3682 | `	 * — which otherwise skips object/array/resource operands. */` |
|   955059 | 3683 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3684 | `	/* php refuses to increment a string OFFSET, whatever byte it holds. */` |
|   955051 | 3685 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3686 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3687 | `	 * php's TypeError, raised BEFORE any set dispatch (the type guard below` |
|        - | 3688 | `	 * would skip the mutation and the tail write-back would otherwise call` |
|        - | 3689 | `	 * the set hook with the unchanged value). */` |
|   955030 | 3690 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|   478574 | 3691 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        5 | 3692 | `		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        5 | 3693 | `		if( VM_HOOK_PEND_IS_RMW(pTopInc->iKind) && pTopInc->nScratchIdx == pTos->nIdx ){` |
|        - | 3694 | `			SyBlob sErrMsg;` |
|        5 | 3695 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 | 3696 | `			VmIncDecTypeErrorMsg(pTos,TRUE,&sErrMsg);` |
|        5 | 3697 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        5 | 3698 | `			VmHookRmwDropTop(&(*pVm));` |
|        5 | 3699 | `			pTos->nIdx = SXU32_HIGH;` |
|        5 | 3700 | `			break;` |
|        - | 3701 | `		}` |
|      ! 0 | 3702 | `	}` |
|   955031 | 3703 | `	PH7_INCDEC_NATIVE_ARITH("+")` |
|   955029 | 3704 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3705 | `		/* php's ++ operand contract: an array, object or resource is a catchable` |
|        - | 3706 | `		 * TypeError, not the silent no-op this used to be. Settle the operand` |
|        - | 3707 | `		 * (the op's result slot) BEFORE throwing, like the arithmetic sites. */` |
|        - | 3708 | `		SyBlob sIncMsg;` |
|        - | 3709 | `		sxi32 rcInc;` |
|       25 | 3710 | `		SyBlobInit(&sIncMsg,&pVm->sAllocator);` |
|       25 | 3711 | `		VmIncDecTypeErrorMsg(pTos,TRUE,&sIncMsg);` |
|       25 | 3712 | `		PH7_MemObjRelease(pTos);` |
|       25 | 3713 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       25 | 3714 | `		pTos->nIdx = SXU32_HIGH;` |
|       37 | 3715 | `		rcInc = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sIncMsg),` |
|       12 | 3716 | `			SyBlobLength(&sIncMsg));` |
|       25 | 3717 | `		SyBlobRelease(&sIncMsg);` |
|       25 | 3718 | `		if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|       25 | 3719 | `		rc = rcInc;` |
|       25 | 3720 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3721 | `	}` |
|        - | 3722 | ``	/* php 8.3+: `++` on a BOOL is a no-op it warns about (E_WARNING, errno 2 —`` |
|        - | 3723 | ``	 * same shape as the null `--` below), where PH7 coerced true to int 2. */`` |
|   955005 | 3724 | `	if( pTos->iFlags & MEMOBJ_BOOL ){` |
|        7 | 3725 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3726 | `			"Increment on type bool has no effect, this will change in the next major version of PHP");` |
|        3 | 3727 | `	}` |
|   955005 | 3728 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_BOOL)) == 0 ){` |
|   954999 | 3729 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3730 | `			ph7_value *pObj;` |
|   954987 | 3731 | `			if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   954987 | 3732 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3733 | `					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric` |
|        - | 3734 | `					 * string (it points at str_increment() instead); PHL rejects it. */` |
|        - | 3735 | `					SyBlob sErrMsg;` |
|        3 | 3736 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3737 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3738 | `						"Increment on a non-numeric string is not supported, use str_increment() instead",` |
|        - | 3739 | `						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);` |
|        3 | 3740 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3741 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3742 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3743 | `					break;` |
|      ! 0 | 3744 | `				}else{` |
|        - | 3745 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - | 3746 | `					 * original value: pTos may alias pObj's blob via` |
|        - | 3747 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - | 3748 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - | 3749 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - | 3750 | `					 * so its old-value view survives the coercion. */` |
|   954985 | 3751 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 | 3752 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        5 | 3753 | `					}` |
|        - | 3754 | `					/* Force a numeric cast on the variable */` |
|   954985 | 3755 | `					PH7_MemObjToNumeric(pObj);` |
|   954985 | 3756 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        7 | 3757 | `						pObj->rVal++;` |
|        - | 3758 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3759 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3760 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3761 | `						 * integer-valued real. */` |
|        7 | 3762 | `						PH7_MemObjTryInteger(pObj);` |
|        4 | 3763 | `					}else{` |
|        - | 3764 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3765 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3766 | `						sxi64 r;` |
|   954979 | 3767 | `						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3768 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        7 | 3769 | `							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;` |
|        7 | 3770 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3771 | `#else` |
|        - | 3772 | `							pObj->x.iVal = r;` |
|        - | 3773 | `#endif` |
|        4 | 3774 | `						}else{` |
|   954973 | 3775 | `							pObj->x.iVal = r;` |
|        - | 3776 | `						}` |
|        - | 3777 | `					}` |
|   954985 | 3778 | `					if( pInstr->iP1 ){` |
|        - | 3779 | `						/* Pre-increment: result is the new value. */` |
|     2186 | 3780 | `						PH7_MemObjStore(pObj,pTos);` |
|     1091 | 3781 | `					}` |
|        - | 3782 | `					/* Post-increment: pTos retains the old value (a string` |
|        - | 3783 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - | 3784 | `					/* A NATIVE class's property is php's own C struct field, and` |
|        - | 3785 | ``					 * `++` writes it BACK through the write handler there — so the`` |
|        - | 3786 | `					 * conversion runs on the mutated slot. php's own answer, and` |
|        - | 3787 | `					 * the EXPRESSION's value is the unconverted sum either way:` |
|        - | 3788 | ``					 * `$i->f = 1.456008; var_dump(++$i->f, $i->f)` prints`` |
|        - | 3789 | `					 * 2.4560079999999997 then 2.456007. */` |
|   954985 | 3790 | `					PH7_NATIVE_SET_AFTER_MUTATE(pTos->nIdx,pObj)` |
|        - | 3791 | `				}` |
|   478529 | 3792 | `			}` |
|   478534 | 3793 | `		}else{` |
|       13 | 3794 | `			if( pInstr->iP1 ){` |
|      ! 0 | 3795 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 | 3796 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 | 3797 | `				}else{` |
|        - | 3798 | `					/* Force a numeric cast */` |
|      ! 0 | 3799 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3800 | `					/* Pre-increment */` |
|      ! 0 | 3801 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3802 | `						pTos->rVal++;` |
|        - | 3803 | `						/* Try to get an integer representation */` |
|      ! 0 | 3804 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3805 | `					}else{` |
|        - | 3806 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3807 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3808 | `						sxi64 r;` |
|      ! 0 | 3809 | `						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3810 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3811 | `							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;` |
|      ! 0 | 3812 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3813 | `#else` |
|        - | 3814 | `							pTos->x.iVal = r;` |
|        - | 3815 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3816 | `#endif` |
|      ! 0 | 3817 | `						}else{` |
|      ! 0 | 3818 | `							pTos->x.iVal = r;` |
|      ! 0 | 3819 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3820 | `						}` |
|        - | 3821 | `					}` |
|        - | 3822 | `				}` |
|      ! 0 | 3823 | `			}` |
|        - | 3824 | `		}` |
|   478535 | 3825 | `	}` |
|   955003 | 3826 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|   955003 | 3827 | `	break;` |
|        - | 3828 | `/*` |
|        - | 3829 | ` * DECR: P1 * *` |
|        - | 3830 | ` *` |
|        - | 3831 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - | 3832 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - | 3833 | ` * and decrement after that.` |
|        - | 3834 | ` */` |
|       88 | 3835 | `case PH7_OP_DECR:` |
|        - | 3836 | `#ifdef UNTRUST` |
|        - | 3837 | `	if( pTos < pStack ){` |
|        - | 3838 | `		goto Abort;` |
|        - | 3839 | `	}` |
|        - | 3840 | `#endif` |
|        - | 3841 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3842 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3843 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|        - | 3844 | `	 * property currently holding null). */` |
|      183 | 3845 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3846 | `	/* php refuses to decrement a string OFFSET, whatever byte it holds. */` |
|      170 | 3847 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3848 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3849 | `	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —` |
|        - | 3850 | ``	 * `--` on null is php's no-op and its write-back still dispatches set). */`` |
|      162 | 3851 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|       91 | 3852 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 3853 | `		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 3854 | `		if( VM_HOOK_PEND_IS_RMW(pTopDec->iKind) && pTopDec->nScratchIdx == pTos->nIdx ){` |
|        - | 3855 | `			SyBlob sErrMsg;` |
|        3 | 3856 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3857 | `			VmIncDecTypeErrorMsg(pTos,FALSE,&sErrMsg);` |
|        3 | 3858 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3859 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 3860 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 3861 | `			break;` |
|        - | 3862 | `		}` |
|      ! 0 | 3863 | `	}` |
|      164 | 3864 | `	PH7_INCDEC_NATIVE_ARITH("-")` |
|      162 | 3865 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3866 | ``		/* php's `--` operand contract, the mirror of INCR's: an array, object or`` |
|        - | 3867 | `		 * resource is a catchable TypeError, not a silent no-op. */` |
|        - | 3868 | `		SyBlob sDecMsg;` |
|        - | 3869 | `		sxi32 rcDec;` |
|       11 | 3870 | `		SyBlobInit(&sDecMsg,&pVm->sAllocator);` |
|       11 | 3871 | `		VmIncDecTypeErrorMsg(pTos,FALSE,&sDecMsg);` |
|       11 | 3872 | `		PH7_MemObjRelease(pTos);` |
|       11 | 3873 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 3874 | `		pTos->nIdx = SXU32_HIGH;` |
|       16 | 3875 | `		rcDec = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sDecMsg),` |
|        5 | 3876 | `			SyBlobLength(&sDecMsg));` |
|       11 | 3877 | `		SyBlobRelease(&sDecMsg);` |
|       11 | 3878 | `		if( rcDec == SXERR_ABORT ){ goto Abort; }` |
|       11 | 3879 | `		rc = rcDec;` |
|       13 | 3880 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3881 | `	}` |
|        - | 3882 | ``	/* NULL and BOOL stay excluded: PHP leaves `--` on either untouched (no-op) --`` |
|        - | 3883 | `	 * but 8.3 warns about both no-ops, same as the non-numeric-string one below. */` |
|      152 | 3884 | `	if( pTos->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|        - | 3885 | `		/* E_WARNING, not E_DEPRECATED -- php reports these at errno 2. */` |
|       19 | 3886 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3887 | `			"Decrement on type %s has no effect, this will change in the next major version of PHP",` |
|       12 | 3888 | `			(pTos->iFlags & MEMOBJ_NULL) ? "null" : "bool");` |
|        6 | 3889 | `	}` |
|      152 | 3890 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      140 | 3891 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3892 | `			ph7_value *pObj;` |
|      138 | 3893 | `			if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      138 | 3894 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3895 | ``					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string`` |
|        - | 3896 | `					 * (php has no string decrement); PHL rejects it. */` |
|        - | 3897 | `					SyBlob sErrMsg;` |
|        3 | 3898 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3899 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3900 | `						"Decrement on a non-numeric string is not supported",` |
|        - | 3901 | `						sizeof("Decrement on a non-numeric string is not supported")-1);` |
|        3 | 3902 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3903 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3904 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3905 | `					break;` |
|      ! 0 | 3906 | `				}else{` |
|        - | 3907 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - | 3908 | `					 * post-decrement must preserve pTos's original value, which` |
|        - | 3909 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - | 3910 | `					 * Force pTos to own its blob before coercing pObj. */` |
|      135 | 3911 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 | 3912 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 | 3913 | `					}` |
|      135 | 3914 | `					PH7_MemObjToNumeric(pObj);` |
|      135 | 3915 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|       11 | 3916 | `						pObj->rVal--;` |
|        - | 3917 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3918 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3919 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3920 | `						 * integer-valued real. */` |
|       11 | 3921 | `						PH7_MemObjTryInteger(pObj);` |
|        6 | 3922 | `					}else{` |
|        - | 3923 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3924 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3925 | `						sxi64 r;` |
|      125 | 3926 | `						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3927 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        3 | 3928 | `							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;` |
|        3 | 3929 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3930 | `#else` |
|        - | 3931 | `							pObj->x.iVal = r;` |
|        - | 3932 | `#endif` |
|        2 | 3933 | `						}else{` |
|      123 | 3934 | `							pObj->x.iVal = r;` |
|        - | 3935 | `						}` |
|        - | 3936 | `					}` |
|      135 | 3937 | `					if( pInstr->iP1 ){` |
|        - | 3938 | `						/* Pre-decrement: result is the new value. */` |
|        3 | 3939 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 | 3940 | `					}` |
|        - | 3941 | `					/* Post-decrement: pTos retains the old value. */` |
|      135 | 3942 | `					PH7_NATIVE_SET_AFTER_MUTATE(pTos->nIdx,pObj)   /* see OP_INCR */` |
|        - | 3943 | `				}` |
|       65 | 3944 | `			}` |
|       68 | 3945 | `		}else{` |
|        3 | 3946 | `			if( pInstr->iP1 ){` |
|        3 | 3947 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - | 3948 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 | 3949 | `				}else{` |
|        - | 3950 | `					/* Force a numeric cast */` |
|        3 | 3951 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3952 | `					/* Pre-decrement */` |
|        3 | 3953 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3954 | `						pTos->rVal--;` |
|        - | 3955 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 | 3956 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3957 | `					}else{` |
|        - | 3958 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3959 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3960 | `						sxi64 r;` |
|        3 | 3961 | `						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3962 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3963 | `							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;` |
|      ! 0 | 3964 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3965 | `#else` |
|        - | 3966 | `							pTos->x.iVal = r;` |
|        - | 3967 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3968 | `#endif` |
|      ! 0 | 3969 | `						}else{` |
|        3 | 3970 | `							pTos->x.iVal = r;` |
|        3 | 3971 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3972 | `						}` |
|        - | 3973 | `					}` |
|        - | 3974 | `				}` |
|        1 | 3975 | `			}` |
|        - | 3976 | `		}` |
|       66 | 3977 | `	}` |
|      149 | 3978 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|      149 | 3979 | `	break;` |
|        - | 3980 | `/*` |
|        - | 3981 | ` * UMINUS: * * *` |
|        - | 3982 | ` *` |
|        - | 3983 | ` * Perform a unary minus operation.` |
|        - | 3984 | ` */` |
|    53749 | 3985 | `case PH7_OP_UMINUS:` |
|        - | 3986 | `#ifdef UNTRUST` |
|        - | 3987 | `	if( pTos < pStack ){` |
|        - | 3988 | `		goto Abort;` |
|        - | 3989 | `	}` |
|        - | 3990 | `#endif` |
|        - | 3991 | ``	/* php's `$x * -1` operand contract: an array, object, resource or`` |
|        - | 3992 | `	 * non-numeric string is a TypeError, not a warned int(-1). */` |
|   107491 | 3993 | `	PH7_UNARY_ARITH_CONTRACT(-1)` |
|        - | 3994 | `	/* Force a numeric (integer,real or both) cast */` |
|   107463 | 3995 | `	PH7_MemObjToNumeric(pTos);` |
|   107463 | 3996 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      333 | 3997 | `		pTos->rVal = -pTos->rVal;` |
|      164 | 3998 | `	}` |
|   107463 | 3999 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|   107201 | 4000 | `		if( pTos->x.iVal == SMALLEST_INT64 ){` |
|        - | 4001 | `			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a` |
|        - | 4002 | `			 * REAL representation is already present it is the negated` |
|        - | 4003 | `			 * authoritative value, so just drop the now-stale cached int. The` |
|        - | 4004 | `			 * integer-only build has no float type, so it wraps (two's` |
|        - | 4005 | `			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */` |
|        - | 4006 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        7 | 4007 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 | 4008 | `				pTos->rVal = -(ph7_real)pTos->x.iVal;` |
|        7 | 4009 | `				MemObjSetType(pTos,MEMOBJ_REAL);` |
|        4 | 4010 | `			}else{` |
|      ! 0 | 4011 | `				pTos->iFlags &= ~MEMOBJ_INT;` |
|        - | 4012 | `			}` |
|        - | 4013 | `#else` |
|        - | 4014 | `			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);` |
|        - | 4015 | `#endif` |
|        4 | 4016 | `		}else{` |
|   107195 | 4017 | `			pTos->x.iVal = -pTos->x.iVal;` |
|        - | 4018 | `		}` |
|    53592 | 4019 | `	}` |
|   107463 | 4020 | `	break;` |
|        - | 4021 | `/*` |
|        - | 4022 | ` * UPLUS: * * *` |
|        - | 4023 | ` *` |
|        - | 4024 | ` * Perform a unary plus operation.` |
|        - | 4025 | ` */` |
|       24 | 4026 | `case PH7_OP_UPLUS:` |
|        - | 4027 | `#ifdef UNTRUST` |
|        - | 4028 | `	if( pTos < pStack ){` |
|        - | 4029 | `		goto Abort;` |
|        - | 4030 | `	}` |
|        - | 4031 | `#endif` |
|        - | 4032 | ``	/* Unary plus is php's `$x * 1`, so it answers the same TypeError as unary`` |
|        - | 4033 | ``	 * minus -- naming `int` as the other operand, exactly as php does. */`` |
|       49 | 4034 | `	PH7_UNARY_ARITH_CONTRACT(1)` |
|        - | 4035 | `	/* Force a numeric (integer,real or both) cast */` |
|       41 | 4036 | `	PH7_MemObjToNumeric(pTos);` |
|       41 | 4037 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 4038 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 | 4039 | `	}` |
|       41 | 4040 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       41 | 4041 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       20 | 4042 | `	}` |
|       41 | 4043 | `	break;` |
|        - | 4044 | `/*` |
|        - | 4045 | ` * OP_LNOT: * * *` |
|        - | 4046 | ` *` |
|        - | 4047 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - | 4048 | ` * with its complement.` |
|        - | 4049 | ` */` |
|    79209 | 4050 | `case PH7_OP_LNOT:` |
|        - | 4051 | `#ifdef UNTRUST` |
|        - | 4052 | `	if( pTos < pStack ){` |
|        - | 4053 | `		goto Abort;` |
|        - | 4054 | `	}` |
|        - | 4055 | `#endif` |
|        - | 4056 | `	/* Force a boolean cast */` |
|   158103 | 4057 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      824 | 4058 | `		PH7_MemObjToBool(pTos);` |
|      405 | 4059 | `	}` |
|   158103 | 4060 | `	pTos->x.iVal = !pTos->x.iVal;` |
|   158103 | 4061 | `	break;` |
|        - | 4062 | `/*` |
|        - | 4063 | ` * OP_BITNOT: * * *` |
|        - | 4064 | ` *` |
|        - | 4065 | ` * Interpret the top of the stack as an value.Replace it` |
|        - | 4066 | ` * with its ones-complement.` |
|        - | 4067 | ` */` |
|      387 | 4068 | `case PH7_OP_BITNOT:` |
|        - | 4069 | `#ifdef UNTRUST` |
|        - | 4070 | `	if( pTos < pStack ){` |
|        - | 4071 | `		goto Abort;` |
|        - | 4072 | `	}` |
|        - | 4073 | `#endif` |
|      778 | 4074 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - | 4075 | ``		/* php's `~` over a STRING is a per-BYTE ones-complement of the same`` |
|        - | 4076 | ``		 * length — a binary string, not an integer operation, so `~"abc"` is`` |
|        - | 4077 | `		 * "\x9e\x9d\x9c" where PHL cast the string to an int and answered` |
|        - | 4078 | `		 * int(-1). The operand's blob can be a READ-ONLY VIEW of the variable's` |
|        - | 4079 | `		 * own buffer (PH7_MemObjLoad), so complement into a fresh blob and swap` |
|        - | 4080 | `		 * it in rather than writing through the view. */` |
|        - | 4081 | `		SyBlob sNotBuf;` |
|       17 | 4082 | `		const unsigned char *zNotIn = (const unsigned char *)SyBlobData(&pTos->sBlob);` |
|       17 | 4083 | `		sxu32 nNotIn = SyBlobLength(&pTos->sBlob), iNot;` |
|       17 | 4084 | `		SyBlobInit(&sNotBuf,&pVm->sAllocator);` |
|       53 | 4085 | `		for( iNot = 0 ; iNot < nNotIn ; ++iNot ){` |
|       37 | 4086 | `			unsigned char cNot = (unsigned char)~zNotIn[iNot];` |
|       37 | 4087 | `			SyBlobAppend(&sNotBuf,(const void *)&cNot,sizeof(char));` |
|       19 | 4088 | `		}` |
|       17 | 4089 | `		PH7_MemObjRelease(pTos);` |
|       17 | 4090 | `		MemObjSetType(pTos,MEMOBJ_STRING);` |
|       17 | 4091 | `		if( SyBlobLength(&sNotBuf) > 0 ){` |
|       15 | 4092 | `			SyBlobAppend(&pTos->sBlob,SyBlobData(&sNotBuf),SyBlobLength(&sNotBuf));` |
|        7 | 4093 | `		}` |
|       17 | 4094 | `		SyBlobRelease(&sNotBuf);` |
|       17 | 4095 | `		break;` |
|        - | 4096 | `	}` |
|      762 | 4097 | `	if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        - | 4098 | `		/* Everything else — null, a bool, an array, an object, a resource — has` |
|        - | 4099 | `		 * no bitwise-not in php at all: a catchable TypeError naming the operand,` |
|        - | 4100 | `		 * where PHL cast it to an int and answered ~0 / ~1. */` |
|        - | 4101 | `		SyBlob sNotMsg;` |
|        - | 4102 | `		sxi32 rcNot;` |
|       27 | 4103 | `		SyBlobInit(&sNotMsg,&pVm->sAllocator);` |
|       27 | 4104 | `		VmBitNotTypeErrorMsg(pTos,&sNotMsg);` |
|       27 | 4105 | `		PH7_MemObjRelease(pTos);` |
|       27 | 4106 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       27 | 4107 | `		pTos->nIdx = SXU32_HIGH;` |
|       40 | 4108 | `		rcNot = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sNotMsg),` |
|       13 | 4109 | `			SyBlobLength(&sNotMsg));` |
|       27 | 4110 | `		SyBlobRelease(&sNotMsg);` |
|       27 | 4111 | `		if( rcNot == SXERR_ABORT ){ goto Abort; }` |
|       27 | 4112 | `		rc = rcNot;` |
|       27 | 4113 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4114 | `	}` |
|        - | 4115 | `	/* Force an integer cast (php deprecates a lossy float here too) */` |
|      736 | 4116 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      736 | 4117 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      736 | 4118 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 4119 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 4120 | `	}` |
|      736 | 4121 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        - | 4122 | `	/* An INTEGRAL float carries a cached MEMOBJ_INT beside MEMOBJ_REAL, so the` |
|        - | 4123 | `` 	 * cast above is skipped and the value kept rendering as a float: `~2.0` `` |
|        - | 4124 | ``	 * answered 2.0 instead of -3. `~` always yields an int. */`` |
|      736 | 4125 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      736 | 4126 | `	break;` |
|        - | 4127 | `/* OP_MUL * * *` |
|        - | 4128 | ` * OP_MUL_STORE * * *` |
|        - | 4129 | ` *` |
|        - | 4130 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - | 4131 | ` * and push the result back onto the stack.` |
|        - | 4132 | ` */` |
|     3803 | 4133 | `case PH7_OP_MUL:` |
|        - | 4134 | `case PH7_OP_MUL_STORE: {` |
|        - | 4135 | `	VmOpRc rcOp;` |
|        - | 4136 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     7597 | 4137 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     7595 | 4138 | `	sState.pTos = pTos;` |
|     7595 | 4139 | `	sState.pc = pc;` |
|     7595 | 4140 | `	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);` |
|     7595 | 4141 | `	pTos = sState.pTos;` |
|     7595 | 4142 | `	pc = sState.pc;` |
|     7595 | 4143 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4144 | `		goto Abort;` |
|     7595 | 4145 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      312 | 4146 | `		goto Exception;` |
|        - | 4147 | `	}` |
|     7285 | 4148 | `	break;` |
|        - | 4149 | `					  }` |
|        - | 4150 | `/* OP_POW * * *` |
|        - | 4151 | ` * OP_POW_STORE * * *` |
|        - | 4152 | ` *` |
|        - | 4153 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - | 4154 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - | 4155 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - | 4156 | ` * fits in sxi64; otherwise the result is a double.` |
|        - | 4157 | ` */` |
|      535 | 4158 | `case PH7_OP_POW:` |
|        - | 4159 | `case PH7_OP_POW_STORE: {` |
|        - | 4160 | `	VmOpRc rcOp;` |
|        - | 4161 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     1072 | 4162 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     1070 | 4163 | `	sState.pTos = pTos;` |
|     1070 | 4164 | `	sState.pc = pc;` |
|     1070 | 4165 | `	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);` |
|     1070 | 4166 | `	pTos = sState.pTos;` |
|     1070 | 4167 | `	pc = sState.pc;` |
|     1070 | 4168 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4169 | `		goto Abort;` |
|     1070 | 4170 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      318 | 4171 | `		goto Exception;` |
|        - | 4172 | `	}` |
|      753 | 4173 | `	break;` |
|        - | 4174 | `					  }` |
|        - | 4175 | `/* OP_ADD * * *` |
|        - | 4176 | ` *` |
|        - | 4177 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 4178 | ` * and push the result back onto the stack.` |
|        - | 4179 | ` */` |
|    20438 | 4180 | `case PH7_OP_ADD:{` |
|    40766 | 4181 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4182 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|        - | 4183 | `	int rcNa;` |
|    40766 | 4184 | `	const char *zArCls = "TypeError";` |
|        - | 4185 | `#ifdef UNTRUST` |
|        - | 4186 | `	if( pNos < pStack ){` |
|        - | 4187 | `		goto Abort;` |
|        - | 4188 | `	}` |
|        - | 4189 | `#endif` |
|        - | 4190 | `	{` |
|        - | 4191 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|        - | 4192 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|        - | 4193 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|        - | 4194 | `		SyBlob sArMsg;` |
|    40766 | 4195 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    40766 | 4196 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"+",pNos,&zArCls,&sArMsg);` |
|    40766 | 4197 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|        - | 4198 | `			sxi32 rcAr;` |
|      182 | 4199 | `			VmPopOperand(&pTos,1);` |
|      182 | 4200 | `			PH7_MemObjRelease(pTos);` |
|      182 | 4201 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      182 | 4202 | `			pTos->nIdx = SXU32_HIGH;` |
|      272 | 4203 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|       90 | 4204 | `				SyBlobLength(&sArMsg));` |
|      182 | 4205 | `			SyBlobRelease(&sArMsg);` |
|      182 | 4206 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      182 | 4207 | `			rc = rcAr;` |
|      182 | 4208 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4209 | `		}` |
|    40586 | 4210 | `		SyBlobRelease(&sArMsg);` |
|        - | 4211 | `	}` |
|        - | 4212 | `	/* Perform the addition (unless a do_operation handler already answered) */` |
|    40586 | 4213 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|    40572 | 4214 | `		PH7_MemObjAdd(pNos,pTos,FALSE);` |
|    20226 | 4215 | `	}` |
|    40586 | 4216 | `	VmPopOperand(&pTos,1);` |
|    40586 | 4217 | `	break;` |
|        - | 4218 | `				}` |
|        - | 4219 | `/*` |
|        - | 4220 | ` * OP_ADD_STORE * * *` |
|        - | 4221 | ` *` |
|        - | 4222 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 4223 | ` * and push the result back onto the stack.` |
|        - | 4224 | ` */` |
|   237241 | 4225 | `case PH7_OP_ADD_STORE:{` |
|   474753 | 4226 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4227 | `	ph7_value *pObj;` |
|        - | 4228 | `	sxu32 nIdx;` |
|        - | 4229 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|        - | 4230 | `	int rcNa;` |
|   474753 | 4231 | `	const char *zArCls = "TypeError";` |
|        - | 4232 | `#ifdef UNTRUST` |
|        - | 4233 | `	if( pNos < pStack ){` |
|        - | 4234 | `		goto Abort;` |
|        - | 4235 | `	}` |
|        - | 4236 | `#endif` |
|        - | 4237 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|   711915 | 4238 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 4239 | `	{` |
|        - | 4240 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 4241 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 4242 | `		SyBlob sArMsg;` |
|   474747 | 4243 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   474747 | 4244 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"+",pTos,&zArCls,&sArMsg);` |
|   474747 | 4245 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|        - | 4246 | `			sxi32 rcAr;` |
|      151 | 4247 | `			VmPopOperand(&pTos,1);` |
|      151 | 4248 | `			PH7_MemObjRelease(pTos);` |
|      151 | 4249 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      151 | 4250 | `			pTos->nIdx = SXU32_HIGH;` |
|      226 | 4251 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|       75 | 4252 | `				SyBlobLength(&sArMsg));` |
|      151 | 4253 | `			SyBlobRelease(&sArMsg);` |
|      151 | 4254 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      151 | 4255 | `			rc = rcAr;` |
|      151 | 4256 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4257 | `		}` |
|   474597 | 4258 | `		SyBlobRelease(&sArMsg);` |
|        - | 4259 | `	}` |
|        - | 4260 | `	/* Perform the addition */` |
|   474597 | 4261 | `	nIdx = pTos->nIdx;` |
|   474597 | 4262 | `	if( nIdx == pVm->nGlobalIdx ){` |
|        - | 4263 | `		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment` |
|        - | 4264 | `		 * (a compile-time fatal in php; raised here, same message). */` |
|        3 | 4265 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 4266 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 4267 | `		pVm->iExitStatus = 255;` |
|        3 | 4268 | `		pVm->bHaltRequested = 1;` |
|        3 | 4269 | `		goto Abort;` |
|        - | 4270 | `	}` |
|   474595 | 4271 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|   474593 | 4272 | `		PH7_MemObjAdd(pTos,pNos,TRUE);` |
|   237427 | 4273 | `	}` |
|        - | 4274 | `	/* Peform the store operation */` |
|   474595 | 4275 | `	if( nIdx == SXU32_HIGH ){` |
|        - | 4276 | ``		/* A read-modify-write THROUGH a temporary (`f()[0] += 5`): php computes it,`` |
|        - | 4277 | `		 * drops it and stays silent. See the OP_STORE member note above. */` |
|   474593 | 4278 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|   474591 | 4279 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|   474587 | 4280 | `		PH7_MemObjStore(pTos,pObj);` |
|   237424 | 4281 | `	}` |
|   474591 | 4282 | `	PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|        - | 4283 | `	/* Ticket 1433-35: Perform a stack dup */` |
|   474591 | 4284 | `	PH7_MemObjStore(pTos,pNos);` |
|   474591 | 4285 | `	VmPopOperand(&pTos,1);` |
|   474591 | 4286 | `	break;` |
|        - | 4287 | `				}` |
|        - | 4288 | `/* OP_SUB * * *` |
|        - | 4289 | ` *` |
|        - | 4290 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 4291 | ` * first (what was next on the stack) from the second (the` |
|        - | 4292 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 4293 | ` */` |
|    36222 | 4294 | `case PH7_OP_SUB: {` |
|        - | 4295 | `	VmOpRc rcOp;` |
|    73685 | 4296 | `	sState.pTos = pTos;` |
|    73685 | 4297 | `	sState.pc = pc;` |
|    73685 | 4298 | `	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);` |
|    73685 | 4299 | `	pTos = sState.pTos;` |
|    73685 | 4300 | `	pc = sState.pc;` |
|    73685 | 4301 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4302 | `		goto Abort;` |
|    73685 | 4303 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      162 | 4304 | `		goto Exception;` |
|        - | 4305 | `	}` |
|    73525 | 4306 | `	break;` |
|        - | 4307 | `					  }` |
|        - | 4308 | `/* OP_SUB_STORE * * *` |
|        - | 4309 | ` *` |
|        - | 4310 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 4311 | ` * first (what was next on the stack) from the second (the` |
|        - | 4312 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 4313 | ` */` |
|      205 | 4314 | `case PH7_OP_SUB_STORE: {` |
|        - | 4315 | `	VmOpRc rcOp;` |
|        - | 4316 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      412 | 4317 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      410 | 4318 | `	sState.pTos = pTos;` |
|      410 | 4319 | `	sState.pc = pc;` |
|      410 | 4320 | `	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);` |
|      410 | 4321 | `	pTos = sState.pTos;` |
|      410 | 4322 | `	pc = sState.pc;` |
|      410 | 4323 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4324 | `		goto Abort;` |
|      410 | 4325 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      153 | 4326 | `		goto Exception;` |
|        - | 4327 | `	}` |
|      258 | 4328 | `	break;` |
|        - | 4329 | `					  }` |
|        - | 4330 |  |
|        - | 4331 | `/*` |
|        - | 4332 | ` * OP_MOD * * *` |
|        - | 4333 | ` *` |
|        - | 4334 | ` * Pop the top two elements from the stack, divide the` |
|        - | 4335 | ` * first (what was next on the stack) from the second (the` |
|        - | 4336 | ` * top of the stack) and push the remainder after division` |
|        - | 4337 | ` * onto the stack.` |
|        - | 4338 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 4339 | ` */` |
|     1344 | 4340 | `case PH7_OP_MOD: {` |
|        - | 4341 | `	VmOpRc rcOp;` |
|     2693 | 4342 | `	sState.pTos = pTos;` |
|     2693 | 4343 | `	sState.pc = pc;` |
|     2693 | 4344 | `	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);` |
|     2693 | 4345 | `	pTos = sState.pTos;` |
|     2693 | 4346 | `	pc = sState.pc;` |
|     2693 | 4347 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4348 | `		goto Abort;` |
|     2693 | 4349 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      248 | 4350 | `		goto Exception;` |
|        - | 4351 | `	}` |
|     2447 | 4352 | `	break;` |
|        - | 4353 | `					  }` |
|        - | 4354 | `/*` |
|        - | 4355 | ` * OP_MOD_STORE * * *` |
|        - | 4356 | ` *` |
|        - | 4357 | ` * Pop the top two elements from the stack, divide the` |
|        - | 4358 | ` * first (what was next on the stack) from the second (the` |
|        - | 4359 | ` * top of the stack) and push the remainder after division` |
|        - | 4360 | ` * onto the stack.` |
|        - | 4361 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 4362 | ` */` |
|      204 | 4363 | `case PH7_OP_MOD_STORE: {` |
|        - | 4364 | `	VmOpRc rcOp;` |
|        - | 4365 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      409 | 4366 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      407 | 4367 | `	sState.pTos = pTos;` |
|      407 | 4368 | `	sState.pc = pc;` |
|      407 | 4369 | `	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);` |
|      407 | 4370 | `	pTos = sState.pTos;` |
|      407 | 4371 | `	pc = sState.pc;` |
|      407 | 4372 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4373 | `		goto Abort;` |
|      407 | 4374 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      221 | 4375 | `		goto Exception;` |
|        - | 4376 | `	}` |
|      187 | 4377 | `	break;` |
|        - | 4378 | `					  }` |
|        - | 4379 | `/*` |
|        - | 4380 | ` * OP_DIV * * *` |
|        - | 4381 | ` *` |
|        - | 4382 | ` * Pop the top two elements from the stack, divide the` |
|        - | 4383 | ` * first (what was next on the stack) from the second (the` |
|        - | 4384 | ` * top of the stack) and push the result onto the stack.` |
|        - | 4385 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 4386 | ` */` |
|      480 | 4387 | `case PH7_OP_DIV: {` |
|        - | 4388 | `	VmOpRc rcOp;` |
|      963 | 4389 | `	sState.pTos = pTos;` |
|      963 | 4390 | `	sState.pc = pc;` |
|      963 | 4391 | `	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);` |
|      963 | 4392 | `	pTos = sState.pTos;` |
|      963 | 4393 | `	pc = sState.pc;` |
|      963 | 4394 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4395 | `		goto Abort;` |
|      963 | 4396 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      233 | 4397 | `		goto Exception;` |
|        - | 4398 | `	}` |
|      731 | 4399 | `	break;` |
|        - | 4400 | `					  }` |
|        - | 4401 | `/*` |
|        - | 4402 | ` * OP_DIV_STORE * * *` |
|        - | 4403 | ` *` |
|        - | 4404 | ` * Pop the top two elements from the stack, divide the` |
|        - | 4405 | ` * first (what was next on the stack) from the second (the` |
|        - | 4406 | ` * top of the stack) and push the result onto the stack.` |
|        - | 4407 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 4408 | ` */` |
|      211 | 4409 | `case PH7_OP_DIV_STORE:{` |
|      423 | 4410 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4411 | `	ph7_value *pObj;` |
|        - | 4412 | `	ph7_real a,b,r;` |
|        - | 4413 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|        - | 4414 | `	int rcNa;` |
|      423 | 4415 | `	const char *zArCls = "TypeError";` |
|        - | 4416 | `#ifdef UNTRUST` |
|        - | 4417 | `	if( pNos < pStack ){` |
|        - | 4418 | `		goto Abort;` |
|        - | 4419 | `	}` |
|        - | 4420 | `#endif` |
|        - | 4421 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      524 | 4422 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 4423 | `	{` |
|        - | 4424 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 4425 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 4426 | `		SyBlob sArMsg;` |
|      421 | 4427 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|      421 | 4428 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"/",pNos,&zArCls,&sArMsg);` |
|      421 | 4429 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|        - | 4430 | `			sxi32 rcAr;` |
|      151 | 4431 | `			VmPopOperand(&pTos,1);` |
|      151 | 4432 | `			PH7_MemObjRelease(pTos);` |
|      151 | 4433 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      151 | 4434 | `			pTos->nIdx = SXU32_HIGH;` |
|      226 | 4435 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|       75 | 4436 | `				SyBlobLength(&sArMsg));` |
|      151 | 4437 | `			SyBlobRelease(&sArMsg);` |
|      151 | 4438 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      151 | 4439 | `			rc = rcAr;` |
|      151 | 4440 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4441 | `		}` |
|      271 | 4442 | `		SyBlobRelease(&sArMsg);` |
|        - | 4443 | `	}` |
|      271 | 4444 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|        - | 4445 | ``		/* php's `/` answers an INT when both operands are ints and the division is`` |
|        - | 4446 | ``		 * exact (`6/3 === 2`, not `2.0`), and `$x /= $y` is that same operator: php`` |
|        - | 4447 | `		 * has one division and the compound form only decides where the answer goes.` |
|        - | 4448 | `		 * OP_DIV grew the rule (the int-boundary work) and this arm, a separate copy` |
|        - | 4449 | ``		 * of it, did not — so `$x = 6; $x /= 3;` left a FLOAT where `$x = $x / 3` left`` |
|        - | 4450 | ``		 * an int, visible through `===`, `var_dump`, `json_encode` and `is_int`.`` |
|        - | 4451 | ``		 * The divisor is screened BEFORE `ia % ib`: x86 computes the overflowing`` |
|        - | 4452 | `		 * PHP_INT_MIN/-1 quotient alongside the remainder and traps (OP_DIV and OP_MOD` |
|        - | 4453 | `		 * guard the same hazard the same way). */` |
|      269 | 4454 | `		int bExactDiv = 0;` |
|      269 | 4455 | `		PH7_MemObjToNumeric(pTos);` |
|      269 | 4456 | `		PH7_MemObjToNumeric(pNos);` |
|      269 | 4457 | `		if( ((pTos->iFlags\|pNos->iFlags) & MEMOBJ_REAL) == 0 ){` |
|      269 | 4458 | `			sxi64 ia = pTos->x.iVal;   /* the lvalue: php's dividend */` |
|      269 | 4459 | `			sxi64 ib = pNos->x.iVal;   /* the right operand: the divisor */` |
|      269 | 4460 | `			sxi64 iQuot = 0;` |
|      269 | 4461 | `			if( ib == 0 ){` |
|       71 | 4462 | `				rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|       73 | 4463 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      199 | 4464 | `			}else if( ib == -1 ){` |
|        - | 4465 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - | 4466 | `				iQuot = ( ia != SMALLEST_INT64 ) ? -ia : SMALLEST_INT64;` |
|        - | 4467 | `				bExactDiv = 1;` |
|        - | 4468 | `#else` |
|        7 | 4469 | `				if( ia != SMALLEST_INT64 ){` |
|        3 | 4470 | `					iQuot = -ia;` |
|        3 | 4471 | `					bExactDiv = 1;` |
|        2 | 4472 | `				}` |
|        - | 4473 | `#endif` |
|      196 | 4474 | `			}else if( ia % ib == 0 ){` |
|      109 | 4475 | `				iQuot = ia / ib;` |
|      109 | 4476 | `				bExactDiv = 1;` |
|       54 | 4477 | `			}` |
|      199 | 4478 | `			if( bExactDiv ){` |
|      111 | 4479 | `				pNos->x.iVal = iQuot;` |
|      111 | 4480 | `				MemObjSetType(pNos,MEMOBJ_INT);` |
|       55 | 4481 | `			}` |
|       99 | 4482 | `		}` |
|      199 | 4483 | `		if( !bExactDiv ){` |
|        - | 4484 | `			/* Force the operands to be real */` |
|       89 | 4485 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       89 | 4486 | `				PH7_MemObjToReal(pTos);` |
|       44 | 4487 | `			}` |
|       89 | 4488 | `			if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       89 | 4489 | `				PH7_MemObjToReal(pNos);` |
|       44 | 4490 | `			}` |
|        - | 4491 | `			/* Perform the requested operation */` |
|       89 | 4492 | `			a = pTos->rVal;` |
|       89 | 4493 | `			b = pNos->rVal;` |
|       89 | 4494 | `			if( b == 0 ){` |
|        - | 4495 | `				/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|        - | 4496 | `				 * not the old non-catchable warning that continued with a 0 result. */` |
|      ! 0 | 4497 | `				rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|      ! 0 | 4498 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      ! 0 | 4499 | `			}else{` |
|       89 | 4500 | `				r = a/b;` |
|        - | 4501 | `				/* Push the result */` |
|       89 | 4502 | `				pNos->rVal = r;` |
|       89 | 4503 | `				MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - | 4504 | `			}` |
|       44 | 4505 | `		}` |
|       99 | 4506 | `	}` |
|      201 | 4507 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|        - | 4508 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|      201 | 4509 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      201 | 4510 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|      201 | 4511 | `		PH7_MemObjStore(pNos,pObj);` |
|      100 | 4512 | `	}` |
|      201 | 4513 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|      201 | 4514 | `	VmPopOperand(&pTos,1);` |
|      201 | 4515 | `	break;` |
|        - | 4516 | `				}` |
|        - | 4517 | `/* OP_BAND * * *` |
|        - | 4518 | ` *` |
|        - | 4519 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4520 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 4521 | ` * two elements.` |
|        - | 4522 | `*/` |
|        - | 4523 | `/* OP_BOR * * *` |
|        - | 4524 | ` *` |
|        - | 4525 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4526 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 4527 | ` * two elements.` |
|        - | 4528 | ` */` |
|        - | 4529 | `/* OP_BXOR * * *` |
|        - | 4530 | ` *` |
|        - | 4531 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4532 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 4533 | ` * two elements.` |
|        - | 4534 | ` */` |
|     4594 | 4535 | `case PH7_OP_BAND:` |
|        - | 4536 | `case PH7_OP_BOR:` |
|        - | 4537 | `case PH7_OP_BXOR:{` |
|     9174 | 4538 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4539 | `	sxi64 a,b,r;` |
|        - | 4540 | `	int cBwOp;` |
|        - | 4541 | `#ifdef UNTRUST` |
|        - | 4542 | `	if( pNos < pStack ){` |
|        - | 4543 | `		goto Abort;` |
|        - | 4544 | `	}` |
|        - | 4545 | `#endif` |
|     9174 | 4546 | `	cBwOp = pInstr->iOp == PH7_OP_BOR ? '\|' : (pInstr->iOp == PH7_OP_BXOR ? '^' : '&');` |
|     9174 | 4547 | `	if( (pNos->iFlags & MEMOBJ_STRING) && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 4548 | `		/* TWO strings: php's per-byte string operation, result a string. */` |
|      149 | 4549 | `		PH7_STRING_BITWISE_RESULT(pNos,pNos,pTos,cBwOp)` |
|      149 | 4550 | `		VmPopOperand(&pTos,1);` |
|      149 | 4551 | `		break;` |
|        - | 4552 | `	}` |
|        - | 4553 | `	{` |
|        - | 4554 | `		char zBwOp[2];` |
|     9026 | 4555 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 4556 | `		/* Anything else is php's arithmetic operand contract, named for this` |
|        - | 4557 | ``		 * operator (`array & int`), not a silent integer cast. */`` |
|     9026 | 4558 | `		PH7_BITWISE_ARITH_CONTRACT(pNos,pTos,zBwOp,1)` |
|        - | 4559 | `	}` |
|        - | 4560 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     8602 | 4561 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     8602 | 4562 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     8596 | 4563 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     8596 | 4564 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     8596 | 4565 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      321 | 4566 | `		PH7_MemObjToInteger(pTos);` |
|      160 | 4567 | `	}` |
|     8596 | 4568 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      323 | 4569 | `		PH7_MemObjToInteger(pNos);` |
|      161 | 4570 | `	}` |
|        - | 4571 | `	/* Perform the requested operation */` |
|     8596 | 4572 | `	a = pNos->x.iVal;` |
|     8596 | 4573 | `	b = pTos->x.iVal;` |
|     8596 | 4574 | `	switch(pInstr->iOp){` |
|      628 | 4575 | `	case PH7_OP_BOR_STORE:` |
|     1261 | 4576 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|      216 | 4577 | `	case PH7_OP_BXOR_STORE:` |
|      433 | 4578 | `	case PH7_OP_BXOR: r = a^b; break;` |
|     3460 | 4579 | `	case PH7_OP_BAND_STORE:` |
|     3443 | 4580 | `	case PH7_OP_BAND:` |
|     6908 | 4581 | `	default:          r = a&b; break;` |
|        - | 4582 | `	}` |
|        - | 4583 | `	/* Push the result */` |
|     8596 | 4584 | `	pNos->x.iVal = r;` |
|     8596 | 4585 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     8596 | 4586 | `	VmPopOperand(&pTos,1);` |
|     8596 | 4587 | `	break;` |
|        - | 4588 | `				 }` |
|        - | 4589 | `/* OP_BAND_STORE * * *` |
|        - | 4590 | ` *` |
|        - | 4591 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4592 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 4593 | ` * two elements.` |
|        - | 4594 | `*/` |
|        - | 4595 | `/* OP_BOR_STORE * * *` |
|        - | 4596 | ` *` |
|        - | 4597 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4598 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 4599 | ` * two elements.` |
|        - | 4600 | ` */` |
|        - | 4601 | `/* OP_BXOR_STORE * * *` |
|        - | 4602 | ` *` |
|        - | 4603 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4604 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 4605 | ` * two elements.` |
|        - | 4606 | ` */` |
|      640 | 4607 | `case PH7_OP_BAND_STORE:` |
|        - | 4608 | `case PH7_OP_BOR_STORE:` |
|        - | 4609 | `case PH7_OP_BXOR_STORE:{` |
|     1281 | 4610 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4611 | `	ph7_value *pObj;` |
|        - | 4612 | `	sxi64 a,b,r;` |
|        - | 4613 | `	int cBwOp,bBwStr;` |
|        - | 4614 | `#ifdef UNTRUST` |
|        - | 4615 | `	if( pNos < pStack ){` |
|        - | 4616 | `		goto Abort;` |
|        - | 4617 | `	}` |
|        - | 4618 | `#endif` |
|        - | 4619 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     1281 | 4620 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     1275 | 4621 | `	cBwOp = pInstr->iOp == PH7_OP_BOR_STORE ? '\|' : (pInstr->iOp == PH7_OP_BXOR_STORE ? '^' : '&');` |
|     1275 | 4622 | `	bBwStr = (pNos->iFlags & MEMOBJ_STRING) != 0 && (pTos->iFlags & MEMOBJ_STRING) != 0;` |
|     1275 | 4623 | `	if( !bBwStr ){` |
|        - | 4624 | `		char zBwOp[2];` |
|     1173 | 4625 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 4626 | ``		/* `$x &= v` answers the same contract as `$x & v` (php's compound`` |
|        - | 4627 | `		 * assignment is the operator plus a store), but through its own error` |
|        - | 4628 | `		 * path, which is POSITIONAL: the lvalue is named first even when the` |
|        - | 4629 | ``		 * right operand is the offender (`$x = 1; $x &= $o` is "int & BsocP",`` |
|        - | 4630 | ``		 * where the plain `1 & $o` is "BsocP & int"). */`` |
|     1173 | 4631 | `		PH7_BITWISE_ARITH_CONTRACT(pTos,pNos,zBwOp,0)` |
|        - | 4632 | `		/* Force the operands to be integer (php deprecates a lossy float here) */` |
|      791 | 4633 | `		rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|      791 | 4634 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      791 | 4635 | `		rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      791 | 4636 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      791 | 4637 | `		if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      309 | 4638 | `			PH7_MemObjToInteger(pTos);` |
|      154 | 4639 | `		}` |
|      791 | 4640 | `		if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      309 | 4641 | `			PH7_MemObjToInteger(pNos);` |
|      154 | 4642 | `		}` |
|      395 | 4643 | `	}` |
|      893 | 4644 | `	if( bBwStr ){` |
|        - | 4645 | `		/* TWO strings: php's per-byte string operation, result a string. The` |
|        - | 4646 | `		 * result lands in pNos, which the store tail below writes into the` |
|        - | 4647 | `		 * lvalue's slot exactly like the integer result. */` |
|      103 | 4648 | `		PH7_STRING_BITWISE_RESULT(pNos,pTos,pNos,cBwOp)` |
|       52 | 4649 | `	}else{` |
|        - | 4650 | `	/* Perform the requested operation */` |
|      791 | 4651 | `	a = pTos->x.iVal;` |
|      791 | 4652 | `	b = pNos->x.iVal;` |
|      791 | 4653 | `	switch(pInstr->iOp){` |
|      151 | 4654 | `	case PH7_OP_BOR_STORE:` |
|      303 | 4655 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|      122 | 4656 | `	case PH7_OP_BXOR_STORE:` |
|      245 | 4657 | `	case PH7_OP_BXOR: r = a^b; break;` |
|      122 | 4658 | `	case PH7_OP_BAND_STORE:` |
|      122 | 4659 | `	case PH7_OP_BAND:` |
|      245 | 4660 | `	default:          r = a&b; break;` |
|        - | 4661 | `	}` |
|        - | 4662 | `	/* Push the result */` |
|      791 | 4663 | `	pNos->x.iVal = r;` |
|      791 | 4664 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        - | 4665 | `	}` |
|      893 | 4666 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|        - | 4667 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|      892 | 4668 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      891 | 4669 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|      891 | 4670 | `		PH7_MemObjStore(pNos,pObj);` |
|      445 | 4671 | `	}` |
|      893 | 4672 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|      893 | 4673 | `	VmPopOperand(&pTos,1);` |
|      893 | 4674 | `	break;` |
|        - | 4675 | `				 }` |
|        - | 4676 | `/* OP_SHL * * *` |
|        - | 4677 | ` *` |
|        - | 4678 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4679 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4680 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4681 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4682 | ` */` |
|        - | 4683 | `/* OP_SHR * * *` |
|        - | 4684 | ` *` |
|        - | 4685 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4686 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4687 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4688 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4689 | ` */` |
|     1680 | 4690 | `case PH7_OP_SHL:` |
|        - | 4691 | `case PH7_OP_SHR: {` |
|        - | 4692 | `	VmOpRc rcOp;` |
|     3362 | 4693 | `	sState.pTos = pTos;` |
|     3362 | 4694 | `	sState.pc = pc;` |
|     3362 | 4695 | `	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);` |
|     3362 | 4696 | `	pTos = sState.pTos;` |
|     3362 | 4697 | `	pc = sState.pc;` |
|     3362 | 4698 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4699 | `		goto Abort;` |
|     3362 | 4700 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      394 | 4701 | `		goto Exception;` |
|        - | 4702 | `	}` |
|     2970 | 4703 | `	break;` |
|        - | 4704 | `					  }` |
|        - | 4705 | `/*  OP_SHL_STORE * * *` |
|        - | 4706 | ` *` |
|        - | 4707 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4708 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4709 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4710 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4711 | ` */` |
|        - | 4712 | `/* OP_SHR_STORE * * *` |
|        - | 4713 | ` *` |
|        - | 4714 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4715 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4716 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4717 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4718 | ` */` |
|      414 | 4719 | `case PH7_OP_SHL_STORE:` |
|        - | 4720 | `case PH7_OP_SHR_STORE: {` |
|        - | 4721 | `	VmOpRc rcOp;` |
|        - | 4722 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      829 | 4723 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      825 | 4724 | `	sState.pTos = pTos;` |
|      825 | 4725 | `	sState.pc = pc;` |
|      825 | 4726 | `	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);` |
|      825 | 4727 | `	pTos = sState.pTos;` |
|      825 | 4728 | `	pc = sState.pc;` |
|      825 | 4729 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4730 | `		goto Abort;` |
|      825 | 4731 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      353 | 4732 | `		goto Exception;` |
|        - | 4733 | `	}` |
|      473 | 4734 | `	break;` |
|        - | 4735 | `					  }` |
|        - | 4736 | `/* CAT:  P1 * *` |
|        - | 4737 | ` *` |
|        - | 4738 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - | 4739 | ` * back.` |
|        - | 4740 | ` */` |
|   177869 | 4741 | `case PH7_OP_CAT:{` |
|        - | 4742 | `	ph7_value *pNos,*pCur;` |
|   355599 | 4743 | `	if( pInstr->iP1 < 1 ){` |
|   299920 | 4744 | `		pNos = &pTos[-1];` |
|   149965 | 4745 | `	}else{` |
|    55684 | 4746 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - | 4747 | `	}` |
|        - | 4748 | `#ifdef UNTRUST` |
|        - | 4749 | `	if( pNos < pStack ){` |
|        - | 4750 | `		goto Abort;` |
|        - | 4751 | `	}` |
|        - | 4752 | `#endif` |
|        - | 4753 | `	/* Force a string cast (user-visible: warns on an array operand).` |
|        - | 4754 | `	 * php coerces the operands left to right, so the leftmost not-stringable` |
|        - | 4755 | `	 * object is the one that throws. */` |
|        - | 4756 | `	{` |
|   355599 | 4757 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|   355599 | 4758 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4759 | `	}` |
|   355597 | 4760 | `	pCur = &pNos[1];` |
|        - | 4761 | `	{` |
|        - | 4762 | `		/* rcSv leaves the loop with the coercion status: the routing macro expands` |
|        - | 4763 | ``		 * to a bare `break` here, which inside the while would fall THROUGH to the`` |
|        - | 4764 | ``		 * `pTos = pNos` below with a throw pending. Route it at case level. */`` |
|   355597 | 4765 | `		sxi32 rcSv = SXRET_OK;` |
|   784885 | 4766 | `		while( pCur <= pTos ){` |
|   429737 | 4767 | `			rcSv = PH7_MemObjToStringUV(pCur);` |
|   429737 | 4768 | `			if( rcSv != SXRET_OK ){` |
|      447 | 4769 | `				break;` |
|        - | 4770 | `			}` |
|        - | 4771 | `			/* Perform the concatenation */` |
|   429293 | 4772 | `			if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   428113 | 4773 | `				if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - | 4774 | `					/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 | 4775 | `					PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4776 | `					goto Abort;` |
|        - | 4777 | `				}` |
|   213841 | 4778 | `			}` |
|   429293 | 4779 | `			SyBlobRelease(&pCur->sBlob);` |
|   429293 | 4780 | `			pCur++;` |
|        5 | 4781 | `		}` |
|   356413 | 4782 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4783 | `	}` |
|   355153 | 4784 | `	pTos = pNos;` |
|   355153 | 4785 | `	break;` |
|        - | 4786 | `				}` |
|        - | 4787 | `/*  CAT_STORE: * * *` |
|        - | 4788 | ` *` |
|        - | 4789 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - | 4790 | ` * back.` |
|        - | 4791 | ` */` |
|    42566 | 4792 | `case PH7_OP_CAT_STORE:{` |
|    90812 | 4793 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4794 | `	ph7_value *pObj;` |
|        - | 4795 | `	sxu32 nIdx;` |
|        - | 4796 | `#ifdef UNTRUST` |
|        - | 4797 | `	if( pNos < pStack ){` |
|        - | 4798 | `		goto Abort;` |
|        - | 4799 | `	}` |
|        - | 4800 | `#endif` |
|        - | 4801 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|    90812 | 4802 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 4803 | `	/* The right operand must be a string to append it (user-visible) */` |
|        - | 4804 | `	{` |
|    90804 | 4805 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|    90808 | 4806 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4807 | `	}` |
|    90794 | 4808 | `	nIdx = pTos->nIdx;` |
|        - | 4809 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - | 4810 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - | 4811 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - | 4812 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - | 4813 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - | 4814 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - | 4815 | `	 * the source we copy from — references share the slot index, so one check` |
|        - | 4816 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - | 4817 | `	 * must run before any mutation (left to the slow path).` |
|        - | 4818 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - | 4819 | `	 * and remains O(n^2) by design. */` |
|    90789 | 4820 | `	if( nIdx != SXU32_HIGH` |
|    90787 | 4821 | `	 && nIdx != pNos->nIdx` |
|    84269 | 4822 | `	 && (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx)) != 0` |
|    84270 | 4823 | `	 && !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|        - | 4824 | `		/* e.g. $x = 5; $x .= "a";  ->  "5a" (user-visible: warns if $x is an array,` |
|        - | 4825 | `		 * throws if it is a not-stringable object — and then the lvalue keeps` |
|        - | 4826 | `		 * holding that object, since the throw abandons the coercion) */` |
|        - | 4827 | `		{` |
|    84258 | 4828 | `			sxi32 rcSv = PH7_MemObjToStringUV(pObj);` |
|    84266 | 4829 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4830 | `		}` |
|    84250 | 4831 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|    83542 | 4832 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4833 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - | 4834 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 | 4835 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4836 | `				goto Abort;` |
|        - | 4837 | `			}` |
|    41350 | 4838 | `		}` |
|        - | 4839 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - | 4840 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - | 4841 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - | 4842 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - | 4843 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - | 4844 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - | 4845 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - | 4846 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - | 4847 | `		 * the same slot is appended to again later in the statement` |
|        - | 4848 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - | 4849 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - | 4850 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|    84250 | 4851 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|       15 | 4852 | `			PH7_MemObjStore(pObj,pNos);` |
|        7 | 4853 | `		}` |
|        - | 4854 | `		/* A hooked lvalue's scratch slot was appended in place — dispatch the` |
|        - | 4855 | `		 * set side now (the consume reads the computed value from the slot). */` |
|    84250 | 4856 | `		PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|    84250 | 4857 | `		pNos->nIdx = SXU32_HIGH;` |
|    84250 | 4858 | `		VmPopOperand(&pTos,1);` |
|    84250 | 4859 | `		break;` |
|        - | 4860 | `	}` |
|        - | 4861 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|        - | 4862 | `	/* Force a string cast (user-visible: warns if the lvalue is an array) */` |
|        - | 4863 | `	{` |
|    13054 | 4864 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|    13054 | 4865 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4866 | `	}` |
|        - | 4867 | `	/* Perform the concatenation (Reverse order) */` |
|       26 | 4868 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       26 | 4869 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4870 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - | 4871 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 | 4872 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4873 | `			goto Abort;` |
|        - | 4874 | `		}` |
|       12 | 4875 | `	}` |
|        - | 4876 | `	/* Perform the store operation */` |
|       26 | 4877 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|        - | 4878 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|       24 | 4879 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       26 | 4880 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       17 | 4881 | `		PH7_MemObjStore(pTos,pObj);` |
|        8 | 4882 | `	}` |
|       21 | 4883 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       21 | 4884 | `	PH7_MemObjStore(pTos,pNos);` |
|       21 | 4885 | `	VmPopOperand(&pTos,1);` |
|       21 | 4886 | `	break;` |
|        - | 4887 | `				}` |
|        - | 4888 | `/* OP_AND: * * *` |
|        - | 4889 | ` *` |
|        - | 4890 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - | 4891 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4892 | ` * stack.` |
|        - | 4893 | ` */` |
|        - | 4894 | `/* OP_OR: * * *` |
|        - | 4895 | ` *` |
|        - | 4896 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - | 4897 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4898 | ` * stack.` |
|        - | 4899 | ` */` |
|   216356 | 4900 | `case PH7_OP_LAND:` |
|        - | 4901 | `case PH7_OP_LOR: {` |
|        - | 4902 | `	VmOpRc rcOp;` |
|   433956 | 4903 | `	sState.pTos = pTos;` |
|   433956 | 4904 | `	sState.pc = pc;` |
|   433956 | 4905 | `	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);` |
|   433956 | 4906 | `	pTos = sState.pTos;` |
|   433956 | 4907 | `	pc = sState.pc;` |
|   433956 | 4908 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4909 | `		goto Abort;` |
|   433956 | 4910 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4911 | `		goto Exception;` |
|        - | 4912 | `	}` |
|   433956 | 4913 | `	break;` |
|        - | 4914 | `					  }` |
|        - | 4915 | `/*` |
|        - | 4916 | ` * OP_NULLC: * * *` |
|        - | 4917 | ` * Null coalescing operator '??'.` |
|        - | 4918 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - | 4919 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - | 4920 | ` */` |
|        - | 4921 | `/*` |
|        - | 4922 | ` * OP_NULLC: * P2 *` |
|        - | 4923 | ` * Short-circuit null coalescing '??'.` |
|        - | 4924 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - | 4925 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - | 4926 | ` */` |
|     1915 | 4927 | `case PH7_OP_NULLC: {` |
|        - | 4928 | `#ifdef UNTRUST` |
|        - | 4929 | `	if( pTos < pStack ){` |
|        - | 4930 | `		goto Abort;` |
|        - | 4931 | `	}` |
|        - | 4932 | `#endif` |
|     3835 | 4933 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 4934 | `		/* Left is not null — keep it and skip the RHS */` |
|     2133 | 4935 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|     1069 | 4936 | `	}else{` |
|        - | 4937 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|     1707 | 4938 | `		VmPopOperand(&pTos, 1);` |
|        - | 4939 | `	}` |
|     3835 | 4940 | `	break;` |
|        - | 4941 | `}` |
|        - | 4942 | `/*` |
|        - | 4943 | ` * OP_NULLC_JMP: * P2 *` |
|        - | 4944 | ` * Null coalescing assignment short-circuit.` |
|        - | 4945 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - | 4946 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - | 4947 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - | 4948 | ` */` |
|       89 | 4949 | `case PH7_OP_NULLC_JMP: {` |
|        - | 4950 | `#ifdef UNTRUST` |
|        - | 4951 | `	if( pTos < pStack ){` |
|        - | 4952 | `		goto Abort;` |
|        - | 4953 | `	}` |
|        - | 4954 | `#endif` |
|      181 | 4955 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       56 | 4956 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —` |
|        - | 4957 | `		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is` |
|        - | 4958 | `		 * dropped by the fetch-point sweep (the landing pc is past its` |
|        - | 4959 | `		 * OP_NULLC_STORE window): the skipped assign never dispatches. */` |
|       27 | 4960 | `	}` |
|      181 | 4961 | `	break;` |
|        - | 4962 | `}` |
|        - | 4963 | `/*` |
|        - | 4964 | ` * OP_NULLC_STORE: * * *` |
|        - | 4965 | ` * Null coalescing assignment store.` |
|        - | 4966 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - | 4967 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - | 4968 | ` * expression result.` |
|        - | 4969 | ` */` |
|        - | 4970 | `/*` |
|        - | 4971 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - | 4972 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - | 4973 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - | 4974 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - | 4975 | ` * non-null, fall through without modifying the stack so the following` |
|        - | 4976 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - | 4977 | ` */` |
|      113 | 4978 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - | 4979 | `#ifdef UNTRUST` |
|        - | 4980 | `	if( pTos < pStack ){` |
|        - | 4981 | `		goto Abort;` |
|        - | 4982 | `	}` |
|        - | 4983 | `#endif` |
|      231 | 4984 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - | 4985 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - | 4986 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       80 | 4987 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       39 | 4988 | `	}` |
|      231 | 4989 | `	break;` |
|        - | 4990 | `}` |
|       59 | 4991 | `case PH7_OP_NULLC_STORE: {` |
|        - | 4992 | `	VmOpRc rcOp;` |
|      121 | 4993 | `	sState.pTos = pTos;` |
|      121 | 4994 | `	sState.pc = pc;` |
|      121 | 4995 | `	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);` |
|      121 | 4996 | `	pTos = sState.pTos;` |
|      121 | 4997 | `	pc = sState.pc;` |
|      121 | 4998 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4999 | `		goto Abort;` |
|      121 | 5000 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       11 | 5001 | `		goto Exception;` |
|        - | 5002 | `	}` |
|      111 | 5003 | `	break;` |
|        - | 5004 | `					  }` |
|        - | 5005 | `/*` |
|        - | 5006 | ` * OP_SPREAD: * * *` |
|        - | 5007 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - | 5008 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - | 5009 | ` * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this` |
|        - | 5010 | ` * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —` |
|        - | 5011 | ` * the CALL may not be the next instruction, and may be an inner call whose own` |
|        - | 5012 | ` * spreads must stay scoped to it.` |
|        - | 5013 | ` * The expansion tail is shared between the plain-array and the materialized` |
|        - | 5014 | ` * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.` |
|        - | 5015 | ` */` |
|      747 | 5016 | `case PH7_OP_SPREAD: {` |
|        - | 5017 | `#ifdef UNTRUST` |
|        - | 5018 | `	if( pTos < pStack ){` |
|        - | 5019 | `		goto Abort;` |
|        - | 5020 | `	}` |
|        - | 5021 | `#endif` |
|        - | 5022 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - | 5023 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - | 5024 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - | 5025 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - | 5026 | `	 * be freed immediately. */` |
|     1404 | 5027 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|       56 | 5028 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - | 5029 | `		sxi32 rcW;` |
|       56 | 5030 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|       56 | 5031 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|       56 | 5032 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|       31 | 5033 | `			sxi32 rcRoute = SXRET_OK; /* the throw already happened; route, do not re-raise */` |
|       31 | 5034 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|       31 | 5035 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|        - | 5036 | ``			/* A bare `goto Exception` unwinds the whole invocation, which is right at a`` |
|        - | 5037 | `			 * CALL boundary and wrong here: an argument list is mid-expression, so a` |
|        - | 5038 | `			 * try/catch around the call catches this and execution must RESUME after` |
|        - | 5039 | `			 * the catch. It did not — the catch ran and every statement after it was` |
|        - | 5040 | ``			 * dropped, exit 0 (a throwing rewind()/key() in `f(...$it)` showed it long`` |
|        - | 5041 | `			 * before the key screen made the path ordinary). */` |
|       31 | 5042 | `			PH7_THROW_ROUTE_MIDEXPR(rcRoute)` |
|        - | 5043 | `		}` |
|        - | 5044 | `		/* Grow the operand stack if this expansion would overflow it (no longer a` |
|        - | 5045 | `		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a` |
|        - | 5046 | `		 * single argument — the historical bounded-fallback, now only under OOM. */` |
|       39 | 5047 | `		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,` |
|       12 | 5048 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 5049 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 5050 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 5051 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 5052 | `				pTmpMap->nEntry);` |
|      ! 0 | 5053 | `			break;` |
|        - | 5054 | `		}` |
|        - | 5055 | `		/* The buffer may have MOVED (and grown): re-anchor the watermark at the whole` |
|        - | 5056 | `		 * new capacity rather than carry a pointer into the freed one. Conservative --` |
|        - | 5057 | `		 * this activation's teardown then sweeps everything -- and OP_SPREAD is rare. */` |
|       27 | 5058 | `		pHigh = pStack + sState.nStackCap - 1;` |
|       27 | 5059 | `		VmSpreadExpandMap(pVm, &pTos, pTmpMap, 0/*a Traversable's values are not the caller's slots*/);` |
|       27 | 5060 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|       27 | 5061 | `		break;` |
|        - | 5062 | `	}` |
|     1352 | 5063 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     1292 | 5064 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|     1888 | 5065 | `		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,` |
|      596 | 5066 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 5067 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 5068 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 5069 | `				pMap->nEntry);` |
|      ! 0 | 5070 | `			break;` |
|        - | 5071 | `		}` |
|     1292 | 5072 | `		pHigh = pStack + sState.nStackCap - 1;   /* see the Traversable arm above */` |
|     1292 | 5073 | `		VmSpreadExpandMap(pVm, &pTos, pMap, pInstr->iP1 != 0);` |
|     1292 | 5074 | `		break;` |
|        - | 5075 | `	}` |
|        - | 5076 | `	/* Neither an array nor a Traversable: php refuses the unpack rather than` |
|        - | 5077 | `	 * passing the value as one ordinary argument, which is what this used to do —` |
|        - | 5078 | ``	 * `f(...'str')` bound "str" to the first parameter and `new C(...null)` bound`` |
|        - | 5079 | `	 * null, silently, on source php will not run. The argument site's class is` |
|        - | 5080 | `	 * TypeError for every type (the array-literal site keeps php's plain Error for` |
|        - | 5081 | `	 * a scalar). */` |
|        - | 5082 | `	{` |
|       62 | 5083 | `		sxi32 rcBad = VmThrowSpreadError(&(*pVm),pTos,1);` |
|       62 | 5084 | `		sxi32 rcRoute = SXRET_OK;` |
|       62 | 5085 | `		if( rcBad == PH7_ABORT \|\| rcBad == SXERR_ABORT ){` |
|      ! 0 | 5086 | `			goto Abort;` |
|        - | 5087 | `		}` |
|       62 | 5088 | `		PH7_THROW_ROUTE_MIDEXPR(rcRoute)` |
|        - | 5089 | `	}` |
|        - | 5090 | `}` |
|        - | 5091 | `/*` |
|        - | 5092 | ` * OP_FLAG_SPREAD: * * *` |
|        - | 5093 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - | 5094 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - | 5095 | ` */` |
|      364 | 5096 | `case PH7_OP_FLAG_SPREAD: {` |
|        - | 5097 | `#ifdef UNTRUST` |
|        - | 5098 | `	if( pTos < pStack ){` |
|        - | 5099 | `		goto Abort;` |
|        - | 5100 | `	}` |
|        - | 5101 | `#endif` |
|      733 | 5102 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|      733 | 5103 | `	break;` |
|        - | 5104 | `}` |
|        - | 5105 | `/* OP_LXOR: * * *` |
|        - | 5106 | ` *` |
|        - | 5107 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - | 5108 | ` * two values and push the resulting boolean value back onto the` |
|        - | 5109 | ` * stack.` |
|        - | 5110 | ` * According to the PHP language reference manual:` |
|        - | 5111 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - | 5112 | ` *  TRUE,but not both.` |
|        - | 5113 | ` */` |
|        6 | 5114 | `case PH7_OP_LXOR:{` |
|       13 | 5115 | `	ph7_value *pNos = &pTos[-1];` |
|       13 | 5116 | `	sxi32 v = 0;` |
|        - | 5117 | `#ifdef UNTRUST` |
|        - | 5118 | `	if( pNos < pStack ){` |
|        - | 5119 | `		goto Abort;` |
|        - | 5120 | `	}` |
|        - | 5121 | `#endif` |
|        - | 5122 | `	/* Force a boolean cast */` |
|       13 | 5123 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 5124 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 5125 | `	}` |
|       13 | 5126 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 5127 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 | 5128 | `	}` |
|       13 | 5129 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 | 5130 | `		v = 1;` |
|        3 | 5131 | `	}` |
|       13 | 5132 | `	VmPopOperand(&pTos,1);` |
|       13 | 5133 | `	pTos->x.iVal = v;` |
|       13 | 5134 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       13 | 5135 | `	break;` |
|        - | 5136 | `				 }` |
|        - | 5137 | `/* OP_EQ P1 P2 P3` |
|        - | 5138 | ` *` |
|        - | 5139 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - | 5140 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - | 5141 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5142 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5143 | ` */` |
|        - | 5144 | `/* OP_NEQ P1 P2 P3` |
|        - | 5145 | ` *` |
|        - | 5146 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - | 5147 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 5148 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5149 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5150 | ` */` |
|     8723 | 5151 | `case PH7_OP_EQ:` |
|        - | 5152 | `case PH7_OP_NEQ: {` |
|        - | 5153 | `	VmOpRc rcOp;` |
|    17436 | 5154 | `	sState.pTos = pTos;` |
|    17436 | 5155 | `	sState.pc = pc;` |
|    17436 | 5156 | `	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);` |
|    17436 | 5157 | `	pTos = sState.pTos;` |
|    17436 | 5158 | `	pc = sState.pc;` |
|    17436 | 5159 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5160 | `		goto Abort;` |
|    17436 | 5161 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       17 | 5162 | `		goto Exception;` |
|        - | 5163 | `	}` |
|    17420 | 5164 | `	break;` |
|        - | 5165 | `					  }` |
|        - | 5166 | `/* OP_TEQ P1 P2 *` |
|        - | 5167 | ` *` |
|        - | 5168 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - | 5169 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 5170 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5171 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5172 | ` */` |
|   506275 | 5173 | `case PH7_OP_TEQ: {` |
|        - | 5174 | `	VmOpRc rcOp;` |
|  1014563 | 5175 | `	sState.pTos = pTos;` |
|  1014563 | 5176 | `	sState.pc = pc;` |
|  1014563 | 5177 | `	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);` |
|  1014563 | 5178 | `	pTos = sState.pTos;` |
|  1014563 | 5179 | `	pc = sState.pc;` |
|  1014563 | 5180 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5181 | `		goto Abort;` |
|  1014563 | 5182 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5183 | `		goto Exception;` |
|        - | 5184 | `	}` |
|  1014563 | 5185 | `	break;` |
|        - | 5186 | `					  }` |
|        - | 5187 | `/* OP_TNE P1 P2 *` |
|        - | 5188 | ` *` |
|        - | 5189 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - | 5190 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - | 5191 | ` * instruction.` |
|        - | 5192 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5193 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5194 | ` *` |
|        - | 5195 | ` */` |
|   639225 | 5196 | `case PH7_OP_TNE: {` |
|        - | 5197 | `	VmOpRc rcOp;` |
|  1279629 | 5198 | `	sState.pTos = pTos;` |
|  1279629 | 5199 | `	sState.pc = pc;` |
|  1279629 | 5200 | `	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);` |
|  1279629 | 5201 | `	pTos = sState.pTos;` |
|  1279629 | 5202 | `	pc = sState.pc;` |
|  1279629 | 5203 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5204 | `		goto Abort;` |
|  1279629 | 5205 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5206 | `		goto Exception;` |
|        - | 5207 | `	}` |
|  1279629 | 5208 | `	break;` |
|        - | 5209 | `					  }` |
|        - | 5210 | `/* OP_LT P1 P2 P3` |
|        - | 5211 | ` *` |
|        - | 5212 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 5213 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 5214 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 5215 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5216 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5217 | ` *` |
|        - | 5218 | ` */` |
|        - | 5219 | `/* OP_LE P1 P2 P3` |
|        - | 5220 | ` *` |
|        - | 5221 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 5222 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 5223 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 5224 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5225 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5226 | ` *` |
|        - | 5227 | ` */` |
|   617834 | 5228 | `case PH7_OP_LT:` |
|        - | 5229 | `case PH7_OP_LE: {` |
|        - | 5230 | `	VmOpRc rcOp;` |
|  1239354 | 5231 | `	sState.pTos = pTos;` |
|  1239354 | 5232 | `	sState.pc = pc;` |
|  1239354 | 5233 | `	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);` |
|  1239354 | 5234 | `	pTos = sState.pTos;` |
|  1239354 | 5235 | `	pc = sState.pc;` |
|  1239354 | 5236 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5237 | `		goto Abort;` |
|  1239354 | 5238 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 5239 | `		goto Exception;` |
|        - | 5240 | `	}` |
|  1239350 | 5241 | `	break;` |
|        - | 5242 | `					  }` |
|        - | 5243 | `/* OP_GT P1 P2 P3` |
|        - | 5244 | ` *` |
|        - | 5245 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 5246 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 5247 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 5248 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5249 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5250 | ` *` |
|        - | 5251 | ` */` |
|        - | 5252 | `/* OP_GE P1 P2 P3` |
|        - | 5253 | ` *` |
|        - | 5254 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 5255 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 5256 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 5257 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 5258 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 5259 | ` *` |
|        - | 5260 | ` */` |
|   172879 | 5261 | `case PH7_OP_GT:` |
|        - | 5262 | `case PH7_OP_GE: {` |
|        - | 5263 | `	VmOpRc rcOp;` |
|   347044 | 5264 | `	sState.pTos = pTos;` |
|   347044 | 5265 | `	sState.pc = pc;` |
|   347044 | 5266 | `	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);` |
|   347044 | 5267 | `	pTos = sState.pTos;` |
|   347044 | 5268 | `	pc = sState.pc;` |
|   347044 | 5269 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5270 | `		goto Abort;` |
|   347044 | 5271 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5272 | `		goto Exception;` |
|        - | 5273 | `	}` |
|   347044 | 5274 | `	break;` |
|        - | 5275 | `					  }` |
|        - | 5276 | `/* OP_SPACESHIP * * *` |
|        - | 5277 | ` *` |
|        - | 5278 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - | 5279 | ` *   -1 if left < right` |
|        - | 5280 | ` *    0 if left == right` |
|        - | 5281 | ` *    1 if left > right` |
|        - | 5282 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - | 5283 | ` */` |
|      925 | 5284 | `case PH7_OP_SPACESHIP: {` |
|        - | 5285 | `	VmOpRc rcOp;` |
|     1855 | 5286 | `	sState.pTos = pTos;` |
|     1855 | 5287 | `	sState.pc = pc;` |
|     1855 | 5288 | `	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);` |
|     1855 | 5289 | `	pTos = sState.pTos;` |
|     1855 | 5290 | `	pc = sState.pc;` |
|     1855 | 5291 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5292 | `		goto Abort;` |
|     1855 | 5293 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 5294 | `		goto Exception;` |
|        - | 5295 | `	}` |
|     1849 | 5296 | `	break;` |
|        - | 5297 | `					  }` |
|        - | 5298 | `/*` |
|        - | 5299 | ` * OP_LOAD_REF * * *` |
|        - | 5300 | ` * Push the index of a referenced object on the stack.` |
|        - | 5301 | ` */` |
|       99 | 5302 | `case PH7_OP_LOAD_REF: {` |
|        - | 5303 | `	sxu32 nIdx;` |
|        - | 5304 | `#ifdef UNTRUST` |
|        - | 5305 | `	if( pTos < pStack ){` |
|        - | 5306 | `		goto Abort;` |
|        - | 5307 | `	}` |
|        - | 5308 | `#endif` |
|      201 | 5309 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|        - | 5310 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|        - | 5311 | `		 * out of a string still carries the BASE VARIABLE's slot index, so taking a` |
|        - | 5312 | `` 		 * reference to it aliased the WHOLE STRING — `$x = [&$s[1]]; $x[0] = "Z";` `` |
|        - | 5313 | ``		 * left $s === "Z". Same Error the `=&` and by-ref-argument paths raise. */`` |
|        3 | 5314 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|        - | 5315 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|        3 | 5316 | `		PH7_MemObjRelease(pTos);` |
|        3 | 5317 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 5318 | `		pTos->nIdx = SXU32_HIGH;` |
|        3 | 5319 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|        3 | 5320 | `		break;` |
|        - | 5321 | `	}` |
|        - | 5322 | `	/* Extract memory object index */` |
|      198 | 5323 | `	nIdx = pTos->nIdx;` |
|      198 | 5324 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - | 5325 | `		/* Nullify the object */` |
|      198 | 5326 | `		PH7_MemObjRelease(pTos);` |
|        - | 5327 | `		/* Mark as constant and store the index on the top of the stack */` |
|      198 | 5328 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      198 | 5329 | `		pTos->nIdx = SXU32_HIGH;` |
|      198 | 5330 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       98 | 5331 | `	}` |
|      198 | 5332 | `	break;` |
|        - | 5333 | `					  }` |
|        - | 5334 | `/*` |
|        - | 5335 | ` * OP_STORE_REF * * P3` |
|        - | 5336 | ` * Perform an assignment operation by reference.` |
|        - | 5337 | ` */` |
|     2533 | 5338 | `case PH7_OP_STORE_REF: {` |
|        - | 5339 | `	VmOpRc rcOp;` |
|     5071 | 5340 | `	sState.pTos = pTos;` |
|     5071 | 5341 | `	sState.pc = pc;` |
|     5071 | 5342 | `	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);` |
|     5071 | 5343 | `	pTos = sState.pTos;` |
|     5071 | 5344 | `	pc = sState.pc;` |
|     5071 | 5345 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 5346 | `		goto Abort;` |
|     5069 | 5347 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       22 | 5348 | `		goto Exception;` |
|        - | 5349 | `	}` |
|     5049 | 5350 | `	break;` |
|        - | 5351 | `					  }` |
|        - | 5352 | `/*` |
|        - | 5353 | ` * OP_UPLINK P1 * *` |
|        - | 5354 | ` * Link a variable to the top active VM frame.` |
|        - | 5355 | ` * This is used to implement the 'global' PHP construct.` |
|        - | 5356 | ` */` |
|      878 | 5357 | `case PH7_OP_UPLINK: {` |
|     1757 | 5358 | `	if( pVm->pFrame->pParent ){` |
|     1757 | 5359 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - | 5360 | `		SyString sName;` |
|        - | 5361 | `		/* rcSv carries the coercion status OUT of the loop: the routing macro is a` |
|        - | 5362 | ``		 * bare `break` here, which would only leave the while and then pop the`` |
|        - | 5363 | `		 * operands with a throw pending. */` |
|     1757 | 5364 | `		sxi32 rcSv = SXRET_OK;` |
|        - | 5365 | `		/* Perform the link */` |
|     4373 | 5366 | `		while( pLink <= pTos ){` |
|        - | 5367 | `			/* Force a string cast — global $$arr link name (user-visible) */` |
|     2627 | 5368 | `			rcSv = PH7_MemObjToStringUV(pLink);` |
|     2627 | 5369 | `			if( rcSv != SXRET_OK ){` |
|        7 | 5370 | `				break;` |
|        - | 5371 | `			}` |
|     2621 | 5372 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|     2621 | 5373 | `			if( sName.nByte > 0 ){` |
|     2621 | 5374 | `				VmFrameLink(&(*pVm),&sName);` |
|     1306 | 5375 | `			}` |
|     2621 | 5376 | `			pLink++;` |
|        5 | 5377 | `		}` |
|     1757 | 5378 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|      871 | 5379 | `	}` |
|     1751 | 5380 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|     1751 | 5381 | `	break;` |
|        - | 5382 | `					}` |
|        - | 5383 | `/*` |
|        - | 5384 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - | 5385 | ` * Push an exception in the corresponding container so that` |
|        - | 5386 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - | 5387 | ` */` |
|   741462 | 5388 | `case PH7_OP_LOAD_EXCEPTION: {` |
|        - | 5389 | `	VmOpRc rcOp;` |
|  1482711 | 5390 | `	sState.pTos = pTos;` |
|  1482711 | 5391 | `	sState.pc = pc;` |
|  1482711 | 5392 | `	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);` |
|  1482711 | 5393 | `	pTos = sState.pTos;` |
|  1482711 | 5394 | `	pc = sState.pc;` |
|  1482711 | 5395 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5396 | `		goto Abort;` |
|  1482711 | 5397 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5398 | `		goto Exception;` |
|        - | 5399 | `	}` |
|  1482711 | 5400 | `	break;` |
|        - | 5401 | `					  }` |
|        - | 5402 | `/*` |
|        - | 5403 | ` * OP_POP_EXCEPTION * * P3` |
|        - | 5404 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - | 5405 | ` */` |
|   688112 | 5406 | `case PH7_OP_POP_EXCEPTION: {` |
|  1376011 | 5407 | `	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;` |
|        - | 5408 | `	VmFrame *pBodyFrame;` |
|        - | 5409 | `	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on` |
|        - | 5410 | `	 * top (matched by compiled origin). pException == NULL means this try's` |
|        - | 5411 | `	 * activation was already consumed (an in-place catch handled a throw and` |
|        - | 5412 | `	 * ran the finally itself); compiled fields keep coming from p3. */` |
|  1376011 | 5413 | `	ph7_exception *pException = 0;` |
|  1376011 | 5414 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|    11118 | 5415 | `		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|    11118 | 5416 | `		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];` |
|        - | 5417 | `		/* Same compiled origin is NOT enough: under recursion, when THIS` |
|        - | 5418 | `		 * level's activation was already consumed by an in-place catch (the` |
|        - | 5419 | `		 * resume lands right here), the top can be an OUTER level's activation` |
|        - | 5420 | `		 * of the same lexical try — popping it would run that level's finally` |
|        - | 5421 | `		 * early and orphan its handler (probed: recursive try/catch/finally` |
|        - | 5422 | `		 * lost the outer catch entirely). The activation must also belong to` |
|        - | 5423 | `		 * the CURRENT body frame. */` |
|    11113 | 5424 | `		if( VmExcMatches(pTop,pCompiledExc)` |
|    11005 | 5425 | `		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){` |
|    10886 | 5426 | `			pException = pTop;` |
|    10886 | 5427 | `			(void)SySetPop(&pVm->aException);` |
|     5359 | 5428 | `		}` |
|     5475 | 5429 | `	}` |
|  1376011 | 5430 | `	if( pCompiledExc->iInlined ){` |
|        - | 5431 | `		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.` |
|        - | 5432 | `		 * Pop this try's handler off aException so a throw in the finally propagates to` |
|        - | 5433 | `		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the` |
|        - | 5434 | `		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the` |
|        - | 5435 | `		 * compiler-emitted JMP falls into the finally. Without a finally, this is the` |
|        - | 5436 | `		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */` |
|      171 | 5437 | `		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */` |
|      171 | 5438 | `		if( pCompiledExc->iHasFinally ){` |
|        - | 5439 | `			VmFinallyAction sAct;` |
|       15 | 5440 | `			SyZero(&sAct,sizeof(sAct));` |
|       15 | 5441 | `			sAct.eKind = PH7_FA_FALLTHROUGH;` |
|       15 | 5442 | `			sAct.iNextPc = pCompiledExc->iEndCatchPc;` |
|       15 | 5443 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        - | 5444 | `			/* keep the transparent frame for OP_END_FINALLY */` |
|      165 | 5445 | `		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       22 | 5446 | `			VmLeaveFrame(&(*pVm));` |
|        9 | 5447 | `		}` |
|      171 | 5448 | `		break;` |
|        - | 5449 | `	}` |
|        - | 5450 | `	/* Leave the exception frame. It is normally on top here (a try that fell through` |
|        - | 5451 | `	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a` |
|        - | 5452 | `	 * RESUMED generator/fiber body whose yield was inside this try, that exception` |
|        - | 5453 | `	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),` |
|        - | 5454 | `	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the` |
|        - | 5455 | `	 * coroutine (and defeat the bHasRet materialization just below, which must see the` |
|        - | 5456 | `	 * body). Only leave a genuine exception frame. */` |
|        - | 5457 | `	/* ...and never THIS exec's own entry frame. A catch body runs as a` |
|        - | 5458 | `	 * mini-program entered on the transparent catch frame, which carries` |
|        - | 5459 | `	 * VM_FRAME_EXCEPTION too; a try nested inside that catch body whose own` |
|        - | 5460 | `	 * exception frame is already gone then popped the mini-program's entry` |
|        - | 5461 | `	 * frame instead, and this body's terminal OP_DONE read it back (bHasRet)` |
|        - | 5462 | `	 * after it had been freed. An exec never owns the teardown of the frame it` |
|        - | 5463 | `	 * was entered on -- whoever entered it does. */` |
|  1375845 | 5464 | `	if( (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) && pVm->pFrame != sState.pEntryFrame ){` |
|  1065386 | 5465 | `		VmLeaveFrame(&(*pVm));` |
|   532607 | 5466 | `	}` |
|        - | 5467 | `	/* Execute the finally block if present and not already executed by the` |
|        - | 5468 | `	 * catch path. No live activation (pException == NULL) means an in-place` |
|        - | 5469 | `	 * catch consumed it — and that path runs the finally itself — so skip. */` |
|  1375845 | 5470 | `	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){` |
|        - | 5471 | `		sxi32 rcFinally;` |
|       69 | 5472 | `		VmExcRelease(&(*pVm),pException);` |
|       69 | 5473 | `		pException = 0;` |
|       69 | 5474 | `		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);` |
|       69 | 5475 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 | 5476 | `			goto Abort;` |
|        - | 5477 | `		}` |
|       69 | 5478 | `		if( rcFinally == PH7_EXCEPTION ){` |
|        - | 5479 | `			/* The finally threw past itself. If an enclosing try IN THIS function` |
|        - | 5480 | `			 * caught the new exception in place, resume at its landing pad;` |
|        - | 5481 | `			 * otherwise it was caught at an outer frame, so unwind this function. */` |
|        - | 5482 | `			sxi32 iResumePc;` |
|        3 | 5483 | `			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 5484 | `				pc = iResumePc;` |
|        3 | 5485 | `				break;` |
|        - | 5486 | `			}` |
|      ! 0 | 5487 | `			goto Exception;` |
|        - | 5488 | `		}` |
|       31 | 5489 | `	}` |
|  1375843 | 5490 | `	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */` |
|  1375843 | 5491 | `	pBodyFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  1375843 | 5492 | `	if( pBodyFrame->bHasRet ){` |
|        - | 5493 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - | 5494 | `		 * function. The return targets the body frame this try belongs to. Drain` |
|        - | 5495 | `		 * outer finally blocks first, then — only in the real function body` |
|        - | 5496 | `		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline` |
|        - | 5497 | `		 * try within a catch/finally) propagate outward so the owning body returns. */` |
|    23993 | 5498 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    23993 | 5499 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 5500 | `			goto Abort;` |
|        - | 5501 | `		}` |
|    23993 | 5502 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 5503 | `			goto Exception;` |
|        - | 5504 | `		}` |
|    23993 | 5505 | `		if( !sState.bReturnPropagates ){` |
|    23987 | 5506 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|    11991 | 5507 | `		}` |
|    23993 | 5508 | `		goto Done;` |
|        - | 5509 | `	}` |
|  1351855 | 5510 | `	if( pBodyFrame->nCatchJmpPc > 0 ){` |
|        - | 5511 | ``		/* A `break`/`continue` inside the catch (OP_CATCH_JMP) parked its loop-exit`` |
|        - | 5512 | `		 * target on this body frame, and this is a landing pad on its way out. */` |
|       87 | 5513 | `		if( pBodyFrame->nCatchJmpLevels > 1 ){` |
|        - | 5514 | `			/* Still one or more detached bodies out from the target's array — this try` |
|        - | 5515 | `			 * was declared INSIDE another catch/finally mini-program. End this one and` |
|        - | 5516 | `			 * let the park travel outward; the next landing pad decrements again. */` |
|        6 | 5517 | `			pBodyFrame->nCatchJmpLevels--;` |
|        6 | 5518 | `			goto Done;` |
|        - | 5519 | `		}` |
|       83 | 5520 | `		if( pBodyFrame->nCatchJmpCross > 0 ){` |
|        - | 5521 | `			/* Enclosing trys between the catch and the loop: the jump lands past their` |
|        - | 5522 | `			 * OP_POP_EXCEPTION, so run their finally (and leave their frames) here. */` |
|        8 | 5523 | `			sxu32 nCross = pBodyFrame->nCatchJmpCross;` |
|        8 | 5524 | `			pBodyFrame->nCatchJmpCross = 0;` |
|        8 | 5525 | `			rc = VmDrainCrossedTrys(&(*pVm),nCross,sState.nExceptionBase);` |
|        8 | 5526 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5527 | `				goto Abort;` |
|        - | 5528 | `			}` |
|        8 | 5529 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 5530 | `				/* A drained finally threw past itself — it supersedes the loop exit. */` |
|      ! 0 | 5531 | `				pBodyFrame->nCatchJmpPc = 0;` |
|      ! 0 | 5532 | `				goto Exception;` |
|        - | 5533 | `			}` |
|        3 | 5534 | `		}` |
|       83 | 5535 | `		pc = (sxi32)pBodyFrame->nCatchJmpPc - 1;` |
|       83 | 5536 | `		pBodyFrame->nCatchJmpPc = 0;` |
|       83 | 5537 | `		break;` |
|        - | 5538 | `	}` |
|  1351771 | 5539 | `	break;` |
|        - | 5540 | `							}` |
|        - | 5541 | `/*` |
|        - | 5542 | ` * OP_CATCH_JMP P1(levels,cross) P2(target pc) *` |
|        - | 5543 | ` * Jump to iP2, leaving the try/catch structures iP1 describes (PH7_CATCH_JMP_P1).` |
|        - | 5544 | ` *` |
|        - | 5545 | ` * CROSS enclosing trys are being left without reaching their OP_POP_EXCEPTION, so this` |
|        - | 5546 | ` * runs their finallys itself. LEVELS says whether the target is even addressable from` |
|        - | 5547 | `` * here: 0 means it is in this same array (a `goto` out of a try body) and the jump is`` |
|        - | 5548 | ` * taken on the spot; above 0 the jump starts inside a DETACHED catch/finally` |
|        - | 5549 | ` * mini-program whose array is not the target's, so the target is PARKED on the owning` |
|        - | 5550 | `` * body's frame and this mini-program ends — mirroring how an explicit `return` in a`` |
|        - | 5551 | ` * catch parks on sRet at OP_DONE above. VmThrowException then runs this try's finally` |
|        - | 5552 | ` * and returns; the resume lands on the try's OP_POP_EXCEPTION, which decrements LEVELS` |
|        - | 5553 | ` * and either re-parks (another mini-program out) or takes the jump.` |
|        - | 5554 | ` */` |
|       47 | 5555 | `case PH7_OP_CATCH_JMP: {` |
|        - | 5556 | `	VmFrame *pTgt;` |
|       98 | 5557 | `	if( PH7_CATCH_JMP_LEVELS(pInstr->iP1) == 0 ){` |
|        - | 5558 | ``		/* No detached body to leave — a `goto` whose target is in THIS array, but which`` |
|        - | 5559 | `		 * jumps out of one or more enclosing trys. Nothing to park: run their finallys` |
|        - | 5560 | `		 * (a bare jump would leave them to fire at teardown) and go. */` |
|       15 | 5561 | `		rc = VmDrainCrossedTrys(&(*pVm),PH7_CATCH_JMP_CROSS(pInstr->iP1),sState.nExceptionBase);` |
|       15 | 5562 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 5563 | `			goto Abort;` |
|        - | 5564 | `		}` |
|       15 | 5565 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 5566 | `			/* A drained finally threw past itself — it supersedes the jump. */` |
|      ! 0 | 5567 | `			goto Exception;` |
|        - | 5568 | `		}` |
|       15 | 5569 | `		pc = (sxi32)pInstr->iP2 - 1;` |
|       15 | 5570 | `		break;` |
|        - | 5571 | `	}` |
|       85 | 5572 | `	pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|       85 | 5573 | `	pTgt->nCatchJmpPc = (sxu32)pInstr->iP2;` |
|       85 | 5574 | `	pTgt->nCatchJmpLevels = PH7_CATCH_JMP_LEVELS(pInstr->iP1);` |
|       85 | 5575 | `	pTgt->nCatchJmpCross = PH7_CATCH_JMP_CROSS(pInstr->iP1);` |
|        - | 5576 | `	/* A try opened INSIDE this catch body that the jump crosses had its finally run by` |
|        - | 5577 | `	 * the compiler-emitted OP_POP_EXCEPTION; drain anything still pending here. */` |
|       85 | 5578 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|       85 | 5579 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 5580 | `		goto Abort;` |
|        - | 5581 | `	}` |
|       85 | 5582 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 5583 | `		/* A drained finally threw past itself — it discards this jump. */` |
|      ! 0 | 5584 | `		pTgt->nCatchJmpPc = 0;` |
|      ! 0 | 5585 | `		goto Exception;` |
|        - | 5586 | `	}` |
|       85 | 5587 | `	goto Done;` |
|        - | 5588 | `					   }` |
|        - | 5589 | `/*` |
|        - | 5590 | ` * OP_CATCH iP1(catch-index) * P3(ph7_exception)` |
|        - | 5591 | ` * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on` |
|        - | 5592 | ` * pException->pInflight by VmThrowInline) into the catch variable, resolved in the` |
|        - | 5593 | ` * enclosing body's scope (PHP: a catch shares the surrounding variable scope).` |
|        - | 5594 | ` */` |
|       42 | 5595 | `case PH7_OP_CATCH: {` |
|        - | 5596 | `	VmOpRc rcOp;` |
|       89 | 5597 | `	sState.pTos = pTos;` |
|       89 | 5598 | `	sState.pc = pc;` |
|       89 | 5599 | `	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);` |
|       89 | 5600 | `	pTos = sState.pTos;` |
|       89 | 5601 | `	pc = sState.pc;` |
|       89 | 5602 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5603 | `		goto Abort;` |
|       89 | 5604 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5605 | `		goto Exception;` |
|        - | 5606 | `	}` |
|       89 | 5607 | `	break;` |
|        - | 5608 | `					  }` |
|        - | 5609 | `/*` |
|        - | 5610 | ` * OP_END_FINALLY * P3(ph7_exception)` |
|        - | 5611 | ` * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch` |
|        - | 5612 | ` * the pending action queued when the finally was entered (fall-through / re-throw /` |
|        - | 5613 | ` * return / break-continue). A return/break threads out through each enclosing finally` |
|        - | 5614 | ` * via pException->iNextFinallyPc.` |
|        - | 5615 | ` */` |
|       26 | 5616 | `case PH7_OP_END_FINALLY: {` |
|       57 | 5617 | `	ph7_exception *pExc = (ph7_exception *)pInstr->p3;` |
|        - | 5618 | `	VmFinallyAction sAct;` |
|       57 | 5619 | `	int eKind = PH7_FA_FALLTHROUGH;` |
|        - | 5620 | `	/* Leave the try's transparent frame kept alive across the finally. */` |
|       57 | 5621 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        8 | 5622 | `		VmLeaveFrame(&(*pVm));` |
|        3 | 5623 | `	}` |
|       57 | 5624 | `	if( SySetUsed(&pVm->aFinallyAction) > 0 ){` |
|       55 | 5625 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);` |
|       55 | 5626 | `		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];` |
|       55 | 5627 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|       55 | 5628 | `		eKind = sAct.eKind;` |
|       30 | 5629 | `	}else{` |
|        3 | 5630 | `		SyZero(&sAct,sizeof(sAct));` |
|        3 | 5631 | `		sAct.iNextPc = pExc->iEndCatchPc;` |
|        - | 5632 | `	}` |
|       57 | 5633 | `	if( eKind == PH7_FA_FALLTHROUGH ){` |
|       16 | 5634 | `		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;` |
|       20 | 5635 | `		break;` |
|       45 | 5636 | `	}else if( eKind == PH7_FA_JMP ){` |
|        - | 5637 | `		/* break/continue: run the remaining crossed finallys, then take the jump. */` |
|        5 | 5638 | `		sxu32 iFpc = 0;` |
|        5 | 5639 | `		int nCross = sAct.nCross;` |
|        5 | 5640 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|      ! 0 | 5641 | `			sAct.nCross = nCross;` |
|      ! 0 | 5642 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      ! 0 | 5643 | `			pc = (sxi32)iFpc - 1;` |
|      ! 0 | 5644 | `			break;` |
|        - | 5645 | `		}` |
|        5 | 5646 | `		pc = (sxi32)sAct.iNextPc - 1;` |
|        5 | 5647 | `		break;` |
|       41 | 5648 | `	}else if( eKind == PH7_FA_RETHROW ){` |
|       11 | 5649 | `		ph7_class_instance *pRe = sAct.pExc;` |
|        - | 5650 | `		sxi32 _iRpE;` |
|       11 | 5651 | `		rc = VmThrowException(&(*pVm),pRe);` |
|       11 | 5652 | `		if( pRe ){ PH7_ClassInstanceUnref(pRe); }` |
|       11 | 5653 | `		if( rc == SXERR_ABORT ){ goto Abort; }` |
|       11 | 5654 | `		PH7_INLINE_RESUME_BREAK()` |
|       11 | 5655 | `		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }` |
|       11 | 5656 | `		goto Exception;` |
|      ! 0 | 5657 | `	}else{ /* PH7_FA_RETURN */` |
|       31 | 5658 | `		sxu32 iFpc = 0;` |
|       31 | 5659 | `		int nCross = sAct.nCross;` |
|       31 | 5660 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        - | 5661 | `			/* Thread the return through the next enclosing finally. */` |
|        6 | 5662 | `			sAct.nCross = nCross;` |
|        6 | 5663 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        6 | 5664 | `			pc = (sxi32)iFpc - 1;` |
|        6 | 5665 | `			break;` |
|        - | 5666 | `		}` |
|        - | 5667 | `		/* No enclosing finally left: materialize the return from this body. */` |
|       27 | 5668 | `		if( sAct.bHasRetVal && sState.pResult ){` |
|        6 | 5669 | `			PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|        2 | 5670 | `		}` |
|       27 | 5671 | `		PH7_MemObjRelease(&sAct.sRet);` |
|       27 | 5672 | `		goto Done;` |
|        - | 5673 | `	}` |
|        - | 5674 | `						 }` |
|        - | 5675 | `/*` |
|        - | 5676 | ` * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)` |
|        - | 5677 | `` * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a`` |
|        - | 5678 | ` * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);` |
|        - | 5679 | ` * OP_END_FINALLY threads it out through the finally chain and then returns.` |
|        - | 5680 | ` */` |
|       30 | 5681 | `case PH7_OP_SET_FINALLY_RET: {` |
|        - | 5682 | `	VmFinallyAction sAct;` |
|       65 | 5683 | `	sxu32 iFpc = 0;` |
|       65 | 5684 | `	int nCross = -1; /* a return crosses every enclosing finally in this function */` |
|       65 | 5685 | `	SyZero(&sAct,sizeof(sAct));` |
|       65 | 5686 | `	sAct.eKind = PH7_FA_RETURN;` |
|       65 | 5687 | `	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       65 | 5688 | `	PH7_MemObjInit(pVm,&sAct.sRet);` |
|       65 | 5689 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|       52 | 5690 | `		PH7_MemObjStore(pTos,&sAct.sRet);` |
|       52 | 5691 | `		sAct.bHasRetVal = 1;` |
|       52 | 5692 | `		VmPopOperand(&pTos,1);` |
|       24 | 5693 | `	}` |
|       65 | 5694 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        9 | 5695 | `		sAct.nCross = nCross;` |
|        9 | 5696 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        9 | 5697 | `		pc = (sxi32)iFpc - 1;` |
|        9 | 5698 | `		break;` |
|        - | 5699 | `	}` |
|        - | 5700 | `	/* No enclosing finally left: return now. */` |
|       59 | 5701 | `	if( sAct.bHasRetVal && sState.pResult ){` |
|       48 | 5702 | `		PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|       22 | 5703 | `	}` |
|       59 | 5704 | `	PH7_MemObjRelease(&sAct.sRet);` |
|       59 | 5705 | `	goto Done;` |
|        - | 5706 | `						 }` |
|        - | 5707 | `/*` |
|        - | 5708 | ` * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)` |
|        - | 5709 | `` * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action`` |
|        - | 5710 | ` * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.` |
|        - | 5711 | ` */` |
|        4 | 5712 | `case PH7_OP_SET_FINALLY_JMP: {` |
|        - | 5713 | `	VmFinallyAction sAct;` |
|       11 | 5714 | `	sxu32 iFpc = 0;` |
|       11 | 5715 | `	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */` |
|       11 | 5716 | `	SyZero(&sAct,sizeof(sAct));` |
|       11 | 5717 | `	sAct.eKind = PH7_FA_JMP;` |
|       11 | 5718 | `	sAct.iNextPc = pInstr->iP2;` |
|       11 | 5719 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        5 | 5720 | `		sAct.nCross = nCross;` |
|        5 | 5721 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        5 | 5722 | `		pc = (sxi32)iFpc - 1;` |
|        5 | 5723 | `		break;` |
|        - | 5724 | `	}` |
|        - | 5725 | `	/* No finally among the crossed trys: just take the break/continue jump. */` |
|        6 | 5726 | `	pc = (sxi32)sAct.iNextPc - 1;` |
|        6 | 5727 | `	break;` |
|        - | 5728 | `						 }` |
|        - | 5729 | `/*` |
|        - | 5730 | ` * OP_THROW * P2 *` |
|        - | 5731 | ` * Throw an user exception.` |
|        - | 5732 | ` */` |
|   500532 | 5733 | `case PH7_OP_THROW: {` |
|        - | 5734 | `	VmOpRc rcOp;` |
|  1001066 | 5735 | `	sState.pTos = pTos;` |
|  1001066 | 5736 | `	sState.pc = pc;` |
|  1001066 | 5737 | `	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);` |
|  1001066 | 5738 | `	pTos = sState.pTos;` |
|  1001066 | 5739 | `	pc = sState.pc;` |
|  1001066 | 5740 | `	if( rcOp == VM_OP_ABORT ){` |
|       51 | 5741 | `		goto Abort;` |
|  1001020 | 5742 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|   600546 | 5743 | `		goto Exception;` |
|        - | 5744 | `	}` |
|   400479 | 5745 | `	break;` |
|        - | 5746 | `					  }` |
|        - | 5747 | `/*` |
|        - | 5748 | ` * OP_FOREACH_INIT * P2 P3` |
|        - | 5749 | ` * Prepare a foreach step.` |
|        - | 5750 | ` */` |
|    21978 | 5751 | `case PH7_OP_FOREACH_INIT: {` |
|        - | 5752 | `	VmOpRc rcOp;` |
|    43892 | 5753 | `	sState.pTos = pTos;` |
|    43892 | 5754 | `	sState.pc = pc;` |
|    43892 | 5755 | `	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);` |
|    43892 | 5756 | `	pTos = sState.pTos;` |
|    43892 | 5757 | `	pc = sState.pc;` |
|    43892 | 5758 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5759 | `		goto Abort;` |
|    43892 | 5760 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       41 | 5761 | `		goto Exception;` |
|        - | 5762 | `	}` |
|    43854 | 5763 | `	break;` |
|        - | 5764 | `					  }` |
|        - | 5765 | `/*` |
|        - | 5766 | ` * OP_FOREACH_STEP * P2 P3` |
|        - | 5767 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - | 5768 | ` */` |
|   322477 | 5769 | `case PH7_OP_FOREACH_STEP: {` |
|        - | 5770 | `	VmOpRc rcOp;` |
|   644134 | 5771 | `	sState.pTos = pTos;` |
|   644134 | 5772 | `	sState.pc = pc;` |
|   644134 | 5773 | `	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);` |
|   644134 | 5774 | `	pTos = sState.pTos;` |
|   644134 | 5775 | `	pc = sState.pc;` |
|   644134 | 5776 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 5777 | `		goto Abort;` |
|   644132 | 5778 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5779 | `		goto Exception;` |
|        - | 5780 | `	}` |
|   644132 | 5781 | `	break;` |
|        - | 5782 | `						  }` |
|        - | 5783 | `/*` |
|        - | 5784 | ` * OP_MEMBER P1 P2` |
|        - | 5785 | ` * Load class attribute/method on the stack.` |
|        - | 5786 | ` */` |
|   195220 | 5787 | `case PH7_OP_MEMBER: {` |
|        - | 5788 | `	VmOpRc rcOp;` |
|   390341 | 5789 | `	sState.pTos = pTos;` |
|   390341 | 5790 | `	sState.pc = pc;` |
|   390341 | 5791 | `	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);` |
|   390341 | 5792 | `	pTos = sState.pTos;` |
|   390341 | 5793 | `	pc = sState.pc;` |
|   390341 | 5794 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 5795 | `		goto Abort;` |
|   390339 | 5796 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      282 | 5797 | `		goto Exception;` |
|        - | 5798 | `	}` |
|   390061 | 5799 | `	break;` |
|        - | 5800 | `					  }` |
|        - | 5801 | `/*` |
|        - | 5802 | ` * OP_NEW P1 * * *` |
|        - | 5803 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - | 5804 | ` */` |
|  1070079 | 5805 | `case PH7_OP_NEW: {` |
|        - | 5806 | `	VmOpRc rcOp;` |
|  2140121 | 5807 | `	sState.pTos = pTos;` |
|  2140121 | 5808 | `	sState.pc = pc;` |
|  2140121 | 5809 | `	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);` |
|  2140121 | 5810 | `	pTos = sState.pTos;` |
|  2140121 | 5811 | `	pc = sState.pc;` |
|  2140121 | 5812 | `	if( rcOp == VM_OP_ABORT ){` |
|       16 | 5813 | `		goto Abort;` |
|  2140109 | 5814 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      338 | 5815 | `		goto Exception;` |
|        - | 5816 | `	}` |
|  2139776 | 5817 | `	break;` |
|        - | 5818 | `					  }` |
|        - | 5819 | `/*` |
|        - | 5820 | ` * OP_CLONE * * *` |
|        - | 5821 | ` * Perfome a clone operation.` |
|        - | 5822 | ` */` |
|      302 | 5823 | `case PH7_OP_CLONE: {` |
|        - | 5824 | `	VmOpRc rcOp;` |
|      609 | 5825 | `	sState.pTos = pTos;` |
|      609 | 5826 | `	sState.pc = pc;` |
|      609 | 5827 | `	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);` |
|      609 | 5828 | `	pTos = sState.pTos;` |
|      609 | 5829 | `	pc = sState.pc;` |
|      609 | 5830 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5831 | `		goto Abort;` |
|      609 | 5832 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       50 | 5833 | `		goto Exception;` |
|        - | 5834 | `	}` |
|      561 | 5835 | `	break;` |
|        - | 5836 | `					  }` |
|        - | 5837 | `/*` |
|        - | 5838 | ` * OP_SWITCH * * P3` |
|        - | 5839 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - | 5840 | ` */` |
|      125 | 5841 | `case PH7_OP_SWITCH: {` |
|        - | 5842 | `	VmOpRc rcOp;` |
|      255 | 5843 | `	sState.pTos = pTos;` |
|      255 | 5844 | `	sState.pc = pc;` |
|      255 | 5845 | `	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);` |
|      255 | 5846 | `	pTos = sState.pTos;` |
|      255 | 5847 | `	pc = sState.pc;` |
|      255 | 5848 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5849 | `		goto Abort;` |
|      255 | 5850 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 5851 | `		goto Exception;` |
|        - | 5852 | `	}` |
|      251 | 5853 | `	break;` |
|        - | 5854 | `					  }` |
|        - | 5855 | `/*` |
|        - | 5856 | ` * OP_MATCH * * P3` |
|        - | 5857 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - | 5858 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - | 5859 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - | 5860 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - | 5861 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - | 5862 | ` */` |
|      118 | 5863 | `case PH7_OP_MATCH: {` |
|        - | 5864 | `	VmOpRc rcOp;` |
|      240 | 5865 | `	sState.pTos = pTos;` |
|      240 | 5866 | `	sState.pc = pc;` |
|      240 | 5867 | `	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);` |
|      240 | 5868 | `	pTos = sState.pTos;` |
|      240 | 5869 | `	pc = sState.pc;` |
|      240 | 5870 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5871 | `		goto Abort;` |
|      240 | 5872 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        6 | 5873 | `		goto Exception;` |
|        - | 5874 | `	}` |
|      236 | 5875 | `	break;` |
|        - | 5876 | `					  }` |
|        - | 5877 | `/*` |
|        - | 5878 | ` * OP_YIELD P1 P2 *` |
|        - | 5879 | ` *  Yield a value from a generator function.` |
|        - | 5880 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - | 5881 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - | 5882 | ` */` |
|      761 | 5883 | `case PH7_OP_YIELD: {` |
|        - | 5884 | `	ph7_generator *pGen;` |
|     1527 | 5885 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5886 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 | 5887 | `		goto Abort;` |
|        - | 5888 | `	}` |
|     1527 | 5889 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5890 | ``		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's`` |
|        - | 5891 | `		 * pending finallys tried to yield — PHP forbids it. */` |
|      ! 0 | 5892 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5893 | `			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5894 | `			goto Abort;` |
|        - | 5895 | `		}` |
|      ! 0 | 5896 | `		goto Exception;` |
|        - | 5897 | `	}` |
|     1527 | 5898 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|     1527 | 5899 | `	if( pInstr->iP2 ){` |
|        - | 5900 | `		/* yield $key => $value: value on top, key below */` |
|        - | 5901 | `#ifdef UNTRUST` |
|        - | 5902 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - | 5903 | `#endif` |
|      166 | 5904 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|      166 | 5905 | `		VmPopOperand(&pTos, 1);` |
|      166 | 5906 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|      166 | 5907 | `		VmPopOperand(&pTos, 1);` |
|        - | 5908 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|      166 | 5909 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|       29 | 5910 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|       29 | 5911 | `			if( nKey >= pGen->iImplicitKey ){` |
|       29 | 5912 | `				pGen->iImplicitKey = nKey + 1;` |
|       13 | 5913 | `			}` |
|       17 | 5914 | `		}` |
|     1446 | 5915 | `	}else if( pInstr->iP1 ){` |
|        - | 5916 | `		/* yield $value */` |
|        - | 5917 | `#ifdef UNTRUST` |
|        - | 5918 | `		if( pTos < pStack ) goto Abort;` |
|        - | 5919 | `#endif` |
|     1363 | 5920 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|     1363 | 5921 | `		VmPopOperand(&pTos, 1);` |
|        - | 5922 | `		/* Auto-increment key */` |
|     1363 | 5923 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|     1363 | 5924 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|     1363 | 5925 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|      684 | 5926 | `	}else{` |
|        - | 5927 | `		/* Bare yield — null value, auto-increment key */` |
|        3 | 5928 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|        3 | 5929 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|        3 | 5930 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|        3 | 5931 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - | 5932 | `	}` |
|        - | 5933 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|     1527 | 5934 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|     1527 | 5935 | `	goto Suspend;` |
|        - | 5936 | `}` |
|        - | 5937 | `/*` |
|        - | 5938 | ` * OP_YIELD_FROM * * *` |
|        - | 5939 | ` *` |
|        - | 5940 | `` * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an`` |
|        - | 5941 | ` * array/Traversable/Generator from the OUTER generator, preserving the inner` |
|        - | 5942 | ` * keys; the expression evaluates to the inner Generator's return value (or NULL).` |
|        - | 5943 | ` *` |
|        - | 5944 | ` * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,` |
|        - | 5945 | ` * advances the per-instance delegate cursor stored on the exec context (never the` |
|        - | 5946 | ` * shared foreach aStep, so independent generator instances cannot clash). The` |
|        - | 5947 | ` * iterable operand is consumed on first entry; the expression result is pushed at` |
|        - | 5948 | ` * exhaustion — net stack effect +1, identical to OP_YIELD.` |
|        - | 5949 | ` */` |
|      111 | 5950 | `case PH7_OP_YIELD_FROM: {` |
|        - | 5951 | `	ph7_generator *pGenFrom;` |
|        - | 5952 | `	ph7_exec_ctx *pCtxFrom;` |
|        - | 5953 | `	ph7_value sKey,sVal;` |
|      227 | 5954 | `	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */` |
|      227 | 5955 | `	int bExhausted = 0;` |
|      227 | 5956 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5957 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");` |
|      ! 0 | 5958 | `		goto Abort;` |
|        - | 5959 | `	}` |
|      227 | 5960 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5961 | ``		/* A `yield from` reached while VmCloseCtx force-drives this destroyed`` |
|        - | 5962 | `		 * generator's pending finallys — PHP forbids it (distinct message from a` |
|        - | 5963 | ``		 * bare `yield`, mirroring the OP_YIELD guard above). */`` |
|      ! 0 | 5964 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5965 | `			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5966 | `			goto Abort;` |
|        - | 5967 | `		}` |
|      ! 0 | 5968 | `		goto Exception;` |
|        - | 5969 | `	}` |
|      227 | 5970 | `	pCtxFrom = pVm->pActiveCtx;` |
|      227 | 5971 | `	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;` |
|      227 | 5972 | `	PH7_MemObjInit(pVm,&sKey);` |
|      227 | 5973 | `	PH7_MemObjInit(pVm,&sVal);` |
|      227 | 5974 | `	if( pCtxFrom->iDelegateState == 0 ){` |
|        - | 5975 | `		/* First entry: classify the iterable on the stack top. */` |
|       97 | 5976 | `		int bIterable = 1;` |
|        - | 5977 | `#ifdef UNTRUST` |
|        - | 5978 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 5979 | `#endif` |
|       97 | 5980 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       35 | 5981 | `			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       35 | 5982 | `			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;` |
|       35 | 5983 | `			pCtxFrom->iDelegateState = 1;` |
|       82 | 5984 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       63 | 5985 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       63 | 5986 | `			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       63 | 5987 | `			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){` |
|       53 | 5988 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       53 | 5989 | `				pCtxFrom->iDelegateState = 3;` |
|       37 | 5990 | `			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){` |
|        9 | 5991 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        9 | 5992 | `				pCtxFrom->iDelegateState = 2;` |
|        6 | 5993 | `			}else{` |
|        6 | 5994 | `				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - | 5995 | `					sizeof("IteratorAggregate")-1,FALSE,0);` |
|        8 | 5996 | `				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){` |
|        - | 5997 | `					/* Delegate to the Iterator returned by getIterator() */` |
|        - | 5998 | `					ph7_value sIt;` |
|        6 | 5999 | `					PH7_MemObjInit(pVm,&sIt);` |
|        6 | 6000 | `					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);` |
|        6 | 6001 | `					if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){` |
|        - | 6002 | `						/* getIterator() threw/aborted: drop it, consume the` |
|        - | 6003 | `						 * operand, and propagate. */` |
|      ! 0 | 6004 | `						PH7_MemObjRelease(&sIt);` |
|      ! 0 | 6005 | `						VmPopOperand(&pTos,1);` |
|      ! 0 | 6006 | `						goto yf_propagate;` |
|        - | 6007 | `					}` |
|        4 | 6008 | `					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls` |
|        6 | 6009 | `						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){` |
|        6 | 6010 | `						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);` |
|        6 | 6011 | `						pCtxFrom->iDelegateState = 2;` |
|        4 | 6012 | `					}else{` |
|      ! 0 | 6013 | `						bIterable = 0;` |
|        - | 6014 | `					}` |
|        6 | 6015 | `					PH7_MemObjRelease(&sIt);` |
|        4 | 6016 | `				}else{` |
|      ! 0 | 6017 | `					bIterable = 0;` |
|        - | 6018 | `				}` |
|        - | 6019 | `			}` |
|       34 | 6020 | `		}else{` |
|        6 | 6021 | `			bIterable = 0;` |
|        - | 6022 | `		}` |
|       97 | 6023 | `		VmPopOperand(&pTos,1); /* Consume the iterable operand */` |
|       97 | 6024 | `		if( !bIterable ){` |
|        - | 6025 | `			/* Non-iterable source: throw a catchable Error (PHP 8.5), then` |
|        - | 6026 | `			 * funnel through the shared teardown/route path. */` |
|        6 | 6027 | `			rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 6028 | `				"Can use \"yield from\" only with arrays and Traversables",` |
|        - | 6029 | `				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);` |
|        6 | 6030 | `			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        6 | 6031 | `			goto yf_propagate;` |
|        - | 6032 | `		}` |
|       93 | 6033 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|        - | 6034 | `			/* A GENERATOR delegate is not rewound: php links it as a child node and` |
|        - | 6035 | ``			 * only INITIALIZES it, so `yield from $g` over a half-consumed generator`` |
|        - | 6036 | `			 * continues from where it stands. One state it refuses outright, with` |
|        - | 6037 | `			 * its own Error rather than the traverse/rewind wording the other entry` |
|        - | 6038 | `			 * points use — a generator that has already run to its end, which PHL` |
|        - | 6039 | `			 * delegated to in silence and yielded NOTHING from. */` |
|       53 | 6040 | `			ph7_class_instance *pDel = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|       53 | 6041 | `			if( PH7_VmGeneratorIsClosed(&(*pVm),pDel) ){` |
|        3 | 6042 | `				rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 6043 | `					"Generator passed to yield from was aborted without proper return and is unable to continue",` |
|        - | 6044 | `					sizeof("Generator passed to yield from was aborted without proper return and is unable to continue")-1);` |
|        3 | 6045 | `				rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        3 | 6046 | `				goto yf_propagate;` |
|        - | 6047 | `			}` |
|       51 | 6048 | `			rcm = PH7_VmGeneratorPrime(&(*pVm),pDel);` |
|       51 | 6049 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       65 | 6050 | `		}else if( pCtxFrom->iDelegateState >= 2 ){` |
|        - | 6051 | `			/* rewind() a plain Iterator delegate */` |
|       13 | 6052 | `			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 6053 | `				"rewind",sizeof("rewind")-1,0);` |
|       13 | 6054 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 6055 | `		}` |
|       45 | 6056 | `	}else{` |
|        - | 6057 | `		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a` |
|        - | 6058 | `		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()` |
|        - | 6059 | `		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)` |
|        - | 6060 | `		 * ignore send() and just advance with next(). */` |
|        - | 6061 | `#ifdef UNTRUST` |
|        - | 6062 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 6063 | `#endif` |
|      135 | 6064 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       75 | 6065 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|        - | 6066 | `			/* A pending Generator::throw() on the outer was parked on pInjected by the` |
|        - | 6067 | `			 * body-entry gate (which skips its own raise for state 3); forward it as a` |
|        - | 6068 | `			 * throw into the delegate, else forward the sent value (pTos, which` |
|        - | 6069 | `			 * VmResumeCtx copies into the inner's own stack). */` |
|       75 | 6070 | `			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;` |
|       75 | 6071 | `			pCtxFrom->pInjected = 0;` |
|       75 | 6072 | `			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       75 | 6073 | `				if( pInjFwd ){` |
|        - | 6074 | `					/* Borrowed ref: the outer's Generator::throw() holds it across this` |
|        - | 6075 | `					 * whole resume, so the inner inject path must not unref it. */` |
|        5 | 6076 | `					pInner->pCtx->pInjected = pInjFwd;` |
|        5 | 6077 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);` |
|        5 | 6078 | `					pInner->pCtx->pInjected = 0;` |
|        3 | 6079 | `				}else{` |
|       71 | 6080 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);` |
|        5 | 6081 | `				}` |
|       35 | 6082 | `			}else if( pInjFwd ){` |
|        - | 6083 | `				/* No live delegate to receive the throw (inner already finished/closed):` |
|        - | 6084 | `				 * raise it at the yield-from in the outer generator's own frame. */` |
|      ! 0 | 6085 | `				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);` |
|      ! 0 | 6086 | `				pTF->iFlags \|= VM_FRAME_THROW;` |
|      ! 0 | 6087 | `				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 6088 | `			}` |
|       75 | 6089 | `			PH7_MemObjRelease(pTos);` |
|       75 | 6090 | `			pTos--;` |
|       75 | 6091 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       37 | 6092 | `		}else{` |
|       65 | 6093 | `			PH7_MemObjRelease(pTos);` |
|       65 | 6094 | `			pTos--;` |
|       65 | 6095 | `			if( pCtxFrom->iDelegateState >= 2 ){` |
|       17 | 6096 | `				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 6097 | `					"next",sizeof("next")-1,0);` |
|       17 | 6098 | `				if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 6099 | `			}` |
|        - | 6100 | `		}` |
|        - | 6101 | `	}` |
|        - | 6102 | `	/* Fetch the current (key,value) of the delegate, or mark exhausted. */` |
|      205 | 6103 | `	if( pCtxFrom->iDelegateState == 1 ){` |
|       81 | 6104 | `		if( pCtxFrom->pDelegateNode == 0 ){` |
|       27 | 6105 | `			bExhausted = 1;` |
|       16 | 6106 | `		}else{` |
|       59 | 6107 | `			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);` |
|       59 | 6108 | `			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);` |
|        - | 6109 | `			/* Forward traversal follows pPrev (the hashmap's "reverse link",` |
|        - | 6110 | `			 * matching PH7_HashmapGetNextEntry). */` |
|       59 | 6111 | `			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;` |
|        - | 6112 | `		}` |
|       43 | 6113 | `	}else{` |
|      129 | 6114 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|        - | 6115 | `		ph7_value sValid;` |
|        - | 6116 | `		int isValid;` |
|      129 | 6117 | `		PH7_MemObjInit(pVm,&sValid);` |
|      129 | 6118 | `		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|      129 | 6119 | `		PH7_MemObjToBool(&sValid);` |
|      129 | 6120 | `		isValid = (sValid.x.iVal != 0);` |
|      129 | 6121 | `		PH7_MemObjRelease(&sValid);` |
|      129 | 6122 | `		if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      129 | 6123 | `		if( !isValid ){` |
|       33 | 6124 | `			bExhausted = 1;` |
|       19 | 6125 | `		}else{` |
|      101 | 6126 | `			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|      101 | 6127 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      101 | 6128 | `			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|      101 | 6129 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        - | 6130 | `		}` |
|        - | 6131 | `	}` |
|      205 | 6132 | `	if( bExhausted ){` |
|        - | 6133 | `		/* Expression value: inner Generator's return value (state 3) or NULL. */` |
|        - | 6134 | `		ph7_value sResult;` |
|       55 | 6135 | `		PH7_MemObjInit(pVm,&sResult);` |
|       55 | 6136 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       29 | 6137 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|       29 | 6138 | `			if( pInner && pInner->pCtx ){` |
|       29 | 6139 | `				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);` |
|       12 | 6140 | `			}` |
|       12 | 6141 | `		}` |
|       55 | 6142 | `		PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       55 | 6143 | `		pCtxFrom->pDelegateNode = 0;` |
|       55 | 6144 | `		pCtxFrom->iDelegateState = 0;` |
|       55 | 6145 | `		pTos++;` |
|       55 | 6146 | `		PH7_MemObjStore(&sResult,pTos);` |
|       55 | 6147 | `		PH7_MemObjRelease(&sResult);` |
|       55 | 6148 | `		PH7_MemObjRelease(&sKey);` |
|       55 | 6149 | `		PH7_MemObjRelease(&sVal);` |
|       55 | 6150 | `		break; /* fall through to pc+1 with the result on the stack top */` |
|        - | 6151 | `	}` |
|        - | 6152 | `	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.` |
|        - | 6153 | `	 * The outer generator's implicit auto-key counter is NOT advanced by the` |
|        - | 6154 | ``	 * delegated keys — PHP keeps it independent across `yield from`, so a later`` |
|        - | 6155 | ``	 * plain `yield` continues from the outer's own counter. */`` |
|      155 | 6156 | `	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);` |
|      155 | 6157 | `	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);` |
|      155 | 6158 | `	PH7_MemObjRelease(&sKey);` |
|      155 | 6159 | `	PH7_MemObjRelease(&sVal);` |
|        - | 6160 | `	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */` |
|      155 | 6161 | `	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));` |
|      155 | 6162 | `	goto Suspend;` |
|       11 | 6163 | `yf_propagate:` |
|        - | 6164 | `	/* A delegate iterator method threw/aborted (or the source was non-iterable):` |
|        - | 6165 | `	 * tear down the delegation, then route via the shared dispatch macro. rcm is` |
|        - | 6166 | `	 * always PH7_EXCEPTION or PH7_ABORT here. */` |
|       27 | 6167 | `	PH7_MemObjRelease(&sKey);` |
|       27 | 6168 | `	PH7_MemObjRelease(&sVal);` |
|       27 | 6169 | `	PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       27 | 6170 | `	pCtxFrom->pDelegateNode = 0;` |
|       27 | 6171 | `	pCtxFrom->iDelegateState = 0;` |
|       27 | 6172 | `	PH7_DISPATCH_ENFORCE_RC(rcm)` |
|      ! 0 | 6173 | `	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */` |
|        - | 6174 | `}` |
|        - | 6175 | `/*` |
|        - | 6176 | ` * OP_CALL P1 * *` |
|        - | 6177 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - | 6178 | ` *  function on the stack.` |
|        - | 6179 | ` */` |
|        - | 6180 | `/*` |
|        - | 6181 | ` * OP_CALL_INIT * P2 *` |
|        - | 6182 | ` *  Screen the callee on TOS where it is WRITTEN — before this call's arguments run.` |
|        - | 6183 | ` *` |
|        - | 6184 | ` *  php resolves a call's target at INIT_FCALL / INIT_FCALL_BY_NAME / INIT_DYNAMIC_CALL` |
|        - | 6185 | `` *  and raises there, so `undefinedFn(s(1))`, `$f(s(1))` over a misspelled name and`` |
|        - | 6186 | `` *  `$v(s(1))` over an int all refuse BEFORE `s(1)` runs. PHL only ever looked at the`` |
|        - | 6187 | ` *  callee inside OP_CALL, one instruction after the whole argument list, so every one` |
|        - | 6188 | ` *  of those programs produced the argument's side effects (or its exception) first and` |
|        - | 6189 | ` *  php's Error second. The messages were already identical; only the order was not.` |
|        - | 6190 | ` *` |
|        - | 6191 | ` *  The verdict is the FIRST-CLASS-CALLABLE creation screen, unchanged and shared: php` |
|        - | 6192 | `` *  gives `f(...)` the direct call's taxonomy word for word, which makes VmFccValueError`` |
|        - | 6193 | ` *  the one builder for both. The value is left exactly as it is — OP_CALL still does its` |
|        - | 6194 | ` *  own resolution — so this adds a refusal and changes nothing that succeeds. P2 == 1` |
|        - | 6195 | ` *  when the compiler namespace-qualified the name, which is the one bit php's` |
|        - | 6196 | `` *  global-function fallback needs (an unqualified `strlen(...)` inside a namespace).`` |
|        - | 6197 | ` *` |
|        - | 6198 | ` *  Not emitted for a callee whose OP_MEMBER already screened it, for a first-class` |
|        - | 6199 | ` *  callable (OP_LOAD_FCC screens it, with nothing running in between), or for a call` |
|        - | 6200 | ` *  with no arguments at all — there the call IS the first thing that happens.` |
|        - | 6201 | ` */` |
|  2054700 | 6202 | `case PH7_OP_CALL_INIT: {` |
|        - | 6203 | `	/* This screen resolves the callee that OP_CALL is about to resolve again -- it is` |
|        - | 6204 | `	 * here only so php's Error lands before the arguments run -- and it was 4.4% of a` |
|        - | 6205 | `	 * phpcs profile. When the callee is a compile-time constant (the push behind this` |
|        - | 6206 | `	 * instruction is an OP_LOADC) the answer can only change if the set of callable` |
|        - | 6207 | `	 * NAMES changes, and that bumps pVm->nCallableGen. So a site that has passed once` |
|        - | 6208 | `	 * passes for free until something is declared. A dynamic callee is never stamped` |
|        - | 6209 | `	 * and is screened on every call, as it must be. */` |
|  4112699 | 6210 | `	if( pInstr->nAux == pVm->nCallableGen ){` |
|  3737736 | 6211 | `		break;` |
|        - | 6212 | `	}` |
|   374968 | 6213 | `	if( pInstr->iP2 & PH7_CALLINIT_CONSTRUCT ){` |
|        - | 6214 | `		/* A language construct's call: its callee is a host function hidden from every` |
|        - | 6215 | `		 * name a script can spell, so the callability screen below would refuse the` |
|        - | 6216 | `		 * engine's own dispatch. OP_CALL resolves it through the same mark. */` |
|   246079 | 6217 | `		break;` |
|        - | 6218 | `	}` |
|   128894 | 6219 | `	bCallInitStamp = 0;` |
|   128889 | 6220 | `	if( (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_MAGICCALL)) == 0` |
|   128894 | 6221 | `	 && !VmValueIsClosure(pVm,pTos) ){` |
|   122023 | 6222 | `		const char *zInitCls = 0,*zInitMeth = 0;` |
|   122023 | 6223 | `		sxu32 nInitCls = 0,nInitMeth = 0;` |
|        - | 6224 | `		char zInitMsg[192];` |
|   122023 | 6225 | `		const char *zInitBad = 0;` |
|   122023 | 6226 | `		SyString sInitName = { 0, 0 };` |
|   122023 | 6227 | `		int bInitScoped = 0;` |
|   122023 | 6228 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|   120059 | 6229 | `			SyStringInitFromBuf(&sInitName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 6230 | `			/* A leading backslash only anchors the name to the global namespace. */` |
|   120059 | 6231 | `			if( sInitName.nByte > 0 && sInitName.zString[0] == '\\' ){` |
|        6 | 6232 | `				sInitName.zString++;` |
|        6 | 6233 | `				sInitName.nByte--;` |
|        2 | 6234 | `			}` |
|   120059 | 6235 | `			bInitScoped = PH7_VmCallableStringParts(sInitName.zString,sInitName.nByte,` |
|        - | 6236 | `				&zInitCls,&nInitCls,&zInitMeth,&nInitMeth);` |
|        - | 6237 | `			/* A plain function NAME is the one verdict that depends on nothing but the` |
|        - | 6238 | ``			 * callable set. A `Class::method` string is screened for VISIBILITY too,`` |
|        - | 6239 | `			 * and a trait's body can run under more than one class, so that one is` |
|        - | 6240 | `			 * asked every time. */` |
|   120059 | 6241 | `			bCallInitStamp = !bInitScoped;` |
|    59615 | 6242 | `		}` |
|   122023 | 6243 | `		if( bInitScoped ){` |
|        - | 6244 | ``			/* A `"Class::method"` string carries its whole taxonomy in one builder — the`` |
|        - | 6245 | `			 * class, the missing/abstract/inaccessible cases and the catch-all routing —` |
|        - | 6246 | `			 * and answers 0 when the call WILL run. It is asked unconditionally because` |
|        - | 6247 | `			 * the predicate below is not the same question: is_callable() accepts a` |
|        - | 6248 | `			 * non-static method named through a class, which the direct call refuses. */` |
|       28 | 6249 | `			zInitBad = VmCallableClassMethodError(&(*pVm),` |
|        9 | 6250 | `				PH7_VmExtractClass(&(*pVm),zInitCls,nInitCls,FALSE,0),` |
|        9 | 6251 | `				zInitCls,nInitCls,zInitMeth,nInitMeth,TRUE,zInitMsg,sizeof(zInitMsg));` |
|   122014 | 6252 | `		}else if( !PH7_VmIsCallable(&(*pVm),pTos,TRUE) ){` |
|        - | 6253 | `			/* Not callable under the name as WRITTEN. php's global fallback below may` |
|        - | 6254 | `			 * still find it, and that verdict is generation-dependent like any other --` |
|        - | 6255 | `			 * so the stamp is left standing here and only a real refusal retires it,` |
|        - | 6256 | `			 * which it does by throwing before the stamp is written. Most calls in a` |
|        - | 6257 | ``			 * namespaced file (every `count()`, `is_array()`, `trim()` in phpcs) take`` |
|        - | 6258 | `			 * exactly this path, and it is the expensive one: two callability screens` |
|        - | 6259 | `			 * and a temporary value for the shortened name. */` |
|      153 | 6260 | `			int bInitOk = 0;` |
|      153 | 6261 | `			if( (pInstr->iP2 & PH7_CALLINIT_NAMESPACED) && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 6262 | `				/* php's global fallback for an UNQUALIFIED name written inside a` |
|        - | 6263 | `				 * namespace: the current namespace first, the global one after. OP_CALL` |
|        - | 6264 | `				 * retries the same way from its argument map; this only has to agree` |
|        - | 6265 | `				 * about whether the call WILL resolve, so the shortened name is tested` |
|        - | 6266 | `				 * and thrown away. */` |
|       81 | 6267 | `				const char *zInitShort = sInitName.zString;` |
|        - | 6268 | `				sxu32 iInitPos;` |
|     1297 | 6269 | `				for( iInitPos = 0 ; iInitPos < sInitName.nByte ; ++iInitPos ){` |
|     1221 | 6270 | `					if( sInitName.zString[iInitPos] == '\\' ){` |
|       95 | 6271 | `						zInitShort = &sInitName.zString[iInitPos + 1];` |
|       45 | 6272 | `					}` |
|      613 | 6273 | `				}` |
|       81 | 6274 | `				if( zInitShort != sInitName.zString ){` |
|        - | 6275 | `					ph7_value sInitShort;` |
|       81 | 6276 | `					PH7_MemObjInit(pVm,&sInitShort);` |
|      119 | 6277 | `					PH7_MemObjStringAppend(&sInitShort,zInitShort,` |
|       76 | 6278 | `						(sxu32)(sInitName.nByte - (sxu32)(zInitShort - sInitName.zString)));` |
|       81 | 6279 | `					bInitOk = PH7_VmIsCallable(&(*pVm),&sInitShort,TRUE);` |
|       81 | 6280 | `					PH7_MemObjRelease(&sInitShort);` |
|       38 | 6281 | `				}` |
|       38 | 6282 | `			}` |
|        - | 6283 | `			/* The FIRST-CLASS-CALLABLE creation screen's builder, unchanged and shared:` |
|        - | 6284 | ``			 * php gives `f(...)` the direct call's taxonomy word for word. It assumes the`` |
|        - | 6285 | `			 * predicate has already declined — a pair a class answers through __call is` |
|        - | 6286 | `			 * callable and never arrives here — which is why it sits under that test. */` |
|      153 | 6287 | `			if( !bInitOk ){` |
|       76 | 6288 | `				zInitBad = VmFccValueError(&(*pVm),pTos,zInitMsg,sizeof(zInitMsg));` |
|       36 | 6289 | `			}` |
|       74 | 6290 | `		}` |
|   122023 | 6291 | `		if( zInitBad ){` |
|        - | 6292 | `			sxi32 rcInit;` |
|       82 | 6293 | `			PH7_MemObjRelease(pTos);` |
|       82 | 6294 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       82 | 6295 | `			pTos->nIdx = SXU32_HIGH;` |
|       82 | 6296 | `			rcInit = VmThrowFromVm(&(*pVm),"Error",zInitBad,(sxu32)SyStrlen(zInitBad));` |
|       82 | 6297 | `			if( rcInit == SXERR_ABORT ){ goto Abort; }` |
|       82 | 6298 | `			rc = rcInit;` |
|      116 | 6299 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6300 | `		}` |
|    60558 | 6301 | `	}` |
|   128816 | 6302 | `	if( bCallInitStamp && pc > 0 && aInstr[pc-1].iOp == PH7_OP_LOADC ){` |
|        - | 6303 | `		/* The callee is the same literal every time this site runs, so record that it` |
|        - | 6304 | `` 		 * was screened -- and at WHICH generation, because a later `function f(){}` `` |
|        - | 6305 | `		 * (or a class, or an unregistered host function) can change the answer. */` |
|   118993 | 6306 | `		pInstr->nAux = pVm->nCallableGen;` |
|    59082 | 6307 | `	}` |
|   128816 | 6308 | `	break;` |
|        - | 6309 | `}` |
|        - | 6310 | `/*` |
|        - | 6311 | ` * OP_ROT_CALLEE P1 P2 *` |
|        - | 6312 | ` *  Turn a call's operand region over: [callee][arg0..argN] becomes [arg0..argN][callee],` |
|        - | 6313 | ` *  which is the layout OP_CALL's entire dispatch is written against.` |
|        - | 6314 | ` *` |
|        - | 6315 | ` *  The codegen pushes the callee FIRST because php resolves it where it is written —` |
|        - | 6316 | ` *  before a single argument runs — so an undefined or inaccessible method is refused` |
|        - | 6317 | `` *  ahead of the argument list's side effects, and a `?->` on null skips the arguments`` |
|        - | 6318 | ` *  altogether. Everything downstream of this instruction still sees the historical` |
|        - | 6319 | ` *  stack, so the reordering costs one memory move per call and nothing else.` |
|        - | 6320 | ` *` |
|        - | 6321 | ` *  P1 is the compile-time argument count; P2 carries PH7_ROT_SPREAD (this call unpacks,` |
|        - | 6322 | ` *  so the runtime count is P1 plus its OWN runs' net growth) and PH7_ROT_TWOSLOT (the` |
|        - | 6323 | ` *  callee is a method pair, [receiver][name]). A __call routing collapses that pair to` |
|        - | 6324 | ` *  one marked carrier at run time, which is read off the slot rather than guessed.` |
|        - | 6325 | ` */` |
|  2573172 | 6326 | `case PH7_OP_ROT_CALLEE: {` |
| 10299195 | 6327 | `	sxi32 nRotArgs = pInstr->iP1` |
|  5149595 | 6328 | `		+ ((pInstr->iP2 & PH7_ROT_SPREAD)` |
|        - | 6329 | `			/* One past the last argument is one past the TOP here: the callee sits` |
|        - | 6330 | `			 * BELOW the region, not above it as at OP_CALL. */` |
|  2573763 | 6331 | `			? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,&pTos[1]) : 0);` |
|  5149600 | 6332 | `	if( nRotArgs < 0 ){` |
|        - | 6333 | `		/* Unreachable: an empty unpack subtracts one per compile-time position, so the` |
|        - | 6334 | `		 * net can reach 0 and no lower. Clamped rather than trusted — reading above the` |
|        - | 6335 | `		 * top to find the callee is not a failure mode worth leaving open. */` |
|      ! 0 | 6336 | `		nRotArgs = 0;` |
|      ! 0 | 6337 | `	}` |
|        - | 6338 | `	{` |
|        - | 6339 | `		ph7_value aCallee[2];` |
|  5149600 | 6340 | `		ph7_value *pTopCallee = &pTos[-nRotArgs];` |
|  5149600 | 6341 | `		sxi32 nCallee = (pInstr->iP2 & PH7_ROT_TWOSLOT) ? 2 : 1;` |
|        - | 6342 | `		ph7_value *pBase;` |
|        - | 6343 | `		sxi32 i;` |
|  5149600 | 6344 | `		if( nCallee > 1 && (pTopCallee->iFlags & MEMOBJ_AUX_MAGICCALL) ){` |
|        - | 6345 | `			/* OP_MEMBER routed a missing/inaccessible name to __call: it consumed the` |
|        - | 6346 | `			 * receiver and left ONE carrier slot, so the pair the compiler counted on` |
|        - | 6347 | `			 * is not there. */` |
|       71 | 6348 | `			nCallee = 1;` |
|       34 | 6349 | `		}` |
|  5149600 | 6350 | `		pBase = pTopCallee - (nCallee - 1);` |
|        - | 6351 | `#ifdef UNTRUST` |
|        - | 6352 | `		if( pBase < pStack ){` |
|        - | 6353 | `			goto Abort;` |
|        - | 6354 | `		}` |
|        - | 6355 | `#endif` |
|  5149600 | 6356 | `		if( nRotArgs > 0 ){` |
| 10324113 | 6357 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  5174552 | 6358 | `				aCallee[i] = pBase[i];` |
|  2588891 | 6359 | `			}` |
| 13990033 | 6360 | `			for( i = 0 ; i < nRotArgs ; ++i ){` |
|  8840472 | 6361 | `				pBase[i] = pBase[i + nCallee];` |
|  4421841 | 6362 | `			}` |
| 10324113 | 6363 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  5174552 | 6364 | `				pBase[nRotArgs + i] = aCallee[i];` |
|  2588891 | 6365 | `			}` |
|  2576406 | 6366 | `		}` |
|  5149600 | 6367 | `		if( pInstr->iP2 & PH7_ROT_SPREAD ){` |
|        - | 6368 | `			/* The argument region now ends nCallee slots lower than it did, so this` |
|        - | 6369 | `			 * call's captured unpack runs — the suffix VmSpreadOwnExtra just assigned` |
|        - | 6370 | `			 * to it, all of them anchored inside the region — move with it, and OP_CALL` |
|        - | 6371 | ``			 * re-derives the same count from them. Unconditional: an `f(...[])` unpack`` |
|        - | 6372 | `			 * moves NO argument (its run is zero-width) and still has to be re-anchored,` |
|        - | 6373 | `			 * or the recount reads it as an ordinary slot and eats one slot too many.` |
|        - | 6374 | `			 * An ENCLOSING call's runs sit below the callee and are left alone. */` |
|     1282 | 6375 | `			sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|     1282 | 6376 | `			VmSpreadRun *aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|        - | 6377 | `			sxu32 r;` |
|     2575 | 6378 | `			for( r = pVm->nSpreadCallBase ; r < nRun ; ++r ){` |
|     1298 | 6379 | `				aRun[r].pStart -= nCallee;` |
|      604 | 6380 | `			}` |
|      591 | 6381 | `		}` |
|        - | 6382 | `	}` |
|  5149600 | 6383 | `	break;` |
|        - | 6384 | `}` |
|  4004078 | 6385 | `case PH7_OP_CALL: {` |
|        - | 6386 | `	/* iP2 is a compile-time bit SET (PH7_CALL_*). PH7_CALL_SPREAD: count only THIS` |
|        - | 6387 | `	 * call's own unpack expansion (VmSpreadOwnExtra, derived from the captured runs on` |
|        - | 6388 | `	 * top of the stack) — an INNER spread-bearing call evaluated inside this argument` |
|        - | 6389 | ``	 * list (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not`` |
|        - | 6390 | `	 * be conflated, which a single shared accumulator could not express. */` |
| 16021463 | 6391 | `	sxi32 nCallArgs = pInstr->iP1` |
|  8010729 | 6392 | `		+ ((pInstr->iP2 & PH7_CALL_SPREAD) ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|        - | 6393 | `	ph7_value *pArg;` |
|        - | 6394 | `	/* Consume the engine's magic-dispatch latch here, at the head of the ONE call` |
|        - | 6395 | `	 * it was set for, whatever path that call then takes. Left standing it would` |
|        - | 6396 | `	 * describe the next call instead. */` |
|  8010734 | 6397 | `	int bMagicDispatch = pVm->bMagicDispatch;` |
|        - | 6398 | `	/* ...and the member resolution's own verdict, which rides the callee SLOT rather` |
|        - | 6399 | `	 * than the VM: an OP_MEMBER that produced this callee already decided its` |
|        - | 6400 | `	 * visibility against the entry it chose, so the screen below must stand down. */` |
| 15940552 | 6401 | `	int bMemberScreened = (pTos->iFlags & MEMOBJ_AUX_MEMBERCALL) != 0` |
|  8010729 | 6402 | `		\|\| pVm->bClosureScreened;` |
|        - | 6403 | `	/* ...and whether the NAME in that slot is one of the engine's own function-table` |
|        - | 6404 | `	 * keys rather than something the program spelled: an OP_MEMBER method resolution` |
|        - | 6405 | ``	 * pushes the method's `sVmName` (`[__Class@meth_xxxxxxxxxx]`), and so do the two`` |
|        - | 6406 | `	 * synthetic call builders. Read here because the member mark is cleared just` |
|        - | 6407 | `	 * below; the closure branch adds its own case further down. It is what lets` |
|        - | 6408 | `	 * PH7_VmGetUserFunction refuse those keys to a SCRIPT that spells one. */` |
|  8010734 | 6409 | `	int bEngineCallee = (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_ENGINEFN)) != 0;` |
|        - | 6410 | `	/* ...and the internal-callback latch, for the same reason: it describes THIS call` |
|        - | 6411 | `	 * (an internal function invoking a userland callback binds its arguments weakly),` |
|        - | 6412 | `	 * and a call the callback body makes must not inherit it. */` |
|  8010734 | 6413 | `	int bCallbackWeak = pVm->bCallbackWeak;` |
|        - | 6414 | `	/* ...and whether this call's ANSWER is thrown away, which is what a` |
|        - | 6415 | `	 * #[\NoDiscard] callee warns about. The compiled call site says so directly` |
|        - | 6416 | `	 * (bDiscard, stamped where the statement pops the value: php's` |
|        - | 6417 | `	 * !RETURN_VALUE_USED); php also propagates the same bit THROUGH` |
|        - | 6418 | ``	 * `call_user_func()`/`call_user_func_array()`, which is why`` |
|        - | 6419 | ``	 * `call_user_func('f');` warns and `array_map('f', $a);` does not, and a`` |
|        - | 6420 | `	 * synthetic OP_CALL has no call site of its own to carry it -- so the two` |
|        - | 6421 | `	 * builtins hand it over on this latch. Consumed here like the rest. */` |
|  8010734 | 6422 | `	int bResultDropped = pVm->bDiscardCallback ? 1 : (pInstr->bDiscard != 0);` |
|        - | 6423 | `	/* php's forwarding of that bit through call_user_func() is a COMPILE-time` |
|        - | 6424 | ``	 * special case on the literal name, so `$g = "call_user_func"; $g("f");` does`` |
|        - | 6425 | `	 * not forward. The callee slot's nIdx says which spelling this is: a compiled` |
|        - | 6426 | `	 * literal is marked constant, a variable read is not. */` |
|  8010734 | 6427 | `	int bLiteralCallee = (pTos->nIdx == SXU32_HIGH);` |
|        - | 6428 | ``	/* php ELIDES a `call_user_func()`/`call_user_func_array()` frame only when its`` |
|        - | 6429 | `	 * compiler can prove the name is the global function -- written in the global` |
|        - | 6430 | `	 * namespace, or fully qualified, or imported. An UNQUALIFIED call inside a` |
|        - | 6431 | `	 * namespace could still resolve to a namespace-local function, so php cannot` |
|        - | 6432 | `	 * fold it and emits the real internal frame: the callback then has no userland` |
|        - | 6433 | ``	 * caller, exactly as under array_map. `bIsNamespaced` is that compile-time`` |
|        - | 6434 | `	 * question, already recorded on the call's argument map for the global-fallback` |
|        - | 6435 | `	 * resolution. (Every probe behind this rule had been written in the global` |
|        - | 6436 | `	 * namespace, which is why the forwards looked unconditionally elided; pest calls` |
|        - | 6437 | ``	 * one from inside `namespace Pest\Concerns`.) */`` |
|  8010734 | 6438 | `	int bNsCallee = (pInstr->p3 && ((VmCallArgMap *)pInstr->p3)->bIsNamespaced) ? 1 : 0;` |
|        - | 6439 | `	/* php's ZEND_CALL_DYNAMIC, decided the way php's compiler decides it: a compiled` |
|        - | 6440 | ``	 * call is dynamic when its callee is not a literal name (`$n()`, `$c()`, an array`` |
|        - | 6441 | `	 * pair; the Closure branch below adds its own case, a temporary holds no slot` |
|        - | 6442 | `	 * index to read). A SYNTHETIC call is one the C dispatcher built, and php's` |
|        - | 6443 | `	 * zend_call_function always marks those -- except for the one forward its` |
|        - | 6444 | ``	 * compiler folded into a direct call (`call_user_func('f', ...)` on a literal,`` |
|        - | 6445 | `	 * global name), which reaches here with NEITHER latch armed. Read by the host` |
|        - | 6446 | `	 * function's context (PH7_CTX_CALL_DYNAMIC): the six functions that answer from` |
|        - | 6447 | `	 * their caller's frame refuse it. */` |
| 13520995 | 6448 | `	int bDynamicCall = (pInstr->nLine == 0)` |
|  3012633 | 6449 | `		? (bCallbackWeak \|\| pVm->pNativeFrameName != 0 \|\| pVm->bDynamicForward)` |
|  8010996 | 6450 | `		: !bLiteralCallee;` |
|  8010734 | 6451 | `	pVm->bDynamicForward = 0;` |
|  8010734 | 6452 | `	pVm->bDiscardCallback = 0;` |
|  8010734 | 6453 | `	pVm->bMagicDispatch = 0;` |
|  8010734 | 6454 | `	pVm->bClosureScreened = 0;` |
|  8010734 | 6455 | `	pVm->bCallbackWeak = 0;` |
|  8010734 | 6456 | `	pTos->iFlags &= ~MEMOBJ_AUX_MEMBERCALL;` |
|  8010734 | 6457 | `	pArg = &pTos[-nCallArgs];` |
|        - | 6458 | `	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED` |
|        - | 6459 | `	 * arguments, and a spread expanding to !=1 element shifts the actual positions` |
|        - | 6460 | `	 * of any following compile-time named args. The effective per-actual-slot name` |
|        - | 6461 | `	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)` |
|        - | 6462 | `	 * is built PER PATH against that path's finalized arg base — a method call pops` |
|        - | 6463 | `	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch` |
|        - | 6464 | `	 * site rather than built once here. */` |
|        - | 6465 | `	VmCallArgMap sEffMap;` |
|  8010734 | 6466 | `	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;` |
|        - | 6467 | `	SyHashEntry *pEntry;` |
|        - | 6468 | `	/* What this call SITE remembered about its callee, consulted once and used by` |
|        - | 6469 | `	 * whichever of the two dispatch branches the answer belongs to. */` |
|  8010734 | 6470 | `	SyHashEntry *pSiteEntry = 0;` |
|  8010734 | 6471 | `	int bSiteHost = 0;` |
|        - | 6472 | `	SyString sName;` |
|        - | 6473 | `	/* The host-call trio, hoisted out of the foreign-function branch below so the` |
|        - | 6474 | `	 * NativeCall label there can also be reached from the compiled-function branch:` |
|        - | 6475 | `	 * a VM_FUNC_NATIVE method is a C body wearing a method's clothes, and it runs on` |
|        - | 6476 | `	 * exactly the foreign path (no frame, no call record, no operand stack) once` |
|        - | 6477 | `	 * $this and the called class have been resolved the method way. Jumping into` |
|        - | 6478 | `	 * that branch would otherwise cross these declarations. */` |
|        - | 6479 | `	ph7_user_func *pFunc;` |
|        - | 6480 | `	ph7_context sCtx;` |
|        - | 6481 | `	ph7_value sRet;` |
|        - | 6482 | `	/* Native-METHOD call state; all 0 for a plain host function, which is what the` |
|        - | 6483 | `	 * foreign branch's own fallthrough leaves.` |
|        - | 6484 | `	 *   pNativeOwned — the reference the method path took, released at the shared tail` |
|        - | 6485 | `	 *   pNativeRecv  — what the body actually sees as $this (0 for a static method)` |
|        - | 6486 | `	 *   pNativeClass — the late-static-binding target */` |
|  8010734 | 6487 | `	ph7_class_instance *pNativeOwned = 0;` |
|  8010734 | 6488 | `	ph7_class_instance *pNativeRecv = 0;` |
|  8010734 | 6489 | `	ph7_class *pNativeClass = 0;` |
|        - | 6490 | `	/*   pNativeMethod / pNativeDeclClass — the method record and the class that` |
|        - | 6491 | ``	 *   DECLARES it, for the `class`/`type` keys of the trace frame php gives an`` |
|        - | 6492 | `	 *   internal call. Left 0 by the plain-builtin fallthrough, whose frame carries` |
|        - | 6493 | `	 *   neither key. */` |
|  8010734 | 6494 | `	ph7_vm_func *pNativeMethod = 0;` |
|  8010734 | 6495 | `	ph7_class *pNativeDeclClass = 0;` |
|        - | 6496 | `	/* php's compiler REWRITES a handful of calls into dedicated opcodes, and a` |
|        - | 6497 | ``	 * refusal raised by the opcode has no internal frame to name: `strlen($a)` on an`` |
|        - | 6498 | ``	 * array reports the CALLER as frame #0 where `str_repeat($a,2)` reports`` |
|        - | 6499 | `	 * str_repeat. Which names fold is php's own list (ZEND_STRLEN, ZEND_COUNT,` |
|        - | 6500 | `	 * ZEND_ARRAY_KEY_EXISTS, ZEND_GET_CLASS); the fold needs the exact arity and a` |
|        - | 6501 | ``	 * literal callee, so a dynamic name, a spread or a `name:` argument all take the`` |
|        - | 6502 | `	 * ordinary path and DO carry the frame. */` |
|  8010734 | 6503 | `	int bFoldedCallee = 0;` |
|        - | 6504 | `	/* The engine's own __call/__callStatic routing: the OP_MEMBER immediately below this` |
|        - | 6505 | `	 * call found a missing (or inaccessible) method on a class declaring the magic handler` |
|        - | 6506 | `	 * and MARKED this callee slot, latching {receiver, class, original name} on the VM.` |
|        - | 6507 | `	 * There is no callable here at all — the mark selects the packing body directly, ahead` |
|        - | 6508 | `	 * of every callable decode below, and the record it dispatches carries no PHP name (it` |
|        - | 6509 | `	 * is not in hHostFunction). This is what replaced writing the string` |
|        - | 6510 | `	 * "__phl_magic_call" into the slot and letting the name lookup find a hidden global.` |
|        - | 6511 | ``	 * Everything from `NativeCall` down is shared with an ordinary builtin call, which is`` |
|        - | 6512 | `	 * what this has always been from the executor's point of view. */` |
|  8010734 | 6513 | `	if( pTos->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|        - | 6514 | `		/* Move the routing off the carrier and onto the VM, HERE — one instruction` |
|        - | 6515 | `		 * before the packing body reads it, with nothing in between that could set` |
|        - | 6516 | `		 * another. OP_MEMBER used to publish it directly, which only held while the` |
|        - | 6517 | `		 * arguments ran before it; now they run after, and a routed call inside this` |
|        - | 6518 | `		 * one's argument list has already come and gone. */` |
|      130 | 6519 | `		VmMagicCall *pPend = (VmMagicCall *)pTos->x.pOther;` |
|      130 | 6520 | `		pTos->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|      130 | 6521 | `		pTos->x.pOther = 0;` |
|      130 | 6522 | `		pVm->pMagicCallThis = pPend ? pPend->pRecv : 0;` |
|      130 | 6523 | `		pVm->pMagicCallClass = pPend ? pPend->pClass : 0;` |
|      130 | 6524 | `		SyBlobReset(&pVm->sMagicCallName);` |
|      130 | 6525 | `		if( pPend && SyBlobLength(&pPend->sName) > 0 ){` |
|      193 | 6526 | `			SyBlobAppend(&pVm->sMagicCallName,SyBlobData(&pPend->sName),` |
|       63 | 6527 | `				SyBlobLength(&pPend->sName));` |
|       63 | 6528 | `		}` |
|      130 | 6529 | `		if( pPend ){` |
|        - | 6530 | `			/* The receiver reference the record held is now the VM's, which` |
|        - | 6531 | `			 * VmMagicCallDispatch gives back — so drop the record without unref'ing. */` |
|      130 | 6532 | `			pPend->pRecv = 0;` |
|      130 | 6533 | `			VmFreeMagicCall(pPend);` |
|       63 | 6534 | `		}` |
|      130 | 6535 | `		pFunc = PH7_VmMagicCallFunc(&(*pVm));` |
|      130 | 6536 | `		if( pFunc == 0 ){` |
|      ! 0 | 6537 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 6538 | `			goto Abort;` |
|        - | 6539 | `		}` |
|        - | 6540 | `		/* D1: the packing body declares no by-ref parameter (php hands __call a packed` |
|        - | 6541 | `		 * ARRAY), so every deferred argument materializes by value, exactly as it did` |
|        - | 6542 | `		 * through the named trampoline's zero by-ref mask. */` |
|        - | 6543 | `		{` |
|      130 | 6544 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffCallMap);` |
|      130 | 6545 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6546 | `		}` |
|      193 | 6547 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|      126 | 6548 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|      130 | 6549 | `		goto NativeCall;` |
|        - | 6550 | `	}` |
|        - | 6551 | `	/* A Closure object is callable: unwrap it to its underlying string callable so the` |
|        - | 6552 | `	 * dispatch below handles it (rather than treating it as a generic object and looking` |
|        - | 6553 | `	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure` |
|        - | 6554 | `	 * so a plain __invoke object skips the temp-value work entirely. */` |
|  8010608 | 6555 | `	if( VmValueIsClosure(pVm,pTos) ){` |
|        - | 6556 | `		ph7_value sCallable;` |
|    21622 | 6557 | `		PH7_MemObjInit(pVm,&sCallable);` |
|    21622 | 6558 | `		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){` |
|    21622 | 6559 | `			PH7_MemObjRelease(pTos);` |
|    21622 | 6560 | `			PH7_MemObjStore(&sCallable,pTos);` |
|        - | 6561 | `` 			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]` `` |
|        - | 6562 | `			 * name, which the lookup below refuses to a name a SCRIPT spelled. */` |
|    21622 | 6563 | `			bEngineCallee = 1;` |
|        - | 6564 | `			/* ...and a call THROUGH a Closure is dynamic whatever it wraps: php marks` |
|        - | 6565 | ``			 * a fake closure's invocation too (`compact(...)` then `$c('a')`). */`` |
|    21622 | 6566 | `			bDynamicCall = 1;` |
|    10649 | 6567 | `		}` |
|    21622 | 6568 | `		PH7_MemObjRelease(&sCallable);` |
|    10649 | 6569 | `	}` |
|        - | 6570 | `	/* Extract function name */` |
|  8010608 | 6571 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|   304275 | 6572 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 6573 | `			ph7_value sResult;` |
|        - | 6574 | `			sxi32 rcArr;` |
|        - | 6575 | `			/* Taken off the VM at the head of the shape check below, not at the dispatch:` |
|        - | 6576 | `			 * everything between the two (the deferred-argument materialization especially)` |
|        - | 6577 | `			 * can throw and jump out of this branch, and a latch left armed would stand the` |
|        - | 6578 | `			 * visibility screen down for whatever call runs next. */` |
|        - | 6579 | `			int bCbScreened;` |
|        - | 6580 | `			{` |
|        - | 6581 | `				/* php validates the SHAPE of an array callable first: it must hold exactly` |
|        - | 6582 | `				 * two elements. PH7 handed any array to the dispatcher, which failed` |
|        - | 6583 | ``				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */`` |
|   100355 | 6584 | `				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - | 6585 | `				char zCbMsg[192];` |
|   100355 | 6586 | `				const char *zCbErr = 0;` |
|   100355 | 6587 | `				int bCbRaised = 0; /* the class lookup ran an autoloader that threw */` |
|        - | 6588 | `				/* A pair the closure UNWRAP just built is not an array the program wrote: its` |
|        - | 6589 | `				 * callee was resolved and screened where the closure was BUILT, the way php` |
|        - | 6590 | `				 * resolves one, and it is a well-formed [target, method] by construction.` |
|        - | 6591 | `				 * Re-deciding it here, against the CALLER, is what refused an escaped` |
|        - | 6592 | ``				 * `$this->priv(...)` php runs. */`` |
|   100355 | 6593 | `				bCbScreened = pVm->bClosureScreened;` |
|   100355 | 6594 | `				pVm->bClosureScreened = 0; /* put back for the one dispatch that reads it */` |
|   100355 | 6595 | `				if( !bCbScreened && pCbMap && pCbMap->nEntry == 2 ){` |
|        - | 6596 | `					/* Shape is right; now check it actually RESOLVES. The shared dispatcher` |
|        - | 6597 | `					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for` |
|        - | 6598 | `					 * an unresolvable [class,method] pair -- silence a caller cannot detect --` |
|        - | 6599 | `					 * and its contract is relied on by call_user_func/usort, so the throw` |
|        - | 6600 | `					 * belongs here at the call site. */` |
|   100204 | 6601 | `					ph7_value *pCbCls = 0;` |
|   100204 | 6602 | `					ph7_value *pCbMeth = 0;` |
|        - | 6603 | `					/* php reads the INTEGER indices 0 and 1, not the first two entries in` |
|        - | 6604 | `					 * insertion order. Two entries at other keys are a SHAPE error of their` |
|        - | 6605 | `					 * own ("has to contain indices 0 and 1"), distinct from the` |
|        - | 6606 | ``					 * wrong-element-COUNT message below; `[1=>'m',0=>'C']` holds both, so`` |
|        - | 6607 | `					 * the target is index 0 -- the reverse of what PH7 walked. */` |
|   100204 | 6608 | `					if( !PH7_VmArrayCallableParts(&(*pVm),pCbMap,&pCbCls,&pCbMeth) ){` |
|       11 | 6609 | `						zCbErr = "Array callback has to contain indices 0 and 1";` |
|        6 | 6610 | `					}else{` |
|        - | 6611 | `						/* The resolution below can run an autoloader that throws; when it does,` |
|        - | 6612 | `						 * php propagates THAT exception and never reports the class missing. */` |
|   100194 | 6613 | `						sxi32 nCbBrc = pVm->nBoundaryRc;` |
|   100194 | 6614 | `						const void *pCbRes = (const void *)pVm->pResumeFrame;` |
|   150289 | 6615 | `						zCbErr = VmDirectArrayCallableError(&(*pVm),pCbCls,pCbMeth,` |
|    50095 | 6616 | `							zCbMsg,sizeof(zCbMsg));` |
|   100194 | 6617 | `						if( zCbErr && PH7_VmClassLookupRaised(&(*pVm),nCbBrc,pCbRes) ){` |
|        6 | 6618 | `							bCbRaised = 1;` |
|        2 | 6619 | `						}` |
|        - | 6620 | `					}` |
|    50100 | 6621 | `				}` |
|   100355 | 6622 | `				if( !bCbScreened && (pCbMap == 0 \|\| pCbMap->nEntry != 2 \|\| zCbErr) ){` |
|        - | 6623 | `					sxi32 rcCb;` |
|       87 | 6624 | `					if( pInstr->iP2 & PH7_CALL_SPREAD ){` |
|      ! 0 | 6625 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 6626 | `					}` |
|       87 | 6627 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 6628 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6629 | `					}` |
|       87 | 6630 | `					PH7_MemObjRelease(pTos);` |
|       87 | 6631 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       87 | 6632 | `					pTos->nIdx = SXU32_HIGH;` |
|       87 | 6633 | `					if( bCbRaised ){` |
|        - | 6634 | `						/* Land the autoloader's own throw: consume the parked status (an` |
|        - | 6635 | `						 * in-place catch leaves 0 behind plus a recorded resume frame, which` |
|        - | 6636 | `						 * the router below picks up). */` |
|        6 | 6637 | `						rcCb = pVm->nBoundaryRc;` |
|        6 | 6638 | `						pVm->nBoundaryRc = 0;` |
|        6 | 6639 | `						if( rcCb == PH7_ABORT ){ goto Abort; }` |
|        6 | 6640 | `						rc = PH7_EXCEPTION;` |
|       18 | 6641 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6642 | `					}` |
|       82 | 6643 | `					if( zCbErr == 0 ){` |
|       17 | 6644 | `						zCbErr = "Array callback must have exactly two elements";` |
|        8 | 6645 | `					}` |
|       82 | 6646 | `					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));` |
|       82 | 6647 | `					if( rcCb == SXERR_ABORT ){ goto Abort; }` |
|       82 | 6648 | `					rc = rcCb;` |
|        - | 6649 | `					/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 6650 | `					 * (SXRET_OK + recorded resume) FELL THROUGH and kept dispatching` |
|        - | 6651 | `					 * the malformed callable inside the try. Route like OP_THROW. */` |
|      108 | 6652 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6653 | `				}` |
|        - | 6654 | `			}` |
|        - | 6655 | `			/* Build the effective spread-key map (and consume this call's runs)` |
|        - | 6656 | `			 * against this path's arg base (the array-callable slot isn't popped). */` |
|   150404 | 6657 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100266 | 6658 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6659 | `			/* Materialize the deferred arguments against the pair's own method (see` |
|        - | 6660 | `			 * VmIndirectCalleeFunc), not against a blanket by-ref assumption. */` |
|        - | 6661 | `			{` |
|   100271 | 6662 | `				sxi32 rcDA = VmResolveIndirectArgs(&(*pVm),pTos,pArg,pTos,pEffCallMap);` |
|   100271 | 6663 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6664 | `			}` |
|   100271 | 6665 | `			SySetReset(&aArg);` |
|   100467 | 6666 | `			while( pArg < pTos ){` |
|      199 | 6667 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      199 | 6668 | `				pArg++;` |
|        3 | 6669 | `			}` |
|   100271 | 6670 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 6671 | `			/* May be a class instance and it's static method. Forward this call's named-arg map` |
|        - | 6672 | ``			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —`` |
|        - | 6673 | `			 * mirroring the __invoke-object branch below. */` |
|   100271 | 6674 | `			pVm->bClosureScreened = bCbScreened; /* see the capture above */` |
|        - | 6675 | `			/* This call SITE is what decides the answer is dropped, and the` |
|        - | 6676 | `			 * dispatch below builds a synthetic OP_CALL that has no site of its` |
|        - | 6677 | `			 * own — hand the bit over on the latch, so a #[\NoDiscard] callee` |
|        - | 6678 | `			 * reached through an array callable, a "C::m" string or __invoke` |
|        - | 6679 | `			 * warns exactly as a directly-spelled one does. */` |
|   100271 | 6680 | `			pVm->bDiscardCallback = bResultDropped;` |
|   100271 | 6681 | `			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|        - | 6682 | `			/* Both latches are consumed by the method OP_CALL this dispatch builds;` |
|        - | 6683 | `			 * clear them here for the paths that never reach one. */` |
|   100271 | 6684 | `			pVm->bClosureScreened = 0;` |
|   100271 | 6685 | `			pVm->bDiscardCallback = 0;` |
|   100271 | 6686 | `			SySetReset(&aArg);` |
|        - | 6687 | `			/* Pop given arguments */` |
|   100271 | 6688 | `			if( nCallArgs > 0 ){` |
|      173 | 6689 | `				VmPopOperand(&pTos,nCallArgs);` |
|       85 | 6690 | `			}` |
|   100271 | 6691 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 | 6692 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 6693 | `				goto Abort;` |
|        - | 6694 | `			}` |
|   100271 | 6695 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - | 6696 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - | 6697 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - | 6698 | `				sxi32 iResumePc;` |
|   100017 | 6699 | `				PH7_MemObjRelease(&sResult);` |
|   100017 | 6700 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100007 | 6701 | `					PH7_MemObjRelease(pTos);` |
|        - | 6702 | ``					/* Drain the abandoned outer-expression operands (`1 + $cb()`)`` |
|        - | 6703 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 6704 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300007 | 6705 | `					PH7_RESUME_DRAIN()` |
|   100007 | 6706 | `					pc = iResumePc;` |
|   100007 | 6707 | `					break;` |
|        - | 6708 | `				}` |
|       11 | 6709 | `				goto Exception;` |
|        - | 6710 | `			}` |
|        - | 6711 | `			/* Copy result */` |
|      256 | 6712 | `			PH7_MemObjStore(&sResult,pTos);` |
|      256 | 6713 | `			PH7_MemObjRelease(&sResult);` |
|   204050 | 6714 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - | 6715 | ``			/* An __invoke object is a METHOD CALL wearing one slot: `$o(...)` is`` |
|        - | 6716 | ``			 * `$o->__invoke(...)` with the name resolved by the ENGINE rather than`` |
|        - | 6717 | `			 * spelled by the source. So give it the layout OP_MEMBER leaves for a` |
|        - | 6718 | `			 * spelled method call -- [args..., receiver, method vm-name] -- and let` |
|        - | 6719 | `			 * the one dispatch below run it, on the stage-2 trampoline like every` |
|        - | 6720 | `			 * other PHP->PHP call.` |
|        - | 6721 | `			 *` |
|        - | 6722 | `			 * It used to be handed to VmCallObjectInvoke, which builds a synthetic` |
|        - | 6723 | `			 * OP_CALL and a fresh VmByteCodeExec: one real C activation per call,` |
|        - | 6724 | `			 * and two user-visible consequences.` |
|        - | 6725 | `			 *` |
|        - | 6726 | `			 *   - Recursion through an invokable object died at nMaxNativeDepth` |
|        - | 6727 | ``			 *     (256) where php runs to memory: `$i()` calling `($this)()` hit`` |
|        - | 6728 | `			 *     "Maximum native nesting depth reached" at 256 and php reached` |
|        - | 6729 | `			 *     20000. A plain closure, a method and a static call were all` |
|        - | 6730 | `			 *     already iterative; only this shape was not.` |
|        - | 6731 | ``			 *   - A Fiber::suspend() reached through `$o(...)` was refused with`` |
|        - | 6732 | `			 *     "Cannot suspend across an internal call boundary", because the` |
|        - | 6733 | `			 *     fiber body's nBodyExecDepth and the suspend's nVmExecDepth no` |
|        - | 6734 | `			 *     longer matched. The trampoline PARKS a nested PHP call` |
|        - | 6735 | `			 *     (pParkedSegment); a native re-entry it cannot. That is the whole` |
|        - | 6736 | `			 *     of what stopped phpstan's FiberNodeScopeResolver, whose fiber body` |
|        - | 6737 | `			 *     calls a ClassStatementsGatherer OBJECT and suspends inside it.` |
|        - | 6738 | `			 *` |
|        - | 6739 | `			 * call_user_func(), array_map() and the rest of the C-callback doors` |
|        - | 6740 | `			 * still reach __invoke through VmCallObjectInvoke, and those ARE the` |
|        - | 6741 | `			 * internal boundaries php's fibers cross and PHL's do not (a recorded` |
|        - | 6742 | `			 * generator/fiber divergence). This changes only the dispatch a call site` |
|        - | 6743 | ``			 * SPELLS as `$o(...)`. */`` |
|   203908 | 6744 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|   203908 | 6745 | `			ph7_class_method *pInvMeth = pThis` |
|   203904 | 6746 | `				? PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) : 0;` |
|   203908 | 6747 | `			if( pInvMeth ){` |
|        - | 6748 | `				/* The compiler sized this body for a ONE-slot callee here, so the` |
|        - | 6749 | `				 * name slot is one more than it budgeted. Ask for it properly rather` |
|        - | 6750 | `				 * than spend VM_STACK_GUARD's slack: the growth is a no-op whenever` |
|        - | 6751 | `				 * the slack is there (which is every ordinary call), and the slot is` |
|        - | 6752 | `				 * released a few lines below by the method branch's own pop, so at` |
|        - | 6753 | `				 * most one is ever outstanding. */` |
|   103904 | 6754 | `				ph7_value *pInvOldBase = pStack;` |
|   155854 | 6755 | `				if( !VmGrowOperandStack(pVm,(sxu32)(pTos - pStack) + 2,` |
|        - | 6756 | `				                        &pStack,&pTos,&sState,` |
|    51950 | 6757 | `				                        pCallTop,ppBaseOwner,pnBaseCap) ){` |
|      ! 0 | 6758 | `					PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 6759 | `					goto Abort;` |
|        - | 6760 | `				}` |
|   103904 | 6761 | `				if( pStack != pInvOldBase ){` |
|        - | 6762 | `					/* The buffer moved: the watermark is a pointer INTO it. Unlike` |
|        - | 6763 | `					 * OP_SPREAD (which re-anchors at the whole capacity), keep it tight —` |
|        - | 6764 | `					 * nLiveTos is what the recycle sweep walks, and this runs on every` |
|        - | 6765 | ``					 * `$o(...)`. */`` |
|     2129 | 6766 | `					pHigh = pStack + (pHigh - pInvOldBase);` |
|     1940 | 6767 | `				}` |
|   103904 | 6768 | `				pTos++;` |
|   103904 | 6769 | `				if( pTos > pHigh ){` |
|        - | 6770 | `					/* The name slot is above this activation's high-water mark until the` |
|        - | 6771 | `					 * next fetch point raises it, and an exit between here and the pop` |
|        - | 6772 | `					 * below (an unresolvable callee, the recursion cap) would leave its` |
|        - | 6773 | `					 * blob unswept. */` |
|       46 | 6774 | `					pHigh = pTos;` |
|       21 | 6775 | `				}` |
|   103904 | 6776 | `				PH7_MemObjRelease(pTos);` |
|   155854 | 6777 | `				SyBlobAppend(&pTos->sBlob,(const void *)SyStringData(&pInvMeth->sVmName),` |
|    51950 | 6778 | `					SyStringLength(&pInvMeth->sVmName));` |
|   103904 | 6779 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - | 6780 | `				/* The engine's own function-table key, not a name the program spelled --` |
|        - | 6781 | `				 * the mark VmCallClassMethodLsb writes on its synthetic callee slot, and` |
|        - | 6782 | `				 * what lets PH7_VmGetUserFunction answer for it. */` |
|   103904 | 6783 | `				pTos->iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|   103904 | 6784 | `				pTos->nIdx = SXU32_HIGH;` |
|   103904 | 6785 | `				pArg = &pTos[-1-nCallArgs];` |
|   103904 | 6786 | `				bEngineCallee = 1;` |
|        - | 6787 | ``				/* php dispatches a non-public __invoke through `$o()` from any scope`` |
|        - | 6788 | `				 * (it only WARNS at the declaration) -- the second half of what` |
|        - | 6789 | `				 * VmCallObjectInvoke stated, with pVm->bMagicDispatch. */` |
|   103904 | 6790 | `				bMagicDispatch = 1;` |
|   103904 | 6791 | `				goto CalleeByName;` |
|        - | 6792 | `			}` |
|        - | 6793 | `			{` |
|        - | 6794 | `				/* No __invoke: php's catchable Error, named after the class.` |
|        - | 6795 | `				 * Building the effective map is what CONSUMES this call's captured` |
|        - | 6796 | `				 * unpack runs, which a refused callee owes just as a dispatched one` |
|        - | 6797 | `				 * does — the map itself is never read from here. */` |
|   150008 | 6798 | `				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100004 | 6799 | `					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6800 | `				/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable` |
|        - | 6801 | `				 * slot itself (it pops top-down and the callable IS pTos), which for a` |
|        - | 6802 | `				 * temporary like (new Plain())(...) holds the only reference — popping it` |
|        - | 6803 | `				 * would free pThis before VmRaiseNotCallable reads its class name. */` |
|   100006 | 6804 | `				if( pThis ){` |
|   100006 | 6805 | `					pThis->iRef++;` |
|    50002 | 6806 | `				}` |
|   100006 | 6807 | `				if( nCallArgs > 0 ){` |
|      ! 0 | 6808 | `					VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6809 | `				}` |
|   100006 | 6810 | `				PH7_MemObjRelease(pTos);` |
|   100006 | 6811 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|   100006 | 6812 | `				pTos->nIdx = SXU32_HIGH;` |
|   100006 | 6813 | `				if( pThis == 0 ){` |
|        - | 6814 | `					/* An object slot with no instance behind it: nothing to name. */` |
|      ! 0 | 6815 | `					sxi32 rcNi = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 6816 | `						"Value of type object is not callable",` |
|        - | 6817 | `						sizeof("Value of type object is not callable")-1);` |
|      ! 0 | 6818 | `					if( rcNi == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 6819 | `					rc = rcNi;` |
|      ! 0 | 6820 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6821 | `				}` |
|   100006 | 6822 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|   100006 | 6823 | `				PH7_ClassInstanceUnref(pThis);` |
|   100006 | 6824 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 6825 | `					goto Abort;` |
|        - | 6826 | `				}` |
|        - | 6827 | `				{` |
|        - | 6828 | `					sxi32 iRp;` |
|   100006 | 6829 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|        - | 6830 | `						/* Drain the abandoned outer-expression operands` |
|        - | 6831 | ``						 * (`1 + $plainObj()`) to the try's base — one leaked`` |
|        - | 6832 | `						 * slot per caught throw otherwise. */` |
|   300010 | 6833 | `						PH7_RESUME_DRAIN()` |
|   100006 | 6834 | `						pc = iRp;` |
|   100006 | 6835 | `						break;` |
|        - | 6836 | `					}` |
|        - | 6837 | `				}` |
|      ! 0 | 6838 | `				goto Exception;` |
|        - | 6839 | `			}` |
|      ! 0 | 6840 | `		}else{` |
|        - | 6841 | `			/* php: calling a non-callable is a catchable Error naming the type` |
|        - | 6842 | `			 * ("Value of type int is not callable"), or -- for an array -- the shape it` |
|        - | 6843 | `			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so` |
|        - | 6844 | ``			 * `$x()` on a number quietly evaluated to nothing. */`` |
|        - | 6845 | `			sxi32 rcNc;` |
|        - | 6846 | `			char zMsg[128];` |
|       17 | 6847 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 6848 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");` |
|      ! 0 | 6849 | `			}else{` |
|       25 | 6850 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",` |
|        8 | 6851 | `					VmArithTypeName(pTos));` |
|        - | 6852 | `			}` |
|        - | 6853 | `			/* Consume this call's captured spread runs — a non-callable target` |
|        - | 6854 | `			 * (int/float/bool/null) reaches no dispatch build site. */` |
|       17 | 6855 | `			if( pInstr->iP2 & PH7_CALL_SPREAD ){` |
|      ! 0 | 6856 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 6857 | `			}` |
|        - | 6858 | `			/* Pop given arguments */` |
|       17 | 6859 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 6860 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6861 | `			}` |
|        - | 6862 | `			/* Settle the call's result slot BEFORE throwing. */` |
|       17 | 6863 | `			PH7_MemObjRelease(pTos);` |
|       17 | 6864 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       17 | 6865 | `			pTos->nIdx = SXU32_HIGH;` |
|       17 | 6866 | `			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       17 | 6867 | `			if( rcNc == SXERR_ABORT ){ goto Abort; }` |
|       17 | 6868 | `			rc = rcNc;` |
|        - | 6869 | `			/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 6870 | `			 * (SXRET_OK + recorded resume) FELL THROUGH and resumed the try body` |
|        - | 6871 | `			 * right after the failed call. Route like OP_THROW. */` |
|       27 | 6872 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6873 | `		}` |
|      256 | 6874 | `		break;` |
|        - | 6875 | `	}` |
|        - | 6876 | `	/* The callee is a NAME in pTos, with its arguments (and, for a method, its` |
|        - | 6877 | `	 * receiver) below it. Reached by falling through from the callable decode` |
|        - | 6878 | `	 * above, and jumped to by the __invoke-object branch, which builds exactly` |
|        - | 6879 | `	 * the two-slot method shape and lands here. */` |
|  3851880 | 6880 | `CalleeByName:` |
|  7810238 | 6881 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 6882 | `	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just` |
|        - | 6883 | `	 * anchors it to the global namespace — strip it before resolving so a` |
|        - | 6884 | ``	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */`` |
|  7810238 | 6885 | `	if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|       18 | 6886 | `		sName.zString++;` |
|       18 | 6887 | `		sName.nByte--;` |
|        8 | 6888 | `	}` |
|        - | 6889 | `	/* Ask this call SITE what its callee name meant last time. Resolving it costs up` |
|        - | 6890 | `	 * to four case-insensitive hash lookups -- the user table under the qualified name` |
|        - | 6891 | `	 * and then the global one, then the same pair of the host table -- and on a phpcs` |
|        - | 6892 | `	 * run that was 9.2% of everything the engine did. The site's answer is stamped with` |
|        - | 6893 | `	 * pVm->nCallableGen, so declaring anything retires all of them at once; see` |
|        - | 6894 | `	 * VmCallSite. */` |
|  7810238 | 6895 | `	pSiteEntry = PH7_VmCallSiteAnswer(pVm,pInstr,&sName,bEngineCallee,&bSiteHost);` |
|  7810238 | 6896 | `	if( pSiteEntry ){` |
|  4339429 | 6897 | `		pEntry = bSiteHost ? 0 : pSiteEntry;` |
|  2171834 | 6898 | `	}else` |
|        - | 6899 | `	{` |
|        - | 6900 | `	/* Check for a compiled function first.` |
|        - | 6901 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 6902 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|  3470814 | 6903 | `	pEntry = PH7_VmGetUserFunction(pVm,(const void *)sName.zString,sName.nByte,bEngineCallee);` |
|        - | 6904 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - | 6905 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - | 6906 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - | 6907 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - | 6908 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - | 6909 | `	{` |
|  3470814 | 6910 | `	VmCallArgMap *pCallMap = pEffCallMap;` |
|  3470814 | 6911 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - | 6912 | `		const char *zFunc;` |
|        - | 6913 | `		const char *zEnd;` |
|        - | 6914 | `		const char *z;` |
|        - | 6915 | `		SyString sGlobal;` |
|       99 | 6916 | `		zFunc = sName.zString;` |
|       99 | 6917 | `		zEnd  = zFunc + sName.nByte;` |
|       99 | 6918 | `		z = zEnd;` |
|        - | 6919 | `		/* Find last namespace separator */` |
|      949 | 6920 | `		while( z > zFunc ){` |
|      949 | 6921 | `			if( z[-1] == '\\' ){` |
|       99 | 6922 | `				break;` |
|        - | 6923 | `			}` |
|      855 | 6924 | `			z--;` |
|        5 | 6925 | `		}` |
|       99 | 6926 | `		if( z > zFunc && z < zEnd ){` |
|        - | 6927 | `			/* Retry lookup using the unqualified/global function name */` |
|       99 | 6928 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       99 | 6929 | `			pEntry = PH7_VmGetUserFunction(pVm,(const void *)sGlobal.zString,sGlobal.nByte,bEngineCallee);` |
|       47 | 6930 | `		}` |
|       47 | 6931 | `	}` |
|        - | 6932 | `	} /* end VmCallArgMap namespace scope */` |
|        - | 6933 | `	/* A user-table hit is the whole answer; a MISS is not (the host branch below` |
|        - | 6934 | `	 * resolves the rest), so only the hit is recorded here. */` |
|  3470814 | 6935 | `	PH7_VmCallSiteRecord(pVm,pInstr,&sName,bEngineCallee,0,pEntry);` |
|        - | 6936 | `	} /* end call-site cache miss */` |
|  7810238 | 6937 | `	if( pEntry ){` |
|        - | 6938 | `		ph7_vm_func_arg *aFormalArg;` |
|        - | 6939 | `		ph7_class_instance *pThis;` |
|        - | 6940 | `		ph7_value *pFrameStack;` |
|        - | 6941 | `		ph7_vm_func *pVmFunc;` |
|        - | 6942 | `		ph7_class *pSelf;` |
|        - | 6943 | `		ph7_class *pSelfHint;` |
|        - | 6944 | `		VmFrame *pFrame;` |
|        - | 6945 | `		ph7_value *pObj;` |
|        - | 6946 | `		VmSlot sArg;` |
|        - | 6947 | `		sxu32 n;` |
|  2404034 | 6948 | `		sxi32 iArgPreFlags = 0; /* the actual's type before its declared-type check */` |
|  2404034 | 6949 | `		int bClosureThis = 0;` |
|  2404034 | 6950 | `		ph7_class *pClosureScope = 0;` |
|        - | 6951 | `		/* initialize fields */` |
|  2404034 | 6952 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|  2404034 | 6953 | `		pThis = 0;` |
|  2404034 | 6954 | `		pSelf = 0;` |
|        - | 6955 | `		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it` |
|        - | 6956 | `		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever` |
|        - | 6957 | `		 * set for a bound plain closure, which dispatches as a function, so the method branch below` |
|        - | 6958 | `		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */` |
|  2404034 | 6959 | `		if( pVm->pClosureThis ){` |
|       79 | 6960 | `			pThis = pVm->pClosureThis;` |
|       79 | 6961 | `			pVm->pClosureThis = 0;` |
|       79 | 6962 | `			bClosureThis = 1;` |
|       37 | 6963 | `		}` |
|  2404034 | 6964 | `		if( pVm->pClosureScope ){` |
|        - | 6965 | `			/* May ride alongside a bound $this, or stand alone for a` |
|        - | 6966 | ``			 * scope-only rebind (`bindTo(null, Scope::class)`). */`` |
|       71 | 6967 | `			pClosureScope = pVm->pClosureScope;` |
|       71 | 6968 | `			pVm->pClosureScope = 0;` |
|       33 | 6969 | `		}` |
|  2404034 | 6970 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - | 6971 | `			ph7_class_method *pMeth;` |
|        - | 6972 | `			/* Class method call */` |
|  2083599 | 6973 | `			ph7_value *pTarget = &pTos[-1];` |
|  2083599 | 6974 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - | 6975 | `				/* Extract the 'this' pointer */` |
|  2083599 | 6976 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - | 6977 | `					/* Instance already loaded */` |
|  1980330 | 6978 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|  1980330 | 6979 | `					pThis->iRef++;` |
|  1980330 | 6980 | `					pSelf = pThis->pClass;` |
|   990080 | 6981 | `				}` |
|  2083599 | 6982 | `				if( pSelf == 0 ){` |
|   103274 | 6983 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - | 6984 | `						/* "Late Static Binding" class name */` |
|   154889 | 6985 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|    51623 | 6986 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|    51623 | 6987 | `					}` |
|   103274 | 6988 | `					if( pSelf == 0 ){` |
|       10 | 6989 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        4 | 6990 | `					}` |
|    51627 | 6991 | `				}` |
|  2083599 | 6992 | `				if( pThis == 0  ){` |
|   103274 | 6993 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|   103274 | 6994 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   103274 | 6995 | `					if( pFrameLocal->pParent ){` |
|        - | 6996 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|      965 | 6997 | `						pThis = pFrameLocal->pThis;` |
|      965 | 6998 | `						if( pThis ){` |
|       81 | 6999 | `							pThis->iRef++;` |
|       39 | 7000 | `						}` |
|      480 | 7001 | `					}` |
|    51627 | 7002 | `				}` |
|  2083599 | 7003 | `				VmPopOperand(&pTos,1);` |
|  2083599 | 7004 | `				PH7_MemObjRelease(pTos);` |
|        - | 7005 | `				/* Synchronize pointers. The method-name slot popped above sat BETWEEN` |
|        - | 7006 | `				 * this call's arguments and the (already-removed) target — so only now` |
|        - | 7007 | `				 * is pTos one past the last actual argument. Re-derive the unpack` |
|        - | 7008 | `				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs` |
|        - | 7009 | `				 * from run ends, which the extra target slot would otherwise offset,` |
|        - | 7010 | ``				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */`` |
|  3125486 | 7011 | `				nCallArgs = pInstr->iP1 + ((pInstr->iP2 & PH7_CALL_SPREAD)` |
|  1042128 | 7012 | `					? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);` |
|  2083599 | 7013 | `				pArg = &pTos[-nCallArgs];` |
|        - | 7014 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - | 7015 | `				 * user have already computed the random generated unique class method name` |
|        - | 7016 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - | 7017 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - | 7018 | `				 */` |
|  2083599 | 7019 | `				while( pArg < pStack ){` |
|      ! 0 | 7020 | `					pArg++;` |
|      ! 0 | 7021 | `				}` |
|  2083599 | 7022 | `				if( pSelf && pVm->bReflectBypass ){` |
|        - | 7023 | `					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection` |
|        - | 7024 | `					 * ignores visibility. Consume-once so nested calls made by the` |
|        - | 7025 | `					 * invoked body are checked normally. */` |
|     1502 | 7026 | `					pVm->bReflectBypass = 0;` |
|      752 | 7027 | `				}else` |
|  2082102 | 7028 | `				if( pSelf && bMagicDispatch && PH7_MagicMethodMustBePublic(&pVmFunc->sName) ){` |
|        - | 7029 | `					/* The ENGINE reaching for a magic method php requires to be public.` |
|        - | 7030 | `					 * php only WARNS at such a declaration and then dispatches the method` |
|        - | 7031 | ``					 * regardless -- the engine calling `__get` is not the outside world`` |
|        - | 7032 | `					 * reaching for a private member -- so a non-public` |
|        - | 7033 | ``					 * `__get`/`__call`/`__invoke`/`__sleep`/... still runs, where PHL threw`` |
|        - | 7034 | ``					 * `Call to private method C::__get()` at the ACCESS and killed the`` |
|        - | 7035 | `					 * script.` |
|        - | 7036 | `					 *` |
|        - | 7037 | `					 * The latch is set by PH7_VmCallMagicMethod at the engine's own` |
|        - | 7038 | `					 * dispatch sites. It is NOT inferred from the instruction: a` |
|        - | 7039 | ``					 * first-class `$o->__get(...)` and `$o->__get('x')` reach the same C`` |
|        - | 7040 | `					 * dispatcher, and php denies both. */` |
|    52525 | 7041 | `				}else` |
|  1977062 | 7042 | `				if( pSelf && !bMemberScreened ){ /* Paranoid edition */` |
|        - | 7043 | `					/* Check if the call is allowed. php binds non-public method` |
|        - | 7044 | `					 * access by the DECLARING class (pVmFunc->pUserData — the class` |
|        - | 7045 | `					 * the callee was compiled in), NOT the instance's class: an` |
|        - | 7046 | `					 * inherited base method calling $this->priv() on a child` |
|        - | 7047 | `					 * instance passes, a child's private SHADOW doesn't hijack the` |
|        - | 7048 | `					 * check for a parent callee, and the denial message names the` |
|        - | 7049 | `					 * declaring class like php. */` |
|  1814981 | 7050 | `					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|        - | 7051 | `					ph7_class *pOwnerClass;` |
|  1814981 | 7052 | `					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|  1814981 | 7053 | `					if( pMeth == 0 && pDeclClass != pSelf ){` |
|      ! 0 | 7054 | `						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      ! 0 | 7055 | `					}` |
|  1814981 | 7056 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        - | 7057 | `						/* ...except that a TRAIT is not a class php still has at run time: it` |
|        - | 7058 | `						 * composed the method INTO the using class, so that class owns the` |
|        - | 7059 | `						 * rule and the name. Deciding against the trait refused a protected` |
|        - | 7060 | `						 * trait method to a SUBCLASS of the composing class (which uses no` |
|        - | 7061 | ``						 * trait of its own) — `class Az { use Tz; } class Bz extends Az {`` |
|        - | 7062 | ``						 * $this->pr(); }` was a fatal php runs. Identity for every non-trait`` |
|        - | 7063 | `						 * method. */` |
|       29 | 7064 | `						pOwnerClass = PH7_VmMethodScopeName(&(*pVm),pSelf,pMeth);` |
|       29 | 7065 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - | 7066 | `							/* php throws a CATCHABLE Error here. The old code merely PRINTED an` |
|        - | 7067 | ``							 * uncaught-exception report and aborted, so `try { $o->priv(); }`` |
|        - | 7068 | ``							 * catch (Error $e)` never caught it and the script died. */`` |
|        - | 7069 | `							char zMsg[256];` |
|        - | 7070 | `							sxi32 rcVis;` |
|        - | 7071 | `							/* php NAMES the calling scope when there is one — "from scope C" —` |
|        - | 7072 | `							 * and says "global scope" only outside every class; the wording is` |
|        - | 7073 | `							 * shared with the first-class-callable screen` |
|        - | 7074 | `							 * (VmMethodVisibilityMsg). */` |
|       19 | 7075 | `							VmMethodVisibilityMsg(&(*pVm),pOwnerClass,` |
|        6 | 7076 | `								pVmFunc->sName.zString,pVmFunc->sName.nByte,` |
|        6 | 7077 | `								pMeth->iProtection,zMsg,sizeof(zMsg));` |
|        - | 7078 | `							/* Consume this call's captured spread runs — this visibility` |
|        - | 7079 | `							 * error exits before the pVmFunc build below. */` |
|       13 | 7080 | `							if( pInstr->iP2 & PH7_CALL_SPREAD ){` |
|      ! 0 | 7081 | `								VmSpreadConsume(pVm);` |
|      ! 0 | 7082 | `							}` |
|        - | 7083 | `							/* Pop given arguments, and leave the call's NULL result behind. */` |
|       13 | 7084 | `							if( nCallArgs > 0 ){` |
|      ! 0 | 7085 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7086 | `							}` |
|       13 | 7087 | `							PH7_MemObjRelease(pTos);` |
|       13 | 7088 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|       13 | 7089 | `							pTos->nIdx = SXU32_HIGH;` |
|       13 | 7090 | `							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       13 | 7091 | `							if( rcVis == SXERR_ABORT ){ goto Abort; }` |
|       13 | 7092 | `							rc = rcVis;` |
|        - | 7093 | `							/* ENFORCE_RC never consulted pResumeFrame, so an in-place` |
|        - | 7094 | `							 * catch (SXRET_OK + recorded resume) FELL THROUGH and` |
|        - | 7095 | `							 * dispatched the DENIED method anyway. Route like OP_THROW. */` |
|       13 | 7096 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7097 | `						}` |
|        8 | 7098 | `					}` |
|   907446 | 7099 | `				}` |
|  1041701 | 7100 | `			}` |
|  1041701 | 7101 | `		}` |
|        - | 7102 | `		/* pArg is now finalized for every pVmFunc callee (a method call popped its` |
|        - | 7103 | `		 * method-name slot above; functions/closures/generators keep the top base).` |
|        - | 7104 | `		 * Build the PHP 8.1 effective spread-key map here so the generator and the` |
|        - | 7105 | `		 * install path below both see it — and so this call's captured runs are` |
|        - | 7106 | `		 * consumed exactly once, against the correct base. */` |
|  3606145 | 7107 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  2404017 | 7108 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 7109 | `		/* Check the PHP call-depth cap (the sole site).` |
|        - | 7110 | `		 * Default is unbounded (heap-bound recursion, decoupled from the C stack` |
|        - | 7111 | `		 * by the stage-2 trampoline); the C stack is guarded separately by` |
|        - | 7112 | `		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and` |
|        - | 7113 | `		 * then raises a clean non-catchable fatal (was: silently set NULL and` |
|        - | 7114 | `		 * continue) and halts. */` |
|  2404022 | 7115 | `		if( VmRecursionExceeded(pVm) ){` |
|        - | 7116 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - | 7117 | `			 * which walks the whole operand stack — don't release them here. */` |
|        3 | 7118 | `			VmRecursionFatal(&(*pVm));` |
|        3 | 7119 | `			goto Abort;` |
|        - | 7120 | `		}` |
|  2404020 | 7121 | `		if( pVmFunc->pNextName ){` |
|        - | 7122 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|       76 | 7123 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       36 | 7124 | `		}` |
|        - | 7125 | ``		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's`` |
|        - | 7126 | ``		 * `Class::method`). Computed after overload resolution so it reflects the selected method.`` |
|        - | 7127 | ``		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed`` |
|        - | 7128 | `		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method` |
|        - | 7129 | ``		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the`` |
|        - | 7130 | `		 * USING class; we don't carry the using class on the shared struct, so fall back to the` |
|        - | 7131 | `		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for` |
|        - | 7132 | `		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */` |
|  2404020 | 7133 | `		pSelfHint = pSelf;` |
|  2404020 | 7134 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|  2083587 | 7135 | `			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;` |
|  2083587 | 7136 | `			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|  2083133 | 7137 | `				pSelfHint = pDecl;` |
|  1041474 | 7138 | `			}` |
|  1041701 | 7139 | `		}` |
|  2404020 | 7140 | `		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 7141 | `			/* Push the closure's called-class as this frame's LSB class so` |
|        - | 7142 | ``			 * `static::` inside the body resolves like php. An explicit`` |
|        - | 7143 | `			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use` |
|        - | 7144 | `			 * the class captured at the closure's creation site. self::/parent::` |
|        - | 7145 | `			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done` |
|        - | 7146 | ``			 * after pSelfHint so a `self`-typed param is unaffected. */`` |
|    37872 | 7147 | `			if( pClosureScope ){` |
|       71 | 7148 | `				pSelf = pClosureScope;` |
|    37839 | 7149 | `			}else if( pVmFunc->pLsbClass ){` |
|      190 | 7150 | `				pSelf = (ph7_class *)pVmFunc->pLsbClass;` |
|       93 | 7151 | `			}` |
|    18706 | 7152 | `		}` |
|  2404020 | 7153 | `		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){` |
|        - | 7154 | `			/* php 8.4 #[\Deprecated] runtime notice — once per call, before` |
|        - | 7155 | `			 * execution (generators: at the g(...) call site, like php). */` |
|      581 | 7156 | `			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,` |
|      384 | 7157 | `				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);` |
|      192 | 7158 | `		}` |
|        - | 7159 | `		/* php 8.5 #[\NoDiscard]: the CALL SITE decided this answer is thrown away` |
|        - | 7160 | ``		 * (the codegen's bDiscard, php's !RETURN_VALUE_USED), and a `(void)` cast`` |
|        - | 7161 | `		 * in front of the statement is what clears it. Raised before the body, so` |
|        - | 7162 | `		 * a callee that throws has already warned — php's order. */` |
|  2404020 | 7163 | `		if( (pVmFunc->iFlags & VM_FUNC_NODISCARD) && bResultDropped ){` |
|        - | 7164 | `			/* php calls it a "method" whenever the callee has a class SCOPE, and a` |
|        - | 7165 | ``			 * closure declared in a class body has one -- `C::{closure:C::go():3}`. */`` |
|       57 | 7166 | `			ph7_class *pNdClass = 0;` |
|       57 | 7167 | `			if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|       21 | 7168 | `				pNdClass = pSelfHint;` |
|       47 | 7169 | `			}else if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        7 | 7170 | `				pNdClass = (ph7_class *)pVmFunc->pLsbClass;` |
|        3 | 7171 | `			}` |
|       57 | 7172 | `			VmNoDiscardWarn(&(*pVm),pVmFunc,pNdClass);` |
|       28 | 7173 | `		}` |
|        - | 7174 | `		/* D1: resolve deferred plain-var arguments against this callee's declaration` |
|        - | 7175 | `		 * now — BEFORE the native/generator splits and VmEnterFrame, while pVm->pFrame is` |
|        - | 7176 | `		 * still the caller. pVmFunc is final here (post-overload). Covers plain functions,` |
|        - | 7177 | `		 * methods, closures, dynamic-name calls, generators and native methods uniformly;` |
|        - | 7178 | `		 * only the SOURCE of the by-ref positions differs between the last one and the` |
|        - | 7179 | `		 * rest (a signature string vs. compiled formal parameters). */` |
|        - | 7180 | `		{` |
|        - | 7181 | `			sxi32 rcDA;` |
|  2404020 | 7182 | `			if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 7183 | `				/* A native method declares no formal parameters to match against —` |
|        - | 7184 | `				 * its by-ref positions come from the same signature-derived mask a` |
|        - | 7185 | `				 * builtin uses, so resolve them the builtin way. Sharing this one` |
|        - | 7186 | `				 * site (rather than repeating it in the branch below) keeps the` |
|        - | 7187 | `				 * throw routing identical for both kinds of callee. */` |
|  2357336 | 7188 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|  1571614 | 7189 | `					pVmFunc->pNative->nByRefMask,0,0,pEffCallMap);` |
|   785722 | 7190 | `			}else{` |
|  1248811 | 7191 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   832401 | 7192 | `					(ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs),SySetUsed(&pVmFunc->aArgs),` |
|   416405 | 7193 | `					0,0,0,pEffCallMap);` |
|        - | 7194 | `			}` |
|  2404028 | 7195 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 7196 | `		}` |
|  2403966 | 7197 | `		if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 7198 | `			/* C-bodied method (VM_FUNC_NATIVE): hand it to the foreign-function path.` |
|        - | 7199 | `			 *` |
|        - | 7200 | `			 * Everything a METHOD needs has already happened above — the sVmName` |
|        - | 7201 | `			 * lookup, overload selection, $this / self / late-static-binding` |
|        - | 7202 | `			 * resolution, the visibility check, the #[\Deprecated] notice, deferred` |
|        - | 7203 | `			 * argument materialization — and everything BELOW is exactly what a C` |
|        - | 7204 | `			 * body has no use for: a frame, an operand stack, a call record.` |
|        - | 7205 | `			 *` |
|        - | 7206 | `			 * The stack shape already matches a builtin's, because the method branch` |
|        - | 7207 | `			 * popped both the method-name slot and the target slot: [pArg,pTos) are` |
|        - | 7208 | `			 * this call's arguments and pTos is the slot that receives the result.` |
|        - | 7209 | `			 * So the jump lands on shared code, not a copy of it. */` |
|  1571619 | 7210 | `			pFunc = pVmFunc->pNative;` |
|  1571619 | 7211 | `			pNativeOwned = pThis; /* borrowed; the shared tail drops this reference */` |
|        - | 7212 | ``			/* A `C::m()` call reaches the method path with no object in the target`` |
|        - | 7213 | `			 * slot, and the path then adopts the CALLER's $this so an instance-context` |
|        - | 7214 | ``			 * `self::m()` still finds one. A native STATIC method must not see that —`` |
|        - | 7215 | `			 * it would read its argument list one slot off — so the receiver handed to` |
|        - | 7216 | `			 * the body is gated on the declaration, not on what the fallback found. */` |
|  1571619 | 7217 | `			pNativeRecv = (pVmFunc->iFlags & VM_FUNC_NATIVE_STATIC) ? 0 : pThis;` |
|  1571619 | 7218 | `			pNativeClass = pSelf;` |
|  1571619 | 7219 | `			pNativeMethod = pVmFunc;` |
|        - | 7220 | ``			/* php's `class` key is the DECLARING class, which oo.c parks on the`` |
|        - | 7221 | `			 * method's pUserData -- an inherited native method reports the base,` |
|        - | 7222 | `			 * not the receiver's class (SplTempFileObject->setMaxLineLen() is` |
|        - | 7223 | ``			 * `SplFileObject->setMaxLineLen` in php's trace). */`` |
|  2357516 | 7224 | `			pNativeDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData` |
|   785897 | 7225 | `				: (pThis ? pThis->pClass : pSelf);` |
|  1571619 | 7226 | `			goto NativeCall;` |
|        - | 7227 | `		}` |
|   832352 | 7228 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - | 7229 | `			/* Generator function: return a Generator object instead of executing */` |
|        - | 7230 | `			ph7_exec_ctx *pExecCtx;` |
|        - | 7231 | `			ph7_generator *pGenerator;` |
|        - | 7232 | `			ph7_class_instance *pGenObj;` |
|        - | 7233 | `			ph7_value *pCtxAttr;` |
|        - | 7234 | `			SyString sAttrName;` |
|        - | 7235 | `			ph7_value **apCallArgs;` |
|        - | 7236 | `			int nGenArgs, iArg;` |
|        - | 7237 | `			/* Collect arguments from the operand stack */` |
|      709 | 7238 | `			nGenArgs = (int)(pTos - pArg);` |
|      709 | 7239 | `			apCallArgs = 0;` |
|      709 | 7240 | `			if( nGenArgs > 0 ){` |
|        - | 7241 | `				/* php refuses a non-variable in a by-ref position at the CALL, and for` |
|        - | 7242 | `				 * a generator this IS the call. Routed like the branch's other` |
|        - | 7243 | `				 * pre-frame throws below: no callee frame exists yet, so drop the` |
|        - | 7244 | `				 * arguments plus the function-name slot and land the enclosing try. */` |
|      248 | 7245 | `				rc = VmScreenGenByRefArgs(&(*pVm),pVmFunc,pEffCallMap,pArg,` |
|       81 | 7246 | `					(sxu32)nGenArgs,pSelfHint);` |
|      167 | 7247 | `				if( rc != SXRET_OK ){` |
|        7 | 7248 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 7249 | `						goto Abort;` |
|        - | 7250 | `					}` |
|      356 | 7251 | `					PH7_INLINE_RESUME_BREAK()` |
|        7 | 7252 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 7253 | `					{` |
|        - | 7254 | `						sxi32 iRpB;` |
|        7 | 7255 | `						if( VmRecordedResume(pVm,&iRpB,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 7256 | `							pc = iRpB;` |
|      ! 0 | 7257 | `							break;` |
|        - | 7258 | `						}` |
|        - | 7259 | `					}` |
|        7 | 7260 | `					goto Exception;` |
|        - | 7261 | `				}` |
|       78 | 7262 | `			}` |
|      703 | 7263 | `			if( nGenArgs > 0 ){` |
|        - | 7264 | `				/* A named call binds each actual to the formal it NAMES and leaves the` |
|        - | 7265 | `				 * formals BETWEEN two named ones on their defaults, so the reordered` |
|        - | 7266 | `				 * list carries one entry per formal and can be LONGER than the number` |
|        - | 7267 | `				 * of actuals. */` |
|      239 | 7268 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|      156 | 7269 | `					((sxu32)nGenArgs + SySetUsed(&pVmFunc->aArgs)) * sizeof(ph7_value *));` |
|      161 | 7270 | `				if( apCallArgs == 0 ){` |
|        - | 7271 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 | 7272 | `					nGenArgs = 0;` |
|      ! 0 | 7273 | `				}else{` |
|      161 | 7274 | `					VmCallArgMap *pGenMap = pEffCallMap;` |
|      161 | 7275 | `					int didReorder = 0;` |
|      161 | 7276 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - | 7277 | `						/* Named-argument reordering for generator */` |
|       38 | 7278 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|       38 | 7279 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|       38 | 7280 | `						sxu32 nNV = nF;` |
|       38 | 7281 | `						sxi32 iVIdx = -1;` |
|        - | 7282 | `						sxi32 *aGSlot;` |
|        - | 7283 | `						sxu8 *aGUsed;` |
|        - | 7284 | `						sxu32 gi;` |
|      120 | 7285 | `						for( gi = 0; gi < nF; gi++ ){` |
|       90 | 7286 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|       45 | 7287 | `						}` |
|       55 | 7288 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       34 | 7289 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|       38 | 7290 | `						if( aGSlot ){` |
|       38 | 7291 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|       55 | 7292 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|       17 | 7293 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|       38 | 7294 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 7295 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 7296 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 7297 | `								goto Abort;` |
|        - | 7298 | `							}` |
|       38 | 7299 | `							if( rc == PH7_EXCEPTION ){` |
|        - | 7300 | `								/* A named-argument error is php's catchable \Error.` |
|        - | 7301 | `								 * No callee frame exists yet on this branch (the` |
|        - | 7302 | `								 * generator body never runs and VmEnterFrame is` |
|        - | 7303 | `								 * further down), so route it like the other` |
|        - | 7304 | `								 * pre-frame OP_CALL throws: drop the args + the` |
|        - | 7305 | `								 * function-name slot and land the enclosing try. */` |
|        3 | 7306 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        3 | 7307 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        3 | 7308 | `								PH7_INLINE_RESUME_BREAK()` |
|        3 | 7309 | `								VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 7310 | `								{` |
|        - | 7311 | `									sxi32 iRpN;` |
|        3 | 7312 | `									if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){` |
|        3 | 7313 | `										pc = iRpN;` |
|        3 | 7314 | `										break;` |
|        - | 7315 | `									}` |
|        - | 7316 | `								}` |
|      ! 0 | 7317 | `								goto Exception;` |
|        - | 7318 | `							}` |
|        - | 7319 | `							{` |
|        - | 7320 | `								/* php's named-hole ArgumentCountError, checked BEFORE` |
|        - | 7321 | `								 * hole compaction: compacting first would report the` |
|        - | 7322 | `								 * positional wording with a fictitious count (g(b:2)` |
|        - | 7323 | ``								 * must be `g(): Argument #1 ($a) not passed`, not`` |
|        - | 7324 | `								 * "1 passed"). Implicit-required watermark, like the` |
|        - | 7325 | `								 * plain-call named path; a hole with NOTHING filled` |
|        - | 7326 | `								 * above it keeps php's count wording — fall through` |
|        - | 7327 | `								 * to VmFiberSetupFrame's check (the compacted count` |
|        - | 7328 | `								 * equals php's num_args there). */` |
|       36 | 7329 | `								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;` |
|       36 | 7330 | `								sxi32 iHole = -1;` |
|       36 | 7331 | `								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);` |
|       92 | 7332 | `								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){` |
|       60 | 7333 | `									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){` |
|       54 | 7334 | `										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);` |
|       25 | 7335 | `									}` |
|       32 | 7336 | `								}` |
|       74 | 7337 | `								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){` |
|        - | 7338 | `									sxu32 gj;` |
|       42 | 7339 | `									int bFound = 0;` |
|       54 | 7340 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       52 | 7341 | `										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }` |
|        9 | 7342 | `									}` |
|       42 | 7343 | `									if( !bFound && gHole + 1 <= nMaxFilledG ){` |
|        3 | 7344 | `										iHole = (sxi32)gHole;` |
|        1 | 7345 | `									}` |
|       23 | 7346 | `								}` |
|       36 | 7347 | `								if( iHole >= 0 ){` |
|        4 | 7348 | `									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,pVmFunc,` |
|        2 | 7349 | `										(sxu32)iHole+1,&aFA[iHole].sName);` |
|        3 | 7350 | `									SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        3 | 7351 | `									SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        3 | 7352 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 7353 | `										goto Abort;` |
|        - | 7354 | `									}` |
|        - | 7355 | `									/* Route like the VmFiberSetupFrame throw below */` |
|        4 | 7356 | `									PH7_INLINE_RESUME_BREAK()` |
|        3 | 7357 | `									VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 7358 | `									{` |
|        - | 7359 | `										sxi32 iRpH;` |
|        3 | 7360 | `										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){` |
|        3 | 7361 | `											pc = iRpH;` |
|        3 | 7362 | `											break;` |
|        - | 7363 | `										}` |
|        - | 7364 | `									}` |
|      ! 0 | 7365 | `									goto Exception;` |
|        - | 7366 | `								}` |
|        - | 7367 | `							}` |
|        - | 7368 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - | 7369 | `							 * append overflow (variadic / positional beyond` |
|        - | 7370 | `							 * formals) so downstream sees every argument.` |
|        - | 7371 | `							 *` |
|        - | 7372 | `							 * A formal that no argument named keeps its place as a` |
|        - | 7373 | `							 * NULL entry, which VmFiberSetupFrame reads as "not` |
|        - | 7374 | `							 * passed" and answers with the declared default. Dropping` |
|        - | 7375 | `							 * the hole instead shifted every later actual down one` |
|        - | 7376 | ``							 * formal, silently: `function g($a,$b=2,$c=3)` called`` |
|        - | 7377 | ``							 * `g(a: 1, c: 9)` bound `$b = 9` and left `$c` on its`` |
|        - | 7378 | ``							 * default, where php binds `1/2/9`. Trailing holes are`` |
|        - | 7379 | `							 * simply not passed. */` |
|        - | 7380 | `							{` |
|       34 | 7381 | `								int nOut = 0, nFilled = 0;` |
|      108 | 7382 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - | 7383 | `									sxu32 gj;` |
|       78 | 7384 | `									ph7_value *pNamed = 0;` |
|      138 | 7385 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|      116 | 7386 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|       56 | 7387 | `											pNamed = &pArg[gj];` |
|       56 | 7388 | `											break;` |
|        - | 7389 | `										}` |
|       33 | 7390 | `									}` |
|       78 | 7391 | `									apCallArgs[nOut++] = pNamed;` |
|       78 | 7392 | `									if( pNamed ){` |
|       56 | 7393 | `										nFilled = nOut;` |
|       26 | 7394 | `									}` |
|       41 | 7395 | `								}` |
|       34 | 7396 | `								nOut = nFilled;` |
|       86 | 7397 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|       56 | 7398 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 | 7399 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 | 7400 | `									}` |
|       30 | 7401 | `								}` |
|       34 | 7402 | `								nGenArgs = nOut;` |
|        - | 7403 | `							}` |
|       34 | 7404 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|       34 | 7405 | `							didReorder = 1;` |
|       15 | 7406 | `						}` |
|        - | 7407 | `						/* If aGSlot allocation failed, fall through to` |
|        - | 7408 | `						 * positional fill below — preserves arg order rather` |
|        - | 7409 | `						 * than passing an uninitialized apCallArgs. */` |
|       15 | 7410 | `					}` |
|      157 | 7411 | `					if( !didReorder ){` |
|      256 | 7412 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|      134 | 7413 | `							apCallArgs[iArg] = &pArg[iArg];` |
|       69 | 7414 | `						}` |
|       61 | 7415 | `					}` |
|        - | 7416 | `				}` |
|       76 | 7417 | `			}` |
|        - | 7418 | `			/* Create execution context and generator wrapper */` |
|      699 | 7419 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|      699 | 7420 | `			if( pExecCtx == 0 ){` |
|      ! 0 | 7421 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 7422 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 7423 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 7424 | `				break;` |
|        - | 7425 | `			}` |
|        - | 7426 | `			/* php's called-scope for the generator BODY: its frame is detached and` |
|        - | 7427 | `			 * created before the receiver is known, so stamp it here, where the call` |
|        - | 7428 | `			 * that made the generator still has it. A trait method's executing scope` |
|        - | 7429 | `			 * is found by walking this class (VmTraitScopeFrom), and without it a` |
|        - | 7430 | `			 * generator declared in a trait could not reach the class's protected` |
|        - | 7431 | `			 * members -- the one activation shape the frame walk cannot recover. */` |
|      699 | 7432 | `			pExecCtx->pFrame->pSelfClass = pSelf ? pSelf : (pThis ? pThis->pClass : 0);` |
|        - | 7433 | `` 			/* And the LATE-STATIC-BINDING class, which is a different question: `self::` `` |
|        - | 7434 | ``			 * is the declaring scope above, `static::` is the class the call was made`` |
|        - | 7435 | `			 * THROUGH. The body resumes long after this call returned, so pVm->aSelf no` |
|        - | 7436 | ``			 * longer holds it and `static::class` / `new static` / `static::m()` inside a`` |
|        - | 7437 | ``			 * generator answered `Class "static" not found`. VmStartCtx republishes this`` |
|        - | 7438 | `			 * for the body's duration. An instance call means the receiver's class; a` |
|        - | 7439 | `			 * static one means the class the call named. */` |
|      699 | 7440 | `			pExecCtx->pLsbClass = pThis ? pThis->pClass : (pSelf ? pSelf : 0);` |
|      699 | 7441 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|      699 | 7442 | `			if( pGenerator == 0 ){` |
|      ! 0 | 7443 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 | 7444 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 7445 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 7446 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 7447 | `				break;` |
|        - | 7448 | `			}` |
|        - | 7449 | `			/* Set up the frame with arguments, closure env, $this */` |
|      699 | 7450 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|      699 | 7451 | `			pVm->pFrame = pExecCtx->pFrame;` |
|     1393 | 7452 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,` |
|      694 | 7453 | `				pEffCallMap ? (pEffCallMap->bStrict ? 1 : 0) : (pVm->bCurStrict ? 1 : 0),` |
|      347 | 7454 | `				pSelfHint,` |
|        - | 7455 | `				TRUE/*generator: the g(...) call site is in the message*/,` |
|        - | 7456 | `				TRUE/*a source-level call binds a by-ref parameter to the caller's slot*/);` |
|      699 | 7457 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|      699 | 7458 | `			pExecCtx->pFrame->pParent = 0;` |
|      699 | 7459 | `			if( apCallArgs ){` |
|      157 | 7460 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       76 | 7461 | `			}` |
|      699 | 7462 | `			if( rc != SXRET_OK ){` |
|       20 | 7463 | `				VmReleaseGenerator(pVm, pGenerator);` |
|       20 | 7464 | `				if( pThis ){` |
|        3 | 7465 | `					PH7_ClassInstanceUnref(pThis);` |
|        1 | 7466 | `				}` |
|       20 | 7467 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 7468 | `					goto Abort;` |
|        - | 7469 | `				}` |
|       20 | 7470 | `				if( rc == PH7_EXCEPTION ){` |
|        - | 7471 | `					/* A declared-type TypeError thrown while binding the` |
|        - | 7472 | `					 * generator's arguments (php binds + type-checks eagerly at` |
|        - | 7473 | `					 * the g(...) call site, before any resume — band A #2). If` |
|        - | 7474 | `					 * an inline try THIS exec owns caught it, land at its` |
|        - | 7475 | `					 * redirect (the drain subsumes the operand pops); else pop` |
|        - | 7476 | `					 * the args + function name and route like the other` |
|        - | 7477 | `					 * OP_CALL throw paths. */` |
|       24 | 7478 | `					PH7_INLINE_RESUME_BREAK()` |
|       18 | 7479 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 7480 | `					{` |
|        - | 7481 | `						sxi32 iRpG;` |
|       18 | 7482 | `						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){` |
|       18 | 7483 | `							pc = iRpG;` |
|       18 | 7484 | `							break;` |
|        - | 7485 | `						}` |
|        - | 7486 | `					}` |
|      ! 0 | 7487 | `					goto Exception;` |
|        - | 7488 | `				}` |
|      ! 0 | 7489 | `				break;` |
|        - | 7490 | `			}` |
|        - | 7491 | `			/* Create Generator class instance */` |
|      681 | 7492 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|      681 | 7493 | `			if( pGenObj == 0 ){` |
|      ! 0 | 7494 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 | 7495 | `				break;` |
|        - | 7496 | `			}` |
|        - | 7497 | `			/* Store generator in __ctx attribute */` |
|      681 | 7498 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      681 | 7499 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|      681 | 7500 | `			if( pCtxAttr ){` |
|      681 | 7501 | `				pCtxAttr->x.pOther = pGenerator;` |
|      681 | 7502 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      338 | 7503 | `			}` |
|        - | 7504 | `			/* Pop args and function name, push Generator object. PH7_NewClassInstance` |
|        - | 7505 | `			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do` |
|        - | 7506 | `			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope` |
|        - | 7507 | ``			 * and its __destruct (which runs pending `finally` blocks and frees the`` |
|        - | 7508 | `			 * exec context) never fires. */` |
|      681 | 7509 | `			PH7_MemObjRelease(pTos);` |
|      681 | 7510 | `			pTos = &pTos[-nCallArgs];` |
|      681 | 7511 | `			pTos->x.pOther = pGenObj;` |
|      681 | 7512 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|      681 | 7513 | `			if( pThis ){` |
|       25 | 7514 | `				PH7_ClassInstanceUnref(pThis);` |
|       11 | 7515 | `			}` |
|      681 | 7516 | `			break;` |
|        - | 7517 | `		}` |
|        - | 7518 | `		/* Extract the formal argument set */` |
|   831648 | 7519 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - | 7520 | `		/* Create a new VM frame  */` |
|   831648 | 7521 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|   831648 | 7522 | `		if( rc == SXRET_OK && pFrame && (bCallbackWeak \|\| pVm->pNativeFrameName) ){` |
|        - | 7523 | `			/* An INTERNAL function reached for this callback, so the frame above it` |
|        - | 7524 | `			 * is that builtin's and not user code: php names no call site in an` |
|        - | 7525 | `			 * argument diagnostic raised here. The same latch already decides the` |
|        - | 7526 | `			 * binding mode and the too-few wording; this records it on the frame,` |
|        - | 7527 | `			 * because the type error is raised further down the argument-binding` |
|        - | 7528 | `			 * path than the latch survives. */` |
|    18979 | 7529 | `			pFrame->iFlags \|= VM_FRAME_NATIVE_CALLER;` |
|        - | 7530 | `			/* ...and WHICH builtin, because php gives it a backtrace frame of its own` |
|        - | 7531 | `			 * (the callback's carries no file/line instead). pCalleeName is the host` |
|        - | 7532 | `			 * function currently running, saved/restored around every dispatch. */` |
|    28534 | 7533 | `			pFrame->pNativeCaller = pVm->pNativeFrameName` |
|    18974 | 7534 | `				? pVm->pNativeFrameName : pVm->pCalleeName;` |
|        - | 7535 | `			/* Consume: the latch describes ONE call, and a call the callback body` |
|        - | 7536 | `			 * makes must not inherit it. */` |
|    18979 | 7537 | `			pVm->pNativeFrameName = 0;` |
|     9419 | 7538 | `		}` |
|   831648 | 7539 | `		if( rc == SXRET_OK ){` |
|        - | 7540 | `			/* This activation now needs the function it is about to run. For a` |
|        - | 7541 | `			 * run-time closure that is a hold on its per-instantiation copy, so the` |
|        - | 7542 | `			 * copy cannot be freed under a body that is still executing when its` |
|        - | 7543 | `			 * last Closure object dies. VmLeaveFrame / VmFreeDetachedFrame give it` |
|        - | 7544 | `			 * back -- and a coroutine's DETACHED body frame keeps it for exactly as` |
|        - | 7545 | `			 * long as the frame lives, which is what a suspended generator needs. */` |
|   831648 | 7546 | `			PH7_VmClosureFuncRef(pVmFunc);` |
|   416026 | 7547 | `		}` |
|   831648 | 7548 | `		if( rc == SXRET_OK && pFrame && pSelf ){` |
|        - | 7549 | `			/* php's called-scope: the class this call was made THROUGH. Same as the` |
|        - | 7550 | `			 * receiver's for an instance call, and the named class for a static one --` |
|        - | 7551 | `			 * which is the only way to say which class composed a static TRAIT method. */` |
|   512181 | 7552 | `			pFrame->pSelfClass = pSelf;` |
|   256088 | 7553 | `		}` |
|   831648 | 7554 | `		if( rc != SXRET_OK ){` |
|        - | 7555 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 7556 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 7557 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 7558 | `				&pVmFunc->sName);` |
|        - | 7559 | `			/* The frame that would own (and later release) $this never got created; for a bound` |
|        - | 7560 | `			 * plain closure the consumed transient is the object's ONLY ref, so release it here` |
|        - | 7561 | `			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */` |
|      ! 0 | 7562 | `			if( bClosureThis && pThis ){` |
|      ! 0 | 7563 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 | 7564 | `			}` |
|        - | 7565 | `			/* Pop given arguments */` |
|      ! 0 | 7566 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 7567 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7568 | `			}` |
|        - | 7569 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 | 7570 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 7571 | `			break;` |
|        - | 7572 | `		}` |
|   831648 | 7573 | `		if( pClosureScope ){` |
|        - | 7574 | `			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /` |
|        - | 7575 | `			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected` |
|        - | 7576 | `			 * access inside the body resolves against it. */` |
|       69 | 7577 | `			pFrame->pBoundScope = pClosureScope;` |
|       32 | 7578 | `		}` |
|        - | 7579 | `		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band` |
|        - | 7580 | `		 * A #4): sArg over-counts (defaulted params installed, variadic packed` |
|        - | 7581 | `		 * as one entry) so php's answers can't be derived from it. */` |
|   831648 | 7582 | `		pFrame->nActualArgs = (int)(pTos - pArg);` |
|   831648 | 7583 | `		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) \|\| bClosureThis) ){` |
|        - | 7584 | `			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */` |
|        - | 7585 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|   411389 | 7586 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|   411389 | 7587 | `			if( pObj ){` |
|        - | 7588 | `				/* Reflect the change */` |
|   411389 | 7589 | `				pObj->x.pOther = pThis;` |
|   411389 | 7590 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   205692 | 7591 | `			}` |
|   205692 | 7592 | `		}` |
|   831648 | 7593 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - | 7594 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - | 7595 | `			/* Install static variables */` |
|      114 | 7596 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      240 | 7597 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      130 | 7598 | `				pStatic = &aStatic[n];` |
|      130 | 7599 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - | 7600 | `					/* Initialize the static variables */` |
|       60 | 7601 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|       60 | 7602 | `					if( pObj ){` |
|        - | 7603 | `						/* Assume a NULL initialization value */` |
|       60 | 7604 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|       60 | 7605 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - | 7606 | `							/* Evaluate initialization expression (Any complex expression).` |
|        - | 7607 | `							 * A static's initializer is the one initializer php does NOT` |
|        - | 7608 | `							 * treat as a compile-time constant: it runs inside the call,` |
|        - | 7609 | ``							 * so `static $x = static::class;` is LATE-bound to the runtime`` |
|        - | 7610 | `							 * class. The aSelf push that PH7_VmPeekTopClass answers from` |
|        - | 7611 | `							 * happens further down (this frame is still being built), so` |
|        - | 7612 | `							 * stand it up for the eval and take it straight back --` |
|        - | 7613 | ``							 * without it `static::` found no class at all. */`` |
|       43 | 7614 | `							int bSelfPushed = 0;` |
|       43 | 7615 | `							if( pSelf ){` |
|        9 | 7616 | `								SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|        9 | 7617 | `								bSelfPushed = 1;` |
|        4 | 7618 | `							}` |
|       43 | 7619 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|       43 | 7620 | `							if( bSelfPushed ){` |
|        9 | 7621 | `								(void)SySetPop(&pVm->aSelf);` |
|        4 | 7622 | `							}` |
|       20 | 7623 | `						}` |
|       60 | 7624 | `						pObj->nIdx = pStatic->nIdx;` |
|        - | 7625 | `						/* Permanent pin: the storage outlives every call */` |
|       60 | 7626 | `						VmPinMemObjSlot(&(*pVm),pStatic->nIdx);` |
|       32 | 7627 | `					}else{` |
|      ! 0 | 7628 | `						continue;` |
|        - | 7629 | `					}` |
|       28 | 7630 | `				}` |
|        - | 7631 | `				/* Install in the current frame — a REGISTERED binding, and the slot is` |
|        - | 7632 | `				 * PINNED: the static's storage belongs to the function, not to this` |
|        - | 7633 | `				 * call, so neither the frame teardown nor an unset of the NAME may` |
|        - | 7634 | `				 * recycle it. Poking hVar directly left the binding invisible to the` |
|        - | 7635 | ``				 * reference table, so an array element sharing the static (`[&$s]`)`` |
|        - | 7636 | ``				 * did not count as a reference and `unset($s)` destroyed the storage —`` |
|        - | 7637 | `				 * the next call started over from the initializer. The pin is taken ONCE,` |
|        - | 7638 | `				 * where the slot is created (above). */` |
|      193 | 7639 | `				PH7_VmBindVarSlot(&(*pVm),pFrame,SyStringData(&pStatic->sName),` |
|       63 | 7640 | `					SyStringLength(&pStatic->sName),pStatic->nIdx);` |
|       67 | 7641 | `			}` |
|       55 | 7642 | `		}` |
|        - | 7643 | `		/* Push arguments in the local frame */` |
|        - | 7644 | `		{` |
|   831648 | 7645 | `		VmCallArgMap *pCallMap3 = pEffCallMap;` |
|        - | 7646 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - | 7647 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|        - | 7648 | `		/* A compiled call in a strict file always carries a map with bStrict set` |
|        - | 7649 | `		 * (GenStateAttachStrictFlag); with no map at all the call is either a` |
|        - | 7650 | `		 * compiled WEAK one — where bCurStrict is 0 for the same unit — or an` |
|        - | 7651 | `		 * ENGINE-dispatched one, whose synthetic OP_CALL has no map to carry the` |
|        - | 7652 | `		 * caller's mode. php scopes parameter coercion by the CALLING file either` |
|        - | 7653 | `		 * way, and bCurStrict is that file's mode.` |
|        - | 7654 | `		 *` |
|        - | 7655 | `		 * Unless an INTERNAL function is what reached for this callback (bCallbackWeak):` |
|        - | 7656 | `		 * php has no calling file at that boundary and binds weakly, so` |
|        - | 7657 | ``		 * `array_map('takesInt', ["5"])` from a strict file RUNS there — PHL raised a`` |
|        - | 7658 | `		 * TypeError on valid php, because the ambient bCurStrict was still the strict` |
|        - | 7659 | `		 * caller's. call_user_func / call_user_func_array are php's two forwards and` |
|        - | 7660 | `		 * carry the caller's mode on a map instead. */` |
|  1246901 | 7661 | `		int bCallIsStrict = pCallMap3 ? (pCallMap3->bStrict ? 1 : 0)` |
|   747477 | 7662 | `		                   : (bCallbackWeak ? 0 : (pVm->bCurStrict ? 1 : 0));` |
|   831648 | 7663 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - | 7664 | `			/* ============================================================` |
|        - | 7665 | `			 * Named-argument matching path (PHP 8.0)` |
|        - | 7666 | `			 *` |
|        - | 7667 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - | 7668 | `			 * or position, then install them in the frame.` |
|        - | 7669 | `			 * ============================================================ */` |
|      481 | 7670 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      481 | 7671 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|      481 | 7672 | `			sxi32 iVariadicIdx = -1;` |
|        - | 7673 | `			sxu32 nNonVariadic;` |
|        - | 7674 | `			sxi32 *aSlot;` |
|        - | 7675 | `			sxu8  *aUsed;` |
|        - | 7676 | `			sxu32 i;` |
|        - | 7677 | `			/* Find variadic parameter index */` |
|     1303 | 7678 | `			for( i = 0; i < nFormal; i++ ){` |
|      937 | 7679 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|      114 | 7680 | `					iVariadicIdx = (sxi32)i;` |
|      114 | 7681 | `					break;` |
|        - | 7682 | `				}` |
|      416 | 7683 | `			}` |
|      481 | 7684 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - | 7685 | `			/* Allocate mapping arrays */` |
|      719 | 7686 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      476 | 7687 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|      481 | 7688 | `			if( aSlot == 0 ){` |
|      ! 0 | 7689 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 | 7690 | `				goto Abort;` |
|        - | 7691 | `			}` |
|      481 | 7692 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - | 7693 | `			/* Resolve named arguments to formal parameters */` |
|      719 | 7694 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|      238 | 7695 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|      481 | 7696 | `			if( rc == PH7_ABORT ){` |
|        8 | 7697 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 | 7698 | `				goto Abort;` |
|        - | 7699 | `			}` |
|      475 | 7700 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 7701 | `				/* php's catchable \Error for a bad named argument. The callee` |
|        - | 7702 | `				 * frame is already entered but its body must not run: unwind` |
|        - | 7703 | `				 * exactly like the named-hole ArgumentCountError path below` |
|        - | 7704 | `				 * (release the not-yet-installed actuals — Pass 2's release` |
|        - | 7705 | `				 * loop has not run — pop the callee slot, mark that there is no` |
|        - | 7706 | `				 * callee operand stack, and let VmCallFinish route the throw). */` |
|        - | 7707 | `				sxu32 iRel;` |
|       19 | 7708 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       45 | 7709 | `				for( iRel = 0; iRel < nActual; iRel++ ){` |
|       29 | 7710 | `					PH7_MemObjRelease(&pArg[iRel]);` |
|       16 | 7711 | `				}` |
|       19 | 7712 | `				PH7_MemObjRelease(pTos);` |
|       19 | 7713 | `				pTos = &pTos[-nCallArgs];` |
|       19 | 7714 | `				pFrameStack = 0;` |
|       19 | 7715 | `				goto SkipFuncBody;` |
|        - | 7716 | `			}` |
|        - | 7717 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|        - | 7718 | `			{` |
|        - | 7719 | `			/* php's required watermark for the hole check below, plus the` |
|        - | 7720 | `			 * highest formal slot an actual resolved to: php words a hole` |
|        - | 7721 | ``			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole`` |
|        - | 7722 | `			 * with nothing filled above it gets the positional count message` |
|        - | 7723 | `			 * (zend's RECV arg_num > EX(num_args) distinction). */` |
|        - | 7724 | `			sxu32 nReqNamed;` |
|        - | 7725 | `			sxu32 nNVNamed;` |
|      459 | 7726 | `			sxu32 nMaxFilled = 0;` |
|      459 | 7727 | `			nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);` |
|     1511 | 7728 | `			for( i = 0; i < nActual; i++ ){` |
|     1057 | 7729 | `				if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|      475 | 7730 | `					nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|      235 | 7731 | `				}` |
|      531 | 7732 | `			}` |
|     1211 | 7733 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - | 7734 | `				/* Find the stack arg mapped to formal n */` |
|      771 | 7735 | `				sxi32 iSrc = -1;` |
|     1223 | 7736 | `				for( i = 0; i < nActual; i++ ){` |
|     1007 | 7737 | `					if( aSlot[i] == (sxi32)n ){` |
|      555 | 7738 | `						iSrc = (sxi32)i;` |
|      555 | 7739 | `						break;` |
|        - | 7740 | `					}` |
|      231 | 7741 | `				}` |
|      771 | 7742 | `				if( iSrc >= 0 ){` |
|        - | 7743 | `					/* Argument was provided — install with type checking */` |
|      555 | 7744 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - | 7745 | `					/* An explicit null is NOT redirected to the default: PHP applies a` |
|        - | 7746 | `					 * default only for an OMITTED argument. An explicit null falls through` |
|        - | 7747 | `					 * to the type check below (TypeError for a non-nullable typed param,` |
|        - | 7748 | ``					 * kept as null for a typeless one). An implicitly-nullable `Type $x =`` |
|        - | 7749 | ``					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */`` |
|        - | 7750 | `					/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 7751 | `					 * coercion and whole-real materialization in place): the shared` |
|        - | 7752 | `					 * per-argument helper — one implementation for both OP_CALL` |
|        - | 7753 | `					 * paths and the generator/fiber binder (a recorded fold). */` |
|      555 | 7754 | `					iArgPreFlags = pVal->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|      555 | 7755 | `					rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pVal,bCallIsStrict,pSelfHint);` |
|      555 | 7756 | `					if( rc != SXRET_OK ){` |
|        7 | 7757 | `						if( rc == PH7_ABORT ) goto Abort;` |
|        7 | 7758 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 | 7759 | `						PH7_MemObjRelease(pTos);` |
|        7 | 7760 | `						pTos = &pTos[-nCallArgs];` |
|        7 | 7761 | `						pFrameStack = 0;` |
|        7 | 7762 | `						rc = PH7_EXCEPTION;` |
|       11 | 7763 | `						goto SkipFuncBody;` |
|        - | 7764 | `					}` |
|        - | 7765 | `					/* Install: by reference or by value */` |
|      549 | 7766 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|       28 | 7767 | `						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){` |
|        - | 7768 | `							/* php 8.1: $GLOBALS cannot be passed by reference */` |
|        - | 7769 | `							SyBlob sMsg;` |
|      ! 0 | 7770 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 7771 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 7772 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 7773 | `							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|      ! 0 | 7774 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 7775 | `								goto Abort;` |
|        - | 7776 | `							}` |
|      ! 0 | 7777 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 7778 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 7779 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 7780 | `							pFrameStack = 0;` |
|      ! 0 | 7781 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 7782 | `							goto SkipFuncBody;` |
|        - | 7783 | `						}` |
|       28 | 7784 | `						if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)iSrc,pVal) ){` |
|        - | 7785 | `							/* php refuses a by-ref argument whose EXPRESSION is not a variable, at` |
|        - | 7786 | `							 * the call and before the callee runs. Deciding it from the VALUE that` |
|        - | 7787 | `							 * arrived was wrong both ways: an operator result carries its LEFT` |
|        - | 7788 | ``							 * operand's slot, so `f($i + 1)` aliased and overwrote `$i`; and a`` |
|        - | 7789 | `							 * literal and a CALL result look alike there, where php accepts the` |
|        - | 7790 | `							 * call. VmArgRefusedByRef reads the call site's compile-time shape mask` |
|        - | 7791 | `							 * and falls back to the old runtime test only when there is none. */` |
|        - | 7792 | `							sxi32 rcRef;` |
|        3 | 7793 | `							rcRef = VmThrowByRefRefusal(&(*pVm),` |
|        2 | 7794 | `								(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|        2 | 7795 | `								&pVmFunc->sName,pVmFunc,(sxu32)(n+1),&aFormalArg[n].sName);` |
|        3 | 7796 | `							if( rcRef == PH7_ABORT ){` |
|      ! 0 | 7797 | `								goto Abort;` |
|        - | 7798 | `							}` |
|        - | 7799 | `							/* Same teardown as the type-check refusal above: free the slot map,` |
|        - | 7800 | `							 * release the result slot and pop the actuals, then let SkipFuncBody` |
|        - | 7801 | `							 * route the throw. */` |
|        3 | 7802 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 7803 | `							PH7_MemObjRelease(pTos);` |
|        3 | 7804 | `							pTos = &pTos[-nCallArgs];` |
|        3 | 7805 | `							pFrameStack = 0;` |
|        3 | 7806 | `							rc = PH7_EXCEPTION;` |
|        3 | 7807 | `							goto SkipFuncBody;` |
|        - | 7808 | `						}` |
|       25 | 7809 | `						PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)iSrc,pVal);` |
|       25 | 7810 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 | 7811 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 7812 | `						}else{` |
|        - | 7813 | `							SyHashEntry *pRefEntry;` |
|        - | 7814 | `							/* The declared type's conversion is what the reference holds. */` |
|       25 | 7815 | `							PH7_VmByRefArgWriteBack(&(*pVm),pVal,iArgPreFlags);` |
|       37 | 7816 | `							pRefEntry = SyHashGet(&pFrame->hVar,` |
|       24 | 7817 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 | 7818 | `							if( pRefEntry == 0 ){` |
|       37 | 7819 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 | 7820 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|       25 | 7821 | `								sArg.nIdx = pVal->nIdx;` |
|       25 | 7822 | `								sArg.pUserData = 0;` |
|       25 | 7823 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 | 7824 | `							}` |
|       25 | 7825 | `							pObj = 0;` |
|        - | 7826 | `						}` |
|       13 | 7827 | `					}else{` |
|      523 | 7828 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 7829 | `					}` |
|      547 | 7830 | `					if( pObj ){` |
|      523 | 7831 | `						PH7_MemObjStore(pVal,pObj);` |
|      523 | 7832 | `						sArg.nIdx = pObj->nIdx;` |
|      523 | 7833 | `						sArg.pUserData = 0;` |
|      523 | 7834 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      259 | 7835 | `					}` |
|      276 | 7836 | `				}else{` |
|        - | 7837 | `					/* Argument was NOT provided — use default or leave unset */` |
|      220 | 7838 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 7839 | `						/* Should not reach here; variadic handled separately below */` |
|      220 | 7840 | `					}else if( n < nReqNamed ){` |
|        - | 7841 | `						/* php's implicit-required rule applies to named calls` |
|        - | 7842 | `						 * too: a hole below the required watermark throws even` |
|        - | 7843 | `						 * when the formal carries a default — f($a,$b=2,$c)` |
|        - | 7844 | ``						 * called as f(a:1,c:3) is `Argument #2 ($b) not`` |
|        - | 7845 | ``						 * passed` (a hole with NO default is always below the`` |
|        - | 7846 | `						 * watermark, so this subsumes the no-default case).` |
|        - | 7847 | `						 * A hole with nothing filled ABOVE it uses php's` |
|        - | 7848 | `						 * positional count wording instead. The passed stack` |
|        - | 7849 | `						 * args were not released yet on this path (that loop` |
|        - | 7850 | `						 * runs after Pass 2) — release them before the exit. */` |
|        7 | 7851 | `						if( n + 1 > nMaxFilled ){` |
|        3 | 7852 | `							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)` |
|      ! 0 | 7853 | `								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 7854 | `									nMaxFilled,nReqNamed,SySetUsed(&pVmFunc->aArgs))` |
|        3 | 7855 | `								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,pVmFunc,` |
|        1 | 7856 | `									nMaxFilled,nReqNamed,nNVNamed,!bCallbackWeak);` |
|        2 | 7857 | `						}else{` |
|        5 | 7858 | `							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,pVmFunc,n+1,&aFormalArg[n].sName);` |
|        - | 7859 | `						}` |
|        7 | 7860 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       15 | 7861 | `						for( i = 0; i < nActual; i++ ){` |
|        9 | 7862 | `							PH7_MemObjRelease(&pArg[i]);` |
|        5 | 7863 | `						}` |
|        7 | 7864 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 7865 | `							goto Abort;` |
|        - | 7866 | `						}` |
|        7 | 7867 | `						PH7_MemObjRelease(pTos);` |
|        7 | 7868 | `						pTos = &pTos[-nCallArgs];` |
|        7 | 7869 | `						pFrameStack = 0;` |
|        7 | 7870 | `						rc = PH7_EXCEPTION;` |
|        7 | 7871 | `						goto SkipFuncBody;` |
|      214 | 7872 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      214 | 7873 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      214 | 7874 | `						if( pObj ){` |
|        - | 7875 | `							VmDefaultScope sDefScope;` |
|      214 | 7876 | `							VmDefaultScopeEnter(&(*pVm),pFrame,pVmFunc,pSelf,&sDefScope);` |
|      214 | 7877 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|      214 | 7878 | `							VmDefaultScopeLeave(&(*pVm),&sDefScope);` |
|      214 | 7879 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      214 | 7880 | `							sArg.nIdx = pObj->nIdx;` |
|      214 | 7881 | `							sArg.pUserData = 0;` |
|      214 | 7882 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 7883 | `							/* A null default on an implicitly-nullable param must stay null` |
|        - | 7884 | `							 * (see the positional-path note above). */` |
|      210 | 7885 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|       44 | 7886 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0` |
|       26 | 7887 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 7888 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 7889 | `								if( xCast ) xCast(pObj);` |
|      ! 0 | 7890 | `							}else{` |
|        - | 7891 | `								/* Mask matched — a const-indirected whole-real default` |
|        - | 7892 | ``								 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|      214 | 7893 | `								VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 7894 | `							}` |
|      105 | 7895 | `						}` |
|      105 | 7896 | `					}` |
|        - | 7897 | `				}` |
|      381 | 7898 | `			}` |
|        - | 7899 | `			} /* end nReqNamed scope */` |
|        - | 7900 | `			/* Handle variadic parameter */` |
|      445 | 7901 | `			if( iVariadicIdx >= 0 ){` |
|      114 | 7902 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|      114 | 7903 | `				if( pObj ){` |
|        - | 7904 | `					/* Capture the slot index now: PH7_HashmapInsert below can` |
|        - | 7905 | `					 * PH7_ReserveMemObj, which used to reallocate pVm->aMemObj and` |
|        - | 7906 | `					 * dangle pObj. Redundant since P1 -- the pool's segments are` |
|        - | 7907 | `					 * fixed, so a slot's address never moves. Left for the harvest` |
|        - | 7908 | `					 * sweep. */` |
|        - | 7909 | `					sxu32 nVariadicSlot;` |
|      114 | 7910 | `					PH7_MemObjToHashmap(pObj);` |
|      114 | 7911 | `					nVariadicSlot = pObj->nIdx;` |
|        - | 7912 | `					{` |
|      114 | 7913 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - | 7914 | `						/* php numbers a failing NAMED variadic element as` |
|        - | 7915 | `						 * max(total positional args, declared non-variadic` |
|        - | 7916 | `						 * formals) + 1 — zend's RECV slots always count —` |
|        - | 7917 | `						 * whichever named element fails; a POSITIONAL element` |
|        - | 7918 | `						 * uses its own 1-based call position. */` |
|      114 | 7919 | `						sxu32 nPositional = 0;` |
|      660 | 7920 | `						for( i = 0; i < nActual; i++ ){` |
|      550 | 7921 | `							if( !(i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0) ){` |
|      333 | 7922 | `								nPositional++;` |
|      165 | 7923 | `							}` |
|      277 | 7924 | `						}` |
|      616 | 7925 | `						for( i = 0; i < nActual; i++ ){` |
|      544 | 7926 | `							if( aSlot[i] == -1 ){` |
|      498 | 7927 | `								int bNamed = (i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0);` |
|      498 | 7928 | `								int bRefElem = 0; /* alias this entry to the caller's slot? */` |
|        - | 7929 | `								/* Same per-element type check + weak coercion as the` |
|        - | 7930 | ``								 * positional-only path (shared helper; no `($name)`). */`` |
|      745 | 7931 | `								rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|      494 | 7932 | `									&aFormalArg[iVariadicIdx],&pArg[i],` |
|      341 | 7933 | `									bNamed ? SXMAX(nPositional,nNonVariadic) + 1 : i + 1,bCallIsStrict);` |
|      498 | 7934 | `								if( rc != SXRET_OK ){` |
|       39 | 7935 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 7936 | `										goto Abort;` |
|        - | 7937 | `									}` |
|       39 | 7938 | `									SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       39 | 7939 | `									PH7_MemObjRelease(pTos);` |
|       39 | 7940 | `									pTos = &pTos[-nCallArgs];` |
|       39 | 7941 | `									pFrameStack = 0;` |
|       39 | 7942 | `									rc = PH7_EXCEPTION;` |
|       39 | 7943 | `									goto SkipFuncBody;` |
|        - | 7944 | `								}` |
|      462 | 7945 | `								if( aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7946 | `									/* php screens a by-ref VARIADIC tail per collected element, in its` |
|        - | 7947 | `									 * no-name wording: a variadic has no per-element parameter name, so` |
|        - | 7948 | ``									 * php says `Argument #N could not be passed by reference` and stops`` |
|        - | 7949 | `									 * there. Nothing screened this arm at all — the branch that collects` |
|        - | 7950 | `									 * a variadic runs before the by-ref binder ever sees a formal. */` |
|        8 | 7951 | `									if( PH7_VmArgRefusedByRef(pCallMap3,i,&pArg[i]) ){` |
|        - | 7952 | `										SyBlob sMsgV;` |
|        - | 7953 | `										sxi32 rcV;` |
|        3 | 7954 | `										SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        3 | 7955 | `										SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        2 | 7956 | `											&pVmFunc->sName,(unsigned)(i + 1));` |
|        3 | 7957 | `										rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        3 | 7958 | `										if( rcV == PH7_ABORT ){` |
|      ! 0 | 7959 | `											goto Abort;` |
|        - | 7960 | `										}` |
|        3 | 7961 | `										SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 7962 | `										PH7_MemObjRelease(pTos);` |
|        3 | 7963 | `										pTos = &pTos[-nCallArgs];` |
|        3 | 7964 | `										pFrameStack = 0;` |
|        3 | 7965 | `										rc = PH7_EXCEPTION;` |
|        3 | 7966 | `										goto SkipFuncBody;` |
|        - | 7967 | `									}` |
|        5 | 7968 | `									PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,i,&pArg[i]);` |
|        2 | 7969 | `								}` |
|        - | 7970 | `								/* A by-ref variadic tail ALIASES its actuals, named entries` |
|        - | 7971 | `								 * included (the positional twin below this branch says why). */` |
|      690 | 7972 | `								bRefElem = (aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF)` |
|      456 | 7973 | `									&& pArg[i].nIdx != SXU32_HIGH;` |
|      460 | 7974 | `								if( bNamed ){` |
|        - | 7975 | `									/* Named variadic entry: insert with string key */` |
|        - | 7976 | `									ph7_value sKey;` |
|      158 | 7977 | `									PH7_MemObjInit(pVm, &sKey);` |
|      158 | 7978 | `									PH7_MemObjStringAppend(&sKey,` |
|      154 | 7979 | `										pCallMap3->aNames[i].zString,` |
|      154 | 7980 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|      158 | 7981 | `									if( bRefElem ){` |
|        5 | 7982 | `										PH7_HashmapInsertByRef(pVarMap, &sKey, pArg[i].nIdx);` |
|        3 | 7983 | `									}else{` |
|      154 | 7984 | `										PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|        - | 7985 | `									}` |
|      158 | 7986 | `									PH7_MemObjRelease(&sKey);` |
|      382 | 7987 | `								}else if( bRefElem ){` |
|        - | 7988 | `									/* Positional variadic entry, aliased */` |
|      ! 0 | 7989 | `									PH7_HashmapInsertByRef(pVarMap, 0, pArg[i].nIdx);` |
|      ! 0 | 7990 | `								}else{` |
|        - | 7991 | `									/* Positional variadic entry */` |
|      305 | 7992 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - | 7993 | `								}` |
|      228 | 7994 | `							}` |
|      255 | 7995 | `						}` |
|        - | 7996 | `					}` |
|       76 | 7997 | `					sArg.nIdx = nVariadicSlot; /* the saved index (see above; redundant since P1) */` |
|       76 | 7998 | `					sArg.pUserData = 0;` |
|       76 | 7999 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       36 | 8000 | `				}` |
|       40 | 8001 | `			}else{` |
|        - | 8002 | `				/* No variadic — preserve unresolved positional overflow` |
|        - | 8003 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - | 8004 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - | 8005 | `				 * the positional-only path's behavior. */` |
|      335 | 8006 | `				sxu32 nAnon = nNonVariadic;` |
|      823 | 8007 | `				for( i = 0; i < nActual; i++ ){` |
|      493 | 8008 | `					if( aSlot[i] == -2 ){` |
|        - | 8009 | `						char zAnonBuf[32];` |
|        - | 8010 | `						SyString sAnonName;` |
|      ! 0 | 8011 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 | 8012 | `							"[%u]apArg",nAnon);` |
|      ! 0 | 8013 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 | 8014 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 | 8015 | `						if( pObj ){` |
|      ! 0 | 8016 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 | 8017 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 | 8018 | `							sArg.pUserData = 0;` |
|      ! 0 | 8019 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 | 8020 | `						}` |
|      ! 0 | 8021 | `						nAnon++;` |
|      ! 0 | 8022 | `					}` |
|      249 | 8023 | `				}` |
|        - | 8024 | `			}` |
|        - | 8025 | `			/* Release all stack arguments */` |
|     1367 | 8026 | `			for( i = 0; i < nActual; i++ ){` |
|      965 | 8027 | `				PH7_MemObjRelease(&pArg[i]);` |
|      485 | 8028 | `			}` |
|      407 | 8029 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - | 8030 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|      407 | 8031 | `			n = nFormal;` |
|      206 | 8032 | `		}else{` |
|        - | 8033 | `		/* ============================================================` |
|        - | 8034 | `		 * Positional-only matching path (original)` |
|        - | 8035 | `		 * ============================================================ */` |
|        - | 8036 | `		/* Base of the actual argument stack, for php's per-ELEMENT argument` |
|        - | 8037 | `		 * number in a variadic type-error (php numbers a variadic-collected` |
|        - | 8038 | `		 * element by its overall 1-based call position, not the formal index). */` |
|   831172 | 8039 | `		ph7_value *pArgBase = pArg;` |
|   831172 | 8040 | `		n = 0;` |
|  1193298 | 8041 | `		while( pArg < pTos ){` |
|   367304 | 8042 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - | 8043 | `				/* Variadic parameter: collect all remaining args into an array */` |
|      772 | 8044 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      772 | 8045 | `				if( pObj ){` |
|        - | 8046 | `					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,` |
|        - | 8047 | `					 * which used to reallocate pVm->aMemObj and dangle pObj (a real UAF, masked by` |
|        - | 8048 | `					 * the pool allocator). Redundant since P1 -- the pool's segments are fixed, so` |
|        - | 8049 | `					 * a slot's address never moves. Left for the harvest sweep. */` |
|        - | 8050 | `					sxu32 nVariadicIdx;` |
|        - | 8051 | `					/* Initialize as empty array */` |
|      772 | 8052 | `					PH7_MemObjToHashmap(pObj);` |
|      772 | 8053 | `					nVariadicIdx = pObj->nIdx;` |
|        - | 8054 | `					{` |
|      772 | 8055 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     3389 | 8056 | `						while( pArg < pTos ){` |
|        - | 8057 | `							/* Per-element type check + weak coercion (shared helper,` |
|        - | 8058 | `							 * also used by the named-argument path). The argument` |
|        - | 8059 | `							 * number is the element's overall 1-based call position` |
|        - | 8060 | `` 							 * ((pArg - pArgBase) + 1) and, like php, the `($name)` `` |
|        - | 8061 | `							 * clause is omitted. */` |
|     3890 | 8062 | `							rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|     2665 | 8063 | `								&aFormalArg[n],pArg,(sxu32)(pArg - pArgBase) + 1,bCallIsStrict);` |
|     2670 | 8064 | `							if( rc != SXRET_OK ){` |
|       47 | 8065 | `								if( rc == PH7_ABORT ){` |
|      ! 0 | 8066 | `									goto Abort;` |
|        - | 8067 | `								}` |
|        - | 8068 | `								/* Skip function body, route through normal cleanup */` |
|       47 | 8069 | `								PH7_MemObjRelease(pTos);` |
|       47 | 8070 | `								pTos = &pTos[-nCallArgs];` |
|       47 | 8071 | `								pFrameStack = 0;` |
|       47 | 8072 | `								rc = PH7_EXCEPTION;` |
|       47 | 8073 | `								goto SkipFuncBody;` |
|        - | 8074 | `							}` |
|     2628 | 8075 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 8076 | `								/* The positional twin of the named path's variadic screen above:` |
|        - | 8077 | `								 * php refuses a non-variable collected into a by-ref variadic tail,` |
|        - | 8078 | `								 * in its no-name wording. */` |
|       52 | 8079 | `								sxu32 nPosV = (sxu32)(pArg - pArgBase);` |
|       52 | 8080 | `								if( PH7_VmArgRefusedByRef(pCallMap3,nPosV,pArg) ){` |
|        - | 8081 | `									SyBlob sMsgV;` |
|        - | 8082 | `									sxi32 rcV;` |
|        7 | 8083 | `									SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        7 | 8084 | `									SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        6 | 8085 | `										&pVmFunc->sName,(unsigned)(nPosV + 1));` |
|        7 | 8086 | `									rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        7 | 8087 | `									if( rcV == PH7_ABORT ){` |
|      ! 0 | 8088 | `										goto Abort;` |
|        - | 8089 | `									}` |
|        7 | 8090 | `									PH7_MemObjRelease(pTos);` |
|        7 | 8091 | `									pTos = &pTos[-nCallArgs];` |
|        7 | 8092 | `									pFrameStack = 0;` |
|        7 | 8093 | `									rc = PH7_EXCEPTION;` |
|        7 | 8094 | `									goto SkipFuncBody;` |
|        - | 8095 | `								}` |
|       46 | 8096 | `								PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,nPosV,pArg);` |
|       46 | 8097 | `								if( pArg->nIdx != SXU32_HIGH ){` |
|        - | 8098 | `									/* php ALIASES each collected element to the caller's slot:` |
|        - | 8099 | ``									 * `function f(&...$xs){ $xs[0] = 'A'; }` writes back, and`` |
|        - | 8100 | ``									 * var_dump($xs) inside the callee shows `&int(1)`. Copying`` |
|        - | 8101 | `									 * them left every actual untouched. The node counts as a` |
|        - | 8102 | `									 * holder of the caller's slot, so the frame teardown that` |
|        - | 8103 | `									 * destroys the variadic array gives the hold back. */` |
|       42 | 8104 | `									PH7_HashmapInsertByRef(pMap, 0, pArg->nIdx);` |
|       42 | 8105 | `									pArg++;` |
|       42 | 8106 | `									continue;` |
|        - | 8107 | `								}` |
|        2 | 8108 | `							}` |
|     2582 | 8109 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|     2582 | 8110 | `							pArg++;` |
|        5 | 8111 | `						}` |
|        - | 8112 | `					}` |
|      724 | 8113 | `					sArg.nIdx = nVariadicIdx; /* the saved index (see above; redundant since P1) */` |
|      724 | 8114 | `					sArg.pUserData = 0;` |
|      724 | 8115 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      312 | 8116 | `				}` |
|      724 | 8117 | `				break; /* All remaining args consumed */` |
|        - | 8118 | `			}` |
|   366537 | 8119 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 8120 | `				/* An explicit null is NOT redirected to the default (PHP applies a` |
|        - | 8121 | `				 * default only for an omitted arg); it falls through to the type check` |
|        - | 8122 | `				 * below — TypeError for a non-nullable typed param, kept as null for a` |
|        - | 8123 | ``				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE`` |
|        - | 8124 | `				 * at compile time so its check accepts null. */` |
|        - | 8125 | `				/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 8126 | `				 * coercion and whole-real materialization in place — nullable` |
|        - | 8127 | `				 * types (?type) let null through): the shared per-argument` |
|        - | 8128 | `				 * helper, one implementation for both OP_CALL paths and the` |
|        - | 8129 | `				 * generator/fiber binder (a recorded fold). */` |
|   352203 | 8130 | `				iArgPreFlags = pArg->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|   352203 | 8131 | `				rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pArg,bCallIsStrict,pSelfHint);` |
|   352203 | 8132 | `				if( rc != SXRET_OK ){` |
|      387 | 8133 | `					if( rc == PH7_ABORT ){` |
|        6 | 8134 | `						goto Abort;` |
|        - | 8135 | `					}` |
|        - | 8136 | `					/* Skip function body, route through normal cleanup */` |
|      383 | 8137 | `					PH7_MemObjRelease(pTos);` |
|      383 | 8138 | `					pTos = &pTos[-nCallArgs];` |
|      383 | 8139 | `					pFrameStack = 0;` |
|      383 | 8140 | `					rc = PH7_EXCEPTION;` |
|      383 | 8141 | `					goto SkipFuncBody;` |
|        - | 8142 | `				}` |
|   351821 | 8143 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 8144 | `					/* Pass by reference */` |
|     8067 | 8145 | `					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){` |
|        - | 8146 | `						/* php 8.1: $GLOBALS cannot be passed by reference —` |
|        - | 8147 | `						 * a catchable Error with php's exact wording. */` |
|        - | 8148 | `						SyBlob sMsg;` |
|        3 | 8149 | `						SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 8150 | `						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|        2 | 8151 | `							&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        3 | 8152 | `						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 8153 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 8154 | `							goto Abort;` |
|        - | 8155 | `						}` |
|        3 | 8156 | `						PH7_MemObjRelease(pTos);` |
|        3 | 8157 | `						pTos = &pTos[-nCallArgs];` |
|        3 | 8158 | `						pFrameStack = 0;` |
|        3 | 8159 | `						rc = PH7_EXCEPTION;` |
|        3 | 8160 | `						goto SkipFuncBody;` |
|        - | 8161 | `					}` |
|     8065 | 8162 | `					if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)n,pArg) ){` |
|        - | 8163 | `						/* php's refusal, decided from the argument's compile-time SHAPE (the` |
|        - | 8164 | `						 * companion of the named-argument binder above; see VmArgRefusedByRef). */` |
|        - | 8165 | `						sxi32 rcRef;` |
|     6035 | 8166 | `						rcRef = VmThrowByRefRefusal(&(*pVm),` |
|     4022 | 8167 | `							(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|     4022 | 8168 | `							&pVmFunc->sName,pVmFunc,(sxu32)(n+1),&aFormalArg[n].sName);` |
|     4024 | 8169 | `						if( rcRef == PH7_ABORT ){` |
|      ! 0 | 8170 | `							goto Abort;` |
|        - | 8171 | `						}` |
|        - | 8172 | `						/* Route the throw like every other binder refusal: release the result` |
|        - | 8173 | `						 * slot, pop the actuals and let SkipFuncBody finish the call. Returning` |
|        - | 8174 | `						 * from here walked out of the dispatch loop with the callee's frame and` |
|        - | 8175 | `						 * stack still live, so a CAUGHT refusal silently abandoned every` |
|        - | 8176 | `						 * statement after the catch. */` |
|     4024 | 8177 | `						PH7_MemObjRelease(pTos);` |
|     4024 | 8178 | `						pTos = &pTos[-nCallArgs];` |
|     4024 | 8179 | `						pFrameStack = 0;` |
|     4024 | 8180 | `						rc = PH7_EXCEPTION;` |
|     4024 | 8181 | `						goto SkipFuncBody;` |
|        - | 8182 | `					}` |
|     4043 | 8183 | `					PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)n,pArg);` |
|     4043 | 8184 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - | 8185 | `						/* Nothing to alias: pass by value. */` |
|      101 | 8186 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       52 | 8187 | `					}else{` |
|        - | 8188 | `						SyHashEntry *pRefEntry;` |
|        - | 8189 | `						/* The declared type's conversion is what the reference holds. */` |
|     3945 | 8190 | `						PH7_VmByRefArgWriteBack(&(*pVm),pArg,iArgPreFlags);` |
|        - | 8191 | `						/* Install the referenced variable in the private function frame */` |
|     3945 | 8192 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|     3945 | 8193 | `						if( pRefEntry == 0 ){` |
|     5911 | 8194 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|     3940 | 8195 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|     3945 | 8196 | `							sArg.nIdx = pArg->nIdx;` |
|     3945 | 8197 | `							sArg.pUserData = 0;` |
|     3945 | 8198 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     1966 | 8199 | `						}` |
|     3945 | 8200 | `						pObj = 0;` |
|        - | 8201 | `					}` |
|     2020 | 8202 | `				}else{` |
|        - | 8203 | `					/* Pass by value,make a copy of the given argument */` |
|   343759 | 8204 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 8205 | `				}` |
|   174689 | 8206 | `			}else{` |
|        - | 8207 | `				char zName[32];` |
|        - | 8208 | `				SyString sArgName;` |
|        - | 8209 | `				/* Set a dummy name */` |
|    14339 | 8210 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|    14339 | 8211 | `				sArgName.zString = zName;` |
|        - | 8212 | `				/* Annonymous argument */` |
|    14339 | 8213 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 8214 | `			}` |
|   362131 | 8215 | `			if( pObj ){` |
|   358191 | 8216 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 8217 | `				/* Insert argument index  */` |
|   358191 | 8218 | `				sArg.nIdx = pObj->nIdx;` |
|   358191 | 8219 | `				sArg.pUserData = 0;` |
|   358191 | 8220 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|   179845 | 8221 | `			}` |
|   362131 | 8222 | `			PH7_MemObjRelease(pArg);` |
|   362131 | 8223 | `			pArg++;` |
|   362131 | 8224 | `			++n;` |
|        5 | 8225 | `		}` |
|        - | 8226 | `		} /* end named vs positional branch */` |
|        - | 8227 | `		/* Set up closure environment */` |
|   827120 | 8228 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 8229 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 8230 | `			ph7_value *pValue;` |
|        - | 8231 | `			sxu32 iEnv;` |
|    37604 | 8232 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|    88484 | 8233 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|    50885 | 8234 | `				pEnv = &aEnv[iEnv];` |
|    50885 | 8235 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 8236 | `					/* Do not install null value */` |
|    36540 | 8237 | `					continue;` |
|        - | 8238 | `				}` |
|    14345 | 8239 | `				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       13 | 8240 | `				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|        - | 8241 | `					/* The Closure instance carries an explicit bound $this` |
|        - | 8242 | `					 * (bindTo/bind/call): it wins over the creation-time` |
|        - | 8243 | `					 * captured $this, php-exact. */` |
|        7 | 8244 | `					continue;` |
|        - | 8245 | `				}` |
|    14344 | 8246 | `				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|        - | 8247 | `					/* Captured by reference: link the name to the shared slot` |
|        - | 8248 | `					 * (no copy), mirroring the by-ref argument install above. */` |
|     4082 | 8249 | `					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|     6106 | 8250 | `						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),` |
|     4077 | 8251 | `							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|     2024 | 8252 | `					}` |
|     4082 | 8253 | `					continue;` |
|        - | 8254 | `				}` |
|    10267 | 8255 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|    10267 | 8256 | `				if( pValue == 0 ){` |
|      ! 0 | 8257 | `					continue;` |
|        - | 8258 | `				}` |
|        - | 8259 | `				/* Invalidate any prior representation */` |
|    10267 | 8260 | `				PH7_MemObjRelease(pValue);` |
|        - | 8261 | `				/* Duplicate bound variable value */` |
|    10267 | 8262 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|     5073 | 8263 | `			}` |
|    18572 | 8264 | `		}` |
|        - | 8265 | `		/* Too-few-arguments check, placed AFTER the passed arguments were` |
|        - | 8266 | `		 * installed and type-checked: php's RECV order means a type error on` |
|        - | 8267 | ``		 * a PASSED argument beats the count error (`f(int $x,$y)` called`` |
|        - | 8268 | `		 * f("str") is a TypeError, not ArgumentCountError). The passed args` |
|        - | 8269 | `		 * were already released by the install loop, so the standard throw` |
|        - | 8270 | `		 * exit leaks nothing. The named path never fires this (its per-hole` |
|        - | 8271 | `		 * check ran in-loop; n == nNonVariadic >= nRequired here).` |
|        - | 8272 | `		 * Builtins written as embedded PHP in the prelude (VM_FUNC_INTERNAL,` |
|        - | 8273 | `		 * with or without VM_FUNC_CLASS_METHOD) are NOT exempt: their declared` |
|        - | 8274 | `		 * signatures were swept against the php 8.5 oracle, so the derived` |
|        - | 8275 | `		 * required count and wording are php's, and VmThrowBuiltinTooFewArgs` |
|        - | 8276 | `		 * words them as php words an internal callable. */` |
|   827120 | 8277 | `		if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 8278 | `			sxu32 nNonVar,nReq;` |
|     7723 | 8279 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     7723 | 8280 | `			if( n < nReq ){` |
|       73 | 8281 | `				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;` |
|       73 | 8282 | `				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|       37 | 8283 | `					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       12 | 8284 | `						nPassed,nReq,SySetUsed(&pVmFunc->aArgs));` |
|       13 | 8285 | `				}else{` |
|        - | 8286 | `					/* php names the call SITE only when the caller is user code: an` |
|        - | 8287 | `					 * INTERNAL function reaching for a callback (array_map, usort,` |
|        - | 8288 | `					 * an autoloader) has no calling line to name, which is exactly` |
|        - | 8289 | `					 * what bCallbackWeak already marks. */` |
|       72 | 8290 | `					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,pVmFunc,` |
|       23 | 8291 | `						nPassed,nReq,nNonVar,!bCallbackWeak);` |
|        - | 8292 | `				}` |
|       73 | 8293 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 8294 | `					goto Abort;` |
|        - | 8295 | `				}` |
|       73 | 8296 | `				PH7_MemObjRelease(pTos);` |
|       73 | 8297 | `				pTos = &pTos[-nCallArgs];` |
|       73 | 8298 | `				pFrameStack = 0;` |
|       73 | 8299 | `				rc = PH7_EXCEPTION;` |
|       73 | 8300 | `				goto SkipFuncBody;` |
|        5 | 8301 | `			}` |
|   823169 | 8302 | `		}else if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) == VM_FUNC_INTERNAL ){` |
|        - | 8303 | `			/* Too-MANY arguments, the mirror php enforces for an internal` |
|        - | 8304 | `			 * callable ("count_chars() expects at most 2 arguments, 3 given").` |
|        - | 8305 | `			 * PHL accepted the extras silently — the surplus only ever reached` |
|        - | 8306 | `			 * func_get_args(). Every formal is filled on this branch, so the call` |
|        - | 8307 | `			 * is over the maximum exactly when more actuals arrived than there are` |
|        - | 8308 | `			 * non-variadic formals; a VARIADIC tail means php declares no maximum` |
|        - | 8309 | `			 * at all, so nNonVar below the formal count is exempt. Prelude` |
|        - | 8310 | `			 * FUNCTIONS only: the chunk-backed class METHODS were not swept against` |
|        - | 8311 | `			 * php's signatures (Fiber::start() is declared argless and reads` |
|        - | 8312 | `			 * func_get_args()). */` |
|        - | 8313 | `			sxu32 nNonVar,nReq;` |
|     2529 | 8314 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     2524 | 8315 | `			if( nNonVar == SySetUsed(&pVmFunc->aArgs)` |
|     2523 | 8316 | `			 && pFrame->nActualArgs >= 0` |
|     2527 | 8317 | `			 && (sxu32)pFrame->nActualArgs > nNonVar ){` |
|       34 | 8318 | `				rc = VmThrowBuiltinTooManyArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       22 | 8319 | `					(sxu32)pFrame->nActualArgs,nNonVar,nReq);` |
|       23 | 8320 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 8321 | `					goto Abort;` |
|        - | 8322 | `				}` |
|       23 | 8323 | `				PH7_MemObjRelease(pTos);` |
|       23 | 8324 | `				pTos = &pTos[-nCallArgs];` |
|       23 | 8325 | `				pFrameStack = 0;` |
|       23 | 8326 | `				rc = PH7_EXCEPTION;` |
|       23 | 8327 | `				goto SkipFuncBody;` |
|        - | 8328 | `			}` |
|     1250 | 8329 | `		}` |
|        - | 8330 | `		/* Process default values for remaining formal parameters */` |
|   844985 | 8331 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    18801 | 8332 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 8333 | `				/* Variadic parameter with no extra args — create empty array */` |
|      844 | 8334 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      844 | 8335 | `				if( pObj ){` |
|      844 | 8336 | `					PH7_MemObjToHashmap(pObj);` |
|      844 | 8337 | `					sArg.nIdx = pObj->nIdx;` |
|      844 | 8338 | `					sArg.pUserData = 0;` |
|      844 | 8339 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      372 | 8340 | `				}` |
|      844 | 8341 | `				n++;` |
|      844 | 8342 | `				break; /* Variadic is always last */` |
|        - | 8343 | `			}` |
|    17962 | 8344 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|    17962 | 8345 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|    17962 | 8346 | `				if( pObj ){` |
|        - | 8347 | `					/* Evaluate the default value and extract it's result */` |
|        - | 8348 | `					VmDefaultScope sDefScope;` |
|    17962 | 8349 | `					VmDefaultScopeEnter(&(*pVm),pFrame,pVmFunc,pSelf,&sDefScope);` |
|    17962 | 8350 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|    17962 | 8351 | `					VmDefaultScopeLeave(&(*pVm),&sDefScope);` |
|    17962 | 8352 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 8353 | `						goto Abort;` |
|        - | 8354 | `					}` |
|        - | 8355 | `					/* Insert argument index */` |
|    17962 | 8356 | `					sArg.nIdx = pObj->nIdx;` |
|    17962 | 8357 | `					sArg.pUserData = 0;` |
|    17962 | 8358 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 8359 | `					/* Make sure the default argument is of the correct type.` |
|        - | 8360 | ``					 * A null default on an implicitly-nullable param (`int $x = null`)`` |
|        - | 8361 | `					 * must stay null — casting it to 0/""/false would diverge from PHP` |
|        - | 8362 | `					 * and contradict the explicit-null path, which now keeps it null. */` |
|    17957 | 8363 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     2455 | 8364 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)` |
|     1227 | 8365 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 8366 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 8367 | `						/* Cast to the desired type */` |
|      ! 0 | 8368 | `						xCast(pObj);` |
|      ! 0 | 8369 | `					}else{` |
|        - | 8370 | `						/* Mask matched — a const-indirected whole-real default` |
|        - | 8371 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|    17962 | 8372 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 8373 | `					}` |
|     8964 | 8374 | `				}` |
|     8964 | 8375 | `			}` |
|    17962 | 8376 | `			++n;` |
|        5 | 8377 | `		}` |
|        - | 8378 | `		} /* end VmCallArgMap scope */` |
|        - | 8379 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 8380 | `		 * does not return anything.` |
|        - | 8381 | `		 */` |
|   827028 | 8382 | `		PH7_MemObjRelease(pTos);` |
|   827028 | 8383 | `		pTos = &pTos[-nCallArgs];` |
|        - | 8384 | `		/* Allocate an operand stack (via the recycling allocator) and evaluate the` |
|        - | 8385 | `		 * function body. Size it to a tight static bound when the body is statically` |
|        - | 8386 | `		 * modelable — the big memory win for deep recursion,` |
|        - | 8387 | `		 * where one such stack lives per frame — falling back to the safe` |
|        - | 8388 | `		 * instruction-count bound otherwise.` |
|        - | 8389 | `		 *` |
|        - | 8390 | `		 * The bound is computed LAZILY on the first call and cached on the func` |
|        - | 8391 | `		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:` |
|        - | 8392 | `		 * self-computing on first use is fail-safe against any body-creation path` |
|        - | 8393 | `		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),` |
|        - | 8394 | `		 * where undersizing is a heap overflow; the amortized cost is one analysis` |
|        - | 8395 | `		 * per function. */` |
|        - | 8396 | `		{` |
|   827028 | 8397 | `			sxu32 nSlots = pVmFunc->nMaxStack;` |
|   827028 | 8398 | `			if( nSlots == 0 ){` |
|    24330 | 8399 | `				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);` |
|    36364 | 8400 | `				sxu32 nTight = VmComputeMaxStack(&(*pVm),` |
|    24325 | 8401 | `					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);` |
|    24330 | 8402 | `				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;` |
|    24330 | 8403 | `				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */` |
|    24330 | 8404 | `				pVmFunc->nMaxStack = nSlots;` |
|    12034 | 8405 | `			}` |
|   827028 | 8406 | `			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);` |
|        - | 8407 | `		}` |
|   827028 | 8408 | `		if( pFrameStack == 0 ){` |
|        - | 8409 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 8410 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 8411 | `				&pVmFunc->sName);` |
|      ! 0 | 8412 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 8413 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 8414 | `			}` |
|      ! 0 | 8415 | `			break;` |
|        - | 8416 | `		}` |
|   413307 | 8417 | `SkipFuncBody:` |
|   831638 | 8418 | `		if( pSelf ){` |
|        - | 8419 | `			/* Push class name */` |
|   512181 | 8420 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|   256088 | 8421 | `		}` |
|        - | 8422 | `		/* Increment nesting level */` |
|   831638 | 8423 | `		pVm->nRecursionDepth++;` |
|   831638 | 8424 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 8425 | `			/* Arg-binding threw: there is no body to run — finish the call` |
|        - | 8426 | `			 * immediately (no record is pushed). */` |
|        - | 8427 | `			VmCallRecord sCallee;` |
|     4615 | 8428 | `			sCallee.pVmFunc = pVmFunc;` |
|     4615 | 8429 | `			sCallee.pFrame = pFrame;` |
|     4615 | 8430 | `			sCallee.pFrameStack = pFrameStack;` |
|     4615 | 8431 | `			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|        - | 8432 | `			/* The body never ran, so this stack is untouched -- but the path is rare` |
|        - | 8433 | `			 * (an argument's own evaluation threw) and sweeping the whole capacity` |
|        - | 8434 | `			 * costs nothing here, so do that rather than reason about the binder. */` |
|     4615 | 8435 | `			sCallee.nLiveTos = sCallee.nStackCap;` |
|     4615 | 8436 | `			sCallee.nLastRef = SXU32_HIGH;` |
|     4615 | 8437 | `			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|     4615 | 8438 | `			sState.pTos = pTos;` |
|     4615 | 8439 | `			sState.pc = pc;` |
|     4615 | 8440 | `			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);` |
|     4615 | 8441 | `			pTos = sState.pTos;` |
|     4615 | 8442 | `			pc = sState.pc;` |
|     4615 | 8443 | `			if( rc == PH7_ABORT ){` |
|        - | 8444 | `				/* Abort processing immeditaley */` |
|      ! 0 | 8445 | `				goto Abort;` |
|     4615 | 8446 | `			}else if( rc == PH7_SUSPEND ){` |
|      ! 0 | 8447 | `				goto Suspend;` |
|     4615 | 8448 | `			}else if( rc == PH7_EXCEPTION ){` |
|      267 | 8449 | `				goto Exception;` |
|        - | 8450 | `			}` |
|     2179 | 8451 | `		}else{` |
|        - | 8452 | `			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a` |
|        - | 8453 | `			 * call record (caller activation + in-flight call) and switch the` |
|        - | 8454 | `			 * loop's locals to the callee — a PHP->PHP call no longer grows` |
|        - | 8455 | `			 * the native stack. The record node is pool-allocated so` |
|        - | 8456 | `			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */` |
|   827028 | 8457 | `			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   827028 | 8458 | `			if( pRec ){` |
|   822171 | 8459 | `				pVm->pIdleCallFrames = (void *)pRec->pPrev;` |
|   411302 | 8460 | `			}else{` |
|     4862 | 8461 | `				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));` |
|        - | 8462 | `			}` |
|   827028 | 8463 | `			if( pRec == 0 ){` |
|        - | 8464 | `				/* OOM: undo the push-time accounting, tear the call down and` |
|        - | 8465 | `				 * raise the non-catchable fatal (the OOM convention —` |
|        - | 8466 | `				 * never a silent NULL). */` |
|      ! 0 | 8467 | `				pVm->nRecursionDepth--;` |
|      ! 0 | 8468 | `				if( pSelf ){` |
|      ! 0 | 8469 | `					(void)SySetPop(&pVm->aSelf);` |
|      ! 0 | 8470 | `				}` |
|      ! 0 | 8471 | `				SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|      ! 0 | 8472 | `				VmLeaveFrame(&(*pVm));` |
|      ! 0 | 8473 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 8474 | `				goto Abort;` |
|        - | 8475 | `			}` |
|   827028 | 8476 | `			sState.pTos = pTos;` |
|   827028 | 8477 | `			sState.pc = pc;` |
|   827028 | 8478 | `			sState.pHigh = pHigh;   /* the caller's watermark rides with its pTos */` |
|   827028 | 8479 | `			pRec->sCaller = sState;` |
|   827028 | 8480 | `			pRec->sCall.pVmFunc = pVmFunc;` |
|   827028 | 8481 | `			pRec->sCall.pFrame = pFrame;` |
|   827028 | 8482 | `			pRec->sCall.pFrameStack = pFrameStack;` |
|   827028 | 8483 | `			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|        - | 8484 | `			/* Overwritten with the callee's real watermark when the call finishes;` |
|        - | 8485 | `			 * the safe default is "sweep everything", so a path that ever reaches` |
|        - | 8486 | `			 * VmCallFinish without going through the unwind above still cleans the` |
|        - | 8487 | `			 * whole buffer rather than parking a live value in the pool. */` |
|   827028 | 8488 | `			pRec->sCall.nLiveTos = pRec->sCall.nStackCap;` |
|   827028 | 8489 | `			pRec->sCall.nLastRef = SXU32_HIGH;` |
|   827028 | 8490 | `			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|   827028 | 8491 | `			pRec->pPrev = pCallTop;` |
|   827028 | 8492 | `			pCallTop = pRec;` |
|        - | 8493 | `			/* Switch to the callee activation (what the recursive` |
|        - | 8494 | `			 * VmByteCodeExec entry used to set up). */` |
|   827028 | 8495 | `			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);` |
|   827028 | 8496 | `			pStack = pFrameStack;` |
|   827028 | 8497 | `			pTos = &pStack[-1];` |
|   827028 | 8498 | `			pc = 0;` |
|   827028 | 8499 | `			sState.aInstr = aInstr;` |
|   827028 | 8500 | `			sState.pStack = pStack;` |
|   827028 | 8501 | `			pHigh = pTos;             /* the callee starts with an empty stack */` |
|   827028 | 8502 | `			sState.pHigh = pHigh;` |
|   827028 | 8503 | `			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */` |
|   827028 | 8504 | `			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */` |
|   827028 | 8505 | `			sState.pTos = pTos;` |
|   827028 | 8506 | `			sState.pc = 0;` |
|   827028 | 8507 | `			sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|   827028 | 8508 | `			sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|   827028 | 8509 | `			sState.pEntryFrame = pVm->pFrame;` |
|   827028 | 8510 | `			sState.pResult = pRec->sCaller.pTos;` |
|   827028 | 8511 | `			sState.pLastRef = &pRec->sCall.nLastRef;` |
|        - | 8512 | `			/* Carries the callee for BOTH terminal-OP_DONE screens: the declared` |
|        - | 8513 | `			 * return type (which re-tests VmFuncHasReturnType) and the by-reference` |
|        - | 8514 | `			 * return above. */` |
|   827028 | 8515 | `			sState.pEnforceRetFunc = ( VmFuncHasReturnType(pVmFunc)` |
|   827023 | 8516 | `				\|\| (pVmFunc->iFlags & VM_FUNC_REF_RETURN) ) ? pVmFunc : 0;` |
|   827028 | 8517 | `			sState.is_callback = 0;` |
|   827028 | 8518 | `			sState.bReturnPropagates = 0;` |
|   827028 | 8519 | `			goto VmLoopFetch;` |
|        - | 8520 | `		}` |
|     2179 | 8521 | `	}else{` |
|        - | 8522 | `		/* Look for an installed foreign function.` |
|        - | 8523 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 8524 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 8525 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 8526 | `		 * global fallback for unqualified function calls in namespaces. */` |
|  5406209 | 8527 | `		if( pSiteEntry && bSiteHost ){` |
|        - | 8528 | `			/* The site already knows which host entry this name means (see the` |
|        - | 8529 | `			 * VmCallSite consult above the user-table lookup). */` |
|  3839767 | 8530 | `			pEntry = pSiteEntry;` |
|  1921462 | 8531 | `		}else{` |
|        - | 8532 | ``		/* bConstruct: `isset`/`empty`/`unset`/`eval`/`print`/`include*`/`require*` are`` |
|        - | 8533 | `		 * registered host functions here and are no function at all in php, so the` |
|        - | 8534 | `		 * registration answers only the call site the CONSTRUCT's codegen emitted` |
|        - | 8535 | ``		 * (PH7_CALL_CONSTRUCT). A program's own `$f = 'include'; $f($p);` misses and`` |
|        - | 8536 | ``		 * gets php's `Call to undefined function include()`. */`` |
|  1566447 | 8537 | `		int bConstructSite = (pInstr->iP2 & PH7_CALL_CONSTRUCT) != 0;` |
|  1566447 | 8538 | `		pEntry = PH7_VmGetHostFunction(pVm,(const void *)sName.zString,sName.nByte,bConstructSite);` |
|        - | 8539 | `		{` |
|  1566447 | 8540 | `		VmCallArgMap *pCallMap2 = pEffCallMap;` |
|  1566447 | 8541 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 8542 | `			/* Compiler-qualified: try short name as global fallback */` |
|       99 | 8543 | `			const char *zShort = sName.zString;` |
|        - | 8544 | `			sxu32 i;` |
|     1581 | 8545 | `			for( i = 0; i < sName.nByte; i++ ){` |
|     1487 | 8546 | `				if( sName.zString[i] == '\\' ){` |
|      113 | 8547 | `					zShort = &sName.zString[i + 1];` |
|       54 | 8548 | `				}` |
|      746 | 8549 | `			}` |
|       99 | 8550 | `			if( zShort != sName.zString ){` |
|       99 | 8551 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       99 | 8552 | `				pEntry = PH7_VmGetHostFunction(pVm,(const void *)zShort,nShort,bConstructSite);` |
|       47 | 8553 | `			}` |
|       47 | 8554 | `		}` |
|        - | 8555 | `		} /* end VmCallArgMap namespace scope */` |
|        - | 8556 | `		/* Every builtin call in a namespaced file lands here, having missed the user` |
|        - | 8557 | `		 * table twice on the way -- this is the answer worth remembering. */` |
|  1566447 | 8558 | `		PH7_VmCallSiteRecord(pVm,pInstr,&sName,bEngineCallee,1,pEntry);` |
|        - | 8559 | `		}` |
|  5406209 | 8560 | `		if( pEntry == 0 ){` |
|        - | 8561 | `			/* php accepts the "Class::method" STATIC-callable string everywhere a` |
|        - | 8562 | `			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).` |
|        - | 8563 | `			 * Split on the LAST "::" (php's own zend_memrchr scan) and route through the` |
|        - | 8564 | `			 * shared array-callable machinery ([class-name, method-name]) instead of` |
|        - | 8565 | `			 * warning undefined. */` |
|   240143 | 8566 | `			const char *zCbCls = 0,*zCbMeth = 0;` |
|   240143 | 8567 | `			sxu32 nCbCls = 0,nCbMeth = 0;` |
|   240143 | 8568 | `			int bScoped = PH7_VmCallableStringParts(sName.zString,sName.nByte,` |
|        - | 8569 | `				&zCbCls,&nCbCls,&zCbMeth,&nCbMeth);` |
|   240143 | 8570 | `			if( bScoped ){` |
|        - | 8571 | `				/* Resolve BEFORE dispatching: the shared dispatcher answers SXRET_OK with` |
|        - | 8572 | `` 				 * a NULL result for a pair it cannot resolve, so `$cb='C::nosuch'; $cb();` `` |
|        - | 8573 | `				 * evaluated to NULL with no diagnostic at all — where php throws the same` |
|        - | 8574 | `				 * Errors the ARRAY form of the same call already raised here. (Its own` |
|        - | 8575 | ``				 * `if( bScoped )` block: this scratch buffer and the dispatch block's`` |
|        - | 8576 | `				 * hashmap are both block-head declarations, and the check runs between` |
|        - | 8577 | `				 * them.) */` |
|        - | 8578 | `				char zSmMsg[192];` |
|   100077 | 8579 | `				sxi32 nSmBrc = pVm->nBoundaryRc;` |
|   100077 | 8580 | `				const void *pSmRes = (const void *)pVm->pResumeFrame;` |
|   150114 | 8581 | `				const char *zSmErr = VmCallableClassMethodError(&(*pVm),` |
|    50037 | 8582 | `					PH7_VmExtractClass(&(*pVm),zCbCls,nCbCls,FALSE,0),` |
|    50037 | 8583 | `					zCbCls,nCbCls,zCbMeth,nCbMeth,TRUE,zSmMsg,sizeof(zSmMsg));` |
|   100077 | 8584 | `				if( zSmErr ){` |
|        - | 8585 | `					sxi32 rcSmErr;` |
|       53 | 8586 | `					int bSmRaised = PH7_VmClassLookupRaised(&(*pVm),nSmBrc,pSmRes);` |
|       53 | 8587 | `					if( pInstr->iP2 & PH7_CALL_SPREAD ){` |
|      ! 0 | 8588 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 8589 | `					}` |
|       53 | 8590 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 8591 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 8592 | `					}` |
|       53 | 8593 | `					PH7_MemObjRelease(pTos);` |
|       53 | 8594 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       53 | 8595 | `					pTos->nIdx = SXU32_HIGH;` |
|       53 | 8596 | `					if( bSmRaised ){` |
|        - | 8597 | `						/* The class name's autoloader threw: land THAT, exactly as the array` |
|        - | 8598 | `						 * form does — php never reports the class missing in this case. */` |
|        6 | 8599 | `						rcSmErr = pVm->nBoundaryRc;` |
|        6 | 8600 | `						pVm->nBoundaryRc = 0;` |
|        6 | 8601 | `						if( rcSmErr == PH7_ABORT ){` |
|      ! 0 | 8602 | `							goto Abort;` |
|        - | 8603 | `						}` |
|        6 | 8604 | `						rc = PH7_EXCEPTION;` |
|       14 | 8605 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 8606 | `					}` |
|       48 | 8607 | `					rcSmErr = VmThrowFromVm(&(*pVm),"Error",zSmErr,(sxu32)SyStrlen(zSmErr));` |
|       48 | 8608 | `					if( rcSmErr == SXERR_ABORT ){` |
|      ! 0 | 8609 | `						goto Abort;` |
|        - | 8610 | `					}` |
|       48 | 8611 | `					rc = rcSmErr;` |
|       74 | 8612 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 8613 | `				}` |
|    50012 | 8614 | `			}` |
|   240093 | 8615 | `			if( bScoped ){` |
|        - | 8616 | `				/* Resolved: hand the callable STRING itself to the shared dispatcher, which` |
|        - | 8617 | ``				 * decodes `Class::method` the same way (it has to, for the callback-argument`` |
|        - | 8618 | `				 * callers). This used to build a throwaway [class,method] map here and enter` |
|        - | 8619 | `				 * through the hashmap branch — two decoders for one spelling. */` |
|        - | 8620 | `				ph7_value sResult;` |
|        - | 8621 | `				sxi32 rcSm;` |
|   150038 | 8622 | `				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100024 | 8623 | `					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|   100026 | 8624 | `				SySetReset(&aArg);` |
|   100040 | 8625 | `				while( pArg < pTos ){` |
|       15 | 8626 | `					SySetPut(&aArg,(const void *)&pArg);` |
|       15 | 8627 | `					pArg++;` |
|        1 | 8628 | `				}` |
|   100026 | 8629 | `				PH7_MemObjInit(pVm,&sResult);` |
|   100026 | 8630 | `				pVm->bDiscardCallback = bResultDropped;   /* see the sibling site */` |
|   150038 | 8631 | `				rcSm = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),` |
|   100024 | 8632 | `					(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|   100026 | 8633 | `				pVm->bDiscardCallback = 0;` |
|   100026 | 8634 | `				SySetReset(&aArg);` |
|   100026 | 8635 | `				if( nCallArgs > 0 ){` |
|       13 | 8636 | `					VmPopOperand(&pTos,nCallArgs);` |
|        6 | 8637 | `				}` |
|   100026 | 8638 | `				if( rcSm == PH7_ABORT ){` |
|      ! 0 | 8639 | `					PH7_MemObjRelease(&sResult);` |
|      ! 0 | 8640 | `					goto Abort;` |
|        - | 8641 | `				}` |
|   100026 | 8642 | `				if( rcSm == PH7_EXCEPTION ){` |
|        - | 8643 | `					sxi32 iResumePc;` |
|   100004 | 8644 | `					PH7_MemObjRelease(&sResult);` |
|   100004 | 8645 | `					if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100001 | 8646 | `						PH7_MemObjRelease(pTos);` |
|        - | 8647 | `						/* Drain the abandoned outer-expression operands` |
|        - | 8648 | ``						 * (`1 + "C::m"()`) to the try's base — one leaked`` |
|        - | 8649 | `						 * slot per caught throw otherwise. */` |
|   300001 | 8650 | `						PH7_RESUME_DRAIN()` |
|   100001 | 8651 | `						pc = iResumePc;` |
|   100001 | 8652 | `						break;` |
|        - | 8653 | `					}` |
|        3 | 8654 | `					goto Exception;` |
|        - | 8655 | `				}` |
|       24 | 8656 | `				PH7_MemObjStore(&sResult,pTos);` |
|       24 | 8657 | `				PH7_MemObjRelease(&sResult);` |
|       24 | 8658 | `				break;` |
|        - | 8659 | `			}` |
|        - | 8660 | `			/* Call to an undefined function is a catchable Error in php 8 — it does` |
|        - | 8661 | `			 * NOT warn and hand back null and carry on, which is what PH7 did (and` |
|        - | 8662 | `			 * which quietly turned a typo into a null-propagating program). */` |
|        - | 8663 | `			{` |
|        - | 8664 | `			SyBlob sMsg;` |
|   140069 | 8665 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   140069 | 8666 | `			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);` |
|        - | 8667 | `			/* Consume this call's captured spread runs so they don't leak into a` |
|        - | 8668 | `			 * later call (this path never reaches VmBuildEffectiveArgMap). */` |
|   140069 | 8669 | `			if( pInstr->iP2 & PH7_CALL_SPREAD ){` |
|      ! 0 | 8670 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 8671 | `			}` |
|        - | 8672 | `			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the` |
|        - | 8673 | `			 * compile-time arg count on the stack, and this early exit skips the` |
|        - | 8674 | `			 * arg-building loop that the normal path uses, so popping only iP1 would` |
|        - | 8675 | `			 * strand the expanded elements and corrupt the enclosing expression. */` |
|   140069 | 8676 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 8677 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 8678 | `			}` |
|   140069 | 8679 | `			PH7_MemObjRelease(pTos);` |
|   210101 | 8680 | `			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|    70032 | 8681 | `				SyBlobLength(&sMsg));` |
|   140069 | 8682 | `			SyBlobRelease(&sMsg);` |
|   140069 | 8683 | `			if( rc == SXERR_ABORT ){` |
|        6 | 8684 | `				goto Abort;` |
|        - | 8685 | `			}` |
|        - | 8686 | `			/* A catch may have run IN PLACE inside VmThrowFromVm (SXRET_OK +` |
|        - | 8687 | ``			 * recorded resume). An unconditional `goto Exception` here unwound the`` |
|        - | 8688 | `` 			 * exec ANYWAY, so `try { nosuchfn(); } catch (Error $e) {} rest();` `` |
|        - | 8689 | `			 * ran the catch and then silently dropped the rest of the script` |
|        - | 8690 | `			 * (exit 0). Route like every other in-exec throw site. */` |
|   360093 | 8691 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 8692 | `			}` |
|        - | 8693 | `		}` |
|  5166071 | 8694 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 8695 | `		/* D1: resolve deferred plain-var arguments against the builtin's by-ref position` |
|        - | 8696 | `		 * mask (derived from its signature). A by-ref out-param (preg_match's $matches, …)` |
|        - | 8697 | `		 * is materialized so PH7_VmStoreArgByRef can write back — this is what keeps a` |
|        - | 8698 | ``		 * DYNAMIC-name by-ref builtin (`$f='preg_match'; $f($p,$s,$m)`) working now that the`` |
|        - | 8699 | `		 * compile-time mask no longer sees it. Every other (by-value) arg warns + passes` |
|        - | 8700 | `		 * NULL, which is also what call_user_func & friends want. pVm->pFrame is the caller. */` |
|        - | 8701 | `		{` |
|  5166071 | 8702 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,pFunc->nByRefMask,0,0,pEffCallMap);` |
|  5166089 | 8703 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 8704 | `		}` |
|        - | 8705 | `		/* Host function (builtin): build the effective spread-key map so the` |
|        - | 8706 | `		 * name-forwarding builtins (call_user_func & friends) relay string keys as` |
|        - | 8707 | `		 * named args, and — critically — so this call's captured runs are consumed.` |
|        - | 8708 | `		 * pArg is the top base here (a builtin call pops no method-name slot). */` |
|  7750201 | 8709 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  5166016 | 8710 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 8711 | `		/* Does php's compiler rewrite THIS call into an opcode of its own? Only a` |
|        - | 8712 | `		 * literal, unambiguous global name at the exact arity the rewrite covers --` |
|        - | 8713 | `		 * the same three disqualifiers the call_user_func fold has (an unqualified` |
|        - | 8714 | `		 * name inside a namespace, a name read from a variable, a spread), plus a` |
|        - | 8715 | ``		 * `name:` argument, which the rewrite cannot reorder. See bFoldedCallee. */`` |
|  5166016 | 8716 | `		if( bLiteralCallee && !bNsCallee && (pInstr->iP2 & PH7_CALL_SPREAD) == 0` |
|  7748250 | 8717 | `		 && (pEffCallMap == 0 \|\| !pEffCallMap->bHasNamed) ){` |
|        - | 8718 | `			static const struct { const char *zName; int nLen; int nArg; } aFolded[] = {` |
|        - | 8719 | `				{ "strlen",           sizeof("strlen")-1,           1 },` |
|        - | 8720 | `				{ "count",            sizeof("count")-1,            1 },` |
|        - | 8721 | `				{ "sizeof",           sizeof("sizeof")-1,           1 },` |
|        - | 8722 | `				{ "array_key_exists", sizeof("array_key_exists")-1, 2 },` |
|        - | 8723 | `				{ "get_class",        sizeof("get_class")-1,        1 },` |
|        - | 8724 | `				{ "get_class",        sizeof("get_class")-1,        0 },` |
|        - | 8725 | `			};` |
|        - | 8726 | `			sxu32 iF;` |
| 34804016 | 8727 | `			for( iF = 0 ; iF < SX_ARRAYSIZE(aFolded) ; ++iF ){` |
| 29873453 | 8728 | `				if( (int)pFunc->sName.nByte == aFolded[iF].nLen` |
| 17648303 | 8729 | `				 && nCallArgs == aFolded[iF].nArg` |
|  4274764 | 8730 | `				 && SyStrnicmp(pFunc->sName.zString,aFolded[iF].zName,(sxu32)aFolded[iF].nLen) == 0 ){` |
|   233464 | 8731 | `					bFoldedCallee = 1;` |
|   233464 | 8732 | `					break;` |
|        - | 8733 | `				}` |
| 14820312 | 8734 | `			}` |
|  2583228 | 8735 | `		}` |
|  2466256 | 8736 | `NativeCall:` |
|        - | 8737 | `		/* A VM_FUNC_NATIVE method joins here, having done the two steps above for` |
|        - | 8738 | `		 * itself: its by-ref mask comes from the same signature machinery, and its` |
|        - | 8739 | `		 * effective arg map was already built (and this call's spread runs already` |
|        - | 8740 | `		 * consumed) on the method path — building it a second time here would` |
|        - | 8741 | `		 * consume them twice. Everything from this point down is shared verbatim:` |
|        - | 8742 | `		 * a native method IS a host call that happens to carry a receiver. */` |
|        - | 8743 | `		/* php raises a deprecated callee's E_DEPRECATED at the CALL, before the` |
|        - | 8744 | ``		 * body and before every screen under it: `curl_close()` with no argument`` |
|        - | 8745 | `		 * warns first and throws the ArgumentCountError second. A native method` |
|        - | 8746 | `		 * joins this block too, and its notice names the DECLARING class. */` |
|  6737761 | 8747 | `		if( pFunc->pDeprecated ){` |
|      275 | 8748 | `			PH7_VmDeprecatedCallNotice(&(*pVm),pFunc->pDeprecated);` |
|      136 | 8749 | `		}` |
|        - | 8750 | `		/* Start collecting function arguments */` |
|  6737761 | 8751 | `		SySetReset(&aArg);` |
| 16915263 | 8752 | `		while( pArg < pTos ){` |
| 10177507 | 8753 | `			SySetPut(&aArg,(const void *)&pArg);` |
| 10177507 | 8754 | `			pArg++;` |
|        5 | 8755 | `		}` |
|        - | 8756 | `		/* Assume a null return value */` |
|  6737761 | 8757 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 8758 | `		/* Init the call context */` |
|  6737761 | 8759 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,bDynamicCall ? PH7_CTX_CALL_DYNAMIC : 0);` |
|        - | 8760 | `		/* Hand the call-site named-argument map to the builtin so name-forwarding` |
|        - | 8761 | `		 * helpers (call_user_func & friends) can relay name: arguments — and the` |
|        - | 8762 | `		 * caller's strict_types mode — to the inner callback. Forwarded whole (not` |
|        - | 8763 | `		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even` |
|        - | 8764 | `		 * when its own call site is purely positional; only the two forwarding` |
|        - | 8765 | `		 * builtins read pArgMap, so this is inert for every other host function. */` |
|  6737761 | 8766 | `		sCtx.pArgMap = pEffCallMap;` |
|        - | 8767 | `		/* Receiver + late-static-binding class for a native METHOD. Both stay 0 for a` |
|        - | 8768 | `		 * plain host function (the locals are initialized once per OP_CALL), so this` |
|        - | 8769 | `		 * is inert on the builtin path. The reference on pNativeOwned belongs to the` |
|        - | 8770 | `		 * caller for the span of the call — the native body borrows it and must not` |
|        - | 8771 | `		 * unref, exactly as a bytecode method's frame $this is borrowed. */` |
|  6737761 | 8772 | `		sCtx.pThis = pNativeRecv;` |
|  6737761 | 8773 | `		sCtx.pCalledClass = pNativeClass;` |
|        - | 8774 | `		{` |
|  6737761 | 8775 | `		int nGiven = (int)SySetUsed(&aArg);` |
|        - | 8776 | `		/* The trace frame php gives this internal call. Linked in below, AFTER the` |
|        - | 8777 | `		 * two screens php answers from the caller's own frame (a named argument it` |
|        - | 8778 | `		 * cannot bind, and a non-variable in a by-reference position -- neither` |
|        - | 8779 | `		 * leaves an internal frame in php's trace), and unlinked unconditionally at` |
|        - | 8780 | `		 * NativeCallDone: pPrev is seeded here so the restore is a no-op on the` |
|        - | 8781 | `		 * paths that jump there before the link. */` |
|        - | 8782 | `		VmNativeCall sNativeCall;` |
|  6737761 | 8783 | `		sNativeCall.pName = &pFunc->sName;` |
|  6737761 | 8784 | `		sNativeCall.pClass = 0;` |
|  6737761 | 8785 | `		sNativeCall.bStatic = 0;` |
|  6737761 | 8786 | `		sNativeCall.nLine = pVm->nCurLine;` |
|  6737761 | 8787 | `		sNativeCall.pFrame = (void *)pVm->pFrame;` |
|  6737761 | 8788 | `		sNativeCall.pPrev = pVm->pNativeCall;` |
|        - | 8789 | ``		/* Bind `name:` arguments to the callee's declared POSITIONS before anything`` |
|        - | 8790 | `		 * reads the vector — the arity screen, the ZPP screen and the C body all take` |
|        - | 8791 | `		 * it positionally. A host function has no compiled parameter records for` |
|        - | 8792 | `		 * VmResolveNamedArgs to walk, so its signature string is the source of names` |
|        - | 8793 | `		 * and defaults (PH7_VmBindNamedArgsToSig). Without this every named argument` |
|        - | 8794 | `		 * simply stayed where it was WRITTEN. */` |
|  6737761 | 8795 | `		if( pEffCallMap && pEffCallMap->bHasNamed && nGiven > 0 ){` |
|      206 | 8796 | `			rc = PH7_VmBindNamedArgsToSig(&sCtx,pFunc,pEffCallMap,&nGiven,` |
|      134 | 8797 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|      139 | 8798 | `			if( rc != SXRET_OK ){` |
|        9 | 8799 | `				goto NativeCallDone;` |
|        - | 8800 | `			}` |
|       63 | 8801 | `		}` |
|        - | 8802 | `		/* php binds a by-reference argument at the CALL, before the callee runs, so a` |
|        - | 8803 | ``		 * non-variable in a `&` position is refused ahead of every ZPP check — and`` |
|        - | 8804 | ``		 * ahead of the too-MANY-arguments one (`array_pop([1,2],5)` is the reference`` |
|        - | 8805 | `		 * Error in php, not an ArgumentCountError). With no argument at all there is` |
|        - | 8806 | `		 * nothing to refuse, which is why the too-FEW check below still speaks first` |
|        - | 8807 | ``		 * for `array_pop()`. */`` |
| 10107709 | 8808 | `		rc = PH7_VmScreenByRefArgShapes(&sCtx,pFunc,pEffCallMap,nGiven,` |
|  6737748 | 8809 | `			(ph7_value **)SySetBasePtr(&aArg));` |
|  6737753 | 8810 | `		if( rc != SXRET_OK ){` |
|       51 | 8811 | `			goto NativeCallDone;` |
|        - | 8812 | `		}` |
|        - | 8813 | `		/* From here down every refusal is one php raises from INSIDE the callee --` |
|        - | 8814 | `		 * the arity screens, the argument-type screen and the C body itself -- so` |
|        - | 8815 | `		 * the internal frame is on the trace for all of them. A native METHOD names` |
|        - | 8816 | ``		 * its DECLARING class, which is what php's `class` key holds (an inherited`` |
|        - | 8817 | `		 * one reports the base, not the receiver's class). */` |
|  6737707 | 8818 | `		if( pNativeMethod ){` |
|        - | 8819 | ``			/* php's `function` key is the BARE method name, with the class in its own`` |
|        - | 8820 | ``			 * key -- the qualified `Class::method` spelling belongs to the diagnostic`` |
|        - | 8821 | `			 * text, and is what the host-function record carries. The method record` |
|        - | 8822 | `			 * holds the name as it was declared. */` |
|  1571617 | 8823 | `			sNativeCall.pName = &pNativeMethod->sName;` |
|  1571617 | 8824 | `			sNativeCall.pClass = pNativeDeclClass;` |
|  1571617 | 8825 | `			sNativeCall.bStatic = (pNativeMethod->iFlags & VM_FUNC_NATIVE_STATIC) != 0;` |
|   785716 | 8826 | `		}` |
|        - | 8827 | `		/* A php LANGUAGE CONSTRUCT is dispatched here as a host function but is not a` |
|        - | 8828 | ``		 * call in php at all: `print`, `isset`, `unset` and `empty` are opcodes with`` |
|        - | 8829 | `		 * no frame, and include/require/eval have a frame of their own SHAPE, built` |
|        - | 8830 | `		 * from the include stack further down the walk. Recording one here emitted` |
|        - | 8831 | `		 * that frame twice -- an exception created at the top level of an included` |
|        - | 8832 | ``		 * unit listed `include()` once for the unit it was thrown in and once for`` |
|        - | 8833 | `		 * the file that loaded it. */` |
|  6737707 | 8834 | `		if( !bFoldedCallee && !pFunc->bConstruct ){` |
|  6245016 | 8835 | `			pVm->pNativeCall = &sNativeCall;` |
|  3122475 | 8836 | `		}` |
|        - | 8837 | `		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum` |
|        - | 8838 | `		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError` |
|        - | 8839 | `		 * before the C routine runs when called with too few arguments — instead` |
|        - | 8840 | `		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""` |
|        - | 8841 | `		 * return. The message wording matches php's ZPP output byte-for-byte. */` |
| 10107644 | 8842 | `		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){` |
|     1013 | 8843 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 8844 | `				"%z() expects %s %d argument%s, %d given",` |
|      336 | 8845 | `				&pFunc->sName,` |
|      672 | 8846 | `				pFunc->bAtLeast ? "at least" : "exactly",` |
|      672 | 8847 | `				(int)pFunc->nMinArg,` |
|      672 | 8848 | `				pFunc->nMinArg == 1 ? "" : "s",` |
|      336 | 8849 | `				nGiven);` |
|  6737371 | 8850 | `		}else if( pFunc->bHasMaxArg && nGiven > (int)pFunc->nMaxArg ){` |
|        - | 8851 | `			/* php enforces the MAXIMUM as well, and PHL only did so where a` |
|        - | 8852 | `			 * builtin happened to hand-roll the check (51 of ~650), so` |
|        - | 8853 | ``			 * `microtime(1,2)`, `strlen("a","b")` and friends silently ignored`` |
|        - | 8854 | `			 * the extras. The count comes from the same signature table as the` |
|        - | 8855 | `			 * minimum; a variadic tail leaves nMaxArg at -1 and is exempt.` |
|        - | 8856 | `			 * php says "exactly" when the bounds coincide, "at most" otherwise. */` |
|      359 | 8857 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 8858 | `				"%z() expects %s %d argument%s, %d given",` |
|      118 | 8859 | `				&pFunc->sName,` |
|      199 | 8860 | `				((int)pFunc->nMinArg == (int)pFunc->nMaxArg && !pFunc->bAtLeast) ? "exactly" : "at most",` |
|      236 | 8861 | `				(int)pFunc->nMaxArg,` |
|      236 | 8862 | `				pFunc->nMaxArg == 1 ? "" : "s",` |
|      118 | 8863 | `				nGiven);` |
| 10106400 | 8864 | `		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,` |
|  6736794 | 8865 | `			(ph7_value **)SySetBasePtr(&aArg))) ){` |
|        - | 8866 | `			/* TypeError thrown: rc carries the caught/uncaught status */` |
|      638 | 8867 | `		}else{` |
|        - | 8868 | `			/* The name of the builtin that is RUNNING, for the few diagnostics` |
|        - | 8869 | `			 * raised so deep inside the engine that no ph7_context reaches them` |
|        - | 8870 | `			 * (a stream filter's, from inside a device read) and which php still` |
|        - | 8871 | `			 * prefixes with the caller. Saved and restored: a builtin can call` |
|        - | 8872 | `			 * back into php and reach this line again. */` |
|  6735527 | 8873 | `			SyString *pSavedCallee = pVm->pCalleeName;` |
|        - | 8874 | `			/* php's two callback FORWARDS pass "the answer is being dropped" on to` |
|        - | 8875 | `			 * the callback they drive; every other builtin ignores this. Saved and` |
|        - | 8876 | `			 * restored for the same reason the callee name is. */` |
|  6735527 | 8877 | `			int bSavedHostDiscard = pVm->bHostDiscard;` |
|  6735527 | 8878 | `			SyString *pSavedNativeFrame = pVm->pNativeFrameName;` |
|  6735527 | 8879 | `			pVm->pCalleeName = &pFunc->sName;` |
|  6735527 | 8880 | `			pVm->bHostDiscard = bResultDropped && bLiteralCallee;` |
|        - | 8881 | `			/* A forward php could not elide invokes its callback the way any other` |
|        - | 8882 | `			 * internal function does: the callback's frame gets no file or line, and` |
|        - | 8883 | `			 * this builtin gets a frame of its own. Only the FRAME shape is affected --` |
|        - | 8884 | `			 * the argument BINDING mode still travels the forward's own map, which is` |
|        - | 8885 | `			 * php's rule and a separate latch (bCallbackWeak). */` |
|  6735527 | 8886 | `			if( bNsCallee \|\| !bLiteralCallee \|\| (pInstr->iP2 & PH7_CALL_SPREAD) ){` |
|        - | 8887 | `				/* ...and two more shapes php cannot fold, for the same compile-time` |
|        - | 8888 | `				 * reason. A name that is not a literal at all` |
|        - | 8889 | ``				 * (`$n = 'call_user_func'; $n($c)`), and an argument list carrying a`` |
|        - | 8890 | ``				 * SPREAD (`call_user_func_array(...$pair)`) -- the fold rewrites the`` |
|        - | 8891 | `				 * call into a direct one and needs the arity at compile time, which a` |
|        - | 8892 | `				 * runtime unpack does not give it. */` |
|     2202 | 8893 | `				pVm->pNativeFrameName = &pFunc->sName;` |
|     1051 | 8894 | `			}` |
|        - | 8895 | `			/* Call the foreign function */` |
|  6735527 | 8896 | `			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));` |
|  6735535 | 8897 | `			pVm->bHostDiscard = bSavedHostDiscard;` |
|  6735535 | 8898 | `			pVm->pCalleeName = pSavedCallee;` |
|  6735535 | 8899 | `			pVm->pNativeFrameName = pSavedNativeFrame;` |
|  6735535 | 8900 | `			if( PH7_CmpRefusalPending(pVm) ){` |
|        - | 8901 | `				/* A native compare handler refused a pair this builtin compared` |
|        - | 8902 | `				 * (in_array, sort, max and switch all drive the same comparator,` |
|        - | 8903 | `				 * which has no throw boundary of its own and only recorded it).` |
|        - | 8904 | `				 * php raises out of the comparison and the builtin never finishes;` |
|        - | 8905 | `				 * this one finishes first and then throws, the way every builtin` |
|        - | 8906 | `				 * whose failure is predicted rather than raised in flight does.` |
|        - | 8907 | `				 * Reported on the call context, so VmHostFuncThrowRc below lands` |
|        - | 8908 | `				 * it exactly as the builtin's own throws are landed -- unless the` |
|        - | 8909 | `				 * builtin ALREADY raised, in which case the first throw wins and` |
|        - | 8910 | `				 * the record is only dropped. */` |
|       18 | 8911 | `				if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND` |
|       20 | 8912 | `				 \|\| sCtx.nThrowRc != 0 ){` |
|      ! 0 | 8913 | `					PH7_CmpRefusalClear(&(*pVm));` |
|      ! 0 | 8914 | `				}else{` |
|       20 | 8915 | `					PH7_CmpRefusalRaiseCtx(&sCtx);` |
|        - | 8916 | `				}` |
|        9 | 8917 | `			}` |
|        - | 8918 | `			/* A host function that RAISED a catchable throw (PH7_VmThrowException)` |
|        - | 8919 | `			 * and still returned PH7_OK reports it here — the throw's catch has` |
|        - | 8920 | `			 * already run in place, so treating the call as a normal return would` |
|        - | 8921 | `			 * resume execution INSIDE the try body the throw abandoned (and, when` |
|        - | 8922 | `			 * uncaught, run on past the reported fatal). The status is recorded on` |
|        - | 8923 | `			 * the call context, so this covers the shared validation helpers whose` |
|        - | 8924 | `			 * callers have no channel to thread a status back. */` |
|  6735535 | 8925 | `			rc = VmHostFuncThrowRc(&sCtx,rc);` |
|        - | 8926 | `		}` |
|  3367804 | 8927 | `NativeCallDone:` |
|  3369960 | 8928 | `		(void)nGiven; /* the named-arg binder's early exit lands here */` |
|  6737769 | 8929 | `		pVm->pNativeCall = sNativeCall.pPrev;` |
|        - | 8930 | `		}` |
|        - | 8931 | `		/* Release the call context */` |
|  6737769 | 8932 | `		VmReleaseCallContext(&sCtx);` |
|  6737769 | 8933 | `		if( pNativeOwned ){` |
|        - | 8934 | `			/* Drop the receiver reference the method branch took (pThis->iRef++ before` |
|        - | 8935 | `			 * the target slot was released). A bytecode method hands this to its frame` |
|        - | 8936 | `			 * and lets the teardown do it; a native method has no frame, so it is` |
|        - | 8937 | `			 * dropped here — the one point EVERY route out of the call passes through,` |
|        - | 8938 | `			 * including the throw/abort/suspend ones below. Ordered after the context` |
|        - | 8939 | `			 * release because sCtx.sThis aliases the instance. Always 0 for a plain` |
|        - | 8940 | `			 * host function. */` |
|  1569070 | 8941 | `			PH7_ClassInstanceUnref(pNativeOwned);` |
|  1569070 | 8942 | `			pNativeOwned = 0;` |
|  1569070 | 8943 | `			pNativeRecv = 0;` |
|   784450 | 8944 | `		}` |
|  6737769 | 8945 | `		if( rc == PH7_ABORT ){` |
|        - | 8946 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 8947 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 8948 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      911 | 8949 | `			PH7_MemObjRelease(&sRet);` |
|      911 | 8950 | `			goto Abort;` |
|        - | 8951 | `		}` |
|  6736863 | 8952 | `		if( rc != PH7_SUSPEND && VmInlineOwnedBy(pVm,aInstr,sState.pEntryFrame) ){` |
|        - | 8953 | `			/* A throw raised inside this host function — directly` |
|        - | 8954 | `			 * (PH7_VmThrowException) or by a PHP callback it invoked — was` |
|        - | 8955 | `			 * caught by an INLINE try (generator body) THIS exec owns.` |
|        - | 8956 | `			 * VmThrowInline records only a pc-redirect: a direct builtin throw` |
|        - | 8957 | `			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION` |
|        - | 8958 | `			 * with the redirect pending, so the rc branches below never land` |
|        - | 8959 | `			 * it (pre-existing hole: explode("") or a throwing usort` |
|        - | 8960 | `			 * comparator inside a generator's try lost the catch AND the` |
|        - | 8961 | `			 * yield). Land at the redirect now — its drain to the try's` |
|        - | 8962 | `			 * operand base subsumes the args + name pops. */` |
|       10 | 8963 | `			PH7_MemObjRelease(&sRet);` |
|       32 | 8964 | `			PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 8965 | `		}` |
|  6736855 | 8966 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 8967 | `			/* A callback invoked by this host function threw. If an in-place catch` |
|        - | 8968 | `			 * recorded a resume target owned by THIS body, resume at its landing pad` |
|        - | 8969 | `			 * (consuming the target); otherwise the exception was caught by an outer` |
|        - | 8970 | `			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW` |
|        - | 8971 | `			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which` |
|        - | 8972 | `			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */` |
|        - | 8973 | `			sxi32 iResumePc;` |
|    16792 | 8974 | `			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 8975 | `				/* Caught by an outer exec, or not caught here: propagate. */` |
|     4742 | 8976 | `				goto Exception;` |
|        - | 8977 | `			}` |
|        - | 8978 | `			/* Exception was caught in place by THIS body's try: pop args and the` |
|        - | 8979 | `			 * result slot, then drain any abandoned outer-expression operands to` |
|        - | 8980 | `			 * the try's base and resume. */` |
|    12055 | 8981 | `			PH7_MemObjRelease(&sRet);` |
|    12055 | 8982 | `			if( nCallArgs > 0 ){` |
|    11477 | 8983 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|     5736 | 8984 | `			}` |
|    12055 | 8985 | `			VmPopOperand(&pTos,1);` |
|    16599 | 8986 | `			PH7_RESUME_DRAIN()` |
|    12055 | 8987 | `			pc = iResumePc;` |
|    12055 | 8988 | `			break;` |
|        - | 8989 | `		}` |
|  6720068 | 8990 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 8991 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 8992 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 8993 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 8994 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 8995 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 8996 | `			 * body), the user-function path above will handle re-saving. */` |
|      ! 0 | 8997 | `			PH7_MemObjRelease(&sRet);` |
|      ! 0 | 8998 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 8999 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      ! 0 | 9000 | `			}` |
|        - | 9001 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 9002 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|      ! 0 | 9003 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|      ! 0 | 9004 | `			goto Suspend;` |
|        - | 9005 | `		}` |
|  6720068 | 9006 | `		if( nCallArgs > 0 ){` |
|        - | 9007 | `			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack` |
|        - | 9008 | `			 * expanded the compile-time arg count on the stack, so popping iP1` |
|        - | 9009 | `			 * strands the extra elements (or, for an unpack that expanded to fewer` |
|        - | 9010 | `			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,` |
|        - | 9011 | `			 * reading a bogus value whose stray flags sent MemObjStore into the` |
|        - | 9012 | `			 * hashmap-release path and hung). Mirrors every other CALL exit. The` |
|        - | 9013 | `			 * function-name slot (pTos) receives the return value below. */` |
|  6623462 | 9014 | `			VmPopOperand(&pTos,nCallArgs);` |
|  3312939 | 9015 | `		}` |
|        - | 9016 | `		/* Save foreign function return value into the (now top) function-name slot */` |
|  6720068 | 9017 | `		PH7_MemObjStore(&sRet,pTos);` |
|        - | 9018 | `		/* ...and clear that slot's index. It is one of the call's own argument slots,` |
|        - | 9019 | `		 * still carrying the variable index the argument was loaded with, and` |
|        - | 9020 | `		 * PH7_MemObjStore does not touch nIdx — so a builtin's return value came back` |
|        - | 9021 | ``		 * looking like an lvalue for the caller's variable (`f(strtoupper($b))` with`` |
|        - | 9022 | ``		 * `function f(&$x)` overwrote `$b`). No host function returns by reference. */`` |
|  6720068 | 9023 | `		pTos->nIdx = SXU32_HIGH;` |
|  6720068 | 9024 | `		PH7_MemObjRelease(&sRet);` |
|        - | 9025 | `	}` |
|  6724416 | 9026 | `	break;` |
|        - | 9027 | `				  }` |
|        - | 9028 | `/*` |
|        - | 9029 | ` * OP_CONSUME: P1 * *` |
|        - | 9030 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 9031 | ` */` |
|    97944 | 9032 | `case PH7_OP_CONSUME: {` |
|        - | 9033 | `	VmOpRc rcOp;` |
|   195732 | 9034 | `	sState.pTos = pTos;` |
|   195732 | 9035 | `	sState.pc = pc;` |
|   195732 | 9036 | `	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);` |
|   195732 | 9037 | `	pTos = sState.pTos;` |
|   195732 | 9038 | `	pc = sState.pc;` |
|   195732 | 9039 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 9040 | `		goto Abort;` |
|   195730 | 9041 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       14 | 9042 | `		goto Exception;` |
|        - | 9043 | `	}` |
|   195713 | 9044 | `	break;` |
|        - | 9045 | `					  }` |
|        - | 9046 |  |
|        - | 9047 | `		} /* Switch() */` |
| 77337674 | 9048 | `		pc++; /* Next instruction in the stream */` |
|        5 | 9049 | `	} /* For(;;) */` |
|  2226168 | 9050 | `Done:` |
|        - | 9051 | `	/* A stacked callee completing lands here too (its result is already in` |
|        - | 9052 | `	 * sState.pResult — the caller's operand slot); Unwind's first iteration` |
|        - | 9053 | `	 * bottoms out identically for the record-less case. */` |
|  4452428 | 9054 | `	rc = SXRET_OK;` |
|  4452428 | 9055 | `	goto Unwind;` |
|      836 | 9056 | `Suspend:` |
|     1677 | 9057 | `	rc = PH7_SUSPEND;` |
|     1677 | 9058 | `	if( pCallTop != 0 ){` |
|        - | 9059 | `		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment` |
|        - | 9060 | `		 * instead of the lossy unwind. pc/nTos of the innermost activation were` |
|        - | 9061 | `		 * already saved into the ctx by VmSuspendCtx; capture the rest (the` |
|        - | 9062 | `		 * record chain, the innermost activation, the suspend-time top frame)` |
|        - | 9063 | `		 * so resume re-enters HERE, inside the innermost callee, like php. The` |
|        - | 9064 | `		 * records / frames / operand stacks stay alive — nothing is freed. Only` |
|        - | 9065 | `		 * fibers reach this (generators yield only at their body level, pCallTop` |
|        - | 9066 | `		 * == 0); a suspend inside a C->PHP callback was already rejected with a` |
|        - | 9067 | `		 * FiberError before it could arrive here. */` |
|      ! 0 | 9068 | `		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));` |
|      ! 0 | 9069 | `		if( pSeg == 0 ){` |
|        - | 9070 | `			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST` |
|        - | 9071 | `			 * pc/nTos into the ctx, so the lossy body-level fallback would resume` |
|        - | 9072 | `			 * the callee's pc against the body stack — silent corruption, exactly` |
|        - | 9073 | `			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the` |
|        - | 9074 | `			 * the convention shared with the stage-2 record-alloc OOM site). */` |
|      ! 0 | 9075 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 9076 | `			rc = PH7_ABORT;` |
|      ! 0 | 9077 | `			goto Unwind;` |
|        - | 9078 | `		}` |
|      ! 0 | 9079 | `		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */` |
|      ! 0 | 9080 | `		sState.pHigh = ( pHigh > pTos ) ? pHigh : pTos;` |
|      ! 0 | 9081 | `		pSeg->sState = sState;` |
|      ! 0 | 9082 | `		pSeg->pCallTop = pCallTop;` |
|      ! 0 | 9083 | `		pSeg->pTopFrame = pVm->pFrame;` |
|      ! 0 | 9084 | `		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;` |
|      ! 0 | 9085 | `		pSeg->nOldFinBase = pVm->pActiveCtx ? pVm->pActiveCtx->nFinallyBase : 0;` |
|        - | 9086 | `		{` |
|        - | 9087 | `			VmCallFrame *pRec;` |
|      ! 0 | 9088 | `			pSeg->nRecords = 0;` |
|      ! 0 | 9089 | `			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){` |
|      ! 0 | 9090 | `				pSeg->nRecords++;` |
|      ! 0 | 9091 | `			}` |
|        - | 9092 | `		}` |
|      ! 0 | 9093 | `		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;` |
|        - | 9094 | `		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */` |
|      ! 0 | 9095 | `		SySetRelease(&aArg);` |
|      ! 0 | 9096 | `		return PH7_SUSPEND;` |
|        - | 9097 | `	}` |
|     1677 | 9098 | `	goto Unwind;` |
|      628 | 9099 | `Abort:` |
|     1260 | 9100 | `	rc = PH7_ABORT;` |
|     1260 | 9101 | `	goto Unwind;` |
|   305897 | 9102 | `Exception:` |
|   611744 | 9103 | `	rc = PH7_EXCEPTION;` |
|   611739 | 9104 | `	goto Unwind;` |
|  2533529 | 9105 | `Unwind:` |
|        - | 9106 | `	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration` |
|        - | 9107 | `	 * finishes the top record exactly as the old per-level native return did:` |
|        - | 9108 | `	 * for ABORT/EXCEPTION, first run what the popped activation's own` |
|        - | 9109 | `	 * Abort/Exception label used to do (clear its pending return, release its` |
|        - | 9110 | `	 * operands — a stacked activation never has bReturnPropagates set), then` |
|        - | 9111 | `	 * VmCallFinish routes in the restored caller (an in-place catch there` |
|        - | 9112 | `	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —` |
|        - | 9113 | `	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible` |
|        - | 9114 | `	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).` |
|        - | 9115 | `	 * At the bottom, VmExecFinalize hands the status to the native caller. */` |
|  2677850 | 9116 | `	for(;;){` |
|  5362133 | 9117 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|        - | 9118 | `			/* Drop any pending hook-RMW write-backs this activation armed — its` |
|        - | 9119 | `			 * statement is abandoned (only the innermost activation at throw time` |
|        - | 9120 | `			 * can own entries: the armed window spans exactly one instruction, so` |
|        - | 9121 | `			 * no OP_CALL record ever intervenes). */` |
|   914503 | 9122 | `			while( SySetUsed(&pVm->aHookRmw) > 0` |
|   914504 | 9123 | `			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){` |
|        3 | 9124 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 9125 | `			}` |
|   457223 | 9126 | `		}` |
|  5373173 | 9127 | `		if( pCallTop == 0 ){` |
|  4546150 | 9128 | `			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);` |
|        - | 9129 | `		}` |
|   827028 | 9130 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|   607601 | 9131 | `			VmClearFramePending(sState.pEntryFrame);` |
|   621021 | 9132 | `			while( pTos >= pStack ){` |
|    13425 | 9133 | `				PH7_MemObjRelease(pTos);` |
|    13425 | 9134 | `				pTos--;` |
|        5 | 9135 | `			}` |
|   303771 | 9136 | `		}` |
|   827028 | 9137 | `		if( rc != PH7_SUSPEND ){` |
|        - | 9138 | `			/* The finishing callee's own leaked finally actions must not survive` |
|        - | 9139 | `			 * into the caller (whose next OP_END_FINALLY would mis-pop them). */` |
|   827028 | 9140 | `			VmDiscardFinallyActions(&(*pVm),sState.nFinallyActBase);` |
|   413716 | 9141 | `		}` |
|        - | 9142 | `		{` |
|   827028 | 9143 | `			VmCallFrame *pRec = pCallTop;` |
|        - | 9144 | `			/* What the finishing callee ever touched of its own operand stack. The` |
|        - | 9145 | `			 * live top is in it too: an op handler that pushed and then threw hands` |
|        - | 9146 | `			 * control to the drain above without another instruction fetch, so pTos` |
|        - | 9147 | `			 * can be above the last sampled watermark. VmCallFinish's recycle sweeps` |
|        - | 9148 | `			 * exactly this much and leaves the rest of the buffer alone. */` |
|   827028 | 9149 | `			if( pTos > pHigh ){` |
|      ! 0 | 9150 | `				pHigh = pTos;` |
|      ! 0 | 9151 | `			}` |
|   827028 | 9152 | `			pRec->sCall.nLiveTos = (pHigh >= pStack) ? (sxu32)(pHigh - pStack) + 1 : 0;` |
|   827028 | 9153 | `			sState = pRec->sCaller;` |
|   827028 | 9154 | `			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);` |
|   827028 | 9155 | `			pCallTop = pRec->pPrev;` |
|   827028 | 9156 | `			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   827028 | 9157 | `			pVm->pIdleCallFrames = (void *)pRec;` |
|   827028 | 9158 | `			aInstr = sState.aInstr;` |
|   827028 | 9159 | `			pStack = sState.pStack;` |
|   827028 | 9160 | `			pTos = sState.pTos;` |
|   827028 | 9161 | `			pHigh = sState.pHigh;` |
|   827028 | 9162 | `			pc = sState.pc;` |
|        - | 9163 | `		}` |
|   827028 | 9164 | `		if( rc == PH7_OK ){` |
|   531989 | 9165 | `			pc++; /* the loop-bottom increment this OP_CALL missed */` |
|   531989 | 9166 | `			goto VmLoopFetch;` |
|        - | 9167 | `		}` |
|        5 | 9168 | `	}` |
|  2275179 | 9169 | `}` |
|        - | 9170 |  |
