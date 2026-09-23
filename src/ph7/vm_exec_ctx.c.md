# src/ph7/vm_exec_ctx.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1253/1516 lines (82.65%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * Section:` |
|       - |    9 | ` *    Execution contexts: the parked-stack machinery shared by Fibers and` |
|       - |   10 | ` *    Generators, closure creation/binding (Closure_* builtins), the` |
|       - |   11 | ` *    Fiber_* and Generator_* builtins and the PH7_VmFiber* API.` |
|       - |   12 | ` *    Registration happens via vm.c (aVmFunc[]/class installs).` |
|       - |   13 | ` * Status:` |
|       - |   14 | ` *    Stable.` |
|       - |   15 | ` */` |
|       - |   16 | `/*` |
|       - |   17 | ` * Allocate and initialize a new execution context for a fiber.` |
|       - |   18 | ` * The context is in CREATED state and ready to be started.` |
|       - |   19 | ` */` |
|     672 |   20 | `PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|       5 |   21 | `{` |
|       - |   22 | `	ph7_exec_ctx *pCtx;` |
|       - |   23 | `	ph7_value *pStack;` |
|       - |   24 | `	VmFrame *pFrame;` |
|     677 |   25 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|     677 |   26 | `	if( pCtx == 0 ){` |
|     ! 0 |   27 | `		return 0;` |
|       - |   28 | `	}` |
|     677 |   29 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|     677 |   30 | `	pCtx->pVm = pVm;` |
|     677 |   31 | `	pCtx->pFunc = pFunc;` |
|     677 |   32 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|     677 |   33 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|     677 |   34 | `	pCtx->pc = 0;` |
|     677 |   35 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|     677 |   36 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|     677 |   37 | `	PH7_MemObjInit(pVm, &pCtx->sDelegate);` |
|       - |   38 | `	/* Container for this body's own exception handlers while suspended (borrowed` |
|       - |   39 | `	 * ph7_exception* pointers — never freed here, owned by the compiled func). */` |
|     677 |   40 | `	SySetInit(&pCtx->aSavedException, &pVm->sAllocator, sizeof(ph7_exception *));` |
|       - |   41 | `	/* ROOT C: this body's own pending finally actions while suspended. */` |
|     677 |   42 | `	SySetInit(&pCtx->aSavedFinally, &pVm->sAllocator, sizeof(VmFinallyAction));` |
|     677 |   43 | `	pCtx->nFinallyBase = 0;` |
|       - |   44 | `	/* Stage 4: this coroutine's own aSelf entries (self::/static:: class context` |
|       - |   45 | `	 * pushed by nested method calls still open at suspend) parked while suspended,` |
|       - |   46 | `	 * so they don't pollute the resumer's aSelf. Borrowed ph7_class* pointers. */` |
|     677 |   47 | `	SySetInit(&pCtx->aSavedSelf, &pVm->sAllocator, sizeof(ph7_class *));` |
|     677 |   48 | `	pCtx->nSelfBase = 0;` |
|       - |   49 | `	/* Caller slots this body's by-reference parameters alias (see the struct). */` |
|     677 |   50 | `	SySetInit(&pCtx->aByRefArg, &pVm->sAllocator, sizeof(sxu32));` |
|     677 |   51 | `	pCtx->pParkedSegment = 0;` |
|     677 |   52 | `	pCtx->nBodyExecDepth = 0;` |
|       - |   53 | `	/* Allocate a private operand stack */` |
|     677 |   54 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|     677 |   55 | `	if( pStack == 0 ){` |
|     ! 0 |   56 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   57 | `		return 0;` |
|       - |   58 | `	}` |
|     677 |   59 | `	pCtx->pStack = pStack;` |
|     677 |   60 | `	pCtx->nStackCap = SySetUsed(&pFunc->aByteCode) + VM_STACK_GUARD; /* grows with pStack on OP_SPREAD */` |
|     677 |   61 | `	pCtx->nStackOrig = pCtx->nStackCap; /* fixed original — headroom reference across resumes */` |
|       - |   62 | `	/* Create a detached frame for the fiber */` |
|     677 |   63 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|     677 |   64 | `	if( pFrame == 0 ){` |
|     ! 0 |   65 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|     ! 0 |   66 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   67 | `		return 0;` |
|       - |   68 | `	}` |
|     677 |   69 | `	pCtx->pFrame = pFrame;` |
|     677 |   70 | `	return pCtx;` |
|     341 |   71 | `}` |
|       - |   72 | `/*` |
|       - |   73 | ` * A suspended coroutine must not leave its own slices of the VM's shared stacks` |
|       - |   74 | ` * sitting above the caller's depth. Three stacks are affected, identically:` |
|       - |   75 | ` *   - pVm->aException: its exception handlers — else a generator/fiber suspended` |
|       - |   76 | ` *     inside a try leaves handlers referencing its now-detached frame on the` |
|       - |   77 | ` *     global stack, corrupting the caller's try/catch.` |
|       - |   78 | ` *   - pVm->aFinallyAction (ROOT C): its pending finally actions — else a yield` |
|       - |   79 | ` *     inside a finally (reached by return/break/rethrow) leaves a record where an` |
|       - |   80 | ` *     out-of-order-resumed sibling generator's OP_END_FINALLY would mis-pop it.` |
|       - |   81 | ` *   - pVm->aSelf (stage 4): its self::/static:: entries pushed by still-open` |
|       - |   82 | ` *     nested method calls — else they sit on the resumer's aSelf and corrupt its` |
|       - |   83 | ` *     self:: resolution.` |
|       - |   84 | ` * Each is the same operation: on suspend move the slice above a captured base` |
|       - |   85 | ` * into a per-ctx park buffer; on resume re-publish it at the (refreshed) caller` |
|       - |   86 | ` * depth. VmParkStackSlice / VmRestoreStackSlice factor it for any element type` |
|       - |   87 | ` * (size taken from the SySet); VmParkCtxState / VmRestoreCtxState drive all three.` |
|       - |   88 | ` *` |
|       - |   89 | ` * Stage 4: the whole suspended segment stays alive, so a parked handler's owner` |
|       - |   90 | ` * frame is never freed underneath it — the parked pointer stays valid and is kept` |
|       - |   91 | ` * (the old stage-2b lossy-path invalidation is gone with the discard). A` |
|       - |   92 | ` * body-level suspend only ever has body-owned handlers here, and its finally/self` |
|       - |   93 | ` * slices are empty (all nested calls already returned) — so those are no-ops.` |
|       - |   94 | ` */` |
|    5040 |   95 | `static void VmParkStackSlice(SySet *pFrom, SySet *pSaved, sxu32 nBase)` |
|       5 |   96 | `{` |
|    5045 |   97 | `	sxu32 nUsed = SySetUsed(pFrom);` |
|    5045 |   98 | `	if( nUsed > nBase ){` |
|     231 |   99 | `		const char *aBase = (const char *)SySetBasePtr(pFrom);` |
|       - |  100 | `		sxu32 i;` |
|     463 |  101 | `		for( i = nBase; i < nUsed; i++ ){` |
|     237 |  102 | `			SySetPut(pSaved, (const void *)(aBase + i * pFrom->eSize));` |
|     121 |  103 | `		}` |
|     231 |  104 | `		SySetTruncate(pFrom, nBase);` |
|     113 |  105 | `	}` |
|    5045 |  106 | `}` |
|    4158 |  107 | `static void VmRestoreStackSlice(SySet *pTo, SySet *pSaved)` |
|       5 |  108 | `{` |
|    4163 |  109 | `	sxu32 i, n = SySetUsed(pSaved);` |
|    4163 |  110 | `	if( n > 0 ){` |
|     219 |  111 | `		const char *aSaved = (const char *)SySetBasePtr(pSaved);` |
|     439 |  112 | `		for( i = 0; i < n; i++ ){` |
|     225 |  113 | `			SySetPut(pTo, (const void *)(aSaved + i * pSaved->eSize));` |
|     115 |  114 | `		}` |
|     219 |  115 | `		SySetReset(pSaved);` |
|     107 |  116 | `	}` |
|    4163 |  117 | `}` |
|    1680 |  118 | `static void VmParkCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  119 | `{` |
|    1685 |  120 | `	VmParkStackSlice(&pVm->aException, &pCtx->aSavedException, pCtx->nExceptionBase);` |
|    1685 |  121 | `	VmParkStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally, pCtx->nFinallyBase);` |
|    1685 |  122 | `	VmParkStackSlice(&pVm->aSelf, &pCtx->aSavedSelf, pCtx->nSelfBase);` |
|    1685 |  123 | `}` |
|    1386 |  124 | `static void VmRestoreCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  125 | `{` |
|    1391 |  126 | `	VmRestoreStackSlice(&pVm->aException, &pCtx->aSavedException);` |
|    1391 |  127 | `	VmRestoreStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally);` |
|    1391 |  128 | `	VmRestoreStackSlice(&pVm->aSelf, &pCtx->aSavedSelf);` |
|    1391 |  129 | `}` |
|       - |  130 | `/*` |
|       - |  131 | ` * On suspend, free the exception (try) frames the yield was nested in. They were` |
|       - |  132 | ` * pushed by OP_LOAD_EXCEPTION between the coroutine body frame (pCtx->pFrame) and` |
|       - |  133 | ` * the current suspend-point top frame. The generator/fiber frame model saves only` |
|       - |  134 | ` * the body frame, so these transparent wrappers would otherwise be orphaned and` |
|       - |  135 | ` * leak on every yield-that-sits-inside-a-try (unbounded for a generator looping` |
|       - |  136 | ` * with a yield in a try). Freeing them loses nothing the resume needs: this body's` |
|       - |  137 | ` * exception HANDLERS are parked separately (VmParkCtxState) and each` |
|       - |  138 | ` * try's landing pad lives on its ph7_exception (iLandingPc), while OP_POP_EXCEPTION` |
|       - |  139 | ` * on resume skips the (now absent) frame pop via its VM_FRAME_EXCEPTION guard and` |
|       - |  140 | ` * OP_LOAD_EXCEPTION re-creates a fresh wrapper when the try is next entered. Must` |
|       - |  141 | ` * run while pVm->pFrame still points at the suspend-time top (before the detach).` |
|       - |  142 | ` */` |
|    1716 |  143 | `static void VmFreeSuspendedExceptionFrames(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  144 | `{` |
|    1933 |  145 | `	while( pVm->pFrame != pCtx->pFrame && (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     217 |  146 | `		VmLeaveFrame(&(*pVm));` |
|       5 |  147 | `	}` |
|    1721 |  148 | `}` |
|       - |  149 | `/*` |
|       - |  150 | ` * Common suspend epilogue for VmStartCtx / VmResumeCtx: detach the suspended` |
|       - |  151 | ` * coroutine from the live VM chain and park its exception handlers. Two forms:` |
|       - |  152 | ` *   - Body-level (pParkedSegment == 0): a generator yield or a fiber suspending` |
|       - |  153 | ` *     directly in its body. The try wrappers the yield sat in are transient —` |
|       - |  154 | ` *     free them (OP_LOAD_EXCEPTION recreates them on re-entry) — and detach the` |
|       - |  155 | ` *     body frame alone.` |
|       - |  156 | ` *   - Deep fiber suspend (pParkedSegment != 0, stage 4): the whole segment (body` |
|       - |  157 | ` *     frame + the nested call/try frames above it) stays alive and is detached` |
|       - |  158 | ` *     as a unit; nothing is freed, so resume can continue inside the innermost` |
|       - |  159 | ` *     callee. Its handlers are parked the same way and rebased on resume.` |
|       - |  160 | ` */` |
|    1680 |  161 | `static void VmSuspendCtxDetach(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  162 | `{` |
|    1685 |  163 | `	if( pCtx->pParkedSegment == 0 ){` |
|    1383 |  164 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     694 |  165 | `	}else{` |
|       - |  166 | `		/* The parked records' push-time accounting (one nRecursionDepth++ each,` |
|       - |  167 | `		 * plus VmCallFinish's aSelf pop, which never ran) leaves the segment` |
|       - |  168 | `		 * counted as active while the fiber is suspended — deactivate it. aSelf` |
|       - |  169 | `		 * is parked wholesale below (base-relative), so drop only the depth. */` |
|     306 |  170 | `		pVm->nRecursionDepth -= ((VmParkedSegment *)pCtx->pParkedSegment)->nRecords;` |
|       - |  171 | `	}` |
|    1685 |  172 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|    1685 |  173 | `	pCtx->pFrame->pParent = 0;` |
|    1685 |  174 | `	VmParkCtxState(pVm, pCtx);` |
|    1685 |  175 | `	if( pResult ){` |
|     363 |  176 | `		PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|     179 |  177 | `	}` |
|    1685 |  178 | `}` |
|       - |  179 | `/*` |
|       - |  180 | ` * The return-type enforcement target for a coroutine body run. A GENERATOR` |
|       - |  181 | ` * function's declared return type belongs to the call site (always a Generator` |
|       - |  182 | ` * object, validated at compile time as "a supertype of Generator"); the body's` |
|       - |  183 | ` * own return value feeds getReturn() and is never type-checked. Gate on the` |
|       - |  184 | ` * VM_FUNC_GENERATOR flag (the semantic property), not pPrivate (a wrapper-linkage` |
|       - |  185 | ` * fact): a Fiber given a generator-flagged callable runs with pPrivate == 0 and` |
|       - |  186 | ` * must not enforce either. Ordinary fiber callables keep their declared` |
|       - |  187 | ` * return-type enforcement (php enforces it).` |
|       - |  188 | ` */` |
|    2018 |  189 | `static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx)` |
|       5 |  190 | `{` |
|    1216 |  191 | `	return ((pCtx->pFunc->iFlags & VM_FUNC_GENERATOR) == 0 && VmFuncHasReturnType(pCtx->pFunc))` |
|    1211 |  192 | `		? pCtx->pFunc : 0;` |
|       5 |  193 | `}` |
|       - |  194 | `/*` |
|       - |  195 | ` * Common epilogue for VmStartCtx / VmResumeCtx once the body run returns rc:` |
|       - |  196 | ` * restore the previous active context, then park on suspend or detach the` |
|       - |  197 | ` * coroutine frame and record the terminal state. On normal completion the value` |
|       - |  198 | ` * belongs to getReturn() only — start()/resume() return the NEXT suspend value,` |
|       - |  199 | ` * which is null at completion (php parity), so pResult is left at its` |
|       - |  200 | ` * caller-initialized null.` |
|       - |  201 | ` */` |
|    2018 |  202 | `static sxi32 VmFinishCtxRun(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_exec_ctx *pOldCtx,` |
|       - |  203 | `	sxi32 rc, ph7_value *pResult)` |
|       5 |  204 | `{` |
|    2023 |  205 | `	pVm->pActiveCtx = pOldCtx;` |
|    2023 |  206 | `	if( rc == PH7_SUSPEND ){` |
|       - |  207 | `		/* Handles a deep RE-suspend too (a resumed segment parking a fresh one):` |
|       - |  208 | `		 * VmSuspendCtxDetach deactivates the new segment's recursion accounting,` |
|       - |  209 | `		 * parks its aSelf, and skips VmFreeSuspendedExceptionFrames for a segment` |
|       - |  210 | `		 * so it can't free the still-live parked try wrappers. */` |
|    1685 |  211 | `		VmSuspendCtxDetach(pVm, pCtx, pResult);` |
|    1685 |  212 | `		return SXRET_OK;` |
|       - |  213 | `	}` |
|       - |  214 | ``	/* A finally entered via the throw redirect whose `return` short-circuited`` |
|       - |  215 | `	 * OP_END_FINALLY leaves the try's transparent wrapper ABOVE the body frame —` |
|       - |  216 | `	 * the detach below would then be skipped and the wrapper (plus the body` |
|       - |  217 | `	 * frame) leak into the RESUMER's frame chain, so the next try at that scope` |
|       - |  218 | `	 * records the wrong owner frame and its caught throw silently unwinds the` |
|       - |  219 | `	 * script. Free trailing exception wrappers exactly like the suspend path. */` |
|     343 |  220 | `	if( pCtx->pParkedSegment == 0 ){` |
|     343 |  221 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     169 |  222 | `	}` |
|       - |  223 | `	/* Detach the coroutine frame from the live chain, unless a deeper unwind` |
|       - |  224 | `	 * already moved pVm->pFrame off it. */` |
|     343 |  225 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|     343 |  226 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|     343 |  227 | `		pCtx->pFrame->pParent = 0;` |
|     169 |  228 | `	}` |
|     343 |  229 | `	if( rc == PH7_ABORT ){` |
|       3 |  230 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       3 |  231 | `		return PH7_ABORT;` |
|       - |  232 | `	}` |
|     341 |  233 | `	if( rc == PH7_EXCEPTION ){` |
|      47 |  234 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      47 |  235 | `		return PH7_EXCEPTION;` |
|       - |  236 | `	}` |
|     299 |  237 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|     299 |  238 | `	return SXRET_OK;` |
|    1014 |  239 | `}` |
|       - |  240 | `/*` |
|       - |  241 | ` * Start executing a fiber context for the first time.` |
|       - |  242 | ` */` |
|     632 |  243 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  244 | `{` |
|       - |  245 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  246 | `	sxi32 rc;` |
|     637 |  247 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|     ! 0 |  248 | `		return SXERR_INVALID;` |
|       - |  249 | `	}` |
|       - |  250 | `	/* A fiber/generator start is a native VmByteCodeExec re-entry, bounded by` |
|       - |  251 | `	 * nMaxNativeDepth. Reject HERE, before attaching the frame / mutating VM` |
|       - |  252 | `	 * state, so the abort is clean — the wrapper's own check fires only after` |
|       - |  253 | `	 * this function has spliced the coroutine into the frame chain, which its` |
|       - |  254 | `	 * post-exec detach cannot fully unwind. The PHP call-depth cap belongs to` |
|       - |  255 | `	 * OP_CALL only (BYTECODE.md stage 5). */` |
|     637 |  256 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  257 | `		return VmNativeNestingFatal(pVm);` |
|       - |  258 | `	}` |
|       - |  259 | `	/* Attach the fiber's frame to the VM frame chain */` |
|     637 |  260 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     637 |  261 | `	pVm->pFrame = pCtx->pFrame;` |
|       - |  262 | `	/* Save and set the active context */` |
|     637 |  263 | `	pOldCtx = pVm->pActiveCtx;` |
|     637 |  264 | `	pVm->pActiveCtx = pCtx;` |
|     637 |  265 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|     637 |  266 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|     637 |  267 | `	pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|     637 |  268 | `	pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|       - |  269 | `	/* Native depth the body runs at (the VmByteCodeExec wrapper bumps +1): a` |
|       - |  270 | `	 * Fiber::suspend() at a deeper depth is inside a C->PHP callback and gets a` |
|       - |  271 | `	 * FiberError instead of parking across the native frame (stage 4). */` |
|     637 |  272 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1;` |
|       - |  273 | `	/* Execute from the beginning (no parked segment). An OP_SPREAD in the body may` |
|       - |  274 | `	 * realloc pCtx->pStack — pass &pCtx->pStack / &pCtx->nStackCap so the grown` |
|       - |  275 | `	 * buffer + capacity persist for the next resume and for ctx teardown. */` |
|     953 |  276 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     316 |  277 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|     316 |  278 | `		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|     637 |  279 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     321 |  280 | `}` |
|       - |  281 | `/*` |
|       - |  282 | ` * Resume a suspended fiber context.` |
|       - |  283 | ` */` |
|    1386 |  284 | `PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|       5 |  285 | `{` |
|       - |  286 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  287 | `	VmParkedSegment *pSeg;` |
|       - |  288 | `	sxi32 rc;` |
|    1391 |  289 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|     ! 0 |  290 | `		return SXERR_INVALID;` |
|       - |  291 | `	}` |
|       - |  292 | `	/* A resume is a native VmByteCodeExec re-entry, bounded by nMaxNativeDepth.` |
|       - |  293 | `	 * Reject HERE, before re-attaching the (possibly deep) parked segment to the` |
|       - |  294 | `	 * frame chain and re-adding its nRecords to nRecursionDepth — a wrapper-level` |
|       - |  295 | `	 * abort past those mutations would leave pVm->pFrame pointing into the parked` |
|       - |  296 | `	 * callee and the depth accounting un-reverted. The PHP call-depth cap is` |
|       - |  297 | `	 * OP_CALL-only (BYTECODE.md stage 5). */` |
|    1391 |  298 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  299 | `		return VmNativeNestingFatal(pVm);` |
|       - |  300 | `	}` |
|       - |  301 | `	/* Push the resume value onto the SUSPENDED activation's operand stack so it` |
|       - |  302 | `	 * appears as Fiber::suspend()'s return value. For a deep suspend (stage 4)` |
|       - |  303 | `	 * that stack is the innermost callee's — parked in the segment — not the` |
|       - |  304 | `	 * body's. nTos was saved one below the return-value slot. */` |
|       - |  305 | `	{` |
|       - |  306 | `		ph7_value *pResumeStack;` |
|    1391 |  307 | `		pSeg = (VmParkedSegment *)pCtx->pParkedSegment;` |
|    1391 |  308 | `		pResumeStack = pSeg ? pSeg->sState.pStack : pCtx->pStack;` |
|    1391 |  309 | `		if( pResumeValue ){` |
|     265 |  310 | `			PH7_MemObjStore(pResumeValue, &pResumeStack[pCtx->nTos + 1]);` |
|     135 |  311 | `		}else{` |
|    1131 |  312 | `			PH7_MemObjRelease(&pResumeStack[pCtx->nTos + 1]);` |
|       - |  313 | `		}` |
|    1391 |  314 | `		pCtx->nTos++;` |
|       - |  315 | `		/* Refresh the caller-depth base and re-publish this body's own exception` |
|       - |  316 | `		 * handlers on top of pVm->aException at that depth, so the resumed body's` |
|       - |  317 | `		 * try/catch and finally-drain bound line up (see VmByteCodeExec's base` |
|       - |  318 | `		 * override). Must run before VmByteCodeExec recaptures its local base. */` |
|    1391 |  319 | `		pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|    1391 |  320 | `		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|    1391 |  321 | `		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|    1391 |  322 | `		VmRestoreCtxState(pVm, pCtx);` |
|    1391 |  323 | `		if( pSeg ){` |
|       - |  324 | `			/* Reactivate the parked records' recursion accounting (mirror of the` |
|       - |  325 | `			 * deactivate at suspend); aSelf was just restored above. */` |
|     106 |  326 | `			pVm->nRecursionDepth += pSeg->nRecords;` |
|       - |  327 | `			/* Rebase the parked segment's absolute exception-floor indices: the` |
|       - |  328 | `			 * fiber may resume at a different caller depth than it suspended at,` |
|       - |  329 | `			 * so every activation's nExceptionBase shifts by the same delta the` |
|       - |  330 | `			 * republished handlers moved (newBase - the park-time base). */` |
|     106 |  331 | `			sxi32 iDelta = (sxi32)pCtx->nExceptionBase - (sxi32)pSeg->nOldExcBase;` |
|       - |  332 | `			/* nFinallyActBase floors rebase by their OWN delta — the exception and` |
|       - |  333 | `			 * finally-action stacks move independently between suspend and resume` |
|       - |  334 | `			 * (a fiber resumed from inside a generator's inline finally sees a` |
|       - |  335 | `			 * DEEPER aFinallyAction with an unchanged aException, and a stale` |
|       - |  336 | `			 * absolute floor would make the activation-end discard eat the` |
|       - |  337 | `			 * resumer's pending action). */` |
|     106 |  338 | `			sxi32 iFinDelta = (sxi32)pCtx->nFinallyBase - (sxi32)pSeg->nOldFinBase;` |
|     106 |  339 | `			if( iDelta != 0 \|\| iFinDelta != 0 ){` |
|       - |  340 | `				VmCallFrame *pRec;` |
|     ! 0 |  341 | `				pSeg->sState.nExceptionBase =` |
|     ! 0 |  342 | `					(sxu32)((sxi32)pSeg->sState.nExceptionBase + iDelta);` |
|     ! 0 |  343 | `				pSeg->sState.nFinallyActBase =` |
|     ! 0 |  344 | `					(sxu32)((sxi32)pSeg->sState.nFinallyActBase + iFinDelta);` |
|     ! 0 |  345 | `				for( pRec = pSeg->pCallTop; pRec; pRec = pRec->pPrev ){` |
|     ! 0 |  346 | `					pRec->sCaller.nExceptionBase =` |
|     ! 0 |  347 | `						(sxu32)((sxi32)pRec->sCaller.nExceptionBase + iDelta);` |
|     ! 0 |  348 | `					pRec->sCaller.nFinallyActBase =` |
|     ! 0 |  349 | `						(sxu32)((sxi32)pRec->sCaller.nFinallyActBase + iFinDelta);` |
|     ! 0 |  350 | `				}` |
|     ! 0 |  351 | `			}` |
|      51 |  352 | `		}` |
|       - |  353 | `		/* Re-attach the coroutine to the live VM frame chain: the body frame's` |
|       - |  354 | `		 * parent becomes the resumer's current frame. For a deep segment the` |
|       - |  355 | `		 * suspend-time top frame (the innermost callee / open-try wrapper) then` |
|       - |  356 | `		 * becomes current so the adopt at VmByteCodeExec entry resumes inside the` |
|       - |  357 | `		 * callee; body-level resumes make the body frame current. */` |
|    1391 |  358 | `		pCtx->pFrame->pParent = pVm->pFrame;` |
|    1391 |  359 | `		pVm->pFrame = pSeg ? pSeg->pTopFrame : pCtx->pFrame;` |
|       - |  360 | `	}` |
|       - |  361 | `	/* The segment (if any) is handed to the body invocation explicitly below; it` |
|       - |  362 | `	 * is no longer part of the suspended ctx state once resume owns it. */` |
|    1391 |  363 | `	pCtx->pParkedSegment = 0;` |
|       - |  364 | `	/* Save and set the active context */` |
|    1391 |  365 | `	pOldCtx = pVm->pActiveCtx;` |
|    1391 |  366 | `	pVm->pActiveCtx = pCtx;` |
|    1391 |  367 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|    1391 |  368 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1; /* see VmStartCtx */` |
|       - |  369 | `	/* Resume execution from saved PC. pSeg, when non-NULL, makes this body` |
|       - |  370 | `	 * re-enter inside the innermost parked callee (deep fiber resume, stage 4). */` |
|    2084 |  371 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     693 |  372 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|     693 |  373 | `		VmCtxEnforceRetFunc(pCtx), FALSE, pSeg, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|    1391 |  374 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     698 |  375 | `}` |
|       - |  376 | `/*` |
|       - |  377 | ` * Force-close a suspended generator context at destruction time, running its` |
|       - |  378 | `` * pending `finally` blocks (PHP runs finally when a generator is unset / goes out`` |
|       - |  379 | ` * of scope / is GC'd before it completes; PHL previously freed the open try` |
|       - |  380 | ` * handlers unexecuted in VmReleaseExecCtx). This is a "close", not a "resume":` |
|       - |  381 | `` * the finally handler of every still-open `try` the generator was suspended`` |
|       - |  382 | `` * inside runs innermost-first, but NO `catch` runs and no code past the finallys`` |
|       - |  383 | ` * executes.` |
|       - |  384 | ` *` |
|       - |  385 | ` * Generators compile finally INLINE (ROOT C): the finally bytecode lives in the` |
|       - |  386 | ` * body program at iFinallyPc, driven by the aException/aFinallyAction pc-redirect` |
|       - |  387 | `` * machinery — not the legacy detached `sFinally` mini-program VmDrainFinally runs.`` |
|       - |  388 | `` * So a close is expressed exactly like a `return` that crosses every enclosing`` |
|       - |  389 | ` * finally (OP_SET_FINALLY_RET): set pCtx->bClosing and resume; the body-entry` |
|       - |  390 | ` * redirect (see VmByteCodeExecBody) seeds a PH7_FA_RETURN action and jumps into` |
|       - |  391 | ` * the innermost open try's finally, and OP_END_FINALLY threads it out through the` |
|       - |  392 | ` * chain, then completes the body.` |
|       - |  393 | ` *` |
|       - |  394 | ` * Scope: a plain body-level suspend (pParkedSegment == 0). A deep fiber segment is` |
|       - |  395 | ` * left to plain release (generators never park one — yield is body-level only). A` |
|       - |  396 | `` * `yield` reached inside a finally during close is rejected by OP_YIELD via`` |
|       - |  397 | ` * pCtx->bClosing (PHP-exact "Cannot yield from finally in a force-closed` |
|       - |  398 | ` * generator"). Deferred edges remain.` |
|       - |  399 | ` *` |
|       - |  400 | ` * Returns whatever the body run returns: SXRET_OK on a clean close, PH7_ABORT if a` |
|       - |  401 | ` * finally aborts, or PH7_EXCEPTION if a finally threw past itself (surfaced to the` |
|       - |  402 | ` * destruct caller).` |
|       - |  403 | ` */` |
|     260 |  404 | `static sxi32 VmCloseCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  405 | `{` |
|       - |  406 | `	sxi32 rc;` |
|     265 |  407 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       - |  408 | `		/* CREATED never ran, so has no open try; COMPLETED/CLOSED already ran theirs. */` |
|     213 |  409 | `		return SXRET_OK;` |
|       - |  410 | `	}` |
|      56 |  411 | `	if( pCtx->pParkedSegment != 0 ){` |
|       - |  412 | `		/* Deep fiber segment (never a generator) — leave to plain release. */` |
|     ! 0 |  413 | `		return SXRET_OK;` |
|       - |  414 | `	}` |
|       - |  415 | ``	/* Suspended mid `yield from` over an inner generator: PHP closes innermost-first,`` |
|       - |  416 | `	 * so run the delegate's finallys before this body's. Both delegate-object states` |
|       - |  417 | `	 * carry the live iterator in sDelegate — state 3 (the delegate IS a Generator) and` |
|       - |  418 | ``	 * state 2 (an Iterator, which for `yield from $aggregate` is the generator returned`` |
|       - |  419 | `	 * by getIterator()); VmGeneratorExtractCtx returns 0 for a non-generator iterator,` |
|       - |  420 | `	 * so a plain Iterator/array delegate is skipped. Recurses for nested delegation;` |
|       - |  421 | `	 * the inner ends COMPLETED, so its own later __destruct close is a no-op. */` |
|      56 |  422 | `	if( pCtx->iDelegateState >= 2 && (pCtx->sDelegate.iFlags & MEMOBJ_OBJ) ){` |
|       5 |  423 | `		ph7_generator *pInner = VmGeneratorExtractCtx(pVm, &pCtx->sDelegate);` |
|       5 |  424 | `		if( pInner && pInner->pCtx ){` |
|       5 |  425 | `			sxi32 rcInner = VmCloseCtx(pVm, pInner->pCtx);` |
|       5 |  426 | `			if( rcInner == PH7_ABORT ){ return PH7_ABORT; }` |
|       2 |  427 | `		}` |
|       2 |  428 | `	}` |
|       - |  429 | `	/* Drive the pending finallys through a real body resume that the entry redirect` |
|       - |  430 | `	 * turns into a finally-chain unwind. VmResumeCtx handles state restore, frame` |
|       - |  431 | `	 * re-attach, active-ctx save/restore and the terminal-state bookkeeping. */` |
|      56 |  432 | `	pCtx->bClosing = 1;` |
|      56 |  433 | `	rc = VmResumeCtx(pVm, pCtx, 0, 0);` |
|      56 |  434 | `	pCtx->bClosing = 0;` |
|      56 |  435 | `	return rc;` |
|     135 |  436 | `}` |
|       - |  437 | `/*` |
|       - |  438 | ` * Free one DETACHED frame (not in the live pVm->pFrame chain): the frame of a` |
|       - |  439 | ` * suspended coroutine's body, or of a segment activation abandoned mid-call.` |
|       - |  440 | ` * Mirrors VmLeaveFrame's teardown minus the chain pop (there is no chain to pop` |
|       - |  441 | ` * from). Factored so the body-frame free and the stage-4 segment free share it.` |
|       - |  442 | ` */` |
|     694 |  443 | `static void VmFreeDetachedFrame(ph7_vm *pVm, VmFrame *pFrame)` |
|       5 |  444 | `{` |
|       - |  445 | `	VmSlot *aSlot;` |
|       - |  446 | `	sxu32 n;` |
|     699 |  447 | `	if( pFrame == 0 ){` |
|     ! 0 |  448 | `		return;` |
|       - |  449 | `	}` |
|       - |  450 | `	/* Remove local references FIRST, then free the locals nothing else holds — the` |
|       - |  451 | `	 * order and the holder test VmLeaveFrame explains. */` |
|     699 |  452 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|    1315 |  453 | `	for( n = 0; n < SySetUsed(&pFrame->sRef); ++n ){` |
|     621 |  454 | `		PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|     313 |  455 | `	}` |
|       - |  456 | `	/* Free local variables */` |
|     699 |  457 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|    1299 |  458 | `	for( n = 0; n < SySetUsed(&pFrame->sLocal); ++n ){` |
|     605 |  459 | `		if( PH7_VmSlotHolderCount(pVm, aSlot[n].nIdx) > 0 ){` |
|     ! 0 |  460 | `			continue;` |
|       - |  461 | `		}` |
|     605 |  462 | `		PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|     305 |  463 | `	}` |
|     699 |  464 | `	SyHashRelease(&pFrame->hVar);` |
|     699 |  465 | `	SySetRelease(&pFrame->sArg);` |
|     699 |  466 | `	SySetRelease(&pFrame->sLocal);` |
|     699 |  467 | `	SySetRelease(&pFrame->sRef);` |
|     699 |  468 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|       - |  469 | `	/* Drop a resume target pointing at this detached frame before we free it (ROOT B). */` |
|     699 |  470 | `	VmDropResumeTarget(pVm,pFrame);` |
|     699 |  471 | `	SyMemBackendPoolFree(&pVm->sAllocator, pFrame);` |
|     352 |  472 | `}` |
|       - |  473 | `/*` |
|       - |  474 | ` * Free a parked deep-suspend segment (stage 4) whose fiber was abandoned while` |
|       - |  475 | ` * suspended. Every record holds a callee's operand stack and VmFrame (the` |
|       - |  476 | ` * topmost record's callee is the innermost activation, running on sState); walk` |
|       - |  477 | ` * the chain releasing each callee stack's live entries then the stack and frame.` |
|       - |  478 | ` * The body frame/stack are NOT here — they are freed by the caller` |
|       - |  479 | ` * (VmReleaseExecCtx) as pCtx->pFrame / pCtx->pStack.` |
|       - |  480 | ` */` |
|     200 |  481 | `static void VmFreeParkedSegment(ph7_vm *pVm, ph7_exec_ctx *pCtx, VmParkedSegment *pSeg)` |
|       1 |  482 | `{` |
|       - |  483 | `	/* Live top-of-stack of the activation running on the current record's callee` |
|       - |  484 | `	 * stack: the innermost (sState) for the topmost record, then each caller. */` |
|     201 |  485 | `	ph7_value *pTosAbove = pSeg->sState.pTos;` |
|     201 |  486 | `	VmCallFrame *pRec = pSeg->pCallTop, *pNext;` |
|     401 |  487 | `	while( pRec ){` |
|     201 |  488 | `		ph7_value *pStk = pRec->sCall.pFrameStack;` |
|     201 |  489 | `		if( pStk ){` |
|     201 |  490 | `			ph7_value *pTos = pTosAbove;` |
|     401 |  491 | `			while( pTos >= pStk ){` |
|     201 |  492 | `				PH7_MemObjRelease(pTos);` |
|     201 |  493 | `				pTos--;` |
|       1 |  494 | `			}` |
|     201 |  495 | `			SyMemBackendFree(&pVm->sAllocator, pStk);` |
|     100 |  496 | `		}` |
|     201 |  497 | `		VmFreeDetachedFrame(pVm, pRec->sCall.pFrame);` |
|       - |  498 | `		/* The caller recorded here runs on the NEXT-lower callee stack; grab its` |
|       - |  499 | `		 * live tos before freeing this node. */` |
|     201 |  500 | `		pTosAbove = pRec->sCaller.pTos;` |
|     201 |  501 | `		pNext = pRec->pPrev;` |
|     201 |  502 | `		SyMemBackendPoolFree(&pVm->sAllocator, pRec);` |
|     201 |  503 | `		pRec = pNext;` |
|       1 |  504 | `	}` |
|       - |  505 | `	/* pTosAbove now points at the BODY activation's live top (the bottom record's` |
|       - |  506 | `	 * sCaller, which runs on pCtx->pStack). pCtx->nTos still holds the INNERMOST` |
|       - |  507 | `	 * index (VmSuspendCtx saved it), which would over-index the body stack in` |
|       - |  508 | `	 * VmReleaseExecCtx's release loop — correct it to the body's real top. */` |
|     201 |  509 | `	pCtx->nTos = (sxi32)(pTosAbove - pCtx->pStack);` |
|     201 |  510 | `	SyMemBackendFree(&pVm->sAllocator, pSeg);` |
|     201 |  511 | `}` |
|       - |  512 | `/*` |
|       - |  513 | ` * Release an execution context and all its resources.` |
|       - |  514 | ` */` |
|     494 |  515 | `PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  516 | `{` |
|     499 |  517 | `	if( pCtx == 0 ){` |
|     ! 0 |  518 | `		return;` |
|       - |  519 | `	}` |
|     499 |  520 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       - |  521 | `		/* Cannot destroy a fiber that is currently executing */` |
|     ! 0 |  522 | `		return;` |
|       - |  523 | `	}` |
|     499 |  524 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       - |  525 | `	/* Release values */` |
|     499 |  526 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|     499 |  527 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|     499 |  528 | `	PH7_MemObjRelease(&pCtx->sDelegate);` |
|       - |  529 | `	/* Stage 2b: the parked entries are per-activation clones now — free them` |
|       - |  530 | `	 * (an abandoned suspended coroutine is their last holder). */` |
|     499 |  531 | `	VmExcReleaseAll(pVm,&pCtx->aSavedException);` |
|     499 |  532 | `	SySetRelease(&pCtx->aSavedException);` |
|       - |  533 | `	/* ROOT C: a generator abandoned while suspended inside a finally may carry parked` |
|       - |  534 | `	 * finally actions holding owned values/refs (a RETURN's sRet, a RETHROW's pExc).` |
|       - |  535 | `	 * Release them so the abandon path leaks nothing. */` |
|       - |  536 | `	{` |
|     499 |  537 | `		sxu32 n = SySetUsed(&pCtx->aSavedFinally);` |
|     499 |  538 | `		if( n > 0 ){` |
|     ! 0 |  539 | `			VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pCtx->aSavedFinally);` |
|       - |  540 | `			sxu32 i;` |
|     ! 0 |  541 | `			for( i = 0; i < n; i++ ){` |
|     ! 0 |  542 | `				if( aA[i].eKind == PH7_FA_RETURN ){` |
|     ! 0 |  543 | `					PH7_MemObjRelease(&aA[i].sRet);` |
|     ! 0 |  544 | `				}else if( aA[i].eKind == PH7_FA_RETHROW && aA[i].pExc ){` |
|     ! 0 |  545 | `					PH7_ClassInstanceUnref(aA[i].pExc);` |
|     ! 0 |  546 | `				}` |
|     ! 0 |  547 | `			}` |
|     ! 0 |  548 | `		}` |
|     499 |  549 | `		SySetRelease(&pCtx->aSavedFinally);` |
|       - |  550 | `	}` |
|       - |  551 | `	/* Stage 4: parked aSelf entries are borrowed class pointers — just free the set. */` |
|     499 |  552 | `	SySetRelease(&pCtx->aSavedSelf);` |
|       - |  553 | `	/* Free a parked deep-suspend segment (stage 4): the fiber was abandoned while` |
|       - |  554 | `	 * suspended inside a nested call, so its record chain / frames / operand` |
|       - |  555 | `	 * stacks are still alive and only this holder references them. Must run` |
|       - |  556 | `	 * before the body frame/stack below (they are the segment's floor). */` |
|     499 |  557 | `	if( pCtx->pParkedSegment ){` |
|     201 |  558 | `		VmFreeParkedSegment(pVm, pCtx, (VmParkedSegment *)pCtx->pParkedSegment);` |
|     201 |  559 | `		pCtx->pParkedSegment = 0;` |
|     100 |  560 | `	}` |
|       - |  561 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|     499 |  562 | `	if( pCtx->pFrame ){` |
|     499 |  563 | `		VmFreeDetachedFrame(pVm, pCtx->pFrame);` |
|     499 |  564 | `		pCtx->pFrame = 0;` |
|     247 |  565 | `	}` |
|       - |  566 | `	/* A by-reference parameter aliased a CALLER's slot; the frame teardown above just` |
|       - |  567 | `	 * dropped this body's name for it. Release it if nothing is left holding it — the` |
|       - |  568 | `	 * caller may already be gone (it skipped the slot precisely because this frame` |
|       - |  569 | `	 * held it), in which case this is its last holder. Runs after the frame so the` |
|       - |  570 | `	 * body's own row is out of the count. */` |
|       - |  571 | `	{` |
|       - |  572 | `		sxu32 n;` |
|     499 |  573 | `		sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pCtx->aByRefArg);` |
|     515 |  574 | `		for( n = 0; n < SySetUsed(&pCtx->aByRefArg); n++ ){` |
|      18 |  575 | `			PH7_VmReleaseUnheldSlot(pVm, aIdx[n]);` |
|      10 |  576 | `		}` |
|     499 |  577 | `		SySetRelease(&pCtx->aByRefArg);` |
|       - |  578 | `	}` |
|       - |  579 | `	/* Release individual operand stack entries (decrement refcounts,` |
|       - |  580 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|       - |  581 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|     499 |  582 | `	if( pCtx->pStack ){` |
|     499 |  583 | `		if( pCtx->nTos >= 0 ){` |
|     439 |  584 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|     873 |  585 | `			while( pTos >= pCtx->pStack ){` |
|     439 |  586 | `				PH7_MemObjRelease(pTos);` |
|     439 |  587 | `				pTos--;` |
|       5 |  588 | `			}` |
|     217 |  589 | `		}` |
|     499 |  590 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|     499 |  591 | `		pCtx->pStack = 0;` |
|     247 |  592 | `	}` |
|       - |  593 | `	/* Free the context itself */` |
|     499 |  594 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     252 |  595 | `}` |
|       - |  596 | `/*` |
|       - |  597 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|       - |  598 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|       - |  599 | ` */` |
|     788 |  600 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|       5 |  601 | `{` |
|       - |  602 | `	ph7_class_instance *pThis;` |
|       - |  603 | `	SyString sAttr;` |
|       - |  604 | `	ph7_value *pAttr;` |
|     793 |  605 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 |  606 | `		return 0;` |
|       - |  607 | `	}` |
|     793 |  608 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|     793 |  609 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|     ! 0 |  610 | `		return 0;` |
|       - |  611 | `	}` |
|     793 |  612 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     793 |  613 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     793 |  614 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     279 |  615 | `		return 0;` |
|       - |  616 | `	}` |
|     519 |  617 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|     399 |  618 | `}` |
|       - |  619 | `/* The three VM_INSTANCE_FCC_* Closure flags live in ph7int.h: vm_exec.c's OP_LOAD_FCC` |
|       - |  620 | ` * stamps VM_INSTANCE_FCC_METHOD and this file reads all three. */` |
|       - |  621 | `/*` |
|       - |  622 | `` * A PHP closure (and a first-class callable `f(...)`) is a real object: an instance of`` |
|       - |  623 | `` * the built-in final `Closure` class carrying its underlying callable in a private`` |
|       - |  624 | `` * `$__fn` attribute (the callable NAME: a registered `[closure_N]`/`[lambda_N]` or a`` |
|       - |  625 | ` * user/host function name) — plus, for a method/static first-class callable, a bound` |
|       - |  626 | `` * `$__this` object and/or a `$__scope` class-name. Storing the callable as plain attributes`` |
|       - |  627 | `` * (rather than a native resource pointer) means the object owns no `ph7_vm_func` — the`` |
|       - |  628 | `` * per-closure function stays owned by `hFunction`/VM-release exactly as before, so there is`` |
|       - |  629 | ` * no extra free path.` |
|       - |  630 | ` *` |
|       - |  631 | ` * Returns non-zero iff pVal is a Closure instance.` |
|       - |  632 | ` */` |
| 5585801 |  633 | `PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm, ph7_value *pVal)` |
|       5 |  634 | `{` |
|       - |  635 | `	ph7_class_instance *pThis;` |
|       - |  636 | `	/* Flag test first: a non-object call target (the hot common case) bails before any` |
|       - |  637 | `	 * pVm dereference; pClosureClass==0 is a one-time pre-init concern, so it goes last. */` |
| 5585806 |  638 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
| 5363068 |  639 | `		return 0;` |
|       - |  640 | `	}` |
|  222743 |  641 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       - |  642 | `	/* Closure is final, so an exact class match is correct (no subclasses possible). */` |
|  222743 |  643 | `	return pThis->pClass == pVm->pClosureClass;` |
| 2794933 |  644 | `}` |
|       - |  645 | `/*` |
|       - |  646 | ` * Unwrap a Closure value into the simple callable the existing dispatch machinery` |
|       - |  647 | ` * already understands, written into pOut (which the caller must have initialised):` |
|       - |  648 | `` *   - bound `$this` set (method first-class callable) -> [ $__this, $__fn ] array callable`` |
|       - |  649 | `` *   - `$__scope` set (static first-class callable)     -> [ $__scope, $__fn ] array callable`` |
|       - |  650 | `` *   - neither (plain function / real closure)          -> the `$__fn` name string`` |
|       - |  651 | ` * The 2-element array form rides the existing MEMOBJ_HASHMAP dispatch, which binds $this` |
|       - |  652 | ` * for an object first element and resolves the class for a class-name-string first element.` |
|       - |  653 | ` * Returns SXRET_OK if pVal was a Closure (pOut filled), SXERR_NOTFOUND otherwise.` |
|       - |  654 | ` */` |
|    8822 |  655 | `PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm, ph7_value *pVal, ph7_value *pOut)` |
|       5 |  656 | `{` |
|       - |  657 | `	ph7_class_instance *pThis;` |
|       - |  658 | `	ph7_value *pFn;` |
|       - |  659 | `	SyString sAttr;` |
|    8827 |  660 | `	if( !VmValueIsClosure(pVm, pVal) ){` |
|     ! 0 |  661 | `		return SXERR_NOTFOUND;` |
|       - |  662 | `	}` |
|    8827 |  663 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    8827 |  664 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    8827 |  665 | `	pFn = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    8827 |  666 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 |  667 | `		return SXERR_NOTFOUND; /* malformed/uninitialised closure */` |
|       - |  668 | `	}` |
|       - |  669 | `	/* Only a bound/static first-class callable (rare) carries $__this/$__scope; the` |
|       - |  670 | `	 * VM_INSTANCE_FCC_BOUND flag (set by VmCreateClosure) keeps the hot plain-closure path` |
|       - |  671 | `	 * to the single $__fn lookup above instead of two extra attribute lookups per dispatch. */` |
|    8827 |  672 | `	if( pThis->iFlags & VM_INSTANCE_FCC_BOUND ){` |
|       - |  673 | `		ph7_value *pBound, *pScope;` |
|       - |  674 | `		int bBoundObj, bScope;` |
|     242 |  675 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     242 |  676 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     242 |  677 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     242 |  678 | `		pScope = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     242 |  679 | `		bBoundObj = pBound && (pBound->iFlags & MEMOBJ_OBJ);` |
|     242 |  680 | `		bScope = pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0;` |
|     242 |  681 | `		if( bBoundObj \|\| bScope ){` |
|       - |  682 | `			/* Method/static first-class callable -> [ target, "method" ] array callable. */` |
|     242 |  683 | `			if( pThis->iFlags & VM_INSTANCE_FCC_INVOKE_OBJ ){` |
|       - |  684 | `				/* Closure::fromCallable($obj): the engine named __invoke, so this` |
|       - |  685 | `				 * dispatch is the engine's own and a non-public one still runs. */` |
|      10 |  686 | `				pVm->bMagicDispatch = 1;` |
|       4 |  687 | `			}` |
|       - |  688 | `			ph7_hashmap *pMap;` |
|       - |  689 | `			ph7_value sTarget, sMeth;` |
|       - |  690 | `			sxi32 rc;` |
|     242 |  691 | `			if( bBoundObj ){` |
|     180 |  692 | `				ph7_class_instance *pBoundObj = (ph7_class_instance *)pBound->x.pOther;` |
|       - |  693 | ``				/* A bound PLAIN closure (`function(){…}->bindTo($o)`): $__fn names a function, not a`` |
|       - |  694 | `				 * method of the bound object's class, so a [obj,method] array callable would fail` |
|       - |  695 | `				 * method resolution. Stash the bound object (own a ref) so the OP_CALL user-function` |
|       - |  696 | `				 * frame setup injects it as $this, and return the plain $__fn string for a normal` |
|       - |  697 | `				 * function dispatch. */` |
|     176 |  698 | `				if( (pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0` |
|     127 |  699 | `				 && PH7_ClassExtractMethod(pBoundObj->pClass,` |
|     105 |  700 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){` |
|       - |  701 | `					/* Only a USER function (anonymous closure / named fn in hFunction) reads $this and` |
|       - |  702 | `					 * reaches the OP_CALL user-function frame-setup that consumes pClosureThis. A HOST` |
|       - |  703 | `					 * function (e.g. Closure::fromCallable('strlen')->bindTo($o)) ignores $this and` |
|       - |  704 | `					 * dispatches via the host path which never consumes the transient — setting it` |
|       - |  705 | `					 * there would leak the ref and inject a stale $this into the next call. */` |
|      93 |  706 | `					if( SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|      65 |  707 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|      65 |  708 | `						pBoundObj->iRef++;` |
|      65 |  709 | `						pVm->pClosureThis = pBoundObj;` |
|       - |  710 | `						/* Carry the bound scope (if set) so private/protected member access inside` |
|       - |  711 | `						 * the closure body resolves against it — bindTo($o, Scope::class) / call($o). */` |
|      65 |  712 | `						if( bScope ){` |
|      72 |  713 | `							pVm->pClosureScope = PH7_VmExtractClass(pVm,` |
|      46 |  714 | `								(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      23 |  715 | `						}` |
|      31 |  716 | `					}` |
|      65 |  717 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      65 |  718 | `					return SXRET_OK;` |
|       - |  719 | `				}` |
|      60 |  720 | `			}else{` |
|       - |  721 | ``				/* Scope-only rebind of a PLAIN closure (`bindTo(null, Scope::class)`):`` |
|       - |  722 | `				 * $__fn names a function, not a static method of the scope class, so` |
|       - |  723 | `				 * the [scope, method] array callable below would fail method` |
|       - |  724 | `				 * resolution. Dispatch the plain $__fn string and carry the scope for` |
|       - |  725 | `				 * private/protected visibility (pClosureThis stays unset — no $this).` |
|       - |  726 | `				 * A static-method FCC ($__fn really is a method of the scope class)` |
|       - |  727 | `				 * falls through to the array-callable path. */` |
|      95 |  728 | `				ph7_class *pScopeClass = PH7_VmExtractClass(pVm,` |
|      62 |  729 | `					(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      62 |  730 | `				if( pScopeClass == 0` |
|      64 |  731 | `				 \|\| ((pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0` |
|      34 |  732 | `				  && PH7_ClassExtractMethod(pScopeClass,` |
|       9 |  733 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0) ){` |
|       6 |  734 | `					if( pScopeClass` |
|       7 |  735 | `					 && SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|       6 |  736 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|       7 |  737 | `						pVm->pClosureScope = pScopeClass;` |
|       3 |  738 | `					}` |
|       7 |  739 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|       7 |  740 | `					return SXRET_OK;` |
|       - |  741 | `				}` |
|       - |  742 | `			}` |
|     173 |  743 | `			pMap = PH7_NewHashmap(&(*pVm), 0, 0);` |
|     173 |  744 | `			if( pMap == 0 ){` |
|     ! 0 |  745 | `				return SXERR_NOTFOUND;` |
|       - |  746 | `			}` |
|     173 |  747 | `			PH7_MemObjInit(pVm, &sTarget);` |
|     173 |  748 | `			PH7_MemObjInit(pVm, &sMeth);` |
|     173 |  749 | `			if( bBoundObj ){` |
|     117 |  750 | `				PH7_MemObjStore(pBound, &sTarget); /* bound object (iRef++) -> binds $this */` |
|      60 |  751 | `			}else{` |
|      58 |  752 | `				PH7_MemObjStringAppend(&sTarget, SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob));` |
|       - |  753 | `			}` |
|     173 |  754 | `			PH7_MemObjStringAppend(&sMeth, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|     173 |  755 | `			rc = PH7_HashmapInsert(pMap, 0, &sTarget);` |
|     173 |  756 | `			if( rc == SXRET_OK ){` |
|     173 |  757 | `				rc = PH7_HashmapInsert(pMap, 0, &sMeth);` |
|      85 |  758 | `			}` |
|     173 |  759 | `			PH7_MemObjRelease(&sTarget);` |
|     173 |  760 | `			PH7_MemObjRelease(&sMeth);` |
|     173 |  761 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  762 | `				PH7_HashmapRelease(pMap, TRUE); /* free the partial map, no leak */` |
|     ! 0 |  763 | `				return SXERR_NOTFOUND;` |
|       - |  764 | `			}` |
|     173 |  765 | `			if( pThis->iFlags & VM_INSTANCE_FCC_SCREENED ){` |
|       - |  766 | `				/* php resolves a method Closure's callee ONCE, where the closure is built, and` |
|       - |  767 | ``				 * keeps the resolved function: an escaped `$this->priv(...)` runs anywhere. PHL`` |
|       - |  768 | `				 * keeps only a NAME, so every dispatch site would re-decide visibility against the` |
|       - |  769 | `				 * CALLER and refuse the closure php runs. The creation sites screen (OP_LOAD_FCC's` |
|       - |  770 | `				 * VmFccMemberError, Closure::fromCallable's PH7_VmIsCallable gate, and reflection,` |
|       - |  771 | `				 * which php lets past protection on purpose) and stamp the mark only when a real` |
|       - |  772 | `				 * method answered — a name the class reaches through __call carries no mark and` |
|       - |  773 | `				 * still routes to the catch-all. Armed only once the pair below really exists, so` |
|       - |  774 | `				 * a failed build cannot leave it standing; the consumers clear it like` |
|       - |  775 | `				 * pClosureThis/pClosureScope. */` |
|     144 |  776 | `				pVm->bClosureScreened = 1;` |
|      71 |  777 | `			}` |
|     173 |  778 | `			pOut->x.pOther = pMap;` |
|     173 |  779 | `			MemObjSetType(pOut, MEMOBJ_HASHMAP);` |
|     173 |  780 | `			return SXRET_OK;` |
|       - |  781 | `		}` |
|     ! 0 |  782 | `	}` |
|    8589 |  783 | `	PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    8589 |  784 | `	return SXRET_OK;` |
|    4416 |  785 | `}` |
|       - |  786 | `/*` |
|       - |  787 | ` * php's SCOPE for a Closure — the class whose private members its body may reach, and the` |
|       - |  788 | ` * class Reflection reports. For a plain closure that is the class it was bound to; for a` |
|       - |  789 | ` * METHOD closure it is the class that DECLARED the method, which is not the class the` |
|       - |  790 | `` * callable NAMED: `(new Kid)->mk()` returning `$this->basePriv(...)` is scoped to Base in`` |
|       - |  791 | `` * php, while `$__scope` records Kid — the class the call goes THROUGH, which is the CALLED`` |
|       - |  792 | `` * scope (`static::`) and a different question. A trait method is composed into the using`` |
|       - |  793 | ` * class, so PH7_VmMethodScopeName answers for it exactly as it does at every refusal site.` |
|       - |  794 | ` * Returns 0 when the closure carries no scope at all.` |
|       - |  795 | ` */` |
|      78 |  796 | `PH7_PRIVATE ph7_class * PH7_VmClosureScopeClass(ph7_vm *pVm, ph7_class_instance *pClosure)` |
|       2 |  797 | `{` |
|       - |  798 | `	SyString sAttr;` |
|       - |  799 | `	ph7_value *pScope, *pFn;` |
|       - |  800 | `	ph7_class *pClass;` |
|      80 |  801 | `	if( pClosure == 0 ){` |
|     ! 0 |  802 | `		return 0;` |
|       - |  803 | `	}` |
|      80 |  804 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      80 |  805 | `	pScope = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      80 |  806 | `	if( pScope == 0 \|\| (pScope->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pScope->sBlob) == 0 ){` |
|     ! 0 |  807 | `		return 0;` |
|       - |  808 | `	}` |
|     119 |  809 | `	pClass = PH7_VmExtractClass(pVm, (const char *)SyBlobData(&pScope->sBlob),` |
|      39 |  810 | `		SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      80 |  811 | `	if( pClass == 0 \|\| (pClosure->iFlags & VM_INSTANCE_FCC_METHOD) == 0 ){` |
|       5 |  812 | `		return pClass;` |
|       - |  813 | `	}` |
|      76 |  814 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|      76 |  815 | `	pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      76 |  816 | `	if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){` |
|     113 |  817 | `		ph7_class_method *pMeth = PH7_ClassExtractMethod(pClass,` |
|      74 |  818 | `			(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      76 |  819 | `		if( pMeth ){` |
|      76 |  820 | `			return PH7_VmMethodScopeName(pVm, pClass, pMeth);` |
|       - |  821 | `		}` |
|     ! 0 |  822 | `	}` |
|     ! 0 |  823 | `	return pClass;` |
|      41 |  824 | `}` |
|       - |  825 | `/*` |
|       - |  826 | `` * Resolve the scope class for a STATIC first-class callable `T::m(...)`, where T is a`` |
|       - |  827 | ` * class-name STRING value: handles the self/static/parent keywords against the live class` |
|       - |  828 | ` * context (so the actual class is bound at FCC-creation time, like PHP), and falls back to` |
|       - |  829 | ` * an explicit class-name lookup. Mirrors the static OP_MEMBER resolution. Returns 0 if the` |
|       - |  830 | ` * class cannot be resolved.` |
|       - |  831 | ` */` |
|     138 |  832 | `PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget)` |
|       3 |  833 | `{` |
|     279 |  834 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     138 |  835 | `		(sxu32)SyBlobLength(&pTarget->sBlob));` |
|       3 |  836 | `}` |
|       - |  837 | `/*` |
|       - |  838 | ` * The same resolution over a raw (name, length) pair, for the callable machinery: php` |
|       - |  839 | `` * resolves `self`/`parent`/`static` in a CALLBACK (is_callable, call_user_func, array_map …)`` |
|       - |  840 | `` * against the live class context, and refuses them in the direct `$cb()` dispatch — so this`` |
|       - |  841 | ` * is deliberately NOT wired into the OP_CALL resolve check, which must keep answering` |
|       - |  842 | `` * `Class "self" not found`.`` |
|       - |  843 | ` */` |
|  100970 |  844 | `PH7_PRIVATE ph7_class * PH7_VmResolveScopeName(ph7_vm *pVm, const char *zCls, sxu32 nCls)` |
|       5 |  845 | `{` |
|       - |  846 | `	ph7_class *pClass;` |
|  100975 |  847 | `	if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|     107 |  848 | `		pClass = PH7_VmPeekSelfClass(&(*pVm)); /* self:: in a trait -> the USING class */` |
|  100924 |  849 | `	}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|      74 |  850 | `		pClass = PH7_VmPeekTopClass(&(*pVm));     /* late static binding */` |
|  100838 |  851 | `	}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|      46 |  852 | `		pClass = PH7_VmResolveParentClass(&(*pVm));` |
|      25 |  853 | `	}else{` |
|  100761 |  854 | `		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - |  855 | `	}` |
|  100975 |  856 | `	return pClass;` |
|       5 |  857 | `}` |
|       - |  858 | `/*` |
|       - |  859 | ` * Create a Closure object wrapping a callable name (+ optional bound $this object and/or` |
|       - |  860 | `` * scope class-name, for the method/static first-class callables `$o->m(...)`/`C::m(...)`).`` |
|       - |  861 | ` * Mirrors the Generator/Fiber "object carries its state in private attributes" pattern.` |
|       - |  862 | ` * Returns the fresh instance (iRef == 0; caller takes the reference), or 0 on OOM.` |
|       - |  863 | ` */` |
|    6998 |  864 | `PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName,` |
|       - |  865 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       5 |  866 | `{` |
|       - |  867 | `	ph7_class_instance *pObj;` |
|       - |  868 | `	ph7_value *pAttr;` |
|       - |  869 | `	SyString sAttr;` |
|    7003 |  870 | `	if( pVm->pClosureClass == 0 ){` |
|     ! 0 |  871 | `		return 0;` |
|       - |  872 | `	}` |
|    7003 |  873 | `	pObj = PH7_NewClassInstance(&(*pVm), pVm->pClosureClass);` |
|    7003 |  874 | `	if( pObj == 0 ){` |
|     ! 0 |  875 | `		return 0;` |
|       - |  876 | `	}` |
|    7003 |  877 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    7003 |  878 | `	pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|    7003 |  879 | `	if( pAttr ){` |
|    7003 |  880 | `		PH7_MemObjStringAppend(pAttr, pName->zString, pName->nByte);` |
|    3499 |  881 | `	}` |
|    7003 |  882 | `	if( pBoundThis ){` |
|     154 |  883 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     154 |  884 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|     154 |  885 | `		if( pAttr ){` |
|     154 |  886 | `			pAttr->x.pOther = pBoundThis;` |
|     154 |  887 | `			MemObjSetType(pAttr, MEMOBJ_OBJ);` |
|     154 |  888 | `			pBoundThis->iRef++; /* keep the bound object alive for the closure's lifetime */` |
|      75 |  889 | `		}` |
|      75 |  890 | `	}` |
|    7003 |  891 | `	if( pScope && pScope->nByte ){` |
|     222 |  892 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     222 |  893 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|     222 |  894 | `		if( pAttr ){` |
|     222 |  895 | `			PH7_MemObjStringAppend(pAttr, pScope->zString, pScope->nByte);` |
|     109 |  896 | `		}` |
|     109 |  897 | `	}` |
|    7003 |  898 | `	if( pBoundThis \|\| (pScope && pScope->nByte) ){` |
|       - |  899 | `		/* Mark bound/static FCC closures so VmClosureUnwrap can skip the $__this/$__scope` |
|       - |  900 | `		 * lookups on the hot plain-closure dispatch path. */` |
|     222 |  901 | `		pObj->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|     109 |  902 | `	}` |
|    7003 |  903 | `	return pObj;` |
|    3504 |  904 | `}` |
|       - |  905 | `/*` |
|       - |  906 | ` * Exported wrapper around VmCreateClosure for builtin libraries outside this` |
|       - |  907 | ` * file (ReflectionFunction::getClosure / ReflectionMethod::getClosure).` |
|       - |  908 | ` */` |
|      16 |  909 | `PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm, const SyString *pName,` |
|       - |  910 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       1 |  911 | `{` |
|      17 |  912 | `	return VmCreateClosure(&(*pVm), pName, pBoundThis, pScope);` |
|       1 |  913 | `}` |
|       - |  914 | `/*` |
|       - |  915 | ` * Exported wrapper around the typed/readonly property store enforcement for` |
|       - |  916 | ` * ReflectionProperty::setValue (vm_builtin_reflection.c). Same contract:` |
|       - |  917 | ` * SXRET_OK (value possibly coerced in place), PH7_EXCEPTION, or PH7_ABORT.` |
|       - |  918 | ` */` |
|       8 |  919 | `PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|       1 |  920 | `{` |
|       9 |  921 | `	return VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pValue,0);` |
|       1 |  922 | `}` |
|       - |  923 | `/*` |
|       - |  924 | ` * Exported reference-table probe for ReflectionReference::fromArrayElement` |
|       - |  925 | ` * (vm_builtin_reflection.c). Returns the number of links (frame variables +` |
|       - |  926 | ` * array entries) attached to the slot's reference record, 0 when the slot` |
|       - |  927 | ` * has none — an array element is a PHP reference when this is >= 2.` |
|       - |  928 | ` */` |
|      18 |  929 | `PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx)` |
|       1 |  930 | `{` |
|      19 |  931 | `	VmRefObj *pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|      19 |  932 | `	if( pRef == 0 ){` |
|     ! 0 |  933 | `		return 0;` |
|       - |  934 | `	}` |
|      19 |  935 | `	return (int)(SySetUsed(&pRef->aReference) + SySetUsed(&pRef->aArrEntries));` |
|      10 |  936 | `}` |
|       - |  937 | `/*` |
|       - |  938 | `` * First-class callable over an arbitrary callable VALUE: `($expr)(...)`.`` |
|       - |  939 | ` * Normalize a validated callable value into a fresh Closure, reusing VmCreateClosure (the same` |
|       - |  940 | ` * object the method/static first-class-callable paths mint, so dispatch round-trips identically` |
|       - |  941 | ` * via VmClosureUnwrap). Handles the three remaining callable shapes a value can hold:` |
|       - |  942 | ` *   - a function-NAME string          -> plain closure ($__fn = name)` |
|       - |  943 | ` *   - a [target, method] array callable -> bound (object target -> $this) / static (class-name` |
|       - |  944 | ` *     target -> scope) closure, mirroring the [obj,m]/[class,m] decode in PH7_VmIsCallable` |
|       - |  945 | ` *   - an __invoke object               -> closure bound to the object's __invoke` |
|       - |  946 | ` * An existing Closure returns 0 here (it is already a Closure — the caller keeps it as-is), so this` |
|       - |  947 | ` * stays idempotent even for a direct caller. Returns the fresh instance (iRef == 0; caller takes the` |
|       - |  948 | ` * reference) or 0 if the value is an existing Closure / not a normalizable callable / on OOM — in` |
|       - |  949 | ` * which case the caller leaves the value untouched (graceful degradation). This is the generic` |
|       - |  950 | ` * "callable value -> Closure" primitive: the body is FCC-agnostic and self-contained, so the future` |
|       - |  951 | ` * Closure::bind/fromCallable work (Increment 2) can call it directly.` |
|       - |  952 | ` */` |
|     192 |  953 | `PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue)` |
|       4 |  954 | `{` |
|       - |  955 | `	/* A Closure is already callable; never double-wrap it (this also stops the __invoke branch` |
|       - |  956 | `	 * below from binding a closure to its OWN __invoke). The OP_LOAD_FCC caller intercepts a` |
|       - |  957 | `	 * Closure first too, but guarding here keeps the primitive safe for a direct Increment-2 caller. */` |
|     196 |  958 | `	if( VmValueIsClosure(pVm, pValue) ){` |
|     ! 0 |  959 | `		return 0;` |
|       - |  960 | `	}` |
|     196 |  961 | `	if( !PH7_VmIsCallable(pVm, pValue, TRUE) ){` |
|      47 |  962 | `		return 0;` |
|       - |  963 | `	}` |
|     150 |  964 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  965 | `		SyString sName;` |
|      94 |  966 | `		SyStringInitFromBuf(&sName, SyBlobData(&pValue->sBlob), SyBlobLength(&pValue->sBlob));` |
|      94 |  967 | `		return VmCreateClosure(pVm, &sName, 0, 0);` |
|       - |  968 | `	}` |
|      58 |  969 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  970 | `		/* [target, method] — the same index-0/1 decode PH7_VmIsCallable uses to validate it` |
|       - |  971 | `		 * (php reads the INTEGER indices, not insertion order). */` |
|      47 |  972 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - |  973 | `		ph7_value *pTarget, *pMeth;` |
|       - |  974 | `		SyString sName;` |
|      47 |  975 | `		if( !PH7_VmArrayCallableParts(pVm, pMap, &pTarget, &pMeth) ){` |
|     ! 0 |  976 | `			return 0;` |
|       - |  977 | `		}` |
|      47 |  978 | `		if( (pMeth->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pMeth->sBlob) == 0 ){` |
|     ! 0 |  979 | `			return 0;` |
|       - |  980 | `		}` |
|      47 |  981 | `		SyStringInitFromBuf(&sName, SyBlobData(&pMeth->sBlob), SyBlobLength(&pMeth->sBlob));` |
|      47 |  982 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      33 |  983 | `			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      49 |  984 | `			ph7_class_instance *pFccObj = VmCreateClosure(pVm, &sName, pBoundThis,` |
|      32 |  985 | `				&pBoundThis->pClass->sName);` |
|      33 |  986 | `			if( pFccObj ){` |
|      33 |  987 | `				pFccObj->iFlags \|= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */` |
|      49 |  988 | `				if( PH7_VmFccMethodIsDirect(pVm,pBoundThis->pClass,` |
|      16 |  989 | `						SyStringData(&sName),SyStringLength(&sName)) ){` |
|       - |  990 | `					/* The PH7_VmIsCallable gate above is php's creation-time screen; a pair it` |
|       - |  991 | `					 * admitted because the class routes the name through __call is NOT settled. */` |
|      29 |  992 | `					pFccObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|      14 |  993 | `				}` |
|      16 |  994 | `			}` |
|      33 |  995 | `			return pFccObj;` |
|     ! 0 |  996 | `		}else{` |
|       - |  997 | `			/* [class-name, method] static callable -> bind the resolved scope. A runtime array` |
|       - |  998 | `			 * callable carries a concrete class name (never self/static/parent), so a plain class` |
|       - |  999 | ``			 * lookup is correct — unlike the syntactic `C::m(...)` path, which must resolve`` |
|       - | 1000 | `			 * self/static/parent via VmFccResolveScope. Matches PH7_VmIsCallable's own decode. */` |
|      15 | 1001 | `			ph7_class *pScopeCls = PH7_VmExtractClassFromValue(pVm, pTarget);` |
|      15 | 1002 | `			ph7_class_instance *pFccObj = pScopeCls` |
|      14 | 1003 | `				? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;` |
|      15 | 1004 | `			if( pFccObj ){` |
|      15 | 1005 | `				pFccObj->iFlags \|= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */` |
|      22 | 1006 | `				if( PH7_VmFccMethodIsDirect(pVm,pScopeCls,` |
|       7 | 1007 | `						SyStringData(&sName),SyStringLength(&sName)) ){` |
|      13 | 1008 | `					pFccObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       6 | 1009 | `				}` |
|       7 | 1010 | `			}` |
|      15 | 1011 | `			return pFccObj;` |
|       - | 1012 | `		}` |
|       - | 1013 | `	}` |
|      12 | 1014 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 1015 | `		/* __invoke object (a real Closure is intercepted by the caller before this point).` |
|       - | 1016 | ``		 * The `__invoke` name is the ENGINE's, so mark the closure: php dispatches a`` |
|       - | 1017 | ``		 * non-public __invoke through this wrapper exactly as it does through `$obj()`. */`` |
|      12 | 1018 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|       - | 1019 | `		ph7_class_instance *pWrap;` |
|       - | 1020 | `		SyString sInvoke;` |
|      12 | 1021 | `		SyStringInitFromBuf(&sInvoke, "__invoke", sizeof("__invoke") - 1);` |
|      12 | 1022 | `		pWrap = VmCreateClosure(pVm, &sInvoke, pObj, &pObj->pClass->sName);` |
|      12 | 1023 | `		if( pWrap ){` |
|      12 | 1024 | `			pWrap->iFlags \|= VM_INSTANCE_FCC_INVOKE_OBJ;` |
|       5 | 1025 | `		}` |
|      12 | 1026 | `		return pWrap;` |
|       - | 1027 | `	}` |
|       - | 1028 | `	/* Unreachable in practice — the PH7_VmIsCallable gate admits only string/array/object, all` |
|       - | 1029 | `	 * handled above; kept to satisfy the non-void return path. */` |
|     ! 0 | 1030 | `	return 0;` |
|     100 | 1031 | `}` |
|       - | 1032 | `/*` |
|       - | 1033 | ` * Return a fresh Closure instance (iRef==0 from create/clone) as a builtin result, taking the` |
|       - | 1034 | ` * one reference into pCtx->pRet — mirrors the OP_LOAD_FCC store convention.` |
|       - | 1035 | ` */` |
|     128 | 1036 | `static int VmClosureResult(ph7_context *pCtx, ph7_class_instance *pClosure)` |
|       4 | 1037 | `{` |
|     132 | 1038 | `	if( pClosure == 0 ){` |
|     ! 0 | 1039 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1040 | `		return PH7_OK;` |
|       - | 1041 | `	}` |
|     132 | 1042 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     132 | 1043 | `	pClosure->iRef++;` |
|     132 | 1044 | `	pCtx->pRet->x.pOther = pClosure;` |
|     132 | 1045 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|     132 | 1046 | `	return PH7_OK;` |
|      68 | 1047 | `}` |
|       - | 1048 | `/*` |
|       - | 1049 | ` * Overwrite a Closure clone's $__this (bound object, or 0 to unbind) and, when pScope != 0, its` |
|       - | 1050 | ` * $__scope (the class-name string; pScope->nByte==0 clears it). pScope==0 leaves $__scope as the` |
|       - | 1051 | ` * clone inherited it (PHP's "static" = keep-scope). Refreshes VM_INSTANCE_FCC_BOUND from the result.` |
|       - | 1052 | ` * The clone inherited a ref on the original $__this (PH7_CloneClassInstance copies via MemObjStore);` |
|       - | 1053 | ` * this drops that and takes one on pNewThis.` |
|       - | 1054 | ` */` |
|      98 | 1055 | `static void VmClosureRebind(ph7_class_instance *pClone,` |
|       - | 1056 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|       3 | 1057 | `{` |
|       - | 1058 | `	SyString sAttr;` |
|       - | 1059 | `	ph7_value *pThisAttr, *pScopeAttr;` |
|     101 | 1060 | `	int bBound = 0;` |
|     101 | 1061 | `	SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     101 | 1062 | `	pThisAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|     101 | 1063 | `	if( pThisAttr ){` |
|       - | 1064 | `		/* PH7_MemObjRelease already drops the cloned-in object's refcount for an OBJ value —` |
|       - | 1065 | `		 * do NOT also decrement by hand (that double-frees the original bound object). */` |
|     101 | 1066 | `		PH7_MemObjRelease(pThisAttr);` |
|     101 | 1067 | `		if( pNewThis ){` |
|      85 | 1068 | `			pThisAttr->x.pOther = pNewThis;` |
|      85 | 1069 | `			MemObjSetType(pThisAttr, MEMOBJ_OBJ);` |
|      85 | 1070 | `			pNewThis->iRef++;` |
|      41 | 1071 | `		}` |
|      49 | 1072 | `	}` |
|     101 | 1073 | `	if( pScope ){` |
|      65 | 1074 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      65 | 1075 | `		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      65 | 1076 | `		if( pScopeAttr ){` |
|      65 | 1077 | `			PH7_MemObjRelease(pScopeAttr);` |
|      65 | 1078 | `			if( pScope->nByte ){` |
|      65 | 1079 | `				PH7_MemObjStringAppend(pScopeAttr, pScope->zString, pScope->nByte);` |
|      31 | 1080 | `			}` |
|      31 | 1081 | `		}` |
|      31 | 1082 | `	}` |
|       - | 1083 | `	/* Refresh the FCC_BOUND fast-path flag from the resulting $__this/$__scope. pThisAttr already` |
|       - | 1084 | `	 * points at the final $__this slot; only $__scope needs a (re)fetch — the top half fetched it` |
|       - | 1085 | `	 * just for pScope != 0. */` |
|     101 | 1086 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     101 | 1087 | `	pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      98 | 1088 | `	if( (pThisAttr && (pThisAttr->iFlags & MEMOBJ_OBJ))` |
|      60 | 1089 | `		\|\| (pScopeAttr && (pScopeAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeAttr->sBlob) > 0) ){` |
|      93 | 1090 | `		bBound = 1;` |
|      45 | 1091 | `	}` |
|     101 | 1092 | `	if( bBound ){` |
|      93 | 1093 | `		pClone->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      48 | 1094 | `	}else{` |
|      10 | 1095 | `		pClone->iFlags &= ~VM_INSTANCE_FCC_BOUND;` |
|       - | 1096 | `	}` |
|     101 | 1097 | `}` |
|       - | 1098 | `/*` |
|       - | 1099 | ` * Resolve the bindTo/bind/call $scope argument to a class-name SyString.` |
|       - | 1100 | ` * Returns 1 and fills *pOut if $__scope should be replaced (pOut->nByte==0 means "clear");` |
|       - | 1101 | ` * returns 0 to leave the scope unchanged (the PHP "static" sentinel / scope omitted).` |
|       - | 1102 | ` */` |
|     104 | 1103 | `static int VmClosureResolveScope(ph7_value *pScopeArg, SyString *pOut)` |
|       3 | 1104 | `{` |
|     107 | 1105 | `	if( pScopeArg == 0 ){` |
|      43 | 1106 | `		return 0; /* keep */` |
|       - | 1107 | `	}` |
|      64 | 1108 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeArg->sBlob) == 6` |
|      35 | 1109 | `		&& SyMemcmp((const void *)SyBlobData(&pScopeArg->sBlob), (const void *)"static", 6) == 0 ){` |
|       6 | 1110 | `		return 0; /* "static" -> keep current scope */` |
|       - | 1111 | `	}` |
|      63 | 1112 | `	if( pScopeArg->iFlags & MEMOBJ_NULL ){` |
|       3 | 1113 | `		SyStringInitFromBuf(pOut, 0, 0); /* unscoped */` |
|       3 | 1114 | `		return 1;` |
|       - | 1115 | `	}` |
|      61 | 1116 | `	if( pScopeArg->iFlags & MEMOBJ_OBJ ){` |
|       6 | 1117 | `		ph7_class_instance *pScopeObj = (ph7_class_instance *)pScopeArg->x.pOther;` |
|       6 | 1118 | `		*pOut = pScopeObj->pClass->sName;` |
|       6 | 1119 | `		return 1;` |
|       - | 1120 | `	}` |
|      57 | 1121 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) == 0 && (pScopeArg->iFlags & MEMOBJ_SCALAR) ){` |
|       - | 1122 | ``		/* php declares `object\|string\|null $newScope` and coerces a scalar into it in weak`` |
|       - | 1123 | ``		 * mode, so `bindTo($o, 5)` reaches the lookup as the NAME "5" and warns that no such`` |
|       - | 1124 | `		 * class exists. Falling through as "keep the current scope" bound it silently. */` |
|       3 | 1125 | `		PH7_MemObjToString(pScopeArg);` |
|       1 | 1126 | `	}` |
|      57 | 1127 | `	if( pScopeArg->iFlags & MEMOBJ_STRING ){` |
|      57 | 1128 | `		SyStringInitFromBuf(pOut, (const char *)SyBlobData(&pScopeArg->sBlob), SyBlobLength(&pScopeArg->sBlob));` |
|      57 | 1129 | `		return 1;` |
|       - | 1130 | `	}` |
|     ! 0 | 1131 | `	return 0;` |
|      55 | 1132 | `}` |
|       - | 1133 | `/*` |
|       - | 1134 | ``  * Fiber's C-bodied methods. Every one of them was a global `__fiber_verb($this,…)` `` |
|       - | 1135 | ` * thunk that a one-line prelude method forwarded to; the class body in the builtin` |
|       - | 1136 | ` * chunk now holds only its two private slots.` |
|       - | 1137 | ` */` |
|    4670 | 1138 | `PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm)` |
|       5 | 1139 | `{` |
|       - | 1140 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 1141 | `		{ "__construct",  PH7_MOD_PUBLIC, "callable $callback",  "",       vm_builtin_Fiber_construct },` |
|       - | 1142 | `		/* Variadic: the arguments now reach the C body directly instead of being` |
|       - | 1143 | `		 * repackaged by a func_get_args() call in the prelude. */` |
|       - | 1144 | `		{ "start",        PH7_MOD_PUBLIC, "mixed ...$args",      "mixed",  vm_builtin_Fiber_start },` |
|       - | 1145 | `		{ "resume",       PH7_MOD_PUBLIC, "mixed $value = null", "mixed",  vm_builtin_Fiber_resume },` |
|       - | 1146 | `		{ "getReturn",    PH7_MOD_PUBLIC, "",                    "mixed",  vm_builtin_Fiber_getReturn },` |
|       - | 1147 | `		{ "isStarted",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isStarted },` |
|       - | 1148 | `		{ "isRunning",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isRunning },` |
|       - | 1149 | `		{ "isSuspended",  PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isSuspended },` |
|       - | 1150 | `		{ "isTerminated", PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isTerminated },` |
|       - | 1151 | `		/* Static, and the only one that never took a receiver even as a thunk:` |
|       - | 1152 | ``		 * `__fiber_suspend($value)` already read the value from argument #0. */`` |
|       - | 1153 | `		{ "suspend",      PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "mixed $value = null", "mixed",` |
|       - | 1154 | `		  vm_builtin_Fiber_suspend },` |
|       - | 1155 | `		{ "__destruct",   PH7_MOD_PUBLIC, "",                    "",       vm_builtin_Fiber_destruct },` |
|       - | 1156 | `	};` |
|       - | 1157 | `	/* The two private slots the methods above keep their state in: the execution` |
|       - | 1158 | `	 * context (a resource) and the callable handed to the constructor. */` |
|       - | 1159 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 1160 | `		{ "__ctx",      PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1161 | `		{ "__callable", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1162 | `	};` |
|       - | 1163 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1164 | `		"Fiber", 0, 0, PH7_CLASS_NOSERIALIZE,` |
|       - | 1165 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|       - | 1166 | `		0, 0,` |
|       - | 1167 | `		aProp, SX_ARRAYSIZE(aProp),` |
|       - | 1168 | `		0, 0, 0` |
|       - | 1169 | `	};` |
|    4675 | 1170 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|       5 | 1171 | `}` |
|       - | 1172 | `/*` |
|       - | 1173 | `` * Generator's C-bodied methods, plus the `implements Iterator` the chunk can no`` |
|       - | 1174 | ` * longer carry: PH7_ClassImplement installs an abstract stub for any interface` |
|       - | 1175 | ` * method the class does not already declare, so it has to run AFTER the eight` |
|       - | 1176 | ` * methods below exist — at which point the stubs are skipped and the class is` |
|       - | 1177 | ` * concrete, exactly as the prelude declaration used to make it.` |
|       - | 1178 | ` */` |
|    4670 | 1179 | `PH7_PRIVATE sxi32 PH7_VmInstallGeneratorNative(ph7_vm *pVm)` |
|       5 | 1180 | `{` |
|       - | 1181 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 1182 | `		{ "current",    PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_current },` |
|       - | 1183 | `		{ "key",        PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_key },` |
|       - | 1184 | `		{ "next",       PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_next },` |
|       - | 1185 | `		{ "rewind",     PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_rewind },` |
|       - | 1186 | `		{ "valid",      PH7_MOD_PUBLIC, "",                 "bool",  vm_builtin_Generator_valid },` |
|       - | 1187 | ``		/* php REQUIRES the argument here; the prelude declared `$value = null`, so`` |
|       - | 1188 | ``		 * `$gen->send()` used to answer the first yielded value instead of raising. */`` |
|       - | 1189 | `		{ "send",       PH7_MOD_PUBLIC, "mixed $value",     "mixed", vm_builtin_Generator_send },` |
|       - | 1190 | `		{ "throw",      PH7_MOD_PUBLIC, "Throwable $exception", "mixed", vm_builtin_Generator_throw },` |
|       - | 1191 | `		{ "getReturn",  PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_getReturn },` |
|       - | 1192 | `		{ "__destruct", PH7_MOD_PUBLIC, "",                 "",      vm_builtin_Generator_destruct },` |
|       - | 1193 | `	};` |
|       - | 1194 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 1195 | `		{ "__ctx", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1196 | `	};` |
|       - | 1197 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1198 | `		"Generator", 0, 0, PH7_CLASS_NOSERIALIZE,` |
|       - | 1199 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|       - | 1200 | `		0, 0,` |
|       - | 1201 | `		aProp, SX_ARRAYSIZE(aProp),` |
|       - | 1202 | `		0, 0, 0` |
|       - | 1203 | `	};` |
|       - | 1204 | `	ph7_class *pClass;` |
|       - | 1205 | `	ph7_class *pIterator;` |
|    4675 | 1206 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    4675 | 1207 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1208 | `		return rc;` |
|       - | 1209 | `	}` |
|       - | 1210 | ``	/* `implements Iterator` last, for the reason in this function's header. */`` |
|    4675 | 1211 | `	pClass = PH7_VmExtractClass(&(*pVm),"Generator",sizeof("Generator")-1,0,0);` |
|    4675 | 1212 | `	pIterator = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,0,0);` |
|    4675 | 1213 | `	if( pClass == 0 \|\| pIterator == 0 ){` |
|     ! 0 | 1214 | `		return SXERR_NOTFOUND;` |
|       - | 1215 | `	}` |
|    4675 | 1216 | `	return PH7_ClassImplement(pClass,pIterator);` |
|    2340 | 1217 | `}` |
|       - | 1218 | `/*` |
|       - | 1219 | ` * Closure::__construct() — php declares it PRIVATE and still words the refusal as` |
|       - | 1220 | ` * an instantiation error rather than a visibility one, so the body has to exist.` |
|       - | 1221 | ` */` |
|     ! 0 | 1222 | `PH7_PRIVATE int vm_builtin_Closure_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     ! 0 | 1223 | `{` |
|     ! 0 | 1224 | `	SXUNUSED(nArg);` |
|     ! 0 | 1225 | `	SXUNUSED(apArg);` |
|     ! 0 | 1226 | `	return PH7_VmThrowException(pCtx, "Error",` |
|       - | 1227 | `		"Instantiation of class Closure is not allowed");` |
|     ! 0 | 1228 | `}` |
|       - | 1229 | `/*` |
|       - | 1230 | ` * Clone a Closure for a rebind, carrying the marks that say WHAT it wraps. They live on the` |
|       - | 1231 | ` * instance rather than in an attribute, so PH7_CloneClassInstance — which copies attributes —` |
|       - | 1232 | `` * left them behind: the clone of a method callable forgot that its `$__fn` names a screened`` |
|       - | 1233 | `` * METHOD, and `$fcc->bindTo($other)` came back as a closure whose every dispatch re-resolved`` |
|       - | 1234 | ` * the name and re-decided its visibility (or, with no method of that name in reach, looked` |
|       - | 1235 | ` * for a global FUNCTION).` |
|       - | 1236 | ` */` |
|      98 | 1237 | `static ph7_class_instance * VmCloneClosureInstance(ph7_class_instance *pClosure)` |
|       3 | 1238 | `{` |
|     101 | 1239 | `	ph7_class_instance *pClone = PH7_CloneClassInstance(pClosure);` |
|     101 | 1240 | `	if( pClone ){` |
|     150 | 1241 | `		pClone->iFlags \|= pClosure->iFlags` |
|      98 | 1242 | `			& (VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED\|VM_INSTANCE_FCC_INVOKE_OBJ);` |
|      49 | 1243 | `	}` |
|     101 | 1244 | `	return pClone;` |
|       3 | 1245 | `}` |
|       - | 1246 | `/*` |
|       - | 1247 | `` * Is this closure STATIC — one that can never take a `$this`? For a plain closure that is the`` |
|       - | 1248 | `` * `static function(){}` declaration flag; for a METHOD callable it is the method's own`` |
|       - | 1249 | `` * staticness, which the flag cannot see because `$__fn` names a method and not a function in`` |
|       - | 1250 | `` * hFunction. `Base::stat(...)` is exactly as static as `static fn()` to php.`` |
|       - | 1251 | ` */` |
|     102 | 1252 | `static int VmClosureIsStatic(ph7_vm *pVm, ph7_class_instance *pClosure)` |
|       3 | 1253 | `{` |
|       - | 1254 | `	SyString sAttr;` |
|       - | 1255 | `	ph7_value *pFn;` |
|     105 | 1256 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|     105 | 1257 | `	pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|     105 | 1258 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 | 1259 | `		return 0;` |
|       - | 1260 | `	}` |
|     105 | 1261 | `	if( pClosure->iFlags & VM_INSTANCE_FCC_METHOD ){` |
|      34 | 1262 | `		ph7_class *pScope = PH7_VmClosureScopeClass(pVm, pClosure);` |
|      34 | 1263 | `		ph7_class_method *pMeth = pScope` |
|      48 | 1264 | `			? PH7_ClassExtractMethod(pScope, (const char *)SyBlobData(&pFn->sBlob),` |
|      32 | 1265 | `				SyBlobLength(&pFn->sBlob)) : 0;` |
|      34 | 1266 | `		return (pMeth && (pMeth->iFlags & PH7_CLASS_ATTR_STATIC)) ? 1 : 0;` |
|       - | 1267 | `	}` |
|       - | 1268 | `	{` |
|     108 | 1269 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob),` |
|      35 | 1270 | `			SyBlobLength(&pFn->sBlob));` |
|      73 | 1271 | `		return (pEntry && (((ph7_vm_func *)pEntry->pUserData)->iFlags & VM_FUNC_STATIC_CL)) ? 1 : 0;` |
|       - | 1272 | `	}` |
|      54 | 1273 | `}` |
|       - | 1274 | `/*` |
|       - | 1275 | ` * php's four refusals for a rebind (zend_valid_closure_binding), in php's order. A closure` |
|       - | 1276 | `` * created from a METHOD is a "fake closure": it wraps a resolved function, so its `$this` may`` |
|       - | 1277 | ` * only move WITHIN the class that declared it and its scope may not move at all. PHL applied` |
|       - | 1278 | `` * none of them beyond the static-closure one, so `$fcc->bindTo($unrelated)` handed back a`` |
|       - | 1279 | `` * closure that could only fail later, and `->bindTo(null)` one with no receiver for a method`` |
|       - | 1280 | ` * that needs one. Each refusal is php's E_WARNING plus a NULL result; returns 0 when it fired.` |
|       - | 1281 | ` *` |
|       - | 1282 | `` * pScope is the resolved $scope ARGUMENT (0 when omitted or `"static"`, which both mean keep);`` |
|       - | 1283 | `` * an empty one is an explicit `null`, which php counts as a rebind like any other.`` |
|       - | 1284 | ` */` |
|     116 | 1285 | `static int VmClosureBindAllowed(ph7_vm *pVm, ph7_class_instance *pClosure,` |
|       - | 1286 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|       3 | 1287 | `{` |
|     119 | 1288 | `	int bMethod = (pClosure->iFlags & VM_INSTANCE_FCC_METHOD) != 0;` |
|     119 | 1289 | `	ph7_class *pOwn = bMethod ? PH7_VmClosureScopeClass(pVm, pClosure) : 0;` |
|     119 | 1290 | `	if( pNewThis ){` |
|      99 | 1291 | `		if( VmClosureIsStatic(pVm, pClosure) ){` |
|       6 | 1292 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1293 | `				"Cannot bind an instance to a static closure, this will be an error in PHP 9");` |
|       6 | 1294 | `			return 0;` |
|       - | 1295 | `		}` |
|      95 | 1296 | `		if( pOwn && !PH7_VmInstanceOf(pNewThis->pClass, pOwn) ){` |
|       - | 1297 | `			SyString sFn;` |
|       - | 1298 | `			ph7_value *pFn;` |
|       5 | 1299 | `			SyStringInitFromBuf(&sFn, "__fn", 4);` |
|       5 | 1300 | `			pFn = PH7_ClassInstanceFetchAttr(pClosure, &sFn);` |
|       5 | 1301 | `			SyStringInitFromBuf(&sFn, pFn ? (const char *)SyBlobData(&pFn->sBlob) : "",` |
|       - | 1302 | `				pFn ? SyBlobLength(&pFn->sBlob) : 0);` |
|       7 | 1303 | `			VmErrorFormat(pVm,PH7_CTX_WARNING,` |
|       - | 1304 | `				"Cannot bind method %z::%z() to object of class %z, this will be an error in PHP 9",` |
|       4 | 1305 | `				&pOwn->sName,&sFn,&pNewThis->pClass->sName);` |
|       5 | 1306 | `			return 0;` |
|       3 | 1307 | `		}` |
|      66 | 1308 | `	}else if( pOwn && !VmClosureIsStatic(pVm, pClosure) ){` |
|       5 | 1309 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1310 | `			"Cannot unbind $this of method, this will be an error in PHP 9");` |
|       5 | 1311 | `		return 0;` |
|       - | 1312 | `	}` |
|     107 | 1313 | `	if( pScope && bMethod ){` |
|      16 | 1314 | `		ph7_class *pWant = pScope->nByte` |
|       9 | 1315 | `			? PH7_VmResolveScopeName(pVm, pScope->zString, pScope->nByte) : 0;` |
|      11 | 1316 | `		if( pWant != pOwn ){` |
|       7 | 1317 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1318 | `				"Cannot rebind scope of closure created from method, this will be an error in PHP 9");` |
|       7 | 1319 | `			return 0;` |
|       - | 1320 | `		}` |
|       2 | 1321 | `	}` |
|     101 | 1322 | `	return 1;` |
|      61 | 1323 | `}` |
|       - | 1324 | `/*` |
|       - | 1325 | ` * Closure::call(object $newThis, mixed ...$args) — bind and invoke in one step.` |
|       - | 1326 | ` *` |
|       - | 1327 | `` * This was the last PHP left in the class: `$bound = $this->bindTo($newThis,`` |
|       - | 1328 | `` * get_class($newThis)); return $bound(...$args);`. That spelling leaked its own`` |
|       - | 1329 | `` * internals — a non-object argument reported `get_class(): Argument #1 ($object)`` |
|       - | 1330 | `` * must be of type object, string given` where php names THIS method's parameter,`` |
|       - | 1331 | `` * which is what declaring `object $newThis` buys (the shared screen words it).`` |
|       - | 1332 | ` * The scope php binds is the new $this's class, exactly as the PHP did.` |
|       - | 1333 | ` */` |
|      22 | 1334 | `PH7_PRIVATE int vm_builtin_Closure_call(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 1335 | `{` |
|      24 | 1336 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1337 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|      24 | 1338 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       - | 1339 | `	ph7_value sBound;` |
|       - | 1340 | `	SyString sScope;` |
|       - | 1341 | `	sxi32 rc;` |
|      24 | 1342 | `	if( nArg < 1 ){` |
|     ! 0 | 1343 | `		return PH7_VmThrowException(pCtx, "ArgumentCountError",` |
|       - | 1344 | `			"Closure::call() expects at least 1 argument, 0 given");` |
|       - | 1345 | `	}` |
|      24 | 1346 | `	if( pRecv == 0 \|\| !VmValueIsClosure(pVm, pRecv) ){` |
|     ! 0 | 1347 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1348 | `		return PH7_OK;` |
|       - | 1349 | `	}` |
|      24 | 1350 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| apArg[0]->x.pOther == 0 ){` |
|       - | 1351 | ``		/* Unreachable while the declared `object $newThis` is screened; kept because`` |
|       - | 1352 | `		 * rule 44's family says a screen written for one body shape has not` |
|       - | 1353 | `		 * necessarily run for this one. */` |
|     ! 0 | 1354 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1355 | `			"Closure::call(): Argument #1 ($newThis) must be of type object");` |
|       - | 1356 | `	}` |
|      24 | 1357 | `	pClosure = (ph7_class_instance *)pRecv->x.pOther;` |
|      24 | 1358 | `	pNewThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      24 | 1359 | `	SyStringInitFromBuf(&sScope, pNewThis->pClass->sName.zString, pNewThis->pClass->sName.nByte);` |
|       - | 1360 | `	/* call() BINDS before it invokes, so php's rebind refusals apply to it — and because the` |
|       - | 1361 | `	 * scope it asks for is the new $this's class, a method callable handed an instance of` |
|       - | 1362 | `	 * anything but its own declaring class is refused for the scope, not the receiver. */` |
|      24 | 1363 | `	if( !VmClosureBindAllowed(pVm, pClosure, pNewThis, &sScope) ){` |
|       3 | 1364 | `		ph7_result_null(pCtx);` |
|       3 | 1365 | `		return PH7_OK;` |
|       - | 1366 | `	}` |
|      22 | 1367 | `	pClone = VmCloneClosureInstance(pClosure);` |
|      22 | 1368 | `	if( pClone == 0 ){` |
|     ! 0 | 1369 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1370 | `		return PH7_OK;` |
|       - | 1371 | `	}` |
|      22 | 1372 | `	VmClosureRebind(pClone, pNewThis, &sScope);` |
|       - | 1373 | `	/* The bound closure is handed to the dispatcher through a STACK carrier that` |
|       - | 1374 | `	 * takes its own reference (rule 16): a context value would be released with the` |
|       - | 1375 | `	 * call context and unref the instance a second time. */` |
|      22 | 1376 | `	PH7_MemObjInit(pVm, &sBound);` |
|      22 | 1377 | `	sBound.x.pOther = pClone;` |
|      22 | 1378 | `	MemObjSetType(&sBound, MEMOBJ_OBJ);` |
|      22 | 1379 | `	pClone->iRef++;` |
|      22 | 1380 | `	rc = PH7_VmCallUserFunction(pVm, &sBound, nArg - 1, apArg + 1, pCtx->pRet);` |
|      22 | 1381 | `	PH7_MemObjRelease(&sBound);` |
|      22 | 1382 | `	return rc;` |
|      13 | 1383 | `}` |
|       - | 1384 | `/*` |
|       - | 1385 | ` * Closure — declared entirely from C.` |
|       - | 1386 | ` *` |
|       - | 1387 | ` * Its three methods were the first in the engine whose body is a C routine rather` |
|       - | 1388 | `` * than bytecode (VM_FUNC_NATIVE), retiring the global `__closure_bindTo` /`` |
|       - | 1389 | `` * `__closure_fromCallable` thunks a prelude method used to forward to. The`` |
|       - | 1390 | ` * DECLARATION stayed in the builtin chunk until now, which cost three things: the` |
|       - | 1391 | `` * engine slots `$__fn`/`$__this`/`$__scope` were on every presentation surface`` |
|       - | 1392 | `` * (php's Closure has NO properties), `__construct` was public where php's is`` |
|       - | 1393 | `` * private, and `call()` was PHP that leaked `get_class()`'s diagnostic.`` |
|       - | 1394 | ` */` |
|    4670 | 1395 | `PH7_PRIVATE sxi32 PH7_VmInstallClosureNative(ph7_vm *pVm)` |
|       5 | 1396 | `{` |
|       - | 1397 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 1398 | `		/* Parameter names are php's own ($newScope, not $scope): this string is the` |
|       - | 1399 | `		 * declaration of record for arity, by-ref positions and the reported` |
|       - | 1400 | `		 * parameter list. */` |
|       - | 1401 | `		{ "__construct",  PH7_MOD_PRIVATE, "", 0,` |
|       - | 1402 | `		  vm_builtin_Closure_construct },` |
|       - | 1403 | `		{ "bindTo",       PH7_MOD_PUBLIC,` |
|       - | 1404 | `		  "?object $newThis, object\|string\|null $newScope = \"static\"", "?Closure",` |
|       - | 1405 | `		  vm_builtin_Closure_bindTo },` |
|       - | 1406 | `		{ "bind",         PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|       - | 1407 | `		  "Closure $closure, ?object $newThis, object\|string\|null $newScope = \"static\"", "?Closure",` |
|       - | 1408 | `		  vm_builtin_Closure_bindTo },` |
|       - | 1409 | `		{ "call",         PH7_MOD_PUBLIC,` |
|       - | 1410 | `		  "object $newThis, mixed ...$args", "mixed",` |
|       - | 1411 | `		  vm_builtin_Closure_call },` |
|       - | 1412 | `		{ "fromCallable", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|       - | 1413 | `		  "callable $callback", "Closure",` |
|       - | 1414 | `		  vm_builtin_Closure_fromCallable },` |
|       - | 1415 | `	};` |
|       - | 1416 | `	/* The engine's own slots: the callable NAME, the bound receiver and the bound` |
|       - | 1417 | `	 * scope. php presents no property at all for a Closure, so all three carry` |
|       - | 1418 | ``	 * PH7_MOD_HIDDEN — they keep working for `new`, `clone` and the C bodies (and`` |
|       - | 1419 | `	 * for serialize(), which this class refuses anyway) and disappear from` |
|       - | 1420 | `	 * var_dump/print_r/(array)/get_object_vars/foreach/json_encode and Reflection. */` |
|       - | 1421 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 1422 | `		{ "__fn",    PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1423 | `		{ "__this",  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1424 | `		{ "__scope", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1425 | `	};` |
|       - | 1426 | `	/* php's get_debug_info for a Closure shows a SHAPE none of those three slots` |
|       - | 1427 | `	 * is (name/file/line or function, static, this, parameter) — see` |
|       - | 1428 | `	 * PH7_ClosurePresent, which lives beside the reflection machinery that already` |
|       - | 1429 | `	 * describes any callable's parameters. */` |
|       - | 1430 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1431 | `		"Closure", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOINSTANTIATE,` |
|       - | 1432 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|       - | 1433 | `		0, 0,` |
|       - | 1434 | `		aProp, SX_ARRAYSIZE(aProp),` |
|       - | 1435 | `		0, 0, PH7_ClosurePresent` |
|       - | 1436 | `	};` |
|    4675 | 1437 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|       5 | 1438 | `}` |
|       - | 1439 | `/*` |
|       - | 1440 | ` * Closure::bindTo($newThis, $scope='static') / Closure::bind($closure, $newThis, $scope='static').` |
|       - | 1441 | ` * Clone the receiver and rebind $this/$scope; returns the new Closure (NULL on a non-Closure` |
|       - | 1442 | ` * receiver, matching PHP's failure mode).` |
|       - | 1443 | ` */` |
|     104 | 1444 | `PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 1445 | `{` |
|     107 | 1446 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1447 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|       - | 1448 | `	ph7_value *pNewThisArg;` |
|       - | 1449 | `	ph7_value *pRecv;` |
|       - | 1450 | `	SyString sScope;` |
|     107 | 1451 | `	const SyString *pScopePtr = 0;` |
|       - | 1452 | `	/* One body, both spellings — as it always was, except the closure now arrives` |
|       - | 1453 | `	 * the way php passes it rather than as a hand-written first argument. Called as` |
|       - | 1454 | `	 * the instance method bindTo(), the receiver IS the closure and the arguments` |
|       - | 1455 | `	 * start at $newThis; called as the static bind(), the closure is argument #1.` |
|       - | 1456 | `	 * Normalizing here is what lets the two share an implementation. */` |
|     107 | 1457 | `	if( PH7_ContextThis(pCtx) ){` |
|      79 | 1458 | `		pRecv = PH7_ContextThisValue(pCtx);` |
|      41 | 1459 | `	}else{` |
|      30 | 1460 | `		if( nArg < 1 ){` |
|     ! 0 | 1461 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1462 | `			return PH7_OK;` |
|       - | 1463 | `		}` |
|      30 | 1464 | `		pRecv = apArg[0];` |
|      30 | 1465 | `		apArg++;` |
|      30 | 1466 | `		nArg--;` |
|       - | 1467 | `	}` |
|     107 | 1468 | `	if( nArg < 1 \|\| !VmValueIsClosure(pVm, pRecv) ){` |
|     ! 0 | 1469 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1470 | `		return PH7_OK;` |
|       - | 1471 | `	}` |
|     107 | 1472 | `	pClosure = (ph7_class_instance *)pRecv->x.pOther;` |
|     107 | 1473 | `	pNewThisArg = apArg[0];` |
|     107 | 1474 | `	if( pNewThisArg->iFlags & MEMOBJ_NULL ){` |
|      25 | 1475 | `		pNewThis = 0;` |
|      96 | 1476 | `	}else if( pNewThisArg->iFlags & MEMOBJ_OBJ ){` |
|      85 | 1477 | `		pNewThis = (ph7_class_instance *)pNewThisArg->x.pOther;` |
|      44 | 1478 | `	}else{` |
|     ! 0 | 1479 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1480 | `			"Closure::bindTo(): Argument #1 ($newThis) must be of type ?object");` |
|       - | 1481 | `	}` |
|     107 | 1482 | `	if( VmClosureResolveScope((nArg > 1) ? apArg[1] : 0, &sScope) ){` |
|      63 | 1483 | `		pScopePtr = &sScope;` |
|      30 | 1484 | `	}` |
|       - | 1485 | `	/* php RESOLVES the scope argument before it decides anything else, and a name no class` |
|       - | 1486 | `	 * answers to is its own warning — ahead of all four rebind refusals, for a plain closure` |
|       - | 1487 | `	 * as much as for a method one. PHL bound the unresolvable scope in silence and then had` |
|       - | 1488 | ``	 * no scope at all, so `bindTo($o, 'Typo')` produced a closure that could not reach the`` |
|       - | 1489 | `	 * private members it was being bound for. */` |
|     104 | 1490 | `	if( pScopePtr && pScopePtr->nByte` |
|      62 | 1491 | `	 && PH7_VmResolveScopeName(pVm, pScopePtr->zString, pScopePtr->nByte) == 0 ){` |
|      11 | 1492 | `		VmErrorFormat(pVm,PH7_CTX_WARNING,"Class \"%z\" not found",pScopePtr);` |
|      11 | 1493 | `		ph7_result_null(pCtx);` |
|      11 | 1494 | `		return PH7_OK;` |
|       - | 1495 | `	}` |
|      97 | 1496 | `	if( !VmClosureBindAllowed(pVm, pClosure, pNewThis, pScopePtr) ){` |
|      18 | 1497 | `		ph7_result_null(pCtx);` |
|      18 | 1498 | `		return PH7_OK;` |
|       - | 1499 | `	}` |
|      81 | 1500 | `	pClone = VmCloneClosureInstance(pClosure);` |
|      81 | 1501 | `	if( pClone == 0 ){` |
|     ! 0 | 1502 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1503 | `		return PH7_OK;` |
|       - | 1504 | `	}` |
|      81 | 1505 | `	VmClosureRebind(pClone, pNewThis, pScopePtr);` |
|      81 | 1506 | `	return VmClosureResult(pCtx, pClone);` |
|      55 | 1507 | `}` |
|       - | 1508 | `/*` |
|       - | 1509 | ` * Closure::fromCallable($callable) — normalize any callable value to a Closure (reuses the` |
|       - | 1510 | ` * VmFccWrapValue primitive); idempotent on a Closure; TypeError on a non-callable.` |
|       - | 1511 | ` */` |
|      68 | 1512 | `PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 1513 | `{` |
|      70 | 1514 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1515 | `	ph7_class_instance *pClosure;` |
|      70 | 1516 | `	if( nArg < 1 ){` |
|     ! 0 | 1517 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1518 | `			"Closure::fromCallable() expects exactly 1 argument, 0 given");` |
|       - | 1519 | `	}` |
|      70 | 1520 | `	if( VmValueIsClosure(pVm, apArg[0]) ){` |
|       3 | 1521 | `		ph7_result_value(pCtx, apArg[0]); /* already a Closure: idempotent */` |
|       3 | 1522 | `		return PH7_OK;` |
|       - | 1523 | `	}` |
|      68 | 1524 | `	pClosure = VmFccWrapValue(pVm, apArg[0]);` |
|      68 | 1525 | `	if( pClosure == 0 ){` |
|       - | 1526 | `		/* php says WHY, with the same reason taxonomy every callback argument uses —` |
|       - | 1527 | ``		 * `Failed to create closure from callable: class P does not have a method "zz"`.`` |
|       - | 1528 | `		 * PHL answered one flat "is not a valid callback" for all eight causes, so a typo` |
|       - | 1529 | `		 * in a method name, a private one, a missing class and a bad array shape were` |
|       - | 1530 | `		 * indistinguishable. PH7_VmCallableReason is the shared builder (its tails are` |
|       - | 1531 | `		 * already byte-exact for call_user_func & friends); the fallback covers the OOM` |
|       - | 1532 | `		 * path, where the value IS callable and the reason is 0. */` |
|       - | 1533 | `		char zWhy[192];` |
|      17 | 1534 | `		const char *zReason = PH7_VmCallableReason(pVm, apArg[0], zWhy, sizeof(zWhy));` |
|      17 | 1535 | `		if( zReason ){` |
|      25 | 1536 | `			return PH7_VmThrowException(pCtx, "TypeError",` |
|       8 | 1537 | `				"Failed to create closure from callable: %s", zReason);` |
|       - | 1538 | `		}` |
|     ! 0 | 1539 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1540 | `			"Failed to create closure from callable");` |
|       - | 1541 | `	}` |
|      52 | 1542 | `	return VmClosureResult(pCtx, pClosure);` |
|      36 | 1543 | `}` |
|       - | 1544 | `/*` |
|       - | 1545 | ` * Fiber::suspend($value = null) — static method.` |
|       - | 1546 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|       - | 1547 | ` */` |
|     362 | 1548 | `PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1549 | `{` |
|     367 | 1550 | `	ph7_vm *pVm = pCtx->pVm;` |
|     367 | 1551 | `	if( pVm->pActiveCtx == 0 ){` |
|     ! 0 | 1552 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1553 | `			"Cannot suspend outside of a fiber");` |
|       - | 1554 | `	}` |
|       - | 1555 | `	/* Stage 4 scoped divergence: the trampoline only makes PHP->PHP CALLs` |
|       - | 1556 | `	 * iterative. Every OTHER re-entry runs on a fresh native VmByteCodeExec` |
|       - | 1557 | `	 * activation (nVmExecDepth bumped) that PH7_SUSPEND cannot unwind across` |
|       - | 1558 | `	 * without real coroutine stacks (BYTECODE.md §2.4): a C->PHP callback` |
|       - | 1559 | `	 * (usort/array_map/preg_replace_callback comparator), and — because fibers` |
|       - | 1560 | `	 * use the LEGACY (non-inline) try/catch machinery — a suspend inside a` |
|       - | 1561 | `	 * fiber's catch/finally body, a match/switch arm, or eval()/include()'d` |
|       - | 1562 | `	 * code, all of which run via VmLocalExec. php does all of these via full` |
|       - | 1563 | `	 * native-stack switching; PHL raises a catchable FiberError instead of the` |
|       - | 1564 | `	 * old silent corruption. A suspend in a fiber's try BODY (not catch/finally)` |
|       - | 1565 | `	 * runs in the main dispatch loop and parks normally. A recorded` |
|       - | 1566 | `	 * residual; making the catch/finally case work needs fibers on the inline` |
|       - | 1567 | `	 * try machinery (the generator ROOT C path), a follow-up. */` |
|     367 | 1568 | `	if( pVm->nVmExecDepth != pVm->pActiveCtx->nBodyExecDepth ){` |
|       6 | 1569 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1570 | `			"Cannot suspend across an internal call boundary");` |
|       - | 1571 | `	}` |
|     363 | 1572 | `	if( nArg > 0 ){` |
|     357 | 1573 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|     181 | 1574 | `	}else{` |
|       9 | 1575 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|       - | 1576 | `	}` |
|     363 | 1577 | `	return PH7_SUSPEND;` |
|     186 | 1578 | `}` |
|       - | 1579 | `/*` |
|       - | 1580 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|       - | 1581 | ` * Actual resolution is deferred to start() so that overload selection` |
|       - | 1582 | ` * and closure-environment binding happen with the correct argument context.` |
|       - | 1583 | ` */` |
|     270 | 1584 | `PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1585 | `{` |
|       - | 1586 | `	ph7_class_instance *pThis;` |
|       - | 1587 | `	ph7_value *pAttr;` |
|       - | 1588 | `	SyString sAttrName;` |
|     275 | 1589 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     275 | 1590 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|     275 | 1591 | `	if( nArg < 1 ){` |
|     ! 0 | 1592 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1593 | `			"Fiber::__construct() expects a callable argument");` |
|       - | 1594 | `	}` |
|     275 | 1595 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1596 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1597 | `			"Fiber::__construct(): invalid $this");` |
|       - | 1598 | `	}` |
|     275 | 1599 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|     275 | 1600 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|     ! 0 | 1601 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1602 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|       - | 1603 | `	}` |
|       - | 1604 | `	/* Basic validation: callable must be a string or closure (object) */` |
|     275 | 1605 | `	if( (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|     ! 0 | 1606 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1607 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|       - | 1608 | `	}` |
|       - | 1609 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|     275 | 1610 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     275 | 1611 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     275 | 1612 | `	if( pAttr ){` |
|     275 | 1613 | `		PH7_MemObjStore(apArg[0], pAttr);` |
|     135 | 1614 | `	}` |
|     275 | 1615 | `	return PH7_OK;` |
|     140 | 1616 | `}` |
|       - | 1617 | `/*` |
|       - | 1618 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|       - | 1619 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|       - | 1620 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|       - | 1621 | ` * so that start() can bind it as $this for the closure environment.` |
|       - | 1622 | ` */` |
|     264 | 1623 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|       - | 1624 | `	ph7_class_instance **ppThis)` |
|       5 | 1625 | `{` |
|     269 | 1626 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1627 | `	ph7_value *pCallable;` |
|       - | 1628 | `	SyString sAttrName;` |
|     269 | 1629 | `	*ppThis = 0;` |
|     269 | 1630 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     269 | 1631 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|     269 | 1632 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|     ! 0 | 1633 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|     ! 0 | 1634 | `		return 0;` |
|       - | 1635 | `	}` |
|     269 | 1636 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|       - | 1637 | `		/* String callable — look up in user functions with overload support */` |
|       - | 1638 | `		SyString sName;` |
|       - | 1639 | `		SyHashEntry *pEntry;` |
|       - | 1640 | `		ph7_vm_func *pFunc;` |
|     235 | 1641 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|     235 | 1642 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|     235 | 1643 | `		if( pEntry == 0 ){` |
|     ! 0 | 1644 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|     ! 0 | 1645 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|     ! 0 | 1646 | `			return 0;` |
|       - | 1647 | `		}` |
|     235 | 1648 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     235 | 1649 | `		return pFunc;` |
|     ! 0 | 1650 | `	}else{` |
|      38 | 1651 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|       - | 1652 | `		ph7_class_method *pMethod;` |
|      38 | 1653 | `		if( VmValueIsClosure(pVm, pCallable) ){` |
|       - | 1654 | `			/* A real Closure object: unwrap to its underlying callable name (the single` |
|       - | 1655 | `			 * source of truth, VmClosureUnwrap) and resolve that function. Its captured` |
|       - | 1656 | ``			 * environment (including any `$this`) rides along in the named function's`` |
|       - | 1657 | `			 * aClosureEnv, installed by VmFiberSetupFrame, so *ppThis stays 0. */` |
|       - | 1658 | `			ph7_value sName;` |
|      38 | 1659 | `			SyHashEntry *pEntry = 0;` |
|      38 | 1660 | `			PH7_MemObjInit(pVm, &sName);` |
|      38 | 1661 | `			if( VmClosureUnwrap(pVm, pCallable, &sName) == SXRET_OK ){` |
|      38 | 1662 | `				pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&sName.sBlob), SyBlobLength(&sName.sBlob));` |
|      17 | 1663 | `			}` |
|      38 | 1664 | `			PH7_MemObjRelease(&sName);` |
|      38 | 1665 | `			if( pEntry ){` |
|       - | 1666 | `				/* A BOUND closure parked its $this in the pClosureThis transient` |
|       - | 1667 | `				 * (VmClosureUnwrap): consume it as the fiber's $this — it wins over` |
|       - | 1668 | `				 * the creation-time env capture (VmFiberSetupFrame's skip). *ppThis` |
|       - | 1669 | `				 * is a borrow (the closure's $__this attr keeps the object alive` |
|       - | 1670 | `				 * through $__callable), so drop the parked ref. The scope transient` |
|       - | 1671 | `				 * is cleared alongside: fiber bodies don't model pBoundScope` |
|       - | 1672 | `				 * visibility (recorded residual), and a stale transient would` |
|       - | 1673 | `				 * poison the next OP_CALL's frame. */` |
|      38 | 1674 | `				if( pVm->pClosureThis ){` |
|     ! 0 | 1675 | `					*ppThis = pVm->pClosureThis;` |
|     ! 0 | 1676 | `					PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1677 | `					pVm->pClosureThis = 0;` |
|     ! 0 | 1678 | `				}` |
|      38 | 1679 | `				pVm->pClosureScope = 0;` |
|      38 | 1680 | `				pVm->bClosureScreened = 0;` |
|      38 | 1681 | `				return (ph7_vm_func *)pEntry->pUserData;` |
|       - | 1682 | `			}` |
|     ! 0 | 1683 | `			if( pVm->pClosureThis ){` |
|       - | 1684 | `				/* Failed resolution: drop the parked transient so it neither leaks` |
|       - | 1685 | `				 * nor poisons the next call. */` |
|     ! 0 | 1686 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1687 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 1688 | `			}` |
|     ! 0 | 1689 | `			pVm->pClosureScope = 0;` |
|     ! 0 | 1690 | `			pVm->bClosureScreened = 0;` |
|     ! 0 | 1691 | `			PH7_VmThrowException(pCtx, "FiberError", "Fiber callable closure could not be resolved");` |
|     ! 0 | 1692 | `			return 0;` |
|       - | 1693 | `		}` |
|       - | 1694 | `		/* Object callable — resolve __invoke method */` |
|     ! 0 | 1695 | `		pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 1696 | `			sizeof("__invoke") - 1);` |
|     ! 0 | 1697 | `		if( pMethod == 0 ){` |
|     ! 0 | 1698 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1699 | `				"Fiber callable object has no __invoke method");` |
|     ! 0 | 1700 | `			return 0;` |
|       - | 1701 | `		}` |
|     ! 0 | 1702 | `		*ppThis = pClosure;` |
|     ! 0 | 1703 | `		return &pMethod->sFunc;` |
|       - | 1704 | `	}` |
|     137 | 1705 | `}` |
|       - | 1706 | `/*` |
|       - | 1707 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|       - | 1708 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|       - | 1709 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|       - | 1710 | ` */` |
|       - | 1711 | `/*` |
|       - | 1712 | ` * Enforce one formal parameter's declared type on an argument being installed.` |
|       - | 1713 | ` * THE single implementation of the per-argument check, shared by the` |
|       - | 1714 | ` * generator/fiber initial-frame binder below (band A #2) and both OP_CALL` |
|       - | 1715 | ` * install paths (named-map and positional — they carried two verbatim copies` |
|       - | 1716 | ` * until the §7.1(f) fold): union types via VmCoerceToUnion, class and` |
|       - | 1717 | ` * pseudo types (VmCheckPseudoType + VmResolveTypeClass + instanceof, so` |
|       - | 1718 | ``  * interfaces/abstract classes and self/parent resolve), the bare `object` `` |
|       - | 1719 | ` * hint, and scalars via VmEnforceScalarType (weak-mode coercion in place,` |
|       - | 1720 | ` * strict rejection otherwise) — with the VM_FUNC_ARG_NULLABLE guard letting` |
|       - | 1721 | `` * null through for `?type` and implicit-nullable `Type $x = null` params,`` |
|       - | 1722 | ` * and whole-real materialization on a mask match.` |
|       - | 1723 | ` * Returns SXRET_OK (value possibly coerced) or the VmThrowTypeErrorForArg` |
|       - | 1724 | ` * status for the caller to route — normalized so an INLINE-caught throw` |
|       - | 1725 | ` * (VmThrowInline records only a pc-redirect and reports SXRET_OK) still` |
|       - | 1726 | ` * comes back as PH7_EXCEPTION: the binding must stop; the OP_CALL sites` |
|       - | 1727 | ` * force rc = PH7_EXCEPTION on any non-OK status anyway, and the generator` |
|       - | 1728 | ` * block's PH7_INLINE_RESUME_BREAK consumes the redirect.` |
|       - | 1729 | ` */` |
|     304 | 1730 | `static sxi32 VmGenArgThrowStatus(ph7_vm *pVm, sxi32 rcThrow)` |
|       5 | 1731 | `{` |
|     309 | 1732 | `	if( rcThrow == SXRET_OK && pVm->pInlineInstr ){` |
|     ! 0 | 1733 | `		return PH7_EXCEPTION;` |
|       - | 1734 | `	}` |
|     309 | 1735 | `	return rcThrow;` |
|     157 | 1736 | `}` |
|  235900 | 1737 | `PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_vm_func_arg *pFormal,` |
|       - | 1738 | `	sxu32 nArgPos, ph7_value *pVal, int bStrict, ph7_class *pSelfHint)` |
|       5 | 1739 | `{` |
|  235905 | 1740 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|     285 | 1741 | `		if( VmCoerceToUnion(pVm,pVal,&pFormal->aUnionAlts,` |
|     290 | 1742 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,bStrict,pSelfHint) != SXRET_OK ){` |
|       - | 1743 | `			const char *zGiven;` |
|      80 | 1744 | `			const char *zExpected = "union";` |
|       - | 1745 | `			char zBuf[128];` |
|       - | 1746 | `			char zTypeBuf[128];` |
|      80 | 1747 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      50 | 1748 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      57 | 1749 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      10 | 1750 | `				zGiven = "null";` |
|       6 | 1751 | `			}else{` |
|      24 | 1752 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|       - | 1753 | `			}` |
|      80 | 1754 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|     118 | 1755 | `				zExpected = VmHintTextResolved(pVm,&pFormal->sTypeName,pSelfHint,` |
|      38 | 1756 | `					zTypeBuf,sizeof(zTypeBuf));` |
|      38 | 1757 | `			}` |
|     118 | 1758 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      38 | 1759 | `				&pFormal->sName,zExpected,zGiven));` |
|       - | 1760 | `		}` |
|     119 | 1761 | `		return SXRET_OK;` |
|       - | 1762 | `	}` |
|  235710 | 1763 | `	if( pFormal->nType == 0` |
|  127018 | 1764 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|  218293 | 1765 | `		return SXRET_OK;` |
|       - | 1766 | `	}` |
|   17427 | 1767 | `	if( pFormal->nType == SXU32_HIGH ){` |
|       - | 1768 | `		/* Class or pseudo type */` |
|    2157 | 1769 | `		SyString *pName = &pFormal->sClass;` |
|       - | 1770 | `		ph7_class *pClass;` |
|    2157 | 1771 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|    2157 | 1772 | `		if( rcPseudo == 0 ){` |
|       - | 1773 | `			char zTypeBuf[128],zGivenBuf[128];` |
|     131 | 1774 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      32 | 1775 | `				&pFormal->sName,` |
|      64 | 1776 | `				VmClassHintTypeName(pName,0,` |
|      64 | 1777 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      32 | 1778 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1779 | `		}` |
|    2093 | 1780 | `		pClass = 0;` |
|    2093 | 1781 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|       - | 1782 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      89 | 1783 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      21 | 1784 | `				&pFormal->sName,` |
|      42 | 1785 | `				VmClassHintTypeName(pName,pClass,` |
|      42 | 1786 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      21 | 1787 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1788 | `		}` |
|    2051 | 1789 | `		return SXRET_OK;` |
|       - | 1790 | `	}` |
|   15275 | 1791 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|       - | 1792 | `		char zGivenBuf[128];` |
|     213 | 1793 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|      77 | 1794 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       8 | 1795 | `				&pFormal->sName,"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1796 | `		}` |
|     197 | 1797 | `		if( VmEnforceScalarType(pVal,pFormal->nType,bStrict) != SXRET_OK ){` |
|       - | 1798 | `			char zTypeBuf[128];` |
|     158 | 1799 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      51 | 1800 | `				&pFormal->sName,` |
|      51 | 1801 | `				VmScalarTypeName(pFormal->nType,&pFormal->sTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      51 | 1802 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1803 | `		}` |
|      50 | 1804 | `	}else{` |
|       - | 1805 | `		/* Mask matched — an int param accepting a whole-real materializes` |
|       - | 1806 | `		 * it (php: g(1.0) into int $x is int(1)). */` |
|   15067 | 1807 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|       - | 1808 | `	}` |
|   15157 | 1809 | `	return SXRET_OK;` |
|  118379 | 1810 | `}` |
|       - | 1811 | `/*` |
|       - | 1812 | ` * Record a caller slot this body's frame now ALIASES through a by-reference` |
|       - | 1813 | ` * parameter. The body outlives its caller, so the two frames cannot each own the` |
|       - | 1814 | ` * slot: the caller's teardown counts this frame's name binding as a holder and` |
|       - | 1815 | ` * leaves the value standing, and VmReleaseExecCtx asks PH7_VmReleaseUnheldSlot for` |
|       - | 1816 | ` * every row here once its own names are gone — whichever dies last frees it.` |
|       - | 1817 | ` */` |
|      32 | 1818 | `static void VmCtxAliasByRefArg(ph7_exec_ctx *pExecCtx,sxu32 nIdx)` |
|       2 | 1819 | `{` |
|      34 | 1820 | `	sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pExecCtx->aByRefArg);` |
|       - | 1821 | `	sxu32 n;` |
|      34 | 1822 | `	for( n = 0 ; n < SySetUsed(&pExecCtx->aByRefArg) ; ++n ){` |
|     ! 0 | 1823 | `		if( aIdx[n] == nIdx ){` |
|       - | 1824 | ``			/* Two parameters over one actual (`g($x,$x)`) is ONE slot to give back. */`` |
|     ! 0 | 1825 | `			return;` |
|       - | 1826 | `		}` |
|     ! 0 | 1827 | `	}` |
|      34 | 1828 | `	SySetPut(&pExecCtx->aByRefArg,(const void *)&nIdx);` |
|      18 | 1829 | `}` |
|     672 | 1830 | `PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|       - | 1831 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg,` |
|       - | 1832 | `	int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg, int bAliasByRef)` |
|       5 | 1833 | `{` |
|     677 | 1834 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|       - | 1835 | `	ph7_vm_func_arg *aFormalArg;` |
|       - | 1836 | `	sxu32 nFormal, n;` |
|     677 | 1837 | `	sxu32 nReqGF = 0, nNonVarGF = 0; /* too-few-args watermark (0 = exempt) */` |
|       - | 1838 | `	VmSlot sSlot;` |
|       - | 1839 | `	sxi32 rc;` |
|       - | 1840 | `	/* Install $this for closure/method callables */` |
|     677 | 1841 | `	if( pClosureThis ){` |
|       - | 1842 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      18 | 1843 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      18 | 1844 | `		if( pObj ){` |
|      18 | 1845 | `			pObj->x.pOther = pClosureThis;` |
|      18 | 1846 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      18 | 1847 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|       7 | 1848 | `		}` |
|       7 | 1849 | `	}` |
|       - | 1850 | `	/* Install static variables */` |
|     677 | 1851 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|       - | 1852 | `		ph7_vm_func_static_var *aStatic;` |
|       - | 1853 | `		ph7_value *pVal;` |
|     ! 0 | 1854 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|     ! 0 | 1855 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|     ! 0 | 1856 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|     ! 0 | 1857 | `			if( pVal ){` |
|     ! 0 | 1858 | `				sSlot.pUserData = 0;` |
|     ! 0 | 1859 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|     ! 0 | 1860 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|     ! 0 | 1861 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|     ! 0 | 1862 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|     ! 0 | 1863 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);` |
|     ! 0 | 1864 | `				}` |
|     ! 0 | 1865 | `			}` |
|     ! 0 | 1866 | `		}` |
|     ! 0 | 1867 | `	}` |
|       - | 1868 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|     677 | 1869 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     677 | 1870 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       - | 1871 | `	/* Actual call arity for func_num_args()/func_get_args() (band A #4) */` |
|     677 | 1872 | `	pExecCtx->pFrame->nActualArgs = nArg;` |
|       - | 1873 | `	{` |
|       - | 1874 | `		/* Too-few-arguments watermark — checked per formal INSIDE the install` |
|       - | 1875 | `		 * loop below, after the passed args' type checks, matching php's` |
|       - | 1876 | `		 * RECV order (a type error on a passed argument beats the count` |
|       - | 1877 | `		 * error). Generators throw at the g(...) call site (message embeds` |
|       - | 1878 | `		 * it); fibers at Fiber::start() (php omits the call-site segment).` |
|       - | 1879 | `		 * Prelude builtins are included — VmThrowBuiltinTooFewArgs words them` |
|       - | 1880 | `		 * as php words an internal callable. */` |
|     677 | 1881 | `	nReqGF = VmFuncRequiredArgCount(pFunc,&nNonVarGF);` |
|       - | 1882 | `	}` |
|     799 | 1883 | `	for( n = 0; n < nFormal; n++ ){` |
|       - | 1884 | `		ph7_value *pObj;` |
|     153 | 1885 | `		if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       - | 1886 | `			/* Variadic formal: collect this and every remaining actual into a` |
|       - | 1887 | `			 * fresh array (php semantics — pre-fix nothing collected here, so a` |
|       - | 1888 | ``			 * `function g(int ...$xs)` generator saw a bare scalar in $xs).`` |
|       - | 1889 | `			 * Per-element checks mirror OP_CALL's variadic install: union via` |
|       - | 1890 | ``			 * the shared helper, scalar coerce/TypeError, `object` hint; a`` |
|       - | 1891 | `			 * class-typed variadic element is (like OP_CALL) not checked. */` |
|       7 | 1892 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       7 | 1893 | `			if( pObj ){` |
|       - | 1894 | `				sxu32 nVariadicIdx;` |
|       - | 1895 | `				ph7_hashmap *pMap;` |
|       - | 1896 | `				sxu32 k;` |
|       7 | 1897 | `				PH7_MemObjToHashmap(pObj);` |
|       - | 1898 | `				/* Capture the slot index now: PH7_HashmapInsert can reallocate` |
|       - | 1899 | `				 * pVm->aMemObj and dangle pObj (same hazard as OP_CALL's path). */` |
|       7 | 1900 | `				nVariadicIdx = pObj->nIdx;` |
|       7 | 1901 | `				pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      15 | 1902 | `				for( k = n; k < (sxu32)nArg; k++ ){` |
|      11 | 1903 | `					if( ((aFormalArg[n].iFlags & VM_FUNC_ARG_UNION)` |
|       9 | 1904 | `					   \|\| (aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH)) ){` |
|       7 | 1905 | `						rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[k],bStrict,pSelfHint);` |
|       7 | 1906 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1907 | `							return rc;` |
|       - | 1908 | `						}` |
|       3 | 1909 | `					}` |
|       8 | 1910 | `					if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)` |
|       6 | 1911 | `					 && apArg[k]->nIdx != SXU32_HIGH ){` |
|       - | 1912 | `						/* A by-ref variadic tail aliases its actuals here too — the` |
|       - | 1913 | `						 * ordinary call's rule, one container over. */` |
|       3 | 1914 | `						VmCtxAliasByRefArg(pExecCtx,apArg[k]->nIdx);` |
|       3 | 1915 | `						PH7_HashmapInsertByRef(pMap,0,apArg[k]->nIdx);` |
|       2 | 1916 | `					}else{` |
|       7 | 1917 | `						PH7_HashmapInsert(pMap,0,apArg[k]);` |
|       - | 1918 | `					}` |
|       5 | 1919 | `				}` |
|       7 | 1920 | `				sSlot.nIdx = nVariadicIdx;` |
|       7 | 1921 | `				sSlot.pUserData = 0;` |
|       7 | 1922 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       3 | 1923 | `			}` |
|       7 | 1924 | `			break; /* All remaining actuals consumed */` |
|       - | 1925 | `		}` |
|     147 | 1926 | `		if( n < (sxu32)nArg ){` |
|       - | 1927 | `			/* Argument provided — install with declared-type enforcement.` |
|       - | 1928 | `			 * php binds and type-checks generator arguments EAGERLY at the` |
|       - | 1929 | `			 * g(...) call site (and fiber arguments at Fiber::start()), so the` |
|       - | 1930 | `			 * enforcement lives here, mirroring the OP_CALL install path via` |
|       - | 1931 | `			 * VmEnforceArgType (TypeError on mismatch, weak coercion in` |
|       - | 1932 | `			 * place otherwise) instead of the old silent xCast. A variadic` |
|       - | 1933 | `			 * formal collects as-is (no per-element declared-type model). */` |
|     128 | 1934 | `			if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)` |
|      79 | 1935 | `			 && apArg[n]->nIdx != SXU32_HIGH ){` |
|       - | 1936 | `				/* php binds a generator's by-REFERENCE parameter to the CALLER's slot at` |
|       - | 1937 | `				 * the g(...) that builds the Generator, so the body's write reaches the` |
|       - | 1938 | `				 * caller's variable whenever it eventually runs. Copying it left the` |
|       - | 1939 | `				 * actual untouched for every resume. The type check runs on the actual,` |
|       - | 1940 | `				 * as OP_CALL's by-ref binder does, and never on a copy the alias` |
|       - | 1941 | `				 * replaces. Fiber::start() and the embedder entry pass by VALUE (php's` |
|       - | 1942 | `				 * own decision at those two boundaries), hence bAliasByRef. */` |
|      32 | 1943 | `				sxi32 iPreFlags = apArg[n]->iFlags;` |
|      32 | 1944 | `				rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[n],bStrict,pSelfHint);` |
|      32 | 1945 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 1946 | `					return rc;` |
|       - | 1947 | `				}` |
|       - | 1948 | `				/* A declared type's conversion is what the reference holds (the ordinary` |
|       - | 1949 | `				 * call's rule; the check ran on the operand-stack copy). */` |
|      32 | 1950 | `				PH7_VmByRefArgWriteBack(pVm,apArg[n],iPreFlags);` |
|      47 | 1951 | `				PH7_VmBindVarSlot(pVm,pExecCtx->pFrame,` |
|      30 | 1952 | `					SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName),` |
|      30 | 1953 | `					apArg[n]->nIdx);` |
|      32 | 1954 | `				VmCtxAliasByRefArg(pExecCtx,apArg[n]->nIdx);` |
|      32 | 1955 | `				sSlot.nIdx = apArg[n]->nIdx;` |
|      32 | 1956 | `				sSlot.pUserData = 0;` |
|      32 | 1957 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      32 | 1958 | `				continue;` |
|       - | 1959 | `			}` |
|     103 | 1960 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|     103 | 1961 | `			if( pObj ){` |
|     103 | 1962 | `				PH7_MemObjStore(apArg[n], pObj);` |
|     103 | 1963 | `				if( (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|     103 | 1964 | `					rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,pObj,bStrict,pSelfHint);` |
|     103 | 1965 | `					if( rc != SXRET_OK ){` |
|      18 | 1966 | `						return rc;` |
|       - | 1967 | `					}` |
|      41 | 1968 | `				}` |
|      87 | 1969 | `				sSlot.nIdx = pObj->nIdx;` |
|      87 | 1970 | `				sSlot.pUserData = 0;` |
|      87 | 1971 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      46 | 1972 | `			}` |
|      58 | 1973 | `		}else if( n < nReqGF ){` |
|       - | 1974 | `			/* Required formal with no actual: php's ArgumentCountError, at` |
|       - | 1975 | `			 * this point in the install order (see the watermark comment). */` |
|       7 | 1976 | `			return VmGenArgThrowStatus(pVm,` |
|       4 | 1977 | `				(pFunc->iFlags & VM_FUNC_INTERNAL)` |
|     ! 0 | 1978 | `					? VmThrowBuiltinTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|     ! 0 | 1979 | `						(sxu32)nArg,nReqGF,SySetUsed(&pFunc->aArgs))` |
|       6 | 1980 | `					: VmThrowTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|       2 | 1981 | `						(sxu32)nArg,nReqGF,nNonVarGF,bCallSiteInMsg));` |
|      13 | 1982 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       - | 1983 | `			/* Default value */` |
|      13 | 1984 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      13 | 1985 | `			if( pObj ){` |
|      13 | 1986 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);` |
|      13 | 1987 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1988 | `					return rc;` |
|       - | 1989 | `				}` |
|       - | 1990 | `` 				/* A null default on an implicitly-nullable `Type $x = null` `` |
|       - | 1991 | `				 * param must stay null (php); only non-null defaults keep the` |
|       - | 1992 | `				 * legacy shaping cast. */` |
|      10 | 1993 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       7 | 1994 | `				 && !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|       3 | 1995 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|     ! 0 | 1996 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|     ! 0 | 1997 | `						if( xCast ){` |
|     ! 0 | 1998 | `							xCast(pObj);` |
|     ! 0 | 1999 | `						}` |
|     ! 0 | 2000 | `					}else{` |
|       - | 2001 | `						/* Mask matched — a const-indirected whole-real default` |
|       - | 2002 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|       3 | 2003 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|       - | 2004 | `					}` |
|       1 | 2005 | `				}` |
|      13 | 2006 | `				sSlot.nIdx = pObj->nIdx;` |
|      13 | 2007 | `				sSlot.pUserData = 0;` |
|      13 | 2008 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       5 | 2009 | `			}` |
|       5 | 2010 | `		}` |
|      51 | 2011 | `	}` |
|       - | 2012 | `	/* Install closure environment (captured variables) */` |
|     657 | 2013 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 2014 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|       - | 2015 | `		ph7_value *pValue;` |
|       - | 2016 | `		sxu32 iEnv;` |
|      53 | 2017 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     113 | 2018 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|      65 | 2019 | `			pEnv = &aEnv[iEnv];` |
|      65 | 2020 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|      44 | 2021 | `				continue;` |
|       - | 2022 | `			}` |
|      20 | 2023 | `			if( pClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       5 | 2024 | `			 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|       - | 2025 | `				/* An explicit bound $this (bindTo/bind/call) wins over the` |
|       - | 2026 | `				 * creation-time captured $this, php-exact (mirrors OP_CALL). */` |
|       3 | 2027 | `				continue;` |
|       - | 2028 | `			}` |
|      21 | 2029 | `			if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|       - | 2030 | `				/* Captured by reference: link the name to the shared slot` |
|       - | 2031 | `				 * (no copy), mirroring the OP_CALL env install. */` |
|       5 | 2032 | `				if( SyHashGet(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|       7 | 2033 | `					SyHashInsert(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),` |
|       4 | 2034 | `						SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|       2 | 2035 | `				}` |
|       5 | 2036 | `				continue;` |
|       - | 2037 | `			}` |
|      17 | 2038 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|      17 | 2039 | `			if( pValue == 0 ){` |
|     ! 0 | 2040 | `				continue;` |
|       - | 2041 | `			}` |
|      17 | 2042 | `			PH7_MemObjRelease(pValue);` |
|      17 | 2043 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|      10 | 2044 | `		}` |
|      24 | 2045 | `	}` |
|     657 | 2046 | `	return SXRET_OK;` |
|     341 | 2047 | `}` |
|       - | 2048 | `/*` |
|       - | 2049 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|       - | 2050 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|       - | 2051 | ` *` |
|       - | 2052 | ` * As a native VARIADIC method the arguments arrive directly as (nArg, apArg);` |
|       - | 2053 | ` * the prelude used to hand them over as a single func_get_args() array, which` |
|       - | 2054 | ` * this had to walk and snapshot out of pVm->aMemObj.` |
|       - | 2055 | ` */` |
|     266 | 2056 | `PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2057 | `{` |
|     271 | 2058 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2059 | `	ph7_class_instance *pThis;` |
|       - | 2060 | `	ph7_class_instance *pClosureThis;` |
|       - | 2061 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 2062 | `	ph7_vm_func *pFunc;` |
|       - | 2063 | `	ph7_value sResult;` |
|       - | 2064 | `	ph7_value *pCtxAttr;` |
|       - | 2065 | `	SyString sAttrName;` |
|       - | 2066 | `	sxi32 rc;` |
|     271 | 2067 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     271 | 2068 | `	if( pRecv == 0 \|\| (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2069 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|       - | 2070 | `	}` |
|     271 | 2071 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 2072 | `	/* Check if already started (has a __ctx) */` |
|     271 | 2073 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     271 | 2074 | `	if( pExecCtx != 0 ){` |
|       3 | 2075 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2076 | `			"Cannot start a fiber that has already been started");` |
|       - | 2077 | `	}` |
|       - | 2078 | `	/* Resolve callable */` |
|     269 | 2079 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|     269 | 2080 | `	if( pFunc == 0 ){` |
|     ! 0 | 2081 | `		return PH7_EXCEPTION;` |
|       - | 2082 | `	}` |
|       - | 2083 | ``	/* Fiber::start()'s own `...$args` are by VALUE whatever the body declares, so php`` |
|       - | 2084 | `		 * warns for every by-reference parameter and the body operates on a copy — the` |
|       - | 2085 | `		 * value PHL already produced, without the one diagnostic that says so. Named off` |
|       - | 2086 | `		 * the stored callable, which is what carries the class for a method one. */` |
|     269 | 2087 | `	if( nArg > 0 ){` |
|       - | 2088 | `		SyString sCbName;` |
|       - | 2089 | `		ph7_value *pCbVal;` |
|      12 | 2090 | `		SyStringInitFromBuf(&sCbName, "__callable", 10);` |
|      12 | 2091 | `		pCbVal = PH7_ClassInstanceFetchAttr(pThis, &sCbName);` |
|      12 | 2092 | `		if( pCbVal ){` |
|      12 | 2093 | `			PH7_VmWarnByRefArgsGivenValue(pVm, pCbVal, nArg, 0, 0);` |
|       5 | 2094 | `		}` |
|       5 | 2095 | `	}` |
|       - | 2096 | `	/* Create execution context now that we know the function */` |
|     269 | 2097 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|     269 | 2098 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 2099 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2100 | `			"Fiber::start(): out of memory");` |
|       - | 2101 | `	}` |
|       - | 2102 | `	/* Store context in $this->__ctx */` |
|     269 | 2103 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     269 | 2104 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     269 | 2105 | `	if( pCtxAttr ){` |
|     269 | 2106 | `		pCtxAttr->x.pOther = pExecCtx;` |
|     269 | 2107 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     132 | 2108 | `	}` |
|       - | 2109 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|       - | 2110 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|       - | 2111 | `	 * into the fiber's frame, not the caller's. */` |
|     269 | 2112 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|     269 | 2113 | `	pVm->pFrame = pExecCtx->pFrame;` |
|       - | 2114 | `	/* Unpack the args array and install into the frame */` |
|       - | 2115 | `	{` |
|       - | 2116 | `		/* The arguments are this call's own operand-stack slots, so they can be` |
|       - | 2117 | `		 * handed to the frame setup as-is. The old form had to snapshot them out of` |
|       - | 2118 | `		 * pVm->aMemObj first, because they arrived as a func_get_args() hashmap` |
|       - | 2119 | `		 * whose element values live in that set — and VmFiberSetupFrame reserves` |
|       - | 2120 | `		 * memory objects (VmExtractMemObj) before reading its arguments, which can` |
|       - | 2121 | `		 * reallocate the set and dangle a raw pool pointer. Operand slots do not` |
|       - | 2122 | `		 * move, so the copy is gone with the array that made it necessary. */` |
|     269 | 2123 | `		ph7_value **apValues = (nArg > 0) ? apArg : 0;` |
|     269 | 2124 | `		int nActual = nArg;` |
|     269 | 2125 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues,` |
|       - | 2126 | `			0 /* weak-mode arg binding, like call_user_func */, 0,` |
|       - | 2127 | `			FALSE/*Fiber::start(): php omits the call-site segment*/,` |
|       - | 2128 | `			FALSE/*php's Fiber::start() passes by VALUE and warns (§7.1)*/);` |
|       - | 2129 | `		/* Nothing to free: apValues aliases the operand stack now, it is not a` |
|       - | 2130 | `		 * buffer this function allocated. */` |
|       - | 2131 | `	}` |
|       - | 2132 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|     269 | 2133 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|     269 | 2134 | `	pExecCtx->pFrame->pParent = 0;` |
|     269 | 2135 | `	if( rc != SXRET_OK ){` |
|       - | 2136 | `		/* Propagate the real status: a declared-type TypeError from the arg` |
|       - | 2137 | `		 * install (band A #2) must stay catchable, not become an abort. */` |
|       5 | 2138 | `		return (rc == PH7_EXCEPTION) ? PH7_EXCEPTION : PH7_ABORT;` |
|       - | 2139 | `	}` |
|     265 | 2140 | `	PH7_MemObjInit(pVm, &sResult);` |
|     265 | 2141 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|     265 | 2142 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 2143 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 2144 | `		return PH7_ABORT;` |
|       - | 2145 | `	}` |
|     265 | 2146 | `	if( rc == PH7_EXCEPTION ){` |
|       6 | 2147 | `		PH7_MemObjRelease(&sResult);` |
|       6 | 2148 | `		return PH7_EXCEPTION;` |
|       - | 2149 | `	}` |
|     261 | 2150 | `	ph7_result_value(pCtx, &sResult);` |
|     261 | 2151 | `	PH7_MemObjRelease(&sResult);` |
|     261 | 2152 | `	return PH7_OK;` |
|     138 | 2153 | `}` |
|       - | 2154 | `/*` |
|       - | 2155 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|       - | 2156 | ` */` |
|     148 | 2157 | `PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2158 | `{` |
|     153 | 2159 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2160 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 2161 | `	ph7_value sResult;` |
|       - | 2162 | `	ph7_value *pResumeVal;` |
|       - | 2163 | `	sxi32 rc;` |
|     153 | 2164 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     153 | 2165 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|     153 | 2166 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2167 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|     ! 0 | 2168 | `		return PH7_OK;` |
|       - | 2169 | `	}` |
|     153 | 2170 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     153 | 2171 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 2172 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|     ! 0 | 2173 | `		return PH7_OK;` |
|       - | 2174 | `	}` |
|     153 | 2175 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       3 | 2176 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2177 | `			"Cannot resume a fiber that is not suspended");` |
|       - | 2178 | `	}` |
|     151 | 2179 | `	pResumeVal = (nArg > 0) ? apArg[0] : 0;` |
|     151 | 2180 | `	PH7_MemObjInit(pVm, &sResult);` |
|     151 | 2181 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|     151 | 2182 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 2183 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 2184 | `		return PH7_ABORT;` |
|       - | 2185 | `	}` |
|     151 | 2186 | `	if( rc == PH7_EXCEPTION ){` |
|       3 | 2187 | `		PH7_MemObjRelease(&sResult);` |
|       3 | 2188 | `		return PH7_EXCEPTION;` |
|       - | 2189 | `	}` |
|     149 | 2190 | `	ph7_result_value(pCtx, &sResult);` |
|     149 | 2191 | `	PH7_MemObjRelease(&sResult);` |
|     149 | 2192 | `	return PH7_OK;` |
|      79 | 2193 | `}` |
|       - | 2194 | `/*` |
|       - | 2195 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|       - | 2196 | ` */` |
|      20 | 2197 | `PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2198 | `{` |
|      25 | 2199 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2200 | `	ph7_exec_ctx *pExecCtx;` |
|      25 | 2201 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      10 | 2202 | `	SXUNUSED(apArg);` |
|      10 | 2203 | `	SXUNUSED(nArg);` |
|      25 | 2204 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      25 | 2205 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2206 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2207 | `		return PH7_OK;` |
|       - | 2208 | `	}` |
|      25 | 2209 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|      25 | 2210 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 2211 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2212 | `		return PH7_OK;` |
|       - | 2213 | `	}` |
|      25 | 2214 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|     ! 0 | 2215 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2216 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2217 | `				"Cannot get fiber return value: The fiber has not been started");` |
|       - | 2218 | `		}` |
|     ! 0 | 2219 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2220 | `			"Cannot get fiber return value: The fiber has not returned");` |
|       - | 2221 | `	}` |
|      25 | 2222 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|      25 | 2223 | `	return PH7_OK;` |
|      15 | 2224 | `}` |
|       - | 2225 | `/*` |
|       - | 2226 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|       - | 2227 | ` */` |
|       8 | 2228 | `PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 2229 | `{` |
|       - | 2230 | `	ph7_exec_ctx *pExecCtx;` |
|      10 | 2231 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       4 | 2232 | `	SXUNUSED(apArg);` |
|       4 | 2233 | `	SXUNUSED(nArg);` |
|      10 | 2234 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      10 | 2235 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|      10 | 2236 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|      10 | 2237 | `	return PH7_OK;` |
|       6 | 2238 | `}` |
|     ! 0 | 2239 | `PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     ! 0 | 2240 | `{` |
|       - | 2241 | `	ph7_exec_ctx *pExecCtx;` |
|     ! 0 | 2242 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     ! 0 | 2243 | `	SXUNUSED(apArg);` |
|     ! 0 | 2244 | `	SXUNUSED(nArg);` |
|     ! 0 | 2245 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     ! 0 | 2246 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|     ! 0 | 2247 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|     ! 0 | 2248 | `	return PH7_OK;` |
|     ! 0 | 2249 | `}` |
|     110 | 2250 | `PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 2251 | `{` |
|       - | 2252 | `	ph7_exec_ctx *pExecCtx;` |
|     113 | 2253 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      55 | 2254 | `	SXUNUSED(apArg);` |
|      55 | 2255 | `	SXUNUSED(nArg);` |
|     113 | 2256 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     113 | 2257 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|     113 | 2258 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|     113 | 2259 | `	return PH7_OK;` |
|      58 | 2260 | `}` |
|      10 | 2261 | `PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 2262 | `{` |
|       - | 2263 | `	ph7_exec_ctx *pExecCtx;` |
|      12 | 2264 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       5 | 2265 | `	SXUNUSED(apArg);` |
|       5 | 2266 | `	SXUNUSED(nArg);` |
|      12 | 2267 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      12 | 2268 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|      12 | 2269 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|      12 | 2270 | `	return PH7_OK;` |
|       7 | 2271 | `}` |
|       - | 2272 | `/*` |
|       - | 2273 | ` * Fiber->__destruct() — clean up the execution context.` |
|       - | 2274 | ` */` |
|     226 | 2275 | `PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 2276 | `{` |
|     229 | 2277 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2278 | `	ph7_exec_ctx *pExecCtx;` |
|     229 | 2279 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     113 | 2280 | `	SXUNUSED(apArg);` |
|     113 | 2281 | `	SXUNUSED(nArg);` |
|     229 | 2282 | `	if( pRecv == 0 ){` |
|     ! 0 | 2283 | `		return PH7_OK;` |
|       - | 2284 | `	}` |
|     229 | 2285 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     229 | 2286 | `	if( pExecCtx ){` |
|     225 | 2287 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|       - | 2288 | `		/* Clear the attribute so double-free is prevented */` |
|     225 | 2289 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|     225 | 2290 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 2291 | `			SyString sAttrName;` |
|       - | 2292 | `			ph7_value *pAttr;` |
|     225 | 2293 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     225 | 2294 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     225 | 2295 | `			if( pAttr ){` |
|     225 | 2296 | `				PH7_MemObjRelease(pAttr);` |
|     111 | 2297 | `			}` |
|     111 | 2298 | `		}` |
|     111 | 2299 | `	}` |
|     229 | 2300 | `	return PH7_OK;` |
|     116 | 2301 | `}` |
|       - | 2302 | `/* ======================== Fiber Public API Helpers ======================== */` |
|     ! 0 | 2303 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|     ! 0 | 2304 | `{` |
|       - | 2305 | `	ph7_class_instance *pThis;` |
|     ! 0 | 2306 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|     ! 0 | 2307 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 | 2308 | `	return pThis->pClass == pVm->pFiberClass;` |
|     ! 0 | 2309 | `}` |
|     ! 0 | 2310 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 2311 | `{` |
|       - | 2312 | `	ph7_class_instance *pThis;` |
|     ! 0 | 2313 | `	ph7_class_instance *pClosureThis = 0;` |
|       - | 2314 | `	ph7_exec_ctx *pCtx;` |
|       - | 2315 | `	ph7_vm_func *pFunc;` |
|       - | 2316 | `	ph7_value *pCallable;` |
|       - | 2317 | `	ph7_value *pCtxAttr;` |
|       - | 2318 | `	SyString sAttrName;` |
|       - | 2319 | `	sxi32 rc;` |
|       - | 2320 | `	/* Must not already be started */` |
|     ! 0 | 2321 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2322 | `	if( pCtx != 0 ){` |
|     ! 0 | 2323 | `		return SXERR_INVALID;` |
|       - | 2324 | `	}` |
|     ! 0 | 2325 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2326 | `		return SXERR_INVALID;` |
|       - | 2327 | `	}` |
|     ! 0 | 2328 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|       - | 2329 | `	/* Get the callable */` |
|     ! 0 | 2330 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     ! 0 | 2331 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 2332 | `	if( pCallable == 0 ){` |
|     ! 0 | 2333 | `		return SXERR_INVALID;` |
|       - | 2334 | `	}` |
|       - | 2335 | `	/* Resolve callable */` |
|     ! 0 | 2336 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|       - | 2337 | `		SyString sName;` |
|       - | 2338 | `		SyHashEntry *pEntry;` |
|     ! 0 | 2339 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|     ! 0 | 2340 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|     ! 0 | 2341 | `		if( pEntry == 0 ){` |
|     ! 0 | 2342 | `			return SXERR_NOTFOUND;` |
|       - | 2343 | `		}` |
|     ! 0 | 2344 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     ! 0 | 2345 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2346 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|     ! 0 | 2347 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 2348 | `			sizeof("__invoke") - 1);` |
|     ! 0 | 2349 | `		if( pMethod == 0 ){` |
|     ! 0 | 2350 | `			return SXERR_INVALID;` |
|       - | 2351 | `		}` |
|     ! 0 | 2352 | `		pClosureThis = pClosure;` |
|     ! 0 | 2353 | `		pFunc = &pMethod->sFunc;` |
|     ! 0 | 2354 | `	}else{` |
|     ! 0 | 2355 | `		return SXERR_INVALID;` |
|       - | 2356 | `	}` |
|       - | 2357 | `	/* Create context */` |
|     ! 0 | 2358 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|     ! 0 | 2359 | `	if( pCtx == 0 ){` |
|     ! 0 | 2360 | `		return SXERR_MEM;` |
|       - | 2361 | `	}` |
|       - | 2362 | `	/* Store in __ctx */` |
|     ! 0 | 2363 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     ! 0 | 2364 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 2365 | `	if( pCtxAttr ){` |
|     ! 0 | 2366 | `		pCtxAttr->x.pOther = pCtx;` |
|     ! 0 | 2367 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     ! 0 | 2368 | `	}` |
|       - | 2369 | `	/* Set up frame with args */` |
|     ! 0 | 2370 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     ! 0 | 2371 | `	pVm->pFrame = pCtx->pFrame;` |
|     ! 0 | 2372 | `	rc = VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg,` |
|       - | 2373 | `		0 /* weak-mode arg binding (embedder entry) */, 0,` |
|       - | 2374 | `		FALSE/*embedder entry: no userland call site*/,` |
|       - | 2375 | `		FALSE/*no source-level actuals to alias*/);` |
|     ! 0 | 2376 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|     ! 0 | 2377 | `	pCtx->pFrame->pParent = 0;` |
|     ! 0 | 2378 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2379 | `		return rc;` |
|       - | 2380 | `	}` |
|     ! 0 | 2381 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|     ! 0 | 2382 | `}` |
|     ! 0 | 2383 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 2384 | `{` |
|     ! 0 | 2385 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2386 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|     ! 0 | 2387 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|     ! 0 | 2388 | `}` |
|     ! 0 | 2389 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2390 | `{` |
|     ! 0 | 2391 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2392 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|     ! 0 | 2393 | `}` |
|     ! 0 | 2394 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2395 | `{` |
|     ! 0 | 2396 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2397 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|     ! 0 | 2398 | `}` |
|     ! 0 | 2399 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2400 | `{` |
|     ! 0 | 2401 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2402 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|     ! 0 | 2403 | `	return &pCtx->sRetValue;` |
|     ! 0 | 2404 | `}` |
|       - | 2405 | `/* ======================== Generator Infrastructure ======================== */` |
|       - | 2406 | `/*` |
|       - | 2407 | ` * Allocate a new generator wrapper around an execution context.` |
|       - | 2408 | ` */` |
|     408 | 2409 | `PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 | 2410 | `{` |
|       - | 2411 | `	ph7_generator *pGen;` |
|     413 | 2412 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|     413 | 2413 | `	if( pGen == 0 ){` |
|     ! 0 | 2414 | `		return 0;` |
|       - | 2415 | `	}` |
|     413 | 2416 | `	SyZero(pGen, sizeof(ph7_generator));` |
|     413 | 2417 | `	pGen->pCtx = pCtx;` |
|     413 | 2418 | `	pGen->iImplicitKey = 0;` |
|     413 | 2419 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|     413 | 2420 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|       - | 2421 | `	/* Link the generator back to the exec context */` |
|     413 | 2422 | `	pCtx->pPrivate = pGen;` |
|     413 | 2423 | `	return pGen;` |
|     209 | 2424 | `}` |
|       - | 2425 | `/*` |
|       - | 2426 | ` * Release a generator and its execution context.` |
|       - | 2427 | ` */` |
|     272 | 2428 | `PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|       5 | 2429 | `{` |
|     277 | 2430 | `	if( pGen == 0 ){` |
|     ! 0 | 2431 | `		return;` |
|       - | 2432 | `	}` |
|     277 | 2433 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|     277 | 2434 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|     277 | 2435 | `	if( pGen->pCtx ){` |
|     277 | 2436 | `		pGen->pCtx->pPrivate = 0;` |
|     277 | 2437 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|     277 | 2438 | `		pGen->pCtx = 0;` |
|     136 | 2439 | `	}` |
|     277 | 2440 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|     141 | 2441 | `}` |
|       - | 2442 | `/*` |
|       - | 2443 | ` * Extract ph7_generator from a Generator class instance.` |
|       - | 2444 | ` */` |
|    4324 | 2445 | `PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|       5 | 2446 | `{` |
|       - | 2447 | `	ph7_class_instance *pThis;` |
|       - | 2448 | `	SyString sAttr;` |
|       - | 2449 | `	ph7_value *pAttr;` |
|    4329 | 2450 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2451 | `		return 0;` |
|       - | 2452 | `	}` |
|    4329 | 2453 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|    4329 | 2454 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|     ! 0 | 2455 | `		return 0;` |
|       - | 2456 | `	}` |
|    4329 | 2457 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    4329 | 2458 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    4329 | 2459 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     ! 0 | 2460 | `		return 0;` |
|       - | 2461 | `	}` |
|    4329 | 2462 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    2167 | 2463 | `}` |
|       - | 2464 | `/*` |
|       - | 2465 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|       - | 2466 | ` */` |
|     224 | 2467 | `PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2468 | `{` |
|       - | 2469 | `	ph7_generator *pGen;` |
|       - | 2470 | `	sxi32 rc;` |
|     229 | 2471 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     112 | 2472 | `	SXUNUSED(apArg);` |
|     112 | 2473 | `	SXUNUSED(nArg);` |
|     229 | 2474 | `	if( pRecv == 0 ) return PH7_OK;` |
|     229 | 2475 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     229 | 2476 | `	if( pGen == 0 ) return PH7_OK;` |
|     229 | 2477 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     227 | 2478 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     227 | 2479 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     227 | 2480 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     108 | 2481 | `	}` |
|     223 | 2482 | `	return PH7_OK;` |
|     117 | 2483 | `}` |
|       - | 2484 | `/*` |
|       - | 2485 | ` * Generator::valid() — true if suspended at a yield point.` |
|       - | 2486 | ` */` |
|    1196 | 2487 | `PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2488 | `{` |
|       - | 2489 | `	ph7_generator *pGen;` |
|    1201 | 2490 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     598 | 2491 | `	SXUNUSED(apArg);` |
|     598 | 2492 | `	SXUNUSED(nArg);` |
|    1201 | 2493 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|    1201 | 2494 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1201 | 2495 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|    1201 | 2496 | `	return PH7_OK;` |
|     603 | 2497 | `}` |
|       - | 2498 | `/*` |
|       - | 2499 | ` * Generator::current() — return the last yielded value.` |
|       - | 2500 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 2501 | ` */` |
|    1198 | 2502 | `PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2503 | `{` |
|       - | 2504 | `	ph7_generator *pGen;` |
|       - | 2505 | `	sxi32 rc;` |
|    1203 | 2506 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     599 | 2507 | `	SXUNUSED(apArg);` |
|     599 | 2508 | `	SXUNUSED(nArg);` |
|    1203 | 2509 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1203 | 2510 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1203 | 2511 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1203 | 2512 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     151 | 2513 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     151 | 2514 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     151 | 2515 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      73 | 2516 | `	}` |
|    1203 | 2517 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|    1201 | 2518 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|     603 | 2519 | `	}else{` |
|       3 | 2520 | `		ph7_result_null(pCtx);` |
|       - | 2521 | `	}` |
|    1203 | 2522 | `	return PH7_OK;` |
|     604 | 2523 | `}` |
|       - | 2524 | `/*` |
|       - | 2525 | ` * Generator::key() — return the last yielded key.` |
|       - | 2526 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 2527 | ` */` |
|     206 | 2528 | `PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2529 | `{` |
|       - | 2530 | `	ph7_generator *pGen;` |
|       - | 2531 | `	sxi32 rc;` |
|     211 | 2532 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     103 | 2533 | `	SXUNUSED(apArg);` |
|     103 | 2534 | `	SXUNUSED(nArg);` |
|     211 | 2535 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     211 | 2536 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     211 | 2537 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     211 | 2538 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2539 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     ! 0 | 2540 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     ! 0 | 2541 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     ! 0 | 2542 | `	}` |
|     211 | 2543 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     211 | 2544 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|     108 | 2545 | `	}else{` |
|     ! 0 | 2546 | `		ph7_result_null(pCtx);` |
|       - | 2547 | `	}` |
|     211 | 2548 | `	return PH7_OK;` |
|     108 | 2549 | `}` |
|       - | 2550 | `/*` |
|       - | 2551 | ` * Generator::next() — advance to the next yield point.` |
|       - | 2552 | ` */` |
|     968 | 2553 | `PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2554 | `{` |
|       - | 2555 | `	ph7_generator *pGen;` |
|       - | 2556 | `	sxi32 rc;` |
|     973 | 2557 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     484 | 2558 | `	SXUNUSED(apArg);` |
|     484 | 2559 | `	SXUNUSED(nArg);` |
|     973 | 2560 | `	if( pRecv == 0 ) return PH7_OK;` |
|     973 | 2561 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     973 | 2562 | `	if( pGen == 0 ) return PH7_OK;` |
|     973 | 2563 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2564 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     973 | 2565 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     973 | 2566 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|     489 | 2567 | `	}else{` |
|     ! 0 | 2568 | `		return PH7_OK;` |
|       - | 2569 | `	}` |
|     973 | 2570 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     971 | 2571 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     961 | 2572 | `	return PH7_OK;` |
|     489 | 2573 | `}` |
|       - | 2574 | `/*` |
|       - | 2575 | ` * Generator::send($value) — resume and send a value into the generator.` |
|       - | 2576 | ` */` |
|     102 | 2577 | `PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2578 | `{` |
|       - | 2579 | `	ph7_generator *pGen;` |
|       - | 2580 | `	ph7_value *pSendVal;` |
|       - | 2581 | `	sxi32 rc;` |
|     107 | 2582 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     107 | 2583 | `	if( pRecv == 0 ) return PH7_OK;` |
|     107 | 2584 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     107 | 2585 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     107 | 2586 | `	pSendVal = (nArg > 0) ? apArg[0] : 0;` |
|     107 | 2587 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       - | 2588 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|     ! 0 | 2589 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     107 | 2590 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     107 | 2591 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|      56 | 2592 | `	}else{` |
|     ! 0 | 2593 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2594 | `		return PH7_OK;` |
|       - | 2595 | `	}` |
|     107 | 2596 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     107 | 2597 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     104 | 2598 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      97 | 2599 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      50 | 2600 | `	}else{` |
|       8 | 2601 | `		ph7_result_null(pCtx);` |
|       - | 2602 | `	}` |
|     104 | 2603 | `	return PH7_OK;` |
|      56 | 2604 | `}` |
|       - | 2605 | `/*` |
|       - | 2606 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|       - | 2607 | ` *` |
|       - | 2608 | ` * PHP semantics: the exception is injected AT the suspended yield point so the` |
|       - | 2609 | ` * generator's OWN try/catch (if any wraps the yield) can handle it and the body` |
|       - | 2610 | ` * resumes after the try; otherwise it propagates to the throw() caller and the` |
|       - | 2611 | ` * generator closes. We implement this by resuming the body with a pending` |
|       - | 2612 | ` * injection (pCtx->pInjected) that the VM loop raises in the body's own frame via` |
|       - | 2613 | ` * the existing OP_THROW / ROOT B resume route — no exception frame is` |
|       - | 2614 | ` * reconstructed on pVm->pFrame, so the return/finally unwind path is untouched.` |
|       - | 2615 | ` * A never-started generator is first run to its first yield, then injected there;` |
|       - | 2616 | ` * a finished/closed generator has no suspend point, so the exception is simply` |
|       - | 2617 | ` * propagated to the caller (its return value stays readable via getReturn()).` |
|       - | 2618 | ` *` |
|       - | 2619 | ` * PHL does not enforce the Throwable parameter hint (interface/class hints are` |
|       - | 2620 | ` * not checked on call), so the Throwable validation — and its PHP TypeError —` |
|       - | 2621 | ` * are done here.` |
|       - | 2622 | ` */` |
|      62 | 2623 | `PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 2624 | `{` |
|       - | 2625 | `	ph7_generator *pGen;` |
|       - | 2626 | `	ph7_class_instance *pInj;` |
|       - | 2627 | `	ph7_class *pThrowable;` |
|       - | 2628 | `	VmFrame *pFrame;` |
|       - | 2629 | `	sxi32 rc;` |
|      66 | 2630 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      66 | 2631 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      66 | 2632 | `	if( nArg < 1 ) return PH7_OK;` |
|       - | 2633 | `	/* Argument #1 must be a Throwable; otherwise PHP raises a TypeError naming` |
|       - | 2634 | `	 * the given type (class name for objects, "null"/"string"/... for scalars). */` |
|      66 | 2635 | `	pThrowable = PH7_VmExtractClass(pCtx->pVm, "Throwable", sizeof("Throwable")-1, 0, 0);` |
|      62 | 2636 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|      66 | 2637 | `	 \|\| (pThrowable && !PH7_VmInstanceOf(((ph7_class_instance *)apArg[0]->x.pOther)->pClass, pThrowable)) ){` |
|       - | 2638 | `		char zCls[128];` |
|     ! 0 | 2639 | `		const char *zGiven = VmValueGivenName(apArg[0], zCls, sizeof(zCls));` |
|     ! 0 | 2640 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|     ! 0 | 2641 | `			"Generator::throw(): Argument #1 ($exception) must be of type Throwable, %s given", zGiven);` |
|       - | 2642 | `	}` |
|      66 | 2643 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      66 | 2644 | `	if( pGen == 0 ) return PH7_OK;` |
|       - | 2645 | `	/* PHP forbids resuming/throwing into a generator that is currently executing. */` |
|      66 | 2646 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       3 | 2647 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 2648 | `			"Cannot resume an already running generator");` |
|       - | 2649 | `	}` |
|       - | 2650 | `	/* Hold a reference to the injected instance for the whole operation: the VM loop` |
|       - | 2651 | `	 * (inject path) or VmThrowException (propagate path) may run catch blocks that bind` |
|       - | 2652 | `	 * and later release it. Dropped on every return path below. */` |
|      64 | 2653 | `	pInj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      64 | 2654 | `	pInj->iRef++;` |
|       - | 2655 | `	/* A never-started generator runs to its first yield, then the exception is injected` |
|       - | 2656 | `	 * there (PHP). Start it first; if it suspended at a yield, fall through to inject; if` |
|       - | 2657 | `	 * it ran to completion without yielding, drop through to the propagate path below. */` |
|      64 | 2658 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       5 | 2659 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       5 | 2660 | `		if( rc == PH7_ABORT ){ PH7_ClassInstanceUnref(pInj); return PH7_ABORT; }` |
|       5 | 2661 | `		if( rc == PH7_EXCEPTION ){ PH7_ClassInstanceUnref(pInj); return PH7_EXCEPTION; }` |
|       2 | 2662 | `	}` |
|      64 | 2663 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       - | 2664 | `		/* Inject at the suspended yield: the resume loop raises it in the body's own` |
|       - | 2665 | `		 * frame so the generator's try/catch can catch it and resume (path 2). */` |
|      58 | 2666 | `		pGen->pCtx->pInjected = pInj;   /* borrowed; ref held here across the resume */` |
|      58 | 2667 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       - | 2668 | `		/* Normally the inject was consumed (cleared) at VmByteCodeExec entry; clear it` |
|       - | 2669 | `		 * here too for the path where VmResumeCtx bails BEFORE entering the loop (e.g. the` |
|       - | 2670 | `		 * recursion-depth fatal), so no dangling borrowed pointer survives the Unref. */` |
|      58 | 2671 | `		pGen->pCtx->pInjected = 0;` |
|      58 | 2672 | `		PH7_ClassInstanceUnref(pInj);` |
|      58 | 2673 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      58 | 2674 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       - | 2675 | `		/* Caught inside the generator and it resumed: return the next yielded value (or` |
|       - | 2676 | `		 * null if it then completed) — symmetric with Generator::send(). */` |
|      46 | 2677 | `		if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      43 | 2678 | `			ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      23 | 2679 | `		}else{` |
|       3 | 2680 | `			ph7_result_null(pCtx);` |
|       - | 2681 | `		}` |
|      46 | 2682 | `		return PH7_OK;` |
|       - | 2683 | `	}` |
|       - | 2684 | `	/* COMPLETED/CLOSED (incl. a CREATED generator that ran to completion without a` |
|       - | 2685 | `	 * yield): no suspend point to inject at. Propagate the real object to the throw()` |
|       - | 2686 | `	 * caller through the normal dispatch path (class/message/trace preserved); its` |
|       - | 2687 | `	 * terminal state — and thus getReturn() — is left intact. */` |
|       8 | 2688 | `	pFrame = pCtx->pVm->pFrame;` |
|       8 | 2689 | `	if( pFrame ){` |
|       8 | 2690 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       8 | 2691 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       3 | 2692 | `	}` |
|       8 | 2693 | `	rc = VmThrowException(pCtx->pVm, pInj);` |
|       8 | 2694 | `	PH7_ClassInstanceUnref(pInj);` |
|       8 | 2695 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2696 | `		return PH7_ABORT;` |
|       - | 2697 | `	}` |
|       8 | 2698 | `	return PH7_EXCEPTION;` |
|      35 | 2699 | `}` |
|       - | 2700 | `/*` |
|       - | 2701 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|       - | 2702 | ` */` |
|      24 | 2703 | `PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 2704 | `{` |
|       - | 2705 | `	ph7_generator *pGen;` |
|      28 | 2706 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      12 | 2707 | `	SXUNUSED(apArg);` |
|      12 | 2708 | `	SXUNUSED(nArg);` |
|      28 | 2709 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      28 | 2710 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      28 | 2711 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      28 | 2712 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|     ! 0 | 2713 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 2714 | `			"Cannot get return value of a generator that hasn't returned");` |
|       - | 2715 | `	}` |
|      28 | 2716 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|      28 | 2717 | `	return PH7_OK;` |
|      16 | 2718 | `}` |
|       - | 2719 | `/*` |
|       - | 2720 | ` * Generator::__destruct() — clean up.` |
|       - | 2721 | ` */` |
|     256 | 2722 | `PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2723 | `{` |
|       - | 2724 | `	ph7_generator *pGen;` |
|     261 | 2725 | `	sxi32 rcClose = SXRET_OK;` |
|     261 | 2726 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     128 | 2727 | `	SXUNUSED(apArg);` |
|     128 | 2728 | `	SXUNUSED(nArg);` |
|     261 | 2729 | `	if( pRecv == 0 ) return PH7_OK;` |
|     261 | 2730 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     261 | 2731 | `	if( pGen ){` |
|       - | 2732 | `` 		/* A generator abandoned before it completes still runs its pending `finally` `` |
|       - | 2733 | `		 * blocks (PHP runs them at generator close/GC). Drive them before teardown. */` |
|     261 | 2734 | `		if( pGen->pCtx ){` |
|     261 | 2735 | `			rcClose = VmCloseCtx(pCtx->pVm, pGen->pCtx);` |
|     128 | 2736 | `		}` |
|     261 | 2737 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|     261 | 2738 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|     261 | 2739 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 2740 | `			SyString sAttrName;` |
|       - | 2741 | `			ph7_value *pAttr;` |
|     261 | 2742 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     261 | 2743 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     261 | 2744 | `			if( pAttr ){` |
|     261 | 2745 | `				PH7_MemObjRelease(pAttr);` |
|     128 | 2746 | `			}` |
|     128 | 2747 | `		}` |
|     128 | 2748 | `	}` |
|       - | 2749 | `	/* Surface an abort/exception raised by a finally that ran during close. */` |
|     261 | 2750 | `	if( rcClose == PH7_ABORT ) return PH7_ABORT;` |
|     261 | 2751 | `	if( rcClose == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     261 | 2752 | `	return PH7_OK;` |
|     133 | 2753 | `}` |
|       - | 2754 | `/* ======================== End Generator Infrastructure ======================== */` |
|       - | 2755 | `/* ======================== End Fiber Infrastructure ======================== */` |
|       - | 2756 |  |
