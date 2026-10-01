# src/ph7/vm_exec_ctx.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1470/1710 lines (85.96%)

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
|     972 |   20 | `PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|       5 |   21 | `{` |
|       - |   22 | `	ph7_exec_ctx *pCtx;` |
|       - |   23 | `	ph7_value *pStack;` |
|       - |   24 | `	VmFrame *pFrame;` |
|     977 |   25 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|     977 |   26 | `	if( pCtx == 0 ){` |
|     ! 0 |   27 | `		return 0;` |
|       - |   28 | `	}` |
|     977 |   29 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|     977 |   30 | `	pCtx->pVm = pVm;` |
|     977 |   31 | `	pCtx->pFunc = pFunc;` |
|       - |   32 | `	/* A coroutine outlives the call that made it and reads pFunc for the whole of` |
|       - |   33 | `	 * its life -- including before its body frame exists, which VmStartCtx creates` |
|       - |   34 | `	 * LAZILY. For a run-time closure that is a hold of its own on the` |
|       - |   35 | ``	 * per-instantiation copy: `(function(){ yield 1; })()` drops the Closure object`` |
|       - |   36 | `	 * at the call, and without this the body was freed under the Generator that` |
|       - |   37 | `	 * still names it. */` |
|     977 |   38 | `	PH7_VmClosureFuncRef(pFunc);` |
|     977 |   39 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|     977 |   40 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|     977 |   41 | `	pCtx->pc = 0;` |
|     977 |   42 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|     977 |   43 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|     977 |   44 | `	PH7_MemObjInit(pVm, &pCtx->sDelegate);` |
|       - |   45 | `	/* Container for this body's own exception handlers while suspended (borrowed` |
|       - |   46 | `	 * ph7_exception* pointers — never freed here, owned by the compiled func). */` |
|     977 |   47 | `	SySetInit(&pCtx->aSavedException, &pVm->sAllocator, sizeof(ph7_exception *));` |
|       - |   48 | `	/* ROOT C: this body's own pending finally actions while suspended. */` |
|     977 |   49 | `	SySetInit(&pCtx->aSavedFinally, &pVm->sAllocator, sizeof(VmFinallyAction));` |
|     977 |   50 | `	pCtx->nFinallyBase = 0;` |
|       - |   51 | `	/* Stage 4: this coroutine's own aSelf entries (self::/static:: class context` |
|       - |   52 | `	 * pushed by nested method calls still open at suspend) parked while suspended,` |
|       - |   53 | `	 * so they don't pollute the resumer's aSelf. Borrowed ph7_class* pointers. */` |
|     977 |   54 | `	SySetInit(&pCtx->aSavedSelf, &pVm->sAllocator, sizeof(ph7_class *));` |
|     977 |   55 | `	pCtx->nSelfBase = 0;` |
|       - |   56 | ``	/* The class this body's `static::` means, taken from the CALL that is creating it`` |
|       - |   57 | `	 * (VmStartCtx pushes it back for the body's duration). */` |
|     977 |   58 | `	pCtx->pLsbClass = PH7_VmPeekTopClass(pVm);` |
|       - |   59 | `	/* Caller slots this body's by-reference parameters alias (see the struct). */` |
|     977 |   60 | `	SySetInit(&pCtx->aByRefArg, &pVm->sAllocator, sizeof(sxu32));` |
|     977 |   61 | `	pCtx->pParkedSegment = 0;` |
|     977 |   62 | `	pCtx->nBodyExecDepth = 0;` |
|       - |   63 | `	/* Allocate a private operand stack */` |
|     977 |   64 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|     977 |   65 | `	if( pStack == 0 ){` |
|     ! 0 |   66 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   67 | `		return 0;` |
|       - |   68 | `	}` |
|     977 |   69 | `	pCtx->pStack = pStack;` |
|     977 |   70 | `	pCtx->nStackCap = SySetUsed(&pFunc->aByteCode) + VM_STACK_GUARD; /* grows with pStack on OP_SPREAD */` |
|     977 |   71 | `	pCtx->nStackOrig = pCtx->nStackCap; /* fixed original — headroom reference across resumes */` |
|       - |   72 | `	/* Create a detached frame for the fiber */` |
|     977 |   73 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|     977 |   74 | `	if( pFrame == 0 ){` |
|     ! 0 |   75 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|     ! 0 |   76 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |   77 | `		return 0;` |
|       - |   78 | `	}` |
|     977 |   79 | `	pCtx->pFrame = pFrame;` |
|     977 |   80 | `	return pCtx;` |
|     491 |   81 | `}` |
|       - |   82 | `/*` |
|       - |   83 | ` * A suspended coroutine must not leave its own slices of the VM's shared stacks` |
|       - |   84 | ` * sitting above the caller's depth. Three stacks are affected, identically:` |
|       - |   85 | ` *   - pVm->aException: its exception handlers — else a generator/fiber suspended` |
|       - |   86 | ` *     inside a try leaves handlers referencing its now-detached frame on the` |
|       - |   87 | ` *     global stack, corrupting the caller's try/catch.` |
|       - |   88 | ` *   - pVm->aFinallyAction (ROOT C): its pending finally actions — else a yield` |
|       - |   89 | ` *     inside a finally (reached by return/break/rethrow) leaves a record where an` |
|       - |   90 | ` *     out-of-order-resumed sibling generator's OP_END_FINALLY would mis-pop it.` |
|       - |   91 | ` *   - pVm->aSelf (stage 4): its self::/static:: entries pushed by still-open` |
|       - |   92 | ` *     nested method calls — else they sit on the resumer's aSelf and corrupt its` |
|       - |   93 | ` *     self:: resolution.` |
|       - |   94 | ` * Each is the same operation: on suspend move the slice above a captured base` |
|       - |   95 | ` * into a per-ctx park buffer; on resume re-publish it at the (refreshed) caller` |
|       - |   96 | ` * depth. VmParkStackSlice / VmRestoreStackSlice factor it for any element type` |
|       - |   97 | ` * (size taken from the SySet); VmParkCtxState / VmRestoreCtxState drive all three.` |
|       - |   98 | ` *` |
|       - |   99 | ` * Stage 4: the whole suspended segment stays alive, so a parked handler's owner` |
|       - |  100 | ` * frame is never freed underneath it — the parked pointer stays valid and is kept` |
|       - |  101 | ` * (the old stage-2b lossy-path invalidation is gone with the discard). A` |
|       - |  102 | ` * body-level suspend only ever has body-owned handlers here, and its finally/self` |
|       - |  103 | ` * slices are empty (all nested calls already returned) — so those are no-ops.` |
|       - |  104 | ` */` |
|    6006 |  105 | `static void VmParkStackSlice(SySet *pFrom, SySet *pSaved, sxu32 nBase)` |
|       5 |  106 | `{` |
|    6011 |  107 | `	sxu32 nUsed = SySetUsed(pFrom);` |
|    6011 |  108 | `	if( nUsed > nBase ){` |
|     287 |  109 | `		const char *aBase = (const char *)SySetBasePtr(pFrom);` |
|       - |  110 | `		sxu32 i;` |
|     579 |  111 | `		for( i = nBase; i < nUsed; i++ ){` |
|     297 |  112 | `			SySetPut(pSaved, (const void *)(aBase + i * pFrom->eSize));` |
|     151 |  113 | `		}` |
|     287 |  114 | `		SySetTruncate(pFrom, nBase);` |
|     141 |  115 | `	}` |
|    6011 |  116 | `}` |
|    5358 |  117 | `static void VmRestoreStackSlice(SySet *pTo, SySet *pSaved)` |
|       5 |  118 | `{` |
|    5363 |  119 | `	sxu32 i, n = SySetUsed(pSaved);` |
|    5363 |  120 | `	if( n > 0 ){` |
|     287 |  121 | `		const char *aSaved = (const char *)SySetBasePtr(pSaved);` |
|     579 |  122 | `		for( i = 0; i < n; i++ ){` |
|     297 |  123 | `			SySetPut(pTo, (const void *)(aSaved + i * pSaved->eSize));` |
|     151 |  124 | `		}` |
|     287 |  125 | `		SySetReset(pSaved);` |
|     141 |  126 | `	}` |
|    5363 |  127 | `}` |
|    2002 |  128 | `static void VmParkCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  129 | `{` |
|    2007 |  130 | `	VmParkStackSlice(&pVm->aException, &pCtx->aSavedException, pCtx->nExceptionBase);` |
|    2007 |  131 | `	VmParkStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally, pCtx->nFinallyBase);` |
|    2007 |  132 | `	VmParkStackSlice(&pVm->aSelf, &pCtx->aSavedSelf, pCtx->nSelfBase);` |
|    2007 |  133 | `}` |
|    1786 |  134 | `static void VmRestoreCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  135 | `{` |
|    1791 |  136 | `	VmRestoreStackSlice(&pVm->aException, &pCtx->aSavedException);` |
|    1791 |  137 | `	VmRestoreStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally);` |
|    1791 |  138 | `	VmRestoreStackSlice(&pVm->aSelf, &pCtx->aSavedSelf);` |
|    1791 |  139 | `}` |
|       - |  140 | `/*` |
|       - |  141 | ` * On suspend, free the exception (try) frames the yield was nested in. They were` |
|       - |  142 | ` * pushed by OP_LOAD_EXCEPTION between the coroutine body frame (pCtx->pFrame) and` |
|       - |  143 | ` * the current suspend-point top frame. The generator/fiber frame model saves only` |
|       - |  144 | ` * the body frame, so these transparent wrappers would otherwise be orphaned and` |
|       - |  145 | ` * leak on every yield-that-sits-inside-a-try (unbounded for a generator looping` |
|       - |  146 | ` * with a yield in a try). Freeing them loses nothing the resume needs: this body's` |
|       - |  147 | ` * exception HANDLERS are parked separately (VmParkCtxState) and each` |
|       - |  148 | ` * try's landing pad lives on its ph7_exception (iLandingPc), while OP_POP_EXCEPTION` |
|       - |  149 | ` * on resume skips the (now absent) frame pop via its VM_FRAME_EXCEPTION guard and` |
|       - |  150 | ` * OP_LOAD_EXCEPTION re-creates a fresh wrapper when the try is next entered. Must` |
|       - |  151 | ` * run while pVm->pFrame still points at the suspend-time top (before the detach).` |
|       - |  152 | ` */` |
|    2396 |  153 | `static void VmFreeSuspendedExceptionFrames(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  154 | `{` |
|    2617 |  155 | `	while( pVm->pFrame != pCtx->pFrame && (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     221 |  156 | `		VmLeaveFrame(&(*pVm));` |
|       5 |  157 | `	}` |
|    2401 |  158 | `}` |
|       - |  159 | `/*` |
|       - |  160 | ` * Stamp a coroutine body frame with the site that is starting or RESUMING it.` |
|       - |  161 | ` *` |
|       - |  162 | ` * An ordinary frame gets this in VmEnterFrame; a coroutine's body frame is built` |
|       - |  163 | ` * detached (VmNewExecCtx -> VmNewFrame) and never went through it, so a backtrace` |
|       - |  164 | ` * taken inside a generator reported the frame below it at line 0 -- printed as` |
|       - |  165 | ` * line 1, in whatever file the include stack happened to top out at. php answers` |
|       - |  166 | ``  * the CURRENT resume site rather than the creation site (`foreach (g() as $v)` `` |
|       - |  167 | `` * for the first step, the `yield from` line for a delegate), which is exactly`` |
|       - |  168 | ` * what this reads, so it is stamped on every start and resume rather than once.` |
|       - |  169 | ` * Must run BEFORE the frame is spliced onto the chain: the site is the resumer's.` |
|       - |  170 | ` */` |
|    2714 |  171 | `static void VmStampCoroutineCallSite(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  172 | `{` |
|       - |  173 | `	SyString *pFile;` |
|    2719 |  174 | `	if( pCtx->pFrame == 0 ){` |
|     ! 0 |  175 | `		return;` |
|       - |  176 | `	}` |
|    2719 |  177 | `	pCtx->pFrame->nCallLine = pVm->nCurLine;` |
|    2719 |  178 | `	pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|    2719 |  179 | `	if( pFile ){` |
|    2719 |  180 | `		pCtx->pFrame->sCallFile = *pFile;` |
|    1357 |  181 | `	}` |
|    1362 |  182 | `}` |
|       - |  183 | `/*` |
|       - |  184 | ` * Common suspend epilogue for VmStartCtx / VmResumeCtx: detach the suspended` |
|       - |  185 | ` * coroutine from the live VM chain and park its exception handlers. Two forms:` |
|       - |  186 | ` *   - Body-level (pParkedSegment == 0): a generator yield or a fiber suspending` |
|       - |  187 | ` *     directly in its body. The try wrappers the yield sat in are transient —` |
|       - |  188 | ` *     free them (OP_LOAD_EXCEPTION recreates them on re-entry) — and detach the` |
|       - |  189 | ` *     body frame alone.` |
|       - |  190 | ` *   - Deep fiber suspend (pParkedSegment != 0, stage 4): the whole segment (body` |
|       - |  191 | ` *     frame + the nested call/try frames above it) stays alive and is detached` |
|       - |  192 | ` *     as a unit; nothing is freed, so resume can continue inside the innermost` |
|       - |  193 | ` *     callee. Its handlers are parked the same way and rebased on resume.` |
|       - |  194 | ` */` |
|    2002 |  195 | `static void VmSuspendCtxDetach(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  196 | `{` |
|    2007 |  197 | `	if( pCtx->pParkedSegment == 0 ){` |
|    1689 |  198 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     847 |  199 | `	}else{` |
|       - |  200 | `		/* The parked records' push-time accounting (one nRecursionDepth++ each,` |
|       - |  201 | `		 * plus VmCallFinish's aSelf pop, which never ran) leaves the segment` |
|       - |  202 | `		 * counted as active while the fiber is suspended — deactivate it. aSelf` |
|       - |  203 | `		 * is parked wholesale below (base-relative), so drop only the depth. */` |
|     323 |  204 | `		pVm->nRecursionDepth -= ((VmParkedSegment *)pCtx->pParkedSegment)->nRecords;` |
|       - |  205 | `	}` |
|    2007 |  206 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|    2007 |  207 | `	pCtx->pFrame->pParent = 0;` |
|    2007 |  208 | `	VmParkCtxState(pVm, pCtx);` |
|    2007 |  209 | `	if( pResult ){` |
|     395 |  210 | `		PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|     195 |  211 | `	}` |
|    2007 |  212 | `}` |
|       - |  213 | `/*` |
|       - |  214 | ` * The return-type enforcement target for a coroutine body run. A GENERATOR` |
|       - |  215 | ` * function's declared return type belongs to the call site (always a Generator` |
|       - |  216 | ` * object, validated at compile time as "a supertype of Generator"); the body's` |
|       - |  217 | ` * own return value feeds getReturn() and is never type-checked. Gate on the` |
|       - |  218 | ` * VM_FUNC_GENERATOR flag (the semantic property), not pPrivate (a wrapper-linkage` |
|       - |  219 | ` * fact): a Fiber given a generator-flagged callable runs with pPrivate == 0 and` |
|       - |  220 | ` * must not enforce either. Ordinary fiber callables keep their declared` |
|       - |  221 | ` * return-type enforcement (php enforces it).` |
|       - |  222 | ` */` |
|    2714 |  223 | `static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx)` |
|       5 |  224 | `{` |
|    1610 |  225 | `	return ((pCtx->pFunc->iFlags & VM_FUNC_GENERATOR) == 0 && VmFuncHasReturnType(pCtx->pFunc))` |
|    1605 |  226 | `		? pCtx->pFunc : 0;` |
|       5 |  227 | `}` |
|       - |  228 | `/*` |
|       - |  229 | ` * Common epilogue for VmStartCtx / VmResumeCtx once the body run returns rc:` |
|       - |  230 | ` * restore the previous active context, then park on suspend or detach the` |
|       - |  231 | ` * coroutine frame and record the terminal state. On normal completion the value` |
|       - |  232 | ` * belongs to getReturn() only — start()/resume() return the NEXT suspend value,` |
|       - |  233 | ` * which is null at completion (php parity), so pResult is left at its` |
|       - |  234 | ` * caller-initialized null.` |
|       - |  235 | ` */` |
|    2714 |  236 | `static sxi32 VmFinishCtxRun(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_exec_ctx *pOldCtx,` |
|       - |  237 | `	sxi32 rc, ph7_value *pResult)` |
|       5 |  238 | `{` |
|    2719 |  239 | `	pVm->pActiveCtx = pOldCtx;` |
|    2719 |  240 | `	if( rc == PH7_SUSPEND ){` |
|       - |  241 | `		/* Handles a deep RE-suspend too (a resumed segment parking a fresh one):` |
|       - |  242 | `		 * VmSuspendCtxDetach deactivates the new segment's recursion accounting,` |
|       - |  243 | `		 * parks its aSelf, and skips VmFreeSuspendedExceptionFrames for a segment` |
|       - |  244 | `		 * so it can't free the still-live parked try wrappers. */` |
|    2007 |  245 | `		VmSuspendCtxDetach(pVm, pCtx, pResult);` |
|    2007 |  246 | `		return SXRET_OK;` |
|       - |  247 | `	}` |
|       - |  248 | ``	/* A finally entered via the throw redirect whose `return` short-circuited`` |
|       - |  249 | `	 * OP_END_FINALLY leaves the try's transparent wrapper ABOVE the body frame —` |
|       - |  250 | `	 * the detach below would then be skipped and the wrapper (plus the body` |
|       - |  251 | `	 * frame) leak into the RESUMER's frame chain, so the next try at that scope` |
|       - |  252 | `	 * records the wrong owner frame and its caught throw silently unwinds the` |
|       - |  253 | `	 * script. Free trailing exception wrappers exactly like the suspend path. */` |
|     717 |  254 | `	if( pCtx->pParkedSegment == 0 ){` |
|     717 |  255 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     356 |  256 | `	}` |
|       - |  257 | `	/* Detach the coroutine frame from the live chain, unless a deeper unwind` |
|       - |  258 | `	 * already moved pVm->pFrame off it. */` |
|     717 |  259 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|     717 |  260 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|     717 |  261 | `		pCtx->pFrame->pParent = 0;` |
|     356 |  262 | `	}` |
|       - |  263 | `	/* The body is over (it did not suspend): drop whatever it left on the shared` |
|       - |  264 | `	 * self stack, which is at least the LSB class VmStartCtx published for it. */` |
|     717 |  265 | `	if( SySetUsed(&pVm->aSelf) > pCtx->nSelfBase ){` |
|      36 |  266 | `		SySetTruncate(&pVm->aSelf, pCtx->nSelfBase);` |
|      16 |  267 | `	}` |
|     717 |  268 | `	if( rc == PH7_ABORT ){` |
|       3 |  269 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       3 |  270 | `		return PH7_ABORT;` |
|       - |  271 | `	}` |
|     715 |  272 | `	if( rc == PH7_EXCEPTION ){` |
|      75 |  273 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      75 |  274 | `		pCtx->bThrew = 1;` |
|      75 |  275 | `		return PH7_EXCEPTION;` |
|       - |  276 | `	}` |
|     645 |  277 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|     645 |  278 | `	return SXRET_OK;` |
|    1362 |  279 | `}` |
|       - |  280 | `/*` |
|       - |  281 | ` * Start executing a fiber context for the first time.` |
|       - |  282 | ` */` |
|     928 |  283 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  284 | `{` |
|       - |  285 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  286 | `	sxi32 rc;` |
|     933 |  287 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|     ! 0 |  288 | `		return SXERR_INVALID;` |
|       - |  289 | `	}` |
|       - |  290 | `	/* A fiber/generator start is a native VmByteCodeExec re-entry, bounded by` |
|       - |  291 | `	 * nMaxNativeDepth. Reject HERE, before attaching the frame / mutating VM` |
|       - |  292 | `	 * state, so the abort is clean — the wrapper's own check fires only after` |
|       - |  293 | `	 * this function has spliced the coroutine into the frame chain, which its` |
|       - |  294 | `	 * post-exec detach cannot fully unwind. The PHP call-depth cap belongs to` |
|       - |  295 | `	 * OP_CALL only (BYTECODE.md stage 5). */` |
|     933 |  296 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  297 | `		return VmNativeNestingFatal(pVm);` |
|       - |  298 | `	}` |
|       - |  299 | `	/* Attach the fiber's frame to the VM frame chain */` |
|     933 |  300 | `	VmStampCoroutineCallSite(pVm, pCtx);` |
|     933 |  301 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     933 |  302 | `	pVm->pFrame = pCtx->pFrame;` |
|       - |  303 | `	/* Save and set the active context */` |
|     933 |  304 | `	pOldCtx = pVm->pActiveCtx;` |
|     933 |  305 | `	pVm->pActiveCtx = pCtx;` |
|     933 |  306 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|     933 |  307 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|     933 |  308 | `	pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|     933 |  309 | `	pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|       - |  310 | `	/* Re-publish the creating call's late-static-binding class ABOVE that base, so the` |
|       - |  311 | ``	 * body's `static::` resolves to what php resolves it to. It rides the ordinary`` |
|       - |  312 | `	 * park/restore of this coroutine's own aSelf slice, so a suspend takes it off the` |
|       - |  313 | `	 * shared stack and a resume puts it back; VmFinishCtxRun truncates it away when the` |
|       - |  314 | `	 * body ends for good. */` |
|     933 |  315 | `	if( pCtx->pLsbClass ){` |
|      36 |  316 | `		SySetPut(&pVm->aSelf,(const void *)&pCtx->pLsbClass);` |
|      16 |  317 | `	}` |
|       - |  318 | `	/* Native depth the body runs at (the VmByteCodeExec wrapper bumps +1): a` |
|       - |  319 | `	 * Fiber::suspend() at a deeper depth is inside a C->PHP callback and gets a` |
|       - |  320 | `	 * FiberError instead of parking across the native frame (stage 4). */` |
|     933 |  321 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1;` |
|       - |  322 | `	/* Execute from the beginning (no parked segment). An OP_SPREAD in the body may` |
|       - |  323 | `	 * realloc pCtx->pStack — pass &pCtx->pStack / &pCtx->nStackCap so the grown` |
|       - |  324 | `	 * buffer + capacity persist for the next resume and for ctx teardown. */` |
|    1397 |  325 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     464 |  326 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|     464 |  327 | `		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|     933 |  328 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     469 |  329 | `}` |
|       - |  330 | `/*` |
|       - |  331 | ` * Resume a suspended fiber context.` |
|       - |  332 | ` */` |
|    1786 |  333 | `PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|       5 |  334 | `{` |
|       - |  335 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  336 | `	VmParkedSegment *pSeg;` |
|       - |  337 | `	sxi32 rc;` |
|    1791 |  338 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|     ! 0 |  339 | `		return SXERR_INVALID;` |
|       - |  340 | `	}` |
|       - |  341 | `	/* A resume is a native VmByteCodeExec re-entry, bounded by nMaxNativeDepth.` |
|       - |  342 | `	 * Reject HERE, before re-attaching the (possibly deep) parked segment to the` |
|       - |  343 | `	 * frame chain and re-adding its nRecords to nRecursionDepth — a wrapper-level` |
|       - |  344 | `	 * abort past those mutations would leave pVm->pFrame pointing into the parked` |
|       - |  345 | `	 * callee and the depth accounting un-reverted. The PHP call-depth cap is` |
|       - |  346 | `	 * OP_CALL-only (BYTECODE.md stage 5). */` |
|    1791 |  347 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  348 | `		return VmNativeNestingFatal(pVm);` |
|       - |  349 | `	}` |
|       - |  350 | `	/* Push the resume value onto the SUSPENDED activation's operand stack so it` |
|       - |  351 | `	 * appears as Fiber::suspend()'s return value. For a deep suspend (stage 4)` |
|       - |  352 | `	 * that stack is the innermost callee's — parked in the segment — not the` |
|       - |  353 | `	 * body's. nTos was saved one below the return-value slot. */` |
|       - |  354 | `	{` |
|       - |  355 | `		ph7_value *pResumeStack;` |
|    1791 |  356 | `		pSeg = (VmParkedSegment *)pCtx->pParkedSegment;` |
|    1791 |  357 | `		pResumeStack = pSeg ? pSeg->sState.pStack : pCtx->pStack;` |
|    1791 |  358 | `		if( pResumeValue ){` |
|     325 |  359 | `			PH7_MemObjStore(pResumeValue, &pResumeStack[pCtx->nTos + 1]);` |
|     165 |  360 | `		}else{` |
|    1471 |  361 | `			PH7_MemObjRelease(&pResumeStack[pCtx->nTos + 1]);` |
|       - |  362 | `		}` |
|    1791 |  363 | `		pCtx->nTos++;` |
|       - |  364 | `		/* Refresh the caller-depth base and re-publish this body's own exception` |
|       - |  365 | `		 * handlers on top of pVm->aException at that depth, so the resumed body's` |
|       - |  366 | `		 * try/catch and finally-drain bound line up (see VmByteCodeExec's base` |
|       - |  367 | `		 * override). Must run before VmByteCodeExec recaptures its local base. */` |
|    1791 |  368 | `		pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|    1791 |  369 | `		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|    1791 |  370 | `		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|    1791 |  371 | `		VmRestoreCtxState(pVm, pCtx);` |
|    1791 |  372 | `		if( pSeg ){` |
|       - |  373 | `			/* Reactivate the parked records' recursion accounting (mirror of the` |
|       - |  374 | `			 * deactivate at suspend); aSelf was just restored above. */` |
|     123 |  375 | `			pVm->nRecursionDepth += pSeg->nRecords;` |
|       - |  376 | `			/* Rebase the parked segment's absolute exception-floor indices: the` |
|       - |  377 | `			 * fiber may resume at a different caller depth than it suspended at,` |
|       - |  378 | `			 * so every activation's nExceptionBase shifts by the same delta the` |
|       - |  379 | `			 * republished handlers moved (newBase - the park-time base). */` |
|     123 |  380 | `			sxi32 iDelta = (sxi32)pCtx->nExceptionBase - (sxi32)pSeg->nOldExcBase;` |
|       - |  381 | `			/* nFinallyActBase floors rebase by their OWN delta — the exception and` |
|       - |  382 | `			 * finally-action stacks move independently between suspend and resume` |
|       - |  383 | `			 * (a fiber resumed from inside a generator's inline finally sees a` |
|       - |  384 | `			 * DEEPER aFinallyAction with an unchanged aException, and a stale` |
|       - |  385 | `			 * absolute floor would make the activation-end discard eat the` |
|       - |  386 | `			 * resumer's pending action). */` |
|     123 |  387 | `			sxi32 iFinDelta = (sxi32)pCtx->nFinallyBase - (sxi32)pSeg->nOldFinBase;` |
|     123 |  388 | `			if( iDelta != 0 \|\| iFinDelta != 0 ){` |
|       - |  389 | `				VmCallFrame *pRec;` |
|     ! 0 |  390 | `				pSeg->sState.nExceptionBase =` |
|     ! 0 |  391 | `					(sxu32)((sxi32)pSeg->sState.nExceptionBase + iDelta);` |
|     ! 0 |  392 | `				pSeg->sState.nFinallyActBase =` |
|     ! 0 |  393 | `					(sxu32)((sxi32)pSeg->sState.nFinallyActBase + iFinDelta);` |
|     ! 0 |  394 | `				for( pRec = pSeg->pCallTop; pRec; pRec = pRec->pPrev ){` |
|     ! 0 |  395 | `					pRec->sCaller.nExceptionBase =` |
|     ! 0 |  396 | `						(sxu32)((sxi32)pRec->sCaller.nExceptionBase + iDelta);` |
|     ! 0 |  397 | `					pRec->sCaller.nFinallyActBase =` |
|     ! 0 |  398 | `						(sxu32)((sxi32)pRec->sCaller.nFinallyActBase + iFinDelta);` |
|     ! 0 |  399 | `				}` |
|     ! 0 |  400 | `			}` |
|      59 |  401 | `		}` |
|       - |  402 | `		/* Re-attach the coroutine to the live VM frame chain: the body frame's` |
|       - |  403 | `		 * parent becomes the resumer's current frame. For a deep segment the` |
|       - |  404 | `		 * suspend-time top frame (the innermost callee / open-try wrapper) then` |
|       - |  405 | `		 * becomes current so the adopt at VmByteCodeExec entry resumes inside the` |
|       - |  406 | `		 * callee; body-level resumes make the body frame current. */` |
|    1791 |  407 | `		VmStampCoroutineCallSite(pVm, pCtx);` |
|    1791 |  408 | `		pCtx->pFrame->pParent = pVm->pFrame;` |
|    1791 |  409 | `		pVm->pFrame = pSeg ? pSeg->pTopFrame : pCtx->pFrame;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* The segment (if any) is handed to the body invocation explicitly below; it` |
|       - |  412 | `	 * is no longer part of the suspended ctx state once resume owns it. */` |
|    1791 |  413 | `	pCtx->pParkedSegment = 0;` |
|       - |  414 | `	/* Save and set the active context */` |
|    1791 |  415 | `	pOldCtx = pVm->pActiveCtx;` |
|    1791 |  416 | `	pVm->pActiveCtx = pCtx;` |
|    1791 |  417 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|    1791 |  418 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1; /* see VmStartCtx */` |
|       - |  419 | `	/* Resume execution from saved PC. pSeg, when non-NULL, makes this body` |
|       - |  420 | `	 * re-enter inside the innermost parked callee (deep fiber resume, stage 4). */` |
|    2684 |  421 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     893 |  422 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|     893 |  423 | `		VmCtxEnforceRetFunc(pCtx), FALSE, pSeg, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|    1791 |  424 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     898 |  425 | `}` |
|       - |  426 | `/*` |
|       - |  427 | ` * Force-close a suspended generator context at destruction time, running its` |
|       - |  428 | `` * pending `finally` blocks (PHP runs finally when a generator is unset / goes out`` |
|       - |  429 | ` * of scope / is GC'd before it completes; PHL previously freed the open try` |
|       - |  430 | ` * handlers unexecuted in VmReleaseExecCtx). This is a "close", not a "resume":` |
|       - |  431 | `` * the finally handler of every still-open `try` the generator was suspended`` |
|       - |  432 | `` * inside runs innermost-first, but NO `catch` runs and no code past the finallys`` |
|       - |  433 | ` * executes.` |
|       - |  434 | ` *` |
|       - |  435 | ` * Generators compile finally INLINE (ROOT C): the finally bytecode lives in the` |
|       - |  436 | ` * body program at iFinallyPc, driven by the aException/aFinallyAction pc-redirect` |
|       - |  437 | `` * machinery — not the legacy detached `sFinally` mini-program VmDrainFinally runs.`` |
|       - |  438 | `` * So a close is expressed exactly like a `return` that crosses every enclosing`` |
|       - |  439 | ` * finally (OP_SET_FINALLY_RET): set pCtx->bClosing and resume; the body-entry` |
|       - |  440 | ` * redirect (see VmByteCodeExecBody) seeds a PH7_FA_RETURN action and jumps into` |
|       - |  441 | ` * the innermost open try's finally, and OP_END_FINALLY threads it out through the` |
|       - |  442 | ` * chain, then completes the body.` |
|       - |  443 | ` *` |
|       - |  444 | ` * Scope: a plain body-level suspend (pParkedSegment == 0). A deep fiber segment is` |
|       - |  445 | ` * left to plain release (generators never park one — yield is body-level only). A` |
|       - |  446 | `` * `yield` reached inside a finally during close is rejected by OP_YIELD via`` |
|       - |  447 | ` * pCtx->bClosing (PHP-exact "Cannot yield from finally in a force-closed` |
|       - |  448 | ` * generator"). Deferred edges remain.` |
|       - |  449 | ` *` |
|       - |  450 | ` * Returns whatever the body run returns: SXRET_OK on a clean close, PH7_ABORT if a` |
|       - |  451 | ` * finally aborts, or PH7_EXCEPTION if a finally threw past itself (surfaced to the` |
|       - |  452 | ` * destruct caller).` |
|       - |  453 | ` */` |
|     636 |  454 | `static sxi32 VmCloseCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  455 | `{` |
|       - |  456 | `	sxi32 rc;` |
|     641 |  457 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       - |  458 | `		/* CREATED never ran, so has no open try; COMPLETED/CLOSED already ran theirs. */` |
|     423 |  459 | `		return SXRET_OK;` |
|       - |  460 | `	}` |
|     223 |  461 | `	if( pCtx->pParkedSegment != 0 ){` |
|       - |  462 | `		/* Deep fiber segment (never a generator) — leave to plain release. */` |
|     ! 0 |  463 | `		return SXRET_OK;` |
|       - |  464 | `	}` |
|       - |  465 | ``	/* Suspended mid `yield from` over an inner generator: PHP closes innermost-first,`` |
|       - |  466 | `	 * so run the delegate's finallys before this body's. Both delegate-object states` |
|       - |  467 | `	 * carry the live iterator in sDelegate — state 3 (the delegate IS a Generator) and` |
|       - |  468 | ``	 * state 2 (an Iterator, which for `yield from $aggregate` is the generator returned`` |
|       - |  469 | `	 * by getIterator()); VmGeneratorExtractCtx returns 0 for a non-generator iterator,` |
|       - |  470 | `	 * so a plain Iterator/array delegate is skipped. Recurses for nested delegation;` |
|       - |  471 | `	 * the inner ends COMPLETED, so its own later __destruct close is a no-op. */` |
|     223 |  472 | `	if( pCtx->iDelegateState >= 2 && (pCtx->sDelegate.iFlags & MEMOBJ_OBJ) ){` |
|      15 |  473 | `		ph7_generator *pInner = VmGeneratorExtractCtx(pVm, &pCtx->sDelegate);` |
|      15 |  474 | `		if( pInner && pInner->pCtx ){` |
|      15 |  475 | `			sxi32 rcInner = VmCloseCtx(pVm, pInner->pCtx);` |
|      15 |  476 | `			if( rcInner == PH7_ABORT ){ return PH7_ABORT; }` |
|       6 |  477 | `		}` |
|       6 |  478 | `	}` |
|       - |  479 | `	/* Drive the pending finallys through a real body resume that the entry redirect` |
|       - |  480 | `	 * turns into a finally-chain unwind. VmResumeCtx handles state restore, frame` |
|       - |  481 | `	 * re-attach, active-ctx save/restore and the terminal-state bookkeeping. */` |
|     223 |  482 | `	pCtx->bClosing = 1;` |
|     223 |  483 | `	rc = VmResumeCtx(pVm, pCtx, 0, 0);` |
|     223 |  484 | `	pCtx->bClosing = 0;` |
|     223 |  485 | `	return rc;` |
|     323 |  486 | `}` |
|       - |  487 | `/*` |
|       - |  488 | ` * Free one DETACHED frame (not in the live pVm->pFrame chain): the frame of a` |
|       - |  489 | ` * suspended coroutine's body, or of a segment activation abandoned mid-call.` |
|       - |  490 | ` * Mirrors VmLeaveFrame's teardown minus the chain pop (there is no chain to pop` |
|       - |  491 | ` * from). Factored so the body-frame free and the stage-4 segment free share it.` |
|       - |  492 | ` */` |
|    1170 |  493 | `static void VmFreeDetachedFrame(ph7_vm *pVm, VmFrame *pFrame)` |
|       5 |  494 | `{` |
|       - |  495 | `	VmSlot *aSlot;` |
|       - |  496 | `	sxu32 n;` |
|    1175 |  497 | `	if( pFrame == 0 ){` |
|     ! 0 |  498 | `		return;` |
|       - |  499 | `	}` |
|       - |  500 | `	/* The activation's hold on the function it was running, the same one` |
|       - |  501 | `	 * VmLeaveFrame gives back for a live frame. */` |
|    1175 |  502 | `	if( pFrame->pUserData ){` |
|    1175 |  503 | `		PH7_VmClosureFuncUnref(pVm,(ph7_vm_func *)pFrame->pUserData);` |
|    1175 |  504 | `		pFrame->pUserData = 0;` |
|     585 |  505 | `	}` |
|       - |  506 | `	/* End the foreach walks this (abandoned) activation never finished — the same` |
|       - |  507 | `	 * teardown, at the same point, VmLeaveFrame does it at. */` |
|    1175 |  508 | `	VmReleaseFrameForeachSteps(pVm,pFrame);` |
|       - |  509 | `	/* Remove local references FIRST, then free the locals nothing else holds — the` |
|       - |  510 | `	 * order and the holder test VmLeaveFrame explains. */` |
|    1175 |  511 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|    2321 |  512 | `	for( n = 0; n < SySetUsed(&pFrame->sRef); ++n ){` |
|    1151 |  513 | `		PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|     578 |  514 | `	}` |
|       - |  515 | `	/* Free local variables */` |
|    1175 |  516 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|    2291 |  517 | `	for( n = 0; n < SySetUsed(&pFrame->sLocal); ++n ){` |
|    1121 |  518 | `		if( PH7_VmSlotHolderCount(pVm, aSlot[n].nIdx) > 0 ){` |
|     ! 0 |  519 | `			continue;` |
|       - |  520 | `		}` |
|    1121 |  521 | `		PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|     563 |  522 | `	}` |
|    1175 |  523 | `	SyHashRelease(&pFrame->hVar);` |
|    1175 |  524 | `	SySetRelease(&pFrame->sArg);` |
|    1175 |  525 | `	SySetRelease(&pFrame->sLocal);` |
|    1175 |  526 | `	SySetRelease(&pFrame->sRef);` |
|    1175 |  527 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|       - |  528 | `	/* Drop a resume target pointing at this detached frame before we free it (ROOT B). */` |
|    1175 |  529 | `	VmDropResumeTarget(pVm,pFrame);` |
|    1175 |  530 | `	SyMemBackendPoolFree(&pVm->sAllocator, pFrame);` |
|     590 |  531 | `}` |
|       - |  532 | `/*` |
|       - |  533 | ` * Free a parked deep-suspend segment (stage 4) whose fiber was abandoned while` |
|       - |  534 | ` * suspended. Every record holds a callee's operand stack and VmFrame (the` |
|       - |  535 | ` * topmost record's callee is the innermost activation, running on sState); walk` |
|       - |  536 | ` * the chain releasing each callee stack's live entries then the stack and frame.` |
|       - |  537 | ` * The body frame/stack are NOT here — they are freed by the caller` |
|       - |  538 | ` * (VmReleaseExecCtx) as pCtx->pFrame / pCtx->pStack.` |
|       - |  539 | ` */` |
|     200 |  540 | `static void VmFreeParkedSegment(ph7_vm *pVm, ph7_exec_ctx *pCtx, VmParkedSegment *pSeg)` |
|       1 |  541 | `{` |
|       - |  542 | `	/* Live top-of-stack of the activation running on the current record's callee` |
|       - |  543 | `	 * stack: the innermost (sState) for the topmost record, then each caller. */` |
|     201 |  544 | `	ph7_value *pTosAbove = pSeg->sState.pTos;` |
|     201 |  545 | `	VmCallFrame *pRec = pSeg->pCallTop, *pNext;` |
|     401 |  546 | `	while( pRec ){` |
|     201 |  547 | `		ph7_value *pStk = pRec->sCall.pFrameStack;` |
|     201 |  548 | `		if( pStk ){` |
|     201 |  549 | `			ph7_value *pTos = pTosAbove;` |
|     401 |  550 | `			while( pTos >= pStk ){` |
|     201 |  551 | `				PH7_MemObjRelease(pTos);` |
|     201 |  552 | `				pTos--;` |
|       1 |  553 | `			}` |
|     201 |  554 | `			SyMemBackendFree(&pVm->sAllocator, pStk);` |
|     100 |  555 | `		}` |
|     201 |  556 | `		VmFreeDetachedFrame(pVm, pRec->sCall.pFrame);` |
|       - |  557 | `		/* The caller recorded here runs on the NEXT-lower callee stack; grab its` |
|       - |  558 | `		 * live tos before freeing this node. */` |
|     201 |  559 | `		pTosAbove = pRec->sCaller.pTos;` |
|     201 |  560 | `		pNext = pRec->pPrev;` |
|     201 |  561 | `		SyMemBackendPoolFree(&pVm->sAllocator, pRec);` |
|     201 |  562 | `		pRec = pNext;` |
|       1 |  563 | `	}` |
|       - |  564 | `	/* pTosAbove now points at the BODY activation's live top (the bottom record's` |
|       - |  565 | `	 * sCaller, which runs on pCtx->pStack). pCtx->nTos still holds the INNERMOST` |
|       - |  566 | `	 * index (VmSuspendCtx saved it), which would over-index the body stack in` |
|       - |  567 | `	 * VmReleaseExecCtx's release loop — correct it to the body's real top. */` |
|     201 |  568 | `	pCtx->nTos = (sxi32)(pTosAbove - pCtx->pStack);` |
|     201 |  569 | `	SyMemBackendFree(&pVm->sAllocator, pSeg);` |
|     201 |  570 | `}` |
|       - |  571 | `/*` |
|       - |  572 | ` * Release an execution context and all its resources.` |
|       - |  573 | ` */` |
|     970 |  574 | `PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  575 | `{` |
|     975 |  576 | `	if( pCtx == 0 ){` |
|     ! 0 |  577 | `		return;` |
|       - |  578 | `	}` |
|     975 |  579 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       - |  580 | `		/* Cannot destroy a fiber that is currently executing */` |
|     ! 0 |  581 | `		return;` |
|       - |  582 | `	}` |
|     975 |  583 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       - |  584 | `	/* ...and give back the hold VmNewExecCtx took on the function this coroutine runs. */` |
|     975 |  585 | `	if( pCtx->pFunc ){` |
|     975 |  586 | `		PH7_VmClosureFuncUnref(pVm,pCtx->pFunc);` |
|     485 |  587 | `	}` |
|       - |  588 | `	/* Release values */` |
|     975 |  589 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|     975 |  590 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|     975 |  591 | `	PH7_MemObjRelease(&pCtx->sDelegate);` |
|       - |  592 | `	/* Stage 2b: the parked entries are per-activation clones now — free them` |
|       - |  593 | `	 * (an abandoned suspended coroutine is their last holder). */` |
|     975 |  594 | `	VmExcReleaseAll(pVm,&pCtx->aSavedException);` |
|     975 |  595 | `	SySetRelease(&pCtx->aSavedException);` |
|       - |  596 | `	/* ROOT C: a generator abandoned while suspended inside a finally may carry parked` |
|       - |  597 | `	 * finally actions holding owned values/refs (a RETURN's sRet, a RETHROW's pExc).` |
|       - |  598 | `	 * Release them so the abandon path leaks nothing. */` |
|       - |  599 | `	{` |
|     975 |  600 | `		sxu32 n = SySetUsed(&pCtx->aSavedFinally);` |
|     975 |  601 | `		if( n > 0 ){` |
|     ! 0 |  602 | `			VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pCtx->aSavedFinally);` |
|       - |  603 | `			sxu32 i;` |
|     ! 0 |  604 | `			for( i = 0; i < n; i++ ){` |
|     ! 0 |  605 | `				if( aA[i].eKind == PH7_FA_RETURN ){` |
|     ! 0 |  606 | `					PH7_MemObjRelease(&aA[i].sRet);` |
|     ! 0 |  607 | `				}else if( aA[i].eKind == PH7_FA_RETHROW && aA[i].pExc ){` |
|     ! 0 |  608 | `					PH7_ClassInstanceUnref(aA[i].pExc);` |
|     ! 0 |  609 | `				}` |
|     ! 0 |  610 | `			}` |
|     ! 0 |  611 | `		}` |
|     975 |  612 | `		SySetRelease(&pCtx->aSavedFinally);` |
|       - |  613 | `	}` |
|       - |  614 | `	/* Stage 4: parked aSelf entries are borrowed class pointers — just free the set. */` |
|     975 |  615 | `	SySetRelease(&pCtx->aSavedSelf);` |
|       - |  616 | `	/* Free a parked deep-suspend segment (stage 4): the fiber was abandoned while` |
|       - |  617 | `	 * suspended inside a nested call, so its record chain / frames / operand` |
|       - |  618 | `	 * stacks are still alive and only this holder references them. Must run` |
|       - |  619 | `	 * before the body frame/stack below (they are the segment's floor). */` |
|     975 |  620 | `	if( pCtx->pParkedSegment ){` |
|     201 |  621 | `		VmFreeParkedSegment(pVm, pCtx, (VmParkedSegment *)pCtx->pParkedSegment);` |
|     201 |  622 | `		pCtx->pParkedSegment = 0;` |
|     100 |  623 | `	}` |
|       - |  624 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|     975 |  625 | `	if( pCtx->pFrame ){` |
|     975 |  626 | `		VmFreeDetachedFrame(pVm, pCtx->pFrame);` |
|     975 |  627 | `		pCtx->pFrame = 0;` |
|     485 |  628 | `	}` |
|       - |  629 | `	/* A by-reference parameter aliased a CALLER's slot; the frame teardown above just` |
|       - |  630 | `	 * dropped this body's name for it. Release it if nothing is left holding it — the` |
|       - |  631 | `	 * caller may already be gone (it skipped the slot precisely because this frame` |
|       - |  632 | `	 * held it), in which case this is its last holder. Runs after the frame so the` |
|       - |  633 | `	 * body's own row is out of the count. */` |
|       - |  634 | `	{` |
|       - |  635 | `		sxu32 n;` |
|     975 |  636 | `		sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pCtx->aByRefArg);` |
|    1007 |  637 | `		for( n = 0; n < SySetUsed(&pCtx->aByRefArg); n++ ){` |
|      34 |  638 | `			PH7_VmReleaseUnheldSlot(pVm, aIdx[n]);` |
|      18 |  639 | `		}` |
|     975 |  640 | `		SySetRelease(&pCtx->aByRefArg);` |
|       - |  641 | `	}` |
|       - |  642 | `	/* Release individual operand stack entries (decrement refcounts,` |
|       - |  643 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|       - |  644 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|     975 |  645 | `	if( pCtx->pStack ){` |
|     975 |  646 | `		if( pCtx->nTos >= 0 ){` |
|     827 |  647 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|    1677 |  648 | `			while( pTos >= pCtx->pStack ){` |
|     855 |  649 | `				PH7_MemObjRelease(pTos);` |
|     855 |  650 | `				pTos--;` |
|       5 |  651 | `			}` |
|     411 |  652 | `		}` |
|     975 |  653 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|     975 |  654 | `		pCtx->pStack = 0;` |
|     485 |  655 | `	}` |
|       - |  656 | `	/* Free the context itself */` |
|     975 |  657 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     490 |  658 | `}` |
|       - |  659 | `/*` |
|       - |  660 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|       - |  661 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|       - |  662 | ` */` |
|    1114 |  663 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|       5 |  664 | `{` |
|       - |  665 | `	ph7_class_instance *pThis;` |
|       - |  666 | `	SyString sAttr;` |
|       - |  667 | `	ph7_value *pAttr;` |
|    1119 |  668 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 |  669 | `		return 0;` |
|       - |  670 | `	}` |
|    1119 |  671 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|    1119 |  672 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|     ! 0 |  673 | `		return 0;` |
|       - |  674 | `	}` |
|    1119 |  675 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    1119 |  676 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    1119 |  677 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     375 |  678 | `		return 0;` |
|       - |  679 | `	}` |
|     749 |  680 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|     562 |  681 | `}` |
|       - |  682 | `/* The three VM_INSTANCE_FCC_* Closure flags live in ph7int.h: vm_exec.c's OP_LOAD_FCC` |
|       - |  683 | ` * stamps VM_INSTANCE_FCC_METHOD and this file reads all three. */` |
|       - |  684 | `/*` |
|       - |  685 | `` * A PHP closure (and a first-class callable `f(...)`) is a real object: an instance of`` |
|       - |  686 | `` * the built-in final `Closure` class carrying its underlying callable in a private`` |
|       - |  687 | `` * `$__fn` attribute (the callable NAME: a registered `[closure_N]`/`[lambda_N]` or a`` |
|       - |  688 | ` * user/host function name) — plus, for a method/static first-class callable, a bound` |
|       - |  689 | `` * `$__this` object and/or a `$__scope` class-name. Storing the callable as plain attributes`` |
|       - |  690 | `` * (rather than a native resource pointer) means the object owns no `ph7_vm_func` — the`` |
|       - |  691 | `` * per-closure function stays owned by `hFunction`/VM-release exactly as before, so there is`` |
|       - |  692 | ` * no extra free path.` |
|       - |  693 | ` *` |
|       - |  694 | ` * Returns non-zero iff pVal is a Closure instance.` |
|       - |  695 | ` */` |
| 9330305 |  696 | `PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm, ph7_value *pVal)` |
|       5 |  697 | `{` |
|       - |  698 | `	ph7_class_instance *pThis;` |
|       - |  699 | `	/* Flag test first: a non-object call target (the hot common case) bails before any` |
|       - |  700 | `	 * pVm dereference; pClosureClass==0 is a one-time pre-init concern, so it goes last. */` |
| 9330310 |  701 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
| 9017419 |  702 | `		return 0;` |
|       - |  703 | `	}` |
|  312896 |  704 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       - |  705 | `	/* Closure is final, so an exact class match is correct (no subclasses possible). */` |
|  312896 |  706 | `	return pThis->pClass == pVm->pClosureClass;` |
| 4663866 |  707 | `}` |
|       - |  708 | `/*` |
|       - |  709 | ` * Unwrap a Closure value into the simple callable the existing dispatch machinery` |
|       - |  710 | ` * already understands, written into pOut (which the caller must have initialised):` |
|       - |  711 | `` *   - bound `$this` set (method first-class callable) -> [ $__this, $__fn ] array callable`` |
|       - |  712 | `` *   - `$__scope` set (static first-class callable)     -> [ $__scope, $__fn ] array callable`` |
|       - |  713 | `` *   - neither (plain function / real closure)          -> the `$__fn` name string`` |
|       - |  714 | ` * The 2-element array form rides the existing MEMOBJ_HASHMAP dispatch, which binds $this` |
|       - |  715 | ` * for an object first element and resolves the class for a class-name-string first element.` |
|       - |  716 | ` * Returns SXRET_OK if pVal was a Closure (pOut filled), SXERR_NOTFOUND otherwise.` |
|       - |  717 | ` */` |
|   36847 |  718 | `PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm, ph7_value *pVal, ph7_value *pOut)` |
|       5 |  719 | `{` |
|       - |  720 | `	ph7_class_instance *pThis;` |
|       - |  721 | `	ph7_value *pFn;` |
|       - |  722 | `	SyString sAttr;` |
|   36852 |  723 | `	if( !VmValueIsClosure(pVm, pVal) ){` |
|     ! 0 |  724 | `		return SXERR_NOTFOUND;` |
|       - |  725 | `	}` |
|   36852 |  726 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|   36852 |  727 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|   36852 |  728 | `	pFn = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|   36852 |  729 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 |  730 | `		return SXERR_NOTFOUND; /* malformed/uninitialised closure */` |
|       - |  731 | `	}` |
|       - |  732 | `	/* Only a bound/static first-class callable (rare) carries $__this/$__scope; the` |
|       - |  733 | `	 * VM_INSTANCE_FCC_BOUND flag (set by VmCreateClosure) keeps the hot plain-closure path` |
|       - |  734 | `	 * to the single $__fn lookup above instead of two extra attribute lookups per dispatch. */` |
|   36852 |  735 | `	if( pThis->iFlags & VM_INSTANCE_FCC_BOUND ){` |
|       - |  736 | `		ph7_value *pBound, *pScope;` |
|       - |  737 | `		int bBoundObj, bScope;` |
|     280 |  738 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     280 |  739 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     280 |  740 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     280 |  741 | `		pScope = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     280 |  742 | `		bBoundObj = pBound && (pBound->iFlags & MEMOBJ_OBJ);` |
|     280 |  743 | `		bScope = pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0;` |
|     280 |  744 | `		if( bBoundObj \|\| bScope ){` |
|       - |  745 | `			/* Method/static first-class callable -> [ target, "method" ] array callable. */` |
|     280 |  746 | `			if( pThis->iFlags & VM_INSTANCE_FCC_INVOKE_OBJ ){` |
|       - |  747 | `				/* Closure::fromCallable($obj): the engine named __invoke, so this` |
|       - |  748 | `				 * dispatch is the engine's own and a non-public one still runs. */` |
|      12 |  749 | `				pVm->bMagicDispatch = 1;` |
|       5 |  750 | `			}` |
|       - |  751 | `			ph7_hashmap *pMap;` |
|       - |  752 | `			ph7_value sTarget, sMeth;` |
|       - |  753 | `			sxi32 rc;` |
|     280 |  754 | `			if( bBoundObj ){` |
|     202 |  755 | `				ph7_class_instance *pBoundObj = (ph7_class_instance *)pBound->x.pOther;` |
|       - |  756 | ``				/* A bound PLAIN closure (`function(){…}->bindTo($o)`): $__fn names a function, not a`` |
|       - |  757 | `				 * method of the bound object's class, so a [obj,method] array callable would fail` |
|       - |  758 | `				 * method resolution. Stash the bound object (own a ref) so the OP_CALL user-function` |
|       - |  759 | `				 * frame setup injects it as $this, and return the plain $__fn string for a normal` |
|       - |  760 | `				 * function dispatch. */` |
|     198 |  761 | `				if( (pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0` |
|     145 |  762 | `				 && PH7_ClassExtractMethod(pBoundObj->pClass,` |
|     126 |  763 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){` |
|       - |  764 | `					/* Only a USER function (anonymous closure / named fn in hFunction) reads $this and` |
|       - |  765 | `					 * reaches the OP_CALL user-function frame-setup that consumes pClosureThis. A HOST` |
|       - |  766 | `					 * function (e.g. Closure::fromCallable('strlen')->bindTo($o)) ignores $this and` |
|       - |  767 | `					 * dispatches via the host path which never consumes the transient — setting it` |
|       - |  768 | `					 * there would leak the ref and inject a stale $this into the next call. */` |
|     111 |  769 | `					if( SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|      78 |  770 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|      78 |  771 | `						pBoundObj->iRef++;` |
|      78 |  772 | `						pVm->pClosureThis = pBoundObj;` |
|       - |  773 | `						/* Carry the bound scope (if set) so private/protected member access inside` |
|       - |  774 | `						 * the closure body resolves against it — bindTo($o, Scope::class) / call($o). */` |
|      78 |  775 | `						if( bScope ){` |
|      91 |  776 | `							pVm->pClosureScope = PH7_VmExtractClass(pVm,` |
|      58 |  777 | `								(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      29 |  778 | `						}` |
|      37 |  779 | `					}` |
|      78 |  780 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      78 |  781 | `					return SXRET_OK;` |
|       - |  782 | `				}` |
|      65 |  783 | `			}else{` |
|       - |  784 | ``				/* Scope-only rebind of a PLAIN closure (`bindTo(null, Scope::class)`):`` |
|       - |  785 | `				 * $__fn names a function, not a static method of the scope class, so` |
|       - |  786 | `				 * the [scope, method] array callable below would fail method` |
|       - |  787 | `				 * resolution. Dispatch the plain $__fn string and carry the scope for` |
|       - |  788 | `				 * private/protected visibility (pClosureThis stays unset — no $this).` |
|       - |  789 | `				 * A static-method FCC ($__fn really is a method of the scope class)` |
|       - |  790 | `				 * falls through to the array-callable path. */` |
|     120 |  791 | `				ph7_class *pScopeClass = PH7_VmExtractClass(pVm,` |
|      78 |  792 | `					(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      78 |  793 | `				if( pScopeClass == 0` |
|      81 |  794 | `				 \|\| ((pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0` |
|      43 |  795 | `				  && PH7_ClassExtractMethod(pScopeClass,` |
|      12 |  796 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0) ){` |
|       8 |  797 | `					if( pScopeClass` |
|       9 |  798 | `					 && SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|       8 |  799 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|       9 |  800 | `						pVm->pClosureScope = pScopeClass;` |
|       4 |  801 | `					}` |
|       9 |  802 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|       9 |  803 | `					return SXRET_OK;` |
|       - |  804 | `				}` |
|       - |  805 | `			}` |
|     197 |  806 | `			pMap = PH7_NewHashmap(&(*pVm), 0, 0);` |
|     197 |  807 | `			if( pMap == 0 ){` |
|     ! 0 |  808 | `				return SXERR_NOTFOUND;` |
|       - |  809 | `			}` |
|     197 |  810 | `			PH7_MemObjInit(pVm, &sTarget);` |
|     197 |  811 | `			PH7_MemObjInit(pVm, &sMeth);` |
|     197 |  812 | `			if( bBoundObj ){` |
|     127 |  813 | `				PH7_MemObjStore(pBound, &sTarget); /* bound object (iRef++) -> binds $this */` |
|      65 |  814 | `			}else{` |
|      73 |  815 | `				PH7_MemObjStringAppend(&sTarget, SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob));` |
|       - |  816 | `			}` |
|     197 |  817 | `			PH7_MemObjStringAppend(&sMeth, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|     197 |  818 | `			rc = PH7_HashmapInsert(pMap, 0, &sTarget);` |
|     197 |  819 | `			if( rc == SXRET_OK ){` |
|     197 |  820 | `				rc = PH7_HashmapInsert(pMap, 0, &sMeth);` |
|      97 |  821 | `			}` |
|     197 |  822 | `			PH7_MemObjRelease(&sTarget);` |
|     197 |  823 | `			PH7_MemObjRelease(&sMeth);` |
|     197 |  824 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  825 | `				PH7_HashmapRelease(pMap, TRUE); /* free the partial map, no leak */` |
|     ! 0 |  826 | `				return SXERR_NOTFOUND;` |
|       - |  827 | `			}` |
|     197 |  828 | `			if( pThis->iFlags & VM_INSTANCE_FCC_SCREENED ){` |
|       - |  829 | `				/* php resolves a method Closure's callee ONCE, where the closure is built, and` |
|       - |  830 | ``				 * keeps the resolved function: an escaped `$this->priv(...)` runs anywhere. PHL`` |
|       - |  831 | `				 * keeps only a NAME, so every dispatch site would re-decide visibility against the` |
|       - |  832 | `				 * CALLER and refuse the closure php runs. The creation sites screen (OP_LOAD_FCC's` |
|       - |  833 | `				 * VmFccMemberError, Closure::fromCallable's PH7_VmIsCallable gate, and reflection,` |
|       - |  834 | `				 * which php lets past protection on purpose) and stamp the mark only when a real` |
|       - |  835 | `				 * method answered — a name the class reaches through __call carries no mark and` |
|       - |  836 | `				 * still routes to the catch-all. Armed only once the pair below really exists, so` |
|       - |  837 | `				 * a failed build cannot leave it standing; the consumers clear it like` |
|       - |  838 | `				 * pClosureThis/pClosureScope. */` |
|     159 |  839 | `				pVm->bClosureScreened = 1;` |
|      78 |  840 | `			}` |
|     197 |  841 | `			pOut->x.pOther = pMap;` |
|     197 |  842 | `			MemObjSetType(pOut, MEMOBJ_HASHMAP);` |
|     197 |  843 | `			return SXRET_OK;` |
|       - |  844 | `		}` |
|     ! 0 |  845 | `	}` |
|   36576 |  846 | `	PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   36576 |  847 | `	return SXRET_OK;` |
|   18191 |  848 | `}` |
|       - |  849 | `/*` |
|       - |  850 | ` * php's SCOPE for a Closure — the class whose private members its body may reach, and the` |
|       - |  851 | ` * class Reflection reports. For a plain closure that is the class it was bound to; for a` |
|       - |  852 | ` * METHOD closure it is the class that DECLARED the method, which is not the class the` |
|       - |  853 | `` * callable NAMED: `(new Kid)->mk()` returning `$this->basePriv(...)` is scoped to Base in`` |
|       - |  854 | `` * php, while `$__scope` records Kid — the class the call goes THROUGH, which is the CALLED`` |
|       - |  855 | `` * scope (`static::`) and a different question. A trait method is composed into the using`` |
|       - |  856 | ` * class, so PH7_VmMethodScopeName answers for it exactly as it does at every refusal site.` |
|       - |  857 | ` * Returns 0 when the closure carries no scope at all.` |
|       - |  858 | ` */` |
|      92 |  859 | `PH7_PRIVATE ph7_class * PH7_VmClosureScopeClass(ph7_vm *pVm, ph7_class_instance *pClosure)` |
|       2 |  860 | `{` |
|       - |  861 | `	SyString sAttr;` |
|       - |  862 | `	ph7_value *pScope, *pFn;` |
|       - |  863 | `	ph7_class *pClass;` |
|      94 |  864 | `	if( pClosure == 0 ){` |
|     ! 0 |  865 | `		return 0;` |
|       - |  866 | `	}` |
|      94 |  867 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      94 |  868 | `	pScope = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      94 |  869 | `	if( pScope == 0 \|\| (pScope->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pScope->sBlob) == 0 ){` |
|     ! 0 |  870 | `		return 0;` |
|       - |  871 | `	}` |
|     140 |  872 | `	pClass = PH7_VmExtractClass(pVm, (const char *)SyBlobData(&pScope->sBlob),` |
|      46 |  873 | `		SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      94 |  874 | `	if( pClass == 0 \|\| (pClosure->iFlags & VM_INSTANCE_FCC_METHOD) == 0 ){` |
|       7 |  875 | `		return pClass;` |
|       - |  876 | `	}` |
|      88 |  877 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|      88 |  878 | `	pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      88 |  879 | `	if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){` |
|     131 |  880 | `		ph7_class_method *pMeth = PH7_ClassExtractMethod(pClass,` |
|      86 |  881 | `			(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      88 |  882 | `		if( pMeth ){` |
|      82 |  883 | `			return PH7_VmMethodScopeName(pVm, pClass, pMeth);` |
|       - |  884 | `		}` |
|       3 |  885 | `	}` |
|       7 |  886 | `	return pClass;` |
|      48 |  887 | `}` |
|       - |  888 | `/*` |
|       - |  889 | `` * Resolve the scope class for a STATIC first-class callable `T::m(...)`, where T is a`` |
|       - |  890 | ` * class-name STRING value: handles the self/static/parent keywords against the live class` |
|       - |  891 | ` * context (so the actual class is bound at FCC-creation time, like PHP), and falls back to` |
|       - |  892 | ` * an explicit class-name lookup. Mirrors the static OP_MEMBER resolution. Returns 0 if the` |
|       - |  893 | ` * class cannot be resolved.` |
|       - |  894 | ` */` |
|     150 |  895 | `PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget)` |
|       5 |  896 | `{` |
|     305 |  897 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     150 |  898 | `		(sxu32)SyBlobLength(&pTarget->sBlob));` |
|       5 |  899 | `}` |
|       - |  900 | `/*` |
|       - |  901 | ` * The same resolution over a raw (name, length) pair, for the callable machinery: php` |
|       - |  902 | `` * resolves `self`/`parent`/`static` in a CALLBACK (is_callable, call_user_func, array_map …)`` |
|       - |  903 | `` * against the live class context, and refuses them in the direct `$cb()` dispatch — so this`` |
|       - |  904 | ` * is deliberately NOT wired into the OP_CALL resolve check, which must keep answering` |
|       - |  905 | `` * `Class "self" not found`.`` |
|       - |  906 | ` */` |
|       - |  907 | `/*` |
|       - |  908 | `` * A Closure object is going: give back its hold on the function `$__fn` names.`` |
|       - |  909 | ` * A run-time closure's per-instantiation ph7_vm_func belongs to the objects that` |
|       - |  910 | ` * name it, and this is where the last of them lets go.` |
|       - |  911 | ` */` |
|   18426 |  912 | `static void VmClosureRelease(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       5 |  913 | `{` |
|   18431 |  914 | `	PH7_VmClosureInstanceRef(pVm, pThis, -1);` |
|   18431 |  915 | `}` |
|  102116 |  916 | `PH7_PRIVATE ph7_class * PH7_VmResolveScopeName(ph7_vm *pVm, const char *zCls, sxu32 nCls)` |
|       5 |  917 | `{` |
|       - |  918 | `	ph7_class *pClass;` |
|  102121 |  919 | `	if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|     139 |  920 | `		pClass = PH7_VmPeekSelfClass(&(*pVm)); /* self:: in a trait -> the USING class */` |
|  102054 |  921 | `	}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|      78 |  922 | `		pClass = PH7_VmPeekTopClass(&(*pVm));     /* late static binding */` |
|  101950 |  923 | `	}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|      52 |  924 | `		pClass = PH7_VmResolveParentClass(&(*pVm));` |
|      28 |  925 | `	}else{` |
|  101865 |  926 | `		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - |  927 | `	}` |
|  102121 |  928 | `	return pClass;` |
|       5 |  929 | `}` |
|       - |  930 | `/*` |
|       - |  931 | ` * Create a Closure object wrapping a callable name (+ optional bound $this object and/or` |
|       - |  932 | `` * scope class-name, for the method/static first-class callables `$o->m(...)`/`C::m(...)`).`` |
|       - |  933 | ` * Mirrors the Generator/Fiber "object carries its state in private attributes" pattern.` |
|       - |  934 | ` * Returns the fresh instance holding ONE reference, which the caller's value TAKES --` |
|       - |  935 | ` * the same handover PH7_NewClassInstance makes to OP_NEW. (It used to say the caller` |
|       - |  936 | ` * had to add one, and every closure site did, so no Closure object ever reached zero.)` |
|       - |  937 | ` */` |
|   18901 |  938 | `PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName,` |
|       - |  939 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       5 |  940 | `{` |
|       - |  941 | `	ph7_class_instance *pObj;` |
|       - |  942 | `	ph7_value *pAttr;` |
|       - |  943 | `	SyString sAttr;` |
|   18906 |  944 | `	if( pVm->pClosureClass == 0 ){` |
|     ! 0 |  945 | `		return 0;` |
|       - |  946 | `	}` |
|   18906 |  947 | `	pObj = PH7_NewClassInstance(&(*pVm), pVm->pClosureClass);` |
|   18906 |  948 | `	if( pObj == 0 ){` |
|     ! 0 |  949 | `		return 0;` |
|       - |  950 | `	}` |
|   18906 |  951 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|   18906 |  952 | `	pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|   18906 |  953 | `	if( pAttr ){` |
|   18906 |  954 | `		PH7_MemObjStringAppend(pAttr, pName->zString, pName->nByte);` |
|       - |  955 | `		/* This object is now a holder of the function it names. For a run-time` |
|       - |  956 | `		 * closure that is what keeps the per-instantiation copy alive, and losing` |
|       - |  957 | `		 * it is what frees the copy. */` |
|   18906 |  958 | `		PH7_VmClosureInstanceRef(pVm, pObj, 1);` |
|    9329 |  959 | `	}` |
|   18906 |  960 | `	if( pBoundThis ){` |
|     165 |  961 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     165 |  962 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|     165 |  963 | `		if( pAttr ){` |
|     165 |  964 | `			pAttr->x.pOther = pBoundThis;` |
|     165 |  965 | `			MemObjSetType(pAttr, MEMOBJ_OBJ);` |
|     165 |  966 | `			pBoundThis->iRef++; /* keep the bound object alive for the closure's lifetime */` |
|      80 |  967 | `		}` |
|      80 |  968 | `	}` |
|   18906 |  969 | `	if( pScope && pScope->nByte ){` |
|     253 |  970 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     253 |  971 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|     253 |  972 | `		if( pAttr ){` |
|     253 |  973 | `			PH7_MemObjStringAppend(pAttr, pScope->zString, pScope->nByte);` |
|     124 |  974 | `		}` |
|     124 |  975 | `	}` |
|   18906 |  976 | `	if( pBoundThis \|\| (pScope && pScope->nByte) ){` |
|       - |  977 | `		/* Mark bound/static FCC closures so VmClosureUnwrap can skip the $__this/$__scope` |
|       - |  978 | `		 * lookups on the hot plain-closure dispatch path. */` |
|     253 |  979 | `		pObj->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|     124 |  980 | `	}` |
|   18906 |  981 | `	return pObj;` |
|    9334 |  982 | `}` |
|       - |  983 | `/*` |
|       - |  984 | ` * Exported wrapper around VmCreateClosure for builtin libraries outside this` |
|       - |  985 | ` * file (ReflectionFunction::getClosure / ReflectionMethod::getClosure).` |
|       - |  986 | ` */` |
|      16 |  987 | `PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm, const SyString *pName,` |
|       - |  988 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       1 |  989 | `{` |
|      17 |  990 | `	return VmCreateClosure(&(*pVm), pName, pBoundThis, pScope);` |
|       1 |  991 | `}` |
|       - |  992 | `/*` |
|       - |  993 | ` * Exported wrapper around the typed/readonly property store enforcement for` |
|       - |  994 | ` * ReflectionProperty::setValue (vm_builtin_reflection.c). Same contract:` |
|       - |  995 | ` * SXRET_OK (value possibly coerced in place), PH7_EXCEPTION, or PH7_ABORT.` |
|       - |  996 | ` */` |
|      10 |  997 | `PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|       1 |  998 | `{` |
|      11 |  999 | `	return VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pValue,0);` |
|       1 | 1000 | `}` |
|       - | 1001 | `/*` |
|       - | 1002 | ` * Exported reference-table probe for ReflectionReference::fromArrayElement` |
|       - | 1003 | ` * (vm_builtin_reflection.c). Returns the number of links (frame variables +` |
|       - | 1004 | ` * array entries) attached to the slot's reference record, 0 when the slot` |
|       - | 1005 | ` * has none — an array element is a PHP reference when this is >= 2.` |
|       - | 1006 | ` */` |
|      18 | 1007 | `PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx)` |
|       1 | 1008 | `{` |
|      19 | 1009 | `	return (int)(PH7_VmSlotEntryCount(&(*pVm),nIdx) + PH7_VmSlotNodeCount(&(*pVm),nIdx));` |
|       1 | 1010 | `}` |
|       - | 1011 | `/*` |
|       - | 1012 | `` * First-class callable over an arbitrary callable VALUE: `($expr)(...)`.`` |
|       - | 1013 | ` * Normalize a validated callable value into a fresh Closure, reusing VmCreateClosure (the same` |
|       - | 1014 | ` * object the method/static first-class-callable paths mint, so dispatch round-trips identically` |
|       - | 1015 | ` * via VmClosureUnwrap). Handles the three remaining callable shapes a value can hold:` |
|       - | 1016 | ` *   - a function-NAME string          -> plain closure ($__fn = name)` |
|       - | 1017 | ` *   - a [target, method] array callable -> bound (object target -> $this) / static (class-name` |
|       - | 1018 | ` *     target -> scope) closure, mirroring the [obj,m]/[class,m] decode in PH7_VmIsCallable` |
|       - | 1019 | ` *   - an __invoke object               -> closure bound to the object's __invoke` |
|       - | 1020 | ` * An existing Closure returns 0 here (it is already a Closure — the caller keeps it as-is), so this` |
|       - | 1021 | ` * stays idempotent even for a direct caller. Returns the fresh instance holding ONE reference,` |
|       - | 1022 | ` * which the caller's value TAKES (see VmCreateClosure) or 0 if the value is an existing Closure / not a normalizable callable / on OOM — in` |
|       - | 1023 | ` * which case the caller leaves the value untouched (graceful degradation). This is the generic` |
|       - | 1024 | ` * "callable value -> Closure" primitive: the body is FCC-agnostic and self-contained, so the future` |
|       - | 1025 | ` * Closure::bind/fromCallable work (Increment 2) can call it directly.` |
|       - | 1026 | ` */` |
|     222 | 1027 | `PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue)` |
|       5 | 1028 | `{` |
|       - | 1029 | `	/* A Closure is already callable; never double-wrap it (this also stops the __invoke branch` |
|       - | 1030 | `	 * below from binding a closure to its OWN __invoke). The OP_LOAD_FCC caller intercepts a` |
|       - | 1031 | `	 * Closure first too, but guarding here keeps the primitive safe for a direct Increment-2 caller. */` |
|     227 | 1032 | `	if( VmValueIsClosure(pVm, pValue) ){` |
|     ! 0 | 1033 | `		return 0;` |
|       - | 1034 | `	}` |
|     227 | 1035 | `	if( !PH7_VmIsCallable(pVm, pValue, TRUE) ){` |
|      51 | 1036 | `		return 0;` |
|       - | 1037 | `	}` |
|     177 | 1038 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - | 1039 | `		SyString sName;` |
|     110 | 1040 | `		const char *zName = (const char *)SyBlobData(&pValue->sBlob);` |
|     110 | 1041 | `		sxu32 nName = SyBlobLength(&pValue->sBlob), nSep;` |
|       - | 1042 | ``		/* `"C::m"` is the SAME callable as `[C, 'm']`, and php mints the same`` |
|       - | 1043 | `		 * closure for it: scope C, name m. PHL kept the whole string as the` |
|       - | 1044 | `		 * function name, so the Closure ran (dispatch splits it) but described` |
|       - | 1045 | `		 * itself as nothing -- ReflectionFunction over it had no name, no scope` |
|       - | 1046 | `		 * and no parameters, and a library that reflects a callback before` |
|       - | 1047 | `		 * calling it (twig compiles every filter that way) died on the read. */` |
|     864 | 1048 | `		for( nSep = 0 ; nSep + 1 < nName ; ++nSep ){` |
|     776 | 1049 | `			if( zName[nSep] == ':' && zName[nSep+1] == ':' ){` |
|      19 | 1050 | `				break;` |
|       - | 1051 | `			}` |
|     381 | 1052 | `		}` |
|     110 | 1053 | `		if( nSep + 1 < nName ){` |
|       - | 1054 | `			ph7_class *pScopeCls;` |
|       - | 1055 | `			SyString sCls;` |
|      19 | 1056 | `			SyStringInitFromBuf(&sCls, zName, nSep);` |
|      19 | 1057 | `			pScopeCls = PH7_VmExtractClass(pVm, SyStringData(&sCls), SyStringLength(&sCls), FALSE, 0);` |
|      19 | 1058 | `			if( pScopeCls ){` |
|       - | 1059 | `				ph7_class_instance *pFccObj;` |
|      13 | 1060 | `				SyStringInitFromBuf(&sName, zName + nSep + 2, nName - (nSep + 2));` |
|      13 | 1061 | `				pFccObj = VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName);` |
|      13 | 1062 | `				if( pFccObj ){` |
|      13 | 1063 | `					pFccObj->iFlags \|= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */` |
|      19 | 1064 | `					if( PH7_VmFccMethodIsDirect(pVm,pScopeCls,` |
|       6 | 1065 | `							SyStringData(&sName),SyStringLength(&sName)) ){` |
|       7 | 1066 | `						pFccObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       3 | 1067 | `					}` |
|       6 | 1068 | `				}` |
|      13 | 1069 | `				return pFccObj;` |
|       - | 1070 | `			}` |
|       3 | 1071 | `		}` |
|      98 | 1072 | `		SyStringInitFromBuf(&sName, zName, nName);` |
|      98 | 1073 | `		return VmCreateClosure(pVm, &sName, 0, 0);` |
|       - | 1074 | `	}` |
|      68 | 1075 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1076 | `		/* [target, method] — the same index-0/1 decode PH7_VmIsCallable uses to validate it` |
|       - | 1077 | `		 * (php reads the INTEGER indices, not insertion order). */` |
|      55 | 1078 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1079 | `		ph7_value *pTarget, *pMeth;` |
|       - | 1080 | `		SyString sName;` |
|      55 | 1081 | `		if( !PH7_VmArrayCallableParts(pVm, pMap, &pTarget, &pMeth) ){` |
|     ! 0 | 1082 | `			return 0;` |
|       - | 1083 | `		}` |
|      55 | 1084 | `		if( (pMeth->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pMeth->sBlob) == 0 ){` |
|     ! 0 | 1085 | `			return 0;` |
|       - | 1086 | `		}` |
|      55 | 1087 | `		SyStringInitFromBuf(&sName, SyBlobData(&pMeth->sBlob), SyBlobLength(&pMeth->sBlob));` |
|      55 | 1088 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      37 | 1089 | `			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      55 | 1090 | `			ph7_class_instance *pFccObj = VmCreateClosure(pVm, &sName, pBoundThis,` |
|      36 | 1091 | `				&pBoundThis->pClass->sName);` |
|      37 | 1092 | `			if( pFccObj ){` |
|      37 | 1093 | `				pFccObj->iFlags \|= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */` |
|      55 | 1094 | `				if( PH7_VmFccMethodIsDirect(pVm,pBoundThis->pClass,` |
|      18 | 1095 | `						SyStringData(&sName),SyStringLength(&sName)) ){` |
|       - | 1096 | `					/* The PH7_VmIsCallable gate above is php's creation-time screen; a pair it` |
|       - | 1097 | `					 * admitted because the class routes the name through __call is NOT settled. */` |
|      31 | 1098 | `					pFccObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|      15 | 1099 | `				}` |
|      18 | 1100 | `			}` |
|      37 | 1101 | `			return pFccObj;` |
|     ! 0 | 1102 | `		}else{` |
|       - | 1103 | `			/* [class-name, method] static callable -> bind the resolved scope. A runtime array` |
|       - | 1104 | `			 * callable carries a concrete class name (never self/static/parent), so a plain class` |
|       - | 1105 | ``			 * lookup is correct — unlike the syntactic `C::m(...)` path, which must resolve`` |
|       - | 1106 | `			 * self/static/parent via VmFccResolveScope. Matches PH7_VmIsCallable's own decode. */` |
|      19 | 1107 | `			ph7_class *pScopeCls = PH7_VmExtractClassFromValue(pVm, pTarget);` |
|      19 | 1108 | `			ph7_class_instance *pFccObj = pScopeCls` |
|      18 | 1109 | `				? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;` |
|      19 | 1110 | `			if( pFccObj ){` |
|      19 | 1111 | `				pFccObj->iFlags \|= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */` |
|      28 | 1112 | `				if( PH7_VmFccMethodIsDirect(pVm,pScopeCls,` |
|       9 | 1113 | `						SyStringData(&sName),SyStringLength(&sName)) ){` |
|      15 | 1114 | `					pFccObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       7 | 1115 | `				}` |
|       9 | 1116 | `			}` |
|      19 | 1117 | `			return pFccObj;` |
|       - | 1118 | `		}` |
|       - | 1119 | `	}` |
|      14 | 1120 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 1121 | `		/* __invoke object (a real Closure is intercepted by the caller before this point).` |
|       - | 1122 | ``		 * The `__invoke` name is the ENGINE's, so mark the closure: php dispatches a`` |
|       - | 1123 | ``		 * non-public __invoke through this wrapper exactly as it does through `$obj()`. */`` |
|      14 | 1124 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|       - | 1125 | `		ph7_class_instance *pWrap;` |
|       - | 1126 | `		SyString sInvoke;` |
|      14 | 1127 | `		SyStringInitFromBuf(&sInvoke, "__invoke", sizeof("__invoke") - 1);` |
|      14 | 1128 | `		pWrap = VmCreateClosure(pVm, &sInvoke, pObj, &pObj->pClass->sName);` |
|      14 | 1129 | `		if( pWrap ){` |
|      14 | 1130 | `			pWrap->iFlags \|= VM_INSTANCE_FCC_INVOKE_OBJ;` |
|       6 | 1131 | `		}` |
|      14 | 1132 | `		return pWrap;` |
|       - | 1133 | `	}` |
|       - | 1134 | `	/* Unreachable in practice — the PH7_VmIsCallable gate admits only string/array/object, all` |
|       - | 1135 | `	 * handled above; kept to satisfy the non-void return path. */` |
|     ! 0 | 1136 | `	return 0;` |
|     116 | 1137 | `}` |
|       - | 1138 | `/*` |
|       - | 1139 | ` * Return a fresh Closure instance (iRef==0 from create/clone) as a builtin result, taking the` |
|       - | 1140 | ` * one reference into pCtx->pRet — mirrors the OP_LOAD_FCC store convention.` |
|       - | 1141 | ` */` |
|     154 | 1142 | `static int VmClosureResult(ph7_context *pCtx, ph7_class_instance *pClosure)` |
|       5 | 1143 | `{` |
|     159 | 1144 | `	if( pClosure == 0 ){` |
|     ! 0 | 1145 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1146 | `		return PH7_OK;` |
|       - | 1147 | `	}` |
|     159 | 1148 | `	PH7_MemObjRelease(pCtx->pRet);` |
|       - | 1149 | `	/* Every caller hands over a FRESH instance, whose own reference is the one this` |
|       - | 1150 | `	 * return value takes (see OP_LOAD_CLOSURE). */` |
|     159 | 1151 | `	pCtx->pRet->x.pOther = pClosure;` |
|     159 | 1152 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|     159 | 1153 | `	return PH7_OK;` |
|      82 | 1154 | `}` |
|       - | 1155 | `/*` |
|       - | 1156 | ` * Overwrite a Closure clone's $__this (bound object, or 0 to unbind) and, when pScope != 0, its` |
|       - | 1157 | ` * $__scope (the class-name string; pScope->nByte==0 clears it). pScope==0 leaves $__scope as the` |
|       - | 1158 | ` * clone inherited it (PHP's "static" = keep-scope). Refreshes VM_INSTANCE_FCC_BOUND from the result.` |
|       - | 1159 | ` * The clone inherited a ref on the original $__this (PH7_CloneClassInstance copies via MemObjStore);` |
|       - | 1160 | ` * this drops that and takes one on pNewThis.` |
|       - | 1161 | ` */` |
|     112 | 1162 | `static void VmClosureRebind(ph7_class_instance *pClone,` |
|       - | 1163 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|       5 | 1164 | `{` |
|       - | 1165 | `	SyString sAttr;` |
|       - | 1166 | `	ph7_value *pThisAttr, *pScopeAttr;` |
|     117 | 1167 | `	int bBound = 0;` |
|     117 | 1168 | `	SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     117 | 1169 | `	pThisAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|     117 | 1170 | `	if( pThisAttr ){` |
|       - | 1171 | `		/* PH7_MemObjRelease already drops the cloned-in object's refcount for an OBJ value —` |
|       - | 1172 | `		 * do NOT also decrement by hand (that double-frees the original bound object). */` |
|     117 | 1173 | `		PH7_MemObjRelease(pThisAttr);` |
|     117 | 1174 | `		if( pNewThis ){` |
|      99 | 1175 | `			pThisAttr->x.pOther = pNewThis;` |
|      99 | 1176 | `			MemObjSetType(pThisAttr, MEMOBJ_OBJ);` |
|      99 | 1177 | `			pNewThis->iRef++;` |
|      47 | 1178 | `		}` |
|      56 | 1179 | `	}` |
|     117 | 1180 | `	if( pScope ){` |
|      80 | 1181 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      80 | 1182 | `		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      80 | 1183 | `		if( pScopeAttr ){` |
|      80 | 1184 | `			PH7_MemObjRelease(pScopeAttr);` |
|      80 | 1185 | `			if( pScope->nByte ){` |
|      80 | 1186 | `				PH7_MemObjStringAppend(pScopeAttr, pScope->zString, pScope->nByte);` |
|      38 | 1187 | `			}` |
|      38 | 1188 | `		}` |
|      38 | 1189 | `	}` |
|       - | 1190 | `	/* Refresh the FCC_BOUND fast-path flag from the resulting $__this/$__scope. pThisAttr already` |
|       - | 1191 | `	 * points at the final $__this slot; only $__scope needs a (re)fetch — the top half fetched it` |
|       - | 1192 | `	 * just for pScope != 0. */` |
|     117 | 1193 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     117 | 1194 | `	pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|     112 | 1195 | `	if( (pThisAttr && (pThisAttr->iFlags & MEMOBJ_OBJ))` |
|      70 | 1196 | `		\|\| (pScopeAttr && (pScopeAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeAttr->sBlob) > 0) ){` |
|     109 | 1197 | `		bBound = 1;` |
|      52 | 1198 | `	}` |
|     117 | 1199 | `	if( bBound ){` |
|     109 | 1200 | `		pClone->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      57 | 1201 | `	}else{` |
|      10 | 1202 | `		pClone->iFlags &= ~VM_INSTANCE_FCC_BOUND;` |
|       - | 1203 | `	}` |
|     117 | 1204 | `}` |
|       - | 1205 | `/*` |
|       - | 1206 | ` * Resolve the bindTo/bind/call $scope argument to a class-name SyString.` |
|       - | 1207 | ` * Returns 1 and fills *pOut if $__scope should be replaced (pOut->nByte==0 means "clear");` |
|       - | 1208 | ` * returns 0 to leave the scope unchanged (the PHP "static" sentinel / scope omitted).` |
|       - | 1209 | ` */` |
|     112 | 1210 | `static int VmClosureResolveScope(ph7_value *pScopeArg, SyString *pOut)` |
|       5 | 1211 | `{` |
|     117 | 1212 | `	if( pScopeArg == 0 ){` |
|      43 | 1213 | `		return 0; /* keep */` |
|       - | 1214 | `	}` |
|      72 | 1215 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeArg->sBlob) == 6` |
|      40 | 1216 | `		&& SyMemcmp((const void *)SyBlobData(&pScopeArg->sBlob), (const void *)"static", 6) == 0 ){` |
|       6 | 1217 | `		return 0; /* "static" -> keep current scope */` |
|       - | 1218 | `	}` |
|      72 | 1219 | `	if( pScopeArg->iFlags & MEMOBJ_NULL ){` |
|       3 | 1220 | `		SyStringInitFromBuf(pOut, 0, 0); /* unscoped */` |
|       3 | 1221 | `		return 1;` |
|       - | 1222 | `	}` |
|      70 | 1223 | `	if( pScopeArg->iFlags & MEMOBJ_OBJ ){` |
|       6 | 1224 | `		ph7_class_instance *pScopeObj = (ph7_class_instance *)pScopeArg->x.pOther;` |
|       6 | 1225 | `		*pOut = pScopeObj->pClass->sName;` |
|       6 | 1226 | `		return 1;` |
|       - | 1227 | `	}` |
|      66 | 1228 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) == 0 && (pScopeArg->iFlags & MEMOBJ_SCALAR) ){` |
|       - | 1229 | ``		/* php declares `object\|string\|null $newScope` and coerces a scalar into it in weak`` |
|       - | 1230 | ``		 * mode, so `bindTo($o, 5)` reaches the lookup as the NAME "5" and warns that no such`` |
|       - | 1231 | `		 * class exists. Falling through as "keep the current scope" bound it silently. */` |
|       3 | 1232 | `		PH7_MemObjToString(pScopeArg);` |
|       1 | 1233 | `	}` |
|      66 | 1234 | `	if( pScopeArg->iFlags & MEMOBJ_STRING ){` |
|      66 | 1235 | `		SyStringInitFromBuf(pOut, (const char *)SyBlobData(&pScopeArg->sBlob), SyBlobLength(&pScopeArg->sBlob));` |
|      66 | 1236 | `		return 1;` |
|       - | 1237 | `	}` |
|     ! 0 | 1238 | `	return 0;` |
|      61 | 1239 | `}` |
|       - | 1240 | `/*` |
|       - | 1241 | ` * Fiber::getCurrent() — the fiber the running code is INSIDE, or null in the main` |
|       - | 1242 | ` * flow. php answers EG(active_fiber), which is the fiber whose body is on the` |
|       - | 1243 | ` * stack, not the innermost coroutine: a GENERATOR iterated from inside a fiber` |
|       - | 1244 | ` * leaves the fiber current, and code running after a fiber suspends back to its` |
|       - | 1245 | ` * caller is outside it again. pVm->pCurFiber is that name, saved and restored` |
|       - | 1246 | ` * around every start/resume, so the nesting is the call structure itself.` |
|       - | 1247 | ` *` |
|       - | 1248 | ` * Reached by every library that logs or schedules per-fiber: monolog asks for it` |
|       - | 1249 | ` * on EVERY record it writes.` |
|       - | 1250 | ` */` |
|      28 | 1251 | `PH7_PRIVATE int vm_builtin_Fiber_getCurrent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 1252 | `{` |
|      29 | 1253 | `	ph7_vm *pVm = pCtx->pVm;` |
|      14 | 1254 | `	SXUNUSED(apArg);` |
|      14 | 1255 | `	SXUNUSED(nArg);` |
|      29 | 1256 | `	if( pVm->pCurFiber == 0 ){` |
|      13 | 1257 | `		ph7_result_null(pCtx);` |
|      13 | 1258 | `		return PH7_OK;` |
|       - | 1259 | `	}` |
|       - | 1260 | `	/* The result slot takes a reference of its own: pCurFiber is borrowed from the` |
|       - | 1261 | `	 * activation that set it, and the value handed back outlives that call. */` |
|      17 | 1262 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      17 | 1263 | `	pVm->pCurFiber->iRef++;` |
|      17 | 1264 | `	pCtx->pRet->x.pOther = pVm->pCurFiber;` |
|      17 | 1265 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|      17 | 1266 | `	return PH7_OK;` |
|      15 | 1267 | `}` |
|       - | 1268 | `/*` |
|       - | 1269 | ``  * Fiber's C-bodied methods. Every one of them was a global `__fiber_verb($this,…)` `` |
|       - | 1270 | ` * thunk that a one-line prelude method forwarded to; the class body in the builtin` |
|       - | 1271 | ` * chunk now holds only its two private slots.` |
|       - | 1272 | ` */` |
|    6721 | 1273 | `PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm)` |
|       5 | 1274 | `{` |
|       - | 1275 | `	/* php's own declaration ORDER, which is what get_class_methods() and` |
|       - | 1276 | `	 * ReflectionClass::getMethods() answer in. One row still differs from php's` |
|       - | 1277 | ``	 * list and is recorded in PLAN.md: `__destruct` is the engine's teardown hook`` |
|       - | 1278 | `	 * published as a method name php does not have (Generator's is the twin). */` |
|       - | 1279 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 1280 | `		{ "__construct",  PH7_MOD_PUBLIC, "callable $callback",  "",       vm_builtin_Fiber_construct },` |
|       - | 1281 | `		/* Variadic: the arguments now reach the C body directly instead of being` |
|       - | 1282 | `		 * repackaged by a func_get_args() call in the prelude. */` |
|       - | 1283 | `		{ "start",        PH7_MOD_PUBLIC, "mixed ...$args",      "mixed",  vm_builtin_Fiber_start },` |
|       - | 1284 | `		{ "resume",       PH7_MOD_PUBLIC, "mixed $value = null", "mixed",  vm_builtin_Fiber_resume },` |
|       - | 1285 | `		{ "throw",        PH7_MOD_PUBLIC, "Throwable $exception", "mixed", vm_builtin_Fiber_throw },` |
|       - | 1286 | `		{ "isStarted",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isStarted },` |
|       - | 1287 | `		{ "isSuspended",  PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isSuspended },` |
|       - | 1288 | `		{ "isRunning",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isRunning },` |
|       - | 1289 | `		{ "isTerminated", PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isTerminated },` |
|       - | 1290 | `		{ "getReturn",    PH7_MOD_PUBLIC, "",                    "mixed",  vm_builtin_Fiber_getReturn },` |
|       - | 1291 | `		/* Static like suspend, and the one method a program calls without holding a` |
|       - | 1292 | `		 * fiber at all — it is how code asks whether it is inside one. */` |
|       - | 1293 | `		{ "getCurrent",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "",                    "?Fiber",` |
|       - | 1294 | `		  vm_builtin_Fiber_getCurrent },` |
|       - | 1295 | `		/* Static, and the only one that never took a receiver even as a thunk:` |
|       - | 1296 | ``		 * `__fiber_suspend($value)` already read the value from argument #0. */`` |
|       - | 1297 | `		{ "suspend",      PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "mixed $value = null", "mixed",` |
|       - | 1298 | `		  vm_builtin_Fiber_suspend },` |
|       - | 1299 | `		{ "__destruct",   PH7_MOD_PUBLIC, "",                    "",       vm_builtin_Fiber_destruct },` |
|       - | 1300 | `	};` |
|       - | 1301 | `	/* The two private slots the methods above keep their state in: the execution` |
|       - | 1302 | `	 * context (a resource) and the callable handed to the constructor. */` |
|       - | 1303 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 1304 | `		{ "__ctx",      PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1305 | `		{ "__callable", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1306 | `	};` |
|       - | 1307 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1308 | `		"Fiber", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE,` |
|       - | 1309 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|       - | 1310 | `		0, 0,` |
|       - | 1311 | `		aProp, SX_ARRAYSIZE(aProp),` |
|       - | 1312 | `		0, 0, 0` |
|       - | 1313 | `	};` |
|    6726 | 1314 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|       5 | 1315 | `}` |
|       - | 1316 | `/*` |
|       - | 1317 | `` * Generator's C-bodied methods, plus the `implements Iterator` the chunk can no`` |
|       - | 1318 | ` * longer carry: PH7_ClassImplement installs an abstract stub for any interface` |
|       - | 1319 | ` * method the class does not already declare, so it has to run AFTER the eight` |
|       - | 1320 | ` * methods below exist — at which point the stubs are skipped and the class is` |
|       - | 1321 | ` * concrete, exactly as the prelude declaration used to make it.` |
|       - | 1322 | ` */` |
|    6721 | 1323 | `PH7_PRIVATE sxi32 PH7_VmInstallGeneratorNative(ph7_vm *pVm)` |
|       5 | 1324 | `{` |
|       - | 1325 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 1326 | `		{ "current",    PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_current },` |
|       - | 1327 | `		{ "key",        PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_key },` |
|       - | 1328 | `		{ "next",       PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_next },` |
|       - | 1329 | `		{ "rewind",     PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_rewind },` |
|       - | 1330 | `		{ "valid",      PH7_MOD_PUBLIC, "",                 "bool",  vm_builtin_Generator_valid },` |
|       - | 1331 | ``		/* php REQUIRES the argument here; the prelude declared `$value = null`, so`` |
|       - | 1332 | ``		 * `$gen->send()` used to answer the first yielded value instead of raising. */`` |
|       - | 1333 | `		{ "send",       PH7_MOD_PUBLIC, "mixed $value",     "mixed", vm_builtin_Generator_send },` |
|       - | 1334 | `		{ "throw",      PH7_MOD_PUBLIC, "Throwable $exception", "mixed", vm_builtin_Generator_throw },` |
|       - | 1335 | `		{ "getReturn",  PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_getReturn },` |
|       - | 1336 | `		{ "__destruct", PH7_MOD_PUBLIC, "",                 "",      vm_builtin_Generator_destruct },` |
|       - | 1337 | `	};` |
|       - | 1338 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 1339 | `		{ "__ctx", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1340 | `	};` |
|       - | 1341 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1342 | `		{ "Generator", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOSERIALIZE,` |
|       - | 1343 | `		  aMethod, SX_ARRAYSIZE(aMethod),` |
|       - | 1344 | `		  0, 0,` |
|       - | 1345 | `		  aProp, SX_ARRAYSIZE(aProp),` |
|       - | 1346 | `		  0, 0, 0 },` |
|       - | 1347 | `		/* php declares this one beside Generator and throws it from nowhere a` |
|       - | 1348 | `		 * script can reach: it is the exception a RESUME of a generator that has` |
|       - | 1349 | `		 * been closed would carry, and php's own paths answer null there. A` |
|       - | 1350 | `		 * program may still name it, catch it and throw it, so it is declared. */` |
|       - | 1351 | `		{ "ClosedGeneratorException", "Exception", 0, 0,` |
|       - | 1352 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1353 | `	};` |
|       - | 1354 | `	ph7_class *pClass;` |
|       - | 1355 | `	ph7_class *pIterator;` |
|    6726 | 1356 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    6726 | 1357 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1358 | `		return rc;` |
|       - | 1359 | `	}` |
|       - | 1360 | ``	/* `implements Iterator` last, for the reason in this function's header. */`` |
|    6726 | 1361 | `	pClass = PH7_VmExtractClass(&(*pVm),"Generator",sizeof("Generator")-1,0,0);` |
|    6726 | 1362 | `	if( pClass ){` |
|       - | 1363 | ``		/* php refuses `new Generator` -- one only ever comes out of a call to a`` |
|       - | 1364 | `		 * function that yields -- and words the refusal per class. */` |
|    6726 | 1365 | `		pClass->zNewRefusal = "The \"Generator\" class is reserved for internal use "` |
|       - | 1366 | `			"and cannot be manually instantiated";` |
|    3356 | 1367 | `	}` |
|    6726 | 1368 | `	pIterator = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,0,0);` |
|    6726 | 1369 | `	if( pClass == 0 \|\| pIterator == 0 ){` |
|     ! 0 | 1370 | `		return SXERR_NOTFOUND;` |
|       - | 1371 | `	}` |
|    6726 | 1372 | `	return PH7_ClassImplement(pClass,pIterator);` |
|    3361 | 1373 | `}` |
|       - | 1374 | `/*` |
|       - | 1375 | ` * Closure::__construct() — php declares it PRIVATE and still words the refusal as` |
|       - | 1376 | ` * an instantiation error rather than a visibility one, so the body has to exist.` |
|       - | 1377 | ` */` |
|     ! 0 | 1378 | `PH7_PRIVATE int vm_builtin_Closure_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     ! 0 | 1379 | `{` |
|     ! 0 | 1380 | `	SXUNUSED(nArg);` |
|     ! 0 | 1381 | `	SXUNUSED(apArg);` |
|     ! 0 | 1382 | `	return PH7_VmThrowException(pCtx, "Error",` |
|       - | 1383 | `		"Instantiation of class Closure is not allowed");` |
|     ! 0 | 1384 | `}` |
|       - | 1385 | `/*` |
|       - | 1386 | ` * Clone a Closure for a rebind, carrying the marks that say WHAT it wraps. They live on the` |
|       - | 1387 | ` * instance rather than in an attribute, so PH7_CloneClassInstance — which copies attributes —` |
|       - | 1388 | `` * left them behind: the clone of a method callable forgot that its `$__fn` names a screened`` |
|       - | 1389 | `` * METHOD, and `$fcc->bindTo($other)` came back as a closure whose every dispatch re-resolved`` |
|       - | 1390 | ` * the name and re-decided its visibility (or, with no method of that name in reach, looked` |
|       - | 1391 | ` * for a global FUNCTION).` |
|       - | 1392 | ` */` |
|     112 | 1393 | `static ph7_class_instance * VmCloneClosureInstance(ph7_class_instance *pClosure)` |
|       5 | 1394 | `{` |
|     117 | 1395 | `	ph7_class_instance *pClone = PH7_CloneClassInstance(pClosure);` |
|     117 | 1396 | `	if( pClone ){` |
|     173 | 1397 | `		pClone->iFlags \|= pClosure->iFlags` |
|     112 | 1398 | `			& (VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED\|VM_INSTANCE_FCC_INVOKE_OBJ);` |
|      56 | 1399 | `	}` |
|     117 | 1400 | `	return pClone;` |
|       5 | 1401 | `}` |
|       - | 1402 | `/*` |
|       - | 1403 | `` * Is this closure STATIC — one that can never take a `$this`? For a plain closure that is the`` |
|       - | 1404 | `` * `static function(){}` declaration flag; for a METHOD callable it is the method's own`` |
|       - | 1405 | `` * staticness, which the flag cannot see because `$__fn` names a method and not a function in`` |
|       - | 1406 | `` * hFunction. `Base::stat(...)` is exactly as static as `static fn()` to php.`` |
|       - | 1407 | ` */` |
|     114 | 1408 | `static int VmClosureIsStatic(ph7_vm *pVm, ph7_class_instance *pClosure)` |
|       5 | 1409 | `{` |
|       - | 1410 | `	SyString sAttr;` |
|       - | 1411 | `	ph7_value *pFn;` |
|     119 | 1412 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|     119 | 1413 | `	pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|     119 | 1414 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 | 1415 | `		return 0;` |
|       - | 1416 | `	}` |
|     119 | 1417 | `	if( pClosure->iFlags & VM_INSTANCE_FCC_METHOD ){` |
|      34 | 1418 | `		ph7_class *pScope = PH7_VmClosureScopeClass(pVm, pClosure);` |
|      34 | 1419 | `		ph7_class_method *pMeth = pScope` |
|      48 | 1420 | `			? PH7_ClassExtractMethod(pScope, (const char *)SyBlobData(&pFn->sBlob),` |
|      32 | 1421 | `				SyBlobLength(&pFn->sBlob)) : 0;` |
|      34 | 1422 | `		return (pMeth && (pMeth->iFlags & PH7_CLASS_ATTR_STATIC)) ? 1 : 0;` |
|       - | 1423 | `	}` |
|       - | 1424 | `	{` |
|     128 | 1425 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob),` |
|      41 | 1426 | `			SyBlobLength(&pFn->sBlob));` |
|      87 | 1427 | `		return (pEntry && (((ph7_vm_func *)pEntry->pUserData)->iFlags & VM_FUNC_STATIC_CL)) ? 1 : 0;` |
|       - | 1428 | `	}` |
|      62 | 1429 | `}` |
|       - | 1430 | `/*` |
|       - | 1431 | ` * php's four refusals for a rebind (zend_valid_closure_binding), in php's order. A closure` |
|       - | 1432 | `` * created from a METHOD is a "fake closure": it wraps a resolved function, so its `$this` may`` |
|       - | 1433 | ` * only move WITHIN the class that declared it and its scope may not move at all. PHL applied` |
|       - | 1434 | `` * none of them beyond the static-closure one, so `$fcc->bindTo($unrelated)` handed back a`` |
|       - | 1435 | `` * closure that could only fail later, and `->bindTo(null)` one with no receiver for a method`` |
|       - | 1436 | ` * that needs one. Each refusal is php's E_WARNING plus a NULL result; returns 0 when it fired.` |
|       - | 1437 | ` *` |
|       - | 1438 | `` * pScope is the resolved $scope ARGUMENT (0 when omitted or `"static"`, which both mean keep);`` |
|       - | 1439 | `` * an empty one is an explicit `null`, which php counts as a rebind like any other.`` |
|       - | 1440 | ` */` |
|     130 | 1441 | `static int VmClosureBindAllowed(ph7_vm *pVm, ph7_class_instance *pClosure,` |
|       - | 1442 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|       5 | 1443 | `{` |
|     135 | 1444 | `	int bMethod = (pClosure->iFlags & VM_INSTANCE_FCC_METHOD) != 0;` |
|     135 | 1445 | `	ph7_class *pOwn = bMethod ? PH7_VmClosureScopeClass(pVm, pClosure) : 0;` |
|     135 | 1446 | `	if( pNewThis ){` |
|     113 | 1447 | `		if( VmClosureIsStatic(pVm, pClosure) ){` |
|       6 | 1448 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1449 | `				"Cannot bind an instance to a static closure, this will be an error in PHP 9");` |
|       6 | 1450 | `			return 0;` |
|       - | 1451 | `		}` |
|     109 | 1452 | `		if( pOwn && !PH7_VmInstanceOf(pNewThis->pClass, pOwn) ){` |
|       - | 1453 | `			SyString sFn;` |
|       - | 1454 | `			ph7_value *pFn;` |
|       5 | 1455 | `			SyStringInitFromBuf(&sFn, "__fn", 4);` |
|       5 | 1456 | `			pFn = PH7_ClassInstanceFetchAttr(pClosure, &sFn);` |
|       5 | 1457 | `			SyStringInitFromBuf(&sFn, pFn ? (const char *)SyBlobData(&pFn->sBlob) : "",` |
|       - | 1458 | `				pFn ? SyBlobLength(&pFn->sBlob) : 0);` |
|       7 | 1459 | `			VmErrorFormat(pVm,PH7_CTX_WARNING,` |
|       - | 1460 | `				"Cannot bind method %z::%z() to object of class %z, this will be an error in PHP 9",` |
|       4 | 1461 | `				&pOwn->sName,&sFn,&pNewThis->pClass->sName);` |
|       5 | 1462 | `			return 0;` |
|       5 | 1463 | `		}` |
|      74 | 1464 | `	}else if( pOwn && !VmClosureIsStatic(pVm, pClosure) ){` |
|       5 | 1465 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1466 | `			"Cannot unbind $this of method, this will be an error in PHP 9");` |
|       5 | 1467 | `		return 0;` |
|       - | 1468 | `	}` |
|     123 | 1469 | `	if( pScope && bMethod ){` |
|      16 | 1470 | `		ph7_class *pWant = pScope->nByte` |
|       9 | 1471 | `			? PH7_VmResolveScopeName(pVm, pScope->zString, pScope->nByte) : 0;` |
|      11 | 1472 | `		if( pWant != pOwn ){` |
|       7 | 1473 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 1474 | `				"Cannot rebind scope of closure created from method, this will be an error in PHP 9");` |
|       7 | 1475 | `			return 0;` |
|       - | 1476 | `		}` |
|       2 | 1477 | `	}` |
|     117 | 1478 | `	return 1;` |
|      70 | 1479 | `}` |
|       - | 1480 | `/*` |
|       - | 1481 | ` * Closure::call(object $newThis, mixed ...$args) — bind and invoke in one step.` |
|       - | 1482 | ` *` |
|       - | 1483 | `` * This was the last PHP left in the class: `$bound = $this->bindTo($newThis,`` |
|       - | 1484 | `` * get_class($newThis)); return $bound(...$args);`. That spelling leaked its own`` |
|       - | 1485 | `` * internals — a non-object argument reported `get_class(): Argument #1 ($object)`` |
|       - | 1486 | `` * must be of type object, string given` where php names THIS method's parameter,`` |
|       - | 1487 | `` * which is what declaring `object $newThis` buys (the shared screen words it).`` |
|       - | 1488 | ` * The scope php binds is the new $this's class, exactly as the PHP did.` |
|       - | 1489 | ` */` |
|      28 | 1490 | `PH7_PRIVATE int vm_builtin_Closure_call(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 1491 | `{` |
|      30 | 1492 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1493 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|      30 | 1494 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       - | 1495 | `	ph7_value sBound;` |
|       - | 1496 | `	SyString sScope;` |
|       - | 1497 | `	sxi32 rc;` |
|      30 | 1498 | `	if( nArg < 1 ){` |
|     ! 0 | 1499 | `		return PH7_VmThrowException(pCtx, "ArgumentCountError",` |
|       - | 1500 | `			"Closure::call() expects at least 1 argument, 0 given");` |
|       - | 1501 | `	}` |
|      30 | 1502 | `	if( pRecv == 0 \|\| !VmValueIsClosure(pVm, pRecv) ){` |
|     ! 0 | 1503 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1504 | `		return PH7_OK;` |
|       - | 1505 | `	}` |
|      30 | 1506 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| apArg[0]->x.pOther == 0 ){` |
|       - | 1507 | ``		/* Unreachable while the declared `object $newThis` is screened; kept because`` |
|       - | 1508 | `		 * rule 44's family says a screen written for one body shape has not` |
|       - | 1509 | `		 * necessarily run for this one. */` |
|     ! 0 | 1510 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1511 | `			"Closure::call(): Argument #1 ($newThis) must be of type object");` |
|       - | 1512 | `	}` |
|      30 | 1513 | `	pClosure = (ph7_class_instance *)pRecv->x.pOther;` |
|      30 | 1514 | `	pNewThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      30 | 1515 | `	SyStringInitFromBuf(&sScope, pNewThis->pClass->sName.zString, pNewThis->pClass->sName.nByte);` |
|       - | 1516 | `	/* call() BINDS before it invokes, so php's rebind refusals apply to it — and because the` |
|       - | 1517 | `	 * scope it asks for is the new $this's class, a method callable handed an instance of` |
|       - | 1518 | `	 * anything but its own declaring class is refused for the scope, not the receiver. */` |
|      30 | 1519 | `	if( !VmClosureBindAllowed(pVm, pClosure, pNewThis, &sScope) ){` |
|       3 | 1520 | `		ph7_result_null(pCtx);` |
|       3 | 1521 | `		return PH7_OK;` |
|       - | 1522 | `	}` |
|      28 | 1523 | `	pClone = VmCloneClosureInstance(pClosure);` |
|      28 | 1524 | `	if( pClone == 0 ){` |
|     ! 0 | 1525 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1526 | `		return PH7_OK;` |
|       - | 1527 | `	}` |
|      28 | 1528 | `	VmClosureRebind(pClone, pNewThis, &sScope);` |
|       - | 1529 | `	/* The bound closure is handed to the dispatcher through a STACK carrier that` |
|       - | 1530 | `	 * takes its own reference (rule 16): a context value would be released with the` |
|       - | 1531 | `	 * call context and unref the instance a second time. */` |
|      28 | 1532 | `	PH7_MemObjInit(pVm, &sBound);` |
|      28 | 1533 | `	sBound.x.pOther = pClone;` |
|      28 | 1534 | `	MemObjSetType(&sBound, MEMOBJ_OBJ);` |
|       - | 1535 | `	/* The clone's own reference is this carrier's, and releasing the carrier below` |
|       - | 1536 | `	 * is what ends the temporary (see OP_LOAD_CLOSURE). */` |
|      28 | 1537 | `	rc = PH7_VmCallUserFunction(pVm, &sBound, nArg - 1, apArg + 1, pCtx->pRet);` |
|      28 | 1538 | `	PH7_MemObjRelease(&sBound);` |
|      28 | 1539 | `	return rc;` |
|      16 | 1540 | `}` |
|       - | 1541 | `/*` |
|       - | 1542 | ` * Closure — declared entirely from C.` |
|       - | 1543 | ` *` |
|       - | 1544 | ` * Its three methods were the first in the engine whose body is a C routine rather` |
|       - | 1545 | `` * than bytecode (VM_FUNC_NATIVE), retiring the global `__closure_bindTo` /`` |
|       - | 1546 | `` * `__closure_fromCallable` thunks a prelude method used to forward to. The`` |
|       - | 1547 | ` * DECLARATION stayed in the builtin chunk until now, which cost three things: the` |
|       - | 1548 | `` * engine slots `$__fn`/`$__this`/`$__scope` were on every presentation surface`` |
|       - | 1549 | `` * (php's Closure has NO properties), `__construct` was public where php's is`` |
|       - | 1550 | `` * private, and `call()` was PHP that leaked `get_class()`'s diagnostic.`` |
|       - | 1551 | ` */` |
|    6721 | 1552 | `PH7_PRIVATE sxi32 PH7_VmInstallClosureNative(ph7_vm *pVm)` |
|       5 | 1553 | `{` |
|       - | 1554 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 1555 | `		/* Parameter names are php's own ($newScope, not $scope): this string is the` |
|       - | 1556 | `		 * declaration of record for arity, by-ref positions and the reported` |
|       - | 1557 | `		 * parameter list. */` |
|       - | 1558 | `		{ "__construct",  PH7_MOD_PRIVATE, "", 0,` |
|       - | 1559 | `		  vm_builtin_Closure_construct },` |
|       - | 1560 | `		{ "bindTo",       PH7_MOD_PUBLIC,` |
|       - | 1561 | `		  "?object $newThis, object\|string\|null $newScope = \"static\"", "?Closure",` |
|       - | 1562 | `		  vm_builtin_Closure_bindTo },` |
|       - | 1563 | `		{ "bind",         PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|       - | 1564 | `		  "Closure $closure, ?object $newThis, object\|string\|null $newScope = \"static\"", "?Closure",` |
|       - | 1565 | `		  vm_builtin_Closure_bindTo },` |
|       - | 1566 | `		{ "call",         PH7_MOD_PUBLIC,` |
|       - | 1567 | `		  "object $newThis, mixed ...$args", "mixed",` |
|       - | 1568 | `		  vm_builtin_Closure_call },` |
|       - | 1569 | `		{ "fromCallable", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|       - | 1570 | `		  "callable $callback", "Closure",` |
|       - | 1571 | `		  vm_builtin_Closure_fromCallable },` |
|       - | 1572 | `	};` |
|       - | 1573 | `	/* The engine's own slots: the callable NAME, the bound receiver and the bound` |
|       - | 1574 | `	 * scope. php presents no property at all for a Closure, so all three carry` |
|       - | 1575 | ``	 * PH7_MOD_HIDDEN — they keep working for `new`, `clone` and the C bodies (and`` |
|       - | 1576 | `	 * for serialize(), which this class refuses anyway) and disappear from` |
|       - | 1577 | `	 * var_dump/print_r/(array)/get_object_vars/foreach/json_encode and Reflection. */` |
|       - | 1578 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 1579 | `		{ "__fn",    PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1580 | `		{ "__this",  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1581 | `		{ "__scope", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1582 | `	};` |
|       - | 1583 | `	/* php's get_debug_info for a Closure shows a SHAPE none of those three slots` |
|       - | 1584 | `	 * is (name/file/line or function, static, this, parameter) — see` |
|       - | 1585 | `	 * PH7_ClosurePresent, which lives beside the reflection machinery that already` |
|       - | 1586 | `	 * describes any callable's parameters. */` |
|       - | 1587 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1588 | `		"Closure", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOINSTANTIATE,` |
|       - | 1589 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|       - | 1590 | `		0, 0,` |
|       - | 1591 | `		aProp, SX_ARRAYSIZE(aProp),` |
|       - | 1592 | `		VmClosureRelease, 0, PH7_ClosurePresent` |
|       - | 1593 | `	};` |
|    6726 | 1594 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|       5 | 1595 | `}` |
|       - | 1596 | `/*` |
|       - | 1597 | ` * Closure::bindTo($newThis, $scope='static') / Closure::bind($closure, $newThis, $scope='static').` |
|       - | 1598 | ` * Clone the receiver and rebind $this/$scope; returns the new Closure (NULL on a non-Closure` |
|       - | 1599 | ` * receiver, matching PHP's failure mode).` |
|       - | 1600 | ` */` |
|     112 | 1601 | `PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1602 | `{` |
|     117 | 1603 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1604 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|       - | 1605 | `	ph7_value *pNewThisArg;` |
|       - | 1606 | `	ph7_value *pRecv;` |
|       - | 1607 | `	SyString sScope;` |
|     117 | 1608 | `	const SyString *pScopePtr = 0;` |
|       - | 1609 | `	/* One body, both spellings — as it always was, except the closure now arrives` |
|       - | 1610 | `	 * the way php passes it rather than as a hand-written first argument. Called as` |
|       - | 1611 | `	 * the instance method bindTo(), the receiver IS the closure and the arguments` |
|       - | 1612 | `	 * start at $newThis; called as the static bind(), the closure is argument #1.` |
|       - | 1613 | `	 * Normalizing here is what lets the two share an implementation. */` |
|     117 | 1614 | `	if( PH7_ContextThis(pCtx) ){` |
|      82 | 1615 | `		pRecv = PH7_ContextThisValue(pCtx);` |
|      43 | 1616 | `	}else{` |
|      37 | 1617 | `		if( nArg < 1 ){` |
|     ! 0 | 1618 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1619 | `			return PH7_OK;` |
|       - | 1620 | `		}` |
|      37 | 1621 | `		pRecv = apArg[0];` |
|      37 | 1622 | `		apArg++;` |
|      37 | 1623 | `		nArg--;` |
|       - | 1624 | `	}` |
|     117 | 1625 | `	if( nArg < 1 \|\| !VmValueIsClosure(pVm, pRecv) ){` |
|     ! 0 | 1626 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1627 | `		return PH7_OK;` |
|       - | 1628 | `	}` |
|     117 | 1629 | `	pClosure = (ph7_class_instance *)pRecv->x.pOther;` |
|     117 | 1630 | `	pNewThisArg = apArg[0];` |
|     117 | 1631 | `	if( pNewThisArg->iFlags & MEMOBJ_NULL ){` |
|      27 | 1632 | `		pNewThis = 0;` |
|     105 | 1633 | `	}else if( pNewThisArg->iFlags & MEMOBJ_OBJ ){` |
|      93 | 1634 | `		pNewThis = (ph7_class_instance *)pNewThisArg->x.pOther;` |
|      49 | 1635 | `	}else{` |
|     ! 0 | 1636 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1637 | `			"Closure::bindTo(): Argument #1 ($newThis) must be of type ?object");` |
|       - | 1638 | `	}` |
|     117 | 1639 | `	if( VmClosureResolveScope((nArg > 1) ? apArg[1] : 0, &sScope) ){` |
|      72 | 1640 | `		pScopePtr = &sScope;` |
|      34 | 1641 | `	}` |
|       - | 1642 | `	/* php RESOLVES the scope argument before it decides anything else, and a name no class` |
|       - | 1643 | `	 * answers to is its own warning — ahead of all four rebind refusals, for a plain closure` |
|       - | 1644 | `	 * as much as for a method one. PHL bound the unresolvable scope in silence and then had` |
|       - | 1645 | ``	 * no scope at all, so `bindTo($o, 'Typo')` produced a closure that could not reach the`` |
|       - | 1646 | `	 * private members it was being bound for. */` |
|     112 | 1647 | `	if( pScopePtr && pScopePtr->nByte` |
|      72 | 1648 | `	 && PH7_VmResolveScopeName(pVm, pScopePtr->zString, pScopePtr->nByte) == 0 ){` |
|      11 | 1649 | `		VmErrorFormat(pVm,PH7_CTX_WARNING,"Class \"%z\" not found",pScopePtr);` |
|      11 | 1650 | `		ph7_result_null(pCtx);` |
|      11 | 1651 | `		return PH7_OK;` |
|       - | 1652 | `	}` |
|     107 | 1653 | `	if( !VmClosureBindAllowed(pVm, pClosure, pNewThis, pScopePtr) ){` |
|      18 | 1654 | `		ph7_result_null(pCtx);` |
|      18 | 1655 | `		return PH7_OK;` |
|       - | 1656 | `	}` |
|      91 | 1657 | `	pClone = VmCloneClosureInstance(pClosure);` |
|      91 | 1658 | `	if( pClone == 0 ){` |
|     ! 0 | 1659 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1660 | `		return PH7_OK;` |
|       - | 1661 | `	}` |
|      91 | 1662 | `	VmClosureRebind(pClone, pNewThis, pScopePtr);` |
|      91 | 1663 | `	return VmClosureResult(pCtx, pClone);` |
|      61 | 1664 | `}` |
|       - | 1665 | `/*` |
|       - | 1666 | ` * Closure::fromCallable($callable) — normalize any callable value to a Closure (reuses the` |
|       - | 1667 | ` * VmFccWrapValue primitive); idempotent on a Closure; TypeError on a non-callable.` |
|       - | 1668 | ` */` |
|      90 | 1669 | `PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       2 | 1670 | `{` |
|      92 | 1671 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1672 | `	ph7_class_instance *pClosure;` |
|      92 | 1673 | `	if( nArg < 1 ){` |
|     ! 0 | 1674 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1675 | `			"Closure::fromCallable() expects exactly 1 argument, 0 given");` |
|       - | 1676 | `	}` |
|      92 | 1677 | `	if( VmValueIsClosure(pVm, apArg[0]) ){` |
|       3 | 1678 | `		ph7_result_value(pCtx, apArg[0]); /* already a Closure: idempotent */` |
|       3 | 1679 | `		return PH7_OK;` |
|       - | 1680 | `	}` |
|      90 | 1681 | `	pClosure = VmFccWrapValue(pVm, apArg[0]);` |
|      90 | 1682 | `	if( pClosure == 0 ){` |
|       - | 1683 | `		/* php says WHY, with the same reason taxonomy every callback argument uses —` |
|       - | 1684 | ``		 * `Failed to create closure from callable: class P does not have a method "zz"`.`` |
|       - | 1685 | `		 * PHL answered one flat "is not a valid callback" for all eight causes, so a typo` |
|       - | 1686 | `		 * in a method name, a private one, a missing class and a bad array shape were` |
|       - | 1687 | `		 * indistinguishable. PH7_VmCallableReason is the shared builder (its tails are` |
|       - | 1688 | `		 * already byte-exact for call_user_func & friends); the fallback covers the OOM` |
|       - | 1689 | `		 * path, where the value IS callable and the reason is 0. */` |
|       - | 1690 | `		char zWhy[192];` |
|      21 | 1691 | `		const char *zReason = PH7_VmCallableReason(pVm, apArg[0], zWhy, sizeof(zWhy));` |
|      21 | 1692 | `		if( zReason ){` |
|      31 | 1693 | `			return PH7_VmThrowException(pCtx, "TypeError",` |
|      10 | 1694 | `				"Failed to create closure from callable: %s", zReason);` |
|       - | 1695 | `		}` |
|     ! 0 | 1696 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 1697 | `			"Failed to create closure from callable");` |
|       - | 1698 | `	}` |
|      70 | 1699 | `	return VmClosureResult(pCtx, pClosure);` |
|      47 | 1700 | `}` |
|       - | 1701 | `/*` |
|       - | 1702 | ` * Fiber::suspend($value = null) — static method.` |
|       - | 1703 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|       - | 1704 | ` */` |
|     398 | 1705 | `PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1706 | `{` |
|     403 | 1707 | `	ph7_vm *pVm = pCtx->pVm;` |
|     403 | 1708 | `	if( pVm->pActiveCtx == 0 ){` |
|     ! 0 | 1709 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1710 | `			"Cannot suspend outside of a fiber");` |
|       - | 1711 | `	}` |
|       - | 1712 | `	/* Stage 4 scoped divergence: the trampoline only makes PHP->PHP CALLs` |
|       - | 1713 | `	 * iterative. Every OTHER re-entry runs on a fresh native VmByteCodeExec` |
|       - | 1714 | `	 * activation (nVmExecDepth bumped) that PH7_SUSPEND cannot unwind across` |
|       - | 1715 | `	 * without real coroutine stacks (BYTECODE.md §2.4): a C->PHP callback` |
|       - | 1716 | `	 * (usort/array_map/preg_replace_callback comparator), and — because fibers` |
|       - | 1717 | `	 * use the LEGACY (non-inline) try/catch machinery — a suspend inside a` |
|       - | 1718 | `	 * fiber's catch/finally body, a match/switch arm, or eval()/include()'d` |
|       - | 1719 | `	 * code, all of which run via VmLocalExec. php does all of these via full` |
|       - | 1720 | `	 * native-stack switching; PHL raises a catchable FiberError instead of the` |
|       - | 1721 | `	 * old silent corruption. A suspend in a fiber's try BODY (not catch/finally)` |
|       - | 1722 | `	 * runs in the main dispatch loop and parks normally. A recorded` |
|       - | 1723 | `	 * residual; making the catch/finally case work needs fibers on the inline` |
|       - | 1724 | `	 * try machinery (the generator ROOT C path), a follow-up. */` |
|     403 | 1725 | `	if( pVm->nVmExecDepth != pVm->pActiveCtx->nBodyExecDepth ){` |
|      10 | 1726 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1727 | `			"Cannot suspend across an internal call boundary");` |
|       - | 1728 | `	}` |
|     395 | 1729 | `	if( nArg > 0 ){` |
|     387 | 1730 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|     196 | 1731 | `	}else{` |
|      12 | 1732 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|       - | 1733 | `	}` |
|     395 | 1734 | `	return PH7_SUSPEND;` |
|     204 | 1735 | `}` |
|       - | 1736 | `/*` |
|       - | 1737 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|       - | 1738 | ` * Actual resolution is deferred to start() so that overload selection` |
|       - | 1739 | ` * and closure-environment binding happen with the correct argument context.` |
|       - | 1740 | ` */` |
|     360 | 1741 | `PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 1742 | `{` |
|       - | 1743 | `	ph7_class_instance *pThis;` |
|       - | 1744 | `	ph7_value *pAttr;` |
|       - | 1745 | `	SyString sAttrName;` |
|     365 | 1746 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     365 | 1747 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|     365 | 1748 | `	if( nArg < 1 ){` |
|     ! 0 | 1749 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1750 | `			"Fiber::__construct() expects a callable argument");` |
|       - | 1751 | `	}` |
|     365 | 1752 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1753 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1754 | `			"Fiber::__construct(): invalid $this");` |
|       - | 1755 | `	}` |
|     365 | 1756 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|     365 | 1757 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|     ! 0 | 1758 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1759 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|       - | 1760 | `	}` |
|       - | 1761 | ``	/* php validates `callable $callback` HERE, with the ordinary callback-argument`` |
|       - | 1762 | ``	 * screen and its whole reason taxonomy -- `new Fiber('nosuch')` is a TypeError at`` |
|       - | 1763 | `	 * CONSTRUCTION, naming the function it could not find. PHL had a hand-rolled shape` |
|       - | 1764 | `	 * check that only asked "string or object", with a FiberError of its own wording,` |
|       - | 1765 | `	 * and left an unresolvable NAME to fail at start() instead: the fiber constructed` |
|       - | 1766 | `	 * fine and the program learned about its typo one call later. */` |
|       - | 1767 | `	{` |
|     365 | 1768 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx, apArg[0], 1, "callback", FALSE);` |
|     365 | 1769 | `		if( rcCb != PH7_OK ){` |
|      13 | 1770 | `			return rcCb;` |
|       - | 1771 | `		}` |
|       - | 1772 | `	}` |
|       - | 1773 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|     353 | 1774 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     353 | 1775 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     353 | 1776 | `	if( pAttr ){` |
|     353 | 1777 | `		PH7_MemObjStore(apArg[0], pAttr);` |
|     174 | 1778 | `	}` |
|     353 | 1779 | `	return PH7_OK;` |
|     185 | 1780 | `}` |
|       - | 1781 | `/*` |
|       - | 1782 | ` * Resolve a fiber's stored callable to the BODY it runs and the receiver that body` |
|       - | 1783 | `` * needs -- for every shape php's `callable` covers, not just the two PHL used to take.`` |
|       - | 1784 | ` *` |
|       - | 1785 | ` * The constructor screens the argument with php's own callback rules now` |
|       - | 1786 | ` * (PH7_CheckCallbackArg), so what arrives here is a callable; this decides which body` |
|       - | 1787 | `` * it names. `[$obj,'m']`, `['Class','stat']` and `"Class::stat"` are the everyday way`` |
|       - | 1788 | ` * to run an object's method as a coroutine, and all three were refused outright --` |
|       - | 1789 | ` * the first two by the constructor's "string or closure" shape check, the third by a` |
|       - | 1790 | ` * plain-function lookup that could never find a method.` |
|       - | 1791 | ` *` |
|       - | 1792 | ` * Answers 0 for a callable this engine has no BYTECODE body for: a host builtin` |
|       - | 1793 | `` * (`new Fiber('strtoupper')`) and a name php routes through __call/__callStatic --`` |
|       - | 1794 | ` * which is where the visibility rule lives, and why this asks for it. php does not` |
|       - | 1795 | ` * reach a private method through a callable, it reaches __call INSTEAD, so running` |
|       - | 1796 | ` * the private body would be a hole rather than a shortcut. Both shapes run on php,` |
|       - | 1797 | ` * whose fiber switches a real stack; here they are a loud refusal (§10 divergence,` |
|       - | 1798 | ` * twin-paired). *pzWhy names the reason for the caller to report.` |
|       - | 1799 | ` */` |
|     336 | 1800 | `static ph7_vm_func * VmFiberCallableBody(ph7_vm *pVm, ph7_value *pCallable,` |
|       - | 1801 | `	ph7_class_instance **ppThis, const char **pzWhy)` |
|       5 | 1802 | `{` |
|     341 | 1803 | `	ph7_class_method *pMethod = 0;` |
|     341 | 1804 | `	ph7_class *pClass = 0;` |
|     341 | 1805 | `	*ppThis = 0;` |
|     341 | 1806 | `	*pzWhy = 0;` |
|     341 | 1807 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1808 | ``		/* php's `[target, method]` pair, decoded by the one shared reader so a fiber`` |
|       - | 1809 | `		 * agrees with is_callable() and with every dispatch site about what it is. */` |
|      16 | 1810 | `		ph7_value *pTarget = 0, *pName = 0;` |
|      14 | 1811 | `		if( !PH7_VmArrayCallableParts(pVm, (ph7_hashmap *)pCallable->x.pOther, &pTarget, &pName)` |
|      16 | 1812 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1813 | `			*pzWhy = "callable is not a valid [target, method] pair";` |
|     ! 0 | 1814 | `			return 0;` |
|       - | 1815 | `		}` |
|      16 | 1816 | `		pClass = PH7_VmExtractClassFromValue(pVm, pTarget);` |
|      16 | 1817 | `		if( pClass ){` |
|      23 | 1818 | `			pMethod = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pName->sBlob),` |
|      14 | 1819 | `				SyBlobLength(&pName->sBlob));` |
|       7 | 1820 | `		}` |
|      16 | 1821 | `		if( pMethod == 0 \|\| !PH7_VmCallableMethodAccessible(pVm, pClass, pMethod) ){` |
|       5 | 1822 | `			*pzWhy = "callable routes through __call(), which cannot be a fiber body here";` |
|       5 | 1823 | `			return 0;` |
|       - | 1824 | `		}` |
|      11 | 1825 | `		if( (pTarget->iFlags & MEMOBJ_OBJ) && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|       5 | 1826 | `			*ppThis = (ph7_class_instance *)pTarget->x.pOther;` |
|       2 | 1827 | `		}` |
|      11 | 1828 | `		return &pMethod->sFunc;` |
|       - | 1829 | `	}` |
|     327 | 1830 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|       - | 1831 | `		const char *zCls, *zMeth;` |
|       - | 1832 | `		sxu32 nCls, nMeth;` |
|       - | 1833 | `		SyString sName;` |
|       - | 1834 | `		SyHashEntry *pEntry;` |
|     327 | 1835 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       - | 1836 | ``		/* php's `"Class::method"` static-callable string is the same callee as the pair. */`` |
|     327 | 1837 | `		if( PH7_VmCallableStringParts(sName.zString, sName.nByte, &zCls, &nCls, &zMeth, &nMeth) ){` |
|       8 | 1838 | `			pClass = PH7_VmResolveScopeName(pVm, zCls, nCls);` |
|       8 | 1839 | `			pMethod = pClass ? PH7_ClassExtractMethod(pClass, zMeth, nMeth) : 0;` |
|       8 | 1840 | `			if( pMethod == 0 \|\| !PH7_VmCallableMethodAccessible(pVm, pClass, pMethod) ){` |
|       5 | 1841 | `				*pzWhy = "callable routes through __callStatic(), which cannot be a fiber body here";` |
|       5 | 1842 | `				return 0;` |
|       - | 1843 | `			}` |
|       3 | 1844 | `			return &pMethod->sFunc;` |
|       - | 1845 | `		}` |
|     479 | 1846 | `		pEntry = PH7_VmGetUserFunction(pVm, sName.zString, sName.nByte,` |
|     316 | 1847 | `			(pCallable->iFlags & MEMOBJ_AUX_ENGINEFN) != 0);` |
|     321 | 1848 | `		if( pEntry == 0 ){` |
|       3 | 1849 | `			*pzWhy = SyHashGet(&pVm->hHostFunction, sName.zString, sName.nByte)` |
|       - | 1850 | `				? "callable is an internal function, which cannot be a fiber body here"` |
|       1 | 1851 | `				: "callable names no such function";` |
|       3 | 1852 | `			return 0;` |
|       - | 1853 | `		}` |
|     319 | 1854 | `		return (ph7_vm_func *)pEntry->pUserData;` |
|       - | 1855 | `	}` |
|     ! 0 | 1856 | `	*pzWhy = "callable is not a string, array or object";` |
|     ! 0 | 1857 | `	return 0;` |
|     173 | 1858 | `}` |
|       - | 1859 | `/*` |
|       - | 1860 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|       - | 1861 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|       - | 1862 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|       - | 1863 | ` * so that start() can bind it as $this for the closure environment.` |
|       - | 1864 | ` */` |
|     338 | 1865 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|       - | 1866 | `	ph7_class_instance **ppThis)` |
|       5 | 1867 | `{` |
|     343 | 1868 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1869 | `	ph7_value *pCallable;` |
|       - | 1870 | `	SyString sAttrName;` |
|     343 | 1871 | `	*ppThis = 0;` |
|     343 | 1872 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     343 | 1873 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|     343 | 1874 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP)) == 0 ){` |
|     ! 0 | 1875 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|     ! 0 | 1876 | `		return 0;` |
|       - | 1877 | `	}` |
|     343 | 1878 | `	if( pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP) ){` |
|     259 | 1879 | `		const char *zWhy = 0;` |
|     259 | 1880 | `		ph7_vm_func *pFunc = VmFiberCallableBody(pVm, pCallable, ppThis, &zWhy);` |
|     259 | 1881 | `		if( pFunc == 0 ){` |
|      11 | 1882 | `			PH7_VmThrowException(pCtx, "FiberError", "Fiber %s", zWhy);` |
|       5 | 1883 | `		}` |
|     259 | 1884 | `		return pFunc;` |
|     ! 0 | 1885 | `	}else{` |
|      89 | 1886 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|       - | 1887 | `		ph7_class_method *pMethod;` |
|      89 | 1888 | `		if( VmValueIsClosure(pVm, pCallable) ){` |
|       - | 1889 | `			/* A real Closure object: unwrap to its underlying callable name (the single` |
|       - | 1890 | `			 * source of truth, VmClosureUnwrap) and resolve that function. Its captured` |
|       - | 1891 | ``			 * environment (including any `$this`) rides along in the named function's`` |
|       - | 1892 | `			 * aClosureEnv, installed by VmFiberSetupFrame, so *ppThis stays 0. */` |
|       - | 1893 | `			ph7_value sName;` |
|      87 | 1894 | `			ph7_vm_func *pUnwrapped = 0;` |
|      87 | 1895 | `			const char *zWhyClo = 0;` |
|      87 | 1896 | `			PH7_MemObjInit(pVm, &sName);` |
|      87 | 1897 | `			if( VmClosureUnwrap(pVm, pCallable, &sName) == SXRET_OK ){` |
|       - | 1898 | ``				/* The engine's own `[closure_N]` key, which only this mark gets past the`` |
|       - | 1899 | `				 * script-facing name screen (PH7_VmGetUserFunction). */` |
|      87 | 1900 | `				sName.iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|       - | 1901 | `` 				/* The unwrap answers a NAME for a plain closure and a `[target, method]` `` |
|       - | 1902 | `				 * pair for a first-class callable taken from a method -- so it goes through` |
|       - | 1903 | `				 * the same body-finder as a callable the program wrote. Without it a` |
|       - | 1904 | ``				 * `$o->stat(...)` fiber could not be resolved at all. */`` |
|      87 | 1905 | `				pUnwrapped = VmFiberCallableBody(pVm, &sName, ppThis, &zWhyClo);` |
|      41 | 1906 | `			}` |
|      87 | 1907 | `			PH7_MemObjRelease(&sName);` |
|      87 | 1908 | `			if( pUnwrapped ){` |
|       - | 1909 | `				/* A BOUND closure parked its $this in the pClosureThis transient` |
|       - | 1910 | `				 * (VmClosureUnwrap): consume it as the fiber's $this — it wins over` |
|       - | 1911 | `				 * the creation-time env capture (VmFiberSetupFrame's skip). *ppThis` |
|       - | 1912 | `				 * is a borrow (the closure's $__this attr keeps the object alive` |
|       - | 1913 | `				 * through $__callable), so drop the parked ref. The scope transient` |
|       - | 1914 | `				 * is cleared alongside: fiber bodies don't model pBoundScope` |
|       - | 1915 | `				 * visibility (recorded residual), and a stale transient would` |
|       - | 1916 | `				 * poison the next OP_CALL's frame. */` |
|      87 | 1917 | `				if( pVm->pClosureThis ){` |
|     ! 0 | 1918 | `					*ppThis = pVm->pClosureThis;` |
|     ! 0 | 1919 | `					PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1920 | `					pVm->pClosureThis = 0;` |
|     ! 0 | 1921 | `				}` |
|      87 | 1922 | `				pVm->pClosureScope = 0;` |
|      87 | 1923 | `				pVm->bClosureScreened = 0;` |
|      87 | 1924 | `				return pUnwrapped;` |
|       - | 1925 | `			}` |
|     ! 0 | 1926 | `			if( pVm->pClosureThis ){` |
|       - | 1927 | `				/* Failed resolution: drop the parked transient so it neither leaks` |
|       - | 1928 | `				 * nor poisons the next call. */` |
|     ! 0 | 1929 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 1930 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 1931 | `			}` |
|     ! 0 | 1932 | `			pVm->pClosureScope = 0;` |
|     ! 0 | 1933 | `			pVm->bClosureScreened = 0;` |
|     ! 0 | 1934 | `			PH7_VmThrowException(pCtx, "FiberError", zWhyClo` |
|     ! 0 | 1935 | `				? "Fiber %s" : "Fiber callable closure could not be resolved", zWhyClo);` |
|     ! 0 | 1936 | `			return 0;` |
|       - | 1937 | `		}` |
|       - | 1938 | `		/* Object callable — resolve __invoke method */` |
|       3 | 1939 | `		pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 1940 | `			sizeof("__invoke") - 1);` |
|       3 | 1941 | `		if( pMethod == 0 ){` |
|     ! 0 | 1942 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 1943 | `				"Fiber callable object has no __invoke method");` |
|     ! 0 | 1944 | `			return 0;` |
|       - | 1945 | `		}` |
|       3 | 1946 | `		*ppThis = pClosure;` |
|       3 | 1947 | `		return &pMethod->sFunc;` |
|       - | 1948 | `	}` |
|     174 | 1949 | `}` |
|       - | 1950 | `/*` |
|       - | 1951 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|       - | 1952 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|       - | 1953 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|       - | 1954 | ` */` |
|       - | 1955 | `/*` |
|       - | 1956 | ` * Enforce one formal parameter's declared type on an argument being installed.` |
|       - | 1957 | ` * THE single implementation of the per-argument check, shared by the` |
|       - | 1958 | ` * generator/fiber initial-frame binder below (band A #2) and both OP_CALL` |
|       - | 1959 | ` * install paths (named-map and positional — they carried two verbatim copies` |
|       - | 1960 | ` * until the §7.1(f) fold): union types via VmCoerceToUnion, class and` |
|       - | 1961 | ` * pseudo types (VmCheckPseudoType + VmResolveTypeClass + instanceof, so` |
|       - | 1962 | ``  * interfaces/abstract classes and self/parent resolve), the bare `object` `` |
|       - | 1963 | ` * hint, and scalars via VmEnforceScalarType (weak-mode coercion in place,` |
|       - | 1964 | ` * strict rejection otherwise) — with the VM_FUNC_ARG_NULLABLE guard letting` |
|       - | 1965 | `` * null through for `?type` and implicit-nullable `Type $x = null` params,`` |
|       - | 1966 | ` * and whole-real materialization on a mask match.` |
|       - | 1967 | ` * Returns SXRET_OK (value possibly coerced) or the VmThrowTypeErrorForArg` |
|       - | 1968 | ` * status for the caller to route — normalized so an INLINE-caught throw` |
|       - | 1969 | ` * (VmThrowInline records only a pc-redirect and reports SXRET_OK) still` |
|       - | 1970 | ` * comes back as PH7_EXCEPTION: the binding must stop; the OP_CALL sites` |
|       - | 1971 | ` * force rc = PH7_EXCEPTION on any non-OK status anyway, and the generator` |
|       - | 1972 | ` * block's PH7_INLINE_RESUME_BREAK consumes the redirect.` |
|       - | 1973 | ` */` |
|     374 | 1974 | `static sxi32 VmGenArgThrowStatus(ph7_vm *pVm, sxi32 rcThrow)` |
|       5 | 1975 | `{` |
|     379 | 1976 | `	if( rcThrow == SXRET_OK && pVm->pInlineInstr ){` |
|     ! 0 | 1977 | `		return PH7_EXCEPTION;` |
|       - | 1978 | `	}` |
|     379 | 1979 | `	return rcThrow;` |
|     192 | 1980 | `}` |
|  315905 | 1981 | `PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_vm_func_arg *pFormal,` |
|       - | 1982 | `	sxu32 nArgPos, ph7_value *pVal, int bStrict, ph7_class *pSelfHint)` |
|       5 | 1983 | `{` |
|  315910 | 1984 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|     288 | 1985 | `		if( VmCoerceToUnion(pVm,pVal,&pFormal->aUnionAlts,` |
|     293 | 1986 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,bStrict,pSelfHint) != SXRET_OK ){` |
|       - | 1987 | `			const char *zGiven;` |
|      80 | 1988 | `			const char *zExpected = "union";` |
|       - | 1989 | `			char zBuf[128];` |
|       - | 1990 | `			char zTypeBuf[128];` |
|      80 | 1991 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      49 | 1992 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      57 | 1993 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      10 | 1994 | `				zGiven = "null";` |
|       6 | 1995 | `			}else{` |
|      24 | 1996 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|       - | 1997 | `			}` |
|      80 | 1998 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|     118 | 1999 | `				zExpected = VmHintTextResolved(pVm,&pFormal->sTypeName,pSelfHint,` |
|      38 | 2000 | `					zTypeBuf,sizeof(zTypeBuf));` |
|      38 | 2001 | `			}` |
|     118 | 2002 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      38 | 2003 | `				&pFormal->sName,zExpected,zGiven));` |
|       - | 2004 | `		}` |
|     121 | 2005 | `		return SXRET_OK;` |
|       - | 2006 | `	}` |
|  315713 | 2007 | `	if( pFormal->nType == 0` |
|  177140 | 2008 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|  278184 | 2009 | `		return SXRET_OK;` |
|       - | 2010 | `	}` |
|   37539 | 2011 | `	if( pFormal->nType == SXU32_HIGH ){` |
|       - | 2012 | `		/* Class or pseudo type */` |
|    5417 | 2013 | `		SyString *pName = &pFormal->sClass;` |
|       - | 2014 | `		ph7_class *pClass;` |
|    5417 | 2015 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|    5417 | 2016 | `		if( rcPseudo == 0 ){` |
|       - | 2017 | `			char zTypeBuf[128],zGivenBuf[128];` |
|     140 | 2018 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      34 | 2019 | `				&pFormal->sName,` |
|      68 | 2020 | `				VmClassHintTypeName(pName,0,` |
|      68 | 2021 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      34 | 2022 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 2023 | `		}` |
|    5349 | 2024 | `		pClass = 0;` |
|    5349 | 2025 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|       - | 2026 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      99 | 2027 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      24 | 2028 | `				&pFormal->sName,` |
|      48 | 2029 | `				VmClassHintTypeName(pName,pClass,` |
|      48 | 2030 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      24 | 2031 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 2032 | `		}` |
|    5301 | 2033 | `		return SXRET_OK;` |
|       - | 2034 | `	}` |
|   32127 | 2035 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|       - | 2036 | `		char zGivenBuf[128];` |
|     345 | 2037 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|     105 | 2038 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       8 | 2039 | `				&pFormal->sName,"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 2040 | `		}` |
|     329 | 2041 | `		if( VmEnforceScalarType(pVal,pFormal->nType,bStrict) != SXRET_OK ){` |
|       - | 2042 | `			char zTypeBuf[128];` |
|     242 | 2043 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      79 | 2044 | `				&pFormal->sName,` |
|      79 | 2045 | `				VmScalarTypeName(pFormal->nType,&pFormal->sTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      79 | 2046 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 2047 | `		}` |
|      87 | 2048 | `	}else{` |
|       - | 2049 | `		/* Mask matched — an int param accepting a whole-real materializes` |
|       - | 2050 | `		 * it (php: g(1.0) into int $x is int(1)). */` |
|   31787 | 2051 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|       - | 2052 | `	}` |
|   31953 | 2053 | `	return SXRET_OK;` |
|  158081 | 2054 | `}` |
|       - | 2055 | `/*` |
|       - | 2056 | ` * Record a caller slot this body's frame now ALIASES through a by-reference` |
|       - | 2057 | ` * parameter. The body outlives its caller, so the two frames cannot each own the` |
|       - | 2058 | ` * slot: the caller's teardown counts this frame's name binding as a holder and` |
|       - | 2059 | ` * leaves the value standing, and VmReleaseExecCtx asks PH7_VmReleaseUnheldSlot for` |
|       - | 2060 | ` * every row here once its own names are gone — whichever dies last frees it.` |
|       - | 2061 | ` */` |
|      32 | 2062 | `static void VmCtxAliasByRefArg(ph7_exec_ctx *pExecCtx,sxu32 nIdx)` |
|       2 | 2063 | `{` |
|      34 | 2064 | `	sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pExecCtx->aByRefArg);` |
|       - | 2065 | `	sxu32 n;` |
|      34 | 2066 | `	for( n = 0 ; n < SySetUsed(&pExecCtx->aByRefArg) ; ++n ){` |
|     ! 0 | 2067 | `		if( aIdx[n] == nIdx ){` |
|       - | 2068 | ``			/* Two parameters over one actual (`g($x,$x)`) is ONE slot to give back. */`` |
|     ! 0 | 2069 | `			return;` |
|       - | 2070 | `		}` |
|     ! 0 | 2071 | `	}` |
|      34 | 2072 | `	SySetPut(&pExecCtx->aByRefArg,(const void *)&nIdx);` |
|      18 | 2073 | `}` |
|     972 | 2074 | `PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|       - | 2075 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg,` |
|       - | 2076 | `	int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg, int bAliasByRef)` |
|       5 | 2077 | `{` |
|     977 | 2078 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|       - | 2079 | `	ph7_vm_func_arg *aFormalArg;` |
|       - | 2080 | `	sxu32 nFormal, n;` |
|     977 | 2081 | `	sxu32 nReqGF = 0, nNonVarGF = 0; /* too-few-args watermark (0 = exempt) */` |
|       - | 2082 | `	VmSlot sSlot;` |
|       - | 2083 | `	sxi32 rc;` |
|       - | 2084 | `	/* Install $this for closure/method callables */` |
|     977 | 2085 | `	if( pClosureThis ){` |
|       - | 2086 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      31 | 2087 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      31 | 2088 | `		if( pObj ){` |
|      31 | 2089 | `			pObj->x.pOther = pClosureThis;` |
|      31 | 2090 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      31 | 2091 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      14 | 2092 | `		}` |
|       - | 2093 | `		/* And on the FRAME, which is what everything asking "whose method is this` |
|       - | 2094 | `		 * activation" reads: a coroutine body installed the receiver only as a` |
|       - | 2095 | ``		 * variable, so a `debug_backtrace()` frame for a generator METHOD came back`` |
|       - | 2096 | ``		 * with its class but no `object` — and twig's error reporter, which finds the`` |
|       - | 2097 | `` 		 * template to blame by looking for `$trace['object'] instanceof Template` `` |
|       - | 2098 | ``		 * across the backtrace, found none and could not say `at line N` for any`` |
|       - | 2099 | `		 * template whose failing frame is a compiled generator. Borrowed exactly like` |
|       - | 2100 | `		 * VmEnterFrame's: the variable installed above owns the reference, and it` |
|       - | 2101 | `		 * lives and dies with this frame. */` |
|      31 | 2102 | `		if( pExecCtx->pFrame ){` |
|      31 | 2103 | `			pExecCtx->pFrame->pThis = pClosureThis;` |
|      14 | 2104 | `		}` |
|      14 | 2105 | `	}` |
|       - | 2106 | `	/* Install static variables */` |
|     977 | 2107 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|       - | 2108 | `		ph7_vm_func_static_var *aStatic;` |
|       - | 2109 | `		ph7_value *pVal;` |
|     ! 0 | 2110 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|     ! 0 | 2111 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|     ! 0 | 2112 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|     ! 0 | 2113 | `			if( pVal ){` |
|     ! 0 | 2114 | `				sSlot.pUserData = 0;` |
|     ! 0 | 2115 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|     ! 0 | 2116 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|     ! 0 | 2117 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|     ! 0 | 2118 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|     ! 0 | 2119 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);` |
|     ! 0 | 2120 | `				}` |
|     ! 0 | 2121 | `			}` |
|     ! 0 | 2122 | `		}` |
|     ! 0 | 2123 | `	}` |
|       - | 2124 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|     977 | 2125 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     977 | 2126 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       - | 2127 | `	/* Actual call arity for func_num_args()/func_get_args() (band A #4) */` |
|     977 | 2128 | `	pExecCtx->pFrame->nActualArgs = nArg;` |
|       - | 2129 | `	{` |
|       - | 2130 | `		/* Too-few-arguments watermark — checked per formal INSIDE the install` |
|       - | 2131 | `		 * loop below, after the passed args' type checks, matching php's` |
|       - | 2132 | `		 * RECV order (a type error on a passed argument beats the count` |
|       - | 2133 | `		 * error). Generators throw at the g(...) call site (message embeds` |
|       - | 2134 | `		 * it); fibers at Fiber::start() (php omits the call-site segment).` |
|       - | 2135 | `		 * Prelude builtins are included — VmThrowBuiltinTooFewArgs words them` |
|       - | 2136 | `		 * as php words an internal callable. */` |
|     977 | 2137 | `	nReqGF = VmFuncRequiredArgCount(pFunc,&nNonVarGF);` |
|       - | 2138 | `	}` |
|    1127 | 2139 | `	for( n = 0; n < nFormal; n++ ){` |
|       - | 2140 | `		ph7_value *pObj;` |
|     184 | 2141 | `		if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       - | 2142 | `			/* Variadic formal: collect this and every remaining actual into a` |
|       - | 2143 | `			 * fresh array (php semantics — pre-fix nothing collected here, so a` |
|       - | 2144 | ``			 * `function g(int ...$xs)` generator saw a bare scalar in $xs).`` |
|       - | 2145 | `			 * Per-element checks mirror OP_CALL's variadic install: union via` |
|       - | 2146 | ``			 * the shared helper, scalar coerce/TypeError, `object` hint; a`` |
|       - | 2147 | `			 * class-typed variadic element is (like OP_CALL) not checked. */` |
|       7 | 2148 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       7 | 2149 | `			if( pObj ){` |
|       - | 2150 | `				sxu32 nVariadicIdx;` |
|       - | 2151 | `				ph7_hashmap *pMap;` |
|       - | 2152 | `				sxu32 k;` |
|       7 | 2153 | `				PH7_MemObjToHashmap(pObj);` |
|       - | 2154 | `				/* Capture the slot index now: PH7_HashmapInsert used to reallocate` |
|       - | 2155 | `				 * pVm->aMemObj and dangle pObj (same hazard as OP_CALL's path).` |
|       - | 2156 | `				 * Redundant since P1; left for the harvest sweep (PERF.md P1). */` |
|       7 | 2157 | `				nVariadicIdx = pObj->nIdx;` |
|       7 | 2158 | `				pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      15 | 2159 | `				for( k = n; k < (sxu32)nArg; k++ ){` |
|      11 | 2160 | `					if( ((aFormalArg[n].iFlags & VM_FUNC_ARG_UNION)` |
|       9 | 2161 | `					   \|\| (aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH)) ){` |
|       7 | 2162 | `						rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[k],bStrict,pSelfHint);` |
|       7 | 2163 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 2164 | `							return rc;` |
|       - | 2165 | `						}` |
|       3 | 2166 | `					}` |
|       8 | 2167 | `					if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)` |
|       6 | 2168 | `					 && apArg[k]->nIdx != SXU32_HIGH ){` |
|       - | 2169 | `						/* A by-ref variadic tail aliases its actuals here too — the` |
|       - | 2170 | `						 * ordinary call's rule, one container over. */` |
|       3 | 2171 | `						VmCtxAliasByRefArg(pExecCtx,apArg[k]->nIdx);` |
|       3 | 2172 | `						PH7_HashmapInsertByRef(pMap,0,apArg[k]->nIdx);` |
|       2 | 2173 | `					}else{` |
|       7 | 2174 | `						PH7_HashmapInsert(pMap,0,apArg[k]);` |
|       - | 2175 | `					}` |
|       5 | 2176 | `				}` |
|       7 | 2177 | `				sSlot.nIdx = nVariadicIdx;` |
|       7 | 2178 | `				sSlot.pUserData = 0;` |
|       7 | 2179 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       3 | 2180 | `			}` |
|       7 | 2181 | `			break; /* All remaining actuals consumed */` |
|       - | 2182 | `		}` |
|     178 | 2183 | `		if( n < (sxu32)nArg ){` |
|       - | 2184 | `			/* Argument provided — install with declared-type enforcement.` |
|       - | 2185 | `			 * php binds and type-checks generator arguments EAGERLY at the` |
|       - | 2186 | `			 * g(...) call site (and fiber arguments at Fiber::start()), so the` |
|       - | 2187 | `			 * enforcement lives here, mirroring the OP_CALL install path via` |
|       - | 2188 | `			 * VmEnforceArgType (TypeError on mismatch, weak coercion in` |
|       - | 2189 | `			 * place otherwise) instead of the old silent xCast. A variadic` |
|       - | 2190 | `			 * formal collects as-is (no per-element declared-type model). */` |
|     156 | 2191 | `			if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)` |
|      90 | 2192 | `			 && apArg[n]->nIdx != SXU32_HIGH ){` |
|       - | 2193 | `				/* php binds a generator's by-REFERENCE parameter to the CALLER's slot at` |
|       - | 2194 | `				 * the g(...) that builds the Generator, so the body's write reaches the` |
|       - | 2195 | `				 * caller's variable whenever it eventually runs. Copying it left the` |
|       - | 2196 | `				 * actual untouched for every resume. The type check runs on the actual,` |
|       - | 2197 | `				 * as OP_CALL's by-ref binder does, and never on a copy the alias` |
|       - | 2198 | `				 * replaces. Fiber::start() and the embedder entry pass by VALUE (php's` |
|       - | 2199 | `				 * own decision at those two boundaries), hence bAliasByRef. */` |
|      32 | 2200 | `				sxi32 iPreFlags = apArg[n]->iFlags;` |
|      32 | 2201 | `				rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[n],bStrict,pSelfHint);` |
|      32 | 2202 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 2203 | `					return rc;` |
|       - | 2204 | `				}` |
|       - | 2205 | `				/* A declared type's conversion is what the reference holds (the ordinary` |
|       - | 2206 | `				 * call's rule; the check ran on the operand-stack copy). */` |
|      32 | 2207 | `				PH7_VmByRefArgWriteBack(pVm,apArg[n],iPreFlags);` |
|      47 | 2208 | `				PH7_VmBindVarSlot(pVm,pExecCtx->pFrame,` |
|      30 | 2209 | `					SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName),` |
|      30 | 2210 | `					apArg[n]->nIdx);` |
|      32 | 2211 | `				VmCtxAliasByRefArg(pExecCtx,apArg[n]->nIdx);` |
|      32 | 2212 | `				sSlot.nIdx = apArg[n]->nIdx;` |
|      32 | 2213 | `				sSlot.pUserData = 0;` |
|      32 | 2214 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      32 | 2215 | `				continue;` |
|       - | 2216 | `			}` |
|     130 | 2217 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|     130 | 2218 | `			if( pObj ){` |
|     130 | 2219 | `				PH7_MemObjStore(apArg[n], pObj);` |
|     130 | 2220 | `				if( (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|     130 | 2221 | `					rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,pObj,bStrict,pSelfHint);` |
|     130 | 2222 | `					if( rc != SXRET_OK ){` |
|      18 | 2223 | `						return rc;` |
|       - | 2224 | `					}` |
|      55 | 2225 | `				}` |
|     114 | 2226 | `				sSlot.nIdx = pObj->nIdx;` |
|     114 | 2227 | `				sSlot.pUserData = 0;` |
|     114 | 2228 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      59 | 2229 | `			}` |
|      75 | 2230 | `		}else if( n < nReqGF ){` |
|       - | 2231 | `			/* Required formal with no actual: php's ArgumentCountError, at` |
|       - | 2232 | `			 * this point in the install order (see the watermark comment). */` |
|      13 | 2233 | `			return VmGenArgThrowStatus(pVm,` |
|       8 | 2234 | `				(pFunc->iFlags & VM_FUNC_INTERNAL)` |
|     ! 0 | 2235 | `					? VmThrowBuiltinTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|     ! 0 | 2236 | `						(sxu32)nArg,nReqGF,SySetUsed(&pFunc->aArgs))` |
|      12 | 2237 | `					: VmThrowTooFewArgs(pVm,pSelfHint,&pFunc->sName,pFunc,` |
|       4 | 2238 | `						(sxu32)nArg,nReqGF,nNonVarGF,bCallSiteInMsg));` |
|      12 | 2239 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       - | 2240 | `			/* Default value */` |
|      12 | 2241 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      12 | 2242 | `			if( pObj ){` |
|      12 | 2243 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);` |
|      12 | 2244 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2245 | `					return rc;` |
|       - | 2246 | `				}` |
|       - | 2247 | `` 				/* A null default on an implicitly-nullable `Type $x = null` `` |
|       - | 2248 | `				 * param must stay null (php); only non-null defaults keep the` |
|       - | 2249 | `				 * legacy shaping cast. */` |
|      10 | 2250 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       6 | 2251 | `				 && !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|       3 | 2252 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|     ! 0 | 2253 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|     ! 0 | 2254 | `						if( xCast ){` |
|     ! 0 | 2255 | `							xCast(pObj);` |
|     ! 0 | 2256 | `						}` |
|     ! 0 | 2257 | `					}else{` |
|       - | 2258 | `						/* Mask matched — a const-indirected whole-real default` |
|       - | 2259 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|       3 | 2260 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|       - | 2261 | `					}` |
|       1 | 2262 | `				}` |
|      12 | 2263 | `				sSlot.nIdx = pObj->nIdx;` |
|      12 | 2264 | `				sSlot.pUserData = 0;` |
|      12 | 2265 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       5 | 2266 | `			}` |
|       5 | 2267 | `		}` |
|      64 | 2268 | `	}` |
|       - | 2269 | `	/* Install closure environment (captured variables) */` |
|     953 | 2270 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 2271 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|       - | 2272 | `		ph7_value *pValue;` |
|       - | 2273 | `		sxu32 iEnv;` |
|     243 | 2274 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     593 | 2275 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|     355 | 2276 | `			pEnv = &aEnv[iEnv];` |
|     355 | 2277 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|     227 | 2278 | `				continue;` |
|       - | 2279 | `			}` |
|     128 | 2280 | `			if( pClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       7 | 2281 | `			 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|       - | 2282 | `				/* An explicit bound $this (bindTo/bind/call) wins over the` |
|       - | 2283 | `				 * creation-time captured $this, php-exact (mirrors OP_CALL). */` |
|       3 | 2284 | `				continue;` |
|       - | 2285 | `			}` |
|     131 | 2286 | `			if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|       - | 2287 | `				/* Captured by reference: link the name to the shared slot` |
|       - | 2288 | `				 * (no copy), mirroring the OP_CALL env install. */` |
|       8 | 2289 | `				if( SyHashGet(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|      11 | 2290 | `					SyHashInsert(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),` |
|       6 | 2291 | `						SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|       3 | 2292 | `				}` |
|       8 | 2293 | `				continue;` |
|       - | 2294 | `			}` |
|     125 | 2295 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|     125 | 2296 | `			if( pValue == 0 ){` |
|     ! 0 | 2297 | `				continue;` |
|       - | 2298 | `			}` |
|     125 | 2299 | `			PH7_MemObjRelease(pValue);` |
|     125 | 2300 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|      65 | 2301 | `		}` |
|     119 | 2302 | `	}` |
|     953 | 2303 | `	return SXRET_OK;` |
|     491 | 2304 | `}` |
|       - | 2305 | `/*` |
|       - | 2306 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|       - | 2307 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|       - | 2308 | ` *` |
|       - | 2309 | ` * As a native VARIADIC method the arguments arrive directly as (nArg, apArg);` |
|       - | 2310 | ` * the prelude used to hand them over as a single func_get_args() array, which` |
|       - | 2311 | ` * this had to walk and snapshot out of pVm->aMemObj.` |
|       - | 2312 | ` */` |
|     340 | 2313 | `PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2314 | `{` |
|     345 | 2315 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2316 | `	ph7_class_instance *pThis;` |
|       - | 2317 | `	ph7_class_instance *pClosureThis;` |
|       - | 2318 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 2319 | `	ph7_vm_func *pFunc;` |
|       - | 2320 | `	ph7_value sResult;` |
|       - | 2321 | `	ph7_value *pCtxAttr;` |
|       - | 2322 | `	SyString sAttrName;` |
|       - | 2323 | `	sxi32 rc;` |
|     345 | 2324 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     345 | 2325 | `	if( pRecv == 0 \|\| (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2326 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|       - | 2327 | `	}` |
|     345 | 2328 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 2329 | `	/* Check if already started (has a __ctx) */` |
|     345 | 2330 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     345 | 2331 | `	if( pExecCtx != 0 ){` |
|       3 | 2332 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2333 | `			"Cannot start a fiber that has already been started");` |
|       - | 2334 | `	}` |
|       - | 2335 | `	/* Resolve callable */` |
|     343 | 2336 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|     343 | 2337 | `	if( pFunc == 0 ){` |
|      11 | 2338 | `		return PH7_EXCEPTION;` |
|       - | 2339 | `	}` |
|       - | 2340 | ``	/* Fiber::start()'s own `...$args` are by VALUE whatever the body declares, so php`` |
|       - | 2341 | `		 * warns for every by-reference parameter and the body operates on a copy — the` |
|       - | 2342 | `		 * value PHL already produced, without the one diagnostic that says so. Named off` |
|       - | 2343 | `		 * the stored callable, which is what carries the class for a method one. */` |
|     333 | 2344 | `	if( nArg > 0 ){` |
|       - | 2345 | `		SyString sCbName;` |
|       - | 2346 | `		ph7_value *pCbVal;` |
|      17 | 2347 | `		SyStringInitFromBuf(&sCbName, "__callable", 10);` |
|      17 | 2348 | `		pCbVal = PH7_ClassInstanceFetchAttr(pThis, &sCbName);` |
|      17 | 2349 | `		if( pCbVal ){` |
|      17 | 2350 | `			PH7_VmWarnByRefArgsGivenValue(pVm, pCbVal, nArg, 0, 0);` |
|       7 | 2351 | `		}` |
|       7 | 2352 | `	}` |
|       - | 2353 | `	/* Create execution context now that we know the function */` |
|     333 | 2354 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|     333 | 2355 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 2356 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2357 | `			"Fiber::start(): out of memory");` |
|       - | 2358 | `	}` |
|       - | 2359 | `	/* Store context in $this->__ctx */` |
|     333 | 2360 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     333 | 2361 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     333 | 2362 | `	if( pCtxAttr ){` |
|     333 | 2363 | `		pCtxAttr->x.pOther = pExecCtx;` |
|     333 | 2364 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     164 | 2365 | `	}` |
|       - | 2366 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|       - | 2367 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|       - | 2368 | `	 * into the fiber's frame, not the caller's. */` |
|     333 | 2369 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|     333 | 2370 | `	pVm->pFrame = pExecCtx->pFrame;` |
|       - | 2371 | `	/* Unpack the args array and install into the frame */` |
|       - | 2372 | `	{` |
|       - | 2373 | `		/* The arguments are this call's own operand-stack slots, so they can be` |
|       - | 2374 | `		 * handed to the frame setup as-is. The old form had to snapshot them out of` |
|       - | 2375 | `		 * pVm->aMemObj first, because they arrived as a func_get_args() hashmap` |
|       - | 2376 | `		 * whose element values live in that pool — and VmFiberSetupFrame reserves` |
|       - | 2377 | `		 * memory objects (VmExtractMemObj) before reading its arguments, which back` |
|       - | 2378 | `		 * then reallocated the pool and dangled a raw pointer into it. Operand slots` |
|       - | 2379 | `		 * do not move, so the copy went with the array that made it necessary. (Pool` |
|       - | 2380 | `		 * slots do not move either since P1, which retires the hazard entirely.) */` |
|     333 | 2381 | `		ph7_value **apValues = (nArg > 0) ? apArg : 0;` |
|     333 | 2382 | `		int nActual = nArg;` |
|     333 | 2383 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues,` |
|       - | 2384 | `			0 /* weak-mode arg binding, like call_user_func */, 0,` |
|       - | 2385 | `			FALSE/*Fiber::start(): php omits the call-site segment*/,` |
|       - | 2386 | `			FALSE/*php's Fiber::start() passes by VALUE and warns (§7.1)*/);` |
|       - | 2387 | `		/* Nothing to free: apValues aliases the operand stack now, it is not a` |
|       - | 2388 | `		 * buffer this function allocated. */` |
|       - | 2389 | `	}` |
|       - | 2390 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|     333 | 2391 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|     333 | 2392 | `	pExecCtx->pFrame->pParent = 0;` |
|     333 | 2393 | `	if( rc != SXRET_OK ){` |
|       - | 2394 | `		/* Propagate the real status: a declared-type TypeError from the arg` |
|       - | 2395 | `		 * install (band A #2) must stay catchable, not become an abort. */` |
|       7 | 2396 | `		return (rc == PH7_EXCEPTION) ? PH7_EXCEPTION : PH7_ABORT;` |
|       - | 2397 | `	}` |
|     327 | 2398 | `	PH7_MemObjInit(pVm, &sResult);` |
|       - | 2399 | `	{` |
|       - | 2400 | `		/* php's EG(active_fiber): the fiber the running code is INSIDE, which is what` |
|       - | 2401 | `		 * Fiber::getCurrent() answers. Saved and restored around the body run, so a` |
|       - | 2402 | `		 * fiber that starts another one nests, and a fiber that finishes hands the` |
|       - | 2403 | `		 * name back to whoever was current before it. Borrowed for the duration: the` |
|       - | 2404 | `		 * receiver of this call owns the reference. */` |
|     327 | 2405 | `		ph7_class_instance *pOldFiber = pVm->pCurFiber;` |
|     327 | 2406 | `		pVm->pCurFiber = pThis;` |
|     327 | 2407 | `		rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|     327 | 2408 | `		pVm->pCurFiber = pOldFiber;` |
|       - | 2409 | `	}` |
|     327 | 2410 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 2411 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 2412 | `		return PH7_ABORT;` |
|       - | 2413 | `	}` |
|     327 | 2414 | `	if( rc == PH7_EXCEPTION ){` |
|      12 | 2415 | `		PH7_MemObjRelease(&sResult);` |
|      12 | 2416 | `		return PH7_EXCEPTION;` |
|       - | 2417 | `	}` |
|     317 | 2418 | `	ph7_result_value(pCtx, &sResult);` |
|     317 | 2419 | `	PH7_MemObjRelease(&sResult);` |
|     317 | 2420 | `	return PH7_OK;` |
|     175 | 2421 | `}` |
|       - | 2422 | `/*` |
|       - | 2423 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|       - | 2424 | ` */` |
|     172 | 2425 | `PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2426 | `{` |
|     177 | 2427 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2428 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 2429 | `	ph7_value sResult;` |
|       - | 2430 | `	ph7_value *pResumeVal;` |
|       - | 2431 | `	sxi32 rc;` |
|     177 | 2432 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     177 | 2433 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|     177 | 2434 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2435 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|     ! 0 | 2436 | `		return PH7_OK;` |
|       - | 2437 | `	}` |
|     177 | 2438 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     177 | 2439 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 2440 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|     ! 0 | 2441 | `		return PH7_OK;` |
|       - | 2442 | `	}` |
|     177 | 2443 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       3 | 2444 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2445 | `			"Cannot resume a fiber that is not suspended");` |
|       - | 2446 | `	}` |
|     175 | 2447 | `	pResumeVal = (nArg > 0) ? apArg[0] : 0;` |
|     175 | 2448 | `	PH7_MemObjInit(pVm, &sResult);` |
|       - | 2449 | `	{` |
|       - | 2450 | `		/* See Fiber::start(): the current fiber is this one for the length of the run. */` |
|     175 | 2451 | `		ph7_class_instance *pOldFiber = pVm->pCurFiber;` |
|     175 | 2452 | `		pVm->pCurFiber = (ph7_class_instance *)pRecv->x.pOther;` |
|     175 | 2453 | `		rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|     175 | 2454 | `		pVm->pCurFiber = pOldFiber;` |
|       - | 2455 | `	}` |
|     175 | 2456 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 2457 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 2458 | `		return PH7_ABORT;` |
|       - | 2459 | `	}` |
|     175 | 2460 | `	if( rc == PH7_EXCEPTION ){` |
|       3 | 2461 | `		PH7_MemObjRelease(&sResult);` |
|       3 | 2462 | `		return PH7_EXCEPTION;` |
|       - | 2463 | `	}` |
|     173 | 2464 | `	ph7_result_value(pCtx, &sResult);` |
|     173 | 2465 | `	PH7_MemObjRelease(&sResult);` |
|     173 | 2466 | `	return PH7_OK;` |
|      91 | 2467 | `}` |
|       - | 2468 | `/*` |
|       - | 2469 | ` * Fiber->throw(Throwable $exception) — resume the fiber by RAISING at its` |
|       - | 2470 | `` * suspension point, so `Fiber::suspend()` throws instead of returning. Same`` |
|       - | 2471 | ` * transport as Generator::throw(): the exception is parked on the context and` |
|       - | 2472 | ` * the resumed body raises it in its own frame at the top of the dispatch loop,` |
|       - | 2473 | ` * which is what lets a try/catch INSIDE the fiber catch it and carry on. The` |
|       - | 2474 | ` * answer is the next suspend value, or null if the body ran to completion --` |
|       - | 2475 | ` * symmetric with resume(). Every non-suspended state is php's one sentence.` |
|       - | 2476 | ` */` |
|      10 | 2477 | `PH7_PRIVATE int vm_builtin_Fiber_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 2478 | `{` |
|      11 | 2479 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2480 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 2481 | `	ph7_class_instance *pInj;` |
|       - | 2482 | `	ph7_value sResult;` |
|       - | 2483 | `	sxi32 rc;` |
|      11 | 2484 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      11 | 2485 | `	if( pRecv == 0 \|\| (pRecv->iFlags & MEMOBJ_OBJ) == 0 \|\| nArg < 1 ){` |
|     ! 0 | 2486 | `		return PH7_OK;` |
|       - | 2487 | `	}` |
|      11 | 2488 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2489 | ``		return PH7_OK; /* the declared `Throwable $exception` screen already spoke */`` |
|       - | 2490 | `	}` |
|      11 | 2491 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|      11 | 2492 | `	if( pExecCtx == 0 \|\| pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       - | 2493 | `		/* php answers the same sentence for never-started, running and terminated. */` |
|       5 | 2494 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2495 | `			"Cannot resume a fiber that is not suspended");` |
|       - | 2496 | `	}` |
|       - | 2497 | `	/* Hold a reference for the whole operation: the resumed body may bind the` |
|       - | 2498 | `	 * instance in a catch and release it again before we are back. */` |
|       7 | 2499 | `	pInj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       7 | 2500 | `	pInj->iRef++;` |
|       7 | 2501 | `	pExecCtx->pInjected = pInj;   /* borrowed; consumed at the resume's loop top */` |
|       7 | 2502 | `	PH7_MemObjInit(pVm, &sResult);` |
|       - | 2503 | `	{` |
|       - | 2504 | `		/* See Fiber::start(): the current fiber is this one for the length of the run. */` |
|       7 | 2505 | `		ph7_class_instance *pOldFiber = pVm->pCurFiber;` |
|       7 | 2506 | `		pVm->pCurFiber = (ph7_class_instance *)pRecv->x.pOther;` |
|       7 | 2507 | `		rc = VmResumeCtx(pVm, pExecCtx, 0, &sResult);` |
|       7 | 2508 | `		pVm->pCurFiber = pOldFiber;` |
|       - | 2509 | `	}` |
|       - | 2510 | `	/* Normally consumed (cleared) at the loop top; clear it here too for the path` |
|       - | 2511 | `	 * where VmResumeCtx bails BEFORE entering the loop (the recursion-depth fatal),` |
|       - | 2512 | `	 * so no dangling borrowed pointer survives the Unref. */` |
|       7 | 2513 | `	pExecCtx->pInjected = 0;` |
|       7 | 2514 | `	PH7_ClassInstanceUnref(pInj);` |
|       7 | 2515 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 2516 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 2517 | `		return PH7_ABORT;` |
|       - | 2518 | `	}` |
|       7 | 2519 | `	if( rc == PH7_EXCEPTION ){` |
|       3 | 2520 | `		PH7_MemObjRelease(&sResult);` |
|       3 | 2521 | `		return PH7_EXCEPTION;` |
|       - | 2522 | `	}` |
|       5 | 2523 | `	ph7_result_value(pCtx, &sResult);` |
|       5 | 2524 | `	PH7_MemObjRelease(&sResult);` |
|       5 | 2525 | `	return PH7_OK;` |
|       6 | 2526 | `}` |
|       - | 2527 | `/*` |
|       - | 2528 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|       - | 2529 | ` */` |
|      72 | 2530 | `PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2531 | `{` |
|      77 | 2532 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2533 | `	ph7_exec_ctx *pExecCtx;` |
|      77 | 2534 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      36 | 2535 | `	SXUNUSED(apArg);` |
|      36 | 2536 | `	SXUNUSED(nArg);` |
|      77 | 2537 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      77 | 2538 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2539 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2540 | `		return PH7_OK;` |
|       - | 2541 | `	}` |
|      77 | 2542 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|      77 | 2543 | `	if( pExecCtx == 0 ){` |
|       - | 2544 | `		/* No context at all IS the never-started state -- the fiber's __ctx slot is` |
|       - | 2545 | `		 * filled by start(). php names it rather than answering null. */` |
|       3 | 2546 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2547 | `			"Cannot get fiber return value: The fiber has not been started");` |
|       - | 2548 | `	}` |
|      75 | 2549 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|       5 | 2550 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 2551 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2552 | `				"Cannot get fiber return value: The fiber has not been started");` |
|       - | 2553 | `		}` |
|       5 | 2554 | `		if( pExecCtx->bThrew ){` |
|       - | 2555 | `			/* Terminated, but with nothing to hand back: php's own third sentence. */` |
|       3 | 2556 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2557 | `				"Cannot get fiber return value: The fiber threw an exception");` |
|       - | 2558 | `		}` |
|       3 | 2559 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2560 | `			"Cannot get fiber return value: The fiber has not returned");` |
|       - | 2561 | `	}` |
|      71 | 2562 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|      71 | 2563 | `	return PH7_OK;` |
|      41 | 2564 | `}` |
|       - | 2565 | `/*` |
|       - | 2566 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|       - | 2567 | ` */` |
|      10 | 2568 | `PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 2569 | `{` |
|       - | 2570 | `	ph7_exec_ctx *pExecCtx;` |
|      13 | 2571 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       5 | 2572 | `	SXUNUSED(apArg);` |
|       5 | 2573 | `	SXUNUSED(nArg);` |
|      13 | 2574 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      13 | 2575 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|      13 | 2576 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|      13 | 2577 | `	return PH7_OK;` |
|       8 | 2578 | `}` |
|       2 | 2579 | `PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 2580 | `{` |
|       - | 2581 | `	ph7_exec_ctx *pExecCtx;` |
|       3 | 2582 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       1 | 2583 | `	SXUNUSED(apArg);` |
|       1 | 2584 | `	SXUNUSED(nArg);` |
|       3 | 2585 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       3 | 2586 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|       3 | 2587 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|       3 | 2588 | `	return PH7_OK;` |
|       2 | 2589 | `}` |
|     130 | 2590 | `PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 2591 | `{` |
|       - | 2592 | `	ph7_exec_ctx *pExecCtx;` |
|     134 | 2593 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      65 | 2594 | `	SXUNUSED(apArg);` |
|      65 | 2595 | `	SXUNUSED(nArg);` |
|     134 | 2596 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     134 | 2597 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|     134 | 2598 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|     134 | 2599 | `	return PH7_OK;` |
|      69 | 2600 | `}` |
|      30 | 2601 | `PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 2602 | `{` |
|       - | 2603 | `	ph7_exec_ctx *pExecCtx;` |
|      34 | 2604 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      15 | 2605 | `	SXUNUSED(apArg);` |
|      15 | 2606 | `	SXUNUSED(nArg);` |
|      34 | 2607 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      34 | 2608 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|       - | 2609 | `	/* php's DEAD state: a body that returned and a body that let an exception` |
|       - | 2610 | `	 * escape are both terminated -- only getReturn() tells them apart. */` |
|      51 | 2611 | `	ph7_result_bool(pCtx, pExecCtx && (pExecCtx->iState == PH7_CTX_STATE_COMPLETED` |
|      17 | 2612 | `		\|\| pExecCtx->iState == PH7_CTX_STATE_CLOSED));` |
|      34 | 2613 | `	return PH7_OK;` |
|      19 | 2614 | `}` |
|       - | 2615 | `/*` |
|       - | 2616 | ` * Fiber->__destruct() — clean up the execution context.` |
|       - | 2617 | ` */` |
|     348 | 2618 | `PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2619 | `{` |
|     353 | 2620 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2621 | `	ph7_exec_ctx *pExecCtx;` |
|     353 | 2622 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     174 | 2623 | `	SXUNUSED(apArg);` |
|     174 | 2624 | `	SXUNUSED(nArg);` |
|     353 | 2625 | `	if( pRecv == 0 ){` |
|     ! 0 | 2626 | `		return PH7_OK;` |
|       - | 2627 | `	}` |
|     353 | 2628 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     353 | 2629 | `	if( pExecCtx ){` |
|     333 | 2630 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|       - | 2631 | `		/* Clear the attribute so double-free is prevented */` |
|     333 | 2632 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|     333 | 2633 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 2634 | `			SyString sAttrName;` |
|       - | 2635 | `			ph7_value *pAttr;` |
|     333 | 2636 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     333 | 2637 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     333 | 2638 | `			if( pAttr ){` |
|     333 | 2639 | `				PH7_MemObjRelease(pAttr);` |
|     164 | 2640 | `			}` |
|     164 | 2641 | `		}` |
|     164 | 2642 | `	}` |
|     353 | 2643 | `	return PH7_OK;` |
|     179 | 2644 | `}` |
|       - | 2645 | `/* ======================== Fiber Public API Helpers ======================== */` |
|     ! 0 | 2646 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|     ! 0 | 2647 | `{` |
|       - | 2648 | `	ph7_class_instance *pThis;` |
|     ! 0 | 2649 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|     ! 0 | 2650 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 | 2651 | `	return pThis->pClass == pVm->pFiberClass;` |
|     ! 0 | 2652 | `}` |
|     ! 0 | 2653 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 2654 | `{` |
|       - | 2655 | `	ph7_class_instance *pThis;` |
|     ! 0 | 2656 | `	ph7_class_instance *pClosureThis = 0;` |
|       - | 2657 | `	ph7_exec_ctx *pCtx;` |
|       - | 2658 | `	ph7_vm_func *pFunc;` |
|       - | 2659 | `	ph7_value *pCallable;` |
|       - | 2660 | `	ph7_value *pCtxAttr;` |
|       - | 2661 | `	SyString sAttrName;` |
|       - | 2662 | `	sxi32 rc;` |
|       - | 2663 | `	/* Must not already be started */` |
|     ! 0 | 2664 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2665 | `	if( pCtx != 0 ){` |
|     ! 0 | 2666 | `		return SXERR_INVALID;` |
|       - | 2667 | `	}` |
|     ! 0 | 2668 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2669 | `		return SXERR_INVALID;` |
|       - | 2670 | `	}` |
|     ! 0 | 2671 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|       - | 2672 | `	/* Get the callable */` |
|     ! 0 | 2673 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     ! 0 | 2674 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 2675 | `	if( pCallable == 0 ){` |
|     ! 0 | 2676 | `		return SXERR_INVALID;` |
|       - | 2677 | `	}` |
|       - | 2678 | `	/* Resolve callable, through the same body-finder the PHP-level start() uses --` |
|       - | 2679 | `	 * these were two copies of one decision, and only the other one grew php's array` |
|       - | 2680 | `	 * and "Class::method" shapes. An embedder has no context to throw through, so the` |
|       - | 2681 | `	 * reason comes back as this entry point's own status. */` |
|     ! 0 | 2682 | `	if( pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP) ){` |
|     ! 0 | 2683 | `		const char *zWhy = 0;` |
|     ! 0 | 2684 | `		pFunc = VmFiberCallableBody(pVm, pCallable, &pClosureThis, &zWhy);` |
|     ! 0 | 2685 | `		if( pFunc == 0 ){` |
|     ! 0 | 2686 | `			return SXERR_NOTFOUND;` |
|     ! 0 | 2687 | `		}` |
|     ! 0 | 2688 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2689 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|     ! 0 | 2690 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 2691 | `			sizeof("__invoke") - 1);` |
|     ! 0 | 2692 | `		if( pMethod == 0 ){` |
|     ! 0 | 2693 | `			return SXERR_INVALID;` |
|       - | 2694 | `		}` |
|     ! 0 | 2695 | `		pClosureThis = pClosure;` |
|     ! 0 | 2696 | `		pFunc = &pMethod->sFunc;` |
|     ! 0 | 2697 | `	}else{` |
|     ! 0 | 2698 | `		return SXERR_INVALID;` |
|       - | 2699 | `	}` |
|       - | 2700 | `	/* Create context */` |
|     ! 0 | 2701 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|     ! 0 | 2702 | `	if( pCtx == 0 ){` |
|     ! 0 | 2703 | `		return SXERR_MEM;` |
|       - | 2704 | `	}` |
|       - | 2705 | `	/* Store in __ctx */` |
|     ! 0 | 2706 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     ! 0 | 2707 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 2708 | `	if( pCtxAttr ){` |
|     ! 0 | 2709 | `		pCtxAttr->x.pOther = pCtx;` |
|     ! 0 | 2710 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     ! 0 | 2711 | `	}` |
|       - | 2712 | `	/* Set up frame with args */` |
|     ! 0 | 2713 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     ! 0 | 2714 | `	pVm->pFrame = pCtx->pFrame;` |
|     ! 0 | 2715 | `	rc = VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg,` |
|       - | 2716 | `		0 /* weak-mode arg binding (embedder entry) */, 0,` |
|       - | 2717 | `		FALSE/*embedder entry: no userland call site*/,` |
|       - | 2718 | `		FALSE/*no source-level actuals to alias*/);` |
|     ! 0 | 2719 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|     ! 0 | 2720 | `	pCtx->pFrame->pParent = 0;` |
|     ! 0 | 2721 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2722 | `		return rc;` |
|       - | 2723 | `	}` |
|     ! 0 | 2724 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|     ! 0 | 2725 | `}` |
|     ! 0 | 2726 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 2727 | `{` |
|     ! 0 | 2728 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2729 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|     ! 0 | 2730 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|     ! 0 | 2731 | `}` |
|     ! 0 | 2732 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2733 | `{` |
|     ! 0 | 2734 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2735 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|     ! 0 | 2736 | `}` |
|     ! 0 | 2737 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2738 | `{` |
|     ! 0 | 2739 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2740 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|     ! 0 | 2741 | `}` |
|     ! 0 | 2742 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 2743 | `{` |
|     ! 0 | 2744 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 2745 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|     ! 0 | 2746 | `	return &pCtx->sRetValue;` |
|     ! 0 | 2747 | `}` |
|       - | 2748 | `/* ======================== Generator Infrastructure ======================== */` |
|       - | 2749 | `/*` |
|       - | 2750 | ` * Allocate a new generator wrapper around an execution context.` |
|       - | 2751 | ` */` |
|     644 | 2752 | `PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 | 2753 | `{` |
|       - | 2754 | `	ph7_generator *pGen;` |
|     649 | 2755 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|     649 | 2756 | `	if( pGen == 0 ){` |
|     ! 0 | 2757 | `		return 0;` |
|       - | 2758 | `	}` |
|     649 | 2759 | `	SyZero(pGen, sizeof(ph7_generator));` |
|     649 | 2760 | `	pGen->pCtx = pCtx;` |
|     649 | 2761 | `	pGen->iImplicitKey = 0;` |
|     649 | 2762 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|     649 | 2763 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|       - | 2764 | `	/* Link the generator back to the exec context */` |
|     649 | 2765 | `	pCtx->pPrivate = pGen;` |
|     649 | 2766 | `	return pGen;` |
|     327 | 2767 | `}` |
|       - | 2768 | `/*` |
|       - | 2769 | ` * Release a generator and its execution context.` |
|       - | 2770 | ` */` |
|     642 | 2771 | `PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|       5 | 2772 | `{` |
|     647 | 2773 | `	if( pGen == 0 ){` |
|     ! 0 | 2774 | `		return;` |
|       - | 2775 | `	}` |
|     647 | 2776 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|     647 | 2777 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|     647 | 2778 | `	if( pGen->pCtx ){` |
|     647 | 2779 | `		pGen->pCtx->pPrivate = 0;` |
|     647 | 2780 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|     647 | 2781 | `		pGen->pCtx = 0;` |
|     321 | 2782 | `	}` |
|     647 | 2783 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|     326 | 2784 | `}` |
|       - | 2785 | `/*` |
|       - | 2786 | ` * Extract ph7_generator from a Generator class instance.` |
|       - | 2787 | ` */` |
|    5880 | 2788 | `PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|       5 | 2789 | `{` |
|       - | 2790 | `	ph7_class_instance *pThis;` |
|       - | 2791 | `	SyString sAttr;` |
|       - | 2792 | `	ph7_value *pAttr;` |
|    5885 | 2793 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2794 | `		return 0;` |
|       - | 2795 | `	}` |
|    5885 | 2796 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|    5885 | 2797 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|     ! 0 | 2798 | `		return 0;` |
|       - | 2799 | `	}` |
|    5885 | 2800 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    5885 | 2801 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    5885 | 2802 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     ! 0 | 2803 | `		return 0;` |
|       - | 2804 | `	}` |
|    5885 | 2805 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    2945 | 2806 | `}` |
|       - | 2807 | `/*` |
|       - | 2808 | ` * php's zend_generator_ensure_initialized, which EVERY accessor on the class runs` |
|       - | 2809 | ` * first: a generator that has never executed is run to its first yield before it` |
|       - | 2810 | `` * is asked anything. `current()`/`key()`/`rewind()` did that here and the rest did`` |
|       - | 2811 | `` * not, so `valid()` answered FALSE for a generator sitting at its first yield —`` |
|       - | 2812 | `` * `while ($g->valid())` never entered the loop — and `next()`/`send()` did the`` |
|       - | 2813 | ` * PRIMING run and called it the advance, which delivers the first element twice` |
|       - | 2814 | `` * and drops what `send()` was given.`` |
|       - | 2815 | ` */` |
|    5194 | 2816 | `static sxi32 VmGeneratorEnsureInit(ph7_vm *pVm, ph7_generator *pGen)` |
|       5 | 2817 | `{` |
|       - | 2818 | `	sxi32 rc;` |
|    5199 | 2819 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|    4593 | 2820 | `		return PH7_OK;` |
|       - | 2821 | `	}` |
|     611 | 2822 | `	rc = VmStartCtx(pVm, pGen->pCtx, 0);` |
|     611 | 2823 | `	if( rc == PH7_OK ){` |
|       - | 2824 | `		/* php sets the flag on the INITIALIZING run itself, whatever it settled` |
|       - | 2825 | `		 * on — so a generator whose body never yields is still rewindable, and` |
|       - | 2826 | `		 * only a later resume takes the flag away. */` |
|     585 | 2827 | `		pGen->bAtFirstYield = 1;` |
|     290 | 2828 | `	}` |
|     611 | 2829 | `	return rc;` |
|    2602 | 2830 | `}` |
|       - | 2831 | `/*` |
|       - | 2832 | ` * Fetch the ph7_generator behind a Generator INSTANCE (the ph7_value-taking` |
|       - | 2833 | ` * VmGeneratorExtractCtx is the same lookup from the other side).` |
|       - | 2834 | ` */` |
|    1294 | 2835 | `static ph7_generator * VmGeneratorFromInstance(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       5 | 2836 | `{` |
|       - | 2837 | `	SyString sAttr;` |
|       - | 2838 | `	ph7_value *pAttr;` |
|    1299 | 2839 | `	if( pThis == 0 \|\| pVm->pGeneratorClass == 0 \|\| pThis->pClass != pVm->pGeneratorClass ){` |
|     869 | 2840 | `		return 0;` |
|       - | 2841 | `	}` |
|     435 | 2842 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     435 | 2843 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     435 | 2844 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     ! 0 | 2845 | `		return 0;` |
|       - | 2846 | `	}` |
|     435 | 2847 | `	return (ph7_generator *)pAttr->x.pOther;` |
|     652 | 2848 | `}` |
|       - | 2849 | `/*` |
|       - | 2850 | ``  * Run a generator to its first yield if it has never executed. `yield from` `` |
|       - | 2851 | ` * needs this and NOT a rewind: php links the delegate as a child node and only` |
|       - | 2852 | ` * initializes it, so delegating to a generator that is already suspended` |
|       - | 2853 | ` * half-way CONTINUES from where it stands.` |
|       - | 2854 | ` */` |
|      46 | 2855 | `PH7_PRIVATE sxi32 PH7_VmGeneratorPrime(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       4 | 2856 | `{` |
|      50 | 2857 | `	ph7_generator *pGen = VmGeneratorFromInstance(pVm, pThis);` |
|      50 | 2858 | `	if( pGen == 0 \|\| pGen->pCtx == 0 ){` |
|     ! 0 | 2859 | `		return PH7_OK;` |
|       - | 2860 | `	}` |
|      50 | 2861 | `	return VmGeneratorEnsureInit(pVm, pGen);` |
|      27 | 2862 | `}` |
|       - | 2863 | `/*` |
|       - | 2864 | ` * Whether a generator instance has already run to its end. php's` |
|       - | 2865 | ` * zend_generator_get_iterator refuses to start a foreach over one` |
|       - | 2866 | ` * ("Cannot traverse an already closed generator") BEFORE the rewind that would` |
|       - | 2867 | ` * otherwise report the coarser "already run" message.` |
|       - | 2868 | ` */` |
|    1248 | 2869 | `PH7_PRIVATE int PH7_VmGeneratorIsClosed(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       5 | 2870 | `{` |
|    1253 | 2871 | `	ph7_generator *pGen = VmGeneratorFromInstance(pVm, pThis);` |
|    1253 | 2872 | `	if( pGen == 0 \|\| pGen->pCtx == 0 ){` |
|     869 | 2873 | `		return 0;` |
|       - | 2874 | `	}` |
|     577 | 2875 | `	return pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED` |
|     384 | 2876 | `		\|\| pGen->pCtx->iState == PH7_CTX_STATE_CLOSED;` |
|     629 | 2877 | `}` |
|       - | 2878 | `/*` |
|       - | 2879 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|       - | 2880 | ` */` |
|     360 | 2881 | `PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2882 | `{` |
|       - | 2883 | `	ph7_generator *pGen;` |
|       - | 2884 | `	sxi32 rc;` |
|     365 | 2885 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     180 | 2886 | `	SXUNUSED(apArg);` |
|     180 | 2887 | `	SXUNUSED(nArg);` |
|     365 | 2888 | `	if( pRecv == 0 ) return PH7_OK;` |
|     365 | 2889 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     365 | 2890 | `	if( pGen == 0 ) return PH7_OK;` |
|     365 | 2891 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|     365 | 2892 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     365 | 2893 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     345 | 2894 | `	if( !pGen->bAtFirstYield ){` |
|       - | 2895 | `		/* php: a generator is not rewindable, so rewind() is only allowed to mean` |
|       - | 2896 | `		 * "initialize". Once the body has moved past its first yield — or run to` |
|       - | 2897 | `		 * the end — php refuses, and PHL accepted in silence, so a foreach over a` |
|       - | 2898 | `		 * partly consumed generator carried on from where it stood while php` |
|       - | 2899 | `		 * stopped the program. */` |
|       5 | 2900 | `		return PH7_VmThrowException(pCtx, "Exception",` |
|       - | 2901 | `			"Cannot rewind a generator that was already run");` |
|       - | 2902 | `	}` |
|     341 | 2903 | `	return PH7_OK;` |
|     185 | 2904 | `}` |
|       - | 2905 | `/*` |
|       - | 2906 | ` * Generator::valid() — true if suspended at a yield point.` |
|       - | 2907 | ` */` |
|    1566 | 2908 | `PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2909 | `{` |
|       - | 2910 | `	ph7_generator *pGen;` |
|    1571 | 2911 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     783 | 2912 | `	SXUNUSED(apArg);` |
|     783 | 2913 | `	SXUNUSED(nArg);` |
|    1571 | 2914 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|    1571 | 2915 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1571 | 2916 | `	if( pGen ){` |
|    1571 | 2917 | `		sxi32 rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|    1571 | 2918 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|    1571 | 2919 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     783 | 2920 | `	}` |
|    1571 | 2921 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|    1571 | 2922 | `	return PH7_OK;` |
|     788 | 2923 | `}` |
|       - | 2924 | `/*` |
|       - | 2925 | ` * Generator::current() — return the last yielded value.` |
|       - | 2926 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 2927 | ` */` |
|    1460 | 2928 | `PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2929 | `{` |
|       - | 2930 | `	ph7_generator *pGen;` |
|       - | 2931 | `	sxi32 rc;` |
|    1465 | 2932 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     730 | 2933 | `	SXUNUSED(apArg);` |
|     730 | 2934 | `	SXUNUSED(nArg);` |
|    1465 | 2935 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1465 | 2936 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1465 | 2937 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1465 | 2938 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|    1465 | 2939 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|    1465 | 2940 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|    1465 | 2941 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|    1463 | 2942 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|     734 | 2943 | `	}else{` |
|       3 | 2944 | `		ph7_result_null(pCtx);` |
|       - | 2945 | `	}` |
|    1465 | 2946 | `	return PH7_OK;` |
|     735 | 2947 | `}` |
|       - | 2948 | `/*` |
|       - | 2949 | ` * Generator::key() — return the last yielded key.` |
|       - | 2950 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 2951 | ` */` |
|     400 | 2952 | `PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2953 | `{` |
|       - | 2954 | `	ph7_generator *pGen;` |
|       - | 2955 | `	sxi32 rc;` |
|     405 | 2956 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     200 | 2957 | `	SXUNUSED(apArg);` |
|     200 | 2958 | `	SXUNUSED(nArg);` |
|     405 | 2959 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     405 | 2960 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     405 | 2961 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     405 | 2962 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|     405 | 2963 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     405 | 2964 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     405 | 2965 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     405 | 2966 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|     205 | 2967 | `	}else{` |
|     ! 0 | 2968 | `		ph7_result_null(pCtx);` |
|       - | 2969 | `	}` |
|     405 | 2970 | `	return PH7_OK;` |
|     205 | 2971 | `}` |
|       - | 2972 | `/*` |
|       - | 2973 | ` * Generator::next() — advance to the next yield point.` |
|       - | 2974 | ` */` |
|    1132 | 2975 | `PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2976 | `{` |
|       - | 2977 | `	ph7_generator *pGen;` |
|       - | 2978 | `	sxi32 rc;` |
|    1137 | 2979 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     566 | 2980 | `	SXUNUSED(apArg);` |
|     566 | 2981 | `	SXUNUSED(nArg);` |
|    1137 | 2982 | `	if( pRecv == 0 ) return PH7_OK;` |
|    1137 | 2983 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1137 | 2984 | `	if( pGen == 0 ) return PH7_OK;` |
|       - | 2985 | `	/* PRIMING is not the advance: php runs a never-executed body to its first` |
|       - | 2986 | ``	 * yield and then still resumes past it, so `$g->next()` on a fresh generator`` |
|       - | 2987 | `	 * lands on the SECOND element. Treating the start as the advance handed the` |
|       - | 2988 | `	 * first one out twice. */` |
|    1137 | 2989 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|    1137 | 2990 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|    1137 | 2991 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|    1137 | 2992 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|     ! 0 | 2993 | `		return PH7_OK;` |
|       - | 2994 | `	}` |
|    1137 | 2995 | `	pGen->bAtFirstYield = 0;` |
|    1137 | 2996 | `	rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|    1137 | 2997 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|    1135 | 2998 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|    1125 | 2999 | `	return PH7_OK;` |
|     571 | 3000 | `}` |
|       - | 3001 | `/*` |
|       - | 3002 | ` * Generator::send($value) — resume and send a value into the generator.` |
|       - | 3003 | ` */` |
|     138 | 3004 | `PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3005 | `{` |
|       - | 3006 | `	ph7_generator *pGen;` |
|       - | 3007 | `	ph7_value *pSendVal;` |
|       - | 3008 | `	sxi32 rc;` |
|     143 | 3009 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     143 | 3010 | `	if( pRecv == 0 ) return PH7_OK;` |
|     143 | 3011 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     143 | 3012 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     143 | 3013 | `	pSendVal = (nArg > 0) ? apArg[0] : 0;` |
|       - | 3014 | `	/* php PRIMES a never-executed generator and THEN resumes it with the value, so` |
|       - | 3015 | ``	 * a first `send('S')` reaches the first `yield`'s left-hand side and answers the`` |
|       - | 3016 | `	 * SECOND yielded value. Stopping at the priming run dropped the value entirely` |
|       - | 3017 | `	 * and answered the first — the whole point of a coroutine's first send. */` |
|     143 | 3018 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|     143 | 3019 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     143 | 3020 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     143 | 3021 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       3 | 3022 | `		ph7_result_null(pCtx);` |
|       3 | 3023 | `		return PH7_OK;` |
|       - | 3024 | `	}` |
|     141 | 3025 | `	pGen->bAtFirstYield = 0;` |
|     141 | 3026 | `	rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|     141 | 3027 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     141 | 3028 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     139 | 3029 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     113 | 3030 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      59 | 3031 | `	}else{` |
|      29 | 3032 | `		ph7_result_null(pCtx);` |
|       - | 3033 | `	}` |
|     139 | 3034 | `	return PH7_OK;` |
|      74 | 3035 | `}` |
|       - | 3036 | `/*` |
|       - | 3037 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|       - | 3038 | ` *` |
|       - | 3039 | ` * PHP semantics: the exception is injected AT the suspended yield point so the` |
|       - | 3040 | ` * generator's OWN try/catch (if any wraps the yield) can handle it and the body` |
|       - | 3041 | ` * resumes after the try; otherwise it propagates to the throw() caller and the` |
|       - | 3042 | ` * generator closes. We implement this by resuming the body with a pending` |
|       - | 3043 | ` * injection (pCtx->pInjected) that the VM loop raises in the body's own frame via` |
|       - | 3044 | ` * the existing OP_THROW / ROOT B resume route — no exception frame is` |
|       - | 3045 | ` * reconstructed on pVm->pFrame, so the return/finally unwind path is untouched.` |
|       - | 3046 | ` * A never-started generator is first run to its first yield, then injected there;` |
|       - | 3047 | ` * a finished/closed generator has no suspend point, so the exception is simply` |
|       - | 3048 | ` * propagated to the caller (its return value stays readable via getReturn()).` |
|       - | 3049 | ` *` |
|       - | 3050 | ` * PHL does not enforce the Throwable parameter hint (interface/class hints are` |
|       - | 3051 | ` * not checked on call), so the Throwable validation — and its PHP TypeError —` |
|       - | 3052 | ` * are done here.` |
|       - | 3053 | ` */` |
|      62 | 3054 | `PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 3055 | `{` |
|       - | 3056 | `	ph7_generator *pGen;` |
|       - | 3057 | `	ph7_class_instance *pInj;` |
|       - | 3058 | `	ph7_class *pThrowable;` |
|       - | 3059 | `	VmFrame *pFrame;` |
|       - | 3060 | `	sxi32 rc;` |
|      66 | 3061 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      66 | 3062 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      66 | 3063 | `	if( nArg < 1 ) return PH7_OK;` |
|       - | 3064 | `	/* Argument #1 must be a Throwable; otherwise PHP raises a TypeError naming` |
|       - | 3065 | `	 * the given type (class name for objects, "null"/"string"/... for scalars). */` |
|      66 | 3066 | `	pThrowable = PH7_VmExtractClass(pCtx->pVm, "Throwable", sizeof("Throwable")-1, 0, 0);` |
|      62 | 3067 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|      66 | 3068 | `	 \|\| (pThrowable && !PH7_VmInstanceOf(((ph7_class_instance *)apArg[0]->x.pOther)->pClass, pThrowable)) ){` |
|       - | 3069 | `		char zCls[128];` |
|     ! 0 | 3070 | `		const char *zGiven = VmValueGivenName(apArg[0], zCls, sizeof(zCls));` |
|     ! 0 | 3071 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|     ! 0 | 3072 | `			"Generator::throw(): Argument #1 ($exception) must be of type Throwable, %s given", zGiven);` |
|       - | 3073 | `	}` |
|      66 | 3074 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      66 | 3075 | `	if( pGen == 0 ) return PH7_OK;` |
|       - | 3076 | `	/* PHP forbids resuming/throwing into a generator that is currently executing. */` |
|      66 | 3077 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       3 | 3078 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 3079 | `			"Cannot resume an already running generator");` |
|       - | 3080 | `	}` |
|       - | 3081 | `	/* Hold a reference to the injected instance for the whole operation: the VM loop` |
|       - | 3082 | `	 * (inject path) or VmThrowException (propagate path) may run catch blocks that bind` |
|       - | 3083 | `	 * and later release it. Dropped on every return path below. */` |
|      64 | 3084 | `	pInj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      64 | 3085 | `	pInj->iRef++;` |
|       - | 3086 | `	/* A never-started generator runs to its first yield, then the exception is injected` |
|       - | 3087 | `	 * there (PHP). Start it first; if it suspended at a yield, fall through to inject; if` |
|       - | 3088 | `	 * it ran to completion without yielding, drop through to the propagate path below. */` |
|       - | 3089 | `	{` |
|      64 | 3090 | `		rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|      64 | 3091 | `		if( rc == PH7_ABORT ){ PH7_ClassInstanceUnref(pInj); return PH7_ABORT; }` |
|      64 | 3092 | `		if( rc == PH7_EXCEPTION ){ PH7_ClassInstanceUnref(pInj); return PH7_EXCEPTION; }` |
|       - | 3093 | `	}` |
|      64 | 3094 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       - | 3095 | `		/* Inject at the suspended yield: the resume loop raises it in the body's own` |
|       - | 3096 | `		 * frame so the generator's try/catch can catch it and resume (path 2). */` |
|      58 | 3097 | `		pGen->pCtx->pInjected = pInj;   /* borrowed; ref held here across the resume */` |
|      58 | 3098 | `		pGen->bAtFirstYield = 0;` |
|      58 | 3099 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       - | 3100 | `		/* Normally the inject was consumed (cleared) at VmByteCodeExec entry; clear it` |
|       - | 3101 | `		 * here too for the path where VmResumeCtx bails BEFORE entering the loop (e.g. the` |
|       - | 3102 | `		 * recursion-depth fatal), so no dangling borrowed pointer survives the Unref. */` |
|      58 | 3103 | `		pGen->pCtx->pInjected = 0;` |
|      58 | 3104 | `		PH7_ClassInstanceUnref(pInj);` |
|      58 | 3105 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      58 | 3106 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       - | 3107 | `		/* Caught inside the generator and it resumed: return the next yielded value (or` |
|       - | 3108 | `		 * null if it then completed) — symmetric with Generator::send(). */` |
|      46 | 3109 | `		if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      43 | 3110 | `			ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      23 | 3111 | `		}else{` |
|       3 | 3112 | `			ph7_result_null(pCtx);` |
|       - | 3113 | `		}` |
|      46 | 3114 | `		return PH7_OK;` |
|       - | 3115 | `	}` |
|       - | 3116 | `	/* COMPLETED/CLOSED (incl. a CREATED generator that ran to completion without a` |
|       - | 3117 | `	 * yield): no suspend point to inject at. Propagate the real object to the throw()` |
|       - | 3118 | `	 * caller through the normal dispatch path (class/message/trace preserved); its` |
|       - | 3119 | `	 * terminal state — and thus getReturn() — is left intact. */` |
|       8 | 3120 | `	pFrame = pCtx->pVm->pFrame;` |
|       8 | 3121 | `	if( pFrame ){` |
|       8 | 3122 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       8 | 3123 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       3 | 3124 | `	}` |
|       8 | 3125 | `	rc = VmThrowException(pCtx->pVm, pInj);` |
|       8 | 3126 | `	PH7_ClassInstanceUnref(pInj);` |
|       8 | 3127 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3128 | `		return PH7_ABORT;` |
|       - | 3129 | `	}` |
|       8 | 3130 | `	return PH7_EXCEPTION;` |
|      35 | 3131 | `}` |
|       - | 3132 | `/*` |
|       - | 3133 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|       - | 3134 | ` */` |
|      32 | 3135 | `PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 3136 | `{` |
|       - | 3137 | `	ph7_generator *pGen;` |
|      36 | 3138 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      16 | 3139 | `	SXUNUSED(apArg);` |
|      16 | 3140 | `	SXUNUSED(nArg);` |
|      36 | 3141 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      36 | 3142 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      36 | 3143 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       - | 3144 | `	{` |
|       - | 3145 | `		/* php initializes here too, so a generator asked for its return value` |
|       - | 3146 | `		 * before anything else has RUN its body up to the first yield — the side` |
|       - | 3147 | `		 * effects before that yield happen either way. */` |
|      36 | 3148 | `		sxi32 rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|      36 | 3149 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      36 | 3150 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       - | 3151 | `	}` |
|      36 | 3152 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|       - | 3153 | `		/* php's class here is Exception, not Error. */` |
|       3 | 3154 | `		return PH7_VmThrowException(pCtx, "Exception",` |
|       - | 3155 | `			"Cannot get return value of a generator that hasn't returned");` |
|       - | 3156 | `	}` |
|      34 | 3157 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|      34 | 3158 | `	return PH7_OK;` |
|      20 | 3159 | `}` |
|       - | 3160 | `/*` |
|       - | 3161 | ` * Generator::__destruct() — clean up.` |
|       - | 3162 | ` */` |
|     624 | 3163 | `PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3164 | `{` |
|       - | 3165 | `	ph7_generator *pGen;` |
|     629 | 3166 | `	sxi32 rcClose = SXRET_OK;` |
|     629 | 3167 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     312 | 3168 | `	SXUNUSED(apArg);` |
|     312 | 3169 | `	SXUNUSED(nArg);` |
|     629 | 3170 | `	if( pRecv == 0 ) return PH7_OK;` |
|     629 | 3171 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     629 | 3172 | `	if( pGen ){` |
|       - | 3173 | `` 		/* A generator abandoned before it completes still runs its pending `finally` `` |
|       - | 3174 | `		 * blocks (PHP runs them at generator close/GC). Drive them before teardown. */` |
|     629 | 3175 | `		if( pGen->pCtx ){` |
|     629 | 3176 | `			rcClose = VmCloseCtx(pCtx->pVm, pGen->pCtx);` |
|     312 | 3177 | `		}` |
|     629 | 3178 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|     629 | 3179 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|     629 | 3180 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 3181 | `			SyString sAttrName;` |
|       - | 3182 | `			ph7_value *pAttr;` |
|     629 | 3183 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     629 | 3184 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     629 | 3185 | `			if( pAttr ){` |
|     629 | 3186 | `				PH7_MemObjRelease(pAttr);` |
|     312 | 3187 | `			}` |
|     312 | 3188 | `		}` |
|     312 | 3189 | `	}` |
|       - | 3190 | `	/* Surface an abort/exception raised by a finally that ran during close. */` |
|     629 | 3191 | `	if( rcClose == PH7_ABORT ) return PH7_ABORT;` |
|     629 | 3192 | `	if( rcClose == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     629 | 3193 | `	return PH7_OK;` |
|     317 | 3194 | `}` |
|       - | 3195 | `/* ======================== End Generator Infrastructure ======================== */` |
|       - | 3196 | `/* ======================== End Fiber Infrastructure ======================== */` |
|       - | 3197 |  |
