# src/ph7/vm_exec_ctx.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1023/1279 lines (79.98%)

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
|     596 |   20 | `PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|       5 |   21 | `{` |
|       - |   22 | `	ph7_exec_ctx *pCtx;` |
|       - |   23 | `	ph7_value *pStack;` |
|       - |   24 | `	VmFrame *pFrame;` |
|     601 |   25 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|     601 |   26 | `	if( pCtx == 0 ){` |
|     ! 0 |   27 | `		return 0;` |
|       - |   28 | `	}` |
|     601 |   29 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|     601 |   30 | `	pCtx->pVm = pVm;` |
|     601 |   31 | `	pCtx->pFunc = pFunc;` |
|     601 |   32 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|     601 |   33 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|     601 |   34 | `	pCtx->pc = 0;` |
|     601 |   35 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|     601 |   36 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|     601 |   37 | `	PH7_MemObjInit(pVm, &pCtx->sDelegate);` |
|       - |   38 | `	/* Container for this body's own exception handlers while suspended (borrowed` |
|       - |   39 | `	 * ph7_exception* pointers — never freed here, owned by the compiled func). */` |
|     601 |   40 | `	SySetInit(&pCtx->aSavedException, &pVm->sAllocator, sizeof(ph7_exception *));` |
|       - |   41 | `	/* ROOT C: this body's own pending finally actions while suspended. */` |
|     601 |   42 | `	SySetInit(&pCtx->aSavedFinally, &pVm->sAllocator, sizeof(VmFinallyAction));` |
|     601 |   43 | `	pCtx->nFinallyBase = 0;` |
|       - |   44 | `	/* Stage 4: this coroutine's own aSelf entries (self::/static:: class context` |
|       - |   45 | `	 * pushed by nested method calls still open at suspend) parked while suspended,` |
|       - |   46 | `	 * so they don't pollute the resumer's aSelf. Borrowed ph7_class* pointers. */` |
|     601 |   47 | `	SySetInit(&pCtx->aSavedSelf, &pVm->sAllocator, sizeof(ph7_class *));` |
|     601 |   48 | `	pCtx->nSelfBase = 0;` |
|     601 |   49 | `	pCtx->pParkedSegment = 0;` |
|     601 |   50 | `	pCtx->nBodyExecDepth = 0;` |
|       - |   51 | `	/* Allocate a private operand stack */` |
|     601 |   52 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|     601 |   53 | `	if( pStack == 0 ){` |
|     ! 0 |   54 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   55 | `		return 0;` |
|       - |   56 | `	}` |
|     601 |   57 | `	pCtx->pStack = pStack;` |
|     601 |   58 | `	pCtx->nStackCap = SySetUsed(&pFunc->aByteCode) + VM_STACK_GUARD; /* grows with pStack on OP_SPREAD */` |
|     601 |   59 | `	pCtx->nStackOrig = pCtx->nStackCap; /* fixed original — headroom reference across resumes */` |
|       - |   60 | `	/* Create a detached frame for the fiber */` |
|     601 |   61 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|     601 |   62 | `	if( pFrame == 0 ){` |
|     ! 0 |   63 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|     ! 0 |   64 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|     601 |   67 | `	pCtx->pFrame = pFrame;` |
|     601 |   68 | `	return pCtx;` |
|     303 |   69 | `}` |
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
|    4938 |   93 | `static void VmParkStackSlice(SySet *pFrom, SySet *pSaved, sxu32 nBase)` |
|       5 |   94 | `{` |
|    4943 |   95 | `	sxu32 nUsed = SySetUsed(pFrom);` |
|    4943 |   96 | `	if( nUsed > nBase ){` |
|     575 |   97 | `		const char *aBase = (const char *)SySetBasePtr(pFrom);` |
|       - |   98 | `		sxu32 i;` |
|    1153 |   99 | `		for( i = nBase; i < nUsed; i++ ){` |
|     583 |  100 | `			SySetPut(pSaved, (const void *)(aBase + i * pFrom->eSize));` |
|     294 |  101 | `		}` |
|     575 |  102 | `		SySetTruncate(pFrom, nBase);` |
|     285 |  103 | `	}` |
|    4943 |  104 | `}` |
|    4116 |  105 | `static void VmRestoreStackSlice(SySet *pTo, SySet *pSaved)` |
|       5 |  106 | `{` |
|    4121 |  107 | `	sxu32 i, n = SySetUsed(pSaved);` |
|    4121 |  108 | `	if( n > 0 ){` |
|     353 |  109 | `		const char *aSaved = (const char *)SySetBasePtr(pSaved);` |
|     709 |  110 | `		for( i = 0; i < n; i++ ){` |
|     361 |  111 | `			SySetPut(pTo, (const void *)(aSaved + i * pSaved->eSize));` |
|     183 |  112 | `		}` |
|     353 |  113 | `		SySetReset(pSaved);` |
|     174 |  114 | `	}` |
|    4121 |  115 | `}` |
|    1646 |  116 | `static void VmParkCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  117 | `{` |
|    1651 |  118 | `	VmParkStackSlice(&pVm->aException, &pCtx->aSavedException, pCtx->nExceptionBase);` |
|    1651 |  119 | `	VmParkStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally, pCtx->nFinallyBase);` |
|    1651 |  120 | `	VmParkStackSlice(&pVm->aSelf, &pCtx->aSavedSelf, pCtx->nSelfBase);` |
|    1651 |  121 | `}` |
|    1372 |  122 | `static void VmRestoreCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  123 | `{` |
|    1377 |  124 | `	VmRestoreStackSlice(&pVm->aException, &pCtx->aSavedException);` |
|    1377 |  125 | `	VmRestoreStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally);` |
|    1377 |  126 | `	VmRestoreStackSlice(&pVm->aSelf, &pCtx->aSavedSelf);` |
|    1377 |  127 | `}` |
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
|    1292 |  141 | `static void VmFreeSuspendedExceptionFrames(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  142 | `{` |
|    1493 |  143 | `	while( pVm->pFrame != pCtx->pFrame && (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     201 |  144 | `		VmLeaveFrame(&(*pVm));` |
|       5 |  145 | `	}` |
|    1297 |  146 | `}` |
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
|    1646 |  159 | `static void VmSuspendCtxDetach(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  160 | `{` |
|    1651 |  161 | `	if( pCtx->pParkedSegment == 0 ){` |
|    1297 |  162 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     651 |  163 | `	}else{` |
|       - |  164 | `		/* The parked records' push-time accounting (one nRecursionDepth++ each,` |
|       - |  165 | `		 * plus VmCallFinish's aSelf pop, which never ran) leaves the segment` |
|       - |  166 | `		 * counted as active while the fiber is suspended — deactivate it. aSelf` |
|       - |  167 | `		 * is parked wholesale below (base-relative), so drop only the depth. */` |
|     359 |  168 | `		pVm->nRecursionDepth -= ((VmParkedSegment *)pCtx->pParkedSegment)->nRecords;` |
|       - |  169 | `	}` |
|    1651 |  170 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|    1651 |  171 | `	pCtx->pFrame->pParent = 0;` |
|    1651 |  172 | `	VmParkCtxState(pVm, pCtx);` |
|    1651 |  173 | `	if( pResult ){` |
|     359 |  174 | `		PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|     177 |  175 | `	}` |
|    1651 |  176 | `}` |
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
|    1944 |  187 | `static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx)` |
|       5 |  188 | `{` |
|    1175 |  189 | `	return ((pCtx->pFunc->iFlags & VM_FUNC_GENERATOR) == 0 && VmFuncHasReturnType(pCtx->pFunc))` |
|    1170 |  190 | `		? pCtx->pFunc : 0;` |
|       5 |  191 | `}` |
|       - |  192 | `/*` |
|       - |  193 | ` * Common epilogue for VmStartCtx / VmResumeCtx once the body run returns rc:` |
|       - |  194 | ` * restore the previous active context, then park on suspend or detach the` |
|       - |  195 | ` * coroutine frame and record the terminal state. On normal completion the value` |
|       - |  196 | ` * belongs to getReturn() only — start()/resume() return the NEXT suspend value,` |
|       - |  197 | ` * which is null at completion (php parity), so pResult is left at its` |
|       - |  198 | ` * caller-initialized null.` |
|       - |  199 | ` */` |
|    1944 |  200 | `static sxi32 VmFinishCtxRun(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_exec_ctx *pOldCtx,` |
|       - |  201 | `	sxi32 rc, ph7_value *pResult)` |
|       5 |  202 | `{` |
|    1949 |  203 | `	pVm->pActiveCtx = pOldCtx;` |
|    1949 |  204 | `	if( rc == PH7_SUSPEND ){` |
|       - |  205 | `		/* Handles a deep RE-suspend too (a resumed segment parking a fresh one):` |
|       - |  206 | `		 * VmSuspendCtxDetach deactivates the new segment's recursion accounting,` |
|       - |  207 | `		 * parks its aSelf, and skips VmFreeSuspendedExceptionFrames for a segment` |
|       - |  208 | `		 * so it can't free the still-live parked try wrappers. */` |
|    1651 |  209 | `		VmSuspendCtxDetach(pVm, pCtx, pResult);` |
|    1651 |  210 | `		return SXRET_OK;` |
|       - |  211 | `	}` |
|       - |  212 | `	/* Detach the coroutine frame from the live chain, unless a deeper unwind` |
|       - |  213 | `	 * already moved pVm->pFrame off it. */` |
|     303 |  214 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|     303 |  215 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|     303 |  216 | `		pCtx->pFrame->pParent = 0;` |
|     149 |  217 | `	}` |
|     303 |  218 | `	if( rc == PH7_ABORT ){` |
|       3 |  219 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       3 |  220 | `		return PH7_ABORT;` |
|       - |  221 | `	}` |
|     301 |  222 | `	if( rc == PH7_EXCEPTION ){` |
|      47 |  223 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      47 |  224 | `		return PH7_EXCEPTION;` |
|       - |  225 | `	}` |
|     259 |  226 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|     259 |  227 | `	return SXRET_OK;` |
|     977 |  228 | `}` |
|       - |  229 | `/*` |
|       - |  230 | ` * Start executing a fiber context for the first time.` |
|       - |  231 | ` */` |
|     572 |  232 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  233 | `{` |
|       - |  234 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  235 | `	sxi32 rc;` |
|     577 |  236 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|     ! 0 |  237 | `		return SXERR_INVALID;` |
|       - |  238 | `	}` |
|       - |  239 | `	/* A fiber/generator start is a native VmByteCodeExec re-entry, bounded by` |
|       - |  240 | `	 * nMaxNativeDepth. Reject HERE, before attaching the frame / mutating VM` |
|       - |  241 | `	 * state, so the abort is clean — the wrapper's own check fires only after` |
|       - |  242 | `	 * this function has spliced the coroutine into the frame chain, which its` |
|       - |  243 | `	 * post-exec detach cannot fully unwind. The PHP call-depth cap belongs to` |
|       - |  244 | `	 * OP_CALL only (BYTECODE.md stage 5). */` |
|     577 |  245 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  246 | `		return VmNativeNestingFatal(pVm);` |
|       - |  247 | `	}` |
|       - |  248 | `	/* Attach the fiber's frame to the VM frame chain */` |
|     577 |  249 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     577 |  250 | `	pVm->pFrame = pCtx->pFrame;` |
|       - |  251 | `	/* Save and set the active context */` |
|     577 |  252 | `	pOldCtx = pVm->pActiveCtx;` |
|     577 |  253 | `	pVm->pActiveCtx = pCtx;` |
|     577 |  254 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|     577 |  255 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|     577 |  256 | `	pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|     577 |  257 | `	pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|       - |  258 | `	/* Native depth the body runs at (the VmByteCodeExec wrapper bumps +1): a` |
|       - |  259 | `	 * Fiber::suspend() at a deeper depth is inside a C->PHP callback and gets a` |
|       - |  260 | `	 * FiberError instead of parking across the native frame (stage 4). */` |
|     577 |  261 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1;` |
|       - |  262 | `	/* Execute from the beginning (no parked segment). An OP_SPREAD in the body may` |
|       - |  263 | `	 * realloc pCtx->pStack — pass &pCtx->pStack / &pCtx->nStackCap so the grown` |
|       - |  264 | `	 * buffer + capacity persist for the next resume and for ctx teardown. */` |
|     863 |  265 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     286 |  266 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|     286 |  267 | `		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|     577 |  268 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     291 |  269 | `}` |
|       - |  270 | `/*` |
|       - |  271 | ` * Resume a suspended fiber context.` |
|       - |  272 | ` */` |
|    1372 |  273 | `PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|       5 |  274 | `{` |
|       - |  275 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  276 | `	VmParkedSegment *pSeg;` |
|       - |  277 | `	sxi32 rc;` |
|    1377 |  278 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|     ! 0 |  279 | `		return SXERR_INVALID;` |
|       - |  280 | `	}` |
|       - |  281 | `	/* A resume is a native VmByteCodeExec re-entry, bounded by nMaxNativeDepth.` |
|       - |  282 | `	 * Reject HERE, before re-attaching the (possibly deep) parked segment to the` |
|       - |  283 | `	 * frame chain and re-adding its nRecords to nRecursionDepth — a wrapper-level` |
|       - |  284 | `	 * abort past those mutations would leave pVm->pFrame pointing into the parked` |
|       - |  285 | `	 * callee and the depth accounting un-reverted. The PHP call-depth cap is` |
|       - |  286 | `	 * OP_CALL-only (BYTECODE.md stage 5). */` |
|    1377 |  287 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  288 | `		return VmNativeNestingFatal(pVm);` |
|       - |  289 | `	}` |
|       - |  290 | `	/* Push the resume value onto the SUSPENDED activation's operand stack so it` |
|       - |  291 | `	 * appears as Fiber::suspend()'s return value. For a deep suspend (stage 4)` |
|       - |  292 | `	 * that stack is the innermost callee's — parked in the segment — not the` |
|       - |  293 | `	 * body's. nTos was saved one below the return-value slot. */` |
|       - |  294 | `	{` |
|       - |  295 | `		ph7_value *pResumeStack;` |
|    1377 |  296 | `		pSeg = (VmParkedSegment *)pCtx->pParkedSegment;` |
|    1377 |  297 | `		pResumeStack = pSeg ? pSeg->sState.pStack : pCtx->pStack;` |
|    1377 |  298 | `		if( pResumeValue ){` |
|     309 |  299 | `			PH7_MemObjStore(pResumeValue, &pResumeStack[pCtx->nTos + 1]);` |
|     157 |  300 | `		}else{` |
|    1073 |  301 | `			PH7_MemObjRelease(&pResumeStack[pCtx->nTos + 1]);` |
|       - |  302 | `		}` |
|    1377 |  303 | `		pCtx->nTos++;` |
|       - |  304 | `		/* Refresh the caller-depth base and re-publish this body's own exception` |
|       - |  305 | `		 * handlers on top of pVm->aException at that depth, so the resumed body's` |
|       - |  306 | `		 * try/catch and finally-drain bound line up (see VmByteCodeExec's base` |
|       - |  307 | `		 * override). Must run before VmByteCodeExec recaptures its local base. */` |
|    1377 |  308 | `		pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|    1377 |  309 | `		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|    1377 |  310 | `		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|    1377 |  311 | `		VmRestoreCtxState(pVm, pCtx);` |
|    1377 |  312 | `		if( pSeg ){` |
|       - |  313 | `			/* Reactivate the parked records' recursion accounting (mirror of the` |
|       - |  314 | `			 * deactivate at suspend); aSelf was just restored above. */` |
|     149 |  315 | `			pVm->nRecursionDepth += pSeg->nRecords;` |
|       - |  316 | `			/* Rebase the parked segment's absolute exception-floor indices: the` |
|       - |  317 | `			 * fiber may resume at a different caller depth than it suspended at,` |
|       - |  318 | `			 * so every activation's nExceptionBase shifts by the same delta the` |
|       - |  319 | `			 * republished handlers moved (newBase - the park-time base). */` |
|     149 |  320 | `			sxi32 iDelta = (sxi32)pCtx->nExceptionBase - (sxi32)pSeg->nOldExcBase;` |
|     149 |  321 | `			if( iDelta != 0 ){` |
|       - |  322 | `				VmCallFrame *pRec;` |
|       3 |  323 | `				pSeg->sState.nExceptionBase =` |
|       2 |  324 | `					(sxu32)((sxi32)pSeg->sState.nExceptionBase + iDelta);` |
|       5 |  325 | `				for( pRec = pSeg->pCallTop; pRec; pRec = pRec->pPrev ){` |
|       3 |  326 | `					pRec->sCaller.nExceptionBase =` |
|       2 |  327 | `						(sxu32)((sxi32)pRec->sCaller.nExceptionBase + iDelta);` |
|       2 |  328 | `				}` |
|       1 |  329 | `			}` |
|      72 |  330 | `		}` |
|       - |  331 | `		/* Re-attach the coroutine to the live VM frame chain: the body frame's` |
|       - |  332 | `		 * parent becomes the resumer's current frame. For a deep segment the` |
|       - |  333 | `		 * suspend-time top frame (the innermost callee / open-try wrapper) then` |
|       - |  334 | `		 * becomes current so the adopt at VmByteCodeExec entry resumes inside the` |
|       - |  335 | `		 * callee; body-level resumes make the body frame current. */` |
|    1377 |  336 | `		pCtx->pFrame->pParent = pVm->pFrame;` |
|    1377 |  337 | `		pVm->pFrame = pSeg ? pSeg->pTopFrame : pCtx->pFrame;` |
|       - |  338 | `	}` |
|       - |  339 | `	/* The segment (if any) is handed to the body invocation explicitly below; it` |
|       - |  340 | `	 * is no longer part of the suspended ctx state once resume owns it. */` |
|    1377 |  341 | `	pCtx->pParkedSegment = 0;` |
|       - |  342 | `	/* Save and set the active context */` |
|    1377 |  343 | `	pOldCtx = pVm->pActiveCtx;` |
|    1377 |  344 | `	pVm->pActiveCtx = pCtx;` |
|    1377 |  345 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|    1377 |  346 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1; /* see VmStartCtx */` |
|       - |  347 | `	/* Resume execution from saved PC. pSeg, when non-NULL, makes this body` |
|       - |  348 | `	 * re-enter inside the innermost parked callee (deep fiber resume, stage 4). */` |
|    2063 |  349 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     686 |  350 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|     686 |  351 | `		VmCtxEnforceRetFunc(pCtx), FALSE, pSeg, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|    1377 |  352 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     691 |  353 | `}` |
|       - |  354 | `/*` |
|       - |  355 | ` * Force-close a suspended generator context at destruction time, running its` |
|       - |  356 | `` * pending `finally` blocks (PHP runs finally when a generator is unset / goes out`` |
|       - |  357 | ` * of scope / is GC'd before it completes; PHL previously freed the open try` |
|       - |  358 | ` * handlers unexecuted in VmReleaseExecCtx). This is a "close", not a "resume":` |
|       - |  359 | `` * the finally handler of every still-open `try` the generator was suspended`` |
|       - |  360 | `` * inside runs innermost-first, but NO `catch` runs and no code past the finallys`` |
|       - |  361 | ` * executes.` |
|       - |  362 | ` *` |
|       - |  363 | ` * Generators compile finally INLINE (ROOT C): the finally bytecode lives in the` |
|       - |  364 | ` * body program at iFinallyPc, driven by the aException/aFinallyAction pc-redirect` |
|       - |  365 | `` * machinery — not the legacy detached `sFinally` mini-program VmDrainFinally runs.`` |
|       - |  366 | `` * So a close is expressed exactly like a `return` that crosses every enclosing`` |
|       - |  367 | ` * finally (OP_SET_FINALLY_RET): set pCtx->bClosing and resume; the body-entry` |
|       - |  368 | ` * redirect (see VmByteCodeExecBody) seeds a PH7_FA_RETURN action and jumps into` |
|       - |  369 | ` * the innermost open try's finally, and OP_END_FINALLY threads it out through the` |
|       - |  370 | ` * chain, then completes the body.` |
|       - |  371 | ` *` |
|       - |  372 | ` * Scope: a plain body-level suspend (pParkedSegment == 0). A deep fiber segment is` |
|       - |  373 | ` * left to plain release (generators never park one — yield is body-level only). A` |
|       - |  374 | `` * `yield` reached inside a finally during close is rejected by OP_YIELD via`` |
|       - |  375 | ` * pCtx->bClosing (PHP-exact "Cannot yield from finally in a force-closed` |
|       - |  376 | ` * generator"). Deferred edges remain.` |
|       - |  377 | ` *` |
|       - |  378 | ` * Returns whatever the body run returns: SXRET_OK on a clean close, PH7_ABORT if a` |
|       - |  379 | ` * finally aborts, or PH7_EXCEPTION if a finally threw past itself (surfaced to the` |
|       - |  380 | ` * destruct caller).` |
|       - |  381 | ` */` |
|     220 |  382 | `static sxi32 VmCloseCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  383 | `{` |
|       - |  384 | `	sxi32 rc;` |
|     225 |  385 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       - |  386 | `		/* CREATED never ran, so has no open try; COMPLETED/CLOSED already ran theirs. */` |
|     187 |  387 | `		return SXRET_OK;` |
|       - |  388 | `	}` |
|      42 |  389 | `	if( pCtx->pParkedSegment != 0 ){` |
|       - |  390 | `		/* Deep fiber segment (never a generator) — leave to plain release. */` |
|     ! 0 |  391 | `		return SXRET_OK;` |
|       - |  392 | `	}` |
|       - |  393 | ``	/* Suspended mid `yield from` over an inner generator: PHP closes innermost-first,`` |
|       - |  394 | `	 * so run the delegate's finallys before this body's. Both delegate-object states` |
|       - |  395 | `	 * carry the live iterator in sDelegate — state 3 (the delegate IS a Generator) and` |
|       - |  396 | ``	 * state 2 (an Iterator, which for `yield from $aggregate` is the generator returned`` |
|       - |  397 | `	 * by getIterator()); VmGeneratorExtractCtx returns 0 for a non-generator iterator,` |
|       - |  398 | `	 * so a plain Iterator/array delegate is skipped. Recurses for nested delegation;` |
|       - |  399 | `	 * the inner ends COMPLETED, so its own later __destruct close is a no-op. */` |
|      42 |  400 | `	if( pCtx->iDelegateState >= 2 && (pCtx->sDelegate.iFlags & MEMOBJ_OBJ) ){` |
|       5 |  401 | `		ph7_generator *pInner = VmGeneratorExtractCtx(pVm, &pCtx->sDelegate);` |
|       5 |  402 | `		if( pInner && pInner->pCtx ){` |
|       5 |  403 | `			sxi32 rcInner = VmCloseCtx(pVm, pInner->pCtx);` |
|       5 |  404 | `			if( rcInner == PH7_ABORT ){ return PH7_ABORT; }` |
|       2 |  405 | `		}` |
|       2 |  406 | `	}` |
|       - |  407 | `	/* Drive the pending finallys through a real body resume that the entry redirect` |
|       - |  408 | `	 * turns into a finally-chain unwind. VmResumeCtx handles state restore, frame` |
|       - |  409 | `	 * re-attach, active-ctx save/restore and the terminal-state bookkeeping. */` |
|      42 |  410 | `	pCtx->bClosing = 1;` |
|      42 |  411 | `	rc = VmResumeCtx(pVm, pCtx, 0, 0);` |
|      42 |  412 | `	pCtx->bClosing = 0;` |
|      42 |  413 | `	return rc;` |
|     115 |  414 | `}` |
|       - |  415 | `/*` |
|       - |  416 | ` * Free one DETACHED frame (not in the live pVm->pFrame chain): the frame of a` |
|       - |  417 | ` * suspended coroutine's body, or of a segment activation abandoned mid-call.` |
|       - |  418 | ` * Mirrors VmLeaveFrame's teardown minus the chain pop (there is no chain to pop` |
|       - |  419 | ` * from). Factored so the body-frame free and the stage-4 segment free share it.` |
|       - |  420 | ` */` |
|     852 |  421 | `static void VmFreeDetachedFrame(ph7_vm *pVm, VmFrame *pFrame)` |
|       5 |  422 | `{` |
|       - |  423 | `	VmSlot *aSlot;` |
|       - |  424 | `	sxu32 n;` |
|     857 |  425 | `	if( pFrame == 0 ){` |
|     ! 0 |  426 | `		return;` |
|       - |  427 | `	}` |
|       - |  428 | `	/* Free local variables */` |
|     857 |  429 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|    1709 |  430 | `	for( n = 0; n < SySetUsed(&pFrame->sLocal); ++n ){` |
|     857 |  431 | `		PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|     431 |  432 | `	}` |
|       - |  433 | `	/* Remove local references */` |
|     857 |  434 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|    1709 |  435 | `	for( n = 0; n < SySetUsed(&pFrame->sRef); ++n ){` |
|     857 |  436 | `		PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|     431 |  437 | `	}` |
|     857 |  438 | `	SyHashRelease(&pFrame->hVar);` |
|     857 |  439 | `	SySetRelease(&pFrame->sArg);` |
|     857 |  440 | `	SySetRelease(&pFrame->sLocal);` |
|     857 |  441 | `	SySetRelease(&pFrame->sRef);` |
|     857 |  442 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|       - |  443 | `	/* Drop a resume target pointing at this detached frame before we free it (ROOT B). */` |
|     857 |  444 | `	VmDropResumeTarget(pVm,pFrame);` |
|     857 |  445 | `	SyMemBackendPoolFree(&pVm->sAllocator, pFrame);` |
|     431 |  446 | `}` |
|       - |  447 | `/*` |
|       - |  448 | ` * Free a parked deep-suspend segment (stage 4) whose fiber was abandoned while` |
|       - |  449 | ` * suspended. Every record holds a callee's operand stack and VmFrame (the` |
|       - |  450 | ` * topmost record's callee is the innermost activation, running on sState); walk` |
|       - |  451 | ` * the chain releasing each callee stack's live entries then the stack and frame.` |
|       - |  452 | ` * The body frame/stack are NOT here — they are freed by the caller` |
|       - |  453 | ` * (VmReleaseExecCtx) as pCtx->pFrame / pCtx->pStack.` |
|       - |  454 | ` */` |
|     206 |  455 | `static void VmFreeParkedSegment(ph7_vm *pVm, ph7_exec_ctx *pCtx, VmParkedSegment *pSeg)` |
|       2 |  456 | `{` |
|       - |  457 | `	/* Live top-of-stack of the activation running on the current record's callee` |
|       - |  458 | `	 * stack: the innermost (sState) for the topmost record, then each caller. */` |
|     208 |  459 | `	ph7_value *pTosAbove = pSeg->sState.pTos;` |
|     208 |  460 | `	VmCallFrame *pRec = pSeg->pCallTop, *pNext;` |
|     614 |  461 | `	while( pRec ){` |
|     408 |  462 | `		ph7_value *pStk = pRec->sCall.pFrameStack;` |
|     408 |  463 | `		if( pStk ){` |
|     408 |  464 | `			ph7_value *pTos = pTosAbove;` |
|     814 |  465 | `			while( pTos >= pStk ){` |
|     408 |  466 | `				PH7_MemObjRelease(pTos);` |
|     408 |  467 | `				pTos--;` |
|       2 |  468 | `			}` |
|     408 |  469 | `			SyMemBackendFree(&pVm->sAllocator, pStk);` |
|     203 |  470 | `		}` |
|     408 |  471 | `		VmFreeDetachedFrame(pVm, pRec->sCall.pFrame);` |
|       - |  472 | `		/* The caller recorded here runs on the NEXT-lower callee stack; grab its` |
|       - |  473 | `		 * live tos before freeing this node. */` |
|     408 |  474 | `		pTosAbove = pRec->sCaller.pTos;` |
|     408 |  475 | `		pNext = pRec->pPrev;` |
|     408 |  476 | `		SyMemBackendPoolFree(&pVm->sAllocator, pRec);` |
|     408 |  477 | `		pRec = pNext;` |
|       2 |  478 | `	}` |
|       - |  479 | `	/* pTosAbove now points at the BODY activation's live top (the bottom record's` |
|       - |  480 | `	 * sCaller, which runs on pCtx->pStack). pCtx->nTos still holds the INNERMOST` |
|       - |  481 | `	 * index (VmSuspendCtx saved it), which would over-index the body stack in` |
|       - |  482 | `	 * VmReleaseExecCtx's release loop — correct it to the body's real top. */` |
|     208 |  483 | `	pCtx->nTos = (sxi32)(pTosAbove - pCtx->pStack);` |
|     208 |  484 | `	SyMemBackendFree(&pVm->sAllocator, pSeg);` |
|     208 |  485 | `}` |
|       - |  486 | `/*` |
|       - |  487 | ` * Release an execution context and all its resources.` |
|       - |  488 | ` */` |
|     446 |  489 | `PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  490 | `{` |
|     451 |  491 | `	if( pCtx == 0 ){` |
|     ! 0 |  492 | `		return;` |
|       - |  493 | `	}` |
|     451 |  494 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       - |  495 | `		/* Cannot destroy a fiber that is currently executing */` |
|     ! 0 |  496 | `		return;` |
|       - |  497 | `	}` |
|     451 |  498 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       - |  499 | `	/* Release values */` |
|     451 |  500 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|     451 |  501 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|     451 |  502 | `	PH7_MemObjRelease(&pCtx->sDelegate);` |
|       - |  503 | `	/* Stage 2b: the parked entries are per-activation clones now — free them` |
|       - |  504 | `	 * (an abandoned suspended coroutine is their last holder). */` |
|     451 |  505 | `	VmExcReleaseAll(pVm,&pCtx->aSavedException);` |
|     451 |  506 | `	SySetRelease(&pCtx->aSavedException);` |
|       - |  507 | `	/* ROOT C: a generator abandoned while suspended inside a finally may carry parked` |
|       - |  508 | `	 * finally actions holding owned values/refs (a RETURN's sRet, a RETHROW's pExc).` |
|       - |  509 | `	 * Release them so the abandon path leaks nothing. */` |
|       - |  510 | `	{` |
|     451 |  511 | `		sxu32 n = SySetUsed(&pCtx->aSavedFinally);` |
|     451 |  512 | `		if( n > 0 ){` |
|     ! 0 |  513 | `			VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pCtx->aSavedFinally);` |
|       - |  514 | `			sxu32 i;` |
|     ! 0 |  515 | `			for( i = 0; i < n; i++ ){` |
|     ! 0 |  516 | `				if( aA[i].eKind == PH7_FA_RETURN ){` |
|     ! 0 |  517 | `					PH7_MemObjRelease(&aA[i].sRet);` |
|     ! 0 |  518 | `				}else if( aA[i].eKind == PH7_FA_RETHROW && aA[i].pExc ){` |
|     ! 0 |  519 | `					PH7_ClassInstanceUnref(aA[i].pExc);` |
|     ! 0 |  520 | `				}` |
|     ! 0 |  521 | `			}` |
|     ! 0 |  522 | `		}` |
|     451 |  523 | `		SySetRelease(&pCtx->aSavedFinally);` |
|       - |  524 | `	}` |
|       - |  525 | `	/* Stage 4: parked aSelf entries are borrowed class pointers — just free the set. */` |
|     451 |  526 | `	SySetRelease(&pCtx->aSavedSelf);` |
|       - |  527 | `	/* Free a parked deep-suspend segment (stage 4): the fiber was abandoned while` |
|       - |  528 | `	 * suspended inside a nested call, so its record chain / frames / operand` |
|       - |  529 | `	 * stacks are still alive and only this holder references them. Must run` |
|       - |  530 | `	 * before the body frame/stack below (they are the segment's floor). */` |
|     451 |  531 | `	if( pCtx->pParkedSegment ){` |
|     208 |  532 | `		VmFreeParkedSegment(pVm, pCtx, (VmParkedSegment *)pCtx->pParkedSegment);` |
|     208 |  533 | `		pCtx->pParkedSegment = 0;` |
|     103 |  534 | `	}` |
|       - |  535 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|     451 |  536 | `	if( pCtx->pFrame ){` |
|     451 |  537 | `		VmFreeDetachedFrame(pVm, pCtx->pFrame);` |
|     451 |  538 | `		pCtx->pFrame = 0;` |
|     223 |  539 | `	}` |
|       - |  540 | `	/* Release individual operand stack entries (decrement refcounts,` |
|       - |  541 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|       - |  542 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|     451 |  543 | `	if( pCtx->pStack ){` |
|     451 |  544 | `		if( pCtx->nTos >= 0 ){` |
|     417 |  545 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|     829 |  546 | `			while( pTos >= pCtx->pStack ){` |
|     417 |  547 | `				PH7_MemObjRelease(pTos);` |
|     417 |  548 | `				pTos--;` |
|       5 |  549 | `			}` |
|     206 |  550 | `		}` |
|     451 |  551 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|     451 |  552 | `		pCtx->pStack = 0;` |
|     223 |  553 | `	}` |
|       - |  554 | `	/* Free the context itself */` |
|     451 |  555 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     228 |  556 | `}` |
|       - |  557 | `/*` |
|       - |  558 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|       - |  559 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|       - |  560 | ` */` |
|     760 |  561 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|       5 |  562 | `{` |
|       - |  563 | `	ph7_class_instance *pThis;` |
|       - |  564 | `	SyString sAttr;` |
|       - |  565 | `	ph7_value *pAttr;` |
|     765 |  566 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 |  567 | `		return 0;` |
|       - |  568 | `	}` |
|     765 |  569 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|     765 |  570 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|     ! 0 |  571 | `		return 0;` |
|       - |  572 | `	}` |
|     765 |  573 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     765 |  574 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     765 |  575 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     269 |  576 | `		return 0;` |
|       - |  577 | `	}` |
|     501 |  578 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|     385 |  579 | `}` |
|       - |  580 | `/* ph7_class_instance.iFlags bit: this Closure is a bound/static first-class callable and` |
|       - |  581 | ` * carries $__this/$__scope. Lets the hot plain-closure unwrap skip those attribute lookups.` |
|       - |  582 | ` * (Distinct from CLASS_INSTANCE_DESTROYED 0x001 and VM_INSTANCE_DUMPING 0x002.) */` |
|       - |  583 | `#define VM_INSTANCE_FCC_BOUND 0x004` |
|       - |  584 | `/*` |
|       - |  585 | `` * A PHP closure (and a first-class callable `f(...)`) is a real object: an instance of`` |
|       - |  586 | `` * the built-in final `Closure` class carrying its underlying callable in a private`` |
|       - |  587 | `` * `$__fn` attribute (the callable NAME: a registered `[closure_N]`/`[lambda_N]` or a`` |
|       - |  588 | ` * user/host function name) — plus, for a method/static first-class callable, a bound` |
|       - |  589 | `` * `$__this` object and/or a `$__scope` class-name. Storing the callable as plain attributes`` |
|       - |  590 | `` * (rather than a native resource pointer) means the object owns no `ph7_vm_func` — the`` |
|       - |  591 | `` * per-closure function stays owned by `hFunction`/VM-release exactly as before, so there is`` |
|       - |  592 | ` * no extra free path.` |
|       - |  593 | ` *` |
|       - |  594 | ` * Returns non-zero iff pVal is a Closure instance.` |
|       - |  595 | ` */` |
| 1045503 |  596 | `PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm, ph7_value *pVal)` |
|       5 |  597 | `{` |
|       - |  598 | `	ph7_class_instance *pThis;` |
|       - |  599 | `	/* Flag test first: a non-object call target (the hot common case) bails before any` |
|       - |  600 | `	 * pVm dereference; pClosureClass==0 is a one-time pre-init concern, so it goes last. */` |
| 1045508 |  601 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
| 1040896 |  602 | `		return 0;` |
|       - |  603 | `	}` |
|    4617 |  604 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       - |  605 | `	/* Closure is final, so an exact class match is correct (no subclasses possible). */` |
|    4617 |  606 | `	return pThis->pClass == pVm->pClosureClass;` |
|  523222 |  607 | `}` |
|       - |  608 | `/*` |
|       - |  609 | ` * Unwrap a Closure value into the simple callable the existing dispatch machinery` |
|       - |  610 | ` * already understands, written into pOut (which the caller must have initialised):` |
|       - |  611 | `` *   - bound `$this` set (method first-class callable) -> [ $__this, $__fn ] array callable`` |
|       - |  612 | `` *   - `$__scope` set (static first-class callable)     -> [ $__scope, $__fn ] array callable`` |
|       - |  613 | `` *   - neither (plain function / real closure)          -> the `$__fn` name string`` |
|       - |  614 | ` * The 2-element array form rides the existing MEMOBJ_HASHMAP dispatch, which binds $this` |
|       - |  615 | ` * for an object first element and resolves the class for a class-name-string first element.` |
|       - |  616 | ` * Returns SXRET_OK if pVal was a Closure (pOut filled), SXERR_NOTFOUND otherwise.` |
|       - |  617 | ` */` |
|    1918 |  618 | `PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm, ph7_value *pVal, ph7_value *pOut)` |
|       5 |  619 | `{` |
|       - |  620 | `	ph7_class_instance *pThis;` |
|       - |  621 | `	ph7_value *pFn;` |
|       - |  622 | `	SyString sAttr;` |
|    1923 |  623 | `	if( !VmValueIsClosure(pVm, pVal) ){` |
|     ! 0 |  624 | `		return SXERR_NOTFOUND;` |
|       - |  625 | `	}` |
|    1923 |  626 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    1923 |  627 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    1923 |  628 | `	pFn = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    1923 |  629 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 |  630 | `		return SXERR_NOTFOUND; /* malformed/uninitialised closure */` |
|       - |  631 | `	}` |
|       - |  632 | `	/* Only a bound/static first-class callable (rare) carries $__this/$__scope; the` |
|       - |  633 | `	 * VM_INSTANCE_FCC_BOUND flag (set by VmCreateClosure) keeps the hot plain-closure path` |
|       - |  634 | `	 * to the single $__fn lookup above instead of two extra attribute lookups per dispatch. */` |
|    1923 |  635 | `	if( pThis->iFlags & VM_INSTANCE_FCC_BOUND ){` |
|       - |  636 | `		ph7_value *pBound, *pScope;` |
|       - |  637 | `		int bBoundObj, bScope;` |
|     107 |  638 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     107 |  639 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     107 |  640 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     107 |  641 | `		pScope = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     107 |  642 | `		bBoundObj = pBound && (pBound->iFlags & MEMOBJ_OBJ);` |
|     107 |  643 | `		bScope = pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0;` |
|     107 |  644 | `		if( bBoundObj \|\| bScope ){` |
|       - |  645 | `			/* Method/static first-class callable -> [ target, "method" ] array callable. */` |
|       - |  646 | `			ph7_hashmap *pMap;` |
|       - |  647 | `			ph7_value sTarget, sMeth;` |
|       - |  648 | `			sxi32 rc;` |
|     107 |  649 | `			if( bBoundObj ){` |
|      75 |  650 | `				ph7_class_instance *pBoundObj = (ph7_class_instance *)pBound->x.pOther;` |
|       - |  651 | ``				/* A bound PLAIN closure (`function(){…}->bindTo($o)`): $__fn names a function, not a`` |
|       - |  652 | `				 * method of the bound object's class, so a [obj,method] array callable would fail` |
|       - |  653 | `				 * method resolution. Stash the bound object (own a ref) so the OP_CALL user-function` |
|       - |  654 | `				 * frame setup injects it as $this, and return the plain $__fn string for a normal` |
|       - |  655 | `				 * function dispatch. */` |
|     111 |  656 | `				if( PH7_ClassExtractMethod(pBoundObj->pClass,` |
|     112 |  657 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){` |
|       - |  658 | `					/* Only a USER function (anonymous closure / named fn in hFunction) reads $this and` |
|       - |  659 | `					 * reaches the OP_CALL user-function frame-setup that consumes pClosureThis. A HOST` |
|       - |  660 | `					 * function (e.g. Closure::fromCallable('strlen')->bindTo($o)) ignores $this and` |
|       - |  661 | `					 * dispatches via the host path which never consumes the transient — setting it` |
|       - |  662 | `					 * there would leak the ref and inject a stale $this into the next call. */` |
|      48 |  663 | `					if( SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|      33 |  664 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|      33 |  665 | `						pBoundObj->iRef++;` |
|      33 |  666 | `						pVm->pClosureThis = pBoundObj;` |
|       - |  667 | `						/* Carry the bound scope (if set) so private/protected member access inside` |
|       - |  668 | `						 * the closure body resolves against it — bindTo($o, Scope::class) / call($o). */` |
|      33 |  669 | `						if( bScope ){` |
|      34 |  670 | `							pVm->pClosureScope = PH7_VmExtractClass(pVm,` |
|      22 |  671 | `								(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      11 |  672 | `						}` |
|      16 |  673 | `					}` |
|      33 |  674 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      33 |  675 | `					return SXRET_OK;` |
|       - |  676 | `				}` |
|      22 |  677 | `			}else{` |
|       - |  678 | ``				/* Scope-only rebind of a PLAIN closure (`bindTo(null, Scope::class)`):`` |
|       - |  679 | `				 * $__fn names a function, not a static method of the scope class, so` |
|       - |  680 | `				 * the [scope, method] array callable below would fail method` |
|       - |  681 | `				 * resolution. Dispatch the plain $__fn string and carry the scope for` |
|       - |  682 | `				 * private/protected visibility (pClosureThis stays unset — no $this).` |
|       - |  683 | `				 * A static-method FCC ($__fn really is a method of the scope class)` |
|       - |  684 | `				 * falls through to the array-callable path. */` |
|      49 |  685 | `				ph7_class *pScopeClass = PH7_VmExtractClass(pVm,` |
|      32 |  686 | `					(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      32 |  687 | `				if( pScopeClass == 0` |
|      33 |  688 | `				 \|\| PH7_ClassExtractMethod(pScopeClass,` |
|      48 |  689 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){` |
|       6 |  690 | `					if( pScopeClass` |
|       7 |  691 | `					 && SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|       6 |  692 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|       7 |  693 | `						pVm->pClosureScope = pScopeClass;` |
|       3 |  694 | `					}` |
|       7 |  695 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|       7 |  696 | `					return SXRET_OK;` |
|       - |  697 | `				}` |
|       - |  698 | `			}` |
|      69 |  699 | `			pMap = PH7_NewHashmap(&(*pVm), 0, 0);` |
|      69 |  700 | `			if( pMap == 0 ){` |
|     ! 0 |  701 | `				return SXERR_NOTFOUND;` |
|       - |  702 | `			}` |
|      69 |  703 | `			PH7_MemObjInit(pVm, &sTarget);` |
|      69 |  704 | `			PH7_MemObjInit(pVm, &sMeth);` |
|      69 |  705 | `			if( bBoundObj ){` |
|      43 |  706 | `				PH7_MemObjStore(pBound, &sTarget); /* bound object (iRef++) -> binds $this */` |
|      22 |  707 | `			}else{` |
|      27 |  708 | `				PH7_MemObjStringAppend(&sTarget, SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob));` |
|       - |  709 | `			}` |
|      69 |  710 | `			PH7_MemObjStringAppend(&sMeth, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      69 |  711 | `			rc = PH7_HashmapInsert(pMap, 0, &sTarget);` |
|      69 |  712 | `			if( rc == SXRET_OK ){` |
|      69 |  713 | `				rc = PH7_HashmapInsert(pMap, 0, &sMeth);` |
|      34 |  714 | `			}` |
|      69 |  715 | `			PH7_MemObjRelease(&sTarget);` |
|      69 |  716 | `			PH7_MemObjRelease(&sMeth);` |
|      69 |  717 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  718 | `				PH7_HashmapRelease(pMap, TRUE); /* free the partial map, no leak */` |
|     ! 0 |  719 | `				return SXERR_NOTFOUND;` |
|       - |  720 | `			}` |
|      69 |  721 | `			pOut->x.pOther = pMap;` |
|      69 |  722 | `			MemObjSetType(pOut, MEMOBJ_HASHMAP);` |
|      69 |  723 | `			return SXRET_OK;` |
|       - |  724 | `		}` |
|     ! 0 |  725 | `	}` |
|    1817 |  726 | `	PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    1817 |  727 | `	return SXRET_OK;` |
|     964 |  728 | `}` |
|       - |  729 | `/*` |
|       - |  730 | `` * Resolve the scope class for a STATIC first-class callable `T::m(...)`, where T is a`` |
|       - |  731 | ` * class-name STRING value: handles the self/static/parent keywords against the live class` |
|       - |  732 | ` * context (so the actual class is bound at FCC-creation time, like PHP), and falls back to` |
|       - |  733 | ` * an explicit class-name lookup. Mirrors the static OP_MEMBER resolution. Returns 0 if the` |
|       - |  734 | ` * class cannot be resolved.` |
|       - |  735 | ` */` |
|      62 |  736 | `PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget)` |
|       1 |  737 | `{` |
|      63 |  738 | `	const char *zCls = (const char *)SyBlobData(&pTarget->sBlob);` |
|      63 |  739 | `	sxu32 nCls = (sxu32)SyBlobLength(&pTarget->sBlob);` |
|       - |  740 | `	ph7_class *pClass;` |
|      63 |  741 | `	if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|      17 |  742 | `		pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      17 |  743 | `		if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|     ! 0 |  744 | `			pClass = PH7_VmPeekTopClass(&(*pVm)); /* self:: in a trait -> using class */` |
|       1 |  745 | `		}` |
|      55 |  746 | `	}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|      29 |  747 | `		pClass = PH7_VmPeekTopClass(&(*pVm));     /* late static binding */` |
|      33 |  748 | `	}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       7 |  749 | `		pClass = PH7_VmResolveParentClass(&(*pVm));` |
|       4 |  750 | `	}else{` |
|      13 |  751 | `		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - |  752 | `	}` |
|      63 |  753 | `	return pClass;` |
|       1 |  754 | `}` |
|       - |  755 | `/*` |
|       - |  756 | ` * Create a Closure object wrapping a callable name (+ optional bound $this object and/or` |
|       - |  757 | `` * scope class-name, for the method/static first-class callables `$o->m(...)`/`C::m(...)`).`` |
|       - |  758 | ` * Mirrors the Generator/Fiber "object carries its state in private attributes" pattern.` |
|       - |  759 | ` * Returns the fresh instance (iRef == 0; caller takes the reference), or 0 on OOM.` |
|       - |  760 | ` */` |
|    1244 |  761 | `PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName,` |
|       - |  762 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       5 |  763 | `{` |
|       - |  764 | `	ph7_class_instance *pObj;` |
|       - |  765 | `	ph7_value *pAttr;` |
|       - |  766 | `	SyString sAttr;` |
|    1249 |  767 | `	if( pVm->pClosureClass == 0 ){` |
|     ! 0 |  768 | `		return 0;` |
|       - |  769 | `	}` |
|    1249 |  770 | `	pObj = PH7_NewClassInstance(&(*pVm), pVm->pClosureClass);` |
|    1249 |  771 | `	if( pObj == 0 ){` |
|     ! 0 |  772 | `		return 0;` |
|       - |  773 | `	}` |
|    1249 |  774 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    1249 |  775 | `	pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|    1249 |  776 | `	if( pAttr ){` |
|    1249 |  777 | `		PH7_MemObjStringAppend(pAttr, pName->zString, pName->nByte);` |
|     622 |  778 | `	}` |
|    1249 |  779 | `	if( pBoundThis ){` |
|      39 |  780 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|      39 |  781 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|      39 |  782 | `		if( pAttr ){` |
|      39 |  783 | `			pAttr->x.pOther = pBoundThis;` |
|      39 |  784 | `			MemObjSetType(pAttr, MEMOBJ_OBJ);` |
|      39 |  785 | `			pBoundThis->iRef++; /* keep the bound object alive for the closure's lifetime */` |
|      19 |  786 | `		}` |
|      19 |  787 | `	}` |
|    1249 |  788 | `	if( pScope && pScope->nByte ){` |
|      65 |  789 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      65 |  790 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|      65 |  791 | `		if( pAttr ){` |
|      65 |  792 | `			PH7_MemObjStringAppend(pAttr, pScope->zString, pScope->nByte);` |
|      32 |  793 | `		}` |
|      32 |  794 | `	}` |
|    1249 |  795 | `	if( pBoundThis \|\| (pScope && pScope->nByte) ){` |
|       - |  796 | `		/* Mark bound/static FCC closures so VmClosureUnwrap can skip the $__this/$__scope` |
|       - |  797 | `		 * lookups on the hot plain-closure dispatch path. */` |
|      65 |  798 | `		pObj->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      32 |  799 | `	}` |
|    1249 |  800 | `	return pObj;` |
|     627 |  801 | `}` |
|       - |  802 | `/*` |
|       - |  803 | ` * Exported wrapper around VmCreateClosure for builtin libraries outside this` |
|       - |  804 | ` * file (ReflectionFunction::getClosure / ReflectionMethod::getClosure).` |
|       - |  805 | ` */` |
|       6 |  806 | `PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm, const SyString *pName,` |
|       - |  807 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       1 |  808 | `{` |
|       7 |  809 | `	return VmCreateClosure(&(*pVm), pName, pBoundThis, pScope);` |
|       1 |  810 | `}` |
|       - |  811 | `/*` |
|       - |  812 | ` * Exported wrapper around the typed/readonly property store enforcement for` |
|       - |  813 | ` * ReflectionProperty::setValue (vm_builtin_reflection.c). Same contract:` |
|       - |  814 | ` * SXRET_OK (value possibly coerced in place), PH7_EXCEPTION, or PH7_ABORT.` |
|       - |  815 | ` */` |
|       4 |  816 | `PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|       1 |  817 | `{` |
|       5 |  818 | `	return VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pValue,0);` |
|       1 |  819 | `}` |
|       - |  820 | `/*` |
|       - |  821 | ` * Exported reference-table probe for ReflectionReference::fromArrayElement` |
|       - |  822 | ` * (vm_builtin_reflection.c). Returns the number of links (frame variables +` |
|       - |  823 | ` * array entries) attached to the slot's reference record, 0 when the slot` |
|       - |  824 | ` * has none — an array element is a PHP reference when this is >= 2.` |
|       - |  825 | ` */` |
|       6 |  826 | `PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx)` |
|       1 |  827 | `{` |
|       7 |  828 | `	VmRefObj *pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       7 |  829 | `	if( pRef == 0 ){` |
|     ! 0 |  830 | `		return 0;` |
|       - |  831 | `	}` |
|       7 |  832 | `	return (int)(SySetUsed(&pRef->aReference) + SySetUsed(&pRef->aArrEntries));` |
|       4 |  833 | `}` |
|       - |  834 | `/*` |
|       - |  835 | `` * First-class callable over an arbitrary callable VALUE: `($expr)(...)`.`` |
|       - |  836 | ` * Normalize a validated callable value into a fresh Closure, reusing VmCreateClosure (the same` |
|       - |  837 | ` * object the method/static first-class-callable paths mint, so dispatch round-trips identically` |
|       - |  838 | ` * via VmClosureUnwrap). Handles the three remaining callable shapes a value can hold:` |
|       - |  839 | ` *   - a function-NAME string          -> plain closure ($__fn = name)` |
|       - |  840 | ` *   - a [target, method] array callable -> bound (object target -> $this) / static (class-name` |
|       - |  841 | ` *     target -> scope) closure, mirroring the [obj,m]/[class,m] decode in PH7_VmIsCallable` |
|       - |  842 | ` *   - an __invoke object               -> closure bound to the object's __invoke` |
|       - |  843 | ` * An existing Closure returns 0 here (it is already a Closure — the caller keeps it as-is), so this` |
|       - |  844 | ` * stays idempotent even for a direct caller. Returns the fresh instance (iRef == 0; caller takes the` |
|       - |  845 | ` * reference) or 0 if the value is an existing Closure / not a normalizable callable / on OOM — in` |
|       - |  846 | ` * which case the caller leaves the value untouched (graceful degradation). This is the generic` |
|       - |  847 | ` * "callable value -> Closure" primitive: the body is FCC-agnostic and self-contained, so the future` |
|       - |  848 | ` * Closure::bind/fromCallable work (Increment 2) can call it directly.` |
|       - |  849 | ` */` |
|      56 |  850 | `PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue)` |
|       1 |  851 | `{` |
|       - |  852 | `	/* A Closure is already callable; never double-wrap it (this also stops the __invoke branch` |
|       - |  853 | `	 * below from binding a closure to its OWN __invoke). The OP_LOAD_FCC caller intercepts a` |
|       - |  854 | `	 * Closure first too, but guarding here keeps the primitive safe for a direct Increment-2 caller. */` |
|      57 |  855 | `	if( VmValueIsClosure(pVm, pValue) ){` |
|     ! 0 |  856 | `		return 0;` |
|       - |  857 | `	}` |
|      57 |  858 | `	if( !PH7_VmIsCallable(pVm, pValue, TRUE) ){` |
|     ! 0 |  859 | `		return 0;` |
|       - |  860 | `	}` |
|      57 |  861 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  862 | `		SyString sName;` |
|      33 |  863 | `		SyStringInitFromBuf(&sName, SyBlobData(&pValue->sBlob), SyBlobLength(&pValue->sBlob));` |
|      33 |  864 | `		return VmCreateClosure(pVm, &sName, 0, 0);` |
|       - |  865 | `	}` |
|      25 |  866 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  867 | `		/* [target, method] — same two-slot decode PH7_VmIsCallable uses to validate it. */` |
|      17 |  868 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - |  869 | `		ph7_value *pTarget, *pMeth;` |
|       - |  870 | `		SyString sName;` |
|      17 |  871 | `		if( pMap->nEntry != 2 ){` |
|     ! 0 |  872 | `			return 0;` |
|       - |  873 | `		}` |
|      17 |  874 | `		pTarget = (ph7_value *)SySetAt(&pVm->aMemObj, pMap->pFirst->nValIdx);` |
|      17 |  875 | `		pMeth   = (ph7_value *)SySetAt(&pVm->aMemObj, pMap->pFirst->pPrev->nValIdx);` |
|      16 |  876 | `		if( pTarget == 0 \|\| pMeth == 0 \|\| (pMeth->iFlags & MEMOBJ_STRING) == 0` |
|      17 |  877 | `			\|\| SyBlobLength(&pMeth->sBlob) == 0 ){` |
|     ! 0 |  878 | `			return 0;` |
|       - |  879 | `		}` |
|      17 |  880 | `		SyStringInitFromBuf(&sName, SyBlobData(&pMeth->sBlob), SyBlobLength(&pMeth->sBlob));` |
|      17 |  881 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      11 |  882 | `			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      11 |  883 | `			return VmCreateClosure(pVm, &sName, pBoundThis, &pBoundThis->pClass->sName);` |
|     ! 0 |  884 | `		}else{` |
|       - |  885 | `			/* [class-name, method] static callable -> bind the resolved scope. A runtime array` |
|       - |  886 | `			 * callable carries a concrete class name (never self/static/parent), so a plain class` |
|       - |  887 | ``			 * lookup is correct — unlike the syntactic `C::m(...)` path, which must resolve`` |
|       - |  888 | `			 * self/static/parent via VmFccResolveScope. Matches PH7_VmIsCallable's own decode. */` |
|       7 |  889 | `			ph7_class *pScopeCls = PH7_VmExtractClassFromValue(pVm, pTarget);` |
|       7 |  890 | `			return pScopeCls ? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;` |
|       - |  891 | `		}` |
|       - |  892 | `	}` |
|       9 |  893 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  894 | `		/* __invoke object (a real Closure is intercepted by the caller before this point). */` |
|       9 |  895 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|       - |  896 | `		SyString sInvoke;` |
|       9 |  897 | `		SyStringInitFromBuf(&sInvoke, "__invoke", sizeof("__invoke") - 1);` |
|       9 |  898 | `		return VmCreateClosure(pVm, &sInvoke, pObj, &pObj->pClass->sName);` |
|       - |  899 | `	}` |
|       - |  900 | `	/* Unreachable in practice — the PH7_VmIsCallable gate admits only string/array/object, all` |
|       - |  901 | `	 * handled above; kept to satisfy the non-void return path. */` |
|     ! 0 |  902 | `	return 0;` |
|      29 |  903 | `}` |
|       - |  904 | `/*` |
|       - |  905 | ` * Return a fresh Closure instance (iRef==0 from create/clone) as a builtin result, taking the` |
|       - |  906 | ` * one reference into pCtx->pRet — mirrors the OP_LOAD_FCC store convention.` |
|       - |  907 | ` */` |
|      54 |  908 | `static int VmClosureResult(ph7_context *pCtx, ph7_class_instance *pClosure)` |
|       1 |  909 | `{` |
|      55 |  910 | `	if( pClosure == 0 ){` |
|     ! 0 |  911 | `		ph7_result_null(pCtx);` |
|     ! 0 |  912 | `		return PH7_OK;` |
|       - |  913 | `	}` |
|      55 |  914 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      55 |  915 | `	pClosure->iRef++;` |
|      55 |  916 | `	pCtx->pRet->x.pOther = pClosure;` |
|      55 |  917 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|      55 |  918 | `	return PH7_OK;` |
|      28 |  919 | `}` |
|       - |  920 | `/*` |
|       - |  921 | ` * Overwrite a Closure clone's $__this (bound object, or 0 to unbind) and, when pScope != 0, its` |
|       - |  922 | ` * $__scope (the class-name string; pScope->nByte==0 clears it). pScope==0 leaves $__scope as the` |
|       - |  923 | ` * clone inherited it (PHP's "static" = keep-scope). Refreshes VM_INSTANCE_FCC_BOUND from the result.` |
|       - |  924 | ` * The clone inherited a ref on the original $__this (PH7_CloneClassInstance copies via MemObjStore);` |
|       - |  925 | ` * this drops that and takes one on pNewThis.` |
|       - |  926 | ` */` |
|      40 |  927 | `static void VmClosureRebind(ph7_class_instance *pClone,` |
|       - |  928 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|       1 |  929 | `{` |
|       - |  930 | `	SyString sAttr;` |
|       - |  931 | `	ph7_value *pThisAttr, *pScopeAttr;` |
|      41 |  932 | `	int bBound = 0;` |
|      41 |  933 | `	SyStringInitFromBuf(&sAttr, "__this", 6);` |
|      41 |  934 | `	pThisAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      41 |  935 | `	if( pThisAttr ){` |
|       - |  936 | `		/* PH7_MemObjRelease already drops the cloned-in object's refcount for an OBJ value —` |
|       - |  937 | `		 * do NOT also decrement by hand (that double-frees the original bound object). */` |
|      41 |  938 | `		PH7_MemObjRelease(pThisAttr);` |
|      41 |  939 | `		if( pNewThis ){` |
|      33 |  940 | `			pThisAttr->x.pOther = pNewThis;` |
|      33 |  941 | `			MemObjSetType(pThisAttr, MEMOBJ_OBJ);` |
|      33 |  942 | `			pNewThis->iRef++;` |
|      16 |  943 | `		}` |
|      20 |  944 | `	}` |
|      41 |  945 | `	if( pScope ){` |
|      29 |  946 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      29 |  947 | `		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      29 |  948 | `		if( pScopeAttr ){` |
|      29 |  949 | `			PH7_MemObjRelease(pScopeAttr);` |
|      29 |  950 | `			if( pScope->nByte ){` |
|      29 |  951 | `				PH7_MemObjStringAppend(pScopeAttr, pScope->zString, pScope->nByte);` |
|      14 |  952 | `			}` |
|      14 |  953 | `		}` |
|      14 |  954 | `	}` |
|       - |  955 | `	/* Refresh the FCC_BOUND fast-path flag from the resulting $__this/$__scope. pThisAttr already` |
|       - |  956 | `	 * points at the final $__this slot; only $__scope needs a (re)fetch — the top half fetched it` |
|       - |  957 | `	 * just for pScope != 0. */` |
|      41 |  958 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      41 |  959 | `	pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      40 |  960 | `	if( (pThisAttr && (pThisAttr->iFlags & MEMOBJ_OBJ))` |
|      25 |  961 | `		\|\| (pScopeAttr && (pScopeAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeAttr->sBlob) > 0) ){` |
|      39 |  962 | `		bBound = 1;` |
|      19 |  963 | `	}` |
|      41 |  964 | `	if( bBound ){` |
|      39 |  965 | `		pClone->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      20 |  966 | `	}else{` |
|       3 |  967 | `		pClone->iFlags &= ~VM_INSTANCE_FCC_BOUND;` |
|       - |  968 | `	}` |
|      41 |  969 | `}` |
|       - |  970 | `/*` |
|       - |  971 | ` * Resolve the bindTo/bind/call $scope argument to a class-name SyString.` |
|       - |  972 | ` * Returns 1 and fills *pOut if $__scope should be replaced (pOut->nByte==0 means "clear");` |
|       - |  973 | ` * returns 0 to leave the scope unchanged (the PHP "static" sentinel / scope omitted).` |
|       - |  974 | ` */` |
|      40 |  975 | `static int VmClosureResolveScope(ph7_value *pScopeArg, SyString *pOut)` |
|       1 |  976 | `{` |
|      41 |  977 | `	if( pScopeArg == 0 ){` |
|     ! 0 |  978 | `		return 0; /* keep */` |
|       - |  979 | `	}` |
|      40 |  980 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeArg->sBlob) == 6` |
|      26 |  981 | `		&& SyMemcmp((const void *)SyBlobData(&pScopeArg->sBlob), (const void *)"static", 6) == 0 ){` |
|      13 |  982 | `		return 0; /* "static" -> keep current scope */` |
|       - |  983 | `	}` |
|      29 |  984 | `	if( pScopeArg->iFlags & MEMOBJ_NULL ){` |
|     ! 0 |  985 | `		SyStringInitFromBuf(pOut, 0, 0); /* unscoped */` |
|     ! 0 |  986 | `		return 1;` |
|       - |  987 | `	}` |
|      29 |  988 | `	if( pScopeArg->iFlags & MEMOBJ_OBJ ){` |
|       3 |  989 | `		ph7_class_instance *pScopeObj = (ph7_class_instance *)pScopeArg->x.pOther;` |
|       3 |  990 | `		*pOut = pScopeObj->pClass->sName;` |
|       3 |  991 | `		return 1;` |
|       - |  992 | `	}` |
|      27 |  993 | `	if( pScopeArg->iFlags & MEMOBJ_STRING ){` |
|      27 |  994 | `		SyStringInitFromBuf(pOut, (const char *)SyBlobData(&pScopeArg->sBlob), SyBlobLength(&pScopeArg->sBlob));` |
|      27 |  995 | `		return 1;` |
|       - |  996 | `	}` |
|     ! 0 |  997 | `	return 0;` |
|      21 |  998 | `}` |
|       - |  999 | `/*` |
|       - | 1000 | ` * Closure::bindTo($newThis, $scope='static') / Closure::bind($closure, $newThis, $scope='static').` |
|       - | 1001 | ` * Clone the receiver and rebind $this/$scope; returns the new Closure (NULL on a non-Closure` |
|       - | 1002 | ` * receiver, matching PHP's failure mode).` |
|       - | 1003 | ` */` |
|      42 | 1004 | `PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 1005 | `{` |
|      43 | 1006 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1007 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|       - | 1008 | `	ph7_value *pNewThisArg;` |
|       - | 1009 | `	SyString sScope;` |
|      43 | 1010 | `	const SyString *pScopePtr = 0;` |
|      43 | 1011 | `	if( nArg < 2 \|\| !VmValueIsClosure(pVm, apArg[0]) ){` |
|     ! 0 | 1012 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1013 | `		return PH7_OK;` |
|       - | 1014 | `	}` |
|      43 | 1015 | `	pClosure = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      43 | 1016 | `	pNewThisArg = apArg[1];` |
|      43 | 1017 | `	if( pNewThisArg->iFlags & MEMOBJ_NULL ){` |
|       9 | 1018 | `		pNewThis = 0;` |
|      39 | 1019 | `	}else if( pNewThisArg->iFlags & MEMOBJ_OBJ ){` |
|      35 | 1020 | `		pNewThis = (ph7_class_instance *)pNewThisArg->x.pOther;` |
|      18 | 1021 | `	}else{` |
|     ! 0 | 1022 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1023 | `			"Closure::bindTo(): Argument #1 ($newThis) must be of type ?object");` |
|       - | 1024 | `	}` |
|      43 | 1025 | `	if( pNewThis ){` |
|       - | 1026 | `		/* php refuses to bind an instance to a static closure: warning + null */` |
|       - | 1027 | `		SyString sAttr;` |
|       - | 1028 | `		ph7_value *pFn;` |
|      35 | 1029 | `		SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|      35 | 1030 | `		pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      35 | 1031 | `		if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){` |
|      35 | 1032 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      35 | 1033 | `			if( pEntry && (((ph7_vm_func *)pEntry->pUserData)->iFlags & VM_FUNC_STATIC_CL) ){` |
|       3 | 1034 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1035 | `					"Cannot bind an instance to a static closure, this will be an error in PHP 9");` |
|       3 | 1036 | `				ph7_result_null(pCtx);` |
|       3 | 1037 | `				return PH7_OK;` |
|       - | 1038 | `			}` |
|      16 | 1039 | `		}` |
|      16 | 1040 | `	}` |
|      41 | 1041 | `	if( VmClosureResolveScope((nArg > 2) ? apArg[2] : 0, &sScope) ){` |
|      29 | 1042 | `		pScopePtr = &sScope;` |
|      14 | 1043 | `	}` |
|      41 | 1044 | `	pClone = PH7_CloneClassInstance(pClosure);` |
|      41 | 1045 | `	if( pClone == 0 ){` |
|     ! 0 | 1046 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1047 | `		return PH7_OK;` |
|       - | 1048 | `	}` |
|      41 | 1049 | `	VmClosureRebind(pClone, pNewThis, pScopePtr);` |
|      41 | 1050 | `	return VmClosureResult(pCtx, pClone);` |
|      22 | 1051 | `}` |
|       - | 1052 | `/*` |
|       - | 1053 | ` * Closure::fromCallable($callable) — normalize any callable value to a Closure (reuses the` |
|       - | 1054 | ` * VmFccWrapValue primitive); idempotent on a Closure; TypeError on a non-callable.` |
|       - | 1055 | ` */` |
|      16 | 1056 | `PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 1057 | `{` |
|      17 | 1058 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1059 | `	ph7_class_instance *pClosure;` |
|      17 | 1060 | `	if( nArg < 1 ){` |
|     ! 0 | 1061 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1062 | `			"Closure::fromCallable() expects exactly 1 argument, 0 given");` |
|       - | 1063 | `	}` |
|      17 | 1064 | `	if( VmValueIsClosure(pVm, apArg[0]) ){` |
|       3 | 1065 | `		ph7_result_value(pCtx, apArg[0]); /* already a Closure: idempotent */` |
|       3 | 1066 | `		return PH7_OK;` |
|       - | 1067 | `	}` |
|      15 | 1068 | `	pClosure = VmFccWrapValue(pVm, apArg[0]);` |
|      15 | 1069 | `	if( pClosure == 0 ){` |
|     ! 0 | 1070 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1071 | `			"Closure::fromCallable(): Argument #1 ($callback) is not a valid callback");` |
|       - | 1072 | `	}` |
|      15 | 1073 | `	return VmClosureResult(pCtx, pClosure);` |
|       9 | 1074 | `}` |
|       - | 1075 | `/*` |
|       - | 1076 | ` * Fiber::suspend($value = null) — static method.` |
|       - | 1077 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|       - | 1078 | ` */` |
|     358 | 1079 | `PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1080 | `{` |
|     363 | 1081 | `	ph7_vm *pVm = pCtx->pVm;` |
|     363 | 1082 | `	if( pVm->pActiveCtx == 0 ){` |
|     ! 0 | 1083 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1084 | `			"Cannot suspend outside of a fiber");` |
|       - | 1085 | `	}` |
|       - | 1086 | `	/* Stage 4 scoped divergence: the trampoline only makes PHP->PHP CALLs` |
|       - | 1087 | `	 * iterative. Every OTHER re-entry runs on a fresh native VmByteCodeExec` |
|       - | 1088 | `	 * activation (nVmExecDepth bumped) that PH7_SUSPEND cannot unwind across` |
|       - | 1089 | `	 * without real coroutine stacks (BYTECODE.md §2.4): a C->PHP callback` |
|       - | 1090 | `	 * (usort/array_map/preg_replace_callback comparator), and — because fibers` |
|       - | 1091 | `	 * use the LEGACY (non-inline) try/catch machinery — a suspend inside a` |
|       - | 1092 | `	 * fiber's catch/finally body, a match/switch arm, or eval()/include()'d` |
|       - | 1093 | `	 * code, all of which run via VmLocalExec. php does all of these via full` |
|       - | 1094 | `	 * native-stack switching; PHL raises a catchable FiberError instead of the` |
|       - | 1095 | `	 * old silent corruption. A suspend in a fiber's try BODY (not catch/finally)` |
|       - | 1096 | `	 * runs in the main dispatch loop and parks normally. A recorded` |
|       - | 1097 | `	 * residual; making the catch/finally case work needs fibers on the inline` |
|       - | 1098 | `	 * try machinery (the generator ROOT C path), a follow-up. */` |
|     363 | 1099 | `	if( pVm->nVmExecDepth != pVm->pActiveCtx->nBodyExecDepth ){` |
|       6 | 1100 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1101 | `			"Cannot suspend across an internal call boundary");` |
|       - | 1102 | `	}` |
|     359 | 1103 | `	if( nArg > 0 ){` |
|     359 | 1104 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|     182 | 1105 | `	}else{` |
|     ! 0 | 1106 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|       - | 1107 | `	}` |
|     359 | 1108 | `	return PH7_SUSPEND;` |
|     184 | 1109 | `}` |
|       - | 1110 | `/*` |
|       - | 1111 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|       - | 1112 | ` * Actual resolution is deferred to start() so that overload selection` |
|       - | 1113 | ` * and closure-environment binding happen with the correct argument context.` |
|       - | 1114 | ` */` |
|     260 | 1115 | `PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1116 | `{` |
|       - | 1117 | `	ph7_class_instance *pThis;` |
|       - | 1118 | `	ph7_value *pAttr;` |
|       - | 1119 | `	SyString sAttrName;` |
|     265 | 1120 | `	if( nArg < 2 ){` |
|     ! 0 | 1121 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1122 | `			"Fiber::__construct() expects a callable argument");` |
|       - | 1123 | `	}` |
|     265 | 1124 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1125 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1126 | `			"Fiber::__construct(): invalid $this");` |
|       - | 1127 | `	}` |
|     265 | 1128 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     265 | 1129 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|     ! 0 | 1130 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1131 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|       - | 1132 | `	}` |
|       - | 1133 | `	/* Basic validation: callable must be a string or closure (object) */` |
|     265 | 1134 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|     ! 0 | 1135 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1136 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|       - | 1137 | `	}` |
|       - | 1138 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|     265 | 1139 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     265 | 1140 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     265 | 1141 | `	if( pAttr ){` |
|     265 | 1142 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|     130 | 1143 | `	}` |
|     265 | 1144 | `	return PH7_OK;` |
|     135 | 1145 | `}` |
|       - | 1146 | `/*` |
|       - | 1147 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|       - | 1148 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|       - | 1149 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|       - | 1150 | ` * so that start() can bind it as $this for the closure environment.` |
|       - | 1151 | ` */` |
|     258 | 1152 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|       - | 1153 | `	ph7_class_instance **ppThis)` |
|       5 | 1154 | `{` |
|     263 | 1155 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1156 | `	ph7_value *pCallable;` |
|       - | 1157 | `	SyString sAttrName;` |
|     263 | 1158 | `	*ppThis = 0;` |
|     263 | 1159 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     263 | 1160 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|     263 | 1161 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|     ! 0 | 1162 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|     ! 0 | 1163 | `		return 0;` |
|       - | 1164 | `	}` |
|     263 | 1165 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|       - | 1166 | `		/* String callable — look up in user functions with overload support */` |
|       - | 1167 | `		SyString sName;` |
|       - | 1168 | `		SyHashEntry *pEntry;` |
|       - | 1169 | `		ph7_vm_func *pFunc;` |
|     232 | 1170 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|     232 | 1171 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|     232 | 1172 | `		if( pEntry == 0 ){` |
|     ! 0 | 1173 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|     ! 0 | 1174 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|     ! 0 | 1175 | `			return 0;` |
|       - | 1176 | `		}` |
|     232 | 1177 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     232 | 1178 | `		return pFunc;` |
|     ! 0 | 1179 | `	}else{` |
|      34 | 1180 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|       - | 1181 | `		ph7_class_method *pMethod;` |
|      34 | 1182 | `		if( VmValueIsClosure(pVm, pCallable) ){` |
|       - | 1183 | `			/* A real Closure object: unwrap to its underlying callable name (the single` |
|       - | 1184 | `			 * source of truth, VmClosureUnwrap) and resolve that function. Its captured` |
|       - | 1185 | ``			 * environment (including any `$this`) rides along in the named function's`` |
|       - | 1186 | `			 * aClosureEnv, installed by VmFiberSetupFrame, so *ppThis stays 0. */` |
|       - | 1187 | `			ph7_value sName;` |
|      34 | 1188 | `			SyHashEntry *pEntry = 0;` |
|      34 | 1189 | `			PH7_MemObjInit(pVm, &sName);` |
|      34 | 1190 | `			if( VmClosureUnwrap(pVm, pCallable, &sName) == SXRET_OK ){` |
|      34 | 1191 | `				pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&sName.sBlob), SyBlobLength(&sName.sBlob));` |
|      15 | 1192 | `			}` |
|      34 | 1193 | `			PH7_MemObjRelease(&sName);` |
|      34 | 1194 | `			if( pEntry ){` |
|       - | 1195 | `				/* A BOUND closure parked its $this in the pClosureThis transient` |
|       - | 1196 | `				 * (VmClosureUnwrap): consume it as the fiber's $this — it wins over` |
|       - | 1197 | `				 * the creation-time env capture (VmFiberSetupFrame's skip). *ppThis` |
|       - | 1198 | `				 * is a borrow (the closure's $__this attr keeps the object alive` |
|       - | 1199 | `				 * through $__callable), so drop the parked ref. The scope transient` |
|       - | 1200 | `				 * is cleared alongside: fiber bodies don't model pBoundScope` |
|       - | 1201 | `				 * visibility (recorded residual), and a stale transient would` |
|       - | 1202 | `				 * poison the next OP_CALL's frame. */` |
|      34 | 1203 | `				if( pVm->pClosureThis ){` |
|     ! 0 | 1204 | `					*ppThis = pVm->pClosureThis;` |
|     ! 0 | 1205 | `					PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1206 | `					pVm->pClosureThis = 0;` |
|     ! 0 | 1207 | `				}` |
|      34 | 1208 | `				pVm->pClosureScope = 0;` |
|      34 | 1209 | `				return (ph7_vm_func *)pEntry->pUserData;` |
|       - | 1210 | `			}` |
|     ! 0 | 1211 | `			if( pVm->pClosureThis ){` |
|       - | 1212 | `				/* Failed resolution: drop the parked transient so it neither leaks` |
|       - | 1213 | `				 * nor poisons the next call. */` |
|     ! 0 | 1214 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1215 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 1216 | `			}` |
|     ! 0 | 1217 | `			pVm->pClosureScope = 0;` |
|     ! 0 | 1218 | `			PH7_VmThrowException(pCtx, "FiberError", "Fiber callable closure could not be resolved");` |
|     ! 0 | 1219 | `			return 0;` |
|       - | 1220 | `		}` |
|       - | 1221 | `		/* Object callable — resolve __invoke method */` |
|     ! 0 | 1222 | `		pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 1223 | `			sizeof("__invoke") - 1);` |
|     ! 0 | 1224 | `		if( pMethod == 0 ){` |
|     ! 0 | 1225 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1226 | `				"Fiber callable object has no __invoke method");` |
|     ! 0 | 1227 | `			return 0;` |
|       - | 1228 | `		}` |
|     ! 0 | 1229 | `		*ppThis = pClosure;` |
|     ! 0 | 1230 | `		return &pMethod->sFunc;` |
|       - | 1231 | `	}` |
|     134 | 1232 | `}` |
|       - | 1233 | `/*` |
|       - | 1234 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|       - | 1235 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|       - | 1236 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|       - | 1237 | ` */` |
|       - | 1238 | `/*` |
|       - | 1239 | ` * Enforce one formal parameter's declared type on an argument being installed` |
|       - | 1240 | ` * into a generator/fiber initial frame (band A #2). Mirrors the OP_CALL` |
|       - | 1241 | ` * install path's checks exactly: union types via VmCoerceToUnion, class and` |
|       - | 1242 | ` * pseudo types (VmCheckPseudoType + VmResolveTypeClass + instanceof, so` |
|       - | 1243 | ``  * interfaces/abstract classes and self/parent resolve), the bare `object` `` |
|       - | 1244 | ` * hint, and scalars via VmEnforceScalarType (weak-mode coercion in place,` |
|       - | 1245 | ` * strict rejection otherwise) — with the VM_FUNC_ARG_NULLABLE guard letting` |
|       - | 1246 | `` * null through for `?type` and implicit-nullable `Type $x = null` params.`` |
|       - | 1247 | ` * Pre-fix, VmFiberSetupFrame only xCast()ed on mismatch, so a typed` |
|       - | 1248 | ` * generator/fiber parameter silently coerced (g(int $x){yield $x;} g(null)` |
|       - | 1249 | ` * yielded int(0) where php throws TypeError at the call site).` |
|       - | 1250 | ` * Returns SXRET_OK (value possibly coerced) or the VmThrowTypeErrorForArg` |
|       - | 1251 | ` * status for the caller to route — normalized so an INLINE-caught throw` |
|       - | 1252 | ` * (VmThrowInline records only a pc-redirect and reports SXRET_OK) still` |
|       - | 1253 | ` * comes back as PH7_EXCEPTION: the binding must stop and the OP_CALL` |
|       - | 1254 | ` * generator block's PH7_INLINE_RESUME_BREAK consumes the redirect.` |
|       - | 1255 | ` */` |
|      18 | 1256 | `static sxi32 VmGenArgThrowStatus(ph7_vm *pVm, sxi32 rcThrow)` |
|       1 | 1257 | `{` |
|      19 | 1258 | `	if( rcThrow == SXRET_OK && pVm->pInlineInstr ){` |
|     ! 0 | 1259 | `		return PH7_EXCEPTION;` |
|       - | 1260 | `	}` |
|      19 | 1261 | `	return rcThrow;` |
|      10 | 1262 | `}` |
|      86 | 1263 | `static sxi32 VmEnforceGenArgType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_vm_func_arg *pFormal,` |
|       - | 1264 | `	sxu32 nArgPos, ph7_value *pVal, int bStrict, ph7_class *pSelfHint)` |
|       4 | 1265 | `{` |
|      90 | 1266 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|     ! 0 | 1267 | `		if( VmCoerceToUnion(pVm,pVal,&pFormal->aUnionAlts,` |
|     ! 0 | 1268 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,bStrict) != SXRET_OK ){` |
|       - | 1269 | `			const char *zGiven;` |
|     ! 0 | 1270 | `			const char *zExpected = "union";` |
|       - | 1271 | `			char zBuf[128];` |
|       - | 1272 | `			char zTypeBuf[128];` |
|     ! 0 | 1273 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 1274 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|     ! 0 | 1275 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 1276 | `				zGiven = "null";` |
|     ! 0 | 1277 | `			}else{` |
|     ! 0 | 1278 | `				zGiven = ph7_type_name(pVal);` |
|       - | 1279 | `			}` |
|     ! 0 | 1280 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|     ! 0 | 1281 | `				zExpected = VmSyStringToCStr(&pFormal->sTypeName,zTypeBuf,sizeof(zTypeBuf));` |
|     ! 0 | 1282 | `			}` |
|     ! 0 | 1283 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|     ! 0 | 1284 | `				&pFormal->sName,zExpected,zGiven));` |
|       - | 1285 | `		}` |
|     ! 0 | 1286 | `		return SXRET_OK;` |
|       - | 1287 | `	}` |
|      86 | 1288 | `	if( pFormal->nType == 0` |
|      66 | 1289 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|      54 | 1290 | `		return SXRET_OK;` |
|       - | 1291 | `	}` |
|      38 | 1292 | `	if( pFormal->nType == SXU32_HIGH ){` |
|       - | 1293 | `		/* Class or pseudo type */` |
|       3 | 1294 | `		SyString *pName = &pFormal->sClass;` |
|       - | 1295 | `		ph7_class *pClass;` |
|       3 | 1296 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|       3 | 1297 | `		if( rcPseudo == 0 ){` |
|       - | 1298 | `			char zTypeBuf[128],zGivenBuf[128];` |
|     ! 0 | 1299 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|     ! 0 | 1300 | `				&pFormal->sName,` |
|     ! 0 | 1301 | `				VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|     ! 0 | 1302 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1303 | `		}` |
|       3 | 1304 | `		pClass = (rcPseudo == 1) ? 0 : VmResolveTypeClass(&(*pVm),pName,pSelfHint);` |
|       3 | 1305 | `		if( pClass ){` |
|       5 | 1306 | `			int bBad = !((pVal->iFlags & MEMOBJ_OBJ)` |
|       2 | 1307 | `				&& PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass));` |
|       3 | 1308 | `			if( bBad ){` |
|       - | 1309 | `				char zTypeBuf[128],zGivenBuf[128];` |
|       5 | 1310 | `				return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       1 | 1311 | `					&pFormal->sName,` |
|       2 | 1312 | `					VmSyStringToCStr(&pClass->sName,zTypeBuf,sizeof(zTypeBuf)),` |
|       1 | 1313 | `					VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1314 | `			}` |
|     ! 0 | 1315 | `		}` |
|     ! 0 | 1316 | `		return SXRET_OK;` |
|       - | 1317 | `	}` |
|      36 | 1318 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|      19 | 1319 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|     ! 0 | 1320 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|     ! 0 | 1321 | `				&pFormal->sName,"object",ph7_type_name(pVal)));` |
|       - | 1322 | `		}` |
|      19 | 1323 | `		if( VmEnforceScalarType(pVal,pFormal->nType,bStrict) != SXRET_OK ){` |
|       - | 1324 | `			char zTypeBuf[128];` |
|      19 | 1325 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       6 | 1326 | `				&pFormal->sName,` |
|       6 | 1327 | `				VmScalarTypeName(pFormal->nType,&pFormal->sTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       6 | 1328 | `				ph7_type_name(pVal)));` |
|       - | 1329 | `		}` |
|       3 | 1330 | `	}` |
|      24 | 1331 | `	return SXRET_OK;` |
|      47 | 1332 | `}` |
|     596 | 1333 | `PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|       - | 1334 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg,` |
|       - | 1335 | `	int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg)` |
|       5 | 1336 | `{` |
|     601 | 1337 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|       - | 1338 | `	ph7_vm_func_arg *aFormalArg;` |
|       - | 1339 | `	sxu32 nFormal, n;` |
|     601 | 1340 | `	sxu32 nReqGF = 0, nNonVarGF = 0; /* too-few-args watermark (0 = exempt) */` |
|       - | 1341 | `	VmSlot sSlot;` |
|       - | 1342 | `	sxi32 rc;` |
|       - | 1343 | `	/* Install $this for closure/method callables */` |
|     601 | 1344 | `	if( pClosureThis ){` |
|       - | 1345 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      31 | 1346 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      31 | 1347 | `		if( pObj ){` |
|      31 | 1348 | `			pObj->x.pOther = pClosureThis;` |
|      31 | 1349 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      31 | 1350 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      14 | 1351 | `		}` |
|      14 | 1352 | `	}` |
|       - | 1353 | `	/* Install static variables */` |
|     601 | 1354 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|       - | 1355 | `		ph7_vm_func_static_var *aStatic;` |
|       - | 1356 | `		ph7_value *pVal;` |
|     ! 0 | 1357 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|     ! 0 | 1358 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|     ! 0 | 1359 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|     ! 0 | 1360 | `			if( pVal ){` |
|     ! 0 | 1361 | `				sSlot.pUserData = 0;` |
|     ! 0 | 1362 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|     ! 0 | 1363 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|     ! 0 | 1364 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|     ! 0 | 1365 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|     ! 0 | 1366 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);` |
|     ! 0 | 1367 | `				}` |
|     ! 0 | 1368 | `			}` |
|     ! 0 | 1369 | `		}` |
|     ! 0 | 1370 | `	}` |
|       - | 1371 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|     601 | 1372 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     601 | 1373 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       - | 1374 | `	/* Actual call arity for func_num_args()/func_get_args() (band A #4) */` |
|     601 | 1375 | `	pExecCtx->pFrame->nActualArgs = nArg;` |
|       - | 1376 | `	{` |
|       - | 1377 | `		/* Too-few-arguments watermark — checked per formal INSIDE the install` |
|       - | 1378 | `		 * loop below, after the passed args' type checks, matching php's` |
|       - | 1379 | `		 * RECV order (a type error on a passed argument beats the count` |
|       - | 1380 | `		 * error). Generators throw at the g(...) call site (message embeds` |
|       - | 1381 | `		 * it); fibers at Fiber::start() (php omits the call-site segment).` |
|       - | 1382 | `		 * Hosted builtin FUNCTIONS self-check; hosted-class methods get` |
|       - | 1383 | `		 * php's ZPP wording. */` |
|     601 | 1384 | `	if( (pFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){` |
|     601 | 1385 | `		nReqGF = VmFuncRequiredArgCount(pFunc,&nNonVarGF);` |
|     298 | 1386 | `	}` |
|       - | 1387 | `	}` |
|     669 | 1388 | `	for( n = 0; n < nFormal; n++ ){` |
|       - | 1389 | `		ph7_value *pObj;` |
|      94 | 1390 | `		if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       - | 1391 | `			/* Variadic formal: collect this and every remaining actual into a` |
|       - | 1392 | `			 * fresh array (php semantics — pre-fix nothing collected here, so a` |
|       - | 1393 | ``			 * `function g(int ...$xs)` generator saw a bare scalar in $xs).`` |
|       - | 1394 | `			 * Per-element checks mirror OP_CALL's variadic install: union via` |
|       - | 1395 | ``			 * the shared helper, scalar coerce/TypeError, `object` hint; a`` |
|       - | 1396 | `			 * class-typed variadic element is (like OP_CALL) not checked. */` |
|       5 | 1397 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       5 | 1398 | `			if( pObj ){` |
|       - | 1399 | `				sxu32 nVariadicIdx;` |
|       - | 1400 | `				ph7_hashmap *pMap;` |
|       - | 1401 | `				sxu32 k;` |
|       5 | 1402 | `				PH7_MemObjToHashmap(pObj);` |
|       - | 1403 | `				/* Capture the slot index now: PH7_HashmapInsert can reallocate` |
|       - | 1404 | `				 * pVm->aMemObj and dangle pObj (same hazard as OP_CALL's path). */` |
|       5 | 1405 | `				nVariadicIdx = pObj->nIdx;` |
|       5 | 1406 | `				pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      11 | 1407 | `				for( k = n; k < (sxu32)nArg; k++ ){` |
|       9 | 1408 | `					if( ((aFormalArg[n].iFlags & VM_FUNC_ARG_UNION)` |
|       7 | 1409 | `					   \|\| (aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH)) ){` |
|       7 | 1410 | `						rc = VmEnforceGenArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[k],bStrict,pSelfHint);` |
|       7 | 1411 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1412 | `							return rc;` |
|       - | 1413 | `						}` |
|       3 | 1414 | `					}` |
|       7 | 1415 | `					PH7_HashmapInsert(pMap,0,apArg[k]);` |
|       4 | 1416 | `				}` |
|       5 | 1417 | `				sSlot.nIdx = nVariadicIdx;` |
|       5 | 1418 | `				sSlot.pUserData = 0;` |
|       5 | 1419 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       2 | 1420 | `			}` |
|       5 | 1421 | `			break; /* All remaining actuals consumed */` |
|       - | 1422 | `		}` |
|      90 | 1423 | `		if( n < (sxu32)nArg ){` |
|       - | 1424 | `			/* Argument provided — install with declared-type enforcement.` |
|       - | 1425 | `			 * php binds and type-checks generator arguments EAGERLY at the` |
|       - | 1426 | `			 * g(...) call site (and fiber arguments at Fiber::start()), so the` |
|       - | 1427 | `			 * enforcement lives here, mirroring the OP_CALL install path via` |
|       - | 1428 | `			 * VmEnforceGenArgType (TypeError on mismatch, weak coercion in` |
|       - | 1429 | `			 * place otherwise) instead of the old silent xCast. A variadic` |
|       - | 1430 | `			 * formal collects as-is (no per-element declared-type model). */` |
|      84 | 1431 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      84 | 1432 | `			if( pObj ){` |
|      84 | 1433 | `				PH7_MemObjStore(apArg[n], pObj);` |
|      84 | 1434 | `				if( (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      84 | 1435 | `					rc = VmEnforceGenArgType(pVm,pFunc,&aFormalArg[n],n+1,pObj,bStrict,pSelfHint);` |
|      84 | 1436 | `					if( rc != SXRET_OK ){` |
|      15 | 1437 | `						return rc;` |
|       - | 1438 | `					}` |
|      33 | 1439 | `				}` |
|      70 | 1440 | `				sSlot.nIdx = pObj->nIdx;` |
|      70 | 1441 | `				sSlot.pUserData = 0;` |
|      70 | 1442 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      37 | 1443 | `			}` |
|      40 | 1444 | `		}else if( n < nReqGF ){` |
|       - | 1445 | `			/* Required formal with no actual: php's ArgumentCountError, at` |
|       - | 1446 | `			 * this point in the install order (see the watermark comment). */` |
|       7 | 1447 | `			return VmGenArgThrowStatus(pVm,` |
|       4 | 1448 | `				(pFunc->iFlags & VM_FUNC_INTERNAL)` |
|     ! 0 | 1449 | `					? VmThrowBuiltinTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|     ! 0 | 1450 | `						(sxu32)nArg,nReqGF,nNonVarGF)` |
|       6 | 1451 | `					: VmThrowTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|       2 | 1452 | `						(sxu32)nArg,nReqGF,nNonVarGF,bCallSiteInMsg));` |
|       3 | 1453 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       - | 1454 | `			/* Default value */` |
|       3 | 1455 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       3 | 1456 | `			if( pObj ){` |
|       3 | 1457 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);` |
|       3 | 1458 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1459 | `					return rc;` |
|       - | 1460 | `				}` |
|       - | 1461 | `` 				/* A null default on an implicitly-nullable `Type $x = null` `` |
|       - | 1462 | `				 * param must stay null (php); only non-null defaults keep the` |
|       - | 1463 | `				 * legacy shaping cast. */` |
|       2 | 1464 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       3 | 1465 | `				 && !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|     ! 0 | 1466 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|     ! 0 | 1467 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|     ! 0 | 1468 | `						if( xCast ){` |
|     ! 0 | 1469 | `							xCast(pObj);` |
|     ! 0 | 1470 | `						}` |
|     ! 0 | 1471 | `					}` |
|     ! 0 | 1472 | `				}` |
|       3 | 1473 | `				sSlot.nIdx = pObj->nIdx;` |
|       3 | 1474 | `				sSlot.pUserData = 0;` |
|       3 | 1475 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       1 | 1476 | `			}` |
|       1 | 1477 | `		}` |
|      38 | 1478 | `	}` |
|       - | 1479 | `	/* Install closure environment (captured variables) */` |
|     583 | 1480 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1481 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|       - | 1482 | `		ph7_value *pValue;` |
|       - | 1483 | `		sxu32 iEnv;` |
|      47 | 1484 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     101 | 1485 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|      59 | 1486 | `			pEnv = &aEnv[iEnv];` |
|      59 | 1487 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|      39 | 1488 | `				continue;` |
|       - | 1489 | `			}` |
|      20 | 1490 | `			if( pClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       4 | 1491 | `			 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|       - | 1492 | `				/* An explicit bound $this (bindTo/bind/call) wins over the` |
|       - | 1493 | `				 * creation-time captured $this, php-exact (mirrors OP_CALL). */` |
|       3 | 1494 | `				continue;` |
|       - | 1495 | `			}` |
|      20 | 1496 | `			if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|       - | 1497 | `				/* Captured by reference: link the name to the shared slot` |
|       - | 1498 | `				 * (no copy), mirroring the OP_CALL env install. */` |
|       5 | 1499 | `				if( SyHashGet(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|       7 | 1500 | `					SyHashInsert(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),` |
|       4 | 1501 | `						SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|       2 | 1502 | `				}` |
|       5 | 1503 | `				continue;` |
|       - | 1504 | `			}` |
|      16 | 1505 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|      16 | 1506 | `			if( pValue == 0 ){` |
|     ! 0 | 1507 | `				continue;` |
|       - | 1508 | `			}` |
|      16 | 1509 | `			PH7_MemObjRelease(pValue);` |
|      16 | 1510 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|       9 | 1511 | `		}` |
|      21 | 1512 | `	}` |
|     583 | 1513 | `	return SXRET_OK;` |
|     303 | 1514 | `}` |
|       - | 1515 | `/*` |
|       - | 1516 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|       - | 1517 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|       - | 1518 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|       - | 1519 | ` */` |
|     260 | 1520 | `PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1521 | `{` |
|     265 | 1522 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1523 | `	ph7_class_instance *pThis;` |
|       - | 1524 | `	ph7_class_instance *pClosureThis;` |
|       - | 1525 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 1526 | `	ph7_vm_func *pFunc;` |
|       - | 1527 | `	ph7_value sResult;` |
|       - | 1528 | `	ph7_value *pCtxAttr;` |
|       - | 1529 | `	SyString sAttrName;` |
|       - | 1530 | `	sxi32 rc;` |
|     265 | 1531 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1532 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|       - | 1533 | `	}` |
|     265 | 1534 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 1535 | `	/* Check if already started (has a __ctx) */` |
|     265 | 1536 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|     265 | 1537 | `	if( pExecCtx != 0 ){` |
|       3 | 1538 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1539 | `			"Cannot start a fiber that has already been started");` |
|       - | 1540 | `	}` |
|       - | 1541 | `	/* Resolve callable */` |
|     263 | 1542 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|     263 | 1543 | `	if( pFunc == 0 ){` |
|     ! 0 | 1544 | `		return PH7_EXCEPTION;` |
|       - | 1545 | `	}` |
|       - | 1546 | `	/* Create execution context now that we know the function */` |
|     263 | 1547 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|     263 | 1548 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 1549 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1550 | `			"Fiber::start(): out of memory");` |
|       - | 1551 | `	}` |
|       - | 1552 | `	/* Store context in $this->__ctx */` |
|     263 | 1553 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     263 | 1554 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     263 | 1555 | `	if( pCtxAttr ){` |
|     263 | 1556 | `		pCtxAttr->x.pOther = pExecCtx;` |
|     263 | 1557 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     129 | 1558 | `	}` |
|       - | 1559 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|       - | 1560 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|       - | 1561 | `	 * into the fiber's frame, not the caller's. */` |
|     263 | 1562 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|     263 | 1563 | `	pVm->pFrame = pExecCtx->pFrame;` |
|       - | 1564 | `	/* Unpack the args array and install into the frame */` |
|       - | 1565 | `	{` |
|     263 | 1566 | `		ph7_value **apValues = 0;` |
|     263 | 1567 | `		ph7_value *aStore = 0;` |
|     263 | 1568 | `		int nActual = 0;` |
|     263 | 1569 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|     263 | 1570 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 1571 | `			ph7_hashmap_node *pNode;` |
|     263 | 1572 | `			sxu32 nCount = pMap->nEntry;` |
|     263 | 1573 | `			if( nCount > 0 ){` |
|       8 | 1574 | `				sxu32 idx = 0;` |
|      11 | 1575 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       3 | 1576 | `					nCount * sizeof(ph7_value *));` |
|      11 | 1577 | `				aStore = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       3 | 1578 | `					nCount * sizeof(ph7_value));` |
|       8 | 1579 | `				if( apValues && aStore ){` |
|       8 | 1580 | `					pNode = pMap->pFirst;` |
|      16 | 1581 | `					while( pNode && idx < nCount ){` |
|       - | 1582 | `						/* Snapshot each source into stable storage: VmFiberSetupFrame reserves` |
|       - | 1583 | `						 * memory objects (VmExtractMemObj) before reading the args, which can` |
|       - | 1584 | `						 * reallocate (move) pVm->aMemObj and dangle a raw pool pointer. A` |
|       - | 1585 | `						 * shallow copy is a safe source — the referent and the heap-resident` |
|       - | 1586 | `						 * blob data survive the move (same sSafeVal idiom the hashmap inserters` |
|       - | 1587 | `						 * use); it owns nothing independently, so it needs no release. */` |
|      10 | 1588 | `						ph7_value *pSrc = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      10 | 1589 | `						if( pSrc ){` |
|      10 | 1590 | `							aStore[idx] = *pSrc;` |
|       6 | 1591 | `						}else{` |
|     ! 0 | 1592 | `							PH7_MemObjInit(pVm, &aStore[idx]);` |
|       - | 1593 | `						}` |
|      10 | 1594 | `						apValues[idx] = &aStore[idx];` |
|      10 | 1595 | `						idx++;` |
|      10 | 1596 | `						pNode = pNode->pPrev;` |
|       2 | 1597 | `					}` |
|       8 | 1598 | `					nActual = (int)idx;` |
|       3 | 1599 | `				}` |
|       3 | 1600 | `			}` |
|     129 | 1601 | `		}` |
|     263 | 1602 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues,` |
|       - | 1603 | `			0 /* weak-mode arg binding, like call_user_func */, 0,` |
|       - | 1604 | `			FALSE/*Fiber::start(): php omits the call-site segment*/);` |
|     263 | 1605 | `		if( aStore ){` |
|       8 | 1606 | `			SyMemBackendFree(&pVm->sAllocator, aStore);` |
|       3 | 1607 | `		}` |
|     263 | 1608 | `		if( apValues ){` |
|       8 | 1609 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|       3 | 1610 | `		}` |
|       - | 1611 | `	}` |
|       - | 1612 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|     263 | 1613 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|     263 | 1614 | `	pExecCtx->pFrame->pParent = 0;` |
|     263 | 1615 | `	if( rc != SXRET_OK ){` |
|       - | 1616 | `		/* Propagate the real status: a declared-type TypeError from the arg` |
|       - | 1617 | `		 * install (band A #2) must stay catchable, not become an abort. */` |
|       5 | 1618 | `		return (rc == PH7_EXCEPTION) ? PH7_EXCEPTION : PH7_ABORT;` |
|       - | 1619 | `	}` |
|     259 | 1620 | `	PH7_MemObjInit(pVm, &sResult);` |
|     259 | 1621 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|     259 | 1622 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 1623 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1624 | `		return PH7_ABORT;` |
|       - | 1625 | `	}` |
|     259 | 1626 | `	if( rc == PH7_EXCEPTION ){` |
|       6 | 1627 | `		PH7_MemObjRelease(&sResult);` |
|       6 | 1628 | `		return PH7_EXCEPTION;` |
|       - | 1629 | `	}` |
|     255 | 1630 | `	ph7_result_value(pCtx, &sResult);` |
|     255 | 1631 | `	PH7_MemObjRelease(&sResult);` |
|     255 | 1632 | `	return PH7_OK;` |
|     135 | 1633 | `}` |
|       - | 1634 | `/*` |
|       - | 1635 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|       - | 1636 | ` */` |
|     146 | 1637 | `PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1638 | `{` |
|     151 | 1639 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1640 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 1641 | `	ph7_value sResult;` |
|       - | 1642 | `	ph7_value *pResumeVal;` |
|       - | 1643 | `	sxi32 rc;` |
|     151 | 1644 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1645 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|     ! 0 | 1646 | `		return PH7_OK;` |
|       - | 1647 | `	}` |
|     151 | 1648 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|     151 | 1649 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 1650 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|     ! 0 | 1651 | `		return PH7_OK;` |
|       - | 1652 | `	}` |
|     151 | 1653 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       3 | 1654 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1655 | `			"Cannot resume a fiber that is not suspended");` |
|       - | 1656 | `	}` |
|     149 | 1657 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|     149 | 1658 | `	PH7_MemObjInit(pVm, &sResult);` |
|     149 | 1659 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|     149 | 1660 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 1661 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1662 | `		return PH7_ABORT;` |
|       - | 1663 | `	}` |
|     149 | 1664 | `	if( rc == PH7_EXCEPTION ){` |
|       3 | 1665 | `		PH7_MemObjRelease(&sResult);` |
|       3 | 1666 | `		return PH7_EXCEPTION;` |
|       - | 1667 | `	}` |
|     147 | 1668 | `	ph7_result_value(pCtx, &sResult);` |
|     147 | 1669 | `	PH7_MemObjRelease(&sResult);` |
|     147 | 1670 | `	return PH7_OK;` |
|      78 | 1671 | `}` |
|       - | 1672 | `/*` |
|       - | 1673 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|       - | 1674 | ` */` |
|      18 | 1675 | `PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1676 | `{` |
|      23 | 1677 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1678 | `	ph7_exec_ctx *pExecCtx;` |
|      23 | 1679 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1680 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1681 | `		return PH7_OK;` |
|       - | 1682 | `	}` |
|      23 | 1683 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|      23 | 1684 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 1685 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1686 | `		return PH7_OK;` |
|       - | 1687 | `	}` |
|      23 | 1688 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|     ! 0 | 1689 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 1690 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1691 | `				"Cannot get fiber return value: The fiber has not been started");` |
|       - | 1692 | `		}` |
|     ! 0 | 1693 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1694 | `			"Cannot get fiber return value: The fiber has not returned");` |
|       - | 1695 | `	}` |
|      23 | 1696 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|      23 | 1697 | `	return PH7_OK;` |
|      14 | 1698 | `}` |
|       - | 1699 | `/*` |
|       - | 1700 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|       - | 1701 | ` */` |
|       6 | 1702 | `PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 1703 | `{` |
|       - | 1704 | `	ph7_exec_ctx *pExecCtx;` |
|       7 | 1705 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       7 | 1706 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|       7 | 1707 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|       7 | 1708 | `	return PH7_OK;` |
|       4 | 1709 | `}` |
|     ! 0 | 1710 | `PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     ! 0 | 1711 | `{` |
|       - | 1712 | `	ph7_exec_ctx *pExecCtx;` |
|     ! 0 | 1713 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     ! 0 | 1714 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|     ! 0 | 1715 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|     ! 0 | 1716 | `	return PH7_OK;` |
|     ! 0 | 1717 | `}` |
|     108 | 1718 | `PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 1719 | `{` |
|       - | 1720 | `	ph7_exec_ctx *pExecCtx;` |
|     110 | 1721 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     110 | 1722 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|     110 | 1723 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|     110 | 1724 | `	return PH7_OK;` |
|      56 | 1725 | `}` |
|       6 | 1726 | `PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 1727 | `{` |
|       - | 1728 | `	ph7_exec_ctx *pExecCtx;` |
|       7 | 1729 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       7 | 1730 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|       7 | 1731 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|       7 | 1732 | `	return PH7_OK;` |
|       4 | 1733 | `}` |
|       - | 1734 | `/*` |
|       - | 1735 | ` * Fiber->__destruct() — clean up the execution context.` |
|       - | 1736 | ` */` |
|     216 | 1737 | `PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 1738 | `{` |
|     219 | 1739 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1740 | `	ph7_exec_ctx *pExecCtx;` |
|     219 | 1741 | `	if( nArg < 1 ){` |
|     ! 0 | 1742 | `		return PH7_OK;` |
|       - | 1743 | `	}` |
|     219 | 1744 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|     219 | 1745 | `	if( pExecCtx ){` |
|     219 | 1746 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|       - | 1747 | `		/* Clear the attribute so double-free is prevented */` |
|     219 | 1748 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     219 | 1749 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 1750 | `			SyString sAttrName;` |
|       - | 1751 | `			ph7_value *pAttr;` |
|     219 | 1752 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     219 | 1753 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     219 | 1754 | `			if( pAttr ){` |
|     219 | 1755 | `				PH7_MemObjRelease(pAttr);` |
|     108 | 1756 | `			}` |
|     108 | 1757 | `		}` |
|     108 | 1758 | `	}` |
|     219 | 1759 | `	return PH7_OK;` |
|     111 | 1760 | `}` |
|       - | 1761 | `/* ======================== Fiber Public API Helpers ======================== */` |
|     ! 0 | 1762 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|     ! 0 | 1763 | `{` |
|       - | 1764 | `	ph7_class_instance *pThis;` |
|     ! 0 | 1765 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|     ! 0 | 1766 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 | 1767 | `	return pThis->pClass == pVm->pFiberClass;` |
|     ! 0 | 1768 | `}` |
|     ! 0 | 1769 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 1770 | `{` |
|       - | 1771 | `	ph7_class_instance *pThis;` |
|     ! 0 | 1772 | `	ph7_class_instance *pClosureThis = 0;` |
|       - | 1773 | `	ph7_exec_ctx *pCtx;` |
|       - | 1774 | `	ph7_vm_func *pFunc;` |
|       - | 1775 | `	ph7_value *pCallable;` |
|       - | 1776 | `	ph7_value *pCtxAttr;` |
|       - | 1777 | `	SyString sAttrName;` |
|       - | 1778 | `	sxi32 rc;` |
|       - | 1779 | `	/* Must not already be started */` |
|     ! 0 | 1780 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 1781 | `	if( pCtx != 0 ){` |
|     ! 0 | 1782 | `		return SXERR_INVALID;` |
|       - | 1783 | `	}` |
|     ! 0 | 1784 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1785 | `		return SXERR_INVALID;` |
|       - | 1786 | `	}` |
|     ! 0 | 1787 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|       - | 1788 | `	/* Get the callable */` |
|     ! 0 | 1789 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     ! 0 | 1790 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 1791 | `	if( pCallable == 0 ){` |
|     ! 0 | 1792 | `		return SXERR_INVALID;` |
|       - | 1793 | `	}` |
|       - | 1794 | `	/* Resolve callable */` |
|     ! 0 | 1795 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|       - | 1796 | `		SyString sName;` |
|       - | 1797 | `		SyHashEntry *pEntry;` |
|     ! 0 | 1798 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|     ! 0 | 1799 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|     ! 0 | 1800 | `		if( pEntry == 0 ){` |
|     ! 0 | 1801 | `			return SXERR_NOTFOUND;` |
|       - | 1802 | `		}` |
|     ! 0 | 1803 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     ! 0 | 1804 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 1805 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|     ! 0 | 1806 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 1807 | `			sizeof("__invoke") - 1);` |
|     ! 0 | 1808 | `		if( pMethod == 0 ){` |
|     ! 0 | 1809 | `			return SXERR_INVALID;` |
|       - | 1810 | `		}` |
|     ! 0 | 1811 | `		pClosureThis = pClosure;` |
|     ! 0 | 1812 | `		pFunc = &pMethod->sFunc;` |
|     ! 0 | 1813 | `	}else{` |
|     ! 0 | 1814 | `		return SXERR_INVALID;` |
|       - | 1815 | `	}` |
|       - | 1816 | `	/* Create context */` |
|     ! 0 | 1817 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|     ! 0 | 1818 | `	if( pCtx == 0 ){` |
|     ! 0 | 1819 | `		return SXERR_MEM;` |
|       - | 1820 | `	}` |
|       - | 1821 | `	/* Store in __ctx */` |
|     ! 0 | 1822 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     ! 0 | 1823 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 1824 | `	if( pCtxAttr ){` |
|     ! 0 | 1825 | `		pCtxAttr->x.pOther = pCtx;` |
|     ! 0 | 1826 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     ! 0 | 1827 | `	}` |
|       - | 1828 | `	/* Set up frame with args */` |
|     ! 0 | 1829 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     ! 0 | 1830 | `	pVm->pFrame = pCtx->pFrame;` |
|     ! 0 | 1831 | `	rc = VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg,` |
|       - | 1832 | `		0 /* weak-mode arg binding (embedder entry) */, 0,` |
|       - | 1833 | `		FALSE/*embedder entry: no userland call site*/);` |
|     ! 0 | 1834 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|     ! 0 | 1835 | `	pCtx->pFrame->pParent = 0;` |
|     ! 0 | 1836 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1837 | `		return rc;` |
|       - | 1838 | `	}` |
|     ! 0 | 1839 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|     ! 0 | 1840 | `}` |
|     ! 0 | 1841 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 1842 | `{` |
|     ! 0 | 1843 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 1844 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|     ! 0 | 1845 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|     ! 0 | 1846 | `}` |
|     ! 0 | 1847 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 1848 | `{` |
|     ! 0 | 1849 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 1850 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|     ! 0 | 1851 | `}` |
|     ! 0 | 1852 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 1853 | `{` |
|     ! 0 | 1854 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 1855 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|     ! 0 | 1856 | `}` |
|     ! 0 | 1857 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 1858 | `{` |
|     ! 0 | 1859 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 1860 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|     ! 0 | 1861 | `	return &pCtx->sRetValue;` |
|     ! 0 | 1862 | `}` |
|       - | 1863 | `/* ======================== Generator Infrastructure ======================== */` |
|       - | 1864 | `/*` |
|       - | 1865 | ` * Allocate a new generator wrapper around an execution context.` |
|       - | 1866 | ` */` |
|     338 | 1867 | `PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 | 1868 | `{` |
|       - | 1869 | `	ph7_generator *pGen;` |
|     343 | 1870 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|     343 | 1871 | `	if( pGen == 0 ){` |
|     ! 0 | 1872 | `		return 0;` |
|       - | 1873 | `	}` |
|     343 | 1874 | `	SyZero(pGen, sizeof(ph7_generator));` |
|     343 | 1875 | `	pGen->pCtx = pCtx;` |
|     343 | 1876 | `	pGen->iImplicitKey = 0;` |
|     343 | 1877 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|     343 | 1878 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|       - | 1879 | `	/* Link the generator back to the exec context */` |
|     343 | 1880 | `	pCtx->pPrivate = pGen;` |
|     343 | 1881 | `	return pGen;` |
|     174 | 1882 | `}` |
|       - | 1883 | `/*` |
|       - | 1884 | ` * Release a generator and its execution context.` |
|       - | 1885 | ` */` |
|     230 | 1886 | `PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|       5 | 1887 | `{` |
|     235 | 1888 | `	if( pGen == 0 ){` |
|     ! 0 | 1889 | `		return;` |
|       - | 1890 | `	}` |
|     235 | 1891 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|     235 | 1892 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|     235 | 1893 | `	if( pGen->pCtx ){` |
|     235 | 1894 | `		pGen->pCtx->pPrivate = 0;` |
|     235 | 1895 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|     235 | 1896 | `		pGen->pCtx = 0;` |
|     115 | 1897 | `	}` |
|     235 | 1898 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|     120 | 1899 | `}` |
|       - | 1900 | `/*` |
|       - | 1901 | ` * Extract ph7_generator from a Generator class instance.` |
|       - | 1902 | ` */` |
|    4232 | 1903 | `PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|       5 | 1904 | `{` |
|       - | 1905 | `	ph7_class_instance *pThis;` |
|       - | 1906 | `	SyString sAttr;` |
|       - | 1907 | `	ph7_value *pAttr;` |
|    4237 | 1908 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1909 | `		return 0;` |
|       - | 1910 | `	}` |
|    4237 | 1911 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|    4237 | 1912 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|     ! 0 | 1913 | `		return 0;` |
|       - | 1914 | `	}` |
|    4237 | 1915 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    4237 | 1916 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    4237 | 1917 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     ! 0 | 1918 | `		return 0;` |
|       - | 1919 | `	}` |
|    4237 | 1920 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    2121 | 1921 | `}` |
|       - | 1922 | `/*` |
|       - | 1923 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|       - | 1924 | ` */` |
|     204 | 1925 | `PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1926 | `{` |
|       - | 1927 | `	ph7_generator *pGen;` |
|       - | 1928 | `	sxi32 rc;` |
|     209 | 1929 | `	if( nArg < 1 ) return PH7_OK;` |
|     209 | 1930 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|     209 | 1931 | `	if( pGen == 0 ) return PH7_OK;` |
|     209 | 1932 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     209 | 1933 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     209 | 1934 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     209 | 1935 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      99 | 1936 | `	}` |
|     203 | 1937 | `	return PH7_OK;` |
|     107 | 1938 | `}` |
|       - | 1939 | `/*` |
|       - | 1940 | ` * Generator::valid() — true if suspended at a yield point.` |
|       - | 1941 | ` */` |
|    1180 | 1942 | `PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1943 | `{` |
|       - | 1944 | `	ph7_generator *pGen;` |
|    1185 | 1945 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|    1185 | 1946 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|    1185 | 1947 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|    1185 | 1948 | `	return PH7_OK;` |
|     595 | 1949 | `}` |
|       - | 1950 | `/*` |
|       - | 1951 | ` * Generator::current() — return the last yielded value.` |
|       - | 1952 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 1953 | ` */` |
|    1170 | 1954 | `PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1955 | `{` |
|       - | 1956 | `	ph7_generator *pGen;` |
|       - | 1957 | `	sxi32 rc;` |
|    1175 | 1958 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1175 | 1959 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|    1175 | 1960 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1175 | 1961 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     115 | 1962 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     115 | 1963 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     115 | 1964 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      55 | 1965 | `	}` |
|    1175 | 1966 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|    1173 | 1967 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|     589 | 1968 | `	}else{` |
|       3 | 1969 | `		ph7_result_null(pCtx);` |
|       - | 1970 | `	}` |
|    1175 | 1971 | `	return PH7_OK;` |
|     590 | 1972 | `}` |
|       - | 1973 | `/*` |
|       - | 1974 | ` * Generator::key() — return the last yielded key.` |
|       - | 1975 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 1976 | ` */` |
|     222 | 1977 | `PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1978 | `{` |
|       - | 1979 | `	ph7_generator *pGen;` |
|       - | 1980 | `	sxi32 rc;` |
|     227 | 1981 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     227 | 1982 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|     227 | 1983 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     227 | 1984 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 1985 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     ! 0 | 1986 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     ! 0 | 1987 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     ! 0 | 1988 | `	}` |
|     227 | 1989 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     227 | 1990 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|     116 | 1991 | `	}else{` |
|     ! 0 | 1992 | `		ph7_result_null(pCtx);` |
|       - | 1993 | `	}` |
|     227 | 1994 | `	return PH7_OK;` |
|     116 | 1995 | `}` |
|       - | 1996 | `/*` |
|       - | 1997 | ` * Generator::next() — advance to the next yield point.` |
|       - | 1998 | ` */` |
|     972 | 1999 | `PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2000 | `{` |
|       - | 2001 | `	ph7_generator *pGen;` |
|       - | 2002 | `	sxi32 rc;` |
|     977 | 2003 | `	if( nArg < 1 ) return PH7_OK;` |
|     977 | 2004 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|     977 | 2005 | `	if( pGen == 0 ) return PH7_OK;` |
|     977 | 2006 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2007 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     977 | 2008 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     977 | 2009 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|     491 | 2010 | `	}else{` |
|     ! 0 | 2011 | `		return PH7_OK;` |
|       - | 2012 | `	}` |
|     977 | 2013 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     975 | 2014 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     965 | 2015 | `	return PH7_OK;` |
|     491 | 2016 | `}` |
|       - | 2017 | `/*` |
|       - | 2018 | ` * Generator::send($value) — resume and send a value into the generator.` |
|       - | 2019 | ` */` |
|     100 | 2020 | `PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 2021 | `{` |
|       - | 2022 | `	ph7_generator *pGen;` |
|       - | 2023 | `	ph7_value *pSendVal;` |
|       - | 2024 | `	sxi32 rc;` |
|     104 | 2025 | `	if( nArg < 1 ) return PH7_OK;` |
|     104 | 2026 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|     104 | 2027 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     104 | 2028 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|     104 | 2029 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       - | 2030 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|     ! 0 | 2031 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     104 | 2032 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     104 | 2033 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|      54 | 2034 | `	}else{` |
|     ! 0 | 2035 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2036 | `		return PH7_OK;` |
|       - | 2037 | `	}` |
|     104 | 2038 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     104 | 2039 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     101 | 2040 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      94 | 2041 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      48 | 2042 | `	}else{` |
|       8 | 2043 | `		ph7_result_null(pCtx);` |
|       - | 2044 | `	}` |
|     101 | 2045 | `	return PH7_OK;` |
|      54 | 2046 | `}` |
|       - | 2047 | `/*` |
|       - | 2048 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|       - | 2049 | ` *` |
|       - | 2050 | ` * PHP semantics: the exception is injected AT the suspended yield point so the` |
|       - | 2051 | ` * generator's OWN try/catch (if any wraps the yield) can handle it and the body` |
|       - | 2052 | ` * resumes after the try; otherwise it propagates to the throw() caller and the` |
|       - | 2053 | ` * generator closes. We implement this by resuming the body with a pending` |
|       - | 2054 | ` * injection (pCtx->pInjected) that the VM loop raises in the body's own frame via` |
|       - | 2055 | ` * the existing OP_THROW / ROOT B resume route — no exception frame is` |
|       - | 2056 | ` * reconstructed on pVm->pFrame, so the return/finally unwind path is untouched.` |
|       - | 2057 | ` * A never-started generator is first run to its first yield, then injected there;` |
|       - | 2058 | ` * a finished/closed generator has no suspend point, so the exception is simply` |
|       - | 2059 | ` * propagated to the caller (its return value stays readable via getReturn()).` |
|       - | 2060 | ` *` |
|       - | 2061 | ` * PHL does not enforce the Throwable parameter hint (interface/class hints are` |
|       - | 2062 | ` * not checked on call), so the Throwable validation — and its PHP TypeError —` |
|       - | 2063 | ` * are done here.` |
|       - | 2064 | ` */` |
|      62 | 2065 | `PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 2066 | `{` |
|       - | 2067 | `	ph7_generator *pGen;` |
|       - | 2068 | `	ph7_class_instance *pInj;` |
|       - | 2069 | `	ph7_class *pThrowable;` |
|       - | 2070 | `	VmFrame *pFrame;` |
|       - | 2071 | `	sxi32 rc;` |
|      66 | 2072 | `	if( nArg < 2 ) return PH7_OK;` |
|       - | 2073 | `	/* Argument #1 must be a Throwable; otherwise PHP raises a TypeError naming` |
|       - | 2074 | `	 * the given type (class name for objects, "null"/"string"/... for scalars). */` |
|      66 | 2075 | `	pThrowable = PH7_VmExtractClass(pCtx->pVm, "Throwable", sizeof("Throwable")-1, 0, 0);` |
|      62 | 2076 | `	if( (apArg[1]->iFlags & MEMOBJ_OBJ) == 0` |
|      66 | 2077 | `	 \|\| (pThrowable && !PH7_VmInstanceOf(((ph7_class_instance *)apArg[1]->x.pOther)->pClass, pThrowable)) ){` |
|       - | 2078 | `		char zCls[128];` |
|     ! 0 | 2079 | `		const char *zGiven = (apArg[1]->iFlags & MEMOBJ_OBJ)` |
|     ! 0 | 2080 | `			? VmFormatValueClassName(apArg[1], zCls, sizeof(zCls))` |
|     ! 0 | 2081 | `			: ((apArg[1]->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(apArg[1]));` |
|     ! 0 | 2082 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|     ! 0 | 2083 | `			"Generator::throw(): Argument #1 ($exception) must be of type Throwable, %s given", zGiven);` |
|       - | 2084 | `	}` |
|      66 | 2085 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      66 | 2086 | `	if( pGen == 0 ) return PH7_OK;` |
|       - | 2087 | `	/* PHP forbids resuming/throwing into a generator that is currently executing. */` |
|      66 | 2088 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       3 | 2089 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 2090 | `			"Cannot resume an already running generator");` |
|       - | 2091 | `	}` |
|       - | 2092 | `	/* Hold a reference to the injected instance for the whole operation: the VM loop` |
|       - | 2093 | `	 * (inject path) or VmThrowException (propagate path) may run catch blocks that bind` |
|       - | 2094 | `	 * and later release it. Dropped on every return path below. */` |
|      64 | 2095 | `	pInj = (ph7_class_instance *)apArg[1]->x.pOther;` |
|      64 | 2096 | `	pInj->iRef++;` |
|       - | 2097 | `	/* A never-started generator runs to its first yield, then the exception is injected` |
|       - | 2098 | `	 * there (PHP). Start it first; if it suspended at a yield, fall through to inject; if` |
|       - | 2099 | `	 * it ran to completion without yielding, drop through to the propagate path below. */` |
|      64 | 2100 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       5 | 2101 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       5 | 2102 | `		if( rc == PH7_ABORT ){ PH7_ClassInstanceUnref(pInj); return PH7_ABORT; }` |
|       5 | 2103 | `		if( rc == PH7_EXCEPTION ){ PH7_ClassInstanceUnref(pInj); return PH7_EXCEPTION; }` |
|       2 | 2104 | `	}` |
|      64 | 2105 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       - | 2106 | `		/* Inject at the suspended yield: the resume loop raises it in the body's own` |
|       - | 2107 | `		 * frame so the generator's try/catch can catch it and resume (path 2). */` |
|      58 | 2108 | `		pGen->pCtx->pInjected = pInj;   /* borrowed; ref held here across the resume */` |
|      58 | 2109 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       - | 2110 | `		/* Normally the inject was consumed (cleared) at VmByteCodeExec entry; clear it` |
|       - | 2111 | `		 * here too for the path where VmResumeCtx bails BEFORE entering the loop (e.g. the` |
|       - | 2112 | `		 * recursion-depth fatal), so no dangling borrowed pointer survives the Unref. */` |
|      58 | 2113 | `		pGen->pCtx->pInjected = 0;` |
|      58 | 2114 | `		PH7_ClassInstanceUnref(pInj);` |
|      58 | 2115 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      58 | 2116 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       - | 2117 | `		/* Caught inside the generator and it resumed: return the next yielded value (or` |
|       - | 2118 | `		 * null if it then completed) — symmetric with Generator::send(). */` |
|      46 | 2119 | `		if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      43 | 2120 | `			ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      23 | 2121 | `		}else{` |
|       3 | 2122 | `			ph7_result_null(pCtx);` |
|       - | 2123 | `		}` |
|      46 | 2124 | `		return PH7_OK;` |
|       - | 2125 | `	}` |
|       - | 2126 | `	/* COMPLETED/CLOSED (incl. a CREATED generator that ran to completion without a` |
|       - | 2127 | `	 * yield): no suspend point to inject at. Propagate the real object to the throw()` |
|       - | 2128 | `	 * caller through the normal dispatch path (class/message/trace preserved); its` |
|       - | 2129 | `	 * terminal state — and thus getReturn() — is left intact. */` |
|       8 | 2130 | `	pFrame = pCtx->pVm->pFrame;` |
|       8 | 2131 | `	if( pFrame ){` |
|       8 | 2132 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       8 | 2133 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       3 | 2134 | `	}` |
|       8 | 2135 | `	rc = VmThrowException(pCtx->pVm, pInj);` |
|       8 | 2136 | `	PH7_ClassInstanceUnref(pInj);` |
|       8 | 2137 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2138 | `		return PH7_ABORT;` |
|       - | 2139 | `	}` |
|       8 | 2140 | `	return PH7_EXCEPTION;` |
|      35 | 2141 | `}` |
|       - | 2142 | `/*` |
|       - | 2143 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|       - | 2144 | ` */` |
|      18 | 2145 | `PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 2146 | `{` |
|       - | 2147 | `	ph7_generator *pGen;` |
|      21 | 2148 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      21 | 2149 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      21 | 2150 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      21 | 2151 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|     ! 0 | 2152 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 2153 | `			"Cannot get return value of a generator that hasn't returned");` |
|       - | 2154 | `	}` |
|      21 | 2155 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|      21 | 2156 | `	return PH7_OK;` |
|      12 | 2157 | `}` |
|       - | 2158 | `/*` |
|       - | 2159 | ` * Generator::__destruct() — clean up.` |
|       - | 2160 | ` */` |
|     216 | 2161 | `PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2162 | `{` |
|       - | 2163 | `	ph7_generator *pGen;` |
|     221 | 2164 | `	sxi32 rcClose = SXRET_OK;` |
|     221 | 2165 | `	if( nArg < 1 ) return PH7_OK;` |
|     221 | 2166 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|     221 | 2167 | `	if( pGen ){` |
|       - | 2168 | `` 		/* A generator abandoned before it completes still runs its pending `finally` `` |
|       - | 2169 | `		 * blocks (PHP runs them at generator close/GC). Drive them before teardown. */` |
|     221 | 2170 | `		if( pGen->pCtx ){` |
|     221 | 2171 | `			rcClose = VmCloseCtx(pCtx->pVm, pGen->pCtx);` |
|     108 | 2172 | `		}` |
|     221 | 2173 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|     221 | 2174 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     221 | 2175 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 2176 | `			SyString sAttrName;` |
|       - | 2177 | `			ph7_value *pAttr;` |
|     221 | 2178 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     221 | 2179 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     221 | 2180 | `			if( pAttr ){` |
|     221 | 2181 | `				PH7_MemObjRelease(pAttr);` |
|     108 | 2182 | `			}` |
|     108 | 2183 | `		}` |
|     108 | 2184 | `	}` |
|       - | 2185 | `	/* Surface an abort/exception raised by a finally that ran during close. */` |
|     221 | 2186 | `	if( rcClose == PH7_ABORT ) return PH7_ABORT;` |
|     221 | 2187 | `	if( rcClose == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     221 | 2188 | `	return PH7_OK;` |
|     113 | 2189 | `}` |
|       - | 2190 | `/* ======================== End Generator Infrastructure ======================== */` |
|       - | 2191 | `/* ======================== End Fiber Infrastructure ======================== */` |
|       - | 2192 |  |
