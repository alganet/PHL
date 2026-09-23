# src/ph7/vm_exec.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3702/4195 lines (88.25%)

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
|      338 |   45 | `static int VmGrowOperandStack(ph7_vm *pVm, sxu32 nNeed,` |
|        - |   46 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |   47 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        3 |   48 | `{` |
|      341 |   49 | `	ph7_value *pOld = *ppStack;` |
|      341 |   50 | `	sxu32 nOldCap = pState->nStackCap;` |
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
|      341 |   64 | `	nReq = nNeed + pState->nStackOrig;` |
|      341 |   65 | `	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */` |
|      ! 0 |   66 | `		nReq = SXU32_HIGH;` |
|      ! 0 |   67 | `	}` |
|        - |   68 | `	/* SyMemBackendRealloc's size argument is sxu32, so the byte count` |
|        - |   69 | `	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack` |
|        - |   70 | `	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and` |
|        - |   71 | `	 * the init loop below would run off it. If the true requirement exceeds the` |
|        - |   72 | `	 * representable cap, treat it as OOM (the caller raises the guard error). */` |
|      341 |   73 | `	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);` |
|      341 |   74 | `	if( nReq > nMaxCap ){` |
|      ! 0 |   75 | `		return 0;` |
|        - |   76 | `	}` |
|      341 |   77 | `	if( nReq <= nOldCap ){` |
|      213 |   78 | `		return 1; /* already fits — no growth needed */` |
|        - |   79 | `	}` |
|      131 |   80 | `	nNewCap = nReq;` |
|        - |   81 | `	/* Amortize repeated spreads in one argument list: at least double, but never` |
|        - |   82 | `	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't` |
|        - |   83 | `	 * itself overflow.) */` |
|      131 |   84 | `	if( nNewCap < nOldCap * 2 ){` |
|      131 |   85 | `		sxu32 nDbl = nOldCap * 2;` |
|      131 |   86 | `		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }` |
|      131 |   87 | `		if( nNewCap < nDbl ){ nNewCap = nDbl; }` |
|       64 |   88 | `	}` |
|      195 |   89 | `	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,` |
|       64 |   90 | `		nNewCap * sizeof(ph7_value));` |
|      131 |   91 | `	if( pNew == 0 ){` |
|      ! 0 |   92 | `		return 0; /* OOM: caller keeps pOld and raises the guard error */` |
|        - |   93 | `	}` |
|        - |   94 | `	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */` |
|    13043 |   95 | `	for( i = nOldCap; i < nNewCap; i++ ){` |
|    12915 |   96 | `		PH7_MemObjInit(pVm, &pNew[i]);` |
|    12915 |   97 | `		pNew[i].nIdx = SXU32_HIGH;` |
|     6459 |   98 | `	}` |
|        - |   99 | `	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */` |
|      131 |  100 | `	*ppTos = pNew + (*ppTos - pOld);` |
|      131 |  101 | `	*ppStack = pNew;` |
|      131 |  102 | `	pState->pStack = pNew + (pState->pStack - pOld);` |
|      131 |  103 | `	pState->pTos = pNew + (pState->pTos - pOld);` |
|      131 |  104 | `	pState->nStackCap = nNewCap;` |
|      131 |  105 | `	if( pCallTop ){` |
|        - |  106 | `		/* This activation is a trampoline callee: its record owns the buffer. */` |
|       84 |  107 | `		pCallTop->sCall.pFrameStack = pNew;` |
|       84 |  108 | `		pCallTop->sCall.nStackCap = nNewCap;` |
|       43 |  109 | `	}else{` |
|        - |  110 | `		/* Base activation: the native entry frees *ppBaseOwner (and, for a` |
|        - |  111 | `		 * resumable coroutine, persists *pnBaseCap across suspend/resume). */` |
|       49 |  112 | `		if( ppBaseOwner ){ *ppBaseOwner = pNew; }` |
|       49 |  113 | `		if( pnBaseCap ){ *pnBaseCap = nNewCap; }` |
|        - |  114 | `	}` |
|        - |  115 | `	/* Re-anchor this activation's captured spread runs (pStart into the old` |
|        - |  116 | `	 * buffer). Enclosing-activation runs live in other buffers — leave them. */` |
|      131 |  117 | `	nRun = SySetUsed(&pVm->aSpreadRun);` |
|      131 |  118 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      131 |  119 | `	for( i = 0; i < nRun; i++ ){` |
|      ! 0 |  120 | `		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){` |
|      ! 0 |  121 | `			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);` |
|      ! 0 |  122 | `		}` |
|      ! 0 |  123 | `	}` |
|      131 |  124 | `	return 1;` |
|      172 |  125 | `}` |
|        - |  126 | `/*` |
|        - |  127 | ` * Ensure the running activation's operand stack can hold a spread of nEntry` |
|        - |  128 | ` * elements pushed at *ppTos (the source slot becomes the first element, so the` |
|        - |  129 | ` * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.` |
|        - |  130 | ` * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.` |
|        - |  131 | ` */` |
|      384 |  132 | `static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,` |
|        - |  133 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |  134 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        3 |  135 | `{` |
|        - |  136 | `	sxu32 nNeed;` |
|      387 |  137 | `	if( nEntry == 0 ){` |
|       49 |  138 | `		return 1; /* empty spread never grows the stack */` |
|        - |  139 | `	}` |
|      341 |  140 | `	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);` |
|      510 |  141 | `	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,` |
|      169 |  142 | `		pCallTop, ppBaseOwner, pnBaseCap);` |
|      195 |  143 | `}` |
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
|  3472814 |  164 | `static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)` |
|        5 |  165 | `{` |
|  3472819 |  166 | `	if( rcTerm != PH7_SUSPEND ){` |
|  3471441 |  167 | `		VmDiscardFinallyActions(&(*pVm),pState->nFinallyActBase);` |
|  1735718 |  168 | `	}` |
|  3472819 |  169 | `	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){` |
|  2016517 |  170 | `		VmClearFramePending(pState->pEntryFrame);` |
|  1008256 |  171 | `	}` |
|  3472819 |  172 | `	SySetRelease(pArg);` |
|  3472819 |  173 | `	if( rcTerm == PH7_ABORT \|\| rcTerm == PH7_EXCEPTION ){` |
|   804565 |  174 | `		while( pTos >= pState->pStack ){` |
|   402877 |  175 | `			PH7_MemObjRelease(pTos);` |
|   402877 |  176 | `			pTos--;` |
|        5 |  177 | `		}` |
|   200844 |  178 | `	}` |
|  3472819 |  179 | `	return rcTerm;` |
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
|  4215310 |  195 | `static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase)` |
|        5 |  196 | `{` |
|  4215325 |  197 | `	while( SySetUsed(&pVm->aFinallyAction) > nBase ){` |
|       13 |  198 | `		VmFinallyAction *pAct = (VmFinallyAction *)SySetPeek(&pVm->aFinallyAction);` |
|       13 |  199 | `		if( pAct->eKind == PH7_FA_RETHROW && pAct->pExc ){` |
|        7 |  200 | `			PH7_ClassInstanceUnref(pAct->pExc);` |
|        9 |  201 | `		}else if( pAct->eKind == PH7_FA_RETURN ){` |
|        3 |  202 | `			PH7_MemObjRelease(&pAct->sRet);` |
|        1 |  203 | `		}` |
|       13 |  204 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|        3 |  205 | `	}` |
|  4215315 |  206 | `}` |
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
|   748348 |  219 | `static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)` |
|        5 |  220 | `{` |
|        - |  221 | `	ph7_value *pObj;` |
|        - |  222 | `	/* Decrement nesting level */` |
|   748353 |  223 | `	pVm->nRecursionDepth--;` |
|   748353 |  224 | `	if( pCallee->bSelfPushed ){` |
|        - |  225 | `		/* Pop class name */` |
|   505193 |  226 | `		(void)SySetPop(&pVm->aSelf);` |
|   252594 |  227 | `	}` |
|   748353 |  228 | `	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  229 | `		/* Return by reference,reflect that */` |
|       66 |  230 | `		if( pCallee->nLastRef != SXU32_HIGH ){` |
|       66 |  231 | `			VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pCallee->pFrame->sLocal);` |
|        - |  232 | `			sxu32 i;` |
|        - |  233 | `			/* Make sure the referenced object is not a local variable */` |
|      120 |  234 | `			for( i = 0 ; i < SySetUsed(&pCallee->pFrame->sLocal) ; ++i ){` |
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
|       35 |  246 | `		}else{` |
|      ! 0 |  247 | `			if( (pCaller->pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  248 | `				VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  249 | `					"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  250 | `					&pCallee->pVmFunc->sName);` |
|      ! 0 |  251 | `			}` |
|        - |  252 | `		}` |
|       66 |  253 | `		pCaller->pTos->nIdx = pCallee->nLastRef;` |
|       35 |  254 | `	}else{` |
|        - |  255 | `		/* A by-VALUE return is a TEMPORARY — php's IS_TMP_VAR — and must not look like` |
|        - |  256 | `		 * an lvalue. The result lands in the slot the call's first ARGUMENT occupied,` |
|        - |  257 | `		 * which still carried that argument's variable index, so the returned value` |
|        - |  258 | ``		 * inherited it: `f(id($z))` with `function f(&$x)` aliased and overwrote `$z`,`` |
|        - |  259 | `		 * a variable neither function was given by reference. Every call form was` |
|        - |  260 | `		 * affected (function, method, static, closure, nested) and every one of them` |
|        - |  261 | `		 * silently. Clearing it here also lets the call site see the temporary for what` |
|        - |  262 | `		 * it is, which is what php's "Only variables should be passed by reference"` |
|        - |  263 | `		 * notice is raised on. */` |
|   748291 |  264 | `		pCaller->pTos->nIdx = SXU32_HIGH;` |
|        - |  265 | `	}` |
|   748353 |  266 | `	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  267 | `		/* The callee threw (or its finally threw past it). If an in-place catch` |
|        - |  268 | `		 * recorded a resume target owned by THIS caller's body, resume there and` |
|        - |  269 | `		 * consume the target (VmRecordedResume); when the catcher is an outer exec` |
|        - |  270 | `		 * — or this is a callback with no bytecode to resume into — propagate so` |
|        - |  271 | `		 * the owning exec lands. This replaces the old "is the caller's parent a` |
|        - |  272 | `		 * resumable try frame" test, which resumed at the caller's OWN try even` |
|        - |  273 | `		 * when the finally's throw was caught further out, losing that catch's` |
|        - |  274 | `		 * return (ROOT B, face c). */` |
|        - |  275 | `		sxi32 iResumePc;` |
|   607639 |  276 | `		VmFrame *pParentFrame = pCallee->pFrame->pParent;` |
|   607639 |  277 | `		if( !pCaller->is_callback && pVm->pInlineInstr == (void *)pCaller->aInstr ){` |
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
|   607639 |  288 | `		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){` |
|        - |  289 | `			/* Pop the result, then drain any abandoned outer-expression operands` |
|        - |  290 | `			 * to the catching try's base (like the inline branch above) — the` |
|        - |  291 | `			 * ops that would have consumed them were abandoned by the throw, and` |
|        - |  292 | `` 			 * leaving them leaks one slot per caught throw (`try { $a = 1 + f(); }` `` |
|        - |  293 | `			 * in a loop overflowed the operand stack). */` |
|   206969 |  294 | `			VmPopOperand(&pCaller->pTos,1);` |
|   812363 |  295 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iResumeStackDepth ){` |
|   605399 |  296 | `				PH7_MemObjRelease(pCaller->pTos);` |
|   605399 |  297 | `				pCaller->pTos--;` |
|        5 |  298 | `			}` |
|   206969 |  299 | `			pCaller->pc = iResumePc;` |
|   206969 |  300 | `			rc = PH7_OK;` |
|   103487 |  301 | `		}else{` |
|   400675 |  302 | `			if( pParentFrame->pParent ){` |
|   400671 |  303 | `				rc = PH7_EXCEPTION;` |
|   200338 |  304 | `			}else{` |
|        - |  305 | `				/* Continue normal execution */` |
|        6 |  306 | `				rc = PH7_OK;` |
|        - |  307 | `			}` |
|        - |  308 | `		}` |
|   303817 |  309 | `	}` |
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
|   748353 |  320 | `	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){` |
|        - |  321 | `		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee` |
|        - |  322 | `		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly` |
|        - |  323 | `		 * the allocated slot count either way. */` |
|   743879 |  324 | `		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap);` |
|   372149 |  325 | `	}` |
|        - |  326 | `	/* Leave the frame */` |
|   748353 |  327 | `	VmLeaveFrame(&(*pVm));` |
|   748353 |  328 | `	if( rc == PH7_ABORT ){` |
|      325 |  329 | `		return PH7_ABORT;` |
|        - |  330 | `	}` |
|   748033 |  331 | `	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  332 | `		/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  333 | `		 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  334 | `		 * overwriting the state saved by the inner level.` |
|        - |  335 | `		 * pTos points to the result slot (not yet written).` |
|        - |  336 | `		 * Save nTos one below so resume pushes at the result slot. */` |
|      ! 0 |  337 | `		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);` |
|      ! 0 |  338 | `		return PH7_SUSPEND;` |
|        - |  339 | `	}` |
|   748033 |  340 | `	if( rc == PH7_EXCEPTION ){` |
|   400671 |  341 | `		return PH7_EXCEPTION;` |
|        - |  342 | `	}` |
|   347367 |  343 | `	return PH7_OK;` |
|   374391 |  344 | `}` |
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
|  3473120 |  380 | `PH7_PRIVATE sxi32 VmByteCodeExec(` |
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
|  3473125 |  400 | `	if( VmNativeNestingExceeded(pVm) ){` |
|        6 |  401 | `		return VmNativeNestingFatal(pVm);` |
|        - |  402 | `	}` |
|        - |  403 | `	/* A fresh native exec entered while a C-boundary throw is parked` |
|        - |  404 | `	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the` |
|        - |  405 | `	 * very opcode that swallowed the throw) must run CLEAN: the parked status` |
|        - |  406 | `	 * belongs to the interrupted outer exec's fetch-point router, not to this` |
|        - |  407 | `	 * one. Save+clear on entry, merge back on exit — the outer status is` |
|        - |  408 | `	 * restored unless this exec parked its own unconsumed (newer) one, with` |
|        - |  409 | `	 * PH7_ABORT dominating either way. */` |
|  3473121 |  410 | `	nSavedBrc = pVm->nBoundaryRc;` |
|  3473121 |  411 | `	pVm->nBoundaryRc = 0;` |
|        - |  412 | `	/* The executing source line belongs to the ACTIVATION. A nested body -- a called` |
|        - |  413 | `	 * function, but equally an attribute-default or default-argument mini-program --` |
|        - |  414 | `	 * runs its own bytecode with its own lines, so it must not leave the caller` |
|        - |  415 | ``	 * reporting the callee's position: `new Exception` stamped line 1 because the`` |
|        - |  416 | ``	 * class's `protected $message = '';` default ran (from the embedded chunk) between`` |
|        - |  417 | `	 * OP_NEW and the stamp. Save on entry, restore on exit. */` |
|  3473121 |  418 | `	nSavedLine = pVm->nCurLine;` |
|  3473121 |  419 | `	pVm->nVmExecDepth++;` |
|  5209679 |  420 | `	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,` |
|  1736558 |  421 | `		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);` |
|  3473121 |  422 | `	pVm->nVmExecDepth--;` |
|  3473121 |  423 | `	pVm->nCurLine = nSavedLine;` |
|  3473121 |  424 | `	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT \|\| pVm->nBoundaryRc == 0) ){` |
|       24 |  425 | `		pVm->nBoundaryRc = nSavedBrc;` |
|       10 |  426 | `	}` |
|  3473121 |  427 | `	return rc;` |
|  1736565 |  428 | `}` |
|        - |  429 | `/*` |
|        - |  430 | ` * D1 commit 2: allocate a captured lvalue path for a deferred element/property call arg.` |
|        - |  431 | ` * eRoot picks the root container: 0 = a real aMemObj slot (nRootIdx), 1 = an undefined` |
|        - |  432 | ` * variable to vivify by name at resolve time (pName, a VM-lifetime bytecode string, borrowed),` |
|        - |  433 | ` * 2 = a string base (subscripting a string -> php refuses a by-ref bind). The struct owns its` |
|        - |  434 | ` * step array and each step's key/name; PH7_MemObjRelease frees it via VmFreeDeferredPath.` |
|        - |  435 | ` */` |
|    42754 |  436 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName)` |
|        5 |  437 | `{` |
|    42759 |  438 | `	VmDeferredPath *pPath = (VmDeferredPath *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDeferredPath));` |
|    42759 |  439 | `	if( pPath == 0 ){` |
|      ! 0 |  440 | `		return 0;` |
|        - |  441 | `	}` |
|    42759 |  442 | `	SyZero(pPath,sizeof(VmDeferredPath));` |
|    42759 |  443 | `	pPath->pAlloc = &pVm->sAllocator;` |
|    42759 |  444 | `	pPath->eRoot = eRoot;` |
|    42759 |  445 | `	pPath->nRootIdx = nRootIdx;` |
|    42759 |  446 | `	if( eRoot == 1 && pName ){` |
|        6 |  447 | `		pPath->sRootName = *pName; /* borrowed VM-lifetime bytes, not copied */` |
|        2 |  448 | `	}` |
|    42759 |  449 | `	return pPath;` |
|    21547 |  450 | `}` |
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
|    42576 |  503 | `static VmDeferStep * VmDeferPathGrow(VmDeferredPath *pPath)` |
|        5 |  504 | `{` |
|    42581 |  505 | `	if( pPath->nStep >= pPath->nAlloc ){` |
|    42573 |  506 | `		sxu32 nNew = pPath->nAlloc ? pPath->nAlloc * 2 : 4;` |
|    64022 |  507 | `		VmDeferStep *aNew = (VmDeferStep *)SyMemBackendRealloc(pPath->pAlloc,pPath->aStep,` |
|    21449 |  508 | `			nNew * sizeof(VmDeferStep));` |
|    42573 |  509 | `		if( aNew == 0 ){` |
|      ! 0 |  510 | `			return 0;` |
|        - |  511 | `		}` |
|    42573 |  512 | `		pPath->aStep = aNew;` |
|    42573 |  513 | `		pPath->nAlloc = nNew;` |
|    21449 |  514 | `	}` |
|    42581 |  515 | `	return &pPath->aStep[pPath->nStep];` |
|    21458 |  516 | `}` |
|        - |  517 | `/* Append an array-element step, deep-copying the index value (the caller releases pKey). */` |
|    42508 |  518 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey)` |
|        5 |  519 | `{` |
|    42513 |  520 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|    42513 |  521 | `	if( pStep == 0 ){` |
|      ! 0 |  522 | `		return SXERR_MEM;` |
|        - |  523 | `	}` |
|    42513 |  524 | `	pStep->isProp = 0;` |
|    42513 |  525 | `	pStep->bAppend = 0;` |
|    42513 |  526 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|    42513 |  527 | `	PH7_MemObjInit(pKey->pVm,&pStep->sKey);` |
|    42513 |  528 | `	PH7_MemObjStore(pKey,&pStep->sKey);` |
|    42513 |  529 | `	pPath->nStep++;` |
|    42513 |  530 | `	return SXRET_OK;` |
|    21424 |  531 | `}` |
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
|      122 |  605 | `PH7_PRIVATE VmMagicCall * VmMagicCallNew(ph7_vm *pVm,ph7_class_instance *pRecv,` |
|        - |  606 | `	ph7_class *pClass,const SyString *pName)` |
|        3 |  607 | `{` |
|      125 |  608 | `	VmMagicCall *pPend = (VmMagicCall *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmMagicCall));` |
|      125 |  609 | `	if( pPend == 0 ){` |
|      ! 0 |  610 | `		return 0;` |
|        - |  611 | `	}` |
|      125 |  612 | `	pPend->pAlloc = &pVm->sAllocator;` |
|      125 |  613 | `	pPend->pRecv = pRecv;` |
|      125 |  614 | `	pPend->pClass = pClass;` |
|      125 |  615 | `	SyBlobInit(&pPend->sName,&pVm->sAllocator);` |
|      125 |  616 | `	if( pName && pName->nByte > 0 ){` |
|      125 |  617 | `		SyBlobAppend(&pPend->sName,(const void *)pName->zString,pName->nByte);` |
|       61 |  618 | `	}` |
|      125 |  619 | `	if( pRecv ){` |
|       84 |  620 | `		pRecv->iRef++;` |
|       41 |  621 | `	}` |
|      125 |  622 | `	return pPend;` |
|       64 |  623 | `}` |
|      122 |  624 | `PH7_PRIVATE void VmFreeMagicCall(VmMagicCall *pPend)` |
|        3 |  625 | `{` |
|        - |  626 | `	SyMemBackend *pAlloc;` |
|      125 |  627 | `	if( pPend == 0 ){` |
|      ! 0 |  628 | `		return;` |
|        - |  629 | `	}` |
|      125 |  630 | `	pAlloc = pPend->pAlloc;` |
|      125 |  631 | `	if( pPend->pRecv ){` |
|      ! 0 |  632 | `		PH7_ClassInstanceUnref(pPend->pRecv);` |
|      ! 0 |  633 | `	}` |
|      125 |  634 | `	SyBlobRelease(&pPend->sName);` |
|      125 |  635 | `	SyMemBackendFree(pAlloc,pPend);` |
|       64 |  636 | `}` |
|    42754 |  637 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath)` |
|        5 |  638 | `{` |
|        - |  639 | `	sxu32 i;` |
|    42759 |  640 | `	if( pPath == 0 ){` |
|      ! 0 |  641 | `		return;` |
|        - |  642 | `	}` |
|    85335 |  643 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|    42581 |  644 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|    42581 |  645 | `		if( pStep->isProp ){` |
|       63 |  646 | `			if( pStep->zProp ){` |
|       63 |  647 | `				SyMemBackendFree(pPath->pAlloc,pStep->zProp);` |
|       33 |  648 | `			}` |
|    42551 |  649 | `		}else if( !pStep->bAppend ){` |
|        - |  650 | `			/* An append step holds no key at all — its sKey was never initialized. */` |
|    42513 |  651 | `			PH7_MemObjRelease(&pStep->sKey);` |
|    21419 |  652 | `		}` |
|    21458 |  653 | `	}` |
|    42759 |  654 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|      220 |  655 | `		PH7_MemObjRelease(&pPath->sPrefetch);` |
|      220 |  656 | `		if( pPath->zOverName ){` |
|      174 |  657 | `			SyMemBackendFree(pPath->pAlloc,pPath->zOverName);` |
|       85 |  658 | `		}` |
|      108 |  659 | `	}` |
|    42759 |  660 | `	if( pPath->aStep ){` |
|    42573 |  661 | `		SyMemBackendFree(pPath->pAlloc,pPath->aStep);` |
|    21449 |  662 | `	}` |
|    42759 |  663 | `	SyMemBackendFree(pPath->pAlloc,pPath);` |
|    21547 |  664 | `}` |
|        - |  665 | `/* Map an op-handler VmOpRc into the main-loop rc convention used by VmByteCodeExecBody. */` |
|    42546 |  666 | `static sxi32 VmOpRcToExecRc(VmOpRc rcOp)` |
|        5 |  667 | `{` |
|    42551 |  668 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 |  669 | `		return PH7_ABORT;` |
|        - |  670 | `	}` |
|    42549 |  671 | `	if( rcOp == VM_OP_EXCEPTION ){` |
|       12 |  672 | `		return PH7_EXCEPTION;` |
|        - |  673 | `	}` |
|    42539 |  674 | `	return SXRET_OK;` |
|    21443 |  675 | `}` |
|        - |  676 | `/*` |
|        - |  677 | ` * D1 commit 2: re-drive ONE captured lvalue step by invoking the real LOAD_IDX / MEMBER` |
|        - |  678 | ` * handler on a synthetic 2-slot stack. This reuses the proven COW / vivify / warning / magic` |
|        - |  679 | ` * machinery instead of hand-walking it. pBase carries the current container (its nIdx must be` |
|        - |  680 | ` * a real aMemObj slot for a by-ref write to vivify in place). iP2 selects the mode:` |
|        - |  681 | ` * LOAD_IDX 1=write(vivify,by-ref) / 0=read(by-value); MEMBER PH7_MEMBER_READ=by-value read.` |
|        - |  682 | ` * On success pOut receives the result value and its nIdx (the aliasable element slot for a` |
|        - |  683 | ` * vivified by-ref element).` |
|        - |  684 | ` */` |
|    42546 |  685 | `static sxi32 VmReDriveStep(ph7_vm *pVm,sxi32 iOp,sxu32 iP2,ph7_value *pBase,ph7_value *pKey,ph7_value *pOut)` |
|        5 |  686 | `{` |
|        - |  687 | `	ph7_value mini[2];` |
|        - |  688 | `	VmInstr aI[2];` |
|        - |  689 | `	VmExecState st;` |
|        - |  690 | `	VmOpRc rcOp;` |
|        - |  691 | ``	/* A NULL key is the APPEND form (`$a[]`): LOAD_IDX takes no index operand, so the`` |
|        - |  692 | `	 * base is the whole stack and iP1 says so. */` |
|    42551 |  693 | `	int bAppend = (pKey == 0);` |
|    42551 |  694 | `	PH7_MemObjInit(pVm,&mini[0]);` |
|    42551 |  695 | `	PH7_MemObjInit(pVm,&mini[1]);` |
|    42551 |  696 | `	PH7_MemObjLoad(pBase,&mini[0]);` |
|    42551 |  697 | `	mini[0].nIdx = pBase->nIdx;` |
|    42551 |  698 | `	if( !bAppend ){` |
|    42545 |  699 | `		PH7_MemObjStore(pKey,&mini[1]);` |
|    21435 |  700 | `	}` |
|    42551 |  701 | `	SyZero((void *)aI,sizeof(aI));` |
|        - |  702 | `	/* LOAD_IDX: iP1=1 means "an index is present". MEMBER: iP1=0 means an INSTANCE member` |
|        - |  703 | ``	 * (iP1=1 would be a static `::` access). */`` |
|    42551 |  704 | `	aI[0].iOp = (sxu8)iOp; aI[0].iP1 = (iOp == PH7_OP_LOAD_IDX && !bAppend) ? 1 : 0; aI[0].iP2 = iP2;` |
|    42551 |  705 | `	SyZero((void *)&st,sizeof(st));` |
|    42551 |  706 | `	st.pStack = mini; st.pTos = bAppend ? &mini[0] : &mini[1]; st.aInstr = aI; st.pc = 0;` |
|    42551 |  707 | `	if( iOp == PH7_OP_LOAD_IDX ){` |
|    42511 |  708 | `		rcOp = VmExecOpLoadIdx(&(*pVm),&st,&aI[0]);` |
|    21423 |  709 | `	}else{` |
|       43 |  710 | `		rcOp = VmExecOpMember(&(*pVm),&st,&aI[0]);` |
|        - |  711 | `	}` |
|        - |  712 | `	/* The handler popped the key/name and left the result in the (now top) base slot.` |
|        - |  713 | `	 * DEEP-COPY it into pOut (MemObjStore, not MemObjLoad): the result is chained as the next` |
|        - |  714 | `	 * step's base and must OWN its buffer — mini[0] is released immediately below, and a` |
|        - |  715 | `	 * read-only view (MemObjLoad) would leave pOut dangling into freed memory. */` |
|    42551 |  716 | `	PH7_MemObjStore(st.pTos,pOut);` |
|    42551 |  717 | `	pOut->nIdx = st.pTos->nIdx;` |
|    42551 |  718 | `	PH7_MemObjRelease(&mini[0]);` |
|    42551 |  719 | `	return VmOpRcToExecRc(rcOp);` |
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
|    42662 |  924 | `static sxi32 VmWalkStepsOverValue(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,` |
|        - |  925 | `	ph7_value *pCur,int bWrite,ph7_value *pSlot)` |
|        5 |  926 | `{` |
|    42667 |  927 | `	sxi32 rc = SXRET_OK;` |
|        - |  928 | `	sxu32 i;` |
|    85137 |  929 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|    42491 |  930 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|        - |  931 | `		ph7_value out;` |
|    42491 |  932 | `		PH7_MemObjInit(&(*pVm),&out);` |
|    42491 |  933 | `		if( pStep->isProp && bWrite && (pCur->iFlags & MEMOBJ_OBJ) ){` |
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
|    42489 |  960 | `		if( pStep->isProp ){` |
|        - |  961 | `			ph7_value nameVal;` |
|       43 |  962 | `			PH7_MemObjInitFromString(&(*pVm),&nameVal,&pStep->sProp);` |
|       43 |  963 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_MEMBER,PH7_MEMBER_READ,pCur,&nameVal,&out);` |
|       43 |  964 | `			PH7_MemObjRelease(&nameVal);` |
|    42469 |  965 | `		}else if( pStep->bAppend ){` |
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
|    42447 |  982 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,bWrite ? 1 : 0,pCur,&pStep->sKey,&out);` |
|        - |  983 | `		}` |
|    42487 |  984 | `		PH7_MemObjRelease(pCur);` |
|    42487 |  985 | `		*pCur = out;` |
|    42487 |  986 | `		if( rc != SXRET_OK ){` |
|       14 |  987 | `			return rc;` |
|        - |  988 | `		}` |
|    21405 |  989 | `	}` |
|    42651 |  990 | `	PH7_MemObjStore(pCur,pSlot);` |
|    42651 |  991 | `	pSlot->nIdx = SXU32_HIGH;` |
|    42651 |  992 | `	return SXRET_OK;` |
|    21501 |  993 | `}` |
|        - |  994 | `/* Resolve a captured lvalue path as a BY-REF target: vivify the whole chain in place and` |
|        - |  995 | ` * leave pSlot->nIdx pointing at the terminal (aliasable) slot for the by-ref binder. */` |
|      120 |  996 | `static sxi32 VmResolvePathByRef(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        5 |  997 | `{` |
|        - |  998 | `	sxu32 nCur;` |
|        - |  999 | `	sxi32 rc;` |
|      125 | 1000 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
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
|       91 | 1015 | `	if( pPath->eRoot == 2 ){` |
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
|       65 | 1044 | `}` |
|        - | 1045 | `/* Resolve a captured lvalue path as a BY-VALUE argument: read the chain (emitting php's` |
|        - | 1046 | ` * undefined-key/property/offset warnings) WITHOUT vivifying, leaving the terminal value in` |
|        - | 1047 | ` * pSlot. Re-drives the read handlers so the warning sequence matches php exactly. */` |
|    42632 | 1048 | `static sxi32 VmResolvePathByValue(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        5 | 1049 | `{` |
|        - | 1050 | `	ph7_value cur;` |
|        - | 1051 | `	sxi32 rc;` |
|    42637 | 1052 | `	PH7_MemObjInit(&(*pVm),&cur);` |
|    42637 | 1053 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|        - | 1054 | `		/* The accessor already ran, where php runs it: a by-VALUE argument simply takes` |
|        - | 1055 | `		 * what it answered, in silence. */` |
|      184 | 1056 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|    42547 | 1057 | `	}else if( pPath->eRoot == 1 ){` |
|      ! 0 | 1058 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,FALSE);` |
|      ! 0 | 1059 | `		if( pRoot == 0 ){` |
|      ! 0 | 1060 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&pPath->sRootName);` |
|      ! 0 | 1061 | `		}else{` |
|      ! 0 | 1062 | `			PH7_MemObjLoad(pRoot,&cur);` |
|      ! 0 | 1063 | `			cur.nIdx = pRoot->nIdx;` |
|        - | 1064 | `		}` |
|      ! 0 | 1065 | `	}else{` |
|    42457 | 1066 | `		ph7_value *pRoot = (ph7_value *)SySetAt(&pVm->aMemObj,pPath->nRootIdx);` |
|    42457 | 1067 | `		if( pRoot ){` |
|    42457 | 1068 | `			PH7_MemObjLoad(pRoot,&cur);` |
|    42457 | 1069 | `			cur.nIdx = pRoot->nIdx;` |
|    21391 | 1070 | `		}` |
|        - | 1071 | `	}` |
|    42637 | 1072 | `	rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,FALSE,pSlot);` |
|    42637 | 1073 | `	PH7_MemObjRelease(&cur);` |
|    42637 | 1074 | `	return rc;` |
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
|     6782 | 1094 | `PH7_PRIVATE int PH7_VmArgRefusedByRef(VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1095 | `{` |
|     6787 | 1096 | `	if( pMap && pMap->bArgShapes && nPos < 31 ){` |
|     6709 | 1097 | `		return (pMap->nNonLvalMask & (1u << nPos)) != 0;` |
|        - | 1098 | `	}` |
|       81 | 1099 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|       65 | 1100 | `		return 0;` |
|        - | 1101 | `	}` |
|       26 | 1102 | `	return (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|       16 | 1103 | `	    && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0;` |
|     3396 | 1104 | `}` |
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
|     2650 | 1125 | `PH7_PRIVATE void PH7_VmByRefArgWriteBack(ph7_vm *pVm,ph7_value *pArg,sxi32 iPreFlags)` |
|        5 | 1126 | `{` |
|        - | 1127 | `	ph7_value *pSlot;` |
|     2650 | 1128 | `	if( pArg->nIdx == SXU32_HIGH` |
|     2655 | 1129 | `	 \|\| (pArg->iFlags & MEMOBJ_ALL) == (iPreFlags & MEMOBJ_ALL) ){` |
|     2631 | 1130 | `		return;` |
|        - | 1131 | `	}` |
|       25 | 1132 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|       25 | 1133 | `	if( pSlot && pSlot != pArg ){` |
|       25 | 1134 | `		PH7_MemObjStore(pArg,pSlot);` |
|       12 | 1135 | `	}` |
|     1330 | 1136 | `}` |
|     3584 | 1137 | `PH7_PRIVATE void PH7_VmArgTempCallNotice(ph7_vm *pVm,VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1138 | `{` |
|     3589 | 1139 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| nPos >= 31 ){` |
|       81 | 1140 | `		return;` |
|        - | 1141 | `	}` |
|     3511 | 1142 | `	if( (pMap->nTempCallMask & (1u << nPos)) == 0 ){` |
|     3471 | 1143 | `		return;` |
|        - | 1144 | `	}` |
|       43 | 1145 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|        3 | 1146 | `		return; /* a by-reference RETURN: php is silent and binds it */` |
|        - | 1147 | `	}` |
|       41 | 1148 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Only variables should be passed by reference");` |
|     1797 | 1149 | `}` |
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
|  4796065 | 1217 | `PH7_PRIVATE sxi32 PH7_VmResolveDeferredArgs(` |
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
|  4796070 | 1229 | `	sxu32 n = 0;` |
|  9694580 | 1230 | `	for( p = pArg ; p < pTos ; ++p, ++n ){` |
|  4898557 | 1231 | `		int bByRef = 0;` |
|        - | 1232 | `		SyString sName;` |
|  4898557 | 1233 | `		if( (p->iFlags & (MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH)) == 0 ){` |
|  4876973 | 1234 | `			continue;` |
|        - | 1235 | `		}` |
|    42779 | 1236 | `		if( bAllByValue ){` |
|      ! 0 | 1237 | `			bByRef = 0;` |
|    42779 | 1238 | `		}else if( bAllByRef ){` |
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
|    42779 | 1250 | `		}else if( pFormal ){` |
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
|    42575 | 1279 | `			bByRef = (n < 31 && (nByRefMask & (1u << n))) ? 1 : 0;` |
|        - | 1280 | `		}` |
|    42779 | 1281 | `		if( p->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|        - | 1282 | `			/* D1 commit 2: a deferred array-element/property lvalue. Detach the descriptor` |
|        - | 1283 | `			 * FIRST (so an exception mid-resolve, or a later stack release, cannot double-free` |
|        - | 1284 | `			 * it) then re-walk it in the chosen mode. */` |
|    42757 | 1285 | `			VmDeferredPath *pPath = (VmDeferredPath *)p->x.pOther;` |
|        - | 1286 | `			sxi32 rc;` |
|    42757 | 1287 | `			p->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|    42757 | 1288 | `			p->x.pOther = 0;` |
|    42757 | 1289 | `			MemObjSetType(p,MEMOBJ_NULL);` |
|    42757 | 1290 | `			p->nIdx = SXU32_HIGH;` |
|    42757 | 1291 | `			if( bByRef ){` |
|      125 | 1292 | `				rc = VmResolvePathByRef(&(*pVm),pPath,p);` |
|       65 | 1293 | `			}else{` |
|    42637 | 1294 | `				rc = VmResolvePathByValue(&(*pVm),pPath,p);` |
|        - | 1295 | `			}` |
|    42757 | 1296 | `			VmFreeDeferredPath(pPath);` |
|    42757 | 1297 | `			if( rc != SXRET_OK ){` |
|       47 | 1298 | `				return rc;` |
|        - | 1299 | `			}` |
|    42715 | 1300 | `			continue;` |
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
|  4796028 | 1320 | `	return SXRET_OK;` |
|  2399052 | 1321 | `}` |
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
|      200 | 1464 | `PH7_PRIVATE int PH7_VmFccMethodIsDirect(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName)` |
|        4 | 1465 | `{` |
|        - | 1466 | `	ph7_class_method *pMethod;` |
|        - | 1467 | `	SyString sDecl;` |
|      204 | 1468 | `	if( pClass == 0 \|\| nName < 1 ){` |
|      ! 0 | 1469 | `		return 0;` |
|        - | 1470 | `	}` |
|      204 | 1471 | `	pMethod = PH7_ClassExtractMethod(pClass,zName,nName);` |
|      204 | 1472 | `	if( pMethod == 0 \|\| (pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       17 | 1473 | `		return 0;` |
|        - | 1474 | `	}` |
|      188 | 1475 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|      118 | 1476 | `		return 1;` |
|        - | 1477 | `	}` |
|       72 | 1478 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1479 | `		SyStringLength(&pMethod->sFunc.sName));` |
|        - | 1480 | `	/* The OWNING class decides (a trait method is owned by the class that composed it) — the` |
|        - | 1481 | `	 * same argument every other visibility site passes. */` |
|      106 | 1482 | `	return PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       68 | 1483 | `		&sDecl,pMethod->iProtection,FALSE) ? 1 : 0;` |
|      104 | 1484 | `}` |
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
|      172 | 1502 | `static const char * VmFccMemberError(ph7_vm *pVm,ph7_class *pClass,` |
|        - | 1503 | `	const char *zCls,sxu32 nCls,const char *zMeth,sxu32 nMeth,int bStaticForm,` |
|        - | 1504 | `	ph7_class_instance **ppRecv,char *zBuf,int nBuf)` |
|        5 | 1505 | `{` |
|        - | 1506 | `	ph7_class_method *pMethod;` |
|        - | 1507 | `	SyString sDecl;` |
|      177 | 1508 | `	*ppRecv = 0;` |
|      177 | 1509 | `	if( pClass == 0 ){` |
|        3 | 1510 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|        3 | 1511 | `		return zBuf;` |
|        - | 1512 | `	}` |
|      175 | 1513 | `	pMethod = nMeth > 0 ? PH7_ClassExtractMethod(pClass,zMeth,nMeth) : 0;` |
|      175 | 1514 | `	if( pMethod == 0 ){` |
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
|      159 | 1531 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1532 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      159 | 1533 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1534 | `		/* Named through the CLASS only: an instance of an abstract class cannot exist, so` |
|        - | 1535 | `		 * the object spelling never reaches an abstract body. */` |
|        7 | 1536 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%.*s()",` |
|        2 | 1537 | `			&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1538 | `		return zBuf;` |
|        - | 1539 | `	}` |
|      150 | 1540 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      114 | 1541 | `	 && !PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
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
|      144 | 1556 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|       15 | 1557 | `		*ppRecv = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|       15 | 1558 | `		if( *ppRecv == 0 ){` |
|        7 | 1559 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|        4 | 1560 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|        5 | 1561 | `			return zBuf;` |
|        - | 1562 | `		}` |
|        5 | 1563 | `	}` |
|      140 | 1564 | `	return 0;` |
|       91 | 1565 | `}` |
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
|   300326 | 1605 | `static ph7_vm_func * VmIndirectCalleeFunc(ph7_vm *pVm,ph7_value *pCallable)` |
|        5 | 1606 | `{` |
|   300331 | 1607 | `	ph7_class_method *pMeth = 0;` |
|   300331 | 1608 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|   100242 | 1609 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|   100242 | 1610 | `		ph7_value *pTarget = 0,*pName = 0;` |
|        - | 1611 | `		ph7_class *pClass;` |
|   100238 | 1612 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|   100238 | 1613 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|   100242 | 1614 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1615 | `			return 0;` |
|        - | 1616 | `		}` |
|   100242 | 1617 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   100242 | 1618 | `		if( pClass == 0 ){` |
|      ! 0 | 1619 | `			return 0;` |
|        - | 1620 | `		}` |
|   150361 | 1621 | `		pMeth = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|   100238 | 1622 | `			SyBlobLength(&pName->sBlob));` |
|   250212 | 1623 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|   200093 | 1624 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|   200093 | 1625 | `		if( pThis == 0 ){` |
|      ! 0 | 1626 | `			return 0;` |
|        - | 1627 | `		}` |
|   200093 | 1628 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|   100044 | 1629 | `	}` |
|   300331 | 1630 | `	return pMeth ? &pMeth->sFunc : 0;` |
|   150168 | 1631 | `}` |
|        - | 1632 | `/*` |
|        - | 1633 | ` * Materialize an indirect dispatch's deferred arguments against that callee — the same` |
|        - | 1634 | ` * split OP_CALL makes for a direct one: a native method's by-ref positions come from its` |
|        - | 1635 | ` * signature mask, a PHP one's from its compiled formals, and an unresolved callee binds` |
|        - | 1636 | ` * everything by value (php's answer for the magic route it is about to take).` |
|        - | 1637 | ` */` |
|   300326 | 1638 | `static sxi32 VmResolveIndirectArgs(ph7_vm *pVm,ph7_value *pCallable,ph7_value *pArg,ph7_value *pTos,` |
|        - | 1639 | `	VmCallArgMap *pCallMap)` |
|        5 | 1640 | `{` |
|   300331 | 1641 | `	ph7_vm_func *pFn = VmIndirectCalleeFunc(&(*pVm),pCallable);` |
|   300331 | 1642 | `	if( pFn == 0 ){` |
|   100042 | 1643 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pCallMap);` |
|        - | 1644 | `	}` |
|   200291 | 1645 | `	if( pFn->iFlags & VM_FUNC_NATIVE ){` |
|        7 | 1646 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|        6 | 1647 | `			pFn->pNative ? pFn->pNative->nByRefMask : 0,0,0,pCallMap);` |
|        - | 1648 | `	}` |
|   300425 | 1649 | `	return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   200280 | 1650 | `		(ph7_vm_func_arg *)SySetBasePtr(&pFn->aArgs),SySetUsed(&pFn->aArgs),0,0,0,pCallMap);` |
|   150168 | 1651 | `}` |
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
|  1676726 | 1737 | `PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,` |
|        - | 1738 | `	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth)` |
|        5 | 1739 | `{` |
|        - | 1740 | `	sxu32 i;` |
| 13306001 | 1741 | `	for( i = nName ; i >= 2 ; --i ){` |
| 11829457 | 1742 | `		if( zName[i-2] == ':' && zName[i-1] == ':' ){` |
|   200185 | 1743 | `			*pzCls = zName;` |
|   200185 | 1744 | `			*pnCls = i - 2;` |
|   200185 | 1745 | `			*pzMeth = &zName[i];` |
|   200185 | 1746 | `			*pnMeth = nName - i;` |
|   200185 | 1747 | `			return TRUE;` |
|        - | 1748 | `		}` |
|  5823710 | 1749 | `	}` |
|  1476549 | 1750 | `	return FALSE;` |
|   839381 | 1751 | `}` |
|  3473116 | 1752 | `static sxi32 VmByteCodeExecBody(` |
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
|  3473121 | 1772 | `	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record` |
|        - | 1773 | `	                            * stack (BYTECODE stage 2); NULL = executing the` |
|        - | 1774 | `	                            * bottom activation. */` |
|        - | 1775 | `	VmExecState sState; /* This activation's boundary state (BYTECODE.md stage 1):` |
|        - | 1776 | `	                     * everything a suspended/nested activation must restore.` |
|        - | 1777 | `	                     * pc/pTos stay in locals for the hot loop and are synced` |
|        - | 1778 | `	                     * into sState only around the call epilogue (stage 2 turns` |
|        - | 1779 | `	                     * that boundary into an explicit record push/pop). */` |
|        - | 1780 | `	sxi32 pc;` |
|        - | 1781 | `	sxi32 rc;` |
|  3473121 | 1782 | `	sState.aInstr = aInstr;` |
|  3473121 | 1783 | `	sState.pStack = pStack;` |
|  3473121 | 1784 | `	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */` |
|  3473121 | 1785 | `	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */` |
|  3473121 | 1786 | `	sState.pResult = pResult;` |
|  3473121 | 1787 | `	sState.pLastRef = pLastRef;` |
|  3473121 | 1788 | `	sState.pEnforceRetFunc = pEnforceRetFunc;` |
|  3473121 | 1789 | `	sState.is_callback = (sxu8)(is_callback ? 1 : 0);` |
|  3473121 | 1790 | `	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);` |
|        - | 1791 | `	/* Argument container */` |
|  3473121 | 1792 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|  3473121 | 1793 | `	if( nTos < 0 ){` |
|  1590467 | 1794 | `		pTos = &pStack[-1];` |
|   795236 | 1795 | `	}else{` |
|  1882659 | 1796 | `		pTos = &pStack[nTos];` |
|        - | 1797 | `	}` |
|  3473121 | 1798 | `	sState.pTos = pTos;` |
|  3473121 | 1799 | `	sState.pc = nPc;` |
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
|  3473116 | 1817 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame` |
|     2438 | 1818 | `	 && pStack == pVm->pActiveCtx->pStack ){` |
|     1921 | 1819 | `		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;` |
|     1921 | 1820 | `		sState.nFinallyActBase = pVm->pActiveCtx->nFinallyBase;` |
|      963 | 1821 | `	}else{` |
|  3471205 | 1822 | `		sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|  3471205 | 1823 | `		sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|        - | 1824 | `	}` |
|  3473121 | 1825 | `	sState.pEntryFrame = pVm->pFrame;` |
|  3473121 | 1826 | `	pc = nPc;` |
|        - | 1827 | `	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call` |
|        - | 1828 | `	 * adopts its parked record segment here. VmResumeCtx hands the segment in` |
|        - | 1829 | `	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a` |
|        - | 1830 | `	 * nested mini-program/callback run inside the resumed body — after it rebased` |
|        - | 1831 | `	 * the segment's exception floors and pushed the resume value into the innermost` |
|        - | 1832 | `	 * stack. Locals switch to the innermost activation so the dispatch loop` |
|        - | 1833 | `	 * continues inside the callee; the record chain is restored so its completion` |
|        - | 1834 | `	 * unwinds back through the body. */` |
|  3473121 | 1835 | `	if( pAdoptSegment ){` |
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
|  3473116 | 1879 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected` |
|     1305 | 1880 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
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
|       35 | 1912 | `		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
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
|       13 | 1929 | `			goto Exception;` |
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
|  3473106 | 1940 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing` |
|     1297 | 1941 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
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
| 22036644 | 1961 | `	for(;;){` |
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
| 45121423 | 1976 | `		if( pVm->nBoundaryRc != 0 \|\| SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      679 | 1977 | `			if( pVm->nBoundaryRc != 0 ){` |
|      259 | 1978 | `				sxi32 rcBr = pVm->nBoundaryRc;` |
|      259 | 1979 | `				pVm->nBoundaryRc = 0;` |
|      259 | 1980 | `				if( rcBr == PH7_ABORT ){` |
|        3 | 1981 | `					goto Abort;` |
|        - | 1982 | `				}` |
|      257 | 1983 | `				if( pVm->pInlineInstr == (void *)aInstr ){` |
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
|      257 | 1994 | `					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){` |
|        - | 1995 | `						/* Caught in place by a try THIS exec owns: drain the abandoned` |
|        - | 1996 | `						 * operands to the catching try's base and land at its pad` |
|        - | 1997 | `						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;` |
|        - | 1998 | `						 * pre-fetch here, so +1 lands on the pad itself). */` |
|      319 | 1999 | `						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      175 | 2000 | `							PH7_MemObjRelease(pTos);` |
|      175 | 2001 | `							pTos--;` |
|        5 | 2002 | `						}` |
|      149 | 2003 | `						pc = iBrPc + 1;` |
|       77 | 2004 | `					}else{` |
|        - | 2005 | `						/* Caught by an outer exec (or uncaught-with-report pending):` |
|        - | 2006 | `						 * unwind out of this exec; the owner's macros/router land it. */` |
|      113 | 2007 | `						goto Exception;` |
|        - | 2008 | `					}` |
|        - | 2009 | `				}` |
|       72 | 2010 | `			}` |
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
|      581 | 2021 | `			while( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      421 | 2022 | `				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      420 | 2023 | `				if( pTopRmw->pOwnerStack != (void *)pStack \|\| pTopRmw->pInstrs != (void *)aInstr` |
|      161 | 2024 | `				 \|\| ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){` |
|      205 | 2025 | `					break; /* not ours, or legitimately in flight */` |
|        - | 2026 | `				}` |
|       13 | 2027 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 2028 | `			}` |
|      282 | 2029 | `		}` |
|        - | 2030 | `		/* Fetch the instruction to execute */` |
| 45121313 | 2031 | `		pInstr = &aInstr[pc];` |
| 45121313 | 2032 | `		if( pInstr->nLine ){` |
|        - | 2033 | `			/* Publish the source position for diagnostics, debug_backtrace() and` |
|        - | 2034 | `			 * Throwable. Instructions the compiler could not attribute (nLine 0)` |
|        - | 2035 | `			 * leave the last known line standing rather than reporting line 0.` |
|        - | 2036 | `			 * The compilation unit's strict_types mode rides the same gate: a` |
|        - | 2037 | `			 * SYNTHETIC instruction (nLine 0) must not claim to speak for a file. */` |
| 41713279 | 2038 | `			pVm->nCurLine = pInstr->nLine;` |
| 41713279 | 2039 | `			pVm->bCurStrict = pInstr->bStrict;` |
| 20876556 | 2040 | `		}` |
| 45121313 | 2041 | `		rc = SXRET_OK;` |
|        - | 2042 | `/*` |
|        - | 2043 | ` * What follows here is a massive switch statement where each case implements a` |
|        - | 2044 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - | 2045 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - | 2046 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - | 2047 | ` * the switch statement will break with convention and be flush-left.` |
|        - | 2048 | ` */` |
| 45121313 | 2049 | `		switch(pInstr->iOp){` |
|        - | 2050 | `/*` |
|        - | 2051 | ` * DONE: P1 * *` |
|        - | 2052 | ` *` |
|        - | 2053 | ` * Program execution completed: Clean up the mess left behind` |
|        - | 2054 | ` * and return immediately.` |
|        - | 2055 | ` */` |
|  1594721 | 2056 | `case PH7_OP_DONE:` |
|  3189871 | 2057 | `	if( pInstr->iP2 && sState.bReturnPropagates ){` |
|        - | 2058 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - | 2059 | `		 * onto the body frame this catch/finally returns from (skip the transparent` |
|        - | 2060 | `		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION` |
|        - | 2061 | `		 * materializes it into sState.pResult. Drain any finally opened within this body` |
|        - | 2062 | `		 * first (nested try/finally inside the catch), which may overwrite the same` |
|        - | 2063 | `		 * frame's slot (finally-over-catch). */` |
|    20281 | 2064 | `		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|    20281 | 2065 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    20277 | 2066 | `			PH7_MemObjStore(pTos,&pTgt->sRet);` |
|    20277 | 2067 | `			VmPopOperand(&pTos,1);` |
|    10141 | 2068 | `		}else{` |
|        6 | 2069 | ``			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */`` |
|        - | 2070 | `		}` |
|    20281 | 2071 | `		pTgt->bHasRet = 1;` |
|    20281 | 2072 | `		pTgt->nRetGen++;` |
|    20281 | 2073 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    20281 | 2074 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2075 | `			goto Abort;` |
|        - | 2076 | `		}` |
|    20281 | 2077 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 2078 | `			/* A drained finally threw past itself — it discards this return. */` |
|      ! 0 | 2079 | `			goto Exception;` |
|        - | 2080 | `		}` |
|    20281 | 2081 | `		goto Done;` |
|        - | 2082 | `	}` |
|        - | 2083 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - | 2084 | `	 * the fiber start/resume paths) set sState.pEnforceRetFunc, so this branch is` |
|        - | 2085 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - | 2086 | `	 * callback trampolines, and the main script. */` |
|  3169590 | 2087 | `	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)` |
|    11101 | 2088 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - | 2089 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - | 2090 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - | 2091 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - | 2092 | `		 * value the function never actually returned, so enforcing here would` |
|        - | 2093 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - | 2094 | `		 * exception. */` |
|    11101 | 2095 | `		ph7_value *pRetVal = 0;` |
|    11101 | 2096 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|     9899 | 2097 | `			pRetVal = pTos;` |
|     4947 | 2098 | `		}` |
|    11101 | 2099 | `		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);` |
|    11101 | 2100 | `		if( rc == PH7_ABORT ) goto Abort;` |
|    11097 | 2101 | `		if( rc == PH7_EXCEPTION ){` |
|      145 | 2102 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      111 | 2103 | `				PH7_MemObjRelease(pTos);` |
|      111 | 2104 | `				pTos--;` |
|       53 | 2105 | `			}` |
|      145 | 2106 | `			goto Exception;` |
|        - | 2107 | `		}` |
|        - | 2108 | `		/* Don't enforce twice if the function loops through multiple` |
|        - | 2109 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - | 2110 | `		 * defensively we clear the pointer after a successful check). */` |
|    10957 | 2111 | `		sState.pEnforceRetFunc = 0;` |
|     5476 | 2112 | `	}` |
|  3169451 | 2113 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|  1708549 | 2114 | `		if( sState.pLastRef ){` |
|   112155 | 2115 | `			*sState.pLastRef = pTos->nIdx;` |
|    56287 | 2116 | `		}` |
|  1708549 | 2117 | `		if( sState.pResult ){` |
|        - | 2118 | `			/* Execution result */` |
|   243915 | 2119 | `			PH7_MemObjStore(pTos,sState.pResult);` |
|   122167 | 2120 | `		}` |
|  1708549 | 2121 | `		VmPopOperand(&pTos,1);` |
|  2315391 | 2122 | `	}else if( sState.pLastRef ){` |
|        - | 2123 | `		/* Nothing referenced — also the throw-unwind path: the compiler routes` |
|        - | 2124 | `		 * an uncaught exception to this terminal OP_DONE with iP1 set but an` |
|        - | 2125 | `		 * empty operand stack (pTos == pStack-1), so there is no return value to` |
|        - | 2126 | `		 * store. Guarding on pTos >= pStack (matching the two sibling branches` |
|        - | 2127 | `		 * above) avoids the below-base read that crashed under glibc/ASan. */` |
|     8015 | 2128 | `		*sState.pLastRef = SXU32_HIGH;` |
|     4005 | 2129 | `	}` |
|        - | 2130 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - | 2131 | `	 * this execution. When 'return' is used inside a try block,` |
|        - | 2132 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - | 2133 | `	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering` |
|        - | 2134 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - | 2135 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - | 2136 | `	 * block can override it (the finally writes this body frame's sRet slot,` |
|        - | 2137 | `	 * materialized below).` |
|        - | 2138 | `	 */` |
|  3169451 | 2139 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|  3169451 | 2140 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2141 | `		goto Abort;` |
|        - | 2142 | `	}` |
|  3169451 | 2143 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 2144 | `		/* A drained finally threw past itself, discarding the value this OP_DONE` |
|        - | 2145 | `		 * stored into sState.pResult. If an enclosing try IN THIS function caught the new` |
|        - | 2146 | `		 * exception in place, resume at its landing pad; otherwise unwind (the` |
|        - | 2147 | `		 * caller's exception-resume pops the stored result). */` |
|        - | 2148 | `		sxi32 iResumePc;` |
|        5 | 2149 | `		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 2150 | `			pc = iResumePc;` |
|        3 | 2151 | `			break;` |
|        - | 2152 | `		}` |
|        3 | 2153 | `		goto Exception;` |
|        - | 2154 | `	}` |
|  3169447 | 2155 | `	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){` |
|        - | 2156 | `		/* A catch/finally issued a 'return' targeting THIS body. If the body is` |
|        - | 2157 | `		 * actually unwinding because an exception escaped it (terminal OP_DONE on` |
|        - | 2158 | `		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type` |
|        - | 2159 | `		 * enforcement above), that exception supersedes the return: discard it.` |
|        - | 2160 | `		 * Otherwise materialize it as this function's result. */` |
|       14 | 2161 | `		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){` |
|      ! 0 | 2162 | `			VmClearFramePending(sState.pEntryFrame);` |
|      ! 0 | 2163 | `		}else{` |
|       14 | 2164 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|        - | 2165 | `		}` |
|        5 | 2166 | `	}` |
|  3169447 | 2167 | `	goto Done;` |
|        - | 2168 | `/*` |
|        - | 2169 | ` * HALT: P1 * *` |
|        - | 2170 | ` *` |
|        - | 2171 | ` * Program execution aborted: Clean up the mess left behind` |
|        - | 2172 | ` * and abort immediately.` |
|        - | 2173 | ` */` |
|       12 | 2174 | `case PH7_OP_HALT:` |
|       28 | 2175 | `	if( pInstr->iP1 ){` |
|        - | 2176 | `#ifdef UNTRUST` |
|        - | 2177 | `		if( pTos < pStack ){` |
|        - | 2178 | `			goto Abort;` |
|        - | 2179 | `		}` |
|        - | 2180 | `#endif` |
|       28 | 2181 | `		if( sState.pLastRef ){` |
|        5 | 2182 | `			*sState.pLastRef = pTos->nIdx;` |
|        2 | 2183 | `		}` |
|       28 | 2184 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       15 | 2185 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - | 2186 | `				/* Output the exit message */` |
|       21 | 2187 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        6 | 2188 | `					pVm->sVmConsumer.pUserData);` |
|       15 | 2189 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        9 | 2190 | `			}` |
|       21 | 2191 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - | 2192 | `			/* Record exit status */` |
|       15 | 2193 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        6 | 2194 | `		}` |
|       28 | 2195 | `		VmPopOperand(&pTos,1);` |
|       12 | 2196 | `	}else if( sState.pLastRef ){` |
|        - | 2197 | `		/* Nothing referenced */` |
|      ! 0 | 2198 | `		*sState.pLastRef = SXU32_HIGH;` |
|      ! 0 | 2199 | `	}` |
|        - | 2200 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - | 2201 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - | 2202 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - | 2203 | `	 */` |
|       28 | 2204 | `	pVm->bHaltRequested = 1;` |
|       28 | 2205 | `	goto Abort;` |
|        - | 2206 | `/*` |
|        - | 2207 | ` * JMP: * P2 *` |
|        - | 2208 | ` *` |
|        - | 2209 | ` * Unconditional jump: The next instruction executed will be` |
|        - | 2210 | ` * the one at index P2 from the beginning of the program.` |
|        - | 2211 | ` */` |
|   451060 | 2212 | `case PH7_OP_JMP:` |
|   902955 | 2213 | `	pc = pInstr->iP2 - 1;` |
|   902955 | 2214 | `	break;` |
|        - | 2215 | `/*` |
|        - | 2216 | ` * JZ: P1 P2 *` |
|        - | 2217 | ` *` |
|        - | 2218 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - | 2219 | ` * entry in the stack if P1 is zero.` |
|        - | 2220 | ` */` |
|  1196448 | 2221 | `case PH7_OP_JZ:` |
|        - | 2222 | `#ifdef UNTRUST` |
|        - | 2223 | `	if( pTos < pStack ){` |
|        - | 2224 | `		goto Abort;` |
|        - | 2225 | `	}` |
|        - | 2226 | `#endif` |
|        - | 2227 | `	/* Get a boolean value */` |
|  2396962 | 2228 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      466 | 2229 | `		PH7_MemObjToBool(pTos);` |
|      231 | 2230 | `	}` |
|  2396962 | 2231 | `	if( !pTos->x.iVal ){` |
|        - | 2232 | `		/* Take the jump */` |
|  1296668 | 2233 | `		pc = pInstr->iP2 - 1;` |
|   649522 | 2234 | `	}` |
|  2396962 | 2235 | `	if( !pInstr->iP1 ){` |
|  2072616 | 2236 | `		VmPopOperand(&pTos,1);` |
|  1038137 | 2237 | `	}` |
|  2396962 | 2238 | `	break;` |
|        - | 2239 | `/*` |
|        - | 2240 | ` * JNZ: P1 P2 *` |
|        - | 2241 | ` *` |
|        - | 2242 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - | 2243 | ` * entry in the stack if P1 is zero.` |
|        - | 2244 | ` */` |
|   135200 | 2245 | `case PH7_OP_JNZ:` |
|        - | 2246 | `#ifdef UNTRUST` |
|        - | 2247 | `	if( pTos < pStack ){` |
|        - | 2248 | `		goto Abort;` |
|        - | 2249 | `	}` |
|        - | 2250 | `#endif` |
|        - | 2251 | `	/* Get a boolean value */` |
|   270837 | 2252 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 2253 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 2254 | `	}` |
|   270837 | 2255 | `	if( pTos->x.iVal ){` |
|        - | 2256 | `		/* Take the jump */` |
|     8897 | 2257 | `		pc = pInstr->iP2 - 1;` |
|     4446 | 2258 | `	}` |
|   270837 | 2259 | `	if( !pInstr->iP1 ){` |
|        8 | 2260 | `		VmPopOperand(&pTos,1);` |
|        3 | 2261 | `	}` |
|   270837 | 2262 | `	break;` |
|        - | 2263 | `/*` |
|        - | 2264 | ` * NOOP: * * *` |
|        - | 2265 | ` *` |
|        - | 2266 | ` * Do nothing. This instruction is often useful as a jump` |
|        - | 2267 | ` * destination.` |
|        - | 2268 | ` */` |
|      ! 0 | 2269 | `case PH7_OP_NOOP:` |
|      ! 0 | 2270 | `	break;` |
|        - | 2271 | `/*` |
|        - | 2272 | ` * POP: P1 * *` |
|        - | 2273 | ` *` |
|        - | 2274 | ` * Pop P1 elements from the operand stack.` |
|        - | 2275 | ` */` |
|  1001187 | 2276 | `case PH7_OP_POP: {` |
|  2005309 | 2277 | `	sxi32 n = pInstr->iP1;` |
|  2005309 | 2278 | `	if( &pTos[-n+1] < pStack ){` |
|        - | 2279 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      412 | 2280 | `		n = (sxi32)(pTos - pStack);` |
|      204 | 2281 | `	}` |
|  2005309 | 2282 | `	VmPopOperand(&pTos,n);` |
|  2005309 | 2283 | `	break;` |
|        - | 2284 | `				 }` |
|        - | 2285 | `/*` |
|        - | 2286 | ` * DUP: * * *` |
|        - | 2287 | ` *` |
|        - | 2288 | ` * Duplicate the top of the stack.` |
|        - | 2289 | ` */` |
|       67 | 2290 | `case PH7_OP_DUP:` |
|        - | 2291 | `#ifdef UNTRUST` |
|        - | 2292 | `	if( pTos < pStack ){` |
|        - | 2293 | `		goto Abort;` |
|        - | 2294 | `	}` |
|        - | 2295 | `#endif` |
|      138 | 2296 | `	pTos++;` |
|      138 | 2297 | `	PH7_MemObjInit(pVm,pTos);` |
|      138 | 2298 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|      138 | 2299 | `	break;` |
|        - | 2300 | `/*` |
|        - | 2301 | ` * CLASS_DEFER: * * P3` |
|        - | 2302 | ` *` |
|        - | 2303 | ` * Execute a class declaration whose parent/interface/trait could not be` |
|        - | 2304 | ` * resolved when the enclosing file compiled (its autoloader had not RUN yet).` |
|        - | 2305 | ` * P3 is the VmDeferredClass record captured by the compiler. A dependency` |
|        - | 2306 | `` * that is STILL missing throws php's catchable `... not found` Error; a`` |
|        - | 2307 | ` * failed re-compile aborts (its errors were already reported). See the` |
|        - | 2308 | ` * deferral block comment in compile_class.c.` |
|        - | 2309 | ` */` |
|       15 | 2310 | `case PH7_OP_CLASS_DEFER: {` |
|       32 | 2311 | `	VmDeferredClass *pDefer = (VmDeferredClass *)pInstr->p3;` |
|       32 | 2312 | `	VmDeferredReq *pMissing = 0;` |
|       32 | 2313 | `	sxi32 rcDecl = SXRET_OK;` |
|       32 | 2314 | `	if( pDefer ){` |
|       32 | 2315 | `		rcDecl = VmExecDeferredClass(&(*pVm),pDefer,&pMissing);` |
|       15 | 2316 | `	}` |
|       32 | 2317 | `	if( pMissing ){` |
|        - | 2318 | `		static const char *azDeferKind[] = { "Class", "Interface", "Trait" };` |
|        - | 2319 | `		char zDeclMsg[520];` |
|       17 | 2320 | `		sxu32 nDeclMsg = SyBufferFormat(zDeclMsg,sizeof(zDeclMsg),"%s \"%.*s\" not found",` |
|        5 | 2321 | `			azDeferKind[pMissing->cKind < 3 ? pMissing->cKind : 0],` |
|       10 | 2322 | `			(int)pMissing->sName.nByte,pMissing->sName.zString);` |
|       12 | 2323 | `		rc = VmThrowFromVm(&(*pVm),"Error",zDeclMsg,nDeclMsg);` |
|       12 | 2324 | `		if( rc == SXERR_ABORT ){` |
|        3 | 2325 | `			goto Abort;` |
|        - | 2326 | `		}` |
|        9 | 2327 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2328 | `	}` |
|       22 | 2329 | `	if( rcDecl == SXERR_ABORT ){` |
|      ! 0 | 2330 | `		goto Abort;` |
|        - | 2331 | `	}` |
|       22 | 2332 | `	break;` |
|        - | 2333 | `				}` |
|        - | 2334 | `/*` |
|        - | 2335 | ` * CVT_INT: * * *` |
|        - | 2336 | ` *` |
|        - | 2337 | ` * Force the top of the stack to be an integer.` |
|        - | 2338 | ` */` |
|      304 | 2339 | `case PH7_OP_CVT_INT:` |
|        - | 2340 | `#ifdef UNTRUST` |
|        - | 2341 | `	if( pTos < pStack ){` |
|        - | 2342 | `		goto Abort;` |
|        - | 2343 | `	}` |
|        - | 2344 | `#endif` |
|      613 | 2345 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      445 | 2346 | `		PH7_MemObjToInteger(pTos);` |
|      220 | 2347 | `	}` |
|        - | 2348 | `	/* Invalidate any prior representation */` |
|      613 | 2349 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      613 | 2350 | `	break;` |
|        - | 2351 | `/*` |
|        - | 2352 | ` * CVT_REAL: * * *` |
|        - | 2353 | ` *` |
|        - | 2354 | ` * Force the top of the stack to be a real.` |
|        - | 2355 | ` */` |
|       43 | 2356 | `case PH7_OP_CVT_REAL:` |
|        - | 2357 | `#ifdef UNTRUST` |
|        - | 2358 | `	if( pTos < pStack ){` |
|        - | 2359 | `		goto Abort;` |
|        - | 2360 | `	}` |
|        - | 2361 | `#endif` |
|       89 | 2362 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 | 2363 | `		PH7_MemObjToReal(pTos);` |
|       26 | 2364 | `	}` |
|        - | 2365 | `	/* Invalidate any prior representation */` |
|       89 | 2366 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       89 | 2367 | `	break;` |
|        - | 2368 | `/*` |
|        - | 2369 | ` * CVT_STR: * * *` |
|        - | 2370 | ` *` |
|        - | 2371 | ` * Force the top of the stack to be a string.` |
|        - | 2372 | ` */` |
|     1724 | 2373 | `case PH7_OP_CVT_STR:` |
|        - | 2374 | `#ifdef UNTRUST` |
|        - | 2375 | `	if( pTos < pStack ){` |
|        - | 2376 | `		goto Abort;` |
|        - | 2377 | `	}` |
|        - | 2378 | `#endif` |
|        - | 2379 | `	/* (string) cast + string interpolation "$arr": php's user-visible` |
|        - | 2380 | `	 * array->string warning site, and the not-stringable-object throw (§2). */` |
|        - | 2381 | `	{` |
|     3453 | 2382 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     3455 | 2383 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2384 | `	}` |
|     3373 | 2385 | `	break;` |
|        - | 2386 | `/*` |
|        - | 2387 | ` * CVT_BOOL: * * *` |
|        - | 2388 | ` *` |
|        - | 2389 | ` * Force the top of the stack to be a boolean.` |
|        - | 2390 | ` */` |
|       19 | 2391 | `case PH7_OP_CVT_BOOL:` |
|        - | 2392 | `#ifdef UNTRUST` |
|        - | 2393 | `	if( pTos < pStack ){` |
|        - | 2394 | `		goto Abort;` |
|        - | 2395 | `	}` |
|        - | 2396 | `#endif` |
|       39 | 2397 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       39 | 2398 | `		PH7_MemObjToBool(pTos);` |
|       19 | 2399 | `	}` |
|       39 | 2400 | `	break;` |
|        - | 2401 | `/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it` |
|        - | 2402 | ` * always raises "The (unset) cast is no longer supported" (php 8 removed` |
|        - | 2403 | ` * the cast), so a program containing it never compiles. The switch has no` |
|        - | 2404 | ` * default arm, so abort loudly rather than fall through as a silent no-op` |
|        - | 2405 | ` * if a future emitter ever produces one without the compile error. */` |
|      ! 0 | 2406 | `case PH7_OP_CVT_NULL:` |
|      ! 0 | 2407 | `	goto Abort;` |
|        - | 2408 | `/*` |
|        - | 2409 | ` * CVT_NUMC: * * *` |
|        - | 2410 | ` *` |
|        - | 2411 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - | 2412 | ` */` |
|      ! 0 | 2413 | `case PH7_OP_CVT_NUMC:` |
|        - | 2414 | `#ifdef UNTRUST` |
|        - | 2415 | `	if( pTos < pStack ){` |
|        - | 2416 | `		goto Abort;` |
|        - | 2417 | `	}` |
|        - | 2418 | `#endif` |
|        - | 2419 | `	/* Force a numeric cast */` |
|      ! 0 | 2420 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 | 2421 | `	break;` |
|        - | 2422 | `/*` |
|        - | 2423 | ` * CVT_ARRAY: * * *` |
|        - | 2424 | ` *` |
|        - | 2425 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - | 2426 | ` */` |
|       63 | 2427 | `case PH7_OP_CVT_ARRAY:` |
|        - | 2428 | `#ifdef UNTRUST` |
|        - | 2429 | `	if( pTos < pStack ){` |
|        - | 2430 | `		goto Abort;` |
|        - | 2431 | `	}` |
|        - | 2432 | `#endif` |
|        - | 2433 | `	/* Force a hashmap cast */` |
|      131 | 2434 | `	rc = PH7_MemObjToHashmap(pTos);` |
|      131 | 2435 | `	if( rc != SXRET_OK ){` |
|        - | 2436 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 | 2437 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - | 2438 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 | 2439 | `	}` |
|      131 | 2440 | `	break;` |
|        - | 2441 | `/*` |
|        - | 2442 | ` * CVT_OBJ: * * *` |
|        - | 2443 | ` *` |
|        - | 2444 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - | 2445 | ` */` |
|       25 | 2446 | `case PH7_OP_CVT_OBJ:` |
|        - | 2447 | `#ifdef UNTRUST` |
|        - | 2448 | `	if( pTos < pStack ){` |
|        - | 2449 | `		goto Abort;` |
|        - | 2450 | `	}` |
|        - | 2451 | `#endif` |
|       52 | 2452 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 2453 | `		/* Force a 'stdClass()' cast */` |
|       52 | 2454 | `		PH7_MemObjToObject(pTos);` |
|       25 | 2455 | `	}` |
|       52 | 2456 | `	break;` |
|        - | 2457 | `/*` |
|        - | 2458 | ` * ERR_CTRL * * *` |
|        - | 2459 | ` *` |
|        - | 2460 | ` * Error control operator.` |
|        - | 2461 | ` */` |
|     3700 | 2462 | `case PH7_OP_UNSET_VAR: {` |
|        - | 2463 | `	VmOpRc rcOp;` |
|     7405 | 2464 | `	sState.pTos = pTos;` |
|     7405 | 2465 | `	sState.pc = pc;` |
|     7405 | 2466 | `	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);` |
|     7405 | 2467 | `	pTos = sState.pTos;` |
|     7405 | 2468 | `	pc = sState.pc;` |
|     7405 | 2469 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 2470 | `		goto Abort;` |
|     7403 | 2471 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2472 | `		goto Exception;` |
|        - | 2473 | `	}` |
|     7403 | 2474 | `	break;` |
|        - | 2475 | `					  }` |
|    39175 | 2476 | `case PH7_OP_ERR_CTRL:` |
|        - | 2477 | `	/*` |
|        - | 2478 | `	 * Error-control operator '@'. Emitted as a PAIR around the suppressed` |
|        - | 2479 | `	 * expression: iP1=1 opens the window (before the operand is evaluated),` |
|        - | 2480 | `	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),` |
|        - | 2481 | `	 * which went unnoticed only because the engine raised so few diagnostics;` |
|        - | 2482 | `	 * every warning/notice/deprecation the parity work added leaked straight` |
|        - | 2483 | ``	 * through `@`. The window nests, and php 8 does NOT let '@' swallow`` |
|        - | 2484 | `	 * fatals or exceptions — those unwind past the closing instruction, so` |
|        - | 2485 | `	 * the depth is reset on the exception path rather than decremented here.` |
|        - | 2486 | `	 */` |
|    78355 | 2487 | `	if( pInstr->iP1 ){` |
|    39241 | 2488 | `		pVm->nErrSuppress++;` |
|    58737 | 2489 | `	}else if( pVm->nErrSuppress > 0 ){` |
|    39119 | 2490 | `		pVm->nErrSuppress--;` |
|    19557 | 2491 | `	}` |
|    78355 | 2492 | `	break;` |
|        - | 2493 | `/*` |
|        - | 2494 | ` * IS_A * * *` |
|        - | 2495 | ` *` |
|        - | 2496 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - | 2497 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - | 2498 | ` * holding a class name or an object).` |
|        - | 2499 | ` * Push TRUE on success. FALSE otherwise.` |
|        - | 2500 | ` */` |
|      230 | 2501 | `case PH7_OP_IS_A:{` |
|      465 | 2502 | `	ph7_value *pNos = &pTos[-1];` |
|      465 | 2503 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - | 2504 | `#ifdef UNTRUST` |
|        - | 2505 | `	if( pNos < pStack ){` |
|        - | 2506 | `		goto Abort;` |
|        - | 2507 | `	}` |
|        - | 2508 | `#endif` |
|      465 | 2509 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      455 | 2510 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      455 | 2511 | `		ph7_class *pClass = 0;` |
|        - | 2512 | `		/* Extract the target class */` |
|      455 | 2513 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - | 2514 | `			/* Instance already loaded */` |
|      ! 0 | 2515 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      455 | 2516 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      455 | 2517 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      455 | 2518 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - | 2519 | `			/* Handle self/static/parent keywords */` |
|      455 | 2520 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        6 | 2521 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      453 | 2522 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 | 2523 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      450 | 2524 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        6 | 2525 | `				pClass = PH7_VmResolveParentClass(&(*pVm));` |
|        4 | 2526 | `			}else{` |
|      445 | 2527 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - | 2528 | `			}` |
|      225 | 2529 | `		}` |
|      455 | 2530 | `		if( pClass ){` |
|        - | 2531 | `			/* Perform the query */` |
|      453 | 2532 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|      224 | 2533 | `		}` |
|      225 | 2534 | `	}` |
|        - | 2535 | `	/* Push result */` |
|      465 | 2536 | `	VmPopOperand(&pTos,1);` |
|      465 | 2537 | `	PH7_MemObjRelease(pTos);` |
|      465 | 2538 | `	pTos->x.iVal = iRes;` |
|      465 | 2539 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      465 | 2540 | `	break;` |
|        - | 2541 | `				 }` |
|        - | 2542 |  |
|        - | 2543 | `/*` |
|        - | 2544 | ` * LOADC P1 P2 *` |
|        - | 2545 | ` *` |
|        - | 2546 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - | 2547 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - | 2548 | ` */` |
|  4547644 | 2549 | `case PH7_OP_LOADC: {` |
|        - | 2550 | `	ph7_value *pObj;` |
|        - | 2551 | `	/* Reserve a room */` |
|  9100148 | 2552 | `	pTos++;` |
|  9100148 | 2553 | `	if( pInstr->iP1 & PH7_LOADC_NOKEY ){` |
|        - | 2554 | `		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */` |
|   728423 | 2555 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|   728423 | 2556 | `		SyBlobReset(&pTos->sBlob);` |
|   728423 | 2557 | `		pTos->iFlags \|= MEMOBJ_AUX_NOKEY;` |
|   728423 | 2558 | `		pTos->nIdx = SXU32_HIGH;` |
|   728423 | 2559 | `		break;` |
|        - | 2560 | `	}` |
|  8371730 | 2561 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  8371730 | 2562 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - | 2563 | `			SyHashEntry *pEntry;` |
|        - | 2564 | `			/* The name php looks up FIRST for an unqualified constant is decided at` |
|        - | 2565 | ``			 * COMPILE time and travels in p3: a `use const` import's FQN, or`` |
|        - | 2566 | ``			 * `current-namespace\NAME`. Absent (global scope, or an already-qualified`` |
|        - | 2567 | `			 * literal), the bare literal is the only name there is. Resolving it here` |
|        - | 2568 | `			 * rather than against the RUNTIME namespace is what makes a function keep` |
|        - | 2569 | `			 * its own namespace when it is called from another one, and what lets a` |
|        - | 2570 | `			 * namespaced constant shadow a global one of the same short name. */` |
|   128009 | 2571 | `			const char *zCand = (const char *)pInstr->p3;` |
|   128009 | 2572 | `			const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|   128009 | 2573 | `			sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|   128009 | 2574 | `			if( zCand ){` |
|       55 | 2575 | `				pEntry = SyHashGet(&pVm->hConstant,zCand,SyStrlen(zCand));` |
|       55 | 2576 | `				if( pEntry ){` |
|       47 | 2577 | `					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       47 | 2578 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       47 | 2579 | `					SyBlobReset(&pTos->sBlob);` |
|       47 | 2580 | `					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|       47 | 2581 | `					pTos->nIdx = SXU32_HIGH;` |
|       47 | 2582 | `					break;` |
|        - | 2583 | `				}` |
|        4 | 2584 | `			}` |
|        - | 2585 | `			/* The GLOBAL step — skipped when the candidate came from an import, which` |
|        - | 2586 | `			 * php resolves without any fallback. */` |
|   127967 | 2587 | `			if( (pInstr->iP1 & PH7_LOADC_NOGLOBAL) == 0 ){` |
|   127965 | 2588 | `				pEntry = SyHashGet(&pVm->hConstant,(const void *)zLit,nLit);` |
|   127965 | 2589 | `				if( pEntry ){` |
|   127797 | 2590 | `					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - | 2591 | `					/* Set a NULL default value */` |
|   127797 | 2592 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|   127797 | 2593 | `					SyBlobReset(&pTos->sBlob);` |
|        - | 2594 | `					/* Invoke the callback and deal with the expanded value */` |
|   127797 | 2595 | `					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|        - | 2596 | `					/* Mark as constant */` |
|   127797 | 2597 | `					pTos->nIdx = SXU32_HIGH;` |
|   127797 | 2598 | `					break;` |
|        - | 2599 | `				}` |
|       84 | 2600 | `			}` |
|        - | 2601 | `			{` |
|        - | 2602 | `				/*` |
|        - | 2603 | `				 * php 8 has no bare-word fallback: an unresolved constant is a catchable` |
|        - | 2604 | `				 * Error, not its own name as a string. PH7 answered "X" for an unknown` |
|        - | 2605 | `				 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —` |
|        - | 2606 | `				 * silently became a string and flowed on. php names the name it looked` |
|        - | 2607 | `				 * for FIRST, so the message reports the candidate when there was one` |
|        - | 2608 | ``				 * ("Undefined constant \"B\NOPE\"" inside `namespace B;`).`` |
|        - | 2609 | `				 *` |
|        - | 2610 | `				 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call` |
|        - | 2611 | ``				 * boundary, so neither a bare `break` nor `goto Exception` is correct`` |
|        - | 2612 | `				 * here (see the macro).` |
|        - | 2613 | `				 */` |
|        - | 2614 | `				SyBlob sMsg;` |
|      175 | 2615 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      175 | 2616 | `				if( zCand ){` |
|        8 | 2617 | `					SyBlobFormat(&sMsg,"Undefined constant \"%s\"",zCand);` |
|        5 | 2618 | `				}else{` |
|      168 | 2619 | `					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);` |
|        - | 2620 | `				}` |
|      175 | 2621 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      175 | 2622 | `				SyBlobReset(&pTos->sBlob);` |
|      175 | 2623 | `				pTos->nIdx = SXU32_HIGH;` |
|      260 | 2624 | `				rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|       85 | 2625 | `					SyBlobLength(&sMsg));` |
|      175 | 2626 | `				SyBlobRelease(&sMsg);` |
|      175 | 2627 | `				if( rc == SXERR_ABORT ){` |
|       49 | 2628 | `					goto Abort;` |
|        - | 2629 | `				}` |
|      149 | 2630 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2631 | `			}` |
|        - | 2632 | `		}` |
|  8243726 | 2633 | `		PH7_MemObjLoad(pObj,pTos);` |
|  4124293 | 2634 | `	}else{` |
|        - | 2635 | `		/* Set a NULL value */` |
|      ! 0 | 2636 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 2637 | `	}` |
|        - | 2638 | `	/* Mark as constant */` |
|  8243726 | 2639 | `	pTos->nIdx = SXU32_HIGH;` |
|  8243726 | 2640 | `	break;` |
|        - | 2641 | `				  }` |
|        - | 2642 | `/*` |
|        - | 2643 | ` * LOAD: P1 * P3` |
|        - | 2644 | ` *` |
|        - | 2645 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - | 2646 | ` * from the P3 operand.` |
|        - | 2647 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - | 2648 | ` * the variable if non existent and push the NULL constant instead.` |
|        - | 2649 | ` */` |
|  3477483 | 2650 | `case PH7_OP_LOAD:{` |
|        - | 2651 | `	ph7_value *pObj;` |
|        - | 2652 | `	SyString sName;` |
|  6966137 | 2653 | `	if( pInstr->p3 == 0 ){` |
|        - | 2654 | `		/* Take the variable name from the top of the stack */` |
|        - | 2655 | `#ifdef UNTRUST` |
|        - | 2656 | `		if( pTos < pStack ){` |
|        - | 2657 | `			goto Abort;` |
|        - | 2658 | `		}` |
|        - | 2659 | `#endif` |
|        - | 2660 | `		/* Force a string cast — variable-variable name $$arr (user-visible, §2) */` |
|        - | 2661 | `		{` |
|       39 | 2662 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|  3477521 | 2663 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2664 | `		}` |
|       37 | 2665 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       20 | 2666 | `	}else{` |
|  6966101 | 2667 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 2668 | `		/* Reserve a room for the target object */` |
|  6966101 | 2669 | `		pTos++;` |
|        - | 2670 | `	}` |
|  6966135 | 2671 | `	if( pInstr->iP2 == 2 ){` |
|        - | 2672 | ``		/* Read-modify-write target (`$x++`, `$x .= 'a'`): php reads the variable`` |
|        - | 2673 | `		 * before writing, so it warns when it does not exist and THEN seeds it.` |
|        - | 2674 | `		 * Peek first (no create) purely to raise that warning; the load below` |
|        - | 2675 | ``		 * still creates the slot the operator needs. A plain `=` never gets here`` |
|        - | 2676 | `		 * — it writes without reading, and stays silent, as php does. */` |
|   707166 | 2677 | `		if( VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE) == 0 ){` |
|        7 | 2678 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        3 | 2679 | `		}` |
|   353999 | 2680 | `	}` |
|        - | 2681 | `	/* Extract the requested memory object */` |
|  6966135 | 2682 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  6966135 | 2683 | `	if( pObj == 0 ){` |
|      145 | 2684 | `		if( pInstr->iP1 ){` |
|        - | 2685 | `			/* Reading a variable that does not exist. php raises E_WARNING` |
|        - | 2686 | `			 * "Undefined variable $x" and evaluates it as NULL; PHL used to` |
|        - | 2687 | `			 * yield NULL silently, which hid typo'd names. iP2 marks the reads` |
|        - | 2688 | `			 * that must stay quiet (isset/empty — see PH7_CompileVariable);` |
|        - | 2689 | `			 * vivifying contexts never get here because they pass iP1 = 0. */` |
|      145 | 2690 | `			if( pInstr->iP2 == 0 ){` |
|       40 | 2691 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|       18 | 2692 | `			}` |
|        - | 2693 | `			/* Variable not found,load NULL */` |
|      145 | 2694 | `			if( !pInstr->p3 ){` |
|       10 | 2695 | `				PH7_MemObjRelease(pTos);` |
|        6 | 2696 | `			}else{` |
|      137 | 2697 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 2698 | `			}` |
|      145 | 2699 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|      145 | 2700 | `			if( pInstr->iP2 == 3 ){` |
|        - | 2701 | `				/* D1 deferred call argument: the variable does not exist, but we do not` |
|        - | 2702 | `				 * know yet whether the callee wants it by-ref (materialize + bind) or` |
|        - | 2703 | `				 * by-value (warn + pass NULL). Tag the slot and stash the variable name` |
|        - | 2704 | `				 * (a VM-lifetime bytecode string, nothing to free) so OP_CALL's` |
|        - | 2705 | `				 * PH7_VmResolveDeferredArgs can decide once the callee is resolved. */` |
|       28 | 2706 | `				pTos->iFlags \|= MEMOBJ_AUX_DEFERRED;` |
|       28 | 2707 | `				pTos->x.pOther = pInstr->p3;` |
|       13 | 2708 | `			}` |
|      145 | 2709 | `			break;` |
|      ! 0 | 2710 | `		}else{` |
|        - | 2711 | `			/* Fatal error */` |
|      ! 0 | 2712 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 2713 | `			goto Abort;` |
|        - | 2714 | `		}` |
|        - | 2715 | `	}` |
|        - | 2716 | `	/* Load variable contents */` |
|  6965995 | 2717 | `	PH7_MemObjLoad(pObj,pTos);` |
|  6965995 | 2718 | `	pTos->nIdx = pObj->nIdx;` |
|  6965995 | 2719 | `	break;` |
|        - | 2720 | `				   }` |
|        - | 2721 | `/*` |
|        - | 2722 | ` * LOAD_MAP P1 * *` |
|        - | 2723 | ` *` |
|        - | 2724 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - | 2725 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - | 2726 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - | 2727 | ` */` |
|    46365 | 2728 | `case PH7_OP_LOAD_MAP: {` |
|        - | 2729 | `	VmOpRc rcOp;` |
|    92735 | 2730 | `	sState.pTos = pTos;` |
|    92735 | 2731 | `	sState.pc = pc;` |
|    92735 | 2732 | `	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);` |
|    92735 | 2733 | `	pTos = sState.pTos;` |
|    92735 | 2734 | `	pc = sState.pc;` |
|    92735 | 2735 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2736 | `		goto Abort;` |
|    92735 | 2737 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       15 | 2738 | `		goto Exception;` |
|        - | 2739 | `	}` |
|    92721 | 2740 | `	break;` |
|        - | 2741 | `					  }` |
|        - | 2742 | `/*` |
|        - | 2743 | ` * LOAD_LIST: P1 * *` |
|        - | 2744 | ` *` |
|        - | 2745 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - | 2746 | ` * This is the VM implementation of the list() PHP construct.` |
|        - | 2747 | ` * Caveats:` |
|        - | 2748 | ` *  This implementation support only a single nesting level.` |
|        - | 2749 | ` */` |
|      445 | 2750 | `case PH7_OP_LOAD_LIST: {` |
|        - | 2751 | `	VmOpRc rcOp;` |
|      895 | 2752 | `	sState.pTos = pTos;` |
|      895 | 2753 | `	sState.pc = pc;` |
|      895 | 2754 | `	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);` |
|      895 | 2755 | `	pTos = sState.pTos;` |
|      895 | 2756 | `	pc = sState.pc;` |
|      895 | 2757 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2758 | `		goto Abort;` |
|      895 | 2759 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2760 | `		goto Exception;` |
|        - | 2761 | `	}` |
|      895 | 2762 | `	break;` |
|        - | 2763 | `					  }` |
|        - | 2764 | `/*` |
|        - | 2765 | ` * LOAD_IDX: P1 P2 *` |
|        - | 2766 | ` *` |
|        - | 2767 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - | 2768 | ` * from the stack.` |
|        - | 2769 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - | 2770 | ` * instead.` |
|        - | 2771 | ` */` |
|   482577 | 2772 | `case PH7_OP_LOAD_IDX: {` |
|        - | 2773 | `	VmOpRc rcOp;` |
|   967181 | 2774 | `	sState.pTos = pTos;` |
|   967181 | 2775 | `	sState.pc = pc;` |
|   967181 | 2776 | `	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);` |
|   967181 | 2777 | `	pTos = sState.pTos;` |
|   967181 | 2778 | `	pc = sState.pc;` |
|   967181 | 2779 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2780 | `		goto Abort;` |
|   967181 | 2781 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       60 | 2782 | `		goto Exception;` |
|        - | 2783 | `	}` |
|   967125 | 2784 | `	break;` |
|        - | 2785 | `					  }` |
|        - | 2786 | `/*` |
|        - | 2787 | ` * LOAD_CLOSURE * * P3` |
|        - | 2788 | ` *` |
|        - | 2789 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - | 2790 | ` * name in the stack.` |
|        - | 2791 | ` */` |
|     3343 | 2792 | `case PH7_OP_LOAD_CLOSURE: {` |
|        - | 2793 | `	VmOpRc rcOp;` |
|     6691 | 2794 | `	sState.pTos = pTos;` |
|     6691 | 2795 | `	sState.pc = pc;` |
|     6691 | 2796 | `	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);` |
|     6691 | 2797 | `	pTos = sState.pTos;` |
|     6691 | 2798 | `	pc = sState.pc;` |
|     6691 | 2799 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2800 | `		goto Abort;` |
|     6691 | 2801 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2802 | `		goto Exception;` |
|        - | 2803 | `	}` |
|     6691 | 2804 | `	break;` |
|        - | 2805 | `					  }` |
|        - | 2806 | `/*` |
|        - | 2807 | ` * LOAD_FCC P1 * *` |
|        - | 2808 | ` *` |
|        - | 2809 | ` * First-class callable: wrap the callee in a Closure object instead of calling it.` |
|        - | 2810 | ` *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS` |
|        - | 2811 | ` *           (from the callee's OP_LOADC). Replace it in place with a Closure whose` |
|        - | 2812 | ` *           $__fn is that name; the existing string-callable dispatch resolves it.` |
|        - | 2813 | ` *           (OOM degrades to leaving the name string on the stack — still callable.)` |
|        - | 2814 | ` *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name` |
|        - | 2815 | ` *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds` |
|        - | 2816 | ` *           $this (scope = its class); a class-name-string target is a static callable` |
|        - | 2817 | ` *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —` |
|        - | 2818 | ` *           the popped target leaves no name string to keep.)` |
|        - | 2819 | ` */` |
|      155 | 2820 | `case PH7_OP_LOAD_FCC:{` |
|      315 | 2821 | `	if( pInstr->iP1 == 1 ){` |
|        - | 2822 | ``		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)`` |
|        - | 2823 | `		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —` |
|        - | 2824 | `		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,` |
|        - | 2825 | ``		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a`` |
|        - | 2826 | `		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is` |
|        - | 2827 | `		 * (graceful degradation), and so is the original on OOM — still whatever it was. */` |
|        - | 2828 | `		ph7_class_instance *pCloObj;` |
|        - | 2829 | `		sxi32 nFccBrc;` |
|        - | 2830 | `		const void *pFccRes;` |
|      140 | 2831 | `		if( VmValueIsClosure(pVm, pTos) ){` |
|       11 | 2832 | `			break;` |
|        - | 2833 | `		}` |
|        - | 2834 | `		/* php's global fallback for an UNQUALIFIED function name written inside a` |
|        - | 2835 | `		 * namespace: the current namespace first, the global one after. The compiler` |
|        - | 2836 | `		 * qualified this name and marks the instruction (iP2==1) when it did, so the` |
|        - | 2837 | `		 * fallback happens here — the OP_CALL path does the same thing from its arg map,` |
|        - | 2838 | ``		 * which an FCC has none of. `strlen(...)` in a namespaced file was`` |
|        - | 2839 | ``		 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|      126 | 2840 | `		if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING)` |
|       14 | 2841 | `			&& !PH7_VmIsCallable(pVm,pTos,TRUE) ){` |
|        7 | 2842 | `			const char *zFccName = (const char *)SyBlobData(&pTos->sBlob);` |
|        7 | 2843 | `			sxu32 nFccName = SyBlobLength(&pTos->sBlob);` |
|        7 | 2844 | `			const char *zFccShort = zFccName;` |
|        - | 2845 | `			sxu32 iFccPos;` |
|      165 | 2846 | `			for( iFccPos = 0 ; iFccPos < nFccName ; ++iFccPos ){` |
|      159 | 2847 | `				if( zFccName[iFccPos] == '\\' ){` |
|        7 | 2848 | `					zFccShort = &zFccName[iFccPos + 1];` |
|        3 | 2849 | `				}` |
|       80 | 2850 | `			}` |
|        7 | 2851 | `			if( zFccShort != zFccName ){` |
|        - | 2852 | `				ph7_value sFccShort;` |
|        7 | 2853 | `				PH7_MemObjInit(pVm,&sFccShort);` |
|       10 | 2854 | `				PH7_MemObjStringAppend(&sFccShort,zFccShort,` |
|        6 | 2855 | `					(sxu32)(nFccName - (sxu32)(zFccShort - zFccName)));` |
|        7 | 2856 | `				if( PH7_VmIsCallable(pVm,&sFccShort,TRUE) ){` |
|        5 | 2857 | `					PH7_MemObjStore(&sFccShort,pTos);` |
|        2 | 2858 | `				}` |
|        7 | 2859 | `				PH7_MemObjRelease(&sFccShort);` |
|        3 | 2860 | `			}` |
|        3 | 2861 | `		}` |
|        - | 2862 | `		/* The array shape's class lookup can run an autoloader that throws; php propagates` |
|        - | 2863 | `		 * THAT exception and never reports the callable bad, exactly as at the OP_CALL sites. */` |
|      130 | 2864 | `		nFccBrc = pVm->nBoundaryRc;` |
|      130 | 2865 | `		pFccRes = (const void *)pVm->pResumeFrame;` |
|      130 | 2866 | `		pCloObj = VmFccWrapValue(pVm, pTos);` |
|      130 | 2867 | `		if( pCloObj ){` |
|      100 | 2868 | `			PH7_MemObjRelease(pTos);` |
|      100 | 2869 | `			pCloObj->iRef++;` |
|      100 | 2870 | `			pTos->x.pOther = pCloObj;` |
|      100 | 2871 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       52 | 2872 | `		}else{` |
|        - | 2873 | `			/* php refuses a non-callable HERE, with the direct dispatch's own wording — the` |
|        - | 2874 | ``			 * `(...)` does not make a bad callable acceptable, it just defers the call. */`` |
|        - | 2875 | `			char zFccMsg[192];` |
|       31 | 2876 | `			const char *zFccBad = VmFccValueError(&(*pVm),pTos,zFccMsg,sizeof(zFccMsg));` |
|       31 | 2877 | `			int bFccRaised = PH7_VmClassLookupRaised(&(*pVm),nFccBrc,pFccRes);` |
|       31 | 2878 | `			if( zFccBad \|\| bFccRaised ){` |
|        - | 2879 | `				sxi32 rcFcc;` |
|       31 | 2880 | `				PH7_MemObjRelease(pTos);` |
|       31 | 2881 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       31 | 2882 | `				pTos->nIdx = SXU32_HIGH;` |
|       31 | 2883 | `				if( bFccRaised ){` |
|      ! 0 | 2884 | `					rcFcc = pVm->nBoundaryRc;` |
|      ! 0 | 2885 | `					pVm->nBoundaryRc = 0;` |
|      ! 0 | 2886 | `					if( rcFcc == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 2887 | `					rc = PH7_EXCEPTION;` |
|       15 | 2888 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2889 | `				}` |
|       31 | 2890 | `				rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccBad,(sxu32)SyStrlen(zFccBad));` |
|       31 | 2891 | `				if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       31 | 2892 | `				rc = rcFcc;` |
|       61 | 2893 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2894 | `			}` |
|        - | 2895 | `		}` |
|       52 | 2896 | `	}else{` |
|        - | 2897 | `		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]` |
|        - | 2898 | `		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its` |
|        - | 2899 | `		 * class); a class-name string target is a static callable (scope = that class). */` |
|      179 | 2900 | `		ph7_value *pTarget = &pTos[-1];` |
|        - | 2901 | `		SyString sName;` |
|        - | 2902 | `		ph7_class_instance *pCloObj;` |
|      179 | 2903 | `		ph7_class *pFccCls = 0;` |
|      179 | 2904 | `		ph7_class_instance *pFccRecv = 0;` |
|      179 | 2905 | `		const char *zFccErr = 0;` |
|        - | 2906 | `		char zFccMsg[192];` |
|      179 | 2907 | `		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));` |
|      179 | 2908 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      105 | 2909 | `			pFccRecv = (ph7_class_instance *)pTarget->x.pOther;` |
|      105 | 2910 | `			pFccCls = pFccRecv->pClass;` |
|      105 | 2911 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pFccCls) ){` |
|        - | 2912 | ``				/* `$inc->m(...)` resolves the method at CREATION, so php's`` |
|        - | 2913 | `				 * incomplete-object call Error is raised here, not at a later` |
|        - | 2914 | `				 * invocation. */` |
|        - | 2915 | `				SyBlob sIncErr;` |
|        - | 2916 | `				sxi32 rcInc;` |
|        3 | 2917 | `				SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|        3 | 2918 | `				PH7_VmIncompleteMsg(&(*pVm),pFccRecv,"call a method",&sIncErr);` |
|        3 | 2919 | `				VmPopOperand(&pTos,1);       /* the method name */` |
|        3 | 2920 | `				PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|        3 | 2921 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 2922 | `				pTos->nIdx = SXU32_HIGH;` |
|        4 | 2923 | `				rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|        1 | 2924 | `					SyBlobLength(&sIncErr));` |
|        3 | 2925 | `				SyBlobRelease(&sIncErr);` |
|        3 | 2926 | `				if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|        3 | 2927 | `				rc = rcInc;` |
|        3 | 2928 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        5 | 2929 | `			}` |
|      126 | 2930 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|        - | 2931 | ``			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class`` |
|        - | 2932 | `			 * now, so the closure binds the concrete scope (matching PHP). */` |
|       77 | 2933 | `			pFccCls = VmFccResolveScope(pVm, pTarget);` |
|       37 | 2934 | `		}` |
|      177 | 2935 | `		if( pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING) ){` |
|        - | 2936 | `			/* php resolves the member HERE, through the same lookup the call would use:` |
|        - | 2937 | `			 * every refusal a call would raise is raised at CREATION, and a non-static` |
|        - | 2938 | `			 * method named through a class binds the calling frame's own $this. */` |
|      177 | 2939 | `			ph7_class_instance *pRecvOut = 0;` |
|      263 | 2940 | `			zFccErr = VmFccMemberError(&(*pVm),pFccCls,` |
|      172 | 2941 | `				(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|       86 | 2942 | `				SyStringData(&sName),SyStringLength(&sName),` |
|      172 | 2943 | `				(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|       86 | 2944 | `				&pRecvOut,zFccMsg,sizeof(zFccMsg));` |
|      177 | 2945 | `			if( pRecvOut ){` |
|       13 | 2946 | ``				pFccRecv = pRecvOut; /* the receiver php binds into a `C::m(...)` callable */`` |
|        6 | 2947 | `			}` |
|       86 | 2948 | `		}` |
|      177 | 2949 | `		if( zFccErr ){` |
|        - | 2950 | `			sxi32 rcFcc;` |
|       24 | 2951 | `			VmPopOperand(&pTos,1);       /* the method name */` |
|       24 | 2952 | `			PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|       24 | 2953 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       24 | 2954 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 | 2955 | `			rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccErr,(sxu32)SyStrlen(zFccErr));` |
|       24 | 2956 | `			if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       24 | 2957 | `			rc = rcFcc;` |
|       30 | 2958 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2959 | `		}` |
|      154 | 2960 | `		if( pFccCls == 0 ){` |
|      ! 0 | 2961 | `			pCloObj = 0;` |
|      154 | 2962 | `		}else if( pFccRecv ){` |
|      104 | 2963 | `			pCloObj = VmCreateClosure(pVm, &sName, pFccRecv, &pFccRecv->pClass->sName);` |
|       54 | 2964 | `		}else{` |
|       53 | 2965 | `			pCloObj = VmCreateClosure(pVm, &sName, 0, &pFccCls->sName);` |
|        - | 2966 | `		}` |
|      154 | 2967 | `		if( pCloObj ){` |
|        - | 2968 | ``			/* `$o->m(...)` / `C::m(...)` names a METHOD, whatever the class turns out to`` |
|        - | 2969 | `			 * declare: the unwrap must not go looking for a FUNCTION of that name, and a` |
|        - | 2970 | `			 * name the class answers only through __call is still a method call. */` |
|      154 | 2971 | `			pCloObj->iFlags \|= VM_INSTANCE_FCC_METHOD;` |
|        - | 2972 | `			/* The screen above already ran, HERE, where php runs it — so record that this` |
|        - | 2973 | `			 * closure's callee is settled and the invocation must not re-decide it. A name` |
|        - | 2974 | `			 * that resolved to the catch-all instead keeps routing there. */` |
|      154 | 2975 | `			if( PH7_VmFccMethodIsDirect(&(*pVm),pFccCls,SyStringData(&sName),SyStringLength(&sName)) ){` |
|      140 | 2976 | `				pCloObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       68 | 2977 | `			}` |
|       75 | 2978 | `		}` |
|        - | 2979 | `		/* Pop the method name and the target, push the Closure. */` |
|      154 | 2980 | `		PH7_MemObjRelease(pTos);` |
|      154 | 2981 | `		pTos--;` |
|      154 | 2982 | `		PH7_MemObjRelease(pTos);` |
|      154 | 2983 | `		if( pCloObj ){` |
|      154 | 2984 | `			pCloObj->iRef++;` |
|      154 | 2985 | `			pTos->x.pOther = pCloObj;` |
|      154 | 2986 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       79 | 2987 | `		}else{` |
|      ! 0 | 2988 | `			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */` |
|        - | 2989 | `		}` |
|        - | 2990 | `	}` |
|      250 | 2991 | `	break;` |
|        - | 2992 | `					 }` |
|        - | 2993 | `/*` |
|        - | 2994 | ` * STORE * P2 P3` |
|        - | 2995 | ` *` |
|        - | 2996 | ` * Perform a store (Assignment) operation.` |
|        - | 2997 | ` */` |
|   465745 | 2998 | `case PH7_OP_STORE: {` |
|        - | 2999 | `	ph7_value *pObj;` |
|        - | 3000 | `	SyString sName;` |
|        - | 3001 | `#ifdef UNTRUST` |
|        - | 3002 | `	if( pTos < pStack ){` |
|        - | 3003 | `		goto Abort;` |
|        - | 3004 | `	}` |
|        - | 3005 | `#endif` |
|   933587 | 3006 | `	if( pInstr->iP2 ){` |
|        - | 3007 | `		sxu32 nIdx;` |
|        - | 3008 | `		sxi32 rcT;` |
|        - | 3009 | `		/* Member store operation */` |
|   101821 | 3010 | `		nIdx = pTos->nIdx;` |
|   101821 | 3011 | `		VmPopOperand(&pTos,1);` |
|   101821 | 3012 | `		if( pVm->pMagicSetThis ){` |
|        - | 3013 | `			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain` |
|        - | 3014 | `			 * store to a missing/inaccessible property on a class declaring __set.` |
|        - | 3015 | `			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on` |
|        - | 3016 | `			 * the stack as the assignment expression's result — php's semantics` |
|        - | 3017 | `			 * (no property is created; a throw rides the boundary rail). */` |
|       31 | 3018 | `			ph7_class_instance *pSetThis = pVm->pMagicSetThis;` |
|        - | 3019 | `			SyString sSetName;` |
|       31 | 3020 | `			pVm->pMagicSetThis = 0;` |
|       31 | 3021 | `			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));` |
|       31 | 3022 | `			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);` |
|       31 | 3023 | `			PH7_ClassInstanceUnref(pSetThis);` |
|       31 | 3024 | `			SyBlobReset(&pVm->sMagicSetName);` |
|       31 | 3025 | `			break;` |
|        - | 3026 | `		}` |
|   101793 | 3027 | `		if( pVm->pHookSetThis ){` |
|        - | 3028 | `			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a` |
|        - | 3029 | `			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with` |
|        - | 3030 | `			 * the rvalue (which stays on the stack as the assignment expression's` |
|        - | 3031 | ``			 * result); a `set => expr` hook's return value is stored into the`` |
|        - | 3032 | `			 * BACKING slot with the ordinary typed enforcement. A property with` |
|        - | 3033 | `			 * only a get hook is php's catchable "is read-only" Error. */` |
|       55 | 3034 | `			ph7_class_instance *pHThis = pVm->pHookSetThis;` |
|       55 | 3035 | `			ph7_class_attr *pHAttr = pVm->pHookSetAttr;` |
|       55 | 3036 | `			sxu32 nBackIdx = pVm->nHookSetIdx;` |
|        - | 3037 | `			sxi32 rcHs;` |
|       55 | 3038 | `			pVm->pHookSetThis = 0;` |
|       55 | 3039 | `			pVm->pHookSetAttr = 0;` |
|       55 | 3040 | `			pVm->nHookSetIdx = SXU32_HIGH;` |
|       55 | 3041 | `			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);` |
|       55 | 3042 | `			PH7_ClassInstanceUnref(pHThis);` |
|       55 | 3043 | `			if( rcHs == PH7_ABORT ){` |
|      ! 0 | 3044 | `				goto Abort;` |
|        - | 3045 | `			}` |
|       55 | 3046 | `			break;` |
|        - | 3047 | `		}` |
|   101741 | 3048 | `		if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 3049 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3050 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|      ! 0 | 3051 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 3052 | `		}else{` |
|        - | 3053 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - | 3054 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|   101741 | 3055 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);` |
|   101741 | 3056 | `			if( rcT == PH7_ABORT ){` |
|       13 | 3057 | `				goto Abort;` |
|        - | 3058 | `			}` |
|   101731 | 3059 | `			if( rcT == PH7_EXCEPTION ){` |
|        - | 3060 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - | 3061 | `				 * control to the nearest catch block if any (draining any` |
|        - | 3062 | `				 * abandoned outer-expression operands to the try's base),` |
|        - | 3063 | `				 * otherwise propagate out of the VM loop. */` |
|   100139 | 3064 | `				VmPopOperand(&pTos,1);` |
|        - | 3065 | `				{` |
|        - | 3066 | `					sxi32 iRp;` |
|   100139 | 3067 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|   400087 | 3068 | `						PH7_RESUME_DRAIN()` |
|   100085 | 3069 | `						pc = iRp;` |
|   100085 | 3070 | `						break;` |
|        - | 3071 | `					}` |
|        - | 3072 | `				}` |
|       59 | 3073 | `				goto Exception;` |
|        - | 3074 | `			}` |
|        - | 3075 | `			/* Point to the desired memory object */` |
|     1597 | 3076 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1597 | 3077 | `			if( pObj ){` |
|        - | 3078 | `				/* Perform the store operation */` |
|     1597 | 3079 | `				PH7_MemObjStore(pTos,pObj);` |
|      796 | 3080 | `			}` |
|        - | 3081 | `		}` |
|     1597 | 3082 | `		break;` |
|   831771 | 3083 | `	}else if( pInstr->p3 == 0 ){` |
|        - | 3084 | `		/* Take the variable name from the next on the stack (user-visible: a` |
|        - | 3085 | `		 * variable-variable NAME $$arr warns on an array, §2) */` |
|        - | 3086 | `		{` |
|       26 | 3087 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       26 | 3088 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3089 | `		}` |
|       24 | 3090 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       24 | 3091 | `		pTos--;` |
|        - | 3092 | `#ifdef UNTRUST` |
|        - | 3093 | `		if( pTos < pStack  ){` |
|        - | 3094 | `			goto Abort;` |
|        - | 3095 | `		}` |
|        - | 3096 | `#endif` |
|       13 | 3097 | `	}else{` |
|   831747 | 3098 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 3099 | `	}` |
|   831764 | 3100 | `	if( sName.nByte == sizeof("GLOBALS")-1` |
|   418253 | 3101 | `	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){` |
|        6 | 3102 | `		if( pInstr->p3 ){` |
|        - | 3103 | `			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time` |
|        - | 3104 | `			 * fatal there; raised at the store site here with the same` |
|        - | 3105 | `			 * message and the same non-catchable outcome). Element writes` |
|        - | 3106 | `			 * are unaffected. */` |
|        3 | 3107 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3108 | `				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 3109 | `			pVm->iExitStatus = 255;` |
|        3 | 3110 | `			pVm->bHaltRequested = 1;` |
|        3 | 3111 | `			goto Abort;` |
|        - | 3112 | `		}` |
|        - | 3113 | `		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's` |
|        - | 3114 | `		 * compile-time case: php quietly creates an ordinary symbol-table` |
|        - | 3115 | `		 * entry named GLOBALS, leaving the auto-global view intact. */` |
|        3 | 3116 | `		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);` |
|        3 | 3117 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 3118 | `		break;` |
|        - | 3119 | `	}` |
|        - | 3120 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   831765 | 3121 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   831765 | 3122 | `	if( pObj == 0 ){` |
|      ! 0 | 3123 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 3124 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 3125 | `		goto Abort;` |
|        - | 3126 | `	}` |
|   831765 | 3127 | `	if( !pInstr->p3 ){` |
|       22 | 3128 | `		PH7_MemObjRelease(&pTos[1]);` |
|       10 | 3129 | `	}` |
|        - | 3130 | `	/* Perform the store operation */` |
|   831765 | 3131 | `	PH7_MemObjStore(pTos,pObj);` |
|   831765 | 3132 | `	break;` |
|        - | 3133 | `				   }` |
|        - | 3134 | `/*` |
|        - | 3135 | ` * STORE_IDX:   P1 * P3` |
|        - | 3136 | ` * STORE_IDX_R: P1 * P3` |
|        - | 3137 | ` *` |
|        - | 3138 | ` * Perfrom a store operation an a hashmap entry.` |
|        - | 3139 | ` */` |
|   162475 | 3140 | `case PH7_OP_STORE_IDX:` |
|        - | 3141 | `case PH7_OP_STORE_IDX_REF: {` |
|        - | 3142 | `	VmOpRc rcOp;` |
|   324955 | 3143 | `	sState.pTos = pTos;` |
|   324955 | 3144 | `	sState.pc = pc;` |
|   324955 | 3145 | `	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);` |
|   324955 | 3146 | `	pTos = sState.pTos;` |
|   324955 | 3147 | `	pc = sState.pc;` |
|   324955 | 3148 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3149 | `		goto Abort;` |
|   324953 | 3150 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       37 | 3151 | `		goto Exception;` |
|        - | 3152 | `	}` |
|   324921 | 3153 | `	break;` |
|        - | 3154 | `					  }` |
|        - | 3155 | `/*` |
|        - | 3156 | ` * INCR: P1 * *` |
|        - | 3157 | ` *` |
|        - | 3158 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - | 3159 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - | 3160 | ` * the stack and increment after that.` |
|        - | 3161 | ` */` |
|   333248 | 3162 | `case PH7_OP_INCR:` |
|        - | 3163 | `#ifdef UNTRUST` |
|        - | 3164 | `	if( pTos < pStack ){` |
|        - | 3165 | `		goto Abort;` |
|        - | 3166 | `	}` |
|        - | 3167 | `#endif` |
|        - | 3168 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3169 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3170 | `	 * — which otherwise skips object/array/resource operands. */` |
|   667344 | 3171 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3172 | `	/* php refuses to increment a string OFFSET, whatever byte it holds. */` |
|   667334 | 3173 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3174 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3175 | `	 * php's TypeError, raised BEFORE any set dispatch (the type guard below` |
|        - | 3176 | `	 * would skip the mutation and the tail write-back would otherwise call` |
|        - | 3177 | `	 * the set hook with the unchanged value). */` |
|   667315 | 3178 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|   334093 | 3179 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        5 | 3180 | `		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        5 | 3181 | `		if( VM_HOOK_PEND_IS_RMW(pTopInc->iKind) && pTopInc->nScratchIdx == pTos->nIdx ){` |
|        - | 3182 | `			SyBlob sErrMsg;` |
|        5 | 3183 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 | 3184 | `			VmIncDecTypeErrorMsg(pTos,TRUE,&sErrMsg);` |
|        5 | 3185 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        5 | 3186 | `			VmHookRmwDropTop(&(*pVm));` |
|        5 | 3187 | `			pTos->nIdx = SXU32_HIGH;` |
|        5 | 3188 | `			break;` |
|        - | 3189 | `		}` |
|      ! 0 | 3190 | `	}` |
|   667316 | 3191 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3192 | `		/* php's ++ operand contract: an array, object or resource is a catchable` |
|        - | 3193 | `		 * TypeError, not the silent no-op this used to be. Settle the operand` |
|        - | 3194 | `		 * (the op's result slot) BEFORE throwing, like the arithmetic sites. */` |
|        - | 3195 | `		SyBlob sIncMsg;` |
|        - | 3196 | `		sxi32 rcInc;` |
|       21 | 3197 | `		SyBlobInit(&sIncMsg,&pVm->sAllocator);` |
|       21 | 3198 | `		VmIncDecTypeErrorMsg(pTos,TRUE,&sIncMsg);` |
|       21 | 3199 | `		PH7_MemObjRelease(pTos);` |
|       21 | 3200 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       21 | 3201 | `		pTos->nIdx = SXU32_HIGH;` |
|       31 | 3202 | `		rcInc = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sIncMsg),` |
|       10 | 3203 | `			SyBlobLength(&sIncMsg));` |
|       21 | 3204 | `		SyBlobRelease(&sIncMsg);` |
|       21 | 3205 | `		if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|       21 | 3206 | `		rc = rcInc;` |
|       23 | 3207 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3208 | `	}` |
|        - | 3209 | ``	/* php 8.3+: `++` on a BOOL is a no-op it warns about (E_WARNING, errno 2 —`` |
|        - | 3210 | ``	 * same shape as the null `--` below), where PH7 coerced true to int 2. */`` |
|   667296 | 3211 | `	if( pTos->iFlags & MEMOBJ_BOOL ){` |
|        7 | 3212 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3213 | `			"Increment on type bool has no effect, this will change in the next major version of PHP");` |
|        3 | 3214 | `	}` |
|   667296 | 3215 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_BOOL)) == 0 ){` |
|   667290 | 3216 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3217 | `			ph7_value *pObj;` |
|   667284 | 3218 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   667284 | 3219 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3220 | `					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric` |
|        - | 3221 | `					 * string (it points at str_increment() instead); PHL rejects it. */` |
|        - | 3222 | `					SyBlob sErrMsg;` |
|        3 | 3223 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3224 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3225 | `						"Increment on a non-numeric string is not supported, use str_increment() instead",` |
|        - | 3226 | `						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);` |
|        3 | 3227 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3228 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3229 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3230 | `					break;` |
|      ! 0 | 3231 | `				}else{` |
|        - | 3232 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - | 3233 | `					 * original value: pTos may alias pObj's blob via` |
|        - | 3234 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - | 3235 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - | 3236 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - | 3237 | `					 * so its old-value view survives the coercion. */` |
|   667282 | 3238 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 | 3239 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        5 | 3240 | `					}` |
|        - | 3241 | `					/* Force a numeric cast on the variable */` |
|   667282 | 3242 | `					PH7_MemObjToNumeric(pObj);` |
|   667282 | 3243 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        5 | 3244 | `						pObj->rVal++;` |
|        - | 3245 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3246 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3247 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3248 | `						 * integer-valued real. */` |
|        5 | 3249 | `						PH7_MemObjTryInteger(pObj);` |
|        3 | 3250 | `					}else{` |
|        - | 3251 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3252 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3253 | `						sxi64 r;` |
|   667278 | 3254 | `						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3255 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        5 | 3256 | `							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;` |
|        5 | 3257 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3258 | `#else` |
|        - | 3259 | `							pObj->x.iVal = r;` |
|        - | 3260 | `#endif` |
|        3 | 3261 | `						}else{` |
|   667274 | 3262 | `							pObj->x.iVal = r;` |
|        - | 3263 | `						}` |
|        - | 3264 | `					}` |
|   667282 | 3265 | `					if( pInstr->iP1 ){` |
|        - | 3266 | `						/* Pre-increment: result is the new value. */` |
|       69 | 3267 | `						PH7_MemObjStore(pObj,pTos);` |
|       34 | 3268 | `					}` |
|        - | 3269 | `					/* Post-increment: pTos retains the old value (a string` |
|        - | 3270 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - | 3271 | `				}` |
|   334057 | 3272 | `			}` |
|   334062 | 3273 | `		}else{` |
|        7 | 3274 | `			if( pInstr->iP1 ){` |
|      ! 0 | 3275 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 | 3276 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 | 3277 | `				}else{` |
|        - | 3278 | `					/* Force a numeric cast */` |
|      ! 0 | 3279 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3280 | `					/* Pre-increment */` |
|      ! 0 | 3281 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3282 | `						pTos->rVal++;` |
|        - | 3283 | `						/* Try to get an integer representation */` |
|      ! 0 | 3284 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3285 | `					}else{` |
|        - | 3286 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3287 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3288 | `						sxi64 r;` |
|      ! 0 | 3289 | `						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3290 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3291 | `							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;` |
|      ! 0 | 3292 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3293 | `#else` |
|        - | 3294 | `							pTos->x.iVal = r;` |
|        - | 3295 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3296 | `#endif` |
|      ! 0 | 3297 | `						}else{` |
|      ! 0 | 3298 | `							pTos->x.iVal = r;` |
|      ! 0 | 3299 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3300 | `						}` |
|        - | 3301 | `					}` |
|        - | 3302 | `				}` |
|      ! 0 | 3303 | `			}` |
|        - | 3304 | `		}` |
|   334060 | 3305 | `	}` |
|   667294 | 3306 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|   667294 | 3307 | `	break;` |
|        - | 3308 | `/*` |
|        - | 3309 | ` * DECR: P1 * *` |
|        - | 3310 | ` *` |
|        - | 3311 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - | 3312 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - | 3313 | ` * and decrement after that.` |
|        - | 3314 | ` */` |
|       70 | 3315 | `case PH7_OP_DECR:` |
|        - | 3316 | `#ifdef UNTRUST` |
|        - | 3317 | `	if( pTos < pStack ){` |
|        - | 3318 | `		goto Abort;` |
|        - | 3319 | `	}` |
|        - | 3320 | `#endif` |
|        - | 3321 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3322 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3323 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|        - | 3324 | `	 * property currently holding null). */` |
|      148 | 3325 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3326 | `	/* php refuses to decrement a string OFFSET, whatever byte it holds. */` |
|      136 | 3327 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3328 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3329 | `	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —` |
|        - | 3330 | ``	 * `--` on null is php's no-op and its write-back still dispatches set). */`` |
|      130 | 3331 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|       73 | 3332 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 3333 | `		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 3334 | `		if( VM_HOOK_PEND_IS_RMW(pTopDec->iKind) && pTopDec->nScratchIdx == pTos->nIdx ){` |
|        - | 3335 | `			SyBlob sErrMsg;` |
|        3 | 3336 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3337 | `			VmIncDecTypeErrorMsg(pTos,FALSE,&sErrMsg);` |
|        3 | 3338 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3339 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 3340 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 3341 | `			break;` |
|        - | 3342 | `		}` |
|      ! 0 | 3343 | `	}` |
|      130 | 3344 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3345 | ``		/* php's `--` operand contract, the mirror of INCR's: an array, object or`` |
|        - | 3346 | `		 * resource is a catchable TypeError, not a silent no-op. */` |
|        - | 3347 | `		SyBlob sDecMsg;` |
|        - | 3348 | `		sxi32 rcDec;` |
|       11 | 3349 | `		SyBlobInit(&sDecMsg,&pVm->sAllocator);` |
|       11 | 3350 | `		VmIncDecTypeErrorMsg(pTos,FALSE,&sDecMsg);` |
|       11 | 3351 | `		PH7_MemObjRelease(pTos);` |
|       11 | 3352 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 3353 | `		pTos->nIdx = SXU32_HIGH;` |
|       16 | 3354 | `		rcDec = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sDecMsg),` |
|        5 | 3355 | `			SyBlobLength(&sDecMsg));` |
|       11 | 3356 | `		SyBlobRelease(&sDecMsg);` |
|       11 | 3357 | `		if( rcDec == SXERR_ABORT ){ goto Abort; }` |
|       11 | 3358 | `		rc = rcDec;` |
|       13 | 3359 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3360 | `	}` |
|        - | 3361 | ``	/* NULL and BOOL stay excluded: PHP leaves `--` on either untouched (no-op) --`` |
|        - | 3362 | `	 * but 8.3 warns about both no-ops, same as the non-numeric-string one below. */` |
|      120 | 3363 | `	if( pTos->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|        - | 3364 | `		/* E_WARNING, not E_DEPRECATED -- php reports these at errno 2. */` |
|       16 | 3365 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3366 | `			"Decrement on type %s has no effect, this will change in the next major version of PHP",` |
|       10 | 3367 | `			(pTos->iFlags & MEMOBJ_NULL) ? "null" : "bool");` |
|        5 | 3368 | `	}` |
|      120 | 3369 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      110 | 3370 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3371 | `			ph7_value *pObj;` |
|      110 | 3372 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      110 | 3373 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3374 | ``					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string`` |
|        - | 3375 | `					 * (php has no string decrement); PHL rejects it. */` |
|        - | 3376 | `					SyBlob sErrMsg;` |
|        3 | 3377 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3378 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3379 | `						"Decrement on a non-numeric string is not supported",` |
|        - | 3380 | `						sizeof("Decrement on a non-numeric string is not supported")-1);` |
|        3 | 3381 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3382 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3383 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3384 | `					break;` |
|      ! 0 | 3385 | `				}else{` |
|        - | 3386 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - | 3387 | `					 * post-decrement must preserve pTos's original value, which` |
|        - | 3388 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - | 3389 | `					 * Force pTos to own its blob before coercing pObj. */` |
|      107 | 3390 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 | 3391 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 | 3392 | `					}` |
|      107 | 3393 | `					PH7_MemObjToNumeric(pObj);` |
|      107 | 3394 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|       11 | 3395 | `						pObj->rVal--;` |
|        - | 3396 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3397 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3398 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3399 | `						 * integer-valued real. */` |
|       11 | 3400 | `						PH7_MemObjTryInteger(pObj);` |
|        6 | 3401 | `					}else{` |
|        - | 3402 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3403 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3404 | `						sxi64 r;` |
|       97 | 3405 | `						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3406 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        3 | 3407 | `							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;` |
|        3 | 3408 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3409 | `#else` |
|        - | 3410 | `							pObj->x.iVal = r;` |
|        - | 3411 | `#endif` |
|        2 | 3412 | `						}else{` |
|       95 | 3413 | `							pObj->x.iVal = r;` |
|        - | 3414 | `						}` |
|        - | 3415 | `					}` |
|      107 | 3416 | `					if( pInstr->iP1 ){` |
|        - | 3417 | `						/* Pre-decrement: result is the new value. */` |
|        3 | 3418 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 | 3419 | `					}` |
|        - | 3420 | `					/* Post-decrement: pTos retains the old value. */` |
|        - | 3421 | `				}` |
|       53 | 3422 | `			}` |
|       54 | 3423 | `		}else{` |
|      ! 0 | 3424 | `			if( pInstr->iP1 ){` |
|      ! 0 | 3425 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - | 3426 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 | 3427 | `				}else{` |
|        - | 3428 | `					/* Force a numeric cast */` |
|      ! 0 | 3429 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3430 | `					/* Pre-decrement */` |
|      ! 0 | 3431 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3432 | `						pTos->rVal--;` |
|        - | 3433 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 | 3434 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3435 | `					}else{` |
|        - | 3436 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3437 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3438 | `						sxi64 r;` |
|      ! 0 | 3439 | `						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3440 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3441 | `							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;` |
|      ! 0 | 3442 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3443 | `#else` |
|        - | 3444 | `							pTos->x.iVal = r;` |
|        - | 3445 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3446 | `#endif` |
|      ! 0 | 3447 | `						}else{` |
|      ! 0 | 3448 | `							pTos->x.iVal = r;` |
|      ! 0 | 3449 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3450 | `						}` |
|        - | 3451 | `					}` |
|        - | 3452 | `				}` |
|      ! 0 | 3453 | `			}` |
|        - | 3454 | `		}` |
|       53 | 3455 | `	}` |
|      117 | 3456 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|      117 | 3457 | `	break;` |
|        - | 3458 | `/*` |
|        - | 3459 | ` * UMINUS: * * *` |
|        - | 3460 | ` *` |
|        - | 3461 | ` * Perform a unary minus operation.` |
|        - | 3462 | ` */` |
|    41098 | 3463 | `case PH7_OP_UMINUS:` |
|        - | 3464 | `#ifdef UNTRUST` |
|        - | 3465 | `	if( pTos < pStack ){` |
|        - | 3466 | `		goto Abort;` |
|        - | 3467 | `	}` |
|        - | 3468 | `#endif` |
|        - | 3469 | ``	/* php's `$x * -1` operand contract: an array, object, resource or`` |
|        - | 3470 | `	 * non-numeric string is a TypeError, not a warned int(-1). */` |
|    82203 | 3471 | `	PH7_UNARY_ARITH_CONTRACT()` |
|        - | 3472 | `	/* Force a numeric (integer,real or both) cast */` |
|    82175 | 3473 | `	PH7_MemObjToNumeric(pTos);` |
|    82175 | 3474 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      216 | 3475 | `		pTos->rVal = -pTos->rVal;` |
|      106 | 3476 | `	}` |
|    82175 | 3477 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    82001 | 3478 | `		if( pTos->x.iVal == SMALLEST_INT64 ){` |
|        - | 3479 | `			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a` |
|        - | 3480 | `			 * REAL representation is already present it is the negated` |
|        - | 3481 | `			 * authoritative value, so just drop the now-stale cached int. The` |
|        - | 3482 | `			 * integer-only build has no float type, so it wraps (two's` |
|        - | 3483 | `			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */` |
|        - | 3484 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        7 | 3485 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 | 3486 | `				pTos->rVal = -(ph7_real)pTos->x.iVal;` |
|        7 | 3487 | `				MemObjSetType(pTos,MEMOBJ_REAL);` |
|        4 | 3488 | `			}else{` |
|      ! 0 | 3489 | `				pTos->iFlags &= ~MEMOBJ_INT;` |
|        - | 3490 | `			}` |
|        - | 3491 | `#else` |
|        - | 3492 | `			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);` |
|        - | 3493 | `#endif` |
|        4 | 3494 | `		}else{` |
|    81995 | 3495 | `			pTos->x.iVal = -pTos->x.iVal;` |
|        - | 3496 | `		}` |
|    40998 | 3497 | `	}` |
|    82175 | 3498 | `	break;` |
|        - | 3499 | `/*` |
|        - | 3500 | ` * UPLUS: * * *` |
|        - | 3501 | ` *` |
|        - | 3502 | ` * Perform a unary plus operation.` |
|        - | 3503 | ` */` |
|       22 | 3504 | `case PH7_OP_UPLUS:` |
|        - | 3505 | `#ifdef UNTRUST` |
|        - | 3506 | `	if( pTos < pStack ){` |
|        - | 3507 | `		goto Abort;` |
|        - | 3508 | `	}` |
|        - | 3509 | `#endif` |
|        - | 3510 | ``	/* Unary plus is php's `$x * 1`, so it answers the same TypeError as unary`` |
|        - | 3511 | ``	 * minus -- naming `int` as the other operand, exactly as php does. */`` |
|       45 | 3512 | `	PH7_UNARY_ARITH_CONTRACT()` |
|        - | 3513 | `	/* Force a numeric (integer,real or both) cast */` |
|       39 | 3514 | `	PH7_MemObjToNumeric(pTos);` |
|       39 | 3515 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3516 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 | 3517 | `	}` |
|       39 | 3518 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       39 | 3519 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       19 | 3520 | `	}` |
|       39 | 3521 | `	break;` |
|        - | 3522 | `/*` |
|        - | 3523 | ` * OP_LNOT: * * *` |
|        - | 3524 | ` *` |
|        - | 3525 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - | 3526 | ` * with its complement.` |
|        - | 3527 | ` */` |
|    30273 | 3528 | `case PH7_OP_LNOT:` |
|        - | 3529 | `#ifdef UNTRUST` |
|        - | 3530 | `	if( pTos < pStack ){` |
|        - | 3531 | `		goto Abort;` |
|        - | 3532 | `	}` |
|        - | 3533 | `#endif` |
|        - | 3534 | `	/* Force a boolean cast */` |
|    60551 | 3535 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      157 | 3536 | `		PH7_MemObjToBool(pTos);` |
|       76 | 3537 | `	}` |
|    60551 | 3538 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    60551 | 3539 | `	break;` |
|        - | 3540 | `/*` |
|        - | 3541 | ` * OP_BITNOT: * * *` |
|        - | 3542 | ` *` |
|        - | 3543 | ` * Interpret the top of the stack as an value.Replace it` |
|        - | 3544 | ` * with its ones-complement.` |
|        - | 3545 | ` */` |
|       71 | 3546 | `case PH7_OP_BITNOT:` |
|        - | 3547 | `#ifdef UNTRUST` |
|        - | 3548 | `	if( pTos < pStack ){` |
|        - | 3549 | `		goto Abort;` |
|        - | 3550 | `	}` |
|        - | 3551 | `#endif` |
|      144 | 3552 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - | 3553 | ``		/* php's `~` over a STRING is a per-BYTE ones-complement of the same`` |
|        - | 3554 | ``		 * length — a binary string, not an integer operation, so `~"abc"` is`` |
|        - | 3555 | `		 * "\x9e\x9d\x9c" where PHL cast the string to an int and answered` |
|        - | 3556 | `		 * int(-1). The operand's blob can be a READ-ONLY VIEW of the variable's` |
|        - | 3557 | `		 * own buffer (PH7_MemObjLoad), so complement into a fresh blob and swap` |
|        - | 3558 | `		 * it in rather than writing through the view. */` |
|        - | 3559 | `		SyBlob sNotBuf;` |
|       17 | 3560 | `		const unsigned char *zNotIn = (const unsigned char *)SyBlobData(&pTos->sBlob);` |
|       17 | 3561 | `		sxu32 nNotIn = SyBlobLength(&pTos->sBlob), iNot;` |
|       17 | 3562 | `		SyBlobInit(&sNotBuf,&pVm->sAllocator);` |
|       53 | 3563 | `		for( iNot = 0 ; iNot < nNotIn ; ++iNot ){` |
|       37 | 3564 | `			unsigned char cNot = (unsigned char)~zNotIn[iNot];` |
|       37 | 3565 | `			SyBlobAppend(&sNotBuf,(const void *)&cNot,sizeof(char));` |
|       19 | 3566 | `		}` |
|       17 | 3567 | `		PH7_MemObjRelease(pTos);` |
|       17 | 3568 | `		MemObjSetType(pTos,MEMOBJ_STRING);` |
|       17 | 3569 | `		if( SyBlobLength(&sNotBuf) > 0 ){` |
|       15 | 3570 | `			SyBlobAppend(&pTos->sBlob,SyBlobData(&sNotBuf),SyBlobLength(&sNotBuf));` |
|        7 | 3571 | `		}` |
|       17 | 3572 | `		SyBlobRelease(&sNotBuf);` |
|       17 | 3573 | `		break;` |
|        - | 3574 | `	}` |
|      128 | 3575 | `	if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        - | 3576 | `		/* Everything else — null, a bool, an array, an object, a resource — has` |
|        - | 3577 | `		 * no bitwise-not in php at all: a catchable TypeError naming the operand,` |
|        - | 3578 | `		 * where PHL cast it to an int and answered ~0 / ~1. */` |
|        - | 3579 | `		SyBlob sNotMsg;` |
|        - | 3580 | `		sxi32 rcNot;` |
|       25 | 3581 | `		SyBlobInit(&sNotMsg,&pVm->sAllocator);` |
|       25 | 3582 | `		VmBitNotTypeErrorMsg(pTos,&sNotMsg);` |
|       25 | 3583 | `		PH7_MemObjRelease(pTos);` |
|       25 | 3584 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       25 | 3585 | `		pTos->nIdx = SXU32_HIGH;` |
|       37 | 3586 | `		rcNot = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sNotMsg),` |
|       12 | 3587 | `			SyBlobLength(&sNotMsg));` |
|       25 | 3588 | `		SyBlobRelease(&sNotMsg);` |
|       25 | 3589 | `		if( rcNot == SXERR_ABORT ){ goto Abort; }` |
|       25 | 3590 | `		rc = rcNot;` |
|       27 | 3591 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3592 | `	}` |
|        - | 3593 | `	/* Force an integer cast (php deprecates a lossy float here too) */` |
|      104 | 3594 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      104 | 3595 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      104 | 3596 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 3597 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 3598 | `	}` |
|      104 | 3599 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        - | 3600 | `	/* An INTEGRAL float carries a cached MEMOBJ_INT beside MEMOBJ_REAL, so the` |
|        - | 3601 | `` 	 * cast above is skipped and the value kept rendering as a float: `~2.0` `` |
|        - | 3602 | ``	 * answered 2.0 instead of -3. `~` always yields an int. */`` |
|      104 | 3603 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      104 | 3604 | `	break;` |
|        - | 3605 | `/* OP_MUL * * *` |
|        - | 3606 | ` * OP_MUL_STORE * * *` |
|        - | 3607 | ` *` |
|        - | 3608 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - | 3609 | ` * and push the result back onto the stack.` |
|        - | 3610 | ` */` |
|     1512 | 3611 | `case PH7_OP_MUL:` |
|        - | 3612 | `case PH7_OP_MUL_STORE: {` |
|        - | 3613 | `	VmOpRc rcOp;` |
|        - | 3614 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     3029 | 3615 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     3027 | 3616 | `	sState.pTos = pTos;` |
|     3027 | 3617 | `	sState.pc = pc;` |
|     3027 | 3618 | `	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);` |
|     3027 | 3619 | `	pTos = sState.pTos;` |
|     3027 | 3620 | `	pc = sState.pc;` |
|     3027 | 3621 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3622 | `		goto Abort;` |
|     3027 | 3623 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3624 | `		goto Exception;` |
|        - | 3625 | `	}` |
|     3027 | 3626 | `	break;` |
|        - | 3627 | `					  }` |
|        - | 3628 | `/* OP_POW * * *` |
|        - | 3629 | ` * OP_POW_STORE * * *` |
|        - | 3630 | ` *` |
|        - | 3631 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - | 3632 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - | 3633 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - | 3634 | ` * fits in sxi64; otherwise the result is a double.` |
|        - | 3635 | ` */` |
|       94 | 3636 | `case PH7_OP_POW:` |
|        - | 3637 | `case PH7_OP_POW_STORE: {` |
|        - | 3638 | `	VmOpRc rcOp;` |
|        - | 3639 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      189 | 3640 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      187 | 3641 | `	sState.pTos = pTos;` |
|      187 | 3642 | `	sState.pc = pc;` |
|      187 | 3643 | `	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);` |
|      187 | 3644 | `	pTos = sState.pTos;` |
|      187 | 3645 | `	pc = sState.pc;` |
|      187 | 3646 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3647 | `		goto Abort;` |
|      187 | 3648 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 3649 | `		goto Exception;` |
|        - | 3650 | `	}` |
|      183 | 3651 | `	break;` |
|        - | 3652 | `					  }` |
|        - | 3653 | `/* OP_ADD * * *` |
|        - | 3654 | ` *` |
|        - | 3655 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 3656 | ` * and push the result back onto the stack.` |
|        - | 3657 | ` */` |
|     9905 | 3658 | `case PH7_OP_ADD:{` |
|    19815 | 3659 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3660 | `#ifdef UNTRUST` |
|        - | 3661 | `	if( pNos < pStack ){` |
|        - | 3662 | `		goto Abort;` |
|        - | 3663 | `	}` |
|        - | 3664 | `#endif` |
|        - | 3665 | `	{` |
|        - | 3666 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|        - | 3667 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|        - | 3668 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|        - | 3669 | `		SyBlob sArMsg;` |
|    19815 | 3670 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    19815 | 3671 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 3672 | `			sxi32 rcAr;` |
|       11 | 3673 | `			VmPopOperand(&pTos,1);` |
|       11 | 3674 | `			PH7_MemObjRelease(pTos);` |
|       11 | 3675 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 3676 | `			pTos->nIdx = SXU32_HIGH;` |
|       16 | 3677 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|        5 | 3678 | `				SyBlobLength(&sArMsg));` |
|       11 | 3679 | `			SyBlobRelease(&sArMsg);` |
|       11 | 3680 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|       11 | 3681 | `			rc = rcAr;` |
|       11 | 3682 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3683 | `		}` |
|    19805 | 3684 | `		SyBlobRelease(&sArMsg);` |
|        - | 3685 | `	}` |
|        - | 3686 | `	/* Perform the addition */` |
|    19805 | 3687 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|    19805 | 3688 | `	VmPopOperand(&pTos,1);` |
|    19805 | 3689 | `	break;` |
|        - | 3690 | `				}` |
|        - | 3691 | `/*` |
|        - | 3692 | ` * OP_ADD_STORE * * *` |
|        - | 3693 | ` *` |
|        - | 3694 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 3695 | ` * and push the result back onto the stack.` |
|        - | 3696 | ` */` |
|     2980 | 3697 | `case PH7_OP_ADD_STORE:{` |
|     5965 | 3698 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3699 | `	ph7_value *pObj;` |
|        - | 3700 | `	sxu32 nIdx;` |
|        - | 3701 | `#ifdef UNTRUST` |
|        - | 3702 | `	if( pNos < pStack ){` |
|        - | 3703 | `		goto Abort;` |
|        - | 3704 | `	}` |
|        - | 3705 | `#endif` |
|        - | 3706 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     5967 | 3707 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 3708 | `	{` |
|        - | 3709 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 3710 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 3711 | `		SyBlob sArMsg;` |
|     5959 | 3712 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     5959 | 3713 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"+",&sArMsg) != SXRET_OK ){` |
|        - | 3714 | `			sxi32 rcAr;` |
|      ! 0 | 3715 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 3716 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 3717 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 3718 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 3719 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 3720 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 3721 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 3722 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 3723 | `			rc = rcAr;` |
|      ! 0 | 3724 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3725 | `		}` |
|     5959 | 3726 | `		SyBlobRelease(&sArMsg);` |
|        - | 3727 | `	}` |
|        - | 3728 | `	/* Perform the addition */` |
|     5959 | 3729 | `	nIdx = pTos->nIdx;` |
|     5959 | 3730 | `	if( nIdx == pVm->nGlobalIdx ){` |
|        - | 3731 | `		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment` |
|        - | 3732 | `		 * (a compile-time fatal in php; raised here, same message). */` |
|        3 | 3733 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3734 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 3735 | `		pVm->iExitStatus = 255;` |
|        3 | 3736 | `		pVm->bHaltRequested = 1;` |
|        3 | 3737 | `		goto Abort;` |
|        - | 3738 | `	}` |
|     5957 | 3739 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - | 3740 | `	/* Peform the store operation */` |
|     5957 | 3741 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 3742 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     5957 | 3743 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     5957 | 3744 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     5957 | 3745 | `		PH7_MemObjStore(pTos,pObj);` |
|     2976 | 3746 | `	}` |
|     5957 | 3747 | `	PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|        - | 3748 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     5957 | 3749 | `	PH7_MemObjStore(pTos,pNos);` |
|     5957 | 3750 | `	VmPopOperand(&pTos,1);` |
|     5957 | 3751 | `	break;` |
|        - | 3752 | `				}` |
|        - | 3753 | `/* OP_SUB * * *` |
|        - | 3754 | ` *` |
|        - | 3755 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 3756 | ` * first (what was next on the stack) from the second (the` |
|        - | 3757 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 3758 | ` */` |
|    24225 | 3759 | `case PH7_OP_SUB: {` |
|        - | 3760 | `	VmOpRc rcOp;` |
|    48784 | 3761 | `	sState.pTos = pTos;` |
|    48784 | 3762 | `	sState.pc = pc;` |
|    48784 | 3763 | `	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);` |
|    48784 | 3764 | `	pTos = sState.pTos;` |
|    48784 | 3765 | `	pc = sState.pc;` |
|    48784 | 3766 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3767 | `		goto Abort;` |
|    48784 | 3768 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 3769 | `		goto Exception;` |
|        - | 3770 | `	}` |
|    48784 | 3771 | `	break;` |
|        - | 3772 | `					  }` |
|        - | 3773 | `/* OP_SUB_STORE * * *` |
|        - | 3774 | ` *` |
|        - | 3775 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 3776 | ` * first (what was next on the stack) from the second (the` |
|        - | 3777 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 3778 | ` */` |
|        8 | 3779 | `case PH7_OP_SUB_STORE: {` |
|        - | 3780 | `	VmOpRc rcOp;` |
|        - | 3781 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       18 | 3782 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       16 | 3783 | `	sState.pTos = pTos;` |
|       16 | 3784 | `	sState.pc = pc;` |
|       16 | 3785 | `	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);` |
|       16 | 3786 | `	pTos = sState.pTos;` |
|       16 | 3787 | `	pc = sState.pc;` |
|       16 | 3788 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3789 | `		goto Abort;` |
|       16 | 3790 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        3 | 3791 | `		goto Exception;` |
|        - | 3792 | `	}` |
|       14 | 3793 | `	break;` |
|        - | 3794 | `					  }` |
|        - | 3795 |  |
|        - | 3796 | `/*` |
|        - | 3797 | ` * OP_MOD * * *` |
|        - | 3798 | ` *` |
|        - | 3799 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3800 | ` * first (what was next on the stack) from the second (the` |
|        - | 3801 | ` * top of the stack) and push the remainder after division` |
|        - | 3802 | ` * onto the stack.` |
|        - | 3803 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 3804 | ` */` |
|      483 | 3805 | `case PH7_OP_MOD: {` |
|        - | 3806 | `	VmOpRc rcOp;` |
|      971 | 3807 | `	sState.pTos = pTos;` |
|      971 | 3808 | `	sState.pc = pc;` |
|      971 | 3809 | `	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);` |
|      971 | 3810 | `	pTos = sState.pTos;` |
|      971 | 3811 | `	pc = sState.pc;` |
|      971 | 3812 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3813 | `		goto Abort;` |
|      971 | 3814 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 3815 | `		goto Exception;` |
|        - | 3816 | `	}` |
|      967 | 3817 | `	break;` |
|        - | 3818 | `					  }` |
|        - | 3819 | `/*` |
|        - | 3820 | ` * OP_MOD_STORE * * *` |
|        - | 3821 | ` *` |
|        - | 3822 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3823 | ` * first (what was next on the stack) from the second (the` |
|        - | 3824 | ` * top of the stack) and push the remainder after division` |
|        - | 3825 | ` * onto the stack.` |
|        - | 3826 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 3827 | ` */` |
|        6 | 3828 | `case PH7_OP_MOD_STORE: {` |
|        - | 3829 | `	VmOpRc rcOp;` |
|        - | 3830 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       13 | 3831 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       11 | 3832 | `	sState.pTos = pTos;` |
|       11 | 3833 | `	sState.pc = pc;` |
|       11 | 3834 | `	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);` |
|       11 | 3835 | `	pTos = sState.pTos;` |
|       11 | 3836 | `	pc = sState.pc;` |
|       11 | 3837 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3838 | `		goto Abort;` |
|       11 | 3839 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 3840 | `		goto Exception;` |
|        - | 3841 | `	}` |
|        7 | 3842 | `	break;` |
|        - | 3843 | `					  }` |
|        - | 3844 | `/*` |
|        - | 3845 | ` * OP_DIV * * *` |
|        - | 3846 | ` *` |
|        - | 3847 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3848 | ` * first (what was next on the stack) from the second (the` |
|        - | 3849 | ` * top of the stack) and push the result onto the stack.` |
|        - | 3850 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 3851 | ` */` |
|       70 | 3852 | `case PH7_OP_DIV: {` |
|        - | 3853 | `	VmOpRc rcOp;` |
|      143 | 3854 | `	sState.pTos = pTos;` |
|      143 | 3855 | `	sState.pc = pc;` |
|      143 | 3856 | `	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);` |
|      143 | 3857 | `	pTos = sState.pTos;` |
|      143 | 3858 | `	pc = sState.pc;` |
|      143 | 3859 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3860 | `		goto Abort;` |
|      143 | 3861 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 3862 | `		goto Exception;` |
|        - | 3863 | `	}` |
|      137 | 3864 | `	break;` |
|        - | 3865 | `					  }` |
|        - | 3866 | `/*` |
|        - | 3867 | ` * OP_DIV_STORE * * *` |
|        - | 3868 | ` *` |
|        - | 3869 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3870 | ` * first (what was next on the stack) from the second (the` |
|        - | 3871 | ` * top of the stack) and push the result onto the stack.` |
|        - | 3872 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 3873 | ` */` |
|        8 | 3874 | `case PH7_OP_DIV_STORE:{` |
|       17 | 3875 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3876 | `	ph7_value *pObj;` |
|        - | 3877 | `	ph7_real a,b,r;` |
|        - | 3878 | `#ifdef UNTRUST` |
|        - | 3879 | `	if( pNos < pStack ){` |
|        - | 3880 | `		goto Abort;` |
|        - | 3881 | `	}` |
|        - | 3882 | `#endif` |
|        - | 3883 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       17 | 3884 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 3885 | `	{` |
|        - | 3886 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 3887 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 3888 | `		SyBlob sArMsg;` |
|       15 | 3889 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|       15 | 3890 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"/",&sArMsg) != SXRET_OK ){` |
|        - | 3891 | `			sxi32 rcAr;` |
|      ! 0 | 3892 | `			VmPopOperand(&pTos,1);` |
|      ! 0 | 3893 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 3894 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 | 3895 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 | 3896 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      ! 0 | 3897 | `				SyBlobLength(&sArMsg));` |
|      ! 0 | 3898 | `			SyBlobRelease(&sArMsg);` |
|      ! 0 | 3899 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      ! 0 | 3900 | `			rc = rcAr;` |
|      ! 0 | 3901 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3902 | `		}` |
|       15 | 3903 | `		SyBlobRelease(&sArMsg);` |
|        - | 3904 | `	}` |
|        - | 3905 | `	/* Force the operands to be real */` |
|       15 | 3906 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       15 | 3907 | `		PH7_MemObjToReal(pTos);` |
|        7 | 3908 | `	}` |
|       15 | 3909 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       15 | 3910 | `		PH7_MemObjToReal(pNos);` |
|        7 | 3911 | `	}` |
|        - | 3912 | `	/* Perform the requested operation */` |
|       15 | 3913 | `	a = pTos->rVal;` |
|       15 | 3914 | `	b = pNos->rVal;` |
|       15 | 3915 | `	if( b == 0 ){` |
|        - | 3916 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|        - | 3917 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|        5 | 3918 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|        9 | 3919 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      ! 0 | 3920 | `	}else{` |
|       11 | 3921 | `		r = a/b;` |
|        - | 3922 | `		/* Push the result */` |
|       11 | 3923 | `		pNos->rVal = r;` |
|       11 | 3924 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - | 3925 | `		/* Try to get an integer representation */` |
|       11 | 3926 | `		PH7_MemObjTryInteger(pNos);` |
|        - | 3927 | `	}` |
|       11 | 3928 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 3929 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       11 | 3930 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       11 | 3931 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       11 | 3932 | `		PH7_MemObjStore(pNos,pObj);` |
|        5 | 3933 | `	}` |
|       11 | 3934 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       11 | 3935 | `	VmPopOperand(&pTos,1);` |
|       11 | 3936 | `	break;` |
|        - | 3937 | `				}` |
|        - | 3938 | `/* OP_BAND * * *` |
|        - | 3939 | ` *` |
|        - | 3940 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3941 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 3942 | ` * two elements.` |
|        - | 3943 | `*/` |
|        - | 3944 | `/* OP_BOR * * *` |
|        - | 3945 | ` *` |
|        - | 3946 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3947 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 3948 | ` * two elements.` |
|        - | 3949 | ` */` |
|        - | 3950 | `/* OP_BXOR * * *` |
|        - | 3951 | ` *` |
|        - | 3952 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 3953 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 3954 | ` * two elements.` |
|        - | 3955 | ` */` |
|      428 | 3956 | `case PH7_OP_BAND:` |
|        - | 3957 | `case PH7_OP_BOR:` |
|        - | 3958 | `case PH7_OP_BXOR:{` |
|      859 | 3959 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3960 | `	sxi64 a,b,r;` |
|        - | 3961 | `	int cBwOp;` |
|        - | 3962 | `#ifdef UNTRUST` |
|        - | 3963 | `	if( pNos < pStack ){` |
|        - | 3964 | `		goto Abort;` |
|        - | 3965 | `	}` |
|        - | 3966 | `#endif` |
|      859 | 3967 | `	cBwOp = pInstr->iOp == PH7_OP_BOR ? '\|' : (pInstr->iOp == PH7_OP_BXOR ? '^' : '&');` |
|      859 | 3968 | `	if( (pNos->iFlags & MEMOBJ_STRING) && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 3969 | `		/* TWO strings: php's per-byte string operation, result a string. */` |
|       35 | 3970 | `		PH7_STRING_BITWISE_RESULT(pNos,pNos,pTos,cBwOp)` |
|       35 | 3971 | `		VmPopOperand(&pTos,1);` |
|       35 | 3972 | `		break;` |
|        - | 3973 | `	}` |
|        - | 3974 | `	{` |
|        - | 3975 | `		char zBwOp[2];` |
|      825 | 3976 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 3977 | `		/* Anything else is php's arithmetic operand contract, named for this` |
|        - | 3978 | ``		 * operator (`array & int`), not a silent integer cast. */`` |
|      827 | 3979 | `		PH7_BITWISE_ARITH_CONTRACT(pNos,pTos,zBwOp,1)` |
|        - | 3980 | `	}` |
|        - | 3981 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|      793 | 3982 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|      793 | 3983 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      793 | 3984 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      793 | 3985 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      793 | 3986 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|        9 | 3987 | `		PH7_MemObjToInteger(pTos);` |
|        4 | 3988 | `	}` |
|      793 | 3989 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|       11 | 3990 | `		PH7_MemObjToInteger(pNos);` |
|        5 | 3991 | `	}` |
|        - | 3992 | `	/* Perform the requested operation */` |
|      793 | 3993 | `	a = pNos->x.iVal;` |
|      793 | 3994 | `	b = pTos->x.iVal;` |
|      793 | 3995 | `	switch(pInstr->iOp){` |
|       99 | 3996 | `	case PH7_OP_BOR_STORE:` |
|      203 | 3997 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        8 | 3998 | `	case PH7_OP_BXOR_STORE:` |
|       17 | 3999 | `	case PH7_OP_BXOR: r = a^b; break;` |
|      287 | 4000 | `	case PH7_OP_BAND_STORE:` |
|      287 | 4001 | `	case PH7_OP_BAND:` |
|      579 | 4002 | `	default:          r = a&b; break;` |
|        - | 4003 | `	}` |
|        - | 4004 | `	/* Push the result */` |
|      793 | 4005 | `	pNos->x.iVal = r;` |
|      793 | 4006 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      793 | 4007 | `	VmPopOperand(&pTos,1);` |
|      793 | 4008 | `	break;` |
|        - | 4009 | `				 }` |
|        - | 4010 | `/* OP_BAND_STORE * * *` |
|        - | 4011 | ` *` |
|        - | 4012 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4013 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 4014 | ` * two elements.` |
|        - | 4015 | `*/` |
|        - | 4016 | `/* OP_BOR_STORE * * *` |
|        - | 4017 | ` *` |
|        - | 4018 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4019 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 4020 | ` * two elements.` |
|        - | 4021 | ` */` |
|        - | 4022 | `/* OP_BXOR_STORE * * *` |
|        - | 4023 | ` *` |
|        - | 4024 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4025 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 4026 | ` * two elements.` |
|        - | 4027 | ` */` |
|       50 | 4028 | `case PH7_OP_BAND_STORE:` |
|        - | 4029 | `case PH7_OP_BOR_STORE:` |
|        - | 4030 | `case PH7_OP_BXOR_STORE:{` |
|      101 | 4031 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4032 | `	ph7_value *pObj;` |
|        - | 4033 | `	sxi64 a,b,r;` |
|        - | 4034 | `	int cBwOp,bBwStr;` |
|        - | 4035 | `#ifdef UNTRUST` |
|        - | 4036 | `	if( pNos < pStack ){` |
|        - | 4037 | `		goto Abort;` |
|        - | 4038 | `	}` |
|        - | 4039 | `#endif` |
|        - | 4040 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      101 | 4041 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       95 | 4042 | `	cBwOp = pInstr->iOp == PH7_OP_BOR_STORE ? '\|' : (pInstr->iOp == PH7_OP_BXOR_STORE ? '^' : '&');` |
|       95 | 4043 | `	bBwStr = (pNos->iFlags & MEMOBJ_STRING) != 0 && (pTos->iFlags & MEMOBJ_STRING) != 0;` |
|       95 | 4044 | `	if( !bBwStr ){` |
|        - | 4045 | `		char zBwOp[2];` |
|       89 | 4046 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 4047 | ``		/* `$x &= v` answers the same contract as `$x & v` (php's compound`` |
|        - | 4048 | `		 * assignment is the operator plus a store), but through its own error` |
|        - | 4049 | `		 * path, which is POSITIONAL: the lvalue is named first even when the` |
|        - | 4050 | ``		 * right operand is the offender (`$x = 1; $x &= $o` is "int & BsocP",`` |
|        - | 4051 | ``		 * where the plain `1 & $o` is "BsocP & int"). */`` |
|       89 | 4052 | `		PH7_BITWISE_ARITH_CONTRACT(pTos,pNos,zBwOp,0)` |
|        - | 4053 | `		/* Force the operands to be integer (php deprecates a lossy float here) */` |
|       85 | 4054 | `		rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|       85 | 4055 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|       85 | 4056 | `		rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|       85 | 4057 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|       85 | 4058 | `		if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 4059 | `			PH7_MemObjToInteger(pTos);` |
|      ! 0 | 4060 | `		}` |
|       85 | 4061 | `		if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        3 | 4062 | `			PH7_MemObjToInteger(pNos);` |
|        1 | 4063 | `		}` |
|       42 | 4064 | `	}` |
|       91 | 4065 | `	if( bBwStr ){` |
|        - | 4066 | `		/* TWO strings: php's per-byte string operation, result a string. The` |
|        - | 4067 | `		 * result lands in pNos, which the store tail below writes into the` |
|        - | 4068 | `		 * lvalue's slot exactly like the integer result. */` |
|        7 | 4069 | `		PH7_STRING_BITWISE_RESULT(pNos,pTos,pNos,cBwOp)` |
|        4 | 4070 | `	}else{` |
|        - | 4071 | `	/* Perform the requested operation */` |
|       85 | 4072 | `	a = pTos->x.iVal;` |
|       85 | 4073 | `	b = pNos->x.iVal;` |
|       85 | 4074 | `	switch(pInstr->iOp){` |
|       32 | 4075 | `	case PH7_OP_BOR_STORE:` |
|       65 | 4076 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        5 | 4077 | `	case PH7_OP_BXOR_STORE:` |
|       11 | 4078 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        5 | 4079 | `	case PH7_OP_BAND_STORE:` |
|        5 | 4080 | `	case PH7_OP_BAND:` |
|       11 | 4081 | `	default:          r = a&b; break;` |
|        - | 4082 | `	}` |
|        - | 4083 | `	/* Push the result */` |
|       85 | 4084 | `	pNos->x.iVal = r;` |
|       85 | 4085 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        - | 4086 | `	}` |
|       91 | 4087 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 4088 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       91 | 4089 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       91 | 4090 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       91 | 4091 | `		PH7_MemObjStore(pNos,pObj);` |
|       45 | 4092 | `	}` |
|       91 | 4093 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       91 | 4094 | `	VmPopOperand(&pTos,1);` |
|       91 | 4095 | `	break;` |
|        - | 4096 | `				 }` |
|        - | 4097 | `/* OP_SHL * * *` |
|        - | 4098 | ` *` |
|        - | 4099 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4100 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4101 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4102 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4103 | ` */` |
|        - | 4104 | `/* OP_SHR * * *` |
|        - | 4105 | ` *` |
|        - | 4106 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4107 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4108 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4109 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4110 | ` */` |
|       53 | 4111 | `case PH7_OP_SHL:` |
|        - | 4112 | `case PH7_OP_SHR: {` |
|        - | 4113 | `	VmOpRc rcOp;` |
|      108 | 4114 | `	sState.pTos = pTos;` |
|      108 | 4115 | `	sState.pc = pc;` |
|      108 | 4116 | `	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);` |
|      108 | 4117 | `	pTos = sState.pTos;` |
|      108 | 4118 | `	pc = sState.pc;` |
|      108 | 4119 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4120 | `		goto Abort;` |
|      108 | 4121 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       29 | 4122 | `		goto Exception;` |
|        - | 4123 | `	}` |
|       80 | 4124 | `	break;` |
|        - | 4125 | `					  }` |
|        - | 4126 | `/*  OP_SHL_STORE * * *` |
|        - | 4127 | ` *` |
|        - | 4128 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4129 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4130 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4131 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4132 | ` */` |
|        - | 4133 | `/* OP_SHR_STORE * * *` |
|        - | 4134 | ` *` |
|        - | 4135 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4136 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4137 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4138 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4139 | ` */` |
|       20 | 4140 | `case PH7_OP_SHL_STORE:` |
|        - | 4141 | `case PH7_OP_SHR_STORE: {` |
|        - | 4142 | `	VmOpRc rcOp;` |
|        - | 4143 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|       41 | 4144 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|       37 | 4145 | `	sState.pTos = pTos;` |
|       37 | 4146 | `	sState.pc = pc;` |
|       37 | 4147 | `	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);` |
|       37 | 4148 | `	pTos = sState.pTos;` |
|       37 | 4149 | `	pc = sState.pc;` |
|       37 | 4150 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4151 | `		goto Abort;` |
|       37 | 4152 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 4153 | `		goto Exception;` |
|        - | 4154 | `	}` |
|       29 | 4155 | `	break;` |
|        - | 4156 | `					  }` |
|        - | 4157 | `/* CAT:  P1 * *` |
|        - | 4158 | ` *` |
|        - | 4159 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - | 4160 | ` * back.` |
|        - | 4161 | ` */` |
|    99557 | 4162 | `case PH7_OP_CAT:{` |
|        - | 4163 | `	ph7_value *pNos,*pCur;` |
|   199119 | 4164 | `	if( pInstr->iP1 < 1 ){` |
|   165613 | 4165 | `		pNos = &pTos[-1];` |
|    82809 | 4166 | `	}else{` |
|    33511 | 4167 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - | 4168 | `	}` |
|        - | 4169 | `#ifdef UNTRUST` |
|        - | 4170 | `	if( pNos < pStack ){` |
|        - | 4171 | `		goto Abort;` |
|        - | 4172 | `	}` |
|        - | 4173 | `#endif` |
|        - | 4174 | `	/* Force a string cast (user-visible: warns on an array operand, §2).` |
|        - | 4175 | `	 * php coerces the operands left to right, so the leftmost not-stringable` |
|        - | 4176 | `	 * object is the one that throws. */` |
|        - | 4177 | `	{` |
|   199119 | 4178 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|   199119 | 4179 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4180 | `	}` |
|   199117 | 4181 | `	pCur = &pNos[1];` |
|        - | 4182 | `	{` |
|        - | 4183 | `		/* rcSv leaves the loop with the coercion status: the routing macro expands` |
|        - | 4184 | ``		 * to a bare `break` here, which inside the while would fall THROUGH to the`` |
|        - | 4185 | ``		 * `pTos = pNos` below with a throw pending. Route it at case level. */`` |
|   199117 | 4186 | `		sxi32 rcSv = SXRET_OK;` |
|   407747 | 4187 | `		while( pCur <= pTos ){` |
|   209079 | 4188 | `			rcSv = PH7_MemObjToStringUV(pCur);` |
|   209079 | 4189 | `			if( rcSv != SXRET_OK ){` |
|      448 | 4190 | `				break;` |
|        - | 4191 | `			}` |
|        - | 4192 | `			/* Perform the concatenation */` |
|   208635 | 4193 | `			if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   208405 | 4194 | `				if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - | 4195 | `					/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 | 4196 | `					PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4197 | `					goto Abort;` |
|        - | 4198 | `				}` |
|   104200 | 4199 | `			}` |
|   208635 | 4200 | `			SyBlobRelease(&pCur->sBlob);` |
|   208635 | 4201 | `			pCur++;` |
|        5 | 4202 | `		}` |
|   199939 | 4203 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4204 | `	}` |
|   198673 | 4205 | `	pTos = pNos;` |
|   198673 | 4206 | `	break;` |
|        - | 4207 | `				}` |
|        - | 4208 | `/*  CAT_STORE: * * *` |
|        - | 4209 | ` *` |
|        - | 4210 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - | 4211 | ` * back.` |
|        - | 4212 | ` */` |
|    16729 | 4213 | `case PH7_OP_CAT_STORE:{` |
|    33463 | 4214 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4215 | `	ph7_value *pObj;` |
|        - | 4216 | `	sxu32 nIdx;` |
|        - | 4217 | `#ifdef UNTRUST` |
|        - | 4218 | `	if( pNos < pStack ){` |
|        - | 4219 | `		goto Abort;` |
|        - | 4220 | `	}` |
|        - | 4221 | `#endif` |
|        - | 4222 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|    50187 | 4223 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 4224 | `	/* The right operand must be a string to append it (user-visible, §2) */` |
|        - | 4225 | `	{` |
|    33461 | 4226 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|    33465 | 4227 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4228 | `	}` |
|    33451 | 4229 | `	nIdx = pTos->nIdx;` |
|        - | 4230 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - | 4231 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - | 4232 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - | 4233 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - | 4234 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - | 4235 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - | 4236 | `	 * the source we copy from — references share the slot index, so one check` |
|        - | 4237 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - | 4238 | `	 * must run before any mutation (left to the slow path).` |
|        - | 4239 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - | 4240 | `	 * and remains O(n^2) by design. */` |
|    33446 | 4241 | `	if( nIdx != SXU32_HIGH` |
|    33446 | 4242 | `	 && nIdx != pNos->nIdx` |
|    33442 | 4243 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|    33443 | 4244 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|    16827 | 4245 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|        - | 4246 | `		/* e.g. $x = 5; $x .= "a";  ->  "5a" (user-visible: warns if $x is an array,` |
|        - | 4247 | `		 * throws if it is a not-stringable object — and then the lvalue keeps` |
|        - | 4248 | `		 * holding that object, since the throw abandons the coercion) */` |
|        - | 4249 | `		{` |
|    33437 | 4250 | `			sxi32 rcSv = PH7_MemObjToStringUV(pObj);` |
|    33445 | 4251 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4252 | `		}` |
|    33433 | 4253 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|    33431 | 4254 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4255 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - | 4256 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 | 4257 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4258 | `				goto Abort;` |
|        - | 4259 | `			}` |
|    16713 | 4260 | `		}` |
|        - | 4261 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - | 4262 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - | 4263 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - | 4264 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - | 4265 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - | 4266 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - | 4267 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - | 4268 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - | 4269 | `		 * the same slot is appended to again later in the statement` |
|        - | 4270 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - | 4271 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - | 4272 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|    33433 | 4273 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 | 4274 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 | 4275 | `		}` |
|        - | 4276 | `		/* A hooked lvalue's scratch slot was appended in place — dispatch the` |
|        - | 4277 | `		 * set side now (the consume reads the computed value from the slot). */` |
|    33433 | 4278 | `		PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|    33433 | 4279 | `		pNos->nIdx = SXU32_HIGH;` |
|    33433 | 4280 | `		VmPopOperand(&pTos,1);` |
|    33433 | 4281 | `		break;` |
|        - | 4282 | `	}` |
|        - | 4283 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|        - | 4284 | `	/* Force a string cast (user-visible: warns if the lvalue is an array, §2) */` |
|        - | 4285 | `	{` |
|       16 | 4286 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       16 | 4287 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4288 | `	}` |
|        - | 4289 | `	/* Perform the concatenation (Reverse order) */` |
|       16 | 4290 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 | 4291 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4292 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - | 4293 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 | 4294 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4295 | `			goto Abort;` |
|        - | 4296 | `		}` |
|        7 | 4297 | `	}` |
|        - | 4298 | `	/* Perform the store operation */` |
|       16 | 4299 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 | 4300 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 | 4301 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       24 | 4302 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 | 4303 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 | 4304 | `	}` |
|       11 | 4305 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       11 | 4306 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 | 4307 | `	VmPopOperand(&pTos,1);` |
|       11 | 4308 | `	break;` |
|        - | 4309 | `				}` |
|        - | 4310 | `/* OP_AND: * * *` |
|        - | 4311 | ` *` |
|        - | 4312 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - | 4313 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4314 | ` * stack.` |
|        - | 4315 | ` */` |
|        - | 4316 | `/* OP_OR: * * *` |
|        - | 4317 | ` *` |
|        - | 4318 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - | 4319 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4320 | ` * stack.` |
|        - | 4321 | ` */` |
|   159764 | 4322 | `case PH7_OP_LAND:` |
|        - | 4323 | `case PH7_OP_LOR: {` |
|        - | 4324 | `	VmOpRc rcOp;` |
|   319967 | 4325 | `	sState.pTos = pTos;` |
|   319967 | 4326 | `	sState.pc = pc;` |
|   319967 | 4327 | `	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);` |
|   319967 | 4328 | `	pTos = sState.pTos;` |
|   319967 | 4329 | `	pc = sState.pc;` |
|   319967 | 4330 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4331 | `		goto Abort;` |
|   319967 | 4332 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4333 | `		goto Exception;` |
|        - | 4334 | `	}` |
|   319967 | 4335 | `	break;` |
|        - | 4336 | `					  }` |
|        - | 4337 | `/*` |
|        - | 4338 | ` * OP_NULLC: * * *` |
|        - | 4339 | ` * Null coalescing operator '??'.` |
|        - | 4340 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - | 4341 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - | 4342 | ` */` |
|        - | 4343 | `/*` |
|        - | 4344 | ` * OP_NULLC: * P2 *` |
|        - | 4345 | ` * Short-circuit null coalescing '??'.` |
|        - | 4346 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - | 4347 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - | 4348 | ` */` |
|      272 | 4349 | `case PH7_OP_NULLC: {` |
|        - | 4350 | `#ifdef UNTRUST` |
|        - | 4351 | `	if( pTos < pStack ){` |
|        - | 4352 | `		goto Abort;` |
|        - | 4353 | `	}` |
|        - | 4354 | `#endif` |
|      549 | 4355 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 4356 | `		/* Left is not null — keep it and skip the RHS */` |
|      383 | 4357 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|      194 | 4358 | `	}else{` |
|        - | 4359 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|      171 | 4360 | `		VmPopOperand(&pTos, 1);` |
|        - | 4361 | `	}` |
|      549 | 4362 | `	break;` |
|        - | 4363 | `}` |
|        - | 4364 | `/*` |
|        - | 4365 | ` * OP_NULLC_JMP: * P2 *` |
|        - | 4366 | ` * Null coalescing assignment short-circuit.` |
|        - | 4367 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - | 4368 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - | 4369 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - | 4370 | ` */` |
|       72 | 4371 | `case PH7_OP_NULLC_JMP: {` |
|        - | 4372 | `#ifdef UNTRUST` |
|        - | 4373 | `	if( pTos < pStack ){` |
|        - | 4374 | `		goto Abort;` |
|        - | 4375 | `	}` |
|        - | 4376 | `#endif` |
|      147 | 4377 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       38 | 4378 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —` |
|        - | 4379 | `		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is` |
|        - | 4380 | `		 * dropped by the fetch-point sweep (the landing pc is past its` |
|        - | 4381 | `		 * OP_NULLC_STORE window): the skipped assign never dispatches. */` |
|       18 | 4382 | `	}` |
|      147 | 4383 | `	break;` |
|        - | 4384 | `}` |
|        - | 4385 | `/*` |
|        - | 4386 | ` * OP_NULLC_STORE: * * *` |
|        - | 4387 | ` * Null coalescing assignment store.` |
|        - | 4388 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - | 4389 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - | 4390 | ` * expression result.` |
|        - | 4391 | ` */` |
|        - | 4392 | `/*` |
|        - | 4393 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - | 4394 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - | 4395 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - | 4396 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - | 4397 | ` * non-null, fall through without modifying the stack so the following` |
|        - | 4398 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - | 4399 | ` */` |
|       68 | 4400 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - | 4401 | `#ifdef UNTRUST` |
|        - | 4402 | `	if( pTos < pStack ){` |
|        - | 4403 | `		goto Abort;` |
|        - | 4404 | `	}` |
|        - | 4405 | `#endif` |
|      141 | 4406 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - | 4407 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - | 4408 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       58 | 4409 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       28 | 4410 | `	}` |
|      141 | 4411 | `	break;` |
|        - | 4412 | `}` |
|       51 | 4413 | `case PH7_OP_NULLC_STORE: {` |
|        - | 4414 | `	VmOpRc rcOp;` |
|      105 | 4415 | `	sState.pTos = pTos;` |
|      105 | 4416 | `	sState.pc = pc;` |
|      105 | 4417 | `	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);` |
|      105 | 4418 | `	pTos = sState.pTos;` |
|      105 | 4419 | `	pc = sState.pc;` |
|      105 | 4420 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4421 | `		goto Abort;` |
|      105 | 4422 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 4423 | `		goto Exception;` |
|        - | 4424 | `	}` |
|       99 | 4425 | `	break;` |
|        - | 4426 | `					  }` |
|        - | 4427 | `/*` |
|        - | 4428 | ` * OP_SPREAD: * * *` |
|        - | 4429 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - | 4430 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - | 4431 | ` * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this` |
|        - | 4432 | ` * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —` |
|        - | 4433 | ` * the CALL may not be the next instruction, and may be an inner call whose own` |
|        - | 4434 | ` * spreads must stay scoped to it.` |
|        - | 4435 | ` * The expansion tail is shared between the plain-array and the materialized` |
|        - | 4436 | ` * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.` |
|        - | 4437 | ` */` |
|      192 | 4438 | `case PH7_OP_SPREAD: {` |
|        - | 4439 | `#ifdef UNTRUST` |
|        - | 4440 | `	if( pTos < pStack ){` |
|        - | 4441 | `		goto Abort;` |
|        - | 4442 | `	}` |
|        - | 4443 | `#endif` |
|        - | 4444 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - | 4445 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - | 4446 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - | 4447 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - | 4448 | `	 * be freed immediately. */` |
|      387 | 4449 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 | 4450 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - | 4451 | `		sxi32 rcW;` |
|        3 | 4452 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 | 4453 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 | 4454 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 | 4455 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 4456 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 4457 | `			goto Exception;` |
|        - | 4458 | `		}` |
|        - | 4459 | `		/* Grow the operand stack if this expansion would overflow it (no longer a` |
|        - | 4460 | `		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a` |
|        - | 4461 | `		 * single argument — the historical bounded-fallback, now only under OOM. */` |
|        4 | 4462 | `		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,` |
|        1 | 4463 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 4464 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 4465 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 4466 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 4467 | `				pTmpMap->nEntry);` |
|      ! 0 | 4468 | `			break;` |
|        - | 4469 | `		}` |
|        3 | 4470 | `		VmSpreadExpandMap(pVm, &pTos, pTmpMap, 0/*a Traversable's values are not the caller's slots*/);` |
|        3 | 4471 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 | 4472 | `		break;` |
|        - | 4473 | `	}` |
|      385 | 4474 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      385 | 4475 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      576 | 4476 | `		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,` |
|      191 | 4477 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 4478 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 4479 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 4480 | `				pMap->nEntry);` |
|      ! 0 | 4481 | `			break;` |
|        - | 4482 | `		}` |
|      385 | 4483 | `		VmSpreadExpandMap(pVm, &pTos, pMap, pInstr->iP1 != 0);` |
|      191 | 4484 | `	}` |
|        - | 4485 | `	/* else: not an array — leave as-is (single arg) */` |
|      385 | 4486 | `	break;` |
|        - | 4487 | `}` |
|        - | 4488 | `/*` |
|        - | 4489 | ` * OP_FLAG_SPREAD: * * *` |
|        - | 4490 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - | 4491 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - | 4492 | ` */` |
|      342 | 4493 | `case PH7_OP_FLAG_SPREAD: {` |
|        - | 4494 | `#ifdef UNTRUST` |
|        - | 4495 | `	if( pTos < pStack ){` |
|        - | 4496 | `		goto Abort;` |
|        - | 4497 | `	}` |
|        - | 4498 | `#endif` |
|      687 | 4499 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|      687 | 4500 | `	break;` |
|        - | 4501 | `}` |
|        - | 4502 | `/* OP_LXOR: * * *` |
|        - | 4503 | ` *` |
|        - | 4504 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - | 4505 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4506 | ` * stack.` |
|        - | 4507 | ` * According to the PHP language reference manual:` |
|        - | 4508 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - | 4509 | ` *  TRUE,but not both.` |
|        - | 4510 | ` */` |
|        6 | 4511 | `case PH7_OP_LXOR:{` |
|       13 | 4512 | `	ph7_value *pNos = &pTos[-1];` |
|       13 | 4513 | `	sxi32 v = 0;` |
|        - | 4514 | `#ifdef UNTRUST` |
|        - | 4515 | `	if( pNos < pStack ){` |
|        - | 4516 | `		goto Abort;` |
|        - | 4517 | `	}` |
|        - | 4518 | `#endif` |
|        - | 4519 | `	/* Force a boolean cast */` |
|       13 | 4520 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 4521 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 4522 | `	}` |
|       13 | 4523 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 4524 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 | 4525 | `	}` |
|       13 | 4526 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 | 4527 | `		v = 1;` |
|        3 | 4528 | `	}` |
|       13 | 4529 | `	VmPopOperand(&pTos,1);` |
|       13 | 4530 | `	pTos->x.iVal = v;` |
|       13 | 4531 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       13 | 4532 | `	break;` |
|        - | 4533 | `				 }` |
|        - | 4534 | `/* OP_EQ P1 P2 P3` |
|        - | 4535 | ` *` |
|        - | 4536 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - | 4537 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - | 4538 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4539 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4540 | ` */` |
|        - | 4541 | `/* OP_NEQ P1 P2 P3` |
|        - | 4542 | ` *` |
|        - | 4543 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - | 4544 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 4545 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4546 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4547 | ` */` |
|     6245 | 4548 | `case PH7_OP_EQ:` |
|        - | 4549 | `case PH7_OP_NEQ: {` |
|        - | 4550 | `	VmOpRc rcOp;` |
|    12495 | 4551 | `	sState.pTos = pTos;` |
|    12495 | 4552 | `	sState.pc = pc;` |
|    12495 | 4553 | `	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);` |
|    12495 | 4554 | `	pTos = sState.pTos;` |
|    12495 | 4555 | `	pc = sState.pc;` |
|    12495 | 4556 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4557 | `		goto Abort;` |
|    12495 | 4558 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4559 | `		goto Exception;` |
|        - | 4560 | `	}` |
|    12495 | 4561 | `	break;` |
|        - | 4562 | `					  }` |
|        - | 4563 | `/* OP_TEQ P1 P2 *` |
|        - | 4564 | ` *` |
|        - | 4565 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - | 4566 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 4567 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4568 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4569 | ` */` |
|   312843 | 4570 | `case PH7_OP_TEQ: {` |
|        - | 4571 | `	VmOpRc rcOp;` |
|   626915 | 4572 | `	sState.pTos = pTos;` |
|   626915 | 4573 | `	sState.pc = pc;` |
|   626915 | 4574 | `	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);` |
|   626915 | 4575 | `	pTos = sState.pTos;` |
|   626915 | 4576 | `	pc = sState.pc;` |
|   626915 | 4577 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4578 | `		goto Abort;` |
|   626915 | 4579 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4580 | `		goto Exception;` |
|        - | 4581 | `	}` |
|   626915 | 4582 | `	break;` |
|        - | 4583 | `					  }` |
|        - | 4584 | `/* OP_TNE P1 P2 *` |
|        - | 4585 | ` *` |
|        - | 4586 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - | 4587 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - | 4588 | ` * instruction.` |
|        - | 4589 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4590 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4591 | ` *` |
|        - | 4592 | ` */` |
|   277802 | 4593 | `case PH7_OP_TNE: {` |
|        - | 4594 | `	VmOpRc rcOp;` |
|   556042 | 4595 | `	sState.pTos = pTos;` |
|   556042 | 4596 | `	sState.pc = pc;` |
|   556042 | 4597 | `	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);` |
|   556042 | 4598 | `	pTos = sState.pTos;` |
|   556042 | 4599 | `	pc = sState.pc;` |
|   556042 | 4600 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4601 | `		goto Abort;` |
|   556042 | 4602 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4603 | `		goto Exception;` |
|        - | 4604 | `	}` |
|   556042 | 4605 | `	break;` |
|        - | 4606 | `					  }` |
|        - | 4607 | `/* OP_LT P1 P2 P3` |
|        - | 4608 | ` *` |
|        - | 4609 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4610 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 4611 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4612 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4613 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4614 | ` *` |
|        - | 4615 | ` */` |
|        - | 4616 | `/* OP_LE P1 P2 P3` |
|        - | 4617 | ` *` |
|        - | 4618 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4619 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 4620 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4621 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4622 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4623 | ` *` |
|        - | 4624 | ` */` |
|   274614 | 4625 | `case PH7_OP_LT:` |
|        - | 4626 | `case PH7_OP_LE: {` |
|        - | 4627 | `	VmOpRc rcOp;` |
|   550486 | 4628 | `	sState.pTos = pTos;` |
|   550486 | 4629 | `	sState.pc = pc;` |
|   550486 | 4630 | `	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);` |
|   550486 | 4631 | `	pTos = sState.pTos;` |
|   550486 | 4632 | `	pc = sState.pc;` |
|   550486 | 4633 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4634 | `		goto Abort;` |
|   550486 | 4635 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4636 | `		goto Exception;` |
|        - | 4637 | `	}` |
|   550486 | 4638 | `	break;` |
|        - | 4639 | `					  }` |
|        - | 4640 | `/* OP_GT P1 P2 P3` |
|        - | 4641 | ` *` |
|        - | 4642 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4643 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 4644 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4645 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4646 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4647 | ` *` |
|        - | 4648 | ` */` |
|        - | 4649 | `/* OP_GE P1 P2 P3` |
|        - | 4650 | ` *` |
|        - | 4651 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4652 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 4653 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4654 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4655 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4656 | ` *` |
|        - | 4657 | ` */` |
|   127819 | 4658 | `case PH7_OP_GT:` |
|        - | 4659 | `case PH7_OP_GE: {` |
|        - | 4660 | `	VmOpRc rcOp;` |
|   256075 | 4661 | `	sState.pTos = pTos;` |
|   256075 | 4662 | `	sState.pc = pc;` |
|   256075 | 4663 | `	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);` |
|   256075 | 4664 | `	pTos = sState.pTos;` |
|   256075 | 4665 | `	pc = sState.pc;` |
|   256075 | 4666 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4667 | `		goto Abort;` |
|   256075 | 4668 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4669 | `		goto Exception;` |
|        - | 4670 | `	}` |
|   256075 | 4671 | `	break;` |
|        - | 4672 | `					  }` |
|        - | 4673 | `/* OP_SPACESHIP * * *` |
|        - | 4674 | ` *` |
|        - | 4675 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - | 4676 | ` *   -1 if left < right` |
|        - | 4677 | ` *    0 if left == right` |
|        - | 4678 | ` *    1 if left > right` |
|        - | 4679 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - | 4680 | ` */` |
|      160 | 4681 | `case PH7_OP_SPACESHIP: {` |
|        - | 4682 | `	VmOpRc rcOp;` |
|      323 | 4683 | `	sState.pTos = pTos;` |
|      323 | 4684 | `	sState.pc = pc;` |
|      323 | 4685 | `	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);` |
|      323 | 4686 | `	pTos = sState.pTos;` |
|      323 | 4687 | `	pc = sState.pc;` |
|      323 | 4688 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4689 | `		goto Abort;` |
|      323 | 4690 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4691 | `		goto Exception;` |
|        - | 4692 | `	}` |
|      323 | 4693 | `	break;` |
|        - | 4694 | `					  }` |
|        - | 4695 | `/*` |
|        - | 4696 | ` * OP_LOAD_REF * * *` |
|        - | 4697 | ` * Push the index of a referenced object on the stack.` |
|        - | 4698 | ` */` |
|       82 | 4699 | `case PH7_OP_LOAD_REF: {` |
|        - | 4700 | `	sxu32 nIdx;` |
|        - | 4701 | `#ifdef UNTRUST` |
|        - | 4702 | `	if( pTos < pStack ){` |
|        - | 4703 | `		goto Abort;` |
|        - | 4704 | `	}` |
|        - | 4705 | `#endif` |
|      166 | 4706 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|        - | 4707 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|        - | 4708 | `		 * out of a string still carries the BASE VARIABLE's slot index, so taking a` |
|        - | 4709 | `` 		 * reference to it aliased the WHOLE STRING — `$x = [&$s[1]]; $x[0] = "Z";` `` |
|        - | 4710 | ``		 * left $s === "Z". Same Error the `=&` and by-ref-argument paths raise. */`` |
|        3 | 4711 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|        - | 4712 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|        3 | 4713 | `		PH7_MemObjRelease(pTos);` |
|        3 | 4714 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 4715 | `		pTos->nIdx = SXU32_HIGH;` |
|        3 | 4716 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|        3 | 4717 | `		break;` |
|        - | 4718 | `	}` |
|        - | 4719 | `	/* Extract memory object index */` |
|      163 | 4720 | `	nIdx = pTos->nIdx;` |
|      163 | 4721 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - | 4722 | `		/* Nullify the object */` |
|      163 | 4723 | `		PH7_MemObjRelease(pTos);` |
|        - | 4724 | `		/* Mark as constant and store the index on the top of the stack */` |
|      163 | 4725 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      163 | 4726 | `		pTos->nIdx = SXU32_HIGH;` |
|      163 | 4727 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       81 | 4728 | `	}` |
|      163 | 4729 | `	break;` |
|        - | 4730 | `					  }` |
|        - | 4731 | `/*` |
|        - | 4732 | ` * OP_STORE_REF * * P3` |
|        - | 4733 | ` * Perform an assignment operation by reference.` |
|        - | 4734 | ` */` |
|     1606 | 4735 | `case PH7_OP_STORE_REF: {` |
|        - | 4736 | `	VmOpRc rcOp;` |
|     3216 | 4737 | `	sState.pTos = pTos;` |
|     3216 | 4738 | `	sState.pc = pc;` |
|     3216 | 4739 | `	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);` |
|     3216 | 4740 | `	pTos = sState.pTos;` |
|     3216 | 4741 | `	pc = sState.pc;` |
|     3216 | 4742 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 4743 | `		goto Abort;` |
|     3213 | 4744 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        9 | 4745 | `		goto Exception;` |
|        - | 4746 | `	}` |
|     3205 | 4747 | `	break;` |
|        - | 4748 | `					  }` |
|        - | 4749 | `/*` |
|        - | 4750 | ` * OP_UPLINK P1 * *` |
|        - | 4751 | ` * Link a variable to the top active VM frame.` |
|        - | 4752 | ` * This is used to implement the 'global' PHP construct.` |
|        - | 4753 | ` */` |
|       40 | 4754 | `case PH7_OP_UPLINK: {` |
|       85 | 4755 | `	if( pVm->pFrame->pParent ){` |
|       85 | 4756 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - | 4757 | `		SyString sName;` |
|        - | 4758 | `		/* rcSv carries the coercion status OUT of the loop: the routing macro is a` |
|        - | 4759 | ``		 * bare `break` here, which would only leave the while and then pop the`` |
|        - | 4760 | `		 * operands with a throw pending. */` |
|       85 | 4761 | `		sxi32 rcSv = SXRET_OK;` |
|        - | 4762 | `		/* Perform the link */` |
|      177 | 4763 | `		while( pLink <= pTos ){` |
|        - | 4764 | `			/* Force a string cast — global $$arr link name (user-visible, §2) */` |
|      103 | 4765 | `			rcSv = PH7_MemObjToStringUV(pLink);` |
|      103 | 4766 | `			if( rcSv != SXRET_OK ){` |
|        7 | 4767 | `				break;` |
|        - | 4768 | `			}` |
|       97 | 4769 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       97 | 4770 | `			if( sName.nByte > 0 ){` |
|       97 | 4771 | `				VmFrameLink(&(*pVm),&sName);` |
|       46 | 4772 | `			}` |
|       97 | 4773 | `			pLink++;` |
|        5 | 4774 | `		}` |
|       85 | 4775 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|       37 | 4776 | `	}` |
|       79 | 4777 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       79 | 4778 | `	break;` |
|        - | 4779 | `					}` |
|        - | 4780 | `/*` |
|        - | 4781 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - | 4782 | ` * Push an exception in the corresponding container so that` |
|        - | 4783 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - | 4784 | ` */` |
|   728990 | 4785 | `case PH7_OP_LOAD_EXCEPTION: {` |
|        - | 4786 | `	VmOpRc rcOp;` |
|  1457985 | 4787 | `	sState.pTos = pTos;` |
|  1457985 | 4788 | `	sState.pc = pc;` |
|  1457985 | 4789 | `	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);` |
|  1457985 | 4790 | `	pTos = sState.pTos;` |
|  1457985 | 4791 | `	pc = sState.pc;` |
|  1457985 | 4792 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4793 | `		goto Abort;` |
|  1457985 | 4794 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4795 | `		goto Exception;` |
|        - | 4796 | `	}` |
|  1457985 | 4797 | `	break;` |
|        - | 4798 | `					  }` |
|        - | 4799 | `/*` |
|        - | 4800 | ` * OP_POP_EXCEPTION * * P3` |
|        - | 4801 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - | 4802 | ` */` |
|   678798 | 4803 | `case PH7_OP_POP_EXCEPTION: {` |
|  1357601 | 4804 | `	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;` |
|        - | 4805 | `	VmFrame *pBodyFrame;` |
|        - | 4806 | `	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on` |
|        - | 4807 | `	 * top (matched by compiled origin). pException == NULL means this try's` |
|        - | 4808 | `	 * activation was already consumed (an in-place catch handled a throw and` |
|        - | 4809 | `	 * ran the finally itself); compiled fields keep coming from p3. */` |
|  1357601 | 4810 | `	ph7_exception *pException = 0;` |
|  1357601 | 4811 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|     3209 | 4812 | `		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|     3209 | 4813 | `		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];` |
|        - | 4814 | `		/* Same compiled origin is NOT enough: under recursion, when THIS` |
|        - | 4815 | `		 * level's activation was already consumed by an in-place catch (the` |
|        - | 4816 | `		 * resume lands right here), the top can be an OUTER level's activation` |
|        - | 4817 | `		 * of the same lexical try — popping it would run that level's finally` |
|        - | 4818 | `		 * early and orphan its handler (probed: recursive try/catch/finally` |
|        - | 4819 | `		 * lost the outer catch entirely). The activation must also belong to` |
|        - | 4820 | `		 * the CURRENT body frame. */` |
|     3204 | 4821 | `		if( VmExcMatches(pTop,pCompiledExc)` |
|     3161 | 4822 | `		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){` |
|     3107 | 4823 | `			pException = pTop;` |
|     3107 | 4824 | `			(void)SySetPop(&pVm->aException);` |
|     1551 | 4825 | `		}` |
|     1602 | 4826 | `	}` |
|  1357601 | 4827 | `	if( pCompiledExc->iInlined ){` |
|        - | 4828 | `		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.` |
|        - | 4829 | `		 * Pop this try's handler off aException so a throw in the finally propagates to` |
|        - | 4830 | `		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the` |
|        - | 4831 | `		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the` |
|        - | 4832 | `		 * compiler-emitted JMP falls into the finally. Without a finally, this is the` |
|        - | 4833 | `		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */` |
|      167 | 4834 | `		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */` |
|      167 | 4835 | `		if( pCompiledExc->iHasFinally ){` |
|        - | 4836 | `			VmFinallyAction sAct;` |
|       14 | 4837 | `			SyZero(&sAct,sizeof(sAct));` |
|       14 | 4838 | `			sAct.eKind = PH7_FA_FALLTHROUGH;` |
|       14 | 4839 | `			sAct.iNextPc = pCompiledExc->iEndCatchPc;` |
|       14 | 4840 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        - | 4841 | `			/* keep the transparent frame for OP_END_FINALLY */` |
|      161 | 4842 | `		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       20 | 4843 | `			VmLeaveFrame(&(*pVm));` |
|        8 | 4844 | `		}` |
|      167 | 4845 | `		break;` |
|        - | 4846 | `	}` |
|        - | 4847 | `	/* Leave the exception frame. It is normally on top here (a try that fell through` |
|        - | 4848 | `	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a` |
|        - | 4849 | `	 * RESUMED generator/fiber body whose yield was inside this try, that exception` |
|        - | 4850 | `	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),` |
|        - | 4851 | `	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the` |
|        - | 4852 | `	 * coroutine (and defeat the bHasRet materialization just below, which must see the` |
|        - | 4853 | `	 * body). Only leave a genuine exception frame. */` |
|  1357439 | 4854 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|  1150473 | 4855 | `		VmLeaveFrame(&(*pVm));` |
|   575234 | 4856 | `	}` |
|        - | 4857 | `	/* Execute the finally block if present and not already executed by the` |
|        - | 4858 | `	 * catch path. No live activation (pException == NULL) means an in-place` |
|        - | 4859 | `	 * catch consumed it — and that path runs the finally itself — so skip. */` |
|  1357439 | 4860 | `	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){` |
|        - | 4861 | `		sxi32 rcFinally;` |
|       57 | 4862 | `		VmExcRelease(&(*pVm),pException);` |
|       57 | 4863 | `		pException = 0;` |
|       57 | 4864 | `		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);` |
|       57 | 4865 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 | 4866 | `			goto Abort;` |
|        - | 4867 | `		}` |
|       57 | 4868 | `		if( rcFinally == PH7_EXCEPTION ){` |
|        - | 4869 | `			/* The finally threw past itself. If an enclosing try IN THIS function` |
|        - | 4870 | `			 * caught the new exception in place, resume at its landing pad;` |
|        - | 4871 | `			 * otherwise it was caught at an outer frame, so unwind this function. */` |
|        - | 4872 | `			sxi32 iResumePc;` |
|        5 | 4873 | `			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 4874 | `				pc = iResumePc;` |
|        3 | 4875 | `				break;` |
|        - | 4876 | `			}` |
|        3 | 4877 | `			goto Exception;` |
|        - | 4878 | `		}` |
|       24 | 4879 | `	}` |
|  1357435 | 4880 | `	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */` |
|  1357435 | 4881 | `	pBodyFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  1357435 | 4882 | `	if( pBodyFrame->bHasRet ){` |
|        - | 4883 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - | 4884 | `		 * function. The return targets the body frame this try belongs to. Drain` |
|        - | 4885 | `		 * outer finally blocks first, then — only in the real function body` |
|        - | 4886 | `		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline` |
|        - | 4887 | `		 * try within a catch/finally) propagate outward so the owning body returns. */` |
|    20255 | 4888 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    20255 | 4889 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4890 | `			goto Abort;` |
|        - | 4891 | `		}` |
|    20255 | 4892 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 4893 | `			goto Exception;` |
|        - | 4894 | `		}` |
|    20255 | 4895 | `		if( !sState.bReturnPropagates ){` |
|    20249 | 4896 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|    10122 | 4897 | `		}` |
|    20255 | 4898 | `		goto Done;` |
|        - | 4899 | `	}` |
|  1337185 | 4900 | `	if( pBodyFrame->nCatchJmpPc > 0 ){` |
|        - | 4901 | ``		/* A `break`/`continue` inside the catch (OP_CATCH_JMP) parked its loop-exit`` |
|        - | 4902 | `		 * target on this body frame, and this is a landing pad on its way out. */` |
|       88 | 4903 | `		if( pBodyFrame->nCatchJmpLevels > 1 ){` |
|        - | 4904 | `			/* Still one or more detached bodies out from the target's array — this try` |
|        - | 4905 | `			 * was declared INSIDE another catch/finally mini-program. End this one and` |
|        - | 4906 | `			 * let the park travel outward; the next landing pad decrements again. */` |
|        6 | 4907 | `			pBodyFrame->nCatchJmpLevels--;` |
|        6 | 4908 | `			goto Done;` |
|        - | 4909 | `		}` |
|       84 | 4910 | `		if( pBodyFrame->nCatchJmpCross > 0 ){` |
|        - | 4911 | `			/* Enclosing trys between the catch and the loop: the jump lands past their` |
|        - | 4912 | `			 * OP_POP_EXCEPTION, so run their finally (and leave their frames) here. */` |
|        8 | 4913 | `			sxu32 nCross = pBodyFrame->nCatchJmpCross;` |
|        8 | 4914 | `			pBodyFrame->nCatchJmpCross = 0;` |
|        8 | 4915 | `			rc = VmDrainCrossedTrys(&(*pVm),nCross,sState.nExceptionBase);` |
|        8 | 4916 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4917 | `				goto Abort;` |
|        - | 4918 | `			}` |
|        8 | 4919 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 4920 | `				/* A drained finally threw past itself — it supersedes the loop exit. */` |
|      ! 0 | 4921 | `				pBodyFrame->nCatchJmpPc = 0;` |
|      ! 0 | 4922 | `				goto Exception;` |
|        - | 4923 | `			}` |
|        3 | 4924 | `		}` |
|       84 | 4925 | `		pc = (sxi32)pBodyFrame->nCatchJmpPc - 1;` |
|       84 | 4926 | `		pBodyFrame->nCatchJmpPc = 0;` |
|       84 | 4927 | `		break;` |
|        - | 4928 | `	}` |
|  1337101 | 4929 | `	break;` |
|        - | 4930 | `							}` |
|        - | 4931 | `/*` |
|        - | 4932 | ` * OP_CATCH_JMP P1(levels,cross) P2(target pc) *` |
|        - | 4933 | ` * Jump to iP2, leaving the try/catch structures iP1 describes (PH7_CATCH_JMP_P1).` |
|        - | 4934 | ` *` |
|        - | 4935 | ` * CROSS enclosing trys are being left without reaching their OP_POP_EXCEPTION, so this` |
|        - | 4936 | ` * runs their finallys itself. LEVELS says whether the target is even addressable from` |
|        - | 4937 | `` * here: 0 means it is in this same array (a `goto` out of a try body) and the jump is`` |
|        - | 4938 | ` * taken on the spot; above 0 the jump starts inside a DETACHED catch/finally` |
|        - | 4939 | ` * mini-program whose array is not the target's, so the target is PARKED on the owning` |
|        - | 4940 | `` * body's frame and this mini-program ends — mirroring how an explicit `return` in a`` |
|        - | 4941 | ` * catch parks on sRet at OP_DONE above. VmThrowException then runs this try's finally` |
|        - | 4942 | ` * and returns; the resume lands on the try's OP_POP_EXCEPTION, which decrements LEVELS` |
|        - | 4943 | ` * and either re-parks (another mini-program out) or takes the jump.` |
|        - | 4944 | ` */` |
|       47 | 4945 | `case PH7_OP_CATCH_JMP: {` |
|        - | 4946 | `	VmFrame *pTgt;` |
|       98 | 4947 | `	if( PH7_CATCH_JMP_LEVELS(pInstr->iP1) == 0 ){` |
|        - | 4948 | ``		/* No detached body to leave — a `goto` whose target is in THIS array, but which`` |
|        - | 4949 | `		 * jumps out of one or more enclosing trys. Nothing to park: run their finallys` |
|        - | 4950 | `		 * (a bare jump would leave them to fire at teardown) and go. */` |
|       15 | 4951 | `		rc = VmDrainCrossedTrys(&(*pVm),PH7_CATCH_JMP_CROSS(pInstr->iP1),sState.nExceptionBase);` |
|       15 | 4952 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4953 | `			goto Abort;` |
|        - | 4954 | `		}` |
|       15 | 4955 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 4956 | `			/* A drained finally threw past itself — it supersedes the jump. */` |
|      ! 0 | 4957 | `			goto Exception;` |
|        - | 4958 | `		}` |
|       15 | 4959 | `		pc = (sxi32)pInstr->iP2 - 1;` |
|       15 | 4960 | `		break;` |
|        - | 4961 | `	}` |
|       86 | 4962 | `	pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|       86 | 4963 | `	pTgt->nCatchJmpPc = (sxu32)pInstr->iP2;` |
|       86 | 4964 | `	pTgt->nCatchJmpLevels = PH7_CATCH_JMP_LEVELS(pInstr->iP1);` |
|       86 | 4965 | `	pTgt->nCatchJmpCross = PH7_CATCH_JMP_CROSS(pInstr->iP1);` |
|        - | 4966 | `	/* A try opened INSIDE this catch body that the jump crosses had its finally run by` |
|        - | 4967 | `	 * the compiler-emitted OP_POP_EXCEPTION; drain anything still pending here. */` |
|       86 | 4968 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|       86 | 4969 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4970 | `		goto Abort;` |
|        - | 4971 | `	}` |
|       86 | 4972 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 4973 | `		/* A drained finally threw past itself — it discards this jump. */` |
|      ! 0 | 4974 | `		pTgt->nCatchJmpPc = 0;` |
|      ! 0 | 4975 | `		goto Exception;` |
|        - | 4976 | `	}` |
|       86 | 4977 | `	goto Done;` |
|        - | 4978 | `					   }` |
|        - | 4979 | `/*` |
|        - | 4980 | ` * OP_CATCH iP1(catch-index) * P3(ph7_exception)` |
|        - | 4981 | ` * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on` |
|        - | 4982 | ` * pException->pInflight by VmThrowInline) into the catch variable, resolved in the` |
|        - | 4983 | ` * enclosing body's scope (PHP: a catch shares the surrounding variable scope).` |
|        - | 4984 | ` */` |
|       38 | 4985 | `case PH7_OP_CATCH: {` |
|        - | 4986 | `	VmOpRc rcOp;` |
|       81 | 4987 | `	sState.pTos = pTos;` |
|       81 | 4988 | `	sState.pc = pc;` |
|       81 | 4989 | `	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);` |
|       81 | 4990 | `	pTos = sState.pTos;` |
|       81 | 4991 | `	pc = sState.pc;` |
|       81 | 4992 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4993 | `		goto Abort;` |
|       81 | 4994 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4995 | `		goto Exception;` |
|        - | 4996 | `	}` |
|       81 | 4997 | `	break;` |
|        - | 4998 | `					  }` |
|        - | 4999 | `/*` |
|        - | 5000 | ` * OP_END_FINALLY * P3(ph7_exception)` |
|        - | 5001 | ` * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch` |
|        - | 5002 | ` * the pending action queued when the finally was entered (fall-through / re-throw /` |
|        - | 5003 | ` * return / break-continue). A return/break threads out through each enclosing finally` |
|        - | 5004 | ` * via pException->iNextFinallyPc.` |
|        - | 5005 | ` */` |
|       24 | 5006 | `case PH7_OP_END_FINALLY: {` |
|       52 | 5007 | `	ph7_exception *pExc = (ph7_exception *)pInstr->p3;` |
|        - | 5008 | `	VmFinallyAction sAct;` |
|       52 | 5009 | `	int eKind = PH7_FA_FALLTHROUGH;` |
|        - | 5010 | `	/* Leave the try's transparent frame kept alive across the finally. */` |
|       52 | 5011 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        3 | 5012 | `		VmLeaveFrame(&(*pVm));` |
|        1 | 5013 | `	}` |
|       52 | 5014 | `	if( SySetUsed(&pVm->aFinallyAction) > 0 ){` |
|       52 | 5015 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);` |
|       52 | 5016 | `		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];` |
|       52 | 5017 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|       52 | 5018 | `		eKind = sAct.eKind;` |
|       28 | 5019 | `	}else{` |
|      ! 0 | 5020 | `		SyZero(&sAct,sizeof(sAct));` |
|      ! 0 | 5021 | `		sAct.iNextPc = pExc->iEndCatchPc;` |
|        - | 5022 | `	}` |
|       52 | 5023 | `	if( eKind == PH7_FA_FALLTHROUGH ){` |
|       12 | 5024 | `		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;` |
|       16 | 5025 | `		break;` |
|       42 | 5026 | `	}else if( eKind == PH7_FA_JMP ){` |
|        - | 5027 | `		/* break/continue: run the remaining crossed finallys, then take the jump. */` |
|        5 | 5028 | `		sxu32 iFpc = 0;` |
|        5 | 5029 | `		int nCross = sAct.nCross;` |
|        5 | 5030 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|      ! 0 | 5031 | `			sAct.nCross = nCross;` |
|      ! 0 | 5032 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      ! 0 | 5033 | `			pc = (sxi32)iFpc - 1;` |
|      ! 0 | 5034 | `			break;` |
|        - | 5035 | `		}` |
|        5 | 5036 | `		pc = (sxi32)sAct.iNextPc - 1;` |
|        5 | 5037 | `		break;` |
|       38 | 5038 | `	}else if( eKind == PH7_FA_RETHROW ){` |
|        8 | 5039 | `		ph7_class_instance *pRe = sAct.pExc;` |
|        - | 5040 | `		sxi32 _iRpE;` |
|        8 | 5041 | `		rc = VmThrowException(&(*pVm),pRe);` |
|        8 | 5042 | `		if( pRe ){ PH7_ClassInstanceUnref(pRe); }` |
|        8 | 5043 | `		if( rc == SXERR_ABORT ){ goto Abort; }` |
|        8 | 5044 | `		PH7_INLINE_RESUME_BREAK()` |
|        8 | 5045 | `		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }` |
|        8 | 5046 | `		goto Exception;` |
|      ! 0 | 5047 | `	}else{ /* PH7_FA_RETURN */` |
|       31 | 5048 | `		sxu32 iFpc = 0;` |
|       31 | 5049 | `		int nCross = sAct.nCross;` |
|       31 | 5050 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        - | 5051 | `			/* Thread the return through the next enclosing finally. */` |
|        6 | 5052 | `			sAct.nCross = nCross;` |
|        6 | 5053 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        6 | 5054 | `			pc = (sxi32)iFpc - 1;` |
|        6 | 5055 | `			break;` |
|        - | 5056 | `		}` |
|        - | 5057 | `		/* No enclosing finally left: materialize the return from this body. */` |
|       27 | 5058 | `		if( sAct.bHasRetVal && sState.pResult ){` |
|        6 | 5059 | `			PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|        2 | 5060 | `		}` |
|       27 | 5061 | `		PH7_MemObjRelease(&sAct.sRet);` |
|       27 | 5062 | `		goto Done;` |
|        - | 5063 | `	}` |
|        - | 5064 | `						 }` |
|        - | 5065 | `/*` |
|        - | 5066 | ` * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)` |
|        - | 5067 | `` * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a`` |
|        - | 5068 | ` * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);` |
|        - | 5069 | ` * OP_END_FINALLY threads it out through the finally chain and then returns.` |
|        - | 5070 | ` */` |
|       22 | 5071 | `case PH7_OP_SET_FINALLY_RET: {` |
|        - | 5072 | `	VmFinallyAction sAct;` |
|       48 | 5073 | `	sxu32 iFpc = 0;` |
|       48 | 5074 | `	int nCross = -1; /* a return crosses every enclosing finally in this function */` |
|       48 | 5075 | `	SyZero(&sAct,sizeof(sAct));` |
|       48 | 5076 | `	sAct.eKind = PH7_FA_RETURN;` |
|       48 | 5077 | `	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       48 | 5078 | `	PH7_MemObjInit(pVm,&sAct.sRet);` |
|       48 | 5079 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|       36 | 5080 | `		PH7_MemObjStore(pTos,&sAct.sRet);` |
|       36 | 5081 | `		sAct.bHasRetVal = 1;` |
|       36 | 5082 | `		VmPopOperand(&pTos,1);` |
|       16 | 5083 | `	}` |
|       48 | 5084 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        9 | 5085 | `		sAct.nCross = nCross;` |
|        9 | 5086 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        9 | 5087 | `		pc = (sxi32)iFpc - 1;` |
|        9 | 5088 | `		break;` |
|        - | 5089 | `	}` |
|        - | 5090 | `	/* No enclosing finally left: return now. */` |
|       42 | 5091 | `	if( sAct.bHasRetVal && sState.pResult ){` |
|       32 | 5092 | `		PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|       14 | 5093 | `	}` |
|       42 | 5094 | `	PH7_MemObjRelease(&sAct.sRet);` |
|       42 | 5095 | `	goto Done;` |
|        - | 5096 | `						 }` |
|        - | 5097 | `/*` |
|        - | 5098 | ` * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)` |
|        - | 5099 | `` * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action`` |
|        - | 5100 | ` * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.` |
|        - | 5101 | ` */` |
|        4 | 5102 | `case PH7_OP_SET_FINALLY_JMP: {` |
|        - | 5103 | `	VmFinallyAction sAct;` |
|       11 | 5104 | `	sxu32 iFpc = 0;` |
|       11 | 5105 | `	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */` |
|       11 | 5106 | `	SyZero(&sAct,sizeof(sAct));` |
|       11 | 5107 | `	sAct.eKind = PH7_FA_JMP;` |
|       11 | 5108 | `	sAct.iNextPc = pInstr->iP2;` |
|       11 | 5109 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        5 | 5110 | `		sAct.nCross = nCross;` |
|        5 | 5111 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        5 | 5112 | `		pc = (sxi32)iFpc - 1;` |
|        5 | 5113 | `		break;` |
|        - | 5114 | `	}` |
|        - | 5115 | `	/* No finally among the crossed trys: just take the break/continue jump. */` |
|        6 | 5116 | `	pc = (sxi32)sAct.iNextPc - 1;` |
|        6 | 5117 | `	break;` |
|        - | 5118 | `						 }` |
|        - | 5119 | `/*` |
|        - | 5120 | ` * OP_THROW * P2 *` |
|        - | 5121 | ` * Throw an user exception.` |
|        - | 5122 | ` */` |
|   500394 | 5123 | `case PH7_OP_THROW: {` |
|        - | 5124 | `	VmOpRc rcOp;` |
|  1000793 | 5125 | `	sState.pTos = pTos;` |
|  1000793 | 5126 | `	sState.pc = pc;` |
|  1000793 | 5127 | `	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);` |
|  1000793 | 5128 | `	pTos = sState.pTos;` |
|  1000793 | 5129 | `	pc = sState.pc;` |
|  1000793 | 5130 | `	if( rcOp == VM_OP_ABORT ){` |
|       27 | 5131 | `		goto Abort;` |
|  1000771 | 5132 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|   600329 | 5133 | `		goto Exception;` |
|        - | 5134 | `	}` |
|   400447 | 5135 | `	break;` |
|        - | 5136 | `					  }` |
|        - | 5137 | `/*` |
|        - | 5138 | ` * OP_FOREACH_INIT * P2 P3` |
|        - | 5139 | ` * Prepare a foreach step.` |
|        - | 5140 | ` */` |
|    14046 | 5141 | `case PH7_OP_FOREACH_INIT: {` |
|        - | 5142 | `	VmOpRc rcOp;` |
|    28097 | 5143 | `	sState.pTos = pTos;` |
|    28097 | 5144 | `	sState.pc = pc;` |
|    28097 | 5145 | `	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);` |
|    28097 | 5146 | `	pTos = sState.pTos;` |
|    28097 | 5147 | `	pc = sState.pc;` |
|    28097 | 5148 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5149 | `		goto Abort;` |
|    28097 | 5150 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 5151 | `		goto Exception;` |
|        - | 5152 | `	}` |
|    28093 | 5153 | `	break;` |
|        - | 5154 | `					  }` |
|        - | 5155 | `/*` |
|        - | 5156 | ` * OP_FOREACH_STEP * P2 P3` |
|        - | 5157 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - | 5158 | ` */` |
|   166788 | 5159 | `case PH7_OP_FOREACH_STEP: {` |
|        - | 5160 | `	VmOpRc rcOp;` |
|   333581 | 5161 | `	sState.pTos = pTos;` |
|   333581 | 5162 | `	sState.pc = pc;` |
|   333581 | 5163 | `	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);` |
|   333581 | 5164 | `	pTos = sState.pTos;` |
|   333581 | 5165 | `	pc = sState.pc;` |
|   333581 | 5166 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 5167 | `		goto Abort;` |
|   333579 | 5168 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5169 | `		goto Exception;` |
|        - | 5170 | `	}` |
|   333579 | 5171 | `	break;` |
|        - | 5172 | `						  }` |
|        - | 5173 | `/*` |
|        - | 5174 | ` * OP_MEMBER P1 P2` |
|        - | 5175 | ` * Load class attribute/method on the stack.` |
|        - | 5176 | ` */` |
|   162297 | 5177 | `case PH7_OP_MEMBER: {` |
|        - | 5178 | `	VmOpRc rcOp;` |
|   324602 | 5179 | `	sState.pTos = pTos;` |
|   324602 | 5180 | `	sState.pc = pc;` |
|   324602 | 5181 | `	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);` |
|   324602 | 5182 | `	pTos = sState.pTos;` |
|   324602 | 5183 | `	pc = sState.pc;` |
|   324602 | 5184 | `	if( rcOp == VM_OP_ABORT ){` |
|        8 | 5185 | `		goto Abort;` |
|   324596 | 5186 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      153 | 5187 | `		goto Exception;` |
|        - | 5188 | `	}` |
|   324446 | 5189 | `	break;` |
|        - | 5190 | `					  }` |
|        - | 5191 | `/*` |
|        - | 5192 | ` * OP_NEW P1 * * *` |
|        - | 5193 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - | 5194 | ` */` |
|  1056600 | 5195 | `case PH7_OP_NEW: {` |
|        - | 5196 | `	VmOpRc rcOp;` |
|  2113205 | 5197 | `	sState.pTos = pTos;` |
|  2113205 | 5198 | `	sState.pc = pc;` |
|  2113205 | 5199 | `	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);` |
|  2113205 | 5200 | `	pTos = sState.pTos;` |
|  2113205 | 5201 | `	pc = sState.pc;` |
|  2113205 | 5202 | `	if( rcOp == VM_OP_ABORT ){` |
|        5 | 5203 | `		goto Abort;` |
|  2113201 | 5204 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      136 | 5205 | `		goto Exception;` |
|        - | 5206 | `	}` |
|  2113067 | 5207 | `	break;` |
|        - | 5208 | `					  }` |
|        - | 5209 | `/*` |
|        - | 5210 | ` * OP_CLONE * * *` |
|        - | 5211 | ` * Perfome a clone operation.` |
|        - | 5212 | ` */` |
|      120 | 5213 | `case PH7_OP_CLONE: {` |
|        - | 5214 | `	VmOpRc rcOp;` |
|      245 | 5215 | `	sState.pTos = pTos;` |
|      245 | 5216 | `	sState.pc = pc;` |
|      245 | 5217 | `	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);` |
|      245 | 5218 | `	pTos = sState.pTos;` |
|      245 | 5219 | `	pc = sState.pc;` |
|      245 | 5220 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5221 | `		goto Abort;` |
|      245 | 5222 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       19 | 5223 | `		goto Exception;` |
|        - | 5224 | `	}` |
|      227 | 5225 | `	break;` |
|        - | 5226 | `					  }` |
|        - | 5227 | `/*` |
|        - | 5228 | ` * OP_SWITCH * * P3` |
|        - | 5229 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - | 5230 | ` */` |
|       36 | 5231 | `case PH7_OP_SWITCH: {` |
|        - | 5232 | `	VmOpRc rcOp;` |
|       77 | 5233 | `	sState.pTos = pTos;` |
|       77 | 5234 | `	sState.pc = pc;` |
|       77 | 5235 | `	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);` |
|       77 | 5236 | `	pTos = sState.pTos;` |
|       77 | 5237 | `	pc = sState.pc;` |
|       77 | 5238 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5239 | `		goto Abort;` |
|       77 | 5240 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5241 | `		goto Exception;` |
|        - | 5242 | `	}` |
|       77 | 5243 | `	break;` |
|        - | 5244 | `					  }` |
|        - | 5245 | `/*` |
|        - | 5246 | ` * OP_MATCH * * P3` |
|        - | 5247 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - | 5248 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - | 5249 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - | 5250 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - | 5251 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - | 5252 | ` */` |
|       77 | 5253 | `case PH7_OP_MATCH: {` |
|        - | 5254 | `	VmOpRc rcOp;` |
|      158 | 5255 | `	sState.pTos = pTos;` |
|      158 | 5256 | `	sState.pc = pc;` |
|      158 | 5257 | `	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);` |
|      158 | 5258 | `	pTos = sState.pTos;` |
|      158 | 5259 | `	pc = sState.pc;` |
|      158 | 5260 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5261 | `		goto Abort;` |
|      158 | 5262 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        6 | 5263 | `		goto Exception;` |
|        - | 5264 | `	}` |
|      153 | 5265 | `	break;` |
|        - | 5266 | `					  }` |
|        - | 5267 | `/*` |
|        - | 5268 | ` * OP_YIELD P1 P2 *` |
|        - | 5269 | ` *  Yield a value from a generator function.` |
|        - | 5270 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - | 5271 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - | 5272 | ` */` |
|      595 | 5273 | `case PH7_OP_YIELD: {` |
|        - | 5274 | `	ph7_generator *pGen;` |
|     1195 | 5275 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5276 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 | 5277 | `		goto Abort;` |
|        - | 5278 | `	}` |
|     1195 | 5279 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5280 | ``		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's`` |
|        - | 5281 | `		 * pending finallys tried to yield — PHP forbids it. */` |
|      ! 0 | 5282 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5283 | `			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5284 | `			goto Abort;` |
|        - | 5285 | `		}` |
|      ! 0 | 5286 | `		goto Exception;` |
|        - | 5287 | `	}` |
|     1195 | 5288 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|     1195 | 5289 | `	if( pInstr->iP2 ){` |
|        - | 5290 | `		/* yield $key => $value: value on top, key below */` |
|        - | 5291 | `#ifdef UNTRUST` |
|        - | 5292 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - | 5293 | `#endif` |
|       20 | 5294 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       20 | 5295 | `		VmPopOperand(&pTos, 1);` |
|       20 | 5296 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       20 | 5297 | `		VmPopOperand(&pTos, 1);` |
|        - | 5298 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       20 | 5299 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 | 5300 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 | 5301 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 | 5302 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 | 5303 | `			}` |
|        2 | 5304 | `		}` |
|     1186 | 5305 | `	}else if( pInstr->iP1 ){` |
|        - | 5306 | `		/* yield $value */` |
|        - | 5307 | `#ifdef UNTRUST` |
|        - | 5308 | `		if( pTos < pStack ) goto Abort;` |
|        - | 5309 | `#endif` |
|     1177 | 5310 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|     1177 | 5311 | `		VmPopOperand(&pTos, 1);` |
|        - | 5312 | `		/* Auto-increment key */` |
|     1177 | 5313 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|     1177 | 5314 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|     1177 | 5315 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|      591 | 5316 | `	}else{` |
|        - | 5317 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 | 5318 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 5319 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 5320 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 | 5321 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - | 5322 | `	}` |
|        - | 5323 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|     1195 | 5324 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|     1195 | 5325 | `	goto Suspend;` |
|        - | 5326 | `}` |
|        - | 5327 | `/*` |
|        - | 5328 | ` * OP_YIELD_FROM * * *` |
|        - | 5329 | ` *` |
|        - | 5330 | `` * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an`` |
|        - | 5331 | ` * array/Traversable/Generator from the OUTER generator, preserving the inner` |
|        - | 5332 | ` * keys; the expression evaluates to the inner Generator's return value (or NULL).` |
|        - | 5333 | ` *` |
|        - | 5334 | ` * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,` |
|        - | 5335 | ` * advances the per-instance delegate cursor stored on the exec context (never the` |
|        - | 5336 | ` * shared foreach aStep, so independent generator instances cannot clash). The` |
|        - | 5337 | ` * iterable operand is consumed on first entry; the expression result is pushed at` |
|        - | 5338 | ` * exhaustion — net stack effect +1, identical to OP_YIELD.` |
|        - | 5339 | ` */` |
|       93 | 5340 | `case PH7_OP_YIELD_FROM: {` |
|        - | 5341 | `	ph7_generator *pGenFrom;` |
|        - | 5342 | `	ph7_exec_ctx *pCtxFrom;` |
|        - | 5343 | `	ph7_value sKey,sVal;` |
|      191 | 5344 | `	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */` |
|      191 | 5345 | `	int bExhausted = 0;` |
|      191 | 5346 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5347 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");` |
|      ! 0 | 5348 | `		goto Abort;` |
|        - | 5349 | `	}` |
|      191 | 5350 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5351 | ``		/* A `yield from` reached while VmCloseCtx force-drives this destroyed`` |
|        - | 5352 | `		 * generator's pending finallys — PHP forbids it (distinct message from a` |
|        - | 5353 | ``		 * bare `yield`, mirroring the OP_YIELD guard above). */`` |
|      ! 0 | 5354 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5355 | `			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5356 | `			goto Abort;` |
|        - | 5357 | `		}` |
|      ! 0 | 5358 | `		goto Exception;` |
|        - | 5359 | `	}` |
|      191 | 5360 | `	pCtxFrom = pVm->pActiveCtx;` |
|      191 | 5361 | `	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;` |
|      191 | 5362 | `	PH7_MemObjInit(pVm,&sKey);` |
|      191 | 5363 | `	PH7_MemObjInit(pVm,&sVal);` |
|      191 | 5364 | `	if( pCtxFrom->iDelegateState == 0 ){` |
|        - | 5365 | `		/* First entry: classify the iterable on the stack top. */` |
|       79 | 5366 | `		int bIterable = 1;` |
|        - | 5367 | `#ifdef UNTRUST` |
|        - | 5368 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 5369 | `#endif` |
|       79 | 5370 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       29 | 5371 | `			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       29 | 5372 | `			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;` |
|       29 | 5373 | `			pCtxFrom->iDelegateState = 1;` |
|       67 | 5374 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       51 | 5375 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       51 | 5376 | `			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       51 | 5377 | `			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){` |
|       41 | 5378 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       41 | 5379 | `				pCtxFrom->iDelegateState = 3;` |
|       31 | 5380 | `			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){` |
|        9 | 5381 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        9 | 5382 | `				pCtxFrom->iDelegateState = 2;` |
|        6 | 5383 | `			}else{` |
|        6 | 5384 | `				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - | 5385 | `					sizeof("IteratorAggregate")-1,FALSE,0);` |
|        8 | 5386 | `				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){` |
|        - | 5387 | `					/* Delegate to the Iterator returned by getIterator() */` |
|        - | 5388 | `					ph7_value sIt;` |
|        6 | 5389 | `					PH7_MemObjInit(pVm,&sIt);` |
|        6 | 5390 | `					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);` |
|        6 | 5391 | `					if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){` |
|        - | 5392 | `						/* getIterator() threw/aborted: drop it, consume the` |
|        - | 5393 | `						 * operand, and propagate. */` |
|      ! 0 | 5394 | `						PH7_MemObjRelease(&sIt);` |
|      ! 0 | 5395 | `						VmPopOperand(&pTos,1);` |
|      ! 0 | 5396 | `						goto yf_propagate;` |
|        - | 5397 | `					}` |
|        4 | 5398 | `					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls` |
|        6 | 5399 | `						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){` |
|        6 | 5400 | `						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);` |
|        6 | 5401 | `						pCtxFrom->iDelegateState = 2;` |
|        4 | 5402 | `					}else{` |
|      ! 0 | 5403 | `						bIterable = 0;` |
|        - | 5404 | `					}` |
|        6 | 5405 | `					PH7_MemObjRelease(&sIt);` |
|        4 | 5406 | `				}else{` |
|      ! 0 | 5407 | `					bIterable = 0;` |
|        - | 5408 | `				}` |
|        - | 5409 | `			}` |
|       28 | 5410 | `		}else{` |
|        6 | 5411 | `			bIterable = 0;` |
|        - | 5412 | `		}` |
|       79 | 5413 | `		VmPopOperand(&pTos,1); /* Consume the iterable operand */` |
|       79 | 5414 | `		if( !bIterable ){` |
|        - | 5415 | `			/* Non-iterable source: throw a catchable Error (PHP 8.5), then` |
|        - | 5416 | `			 * funnel through the shared teardown/route path. */` |
|        6 | 5417 | `			rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 5418 | `				"Can use \"yield from\" only with arrays and Traversables",` |
|        - | 5419 | `				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);` |
|        6 | 5420 | `			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        6 | 5421 | `			goto yf_propagate;` |
|        - | 5422 | `		}` |
|       75 | 5423 | `		if( pCtxFrom->iDelegateState >= 2 ){` |
|        - | 5424 | `			/* rewind() the delegate (also starts a fresh generator) */` |
|       51 | 5425 | `			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 5426 | `				"rewind",sizeof("rewind")-1,0);` |
|       51 | 5427 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       23 | 5428 | `		}` |
|       40 | 5429 | `	}else{` |
|        - | 5430 | `		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a` |
|        - | 5431 | `		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()` |
|        - | 5432 | `		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)` |
|        - | 5433 | `		 * ignore send() and just advance with next(). */` |
|        - | 5434 | `#ifdef UNTRUST` |
|        - | 5435 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 5436 | `#endif` |
|      117 | 5437 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       68 | 5438 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|        - | 5439 | `			/* A pending Generator::throw() on the outer was parked on pInjected by the` |
|        - | 5440 | `			 * body-entry gate (which skips its own raise for state 3); forward it as a` |
|        - | 5441 | `			 * throw into the delegate, else forward the sent value (pTos, which` |
|        - | 5442 | `			 * VmResumeCtx copies into the inner's own stack). */` |
|       68 | 5443 | `			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;` |
|       68 | 5444 | `			pCtxFrom->pInjected = 0;` |
|       68 | 5445 | `			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       68 | 5446 | `				if( pInjFwd ){` |
|        - | 5447 | `					/* Borrowed ref: the outer's Generator::throw() holds it across this` |
|        - | 5448 | `					 * whole resume, so the inner inject path must not unref it. */` |
|        5 | 5449 | `					pInner->pCtx->pInjected = pInjFwd;` |
|        5 | 5450 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);` |
|        5 | 5451 | `					pInner->pCtx->pInjected = 0;` |
|        3 | 5452 | `				}else{` |
|       64 | 5453 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);` |
|        4 | 5454 | `				}` |
|       32 | 5455 | `			}else if( pInjFwd ){` |
|        - | 5456 | `				/* No live delegate to receive the throw (inner already finished/closed):` |
|        - | 5457 | `				 * raise it at the yield-from in the outer generator's own frame. */` |
|      ! 0 | 5458 | `				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);` |
|      ! 0 | 5459 | `				pTF->iFlags \|= VM_FRAME_THROW;` |
|      ! 0 | 5460 | `				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 5461 | `			}` |
|       68 | 5462 | `			PH7_MemObjRelease(pTos);` |
|       68 | 5463 | `			pTos--;` |
|       68 | 5464 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       32 | 5465 | `		}else{` |
|       53 | 5466 | `			PH7_MemObjRelease(pTos);` |
|       53 | 5467 | `			pTos--;` |
|       53 | 5468 | `			if( pCtxFrom->iDelegateState >= 2 ){` |
|       17 | 5469 | `				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 5470 | `					"next",sizeof("next")-1,0);` |
|       17 | 5471 | `				if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 5472 | `			}` |
|        - | 5473 | `		}` |
|        - | 5474 | `	}` |
|        - | 5475 | `	/* Fetch the current (key,value) of the delegate, or mark exhausted. */` |
|      177 | 5476 | `	if( pCtxFrom->iDelegateState == 1 ){` |
|       63 | 5477 | `		if( pCtxFrom->pDelegateNode == 0 ){` |
|       21 | 5478 | `			bExhausted = 1;` |
|       13 | 5479 | `		}else{` |
|       47 | 5480 | `			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);` |
|       47 | 5481 | `			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);` |
|        - | 5482 | `			/* Forward traversal follows pPrev (the hashmap's "reverse link",` |
|        - | 5483 | `			 * matching PH7_HashmapGetNextEntry). */` |
|       47 | 5484 | `			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;` |
|        - | 5485 | `		}` |
|       34 | 5486 | `	}else{` |
|      119 | 5487 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|        - | 5488 | `		ph7_value sValid;` |
|        - | 5489 | `		int isValid;` |
|      119 | 5490 | `		PH7_MemObjInit(pVm,&sValid);` |
|      119 | 5491 | `		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|      119 | 5492 | `		PH7_MemObjToBool(&sValid);` |
|      119 | 5493 | `		isValid = (sValid.x.iVal != 0);` |
|      119 | 5494 | `		PH7_MemObjRelease(&sValid);` |
|      119 | 5495 | `		if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      119 | 5496 | `		if( !isValid ){` |
|       28 | 5497 | `			bExhausted = 1;` |
|       16 | 5498 | `		}else{` |
|       95 | 5499 | `			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|       95 | 5500 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       95 | 5501 | `			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       95 | 5502 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        - | 5503 | `		}` |
|        - | 5504 | `	}` |
|      177 | 5505 | `	if( bExhausted ){` |
|        - | 5506 | `		/* Expression value: inner Generator's return value (state 3) or NULL. */` |
|        - | 5507 | `		ph7_value sResult;` |
|       45 | 5508 | `		PH7_MemObjInit(pVm,&sResult);` |
|       45 | 5509 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       23 | 5510 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|       23 | 5511 | `			if( pInner && pInner->pCtx ){` |
|       23 | 5512 | `				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);` |
|       10 | 5513 | `			}` |
|       10 | 5514 | `		}` |
|       45 | 5515 | `		PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       45 | 5516 | `		pCtxFrom->pDelegateNode = 0;` |
|       45 | 5517 | `		pCtxFrom->iDelegateState = 0;` |
|       45 | 5518 | `		pTos++;` |
|       45 | 5519 | `		PH7_MemObjStore(&sResult,pTos);` |
|       45 | 5520 | `		PH7_MemObjRelease(&sResult);` |
|       45 | 5521 | `		PH7_MemObjRelease(&sKey);` |
|       45 | 5522 | `		PH7_MemObjRelease(&sVal);` |
|       45 | 5523 | `		break; /* fall through to pc+1 with the result on the stack top */` |
|        - | 5524 | `	}` |
|        - | 5525 | `	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.` |
|        - | 5526 | `	 * The outer generator's implicit auto-key counter is NOT advanced by the` |
|        - | 5527 | ``	 * delegated keys — PHP keeps it independent across `yield from`, so a later`` |
|        - | 5528 | ``	 * plain `yield` continues from the outer's own counter. */`` |
|      137 | 5529 | `	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);` |
|      137 | 5530 | `	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);` |
|      137 | 5531 | `	PH7_MemObjRelease(&sKey);` |
|      137 | 5532 | `	PH7_MemObjRelease(&sVal);` |
|        - | 5533 | `	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */` |
|      137 | 5534 | `	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));` |
|      137 | 5535 | `	goto Suspend;` |
|        7 | 5536 | `yf_propagate:` |
|        - | 5537 | `	/* A delegate iterator method threw/aborted (or the source was non-iterable):` |
|        - | 5538 | `	 * tear down the delegation, then route via the shared dispatch macro. rcm is` |
|        - | 5539 | `	 * always PH7_EXCEPTION or PH7_ABORT here. */` |
|       18 | 5540 | `	PH7_MemObjRelease(&sKey);` |
|       18 | 5541 | `	PH7_MemObjRelease(&sVal);` |
|       18 | 5542 | `	PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       18 | 5543 | `	pCtxFrom->pDelegateNode = 0;` |
|       18 | 5544 | `	pCtxFrom->iDelegateState = 0;` |
|       18 | 5545 | `	PH7_DISPATCH_ENFORCE_RC(rcm)` |
|      ! 0 | 5546 | `	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */` |
|        - | 5547 | `}` |
|        - | 5548 | `/*` |
|        - | 5549 | ` * OP_CALL P1 * *` |
|        - | 5550 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - | 5551 | ` *  function on the stack.` |
|        - | 5552 | ` */` |
|        - | 5553 | `/*` |
|        - | 5554 | ` * OP_CALL_INIT * P2 *` |
|        - | 5555 | ` *  Screen the callee on TOS where it is WRITTEN — before this call's arguments run.` |
|        - | 5556 | ` *` |
|        - | 5557 | ` *  php resolves a call's target at INIT_FCALL / INIT_FCALL_BY_NAME / INIT_DYNAMIC_CALL` |
|        - | 5558 | `` *  and raises there, so `undefinedFn(s(1))`, `$f(s(1))` over a misspelled name and`` |
|        - | 5559 | `` *  `$v(s(1))` over an int all refuse BEFORE `s(1)` runs. PHL only ever looked at the`` |
|        - | 5560 | ` *  callee inside OP_CALL, one instruction after the whole argument list, so every one` |
|        - | 5561 | ` *  of those programs produced the argument's side effects (or its exception) first and` |
|        - | 5562 | ` *  php's Error second. The messages were already identical; only the order was not.` |
|        - | 5563 | ` *` |
|        - | 5564 | ` *  The verdict is the FIRST-CLASS-CALLABLE creation screen, unchanged and shared: php` |
|        - | 5565 | `` *  gives `f(...)` the direct call's taxonomy word for word, which makes VmFccValueError`` |
|        - | 5566 | ` *  the one builder for both. The value is left exactly as it is — OP_CALL still does its` |
|        - | 5567 | ` *  own resolution — so this adds a refusal and changes nothing that succeeds. P2 == 1` |
|        - | 5568 | ` *  when the compiler namespace-qualified the name, which is the one bit php's` |
|        - | 5569 | `` *  global-function fallback needs (an unqualified `strlen(...)` inside a namespace).`` |
|        - | 5570 | ` *` |
|        - | 5571 | ` *  Not emitted for a callee whose OP_MEMBER already screened it, for a first-class` |
|        - | 5572 | ` *  callable (OP_LOAD_FCC screens it, with nothing running in between), or for a call` |
|        - | 5573 | ` *  with no arguments at all — there the call IS the first thing that happens.` |
|        - | 5574 | ` */` |
|   666228 | 5575 | `case PH7_OP_CALL_INIT: {` |
|  1334482 | 5576 | `	if( (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_MAGICCALL)) == 0` |
|  1334487 | 5577 | `	 && !VmValueIsClosure(pVm,pTos) ){` |
|  1333449 | 5578 | `		const char *zInitCls = 0,*zInitMeth = 0;` |
|  1333449 | 5579 | `		sxu32 nInitCls = 0,nInitMeth = 0;` |
|        - | 5580 | `		char zInitMsg[192];` |
|  1333449 | 5581 | `		const char *zInitBad = 0;` |
|  1333449 | 5582 | `		SyString sInitName = { 0, 0 };` |
|  1333449 | 5583 | `		int bInitScoped = 0;` |
|  1333449 | 5584 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|  1333299 | 5585 | `			SyStringInitFromBuf(&sInitName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 5586 | `			/* A leading backslash only anchors the name to the global namespace. */` |
|  1333299 | 5587 | `			if( sInitName.nByte > 0 && sInitName.zString[0] == '\\' ){` |
|        3 | 5588 | `				sInitName.zString++;` |
|        3 | 5589 | `				sInitName.nByte--;` |
|        1 | 5590 | `			}` |
|  1333299 | 5591 | `			bInitScoped = PH7_VmCallableStringParts(sInitName.zString,sInitName.nByte,` |
|        - | 5592 | `				&zInitCls,&nInitCls,&zInitMeth,&nInitMeth);` |
|   667660 | 5593 | `		}` |
|  1333449 | 5594 | `		if( bInitScoped ){` |
|        - | 5595 | ``			/* A `"Class::method"` string carries its whole taxonomy in one builder — the`` |
|        - | 5596 | `			 * class, the missing/abstract/inaccessible cases and the catch-all routing —` |
|        - | 5597 | `			 * and answers 0 when the call WILL run. It is asked unconditionally because` |
|        - | 5598 | `			 * the predicate below is not the same question: is_callable() accepts a` |
|        - | 5599 | `			 * non-static method named through a class, which the direct call refuses. */` |
|       28 | 5600 | `			zInitBad = VmCallableClassMethodError(&(*pVm),` |
|        9 | 5601 | `				PH7_VmExtractClass(&(*pVm),zInitCls,nInitCls,FALSE,0),` |
|        9 | 5602 | `				zInitCls,nInitCls,zInitMeth,nInitMeth,TRUE,zInitMsg,sizeof(zInitMsg));` |
|  1333440 | 5603 | `		}else if( !PH7_VmIsCallable(&(*pVm),pTos,TRUE) ){` |
|       93 | 5604 | `			int bInitOk = 0;` |
|       93 | 5605 | `			if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 5606 | `				/* php's global fallback for an UNQUALIFIED name written inside a` |
|        - | 5607 | `				 * namespace: the current namespace first, the global one after. OP_CALL` |
|        - | 5608 | `				 * retries the same way from its argument map; this only has to agree` |
|        - | 5609 | `				 * about whether the call WILL resolve, so the shortened name is tested` |
|        - | 5610 | `				 * and thrown away. */` |
|       57 | 5611 | `				const char *zInitShort = sInitName.zString;` |
|        - | 5612 | `				sxu32 iInitPos;` |
|      857 | 5613 | `				for( iInitPos = 0 ; iInitPos < sInitName.nByte ; ++iInitPos ){` |
|      805 | 5614 | `					if( sInitName.zString[iInitPos] == '\\' ){` |
|       71 | 5615 | `						zInitShort = &sInitName.zString[iInitPos + 1];` |
|       33 | 5616 | `					}` |
|      405 | 5617 | `				}` |
|       57 | 5618 | `				if( zInitShort != sInitName.zString ){` |
|        - | 5619 | `					ph7_value sInitShort;` |
|       57 | 5620 | `					PH7_MemObjInit(pVm,&sInitShort);` |
|       83 | 5621 | `					PH7_MemObjStringAppend(&sInitShort,zInitShort,` |
|       52 | 5622 | `						(sxu32)(sInitName.nByte - (sxu32)(zInitShort - sInitName.zString)));` |
|       57 | 5623 | `					bInitOk = PH7_VmIsCallable(&(*pVm),&sInitShort,TRUE);` |
|       57 | 5624 | `					PH7_MemObjRelease(&sInitShort);` |
|       26 | 5625 | `				}` |
|       26 | 5626 | `			}` |
|        - | 5627 | `			/* The FIRST-CLASS-CALLABLE creation screen's builder, unchanged and shared:` |
|        - | 5628 | ``			 * php gives `f(...)` the direct call's taxonomy word for word. It assumes the`` |
|        - | 5629 | `			 * predicate has already declined — a pair a class answers through __call is` |
|        - | 5630 | `			 * callable and never arrives here — which is why it sits under that test. */` |
|       93 | 5631 | `			if( !bInitOk ){` |
|       38 | 5632 | `				zInitBad = VmFccValueError(&(*pVm),pTos,zInitMsg,sizeof(zInitMsg));` |
|       18 | 5633 | `			}` |
|       44 | 5634 | `		}` |
|  1333449 | 5635 | `		if( zInitBad ){` |
|        - | 5636 | `			sxi32 rcInit;` |
|       44 | 5637 | `			PH7_MemObjRelease(pTos);` |
|       44 | 5638 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       44 | 5639 | `			pTos->nIdx = SXU32_HIGH;` |
|       44 | 5640 | `			rcInit = VmThrowFromVm(&(*pVm),"Error",zInitBad,(sxu32)SyStrlen(zInitBad));` |
|       44 | 5641 | `			if( rcInit == SXERR_ABORT ){ goto Abort; }` |
|       44 | 5642 | `			rc = rcInit;` |
|       62 | 5643 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5644 | `		}` |
|   667714 | 5645 | `	}` |
|  1334445 | 5646 | `	break;` |
|        - | 5647 | `}` |
|        - | 5648 | `/*` |
|        - | 5649 | ` * OP_ROT_CALLEE P1 P2 *` |
|        - | 5650 | ` *  Turn a call's operand region over: [callee][arg0..argN] becomes [arg0..argN][callee],` |
|        - | 5651 | ` *  which is the layout OP_CALL's entire dispatch is written against.` |
|        - | 5652 | ` *` |
|        - | 5653 | ` *  The codegen pushes the callee FIRST because php resolves it where it is written —` |
|        - | 5654 | ` *  before a single argument runs — so an undefined or inaccessible method is refused` |
|        - | 5655 | `` *  ahead of the argument list's side effects, and a `?->` on null skips the arguments`` |
|        - | 5656 | ` *  altogether. Everything downstream of this instruction still sees the historical` |
|        - | 5657 | ` *  stack, so the reordering costs one memory move per call and nothing else.` |
|        - | 5658 | ` *` |
|        - | 5659 | ` *  P1 is the compile-time argument count; P2 carries PH7_ROT_SPREAD (this call unpacks,` |
|        - | 5660 | ` *  so the runtime count is P1 plus its OWN runs' net growth) and PH7_ROT_TWOSLOT (the` |
|        - | 5661 | ` *  callee is a method pair, [receiver][name]). A __call routing collapses that pair to` |
|        - | 5662 | ` *  one marked carrier at run time, which is read off the slot rather than guessed.` |
|        - | 5663 | ` */` |
|  1169394 | 5664 | `case PH7_OP_ROT_CALLEE: {` |
|  4681633 | 5665 | `	sxi32 nRotArgs = pInstr->iP1` |
|  2340814 | 5666 | `		+ ((pInstr->iP2 & PH7_ROT_SPREAD)` |
|        - | 5667 | `			/* One past the last argument is one past the TOP here: the callee sits` |
|        - | 5668 | `			 * BELOW the region, not above it as at OP_CALL. */` |
|  1169579 | 5669 | `			? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,&pTos[1]) : 0);` |
|  2340819 | 5670 | `	if( nRotArgs < 0 ){` |
|        - | 5671 | `		/* Unreachable: an empty unpack subtracts one per compile-time position, so the` |
|        - | 5672 | `		 * net can reach 0 and no lower. Clamped rather than trusted — reading above the` |
|        - | 5673 | `		 * top to find the callee is not a failure mode worth leaving open. */` |
|      ! 0 | 5674 | `		nRotArgs = 0;` |
|      ! 0 | 5675 | `	}` |
|        - | 5676 | `	{` |
|        - | 5677 | `		ph7_value aCallee[2];` |
|  2340819 | 5678 | `		ph7_value *pTopCallee = &pTos[-nRotArgs];` |
|  2340819 | 5679 | `		sxi32 nCallee = (pInstr->iP2 & PH7_ROT_TWOSLOT) ? 2 : 1;` |
|        - | 5680 | `		ph7_value *pBase;` |
|        - | 5681 | `		sxi32 i;` |
|  2340819 | 5682 | `		if( nCallee > 1 && (pTopCallee->iFlags & MEMOBJ_AUX_MAGICCALL) ){` |
|        - | 5683 | `			/* OP_MEMBER routed a missing/inaccessible name to __call: it consumed the` |
|        - | 5684 | `			 * receiver and left ONE carrier slot, so the pair the compiler counted on` |
|        - | 5685 | `			 * is not there. */` |
|       71 | 5686 | `			nCallee = 1;` |
|       34 | 5687 | `		}` |
|  2340819 | 5688 | `		pBase = pTopCallee - (nCallee - 1);` |
|        - | 5689 | `#ifdef UNTRUST` |
|        - | 5690 | `		if( pBase < pStack ){` |
|        - | 5691 | `			goto Abort;` |
|        - | 5692 | `		}` |
|        - | 5693 | `#endif` |
|  2340819 | 5694 | `		if( nRotArgs > 0 ){` |
|  4685113 | 5695 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  2344321 | 5696 | `				aCallee[i] = pBase[i];` |
|  1173176 | 5697 | `			}` |
|  5756285 | 5698 | `			for( i = 0 ; i < nRotArgs ; ++i ){` |
|  3415493 | 5699 | `				pBase[i] = pBase[i + nCallee];` |
|  1709186 | 5700 | `			}` |
|  4685113 | 5701 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  2344321 | 5702 | `				pBase[nRotArgs + i] = aCallee[i];` |
|  1173176 | 5703 | `			}` |
|  1171409 | 5704 | `		}` |
|  2340819 | 5705 | `		if( pInstr->iP2 & PH7_ROT_SPREAD ){` |
|        - | 5706 | `			/* The argument region now ends nCallee slots lower than it did, so this` |
|        - | 5707 | `			 * call's captured unpack runs — the suffix VmSpreadOwnExtra just assigned` |
|        - | 5708 | `			 * to it, all of them anchored inside the region — move with it, and OP_CALL` |
|        - | 5709 | ``			 * re-derives the same count from them. Unconditional: an `f(...[])` unpack`` |
|        - | 5710 | `			 * moves NO argument (its run is zero-width) and still has to be re-anchored,` |
|        - | 5711 | `			 * or the recount reads it as an ordinary slot and eats one slot too many.` |
|        - | 5712 | `			 * An ENCLOSING call's runs sit below the callee and are left alone. */` |
|      373 | 5713 | `			sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      373 | 5714 | `			VmSpreadRun *aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|        - | 5715 | `			sxu32 r;` |
|      757 | 5716 | `			for( r = pVm->nSpreadCallBase ; r < nRun ; ++r ){` |
|      387 | 5717 | `				aRun[r].pStart -= nCallee;` |
|      195 | 5718 | `			}` |
|      185 | 5719 | `		}` |
|        - | 5720 | `	}` |
|  2340819 | 5721 | `	break;` |
|        - | 5722 | `}` |
|  2015025 | 5723 | `case PH7_OP_CALL: {` |
|        - | 5724 | `	/* iP2 = hasSpread (compile-time). Count only THIS call's own unpack` |
|        - | 5725 | `	 * expansion (VmSpreadOwnExtra, derived from the captured runs on top of the` |
|        - | 5726 | `	 * stack) — an INNER spread-bearing call evaluated inside this argument list` |
|        - | 5727 | ``	 * (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not`` |
|        - | 5728 | `	 * be conflated, which a single shared accumulator could not express. */` |
|  4032084 | 5729 | `	sxi32 nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|        - | 5730 | `	ph7_value *pArg;` |
|        - | 5731 | `	/* Consume the engine's magic-dispatch latch here, at the head of the ONE call` |
|        - | 5732 | `	 * it was set for, whatever path that call then takes. Left standing it would` |
|        - | 5733 | `	 * describe the next call instead. */` |
|  4032084 | 5734 | `	int bMagicDispatch = pVm->bMagicDispatch;` |
|        - | 5735 | `	/* ...and the member resolution's own verdict, which rides the callee SLOT rather` |
|        - | 5736 | `	 * than the VM: an OP_MEMBER that produced this callee already decided its` |
|        - | 5737 | `	 * visibility against the entry it chose, so the screen below must stand down. */` |
|  8005754 | 5738 | `	int bMemberScreened = (pTos->iFlags & MEMOBJ_AUX_MEMBERCALL) != 0` |
|  4032079 | 5739 | `		\|\| pVm->bClosureScreened;` |
|        - | 5740 | `	/* ...and the internal-callback latch, for the same reason: it describes THIS call` |
|        - | 5741 | `	 * (an internal function invoking a userland callback binds its arguments weakly),` |
|        - | 5742 | `	 * and a call the callback body makes must not inherit it. */` |
|  4032084 | 5743 | `	int bCallbackWeak = pVm->bCallbackWeak;` |
|  4032084 | 5744 | `	pVm->bMagicDispatch = 0;` |
|  4032084 | 5745 | `	pVm->bClosureScreened = 0;` |
|  4032084 | 5746 | `	pVm->bCallbackWeak = 0;` |
|  4032084 | 5747 | `	pTos->iFlags &= ~MEMOBJ_AUX_MEMBERCALL;` |
|  4032084 | 5748 | `	pArg = &pTos[-nCallArgs];` |
|        - | 5749 | `	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED` |
|        - | 5750 | `	 * arguments, and a spread expanding to !=1 element shifts the actual positions` |
|        - | 5751 | `	 * of any following compile-time named args. The effective per-actual-slot name` |
|        - | 5752 | `	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)` |
|        - | 5753 | `	 * is built PER PATH against that path's finalized arg base — a method call pops` |
|        - | 5754 | `	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch` |
|        - | 5755 | `	 * site rather than built once here. */` |
|        - | 5756 | `	VmCallArgMap sEffMap;` |
|  4032084 | 5757 | `	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;` |
|        - | 5758 | `	SyHashEntry *pEntry;` |
|        - | 5759 | `	SyString sName;` |
|        - | 5760 | `	/* The host-call trio, hoisted out of the foreign-function branch below so the` |
|        - | 5761 | `	 * NativeCall label there can also be reached from the compiled-function branch:` |
|        - | 5762 | `	 * a VM_FUNC_NATIVE method is a C body wearing a method's clothes, and it runs on` |
|        - | 5763 | `	 * exactly the foreign path (no frame, no call record, no operand stack) once` |
|        - | 5764 | `	 * $this and the called class have been resolved the method way. Jumping into` |
|        - | 5765 | `	 * that branch would otherwise cross these declarations. */` |
|        - | 5766 | `	ph7_user_func *pFunc;` |
|        - | 5767 | `	ph7_context sCtx;` |
|        - | 5768 | `	ph7_value sRet;` |
|        - | 5769 | `	/* Native-METHOD call state; all 0 for a plain host function, which is what the` |
|        - | 5770 | `	 * foreign branch's own fallthrough leaves.` |
|        - | 5771 | `	 *   pNativeOwned — the reference the method path took, released at the shared tail` |
|        - | 5772 | `	 *   pNativeRecv  — what the body actually sees as $this (0 for a static method)` |
|        - | 5773 | `	 *   pNativeClass — the late-static-binding target */` |
|  4032084 | 5774 | `	ph7_class_instance *pNativeOwned = 0;` |
|  4032084 | 5775 | `	ph7_class_instance *pNativeRecv = 0;` |
|  4032084 | 5776 | `	ph7_class *pNativeClass = 0;` |
|        - | 5777 | `	/* The engine's own __call/__callStatic routing: the OP_MEMBER immediately below this` |
|        - | 5778 | `	 * call found a missing (or inaccessible) method on a class declaring the magic handler` |
|        - | 5779 | `	 * and MARKED this callee slot, latching {receiver, class, original name} on the VM.` |
|        - | 5780 | `	 * There is no callable here at all — the mark selects the packing body directly, ahead` |
|        - | 5781 | `	 * of every callable decode below, and the record it dispatches carries no PHP name (it` |
|        - | 5782 | `	 * is not in hHostFunction). This is what replaced writing the string` |
|        - | 5783 | `	 * "__phl_magic_call" into the slot and letting the name lookup find a hidden global.` |
|        - | 5784 | ``	 * Everything from `NativeCall` down is shared with an ordinary builtin call, which is`` |
|        - | 5785 | `	 * what this has always been from the executor's point of view. */` |
|  4032084 | 5786 | `	if( pTos->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|        - | 5787 | `		/* Move the routing off the carrier and onto the VM, HERE — one instruction` |
|        - | 5788 | `		 * before the packing body reads it, with nothing in between that could set` |
|        - | 5789 | `		 * another. OP_MEMBER used to publish it directly, which only held while the` |
|        - | 5790 | `		 * arguments ran before it; now they run after, and a routed call inside this` |
|        - | 5791 | `		 * one's argument list has already come and gone. */` |
|      125 | 5792 | `		VmMagicCall *pPend = (VmMagicCall *)pTos->x.pOther;` |
|      125 | 5793 | `		pTos->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|      125 | 5794 | `		pTos->x.pOther = 0;` |
|      125 | 5795 | `		pVm->pMagicCallThis = pPend ? pPend->pRecv : 0;` |
|      125 | 5796 | `		pVm->pMagicCallClass = pPend ? pPend->pClass : 0;` |
|      125 | 5797 | `		SyBlobReset(&pVm->sMagicCallName);` |
|      125 | 5798 | `		if( pPend && SyBlobLength(&pPend->sName) > 0 ){` |
|      186 | 5799 | `			SyBlobAppend(&pVm->sMagicCallName,SyBlobData(&pPend->sName),` |
|       61 | 5800 | `				SyBlobLength(&pPend->sName));` |
|       61 | 5801 | `		}` |
|      125 | 5802 | `		if( pPend ){` |
|        - | 5803 | `			/* The receiver reference the record held is now the VM's, which` |
|        - | 5804 | `			 * VmMagicCallDispatch gives back — so drop the record without unref'ing. */` |
|      125 | 5805 | `			pPend->pRecv = 0;` |
|      125 | 5806 | `			VmFreeMagicCall(pPend);` |
|       61 | 5807 | `		}` |
|      125 | 5808 | `		pFunc = PH7_VmMagicCallFunc(&(*pVm));` |
|      125 | 5809 | `		if( pFunc == 0 ){` |
|      ! 0 | 5810 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 5811 | `			goto Abort;` |
|        - | 5812 | `		}` |
|        - | 5813 | `		/* D1: the packing body declares no by-ref parameter (php hands __call a packed` |
|        - | 5814 | `		 * ARRAY), so every deferred argument materializes by value, exactly as it did` |
|        - | 5815 | `		 * through the named trampoline's zero by-ref mask. */` |
|        - | 5816 | `		{` |
|      125 | 5817 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffCallMap);` |
|      125 | 5818 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 5819 | `		}` |
|      186 | 5820 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|      122 | 5821 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|      125 | 5822 | `		goto NativeCall;` |
|        - | 5823 | `	}` |
|        - | 5824 | `	/* A Closure object is callable: unwrap it to its underlying string callable so the` |
|        - | 5825 | `	 * dispatch below handles it (rather than treating it as a generic object and looking` |
|        - | 5826 | `	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure` |
|        - | 5827 | `	 * so a plain __invoke object skips the temp-value work entirely. */` |
|  4031962 | 5828 | `	if( VmValueIsClosure(pVm,pTos) ){` |
|        - | 5829 | `		ph7_value sCallable;` |
|     6681 | 5830 | `		PH7_MemObjInit(pVm,&sCallable);` |
|     6681 | 5831 | `		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){` |
|     6681 | 5832 | `			PH7_MemObjRelease(pTos);` |
|     6681 | 5833 | `			PH7_MemObjStore(&sCallable,pTos);` |
|     3338 | 5834 | `		}` |
|     6681 | 5835 | `		PH7_MemObjRelease(&sCallable);` |
|     3338 | 5836 | `	}` |
|        - | 5837 | `	/* Extract function name */` |
|  4031962 | 5838 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|   300431 | 5839 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 5840 | `			ph7_value sResult;` |
|        - | 5841 | `			sxi32 rcArr;` |
|        - | 5842 | `			/* Taken off the VM at the head of the shape check below, not at the dispatch:` |
|        - | 5843 | `			 * everything between the two (the deferred-argument materialization especially)` |
|        - | 5844 | `			 * can throw and jump out of this branch, and a latch left armed would stand the` |
|        - | 5845 | `			 * visibility screen down for whatever call runs next. */` |
|        - | 5846 | `			int bCbScreened;` |
|        - | 5847 | `			{` |
|        - | 5848 | `				/* php validates the SHAPE of an array callable first: it must hold exactly` |
|        - | 5849 | `				 * two elements. PH7 handed any array to the dispatcher, which failed` |
|        - | 5850 | ``				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */`` |
|   100327 | 5851 | `				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - | 5852 | `				char zCbMsg[192];` |
|   100327 | 5853 | `				const char *zCbErr = 0;` |
|   100327 | 5854 | `				int bCbRaised = 0; /* the class lookup ran an autoloader that threw */` |
|        - | 5855 | `				/* A pair the closure UNWRAP just built is not an array the program wrote: its` |
|        - | 5856 | `				 * callee was resolved and screened where the closure was BUILT, the way php` |
|        - | 5857 | `				 * resolves one, and it is a well-formed [target, method] by construction.` |
|        - | 5858 | `				 * Re-deciding it here, against the CALLER, is what refused an escaped` |
|        - | 5859 | ``				 * `$this->priv(...)` php runs. */`` |
|   100327 | 5860 | `				bCbScreened = pVm->bClosureScreened;` |
|   100327 | 5861 | `				pVm->bClosureScreened = 0; /* put back for the one dispatch that reads it */` |
|   100327 | 5862 | `				if( !bCbScreened && pCbMap && pCbMap->nEntry == 2 ){` |
|        - | 5863 | `					/* Shape is right; now check it actually RESOLVES. The shared dispatcher` |
|        - | 5864 | `					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for` |
|        - | 5865 | `					 * an unresolvable [class,method] pair -- silence a caller cannot detect --` |
|        - | 5866 | `					 * and its contract is relied on by call_user_func/usort, so the throw` |
|        - | 5867 | `					 * belongs here at the call site. */` |
|   100188 | 5868 | `					ph7_value *pCbCls = 0;` |
|   100188 | 5869 | `					ph7_value *pCbMeth = 0;` |
|        - | 5870 | `					/* php reads the INTEGER indices 0 and 1, not the first two entries in` |
|        - | 5871 | `					 * insertion order. Two entries at other keys are a SHAPE error of their` |
|        - | 5872 | `					 * own ("has to contain indices 0 and 1"), distinct from the` |
|        - | 5873 | ``					 * wrong-element-COUNT message below; `[1=>'m',0=>'C']` holds both, so`` |
|        - | 5874 | `					 * the target is index 0 -- the reverse of what PH7 walked. */` |
|   100188 | 5875 | `					if( !PH7_VmArrayCallableParts(&(*pVm),pCbMap,&pCbCls,&pCbMeth) ){` |
|       11 | 5876 | `						zCbErr = "Array callback has to contain indices 0 and 1";` |
|        6 | 5877 | `					}else{` |
|        - | 5878 | `						/* The resolution below can run an autoloader that throws; when it does,` |
|        - | 5879 | `						 * php propagates THAT exception and never reports the class missing. */` |
|   100178 | 5880 | `						sxi32 nCbBrc = pVm->nBoundaryRc;` |
|   100178 | 5881 | `						const void *pCbRes = (const void *)pVm->pResumeFrame;` |
|   150265 | 5882 | `						zCbErr = VmDirectArrayCallableError(&(*pVm),pCbCls,pCbMeth,` |
|    50087 | 5883 | `							zCbMsg,sizeof(zCbMsg));` |
|   100178 | 5884 | `						if( zCbErr && PH7_VmClassLookupRaised(&(*pVm),nCbBrc,pCbRes) ){` |
|        6 | 5885 | `							bCbRaised = 1;` |
|        2 | 5886 | `						}` |
|        - | 5887 | `					}` |
|    50092 | 5888 | `				}` |
|   100327 | 5889 | `				if( !bCbScreened && (pCbMap == 0 \|\| pCbMap->nEntry != 2 \|\| zCbErr) ){` |
|        - | 5890 | `					sxi32 rcCb;` |
|       87 | 5891 | `					if( pInstr->iP2 ){` |
|      ! 0 | 5892 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 5893 | `					}` |
|       87 | 5894 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 5895 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 5896 | `					}` |
|       87 | 5897 | `					PH7_MemObjRelease(pTos);` |
|       87 | 5898 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       87 | 5899 | `					pTos->nIdx = SXU32_HIGH;` |
|       87 | 5900 | `					if( bCbRaised ){` |
|        - | 5901 | `						/* Land the autoloader's own throw: consume the parked status (an` |
|        - | 5902 | `						 * in-place catch leaves 0 behind plus a recorded resume frame, which` |
|        - | 5903 | `						 * the router below picks up). */` |
|        6 | 5904 | `						rcCb = pVm->nBoundaryRc;` |
|        6 | 5905 | `						pVm->nBoundaryRc = 0;` |
|        6 | 5906 | `						if( rcCb == PH7_ABORT ){ goto Abort; }` |
|        6 | 5907 | `						rc = PH7_EXCEPTION;` |
|       18 | 5908 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5909 | `					}` |
|       82 | 5910 | `					if( zCbErr == 0 ){` |
|       17 | 5911 | `						zCbErr = "Array callback must have exactly two elements";` |
|        8 | 5912 | `					}` |
|       82 | 5913 | `					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));` |
|       82 | 5914 | `					if( rcCb == SXERR_ABORT ){ goto Abort; }` |
|       82 | 5915 | `					rc = rcCb;` |
|        - | 5916 | `					/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 5917 | `					 * (SXRET_OK + recorded resume) FELL THROUGH and kept dispatching` |
|        - | 5918 | `					 * the malformed callable inside the try. Route like OP_THROW. */` |
|      106 | 5919 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5920 | `				}` |
|        - | 5921 | `			}` |
|        - | 5922 | `			/* Build the effective spread-key map (and consume this call's runs)` |
|        - | 5923 | `			 * against this path's arg base (the array-callable slot isn't popped). */` |
|   150361 | 5924 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100238 | 5925 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 5926 | `			/* Materialize the deferred arguments against the pair's own method (see` |
|        - | 5927 | `			 * VmIndirectCalleeFunc), not against a blanket by-ref assumption. */` |
|        - | 5928 | `			{` |
|   100242 | 5929 | `				sxi32 rcDA = VmResolveIndirectArgs(&(*pVm),pTos,pArg,pTos,pEffCallMap);` |
|   100242 | 5930 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 5931 | `			}` |
|   100242 | 5932 | `			SySetReset(&aArg);` |
|   100416 | 5933 | `			while( pArg < pTos ){` |
|      176 | 5934 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      176 | 5935 | `				pArg++;` |
|        2 | 5936 | `			}` |
|   100242 | 5937 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 5938 | `			/* May be a class instance and it's static method. Forward this call's named-arg map` |
|        - | 5939 | ``			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —`` |
|        - | 5940 | `			 * mirroring the __invoke-object branch below. */` |
|   100242 | 5941 | `			pVm->bClosureScreened = bCbScreened; /* see the capture above */` |
|   100242 | 5942 | `			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|        - | 5943 | `			/* The latch is consumed by the method OP_CALL this dispatch builds; clear it here` |
|        - | 5944 | `			 * for the paths that never reach one. */` |
|   100242 | 5945 | `			pVm->bClosureScreened = 0;` |
|   100242 | 5946 | `			SySetReset(&aArg);` |
|        - | 5947 | `			/* Pop given arguments */` |
|   100242 | 5948 | `			if( nCallArgs > 0 ){` |
|      150 | 5949 | `				VmPopOperand(&pTos,nCallArgs);` |
|       74 | 5950 | `			}` |
|   100242 | 5951 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 | 5952 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 5953 | `				goto Abort;` |
|        - | 5954 | `			}` |
|   100242 | 5955 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - | 5956 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - | 5957 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - | 5958 | `				sxi32 iResumePc;` |
|   100014 | 5959 | `				PH7_MemObjRelease(&sResult);` |
|   100014 | 5960 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100004 | 5961 | `					PH7_MemObjRelease(pTos);` |
|        - | 5962 | ``					/* Drain the abandoned outer-expression operands (`1 + $cb()`)`` |
|        - | 5963 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 5964 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300006 | 5965 | `					PH7_RESUME_DRAIN()` |
|   100004 | 5966 | `					pc = iResumePc;` |
|   100004 | 5967 | `					break;` |
|        - | 5968 | `				}` |
|       11 | 5969 | `				goto Exception;` |
|        - | 5970 | `			}` |
|        - | 5971 | `			/* Copy result */` |
|      229 | 5972 | `			PH7_MemObjStore(&sResult,pTos);` |
|      229 | 5973 | `			PH7_MemObjRelease(&sResult);` |
|   200222 | 5974 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|   200093 | 5975 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - | 5976 | `			ph7_value sResult;` |
|        - | 5977 | `			sxi32 rcInv;` |
|        - | 5978 | `			/* __invoke object callable: the object slot isn't popped, so pArg is` |
|        - | 5979 | `			 * already this call's arg base — build the map + consume the runs. */` |
|   300137 | 5980 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   200088 | 5981 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 5982 | `			/* Materialize the deferred arguments against this object's __invoke, the` |
|        - | 5983 | `			 * array-callable path's rule one shape over. */` |
|        - | 5984 | `			{` |
|   200093 | 5985 | `				sxi32 rcDA = VmResolveIndirectArgs(&(*pVm),pTos,pArg,pTos,pEffCallMap);` |
|   200093 | 5986 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 5987 | `			}` |
|   200093 | 5988 | `			SySetReset(&aArg);` |
|   200187 | 5989 | `			while( pArg < pTos ){` |
|       98 | 5990 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       98 | 5991 | `				pArg++;` |
|        4 | 5992 | `			}` |
|   200093 | 5993 | `			PH7_MemObjInit(pVm,&sResult);` |
|   300137 | 5994 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|   200088 | 5995 | `				(int)SySetUsed(&aArg),` |
|   200088 | 5996 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - | 5997 | `				&sResult,` |
|   100044 | 5998 | `				pEffCallMap);` |
|   200093 | 5999 | `			SySetReset(&aArg);` |
|        - | 6000 | `			/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable` |
|        - | 6001 | `			 * slot itself (it pops top-down and the callable IS pTos), which for a` |
|        - | 6002 | `			 * temporary like (new Plain())(...) holds the only reference — popping it` |
|        - | 6003 | `			 * would free pThis before VmRaiseNotCallable reads its class name below.` |
|        - | 6004 | `			 * Only the not-callable branch dereferences pThis afterwards, so pin just` |
|        - | 6005 | `			 * for that case; the matching PH7_ClassInstanceUnref drops it (destroying` |
|        - | 6006 | `			 * the temporary). The other branches let the pop free the temp as before. */` |
|   200093 | 6007 | `			if( rcInv == SXERR_INVALID ){` |
|   100006 | 6008 | `				pThis->iRef++;` |
|    50002 | 6009 | `			}` |
|   200093 | 6010 | `			if( nCallArgs > 0 ){` |
|       78 | 6011 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 | 6012 | `			}` |
|   200093 | 6013 | `			if( rcInv == SXERR_INVALID ){` |
|        - | 6014 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - | 6015 | `				 * sResult was already released by VmCallObjectInvoke. */` |
|   100006 | 6016 | `				PH7_MemObjRelease(pTos);` |
|   100006 | 6017 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|   100006 | 6018 | `				PH7_ClassInstanceUnref(pThis);` |
|   100006 | 6019 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 6020 | `					goto Abort;` |
|        - | 6021 | `				}` |
|        - | 6022 | `				{` |
|        - | 6023 | `					sxi32 iRp;` |
|   100006 | 6024 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|        - | 6025 | `						/* Drain the abandoned outer-expression operands` |
|        - | 6026 | ``						 * (`1 + $plainObj()`) to the try's base — one leaked`` |
|        - | 6027 | `						 * slot per caught throw otherwise. */` |
|   300010 | 6028 | `						PH7_RESUME_DRAIN()` |
|   100006 | 6029 | `						pc = iRp;` |
|   100006 | 6030 | `						break;` |
|        - | 6031 | `					}` |
|        - | 6032 | `				}` |
|      ! 0 | 6033 | `				goto Exception;` |
|        - | 6034 | `			}` |
|   100089 | 6035 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 | 6036 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 6037 | `				goto Abort;` |
|        - | 6038 | `			}` |
|   100089 | 6039 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - | 6040 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - | 6041 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - | 6042 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - | 6043 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - | 6044 | `				sxi32 iResumePc;` |
|   100008 | 6045 | `				PH7_MemObjRelease(&sResult);` |
|   100008 | 6046 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100006 | 6047 | `					PH7_MemObjRelease(pTos);` |
|        - | 6048 | ``					/* Drain the abandoned outer-expression operands (`1 + $inv()`)`` |
|        - | 6049 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 6050 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300010 | 6051 | `					PH7_RESUME_DRAIN()` |
|   100006 | 6052 | `					pc = iResumePc;` |
|   100006 | 6053 | `					break;` |
|        - | 6054 | `				}` |
|        3 | 6055 | `				goto Exception;` |
|        - | 6056 | `			}` |
|       82 | 6057 | `			PH7_MemObjStore(&sResult,pTos);` |
|       82 | 6058 | `			PH7_MemObjRelease(&sResult);` |
|       43 | 6059 | `		}else{` |
|        - | 6060 | `			/* php: calling a non-callable is a catchable Error naming the type` |
|        - | 6061 | `			 * ("Value of type int is not callable"), or -- for an array -- the shape it` |
|        - | 6062 | `			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so` |
|        - | 6063 | ``			 * `$x()` on a number quietly evaluated to nothing. */`` |
|        - | 6064 | `			sxi32 rcNc;` |
|        - | 6065 | `			char zMsg[128];` |
|       17 | 6066 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 6067 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");` |
|      ! 0 | 6068 | `			}else{` |
|       25 | 6069 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",` |
|        8 | 6070 | `					VmArithTypeName(pTos));` |
|        - | 6071 | `			}` |
|        - | 6072 | `			/* Consume this call's captured spread runs — a non-callable target` |
|        - | 6073 | `			 * (int/float/bool/null) reaches no dispatch build site. */` |
|       17 | 6074 | `			if( pInstr->iP2 ){` |
|      ! 0 | 6075 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 6076 | `			}` |
|        - | 6077 | `			/* Pop given arguments */` |
|       17 | 6078 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 6079 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6080 | `			}` |
|        - | 6081 | `			/* Settle the call's result slot BEFORE throwing. */` |
|       17 | 6082 | `			PH7_MemObjRelease(pTos);` |
|       17 | 6083 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       17 | 6084 | `			pTos->nIdx = SXU32_HIGH;` |
|       17 | 6085 | `			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       17 | 6086 | `			if( rcNc == SXERR_ABORT ){ goto Abort; }` |
|       17 | 6087 | `			rc = rcNc;` |
|        - | 6088 | `			/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 6089 | `			 * (SXRET_OK + recorded resume) FELL THROUGH and resumed the try body` |
|        - | 6090 | `			 * right after the failed call. Route like OP_THROW. */` |
|       31 | 6091 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6092 | `		}` |
|      308 | 6093 | `		break;` |
|        - | 6094 | `	}` |
|  3731536 | 6095 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 6096 | `	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just` |
|        - | 6097 | `	 * anchors it to the global namespace — strip it before resolving so a` |
|        - | 6098 | ``	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */`` |
|  3731536 | 6099 | `	if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|       15 | 6100 | `		sName.zString++;` |
|       15 | 6101 | `		sName.nByte--;` |
|        7 | 6102 | `	}` |
|        - | 6103 | `	/* Check for a compiled function first.` |
|        - | 6104 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 6105 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|  3731536 | 6106 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 6107 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - | 6108 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - | 6109 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - | 6110 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - | 6111 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - | 6112 | `	{` |
|  3731536 | 6113 | `	VmCallArgMap *pCallMap = pEffCallMap;` |
|  3731536 | 6114 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - | 6115 | `		const char *zFunc;` |
|        - | 6116 | `		const char *zEnd;` |
|        - | 6117 | `		const char *z;` |
|        - | 6118 | `		SyString sGlobal;` |
|       57 | 6119 | `		zFunc = sName.zString;` |
|       57 | 6120 | `		zEnd  = zFunc + sName.nByte;` |
|       57 | 6121 | `		z = zEnd;` |
|        - | 6122 | `		/* Find last namespace separator */` |
|      529 | 6123 | `		while( z > zFunc ){` |
|      529 | 6124 | `			if( z[-1] == '\\' ){` |
|       57 | 6125 | `				break;` |
|        - | 6126 | `			}` |
|      477 | 6127 | `			z--;` |
|        5 | 6128 | `		}` |
|       57 | 6129 | `		if( z > zFunc && z < zEnd ){` |
|        - | 6130 | `			/* Retry lookup using the unqualified/global function name */` |
|       57 | 6131 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       57 | 6132 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       26 | 6133 | `		}` |
|       26 | 6134 | `	}` |
|        - | 6135 | `	} /* end VmCallArgMap namespace scope */` |
|  3731536 | 6136 | `	if( pEntry ){` |
|        - | 6137 | `		ph7_vm_func_arg *aFormalArg;` |
|        - | 6138 | `		ph7_class_instance *pThis;` |
|        - | 6139 | `		ph7_value *pFrameStack;` |
|        - | 6140 | `		ph7_vm_func *pVmFunc;` |
|        - | 6141 | `		ph7_class *pSelf;` |
|        - | 6142 | `		ph7_class *pSelfHint;` |
|        - | 6143 | `		VmFrame *pFrame;` |
|        - | 6144 | `		ph7_value *pObj;` |
|        - | 6145 | `		VmSlot sArg;` |
|        - | 6146 | `		sxu32 n;` |
|  2238908 | 6147 | `		sxi32 iArgPreFlags = 0; /* the actual's type before its declared-type check */` |
|  2238908 | 6148 | `		int bClosureThis = 0;` |
|  2238908 | 6149 | `		ph7_class *pClosureScope = 0;` |
|        - | 6150 | `		/* initialize fields */` |
|  2238908 | 6151 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|  2238908 | 6152 | `		pThis = 0;` |
|  2238908 | 6153 | `		pSelf = 0;` |
|        - | 6154 | `		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it` |
|        - | 6155 | `		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever` |
|        - | 6156 | `		 * set for a bound plain closure, which dispatches as a function, so the method branch below` |
|        - | 6157 | `		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */` |
|  2238908 | 6158 | `		if( pVm->pClosureThis ){` |
|       65 | 6159 | `			pThis = pVm->pClosureThis;` |
|       65 | 6160 | `			pVm->pClosureThis = 0;` |
|       65 | 6161 | `			bClosureThis = 1;` |
|       31 | 6162 | `		}` |
|  2238908 | 6163 | `		if( pVm->pClosureScope ){` |
|        - | 6164 | `			/* May ride alongside a bound $this, or stand alone for a` |
|        - | 6165 | ``			 * scope-only rebind (`bindTo(null, Scope::class)`). */`` |
|       55 | 6166 | `			pClosureScope = pVm->pClosureScope;` |
|       55 | 6167 | `			pVm->pClosureScope = 0;` |
|       26 | 6168 | `		}` |
|  2238908 | 6169 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - | 6170 | `			ph7_class_method *pMeth;` |
|        - | 6171 | `			/* Class method call */` |
|  1994934 | 6172 | `			ph7_value *pTarget = &pTos[-1];` |
|  1994934 | 6173 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - | 6174 | `				/* Extract the 'this' pointer */` |
|  1994934 | 6175 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - | 6176 | `					/* Instance already loaded */` |
|  1893342 | 6177 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|  1893342 | 6178 | `					pThis->iRef++;` |
|  1893342 | 6179 | `					pSelf = pThis->pClass;` |
|   946670 | 6180 | `				}` |
|  1994934 | 6181 | `				if( pSelf == 0 ){` |
|   101597 | 6182 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - | 6183 | `						/* "Late Static Binding" class name */` |
|   152384 | 6184 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|    50793 | 6185 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|    50793 | 6186 | `					}` |
|   101597 | 6187 | `					if( pSelf == 0 ){` |
|        7 | 6188 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 | 6189 | `					}` |
|    50796 | 6190 | `				}` |
|  1994934 | 6191 | `				if( pThis == 0  ){` |
|   101597 | 6192 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|   101597 | 6193 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   101597 | 6194 | `					if( pFrameLocal->pParent ){` |
|        - | 6195 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|      803 | 6196 | `						pThis = pFrameLocal->pThis;` |
|      803 | 6197 | `						if( pThis ){` |
|      149 | 6198 | `							pThis->iRef++;` |
|       73 | 6199 | `						}` |
|      399 | 6200 | `					}` |
|    50796 | 6201 | `				}` |
|  1994934 | 6202 | `				VmPopOperand(&pTos,1);` |
|  1994934 | 6203 | `				PH7_MemObjRelease(pTos);` |
|        - | 6204 | `				/* Synchronize pointers. The method-name slot popped above sat BETWEEN` |
|        - | 6205 | `				 * this call's arguments and the (already-removed) target — so only now` |
|        - | 6206 | `				 * is pTos one past the last actual argument. Re-derive the unpack` |
|        - | 6207 | `				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs` |
|        - | 6208 | `				 * from run ends, which the extra target slot would otherwise offset,` |
|        - | 6209 | ``				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */`` |
|  1994934 | 6210 | `				nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);` |
|  1994934 | 6211 | `				pArg = &pTos[-nCallArgs];` |
|        - | 6212 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - | 6213 | `				 * user have already computed the random generated unique class method name` |
|        - | 6214 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - | 6215 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - | 6216 | `				 */` |
|  1994934 | 6217 | `				while( pArg < pStack ){` |
|      ! 0 | 6218 | `					pArg++;` |
|      ! 0 | 6219 | `				}` |
|  1994934 | 6220 | `				if( pSelf && pVm->bReflectBypass ){` |
|        - | 6221 | `					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection` |
|        - | 6222 | `					 * ignores visibility. Consume-once so nested calls made by the` |
|        - | 6223 | `					 * invoked body are checked normally. */` |
|      219 | 6224 | `					pVm->bReflectBypass = 0;` |
|      110 | 6225 | `				}else` |
|  1994716 | 6226 | `				if( pSelf && bMagicDispatch && PH7_MagicMethodMustBePublic(&pVmFunc->sName) ){` |
|        - | 6227 | `					/* The ENGINE reaching for a magic method php requires to be public.` |
|        - | 6228 | `					 * php only WARNS at such a declaration and then dispatches the method` |
|        - | 6229 | ``					 * regardless -- the engine calling `__get` is not the outside world`` |
|        - | 6230 | `					 * reaching for a private member -- so a non-public` |
|        - | 6231 | ``					 * `__get`/`__call`/`__invoke`/`__sleep`/... still runs, where PHL threw`` |
|        - | 6232 | ``					 * `Call to private method C::__get()` at the ACCESS and killed the`` |
|        - | 6233 | `					 * script.` |
|        - | 6234 | `					 *` |
|        - | 6235 | `					 * The latch is set by PH7_VmCallMagicMethod at the engine's own` |
|        - | 6236 | `					 * dispatch sites. It is NOT inferred from the instruction: a` |
|        - | 6237 | ``					 * first-class `$o->__get(...)` and `$o->__get('x')` reach the same C`` |
|        - | 6238 | `					 * dispatcher, and php denies both. */` |
|    50572 | 6239 | `				}else` |
|  1893582 | 6240 | `				if( pSelf && !bMemberScreened ){ /* Paranoid edition */` |
|        - | 6241 | `					/* Check if the call is allowed. php binds non-public method` |
|        - | 6242 | `					 * access by the DECLARING class (pVmFunc->pUserData — the class` |
|        - | 6243 | `					 * the callee was compiled in), NOT the instance's class: an` |
|        - | 6244 | `					 * inherited base method calling $this->priv() on a child` |
|        - | 6245 | `					 * instance passes, a child's private SHADOW doesn't hijack the` |
|        - | 6246 | `					 * check for a parent callee, and the denial message names the` |
|        - | 6247 | `					 * declaring class like php. */` |
|  1776625 | 6248 | `					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|        - | 6249 | `					ph7_class *pOwnerClass;` |
|  1776625 | 6250 | `					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|  1776625 | 6251 | `					if( pMeth == 0 && pDeclClass != pSelf ){` |
|      ! 0 | 6252 | `						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      ! 0 | 6253 | `					}` |
|  1776625 | 6254 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        - | 6255 | `						/* ...except that a TRAIT is not a class php still has at run time: it` |
|        - | 6256 | `						 * composed the method INTO the using class, so that class owns the` |
|        - | 6257 | `						 * rule and the name. Deciding against the trait refused a protected` |
|        - | 6258 | `						 * trait method to a SUBCLASS of the composing class (which uses no` |
|        - | 6259 | ``						 * trait of its own) — `class Az { use Tz; } class Bz extends Az {`` |
|        - | 6260 | ``						 * $this->pr(); }` was a fatal php runs. Identity for every non-trait`` |
|        - | 6261 | `						 * method. */` |
|       27 | 6262 | `						pOwnerClass = PH7_VmMethodScopeName(&(*pVm),pSelf,pMeth);` |
|       27 | 6263 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - | 6264 | `							/* php throws a CATCHABLE Error here. The old code merely PRINTED an` |
|        - | 6265 | ``							 * uncaught-exception report and aborted, so `try { $o->priv(); }`` |
|        - | 6266 | ``							 * catch (Error $e)` never caught it and the script died. */`` |
|        - | 6267 | `							char zMsg[256];` |
|        - | 6268 | `							sxi32 rcVis;` |
|        - | 6269 | `							/* php NAMES the calling scope when there is one — "from scope C" —` |
|        - | 6270 | `							 * and says "global scope" only outside every class; the wording is` |
|        - | 6271 | `							 * shared with the first-class-callable screen` |
|        - | 6272 | `							 * (VmMethodVisibilityMsg). */` |
|       19 | 6273 | `							VmMethodVisibilityMsg(&(*pVm),pOwnerClass,` |
|        6 | 6274 | `								pVmFunc->sName.zString,pVmFunc->sName.nByte,` |
|        6 | 6275 | `								pMeth->iProtection,zMsg,sizeof(zMsg));` |
|        - | 6276 | `							/* Consume this call's captured spread runs — this visibility` |
|        - | 6277 | `							 * error exits before the pVmFunc build below. */` |
|       13 | 6278 | `							if( pInstr->iP2 ){` |
|      ! 0 | 6279 | `								VmSpreadConsume(pVm);` |
|      ! 0 | 6280 | `							}` |
|        - | 6281 | `							/* Pop given arguments, and leave the call's NULL result behind. */` |
|       13 | 6282 | `							if( nCallArgs > 0 ){` |
|      ! 0 | 6283 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6284 | `							}` |
|       13 | 6285 | `							PH7_MemObjRelease(pTos);` |
|       13 | 6286 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|       13 | 6287 | `							pTos->nIdx = SXU32_HIGH;` |
|       13 | 6288 | `							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       13 | 6289 | `							if( rcVis == SXERR_ABORT ){ goto Abort; }` |
|       13 | 6290 | `							rc = rcVis;` |
|        - | 6291 | `							/* ENFORCE_RC never consulted pResumeFrame, so an in-place` |
|        - | 6292 | `							 * catch (SXRET_OK + recorded resume) FELL THROUGH and` |
|        - | 6293 | `							 * dispatched the DENIED method anyway. Route like OP_THROW. */` |
|       13 | 6294 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6295 | `						}` |
|        7 | 6296 | `					}` |
|   888304 | 6297 | `				}` |
|   997460 | 6298 | `			}` |
|   997460 | 6299 | `		}` |
|        - | 6300 | `		/* pArg is now finalized for every pVmFunc callee (a method call popped its` |
|        - | 6301 | `		 * method-name slot above; functions/closures/generators keep the top base).` |
|        - | 6302 | `		 * Build the PHP 8.1 effective spread-key map here so the generator and the` |
|        - | 6303 | `		 * install path below both see it — and so this call's captured runs are` |
|        - | 6304 | `		 * consumed exactly once, against the correct base. */` |
|  3358555 | 6305 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  2238891 | 6306 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6307 | `		/* Check the PHP call-depth cap (the sole site — BYTECODE.md stage 5).` |
|        - | 6308 | `		 * Default is unbounded (heap-bound recursion, decoupled from the C stack` |
|        - | 6309 | `		 * by the stage-2 trampoline); the C stack is guarded separately by` |
|        - | 6310 | `		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and` |
|        - | 6311 | `		 * then raises a clean non-catchable fatal (was: silently set NULL and` |
|        - | 6312 | `		 * continue) and halts. */` |
|  2238896 | 6313 | `		if( VmRecursionExceeded(pVm) ){` |
|        - | 6314 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - | 6315 | `			 * which walks the whole operand stack — don't release them here. */` |
|        3 | 6316 | `			VmRecursionFatal(&(*pVm));` |
|        3 | 6317 | `			goto Abort;` |
|        - | 6318 | `		}` |
|  2238894 | 6319 | `		if( pVmFunc->pNextName ){` |
|        - | 6320 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      288 | 6321 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|      142 | 6322 | `		}` |
|        - | 6323 | ``		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's`` |
|        - | 6324 | ``		 * `Class::method`). Computed after overload resolution so it reflects the selected method.`` |
|        - | 6325 | ``		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed`` |
|        - | 6326 | `		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method` |
|        - | 6327 | ``		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the`` |
|        - | 6328 | `		 * USING class; we don't carry the using class on the shared struct, so fall back to the` |
|        - | 6329 | `		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for` |
|        - | 6330 | `		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */` |
|  2238894 | 6331 | `		pSelfHint = pSelf;` |
|  2238894 | 6332 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|  1994922 | 6333 | `			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;` |
|  1994922 | 6334 | `			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|  1994616 | 6335 | `				pSelfHint = pDecl;` |
|   997307 | 6336 | `			}` |
|   997460 | 6337 | `		}` |
|  2238894 | 6338 | `		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 6339 | `			/* Push the closure's called-class as this frame's LSB class so` |
|        - | 6340 | ``			 * `static::` inside the body resolves like php. An explicit`` |
|        - | 6341 | `			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use` |
|        - | 6342 | `			 * the class captured at the closure's creation site. self::/parent::` |
|        - | 6343 | `			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done` |
|        - | 6344 | ``			 * after pSelfHint so a `self`-typed param is unaffected. */`` |
|     8503 | 6345 | `			if( pClosureScope ){` |
|       55 | 6346 | `				pSelf = pClosureScope;` |
|     8477 | 6347 | `			}else if( pVmFunc->pLsbClass ){` |
|      133 | 6348 | `				pSelf = (ph7_class *)pVmFunc->pLsbClass;` |
|       64 | 6349 | `			}` |
|     4249 | 6350 | `		}` |
|  2238894 | 6351 | `		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){` |
|        - | 6352 | `			/* php 8.4 #[\Deprecated] runtime notice — once per call, before` |
|        - | 6353 | `			 * execution (generators: at the g(...) call site, like php). */` |
|      284 | 6354 | `			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,` |
|      186 | 6355 | `				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);` |
|       93 | 6356 | `		}` |
|        - | 6357 | `		/* D1: resolve deferred plain-var arguments against this callee's declaration` |
|        - | 6358 | `		 * now — BEFORE the native/generator splits and VmEnterFrame, while pVm->pFrame is` |
|        - | 6359 | `		 * still the caller. pVmFunc is final here (post-overload). Covers plain functions,` |
|        - | 6360 | `		 * methods, closures, dynamic-name calls, generators and native methods uniformly;` |
|        - | 6361 | `		 * only the SOURCE of the by-ref positions differs between the last one and the` |
|        - | 6362 | `		 * rest (a signature string vs. compiled formal parameters). */` |
|        - | 6363 | `		{` |
|        - | 6364 | `			sxi32 rcDA;` |
|  2238894 | 6365 | `			if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 6366 | `				/* A native method declares no formal parameters to match against —` |
|        - | 6367 | `				 * its by-ref positions come from the same signature-derived mask a` |
|        - | 6368 | `				 * builtin uses, so resolve them the builtin way. Sharing this one` |
|        - | 6369 | `				 * site (rather than repeating it in the branch below) keeps the` |
|        - | 6370 | `				 * throw routing identical for both kinds of callee. */` |
|  2234840 | 6371 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|  1489889 | 6372 | `					pVmFunc->pNative->nByRefMask,0,0,pEffCallMap);` |
|   744951 | 6373 | `			}else{` |
|  1123717 | 6374 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   749000 | 6375 | `					(ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs),SySetUsed(&pVmFunc->aArgs),` |
|   374712 | 6376 | `					0,0,0,pEffCallMap);` |
|        - | 6377 | `			}` |
|  2238906 | 6378 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6379 | `		}` |
|  2238868 | 6380 | `		if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 6381 | `			/* C-bodied method (VM_FUNC_NATIVE): hand it to the foreign-function path.` |
|        - | 6382 | `			 *` |
|        - | 6383 | `			 * Everything a METHOD needs has already happened above — the sVmName` |
|        - | 6384 | `			 * lookup, overload selection, $this / self / late-static-binding` |
|        - | 6385 | `			 * resolution, the visibility check, the #[\Deprecated] notice, deferred` |
|        - | 6386 | `			 * argument materialization — and everything BELOW is exactly what a C` |
|        - | 6387 | `			 * body has no use for: a frame, an operand stack, a call record.` |
|        - | 6388 | `			 *` |
|        - | 6389 | `			 * The stack shape already matches a builtin's, because the method branch` |
|        - | 6390 | `			 * popped both the method-name slot and the target slot: [pArg,pTos) are` |
|        - | 6391 | `			 * this call's arguments and pTos is the slot that receives the result.` |
|        - | 6392 | `			 * So the jump lands on shared code, not a copy of it. */` |
|  1489894 | 6393 | `			pFunc = pVmFunc->pNative;` |
|  1489894 | 6394 | `			pNativeOwned = pThis; /* borrowed; the shared tail drops this reference */` |
|        - | 6395 | ``			/* A `C::m()` call reaches the method path with no object in the target`` |
|        - | 6396 | `			 * slot, and the path then adopts the CALLER's $this so an instance-context` |
|        - | 6397 | ``			 * `self::m()` still finds one. A native STATIC method must not see that —`` |
|        - | 6398 | `			 * it would read its argument list one slot off — so the receiver handed to` |
|        - | 6399 | `			 * the body is gated on the declaration, not on what the fallback found. */` |
|  1489894 | 6400 | `			pNativeRecv = (pVmFunc->iFlags & VM_FUNC_NATIVE_STATIC) ? 0 : pThis;` |
|  1489894 | 6401 | `			pNativeClass = pSelf;` |
|  1489894 | 6402 | `			goto NativeCall;` |
|        - | 6403 | `		}` |
|   748979 | 6404 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - | 6405 | `			/* Generator function: return a Generator object instead of executing */` |
|        - | 6406 | `			ph7_exec_ctx *pExecCtx;` |
|        - | 6407 | `			ph7_generator *pGenerator;` |
|        - | 6408 | `			ph7_class_instance *pGenObj;` |
|        - | 6409 | `			ph7_value *pCtxAttr;` |
|        - | 6410 | `			SyString sAttrName;` |
|        - | 6411 | `			ph7_value **apCallArgs;` |
|        - | 6412 | `			int nGenArgs, iArg;` |
|        - | 6413 | `			/* Collect arguments from the operand stack */` |
|      421 | 6414 | `			nGenArgs = (int)(pTos - pArg);` |
|      421 | 6415 | `			apCallArgs = 0;` |
|      421 | 6416 | `			if( nGenArgs > 0 ){` |
|        - | 6417 | `				/* php refuses a non-variable in a by-ref position at the CALL, and for` |
|        - | 6418 | `				 * a generator this IS the call. Routed like the branch's other` |
|        - | 6419 | `				 * pre-frame throws below: no callee frame exists yet, so drop the` |
|        - | 6420 | `				 * arguments plus the function-name slot and land the enclosing try. */` |
|      182 | 6421 | `				rc = VmScreenGenByRefArgs(&(*pVm),pVmFunc,pEffCallMap,pArg,` |
|       59 | 6422 | `					(sxu32)nGenArgs,pSelfHint);` |
|      123 | 6423 | `				if( rc != SXRET_OK ){` |
|        7 | 6424 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 6425 | `						goto Abort;` |
|        - | 6426 | `					}` |
|      212 | 6427 | `					PH7_INLINE_RESUME_BREAK()` |
|        7 | 6428 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6429 | `					{` |
|        - | 6430 | `						sxi32 iRpB;` |
|        7 | 6431 | `						if( VmRecordedResume(pVm,&iRpB,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 6432 | `							pc = iRpB;` |
|      ! 0 | 6433 | `							break;` |
|        - | 6434 | `						}` |
|        - | 6435 | `					}` |
|        7 | 6436 | `					goto Exception;` |
|        - | 6437 | `				}` |
|       56 | 6438 | `			}` |
|      415 | 6439 | `			if( nGenArgs > 0 ){` |
|      173 | 6440 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       56 | 6441 | `					nGenArgs * sizeof(ph7_value *));` |
|      117 | 6442 | `				if( apCallArgs == 0 ){` |
|        - | 6443 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 | 6444 | `					nGenArgs = 0;` |
|      ! 0 | 6445 | `				}else{` |
|      117 | 6446 | `					VmCallArgMap *pGenMap = pEffCallMap;` |
|      117 | 6447 | `					int didReorder = 0;` |
|      117 | 6448 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - | 6449 | `						/* Named-argument reordering for generator */` |
|       15 | 6450 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|       15 | 6451 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|       15 | 6452 | `						sxu32 nNV = nF;` |
|       15 | 6453 | `						sxi32 iVIdx = -1;` |
|        - | 6454 | `						sxi32 *aGSlot;` |
|        - | 6455 | `						sxu8 *aGUsed;` |
|        - | 6456 | `						sxu32 gi;` |
|       33 | 6457 | `						for( gi = 0; gi < nF; gi++ ){` |
|       21 | 6458 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|       12 | 6459 | `						}` |
|       21 | 6460 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       12 | 6461 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|       15 | 6462 | `						if( aGSlot ){` |
|       15 | 6463 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|       21 | 6464 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        6 | 6465 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|       15 | 6466 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 6467 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 6468 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6469 | `								goto Abort;` |
|        - | 6470 | `							}` |
|       15 | 6471 | `							if( rc == PH7_EXCEPTION ){` |
|        - | 6472 | `								/* A named-argument error is php's catchable \Error.` |
|        - | 6473 | `								 * No callee frame exists yet on this branch (the` |
|        - | 6474 | `								 * generator body never runs and VmEnterFrame is` |
|        - | 6475 | `								 * further down), so route it like the other` |
|        - | 6476 | `								 * pre-frame OP_CALL throws: drop the args + the` |
|        - | 6477 | `								 * function-name slot and land the enclosing try. */` |
|        3 | 6478 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        3 | 6479 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        3 | 6480 | `								PH7_INLINE_RESUME_BREAK()` |
|        3 | 6481 | `								VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6482 | `								{` |
|        - | 6483 | `									sxi32 iRpN;` |
|        3 | 6484 | `									if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){` |
|        3 | 6485 | `										pc = iRpN;` |
|        3 | 6486 | `										break;` |
|        - | 6487 | `									}` |
|        - | 6488 | `								}` |
|      ! 0 | 6489 | `								goto Exception;` |
|        - | 6490 | `							}` |
|        - | 6491 | `							{` |
|        - | 6492 | `								/* php's named-hole ArgumentCountError, checked BEFORE` |
|        - | 6493 | `								 * hole compaction: compacting first would report the` |
|        - | 6494 | `								 * positional wording with a fictitious count (g(b:2)` |
|        - | 6495 | ``								 * must be `g(): Argument #1 ($a) not passed`, not`` |
|        - | 6496 | `								 * "1 passed"). Implicit-required watermark, like the` |
|        - | 6497 | `								 * plain-call named path; a hole with NOTHING filled` |
|        - | 6498 | `								 * above it keeps php's count wording — fall through` |
|        - | 6499 | `								 * to VmFiberSetupFrame's check (the compacted count` |
|        - | 6500 | `								 * equals php's num_args there). */` |
|       13 | 6501 | `								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;` |
|       13 | 6502 | `								sxi32 iHole = -1;` |
|       13 | 6503 | `								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);` |
|       29 | 6504 | `								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){` |
|       19 | 6505 | `									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){` |
|       15 | 6506 | `										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);` |
|        6 | 6507 | `									}` |
|       11 | 6508 | `								}` |
|       27 | 6509 | `								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){` |
|        - | 6510 | `									sxu32 gj;` |
|       17 | 6511 | `									int bFound = 0;` |
|       23 | 6512 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       23 | 6513 | `										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }` |
|        5 | 6514 | `									}` |
|       17 | 6515 | `									if( !bFound && gHole + 1 <= nMaxFilledG ){` |
|      ! 0 | 6516 | `										iHole = (sxi32)gHole;` |
|      ! 0 | 6517 | `									}` |
|       10 | 6518 | `								}` |
|       13 | 6519 | `								if( iHole >= 0 ){` |
|      ! 0 | 6520 | `									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 6521 | `										(sxu32)iHole+1,&aFA[iHole].sName);` |
|      ! 0 | 6522 | `									SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 6523 | `									SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6524 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 6525 | `										goto Abort;` |
|        - | 6526 | `									}` |
|        - | 6527 | `									/* Route like the VmFiberSetupFrame throw below */` |
|      ! 0 | 6528 | `									PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 6529 | `									VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6530 | `									{` |
|        - | 6531 | `										sxi32 iRpH;` |
|      ! 0 | 6532 | `										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 6533 | `											pc = iRpH;` |
|      ! 0 | 6534 | `											break;` |
|        - | 6535 | `										}` |
|        - | 6536 | `									}` |
|      ! 0 | 6537 | `									goto Exception;` |
|        - | 6538 | `								}` |
|        - | 6539 | `							}` |
|        - | 6540 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - | 6541 | `							 * append overflow (variadic / positional beyond` |
|        - | 6542 | `							 * formals) so downstream sees every argument. */` |
|        - | 6543 | `							{` |
|       13 | 6544 | `								int nOut = 0;` |
|       29 | 6545 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - | 6546 | `									sxu32 gj;` |
|       25 | 6547 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       25 | 6548 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|       19 | 6549 | `											apCallArgs[nOut++] = &pArg[gj];` |
|       19 | 6550 | `											break;` |
|        - | 6551 | `										}` |
|        5 | 6552 | `									}` |
|       11 | 6553 | `								}` |
|       29 | 6554 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|       19 | 6555 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 | 6556 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 | 6557 | `									}` |
|       11 | 6558 | `								}` |
|       13 | 6559 | `								nGenArgs = nOut;` |
|        - | 6560 | `							}` |
|       13 | 6561 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|       13 | 6562 | `							didReorder = 1;` |
|        5 | 6563 | `						}` |
|        - | 6564 | `						/* If aGSlot allocation failed, fall through to` |
|        - | 6565 | `						 * positional fill below — preserves arg order rather` |
|        - | 6566 | `						 * than passing an uninitialized apCallArgs. */` |
|        5 | 6567 | `					}` |
|      115 | 6568 | `					if( !didReorder ){` |
|      211 | 6569 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|      111 | 6570 | `							apCallArgs[iArg] = &pArg[iArg];` |
|       58 | 6571 | `						}` |
|       50 | 6572 | `					}` |
|        - | 6573 | `				}` |
|       55 | 6574 | `			}` |
|        - | 6575 | `			/* Create execution context and generator wrapper */` |
|      413 | 6576 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|      413 | 6577 | `			if( pExecCtx == 0 ){` |
|      ! 0 | 6578 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6579 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 6580 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 6581 | `				break;` |
|        - | 6582 | `			}` |
|      413 | 6583 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|      413 | 6584 | `			if( pGenerator == 0 ){` |
|      ! 0 | 6585 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 | 6586 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6587 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 6588 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 6589 | `				break;` |
|        - | 6590 | `			}` |
|        - | 6591 | `			/* Set up the frame with arguments, closure env, $this */` |
|      413 | 6592 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|      413 | 6593 | `			pVm->pFrame = pExecCtx->pFrame;` |
|      821 | 6594 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,` |
|      408 | 6595 | `				pEffCallMap ? (pEffCallMap->bStrict ? 1 : 0) : (pVm->bCurStrict ? 1 : 0),` |
|      204 | 6596 | `				pSelfHint,` |
|        - | 6597 | `				TRUE/*generator: the g(...) call site is in the message*/,` |
|        - | 6598 | `				TRUE/*a source-level call binds a by-ref parameter to the caller's slot*/);` |
|      413 | 6599 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|      413 | 6600 | `			pExecCtx->pFrame->pParent = 0;` |
|      413 | 6601 | `			if( apCallArgs ){` |
|      115 | 6602 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       55 | 6603 | `			}` |
|      413 | 6604 | `			if( rc != SXRET_OK ){` |
|       18 | 6605 | `				VmReleaseGenerator(pVm, pGenerator);` |
|       18 | 6606 | `				if( pThis ){` |
|        3 | 6607 | `					PH7_ClassInstanceUnref(pThis);` |
|        1 | 6608 | `				}` |
|       18 | 6609 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 6610 | `					goto Abort;` |
|        - | 6611 | `				}` |
|       18 | 6612 | `				if( rc == PH7_EXCEPTION ){` |
|        - | 6613 | `					/* A declared-type TypeError thrown while binding the` |
|        - | 6614 | `					 * generator's arguments (php binds + type-checks eagerly at` |
|        - | 6615 | `					 * the g(...) call site, before any resume — band A #2). If` |
|        - | 6616 | `					 * an inline try THIS exec owns caught it, land at its` |
|        - | 6617 | `					 * redirect (the drain subsumes the operand pops); else pop` |
|        - | 6618 | `					 * the args + function name and route like the other` |
|        - | 6619 | `					 * OP_CALL throw paths. */` |
|       22 | 6620 | `					PH7_INLINE_RESUME_BREAK()` |
|       16 | 6621 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6622 | `					{` |
|        - | 6623 | `						sxi32 iRpG;` |
|       16 | 6624 | `						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){` |
|       16 | 6625 | `							pc = iRpG;` |
|       16 | 6626 | `							break;` |
|        - | 6627 | `						}` |
|        - | 6628 | `					}` |
|      ! 0 | 6629 | `					goto Exception;` |
|        - | 6630 | `				}` |
|      ! 0 | 6631 | `				break;` |
|        - | 6632 | `			}` |
|        - | 6633 | `			/* Create Generator class instance */` |
|      397 | 6634 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|      397 | 6635 | `			if( pGenObj == 0 ){` |
|      ! 0 | 6636 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 | 6637 | `				break;` |
|        - | 6638 | `			}` |
|        - | 6639 | `			/* Store generator in __ctx attribute */` |
|      397 | 6640 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      397 | 6641 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|      397 | 6642 | `			if( pCtxAttr ){` |
|      397 | 6643 | `				pCtxAttr->x.pOther = pGenerator;` |
|      397 | 6644 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      196 | 6645 | `			}` |
|        - | 6646 | `			/* Pop args and function name, push Generator object. PH7_NewClassInstance` |
|        - | 6647 | `			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do` |
|        - | 6648 | `			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope` |
|        - | 6649 | ``			 * and its __destruct (which runs pending `finally` blocks and frees the`` |
|        - | 6650 | `			 * exec context) never fires. */` |
|      397 | 6651 | `			PH7_MemObjRelease(pTos);` |
|      397 | 6652 | `			pTos = &pTos[-nCallArgs];` |
|      397 | 6653 | `			pTos->x.pOther = pGenObj;` |
|      397 | 6654 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|      397 | 6655 | `			if( pThis ){` |
|       16 | 6656 | `				PH7_ClassInstanceUnref(pThis);` |
|        6 | 6657 | `			}` |
|      397 | 6658 | `			break;` |
|        - | 6659 | `		}` |
|        - | 6660 | `		/* Extract the formal argument set */` |
|   748563 | 6661 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - | 6662 | `		/* Create a new VM frame  */` |
|   748563 | 6663 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|   748563 | 6664 | `		if( rc != SXRET_OK ){` |
|        - | 6665 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 6666 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 6667 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 6668 | `				&pVmFunc->sName);` |
|        - | 6669 | `			/* The frame that would own (and later release) $this never got created; for a bound` |
|        - | 6670 | `			 * plain closure the consumed transient is the object's ONLY ref, so release it here` |
|        - | 6671 | `			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */` |
|      ! 0 | 6672 | `			if( bClosureThis && pThis ){` |
|      ! 0 | 6673 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 | 6674 | `			}` |
|        - | 6675 | `			/* Pop given arguments */` |
|      ! 0 | 6676 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 6677 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6678 | `			}` |
|        - | 6679 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 | 6680 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 6681 | `			break;` |
|        - | 6682 | `		}` |
|   748563 | 6683 | `		if( pClosureScope ){` |
|        - | 6684 | `			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /` |
|        - | 6685 | `			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected` |
|        - | 6686 | `			 * access inside the body resolves against it. */` |
|       53 | 6687 | `			pFrame->pBoundScope = pClosureScope;` |
|       25 | 6688 | `		}` |
|        - | 6689 | `		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band` |
|        - | 6690 | `		 * A #4): sArg over-counts (defaulted params installed, variadic packed` |
|        - | 6691 | `		 * as one entry) so php's answers can't be derived from it. */` |
|   748563 | 6692 | `		pFrame->nActualArgs = (int)(pTos - pArg);` |
|   748563 | 6693 | `		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) \|\| bClosureThis) ){` |
|        - | 6694 | `			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */` |
|        - | 6695 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|   404593 | 6696 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|   404593 | 6697 | `			if( pObj ){` |
|        - | 6698 | `				/* Reflect the change */` |
|   404593 | 6699 | `				pObj->x.pOther = pThis;` |
|   404593 | 6700 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   202294 | 6701 | `			}` |
|   202294 | 6702 | `		}` |
|   748563 | 6703 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - | 6704 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - | 6705 | `			/* Install static variables */` |
|       47 | 6706 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       91 | 6707 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|       47 | 6708 | `				pStatic = &aStatic[n];` |
|       47 | 6709 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - | 6710 | `					/* Initialize the static variables */` |
|       27 | 6711 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|       27 | 6712 | `					if( pObj ){` |
|        - | 6713 | `						/* Assume a NULL initialization value */` |
|       27 | 6714 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|       27 | 6715 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - | 6716 | `							/* Evaluate initialization expression (Any complex expression) */` |
|       27 | 6717 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|       12 | 6718 | `						}` |
|       27 | 6719 | `						pObj->nIdx = pStatic->nIdx;` |
|        - | 6720 | `						/* Permanent pin: the storage outlives every call */` |
|       27 | 6721 | `						VmPinMemObjSlot(&(*pVm),pStatic->nIdx);` |
|       15 | 6722 | `					}else{` |
|      ! 0 | 6723 | `						continue;` |
|        - | 6724 | `					}` |
|       12 | 6725 | `				}` |
|        - | 6726 | `				/* Install in the current frame — a REGISTERED binding, and the slot is` |
|        - | 6727 | `				 * PINNED: the static's storage belongs to the function, not to this` |
|        - | 6728 | `				 * call, so neither the frame teardown nor an unset of the NAME may` |
|        - | 6729 | `				 * recycle it. Poking hVar directly left the binding invisible to the` |
|        - | 6730 | ``				 * reference table, so an array element sharing the static (`[&$s]`)`` |
|        - | 6731 | ``				 * did not count as a reference and `unset($s)` destroyed the storage —`` |
|        - | 6732 | `				 * the next call started over from the initializer. The pin is taken ONCE,` |
|        - | 6733 | `				 * where the slot is created (above). */` |
|       69 | 6734 | `				PH7_VmBindVarSlot(&(*pVm),pFrame,SyStringData(&pStatic->sName),` |
|       22 | 6735 | `					SyStringLength(&pStatic->sName),pStatic->nIdx);` |
|       25 | 6736 | `			}` |
|       22 | 6737 | `		}` |
|        - | 6738 | `		/* Push arguments in the local frame */` |
|        - | 6739 | `		{` |
|   748563 | 6740 | `		VmCallArgMap *pCallMap3 = pEffCallMap;` |
|        - | 6741 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - | 6742 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|        - | 6743 | `		/* A compiled call in a strict file always carries a map with bStrict set` |
|        - | 6744 | `		 * (GenStateAttachStrictFlag); with no map at all the call is either a` |
|        - | 6745 | `		 * compiled WEAK one — where bCurStrict is 0 for the same unit — or an` |
|        - | 6746 | `		 * ENGINE-dispatched one, whose synthetic OP_CALL has no map to carry the` |
|        - | 6747 | `		 * caller's mode. php scopes parameter coercion by the CALLING file either` |
|        - | 6748 | `		 * way, and bCurStrict is that file's mode.` |
|        - | 6749 | `		 *` |
|        - | 6750 | `		 * Unless an INTERNAL function is what reached for this callback (bCallbackWeak):` |
|        - | 6751 | `		 * php has no calling file at that boundary and binds weakly, so` |
|        - | 6752 | ``		 * `array_map('takesInt', ["5"])` from a strict file RUNS there — PHL raised a`` |
|        - | 6753 | `		 * TypeError on valid php, because the ambient bCurStrict was still the strict` |
|        - | 6754 | `		 * caller's. call_user_func / call_user_func_array are php's two forwards and` |
|        - | 6755 | `		 * carry the caller's mode on a map instead. */` |
|  1122630 | 6756 | `		int bCallIsStrict = pCallMap3 ? (pCallMap3->bStrict ? 1 : 0)` |
|   690595 | 6757 | `		                   : (bCallbackWeak ? 0 : (pVm->bCurStrict ? 1 : 0));` |
|   748563 | 6758 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - | 6759 | `			/* ============================================================` |
|        - | 6760 | `			 * Named-argument matching path (PHP 8.0)` |
|        - | 6761 | `			 *` |
|        - | 6762 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - | 6763 | `			 * or position, then install them in the frame.` |
|        - | 6764 | `			 * ============================================================ */` |
|      385 | 6765 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      385 | 6766 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|      385 | 6767 | `			sxi32 iVariadicIdx = -1;` |
|        - | 6768 | `			sxu32 nNonVariadic;` |
|        - | 6769 | `			sxi32 *aSlot;` |
|        - | 6770 | `			sxu8  *aUsed;` |
|        - | 6771 | `			sxu32 i;` |
|        - | 6772 | `			/* Find variadic parameter index */` |
|      993 | 6773 | `			for( i = 0; i < nFormal; i++ ){` |
|      719 | 6774 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|      110 | 6775 | `					iVariadicIdx = (sxi32)i;` |
|      110 | 6776 | `					break;` |
|        - | 6777 | `				}` |
|      309 | 6778 | `			}` |
|      385 | 6779 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - | 6780 | `			/* Allocate mapping arrays */` |
|      575 | 6781 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      380 | 6782 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|      385 | 6783 | `			if( aSlot == 0 ){` |
|      ! 0 | 6784 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 | 6785 | `				goto Abort;` |
|        - | 6786 | `			}` |
|      385 | 6787 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - | 6788 | `			/* Resolve named arguments to formal parameters */` |
|      575 | 6789 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|      190 | 6790 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|      385 | 6791 | `			if( rc == PH7_ABORT ){` |
|        8 | 6792 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 | 6793 | `				goto Abort;` |
|        - | 6794 | `			}` |
|      379 | 6795 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 6796 | `				/* php's catchable \Error for a bad named argument. The callee` |
|        - | 6797 | `				 * frame is already entered but its body must not run: unwind` |
|        - | 6798 | `				 * exactly like the named-hole ArgumentCountError path below` |
|        - | 6799 | `				 * (release the not-yet-installed actuals — Pass 2's release` |
|        - | 6800 | `				 * loop has not run — pop the callee slot, mark that there is no` |
|        - | 6801 | `				 * callee operand stack, and let VmCallFinish route the throw). */` |
|        - | 6802 | `				sxu32 iRel;` |
|        5 | 6803 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       11 | 6804 | `				for( iRel = 0; iRel < nActual; iRel++ ){` |
|        7 | 6805 | `					PH7_MemObjRelease(&pArg[iRel]);` |
|        4 | 6806 | `				}` |
|        5 | 6807 | `				PH7_MemObjRelease(pTos);` |
|        5 | 6808 | `				pTos = &pTos[-nCallArgs];` |
|        5 | 6809 | `				pFrameStack = 0;` |
|        5 | 6810 | `				goto SkipFuncBody;` |
|        - | 6811 | `			}` |
|        - | 6812 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|        - | 6813 | `			{` |
|        - | 6814 | `			/* php's required watermark for the hole check below, plus the` |
|        - | 6815 | `			 * highest formal slot an actual resolved to: php words a hole` |
|        - | 6816 | ``			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole`` |
|        - | 6817 | `			 * with nothing filled above it gets the positional count message` |
|        - | 6818 | `			 * (zend's RECV arg_num > EX(num_args) distinction). */` |
|        - | 6819 | `			sxu32 nReqNamed;` |
|        - | 6820 | `			sxu32 nNVNamed;` |
|      375 | 6821 | `			sxu32 nMaxFilled = 0;` |
|      375 | 6822 | `			nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);` |
|     1299 | 6823 | `			for( i = 0; i < nActual; i++ ){` |
|      929 | 6824 | `				if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|      386 | 6825 | `					nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|      191 | 6826 | `				}` |
|      467 | 6827 | `			}` |
|      953 | 6828 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - | 6829 | `				/* Find the stack arg mapped to formal n */` |
|      594 | 6830 | `				sxi32 iSrc = -1;` |
|      940 | 6831 | `				for( i = 0; i < nActual; i++ ){` |
|      808 | 6832 | `					if( aSlot[i] == (sxi32)n ){` |
|      462 | 6833 | `						iSrc = (sxi32)i;` |
|      462 | 6834 | `						break;` |
|        - | 6835 | `					}` |
|      177 | 6836 | `				}` |
|      594 | 6837 | `				if( iSrc >= 0 ){` |
|        - | 6838 | `					/* Argument was provided — install with type checking */` |
|      462 | 6839 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - | 6840 | `					/* An explicit null is NOT redirected to the default: PHP applies a` |
|        - | 6841 | `					 * default only for an OMITTED argument. An explicit null falls through` |
|        - | 6842 | `					 * to the type check below (TypeError for a non-nullable typed param,` |
|        - | 6843 | ``					 * kept as null for a typeless one). An implicitly-nullable `Type $x =`` |
|        - | 6844 | ``					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */`` |
|        - | 6845 | `					/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 6846 | `					 * coercion and whole-real materialization in place): the shared` |
|        - | 6847 | `					 * per-argument helper — one implementation for both OP_CALL` |
|        - | 6848 | `					 * paths and the generator/fiber binder (§7.1(f) fold). */` |
|      462 | 6849 | `					iArgPreFlags = pVal->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|      462 | 6850 | `					rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pVal,bCallIsStrict,pSelfHint);` |
|      462 | 6851 | `					if( rc != SXRET_OK ){` |
|        7 | 6852 | `						if( rc == PH7_ABORT ) goto Abort;` |
|        7 | 6853 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 | 6854 | `						PH7_MemObjRelease(pTos);` |
|        7 | 6855 | `						pTos = &pTos[-nCallArgs];` |
|        7 | 6856 | `						pFrameStack = 0;` |
|        7 | 6857 | `						rc = PH7_EXCEPTION;` |
|       10 | 6858 | `						goto SkipFuncBody;` |
|        - | 6859 | `					}` |
|        - | 6860 | `					/* Install: by reference or by value */` |
|      456 | 6861 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|       28 | 6862 | `						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){` |
|        - | 6863 | `							/* php 8.1: $GLOBALS cannot be passed by reference */` |
|        - | 6864 | `							SyBlob sMsg;` |
|      ! 0 | 6865 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 6866 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 6867 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 6868 | `							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|      ! 0 | 6869 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 6870 | `								goto Abort;` |
|        - | 6871 | `							}` |
|      ! 0 | 6872 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 6873 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 6874 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 6875 | `							pFrameStack = 0;` |
|      ! 0 | 6876 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 6877 | `							goto SkipFuncBody;` |
|        - | 6878 | `						}` |
|       28 | 6879 | `						if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)iSrc,pVal) ){` |
|        - | 6880 | `							/* php refuses a by-ref argument whose EXPRESSION is not a variable, at` |
|        - | 6881 | `							 * the call and before the callee runs. Deciding it from the VALUE that` |
|        - | 6882 | `							 * arrived was wrong both ways: an operator result carries its LEFT` |
|        - | 6883 | ``							 * operand's slot, so `f($i + 1)` aliased and overwrote `$i`; and a`` |
|        - | 6884 | `							 * literal and a CALL result look alike there, where php accepts the` |
|        - | 6885 | `							 * call. VmArgRefusedByRef reads the call site's compile-time shape mask` |
|        - | 6886 | `							 * and falls back to the old runtime test only when there is none. */` |
|        - | 6887 | `							sxi32 rcRef;` |
|        3 | 6888 | `							rcRef = VmThrowByRefRefusal(&(*pVm),` |
|        2 | 6889 | `								(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|        2 | 6890 | `								&pVmFunc->sName,(sxu32)(n+1),&aFormalArg[n].sName);` |
|        3 | 6891 | `							if( rcRef == PH7_ABORT ){` |
|      ! 0 | 6892 | `								goto Abort;` |
|        - | 6893 | `							}` |
|        - | 6894 | `							/* Same teardown as the type-check refusal above: free the slot map,` |
|        - | 6895 | `							 * release the result slot and pop the actuals, then let SkipFuncBody` |
|        - | 6896 | `							 * route the throw. */` |
|        3 | 6897 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 6898 | `							PH7_MemObjRelease(pTos);` |
|        3 | 6899 | `							pTos = &pTos[-nCallArgs];` |
|        3 | 6900 | `							pFrameStack = 0;` |
|        3 | 6901 | `							rc = PH7_EXCEPTION;` |
|        3 | 6902 | `							goto SkipFuncBody;` |
|        - | 6903 | `						}` |
|       25 | 6904 | `						PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)iSrc,pVal);` |
|       25 | 6905 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 | 6906 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 6907 | `						}else{` |
|        - | 6908 | `							SyHashEntry *pRefEntry;` |
|        - | 6909 | `							/* The declared type's conversion is what the reference holds. */` |
|       25 | 6910 | `							PH7_VmByRefArgWriteBack(&(*pVm),pVal,iArgPreFlags);` |
|       37 | 6911 | `							pRefEntry = SyHashGet(&pFrame->hVar,` |
|       24 | 6912 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 | 6913 | `							if( pRefEntry == 0 ){` |
|       37 | 6914 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 | 6915 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|       25 | 6916 | `								sArg.nIdx = pVal->nIdx;` |
|       25 | 6917 | `								sArg.pUserData = 0;` |
|       25 | 6918 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 | 6919 | `							}` |
|       25 | 6920 | `							pObj = 0;` |
|        - | 6921 | `						}` |
|       13 | 6922 | `					}else{` |
|      430 | 6923 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 6924 | `					}` |
|      454 | 6925 | `					if( pObj ){` |
|      430 | 6926 | `						PH7_MemObjStore(pVal,pObj);` |
|      430 | 6927 | `						sArg.nIdx = pObj->nIdx;` |
|      430 | 6928 | `						sArg.pUserData = 0;` |
|      430 | 6929 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      213 | 6930 | `					}` |
|      229 | 6931 | `				}else{` |
|        - | 6932 | `					/* Argument was NOT provided — use default or leave unset */` |
|      135 | 6933 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 6934 | `						/* Should not reach here; variadic handled separately below */` |
|      135 | 6935 | `					}else if( n < nReqNamed ){` |
|        - | 6936 | `						/* php's implicit-required rule applies to named calls` |
|        - | 6937 | `						 * too: a hole below the required watermark throws even` |
|        - | 6938 | `						 * when the formal carries a default — f($a,$b=2,$c)` |
|        - | 6939 | ``						 * called as f(a:1,c:3) is `Argument #2 ($b) not`` |
|        - | 6940 | ``						 * passed` (a hole with NO default is always below the`` |
|        - | 6941 | `						 * watermark, so this subsumes the no-default case).` |
|        - | 6942 | `						 * A hole with nothing filled ABOVE it uses php's` |
|        - | 6943 | `						 * positional count wording instead. The passed stack` |
|        - | 6944 | `						 * args were not released yet on this path (that loop` |
|        - | 6945 | `						 * runs after Pass 2) — release them before the exit. */` |
|        5 | 6946 | `						if( n + 1 > nMaxFilled ){` |
|        3 | 6947 | `							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)` |
|      ! 0 | 6948 | `								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 6949 | `									nMaxFilled,nReqNamed,SySetUsed(&pVmFunc->aArgs))` |
|        3 | 6950 | `								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        1 | 6951 | `									nMaxFilled,nReqNamed,nNVNamed,TRUE);` |
|        2 | 6952 | `						}else{` |
|        3 | 6953 | `							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        - | 6954 | `						}` |
|        5 | 6955 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        9 | 6956 | `						for( i = 0; i < nActual; i++ ){` |
|        5 | 6957 | `							PH7_MemObjRelease(&pArg[i]);` |
|        3 | 6958 | `						}` |
|        5 | 6959 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 6960 | `							goto Abort;` |
|        - | 6961 | `						}` |
|        5 | 6962 | `						PH7_MemObjRelease(pTos);` |
|        5 | 6963 | `						pTos = &pTos[-nCallArgs];` |
|        5 | 6964 | `						pFrameStack = 0;` |
|        5 | 6965 | `						rc = PH7_EXCEPTION;` |
|        5 | 6966 | `						goto SkipFuncBody;` |
|      131 | 6967 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      131 | 6968 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      131 | 6969 | `						if( pObj ){` |
|      131 | 6970 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|      131 | 6971 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      131 | 6972 | `							sArg.nIdx = pObj->nIdx;` |
|      131 | 6973 | `							sArg.pUserData = 0;` |
|      131 | 6974 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 6975 | `							/* A null default on an implicitly-nullable param must stay null` |
|        - | 6976 | `							 * (see the positional-path note above). */` |
|      128 | 6977 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|       42 | 6978 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0` |
|       24 | 6979 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 6980 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 6981 | `								if( xCast ) xCast(pObj);` |
|      ! 0 | 6982 | `							}else{` |
|        - | 6983 | `								/* Mask matched — a const-indirected whole-real default` |
|        - | 6984 | ``								 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|      131 | 6985 | `								VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 6986 | `							}` |
|       64 | 6987 | `						}` |
|       64 | 6988 | `					}` |
|        - | 6989 | `				}` |
|      293 | 6990 | `			}` |
|        - | 6991 | `			} /* end nReqNamed scope */` |
|        - | 6992 | `			/* Handle variadic parameter */` |
|      362 | 6993 | `			if( iVariadicIdx >= 0 ){` |
|      110 | 6994 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|      110 | 6995 | `				if( pObj ){` |
|        - | 6996 | `					/* Capture the slot index now: PH7_HashmapInsert below can` |
|        - | 6997 | `					 * PH7_ReserveMemObj, reallocating pVm->aMemObj and dangling pObj` |
|        - | 6998 | `					 * (same latent UAF the positional path guards against). */` |
|        - | 6999 | `					sxu32 nVariadicSlot;` |
|      110 | 7000 | `					PH7_MemObjToHashmap(pObj);` |
|      110 | 7001 | `					nVariadicSlot = pObj->nIdx;` |
|        - | 7002 | `					{` |
|      110 | 7003 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - | 7004 | `						/* php numbers a failing NAMED variadic element as` |
|        - | 7005 | `						 * max(total positional args, declared non-variadic` |
|        - | 7006 | `						 * formals) + 1 — zend's RECV slots always count —` |
|        - | 7007 | `						 * whichever named element fails; a POSITIONAL element` |
|        - | 7008 | `						 * uses its own 1-based call position. */` |
|      110 | 7009 | `						sxu32 nPositional = 0;` |
|      622 | 7010 | `						for( i = 0; i < nActual; i++ ){` |
|      516 | 7011 | `							if( !(i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0) ){` |
|      333 | 7012 | `								nPositional++;` |
|      165 | 7013 | `							}` |
|      260 | 7014 | `						}` |
|      578 | 7015 | `						for( i = 0; i < nActual; i++ ){` |
|      510 | 7016 | `							if( aSlot[i] == -1 ){` |
|      464 | 7017 | `								int bNamed = (i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0);` |
|      464 | 7018 | `								int bRefElem = 0; /* alias this entry to the caller's slot? */` |
|        - | 7019 | `								/* Same per-element type check + weak coercion as the` |
|        - | 7020 | ``								 * positional-only path (shared helper; no `($name)`). */`` |
|      694 | 7021 | `								rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|      460 | 7022 | `									&aFormalArg[iVariadicIdx],&pArg[i],` |
|      307 | 7023 | `									bNamed ? SXMAX(nPositional,nNonVariadic) + 1 : i + 1,bCallIsStrict);` |
|      464 | 7024 | `								if( rc != SXRET_OK ){` |
|       39 | 7025 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 7026 | `										goto Abort;` |
|        - | 7027 | `									}` |
|       39 | 7028 | `									SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       39 | 7029 | `									PH7_MemObjRelease(pTos);` |
|       39 | 7030 | `									pTos = &pTos[-nCallArgs];` |
|       39 | 7031 | `									pFrameStack = 0;` |
|       39 | 7032 | `									rc = PH7_EXCEPTION;` |
|       39 | 7033 | `									goto SkipFuncBody;` |
|        - | 7034 | `								}` |
|      428 | 7035 | `								if( aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7036 | `									/* php screens a by-ref VARIADIC tail per collected element, in its` |
|        - | 7037 | `									 * no-name wording: a variadic has no per-element parameter name, so` |
|        - | 7038 | ``									 * php says `Argument #N could not be passed by reference` and stops`` |
|        - | 7039 | `									 * there. Nothing screened this arm at all — the branch that collects` |
|        - | 7040 | `									 * a variadic runs before the by-ref binder ever sees a formal. */` |
|        8 | 7041 | `									if( PH7_VmArgRefusedByRef(pCallMap3,i,&pArg[i]) ){` |
|        - | 7042 | `										SyBlob sMsgV;` |
|        - | 7043 | `										sxi32 rcV;` |
|        3 | 7044 | `										SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        3 | 7045 | `										SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        2 | 7046 | `											&pVmFunc->sName,(unsigned)(i + 1));` |
|        3 | 7047 | `										rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        3 | 7048 | `										if( rcV == PH7_ABORT ){` |
|      ! 0 | 7049 | `											goto Abort;` |
|        - | 7050 | `										}` |
|        3 | 7051 | `										SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 7052 | `										PH7_MemObjRelease(pTos);` |
|        3 | 7053 | `										pTos = &pTos[-nCallArgs];` |
|        3 | 7054 | `										pFrameStack = 0;` |
|        3 | 7055 | `										rc = PH7_EXCEPTION;` |
|        3 | 7056 | `										goto SkipFuncBody;` |
|        - | 7057 | `									}` |
|        5 | 7058 | `									PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,i,&pArg[i]);` |
|        2 | 7059 | `								}` |
|        - | 7060 | `								/* A by-ref variadic tail ALIASES its actuals, named entries` |
|        - | 7061 | `								 * included (the positional twin below this branch says why). */` |
|      639 | 7062 | `								bRefElem = (aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF)` |
|      422 | 7063 | `									&& pArg[i].nIdx != SXU32_HIGH;` |
|      426 | 7064 | `								if( bNamed ){` |
|        - | 7065 | `									/* Named variadic entry: insert with string key */` |
|        - | 7066 | `									ph7_value sKey;` |
|      124 | 7067 | `									PH7_MemObjInit(pVm, &sKey);` |
|      124 | 7068 | `									PH7_MemObjStringAppend(&sKey,` |
|      120 | 7069 | `										pCallMap3->aNames[i].zString,` |
|      120 | 7070 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|      124 | 7071 | `									if( bRefElem ){` |
|        5 | 7072 | `										PH7_HashmapInsertByRef(pVarMap, &sKey, pArg[i].nIdx);` |
|        3 | 7073 | `									}else{` |
|      120 | 7074 | `										PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|        - | 7075 | `									}` |
|      124 | 7076 | `									PH7_MemObjRelease(&sKey);` |
|      365 | 7077 | `								}else if( bRefElem ){` |
|        - | 7078 | `									/* Positional variadic entry, aliased */` |
|      ! 0 | 7079 | `									PH7_HashmapInsertByRef(pVarMap, 0, pArg[i].nIdx);` |
|      ! 0 | 7080 | `								}else{` |
|        - | 7081 | `									/* Positional variadic entry */` |
|      305 | 7082 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - | 7083 | `								}` |
|      211 | 7084 | `							}` |
|      238 | 7085 | `						}` |
|        - | 7086 | `					}` |
|       72 | 7087 | `					sArg.nIdx = nVariadicSlot; /* pObj may be stale here (aMemObj realloc) */` |
|       72 | 7088 | `					sArg.pUserData = 0;` |
|       72 | 7089 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       34 | 7090 | `				}` |
|       38 | 7091 | `			}else{` |
|        - | 7092 | `				/* No variadic — preserve unresolved positional overflow` |
|        - | 7093 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - | 7094 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - | 7095 | `				 * the positional-only path's behavior. */` |
|      254 | 7096 | `				sxu32 nAnon = nNonVariadic;` |
|      652 | 7097 | `				for( i = 0; i < nActual; i++ ){` |
|      400 | 7098 | `					if( aSlot[i] == -2 ){` |
|        - | 7099 | `						char zAnonBuf[32];` |
|        - | 7100 | `						SyString sAnonName;` |
|      ! 0 | 7101 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 | 7102 | `							"[%u]apArg",nAnon);` |
|      ! 0 | 7103 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 | 7104 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 | 7105 | `						if( pObj ){` |
|      ! 0 | 7106 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 | 7107 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 | 7108 | `							sArg.pUserData = 0;` |
|      ! 0 | 7109 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 | 7110 | `						}` |
|      ! 0 | 7111 | `						nAnon++;` |
|      ! 0 | 7112 | `					}` |
|      201 | 7113 | `				}` |
|        - | 7114 | `			}` |
|        - | 7115 | `			/* Release all stack arguments */` |
|     1160 | 7116 | `			for( i = 0; i < nActual; i++ ){` |
|      840 | 7117 | `				PH7_MemObjRelease(&pArg[i]);` |
|      422 | 7118 | `			}` |
|      324 | 7119 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - | 7120 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|      324 | 7121 | `			n = nFormal;` |
|      164 | 7122 | `		}else{` |
|        - | 7123 | `		/* ============================================================` |
|        - | 7124 | `		 * Positional-only matching path (original)` |
|        - | 7125 | `		 * ============================================================ */` |
|        - | 7126 | `		/* Base of the actual argument stack, for php's per-ELEMENT argument` |
|        - | 7127 | `		 * number in a variadic type-error (php numbers a variadic-collected` |
|        - | 7128 | `		 * element by its overall 1-based call position, not the formal index). */` |
|   748183 | 7129 | `		ph7_value *pArgBase = pArg;` |
|   748183 | 7130 | `		n = 0;` |
|   980929 | 7131 | `		while( pArg < pTos ){` |
|   237329 | 7132 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - | 7133 | `				/* Variadic parameter: collect all remaining args into an array */` |
|      285 | 7134 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      285 | 7135 | `				if( pObj ){` |
|        - | 7136 | `					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,` |
|        - | 7137 | `					 * reallocating pVm->aMemObj and dangling pObj — so don't read pObj->nIdx after` |
|        - | 7138 | `					 * the packing loop (pre-existing UAF, masked by the pool allocator). pMap is a` |
|        - | 7139 | `					 * separately-allocated hashmap and stays valid across the realloc. */` |
|        - | 7140 | `					sxu32 nVariadicIdx;` |
|        - | 7141 | `					/* Initialize as empty array */` |
|      285 | 7142 | `					PH7_MemObjToHashmap(pObj);` |
|      285 | 7143 | `					nVariadicIdx = pObj->nIdx;` |
|        - | 7144 | `					{` |
|      285 | 7145 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     1923 | 7146 | `						while( pArg < pTos ){` |
|        - | 7147 | `							/* Per-element type check + weak coercion (shared helper,` |
|        - | 7148 | `							 * also used by the named-argument path). The argument` |
|        - | 7149 | `							 * number is the element's overall 1-based call position` |
|        - | 7150 | `` 							 * ((pArg - pArgBase) + 1) and, like php, the `($name)` `` |
|        - | 7151 | `							 * clause is omitted. */` |
|     2531 | 7152 | `							rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|     1684 | 7153 | `								&aFormalArg[n],pArg,(sxu32)(pArg - pArgBase) + 1,bCallIsStrict);` |
|     1689 | 7154 | `							if( rc != SXRET_OK ){` |
|       43 | 7155 | `								if( rc == PH7_ABORT ){` |
|      ! 0 | 7156 | `									goto Abort;` |
|        - | 7157 | `								}` |
|        - | 7158 | `								/* Skip function body, route through normal cleanup */` |
|       43 | 7159 | `								PH7_MemObjRelease(pTos);` |
|       43 | 7160 | `								pTos = &pTos[-nCallArgs];` |
|       43 | 7161 | `								pFrameStack = 0;` |
|       43 | 7162 | `								rc = PH7_EXCEPTION;` |
|       43 | 7163 | `								goto SkipFuncBody;` |
|        - | 7164 | `							}` |
|     1649 | 7165 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7166 | `								/* The positional twin of the named path's variadic screen above:` |
|        - | 7167 | `								 * php refuses a non-variable collected into a by-ref variadic tail,` |
|        - | 7168 | `								 * in its no-name wording. */` |
|       52 | 7169 | `								sxu32 nPosV = (sxu32)(pArg - pArgBase);` |
|       52 | 7170 | `								if( PH7_VmArgRefusedByRef(pCallMap3,nPosV,pArg) ){` |
|        - | 7171 | `									SyBlob sMsgV;` |
|        - | 7172 | `									sxi32 rcV;` |
|        7 | 7173 | `									SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        7 | 7174 | `									SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        6 | 7175 | `										&pVmFunc->sName,(unsigned)(nPosV + 1));` |
|        7 | 7176 | `									rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        7 | 7177 | `									if( rcV == PH7_ABORT ){` |
|      ! 0 | 7178 | `										goto Abort;` |
|        - | 7179 | `									}` |
|        7 | 7180 | `									PH7_MemObjRelease(pTos);` |
|        7 | 7181 | `									pTos = &pTos[-nCallArgs];` |
|        7 | 7182 | `									pFrameStack = 0;` |
|        7 | 7183 | `									rc = PH7_EXCEPTION;` |
|        7 | 7184 | `									goto SkipFuncBody;` |
|        - | 7185 | `								}` |
|       46 | 7186 | `								PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,nPosV,pArg);` |
|       46 | 7187 | `								if( pArg->nIdx != SXU32_HIGH ){` |
|        - | 7188 | `									/* php ALIASES each collected element to the caller's slot:` |
|        - | 7189 | ``									 * `function f(&...$xs){ $xs[0] = 'A'; }` writes back, and`` |
|        - | 7190 | ``									 * var_dump($xs) inside the callee shows `&int(1)`. Copying`` |
|        - | 7191 | `									 * them left every actual untouched. The node counts as a` |
|        - | 7192 | `									 * holder of the caller's slot, so the frame teardown that` |
|        - | 7193 | `									 * destroys the variadic array gives the hold back. */` |
|       42 | 7194 | `									PH7_HashmapInsertByRef(pMap, 0, pArg->nIdx);` |
|       42 | 7195 | `									pArg++;` |
|       42 | 7196 | `									continue;` |
|        - | 7197 | `								}` |
|        2 | 7198 | `							}` |
|     1603 | 7199 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|     1603 | 7200 | `							pArg++;` |
|        5 | 7201 | `						}` |
|        - | 7202 | `					}` |
|      239 | 7203 | `					sArg.nIdx = nVariadicIdx; /* pObj may be stale here (aMemObj realloc) — use the saved index */` |
|      239 | 7204 | `					sArg.pUserData = 0;` |
|      239 | 7205 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      117 | 7206 | `				}` |
|      239 | 7207 | `				break; /* All remaining args consumed */` |
|        - | 7208 | `			}` |
|   237049 | 7209 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 7210 | `				/* An explicit null is NOT redirected to the default (PHP applies a` |
|        - | 7211 | `				 * default only for an omitted arg); it falls through to the type check` |
|        - | 7212 | `				 * below — TypeError for a non-nullable typed param, kept as null for a` |
|        - | 7213 | ``				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE`` |
|        - | 7214 | `				 * at compile time so its check accepts null. */` |
|        - | 7215 | `				/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 7216 | `				 * coercion and whole-real materialization in place — nullable` |
|        - | 7217 | `				 * types (?type) let null through): the shared per-argument` |
|        - | 7218 | `				 * helper, one implementation for both OP_CALL paths and the` |
|        - | 7219 | `				 * generator/fiber binder (§7.1(f) fold). */` |
|   235313 | 7220 | `				iArgPreFlags = pArg->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|   235313 | 7221 | `				rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pArg,bCallIsStrict,pSelfHint);` |
|   235313 | 7222 | `				if( rc != SXRET_OK ){` |
|      283 | 7223 | `					if( rc == PH7_ABORT ){` |
|        6 | 7224 | `						goto Abort;` |
|        - | 7225 | `					}` |
|        - | 7226 | `					/* Skip function body, route through normal cleanup */` |
|      279 | 7227 | `					PH7_MemObjRelease(pTos);` |
|      279 | 7228 | `					pTos = &pTos[-nCallArgs];` |
|      279 | 7229 | `					pFrameStack = 0;` |
|      279 | 7230 | `					rc = PH7_EXCEPTION;` |
|      279 | 7231 | `					goto SkipFuncBody;` |
|        - | 7232 | `				}` |
|   235035 | 7233 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7234 | `					/* Pass by reference */` |
|     6665 | 7235 | `					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){` |
|        - | 7236 | `						/* php 8.1: $GLOBALS cannot be passed by reference —` |
|        - | 7237 | `						 * a catchable Error with php's exact wording. */` |
|        - | 7238 | `						SyBlob sMsg;` |
|        3 | 7239 | `						SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 7240 | `						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|        2 | 7241 | `							&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        3 | 7242 | `						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 7243 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 7244 | `							goto Abort;` |
|        - | 7245 | `						}` |
|        3 | 7246 | `						PH7_MemObjRelease(pTos);` |
|        3 | 7247 | `						pTos = &pTos[-nCallArgs];` |
|        3 | 7248 | `						pFrameStack = 0;` |
|        3 | 7249 | `						rc = PH7_EXCEPTION;` |
|        3 | 7250 | `						goto SkipFuncBody;` |
|        - | 7251 | `					}` |
|     6663 | 7252 | `					if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)n,pArg) ){` |
|        - | 7253 | `						/* php's refusal, decided from the argument's compile-time SHAPE (the` |
|        - | 7254 | `						 * companion of the named-argument binder above; see VmArgRefusedByRef). */` |
|        - | 7255 | `						sxi32 rcRef;` |
|     6028 | 7256 | `						rcRef = VmThrowByRefRefusal(&(*pVm),` |
|     4018 | 7257 | `							(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|     4018 | 7258 | `							&pVmFunc->sName,(sxu32)(n+1),&aFormalArg[n].sName);` |
|     4019 | 7259 | `						if( rcRef == PH7_ABORT ){` |
|      ! 0 | 7260 | `							goto Abort;` |
|        - | 7261 | `						}` |
|        - | 7262 | `						/* Route the throw like every other binder refusal: release the result` |
|        - | 7263 | `						 * slot, pop the actuals and let SkipFuncBody finish the call. Returning` |
|        - | 7264 | `						 * from here walked out of the dispatch loop with the callee's frame and` |
|        - | 7265 | `						 * stack still live, so a CAUGHT refusal silently abandoned every` |
|        - | 7266 | `						 * statement after the catch. */` |
|     4019 | 7267 | `						PH7_MemObjRelease(pTos);` |
|     4019 | 7268 | `						pTos = &pTos[-nCallArgs];` |
|     4019 | 7269 | `						pFrameStack = 0;` |
|     4019 | 7270 | `						rc = PH7_EXCEPTION;` |
|     4019 | 7271 | `						goto SkipFuncBody;` |
|        - | 7272 | `					}` |
|     2645 | 7273 | `					PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)n,pArg);` |
|     2645 | 7274 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - | 7275 | `						/* Nothing to alias: pass by value. */` |
|       47 | 7276 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       25 | 7277 | `					}else{` |
|        - | 7278 | `						SyHashEntry *pRefEntry;` |
|        - | 7279 | `						/* The declared type's conversion is what the reference holds. */` |
|     2601 | 7280 | `						PH7_VmByRefArgWriteBack(&(*pVm),pArg,iArgPreFlags);` |
|        - | 7281 | `						/* Install the referenced variable in the private function frame */` |
|     2601 | 7282 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|     2601 | 7283 | `						if( pRefEntry == 0 ){` |
|     3899 | 7284 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|     2596 | 7285 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|     2601 | 7286 | `							sArg.nIdx = pArg->nIdx;` |
|     2601 | 7287 | `							sArg.pUserData = 0;` |
|     2601 | 7288 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     1298 | 7289 | `						}` |
|     2601 | 7290 | `						pObj = 0;` |
|        - | 7291 | `					}` |
|     1325 | 7292 | `				}else{` |
|        - | 7293 | `					/* Pass by value,make a copy of the given argument */` |
|   228375 | 7294 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 7295 | `				}` |
|   115934 | 7296 | `			}else{` |
|        - | 7297 | `				char zName[32];` |
|        - | 7298 | `				SyString sArgName;` |
|        - | 7299 | `				/* Set a dummy name */` |
|     1741 | 7300 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|     1741 | 7301 | `				sArgName.zString = zName;` |
|        - | 7302 | `				/* Annonymous argument */` |
|     1741 | 7303 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 7304 | `			}` |
|   232751 | 7305 | `			if( pObj ){` |
|   230155 | 7306 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 7307 | `				/* Insert argument index  */` |
|   230155 | 7308 | `				sArg.nIdx = pObj->nIdx;` |
|   230155 | 7309 | `				sArg.pUserData = 0;` |
|   230155 | 7310 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|   115499 | 7311 | `			}` |
|   232751 | 7312 | `			PH7_MemObjRelease(pArg);` |
|   232751 | 7313 | `			pArg++;` |
|   232751 | 7314 | `			++n;` |
|        5 | 7315 | `		}` |
|        - | 7316 | `		} /* end named vs positional branch */` |
|        - | 7317 | `		/* Set up closure environment */` |
|   744159 | 7318 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 7319 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 7320 | `			ph7_value *pValue;` |
|        - | 7321 | `			sxu32 iEnv;` |
|     8449 | 7322 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|    18833 | 7323 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|    10389 | 7324 | `				pEnv = &aEnv[iEnv];` |
|    10389 | 7325 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 7326 | `					/* Do not install null value */` |
|     8291 | 7327 | `					continue;` |
|        - | 7328 | `				}` |
|     2098 | 7329 | `				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       13 | 7330 | `				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|        - | 7331 | `					/* The Closure instance carries an explicit bound $this` |
|        - | 7332 | `					 * (bindTo/bind/call): it wins over the creation-time` |
|        - | 7333 | `					 * captured $this, php-exact. */` |
|        7 | 7334 | `					continue;` |
|        - | 7335 | `				}` |
|     2097 | 7336 | `				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|        - | 7337 | `					/* Captured by reference: link the name to the shared slot` |
|        - | 7338 | `					 * (no copy), mirroring the by-ref argument install above. */` |
|      258 | 7339 | `					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|      385 | 7340 | `						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),` |
|      254 | 7341 | `							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|      127 | 7342 | `					}` |
|      258 | 7343 | `					continue;` |
|        - | 7344 | `				}` |
|     1843 | 7345 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|     1843 | 7346 | `				if( pValue == 0 ){` |
|      ! 0 | 7347 | `					continue;` |
|        - | 7348 | `				}` |
|        - | 7349 | `				/* Invalidate any prior representation */` |
|     1843 | 7350 | `				PH7_MemObjRelease(pValue);` |
|        - | 7351 | `				/* Duplicate bound variable value */` |
|     1843 | 7352 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|      924 | 7353 | `			}` |
|     4222 | 7354 | `		}` |
|        - | 7355 | `		/* Too-few-arguments check, placed AFTER the passed arguments were` |
|        - | 7356 | `		 * installed and type-checked: php's RECV order means a type error on` |
|        - | 7357 | ``		 * a PASSED argument beats the count error (`f(int $x,$y)` called`` |
|        - | 7358 | `		 * f("str") is a TypeError, not ArgumentCountError). The passed args` |
|        - | 7359 | `		 * were already released by the install loop, so the standard throw` |
|        - | 7360 | `		 * exit leaks nothing. The named path never fires this (its per-hole` |
|        - | 7361 | `		 * check ran in-loop; n == nNonVariadic >= nRequired here).` |
|        - | 7362 | `		 * Builtins written as embedded PHP in the prelude (VM_FUNC_INTERNAL,` |
|        - | 7363 | `		 * with or without VM_FUNC_CLASS_METHOD) are NOT exempt: their declared` |
|        - | 7364 | `		 * signatures were swept against the php 8.5 oracle, so the derived` |
|        - | 7365 | `		 * required count and wording are php's, and VmThrowBuiltinTooFewArgs` |
|        - | 7366 | `		 * words them as php words an internal callable. */` |
|   744159 | 7367 | `		if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 7368 | `			sxu32 nNonVar,nReq;` |
|     4231 | 7369 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     4231 | 7370 | `			if( n < nReq ){` |
|       57 | 7371 | `				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;` |
|       57 | 7372 | `				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|       52 | 7373 | `					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       17 | 7374 | `						nPassed,nReq,SySetUsed(&pVmFunc->aArgs));` |
|       18 | 7375 | `				}else{` |
|       33 | 7376 | `					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       10 | 7377 | `						nPassed,nReq,nNonVar,TRUE);` |
|        - | 7378 | `				}` |
|       57 | 7379 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 7380 | `					goto Abort;` |
|        - | 7381 | `				}` |
|       57 | 7382 | `				PH7_MemObjRelease(pTos);` |
|       57 | 7383 | `				pTos = &pTos[-nCallArgs];` |
|       57 | 7384 | `				pFrameStack = 0;` |
|       57 | 7385 | `				rc = PH7_EXCEPTION;` |
|       57 | 7386 | `				goto SkipFuncBody;` |
|        5 | 7387 | `			}` |
|   742019 | 7388 | `		}else if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) == VM_FUNC_INTERNAL ){` |
|        - | 7389 | `			/* Too-MANY arguments, the mirror php enforces for an internal` |
|        - | 7390 | `			 * callable ("count_chars() expects at most 2 arguments, 3 given").` |
|        - | 7391 | `			 * PHL accepted the extras silently — the surplus only ever reached` |
|        - | 7392 | `			 * func_get_args(). Every formal is filled on this branch, so the call` |
|        - | 7393 | `			 * is over the maximum exactly when more actuals arrived than there are` |
|        - | 7394 | `			 * non-variadic formals; a VARIADIC tail means php declares no maximum` |
|        - | 7395 | `			 * at all, so nNonVar below the formal count is exempt. Prelude` |
|        - | 7396 | `			 * FUNCTIONS only: the chunk-backed class METHODS were not swept against` |
|        - | 7397 | `			 * php's signatures (Fiber::start() is declared argless and reads` |
|        - | 7398 | `			 * func_get_args()). */` |
|        - | 7399 | `			sxu32 nNonVar,nReq;` |
|      430 | 7400 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|      426 | 7401 | `			if( nNonVar == SySetUsed(&pVmFunc->aArgs)` |
|      425 | 7402 | `			 && pFrame->nActualArgs >= 0` |
|      428 | 7403 | `			 && (sxu32)pFrame->nActualArgs > nNonVar ){` |
|       40 | 7404 | `				rc = VmThrowBuiltinTooManyArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       26 | 7405 | `					(sxu32)pFrame->nActualArgs,nNonVar,nReq);` |
|       27 | 7406 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 7407 | `					goto Abort;` |
|        - | 7408 | `				}` |
|       27 | 7409 | `				PH7_MemObjRelease(pTos);` |
|       27 | 7410 | `				pTos = &pTos[-nCallArgs];` |
|       27 | 7411 | `				pFrameStack = 0;` |
|       27 | 7412 | `				rc = PH7_EXCEPTION;` |
|       27 | 7413 | `				goto SkipFuncBody;` |
|        - | 7414 | `			}` |
|      200 | 7415 | `		}` |
|        - | 7416 | `		/* Process default values for remaining formal parameters */` |
|   756023 | 7417 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    12227 | 7418 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 7419 | `				/* Variadic parameter with no extra args — create empty array */` |
|      283 | 7420 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      283 | 7421 | `				if( pObj ){` |
|      283 | 7422 | `					PH7_MemObjToHashmap(pObj);` |
|      283 | 7423 | `					sArg.nIdx = pObj->nIdx;` |
|      283 | 7424 | `					sArg.pUserData = 0;` |
|      283 | 7425 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      139 | 7426 | `				}` |
|      283 | 7427 | `				n++;` |
|      283 | 7428 | `				break; /* Variadic is always last */` |
|        - | 7429 | `			}` |
|    11949 | 7430 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|    11949 | 7431 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|    11949 | 7432 | `				if( pObj ){` |
|        - | 7433 | `					/* Evaluate the default value and extract it's result */` |
|    11949 | 7434 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|    11949 | 7435 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 7436 | `						goto Abort;` |
|        - | 7437 | `					}` |
|        - | 7438 | `					/* Insert argument index */` |
|    11949 | 7439 | `					sArg.nIdx = pObj->nIdx;` |
|    11949 | 7440 | `					sArg.pUserData = 0;` |
|    11949 | 7441 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 7442 | `					/* Make sure the default argument is of the correct type.` |
|        - | 7443 | ``					 * A null default on an implicitly-nullable param (`int $x = null`)`` |
|        - | 7444 | `					 * must stay null — casting it to 0/""/false would diverge from PHP` |
|        - | 7445 | `					 * and contradict the explicit-null path, which now keeps it null. */` |
|    11944 | 7446 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1390 | 7447 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)` |
|      703 | 7448 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 7449 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 7450 | `						/* Cast to the desired type */` |
|      ! 0 | 7451 | `						xCast(pObj);` |
|      ! 0 | 7452 | `					}else{` |
|        - | 7453 | `						/* Mask matched — a const-indirected whole-real default` |
|        - | 7454 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|    11949 | 7455 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 7456 | `					}` |
|     5972 | 7457 | `				}` |
|     5972 | 7458 | `			}` |
|    11949 | 7459 | `			++n;` |
|        5 | 7460 | `		}` |
|        - | 7461 | `		} /* end VmCallArgMap scope */` |
|        - | 7462 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 7463 | `		 * does not return anything.` |
|        - | 7464 | `		 */` |
|   744079 | 7465 | `		PH7_MemObjRelease(pTos);` |
|   744079 | 7466 | `		pTos = &pTos[-nCallArgs];` |
|        - | 7467 | `		/* Allocate an operand stack (via the recycling allocator) and evaluate the` |
|        - | 7468 | `		 * function body. Size it to a tight static bound when the body is statically` |
|        - | 7469 | `		 * modelable (BYTECODE.md stage 7) — the big memory win for deep recursion,` |
|        - | 7470 | `		 * where one such stack lives per frame — falling back to the safe` |
|        - | 7471 | `		 * instruction-count bound otherwise.` |
|        - | 7472 | `		 *` |
|        - | 7473 | `		 * The bound is computed LAZILY on the first call and cached on the func` |
|        - | 7474 | `		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:` |
|        - | 7475 | `		 * self-computing on first use is fail-safe against any body-creation path` |
|        - | 7476 | `		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),` |
|        - | 7477 | `		 * where undersizing is a heap overflow; the amortized cost is one analysis` |
|        - | 7478 | `		 * per function. */` |
|        - | 7479 | `		{` |
|   744079 | 7480 | `			sxu32 nSlots = pVmFunc->nMaxStack;` |
|   744079 | 7481 | `			if( nSlots == 0 ){` |
|    10679 | 7482 | `				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);` |
|    16016 | 7483 | `				sxu32 nTight = VmComputeMaxStack(&(*pVm),` |
|    10674 | 7484 | `					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);` |
|    10679 | 7485 | `				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;` |
|    10679 | 7486 | `				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */` |
|    10679 | 7487 | `				pVmFunc->nMaxStack = nSlots;` |
|     5337 | 7488 | `			}` |
|   744079 | 7489 | `			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);` |
|        - | 7490 | `		}` |
|   744079 | 7491 | `		if( pFrameStack == 0 ){` |
|        - | 7492 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 7493 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 7494 | `				&pVmFunc->sName);` |
|      ! 0 | 7495 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 7496 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7497 | `			}` |
|      ! 0 | 7498 | `			break;` |
|        - | 7499 | `		}` |
|   371825 | 7500 | `SkipFuncBody:` |
|   748553 | 7501 | `		if( pSelf ){` |
|        - | 7502 | `			/* Push class name */` |
|   505193 | 7503 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|   252594 | 7504 | `		}` |
|        - | 7505 | `		/* Increment nesting level */` |
|   748553 | 7506 | `		pVm->nRecursionDepth++;` |
|   748553 | 7507 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 7508 | `			/* Arg-binding threw: there is no body to run — finish the call` |
|        - | 7509 | `			 * immediately (no record is pushed). */` |
|        - | 7510 | `			VmCallRecord sCallee;` |
|     4479 | 7511 | `			sCallee.pVmFunc = pVmFunc;` |
|     4479 | 7512 | `			sCallee.pFrame = pFrame;` |
|     4479 | 7513 | `			sCallee.pFrameStack = pFrameStack;` |
|     4479 | 7514 | `			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|     4479 | 7515 | `			sCallee.nLastRef = SXU32_HIGH;` |
|     4479 | 7516 | `			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|     4479 | 7517 | `			sState.pTos = pTos;` |
|     4479 | 7518 | `			sState.pc = pc;` |
|     4479 | 7519 | `			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);` |
|     4479 | 7520 | `			pTos = sState.pTos;` |
|     4479 | 7521 | `			pc = sState.pc;` |
|     4479 | 7522 | `			if( rc == PH7_ABORT ){` |
|        - | 7523 | `				/* Abort processing immeditaley */` |
|      ! 0 | 7524 | `				goto Abort;` |
|     4479 | 7525 | `			}else if( rc == PH7_SUSPEND ){` |
|      ! 0 | 7526 | `				goto Suspend;` |
|     4479 | 7527 | `			}else if( rc == PH7_EXCEPTION ){` |
|      165 | 7528 | `				goto Exception;` |
|        - | 7529 | `			}` |
|     2162 | 7530 | `		}else{` |
|        - | 7531 | `			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a` |
|        - | 7532 | `			 * call record (caller activation + in-flight call) and switch the` |
|        - | 7533 | `			 * loop's locals to the callee — a PHP->PHP call no longer grows` |
|        - | 7534 | `			 * the native stack. The record node is pool-allocated so` |
|        - | 7535 | `			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */` |
|   744079 | 7536 | `			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   744079 | 7537 | `			if( pRec ){` |
|   742189 | 7538 | `				pVm->pIdleCallFrames = (void *)pRec->pPrev;` |
|   371309 | 7539 | `			}else{` |
|     1895 | 7540 | `				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));` |
|        - | 7541 | `			}` |
|   744079 | 7542 | `			if( pRec == 0 ){` |
|        - | 7543 | `				/* OOM: undo the push-time accounting, tear the call down and` |
|        - | 7544 | `				 * raise the non-catchable fatal (the §3.1 OOM convention —` |
|        - | 7545 | `				 * never a silent NULL). */` |
|      ! 0 | 7546 | `				pVm->nRecursionDepth--;` |
|      ! 0 | 7547 | `				if( pSelf ){` |
|      ! 0 | 7548 | `					(void)SySetPop(&pVm->aSelf);` |
|      ! 0 | 7549 | `				}` |
|      ! 0 | 7550 | `				SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|      ! 0 | 7551 | `				VmLeaveFrame(&(*pVm));` |
|      ! 0 | 7552 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 7553 | `				goto Abort;` |
|        - | 7554 | `			}` |
|   744079 | 7555 | `			sState.pTos = pTos;` |
|   744079 | 7556 | `			sState.pc = pc;` |
|   744079 | 7557 | `			pRec->sCaller = sState;` |
|   744079 | 7558 | `			pRec->sCall.pVmFunc = pVmFunc;` |
|   744079 | 7559 | `			pRec->sCall.pFrame = pFrame;` |
|   744079 | 7560 | `			pRec->sCall.pFrameStack = pFrameStack;` |
|   744079 | 7561 | `			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|   744079 | 7562 | `			pRec->sCall.nLastRef = SXU32_HIGH;` |
|   744079 | 7563 | `			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|   744079 | 7564 | `			pRec->pPrev = pCallTop;` |
|   744079 | 7565 | `			pCallTop = pRec;` |
|        - | 7566 | `			/* Switch to the callee activation (what the recursive` |
|        - | 7567 | `			 * VmByteCodeExec entry used to set up). */` |
|   744079 | 7568 | `			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);` |
|   744079 | 7569 | `			pStack = pFrameStack;` |
|   744079 | 7570 | `			pTos = &pStack[-1];` |
|   744079 | 7571 | `			pc = 0;` |
|   744079 | 7572 | `			sState.aInstr = aInstr;` |
|   744079 | 7573 | `			sState.pStack = pStack;` |
|   744079 | 7574 | `			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */` |
|   744079 | 7575 | `			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */` |
|   744079 | 7576 | `			sState.pTos = pTos;` |
|   744079 | 7577 | `			sState.pc = 0;` |
|   744079 | 7578 | `			sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|   744079 | 7579 | `			sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|   744079 | 7580 | `			sState.pEntryFrame = pVm->pFrame;` |
|   744079 | 7581 | `			sState.pResult = pRec->sCaller.pTos;` |
|   744079 | 7582 | `			sState.pLastRef = &pRec->sCall.nLastRef;` |
|   744079 | 7583 | `			sState.pEnforceRetFunc = VmFuncHasReturnType(pVmFunc) ? pVmFunc : 0;` |
|   744079 | 7584 | `			sState.is_callback = 0;` |
|   744079 | 7585 | `			sState.bReturnPropagates = 0;` |
|   744079 | 7586 | `			goto VmLoopFetch;` |
|        - | 7587 | `		}` |
|     2162 | 7588 | `	}else{` |
|        - | 7589 | `		/* Look for an installed foreign function.` |
|        - | 7590 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 7591 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 7592 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 7593 | `		 * global fallback for unqualified function calls in namespaces. */` |
|  1492633 | 7594 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 7595 | `		{` |
|  1492633 | 7596 | `		VmCallArgMap *pCallMap2 = pEffCallMap;` |
|  1492633 | 7597 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 7598 | `			/* Compiler-qualified: try short name as global fallback */` |
|       57 | 7599 | `			const char *zShort = sName.zString;` |
|        - | 7600 | `			sxu32 i;` |
|      857 | 7601 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      805 | 7602 | `				if( sName.zString[i] == '\\' ){` |
|       71 | 7603 | `					zShort = &sName.zString[i + 1];` |
|       33 | 7604 | `				}` |
|      405 | 7605 | `			}` |
|       57 | 7606 | `			if( zShort != sName.zString ){` |
|       57 | 7607 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       57 | 7608 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       26 | 7609 | `			}` |
|       26 | 7610 | `		}` |
|        - | 7611 | `		} /* end VmCallArgMap namespace scope */` |
|  1492633 | 7612 | `		if( pEntry == 0 ){` |
|        - | 7613 | `			/* php accepts the "Class::method" STATIC-callable string everywhere a` |
|        - | 7614 | `			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).` |
|        - | 7615 | `			 * Split on the LAST "::" (php's own zend_memrchr scan) and route through the` |
|        - | 7616 | `			 * shared array-callable machinery ([class-name, method-name]) instead of` |
|        - | 7617 | `			 * warning undefined. */` |
|   240132 | 7618 | `			const char *zCbCls = 0,*zCbMeth = 0;` |
|   240132 | 7619 | `			sxu32 nCbCls = 0,nCbMeth = 0;` |
|   240132 | 7620 | `			int bScoped = PH7_VmCallableStringParts(sName.zString,sName.nByte,` |
|        - | 7621 | `				&zCbCls,&nCbCls,&zCbMeth,&nCbMeth);` |
|   240132 | 7622 | `			if( bScoped ){` |
|        - | 7623 | `				/* Resolve BEFORE dispatching: the shared dispatcher answers SXRET_OK with` |
|        - | 7624 | `` 				 * a NULL result for a pair it cannot resolve, so `$cb='C::nosuch'; $cb();` `` |
|        - | 7625 | `				 * evaluated to NULL with no diagnostic at all — where php throws the same` |
|        - | 7626 | `				 * Errors the ARRAY form of the same call already raised here. (Its own` |
|        - | 7627 | ``				 * `if( bScoped )` block: this scratch buffer and the dispatch block's`` |
|        - | 7628 | `				 * hashmap are both block-head declarations, and the check runs between` |
|        - | 7629 | `				 * them.) */` |
|        - | 7630 | `				char zSmMsg[192];` |
|   100079 | 7631 | `				sxi32 nSmBrc = pVm->nBoundaryRc;` |
|   100079 | 7632 | `				const void *pSmRes = (const void *)pVm->pResumeFrame;` |
|   150117 | 7633 | `				const char *zSmErr = VmCallableClassMethodError(&(*pVm),` |
|    50038 | 7634 | `					PH7_VmExtractClass(&(*pVm),zCbCls,nCbCls,FALSE,0),` |
|    50038 | 7635 | `					zCbCls,nCbCls,zCbMeth,nCbMeth,TRUE,zSmMsg,sizeof(zSmMsg));` |
|   100079 | 7636 | `				if( zSmErr ){` |
|        - | 7637 | `					sxi32 rcSmErr;` |
|       53 | 7638 | `					int bSmRaised = PH7_VmClassLookupRaised(&(*pVm),nSmBrc,pSmRes);` |
|       53 | 7639 | `					if( pInstr->iP2 ){` |
|      ! 0 | 7640 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 7641 | `					}` |
|       53 | 7642 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 7643 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7644 | `					}` |
|       53 | 7645 | `					PH7_MemObjRelease(pTos);` |
|       53 | 7646 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       53 | 7647 | `					pTos->nIdx = SXU32_HIGH;` |
|       53 | 7648 | `					if( bSmRaised ){` |
|        - | 7649 | `						/* The class name's autoloader threw: land THAT, exactly as the array` |
|        - | 7650 | `						 * form does — php never reports the class missing in this case. */` |
|        6 | 7651 | `						rcSmErr = pVm->nBoundaryRc;` |
|        6 | 7652 | `						pVm->nBoundaryRc = 0;` |
|        6 | 7653 | `						if( rcSmErr == PH7_ABORT ){` |
|      ! 0 | 7654 | `							goto Abort;` |
|        - | 7655 | `						}` |
|        6 | 7656 | `						rc = PH7_EXCEPTION;` |
|       14 | 7657 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7658 | `					}` |
|       48 | 7659 | `					rcSmErr = VmThrowFromVm(&(*pVm),"Error",zSmErr,(sxu32)SyStrlen(zSmErr));` |
|       48 | 7660 | `					if( rcSmErr == SXERR_ABORT ){` |
|      ! 0 | 7661 | `						goto Abort;` |
|        - | 7662 | `					}` |
|       48 | 7663 | `					rc = rcSmErr;` |
|       64 | 7664 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7665 | `				}` |
|    50013 | 7666 | `			}` |
|   240081 | 7667 | `			if( bScoped ){` |
|        - | 7668 | `				/* Resolved: hand the callable STRING itself to the shared dispatcher, which` |
|        - | 7669 | ``				 * decodes `Class::method` the same way (it has to, for the callback-argument`` |
|        - | 7670 | `				 * callers). This used to build a throwaway [class,method] map here and enter` |
|        - | 7671 | `				 * through the hashmap branch — two decoders for one spelling. */` |
|        - | 7672 | `				ph7_value sResult;` |
|        - | 7673 | `				sxi32 rcSm;` |
|   150041 | 7674 | `				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100026 | 7675 | `					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|   100028 | 7676 | `				SySetReset(&aArg);` |
|   100044 | 7677 | `				while( pArg < pTos ){` |
|       17 | 7678 | `					SySetPut(&aArg,(const void *)&pArg);` |
|       17 | 7679 | `					pArg++;` |
|        1 | 7680 | `				}` |
|   100028 | 7681 | `				PH7_MemObjInit(pVm,&sResult);` |
|   150041 | 7682 | `				rcSm = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),` |
|   100026 | 7683 | `					(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|   100028 | 7684 | `				SySetReset(&aArg);` |
|   100028 | 7685 | `				if( nCallArgs > 0 ){` |
|       15 | 7686 | `					VmPopOperand(&pTos,nCallArgs);` |
|        7 | 7687 | `				}` |
|   100028 | 7688 | `				if( rcSm == PH7_ABORT ){` |
|      ! 0 | 7689 | `					PH7_MemObjRelease(&sResult);` |
|      ! 0 | 7690 | `					goto Abort;` |
|        - | 7691 | `				}` |
|   100028 | 7692 | `				if( rcSm == PH7_EXCEPTION ){` |
|        - | 7693 | `					sxi32 iResumePc;` |
|   100004 | 7694 | `					PH7_MemObjRelease(&sResult);` |
|   100004 | 7695 | `					if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100001 | 7696 | `						PH7_MemObjRelease(pTos);` |
|        - | 7697 | `						/* Drain the abandoned outer-expression operands` |
|        - | 7698 | ``						 * (`1 + "C::m"()`) to the try's base — one leaked`` |
|        - | 7699 | `						 * slot per caught throw otherwise. */` |
|   300001 | 7700 | `						PH7_RESUME_DRAIN()` |
|   100001 | 7701 | `						pc = iResumePc;` |
|   100001 | 7702 | `						break;` |
|        - | 7703 | `					}` |
|        3 | 7704 | `					goto Exception;` |
|        - | 7705 | `				}` |
|       25 | 7706 | `				PH7_MemObjStore(&sResult,pTos);` |
|       25 | 7707 | `				PH7_MemObjRelease(&sResult);` |
|       25 | 7708 | `				break;` |
|        - | 7709 | `			}` |
|        - | 7710 | `			/* Call to an undefined function is a catchable Error in php 8 — it does` |
|        - | 7711 | `			 * NOT warn and hand back null and carry on, which is what PH7 did (and` |
|        - | 7712 | `			 * which quietly turned a typo into a null-propagating program). */` |
|        - | 7713 | `			{` |
|        - | 7714 | `			SyBlob sMsg;` |
|   140055 | 7715 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   140055 | 7716 | `			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);` |
|        - | 7717 | `			/* Consume this call's captured spread runs so they don't leak into a` |
|        - | 7718 | `			 * later call (this path never reaches VmBuildEffectiveArgMap). */` |
|   140055 | 7719 | `			if( pInstr->iP2 ){` |
|      ! 0 | 7720 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 7721 | `			}` |
|        - | 7722 | `			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the` |
|        - | 7723 | `			 * compile-time arg count on the stack, and this early exit skips the` |
|        - | 7724 | `			 * arg-building loop that the normal path uses, so popping only iP1 would` |
|        - | 7725 | `			 * strand the expanded elements and corrupt the enclosing expression. */` |
|   140055 | 7726 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 7727 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7728 | `			}` |
|   140055 | 7729 | `			PH7_MemObjRelease(pTos);` |
|   210081 | 7730 | `			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|    70026 | 7731 | `				SyBlobLength(&sMsg));` |
|   140055 | 7732 | `			SyBlobRelease(&sMsg);` |
|   140055 | 7733 | `			if( rc == SXERR_ABORT ){` |
|        6 | 7734 | `				goto Abort;` |
|        - | 7735 | `			}` |
|        - | 7736 | `			/* A catch may have run IN PLACE inside VmThrowFromVm (SXRET_OK +` |
|        - | 7737 | ``			 * recorded resume). An unconditional `goto Exception` here unwound the`` |
|        - | 7738 | `` 			 * exec ANYWAY, so `try { nosuchfn(); } catch (Error $e) {} rest();` `` |
|        - | 7739 | `			 * ran the catch and then silently dropped the rest of the script` |
|        - | 7740 | `			 * (exit 0). Route like every other in-exec throw site. */` |
|   380094 | 7741 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7742 | `			}` |
|        - | 7743 | `		}` |
|  1252505 | 7744 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 7745 | `		/* D1: resolve deferred plain-var arguments against the builtin's by-ref position` |
|        - | 7746 | `		 * mask (derived from its signature). A by-ref out-param (preg_match's $matches, …)` |
|        - | 7747 | `		 * is materialized so PH7_VmStoreArgByRef can write back — this is what keeps a` |
|        - | 7748 | ``		 * DYNAMIC-name by-ref builtin (`$f='preg_match'; $f($p,$s,$m)`) working now that the`` |
|        - | 7749 | `		 * compile-time mask no longer sees it. Every other (by-value) arg warns + passes` |
|        - | 7750 | `		 * NULL, which is also what call_user_func & friends want. pVm->pFrame is the caller. */` |
|        - | 7751 | `		{` |
|  1252505 | 7752 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,pFunc->nByRefMask,0,0,pEffCallMap);` |
|  1252521 | 7753 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 7754 | `		}` |
|        - | 7755 | `		/* Host function (builtin): build the effective spread-key map so the` |
|        - | 7756 | `		 * name-forwarding builtins (call_user_func & friends) relay string keys as` |
|        - | 7757 | `		 * named args, and — critically — so this call's captured runs are consumed.` |
|        - | 7758 | `		 * pArg is the top base here (a builtin call pops no method-name slot). */` |
|  1879535 | 7759 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  1252486 | 7760 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|  1370446 | 7761 | `NativeCall:` |
|        - | 7762 | `		/* A VM_FUNC_NATIVE method joins here, having done the two steps above for` |
|        - | 7763 | `		 * itself: its by-ref mask comes from the same signature machinery, and its` |
|        - | 7764 | `		 * effective arg map was already built (and this call's spread runs already` |
|        - | 7765 | `		 * consumed) on the method path — building it a second time here would` |
|        - | 7766 | `		 * consume them twice. Everything from this point down is shared verbatim:` |
|        - | 7767 | `		 * a native method IS a host call that happens to carry a receiver. */` |
|        - | 7768 | `		/* Start collecting function arguments */` |
|  2742502 | 7769 | `		SySetReset(&aArg);` |
|  6395684 | 7770 | `		while( pArg < pTos ){` |
|  3653187 | 7771 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  3653187 | 7772 | `			pArg++;` |
|        5 | 7773 | `		}` |
|        - | 7774 | `		/* Assume a null return value */` |
|  2742502 | 7775 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 7776 | `		/* Init the call context */` |
|  2742502 | 7777 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 7778 | `		/* Hand the call-site named-argument map to the builtin so name-forwarding` |
|        - | 7779 | `		 * helpers (call_user_func & friends) can relay name: arguments — and the` |
|        - | 7780 | `		 * caller's strict_types mode — to the inner callback. Forwarded whole (not` |
|        - | 7781 | `		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even` |
|        - | 7782 | `		 * when its own call site is purely positional; only the two forwarding` |
|        - | 7783 | `		 * builtins read pArgMap, so this is inert for every other host function. */` |
|  2742502 | 7784 | `		sCtx.pArgMap = pEffCallMap;` |
|        - | 7785 | `		/* Receiver + late-static-binding class for a native METHOD. Both stay 0 for a` |
|        - | 7786 | `		 * plain host function (the locals are initialized once per OP_CALL), so this` |
|        - | 7787 | `		 * is inert on the builtin path. The reference on pNativeOwned belongs to the` |
|        - | 7788 | `		 * caller for the span of the call — the native body borrows it and must not` |
|        - | 7789 | `		 * unref, exactly as a bytecode method's frame $this is borrowed. */` |
|  2742502 | 7790 | `		sCtx.pThis = pNativeRecv;` |
|  2742502 | 7791 | `		sCtx.pCalledClass = pNativeClass;` |
|        - | 7792 | `		{` |
|  2742502 | 7793 | `		int nGiven = (int)SySetUsed(&aArg);` |
|        - | 7794 | ``		/* Bind `name:` arguments to the callee's declared POSITIONS before anything`` |
|        - | 7795 | `		 * reads the vector — the arity screen, the ZPP screen and the C body all take` |
|        - | 7796 | `		 * it positionally. A host function has no compiled parameter records for` |
|        - | 7797 | `		 * VmResolveNamedArgs to walk, so its signature string is the source of names` |
|        - | 7798 | `		 * and defaults (PH7_VmBindNamedArgsToSig). Without this every named argument` |
|        - | 7799 | `		 * simply stayed where it was WRITTEN. */` |
|  2742502 | 7800 | `		if( pEffCallMap && pEffCallMap->bHasNamed && nGiven > 0 ){` |
|      143 | 7801 | `			rc = PH7_VmBindNamedArgsToSig(&sCtx,pFunc,pEffCallMap,&nGiven,` |
|       94 | 7802 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|       96 | 7803 | `			if( rc != SXRET_OK ){` |
|        9 | 7804 | `				goto NativeCallDone;` |
|        - | 7805 | `			}` |
|       43 | 7806 | `		}` |
|        - | 7807 | `		/* php binds a by-reference argument at the CALL, before the callee runs, so a` |
|        - | 7808 | ``		 * non-variable in a `&` position is refused ahead of every ZPP check — and`` |
|        - | 7809 | ``		 * ahead of the too-MANY-arguments one (`array_pop([1,2],5)` is the reference`` |
|        - | 7810 | `		 * Error in php, not an ArgumentCountError). With no argument at all there is` |
|        - | 7811 | `		 * nothing to refuse, which is why the too-FEW check below still speaks first` |
|        - | 7812 | ``		 * for `array_pop()`. */`` |
|  4114541 | 7813 | `		rc = PH7_VmScreenByRefArgShapes(&sCtx,pFunc,pEffCallMap,nGiven,` |
|  2742489 | 7814 | `			(ph7_value **)SySetBasePtr(&aArg));` |
|  2742494 | 7815 | `		if( rc != SXRET_OK ){` |
|       55 | 7816 | `			goto NativeCallDone;` |
|        - | 7817 | `		}` |
|        - | 7818 | `		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum` |
|        - | 7819 | `		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError` |
|        - | 7820 | `		 * before the C routine runs when called with too few arguments — instead` |
|        - | 7821 | `		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""` |
|        - | 7822 | `		 * return. The message wording matches php's ZPP output byte-for-byte. */` |
|  4114466 | 7823 | `		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){` |
|      848 | 7824 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 7825 | `				"%z() expects %s %d argument%s, %d given",` |
|      281 | 7826 | `				&pFunc->sName,` |
|      562 | 7827 | `				pFunc->bAtLeast ? "at least" : "exactly",` |
|      562 | 7828 | `				(int)pFunc->nMinArg,` |
|      562 | 7829 | `				pFunc->nMinArg == 1 ? "" : "s",` |
|      281 | 7830 | `				nGiven);` |
|  2742163 | 7831 | `		}else if( pFunc->bHasMaxArg && nGiven > (int)pFunc->nMaxArg ){` |
|        - | 7832 | `			/* php enforces the MAXIMUM as well, and PHL only did so where a` |
|        - | 7833 | `			 * builtin happened to hand-roll the check (51 of ~650), so` |
|        - | 7834 | ``			 * `microtime(1,2)`, `strlen("a","b")` and friends silently ignored`` |
|        - | 7835 | `			 * the extras. The count comes from the same signature table as the` |
|        - | 7836 | `			 * minimum; a variadic tail leaves nMaxArg at -1 and is exempt.` |
|        - | 7837 | `			 * php says "exactly" when the bounds coincide, "at most" otherwise. */` |
|      218 | 7838 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 7839 | `				"%z() expects %s %d argument%s, %d given",` |
|       71 | 7840 | `				&pFunc->sName,` |
|      117 | 7841 | `				((int)pFunc->nMinArg == (int)pFunc->nMaxArg && !pFunc->bAtLeast) ? "exactly" : "at most",` |
|      142 | 7842 | `				(int)pFunc->nMaxArg,` |
|      142 | 7843 | `				pFunc->nMaxArg == 1 ? "" : "s",` |
|       71 | 7844 | `				nGiven);` |
|  4113481 | 7845 | `		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,` |
|  2741735 | 7846 | `			(ph7_value **)SySetBasePtr(&aArg))) ){` |
|        - | 7847 | `			/* TypeError thrown: rc carries the caught/uncaught status */` |
|      486 | 7848 | `		}else{` |
|        - | 7849 | `			/* Call the foreign function */` |
|  2740778 | 7850 | `			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));` |
|        - | 7851 | `			/* A host function that RAISED a catchable throw (PH7_VmThrowException)` |
|        - | 7852 | `			 * and still returned PH7_OK reports it here — the throw's catch has` |
|        - | 7853 | `			 * already run in place, so treating the call as a normal return would` |
|        - | 7854 | `			 * resume execution INSIDE the try body the throw abandoned (and, when` |
|        - | 7855 | `			 * uncaught, run on past the reported fatal). The status is recorded on` |
|        - | 7856 | `			 * the call context, so this covers the shared validation helpers whose` |
|        - | 7857 | `			 * callers have no channel to thread a status back. */` |
|  2740778 | 7858 | `			rc = VmHostFuncThrowRc(&sCtx,rc);` |
|        - | 7859 | `		}` |
|  1370446 | 7860 | `NativeCallDone:` |
|  1372051 | 7861 | `		(void)nGiven; /* the named-arg binder's early exit lands here */` |
|        - | 7862 | `		}` |
|        - | 7863 | `		/* Release the call context */` |
|  2742502 | 7864 | `		VmReleaseCallContext(&sCtx);` |
|  2742502 | 7865 | `		if( pNativeOwned ){` |
|        - | 7866 | `			/* Drop the receiver reference the method branch took (pThis->iRef++ before` |
|        - | 7867 | `			 * the target slot was released). A bytecode method hands this to its frame` |
|        - | 7868 | `			 * and lets the teardown do it; a native method has no frame, so it is` |
|        - | 7869 | `			 * dropped here — the one point EVERY route out of the call passes through,` |
|        - | 7870 | `			 * including the throw/abort/suspend ones below. Ordered after the context` |
|        - | 7871 | `			 * release because sCtx.sThis aliases the instance. Always 0 for a plain` |
|        - | 7872 | `			 * host function. */` |
|  1488944 | 7873 | `			PH7_ClassInstanceUnref(pNativeOwned);` |
|  1488944 | 7874 | `			pNativeOwned = 0;` |
|  1488944 | 7875 | `			pNativeRecv = 0;` |
|   744471 | 7876 | `		}` |
|  2742502 | 7877 | `		if( rc == PH7_ABORT ){` |
|        - | 7878 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 7879 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 7880 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      601 | 7881 | `			PH7_MemObjRelease(&sRet);` |
|      601 | 7882 | `			goto Abort;` |
|        - | 7883 | `		}` |
|  2741906 | 7884 | `		if( rc != PH7_SUSPEND && pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 7885 | `			/* A throw raised inside this host function — directly` |
|        - | 7886 | `			 * (PH7_VmThrowException) or by a PHP callback it invoked — was` |
|        - | 7887 | `			 * caught by an INLINE try (generator body) THIS exec owns.` |
|        - | 7888 | `			 * VmThrowInline records only a pc-redirect: a direct builtin throw` |
|        - | 7889 | `			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION` |
|        - | 7890 | `			 * with the redirect pending, so the rc branches below never land` |
|        - | 7891 | `			 * it (pre-existing hole: explode("") or a throwing usort` |
|        - | 7892 | `			 * comparator inside a generator's try lost the catch AND the` |
|        - | 7893 | `			 * yield). Land at the redirect now — its drain to the try's` |
|        - | 7894 | `			 * operand base subsumes the args + name pops. */` |
|       10 | 7895 | `			PH7_MemObjRelease(&sRet);` |
|       32 | 7896 | `			PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 7897 | `		}` |
|  2741898 | 7898 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 7899 | `			/* A callback invoked by this host function threw. If an in-place catch` |
|        - | 7900 | `			 * recorded a resume target owned by THIS body, resume at its landing pad` |
|        - | 7901 | `			 * (consuming the target); otherwise the exception was caught by an outer` |
|        - | 7902 | `			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW` |
|        - | 7903 | `			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which` |
|        - | 7904 | `			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */` |
|        - | 7905 | `			sxi32 iResumePc;` |
|     7589 | 7906 | `			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 7907 | `				/* Caught by an outer exec, or not caught here: propagate. */` |
|     1675 | 7908 | `				goto Exception;` |
|        - | 7909 | `			}` |
|        - | 7910 | `			/* Exception was caught in place by THIS body's try: pop args and the` |
|        - | 7911 | `			 * result slot, then drain any abandoned outer-expression operands to` |
|        - | 7912 | `			 * the try's base and resume. */` |
|     5919 | 7913 | `			PH7_MemObjRelease(&sRet);` |
|     5919 | 7914 | `			if( nCallArgs > 0 ){` |
|     5475 | 7915 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|     2735 | 7916 | `			}` |
|     5919 | 7917 | `			VmPopOperand(&pTos,1);` |
|    10093 | 7918 | `			PH7_RESUME_DRAIN()` |
|     5919 | 7919 | `			pc = iResumePc;` |
|     5919 | 7920 | `			break;` |
|        - | 7921 | `		}` |
|  2734314 | 7922 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 7923 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 7924 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 7925 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 7926 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 7927 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 7928 | `			 * body), the user-function path above will handle re-saving. */` |
|      363 | 7929 | `			PH7_MemObjRelease(&sRet);` |
|      363 | 7930 | `			if( nCallArgs > 0 ){` |
|      357 | 7931 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      176 | 7932 | `			}` |
|        - | 7933 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 7934 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|      363 | 7935 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|      363 | 7936 | `			goto Suspend;` |
|        - | 7937 | `		}` |
|  2733956 | 7938 | `		if( nCallArgs > 0 ){` |
|        - | 7939 | `			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack` |
|        - | 7940 | `			 * expanded the compile-time arg count on the stack, so popping iP1` |
|        - | 7941 | `			 * strands the extra elements (or, for an unpack that expanded to fewer` |
|        - | 7942 | `			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,` |
|        - | 7943 | `			 * reading a bogus value whose stray flags sent MemObjStore into the` |
|        - | 7944 | `			 * hashmap-release path and hung). Mirrors every other CALL exit. The` |
|        - | 7945 | `			 * function-name slot (pTos) receives the return value below. */` |
|  2685973 | 7946 | `			VmPopOperand(&pTos,nCallArgs);` |
|  1343785 | 7947 | `		}` |
|        - | 7948 | `		/* Save foreign function return value into the (now top) function-name slot */` |
|  2733956 | 7949 | `		PH7_MemObjStore(&sRet,pTos);` |
|        - | 7950 | `		/* ...and clear that slot's index. It is one of the call's own argument slots,` |
|        - | 7951 | `		 * still carrying the variable index the argument was loaded with, and` |
|        - | 7952 | `		 * PH7_MemObjStore does not touch nIdx — so a builtin's return value came back` |
|        - | 7953 | ``		 * looking like an lvalue for the caller's variable (`f(strtoupper($b))` with`` |
|        - | 7954 | ``		 * `function f(&$x)` overwrote `$b`). No host function returns by reference. */`` |
|  2733956 | 7955 | `		pTos->nIdx = SXU32_HIGH;` |
|  2733956 | 7956 | `		PH7_MemObjRelease(&sRet);` |
|        - | 7957 | `	}` |
|  2738270 | 7958 | `	break;` |
|        - | 7959 | `				  }` |
|        - | 7960 | `/*` |
|        - | 7961 | ` * OP_CONSUME: P1 * *` |
|        - | 7962 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 7963 | ` */` |
|    48007 | 7964 | `case PH7_OP_CONSUME: {` |
|        - | 7965 | `	VmOpRc rcOp;` |
|    96019 | 7966 | `	sState.pTos = pTos;` |
|    96019 | 7967 | `	sState.pc = pc;` |
|    96019 | 7968 | `	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);` |
|    96019 | 7969 | `	pTos = sState.pTos;` |
|    96019 | 7970 | `	pc = sState.pc;` |
|    96019 | 7971 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 7972 | `		goto Abort;` |
|    96017 | 7973 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       14 | 7974 | `		goto Exception;` |
|        - | 7975 | `	}` |
|    96000 | 7976 | `	break;` |
|        - | 7977 | `					  }` |
|        - | 7978 |  |
|        - | 7979 | `		} /* Switch() */` |
| 40561229 | 7980 | `		pc++; /* Next instruction in the stream */` |
|        5 | 7981 | `	} /* For(;;) */` |
|  1604862 | 7982 | `Done:` |
|        - | 7983 | `	/* A stacked callee completing lands here too (its result is already in` |
|        - | 7984 | `	 * sState.pResult — the caller's operand slot); Unwind's first iteration` |
|        - | 7985 | `	 * bottoms out identically for the record-less case. */` |
|  3210153 | 7986 | `	rc = SXRET_OK;` |
|  3210153 | 7987 | `	goto Unwind;` |
|      840 | 7988 | `Suspend:` |
|     1685 | 7989 | `	rc = PH7_SUSPEND;` |
|     1685 | 7990 | `	if( pCallTop != 0 ){` |
|        - | 7991 | `		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment` |
|        - | 7992 | `		 * instead of the lossy unwind. pc/nTos of the innermost activation were` |
|        - | 7993 | `		 * already saved into the ctx by VmSuspendCtx; capture the rest (the` |
|        - | 7994 | `		 * record chain, the innermost activation, the suspend-time top frame)` |
|        - | 7995 | `		 * so resume re-enters HERE, inside the innermost callee, like php. The` |
|        - | 7996 | `		 * records / frames / operand stacks stay alive — nothing is freed. Only` |
|        - | 7997 | `		 * fibers reach this (generators yield only at their body level, pCallTop` |
|        - | 7998 | `		 * == 0); a suspend inside a C->PHP callback was already rejected with a` |
|        - | 7999 | `		 * FiberError before it could arrive here. */` |
|      306 | 8000 | `		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));` |
|      306 | 8001 | `		if( pSeg == 0 ){` |
|        - | 8002 | `			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST` |
|        - | 8003 | `			 * pc/nTos into the ctx, so the lossy body-level fallback would resume` |
|        - | 8004 | `			 * the callee's pc against the body stack — silent corruption, exactly` |
|        - | 8005 | `			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the` |
|        - | 8006 | `			 * §3.1 convention shared with the stage-2 record-alloc OOM site). */` |
|      ! 0 | 8007 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 8008 | `			rc = PH7_ABORT;` |
|      ! 0 | 8009 | `			goto Unwind;` |
|        - | 8010 | `		}` |
|      306 | 8011 | `		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */` |
|      306 | 8012 | `		pSeg->sState = sState;` |
|      306 | 8013 | `		pSeg->pCallTop = pCallTop;` |
|      306 | 8014 | `		pSeg->pTopFrame = pVm->pFrame;` |
|      306 | 8015 | `		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;` |
|      306 | 8016 | `		pSeg->nOldFinBase = pVm->pActiveCtx ? pVm->pActiveCtx->nFinallyBase : 0;` |
|        - | 8017 | `		{` |
|        - | 8018 | `			VmCallFrame *pRec;` |
|      306 | 8019 | `			pSeg->nRecords = 0;` |
|      608 | 8020 | `			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){` |
|      306 | 8021 | `				pSeg->nRecords++;` |
|      155 | 8022 | `			}` |
|        - | 8023 | `		}` |
|      306 | 8024 | `		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;` |
|        - | 8025 | `		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */` |
|      306 | 8026 | `		SySetRelease(&aArg);` |
|      306 | 8027 | `		return PH7_SUSPEND;` |
|        - | 8028 | `	}` |
|     1383 | 8029 | `	goto Unwind;` |
|      407 | 8030 | `Abort:` |
|      819 | 8031 | `	rc = PH7_ABORT;` |
|      819 | 8032 | `	goto Unwind;` |
|   301761 | 8033 | `Exception:` |
|   603527 | 8034 | `	rc = PH7_EXCEPTION;` |
|   603522 | 8035 | `	goto Unwind;` |
|  1907719 | 8036 | `Unwind:` |
|        - | 8037 | `	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration` |
|        - | 8038 | `	 * finishes the top record exactly as the old per-level native return did:` |
|        - | 8039 | `	 * for ABORT/EXCEPTION, first run what the popped activation's own` |
|        - | 8040 | `	 * Abort/Exception label used to do (clear its pending return, release its` |
|        - | 8041 | `	 * operands — a stacked activation never has bReturnPropagates set), then` |
|        - | 8042 | `	 * VmCallFinish routes in the restored caller (an in-place catch there` |
|        - | 8043 | `	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —` |
|        - | 8044 | `	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible` |
|        - | 8045 | `	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).` |
|        - | 8046 | `	 * At the bottom, VmExecFinalize hands the status to the native caller. */` |
|  2108556 | 8047 | `	for(;;){` |
|  4216693 | 8048 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|        - | 8049 | `			/* Drop any pending hook-RMW write-backs this activation armed — its` |
|        - | 8050 | `			 * statement is abandoned (only the innermost activation at throw time` |
|        - | 8051 | `			 * can own entries: the armed window spans exactly one instruction, so` |
|        - | 8052 | `			 * no OP_CALL record ever intervenes). */` |
|  1005166 | 8053 | `			while( SySetUsed(&pVm->aHookRmw) > 0` |
|  1005167 | 8054 | `			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){` |
|      ! 0 | 8055 | `				VmHookRmwDropTop(&(*pVm));` |
|      ! 0 | 8056 | `			}` |
|   502581 | 8057 | `		}` |
|  4216693 | 8058 | `		if( pCallTop == 0 ){` |
|  3472817 | 8059 | `			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);` |
|        - | 8060 | `		}` |
|   743881 | 8061 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|   603479 | 8062 | `			VmClearFramePending(sState.pEntryFrame);` |
|   608995 | 8063 | `			while( pTos >= pStack ){` |
|     5521 | 8064 | `				PH7_MemObjRelease(pTos);` |
|     5521 | 8065 | `				pTos--;` |
|        5 | 8066 | `			}` |
|   301737 | 8067 | `		}` |
|   743881 | 8068 | `		if( rc != PH7_SUSPEND ){` |
|        - | 8069 | `			/* The finishing callee's own leaked finally actions must not survive` |
|        - | 8070 | `			 * into the caller (whose next OP_END_FINALLY would mis-pop them). */` |
|   743879 | 8071 | `			VmDiscardFinallyActions(&(*pVm),sState.nFinallyActBase);` |
|   372149 | 8072 | `		}` |
|        - | 8073 | `		{` |
|   743881 | 8074 | `			VmCallFrame *pRec = pCallTop;` |
|   743881 | 8075 | `			sState = pRec->sCaller;` |
|   743881 | 8076 | `			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);` |
|   743881 | 8077 | `			pCallTop = pRec->pPrev;` |
|   743881 | 8078 | `			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   743881 | 8079 | `			pVm->pIdleCallFrames = (void *)pRec;` |
|   743881 | 8080 | `			aInstr = sState.aInstr;` |
|   743881 | 8081 | `			pStack = sState.pStack;` |
|   743881 | 8082 | `			pTos = sState.pTos;` |
|   743881 | 8083 | `			pc = sState.pc;` |
|        - | 8084 | `		}` |
|   743881 | 8085 | `		if( rc == PH7_OK ){` |
|   343051 | 8086 | `			pc++; /* the loop-bottom increment this OP_CALL missed */` |
|   343051 | 8087 | `			goto VmLoopFetch;` |
|        - | 8088 | `		}` |
|        5 | 8089 | `	}` |
|  1736561 | 8090 | `}` |
|        - | 8091 |  |
