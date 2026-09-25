# src/ph7/vm_exec_ctx.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1302/1561 lines (83.41%)

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
|     694 |   20 | `PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|       5 |   21 | `{` |
|       - |   22 | `	ph7_exec_ctx *pCtx;` |
|       - |   23 | `	ph7_value *pStack;` |
|       - |   24 | `	VmFrame *pFrame;` |
|     699 |   25 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|     699 |   26 | `	if( pCtx == 0 ){` |
|     ! 0 |   27 | `		return 0;` |
|       - |   28 | `	}` |
|     699 |   29 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|     699 |   30 | `	pCtx->pVm = pVm;` |
|     699 |   31 | `	pCtx->pFunc = pFunc;` |
|     699 |   32 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|     699 |   33 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|     699 |   34 | `	pCtx->pc = 0;` |
|     699 |   35 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|     699 |   36 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|     699 |   37 | `	PH7_MemObjInit(pVm, &pCtx->sDelegate);` |
|       - |   38 | `	/* Container for this body's own exception handlers while suspended (borrowed` |
|       - |   39 | `	 * ph7_exception* pointers — never freed here, owned by the compiled func). */` |
|     699 |   40 | `	SySetInit(&pCtx->aSavedException, &pVm->sAllocator, sizeof(ph7_exception *));` |
|       - |   41 | `	/* ROOT C: this body's own pending finally actions while suspended. */` |
|     699 |   42 | `	SySetInit(&pCtx->aSavedFinally, &pVm->sAllocator, sizeof(VmFinallyAction));` |
|     699 |   43 | `	pCtx->nFinallyBase = 0;` |
|       - |   44 | `	/* Stage 4: this coroutine's own aSelf entries (self::/static:: class context` |
|       - |   45 | `	 * pushed by nested method calls still open at suspend) parked while suspended,` |
|       - |   46 | `	 * so they don't pollute the resumer's aSelf. Borrowed ph7_class* pointers. */` |
|     699 |   47 | `	SySetInit(&pCtx->aSavedSelf, &pVm->sAllocator, sizeof(ph7_class *));` |
|     699 |   48 | `	pCtx->nSelfBase = 0;` |
|       - |   49 | `	/* Caller slots this body's by-reference parameters alias (see the struct). */` |
|     699 |   50 | `	SySetInit(&pCtx->aByRefArg, &pVm->sAllocator, sizeof(sxu32));` |
|     699 |   51 | `	pCtx->pParkedSegment = 0;` |
|     699 |   52 | `	pCtx->nBodyExecDepth = 0;` |
|       - |   53 | `	/* Allocate a private operand stack */` |
|     699 |   54 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|     699 |   55 | `	if( pStack == 0 ){` |
|     ! 0 |   56 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   57 | `		return 0;` |
|       - |   58 | `	}` |
|     699 |   59 | `	pCtx->pStack = pStack;` |
|     699 |   60 | `	pCtx->nStackCap = SySetUsed(&pFunc->aByteCode) + VM_STACK_GUARD; /* grows with pStack on OP_SPREAD */` |
|     699 |   61 | `	pCtx->nStackOrig = pCtx->nStackCap; /* fixed original — headroom reference across resumes */` |
|       - |   62 | `	/* Create a detached frame for the fiber */` |
|     699 |   63 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|     699 |   64 | `	if( pFrame == 0 ){` |
|     ! 0 |   65 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|     ! 0 |   66 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   67 | `		return 0;` |
|       - |   68 | `	}` |
|     699 |   69 | `	pCtx->pFrame = pFrame;` |
|     699 |   70 | `	return pCtx;` |
|     352 |   71 | `}` |
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
|    5052 |   95 | `static void VmParkStackSlice(SySet *pFrom, SySet *pSaved, sxu32 nBase)` |
|       5 |   96 | `{` |
|    5057 |   97 | `	sxu32 nUsed = SySetUsed(pFrom);` |
|    5057 |   98 | `	if( nUsed > nBase ){` |
|     231 |   99 | `		const char *aBase = (const char *)SySetBasePtr(pFrom);` |
|       - |  100 | `		sxu32 i;` |
|     463 |  101 | `		for( i = nBase; i < nUsed; i++ ){` |
|     237 |  102 | `			SySetPut(pSaved, (const void *)(aBase + i * pFrom->eSize));` |
|     121 |  103 | `		}` |
|     231 |  104 | `		SySetTruncate(pFrom, nBase);` |
|     113 |  105 | `	}` |
|    5057 |  106 | `}` |
|    4170 |  107 | `static void VmRestoreStackSlice(SySet *pTo, SySet *pSaved)` |
|       5 |  108 | `{` |
|    4175 |  109 | `	sxu32 i, n = SySetUsed(pSaved);` |
|    4175 |  110 | `	if( n > 0 ){` |
|     219 |  111 | `		const char *aSaved = (const char *)SySetBasePtr(pSaved);` |
|     439 |  112 | `		for( i = 0; i < n; i++ ){` |
|     225 |  113 | `			SySetPut(pTo, (const void *)(aSaved + i * pSaved->eSize));` |
|     115 |  114 | `		}` |
|     219 |  115 | `		SySetReset(pSaved);` |
|     107 |  116 | `	}` |
|    4175 |  117 | `}` |
|    1684 |  118 | `static void VmParkCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  119 | `{` |
|    1689 |  120 | `	VmParkStackSlice(&pVm->aException, &pCtx->aSavedException, pCtx->nExceptionBase);` |
|    1689 |  121 | `	VmParkStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally, pCtx->nFinallyBase);` |
|    1689 |  122 | `	VmParkStackSlice(&pVm->aSelf, &pCtx->aSavedSelf, pCtx->nSelfBase);` |
|    1689 |  123 | `}` |
|    1390 |  124 | `static void VmRestoreCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  125 | `{` |
|    1395 |  126 | `	VmRestoreStackSlice(&pVm->aException, &pCtx->aSavedException);` |
|    1395 |  127 | `	VmRestoreStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally);` |
|    1395 |  128 | `	VmRestoreStackSlice(&pVm->aSelf, &pCtx->aSavedSelf);` |
|    1395 |  129 | `}` |
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
|    1742 |  143 | `static void VmFreeSuspendedExceptionFrames(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  144 | `{` |
|    1959 |  145 | `	while( pVm->pFrame != pCtx->pFrame && (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     217 |  146 | `		VmLeaveFrame(&(*pVm));` |
|       5 |  147 | `	}` |
|    1747 |  148 | `}` |
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
|    1684 |  161 | `static void VmSuspendCtxDetach(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  162 | `{` |
|    1689 |  163 | `	if( pCtx->pParkedSegment == 0 ){` |
|    1387 |  164 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     696 |  165 | `	}else{` |
|       - |  166 | `		/* The parked records' push-time accounting (one nRecursionDepth++ each,` |
|       - |  167 | `		 * plus VmCallFinish's aSelf pop, which never ran) leaves the segment` |
|       - |  168 | `		 * counted as active while the fiber is suspended — deactivate it. aSelf` |
|       - |  169 | `		 * is parked wholesale below (base-relative), so drop only the depth. */` |
|     306 |  170 | `		pVm->nRecursionDepth -= ((VmParkedSegment *)pCtx->pParkedSegment)->nRecords;` |
|       - |  171 | `	}` |
|    1689 |  172 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|    1689 |  173 | `	pCtx->pFrame->pParent = 0;` |
|    1689 |  174 | `	VmParkCtxState(pVm, pCtx);` |
|    1689 |  175 | `	if( pResult ){` |
|     367 |  176 | `		PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|     181 |  177 | `	}` |
|    1689 |  178 | `}` |
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
|    2044 |  189 | `static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx)` |
|       5 |  190 | `{` |
|    1242 |  191 | `	return ((pCtx->pFunc->iFlags & VM_FUNC_GENERATOR) == 0 && VmFuncHasReturnType(pCtx->pFunc))` |
|    1237 |  192 | `		? pCtx->pFunc : 0;` |
|       5 |  193 | `}` |
|       - |  194 | `/*` |
|       - |  195 | ` * Common epilogue for VmStartCtx / VmResumeCtx once the body run returns rc:` |
|       - |  196 | ` * restore the previous active context, then park on suspend or detach the` |
|       - |  197 | ` * coroutine frame and record the terminal state. On normal completion the value` |
|       - |  198 | ` * belongs to getReturn() only — start()/resume() return the NEXT suspend value,` |
|       - |  199 | ` * which is null at completion (php parity), so pResult is left at its` |
|       - |  200 | ` * caller-initialized null.` |
|       - |  201 | ` */` |
|    2044 |  202 | `static sxi32 VmFinishCtxRun(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_exec_ctx *pOldCtx,` |
|       - |  203 | `	sxi32 rc, ph7_value *pResult)` |
|       5 |  204 | `{` |
|    2049 |  205 | `	pVm->pActiveCtx = pOldCtx;` |
|    2049 |  206 | `	if( rc == PH7_SUSPEND ){` |
|       - |  207 | `		/* Handles a deep RE-suspend too (a resumed segment parking a fresh one):` |
|       - |  208 | `		 * VmSuspendCtxDetach deactivates the new segment's recursion accounting,` |
|       - |  209 | `		 * parks its aSelf, and skips VmFreeSuspendedExceptionFrames for a segment` |
|       - |  210 | `		 * so it can't free the still-live parked try wrappers. */` |
|    1689 |  211 | `		VmSuspendCtxDetach(pVm, pCtx, pResult);` |
|    1689 |  212 | `		return SXRET_OK;` |
|       - |  213 | `	}` |
|       - |  214 | ``	/* A finally entered via the throw redirect whose `return` short-circuited`` |
|       - |  215 | `	 * OP_END_FINALLY leaves the try's transparent wrapper ABOVE the body frame —` |
|       - |  216 | `	 * the detach below would then be skipped and the wrapper (plus the body` |
|       - |  217 | `	 * frame) leak into the RESUMER's frame chain, so the next try at that scope` |
|       - |  218 | `	 * records the wrong owner frame and its caught throw silently unwinds the` |
|       - |  219 | `	 * script. Free trailing exception wrappers exactly like the suspend path. */` |
|     365 |  220 | `	if( pCtx->pParkedSegment == 0 ){` |
|     365 |  221 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     180 |  222 | `	}` |
|       - |  223 | `	/* Detach the coroutine frame from the live chain, unless a deeper unwind` |
|       - |  224 | `	 * already moved pVm->pFrame off it. */` |
|     365 |  225 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|     365 |  226 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|     365 |  227 | `		pCtx->pFrame->pParent = 0;` |
|     180 |  228 | `	}` |
|     365 |  229 | `	if( rc == PH7_ABORT ){` |
|       3 |  230 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       3 |  231 | `		return PH7_ABORT;` |
|       - |  232 | `	}` |
|     363 |  233 | `	if( rc == PH7_EXCEPTION ){` |
|      47 |  234 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      47 |  235 | `		return PH7_EXCEPTION;` |
|       - |  236 | `	}` |
|     321 |  237 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|     321 |  238 | `	return SXRET_OK;` |
|    1027 |  239 | `}` |
|       - |  240 | `/*` |
|       - |  241 | ` * Start executing a fiber context for the first time.` |
|       - |  242 | ` */` |
|     654 |  243 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  244 | `{` |
|       - |  245 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  246 | `	sxi32 rc;` |
|     659 |  247 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|     ! 0 |  248 | `		return SXERR_INVALID;` |
|       - |  249 | `	}` |
|       - |  250 | `	/* A fiber/generator start is a native VmByteCodeExec re-entry, bounded by` |
|       - |  251 | `	 * nMaxNativeDepth. Reject HERE, before attaching the frame / mutating VM` |
|       - |  252 | `	 * state, so the abort is clean — the wrapper's own check fires only after` |
|       - |  253 | `	 * this function has spliced the coroutine into the frame chain, which its` |
|       - |  254 | `	 * post-exec detach cannot fully unwind. The PHP call-depth cap belongs to` |
|       - |  255 | `	 * OP_CALL only (BYTECODE.md stage 5). */` |
|     659 |  256 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  257 | `		return VmNativeNestingFatal(pVm);` |
|       - |  258 | `	}` |
|       - |  259 | `	/* Attach the fiber's frame to the VM frame chain */` |
|     659 |  260 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     659 |  261 | `	pVm->pFrame = pCtx->pFrame;` |
|       - |  262 | `	/* Save and set the active context */` |
|     659 |  263 | `	pOldCtx = pVm->pActiveCtx;` |
|     659 |  264 | `	pVm->pActiveCtx = pCtx;` |
|     659 |  265 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|     659 |  266 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|     659 |  267 | `	pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|     659 |  268 | `	pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|       - |  269 | `	/* Native depth the body runs at (the VmByteCodeExec wrapper bumps +1): a` |
|       - |  270 | `	 * Fiber::suspend() at a deeper depth is inside a C->PHP callback and gets a` |
|       - |  271 | `	 * FiberError instead of parking across the native frame (stage 4). */` |
|     659 |  272 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1;` |
|       - |  273 | `	/* Execute from the beginning (no parked segment). An OP_SPREAD in the body may` |
|       - |  274 | `	 * realloc pCtx->pStack — pass &pCtx->pStack / &pCtx->nStackCap so the grown` |
|       - |  275 | `	 * buffer + capacity persist for the next resume and for ctx teardown. */` |
|     986 |  276 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     327 |  277 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|     327 |  278 | `		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|     659 |  279 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     332 |  280 | `}` |
|       - |  281 | `/*` |
|       - |  282 | ` * Resume a suspended fiber context.` |
|       - |  283 | ` */` |
|    1390 |  284 | `PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|       5 |  285 | `{` |
|       - |  286 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  287 | `	VmParkedSegment *pSeg;` |
|       - |  288 | `	sxi32 rc;` |
|    1395 |  289 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|     ! 0 |  290 | `		return SXERR_INVALID;` |
|       - |  291 | `	}` |
|       - |  292 | `	/* A resume is a native VmByteCodeExec re-entry, bounded by nMaxNativeDepth.` |
|       - |  293 | `	 * Reject HERE, before re-attaching the (possibly deep) parked segment to the` |
|       - |  294 | `	 * frame chain and re-adding its nRecords to nRecursionDepth — a wrapper-level` |
|       - |  295 | `	 * abort past those mutations would leave pVm->pFrame pointing into the parked` |
|       - |  296 | `	 * callee and the depth accounting un-reverted. The PHP call-depth cap is` |
|       - |  297 | `	 * OP_CALL-only (BYTECODE.md stage 5). */` |
|    1395 |  298 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  299 | `		return VmNativeNestingFatal(pVm);` |
|       - |  300 | `	}` |
|       - |  301 | `	/* Push the resume value onto the SUSPENDED activation's operand stack so it` |
|       - |  302 | `	 * appears as Fiber::suspend()'s return value. For a deep suspend (stage 4)` |
|       - |  303 | `	 * that stack is the innermost callee's — parked in the segment — not the` |
|       - |  304 | `	 * body's. nTos was saved one below the return-value slot. */` |
|       - |  305 | `	{` |
|       - |  306 | `		ph7_value *pResumeStack;` |
|    1395 |  307 | `		pSeg = (VmParkedSegment *)pCtx->pParkedSegment;` |
|    1395 |  308 | `		pResumeStack = pSeg ? pSeg->sState.pStack : pCtx->pStack;` |
|    1395 |  309 | `		if( pResumeValue ){` |
|     267 |  310 | `			PH7_MemObjStore(pResumeValue, &pResumeStack[pCtx->nTos + 1]);` |
|     136 |  311 | `		}else{` |
|    1133 |  312 | `			PH7_MemObjRelease(&pResumeStack[pCtx->nTos + 1]);` |
|       - |  313 | `		}` |
|    1395 |  314 | `		pCtx->nTos++;` |
|       - |  315 | `		/* Refresh the caller-depth base and re-publish this body's own exception` |
|       - |  316 | `		 * handlers on top of pVm->aException at that depth, so the resumed body's` |
|       - |  317 | `		 * try/catch and finally-drain bound line up (see VmByteCodeExec's base` |
|       - |  318 | `		 * override). Must run before VmByteCodeExec recaptures its local base. */` |
|    1395 |  319 | `		pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|    1395 |  320 | `		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|    1395 |  321 | `		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|    1395 |  322 | `		VmRestoreCtxState(pVm, pCtx);` |
|    1395 |  323 | `		if( pSeg ){` |
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
|    1395 |  358 | `		pCtx->pFrame->pParent = pVm->pFrame;` |
|    1395 |  359 | `		pVm->pFrame = pSeg ? pSeg->pTopFrame : pCtx->pFrame;` |
|       - |  360 | `	}` |
|       - |  361 | `	/* The segment (if any) is handed to the body invocation explicitly below; it` |
|       - |  362 | `	 * is no longer part of the suspended ctx state once resume owns it. */` |
|    1395 |  363 | `	pCtx->pParkedSegment = 0;` |
|       - |  364 | `	/* Save and set the active context */` |
|    1395 |  365 | `	pOldCtx = pVm->pActiveCtx;` |
|    1395 |  366 | `	pVm->pActiveCtx = pCtx;` |
|    1395 |  367 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|    1395 |  368 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1; /* see VmStartCtx */` |
|       - |  369 | `	/* Resume execution from saved PC. pSeg, when non-NULL, makes this body` |
|       - |  370 | `	 * re-enter inside the innermost parked callee (deep fiber resume, stage 4). */` |
|    2090 |  371 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     695 |  372 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|     695 |  373 | `		VmCtxEnforceRetFunc(pCtx), FALSE, pSeg, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|    1395 |  374 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     700 |  375 | `}` |
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
|     710 |  443 | `static void VmFreeDetachedFrame(ph7_vm *pVm, VmFrame *pFrame)` |
|       5 |  444 | `{` |
|       - |  445 | `	VmSlot *aSlot;` |
|       - |  446 | `	sxu32 n;` |
|     715 |  447 | `	if( pFrame == 0 ){` |
|     ! 0 |  448 | `		return;` |
|       - |  449 | `	}` |
|       - |  450 | `	/* Remove local references FIRST, then free the locals nothing else holds — the` |
|       - |  451 | `	 * order and the holder test VmLeaveFrame explains. */` |
|     715 |  452 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|    1335 |  453 | `	for( n = 0; n < SySetUsed(&pFrame->sRef); ++n ){` |
|     625 |  454 | `		PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|     315 |  455 | `	}` |
|       - |  456 | `	/* Free local variables */` |
|     715 |  457 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|    1319 |  458 | `	for( n = 0; n < SySetUsed(&pFrame->sLocal); ++n ){` |
|     609 |  459 | `		if( PH7_VmSlotHolderCount(pVm, aSlot[n].nIdx) > 0 ){` |
|     ! 0 |  460 | `			continue;` |
|       - |  461 | `		}` |
|     609 |  462 | `		PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|     307 |  463 | `	}` |
|     715 |  464 | `	SyHashRelease(&pFrame->hVar);` |
|     715 |  465 | `	SySetRelease(&pFrame->sArg);` |
|     715 |  466 | `	SySetRelease(&pFrame->sLocal);` |
|     715 |  467 | `	SySetRelease(&pFrame->sRef);` |
|     715 |  468 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|       - |  469 | `	/* Drop a resume target pointing at this detached frame before we free it (ROOT B). */` |
|     715 |  470 | `	VmDropResumeTarget(pVm,pFrame);` |
|     715 |  471 | `	SyMemBackendPoolFree(&pVm->sAllocator, pFrame);` |
|     360 |  472 | `}` |
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
|     510 |  515 | `PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  516 | `{` |
|     515 |  517 | `	if( pCtx == 0 ){` |
|     ! 0 |  518 | `		return;` |
|       - |  519 | `	}` |
|     515 |  520 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       - |  521 | `		/* Cannot destroy a fiber that is currently executing */` |
|     ! 0 |  522 | `		return;` |
|       - |  523 | `	}` |
|     515 |  524 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       - |  525 | `	/* Release values */` |
|     515 |  526 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|     515 |  527 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|     515 |  528 | `	PH7_MemObjRelease(&pCtx->sDelegate);` |
|       - |  529 | `	/* Stage 2b: the parked entries are per-activation clones now — free them` |
|       - |  530 | `	 * (an abandoned suspended coroutine is their last holder). */` |
|     515 |  531 | `	VmExcReleaseAll(pVm,&pCtx->aSavedException);` |
|     515 |  532 | `	SySetRelease(&pCtx->aSavedException);` |
|       - |  533 | `	/* ROOT C: a generator abandoned while suspended inside a finally may carry parked` |
|       - |  534 | `	 * finally actions holding owned values/refs (a RETURN's sRet, a RETHROW's pExc).` |
|       - |  535 | `	 * Release them so the abandon path leaks nothing. */` |
|       - |  536 | `	{` |
|     515 |  537 | `		sxu32 n = SySetUsed(&pCtx->aSavedFinally);` |
|     515 |  538 | `		if( n > 0 ){` |
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
|     515 |  549 | `		SySetRelease(&pCtx->aSavedFinally);` |
|       - |  550 | `	}` |
|       - |  551 | `	/* Stage 4: parked aSelf entries are borrowed class pointers — just free the set. */` |
|     515 |  552 | `	SySetRelease(&pCtx->aSavedSelf);` |
|       - |  553 | `	/* Free a parked deep-suspend segment (stage 4): the fiber was abandoned while` |
|       - |  554 | `	 * suspended inside a nested call, so its record chain / frames / operand` |
|       - |  555 | `	 * stacks are still alive and only this holder references them. Must run` |
|       - |  556 | `	 * before the body frame/stack below (they are the segment's floor). */` |
|     515 |  557 | `	if( pCtx->pParkedSegment ){` |
|     201 |  558 | `		VmFreeParkedSegment(pVm, pCtx, (VmParkedSegment *)pCtx->pParkedSegment);` |
|     201 |  559 | `		pCtx->pParkedSegment = 0;` |
|     100 |  560 | `	}` |
|       - |  561 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|     515 |  562 | `	if( pCtx->pFrame ){` |
|     515 |  563 | `		VmFreeDetachedFrame(pVm, pCtx->pFrame);` |
|     515 |  564 | `		pCtx->pFrame = 0;` |
|     255 |  565 | `	}` |
|       - |  566 | `	/* A by-reference parameter aliased a CALLER's slot; the frame teardown above just` |
|       - |  567 | `	 * dropped this body's name for it. Release it if nothing is left holding it — the` |
|       - |  568 | `	 * caller may already be gone (it skipped the slot precisely because this frame` |
|       - |  569 | `	 * held it), in which case this is its last holder. Runs after the frame so the` |
|       - |  570 | `	 * body's own row is out of the count. */` |
|       - |  571 | `	{` |
|       - |  572 | `		sxu32 n;` |
|     515 |  573 | `		sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pCtx->aByRefArg);` |
|     531 |  574 | `		for( n = 0; n < SySetUsed(&pCtx->aByRefArg); n++ ){` |
|      18 |  575 | `			PH7_VmReleaseUnheldSlot(pVm, aIdx[n]);` |
|      10 |  576 | `		}` |
|     515 |  577 | `		SySetRelease(&pCtx->aByRefArg);` |
|       - |  578 | `	}` |
|       - |  579 | `	/* Release individual operand stack entries (decrement refcounts,` |
|       - |  580 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|       - |  581 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|     515 |  582 | `	if( pCtx->pStack ){` |
|     515 |  583 | `		if( pCtx->nTos >= 0 ){` |
|     441 |  584 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|     879 |  585 | `			while( pTos >= pCtx->pStack ){` |
|     443 |  586 | `				PH7_MemObjRelease(pTos);` |
|     443 |  587 | `				pTos--;` |
|       5 |  588 | `			}` |
|     218 |  589 | `		}` |
|     515 |  590 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|     515 |  591 | `		pCtx->pStack = 0;` |
|     255 |  592 | `	}` |
|       - |  593 | `	/* Free the context itself */` |
|     515 |  594 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     260 |  595 | `}` |
|       - |  596 | `/*` |
|       - |  597 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|       - |  598 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|       - |  599 | ` */` |
|     898 |  600 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|       5 |  601 | `{` |
|       - |  602 | `	ph7_class_instance *pThis;` |
|       - |  603 | `	SyString sAttr;` |
|       - |  604 | `	ph7_value *pAttr;` |
|     903 |  605 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 |  606 | `		return 0;` |
|       - |  607 | `	}` |
|     903 |  608 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|     903 |  609 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|     ! 0 |  610 | `		return 0;` |
|       - |  611 | `	}` |
|     903 |  612 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     903 |  613 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     903 |  614 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     331 |  615 | `		return 0;` |
|       - |  616 | `	}` |
|     577 |  617 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|     454 |  618 | `}` |
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
| 5876877 |  633 | `PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm, ph7_value *pVal)` |
|       5 |  634 | `{` |
|       - |  635 | `	ph7_class_instance *pThis;` |
|       - |  636 | `	/* Flag test first: a non-object call target (the hot common case) bails before any` |
|       - |  637 | `	 * pVm dereference; pClosureClass==0 is a one-time pre-init concern, so it goes last. */` |
| 5876882 |  638 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
| 5634300 |  639 | `		return 0;` |
|       - |  640 | `	}` |
|  242587 |  641 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       - |  642 | `	/* Closure is final, so an exact class match is correct (no subclasses possible). */` |
|  242587 |  643 | `	return pThis->pClass == pVm->pClosureClass;` |
| 2940536 |  644 | `}` |
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
|   14810 |  655 | `PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm, ph7_value *pVal, ph7_value *pOut)` |
|       5 |  656 | `{` |
|       - |  657 | `	ph7_class_instance *pThis;` |
|       - |  658 | `	ph7_value *pFn;` |
|       - |  659 | `	SyString sAttr;` |
|   14815 |  660 | `	if( !VmValueIsClosure(pVm, pVal) ){` |
|     ! 0 |  661 | `		return SXERR_NOTFOUND;` |
|       - |  662 | `	}` |
|   14815 |  663 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|   14815 |  664 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|   14815 |  665 | `	pFn = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|   14815 |  666 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 |  667 | `		return SXERR_NOTFOUND; /* malformed/uninitialised closure */` |
|       - |  668 | `	}` |
|       - |  669 | `	/* Only a bound/static first-class callable (rare) carries $__this/$__scope; the` |
|       - |  670 | `	 * VM_INSTANCE_FCC_BOUND flag (set by VmCreateClosure) keeps the hot plain-closure path` |
|       - |  671 | `	 * to the single $__fn lookup above instead of two extra attribute lookups per dispatch. */` |
|   14815 |  672 | `	if( pThis->iFlags & VM_INSTANCE_FCC_BOUND ){` |
|       - |  673 | `		ph7_value *pBound, *pScope;` |
|       - |  674 | `		int bBoundObj, bScope;` |
|     252 |  675 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     252 |  676 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     252 |  677 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     252 |  678 | `		pScope = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     252 |  679 | `		bBoundObj = pBound && (pBound->iFlags & MEMOBJ_OBJ);` |
|     252 |  680 | `		bScope = pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0;` |
|     252 |  681 | `		if( bBoundObj \|\| bScope ){` |
|       - |  682 | `			/* Method/static first-class callable -> [ target, "method" ] array callable. */` |
|     252 |  683 | `			if( pThis->iFlags & VM_INSTANCE_FCC_INVOKE_OBJ ){` |
|       - |  684 | `				/* Closure::fromCallable($obj): the engine named __invoke, so this` |
|       - |  685 | `				 * dispatch is the engine's own and a non-public one still runs. */` |
|      10 |  686 | `				pVm->bMagicDispatch = 1;` |
|       4 |  687 | `			}` |
|       - |  688 | `			ph7_hashmap *pMap;` |
|       - |  689 | `			ph7_value sTarget, sMeth;` |
|       - |  690 | `			sxi32 rc;` |
|     252 |  691 | `			if( bBoundObj ){` |
|     188 |  692 | `				ph7_class_instance *pBoundObj = (ph7_class_instance *)pBound->x.pOther;` |
|       - |  693 | ``				/* A bound PLAIN closure (`function(){…}->bindTo($o)`): $__fn names a function, not a`` |
|       - |  694 | `				 * method of the bound object's class, so a [obj,method] array callable would fail` |
|       - |  695 | `				 * method resolution. Stash the bound object (own a ref) so the OP_CALL user-function` |
|       - |  696 | `				 * frame setup injects it as $this, and return the plain $__fn string for a normal` |
|       - |  697 | `				 * function dispatch. */` |
|     184 |  698 | `				if( (pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0` |
|     133 |  699 | `				 && PH7_ClassExtractMethod(pBoundObj->pClass,` |
|     111 |  700 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){` |
|       - |  701 | `					/* Only a USER function (anonymous closure / named fn in hFunction) reads $this and` |
|       - |  702 | `					 * reaches the OP_CALL user-function frame-setup that consumes pClosureThis. A HOST` |
|       - |  703 | `					 * function (e.g. Closure::fromCallable('strlen')->bindTo($o)) ignores $this and` |
|       - |  704 | `					 * dispatches via the host path which never consumes the transient — setting it` |
|       - |  705 | `					 * there would leak the ref and inject a stale $this into the next call. */` |
|      99 |  706 | `					if( SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|      69 |  707 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|      69 |  708 | `						pBoundObj->iRef++;` |
|      69 |  709 | `						pVm->pClosureThis = pBoundObj;` |
|       - |  710 | `						/* Carry the bound scope (if set) so private/protected member access inside` |
|       - |  711 | `						 * the closure body resolves against it — bindTo($o, Scope::class) / call($o). */` |
|      69 |  712 | `						if( bScope ){` |
|      78 |  713 | `							pVm->pClosureScope = PH7_VmExtractClass(pVm,` |
|      50 |  714 | `								(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      25 |  715 | `						}` |
|      33 |  716 | `					}` |
|      69 |  717 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      69 |  718 | `					return SXRET_OK;` |
|       - |  719 | `				}` |
|      63 |  720 | `			}else{` |
|       - |  721 | ``				/* Scope-only rebind of a PLAIN closure (`bindTo(null, Scope::class)`):`` |
|       - |  722 | `				 * $__fn names a function, not a static method of the scope class, so` |
|       - |  723 | `				 * the [scope, method] array callable below would fail method` |
|       - |  724 | `				 * resolution. Dispatch the plain $__fn string and carry the scope for` |
|       - |  725 | `				 * private/protected visibility (pClosureThis stays unset — no $this).` |
|       - |  726 | `				 * A static-method FCC ($__fn really is a method of the scope class)` |
|       - |  727 | `				 * falls through to the array-callable path. */` |
|      99 |  728 | `				ph7_class *pScopeClass = PH7_VmExtractClass(pVm,` |
|      64 |  729 | `					(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      64 |  730 | `				if( pScopeClass == 0` |
|      67 |  731 | `				 \|\| ((pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0` |
|      35 |  732 | `				  && PH7_ClassExtractMethod(pScopeClass,` |
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
|     180 |  743 | `			pMap = PH7_NewHashmap(&(*pVm), 0, 0);` |
|     180 |  744 | `			if( pMap == 0 ){` |
|     ! 0 |  745 | `				return SXERR_NOTFOUND;` |
|       - |  746 | `			}` |
|     180 |  747 | `			PH7_MemObjInit(pVm, &sTarget);` |
|     180 |  748 | `			PH7_MemObjInit(pVm, &sMeth);` |
|     180 |  749 | `			if( bBoundObj ){` |
|     122 |  750 | `				PH7_MemObjStore(pBound, &sTarget); /* bound object (iRef++) -> binds $this */` |
|      63 |  751 | `			}else{` |
|      61 |  752 | `				PH7_MemObjStringAppend(&sTarget, SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob));` |
|       - |  753 | `			}` |
|     180 |  754 | `			PH7_MemObjStringAppend(&sMeth, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|     180 |  755 | `			rc = PH7_HashmapInsert(pMap, 0, &sTarget);` |
|     180 |  756 | `			if( rc == SXRET_OK ){` |
|     180 |  757 | `				rc = PH7_HashmapInsert(pMap, 0, &sMeth);` |
|      88 |  758 | `			}` |
|     180 |  759 | `			PH7_MemObjRelease(&sTarget);` |
|     180 |  760 | `			PH7_MemObjRelease(&sMeth);` |
|     180 |  761 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  762 | `				PH7_HashmapRelease(pMap, TRUE); /* free the partial map, no leak */` |
|     ! 0 |  763 | `				return SXERR_NOTFOUND;` |
|       - |  764 | `			}` |
|     180 |  765 | `			if( pThis->iFlags & VM_INSTANCE_FCC_SCREENED ){` |
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
|     152 |  776 | `				pVm->bClosureScreened = 1;` |
|      74 |  777 | `			}` |
|     180 |  778 | `			pOut->x.pOther = pMap;` |
|     180 |  779 | `			MemObjSetType(pOut, MEMOBJ_HASHMAP);` |
|     180 |  780 | `			return SXRET_OK;` |
|       - |  781 | `		}` |
|     ! 0 |  782 | `	}` |
|   14567 |  783 | `	PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   14567 |  784 | `	return SXRET_OK;` |
|    7410 |  785 | `}` |
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
|     140 |  832 | `PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget)` |
|       4 |  833 | `{` |
|     284 |  834 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     140 |  835 | `		(sxu32)SyBlobLength(&pTarget->sBlob));` |
|       4 |  836 | `}` |
|       - |  837 | `/*` |
|       - |  838 | ` * The same resolution over a raw (name, length) pair, for the callable machinery: php` |
|       - |  839 | `` * resolves `self`/`parent`/`static` in a CALLBACK (is_callable, call_user_func, array_map …)`` |
|       - |  840 | `` * against the live class context, and refuses them in the direct `$cb()` dispatch — so this`` |
|       - |  841 | ` * is deliberately NOT wired into the OP_CALL resolve check, which must keep answering` |
|       - |  842 | `` * `Class "self" not found`.`` |
|       - |  843 | ` */` |
|  101058 |  844 | `PH7_PRIVATE ph7_class * PH7_VmResolveScopeName(ph7_vm *pVm, const char *zCls, sxu32 nCls)` |
|       5 |  845 | `{` |
|       - |  846 | `	ph7_class *pClass;` |
|  101063 |  847 | `	if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|     106 |  848 | `		pClass = PH7_VmPeekSelfClass(&(*pVm)); /* self:: in a trait -> the USING class */` |
|  101012 |  849 | `	}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|      74 |  850 | `		pClass = PH7_VmPeekTopClass(&(*pVm));     /* late static binding */` |
|  100926 |  851 | `	}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|      46 |  852 | `		pClass = PH7_VmResolveParentClass(&(*pVm));` |
|      25 |  853 | `	}else{` |
|  100849 |  854 | `		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - |  855 | `	}` |
|  101063 |  856 | `	return pClass;` |
|       5 |  857 | `}` |
|       - |  858 | `/*` |
|       - |  859 | ` * Create a Closure object wrapping a callable name (+ optional bound $this object and/or` |
|       - |  860 | `` * scope class-name, for the method/static first-class callables `$o->m(...)`/`C::m(...)`).`` |
|       - |  861 | ` * Mirrors the Generator/Fiber "object carries its state in private attributes" pattern.` |
|       - |  862 | ` * Returns the fresh instance (iRef == 0; caller takes the reference), or 0 on OOM.` |
|       - |  863 | ` */` |
|    8632 |  864 | `PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName,` |
|       - |  865 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       5 |  866 | `{` |
|       - |  867 | `	ph7_class_instance *pObj;` |
|       - |  868 | `	ph7_value *pAttr;` |
|       - |  869 | `	SyString sAttr;` |
|    8637 |  870 | `	if( pVm->pClosureClass == 0 ){` |
|     ! 0 |  871 | `		return 0;` |
|       - |  872 | `	}` |
|    8637 |  873 | `	pObj = PH7_NewClassInstance(&(*pVm), pVm->pClosureClass);` |
|    8637 |  874 | `	if( pObj == 0 ){` |
|     ! 0 |  875 | `		return 0;` |
|       - |  876 | `	}` |
|    8637 |  877 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    8637 |  878 | `	pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|    8637 |  879 | `	if( pAttr ){` |
|    8637 |  880 | `		PH7_MemObjStringAppend(pAttr, pName->zString, pName->nByte);` |
|    4316 |  881 | `	}` |
|    8637 |  882 | `	if( pBoundThis ){` |
|     158 |  883 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     158 |  884 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|     158 |  885 | `		if( pAttr ){` |
|     158 |  886 | `			pAttr->x.pOther = pBoundThis;` |
|     158 |  887 | `			MemObjSetType(pAttr, MEMOBJ_OBJ);` |
|     158 |  888 | `			pBoundThis->iRef++; /* keep the bound object alive for the closure's lifetime */` |
|      77 |  889 | `		}` |
|      77 |  890 | `	}` |
|    8637 |  891 | `	if( pScope && pScope->nByte ){` |
|     228 |  892 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     228 |  893 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|     228 |  894 | `		if( pAttr ){` |
|     228 |  895 | `			PH7_MemObjStringAppend(pAttr, pScope->zString, pScope->nByte);` |
|     112 |  896 | `		}` |
|     112 |  897 | `	}` |
|    8637 |  898 | `	if( pBoundThis \|\| (pScope && pScope->nByte) ){` |
|       - |  899 | `		/* Mark bound/static FCC closures so VmClosureUnwrap can skip the $__this/$__scope` |
|       - |  900 | `		 * lookups on the hot plain-closure dispatch path. */` |
|     228 |  901 | `		pObj->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|     112 |  902 | `	}` |
|    8637 |  903 | `	return pObj;` |
|    4321 |  904 | `}` |
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
|     196 |  953 | `PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue)` |
|       4 |  954 | `{` |
|       - |  955 | `	/* A Closure is already callable; never double-wrap it (this also stops the __invoke branch` |
|       - |  956 | `	 * below from binding a closure to its OWN __invoke). The OP_LOAD_FCC caller intercepts a` |
|       - |  957 | `	 * Closure first too, but guarding here keeps the primitive safe for a direct Increment-2 caller. */` |
|     200 |  958 | `	if( VmValueIsClosure(pVm, pValue) ){` |
|     ! 0 |  959 | `		return 0;` |
|       - |  960 | `	}` |
|     200 |  961 | `	if( !PH7_VmIsCallable(pVm, pValue, TRUE) ){` |
|      47 |  962 | `		return 0;` |
|       - |  963 | `	}` |
|     154 |  964 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - |  965 | `		SyString sName;` |
|      97 |  966 | `		SyStringInitFromBuf(&sName, SyBlobData(&pValue->sBlob), SyBlobLength(&pValue->sBlob));` |
|      97 |  967 | `		return VmCreateClosure(pVm, &sName, 0, 0);` |
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
|     102 | 1031 | `}` |
|       - | 1032 | `/*` |
|       - | 1033 | ` * Return a fresh Closure instance (iRef==0 from create/clone) as a builtin result, taking the` |
|       - | 1034 | ` * one reference into pCtx->pRet — mirrors the OP_LOAD_FCC store convention.` |
|       - | 1035 | ` */` |
|     132 | 1036 | `static int VmClosureResult(ph7_context *pCtx, ph7_class_instance *pClosure)` |
|       4 | 1037 | `{` |
|     136 | 1038 | `	if( pClosure == 0 ){` |
|     ! 0 | 1039 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1040 | `		return PH7_OK;` |
|       - | 1041 | `	}` |
|     136 | 1042 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     136 | 1043 | `	pClosure->iRef++;` |
|     136 | 1044 | `	pCtx->pRet->x.pOther = pClosure;` |
|     136 | 1045 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|     136 | 1046 | `	return PH7_OK;` |
|      70 | 1047 | `}` |
|       - | 1048 | `/*` |
|       - | 1049 | ` * Overwrite a Closure clone's $__this (bound object, or 0 to unbind) and, when pScope != 0, its` |
|       - | 1050 | ` * $__scope (the class-name string; pScope->nByte==0 clears it). pScope==0 leaves $__scope as the` |
|       - | 1051 | ` * clone inherited it (PHP's "static" = keep-scope). Refreshes VM_INSTANCE_FCC_BOUND from the result.` |
|       - | 1052 | ` * The clone inherited a ref on the original $__this (PH7_CloneClassInstance copies via MemObjStore);` |
|       - | 1053 | ` * this drops that and takes one on pNewThis.` |
|       - | 1054 | ` */` |
|     102 | 1055 | `static void VmClosureRebind(ph7_class_instance *pClone,` |
|       - | 1056 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|       4 | 1057 | `{` |
|       - | 1058 | `	SyString sAttr;` |
|       - | 1059 | `	ph7_value *pThisAttr, *pScopeAttr;` |
|     106 | 1060 | `	int bBound = 0;` |
|     106 | 1061 | `	SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     106 | 1062 | `	pThisAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|     106 | 1063 | `	if( pThisAttr ){` |
|       - | 1064 | `		/* PH7_MemObjRelease already drops the cloned-in object's refcount for an OBJ value —` |
|       - | 1065 | `		 * do NOT also decrement by hand (that double-frees the original bound object). */` |
|     106 | 1066 | `		PH7_MemObjRelease(pThisAttr);` |
|     106 | 1067 | `		if( pNewThis ){` |
|      90 | 1068 | `			pThisAttr->x.pOther = pNewThis;` |
|      90 | 1069 | `			MemObjSetType(pThisAttr, MEMOBJ_OBJ);` |
|      90 | 1070 | `			pNewThis->iRef++;` |
|      43 | 1071 | `		}` |
|      51 | 1072 | `	}` |
|     106 | 1073 | `	if( pScope ){` |
|      69 | 1074 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      69 | 1075 | `		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      69 | 1076 | `		if( pScopeAttr ){` |
|      69 | 1077 | `			PH7_MemObjRelease(pScopeAttr);` |
|      69 | 1078 | `			if( pScope->nByte ){` |
|      69 | 1079 | `				PH7_MemObjStringAppend(pScopeAttr, pScope->zString, pScope->nByte);` |
|      33 | 1080 | `			}` |
|      33 | 1081 | `		}` |
|      33 | 1082 | `	}` |
|       - | 1083 | `	/* Refresh the FCC_BOUND fast-path flag from the resulting $__this/$__scope. pThisAttr already` |
|       - | 1084 | `	 * points at the final $__this slot; only $__scope needs a (re)fetch — the top half fetched it` |
|       - | 1085 | `	 * just for pScope != 0. */` |
|     106 | 1086 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     106 | 1087 | `	pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|     102 | 1088 | `	if( (pThisAttr && (pThisAttr->iFlags & MEMOBJ_OBJ))` |
|      63 | 1089 | `		\|\| (pScopeAttr && (pScopeAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeAttr->sBlob) > 0) ){` |
|      98 | 1090 | `		bBound = 1;` |
|      47 | 1091 | `	}` |
|     106 | 1092 | `	if( bBound ){` |
|      98 | 1093 | `		pClone->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      51 | 1094 | `	}else{` |
|      10 | 1095 | `		pClone->iFlags &= ~VM_INSTANCE_FCC_BOUND;` |
|       - | 1096 | `	}` |
|     106 | 1097 | `}` |
|       - | 1098 | `/*` |
|       - | 1099 | ` * Resolve the bindTo/bind/call $scope argument to a class-name SyString.` |
|       - | 1100 | ` * Returns 1 and fills *pOut if $__scope should be replaced (pOut->nByte==0 means "clear");` |
|       - | 1101 | ` * returns 0 to leave the scope unchanged (the PHP "static" sentinel / scope omitted).` |
|       - | 1102 | ` */` |
|     108 | 1103 | `static int VmClosureResolveScope(ph7_value *pScopeArg, SyString *pOut)` |
|       4 | 1104 | `{` |
|     112 | 1105 | `	if( pScopeArg == 0 ){` |
|      43 | 1106 | `		return 0; /* keep */` |
|       - | 1107 | `	}` |
|      68 | 1108 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeArg->sBlob) == 6` |
|      37 | 1109 | `		&& SyMemcmp((const void *)SyBlobData(&pScopeArg->sBlob), (const void *)"static", 6) == 0 ){` |
|       6 | 1110 | `		return 0; /* "static" -> keep current scope */` |
|       - | 1111 | `	}` |
|      67 | 1112 | `	if( pScopeArg->iFlags & MEMOBJ_NULL ){` |
|       3 | 1113 | `		SyStringInitFromBuf(pOut, 0, 0); /* unscoped */` |
|       3 | 1114 | `		return 1;` |
|       - | 1115 | `	}` |
|      65 | 1116 | `	if( pScopeArg->iFlags & MEMOBJ_OBJ ){` |
|       6 | 1117 | `		ph7_class_instance *pScopeObj = (ph7_class_instance *)pScopeArg->x.pOther;` |
|       6 | 1118 | `		*pOut = pScopeObj->pClass->sName;` |
|       6 | 1119 | `		return 1;` |
|       - | 1120 | `	}` |
|      61 | 1121 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) == 0 && (pScopeArg->iFlags & MEMOBJ_SCALAR) ){` |
|       - | 1122 | ``		/* php declares `object\|string\|null $newScope` and coerces a scalar into it in weak`` |
|       - | 1123 | ``		 * mode, so `bindTo($o, 5)` reaches the lookup as the NAME "5" and warns that no such`` |
|       - | 1124 | `		 * class exists. Falling through as "keep the current scope" bound it silently. */` |
|       3 | 1125 | `		PH7_MemObjToString(pScopeArg);` |
|       1 | 1126 | `	}` |
|      61 | 1127 | `	if( pScopeArg->iFlags & MEMOBJ_STRING ){` |
|      61 | 1128 | `		SyStringInitFromBuf(pOut, (const char *)SyBlobData(&pScopeArg->sBlob), SyBlobLength(&pScopeArg->sBlob));` |
|      61 | 1129 | `		return 1;` |
|       - | 1130 | `	}` |
|     ! 0 | 1131 | `	return 0;` |
|      58 | 1132 | `}` |
|       - | 1133 | `/*` |
|       - | 1134 | ``  * Fiber's C-bodied methods. Every one of them was a global `__fiber_verb($this,…)` `` |
|       - | 1135 | ` * thunk that a one-line prelude method forwarded to; the class body in the builtin` |
|       - | 1136 | ` * chunk now holds only its two private slots.` |
|       - | 1137 | ` */` |
|    5146 | 1138 | `PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm)` |
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
|    5151 | 1170 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|       5 | 1171 | `}` |
|       - | 1172 | `/*` |
|       - | 1173 | `` * Generator's C-bodied methods, plus the `implements Iterator` the chunk can no`` |
|       - | 1174 | ` * longer carry: PH7_ClassImplement installs an abstract stub for any interface` |
|       - | 1175 | ` * method the class does not already declare, so it has to run AFTER the eight` |
|       - | 1176 | ` * methods below exist — at which point the stubs are skipped and the class is` |
|       - | 1177 | ` * concrete, exactly as the prelude declaration used to make it.` |
|       - | 1178 | ` */` |
|    5146 | 1179 | `PH7_PRIVATE sxi32 PH7_VmInstallGeneratorNative(ph7_vm *pVm)` |
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
|    5151 | 1206 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    5151 | 1207 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1208 | `		return rc;` |
|       - | 1209 | `	}` |
|       - | 1210 | ``	/* `implements Iterator` last, for the reason in this function's header. */`` |
|    5151 | 1211 | `	pClass = PH7_VmExtractClass(&(*pVm),"Generator",sizeof("Generator")-1,0,0);` |
|    5151 | 1212 | `	pIterator = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,0,0);` |
|    5151 | 1213 | `	if( pClass == 0 \|\| pIterator == 0 ){` |
|     ! 0 | 1214 | `		return SXERR_NOTFOUND;` |
|       - | 1215 | `	}` |
|    5151 | 1216 | `	return PH7_ClassImplement(pClass,pIterator);` |
|    2578 | 1217 | `}` |
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
|     102 | 1237 | `static ph7_class_instance * VmCloneClosureInstance(ph7_class_instance *pClosure)` |
|       4 | 1238 | `{` |
|     106 | 1239 | `	ph7_class_instance *pClone = PH7_CloneClassInstance(pClosure);` |
|     106 | 1240 | `	if( pClone ){` |
|     157 | 1241 | `		pClone->iFlags \|= pClosure->iFlags` |
|     102 | 1242 | `			& (VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED\|VM_INSTANCE_FCC_INVOKE_OBJ);` |
|      51 | 1243 | `	}` |
|     106 | 1244 | `	return pClone;` |
|       4 | 1245 | `}` |
|       - | 1246 | `/*` |
|       - | 1247 | `` * Is this closure STATIC — one that can never take a `$this`? For a plain closure that is the`` |
|       - | 1248 | `` * `static function(){}` declaration flag; for a METHOD callable it is the method's own`` |
|       - | 1249 | `` * staticness, which the flag cannot see because `$__fn` names a method and not a function in`` |
|       - | 1250 | `` * hFunction. `Base::stat(...)` is exactly as static as `static fn()` to php.`` |
|       - | 1251 | ` */` |
|     106 | 1252 | `static int VmClosureIsStatic(ph7_vm *pVm, ph7_class_instance *pClosure)` |
|       4 | 1253 | `{` |
|       - | 1254 | `	SyString sAttr;` |
|       - | 1255 | `	ph7_value *pFn;` |
|     110 | 1256 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|     110 | 1257 | `	pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|     110 | 1258 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 | 1259 | `		return 0;` |
|       - | 1260 | `	}` |
|     110 | 1261 | `	if( pClosure->iFlags & VM_INSTANCE_FCC_METHOD ){` |
|      34 | 1262 | `		ph7_class *pScope = PH7_VmClosureScopeClass(pVm, pClosure);` |
|      34 | 1263 | `		ph7_class_method *pMeth = pScope` |
|      48 | 1264 | `			? PH7_ClassExtractMethod(pScope, (const char *)SyBlobData(&pFn->sBlob),` |
|      32 | 1265 | `				SyBlobLength(&pFn->sBlob)) : 0;` |
|      34 | 1266 | `		return (pMeth && (pMeth->iFlags & PH7_CLASS_ATTR_STATIC)) ? 1 : 0;` |
|       - | 1267 | `	}` |
|       - | 1268 | `	{` |
|     115 | 1269 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob),` |
|      37 | 1270 | `			SyBlobLength(&pFn->sBlob));` |
|      78 | 1271 | `		return (pEntry && (((ph7_vm_func *)pEntry->pUserData)->iFlags & VM_FUNC_STATIC_CL)) ? 1 : 0;` |
|       - | 1272 | `	}` |
|      57 | 1273 | `}` |
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
|     120 | 1285 | `static int VmClosureBindAllowed(ph7_vm *pVm, ph7_class_instance *pClosure,` |
|       - | 1286 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|       4 | 1287 | `{` |
|     124 | 1288 | `	int bMethod = (pClosure->iFlags & VM_INSTANCE_FCC_METHOD) != 0;` |
|     124 | 1289 | `	ph7_class *pOwn = bMethod ? PH7_VmClosureScopeClass(pVm, pClosure) : 0;` |
|     124 | 1290 | `	if( pNewThis ){` |
|     104 | 1291 | `		if( VmClosureIsStatic(pVm, pClosure) ){` |
|       6 | 1292 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1293 | `				"Cannot bind an instance to a static closure, this will be an error in PHP 9");` |
|       6 | 1294 | `			return 0;` |
|       - | 1295 | `		}` |
|     100 | 1296 | `		if( pOwn && !PH7_VmInstanceOf(pNewThis->pClass, pOwn) ){` |
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
|       4 | 1307 | `		}` |
|      68 | 1308 | `	}else if( pOwn && !VmClosureIsStatic(pVm, pClosure) ){` |
|       5 | 1309 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1310 | `			"Cannot unbind $this of method, this will be an error in PHP 9");` |
|       5 | 1311 | `		return 0;` |
|       - | 1312 | `	}` |
|     112 | 1313 | `	if( pScope && bMethod ){` |
|      16 | 1314 | `		ph7_class *pWant = pScope->nByte` |
|       9 | 1315 | `			? PH7_VmResolveScopeName(pVm, pScope->zString, pScope->nByte) : 0;` |
|      11 | 1316 | `		if( pWant != pOwn ){` |
|       7 | 1317 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1318 | `				"Cannot rebind scope of closure created from method, this will be an error in PHP 9");` |
|       7 | 1319 | `			return 0;` |
|       - | 1320 | `		}` |
|       2 | 1321 | `	}` |
|     106 | 1322 | `	return 1;` |
|      64 | 1323 | `}` |
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
|    5146 | 1395 | `PH7_PRIVATE sxi32 PH7_VmInstallClosureNative(ph7_vm *pVm)` |
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
|    5151 | 1437 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|       5 | 1438 | `}` |
|       - | 1439 | `/*` |
|       - | 1440 | ` * Closure::bindTo($newThis, $scope='static') / Closure::bind($closure, $newThis, $scope='static').` |
|       - | 1441 | ` * Clone the receiver and rebind $this/$scope; returns the new Closure (NULL on a non-Closure` |
|       - | 1442 | ` * receiver, matching PHP's failure mode).` |
|       - | 1443 | ` */` |
|     108 | 1444 | `PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 1445 | `{` |
|     112 | 1446 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1447 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|       - | 1448 | `	ph7_value *pNewThisArg;` |
|       - | 1449 | `	ph7_value *pRecv;` |
|       - | 1450 | `	SyString sScope;` |
|     112 | 1451 | `	const SyString *pScopePtr = 0;` |
|       - | 1452 | `	/* One body, both spellings — as it always was, except the closure now arrives` |
|       - | 1453 | `	 * the way php passes it rather than as a hand-written first argument. Called as` |
|       - | 1454 | `	 * the instance method bindTo(), the receiver IS the closure and the arguments` |
|       - | 1455 | `	 * start at $newThis; called as the static bind(), the closure is argument #1.` |
|       - | 1456 | `	 * Normalizing here is what lets the two share an implementation. */` |
|     112 | 1457 | `	if( PH7_ContextThis(pCtx) ){` |
|      80 | 1458 | `		pRecv = PH7_ContextThisValue(pCtx);` |
|      42 | 1459 | `	}else{` |
|      35 | 1460 | `		if( nArg < 1 ){` |
|     ! 0 | 1461 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1462 | `			return PH7_OK;` |
|       - | 1463 | `		}` |
|      35 | 1464 | `		pRecv = apArg[0];` |
|      35 | 1465 | `		apArg++;` |
|      35 | 1466 | `		nArg--;` |
|       - | 1467 | `	}` |
|     112 | 1468 | `	if( nArg < 1 \|\| !VmValueIsClosure(pVm, pRecv) ){` |
|     ! 0 | 1469 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1470 | `		return PH7_OK;` |
|       - | 1471 | `	}` |
|     112 | 1472 | `	pClosure = (ph7_class_instance *)pRecv->x.pOther;` |
|     112 | 1473 | `	pNewThisArg = apArg[0];` |
|     112 | 1474 | `	if( pNewThisArg->iFlags & MEMOBJ_NULL ){` |
|      25 | 1475 | `		pNewThis = 0;` |
|     101 | 1476 | `	}else if( pNewThisArg->iFlags & MEMOBJ_OBJ ){` |
|      90 | 1477 | `		pNewThis = (ph7_class_instance *)pNewThisArg->x.pOther;` |
|      47 | 1478 | `	}else{` |
|     ! 0 | 1479 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1480 | `			"Closure::bindTo(): Argument #1 ($newThis) must be of type ?object");` |
|       - | 1481 | `	}` |
|     112 | 1482 | `	if( VmClosureResolveScope((nArg > 1) ? apArg[1] : 0, &sScope) ){` |
|      67 | 1483 | `		pScopePtr = &sScope;` |
|      32 | 1484 | `	}` |
|       - | 1485 | `	/* php RESOLVES the scope argument before it decides anything else, and a name no class` |
|       - | 1486 | `	 * answers to is its own warning — ahead of all four rebind refusals, for a plain closure` |
|       - | 1487 | `	 * as much as for a method one. PHL bound the unresolvable scope in silence and then had` |
|       - | 1488 | ``	 * no scope at all, so `bindTo($o, 'Typo')` produced a closure that could not reach the`` |
|       - | 1489 | `	 * private members it was being bound for. */` |
|     108 | 1490 | `	if( pScopePtr && pScopePtr->nByte` |
|      67 | 1491 | `	 && PH7_VmResolveScopeName(pVm, pScopePtr->zString, pScopePtr->nByte) == 0 ){` |
|      11 | 1492 | `		VmErrorFormat(pVm,PH7_CTX_WARNING,"Class \"%z\" not found",pScopePtr);` |
|      11 | 1493 | `		ph7_result_null(pCtx);` |
|      11 | 1494 | `		return PH7_OK;` |
|       - | 1495 | `	}` |
|     102 | 1496 | `	if( !VmClosureBindAllowed(pVm, pClosure, pNewThis, pScopePtr) ){` |
|      18 | 1497 | `		ph7_result_null(pCtx);` |
|      18 | 1498 | `		return PH7_OK;` |
|       - | 1499 | `	}` |
|      86 | 1500 | `	pClone = VmCloneClosureInstance(pClosure);` |
|      86 | 1501 | `	if( pClone == 0 ){` |
|     ! 0 | 1502 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1503 | `		return PH7_OK;` |
|       - | 1504 | `	}` |
|      86 | 1505 | `	VmClosureRebind(pClone, pNewThis, pScopePtr);` |
|      86 | 1506 | `	return VmClosureResult(pCtx, pClone);` |
|      58 | 1507 | `}` |
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
|     366 | 1548 | `PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1549 | `{` |
|     371 | 1550 | `	ph7_vm *pVm = pCtx->pVm;` |
|     371 | 1551 | `	if( pVm->pActiveCtx == 0 ){` |
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
|     371 | 1568 | `	if( pVm->nVmExecDepth != pVm->pActiveCtx->nBodyExecDepth ){` |
|       6 | 1569 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1570 | `			"Cannot suspend across an internal call boundary");` |
|       - | 1571 | `	}` |
|     367 | 1572 | `	if( nArg > 0 ){` |
|     359 | 1573 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|     182 | 1574 | `	}else{` |
|      12 | 1575 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|       - | 1576 | `	}` |
|     367 | 1577 | `	return PH7_SUSPEND;` |
|     188 | 1578 | `}` |
|       - | 1579 | `/*` |
|       - | 1580 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|       - | 1581 | ` * Actual resolution is deferred to start() so that overload selection` |
|       - | 1582 | ` * and closure-environment binding happen with the correct argument context.` |
|       - | 1583 | ` */` |
|     314 | 1584 | `PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1585 | `{` |
|       - | 1586 | `	ph7_class_instance *pThis;` |
|       - | 1587 | `	ph7_value *pAttr;` |
|       - | 1588 | `	SyString sAttrName;` |
|     319 | 1589 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     319 | 1590 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|     319 | 1591 | `	if( nArg < 1 ){` |
|     ! 0 | 1592 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1593 | `			"Fiber::__construct() expects a callable argument");` |
|       - | 1594 | `	}` |
|     319 | 1595 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1596 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1597 | `			"Fiber::__construct(): invalid $this");` |
|       - | 1598 | `	}` |
|     319 | 1599 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|     319 | 1600 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|     ! 0 | 1601 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1602 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|       - | 1603 | `	}` |
|       - | 1604 | ``	/* php validates `callable $callback` HERE, with the ordinary callback-argument`` |
|       - | 1605 | ``	 * screen and its whole reason taxonomy -- `new Fiber('nosuch')` is a TypeError at`` |
|       - | 1606 | `	 * CONSTRUCTION, naming the function it could not find. PHL had a hand-rolled shape` |
|       - | 1607 | `	 * check that only asked "string or object", with a FiberError of its own wording,` |
|       - | 1608 | `	 * and left an unresolvable NAME to fail at start() instead: the fiber constructed` |
|       - | 1609 | `	 * fine and the program learned about its typo one call later. */` |
|       - | 1610 | `	{` |
|     319 | 1611 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx, apArg[0], 1, "callback", FALSE);` |
|     319 | 1612 | `		if( rcCb != PH7_OK ){` |
|      13 | 1613 | `			return rcCb;` |
|       - | 1614 | `		}` |
|       - | 1615 | `	}` |
|       - | 1616 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|     307 | 1617 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     307 | 1618 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     307 | 1619 | `	if( pAttr ){` |
|     307 | 1620 | `		PH7_MemObjStore(apArg[0], pAttr);` |
|     151 | 1621 | `	}` |
|     307 | 1622 | `	return PH7_OK;` |
|     162 | 1623 | `}` |
|       - | 1624 | `/*` |
|       - | 1625 | ` * Resolve a fiber's stored callable to the BODY it runs and the receiver that body` |
|       - | 1626 | `` * needs -- for every shape php's `callable` covers, not just the two PHL used to take.`` |
|       - | 1627 | ` *` |
|       - | 1628 | ` * The constructor screens the argument with php's own callback rules now` |
|       - | 1629 | ` * (PH7_CheckCallbackArg), so what arrives here is a callable; this decides which body` |
|       - | 1630 | `` * it names. `[$obj,'m']`, `['Class','stat']` and `"Class::stat"` are the everyday way`` |
|       - | 1631 | ` * to run an object's method as a coroutine, and all three were refused outright --` |
|       - | 1632 | ` * the first two by the constructor's "string or closure" shape check, the third by a` |
|       - | 1633 | ` * plain-function lookup that could never find a method.` |
|       - | 1634 | ` *` |
|       - | 1635 | ` * Answers 0 for a callable this engine has no BYTECODE body for: a host builtin` |
|       - | 1636 | `` * (`new Fiber('strtoupper')`) and a name php routes through __call/__callStatic --`` |
|       - | 1637 | ` * which is where the visibility rule lives, and why this asks for it. php does not` |
|       - | 1638 | ` * reach a private method through a callable, it reaches __call INSTEAD, so running` |
|       - | 1639 | ` * the private body would be a hole rather than a shortcut. Both shapes run on php,` |
|       - | 1640 | ` * whose fiber switches a real stack; here they are a loud refusal (§10 divergence,` |
|       - | 1641 | ` * twin-paired). *pzWhy names the reason for the caller to report.` |
|       - | 1642 | ` */` |
|     294 | 1643 | `static ph7_vm_func * VmFiberCallableBody(ph7_vm *pVm, ph7_value *pCallable,` |
|       - | 1644 | `	ph7_class_instance **ppThis, const char **pzWhy)` |
|       5 | 1645 | `{` |
|     299 | 1646 | `	ph7_class_method *pMethod = 0;` |
|     299 | 1647 | `	ph7_class *pClass = 0;` |
|     299 | 1648 | `	*ppThis = 0;` |
|     299 | 1649 | `	*pzWhy = 0;` |
|     299 | 1650 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1651 | ``		/* php's `[target, method]` pair, decoded by the one shared reader so a fiber`` |
|       - | 1652 | `		 * agrees with is_callable() and with every dispatch site about what it is. */` |
|      16 | 1653 | `		ph7_value *pTarget = 0, *pName = 0;` |
|      14 | 1654 | `		if( !PH7_VmArrayCallableParts(pVm, (ph7_hashmap *)pCallable->x.pOther, &pTarget, &pName)` |
|      16 | 1655 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1656 | `			*pzWhy = "callable is not a valid [target, method] pair";` |
|     ! 0 | 1657 | `			return 0;` |
|       - | 1658 | `		}` |
|      16 | 1659 | `		pClass = PH7_VmExtractClassFromValue(pVm, pTarget);` |
|      16 | 1660 | `		if( pClass ){` |
|      23 | 1661 | `			pMethod = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pName->sBlob),` |
|      14 | 1662 | `				SyBlobLength(&pName->sBlob));` |
|       7 | 1663 | `		}` |
|      16 | 1664 | `		if( pMethod == 0 \|\| !PH7_VmCallableMethodAccessible(pVm, pClass, pMethod) ){` |
|       5 | 1665 | `			*pzWhy = "callable routes through __call(), which cannot be a fiber body here";` |
|       5 | 1666 | `			return 0;` |
|       - | 1667 | `		}` |
|      11 | 1668 | `		if( (pTarget->iFlags & MEMOBJ_OBJ) && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|       5 | 1669 | `			*ppThis = (ph7_class_instance *)pTarget->x.pOther;` |
|       2 | 1670 | `		}` |
|      11 | 1671 | `		return &pMethod->sFunc;` |
|       - | 1672 | `	}` |
|     285 | 1673 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|       - | 1674 | `		const char *zCls, *zMeth;` |
|       - | 1675 | `		sxu32 nCls, nMeth;` |
|       - | 1676 | `		SyString sName;` |
|       - | 1677 | `		SyHashEntry *pEntry;` |
|     285 | 1678 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       - | 1679 | ``		/* php's `"Class::method"` static-callable string is the same callee as the pair. */`` |
|     285 | 1680 | `		if( PH7_VmCallableStringParts(sName.zString, sName.nByte, &zCls, &nCls, &zMeth, &nMeth) ){` |
|       8 | 1681 | `			pClass = PH7_VmResolveScopeName(pVm, zCls, nCls);` |
|       8 | 1682 | `			pMethod = pClass ? PH7_ClassExtractMethod(pClass, zMeth, nMeth) : 0;` |
|       8 | 1683 | `			if( pMethod == 0 \|\| !PH7_VmCallableMethodAccessible(pVm, pClass, pMethod) ){` |
|       5 | 1684 | `				*pzWhy = "callable routes through __callStatic(), which cannot be a fiber body here";` |
|       5 | 1685 | `				return 0;` |
|       - | 1686 | `			}` |
|       3 | 1687 | `			return &pMethod->sFunc;` |
|       - | 1688 | `		}` |
|     416 | 1689 | `		pEntry = PH7_VmGetUserFunction(pVm, sName.zString, sName.nByte,` |
|     274 | 1690 | `			(pCallable->iFlags & MEMOBJ_AUX_ENGINEFN) != 0);` |
|     279 | 1691 | `		if( pEntry == 0 ){` |
|       3 | 1692 | `			*pzWhy = SyHashGet(&pVm->hHostFunction, sName.zString, sName.nByte)` |
|       - | 1693 | `				? "callable is an internal function, which cannot be a fiber body here"` |
|       1 | 1694 | `				: "callable names no such function";` |
|       3 | 1695 | `			return 0;` |
|       - | 1696 | `		}` |
|     277 | 1697 | `		return (ph7_vm_func *)pEntry->pUserData;` |
|       - | 1698 | `	}` |
|     ! 0 | 1699 | `	*pzWhy = "callable is not a string, array or object";` |
|     ! 0 | 1700 | `	return 0;` |
|     152 | 1701 | `}` |
|       - | 1702 | `/*` |
|       - | 1703 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|       - | 1704 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|       - | 1705 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|       - | 1706 | ` * so that start() can bind it as $this for the closure environment.` |
|       - | 1707 | ` */` |
|     296 | 1708 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|       - | 1709 | `	ph7_class_instance **ppThis)` |
|       5 | 1710 | `{` |
|     301 | 1711 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1712 | `	ph7_value *pCallable;` |
|       - | 1713 | `	SyString sAttrName;` |
|     301 | 1714 | `	*ppThis = 0;` |
|     301 | 1715 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     301 | 1716 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|     301 | 1717 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP)) == 0 ){` |
|     ! 0 | 1718 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|     ! 0 | 1719 | `		return 0;` |
|       - | 1720 | `	}` |
|     301 | 1721 | `	if( pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP) ){` |
|     259 | 1722 | `		const char *zWhy = 0;` |
|     259 | 1723 | `		ph7_vm_func *pFunc = VmFiberCallableBody(pVm, pCallable, ppThis, &zWhy);` |
|     259 | 1724 | `		if( pFunc == 0 ){` |
|      11 | 1725 | `			PH7_VmThrowException(pCtx, "FiberError", "Fiber %s", zWhy);` |
|       5 | 1726 | `		}` |
|     259 | 1727 | `		return pFunc;` |
|     ! 0 | 1728 | `	}else{` |
|      46 | 1729 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|       - | 1730 | `		ph7_class_method *pMethod;` |
|      46 | 1731 | `		if( VmValueIsClosure(pVm, pCallable) ){` |
|       - | 1732 | `			/* A real Closure object: unwrap to its underlying callable name (the single` |
|       - | 1733 | `			 * source of truth, VmClosureUnwrap) and resolve that function. Its captured` |
|       - | 1734 | ``			 * environment (including any `$this`) rides along in the named function's`` |
|       - | 1735 | `			 * aClosureEnv, installed by VmFiberSetupFrame, so *ppThis stays 0. */` |
|       - | 1736 | `			ph7_value sName;` |
|      44 | 1737 | `			ph7_vm_func *pUnwrapped = 0;` |
|      44 | 1738 | `			const char *zWhyClo = 0;` |
|      44 | 1739 | `			PH7_MemObjInit(pVm, &sName);` |
|      44 | 1740 | `			if( VmClosureUnwrap(pVm, pCallable, &sName) == SXRET_OK ){` |
|       - | 1741 | ``				/* The engine's own `[closure_N]` key, which only this mark gets past the`` |
|       - | 1742 | `				 * script-facing name screen (PH7_VmGetUserFunction). */` |
|      44 | 1743 | `				sName.iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|       - | 1744 | `` 				/* The unwrap answers a NAME for a plain closure and a `[target, method]` `` |
|       - | 1745 | `				 * pair for a first-class callable taken from a method -- so it goes through` |
|       - | 1746 | `				 * the same body-finder as a callable the program wrote. Without it a` |
|       - | 1747 | ``				 * `$o->stat(...)` fiber could not be resolved at all. */`` |
|      44 | 1748 | `				pUnwrapped = VmFiberCallableBody(pVm, &sName, ppThis, &zWhyClo);` |
|      20 | 1749 | `			}` |
|      44 | 1750 | `			PH7_MemObjRelease(&sName);` |
|      44 | 1751 | `			if( pUnwrapped ){` |
|       - | 1752 | `				/* A BOUND closure parked its $this in the pClosureThis transient` |
|       - | 1753 | `				 * (VmClosureUnwrap): consume it as the fiber's $this — it wins over` |
|       - | 1754 | `				 * the creation-time env capture (VmFiberSetupFrame's skip). *ppThis` |
|       - | 1755 | `				 * is a borrow (the closure's $__this attr keeps the object alive` |
|       - | 1756 | `				 * through $__callable), so drop the parked ref. The scope transient` |
|       - | 1757 | `				 * is cleared alongside: fiber bodies don't model pBoundScope` |
|       - | 1758 | `				 * visibility (recorded residual), and a stale transient would` |
|       - | 1759 | `				 * poison the next OP_CALL's frame. */` |
|      44 | 1760 | `				if( pVm->pClosureThis ){` |
|     ! 0 | 1761 | `					*ppThis = pVm->pClosureThis;` |
|     ! 0 | 1762 | `					PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1763 | `					pVm->pClosureThis = 0;` |
|     ! 0 | 1764 | `				}` |
|      44 | 1765 | `				pVm->pClosureScope = 0;` |
|      44 | 1766 | `				pVm->bClosureScreened = 0;` |
|      44 | 1767 | `				return pUnwrapped;` |
|       - | 1768 | `			}` |
|     ! 0 | 1769 | `			if( pVm->pClosureThis ){` |
|       - | 1770 | `				/* Failed resolution: drop the parked transient so it neither leaks` |
|       - | 1771 | `				 * nor poisons the next call. */` |
|     ! 0 | 1772 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1773 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 1774 | `			}` |
|     ! 0 | 1775 | `			pVm->pClosureScope = 0;` |
|     ! 0 | 1776 | `			pVm->bClosureScreened = 0;` |
|     ! 0 | 1777 | `			PH7_VmThrowException(pCtx, "FiberError", zWhyClo` |
|     ! 0 | 1778 | `				? "Fiber %s" : "Fiber callable closure could not be resolved", zWhyClo);` |
|     ! 0 | 1779 | `			return 0;` |
|       - | 1780 | `		}` |
|       - | 1781 | `		/* Object callable — resolve __invoke method */` |
|       3 | 1782 | `		pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 1783 | `			sizeof("__invoke") - 1);` |
|       3 | 1784 | `		if( pMethod == 0 ){` |
|     ! 0 | 1785 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1786 | `				"Fiber callable object has no __invoke method");` |
|     ! 0 | 1787 | `			return 0;` |
|       - | 1788 | `		}` |
|       3 | 1789 | `		*ppThis = pClosure;` |
|       3 | 1790 | `		return &pMethod->sFunc;` |
|       - | 1791 | `	}` |
|     153 | 1792 | `}` |
|       - | 1793 | `/*` |
|       - | 1794 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|       - | 1795 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|       - | 1796 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|       - | 1797 | ` */` |
|       - | 1798 | `/*` |
|       - | 1799 | ` * Enforce one formal parameter's declared type on an argument being installed.` |
|       - | 1800 | ` * THE single implementation of the per-argument check, shared by the` |
|       - | 1801 | ` * generator/fiber initial-frame binder below (band A #2) and both OP_CALL` |
|       - | 1802 | ` * install paths (named-map and positional — they carried two verbatim copies` |
|       - | 1803 | ` * until the §7.1(f) fold): union types via VmCoerceToUnion, class and` |
|       - | 1804 | ` * pseudo types (VmCheckPseudoType + VmResolveTypeClass + instanceof, so` |
|       - | 1805 | ``  * interfaces/abstract classes and self/parent resolve), the bare `object` `` |
|       - | 1806 | ` * hint, and scalars via VmEnforceScalarType (weak-mode coercion in place,` |
|       - | 1807 | ` * strict rejection otherwise) — with the VM_FUNC_ARG_NULLABLE guard letting` |
|       - | 1808 | `` * null through for `?type` and implicit-nullable `Type $x = null` params,`` |
|       - | 1809 | ` * and whole-real materialization on a mask match.` |
|       - | 1810 | ` * Returns SXRET_OK (value possibly coerced) or the VmThrowTypeErrorForArg` |
|       - | 1811 | ` * status for the caller to route — normalized so an INLINE-caught throw` |
|       - | 1812 | ` * (VmThrowInline records only a pc-redirect and reports SXRET_OK) still` |
|       - | 1813 | ` * comes back as PH7_EXCEPTION: the binding must stop; the OP_CALL sites` |
|       - | 1814 | ` * force rc = PH7_EXCEPTION on any non-OK status anyway, and the generator` |
|       - | 1815 | ` * block's PH7_INLINE_RESUME_BREAK consumes the redirect.` |
|       - | 1816 | ` */` |
|     308 | 1817 | `static sxi32 VmGenArgThrowStatus(ph7_vm *pVm, sxi32 rcThrow)` |
|       5 | 1818 | `{` |
|     313 | 1819 | `	if( rcThrow == SXRET_OK && pVm->pInlineInstr ){` |
|     ! 0 | 1820 | `		return PH7_EXCEPTION;` |
|       - | 1821 | `	}` |
|     313 | 1822 | `	return rcThrow;` |
|     159 | 1823 | `}` |
|  260633 | 1824 | `PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_vm_func_arg *pFormal,` |
|       - | 1825 | `	sxu32 nArgPos, ph7_value *pVal, int bStrict, ph7_class *pSelfHint)` |
|       5 | 1826 | `{` |
|  260638 | 1827 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|     285 | 1828 | `		if( VmCoerceToUnion(pVm,pVal,&pFormal->aUnionAlts,` |
|     290 | 1829 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,bStrict,pSelfHint) != SXRET_OK ){` |
|       - | 1830 | `			const char *zGiven;` |
|      80 | 1831 | `			const char *zExpected = "union";` |
|       - | 1832 | `			char zBuf[128];` |
|       - | 1833 | `			char zTypeBuf[128];` |
|      80 | 1834 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      50 | 1835 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      57 | 1836 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      10 | 1837 | `				zGiven = "null";` |
|       6 | 1838 | `			}else{` |
|      24 | 1839 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|       - | 1840 | `			}` |
|      80 | 1841 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|     118 | 1842 | `				zExpected = VmHintTextResolved(pVm,&pFormal->sTypeName,pSelfHint,` |
|      38 | 1843 | `					zTypeBuf,sizeof(zTypeBuf));` |
|      38 | 1844 | `			}` |
|     118 | 1845 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      38 | 1846 | `				&pFormal->sName,zExpected,zGiven));` |
|       - | 1847 | `		}` |
|     119 | 1848 | `		return SXRET_OK;` |
|       - | 1849 | `	}` |
|  260443 | 1850 | `	if( pFormal->nType == 0` |
|  140036 | 1851 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|  241765 | 1852 | `		return SXRET_OK;` |
|       - | 1853 | `	}` |
|   18688 | 1854 | `	if( pFormal->nType == SXU32_HIGH ){` |
|       - | 1855 | `		/* Class or pseudo type */` |
|    2393 | 1856 | `		SyString *pName = &pFormal->sClass;` |
|       - | 1857 | `		ph7_class *pClass;` |
|    2393 | 1858 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|    2393 | 1859 | `		if( rcPseudo == 0 ){` |
|       - | 1860 | `			char zTypeBuf[128],zGivenBuf[128];` |
|     139 | 1861 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      34 | 1862 | `				&pFormal->sName,` |
|      68 | 1863 | `				VmClassHintTypeName(pName,0,` |
|      68 | 1864 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      34 | 1865 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1866 | `		}` |
|    2325 | 1867 | `		pClass = 0;` |
|    2325 | 1868 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|       - | 1869 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      89 | 1870 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      21 | 1871 | `				&pFormal->sName,` |
|      42 | 1872 | `				VmClassHintTypeName(pName,pClass,` |
|      42 | 1873 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      21 | 1874 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1875 | `		}` |
|    2283 | 1876 | `		return SXRET_OK;` |
|       - | 1877 | `	}` |
|   16300 | 1878 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|       - | 1879 | `		char zGivenBuf[128];` |
|     213 | 1880 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|      77 | 1881 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       8 | 1882 | `				&pFormal->sName,"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1883 | `		}` |
|     197 | 1884 | `		if( VmEnforceScalarType(pVal,pFormal->nType,bStrict) != SXRET_OK ){` |
|       - | 1885 | `			char zTypeBuf[128];` |
|     158 | 1886 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      51 | 1887 | `				&pFormal->sName,` |
|      51 | 1888 | `				VmScalarTypeName(pFormal->nType,&pFormal->sTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      51 | 1889 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 1890 | `		}` |
|      49 | 1891 | `	}else{` |
|       - | 1892 | `		/* Mask matched — an int param accepting a whole-real materializes` |
|       - | 1893 | `		 * it (php: g(1.0) into int $x is int(1)). */` |
|   16092 | 1894 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|       - | 1895 | `	}` |
|   16182 | 1896 | `	return SXRET_OK;` |
|  130766 | 1897 | `}` |
|       - | 1898 | `/*` |
|       - | 1899 | ` * Record a caller slot this body's frame now ALIASES through a by-reference` |
|       - | 1900 | ` * parameter. The body outlives its caller, so the two frames cannot each own the` |
|       - | 1901 | ` * slot: the caller's teardown counts this frame's name binding as a holder and` |
|       - | 1902 | ` * leaves the value standing, and VmReleaseExecCtx asks PH7_VmReleaseUnheldSlot for` |
|       - | 1903 | ` * every row here once its own names are gone — whichever dies last frees it.` |
|       - | 1904 | ` */` |
|      32 | 1905 | `static void VmCtxAliasByRefArg(ph7_exec_ctx *pExecCtx,sxu32 nIdx)` |
|       2 | 1906 | `{` |
|      34 | 1907 | `	sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pExecCtx->aByRefArg);` |
|       - | 1908 | `	sxu32 n;` |
|      34 | 1909 | `	for( n = 0 ; n < SySetUsed(&pExecCtx->aByRefArg) ; ++n ){` |
|     ! 0 | 1910 | `		if( aIdx[n] == nIdx ){` |
|       - | 1911 | ``			/* Two parameters over one actual (`g($x,$x)`) is ONE slot to give back. */`` |
|     ! 0 | 1912 | `			return;` |
|       - | 1913 | `		}` |
|     ! 0 | 1914 | `	}` |
|      34 | 1915 | `	SySetPut(&pExecCtx->aByRefArg,(const void *)&nIdx);` |
|      18 | 1916 | `}` |
|     694 | 1917 | `PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|       - | 1918 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg,` |
|       - | 1919 | `	int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg, int bAliasByRef)` |
|       5 | 1920 | `{` |
|     699 | 1921 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|       - | 1922 | `	ph7_vm_func_arg *aFormalArg;` |
|       - | 1923 | `	sxu32 nFormal, n;` |
|     699 | 1924 | `	sxu32 nReqGF = 0, nNonVarGF = 0; /* too-few-args watermark (0 = exempt) */` |
|       - | 1925 | `	VmSlot sSlot;` |
|       - | 1926 | `	sxi32 rc;` |
|       - | 1927 | `	/* Install $this for closure/method callables */` |
|     699 | 1928 | `	if( pClosureThis ){` |
|       - | 1929 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      24 | 1930 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      24 | 1931 | `		if( pObj ){` |
|      24 | 1932 | `			pObj->x.pOther = pClosureThis;` |
|      24 | 1933 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      24 | 1934 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      10 | 1935 | `		}` |
|      10 | 1936 | `	}` |
|       - | 1937 | `	/* Install static variables */` |
|     699 | 1938 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|       - | 1939 | `		ph7_vm_func_static_var *aStatic;` |
|       - | 1940 | `		ph7_value *pVal;` |
|     ! 0 | 1941 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|     ! 0 | 1942 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|     ! 0 | 1943 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|     ! 0 | 1944 | `			if( pVal ){` |
|     ! 0 | 1945 | `				sSlot.pUserData = 0;` |
|     ! 0 | 1946 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|     ! 0 | 1947 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|     ! 0 | 1948 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|     ! 0 | 1949 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|     ! 0 | 1950 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);` |
|     ! 0 | 1951 | `				}` |
|     ! 0 | 1952 | `			}` |
|     ! 0 | 1953 | `		}` |
|     ! 0 | 1954 | `	}` |
|       - | 1955 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|     699 | 1956 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     699 | 1957 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       - | 1958 | `	/* Actual call arity for func_num_args()/func_get_args() (band A #4) */` |
|     699 | 1959 | `	pExecCtx->pFrame->nActualArgs = nArg;` |
|       - | 1960 | `	{` |
|       - | 1961 | `		/* Too-few-arguments watermark — checked per formal INSIDE the install` |
|       - | 1962 | `		 * loop below, after the passed args' type checks, matching php's` |
|       - | 1963 | `		 * RECV order (a type error on a passed argument beats the count` |
|       - | 1964 | `		 * error). Generators throw at the g(...) call site (message embeds` |
|       - | 1965 | `		 * it); fibers at Fiber::start() (php omits the call-site segment).` |
|       - | 1966 | `		 * Prelude builtins are included — VmThrowBuiltinTooFewArgs words them` |
|       - | 1967 | `		 * as php words an internal callable. */` |
|     699 | 1968 | `	nReqGF = VmFuncRequiredArgCount(pFunc,&nNonVarGF);` |
|       - | 1969 | `	}` |
|     821 | 1970 | `	for( n = 0; n < nFormal; n++ ){` |
|       - | 1971 | `		ph7_value *pObj;` |
|     153 | 1972 | `		if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       - | 1973 | `			/* Variadic formal: collect this and every remaining actual into a` |
|       - | 1974 | `			 * fresh array (php semantics — pre-fix nothing collected here, so a` |
|       - | 1975 | ``			 * `function g(int ...$xs)` generator saw a bare scalar in $xs).`` |
|       - | 1976 | `			 * Per-element checks mirror OP_CALL's variadic install: union via` |
|       - | 1977 | ``			 * the shared helper, scalar coerce/TypeError, `object` hint; a`` |
|       - | 1978 | `			 * class-typed variadic element is (like OP_CALL) not checked. */` |
|       7 | 1979 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       7 | 1980 | `			if( pObj ){` |
|       - | 1981 | `				sxu32 nVariadicIdx;` |
|       - | 1982 | `				ph7_hashmap *pMap;` |
|       - | 1983 | `				sxu32 k;` |
|       7 | 1984 | `				PH7_MemObjToHashmap(pObj);` |
|       - | 1985 | `				/* Capture the slot index now: PH7_HashmapInsert can reallocate` |
|       - | 1986 | `				 * pVm->aMemObj and dangle pObj (same hazard as OP_CALL's path). */` |
|       7 | 1987 | `				nVariadicIdx = pObj->nIdx;` |
|       7 | 1988 | `				pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      15 | 1989 | `				for( k = n; k < (sxu32)nArg; k++ ){` |
|      11 | 1990 | `					if( ((aFormalArg[n].iFlags & VM_FUNC_ARG_UNION)` |
|       9 | 1991 | `					   \|\| (aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH)) ){` |
|       7 | 1992 | `						rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[k],bStrict,pSelfHint);` |
|       7 | 1993 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1994 | `							return rc;` |
|       - | 1995 | `						}` |
|       3 | 1996 | `					}` |
|       8 | 1997 | `					if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)` |
|       6 | 1998 | `					 && apArg[k]->nIdx != SXU32_HIGH ){` |
|       - | 1999 | `						/* A by-ref variadic tail aliases its actuals here too — the` |
|       - | 2000 | `						 * ordinary call's rule, one container over. */` |
|       3 | 2001 | `						VmCtxAliasByRefArg(pExecCtx,apArg[k]->nIdx);` |
|       3 | 2002 | `						PH7_HashmapInsertByRef(pMap,0,apArg[k]->nIdx);` |
|       2 | 2003 | `					}else{` |
|       7 | 2004 | `						PH7_HashmapInsert(pMap,0,apArg[k]);` |
|       - | 2005 | `					}` |
|       5 | 2006 | `				}` |
|       7 | 2007 | `				sSlot.nIdx = nVariadicIdx;` |
|       7 | 2008 | `				sSlot.pUserData = 0;` |
|       7 | 2009 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       3 | 2010 | `			}` |
|       7 | 2011 | `			break; /* All remaining actuals consumed */` |
|       - | 2012 | `		}` |
|     147 | 2013 | `		if( n < (sxu32)nArg ){` |
|       - | 2014 | `			/* Argument provided — install with declared-type enforcement.` |
|       - | 2015 | `			 * php binds and type-checks generator arguments EAGERLY at the` |
|       - | 2016 | `			 * g(...) call site (and fiber arguments at Fiber::start()), so the` |
|       - | 2017 | `			 * enforcement lives here, mirroring the OP_CALL install path via` |
|       - | 2018 | `			 * VmEnforceArgType (TypeError on mismatch, weak coercion in` |
|       - | 2019 | `			 * place otherwise) instead of the old silent xCast. A variadic` |
|       - | 2020 | `			 * formal collects as-is (no per-element declared-type model). */` |
|     128 | 2021 | `			if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)` |
|      79 | 2022 | `			 && apArg[n]->nIdx != SXU32_HIGH ){` |
|       - | 2023 | `				/* php binds a generator's by-REFERENCE parameter to the CALLER's slot at` |
|       - | 2024 | `				 * the g(...) that builds the Generator, so the body's write reaches the` |
|       - | 2025 | `				 * caller's variable whenever it eventually runs. Copying it left the` |
|       - | 2026 | `				 * actual untouched for every resume. The type check runs on the actual,` |
|       - | 2027 | `				 * as OP_CALL's by-ref binder does, and never on a copy the alias` |
|       - | 2028 | `				 * replaces. Fiber::start() and the embedder entry pass by VALUE (php's` |
|       - | 2029 | `				 * own decision at those two boundaries), hence bAliasByRef. */` |
|      32 | 2030 | `				sxi32 iPreFlags = apArg[n]->iFlags;` |
|      32 | 2031 | `				rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[n],bStrict,pSelfHint);` |
|      32 | 2032 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 2033 | `					return rc;` |
|       - | 2034 | `				}` |
|       - | 2035 | `				/* A declared type's conversion is what the reference holds (the ordinary` |
|       - | 2036 | `				 * call's rule; the check ran on the operand-stack copy). */` |
|      32 | 2037 | `				PH7_VmByRefArgWriteBack(pVm,apArg[n],iPreFlags);` |
|      47 | 2038 | `				PH7_VmBindVarSlot(pVm,pExecCtx->pFrame,` |
|      30 | 2039 | `					SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName),` |
|      30 | 2040 | `					apArg[n]->nIdx);` |
|      32 | 2041 | `				VmCtxAliasByRefArg(pExecCtx,apArg[n]->nIdx);` |
|      32 | 2042 | `				sSlot.nIdx = apArg[n]->nIdx;` |
|      32 | 2043 | `				sSlot.pUserData = 0;` |
|      32 | 2044 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      32 | 2045 | `				continue;` |
|       - | 2046 | `			}` |
|     103 | 2047 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|     103 | 2048 | `			if( pObj ){` |
|     103 | 2049 | `				PH7_MemObjStore(apArg[n], pObj);` |
|     103 | 2050 | `				if( (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|     103 | 2051 | `					rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,pObj,bStrict,pSelfHint);` |
|     103 | 2052 | `					if( rc != SXRET_OK ){` |
|      18 | 2053 | `						return rc;` |
|       - | 2054 | `					}` |
|      41 | 2055 | `				}` |
|      87 | 2056 | `				sSlot.nIdx = pObj->nIdx;` |
|      87 | 2057 | `				sSlot.pUserData = 0;` |
|      87 | 2058 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      46 | 2059 | `			}` |
|      58 | 2060 | `		}else if( n < nReqGF ){` |
|       - | 2061 | `			/* Required formal with no actual: php's ArgumentCountError, at` |
|       - | 2062 | `			 * this point in the install order (see the watermark comment). */` |
|       7 | 2063 | `			return VmGenArgThrowStatus(pVm,` |
|       4 | 2064 | `				(pFunc->iFlags & VM_FUNC_INTERNAL)` |
|     ! 0 | 2065 | `					? VmThrowBuiltinTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|     ! 0 | 2066 | `						(sxu32)nArg,nReqGF,SySetUsed(&pFunc->aArgs))` |
|       6 | 2067 | `					: VmThrowTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|       2 | 2068 | `						(sxu32)nArg,nReqGF,nNonVarGF,bCallSiteInMsg));` |
|      13 | 2069 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       - | 2070 | `			/* Default value */` |
|      13 | 2071 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      13 | 2072 | `			if( pObj ){` |
|      13 | 2073 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);` |
|      13 | 2074 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2075 | `					return rc;` |
|       - | 2076 | `				}` |
|       - | 2077 | `` 				/* A null default on an implicitly-nullable `Type $x = null` `` |
|       - | 2078 | `				 * param must stay null (php); only non-null defaults keep the` |
|       - | 2079 | `				 * legacy shaping cast. */` |
|      10 | 2080 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       7 | 2081 | `				 && !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|       3 | 2082 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|     ! 0 | 2083 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|     ! 0 | 2084 | `						if( xCast ){` |
|     ! 0 | 2085 | `							xCast(pObj);` |
|     ! 0 | 2086 | `						}` |
|     ! 0 | 2087 | `					}else{` |
|       - | 2088 | `						/* Mask matched — a const-indirected whole-real default` |
|       - | 2089 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|       3 | 2090 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|       - | 2091 | `					}` |
|       1 | 2092 | `				}` |
|      13 | 2093 | `				sSlot.nIdx = pObj->nIdx;` |
|      13 | 2094 | `				sSlot.pUserData = 0;` |
|      13 | 2095 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       5 | 2096 | `			}` |
|       5 | 2097 | `		}` |
|      51 | 2098 | `	}` |
|       - | 2099 | `	/* Install closure environment (captured variables) */` |
|     679 | 2100 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 2101 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|       - | 2102 | `		ph7_value *pValue;` |
|       - | 2103 | `		sxu32 iEnv;` |
|      57 | 2104 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     121 | 2105 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|      69 | 2106 | `			pEnv = &aEnv[iEnv];` |
|      69 | 2107 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|      49 | 2108 | `				continue;` |
|       - | 2109 | `			}` |
|      20 | 2110 | `			if( pClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       4 | 2111 | `			 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|       - | 2112 | `				/* An explicit bound $this (bindTo/bind/call) wins over the` |
|       - | 2113 | `				 * creation-time captured $this, php-exact (mirrors OP_CALL). */` |
|       3 | 2114 | `				continue;` |
|       - | 2115 | `			}` |
|      20 | 2116 | `			if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|       - | 2117 | `				/* Captured by reference: link the name to the shared slot` |
|       - | 2118 | `				 * (no copy), mirroring the OP_CALL env install. */` |
|       5 | 2119 | `				if( SyHashGet(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|       7 | 2120 | `					SyHashInsert(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),` |
|       4 | 2121 | `						SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|       2 | 2122 | `				}` |
|       5 | 2123 | `				continue;` |
|       - | 2124 | `			}` |
|      16 | 2125 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|      16 | 2126 | `			if( pValue == 0 ){` |
|     ! 0 | 2127 | `				continue;` |
|       - | 2128 | `			}` |
|      16 | 2129 | `			PH7_MemObjRelease(pValue);` |
|      16 | 2130 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|       9 | 2131 | `		}` |
|      26 | 2132 | `	}` |
|     679 | 2133 | `	return SXRET_OK;` |
|     352 | 2134 | `}` |
|       - | 2135 | `/*` |
|       - | 2136 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|       - | 2137 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|       - | 2138 | ` *` |
|       - | 2139 | ` * As a native VARIADIC method the arguments arrive directly as (nArg, apArg);` |
|       - | 2140 | ` * the prelude used to hand them over as a single func_get_args() array, which` |
|       - | 2141 | ` * this had to walk and snapshot out of pVm->aMemObj.` |
|       - | 2142 | ` */` |
|     298 | 2143 | `PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2144 | `{` |
|     303 | 2145 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2146 | `	ph7_class_instance *pThis;` |
|       - | 2147 | `	ph7_class_instance *pClosureThis;` |
|       - | 2148 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 2149 | `	ph7_vm_func *pFunc;` |
|       - | 2150 | `	ph7_value sResult;` |
|       - | 2151 | `	ph7_value *pCtxAttr;` |
|       - | 2152 | `	SyString sAttrName;` |
|       - | 2153 | `	sxi32 rc;` |
|     303 | 2154 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     303 | 2155 | `	if( pRecv == 0 \|\| (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2156 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|       - | 2157 | `	}` |
|     303 | 2158 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 2159 | `	/* Check if already started (has a __ctx) */` |
|     303 | 2160 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     303 | 2161 | `	if( pExecCtx != 0 ){` |
|       3 | 2162 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2163 | `			"Cannot start a fiber that has already been started");` |
|       - | 2164 | `	}` |
|       - | 2165 | `	/* Resolve callable */` |
|     301 | 2166 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|     301 | 2167 | `	if( pFunc == 0 ){` |
|      11 | 2168 | `		return PH7_EXCEPTION;` |
|       - | 2169 | `	}` |
|       - | 2170 | ``	/* Fiber::start()'s own `...$args` are by VALUE whatever the body declares, so php`` |
|       - | 2171 | `		 * warns for every by-reference parameter and the body operates on a copy — the` |
|       - | 2172 | `		 * value PHL already produced, without the one diagnostic that says so. Named off` |
|       - | 2173 | `		 * the stored callable, which is what carries the class for a method one. */` |
|     291 | 2174 | `	if( nArg > 0 ){` |
|       - | 2175 | `		SyString sCbName;` |
|       - | 2176 | `		ph7_value *pCbVal;` |
|      12 | 2177 | `		SyStringInitFromBuf(&sCbName, "__callable", 10);` |
|      12 | 2178 | `		pCbVal = PH7_ClassInstanceFetchAttr(pThis, &sCbName);` |
|      12 | 2179 | `		if( pCbVal ){` |
|      12 | 2180 | `			PH7_VmWarnByRefArgsGivenValue(pVm, pCbVal, nArg, 0, 0);` |
|       5 | 2181 | `		}` |
|       5 | 2182 | `	}` |
|       - | 2183 | `	/* Create execution context now that we know the function */` |
|     291 | 2184 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|     291 | 2185 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 2186 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2187 | `			"Fiber::start(): out of memory");` |
|       - | 2188 | `	}` |
|       - | 2189 | `	/* Store context in $this->__ctx */` |
|     291 | 2190 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     291 | 2191 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     291 | 2192 | `	if( pCtxAttr ){` |
|     291 | 2193 | `		pCtxAttr->x.pOther = pExecCtx;` |
|     291 | 2194 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     143 | 2195 | `	}` |
|       - | 2196 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|       - | 2197 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|       - | 2198 | `	 * into the fiber's frame, not the caller's. */` |
|     291 | 2199 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|     291 | 2200 | `	pVm->pFrame = pExecCtx->pFrame;` |
|       - | 2201 | `	/* Unpack the args array and install into the frame */` |
|       - | 2202 | `	{` |
|       - | 2203 | `		/* The arguments are this call's own operand-stack slots, so they can be` |
|       - | 2204 | `		 * handed to the frame setup as-is. The old form had to snapshot them out of` |
|       - | 2205 | `		 * pVm->aMemObj first, because they arrived as a func_get_args() hashmap` |
|       - | 2206 | `		 * whose element values live in that set — and VmFiberSetupFrame reserves` |
|       - | 2207 | `		 * memory objects (VmExtractMemObj) before reading its arguments, which can` |
|       - | 2208 | `		 * reallocate the set and dangle a raw pool pointer. Operand slots do not` |
|       - | 2209 | `		 * move, so the copy is gone with the array that made it necessary. */` |
|     291 | 2210 | `		ph7_value **apValues = (nArg > 0) ? apArg : 0;` |
|     291 | 2211 | `		int nActual = nArg;` |
|     291 | 2212 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues,` |
|       - | 2213 | `			0 /* weak-mode arg binding, like call_user_func */, 0,` |
|       - | 2214 | `			FALSE/*Fiber::start(): php omits the call-site segment*/,` |
|       - | 2215 | `			FALSE/*php's Fiber::start() passes by VALUE and warns (§7.1)*/);` |
|       - | 2216 | `		/* Nothing to free: apValues aliases the operand stack now, it is not a` |
|       - | 2217 | `		 * buffer this function allocated. */` |
|       - | 2218 | `	}` |
|       - | 2219 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|     291 | 2220 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|     291 | 2221 | `	pExecCtx->pFrame->pParent = 0;` |
|     291 | 2222 | `	if( rc != SXRET_OK ){` |
|       - | 2223 | `		/* Propagate the real status: a declared-type TypeError from the arg` |
|       - | 2224 | `		 * install (band A #2) must stay catchable, not become an abort. */` |
|       5 | 2225 | `		return (rc == PH7_EXCEPTION) ? PH7_EXCEPTION : PH7_ABORT;` |
|       - | 2226 | `	}` |
|     287 | 2227 | `	PH7_MemObjInit(pVm, &sResult);` |
|     287 | 2228 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|     287 | 2229 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 2230 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 2231 | `		return PH7_ABORT;` |
|       - | 2232 | `	}` |
|     287 | 2233 | `	if( rc == PH7_EXCEPTION ){` |
|       6 | 2234 | `		PH7_MemObjRelease(&sResult);` |
|       6 | 2235 | `		return PH7_EXCEPTION;` |
|       - | 2236 | `	}` |
|     283 | 2237 | `	ph7_result_value(pCtx, &sResult);` |
|     283 | 2238 | `	PH7_MemObjRelease(&sResult);` |
|     283 | 2239 | `	return PH7_OK;` |
|     154 | 2240 | `}` |
|       - | 2241 | `/*` |
|       - | 2242 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|       - | 2243 | ` */` |
|     152 | 2244 | `PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2245 | `{` |
|     157 | 2246 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2247 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 2248 | `	ph7_value sResult;` |
|       - | 2249 | `	ph7_value *pResumeVal;` |
|       - | 2250 | `	sxi32 rc;` |
|     157 | 2251 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     157 | 2252 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|     157 | 2253 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2254 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|     ! 0 | 2255 | `		return PH7_OK;` |
|       - | 2256 | `	}` |
|     157 | 2257 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     157 | 2258 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 2259 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|     ! 0 | 2260 | `		return PH7_OK;` |
|       - | 2261 | `	}` |
|     157 | 2262 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       3 | 2263 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2264 | `			"Cannot resume a fiber that is not suspended");` |
|       - | 2265 | `	}` |
|     155 | 2266 | `	pResumeVal = (nArg > 0) ? apArg[0] : 0;` |
|     155 | 2267 | `	PH7_MemObjInit(pVm, &sResult);` |
|     155 | 2268 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|     155 | 2269 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 2270 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 2271 | `		return PH7_ABORT;` |
|       - | 2272 | `	}` |
|     155 | 2273 | `	if( rc == PH7_EXCEPTION ){` |
|       3 | 2274 | `		PH7_MemObjRelease(&sResult);` |
|       3 | 2275 | `		return PH7_EXCEPTION;` |
|       - | 2276 | `	}` |
|     153 | 2277 | `	ph7_result_value(pCtx, &sResult);` |
|     153 | 2278 | `	PH7_MemObjRelease(&sResult);` |
|     153 | 2279 | `	return PH7_OK;` |
|      81 | 2280 | `}` |
|       - | 2281 | `/*` |
|       - | 2282 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|       - | 2283 | ` */` |
|      42 | 2284 | `PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2285 | `{` |
|      47 | 2286 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2287 | `	ph7_exec_ctx *pExecCtx;` |
|      47 | 2288 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      21 | 2289 | `	SXUNUSED(apArg);` |
|      21 | 2290 | `	SXUNUSED(nArg);` |
|      47 | 2291 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      47 | 2292 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2293 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2294 | `		return PH7_OK;` |
|       - | 2295 | `	}` |
|      47 | 2296 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|      47 | 2297 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 2298 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2299 | `		return PH7_OK;` |
|       - | 2300 | `	}` |
|      47 | 2301 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|     ! 0 | 2302 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2303 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2304 | `				"Cannot get fiber return value: The fiber has not been started");` |
|       - | 2305 | `		}` |
|     ! 0 | 2306 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2307 | `			"Cannot get fiber return value: The fiber has not returned");` |
|       - | 2308 | `	}` |
|      47 | 2309 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|      47 | 2310 | `	return PH7_OK;` |
|      26 | 2311 | `}` |
|       - | 2312 | `/*` |
|       - | 2313 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|       - | 2314 | ` */` |
|       8 | 2315 | `PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 2316 | `{` |
|       - | 2317 | `	ph7_exec_ctx *pExecCtx;` |
|      10 | 2318 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       4 | 2319 | `	SXUNUSED(apArg);` |
|       4 | 2320 | `	SXUNUSED(nArg);` |
|      10 | 2321 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      10 | 2322 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|      10 | 2323 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|      10 | 2324 | `	return PH7_OK;` |
|       6 | 2325 | `}` |
|     ! 0 | 2326 | `PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     ! 0 | 2327 | `{` |
|       - | 2328 | `	ph7_exec_ctx *pExecCtx;` |
|     ! 0 | 2329 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     ! 0 | 2330 | `	SXUNUSED(apArg);` |
|     ! 0 | 2331 | `	SXUNUSED(nArg);` |
|     ! 0 | 2332 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     ! 0 | 2333 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|     ! 0 | 2334 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|     ! 0 | 2335 | `	return PH7_OK;` |
|     ! 0 | 2336 | `}` |
|     110 | 2337 | `PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 2338 | `{` |
|       - | 2339 | `	ph7_exec_ctx *pExecCtx;` |
|     113 | 2340 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      55 | 2341 | `	SXUNUSED(apArg);` |
|      55 | 2342 | `	SXUNUSED(nArg);` |
|     113 | 2343 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     113 | 2344 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|     113 | 2345 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|     113 | 2346 | `	return PH7_OK;` |
|      58 | 2347 | `}` |
|      26 | 2348 | `PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 2349 | `{` |
|       - | 2350 | `	ph7_exec_ctx *pExecCtx;` |
|      29 | 2351 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      13 | 2352 | `	SXUNUSED(apArg);` |
|      13 | 2353 | `	SXUNUSED(nArg);` |
|      29 | 2354 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      29 | 2355 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|      29 | 2356 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|      29 | 2357 | `	return PH7_OK;` |
|      16 | 2358 | `}` |
|       - | 2359 | `/*` |
|       - | 2360 | ` * Fiber->__destruct() — clean up the execution context.` |
|       - | 2361 | ` */` |
|     262 | 2362 | `PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 2363 | `{` |
|     266 | 2364 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2365 | `	ph7_exec_ctx *pExecCtx;` |
|     266 | 2366 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     131 | 2367 | `	SXUNUSED(apArg);` |
|     131 | 2368 | `	SXUNUSED(nArg);` |
|     266 | 2369 | `	if( pRecv == 0 ){` |
|     ! 0 | 2370 | `		return PH7_OK;` |
|       - | 2371 | `	}` |
|     266 | 2372 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     266 | 2373 | `	if( pExecCtx ){` |
|     241 | 2374 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|       - | 2375 | `		/* Clear the attribute so double-free is prevented */` |
|     241 | 2376 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|     241 | 2377 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 2378 | `			SyString sAttrName;` |
|       - | 2379 | `			ph7_value *pAttr;` |
|     241 | 2380 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     241 | 2381 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     241 | 2382 | `			if( pAttr ){` |
|     241 | 2383 | `				PH7_MemObjRelease(pAttr);` |
|     119 | 2384 | `			}` |
|     119 | 2385 | `		}` |
|     119 | 2386 | `	}` |
|     266 | 2387 | `	return PH7_OK;` |
|     135 | 2388 | `}` |
|       - | 2389 | `/* ======================== Fiber Public API Helpers ======================== */` |
|     ! 0 | 2390 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|     ! 0 | 2391 | `{` |
|       - | 2392 | `	ph7_class_instance *pThis;` |
|     ! 0 | 2393 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|     ! 0 | 2394 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 | 2395 | `	return pThis->pClass == pVm->pFiberClass;` |
|     ! 0 | 2396 | `}` |
|     ! 0 | 2397 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 2398 | `{` |
|       - | 2399 | `	ph7_class_instance *pThis;` |
|     ! 0 | 2400 | `	ph7_class_instance *pClosureThis = 0;` |
|       - | 2401 | `	ph7_exec_ctx *pCtx;` |
|       - | 2402 | `	ph7_vm_func *pFunc;` |
|       - | 2403 | `	ph7_value *pCallable;` |
|       - | 2404 | `	ph7_value *pCtxAttr;` |
|       - | 2405 | `	SyString sAttrName;` |
|       - | 2406 | `	sxi32 rc;` |
|       - | 2407 | `	/* Must not already be started */` |
|     ! 0 | 2408 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2409 | `	if( pCtx != 0 ){` |
|     ! 0 | 2410 | `		return SXERR_INVALID;` |
|       - | 2411 | `	}` |
|     ! 0 | 2412 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2413 | `		return SXERR_INVALID;` |
|       - | 2414 | `	}` |
|     ! 0 | 2415 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|       - | 2416 | `	/* Get the callable */` |
|     ! 0 | 2417 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     ! 0 | 2418 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 2419 | `	if( pCallable == 0 ){` |
|     ! 0 | 2420 | `		return SXERR_INVALID;` |
|       - | 2421 | `	}` |
|       - | 2422 | `	/* Resolve callable, through the same body-finder the PHP-level start() uses --` |
|       - | 2423 | `	 * these were two copies of one decision, and only the other one grew php's array` |
|       - | 2424 | `	 * and "Class::method" shapes. An embedder has no context to throw through, so the` |
|       - | 2425 | `	 * reason comes back as this entry point's own status. */` |
|     ! 0 | 2426 | `	if( pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP) ){` |
|     ! 0 | 2427 | `		const char *zWhy = 0;` |
|     ! 0 | 2428 | `		pFunc = VmFiberCallableBody(pVm, pCallable, &pClosureThis, &zWhy);` |
|     ! 0 | 2429 | `		if( pFunc == 0 ){` |
|     ! 0 | 2430 | `			return SXERR_NOTFOUND;` |
|     ! 0 | 2431 | `		}` |
|     ! 0 | 2432 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2433 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|     ! 0 | 2434 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 2435 | `			sizeof("__invoke") - 1);` |
|     ! 0 | 2436 | `		if( pMethod == 0 ){` |
|     ! 0 | 2437 | `			return SXERR_INVALID;` |
|       - | 2438 | `		}` |
|     ! 0 | 2439 | `		pClosureThis = pClosure;` |
|     ! 0 | 2440 | `		pFunc = &pMethod->sFunc;` |
|     ! 0 | 2441 | `	}else{` |
|     ! 0 | 2442 | `		return SXERR_INVALID;` |
|       - | 2443 | `	}` |
|       - | 2444 | `	/* Create context */` |
|     ! 0 | 2445 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|     ! 0 | 2446 | `	if( pCtx == 0 ){` |
|     ! 0 | 2447 | `		return SXERR_MEM;` |
|       - | 2448 | `	}` |
|       - | 2449 | `	/* Store in __ctx */` |
|     ! 0 | 2450 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     ! 0 | 2451 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 2452 | `	if( pCtxAttr ){` |
|     ! 0 | 2453 | `		pCtxAttr->x.pOther = pCtx;` |
|     ! 0 | 2454 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     ! 0 | 2455 | `	}` |
|       - | 2456 | `	/* Set up frame with args */` |
|     ! 0 | 2457 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     ! 0 | 2458 | `	pVm->pFrame = pCtx->pFrame;` |
|     ! 0 | 2459 | `	rc = VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg,` |
|       - | 2460 | `		0 /* weak-mode arg binding (embedder entry) */, 0,` |
|       - | 2461 | `		FALSE/*embedder entry: no userland call site*/,` |
|       - | 2462 | `		FALSE/*no source-level actuals to alias*/);` |
|     ! 0 | 2463 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|     ! 0 | 2464 | `	pCtx->pFrame->pParent = 0;` |
|     ! 0 | 2465 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2466 | `		return rc;` |
|       - | 2467 | `	}` |
|     ! 0 | 2468 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|     ! 0 | 2469 | `}` |
|     ! 0 | 2470 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 2471 | `{` |
|     ! 0 | 2472 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2473 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|     ! 0 | 2474 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|     ! 0 | 2475 | `}` |
|     ! 0 | 2476 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2477 | `{` |
|     ! 0 | 2478 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2479 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|     ! 0 | 2480 | `}` |
|     ! 0 | 2481 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2482 | `{` |
|     ! 0 | 2483 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2484 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|     ! 0 | 2485 | `}` |
|     ! 0 | 2486 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2487 | `{` |
|     ! 0 | 2488 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2489 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|     ! 0 | 2490 | `	return &pCtx->sRetValue;` |
|     ! 0 | 2491 | `}` |
|       - | 2492 | `/* ======================== Generator Infrastructure ======================== */` |
|       - | 2493 | `/*` |
|       - | 2494 | ` * Allocate a new generator wrapper around an execution context.` |
|       - | 2495 | ` */` |
|     408 | 2496 | `PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 | 2497 | `{` |
|       - | 2498 | `	ph7_generator *pGen;` |
|     413 | 2499 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|     413 | 2500 | `	if( pGen == 0 ){` |
|     ! 0 | 2501 | `		return 0;` |
|       - | 2502 | `	}` |
|     413 | 2503 | `	SyZero(pGen, sizeof(ph7_generator));` |
|     413 | 2504 | `	pGen->pCtx = pCtx;` |
|     413 | 2505 | `	pGen->iImplicitKey = 0;` |
|     413 | 2506 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|     413 | 2507 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|       - | 2508 | `	/* Link the generator back to the exec context */` |
|     413 | 2509 | `	pCtx->pPrivate = pGen;` |
|     413 | 2510 | `	return pGen;` |
|     209 | 2511 | `}` |
|       - | 2512 | `/*` |
|       - | 2513 | ` * Release a generator and its execution context.` |
|       - | 2514 | ` */` |
|     272 | 2515 | `PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|       5 | 2516 | `{` |
|     277 | 2517 | `	if( pGen == 0 ){` |
|     ! 0 | 2518 | `		return;` |
|       - | 2519 | `	}` |
|     277 | 2520 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|     277 | 2521 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|     277 | 2522 | `	if( pGen->pCtx ){` |
|     277 | 2523 | `		pGen->pCtx->pPrivate = 0;` |
|     277 | 2524 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|     277 | 2525 | `		pGen->pCtx = 0;` |
|     136 | 2526 | `	}` |
|     277 | 2527 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|     141 | 2528 | `}` |
|       - | 2529 | `/*` |
|       - | 2530 | ` * Extract ph7_generator from a Generator class instance.` |
|       - | 2531 | ` */` |
|    4324 | 2532 | `PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|       5 | 2533 | `{` |
|       - | 2534 | `	ph7_class_instance *pThis;` |
|       - | 2535 | `	SyString sAttr;` |
|       - | 2536 | `	ph7_value *pAttr;` |
|    4329 | 2537 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2538 | `		return 0;` |
|       - | 2539 | `	}` |
|    4329 | 2540 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|    4329 | 2541 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|     ! 0 | 2542 | `		return 0;` |
|       - | 2543 | `	}` |
|    4329 | 2544 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    4329 | 2545 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    4329 | 2546 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     ! 0 | 2547 | `		return 0;` |
|       - | 2548 | `	}` |
|    4329 | 2549 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    2167 | 2550 | `}` |
|       - | 2551 | `/*` |
|       - | 2552 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|       - | 2553 | ` */` |
|     224 | 2554 | `PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2555 | `{` |
|       - | 2556 | `	ph7_generator *pGen;` |
|       - | 2557 | `	sxi32 rc;` |
|     229 | 2558 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     112 | 2559 | `	SXUNUSED(apArg);` |
|     112 | 2560 | `	SXUNUSED(nArg);` |
|     229 | 2561 | `	if( pRecv == 0 ) return PH7_OK;` |
|     229 | 2562 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     229 | 2563 | `	if( pGen == 0 ) return PH7_OK;` |
|     229 | 2564 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     227 | 2565 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     227 | 2566 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     227 | 2567 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     108 | 2568 | `	}` |
|     223 | 2569 | `	return PH7_OK;` |
|     117 | 2570 | `}` |
|       - | 2571 | `/*` |
|       - | 2572 | ` * Generator::valid() — true if suspended at a yield point.` |
|       - | 2573 | ` */` |
|    1196 | 2574 | `PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2575 | `{` |
|       - | 2576 | `	ph7_generator *pGen;` |
|    1201 | 2577 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     598 | 2578 | `	SXUNUSED(apArg);` |
|     598 | 2579 | `	SXUNUSED(nArg);` |
|    1201 | 2580 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|    1201 | 2581 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1201 | 2582 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|    1201 | 2583 | `	return PH7_OK;` |
|     603 | 2584 | `}` |
|       - | 2585 | `/*` |
|       - | 2586 | ` * Generator::current() — return the last yielded value.` |
|       - | 2587 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 2588 | ` */` |
|    1198 | 2589 | `PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2590 | `{` |
|       - | 2591 | `	ph7_generator *pGen;` |
|       - | 2592 | `	sxi32 rc;` |
|    1203 | 2593 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     599 | 2594 | `	SXUNUSED(apArg);` |
|     599 | 2595 | `	SXUNUSED(nArg);` |
|    1203 | 2596 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1203 | 2597 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1203 | 2598 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1203 | 2599 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     151 | 2600 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     151 | 2601 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     151 | 2602 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      73 | 2603 | `	}` |
|    1203 | 2604 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|    1201 | 2605 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|     603 | 2606 | `	}else{` |
|       3 | 2607 | `		ph7_result_null(pCtx);` |
|       - | 2608 | `	}` |
|    1203 | 2609 | `	return PH7_OK;` |
|     604 | 2610 | `}` |
|       - | 2611 | `/*` |
|       - | 2612 | ` * Generator::key() — return the last yielded key.` |
|       - | 2613 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 2614 | ` */` |
|     206 | 2615 | `PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2616 | `{` |
|       - | 2617 | `	ph7_generator *pGen;` |
|       - | 2618 | `	sxi32 rc;` |
|     211 | 2619 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     103 | 2620 | `	SXUNUSED(apArg);` |
|     103 | 2621 | `	SXUNUSED(nArg);` |
|     211 | 2622 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     211 | 2623 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     211 | 2624 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     211 | 2625 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2626 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     ! 0 | 2627 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     ! 0 | 2628 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     ! 0 | 2629 | `	}` |
|     211 | 2630 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     211 | 2631 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|     108 | 2632 | `	}else{` |
|     ! 0 | 2633 | `		ph7_result_null(pCtx);` |
|       - | 2634 | `	}` |
|     211 | 2635 | `	return PH7_OK;` |
|     108 | 2636 | `}` |
|       - | 2637 | `/*` |
|       - | 2638 | ` * Generator::next() — advance to the next yield point.` |
|       - | 2639 | ` */` |
|     968 | 2640 | `PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2641 | `{` |
|       - | 2642 | `	ph7_generator *pGen;` |
|       - | 2643 | `	sxi32 rc;` |
|     973 | 2644 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     484 | 2645 | `	SXUNUSED(apArg);` |
|     484 | 2646 | `	SXUNUSED(nArg);` |
|     973 | 2647 | `	if( pRecv == 0 ) return PH7_OK;` |
|     973 | 2648 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     973 | 2649 | `	if( pGen == 0 ) return PH7_OK;` |
|     973 | 2650 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2651 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     973 | 2652 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     973 | 2653 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|     489 | 2654 | `	}else{` |
|     ! 0 | 2655 | `		return PH7_OK;` |
|       - | 2656 | `	}` |
|     973 | 2657 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     971 | 2658 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     961 | 2659 | `	return PH7_OK;` |
|     489 | 2660 | `}` |
|       - | 2661 | `/*` |
|       - | 2662 | ` * Generator::send($value) — resume and send a value into the generator.` |
|       - | 2663 | ` */` |
|     102 | 2664 | `PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2665 | `{` |
|       - | 2666 | `	ph7_generator *pGen;` |
|       - | 2667 | `	ph7_value *pSendVal;` |
|       - | 2668 | `	sxi32 rc;` |
|     107 | 2669 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     107 | 2670 | `	if( pRecv == 0 ) return PH7_OK;` |
|     107 | 2671 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     107 | 2672 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     107 | 2673 | `	pSendVal = (nArg > 0) ? apArg[0] : 0;` |
|     107 | 2674 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       - | 2675 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|     ! 0 | 2676 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|     107 | 2677 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     107 | 2678 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|      56 | 2679 | `	}else{` |
|     ! 0 | 2680 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2681 | `		return PH7_OK;` |
|       - | 2682 | `	}` |
|     107 | 2683 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     107 | 2684 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     104 | 2685 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      98 | 2686 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      51 | 2687 | `	}else{` |
|       7 | 2688 | `		ph7_result_null(pCtx);` |
|       - | 2689 | `	}` |
|     104 | 2690 | `	return PH7_OK;` |
|      56 | 2691 | `}` |
|       - | 2692 | `/*` |
|       - | 2693 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|       - | 2694 | ` *` |
|       - | 2695 | ` * PHP semantics: the exception is injected AT the suspended yield point so the` |
|       - | 2696 | ` * generator's OWN try/catch (if any wraps the yield) can handle it and the body` |
|       - | 2697 | ` * resumes after the try; otherwise it propagates to the throw() caller and the` |
|       - | 2698 | ` * generator closes. We implement this by resuming the body with a pending` |
|       - | 2699 | ` * injection (pCtx->pInjected) that the VM loop raises in the body's own frame via` |
|       - | 2700 | ` * the existing OP_THROW / ROOT B resume route — no exception frame is` |
|       - | 2701 | ` * reconstructed on pVm->pFrame, so the return/finally unwind path is untouched.` |
|       - | 2702 | ` * A never-started generator is first run to its first yield, then injected there;` |
|       - | 2703 | ` * a finished/closed generator has no suspend point, so the exception is simply` |
|       - | 2704 | ` * propagated to the caller (its return value stays readable via getReturn()).` |
|       - | 2705 | ` *` |
|       - | 2706 | ` * PHL does not enforce the Throwable parameter hint (interface/class hints are` |
|       - | 2707 | ` * not checked on call), so the Throwable validation — and its PHP TypeError —` |
|       - | 2708 | ` * are done here.` |
|       - | 2709 | ` */` |
|      62 | 2710 | `PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 2711 | `{` |
|       - | 2712 | `	ph7_generator *pGen;` |
|       - | 2713 | `	ph7_class_instance *pInj;` |
|       - | 2714 | `	ph7_class *pThrowable;` |
|       - | 2715 | `	VmFrame *pFrame;` |
|       - | 2716 | `	sxi32 rc;` |
|      66 | 2717 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      66 | 2718 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      66 | 2719 | `	if( nArg < 1 ) return PH7_OK;` |
|       - | 2720 | `	/* Argument #1 must be a Throwable; otherwise PHP raises a TypeError naming` |
|       - | 2721 | `	 * the given type (class name for objects, "null"/"string"/... for scalars). */` |
|      66 | 2722 | `	pThrowable = PH7_VmExtractClass(pCtx->pVm, "Throwable", sizeof("Throwable")-1, 0, 0);` |
|      62 | 2723 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|      66 | 2724 | `	 \|\| (pThrowable && !PH7_VmInstanceOf(((ph7_class_instance *)apArg[0]->x.pOther)->pClass, pThrowable)) ){` |
|       - | 2725 | `		char zCls[128];` |
|     ! 0 | 2726 | `		const char *zGiven = VmValueGivenName(apArg[0], zCls, sizeof(zCls));` |
|     ! 0 | 2727 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|     ! 0 | 2728 | `			"Generator::throw(): Argument #1 ($exception) must be of type Throwable, %s given", zGiven);` |
|       - | 2729 | `	}` |
|      66 | 2730 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      66 | 2731 | `	if( pGen == 0 ) return PH7_OK;` |
|       - | 2732 | `	/* PHP forbids resuming/throwing into a generator that is currently executing. */` |
|      66 | 2733 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       3 | 2734 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 2735 | `			"Cannot resume an already running generator");` |
|       - | 2736 | `	}` |
|       - | 2737 | `	/* Hold a reference to the injected instance for the whole operation: the VM loop` |
|       - | 2738 | `	 * (inject path) or VmThrowException (propagate path) may run catch blocks that bind` |
|       - | 2739 | `	 * and later release it. Dropped on every return path below. */` |
|      64 | 2740 | `	pInj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      64 | 2741 | `	pInj->iRef++;` |
|       - | 2742 | `	/* A never-started generator runs to its first yield, then the exception is injected` |
|       - | 2743 | `	 * there (PHP). Start it first; if it suspended at a yield, fall through to inject; if` |
|       - | 2744 | `	 * it ran to completion without yielding, drop through to the propagate path below. */` |
|      64 | 2745 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       5 | 2746 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       5 | 2747 | `		if( rc == PH7_ABORT ){ PH7_ClassInstanceUnref(pInj); return PH7_ABORT; }` |
|       5 | 2748 | `		if( rc == PH7_EXCEPTION ){ PH7_ClassInstanceUnref(pInj); return PH7_EXCEPTION; }` |
|       2 | 2749 | `	}` |
|      64 | 2750 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       - | 2751 | `		/* Inject at the suspended yield: the resume loop raises it in the body's own` |
|       - | 2752 | `		 * frame so the generator's try/catch can catch it and resume (path 2). */` |
|      58 | 2753 | `		pGen->pCtx->pInjected = pInj;   /* borrowed; ref held here across the resume */` |
|      58 | 2754 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       - | 2755 | `		/* Normally the inject was consumed (cleared) at VmByteCodeExec entry; clear it` |
|       - | 2756 | `		 * here too for the path where VmResumeCtx bails BEFORE entering the loop (e.g. the` |
|       - | 2757 | `		 * recursion-depth fatal), so no dangling borrowed pointer survives the Unref. */` |
|      58 | 2758 | `		pGen->pCtx->pInjected = 0;` |
|      58 | 2759 | `		PH7_ClassInstanceUnref(pInj);` |
|      58 | 2760 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      58 | 2761 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       - | 2762 | `		/* Caught inside the generator and it resumed: return the next yielded value (or` |
|       - | 2763 | `		 * null if it then completed) — symmetric with Generator::send(). */` |
|      46 | 2764 | `		if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      44 | 2765 | `			ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      24 | 2766 | `		}else{` |
|       3 | 2767 | `			ph7_result_null(pCtx);` |
|       - | 2768 | `		}` |
|      46 | 2769 | `		return PH7_OK;` |
|       - | 2770 | `	}` |
|       - | 2771 | `	/* COMPLETED/CLOSED (incl. a CREATED generator that ran to completion without a` |
|       - | 2772 | `	 * yield): no suspend point to inject at. Propagate the real object to the throw()` |
|       - | 2773 | `	 * caller through the normal dispatch path (class/message/trace preserved); its` |
|       - | 2774 | `	 * terminal state — and thus getReturn() — is left intact. */` |
|       8 | 2775 | `	pFrame = pCtx->pVm->pFrame;` |
|       8 | 2776 | `	if( pFrame ){` |
|       8 | 2777 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       8 | 2778 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       3 | 2779 | `	}` |
|       8 | 2780 | `	rc = VmThrowException(pCtx->pVm, pInj);` |
|       8 | 2781 | `	PH7_ClassInstanceUnref(pInj);` |
|       8 | 2782 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2783 | `		return PH7_ABORT;` |
|       - | 2784 | `	}` |
|       8 | 2785 | `	return PH7_EXCEPTION;` |
|      35 | 2786 | `}` |
|       - | 2787 | `/*` |
|       - | 2788 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|       - | 2789 | ` */` |
|      24 | 2790 | `PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2791 | `{` |
|       - | 2792 | `	ph7_generator *pGen;` |
|      29 | 2793 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      12 | 2794 | `	SXUNUSED(apArg);` |
|      12 | 2795 | `	SXUNUSED(nArg);` |
|      29 | 2796 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      29 | 2797 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      29 | 2798 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      29 | 2799 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|     ! 0 | 2800 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 2801 | `			"Cannot get return value of a generator that hasn't returned");` |
|       - | 2802 | `	}` |
|      29 | 2803 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|      29 | 2804 | `	return PH7_OK;` |
|      17 | 2805 | `}` |
|       - | 2806 | `/*` |
|       - | 2807 | ` * Generator::__destruct() — clean up.` |
|       - | 2808 | ` */` |
|     256 | 2809 | `PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2810 | `{` |
|       - | 2811 | `	ph7_generator *pGen;` |
|     261 | 2812 | `	sxi32 rcClose = SXRET_OK;` |
|     261 | 2813 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     128 | 2814 | `	SXUNUSED(apArg);` |
|     128 | 2815 | `	SXUNUSED(nArg);` |
|     261 | 2816 | `	if( pRecv == 0 ) return PH7_OK;` |
|     261 | 2817 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     261 | 2818 | `	if( pGen ){` |
|       - | 2819 | `` 		/* A generator abandoned before it completes still runs its pending `finally` `` |
|       - | 2820 | `		 * blocks (PHP runs them at generator close/GC). Drive them before teardown. */` |
|     261 | 2821 | `		if( pGen->pCtx ){` |
|     261 | 2822 | `			rcClose = VmCloseCtx(pCtx->pVm, pGen->pCtx);` |
|     128 | 2823 | `		}` |
|     261 | 2824 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|     261 | 2825 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|     261 | 2826 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 2827 | `			SyString sAttrName;` |
|       - | 2828 | `			ph7_value *pAttr;` |
|     261 | 2829 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     261 | 2830 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     261 | 2831 | `			if( pAttr ){` |
|     261 | 2832 | `				PH7_MemObjRelease(pAttr);` |
|     128 | 2833 | `			}` |
|     128 | 2834 | `		}` |
|     128 | 2835 | `	}` |
|       - | 2836 | `	/* Surface an abort/exception raised by a finally that ran during close. */` |
|     261 | 2837 | `	if( rcClose == PH7_ABORT ) return PH7_ABORT;` |
|     261 | 2838 | `	if( rcClose == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     261 | 2839 | `	return PH7_OK;` |
|     133 | 2840 | `}` |
|       - | 2841 | `/* ======================== End Generator Infrastructure ======================== */` |
|       - | 2842 | `/* ======================== End Fiber Infrastructure ======================== */` |
|       - | 2843 |  |
