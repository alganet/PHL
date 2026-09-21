# src/ph7/vm_exec_ctx.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1058/1287 lines (82.21%)

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
|     620 |   20 | `PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|       5 |   21 | `{` |
|       - |   22 | `	ph7_exec_ctx *pCtx;` |
|       - |   23 | `	ph7_value *pStack;` |
|       - |   24 | `	VmFrame *pFrame;` |
|     625 |   25 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|     625 |   26 | `	if( pCtx == 0 ){` |
|     ! 0 |   27 | `		return 0;` |
|       - |   28 | `	}` |
|     625 |   29 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|     625 |   30 | `	pCtx->pVm = pVm;` |
|     625 |   31 | `	pCtx->pFunc = pFunc;` |
|     625 |   32 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|     625 |   33 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|     625 |   34 | `	pCtx->pc = 0;` |
|     625 |   35 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|     625 |   36 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|     625 |   37 | `	PH7_MemObjInit(pVm, &pCtx->sDelegate);` |
|       - |   38 | `	/* Container for this body's own exception handlers while suspended (borrowed` |
|       - |   39 | `	 * ph7_exception* pointers — never freed here, owned by the compiled func). */` |
|     625 |   40 | `	SySetInit(&pCtx->aSavedException, &pVm->sAllocator, sizeof(ph7_exception *));` |
|       - |   41 | `	/* ROOT C: this body's own pending finally actions while suspended. */` |
|     625 |   42 | `	SySetInit(&pCtx->aSavedFinally, &pVm->sAllocator, sizeof(VmFinallyAction));` |
|     625 |   43 | `	pCtx->nFinallyBase = 0;` |
|       - |   44 | `	/* Stage 4: this coroutine's own aSelf entries (self::/static:: class context` |
|       - |   45 | `	 * pushed by nested method calls still open at suspend) parked while suspended,` |
|       - |   46 | `	 * so they don't pollute the resumer's aSelf. Borrowed ph7_class* pointers. */` |
|     625 |   47 | `	SySetInit(&pCtx->aSavedSelf, &pVm->sAllocator, sizeof(ph7_class *));` |
|     625 |   48 | `	pCtx->nSelfBase = 0;` |
|     625 |   49 | `	pCtx->pParkedSegment = 0;` |
|     625 |   50 | `	pCtx->nBodyExecDepth = 0;` |
|       - |   51 | `	/* Allocate a private operand stack */` |
|     625 |   52 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|     625 |   53 | `	if( pStack == 0 ){` |
|     ! 0 |   54 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   55 | `		return 0;` |
|       - |   56 | `	}` |
|     625 |   57 | `	pCtx->pStack = pStack;` |
|     625 |   58 | `	pCtx->nStackCap = SySetUsed(&pFunc->aByteCode) + VM_STACK_GUARD; /* grows with pStack on OP_SPREAD */` |
|     625 |   59 | `	pCtx->nStackOrig = pCtx->nStackCap; /* fixed original — headroom reference across resumes */` |
|       - |   60 | `	/* Create a detached frame for the fiber */` |
|     625 |   61 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|     625 |   62 | `	if( pFrame == 0 ){` |
|     ! 0 |   63 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|     ! 0 |   64 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|     625 |   67 | `	pCtx->pFrame = pFrame;` |
|     625 |   68 | `	return pCtx;` |
|     315 |   69 | `}` |
|       - |   70 | `/*` |
|       - |   71 | ` * A suspended coroutine must not leave its own slices of the VM's shared stacks` |
|       - |   72 | ` * sitting above the caller's depth. Three stacks are affected, identically:` |
|       - |   73 | ` *   - pVm->aException: its exception handlers — else a generator/fiber suspended` |
|       - |   74 | ` *     inside a try leaves handlers referencing its now-detached frame on the` |
|       - |   75 | ` *     global stack, corrupting the caller's try/catch.` |
|       - |   76 | ` *   - pVm->aFinallyAction (ROOT C): its pending finally actions — else a yield` |
|       - |   77 | ` *     inside a finally (reached by return/break/rethrow) leaves a record where an` |
|       - |   78 | ` *     out-of-order-resumed sibling generator's OP_END_FINALLY would mis-pop it.` |
|       - |   79 | ` *   - pVm->aSelf (stage 4): its self::/static:: entries pushed by still-open` |
|       - |   80 | ` *     nested method calls — else they sit on the resumer's aSelf and corrupt its` |
|       - |   81 | ` *     self:: resolution.` |
|       - |   82 | ` * Each is the same operation: on suspend move the slice above a captured base` |
|       - |   83 | ` * into a per-ctx park buffer; on resume re-publish it at the (refreshed) caller` |
|       - |   84 | ` * depth. VmParkStackSlice / VmRestoreStackSlice factor it for any element type` |
|       - |   85 | ` * (size taken from the SySet); VmParkCtxState / VmRestoreCtxState drive all three.` |
|       - |   86 | ` *` |
|       - |   87 | ` * Stage 4: the whole suspended segment stays alive, so a parked handler's owner` |
|       - |   88 | ` * frame is never freed underneath it — the parked pointer stays valid and is kept` |
|       - |   89 | ` * (the old stage-2b lossy-path invalidation is gone with the discard). A` |
|       - |   90 | ` * body-level suspend only ever has body-owned handlers here, and its finally/self` |
|       - |   91 | ` * slices are empty (all nested calls already returned) — so those are no-ops.` |
|       - |   92 | ` */` |
|    4998 |   93 | `static void VmParkStackSlice(SySet *pFrom, SySet *pSaved, sxu32 nBase)` |
|       5 |   94 | `{` |
|    5003 |   95 | `	sxu32 nUsed = SySetUsed(pFrom);` |
|    5003 |   96 | `	if( nUsed > nBase ){` |
|     579 |   97 | `		const char *aBase = (const char *)SySetBasePtr(pFrom);` |
|       - |   98 | `		sxu32 i;` |
|    1161 |   99 | `		for( i = nBase; i < nUsed; i++ ){` |
|     587 |  100 | `			SySetPut(pSaved, (const void *)(aBase + i * pFrom->eSize));` |
|     296 |  101 | `		}` |
|     579 |  102 | `		SySetTruncate(pFrom, nBase);` |
|     287 |  103 | `	}` |
|    5003 |  104 | `}` |
|    4170 |  105 | `static void VmRestoreStackSlice(SySet *pTo, SySet *pSaved)` |
|       5 |  106 | `{` |
|    4175 |  107 | `	sxu32 i, n = SySetUsed(pSaved);` |
|    4175 |  108 | `	if( n > 0 ){` |
|     357 |  109 | `		const char *aSaved = (const char *)SySetBasePtr(pSaved);` |
|     717 |  110 | `		for( i = 0; i < n; i++ ){` |
|     365 |  111 | `			SySetPut(pTo, (const void *)(aSaved + i * pSaved->eSize));` |
|     185 |  112 | `		}` |
|     357 |  113 | `		SySetReset(pSaved);` |
|     176 |  114 | `	}` |
|    4175 |  115 | `}` |
|    1666 |  116 | `static void VmParkCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  117 | `{` |
|    1671 |  118 | `	VmParkStackSlice(&pVm->aException, &pCtx->aSavedException, pCtx->nExceptionBase);` |
|    1671 |  119 | `	VmParkStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally, pCtx->nFinallyBase);` |
|    1671 |  120 | `	VmParkStackSlice(&pVm->aSelf, &pCtx->aSavedSelf, pCtx->nSelfBase);` |
|    1671 |  121 | `}` |
|    1390 |  122 | `static void VmRestoreCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  123 | `{` |
|    1395 |  124 | `	VmRestoreStackSlice(&pVm->aException, &pCtx->aSavedException);` |
|    1395 |  125 | `	VmRestoreStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally);` |
|    1395 |  126 | `	VmRestoreStackSlice(&pVm->aSelf, &pCtx->aSavedSelf);` |
|    1395 |  127 | `}` |
|       - |  128 | `/*` |
|       - |  129 | ` * On suspend, free the exception (try) frames the yield was nested in. They were` |
|       - |  130 | ` * pushed by OP_LOAD_EXCEPTION between the coroutine body frame (pCtx->pFrame) and` |
|       - |  131 | ` * the current suspend-point top frame. The generator/fiber frame model saves only` |
|       - |  132 | ` * the body frame, so these transparent wrappers would otherwise be orphaned and` |
|       - |  133 | ` * leak on every yield-that-sits-inside-a-try (unbounded for a generator looping` |
|       - |  134 | ` * with a yield in a try). Freeing them loses nothing the resume needs: this body's` |
|       - |  135 | ` * exception HANDLERS are parked separately (VmParkCtxState) and each` |
|       - |  136 | ` * try's landing pad lives on its ph7_exception (iLandingPc), while OP_POP_EXCEPTION` |
|       - |  137 | ` * on resume skips the (now absent) frame pop via its VM_FRAME_EXCEPTION guard and` |
|       - |  138 | ` * OP_LOAD_EXCEPTION re-creates a fresh wrapper when the try is next entered. Must` |
|       - |  139 | ` * run while pVm->pFrame still points at the suspend-time top (before the detach).` |
|       - |  140 | ` */` |
|    1630 |  141 | `static void VmFreeSuspendedExceptionFrames(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  142 | `{` |
|    1841 |  143 | `	while( pVm->pFrame != pCtx->pFrame && (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     211 |  144 | `		VmLeaveFrame(&(*pVm));` |
|       5 |  145 | `	}` |
|    1635 |  146 | `}` |
|       - |  147 | `/*` |
|       - |  148 | ` * Common suspend epilogue for VmStartCtx / VmResumeCtx: detach the suspended` |
|       - |  149 | ` * coroutine from the live VM chain and park its exception handlers. Two forms:` |
|       - |  150 | ` *   - Body-level (pParkedSegment == 0): a generator yield or a fiber suspending` |
|       - |  151 | ` *     directly in its body. The try wrappers the yield sat in are transient —` |
|       - |  152 | ` *     free them (OP_LOAD_EXCEPTION recreates them on re-entry) — and detach the` |
|       - |  153 | ` *     body frame alone.` |
|       - |  154 | ` *   - Deep fiber suspend (pParkedSegment != 0, stage 4): the whole segment (body` |
|       - |  155 | ` *     frame + the nested call/try frames above it) stays alive and is detached` |
|       - |  156 | ` *     as a unit; nothing is freed, so resume can continue inside the innermost` |
|       - |  157 | ` *     callee. Its handlers are parked the same way and rebased on resume.` |
|       - |  158 | ` */` |
|    1666 |  159 | `static void VmSuspendCtxDetach(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  160 | `{` |
|    1671 |  161 | `	if( pCtx->pParkedSegment == 0 ){` |
|    1317 |  162 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     661 |  163 | `	}else{` |
|       - |  164 | `		/* The parked records' push-time accounting (one nRecursionDepth++ each,` |
|       - |  165 | `		 * plus VmCallFinish's aSelf pop, which never ran) leaves the segment` |
|       - |  166 | `		 * counted as active while the fiber is suspended — deactivate it. aSelf` |
|       - |  167 | `		 * is parked wholesale below (base-relative), so drop only the depth. */` |
|     359 |  168 | `		pVm->nRecursionDepth -= ((VmParkedSegment *)pCtx->pParkedSegment)->nRecords;` |
|       - |  169 | `	}` |
|    1671 |  170 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|    1671 |  171 | `	pCtx->pFrame->pParent = 0;` |
|    1671 |  172 | `	VmParkCtxState(pVm, pCtx);` |
|    1671 |  173 | `	if( pResult ){` |
|     359 |  174 | `		PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|     177 |  175 | `	}` |
|    1671 |  176 | `}` |
|       - |  177 | `/*` |
|       - |  178 | ` * The return-type enforcement target for a coroutine body run. A GENERATOR` |
|       - |  179 | ` * function's declared return type belongs to the call site (always a Generator` |
|       - |  180 | ` * object, validated at compile time as "a supertype of Generator"); the body's` |
|       - |  181 | ` * own return value feeds getReturn() and is never type-checked. Gate on the` |
|       - |  182 | ` * VM_FUNC_GENERATOR flag (the semantic property), not pPrivate (a wrapper-linkage` |
|       - |  183 | ` * fact): a Fiber given a generator-flagged callable runs with pPrivate == 0 and` |
|       - |  184 | ` * must not enforce either. Ordinary fiber callables keep their declared` |
|       - |  185 | ` * return-type enforcement (php enforces it).` |
|       - |  186 | ` */` |
|    1984 |  187 | `static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx)` |
|       5 |  188 | `{` |
|    1195 |  189 | `	return ((pCtx->pFunc->iFlags & VM_FUNC_GENERATOR) == 0 && VmFuncHasReturnType(pCtx->pFunc))` |
|    1190 |  190 | `		? pCtx->pFunc : 0;` |
|       5 |  191 | `}` |
|       - |  192 | `/*` |
|       - |  193 | ` * Common epilogue for VmStartCtx / VmResumeCtx once the body run returns rc:` |
|       - |  194 | ` * restore the previous active context, then park on suspend or detach the` |
|       - |  195 | ` * coroutine frame and record the terminal state. On normal completion the value` |
|       - |  196 | ` * belongs to getReturn() only — start()/resume() return the NEXT suspend value,` |
|       - |  197 | ` * which is null at completion (php parity), so pResult is left at its` |
|       - |  198 | ` * caller-initialized null.` |
|       - |  199 | ` */` |
|    1984 |  200 | `static sxi32 VmFinishCtxRun(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_exec_ctx *pOldCtx,` |
|       - |  201 | `	sxi32 rc, ph7_value *pResult)` |
|       5 |  202 | `{` |
|    1989 |  203 | `	pVm->pActiveCtx = pOldCtx;` |
|    1989 |  204 | `	if( rc == PH7_SUSPEND ){` |
|       - |  205 | `		/* Handles a deep RE-suspend too (a resumed segment parking a fresh one):` |
|       - |  206 | `		 * VmSuspendCtxDetach deactivates the new segment's recursion accounting,` |
|       - |  207 | `		 * parks its aSelf, and skips VmFreeSuspendedExceptionFrames for a segment` |
|       - |  208 | `		 * so it can't free the still-live parked try wrappers. */` |
|    1671 |  209 | `		VmSuspendCtxDetach(pVm, pCtx, pResult);` |
|    1671 |  210 | `		return SXRET_OK;` |
|       - |  211 | `	}` |
|       - |  212 | ``	/* A finally entered via the throw redirect whose `return` short-circuited`` |
|       - |  213 | `	 * OP_END_FINALLY leaves the try's transparent wrapper ABOVE the body frame —` |
|       - |  214 | `	 * the detach below would then be skipped and the wrapper (plus the body` |
|       - |  215 | `	 * frame) leak into the RESUMER's frame chain, so the next try at that scope` |
|       - |  216 | `	 * records the wrong owner frame and its caught throw silently unwinds the` |
|       - |  217 | `	 * script. Free trailing exception wrappers exactly like the suspend path. */` |
|     323 |  218 | `	if( pCtx->pParkedSegment == 0 ){` |
|     323 |  219 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     159 |  220 | `	}` |
|       - |  221 | `	/* Detach the coroutine frame from the live chain, unless a deeper unwind` |
|       - |  222 | `	 * already moved pVm->pFrame off it. */` |
|     323 |  223 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|     323 |  224 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|     323 |  225 | `		pCtx->pFrame->pParent = 0;` |
|     159 |  226 | `	}` |
|     323 |  227 | `	if( rc == PH7_ABORT ){` |
|       3 |  228 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       3 |  229 | `		return PH7_ABORT;` |
|       - |  230 | `	}` |
|     321 |  231 | `	if( rc == PH7_EXCEPTION ){` |
|      47 |  232 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      47 |  233 | `		return PH7_EXCEPTION;` |
|       - |  234 | `	}` |
|     279 |  235 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|     279 |  236 | `	return SXRET_OK;` |
|     997 |  237 | `}` |
|       - |  238 | `/*` |
|       - |  239 | ` * Start executing a fiber context for the first time.` |
|       - |  240 | ` */` |
|     594 |  241 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  242 | `{` |
|       - |  243 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  244 | `	sxi32 rc;` |
|     599 |  245 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|     ! 0 |  246 | `		return SXERR_INVALID;` |
|       - |  247 | `	}` |
|       - |  248 | `	/* A fiber/generator start is a native VmByteCodeExec re-entry, bounded by` |
|       - |  249 | `	 * nMaxNativeDepth. Reject HERE, before attaching the frame / mutating VM` |
|       - |  250 | `	 * state, so the abort is clean — the wrapper's own check fires only after` |
|       - |  251 | `	 * this function has spliced the coroutine into the frame chain, which its` |
|       - |  252 | `	 * post-exec detach cannot fully unwind. The PHP call-depth cap belongs to` |
|       - |  253 | `	 * OP_CALL only (BYTECODE.md stage 5). */` |
|     599 |  254 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  255 | `		return VmNativeNestingFatal(pVm);` |
|       - |  256 | `	}` |
|       - |  257 | `	/* Attach the fiber's frame to the VM frame chain */` |
|     599 |  258 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     599 |  259 | `	pVm->pFrame = pCtx->pFrame;` |
|       - |  260 | `	/* Save and set the active context */` |
|     599 |  261 | `	pOldCtx = pVm->pActiveCtx;` |
|     599 |  262 | `	pVm->pActiveCtx = pCtx;` |
|     599 |  263 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|     599 |  264 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|     599 |  265 | `	pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|     599 |  266 | `	pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|       - |  267 | `	/* Native depth the body runs at (the VmByteCodeExec wrapper bumps +1): a` |
|       - |  268 | `	 * Fiber::suspend() at a deeper depth is inside a C->PHP callback and gets a` |
|       - |  269 | `	 * FiberError instead of parking across the native frame (stage 4). */` |
|     599 |  270 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1;` |
|       - |  271 | `	/* Execute from the beginning (no parked segment). An OP_SPREAD in the body may` |
|       - |  272 | `	 * realloc pCtx->pStack — pass &pCtx->pStack / &pCtx->nStackCap so the grown` |
|       - |  273 | `	 * buffer + capacity persist for the next resume and for ctx teardown. */` |
|     896 |  274 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     297 |  275 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|     297 |  276 | `		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|     599 |  277 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     302 |  278 | `}` |
|       - |  279 | `/*` |
|       - |  280 | ` * Resume a suspended fiber context.` |
|       - |  281 | ` */` |
|    1390 |  282 | `PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|       5 |  283 | `{` |
|       - |  284 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  285 | `	VmParkedSegment *pSeg;` |
|       - |  286 | `	sxi32 rc;` |
|    1395 |  287 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|     ! 0 |  288 | `		return SXERR_INVALID;` |
|       - |  289 | `	}` |
|       - |  290 | `	/* A resume is a native VmByteCodeExec re-entry, bounded by nMaxNativeDepth.` |
|       - |  291 | `	 * Reject HERE, before re-attaching the (possibly deep) parked segment to the` |
|       - |  292 | `	 * frame chain and re-adding its nRecords to nRecursionDepth — a wrapper-level` |
|       - |  293 | `	 * abort past those mutations would leave pVm->pFrame pointing into the parked` |
|       - |  294 | `	 * callee and the depth accounting un-reverted. The PHP call-depth cap is` |
|       - |  295 | `	 * OP_CALL-only (BYTECODE.md stage 5). */` |
|    1395 |  296 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  297 | `		return VmNativeNestingFatal(pVm);` |
|       - |  298 | `	}` |
|       - |  299 | `	/* Push the resume value onto the SUSPENDED activation's operand stack so it` |
|       - |  300 | `	 * appears as Fiber::suspend()'s return value. For a deep suspend (stage 4)` |
|       - |  301 | `	 * that stack is the innermost callee's — parked in the segment — not the` |
|       - |  302 | `	 * body's. nTos was saved one below the return-value slot. */` |
|       - |  303 | `	{` |
|       - |  304 | `		ph7_value *pResumeStack;` |
|    1395 |  305 | `		pSeg = (VmParkedSegment *)pCtx->pParkedSegment;` |
|    1395 |  306 | `		pResumeStack = pSeg ? pSeg->sState.pStack : pCtx->pStack;` |
|    1395 |  307 | `		if( pResumeValue ){` |
|     309 |  308 | `			PH7_MemObjStore(pResumeValue, &pResumeStack[pCtx->nTos + 1]);` |
|     157 |  309 | `		}else{` |
|    1091 |  310 | `			PH7_MemObjRelease(&pResumeStack[pCtx->nTos + 1]);` |
|       - |  311 | `		}` |
|    1395 |  312 | `		pCtx->nTos++;` |
|       - |  313 | `		/* Refresh the caller-depth base and re-publish this body's own exception` |
|       - |  314 | `		 * handlers on top of pVm->aException at that depth, so the resumed body's` |
|       - |  315 | `		 * try/catch and finally-drain bound line up (see VmByteCodeExec's base` |
|       - |  316 | `		 * override). Must run before VmByteCodeExec recaptures its local base. */` |
|    1395 |  317 | `		pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|    1395 |  318 | `		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|    1395 |  319 | `		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|    1395 |  320 | `		VmRestoreCtxState(pVm, pCtx);` |
|    1395 |  321 | `		if( pSeg ){` |
|       - |  322 | `			/* Reactivate the parked records' recursion accounting (mirror of the` |
|       - |  323 | `			 * deactivate at suspend); aSelf was just restored above. */` |
|     149 |  324 | `			pVm->nRecursionDepth += pSeg->nRecords;` |
|       - |  325 | `			/* Rebase the parked segment's absolute exception-floor indices: the` |
|       - |  326 | `			 * fiber may resume at a different caller depth than it suspended at,` |
|       - |  327 | `			 * so every activation's nExceptionBase shifts by the same delta the` |
|       - |  328 | `			 * republished handlers moved (newBase - the park-time base). */` |
|     149 |  329 | `			sxi32 iDelta = (sxi32)pCtx->nExceptionBase - (sxi32)pSeg->nOldExcBase;` |
|       - |  330 | `			/* nFinallyActBase floors rebase by their OWN delta — the exception and` |
|       - |  331 | `			 * finally-action stacks move independently between suspend and resume` |
|       - |  332 | `			 * (a fiber resumed from inside a generator's inline finally sees a` |
|       - |  333 | `			 * DEEPER aFinallyAction with an unchanged aException, and a stale` |
|       - |  334 | `			 * absolute floor would make the activation-end discard eat the` |
|       - |  335 | `			 * resumer's pending action). */` |
|     149 |  336 | `			sxi32 iFinDelta = (sxi32)pCtx->nFinallyBase - (sxi32)pSeg->nOldFinBase;` |
|     149 |  337 | `			if( iDelta != 0 \|\| iFinDelta != 0 ){` |
|       - |  338 | `				VmCallFrame *pRec;` |
|       3 |  339 | `				pSeg->sState.nExceptionBase =` |
|       2 |  340 | `					(sxu32)((sxi32)pSeg->sState.nExceptionBase + iDelta);` |
|       3 |  341 | `				pSeg->sState.nFinallyActBase =` |
|       2 |  342 | `					(sxu32)((sxi32)pSeg->sState.nFinallyActBase + iFinDelta);` |
|       5 |  343 | `				for( pRec = pSeg->pCallTop; pRec; pRec = pRec->pPrev ){` |
|       3 |  344 | `					pRec->sCaller.nExceptionBase =` |
|       2 |  345 | `						(sxu32)((sxi32)pRec->sCaller.nExceptionBase + iDelta);` |
|       3 |  346 | `					pRec->sCaller.nFinallyActBase =` |
|       2 |  347 | `						(sxu32)((sxi32)pRec->sCaller.nFinallyActBase + iFinDelta);` |
|       2 |  348 | `				}` |
|       1 |  349 | `			}` |
|      72 |  350 | `		}` |
|       - |  351 | `		/* Re-attach the coroutine to the live VM frame chain: the body frame's` |
|       - |  352 | `		 * parent becomes the resumer's current frame. For a deep segment the` |
|       - |  353 | `		 * suspend-time top frame (the innermost callee / open-try wrapper) then` |
|       - |  354 | `		 * becomes current so the adopt at VmByteCodeExec entry resumes inside the` |
|       - |  355 | `		 * callee; body-level resumes make the body frame current. */` |
|    1395 |  356 | `		pCtx->pFrame->pParent = pVm->pFrame;` |
|    1395 |  357 | `		pVm->pFrame = pSeg ? pSeg->pTopFrame : pCtx->pFrame;` |
|       - |  358 | `	}` |
|       - |  359 | `	/* The segment (if any) is handed to the body invocation explicitly below; it` |
|       - |  360 | `	 * is no longer part of the suspended ctx state once resume owns it. */` |
|    1395 |  361 | `	pCtx->pParkedSegment = 0;` |
|       - |  362 | `	/* Save and set the active context */` |
|    1395 |  363 | `	pOldCtx = pVm->pActiveCtx;` |
|    1395 |  364 | `	pVm->pActiveCtx = pCtx;` |
|    1395 |  365 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|    1395 |  366 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1; /* see VmStartCtx */` |
|       - |  367 | `	/* Resume execution from saved PC. pSeg, when non-NULL, makes this body` |
|       - |  368 | `	 * re-enter inside the innermost parked callee (deep fiber resume, stage 4). */` |
|    2090 |  369 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     695 |  370 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|     695 |  371 | `		VmCtxEnforceRetFunc(pCtx), FALSE, pSeg, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|    1395 |  372 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     700 |  373 | `}` |
|       - |  374 | `/*` |
|       - |  375 | ` * Force-close a suspended generator context at destruction time, running its` |
|       - |  376 | `` * pending `finally` blocks (PHP runs finally when a generator is unset / goes out`` |
|       - |  377 | ` * of scope / is GC'd before it completes; PHL previously freed the open try` |
|       - |  378 | ` * handlers unexecuted in VmReleaseExecCtx). This is a "close", not a "resume":` |
|       - |  379 | `` * the finally handler of every still-open `try` the generator was suspended`` |
|       - |  380 | `` * inside runs innermost-first, but NO `catch` runs and no code past the finallys`` |
|       - |  381 | ` * executes.` |
|       - |  382 | ` *` |
|       - |  383 | ` * Generators compile finally INLINE (ROOT C): the finally bytecode lives in the` |
|       - |  384 | ` * body program at iFinallyPc, driven by the aException/aFinallyAction pc-redirect` |
|       - |  385 | `` * machinery — not the legacy detached `sFinally` mini-program VmDrainFinally runs.`` |
|       - |  386 | `` * So a close is expressed exactly like a `return` that crosses every enclosing`` |
|       - |  387 | ` * finally (OP_SET_FINALLY_RET): set pCtx->bClosing and resume; the body-entry` |
|       - |  388 | ` * redirect (see VmByteCodeExecBody) seeds a PH7_FA_RETURN action and jumps into` |
|       - |  389 | ` * the innermost open try's finally, and OP_END_FINALLY threads it out through the` |
|       - |  390 | ` * chain, then completes the body.` |
|       - |  391 | ` *` |
|       - |  392 | ` * Scope: a plain body-level suspend (pParkedSegment == 0). A deep fiber segment is` |
|       - |  393 | ` * left to plain release (generators never park one — yield is body-level only). A` |
|       - |  394 | `` * `yield` reached inside a finally during close is rejected by OP_YIELD via`` |
|       - |  395 | ` * pCtx->bClosing (PHP-exact "Cannot yield from finally in a force-closed` |
|       - |  396 | ` * generator"). Deferred edges remain.` |
|       - |  397 | ` *` |
|       - |  398 | ` * Returns whatever the body run returns: SXRET_OK on a clean close, PH7_ABORT if a` |
|       - |  399 | ` * finally aborts, or PH7_EXCEPTION if a finally threw past itself (surfaced to the` |
|       - |  400 | ` * destruct caller).` |
|       - |  401 | ` */` |
|     238 |  402 | `static sxi32 VmCloseCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  403 | `{` |
|       - |  404 | `	sxi32 rc;` |
|     243 |  405 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       - |  406 | `		/* CREATED never ran, so has no open try; COMPLETED/CLOSED already ran theirs. */` |
|     205 |  407 | `		return SXRET_OK;` |
|       - |  408 | `	}` |
|      42 |  409 | `	if( pCtx->pParkedSegment != 0 ){` |
|       - |  410 | `		/* Deep fiber segment (never a generator) — leave to plain release. */` |
|     ! 0 |  411 | `		return SXRET_OK;` |
|       - |  412 | `	}` |
|       - |  413 | ``	/* Suspended mid `yield from` over an inner generator: PHP closes innermost-first,`` |
|       - |  414 | `	 * so run the delegate's finallys before this body's. Both delegate-object states` |
|       - |  415 | `	 * carry the live iterator in sDelegate — state 3 (the delegate IS a Generator) and` |
|       - |  416 | ``	 * state 2 (an Iterator, which for `yield from $aggregate` is the generator returned`` |
|       - |  417 | `	 * by getIterator()); VmGeneratorExtractCtx returns 0 for a non-generator iterator,` |
|       - |  418 | `	 * so a plain Iterator/array delegate is skipped. Recurses for nested delegation;` |
|       - |  419 | `	 * the inner ends COMPLETED, so its own later __destruct close is a no-op. */` |
|      42 |  420 | `	if( pCtx->iDelegateState >= 2 && (pCtx->sDelegate.iFlags & MEMOBJ_OBJ) ){` |
|       5 |  421 | `		ph7_generator *pInner = VmGeneratorExtractCtx(pVm, &pCtx->sDelegate);` |
|       5 |  422 | `		if( pInner && pInner->pCtx ){` |
|       5 |  423 | `			sxi32 rcInner = VmCloseCtx(pVm, pInner->pCtx);` |
|       5 |  424 | `			if( rcInner == PH7_ABORT ){ return PH7_ABORT; }` |
|       2 |  425 | `		}` |
|       2 |  426 | `	}` |
|       - |  427 | `	/* Drive the pending finallys through a real body resume that the entry redirect` |
|       - |  428 | `	 * turns into a finally-chain unwind. VmResumeCtx handles state restore, frame` |
|       - |  429 | `	 * re-attach, active-ctx save/restore and the terminal-state bookkeeping. */` |
|      42 |  430 | `	pCtx->bClosing = 1;` |
|      42 |  431 | `	rc = VmResumeCtx(pVm, pCtx, 0, 0);` |
|      42 |  432 | `	pCtx->bClosing = 0;` |
|      42 |  433 | `	return rc;` |
|     124 |  434 | `}` |
|       - |  435 | `/*` |
|       - |  436 | ` * Free one DETACHED frame (not in the live pVm->pFrame chain): the frame of a` |
|       - |  437 | ` * suspended coroutine's body, or of a segment activation abandoned mid-call.` |
|       - |  438 | ` * Mirrors VmLeaveFrame's teardown minus the chain pop (there is no chain to pop` |
|       - |  439 | ` * from). Factored so the body-frame free and the stage-4 segment free share it.` |
|       - |  440 | ` */` |
|     872 |  441 | `static void VmFreeDetachedFrame(ph7_vm *pVm, VmFrame *pFrame)` |
|       5 |  442 | `{` |
|       - |  443 | `	VmSlot *aSlot;` |
|       - |  444 | `	sxu32 n;` |
|     877 |  445 | `	if( pFrame == 0 ){` |
|     ! 0 |  446 | `		return;` |
|       - |  447 | `	}` |
|       - |  448 | `	/* Free local variables */` |
|     877 |  449 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|    1743 |  450 | `	for( n = 0; n < SySetUsed(&pFrame->sLocal); ++n ){` |
|     871 |  451 | `		PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|     438 |  452 | `	}` |
|       - |  453 | `	/* Remove local references */` |
|     877 |  454 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|    1743 |  455 | `	for( n = 0; n < SySetUsed(&pFrame->sRef); ++n ){` |
|     871 |  456 | `		PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|     438 |  457 | `	}` |
|     877 |  458 | `	SyHashRelease(&pFrame->hVar);` |
|     877 |  459 | `	SySetRelease(&pFrame->sArg);` |
|     877 |  460 | `	SySetRelease(&pFrame->sLocal);` |
|     877 |  461 | `	SySetRelease(&pFrame->sRef);` |
|     877 |  462 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|       - |  463 | `	/* Drop a resume target pointing at this detached frame before we free it (ROOT B). */` |
|     877 |  464 | `	VmDropResumeTarget(pVm,pFrame);` |
|     877 |  465 | `	SyMemBackendPoolFree(&pVm->sAllocator, pFrame);` |
|     441 |  466 | `}` |
|       - |  467 | `/*` |
|       - |  468 | ` * Free a parked deep-suspend segment (stage 4) whose fiber was abandoned while` |
|       - |  469 | ` * suspended. Every record holds a callee's operand stack and VmFrame (the` |
|       - |  470 | ` * topmost record's callee is the innermost activation, running on sState); walk` |
|       - |  471 | ` * the chain releasing each callee stack's live entries then the stack and frame.` |
|       - |  472 | ` * The body frame/stack are NOT here — they are freed by the caller` |
|       - |  473 | ` * (VmReleaseExecCtx) as pCtx->pFrame / pCtx->pStack.` |
|       - |  474 | ` */` |
|     206 |  475 | `static void VmFreeParkedSegment(ph7_vm *pVm, ph7_exec_ctx *pCtx, VmParkedSegment *pSeg)` |
|       2 |  476 | `{` |
|       - |  477 | `	/* Live top-of-stack of the activation running on the current record's callee` |
|       - |  478 | `	 * stack: the innermost (sState) for the topmost record, then each caller. */` |
|     208 |  479 | `	ph7_value *pTosAbove = pSeg->sState.pTos;` |
|     208 |  480 | `	VmCallFrame *pRec = pSeg->pCallTop, *pNext;` |
|     614 |  481 | `	while( pRec ){` |
|     408 |  482 | `		ph7_value *pStk = pRec->sCall.pFrameStack;` |
|     408 |  483 | `		if( pStk ){` |
|     408 |  484 | `			ph7_value *pTos = pTosAbove;` |
|     814 |  485 | `			while( pTos >= pStk ){` |
|     408 |  486 | `				PH7_MemObjRelease(pTos);` |
|     408 |  487 | `				pTos--;` |
|       2 |  488 | `			}` |
|     408 |  489 | `			SyMemBackendFree(&pVm->sAllocator, pStk);` |
|     203 |  490 | `		}` |
|     408 |  491 | `		VmFreeDetachedFrame(pVm, pRec->sCall.pFrame);` |
|       - |  492 | `		/* The caller recorded here runs on the NEXT-lower callee stack; grab its` |
|       - |  493 | `		 * live tos before freeing this node. */` |
|     408 |  494 | `		pTosAbove = pRec->sCaller.pTos;` |
|     408 |  495 | `		pNext = pRec->pPrev;` |
|     408 |  496 | `		SyMemBackendPoolFree(&pVm->sAllocator, pRec);` |
|     408 |  497 | `		pRec = pNext;` |
|       2 |  498 | `	}` |
|       - |  499 | `	/* pTosAbove now points at the BODY activation's live top (the bottom record's` |
|       - |  500 | `	 * sCaller, which runs on pCtx->pStack). pCtx->nTos still holds the INNERMOST` |
|       - |  501 | `	 * index (VmSuspendCtx saved it), which would over-index the body stack in` |
|       - |  502 | `	 * VmReleaseExecCtx's release loop — correct it to the body's real top. */` |
|     208 |  503 | `	pCtx->nTos = (sxi32)(pTosAbove - pCtx->pStack);` |
|     208 |  504 | `	SyMemBackendFree(&pVm->sAllocator, pSeg);` |
|     208 |  505 | `}` |
|       - |  506 | `/*` |
|       - |  507 | ` * Release an execution context and all its resources.` |
|       - |  508 | ` */` |
|     466 |  509 | `PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  510 | `{` |
|     471 |  511 | `	if( pCtx == 0 ){` |
|     ! 0 |  512 | `		return;` |
|       - |  513 | `	}` |
|     471 |  514 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       - |  515 | `		/* Cannot destroy a fiber that is currently executing */` |
|     ! 0 |  516 | `		return;` |
|       - |  517 | `	}` |
|     471 |  518 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       - |  519 | `	/* Release values */` |
|     471 |  520 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|     471 |  521 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|     471 |  522 | `	PH7_MemObjRelease(&pCtx->sDelegate);` |
|       - |  523 | `	/* Stage 2b: the parked entries are per-activation clones now — free them` |
|       - |  524 | `	 * (an abandoned suspended coroutine is their last holder). */` |
|     471 |  525 | `	VmExcReleaseAll(pVm,&pCtx->aSavedException);` |
|     471 |  526 | `	SySetRelease(&pCtx->aSavedException);` |
|       - |  527 | `	/* ROOT C: a generator abandoned while suspended inside a finally may carry parked` |
|       - |  528 | `	 * finally actions holding owned values/refs (a RETURN's sRet, a RETHROW's pExc).` |
|       - |  529 | `	 * Release them so the abandon path leaks nothing. */` |
|       - |  530 | `	{` |
|     471 |  531 | `		sxu32 n = SySetUsed(&pCtx->aSavedFinally);` |
|     471 |  532 | `		if( n > 0 ){` |
|     ! 0 |  533 | `			VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pCtx->aSavedFinally);` |
|       - |  534 | `			sxu32 i;` |
|     ! 0 |  535 | `			for( i = 0; i < n; i++ ){` |
|     ! 0 |  536 | `				if( aA[i].eKind == PH7_FA_RETURN ){` |
|     ! 0 |  537 | `					PH7_MemObjRelease(&aA[i].sRet);` |
|     ! 0 |  538 | `				}else if( aA[i].eKind == PH7_FA_RETHROW && aA[i].pExc ){` |
|     ! 0 |  539 | `					PH7_ClassInstanceUnref(aA[i].pExc);` |
|     ! 0 |  540 | `				}` |
|     ! 0 |  541 | `			}` |
|     ! 0 |  542 | `		}` |
|     471 |  543 | `		SySetRelease(&pCtx->aSavedFinally);` |
|       - |  544 | `	}` |
|       - |  545 | `	/* Stage 4: parked aSelf entries are borrowed class pointers — just free the set. */` |
|     471 |  546 | `	SySetRelease(&pCtx->aSavedSelf);` |
|       - |  547 | `	/* Free a parked deep-suspend segment (stage 4): the fiber was abandoned while` |
|       - |  548 | `	 * suspended inside a nested call, so its record chain / frames / operand` |
|       - |  549 | `	 * stacks are still alive and only this holder references them. Must run` |
|       - |  550 | `	 * before the body frame/stack below (they are the segment's floor). */` |
|     471 |  551 | `	if( pCtx->pParkedSegment ){` |
|     208 |  552 | `		VmFreeParkedSegment(pVm, pCtx, (VmParkedSegment *)pCtx->pParkedSegment);` |
|     208 |  553 | `		pCtx->pParkedSegment = 0;` |
|     103 |  554 | `	}` |
|       - |  555 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|     471 |  556 | `	if( pCtx->pFrame ){` |
|     471 |  557 | `		VmFreeDetachedFrame(pVm, pCtx->pFrame);` |
|     471 |  558 | `		pCtx->pFrame = 0;` |
|     233 |  559 | `	}` |
|       - |  560 | `	/* Release individual operand stack entries (decrement refcounts,` |
|       - |  561 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|       - |  562 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|     471 |  563 | `	if( pCtx->pStack ){` |
|     471 |  564 | `		if( pCtx->nTos >= 0 ){` |
|     429 |  565 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|     853 |  566 | `			while( pTos >= pCtx->pStack ){` |
|     429 |  567 | `				PH7_MemObjRelease(pTos);` |
|     429 |  568 | `				pTos--;` |
|       5 |  569 | `			}` |
|     212 |  570 | `		}` |
|     471 |  571 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|     471 |  572 | `		pCtx->pStack = 0;` |
|     233 |  573 | `	}` |
|       - |  574 | `	/* Free the context itself */` |
|     471 |  575 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     238 |  576 | `}` |
|       - |  577 | `/*` |
|       - |  578 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|       - |  579 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|       - |  580 | ` */` |
|     760 |  581 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|       5 |  582 | `{` |
|       - |  583 | `	ph7_class_instance *pThis;` |
|       - |  584 | `	SyString sAttr;` |
|       - |  585 | `	ph7_value *pAttr;` |
|     765 |  586 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 |  587 | `		return 0;` |
|       - |  588 | `	}` |
|     765 |  589 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|     765 |  590 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|     ! 0 |  591 | `		return 0;` |
|       - |  592 | `	}` |
|     765 |  593 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     765 |  594 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     765 |  595 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     269 |  596 | `		return 0;` |
|       - |  597 | `	}` |
|     501 |  598 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|     385 |  599 | `}` |
|       - |  600 | `/* ph7_class_instance.iFlags bit: this Closure is a bound/static first-class callable and` |
|       - |  601 | ` * carries $__this/$__scope. Lets the hot plain-closure unwrap skip those attribute lookups.` |
|       - |  602 | ` * (Distinct from CLASS_INSTANCE_DESTROYED 0x001 and VM_INSTANCE_DUMPING 0x002.) */` |
|       - |  603 | `#define VM_INSTANCE_FCC_BOUND 0x004` |
|       - |  604 | `/*` |
|       - |  605 | `` * A PHP closure (and a first-class callable `f(...)`) is a real object: an instance of`` |
|       - |  606 | `` * the built-in final `Closure` class carrying its underlying callable in a private`` |
|       - |  607 | `` * `$__fn` attribute (the callable NAME: a registered `[closure_N]`/`[lambda_N]` or a`` |
|       - |  608 | ` * user/host function name) — plus, for a method/static first-class callable, a bound` |
|       - |  609 | `` * `$__this` object and/or a `$__scope` class-name. Storing the callable as plain attributes`` |
|       - |  610 | `` * (rather than a native resource pointer) means the object owns no `ph7_vm_func` — the`` |
|       - |  611 | `` * per-closure function stays owned by `hFunction`/VM-release exactly as before, so there is`` |
|       - |  612 | ` * no extra free path.` |
|       - |  613 | ` *` |
|       - |  614 | ` * Returns non-zero iff pVal is a Closure instance.` |
|       - |  615 | ` */` |
| 6865366 |  616 | `PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm, ph7_value *pVal)` |
|       5 |  617 | `{` |
|       - |  618 | `	ph7_class_instance *pThis;` |
|       - |  619 | `	/* Flag test first: a non-object call target (the hot common case) bails before any` |
|       - |  620 | `	 * pVm dereference; pClosureClass==0 is a one-time pre-init concern, so it goes last. */` |
| 6865371 |  621 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
| 6659225 |  622 | `		return 0;` |
|       - |  623 | `	}` |
|  206151 |  624 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       - |  625 | `	/* Closure is final, so an exact class match is correct (no subclasses possible). */` |
|  206151 |  626 | `	return pThis->pClass == pVm->pClosureClass;` |
| 3433453 |  627 | `}` |
|       - |  628 | `/*` |
|       - |  629 | ` * Unwrap a Closure value into the simple callable the existing dispatch machinery` |
|       - |  630 | ` * already understands, written into pOut (which the caller must have initialised):` |
|       - |  631 | `` *   - bound `$this` set (method first-class callable) -> [ $__this, $__fn ] array callable`` |
|       - |  632 | `` *   - `$__scope` set (static first-class callable)     -> [ $__scope, $__fn ] array callable`` |
|       - |  633 | `` *   - neither (plain function / real closure)          -> the `$__fn` name string`` |
|       - |  634 | ` * The 2-element array form rides the existing MEMOBJ_HASHMAP dispatch, which binds $this` |
|       - |  635 | ` * for an object first element and resolves the class for a class-name-string first element.` |
|       - |  636 | ` * Returns SXRET_OK if pVal was a Closure (pOut filled), SXERR_NOTFOUND otherwise.` |
|       - |  637 | ` */` |
|    2544 |  638 | `PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm, ph7_value *pVal, ph7_value *pOut)` |
|       5 |  639 | `{` |
|       - |  640 | `	ph7_class_instance *pThis;` |
|       - |  641 | `	ph7_value *pFn;` |
|       - |  642 | `	SyString sAttr;` |
|    2549 |  643 | `	if( !VmValueIsClosure(pVm, pVal) ){` |
|     ! 0 |  644 | `		return SXERR_NOTFOUND;` |
|       - |  645 | `	}` |
|    2549 |  646 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    2549 |  647 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    2549 |  648 | `	pFn = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    2549 |  649 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 |  650 | `		return SXERR_NOTFOUND; /* malformed/uninitialised closure */` |
|       - |  651 | `	}` |
|       - |  652 | `	/* Only a bound/static first-class callable (rare) carries $__this/$__scope; the` |
|       - |  653 | `	 * VM_INSTANCE_FCC_BOUND flag (set by VmCreateClosure) keeps the hot plain-closure path` |
|       - |  654 | `	 * to the single $__fn lookup above instead of two extra attribute lookups per dispatch. */` |
|    2549 |  655 | `	if( pThis->iFlags & VM_INSTANCE_FCC_BOUND ){` |
|       - |  656 | `		ph7_value *pBound, *pScope;` |
|       - |  657 | `		int bBoundObj, bScope;` |
|     107 |  658 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     107 |  659 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     107 |  660 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     107 |  661 | `		pScope = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     107 |  662 | `		bBoundObj = pBound && (pBound->iFlags & MEMOBJ_OBJ);` |
|     107 |  663 | `		bScope = pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0;` |
|     107 |  664 | `		if( bBoundObj \|\| bScope ){` |
|       - |  665 | `			/* Method/static first-class callable -> [ target, "method" ] array callable. */` |
|       - |  666 | `			ph7_hashmap *pMap;` |
|       - |  667 | `			ph7_value sTarget, sMeth;` |
|       - |  668 | `			sxi32 rc;` |
|     107 |  669 | `			if( bBoundObj ){` |
|      75 |  670 | `				ph7_class_instance *pBoundObj = (ph7_class_instance *)pBound->x.pOther;` |
|       - |  671 | ``				/* A bound PLAIN closure (`function(){…}->bindTo($o)`): $__fn names a function, not a`` |
|       - |  672 | `				 * method of the bound object's class, so a [obj,method] array callable would fail` |
|       - |  673 | `				 * method resolution. Stash the bound object (own a ref) so the OP_CALL user-function` |
|       - |  674 | `				 * frame setup injects it as $this, and return the plain $__fn string for a normal` |
|       - |  675 | `				 * function dispatch. */` |
|     111 |  676 | `				if( PH7_ClassExtractMethod(pBoundObj->pClass,` |
|     112 |  677 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){` |
|       - |  678 | `					/* Only a USER function (anonymous closure / named fn in hFunction) reads $this and` |
|       - |  679 | `					 * reaches the OP_CALL user-function frame-setup that consumes pClosureThis. A HOST` |
|       - |  680 | `					 * function (e.g. Closure::fromCallable('strlen')->bindTo($o)) ignores $this and` |
|       - |  681 | `					 * dispatches via the host path which never consumes the transient — setting it` |
|       - |  682 | `					 * there would leak the ref and inject a stale $this into the next call. */` |
|      48 |  683 | `					if( SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|      33 |  684 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|      33 |  685 | `						pBoundObj->iRef++;` |
|      33 |  686 | `						pVm->pClosureThis = pBoundObj;` |
|       - |  687 | `						/* Carry the bound scope (if set) so private/protected member access inside` |
|       - |  688 | `						 * the closure body resolves against it — bindTo($o, Scope::class) / call($o). */` |
|      33 |  689 | `						if( bScope ){` |
|      34 |  690 | `							pVm->pClosureScope = PH7_VmExtractClass(pVm,` |
|      22 |  691 | `								(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      11 |  692 | `						}` |
|      16 |  693 | `					}` |
|      33 |  694 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      33 |  695 | `					return SXRET_OK;` |
|       - |  696 | `				}` |
|      22 |  697 | `			}else{` |
|       - |  698 | ``				/* Scope-only rebind of a PLAIN closure (`bindTo(null, Scope::class)`):`` |
|       - |  699 | `				 * $__fn names a function, not a static method of the scope class, so` |
|       - |  700 | `				 * the [scope, method] array callable below would fail method` |
|       - |  701 | `				 * resolution. Dispatch the plain $__fn string and carry the scope for` |
|       - |  702 | `				 * private/protected visibility (pClosureThis stays unset — no $this).` |
|       - |  703 | `				 * A static-method FCC ($__fn really is a method of the scope class)` |
|       - |  704 | `				 * falls through to the array-callable path. */` |
|      49 |  705 | `				ph7_class *pScopeClass = PH7_VmExtractClass(pVm,` |
|      32 |  706 | `					(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      32 |  707 | `				if( pScopeClass == 0` |
|      33 |  708 | `				 \|\| PH7_ClassExtractMethod(pScopeClass,` |
|      48 |  709 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){` |
|       6 |  710 | `					if( pScopeClass` |
|       7 |  711 | `					 && SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|       6 |  712 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|       7 |  713 | `						pVm->pClosureScope = pScopeClass;` |
|       3 |  714 | `					}` |
|       7 |  715 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|       7 |  716 | `					return SXRET_OK;` |
|       - |  717 | `				}` |
|       - |  718 | `			}` |
|      69 |  719 | `			pMap = PH7_NewHashmap(&(*pVm), 0, 0);` |
|      69 |  720 | `			if( pMap == 0 ){` |
|     ! 0 |  721 | `				return SXERR_NOTFOUND;` |
|       - |  722 | `			}` |
|      69 |  723 | `			PH7_MemObjInit(pVm, &sTarget);` |
|      69 |  724 | `			PH7_MemObjInit(pVm, &sMeth);` |
|      69 |  725 | `			if( bBoundObj ){` |
|      43 |  726 | `				PH7_MemObjStore(pBound, &sTarget); /* bound object (iRef++) -> binds $this */` |
|      22 |  727 | `			}else{` |
|      27 |  728 | `				PH7_MemObjStringAppend(&sTarget, SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob));` |
|       - |  729 | `			}` |
|      69 |  730 | `			PH7_MemObjStringAppend(&sMeth, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      69 |  731 | `			rc = PH7_HashmapInsert(pMap, 0, &sTarget);` |
|      69 |  732 | `			if( rc == SXRET_OK ){` |
|      69 |  733 | `				rc = PH7_HashmapInsert(pMap, 0, &sMeth);` |
|      34 |  734 | `			}` |
|      69 |  735 | `			PH7_MemObjRelease(&sTarget);` |
|      69 |  736 | `			PH7_MemObjRelease(&sMeth);` |
|      69 |  737 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  738 | `				PH7_HashmapRelease(pMap, TRUE); /* free the partial map, no leak */` |
|     ! 0 |  739 | `				return SXERR_NOTFOUND;` |
|       - |  740 | `			}` |
|      69 |  741 | `			pOut->x.pOther = pMap;` |
|      69 |  742 | `			MemObjSetType(pOut, MEMOBJ_HASHMAP);` |
|      69 |  743 | `			return SXRET_OK;` |
|       - |  744 | `		}` |
|     ! 0 |  745 | `	}` |
|    2443 |  746 | `	PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    2443 |  747 | `	return SXRET_OK;` |
|    1277 |  748 | `}` |
|       - |  749 | `/*` |
|       - |  750 | `` * Resolve the scope class for a STATIC first-class callable `T::m(...)`, where T is a`` |
|       - |  751 | ` * class-name STRING value: handles the self/static/parent keywords against the live class` |
|       - |  752 | ` * context (so the actual class is bound at FCC-creation time, like PHP), and falls back to` |
|       - |  753 | ` * an explicit class-name lookup. Mirrors the static OP_MEMBER resolution. Returns 0 if the` |
|       - |  754 | ` * class cannot be resolved.` |
|       - |  755 | ` */` |
|      66 |  756 | `PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget)` |
|       1 |  757 | `{` |
|      67 |  758 | `	const char *zCls = (const char *)SyBlobData(&pTarget->sBlob);` |
|      67 |  759 | `	sxu32 nCls = (sxu32)SyBlobLength(&pTarget->sBlob);` |
|       - |  760 | `	ph7_class *pClass;` |
|      67 |  761 | `	if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|      17 |  762 | `		pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      17 |  763 | `		if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|     ! 0 |  764 | `			pClass = PH7_VmPeekTopClass(&(*pVm)); /* self:: in a trait -> using class */` |
|       1 |  765 | `		}` |
|      59 |  766 | `	}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|      31 |  767 | `		pClass = PH7_VmPeekTopClass(&(*pVm));     /* late static binding */` |
|      36 |  768 | `	}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       7 |  769 | `		pClass = PH7_VmResolveParentClass(&(*pVm));` |
|       4 |  770 | `	}else{` |
|      15 |  771 | `		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - |  772 | `	}` |
|      67 |  773 | `	return pClass;` |
|       1 |  774 | `}` |
|       - |  775 | `/*` |
|       - |  776 | ` * Create a Closure object wrapping a callable name (+ optional bound $this object and/or` |
|       - |  777 | `` * scope class-name, for the method/static first-class callables `$o->m(...)`/`C::m(...)`).`` |
|       - |  778 | ` * Mirrors the Generator/Fiber "object carries its state in private attributes" pattern.` |
|       - |  779 | ` * Returns the fresh instance (iRef == 0; caller takes the reference), or 0 on OOM.` |
|       - |  780 | ` */` |
|    1642 |  781 | `PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName,` |
|       - |  782 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       5 |  783 | `{` |
|       - |  784 | `	ph7_class_instance *pObj;` |
|       - |  785 | `	ph7_value *pAttr;` |
|       - |  786 | `	SyString sAttr;` |
|    1647 |  787 | `	if( pVm->pClosureClass == 0 ){` |
|     ! 0 |  788 | `		return 0;` |
|       - |  789 | `	}` |
|    1647 |  790 | `	pObj = PH7_NewClassInstance(&(*pVm), pVm->pClosureClass);` |
|    1647 |  791 | `	if( pObj == 0 ){` |
|     ! 0 |  792 | `		return 0;` |
|       - |  793 | `	}` |
|    1647 |  794 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    1647 |  795 | `	pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|    1647 |  796 | `	if( pAttr ){` |
|    1647 |  797 | `		PH7_MemObjStringAppend(pAttr, pName->zString, pName->nByte);` |
|     821 |  798 | `	}` |
|    1647 |  799 | `	if( pBoundThis ){` |
|      43 |  800 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|      43 |  801 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|      43 |  802 | `		if( pAttr ){` |
|      43 |  803 | `			pAttr->x.pOther = pBoundThis;` |
|      43 |  804 | `			MemObjSetType(pAttr, MEMOBJ_OBJ);` |
|      43 |  805 | `			pBoundThis->iRef++; /* keep the bound object alive for the closure's lifetime */` |
|      21 |  806 | `		}` |
|      21 |  807 | `	}` |
|    1647 |  808 | `	if( pScope && pScope->nByte ){` |
|      71 |  809 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      71 |  810 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|      71 |  811 | `		if( pAttr ){` |
|      71 |  812 | `			PH7_MemObjStringAppend(pAttr, pScope->zString, pScope->nByte);` |
|      35 |  813 | `		}` |
|      35 |  814 | `	}` |
|    1647 |  815 | `	if( pBoundThis \|\| (pScope && pScope->nByte) ){` |
|       - |  816 | `		/* Mark bound/static FCC closures so VmClosureUnwrap can skip the $__this/$__scope` |
|       - |  817 | `		 * lookups on the hot plain-closure dispatch path. */` |
|      71 |  818 | `		pObj->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      35 |  819 | `	}` |
|    1647 |  820 | `	return pObj;` |
|     826 |  821 | `}` |
|       - |  822 | `/*` |
|       - |  823 | ` * Exported wrapper around VmCreateClosure for builtin libraries outside this` |
|       - |  824 | ` * file (ReflectionFunction::getClosure / ReflectionMethod::getClosure).` |
|       - |  825 | ` */` |
|       6 |  826 | `PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm, const SyString *pName,` |
|       - |  827 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       1 |  828 | `{` |
|       7 |  829 | `	return VmCreateClosure(&(*pVm), pName, pBoundThis, pScope);` |
|       1 |  830 | `}` |
|       - |  831 | `/*` |
|       - |  832 | ` * Exported wrapper around the typed/readonly property store enforcement for` |
|       - |  833 | ` * ReflectionProperty::setValue (vm_builtin_reflection.c). Same contract:` |
|       - |  834 | ` * SXRET_OK (value possibly coerced in place), PH7_EXCEPTION, or PH7_ABORT.` |
|       - |  835 | ` */` |
|       4 |  836 | `PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|       1 |  837 | `{` |
|       5 |  838 | `	return VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pValue,0);` |
|       1 |  839 | `}` |
|       - |  840 | `/*` |
|       - |  841 | ` * Exported reference-table probe for ReflectionReference::fromArrayElement` |
|       - |  842 | ` * (vm_builtin_reflection.c). Returns the number of links (frame variables +` |
|       - |  843 | ` * array entries) attached to the slot's reference record, 0 when the slot` |
|       - |  844 | ` * has none — an array element is a PHP reference when this is >= 2.` |
|       - |  845 | ` */` |
|       6 |  846 | `PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx)` |
|       1 |  847 | `{` |
|       7 |  848 | `	VmRefObj *pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       7 |  849 | `	if( pRef == 0 ){` |
|     ! 0 |  850 | `		return 0;` |
|       - |  851 | `	}` |
|       7 |  852 | `	return (int)(SySetUsed(&pRef->aReference) + SySetUsed(&pRef->aArrEntries));` |
|       4 |  853 | `}` |
|       - |  854 | `/*` |
|       - |  855 | `` * First-class callable over an arbitrary callable VALUE: `($expr)(...)`.`` |
|       - |  856 | ` * Normalize a validated callable value into a fresh Closure, reusing VmCreateClosure (the same` |
|       - |  857 | ` * object the method/static first-class-callable paths mint, so dispatch round-trips identically` |
|       - |  858 | ` * via VmClosureUnwrap). Handles the three remaining callable shapes a value can hold:` |
|       - |  859 | ` *   - a function-NAME string          -> plain closure ($__fn = name)` |
|       - |  860 | ` *   - a [target, method] array callable -> bound (object target -> $this) / static (class-name` |
|       - |  861 | ` *     target -> scope) closure, mirroring the [obj,m]/[class,m] decode in PH7_VmIsCallable` |
|       - |  862 | ` *   - an __invoke object               -> closure bound to the object's __invoke` |
|       - |  863 | ` * An existing Closure returns 0 here (it is already a Closure — the caller keeps it as-is), so this` |
|       - |  864 | ` * stays idempotent even for a direct caller. Returns the fresh instance (iRef == 0; caller takes the` |
|       - |  865 | ` * reference) or 0 if the value is an existing Closure / not a normalizable callable / on OOM — in` |
|       - |  866 | ` * which case the caller leaves the value untouched (graceful degradation). This is the generic` |
|       - |  867 | ` * "callable value -> Closure" primitive: the body is FCC-agnostic and self-contained, so the future` |
|       - |  868 | ` * Closure::bind/fromCallable work (Increment 2) can call it directly.` |
|       - |  869 | ` */` |
|      66 |  870 | `PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue)` |
|       3 |  871 | `{` |
|       - |  872 | `	/* A Closure is already callable; never double-wrap it (this also stops the __invoke branch` |
|       - |  873 | `	 * below from binding a closure to its OWN __invoke). The OP_LOAD_FCC caller intercepts a` |
|       - |  874 | `	 * Closure first too, but guarding here keeps the primitive safe for a direct Increment-2 caller. */` |
|      69 |  875 | `	if( VmValueIsClosure(pVm, pValue) ){` |
|     ! 0 |  876 | `		return 0;` |
|       - |  877 | `	}` |
|      69 |  878 | `	if( !PH7_VmIsCallable(pVm, pValue, TRUE) ){` |
|     ! 0 |  879 | `		return 0;` |
|       - |  880 | `	}` |
|      69 |  881 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  882 | `		SyString sName;` |
|      43 |  883 | `		SyStringInitFromBuf(&sName, SyBlobData(&pValue->sBlob), SyBlobLength(&pValue->sBlob));` |
|      43 |  884 | `		return VmCreateClosure(pVm, &sName, 0, 0);` |
|       - |  885 | `	}` |
|      27 |  886 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  887 | `		/* [target, method] — same two-slot decode PH7_VmIsCallable uses to validate it. */` |
|      19 |  888 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - |  889 | `		ph7_value *pTarget, *pMeth;` |
|       - |  890 | `		SyString sName;` |
|      19 |  891 | `		if( pMap->nEntry != 2 ){` |
|     ! 0 |  892 | `			return 0;` |
|       - |  893 | `		}` |
|      19 |  894 | `		pTarget = (ph7_value *)SySetAt(&pVm->aMemObj, pMap->pFirst->nValIdx);` |
|      19 |  895 | `		pMeth   = (ph7_value *)SySetAt(&pVm->aMemObj, pMap->pFirst->pPrev->nValIdx);` |
|      18 |  896 | `		if( pTarget == 0 \|\| pMeth == 0 \|\| (pMeth->iFlags & MEMOBJ_STRING) == 0` |
|      19 |  897 | `			\|\| SyBlobLength(&pMeth->sBlob) == 0 ){` |
|     ! 0 |  898 | `			return 0;` |
|       - |  899 | `		}` |
|      19 |  900 | `		SyStringInitFromBuf(&sName, SyBlobData(&pMeth->sBlob), SyBlobLength(&pMeth->sBlob));` |
|      19 |  901 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      13 |  902 | `			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      13 |  903 | `			return VmCreateClosure(pVm, &sName, pBoundThis, &pBoundThis->pClass->sName);` |
|     ! 0 |  904 | `		}else{` |
|       - |  905 | `			/* [class-name, method] static callable -> bind the resolved scope. A runtime array` |
|       - |  906 | `			 * callable carries a concrete class name (never self/static/parent), so a plain class` |
|       - |  907 | ``			 * lookup is correct — unlike the syntactic `C::m(...)` path, which must resolve`` |
|       - |  908 | `			 * self/static/parent via VmFccResolveScope. Matches PH7_VmIsCallable's own decode. */` |
|       7 |  909 | `			ph7_class *pScopeCls = PH7_VmExtractClassFromValue(pVm, pTarget);` |
|       7 |  910 | `			return pScopeCls ? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;` |
|       - |  911 | `		}` |
|       - |  912 | `	}` |
|       9 |  913 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  914 | `		/* __invoke object (a real Closure is intercepted by the caller before this point). */` |
|       9 |  915 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|       - |  916 | `		SyString sInvoke;` |
|       9 |  917 | `		SyStringInitFromBuf(&sInvoke, "__invoke", sizeof("__invoke") - 1);` |
|       9 |  918 | `		return VmCreateClosure(pVm, &sInvoke, pObj, &pObj->pClass->sName);` |
|       - |  919 | `	}` |
|       - |  920 | `	/* Unreachable in practice — the PH7_VmIsCallable gate admits only string/array/object, all` |
|       - |  921 | `	 * handled above; kept to satisfy the non-void return path. */` |
|     ! 0 |  922 | `	return 0;` |
|      36 |  923 | `}` |
|       - |  924 | `/*` |
|       - |  925 | ` * Return a fresh Closure instance (iRef==0 from create/clone) as a builtin result, taking the` |
|       - |  926 | ` * one reference into pCtx->pRet — mirrors the OP_LOAD_FCC store convention.` |
|       - |  927 | ` */` |
|      58 |  928 | `static int VmClosureResult(ph7_context *pCtx, ph7_class_instance *pClosure)` |
|       2 |  929 | `{` |
|      60 |  930 | `	if( pClosure == 0 ){` |
|     ! 0 |  931 | `		ph7_result_null(pCtx);` |
|     ! 0 |  932 | `		return PH7_OK;` |
|       - |  933 | `	}` |
|      60 |  934 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      60 |  935 | `	pClosure->iRef++;` |
|      60 |  936 | `	pCtx->pRet->x.pOther = pClosure;` |
|      60 |  937 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|      60 |  938 | `	return PH7_OK;` |
|      31 |  939 | `}` |
|       - |  940 | `/*` |
|       - |  941 | ` * Overwrite a Closure clone's $__this (bound object, or 0 to unbind) and, when pScope != 0, its` |
|       - |  942 | ` * $__scope (the class-name string; pScope->nByte==0 clears it). pScope==0 leaves $__scope as the` |
|       - |  943 | ` * clone inherited it (PHP's "static" = keep-scope). Refreshes VM_INSTANCE_FCC_BOUND from the result.` |
|       - |  944 | ` * The clone inherited a ref on the original $__this (PH7_CloneClassInstance copies via MemObjStore);` |
|       - |  945 | ` * this drops that and takes one on pNewThis.` |
|       - |  946 | ` */` |
|      42 |  947 | `static void VmClosureRebind(ph7_class_instance *pClone,` |
|       - |  948 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|       2 |  949 | `{` |
|       - |  950 | `	SyString sAttr;` |
|       - |  951 | `	ph7_value *pThisAttr, *pScopeAttr;` |
|      44 |  952 | `	int bBound = 0;` |
|      44 |  953 | `	SyStringInitFromBuf(&sAttr, "__this", 6);` |
|      44 |  954 | `	pThisAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      44 |  955 | `	if( pThisAttr ){` |
|       - |  956 | `		/* PH7_MemObjRelease already drops the cloned-in object's refcount for an OBJ value —` |
|       - |  957 | `		 * do NOT also decrement by hand (that double-frees the original bound object). */` |
|      44 |  958 | `		PH7_MemObjRelease(pThisAttr);` |
|      44 |  959 | `		if( pNewThis ){` |
|      36 |  960 | `			pThisAttr->x.pOther = pNewThis;` |
|      36 |  961 | `			MemObjSetType(pThisAttr, MEMOBJ_OBJ);` |
|      36 |  962 | `			pNewThis->iRef++;` |
|      17 |  963 | `		}` |
|      21 |  964 | `	}` |
|      44 |  965 | `	if( pScope ){` |
|      29 |  966 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      29 |  967 | `		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      29 |  968 | `		if( pScopeAttr ){` |
|      29 |  969 | `			PH7_MemObjRelease(pScopeAttr);` |
|      29 |  970 | `			if( pScope->nByte ){` |
|      29 |  971 | `				PH7_MemObjStringAppend(pScopeAttr, pScope->zString, pScope->nByte);` |
|      14 |  972 | `			}` |
|      14 |  973 | `		}` |
|      14 |  974 | `	}` |
|       - |  975 | `	/* Refresh the FCC_BOUND fast-path flag from the resulting $__this/$__scope. pThisAttr already` |
|       - |  976 | `	 * points at the final $__this slot; only $__scope needs a (re)fetch — the top half fetched it` |
|       - |  977 | `	 * just for pScope != 0. */` |
|      44 |  978 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      44 |  979 | `	pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      42 |  980 | `	if( (pThisAttr && (pThisAttr->iFlags & MEMOBJ_OBJ))` |
|      27 |  981 | `		\|\| (pScopeAttr && (pScopeAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeAttr->sBlob) > 0) ){` |
|      42 |  982 | `		bBound = 1;` |
|      20 |  983 | `	}` |
|      44 |  984 | `	if( bBound ){` |
|      42 |  985 | `		pClone->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      22 |  986 | `	}else{` |
|       3 |  987 | `		pClone->iFlags &= ~VM_INSTANCE_FCC_BOUND;` |
|       - |  988 | `	}` |
|      44 |  989 | `}` |
|       - |  990 | `/*` |
|       - |  991 | ` * Resolve the bindTo/bind/call $scope argument to a class-name SyString.` |
|       - |  992 | ` * Returns 1 and fills *pOut if $__scope should be replaced (pOut->nByte==0 means "clear");` |
|       - |  993 | ` * returns 0 to leave the scope unchanged (the PHP "static" sentinel / scope omitted).` |
|       - |  994 | ` */` |
|      42 |  995 | `static int VmClosureResolveScope(ph7_value *pScopeArg, SyString *pOut)` |
|       2 |  996 | `{` |
|      44 |  997 | `	if( pScopeArg == 0 ){` |
|     ! 0 |  998 | `		return 0; /* keep */` |
|       - |  999 | `	}` |
|      42 | 1000 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeArg->sBlob) == 6` |
|      29 | 1001 | `		&& SyMemcmp((const void *)SyBlobData(&pScopeArg->sBlob), (const void *)"static", 6) == 0 ){` |
|      16 | 1002 | `		return 0; /* "static" -> keep current scope */` |
|       - | 1003 | `	}` |
|      29 | 1004 | `	if( pScopeArg->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 1005 | `		SyStringInitFromBuf(pOut, 0, 0); /* unscoped */` |
|     ! 0 | 1006 | `		return 1;` |
|       - | 1007 | `	}` |
|      29 | 1008 | `	if( pScopeArg->iFlags & MEMOBJ_OBJ ){` |
|       3 | 1009 | `		ph7_class_instance *pScopeObj = (ph7_class_instance *)pScopeArg->x.pOther;` |
|       3 | 1010 | `		*pOut = pScopeObj->pClass->sName;` |
|       3 | 1011 | `		return 1;` |
|       - | 1012 | `	}` |
|      27 | 1013 | `	if( pScopeArg->iFlags & MEMOBJ_STRING ){` |
|      27 | 1014 | `		SyStringInitFromBuf(pOut, (const char *)SyBlobData(&pScopeArg->sBlob), SyBlobLength(&pScopeArg->sBlob));` |
|      27 | 1015 | `		return 1;` |
|       - | 1016 | `	}` |
|     ! 0 | 1017 | `	return 0;` |
|      23 | 1018 | `}` |
|       - | 1019 | `/*` |
|       - | 1020 | ` * Closure::bindTo($newThis, $scope='static') / Closure::bind($closure, $newThis, $scope='static').` |
|       - | 1021 | ` * Clone the receiver and rebind $this/$scope; returns the new Closure (NULL on a non-Closure` |
|       - | 1022 | ` * receiver, matching PHP's failure mode).` |
|       - | 1023 | ` */` |
|      44 | 1024 | `PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 1025 | `{` |
|      46 | 1026 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1027 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|       - | 1028 | `	ph7_value *pNewThisArg;` |
|       - | 1029 | `	SyString sScope;` |
|      46 | 1030 | `	const SyString *pScopePtr = 0;` |
|      46 | 1031 | `	if( nArg < 2 \|\| !VmValueIsClosure(pVm, apArg[0]) ){` |
|     ! 0 | 1032 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1033 | `		return PH7_OK;` |
|       - | 1034 | `	}` |
|      46 | 1035 | `	pClosure = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      46 | 1036 | `	pNewThisArg = apArg[1];` |
|      46 | 1037 | `	if( pNewThisArg->iFlags & MEMOBJ_NULL ){` |
|       9 | 1038 | `		pNewThis = 0;` |
|      42 | 1039 | `	}else if( pNewThisArg->iFlags & MEMOBJ_OBJ ){` |
|      38 | 1040 | `		pNewThis = (ph7_class_instance *)pNewThisArg->x.pOther;` |
|      20 | 1041 | `	}else{` |
|     ! 0 | 1042 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1043 | `			"Closure::bindTo(): Argument #1 ($newThis) must be of type ?object");` |
|       - | 1044 | `	}` |
|      46 | 1045 | `	if( pNewThis ){` |
|       - | 1046 | `		/* php refuses to bind an instance to a static closure: warning + null */` |
|       - | 1047 | `		SyString sAttr;` |
|       - | 1048 | `		ph7_value *pFn;` |
|      38 | 1049 | `		SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|      38 | 1050 | `		pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      38 | 1051 | `		if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){` |
|      38 | 1052 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      38 | 1053 | `			if( pEntry && (((ph7_vm_func *)pEntry->pUserData)->iFlags & VM_FUNC_STATIC_CL) ){` |
|       3 | 1054 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1055 | `					"Cannot bind an instance to a static closure, this will be an error in PHP 9");` |
|       3 | 1056 | `				ph7_result_null(pCtx);` |
|       3 | 1057 | `				return PH7_OK;` |
|       - | 1058 | `			}` |
|      17 | 1059 | `		}` |
|      17 | 1060 | `	}` |
|      44 | 1061 | `	if( VmClosureResolveScope((nArg > 2) ? apArg[2] : 0, &sScope) ){` |
|      29 | 1062 | `		pScopePtr = &sScope;` |
|      14 | 1063 | `	}` |
|      44 | 1064 | `	pClone = PH7_CloneClassInstance(pClosure);` |
|      44 | 1065 | `	if( pClone == 0 ){` |
|     ! 0 | 1066 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1067 | `		return PH7_OK;` |
|       - | 1068 | `	}` |
|      44 | 1069 | `	VmClosureRebind(pClone, pNewThis, pScopePtr);` |
|      44 | 1070 | `	return VmClosureResult(pCtx, pClone);` |
|      24 | 1071 | `}` |
|       - | 1072 | `/*` |
|       - | 1073 | ` * Closure::fromCallable($callable) — normalize any callable value to a Closure (reuses the` |
|       - | 1074 | ` * VmFccWrapValue primitive); idempotent on a Closure; TypeError on a non-callable.` |
|       - | 1075 | ` */` |
|      18 | 1076 | `PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 1077 | `{` |
|      19 | 1078 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1079 | `	ph7_class_instance *pClosure;` |
|      19 | 1080 | `	if( nArg < 1 ){` |
|     ! 0 | 1081 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1082 | `			"Closure::fromCallable() expects exactly 1 argument, 0 given");` |
|       - | 1083 | `	}` |
|      19 | 1084 | `	if( VmValueIsClosure(pVm, apArg[0]) ){` |
|       3 | 1085 | `		ph7_result_value(pCtx, apArg[0]); /* already a Closure: idempotent */` |
|       3 | 1086 | `		return PH7_OK;` |
|       - | 1087 | `	}` |
|      17 | 1088 | `	pClosure = VmFccWrapValue(pVm, apArg[0]);` |
|      17 | 1089 | `	if( pClosure == 0 ){` |
|     ! 0 | 1090 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1091 | `			"Closure::fromCallable(): Argument #1 ($callback) is not a valid callback");` |
|       - | 1092 | `	}` |
|      17 | 1093 | `	return VmClosureResult(pCtx, pClosure);` |
|      10 | 1094 | `}` |
|       - | 1095 | `/*` |
|       - | 1096 | ` * Fiber::suspend($value = null) — static method.` |
|       - | 1097 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|       - | 1098 | ` */` |
|     358 | 1099 | `PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1100 | `{` |
|     363 | 1101 | `	ph7_vm *pVm = pCtx->pVm;` |
|     363 | 1102 | `	if( pVm->pActiveCtx == 0 ){` |
|     ! 0 | 1103 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1104 | `			"Cannot suspend outside of a fiber");` |
|       - | 1105 | `	}` |
|       - | 1106 | `	/* Stage 4 scoped divergence: the trampoline only makes PHP->PHP CALLs` |
|       - | 1107 | `	 * iterative. Every OTHER re-entry runs on a fresh native VmByteCodeExec` |
|       - | 1108 | `	 * activation (nVmExecDepth bumped) that PH7_SUSPEND cannot unwind across` |
|       - | 1109 | `	 * without real coroutine stacks (BYTECODE.md §2.4): a C->PHP callback` |
|       - | 1110 | `	 * (usort/array_map/preg_replace_callback comparator), and — because fibers` |
|       - | 1111 | `	 * use the LEGACY (non-inline) try/catch machinery — a suspend inside a` |
|       - | 1112 | `	 * fiber's catch/finally body, a match/switch arm, or eval()/include()'d` |
|       - | 1113 | `	 * code, all of which run via VmLocalExec. php does all of these via full` |
|       - | 1114 | `	 * native-stack switching; PHL raises a catchable FiberError instead of the` |
|       - | 1115 | `	 * old silent corruption. A suspend in a fiber's try BODY (not catch/finally)` |
|       - | 1116 | `	 * runs in the main dispatch loop and parks normally. A recorded` |
|       - | 1117 | `	 * residual; making the catch/finally case work needs fibers on the inline` |
|       - | 1118 | `	 * try machinery (the generator ROOT C path), a follow-up. */` |
|     363 | 1119 | `	if( pVm->nVmExecDepth != pVm->pActiveCtx->nBodyExecDepth ){` |
|       6 | 1120 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1121 | `			"Cannot suspend across an internal call boundary");` |
|       - | 1122 | `	}` |
|     359 | 1123 | `	if( nArg > 0 ){` |
|     359 | 1124 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|     182 | 1125 | `	}else{` |
|     ! 0 | 1126 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|       - | 1127 | `	}` |
|     359 | 1128 | `	return PH7_SUSPEND;` |
|     184 | 1129 | `}` |
|       - | 1130 | `/*` |
|       - | 1131 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|       - | 1132 | ` * Actual resolution is deferred to start() so that overload selection` |
|       - | 1133 | ` * and closure-environment binding happen with the correct argument context.` |
|       - | 1134 | ` */` |
|     260 | 1135 | `PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1136 | `{` |
|       - | 1137 | `	ph7_class_instance *pThis;` |
|       - | 1138 | `	ph7_value *pAttr;` |
|       - | 1139 | `	SyString sAttrName;` |
|     265 | 1140 | `	if( nArg < 2 ){` |
|     ! 0 | 1141 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1142 | `			"Fiber::__construct() expects a callable argument");` |
|       - | 1143 | `	}` |
|     265 | 1144 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1145 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1146 | `			"Fiber::__construct(): invalid $this");` |
|       - | 1147 | `	}` |
|     265 | 1148 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     265 | 1149 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|     ! 0 | 1150 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1151 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|       - | 1152 | `	}` |
|       - | 1153 | `	/* Basic validation: callable must be a string or closure (object) */` |
|     265 | 1154 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|     ! 0 | 1155 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1156 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|       - | 1157 | `	}` |
|       - | 1158 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|     265 | 1159 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     265 | 1160 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     265 | 1161 | `	if( pAttr ){` |
|     265 | 1162 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|     130 | 1163 | `	}` |
|     265 | 1164 | `	return PH7_OK;` |
|     135 | 1165 | `}` |
|       - | 1166 | `/*` |
|       - | 1167 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|       - | 1168 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|       - | 1169 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|       - | 1170 | ` * so that start() can bind it as $this for the closure environment.` |
|       - | 1171 | ` */` |
|     258 | 1172 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|       - | 1173 | `	ph7_class_instance **ppThis)` |
|       5 | 1174 | `{` |
|     263 | 1175 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1176 | `	ph7_value *pCallable;` |
|       - | 1177 | `	SyString sAttrName;` |
|     263 | 1178 | `	*ppThis = 0;` |
|     263 | 1179 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     263 | 1180 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|     263 | 1181 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|     ! 0 | 1182 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|     ! 0 | 1183 | `		return 0;` |
|       - | 1184 | `	}` |
|     263 | 1185 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|       - | 1186 | `		/* String callable — look up in user functions with overload support */` |
|       - | 1187 | `		SyString sName;` |
|       - | 1188 | `		SyHashEntry *pEntry;` |
|       - | 1189 | `		ph7_vm_func *pFunc;` |
|     232 | 1190 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|     232 | 1191 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|     232 | 1192 | `		if( pEntry == 0 ){` |
|     ! 0 | 1193 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|     ! 0 | 1194 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|     ! 0 | 1195 | `			return 0;` |
|       - | 1196 | `		}` |
|     232 | 1197 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     232 | 1198 | `		return pFunc;` |
|     ! 0 | 1199 | `	}else{` |
|      34 | 1200 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|       - | 1201 | `		ph7_class_method *pMethod;` |
|      34 | 1202 | `		if( VmValueIsClosure(pVm, pCallable) ){` |
|       - | 1203 | `			/* A real Closure object: unwrap to its underlying callable name (the single` |
|       - | 1204 | `			 * source of truth, VmClosureUnwrap) and resolve that function. Its captured` |
|       - | 1205 | ``			 * environment (including any `$this`) rides along in the named function's`` |
|       - | 1206 | `			 * aClosureEnv, installed by VmFiberSetupFrame, so *ppThis stays 0. */` |
|       - | 1207 | `			ph7_value sName;` |
|      34 | 1208 | `			SyHashEntry *pEntry = 0;` |
|      34 | 1209 | `			PH7_MemObjInit(pVm, &sName);` |
|      34 | 1210 | `			if( VmClosureUnwrap(pVm, pCallable, &sName) == SXRET_OK ){` |
|      34 | 1211 | `				pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&sName.sBlob), SyBlobLength(&sName.sBlob));` |
|      15 | 1212 | `			}` |
|      34 | 1213 | `			PH7_MemObjRelease(&sName);` |
|      34 | 1214 | `			if( pEntry ){` |
|       - | 1215 | `				/* A BOUND closure parked its $this in the pClosureThis transient` |
|       - | 1216 | `				 * (VmClosureUnwrap): consume it as the fiber's $this — it wins over` |
|       - | 1217 | `				 * the creation-time env capture (VmFiberSetupFrame's skip). *ppThis` |
|       - | 1218 | `				 * is a borrow (the closure's $__this attr keeps the object alive` |
|       - | 1219 | `				 * through $__callable), so drop the parked ref. The scope transient` |
|       - | 1220 | `				 * is cleared alongside: fiber bodies don't model pBoundScope` |
|       - | 1221 | `				 * visibility (recorded residual), and a stale transient would` |
|       - | 1222 | `				 * poison the next OP_CALL's frame. */` |
|      34 | 1223 | `				if( pVm->pClosureThis ){` |
|     ! 0 | 1224 | `					*ppThis = pVm->pClosureThis;` |
|     ! 0 | 1225 | `					PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1226 | `					pVm->pClosureThis = 0;` |
|     ! 0 | 1227 | `				}` |
|      34 | 1228 | `				pVm->pClosureScope = 0;` |
|      34 | 1229 | `				return (ph7_vm_func *)pEntry->pUserData;` |
|       - | 1230 | `			}` |
|     ! 0 | 1231 | `			if( pVm->pClosureThis ){` |
|       - | 1232 | `				/* Failed resolution: drop the parked transient so it neither leaks` |
|       - | 1233 | `				 * nor poisons the next call. */` |
|     ! 0 | 1234 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1235 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 1236 | `			}` |
|     ! 0 | 1237 | `			pVm->pClosureScope = 0;` |
|     ! 0 | 1238 | `			PH7_VmThrowException(pCtx, "FiberError", "Fiber callable closure could not be resolved");` |
|     ! 0 | 1239 | `			return 0;` |
|       - | 1240 | `		}` |
|       - | 1241 | `		/* Object callable — resolve __invoke method */` |
|     ! 0 | 1242 | `		pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 1243 | `			sizeof("__invoke") - 1);` |
|     ! 0 | 1244 | `		if( pMethod == 0 ){` |
|     ! 0 | 1245 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1246 | `				"Fiber callable object has no __invoke method");` |
|     ! 0 | 1247 | `			return 0;` |
|       - | 1248 | `		}` |
|     ! 0 | 1249 | `		*ppThis = pClosure;` |
|     ! 0 | 1250 | `		return &pMethod->sFunc;` |
|       - | 1251 | `	}` |
|     134 | 1252 | `}` |
|       - | 1253 | `/*` |
|       - | 1254 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|       - | 1255 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|       - | 1256 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|       - | 1257 | ` */` |
|       - | 1258 | `/*` |
|       - | 1259 | ` * Enforce one formal parameter's declared type on an argument being installed.` |
|       - | 1260 | ` * THE single implementation of the per-argument check, shared by the` |
|       - | 1261 | ` * generator/fiber initial-frame binder below (band A #2) and both OP_CALL` |
|       - | 1262 | ` * install paths (named-map and positional — they carried two verbatim copies` |
|       - | 1263 | ` * until the §7.1(f) fold): union types via VmCoerceToUnion, class and` |
|       - | 1264 | ` * pseudo types (VmCheckPseudoType + VmResolveTypeClass + instanceof, so` |
|       - | 1265 | ``  * interfaces/abstract classes and self/parent resolve), the bare `object` `` |
|       - | 1266 | ` * hint, and scalars via VmEnforceScalarType (weak-mode coercion in place,` |
|       - | 1267 | ` * strict rejection otherwise) — with the VM_FUNC_ARG_NULLABLE guard letting` |
|       - | 1268 | `` * null through for `?type` and implicit-nullable `Type $x = null` params,`` |
|       - | 1269 | ` * and whole-real materialization on a mask match.` |
|       - | 1270 | ` * Returns SXRET_OK (value possibly coerced) or the VmThrowTypeErrorForArg` |
|       - | 1271 | ` * status for the caller to route — normalized so an INLINE-caught throw` |
|       - | 1272 | ` * (VmThrowInline records only a pc-redirect and reports SXRET_OK) still` |
|       - | 1273 | ` * comes back as PH7_EXCEPTION: the binding must stop; the OP_CALL sites` |
|       - | 1274 | ` * force rc = PH7_EXCEPTION on any non-OK status anyway, and the generator` |
|       - | 1275 | ` * block's PH7_INLINE_RESUME_BREAK consumes the redirect.` |
|       - | 1276 | ` */` |
|     162 | 1277 | `static sxi32 VmGenArgThrowStatus(ph7_vm *pVm, sxi32 rcThrow)` |
|       5 | 1278 | `{` |
|     167 | 1279 | `	if( rcThrow == SXRET_OK && pVm->pInlineInstr ){` |
|     ! 0 | 1280 | `		return PH7_EXCEPTION;` |
|       - | 1281 | `	}` |
|     167 | 1282 | `	return rcThrow;` |
|      86 | 1283 | `}` |
| 1632354 | 1284 | `PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_vm_func_arg *pFormal,` |
|       - | 1285 | `	sxu32 nArgPos, ph7_value *pVal, int bStrict, ph7_class *pSelfHint)` |
|       5 | 1286 | `{` |
| 1632359 | 1287 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|     147 | 1288 | `		if( VmCoerceToUnion(pVm,pVal,&pFormal->aUnionAlts,` |
|     152 | 1289 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,bStrict) != SXRET_OK ){` |
|       - | 1290 | `			const char *zGiven;` |
|      26 | 1291 | `			const char *zExpected = "union";` |
|       - | 1292 | `			char zBuf[128];` |
|       - | 1293 | `			char zTypeBuf[128];` |
|      26 | 1294 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      13 | 1295 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      21 | 1296 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      10 | 1297 | `				zGiven = "null";` |
|       6 | 1298 | `			}else{` |
|       6 | 1299 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|       - | 1300 | `			}` |
|      26 | 1301 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|      26 | 1302 | `				zExpected = VmSyStringToCStr(&pFormal->sTypeName,zTypeBuf,sizeof(zTypeBuf));` |
|      11 | 1303 | `			}` |
|      37 | 1304 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      11 | 1305 | `				&pFormal->sName,zExpected,zGiven));` |
|       - | 1306 | `		}` |
|      79 | 1307 | `		return SXRET_OK;` |
|       - | 1308 | `	}` |
| 1632256 | 1309 | `	if( pFormal->nType == 0` |
|  823739 | 1310 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
| 1617743 | 1311 | `		return SXRET_OK;` |
|       - | 1312 | `	}` |
|   14523 | 1313 | `	if( pFormal->nType == SXU32_HIGH ){` |
|       - | 1314 | `		/* Class or pseudo type */` |
|     571 | 1315 | `		SyString *pName = &pFormal->sClass;` |
|       - | 1316 | `		ph7_class *pClass;` |
|     571 | 1317 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|     571 | 1318 | `		if( rcPseudo == 0 ){` |
|       - | 1319 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       7 | 1320 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       2 | 1321 | `				&pFormal->sName,` |
|       2 | 1322 | `				VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|       2 | 1323 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1324 | `		}` |
|     567 | 1325 | `		pClass = (rcPseudo == 1) ? 0 : VmResolveTypeClass(&(*pVm),pName,pSelfHint);` |
|     567 | 1326 | `		if( pClass ){` |
|     687 | 1327 | `			int bBad = !((pVal->iFlags & MEMOBJ_OBJ)` |
|     341 | 1328 | `				&& PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass));` |
|     357 | 1329 | `			if( bBad ){` |
|       - | 1330 | `				char zTypeBuf[128],zGivenBuf[128];` |
|      71 | 1331 | `				return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      17 | 1332 | `					&pFormal->sName,` |
|      34 | 1333 | `					VmSyStringToCStr(&pClass->sName,zTypeBuf,sizeof(zTypeBuf)),` |
|      17 | 1334 | `					VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1335 | `			}` |
|     159 | 1336 | `		}` |
|     533 | 1337 | `		return SXRET_OK;` |
|       - | 1338 | `	}` |
|   13957 | 1339 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|       - | 1340 | `		char zGivenBuf[128];` |
|     145 | 1341 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|      67 | 1342 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       8 | 1343 | `				&pFormal->sName,"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1344 | `		}` |
|     129 | 1345 | `		if( VmEnforceScalarType(pVal,pFormal->nType,bStrict) != SXRET_OK ){` |
|       - | 1346 | `			char zTypeBuf[128];` |
|     128 | 1347 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      41 | 1348 | `				&pFormal->sName,` |
|      41 | 1349 | `				VmScalarTypeName(pFormal->nType,&pFormal->sTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      41 | 1350 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1351 | `		}` |
|      24 | 1352 | `	}else{` |
|       - | 1353 | `		/* Mask matched — an int param accepting a whole-real materializes` |
|       - | 1354 | `		 * it (php: g(1.0) into int $x is int(1)). */` |
|   13817 | 1355 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|       - | 1356 | `	}` |
|   13859 | 1357 | `	return SXRET_OK;` |
|  816507 | 1358 | `}` |
|     620 | 1359 | `PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|       - | 1360 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg,` |
|       - | 1361 | `	int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg)` |
|       5 | 1362 | `{` |
|     625 | 1363 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|       - | 1364 | `	ph7_vm_func_arg *aFormalArg;` |
|       - | 1365 | `	sxu32 nFormal, n;` |
|     625 | 1366 | `	sxu32 nReqGF = 0, nNonVarGF = 0; /* too-few-args watermark (0 = exempt) */` |
|       - | 1367 | `	VmSlot sSlot;` |
|       - | 1368 | `	sxi32 rc;` |
|       - | 1369 | `	/* Install $this for closure/method callables */` |
|     625 | 1370 | `	if( pClosureThis ){` |
|       - | 1371 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      31 | 1372 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      31 | 1373 | `		if( pObj ){` |
|      31 | 1374 | `			pObj->x.pOther = pClosureThis;` |
|      31 | 1375 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      31 | 1376 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      14 | 1377 | `		}` |
|      14 | 1378 | `	}` |
|       - | 1379 | `	/* Install static variables */` |
|     625 | 1380 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|       - | 1381 | `		ph7_vm_func_static_var *aStatic;` |
|       - | 1382 | `		ph7_value *pVal;` |
|     ! 0 | 1383 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|     ! 0 | 1384 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|     ! 0 | 1385 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|     ! 0 | 1386 | `			if( pVal ){` |
|     ! 0 | 1387 | `				sSlot.pUserData = 0;` |
|     ! 0 | 1388 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|     ! 0 | 1389 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|     ! 0 | 1390 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|     ! 0 | 1391 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|     ! 0 | 1392 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);` |
|     ! 0 | 1393 | `				}` |
|     ! 0 | 1394 | `			}` |
|     ! 0 | 1395 | `		}` |
|     ! 0 | 1396 | `	}` |
|       - | 1397 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|     625 | 1398 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     625 | 1399 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       - | 1400 | `	/* Actual call arity for func_num_args()/func_get_args() (band A #4) */` |
|     625 | 1401 | `	pExecCtx->pFrame->nActualArgs = nArg;` |
|       - | 1402 | `	{` |
|       - | 1403 | `		/* Too-few-arguments watermark — checked per formal INSIDE the install` |
|       - | 1404 | `		 * loop below, after the passed args' type checks, matching php's` |
|       - | 1405 | `		 * RECV order (a type error on a passed argument beats the count` |
|       - | 1406 | `		 * error). Generators throw at the g(...) call site (message embeds` |
|       - | 1407 | `		 * it); fibers at Fiber::start() (php omits the call-site segment).` |
|       - | 1408 | `		 * Hosted builtin FUNCTIONS self-check; hosted-class methods get` |
|       - | 1409 | `		 * php's ZPP wording. */` |
|     625 | 1410 | `	if( (pFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){` |
|     625 | 1411 | `		nReqGF = VmFuncRequiredArgCount(pFunc,&nNonVarGF);` |
|     310 | 1412 | `	}` |
|       - | 1413 | `	}` |
|     699 | 1414 | `	for( n = 0; n < nFormal; n++ ){` |
|       - | 1415 | `		ph7_value *pObj;` |
|     102 | 1416 | `		if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       - | 1417 | `			/* Variadic formal: collect this and every remaining actual into a` |
|       - | 1418 | `			 * fresh array (php semantics — pre-fix nothing collected here, so a` |
|       - | 1419 | ``			 * `function g(int ...$xs)` generator saw a bare scalar in $xs).`` |
|       - | 1420 | `			 * Per-element checks mirror OP_CALL's variadic install: union via` |
|       - | 1421 | ``			 * the shared helper, scalar coerce/TypeError, `object` hint; a`` |
|       - | 1422 | `			 * class-typed variadic element is (like OP_CALL) not checked. */` |
|       5 | 1423 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       5 | 1424 | `			if( pObj ){` |
|       - | 1425 | `				sxu32 nVariadicIdx;` |
|       - | 1426 | `				ph7_hashmap *pMap;` |
|       - | 1427 | `				sxu32 k;` |
|       5 | 1428 | `				PH7_MemObjToHashmap(pObj);` |
|       - | 1429 | `				/* Capture the slot index now: PH7_HashmapInsert can reallocate` |
|       - | 1430 | `				 * pVm->aMemObj and dangle pObj (same hazard as OP_CALL's path). */` |
|       5 | 1431 | `				nVariadicIdx = pObj->nIdx;` |
|       5 | 1432 | `				pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      11 | 1433 | `				for( k = n; k < (sxu32)nArg; k++ ){` |
|       9 | 1434 | `					if( ((aFormalArg[n].iFlags & VM_FUNC_ARG_UNION)` |
|       7 | 1435 | `					   \|\| (aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH)) ){` |
|       7 | 1436 | `						rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[k],bStrict,pSelfHint);` |
|       7 | 1437 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1438 | `							return rc;` |
|       - | 1439 | `						}` |
|       3 | 1440 | `					}` |
|       7 | 1441 | `					PH7_HashmapInsert(pMap,0,apArg[k]);` |
|       4 | 1442 | `				}` |
|       5 | 1443 | `				sSlot.nIdx = nVariadicIdx;` |
|       5 | 1444 | `				sSlot.pUserData = 0;` |
|       5 | 1445 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       2 | 1446 | `			}` |
|       5 | 1447 | `			break; /* All remaining actuals consumed */` |
|       - | 1448 | `		}` |
|      98 | 1449 | `		if( n < (sxu32)nArg ){` |
|       - | 1450 | `			/* Argument provided — install with declared-type enforcement.` |
|       - | 1451 | `			 * php binds and type-checks generator arguments EAGERLY at the` |
|       - | 1452 | `			 * g(...) call site (and fiber arguments at Fiber::start()), so the` |
|       - | 1453 | `			 * enforcement lives here, mirroring the OP_CALL install path via` |
|       - | 1454 | `			 * VmEnforceArgType (TypeError on mismatch, weak coercion in` |
|       - | 1455 | `			 * place otherwise) instead of the old silent xCast. A variadic` |
|       - | 1456 | `			 * formal collects as-is (no per-element declared-type model). */` |
|      90 | 1457 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      90 | 1458 | `			if( pObj ){` |
|      90 | 1459 | `				PH7_MemObjStore(apArg[n], pObj);` |
|      90 | 1460 | `				if( (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      90 | 1461 | `					rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,pObj,bStrict,pSelfHint);` |
|      90 | 1462 | `					if( rc != SXRET_OK ){` |
|      18 | 1463 | `						return rc;` |
|       - | 1464 | `					}` |
|      35 | 1465 | `				}` |
|      74 | 1466 | `				sSlot.nIdx = pObj->nIdx;` |
|      74 | 1467 | `				sSlot.pUserData = 0;` |
|      74 | 1468 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      39 | 1469 | `			}` |
|      45 | 1470 | `		}else if( n < nReqGF ){` |
|       - | 1471 | `			/* Required formal with no actual: php's ArgumentCountError, at` |
|       - | 1472 | `			 * this point in the install order (see the watermark comment). */` |
|       7 | 1473 | `			return VmGenArgThrowStatus(pVm,` |
|       4 | 1474 | `				(pFunc->iFlags & VM_FUNC_INTERNAL)` |
|     ! 0 | 1475 | `					? VmThrowBuiltinTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|     ! 0 | 1476 | `						(sxu32)nArg,nReqGF,nNonVarGF)` |
|       6 | 1477 | `					: VmThrowTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|       2 | 1478 | `						(sxu32)nArg,nReqGF,nNonVarGF,bCallSiteInMsg));` |
|       6 | 1479 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       - | 1480 | `			/* Default value */` |
|       6 | 1481 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       6 | 1482 | `			if( pObj ){` |
|       6 | 1483 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);` |
|       6 | 1484 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1485 | `					return rc;` |
|       - | 1486 | `				}` |
|       - | 1487 | `` 				/* A null default on an implicitly-nullable `Type $x = null` `` |
|       - | 1488 | `				 * param must stay null (php); only non-null defaults keep the` |
|       - | 1489 | `				 * legacy shaping cast. */` |
|       4 | 1490 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       6 | 1491 | `				 && !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|       3 | 1492 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|     ! 0 | 1493 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|     ! 0 | 1494 | `						if( xCast ){` |
|     ! 0 | 1495 | `							xCast(pObj);` |
|     ! 0 | 1496 | `						}` |
|     ! 0 | 1497 | `					}else{` |
|       - | 1498 | `						/* Mask matched — a const-indirected whole-real default` |
|       - | 1499 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|       3 | 1500 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|       - | 1501 | `					}` |
|       1 | 1502 | `				}` |
|       6 | 1503 | `				sSlot.nIdx = pObj->nIdx;` |
|       6 | 1504 | `				sSlot.pUserData = 0;` |
|       6 | 1505 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       2 | 1506 | `			}` |
|       2 | 1507 | `		}` |
|      41 | 1508 | `	}` |
|       - | 1509 | `	/* Install closure environment (captured variables) */` |
|     605 | 1510 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1511 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|       - | 1512 | `		ph7_value *pValue;` |
|       - | 1513 | `		sxu32 iEnv;` |
|      46 | 1514 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     100 | 1515 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|      58 | 1516 | `			pEnv = &aEnv[iEnv];` |
|      58 | 1517 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|      38 | 1518 | `				continue;` |
|       - | 1519 | `			}` |
|      20 | 1520 | `			if( pClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       5 | 1521 | `			 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|       - | 1522 | `				/* An explicit bound $this (bindTo/bind/call) wins over the` |
|       - | 1523 | `				 * creation-time captured $this, php-exact (mirrors OP_CALL). */` |
|       3 | 1524 | `				continue;` |
|       - | 1525 | `			}` |
|      21 | 1526 | `			if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|       - | 1527 | `				/* Captured by reference: link the name to the shared slot` |
|       - | 1528 | `				 * (no copy), mirroring the OP_CALL env install. */` |
|       5 | 1529 | `				if( SyHashGet(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|       7 | 1530 | `					SyHashInsert(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),` |
|       4 | 1531 | `						SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|       2 | 1532 | `				}` |
|       5 | 1533 | `				continue;` |
|       - | 1534 | `			}` |
|      17 | 1535 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|      17 | 1536 | `			if( pValue == 0 ){` |
|     ! 0 | 1537 | `				continue;` |
|       - | 1538 | `			}` |
|      17 | 1539 | `			PH7_MemObjRelease(pValue);` |
|      17 | 1540 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|      10 | 1541 | `		}` |
|      21 | 1542 | `	}` |
|     605 | 1543 | `	return SXRET_OK;` |
|     315 | 1544 | `}` |
|       - | 1545 | `/*` |
|       - | 1546 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|       - | 1547 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|       - | 1548 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|       - | 1549 | ` */` |
|     260 | 1550 | `PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1551 | `{` |
|     265 | 1552 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1553 | `	ph7_class_instance *pThis;` |
|       - | 1554 | `	ph7_class_instance *pClosureThis;` |
|       - | 1555 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 1556 | `	ph7_vm_func *pFunc;` |
|       - | 1557 | `	ph7_value sResult;` |
|       - | 1558 | `	ph7_value *pCtxAttr;` |
|       - | 1559 | `	SyString sAttrName;` |
|       - | 1560 | `	sxi32 rc;` |
|     265 | 1561 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1562 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|       - | 1563 | `	}` |
|     265 | 1564 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 1565 | `	/* Check if already started (has a __ctx) */` |
|     265 | 1566 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|     265 | 1567 | `	if( pExecCtx != 0 ){` |
|       3 | 1568 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1569 | `			"Cannot start a fiber that has already been started");` |
|       - | 1570 | `	}` |
|       - | 1571 | `	/* Resolve callable */` |
|     263 | 1572 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|     263 | 1573 | `	if( pFunc == 0 ){` |
|     ! 0 | 1574 | `		return PH7_EXCEPTION;` |
|       - | 1575 | `	}` |
|       - | 1576 | `	/* Create execution context now that we know the function */` |
|     263 | 1577 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|     263 | 1578 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 1579 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1580 | `			"Fiber::start(): out of memory");` |
|       - | 1581 | `	}` |
|       - | 1582 | `	/* Store context in $this->__ctx */` |
|     263 | 1583 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     263 | 1584 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     263 | 1585 | `	if( pCtxAttr ){` |
|     263 | 1586 | `		pCtxAttr->x.pOther = pExecCtx;` |
|     263 | 1587 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     129 | 1588 | `	}` |
|       - | 1589 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|       - | 1590 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|       - | 1591 | `	 * into the fiber's frame, not the caller's. */` |
|     263 | 1592 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|     263 | 1593 | `	pVm->pFrame = pExecCtx->pFrame;` |
|       - | 1594 | `	/* Unpack the args array and install into the frame */` |
|       - | 1595 | `	{` |
|     263 | 1596 | `		ph7_value **apValues = 0;` |
|     263 | 1597 | `		ph7_value *aStore = 0;` |
|     263 | 1598 | `		int nActual = 0;` |
|     263 | 1599 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|     263 | 1600 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 1601 | `			ph7_hashmap_node *pNode;` |
|     263 | 1602 | `			sxu32 nCount = pMap->nEntry;` |
|     263 | 1603 | `			if( nCount > 0 ){` |
|       8 | 1604 | `				sxu32 idx = 0;` |
|      11 | 1605 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       3 | 1606 | `					nCount * sizeof(ph7_value *));` |
|      11 | 1607 | `				aStore = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       3 | 1608 | `					nCount * sizeof(ph7_value));` |
|       8 | 1609 | `				if( apValues && aStore ){` |
|       8 | 1610 | `					pNode = pMap->pFirst;` |
|      16 | 1611 | `					while( pNode && idx < nCount ){` |
|       - | 1612 | `						/* Snapshot each source into stable storage: VmFiberSetupFrame reserves` |
|       - | 1613 | `						 * memory objects (VmExtractMemObj) before reading the args, which can` |
|       - | 1614 | `						 * reallocate (move) pVm->aMemObj and dangle a raw pool pointer. A` |
|       - | 1615 | `						 * shallow copy is a safe source — the referent and the heap-resident` |
|       - | 1616 | `						 * blob data survive the move (same sSafeVal idiom the hashmap inserters` |
|       - | 1617 | `						 * use); it owns nothing independently, so it needs no release. */` |
|      10 | 1618 | `						ph7_value *pSrc = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      10 | 1619 | `						if( pSrc ){` |
|      10 | 1620 | `							aStore[idx] = *pSrc;` |
|       6 | 1621 | `						}else{` |
|     ! 0 | 1622 | `							PH7_MemObjInit(pVm, &aStore[idx]);` |
|       - | 1623 | `						}` |
|      10 | 1624 | `						apValues[idx] = &aStore[idx];` |
|      10 | 1625 | `						idx++;` |
|      10 | 1626 | `						pNode = pNode->pPrev;` |
|       2 | 1627 | `					}` |
|       8 | 1628 | `					nActual = (int)idx;` |
|       3 | 1629 | `				}` |
|       3 | 1630 | `			}` |
|     129 | 1631 | `		}` |
|     263 | 1632 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues,` |
|       - | 1633 | `			0 /* weak-mode arg binding, like call_user_func */, 0,` |
|       - | 1634 | `			FALSE/*Fiber::start(): php omits the call-site segment*/);` |
|     263 | 1635 | `		if( aStore ){` |
|       8 | 1636 | `			SyMemBackendFree(&pVm->sAllocator, aStore);` |
|       3 | 1637 | `		}` |
|     263 | 1638 | `		if( apValues ){` |
|       8 | 1639 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|       3 | 1640 | `		}` |
|       - | 1641 | `	}` |
|       - | 1642 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|     263 | 1643 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|     263 | 1644 | `	pExecCtx->pFrame->pParent = 0;` |
|     263 | 1645 | `	if( rc != SXRET_OK ){` |
|       - | 1646 | `		/* Propagate the real status: a declared-type TypeError from the arg` |
|       - | 1647 | `		 * install (band A #2) must stay catchable, not become an abort. */` |
|       5 | 1648 | `		return (rc == PH7_EXCEPTION) ? PH7_EXCEPTION : PH7_ABORT;` |
|       - | 1649 | `	}` |
|     259 | 1650 | `	PH7_MemObjInit(pVm, &sResult);` |
|     259 | 1651 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|     259 | 1652 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 1653 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1654 | `		return PH7_ABORT;` |
|       - | 1655 | `	}` |
|     259 | 1656 | `	if( rc == PH7_EXCEPTION ){` |
|       6 | 1657 | `		PH7_MemObjRelease(&sResult);` |
|       6 | 1658 | `		return PH7_EXCEPTION;` |
|       - | 1659 | `	}` |
|     255 | 1660 | `	ph7_result_value(pCtx, &sResult);` |
|     255 | 1661 | `	PH7_MemObjRelease(&sResult);` |
|     255 | 1662 | `	return PH7_OK;` |
|     135 | 1663 | `}` |
|       - | 1664 | `/*` |
|       - | 1665 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|       - | 1666 | ` */` |
|     146 | 1667 | `PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1668 | `{` |
|     151 | 1669 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1670 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 1671 | `	ph7_value sResult;` |
|       - | 1672 | `	ph7_value *pResumeVal;` |
|       - | 1673 | `	sxi32 rc;` |
|     151 | 1674 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1675 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|     ! 0 | 1676 | `		return PH7_OK;` |
|       - | 1677 | `	}` |
|     151 | 1678 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|     151 | 1679 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 1680 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|     ! 0 | 1681 | `		return PH7_OK;` |
|       - | 1682 | `	}` |
|     151 | 1683 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       3 | 1684 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1685 | `			"Cannot resume a fiber that is not suspended");` |
|       - | 1686 | `	}` |
|     149 | 1687 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|     149 | 1688 | `	PH7_MemObjInit(pVm, &sResult);` |
|     149 | 1689 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|     149 | 1690 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 1691 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1692 | `		return PH7_ABORT;` |
|       - | 1693 | `	}` |
|     149 | 1694 | `	if( rc == PH7_EXCEPTION ){` |
|       3 | 1695 | `		PH7_MemObjRelease(&sResult);` |
|       3 | 1696 | `		return PH7_EXCEPTION;` |
|       - | 1697 | `	}` |
|     147 | 1698 | `	ph7_result_value(pCtx, &sResult);` |
|     147 | 1699 | `	PH7_MemObjRelease(&sResult);` |
|     147 | 1700 | `	return PH7_OK;` |
|      78 | 1701 | `}` |
|       - | 1702 | `/*` |
|       - | 1703 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|       - | 1704 | ` */` |
|      18 | 1705 | `PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1706 | `{` |
|      23 | 1707 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1708 | `	ph7_exec_ctx *pExecCtx;` |
|      23 | 1709 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1710 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1711 | `		return PH7_OK;` |
|       - | 1712 | `	}` |
|      23 | 1713 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|      23 | 1714 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 1715 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1716 | `		return PH7_OK;` |
|       - | 1717 | `	}` |
|      23 | 1718 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|     ! 0 | 1719 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 1720 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1721 | `				"Cannot get fiber return value: The fiber has not been started");` |
|       - | 1722 | `		}` |
|     ! 0 | 1723 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1724 | `			"Cannot get fiber return value: The fiber has not returned");` |
|       - | 1725 | `	}` |
|      23 | 1726 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|      23 | 1727 | `	return PH7_OK;` |
|      14 | 1728 | `}` |
|       - | 1729 | `/*` |
|       - | 1730 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|       - | 1731 | ` */` |
|       6 | 1732 | `PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 1733 | `{` |
|       - | 1734 | `	ph7_exec_ctx *pExecCtx;` |
|       7 | 1735 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       7 | 1736 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|       7 | 1737 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|       7 | 1738 | `	return PH7_OK;` |
|       4 | 1739 | `}` |
|     ! 0 | 1740 | `PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     ! 0 | 1741 | `{` |
|       - | 1742 | `	ph7_exec_ctx *pExecCtx;` |
|     ! 0 | 1743 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     ! 0 | 1744 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|     ! 0 | 1745 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|     ! 0 | 1746 | `	return PH7_OK;` |
|     ! 0 | 1747 | `}` |
|     108 | 1748 | `PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 1749 | `{` |
|       - | 1750 | `	ph7_exec_ctx *pExecCtx;` |
|     110 | 1751 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     110 | 1752 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|     110 | 1753 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|     110 | 1754 | `	return PH7_OK;` |
|      56 | 1755 | `}` |
|       6 | 1756 | `PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 1757 | `{` |
|       - | 1758 | `	ph7_exec_ctx *pExecCtx;` |
|       7 | 1759 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       7 | 1760 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|       7 | 1761 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|       7 | 1762 | `	return PH7_OK;` |
|       4 | 1763 | `}` |
|       - | 1764 | `/*` |
|       - | 1765 | ` * Fiber->__destruct() — clean up the execution context.` |
|       - | 1766 | ` */` |
|     216 | 1767 | `PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 1768 | `{` |
|     219 | 1769 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1770 | `	ph7_exec_ctx *pExecCtx;` |
|     219 | 1771 | `	if( nArg < 1 ){` |
|     ! 0 | 1772 | `		return PH7_OK;` |
|       - | 1773 | `	}` |
|     219 | 1774 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|     219 | 1775 | `	if( pExecCtx ){` |
|     219 | 1776 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|       - | 1777 | `		/* Clear the attribute so double-free is prevented */` |
|     219 | 1778 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     219 | 1779 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 1780 | `			SyString sAttrName;` |
|       - | 1781 | `			ph7_value *pAttr;` |
|     219 | 1782 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     219 | 1783 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     219 | 1784 | `			if( pAttr ){` |
|     219 | 1785 | `				PH7_MemObjRelease(pAttr);` |
|     108 | 1786 | `			}` |
|     108 | 1787 | `		}` |
|     108 | 1788 | `	}` |
|     219 | 1789 | `	return PH7_OK;` |
|     111 | 1790 | `}` |
|       - | 1791 | `/* ======================== Fiber Public API Helpers ======================== */` |
|     ! 0 | 1792 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|     ! 0 | 1793 | `{` |
|       - | 1794 | `	ph7_class_instance *pThis;` |
|     ! 0 | 1795 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|     ! 0 | 1796 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 | 1797 | `	return pThis->pClass == pVm->pFiberClass;` |
|     ! 0 | 1798 | `}` |
|     ! 0 | 1799 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 1800 | `{` |
|       - | 1801 | `	ph7_class_instance *pThis;` |
|     ! 0 | 1802 | `	ph7_class_instance *pClosureThis = 0;` |
|       - | 1803 | `	ph7_exec_ctx *pCtx;` |
|       - | 1804 | `	ph7_vm_func *pFunc;` |
|       - | 1805 | `	ph7_value *pCallable;` |
|       - | 1806 | `	ph7_value *pCtxAttr;` |
|       - | 1807 | `	SyString sAttrName;` |
|       - | 1808 | `	sxi32 rc;` |
|       - | 1809 | `	/* Must not already be started */` |
|     ! 0 | 1810 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 1811 | `	if( pCtx != 0 ){` |
|     ! 0 | 1812 | `		return SXERR_INVALID;` |
|       - | 1813 | `	}` |
|     ! 0 | 1814 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1815 | `		return SXERR_INVALID;` |
|       - | 1816 | `	}` |
|     ! 0 | 1817 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|       - | 1818 | `	/* Get the callable */` |
|     ! 0 | 1819 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     ! 0 | 1820 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 1821 | `	if( pCallable == 0 ){` |
|     ! 0 | 1822 | `		return SXERR_INVALID;` |
|       - | 1823 | `	}` |
|       - | 1824 | `	/* Resolve callable */` |
|     ! 0 | 1825 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|       - | 1826 | `		SyString sName;` |
|       - | 1827 | `		SyHashEntry *pEntry;` |
|     ! 0 | 1828 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|     ! 0 | 1829 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|     ! 0 | 1830 | `		if( pEntry == 0 ){` |
|     ! 0 | 1831 | `			return SXERR_NOTFOUND;` |
|       - | 1832 | `		}` |
|     ! 0 | 1833 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     ! 0 | 1834 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 1835 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|     ! 0 | 1836 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 1837 | `			sizeof("__invoke") - 1);` |
|     ! 0 | 1838 | `		if( pMethod == 0 ){` |
|     ! 0 | 1839 | `			return SXERR_INVALID;` |
|       - | 1840 | `		}` |
|     ! 0 | 1841 | `		pClosureThis = pClosure;` |
|     ! 0 | 1842 | `		pFunc = &pMethod->sFunc;` |
|     ! 0 | 1843 | `	}else{` |
|     ! 0 | 1844 | `		return SXERR_INVALID;` |
|       - | 1845 | `	}` |
|       - | 1846 | `	/* Create context */` |
|     ! 0 | 1847 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|     ! 0 | 1848 | `	if( pCtx == 0 ){` |
|     ! 0 | 1849 | `		return SXERR_MEM;` |
|       - | 1850 | `	}` |
|       - | 1851 | `	/* Store in __ctx */` |
|     ! 0 | 1852 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     ! 0 | 1853 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 1854 | `	if( pCtxAttr ){` |
|     ! 0 | 1855 | `		pCtxAttr->x.pOther = pCtx;` |
|     ! 0 | 1856 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     ! 0 | 1857 | `	}` |
|       - | 1858 | `	/* Set up frame with args */` |
|     ! 0 | 1859 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     ! 0 | 1860 | `	pVm->pFrame = pCtx->pFrame;` |
|     ! 0 | 1861 | `	rc = VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg,` |
|       - | 1862 | `		0 /* weak-mode arg binding (embedder entry) */, 0,` |
|       - | 1863 | `		FALSE/*embedder entry: no userland call site*/);` |
|     ! 0 | 1864 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|     ! 0 | 1865 | `	pCtx->pFrame->pParent = 0;` |
|     ! 0 | 1866 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1867 | `		return rc;` |
|       - | 1868 | `	}` |
|     ! 0 | 1869 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|     ! 0 | 1870 | `}` |
|     ! 0 | 1871 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 1872 | `{` |
|     ! 0 | 1873 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 1874 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|     ! 0 | 1875 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|     ! 0 | 1876 | `}` |
|     ! 0 | 1877 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 1878 | `{` |
|     ! 0 | 1879 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 1880 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|     ! 0 | 1881 | `}` |
|     ! 0 | 1882 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 1883 | `{` |
|     ! 0 | 1884 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 1885 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|     ! 0 | 1886 | `}` |
|     ! 0 | 1887 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 1888 | `{` |
|     ! 0 | 1889 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 1890 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|     ! 0 | 1891 | `	return &pCtx->sRetValue;` |
|     ! 0 | 1892 | `}` |
|       - | 1893 | `/* ======================== Generator Infrastructure ======================== */` |
|       - | 1894 | `/*` |
|       - | 1895 | ` * Allocate a new generator wrapper around an execution context.` |
|       - | 1896 | ` */` |
|     362 | 1897 | `PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 | 1898 | `{` |
|       - | 1899 | `	ph7_generator *pGen;` |
|     367 | 1900 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|     367 | 1901 | `	if( pGen == 0 ){` |
|     ! 0 | 1902 | `		return 0;` |
|       - | 1903 | `	}` |
|     367 | 1904 | `	SyZero(pGen, sizeof(ph7_generator));` |
|     367 | 1905 | `	pGen->pCtx = pCtx;` |
|     367 | 1906 | `	pGen->iImplicitKey = 0;` |
|     367 | 1907 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|     367 | 1908 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|       - | 1909 | `	/* Link the generator back to the exec context */` |
|     367 | 1910 | `	pCtx->pPrivate = pGen;` |
|     367 | 1911 | `	return pGen;` |
|     186 | 1912 | `}` |
|       - | 1913 | `/*` |
|       - | 1914 | ` * Release a generator and its execution context.` |
|       - | 1915 | ` */` |
|     250 | 1916 | `PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|       5 | 1917 | `{` |
|     255 | 1918 | `	if( pGen == 0 ){` |
|     ! 0 | 1919 | `		return;` |
|       - | 1920 | `	}` |
|     255 | 1921 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|     255 | 1922 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|     255 | 1923 | `	if( pGen->pCtx ){` |
|     255 | 1924 | `		pGen->pCtx->pPrivate = 0;` |
|     255 | 1925 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|     255 | 1926 | `		pGen->pCtx = 0;` |
|     125 | 1927 | `	}` |
|     255 | 1928 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|     130 | 1929 | `}` |
|       - | 1930 | `/*` |
|       - | 1931 | ` * Extract ph7_generator from a Generator class instance.` |
|       - | 1932 | ` */` |
|    4348 | 1933 | `PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|       5 | 1934 | `{` |
|       - | 1935 | `	ph7_class_instance *pThis;` |
|       - | 1936 | `	SyString sAttr;` |
|       - | 1937 | `	ph7_value *pAttr;` |
|    4353 | 1938 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1939 | `		return 0;` |
|       - | 1940 | `	}` |
|    4353 | 1941 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|    4353 | 1942 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|     ! 0 | 1943 | `		return 0;` |
|       - | 1944 | `	}` |
|    4353 | 1945 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    4353 | 1946 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    4353 | 1947 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     ! 0 | 1948 | `		return 0;` |
|       - | 1949 | `	}` |
|    4353 | 1950 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    2179 | 1951 | `}` |
|       - | 1952 | `/*` |
|       - | 1953 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|       - | 1954 | ` */` |
|     224 | 1955 | `PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1956 | `{` |
|       - | 1957 | `	ph7_generator *pGen;` |
|       - | 1958 | `	sxi32 rc;` |
|     229 | 1959 | `	if( nArg < 1 ) return PH7_OK;` |
|     229 | 1960 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|     229 | 1961 | `	if( pGen == 0 ) return PH7_OK;` |
|     229 | 1962 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     229 | 1963 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     229 | 1964 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     229 | 1965 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     109 | 1966 | `	}` |
|     223 | 1967 | `	return PH7_OK;` |
|     117 | 1968 | `}` |
|       - | 1969 | `/*` |
|       - | 1970 | ` * Generator::valid() — true if suspended at a yield point.` |
|       - | 1971 | ` */` |
|    1218 | 1972 | `PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1973 | `{` |
|       - | 1974 | `	ph7_generator *pGen;` |
|    1223 | 1975 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|    1223 | 1976 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|    1223 | 1977 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|    1223 | 1978 | `	return PH7_OK;` |
|     614 | 1979 | `}` |
|       - | 1980 | `/*` |
|       - | 1981 | ` * Generator::current() — return the last yielded value.` |
|       - | 1982 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 1983 | ` */` |
|    1190 | 1984 | `PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1985 | `{` |
|       - | 1986 | `	ph7_generator *pGen;` |
|       - | 1987 | `	sxi32 rc;` |
|    1195 | 1988 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1195 | 1989 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|    1195 | 1990 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1195 | 1991 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     117 | 1992 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     117 | 1993 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     117 | 1994 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      56 | 1995 | `	}` |
|    1195 | 1996 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|    1193 | 1997 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|     599 | 1998 | `	}else{` |
|       3 | 1999 | `		ph7_result_null(pCtx);` |
|       - | 2000 | `	}` |
|    1195 | 2001 | `	return PH7_OK;` |
|     600 | 2002 | `}` |
|       - | 2003 | `/*` |
|       - | 2004 | ` * Generator::key() — return the last yielded key.` |
|       - | 2005 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 2006 | ` */` |
|     222 | 2007 | `PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2008 | `{` |
|       - | 2009 | `	ph7_generator *pGen;` |
|       - | 2010 | `	sxi32 rc;` |
|     227 | 2011 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     227 | 2012 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|     227 | 2013 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     227 | 2014 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2015 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     ! 0 | 2016 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     ! 0 | 2017 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     ! 0 | 2018 | `	}` |
|     227 | 2019 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     227 | 2020 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|     116 | 2021 | `	}else{` |
|     ! 0 | 2022 | `		ph7_result_null(pCtx);` |
|       - | 2023 | `	}` |
|     227 | 2024 | `	return PH7_OK;` |
|     116 | 2025 | `}` |
|       - | 2026 | `/*` |
|       - | 2027 | ` * Generator::next() — advance to the next yield point.` |
|       - | 2028 | ` */` |
|     990 | 2029 | `PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2030 | `{` |
|       - | 2031 | `	ph7_generator *pGen;` |
|       - | 2032 | `	sxi32 rc;` |
|     995 | 2033 | `	if( nArg < 1 ) return PH7_OK;` |
|     995 | 2034 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|     995 | 2035 | `	if( pGen == 0 ) return PH7_OK;` |
|     995 | 2036 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2037 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     995 | 2038 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     995 | 2039 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|     500 | 2040 | `	}else{` |
|     ! 0 | 2041 | `		return PH7_OK;` |
|       - | 2042 | `	}` |
|     995 | 2043 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     993 | 2044 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     983 | 2045 | `	return PH7_OK;` |
|     500 | 2046 | `}` |
|       - | 2047 | `/*` |
|       - | 2048 | ` * Generator::send($value) — resume and send a value into the generator.` |
|       - | 2049 | ` */` |
|     100 | 2050 | `PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 2051 | `{` |
|       - | 2052 | `	ph7_generator *pGen;` |
|       - | 2053 | `	ph7_value *pSendVal;` |
|       - | 2054 | `	sxi32 rc;` |
|     104 | 2055 | `	if( nArg < 1 ) return PH7_OK;` |
|     104 | 2056 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|     104 | 2057 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     104 | 2058 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|     104 | 2059 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       - | 2060 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|     ! 0 | 2061 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     104 | 2062 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     104 | 2063 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|      54 | 2064 | `	}else{` |
|     ! 0 | 2065 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2066 | `		return PH7_OK;` |
|       - | 2067 | `	}` |
|     104 | 2068 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     104 | 2069 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     101 | 2070 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      94 | 2071 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      48 | 2072 | `	}else{` |
|       8 | 2073 | `		ph7_result_null(pCtx);` |
|       - | 2074 | `	}` |
|     101 | 2075 | `	return PH7_OK;` |
|      54 | 2076 | `}` |
|       - | 2077 | `/*` |
|       - | 2078 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|       - | 2079 | ` *` |
|       - | 2080 | ` * PHP semantics: the exception is injected AT the suspended yield point so the` |
|       - | 2081 | ` * generator's OWN try/catch (if any wraps the yield) can handle it and the body` |
|       - | 2082 | ` * resumes after the try; otherwise it propagates to the throw() caller and the` |
|       - | 2083 | ` * generator closes. We implement this by resuming the body with a pending` |
|       - | 2084 | ` * injection (pCtx->pInjected) that the VM loop raises in the body's own frame via` |
|       - | 2085 | ` * the existing OP_THROW / ROOT B resume route — no exception frame is` |
|       - | 2086 | ` * reconstructed on pVm->pFrame, so the return/finally unwind path is untouched.` |
|       - | 2087 | ` * A never-started generator is first run to its first yield, then injected there;` |
|       - | 2088 | ` * a finished/closed generator has no suspend point, so the exception is simply` |
|       - | 2089 | ` * propagated to the caller (its return value stays readable via getReturn()).` |
|       - | 2090 | ` *` |
|       - | 2091 | ` * PHL does not enforce the Throwable parameter hint (interface/class hints are` |
|       - | 2092 | ` * not checked on call), so the Throwable validation — and its PHP TypeError —` |
|       - | 2093 | ` * are done here.` |
|       - | 2094 | ` */` |
|      62 | 2095 | `PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 2096 | `{` |
|       - | 2097 | `	ph7_generator *pGen;` |
|       - | 2098 | `	ph7_class_instance *pInj;` |
|       - | 2099 | `	ph7_class *pThrowable;` |
|       - | 2100 | `	VmFrame *pFrame;` |
|       - | 2101 | `	sxi32 rc;` |
|      66 | 2102 | `	if( nArg < 2 ) return PH7_OK;` |
|       - | 2103 | `	/* Argument #1 must be a Throwable; otherwise PHP raises a TypeError naming` |
|       - | 2104 | `	 * the given type (class name for objects, "null"/"string"/... for scalars). */` |
|      66 | 2105 | `	pThrowable = PH7_VmExtractClass(pCtx->pVm, "Throwable", sizeof("Throwable")-1, 0, 0);` |
|      62 | 2106 | `	if( (apArg[1]->iFlags & MEMOBJ_OBJ) == 0` |
|      66 | 2107 | `	 \|\| (pThrowable && !PH7_VmInstanceOf(((ph7_class_instance *)apArg[1]->x.pOther)->pClass, pThrowable)) ){` |
|       - | 2108 | `		char zCls[128];` |
|     ! 0 | 2109 | `		const char *zGiven = VmValueGivenName(apArg[1], zCls, sizeof(zCls));` |
|     ! 0 | 2110 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|     ! 0 | 2111 | `			"Generator::throw(): Argument #1 ($exception) must be of type Throwable, %s given", zGiven);` |
|       - | 2112 | `	}` |
|      66 | 2113 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      66 | 2114 | `	if( pGen == 0 ) return PH7_OK;` |
|       - | 2115 | `	/* PHP forbids resuming/throwing into a generator that is currently executing. */` |
|      66 | 2116 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       3 | 2117 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 2118 | `			"Cannot resume an already running generator");` |
|       - | 2119 | `	}` |
|       - | 2120 | `	/* Hold a reference to the injected instance for the whole operation: the VM loop` |
|       - | 2121 | `	 * (inject path) or VmThrowException (propagate path) may run catch blocks that bind` |
|       - | 2122 | `	 * and later release it. Dropped on every return path below. */` |
|      64 | 2123 | `	pInj = (ph7_class_instance *)apArg[1]->x.pOther;` |
|      64 | 2124 | `	pInj->iRef++;` |
|       - | 2125 | `	/* A never-started generator runs to its first yield, then the exception is injected` |
|       - | 2126 | `	 * there (PHP). Start it first; if it suspended at a yield, fall through to inject; if` |
|       - | 2127 | `	 * it ran to completion without yielding, drop through to the propagate path below. */` |
|      64 | 2128 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       5 | 2129 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       5 | 2130 | `		if( rc == PH7_ABORT ){ PH7_ClassInstanceUnref(pInj); return PH7_ABORT; }` |
|       5 | 2131 | `		if( rc == PH7_EXCEPTION ){ PH7_ClassInstanceUnref(pInj); return PH7_EXCEPTION; }` |
|       2 | 2132 | `	}` |
|      64 | 2133 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       - | 2134 | `		/* Inject at the suspended yield: the resume loop raises it in the body's own` |
|       - | 2135 | `		 * frame so the generator's try/catch can catch it and resume (path 2). */` |
|      58 | 2136 | `		pGen->pCtx->pInjected = pInj;   /* borrowed; ref held here across the resume */` |
|      58 | 2137 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       - | 2138 | `		/* Normally the inject was consumed (cleared) at VmByteCodeExec entry; clear it` |
|       - | 2139 | `		 * here too for the path where VmResumeCtx bails BEFORE entering the loop (e.g. the` |
|       - | 2140 | `		 * recursion-depth fatal), so no dangling borrowed pointer survives the Unref. */` |
|      58 | 2141 | `		pGen->pCtx->pInjected = 0;` |
|      58 | 2142 | `		PH7_ClassInstanceUnref(pInj);` |
|      58 | 2143 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      58 | 2144 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       - | 2145 | `		/* Caught inside the generator and it resumed: return the next yielded value (or` |
|       - | 2146 | `		 * null if it then completed) — symmetric with Generator::send(). */` |
|      46 | 2147 | `		if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      43 | 2148 | `			ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      23 | 2149 | `		}else{` |
|       3 | 2150 | `			ph7_result_null(pCtx);` |
|       - | 2151 | `		}` |
|      46 | 2152 | `		return PH7_OK;` |
|       - | 2153 | `	}` |
|       - | 2154 | `	/* COMPLETED/CLOSED (incl. a CREATED generator that ran to completion without a` |
|       - | 2155 | `	 * yield): no suspend point to inject at. Propagate the real object to the throw()` |
|       - | 2156 | `	 * caller through the normal dispatch path (class/message/trace preserved); its` |
|       - | 2157 | `	 * terminal state — and thus getReturn() — is left intact. */` |
|       8 | 2158 | `	pFrame = pCtx->pVm->pFrame;` |
|       8 | 2159 | `	if( pFrame ){` |
|       8 | 2160 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       8 | 2161 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       3 | 2162 | `	}` |
|       8 | 2163 | `	rc = VmThrowException(pCtx->pVm, pInj);` |
|       8 | 2164 | `	PH7_ClassInstanceUnref(pInj);` |
|       8 | 2165 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2166 | `		return PH7_ABORT;` |
|       - | 2167 | `	}` |
|       8 | 2168 | `	return PH7_EXCEPTION;` |
|      35 | 2169 | `}` |
|       - | 2170 | `/*` |
|       - | 2171 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|       - | 2172 | ` */` |
|      20 | 2173 | `PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 2174 | `{` |
|       - | 2175 | `	ph7_generator *pGen;` |
|      23 | 2176 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      23 | 2177 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      23 | 2178 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      23 | 2179 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|     ! 0 | 2180 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 2181 | `			"Cannot get return value of a generator that hasn't returned");` |
|       - | 2182 | `	}` |
|      23 | 2183 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|      23 | 2184 | `	return PH7_OK;` |
|      13 | 2185 | `}` |
|       - | 2186 | `/*` |
|       - | 2187 | ` * Generator::__destruct() — clean up.` |
|       - | 2188 | ` */` |
|     234 | 2189 | `PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2190 | `{` |
|       - | 2191 | `	ph7_generator *pGen;` |
|     239 | 2192 | `	sxi32 rcClose = SXRET_OK;` |
|     239 | 2193 | `	if( nArg < 1 ) return PH7_OK;` |
|     239 | 2194 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|     239 | 2195 | `	if( pGen ){` |
|       - | 2196 | `` 		/* A generator abandoned before it completes still runs its pending `finally` `` |
|       - | 2197 | `		 * blocks (PHP runs them at generator close/GC). Drive them before teardown. */` |
|     239 | 2198 | `		if( pGen->pCtx ){` |
|     239 | 2199 | `			rcClose = VmCloseCtx(pCtx->pVm, pGen->pCtx);` |
|     117 | 2200 | `		}` |
|     239 | 2201 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|     239 | 2202 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     239 | 2203 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 2204 | `			SyString sAttrName;` |
|       - | 2205 | `			ph7_value *pAttr;` |
|     239 | 2206 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     239 | 2207 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     239 | 2208 | `			if( pAttr ){` |
|     239 | 2209 | `				PH7_MemObjRelease(pAttr);` |
|     117 | 2210 | `			}` |
|     117 | 2211 | `		}` |
|     117 | 2212 | `	}` |
|       - | 2213 | `	/* Surface an abort/exception raised by a finally that ran during close. */` |
|     239 | 2214 | `	if( rcClose == PH7_ABORT ) return PH7_ABORT;` |
|     239 | 2215 | `	if( rcClose == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     239 | 2216 | `	return PH7_OK;` |
|     122 | 2217 | `}` |
|       - | 2218 | `/* ======================== End Generator Infrastructure ======================== */` |
|       - | 2219 | `/* ======================== End Fiber Infrastructure ======================== */` |
|       - | 2220 |  |
