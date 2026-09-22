# src/ph7/vm_exec_ctx.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1100/1343 lines (81.91%)

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
|     642 |   20 | `PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|       5 |   21 | `{` |
|       - |   22 | `	ph7_exec_ctx *pCtx;` |
|       - |   23 | `	ph7_value *pStack;` |
|       - |   24 | `	VmFrame *pFrame;` |
|     647 |   25 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|     647 |   26 | `	if( pCtx == 0 ){` |
|     ! 0 |   27 | `		return 0;` |
|       - |   28 | `	}` |
|     647 |   29 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|     647 |   30 | `	pCtx->pVm = pVm;` |
|     647 |   31 | `	pCtx->pFunc = pFunc;` |
|     647 |   32 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|     647 |   33 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|     647 |   34 | `	pCtx->pc = 0;` |
|     647 |   35 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|     647 |   36 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|     647 |   37 | `	PH7_MemObjInit(pVm, &pCtx->sDelegate);` |
|       - |   38 | `	/* Container for this body's own exception handlers while suspended (borrowed` |
|       - |   39 | `	 * ph7_exception* pointers — never freed here, owned by the compiled func). */` |
|     647 |   40 | `	SySetInit(&pCtx->aSavedException, &pVm->sAllocator, sizeof(ph7_exception *));` |
|       - |   41 | `	/* ROOT C: this body's own pending finally actions while suspended. */` |
|     647 |   42 | `	SySetInit(&pCtx->aSavedFinally, &pVm->sAllocator, sizeof(VmFinallyAction));` |
|     647 |   43 | `	pCtx->nFinallyBase = 0;` |
|       - |   44 | `	/* Stage 4: this coroutine's own aSelf entries (self::/static:: class context` |
|       - |   45 | `	 * pushed by nested method calls still open at suspend) parked while suspended,` |
|       - |   46 | `	 * so they don't pollute the resumer's aSelf. Borrowed ph7_class* pointers. */` |
|     647 |   47 | `	SySetInit(&pCtx->aSavedSelf, &pVm->sAllocator, sizeof(ph7_class *));` |
|     647 |   48 | `	pCtx->nSelfBase = 0;` |
|     647 |   49 | `	pCtx->pParkedSegment = 0;` |
|     647 |   50 | `	pCtx->nBodyExecDepth = 0;` |
|       - |   51 | `	/* Allocate a private operand stack */` |
|     647 |   52 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|     647 |   53 | `	if( pStack == 0 ){` |
|     ! 0 |   54 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   55 | `		return 0;` |
|       - |   56 | `	}` |
|     647 |   57 | `	pCtx->pStack = pStack;` |
|     647 |   58 | `	pCtx->nStackCap = SySetUsed(&pFunc->aByteCode) + VM_STACK_GUARD; /* grows with pStack on OP_SPREAD */` |
|     647 |   59 | `	pCtx->nStackOrig = pCtx->nStackCap; /* fixed original — headroom reference across resumes */` |
|       - |   60 | `	/* Create a detached frame for the fiber */` |
|     647 |   61 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|     647 |   62 | `	if( pFrame == 0 ){` |
|     ! 0 |   63 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|     ! 0 |   64 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|     647 |   67 | `	pCtx->pFrame = pFrame;` |
|     647 |   68 | `	return pCtx;` |
|     326 |   69 | `}` |
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
|    5076 |   93 | `static void VmParkStackSlice(SySet *pFrom, SySet *pSaved, sxu32 nBase)` |
|       5 |   94 | `{` |
|    5081 |   95 | `	sxu32 nUsed = SySetUsed(pFrom);` |
|    5081 |   96 | `	if( nUsed > nBase ){` |
|     231 |   97 | `		const char *aBase = (const char *)SySetBasePtr(pFrom);` |
|       - |   98 | `		sxu32 i;` |
|     463 |   99 | `		for( i = nBase; i < nUsed; i++ ){` |
|     237 |  100 | `			SySetPut(pSaved, (const void *)(aBase + i * pFrom->eSize));` |
|     121 |  101 | `		}` |
|     231 |  102 | `		SySetTruncate(pFrom, nBase);` |
|     113 |  103 | `	}` |
|    5081 |  104 | `}` |
|    4248 |  105 | `static void VmRestoreStackSlice(SySet *pTo, SySet *pSaved)` |
|       5 |  106 | `{` |
|    4253 |  107 | `	sxu32 i, n = SySetUsed(pSaved);` |
|    4253 |  108 | `	if( n > 0 ){` |
|     219 |  109 | `		const char *aSaved = (const char *)SySetBasePtr(pSaved);` |
|     439 |  110 | `		for( i = 0; i < n; i++ ){` |
|     225 |  111 | `			SySetPut(pTo, (const void *)(aSaved + i * pSaved->eSize));` |
|     115 |  112 | `		}` |
|     219 |  113 | `		SySetReset(pSaved);` |
|     107 |  114 | `	}` |
|    4253 |  115 | `}` |
|    1692 |  116 | `static void VmParkCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  117 | `{` |
|    1697 |  118 | `	VmParkStackSlice(&pVm->aException, &pCtx->aSavedException, pCtx->nExceptionBase);` |
|    1697 |  119 | `	VmParkStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally, pCtx->nFinallyBase);` |
|    1697 |  120 | `	VmParkStackSlice(&pVm->aSelf, &pCtx->aSavedSelf, pCtx->nSelfBase);` |
|    1697 |  121 | `}` |
|    1416 |  122 | `static void VmRestoreCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  123 | `{` |
|    1421 |  124 | `	VmRestoreStackSlice(&pVm->aException, &pCtx->aSavedException);` |
|    1421 |  125 | `	VmRestoreStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally);` |
|    1421 |  126 | `	VmRestoreStackSlice(&pVm->aSelf, &pCtx->aSavedSelf);` |
|    1421 |  127 | `}` |
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
|    1728 |  141 | `static void VmFreeSuspendedExceptionFrames(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  142 | `{` |
|    1945 |  143 | `	while( pVm->pFrame != pCtx->pFrame && (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     217 |  144 | `		VmLeaveFrame(&(*pVm));` |
|       5 |  145 | `	}` |
|    1733 |  146 | `}` |
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
|    1692 |  159 | `static void VmSuspendCtxDetach(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  160 | `{` |
|    1697 |  161 | `	if( pCtx->pParkedSegment == 0 ){` |
|    1395 |  162 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     700 |  163 | `	}else{` |
|       - |  164 | `		/* The parked records' push-time accounting (one nRecursionDepth++ each,` |
|       - |  165 | `		 * plus VmCallFinish's aSelf pop, which never ran) leaves the segment` |
|       - |  166 | `		 * counted as active while the fiber is suspended — deactivate it. aSelf` |
|       - |  167 | `		 * is parked wholesale below (base-relative), so drop only the depth. */` |
|     306 |  168 | `		pVm->nRecursionDepth -= ((VmParkedSegment *)pCtx->pParkedSegment)->nRecords;` |
|       - |  169 | `	}` |
|    1697 |  170 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|    1697 |  171 | `	pCtx->pFrame->pParent = 0;` |
|    1697 |  172 | `	VmParkCtxState(pVm, pCtx);` |
|    1697 |  173 | `	if( pResult ){` |
|     361 |  174 | `		PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|     178 |  175 | `	}` |
|    1697 |  176 | `}` |
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
|    2030 |  187 | `static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx)` |
|       5 |  188 | `{` |
|    1220 |  189 | `	return ((pCtx->pFunc->iFlags & VM_FUNC_GENERATOR) == 0 && VmFuncHasReturnType(pCtx->pFunc))` |
|    1215 |  190 | `		? pCtx->pFunc : 0;` |
|       5 |  191 | `}` |
|       - |  192 | `/*` |
|       - |  193 | ` * Common epilogue for VmStartCtx / VmResumeCtx once the body run returns rc:` |
|       - |  194 | ` * restore the previous active context, then park on suspend or detach the` |
|       - |  195 | ` * coroutine frame and record the terminal state. On normal completion the value` |
|       - |  196 | ` * belongs to getReturn() only — start()/resume() return the NEXT suspend value,` |
|       - |  197 | ` * which is null at completion (php parity), so pResult is left at its` |
|       - |  198 | ` * caller-initialized null.` |
|       - |  199 | ` */` |
|    2030 |  200 | `static sxi32 VmFinishCtxRun(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_exec_ctx *pOldCtx,` |
|       - |  201 | `	sxi32 rc, ph7_value *pResult)` |
|       5 |  202 | `{` |
|    2035 |  203 | `	pVm->pActiveCtx = pOldCtx;` |
|    2035 |  204 | `	if( rc == PH7_SUSPEND ){` |
|       - |  205 | `		/* Handles a deep RE-suspend too (a resumed segment parking a fresh one):` |
|       - |  206 | `		 * VmSuspendCtxDetach deactivates the new segment's recursion accounting,` |
|       - |  207 | `		 * parks its aSelf, and skips VmFreeSuspendedExceptionFrames for a segment` |
|       - |  208 | `		 * so it can't free the still-live parked try wrappers. */` |
|    1697 |  209 | `		VmSuspendCtxDetach(pVm, pCtx, pResult);` |
|    1697 |  210 | `		return SXRET_OK;` |
|       - |  211 | `	}` |
|       - |  212 | ``	/* A finally entered via the throw redirect whose `return` short-circuited`` |
|       - |  213 | `	 * OP_END_FINALLY leaves the try's transparent wrapper ABOVE the body frame —` |
|       - |  214 | `	 * the detach below would then be skipped and the wrapper (plus the body` |
|       - |  215 | `	 * frame) leak into the RESUMER's frame chain, so the next try at that scope` |
|       - |  216 | `	 * records the wrong owner frame and its caught throw silently unwinds the` |
|       - |  217 | `	 * script. Free trailing exception wrappers exactly like the suspend path. */` |
|     343 |  218 | `	if( pCtx->pParkedSegment == 0 ){` |
|     343 |  219 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     169 |  220 | `	}` |
|       - |  221 | `	/* Detach the coroutine frame from the live chain, unless a deeper unwind` |
|       - |  222 | `	 * already moved pVm->pFrame off it. */` |
|     343 |  223 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|     343 |  224 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|     343 |  225 | `		pCtx->pFrame->pParent = 0;` |
|     169 |  226 | `	}` |
|     343 |  227 | `	if( rc == PH7_ABORT ){` |
|       3 |  228 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       3 |  229 | `		return PH7_ABORT;` |
|       - |  230 | `	}` |
|     341 |  231 | `	if( rc == PH7_EXCEPTION ){` |
|      46 |  232 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      46 |  233 | `		return PH7_EXCEPTION;` |
|       - |  234 | `	}` |
|     299 |  235 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|     299 |  236 | `	return SXRET_OK;` |
|    1020 |  237 | `}` |
|       - |  238 | `/*` |
|       - |  239 | ` * Start executing a fiber context for the first time.` |
|       - |  240 | ` */` |
|     614 |  241 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  242 | `{` |
|       - |  243 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  244 | `	sxi32 rc;` |
|     619 |  245 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|     ! 0 |  246 | `		return SXERR_INVALID;` |
|       - |  247 | `	}` |
|       - |  248 | `	/* A fiber/generator start is a native VmByteCodeExec re-entry, bounded by` |
|       - |  249 | `	 * nMaxNativeDepth. Reject HERE, before attaching the frame / mutating VM` |
|       - |  250 | `	 * state, so the abort is clean — the wrapper's own check fires only after` |
|       - |  251 | `	 * this function has spliced the coroutine into the frame chain, which its` |
|       - |  252 | `	 * post-exec detach cannot fully unwind. The PHP call-depth cap belongs to` |
|       - |  253 | `	 * OP_CALL only (BYTECODE.md stage 5). */` |
|     619 |  254 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  255 | `		return VmNativeNestingFatal(pVm);` |
|       - |  256 | `	}` |
|       - |  257 | `	/* Attach the fiber's frame to the VM frame chain */` |
|     619 |  258 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     619 |  259 | `	pVm->pFrame = pCtx->pFrame;` |
|       - |  260 | `	/* Save and set the active context */` |
|     619 |  261 | `	pOldCtx = pVm->pActiveCtx;` |
|     619 |  262 | `	pVm->pActiveCtx = pCtx;` |
|     619 |  263 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|     619 |  264 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|     619 |  265 | `	pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|     619 |  266 | `	pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|       - |  267 | `	/* Native depth the body runs at (the VmByteCodeExec wrapper bumps +1): a` |
|       - |  268 | `	 * Fiber::suspend() at a deeper depth is inside a C->PHP callback and gets a` |
|       - |  269 | `	 * FiberError instead of parking across the native frame (stage 4). */` |
|     619 |  270 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1;` |
|       - |  271 | `	/* Execute from the beginning (no parked segment). An OP_SPREAD in the body may` |
|       - |  272 | `	 * realloc pCtx->pStack — pass &pCtx->pStack / &pCtx->nStackCap so the grown` |
|       - |  273 | `	 * buffer + capacity persist for the next resume and for ctx teardown. */` |
|     926 |  274 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     307 |  275 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|     307 |  276 | `		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|     619 |  277 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     312 |  278 | `}` |
|       - |  279 | `/*` |
|       - |  280 | ` * Resume a suspended fiber context.` |
|       - |  281 | ` */` |
|    1416 |  282 | `PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|       5 |  283 | `{` |
|       - |  284 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  285 | `	VmParkedSegment *pSeg;` |
|       - |  286 | `	sxi32 rc;` |
|    1421 |  287 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|     ! 0 |  288 | `		return SXERR_INVALID;` |
|       - |  289 | `	}` |
|       - |  290 | `	/* A resume is a native VmByteCodeExec re-entry, bounded by nMaxNativeDepth.` |
|       - |  291 | `	 * Reject HERE, before re-attaching the (possibly deep) parked segment to the` |
|       - |  292 | `	 * frame chain and re-adding its nRecords to nRecursionDepth — a wrapper-level` |
|       - |  293 | `	 * abort past those mutations would leave pVm->pFrame pointing into the parked` |
|       - |  294 | `	 * callee and the depth accounting un-reverted. The PHP call-depth cap is` |
|       - |  295 | `	 * OP_CALL-only (BYTECODE.md stage 5). */` |
|    1421 |  296 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  297 | `		return VmNativeNestingFatal(pVm);` |
|       - |  298 | `	}` |
|       - |  299 | `	/* Push the resume value onto the SUSPENDED activation's operand stack so it` |
|       - |  300 | `	 * appears as Fiber::suspend()'s return value. For a deep suspend (stage 4)` |
|       - |  301 | `	 * that stack is the innermost callee's — parked in the segment — not the` |
|       - |  302 | `	 * body's. nTos was saved one below the return-value slot. */` |
|       - |  303 | `	{` |
|       - |  304 | `		ph7_value *pResumeStack;` |
|    1421 |  305 | `		pSeg = (VmParkedSegment *)pCtx->pParkedSegment;` |
|    1421 |  306 | `		pResumeStack = pSeg ? pSeg->sState.pStack : pCtx->pStack;` |
|    1421 |  307 | `		if( pResumeValue ){` |
|     265 |  308 | `			PH7_MemObjStore(pResumeValue, &pResumeStack[pCtx->nTos + 1]);` |
|     135 |  309 | `		}else{` |
|    1161 |  310 | `			PH7_MemObjRelease(&pResumeStack[pCtx->nTos + 1]);` |
|       - |  311 | `		}` |
|    1421 |  312 | `		pCtx->nTos++;` |
|       - |  313 | `		/* Refresh the caller-depth base and re-publish this body's own exception` |
|       - |  314 | `		 * handlers on top of pVm->aException at that depth, so the resumed body's` |
|       - |  315 | `		 * try/catch and finally-drain bound line up (see VmByteCodeExec's base` |
|       - |  316 | `		 * override). Must run before VmByteCodeExec recaptures its local base. */` |
|    1421 |  317 | `		pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|    1421 |  318 | `		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|    1421 |  319 | `		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|    1421 |  320 | `		VmRestoreCtxState(pVm, pCtx);` |
|    1421 |  321 | `		if( pSeg ){` |
|       - |  322 | `			/* Reactivate the parked records' recursion accounting (mirror of the` |
|       - |  323 | `			 * deactivate at suspend); aSelf was just restored above. */` |
|     106 |  324 | `			pVm->nRecursionDepth += pSeg->nRecords;` |
|       - |  325 | `			/* Rebase the parked segment's absolute exception-floor indices: the` |
|       - |  326 | `			 * fiber may resume at a different caller depth than it suspended at,` |
|       - |  327 | `			 * so every activation's nExceptionBase shifts by the same delta the` |
|       - |  328 | `			 * republished handlers moved (newBase - the park-time base). */` |
|     106 |  329 | `			sxi32 iDelta = (sxi32)pCtx->nExceptionBase - (sxi32)pSeg->nOldExcBase;` |
|       - |  330 | `			/* nFinallyActBase floors rebase by their OWN delta — the exception and` |
|       - |  331 | `			 * finally-action stacks move independently between suspend and resume` |
|       - |  332 | `			 * (a fiber resumed from inside a generator's inline finally sees a` |
|       - |  333 | `			 * DEEPER aFinallyAction with an unchanged aException, and a stale` |
|       - |  334 | `			 * absolute floor would make the activation-end discard eat the` |
|       - |  335 | `			 * resumer's pending action). */` |
|     106 |  336 | `			sxi32 iFinDelta = (sxi32)pCtx->nFinallyBase - (sxi32)pSeg->nOldFinBase;` |
|     106 |  337 | `			if( iDelta != 0 \|\| iFinDelta != 0 ){` |
|       - |  338 | `				VmCallFrame *pRec;` |
|     ! 0 |  339 | `				pSeg->sState.nExceptionBase =` |
|     ! 0 |  340 | `					(sxu32)((sxi32)pSeg->sState.nExceptionBase + iDelta);` |
|     ! 0 |  341 | `				pSeg->sState.nFinallyActBase =` |
|     ! 0 |  342 | `					(sxu32)((sxi32)pSeg->sState.nFinallyActBase + iFinDelta);` |
|     ! 0 |  343 | `				for( pRec = pSeg->pCallTop; pRec; pRec = pRec->pPrev ){` |
|     ! 0 |  344 | `					pRec->sCaller.nExceptionBase =` |
|     ! 0 |  345 | `						(sxu32)((sxi32)pRec->sCaller.nExceptionBase + iDelta);` |
|     ! 0 |  346 | `					pRec->sCaller.nFinallyActBase =` |
|     ! 0 |  347 | `						(sxu32)((sxi32)pRec->sCaller.nFinallyActBase + iFinDelta);` |
|     ! 0 |  348 | `				}` |
|     ! 0 |  349 | `			}` |
|      51 |  350 | `		}` |
|       - |  351 | `		/* Re-attach the coroutine to the live VM frame chain: the body frame's` |
|       - |  352 | `		 * parent becomes the resumer's current frame. For a deep segment the` |
|       - |  353 | `		 * suspend-time top frame (the innermost callee / open-try wrapper) then` |
|       - |  354 | `		 * becomes current so the adopt at VmByteCodeExec entry resumes inside the` |
|       - |  355 | `		 * callee; body-level resumes make the body frame current. */` |
|    1421 |  356 | `		pCtx->pFrame->pParent = pVm->pFrame;` |
|    1421 |  357 | `		pVm->pFrame = pSeg ? pSeg->pTopFrame : pCtx->pFrame;` |
|       - |  358 | `	}` |
|       - |  359 | `	/* The segment (if any) is handed to the body invocation explicitly below; it` |
|       - |  360 | `	 * is no longer part of the suspended ctx state once resume owns it. */` |
|    1421 |  361 | `	pCtx->pParkedSegment = 0;` |
|       - |  362 | `	/* Save and set the active context */` |
|    1421 |  363 | `	pOldCtx = pVm->pActiveCtx;` |
|    1421 |  364 | `	pVm->pActiveCtx = pCtx;` |
|    1421 |  365 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|    1421 |  366 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1; /* see VmStartCtx */` |
|       - |  367 | `	/* Resume execution from saved PC. pSeg, when non-NULL, makes this body` |
|       - |  368 | `	 * re-enter inside the innermost parked callee (deep fiber resume, stage 4). */` |
|    2129 |  369 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     708 |  370 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|     708 |  371 | `		VmCtxEnforceRetFunc(pCtx), FALSE, pSeg, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|    1421 |  372 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     713 |  373 | `}` |
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
|     252 |  402 | `static sxi32 VmCloseCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  403 | `{` |
|       - |  404 | `	sxi32 rc;` |
|     257 |  405 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       - |  406 | `		/* CREATED never ran, so has no open try; COMPLETED/CLOSED already ran theirs. */` |
|     219 |  407 | `		return SXRET_OK;` |
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
|     131 |  434 | `}` |
|       - |  435 | `/*` |
|       - |  436 | ` * Free one DETACHED frame (not in the live pVm->pFrame chain): the frame of a` |
|       - |  437 | ` * suspended coroutine's body, or of a segment activation abandoned mid-call.` |
|       - |  438 | ` * Mirrors VmLeaveFrame's teardown minus the chain pop (there is no chain to pop` |
|       - |  439 | ` * from). Factored so the body-frame free and the stage-4 segment free share it.` |
|       - |  440 | ` */` |
|     682 |  441 | `static void VmFreeDetachedFrame(ph7_vm *pVm, VmFrame *pFrame)` |
|       5 |  442 | `{` |
|       - |  443 | `	VmSlot *aSlot;` |
|       - |  444 | `	sxu32 n;` |
|     687 |  445 | `	if( pFrame == 0 ){` |
|     ! 0 |  446 | `		return;` |
|       - |  447 | `	}` |
|       - |  448 | `	/* Free local variables */` |
|     687 |  449 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|    1367 |  450 | `	for( n = 0; n < SySetUsed(&pFrame->sLocal); ++n ){` |
|     685 |  451 | `		PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|     345 |  452 | `	}` |
|       - |  453 | `	/* Remove local references */` |
|     687 |  454 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|    1367 |  455 | `	for( n = 0; n < SySetUsed(&pFrame->sRef); ++n ){` |
|     685 |  456 | `		PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|     345 |  457 | `	}` |
|     687 |  458 | `	SyHashRelease(&pFrame->hVar);` |
|     687 |  459 | `	SySetRelease(&pFrame->sArg);` |
|     687 |  460 | `	SySetRelease(&pFrame->sLocal);` |
|     687 |  461 | `	SySetRelease(&pFrame->sRef);` |
|     687 |  462 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|       - |  463 | `	/* Drop a resume target pointing at this detached frame before we free it (ROOT B). */` |
|     687 |  464 | `	VmDropResumeTarget(pVm,pFrame);` |
|     687 |  465 | `	SyMemBackendPoolFree(&pVm->sAllocator, pFrame);` |
|     346 |  466 | `}` |
|       - |  467 | `/*` |
|       - |  468 | ` * Free a parked deep-suspend segment (stage 4) whose fiber was abandoned while` |
|       - |  469 | ` * suspended. Every record holds a callee's operand stack and VmFrame (the` |
|       - |  470 | ` * topmost record's callee is the innermost activation, running on sState); walk` |
|       - |  471 | ` * the chain releasing each callee stack's live entries then the stack and frame.` |
|       - |  472 | ` * The body frame/stack are NOT here — they are freed by the caller` |
|       - |  473 | ` * (VmReleaseExecCtx) as pCtx->pFrame / pCtx->pStack.` |
|       - |  474 | ` */` |
|     200 |  475 | `static void VmFreeParkedSegment(ph7_vm *pVm, ph7_exec_ctx *pCtx, VmParkedSegment *pSeg)` |
|       1 |  476 | `{` |
|       - |  477 | `	/* Live top-of-stack of the activation running on the current record's callee` |
|       - |  478 | `	 * stack: the innermost (sState) for the topmost record, then each caller. */` |
|     201 |  479 | `	ph7_value *pTosAbove = pSeg->sState.pTos;` |
|     201 |  480 | `	VmCallFrame *pRec = pSeg->pCallTop, *pNext;` |
|     401 |  481 | `	while( pRec ){` |
|     201 |  482 | `		ph7_value *pStk = pRec->sCall.pFrameStack;` |
|     201 |  483 | `		if( pStk ){` |
|     201 |  484 | `			ph7_value *pTos = pTosAbove;` |
|     401 |  485 | `			while( pTos >= pStk ){` |
|     201 |  486 | `				PH7_MemObjRelease(pTos);` |
|     201 |  487 | `				pTos--;` |
|       1 |  488 | `			}` |
|     201 |  489 | `			SyMemBackendFree(&pVm->sAllocator, pStk);` |
|     100 |  490 | `		}` |
|     201 |  491 | `		VmFreeDetachedFrame(pVm, pRec->sCall.pFrame);` |
|       - |  492 | `		/* The caller recorded here runs on the NEXT-lower callee stack; grab its` |
|       - |  493 | `		 * live tos before freeing this node. */` |
|     201 |  494 | `		pTosAbove = pRec->sCaller.pTos;` |
|     201 |  495 | `		pNext = pRec->pPrev;` |
|     201 |  496 | `		SyMemBackendPoolFree(&pVm->sAllocator, pRec);` |
|     201 |  497 | `		pRec = pNext;` |
|       1 |  498 | `	}` |
|       - |  499 | `	/* pTosAbove now points at the BODY activation's live top (the bottom record's` |
|       - |  500 | `	 * sCaller, which runs on pCtx->pStack). pCtx->nTos still holds the INNERMOST` |
|       - |  501 | `	 * index (VmSuspendCtx saved it), which would over-index the body stack in` |
|       - |  502 | `	 * VmReleaseExecCtx's release loop — correct it to the body's real top. */` |
|     201 |  503 | `	pCtx->nTos = (sxi32)(pTosAbove - pCtx->pStack);` |
|     201 |  504 | `	SyMemBackendFree(&pVm->sAllocator, pSeg);` |
|     201 |  505 | `}` |
|       - |  506 | `/*` |
|       - |  507 | ` * Release an execution context and all its resources.` |
|       - |  508 | ` */` |
|     482 |  509 | `PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  510 | `{` |
|     487 |  511 | `	if( pCtx == 0 ){` |
|     ! 0 |  512 | `		return;` |
|       - |  513 | `	}` |
|     487 |  514 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       - |  515 | `		/* Cannot destroy a fiber that is currently executing */` |
|     ! 0 |  516 | `		return;` |
|       - |  517 | `	}` |
|     487 |  518 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       - |  519 | `	/* Release values */` |
|     487 |  520 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|     487 |  521 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|     487 |  522 | `	PH7_MemObjRelease(&pCtx->sDelegate);` |
|       - |  523 | `	/* Stage 2b: the parked entries are per-activation clones now — free them` |
|       - |  524 | `	 * (an abandoned suspended coroutine is their last holder). */` |
|     487 |  525 | `	VmExcReleaseAll(pVm,&pCtx->aSavedException);` |
|     487 |  526 | `	SySetRelease(&pCtx->aSavedException);` |
|       - |  527 | `	/* ROOT C: a generator abandoned while suspended inside a finally may carry parked` |
|       - |  528 | `	 * finally actions holding owned values/refs (a RETURN's sRet, a RETHROW's pExc).` |
|       - |  529 | `	 * Release them so the abandon path leaks nothing. */` |
|       - |  530 | `	{` |
|     487 |  531 | `		sxu32 n = SySetUsed(&pCtx->aSavedFinally);` |
|     487 |  532 | `		if( n > 0 ){` |
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
|     487 |  543 | `		SySetRelease(&pCtx->aSavedFinally);` |
|       - |  544 | `	}` |
|       - |  545 | `	/* Stage 4: parked aSelf entries are borrowed class pointers — just free the set. */` |
|     487 |  546 | `	SySetRelease(&pCtx->aSavedSelf);` |
|       - |  547 | `	/* Free a parked deep-suspend segment (stage 4): the fiber was abandoned while` |
|       - |  548 | `	 * suspended inside a nested call, so its record chain / frames / operand` |
|       - |  549 | `	 * stacks are still alive and only this holder references them. Must run` |
|       - |  550 | `	 * before the body frame/stack below (they are the segment's floor). */` |
|     487 |  551 | `	if( pCtx->pParkedSegment ){` |
|     201 |  552 | `		VmFreeParkedSegment(pVm, pCtx, (VmParkedSegment *)pCtx->pParkedSegment);` |
|     201 |  553 | `		pCtx->pParkedSegment = 0;` |
|     100 |  554 | `	}` |
|       - |  555 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|     487 |  556 | `	if( pCtx->pFrame ){` |
|     487 |  557 | `		VmFreeDetachedFrame(pVm, pCtx->pFrame);` |
|     487 |  558 | `		pCtx->pFrame = 0;` |
|     241 |  559 | `	}` |
|       - |  560 | `	/* Release individual operand stack entries (decrement refcounts,` |
|       - |  561 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|       - |  562 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|     487 |  563 | `	if( pCtx->pStack ){` |
|     487 |  564 | `		if( pCtx->nTos >= 0 ){` |
|     437 |  565 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|     869 |  566 | `			while( pTos >= pCtx->pStack ){` |
|     437 |  567 | `				PH7_MemObjRelease(pTos);` |
|     437 |  568 | `				pTos--;` |
|       5 |  569 | `			}` |
|     216 |  570 | `		}` |
|     487 |  571 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|     487 |  572 | `		pCtx->pStack = 0;` |
|     241 |  573 | `	}` |
|       - |  574 | `	/* Free the context itself */` |
|     487 |  575 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     246 |  576 | `}` |
|       - |  577 | `/*` |
|       - |  578 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|       - |  579 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|       - |  580 | ` */` |
|     780 |  581 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|       5 |  582 | `{` |
|       - |  583 | `	ph7_class_instance *pThis;` |
|       - |  584 | `	SyString sAttr;` |
|       - |  585 | `	ph7_value *pAttr;` |
|     785 |  586 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 |  587 | `		return 0;` |
|       - |  588 | `	}` |
|     785 |  589 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|     785 |  590 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|     ! 0 |  591 | `		return 0;` |
|       - |  592 | `	}` |
|     785 |  593 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     785 |  594 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     785 |  595 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     275 |  596 | `		return 0;` |
|       - |  597 | `	}` |
|     515 |  598 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|     395 |  599 | `}` |
|       - |  600 | `/* ph7_class_instance.iFlags bit: this Closure is a bound/static first-class callable and` |
|       - |  601 | ` * carries $__this/$__scope. Lets the hot plain-closure unwrap skip those attribute lookups.` |
|       - |  602 | ` * (Distinct from CLASS_INSTANCE_DESTROYED 0x001 and VM_INSTANCE_DUMPING 0x002.) */` |
|       - |  603 | `#define VM_INSTANCE_FCC_BOUND 0x004` |
|       - |  604 | `/* ph7_class_instance.iFlags bit: this Closure wraps an __invoke OBJECT, and the engine —` |
|       - |  605 | `` * not the source — is what named `__invoke` (Closure::fromCallable($obj)). php resolves it`` |
|       - |  606 | `` * the way it resolves `$obj()`, so a non-public __invoke is dispatched rather than denied;`` |
|       - |  607 | `` * `$obj->__invoke(...)` and `[$obj,'__invoke']`, which the SOURCE names, stay denied and`` |
|       - |  608 | ` * never carry this bit. Read by VmClosureUnwrap, which arms the engine's magic latch. */` |
|       - |  609 | `#define VM_INSTANCE_FCC_INVOKE_OBJ 0x010` |
|       - |  610 | `/*` |
|       - |  611 | `` * A PHP closure (and a first-class callable `f(...)`) is a real object: an instance of`` |
|       - |  612 | `` * the built-in final `Closure` class carrying its underlying callable in a private`` |
|       - |  613 | `` * `$__fn` attribute (the callable NAME: a registered `[closure_N]`/`[lambda_N]` or a`` |
|       - |  614 | ` * user/host function name) — plus, for a method/static first-class callable, a bound` |
|       - |  615 | `` * `$__this` object and/or a `$__scope` class-name. Storing the callable as plain attributes`` |
|       - |  616 | `` * (rather than a native resource pointer) means the object owns no `ph7_vm_func` — the`` |
|       - |  617 | `` * per-closure function stays owned by `hFunction`/VM-release exactly as before, so there is`` |
|       - |  618 | ` * no extra free path.` |
|       - |  619 | ` *` |
|       - |  620 | ` * Returns non-zero iff pVal is a Closure instance.` |
|       - |  621 | ` */` |
| 7026687 |  622 | `PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm, ph7_value *pVal)` |
|       5 |  623 | `{` |
|       - |  624 | `	ph7_class_instance *pThis;` |
|       - |  625 | `	/* Flag test first: a non-object call target (the hot common case) bails before any` |
|       - |  626 | `	 * pVm dereference; pClosureClass==0 is a one-time pre-init concern, so it goes last. */` |
| 7026692 |  627 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
| 6814200 |  628 | `		return 0;` |
|       - |  629 | `	}` |
|  212497 |  630 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       - |  631 | `	/* Closure is final, so an exact class match is correct (no subclasses possible). */` |
|  212497 |  632 | `	return pThis->pClass == pVm->pClosureClass;` |
| 3514290 |  633 | `}` |
|       - |  634 | `/*` |
|       - |  635 | ` * Unwrap a Closure value into the simple callable the existing dispatch machinery` |
|       - |  636 | ` * already understands, written into pOut (which the caller must have initialised):` |
|       - |  637 | `` *   - bound `$this` set (method first-class callable) -> [ $__this, $__fn ] array callable`` |
|       - |  638 | `` *   - `$__scope` set (static first-class callable)     -> [ $__scope, $__fn ] array callable`` |
|       - |  639 | `` *   - neither (plain function / real closure)          -> the `$__fn` name string`` |
|       - |  640 | ` * The 2-element array form rides the existing MEMOBJ_HASHMAP dispatch, which binds $this` |
|       - |  641 | ` * for an object first element and resolves the class for a class-name-string first element.` |
|       - |  642 | ` * Returns SXRET_OK if pVal was a Closure (pOut filled), SXERR_NOTFOUND otherwise.` |
|       - |  643 | ` */` |
|    4910 |  644 | `PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm, ph7_value *pVal, ph7_value *pOut)` |
|       5 |  645 | `{` |
|       - |  646 | `	ph7_class_instance *pThis;` |
|       - |  647 | `	ph7_value *pFn;` |
|       - |  648 | `	SyString sAttr;` |
|    4915 |  649 | `	if( !VmValueIsClosure(pVm, pVal) ){` |
|     ! 0 |  650 | `		return SXERR_NOTFOUND;` |
|       - |  651 | `	}` |
|    4915 |  652 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    4915 |  653 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    4915 |  654 | `	pFn = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    4915 |  655 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 |  656 | `		return SXERR_NOTFOUND; /* malformed/uninitialised closure */` |
|       - |  657 | `	}` |
|       - |  658 | `	/* Only a bound/static first-class callable (rare) carries $__this/$__scope; the` |
|       - |  659 | `	 * VM_INSTANCE_FCC_BOUND flag (set by VmCreateClosure) keeps the hot plain-closure path` |
|       - |  660 | `	 * to the single $__fn lookup above instead of two extra attribute lookups per dispatch. */` |
|    4915 |  661 | `	if( pThis->iFlags & VM_INSTANCE_FCC_BOUND ){` |
|       - |  662 | `		ph7_value *pBound, *pScope;` |
|       - |  663 | `		int bBoundObj, bScope;` |
|     125 |  664 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     125 |  665 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     125 |  666 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     125 |  667 | `		pScope = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     125 |  668 | `		bBoundObj = pBound && (pBound->iFlags & MEMOBJ_OBJ);` |
|     125 |  669 | `		bScope = pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0;` |
|     125 |  670 | `		if( bBoundObj \|\| bScope ){` |
|       - |  671 | `			/* Method/static first-class callable -> [ target, "method" ] array callable. */` |
|     125 |  672 | `			if( pThis->iFlags & VM_INSTANCE_FCC_INVOKE_OBJ ){` |
|       - |  673 | `				/* Closure::fromCallable($obj): the engine named __invoke, so this` |
|       - |  674 | `				 * dispatch is the engine's own and a non-public one still runs. */` |
|      10 |  675 | `				pVm->bMagicDispatch = 1;` |
|       4 |  676 | `			}` |
|       - |  677 | `			ph7_hashmap *pMap;` |
|       - |  678 | `			ph7_value sTarget, sMeth;` |
|       - |  679 | `			sxi32 rc;` |
|     125 |  680 | `			if( bBoundObj ){` |
|      89 |  681 | `				ph7_class_instance *pBoundObj = (ph7_class_instance *)pBound->x.pOther;` |
|       - |  682 | ``				/* A bound PLAIN closure (`function(){…}->bindTo($o)`): $__fn names a function, not a`` |
|       - |  683 | `				 * method of the bound object's class, so a [obj,method] array callable would fail` |
|       - |  684 | `				 * method resolution. Stash the bound object (own a ref) so the OP_CALL user-function` |
|       - |  685 | `				 * frame setup injects it as $this, and return the plain $__fn string for a normal` |
|       - |  686 | `				 * function dispatch. */` |
|     129 |  687 | `				if( PH7_ClassExtractMethod(pBoundObj->pClass,` |
|     132 |  688 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){` |
|       - |  689 | `					/* Only a USER function (anonymous closure / named fn in hFunction) reads $this and` |
|       - |  690 | `					 * reaches the OP_CALL user-function frame-setup that consumes pClosureThis. A HOST` |
|       - |  691 | `					 * function (e.g. Closure::fromCallable('strlen')->bindTo($o)) ignores $this and` |
|       - |  692 | `					 * dispatches via the host path which never consumes the transient — setting it` |
|       - |  693 | `					 * there would leak the ref and inject a stale $this into the next call. */` |
|      60 |  694 | `					if( SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|      41 |  695 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|      41 |  696 | `						pBoundObj->iRef++;` |
|      41 |  697 | `						pVm->pClosureThis = pBoundObj;` |
|       - |  698 | `						/* Carry the bound scope (if set) so private/protected member access inside` |
|       - |  699 | `						 * the closure body resolves against it — bindTo($o, Scope::class) / call($o). */` |
|      41 |  700 | `						if( bScope ){` |
|      46 |  701 | `							pVm->pClosureScope = PH7_VmExtractClass(pVm,` |
|      30 |  702 | `								(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      15 |  703 | `						}` |
|      20 |  704 | `					}` |
|      41 |  705 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      41 |  706 | `					return SXRET_OK;` |
|       - |  707 | `				}` |
|      26 |  708 | `			}else{` |
|       - |  709 | ``				/* Scope-only rebind of a PLAIN closure (`bindTo(null, Scope::class)`):`` |
|       - |  710 | `				 * $__fn names a function, not a static method of the scope class, so` |
|       - |  711 | `				 * the [scope, method] array callable below would fail method` |
|       - |  712 | `				 * resolution. Dispatch the plain $__fn string and carry the scope for` |
|       - |  713 | `				 * private/protected visibility (pClosureThis stays unset — no $this).` |
|       - |  714 | `				 * A static-method FCC ($__fn really is a method of the scope class)` |
|       - |  715 | `				 * falls through to the array-callable path. */` |
|      55 |  716 | `				ph7_class *pScopeClass = PH7_VmExtractClass(pVm,` |
|      36 |  717 | `					(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      36 |  718 | `				if( pScopeClass == 0` |
|      37 |  719 | `				 \|\| PH7_ClassExtractMethod(pScopeClass,` |
|      54 |  720 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){` |
|       6 |  721 | `					if( pScopeClass` |
|       7 |  722 | `					 && SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|       6 |  723 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|       7 |  724 | `						pVm->pClosureScope = pScopeClass;` |
|       3 |  725 | `					}` |
|       7 |  726 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|       7 |  727 | `					return SXRET_OK;` |
|       - |  728 | `				}` |
|       - |  729 | `			}` |
|      79 |  730 | `			pMap = PH7_NewHashmap(&(*pVm), 0, 0);` |
|      79 |  731 | `			if( pMap == 0 ){` |
|     ! 0 |  732 | `				return SXERR_NOTFOUND;` |
|       - |  733 | `			}` |
|      79 |  734 | `			PH7_MemObjInit(pVm, &sTarget);` |
|      79 |  735 | `			PH7_MemObjInit(pVm, &sMeth);` |
|      79 |  736 | `			if( bBoundObj ){` |
|      49 |  737 | `				PH7_MemObjStore(pBound, &sTarget); /* bound object (iRef++) -> binds $this */` |
|      26 |  738 | `			}else{` |
|      31 |  739 | `				PH7_MemObjStringAppend(&sTarget, SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob));` |
|       - |  740 | `			}` |
|      79 |  741 | `			PH7_MemObjStringAppend(&sMeth, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      79 |  742 | `			rc = PH7_HashmapInsert(pMap, 0, &sTarget);` |
|      79 |  743 | `			if( rc == SXRET_OK ){` |
|      79 |  744 | `				rc = PH7_HashmapInsert(pMap, 0, &sMeth);` |
|      38 |  745 | `			}` |
|      79 |  746 | `			PH7_MemObjRelease(&sTarget);` |
|      79 |  747 | `			PH7_MemObjRelease(&sMeth);` |
|      79 |  748 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  749 | `				PH7_HashmapRelease(pMap, TRUE); /* free the partial map, no leak */` |
|     ! 0 |  750 | `				return SXERR_NOTFOUND;` |
|       - |  751 | `			}` |
|      79 |  752 | `			pOut->x.pOther = pMap;` |
|      79 |  753 | `			MemObjSetType(pOut, MEMOBJ_HASHMAP);` |
|      79 |  754 | `			return SXRET_OK;` |
|       - |  755 | `		}` |
|     ! 0 |  756 | `	}` |
|    4793 |  757 | `	PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    4793 |  758 | `	return SXRET_OK;` |
|    2460 |  759 | `}` |
|       - |  760 | `/*` |
|       - |  761 | `` * Resolve the scope class for a STATIC first-class callable `T::m(...)`, where T is a`` |
|       - |  762 | ` * class-name STRING value: handles the self/static/parent keywords against the live class` |
|       - |  763 | ` * context (so the actual class is bound at FCC-creation time, like PHP), and falls back to` |
|       - |  764 | ` * an explicit class-name lookup. Mirrors the static OP_MEMBER resolution. Returns 0 if the` |
|       - |  765 | ` * class cannot be resolved.` |
|       - |  766 | ` */` |
|      68 |  767 | `PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget)` |
|       2 |  768 | `{` |
|     138 |  769 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|      68 |  770 | `		(sxu32)SyBlobLength(&pTarget->sBlob));` |
|       2 |  771 | `}` |
|       - |  772 | `/*` |
|       - |  773 | ` * The same resolution over a raw (name, length) pair, for the callable machinery: php` |
|       - |  774 | `` * resolves `self`/`parent`/`static` in a CALLBACK (is_callable, call_user_func, array_map …)`` |
|       - |  775 | `` * against the live class context, and refuses them in the direct `$cb()` dispatch — so this`` |
|       - |  776 | ` * is deliberately NOT wired into the OP_CALL resolve check, which must keep answering` |
|       - |  777 | `` * `Class "self" not found`.`` |
|       - |  778 | ` */` |
|  100602 |  779 | `PH7_PRIVATE ph7_class * PH7_VmResolveScopeName(ph7_vm *pVm, const char *zCls, sxu32 nCls)` |
|       5 |  780 | `{` |
|       - |  781 | `	ph7_class *pClass;` |
|  100607 |  782 | `	if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|      80 |  783 | `		pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      80 |  784 | `		if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       3 |  785 | `			pClass = PH7_VmPeekTopClass(&(*pVm)); /* self:: in a trait -> using class */` |
|       5 |  786 | `		}` |
|  100569 |  787 | `	}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|      66 |  788 | `		pClass = PH7_VmPeekTopClass(&(*pVm));     /* late static binding */` |
|  100500 |  789 | `	}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|      44 |  790 | `		pClass = PH7_VmResolveParentClass(&(*pVm));` |
|      24 |  791 | `	}else{` |
|  100429 |  792 | `		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - |  793 | `	}` |
|  100607 |  794 | `	return pClass;` |
|       5 |  795 | `}` |
|       - |  796 | `/*` |
|       - |  797 | ` * Create a Closure object wrapping a callable name (+ optional bound $this object and/or` |
|       - |  798 | `` * scope class-name, for the method/static first-class callables `$o->m(...)`/`C::m(...)`).`` |
|       - |  799 | ` * Mirrors the Generator/Fiber "object carries its state in private attributes" pattern.` |
|       - |  800 | ` * Returns the fresh instance (iRef == 0; caller takes the reference), or 0 on OOM.` |
|       - |  801 | ` */` |
|    3640 |  802 | `PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName,` |
|       - |  803 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       5 |  804 | `{` |
|       - |  805 | `	ph7_class_instance *pObj;` |
|       - |  806 | `	ph7_value *pAttr;` |
|       - |  807 | `	SyString sAttr;` |
|    3645 |  808 | `	if( pVm->pClosureClass == 0 ){` |
|     ! 0 |  809 | `		return 0;` |
|       - |  810 | `	}` |
|    3645 |  811 | `	pObj = PH7_NewClassInstance(&(*pVm), pVm->pClosureClass);` |
|    3645 |  812 | `	if( pObj == 0 ){` |
|     ! 0 |  813 | `		return 0;` |
|       - |  814 | `	}` |
|    3645 |  815 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    3645 |  816 | `	pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|    3645 |  817 | `	if( pAttr ){` |
|    3645 |  818 | `		PH7_MemObjStringAppend(pAttr, pName->zString, pName->nByte);` |
|    1820 |  819 | `	}` |
|    3645 |  820 | `	if( pBoundThis ){` |
|      49 |  821 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|      49 |  822 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|      49 |  823 | `		if( pAttr ){` |
|      49 |  824 | `			pAttr->x.pOther = pBoundThis;` |
|      49 |  825 | `			MemObjSetType(pAttr, MEMOBJ_OBJ);` |
|      49 |  826 | `			pBoundThis->iRef++; /* keep the bound object alive for the closure's lifetime */` |
|      23 |  827 | `		}` |
|      23 |  828 | `	}` |
|    3645 |  829 | `	if( pScope && pScope->nByte ){` |
|      81 |  830 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      81 |  831 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|      81 |  832 | `		if( pAttr ){` |
|      81 |  833 | `			PH7_MemObjStringAppend(pAttr, pScope->zString, pScope->nByte);` |
|      39 |  834 | `		}` |
|      39 |  835 | `	}` |
|    3645 |  836 | `	if( pBoundThis \|\| (pScope && pScope->nByte) ){` |
|       - |  837 | `		/* Mark bound/static FCC closures so VmClosureUnwrap can skip the $__this/$__scope` |
|       - |  838 | `		 * lookups on the hot plain-closure dispatch path. */` |
|      81 |  839 | `		pObj->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      39 |  840 | `	}` |
|    3645 |  841 | `	return pObj;` |
|    1825 |  842 | `}` |
|       - |  843 | `/*` |
|       - |  844 | ` * Exported wrapper around VmCreateClosure for builtin libraries outside this` |
|       - |  845 | ` * file (ReflectionFunction::getClosure / ReflectionMethod::getClosure).` |
|       - |  846 | ` */` |
|       6 |  847 | `PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm, const SyString *pName,` |
|       - |  848 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       1 |  849 | `{` |
|       7 |  850 | `	return VmCreateClosure(&(*pVm), pName, pBoundThis, pScope);` |
|       1 |  851 | `}` |
|       - |  852 | `/*` |
|       - |  853 | ` * Exported wrapper around the typed/readonly property store enforcement for` |
|       - |  854 | ` * ReflectionProperty::setValue (vm_builtin_reflection.c). Same contract:` |
|       - |  855 | ` * SXRET_OK (value possibly coerced in place), PH7_EXCEPTION, or PH7_ABORT.` |
|       - |  856 | ` */` |
|       4 |  857 | `PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|       1 |  858 | `{` |
|       5 |  859 | `	return VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pValue,0);` |
|       1 |  860 | `}` |
|       - |  861 | `/*` |
|       - |  862 | ` * Exported reference-table probe for ReflectionReference::fromArrayElement` |
|       - |  863 | ` * (vm_builtin_reflection.c). Returns the number of links (frame variables +` |
|       - |  864 | ` * array entries) attached to the slot's reference record, 0 when the slot` |
|       - |  865 | ` * has none — an array element is a PHP reference when this is >= 2.` |
|       - |  866 | ` */` |
|       6 |  867 | `PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx)` |
|       1 |  868 | `{` |
|       7 |  869 | `	VmRefObj *pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       7 |  870 | `	if( pRef == 0 ){` |
|     ! 0 |  871 | `		return 0;` |
|       - |  872 | `	}` |
|       7 |  873 | `	return (int)(SySetUsed(&pRef->aReference) + SySetUsed(&pRef->aArrEntries));` |
|       4 |  874 | `}` |
|       - |  875 | `/*` |
|       - |  876 | `` * First-class callable over an arbitrary callable VALUE: `($expr)(...)`.`` |
|       - |  877 | ` * Normalize a validated callable value into a fresh Closure, reusing VmCreateClosure (the same` |
|       - |  878 | ` * object the method/static first-class-callable paths mint, so dispatch round-trips identically` |
|       - |  879 | ` * via VmClosureUnwrap). Handles the three remaining callable shapes a value can hold:` |
|       - |  880 | ` *   - a function-NAME string          -> plain closure ($__fn = name)` |
|       - |  881 | ` *   - a [target, method] array callable -> bound (object target -> $this) / static (class-name` |
|       - |  882 | ` *     target -> scope) closure, mirroring the [obj,m]/[class,m] decode in PH7_VmIsCallable` |
|       - |  883 | ` *   - an __invoke object               -> closure bound to the object's __invoke` |
|       - |  884 | ` * An existing Closure returns 0 here (it is already a Closure — the caller keeps it as-is), so this` |
|       - |  885 | ` * stays idempotent even for a direct caller. Returns the fresh instance (iRef == 0; caller takes the` |
|       - |  886 | ` * reference) or 0 if the value is an existing Closure / not a normalizable callable / on OOM — in` |
|       - |  887 | ` * which case the caller leaves the value untouched (graceful degradation). This is the generic` |
|       - |  888 | ` * "callable value -> Closure" primitive: the body is FCC-agnostic and self-contained, so the future` |
|       - |  889 | ` * Closure::bind/fromCallable work (Increment 2) can call it directly.` |
|       - |  890 | ` */` |
|      82 |  891 | `PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue)` |
|       3 |  892 | `{` |
|       - |  893 | `	/* A Closure is already callable; never double-wrap it (this also stops the __invoke branch` |
|       - |  894 | `	 * below from binding a closure to its OWN __invoke). The OP_LOAD_FCC caller intercepts a` |
|       - |  895 | `	 * Closure first too, but guarding here keeps the primitive safe for a direct Increment-2 caller. */` |
|      85 |  896 | `	if( VmValueIsClosure(pVm, pValue) ){` |
|     ! 0 |  897 | `		return 0;` |
|       - |  898 | `	}` |
|      85 |  899 | `	if( !PH7_VmIsCallable(pVm, pValue, TRUE) ){` |
|     ! 0 |  900 | `		return 0;` |
|       - |  901 | `	}` |
|      85 |  902 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  903 | `		SyString sName;` |
|      53 |  904 | `		SyStringInitFromBuf(&sName, SyBlobData(&pValue->sBlob), SyBlobLength(&pValue->sBlob));` |
|      53 |  905 | `		return VmCreateClosure(pVm, &sName, 0, 0);` |
|       - |  906 | `	}` |
|      34 |  907 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  908 | `		/* [target, method] — the same index-0/1 decode PH7_VmIsCallable uses to validate it` |
|       - |  909 | `		 * (php reads the INTEGER indices, not insertion order). */` |
|      23 |  910 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - |  911 | `		ph7_value *pTarget, *pMeth;` |
|       - |  912 | `		SyString sName;` |
|      23 |  913 | `		if( !PH7_VmArrayCallableParts(pVm, pMap, &pTarget, &pMeth) ){` |
|     ! 0 |  914 | `			return 0;` |
|       - |  915 | `		}` |
|      23 |  916 | `		if( (pMeth->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pMeth->sBlob) == 0 ){` |
|     ! 0 |  917 | `			return 0;` |
|       - |  918 | `		}` |
|      23 |  919 | `		SyStringInitFromBuf(&sName, SyBlobData(&pMeth->sBlob), SyBlobLength(&pMeth->sBlob));` |
|      23 |  920 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      13 |  921 | `			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      13 |  922 | `			return VmCreateClosure(pVm, &sName, pBoundThis, &pBoundThis->pClass->sName);` |
|     ! 0 |  923 | `		}else{` |
|       - |  924 | `			/* [class-name, method] static callable -> bind the resolved scope. A runtime array` |
|       - |  925 | `			 * callable carries a concrete class name (never self/static/parent), so a plain class` |
|       - |  926 | ``			 * lookup is correct — unlike the syntactic `C::m(...)` path, which must resolve`` |
|       - |  927 | `			 * self/static/parent via VmFccResolveScope. Matches PH7_VmIsCallable's own decode. */` |
|      11 |  928 | `			ph7_class *pScopeCls = PH7_VmExtractClassFromValue(pVm, pTarget);` |
|      11 |  929 | `			return pScopeCls ? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;` |
|       - |  930 | `		}` |
|       - |  931 | `	}` |
|      12 |  932 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - |  933 | `		/* __invoke object (a real Closure is intercepted by the caller before this point).` |
|       - |  934 | ``		 * The `__invoke` name is the ENGINE's, so mark the closure: php dispatches a`` |
|       - |  935 | ``		 * non-public __invoke through this wrapper exactly as it does through `$obj()`. */`` |
|      12 |  936 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|       - |  937 | `		ph7_class_instance *pWrap;` |
|       - |  938 | `		SyString sInvoke;` |
|      12 |  939 | `		SyStringInitFromBuf(&sInvoke, "__invoke", sizeof("__invoke") - 1);` |
|      12 |  940 | `		pWrap = VmCreateClosure(pVm, &sInvoke, pObj, &pObj->pClass->sName);` |
|      12 |  941 | `		if( pWrap ){` |
|      12 |  942 | `			pWrap->iFlags \|= VM_INSTANCE_FCC_INVOKE_OBJ;` |
|       5 |  943 | `		}` |
|      12 |  944 | `		return pWrap;` |
|       - |  945 | `	}` |
|       - |  946 | `	/* Unreachable in practice — the PH7_VmIsCallable gate admits only string/array/object, all` |
|       - |  947 | `	 * handled above; kept to satisfy the non-void return path. */` |
|     ! 0 |  948 | `	return 0;` |
|      44 |  949 | `}` |
|       - |  950 | `/*` |
|       - |  951 | ` * Return a fresh Closure instance (iRef==0 from create/clone) as a builtin result, taking the` |
|       - |  952 | ` * one reference into pCtx->pRet — mirrors the OP_LOAD_FCC store convention.` |
|       - |  953 | ` */` |
|      72 |  954 | `static int VmClosureResult(ph7_context *pCtx, ph7_class_instance *pClosure)` |
|       2 |  955 | `{` |
|      74 |  956 | `	if( pClosure == 0 ){` |
|     ! 0 |  957 | `		ph7_result_null(pCtx);` |
|     ! 0 |  958 | `		return PH7_OK;` |
|       - |  959 | `	}` |
|      74 |  960 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      74 |  961 | `	pClosure->iRef++;` |
|      74 |  962 | `	pCtx->pRet->x.pOther = pClosure;` |
|      74 |  963 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|      74 |  964 | `	return PH7_OK;` |
|      38 |  965 | `}` |
|       - |  966 | `/*` |
|       - |  967 | ` * Overwrite a Closure clone's $__this (bound object, or 0 to unbind) and, when pScope != 0, its` |
|       - |  968 | ` * $__scope (the class-name string; pScope->nByte==0 clears it). pScope==0 leaves $__scope as the` |
|       - |  969 | ` * clone inherited it (PHP's "static" = keep-scope). Refreshes VM_INSTANCE_FCC_BOUND from the result.` |
|       - |  970 | ` * The clone inherited a ref on the original $__this (PH7_CloneClassInstance copies via MemObjStore);` |
|       - |  971 | ` * this drops that and takes one on pNewThis.` |
|       - |  972 | ` */` |
|      50 |  973 | `static void VmClosureRebind(ph7_class_instance *pClone,` |
|       - |  974 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|       2 |  975 | `{` |
|       - |  976 | `	SyString sAttr;` |
|       - |  977 | `	ph7_value *pThisAttr, *pScopeAttr;` |
|      52 |  978 | `	int bBound = 0;` |
|      52 |  979 | `	SyStringInitFromBuf(&sAttr, "__this", 6);` |
|      52 |  980 | `	pThisAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      52 |  981 | `	if( pThisAttr ){` |
|       - |  982 | `		/* PH7_MemObjRelease already drops the cloned-in object's refcount for an OBJ value —` |
|       - |  983 | `		 * do NOT also decrement by hand (that double-frees the original bound object). */` |
|      52 |  984 | `		PH7_MemObjRelease(pThisAttr);` |
|      52 |  985 | `		if( pNewThis ){` |
|      44 |  986 | `			pThisAttr->x.pOther = pNewThis;` |
|      44 |  987 | `			MemObjSetType(pThisAttr, MEMOBJ_OBJ);` |
|      44 |  988 | `			pNewThis->iRef++;` |
|      21 |  989 | `		}` |
|      25 |  990 | `	}` |
|      52 |  991 | `	if( pScope ){` |
|      37 |  992 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      37 |  993 | `		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      37 |  994 | `		if( pScopeAttr ){` |
|      37 |  995 | `			PH7_MemObjRelease(pScopeAttr);` |
|      37 |  996 | `			if( pScope->nByte ){` |
|      37 |  997 | `				PH7_MemObjStringAppend(pScopeAttr, pScope->zString, pScope->nByte);` |
|      18 |  998 | `			}` |
|      18 |  999 | `		}` |
|      18 | 1000 | `	}` |
|       - | 1001 | `	/* Refresh the FCC_BOUND fast-path flag from the resulting $__this/$__scope. pThisAttr already` |
|       - | 1002 | `	 * points at the final $__this slot; only $__scope needs a (re)fetch — the top half fetched it` |
|       - | 1003 | `	 * just for pScope != 0. */` |
|      52 | 1004 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      52 | 1005 | `	pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      50 | 1006 | `	if( (pThisAttr && (pThisAttr->iFlags & MEMOBJ_OBJ))` |
|      31 | 1007 | `		\|\| (pScopeAttr && (pScopeAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeAttr->sBlob) > 0) ){` |
|      50 | 1008 | `		bBound = 1;` |
|      24 | 1009 | `	}` |
|      52 | 1010 | `	if( bBound ){` |
|      50 | 1011 | `		pClone->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      26 | 1012 | `	}else{` |
|       3 | 1013 | `		pClone->iFlags &= ~VM_INSTANCE_FCC_BOUND;` |
|       - | 1014 | `	}` |
|      52 | 1015 | `}` |
|       - | 1016 | `/*` |
|       - | 1017 | ` * Resolve the bindTo/bind/call $scope argument to a class-name SyString.` |
|       - | 1018 | ` * Returns 1 and fills *pOut if $__scope should be replaced (pOut->nByte==0 means "clear");` |
|       - | 1019 | ` * returns 0 to leave the scope unchanged (the PHP "static" sentinel / scope omitted).` |
|       - | 1020 | ` */` |
|      50 | 1021 | `static int VmClosureResolveScope(ph7_value *pScopeArg, SyString *pOut)` |
|       2 | 1022 | `{` |
|      52 | 1023 | `	if( pScopeArg == 0 ){` |
|      16 | 1024 | `		return 0; /* keep */` |
|       - | 1025 | `	}` |
|      36 | 1026 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeArg->sBlob) == 6` |
|      18 | 1027 | `		&& SyMemcmp((const void *)SyBlobData(&pScopeArg->sBlob), (const void *)"static", 6) == 0 ){` |
|     ! 0 | 1028 | `		return 0; /* "static" -> keep current scope */` |
|       - | 1029 | `	}` |
|      37 | 1030 | `	if( pScopeArg->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 1031 | `		SyStringInitFromBuf(pOut, 0, 0); /* unscoped */` |
|     ! 0 | 1032 | `		return 1;` |
|       - | 1033 | `	}` |
|      37 | 1034 | `	if( pScopeArg->iFlags & MEMOBJ_OBJ ){` |
|       3 | 1035 | `		ph7_class_instance *pScopeObj = (ph7_class_instance *)pScopeArg->x.pOther;` |
|       3 | 1036 | `		*pOut = pScopeObj->pClass->sName;` |
|       3 | 1037 | `		return 1;` |
|       - | 1038 | `	}` |
|      35 | 1039 | `	if( pScopeArg->iFlags & MEMOBJ_STRING ){` |
|      35 | 1040 | `		SyStringInitFromBuf(pOut, (const char *)SyBlobData(&pScopeArg->sBlob), SyBlobLength(&pScopeArg->sBlob));` |
|      35 | 1041 | `		return 1;` |
|       - | 1042 | `	}` |
|     ! 0 | 1043 | `	return 0;` |
|      27 | 1044 | `}` |
|       - | 1045 | `/*` |
|       - | 1046 | ``  * Fiber's C-bodied methods. Every one of them was a global `__fiber_verb($this,…)` `` |
|       - | 1047 | ` * thunk that a one-line prelude method forwarded to; the class body in the builtin` |
|       - | 1048 | ` * chunk now holds only its two private slots.` |
|       - | 1049 | ` */` |
|    4528 | 1050 | `PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm)` |
|       5 | 1051 | `{` |
|       - | 1052 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 1053 | `		{ "__construct",  PH7_MOD_PUBLIC, "callable $callback",  "",       vm_builtin_Fiber_construct },` |
|       - | 1054 | `		/* Variadic: the arguments now reach the C body directly instead of being` |
|       - | 1055 | `		 * repackaged by a func_get_args() call in the prelude. */` |
|       - | 1056 | `		{ "start",        PH7_MOD_PUBLIC, "mixed ...$args",      "mixed",  vm_builtin_Fiber_start },` |
|       - | 1057 | `		{ "resume",       PH7_MOD_PUBLIC, "mixed $value = null", "mixed",  vm_builtin_Fiber_resume },` |
|       - | 1058 | `		{ "getReturn",    PH7_MOD_PUBLIC, "",                    "mixed",  vm_builtin_Fiber_getReturn },` |
|       - | 1059 | `		{ "isStarted",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isStarted },` |
|       - | 1060 | `		{ "isRunning",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isRunning },` |
|       - | 1061 | `		{ "isSuspended",  PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isSuspended },` |
|       - | 1062 | `		{ "isTerminated", PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isTerminated },` |
|       - | 1063 | `		/* Static, and the only one that never took a receiver even as a thunk:` |
|       - | 1064 | ``		 * `__fiber_suspend($value)` already read the value from argument #0. */`` |
|       - | 1065 | `		{ "suspend",      PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "mixed $value = null", "mixed",` |
|       - | 1066 | `		  vm_builtin_Fiber_suspend },` |
|       - | 1067 | `		{ "__destruct",   PH7_MOD_PUBLIC, "",                    "",       vm_builtin_Fiber_destruct },` |
|       - | 1068 | `	};` |
|       - | 1069 | `	/* The two private slots the methods above keep their state in: the execution` |
|       - | 1070 | `	 * context (a resource) and the callable handed to the constructor. */` |
|       - | 1071 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 1072 | `		{ "__ctx",      PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },` |
|       - | 1073 | `		{ "__callable", PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },` |
|       - | 1074 | `	};` |
|       - | 1075 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1076 | `		"Fiber", 0, 0, 0,` |
|       - | 1077 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|       - | 1078 | `		0, 0,` |
|       - | 1079 | `		aProp, SX_ARRAYSIZE(aProp)` |
|       - | 1080 | `	};` |
|    4533 | 1081 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|       5 | 1082 | `}` |
|       - | 1083 | `/*` |
|       - | 1084 | `` * Generator's C-bodied methods, plus the `implements Iterator` the chunk can no`` |
|       - | 1085 | ` * longer carry: PH7_ClassImplement installs an abstract stub for any interface` |
|       - | 1086 | ` * method the class does not already declare, so it has to run AFTER the eight` |
|       - | 1087 | ` * methods below exist — at which point the stubs are skipped and the class is` |
|       - | 1088 | ` * concrete, exactly as the prelude declaration used to make it.` |
|       - | 1089 | ` */` |
|    4528 | 1090 | `PH7_PRIVATE sxi32 PH7_VmInstallGeneratorNative(ph7_vm *pVm)` |
|       5 | 1091 | `{` |
|       - | 1092 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 1093 | `		{ "current",    PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_current },` |
|       - | 1094 | `		{ "key",        PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_key },` |
|       - | 1095 | `		{ "next",       PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_next },` |
|       - | 1096 | `		{ "rewind",     PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_rewind },` |
|       - | 1097 | `		{ "valid",      PH7_MOD_PUBLIC, "",                 "bool",  vm_builtin_Generator_valid },` |
|       - | 1098 | ``		/* php REQUIRES the argument here; the prelude declared `$value = null`, so`` |
|       - | 1099 | ``		 * `$gen->send()` used to answer the first yielded value instead of raising. */`` |
|       - | 1100 | `		{ "send",       PH7_MOD_PUBLIC, "mixed $value",     "mixed", vm_builtin_Generator_send },` |
|       - | 1101 | `		{ "throw",      PH7_MOD_PUBLIC, "Throwable $exception", "mixed", vm_builtin_Generator_throw },` |
|       - | 1102 | `		{ "getReturn",  PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_getReturn },` |
|       - | 1103 | `		{ "__destruct", PH7_MOD_PUBLIC, "",                 "",      vm_builtin_Generator_destruct },` |
|       - | 1104 | `	};` |
|       - | 1105 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 1106 | `		{ "__ctx", PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },` |
|       - | 1107 | `	};` |
|       - | 1108 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1109 | `		"Generator", 0, 0, 0,` |
|       - | 1110 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|       - | 1111 | `		0, 0,` |
|       - | 1112 | `		aProp, SX_ARRAYSIZE(aProp)` |
|       - | 1113 | `	};` |
|       - | 1114 | `	ph7_class *pClass;` |
|       - | 1115 | `	ph7_class *pIterator;` |
|    4533 | 1116 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    4533 | 1117 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1118 | `		return rc;` |
|       - | 1119 | `	}` |
|       - | 1120 | ``	/* `implements Iterator` last, for the reason in this function's header. */`` |
|    4533 | 1121 | `	pClass = PH7_VmExtractClass(&(*pVm),"Generator",sizeof("Generator")-1,0,0);` |
|    4533 | 1122 | `	pIterator = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,0,0);` |
|    4533 | 1123 | `	if( pClass == 0 \|\| pIterator == 0 ){` |
|     ! 0 | 1124 | `		return SXERR_NOTFOUND;` |
|       - | 1125 | `	}` |
|    4533 | 1126 | `	return PH7_ClassImplement(pClass,pIterator);` |
|    2269 | 1127 | `}` |
|       - | 1128 | `/*` |
|       - | 1129 | ` * Closure's C-bodied methods.` |
|       - | 1130 | ` *` |
|       - | 1131 | ` * These are the first methods in the engine whose body is a C routine rather than` |
|       - | 1132 | `` * bytecode (VM_FUNC_NATIVE). They were global `__closure_bindTo` /`` |
|       - | 1133 | `` * `__closure_fromCallable` thunks that the prelude's one-line PHP methods forwarded`` |
|       - | 1134 | ` * to — the shape every builtin class had to take before a method could BE C. The` |
|       - | 1135 | ` * class is still declared in the builtin chunk; only these three methods are` |
|       - | 1136 | ` * attached from here, after that chunk has compiled.` |
|       - | 1137 | ` */` |
|    4528 | 1138 | `PH7_PRIVATE sxi32 PH7_VmInstallClosureNative(ph7_vm *pVm)` |
|       5 | 1139 | `{` |
|       - | 1140 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 1141 | `		/* Parameter names are php's own ($newScope, not $scope): this string is the` |
|       - | 1142 | `		 * declaration of record for arity, by-ref positions and — once the reflection` |
|       - | 1143 | `		 * chunk reads a native method's signature the way it already reads a` |
|       - | 1144 | `		 * builtin's — the reported parameter list. */` |
|       - | 1145 | `		{ "bindTo",       PH7_MOD_PUBLIC,` |
|       - | 1146 | `		  "?object $newThis, object\|string\|null $newScope = \"static\"", "?Closure",` |
|       - | 1147 | `		  vm_builtin_Closure_bindTo },` |
|       - | 1148 | `		{ "bind",         PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|       - | 1149 | `		  "Closure $closure, ?object $newThis, object\|string\|null $newScope = \"static\"", "?Closure",` |
|       - | 1150 | `		  vm_builtin_Closure_bindTo },` |
|       - | 1151 | `		{ "fromCallable", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|       - | 1152 | `		  "callable $callback", "Closure",` |
|       - | 1153 | `		  vm_builtin_Closure_fromCallable },` |
|       - | 1154 | `	};` |
|    4533 | 1155 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"Closure",sizeof("Closure")-1,0,0);` |
|       - | 1156 | `	sxu32 n;` |
|    4533 | 1157 | `	if( pClass == 0 ){` |
|     ! 0 | 1158 | `		return SXERR_NOTFOUND;` |
|       - | 1159 | `	}` |
|   18117 | 1160 | `	for( n = 0 ; n < SX_ARRAYSIZE(aMethod) ; n++ ){` |
|   13589 | 1161 | `		sxi32 rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,&aMethod[n],0);` |
|   13589 | 1162 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1163 | `			return rc;` |
|       - | 1164 | `		}` |
|    6797 | 1165 | `	}` |
|    4533 | 1166 | `	return SXRET_OK;` |
|    2269 | 1167 | `}` |
|       - | 1168 | `/*` |
|       - | 1169 | ` * Closure::bindTo($newThis, $scope='static') / Closure::bind($closure, $newThis, $scope='static').` |
|       - | 1170 | ` * Clone the receiver and rebind $this/$scope; returns the new Closure (NULL on a non-Closure` |
|       - | 1171 | ` * receiver, matching PHP's failure mode).` |
|       - | 1172 | ` */` |
|      52 | 1173 | `PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 1174 | `{` |
|      54 | 1175 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1176 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|       - | 1177 | `	ph7_value *pNewThisArg;` |
|       - | 1178 | `	ph7_value *pRecv;` |
|       - | 1179 | `	SyString sScope;` |
|      54 | 1180 | `	const SyString *pScopePtr = 0;` |
|       - | 1181 | `	/* One body, both spellings — as it always was, except the closure now arrives` |
|       - | 1182 | `	 * the way php passes it rather than as a hand-written first argument. Called as` |
|       - | 1183 | `	 * the instance method bindTo(), the receiver IS the closure and the arguments` |
|       - | 1184 | `	 * start at $newThis; called as the static bind(), the closure is argument #1.` |
|       - | 1185 | `	 * Normalizing here is what lets the two share an implementation. */` |
|      54 | 1186 | `	if( PH7_ContextThis(pCtx) ){` |
|      32 | 1187 | `		pRecv = PH7_ContextThisValue(pCtx);` |
|      17 | 1188 | `	}else{` |
|      23 | 1189 | `		if( nArg < 1 ){` |
|     ! 0 | 1190 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1191 | `			return PH7_OK;` |
|       - | 1192 | `		}` |
|      23 | 1193 | `		pRecv = apArg[0];` |
|      23 | 1194 | `		apArg++;` |
|      23 | 1195 | `		nArg--;` |
|       - | 1196 | `	}` |
|      54 | 1197 | `	if( nArg < 1 \|\| !VmValueIsClosure(pVm, pRecv) ){` |
|     ! 0 | 1198 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1199 | `		return PH7_OK;` |
|       - | 1200 | `	}` |
|      54 | 1201 | `	pClosure = (ph7_class_instance *)pRecv->x.pOther;` |
|      54 | 1202 | `	pNewThisArg = apArg[0];` |
|      54 | 1203 | `	if( pNewThisArg->iFlags & MEMOBJ_NULL ){` |
|       9 | 1204 | `		pNewThis = 0;` |
|      50 | 1205 | `	}else if( pNewThisArg->iFlags & MEMOBJ_OBJ ){` |
|      46 | 1206 | `		pNewThis = (ph7_class_instance *)pNewThisArg->x.pOther;` |
|      24 | 1207 | `	}else{` |
|     ! 0 | 1208 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1209 | `			"Closure::bindTo(): Argument #1 ($newThis) must be of type ?object");` |
|       - | 1210 | `	}` |
|      54 | 1211 | `	if( pNewThis ){` |
|       - | 1212 | `		/* php refuses to bind an instance to a static closure: warning + null */` |
|       - | 1213 | `		SyString sAttr;` |
|       - | 1214 | `		ph7_value *pFn;` |
|      46 | 1215 | `		SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|      46 | 1216 | `		pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      46 | 1217 | `		if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){` |
|      46 | 1218 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      46 | 1219 | `			if( pEntry && (((ph7_vm_func *)pEntry->pUserData)->iFlags & VM_FUNC_STATIC_CL) ){` |
|       3 | 1220 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1221 | `					"Cannot bind an instance to a static closure, this will be an error in PHP 9");` |
|       3 | 1222 | `				ph7_result_null(pCtx);` |
|       3 | 1223 | `				return PH7_OK;` |
|       - | 1224 | `			}` |
|      21 | 1225 | `		}` |
|      21 | 1226 | `	}` |
|      52 | 1227 | `	if( VmClosureResolveScope((nArg > 1) ? apArg[1] : 0, &sScope) ){` |
|      37 | 1228 | `		pScopePtr = &sScope;` |
|      18 | 1229 | `	}` |
|      52 | 1230 | `	pClone = PH7_CloneClassInstance(pClosure);` |
|      52 | 1231 | `	if( pClone == 0 ){` |
|     ! 0 | 1232 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1233 | `		return PH7_OK;` |
|       - | 1234 | `	}` |
|      52 | 1235 | `	VmClosureRebind(pClone, pNewThis, pScopePtr);` |
|      52 | 1236 | `	return VmClosureResult(pCtx, pClone);` |
|      28 | 1237 | `}` |
|       - | 1238 | `/*` |
|       - | 1239 | ` * Closure::fromCallable($callable) — normalize any callable value to a Closure (reuses the` |
|       - | 1240 | ` * VmFccWrapValue primitive); idempotent on a Closure; TypeError on a non-callable.` |
|       - | 1241 | ` */` |
|      24 | 1242 | `PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 1243 | `{` |
|      26 | 1244 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1245 | `	ph7_class_instance *pClosure;` |
|      26 | 1246 | `	if( nArg < 1 ){` |
|     ! 0 | 1247 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1248 | `			"Closure::fromCallable() expects exactly 1 argument, 0 given");` |
|       - | 1249 | `	}` |
|      26 | 1250 | `	if( VmValueIsClosure(pVm, apArg[0]) ){` |
|       3 | 1251 | `		ph7_result_value(pCtx, apArg[0]); /* already a Closure: idempotent */` |
|       3 | 1252 | `		return PH7_OK;` |
|       - | 1253 | `	}` |
|      24 | 1254 | `	pClosure = VmFccWrapValue(pVm, apArg[0]);` |
|      24 | 1255 | `	if( pClosure == 0 ){` |
|     ! 0 | 1256 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1257 | `			"Closure::fromCallable(): Argument #1 ($callback) is not a valid callback");` |
|       - | 1258 | `	}` |
|      24 | 1259 | `	return VmClosureResult(pCtx, pClosure);` |
|      14 | 1260 | `}` |
|       - | 1261 | `/*` |
|       - | 1262 | ` * Fiber::suspend($value = null) — static method.` |
|       - | 1263 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|       - | 1264 | ` */` |
|     360 | 1265 | `PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1266 | `{` |
|     365 | 1267 | `	ph7_vm *pVm = pCtx->pVm;` |
|     365 | 1268 | `	if( pVm->pActiveCtx == 0 ){` |
|     ! 0 | 1269 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1270 | `			"Cannot suspend outside of a fiber");` |
|       - | 1271 | `	}` |
|       - | 1272 | `	/* Stage 4 scoped divergence: the trampoline only makes PHP->PHP CALLs` |
|       - | 1273 | `	 * iterative. Every OTHER re-entry runs on a fresh native VmByteCodeExec` |
|       - | 1274 | `	 * activation (nVmExecDepth bumped) that PH7_SUSPEND cannot unwind across` |
|       - | 1275 | `	 * without real coroutine stacks (BYTECODE.md §2.4): a C->PHP callback` |
|       - | 1276 | `	 * (usort/array_map/preg_replace_callback comparator), and — because fibers` |
|       - | 1277 | `	 * use the LEGACY (non-inline) try/catch machinery — a suspend inside a` |
|       - | 1278 | `	 * fiber's catch/finally body, a match/switch arm, or eval()/include()'d` |
|       - | 1279 | `	 * code, all of which run via VmLocalExec. php does all of these via full` |
|       - | 1280 | `	 * native-stack switching; PHL raises a catchable FiberError instead of the` |
|       - | 1281 | `	 * old silent corruption. A suspend in a fiber's try BODY (not catch/finally)` |
|       - | 1282 | `	 * runs in the main dispatch loop and parks normally. A recorded` |
|       - | 1283 | `	 * residual; making the catch/finally case work needs fibers on the inline` |
|       - | 1284 | `	 * try machinery (the generator ROOT C path), a follow-up. */` |
|     365 | 1285 | `	if( pVm->nVmExecDepth != pVm->pActiveCtx->nBodyExecDepth ){` |
|       6 | 1286 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1287 | `			"Cannot suspend across an internal call boundary");` |
|       - | 1288 | `	}` |
|     361 | 1289 | `	if( nArg > 0 ){` |
|     355 | 1290 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|     180 | 1291 | `	}else{` |
|       9 | 1292 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|       - | 1293 | `	}` |
|     361 | 1294 | `	return PH7_SUSPEND;` |
|     185 | 1295 | `}` |
|       - | 1296 | `/*` |
|       - | 1297 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|       - | 1298 | ` * Actual resolution is deferred to start() so that overload selection` |
|       - | 1299 | ` * and closure-environment binding happen with the correct argument context.` |
|       - | 1300 | ` */` |
|     264 | 1301 | `PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1302 | `{` |
|       - | 1303 | `	ph7_class_instance *pThis;` |
|       - | 1304 | `	ph7_value *pAttr;` |
|       - | 1305 | `	SyString sAttrName;` |
|     269 | 1306 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     269 | 1307 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|     269 | 1308 | `	if( nArg < 1 ){` |
|     ! 0 | 1309 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1310 | `			"Fiber::__construct() expects a callable argument");` |
|       - | 1311 | `	}` |
|     269 | 1312 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1313 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1314 | `			"Fiber::__construct(): invalid $this");` |
|       - | 1315 | `	}` |
|     269 | 1316 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|     269 | 1317 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|     ! 0 | 1318 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1319 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|       - | 1320 | `	}` |
|       - | 1321 | `	/* Basic validation: callable must be a string or closure (object) */` |
|     269 | 1322 | `	if( (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|     ! 0 | 1323 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1324 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|       - | 1325 | `	}` |
|       - | 1326 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|     269 | 1327 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     269 | 1328 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     269 | 1329 | `	if( pAttr ){` |
|     269 | 1330 | `		PH7_MemObjStore(apArg[0], pAttr);` |
|     132 | 1331 | `	}` |
|     269 | 1332 | `	return PH7_OK;` |
|     137 | 1333 | `}` |
|       - | 1334 | `/*` |
|       - | 1335 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|       - | 1336 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|       - | 1337 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|       - | 1338 | ` * so that start() can bind it as $this for the closure environment.` |
|       - | 1339 | ` */` |
|     260 | 1340 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|       - | 1341 | `	ph7_class_instance **ppThis)` |
|       5 | 1342 | `{` |
|     265 | 1343 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1344 | `	ph7_value *pCallable;` |
|       - | 1345 | `	SyString sAttrName;` |
|     265 | 1346 | `	*ppThis = 0;` |
|     265 | 1347 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     265 | 1348 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|     265 | 1349 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|     ! 0 | 1350 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|     ! 0 | 1351 | `		return 0;` |
|       - | 1352 | `	}` |
|     265 | 1353 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|       - | 1354 | `		/* String callable — look up in user functions with overload support */` |
|       - | 1355 | `		SyString sName;` |
|       - | 1356 | `		SyHashEntry *pEntry;` |
|       - | 1357 | `		ph7_vm_func *pFunc;` |
|     232 | 1358 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|     232 | 1359 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|     232 | 1360 | `		if( pEntry == 0 ){` |
|     ! 0 | 1361 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|     ! 0 | 1362 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|     ! 0 | 1363 | `			return 0;` |
|       - | 1364 | `		}` |
|     232 | 1365 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     232 | 1366 | `		return pFunc;` |
|     ! 0 | 1367 | `	}else{` |
|      36 | 1368 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|       - | 1369 | `		ph7_class_method *pMethod;` |
|      36 | 1370 | `		if( VmValueIsClosure(pVm, pCallable) ){` |
|       - | 1371 | `			/* A real Closure object: unwrap to its underlying callable name (the single` |
|       - | 1372 | `			 * source of truth, VmClosureUnwrap) and resolve that function. Its captured` |
|       - | 1373 | ``			 * environment (including any `$this`) rides along in the named function's`` |
|       - | 1374 | `			 * aClosureEnv, installed by VmFiberSetupFrame, so *ppThis stays 0. */` |
|       - | 1375 | `			ph7_value sName;` |
|      36 | 1376 | `			SyHashEntry *pEntry = 0;` |
|      36 | 1377 | `			PH7_MemObjInit(pVm, &sName);` |
|      36 | 1378 | `			if( VmClosureUnwrap(pVm, pCallable, &sName) == SXRET_OK ){` |
|      36 | 1379 | `				pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&sName.sBlob), SyBlobLength(&sName.sBlob));` |
|      16 | 1380 | `			}` |
|      36 | 1381 | `			PH7_MemObjRelease(&sName);` |
|      36 | 1382 | `			if( pEntry ){` |
|       - | 1383 | `				/* A BOUND closure parked its $this in the pClosureThis transient` |
|       - | 1384 | `				 * (VmClosureUnwrap): consume it as the fiber's $this — it wins over` |
|       - | 1385 | `				 * the creation-time env capture (VmFiberSetupFrame's skip). *ppThis` |
|       - | 1386 | `				 * is a borrow (the closure's $__this attr keeps the object alive` |
|       - | 1387 | `				 * through $__callable), so drop the parked ref. The scope transient` |
|       - | 1388 | `				 * is cleared alongside: fiber bodies don't model pBoundScope` |
|       - | 1389 | `				 * visibility (recorded residual), and a stale transient would` |
|       - | 1390 | `				 * poison the next OP_CALL's frame. */` |
|      36 | 1391 | `				if( pVm->pClosureThis ){` |
|     ! 0 | 1392 | `					*ppThis = pVm->pClosureThis;` |
|     ! 0 | 1393 | `					PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1394 | `					pVm->pClosureThis = 0;` |
|     ! 0 | 1395 | `				}` |
|      36 | 1396 | `				pVm->pClosureScope = 0;` |
|      36 | 1397 | `				return (ph7_vm_func *)pEntry->pUserData;` |
|       - | 1398 | `			}` |
|     ! 0 | 1399 | `			if( pVm->pClosureThis ){` |
|       - | 1400 | `				/* Failed resolution: drop the parked transient so it neither leaks` |
|       - | 1401 | `				 * nor poisons the next call. */` |
|     ! 0 | 1402 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1403 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 1404 | `			}` |
|     ! 0 | 1405 | `			pVm->pClosureScope = 0;` |
|     ! 0 | 1406 | `			PH7_VmThrowException(pCtx, "FiberError", "Fiber callable closure could not be resolved");` |
|     ! 0 | 1407 | `			return 0;` |
|       - | 1408 | `		}` |
|       - | 1409 | `		/* Object callable — resolve __invoke method */` |
|     ! 0 | 1410 | `		pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 1411 | `			sizeof("__invoke") - 1);` |
|     ! 0 | 1412 | `		if( pMethod == 0 ){` |
|     ! 0 | 1413 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1414 | `				"Fiber callable object has no __invoke method");` |
|     ! 0 | 1415 | `			return 0;` |
|       - | 1416 | `		}` |
|     ! 0 | 1417 | `		*ppThis = pClosure;` |
|     ! 0 | 1418 | `		return &pMethod->sFunc;` |
|       - | 1419 | `	}` |
|     135 | 1420 | `}` |
|       - | 1421 | `/*` |
|       - | 1422 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|       - | 1423 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|       - | 1424 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|       - | 1425 | ` */` |
|       - | 1426 | `/*` |
|       - | 1427 | ` * Enforce one formal parameter's declared type on an argument being installed.` |
|       - | 1428 | ` * THE single implementation of the per-argument check, shared by the` |
|       - | 1429 | ` * generator/fiber initial-frame binder below (band A #2) and both OP_CALL` |
|       - | 1430 | ` * install paths (named-map and positional — they carried two verbatim copies` |
|       - | 1431 | ` * until the §7.1(f) fold): union types via VmCoerceToUnion, class and` |
|       - | 1432 | ` * pseudo types (VmCheckPseudoType + VmResolveTypeClass + instanceof, so` |
|       - | 1433 | ``  * interfaces/abstract classes and self/parent resolve), the bare `object` `` |
|       - | 1434 | ` * hint, and scalars via VmEnforceScalarType (weak-mode coercion in place,` |
|       - | 1435 | ` * strict rejection otherwise) — with the VM_FUNC_ARG_NULLABLE guard letting` |
|       - | 1436 | `` * null through for `?type` and implicit-nullable `Type $x = null` params,`` |
|       - | 1437 | ` * and whole-real materialization on a mask match.` |
|       - | 1438 | ` * Returns SXRET_OK (value possibly coerced) or the VmThrowTypeErrorForArg` |
|       - | 1439 | ` * status for the caller to route — normalized so an INLINE-caught throw` |
|       - | 1440 | ` * (VmThrowInline records only a pc-redirect and reports SXRET_OK) still` |
|       - | 1441 | ` * comes back as PH7_EXCEPTION: the binding must stop; the OP_CALL sites` |
|       - | 1442 | ` * force rc = PH7_EXCEPTION on any non-OK status anyway, and the generator` |
|       - | 1443 | ` * block's PH7_INLINE_RESUME_BREAK consumes the redirect.` |
|       - | 1444 | ` */` |
|     292 | 1445 | `static sxi32 VmGenArgThrowStatus(ph7_vm *pVm, sxi32 rcThrow)` |
|       5 | 1446 | `{` |
|     297 | 1447 | `	if( rcThrow == SXRET_OK && pVm->pInlineInstr ){` |
|     ! 0 | 1448 | `		return PH7_EXCEPTION;` |
|       - | 1449 | `	}` |
|     297 | 1450 | `	return rcThrow;` |
|     151 | 1451 | `}` |
| 1674848 | 1452 | `PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_vm_func_arg *pFormal,` |
|       - | 1453 | `	sxu32 nArgPos, ph7_value *pVal, int bStrict, ph7_class *pSelfHint)` |
|       5 | 1454 | `{` |
| 1674853 | 1455 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|     282 | 1456 | `		if( VmCoerceToUnion(pVm,pVal,&pFormal->aUnionAlts,` |
|     287 | 1457 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,bStrict,pSelfHint) != SXRET_OK ){` |
|       - | 1458 | `			const char *zGiven;` |
|      80 | 1459 | `			const char *zExpected = "union";` |
|       - | 1460 | `			char zBuf[128];` |
|       - | 1461 | `			char zTypeBuf[128];` |
|      80 | 1462 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      49 | 1463 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      57 | 1464 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      10 | 1465 | `				zGiven = "null";` |
|       6 | 1466 | `			}else{` |
|      24 | 1467 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|       - | 1468 | `			}` |
|      80 | 1469 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|     118 | 1470 | `				zExpected = VmHintTextResolved(pVm,&pFormal->sTypeName,pSelfHint,` |
|      38 | 1471 | `					zTypeBuf,sizeof(zTypeBuf));` |
|      38 | 1472 | `			}` |
|     118 | 1473 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      38 | 1474 | `				&pFormal->sName,zExpected,zGiven));` |
|       - | 1475 | `		}` |
|     117 | 1476 | `		return SXRET_OK;` |
|       - | 1477 | `	}` |
| 1674660 | 1478 | `	if( pFormal->nType == 0` |
|  846024 | 1479 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
| 1658127 | 1480 | `		return SXRET_OK;` |
|       - | 1481 | `	}` |
|   16543 | 1482 | `	if( pFormal->nType == SXU32_HIGH ){` |
|       - | 1483 | `		/* Class or pseudo type */` |
|    1661 | 1484 | `		SyString *pName = &pFormal->sClass;` |
|       - | 1485 | `		ph7_class *pClass;` |
|    1661 | 1486 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|    1661 | 1487 | `		if( rcPseudo == 0 ){` |
|       - | 1488 | `			char zTypeBuf[128],zGivenBuf[128];` |
|     132 | 1489 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      32 | 1490 | `				&pFormal->sName,` |
|      64 | 1491 | `				VmClassHintTypeName(pName,0,` |
|      64 | 1492 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      32 | 1493 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1494 | `		}` |
|    1597 | 1495 | `		pClass = 0;` |
|    1597 | 1496 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|       - | 1497 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      88 | 1498 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      21 | 1499 | `				&pFormal->sName,` |
|      42 | 1500 | `				VmClassHintTypeName(pName,pClass,` |
|      42 | 1501 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      21 | 1502 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1503 | `		}` |
|    1555 | 1504 | `		return SXRET_OK;` |
|       - | 1505 | `	}` |
|   14887 | 1506 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|       - | 1507 | `		char zGivenBuf[128];` |
|     161 | 1508 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|      71 | 1509 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       8 | 1510 | `				&pFormal->sName,"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1511 | `		}` |
|     145 | 1512 | `		if( VmEnforceScalarType(pVal,pFormal->nType,bStrict) != SXRET_OK ){` |
|       - | 1513 | `			char zTypeBuf[128];` |
|     140 | 1514 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      45 | 1515 | `				&pFormal->sName,` |
|      45 | 1516 | `				VmScalarTypeName(pFormal->nType,&pFormal->sTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      45 | 1517 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1518 | `		}` |
|      30 | 1519 | `	}else{` |
|       - | 1520 | `		/* Mask matched — an int param accepting a whole-real materializes` |
|       - | 1521 | `		 * it (php: g(1.0) into int $x is int(1)). */` |
|   14731 | 1522 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|       - | 1523 | `	}` |
|   14781 | 1524 | `	return SXRET_OK;` |
|  837825 | 1525 | `}` |
|     642 | 1526 | `PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|       - | 1527 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg,` |
|       - | 1528 | `	int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg)` |
|       5 | 1529 | `{` |
|     647 | 1530 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|       - | 1531 | `	ph7_vm_func_arg *aFormalArg;` |
|       - | 1532 | `	sxu32 nFormal, n;` |
|     647 | 1533 | `	sxu32 nReqGF = 0, nNonVarGF = 0; /* too-few-args watermark (0 = exempt) */` |
|       - | 1534 | `	VmSlot sSlot;` |
|       - | 1535 | `	sxi32 rc;` |
|       - | 1536 | `	/* Install $this for closure/method callables */` |
|     647 | 1537 | `	if( pClosureThis ){` |
|       - | 1538 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      34 | 1539 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      34 | 1540 | `		if( pObj ){` |
|      34 | 1541 | `			pObj->x.pOther = pClosureThis;` |
|      34 | 1542 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      34 | 1543 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      15 | 1544 | `		}` |
|      15 | 1545 | `	}` |
|       - | 1546 | `	/* Install static variables */` |
|     647 | 1547 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|       - | 1548 | `		ph7_vm_func_static_var *aStatic;` |
|       - | 1549 | `		ph7_value *pVal;` |
|     ! 0 | 1550 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|     ! 0 | 1551 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|     ! 0 | 1552 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|     ! 0 | 1553 | `			if( pVal ){` |
|     ! 0 | 1554 | `				sSlot.pUserData = 0;` |
|     ! 0 | 1555 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|     ! 0 | 1556 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|     ! 0 | 1557 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|     ! 0 | 1558 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|     ! 0 | 1559 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);` |
|     ! 0 | 1560 | `				}` |
|     ! 0 | 1561 | `			}` |
|     ! 0 | 1562 | `		}` |
|     ! 0 | 1563 | `	}` |
|       - | 1564 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|     647 | 1565 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     647 | 1566 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       - | 1567 | `	/* Actual call arity for func_num_args()/func_get_args() (band A #4) */` |
|     647 | 1568 | `	pExecCtx->pFrame->nActualArgs = nArg;` |
|       - | 1569 | `	{` |
|       - | 1570 | `		/* Too-few-arguments watermark — checked per formal INSIDE the install` |
|       - | 1571 | `		 * loop below, after the passed args' type checks, matching php's` |
|       - | 1572 | `		 * RECV order (a type error on a passed argument beats the count` |
|       - | 1573 | `		 * error). Generators throw at the g(...) call site (message embeds` |
|       - | 1574 | `		 * it); fibers at Fiber::start() (php omits the call-site segment).` |
|       - | 1575 | `		 * Prelude builtins are included — VmThrowBuiltinTooFewArgs words them` |
|       - | 1576 | `		 * as php words an internal callable. */` |
|     647 | 1577 | `	nReqGF = VmFuncRequiredArgCount(pFunc,&nNonVarGF);` |
|       - | 1578 | `	}` |
|     725 | 1579 | `	for( n = 0; n < nFormal; n++ ){` |
|       - | 1580 | `		ph7_value *pObj;` |
|     107 | 1581 | `		if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       - | 1582 | `			/* Variadic formal: collect this and every remaining actual into a` |
|       - | 1583 | `			 * fresh array (php semantics — pre-fix nothing collected here, so a` |
|       - | 1584 | ``			 * `function g(int ...$xs)` generator saw a bare scalar in $xs).`` |
|       - | 1585 | `			 * Per-element checks mirror OP_CALL's variadic install: union via` |
|       - | 1586 | ``			 * the shared helper, scalar coerce/TypeError, `object` hint; a`` |
|       - | 1587 | `			 * class-typed variadic element is (like OP_CALL) not checked. */` |
|       5 | 1588 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       5 | 1589 | `			if( pObj ){` |
|       - | 1590 | `				sxu32 nVariadicIdx;` |
|       - | 1591 | `				ph7_hashmap *pMap;` |
|       - | 1592 | `				sxu32 k;` |
|       5 | 1593 | `				PH7_MemObjToHashmap(pObj);` |
|       - | 1594 | `				/* Capture the slot index now: PH7_HashmapInsert can reallocate` |
|       - | 1595 | `				 * pVm->aMemObj and dangle pObj (same hazard as OP_CALL's path). */` |
|       5 | 1596 | `				nVariadicIdx = pObj->nIdx;` |
|       5 | 1597 | `				pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      11 | 1598 | `				for( k = n; k < (sxu32)nArg; k++ ){` |
|       9 | 1599 | `					if( ((aFormalArg[n].iFlags & VM_FUNC_ARG_UNION)` |
|       7 | 1600 | `					   \|\| (aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH)) ){` |
|       7 | 1601 | `						rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[k],bStrict,pSelfHint);` |
|       7 | 1602 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1603 | `							return rc;` |
|       - | 1604 | `						}` |
|       3 | 1605 | `					}` |
|       7 | 1606 | `					PH7_HashmapInsert(pMap,0,apArg[k]);` |
|       4 | 1607 | `				}` |
|       5 | 1608 | `				sSlot.nIdx = nVariadicIdx;` |
|       5 | 1609 | `				sSlot.pUserData = 0;` |
|       5 | 1610 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       2 | 1611 | `			}` |
|       5 | 1612 | `			break; /* All remaining actuals consumed */` |
|       - | 1613 | `		}` |
|     103 | 1614 | `		if( n < (sxu32)nArg ){` |
|       - | 1615 | `			/* Argument provided — install with declared-type enforcement.` |
|       - | 1616 | `			 * php binds and type-checks generator arguments EAGERLY at the` |
|       - | 1617 | `			 * g(...) call site (and fiber arguments at Fiber::start()), so the` |
|       - | 1618 | `			 * enforcement lives here, mirroring the OP_CALL install path via` |
|       - | 1619 | `			 * VmEnforceArgType (TypeError on mismatch, weak coercion in` |
|       - | 1620 | `			 * place otherwise) instead of the old silent xCast. A variadic` |
|       - | 1621 | `			 * formal collects as-is (no per-element declared-type model). */` |
|      95 | 1622 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      95 | 1623 | `			if( pObj ){` |
|      95 | 1624 | `				PH7_MemObjStore(apArg[n], pObj);` |
|      95 | 1625 | `				if( (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      95 | 1626 | `					rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,pObj,bStrict,pSelfHint);` |
|      95 | 1627 | `					if( rc != SXRET_OK ){` |
|      18 | 1628 | `						return rc;` |
|       - | 1629 | `					}` |
|      37 | 1630 | `				}` |
|      79 | 1631 | `				sSlot.nIdx = pObj->nIdx;` |
|      79 | 1632 | `				sSlot.pUserData = 0;` |
|      79 | 1633 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      42 | 1634 | `			}` |
|      47 | 1635 | `		}else if( n < nReqGF ){` |
|       - | 1636 | `			/* Required formal with no actual: php's ArgumentCountError, at` |
|       - | 1637 | `			 * this point in the install order (see the watermark comment). */` |
|       7 | 1638 | `			return VmGenArgThrowStatus(pVm,` |
|       4 | 1639 | `				(pFunc->iFlags & VM_FUNC_INTERNAL)` |
|     ! 0 | 1640 | `					? VmThrowBuiltinTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|     ! 0 | 1641 | `						(sxu32)nArg,nReqGF,SySetUsed(&pFunc->aArgs))` |
|       6 | 1642 | `					: VmThrowTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|       2 | 1643 | `						(sxu32)nArg,nReqGF,nNonVarGF,bCallSiteInMsg));` |
|       6 | 1644 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       - | 1645 | `			/* Default value */` |
|       6 | 1646 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       6 | 1647 | `			if( pObj ){` |
|       6 | 1648 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);` |
|       6 | 1649 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1650 | `					return rc;` |
|       - | 1651 | `				}` |
|       - | 1652 | `` 				/* A null default on an implicitly-nullable `Type $x = null` `` |
|       - | 1653 | `				 * param must stay null (php); only non-null defaults keep the` |
|       - | 1654 | `				 * legacy shaping cast. */` |
|       4 | 1655 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       6 | 1656 | `				 && !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|       3 | 1657 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|     ! 0 | 1658 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|     ! 0 | 1659 | `						if( xCast ){` |
|     ! 0 | 1660 | `							xCast(pObj);` |
|     ! 0 | 1661 | `						}` |
|     ! 0 | 1662 | `					}else{` |
|       - | 1663 | `						/* Mask matched — a const-indirected whole-real default` |
|       - | 1664 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|       3 | 1665 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|       - | 1666 | `					}` |
|       1 | 1667 | `				}` |
|       6 | 1668 | `				sSlot.nIdx = pObj->nIdx;` |
|       6 | 1669 | `				sSlot.pUserData = 0;` |
|       6 | 1670 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       2 | 1671 | `			}` |
|       2 | 1672 | `		}` |
|      44 | 1673 | `	}` |
|       - | 1674 | `	/* Install closure environment (captured variables) */` |
|     627 | 1675 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1676 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|       - | 1677 | `		ph7_value *pValue;` |
|       - | 1678 | `		sxu32 iEnv;` |
|      49 | 1679 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     105 | 1680 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|      61 | 1681 | `			pEnv = &aEnv[iEnv];` |
|      61 | 1682 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|      40 | 1683 | `				continue;` |
|       - | 1684 | `			}` |
|      20 | 1685 | `			if( pClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       5 | 1686 | `			 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|       - | 1687 | `				/* An explicit bound $this (bindTo/bind/call) wins over the` |
|       - | 1688 | `				 * creation-time captured $this, php-exact (mirrors OP_CALL). */` |
|       3 | 1689 | `				continue;` |
|       - | 1690 | `			}` |
|      21 | 1691 | `			if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|       - | 1692 | `				/* Captured by reference: link the name to the shared slot` |
|       - | 1693 | `				 * (no copy), mirroring the OP_CALL env install. */` |
|       5 | 1694 | `				if( SyHashGet(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|       7 | 1695 | `					SyHashInsert(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),` |
|       4 | 1696 | `						SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|       2 | 1697 | `				}` |
|       5 | 1698 | `				continue;` |
|       - | 1699 | `			}` |
|      17 | 1700 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|      17 | 1701 | `			if( pValue == 0 ){` |
|     ! 0 | 1702 | `				continue;` |
|       - | 1703 | `			}` |
|      17 | 1704 | `			PH7_MemObjRelease(pValue);` |
|      17 | 1705 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|      10 | 1706 | `		}` |
|      22 | 1707 | `	}` |
|     627 | 1708 | `	return SXRET_OK;` |
|     326 | 1709 | `}` |
|       - | 1710 | `/*` |
|       - | 1711 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|       - | 1712 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|       - | 1713 | ` *` |
|       - | 1714 | ` * As a native VARIADIC method the arguments arrive directly as (nArg, apArg);` |
|       - | 1715 | ` * the prelude used to hand them over as a single func_get_args() array, which` |
|       - | 1716 | ` * this had to walk and snapshot out of pVm->aMemObj.` |
|       - | 1717 | ` */` |
|     262 | 1718 | `PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1719 | `{` |
|     267 | 1720 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1721 | `	ph7_class_instance *pThis;` |
|       - | 1722 | `	ph7_class_instance *pClosureThis;` |
|       - | 1723 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 1724 | `	ph7_vm_func *pFunc;` |
|       - | 1725 | `	ph7_value sResult;` |
|       - | 1726 | `	ph7_value *pCtxAttr;` |
|       - | 1727 | `	SyString sAttrName;` |
|       - | 1728 | `	sxi32 rc;` |
|     267 | 1729 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     267 | 1730 | `	if( pRecv == 0 \|\| (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1731 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|       - | 1732 | `	}` |
|     267 | 1733 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 1734 | `	/* Check if already started (has a __ctx) */` |
|     267 | 1735 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     267 | 1736 | `	if( pExecCtx != 0 ){` |
|       3 | 1737 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1738 | `			"Cannot start a fiber that has already been started");` |
|       - | 1739 | `	}` |
|       - | 1740 | `	/* Resolve callable */` |
|     265 | 1741 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|     265 | 1742 | `	if( pFunc == 0 ){` |
|     ! 0 | 1743 | `		return PH7_EXCEPTION;` |
|       - | 1744 | `	}` |
|       - | 1745 | `	/* Create execution context now that we know the function */` |
|     265 | 1746 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|     265 | 1747 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 1748 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1749 | `			"Fiber::start(): out of memory");` |
|       - | 1750 | `	}` |
|       - | 1751 | `	/* Store context in $this->__ctx */` |
|     265 | 1752 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     265 | 1753 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     265 | 1754 | `	if( pCtxAttr ){` |
|     265 | 1755 | `		pCtxAttr->x.pOther = pExecCtx;` |
|     265 | 1756 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     130 | 1757 | `	}` |
|       - | 1758 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|       - | 1759 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|       - | 1760 | `	 * into the fiber's frame, not the caller's. */` |
|     265 | 1761 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|     265 | 1762 | `	pVm->pFrame = pExecCtx->pFrame;` |
|       - | 1763 | `	/* Unpack the args array and install into the frame */` |
|       - | 1764 | `	{` |
|       - | 1765 | `		/* The arguments are this call's own operand-stack slots, so they can be` |
|       - | 1766 | `		 * handed to the frame setup as-is. The old form had to snapshot them out of` |
|       - | 1767 | `		 * pVm->aMemObj first, because they arrived as a func_get_args() hashmap` |
|       - | 1768 | `		 * whose element values live in that set — and VmFiberSetupFrame reserves` |
|       - | 1769 | `		 * memory objects (VmExtractMemObj) before reading its arguments, which can` |
|       - | 1770 | `		 * reallocate the set and dangle a raw pool pointer. Operand slots do not` |
|       - | 1771 | `		 * move, so the copy is gone with the array that made it necessary. */` |
|     265 | 1772 | `		ph7_value **apValues = (nArg > 0) ? apArg : 0;` |
|     265 | 1773 | `		int nActual = nArg;` |
|     265 | 1774 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues,` |
|       - | 1775 | `			0 /* weak-mode arg binding, like call_user_func */, 0,` |
|       - | 1776 | `			FALSE/*Fiber::start(): php omits the call-site segment*/);` |
|       - | 1777 | `		/* Nothing to free: apValues aliases the operand stack now, it is not a` |
|       - | 1778 | `		 * buffer this function allocated. */` |
|       - | 1779 | `	}` |
|       - | 1780 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|     265 | 1781 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|     265 | 1782 | `	pExecCtx->pFrame->pParent = 0;` |
|     265 | 1783 | `	if( rc != SXRET_OK ){` |
|       - | 1784 | `		/* Propagate the real status: a declared-type TypeError from the arg` |
|       - | 1785 | `		 * install (band A #2) must stay catchable, not become an abort. */` |
|       5 | 1786 | `		return (rc == PH7_EXCEPTION) ? PH7_EXCEPTION : PH7_ABORT;` |
|       - | 1787 | `	}` |
|     261 | 1788 | `	PH7_MemObjInit(pVm, &sResult);` |
|     261 | 1789 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|     261 | 1790 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 1791 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1792 | `		return PH7_ABORT;` |
|       - | 1793 | `	}` |
|     261 | 1794 | `	if( rc == PH7_EXCEPTION ){` |
|       6 | 1795 | `		PH7_MemObjRelease(&sResult);` |
|       6 | 1796 | `		return PH7_EXCEPTION;` |
|       - | 1797 | `	}` |
|     257 | 1798 | `	ph7_result_value(pCtx, &sResult);` |
|     257 | 1799 | `	PH7_MemObjRelease(&sResult);` |
|     257 | 1800 | `	return PH7_OK;` |
|     136 | 1801 | `}` |
|       - | 1802 | `/*` |
|       - | 1803 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|       - | 1804 | ` */` |
|     148 | 1805 | `PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1806 | `{` |
|     153 | 1807 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1808 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 1809 | `	ph7_value sResult;` |
|       - | 1810 | `	ph7_value *pResumeVal;` |
|       - | 1811 | `	sxi32 rc;` |
|     153 | 1812 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     153 | 1813 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|     153 | 1814 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1815 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|     ! 0 | 1816 | `		return PH7_OK;` |
|       - | 1817 | `	}` |
|     153 | 1818 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     153 | 1819 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 1820 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|     ! 0 | 1821 | `		return PH7_OK;` |
|       - | 1822 | `	}` |
|     153 | 1823 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       3 | 1824 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1825 | `			"Cannot resume a fiber that is not suspended");` |
|       - | 1826 | `	}` |
|     151 | 1827 | `	pResumeVal = (nArg > 0) ? apArg[0] : 0;` |
|     151 | 1828 | `	PH7_MemObjInit(pVm, &sResult);` |
|     151 | 1829 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|     151 | 1830 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 1831 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1832 | `		return PH7_ABORT;` |
|       - | 1833 | `	}` |
|     151 | 1834 | `	if( rc == PH7_EXCEPTION ){` |
|       3 | 1835 | `		PH7_MemObjRelease(&sResult);` |
|       3 | 1836 | `		return PH7_EXCEPTION;` |
|       - | 1837 | `	}` |
|     149 | 1838 | `	ph7_result_value(pCtx, &sResult);` |
|     149 | 1839 | `	PH7_MemObjRelease(&sResult);` |
|     149 | 1840 | `	return PH7_OK;` |
|      79 | 1841 | `}` |
|       - | 1842 | `/*` |
|       - | 1843 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|       - | 1844 | ` */` |
|      20 | 1845 | `PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1846 | `{` |
|      25 | 1847 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1848 | `	ph7_exec_ctx *pExecCtx;` |
|      25 | 1849 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      10 | 1850 | `	SXUNUSED(apArg);` |
|      10 | 1851 | `	SXUNUSED(nArg);` |
|      25 | 1852 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      25 | 1853 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1854 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1855 | `		return PH7_OK;` |
|       - | 1856 | `	}` |
|      25 | 1857 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|      25 | 1858 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 1859 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1860 | `		return PH7_OK;` |
|       - | 1861 | `	}` |
|      25 | 1862 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|     ! 0 | 1863 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 1864 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1865 | `				"Cannot get fiber return value: The fiber has not been started");` |
|       - | 1866 | `		}` |
|     ! 0 | 1867 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1868 | `			"Cannot get fiber return value: The fiber has not returned");` |
|       - | 1869 | `	}` |
|      25 | 1870 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|      25 | 1871 | `	return PH7_OK;` |
|      15 | 1872 | `}` |
|       - | 1873 | `/*` |
|       - | 1874 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|       - | 1875 | ` */` |
|       8 | 1876 | `PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 1877 | `{` |
|       - | 1878 | `	ph7_exec_ctx *pExecCtx;` |
|      10 | 1879 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       4 | 1880 | `	SXUNUSED(apArg);` |
|       4 | 1881 | `	SXUNUSED(nArg);` |
|      10 | 1882 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      10 | 1883 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|      10 | 1884 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|      10 | 1885 | `	return PH7_OK;` |
|       6 | 1886 | `}` |
|     ! 0 | 1887 | `PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     ! 0 | 1888 | `{` |
|       - | 1889 | `	ph7_exec_ctx *pExecCtx;` |
|     ! 0 | 1890 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     ! 0 | 1891 | `	SXUNUSED(apArg);` |
|     ! 0 | 1892 | `	SXUNUSED(nArg);` |
|     ! 0 | 1893 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     ! 0 | 1894 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|     ! 0 | 1895 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|     ! 0 | 1896 | `	return PH7_OK;` |
|     ! 0 | 1897 | `}` |
|     110 | 1898 | `PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 1899 | `{` |
|       - | 1900 | `	ph7_exec_ctx *pExecCtx;` |
|     113 | 1901 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      55 | 1902 | `	SXUNUSED(apArg);` |
|      55 | 1903 | `	SXUNUSED(nArg);` |
|     113 | 1904 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     113 | 1905 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|     113 | 1906 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|     113 | 1907 | `	return PH7_OK;` |
|      58 | 1908 | `}` |
|      10 | 1909 | `PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 1910 | `{` |
|       - | 1911 | `	ph7_exec_ctx *pExecCtx;` |
|      12 | 1912 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       5 | 1913 | `	SXUNUSED(apArg);` |
|       5 | 1914 | `	SXUNUSED(nArg);` |
|      12 | 1915 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      12 | 1916 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|      12 | 1917 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|      12 | 1918 | `	return PH7_OK;` |
|       7 | 1919 | `}` |
|       - | 1920 | `/*` |
|       - | 1921 | ` * Fiber->__destruct() — clean up the execution context.` |
|       - | 1922 | ` */` |
|     222 | 1923 | `PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 1924 | `{` |
|     225 | 1925 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1926 | `	ph7_exec_ctx *pExecCtx;` |
|     225 | 1927 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     111 | 1928 | `	SXUNUSED(apArg);` |
|     111 | 1929 | `	SXUNUSED(nArg);` |
|     225 | 1930 | `	if( pRecv == 0 ){` |
|     ! 0 | 1931 | `		return PH7_OK;` |
|       - | 1932 | `	}` |
|     225 | 1933 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     225 | 1934 | `	if( pExecCtx ){` |
|     221 | 1935 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|       - | 1936 | `		/* Clear the attribute so double-free is prevented */` |
|     221 | 1937 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|     221 | 1938 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 1939 | `			SyString sAttrName;` |
|       - | 1940 | `			ph7_value *pAttr;` |
|     221 | 1941 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     221 | 1942 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     221 | 1943 | `			if( pAttr ){` |
|     221 | 1944 | `				PH7_MemObjRelease(pAttr);` |
|     109 | 1945 | `			}` |
|     109 | 1946 | `		}` |
|     109 | 1947 | `	}` |
|     225 | 1948 | `	return PH7_OK;` |
|     114 | 1949 | `}` |
|       - | 1950 | `/* ======================== Fiber Public API Helpers ======================== */` |
|     ! 0 | 1951 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|     ! 0 | 1952 | `{` |
|       - | 1953 | `	ph7_class_instance *pThis;` |
|     ! 0 | 1954 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|     ! 0 | 1955 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 | 1956 | `	return pThis->pClass == pVm->pFiberClass;` |
|     ! 0 | 1957 | `}` |
|     ! 0 | 1958 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 1959 | `{` |
|       - | 1960 | `	ph7_class_instance *pThis;` |
|     ! 0 | 1961 | `	ph7_class_instance *pClosureThis = 0;` |
|       - | 1962 | `	ph7_exec_ctx *pCtx;` |
|       - | 1963 | `	ph7_vm_func *pFunc;` |
|       - | 1964 | `	ph7_value *pCallable;` |
|       - | 1965 | `	ph7_value *pCtxAttr;` |
|       - | 1966 | `	SyString sAttrName;` |
|       - | 1967 | `	sxi32 rc;` |
|       - | 1968 | `	/* Must not already be started */` |
|     ! 0 | 1969 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 1970 | `	if( pCtx != 0 ){` |
|     ! 0 | 1971 | `		return SXERR_INVALID;` |
|       - | 1972 | `	}` |
|     ! 0 | 1973 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1974 | `		return SXERR_INVALID;` |
|       - | 1975 | `	}` |
|     ! 0 | 1976 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|       - | 1977 | `	/* Get the callable */` |
|     ! 0 | 1978 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     ! 0 | 1979 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 1980 | `	if( pCallable == 0 ){` |
|     ! 0 | 1981 | `		return SXERR_INVALID;` |
|       - | 1982 | `	}` |
|       - | 1983 | `	/* Resolve callable */` |
|     ! 0 | 1984 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|       - | 1985 | `		SyString sName;` |
|       - | 1986 | `		SyHashEntry *pEntry;` |
|     ! 0 | 1987 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|     ! 0 | 1988 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|     ! 0 | 1989 | `		if( pEntry == 0 ){` |
|     ! 0 | 1990 | `			return SXERR_NOTFOUND;` |
|       - | 1991 | `		}` |
|     ! 0 | 1992 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     ! 0 | 1993 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 1994 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|     ! 0 | 1995 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 1996 | `			sizeof("__invoke") - 1);` |
|     ! 0 | 1997 | `		if( pMethod == 0 ){` |
|     ! 0 | 1998 | `			return SXERR_INVALID;` |
|       - | 1999 | `		}` |
|     ! 0 | 2000 | `		pClosureThis = pClosure;` |
|     ! 0 | 2001 | `		pFunc = &pMethod->sFunc;` |
|     ! 0 | 2002 | `	}else{` |
|     ! 0 | 2003 | `		return SXERR_INVALID;` |
|       - | 2004 | `	}` |
|       - | 2005 | `	/* Create context */` |
|     ! 0 | 2006 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|     ! 0 | 2007 | `	if( pCtx == 0 ){` |
|     ! 0 | 2008 | `		return SXERR_MEM;` |
|       - | 2009 | `	}` |
|       - | 2010 | `	/* Store in __ctx */` |
|     ! 0 | 2011 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     ! 0 | 2012 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 2013 | `	if( pCtxAttr ){` |
|     ! 0 | 2014 | `		pCtxAttr->x.pOther = pCtx;` |
|     ! 0 | 2015 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     ! 0 | 2016 | `	}` |
|       - | 2017 | `	/* Set up frame with args */` |
|     ! 0 | 2018 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     ! 0 | 2019 | `	pVm->pFrame = pCtx->pFrame;` |
|     ! 0 | 2020 | `	rc = VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg,` |
|       - | 2021 | `		0 /* weak-mode arg binding (embedder entry) */, 0,` |
|       - | 2022 | `		FALSE/*embedder entry: no userland call site*/);` |
|     ! 0 | 2023 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|     ! 0 | 2024 | `	pCtx->pFrame->pParent = 0;` |
|     ! 0 | 2025 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2026 | `		return rc;` |
|       - | 2027 | `	}` |
|     ! 0 | 2028 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|     ! 0 | 2029 | `}` |
|     ! 0 | 2030 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 2031 | `{` |
|     ! 0 | 2032 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2033 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|     ! 0 | 2034 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|     ! 0 | 2035 | `}` |
|     ! 0 | 2036 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2037 | `{` |
|     ! 0 | 2038 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2039 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|     ! 0 | 2040 | `}` |
|     ! 0 | 2041 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2042 | `{` |
|     ! 0 | 2043 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2044 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|     ! 0 | 2045 | `}` |
|     ! 0 | 2046 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2047 | `{` |
|     ! 0 | 2048 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2049 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|     ! 0 | 2050 | `	return &pCtx->sRetValue;` |
|     ! 0 | 2051 | `}` |
|       - | 2052 | `/* ======================== Generator Infrastructure ======================== */` |
|       - | 2053 | `/*` |
|       - | 2054 | ` * Allocate a new generator wrapper around an execution context.` |
|       - | 2055 | ` */` |
|     382 | 2056 | `PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 | 2057 | `{` |
|       - | 2058 | `	ph7_generator *pGen;` |
|     387 | 2059 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|     387 | 2060 | `	if( pGen == 0 ){` |
|     ! 0 | 2061 | `		return 0;` |
|       - | 2062 | `	}` |
|     387 | 2063 | `	SyZero(pGen, sizeof(ph7_generator));` |
|     387 | 2064 | `	pGen->pCtx = pCtx;` |
|     387 | 2065 | `	pGen->iImplicitKey = 0;` |
|     387 | 2066 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|     387 | 2067 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|       - | 2068 | `	/* Link the generator back to the exec context */` |
|     387 | 2069 | `	pCtx->pPrivate = pGen;` |
|     387 | 2070 | `	return pGen;` |
|     196 | 2071 | `}` |
|       - | 2072 | `/*` |
|       - | 2073 | ` * Release a generator and its execution context.` |
|       - | 2074 | ` */` |
|     264 | 2075 | `PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|       5 | 2076 | `{` |
|     269 | 2077 | `	if( pGen == 0 ){` |
|     ! 0 | 2078 | `		return;` |
|       - | 2079 | `	}` |
|     269 | 2080 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|     269 | 2081 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|     269 | 2082 | `	if( pGen->pCtx ){` |
|     269 | 2083 | `		pGen->pCtx->pPrivate = 0;` |
|     269 | 2084 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|     269 | 2085 | `		pGen->pCtx = 0;` |
|     132 | 2086 | `	}` |
|     269 | 2087 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|     137 | 2088 | `}` |
|       - | 2089 | `/*` |
|       - | 2090 | ` * Extract ph7_generator from a Generator class instance.` |
|       - | 2091 | ` */` |
|    4474 | 2092 | `PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|       5 | 2093 | `{` |
|       - | 2094 | `	ph7_class_instance *pThis;` |
|       - | 2095 | `	SyString sAttr;` |
|       - | 2096 | `	ph7_value *pAttr;` |
|    4479 | 2097 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2098 | `		return 0;` |
|       - | 2099 | `	}` |
|    4479 | 2100 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|    4479 | 2101 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|     ! 0 | 2102 | `		return 0;` |
|       - | 2103 | `	}` |
|    4479 | 2104 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    4479 | 2105 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    4479 | 2106 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     ! 0 | 2107 | `		return 0;` |
|       - | 2108 | `	}` |
|    4479 | 2109 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    2242 | 2110 | `}` |
|       - | 2111 | `/*` |
|       - | 2112 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|       - | 2113 | ` */` |
|     240 | 2114 | `PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2115 | `{` |
|       - | 2116 | `	ph7_generator *pGen;` |
|       - | 2117 | `	sxi32 rc;` |
|     245 | 2118 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     120 | 2119 | `	SXUNUSED(apArg);` |
|     120 | 2120 | `	SXUNUSED(nArg);` |
|     245 | 2121 | `	if( pRecv == 0 ) return PH7_OK;` |
|     245 | 2122 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     245 | 2123 | `	if( pGen == 0 ) return PH7_OK;` |
|     245 | 2124 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     245 | 2125 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     245 | 2126 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     245 | 2127 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     117 | 2128 | `	}` |
|     239 | 2129 | `	return PH7_OK;` |
|     125 | 2130 | `}` |
|       - | 2131 | `/*` |
|       - | 2132 | ` * Generator::valid() — true if suspended at a yield point.` |
|       - | 2133 | ` */` |
|    1258 | 2134 | `PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2135 | `{` |
|       - | 2136 | `	ph7_generator *pGen;` |
|    1263 | 2137 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     629 | 2138 | `	SXUNUSED(apArg);` |
|     629 | 2139 | `	SXUNUSED(nArg);` |
|    1263 | 2140 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|    1263 | 2141 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1263 | 2142 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|    1263 | 2143 | `	return PH7_OK;` |
|     634 | 2144 | `}` |
|       - | 2145 | `/*` |
|       - | 2146 | ` * Generator::current() — return the last yielded value.` |
|       - | 2147 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 2148 | ` */` |
|    1212 | 2149 | `PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2150 | `{` |
|       - | 2151 | `	ph7_generator *pGen;` |
|       - | 2152 | `	sxi32 rc;` |
|    1217 | 2153 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     606 | 2154 | `	SXUNUSED(apArg);` |
|     606 | 2155 | `	SXUNUSED(nArg);` |
|    1217 | 2156 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1217 | 2157 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1217 | 2158 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1217 | 2159 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     119 | 2160 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     119 | 2161 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     119 | 2162 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      57 | 2163 | `	}` |
|    1217 | 2164 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|    1215 | 2165 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|     610 | 2166 | `	}else{` |
|       3 | 2167 | `		ph7_result_null(pCtx);` |
|       - | 2168 | `	}` |
|    1217 | 2169 | `	return PH7_OK;` |
|     611 | 2170 | `}` |
|       - | 2171 | `/*` |
|       - | 2172 | ` * Generator::key() — return the last yielded key.` |
|       - | 2173 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 2174 | ` */` |
|     228 | 2175 | `PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2176 | `{` |
|       - | 2177 | `	ph7_generator *pGen;` |
|       - | 2178 | `	sxi32 rc;` |
|     233 | 2179 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     114 | 2180 | `	SXUNUSED(apArg);` |
|     114 | 2181 | `	SXUNUSED(nArg);` |
|     233 | 2182 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     233 | 2183 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     233 | 2184 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     233 | 2185 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2186 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     ! 0 | 2187 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     ! 0 | 2188 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     ! 0 | 2189 | `	}` |
|     233 | 2190 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     233 | 2191 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|     119 | 2192 | `	}else{` |
|     ! 0 | 2193 | `		ph7_result_null(pCtx);` |
|       - | 2194 | `	}` |
|     233 | 2195 | `	return PH7_OK;` |
|     119 | 2196 | `}` |
|       - | 2197 | `/*` |
|       - | 2198 | ` * Generator::next() — advance to the next yield point.` |
|       - | 2199 | ` */` |
|    1012 | 2200 | `PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2201 | `{` |
|       - | 2202 | `	ph7_generator *pGen;` |
|       - | 2203 | `	sxi32 rc;` |
|    1017 | 2204 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     506 | 2205 | `	SXUNUSED(apArg);` |
|     506 | 2206 | `	SXUNUSED(nArg);` |
|    1017 | 2207 | `	if( pRecv == 0 ) return PH7_OK;` |
|    1017 | 2208 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1017 | 2209 | `	if( pGen == 0 ) return PH7_OK;` |
|    1017 | 2210 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2211 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|    1017 | 2212 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|    1017 | 2213 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|     511 | 2214 | `	}else{` |
|     ! 0 | 2215 | `		return PH7_OK;` |
|       - | 2216 | `	}` |
|    1017 | 2217 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|    1015 | 2218 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|    1005 | 2219 | `	return PH7_OK;` |
|     511 | 2220 | `}` |
|       - | 2221 | `/*` |
|       - | 2222 | ` * Generator::send($value) — resume and send a value into the generator.` |
|       - | 2223 | ` */` |
|     102 | 2224 | `PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2225 | `{` |
|       - | 2226 | `	ph7_generator *pGen;` |
|       - | 2227 | `	ph7_value *pSendVal;` |
|       - | 2228 | `	sxi32 rc;` |
|     107 | 2229 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     107 | 2230 | `	if( pRecv == 0 ) return PH7_OK;` |
|     107 | 2231 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     107 | 2232 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     107 | 2233 | `	pSendVal = (nArg > 0) ? apArg[0] : 0;` |
|     107 | 2234 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       - | 2235 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|     ! 0 | 2236 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     107 | 2237 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     107 | 2238 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|      56 | 2239 | `	}else{` |
|     ! 0 | 2240 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2241 | `		return PH7_OK;` |
|       - | 2242 | `	}` |
|     107 | 2243 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     107 | 2244 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     104 | 2245 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      98 | 2246 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      51 | 2247 | `	}else{` |
|       7 | 2248 | `		ph7_result_null(pCtx);` |
|       - | 2249 | `	}` |
|     104 | 2250 | `	return PH7_OK;` |
|      56 | 2251 | `}` |
|       - | 2252 | `/*` |
|       - | 2253 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|       - | 2254 | ` *` |
|       - | 2255 | ` * PHP semantics: the exception is injected AT the suspended yield point so the` |
|       - | 2256 | ` * generator's OWN try/catch (if any wraps the yield) can handle it and the body` |
|       - | 2257 | ` * resumes after the try; otherwise it propagates to the throw() caller and the` |
|       - | 2258 | ` * generator closes. We implement this by resuming the body with a pending` |
|       - | 2259 | ` * injection (pCtx->pInjected) that the VM loop raises in the body's own frame via` |
|       - | 2260 | ` * the existing OP_THROW / ROOT B resume route — no exception frame is` |
|       - | 2261 | ` * reconstructed on pVm->pFrame, so the return/finally unwind path is untouched.` |
|       - | 2262 | ` * A never-started generator is first run to its first yield, then injected there;` |
|       - | 2263 | ` * a finished/closed generator has no suspend point, so the exception is simply` |
|       - | 2264 | ` * propagated to the caller (its return value stays readable via getReturn()).` |
|       - | 2265 | ` *` |
|       - | 2266 | ` * PHL does not enforce the Throwable parameter hint (interface/class hints are` |
|       - | 2267 | ` * not checked on call), so the Throwable validation — and its PHP TypeError —` |
|       - | 2268 | ` * are done here.` |
|       - | 2269 | ` */` |
|      70 | 2270 | `PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 2271 | `{` |
|       - | 2272 | `	ph7_generator *pGen;` |
|       - | 2273 | `	ph7_class_instance *pInj;` |
|       - | 2274 | `	ph7_class *pThrowable;` |
|       - | 2275 | `	VmFrame *pFrame;` |
|       - | 2276 | `	sxi32 rc;` |
|      74 | 2277 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      74 | 2278 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      74 | 2279 | `	if( nArg < 1 ) return PH7_OK;` |
|       - | 2280 | `	/* Argument #1 must be a Throwable; otherwise PHP raises a TypeError naming` |
|       - | 2281 | `	 * the given type (class name for objects, "null"/"string"/... for scalars). */` |
|      74 | 2282 | `	pThrowable = PH7_VmExtractClass(pCtx->pVm, "Throwable", sizeof("Throwable")-1, 0, 0);` |
|      70 | 2283 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|      71 | 2284 | `	 \|\| (pThrowable && !PH7_VmInstanceOf(((ph7_class_instance *)apArg[0]->x.pOther)->pClass, pThrowable)) ){` |
|       - | 2285 | `		char zCls[128];` |
|      10 | 2286 | `		const char *zGiven = VmValueGivenName(apArg[0], zCls, sizeof(zCls));` |
|      14 | 2287 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       4 | 2288 | `			"Generator::throw(): Argument #1 ($exception) must be of type Throwable, %s given", zGiven);` |
|       - | 2289 | `	}` |
|      66 | 2290 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      66 | 2291 | `	if( pGen == 0 ) return PH7_OK;` |
|       - | 2292 | `	/* PHP forbids resuming/throwing into a generator that is currently executing. */` |
|      66 | 2293 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       3 | 2294 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 2295 | `			"Cannot resume an already running generator");` |
|       - | 2296 | `	}` |
|       - | 2297 | `	/* Hold a reference to the injected instance for the whole operation: the VM loop` |
|       - | 2298 | `	 * (inject path) or VmThrowException (propagate path) may run catch blocks that bind` |
|       - | 2299 | `	 * and later release it. Dropped on every return path below. */` |
|      64 | 2300 | `	pInj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      64 | 2301 | `	pInj->iRef++;` |
|       - | 2302 | `	/* A never-started generator runs to its first yield, then the exception is injected` |
|       - | 2303 | `	 * there (PHP). Start it first; if it suspended at a yield, fall through to inject; if` |
|       - | 2304 | `	 * it ran to completion without yielding, drop through to the propagate path below. */` |
|      64 | 2305 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       5 | 2306 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       5 | 2307 | `		if( rc == PH7_ABORT ){ PH7_ClassInstanceUnref(pInj); return PH7_ABORT; }` |
|       5 | 2308 | `		if( rc == PH7_EXCEPTION ){ PH7_ClassInstanceUnref(pInj); return PH7_EXCEPTION; }` |
|       2 | 2309 | `	}` |
|      64 | 2310 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       - | 2311 | `		/* Inject at the suspended yield: the resume loop raises it in the body's own` |
|       - | 2312 | `		 * frame so the generator's try/catch can catch it and resume (path 2). */` |
|      58 | 2313 | `		pGen->pCtx->pInjected = pInj;   /* borrowed; ref held here across the resume */` |
|      58 | 2314 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       - | 2315 | `		/* Normally the inject was consumed (cleared) at VmByteCodeExec entry; clear it` |
|       - | 2316 | `		 * here too for the path where VmResumeCtx bails BEFORE entering the loop (e.g. the` |
|       - | 2317 | `		 * recursion-depth fatal), so no dangling borrowed pointer survives the Unref. */` |
|      58 | 2318 | `		pGen->pCtx->pInjected = 0;` |
|      58 | 2319 | `		PH7_ClassInstanceUnref(pInj);` |
|      58 | 2320 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      58 | 2321 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       - | 2322 | `		/* Caught inside the generator and it resumed: return the next yielded value (or` |
|       - | 2323 | `		 * null if it then completed) — symmetric with Generator::send(). */` |
|      46 | 2324 | `		if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      44 | 2325 | `			ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      24 | 2326 | `		}else{` |
|       3 | 2327 | `			ph7_result_null(pCtx);` |
|       - | 2328 | `		}` |
|      46 | 2329 | `		return PH7_OK;` |
|       - | 2330 | `	}` |
|       - | 2331 | `	/* COMPLETED/CLOSED (incl. a CREATED generator that ran to completion without a` |
|       - | 2332 | `	 * yield): no suspend point to inject at. Propagate the real object to the throw()` |
|       - | 2333 | `	 * caller through the normal dispatch path (class/message/trace preserved); its` |
|       - | 2334 | `	 * terminal state — and thus getReturn() — is left intact. */` |
|       8 | 2335 | `	pFrame = pCtx->pVm->pFrame;` |
|       8 | 2336 | `	if( pFrame ){` |
|       8 | 2337 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       8 | 2338 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       3 | 2339 | `	}` |
|       8 | 2340 | `	rc = VmThrowException(pCtx->pVm, pInj);` |
|       8 | 2341 | `	PH7_ClassInstanceUnref(pInj);` |
|       8 | 2342 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2343 | `		return PH7_ABORT;` |
|       - | 2344 | `	}` |
|       8 | 2345 | `	return PH7_EXCEPTION;` |
|      39 | 2346 | `}` |
|       - | 2347 | `/*` |
|       - | 2348 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|       - | 2349 | ` */` |
|      24 | 2350 | `PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2351 | `{` |
|       - | 2352 | `	ph7_generator *pGen;` |
|      29 | 2353 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      12 | 2354 | `	SXUNUSED(apArg);` |
|      12 | 2355 | `	SXUNUSED(nArg);` |
|      29 | 2356 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      29 | 2357 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      29 | 2358 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      29 | 2359 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|     ! 0 | 2360 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 2361 | `			"Cannot get return value of a generator that hasn't returned");` |
|       - | 2362 | `	}` |
|      29 | 2363 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|      29 | 2364 | `	return PH7_OK;` |
|      17 | 2365 | `}` |
|       - | 2366 | `/*` |
|       - | 2367 | ` * Generator::__destruct() — clean up.` |
|       - | 2368 | ` */` |
|     248 | 2369 | `PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2370 | `{` |
|       - | 2371 | `	ph7_generator *pGen;` |
|     253 | 2372 | `	sxi32 rcClose = SXRET_OK;` |
|     253 | 2373 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     124 | 2374 | `	SXUNUSED(apArg);` |
|     124 | 2375 | `	SXUNUSED(nArg);` |
|     253 | 2376 | `	if( pRecv == 0 ) return PH7_OK;` |
|     253 | 2377 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     253 | 2378 | `	if( pGen ){` |
|       - | 2379 | `` 		/* A generator abandoned before it completes still runs its pending `finally` `` |
|       - | 2380 | `		 * blocks (PHP runs them at generator close/GC). Drive them before teardown. */` |
|     253 | 2381 | `		if( pGen->pCtx ){` |
|     253 | 2382 | `			rcClose = VmCloseCtx(pCtx->pVm, pGen->pCtx);` |
|     124 | 2383 | `		}` |
|     253 | 2384 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|     253 | 2385 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|     253 | 2386 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 2387 | `			SyString sAttrName;` |
|       - | 2388 | `			ph7_value *pAttr;` |
|     253 | 2389 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     253 | 2390 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     253 | 2391 | `			if( pAttr ){` |
|     253 | 2392 | `				PH7_MemObjRelease(pAttr);` |
|     124 | 2393 | `			}` |
|     124 | 2394 | `		}` |
|     124 | 2395 | `	}` |
|       - | 2396 | `	/* Surface an abort/exception raised by a finally that ran during close. */` |
|     253 | 2397 | `	if( rcClose == PH7_ABORT ) return PH7_ABORT;` |
|     253 | 2398 | `	if( rcClose == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     253 | 2399 | `	return PH7_OK;` |
|     129 | 2400 | `}` |
|       - | 2401 | `/* ======================== End Generator Infrastructure ======================== */` |
|       - | 2402 | `/* ======================== End Fiber Infrastructure ======================== */` |
|       - | 2403 |  |
