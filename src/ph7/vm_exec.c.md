# src/ph7/vm_exec.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3854/4304 lines (89.54%)

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
|      932 |   45 | `static int VmGrowOperandStack(ph7_vm *pVm, sxu32 nNeed,` |
|        - |   46 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |   47 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        4 |   48 | `{` |
|      936 |   49 | `	ph7_value *pOld = *ppStack;` |
|      936 |   50 | `	sxu32 nOldCap = pState->nStackCap;` |
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
|      936 |   64 | `	nReq = nNeed + pState->nStackOrig;` |
|      936 |   65 | `	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */` |
|      ! 0 |   66 | `		nReq = SXU32_HIGH;` |
|      ! 0 |   67 | `	}` |
|        - |   68 | `	/* SyMemBackendRealloc's size argument is sxu32, so the byte count` |
|        - |   69 | `	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack` |
|        - |   70 | `	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and` |
|        - |   71 | `	 * the init loop below would run off it. If the true requirement exceeds the` |
|        - |   72 | `	 * representable cap, treat it as OOM (the caller raises the guard error). */` |
|      936 |   73 | `	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);` |
|      936 |   74 | `	if( nReq > nMaxCap ){` |
|      ! 0 |   75 | `		return 0;` |
|        - |   76 | `	}` |
|      936 |   77 | `	if( nReq <= nOldCap ){` |
|      582 |   78 | `		return 1; /* already fits — no growth needed */` |
|        - |   79 | `	}` |
|      358 |   80 | `	nNewCap = nReq;` |
|        - |   81 | `	/* Amortize repeated spreads in one argument list: at least double, but never` |
|        - |   82 | `	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't` |
|        - |   83 | `	 * itself overflow.) */` |
|      358 |   84 | `	if( nNewCap < nOldCap * 2 ){` |
|      358 |   85 | `		sxu32 nDbl = nOldCap * 2;` |
|      358 |   86 | `		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }` |
|      358 |   87 | `		if( nNewCap < nDbl ){ nNewCap = nDbl; }` |
|      177 |   88 | `	}` |
|      535 |   89 | `	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,` |
|      177 |   90 | `		nNewCap * sizeof(ph7_value));` |
|      358 |   91 | `	if( pNew == 0 ){` |
|      ! 0 |   92 | `		return 0; /* OOM: caller keeps pOld and raises the guard error */` |
|        - |   93 | `	}` |
|        - |   94 | `	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */` |
|    26452 |   95 | `	for( i = nOldCap; i < nNewCap; i++ ){` |
|    26098 |   96 | `		PH7_MemObjInit(pVm, &pNew[i]);` |
|    26098 |   97 | `		pNew[i].nIdx = SXU32_HIGH;` |
|    13051 |   98 | `	}` |
|        - |   99 | `	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */` |
|      358 |  100 | `	*ppTos = pNew + (*ppTos - pOld);` |
|      358 |  101 | `	*ppStack = pNew;` |
|      358 |  102 | `	pState->pStack = pNew + (pState->pStack - pOld);` |
|      358 |  103 | `	pState->pTos = pNew + (pState->pTos - pOld);` |
|      358 |  104 | `	pState->nStackCap = nNewCap;` |
|      358 |  105 | `	if( pCallTop ){` |
|        - |  106 | `		/* This activation is a trampoline callee: its record owns the buffer. */` |
|      302 |  107 | `		pCallTop->sCall.pFrameStack = pNew;` |
|      302 |  108 | `		pCallTop->sCall.nStackCap = nNewCap;` |
|      152 |  109 | `	}else{` |
|        - |  110 | `		/* Base activation: the native entry frees *ppBaseOwner (and, for a` |
|        - |  111 | `		 * resumable coroutine, persists *pnBaseCap across suspend/resume). */` |
|       58 |  112 | `		if( ppBaseOwner ){ *ppBaseOwner = pNew; }` |
|       58 |  113 | `		if( pnBaseCap ){ *pnBaseCap = nNewCap; }` |
|        - |  114 | `	}` |
|        - |  115 | `	/* Re-anchor this activation's captured spread runs (pStart into the old` |
|        - |  116 | `	 * buffer). Enclosing-activation runs live in other buffers — leave them. */` |
|      358 |  117 | `	nRun = SySetUsed(&pVm->aSpreadRun);` |
|      358 |  118 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      358 |  119 | `	for( i = 0; i < nRun; i++ ){` |
|      ! 0 |  120 | `		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){` |
|      ! 0 |  121 | `			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);` |
|      ! 0 |  122 | `		}` |
|      ! 0 |  123 | `	}` |
|      358 |  124 | `	return 1;` |
|      470 |  125 | `}` |
|        - |  126 | `/*` |
|        - |  127 | ` * Ensure the running activation's operand stack can hold a spread of nEntry` |
|        - |  128 | ` * elements pushed at *ppTos (the source slot becomes the first element, so the` |
|        - |  129 | ` * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.` |
|        - |  130 | ` * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.` |
|        - |  131 | ` */` |
|     1134 |  132 | `static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,` |
|        - |  133 | `	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,` |
|        - |  134 | `	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)` |
|        4 |  135 | `{` |
|        - |  136 | `	sxu32 nNeed;` |
|     1138 |  137 | `	if( nEntry == 0 ){` |
|      205 |  138 | `		return 1; /* empty spread never grows the stack */` |
|        - |  139 | `	}` |
|      936 |  140 | `	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);` |
|     1402 |  141 | `	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,` |
|      466 |  142 | `		pCallTop, ppBaseOwner, pnBaseCap);` |
|      571 |  143 | `}` |
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
|  4614194 |  164 | `static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)` |
|        5 |  165 | `{` |
|  4614199 |  166 | `	if( rcTerm != PH7_SUSPEND ){` |
|  4612733 |  167 | `		VmDiscardFinallyActions(&(*pVm),pState->nFinallyActBase);` |
|  2306360 |  168 | `	}` |
|  4614199 |  169 | `	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){` |
|  3149532 |  170 | `		VmClearFramePending(pState->pEntryFrame);` |
|  1574760 |  171 | `	}` |
|  4614199 |  172 | `	SySetRelease(pArg);` |
|  4614199 |  173 | `	if( rcTerm == PH7_ABORT \|\| rcTerm == PH7_EXCEPTION ){` |
|   815651 |  174 | `		while( pTos >= pState->pStack ){` |
|   409244 |  175 | `			PH7_MemObjRelease(pTos);` |
|   409244 |  176 | `			pTos--;` |
|        5 |  177 | `		}` |
|   203203 |  178 | `	}` |
|  4614199 |  179 | `	return rcTerm;` |
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
|  5401472 |  195 | `static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase)` |
|        5 |  196 | `{` |
|  5401487 |  197 | `	while( SySetUsed(&pVm->aFinallyAction) > nBase ){` |
|       13 |  198 | `		VmFinallyAction *pAct = (VmFinallyAction *)SySetPeek(&pVm->aFinallyAction);` |
|       13 |  199 | `		if( pAct->eKind == PH7_FA_RETHROW && pAct->pExc ){` |
|        7 |  200 | `			PH7_ClassInstanceUnref(pAct->pExc);` |
|        9 |  201 | `		}else if( pAct->eKind == PH7_FA_RETURN ){` |
|        3 |  202 | `			PH7_MemObjRelease(&pAct->sRet);` |
|        1 |  203 | `		}` |
|       13 |  204 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|        3 |  205 | `	}` |
|  5401477 |  206 | `}` |
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
|   793222 |  219 | `static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)` |
|        5 |  220 | `{` |
|        - |  221 | `	ph7_value *pObj;` |
|        - |  222 | `	/* Decrement nesting level */` |
|   793227 |  223 | `	pVm->nRecursionDepth--;` |
|   793227 |  224 | `	if( pCallee->bSelfPushed ){` |
|        - |  225 | `		/* Pop class name */` |
|   506549 |  226 | `		(void)SySetPop(&pVm->aSelf);` |
|   253272 |  227 | `	}` |
|   793227 |  228 | `	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  229 | `		/* Return by reference,reflect that */` |
|       70 |  230 | `		if( pCallee->nLastRef != SXU32_HIGH ){` |
|       70 |  231 | `			VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pCallee->pFrame->sLocal);` |
|        - |  232 | `			sxu32 i;` |
|        - |  233 | `			/* Make sure the referenced object is not a local variable */` |
|      124 |  234 | `			for( i = 0 ; i < SySetUsed(&pCallee->pFrame->sLocal) ; ++i ){` |
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
|       37 |  246 | `		}else{` |
|      ! 0 |  247 | `			if( (pCaller->pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  248 | `				VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  249 | `					"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  250 | `					&pCallee->pVmFunc->sName);` |
|      ! 0 |  251 | `			}` |
|        - |  252 | `		}` |
|       70 |  253 | `		pCaller->pTos->nIdx = pCallee->nLastRef;` |
|       37 |  254 | `	}else{` |
|        - |  255 | `		/* A by-VALUE return is a TEMPORARY — php's IS_TMP_VAR — and must not look like` |
|        - |  256 | `		 * an lvalue. The result lands in the slot the call's first ARGUMENT occupied,` |
|        - |  257 | `		 * which still carried that argument's variable index, so the returned value` |
|        - |  258 | ``		 * inherited it: `f(id($z))` with `function f(&$x)` aliased and overwrote `$z`,`` |
|        - |  259 | `		 * a variable neither function was given by reference. Every call form was` |
|        - |  260 | `		 * affected (function, method, static, closure, nested) and every one of them` |
|        - |  261 | `		 * silently. Clearing it here also lets the call site see the temporary for what` |
|        - |  262 | `		 * it is, which is what php's "Only variables should be passed by reference"` |
|        - |  263 | `		 * notice is raised on. */` |
|   793161 |  264 | `		pCaller->pTos->nIdx = SXU32_HIGH;` |
|        - |  265 | `	}` |
|   793227 |  266 | `	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  267 | `		/* The callee threw (or its finally threw past it). If an in-place catch` |
|        - |  268 | `		 * recorded a resume target owned by THIS caller's body, resume there and` |
|        - |  269 | `		 * consume the target (VmRecordedResume); when the catcher is an outer exec` |
|        - |  270 | `		 * — or this is a callback with no bytecode to resume into — propagate so` |
|        - |  271 | `		 * the owning exec lands. This replaces the old "is the caller's parent a` |
|        - |  272 | `		 * resumable try frame" test, which resumed at the caller's OWN try even` |
|        - |  273 | `		 * when the finally's throw was caught further out, losing that catch's` |
|        - |  274 | `		 * return (ROOT B, face c). */` |
|        - |  275 | `		sxi32 iResumePc;` |
|   609525 |  276 | `		VmFrame *pParentFrame = pCallee->pFrame->pParent;` |
|   609525 |  277 | `		if( !pCaller->is_callback && pVm->pInlineInstr == (void *)pCaller->aInstr ){` |
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
|   609525 |  288 | `		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){` |
|        - |  289 | `			/* Pop the result, then drain any abandoned outer-expression operands` |
|        - |  290 | `			 * to the catching try's base (like the inline branch above) — the` |
|        - |  291 | `			 * ops that would have consumed them were abandoned by the throw, and` |
|        - |  292 | `` 			 * leaving them leaks one slot per caught throw (`try { $a = 1 + f(); }` `` |
|        - |  293 | `			 * in a loop overflowed the operand stack). */` |
|   208761 |  294 | `			VmPopOperand(&pCaller->pTos,1);` |
|   815105 |  295 | `			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iResumeStackDepth ){` |
|   606349 |  296 | `				PH7_MemObjRelease(pCaller->pTos);` |
|   606349 |  297 | `				pCaller->pTos--;` |
|        5 |  298 | `			}` |
|   208761 |  299 | `			pCaller->pc = iResumePc;` |
|   208761 |  300 | `			rc = PH7_OK;` |
|   104383 |  301 | `		}else{` |
|   400769 |  302 | `			if( pParentFrame->pParent ){` |
|   400765 |  303 | `				rc = PH7_EXCEPTION;` |
|   200385 |  304 | `			}else{` |
|        - |  305 | `				/* Continue normal execution */` |
|        6 |  306 | `				rc = PH7_OK;` |
|        - |  307 | `			}` |
|        - |  308 | `		}` |
|   304760 |  309 | `	}` |
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
|   793227 |  320 | `	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){` |
|        - |  321 | `		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee` |
|        - |  322 | `		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly` |
|        - |  323 | `		 * the allocated slot count either way. */` |
|   788749 |  324 | `		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap);` |
|   394587 |  325 | `	}` |
|        - |  326 | `	/* Leave the frame */` |
|   793227 |  327 | `	VmLeaveFrame(&(*pVm));` |
|   793227 |  328 | `	if( rc == PH7_ABORT ){` |
|      399 |  329 | `		return PH7_ABORT;` |
|        - |  330 | `	}` |
|   792833 |  331 | `	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  332 | `		/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  333 | `		 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  334 | `		 * overwriting the state saved by the inner level.` |
|        - |  335 | `		 * pTos points to the result slot (not yet written).` |
|        - |  336 | `		 * Save nTos one below so resume pushes at the result slot. */` |
|      ! 0 |  337 | `		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);` |
|      ! 0 |  338 | `		return PH7_SUSPEND;` |
|        - |  339 | `	}` |
|   792833 |  340 | `	if( rc == PH7_EXCEPTION ){` |
|   400765 |  341 | `		return PH7_EXCEPTION;` |
|        - |  342 | `	}` |
|   392073 |  343 | `	return PH7_OK;` |
|   396831 |  344 | `}` |
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
|  4614500 |  380 | `PH7_PRIVATE sxi32 VmByteCodeExec(` |
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
|  4614505 |  400 | `	if( VmNativeNestingExceeded(pVm) ){` |
|        6 |  401 | `		return VmNativeNestingFatal(pVm);` |
|        - |  402 | `	}` |
|        - |  403 | `	/* A fresh native exec entered while a C-boundary throw is parked` |
|        - |  404 | `	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the` |
|        - |  405 | `	 * very opcode that swallowed the throw) must run CLEAN: the parked status` |
|        - |  406 | `	 * belongs to the interrupted outer exec's fetch-point router, not to this` |
|        - |  407 | `	 * one. Save+clear on entry, merge back on exit — the outer status is` |
|        - |  408 | `	 * restored unless this exec parked its own unconsumed (newer) one, with` |
|        - |  409 | `	 * PH7_ABORT dominating either way. */` |
|  4614501 |  410 | `	nSavedBrc = pVm->nBoundaryRc;` |
|  4614501 |  411 | `	pVm->nBoundaryRc = 0;` |
|        - |  412 | `	/* The executing source line belongs to the ACTIVATION. A nested body -- a called` |
|        - |  413 | `	 * function, but equally an attribute-default or default-argument mini-program --` |
|        - |  414 | `	 * runs its own bytecode with its own lines, so it must not leave the caller` |
|        - |  415 | ``	 * reporting the callee's position: `new Exception` stamped line 1 because the`` |
|        - |  416 | ``	 * class's `protected $message = '';` default ran (from the embedded chunk) between`` |
|        - |  417 | `	 * OP_NEW and the stamp. Save on entry, restore on exit. */` |
|  4614501 |  418 | `	nSavedLine = pVm->nCurLine;` |
|  4614501 |  419 | `	pVm->nVmExecDepth++;` |
|  6921745 |  420 | `	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,` |
|  2307244 |  421 | `		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);` |
|  4614501 |  422 | `	pVm->nVmExecDepth--;` |
|  4614501 |  423 | `	pVm->nCurLine = nSavedLine;` |
|  4614501 |  424 | `	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT \|\| pVm->nBoundaryRc == 0) ){` |
|       43 |  425 | `		pVm->nBoundaryRc = nSavedBrc;` |
|       19 |  426 | `	}` |
|  4614501 |  427 | `	return rc;` |
|  2307251 |  428 | `}` |
|        - |  429 | `/*` |
|        - |  430 | ` * D1 commit 2: allocate a captured lvalue path for a deferred element/property call arg.` |
|        - |  431 | ` * eRoot picks the root container: 0 = a real aMemObj slot (nRootIdx), 1 = an undefined` |
|        - |  432 | ` * variable to vivify by name at resolve time (pName, a VM-lifetime bytecode string, borrowed),` |
|        - |  433 | ` * 2 = a string base (subscripting a string -> php refuses a by-ref bind). The struct owns its` |
|        - |  434 | ` * step array and each step's key/name; PH7_MemObjRelease frees it via VmFreeDeferredPath.` |
|        - |  435 | ` */` |
|    47454 |  436 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName)` |
|        5 |  437 | `{` |
|    47459 |  438 | `	VmDeferredPath *pPath = (VmDeferredPath *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDeferredPath));` |
|    47459 |  439 | `	if( pPath == 0 ){` |
|      ! 0 |  440 | `		return 0;` |
|        - |  441 | `	}` |
|    47459 |  442 | `	SyZero(pPath,sizeof(VmDeferredPath));` |
|    47459 |  443 | `	pPath->pAlloc = &pVm->sAllocator;` |
|    47459 |  444 | `	pPath->eRoot = eRoot;` |
|    47459 |  445 | `	pPath->nRootIdx = nRootIdx;` |
|    47459 |  446 | `	if( eRoot == 1 && pName ){` |
|        8 |  447 | `		pPath->sRootName = *pName; /* borrowed VM-lifetime bytes, not copied */` |
|        3 |  448 | `	}` |
|    47459 |  449 | `	return pPath;` |
|    23898 |  450 | `}` |
|        - |  451 | `/*` |
|        - |  452 | ` * A carrier for a fetch that ALREADY HAPPENED: an overloaded container answered with a` |
|        - |  453 | ` * value, and only the by-ref verdict is still pending (VM_DEFER_ROOT_PREFETCH). Takes a` |
|        - |  454 | ` * copy of the value; the caller keeps its own.` |
|        - |  455 | ` */` |
|     2344 |  456 | `PH7_PRIVATE VmDeferredPath * VmDeferPathNewPrefetch(ph7_vm *pVm,int nKind,ph7_class *pClass,` |
|        - |  457 | `	const SyString *pName,ph7_value *pVal)` |
|        5 |  458 | `{` |
|     2349 |  459 | `	VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),VM_DEFER_ROOT_PREFETCH,SXU32_HIGH,0);` |
|     2349 |  460 | `	if( pPath == 0 ){` |
|      ! 0 |  461 | `		return 0;` |
|        - |  462 | `	}` |
|     2349 |  463 | `	pPath->nOverKind = (sxu8)nKind;` |
|     2349 |  464 | `	pPath->pOverClass = pClass;` |
|     2349 |  465 | `	if( pName && pName->nByte > 0 ){` |
|     2251 |  466 | `		pPath->zOverName = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|     2251 |  467 | `		if( pPath->zOverName == 0 ){` |
|      ! 0 |  468 | `			VmFreeDeferredPath(pPath);` |
|      ! 0 |  469 | `			return 0;` |
|        - |  470 | `		}` |
|     2251 |  471 | `		SyStringInitFromBuf(&pPath->sOverName,pPath->zOverName,pName->nByte);` |
|     1123 |  472 | `	}` |
|     2349 |  473 | `	PH7_MemObjInit(&(*pVm),&pPath->sPrefetch);` |
|     2349 |  474 | `	PH7_MemObjStore(pVal,&pPath->sPrefetch);` |
|     2349 |  475 | `	return pPath;` |
|     1177 |  476 | `}` |
|        - |  477 | `/*` |
|        - |  478 | ` * The verdict a prefetched value gets when the callee turns out to want it BY REFERENCE:` |
|        - |  479 | ` * php asked the object for something to modify and it could only answer with a value.` |
|        - |  480 | ` * Two of the three are notices php carries on from; a HOOKED property is the one php` |
|        - |  481 | ` * refuses outright. All three stay silent for a by-VALUE parameter, which is the whole` |
|        - |  482 | ` * reason the fetch could not decide them itself.` |
|        - |  483 | ` */` |
|       44 |  484 | `static sxi32 VmPrefetchByRefVerdict(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pVal)` |
|        1 |  485 | `{` |
|       45 |  486 | `	if( pPath->nOverKind == VM_OVER_PROP ){` |
|       17 |  487 | `		PH7_VmOverloadedPropNotice(&(*pVm),pPath->pOverClass,&pPath->sOverName,pVal);` |
|       17 |  488 | `		return SXRET_OK;` |
|        - |  489 | `	}` |
|       29 |  490 | `	if( pPath->nOverKind == VM_OVER_HOOK ){` |
|        - |  491 | `		SyBlob sErrMsg;` |
|        - |  492 | `		sxi32 rcH;` |
|        7 |  493 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        7 |  494 | `		SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|        6 |  495 | `			&pPath->pOverClass->sName,&pPath->sOverName);` |
|        7 |  496 | `		rcH = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg);` |
|        7 |  497 | `		SyBlobRelease(&sErrMsg);` |
|        7 |  498 | `		return (rcH == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  499 | `	}` |
|       23 |  500 | `	PH7_VmOverloadedElemNotice(&(*pVm),pPath->pOverClass,pVal);` |
|       23 |  501 | `	return SXRET_OK;` |
|       23 |  502 | `}` |
|    45410 |  503 | `static VmDeferStep * VmDeferPathGrow(VmDeferredPath *pPath)` |
|        5 |  504 | `{` |
|    45415 |  505 | `	if( pPath->nStep >= pPath->nAlloc ){` |
|    45363 |  506 | `		sxu32 nNew = pPath->nAlloc ? pPath->nAlloc * 2 : 4;` |
|    68208 |  507 | `		VmDeferStep *aNew = (VmDeferStep *)SyMemBackendRealloc(pPath->pAlloc,pPath->aStep,` |
|    22845 |  508 | `			nNew * sizeof(VmDeferStep));` |
|    45363 |  509 | `		if( aNew == 0 ){` |
|      ! 0 |  510 | `			return 0;` |
|        - |  511 | `		}` |
|    45363 |  512 | `		pPath->aStep = aNew;` |
|    45363 |  513 | `		pPath->nAlloc = nNew;` |
|    22845 |  514 | `	}` |
|    45415 |  515 | `	return &pPath->aStep[pPath->nStep];` |
|    22876 |  516 | `}` |
|        - |  517 | `/* Append an array-element step, deep-copying the index value (the caller releases pKey). */` |
|    45044 |  518 | `PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey)` |
|        5 |  519 | `{` |
|    45049 |  520 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|    45049 |  521 | `	if( pStep == 0 ){` |
|      ! 0 |  522 | `		return SXERR_MEM;` |
|        - |  523 | `	}` |
|    45049 |  524 | `	pStep->isProp = 0;` |
|    45049 |  525 | `	pStep->bAppend = 0;` |
|    45049 |  526 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|    45049 |  527 | `	PH7_MemObjInit(pKey->pVm,&pStep->sKey);` |
|    45049 |  528 | `	PH7_MemObjStore(pKey,&pStep->sKey);` |
|    45049 |  529 | `	pPath->nStep++;` |
|    45049 |  530 | `	return SXRET_OK;` |
|    22693 |  531 | `}` |
|        - |  532 | ``/* Append a KEYLESS element step — the `[]` of `f($a[])`. It carries no index at all,`` |
|        - |  533 | ` * so the two resolvers differ on it: by reference it creates the next element (php` |
|        - |  534 | `` * binds the parameter to it), by value it is php's `Cannot use [] for reading`. */`` |
|       12 |  535 | `PH7_PRIVATE sxi32 VmDeferPathPushAppend(VmDeferredPath *pPath)` |
|        1 |  536 | `{` |
|       13 |  537 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|       13 |  538 | `	if( pStep == 0 ){` |
|      ! 0 |  539 | `		return SXERR_MEM;` |
|        - |  540 | `	}` |
|       13 |  541 | `	pStep->isProp = 0;` |
|       13 |  542 | `	pStep->bAppend = 1;` |
|       13 |  543 | `	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;` |
|       13 |  544 | `	SyZero((void *)&pStep->sKey,sizeof(ph7_value));` |
|       13 |  545 | `	pPath->nStep++;` |
|       13 |  546 | `	return SXRET_OK;` |
|        7 |  547 | `}` |
|        - |  548 | `/* Append an object-property step, owning a private copy of the name bytes. */` |
|      354 |  549 | `PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName)` |
|        5 |  550 | `{` |
|      359 |  551 | `	VmDeferStep *pStep = VmDeferPathGrow(pPath);` |
|        - |  552 | `	char *zCopy;` |
|      359 |  553 | `	if( pStep == 0 ){` |
|      ! 0 |  554 | `		return SXERR_MEM;` |
|        - |  555 | `	}` |
|      359 |  556 | `	zCopy = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);` |
|      359 |  557 | `	if( zCopy == 0 ){` |
|      ! 0 |  558 | `		return SXERR_MEM;` |
|        - |  559 | `	}` |
|      359 |  560 | `	pStep->isProp = 1;` |
|      359 |  561 | `	pStep->bAppend = 0;` |
|      359 |  562 | `	pStep->zProp = zCopy;` |
|      359 |  563 | `	SyStringInitFromBuf(&pStep->sProp,zCopy,pName->nByte);` |
|      359 |  564 | `	pPath->nStep++;` |
|      359 |  565 | `	return SXRET_OK;` |
|      182 |  566 | `}` |
|        - |  567 | `/* Release a captured lvalue path and everything it owns (element keys, property names). */` |
|        - |  568 | `/*` |
|        - |  569 | `` * The pending offset of a `$s[k] ??= v`: a heap copy of the RAW key, owned by the`` |
|        - |  570 | ` * peek's MEMOBJ_AUX_COALSTROFF result on the operand stack. One carrier per` |
|        - |  571 | `` * pending ??=, so `$s[9] ??= ($t[9] ??= "q")` nests — a single VM-wide slot could`` |
|        - |  572 | ` * not (the inner peek overwrote the outer's offset, and the outer store then` |
|        - |  573 | ` * replaced the whole string).` |
|        - |  574 | ` */` |
|       54 |  575 | `PH7_PRIVATE VmCoalStrOff * VmCoalStrOffNew(ph7_vm *pVm,ph7_value *pKey)` |
|        1 |  576 | `{` |
|       55 |  577 | `	VmCoalStrOff *pCoal = (VmCoalStrOff *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmCoalStrOff));` |
|       55 |  578 | `	if( pCoal == 0 ){` |
|      ! 0 |  579 | `		return 0;` |
|        - |  580 | `	}` |
|       55 |  581 | `	pCoal->pAlloc = &pVm->sAllocator;` |
|       55 |  582 | `	PH7_MemObjInit(&(*pVm),&pCoal->sKey);` |
|       55 |  583 | `	if( pKey ){` |
|       55 |  584 | `		PH7_MemObjStore(pKey,&pCoal->sKey);` |
|       27 |  585 | `	}` |
|       55 |  586 | `	return pCoal;` |
|       28 |  587 | `}` |
|       90 |  588 | `PH7_PRIVATE void VmFreeCoalStrOff(VmCoalStrOff *pCoal)` |
|        4 |  589 | `{` |
|        - |  590 | `	SyMemBackend *pAlloc;` |
|       94 |  591 | `	if( pCoal == 0 ){` |
|       40 |  592 | `		return;` |
|        - |  593 | `	}` |
|       55 |  594 | `	pAlloc = pCoal->pAlloc;` |
|       55 |  595 | `	PH7_MemObjRelease(&pCoal->sKey);` |
|       55 |  596 | `	SyMemBackendFree(pAlloc,pCoal);` |
|       49 |  597 | `}` |
|        - |  598 | `/*` |
|        - |  599 | ` * Build the pending __call/__callStatic routing OP_MEMBER hands to the OP_CALL that` |
|        - |  600 | ` * follows it, and hang it off the marked carrier slot. One record per routed call, so a` |
|        - |  601 | ` * routed call evaluated inside another routed call's ARGUMENT LIST — which is where they` |
|        - |  602 | ` * now sit, php's order — keeps its own {receiver, class, name}. Takes the receiver` |
|        - |  603 | ` * reference; the record owns it from here.` |
|        - |  604 | ` */` |
|      126 |  605 | `PH7_PRIVATE VmMagicCall * VmMagicCallNew(ph7_vm *pVm,ph7_class_instance *pRecv,` |
|        - |  606 | `	ph7_class *pClass,const SyString *pName)` |
|        3 |  607 | `{` |
|      129 |  608 | `	VmMagicCall *pPend = (VmMagicCall *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmMagicCall));` |
|      129 |  609 | `	if( pPend == 0 ){` |
|      ! 0 |  610 | `		return 0;` |
|        - |  611 | `	}` |
|      129 |  612 | `	pPend->pAlloc = &pVm->sAllocator;` |
|      129 |  613 | `	pPend->pRecv = pRecv;` |
|      129 |  614 | `	pPend->pClass = pClass;` |
|      129 |  615 | `	SyBlobInit(&pPend->sName,&pVm->sAllocator);` |
|      129 |  616 | `	if( pName && pName->nByte > 0 ){` |
|      129 |  617 | `		SyBlobAppend(&pPend->sName,(const void *)pName->zString,pName->nByte);` |
|       63 |  618 | `	}` |
|      129 |  619 | `	if( pRecv ){` |
|       86 |  620 | `		pRecv->iRef++;` |
|       42 |  621 | `	}` |
|      129 |  622 | `	return pPend;` |
|       66 |  623 | `}` |
|      126 |  624 | `PH7_PRIVATE void VmFreeMagicCall(VmMagicCall *pPend)` |
|        3 |  625 | `{` |
|        - |  626 | `	SyMemBackend *pAlloc;` |
|      129 |  627 | `	if( pPend == 0 ){` |
|      ! 0 |  628 | `		return;` |
|        - |  629 | `	}` |
|      129 |  630 | `	pAlloc = pPend->pAlloc;` |
|      129 |  631 | `	if( pPend->pRecv ){` |
|      ! 0 |  632 | `		PH7_ClassInstanceUnref(pPend->pRecv);` |
|      ! 0 |  633 | `	}` |
|      129 |  634 | `	SyBlobRelease(&pPend->sName);` |
|      129 |  635 | `	SyMemBackendFree(pAlloc,pPend);` |
|       66 |  636 | `}` |
|    47454 |  637 | `PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath)` |
|        5 |  638 | `{` |
|        - |  639 | `	sxu32 i;` |
|    47459 |  640 | `	if( pPath == 0 ){` |
|      ! 0 |  641 | `		return;` |
|        - |  642 | `	}` |
|    92869 |  643 | `	for( i = 0 ; i < pPath->nStep ; ++i ){` |
|    45415 |  644 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|    45415 |  645 | `		if( pStep->isProp ){` |
|      359 |  646 | `			if( pStep->zProp ){` |
|      359 |  647 | `				SyMemBackendFree(pPath->pAlloc,pStep->zProp);` |
|      182 |  648 | `			}` |
|    45238 |  649 | `		}else if( !pStep->bAppend ){` |
|        - |  650 | `			/* An append step holds no key at all — its sKey was never initialized. */` |
|    45049 |  651 | `			PH7_MemObjRelease(&pStep->sKey);` |
|    22688 |  652 | `		}` |
|    22876 |  653 | `	}` |
|    47459 |  654 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|     2349 |  655 | `		PH7_MemObjRelease(&pPath->sPrefetch);` |
|     2349 |  656 | `		if( pPath->zOverName ){` |
|     2251 |  657 | `			SyMemBackendFree(pPath->pAlloc,pPath->zOverName);` |
|     1123 |  658 | `		}` |
|     1172 |  659 | `	}` |
|    47459 |  660 | `	if( pPath->aStep ){` |
|    45363 |  661 | `		SyMemBackendFree(pPath->pAlloc,pPath->aStep);` |
|    22845 |  662 | `	}` |
|    47459 |  663 | `	SyMemBackendFree(pPath->pAlloc,pPath);` |
|    23898 |  664 | `}` |
|        - |  665 | `/* Map an op-handler VmOpRc into the main-loop rc convention used by VmByteCodeExecBody. */` |
|    45324 |  666 | `static sxi32 VmOpRcToExecRc(VmOpRc rcOp)` |
|        5 |  667 | `{` |
|    45329 |  668 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 |  669 | `		return PH7_ABORT;` |
|        - |  670 | `	}` |
|    45327 |  671 | `	if( rcOp == VM_OP_EXCEPTION ){` |
|       15 |  672 | `		return PH7_EXCEPTION;` |
|        - |  673 | `	}` |
|    45315 |  674 | `	return SXRET_OK;` |
|    22833 |  675 | `}` |
|        - |  676 | `/*` |
|        - |  677 | ` * D1 commit 2: re-drive ONE captured lvalue step by invoking the real LOAD_IDX / MEMBER` |
|        - |  678 | ` * handler on a synthetic 2-slot stack. This reuses the proven COW / vivify / warning / magic` |
|        - |  679 | ` * machinery instead of hand-walking it. pBase carries the current container (its nIdx must be` |
|        - |  680 | ` * a real aMemObj slot for a by-ref write to vivify in place). iP2 selects the mode:` |
|        - |  681 | ` * LOAD_IDX 1=write(vivify,by-ref) / 0=read(by-value); MEMBER PH7_MEMBER_READ=by-value read.` |
|        - |  682 | ` * On success pOut receives the result value and its nIdx (the aliasable element slot for a` |
|        - |  683 | ` * vivified by-ref element).` |
|        - |  684 | ` */` |
|    45324 |  685 | `static sxi32 VmReDriveStep(ph7_vm *pVm,sxi32 iOp,sxu32 iP2,ph7_value *pBase,ph7_value *pKey,ph7_value *pOut)` |
|        5 |  686 | `{` |
|        - |  687 | `	ph7_value mini[2];` |
|        - |  688 | `	VmInstr aI[2];` |
|        - |  689 | `	VmExecState st;` |
|        - |  690 | `	VmOpRc rcOp;` |
|        - |  691 | ``	/* A NULL key is the APPEND form (`$a[]`): LOAD_IDX takes no index operand, so the`` |
|        - |  692 | `	 * base is the whole stack and iP1 says so. */` |
|    45329 |  693 | `	int bAppend = (pKey == 0);` |
|    45329 |  694 | `	PH7_MemObjInit(pVm,&mini[0]);` |
|    45329 |  695 | `	PH7_MemObjInit(pVm,&mini[1]);` |
|    45329 |  696 | `	PH7_MemObjLoad(pBase,&mini[0]);` |
|    45329 |  697 | `	mini[0].nIdx = pBase->nIdx;` |
|    45329 |  698 | `	if( !bAppend ){` |
|    45319 |  699 | `		PH7_MemObjStore(pKey,&mini[1]);` |
|    22823 |  700 | `	}` |
|    45329 |  701 | `	SyZero((void *)aI,sizeof(aI));` |
|        - |  702 | `	/* LOAD_IDX: iP1=1 means "an index is present". MEMBER: iP1=0 means an INSTANCE member` |
|        - |  703 | ``	 * (iP1=1 would be a static `::` access). */`` |
|    45329 |  704 | `	aI[0].iOp = (sxu8)iOp; aI[0].iP1 = (iOp == PH7_OP_LOAD_IDX && !bAppend) ? 1 : 0; aI[0].iP2 = iP2;` |
|    45329 |  705 | `	SyZero((void *)&st,sizeof(st));` |
|    45329 |  706 | `	st.pStack = mini; st.pTos = bAppend ? &mini[0] : &mini[1]; st.aInstr = aI; st.pc = 0;` |
|    45329 |  707 | `	if( iOp == PH7_OP_LOAD_IDX ){` |
|    45043 |  708 | `		rcOp = VmExecOpLoadIdx(&(*pVm),&st,&aI[0]);` |
|    22690 |  709 | `	}else{` |
|      291 |  710 | `		rcOp = VmExecOpMember(&(*pVm),&st,&aI[0]);` |
|        - |  711 | `	}` |
|        - |  712 | `	/* The handler popped the key/name and left the result in the (now top) base slot.` |
|        - |  713 | `	 * DEEP-COPY it into pOut (MemObjStore, not MemObjLoad): the result is chained as the next` |
|        - |  714 | `	 * step's base and must OWN its buffer — mini[0] is released immediately below, and a` |
|        - |  715 | `	 * read-only view (MemObjLoad) would leave pOut dangling into freed memory. */` |
|    45329 |  716 | `	PH7_MemObjStore(st.pTos,pOut);` |
|    45329 |  717 | `	pOut->nIdx = st.pTos->nIdx;` |
|    45329 |  718 | `	PH7_MemObjRelease(&mini[0]);` |
|    45329 |  719 | `	return VmOpRcToExecRc(rcOp);` |
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
|       66 |  730 | `static sxi32 VmBindPropByRef(ph7_vm *pVm,ph7_value *pObj,const SyString *pName,sxu32 *pnOut,int *pbNoBind,ph7_value *pValOut)` |
|        3 |  731 | `{` |
|        - |  732 | `	ph7_class_instance *pThis;` |
|        - |  733 | `	ph7_class *pClass;` |
|        - |  734 | `	SyHashEntry *pEntry;` |
|       69 |  735 | `	VmClassAttr *pAttr = 0;` |
|       69 |  736 | `	*pbNoBind = 0;` |
|       69 |  737 | `	if( pObj == 0 ){` |
|        - |  738 | `		/* The root slot is gone (a detached frame): nothing to bind and nothing to say. */` |
|      ! 0 |  739 | `		*pbNoBind = 1;` |
|      ! 0 |  740 | `		return SXRET_OK;` |
|        - |  741 | `	}` |
|       69 |  742 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  743 | `		/* A non-object base has no property to alias, and php does not pass NULL and carry` |
|        - |  744 | `		 * on: asking one for something to MODIFY is its catchable Error, the same one every` |
|        - |  745 | `		 * other write shape through a null/int/string base raises. PHL warned about a READ` |
|        - |  746 | ``		 * it never performed and handed the by-ref parameter a NULL, so `f($u->p)` with`` |
|        - |  747 | ``		 * `function f(&$x)` wrote into nothing on a statement php stops the script for. The`` |
|        - |  748 | `		 * by-VALUE binding still takes the read warning — that half is php-exact — and is` |
|        - |  749 | `		 * what the value re-drive next door produces. */` |
|        - |  750 | `		SyBlob sErrM;` |
|        - |  751 | `		sxi32 rcErr;` |
|       21 |  752 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       21 |  753 | `		SyBlobFormat(&sErrM,"Attempt to modify property \"%z\" on %s",` |
|       10 |  754 | `			pName,VmArithValueName(pObj));` |
|       31 |  755 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       10 |  756 | `			SyBlobLength(&sErrM));` |
|       21 |  757 | `		SyBlobRelease(&sErrM);` |
|       21 |  758 | `		*pbNoBind = 1;` |
|       21 |  759 | `		return (rcErr == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  760 | `	}` |
|       49 |  761 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|       49 |  762 | `	pClass = pThis->pClass;` |
|       49 |  763 | `	if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
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
|       47 |  776 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|       47 |  777 | `	if( pEntry ){` |
|        3 |  778 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        3 |  779 | `		if( (pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      ! 0 |  780 | `			if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|        - |  781 | `				/* A by-reference argument asks for something to MODIFY, and this` |
|        - |  782 | `				 * class's handler refuses every write: php's catchable Error, the` |
|        - |  783 | `				 * same sentence a plain store gets. */` |
|      ! 0 |  784 | `				*pbNoBind = 1;` |
|      ! 0 |  785 | `				return VmThrowNativeNoWrite(&(*pVm),pAttr->pOwner,pAttr->pAttr);` |
|        - |  786 | `			}` |
|      ! 0 |  787 | `			if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|        - |  788 | `				/* A native class's property is a field of php's own C struct, not` |
|        - |  789 | `				 * storage a script may alias: php has no ptr_ptr handler for one, so` |
|        - |  790 | ``				 * `g($i->s)` with `function g(&$x)` passes the VALUE and the callee's`` |
|        - |  791 | `				 * write is lost — in SILENCE, unlike the overloaded case below, which` |
|        - |  792 | ``				 * php has a notice for. `preg_match($p,$s,$i->s)` is the same answer. */`` |
|      ! 0 |  793 | `				ph7_value *pCur = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|      ! 0 |  794 | `				*pbNoBind = 1;` |
|      ! 0 |  795 | `				if( pValOut && pCur ){` |
|      ! 0 |  796 | `					PH7_MemObjStore(pCur,pValOut);` |
|      ! 0 |  797 | `				}` |
|      ! 0 |  798 | `				return SXRET_OK;` |
|        - |  799 | `			}` |
|      ! 0 |  800 | `			*pnOut = pAttr->nIdx;` |
|      ! 0 |  801 | `			return SXRET_OK;` |
|        - |  802 | `		}` |
|        - |  803 | `		/* A static property is the CLASS's: php does not find it through an` |
|        - |  804 | ``		 * instance, so binding `f($o->s)` by reference must not hand out the`` |
|        - |  805 | `		 * class slot — that let a by-ref callee overwrite shared class state` |
|        - |  806 | `		 * through an object, and with no diagnostic at all (the value pass that` |
|        - |  807 | `		 * carries the notice at the fetch site never runs for a by-ref arg).` |
|        - |  808 | `		 * Notice here and fall through to the missing-property handling. */` |
|        4 |  809 | `		if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->pAttr->sName,` |
|        2 |  810 | `			pAttr->pAttr->iProtection,FALSE) ){` |
|        4 |  811 | `			VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  812 | `				"Accessing static property %z::$%z as non static",` |
|        1 |  813 | `				&pClass->sName,pName);` |
|        1 |  814 | `		}` |
|        3 |  815 | `		pAttr = 0;` |
|        1 |  816 | `	}` |
|       47 |  817 | `	if( PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|        - |  818 | `		/* Overloaded (magic) property: php passes it by-value with a Notice and drops the` |
|        - |  819 | `		 * write-back — "has no effect". The value it passes is __get's, which the caller` |
|        - |  820 | `		 * takes through pValOut; leaving the argument NULL instead turned` |
|        - |  821 | ``		 * `sort($o->magic)` — a statement php performs on a temporary — into`` |
|        - |  822 | ``		 * `sort(): Argument #1 ($array) must be of type array, null given`.`` |
|        - |  823 | `		 *` |
|        - |  824 | ``		 * `__get` ALONE decides it, which is php's own test`` |
|        - |  825 | ``		 * (zend_std_get_property_ptr_ptr consults `ce->__get` and nothing else): a`` |
|        - |  826 | ``		 * class carrying only `__set` has no way to ANSWER the fetch, so php falls`` |
|        - |  827 | `		 * through and creates an ordinary dynamic property instead — which is §10's` |
|        - |  828 | `		 * refusal here, not this notice. */` |
|      ! 0 |  829 | `		VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  830 | `			"Indirect modification of overloaded property %z::$%z has no effect",` |
|      ! 0 |  831 | `			&pClass->sName,pName);` |
|      ! 0 |  832 | `		*pbNoBind = 1;` |
|      ! 0 |  833 | `		if( pValOut && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|      ! 0 |  834 | `		 && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g') ){` |
|      ! 0 |  835 | `			VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|      ! 0 |  836 | `			PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pValOut);` |
|      ! 0 |  837 | `			VmMagicGuardPop(pVm);` |
|      ! 0 |  838 | `		}` |
|      ! 0 |  839 | `		return SXRET_OK;` |
|        - |  840 | `	}` |
|        - |  841 | `	{` |
|       47 |  842 | `		ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);` |
|       47 |  843 | `		if( pDecl && PH7_ATTR_LAZY_ABSENT(pDecl,pThis) ){` |
|      ! 0 |  844 | `			if( pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|        - |  845 | `				/* Refused with or without a struct behind it. */` |
|      ! 0 |  846 | `				*pbNoBind = 1;` |
|      ! 0 |  847 | `				return VmThrowNativeNoWrite(&(*pVm),pClass,pDecl);` |
|        - |  848 | `			}` |
|      ! 0 |  849 | `			pDecl = 0;   /* never held: php creates a dynamic property, PHL refuses (§10) */` |
|      ! 0 |  850 | `		}` |
|       47 |  851 | `		if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      ! 0 |  852 | `			VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pAttr);` |
|       47 |  853 | `		}else if( VmClassAllowsDynamicProps(&(*pVm),pClass) ){` |
|       38 |  854 | `			PH7_VmCreateDynamicAttr(&(*pVm),pThis,pName->zString,pName->nByte,&pAttr);` |
|       20 |  855 | `		}else{` |
|        - |  856 | `			SyBlob sMsg;` |
|        - |  857 | `			sxi32 rcT;` |
|       10 |  858 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       10 |  859 | `			SyBlobFormat(&sMsg,"Cannot create dynamic property %z::$%z",&pClass->sName,pName);` |
|       10 |  860 | `			rcT = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       10 |  861 | `			SyBlobRelease(&sMsg);` |
|       10 |  862 | `			*pbNoBind = 1;` |
|       10 |  863 | `			return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - |  864 | `		}` |
|       38 |  865 | `		if( pAttr ){` |
|       38 |  866 | `			*pnOut = pAttr->nIdx;` |
|       20 |  867 | `		}else{` |
|      ! 0 |  868 | `			*pbNoBind = 1;` |
|        - |  869 | `		}` |
|        - |  870 | `	}` |
|       38 |  871 | `	return SXRET_OK;` |
|       36 |  872 | `}` |
|        - |  873 | `/*` |
|        - |  874 | ` * Walk a captured path's steps over SLOTS: each step vivifies in place and answers the` |
|        - |  875 | ` * next one, so the terminal slot is what the by-ref binder aliases. Split out of the` |
|        - |  876 | ` * resolver below because the VALUE walk hands control back to it — an accessor that` |
|        - |  877 | ` * answered with an OBJECT is a handle, not a temporary, and everything under it is` |
|        - |  878 | ` * addressable again.` |
|        - |  879 | ` */` |
|      132 |  880 | `static sxi32 VmWalkStepsFromSlot(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,sxu32 nCur,` |
|        - |  881 | `	ph7_value *pSlot)` |
|        3 |  882 | `{` |
|        - |  883 | `	sxu32 i;` |
|        - |  884 | `	sxi32 rc;` |
|      239 |  885 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|      139 |  886 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|      139 |  887 | `		if( pStep->isProp ){` |
|       67 |  888 | `			sxu32 nOut = SXU32_HIGH;` |
|       67 |  889 | `			int bNoBind = 0;` |
|        - |  890 | `			ph7_value sMagicVal;` |
|       67 |  891 | `			int bLastStep = (i + 1 == pPath->nStep);` |
|       67 |  892 | `			PH7_MemObjInit(&(*pVm),&sMagicVal);` |
|      131 |  893 | `			rc = VmBindPropByRef(&(*pVm),(ph7_value *)SySetAt(&pVm->aMemObj,nCur),` |
|       64 |  894 | `				&pStep->sProp,&nOut,&bNoBind,bLastStep ? &sMagicVal : 0);` |
|       67 |  895 | `			if( rc != SXRET_OK ){` |
|       33 |  896 | `				PH7_MemObjRelease(&sMagicVal);` |
|       33 |  897 | `				return rc;` |
|        - |  898 | `			}` |
|       36 |  899 | `			if( bNoBind ){` |
|        - |  900 | `				/* magic/non-object: nothing to alias, so the argument is passed BY` |
|        - |  901 | `				 * VALUE — which for an overloaded property is what __get answered,` |
|        - |  902 | `				 * not the NULL this used to leave behind. */` |
|      ! 0 |  903 | `				if( bLastStep ){` |
|      ! 0 |  904 | `					PH7_MemObjStore(&sMagicVal,pSlot);` |
|      ! 0 |  905 | `					pSlot->nIdx = SXU32_HIGH;` |
|      ! 0 |  906 | `				}` |
|      ! 0 |  907 | `				PH7_MemObjRelease(&sMagicVal);` |
|      ! 0 |  908 | `				return SXRET_OK;` |
|        - |  909 | `			}` |
|       36 |  910 | `			PH7_MemObjRelease(&sMagicVal);` |
|       36 |  911 | `			nCur = nOut;` |
|       19 |  912 | `		}else{` |
|        - |  913 | `			ph7_value out;` |
|       74 |  914 | `			ph7_value *pContainer = (ph7_value *)SySetAt(&pVm->aMemObj,nCur);` |
|       74 |  915 | `			if( pContainer == 0 ){` |
|      ! 0 |  916 | `				return SXRET_OK;` |
|        - |  917 | `			}` |
|       74 |  918 | `			PH7_MemObjInit(&(*pVm),&out);` |
|        - |  919 | ``			/* `f($a[])` bound to a by-reference parameter: php CREATES the next element`` |
|        - |  920 | `			 * and aliases the parameter to it. */` |
|      110 |  921 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,1,pContainer,` |
|       72 |  922 | `				pStep->bAppend ? 0 : &pStep->sKey,&out);` |
|       74 |  923 | `			nCur = out.nIdx;` |
|       74 |  924 | `			PH7_MemObjRelease(&out);` |
|       74 |  925 | `			if( rc != SXRET_OK ){` |
|        3 |  926 | `				return rc;` |
|        - |  927 | `			}` |
|       72 |  928 | `			if( nCur == SXU32_HIGH ){` |
|      ! 0 |  929 | `				return SXRET_OK; /* no aliasable slot (e.g. a non-lvalue container): pass by value */` |
|        - |  930 | `			}` |
|        - |  931 | `		}` |
|       54 |  932 | `	}` |
|        - |  933 | `	{` |
|        - |  934 | `		/* The terminal slot is what the by-ref binder aliases, but the argument also has` |
|        - |  935 | `		 * to CARRY the element's value: a builtin reads what it is handed and writes back` |
|        - |  936 | `		 * through the slot. That was invisible while a path was only ever captured on a` |
|        - |  937 | `		 * MISS — the vivified element is NULL and so was the carrier — and stopped being` |
|        - |  938 | `		 * true when a WRITABLE container's existing element started riding one` |
|        - |  939 | ``		 * (`sort($ao['a'])` reached sort() as NULL). */`` |
|      102 |  940 | `		ph7_value *pFinal = (ph7_value *)SySetAt(&pVm->aMemObj,nCur);` |
|      102 |  941 | `		if( pFinal ){` |
|      102 |  942 | `			PH7_MemObjLoad(pFinal,pSlot);` |
|       50 |  943 | `		}` |
|        - |  944 | `	}` |
|      102 |  945 | `	pSlot->nIdx = nCur;` |
|      102 |  946 | `	return SXRET_OK;` |
|       69 |  947 | `}` |
|        - |  948 | `/*` |
|        - |  949 | ` * Walk a captured path's remaining steps over a VALUE rather than a slot — the` |
|        - |  950 | ` * continuation both resolvers need once the chain has left addressable storage: an` |
|        - |  951 | ` * overloaded container's answer is a temporary, and everything subscripted off it is a` |
|        - |  952 | ` * temporary too. bWrite picks php's fetch mode for those steps: a by-REFERENCE argument` |
|        - |  953 | ` * makes them W fetches, which vivify inside the temporary in SILENCE (php's` |
|        - |  954 | `` * `f($o['a']['zz'])` says only its notice), while a by-VALUE one reads and warns about a`` |
|        - |  955 | ` * key that is not there.` |
|        - |  956 | ` */` |
|    47302 |  957 | `static sxi32 VmWalkStepsOverValue(ph7_vm *pVm,VmDeferredPath *pPath,sxu32 iFrom,` |
|        - |  958 | `	ph7_value *pCur,int bWrite,ph7_value *pSlot)` |
|        5 |  959 | `{` |
|    47307 |  960 | `	sxi32 rc = SXRET_OK;` |
|        - |  961 | `	sxu32 i;` |
|    92547 |  962 | `	for( i = iFrom ; i < pPath->nStep ; ++i ){` |
|    45261 |  963 | `		VmDeferStep *pStep = &pPath->aStep[i];` |
|        - |  964 | `		ph7_value out;` |
|    45261 |  965 | `		PH7_MemObjInit(&(*pVm),&out);` |
|    45261 |  966 | `		if( pStep->isProp && bWrite && (pCur->iFlags & MEMOBJ_OBJ) ){` |
|        - |  967 | `			/* php's "indirect" only ever describes a VALUE: an object is a HANDLE, so a` |
|        - |  968 | `			 * write through one lands however the handle was obtained — which is also why` |
|        - |  969 | ``			 * the notice above stays silent for an object. `f($o->magic->p)` with`` |
|        - |  970 | ``			 * `function f(&$x)` really does create and write `p` on the object __get`` |
|        - |  971 | `			 * answered with. Bind the property and let the slot walk finish the chain. */` |
|        3 |  972 | `			sxu32 nOut = SXU32_HIGH;` |
|        3 |  973 | `			int bNoBind = 0;` |
|        3 |  974 | `			int bLastStep = (i + 1 == pPath->nStep);` |
|        - |  975 | `			ph7_value sMagicVal;` |
|        3 |  976 | `			PH7_MemObjRelease(&out);` |
|        3 |  977 | `			PH7_MemObjInit(&(*pVm),&sMagicVal);` |
|        4 |  978 | `			rc = VmBindPropByRef(&(*pVm),pCur,&pStep->sProp,&nOut,&bNoBind,` |
|        1 |  979 | `				bLastStep ? &sMagicVal : 0);` |
|        3 |  980 | `			if( rc != SXRET_OK \|\| bNoBind ){` |
|      ! 0 |  981 | `				if( rc == SXRET_OK && bLastStep ){` |
|        - |  982 | `					/* An overloaded property one level down: its own notice has been` |
|        - |  983 | `					 * raised and what __get answered is what php passes. */` |
|      ! 0 |  984 | `					PH7_MemObjStore(&sMagicVal,pSlot);` |
|      ! 0 |  985 | `					pSlot->nIdx = SXU32_HIGH;` |
|      ! 0 |  986 | `				}` |
|      ! 0 |  987 | `				PH7_MemObjRelease(&sMagicVal);` |
|      ! 0 |  988 | `				return rc;` |
|        - |  989 | `			}` |
|        3 |  990 | `			PH7_MemObjRelease(&sMagicVal);` |
|        3 |  991 | `			return VmWalkStepsFromSlot(&(*pVm),pPath,i + 1,nOut,pSlot);` |
|        - |  992 | `		}` |
|    45259 |  993 | `		if( pStep->isProp ){` |
|        - |  994 | `			ph7_value nameVal;` |
|      291 |  995 | `			PH7_MemObjInitFromString(&(*pVm),&nameVal,&pStep->sProp);` |
|      291 |  996 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_MEMBER,PH7_MEMBER_READ,pCur,&nameVal,&out);` |
|      291 |  997 | `			PH7_MemObjRelease(&nameVal);` |
|    45116 |  998 | `		}else if( pStep->bAppend ){` |
|        - |  999 | ``			/* `f($o['a'][])`: the append lands in the temporary either way. A by-VALUE`` |
|        - | 1000 | ``			 * binding is php's runtime `Cannot use [] for reading`, the same Error the`` |
|        - | 1001 | `			 * slot-based walk raises for it. */` |
|        - | 1002 | `			sxi32 rcAp;` |
|        3 | 1003 | `			if( bWrite ){` |
|        - | 1004 | `				/* The appended element is a fresh NULL that nothing else can see —` |
|        - | 1005 | `				 * php binds the parameter to it and the temporary is dropped. */` |
|      ! 0 | 1006 | `				PH7_MemObjRelease(pCur);` |
|      ! 0 | 1007 | `				*pCur = out;` |
|      ! 0 | 1008 | `				continue;` |
|        - | 1009 | `			}` |
|        3 | 1010 | `			PH7_MemObjRelease(&out);` |
|        3 | 1011 | `			rcAp = VmThrowFromVm(&(*pVm),"Error","Cannot use [] for reading",` |
|        - | 1012 | `				sizeof("Cannot use [] for reading")-1);` |
|        3 | 1013 | `			return (rcAp == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 1014 | `		}else{` |
|    44971 | 1015 | `			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,bWrite ? 1 : 0,pCur,&pStep->sKey,&out);` |
|        - | 1016 | `		}` |
|    45257 | 1017 | `		PH7_MemObjRelease(pCur);` |
|    45257 | 1018 | `		*pCur = out;` |
|    45257 | 1019 | `		if( rc != SXRET_OK ){` |
|       14 | 1020 | `			return rc;` |
|        - | 1021 | `		}` |
|    22791 | 1022 | `	}` |
|    47291 | 1023 | `	PH7_MemObjStore(pCur,pSlot);` |
|    47291 | 1024 | `	pSlot->nIdx = SXU32_HIGH;` |
|    47291 | 1025 | `	return SXRET_OK;` |
|    23822 | 1026 | `}` |
|        - | 1027 | `/* Resolve a captured lvalue path as a BY-REF target: vivify the whole chain in place and` |
|        - | 1028 | ` * leave pSlot->nIdx pointing at the terminal (aliasable) slot for the by-ref binder. */` |
|      188 | 1029 | `static sxi32 VmResolvePathByRef(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        3 | 1030 | `{` |
|        - | 1031 | `	sxu32 nCur;` |
|        - | 1032 | `	sxi32 rc;` |
|      191 | 1033 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|        - | 1034 | `		/* An overloaded container was asked for something to MODIFY and could only hand` |
|        - | 1035 | `		 * back a value: php notices that the write has no effect and carries on with the` |
|        - | 1036 | `		 * temporary. The notice is raised HERE — the fetch itself cannot know whether the` |
|        - | 1037 | `		 * parameter it feeds is by-reference, and a by-VALUE one is silent. */` |
|        - | 1038 | `		ph7_value cur;` |
|       45 | 1039 | `		PH7_MemObjInit(&(*pVm),&cur);` |
|       45 | 1040 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|       45 | 1041 | `		rc = VmPrefetchByRefVerdict(&(*pVm),pPath,&cur);` |
|       45 | 1042 | `		if( rc == SXRET_OK ){` |
|       39 | 1043 | `			rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,TRUE,pSlot);` |
|       19 | 1044 | `		}` |
|       45 | 1045 | `		PH7_MemObjRelease(&cur);` |
|       45 | 1046 | `		return rc;` |
|        - | 1047 | `	}` |
|      147 | 1048 | `	if( pPath->eRoot == 2 ){` |
|        - | 1049 | `		/* Subscripting a string: php refuses a by-ref bind to a string offset — but` |
|        - | 1050 | ``		 * it applies its OFFSET rules first, so `f($s["p"])` is the offset TypeError`` |
|        - | 1051 | ``		 * and `f($s[1.5])` warns about the cast before this Error is raised. */`` |
|        - | 1052 | `		sxi32 rcT;` |
|       17 | 1053 | `		if( pPath->nStep > 0 && !pPath->aStep[0].isProp ){` |
|        - | 1054 | `			SyBlob sTypeMsg;` |
|       17 | 1055 | `			sxi64 iOfft = 0;` |
|       14 | 1056 | `			if( VmStringOffsetResolve(&(*pVm),&pPath->aStep[0].sKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg)` |
|       10 | 1057 | `				== VM_STROFF_REJECT ){` |
|        3 | 1058 | `				rcT = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|        3 | 1059 | `				return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1060 | `			}` |
|        6 | 1061 | `		}` |
|        - | 1062 | `		{` |
|        - | 1063 | `			/* WHICH refusal is php's depends on what the argument fetch was reaching` |
|        - | 1064 | ``			 * for: the offset ITSELF (`f($s[1])`, one step) cannot be referenced, while`` |
|        - | 1065 | `			 * a step INTO it is the same reach-inside every other consumer gets, named` |
|        - | 1066 | ``			 * for the STEP — `f($s[0][1])` is `... as an array` and `f($s[0]->p)` is`` |
|        - | 1067 | ``			 * `... as an object`. php derives all three from the opcode that consumes`` |
|        - | 1068 | `			 * the fetch; here the step count and the last step's kind are that. */` |
|       21 | 1069 | `			const char *zSoMsg = (pPath->nStep > 1)` |
|        4 | 1070 | `				? (pPath->aStep[pPath->nStep-1].isProp` |
|        - | 1071 | `					? "Cannot use string offset as an object"` |
|        - | 1072 | `					: "Cannot use string offset as an array")` |
|        6 | 1073 | `				: "Cannot create references to/from string offsets";` |
|       15 | 1074 | `			rcT = VmThrowFromVm(&(*pVm),"Error",zSoMsg,SyStrlen(zSoMsg));` |
|        - | 1075 | `		}` |
|       15 | 1076 | `		return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1077 | `	}` |
|      133 | 1078 | `	if( pPath->eRoot == 1 ){` |
|        8 | 1079 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,TRUE); /* vivify $a */` |
|        8 | 1080 | `		if( pRoot == 0 ){` |
|      ! 0 | 1081 | `			return SXRET_OK;` |
|        - | 1082 | `		}` |
|        8 | 1083 | `		nCur = pRoot->nIdx;` |
|        5 | 1084 | `	}else{` |
|      127 | 1085 | `		nCur = pPath->nRootIdx;` |
|        - | 1086 | `	}` |
|      133 | 1087 | `	return VmWalkStepsFromSlot(&(*pVm),pPath,0,nCur,pSlot);` |
|       97 | 1088 | `}` |
|        - | 1089 | `/* Resolve a captured lvalue path as a BY-VALUE argument: read the chain (emitting php's` |
|        - | 1090 | ` * undefined-key/property/offset warnings) WITHOUT vivifying, leaving the terminal value in` |
|        - | 1091 | ` * pSlot. Re-drives the read handlers so the warning sequence matches php exactly. */` |
|    47264 | 1092 | `static sxi32 VmResolvePathByValue(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)` |
|        5 | 1093 | `{` |
|        - | 1094 | `	ph7_value cur;` |
|        - | 1095 | `	sxi32 rc;` |
|    47269 | 1096 | `	PH7_MemObjInit(&(*pVm),&cur);` |
|    47269 | 1097 | `	if( pPath->eRoot == VM_DEFER_ROOT_PREFETCH ){` |
|        - | 1098 | `		/* The accessor already ran, where php runs it: a by-VALUE argument simply takes` |
|        - | 1099 | `		 * what it answered, in silence. */` |
|     2303 | 1100 | `		PH7_MemObjStore(&pPath->sPrefetch,&cur);` |
|    46120 | 1101 | `	}else if( pPath->eRoot == 1 ){` |
|      ! 0 | 1102 | `		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,FALSE);` |
|      ! 0 | 1103 | `		if( pRoot == 0 ){` |
|      ! 0 | 1104 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&pPath->sRootName);` |
|      ! 0 | 1105 | `		}else{` |
|      ! 0 | 1106 | `			PH7_MemObjLoad(pRoot,&cur);` |
|      ! 0 | 1107 | `			cur.nIdx = pRoot->nIdx;` |
|        - | 1108 | `		}` |
|      ! 0 | 1109 | `	}else{` |
|    44971 | 1110 | `		ph7_value *pRoot = (ph7_value *)SySetAt(&pVm->aMemObj,pPath->nRootIdx);` |
|    44971 | 1111 | `		if( pRoot ){` |
|    44971 | 1112 | `			PH7_MemObjLoad(pRoot,&cur);` |
|    44971 | 1113 | `			cur.nIdx = pRoot->nIdx;` |
|    22649 | 1114 | `		}` |
|        - | 1115 | `	}` |
|    47269 | 1116 | `	rc = VmWalkStepsOverValue(&(*pVm),pPath,0,&cur,FALSE,pSlot);` |
|    47269 | 1117 | `	PH7_MemObjRelease(&cur);` |
|    47269 | 1118 | `	return rc;` |
|        5 | 1119 | `}` |
|        - | 1120 | `/*` |
|        - | 1121 | ` * Is this actual argument REFUSED by a by-reference parameter?` |
|        - | 1122 | ` *` |
|        - | 1123 | ` * php answers from the argument's compile-time SHAPE, which the call site carries in` |
|        - | 1124 | ` * VmCallArgMap.nNonLvalMask (GenStateArgShape, compile.c): a literal, an operator or` |
|        - | 1125 | `` * cast result, a class constant, `@$x`, `$o?->p` or an assignment is a hard non-lvalue`` |
|        - | 1126 | `` * and binding one is `Argument #N ($p) could not be passed by reference`.`` |
|        - | 1127 | ` *` |
|        - | 1128 | ` * nPos is the argument's position on the operand stack, which is the position the` |
|        - | 1129 | ` * compiler classified — named arguments change which FORMAL a slot binds to, not the` |
|        - | 1130 | ` * slot's index, so both binders index the mask the same way.` |
|        - | 1131 | ` *` |
|        - | 1132 | ` * Without a shape mask (a SPREAD call, an engine-synthesized call, an indirect dispatch` |
|        - | 1133 | ` * through call_user_func or an array callable) this falls back to the runtime test the` |
|        - | 1134 | ` * binders used before: no slot to write back through, and not one of the values PH7 has` |
|        - | 1135 | ` * always passed by value instead. That test cannot tell a literal from a call RESULT —` |
|        - | 1136 | ` * php accepts the latter — which is exactly why the mask exists.` |
|        - | 1137 | ` */` |
|     7306 | 1138 | `PH7_PRIVATE int PH7_VmArgRefusedByRef(VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1139 | `{` |
|     7311 | 1140 | `	if( pMap && pMap->bArgShapes && nPos < 31 ){` |
|     7087 | 1141 | `		return (pMap->nNonLvalMask & (1u << nPos)) != 0;` |
|        - | 1142 | `	}` |
|      228 | 1143 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|      166 | 1144 | `		return 0;` |
|        - | 1145 | `	}` |
|       93 | 1146 | `	return (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0` |
|       62 | 1147 | `	    && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0;` |
|     3658 | 1148 | `}` |
|        - | 1149 | `/*` |
|        - | 1150 | `` * The same call site's OTHER answer: the argument is the RESULT of a call or of `new`.`` |
|        - | 1151 | ` *` |
|        - | 1152 | ` * php cannot know at compile time whether the callee returns a reference, so it defers` |
|        - | 1153 | ` * to the value: one that arrived WITH a reference binds silently, and one without gets` |
|        - | 1154 | ` * php's E_NOTICE and the callee then operates on the temporary. Emitting it is all this` |
|        - | 1155 | ` * does — a temp-call argument is never refused.` |
|        - | 1156 | ` */` |
|        - | 1157 | `/*` |
|        - | 1158 | ` * A typed by-REFERENCE parameter's coercion belongs to the CALLER's variable. php` |
|        - | 1159 | ` * converts the actual in weak mode and the REFERENCE then holds the conversion, so` |
|        - | 1160 | `` * `$v = 1.0; f($v);` with `function f(int &$x)` leaves both views int(1). PHL ran the`` |
|        - | 1161 | ` * declared-type check on the operand-stack COPY while the binder aliases the caller's` |
|        - | 1162 | ` * slot by index, so the conversion reached neither the callee (which reads through the` |
|        - | 1163 | ` * alias) nor the caller: both stayed float, and every other pair did the same` |
|        - | 1164 | `` * (`float &$y` given an int, `string &$s` given an int, `bool &$b` given an int).`` |
|        - | 1165 | ` *` |
|        - | 1166 | ` * Writes back only when the check actually changed the value's TYPE — an untyped` |
|        - | 1167 | ` * parameter, or one the actual already satisfies, copies nothing.` |
|        - | 1168 | ` */` |
|     3122 | 1169 | `PH7_PRIVATE void PH7_VmByRefArgWriteBack(ph7_vm *pVm,ph7_value *pArg,sxi32 iPreFlags)` |
|        5 | 1170 | `{` |
|        - | 1171 | `	ph7_value *pSlot;` |
|     3122 | 1172 | `	if( pArg->nIdx == SXU32_HIGH` |
|     3127 | 1173 | `	 \|\| (pArg->iFlags & MEMOBJ_ALL) == (iPreFlags & MEMOBJ_ALL) ){` |
|     3103 | 1174 | `		return;` |
|        - | 1175 | `	}` |
|       25 | 1176 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|       25 | 1177 | `	if( pSlot && pSlot != pArg ){` |
|       25 | 1178 | `		PH7_MemObjStore(pArg,pSlot);` |
|       12 | 1179 | `	}` |
|     1566 | 1180 | `}` |
|     6686 | 1181 | `PH7_PRIVATE void PH7_VmArgTempCallNotice(ph7_vm *pVm,VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal)` |
|        5 | 1182 | `{` |
|     6691 | 1183 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| nPos >= 31 ){` |
|      228 | 1184 | `		return;` |
|        - | 1185 | `	}` |
|     6467 | 1186 | `	if( (pMap->nTempCallMask & (1u << nPos)) == 0 ){` |
|     6427 | 1187 | `		return;` |
|        - | 1188 | `	}` |
|       43 | 1189 | `	if( pVal->nIdx != SXU32_HIGH ){` |
|        3 | 1190 | `		return; /* a by-reference RETURN: php is silent and binds it */` |
|        - | 1191 | `	}` |
|       41 | 1192 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Only variables should be passed by reference");` |
|     3348 | 1193 | `}` |
|        - | 1194 | `/*` |
|        - | 1195 | `` * A GENERATOR's arguments are bound at the `g(...)` that BUILDS the Generator object,`` |
|        - | 1196 | ` * before any resume — php's rule, and where php also refuses a by-reference parameter` |
|        - | 1197 | ` * handed a non-variable. That branch collects its actuals into a vector of its own (and` |
|        - | 1198 | ` * reorders it for named arguments), so neither of the two OP_CALL binders ever sees them` |
|        - | 1199 | `` * and `function g(&$x){ yield; } g(1 + 1);` built a Generator in silence.`` |
|        - | 1200 | ` *` |
|        - | 1201 | ` * Answers PH7_EXCEPTION (or PH7_ABORT) for the first refused position, having raised the` |
|        - | 1202 | ` * throw; SXRET_OK otherwise, with php's temp-call notice emitted along the way. Named` |
|        - | 1203 | ` * arguments are resolved by NAME against the formals here rather than through the` |
|        - | 1204 | ` * branch's own mapping, which is built later and freed inside its block.` |
|        - | 1205 | ` */` |
|      120 | 1206 | `static sxi32 VmScreenGenByRefArgs(ph7_vm *pVm,ph7_vm_func *pFunc,VmCallArgMap *pMap,` |
|        - | 1207 | `	ph7_value *pArg,sxu32 nActual,ph7_class *pSelfHint)` |
|        5 | 1208 | `{` |
|      125 | 1209 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      125 | 1210 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|        - | 1211 | `	sxu32 i;` |
|      255 | 1212 | `	for( i = 0 ; i < nActual ; ++i ){` |
|      141 | 1213 | `		sxu32 n = i;` |
|      141 | 1214 | `		if( pMap && pMap->bHasNamed && i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       35 | 1215 | `			for( n = 0 ; n < nFormal ; ++n ){` |
|       30 | 1216 | `				if( pMap->aNames[i].nByte == SyStringLength(&aFormal[n].sName)` |
|       30 | 1217 | `				 && SyMemcmp(pMap->aNames[i].zString,SyStringData(&aFormal[n].sName),` |
|       36 | 1218 | `					pMap->aNames[i].nByte) == 0 ){` |
|       23 | 1219 | `					break;` |
|        - | 1220 | `				}` |
|        8 | 1221 | `			}` |
|       11 | 1222 | `		}` |
|      141 | 1223 | `		if( n >= nFormal \|\| (aFormal[n].iFlags & VM_FUNC_ARG_BY_REF) == 0 ){` |
|       99 | 1224 | `			continue;` |
|        - | 1225 | `		}` |
|       44 | 1226 | `		if( PH7_VmArgRefusedByRef(pMap,i,&pArg[i]) ){` |
|       10 | 1227 | `			sxi32 rcT = VmThrowByRefRefusal(&(*pVm),` |
|        6 | 1228 | `				(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|        6 | 1229 | `				&pFunc->sName,n + 1,&aFormal[n].sName);` |
|        7 | 1230 | `			return (rcT == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 1231 | `		}` |
|       38 | 1232 | `		PH7_VmArgTempCallNotice(&(*pVm),pMap,i,&pArg[i]);` |
|       20 | 1233 | `	}` |
|      119 | 1234 | `	return SXRET_OK;` |
|       65 | 1235 | `}` |
|        - | 1236 | `/*` |
|        - | 1237 | ` * D1: resolve deferred call arguments in [pArg, pTos) before the callee consumes them.` |
|        - | 1238 | ` *` |
|        - | 1239 | `` * A plain `$var` call argument whose callee signature is unknown at compile time is`` |
|        - | 1240 | ` * emitted as a DEFERRED load (OP_LOAD iP1=1,iP2=3): when the variable does not exist it` |
|        - | 1241 | ` * yields a NULL slot tagged MEMOBJ_AUX_DEFERRED that carries the variable name in x.pOther` |
|        - | 1242 | ` * (a VM-lifetime bytecode string). This helper runs at each OP_CALL dispatch branch, while` |
|        - | 1243 | ` * pVm->pFrame is still the CALLER frame, once the callee's by-ref shape is known:` |
|        - | 1244 | ` *` |
|        - | 1245 | ` *   by-ref position  -> create the variable in the caller frame now and give the slot its` |
|        - | 1246 | ` *                       real nIdx so the ordinary by-ref binder aliases it (silent, as php).` |
|        - | 1247 | ` *   by-value position-> raise php's "Undefined variable $x" and pass a clean NULL WITHOUT` |
|        - | 1248 | ` *                       creating the variable in the caller.` |
|        - | 1249 | ` *` |
|        - | 1250 | ` * The by-ref decision for positional argument n comes from, in priority order:` |
|        - | 1251 | ` *   bAllByValue  -> everything by-value (an unresolvable/erroring callee);` |
|        - | 1252 | ` *   bAllByRef    -> everything by-ref (the indirect array-callable/__invoke dispatch paths,` |
|        - | 1253 | ` *                   which historically over-vivified every plain-var arg — preserved here` |
|        - | 1254 | ` *                   rather than regressed; their by-value refinement is a later slice);` |
|        - | 1255 | ` *   pFormal      -> a user function's formal-argument array (honoring a trailing variadic);` |
|        - | 1256 | ` *   nByRefMask   -> a builtin by-ref position bitmask (used when pFormal == 0).` |
|        - | 1257 | ` *` |
|        - | 1258 | ` * A DEFINED variable never carries the marker (it loads with its real nIdx), so this is a` |
|        - | 1259 | ` * no-op for it; a call with no deferred args pays only one flag test per slot.` |
|        - | 1260 | ` */` |
|  8150367 | 1261 | `PH7_PRIVATE sxi32 PH7_VmResolveDeferredArgs(` |
|        - | 1262 | `	ph7_vm *pVm,` |
|        - | 1263 | `	ph7_value *pArg,` |
|        - | 1264 | `	ph7_value *pTos,` |
|        - | 1265 | `	ph7_vm_func_arg *pFormal,` |
|        - | 1266 | `	sxu32 nFormal,` |
|        - | 1267 | `	sxu32 nByRefMask,` |
|        - | 1268 | `	int bAllByRef,` |
|        - | 1269 | `	int bAllByValue,` |
|        - | 1270 | `	VmCallArgMap *pCallMap)` |
|        5 | 1271 | `{` |
|        - | 1272 | `	ph7_value *p;` |
|  8150372 | 1273 | `	sxu32 n = 0;` |
| 18435344 | 1274 | `	for( p = pArg ; p < pTos ; ++p, ++n ){` |
| 10285059 | 1275 | `		int bByRef = 0;` |
| 10285059 | 1276 | `		int bDeferred = (p->iFlags & (MEMOBJ_AUX_DEFERRED\|MEMOBJ_AUX_DEFPATH)) != 0;` |
|        - | 1277 | `		SyString sName;` |
| 10285054 | 1278 | `		if( !bDeferred` |
| 10261450 | 1279 | `		 && (bAllByValue \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0) ){` |
|        - | 1280 | `			/* Nothing deferred in this position, and no slot in the VM could refuse` |
|        - | 1281 | `			 * being aliased -- the ordinary case, out through one test. */` |
|  5863003 | 1282 | `			continue;` |
|        - | 1283 | `		}` |
|  8840853 | 1284 | `		if( bAllByValue ){` |
|      ! 0 | 1285 | `			bByRef = 0;` |
|  8840853 | 1286 | `		}else if( bAllByRef ){` |
|        - | 1287 | `			/* bAllByRef comes ONLY from the array-callable-value and __invoke dispatch paths,` |
|        - | 1288 | `			 * which cannot expose the target's per-parameter by-ref flags here. Commit 1 chose` |
|        - | 1289 | `			 * to over-vivify every deferred PLAIN-VAR arg on those paths (harmless: it just` |
|        - | 1290 | `			 * materializes the caller variable), and that is preserved. But a deferred` |
|        - | 1291 | `			 * ELEMENT/PROPERTY (MEMOBJ_AUX_DEFPATH) must NOT be blanket-vivified there: a missing` |
|        - | 1292 | `			 * property would fatal ("Cannot create dynamic property") and a missing element would` |
|        - | 1293 | `			 * silently vivify + swallow php's "Undefined array key" warning. Resolve those` |
|        - | 1294 | `			 * by-value (warn + pass NULL), which matches php for a by-VALUE __invoke/callable —` |
|        - | 1295 | `			 * the genuine by-ref-out-param-into-an-element case stays unsupported here, exactly` |
|        - | 1296 | `			 * as it was before this slice. */` |
|      ! 0 | 1297 | `			bByRef = (p->iFlags & MEMOBJ_AUX_DEFPATH) ? 0 : 1;` |
|  8840853 | 1298 | `		}else if( pFormal ){` |
|    85094 | 1299 | `			sxu32 idx = n;` |
|    85089 | 1300 | `			if( pCallMap && pCallMap->bHasNamed && n < pCallMap->nTotal` |
|     1322 | 1301 | `			 && pCallMap->aNames[n].nByte > 0 ){` |
|        - | 1302 | `				/* A NAMED actual binds to the formal its NAME picks, not to the one at its` |
|        - | 1303 | ``				 * stack position: `r(x: $a["k"])` is argument #1 on the stack and parameter`` |
|        - | 1304 | `				 * $x in the declaration. Reading the by-ref-ness positionally consulted the` |
|        - | 1305 | `				 * wrong formal, so a by-reference named argument naming a missing element` |
|        - | 1306 | ``				 * warned `Undefined array key` and passed NULL where php creates it. */`` |
|        - | 1307 | `				sxu32 f;` |
|      635 | 1308 | `				idx = SXU32_HIGH;` |
|     1169 | 1309 | `				for( f = 0 ; f < nFormal ; ++f ){` |
|      976 | 1310 | `					if( pCallMap->aNames[n].nByte == SyStringLength(&pFormal[f].sName)` |
|      890 | 1311 | `					 && SyMemcmp(pCallMap->aNames[n].zString,` |
|     1191 | 1312 | `						SyStringData(&pFormal[f].sName),pCallMap->aNames[n].nByte) == 0 ){` |
|      447 | 1313 | `						idx = f;` |
|      447 | 1314 | `						break;` |
|        - | 1315 | `					}` |
|      272 | 1316 | `				}` |
|    84779 | 1317 | `			}else if( idx >= nFormal ){` |
|        - | 1318 | `				/* Beyond the declared formals: a trailing variadic absorbs the tail` |
|        - | 1319 | `				 * (and dictates its by-ref-ness); otherwise the extra arg is by-value. */` |
|     7796 | 1320 | `				idx = (nFormal > 0 && (pFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC))` |
|     6063 | 1321 | `					? nFormal - 1 : SXU32_HIGH;` |
|     2585 | 1322 | `			}` |
|    85094 | 1323 | `			if( idx != SXU32_HIGH ){` |
|    81432 | 1324 | `				bByRef = (pFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|    40806 | 1325 | `			}` |
|    42633 | 1326 | `		}else{` |
|  8755764 | 1327 | `			bByRef = (n < 31 && (nByRefMask & (1u << n))) ? 1 : 0;` |
|        - | 1328 | `		}` |
|  8840853 | 1329 | `		if( !bDeferred ){` |
|        - | 1330 | `			/* An argument that ALREADY resolved to a slot, in a by-reference` |
|        - | 1331 | `			 * position: php screens the property behind it here, because binding` |
|        - | 1332 | `			 * the callee's parameter to it is an INDIRECT modification -- the` |
|        - | 1333 | `			 * callee's write would reach a readonly property, or a native one whose` |
|        - | 1334 | `			 * handler refuses every write, with nothing in the way. This is the one` |
|        - | 1335 | `			 * place both kinds of callee agree on: a user function's formals and a` |
|        - | 1336 | `			 * builtin's signature-derived mask both arrive here. */` |
|  8793303 | 1337 | `			if( bByRef ){` |
|     6293 | 1338 | `				sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),p->nIdx);` |
|     6293 | 1339 | `				if( rcInd != SXRET_OK ){` |
|       50 | 1340 | `					return rcInd;` |
|        - | 1341 | `				}` |
|     3136 | 1342 | `			}` |
|  8793287 | 1343 | `			continue;` |
|        - | 1344 | `		}` |
|    47555 | 1345 | `		if( p->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|        - | 1346 | `			/* D1 commit 2: a deferred array-element/property lvalue. Detach the descriptor` |
|        - | 1347 | `			 * FIRST (so an exception mid-resolve, or a later stack release, cannot double-free` |
|        - | 1348 | `			 * it) then re-walk it in the chosen mode. */` |
|    47457 | 1349 | `			VmDeferredPath *pPath = (VmDeferredPath *)p->x.pOther;` |
|        - | 1350 | `			sxi32 rc;` |
|    47457 | 1351 | `			p->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|    47457 | 1352 | `			p->x.pOther = 0;` |
|    47457 | 1353 | `			MemObjSetType(p,MEMOBJ_NULL);` |
|    47457 | 1354 | `			p->nIdx = SXU32_HIGH;` |
|    47457 | 1355 | `			if( bByRef ){` |
|      191 | 1356 | `				rc = VmResolvePathByRef(&(*pVm),pPath,p);` |
|       97 | 1357 | `			}else{` |
|    47269 | 1358 | `				rc = VmResolvePathByValue(&(*pVm),pPath,p);` |
|        - | 1359 | `			}` |
|    47457 | 1360 | `			VmFreeDeferredPath(pPath);` |
|    47457 | 1361 | `			if( rc != SXRET_OK ){` |
|       70 | 1362 | `				return rc;` |
|        - | 1363 | `			}` |
|    47391 | 1364 | `			continue;` |
|        - | 1365 | `		}` |
|        - | 1366 | `		/* Recover the deferred variable name and drop the marker + carrier. */` |
|      100 | 1367 | `		SyStringInitFromBuf(&sName,(const char *)p->x.pOther,` |
|        - | 1368 | `			p->x.pOther ? SyStrlen((const char *)p->x.pOther) : 0);` |
|      100 | 1369 | `		p->iFlags &= ~MEMOBJ_AUX_DEFERRED;` |
|      100 | 1370 | `		p->x.pOther = 0;` |
|      100 | 1371 | `		if( bByRef ){` |
|        - | 1372 | `			/* Materialize in the caller frame; the value stays NULL, only the slot` |
|        - | 1373 | `			 * back-reference (nIdx) matters for the by-ref binder / write-back. */` |
|       87 | 1374 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|       87 | 1375 | `			if( pObj ){` |
|       87 | 1376 | `				p->nIdx = pObj->nIdx;` |
|       43 | 1377 | `			}` |
|       44 | 1378 | `		}else{` |
|        - | 1379 | `			/* php warns and passes NULL without creating the variable; the slot is` |
|        - | 1380 | `			 * already a clean NULL with nIdx == SXU32_HIGH from the deferred load. */` |
|       14 | 1381 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        - | 1382 | `		}` |
|       51 | 1383 | `	}` |
|  8150290 | 1384 | `	return SXRET_OK;` |
|  4076937 | 1385 | `}` |
|        - | 1386 | `/*` |
|        - | 1387 | ` * Did resolving a class NAME raise?` |
|        - | 1388 | ` *` |
|        - | 1389 | ` * The lookup can run an AUTOLOADER, and that autoloader can throw. The boundary rail` |
|        - | 1390 | ` * either parks the status in nBoundaryRc or — when a try caught it in place — records a` |
|        - | 1391 | ` * resume frame; either way the throw is already the engine's to land. A call site that` |
|        - | 1392 | `` * sees the class "missing" and piles its own `Class "X" not found` Error on top reports a`` |
|        - | 1393 | ` * failure php never reports, and that second Error belongs to nobody: it came back` |
|        - | 1394 | ` * UNCAUGHT and killed the script right after the real exception had been handled.` |
|        - | 1395 | ` *` |
|        - | 1396 | ` * Snapshot (nBoundaryRc, pResumeFrame) before the lookup and pass them here after.` |
|        - | 1397 | ` */` |
|      190 | 1398 | `PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore)` |
|        5 | 1399 | `{` |
|      195 | 1400 | `	return pVm->nBoundaryRc != nBrcBefore \|\| (const void *)pVm->pResumeFrame != pResumeBefore;` |
|        5 | 1401 | `}` |
|        - | 1402 | `/*` |
|        - | 1403 | `` * Name php's error for a class+method callable that the DIRECT `$cb()` dispatch cannot`` |
|        - | 1404 | ` * call, or return 0 when it resolves.` |
|        - | 1405 | ` *` |
|        - | 1406 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|        - | 1407 | ` * result for an unresolvable pair — silence a caller cannot detect — so the direct call` |
|        - | 1408 | ` * site has to decide for itself. It used to do that only for the ARRAY form; the` |
|        - | 1409 | ``  * `"Class::method"` STRING form went straight to the dispatcher, and `$cb='C::nosuch'` `` |
|        - | 1410 | ` * evaluated to NULL with no diagnostic at all where php throws.` |
|        - | 1411 | ` *` |
|        - | 1412 | ` * pClass is the resolved target class (0 when the name named nothing); zCls/nCls is the` |
|        - | 1413 | ` * class name AS WRITTEN, which is what php's not-found message quotes. bStaticForm says the` |
|        - | 1414 | ` * target was a class NAME rather than an object. Messages that interpolate a name are built` |
|        - | 1415 | ` * into zBuf.` |
|        - | 1416 | ` *` |
|        - | 1417 | ` * Visibility is NOT decided here: an inaccessible method is diagnosed downstream by the` |
|        - | 1418 | ` * dispatch itself ("Call to private method C::p() from global scope"), php-exact already —` |
|        - | 1419 | ` * and php reports visibility BEFORE staticness, so the static rule below has to stay quiet` |
|        - | 1420 | ` * for a method this scope could not reach anyway.` |
|        - | 1421 | ` */` |
|   200270 | 1422 | `static const char * VmCallableClassMethodError(` |
|        - | 1423 | `	ph7_vm *pVm,` |
|        - | 1424 | `	ph7_class *pClass,             /* Resolved target class, or 0 */` |
|        - | 1425 | `	const char *zCls,sxu32 nCls,   /* Its name as the callable wrote it */` |
|        - | 1426 | `	const char *zMeth,sxu32 nMeth, /* The method name */` |
|        - | 1427 | `	int bStaticForm,               /* TRUE when the target is a class NAME, not an object */` |
|        - | 1428 | `	char *zBuf,int nBuf            /* Scratch for the messages that quote a name */` |
|        - | 1429 | `	)` |
|        4 | 1430 | `{` |
|        - | 1431 | `	ph7_class_method *pMethod;` |
|        - | 1432 | `	SyString sMeth;` |
|        - | 1433 | `` 	/* php's fallback for a name this class cannot reach: when the CALLER holds a `$this` `` |
|        - | 1434 | `	 * that is an instance of it, the name resolves to the __call TRAMPOLINE rather than to` |
|        - | 1435 | `	 * __callStatic — and a trampoline is a NON-STATIC function, so this dispatch, which` |
|        - | 1436 | `	 * carries no object, refuses it exactly as it refuses any other non-static method named` |
|        - | 1437 | `	 * through a class. The callback spellings bind that receiver and run (php's` |
|        - | 1438 | `	 * direct-vs-callback asymmetry, one rule apart). The message names the class the` |
|        - | 1439 | `	 * callable WROTE and the name as written, even when the name is a declared static` |
|        - | 1440 | `	 * method: it is the trampoline being refused, not the method. */` |
|   200274 | 1441 | `	int bFallback = bStaticForm && PH7_VmStaticFallbackThis(&(*pVm),pClass) != 0;` |
|   200274 | 1442 | `	if( pClass == 0 ){` |
|       49 | 1443 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|       49 | 1444 | `		return zBuf;` |
|        - | 1445 | `	}` |
|   200227 | 1446 | `	pMethod = PH7_ClassExtractMethod(pClass,zMeth,nMeth);` |
|   200227 | 1447 | `	if( pMethod == 0 ){` |
|       79 | 1448 | `		if( bFallback ){` |
|        7 | 1449 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|        2 | 1450 | `				&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1451 | `			return zBuf;` |
|        - | 1452 | `		}` |
|        - | 1453 | `		/* A class that answers for unknown names through the catch-all has nothing to` |
|        - | 1454 | `		 * report: php runs __callStatic (class-name target) / __call (object target) for` |
|        - | 1455 | `		 * ANY method name, and the dispatcher below routes it. */` |
|       75 | 1456 | `		const char *zMagic = bStaticForm ? "__callStatic" : "__call";` |
|       75 | 1457 | `		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){` |
|       47 | 1458 | `			return 0;` |
|        - | 1459 | `		}` |
|       43 | 1460 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",` |
|       14 | 1461 | `			&pClass->sName,(int)nMeth,zMeth);` |
|       29 | 1462 | `		return zBuf;` |
|        - | 1463 | `	}` |
|        - | 1464 | `	/* An ABSTRACT method (an interface's included) has no body to call — the same message` |
|        - | 1465 | ``	 * the `C::m()` SYNTAX raises in vm_ops_oo.c, which this dispatch reached only as a`` |
|        - | 1466 | `	 * mangled internal function name. */` |
|   200149 | 1467 | `	SyStringInitFromBuf(&sMeth,zMeth,nMeth);` |
|   200149 | 1468 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        7 | 1469 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%z()",&pClass->sName,&sMeth);` |
|        7 | 1470 | `		return zBuf;` |
|        - | 1471 | `	}` |
|        - | 1472 | `	/* Named through a class NAME, a non-static method is never callable: php refuses even` |
|        - | 1473 | `	 * when the CALLER has a compatible $this (unlike call_user_func, which binds it). The` |
|        - | 1474 | `	 * message names the OWNING class and the method's declared spelling. */` |
|        - | 1475 | `	{` |
|        - | 1476 | `		SyString sDecl;` |
|        - | 1477 | `		int bAccessible;` |
|   200143 | 1478 | `		SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1479 | `			SyStringLength(&pMethod->sFunc.sName));` |
|   300232 | 1480 | `		bAccessible = pMethod->iProtection == PH7_CLASS_PROT_PUBLIC` |
|   200140 | 1481 | `			\|\| PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       19 | 1482 | `				&sDecl,pMethod->iProtection,FALSE);` |
|   200143 | 1483 | `		if( !bAccessible && bFallback ){` |
|        - | 1484 | `			/* Inaccessible goes the same way as missing: php never reports the visibility,` |
|        - | 1485 | `			 * because the name resolved to the trampoline before visibility could matter. */` |
|       13 | 1486 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%.*s() cannot be called statically",` |
|        4 | 1487 | `				&pClass->sName,(int)nMeth,zMeth);` |
|       17 | 1488 | `			return zBuf;` |
|        - | 1489 | `		}` |
|   200135 | 1490 | `		if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 && bAccessible ){` |
|        - | 1491 | `			/* Deciding class vs NAMED class: a trait is php's compile-time construct, so` |
|        - | 1492 | `			 * every message names the class that composed it (PH7_VmMethodScopeName). */` |
|       25 | 1493 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|       16 | 1494 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|       17 | 1495 | `			return zBuf;` |
|        - | 1496 | `		}` |
|        - | 1497 | `	}` |
|   200119 | 1498 | `	return 0;` |
|   100139 | 1499 | `}` |
|        - | 1500 | `/*` |
|        - | 1501 | ` * php's visibility refusal for a method call, worded once: "Call to private A::m() from` |
|        - | 1502 | ` * scope S" (or "from global scope"). Two sites raise it — OP_CALL's screen and the` |
|        - | 1503 | ` * first-class-callable one below — and php names the DECLARING class, not the class the` |
|        - | 1504 | ` * lookup went through.` |
|        - | 1505 | ` */` |
|       24 | 1506 | `static const char * VmMethodVisibilityMsg(ph7_vm *pVm,ph7_class *pDecl,` |
|        - | 1507 | `	const char *zMeth,sxu32 nMeth,sxi32 iProtection,char *zBuf,int nBuf)` |
|        2 | 1508 | `{` |
|       26 | 1509 | `	const char *zVis = iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       26 | 1510 | `	ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));` |
|       26 | 1511 | `	if( pScope ){` |
|        4 | 1512 | `		SyBufferFormat(zBuf,nBuf,"Call to %s method %z::%.*s() from scope %z",` |
|        1 | 1513 | `			zVis,&pDecl->sName,(int)nMeth,zMeth,&pScope->sName);` |
|        2 | 1514 | `	}else{` |
|       35 | 1515 | `		SyBufferFormat(zBuf,nBuf,"Call to %s method %z::%.*s() from global scope",` |
|       11 | 1516 | `			zVis,&pDecl->sName,(int)nMeth,zMeth);` |
|        - | 1517 | `	}` |
|       26 | 1518 | `	return zBuf;` |
|        2 | 1519 | `}` |
|        - | 1520 | `/*` |
|        - | 1521 | ` * Is this class+method pair one a call would reach DIRECTLY from here — a real method (not` |
|        - | 1522 | ` * abstract, not a name only the catch-all answers) that the current scope may call? php` |
|        - | 1523 | ` * decides exactly this when it BUILDS a method Closure, and stores the resolved function; the` |
|        - | 1524 | ` * answer is what the VM_INSTANCE_FCC_SCREENED mark records, so the invocation never asks again.` |
|        - | 1525 | ` * A pair that answers FALSE here is the __call/__callStatic trampoline's, and its closure must` |
|        - | 1526 | ` * keep routing there.` |
|        - | 1527 | ` */` |
|      206 | 1528 | `PH7_PRIVATE int PH7_VmFccMethodIsDirect(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName)` |
|        4 | 1529 | `{` |
|        - | 1530 | `	ph7_class_method *pMethod;` |
|        - | 1531 | `	SyString sDecl;` |
|      210 | 1532 | `	if( pClass == 0 \|\| nName < 1 ){` |
|      ! 0 | 1533 | `		return 0;` |
|        - | 1534 | `	}` |
|      210 | 1535 | `	pMethod = PH7_ClassExtractMethod(pClass,zName,nName);` |
|      210 | 1536 | `	if( pMethod == 0 \|\| (pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       17 | 1537 | `		return 0;` |
|        - | 1538 | `	}` |
|      194 | 1539 | `	if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|      125 | 1540 | `		return 1;` |
|        - | 1541 | `	}` |
|       72 | 1542 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1543 | `		SyStringLength(&pMethod->sFunc.sName));` |
|        - | 1544 | `	/* The OWNING class decides (a trait method is owned by the class that composed it) — the` |
|        - | 1545 | `	 * same argument every other visibility site passes. */` |
|      106 | 1546 | `	return PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       68 | 1547 | `		&sDecl,pMethod->iProtection,FALSE) ? 1 : 0;` |
|      107 | 1548 | `}` |
|        - | 1549 | `/*` |
|        - | 1550 | `` * Resolve `$o->m(...)` / `C::m(...)` the way php resolves the CALL it stands for, and say`` |
|        - | 1551 | ` * why when it cannot. php builds a first-class callable through the same member lookup a` |
|        - | 1552 | ` * real call goes through, so every refusal a call would raise happens HERE, at creation:` |
|        - | 1553 | ` * an undefined method, an inaccessible one, an abstract one, and a non-static one named` |
|        - | 1554 | ` * through a class with no receiver to run on. PHL created a Closure for all four and only` |
|        - | 1555 | ` * discovered the problem when (and if) it was invoked — a closure that is built and dropped` |
|        - | 1556 | ` * reported nothing at all.` |
|        - | 1557 | ` *` |
|        - | 1558 | ` * The receiver is the other half of the same lookup. php's ZEND_INIT_STATIC_METHOD_CALL` |
|        - | 1559 | `` * binds the CALLING frame's `$this` when the resolved method is non-static and that object`` |
|        - | 1560 | `` * is an instance of the named class, which is what makes `self::m(...)` inside an instance`` |
|        - | 1561 | ` * method a working callable rather than a static one; PHL bound only the scope, so the` |
|        - | 1562 | ` * closure could never run. *ppRecv is that object, or 0 for a genuinely static callable.` |
|        - | 1563 | ` *` |
|        - | 1564 | ` * Answers 0 when the callable is valid. Messages that quote a name are built into zBuf.` |
|        - | 1565 | ` */` |
|      178 | 1566 | `static const char * VmFccMemberError(ph7_vm *pVm,ph7_class *pClass,` |
|        - | 1567 | `	const char *zCls,sxu32 nCls,const char *zMeth,sxu32 nMeth,int bStaticForm,` |
|        - | 1568 | `	ph7_class_instance **ppRecv,char *zBuf,int nBuf)` |
|        4 | 1569 | `{` |
|        - | 1570 | `	ph7_class_method *pMethod;` |
|        - | 1571 | `	SyString sDecl;` |
|      182 | 1572 | `	*ppRecv = 0;` |
|      182 | 1573 | `	if( pClass == 0 ){` |
|        3 | 1574 | `		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);` |
|        3 | 1575 | `		return zBuf;` |
|        - | 1576 | `	}` |
|      180 | 1577 | `	pMethod = nMeth > 0 ? PH7_ClassExtractMethod(pClass,zMeth,nMeth) : 0;` |
|      180 | 1578 | `	if( pMethod == 0 ){` |
|        - | 1579 | `		/* A name the class answers through the catch-all is callable, and the catch-all` |
|        - | 1580 | `		 * the STATIC spelling reaches depends on the receiver, exactly as it does for a` |
|        - | 1581 | `		 * call (PH7_VmStaticFallbackThis). */` |
|       17 | 1582 | `		if( bStaticForm ){` |
|       11 | 1583 | `			*ppRecv = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|       10 | 1584 | `			if( *ppRecv` |
|       10 | 1585 | `			 \|\| PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1) ){` |
|        9 | 1586 | `				return 0;` |
|        1 | 1587 | `			}` |
|        8 | 1588 | `		}else if( PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) ){` |
|        5 | 1589 | `			return 0;` |
|        - | 1590 | `		}` |
|        7 | 1591 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",` |
|        2 | 1592 | `			&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1593 | `		return zBuf;` |
|        - | 1594 | `	}` |
|      164 | 1595 | `	SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),` |
|        - | 1596 | `		SyStringLength(&pMethod->sFunc.sName));` |
|      164 | 1597 | `	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1598 | `		/* Named through the CLASS only: an instance of an abstract class cannot exist, so` |
|        - | 1599 | `		 * the object spelling never reaches an abstract body. */` |
|        7 | 1600 | `		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%.*s()",` |
|        2 | 1601 | `			&pClass->sName,(int)nMeth,zMeth);` |
|        5 | 1602 | `		return zBuf;` |
|        - | 1603 | `	}` |
|      156 | 1604 | `	if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      116 | 1605 | `	 && !PH7_VmClassMemberAccess(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|       34 | 1606 | `			&sDecl,pMethod->iProtection,FALSE) ){` |
|        - | 1607 | `		/* Inaccessible: the catch-all answers for it, on the same receiver a call would use. */` |
|       12 | 1608 | `		*ppRecv = bStaticForm ? PH7_VmStaticFallbackThis(&(*pVm),pClass) : 0;` |
|       12 | 1609 | `		if( *ppRecv ){` |
|      ! 0 | 1610 | `			return 0;` |
|        - | 1611 | `		}` |
|       12 | 1612 | `		if( !bStaticForm && PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1) ){` |
|        3 | 1613 | `			return 0;` |
|        - | 1614 | `		}` |
|        - | 1615 | `		/* The DECIDING class is the declaring one (its trait grants live there); the class` |
|        - | 1616 | `		 * php NAMES is the composing one — a trait has no runtime existence in php. */` |
|       14 | 1617 | `		return VmMethodVisibilityMsg(&(*pVm),PH7_VmMethodScopeName(&(*pVm),pClass,pMethod),` |
|        4 | 1618 | `			zMeth,nMeth,pMethod->iProtection,zBuf,nBuf);` |
|        - | 1619 | `	}` |
|      150 | 1620 | `	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|       15 | 1621 | `		*ppRecv = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|       15 | 1622 | `		if( *ppRecv == 0 ){` |
|        7 | 1623 | `			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",` |
|        4 | 1624 | `				&PH7_VmMethodScopeName(&(*pVm),pClass,pMethod)->sName,&sDecl);` |
|        5 | 1625 | `			return zBuf;` |
|        - | 1626 | `		}` |
|        5 | 1627 | `	}` |
|      146 | 1628 | `	return 0;` |
|       93 | 1629 | `}` |
|        - | 1630 | `/*` |
|        - | 1631 | ` * The same check for the ARRAY form, whose two members carry php's own shape messages` |
|        - | 1632 | ` * before anything is resolved: the target must be an object or a class-name string, the` |
|        - | 1633 | `` * method must be a string. php probes them in that order (`[5,5]` names the FIRST member,`` |
|        - | 1634 | `` * `['NoSuch',5]` the SECOND — the member shape decides before the class is looked up).`` |
|        - | 1635 | ` */` |
|   100188 | 1636 | `static const char * VmDirectArrayCallableError(ph7_vm *pVm,ph7_value *pTarget,ph7_value *pMethod,` |
|        - | 1637 | `	char *zBuf,int nBuf)` |
|        4 | 1638 | `{` |
|        - | 1639 | `	ph7_class *pClass;` |
|   100192 | 1640 | `	if( (pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|       11 | 1641 | `		return "First array member is not a valid class name or object";` |
|        - | 1642 | `	}` |
|   100182 | 1643 | `	if( (pMethod->iFlags & MEMOBJ_STRING) == 0 ){` |
|        9 | 1644 | `		return "Second array member is not a valid method";` |
|        - | 1645 | `	}` |
|   100174 | 1646 | `	pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   150259 | 1647 | `	return VmCallableClassMethodError(&(*pVm),pClass,` |
|   100170 | 1648 | `		(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|   100170 | 1649 | `		(const char *)SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob),` |
|        - | 1650 | `		/* An OBJECT target carries its own $this; only a class NAME is the static form. */` |
|   100170 | 1651 | `		(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|    50085 | 1652 | `		zBuf,nBuf);` |
|    50098 | 1653 | `}` |
|        - | 1654 | `/*` |
|        - | 1655 | ` * The by-reference SHAPE of the callee an INDIRECT dispatch is about to reach — an` |
|        - | 1656 | `` * array callable VALUE (`$cb = [$o,'m']; $cb($a['k']);`) and an __invoke object.`` |
|        - | 1657 | ` * Both go through a shared helper that hides the target from OP_CALL, so the two` |
|        - | 1658 | ` * sites used to materialize EVERY deferred plain-var argument by reference and every` |
|        - | 1659 | ` * deferred element/property by value: a genuine by-ref out-param into an element was` |
|        - | 1660 | `` * unsupported (`$cb($a['new'])` warned `Undefined array key` and handed the callee a`` |
|        - | 1661 | ` * NULL where php creates the element and writes it), and a by-VALUE parameter` |
|        - | 1662 | `` * swallowed php's `Undefined variable` and CREATED the caller's variable.`` |
|        - | 1663 | ` *` |
|        - | 1664 | ` * The target is knowable here: the pair resolves to a class and a method, an object to` |
|        - | 1665 | ` * its __invoke. Answers 0 when nothing resolves — a name routed through` |
|        - | 1666 | ` * __call/__callStatic (php packs those into an ARRAY, so they are by-value anyway) or` |
|        - | 1667 | ` * a pair the screen above is about to refuse.` |
|        - | 1668 | ` */` |
|   300334 | 1669 | `static ph7_vm_func * VmIndirectCalleeFunc(ph7_vm *pVm,ph7_value *pCallable)` |
|        4 | 1670 | `{` |
|   300338 | 1671 | `	ph7_class_method *pMeth = 0;` |
|   300338 | 1672 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|   100247 | 1673 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCallable->x.pOther;` |
|   100247 | 1674 | `		ph7_value *pTarget = 0,*pName = 0;` |
|        - | 1675 | `		ph7_class *pClass;` |
|   100244 | 1676 | `		if( pMap == 0 \|\| pMap->nEntry != 2` |
|   100244 | 1677 | `		 \|\| !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pName)` |
|   100247 | 1678 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1679 | `			return 0;` |
|        - | 1680 | `		}` |
|   100247 | 1681 | `		pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|   100247 | 1682 | `		if( pClass == 0 ){` |
|      ! 0 | 1683 | `			return 0;` |
|        - | 1684 | `		}` |
|   150369 | 1685 | `		pMeth = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pName->sBlob),` |
|   100244 | 1686 | `			SyBlobLength(&pName->sBlob));` |
|   250216 | 1687 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|   200094 | 1688 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCallable->x.pOther;` |
|   200094 | 1689 | `		if( pThis == 0 ){` |
|      ! 0 | 1690 | `			return 0;` |
|        - | 1691 | `		}` |
|   200094 | 1692 | `		pMeth = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|   100045 | 1693 | `	}` |
|   300338 | 1694 | `	return pMeth ? &pMeth->sFunc : 0;` |
|   150171 | 1695 | `}` |
|        - | 1696 | `/*` |
|        - | 1697 | ` * Materialize an indirect dispatch's deferred arguments against that callee — the same` |
|        - | 1698 | ` * split OP_CALL makes for a direct one: a native method's by-ref positions come from its` |
|        - | 1699 | ` * signature mask, a PHP one's from its compiled formals, and an unresolved callee binds` |
|        - | 1700 | ` * everything by value (php's answer for the magic route it is about to take).` |
|        - | 1701 | ` */` |
|   300334 | 1702 | `static sxi32 VmResolveIndirectArgs(ph7_vm *pVm,ph7_value *pCallable,ph7_value *pArg,ph7_value *pTos,` |
|        - | 1703 | `	VmCallArgMap *pCallMap)` |
|        4 | 1704 | `{` |
|   300338 | 1705 | `	ph7_vm_func *pFn = VmIndirectCalleeFunc(&(*pVm),pCallable);` |
|   300338 | 1706 | `	if( pFn == 0 ){` |
|   100042 | 1707 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pCallMap);` |
|        - | 1708 | `	}` |
|   200298 | 1709 | `	if( pFn->iFlags & VM_FUNC_NATIVE ){` |
|        7 | 1710 | `		return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|        6 | 1711 | `			pFn->pNative ? pFn->pNative->nByRefMask : 0,0,0,pCallMap);` |
|        - | 1712 | `	}` |
|   300436 | 1713 | `	return PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   200288 | 1714 | `		(ph7_vm_func_arg *)SySetBasePtr(&pFn->aArgs),SySetUsed(&pFn->aArgs),0,0,0,pCallMap);` |
|   150171 | 1715 | `}` |
|        - | 1716 | `/*` |
|        - | 1717 | `` * Why a VALUE cannot be made into a first-class callable. php answers `($v)(...)` with`` |
|        - | 1718 | `` * exactly what it answers `($v)()` — the taxonomy is the DIRECT dispatch's, word for word —`` |
|        - | 1719 | ` * so this walks the same three shapes the OP_CALL sites do and reuses their builders. PHL` |
|        - | 1720 | `` * left a non-callable value STANDING instead: `$x = 5; $f = ($x)(...);` evaluated to int(5),`` |
|        - | 1721 | ` * an array to the array, a misspelled function name to its own string — a value that is not` |
|        - | 1722 | ` * a Closure where php throws, silently, on every shape.` |
|        - | 1723 | ` *` |
|        - | 1724 | ` * Returns 0 when the value IS callable (unreachable through the FCC caller, which asks only` |
|        - | 1725 | ` * after the wrap declined, but it keeps the helper honest for a direct reader).` |
|        - | 1726 | ` */` |
|       74 | 1727 | `static const char * VmFccValueError(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf)` |
|        4 | 1728 | `{` |
|       78 | 1729 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       23 | 1730 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       23 | 1731 | `		ph7_value *pTarget = 0,*pMeth = 0;` |
|        - | 1732 | `		const char *zWhy;` |
|       23 | 1733 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|        9 | 1734 | `			return "Array callback must have exactly two elements";` |
|        - | 1735 | `		}` |
|       15 | 1736 | `		if( !PH7_VmArrayCallableParts(&(*pVm),pMap,&pTarget,&pMeth) ){` |
|        3 | 1737 | `			return "Array callback has to contain indices 0 and 1";` |
|        - | 1738 | `		}` |
|       13 | 1739 | `		zWhy = VmDirectArrayCallableError(&(*pVm),pTarget,pMeth,zBuf,nBuf);` |
|       13 | 1740 | `		if( zWhy ){` |
|        9 | 1741 | `			return zWhy;` |
|        - | 1742 | `		}` |
|        - | 1743 | `		/* That check deliberately leaves VISIBILITY to OP_CALL's own screen, which raises it` |
|        - | 1744 | `		 * when the pair is finally called — and a first-class callable never gets there: php` |
|        - | 1745 | ``		 * refuses `[$o,'priv'](...)` at the creation, with the direct dispatch's wording.`` |
|        - | 1746 | `		 * Reached only for a pair PH7_VmIsCallable already declined, so a class routing the` |
|        - | 1747 | `		 * name through __call (which makes it callable) cannot arrive here. */` |
|        5 | 1748 | `		if( (pMeth->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMeth->sBlob) > 0 ){` |
|        5 | 1749 | `			ph7_class *pCbCls = PH7_VmExtractClassFromValue(&(*pVm),pTarget);` |
|        5 | 1750 | `			const char *zM = (const char *)SyBlobData(&pMeth->sBlob);` |
|        5 | 1751 | `			sxu32 nM = SyBlobLength(&pMeth->sBlob);` |
|        5 | 1752 | `			ph7_class_method *pCbMeth = pCbCls ? PH7_ClassExtractMethod(pCbCls,zM,nM) : 0;` |
|        4 | 1753 | `			if( pCbMeth && pCbMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|        5 | 1754 | `			 && !PH7_VmFccMethodIsDirect(&(*pVm),pCbCls,zM,nM) ){` |
|        7 | 1755 | `				return VmMethodVisibilityMsg(&(*pVm),` |
|        2 | 1756 | `					PH7_VmMethodScopeName(&(*pVm),pCbCls,pCbMeth),` |
|        2 | 1757 | `					zM,nM,pCbMeth->iProtection,zBuf,nBuf);` |
|        - | 1758 | `			}` |
|      ! 0 | 1759 | `		}` |
|      ! 0 | 1760 | `		return 0;` |
|        - | 1761 | `	}` |
|       56 | 1762 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       15 | 1763 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|       15 | 1764 | `		if( pObj == 0 ){` |
|      ! 0 | 1765 | `			return "Value of type object is not callable";` |
|        - | 1766 | `		}` |
|       15 | 1767 | `		if( PH7_ClassExtractMethod(pObj->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|      ! 0 | 1768 | `			return 0;` |
|        - | 1769 | `		}` |
|       15 | 1770 | `		SyBufferFormat(zBuf,nBuf,"Object of type %z is not callable",&pObj->pClass->sName);` |
|       15 | 1771 | `		return zBuf;` |
|        - | 1772 | `	}` |
|       42 | 1773 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       30 | 1774 | `		const char *zCls = 0,*zMeth = 0;` |
|       30 | 1775 | `		sxu32 nCls = 0,nMeth = 0;` |
|        - | 1776 | `		SyString sName;` |
|       30 | 1777 | `		SyStringInitFromBuf(&sName,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|        - | 1778 | `		/* A leading backslash only anchors the name to the global namespace. */` |
|       30 | 1779 | `		if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|      ! 0 | 1780 | `			sName.zString++;` |
|      ! 0 | 1781 | `			sName.nByte--;` |
|      ! 0 | 1782 | `		}` |
|       30 | 1783 | `		if( PH7_VmCallableStringParts(sName.zString,sName.nByte,&zCls,&nCls,&zMeth,&nMeth) ){` |
|        - | 1784 | `			/* "Class::method" carries the class/method taxonomy, not the function one. */` |
|        7 | 1785 | `			return VmCallableClassMethodError(&(*pVm),` |
|        2 | 1786 | `				PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0),` |
|        2 | 1787 | `				zCls,nCls,zMeth,nMeth,TRUE,zBuf,nBuf);` |
|        - | 1788 | `		}` |
|       26 | 1789 | `		SyBufferFormat(zBuf,nBuf,"Call to undefined function %z()",&sName);` |
|       26 | 1790 | `		return zBuf;` |
|        - | 1791 | `	}` |
|       13 | 1792 | `	SyBufferFormat(zBuf,nBuf,"Value of type %s is not callable",VmArithTypeName(pValue));` |
|       13 | 1793 | `	return zBuf;` |
|       41 | 1794 | `}` |
|        - | 1795 | `/*` |
|        - | 1796 | `` * Split a `"Class::method"` callable string. php scans for the LAST "::" (zend_memrchr),`` |
|        - | 1797 | `` * so `"C::s::x"` names the class `C::s` — not `C` — and reports it not found. Returns TRUE`` |
|        - | 1798 | `` * and the two halves (either may be empty: `"C::"` and `"::s"` are shapes php accepts here`` |
|        - | 1799 | ` * and rejects further down), FALSE when the string carries no "::" at all.` |
|        - | 1800 | ` */` |
|  4948223 | 1801 | `PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,` |
|        - | 1802 | `	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth)` |
|        5 | 1803 | `{` |
|        - | 1804 | `	sxu32 i;` |
| 39007365 | 1805 | `	for( i = nName ; i >= 2 ; --i ){` |
| 34259342 | 1806 | `		if( zName[i-2] == ':' && zName[i-1] == ':' ){` |
|   200205 | 1807 | `			*pzCls = zName;` |
|   200205 | 1808 | `			*pnCls = i - 2;` |
|   200205 | 1809 | `			*pzMeth = &zName[i];` |
|   200205 | 1810 | `			*pnMeth = nName - i;` |
|   200205 | 1811 | `			return TRUE;` |
|        - | 1812 | `		}` |
| 17042982 | 1813 | `	}` |
|  4748028 | 1814 | `	return FALSE;` |
|  2475863 | 1815 | `}` |
|  4614496 | 1816 | `static sxi32 VmByteCodeExecBody(` |
|        - | 1817 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 1818 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - | 1819 | `	ph7_value *pStack,   /* Operand stack */` |
|        - | 1820 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - | 1821 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - | 1822 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - | 1823 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - | 1824 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - | 1825 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - | 1826 | `	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */` |
|        - | 1827 | `	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */` |
|        - | 1828 | `	ph7_value **ppBaseOwner, /* Base (pCallTop==0) operand-stack owner slot the native entry frees; OP_SPREAD growth of the base stack writes the new pointer here (see VmGrowOperandStack). */` |
|        - | 1829 | `	sxu32 *pnBaseCap, /* Base stack capacity (persisted across coroutine suspend/resume); read for the initial capacity and updated on base-stack growth. */` |
|        - | 1830 | `	sxu32 nStackOrig /* Base stack's ORIGINAL (ungrown) allocation size — the fixed headroom reference for OP_SPREAD growth (see nStackOrig in VmExecState). */` |
|        - | 1831 | `	)` |
|        5 | 1832 | `{` |
|        - | 1833 | `	VmInstr *pInstr;` |
|        - | 1834 | `	ph7_value *pTos;` |
|        - | 1835 | `	SySet aArg;` |
|  4614501 | 1836 | `	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record` |
|        - | 1837 | `	                            * stack (BYTECODE stage 2); NULL = executing the` |
|        - | 1838 | `	                            * bottom activation. */` |
|        - | 1839 | `	VmExecState sState; /* This activation's boundary state (BYTECODE.md stage 1):` |
|        - | 1840 | `	                     * everything a suspended/nested activation must restore.` |
|        - | 1841 | `	                     * pc/pTos stay in locals for the hot loop and are synced` |
|        - | 1842 | `	                     * into sState only around the call epilogue (stage 2 turns` |
|        - | 1843 | `	                     * that boundary into an explicit record push/pop). */` |
|        - | 1844 | `	sxi32 pc;` |
|        - | 1845 | `	sxi32 rc;` |
|  4614501 | 1846 | `	sState.aInstr = aInstr;` |
|  4614501 | 1847 | `	sState.pStack = pStack;` |
|  4614501 | 1848 | `	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */` |
|  4614501 | 1849 | `	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */` |
|  4614501 | 1850 | `	sState.pResult = pResult;` |
|  4614501 | 1851 | `	sState.pLastRef = pLastRef;` |
|  4614501 | 1852 | `	sState.pEnforceRetFunc = pEnforceRetFunc;` |
|  4614501 | 1853 | `	sState.is_callback = (sxu8)(is_callback ? 1 : 0);` |
|  4614501 | 1854 | `	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);` |
|        - | 1855 | `	/* Argument container */` |
|  4614501 | 1856 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|  4614501 | 1857 | `	if( nTos < 0 ){` |
|  1515690 | 1858 | `		pTos = &pStack[-1];` |
|   757847 | 1859 | `	}else{` |
|  3098816 | 1860 | `		pTos = &pStack[nTos];` |
|        - | 1861 | `	}` |
|  4614501 | 1862 | `	sState.pTos = pTos;` |
|  4614501 | 1863 | `	sState.pc = nPc;` |
|        - | 1864 | `	/* Finally-drain base. For a resumed generator/fiber TOP-LEVEL body, its own` |
|        - | 1865 | `	 * exception handlers were just re-published above the caller depth` |
|        - | 1866 | `	 * (VmRestoreCtxState), so the live SySetUsed over-counts; take the` |
|        - | 1867 | `	 * caller-depth base recorded on the ctx instead.` |
|        - | 1868 | `	 *` |
|        - | 1869 | `	 * The discriminator is the OPERAND STACK, not the frame: the body exec runs on` |
|        - | 1870 | `	 * the ctx's own pStack, whereas every nested frame-less mini-program run within` |
|        - | 1871 | `	 * it — a default-argument / constructor trampoline, or a catch/finally body via` |
|        - | 1872 | `	 * VmLocalExec — runs on a FRESH operand stack while SHARING pVm->pFrame (those` |
|        - | 1873 | `	 * mini-programs do not push a VM frame). A pFrame-only guard therefore misfires` |
|        - | 1874 | `	 * for such a mini-program and hands it the generator's low caller-base; its` |
|        - | 1875 | `	 * terminal OP_DONE then drains VmDrainFinally down to that base, tearing down a` |
|        - | 1876 | ``	 * live try that the surrounding finally had just opened (e.g. `finally { try {`` |
|        - | 1877 | ``	 * throw new E(); } catch (E) {} }` in a generator: the `new E()` trampoline's`` |
|        - | 1878 | `	 * OP_DONE popped the inner try before its OP_THROW ran, so the throw escaped` |
|        - | 1879 | `	 * uncaught). Requiring pStack == pCtx->pStack pins the override to the resumed` |
|        - | 1880 | `	 * body itself; nested mini-programs fall through to the correct live depth. */` |
|  4614496 | 1881 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame` |
|     2595 | 1882 | `	 && pStack == pVm->pActiveCtx->pStack ){` |
|     2089 | 1883 | `		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;` |
|     2089 | 1884 | `		sState.nFinallyActBase = pVm->pActiveCtx->nFinallyBase;` |
|     1047 | 1885 | `	}else{` |
|  4612417 | 1886 | `		sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|  4612417 | 1887 | `		sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|        - | 1888 | `	}` |
|  4614501 | 1889 | `	sState.pEntryFrame = pVm->pFrame;` |
|  4614501 | 1890 | `	pc = nPc;` |
|        - | 1891 | `	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call` |
|        - | 1892 | `	 * adopts its parked record segment here. VmResumeCtx hands the segment in` |
|        - | 1893 | `	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a` |
|        - | 1894 | `	 * nested mini-program/callback run inside the resumed body — after it rebased` |
|        - | 1895 | `	 * the segment's exception floors and pushed the resume value into the innermost` |
|        - | 1896 | `	 * stack. Locals switch to the innermost activation so the dispatch loop` |
|        - | 1897 | `	 * continues inside the callee; the record chain is restored so its completion` |
|        - | 1898 | `	 * unwinds back through the body. */` |
|  4614501 | 1899 | `	if( pAdoptSegment ){` |
|      106 | 1900 | `		VmParkedSegment *pSeg = pAdoptSegment;` |
|      106 | 1901 | `		pCallTop = pSeg->pCallTop;` |
|      106 | 1902 | `		sState = pSeg->sState;` |
|      106 | 1903 | `		aInstr = sState.aInstr;` |
|      106 | 1904 | `		pStack = sState.pStack;` |
|        - | 1905 | `		/* pc is already nPc (== pCtx->pc, the innermost's post-suspend pc) from the` |
|        - | 1906 | `		 * init above; only the stack/top move to the innermost activation. */` |
|      106 | 1907 | `		pTos = &pStack[nTos];   /* nTos == pCtx->nTos — innermost, resume value pushed */` |
|      106 | 1908 | `		SyMemBackendFree(&pVm->sAllocator,pSeg); /* holder only; its contents are now live */` |
|       51 | 1909 | `	}` |
|        - | 1910 | `/*` |
|        - | 1911 | ` * Route an enforcement helper's (or yield-from delegate's) return code from inside` |
|        - | 1912 | ` * the main switch: proceed on SXRET_OK, abort on PH7_ABORT, and on PH7_EXCEPTION` |
|        - | 1913 | ` * resume at the landing pad of the body that actually caught the exception in place` |
|        - | 1914 | ` * (VmRecordedResume) or, when it was caught by an outer exec, unwind out of the VM` |
|        - | 1915 | ` * loop. Replaces the old "jump to pVm->pFrame's nearest iExceptionJump" which ran` |
|        - | 1916 | ` * the statement after the try even when the catch was at an enclosing frame (ROOT B,` |
|        - | 1917 | `` * face b — `yield from` over a throwing sub-generator).`` |
|        - | 1918 | ` */` |
|        - | 1919 | `/* Dispatch-routing macros: bodies live in vm_dispatch.h, written against the` |
|        - | 1920 | ` * VM_EXIT_* primitives. Here they bind to the loop's own control flow. */` |
|        - | 1921 | `#define VM_EXIT_BREAK break` |
|        - | 1922 | `#define VM_EXIT_ABORT goto Abort` |
|        - | 1923 | `#define VM_EXIT_EXCEPTION goto Exception` |
|        - | 1924 | `#include "vm_dispatch.h"` |
|        - | 1925 | `	/* Generator::throw() inject-at-yield: when this invocation is a resumed generator/fiber` |
|        - | 1926 | `	 * body carrying a pending injected exception, raise it HERE — once, before the dispatch` |
|        - | 1927 | `	 * loop (pc is already at the resume point) — so the existing OP_THROW route` |
|        - | 1928 | `	 * (VmThrowException + VmRecordedResume) lands it at the generator's own try/catch landing` |
|        - | 1929 | `	 * pad WITHOUT reconstructing the suspended exception frame on pVm->pFrame. Because` |
|        - | 1930 | `	 * pVm->pFrame stays the body, the return/finally/sRet subsystem is untouched (this is why` |
|        - | 1931 | `	 * the reverted frame-reconstruction approach's regression cannot recur). Behaviorally` |
|        - | 1932 | ``	 * identical to a `throw` executed at the yield point: if the generator's own try catches`` |
|        - | 1933 | `	 * it we resume after the try; otherwise it propagates to the throw() caller (ROOT B lands` |
|        - | 1934 | `	 * the caller's handler) and the ctx closes. pInjected is one-shot and only meaningful at` |
|        - | 1935 | `	 * the resume pc, so this is checked once at entry — NOT per-instruction — keeping the hot` |
|        - | 1936 | `	 * dispatch loop untouched for all normal code. The pFrame gate keeps a nested call / catch` |
|        - | 1937 | `	 * mini-program sharing the ctx (a separate VmByteCodeExec entry) from re-firing.` |
|        - | 1938 | `	 *` |
|        - | 1939 | ``	 * Exception: when this body is suspended mid `yield from` over an inner Generator`` |
|        - | 1940 | `	 * (iDelegateState==3), a Generator::throw() on the OUTER generator must be forwarded` |
|        - | 1941 | `	 * INTO the delegate (PHP yield-from transparency), not raised here. Leave pInjected` |
|        - | 1942 | `	 * set and skip; the resume pc is that OP_YIELD_FROM, which consumes and forwards it. */` |
|  4614496 | 1943 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected` |
|     1383 | 1944 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       63 | 1945 | `	 && pVm->pActiveCtx->iDelegateState != 3 ){` |
|       58 | 1946 | `		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;` |
|        - | 1947 | `		VmFrame *pThrowFrame;` |
|        - | 1948 | `		sxi32 iResumePc;` |
|       58 | 1949 | `		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */` |
|        - | 1950 | `		/* Raising the injection at the yield-from point abandons any array/Iterator` |
|        - | 1951 | `		 * delegation in progress (state 1/2 — state 3 was forwarded, not raised here).` |
|        - | 1952 | `		 * Tear the delegate down so that if the generator's own try catches this and` |
|        - | 1953 | ``		 * runs on to a LATER `yield from`, that opcode classifies its operand fresh`` |
|        - | 1954 | `		 * instead of resuming this now-stale delegate cursor. */` |
|       58 | 1955 | `		if( pVm->pActiveCtx->iDelegateState != 0 ){` |
|        3 | 1956 | `			PH7_MemObjRelease(&pVm->pActiveCtx->sDelegate);` |
|        3 | 1957 | `			pVm->pActiveCtx->pDelegateNode = 0;` |
|        3 | 1958 | `			pVm->pActiveCtx->iDelegateState = 0;` |
|        1 | 1959 | `		}` |
|       58 | 1960 | `		pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       58 | 1961 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|       58 | 1962 | `		rc = VmThrowException(&(*pVm),pInj);` |
|       58 | 1963 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1964 | `			goto Abort;` |
|        - | 1965 | `		}` |
|       58 | 1966 | `		if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 1967 | `			/* ROOT C: the inject was caught by an inline try in THIS generator. Drain the` |
|        - | 1968 | `			 * abandoned mid-expression operands and land at the catch/finally body. This is` |
|        - | 1969 | `			 * the pre-loop path (the first fetch uses pc directly), so no -1 adjustment. */` |
|       92 | 1970 | `			while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|       48 | 1971 | `				PH7_MemObjRelease(pTos);` |
|       48 | 1972 | `				pTos--;` |
|        4 | 1973 | `			}` |
|       48 | 1974 | `			pc = (sxi32)pVm->iInlinePc;` |
|       48 | 1975 | `			pVm->pInlineInstr = 0;` |
|       35 | 1976 | `		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 1977 | `			/* Caught by THIS generator's own try. (VmRecordedResume returns FALSE unless a` |
|        - | 1978 | `			 * catch recorded a resume target for this exec, so rc need not be pre-checked;` |
|        - | 1979 | `			 * unlike OP_THROW there is no lexical-try fallthrough here — the else just` |
|        - | 1980 | `			 * propagates.) Drain the abandoned mid-expression operand slots back to the` |
|        - | 1981 | `			 * catching try's base, then land at its pad (iResumePc is landing-1 for the` |
|        - | 1982 | `			 * dispatcher's trailing pc++; the loop below fetches at pc with no leading pc++,` |
|        - | 1983 | `			 * so add 1 to land on the pad itself). */` |
|      ! 0 | 1984 | `			while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      ! 0 | 1985 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 | 1986 | `				pTos--;` |
|      ! 0 | 1987 | `			}` |
|      ! 0 | 1988 | `			pc = iResumePc + 1;` |
|      ! 0 | 1989 | `		}else{` |
|        - | 1990 | `			/* Not caught in this generator (no match, or caught by an outer/caller frame` |
|        - | 1991 | `			 * that ROOT B will land at its own OP_CALL site): propagate out so the ctx` |
|        - | 1992 | `			 * closes and the caller sees the exception. */` |
|       13 | 1993 | `			goto Exception;` |
|        - | 1994 | `		}` |
|       22 | 1995 | `	}` |
|        - | 1996 | `	/* Force-close entry (VmCloseCtx): a suspended generator being destroyed runs its` |
|        - | 1997 | ``	 * pending `finally` blocks. Instead of resuming at the yield, redirect straight`` |
|        - | 1998 | ``	 * into the innermost open try's finally as if a `return` had crossed every`` |
|        - | 1999 | `	 * enclosing finally (mirrors OP_SET_FINALLY_RET; OP_END_FINALLY then threads the` |
|        - | 2000 | `	 * PH7_FA_RETURN out through the whole chain and completes the body). Gate on the` |
|        - | 2001 | `	 * BODY bytecode (aInstr == the function program) so nested mini-programs sharing` |
|        - | 2002 | `	 * this ctx/frame never re-fire it; bClosing stays set so OP_YIELD can reject a` |
|        - | 2003 | `	 * yield reached inside one of these finallys. */` |
|  4614486 | 2004 | `	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing` |
|     1385 | 2005 | `	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame` |
|       77 | 2006 | `	 && aInstr == (VmInstr *)SySetBasePtr(&pVm->pActiveCtx->pFunc->aByteCode) ){` |
|        - | 2007 | `		VmFinallyAction sAct;` |
|       76 | 2008 | `		sxu32 iFpc = 0;` |
|       76 | 2009 | `		int nCross = -1; /* cross every enclosing finally of this body */` |
|       76 | 2010 | `		SyZero(&sAct,sizeof(sAct));` |
|       76 | 2011 | `		sAct.eKind = PH7_FA_RETURN;` |
|       76 | 2012 | `		sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       76 | 2013 | `		PH7_MemObjInit(pVm,&sAct.sRet); /* discarded return; getReturn() is moot post-close */` |
|       76 | 2014 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|       21 | 2015 | `			sAct.nCross = nCross;` |
|       21 | 2016 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       21 | 2017 | `			pc = (sxi32)iFpc; /* pre-loop: the first fetch uses pc directly (no -1) */` |
|       11 | 2018 | `		}else{` |
|        - | 2019 | `			/* No open try had a finally: nothing to run, complete the body. */` |
|       55 | 2020 | `			PH7_MemObjRelease(&sAct.sRet);` |
|       55 | 2021 | `			goto Done;` |
|        - | 2022 | `		}` |
|       10 | 2023 | `	}` |
|        - | 2024 | `	/* Execute as much as we can */` |
| 34640786 | 2025 | `	for(;;){` |
|      ! 0 | 2026 | `VmLoopFetch:` |
|        - | 2027 | `		/* C-boundary throw routing (band A #1): a PHP callee invoked from a C` |
|        - | 2028 | `		 * site with no status channel — a __toString/__toInt cast, __get/__set/` |
|        - | 2029 | `		 * offsetGet/offsetSet, __clone, __destruct, a user callback inside a` |
|        - | 2030 | `		 * builtin — raised, and the site continued with a fallback value; the` |
|        - | 2031 | `		 * invocation boundary parked the status here (VmBoundaryPark). Route it` |
|        - | 2032 | `		 * exactly as the throw site's dispatch macro would have: abort, land at` |
|        - | 2033 | `		 * an inline-try redirect, resume at a recorded in-place catch (draining` |
|        - | 2034 | `		 * the abandoned mid-expression operands to the catching try's base), or` |
|        - | 2035 | `		 * propagate out of this exec. Checked at the fetch point, so a swallowed` |
|        - | 2036 | `		 * throw outlives at most the C remainder of ONE opcode instead of` |
|        - | 2037 | `		 * silently resuming the surrounding PHP code with a bogus value.` |
|        - | 2038 | `		 * The pending write-back sweep shares this one guard so the hot` |
|        - | 2039 | `		 * no-hooks path pays a single predicted branch per fetch. */` |
| 70399068 | 2040 | `		if( pVm->nBoundaryRc != 0 \|\| SySetUsed(&pVm->aHookRmw) > 0 ){` |
|     1092 | 2041 | `			if( pVm->nBoundaryRc != 0 ){` |
|      618 | 2042 | `				sxi32 rcBr = pVm->nBoundaryRc;` |
|      618 | 2043 | `				pVm->nBoundaryRc = 0;` |
|      618 | 2044 | `				if( rcBr == PH7_ABORT ){` |
|        3 | 2045 | `					goto Abort;` |
|        - | 2046 | `				}` |
|      616 | 2047 | `				if( pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 2048 | `					/* Caught by an inline try (generator body) THIS exec owns: drain` |
|        - | 2049 | `					 * and land (pre-fetch path: pc is used directly, no trailing ++). */` |
|      ! 0 | 2050 | `					while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){` |
|      ! 0 | 2051 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 | 2052 | `						pTos--;` |
|      ! 0 | 2053 | `					}` |
|      ! 0 | 2054 | `					pc = (sxi32)pVm->iInlinePc;` |
|      ! 0 | 2055 | `					pVm->pInlineInstr = 0;` |
|      ! 0 | 2056 | `				}else{` |
|        - | 2057 | `					sxi32 iBrPc;` |
|      616 | 2058 | `					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){` |
|        - | 2059 | `						/* Caught in place by a try THIS exec owns: drain the abandoned` |
|        - | 2060 | `						 * operands to the catching try's base and land at its pad` |
|        - | 2061 | `						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;` |
|        - | 2062 | `						 * pre-fetch here, so +1 lands on the pad itself). */` |
|      613 | 2063 | `						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){` |
|      336 | 2064 | `							PH7_MemObjRelease(pTos);` |
|      336 | 2065 | `							pTos--;` |
|        5 | 2066 | `						}` |
|      282 | 2067 | `						pc = iBrPc + 1;` |
|      143 | 2068 | `					}else{` |
|        - | 2069 | `						/* Caught by an outer exec (or uncaught-with-report pending):` |
|        - | 2070 | `						 * unwind out of this exec; the owner's macros/router land it. */` |
|      339 | 2071 | `						goto Exception;` |
|        - | 2072 | `					}` |
|        - | 2073 | `				}` |
|      138 | 2074 | `			}` |
|        - | 2075 | `			/* Stale write-back sweep: a pending entry whose OWNING activation is` |
|        - | 2076 | `			 * fetching any pc outside its armed window [nJmpPc, nPc] is dead — for` |
|        - | 2077 | `			 * an RMW entry (window = the modify op alone) a routed throw abandoned` |
|        - | 2078 | `			 * the arming statement mid-flight; for a ??= entry either a throw` |
|        - | 2079 | `			 * abandoned the RHS or the short-circuit jump landed past the` |
|        - | 2080 | `			 * OP_NULLC_STORE (the assign is skipped). Drop it — no set dispatch` |
|        - | 2081 | `			 * (php: the throw/skip discards the write). A nested exec (even a` |
|        - | 2082 | `			 * recursive one over the same bytecode) has a different operand-stack` |
|        - | 2083 | `			 * base and leaves enclosing entries alone; entries below a live top` |
|        - | 2084 | `			 * are reached as the drops expose them. */` |
|      768 | 2085 | `			while( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      475 | 2086 | `				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      474 | 2087 | `				if( pTopRmw->pOwnerStack != (void *)pStack \|\| pTopRmw->pInstrs != (void *)aInstr` |
|      171 | 2088 | `				 \|\| ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){` |
|      232 | 2089 | `					break; /* not ours, or legitimately in flight */` |
|        - | 2090 | `				}` |
|       13 | 2091 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 2092 | `			}` |
|      375 | 2093 | `		}` |
|        - | 2094 | `		/* Fetch the instruction to execute */` |
| 70398732 | 2095 | `		pInstr = &aInstr[pc];` |
| 70398732 | 2096 | `		if( pInstr->nLine ){` |
|        - | 2097 | `			/* Publish the source position for diagnostics, debug_backtrace() and` |
|        - | 2098 | `			 * Throwable. Instructions the compiler could not attribute (nLine 0)` |
|        - | 2099 | `			 * leave the last known line standing rather than reporting line 0.` |
|        - | 2100 | `			 * The compilation unit's strict_types mode rides the same gate: a` |
|        - | 2101 | `			 * SYNTHETIC instruction (nLine 0) must not claim to speak for a file. */` |
| 64553389 | 2102 | `			pVm->nCurLine = pInstr->nLine;` |
| 64553389 | 2103 | `			pVm->bCurStrict = pInstr->bStrict;` |
| 32306729 | 2104 | `		}` |
| 70398732 | 2105 | `		rc = SXRET_OK;` |
|        - | 2106 | `/*` |
|        - | 2107 | ` * What follows here is a massive switch statement where each case implements a` |
|        - | 2108 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - | 2109 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - | 2110 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - | 2111 | ` * the switch statement will break with convention and be flush-left.` |
|        - | 2112 | ` */` |
| 70398732 | 2113 | `		switch(pInstr->iOp){` |
|        - | 2114 | `/*` |
|        - | 2115 | ` * DONE: P1 * *` |
|        - | 2116 | ` *` |
|        - | 2117 | ` * Program execution completed: Clean up the mess left behind` |
|        - | 2118 | ` * and return immediately.` |
|        - | 2119 | ` */` |
|  2182607 | 2120 | `case PH7_OP_DONE:` |
|  4365642 | 2121 | `	if( pInstr->iP2 && sState.bReturnPropagates ){` |
|        - | 2122 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - | 2123 | `		 * onto the body frame this catch/finally returns from (skip the transparent` |
|        - | 2124 | `		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION` |
|        - | 2125 | `		 * materializes it into sState.pResult. Drain any finally opened within this body` |
|        - | 2126 | `		 * first (nested try/finally inside the catch), which may overwrite the same` |
|        - | 2127 | `		 * frame's slot (finally-over-catch). */` |
|    23971 | 2128 | `		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|    23971 | 2129 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    23965 | 2130 | `			PH7_MemObjStore(pTos,&pTgt->sRet);` |
|    23965 | 2131 | `			VmPopOperand(&pTos,1);` |
|    11985 | 2132 | `		}else{` |
|        8 | 2133 | ``			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */`` |
|        - | 2134 | `		}` |
|    23971 | 2135 | `		pTgt->bHasRet = 1;` |
|    23971 | 2136 | `		pTgt->nRetGen++;` |
|    23971 | 2137 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    23971 | 2138 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2139 | `			goto Abort;` |
|        - | 2140 | `		}` |
|    23971 | 2141 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 2142 | `			/* A drained finally threw past itself — it discards this return. */` |
|      ! 0 | 2143 | `			goto Exception;` |
|        - | 2144 | `		}` |
|    23971 | 2145 | `		goto Done;` |
|        - | 2146 | `	}` |
|        - | 2147 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - | 2148 | `	 * the fiber start/resume paths) set sState.pEnforceRetFunc, so this branch is` |
|        - | 2149 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - | 2150 | `	 * callback trampolines, and the main script. */` |
|  4341671 | 2151 | `	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)` |
|    13827 | 2152 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - | 2153 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - | 2154 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - | 2155 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - | 2156 | `		 * value the function never actually returned, so enforcing here would` |
|        - | 2157 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - | 2158 | `		 * exception. */` |
|    13827 | 2159 | `		ph7_value *pRetVal = 0;` |
|    13827 | 2160 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|    11395 | 2161 | `			pRetVal = pTos;` |
|     5692 | 2162 | `		}` |
|    13827 | 2163 | `		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);` |
|    13827 | 2164 | `		if( rc == PH7_ABORT ) goto Abort;` |
|    13823 | 2165 | `		if( rc == PH7_EXCEPTION ){` |
|      147 | 2166 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      113 | 2167 | `				PH7_MemObjRelease(pTos);` |
|      113 | 2168 | `				pTos--;` |
|       54 | 2169 | `			}` |
|      147 | 2170 | `			goto Exception;` |
|        - | 2171 | `		}` |
|        - | 2172 | `		/* Don't enforce twice if the function loops through multiple` |
|        - | 2173 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - | 2174 | `		 * defensively we clear the pointer after a successful check). */` |
|    13681 | 2175 | `		sState.pEnforceRetFunc = 0;` |
|     6835 | 2176 | `	}` |
|  4341530 | 2177 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|  2866965 | 2178 | `		if( sState.pLastRef ){` |
|   145079 | 2179 | `			*sState.pLastRef = pTos->nIdx;` |
|    72752 | 2180 | `		}` |
|  2866965 | 2181 | `		if( sState.pResult ){` |
|        - | 2182 | `			/* Execution result */` |
|  1383216 | 2183 | `			PH7_MemObjStore(pTos,sState.pResult);` |
|   691817 | 2184 | `		}` |
|  2866965 | 2185 | `		VmPopOperand(&pTos,1);` |
|  1433697 | 2186 | `	}else{` |
|  1474570 | 2187 | `		if( pInstr->iP1 == 0 && pInstr->iP2 && sState.pResult ){` |
|        - | 2188 | ``			/* An EXPLICIT `return;` with no value answers NULL. It reads as a`` |
|        - | 2189 | `			 * no-op for a function (whose result slot starts out null anyway)` |
|        - | 2190 | `			 * and matters for an included CHUNK, whose slot is seeded with the 1` |
|        - | 2191 | ``			 * a file that returns nothing answers: `<?php return;` is php's`` |
|        - | 2192 | `			 * NULL, not that 1. */` |
|       49 | 2193 | `			PH7_MemObjRelease(sState.pResult);` |
|       23 | 2194 | `		}` |
|        - | 2195 | `		/* Nothing referenced — also the throw-unwind path: the compiler routes` |
|        - | 2196 | `		 * an uncaught exception to this terminal OP_DONE with iP1 set but an` |
|        - | 2197 | `		 * empty operand stack (pTos == pStack-1), so there is no return value to` |
|        - | 2198 | `		 * store. Guarding on pTos >= pStack (matching the sibling branch above)` |
|        - | 2199 | `		 * avoids the below-base read that crashed under glibc/ASan. */` |
|  1474570 | 2200 | `		if( sState.pLastRef ){` |
|    14323 | 2201 | `			*sState.pLastRef = SXU32_HIGH;` |
|     7159 | 2202 | `		}` |
|        - | 2203 | `	}` |
|        - | 2204 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - | 2205 | `	 * this execution. When 'return' is used inside a try block,` |
|        - | 2206 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - | 2207 | `	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering` |
|        - | 2208 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - | 2209 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - | 2210 | `	 * block can override it (the finally writes this body frame's sRet slot,` |
|        - | 2211 | `	 * materialized below).` |
|        - | 2212 | `	 */` |
|  4341530 | 2213 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|  4341530 | 2214 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2215 | `		goto Abort;` |
|        - | 2216 | `	}` |
|  4341530 | 2217 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 2218 | `		/* A drained finally threw past itself, discarding the value this OP_DONE` |
|        - | 2219 | `		 * stored into sState.pResult. If an enclosing try IN THIS function caught the new` |
|        - | 2220 | `		 * exception in place, resume at its landing pad; otherwise unwind (the` |
|        - | 2221 | `		 * caller's exception-resume pops the stored result). */` |
|        - | 2222 | `		sxi32 iResumePc;` |
|        5 | 2223 | `		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 2224 | `			pc = iResumePc;` |
|        3 | 2225 | `			break;` |
|        - | 2226 | `		}` |
|        3 | 2227 | `		goto Exception;` |
|        - | 2228 | `	}` |
|  4341526 | 2229 | `	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){` |
|        - | 2230 | `		/* A catch/finally issued a 'return' targeting THIS body. If the body is` |
|        - | 2231 | `		 * actually unwinding because an exception escaped it (terminal OP_DONE on` |
|        - | 2232 | `		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type` |
|        - | 2233 | `		 * enforcement above), that exception supersedes the return: discard it.` |
|        - | 2234 | `		 * Otherwise materialize it as this function's result. */` |
|       15 | 2235 | `		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){` |
|      ! 0 | 2236 | `			VmClearFramePending(sState.pEntryFrame);` |
|      ! 0 | 2237 | `		}else{` |
|       15 | 2238 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|        - | 2239 | `		}` |
|        5 | 2240 | `	}` |
|  4341526 | 2241 | `	goto Done;` |
|        - | 2242 | `/*` |
|        - | 2243 | ` * HALT: P1 * *` |
|        - | 2244 | ` *` |
|        - | 2245 | ` * Program execution aborted: Clean up the mess left behind` |
|        - | 2246 | ` * and abort immediately.` |
|        - | 2247 | ` */` |
|       41 | 2248 | `case PH7_OP_HALT:` |
|       86 | 2249 | `	if( pInstr->iP1 ){` |
|        - | 2250 | `#ifdef UNTRUST` |
|        - | 2251 | `		if( pTos < pStack ){` |
|        - | 2252 | `			goto Abort;` |
|        - | 2253 | `		}` |
|        - | 2254 | `#endif` |
|       86 | 2255 | `		if( sState.pLastRef ){` |
|       63 | 2256 | `			*sState.pLastRef = pTos->nIdx;` |
|       30 | 2257 | `		}` |
|       86 | 2258 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       72 | 2259 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - | 2260 | `				/* Output the exit message */` |
|      106 | 2261 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|       34 | 2262 | `					pVm->sVmConsumer.pUserData);` |
|       72 | 2263 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|       38 | 2264 | `			}` |
|       51 | 2265 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - | 2266 | `			/* Record exit status */` |
|       17 | 2267 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        7 | 2268 | `		}` |
|       86 | 2269 | `		VmPopOperand(&pTos,1);` |
|       41 | 2270 | `	}else if( sState.pLastRef ){` |
|        - | 2271 | `		/* Nothing referenced */` |
|      ! 0 | 2272 | `		*sState.pLastRef = SXU32_HIGH;` |
|      ! 0 | 2273 | `	}` |
|        - | 2274 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - | 2275 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - | 2276 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - | 2277 | `	 */` |
|       86 | 2278 | `	pVm->bHaltRequested = 1;` |
|       86 | 2279 | `	goto Abort;` |
|        - | 2280 | `/*` |
|        - | 2281 | ` * JMP: * P2 *` |
|        - | 2282 | ` *` |
|        - | 2283 | ` * Unconditional jump: The next instruction executed will be` |
|        - | 2284 | ` * the one at index P2 from the beginning of the program.` |
|        - | 2285 | ` */` |
|   762462 | 2286 | `case PH7_OP_JMP:` |
|  1526399 | 2287 | `	pc = pInstr->iP2 - 1;` |
|  1526399 | 2288 | `	break;` |
|        - | 2289 | `/*` |
|        - | 2290 | ` * JZ: P1 P2 *` |
|        - | 2291 | ` *` |
|        - | 2292 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - | 2293 | ` * entry in the stack if P1 is zero.` |
|        - | 2294 | ` */` |
|  1896800 | 2295 | `case PH7_OP_JZ:` |
|        - | 2296 | `#ifdef UNTRUST` |
|        - | 2297 | `	if( pTos < pStack ){` |
|        - | 2298 | `		goto Abort;` |
|        - | 2299 | `	}` |
|        - | 2300 | `#endif` |
|        - | 2301 | `	/* Get a boolean value */` |
|  3799528 | 2302 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     2719 | 2303 | `		PH7_MemObjToBool(pTos);` |
|     1357 | 2304 | `	}` |
|  3799528 | 2305 | `	if( !pTos->x.iVal ){` |
|        - | 2306 | `		/* Take the jump */` |
|  2070076 | 2307 | `		pc = pInstr->iP2 - 1;` |
|  1036814 | 2308 | `	}` |
|  3799528 | 2309 | `	if( !pInstr->iP1 ){` |
|  3351433 | 2310 | `		VmPopOperand(&pTos,1);` |
|  1678342 | 2311 | `	}` |
|  3799528 | 2312 | `	break;` |
|        - | 2313 | `/*` |
|        - | 2314 | ` * JNZ: P1 P2 *` |
|        - | 2315 | ` *` |
|        - | 2316 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - | 2317 | ` * entry in the stack if P1 is zero.` |
|        - | 2318 | ` */` |
|   148878 | 2319 | `case PH7_OP_JNZ:` |
|        - | 2320 | `#ifdef UNTRUST` |
|        - | 2321 | `	if( pTos < pStack ){` |
|        - | 2322 | `		goto Abort;` |
|        - | 2323 | `	}` |
|        - | 2324 | `#endif` |
|        - | 2325 | `	/* Get a boolean value */` |
|   298195 | 2326 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 | 2327 | `		PH7_MemObjToBool(pTos);` |
|        1 | 2328 | `	}` |
|   298195 | 2329 | `	if( pTos->x.iVal ){` |
|        - | 2330 | `		/* Take the jump */` |
|    10348 | 2331 | `		pc = pInstr->iP2 - 1;` |
|     5173 | 2332 | `	}` |
|   298195 | 2333 | `	if( !pInstr->iP1 ){` |
|       37 | 2334 | `		VmPopOperand(&pTos,1);` |
|       19 | 2335 | `	}` |
|   298195 | 2336 | `	break;` |
|        - | 2337 | `/*` |
|        - | 2338 | ` * NOOP: * * *` |
|        - | 2339 | ` *` |
|        - | 2340 | ` * Do nothing. This instruction is often useful as a jump` |
|        - | 2341 | ` * destination.` |
|        - | 2342 | ` */` |
|        3 | 2343 | `case PH7_OP_NOOP:` |
|        6 | 2344 | `	break;` |
|        - | 2345 | `/*` |
|        - | 2346 | ` * POP: P1 * *` |
|        - | 2347 | ` *` |
|        - | 2348 | ` * Pop P1 elements from the operand stack.` |
|        - | 2349 | ` */` |
|  1635022 | 2350 | `case PH7_OP_POP: {` |
|  3274638 | 2351 | `	sxi32 n = pInstr->iP1;` |
|  3274638 | 2352 | `	if( &pTos[-n+1] < pStack ){` |
|        - | 2353 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      436 | 2354 | `		n = (sxi32)(pTos - pStack);` |
|      216 | 2355 | `	}` |
|  3274638 | 2356 | `	VmPopOperand(&pTos,n);` |
|  3274638 | 2357 | `	break;` |
|        - | 2358 | `				 }` |
|        - | 2359 | `/*` |
|        - | 2360 | ` * DUP: * * *` |
|        - | 2361 | ` *` |
|        - | 2362 | ` * Duplicate the top of the stack.` |
|        - | 2363 | ` */` |
|      140 | 2364 | `case PH7_OP_DUP:` |
|        - | 2365 | `#ifdef UNTRUST` |
|        - | 2366 | `	if( pTos < pStack ){` |
|        - | 2367 | `		goto Abort;` |
|        - | 2368 | `	}` |
|        - | 2369 | `#endif` |
|      284 | 2370 | `	pTos++;` |
|      284 | 2371 | `	PH7_MemObjInit(pVm,pTos);` |
|      284 | 2372 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|      284 | 2373 | `	break;` |
|        - | 2374 | `/*` |
|        - | 2375 | ` * CLASS_DEFER: * * P3` |
|        - | 2376 | ` *` |
|        - | 2377 | ` * Execute a class declaration whose parent/interface/trait could not be` |
|        - | 2378 | ` * resolved when the enclosing file compiled (its autoloader had not RUN yet).` |
|        - | 2379 | ` * P3 is the VmDeferredClass record captured by the compiler. A dependency` |
|        - | 2380 | `` * that is STILL missing throws php's catchable `... not found` Error; a`` |
|        - | 2381 | ` * failed re-compile aborts (its errors were already reported). See the` |
|        - | 2382 | ` * deferral block comment in compile_class.c.` |
|        - | 2383 | ` */` |
|       15 | 2384 | `case PH7_OP_CLASS_DEFER: {` |
|       33 | 2385 | `	VmDeferredClass *pDefer = (VmDeferredClass *)pInstr->p3;` |
|       33 | 2386 | `	VmDeferredReq *pMissing = 0;` |
|       33 | 2387 | `	sxi32 rcDecl = SXRET_OK;` |
|       33 | 2388 | `	if( pDefer ){` |
|       33 | 2389 | `		rcDecl = VmExecDeferredClass(&(*pVm),pDefer,&pMissing);` |
|       15 | 2390 | `	}` |
|       33 | 2391 | `	if( pMissing ){` |
|        - | 2392 | `		static const char *azDeferKind[] = { "Class", "Interface", "Trait" };` |
|        - | 2393 | `		char zDeclMsg[520];` |
|       16 | 2394 | `		sxu32 nDeclMsg = SyBufferFormat(zDeclMsg,sizeof(zDeclMsg),"%s \"%.*s\" not found",` |
|        5 | 2395 | `			azDeferKind[pMissing->cKind < 3 ? pMissing->cKind : 0],` |
|       10 | 2396 | `			(int)pMissing->sName.nByte,pMissing->sName.zString);` |
|       11 | 2397 | `		rc = VmThrowFromVm(&(*pVm),"Error",zDeclMsg,nDeclMsg);` |
|       11 | 2398 | `		if( rc == SXERR_ABORT ){` |
|        3 | 2399 | `			goto Abort;` |
|        - | 2400 | `		}` |
|        9 | 2401 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2402 | `	}` |
|       23 | 2403 | `	if( rcDecl == SXERR_ABORT ){` |
|      ! 0 | 2404 | `		goto Abort;` |
|        - | 2405 | `	}` |
|       23 | 2406 | `	break;` |
|        - | 2407 | `				}` |
|        - | 2408 | `/*` |
|        - | 2409 | ` * CVT_INT: * * *` |
|        - | 2410 | ` *` |
|        - | 2411 | ` * Force the top of the stack to be an integer.` |
|        - | 2412 | ` */` |
|      523 | 2413 | `case PH7_OP_CVT_INT:` |
|        - | 2414 | `#ifdef UNTRUST` |
|        - | 2415 | `	if( pTos < pStack ){` |
|        - | 2416 | `		goto Abort;` |
|        - | 2417 | `	}` |
|        - | 2418 | `#endif` |
|     1051 | 2419 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|        - | 2420 | `		/* php warns from the conversion itself when no int can hold the float` |
|        - | 2421 | ``		 * (`(int)1e19`); the value it then answers is the modular wrap. */`` |
|      881 | 2422 | `		PH7_MemObjWarnIntCast(pTos);` |
|      881 | 2423 | `		PH7_MemObjToInteger(pTos);` |
|      438 | 2424 | `	}` |
|        - | 2425 | `	/* Invalidate any prior representation */` |
|     1051 | 2426 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|     1051 | 2427 | `	break;` |
|        - | 2428 | `/*` |
|        - | 2429 | ` * CVT_REAL: * * *` |
|        - | 2430 | ` *` |
|        - | 2431 | ` * Force the top of the stack to be a real.` |
|        - | 2432 | ` */` |
|      228 | 2433 | `case PH7_OP_CVT_REAL:` |
|        - | 2434 | `#ifdef UNTRUST` |
|        - | 2435 | `	if( pTos < pStack ){` |
|        - | 2436 | `		goto Abort;` |
|        - | 2437 | `	}` |
|        - | 2438 | `#endif` |
|      459 | 2439 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       76 | 2440 | `		PH7_MemObjToReal(pTos);` |
|       37 | 2441 | `	}` |
|        - | 2442 | `	/* Invalidate any prior representation */` |
|      459 | 2443 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|      459 | 2444 | `	break;` |
|        - | 2445 | `/*` |
|        - | 2446 | ` * CVT_STR: * * *` |
|        - | 2447 | ` *` |
|        - | 2448 | ` * Force the top of the stack to be a string.` |
|        - | 2449 | ` */` |
|     2852 | 2450 | `case PH7_OP_CVT_STR:` |
|        - | 2451 | `#ifdef UNTRUST` |
|        - | 2452 | `	if( pTos < pStack ){` |
|        - | 2453 | `		goto Abort;` |
|        - | 2454 | `	}` |
|        - | 2455 | `#endif` |
|        - | 2456 | `	/* (string) cast + string interpolation "$arr": php's user-visible` |
|        - | 2457 | `	 * array->string warning site, and the not-stringable-object throw (§2). */` |
|        - | 2458 | `	{` |
|     5709 | 2459 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     5713 | 2460 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2461 | `	}` |
|     5623 | 2462 | `	break;` |
|        - | 2463 | `/*` |
|        - | 2464 | ` * CVT_BOOL: * * *` |
|        - | 2465 | ` *` |
|        - | 2466 | ` * Force the top of the stack to be a boolean.` |
|        - | 2467 | ` */` |
|       44 | 2468 | `case PH7_OP_CVT_BOOL:` |
|        - | 2469 | `#ifdef UNTRUST` |
|        - | 2470 | `	if( pTos < pStack ){` |
|        - | 2471 | `		goto Abort;` |
|        - | 2472 | `	}` |
|        - | 2473 | `#endif` |
|       93 | 2474 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       91 | 2475 | `		PH7_MemObjToBool(pTos);` |
|       43 | 2476 | `	}` |
|       93 | 2477 | `	break;` |
|        - | 2478 | `/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it` |
|        - | 2479 | ` * always raises "The (unset) cast is no longer supported" (php 8 removed` |
|        - | 2480 | ` * the cast), so a program containing it never compiles. The switch has no` |
|        - | 2481 | ` * default arm, so abort loudly rather than fall through as a silent no-op` |
|        - | 2482 | ` * if a future emitter ever produces one without the compile error. */` |
|      ! 0 | 2483 | `case PH7_OP_CVT_NULL:` |
|      ! 0 | 2484 | `	goto Abort;` |
|        - | 2485 | `/*` |
|        - | 2486 | ` * CVT_NUMC: * * *` |
|        - | 2487 | ` *` |
|        - | 2488 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - | 2489 | ` */` |
|      ! 0 | 2490 | `case PH7_OP_CVT_NUMC:` |
|        - | 2491 | `#ifdef UNTRUST` |
|        - | 2492 | `	if( pTos < pStack ){` |
|        - | 2493 | `		goto Abort;` |
|        - | 2494 | `	}` |
|        - | 2495 | `#endif` |
|        - | 2496 | `	/* Force a numeric cast */` |
|      ! 0 | 2497 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 | 2498 | `	break;` |
|        - | 2499 | `/*` |
|        - | 2500 | ` * CVT_ARRAY: * * *` |
|        - | 2501 | ` *` |
|        - | 2502 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - | 2503 | ` */` |
|      290 | 2504 | `case PH7_OP_CVT_ARRAY:` |
|        - | 2505 | `#ifdef UNTRUST` |
|        - | 2506 | `	if( pTos < pStack ){` |
|        - | 2507 | `		goto Abort;` |
|        - | 2508 | `	}` |
|        - | 2509 | `#endif` |
|        - | 2510 | `	/* Force a hashmap cast */` |
|      585 | 2511 | `	rc = PH7_MemObjToHashmap(pTos);` |
|      585 | 2512 | `	if( rc != SXRET_OK ){` |
|        - | 2513 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 | 2514 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - | 2515 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 | 2516 | `	}` |
|      585 | 2517 | `	break;` |
|        - | 2518 | `/*` |
|        - | 2519 | ` * CVT_OBJ: * * *` |
|        - | 2520 | ` *` |
|        - | 2521 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - | 2522 | ` */` |
|       27 | 2523 | `case PH7_OP_CVT_OBJ:` |
|        - | 2524 | `#ifdef UNTRUST` |
|        - | 2525 | `	if( pTos < pStack ){` |
|        - | 2526 | `		goto Abort;` |
|        - | 2527 | `	}` |
|        - | 2528 | `#endif` |
|       57 | 2529 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 2530 | `		/* Force a 'stdClass()' cast */` |
|       57 | 2531 | `		PH7_MemObjToObject(pTos);` |
|       27 | 2532 | `	}` |
|       57 | 2533 | `	break;` |
|        - | 2534 | `/*` |
|        - | 2535 | ` * ERR_CTRL * * *` |
|        - | 2536 | ` *` |
|        - | 2537 | ` * Error control operator.` |
|        - | 2538 | ` */` |
|     4081 | 2539 | `case PH7_OP_UNSET_VAR: {` |
|        - | 2540 | `	VmOpRc rcOp;` |
|     8167 | 2541 | `	sState.pTos = pTos;` |
|     8167 | 2542 | `	sState.pc = pc;` |
|     8167 | 2543 | `	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);` |
|     8167 | 2544 | `	pTos = sState.pTos;` |
|     8167 | 2545 | `	pc = sState.pc;` |
|     8167 | 2546 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 2547 | `		goto Abort;` |
|     8165 | 2548 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2549 | `		goto Exception;` |
|        - | 2550 | `	}` |
|     8165 | 2551 | `	break;` |
|        - | 2552 | `					  }` |
|    46855 | 2553 | `case PH7_OP_ERR_CTRL:` |
|        - | 2554 | `	/*` |
|        - | 2555 | `	 * Error-control operator '@'. Emitted as a PAIR around the suppressed` |
|        - | 2556 | `	 * expression: iP1=1 opens the window (before the operand is evaluated),` |
|        - | 2557 | `	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),` |
|        - | 2558 | `	 * which went unnoticed only because the engine raised so few diagnostics;` |
|        - | 2559 | `	 * every warning/notice/deprecation the parity work added leaked straight` |
|        - | 2560 | ``	 * through `@`. The window nests, and php 8 does NOT let '@' swallow`` |
|        - | 2561 | `	 * fatals or exceptions — those unwind past the closing instruction, so` |
|        - | 2562 | `	 * the depth is reset on the exception path rather than decremented here.` |
|        - | 2563 | `	 */` |
|    93715 | 2564 | `	if( pInstr->iP1 ){` |
|    46949 | 2565 | `		pVm->nErrSuppress++;` |
|    70243 | 2566 | `	}else if( pVm->nErrSuppress > 0 ){` |
|    46771 | 2567 | `		pVm->nErrSuppress--;` |
|    23383 | 2568 | `	}` |
|    93715 | 2569 | `	break;` |
|        - | 2570 | `/*` |
|        - | 2571 | ` * IS_A * * *` |
|        - | 2572 | ` *` |
|        - | 2573 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - | 2574 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - | 2575 | ` * holding a class name or an object).` |
|        - | 2576 | ` * Push TRUE on success. FALSE otherwise.` |
|        - | 2577 | ` */` |
|      445 | 2578 | `case PH7_OP_IS_A:{` |
|      895 | 2579 | `	ph7_value *pNos = &pTos[-1];` |
|      895 | 2580 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - | 2581 | `#ifdef UNTRUST` |
|        - | 2582 | `	if( pNos < pStack ){` |
|        - | 2583 | `		goto Abort;` |
|        - | 2584 | `	}` |
|        - | 2585 | `#endif` |
|      895 | 2586 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      757 | 2587 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      757 | 2588 | `		ph7_class *pClass = 0;` |
|        - | 2589 | `		/* Extract the target class */` |
|      757 | 2590 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - | 2591 | `			/* Instance already loaded */` |
|      ! 0 | 2592 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      757 | 2593 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      757 | 2594 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      757 | 2595 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - | 2596 | `			/* Handle self/static/parent keywords */` |
|      757 | 2597 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        6 | 2598 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      755 | 2599 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 | 2600 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      752 | 2601 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        6 | 2602 | `				pClass = PH7_VmResolveParentClass(&(*pVm));` |
|        4 | 2603 | `			}else{` |
|      747 | 2604 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - | 2605 | `			}` |
|      376 | 2606 | `		}` |
|      757 | 2607 | `		if( pClass ){` |
|        - | 2608 | `			/* Perform the query */` |
|      755 | 2609 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|      375 | 2610 | `		}` |
|      376 | 2611 | `	}` |
|        - | 2612 | `	/* Push result */` |
|      895 | 2613 | `	VmPopOperand(&pTos,1);` |
|      895 | 2614 | `	PH7_MemObjRelease(pTos);` |
|      895 | 2615 | `	pTos->x.iVal = iRes;` |
|      895 | 2616 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      895 | 2617 | `	break;` |
|        - | 2618 | `				 }` |
|        - | 2619 |  |
|        - | 2620 | `/*` |
|        - | 2621 | ` * LOADC P1 P2 *` |
|        - | 2622 | ` *` |
|        - | 2623 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - | 2624 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - | 2625 | ` */` |
|  6871749 | 2626 | `case PH7_OP_LOADC: {` |
|        - | 2627 | `	ph7_value *pObj;` |
|        - | 2628 | `	/* Reserve a room */` |
| 13751120 | 2629 | `	pTos++;` |
| 13751120 | 2630 | `	if( pInstr->iP1 & PH7_LOADC_NOKEY ){` |
|        - | 2631 | `		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */` |
|   756211 | 2632 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|   756211 | 2633 | `		SyBlobReset(&pTos->sBlob);` |
|   756211 | 2634 | `		pTos->iFlags \|= MEMOBJ_AUX_NOKEY;` |
|   756211 | 2635 | `		pTos->nIdx = SXU32_HIGH;` |
|   756211 | 2636 | `		break;` |
|        - | 2637 | `	}` |
| 12994914 | 2638 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
| 12994914 | 2639 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - | 2640 | `			SyHashEntry *pEntry;` |
|        - | 2641 | `			/* The name php looks up FIRST for an unqualified constant is decided at` |
|        - | 2642 | ``			 * COMPILE time and travels in p3: a `use const` import's FQN, or`` |
|        - | 2643 | ``			 * `current-namespace\NAME`. Absent (global scope, or an already-qualified`` |
|        - | 2644 | `			 * literal), the bare literal is the only name there is. Resolving it here` |
|        - | 2645 | `			 * rather than against the RUNTIME namespace is what makes a function keep` |
|        - | 2646 | `			 * its own namespace when it is called from another one, and what lets a` |
|        - | 2647 | `			 * namespaced constant shadow a global one of the same short name. */` |
|   143188 | 2648 | `			const char *zCand = (const char *)pInstr->p3;` |
|   143188 | 2649 | `			const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|   143188 | 2650 | `			sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|   143188 | 2651 | `			if( zCand ){` |
|       55 | 2652 | `				pEntry = SyHashGet(&pVm->hConstant,zCand,SyStrlen(zCand));` |
|       55 | 2653 | `				if( pEntry ){` |
|       47 | 2654 | `					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       47 | 2655 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       47 | 2656 | `					SyBlobReset(&pTos->sBlob);` |
|       47 | 2657 | `					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|       47 | 2658 | `					pTos->nIdx = SXU32_HIGH;` |
|       47 | 2659 | `					break;` |
|        - | 2660 | `				}` |
|        4 | 2661 | `			}` |
|        - | 2662 | `			/* The GLOBAL step — skipped when the candidate came from an import, which` |
|        - | 2663 | `			 * php resolves without any fallback. */` |
|   143146 | 2664 | `			if( (pInstr->iP1 & PH7_LOADC_NOGLOBAL) == 0 ){` |
|   143144 | 2665 | `				pEntry = SyHashGet(&pVm->hConstant,(const void *)zLit,nLit);` |
|   143144 | 2666 | `				if( pEntry ){` |
|   142972 | 2667 | `					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - | 2668 | `					/* Set a NULL default value */` |
|   142972 | 2669 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|   142972 | 2670 | `					SyBlobReset(&pTos->sBlob);` |
|        - | 2671 | `					/* Invoke the callback and deal with the expanded value */` |
|   142972 | 2672 | `					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);` |
|        - | 2673 | `					/* Mark as constant */` |
|   142972 | 2674 | `					pTos->nIdx = SXU32_HIGH;` |
|   142972 | 2675 | `					break;` |
|        - | 2676 | `				}` |
|       86 | 2677 | `			}` |
|        - | 2678 | `			{` |
|        - | 2679 | `				/*` |
|        - | 2680 | `				 * php 8 has no bare-word fallback: an unresolved constant is a catchable` |
|        - | 2681 | `				 * Error, not its own name as a string. PH7 answered "X" for an unknown` |
|        - | 2682 | `				 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —` |
|        - | 2683 | `				 * silently became a string and flowed on. php names the name it looked` |
|        - | 2684 | `				 * for FIRST, so the message reports the candidate when there was one` |
|        - | 2685 | ``				 * ("Undefined constant \"B\NOPE\"" inside `namespace B;`).`` |
|        - | 2686 | `				 *` |
|        - | 2687 | `				 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call` |
|        - | 2688 | ``				 * boundary, so neither a bare `break` nor `goto Exception` is correct`` |
|        - | 2689 | `				 * here (see the macro).` |
|        - | 2690 | `				 */` |
|        - | 2691 | `				SyBlob sMsg;` |
|      179 | 2692 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      179 | 2693 | `				if( zCand ){` |
|        8 | 2694 | `					SyBlobFormat(&sMsg,"Undefined constant \"%s\"",zCand);` |
|        5 | 2695 | `				}else{` |
|      173 | 2696 | `					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);` |
|        - | 2697 | `				}` |
|      179 | 2698 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      179 | 2699 | `				SyBlobReset(&pTos->sBlob);` |
|      179 | 2700 | `				pTos->nIdx = SXU32_HIGH;` |
|      266 | 2701 | `				rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|       87 | 2702 | `					SyBlobLength(&sMsg));` |
|      179 | 2703 | `				SyBlobRelease(&sMsg);` |
|      179 | 2704 | `				if( rc == SXERR_ABORT ){` |
|       49 | 2705 | `					goto Abort;` |
|        - | 2706 | `				}` |
|      157 | 2707 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2708 | `			}` |
|        - | 2709 | `		}` |
| 12851731 | 2710 | `		PH7_MemObjLoad(pObj,pTos);` |
|  6429675 | 2711 | `	}else{` |
|        - | 2712 | `		/* Set a NULL value */` |
|      ! 0 | 2713 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 2714 | `	}` |
|        - | 2715 | `	/* Mark as constant */` |
| 12851731 | 2716 | `	pTos->nIdx = SXU32_HIGH;` |
| 12851731 | 2717 | `	break;` |
|        - | 2718 | `				  }` |
|        - | 2719 | `/*` |
|        - | 2720 | ` * LOAD: P1 * P3` |
|        - | 2721 | ` *` |
|        - | 2722 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - | 2723 | ` * from the P3 operand.` |
|        - | 2724 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - | 2725 | ` * the variable if non existent and push the NULL constant instead.` |
|        - | 2726 | ` */` |
|  6202202 | 2727 | `case PH7_OP_LOAD:{` |
|        - | 2728 | `	ph7_value *pObj;` |
|        - | 2729 | `	SyString sName;` |
| 12420536 | 2730 | `	if( pInstr->p3 == 0 ){` |
|        - | 2731 | `		/* Take the variable name from the top of the stack */` |
|        - | 2732 | `#ifdef UNTRUST` |
|        - | 2733 | `		if( pTos < pStack ){` |
|        - | 2734 | `			goto Abort;` |
|        - | 2735 | `		}` |
|        - | 2736 | `#endif` |
|        - | 2737 | `		/* Force a string cast — variable-variable name $$arr (user-visible, §2) */` |
|        - | 2738 | `		{` |
|       40 | 2739 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|  6202241 | 2740 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 2741 | `		}` |
|       38 | 2742 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       21 | 2743 | `	}else{` |
| 12420500 | 2744 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 2745 | `		/* Reserve a room for the target object */` |
| 12420500 | 2746 | `		pTos++;` |
|        - | 2747 | `	}` |
| 12420534 | 2748 | `	if( pInstr->iP2 == 2 ){` |
|        - | 2749 | ``		/* Read-modify-write target (`$x++`, `$x .= 'a'`): php reads the variable`` |
|        - | 2750 | `		 * before writing, so it warns when it does not exist and THEN seeds it.` |
|        - | 2751 | `		 * Peek first (no create) purely to raise that warning; the load below` |
|        - | 2752 | ``		 * still creates the slot the operator needs. A plain `=` never gets here`` |
|        - | 2753 | `		 * — it writes without reading, and stays silent, as php does. */` |
|  1205938 | 2754 | `		if( VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE) == 0 ){` |
|       10 | 2755 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|        4 | 2756 | `		}` |
|   603704 | 2757 | `	}` |
|        - | 2758 | `	/* Extract the requested memory object */` |
| 12420534 | 2759 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
| 12420534 | 2760 | `	if( pObj == 0 ){` |
|      229 | 2761 | `		if( pInstr->iP1 ){` |
|        - | 2762 | `			/* Reading a variable that does not exist. php raises E_WARNING` |
|        - | 2763 | `			 * "Undefined variable $x" and evaluates it as NULL; PHL used to` |
|        - | 2764 | `			 * yield NULL silently, which hid typo'd names. iP2 marks the reads` |
|        - | 2765 | `			 * that must stay quiet (isset/empty — see PH7_CompileVariable);` |
|        - | 2766 | `			 * vivifying contexts never get here because they pass iP1 = 0. */` |
|      229 | 2767 | `			if( pInstr->iP2 == 0 ){` |
|       45 | 2768 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);` |
|       20 | 2769 | `			}` |
|        - | 2770 | `			/* Variable not found,load NULL */` |
|      229 | 2771 | `			if( !pInstr->p3 ){` |
|       10 | 2772 | `				PH7_MemObjRelease(pTos);` |
|        6 | 2773 | `			}else{` |
|      221 | 2774 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - | 2775 | `			}` |
|      229 | 2776 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|      229 | 2777 | `			if( pInstr->iP2 == 3 ){` |
|        - | 2778 | `				/* D1 deferred call argument: the variable does not exist, but we do not` |
|        - | 2779 | `				 * know yet whether the callee wants it by-ref (materialize + bind) or` |
|        - | 2780 | `				 * by-value (warn + pass NULL). Tag the slot and stash the variable name` |
|        - | 2781 | `				 * (a VM-lifetime bytecode string, nothing to free) so OP_CALL's` |
|        - | 2782 | `				 * PH7_VmResolveDeferredArgs can decide once the callee is resolved. */` |
|      106 | 2783 | `				pTos->iFlags \|= MEMOBJ_AUX_DEFERRED;` |
|      106 | 2784 | `				pTos->x.pOther = pInstr->p3;` |
|       52 | 2785 | `			}` |
|      229 | 2786 | `			break;` |
|      ! 0 | 2787 | `		}else{` |
|        - | 2788 | `			/* Fatal error */` |
|      ! 0 | 2789 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 2790 | `			goto Abort;` |
|        - | 2791 | `		}` |
|        - | 2792 | `	}` |
|        - | 2793 | `	/* Load variable contents */` |
| 12420310 | 2794 | `	PH7_MemObjLoad(pObj,pTos);` |
| 12420310 | 2795 | `	pTos->nIdx = pObj->nIdx;` |
| 12420310 | 2796 | `	break;` |
|        - | 2797 | `				   }` |
|        - | 2798 | `/*` |
|        - | 2799 | ` * LOAD_MAP P1 * *` |
|        - | 2800 | ` *` |
|        - | 2801 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - | 2802 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - | 2803 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - | 2804 | ` */` |
|    57788 | 2805 | `case PH7_OP_LOAD_MAP: {` |
|        - | 2806 | `	VmOpRc rcOp;` |
|   115581 | 2807 | `	sState.pTos = pTos;` |
|   115581 | 2808 | `	sState.pc = pc;` |
|   115581 | 2809 | `	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);` |
|   115581 | 2810 | `	pTos = sState.pTos;` |
|   115581 | 2811 | `	pc = sState.pc;` |
|   115581 | 2812 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2813 | `		goto Abort;` |
|   115581 | 2814 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       18 | 2815 | `		goto Exception;` |
|        - | 2816 | `	}` |
|   115565 | 2817 | `	break;` |
|        - | 2818 | `					  }` |
|        - | 2819 | `/*` |
|        - | 2820 | ` * LOAD_LIST: P1 * *` |
|        - | 2821 | ` *` |
|        - | 2822 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - | 2823 | ` * This is the VM implementation of the list() PHP construct.` |
|        - | 2824 | ` * Caveats:` |
|        - | 2825 | ` *  This implementation support only a single nesting level.` |
|        - | 2826 | ` */` |
|   196262 | 2827 | `case PH7_OP_LOAD_LIST: {` |
|        - | 2828 | `	VmOpRc rcOp;` |
|   392529 | 2829 | `	sState.pTos = pTos;` |
|   392529 | 2830 | `	sState.pc = pc;` |
|   392529 | 2831 | `	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);` |
|   392529 | 2832 | `	pTos = sState.pTos;` |
|   392529 | 2833 | `	pc = sState.pc;` |
|   392529 | 2834 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2835 | `		goto Abort;` |
|   392529 | 2836 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 2837 | `		goto Exception;` |
|        - | 2838 | `	}` |
|   392525 | 2839 | `	break;` |
|        - | 2840 | `					  }` |
|        - | 2841 | `/*` |
|        - | 2842 | ` * LOAD_IDX: P1 P2 *` |
|        - | 2843 | ` *` |
|        - | 2844 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - | 2845 | ` * from the stack.` |
|        - | 2846 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - | 2847 | ` * instead.` |
|        - | 2848 | ` */` |
|   549390 | 2849 | `case PH7_OP_LOAD_IDX: {` |
|        - | 2850 | `	VmOpRc rcOp;` |
|  1101133 | 2851 | `	sState.pTos = pTos;` |
|  1101133 | 2852 | `	sState.pc = pc;` |
|  1101133 | 2853 | `	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);` |
|  1101133 | 2854 | `	pTos = sState.pTos;` |
|  1101133 | 2855 | `	pc = sState.pc;` |
|  1101133 | 2856 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2857 | `		goto Abort;` |
|  1101133 | 2858 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      100 | 2859 | `		goto Exception;` |
|        - | 2860 | `	}` |
|  1101037 | 2861 | `	break;` |
|        - | 2862 | `					  }` |
|        - | 2863 | `/*` |
|        - | 2864 | ` * LOAD_CLOSURE * * P3` |
|        - | 2865 | ` *` |
|        - | 2866 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - | 2867 | ` * name in the stack.` |
|        - | 2868 | ` */` |
|     6236 | 2869 | `case PH7_OP_LOAD_CLOSURE: {` |
|        - | 2870 | `	VmOpRc rcOp;` |
|    12477 | 2871 | `	sState.pTos = pTos;` |
|    12477 | 2872 | `	sState.pc = pc;` |
|    12477 | 2873 | `	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);` |
|    12477 | 2874 | `	pTos = sState.pTos;` |
|    12477 | 2875 | `	pc = sState.pc;` |
|    12477 | 2876 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 2877 | `		goto Abort;` |
|    12477 | 2878 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 2879 | `		goto Exception;` |
|        - | 2880 | `	}` |
|    12477 | 2881 | `	break;` |
|        - | 2882 | `					  }` |
|        - | 2883 | `/*` |
|        - | 2884 | ` * LOAD_FCC P1 * *` |
|        - | 2885 | ` *` |
|        - | 2886 | ` * First-class callable: wrap the callee in a Closure object instead of calling it.` |
|        - | 2887 | ` *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS` |
|        - | 2888 | ` *           (from the callee's OP_LOADC). Replace it in place with a Closure whose` |
|        - | 2889 | ` *           $__fn is that name; the existing string-callable dispatch resolves it.` |
|        - | 2890 | ` *           (OOM degrades to leaving the name string on the stack — still callable.)` |
|        - | 2891 | ` *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name` |
|        - | 2892 | ` *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds` |
|        - | 2893 | ` *           $this (scope = its class); a class-name-string target is a static callable` |
|        - | 2894 | ` *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —` |
|        - | 2895 | ` *           the popped target leaves no name string to keep.)` |
|        - | 2896 | ` */` |
|      161 | 2897 | `case PH7_OP_LOAD_FCC:{` |
|      326 | 2898 | `	if( pInstr->iP1 == 1 ){` |
|        - | 2899 | ``		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)`` |
|        - | 2900 | `		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —` |
|        - | 2901 | `		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,` |
|        - | 2902 | ``		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a`` |
|        - | 2903 | `		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is` |
|        - | 2904 | `		 * (graceful degradation), and so is the original on OOM — still whatever it was. */` |
|        - | 2905 | `		ph7_class_instance *pCloObj;` |
|        - | 2906 | `		sxi32 nFccBrc;` |
|        - | 2907 | `		const void *pFccRes;` |
|      145 | 2908 | `		if( VmValueIsClosure(pVm, pTos) ){` |
|       11 | 2909 | `			break;` |
|        - | 2910 | `		}` |
|        - | 2911 | `		/* php's global fallback for an UNQUALIFIED function name written inside a` |
|        - | 2912 | `		 * namespace: the current namespace first, the global one after. The compiler` |
|        - | 2913 | `		 * qualified this name and marks the instruction (iP2==1) when it did, so the` |
|        - | 2914 | `		 * fallback happens here — the OP_CALL path does the same thing from its arg map,` |
|        - | 2915 | ``		 * which an FCC has none of. `strlen(...)` in a namespaced file was`` |
|        - | 2916 | ``		 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|      132 | 2917 | `		if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING)` |
|       13 | 2918 | `			&& !PH7_VmIsCallable(pVm,pTos,TRUE) ){` |
|        7 | 2919 | `			const char *zFccName = (const char *)SyBlobData(&pTos->sBlob);` |
|        7 | 2920 | `			sxu32 nFccName = SyBlobLength(&pTos->sBlob);` |
|        7 | 2921 | `			const char *zFccShort = zFccName;` |
|        - | 2922 | `			sxu32 iFccPos;` |
|      165 | 2923 | `			for( iFccPos = 0 ; iFccPos < nFccName ; ++iFccPos ){` |
|      159 | 2924 | `				if( zFccName[iFccPos] == '\\' ){` |
|        7 | 2925 | `					zFccShort = &zFccName[iFccPos + 1];` |
|        3 | 2926 | `				}` |
|       80 | 2927 | `			}` |
|        7 | 2928 | `			if( zFccShort != zFccName ){` |
|        - | 2929 | `				ph7_value sFccShort;` |
|        7 | 2930 | `				PH7_MemObjInit(pVm,&sFccShort);` |
|       10 | 2931 | `				PH7_MemObjStringAppend(&sFccShort,zFccShort,` |
|        6 | 2932 | `					(sxu32)(nFccName - (sxu32)(zFccShort - zFccName)));` |
|        7 | 2933 | `				if( PH7_VmIsCallable(pVm,&sFccShort,TRUE) ){` |
|        5 | 2934 | `					PH7_MemObjStore(&sFccShort,pTos);` |
|        2 | 2935 | `				}` |
|        7 | 2936 | `				PH7_MemObjRelease(&sFccShort);` |
|        3 | 2937 | `			}` |
|        3 | 2938 | `		}` |
|        - | 2939 | `		/* The array shape's class lookup can run an autoloader that throws; php propagates` |
|        - | 2940 | `		 * THAT exception and never reports the callable bad, exactly as at the OP_CALL sites. */` |
|      135 | 2941 | `		nFccBrc = pVm->nBoundaryRc;` |
|      135 | 2942 | `		pFccRes = (const void *)pVm->pResumeFrame;` |
|      135 | 2943 | `		pCloObj = VmFccWrapValue(pVm, pTos);` |
|      135 | 2944 | `		if( pCloObj ){` |
|      105 | 2945 | `			PH7_MemObjRelease(pTos);` |
|      105 | 2946 | `			pCloObj->iRef++;` |
|      105 | 2947 | `			pTos->x.pOther = pCloObj;` |
|      105 | 2948 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       54 | 2949 | `		}else{` |
|        - | 2950 | `			/* php refuses a non-callable HERE, with the direct dispatch's own wording — the` |
|        - | 2951 | ``			 * `(...)` does not make a bad callable acceptable, it just defers the call. */`` |
|        - | 2952 | `			char zFccMsg[192];` |
|       31 | 2953 | `			const char *zFccBad = VmFccValueError(&(*pVm),pTos,zFccMsg,sizeof(zFccMsg));` |
|       31 | 2954 | `			int bFccRaised = PH7_VmClassLookupRaised(&(*pVm),nFccBrc,pFccRes);` |
|       31 | 2955 | `			if( zFccBad \|\| bFccRaised ){` |
|        - | 2956 | `				sxi32 rcFcc;` |
|       31 | 2957 | `				PH7_MemObjRelease(pTos);` |
|       31 | 2958 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       31 | 2959 | `				pTos->nIdx = SXU32_HIGH;` |
|       31 | 2960 | `				if( bFccRaised ){` |
|      ! 0 | 2961 | `					rcFcc = pVm->nBoundaryRc;` |
|      ! 0 | 2962 | `					pVm->nBoundaryRc = 0;` |
|      ! 0 | 2963 | `					if( rcFcc == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 2964 | `					rc = PH7_EXCEPTION;` |
|       15 | 2965 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2966 | `				}` |
|       31 | 2967 | `				rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccBad,(sxu32)SyStrlen(zFccBad));` |
|       31 | 2968 | `				if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       31 | 2969 | `				rc = rcFcc;` |
|       61 | 2970 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 2971 | `			}` |
|        - | 2972 | `		}` |
|       54 | 2973 | `	}else{` |
|        - | 2974 | `		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]` |
|        - | 2975 | `		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its` |
|        - | 2976 | `		 * class); a class-name string target is a static callable (scope = that class). */` |
|      184 | 2977 | `		ph7_value *pTarget = &pTos[-1];` |
|        - | 2978 | `		SyString sName;` |
|        - | 2979 | `		ph7_class_instance *pCloObj;` |
|      184 | 2980 | `		ph7_class *pFccCls = 0;` |
|      184 | 2981 | `		ph7_class_instance *pFccRecv = 0;` |
|      184 | 2982 | `		const char *zFccErr = 0;` |
|        - | 2983 | `		char zFccMsg[192];` |
|      184 | 2984 | `		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));` |
|      184 | 2985 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      108 | 2986 | `			pFccRecv = (ph7_class_instance *)pTarget->x.pOther;` |
|      108 | 2987 | `			pFccCls = pFccRecv->pClass;` |
|      108 | 2988 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pFccCls) ){` |
|        - | 2989 | ``				/* `$inc->m(...)` resolves the method at CREATION, so php's`` |
|        - | 2990 | `				 * incomplete-object call Error is raised here, not at a later` |
|        - | 2991 | `				 * invocation. */` |
|        - | 2992 | `				SyBlob sIncErr;` |
|        - | 2993 | `				sxi32 rcInc;` |
|        3 | 2994 | `				SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|        3 | 2995 | `				PH7_VmIncompleteMsg(&(*pVm),pFccRecv,"call a method",&sIncErr);` |
|        3 | 2996 | `				VmPopOperand(&pTos,1);       /* the method name */` |
|        3 | 2997 | `				PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|        3 | 2998 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 2999 | `				pTos->nIdx = SXU32_HIGH;` |
|        4 | 3000 | `				rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|        1 | 3001 | `					SyBlobLength(&sIncErr));` |
|        3 | 3002 | `				SyBlobRelease(&sIncErr);` |
|        3 | 3003 | `				if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|        3 | 3004 | `				rc = rcInc;` |
|        3 | 3005 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        4 | 3006 | `			}` |
|      130 | 3007 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|        - | 3008 | ``			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class`` |
|        - | 3009 | `			 * now, so the closure binds the concrete scope (matching PHP). */` |
|       79 | 3010 | `			pFccCls = VmFccResolveScope(pVm, pTarget);` |
|       38 | 3011 | `		}` |
|      182 | 3012 | `		if( pTarget->iFlags & (MEMOBJ_OBJ\|MEMOBJ_STRING) ){` |
|        - | 3013 | `			/* php resolves the member HERE, through the same lookup the call would use:` |
|        - | 3014 | `			 * every refusal a call would raise is raised at CREATION, and a non-static` |
|        - | 3015 | `			 * method named through a class binds the calling frame's own $this. */` |
|      182 | 3016 | `			ph7_class_instance *pRecvOut = 0;` |
|      271 | 3017 | `			zFccErr = VmFccMemberError(&(*pVm),pFccCls,` |
|      178 | 3018 | `				(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),` |
|       89 | 3019 | `				SyStringData(&sName),SyStringLength(&sName),` |
|      178 | 3020 | `				(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,` |
|       89 | 3021 | `				&pRecvOut,zFccMsg,sizeof(zFccMsg));` |
|      182 | 3022 | `			if( pRecvOut ){` |
|       13 | 3023 | ``				pFccRecv = pRecvOut; /* the receiver php binds into a `C::m(...)` callable */`` |
|        6 | 3024 | `			}` |
|       89 | 3025 | `		}` |
|      182 | 3026 | `		if( zFccErr ){` |
|        - | 3027 | `			sxi32 rcFcc;` |
|       24 | 3028 | `			VmPopOperand(&pTos,1);       /* the method name */` |
|       24 | 3029 | `			PH7_MemObjRelease(pTos);     /* the target slot becomes the NULL result */` |
|       24 | 3030 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       24 | 3031 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 | 3032 | `			rcFcc = VmThrowFromVm(&(*pVm),"Error",zFccErr,(sxu32)SyStrlen(zFccErr));` |
|       24 | 3033 | `			if( rcFcc == SXERR_ABORT ){ goto Abort; }` |
|       24 | 3034 | `			rc = rcFcc;` |
|       30 | 3035 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3036 | `		}` |
|      160 | 3037 | `		if( pFccCls == 0 ){` |
|      ! 0 | 3038 | `			pCloObj = 0;` |
|      160 | 3039 | `		}else if( pFccRecv ){` |
|      108 | 3040 | `			pCloObj = VmCreateClosure(pVm, &sName, pFccRecv, &pFccRecv->pClass->sName);` |
|       56 | 3041 | `		}else{` |
|       55 | 3042 | `			pCloObj = VmCreateClosure(pVm, &sName, 0, &pFccCls->sName);` |
|        - | 3043 | `		}` |
|      160 | 3044 | `		if( pCloObj ){` |
|        - | 3045 | ``			/* `$o->m(...)` / `C::m(...)` names a METHOD, whatever the class turns out to`` |
|        - | 3046 | `			 * declare: the unwrap must not go looking for a FUNCTION of that name, and a` |
|        - | 3047 | `			 * name the class answers only through __call is still a method call. */` |
|      160 | 3048 | `			pCloObj->iFlags \|= VM_INSTANCE_FCC_METHOD;` |
|        - | 3049 | `			/* The screen above already ran, HERE, where php runs it — so record that this` |
|        - | 3050 | `			 * closure's callee is settled and the invocation must not re-decide it. A name` |
|        - | 3051 | `			 * that resolved to the catch-all instead keeps routing there. */` |
|      160 | 3052 | `			if( PH7_VmFccMethodIsDirect(&(*pVm),pFccCls,SyStringData(&sName),SyStringLength(&sName)) ){` |
|      146 | 3053 | `				pCloObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       71 | 3054 | `			}` |
|       78 | 3055 | `		}` |
|        - | 3056 | `		/* Pop the method name and the target, push the Closure. */` |
|      160 | 3057 | `		PH7_MemObjRelease(pTos);` |
|      160 | 3058 | `		pTos--;` |
|      160 | 3059 | `		PH7_MemObjRelease(pTos);` |
|      160 | 3060 | `		if( pCloObj ){` |
|      160 | 3061 | `			pCloObj->iRef++;` |
|      160 | 3062 | `			pTos->x.pOther = pCloObj;` |
|      160 | 3063 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       82 | 3064 | `		}else{` |
|      ! 0 | 3065 | `			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */` |
|        - | 3066 | `		}` |
|        - | 3067 | `	}` |
|      262 | 3068 | `	break;` |
|        - | 3069 | `					 }` |
|        - | 3070 | `/*` |
|        - | 3071 | ` * STORE * P2 P3` |
|        - | 3072 | ` *` |
|        - | 3073 | ` * Perform a store (Assignment) operation.` |
|        - | 3074 | ` */` |
|   578131 | 3075 | `case PH7_OP_STORE: {` |
|        - | 3076 | `	ph7_value *pObj;` |
|        - | 3077 | `	SyString sName;` |
|        - | 3078 | `#ifdef UNTRUST` |
|        - | 3079 | `	if( pTos < pStack ){` |
|        - | 3080 | `		goto Abort;` |
|        - | 3081 | `	}` |
|        - | 3082 | `#endif` |
|  1159391 | 3083 | `	if( pInstr->iP2 ){` |
|        - | 3084 | `		sxu32 nIdx;` |
|        - | 3085 | `		sxi32 rcT;` |
|        - | 3086 | `		/* Member store operation */` |
|   103469 | 3087 | `		nIdx = pTos->nIdx;` |
|   103469 | 3088 | `		VmPopOperand(&pTos,1);` |
|   103469 | 3089 | `		if( pVm->pMagicSetThis ){` |
|        - | 3090 | `			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain` |
|        - | 3091 | `			 * store to a missing/inaccessible property on a class declaring __set.` |
|        - | 3092 | `			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on` |
|        - | 3093 | `			 * the stack as the assignment expression's result — php's semantics` |
|        - | 3094 | `			 * (no property is created; a throw rides the boundary rail). */` |
|      377 | 3095 | `			ph7_class_instance *pSetThis = pVm->pMagicSetThis;` |
|        - | 3096 | `			SyString sSetName;` |
|      377 | 3097 | `			pVm->pMagicSetThis = 0;` |
|      377 | 3098 | `			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));` |
|      377 | 3099 | `			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);` |
|      377 | 3100 | `			PH7_ClassInstanceUnref(pSetThis);` |
|      377 | 3101 | `			SyBlobReset(&pVm->sMagicSetName);` |
|      377 | 3102 | `			break;` |
|        - | 3103 | `		}` |
|   103095 | 3104 | `		if( pVm->pHookSetThis ){` |
|        - | 3105 | `			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a` |
|        - | 3106 | `			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with` |
|        - | 3107 | `			 * the rvalue (which stays on the stack as the assignment expression's` |
|        - | 3108 | ``			 * result); a `set => expr` hook's return value is stored into the`` |
|        - | 3109 | `			 * BACKING slot with the ordinary typed enforcement. A property with` |
|        - | 3110 | `			 * only a get hook is php's catchable "is read-only" Error. */` |
|       56 | 3111 | `			ph7_class_instance *pHThis = pVm->pHookSetThis;` |
|       56 | 3112 | `			ph7_class_attr *pHAttr = pVm->pHookSetAttr;` |
|       56 | 3113 | `			sxu32 nBackIdx = pVm->nHookSetIdx;` |
|        - | 3114 | `			sxi32 rcHs;` |
|       56 | 3115 | `			pVm->pHookSetThis = 0;` |
|       56 | 3116 | `			pVm->pHookSetAttr = 0;` |
|       56 | 3117 | `			pVm->nHookSetIdx = SXU32_HIGH;` |
|       56 | 3118 | `			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);` |
|       56 | 3119 | `			PH7_ClassInstanceUnref(pHThis);` |
|       56 | 3120 | `			if( rcHs == PH7_ABORT ){` |
|      ! 0 | 3121 | `				goto Abort;` |
|        - | 3122 | `			}` |
|       56 | 3123 | `			break;` |
|        - | 3124 | `		}` |
|   103043 | 3125 | `		if( nIdx == SXU32_HIGH ){` |
|        - | 3126 | `			/* No slot behind the property: the receiver is a TEMPORARY nothing else` |
|        - | 3127 | ``			 * holds (`mk()->p = 5` on a freshly built object, `(new A)->p` where the`` |
|        - | 3128 | `			 * compile-time screen lets it through). php performs the write on the` |
|        - | 3129 | `			 * doomed object and says nothing — the object is gone at the end of the` |
|        - | 3130 | `			 * statement, so the store is unobservable either way. PHL announced it` |
|        - | 3131 | ``			 * with `Cannot perform assignment on a constant class attribute,PH7 is`` |
|        - | 3132 | ``			 * loading NULL`, a diagnostic php has no equivalent of. Every case that`` |
|        - | 3133 | `			 * IS a refusal — a class constant, a hooked or handler-backed property —` |
|        - | 3134 | `			 * is decided before this point now. */` |
|        3 | 3135 | `			pTos->nIdx = SXU32_HIGH;` |
|        2 | 3136 | `		}else{` |
|        - | 3137 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - | 3138 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|   103041 | 3139 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);` |
|   103041 | 3140 | `			if( rcT == PH7_ABORT ){` |
|       13 | 3141 | `				goto Abort;` |
|        - | 3142 | `			}` |
|   103031 | 3143 | `			if( rcT == PH7_EXCEPTION ){` |
|        - | 3144 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - | 3145 | `				 * control to the nearest catch block if any (draining any` |
|        - | 3146 | `				 * abandoned outer-expression operands to the try's base),` |
|        - | 3147 | `				 * otherwise propagate out of the VM loop. */` |
|   100201 | 3148 | `				VmPopOperand(&pTos,1);` |
|        - | 3149 | `				{` |
|        - | 3150 | `					sxi32 iRp;` |
|   100201 | 3151 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|   400109 | 3152 | `						PH7_RESUME_DRAIN()` |
|   100107 | 3153 | `						pc = iRp;` |
|   100107 | 3154 | `						break;` |
|        - | 3155 | `					}` |
|        - | 3156 | `				}` |
|       98 | 3157 | `				goto Exception;` |
|        - | 3158 | `			}` |
|        - | 3159 | `			/* Point to the desired memory object */` |
|     2835 | 3160 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2835 | 3161 | `			if( pObj ){` |
|        - | 3162 | `				/* Perform the store operation */` |
|     2835 | 3163 | `				PH7_MemObjStore(pTos,pObj);` |
|     1415 | 3164 | `			}` |
|        - | 3165 | `		}` |
|     2837 | 3166 | `		break;` |
|  1055927 | 3167 | `	}else if( pInstr->p3 == 0 ){` |
|        - | 3168 | `		/* Take the variable name from the next on the stack (user-visible: a` |
|        - | 3169 | `		 * variable-variable NAME $$arr warns on an array, §2) */` |
|        - | 3170 | `		{` |
|       26 | 3171 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       26 | 3172 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 3173 | `		}` |
|       24 | 3174 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       24 | 3175 | `		pTos--;` |
|        - | 3176 | `#ifdef UNTRUST` |
|        - | 3177 | `		if( pTos < pStack  ){` |
|        - | 3178 | `			goto Abort;` |
|        - | 3179 | `		}` |
|        - | 3180 | `#endif` |
|       13 | 3181 | `	}else{` |
|  1055903 | 3182 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - | 3183 | `	}` |
|  1055920 | 3184 | `	if( sName.nByte == sizeof("GLOBALS")-1` |
|   531519 | 3185 | `	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){` |
|        6 | 3186 | `		if( pInstr->p3 ){` |
|        - | 3187 | `			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time` |
|        - | 3188 | `			 * fatal there; raised at the store site here with the same` |
|        - | 3189 | `			 * message and the same non-catchable outcome). Element writes` |
|        - | 3190 | `			 * are unaffected. */` |
|        3 | 3191 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3192 | `				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 3193 | `			pVm->iExitStatus = 255;` |
|        3 | 3194 | `			pVm->bHaltRequested = 1;` |
|        3 | 3195 | `			goto Abort;` |
|        - | 3196 | `		}` |
|        - | 3197 | `		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's` |
|        - | 3198 | `		 * compile-time case: php quietly creates an ordinary symbol-table` |
|        - | 3199 | `		 * entry named GLOBALS, leaving the auto-global view intact. */` |
|        3 | 3200 | `		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);` |
|        3 | 3201 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 | 3202 | `		break;` |
|        - | 3203 | `	}` |
|        - | 3204 | `	/* Extract the desired variable and if not available dynamically create it */` |
|  1055921 | 3205 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|  1055921 | 3206 | `	if( pObj == 0 ){` |
|      ! 0 | 3207 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 3208 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 | 3209 | `		goto Abort;` |
|        - | 3210 | `	}` |
|  1055921 | 3211 | `	if( !pInstr->p3 ){` |
|       22 | 3212 | `		PH7_MemObjRelease(&pTos[1]);` |
|       10 | 3213 | `	}` |
|        - | 3214 | `	/* Perform the store operation */` |
|  1055921 | 3215 | `	PH7_MemObjStore(pTos,pObj);` |
|  1055921 | 3216 | `	break;` |
|        - | 3217 | `				   }` |
|        - | 3218 | `/*` |
|        - | 3219 | ` * STORE_IDX:   P1 * P3` |
|        - | 3220 | ` * STORE_IDX_R: P1 * P3` |
|        - | 3221 | ` *` |
|        - | 3222 | ` * Perfrom a store operation an a hashmap entry.` |
|        - | 3223 | ` */` |
|   218391 | 3224 | `case PH7_OP_STORE_IDX:` |
|        - | 3225 | `case PH7_OP_STORE_IDX_REF: {` |
|        - | 3226 | `	VmOpRc rcOp;` |
|   436770 | 3227 | `	sState.pTos = pTos;` |
|   436770 | 3228 | `	sState.pc = pc;` |
|   436770 | 3229 | `	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);` |
|   436770 | 3230 | `	pTos = sState.pTos;` |
|   436770 | 3231 | `	pc = sState.pc;` |
|   436770 | 3232 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 3233 | `		goto Abort;` |
|   436768 | 3234 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       54 | 3235 | `		goto Exception;` |
|        - | 3236 | `	}` |
|   436718 | 3237 | `	break;` |
|        - | 3238 | `					  }` |
|        - | 3239 | `/*` |
|        - | 3240 | ` * INCR: P1 * *` |
|        - | 3241 | ` *` |
|        - | 3242 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - | 3243 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - | 3244 | ` * the stack and increment after that.` |
|        - | 3245 | ` */` |
|   364231 | 3246 | `case PH7_OP_INCR:` |
|        - | 3247 | `#ifdef UNTRUST` |
|        - | 3248 | `	if( pTos < pStack ){` |
|        - | 3249 | `		goto Abort;` |
|        - | 3250 | `	}` |
|        - | 3251 | `#endif` |
|        - | 3252 | ``	/* `++` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3253 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3254 | `	 * — which otherwise skips object/array/resource operands. */` |
|   729594 | 3255 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3256 | `	/* php refuses to increment a string OFFSET, whatever byte it holds. */` |
|   729582 | 3257 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3258 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3259 | `	 * php's TypeError, raised BEFORE any set dispatch (the type guard below` |
|        - | 3260 | `	 * would skip the mutation and the tail write-back would otherwise call` |
|        - | 3261 | `	 * the set hook with the unchanged value). */` |
|   729561 | 3262 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|   365360 | 3263 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        5 | 3264 | `		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        5 | 3265 | `		if( VM_HOOK_PEND_IS_RMW(pTopInc->iKind) && pTopInc->nScratchIdx == pTos->nIdx ){` |
|        - | 3266 | `			SyBlob sErrMsg;` |
|        5 | 3267 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        5 | 3268 | `			VmIncDecTypeErrorMsg(pTos,TRUE,&sErrMsg);` |
|        5 | 3269 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        5 | 3270 | `			VmHookRmwDropTop(&(*pVm));` |
|        5 | 3271 | `			pTos->nIdx = SXU32_HIGH;` |
|        5 | 3272 | `			break;` |
|        - | 3273 | `		}` |
|      ! 0 | 3274 | `	}` |
|   729562 | 3275 | `	PH7_INCDEC_NATIVE_ARITH("+")` |
|   729560 | 3276 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3277 | `		/* php's ++ operand contract: an array, object or resource is a catchable` |
|        - | 3278 | `		 * TypeError, not the silent no-op this used to be. Settle the operand` |
|        - | 3279 | `		 * (the op's result slot) BEFORE throwing, like the arithmetic sites. */` |
|        - | 3280 | `		SyBlob sIncMsg;` |
|        - | 3281 | `		sxi32 rcInc;` |
|       23 | 3282 | `		SyBlobInit(&sIncMsg,&pVm->sAllocator);` |
|       23 | 3283 | `		VmIncDecTypeErrorMsg(pTos,TRUE,&sIncMsg);` |
|       23 | 3284 | `		PH7_MemObjRelease(pTos);` |
|       23 | 3285 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       23 | 3286 | `		pTos->nIdx = SXU32_HIGH;` |
|       34 | 3287 | `		rcInc = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sIncMsg),` |
|       11 | 3288 | `			SyBlobLength(&sIncMsg));` |
|       23 | 3289 | `		SyBlobRelease(&sIncMsg);` |
|       23 | 3290 | `		if( rcInc == SXERR_ABORT ){ goto Abort; }` |
|       23 | 3291 | `		rc = rcInc;` |
|       25 | 3292 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3293 | `	}` |
|        - | 3294 | ``	/* php 8.3+: `++` on a BOOL is a no-op it warns about (E_WARNING, errno 2 —`` |
|        - | 3295 | ``	 * same shape as the null `--` below), where PH7 coerced true to int 2. */`` |
|   729538 | 3296 | `	if( pTos->iFlags & MEMOBJ_BOOL ){` |
|        7 | 3297 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3298 | `			"Increment on type bool has no effect, this will change in the next major version of PHP");` |
|        3 | 3299 | `	}` |
|   729538 | 3300 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_BOOL)) == 0 ){` |
|   729532 | 3301 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3302 | `			ph7_value *pObj;` |
|   729520 | 3303 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   729520 | 3304 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3305 | `					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric` |
|        - | 3306 | `					 * string (it points at str_increment() instead); PHL rejects it. */` |
|        - | 3307 | `					SyBlob sErrMsg;` |
|        3 | 3308 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3309 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3310 | `						"Increment on a non-numeric string is not supported, use str_increment() instead",` |
|        - | 3311 | `						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);` |
|        3 | 3312 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3313 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3314 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3315 | `					break;` |
|      ! 0 | 3316 | `				}else{` |
|        - | 3317 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - | 3318 | `					 * original value: pTos may alias pObj's blob via` |
|        - | 3319 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - | 3320 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - | 3321 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - | 3322 | `					 * so its old-value view survives the coercion. */` |
|   729518 | 3323 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 | 3324 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        5 | 3325 | `					}` |
|        - | 3326 | `					/* Force a numeric cast on the variable */` |
|   729518 | 3327 | `					PH7_MemObjToNumeric(pObj);` |
|   729518 | 3328 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        7 | 3329 | `						pObj->rVal++;` |
|        - | 3330 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3331 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3332 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3333 | `						 * integer-valued real. */` |
|        7 | 3334 | `						PH7_MemObjTryInteger(pObj);` |
|        4 | 3335 | `					}else{` |
|        - | 3336 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3337 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3338 | `						sxi64 r;` |
|   729512 | 3339 | `						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3340 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        7 | 3341 | `							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;` |
|        7 | 3342 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3343 | `#else` |
|        - | 3344 | `							pObj->x.iVal = r;` |
|        - | 3345 | `#endif` |
|        4 | 3346 | `						}else{` |
|   729506 | 3347 | `							pObj->x.iVal = r;` |
|        - | 3348 | `						}` |
|        - | 3349 | `					}` |
|   729518 | 3350 | `					if( pInstr->iP1 ){` |
|        - | 3351 | `						/* Pre-increment: result is the new value. */` |
|       73 | 3352 | `						PH7_MemObjStore(pObj,pTos);` |
|       36 | 3353 | `					}` |
|        - | 3354 | `					/* Post-increment: pTos retains the old value (a string` |
|        - | 3355 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - | 3356 | `					/* A NATIVE class's property is php's own C struct field, and` |
|        - | 3357 | ``					 * `++` writes it BACK through the write handler there — so the`` |
|        - | 3358 | `					 * conversion runs on the mutated slot. php's own answer, and` |
|        - | 3359 | `					 * the EXPRESSION's value is the unconverted sum either way:` |
|        - | 3360 | ``					 * `$i->f = 1.456008; var_dump(++$i->f, $i->f)` prints`` |
|        - | 3361 | `					 * 2.4560079999999997 then 2.456007. */` |
|   729518 | 3362 | `					PH7_NATIVE_SET_AFTER_MUTATE(pTos->nIdx,pObj)` |
|        - | 3363 | `				}` |
|   365317 | 3364 | `			}` |
|   365322 | 3365 | `		}else{` |
|       13 | 3366 | `			if( pInstr->iP1 ){` |
|      ! 0 | 3367 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 | 3368 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 | 3369 | `				}else{` |
|        - | 3370 | `					/* Force a numeric cast */` |
|      ! 0 | 3371 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3372 | `					/* Pre-increment */` |
|      ! 0 | 3373 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3374 | `						pTos->rVal++;` |
|        - | 3375 | `						/* Try to get an integer representation */` |
|      ! 0 | 3376 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3377 | `					}else{` |
|        - | 3378 | `						/* PHP promotes PHP_INT_MAX++ to float; the integer-only` |
|        - | 3379 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3380 | `						sxi64 r;` |
|      ! 0 | 3381 | `						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3382 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3383 | `							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;` |
|      ! 0 | 3384 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3385 | `#else` |
|        - | 3386 | `							pTos->x.iVal = r;` |
|        - | 3387 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3388 | `#endif` |
|      ! 0 | 3389 | `						}else{` |
|      ! 0 | 3390 | `							pTos->x.iVal = r;` |
|      ! 0 | 3391 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3392 | `						}` |
|        - | 3393 | `					}` |
|        - | 3394 | `				}` |
|      ! 0 | 3395 | `			}` |
|        - | 3396 | `		}` |
|   365323 | 3397 | `	}` |
|   729536 | 3398 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|   729536 | 3399 | `	break;` |
|        - | 3400 | `/*` |
|        - | 3401 | ` * DECR: P1 * *` |
|        - | 3402 | ` *` |
|        - | 3403 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - | 3404 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - | 3405 | ` * and decrement after that.` |
|        - | 3406 | ` */` |
|       78 | 3407 | `case PH7_OP_DECR:` |
|        - | 3408 | `#ifdef UNTRUST` |
|        - | 3409 | `	if( pTos < pStack ){` |
|        - | 3410 | `		goto Abort;` |
|        - | 3411 | `	}` |
|        - | 3412 | `#endif` |
|        - | 3413 | ``	/* `--` on a readonly property is forbidden regardless of the current value's`` |
|        - | 3414 | `	 * type (it bypasses the store path), so enforce before the type guard below` |
|        - | 3415 | `	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly` |
|        - | 3416 | `	 * property currently holding null). */` |
|      164 | 3417 | `	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);` |
|        - | 3418 | `	/* php refuses to decrement a string OFFSET, whatever byte it holds. */` |
|      151 | 3419 | `	PH7_REJECT_STROFFSET_INCDEC()` |
|        - | 3420 | `	/* A hooked property whose get hook returned an array/object/resource:` |
|        - | 3421 | `	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —` |
|        - | 3422 | ``	 * `--` on null is php's no-op and its write-back still dispatches set). */`` |
|      144 | 3423 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0` |
|       82 | 3424 | `	 && SySetUsed(&pVm->aHookRmw) > 0 ){` |
|        3 | 3425 | `		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        3 | 3426 | `		if( VM_HOOK_PEND_IS_RMW(pTopDec->iKind) && pTopDec->nScratchIdx == pTos->nIdx ){` |
|        - | 3427 | `			SyBlob sErrMsg;` |
|        3 | 3428 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3429 | `			VmIncDecTypeErrorMsg(pTos,FALSE,&sErrMsg);` |
|        3 | 3430 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3431 | `			VmHookRmwDropTop(&(*pVm));` |
|        3 | 3432 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 | 3433 | `			break;` |
|        - | 3434 | `		}` |
|      ! 0 | 3435 | `	}` |
|      145 | 3436 | `	PH7_INCDEC_NATIVE_ARITH("-")` |
|      143 | 3437 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) != 0 ){` |
|        - | 3438 | ``		/* php's `--` operand contract, the mirror of INCR's: an array, object or`` |
|        - | 3439 | `		 * resource is a catchable TypeError, not a silent no-op. */` |
|        - | 3440 | `		SyBlob sDecMsg;` |
|        - | 3441 | `		sxi32 rcDec;` |
|       11 | 3442 | `		SyBlobInit(&sDecMsg,&pVm->sAllocator);` |
|       11 | 3443 | `		VmIncDecTypeErrorMsg(pTos,FALSE,&sDecMsg);` |
|       11 | 3444 | `		PH7_MemObjRelease(pTos);` |
|       11 | 3445 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 | 3446 | `		pTos->nIdx = SXU32_HIGH;` |
|       16 | 3447 | `		rcDec = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sDecMsg),` |
|        5 | 3448 | `			SyBlobLength(&sDecMsg));` |
|       11 | 3449 | `		SyBlobRelease(&sDecMsg);` |
|       11 | 3450 | `		if( rcDec == SXERR_ABORT ){ goto Abort; }` |
|       11 | 3451 | `		rc = rcDec;` |
|       13 | 3452 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3453 | `	}` |
|        - | 3454 | ``	/* NULL and BOOL stay excluded: PHP leaves `--` on either untouched (no-op) --`` |
|        - | 3455 | `	 * but 8.3 warns about both no-ops, same as the non-numeric-string one below. */` |
|      133 | 3456 | `	if( pTos->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|        - | 3457 | `		/* E_WARNING, not E_DEPRECATED -- php reports these at errno 2. */` |
|       19 | 3458 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3459 | `			"Decrement on type %s has no effect, this will change in the next major version of PHP",` |
|       12 | 3460 | `			(pTos->iFlags & MEMOBJ_NULL) ? "null" : "bool");` |
|        6 | 3461 | `	}` |
|      133 | 3462 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      121 | 3463 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - | 3464 | `			ph7_value *pObj;` |
|      119 | 3465 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      119 | 3466 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - | 3467 | ``					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string`` |
|        - | 3468 | `					 * (php has no string decrement); PHL rejects it. */` |
|        - | 3469 | `					SyBlob sErrMsg;` |
|        3 | 3470 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|        3 | 3471 | `					SyBlobAppend(&sErrMsg,` |
|        - | 3472 | `						"Decrement on a non-numeric string is not supported",` |
|        - | 3473 | `						sizeof("Decrement on a non-numeric string is not supported")-1);` |
|        3 | 3474 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|        3 | 3475 | `					VmHookRmwDropTop(&(*pVm));` |
|        3 | 3476 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 | 3477 | `					break;` |
|      ! 0 | 3478 | `				}else{` |
|        - | 3479 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - | 3480 | `					 * post-decrement must preserve pTos's original value, which` |
|        - | 3481 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - | 3482 | `					 * Force pTos to own its blob before coercing pObj. */` |
|      116 | 3483 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 | 3484 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 | 3485 | `					}` |
|      116 | 3486 | `					PH7_MemObjToNumeric(pObj);` |
|      116 | 3487 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|       11 | 3488 | `						pObj->rVal--;` |
|        - | 3489 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - | 3490 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - | 3491 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - | 3492 | `						 * integer-valued real. */` |
|       11 | 3493 | `						PH7_MemObjTryInteger(pObj);` |
|        6 | 3494 | `					}else{` |
|        - | 3495 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3496 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3497 | `						sxi64 r;` |
|      106 | 3498 | `						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){` |
|        - | 3499 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        3 | 3500 | `							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;` |
|        3 | 3501 | `							MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - | 3502 | `#else` |
|        - | 3503 | `							pObj->x.iVal = r;` |
|        - | 3504 | `#endif` |
|        2 | 3505 | `						}else{` |
|      104 | 3506 | `							pObj->x.iVal = r;` |
|        - | 3507 | `						}` |
|        - | 3508 | `					}` |
|      116 | 3509 | `					if( pInstr->iP1 ){` |
|        - | 3510 | `						/* Pre-decrement: result is the new value. */` |
|        3 | 3511 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 | 3512 | `					}` |
|        - | 3513 | `					/* Post-decrement: pTos retains the old value. */` |
|      116 | 3514 | `					PH7_NATIVE_SET_AFTER_MUTATE(pTos->nIdx,pObj)   /* see OP_INCR */` |
|        - | 3515 | `				}` |
|       57 | 3516 | `			}` |
|       59 | 3517 | `		}else{` |
|        3 | 3518 | `			if( pInstr->iP1 ){` |
|        3 | 3519 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - | 3520 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 | 3521 | `				}else{` |
|        - | 3522 | `					/* Force a numeric cast */` |
|        3 | 3523 | `					PH7_MemObjToNumeric(pTos);` |
|        - | 3524 | `					/* Pre-decrement */` |
|        3 | 3525 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3526 | `						pTos->rVal--;` |
|        - | 3527 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 | 3528 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 | 3529 | `					}else{` |
|        - | 3530 | `						/* PHP promotes PHP_INT_MIN-- to float; the integer-only` |
|        - | 3531 | `						 * build wraps like OP_POW's OMIT path. */` |
|        - | 3532 | `						sxi64 r;` |
|        3 | 3533 | `						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){` |
|        - | 3534 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      ! 0 | 3535 | `							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;` |
|      ! 0 | 3536 | `							MemObjSetType(pTos,MEMOBJ_REAL);` |
|        - | 3537 | `#else` |
|        - | 3538 | `							pTos->x.iVal = r;` |
|        - | 3539 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3540 | `#endif` |
|      ! 0 | 3541 | `						}else{` |
|        3 | 3542 | `							pTos->x.iVal = r;` |
|        3 | 3543 | `							MemObjSetType(pTos,MEMOBJ_INT);` |
|        - | 3544 | `						}` |
|        - | 3545 | `					}` |
|        - | 3546 | `				}` |
|        1 | 3547 | `			}` |
|        - | 3548 | `		}` |
|       58 | 3549 | `	}` |
|      130 | 3550 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);` |
|      130 | 3551 | `	break;` |
|        - | 3552 | `/*` |
|        - | 3553 | ` * UMINUS: * * *` |
|        - | 3554 | ` *` |
|        - | 3555 | ` * Perform a unary minus operation.` |
|        - | 3556 | ` */` |
|    47518 | 3557 | `case PH7_OP_UMINUS:` |
|        - | 3558 | `#ifdef UNTRUST` |
|        - | 3559 | `	if( pTos < pStack ){` |
|        - | 3560 | `		goto Abort;` |
|        - | 3561 | `	}` |
|        - | 3562 | `#endif` |
|        - | 3563 | ``	/* php's `$x * -1` operand contract: an array, object, resource or`` |
|        - | 3564 | `	 * non-numeric string is a TypeError, not a warned int(-1). */` |
|    95043 | 3565 | `	PH7_UNARY_ARITH_CONTRACT(-1)` |
|        - | 3566 | `	/* Force a numeric (integer,real or both) cast */` |
|    95013 | 3567 | `	PH7_MemObjToNumeric(pTos);` |
|    95013 | 3568 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      315 | 3569 | `		pTos->rVal = -pTos->rVal;` |
|      155 | 3570 | `	}` |
|    95013 | 3571 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    94765 | 3572 | `		if( pTos->x.iVal == SMALLEST_INT64 ){` |
|        - | 3573 | `			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a` |
|        - | 3574 | `			 * REAL representation is already present it is the negated` |
|        - | 3575 | `			 * authoritative value, so just drop the now-stale cached int. The` |
|        - | 3576 | `			 * integer-only build has no float type, so it wraps (two's` |
|        - | 3577 | `			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */` |
|        - | 3578 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        7 | 3579 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 | 3580 | `				pTos->rVal = -(ph7_real)pTos->x.iVal;` |
|        7 | 3581 | `				MemObjSetType(pTos,MEMOBJ_REAL);` |
|        4 | 3582 | `			}else{` |
|      ! 0 | 3583 | `				pTos->iFlags &= ~MEMOBJ_INT;` |
|        - | 3584 | `			}` |
|        - | 3585 | `#else` |
|        - | 3586 | `			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);` |
|        - | 3587 | `#endif` |
|        4 | 3588 | `		}else{` |
|    94759 | 3589 | `			pTos->x.iVal = -pTos->x.iVal;` |
|        - | 3590 | `		}` |
|    47380 | 3591 | `	}` |
|    95013 | 3592 | `	break;` |
|        - | 3593 | `/*` |
|        - | 3594 | ` * UPLUS: * * *` |
|        - | 3595 | ` *` |
|        - | 3596 | ` * Perform a unary plus operation.` |
|        - | 3597 | ` */` |
|       24 | 3598 | `case PH7_OP_UPLUS:` |
|        - | 3599 | `#ifdef UNTRUST` |
|        - | 3600 | `	if( pTos < pStack ){` |
|        - | 3601 | `		goto Abort;` |
|        - | 3602 | `	}` |
|        - | 3603 | `#endif` |
|        - | 3604 | ``	/* Unary plus is php's `$x * 1`, so it answers the same TypeError as unary`` |
|        - | 3605 | ``	 * minus -- naming `int` as the other operand, exactly as php does. */`` |
|       49 | 3606 | `	PH7_UNARY_ARITH_CONTRACT(1)` |
|        - | 3607 | `	/* Force a numeric (integer,real or both) cast */` |
|       41 | 3608 | `	PH7_MemObjToNumeric(pTos);` |
|       41 | 3609 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 3610 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 | 3611 | `	}` |
|       41 | 3612 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       41 | 3613 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       20 | 3614 | `	}` |
|       41 | 3615 | `	break;` |
|        - | 3616 | `/*` |
|        - | 3617 | ` * OP_LNOT: * * *` |
|        - | 3618 | ` *` |
|        - | 3619 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - | 3620 | ` * with its complement.` |
|        - | 3621 | ` */` |
|    49522 | 3622 | `case PH7_OP_LNOT:` |
|        - | 3623 | `#ifdef UNTRUST` |
|        - | 3624 | `	if( pTos < pStack ){` |
|        - | 3625 | `		goto Abort;` |
|        - | 3626 | `	}` |
|        - | 3627 | `#endif` |
|        - | 3628 | `	/* Force a boolean cast */` |
|    99396 | 3629 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      443 | 3630 | `		PH7_MemObjToBool(pTos);` |
|      219 | 3631 | `	}` |
|    99396 | 3632 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    99396 | 3633 | `	break;` |
|        - | 3634 | `/*` |
|        - | 3635 | ` * OP_BITNOT: * * *` |
|        - | 3636 | ` *` |
|        - | 3637 | ` * Interpret the top of the stack as an value.Replace it` |
|        - | 3638 | ` * with its ones-complement.` |
|        - | 3639 | ` */` |
|      310 | 3640 | `case PH7_OP_BITNOT:` |
|        - | 3641 | `#ifdef UNTRUST` |
|        - | 3642 | `	if( pTos < pStack ){` |
|        - | 3643 | `		goto Abort;` |
|        - | 3644 | `	}` |
|        - | 3645 | `#endif` |
|      625 | 3646 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - | 3647 | ``		/* php's `~` over a STRING is a per-BYTE ones-complement of the same`` |
|        - | 3648 | ``		 * length — a binary string, not an integer operation, so `~"abc"` is`` |
|        - | 3649 | `		 * "\x9e\x9d\x9c" where PHL cast the string to an int and answered` |
|        - | 3650 | `		 * int(-1). The operand's blob can be a READ-ONLY VIEW of the variable's` |
|        - | 3651 | `		 * own buffer (PH7_MemObjLoad), so complement into a fresh blob and swap` |
|        - | 3652 | `		 * it in rather than writing through the view. */` |
|        - | 3653 | `		SyBlob sNotBuf;` |
|       17 | 3654 | `		const unsigned char *zNotIn = (const unsigned char *)SyBlobData(&pTos->sBlob);` |
|       17 | 3655 | `		sxu32 nNotIn = SyBlobLength(&pTos->sBlob), iNot;` |
|       17 | 3656 | `		SyBlobInit(&sNotBuf,&pVm->sAllocator);` |
|       53 | 3657 | `		for( iNot = 0 ; iNot < nNotIn ; ++iNot ){` |
|       37 | 3658 | `			unsigned char cNot = (unsigned char)~zNotIn[iNot];` |
|       37 | 3659 | `			SyBlobAppend(&sNotBuf,(const void *)&cNot,sizeof(char));` |
|       19 | 3660 | `		}` |
|       17 | 3661 | `		PH7_MemObjRelease(pTos);` |
|       17 | 3662 | `		MemObjSetType(pTos,MEMOBJ_STRING);` |
|       17 | 3663 | `		if( SyBlobLength(&sNotBuf) > 0 ){` |
|       15 | 3664 | `			SyBlobAppend(&pTos->sBlob,SyBlobData(&sNotBuf),SyBlobLength(&sNotBuf));` |
|        7 | 3665 | `		}` |
|       17 | 3666 | `		SyBlobRelease(&sNotBuf);` |
|       17 | 3667 | `		break;` |
|        - | 3668 | `	}` |
|      609 | 3669 | `	if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        - | 3670 | `		/* Everything else — null, a bool, an array, an object, a resource — has` |
|        - | 3671 | `		 * no bitwise-not in php at all: a catchable TypeError naming the operand,` |
|        - | 3672 | `		 * where PHL cast it to an int and answered ~0 / ~1. */` |
|        - | 3673 | `		SyBlob sNotMsg;` |
|        - | 3674 | `		sxi32 rcNot;` |
|       27 | 3675 | `		SyBlobInit(&sNotMsg,&pVm->sAllocator);` |
|       27 | 3676 | `		VmBitNotTypeErrorMsg(pTos,&sNotMsg);` |
|       27 | 3677 | `		PH7_MemObjRelease(pTos);` |
|       27 | 3678 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       27 | 3679 | `		pTos->nIdx = SXU32_HIGH;` |
|       40 | 3680 | `		rcNot = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sNotMsg),` |
|       13 | 3681 | `			SyBlobLength(&sNotMsg));` |
|       27 | 3682 | `		SyBlobRelease(&sNotMsg);` |
|       27 | 3683 | `		if( rcNot == SXERR_ABORT ){ goto Abort; }` |
|       27 | 3684 | `		rc = rcNot;` |
|       29 | 3685 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3686 | `	}` |
|        - | 3687 | `	/* Force an integer cast (php deprecates a lossy float here too) */` |
|      583 | 3688 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      583 | 3689 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      583 | 3690 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 | 3691 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 | 3692 | `	}` |
|      583 | 3693 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        - | 3694 | `	/* An INTEGRAL float carries a cached MEMOBJ_INT beside MEMOBJ_REAL, so the` |
|        - | 3695 | `` 	 * cast above is skipped and the value kept rendering as a float: `~2.0` `` |
|        - | 3696 | ``	 * answered 2.0 instead of -3. `~` always yields an int. */`` |
|      583 | 3697 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      583 | 3698 | `	break;` |
|        - | 3699 | `/* OP_MUL * * *` |
|        - | 3700 | ` * OP_MUL_STORE * * *` |
|        - | 3701 | ` *` |
|        - | 3702 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - | 3703 | ` * and push the result back onto the stack.` |
|        - | 3704 | ` */` |
|     2445 | 3705 | `case PH7_OP_MUL:` |
|        - | 3706 | `case PH7_OP_MUL_STORE: {` |
|        - | 3707 | `	VmOpRc rcOp;` |
|        - | 3708 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     4895 | 3709 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     4893 | 3710 | `	sState.pTos = pTos;` |
|     4893 | 3711 | `	sState.pc = pc;` |
|     4893 | 3712 | `	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);` |
|     4893 | 3713 | `	pTos = sState.pTos;` |
|     4893 | 3714 | `	pc = sState.pc;` |
|     4893 | 3715 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3716 | `		goto Abort;` |
|     4893 | 3717 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      312 | 3718 | `		goto Exception;` |
|        - | 3719 | `	}` |
|     4583 | 3720 | `	break;` |
|        - | 3721 | `					  }` |
|        - | 3722 | `/* OP_POW * * *` |
|        - | 3723 | ` * OP_POW_STORE * * *` |
|        - | 3724 | ` *` |
|        - | 3725 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - | 3726 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - | 3727 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - | 3728 | ` * fits in sxi64; otherwise the result is a double.` |
|        - | 3729 | ` */` |
|      535 | 3730 | `case PH7_OP_POW:` |
|        - | 3731 | `case PH7_OP_POW_STORE: {` |
|        - | 3732 | `	VmOpRc rcOp;` |
|        - | 3733 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     1072 | 3734 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     1070 | 3735 | `	sState.pTos = pTos;` |
|     1070 | 3736 | `	sState.pc = pc;` |
|     1070 | 3737 | `	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);` |
|     1070 | 3738 | `	pTos = sState.pTos;` |
|     1070 | 3739 | `	pc = sState.pc;` |
|     1070 | 3740 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3741 | `		goto Abort;` |
|     1070 | 3742 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      318 | 3743 | `		goto Exception;` |
|        - | 3744 | `	}` |
|      753 | 3745 | `	break;` |
|        - | 3746 | `					  }` |
|        - | 3747 | `/* OP_ADD * * *` |
|        - | 3748 | ` *` |
|        - | 3749 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 3750 | ` * and push the result back onto the stack.` |
|        - | 3751 | ` */` |
|    16603 | 3752 | `case PH7_OP_ADD:{` |
|    33211 | 3753 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3754 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|        - | 3755 | `	int rcNa;` |
|    33211 | 3756 | `	const char *zArCls = "TypeError";` |
|        - | 3757 | `#ifdef UNTRUST` |
|        - | 3758 | `	if( pNos < pStack ){` |
|        - | 3759 | `		goto Abort;` |
|        - | 3760 | `	}` |
|        - | 3761 | `#endif` |
|        - | 3762 | `	{` |
|        - | 3763 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|        - | 3764 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|        - | 3765 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|        - | 3766 | `		SyBlob sArMsg;` |
|    33211 | 3767 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    33211 | 3768 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"+",pNos,&zArCls,&sArMsg);` |
|    33211 | 3769 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|        - | 3770 | `			sxi32 rcAr;` |
|      182 | 3771 | `			VmPopOperand(&pTos,1);` |
|      182 | 3772 | `			PH7_MemObjRelease(pTos);` |
|      182 | 3773 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      182 | 3774 | `			pTos->nIdx = SXU32_HIGH;` |
|      272 | 3775 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|       90 | 3776 | `				SyBlobLength(&sArMsg));` |
|      182 | 3777 | `			SyBlobRelease(&sArMsg);` |
|      182 | 3778 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      182 | 3779 | `			rc = rcAr;` |
|      182 | 3780 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3781 | `		}` |
|    33031 | 3782 | `		SyBlobRelease(&sArMsg);` |
|        - | 3783 | `	}` |
|        - | 3784 | `	/* Perform the addition (unless a do_operation handler already answered) */` |
|    33031 | 3785 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|    33017 | 3786 | `		PH7_MemObjAdd(pNos,pTos,FALSE);` |
|    16506 | 3787 | `	}` |
|    33031 | 3788 | `	VmPopOperand(&pTos,1);` |
|    33031 | 3789 | `	break;` |
|        - | 3790 | `				}` |
|        - | 3791 | `/*` |
|        - | 3792 | ` * OP_ADD_STORE * * *` |
|        - | 3793 | ` *` |
|        - | 3794 | ` * Pop the top two elements from the stack, add them together,` |
|        - | 3795 | ` * and push the result back onto the stack.` |
|        - | 3796 | ` */` |
|   214033 | 3797 | `case PH7_OP_ADD_STORE:{` |
|   428425 | 3798 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3799 | `	ph7_value *pObj;` |
|        - | 3800 | `	sxu32 nIdx;` |
|        - | 3801 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|        - | 3802 | `	int rcNa;` |
|   428425 | 3803 | `	const char *zArCls = "TypeError";` |
|        - | 3804 | `#ifdef UNTRUST` |
|        - | 3805 | `	if( pNos < pStack ){` |
|        - | 3806 | `		goto Abort;` |
|        - | 3807 | `	}` |
|        - | 3808 | `#endif` |
|        - | 3809 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|   642381 | 3810 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 3811 | `	{` |
|        - | 3812 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 3813 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 3814 | `		SyBlob sArMsg;` |
|   428419 | 3815 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   428419 | 3816 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"+",pTos,&zArCls,&sArMsg);` |
|   428419 | 3817 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|        - | 3818 | `			sxi32 rcAr;` |
|      149 | 3819 | `			VmPopOperand(&pTos,1);` |
|      149 | 3820 | `			PH7_MemObjRelease(pTos);` |
|      149 | 3821 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      149 | 3822 | `			pTos->nIdx = SXU32_HIGH;` |
|      223 | 3823 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|       74 | 3824 | `				SyBlobLength(&sArMsg));` |
|      149 | 3825 | `			SyBlobRelease(&sArMsg);` |
|      149 | 3826 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      149 | 3827 | `			rc = rcAr;` |
|      149 | 3828 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 3829 | `		}` |
|   428271 | 3830 | `		SyBlobRelease(&sArMsg);` |
|        - | 3831 | `	}` |
|        - | 3832 | `	/* Perform the addition */` |
|   428271 | 3833 | `	nIdx = pTos->nIdx;` |
|   428271 | 3834 | `	if( nIdx == pVm->nGlobalIdx ){` |
|        - | 3835 | `		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment` |
|        - | 3836 | `		 * (a compile-time fatal in php; raised here, same message). */` |
|        3 | 3837 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 3838 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 | 3839 | `		pVm->iExitStatus = 255;` |
|        3 | 3840 | `		pVm->bHaltRequested = 1;` |
|        3 | 3841 | `		goto Abort;` |
|        - | 3842 | `	}` |
|   428269 | 3843 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|   428267 | 3844 | `		PH7_MemObjAdd(pTos,pNos,TRUE);` |
|   214308 | 3845 | `	}` |
|        - | 3846 | `	/* Peform the store operation */` |
|   428269 | 3847 | `	if( nIdx == SXU32_HIGH ){` |
|        - | 3848 | ``		/* A read-modify-write THROUGH a temporary (`f()[0] += 5`): php computes it,`` |
|        - | 3849 | `		 * drops it and stays silent. See the OP_STORE member note above. */` |
|   428267 | 3850 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|   428265 | 3851 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|   428261 | 3852 | `		PH7_MemObjStore(pTos,pObj);` |
|   214305 | 3853 | `	}` |
|   428265 | 3854 | `	PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|        - | 3855 | `	/* Ticket 1433-35: Perform a stack dup */` |
|   428265 | 3856 | `	PH7_MemObjStore(pTos,pNos);` |
|   428265 | 3857 | `	VmPopOperand(&pTos,1);` |
|   428265 | 3858 | `	break;` |
|        - | 3859 | `				}` |
|        - | 3860 | `/* OP_SUB * * *` |
|        - | 3861 | ` *` |
|        - | 3862 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 3863 | ` * first (what was next on the stack) from the second (the` |
|        - | 3864 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 3865 | ` */` |
|    27066 | 3866 | `case PH7_OP_SUB: {` |
|        - | 3867 | `	VmOpRc rcOp;` |
|    54469 | 3868 | `	sState.pTos = pTos;` |
|    54469 | 3869 | `	sState.pc = pc;` |
|    54469 | 3870 | `	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);` |
|    54469 | 3871 | `	pTos = sState.pTos;` |
|    54469 | 3872 | `	pc = sState.pc;` |
|    54469 | 3873 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3874 | `		goto Abort;` |
|    54469 | 3875 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      162 | 3876 | `		goto Exception;` |
|        - | 3877 | `	}` |
|    54309 | 3878 | `	break;` |
|        - | 3879 | `					  }` |
|        - | 3880 | `/* OP_SUB_STORE * * *` |
|        - | 3881 | ` *` |
|        - | 3882 | ` * Pop the top two elements from the stack, subtract the` |
|        - | 3883 | ` * first (what was next on the stack) from the second (the` |
|        - | 3884 | ` * top of the stack) and push the result back onto the stack.` |
|        - | 3885 | ` */` |
|      205 | 3886 | `case PH7_OP_SUB_STORE: {` |
|        - | 3887 | `	VmOpRc rcOp;` |
|        - | 3888 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      412 | 3889 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      410 | 3890 | `	sState.pTos = pTos;` |
|      410 | 3891 | `	sState.pc = pc;` |
|      410 | 3892 | `	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);` |
|      410 | 3893 | `	pTos = sState.pTos;` |
|      410 | 3894 | `	pc = sState.pc;` |
|      410 | 3895 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3896 | `		goto Abort;` |
|      410 | 3897 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      153 | 3898 | `		goto Exception;` |
|        - | 3899 | `	}` |
|      258 | 3900 | `	break;` |
|        - | 3901 | `					  }` |
|        - | 3902 |  |
|        - | 3903 | `/*` |
|        - | 3904 | ` * OP_MOD * * *` |
|        - | 3905 | ` *` |
|        - | 3906 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3907 | ` * first (what was next on the stack) from the second (the` |
|        - | 3908 | ` * top of the stack) and push the remainder after division` |
|        - | 3909 | ` * onto the stack.` |
|        - | 3910 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 3911 | ` */` |
|     1085 | 3912 | `case PH7_OP_MOD: {` |
|        - | 3913 | `	VmOpRc rcOp;` |
|     2175 | 3914 | `	sState.pTos = pTos;` |
|     2175 | 3915 | `	sState.pc = pc;` |
|     2175 | 3916 | `	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);` |
|     2175 | 3917 | `	pTos = sState.pTos;` |
|     2175 | 3918 | `	pc = sState.pc;` |
|     2175 | 3919 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3920 | `		goto Abort;` |
|     2175 | 3921 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      248 | 3922 | `		goto Exception;` |
|        - | 3923 | `	}` |
|     1929 | 3924 | `	break;` |
|        - | 3925 | `					  }` |
|        - | 3926 | `/*` |
|        - | 3927 | ` * OP_MOD_STORE * * *` |
|        - | 3928 | ` *` |
|        - | 3929 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3930 | ` * first (what was next on the stack) from the second (the` |
|        - | 3931 | ` * top of the stack) and push the remainder after division` |
|        - | 3932 | ` * onto the stack.` |
|        - | 3933 | ` * Note: Only integer arithemtic is allowed.` |
|        - | 3934 | ` */` |
|      204 | 3935 | `case PH7_OP_MOD_STORE: {` |
|        - | 3936 | `	VmOpRc rcOp;` |
|        - | 3937 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      409 | 3938 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      407 | 3939 | `	sState.pTos = pTos;` |
|      407 | 3940 | `	sState.pc = pc;` |
|      407 | 3941 | `	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);` |
|      407 | 3942 | `	pTos = sState.pTos;` |
|      407 | 3943 | `	pc = sState.pc;` |
|      407 | 3944 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3945 | `		goto Abort;` |
|      407 | 3946 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      221 | 3947 | `		goto Exception;` |
|        - | 3948 | `	}` |
|      187 | 3949 | `	break;` |
|        - | 3950 | `					  }` |
|        - | 3951 | `/*` |
|        - | 3952 | ` * OP_DIV * * *` |
|        - | 3953 | ` *` |
|        - | 3954 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3955 | ` * first (what was next on the stack) from the second (the` |
|        - | 3956 | ` * top of the stack) and push the result onto the stack.` |
|        - | 3957 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 3958 | ` */` |
|      478 | 3959 | `case PH7_OP_DIV: {` |
|        - | 3960 | `	VmOpRc rcOp;` |
|      959 | 3961 | `	sState.pTos = pTos;` |
|      959 | 3962 | `	sState.pc = pc;` |
|      959 | 3963 | `	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);` |
|      959 | 3964 | `	pTos = sState.pTos;` |
|      959 | 3965 | `	pc = sState.pc;` |
|      959 | 3966 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 3967 | `		goto Abort;` |
|      959 | 3968 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      233 | 3969 | `		goto Exception;` |
|        - | 3970 | `	}` |
|      727 | 3971 | `	break;` |
|        - | 3972 | `					  }` |
|        - | 3973 | `/*` |
|        - | 3974 | ` * OP_DIV_STORE * * *` |
|        - | 3975 | ` *` |
|        - | 3976 | ` * Pop the top two elements from the stack, divide the` |
|        - | 3977 | ` * first (what was next on the stack) from the second (the` |
|        - | 3978 | ` * top of the stack) and push the result onto the stack.` |
|        - | 3979 | ` * Note: Only floating point arithemtic is allowed.` |
|        - | 3980 | ` */` |
|      211 | 3981 | `case PH7_OP_DIV_STORE:{` |
|      423 | 3982 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 3983 | `	ph7_value *pObj;` |
|        - | 3984 | `	ph7_real a,b,r;` |
|        - | 3985 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|        - | 3986 | `	int rcNa;` |
|      423 | 3987 | `	const char *zArCls = "TypeError";` |
|        - | 3988 | `#ifdef UNTRUST` |
|        - | 3989 | `	if( pNos < pStack ){` |
|        - | 3990 | `		goto Abort;` |
|        - | 3991 | `	}` |
|        - | 3992 | `#endif` |
|        - | 3993 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      524 | 3994 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 3995 | `	{` |
|        - | 3996 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|        - | 3997 | `		 * array, object or resource operand is a TypeError too. */` |
|        - | 3998 | `		SyBlob sArMsg;` |
|      421 | 3999 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|      421 | 4000 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"/",pNos,&zArCls,&sArMsg);` |
|      421 | 4001 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|        - | 4002 | `			sxi32 rcAr;` |
|      151 | 4003 | `			VmPopOperand(&pTos,1);` |
|      151 | 4004 | `			PH7_MemObjRelease(pTos);` |
|      151 | 4005 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      151 | 4006 | `			pTos->nIdx = SXU32_HIGH;` |
|      226 | 4007 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|       75 | 4008 | `				SyBlobLength(&sArMsg));` |
|      151 | 4009 | `			SyBlobRelease(&sArMsg);` |
|      151 | 4010 | `			if( rcAr == SXERR_ABORT ){ goto Abort; }` |
|      151 | 4011 | `			rc = rcAr;` |
|      151 | 4012 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 4013 | `		}` |
|      271 | 4014 | `		SyBlobRelease(&sArMsg);` |
|        - | 4015 | `	}` |
|      271 | 4016 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|        - | 4017 | ``		/* php's `/` answers an INT when both operands are ints and the division is`` |
|        - | 4018 | ``		 * exact (`6/3 === 2`, not `2.0`), and `$x /= $y` is that same operator: php`` |
|        - | 4019 | `		 * has one division and the compound form only decides where the answer goes.` |
|        - | 4020 | `		 * OP_DIV grew the rule (§2's int-boundary work) and this arm, a separate copy` |
|        - | 4021 | ``		 * of it, did not — so `$x = 6; $x /= 3;` left a FLOAT where `$x = $x / 3` left`` |
|        - | 4022 | ``		 * an int, visible through `===`, `var_dump`, `json_encode` and `is_int`.`` |
|        - | 4023 | ``		 * The divisor is screened BEFORE `ia % ib`: x86 computes the overflowing`` |
|        - | 4024 | `		 * PHP_INT_MIN/-1 quotient alongside the remainder and traps (OP_DIV and OP_MOD` |
|        - | 4025 | `		 * guard the same hazard the same way). */` |
|      269 | 4026 | `		int bExactDiv = 0;` |
|      269 | 4027 | `		PH7_MemObjToNumeric(pTos);` |
|      269 | 4028 | `		PH7_MemObjToNumeric(pNos);` |
|      269 | 4029 | `		if( ((pTos->iFlags\|pNos->iFlags) & MEMOBJ_REAL) == 0 ){` |
|      269 | 4030 | `			sxi64 ia = pTos->x.iVal;   /* the lvalue: php's dividend */` |
|      269 | 4031 | `			sxi64 ib = pNos->x.iVal;   /* the right operand: the divisor */` |
|      269 | 4032 | `			sxi64 iQuot = 0;` |
|      269 | 4033 | `			if( ib == 0 ){` |
|       71 | 4034 | `				rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|       75 | 4035 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      199 | 4036 | `			}else if( ib == -1 ){` |
|        - | 4037 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - | 4038 | `				iQuot = ( ia != SMALLEST_INT64 ) ? -ia : SMALLEST_INT64;` |
|        - | 4039 | `				bExactDiv = 1;` |
|        - | 4040 | `#else` |
|        7 | 4041 | `				if( ia != SMALLEST_INT64 ){` |
|        3 | 4042 | `					iQuot = -ia;` |
|        3 | 4043 | `					bExactDiv = 1;` |
|        2 | 4044 | `				}` |
|        - | 4045 | `#endif` |
|      196 | 4046 | `			}else if( ia % ib == 0 ){` |
|      109 | 4047 | `				iQuot = ia / ib;` |
|      109 | 4048 | `				bExactDiv = 1;` |
|       54 | 4049 | `			}` |
|      199 | 4050 | `			if( bExactDiv ){` |
|      111 | 4051 | `				pNos->x.iVal = iQuot;` |
|      111 | 4052 | `				MemObjSetType(pNos,MEMOBJ_INT);` |
|       55 | 4053 | `			}` |
|       99 | 4054 | `		}` |
|      199 | 4055 | `		if( !bExactDiv ){` |
|        - | 4056 | `			/* Force the operands to be real */` |
|       89 | 4057 | `			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       89 | 4058 | `				PH7_MemObjToReal(pTos);` |
|       44 | 4059 | `			}` |
|       89 | 4060 | `			if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       89 | 4061 | `				PH7_MemObjToReal(pNos);` |
|       44 | 4062 | `			}` |
|        - | 4063 | `			/* Perform the requested operation */` |
|       89 | 4064 | `			a = pTos->rVal;` |
|       89 | 4065 | `			b = pNos->rVal;` |
|       89 | 4066 | `			if( b == 0 ){` |
|        - | 4067 | `				/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|        - | 4068 | `				 * not the old non-catchable warning that continued with a 0 result. */` |
|      ! 0 | 4069 | `				rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|      ! 0 | 4070 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      ! 0 | 4071 | `			}else{` |
|       89 | 4072 | `				r = a/b;` |
|        - | 4073 | `				/* Push the result */` |
|       89 | 4074 | `				pNos->rVal = r;` |
|       89 | 4075 | `				MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - | 4076 | `			}` |
|       44 | 4077 | `		}` |
|       99 | 4078 | `	}` |
|      201 | 4079 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|        - | 4080 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|      201 | 4081 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      201 | 4082 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|      201 | 4083 | `		PH7_MemObjStore(pNos,pObj);` |
|      100 | 4084 | `	}` |
|      201 | 4085 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|      201 | 4086 | `	VmPopOperand(&pTos,1);` |
|      201 | 4087 | `	break;` |
|        - | 4088 | `				}` |
|        - | 4089 | `/* OP_BAND * * *` |
|        - | 4090 | ` *` |
|        - | 4091 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4092 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 4093 | ` * two elements.` |
|        - | 4094 | `*/` |
|        - | 4095 | `/* OP_BOR * * *` |
|        - | 4096 | ` *` |
|        - | 4097 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4098 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 4099 | ` * two elements.` |
|        - | 4100 | ` */` |
|        - | 4101 | `/* OP_BXOR * * *` |
|        - | 4102 | ` *` |
|        - | 4103 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4104 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 4105 | ` * two elements.` |
|        - | 4106 | ` */` |
|     2579 | 4107 | `case PH7_OP_BAND:` |
|        - | 4108 | `case PH7_OP_BOR:` |
|        - | 4109 | `case PH7_OP_BXOR:{` |
|     5161 | 4110 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4111 | `	sxi64 a,b,r;` |
|        - | 4112 | `	int cBwOp;` |
|        - | 4113 | `#ifdef UNTRUST` |
|        - | 4114 | `	if( pNos < pStack ){` |
|        - | 4115 | `		goto Abort;` |
|        - | 4116 | `	}` |
|        - | 4117 | `#endif` |
|     5161 | 4118 | `	cBwOp = pInstr->iOp == PH7_OP_BOR ? '\|' : (pInstr->iOp == PH7_OP_BXOR ? '^' : '&');` |
|     5161 | 4119 | `	if( (pNos->iFlags & MEMOBJ_STRING) && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 4120 | `		/* TWO strings: php's per-byte string operation, result a string. */` |
|      149 | 4121 | `		PH7_STRING_BITWISE_RESULT(pNos,pNos,pTos,cBwOp)` |
|      149 | 4122 | `		VmPopOperand(&pTos,1);` |
|      149 | 4123 | `		break;` |
|        - | 4124 | `	}` |
|        - | 4125 | `	{` |
|        - | 4126 | `		char zBwOp[2];` |
|     5013 | 4127 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 4128 | `		/* Anything else is php's arithmetic operand contract, named for this` |
|        - | 4129 | ``		 * operator (`array & int`), not a silent integer cast. */`` |
|     5015 | 4130 | `		PH7_BITWISE_ARITH_CONTRACT(pNos,pTos,zBwOp,1)` |
|        - | 4131 | `	}` |
|        - | 4132 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     4589 | 4133 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     4589 | 4134 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     4583 | 4135 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     4583 | 4136 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     4583 | 4137 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      321 | 4138 | `		PH7_MemObjToInteger(pTos);` |
|      160 | 4139 | `	}` |
|     4583 | 4140 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      323 | 4141 | `		PH7_MemObjToInteger(pNos);` |
|      161 | 4142 | `	}` |
|        - | 4143 | `	/* Perform the requested operation */` |
|     4583 | 4144 | `	a = pNos->x.iVal;` |
|     4583 | 4145 | `	b = pTos->x.iVal;` |
|     4583 | 4146 | `	switch(pInstr->iOp){` |
|      470 | 4147 | `	case PH7_OP_BOR_STORE:` |
|      945 | 4148 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|      214 | 4149 | `	case PH7_OP_BXOR_STORE:` |
|      429 | 4150 | `	case PH7_OP_BXOR: r = a^b; break;` |
|     1605 | 4151 | `	case PH7_OP_BAND_STORE:` |
|     1605 | 4152 | `	case PH7_OP_BAND:` |
|     3215 | 4153 | `	default:          r = a&b; break;` |
|        - | 4154 | `	}` |
|        - | 4155 | `	/* Push the result */` |
|     4583 | 4156 | `	pNos->x.iVal = r;` |
|     4583 | 4157 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     4583 | 4158 | `	VmPopOperand(&pTos,1);` |
|     4583 | 4159 | `	break;` |
|        - | 4160 | `				 }` |
|        - | 4161 | `/* OP_BAND_STORE * * *` |
|        - | 4162 | ` *` |
|        - | 4163 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4164 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - | 4165 | ` * two elements.` |
|        - | 4166 | `*/` |
|        - | 4167 | `/* OP_BOR_STORE * * *` |
|        - | 4168 | ` *` |
|        - | 4169 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4170 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - | 4171 | ` * two elements.` |
|        - | 4172 | ` */` |
|        - | 4173 | `/* OP_BXOR_STORE * * *` |
|        - | 4174 | ` *` |
|        - | 4175 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4176 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - | 4177 | ` * two elements.` |
|        - | 4178 | ` */` |
|      640 | 4179 | `case PH7_OP_BAND_STORE:` |
|        - | 4180 | `case PH7_OP_BOR_STORE:` |
|        - | 4181 | `case PH7_OP_BXOR_STORE:{` |
|     1281 | 4182 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4183 | `	ph7_value *pObj;` |
|        - | 4184 | `	sxi64 a,b,r;` |
|        - | 4185 | `	int cBwOp,bBwStr;` |
|        - | 4186 | `#ifdef UNTRUST` |
|        - | 4187 | `	if( pNos < pStack ){` |
|        - | 4188 | `		goto Abort;` |
|        - | 4189 | `	}` |
|        - | 4190 | `#endif` |
|        - | 4191 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|     1281 | 4192 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|     1275 | 4193 | `	cBwOp = pInstr->iOp == PH7_OP_BOR_STORE ? '\|' : (pInstr->iOp == PH7_OP_BXOR_STORE ? '^' : '&');` |
|     1275 | 4194 | `	bBwStr = (pNos->iFlags & MEMOBJ_STRING) != 0 && (pTos->iFlags & MEMOBJ_STRING) != 0;` |
|     1275 | 4195 | `	if( !bBwStr ){` |
|        - | 4196 | `		char zBwOp[2];` |
|     1173 | 4197 | `		zBwOp[0] = (char)cBwOp; zBwOp[1] = 0;` |
|        - | 4198 | ``		/* `$x &= v` answers the same contract as `$x & v` (php's compound`` |
|        - | 4199 | `		 * assignment is the operator plus a store), but through its own error` |
|        - | 4200 | `		 * path, which is POSITIONAL: the lvalue is named first even when the` |
|        - | 4201 | ``		 * right operand is the offender (`$x = 1; $x &= $o` is "int & BsocP",`` |
|        - | 4202 | ``		 * where the plain `1 & $o` is "BsocP & int"). */`` |
|     1173 | 4203 | `		PH7_BITWISE_ARITH_CONTRACT(pTos,pNos,zBwOp,0)` |
|        - | 4204 | `		/* Force the operands to be integer (php deprecates a lossy float here) */` |
|      791 | 4205 | `		rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|      791 | 4206 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      791 | 4207 | `		rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      791 | 4208 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|      791 | 4209 | `		if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      309 | 4210 | `			PH7_MemObjToInteger(pTos);` |
|      154 | 4211 | `		}` |
|      791 | 4212 | `		if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      309 | 4213 | `			PH7_MemObjToInteger(pNos);` |
|      154 | 4214 | `		}` |
|      395 | 4215 | `	}` |
|      893 | 4216 | `	if( bBwStr ){` |
|        - | 4217 | `		/* TWO strings: php's per-byte string operation, result a string. The` |
|        - | 4218 | `		 * result lands in pNos, which the store tail below writes into the` |
|        - | 4219 | `		 * lvalue's slot exactly like the integer result. */` |
|      103 | 4220 | `		PH7_STRING_BITWISE_RESULT(pNos,pTos,pNos,cBwOp)` |
|       52 | 4221 | `	}else{` |
|        - | 4222 | `	/* Perform the requested operation */` |
|      791 | 4223 | `	a = pTos->x.iVal;` |
|      791 | 4224 | `	b = pNos->x.iVal;` |
|      791 | 4225 | `	switch(pInstr->iOp){` |
|      151 | 4226 | `	case PH7_OP_BOR_STORE:` |
|      303 | 4227 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|      122 | 4228 | `	case PH7_OP_BXOR_STORE:` |
|      245 | 4229 | `	case PH7_OP_BXOR: r = a^b; break;` |
|      122 | 4230 | `	case PH7_OP_BAND_STORE:` |
|      122 | 4231 | `	case PH7_OP_BAND:` |
|      245 | 4232 | `	default:          r = a&b; break;` |
|        - | 4233 | `	}` |
|        - | 4234 | `	/* Push the result */` |
|      791 | 4235 | `	pNos->x.iVal = r;` |
|      791 | 4236 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        - | 4237 | `	}` |
|      893 | 4238 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|        - | 4239 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|      892 | 4240 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      891 | 4241 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|      891 | 4242 | `		PH7_MemObjStore(pNos,pObj);` |
|      445 | 4243 | `	}` |
|      893 | 4244 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|      893 | 4245 | `	VmPopOperand(&pTos,1);` |
|      893 | 4246 | `	break;` |
|        - | 4247 | `				 }` |
|        - | 4248 | `/* OP_SHL * * *` |
|        - | 4249 | ` *` |
|        - | 4250 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4251 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4252 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4253 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4254 | ` */` |
|        - | 4255 | `/* OP_SHR * * *` |
|        - | 4256 | ` *` |
|        - | 4257 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4258 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4259 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4260 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4261 | ` */` |
|      465 | 4262 | `case PH7_OP_SHL:` |
|        - | 4263 | `case PH7_OP_SHR: {` |
|        - | 4264 | `	VmOpRc rcOp;` |
|      933 | 4265 | `	sState.pTos = pTos;` |
|      933 | 4266 | `	sState.pc = pc;` |
|      933 | 4267 | `	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);` |
|      933 | 4268 | `	pTos = sState.pTos;` |
|      933 | 4269 | `	pc = sState.pc;` |
|      933 | 4270 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4271 | `		goto Abort;` |
|      933 | 4272 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      394 | 4273 | `		goto Exception;` |
|        - | 4274 | `	}` |
|      540 | 4275 | `	break;` |
|        - | 4276 | `					  }` |
|        - | 4277 | `/*  OP_SHL_STORE * * *` |
|        - | 4278 | ` *` |
|        - | 4279 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4280 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4281 | ` * left by N bits where N is the top element on the stack.` |
|        - | 4282 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4283 | ` */` |
|        - | 4284 | `/* OP_SHR_STORE * * *` |
|        - | 4285 | ` *` |
|        - | 4286 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - | 4287 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - | 4288 | ` * right by N bits where N is the top element on the stack.` |
|        - | 4289 | ` * Note: Only integer arithmetic is allowed.` |
|        - | 4290 | ` */` |
|      414 | 4291 | `case PH7_OP_SHL_STORE:` |
|        - | 4292 | `case PH7_OP_SHR_STORE: {` |
|        - | 4293 | `	VmOpRc rcOp;` |
|        - | 4294 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|      829 | 4295 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|      825 | 4296 | `	sState.pTos = pTos;` |
|      825 | 4297 | `	sState.pc = pc;` |
|      825 | 4298 | `	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);` |
|      825 | 4299 | `	pTos = sState.pTos;` |
|      825 | 4300 | `	pc = sState.pc;` |
|      825 | 4301 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4302 | `		goto Abort;` |
|      825 | 4303 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      353 | 4304 | `		goto Exception;` |
|        - | 4305 | `	}` |
|      473 | 4306 | `	break;` |
|        - | 4307 | `					  }` |
|        - | 4308 | `/* CAT:  P1 * *` |
|        - | 4309 | ` *` |
|        - | 4310 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - | 4311 | ` * back.` |
|        - | 4312 | ` */` |
|   145565 | 4313 | `case PH7_OP_CAT:{` |
|        - | 4314 | `	ph7_value *pNos,*pCur;` |
|   291486 | 4315 | `	if( pInstr->iP1 < 1 ){` |
|   243255 | 4316 | `		pNos = &pTos[-1];` |
|   121807 | 4317 | `	}else{` |
|    48236 | 4318 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - | 4319 | `	}` |
|        - | 4320 | `#ifdef UNTRUST` |
|        - | 4321 | `	if( pNos < pStack ){` |
|        - | 4322 | `		goto Abort;` |
|        - | 4323 | `	}` |
|        - | 4324 | `#endif` |
|        - | 4325 | `	/* Force a string cast (user-visible: warns on an array operand, §2).` |
|        - | 4326 | `	 * php coerces the operands left to right, so the leftmost not-stringable` |
|        - | 4327 | `	 * object is the one that throws. */` |
|        - | 4328 | `	{` |
|   291486 | 4329 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|   291486 | 4330 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4331 | `	}` |
|   291484 | 4332 | `	pCur = &pNos[1];` |
|        - | 4333 | `	{` |
|        - | 4334 | `		/* rcSv leaves the loop with the coercion status: the routing macro expands` |
|        - | 4335 | ``		 * to a bare `break` here, which inside the while would fall THROUGH to the`` |
|        - | 4336 | ``		 * `pTos = pNos` below with a throw pending. Route it at case level. */`` |
|   291484 | 4337 | `		sxi32 rcSv = SXRET_OK;` |
|   647244 | 4338 | `		while( pCur <= pTos ){` |
|   356209 | 4339 | `			rcSv = PH7_MemObjToStringUV(pCur);` |
|   356209 | 4340 | `			if( rcSv != SXRET_OK ){` |
|      448 | 4341 | `				break;` |
|        - | 4342 | `			}` |
|        - | 4343 | `			/* Perform the concatenation */` |
|   355765 | 4344 | `			if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   355075 | 4345 | `				if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - | 4346 | `					/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 | 4347 | `					PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4348 | `					goto Abort;` |
|        - | 4349 | `				}` |
|   177703 | 4350 | `			}` |
|   355765 | 4351 | `			SyBlobRelease(&pCur->sBlob);` |
|   355765 | 4352 | `			pCur++;` |
|        5 | 4353 | `		}` |
|   292306 | 4354 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4355 | `	}` |
|   291040 | 4356 | `	pTos = pNos;` |
|   291040 | 4357 | `	break;` |
|        - | 4358 | `				}` |
|        - | 4359 | `/*  CAT_STORE: * * *` |
|        - | 4360 | ` *` |
|        - | 4361 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - | 4362 | ` * back.` |
|        - | 4363 | ` */` |
|    21771 | 4364 | `case PH7_OP_CAT_STORE:{` |
|    43547 | 4365 | `	ph7_value *pNos = &pTos[-1];` |
|        - | 4366 | `	ph7_value *pObj;` |
|        - | 4367 | `	sxu32 nIdx;` |
|        - | 4368 | `#ifdef UNTRUST` |
|        - | 4369 | `	if( pNos < pStack ){` |
|        - | 4370 | `		goto Abort;` |
|        - | 4371 | `	}` |
|        - | 4372 | `#endif` |
|        - | 4373 | `	/* php refuses a compound assignment THROUGH a string offset. */` |
|    65309 | 4374 | `	PH7_REJECT_STROFFSET_ASSIGNOP()` |
|        - | 4375 | `	/* The right operand must be a string to append it (user-visible, §2) */` |
|        - | 4376 | `	{` |
|    43539 | 4377 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNos);` |
|    43543 | 4378 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4379 | `	}` |
|    43529 | 4380 | `	nIdx = pTos->nIdx;` |
|        - | 4381 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - | 4382 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - | 4383 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - | 4384 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - | 4385 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - | 4386 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - | 4387 | `	 * the source we copy from — references share the slot index, so one check` |
|        - | 4388 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - | 4389 | `	 * must run before any mutation (left to the slow path).` |
|        - | 4390 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - | 4391 | `	 * and remains O(n^2) by design. */` |
|    43524 | 4392 | `	if( nIdx != SXU32_HIGH` |
|    43522 | 4393 | `	 && nIdx != pNos->nIdx` |
|    43516 | 4394 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|    43517 | 4395 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|    22268 | 4396 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|        - | 4397 | `		/* e.g. $x = 5; $x .= "a";  ->  "5a" (user-visible: warns if $x is an array,` |
|        - | 4398 | `		 * throws if it is a not-stringable object — and then the lvalue keeps` |
|        - | 4399 | `		 * holding that object, since the throw abandons the coercion) */` |
|        - | 4400 | `		{` |
|    43507 | 4401 | `			sxi32 rcSv = PH7_MemObjToStringUV(pObj);` |
|    43515 | 4402 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4403 | `		}` |
|    43501 | 4404 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|    43359 | 4405 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4406 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - | 4407 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 | 4408 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4409 | `				goto Abort;` |
|        - | 4410 | `			}` |
|    21677 | 4411 | `		}` |
|        - | 4412 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - | 4413 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - | 4414 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - | 4415 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - | 4416 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - | 4417 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - | 4418 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - | 4419 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - | 4420 | `		 * the same slot is appended to again later in the statement` |
|        - | 4421 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - | 4422 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - | 4423 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|    43501 | 4424 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 | 4425 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 | 4426 | `		}` |
|        - | 4427 | `		/* A hooked lvalue's scratch slot was appended in place — dispatch the` |
|        - | 4428 | `		 * set side now (the consume reads the computed value from the slot). */` |
|    43501 | 4429 | `		PH7_HOOK_RMW_WRITEBACK(nIdx,0);` |
|    43501 | 4430 | `		pNos->nIdx = SXU32_HIGH;` |
|    43501 | 4431 | `		VmPopOperand(&pTos,1);` |
|    43501 | 4432 | `		break;` |
|        - | 4433 | `	}` |
|        - | 4434 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|        - | 4435 | `	/* Force a string cast (user-visible: warns if the lvalue is an array, §2) */` |
|        - | 4436 | `	{` |
|       24 | 4437 | `		sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|       24 | 4438 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|        - | 4439 | `	}` |
|        - | 4440 | `	/* Perform the concatenation (Reverse order) */` |
|       24 | 4441 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       24 | 4442 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - | 4443 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - | 4444 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 | 4445 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 4446 | `			goto Abort;` |
|        - | 4447 | `		}` |
|       11 | 4448 | `	}` |
|        - | 4449 | `	/* Perform the store operation */` |
|       24 | 4450 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|        - | 4451 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|       22 | 4452 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       28 | 4453 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       15 | 4454 | `		PH7_MemObjStore(pTos,pObj);` |
|        7 | 4455 | `	}` |
|       19 | 4456 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|       19 | 4457 | `	PH7_MemObjStore(pTos,pNos);` |
|       19 | 4458 | `	VmPopOperand(&pTos,1);` |
|       19 | 4459 | `	break;` |
|        - | 4460 | `				}` |
|        - | 4461 | `/* OP_AND: * * *` |
|        - | 4462 | ` *` |
|        - | 4463 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - | 4464 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4465 | ` * stack.` |
|        - | 4466 | ` */` |
|        - | 4467 | `/* OP_OR: * * *` |
|        - | 4468 | ` *` |
|        - | 4469 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - | 4470 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4471 | ` * stack.` |
|        - | 4472 | ` */` |
|   177606 | 4473 | `case PH7_OP_LAND:` |
|        - | 4474 | `case PH7_OP_LOR: {` |
|        - | 4475 | `	VmOpRc rcOp;` |
|   355655 | 4476 | `	sState.pTos = pTos;` |
|   355655 | 4477 | `	sState.pc = pc;` |
|   355655 | 4478 | `	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);` |
|   355655 | 4479 | `	pTos = sState.pTos;` |
|   355655 | 4480 | `	pc = sState.pc;` |
|   355655 | 4481 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4482 | `		goto Abort;` |
|   355655 | 4483 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4484 | `		goto Exception;` |
|        - | 4485 | `	}` |
|   355655 | 4486 | `	break;` |
|        - | 4487 | `					  }` |
|        - | 4488 | `/*` |
|        - | 4489 | ` * OP_NULLC: * * *` |
|        - | 4490 | ` * Null coalescing operator '??'.` |
|        - | 4491 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - | 4492 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - | 4493 | ` */` |
|        - | 4494 | `/*` |
|        - | 4495 | ` * OP_NULLC: * P2 *` |
|        - | 4496 | ` * Short-circuit null coalescing '??'.` |
|        - | 4497 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - | 4498 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - | 4499 | ` */` |
|      687 | 4500 | `case PH7_OP_NULLC: {` |
|        - | 4501 | `#ifdef UNTRUST` |
|        - | 4502 | `	if( pTos < pStack ){` |
|        - | 4503 | `		goto Abort;` |
|        - | 4504 | `	}` |
|        - | 4505 | `#endif` |
|     1379 | 4506 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 4507 | `		/* Left is not null — keep it and skip the RHS */` |
|     1053 | 4508 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|      529 | 4509 | `	}else{` |
|        - | 4510 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|      331 | 4511 | `		VmPopOperand(&pTos, 1);` |
|        - | 4512 | `	}` |
|     1379 | 4513 | `	break;` |
|        - | 4514 | `}` |
|        - | 4515 | `/*` |
|        - | 4516 | ` * OP_NULLC_JMP: * P2 *` |
|        - | 4517 | ` * Null coalescing assignment short-circuit.` |
|        - | 4518 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - | 4519 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - | 4520 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - | 4521 | ` */` |
|       86 | 4522 | `case PH7_OP_NULLC_JMP: {` |
|        - | 4523 | `#ifdef UNTRUST` |
|        - | 4524 | `	if( pTos < pStack ){` |
|        - | 4525 | `		goto Abort;` |
|        - | 4526 | `	}` |
|        - | 4527 | `#endif` |
|      175 | 4528 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       56 | 4529 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —` |
|        - | 4530 | `		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is` |
|        - | 4531 | `		 * dropped by the fetch-point sweep (the landing pc is past its` |
|        - | 4532 | `		 * OP_NULLC_STORE window): the skipped assign never dispatches. */` |
|       27 | 4533 | `	}` |
|      175 | 4534 | `	break;` |
|        - | 4535 | `}` |
|        - | 4536 | `/*` |
|        - | 4537 | ` * OP_NULLC_STORE: * * *` |
|        - | 4538 | ` * Null coalescing assignment store.` |
|        - | 4539 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - | 4540 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - | 4541 | ` * expression result.` |
|        - | 4542 | ` */` |
|        - | 4543 | `/*` |
|        - | 4544 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - | 4545 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - | 4546 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - | 4547 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - | 4548 | ` * non-null, fall through without modifying the stack so the following` |
|        - | 4549 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - | 4550 | ` */` |
|       90 | 4551 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - | 4552 | `#ifdef UNTRUST` |
|        - | 4553 | `	if( pTos < pStack ){` |
|        - | 4554 | `		goto Abort;` |
|        - | 4555 | `	}` |
|        - | 4556 | `#endif` |
|      185 | 4557 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - | 4558 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - | 4559 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       70 | 4560 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       34 | 4561 | `	}` |
|      185 | 4562 | `	break;` |
|        - | 4563 | `}` |
|       56 | 4564 | `case PH7_OP_NULLC_STORE: {` |
|        - | 4565 | `	VmOpRc rcOp;` |
|      115 | 4566 | `	sState.pTos = pTos;` |
|      115 | 4567 | `	sState.pc = pc;` |
|      115 | 4568 | `	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);` |
|      115 | 4569 | `	pTos = sState.pTos;` |
|      115 | 4570 | `	pc = sState.pc;` |
|      115 | 4571 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4572 | `		goto Abort;` |
|      115 | 4573 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       11 | 4574 | `		goto Exception;` |
|        - | 4575 | `	}` |
|      105 | 4576 | `	break;` |
|        - | 4577 | `					  }` |
|        - | 4578 | `/*` |
|        - | 4579 | ` * OP_SPREAD: * * *` |
|        - | 4580 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - | 4581 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - | 4582 | ` * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this` |
|        - | 4583 | ` * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —` |
|        - | 4584 | ` * the CALL may not be the next instruction, and may be an inner call whose own` |
|        - | 4585 | ` * spreads must stay scoped to it.` |
|        - | 4586 | ` * The expansion tail is shared between the plain-array and the materialized` |
|        - | 4587 | ` * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.` |
|        - | 4588 | ` */` |
|      567 | 4589 | `case PH7_OP_SPREAD: {` |
|        - | 4590 | `#ifdef UNTRUST` |
|        - | 4591 | `	if( pTos < pStack ){` |
|        - | 4592 | `		goto Abort;` |
|        - | 4593 | `	}` |
|        - | 4594 | `#endif` |
|        - | 4595 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - | 4596 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - | 4597 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - | 4598 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - | 4599 | `	 * be freed immediately. */` |
|     1138 | 4600 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 | 4601 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - | 4602 | `		sxi32 rcW;` |
|        3 | 4603 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 | 4604 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 | 4605 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 | 4606 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 4607 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 | 4608 | `			goto Exception;` |
|        - | 4609 | `		}` |
|        - | 4610 | `		/* Grow the operand stack if this expansion would overflow it (no longer a` |
|        - | 4611 | `		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a` |
|        - | 4612 | `		 * single argument — the historical bounded-fallback, now only under OOM. */` |
|        4 | 4613 | `		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,` |
|        1 | 4614 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 4615 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 | 4616 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 4617 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 4618 | `				pTmpMap->nEntry);` |
|      ! 0 | 4619 | `			break;` |
|        - | 4620 | `		}` |
|        3 | 4621 | `		VmSpreadExpandMap(pVm, &pTos, pTmpMap, 0/*a Traversable's values are not the caller's slots*/);` |
|        3 | 4622 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 | 4623 | `		break;` |
|        - | 4624 | `	}` |
|     1136 | 4625 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     1136 | 4626 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|     1702 | 4627 | `		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,` |
|      566 | 4628 | `		                            pCallTop, ppBaseOwner, pnBaseCap) ){` |
|      ! 0 | 4629 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - | 4630 | `				"Argument unpacking: out of memory while expanding %u elements",` |
|      ! 0 | 4631 | `				pMap->nEntry);` |
|      ! 0 | 4632 | `			break;` |
|        - | 4633 | `		}` |
|     1136 | 4634 | `		VmSpreadExpandMap(pVm, &pTos, pMap, pInstr->iP1 != 0);` |
|      566 | 4635 | `	}` |
|        - | 4636 | `	/* else: not an array — leave as-is (single arg) */` |
|     1136 | 4637 | `	break;` |
|        - | 4638 | `}` |
|        - | 4639 | `/*` |
|        - | 4640 | ` * OP_FLAG_SPREAD: * * *` |
|        - | 4641 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - | 4642 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - | 4643 | ` */` |
|      343 | 4644 | `case PH7_OP_FLAG_SPREAD: {` |
|        - | 4645 | `#ifdef UNTRUST` |
|        - | 4646 | `	if( pTos < pStack ){` |
|        - | 4647 | `		goto Abort;` |
|        - | 4648 | `	}` |
|        - | 4649 | `#endif` |
|      690 | 4650 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|      690 | 4651 | `	break;` |
|        - | 4652 | `}` |
|        - | 4653 | `/* OP_LXOR: * * *` |
|        - | 4654 | ` *` |
|        - | 4655 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - | 4656 | ` * two values and push the resulting boolean value back onto the` |
|        - | 4657 | ` * stack.` |
|        - | 4658 | ` * According to the PHP language reference manual:` |
|        - | 4659 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - | 4660 | ` *  TRUE,but not both.` |
|        - | 4661 | ` */` |
|        6 | 4662 | `case PH7_OP_LXOR:{` |
|       13 | 4663 | `	ph7_value *pNos = &pTos[-1];` |
|       13 | 4664 | `	sxi32 v = 0;` |
|        - | 4665 | `#ifdef UNTRUST` |
|        - | 4666 | `	if( pNos < pStack ){` |
|        - | 4667 | `		goto Abort;` |
|        - | 4668 | `	}` |
|        - | 4669 | `#endif` |
|        - | 4670 | `	/* Force a boolean cast */` |
|       13 | 4671 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 4672 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 | 4673 | `	}` |
|       13 | 4674 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 4675 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 | 4676 | `	}` |
|       13 | 4677 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 | 4678 | `		v = 1;` |
|        3 | 4679 | `	}` |
|       13 | 4680 | `	VmPopOperand(&pTos,1);` |
|       13 | 4681 | `	pTos->x.iVal = v;` |
|       13 | 4682 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       13 | 4683 | `	break;` |
|        - | 4684 | `				 }` |
|        - | 4685 | `/* OP_EQ P1 P2 P3` |
|        - | 4686 | ` *` |
|        - | 4687 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - | 4688 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - | 4689 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4690 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4691 | ` */` |
|        - | 4692 | `/* OP_NEQ P1 P2 P3` |
|        - | 4693 | ` *` |
|        - | 4694 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - | 4695 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 4696 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4697 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4698 | ` */` |
|     7698 | 4699 | `case PH7_OP_EQ:` |
|        - | 4700 | `case PH7_OP_NEQ: {` |
|        - | 4701 | `	VmOpRc rcOp;` |
|    15401 | 4702 | `	sState.pTos = pTos;` |
|    15401 | 4703 | `	sState.pc = pc;` |
|    15401 | 4704 | `	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);` |
|    15401 | 4705 | `	pTos = sState.pTos;` |
|    15401 | 4706 | `	pc = sState.pc;` |
|    15401 | 4707 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4708 | `		goto Abort;` |
|    15401 | 4709 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       17 | 4710 | `		goto Exception;` |
|        - | 4711 | `	}` |
|    15385 | 4712 | `	break;` |
|        - | 4713 | `					  }` |
|        - | 4714 | `/* OP_TEQ P1 P2 *` |
|        - | 4715 | ` *` |
|        - | 4716 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - | 4717 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - | 4718 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4719 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4720 | ` */` |
|   398228 | 4721 | `case PH7_OP_TEQ: {` |
|        - | 4722 | `	VmOpRc rcOp;` |
|   798222 | 4723 | `	sState.pTos = pTos;` |
|   798222 | 4724 | `	sState.pc = pc;` |
|   798222 | 4725 | `	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);` |
|   798222 | 4726 | `	pTos = sState.pTos;` |
|   798222 | 4727 | `	pc = sState.pc;` |
|   798222 | 4728 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4729 | `		goto Abort;` |
|   798222 | 4730 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4731 | `		goto Exception;` |
|        - | 4732 | `	}` |
|   798222 | 4733 | `	break;` |
|        - | 4734 | `					  }` |
|        - | 4735 | `/* OP_TNE P1 P2 *` |
|        - | 4736 | ` *` |
|        - | 4737 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - | 4738 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - | 4739 | ` * instruction.` |
|        - | 4740 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4741 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4742 | ` *` |
|        - | 4743 | ` */` |
|   553936 | 4744 | `case PH7_OP_TNE: {` |
|        - | 4745 | `	VmOpRc rcOp;` |
|  1108330 | 4746 | `	sState.pTos = pTos;` |
|  1108330 | 4747 | `	sState.pc = pc;` |
|  1108330 | 4748 | `	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);` |
|  1108330 | 4749 | `	pTos = sState.pTos;` |
|  1108330 | 4750 | `	pc = sState.pc;` |
|  1108330 | 4751 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4752 | `		goto Abort;` |
|  1108330 | 4753 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4754 | `		goto Exception;` |
|        - | 4755 | `	}` |
|  1108330 | 4756 | `	break;` |
|        - | 4757 | `					  }` |
|        - | 4758 | `/* OP_LT P1 P2 P3` |
|        - | 4759 | ` *` |
|        - | 4760 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4761 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 4762 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4763 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4764 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4765 | ` *` |
|        - | 4766 | ` */` |
|        - | 4767 | `/* OP_LE P1 P2 P3` |
|        - | 4768 | ` *` |
|        - | 4769 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4770 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 4771 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4772 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4773 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4774 | ` *` |
|        - | 4775 | ` */` |
|   508469 | 4776 | `case PH7_OP_LT:` |
|        - | 4777 | `case PH7_OP_LE: {` |
|        - | 4778 | `	VmOpRc rcOp;` |
|  1018839 | 4779 | `	sState.pTos = pTos;` |
|  1018839 | 4780 | `	sState.pc = pc;` |
|  1018839 | 4781 | `	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);` |
|  1018839 | 4782 | `	pTos = sState.pTos;` |
|  1018839 | 4783 | `	pc = sState.pc;` |
|  1018839 | 4784 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4785 | `		goto Abort;` |
|  1018839 | 4786 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 4787 | `		goto Exception;` |
|        - | 4788 | `	}` |
|  1018835 | 4789 | `	break;` |
|        - | 4790 | `					  }` |
|        - | 4791 | `/* OP_GT P1 P2 P3` |
|        - | 4792 | ` *` |
|        - | 4793 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4794 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - | 4795 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4796 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4797 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4798 | ` *` |
|        - | 4799 | ` */` |
|        - | 4800 | `/* OP_GE P1 P2 P3` |
|        - | 4801 | ` *` |
|        - | 4802 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - | 4803 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - | 4804 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - | 4805 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - | 4806 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - | 4807 | ` *` |
|        - | 4808 | ` */` |
|   139441 | 4809 | `case PH7_OP_GT:` |
|        - | 4810 | `case PH7_OP_GE: {` |
|        - | 4811 | `	VmOpRc rcOp;` |
|   279339 | 4812 | `	sState.pTos = pTos;` |
|   279339 | 4813 | `	sState.pc = pc;` |
|   279339 | 4814 | `	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);` |
|   279339 | 4815 | `	pTos = sState.pTos;` |
|   279339 | 4816 | `	pc = sState.pc;` |
|   279339 | 4817 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4818 | `		goto Abort;` |
|   279339 | 4819 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4820 | `		goto Exception;` |
|        - | 4821 | `	}` |
|   279339 | 4822 | `	break;` |
|        - | 4823 | `					  }` |
|        - | 4824 | `/* OP_SPACESHIP * * *` |
|        - | 4825 | ` *` |
|        - | 4826 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - | 4827 | ` *   -1 if left < right` |
|        - | 4828 | ` *    0 if left == right` |
|        - | 4829 | ` *    1 if left > right` |
|        - | 4830 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - | 4831 | ` */` |
|      389 | 4832 | `case PH7_OP_SPACESHIP: {` |
|        - | 4833 | `	VmOpRc rcOp;` |
|      782 | 4834 | `	sState.pTos = pTos;` |
|      782 | 4835 | `	sState.pc = pc;` |
|      782 | 4836 | `	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);` |
|      782 | 4837 | `	pTos = sState.pTos;` |
|      782 | 4838 | `	pc = sState.pc;` |
|      782 | 4839 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4840 | `		goto Abort;` |
|      782 | 4841 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        7 | 4842 | `		goto Exception;` |
|        - | 4843 | `	}` |
|      776 | 4844 | `	break;` |
|        - | 4845 | `					  }` |
|        - | 4846 | `/*` |
|        - | 4847 | ` * OP_LOAD_REF * * *` |
|        - | 4848 | ` * Push the index of a referenced object on the stack.` |
|        - | 4849 | ` */` |
|       82 | 4850 | `case PH7_OP_LOAD_REF: {` |
|        - | 4851 | `	sxu32 nIdx;` |
|        - | 4852 | `#ifdef UNTRUST` |
|        - | 4853 | `	if( pTos < pStack ){` |
|        - | 4854 | `		goto Abort;` |
|        - | 4855 | `	}` |
|        - | 4856 | `#endif` |
|      166 | 4857 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|        - | 4858 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|        - | 4859 | `		 * out of a string still carries the BASE VARIABLE's slot index, so taking a` |
|        - | 4860 | `` 		 * reference to it aliased the WHOLE STRING — `$x = [&$s[1]]; $x[0] = "Z";` `` |
|        - | 4861 | ``		 * left $s === "Z". Same Error the `=&` and by-ref-argument paths raise. */`` |
|        3 | 4862 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|        - | 4863 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|        3 | 4864 | `		PH7_MemObjRelease(pTos);` |
|        3 | 4865 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 | 4866 | `		pTos->nIdx = SXU32_HIGH;` |
|        3 | 4867 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|        3 | 4868 | `		break;` |
|        - | 4869 | `	}` |
|        - | 4870 | `	/* Extract memory object index */` |
|      163 | 4871 | `	nIdx = pTos->nIdx;` |
|      163 | 4872 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - | 4873 | `		/* Nullify the object */` |
|      163 | 4874 | `		PH7_MemObjRelease(pTos);` |
|        - | 4875 | `		/* Mark as constant and store the index on the top of the stack */` |
|      163 | 4876 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      163 | 4877 | `		pTos->nIdx = SXU32_HIGH;` |
|      163 | 4878 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       81 | 4879 | `	}` |
|      163 | 4880 | `	break;` |
|        - | 4881 | `					  }` |
|        - | 4882 | `/*` |
|        - | 4883 | ` * OP_STORE_REF * * P3` |
|        - | 4884 | ` * Perform an assignment operation by reference.` |
|        - | 4885 | ` */` |
|     1627 | 4886 | `case PH7_OP_STORE_REF: {` |
|        - | 4887 | `	VmOpRc rcOp;` |
|     3259 | 4888 | `	sState.pTos = pTos;` |
|     3259 | 4889 | `	sState.pc = pc;` |
|     3259 | 4890 | `	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);` |
|     3259 | 4891 | `	pTos = sState.pTos;` |
|     3259 | 4892 | `	pc = sState.pc;` |
|     3259 | 4893 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 4894 | `		goto Abort;` |
|     3257 | 4895 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       20 | 4896 | `		goto Exception;` |
|        - | 4897 | `	}` |
|     3239 | 4898 | `	break;` |
|        - | 4899 | `					  }` |
|        - | 4900 | `/*` |
|        - | 4901 | ` * OP_UPLINK P1 * *` |
|        - | 4902 | ` * Link a variable to the top active VM frame.` |
|        - | 4903 | ` * This is used to implement the 'global' PHP construct.` |
|        - | 4904 | ` */` |
|      225 | 4905 | `case PH7_OP_UPLINK: {` |
|      455 | 4906 | `	if( pVm->pFrame->pParent ){` |
|      455 | 4907 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - | 4908 | `		SyString sName;` |
|        - | 4909 | `		/* rcSv carries the coercion status OUT of the loop: the routing macro is a` |
|        - | 4910 | ``		 * bare `break` here, which would only leave the while and then pop the`` |
|        - | 4911 | `		 * operands with a throw pending. */` |
|      455 | 4912 | `		sxi32 rcSv = SXRET_OK;` |
|        - | 4913 | `		/* Perform the link */` |
|      917 | 4914 | `		while( pLink <= pTos ){` |
|        - | 4915 | `			/* Force a string cast — global $$arr link name (user-visible, §2) */` |
|      473 | 4916 | `			rcSv = PH7_MemObjToStringUV(pLink);` |
|      473 | 4917 | `			if( rcSv != SXRET_OK ){` |
|        7 | 4918 | `				break;` |
|        - | 4919 | `			}` |
|      467 | 4920 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|      467 | 4921 | `			if( sName.nByte > 0 ){` |
|      467 | 4922 | `				VmFrameLink(&(*pVm),&sName);` |
|      231 | 4923 | `			}` |
|      467 | 4924 | `			pLink++;` |
|        5 | 4925 | `		}` |
|      455 | 4926 | `		PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|      222 | 4927 | `	}` |
|      449 | 4928 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|      449 | 4929 | `	break;` |
|        - | 4930 | `					}` |
|        - | 4931 | `/*` |
|        - | 4932 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - | 4933 | ` * Push an exception in the corresponding container so that` |
|        - | 4934 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - | 4935 | ` */` |
|   738234 | 4936 | `case PH7_OP_LOAD_EXCEPTION: {` |
|        - | 4937 | `	VmOpRc rcOp;` |
|  1476474 | 4938 | `	sState.pTos = pTos;` |
|  1476474 | 4939 | `	sState.pc = pc;` |
|  1476474 | 4940 | `	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);` |
|  1476474 | 4941 | `	pTos = sState.pTos;` |
|  1476474 | 4942 | `	pc = sState.pc;` |
|  1476474 | 4943 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 4944 | `		goto Abort;` |
|  1476474 | 4945 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 4946 | `		goto Exception;` |
|        - | 4947 | `	}` |
|  1476474 | 4948 | `	break;` |
|        - | 4949 | `					  }` |
|        - | 4950 | `/*` |
|        - | 4951 | ` * OP_POP_EXCEPTION * * P3` |
|        - | 4952 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - | 4953 | ` */` |
|   684911 | 4954 | `case PH7_OP_POP_EXCEPTION: {` |
|  1369828 | 4955 | `	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;` |
|        - | 4956 | `	VmFrame *pBodyFrame;` |
|        - | 4957 | `	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on` |
|        - | 4958 | `	 * top (matched by compiled origin). pException == NULL means this try's` |
|        - | 4959 | `	 * activation was already consumed (an in-place catch handled a throw and` |
|        - | 4960 | `	 * ran the finally itself); compiled fields keep coming from p3. */` |
|  1369828 | 4961 | `	ph7_exception *pException = 0;` |
|  1369828 | 4962 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|     7283 | 4963 | `		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|     7283 | 4964 | `		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];` |
|        - | 4965 | `		/* Same compiled origin is NOT enough: under recursion, when THIS` |
|        - | 4966 | `		 * level's activation was already consumed by an in-place catch (the` |
|        - | 4967 | `		 * resume lands right here), the top can be an OUTER level's activation` |
|        - | 4968 | `		 * of the same lexical try — popping it would run that level's finally` |
|        - | 4969 | `		 * early and orphan its handler (probed: recursive try/catch/finally` |
|        - | 4970 | `		 * lost the outer catch entirely). The activation must also belong to` |
|        - | 4971 | `		 * the CURRENT body frame. */` |
|     7278 | 4972 | `		if( VmExcMatches(pTop,pCompiledExc)` |
|     7179 | 4973 | `		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){` |
|     7069 | 4974 | `			pException = pTop;` |
|     7069 | 4975 | `			(void)SySetPop(&pVm->aException);` |
|     3533 | 4976 | `		}` |
|     3640 | 4977 | `	}` |
|  1369828 | 4978 | `	if( pCompiledExc->iInlined ){` |
|        - | 4979 | `		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.` |
|        - | 4980 | `		 * Pop this try's handler off aException so a throw in the finally propagates to` |
|        - | 4981 | `		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the` |
|        - | 4982 | `		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the` |
|        - | 4983 | `		 * compiler-emitted JMP falls into the finally. Without a finally, this is the` |
|        - | 4984 | `		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */` |
|      167 | 4985 | `		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */` |
|      167 | 4986 | `		if( pCompiledExc->iHasFinally ){` |
|        - | 4987 | `			VmFinallyAction sAct;` |
|       14 | 4988 | `			SyZero(&sAct,sizeof(sAct));` |
|       14 | 4989 | `			sAct.eKind = PH7_FA_FALLTHROUGH;` |
|       14 | 4990 | `			sAct.iNextPc = pCompiledExc->iEndCatchPc;` |
|       14 | 4991 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        - | 4992 | `			/* keep the transparent frame for OP_END_FINALLY */` |
|      161 | 4993 | `		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       20 | 4994 | `			VmLeaveFrame(&(*pVm));` |
|        8 | 4995 | `		}` |
|      167 | 4996 | `		break;` |
|        - | 4997 | `	}` |
|        - | 4998 | `	/* Leave the exception frame. It is normally on top here (a try that fell through` |
|        - | 4999 | `	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a` |
|        - | 5000 | `	 * RESUMED generator/fiber body whose yield was inside this try, that exception` |
|        - | 5001 | `	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),` |
|        - | 5002 | `	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the` |
|        - | 5003 | `	 * coroutine (and defeat the bHasRet materialization just below, which must see the` |
|        - | 5004 | `	 * body). Only leave a genuine exception frame. */` |
|  1369666 | 5005 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|  1160908 | 5006 | `		VmLeaveFrame(&(*pVm));` |
|   580452 | 5007 | `	}` |
|        - | 5008 | `	/* Execute the finally block if present and not already executed by the` |
|        - | 5009 | `	 * catch path. No live activation (pException == NULL) means an in-place` |
|        - | 5010 | `	 * catch consumed it — and that path runs the finally itself — so skip. */` |
|  1369666 | 5011 | `	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){` |
|        - | 5012 | `		sxi32 rcFinally;` |
|       65 | 5013 | `		VmExcRelease(&(*pVm),pException);` |
|       65 | 5014 | `		pException = 0;` |
|       65 | 5015 | `		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);` |
|       65 | 5016 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 | 5017 | `			goto Abort;` |
|        - | 5018 | `		}` |
|       65 | 5019 | `		if( rcFinally == PH7_EXCEPTION ){` |
|        - | 5020 | `			/* The finally threw past itself. If an enclosing try IN THIS function` |
|        - | 5021 | `			 * caught the new exception in place, resume at its landing pad;` |
|        - | 5022 | `			 * otherwise it was caught at an outer frame, so unwind this function. */` |
|        - | 5023 | `			sxi32 iResumePc;` |
|        5 | 5024 | `			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        3 | 5025 | `				pc = iResumePc;` |
|        3 | 5026 | `				break;` |
|        - | 5027 | `			}` |
|        3 | 5028 | `			goto Exception;` |
|        - | 5029 | `		}` |
|       28 | 5030 | `	}` |
|  1369662 | 5031 | `	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */` |
|  1369662 | 5032 | `	pBodyFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  1369662 | 5033 | `	if( pBodyFrame->bHasRet ){` |
|        - | 5034 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - | 5035 | `		 * function. The return targets the body frame this try belongs to. Drain` |
|        - | 5036 | `		 * outer finally blocks first, then — only in the real function body` |
|        - | 5037 | `		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline` |
|        - | 5038 | `		 * try within a catch/finally) propagate outward so the owning body returns. */` |
|    23945 | 5039 | `		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|    23945 | 5040 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 5041 | `			goto Abort;` |
|        - | 5042 | `		}` |
|    23945 | 5043 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 5044 | `			goto Exception;` |
|        - | 5045 | `		}` |
|    23945 | 5046 | `		if( !sState.bReturnPropagates ){` |
|    23939 | 5047 | `			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);` |
|    11967 | 5048 | `		}` |
|    23945 | 5049 | `		goto Done;` |
|        - | 5050 | `	}` |
|  1345722 | 5051 | `	if( pBodyFrame->nCatchJmpPc > 0 ){` |
|        - | 5052 | ``		/* A `break`/`continue` inside the catch (OP_CATCH_JMP) parked its loop-exit`` |
|        - | 5053 | `		 * target on this body frame, and this is a landing pad on its way out. */` |
|       88 | 5054 | `		if( pBodyFrame->nCatchJmpLevels > 1 ){` |
|        - | 5055 | `			/* Still one or more detached bodies out from the target's array — this try` |
|        - | 5056 | `			 * was declared INSIDE another catch/finally mini-program. End this one and` |
|        - | 5057 | `			 * let the park travel outward; the next landing pad decrements again. */` |
|        6 | 5058 | `			pBodyFrame->nCatchJmpLevels--;` |
|        6 | 5059 | `			goto Done;` |
|        - | 5060 | `		}` |
|       84 | 5061 | `		if( pBodyFrame->nCatchJmpCross > 0 ){` |
|        - | 5062 | `			/* Enclosing trys between the catch and the loop: the jump lands past their` |
|        - | 5063 | `			 * OP_POP_EXCEPTION, so run their finally (and leave their frames) here. */` |
|        8 | 5064 | `			sxu32 nCross = pBodyFrame->nCatchJmpCross;` |
|        8 | 5065 | `			pBodyFrame->nCatchJmpCross = 0;` |
|        8 | 5066 | `			rc = VmDrainCrossedTrys(&(*pVm),nCross,sState.nExceptionBase);` |
|        8 | 5067 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5068 | `				goto Abort;` |
|        - | 5069 | `			}` |
|        8 | 5070 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 5071 | `				/* A drained finally threw past itself — it supersedes the loop exit. */` |
|      ! 0 | 5072 | `				pBodyFrame->nCatchJmpPc = 0;` |
|      ! 0 | 5073 | `				goto Exception;` |
|        - | 5074 | `			}` |
|        3 | 5075 | `		}` |
|       84 | 5076 | `		pc = (sxi32)pBodyFrame->nCatchJmpPc - 1;` |
|       84 | 5077 | `		pBodyFrame->nCatchJmpPc = 0;` |
|       84 | 5078 | `		break;` |
|        - | 5079 | `	}` |
|  1345638 | 5080 | `	break;` |
|        - | 5081 | `							}` |
|        - | 5082 | `/*` |
|        - | 5083 | ` * OP_CATCH_JMP P1(levels,cross) P2(target pc) *` |
|        - | 5084 | ` * Jump to iP2, leaving the try/catch structures iP1 describes (PH7_CATCH_JMP_P1).` |
|        - | 5085 | ` *` |
|        - | 5086 | ` * CROSS enclosing trys are being left without reaching their OP_POP_EXCEPTION, so this` |
|        - | 5087 | ` * runs their finallys itself. LEVELS says whether the target is even addressable from` |
|        - | 5088 | `` * here: 0 means it is in this same array (a `goto` out of a try body) and the jump is`` |
|        - | 5089 | ` * taken on the spot; above 0 the jump starts inside a DETACHED catch/finally` |
|        - | 5090 | ` * mini-program whose array is not the target's, so the target is PARKED on the owning` |
|        - | 5091 | `` * body's frame and this mini-program ends — mirroring how an explicit `return` in a`` |
|        - | 5092 | ` * catch parks on sRet at OP_DONE above. VmThrowException then runs this try's finally` |
|        - | 5093 | ` * and returns; the resume lands on the try's OP_POP_EXCEPTION, which decrements LEVELS` |
|        - | 5094 | ` * and either re-parks (another mini-program out) or takes the jump.` |
|        - | 5095 | ` */` |
|       47 | 5096 | `case PH7_OP_CATCH_JMP: {` |
|        - | 5097 | `	VmFrame *pTgt;` |
|       98 | 5098 | `	if( PH7_CATCH_JMP_LEVELS(pInstr->iP1) == 0 ){` |
|        - | 5099 | ``		/* No detached body to leave — a `goto` whose target is in THIS array, but which`` |
|        - | 5100 | `		 * jumps out of one or more enclosing trys. Nothing to park: run their finallys` |
|        - | 5101 | `		 * (a bare jump would leave them to fire at teardown) and go. */` |
|       14 | 5102 | `		rc = VmDrainCrossedTrys(&(*pVm),PH7_CATCH_JMP_CROSS(pInstr->iP1),sState.nExceptionBase);` |
|       14 | 5103 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 5104 | `			goto Abort;` |
|        - | 5105 | `		}` |
|       14 | 5106 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 5107 | `			/* A drained finally threw past itself — it supersedes the jump. */` |
|      ! 0 | 5108 | `			goto Exception;` |
|        - | 5109 | `		}` |
|       14 | 5110 | `		pc = (sxi32)pInstr->iP2 - 1;` |
|       14 | 5111 | `		break;` |
|        - | 5112 | `	}` |
|       86 | 5113 | `	pTgt = VmSkipExceptionFrames(pVm->pFrame);` |
|       86 | 5114 | `	pTgt->nCatchJmpPc = (sxu32)pInstr->iP2;` |
|       86 | 5115 | `	pTgt->nCatchJmpLevels = PH7_CATCH_JMP_LEVELS(pInstr->iP1);` |
|       86 | 5116 | `	pTgt->nCatchJmpCross = PH7_CATCH_JMP_CROSS(pInstr->iP1);` |
|        - | 5117 | `	/* A try opened INSIDE this catch body that the jump crosses had its finally run by` |
|        - | 5118 | `	 * the compiler-emitted OP_POP_EXCEPTION; drain anything still pending here. */` |
|       86 | 5119 | `	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);` |
|       86 | 5120 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 5121 | `		goto Abort;` |
|        - | 5122 | `	}` |
|       86 | 5123 | `	if( rc == PH7_EXCEPTION ){` |
|        - | 5124 | `		/* A drained finally threw past itself — it discards this jump. */` |
|      ! 0 | 5125 | `		pTgt->nCatchJmpPc = 0;` |
|      ! 0 | 5126 | `		goto Exception;` |
|        - | 5127 | `	}` |
|       86 | 5128 | `	goto Done;` |
|        - | 5129 | `					   }` |
|        - | 5130 | `/*` |
|        - | 5131 | ` * OP_CATCH iP1(catch-index) * P3(ph7_exception)` |
|        - | 5132 | ` * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on` |
|        - | 5133 | ` * pException->pInflight by VmThrowInline) into the catch variable, resolved in the` |
|        - | 5134 | ` * enclosing body's scope (PHP: a catch shares the surrounding variable scope).` |
|        - | 5135 | ` */` |
|       39 | 5136 | `case PH7_OP_CATCH: {` |
|        - | 5137 | `	VmOpRc rcOp;` |
|       83 | 5138 | `	sState.pTos = pTos;` |
|       83 | 5139 | `	sState.pc = pc;` |
|       83 | 5140 | `	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);` |
|       83 | 5141 | `	pTos = sState.pTos;` |
|       83 | 5142 | `	pc = sState.pc;` |
|       83 | 5143 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5144 | `		goto Abort;` |
|       83 | 5145 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5146 | `		goto Exception;` |
|        - | 5147 | `	}` |
|       83 | 5148 | `	break;` |
|        - | 5149 | `					  }` |
|        - | 5150 | `/*` |
|        - | 5151 | ` * OP_END_FINALLY * P3(ph7_exception)` |
|        - | 5152 | ` * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch` |
|        - | 5153 | ` * the pending action queued when the finally was entered (fall-through / re-throw /` |
|        - | 5154 | ` * return / break-continue). A return/break threads out through each enclosing finally` |
|        - | 5155 | ` * via pException->iNextFinallyPc.` |
|        - | 5156 | ` */` |
|       24 | 5157 | `case PH7_OP_END_FINALLY: {` |
|       52 | 5158 | `	ph7_exception *pExc = (ph7_exception *)pInstr->p3;` |
|        - | 5159 | `	VmFinallyAction sAct;` |
|       52 | 5160 | `	int eKind = PH7_FA_FALLTHROUGH;` |
|        - | 5161 | `	/* Leave the try's transparent frame kept alive across the finally. */` |
|       52 | 5162 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        3 | 5163 | `		VmLeaveFrame(&(*pVm));` |
|        1 | 5164 | `	}` |
|       52 | 5165 | `	if( SySetUsed(&pVm->aFinallyAction) > 0 ){` |
|       52 | 5166 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);` |
|       52 | 5167 | `		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];` |
|       52 | 5168 | `		(void)SySetPop(&pVm->aFinallyAction);` |
|       52 | 5169 | `		eKind = sAct.eKind;` |
|       28 | 5170 | `	}else{` |
|      ! 0 | 5171 | `		SyZero(&sAct,sizeof(sAct));` |
|      ! 0 | 5172 | `		sAct.iNextPc = pExc->iEndCatchPc;` |
|        - | 5173 | `	}` |
|       52 | 5174 | `	if( eKind == PH7_FA_FALLTHROUGH ){` |
|       12 | 5175 | `		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;` |
|       16 | 5176 | `		break;` |
|       42 | 5177 | `	}else if( eKind == PH7_FA_JMP ){` |
|        - | 5178 | `		/* break/continue: run the remaining crossed finallys, then take the jump. */` |
|        5 | 5179 | `		sxu32 iFpc = 0;` |
|        5 | 5180 | `		int nCross = sAct.nCross;` |
|        5 | 5181 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|      ! 0 | 5182 | `			sAct.nCross = nCross;` |
|      ! 0 | 5183 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      ! 0 | 5184 | `			pc = (sxi32)iFpc - 1;` |
|      ! 0 | 5185 | `			break;` |
|        - | 5186 | `		}` |
|        5 | 5187 | `		pc = (sxi32)sAct.iNextPc - 1;` |
|        5 | 5188 | `		break;` |
|       38 | 5189 | `	}else if( eKind == PH7_FA_RETHROW ){` |
|        8 | 5190 | `		ph7_class_instance *pRe = sAct.pExc;` |
|        - | 5191 | `		sxi32 _iRpE;` |
|        8 | 5192 | `		rc = VmThrowException(&(*pVm),pRe);` |
|        8 | 5193 | `		if( pRe ){ PH7_ClassInstanceUnref(pRe); }` |
|        8 | 5194 | `		if( rc == SXERR_ABORT ){ goto Abort; }` |
|        8 | 5195 | `		PH7_INLINE_RESUME_BREAK()` |
|        8 | 5196 | `		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }` |
|        8 | 5197 | `		goto Exception;` |
|      ! 0 | 5198 | `	}else{ /* PH7_FA_RETURN */` |
|       31 | 5199 | `		sxu32 iFpc = 0;` |
|       31 | 5200 | `		int nCross = sAct.nCross;` |
|       31 | 5201 | `		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        - | 5202 | `			/* Thread the return through the next enclosing finally. */` |
|        6 | 5203 | `			sAct.nCross = nCross;` |
|        6 | 5204 | `			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        6 | 5205 | `			pc = (sxi32)iFpc - 1;` |
|        6 | 5206 | `			break;` |
|        - | 5207 | `		}` |
|        - | 5208 | `		/* No enclosing finally left: materialize the return from this body. */` |
|       27 | 5209 | `		if( sAct.bHasRetVal && sState.pResult ){` |
|        6 | 5210 | `			PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|        2 | 5211 | `		}` |
|       27 | 5212 | `		PH7_MemObjRelease(&sAct.sRet);` |
|       27 | 5213 | `		goto Done;` |
|        - | 5214 | `	}` |
|        - | 5215 | `						 }` |
|        - | 5216 | `/*` |
|        - | 5217 | ` * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)` |
|        - | 5218 | `` * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a`` |
|        - | 5219 | ` * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);` |
|        - | 5220 | ` * OP_END_FINALLY threads it out through the finally chain and then returns.` |
|        - | 5221 | ` */` |
|       30 | 5222 | `case PH7_OP_SET_FINALLY_RET: {` |
|        - | 5223 | `	VmFinallyAction sAct;` |
|       65 | 5224 | `	sxu32 iFpc = 0;` |
|       65 | 5225 | `	int nCross = -1; /* a return crosses every enclosing finally in this function */` |
|       65 | 5226 | `	SyZero(&sAct,sizeof(sAct));` |
|       65 | 5227 | `	sAct.eKind = PH7_FA_RETURN;` |
|       65 | 5228 | `	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       65 | 5229 | `	PH7_MemObjInit(pVm,&sAct.sRet);` |
|       65 | 5230 | `	if( pInstr->iP1 && pTos >= pStack ){` |
|       53 | 5231 | `		PH7_MemObjStore(pTos,&sAct.sRet);` |
|       53 | 5232 | `		sAct.bHasRetVal = 1;` |
|       53 | 5233 | `		VmPopOperand(&pTos,1);` |
|       24 | 5234 | `	}` |
|       65 | 5235 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        9 | 5236 | `		sAct.nCross = nCross;` |
|        9 | 5237 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        9 | 5238 | `		pc = (sxi32)iFpc - 1;` |
|        9 | 5239 | `		break;` |
|        - | 5240 | `	}` |
|        - | 5241 | `	/* No enclosing finally left: return now. */` |
|       59 | 5242 | `	if( sAct.bHasRetVal && sState.pResult ){` |
|       49 | 5243 | `		PH7_MemObjStore(&sAct.sRet,sState.pResult);` |
|       22 | 5244 | `	}` |
|       59 | 5245 | `	PH7_MemObjRelease(&sAct.sRet);` |
|       59 | 5246 | `	goto Done;` |
|        - | 5247 | `						 }` |
|        - | 5248 | `/*` |
|        - | 5249 | ` * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)` |
|        - | 5250 | `` * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action`` |
|        - | 5251 | ` * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.` |
|        - | 5252 | ` */` |
|        4 | 5253 | `case PH7_OP_SET_FINALLY_JMP: {` |
|        - | 5254 | `	VmFinallyAction sAct;` |
|       11 | 5255 | `	sxu32 iFpc = 0;` |
|       11 | 5256 | `	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */` |
|       11 | 5257 | `	SyZero(&sAct,sizeof(sAct));` |
|       11 | 5258 | `	sAct.eKind = PH7_FA_JMP;` |
|       11 | 5259 | `	sAct.iNextPc = pInstr->iP2;` |
|       11 | 5260 | `	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){` |
|        5 | 5261 | `		sAct.nCross = nCross;` |
|        5 | 5262 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|        5 | 5263 | `		pc = (sxi32)iFpc - 1;` |
|        5 | 5264 | `		break;` |
|        - | 5265 | `	}` |
|        - | 5266 | `	/* No finally among the crossed trys: just take the break/continue jump. */` |
|        6 | 5267 | `	pc = (sxi32)sAct.iNextPc - 1;` |
|        6 | 5268 | `	break;` |
|        - | 5269 | `						 }` |
|        - | 5270 | `/*` |
|        - | 5271 | ` * OP_THROW * P2 *` |
|        - | 5272 | ` * Throw an user exception.` |
|        - | 5273 | ` */` |
|   500443 | 5274 | `case PH7_OP_THROW: {` |
|        - | 5275 | `	VmOpRc rcOp;` |
|  1000891 | 5276 | `	sState.pTos = pTos;` |
|  1000891 | 5277 | `	sState.pc = pc;` |
|  1000891 | 5278 | `	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);` |
|  1000891 | 5279 | `	pTos = sState.pTos;` |
|  1000891 | 5280 | `	pc = sState.pc;` |
|  1000891 | 5281 | `	if( rcOp == VM_OP_ABORT ){` |
|       41 | 5282 | `		goto Abort;` |
|  1000855 | 5283 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|   600405 | 5284 | `		goto Exception;` |
|        - | 5285 | `	}` |
|   400455 | 5286 | `	break;` |
|        - | 5287 | `					  }` |
|        - | 5288 | `/*` |
|        - | 5289 | ` * OP_FOREACH_INIT * P2 P3` |
|        - | 5290 | ` * Prepare a foreach step.` |
|        - | 5291 | ` */` |
|    17716 | 5292 | `case PH7_OP_FOREACH_INIT: {` |
|        - | 5293 | `	VmOpRc rcOp;` |
|    35437 | 5294 | `	sState.pTos = pTos;` |
|    35437 | 5295 | `	sState.pc = pc;` |
|    35437 | 5296 | `	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);` |
|    35437 | 5297 | `	pTos = sState.pTos;` |
|    35437 | 5298 | `	pc = sState.pc;` |
|    35437 | 5299 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5300 | `		goto Abort;` |
|    35437 | 5301 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       27 | 5302 | `		goto Exception;` |
|        - | 5303 | `	}` |
|    35411 | 5304 | `	break;` |
|        - | 5305 | `					  }` |
|        - | 5306 | `/*` |
|        - | 5307 | ` * OP_FOREACH_STEP * P2 P3` |
|        - | 5308 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - | 5309 | ` */` |
|   237987 | 5310 | `case PH7_OP_FOREACH_STEP: {` |
|        - | 5311 | `	VmOpRc rcOp;` |
|   475988 | 5312 | `	sState.pTos = pTos;` |
|   475988 | 5313 | `	sState.pc = pc;` |
|   475988 | 5314 | `	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);` |
|   475988 | 5315 | `	pTos = sState.pTos;` |
|   475988 | 5316 | `	pc = sState.pc;` |
|   475988 | 5317 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 5318 | `		goto Abort;` |
|   475986 | 5319 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      ! 0 | 5320 | `		goto Exception;` |
|        - | 5321 | `	}` |
|   475986 | 5322 | `	break;` |
|        - | 5323 | `						  }` |
|        - | 5324 | `/*` |
|        - | 5325 | ` * OP_MEMBER P1 P2` |
|        - | 5326 | ` * Load class attribute/method on the stack.` |
|        - | 5327 | ` */` |
|   183615 | 5328 | `case PH7_OP_MEMBER: {` |
|        - | 5329 | `	VmOpRc rcOp;` |
|   367239 | 5330 | `	sState.pTos = pTos;` |
|   367239 | 5331 | `	sState.pc = pc;` |
|   367239 | 5332 | `	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);` |
|   367239 | 5333 | `	pTos = sState.pTos;` |
|   367239 | 5334 | `	pc = sState.pc;` |
|   367239 | 5335 | `	if( rcOp == VM_OP_ABORT ){` |
|        8 | 5336 | `		goto Abort;` |
|   367233 | 5337 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      184 | 5338 | `		goto Exception;` |
|        - | 5339 | `	}` |
|   367051 | 5340 | `	break;` |
|        - | 5341 | `					  }` |
|        - | 5342 | `/*` |
|        - | 5343 | ` * OP_NEW P1 * * *` |
|        - | 5344 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - | 5345 | ` */` |
|  1064098 | 5346 | `case PH7_OP_NEW: {` |
|        - | 5347 | `	VmOpRc rcOp;` |
|  2128201 | 5348 | `	sState.pTos = pTos;` |
|  2128201 | 5349 | `	sState.pc = pc;` |
|  2128201 | 5350 | `	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);` |
|  2128201 | 5351 | `	pTos = sState.pTos;` |
|  2128201 | 5352 | `	pc = sState.pc;` |
|  2128201 | 5353 | `	if( rcOp == VM_OP_ABORT ){` |
|        5 | 5354 | `		goto Abort;` |
|  2128197 | 5355 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|      272 | 5356 | `		goto Exception;` |
|        - | 5357 | `	}` |
|  2127927 | 5358 | `	break;` |
|        - | 5359 | `					  }` |
|        - | 5360 | `/*` |
|        - | 5361 | ` * OP_CLONE * * *` |
|        - | 5362 | ` * Perfome a clone operation.` |
|        - | 5363 | ` */` |
|      208 | 5364 | `case PH7_OP_CLONE: {` |
|        - | 5365 | `	VmOpRc rcOp;` |
|      421 | 5366 | `	sState.pTos = pTos;` |
|      421 | 5367 | `	sState.pc = pc;` |
|      421 | 5368 | `	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);` |
|      421 | 5369 | `	pTos = sState.pTos;` |
|      421 | 5370 | `	pc = sState.pc;` |
|      421 | 5371 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5372 | `		goto Abort;` |
|      421 | 5373 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       41 | 5374 | `		goto Exception;` |
|        - | 5375 | `	}` |
|      381 | 5376 | `	break;` |
|        - | 5377 | `					  }` |
|        - | 5378 | `/*` |
|        - | 5379 | ` * OP_SWITCH * * P3` |
|        - | 5380 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - | 5381 | ` */` |
|      115 | 5382 | `case PH7_OP_SWITCH: {` |
|        - | 5383 | `	VmOpRc rcOp;` |
|      235 | 5384 | `	sState.pTos = pTos;` |
|      235 | 5385 | `	sState.pc = pc;` |
|      235 | 5386 | `	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);` |
|      235 | 5387 | `	pTos = sState.pTos;` |
|      235 | 5388 | `	pc = sState.pc;` |
|      235 | 5389 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5390 | `		goto Abort;` |
|      235 | 5391 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        5 | 5392 | `		goto Exception;` |
|        - | 5393 | `	}` |
|      231 | 5394 | `	break;` |
|        - | 5395 | `					  }` |
|        - | 5396 | `/*` |
|        - | 5397 | ` * OP_MATCH * * P3` |
|        - | 5398 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - | 5399 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - | 5400 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - | 5401 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - | 5402 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - | 5403 | ` */` |
|      105 | 5404 | `case PH7_OP_MATCH: {` |
|        - | 5405 | `	VmOpRc rcOp;` |
|      213 | 5406 | `	sState.pTos = pTos;` |
|      213 | 5407 | `	sState.pc = pc;` |
|      213 | 5408 | `	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);` |
|      213 | 5409 | `	pTos = sState.pTos;` |
|      213 | 5410 | `	pc = sState.pc;` |
|      213 | 5411 | `	if( rcOp == VM_OP_ABORT ){` |
|      ! 0 | 5412 | `		goto Abort;` |
|      213 | 5413 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|        6 | 5414 | `		goto Exception;` |
|        - | 5415 | `	}` |
|      209 | 5416 | `	break;` |
|        - | 5417 | `					  }` |
|        - | 5418 | `/*` |
|        - | 5419 | ` * OP_YIELD P1 P2 *` |
|        - | 5420 | ` *  Yield a value from a generator function.` |
|        - | 5421 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - | 5422 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - | 5423 | ` */` |
|      631 | 5424 | `case PH7_OP_YIELD: {` |
|        - | 5425 | `	ph7_generator *pGen;` |
|     1267 | 5426 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5427 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 | 5428 | `		goto Abort;` |
|        - | 5429 | `	}` |
|     1267 | 5430 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5431 | ``		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's`` |
|        - | 5432 | `		 * pending finallys tried to yield — PHP forbids it. */` |
|      ! 0 | 5433 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5434 | `			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5435 | `			goto Abort;` |
|        - | 5436 | `		}` |
|      ! 0 | 5437 | `		goto Exception;` |
|        - | 5438 | `	}` |
|     1267 | 5439 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|     1267 | 5440 | `	if( pInstr->iP2 ){` |
|        - | 5441 | `		/* yield $key => $value: value on top, key below */` |
|        - | 5442 | `#ifdef UNTRUST` |
|        - | 5443 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - | 5444 | `#endif` |
|       30 | 5445 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       30 | 5446 | `		VmPopOperand(&pTos, 1);` |
|       30 | 5447 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       30 | 5448 | `		VmPopOperand(&pTos, 1);` |
|        - | 5449 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       30 | 5450 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|        9 | 5451 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|        9 | 5452 | `			if( nKey >= pGen->iImplicitKey ){` |
|        9 | 5453 | `				pGen->iImplicitKey = nKey + 1;` |
|        4 | 5454 | `			}` |
|        6 | 5455 | `		}` |
|     1253 | 5456 | `	}else if( pInstr->iP1 ){` |
|        - | 5457 | `		/* yield $value */` |
|        - | 5458 | `#ifdef UNTRUST` |
|        - | 5459 | `		if( pTos < pStack ) goto Abort;` |
|        - | 5460 | `#endif` |
|     1237 | 5461 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|     1237 | 5462 | `		VmPopOperand(&pTos, 1);` |
|        - | 5463 | `		/* Auto-increment key */` |
|     1237 | 5464 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|     1237 | 5465 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|     1237 | 5466 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|      621 | 5467 | `	}else{` |
|        - | 5468 | `		/* Bare yield — null value, auto-increment key */` |
|        3 | 5469 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|        3 | 5470 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|        3 | 5471 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|        3 | 5472 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - | 5473 | `	}` |
|        - | 5474 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|     1267 | 5475 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|     1267 | 5476 | `	goto Suspend;` |
|        - | 5477 | `}` |
|        - | 5478 | `/*` |
|        - | 5479 | ` * OP_YIELD_FROM * * *` |
|        - | 5480 | ` *` |
|        - | 5481 | `` * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an`` |
|        - | 5482 | ` * array/Traversable/Generator from the OUTER generator, preserving the inner` |
|        - | 5483 | ` * keys; the expression evaluates to the inner Generator's return value (or NULL).` |
|        - | 5484 | ` *` |
|        - | 5485 | ` * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,` |
|        - | 5486 | ` * advances the per-instance delegate cursor stored on the exec context (never the` |
|        - | 5487 | ` * shared foreach aStep, so independent generator instances cannot clash). The` |
|        - | 5488 | ` * iterable operand is consumed on first entry; the expression result is pushed at` |
|        - | 5489 | ` * exhaustion — net stack effect +1, identical to OP_YIELD.` |
|        - | 5490 | ` */` |
|      103 | 5491 | `case PH7_OP_YIELD_FROM: {` |
|        - | 5492 | `	ph7_generator *pGenFrom;` |
|        - | 5493 | `	ph7_exec_ctx *pCtxFrom;` |
|        - | 5494 | `	ph7_value sKey,sVal;` |
|      211 | 5495 | `	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */` |
|      211 | 5496 | `	int bExhausted = 0;` |
|      211 | 5497 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 | 5498 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");` |
|      ! 0 | 5499 | `		goto Abort;` |
|        - | 5500 | `	}` |
|      211 | 5501 | `	if( pVm->pActiveCtx->bClosing ){` |
|        - | 5502 | ``		/* A `yield from` reached while VmCloseCtx force-drives this destroyed`` |
|        - | 5503 | `		 * generator's pending finallys — PHP forbids it (distinct message from a` |
|        - | 5504 | ``		 * bare `yield`, mirroring the OP_YIELD guard above). */`` |
|      ! 0 | 5505 | `		if( VmThrowFixedError(&(*pVm), "Error",` |
|      ! 0 | 5506 | `			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){` |
|      ! 0 | 5507 | `			goto Abort;` |
|        - | 5508 | `		}` |
|      ! 0 | 5509 | `		goto Exception;` |
|        - | 5510 | `	}` |
|      211 | 5511 | `	pCtxFrom = pVm->pActiveCtx;` |
|      211 | 5512 | `	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;` |
|      211 | 5513 | `	PH7_MemObjInit(pVm,&sKey);` |
|      211 | 5514 | `	PH7_MemObjInit(pVm,&sVal);` |
|      211 | 5515 | `	if( pCtxFrom->iDelegateState == 0 ){` |
|        - | 5516 | `		/* First entry: classify the iterable on the stack top. */` |
|       87 | 5517 | `		int bIterable = 1;` |
|        - | 5518 | `#ifdef UNTRUST` |
|        - | 5519 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 5520 | `#endif` |
|       87 | 5521 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       33 | 5522 | `			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       33 | 5523 | `			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;` |
|       33 | 5524 | `			pCtxFrom->iDelegateState = 1;` |
|       73 | 5525 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       55 | 5526 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       55 | 5527 | `			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       55 | 5528 | `			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){` |
|       45 | 5529 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|       45 | 5530 | `				pCtxFrom->iDelegateState = 3;` |
|       33 | 5531 | `			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){` |
|        9 | 5532 | `				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);` |
|        9 | 5533 | `				pCtxFrom->iDelegateState = 2;` |
|        6 | 5534 | `			}else{` |
|        6 | 5535 | `				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - | 5536 | `					sizeof("IteratorAggregate")-1,FALSE,0);` |
|        8 | 5537 | `				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){` |
|        - | 5538 | `					/* Delegate to the Iterator returned by getIterator() */` |
|        - | 5539 | `					ph7_value sIt;` |
|        6 | 5540 | `					PH7_MemObjInit(pVm,&sIt);` |
|        6 | 5541 | `					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);` |
|        6 | 5542 | `					if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){` |
|        - | 5543 | `						/* getIterator() threw/aborted: drop it, consume the` |
|        - | 5544 | `						 * operand, and propagate. */` |
|      ! 0 | 5545 | `						PH7_MemObjRelease(&sIt);` |
|      ! 0 | 5546 | `						VmPopOperand(&pTos,1);` |
|      ! 0 | 5547 | `						goto yf_propagate;` |
|        - | 5548 | `					}` |
|        4 | 5549 | `					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls` |
|        6 | 5550 | `						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){` |
|        6 | 5551 | `						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);` |
|        6 | 5552 | `						pCtxFrom->iDelegateState = 2;` |
|        4 | 5553 | `					}else{` |
|      ! 0 | 5554 | `						bIterable = 0;` |
|        - | 5555 | `					}` |
|        6 | 5556 | `					PH7_MemObjRelease(&sIt);` |
|        4 | 5557 | `				}else{` |
|      ! 0 | 5558 | `					bIterable = 0;` |
|        - | 5559 | `				}` |
|        - | 5560 | `			}` |
|       30 | 5561 | `		}else{` |
|        6 | 5562 | `			bIterable = 0;` |
|        - | 5563 | `		}` |
|       87 | 5564 | `		VmPopOperand(&pTos,1); /* Consume the iterable operand */` |
|       87 | 5565 | `		if( !bIterable ){` |
|        - | 5566 | `			/* Non-iterable source: throw a catchable Error (PHP 8.5), then` |
|        - | 5567 | `			 * funnel through the shared teardown/route path. */` |
|        6 | 5568 | `			rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 5569 | `				"Can use \"yield from\" only with arrays and Traversables",` |
|        - | 5570 | `				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);` |
|        6 | 5571 | `			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        6 | 5572 | `			goto yf_propagate;` |
|        - | 5573 | `		}` |
|       83 | 5574 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|        - | 5575 | `			/* A GENERATOR delegate is not rewound: php links it as a child node and` |
|        - | 5576 | ``			 * only INITIALIZES it, so `yield from $g` over a half-consumed generator`` |
|        - | 5577 | `			 * continues from where it stands. One state it refuses outright, with` |
|        - | 5578 | `			 * its own Error rather than the traverse/rewind wording the other entry` |
|        - | 5579 | `			 * points use — a generator that has already run to its end, which PHL` |
|        - | 5580 | `			 * delegated to in silence and yielded NOTHING from. */` |
|       45 | 5581 | `			ph7_class_instance *pDel = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|       45 | 5582 | `			if( PH7_VmGeneratorIsClosed(&(*pVm),pDel) ){` |
|        3 | 5583 | `				rc = VmThrowFromVm(&(*pVm),"Error",` |
|        - | 5584 | `					"Generator passed to yield from was aborted without proper return and is unable to continue",` |
|        - | 5585 | `					sizeof("Generator passed to yield from was aborted without proper return and is unable to continue")-1);` |
|        3 | 5586 | `				rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        3 | 5587 | `				goto yf_propagate;` |
|        - | 5588 | `			}` |
|       43 | 5589 | `			rcm = PH7_VmGeneratorPrime(&(*pVm),pDel);` |
|       43 | 5590 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       62 | 5591 | `		}else if( pCtxFrom->iDelegateState >= 2 ){` |
|        - | 5592 | `			/* rewind() a plain Iterator delegate */` |
|       13 | 5593 | `			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 5594 | `				"rewind",sizeof("rewind")-1,0);` |
|       13 | 5595 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 5596 | `		}` |
|       43 | 5597 | `	}else{` |
|        - | 5598 | `		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a` |
|        - | 5599 | `		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()` |
|        - | 5600 | `		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)` |
|        - | 5601 | `		 * ignore send() and just advance with next(). */` |
|        - | 5602 | `#ifdef UNTRUST` |
|        - | 5603 | `		if( pTos < pStack ){ goto Abort; }` |
|        - | 5604 | `#endif` |
|      129 | 5605 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       73 | 5606 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|        - | 5607 | `			/* A pending Generator::throw() on the outer was parked on pInjected by the` |
|        - | 5608 | `			 * body-entry gate (which skips its own raise for state 3); forward it as a` |
|        - | 5609 | `			 * throw into the delegate, else forward the sent value (pTos, which` |
|        - | 5610 | `			 * VmResumeCtx copies into the inner's own stack). */` |
|       73 | 5611 | `			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;` |
|       73 | 5612 | `			pCtxFrom->pInjected = 0;` |
|       73 | 5613 | `			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       73 | 5614 | `				if( pInjFwd ){` |
|        - | 5615 | `					/* Borrowed ref: the outer's Generator::throw() holds it across this` |
|        - | 5616 | `					 * whole resume, so the inner inject path must not unref it. */` |
|        5 | 5617 | `					pInner->pCtx->pInjected = pInjFwd;` |
|        5 | 5618 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);` |
|        5 | 5619 | `					pInner->pCtx->pInjected = 0;` |
|        3 | 5620 | `				}else{` |
|       69 | 5621 | `					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);` |
|        5 | 5622 | `				}` |
|       34 | 5623 | `			}else if( pInjFwd ){` |
|        - | 5624 | `				/* No live delegate to receive the throw (inner already finished/closed):` |
|        - | 5625 | `				 * raise it at the yield-from in the outer generator's own frame. */` |
|      ! 0 | 5626 | `				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);` |
|      ! 0 | 5627 | `				pTF->iFlags \|= VM_FRAME_THROW;` |
|      ! 0 | 5628 | `				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|      ! 0 | 5629 | `			}` |
|       73 | 5630 | `			PH7_MemObjRelease(pTos);` |
|       73 | 5631 | `			pTos--;` |
|       73 | 5632 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       35 | 5633 | `		}else{` |
|       61 | 5634 | `			PH7_MemObjRelease(pTos);` |
|       61 | 5635 | `			pTos--;` |
|       61 | 5636 | `			if( pCtxFrom->iDelegateState >= 2 ){` |
|       17 | 5637 | `				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,` |
|        - | 5638 | `					"next",sizeof("next")-1,0);` |
|       17 | 5639 | `				if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        5 | 5640 | `			}` |
|        - | 5641 | `		}` |
|        - | 5642 | `	}` |
|        - | 5643 | `	/* Fetch the current (key,value) of the delegate, or mark exhausted. */` |
|      195 | 5644 | `	if( pCtxFrom->iDelegateState == 1 ){` |
|       75 | 5645 | `		if( pCtxFrom->pDelegateNode == 0 ){` |
|       25 | 5646 | `			bExhausted = 1;` |
|       15 | 5647 | `		}else{` |
|       55 | 5648 | `			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);` |
|       55 | 5649 | `			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);` |
|        - | 5650 | `			/* Forward traversal follows pPrev (the hashmap's "reverse link",` |
|        - | 5651 | `			 * matching PH7_HashmapGetNextEntry). */` |
|       55 | 5652 | `			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;` |
|        - | 5653 | `		}` |
|       40 | 5654 | `	}else{` |
|      125 | 5655 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;` |
|        - | 5656 | `		ph7_value sValid;` |
|        - | 5657 | `		int isValid;` |
|      125 | 5658 | `		PH7_MemObjInit(pVm,&sValid);` |
|      125 | 5659 | `		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|      125 | 5660 | `		PH7_MemObjToBool(&sValid);` |
|      125 | 5661 | `		isValid = (sValid.x.iVal != 0);` |
|      125 | 5662 | `		PH7_MemObjRelease(&sValid);` |
|      125 | 5663 | `		if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|      125 | 5664 | `		if( !isValid ){` |
|       31 | 5665 | `			bExhausted = 1;` |
|       18 | 5666 | `		}else{` |
|       99 | 5667 | `			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|       99 | 5668 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|       99 | 5669 | `			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       99 | 5670 | `			if( rcm == PH7_ABORT \|\| rcm == PH7_EXCEPTION ){ goto yf_propagate; }` |
|        - | 5671 | `		}` |
|        - | 5672 | `	}` |
|      195 | 5673 | `	if( bExhausted ){` |
|        - | 5674 | `		/* Expression value: inner Generator's return value (state 3) or NULL. */` |
|        - | 5675 | `		ph7_value sResult;` |
|       51 | 5676 | `		PH7_MemObjInit(pVm,&sResult);` |
|       51 | 5677 | `		if( pCtxFrom->iDelegateState == 3 ){` |
|       26 | 5678 | `			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);` |
|       26 | 5679 | `			if( pInner && pInner->pCtx ){` |
|       26 | 5680 | `				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);` |
|       11 | 5681 | `			}` |
|       11 | 5682 | `		}` |
|       51 | 5683 | `		PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       51 | 5684 | `		pCtxFrom->pDelegateNode = 0;` |
|       51 | 5685 | `		pCtxFrom->iDelegateState = 0;` |
|       51 | 5686 | `		pTos++;` |
|       51 | 5687 | `		PH7_MemObjStore(&sResult,pTos);` |
|       51 | 5688 | `		PH7_MemObjRelease(&sResult);` |
|       51 | 5689 | `		PH7_MemObjRelease(&sKey);` |
|       51 | 5690 | `		PH7_MemObjRelease(&sVal);` |
|       51 | 5691 | `		break; /* fall through to pc+1 with the result on the stack top */` |
|        - | 5692 | `	}` |
|        - | 5693 | `	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.` |
|        - | 5694 | `	 * The outer generator's implicit auto-key counter is NOT advanced by the` |
|        - | 5695 | ``	 * delegated keys — PHP keeps it independent across `yield from`, so a later`` |
|        - | 5696 | ``	 * plain `yield` continues from the outer's own counter. */`` |
|      149 | 5697 | `	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);` |
|      149 | 5698 | `	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);` |
|      149 | 5699 | `	PH7_MemObjRelease(&sKey);` |
|      149 | 5700 | `	PH7_MemObjRelease(&sVal);` |
|        - | 5701 | `	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */` |
|      149 | 5702 | `	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));` |
|      149 | 5703 | `	goto Suspend;` |
|        8 | 5704 | `yf_propagate:` |
|        - | 5705 | `	/* A delegate iterator method threw/aborted (or the source was non-iterable):` |
|        - | 5706 | `	 * tear down the delegation, then route via the shared dispatch macro. rcm is` |
|        - | 5707 | `	 * always PH7_EXCEPTION or PH7_ABORT here. */` |
|       21 | 5708 | `	PH7_MemObjRelease(&sKey);` |
|       21 | 5709 | `	PH7_MemObjRelease(&sVal);` |
|       21 | 5710 | `	PH7_MemObjRelease(&pCtxFrom->sDelegate);` |
|       21 | 5711 | `	pCtxFrom->pDelegateNode = 0;` |
|       21 | 5712 | `	pCtxFrom->iDelegateState = 0;` |
|       21 | 5713 | `	PH7_DISPATCH_ENFORCE_RC(rcm)` |
|      ! 0 | 5714 | `	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */` |
|        - | 5715 | `}` |
|        - | 5716 | `/*` |
|        - | 5717 | ` * OP_CALL P1 * *` |
|        - | 5718 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - | 5719 | ` *  function on the stack.` |
|        - | 5720 | ` */` |
|        - | 5721 | `/*` |
|        - | 5722 | ` * OP_CALL_INIT * P2 *` |
|        - | 5723 | ` *  Screen the callee on TOS where it is WRITTEN — before this call's arguments run.` |
|        - | 5724 | ` *` |
|        - | 5725 | ` *  php resolves a call's target at INIT_FCALL / INIT_FCALL_BY_NAME / INIT_DYNAMIC_CALL` |
|        - | 5726 | `` *  and raises there, so `undefinedFn(s(1))`, `$f(s(1))` over a misspelled name and`` |
|        - | 5727 | `` *  `$v(s(1))` over an int all refuse BEFORE `s(1)` runs. PHL only ever looked at the`` |
|        - | 5728 | ` *  callee inside OP_CALL, one instruction after the whole argument list, so every one` |
|        - | 5729 | ` *  of those programs produced the argument's side effects (or its exception) first and` |
|        - | 5730 | ` *  php's Error second. The messages were already identical; only the order was not.` |
|        - | 5731 | ` *` |
|        - | 5732 | ` *  The verdict is the FIRST-CLASS-CALLABLE creation screen, unchanged and shared: php` |
|        - | 5733 | `` *  gives `f(...)` the direct call's taxonomy word for word, which makes VmFccValueError`` |
|        - | 5734 | ` *  the one builder for both. The value is left exactly as it is — OP_CALL still does its` |
|        - | 5735 | ` *  own resolution — so this adds a refusal and changes nothing that succeeds. P2 == 1` |
|        - | 5736 | ` *  when the compiler namespace-qualified the name, which is the one bit php's` |
|        - | 5737 | `` *  global-function fallback needs (an unqualified `strlen(...)` inside a namespace).`` |
|        - | 5738 | ` *` |
|        - | 5739 | ` *  Not emitted for a callee whose OP_MEMBER already screened it, for a first-class` |
|        - | 5740 | ` *  callable (OP_LOAD_FCC screens it, with nothing running in between), or for a call` |
|        - | 5741 | ` *  with no arguments at all — there the call IS the first thing that happens.` |
|        - | 5742 | ` */` |
|  1710911 | 5743 | `case PH7_OP_CALL_INIT: {` |
|  3425325 | 5744 | `	if( (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_MAGICCALL)) == 0` |
|  3425330 | 5745 | `	 && !VmValueIsClosure(pVm,pTos) ){` |
|  3421106 | 5746 | `		const char *zInitCls = 0,*zInitMeth = 0;` |
|  3421106 | 5747 | `		sxu32 nInitCls = 0,nInitMeth = 0;` |
|        - | 5748 | `		char zInitMsg[192];` |
|  3421106 | 5749 | `		const char *zInitBad = 0;` |
|  3421106 | 5750 | `		SyString sInitName = { 0, 0 };` |
|  3421106 | 5751 | `		int bInitScoped = 0;` |
|  3421106 | 5752 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|  3420956 | 5753 | `			SyStringInitFromBuf(&sInitName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 5754 | `			/* A leading backslash only anchors the name to the global namespace. */` |
|  3420956 | 5755 | `			if( sInitName.nByte > 0 && sInitName.zString[0] == '\\' ){` |
|        3 | 5756 | `				sInitName.zString++;` |
|        3 | 5757 | `				sInitName.nByte--;` |
|        1 | 5758 | `			}` |
|  3420956 | 5759 | `			bInitScoped = PH7_VmCallableStringParts(sInitName.zString,sInitName.nByte,` |
|        - | 5760 | `				&zInitCls,&nInitCls,&zInitMeth,&nInitMeth);` |
|  1712227 | 5761 | `		}` |
|  3421106 | 5762 | `		if( bInitScoped ){` |
|        - | 5763 | ``			/* A `"Class::method"` string carries its whole taxonomy in one builder — the`` |
|        - | 5764 | `			 * class, the missing/abstract/inaccessible cases and the catch-all routing —` |
|        - | 5765 | `			 * and answers 0 when the call WILL run. It is asked unconditionally because` |
|        - | 5766 | `			 * the predicate below is not the same question: is_callable() accepts a` |
|        - | 5767 | `			 * non-static method named through a class, which the direct call refuses. */` |
|       28 | 5768 | `			zInitBad = VmCallableClassMethodError(&(*pVm),` |
|        9 | 5769 | `				PH7_VmExtractClass(&(*pVm),zInitCls,nInitCls,FALSE,0),` |
|        9 | 5770 | `				zInitCls,nInitCls,zInitMeth,nInitMeth,TRUE,zInitMsg,sizeof(zInitMsg));` |
|  3421097 | 5771 | `		}else if( !PH7_VmIsCallable(&(*pVm),pTos,TRUE) ){` |
|      101 | 5772 | `			int bInitOk = 0;` |
|      101 | 5773 | `			if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        - | 5774 | `				/* php's global fallback for an UNQUALIFIED name written inside a` |
|        - | 5775 | `				 * namespace: the current namespace first, the global one after. OP_CALL` |
|        - | 5776 | `				 * retries the same way from its argument map; this only has to agree` |
|        - | 5777 | `				 * about whether the call WILL resolve, so the shortened name is tested` |
|        - | 5778 | `				 * and thrown away. */` |
|       57 | 5779 | `				const char *zInitShort = sInitName.zString;` |
|        - | 5780 | `				sxu32 iInitPos;` |
|      857 | 5781 | `				for( iInitPos = 0 ; iInitPos < sInitName.nByte ; ++iInitPos ){` |
|      805 | 5782 | `					if( sInitName.zString[iInitPos] == '\\' ){` |
|       71 | 5783 | `						zInitShort = &sInitName.zString[iInitPos + 1];` |
|       33 | 5784 | `					}` |
|      405 | 5785 | `				}` |
|       57 | 5786 | `				if( zInitShort != sInitName.zString ){` |
|        - | 5787 | `					ph7_value sInitShort;` |
|       57 | 5788 | `					PH7_MemObjInit(pVm,&sInitShort);` |
|       83 | 5789 | `					PH7_MemObjStringAppend(&sInitShort,zInitShort,` |
|       52 | 5790 | `						(sxu32)(sInitName.nByte - (sxu32)(zInitShort - sInitName.zString)));` |
|       57 | 5791 | `					bInitOk = PH7_VmIsCallable(&(*pVm),&sInitShort,TRUE);` |
|       57 | 5792 | `					PH7_MemObjRelease(&sInitShort);` |
|       26 | 5793 | `				}` |
|       26 | 5794 | `			}` |
|        - | 5795 | `			/* The FIRST-CLASS-CALLABLE creation screen's builder, unchanged and shared:` |
|        - | 5796 | ``			 * php gives `f(...)` the direct call's taxonomy word for word. It assumes the`` |
|        - | 5797 | `			 * predicate has already declined — a pair a class answers through __call is` |
|        - | 5798 | `			 * callable and never arrives here — which is why it sits under that test. */` |
|      101 | 5799 | `			if( !bInitOk ){` |
|       48 | 5800 | `				zInitBad = VmFccValueError(&(*pVm),pTos,zInitMsg,sizeof(zInitMsg));` |
|       22 | 5801 | `			}` |
|       48 | 5802 | `		}` |
|  3421106 | 5803 | `		if( zInitBad ){` |
|        - | 5804 | `			sxi32 rcInit;` |
|       54 | 5805 | `			PH7_MemObjRelease(pTos);` |
|       54 | 5806 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       54 | 5807 | `			pTos->nIdx = SXU32_HIGH;` |
|       54 | 5808 | `			rcInit = VmThrowFromVm(&(*pVm),"Error",zInitBad,(sxu32)SyStrlen(zInitBad));` |
|       54 | 5809 | `			if( rcInit == SXERR_ABORT ){ goto Abort; }` |
|       54 | 5810 | `			rc = rcInit;` |
|       80 | 5811 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 5812 | `		}` |
|  1712277 | 5813 | `	}` |
|  3425280 | 5814 | `	break;` |
|        - | 5815 | `}` |
|        - | 5816 | `/*` |
|        - | 5817 | ` * OP_ROT_CALLEE P1 P2 *` |
|        - | 5818 | ` *  Turn a call's operand region over: [callee][arg0..argN] becomes [arg0..argN][callee],` |
|        - | 5819 | ` *  which is the layout OP_CALL's entire dispatch is written against.` |
|        - | 5820 | ` *` |
|        - | 5821 | ` *  The codegen pushes the callee FIRST because php resolves it where it is written —` |
|        - | 5822 | ` *  before a single argument runs — so an undefined or inaccessible method is refused` |
|        - | 5823 | `` *  ahead of the argument list's side effects, and a `?->` on null skips the arguments`` |
|        - | 5824 | ` *  altogether. Everything downstream of this instruction still sees the historical` |
|        - | 5825 | ` *  stack, so the reordering costs one memory move per call and nothing else.` |
|        - | 5826 | ` *` |
|        - | 5827 | ` *  P1 is the compile-time argument count; P2 carries PH7_ROT_SPREAD (this call unpacks,` |
|        - | 5828 | ` *  so the runtime count is P1 plus its OWN runs' net growth) and PH7_ROT_TWOSLOT (the` |
|        - | 5829 | ` *  callee is a method pair, [receiver][name]). A __call routing collapses that pair to` |
|        - | 5830 | ` *  one marked carrier at run time, which is read off the slot rather than guessed.` |
|        - | 5831 | ` */` |
|  2224146 | 5832 | `case PH7_OP_ROT_CALLEE: {` |
|  8903595 | 5833 | `	sxi32 nRotArgs = pInstr->iP1` |
|  4451795 | 5834 | `		+ ((pInstr->iP2 & PH7_ROT_SPREAD)` |
|        - | 5835 | `			/* One past the last argument is one past the TOP here: the callee sits` |
|        - | 5836 | `			 * BELOW the region, not above it as at OP_CALL. */` |
|  2224706 | 5837 | `			? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,&pTos[1]) : 0);` |
|  4451800 | 5838 | `	if( nRotArgs < 0 ){` |
|        - | 5839 | `		/* Unreachable: an empty unpack subtracts one per compile-time position, so the` |
|        - | 5840 | `		 * net can reach 0 and no lower. Clamped rather than trusted — reading above the` |
|        - | 5841 | `		 * top to find the callee is not a failure mode worth leaving open. */` |
|      ! 0 | 5842 | `		nRotArgs = 0;` |
|      ! 0 | 5843 | `	}` |
|        - | 5844 | `	{` |
|        - | 5845 | `		ph7_value aCallee[2];` |
|  4451800 | 5846 | `		ph7_value *pTopCallee = &pTos[-nRotArgs];` |
|  4451800 | 5847 | `		sxi32 nCallee = (pInstr->iP2 & PH7_ROT_TWOSLOT) ? 2 : 1;` |
|        - | 5848 | `		ph7_value *pBase;` |
|        - | 5849 | `		sxi32 i;` |
|  4451800 | 5850 | `		if( nCallee > 1 && (pTopCallee->iFlags & MEMOBJ_AUX_MAGICCALL) ){` |
|        - | 5851 | `			/* OP_MEMBER routed a missing/inaccessible name to __call: it consumed the` |
|        - | 5852 | `			 * receiver and left ONE carrier slot, so the pair the compiler counted on` |
|        - | 5853 | `			 * is not there. */` |
|       71 | 5854 | `			nCallee = 1;` |
|       34 | 5855 | `		}` |
|  4451800 | 5856 | `		pBase = pTopCallee - (nCallee - 1);` |
|        - | 5857 | `#ifdef UNTRUST` |
|        - | 5858 | `		if( pBase < pStack ){` |
|        - | 5859 | `			goto Abort;` |
|        - | 5860 | `		}` |
|        - | 5861 | `#endif` |
|  4451800 | 5862 | `		if( nRotArgs > 0 ){` |
|  8921951 | 5863 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  4470180 | 5864 | `				aCallee[i] = pBase[i];` |
|  2236844 | 5865 | `			}` |
| 12033755 | 5866 | `			for( i = 0 ; i < nRotArgs ; ++i ){` |
|  7581984 | 5867 | `				pBase[i] = pBase[i + nCallee];` |
|  3793549 | 5868 | `			}` |
|  8921951 | 5869 | `			for( i = 0 ; i < nCallee ; ++i ){` |
|  4470180 | 5870 | `				pBase[nRotArgs + i] = aCallee[i];` |
|  2236844 | 5871 | `			}` |
|  2227637 | 5872 | `		}` |
|  4451800 | 5873 | `		if( pInstr->iP2 & PH7_ROT_SPREAD ){` |
|        - | 5874 | `			/* The argument region now ends nCallee slots lower than it did, so this` |
|        - | 5875 | `			 * call's captured unpack runs — the suffix VmSpreadOwnExtra just assigned` |
|        - | 5876 | `			 * to it, all of them anchored inside the region — move with it, and OP_CALL` |
|        - | 5877 | ``			 * re-derives the same count from them. Unconditional: an `f(...[])` unpack`` |
|        - | 5878 | `			 * moves NO argument (its run is zero-width) and still has to be re-anchored,` |
|        - | 5879 | `			 * or the recount reads it as an ordinary slot and eats one slot too many.` |
|        - | 5880 | `			 * An ENCLOSING call's runs sit below the callee and are left alone. */` |
|     1124 | 5881 | `			sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|     1124 | 5882 | `			VmSpreadRun *aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|        - | 5883 | `			sxu32 r;` |
|     2258 | 5884 | `			for( r = pVm->nSpreadCallBase ; r < nRun ; ++r ){` |
|     1138 | 5885 | `				aRun[r].pStart -= nCallee;` |
|      571 | 5886 | `			}` |
|      560 | 5887 | `		}` |
|        - | 5888 | `	}` |
|  4451800 | 5889 | `	break;` |
|        - | 5890 | `}` |
|  3688286 | 5891 | `case PH7_OP_CALL: {` |
|        - | 5892 | `	/* iP2 = hasSpread (compile-time). Count only THIS call's own unpack` |
|        - | 5893 | `	 * expansion (VmSpreadOwnExtra, derived from the captured runs on top of the` |
|        - | 5894 | `	 * stack) — an INNER spread-bearing call evaluated inside this argument list` |
|        - | 5895 | ``	 * (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not`` |
|        - | 5896 | `	 * be conflated, which a single shared accumulator could not express. */` |
|  7380074 | 5897 | `	sxi32 nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|        - | 5898 | `	ph7_value *pArg;` |
|        - | 5899 | `	/* Consume the engine's magic-dispatch latch here, at the head of the ONE call` |
|        - | 5900 | `	 * it was set for, whatever path that call then takes. Left standing it would` |
|        - | 5901 | `	 * describe the next call instead. */` |
|  7380074 | 5902 | `	int bMagicDispatch = pVm->bMagicDispatch;` |
|        - | 5903 | `	/* ...and the member resolution's own verdict, which rides the callee SLOT rather` |
|        - | 5904 | `	 * than the VM: an OP_MEMBER that produced this callee already decided its` |
|        - | 5905 | `	 * visibility against the entry it chose, so the screen below must stand down. */` |
| 14686451 | 5906 | `	int bMemberScreened = (pTos->iFlags & MEMOBJ_AUX_MEMBERCALL) != 0` |
|  7380069 | 5907 | `		\|\| pVm->bClosureScreened;` |
|        - | 5908 | `	/* ...and whether the NAME in that slot is one of the engine's own function-table` |
|        - | 5909 | `	 * keys rather than something the program spelled: an OP_MEMBER method resolution` |
|        - | 5910 | ``	 * pushes the method's `sVmName` (`[__Class@meth_xxxxxxxxxx]`), and so do the two`` |
|        - | 5911 | `	 * synthetic call builders. Read here because the member mark is cleared just` |
|        - | 5912 | `	 * below; the closure branch adds its own case further down. It is what lets` |
|        - | 5913 | `	 * PH7_VmGetUserFunction refuse those keys to a SCRIPT that spells one. */` |
|  7380074 | 5914 | `	int bEngineCallee = (pTos->iFlags & (MEMOBJ_AUX_MEMBERCALL\|MEMOBJ_AUX_ENGINEFN)) != 0;` |
|        - | 5915 | `	/* ...and the internal-callback latch, for the same reason: it describes THIS call` |
|        - | 5916 | `	 * (an internal function invoking a userland callback binds its arguments weakly),` |
|        - | 5917 | `	 * and a call the callback body makes must not inherit it. */` |
|  7380074 | 5918 | `	int bCallbackWeak = pVm->bCallbackWeak;` |
|        - | 5919 | `	/* ...and whether this call's ANSWER is thrown away, which is what a` |
|        - | 5920 | `	 * #[\NoDiscard] callee warns about. The compiled call site says so directly` |
|        - | 5921 | `	 * (bDiscard, stamped where the statement pops the value: php's` |
|        - | 5922 | `	 * !RETURN_VALUE_USED); php also propagates the same bit THROUGH` |
|        - | 5923 | ``	 * `call_user_func()`/`call_user_func_array()`, which is why`` |
|        - | 5924 | ``	 * `call_user_func('f');` warns and `array_map('f', $a);` does not, and a`` |
|        - | 5925 | `	 * synthetic OP_CALL has no call site of its own to carry it -- so the two` |
|        - | 5926 | `	 * builtins hand it over on this latch. Consumed here like the rest. */` |
|  7380074 | 5927 | `	int bResultDropped = pVm->bDiscardCallback ? 1 : (pInstr->bDiscard != 0);` |
|        - | 5928 | `	/* php's forwarding of that bit through call_user_func() is a COMPILE-time` |
|        - | 5929 | ``	 * special case on the literal name, so `$g = "call_user_func"; $g("f");` does`` |
|        - | 5930 | `	 * not forward. The callee slot's nIdx says which spelling this is: a compiled` |
|        - | 5931 | `	 * literal is marked constant, a variable read is not. */` |
|  7380074 | 5932 | `	int bLiteralCallee = (pTos->nIdx == SXU32_HIGH);` |
|  7380074 | 5933 | `	pVm->bDiscardCallback = 0;` |
|  7380074 | 5934 | `	pVm->bMagicDispatch = 0;` |
|  7380074 | 5935 | `	pVm->bClosureScreened = 0;` |
|  7380074 | 5936 | `	pVm->bCallbackWeak = 0;` |
|  7380074 | 5937 | `	pTos->iFlags &= ~MEMOBJ_AUX_MEMBERCALL;` |
|  7380074 | 5938 | `	pArg = &pTos[-nCallArgs];` |
|        - | 5939 | `	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED` |
|        - | 5940 | `	 * arguments, and a spread expanding to !=1 element shifts the actual positions` |
|        - | 5941 | `	 * of any following compile-time named args. The effective per-actual-slot name` |
|        - | 5942 | `	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)` |
|        - | 5943 | `	 * is built PER PATH against that path's finalized arg base — a method call pops` |
|        - | 5944 | `	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch` |
|        - | 5945 | `	 * site rather than built once here. */` |
|        - | 5946 | `	VmCallArgMap sEffMap;` |
|  7380074 | 5947 | `	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;` |
|        - | 5948 | `	SyHashEntry *pEntry;` |
|        - | 5949 | `	SyString sName;` |
|        - | 5950 | `	/* The host-call trio, hoisted out of the foreign-function branch below so the` |
|        - | 5951 | `	 * NativeCall label there can also be reached from the compiled-function branch:` |
|        - | 5952 | `	 * a VM_FUNC_NATIVE method is a C body wearing a method's clothes, and it runs on` |
|        - | 5953 | `	 * exactly the foreign path (no frame, no call record, no operand stack) once` |
|        - | 5954 | `	 * $this and the called class have been resolved the method way. Jumping into` |
|        - | 5955 | `	 * that branch would otherwise cross these declarations. */` |
|        - | 5956 | `	ph7_user_func *pFunc;` |
|        - | 5957 | `	ph7_context sCtx;` |
|        - | 5958 | `	ph7_value sRet;` |
|        - | 5959 | `	/* Native-METHOD call state; all 0 for a plain host function, which is what the` |
|        - | 5960 | `	 * foreign branch's own fallthrough leaves.` |
|        - | 5961 | `	 *   pNativeOwned — the reference the method path took, released at the shared tail` |
|        - | 5962 | `	 *   pNativeRecv  — what the body actually sees as $this (0 for a static method)` |
|        - | 5963 | `	 *   pNativeClass — the late-static-binding target */` |
|  7380074 | 5964 | `	ph7_class_instance *pNativeOwned = 0;` |
|  7380074 | 5965 | `	ph7_class_instance *pNativeRecv = 0;` |
|  7380074 | 5966 | `	ph7_class *pNativeClass = 0;` |
|        - | 5967 | `	/* The engine's own __call/__callStatic routing: the OP_MEMBER immediately below this` |
|        - | 5968 | `	 * call found a missing (or inaccessible) method on a class declaring the magic handler` |
|        - | 5969 | `	 * and MARKED this callee slot, latching {receiver, class, original name} on the VM.` |
|        - | 5970 | `	 * There is no callable here at all — the mark selects the packing body directly, ahead` |
|        - | 5971 | `	 * of every callable decode below, and the record it dispatches carries no PHP name (it` |
|        - | 5972 | `	 * is not in hHostFunction). This is what replaced writing the string` |
|        - | 5973 | `	 * "__phl_magic_call" into the slot and letting the name lookup find a hidden global.` |
|        - | 5974 | ``	 * Everything from `NativeCall` down is shared with an ordinary builtin call, which is`` |
|        - | 5975 | `	 * what this has always been from the executor's point of view. */` |
|  7380074 | 5976 | `	if( pTos->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|        - | 5977 | `		/* Move the routing off the carrier and onto the VM, HERE — one instruction` |
|        - | 5978 | `		 * before the packing body reads it, with nothing in between that could set` |
|        - | 5979 | `		 * another. OP_MEMBER used to publish it directly, which only held while the` |
|        - | 5980 | `		 * arguments ran before it; now they run after, and a routed call inside this` |
|        - | 5981 | `		 * one's argument list has already come and gone. */` |
|      129 | 5982 | `		VmMagicCall *pPend = (VmMagicCall *)pTos->x.pOther;` |
|      129 | 5983 | `		pTos->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|      129 | 5984 | `		pTos->x.pOther = 0;` |
|      129 | 5985 | `		pVm->pMagicCallThis = pPend ? pPend->pRecv : 0;` |
|      129 | 5986 | `		pVm->pMagicCallClass = pPend ? pPend->pClass : 0;` |
|      129 | 5987 | `		SyBlobReset(&pVm->sMagicCallName);` |
|      129 | 5988 | `		if( pPend && SyBlobLength(&pPend->sName) > 0 ){` |
|      192 | 5989 | `			SyBlobAppend(&pVm->sMagicCallName,SyBlobData(&pPend->sName),` |
|       63 | 5990 | `				SyBlobLength(&pPend->sName));` |
|       63 | 5991 | `		}` |
|      129 | 5992 | `		if( pPend ){` |
|        - | 5993 | `			/* The receiver reference the record held is now the VM's, which` |
|        - | 5994 | `			 * VmMagicCallDispatch gives back — so drop the record without unref'ing. */` |
|      129 | 5995 | `			pPend->pRecv = 0;` |
|      129 | 5996 | `			VmFreeMagicCall(pPend);` |
|       63 | 5997 | `		}` |
|      129 | 5998 | `		pFunc = PH7_VmMagicCallFunc(&(*pVm));` |
|      129 | 5999 | `		if( pFunc == 0 ){` |
|      ! 0 | 6000 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 6001 | `			goto Abort;` |
|        - | 6002 | `		}` |
|        - | 6003 | `		/* D1: the packing body declares no by-ref parameter (php hands __call a packed` |
|        - | 6004 | `		 * ARRAY), so every deferred argument materializes by value, exactly as it did` |
|        - | 6005 | `		 * through the named trampoline's zero by-ref mask. */` |
|        - | 6006 | `		{` |
|      129 | 6007 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffCallMap);` |
|      129 | 6008 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6009 | `		}` |
|      192 | 6010 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|      126 | 6011 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|      129 | 6012 | `		goto NativeCall;` |
|        - | 6013 | `	}` |
|        - | 6014 | `	/* A Closure object is callable: unwrap it to its underlying string callable so the` |
|        - | 6015 | `	 * dispatch below handles it (rather than treating it as a generic object and looking` |
|        - | 6016 | `	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure` |
|        - | 6017 | `	 * so a plain __invoke object skips the temp-value work entirely. */` |
|  7379948 | 6018 | `	if( VmValueIsClosure(pVm,pTos) ){` |
|        - | 6019 | `		ph7_value sCallable;` |
|    14463 | 6020 | `		PH7_MemObjInit(pVm,&sCallable);` |
|    14463 | 6021 | `		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){` |
|    14463 | 6022 | `			PH7_MemObjRelease(pTos);` |
|    14463 | 6023 | `			PH7_MemObjStore(&sCallable,pTos);` |
|        - | 6024 | `` 			/* The plain-closure shape of the unwrap is the engine's own `[closure_N]` `` |
|        - | 6025 | `			 * name, which the lookup below refuses to a name a SCRIPT spelled. */` |
|    14463 | 6026 | `			bEngineCallee = 1;` |
|     7229 | 6027 | `		}` |
|    14463 | 6028 | `		PH7_MemObjRelease(&sCallable);` |
|     7229 | 6029 | `	}` |
|        - | 6030 | `	/* Extract function name */` |
|  7379948 | 6031 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|   300439 | 6032 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 6033 | `			ph7_value sResult;` |
|        - | 6034 | `			sxi32 rcArr;` |
|        - | 6035 | `			/* Taken off the VM at the head of the shape check below, not at the dispatch:` |
|        - | 6036 | `			 * everything between the two (the deferred-argument materialization especially)` |
|        - | 6037 | `			 * can throw and jump out of this branch, and a latch left armed would stand the` |
|        - | 6038 | `			 * visibility screen down for whatever call runs next. */` |
|        - | 6039 | `			int bCbScreened;` |
|        - | 6040 | `			{` |
|        - | 6041 | `				/* php validates the SHAPE of an array callable first: it must hold exactly` |
|        - | 6042 | `				 * two elements. PH7 handed any array to the dispatcher, which failed` |
|        - | 6043 | ``				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */`` |
|   100332 | 6044 | `				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - | 6045 | `				char zCbMsg[192];` |
|   100332 | 6046 | `				const char *zCbErr = 0;` |
|   100332 | 6047 | `				int bCbRaised = 0; /* the class lookup ran an autoloader that threw */` |
|        - | 6048 | `				/* A pair the closure UNWRAP just built is not an array the program wrote: its` |
|        - | 6049 | `				 * callee was resolved and screened where the closure was BUILT, the way php` |
|        - | 6050 | `				 * resolves one, and it is a well-formed [target, method] by construction.` |
|        - | 6051 | `				 * Re-deciding it here, against the CALLER, is what refused an escaped` |
|        - | 6052 | ``				 * `$this->priv(...)` php runs. */`` |
|   100332 | 6053 | `				bCbScreened = pVm->bClosureScreened;` |
|   100332 | 6054 | `				pVm->bClosureScreened = 0; /* put back for the one dispatch that reads it */` |
|   100332 | 6055 | `				if( !bCbScreened && pCbMap && pCbMap->nEntry == 2 ){` |
|        - | 6056 | `					/* Shape is right; now check it actually RESOLVES. The shared dispatcher` |
|        - | 6057 | `					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for` |
|        - | 6058 | `					 * an unresolvable [class,method] pair -- silence a caller cannot detect --` |
|        - | 6059 | `					 * and its contract is relied on by call_user_func/usort, so the throw` |
|        - | 6060 | `					 * belongs here at the call site. */` |
|   100190 | 6061 | `					ph7_value *pCbCls = 0;` |
|   100190 | 6062 | `					ph7_value *pCbMeth = 0;` |
|        - | 6063 | `					/* php reads the INTEGER indices 0 and 1, not the first two entries in` |
|        - | 6064 | `					 * insertion order. Two entries at other keys are a SHAPE error of their` |
|        - | 6065 | `					 * own ("has to contain indices 0 and 1"), distinct from the` |
|        - | 6066 | ``					 * wrong-element-COUNT message below; `[1=>'m',0=>'C']` holds both, so`` |
|        - | 6067 | `					 * the target is index 0 -- the reverse of what PH7 walked. */` |
|   100190 | 6068 | `					if( !PH7_VmArrayCallableParts(&(*pVm),pCbMap,&pCbCls,&pCbMeth) ){` |
|       11 | 6069 | `						zCbErr = "Array callback has to contain indices 0 and 1";` |
|        6 | 6070 | `					}else{` |
|        - | 6071 | `						/* The resolution below can run an autoloader that throws; when it does,` |
|        - | 6072 | `						 * php propagates THAT exception and never reports the class missing. */` |
|   100180 | 6073 | `						sxi32 nCbBrc = pVm->nBoundaryRc;` |
|   100180 | 6074 | `						const void *pCbRes = (const void *)pVm->pResumeFrame;` |
|   150268 | 6075 | `						zCbErr = VmDirectArrayCallableError(&(*pVm),pCbCls,pCbMeth,` |
|    50088 | 6076 | `							zCbMsg,sizeof(zCbMsg));` |
|   100180 | 6077 | `						if( zCbErr && PH7_VmClassLookupRaised(&(*pVm),nCbBrc,pCbRes) ){` |
|        6 | 6078 | `							bCbRaised = 1;` |
|        2 | 6079 | `						}` |
|        - | 6080 | `					}` |
|    50093 | 6081 | `				}` |
|   100332 | 6082 | `				if( !bCbScreened && (pCbMap == 0 \|\| pCbMap->nEntry != 2 \|\| zCbErr) ){` |
|        - | 6083 | `					sxi32 rcCb;` |
|       87 | 6084 | `					if( pInstr->iP2 ){` |
|      ! 0 | 6085 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 6086 | `					}` |
|       87 | 6087 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 6088 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6089 | `					}` |
|       87 | 6090 | `					PH7_MemObjRelease(pTos);` |
|       87 | 6091 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       87 | 6092 | `					pTos->nIdx = SXU32_HIGH;` |
|       87 | 6093 | `					if( bCbRaised ){` |
|        - | 6094 | `						/* Land the autoloader's own throw: consume the parked status (an` |
|        - | 6095 | `						 * in-place catch leaves 0 behind plus a recorded resume frame, which` |
|        - | 6096 | `						 * the router below picks up). */` |
|        6 | 6097 | `						rcCb = pVm->nBoundaryRc;` |
|        6 | 6098 | `						pVm->nBoundaryRc = 0;` |
|        6 | 6099 | `						if( rcCb == PH7_ABORT ){ goto Abort; }` |
|        6 | 6100 | `						rc = PH7_EXCEPTION;` |
|       18 | 6101 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6102 | `					}` |
|       82 | 6103 | `					if( zCbErr == 0 ){` |
|       17 | 6104 | `						zCbErr = "Array callback must have exactly two elements";` |
|        8 | 6105 | `					}` |
|       82 | 6106 | `					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));` |
|       82 | 6107 | `					if( rcCb == SXERR_ABORT ){ goto Abort; }` |
|       82 | 6108 | `					rc = rcCb;` |
|        - | 6109 | `					/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 6110 | `					 * (SXRET_OK + recorded resume) FELL THROUGH and kept dispatching` |
|        - | 6111 | `					 * the malformed callable inside the try. Route like OP_THROW. */` |
|      106 | 6112 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6113 | `				}` |
|        - | 6114 | `			}` |
|        - | 6115 | `			/* Build the effective spread-key map (and consume this call's runs)` |
|        - | 6116 | `			 * against this path's arg base (the array-callable slot isn't popped). */` |
|   150369 | 6117 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100244 | 6118 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6119 | `			/* Materialize the deferred arguments against the pair's own method (see` |
|        - | 6120 | `			 * VmIndirectCalleeFunc), not against a blanket by-ref assumption. */` |
|        - | 6121 | `			{` |
|   100247 | 6122 | `				sxi32 rcDA = VmResolveIndirectArgs(&(*pVm),pTos,pArg,pTos,pEffCallMap);` |
|   100247 | 6123 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6124 | `			}` |
|   100247 | 6125 | `			SySetReset(&aArg);` |
|   100425 | 6126 | `			while( pArg < pTos ){` |
|      180 | 6127 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      180 | 6128 | `				pArg++;` |
|        2 | 6129 | `			}` |
|   100247 | 6130 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 6131 | `			/* May be a class instance and it's static method. Forward this call's named-arg map` |
|        - | 6132 | ``			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —`` |
|        - | 6133 | `			 * mirroring the __invoke-object branch below. */` |
|   100247 | 6134 | `			pVm->bClosureScreened = bCbScreened; /* see the capture above */` |
|        - | 6135 | `			/* This call SITE is what decides the answer is dropped, and the` |
|        - | 6136 | `			 * dispatch below builds a synthetic OP_CALL that has no site of its` |
|        - | 6137 | `			 * own — hand the bit over on the latch, so a #[\NoDiscard] callee` |
|        - | 6138 | `			 * reached through an array callable, a "C::m" string or __invoke` |
|        - | 6139 | `			 * warns exactly as a directly-spelled one does. */` |
|   100247 | 6140 | `			pVm->bDiscardCallback = bResultDropped;` |
|   100247 | 6141 | `			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|        - | 6142 | `			/* Both latches are consumed by the method OP_CALL this dispatch builds;` |
|        - | 6143 | `			 * clear them here for the paths that never reach one. */` |
|   100247 | 6144 | `			pVm->bClosureScreened = 0;` |
|   100247 | 6145 | `			pVm->bDiscardCallback = 0;` |
|   100247 | 6146 | `			SySetReset(&aArg);` |
|        - | 6147 | `			/* Pop given arguments */` |
|   100247 | 6148 | `			if( nCallArgs > 0 ){` |
|      154 | 6149 | `				VmPopOperand(&pTos,nCallArgs);` |
|       76 | 6150 | `			}` |
|   100247 | 6151 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 | 6152 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 6153 | `				goto Abort;` |
|        - | 6154 | `			}` |
|   100247 | 6155 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - | 6156 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - | 6157 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - | 6158 | `				sxi32 iResumePc;` |
|   100014 | 6159 | `				PH7_MemObjRelease(&sResult);` |
|   100014 | 6160 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100004 | 6161 | `					PH7_MemObjRelease(pTos);` |
|        - | 6162 | ``					/* Drain the abandoned outer-expression operands (`1 + $cb()`)`` |
|        - | 6163 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 6164 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300006 | 6165 | `					PH7_RESUME_DRAIN()` |
|   100004 | 6166 | `					pc = iResumePc;` |
|   100004 | 6167 | `					break;` |
|        - | 6168 | `				}` |
|       11 | 6169 | `				goto Exception;` |
|        - | 6170 | `			}` |
|        - | 6171 | `			/* Copy result */` |
|      234 | 6172 | `			PH7_MemObjStore(&sResult,pTos);` |
|      234 | 6173 | `			PH7_MemObjRelease(&sResult);` |
|   200226 | 6174 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|   200094 | 6175 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - | 6176 | `			ph7_value sResult;` |
|        - | 6177 | `			sxi32 rcInv;` |
|        - | 6178 | `			/* __invoke object callable: the object slot isn't popped, so pArg is` |
|        - | 6179 | `			 * already this call's arg base — build the map + consume the runs. */` |
|   300139 | 6180 | `			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   200090 | 6181 | `				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6182 | `			/* Materialize the deferred arguments against this object's __invoke, the` |
|        - | 6183 | `			 * array-callable path's rule one shape over. */` |
|        - | 6184 | `			{` |
|   200094 | 6185 | `				sxi32 rcDA = VmResolveIndirectArgs(&(*pVm),pTos,pArg,pTos,pEffCallMap);` |
|   200094 | 6186 | `				PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6187 | `			}` |
|   200094 | 6188 | `			SySetReset(&aArg);` |
|   200188 | 6189 | `			while( pArg < pTos ){` |
|       98 | 6190 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       98 | 6191 | `				pArg++;` |
|        4 | 6192 | `			}` |
|   200094 | 6193 | `			PH7_MemObjInit(pVm,&sResult);` |
|   200094 | 6194 | `			pVm->bDiscardCallback = bResultDropped;   /* see the array-callable site */` |
|   300139 | 6195 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|   200090 | 6196 | `				(int)SySetUsed(&aArg),` |
|   200090 | 6197 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - | 6198 | `				&sResult,` |
|   100045 | 6199 | `				pEffCallMap);` |
|   200094 | 6200 | `			pVm->bDiscardCallback = 0;` |
|   200094 | 6201 | `			SySetReset(&aArg);` |
|        - | 6202 | `			/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable` |
|        - | 6203 | `			 * slot itself (it pops top-down and the callable IS pTos), which for a` |
|        - | 6204 | `			 * temporary like (new Plain())(...) holds the only reference — popping it` |
|        - | 6205 | `			 * would free pThis before VmRaiseNotCallable reads its class name below.` |
|        - | 6206 | `			 * Only the not-callable branch dereferences pThis afterwards, so pin just` |
|        - | 6207 | `			 * for that case; the matching PH7_ClassInstanceUnref drops it (destroying` |
|        - | 6208 | `			 * the temporary). The other branches let the pop free the temp as before. */` |
|   200094 | 6209 | `			if( rcInv == SXERR_INVALID ){` |
|   100006 | 6210 | `				pThis->iRef++;` |
|    50002 | 6211 | `			}` |
|   200094 | 6212 | `			if( nCallArgs > 0 ){` |
|       78 | 6213 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 | 6214 | `			}` |
|   200094 | 6215 | `			if( rcInv == SXERR_INVALID ){` |
|        - | 6216 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - | 6217 | `				 * sResult was already released by VmCallObjectInvoke. */` |
|   100006 | 6218 | `				PH7_MemObjRelease(pTos);` |
|   100006 | 6219 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|   100006 | 6220 | `				PH7_ClassInstanceUnref(pThis);` |
|   100006 | 6221 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 6222 | `					goto Abort;` |
|        - | 6223 | `				}` |
|        - | 6224 | `				{` |
|        - | 6225 | `					sxi32 iRp;` |
|   100006 | 6226 | `					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){` |
|        - | 6227 | `						/* Drain the abandoned outer-expression operands` |
|        - | 6228 | ``						 * (`1 + $plainObj()`) to the try's base — one leaked`` |
|        - | 6229 | `						 * slot per caught throw otherwise. */` |
|   300010 | 6230 | `						PH7_RESUME_DRAIN()` |
|   100006 | 6231 | `						pc = iRp;` |
|   100006 | 6232 | `						break;` |
|        - | 6233 | `					}` |
|        - | 6234 | `				}` |
|      ! 0 | 6235 | `				goto Exception;` |
|        - | 6236 | `			}` |
|   100090 | 6237 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 | 6238 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 | 6239 | `				goto Abort;` |
|        - | 6240 | `			}` |
|   100090 | 6241 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - | 6242 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - | 6243 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - | 6244 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - | 6245 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - | 6246 | `				sxi32 iResumePc;` |
|   100008 | 6247 | `				PH7_MemObjRelease(&sResult);` |
|   100008 | 6248 | `				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100006 | 6249 | `					PH7_MemObjRelease(pTos);` |
|        - | 6250 | ``					/* Drain the abandoned outer-expression operands (`1 + $inv()`)`` |
|        - | 6251 | `					 * to the try's base — one leaked slot per caught throw` |
|        - | 6252 | `					 * otherwise (ASan heap-buffer-overflow in a catch loop). */` |
|   300010 | 6253 | `					PH7_RESUME_DRAIN()` |
|   100006 | 6254 | `					pc = iResumePc;` |
|   100006 | 6255 | `					break;` |
|        - | 6256 | `				}` |
|        3 | 6257 | `				goto Exception;` |
|        - | 6258 | `			}` |
|       84 | 6259 | `			PH7_MemObjStore(&sResult,pTos);` |
|       84 | 6260 | `			PH7_MemObjRelease(&sResult);` |
|       44 | 6261 | `		}else{` |
|        - | 6262 | `			/* php: calling a non-callable is a catchable Error naming the type` |
|        - | 6263 | `			 * ("Value of type int is not callable"), or -- for an array -- the shape it` |
|        - | 6264 | `			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so` |
|        - | 6265 | ``			 * `$x()` on a number quietly evaluated to nothing. */`` |
|        - | 6266 | `			sxi32 rcNc;` |
|        - | 6267 | `			char zMsg[128];` |
|       17 | 6268 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 6269 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");` |
|      ! 0 | 6270 | `			}else{` |
|       25 | 6271 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",` |
|        8 | 6272 | `					VmArithTypeName(pTos));` |
|        - | 6273 | `			}` |
|        - | 6274 | `			/* Consume this call's captured spread runs — a non-callable target` |
|        - | 6275 | `			 * (int/float/bool/null) reaches no dispatch build site. */` |
|       17 | 6276 | `			if( pInstr->iP2 ){` |
|      ! 0 | 6277 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 6278 | `			}` |
|        - | 6279 | `			/* Pop given arguments */` |
|       17 | 6280 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 6281 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6282 | `			}` |
|        - | 6283 | `			/* Settle the call's result slot BEFORE throwing. */` |
|       17 | 6284 | `			PH7_MemObjRelease(pTos);` |
|       17 | 6285 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       17 | 6286 | `			pTos->nIdx = SXU32_HIGH;` |
|       17 | 6287 | `			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       17 | 6288 | `			if( rcNc == SXERR_ABORT ){ goto Abort; }` |
|       17 | 6289 | `			rc = rcNc;` |
|        - | 6290 | `			/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch` |
|        - | 6291 | `			 * (SXRET_OK + recorded resume) FELL THROUGH and resumed the try body` |
|        - | 6292 | `			 * right after the failed call. Route like OP_THROW. */` |
|       31 | 6293 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6294 | `		}` |
|      316 | 6295 | `		break;` |
|        - | 6296 | `	}` |
|  7079514 | 6297 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - | 6298 | `	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just` |
|        - | 6299 | `	 * anchors it to the global namespace — strip it before resolving so a` |
|        - | 6300 | ``	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */`` |
|  7079514 | 6301 | `	if( sName.nByte > 0 && sName.zString[0] == '\\' ){` |
|       15 | 6302 | `		sName.zString++;` |
|       15 | 6303 | `		sName.nByte--;` |
|        7 | 6304 | `	}` |
|        - | 6305 | `	/* Check for a compiled function first.` |
|        - | 6306 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 6307 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|  7079514 | 6308 | `	pEntry = PH7_VmGetUserFunction(pVm,(const void *)sName.zString,sName.nByte,bEngineCallee);` |
|        - | 6309 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - | 6310 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - | 6311 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - | 6312 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - | 6313 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - | 6314 | `	{` |
|  7079514 | 6315 | `	VmCallArgMap *pCallMap = pEffCallMap;` |
|  7079514 | 6316 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - | 6317 | `		const char *zFunc;` |
|        - | 6318 | `		const char *zEnd;` |
|        - | 6319 | `		const char *z;` |
|        - | 6320 | `		SyString sGlobal;` |
|       57 | 6321 | `		zFunc = sName.zString;` |
|       57 | 6322 | `		zEnd  = zFunc + sName.nByte;` |
|       57 | 6323 | `		z = zEnd;` |
|        - | 6324 | `		/* Find last namespace separator */` |
|      529 | 6325 | `		while( z > zFunc ){` |
|      529 | 6326 | `			if( z[-1] == '\\' ){` |
|       57 | 6327 | `				break;` |
|        - | 6328 | `			}` |
|      477 | 6329 | `			z--;` |
|        5 | 6330 | `		}` |
|       57 | 6331 | `		if( z > zFunc && z < zEnd ){` |
|        - | 6332 | `			/* Retry lookup using the unqualified/global function name */` |
|       57 | 6333 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       57 | 6334 | `			pEntry = PH7_VmGetUserFunction(pVm,(const void *)sGlobal.zString,sGlobal.nByte,bEngineCallee);` |
|       26 | 6335 | `		}` |
|       26 | 6336 | `	}` |
|        - | 6337 | `	} /* end VmCallArgMap namespace scope */` |
|  7079514 | 6338 | `	if( pEntry ){` |
|        - | 6339 | `		ph7_vm_func_arg *aFormalArg;` |
|        - | 6340 | `		ph7_class_instance *pThis;` |
|        - | 6341 | `		ph7_value *pFrameStack;` |
|        - | 6342 | `		ph7_vm_func *pVmFunc;` |
|        - | 6343 | `		ph7_class *pSelf;` |
|        - | 6344 | `		ph7_class *pSelfHint;` |
|        - | 6345 | `		VmFrame *pFrame;` |
|        - | 6346 | `		ph7_value *pObj;` |
|        - | 6347 | `		VmSlot sArg;` |
|        - | 6348 | `		sxu32 n;` |
|  2345659 | 6349 | `		sxi32 iArgPreFlags = 0; /* the actual's type before its declared-type check */` |
|  2345659 | 6350 | `		int bClosureThis = 0;` |
|  2345659 | 6351 | `		ph7_class *pClosureScope = 0;` |
|        - | 6352 | `		/* initialize fields */` |
|  2345659 | 6353 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|  2345659 | 6354 | `		pThis = 0;` |
|  2345659 | 6355 | `		pSelf = 0;` |
|        - | 6356 | `		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it` |
|        - | 6357 | `		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever` |
|        - | 6358 | `		 * set for a bound plain closure, which dispatches as a function, so the method branch below` |
|        - | 6359 | `		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */` |
|  2345659 | 6360 | `		if( pVm->pClosureThis ){` |
|       71 | 6361 | `			pThis = pVm->pClosureThis;` |
|       71 | 6362 | `			pVm->pClosureThis = 0;` |
|       71 | 6363 | `			bClosureThis = 1;` |
|       34 | 6364 | `		}` |
|  2345659 | 6365 | `		if( pVm->pClosureScope ){` |
|        - | 6366 | `			/* May ride alongside a bound $this, or stand alone for a` |
|        - | 6367 | ``			 * scope-only rebind (`bindTo(null, Scope::class)`). */`` |
|       61 | 6368 | `			pClosureScope = pVm->pClosureScope;` |
|       61 | 6369 | `			pVm->pClosureScope = 0;` |
|       29 | 6370 | `		}` |
|  2345659 | 6371 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - | 6372 | `			ph7_class_method *pMeth;` |
|        - | 6373 | `			/* Class method call */` |
|  2058077 | 6374 | `			ph7_value *pTarget = &pTos[-1];` |
|  2058077 | 6375 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - | 6376 | `				/* Extract the 'this' pointer */` |
|  2058077 | 6377 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - | 6378 | `					/* Instance already loaded */` |
|  1955203 | 6379 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|  1955203 | 6380 | `					pThis->iRef++;` |
|  1955203 | 6381 | `					pSelf = pThis->pClass;` |
|   977602 | 6382 | `				}` |
|  2058077 | 6383 | `				if( pSelf == 0 ){` |
|   102879 | 6384 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - | 6385 | `						/* "Late Static Binding" class name */` |
|   154307 | 6386 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|    51434 | 6387 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|    51434 | 6388 | `					}` |
|   102879 | 6389 | `					if( pSelf == 0 ){` |
|        7 | 6390 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 | 6391 | `					}` |
|    51437 | 6392 | `				}` |
|  2058077 | 6393 | `				if( pThis == 0  ){` |
|   102879 | 6394 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|   102879 | 6395 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   102879 | 6396 | `					if( pFrameLocal->pParent ){` |
|        - | 6397 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|      807 | 6398 | `						pThis = pFrameLocal->pThis;` |
|      807 | 6399 | `						if( pThis ){` |
|       62 | 6400 | `							pThis->iRef++;` |
|       30 | 6401 | `						}` |
|      401 | 6402 | `					}` |
|    51437 | 6403 | `				}` |
|  2058077 | 6404 | `				VmPopOperand(&pTos,1);` |
|  2058077 | 6405 | `				PH7_MemObjRelease(pTos);` |
|        - | 6406 | `				/* Synchronize pointers. The method-name slot popped above sat BETWEEN` |
|        - | 6407 | `				 * this call's arguments and the (already-removed) target — so only now` |
|        - | 6408 | `				 * is pTos one past the last actual argument. Re-derive the unpack` |
|        - | 6409 | `				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs` |
|        - | 6410 | `				 * from run ends, which the extra target slot would otherwise offset,` |
|        - | 6411 | ``				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */`` |
|  2058077 | 6412 | `				nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);` |
|  2058077 | 6413 | `				pArg = &pTos[-nCallArgs];` |
|        - | 6414 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - | 6415 | `				 * user have already computed the random generated unique class method name` |
|        - | 6416 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - | 6417 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - | 6418 | `				 */` |
|  2058077 | 6419 | `				while( pArg < pStack ){` |
|      ! 0 | 6420 | `					pArg++;` |
|      ! 0 | 6421 | `				}` |
|  2058077 | 6422 | `				if( pSelf && pVm->bReflectBypass ){` |
|        - | 6423 | `					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection` |
|        - | 6424 | `					 * ignores visibility. Consume-once so nested calls made by the` |
|        - | 6425 | `					 * invoked body are checked normally. */` |
|      275 | 6426 | `					pVm->bReflectBypass = 0;` |
|      138 | 6427 | `				}else` |
|  2057803 | 6428 | `				if( pSelf && bMagicDispatch && PH7_MagicMethodMustBePublic(&pVmFunc->sName) ){` |
|        - | 6429 | `					/* The ENGINE reaching for a magic method php requires to be public.` |
|        - | 6430 | `					 * php only WARNS at such a declaration and then dispatches the method` |
|        - | 6431 | ``					 * regardless -- the engine calling `__get` is not the outside world`` |
|        - | 6432 | `					 * reaching for a private member -- so a non-public` |
|        - | 6433 | ``					 * `__get`/`__call`/`__invoke`/`__sleep`/... still runs, where PHL threw`` |
|        - | 6434 | ``					 * `Call to private method C::__get()` at the ACCESS and killed the`` |
|        - | 6435 | `					 * script.` |
|        - | 6436 | `					 *` |
|        - | 6437 | `					 * The latch is set by PH7_VmCallMagicMethod at the engine's own` |
|        - | 6438 | `					 * dispatch sites. It is NOT inferred from the instruction: a` |
|        - | 6439 | ``					 * first-class `$o->__get(...)` and `$o->__get('x')` reach the same C`` |
|        - | 6440 | `					 * dispatcher, and php denies both. */` |
|    53729 | 6441 | `				}else` |
|  1950356 | 6442 | `				if( pSelf && !bMemberScreened ){ /* Paranoid edition */` |
|        - | 6443 | `					/* Check if the call is allowed. php binds non-public method` |
|        - | 6444 | `					 * access by the DECLARING class (pVmFunc->pUserData — the class` |
|        - | 6445 | `					 * the callee was compiled in), NOT the instance's class: an` |
|        - | 6446 | `					 * inherited base method calling $this->priv() on a child` |
|        - | 6447 | `					 * instance passes, a child's private SHADOW doesn't hijack the` |
|        - | 6448 | `					 * check for a parent callee, and the denial message names the` |
|        - | 6449 | `					 * declaring class like php. */` |
|  1802829 | 6450 | `					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;` |
|        - | 6451 | `					ph7_class *pOwnerClass;` |
|  1802829 | 6452 | `					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|  1802829 | 6453 | `					if( pMeth == 0 && pDeclClass != pSelf ){` |
|      ! 0 | 6454 | `						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      ! 0 | 6455 | `					}` |
|  1802829 | 6456 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        - | 6457 | `						/* ...except that a TRAIT is not a class php still has at run time: it` |
|        - | 6458 | `						 * composed the method INTO the using class, so that class owns the` |
|        - | 6459 | `						 * rule and the name. Deciding against the trait refused a protected` |
|        - | 6460 | `						 * trait method to a SUBCLASS of the composing class (which uses no` |
|        - | 6461 | ``						 * trait of its own) — `class Az { use Tz; } class Bz extends Az {`` |
|        - | 6462 | ``						 * $this->pr(); }` was a fatal php runs. Identity for every non-trait`` |
|        - | 6463 | `						 * method. */` |
|       27 | 6464 | `						pOwnerClass = PH7_VmMethodScopeName(&(*pVm),pSelf,pMeth);` |
|       27 | 6465 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - | 6466 | `							/* php throws a CATCHABLE Error here. The old code merely PRINTED an` |
|        - | 6467 | ``							 * uncaught-exception report and aborted, so `try { $o->priv(); }`` |
|        - | 6468 | ``							 * catch (Error $e)` never caught it and the script died. */`` |
|        - | 6469 | `							char zMsg[256];` |
|        - | 6470 | `							sxi32 rcVis;` |
|        - | 6471 | `							/* php NAMES the calling scope when there is one — "from scope C" —` |
|        - | 6472 | `							 * and says "global scope" only outside every class; the wording is` |
|        - | 6473 | `							 * shared with the first-class-callable screen` |
|        - | 6474 | `							 * (VmMethodVisibilityMsg). */` |
|       19 | 6475 | `							VmMethodVisibilityMsg(&(*pVm),pOwnerClass,` |
|        6 | 6476 | `								pVmFunc->sName.zString,pVmFunc->sName.nByte,` |
|        6 | 6477 | `								pMeth->iProtection,zMsg,sizeof(zMsg));` |
|        - | 6478 | `							/* Consume this call's captured spread runs — this visibility` |
|        - | 6479 | `							 * error exits before the pVmFunc build below. */` |
|       13 | 6480 | `							if( pInstr->iP2 ){` |
|      ! 0 | 6481 | `								VmSpreadConsume(pVm);` |
|      ! 0 | 6482 | `							}` |
|        - | 6483 | `							/* Pop given arguments, and leave the call's NULL result behind. */` |
|       13 | 6484 | `							if( nCallArgs > 0 ){` |
|      ! 0 | 6485 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6486 | `							}` |
|       13 | 6487 | `							PH7_MemObjRelease(pTos);` |
|       13 | 6488 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|       13 | 6489 | `							pTos->nIdx = SXU32_HIGH;` |
|       13 | 6490 | `							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));` |
|       13 | 6491 | `							if( rcVis == SXERR_ABORT ){ goto Abort; }` |
|       13 | 6492 | `							rc = rcVis;` |
|        - | 6493 | `							/* ENFORCE_RC never consulted pResumeFrame, so an in-place` |
|        - | 6494 | `							 * catch (SXRET_OK + recorded resume) FELL THROUGH and` |
|        - | 6495 | `							 * dispatched the DENIED method anyway. Route like OP_THROW. */` |
|       13 | 6496 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 6497 | `						}` |
|        7 | 6498 | `					}` |
|   901407 | 6499 | `				}` |
|  1029033 | 6500 | `			}` |
|  1029033 | 6501 | `		}` |
|        - | 6502 | `		/* pArg is now finalized for every pVmFunc callee (a method call popped its` |
|        - | 6503 | `		 * method-name slot above; functions/closures/generators keep the top base).` |
|        - | 6504 | `		 * Build the PHP 8.1 effective spread-key map here so the generator and the` |
|        - | 6505 | `		 * install path below both see it — and so this call's captured runs are` |
|        - | 6506 | `		 * consumed exactly once, against the correct base. */` |
|  3518686 | 6507 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  2345642 | 6508 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|        - | 6509 | `		/* Check the PHP call-depth cap (the sole site — BYTECODE.md stage 5).` |
|        - | 6510 | `		 * Default is unbounded (heap-bound recursion, decoupled from the C stack` |
|        - | 6511 | `		 * by the stage-2 trampoline); the C stack is guarded separately by` |
|        - | 6512 | `		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and` |
|        - | 6513 | `		 * then raises a clean non-catchable fatal (was: silently set NULL and` |
|        - | 6514 | `		 * continue) and halts. */` |
|  2345647 | 6515 | `		if( VmRecursionExceeded(pVm) ){` |
|        - | 6516 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - | 6517 | `			 * which walks the whole operand stack — don't release them here. */` |
|        3 | 6518 | `			VmRecursionFatal(&(*pVm));` |
|        3 | 6519 | `			goto Abort;` |
|        - | 6520 | `		}` |
|  2345645 | 6521 | `		if( pVmFunc->pNextName ){` |
|        - | 6522 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      289 | 6523 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|      142 | 6524 | `		}` |
|        - | 6525 | ``		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's`` |
|        - | 6526 | ``		 * `Class::method`). Computed after overload resolution so it reflects the selected method.`` |
|        - | 6527 | ``		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed`` |
|        - | 6528 | `		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method` |
|        - | 6529 | ``		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the`` |
|        - | 6530 | `		 * USING class; we don't carry the using class on the shared struct, so fall back to the` |
|        - | 6531 | `		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for` |
|        - | 6532 | `		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */` |
|  2345645 | 6533 | `		pSelfHint = pSelf;` |
|  2345645 | 6534 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|  2058065 | 6535 | `			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;` |
|  2058065 | 6536 | `			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|  2057757 | 6537 | `				pSelfHint = pDecl;` |
|  1028879 | 6538 | `			}` |
|  1029033 | 6539 | `		}` |
|  2345645 | 6540 | `		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - | 6541 | `			/* Push the closure's called-class as this frame's LSB class so` |
|        - | 6542 | ``			 * `static::` inside the body resolves like php. An explicit`` |
|        - | 6543 | `			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use` |
|        - | 6544 | `			 * the class captured at the closure's creation site. self::/parent::` |
|        - | 6545 | `			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done` |
|        - | 6546 | ``			 * after pSelfHint so a `self`-typed param is unaffected. */`` |
|    25047 | 6547 | `			if( pClosureScope ){` |
|       61 | 6548 | `				pSelf = pClosureScope;` |
|    25018 | 6549 | `			}else if( pVmFunc->pLsbClass ){` |
|      147 | 6550 | `				pSelf = (ph7_class *)pVmFunc->pLsbClass;` |
|       71 | 6551 | `			}` |
|    12516 | 6552 | `		}` |
|  2345645 | 6553 | `		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){` |
|        - | 6554 | `			/* php 8.4 #[\Deprecated] runtime notice — once per call, before` |
|        - | 6555 | `			 * execution (generators: at the g(...) call site, like php). */` |
|      581 | 6556 | `			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,` |
|      384 | 6557 | `				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);` |
|      192 | 6558 | `		}` |
|        - | 6559 | `		/* php 8.5 #[\NoDiscard]: the CALL SITE decided this answer is thrown away` |
|        - | 6560 | ``		 * (the codegen's bDiscard, php's !RETURN_VALUE_USED), and a `(void)` cast`` |
|        - | 6561 | `		 * in front of the statement is what clears it. Raised before the body, so` |
|        - | 6562 | `		 * a callee that throws has already warned — php's order. */` |
|  2345645 | 6563 | `		if( (pVmFunc->iFlags & VM_FUNC_NODISCARD) && bResultDropped ){` |
|        - | 6564 | `			/* php calls it a "method" whenever the callee has a class SCOPE, and a` |
|        - | 6565 | ``			 * closure declared in a class body has one -- `C::{closure:C::go():3}`. */`` |
|       57 | 6566 | `			ph7_class *pNdClass = 0;` |
|       57 | 6567 | `			if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|       21 | 6568 | `				pNdClass = pSelfHint;` |
|       47 | 6569 | `			}else if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        7 | 6570 | `				pNdClass = (ph7_class *)pVmFunc->pLsbClass;` |
|        3 | 6571 | `			}` |
|       57 | 6572 | `			VmNoDiscardWarn(&(*pVm),pVmFunc,pNdClass);` |
|       28 | 6573 | `		}` |
|        - | 6574 | `		/* D1: resolve deferred plain-var arguments against this callee's declaration` |
|        - | 6575 | `		 * now — BEFORE the native/generator splits and VmEnterFrame, while pVm->pFrame is` |
|        - | 6576 | `		 * still the caller. pVmFunc is final here (post-overload). Covers plain functions,` |
|        - | 6577 | `		 * methods, closures, dynamic-name calls, generators and native methods uniformly;` |
|        - | 6578 | `		 * only the SOURCE of the by-ref positions differs between the last one and the` |
|        - | 6579 | `		 * rest (a signature string vs. compiled formal parameters). */` |
|        - | 6580 | `		{` |
|        - | 6581 | `			sxi32 rcDA;` |
|  2345645 | 6582 | `			if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 6583 | `				/* A native method declares no formal parameters to match against —` |
|        - | 6584 | `				 * its by-ref positions come from the same signature-derived mask a` |
|        - | 6585 | `				 * builtin uses, so resolve them the builtin way. Sharing this one` |
|        - | 6586 | `				 * site (rather than repeating it in the branch below) keeps the` |
|        - | 6587 | `				 * throw routing identical for both kinds of callee. */` |
|  2327552 | 6588 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
|  1551696 | 6589 | `					pVmFunc->pNative->nByRefMask,0,0,pEffCallMap);` |
|   775856 | 6590 | `			}else{` |
|  1191136 | 6591 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|   793944 | 6592 | `					(ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs),SySetUsed(&pVmFunc->aArgs),` |
|   397187 | 6593 | `					0,0,0,pEffCallMap);` |
|        - | 6594 | `			}` |
|  2345657 | 6595 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 6596 | `		}` |
|  2345603 | 6597 | `		if( pVmFunc->iFlags & VM_FUNC_NATIVE ){` |
|        - | 6598 | `			/* C-bodied method (VM_FUNC_NATIVE): hand it to the foreign-function path.` |
|        - | 6599 | `			 *` |
|        - | 6600 | `			 * Everything a METHOD needs has already happened above — the sVmName` |
|        - | 6601 | `			 * lookup, overload selection, $this / self / late-static-binding` |
|        - | 6602 | `			 * resolution, the visibility check, the #[\Deprecated] notice, deferred` |
|        - | 6603 | `			 * argument materialization — and everything BELOW is exactly what a C` |
|        - | 6604 | `			 * body has no use for: a frame, an operand stack, a call record.` |
|        - | 6605 | `			 *` |
|        - | 6606 | `			 * The stack shape already matches a builtin's, because the method branch` |
|        - | 6607 | `			 * popped both the method-name slot and the target slot: [pArg,pTos) are` |
|        - | 6608 | `			 * this call's arguments and pTos is the slot that receives the result.` |
|        - | 6609 | `			 * So the jump lands on shared code, not a copy of it. */` |
|  1551701 | 6610 | `			pFunc = pVmFunc->pNative;` |
|  1551701 | 6611 | `			pNativeOwned = pThis; /* borrowed; the shared tail drops this reference */` |
|        - | 6612 | ``			/* A `C::m()` call reaches the method path with no object in the target`` |
|        - | 6613 | `			 * slot, and the path then adopts the CALLER's $this so an instance-context` |
|        - | 6614 | ``			 * `self::m()` still finds one. A native STATIC method must not see that —`` |
|        - | 6615 | `			 * it would read its argument list one slot off — so the receiver handed to` |
|        - | 6616 | `			 * the body is gated on the declaration, not on what the fallback found. */` |
|  1551701 | 6617 | `			pNativeRecv = (pVmFunc->iFlags & VM_FUNC_NATIVE_STATIC) ? 0 : pThis;` |
|  1551701 | 6618 | `			pNativeClass = pSelf;` |
|  1551701 | 6619 | `			goto NativeCall;` |
|        - | 6620 | `		}` |
|   793907 | 6621 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - | 6622 | `			/* Generator function: return a Generator object instead of executing */` |
|        - | 6623 | `			ph7_exec_ctx *pExecCtx;` |
|        - | 6624 | `			ph7_generator *pGenerator;` |
|        - | 6625 | `			ph7_class_instance *pGenObj;` |
|        - | 6626 | `			ph7_value *pCtxAttr;` |
|        - | 6627 | `			SyString sAttrName;` |
|        - | 6628 | `			ph7_value **apCallArgs;` |
|        - | 6629 | `			int nGenArgs, iArg;` |
|        - | 6630 | `			/* Collect arguments from the operand stack */` |
|      475 | 6631 | `			nGenArgs = (int)(pTos - pArg);` |
|      475 | 6632 | `			apCallArgs = 0;` |
|      475 | 6633 | `			if( nGenArgs > 0 ){` |
|        - | 6634 | `				/* php refuses a non-variable in a by-ref position at the CALL, and for` |
|        - | 6635 | `				 * a generator this IS the call. Routed like the branch's other` |
|        - | 6636 | `				 * pre-frame throws below: no callee frame exists yet, so drop the` |
|        - | 6637 | `				 * arguments plus the function-name slot and land the enclosing try. */` |
|      185 | 6638 | `				rc = VmScreenGenByRefArgs(&(*pVm),pVmFunc,pEffCallMap,pArg,` |
|       60 | 6639 | `					(sxu32)nGenArgs,pSelfHint);` |
|      125 | 6640 | `				if( rc != SXRET_OK ){` |
|        7 | 6641 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 6642 | `						goto Abort;` |
|        - | 6643 | `					}` |
|      239 | 6644 | `					PH7_INLINE_RESUME_BREAK()` |
|        7 | 6645 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6646 | `					{` |
|        - | 6647 | `						sxi32 iRpB;` |
|        7 | 6648 | `						if( VmRecordedResume(pVm,&iRpB,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 6649 | `							pc = iRpB;` |
|      ! 0 | 6650 | `							break;` |
|        - | 6651 | `						}` |
|        - | 6652 | `					}` |
|        7 | 6653 | `					goto Exception;` |
|        - | 6654 | `				}` |
|       57 | 6655 | `			}` |
|      469 | 6656 | `			if( nGenArgs > 0 ){` |
|      176 | 6657 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       57 | 6658 | `					nGenArgs * sizeof(ph7_value *));` |
|      119 | 6659 | `				if( apCallArgs == 0 ){` |
|        - | 6660 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 | 6661 | `					nGenArgs = 0;` |
|      ! 0 | 6662 | `				}else{` |
|      119 | 6663 | `					VmCallArgMap *pGenMap = pEffCallMap;` |
|      119 | 6664 | `					int didReorder = 0;` |
|      119 | 6665 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - | 6666 | `						/* Named-argument reordering for generator */` |
|       15 | 6667 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|       15 | 6668 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|       15 | 6669 | `						sxu32 nNV = nF;` |
|       15 | 6670 | `						sxi32 iVIdx = -1;` |
|        - | 6671 | `						sxi32 *aGSlot;` |
|        - | 6672 | `						sxu8 *aGUsed;` |
|        - | 6673 | `						sxu32 gi;` |
|       33 | 6674 | `						for( gi = 0; gi < nF; gi++ ){` |
|       21 | 6675 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|       12 | 6676 | `						}` |
|       21 | 6677 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       12 | 6678 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|       15 | 6679 | `						if( aGSlot ){` |
|       15 | 6680 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|       21 | 6681 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        6 | 6682 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|       15 | 6683 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 6684 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 6685 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6686 | `								goto Abort;` |
|        - | 6687 | `							}` |
|       15 | 6688 | `							if( rc == PH7_EXCEPTION ){` |
|        - | 6689 | `								/* A named-argument error is php's catchable \Error.` |
|        - | 6690 | `								 * No callee frame exists yet on this branch (the` |
|        - | 6691 | `								 * generator body never runs and VmEnterFrame is` |
|        - | 6692 | `								 * further down), so route it like the other` |
|        - | 6693 | `								 * pre-frame OP_CALL throws: drop the args + the` |
|        - | 6694 | `								 * function-name slot and land the enclosing try. */` |
|        3 | 6695 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        3 | 6696 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        3 | 6697 | `								PH7_INLINE_RESUME_BREAK()` |
|        3 | 6698 | `								VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6699 | `								{` |
|        - | 6700 | `									sxi32 iRpN;` |
|        3 | 6701 | `									if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){` |
|        3 | 6702 | `										pc = iRpN;` |
|        3 | 6703 | `										break;` |
|        - | 6704 | `									}` |
|        - | 6705 | `								}` |
|      ! 0 | 6706 | `								goto Exception;` |
|        - | 6707 | `							}` |
|        - | 6708 | `							{` |
|        - | 6709 | `								/* php's named-hole ArgumentCountError, checked BEFORE` |
|        - | 6710 | `								 * hole compaction: compacting first would report the` |
|        - | 6711 | `								 * positional wording with a fictitious count (g(b:2)` |
|        - | 6712 | ``								 * must be `g(): Argument #1 ($a) not passed`, not`` |
|        - | 6713 | `								 * "1 passed"). Implicit-required watermark, like the` |
|        - | 6714 | `								 * plain-call named path; a hole with NOTHING filled` |
|        - | 6715 | `								 * above it keeps php's count wording — fall through` |
|        - | 6716 | `								 * to VmFiberSetupFrame's check (the compacted count` |
|        - | 6717 | `								 * equals php's num_args there). */` |
|       13 | 6718 | `								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;` |
|       13 | 6719 | `								sxi32 iHole = -1;` |
|       13 | 6720 | `								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);` |
|       29 | 6721 | `								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){` |
|       19 | 6722 | `									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){` |
|       15 | 6723 | `										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);` |
|        6 | 6724 | `									}` |
|       11 | 6725 | `								}` |
|       27 | 6726 | `								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){` |
|        - | 6727 | `									sxu32 gj;` |
|       17 | 6728 | `									int bFound = 0;` |
|       23 | 6729 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       23 | 6730 | `										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }` |
|        5 | 6731 | `									}` |
|       17 | 6732 | `									if( !bFound && gHole + 1 <= nMaxFilledG ){` |
|      ! 0 | 6733 | `										iHole = (sxi32)gHole;` |
|      ! 0 | 6734 | `									}` |
|       10 | 6735 | `								}` |
|       13 | 6736 | `								if( iHole >= 0 ){` |
|      ! 0 | 6737 | `									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 6738 | `										(sxu32)iHole+1,&aFA[iHole].sName);` |
|      ! 0 | 6739 | `									SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 | 6740 | `									SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6741 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 6742 | `										goto Abort;` |
|        - | 6743 | `									}` |
|        - | 6744 | `									/* Route like the VmFiberSetupFrame throw below */` |
|      ! 0 | 6745 | `									PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 6746 | `									VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6747 | `									{` |
|        - | 6748 | `										sxi32 iRpH;` |
|      ! 0 | 6749 | `										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){` |
|      ! 0 | 6750 | `											pc = iRpH;` |
|      ! 0 | 6751 | `											break;` |
|        - | 6752 | `										}` |
|        - | 6753 | `									}` |
|      ! 0 | 6754 | `									goto Exception;` |
|        - | 6755 | `								}` |
|        - | 6756 | `							}` |
|        - | 6757 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - | 6758 | `							 * append overflow (variadic / positional beyond` |
|        - | 6759 | `							 * formals) so downstream sees every argument. */` |
|        - | 6760 | `							{` |
|       13 | 6761 | `								int nOut = 0;` |
|       29 | 6762 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - | 6763 | `									sxu32 gj;` |
|       25 | 6764 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       25 | 6765 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|       19 | 6766 | `											apCallArgs[nOut++] = &pArg[gj];` |
|       19 | 6767 | `											break;` |
|        - | 6768 | `										}` |
|        5 | 6769 | `									}` |
|       11 | 6770 | `								}` |
|       29 | 6771 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|       19 | 6772 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 | 6773 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 | 6774 | `									}` |
|       11 | 6775 | `								}` |
|       13 | 6776 | `								nGenArgs = nOut;` |
|        - | 6777 | `							}` |
|       13 | 6778 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|       13 | 6779 | `							didReorder = 1;` |
|        5 | 6780 | `						}` |
|        - | 6781 | `						/* If aGSlot allocation failed, fall through to` |
|        - | 6782 | `						 * positional fill below — preserves arg order rather` |
|        - | 6783 | `						 * than passing an uninitialized apCallArgs. */` |
|        5 | 6784 | `					}` |
|      117 | 6785 | `					if( !didReorder ){` |
|      217 | 6786 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|      115 | 6787 | `							apCallArgs[iArg] = &pArg[iArg];` |
|       60 | 6788 | `						}` |
|       51 | 6789 | `					}` |
|        - | 6790 | `				}` |
|       56 | 6791 | `			}` |
|        - | 6792 | `			/* Create execution context and generator wrapper */` |
|      467 | 6793 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|      467 | 6794 | `			if( pExecCtx == 0 ){` |
|      ! 0 | 6795 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6796 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 6797 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 6798 | `				break;` |
|        - | 6799 | `			}` |
|      467 | 6800 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|      467 | 6801 | `			if( pGenerator == 0 ){` |
|      ! 0 | 6802 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 | 6803 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 | 6804 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 | 6805 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 | 6806 | `				break;` |
|        - | 6807 | `			}` |
|        - | 6808 | `			/* Set up the frame with arguments, closure env, $this */` |
|      467 | 6809 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|      467 | 6810 | `			pVm->pFrame = pExecCtx->pFrame;` |
|      929 | 6811 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,` |
|      462 | 6812 | `				pEffCallMap ? (pEffCallMap->bStrict ? 1 : 0) : (pVm->bCurStrict ? 1 : 0),` |
|      231 | 6813 | `				pSelfHint,` |
|        - | 6814 | `				TRUE/*generator: the g(...) call site is in the message*/,` |
|        - | 6815 | `				TRUE/*a source-level call binds a by-ref parameter to the caller's slot*/);` |
|      467 | 6816 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|      467 | 6817 | `			pExecCtx->pFrame->pParent = 0;` |
|      467 | 6818 | `			if( apCallArgs ){` |
|      117 | 6819 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|       56 | 6820 | `			}` |
|      467 | 6821 | `			if( rc != SXRET_OK ){` |
|       18 | 6822 | `				VmReleaseGenerator(pVm, pGenerator);` |
|       18 | 6823 | `				if( pThis ){` |
|        3 | 6824 | `					PH7_ClassInstanceUnref(pThis);` |
|        1 | 6825 | `				}` |
|       18 | 6826 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 6827 | `					goto Abort;` |
|        - | 6828 | `				}` |
|       18 | 6829 | `				if( rc == PH7_EXCEPTION ){` |
|        - | 6830 | `					/* A declared-type TypeError thrown while binding the` |
|        - | 6831 | `					 * generator's arguments (php binds + type-checks eagerly at` |
|        - | 6832 | `					 * the g(...) call site, before any resume — band A #2). If` |
|        - | 6833 | `					 * an inline try THIS exec owns caught it, land at its` |
|        - | 6834 | `					 * redirect (the drain subsumes the operand pops); else pop` |
|        - | 6835 | `					 * the args + function name and route like the other` |
|        - | 6836 | `					 * OP_CALL throw paths. */` |
|       22 | 6837 | `					PH7_INLINE_RESUME_BREAK()` |
|       16 | 6838 | `					VmPopOperand(&pTos,nCallArgs + 1);` |
|        - | 6839 | `					{` |
|        - | 6840 | `						sxi32 iRpG;` |
|       16 | 6841 | `						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){` |
|       16 | 6842 | `							pc = iRpG;` |
|       16 | 6843 | `							break;` |
|        - | 6844 | `						}` |
|        - | 6845 | `					}` |
|      ! 0 | 6846 | `					goto Exception;` |
|        - | 6847 | `				}` |
|      ! 0 | 6848 | `				break;` |
|        - | 6849 | `			}` |
|        - | 6850 | `			/* Create Generator class instance */` |
|      451 | 6851 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|      451 | 6852 | `			if( pGenObj == 0 ){` |
|      ! 0 | 6853 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 | 6854 | `				break;` |
|        - | 6855 | `			}` |
|        - | 6856 | `			/* Store generator in __ctx attribute */` |
|      451 | 6857 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      451 | 6858 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|      451 | 6859 | `			if( pCtxAttr ){` |
|      451 | 6860 | `				pCtxAttr->x.pOther = pGenerator;` |
|      451 | 6861 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      223 | 6862 | `			}` |
|        - | 6863 | `			/* Pop args and function name, push Generator object. PH7_NewClassInstance` |
|        - | 6864 | `			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do` |
|        - | 6865 | `			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope` |
|        - | 6866 | ``			 * and its __destruct (which runs pending `finally` blocks and frees the`` |
|        - | 6867 | `			 * exec context) never fires. */` |
|      451 | 6868 | `			PH7_MemObjRelease(pTos);` |
|      451 | 6869 | `			pTos = &pTos[-nCallArgs];` |
|      451 | 6870 | `			pTos->x.pOther = pGenObj;` |
|      451 | 6871 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|      451 | 6872 | `			if( pThis ){` |
|       16 | 6873 | `				PH7_ClassInstanceUnref(pThis);` |
|        6 | 6874 | `			}` |
|      451 | 6875 | `			break;` |
|        - | 6876 | `		}` |
|        - | 6877 | `		/* Extract the formal argument set */` |
|   793437 | 6878 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - | 6879 | `		/* Create a new VM frame  */` |
|   793437 | 6880 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|   793437 | 6881 | `		if( rc != SXRET_OK ){` |
|        - | 6882 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 6883 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 6884 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 6885 | `				&pVmFunc->sName);` |
|        - | 6886 | `			/* The frame that would own (and later release) $this never got created; for a bound` |
|        - | 6887 | `			 * plain closure the consumed transient is the object's ONLY ref, so release it here` |
|        - | 6888 | `			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */` |
|      ! 0 | 6889 | `			if( bClosureThis && pThis ){` |
|      ! 0 | 6890 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 | 6891 | `			}` |
|        - | 6892 | `			/* Pop given arguments */` |
|      ! 0 | 6893 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 6894 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 6895 | `			}` |
|        - | 6896 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 | 6897 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 | 6898 | `			break;` |
|        - | 6899 | `		}` |
|   793437 | 6900 | `		if( pClosureScope ){` |
|        - | 6901 | `			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /` |
|        - | 6902 | `			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected` |
|        - | 6903 | `			 * access inside the body resolves against it. */` |
|       59 | 6904 | `			pFrame->pBoundScope = pClosureScope;` |
|       28 | 6905 | `		}` |
|        - | 6906 | `		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band` |
|        - | 6907 | `		 * A #4): sArg over-counts (defaulted params installed, variadic packed` |
|        - | 6908 | `		 * as one entry) so php's answers can't be derived from it. */` |
|   793437 | 6909 | `		pFrame->nActualArgs = (int)(pTos - pArg);` |
|   793437 | 6910 | `		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) \|\| bClosureThis) ){` |
|        - | 6911 | `			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */` |
|        - | 6912 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|   405903 | 6913 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|   405903 | 6914 | `			if( pObj ){` |
|        - | 6915 | `				/* Reflect the change */` |
|   405903 | 6916 | `				pObj->x.pOther = pThis;` |
|   405903 | 6917 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   202949 | 6918 | `			}` |
|   202949 | 6919 | `		}` |
|   793437 | 6920 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - | 6921 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - | 6922 | `			/* Install static variables */` |
|       78 | 6923 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      154 | 6924 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|       78 | 6925 | `				pStatic = &aStatic[n];` |
|       78 | 6926 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - | 6927 | `					/* Initialize the static variables */` |
|       34 | 6928 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|       34 | 6929 | `					if( pObj ){` |
|        - | 6930 | `						/* Assume a NULL initialization value */` |
|       34 | 6931 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|       34 | 6932 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - | 6933 | `							/* Evaluate initialization expression (Any complex expression) */` |
|       28 | 6934 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|       13 | 6935 | `						}` |
|       34 | 6936 | `						pObj->nIdx = pStatic->nIdx;` |
|        - | 6937 | `						/* Permanent pin: the storage outlives every call */` |
|       34 | 6938 | `						VmPinMemObjSlot(&(*pVm),pStatic->nIdx);` |
|       18 | 6939 | `					}else{` |
|      ! 0 | 6940 | `						continue;` |
|        - | 6941 | `					}` |
|       16 | 6942 | `				}` |
|        - | 6943 | `				/* Install in the current frame — a REGISTERED binding, and the slot is` |
|        - | 6944 | `				 * PINNED: the static's storage belongs to the function, not to this` |
|        - | 6945 | `				 * call, so neither the frame teardown nor an unset of the NAME may` |
|        - | 6946 | `				 * recycle it. Poking hVar directly left the binding invisible to the` |
|        - | 6947 | ``				 * reference table, so an array element sharing the static (`[&$s]`)`` |
|        - | 6948 | ``				 * did not count as a reference and `unset($s)` destroyed the storage —`` |
|        - | 6949 | `				 * the next call started over from the initializer. The pin is taken ONCE,` |
|        - | 6950 | `				 * where the slot is created (above). */` |
|      116 | 6951 | `				PH7_VmBindVarSlot(&(*pVm),pFrame,SyStringData(&pStatic->sName),` |
|       38 | 6952 | `					SyStringLength(&pStatic->sName),pStatic->nIdx);` |
|       40 | 6953 | `			}` |
|       38 | 6954 | `		}` |
|        - | 6955 | `		/* Push arguments in the local frame */` |
|        - | 6956 | `		{` |
|   793437 | 6957 | `		VmCallArgMap *pCallMap3 = pEffCallMap;` |
|        - | 6958 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - | 6959 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|        - | 6960 | `		/* A compiled call in a strict file always carries a map with bStrict set` |
|        - | 6961 | `		 * (GenStateAttachStrictFlag); with no map at all the call is either a` |
|        - | 6962 | `		 * compiled WEAK one — where bCurStrict is 0 for the same unit — or an` |
|        - | 6963 | `		 * ENGINE-dispatched one, whose synthetic OP_CALL has no map to carry the` |
|        - | 6964 | `		 * caller's mode. php scopes parameter coercion by the CALLING file either` |
|        - | 6965 | `		 * way, and bCurStrict is that file's mode.` |
|        - | 6966 | `		 *` |
|        - | 6967 | `		 * Unless an INTERNAL function is what reached for this callback (bCallbackWeak):` |
|        - | 6968 | `		 * php has no calling file at that boundary and binds weakly, so` |
|        - | 6969 | ``		 * `array_map('takesInt', ["5"])` from a strict file RUNS there — PHL raised a`` |
|        - | 6970 | `		 * TypeError on valid php, because the ambient bCurStrict was still the strict` |
|        - | 6971 | `		 * caller's. call_user_func / call_user_func_array are php's two forwards and` |
|        - | 6972 | `		 * carry the caller's mode on a map instead. */` |
|  1189928 | 6973 | `		int bCallIsStrict = pCallMap3 ? (pCallMap3->bStrict ? 1 : 0)` |
|   721294 | 6974 | `		                   : (bCallbackWeak ? 0 : (pVm->bCurStrict ? 1 : 0));` |
|   793437 | 6975 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - | 6976 | `			/* ============================================================` |
|        - | 6977 | `			 * Named-argument matching path (PHP 8.0)` |
|        - | 6978 | `			 *` |
|        - | 6979 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - | 6980 | `			 * or position, then install them in the frame.` |
|        - | 6981 | `			 * ============================================================ */` |
|      391 | 6982 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|      391 | 6983 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|      391 | 6984 | `			sxi32 iVariadicIdx = -1;` |
|        - | 6985 | `			sxu32 nNonVariadic;` |
|        - | 6986 | `			sxi32 *aSlot;` |
|        - | 6987 | `			sxu8  *aUsed;` |
|        - | 6988 | `			sxu32 i;` |
|        - | 6989 | `			/* Find variadic parameter index */` |
|     1011 | 6990 | `			for( i = 0; i < nFormal; i++ ){` |
|      733 | 6991 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|      112 | 6992 | `					iVariadicIdx = (sxi32)i;` |
|      112 | 6993 | `					break;` |
|        - | 6994 | `				}` |
|      315 | 6995 | `			}` |
|      391 | 6996 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - | 6997 | `			/* Allocate mapping arrays */` |
|      584 | 6998 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      386 | 6999 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|      391 | 7000 | `			if( aSlot == 0 ){` |
|      ! 0 | 7001 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 | 7002 | `				goto Abort;` |
|        - | 7003 | `			}` |
|      391 | 7004 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - | 7005 | `			/* Resolve named arguments to formal parameters */` |
|      584 | 7006 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|      193 | 7007 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|      391 | 7008 | `			if( rc == PH7_ABORT ){` |
|        8 | 7009 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 | 7010 | `				goto Abort;` |
|        - | 7011 | `			}` |
|      385 | 7012 | `			if( rc == PH7_EXCEPTION ){` |
|        - | 7013 | `				/* php's catchable \Error for a bad named argument. The callee` |
|        - | 7014 | `				 * frame is already entered but its body must not run: unwind` |
|        - | 7015 | `				 * exactly like the named-hole ArgumentCountError path below` |
|        - | 7016 | `				 * (release the not-yet-installed actuals — Pass 2's release` |
|        - | 7017 | `				 * loop has not run — pop the callee slot, mark that there is no` |
|        - | 7018 | `				 * callee operand stack, and let VmCallFinish route the throw). */` |
|        - | 7019 | `				sxu32 iRel;` |
|        5 | 7020 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       11 | 7021 | `				for( iRel = 0; iRel < nActual; iRel++ ){` |
|        7 | 7022 | `					PH7_MemObjRelease(&pArg[iRel]);` |
|        4 | 7023 | `				}` |
|        5 | 7024 | `				PH7_MemObjRelease(pTos);` |
|        5 | 7025 | `				pTos = &pTos[-nCallArgs];` |
|        5 | 7026 | `				pFrameStack = 0;` |
|        5 | 7027 | `				goto SkipFuncBody;` |
|        - | 7028 | `			}` |
|        - | 7029 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|        - | 7030 | `			{` |
|        - | 7031 | `			/* php's required watermark for the hole check below, plus the` |
|        - | 7032 | `			 * highest formal slot an actual resolved to: php words a hole` |
|        - | 7033 | ``			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole`` |
|        - | 7034 | `			 * with nothing filled above it gets the positional count message` |
|        - | 7035 | `			 * (zend's RECV arg_num > EX(num_args) distinction). */` |
|        - | 7036 | `			sxu32 nReqNamed;` |
|        - | 7037 | `			sxu32 nNVNamed;` |
|      381 | 7038 | `			sxu32 nMaxFilled = 0;` |
|      381 | 7039 | `			nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);` |
|     1345 | 7040 | `			for( i = 0; i < nActual; i++ ){` |
|      969 | 7041 | `				if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|      393 | 7042 | `					nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|      194 | 7043 | `				}` |
|      487 | 7044 | `			}` |
|      971 | 7045 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - | 7046 | `				/* Find the stack arg mapped to formal n */` |
|      607 | 7047 | `				sxi32 iSrc = -1;` |
|      965 | 7048 | `				for( i = 0; i < nActual; i++ ){` |
|      829 | 7049 | `					if( aSlot[i] == (sxi32)n ){` |
|      471 | 7050 | `						iSrc = (sxi32)i;` |
|      471 | 7051 | `						break;` |
|        - | 7052 | `					}` |
|      183 | 7053 | `				}` |
|      607 | 7054 | `				if( iSrc >= 0 ){` |
|        - | 7055 | `					/* Argument was provided — install with type checking */` |
|      471 | 7056 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - | 7057 | `					/* An explicit null is NOT redirected to the default: PHP applies a` |
|        - | 7058 | `					 * default only for an OMITTED argument. An explicit null falls through` |
|        - | 7059 | `					 * to the type check below (TypeError for a non-nullable typed param,` |
|        - | 7060 | ``					 * kept as null for a typeless one). An implicitly-nullable `Type $x =`` |
|        - | 7061 | ``					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */`` |
|        - | 7062 | `					/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 7063 | `					 * coercion and whole-real materialization in place): the shared` |
|        - | 7064 | `					 * per-argument helper — one implementation for both OP_CALL` |
|        - | 7065 | `					 * paths and the generator/fiber binder (§7.1(f) fold). */` |
|      471 | 7066 | `					iArgPreFlags = pVal->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|      471 | 7067 | `					rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pVal,bCallIsStrict,pSelfHint);` |
|      471 | 7068 | `					if( rc != SXRET_OK ){` |
|        7 | 7069 | `						if( rc == PH7_ABORT ) goto Abort;` |
|        7 | 7070 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 | 7071 | `						PH7_MemObjRelease(pTos);` |
|        7 | 7072 | `						pTos = &pTos[-nCallArgs];` |
|        7 | 7073 | `						pFrameStack = 0;` |
|        7 | 7074 | `						rc = PH7_EXCEPTION;` |
|       10 | 7075 | `						goto SkipFuncBody;` |
|        - | 7076 | `					}` |
|        - | 7077 | `					/* Install: by reference or by value */` |
|      465 | 7078 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|       28 | 7079 | `						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){` |
|        - | 7080 | `							/* php 8.1: $GLOBALS cannot be passed by reference */` |
|        - | 7081 | `							SyBlob sMsg;` |
|      ! 0 | 7082 | `							SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 7083 | `							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|      ! 0 | 7084 | `								&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|      ! 0 | 7085 | `							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|      ! 0 | 7086 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 7087 | `								goto Abort;` |
|        - | 7088 | `							}` |
|      ! 0 | 7089 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 | 7090 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 | 7091 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 | 7092 | `							pFrameStack = 0;` |
|      ! 0 | 7093 | `							rc = PH7_EXCEPTION;` |
|      ! 0 | 7094 | `							goto SkipFuncBody;` |
|        - | 7095 | `						}` |
|       28 | 7096 | `						if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)iSrc,pVal) ){` |
|        - | 7097 | `							/* php refuses a by-ref argument whose EXPRESSION is not a variable, at` |
|        - | 7098 | `							 * the call and before the callee runs. Deciding it from the VALUE that` |
|        - | 7099 | `							 * arrived was wrong both ways: an operator result carries its LEFT` |
|        - | 7100 | ``							 * operand's slot, so `f($i + 1)` aliased and overwrote `$i`; and a`` |
|        - | 7101 | `							 * literal and a CALL result look alike there, where php accepts the` |
|        - | 7102 | `							 * call. VmArgRefusedByRef reads the call site's compile-time shape mask` |
|        - | 7103 | `							 * and falls back to the old runtime test only when there is none. */` |
|        - | 7104 | `							sxi32 rcRef;` |
|        3 | 7105 | `							rcRef = VmThrowByRefRefusal(&(*pVm),` |
|        2 | 7106 | `								(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|        2 | 7107 | `								&pVmFunc->sName,(sxu32)(n+1),&aFormalArg[n].sName);` |
|        3 | 7108 | `							if( rcRef == PH7_ABORT ){` |
|      ! 0 | 7109 | `								goto Abort;` |
|        - | 7110 | `							}` |
|        - | 7111 | `							/* Same teardown as the type-check refusal above: free the slot map,` |
|        - | 7112 | `							 * release the result slot and pop the actuals, then let SkipFuncBody` |
|        - | 7113 | `							 * route the throw. */` |
|        3 | 7114 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 7115 | `							PH7_MemObjRelease(pTos);` |
|        3 | 7116 | `							pTos = &pTos[-nCallArgs];` |
|        3 | 7117 | `							pFrameStack = 0;` |
|        3 | 7118 | `							rc = PH7_EXCEPTION;` |
|        3 | 7119 | `							goto SkipFuncBody;` |
|        - | 7120 | `						}` |
|       25 | 7121 | `						PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)iSrc,pVal);` |
|       25 | 7122 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 | 7123 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 7124 | `						}else{` |
|        - | 7125 | `							SyHashEntry *pRefEntry;` |
|        - | 7126 | `							/* The declared type's conversion is what the reference holds. */` |
|       25 | 7127 | `							PH7_VmByRefArgWriteBack(&(*pVm),pVal,iArgPreFlags);` |
|       37 | 7128 | `							pRefEntry = SyHashGet(&pFrame->hVar,` |
|       24 | 7129 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 | 7130 | `							if( pRefEntry == 0 ){` |
|       37 | 7131 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 | 7132 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|       25 | 7133 | `								sArg.nIdx = pVal->nIdx;` |
|       25 | 7134 | `								sArg.pUserData = 0;` |
|       25 | 7135 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 | 7136 | `							}` |
|       25 | 7137 | `							pObj = 0;` |
|        - | 7138 | `						}` |
|       13 | 7139 | `					}else{` |
|      439 | 7140 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 7141 | `					}` |
|      463 | 7142 | `					if( pObj ){` |
|      439 | 7143 | `						PH7_MemObjStore(pVal,pObj);` |
|      439 | 7144 | `						sArg.nIdx = pObj->nIdx;` |
|      439 | 7145 | `						sArg.pUserData = 0;` |
|      439 | 7146 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      217 | 7147 | `					}` |
|      234 | 7148 | `				}else{` |
|        - | 7149 | `					/* Argument was NOT provided — use default or leave unset */` |
|      139 | 7150 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 7151 | `						/* Should not reach here; variadic handled separately below */` |
|      139 | 7152 | `					}else if( n < nReqNamed ){` |
|        - | 7153 | `						/* php's implicit-required rule applies to named calls` |
|        - | 7154 | `						 * too: a hole below the required watermark throws even` |
|        - | 7155 | `						 * when the formal carries a default — f($a,$b=2,$c)` |
|        - | 7156 | ``						 * called as f(a:1,c:3) is `Argument #2 ($b) not`` |
|        - | 7157 | ``						 * passed` (a hole with NO default is always below the`` |
|        - | 7158 | `						 * watermark, so this subsumes the no-default case).` |
|        - | 7159 | `						 * A hole with nothing filled ABOVE it uses php's` |
|        - | 7160 | `						 * positional count wording instead. The passed stack` |
|        - | 7161 | `						 * args were not released yet on this path (that loop` |
|        - | 7162 | `						 * runs after Pass 2) — release them before the exit. */` |
|        5 | 7163 | `						if( n + 1 > nMaxFilled ){` |
|        3 | 7164 | `							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)` |
|      ! 0 | 7165 | `								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|      ! 0 | 7166 | `									nMaxFilled,nReqNamed,SySetUsed(&pVmFunc->aArgs))` |
|        3 | 7167 | `								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|        1 | 7168 | `									nMaxFilled,nReqNamed,nNVNamed,TRUE);` |
|        2 | 7169 | `						}else{` |
|        3 | 7170 | `							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        - | 7171 | `						}` |
|        5 | 7172 | `						SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        9 | 7173 | `						for( i = 0; i < nActual; i++ ){` |
|        5 | 7174 | `							PH7_MemObjRelease(&pArg[i]);` |
|        3 | 7175 | `						}` |
|        5 | 7176 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 7177 | `							goto Abort;` |
|        - | 7178 | `						}` |
|        5 | 7179 | `						PH7_MemObjRelease(pTos);` |
|        5 | 7180 | `						pTos = &pTos[-nCallArgs];` |
|        5 | 7181 | `						pFrameStack = 0;` |
|        5 | 7182 | `						rc = PH7_EXCEPTION;` |
|        5 | 7183 | `						goto SkipFuncBody;` |
|      135 | 7184 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      135 | 7185 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      135 | 7186 | `						if( pObj ){` |
|      135 | 7187 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|      135 | 7188 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      135 | 7189 | `							sArg.nIdx = pObj->nIdx;` |
|      135 | 7190 | `							sArg.pUserData = 0;` |
|      135 | 7191 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 7192 | `							/* A null default on an implicitly-nullable param must stay null` |
|        - | 7193 | `							 * (see the positional-path note above). */` |
|      132 | 7194 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|       42 | 7195 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0` |
|       24 | 7196 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 7197 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 7198 | `								if( xCast ) xCast(pObj);` |
|      ! 0 | 7199 | `							}else{` |
|        - | 7200 | `								/* Mask matched — a const-indirected whole-real default` |
|        - | 7201 | ``								 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|      135 | 7202 | `								VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 7203 | `							}` |
|       66 | 7204 | `						}` |
|       66 | 7205 | `					}` |
|        - | 7206 | `				}` |
|      300 | 7207 | `			}` |
|        - | 7208 | `			} /* end nReqNamed scope */` |
|        - | 7209 | `			/* Handle variadic parameter */` |
|      369 | 7210 | `			if( iVariadicIdx >= 0 ){` |
|      112 | 7211 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|      112 | 7212 | `				if( pObj ){` |
|        - | 7213 | `					/* Capture the slot index now: PH7_HashmapInsert below can` |
|        - | 7214 | `					 * PH7_ReserveMemObj, reallocating pVm->aMemObj and dangling pObj` |
|        - | 7215 | `					 * (same latent UAF the positional path guards against). */` |
|        - | 7216 | `					sxu32 nVariadicSlot;` |
|      112 | 7217 | `					PH7_MemObjToHashmap(pObj);` |
|      112 | 7218 | `					nVariadicSlot = pObj->nIdx;` |
|        - | 7219 | `					{` |
|      112 | 7220 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - | 7221 | `						/* php numbers a failing NAMED variadic element as` |
|        - | 7222 | `						 * max(total positional args, declared non-variadic` |
|        - | 7223 | `						 * formals) + 1 — zend's RECV slots always count —` |
|        - | 7224 | `						 * whichever named element fails; a POSITIONAL element` |
|        - | 7225 | `						 * uses its own 1-based call position. */` |
|      112 | 7226 | `						sxu32 nPositional = 0;` |
|      656 | 7227 | `						for( i = 0; i < nActual; i++ ){` |
|      548 | 7228 | `							if( !(i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0) ){` |
|      333 | 7229 | `								nPositional++;` |
|      165 | 7230 | `							}` |
|      276 | 7231 | `						}` |
|      612 | 7232 | `						for( i = 0; i < nActual; i++ ){` |
|      542 | 7233 | `							if( aSlot[i] == -1 ){` |
|      496 | 7234 | `								int bNamed = (i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0);` |
|      496 | 7235 | `								int bRefElem = 0; /* alias this entry to the caller's slot? */` |
|        - | 7236 | `								/* Same per-element type check + weak coercion as the` |
|        - | 7237 | ``								 * positional-only path (shared helper; no `($name)`). */`` |
|      742 | 7238 | `								rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|      492 | 7239 | `									&aFormalArg[iVariadicIdx],&pArg[i],` |
|      339 | 7240 | `									bNamed ? SXMAX(nPositional,nNonVariadic) + 1 : i + 1,bCallIsStrict);` |
|      496 | 7241 | `								if( rc != SXRET_OK ){` |
|       39 | 7242 | `									if( rc == PH7_ABORT ){` |
|      ! 0 | 7243 | `										goto Abort;` |
|        - | 7244 | `									}` |
|       39 | 7245 | `									SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|       39 | 7246 | `									PH7_MemObjRelease(pTos);` |
|       39 | 7247 | `									pTos = &pTos[-nCallArgs];` |
|       39 | 7248 | `									pFrameStack = 0;` |
|       39 | 7249 | `									rc = PH7_EXCEPTION;` |
|       39 | 7250 | `									goto SkipFuncBody;` |
|        - | 7251 | `								}` |
|      460 | 7252 | `								if( aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7253 | `									/* php screens a by-ref VARIADIC tail per collected element, in its` |
|        - | 7254 | `									 * no-name wording: a variadic has no per-element parameter name, so` |
|        - | 7255 | ``									 * php says `Argument #N could not be passed by reference` and stops`` |
|        - | 7256 | `									 * there. Nothing screened this arm at all — the branch that collects` |
|        - | 7257 | `									 * a variadic runs before the by-ref binder ever sees a formal. */` |
|        8 | 7258 | `									if( PH7_VmArgRefusedByRef(pCallMap3,i,&pArg[i]) ){` |
|        - | 7259 | `										SyBlob sMsgV;` |
|        - | 7260 | `										sxi32 rcV;` |
|        3 | 7261 | `										SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        3 | 7262 | `										SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        2 | 7263 | `											&pVmFunc->sName,(unsigned)(i + 1));` |
|        3 | 7264 | `										rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        3 | 7265 | `										if( rcV == PH7_ABORT ){` |
|      ! 0 | 7266 | `											goto Abort;` |
|        - | 7267 | `										}` |
|        3 | 7268 | `										SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        3 | 7269 | `										PH7_MemObjRelease(pTos);` |
|        3 | 7270 | `										pTos = &pTos[-nCallArgs];` |
|        3 | 7271 | `										pFrameStack = 0;` |
|        3 | 7272 | `										rc = PH7_EXCEPTION;` |
|        3 | 7273 | `										goto SkipFuncBody;` |
|        - | 7274 | `									}` |
|        5 | 7275 | `									PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,i,&pArg[i]);` |
|        2 | 7276 | `								}` |
|        - | 7277 | `								/* A by-ref variadic tail ALIASES its actuals, named entries` |
|        - | 7278 | `								 * included (the positional twin below this branch says why). */` |
|      687 | 7279 | `								bRefElem = (aFormalArg[iVariadicIdx].iFlags & VM_FUNC_ARG_BY_REF)` |
|      454 | 7280 | `									&& pArg[i].nIdx != SXU32_HIGH;` |
|      458 | 7281 | `								if( bNamed ){` |
|        - | 7282 | `									/* Named variadic entry: insert with string key */` |
|        - | 7283 | `									ph7_value sKey;` |
|      156 | 7284 | `									PH7_MemObjInit(pVm, &sKey);` |
|      156 | 7285 | `									PH7_MemObjStringAppend(&sKey,` |
|      152 | 7286 | `										pCallMap3->aNames[i].zString,` |
|      152 | 7287 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|      156 | 7288 | `									if( bRefElem ){` |
|        5 | 7289 | `										PH7_HashmapInsertByRef(pVarMap, &sKey, pArg[i].nIdx);` |
|        3 | 7290 | `									}else{` |
|      152 | 7291 | `										PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|        - | 7292 | `									}` |
|      156 | 7293 | `									PH7_MemObjRelease(&sKey);` |
|      381 | 7294 | `								}else if( bRefElem ){` |
|        - | 7295 | `									/* Positional variadic entry, aliased */` |
|      ! 0 | 7296 | `									PH7_HashmapInsertByRef(pVarMap, 0, pArg[i].nIdx);` |
|      ! 0 | 7297 | `								}else{` |
|        - | 7298 | `									/* Positional variadic entry */` |
|      305 | 7299 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - | 7300 | `								}` |
|      227 | 7301 | `							}` |
|      254 | 7302 | `						}` |
|        - | 7303 | `					}` |
|       74 | 7304 | `					sArg.nIdx = nVariadicSlot; /* pObj may be stale here (aMemObj realloc) */` |
|       74 | 7305 | `					sArg.pUserData = 0;` |
|       74 | 7306 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       35 | 7307 | `				}` |
|       39 | 7308 | `			}else{` |
|        - | 7309 | `				/* No variadic — preserve unresolved positional overflow` |
|        - | 7310 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - | 7311 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - | 7312 | `				 * the positional-only path's behavior. */` |
|      259 | 7313 | `				sxu32 nAnon = nNonVariadic;` |
|      665 | 7314 | `				for( i = 0; i < nActual; i++ ){` |
|      409 | 7315 | `					if( aSlot[i] == -2 ){` |
|        - | 7316 | `						char zAnonBuf[32];` |
|        - | 7317 | `						SyString sAnonName;` |
|      ! 0 | 7318 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 | 7319 | `							"[%u]apArg",nAnon);` |
|      ! 0 | 7320 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 | 7321 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 | 7322 | `						if( pObj ){` |
|      ! 0 | 7323 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 | 7324 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 | 7325 | `							sArg.pUserData = 0;` |
|      ! 0 | 7326 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 | 7327 | `						}` |
|      ! 0 | 7328 | `						nAnon++;` |
|      ! 0 | 7329 | `					}` |
|      206 | 7330 | `				}` |
|        - | 7331 | `			}` |
|        - | 7332 | `			/* Release all stack arguments */` |
|     1207 | 7333 | `			for( i = 0; i < nActual; i++ ){` |
|      881 | 7334 | `				PH7_MemObjRelease(&pArg[i]);` |
|      443 | 7335 | `			}` |
|      331 | 7336 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - | 7337 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|      331 | 7338 | `			n = nFormal;` |
|      168 | 7339 | `		}else{` |
|        - | 7340 | `		/* ============================================================` |
|        - | 7341 | `		 * Positional-only matching path (original)` |
|        - | 7342 | `		 * ============================================================ */` |
|        - | 7343 | `		/* Base of the actual argument stack, for php's per-ELEMENT argument` |
|        - | 7344 | `		 * number in a variadic type-error (php numbers a variadic-collected` |
|        - | 7345 | `		 * element by its overall 1-based call position, not the formal index). */` |
|   793051 | 7346 | `		ph7_value *pArgBase = pArg;` |
|   793051 | 7347 | `		n = 0;` |
|  1092746 | 7348 | `		while( pArg < pTos ){` |
|   304628 | 7349 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - | 7350 | `				/* Variadic parameter: collect all remaining args into an array */` |
|      631 | 7351 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      631 | 7352 | `				if( pObj ){` |
|        - | 7353 | `					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,` |
|        - | 7354 | `					 * reallocating pVm->aMemObj and dangling pObj — so don't read pObj->nIdx after` |
|        - | 7355 | `					 * the packing loop (pre-existing UAF, masked by the pool allocator). pMap is a` |
|        - | 7356 | `					 * separately-allocated hashmap and stays valid across the realloc. */` |
|        - | 7357 | `					sxu32 nVariadicIdx;` |
|        - | 7358 | `					/* Initialize as empty array */` |
|      631 | 7359 | `					PH7_MemObjToHashmap(pObj);` |
|      631 | 7360 | `					nVariadicIdx = pObj->nIdx;` |
|        - | 7361 | `					{` |
|      631 | 7362 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     2893 | 7363 | `						while( pArg < pTos ){` |
|        - | 7364 | `							/* Per-element type check + weak coercion (shared helper,` |
|        - | 7365 | `							 * also used by the named-argument path). The argument` |
|        - | 7366 | `							 * number is the element's overall 1-based call position` |
|        - | 7367 | `` 							 * ((pArg - pArgBase) + 1) and, like php, the `($name)` `` |
|        - | 7368 | `							 * clause is omitted. */` |
|     3467 | 7369 | `							rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,` |
|     2308 | 7370 | `								&aFormalArg[n],pArg,(sxu32)(pArg - pArgBase) + 1,bCallIsStrict);` |
|     2313 | 7371 | `							if( rc != SXRET_OK ){` |
|       44 | 7372 | `								if( rc == PH7_ABORT ){` |
|      ! 0 | 7373 | `									goto Abort;` |
|        - | 7374 | `								}` |
|        - | 7375 | `								/* Skip function body, route through normal cleanup */` |
|       44 | 7376 | `								PH7_MemObjRelease(pTos);` |
|       44 | 7377 | `								pTos = &pTos[-nCallArgs];` |
|       44 | 7378 | `								pFrameStack = 0;` |
|       44 | 7379 | `								rc = PH7_EXCEPTION;` |
|       44 | 7380 | `								goto SkipFuncBody;` |
|        - | 7381 | `							}` |
|     2272 | 7382 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7383 | `								/* The positional twin of the named path's variadic screen above:` |
|        - | 7384 | `								 * php refuses a non-variable collected into a by-ref variadic tail,` |
|        - | 7385 | `								 * in its no-name wording. */` |
|       52 | 7386 | `								sxu32 nPosV = (sxu32)(pArg - pArgBase);` |
|       52 | 7387 | `								if( PH7_VmArgRefusedByRef(pCallMap3,nPosV,pArg) ){` |
|        - | 7388 | `									SyBlob sMsgV;` |
|        - | 7389 | `									sxi32 rcV;` |
|        7 | 7390 | `									SyBlobInit(&sMsgV,&pVm->sAllocator);` |
|        7 | 7391 | `									SyBlobFormat(&sMsgV,"%z(): Argument #%u could not be passed by reference",` |
|        6 | 7392 | `										&pVmFunc->sName,(unsigned)(nPosV + 1));` |
|        7 | 7393 | `									rcV = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsgV);` |
|        7 | 7394 | `									if( rcV == PH7_ABORT ){` |
|      ! 0 | 7395 | `										goto Abort;` |
|        - | 7396 | `									}` |
|        7 | 7397 | `									PH7_MemObjRelease(pTos);` |
|        7 | 7398 | `									pTos = &pTos[-nCallArgs];` |
|        7 | 7399 | `									pFrameStack = 0;` |
|        7 | 7400 | `									rc = PH7_EXCEPTION;` |
|        7 | 7401 | `									goto SkipFuncBody;` |
|        - | 7402 | `								}` |
|       46 | 7403 | `								PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,nPosV,pArg);` |
|       46 | 7404 | `								if( pArg->nIdx != SXU32_HIGH ){` |
|        - | 7405 | `									/* php ALIASES each collected element to the caller's slot:` |
|        - | 7406 | ``									 * `function f(&...$xs){ $xs[0] = 'A'; }` writes back, and`` |
|        - | 7407 | ``									 * var_dump($xs) inside the callee shows `&int(1)`. Copying`` |
|        - | 7408 | `									 * them left every actual untouched. The node counts as a` |
|        - | 7409 | `									 * holder of the caller's slot, so the frame teardown that` |
|        - | 7410 | `									 * destroys the variadic array gives the hold back. */` |
|       42 | 7411 | `									PH7_HashmapInsertByRef(pMap, 0, pArg->nIdx);` |
|       42 | 7412 | `									pArg++;` |
|       42 | 7413 | `									continue;` |
|        - | 7414 | `								}` |
|        2 | 7415 | `							}` |
|     2226 | 7416 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|     2226 | 7417 | `							pArg++;` |
|        4 | 7418 | `						}` |
|        - | 7419 | `					}` |
|      584 | 7420 | `					sArg.nIdx = nVariadicIdx; /* pObj may be stale here (aMemObj realloc) — use the saved index */` |
|      584 | 7421 | `					sArg.pUserData = 0;` |
|      584 | 7422 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      290 | 7423 | `				}` |
|      584 | 7424 | `				break; /* All remaining args consumed */` |
|        - | 7425 | `			}` |
|   304002 | 7426 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 7427 | `				/* An explicit null is NOT redirected to the default (PHP applies a` |
|        - | 7428 | `				 * default only for an omitted arg); it falls through to the type check` |
|        - | 7429 | `				 * below — TypeError for a non-nullable typed param, kept as null for a` |
|        - | 7430 | ``				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE`` |
|        - | 7431 | `				 * at compile time so its check accepts null. */` |
|        - | 7432 | `				/* Type checking (union / class / pseudo / scalar, with weak-mode` |
|        - | 7433 | `				 * coercion and whole-real materialization in place — nullable` |
|        - | 7434 | `				 * types (?type) let null through): the shared per-argument` |
|        - | 7435 | `				 * helper, one implementation for both OP_CALL paths and the` |
|        - | 7436 | `				 * generator/fiber binder (§7.1(f) fold). */` |
|   294398 | 7437 | `				iArgPreFlags = pArg->iFlags; /* did the check COERCE it? (by-ref write-back) */` |
|   294398 | 7438 | `				rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pArg,bCallIsStrict,pSelfHint);` |
|   294398 | 7439 | `				if( rc != SXRET_OK ){` |
|      287 | 7440 | `					if( rc == PH7_ABORT ){` |
|        6 | 7441 | `						goto Abort;` |
|        - | 7442 | `					}` |
|        - | 7443 | `					/* Skip function body, route through normal cleanup */` |
|      283 | 7444 | `					PH7_MemObjRelease(pTos);` |
|      283 | 7445 | `					pTos = &pTos[-nCallArgs];` |
|      283 | 7446 | `					pFrameStack = 0;` |
|      283 | 7447 | `					rc = PH7_EXCEPTION;` |
|      283 | 7448 | `					goto SkipFuncBody;` |
|        - | 7449 | `				}` |
|   294116 | 7450 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 7451 | `					/* Pass by reference */` |
|     7189 | 7452 | `					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){` |
|        - | 7453 | `						/* php 8.1: $GLOBALS cannot be passed by reference —` |
|        - | 7454 | `						 * a catchable Error with php's exact wording. */` |
|        - | 7455 | `						SyBlob sMsg;` |
|        3 | 7456 | `						SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 7457 | `						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",` |
|        2 | 7458 | `							&pVmFunc->sName,n+1,&aFormalArg[n].sName);` |
|        3 | 7459 | `						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 7460 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 7461 | `							goto Abort;` |
|        - | 7462 | `						}` |
|        3 | 7463 | `						PH7_MemObjRelease(pTos);` |
|        3 | 7464 | `						pTos = &pTos[-nCallArgs];` |
|        3 | 7465 | `						pFrameStack = 0;` |
|        3 | 7466 | `						rc = PH7_EXCEPTION;` |
|        3 | 7467 | `						goto SkipFuncBody;` |
|        - | 7468 | `					}` |
|     7187 | 7469 | `					if( PH7_VmArgRefusedByRef(pCallMap3,(sxu32)n,pArg) ){` |
|        - | 7470 | `						/* php's refusal, decided from the argument's compile-time SHAPE (the` |
|        - | 7471 | `						 * companion of the named-argument binder above; see VmArgRefusedByRef). */` |
|        - | 7472 | `						sxi32 rcRef;` |
|     6028 | 7473 | `						rcRef = VmThrowByRefRefusal(&(*pVm),` |
|     4018 | 7474 | `							(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0,` |
|     4018 | 7475 | `							&pVmFunc->sName,(sxu32)(n+1),&aFormalArg[n].sName);` |
|     4019 | 7476 | `						if( rcRef == PH7_ABORT ){` |
|      ! 0 | 7477 | `							goto Abort;` |
|        - | 7478 | `						}` |
|        - | 7479 | `						/* Route the throw like every other binder refusal: release the result` |
|        - | 7480 | `						 * slot, pop the actuals and let SkipFuncBody finish the call. Returning` |
|        - | 7481 | `						 * from here walked out of the dispatch loop with the callee's frame and` |
|        - | 7482 | `						 * stack still live, so a CAUGHT refusal silently abandoned every` |
|        - | 7483 | `						 * statement after the catch. */` |
|     4019 | 7484 | `						PH7_MemObjRelease(pTos);` |
|     4019 | 7485 | `						pTos = &pTos[-nCallArgs];` |
|     4019 | 7486 | `						pFrameStack = 0;` |
|     4019 | 7487 | `						rc = PH7_EXCEPTION;` |
|     4019 | 7488 | `						goto SkipFuncBody;` |
|        - | 7489 | `					}` |
|     3169 | 7490 | `					PH7_VmArgTempCallNotice(&(*pVm),pCallMap3,(sxu32)n,pArg);` |
|     3169 | 7491 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - | 7492 | `						/* Nothing to alias: pass by value. */` |
|       99 | 7493 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       51 | 7494 | `					}else{` |
|        - | 7495 | `						SyHashEntry *pRefEntry;` |
|        - | 7496 | `						/* The declared type's conversion is what the reference holds. */` |
|     3073 | 7497 | `						PH7_VmByRefArgWriteBack(&(*pVm),pArg,iArgPreFlags);` |
|        - | 7498 | `						/* Install the referenced variable in the private function frame */` |
|     3073 | 7499 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|     3073 | 7500 | `						if( pRefEntry == 0 ){` |
|     4607 | 7501 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|     3068 | 7502 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|     3073 | 7503 | `							sArg.nIdx = pArg->nIdx;` |
|     3073 | 7504 | `							sArg.pUserData = 0;` |
|     3073 | 7505 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     1534 | 7506 | `						}` |
|     3073 | 7507 | `						pObj = 0;` |
|        - | 7508 | `					}` |
|     1587 | 7509 | `				}else{` |
|        - | 7510 | `					/* Pass by value,make a copy of the given argument */` |
|   286932 | 7511 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 7512 | `				}` |
|   145477 | 7513 | `			}else{` |
|        - | 7514 | `				char zName[32];` |
|        - | 7515 | `				SyString sArgName;` |
|        - | 7516 | `				/* Set a dummy name */` |
|     9609 | 7517 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|     9609 | 7518 | `				sArgName.zString = zName;` |
|        - | 7519 | `				/* Annonymous argument */` |
|     9609 | 7520 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 7521 | `			}` |
|   299700 | 7522 | `			if( pObj ){` |
|   296632 | 7523 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 7524 | `				/* Insert argument index  */` |
|   296632 | 7525 | `				sArg.nIdx = pObj->nIdx;` |
|   296632 | 7526 | `				sArg.pUserData = 0;` |
|   296632 | 7527 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|   148731 | 7528 | `			}` |
|   299700 | 7529 | `			PH7_MemObjRelease(pArg);` |
|   299700 | 7530 | `			pArg++;` |
|   299700 | 7531 | `			++n;` |
|        5 | 7532 | `		}` |
|        - | 7533 | `		} /* end named vs positional branch */` |
|        - | 7534 | `		/* Set up closure environment */` |
|   789029 | 7535 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 7536 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 7537 | `			ph7_value *pValue;` |
|        - | 7538 | `			sxu32 iEnv;` |
|    24961 | 7539 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|    56701 | 7540 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|    31745 | 7541 | `				pEnv = &aEnv[iEnv];` |
|    31745 | 7542 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 7543 | `					/* Do not install null value */` |
|    24225 | 7544 | `					continue;` |
|        - | 7545 | `				}` |
|     7520 | 7546 | `				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       13 | 7547 | `				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|        - | 7548 | `					/* The Closure instance carries an explicit bound $this` |
|        - | 7549 | `					 * (bindTo/bind/call): it wins over the creation-time` |
|        - | 7550 | `					 * captured $this, php-exact. */` |
|        7 | 7551 | `					continue;` |
|        - | 7552 | `				}` |
|     7519 | 7553 | `				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|        - | 7554 | `					/* Captured by reference: link the name to the shared slot` |
|        - | 7555 | `					 * (no copy), mirroring the by-ref argument install above. */` |
|     1083 | 7556 | `					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|     1617 | 7557 | `						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),` |
|     1078 | 7558 | `							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|      534 | 7559 | `					}` |
|     1083 | 7560 | `					continue;` |
|        - | 7561 | `				}` |
|     6441 | 7562 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|     6441 | 7563 | `				if( pValue == 0 ){` |
|      ! 0 | 7564 | `					continue;` |
|        - | 7565 | `				}` |
|        - | 7566 | `				/* Invalidate any prior representation */` |
|     6441 | 7567 | `				PH7_MemObjRelease(pValue);` |
|        - | 7568 | `				/* Duplicate bound variable value */` |
|     6441 | 7569 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|     3223 | 7570 | `			}` |
|    12473 | 7571 | `		}` |
|        - | 7572 | `		/* Too-few-arguments check, placed AFTER the passed arguments were` |
|        - | 7573 | `		 * installed and type-checked: php's RECV order means a type error on` |
|        - | 7574 | ``		 * a PASSED argument beats the count error (`f(int $x,$y)` called`` |
|        - | 7575 | `		 * f("str") is a TypeError, not ArgumentCountError). The passed args` |
|        - | 7576 | `		 * were already released by the install loop, so the standard throw` |
|        - | 7577 | `		 * exit leaks nothing. The named path never fires this (its per-hole` |
|        - | 7578 | `		 * check ran in-loop; n == nNonVariadic >= nRequired here).` |
|        - | 7579 | `		 * Builtins written as embedded PHP in the prelude (VM_FUNC_INTERNAL,` |
|        - | 7580 | `		 * with or without VM_FUNC_CLASS_METHOD) are NOT exempt: their declared` |
|        - | 7581 | `		 * signatures were swept against the php 8.5 oracle, so the derived` |
|        - | 7582 | `		 * required count and wording are php's, and VmThrowBuiltinTooFewArgs` |
|        - | 7583 | `		 * words them as php words an internal callable. */` |
|   789029 | 7584 | `		if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|        - | 7585 | `			sxu32 nNonVar,nReq;` |
|     6449 | 7586 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     6449 | 7587 | `			if( n < nReq ){` |
|       57 | 7588 | `				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;` |
|       57 | 7589 | `				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){` |
|       52 | 7590 | `					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       17 | 7591 | `						nPassed,nReq,SySetUsed(&pVmFunc->aArgs));` |
|       18 | 7592 | `				}else{` |
|       33 | 7593 | `					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       10 | 7594 | `						nPassed,nReq,nNonVar,TRUE);` |
|        - | 7595 | `				}` |
|       57 | 7596 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 7597 | `					goto Abort;` |
|        - | 7598 | `				}` |
|       57 | 7599 | `				PH7_MemObjRelease(pTos);` |
|       57 | 7600 | `				pTos = &pTos[-nCallArgs];` |
|       57 | 7601 | `				pFrameStack = 0;` |
|       57 | 7602 | `				rc = PH7_EXCEPTION;` |
|       57 | 7603 | `				goto SkipFuncBody;` |
|        5 | 7604 | `			}` |
|   785780 | 7605 | `		}else if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) == VM_FUNC_INTERNAL ){` |
|        - | 7606 | `			/* Too-MANY arguments, the mirror php enforces for an internal` |
|        - | 7607 | `			 * callable ("count_chars() expects at most 2 arguments, 3 given").` |
|        - | 7608 | `			 * PHL accepted the extras silently — the surplus only ever reached` |
|        - | 7609 | `			 * func_get_args(). Every formal is filled on this branch, so the call` |
|        - | 7610 | `			 * is over the maximum exactly when more actuals arrived than there are` |
|        - | 7611 | `			 * non-variadic formals; a VARIADIC tail means php declares no maximum` |
|        - | 7612 | `			 * at all, so nNonVar below the formal count is exempt. Prelude` |
|        - | 7613 | `			 * FUNCTIONS only: the chunk-backed class METHODS were not swept against` |
|        - | 7614 | `			 * php's signatures (Fiber::start() is declared argless and reads` |
|        - | 7615 | `			 * func_get_args()). */` |
|        - | 7616 | `			sxu32 nNonVar,nReq;` |
|     1541 | 7617 | `			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);` |
|     1536 | 7618 | `			if( nNonVar == SySetUsed(&pVmFunc->aArgs)` |
|     1535 | 7619 | `			 && pFrame->nActualArgs >= 0` |
|     1539 | 7620 | `			 && (sxu32)pFrame->nActualArgs > nNonVar ){` |
|       40 | 7621 | `				rc = VmThrowBuiltinTooManyArgs(&(*pVm),pSelfHint,&pVmFunc->sName,` |
|       26 | 7622 | `					(sxu32)pFrame->nActualArgs,nNonVar,nReq);` |
|       27 | 7623 | `				if( rc == PH7_ABORT ){` |
|      ! 0 | 7624 | `					goto Abort;` |
|        - | 7625 | `				}` |
|       27 | 7626 | `				PH7_MemObjRelease(pTos);` |
|       27 | 7627 | `				pTos = &pTos[-nCallArgs];` |
|       27 | 7628 | `				pFrameStack = 0;` |
|       27 | 7629 | `				rc = PH7_EXCEPTION;` |
|       27 | 7630 | `				goto SkipFuncBody;` |
|        - | 7631 | `			}` |
|      755 | 7632 | `		}` |
|        - | 7633 | `		/* Process default values for remaining formal parameters */` |
|   804279 | 7634 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    16027 | 7635 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 7636 | `				/* Variadic parameter with no extra args — create empty array */` |
|      696 | 7637 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      696 | 7638 | `				if( pObj ){` |
|      696 | 7639 | `					PH7_MemObjToHashmap(pObj);` |
|      696 | 7640 | `					sArg.nIdx = pObj->nIdx;` |
|      696 | 7641 | `					sArg.pUserData = 0;` |
|      696 | 7642 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      346 | 7643 | `				}` |
|      696 | 7644 | `				n++;` |
|      696 | 7645 | `				break; /* Variadic is always last */` |
|        - | 7646 | `			}` |
|    15335 | 7647 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|    15335 | 7648 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|    15335 | 7649 | `				if( pObj ){` |
|        - | 7650 | `					/* Evaluate the default value and extract it's result */` |
|    15335 | 7651 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|    15335 | 7652 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 7653 | `						goto Abort;` |
|        - | 7654 | `					}` |
|        - | 7655 | `					/* Insert argument index */` |
|    15335 | 7656 | `					sArg.nIdx = pObj->nIdx;` |
|    15335 | 7657 | `					sArg.pUserData = 0;` |
|    15335 | 7658 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 7659 | `					/* Make sure the default argument is of the correct type.` |
|        - | 7660 | ``					 * A null default on an implicitly-nullable param (`int $x = null`)`` |
|        - | 7661 | `					 * must stay null — casting it to 0/""/false would diverge from PHP` |
|        - | 7662 | `					 * and contradict the explicit-null path, which now keeps it null. */` |
|    15330 | 7663 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1972 | 7664 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)` |
|      994 | 7665 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|      ! 0 | 7666 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 7667 | `						/* Cast to the desired type */` |
|      ! 0 | 7668 | `						xCast(pObj);` |
|      ! 0 | 7669 | `					}else{` |
|        - | 7670 | `						/* Mask matched — a const-indirected whole-real default` |
|        - | 7671 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|    15335 | 7672 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|        - | 7673 | `					}` |
|     7665 | 7674 | `				}` |
|     7665 | 7675 | `			}` |
|    15335 | 7676 | `			++n;` |
|        5 | 7677 | `		}` |
|        - | 7678 | `		} /* end VmCallArgMap scope */` |
|        - | 7679 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 7680 | `		 * does not return anything.` |
|        - | 7681 | `		 */` |
|   788949 | 7682 | `		PH7_MemObjRelease(pTos);` |
|   788949 | 7683 | `		pTos = &pTos[-nCallArgs];` |
|        - | 7684 | `		/* Allocate an operand stack (via the recycling allocator) and evaluate the` |
|        - | 7685 | `		 * function body. Size it to a tight static bound when the body is statically` |
|        - | 7686 | `		 * modelable (BYTECODE.md stage 7) — the big memory win for deep recursion,` |
|        - | 7687 | `		 * where one such stack lives per frame — falling back to the safe` |
|        - | 7688 | `		 * instruction-count bound otherwise.` |
|        - | 7689 | `		 *` |
|        - | 7690 | `		 * The bound is computed LAZILY on the first call and cached on the func` |
|        - | 7691 | `		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:` |
|        - | 7692 | `		 * self-computing on first use is fail-safe against any body-creation path` |
|        - | 7693 | `		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),` |
|        - | 7694 | `		 * where undersizing is a heap overflow; the amortized cost is one analysis` |
|        - | 7695 | `		 * per function. */` |
|        - | 7696 | `		{` |
|   788949 | 7697 | `			sxu32 nSlots = pVmFunc->nMaxStack;` |
|   788949 | 7698 | `			if( nSlots == 0 ){` |
|    17067 | 7699 | `				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);` |
|    25598 | 7700 | `				sxu32 nTight = VmComputeMaxStack(&(*pVm),` |
|    17062 | 7701 | `					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);` |
|    17067 | 7702 | `				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;` |
|    17067 | 7703 | `				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */` |
|    17067 | 7704 | `				pVmFunc->nMaxStack = nSlots;` |
|     8531 | 7705 | `			}` |
|   788949 | 7706 | `			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);` |
|        - | 7707 | `		}` |
|   788949 | 7708 | `		if( pFrameStack == 0 ){` |
|        - | 7709 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 7710 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 7711 | `				&pVmFunc->sName);` |
|      ! 0 | 7712 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 7713 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7714 | `			}` |
|      ! 0 | 7715 | `			break;` |
|        - | 7716 | `		}` |
|   394257 | 7717 | `SkipFuncBody:` |
|   793427 | 7718 | `		if( pSelf ){` |
|        - | 7719 | `			/* Push class name */` |
|   506549 | 7720 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|   253272 | 7721 | `		}` |
|        - | 7722 | `		/* Increment nesting level */` |
|   793427 | 7723 | `		pVm->nRecursionDepth++;` |
|   793427 | 7724 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 7725 | `			/* Arg-binding threw: there is no body to run — finish the call` |
|        - | 7726 | `			 * immediately (no record is pushed). */` |
|        - | 7727 | `			VmCallRecord sCallee;` |
|     4483 | 7728 | `			sCallee.pVmFunc = pVmFunc;` |
|     4483 | 7729 | `			sCallee.pFrame = pFrame;` |
|     4483 | 7730 | `			sCallee.pFrameStack = pFrameStack;` |
|     4483 | 7731 | `			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|     4483 | 7732 | `			sCallee.nLastRef = SXU32_HIGH;` |
|     4483 | 7733 | `			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|     4483 | 7734 | `			sState.pTos = pTos;` |
|     4483 | 7735 | `			sState.pc = pc;` |
|     4483 | 7736 | `			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);` |
|     4483 | 7737 | `			pTos = sState.pTos;` |
|     4483 | 7738 | `			pc = sState.pc;` |
|     4483 | 7739 | `			if( rc == PH7_ABORT ){` |
|        - | 7740 | `				/* Abort processing immeditaley */` |
|      ! 0 | 7741 | `				goto Abort;` |
|     4483 | 7742 | `			}else if( rc == PH7_SUSPEND ){` |
|      ! 0 | 7743 | `				goto Suspend;` |
|     4483 | 7744 | `			}else if( rc == PH7_EXCEPTION ){` |
|      165 | 7745 | `				goto Exception;` |
|        - | 7746 | `			}` |
|     2164 | 7747 | `		}else{` |
|        - | 7748 | `			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a` |
|        - | 7749 | `			 * call record (caller activation + in-flight call) and switch the` |
|        - | 7750 | `			 * loop's locals to the callee — a PHP->PHP call no longer grows` |
|        - | 7751 | `			 * the native stack. The record node is pool-allocated so` |
|        - | 7752 | `			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */` |
|   788949 | 7753 | `			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   788949 | 7754 | `			if( pRec ){` |
|   786603 | 7755 | `				pVm->pIdleCallFrames = (void *)pRec->pPrev;` |
|   393519 | 7756 | `			}else{` |
|     2351 | 7757 | `				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));` |
|        - | 7758 | `			}` |
|   788949 | 7759 | `			if( pRec == 0 ){` |
|        - | 7760 | `				/* OOM: undo the push-time accounting, tear the call down and` |
|        - | 7761 | `				 * raise the non-catchable fatal (the §3.1 OOM convention —` |
|        - | 7762 | `				 * never a silent NULL). */` |
|      ! 0 | 7763 | `				pVm->nRecursionDepth--;` |
|      ! 0 | 7764 | `				if( pSelf ){` |
|      ! 0 | 7765 | `					(void)SySetPop(&pVm->aSelf);` |
|      ! 0 | 7766 | `				}` |
|      ! 0 | 7767 | `				SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|      ! 0 | 7768 | `				VmLeaveFrame(&(*pVm));` |
|      ! 0 | 7769 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 7770 | `				goto Abort;` |
|        - | 7771 | `			}` |
|   788949 | 7772 | `			sState.pTos = pTos;` |
|   788949 | 7773 | `			sState.pc = pc;` |
|   788949 | 7774 | `			pRec->sCaller = sState;` |
|   788949 | 7775 | `			pRec->sCall.pVmFunc = pVmFunc;` |
|   788949 | 7776 | `			pRec->sCall.pFrame = pFrame;` |
|   788949 | 7777 | `			pRec->sCall.pFrameStack = pFrameStack;` |
|   788949 | 7778 | `			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;` |
|   788949 | 7779 | `			pRec->sCall.nLastRef = SXU32_HIGH;` |
|   788949 | 7780 | `			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);` |
|   788949 | 7781 | `			pRec->pPrev = pCallTop;` |
|   788949 | 7782 | `			pCallTop = pRec;` |
|        - | 7783 | `			/* Switch to the callee activation (what the recursive` |
|        - | 7784 | `			 * VmByteCodeExec entry used to set up). */` |
|   788949 | 7785 | `			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);` |
|   788949 | 7786 | `			pStack = pFrameStack;` |
|   788949 | 7787 | `			pTos = &pStack[-1];` |
|   788949 | 7788 | `			pc = 0;` |
|   788949 | 7789 | `			sState.aInstr = aInstr;` |
|   788949 | 7790 | `			sState.pStack = pStack;` |
|   788949 | 7791 | `			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */` |
|   788949 | 7792 | `			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */` |
|   788949 | 7793 | `			sState.pTos = pTos;` |
|   788949 | 7794 | `			sState.pc = 0;` |
|   788949 | 7795 | `			sState.nExceptionBase = SySetUsed(&pVm->aException);` |
|   788949 | 7796 | `			sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);` |
|   788949 | 7797 | `			sState.pEntryFrame = pVm->pFrame;` |
|   788949 | 7798 | `			sState.pResult = pRec->sCaller.pTos;` |
|   788949 | 7799 | `			sState.pLastRef = &pRec->sCall.nLastRef;` |
|   788949 | 7800 | `			sState.pEnforceRetFunc = VmFuncHasReturnType(pVmFunc) ? pVmFunc : 0;` |
|   788949 | 7801 | `			sState.is_callback = 0;` |
|   788949 | 7802 | `			sState.bReturnPropagates = 0;` |
|   788949 | 7803 | `			goto VmLoopFetch;` |
|        - | 7804 | `		}` |
|     2164 | 7805 | `	}else{` |
|        - | 7806 | `		/* Look for an installed foreign function.` |
|        - | 7807 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 7808 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 7809 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 7810 | `		 * global fallback for unqualified function calls in namespaces. */` |
|  4733860 | 7811 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 7812 | `		{` |
|  4733860 | 7813 | `		VmCallArgMap *pCallMap2 = pEffCallMap;` |
|  4733860 | 7814 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 7815 | `			/* Compiler-qualified: try short name as global fallback */` |
|       57 | 7816 | `			const char *zShort = sName.zString;` |
|        - | 7817 | `			sxu32 i;` |
|      857 | 7818 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      805 | 7819 | `				if( sName.zString[i] == '\\' ){` |
|       71 | 7820 | `					zShort = &sName.zString[i + 1];` |
|       33 | 7821 | `				}` |
|      405 | 7822 | `			}` |
|       57 | 7823 | `			if( zShort != sName.zString ){` |
|       57 | 7824 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       57 | 7825 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       26 | 7826 | `			}` |
|       26 | 7827 | `		}` |
|        - | 7828 | `		} /* end VmCallArgMap namespace scope */` |
|  4733860 | 7829 | `		if( pEntry == 0 ){` |
|        - | 7830 | `			/* php accepts the "Class::method" STATIC-callable string everywhere a` |
|        - | 7831 | `			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).` |
|        - | 7832 | `			 * Split on the LAST "::" (php's own zend_memrchr scan) and route through the` |
|        - | 7833 | `			 * shared array-callable machinery ([class-name, method-name]) instead of` |
|        - | 7834 | `			 * warning undefined. */` |
|   240143 | 7835 | `			const char *zCbCls = 0,*zCbMeth = 0;` |
|   240143 | 7836 | `			sxu32 nCbCls = 0,nCbMeth = 0;` |
|   240143 | 7837 | `			int bScoped = PH7_VmCallableStringParts(sName.zString,sName.nByte,` |
|        - | 7838 | `				&zCbCls,&nCbCls,&zCbMeth,&nCbMeth);` |
|   240143 | 7839 | `			if( bScoped ){` |
|        - | 7840 | `				/* Resolve BEFORE dispatching: the shared dispatcher answers SXRET_OK with` |
|        - | 7841 | `` 				 * a NULL result for a pair it cannot resolve, so `$cb='C::nosuch'; $cb();` `` |
|        - | 7842 | `				 * evaluated to NULL with no diagnostic at all — where php throws the same` |
|        - | 7843 | `				 * Errors the ARRAY form of the same call already raised here. (Its own` |
|        - | 7844 | ``				 * `if( bScoped )` block: this scratch buffer and the dispatch block's`` |
|        - | 7845 | `				 * hashmap are both block-head declarations, and the check runs between` |
|        - | 7846 | `				 * them.) */` |
|        - | 7847 | `				char zSmMsg[192];` |
|   100082 | 7848 | `				sxi32 nSmBrc = pVm->nBoundaryRc;` |
|   100082 | 7849 | `				const void *pSmRes = (const void *)pVm->pResumeFrame;` |
|   150121 | 7850 | `				const char *zSmErr = VmCallableClassMethodError(&(*pVm),` |
|    50039 | 7851 | `					PH7_VmExtractClass(&(*pVm),zCbCls,nCbCls,FALSE,0),` |
|    50039 | 7852 | `					zCbCls,nCbCls,zCbMeth,nCbMeth,TRUE,zSmMsg,sizeof(zSmMsg));` |
|   100082 | 7853 | `				if( zSmErr ){` |
|        - | 7854 | `					sxi32 rcSmErr;` |
|       53 | 7855 | `					int bSmRaised = PH7_VmClassLookupRaised(&(*pVm),nSmBrc,pSmRes);` |
|       53 | 7856 | `					if( pInstr->iP2 ){` |
|      ! 0 | 7857 | `						VmSpreadConsume(pVm);` |
|      ! 0 | 7858 | `					}` |
|       53 | 7859 | `					if( nCallArgs > 0 ){` |
|      ! 0 | 7860 | `						VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7861 | `					}` |
|       53 | 7862 | `					PH7_MemObjRelease(pTos);` |
|       53 | 7863 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       53 | 7864 | `					pTos->nIdx = SXU32_HIGH;` |
|       53 | 7865 | `					if( bSmRaised ){` |
|        - | 7866 | `						/* The class name's autoloader threw: land THAT, exactly as the array` |
|        - | 7867 | `						 * form does — php never reports the class missing in this case. */` |
|        6 | 7868 | `						rcSmErr = pVm->nBoundaryRc;` |
|        6 | 7869 | `						pVm->nBoundaryRc = 0;` |
|        6 | 7870 | `						if( rcSmErr == PH7_ABORT ){` |
|      ! 0 | 7871 | `							goto Abort;` |
|        - | 7872 | `						}` |
|        6 | 7873 | `						rc = PH7_EXCEPTION;` |
|       14 | 7874 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7875 | `					}` |
|       48 | 7876 | `					rcSmErr = VmThrowFromVm(&(*pVm),"Error",zSmErr,(sxu32)SyStrlen(zSmErr));` |
|       48 | 7877 | `					if( rcSmErr == SXERR_ABORT ){` |
|      ! 0 | 7878 | `						goto Abort;` |
|        - | 7879 | `					}` |
|       48 | 7880 | `					rc = rcSmErr;` |
|       64 | 7881 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7882 | `				}` |
|    50014 | 7883 | `			}` |
|   240093 | 7884 | `			if( bScoped ){` |
|        - | 7885 | `				/* Resolved: hand the callable STRING itself to the shared dispatcher, which` |
|        - | 7886 | ``				 * decodes `Class::method` the same way (it has to, for the callback-argument`` |
|        - | 7887 | `				 * callers). This used to build a throwaway [class,method] map here and enter` |
|        - | 7888 | `				 * through the hashmap branch — two decoders for one spelling. */` |
|        - | 7889 | `				ph7_value sResult;` |
|        - | 7890 | `				sxi32 rcSm;` |
|   150045 | 7891 | `				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|   100028 | 7892 | `					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|   100031 | 7893 | `				SySetReset(&aArg);` |
|   100047 | 7894 | `				while( pArg < pTos ){` |
|       17 | 7895 | `					SySetPut(&aArg,(const void *)&pArg);` |
|       17 | 7896 | `					pArg++;` |
|        1 | 7897 | `				}` |
|   100031 | 7898 | `				PH7_MemObjInit(pVm,&sResult);` |
|   100031 | 7899 | `				pVm->bDiscardCallback = bResultDropped;   /* see the sibling site */` |
|   150045 | 7900 | `				rcSm = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),` |
|   100028 | 7901 | `					(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);` |
|   100031 | 7902 | `				pVm->bDiscardCallback = 0;` |
|   100031 | 7903 | `				SySetReset(&aArg);` |
|   100031 | 7904 | `				if( nCallArgs > 0 ){` |
|       15 | 7905 | `					VmPopOperand(&pTos,nCallArgs);` |
|        7 | 7906 | `				}` |
|   100031 | 7907 | `				if( rcSm == PH7_ABORT ){` |
|      ! 0 | 7908 | `					PH7_MemObjRelease(&sResult);` |
|      ! 0 | 7909 | `					goto Abort;` |
|        - | 7910 | `				}` |
|   100031 | 7911 | `				if( rcSm == PH7_EXCEPTION ){` |
|        - | 7912 | `					sxi32 iResumePc;` |
|   100004 | 7913 | `					PH7_MemObjRelease(&sResult);` |
|   100004 | 7914 | `					if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|   100001 | 7915 | `						PH7_MemObjRelease(pTos);` |
|        - | 7916 | `						/* Drain the abandoned outer-expression operands` |
|        - | 7917 | ``						 * (`1 + "C::m"()`) to the try's base — one leaked`` |
|        - | 7918 | `						 * slot per caught throw otherwise. */` |
|   300001 | 7919 | `						PH7_RESUME_DRAIN()` |
|   100001 | 7920 | `						pc = iResumePc;` |
|   100001 | 7921 | `						break;` |
|        - | 7922 | `					}` |
|        3 | 7923 | `					goto Exception;` |
|        - | 7924 | `				}` |
|       28 | 7925 | `				PH7_MemObjStore(&sResult,pTos);` |
|       28 | 7926 | `				PH7_MemObjRelease(&sResult);` |
|       28 | 7927 | `				break;` |
|        - | 7928 | `			}` |
|        - | 7929 | `			/* Call to an undefined function is a catchable Error in php 8 — it does` |
|        - | 7930 | `			 * NOT warn and hand back null and carry on, which is what PH7 did (and` |
|        - | 7931 | `			 * which quietly turned a typo into a null-propagating program). */` |
|        - | 7932 | `			{` |
|        - | 7933 | `			SyBlob sMsg;` |
|   140065 | 7934 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   140065 | 7935 | `			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);` |
|        - | 7936 | `			/* Consume this call's captured spread runs so they don't leak into a` |
|        - | 7937 | `			 * later call (this path never reaches VmBuildEffectiveArgMap). */` |
|   140065 | 7938 | `			if( pInstr->iP2 ){` |
|      ! 0 | 7939 | `				VmSpreadConsume(pVm);` |
|      ! 0 | 7940 | `			}` |
|        - | 7941 | `			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the` |
|        - | 7942 | `			 * compile-time arg count on the stack, and this early exit skips the` |
|        - | 7943 | `			 * arg-building loop that the normal path uses, so popping only iP1 would` |
|        - | 7944 | `			 * strand the expanded elements and corrupt the enclosing expression. */` |
|   140065 | 7945 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 7946 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 7947 | `			}` |
|   140065 | 7948 | `			PH7_MemObjRelease(pTos);` |
|   210095 | 7949 | `			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),` |
|    70030 | 7950 | `				SyBlobLength(&sMsg));` |
|   140065 | 7951 | `			SyBlobRelease(&sMsg);` |
|   140065 | 7952 | `			if( rc == SXERR_ABORT ){` |
|        6 | 7953 | `				goto Abort;` |
|        - | 7954 | `			}` |
|        - | 7955 | `			/* A catch may have run IN PLACE inside VmThrowFromVm (SXRET_OK +` |
|        - | 7956 | ``			 * recorded resume). An unconditional `goto Exception` here unwound the`` |
|        - | 7957 | `` 			 * exec ANYWAY, so `try { nosuchfn(); } catch (Error $e) {} rest();` `` |
|        - | 7958 | `			 * ran the catch and then silently dropped the rest of the script` |
|        - | 7959 | `			 * (exit 0). Route like every other in-exec throw site. */` |
|   380112 | 7960 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|        - | 7961 | `			}` |
|        - | 7962 | `		}` |
|  4493722 | 7963 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 7964 | `		/* D1: resolve deferred plain-var arguments against the builtin's by-ref position` |
|        - | 7965 | `		 * mask (derived from its signature). A by-ref out-param (preg_match's $matches, …)` |
|        - | 7966 | `		 * is materialized so PH7_VmStoreArgByRef can write back — this is what keeps a` |
|        - | 7967 | ``		 * DYNAMIC-name by-ref builtin (`$f='preg_match'; $f($p,$s,$m)`) working now that the`` |
|        - | 7968 | `		 * compile-time mask no longer sees it. Every other (by-value) arg warns + passes` |
|        - | 7969 | `		 * NULL, which is also what call_user_func & friends want. pVm->pFrame is the caller. */` |
|        - | 7970 | `		{` |
|  4493722 | 7971 | `			sxi32 rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,pFunc->nByRefMask,0,0,pEffCallMap);` |
|  4493738 | 7972 | `			PH7_DISPATCH_ENFORCE_RC(rcDA)` |
|        - | 7973 | `		}` |
|        - | 7974 | `		/* Host function (builtin): build the effective spread-key map so the` |
|        - | 7975 | `		 * name-forwarding builtins (call_user_func & friends) relay string keys as` |
|        - | 7976 | `		 * named args, and — critically — so this call's captured runs are consumed.` |
|        - | 7977 | `		 * pArg is the top base here (a builtin call pops no method-name slot). */` |
|  6742054 | 7978 | `		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  4493679 | 7979 | `			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);` |
|  3021217 | 7980 | `NativeCall:` |
|        - | 7981 | `		/* A VM_FUNC_NATIVE method joins here, having done the two steps above for` |
|        - | 7982 | `		 * itself: its by-ref mask comes from the same signature machinery, and its` |
|        - | 7983 | `		 * effective arg map was already built (and this call's spread runs already` |
|        - | 7984 | `		 * consumed) on the method path — building it a second time here would` |
|        - | 7985 | `		 * consume them twice. Everything from this point down is shared verbatim:` |
|        - | 7986 | `		 * a native method IS a host call that happens to carry a receiver. */` |
|        - | 7987 | `		/* Start collecting function arguments */` |
|  6045506 | 7988 | `		SySetReset(&aArg);` |
| 15010007 | 7989 | `		while( pArg < pTos ){` |
|  8964506 | 7990 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  8964506 | 7991 | `			pArg++;` |
|        5 | 7992 | `		}` |
|        - | 7993 | `		/* Assume a null return value */` |
|  6045506 | 7994 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 7995 | `		/* Init the call context */` |
|  6045506 | 7996 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 7997 | `		/* Hand the call-site named-argument map to the builtin so name-forwarding` |
|        - | 7998 | `		 * helpers (call_user_func & friends) can relay name: arguments — and the` |
|        - | 7999 | `		 * caller's strict_types mode — to the inner callback. Forwarded whole (not` |
|        - | 8000 | `		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even` |
|        - | 8001 | `		 * when its own call site is purely positional; only the two forwarding` |
|        - | 8002 | `		 * builtins read pArgMap, so this is inert for every other host function. */` |
|  6045506 | 8003 | `		sCtx.pArgMap = pEffCallMap;` |
|        - | 8004 | `		/* Receiver + late-static-binding class for a native METHOD. Both stay 0 for a` |
|        - | 8005 | `		 * plain host function (the locals are initialized once per OP_CALL), so this` |
|        - | 8006 | `		 * is inert on the builtin path. The reference on pNativeOwned belongs to the` |
|        - | 8007 | `		 * caller for the span of the call — the native body borrows it and must not` |
|        - | 8008 | `		 * unref, exactly as a bytecode method's frame $this is borrowed. */` |
|  6045506 | 8009 | `		sCtx.pThis = pNativeRecv;` |
|  6045506 | 8010 | `		sCtx.pCalledClass = pNativeClass;` |
|        - | 8011 | `		{` |
|  6045506 | 8012 | `		int nGiven = (int)SySetUsed(&aArg);` |
|        - | 8013 | ``		/* Bind `name:` arguments to the callee's declared POSITIONS before anything`` |
|        - | 8014 | `		 * reads the vector — the arity screen, the ZPP screen and the C body all take` |
|        - | 8015 | `		 * it positionally. A host function has no compiled parameter records for` |
|        - | 8016 | `		 * VmResolveNamedArgs to walk, so its signature string is the source of names` |
|        - | 8017 | `		 * and defaults (PH7_VmBindNamedArgsToSig). Without this every named argument` |
|        - | 8018 | `		 * simply stayed where it was WRITTEN. */` |
|  6045506 | 8019 | `		if( pEffCallMap && pEffCallMap->bHasNamed && nGiven > 0 ){` |
|      143 | 8020 | `			rc = PH7_VmBindNamedArgsToSig(&sCtx,pFunc,pEffCallMap,&nGiven,` |
|       94 | 8021 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|       96 | 8022 | `			if( rc != SXRET_OK ){` |
|        9 | 8023 | `				goto NativeCallDone;` |
|        - | 8024 | `			}` |
|       43 | 8025 | `		}` |
|        - | 8026 | `		/* php binds a by-reference argument at the CALL, before the callee runs, so a` |
|        - | 8027 | ``		 * non-variable in a `&` position is refused ahead of every ZPP check — and`` |
|        - | 8028 | ``		 * ahead of the too-MANY-arguments one (`array_pop([1,2],5)` is the reference`` |
|        - | 8029 | `		 * Error in php, not an ArgumentCountError). With no argument at all there is` |
|        - | 8030 | `		 * nothing to refuse, which is why the too-FEW check below still speaks first` |
|        - | 8031 | ``		 * for `array_pop()`. */`` |
|  9069778 | 8032 | `		rc = PH7_VmScreenByRefArgShapes(&sCtx,pFunc,pEffCallMap,nGiven,` |
|  6045493 | 8033 | `			(ph7_value **)SySetBasePtr(&aArg));` |
|  6045498 | 8034 | `		if( rc != SXRET_OK ){` |
|       57 | 8035 | `			goto NativeCallDone;` |
|        - | 8036 | `		}` |
|        - | 8037 | `		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum` |
|        - | 8038 | `		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError` |
|        - | 8039 | `		 * before the C routine runs when called with too few arguments — instead` |
|        - | 8040 | `		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""` |
|        - | 8041 | `		 * return. The message wording matches php's ZPP output byte-for-byte. */` |
|  9069700 | 8042 | `		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){` |
|      908 | 8043 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 8044 | `				"%z() expects %s %d argument%s, %d given",` |
|      301 | 8045 | `				&pFunc->sName,` |
|      602 | 8046 | `				pFunc->bAtLeast ? "at least" : "exactly",` |
|      602 | 8047 | `				(int)pFunc->nMinArg,` |
|      602 | 8048 | `				pFunc->nMinArg == 1 ? "" : "s",` |
|      301 | 8049 | `				nGiven);` |
|  6045145 | 8050 | `		}else if( pFunc->bHasMaxArg && nGiven > (int)pFunc->nMaxArg ){` |
|        - | 8051 | `			/* php enforces the MAXIMUM as well, and PHL only did so where a` |
|        - | 8052 | `			 * builtin happened to hand-roll the check (51 of ~650), so` |
|        - | 8053 | ``			 * `microtime(1,2)`, `strlen("a","b")` and friends silently ignored`` |
|        - | 8054 | `			 * the extras. The count comes from the same signature table as the` |
|        - | 8055 | `			 * minimum; a variadic tail leaves nMaxArg at -1 and is exempt.` |
|        - | 8056 | `			 * php says "exactly" when the bounds coincide, "at most" otherwise. */` |
|      263 | 8057 | `			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",` |
|        - | 8058 | `				"%z() expects %s %d argument%s, %d given",` |
|       86 | 8059 | `				&pFunc->sName,` |
|      143 | 8060 | `				((int)pFunc->nMinArg == (int)pFunc->nMaxArg && !pFunc->bAtLeast) ? "exactly" : "at most",` |
|      172 | 8061 | `				(int)pFunc->nMaxArg,` |
|      172 | 8062 | `				pFunc->nMaxArg == 1 ? "" : "s",` |
|       86 | 8063 | `				nGiven);` |
|  9068625 | 8064 | `		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,` |
|  6044667 | 8065 | `			(ph7_value **)SySetBasePtr(&aArg))) ){` |
|        - | 8066 | `			/* TypeError thrown: rc carries the caught/uncaught status */` |
|      579 | 8067 | `		}else{` |
|        - | 8068 | `			/* The name of the builtin that is RUNNING, for the few diagnostics` |
|        - | 8069 | `			 * raised so deep inside the engine that no ph7_context reaches them` |
|        - | 8070 | `			 * (a stream filter's, from inside a device read) and which php still` |
|        - | 8071 | `			 * prefixes with the caller. Saved and restored: a builtin can call` |
|        - | 8072 | `			 * back into php and reach this line again. */` |
|  6043524 | 8073 | `			SyString *pSavedCallee = pVm->pCalleeName;` |
|        - | 8074 | `			/* php's two callback FORWARDS pass "the answer is being dropped" on to` |
|        - | 8075 | `			 * the callback they drive; every other builtin ignores this. Saved and` |
|        - | 8076 | `			 * restored for the same reason the callee name is. */` |
|  6043524 | 8077 | `			int bSavedHostDiscard = pVm->bHostDiscard;` |
|  6043524 | 8078 | `			pVm->pCalleeName = &pFunc->sName;` |
|  6043524 | 8079 | `			pVm->bHostDiscard = bResultDropped && bLiteralCallee;` |
|        - | 8080 | `			/* Call the foreign function */` |
|  6043524 | 8081 | `			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));` |
|  6043524 | 8082 | `			pVm->bHostDiscard = bSavedHostDiscard;` |
|  6043524 | 8083 | `			pVm->pCalleeName = pSavedCallee;` |
|  6043524 | 8084 | `			if( PH7_CmpRefusalPending(pVm) ){` |
|        - | 8085 | `				/* A native compare handler refused a pair this builtin compared` |
|        - | 8086 | `				 * (in_array, sort, max and switch all drive the same comparator,` |
|        - | 8087 | `				 * which has no throw boundary of its own and only recorded it).` |
|        - | 8088 | `				 * php raises out of the comparison and the builtin never finishes;` |
|        - | 8089 | `				 * this one finishes first and then throws, the way every builtin` |
|        - | 8090 | `				 * whose failure is predicted rather than raised in flight does.` |
|        - | 8091 | `				 * Reported on the call context, so VmHostFuncThrowRc below lands` |
|        - | 8092 | `				 * it exactly as the builtin's own throws are landed -- unless the` |
|        - | 8093 | `				 * builtin ALREADY raised, in which case the first throw wins and` |
|        - | 8094 | `				 * the record is only dropped. */` |
|       16 | 8095 | `				if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND` |
|       17 | 8096 | `				 \|\| sCtx.nThrowRc != 0 ){` |
|      ! 0 | 8097 | `					PH7_CmpRefusalClear(&(*pVm));` |
|      ! 0 | 8098 | `				}else{` |
|       17 | 8099 | `					PH7_CmpRefusalRaiseCtx(&sCtx);` |
|        - | 8100 | `				}` |
|        8 | 8101 | `			}` |
|        - | 8102 | `			/* A host function that RAISED a catchable throw (PH7_VmThrowException)` |
|        - | 8103 | `			 * and still returned PH7_OK reports it here — the throw's catch has` |
|        - | 8104 | `			 * already run in place, so treating the call as a normal return would` |
|        - | 8105 | `			 * resume execution INSIDE the try body the throw abandoned (and, when` |
|        - | 8106 | `			 * uncaught, run on past the reported fatal). The status is recorded on` |
|        - | 8107 | `			 * the call context, so this covers the shared validation helpers whose` |
|        - | 8108 | `			 * callers have no channel to thread a status back. */` |
|  6043524 | 8109 | `			rc = VmHostFuncThrowRc(&sCtx,rc);` |
|        - | 8110 | `		}` |
|  3021217 | 8111 | `NativeCallDone:` |
|  3024284 | 8112 | `		(void)nGiven; /* the named-arg binder's early exit lands here */` |
|        - | 8113 | `		}` |
|        - | 8114 | `		/* Release the call context */` |
|  6045506 | 8115 | `		VmReleaseCallContext(&sCtx);` |
|  6045506 | 8116 | `		if( pNativeOwned ){` |
|        - | 8117 | `			/* Drop the receiver reference the method branch took (pThis->iRef++ before` |
|        - | 8118 | `			 * the target slot was released). A bytecode method hands this to its frame` |
|        - | 8119 | `			 * and lets the teardown do it; a native method has no frame, so it is` |
|        - | 8120 | `			 * dropped here — the one point EVERY route out of the call passes through,` |
|        - | 8121 | `			 * including the throw/abort/suspend ones below. Ordered after the context` |
|        - | 8122 | `			 * release because sCtx.sThis aliases the instance. Always 0 for a plain` |
|        - | 8123 | `			 * host function. */` |
|  1549415 | 8124 | `			PH7_ClassInstanceUnref(pNativeOwned);` |
|  1549415 | 8125 | `			pNativeOwned = 0;` |
|  1549415 | 8126 | `			pNativeRecv = 0;` |
|   774708 | 8127 | `		}` |
|  6045506 | 8128 | `		if( rc == PH7_ABORT ){` |
|        - | 8129 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 8130 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 8131 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      681 | 8132 | `			PH7_MemObjRelease(&sRet);` |
|      681 | 8133 | `			goto Abort;` |
|        - | 8134 | `		}` |
|  6044830 | 8135 | `		if( rc != PH7_SUSPEND && pVm->pInlineInstr == (void *)aInstr ){` |
|        - | 8136 | `			/* A throw raised inside this host function — directly` |
|        - | 8137 | `			 * (PH7_VmThrowException) or by a PHP callback it invoked — was` |
|        - | 8138 | `			 * caught by an INLINE try (generator body) THIS exec owns.` |
|        - | 8139 | `			 * VmThrowInline records only a pc-redirect: a direct builtin throw` |
|        - | 8140 | `			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION` |
|        - | 8141 | `			 * with the redirect pending, so the rc branches below never land` |
|        - | 8142 | `			 * it (pre-existing hole: explode("") or a throwing usort` |
|        - | 8143 | `			 * comparator inside a generator's try lost the catch AND the` |
|        - | 8144 | `			 * yield). Land at the redirect now — its drain to the try's` |
|        - | 8145 | `			 * operand base subsumes the args + name pops. */` |
|       10 | 8146 | `			PH7_MemObjRelease(&sRet);` |
|       32 | 8147 | `			PH7_INLINE_RESUME_BREAK()` |
|      ! 0 | 8148 | `		}` |
|  6044822 | 8149 | `		if( rc == PH7_EXCEPTION ){` |
|        - | 8150 | `			/* A callback invoked by this host function threw. If an in-place catch` |
|        - | 8151 | `			 * recorded a resume target owned by THIS body, resume at its landing pad` |
|        - | 8152 | `			 * (consuming the target); otherwise the exception was caught by an outer` |
|        - | 8153 | `			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW` |
|        - | 8154 | `			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which` |
|        - | 8155 | `			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */` |
|        - | 8156 | `			sxi32 iResumePc;` |
|    15300 | 8157 | `			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){` |
|        - | 8158 | `				/* Caught by an outer exec, or not caught here: propagate. */` |
|     3654 | 8159 | `				goto Exception;` |
|        - | 8160 | `			}` |
|        - | 8161 | `			/* Exception was caught in place by THIS body's try: pop args and the` |
|        - | 8162 | `			 * result slot, then drain any abandoned outer-expression operands to` |
|        - | 8163 | `			 * the try's base and resume. */` |
|    11651 | 8164 | `			PH7_MemObjRelease(&sRet);` |
|    11651 | 8165 | `			if( nCallArgs > 0 ){` |
|    11107 | 8166 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|     5551 | 8167 | `			}` |
|    11651 | 8168 | `			VmPopOperand(&pTos,1);` |
|    16229 | 8169 | `			PH7_RESUME_DRAIN()` |
|    11651 | 8170 | `			pc = iResumePc;` |
|    11651 | 8171 | `			break;` |
|        - | 8172 | `		}` |
|  6029527 | 8173 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 8174 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 8175 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 8176 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 8177 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 8178 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 8179 | `			 * body), the user-function path above will handle re-saving. */` |
|      367 | 8180 | `			PH7_MemObjRelease(&sRet);` |
|      367 | 8181 | `			if( nCallArgs > 0 ){` |
|      359 | 8182 | `				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */` |
|      177 | 8183 | `			}` |
|        - | 8184 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 8185 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|      367 | 8186 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|      367 | 8187 | `			goto Suspend;` |
|        - | 8188 | `		}` |
|  6029165 | 8189 | `		if( nCallArgs > 0 ){` |
|        - | 8190 | `			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack` |
|        - | 8191 | `			 * expanded the compile-time arg count on the stack, so popping iP1` |
|        - | 8192 | `			 * strands the extra elements (or, for an unpack that expanded to fewer` |
|        - | 8193 | `			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,` |
|        - | 8194 | `			 * reading a bogus value whose stray flags sent MemObjStore into the` |
|        - | 8195 | `			 * hashmap-release path and hung). Mirrors every other CALL exit. The` |
|        - | 8196 | `			 * function-name slot (pTos) receives the return value below. */` |
|  5950657 | 8197 | `			VmPopOperand(&pTos,nCallArgs);` |
|  2976858 | 8198 | `		}` |
|        - | 8199 | `		/* Save foreign function return value into the (now top) function-name slot */` |
|  6029165 | 8200 | `		PH7_MemObjStore(&sRet,pTos);` |
|        - | 8201 | `		/* ...and clear that slot's index. It is one of the call's own argument slots,` |
|        - | 8202 | `		 * still carrying the variable index the argument was loaded with, and` |
|        - | 8203 | `		 * PH7_MemObjStore does not touch nIdx — so a builtin's return value came back` |
|        - | 8204 | ``		 * looking like an lvalue for the caller's variable (`f(strtoupper($b))` with`` |
|        - | 8205 | ``		 * `function f(&$x)` overwrote `$b`). No host function returns by reference. */`` |
|  6029165 | 8206 | `		pTos->nIdx = SXU32_HIGH;` |
|  6029165 | 8207 | `		PH7_MemObjRelease(&sRet);` |
|        - | 8208 | `	}` |
|  6033483 | 8209 | `	break;` |
|        - | 8210 | `				  }` |
|        - | 8211 | `/*` |
|        - | 8212 | ` * OP_CONSUME: P1 * *` |
|        - | 8213 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 8214 | ` */` |
|    70540 | 8215 | `case PH7_OP_CONSUME: {` |
|        - | 8216 | `	VmOpRc rcOp;` |
|   141085 | 8217 | `	sState.pTos = pTos;` |
|   141085 | 8218 | `	sState.pc = pc;` |
|   141085 | 8219 | `	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);` |
|   141085 | 8220 | `	pTos = sState.pTos;` |
|   141085 | 8221 | `	pc = sState.pc;` |
|   141085 | 8222 | `	if( rcOp == VM_OP_ABORT ){` |
|        3 | 8223 | `		goto Abort;` |
|   141083 | 8224 | `	}else if( rcOp == VM_OP_EXCEPTION ){` |
|       14 | 8225 | `		goto Exception;` |
|        - | 8226 | `	}` |
|   141066 | 8227 | `	break;` |
|        - | 8228 | `					  }` |
|        - | 8229 |  |
|        - | 8230 | `		} /* Switch() */` |
| 64607942 | 8231 | `		pc++; /* Next instruction in the stream */` |
|        5 | 8232 | `	} /* For(;;) */` |
|  2194610 | 8233 | `Done:` |
|        - | 8234 | `	/* A stacked callee completing lands here too (its result is already in` |
|        - | 8235 | `	 * sState.pResult — the caller's operand slot); Unwind's first iteration` |
|        - | 8236 | `	 * bottoms out identically for the record-less case. */` |
|  4389648 | 8237 | `	rc = SXRET_OK;` |
|  4389648 | 8238 | `	goto Unwind;` |
|      884 | 8239 | `Suspend:` |
|     1773 | 8240 | `	rc = PH7_SUSPEND;` |
|     1773 | 8241 | `	if( pCallTop != 0 ){` |
|        - | 8242 | `		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment` |
|        - | 8243 | `		 * instead of the lossy unwind. pc/nTos of the innermost activation were` |
|        - | 8244 | `		 * already saved into the ctx by VmSuspendCtx; capture the rest (the` |
|        - | 8245 | `		 * record chain, the innermost activation, the suspend-time top frame)` |
|        - | 8246 | `		 * so resume re-enters HERE, inside the innermost callee, like php. The` |
|        - | 8247 | `		 * records / frames / operand stacks stay alive — nothing is freed. Only` |
|        - | 8248 | `		 * fibers reach this (generators yield only at their body level, pCallTop` |
|        - | 8249 | `		 * == 0); a suspend inside a C->PHP callback was already rejected with a` |
|        - | 8250 | `		 * FiberError before it could arrive here. */` |
|      306 | 8251 | `		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));` |
|      306 | 8252 | `		if( pSeg == 0 ){` |
|        - | 8253 | `			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST` |
|        - | 8254 | `			 * pc/nTos into the ctx, so the lossy body-level fallback would resume` |
|        - | 8255 | `			 * the callee's pc against the body stack — silent corruption, exactly` |
|        - | 8256 | `			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the` |
|        - | 8257 | `			 * §3.1 convention shared with the stage-2 record-alloc OOM site). */` |
|      ! 0 | 8258 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 | 8259 | `			rc = PH7_ABORT;` |
|      ! 0 | 8260 | `			goto Unwind;` |
|        - | 8261 | `		}` |
|      306 | 8262 | `		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */` |
|      306 | 8263 | `		pSeg->sState = sState;` |
|      306 | 8264 | `		pSeg->pCallTop = pCallTop;` |
|      306 | 8265 | `		pSeg->pTopFrame = pVm->pFrame;` |
|      306 | 8266 | `		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;` |
|      306 | 8267 | `		pSeg->nOldFinBase = pVm->pActiveCtx ? pVm->pActiveCtx->nFinallyBase : 0;` |
|        - | 8268 | `		{` |
|        - | 8269 | `			VmCallFrame *pRec;` |
|      306 | 8270 | `			pSeg->nRecords = 0;` |
|      608 | 8271 | `			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){` |
|      306 | 8272 | `				pSeg->nRecords++;` |
|      155 | 8273 | `			}` |
|        - | 8274 | `		}` |
|      306 | 8275 | `		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;` |
|        - | 8276 | `		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */` |
|      306 | 8277 | `		SySetRelease(&aArg);` |
|      306 | 8278 | `		return PH7_SUSPEND;` |
|        - | 8279 | `	}` |
|     1471 | 8280 | `	goto Unwind;` |
|      483 | 8281 | `Abort:` |
|      971 | 8282 | `	rc = PH7_ABORT;` |
|      971 | 8283 | `	goto Unwind;` |
|   304935 | 8284 | `Exception:` |
|   609874 | 8285 | `	rc = PH7_EXCEPTION;` |
|   609869 | 8286 | `	goto Unwind;` |
|  2500761 | 8287 | `Unwind:` |
|        - | 8288 | `	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration` |
|        - | 8289 | `	 * finishes the top record exactly as the old per-level native return did:` |
|        - | 8290 | `	 * for ABORT/EXCEPTION, first run what the popped activation's own` |
|        - | 8291 | `	 * Abort/Exception label used to do (clear its pending return, release its` |
|        - | 8292 | `	 * operands — a stacked activation never has bReturnPropagates set), then` |
|        - | 8293 | `	 * VmCallFinish routes in the restored caller (an in-place catch there` |
|        - | 8294 | `	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —` |
|        - | 8295 | `	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible` |
|        - | 8296 | `	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).` |
|        - | 8297 | `	 * At the bottom, VmExecFinalize hands the status to the native caller. */` |
|  2701680 | 8298 | `	for(;;){` |
|  5402943 | 8299 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|        - | 8300 | `			/* Drop any pending hook-RMW write-backs this activation armed — its` |
|        - | 8301 | `			 * statement is abandoned (only the innermost activation at throw time` |
|        - | 8302 | `			 * can own entries: the armed window spans exactly one instruction, so` |
|        - | 8303 | `			 * no OP_CALL record ever intervenes). */` |
|  1011835 | 8304 | `			while( SySetUsed(&pVm->aHookRmw) > 0` |
|  1011836 | 8305 | `			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){` |
|        3 | 8306 | `				VmHookRmwDropTop(&(*pVm));` |
|        1 | 8307 | `			}` |
|   505914 | 8308 | `		}` |
|  5402943 | 8309 | `		if( pCallTop == 0 ){` |
|  4614197 | 8310 | `			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);` |
|        - | 8311 | `		}` |
|   788751 | 8312 | `		if( rc == PH7_ABORT \|\| rc == PH7_EXCEPTION ){` |
|   605427 | 8313 | `			VmClearFramePending(sState.pEntryFrame);` |
|   614779 | 8314 | `			while( pTos >= pStack ){` |
|     9357 | 8315 | `				PH7_MemObjRelease(pTos);` |
|     9357 | 8316 | `				pTos--;` |
|        5 | 8317 | `			}` |
|   302711 | 8318 | `		}` |
|   788751 | 8319 | `		if( rc != PH7_SUSPEND ){` |
|        - | 8320 | `			/* The finishing callee's own leaked finally actions must not survive` |
|        - | 8321 | `			 * into the caller (whose next OP_END_FINALLY would mis-pop them). */` |
|   788749 | 8322 | `			VmDiscardFinallyActions(&(*pVm),sState.nFinallyActBase);` |
|   394587 | 8323 | `		}` |
|        - | 8324 | `		{` |
|   788751 | 8325 | `			VmCallFrame *pRec = pCallTop;` |
|   788751 | 8326 | `			sState = pRec->sCaller;` |
|   788751 | 8327 | `			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);` |
|   788751 | 8328 | `			pCallTop = pRec->pPrev;` |
|   788751 | 8329 | `			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;` |
|   788751 | 8330 | `			pVm->pIdleCallFrames = (void *)pRec;` |
|   788751 | 8331 | `			aInstr = sState.aInstr;` |
|   788751 | 8332 | `			pStack = sState.pStack;` |
|   788751 | 8333 | `			pTos = sState.pTos;` |
|   788751 | 8334 | `			pc = sState.pc;` |
|        - | 8335 | `		}` |
|   788751 | 8336 | `		if( rc == PH7_OK ){` |
|   387753 | 8337 | `			pc++; /* the loop-bottom increment this OP_CALL missed */` |
|   387753 | 8338 | `			goto VmLoopFetch;` |
|        - | 8339 | `		}` |
|        5 | 8340 | `	}` |
|  2307247 | 8341 | `}` |
|        - | 8342 |  |
