/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include "compile_int.h"
/*
 * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.
 * That is, routines defined in this file takes a stream of tokens and output
 * PH7 bytecode instructions.
 */
/* Forward declaration */
/*
 * Local utility routines used in the code generation phase.
 */
/*
 * Check if the given name refer to a valid label declared in the given function
 * (NULL = file scope).
 * Return SXRET_OK and write a pointer to that label on success.
 * Any other return value indicates no such label.
 *
 * Labels are scoped PER FUNCTION in php, so the owning function is part of the key:
 * the same name may be declared in as many functions as one likes, and each goto sees
 * only its own. Matching on the name alone made the first declaration win everywhere,
 * which rejected `function a(){ done: } function b(){ goto done; done: }` — ordinary
 * php — as a jump to an undefined label.
 *
 * Also serves PH7_CompileLabel, which asks the same question at DECLARATION time to reject
 * a name its function already declared.
 */
PH7_PRIVATE sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,ph7_vm_func *pFunc,Label **ppOut)
{
	Label *aLabel;
	sxu32 n;
	/* Perform a linear scan on the label table */
	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);
	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){
		if( aLabel[n].pFunc == pFunc && SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){
			/* Jump destination found */
			if( ppOut ){
				*ppOut = &aLabel[n];
			}
			return SXRET_OK;
		}
	}
	/* No such destination */
	return SXERR_NOTFOUND;
}
/*
 * Fetch a block that correspond to the given criteria from the stack of
 * compiled blocks.
 * Return a pointer to that block on success. NULL otherwise.
 */
/*
 * Is the declaration the generator is standing on CONDITIONAL -- php's "not early
 * bound"? A `function` or `class` written at the top level of a unit is bound when
 * the unit compiles, and one written anywhere else -- inside an `if`, a loop, a
 * `try`, another function's body -- is bound when execution REACHES it, and not
 * before.
 *
 * The difference is not academic: `if (!function_exists('mb_convert_encoding')) {
 * function mb_convert_encoding(...) {...} }` is how every symfony/polyfill-* package
 * is written, and binding that body unconditionally REPLACED the engine's own
 * builtin with the polyfill -- in a tree that has one, which is nearly every real
 * project. `if (false) { function f(){} }` declared `f` too.
 */
PH7_PRIVATE int GenStateDeclIsConditional(ph7_gen_state *pGen)
{
	GenBlock *pBlock = pGen->pCurrent;
	return pBlock != 0 && (pBlock->iFlags & GEN_BLOCK_GLOBAL) == 0;
}

PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)
{
	GenBlock *pBlock = pCurrent;
	for(;;){
		if( pBlock->iFlags & iBlockType ){
			iCount--; /* Decrement nesting level */
			if( iCount < 1 ){
				/* Block meet with the desired criteria */
				return pBlock;
			}
		}
		/* Point to the upper block */
		pBlock = pBlock->pParent;
		if( pBlock == 0 || (pBlock->iFlags & (GEN_BLOCK_PROTECTED|GEN_BLOCK_FUNC)) ){
			/* Forbidden */
			break;
		}
	}
	/* No such block */
	return 0;
}
/*
 * Initialize a freshly allocated block instance.
 */
static void GenStateInitBlock(
	ph7_gen_state *pGen, /* Code generator state */
	GenBlock *pBlock,    /* Target block */
	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/
	sxu32 nFirstInstr,   /* First instruction to compile */
	void *pUserData      /* Upper layer private data */
	)
{
	/* Initialize block fields */
	pBlock->nFirstInstr = nFirstInstr;
	pBlock->pUserData   = pUserData;
	pBlock->pGen        = pGen;
	pBlock->iFlags      = iType;
	pBlock->pParent     = 0;
	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));
	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));
}
/*
 * Allocate a new block instance.
 * Return SXRET_OK and write a pointer to the new instantiated block
 * on success.Otherwise generate a compile-time error and abort
 * processing on failure.
 */
PH7_PRIVATE sxi32 GenStateEnterBlock(
	ph7_gen_state *pGen,  /* Code generator state */
	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/
	sxu32 nFirstInstr,    /* First instruction to compile */
	void *pUserData,      /* Upper layer private data */
	GenBlock **ppBlock    /* OUT: instantiated block */
	)
{
	GenBlock *pBlock;
	/* Allocate a new block instance */
	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));
	if( pBlock == 0 ){
		/* If the supplied memory subsystem is so sick that we are unable to allocate
		 * a tiny chunk of memory, there is no much we can do here.
		 */
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");
		/* Abort processing immediately */
		return SXERR_ABORT;
	}
	/* Zero the structure */
	SyZero(pBlock,sizeof(GenBlock));
	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);
	/* Link to the parent block */
	pBlock->pParent = pGen->pCurrent;
	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's
	 * and a label's positions can be compared after compilation (see aLoopParent). */
	if( iType & (GEN_BLOCK_LOOP|GEN_BLOCK_SWITCH) ){
		sxu32 nParent = pGen->nCurLoopId;
		pGen->nLoopId++;
		SySetPut(&pGen->aLoopParent,(const void *)&nParent);
		pBlock->nLoopId = pGen->nLoopId;
		pBlock->nOuterLoopId = nParent;
		pGen->nCurLoopId = pGen->nLoopId;
	}
	/* A try/catch/finally block gets a scope id, and remembers the scope it nests inside,
	 * so the chain between any two points can be walked after compilation (aScope). Every
	 * other block simply inherits the scope in effect. */
	pBlock->nOuterScopeId = pGen->nCurScopeId;
	pBlock->nScopeId = pGen->nCurScopeId;
	if( iType & GEN_BLOCK_EXCEPTION ){
		GenScope sScope;
		sScope.nParent = pGen->nCurScopeId;
		sScope.pUserData = pUserData;
		if( iType & GEN_BLOCK_FINALLY ){
			sScope.iKind = GEN_SCOPE_FINALLY;
		}else if( iType & GEN_BLOCK_DETACHED ){
			sScope.iKind = GEN_SCOPE_DETACHED;
		}else{
			/* A try. pUserData is its ph7_exception, which every try-block site passes at
			 * ENTRY precisely so this can classify it. */
			sScope.iKind = GenStateInlineTryCatch(pGen) ? GEN_SCOPE_TRY_INLINE : GEN_SCOPE_TRY;
		}
		if( SySetPut(&pGen->aScope,(const void *)&sScope) == SXRET_OK ){
			pBlock->nScopeId = SySetUsed(&pGen->aScope);
			pGen->nCurScopeId = pBlock->nScopeId;
		}
	}
	/* Mark as the current block */
	pGen->pCurrent = pBlock;
	if( ppBlock ){
		/* Write a pointer to the new instance */
		*ppBlock = pBlock;
	}
	return SXRET_OK;
}
/*
 * Release block fields without freeing the whole instance.
 */
static void GenStateReleaseBlock(GenBlock *pBlock)
{
	SySetRelease(&pBlock->aPostContFix);
	SySetRelease(&pBlock->aJumpFix);
}
/*
 * Release a block.
 */
static void GenStateFreeBlock(GenBlock *pBlock)
{
	ph7_gen_state *pGen = pBlock->pGen;
	GenStateReleaseBlock(&(*pBlock));
	/* Free the instance */
	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);
}
/*
 * POP and release a block from the stack of compiled blocks.
 */
PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)
{
	GenBlock *pBlock = pGen->pCurrent;
	if( pBlock == 0 ){
		/* No more block to pop */
		return SXERR_EMPTY;
	}
	if( pBlock->iFlags & (GEN_BLOCK_LOOP|GEN_BLOCK_SWITCH) ){
		pGen->nCurLoopId = pBlock->nOuterLoopId;
	}
	if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){
		pGen->nCurScopeId = pBlock->nOuterScopeId;
	}
	/* Point to the upper block */
	pGen->pCurrent = pBlock->pParent;
	if( ppBlock ){
		/* Write a pointer to the popped block */
		*ppBlock = pBlock;
	}else{
		/* Safely release the block */
		GenStateFreeBlock(&(*pBlock));
	}
	return SXRET_OK;
}
/*
 * PHP-parity redeclaration guard.
 *
 * PHP raises a fatal "Cannot redeclare ..." when a class/interface/trait/enum
 * or a function is declared a second time. PHL hoists every declaration into
 * the VM at compile time (so `if(false){class C{}}` already makes C exist), and
 * historically it silently *overwrote* duplicates. We reproduce PHP for the
 * case that matters and that real code hits: a declaration that is
 * UNCONDITIONAL and at file top level, whose name is already bound by another
 * unconditional top-level declaration (or by a builtin). Conditional
 * declarations (inside if/loops/switch/try or nested in a function) are left
 * hoisting as before, so the `if(!class_exists('C')){class C{}}` and
 * `if(false){class C{}} class C{}` guard idioms keep working.
 *
 * Included files compile at include time (i.e. at run time relative to the main
 * script), so this compile-time check surfaces the fatal at the same moment PHP
 * does for the cross-include case too.
 */
PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)
{
	GenBlock *pBlock = pGen->pCurrent;
	while( pBlock ){
		if( pBlock->iFlags & (GEN_BLOCK_COND|GEN_BLOCK_LOOP|GEN_BLOCK_FUNC|GEN_BLOCK_SWITCH|GEN_BLOCK_EXCEPTION) ){
			return 0; /* conditional / nested */
		}
		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){
			return 1; /* reached the global block with no conditional ancestor */
		}
		pBlock = pBlock->pParent;
	}
	return 1;
}
/*
 * Guard a top-level function about to be installed. Same contract as the class
 * guard above.
 */
PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)
{
	SyHashEntry *pEntry;
	if( !GenStateUnconditionalTopLevel(pGen) ){
		return SXRET_OK;
	}
	pFunc->iFlags |= VM_FUNC_BOUND;
	if( pGen->pVm->bCompilingBuiltin ){
		return SXRET_OK;
	}
	/* Host functions present AT COMPILE TIME are guarded here, and they have to be
	 * for the migration of the builtin library into C to be behaviour-preserving: a
	 * function moving from an embedded PHP chunk to a C routine moves from hFunction
	 * to hHostFunction, and would otherwise silently LOSE the redeclaration guard it
	 * had. `function ini_get(){}` really did win over the builtin for the rest of
	 * the program once ini_get became C.
	 *
	 * Which builtins that covers depends on WHERE they register. The subsystems
	 * installed inside PH7_VmInit's bCompilingBuiltin window (INI, libxml, ...) are
	 * in hHostFunction before any user code compiles, so they are caught. The ~650
	 * core builtins (strlen, ...) register later, in PH7_VmMakeReady, which runs
	 * AFTER compilation — hHostFunction has no entry for them yet, so shadowing one
	 * remains the known divergence it has always been (php fatals; §7.2). Nothing
	 * about their behaviour changes here.
	 *
	 * The bCompilingBuiltin early-return above keeps the prelude itself exempt. */
	if( SyHashGet(&pGen->pVm->hHostFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte) ){
		PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,
			"Cannot redeclare function %z()",&pFunc->sName);
		return SXERR_ABORT;
	}
	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);
	if( pEntry ){
		ph7_vm_func *pPrev = (ph7_vm_func *)pEntry->pUserData;
		while( pPrev ){
			if( pPrev->iFlags & VM_FUNC_BOUND ){
				if( pPrev->sFile.nByte > 0 ){
					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,
						"Cannot redeclare function %z() (previously declared in %.*s:%u)",
						&pFunc->sName,pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);
				}else{
					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,
						"Cannot redeclare function %z()",&pFunc->sName);
				}
				return SXERR_ABORT;
			}
			pPrev = pPrev->pNextName;
		}
	}
	return SXRET_OK;
}
/*
 * Emit a forward jump.
 * Notes on forward jumps
 *  Compilation of some PHP constructs such as if,for,while and the logical or
 *  (||) and logical and (&&) operators in expressions requires the
 *  generation of forward jumps.
 *  Since the destination PC target of these jumps isn't known when the jumps
 *  are emitted, we record each forward jump in an instance of the following
 *  structure. Those jumps are fixed later when the jump destination is resolved.
 */
PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)
{
	JumpFixup sJumpFix;
	sxi32 rc;
	/* Init the JumpFixup structure */
	sJumpFix.nJumpType = nJumpType;
	sJumpFix.nInstrIdx = nInstrIdx;
	/* Remember which bytecode array the emitted instruction lives in: the block whose
	 * table this lands in may be resolved after a container swap (see JumpFixup). */
	sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pBlock->pGen->pVm);
	/* Insert in the jump fixup table */
	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);
	return rc;
}
/*
 * TRUE when the body being compiled has its try/catch/finally compiled INLINE
 * (ROOT C, generator bodies) rather than into detached mini-programs.
 */
PH7_PRIVATE int GenStateInlineTryCatch(ph7_gen_state *pGen)
{
	return pGen->bInGenerator && pGen->pVm->bInlineTryCatch;
}
/*
 * Walk the scope chain from nFrom (where a jump is) out to nTo (where it lands) and
 * describe what it crosses. One walk serves `break`, `continue` and `goto` alike,
 * because they all ask the same question of the same chain — only the two endpoints
 * differ, and for a goto they are not both known until compilation ends.
 *
 * Returns TRUE when nTo was actually reached, i.e. the target's scope ENCLOSES the
 * jump. FALSE means the target sits inside a try/catch the jump is not in — jumping
 * into one, which PHL cannot express (its handler is pushed by the try's
 * OP_LOAD_EXCEPTION, and a catch body is a mini-program entered at instruction 0).
 * Depth counting cannot answer this: two sibling trys have the same depth.
 *
 * What is counted, for the opcode the caller then picks:
 *  nDet    — DETACHED catch/finally bodies left. Each is its own bytecode array, so a
 *            jump out of one cannot be a plain OP_JMP: it parks and travels out through
 *            one OP_POP_EXCEPTION landing pad per boundary (OP_CATCH_JMP);
 *  nTry    — legacy trys left whose OP_POP_EXCEPTION the jump SKIPS, so nothing else
 *            would run their finally. Trys BELOW the first boundary do not qualify: a
 *            break/continue emits their OP_POP_EXCEPTION right here (bEmitPops), and a
 *            goto drains them where it parks — hence the reset when one is reached;
 *  nInline — ROOT C inline trys left. Their finallys are driven by VmFinallyAdvance,
 *            not by the aException drain, so they are crossed with OP_SET_FINALLY_JMP;
 *  nFinally — `finally` bodies left, which php forbids outright. When this is non-zero the
 *            three above are NOT computed: callers must test it first and reject.
 */
PH7_PRIVATE int GenStateJumpScope(ph7_gen_state *pGen,sxu32 nFrom,sxu32 nTo,int bEmitPops,
	GenJumpScope *pScope)
{
	GenScope *aScope = (GenScope *)SySetBasePtr(&pGen->aScope);
	sxu32 nUsed = SySetUsed(&pGen->aScope);
	sxu32 nCur = nFrom;
	SyZero(pScope,sizeof(*pScope));
	while( nCur != nTo ){
		GenScope *pScopeEnt;
		if( nCur == 0 || nCur > nUsed ){
			return FALSE; /* ran off the top without meeting nTo */
		}
		pScopeEnt = &aScope[nCur - 1];
		if( pScopeEnt->iKind == GEN_SCOPE_FINALLY ){
			/* php: `jump out of a finally block is disallowed`. Counted rather than
			 * rejected here because the caller owns the diagnostic and its line — but
			 * ONLY counted: the jump is illegal, so the other three fields are left as
			 * they are rather than pretending to describe a crossing that will never be
			 * emitted. (They could not be right anyway: this kind covers both the legacy
			 * detached finally and the generator's INLINE one, which is not a separate
			 * bytecode container.) Every caller tests nFinally first. A jump that stays
			 * INSIDE the finally never reaches this scope, so it stays legal. */
			pScope->nFinally++;
		}else if( pScopeEnt->iKind == GEN_SCOPE_DETACHED ){
			if( pScope->nDet == 0 ){
				pScope->nTry = 0;    /* below the first boundary: not the landing pad's */
				pScope->nInline = 0;
			}
			pScope->nDet++;
		}else if( pScopeEnt->iKind == GEN_SCOPE_TRY_INLINE ){
			pScope->nInline++;
		}else if( pScope->nDet == 0 && bEmitPops ){
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pScopeEnt->pUserData,0);
		}else{
			pScope->nTry++;
		}
		nCur = pScopeEnt->nParent;
	}
	return TRUE;
}
/*
 * Pick the jump opcode for a crossing described by GenStateJumpScope, and its iP1.
 * Shared by break/continue (which know their target at emit time) and goto (which
 * settles this in GenStateFixGoto, once the label fixes the counts).
 */
PH7_PRIVATE sxi32 GenStateScopeJumpOp(const GenJumpScope *pCross,sxi32 *piP1)
{
	if( pCross->nDet > 0 || pCross->nTry > 0 ){
		*piP1 = PH7_CATCH_JMP_P1(pCross->nDet,pCross->nTry);
		return PH7_OP_CATCH_JMP;
	}
	if( pCross->nInline > 0 ){
		*piP1 = (sxi32)pCross->nInline;
		return PH7_OP_SET_FINALLY_JMP;
	}
	*piP1 = 0;
	return PH7_OP_JMP;
}
/*
 * Resolve a recorded fixup to its VM instruction, in the container it was emitted
 * into (see JumpFixup.pContainer) rather than whichever one is current now.
 */
PH7_PRIVATE VmInstr * GenStateFixupInstr(const JumpFixup *pFix)
{
	return (VmInstr *)SySetAt(pFix->pContainer,pFix->nInstrIdx);
}
/*
 * Fix a forward jump now the jump destination is resolved.
 * Return the total number of fixed jumps.
 * Notes on forward jumps:
 *  Compilation of some PHP constructs such as if,for,while and the logical or
 *  (||) and logical and (&&) operators in expressions requires the
 *  generation of forward jumps.
 *  Since the destination PC target of these jumps isn't known when the jumps
 *  are emitted, we record each forward jump in an instance of the following
 *  structure.Those jumps are fixed later when the jump destination is resolved.
 */
PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)
{
	JumpFixup *aFix;
	VmInstr *pInstr;
	sxu32 nFixed;
	sxu32 n;
	/* Point to the jump fixup table */
	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);
	/* Fix the desired jumps */
	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){
		if( aFix[n].nJumpType < 0 ){
			/* Already fixed */
			continue;
		}
		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){
			/* Not of our interest */
			continue;
		}
		/* Point to the instruction to fix */
		pInstr = GenStateFixupInstr(&aFix[n]);
		if( pInstr ){
			pInstr->iP2 = nJumpDest;
			nFixed++;
			/* Mark as fixed */
			aFix[n].nJumpType = -1;
		}
	}
	/* Total number of fixed jumps */
	return nFixed;
}
/*
 * Fix a 'goto' now the jump destination is resolved.
 * The goto statement can be used to jump to another section
 * in the program.
 * Refer to the routine responsible of compiling the goto
 * statement for more information.
 */
PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)
{
	JumpFixup *pJump,*aJumps;
	GenJumpScope sCross;
	Label *pLabel;
	VmInstr *pInstr;
	sxi32 rc;
	sxu32 n;
	/* Point to the goto table */
	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);
	/* Fix */
	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){
		pJump = &aJumps[n];
		/* Extract the target label */
		/* A label declared in ANOTHER function is not a destination: the lookup is keyed
		 * on the goto's own function, so a same-named label elsewhere simply does not
		 * answer and this reports php's undefined-label fatal. */
		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,pJump->pFunc,&pLabel);
		if( rc != SXRET_OK ){
			/* No such label */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			continue;
		}
		/* php's one goto restriction: you may not jump INTO a loop or a switch. The label
		 * is inside one exactly when it carries a loop id; that is legal only if the same
		 * loop also encloses the goto, i.e. the label's loop is the goto's loop or one of
		 * its ancestors. Walk up from the goto's loop looking for the label's. */
		if( pLabel->nLoopId != 0 ){
			sxu32 *aParent = (sxu32 *)SySetBasePtr(&pGen->aLoopParent);
			sxu32 nCur = pJump->nLoopId;
			int bInside = 0;
			while( nCur != 0 ){
				if( nCur == pLabel->nLoopId ){
					bInside = 1;
					break;
				}
				nCur = (nCur <= SySetUsed(&pGen->aLoopParent)) ? aParent[nCur - 1] : 0;
			}
			if( !bInside ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,
					"'goto' into loop or switch statement is disallowed");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				continue;
			}
		}
		/* What the jump crosses, and whether it is legal at all: the label's scope must
		 * ENCLOSE the goto. Jumping INTO a try/catch/finally is fine in php (its handlers
		 * are instruction RANGES, so landing anywhere in the body is being in the try),
		 * but PHL pushes a handler at the try's OP_LOAD_EXCEPTION and runs a catch body
		 * as a mini-program entered at its first instruction — there is no way to arrive
		 * mid-body with the handler live. Say so rather than jump nowhere in silence,
		 * skip a finally, or land in a foreign array. */
		if( !GenStateJumpScope(&(*pGen),pJump->nScopeId,pLabel->nScopeId,FALSE,&sCross) ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,
				"'goto' into a try, catch or finally block is disallowed");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			continue;
		}
		if( sCross.nFinally > 0 ){
			/* php's other structural rule, shared with break/continue. Tested AFTER the
			 * reach test above, so a goto that both leaves a finally and lands somewhere
			 * that does not enclose it reports the into-a-try wording instead of this
			 * one. Both are fatal on the same line, and the two cannot be told apart
			 * without a second walk outward from the LABEL — php accepts one of them
			 * (`finally { goto L; try { L: … } }`), which is the §7.2 divergence, and
			 * rejects the other. Not worth a second walk for a message on input that is
			 * rejected either way. */
			if( GenStateJumpOutOfFinally(&(*pGen),pJump->nLine) == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			continue;
		}
		/* Fix the jump now the destination is resolved — in the container the goto was
		 * emitted into, which for a goto inside a catch/finally body is not the one
		 * current here (gotos resolve at end of compilation, after every swap back). */
		pInstr = GenStateFixupInstr(pJump);
		if( pInstr ){
			pInstr->iP2 = pLabel->nJumpDest;
			if( pInstr->iOp == PH7_OP_CATCH_JMP ){
				/* Emitted as a structure-crossing jump because the goto sits inside a
				 * try or a detached body. Now that the crossing is known it may well
				 * turn out to leave nothing, and downgrade to a plain OP_JMP. */
				sxi32 iP1 = 0;
				pInstr->iOp = (sxu8)GenStateScopeJumpOp(&sCross,&iP1);
				pInstr->iP1 = iP1;
			}
		}
	}
	/* php says nothing about a label nobody jumps to — the old "defined but not
	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */
	return SXRET_OK;
}
/*
 * Check if a given token value is installed in the literal table.
 */
PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)
{
	SyHashEntry *pEntry;
	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);
	if( pEntry == 0 ){
		return SXERR_NOTFOUND;
	}
	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);
	return SXRET_OK;
}
/*
 * Install a given constant index in the literal table.
 * In order to be installed, the ph7_value must be of type string.
 *
 * NOTE: empty strings are deliberately omitted here.  The VM reserves a
 * single shared constant for "" during initialization (pVm->nEmptyStringIdx)
 * and the compiler emits a LOADC referencing that slot whenever an empty
 * literal is encountered.  This keeps the literal hash from growing when
 * many "" literals appear in user code.
 */
PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)
{
	if( SyBlobLength(&pObj->sBlob) > 0 ){
		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));
	}
	return SXRET_OK;
}
/*
 * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]
 * in the constant table.
 */
PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)
{
	ph7_value *pObj;
	sxu32 nIdx = 0; /* cc warning */
	/* Reserve a new constant */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
		return 0;
	}
	*pIdx = nIdx;
	/* TODO(chems): Create a numeric table (64bit int keys) same as
	 * the constant string iterals table [optimization purposes].
	 */
	return pObj;
}
/*
 * Implementation of the PHP language constructs.
 */
/*
 * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap
 * that reflects the caller file's strict_types mode. Returns the (possibly
 * newly allocated and zero-initialized) map pointer. In weak-mode files
 * this is a no-op and the caller's p3 is returned unchanged.
 *
 * NOTE: on allocation failure the call reverts to weak semantics rather
 * than aborting compilation — out-of-memory during a map allocation is
 * vanishingly unlikely and silently dropping to weak mode matches the
 * surrounding callsites' zero-check fallback pattern.
 */
PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)
{
	VmCallArgMap *pMap;
	if( !pGen->bStrictTypes ) return p3;
	if( p3 == 0 ){
		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));
		if( pMap == 0 ) return 0;
		SyZero(pMap,sizeof(VmCallArgMap));
		p3 = (void *)pMap;
	}
	((VmCallArgMap *)p3)->bStrict = 1;
	return p3;
}
/* Forward declaration */
static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);
/* Forward declarations */
/*
 * Recover from a compile-time error. In other words synchronize
 * the token stream cursor with the first semi-colon seen.
 */
static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)
{
	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Check if the given identifier name is reserved or not.
 * Return TRUE if reserved.FALSE otherwise.
 */
PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)
{
	if( pName->nByte == sizeof("null") - 1 ){
		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){
			return TRUE;
		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){
			return TRUE;
		}
	}else if( pName->nByte == sizeof("false") - 1 ){
		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){
			return TRUE;
		}
	}
	/* Not a reserved constant */
	return FALSE;
}
/*
 * Chain operators participate in a postfix member-access chain.
 * A `?->` emitted inside such a chain must short-circuit to the end of
 * the chain, not just past its own member access. Any non-chain ancestor
 * terminates the chain and is where pending NULLSAFE_JMP targets are patched.
 */
#define GEN_IS_CHAIN_OP(iOp) \
  ((iOp) == EXPR_OP_ARROW || (iOp) == EXPR_OP_NULLSAFE_ARROW || \
   (iOp) == EXPR_OP_DC    || (iOp) == EXPR_OP_SUBSCRIPT     || \
   (iOp) == EXPR_OP_FUNC_CALL)
/*
 * The chain operators that ACCESS a container -- the same four minus the call,
 * whose result is an ordinary value however the chain around it is read. Used
 * to spot an INTERMEDIATE link of an isset()/empty() chain, which php reads for
 * its value rather than for a truth.
 */
#define GEN_IS_ACCESS_OP(iOp) \
  ((iOp) == EXPR_OP_ARROW || (iOp) == EXPR_OP_NULLSAFE_ARROW || \
   (iOp) == EXPR_OP_DC    || (iOp) == EXPR_OP_SUBSCRIPT)

/*
 * Patch every pending NULLSAFE_JMP recorded after the given baseline so
 * that it jumps to the current end-of-emission instruction. Then drop the
 * patched entries from the pending set.
 */
static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)
{
	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);
	sxu32 nTarget;
	sxu32 *aIdx;
	sxu32 i;
	if( nCur <= nBaseline ){
		return;
	}
	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);
	nTarget = PH7_VmInstrLength(pGen->pVm);
	for( i = nBaseline ; i < nCur ; ++i ){
		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);
		if( pInstr ){
			pInstr->iP2 = (sxi32)nTarget;
		}
	}
	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);
}

/*
 * Does this call-argument node reach its target THROUGH a property? `$o->p`,
 * `$this->m['k']`, `$o->a->b` all do; `$a['k']`, `$a[$i][$j]` and a plain `$var`
 * do not.
 *
 * Only the SUBSCRIPT spine is walked, because that is the only operator whose
 * base is still part of the same lvalue: everything else (a call, a cast, `::`,
 * `?->`) either ends the path or is not writable through at all.
 */
static int GenStateArgHasPropertyStep(ph7_expr_node *pNode)
{
	while( pNode && pNode->pOp ){
		if( pNode->pOp->iOp == EXPR_OP_ARROW ){
			return 1;
		}
		if( pNode->pOp->iOp != EXPR_OP_SUBSCRIPT ){
			return 0;
		}
		pNode = pNode->pLeft;
	}
	return 0;
}
/*
 * By-reference out-parameters of builtin functions.
 *
 * PH7 foreign/builtin functions carry no parameter signature, so the call
 * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument
 * ($matches) is passed by reference. Without that knowledge an *undefined*
 * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)
 * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-
 * back is a silent no-op — the caller's variable stays null unless it was
 * pre-initialised. This table maps a builtin name to a bitmask of the argument
 * positions it writes back through, letting the caller auto-vivify just those
 * argument variables (PHP's exact "passing an undefined var by reference
 * creates it" behaviour).
 *
 * Bit N (1u<<N) set => the argument at position N is by reference. Out-params
 * live at low indices, so a 32-bit mask is sufficient.
 */
static sxu32 GenStateByRefBuiltinMask(SyString *pName)
{
	static const struct {
		const char *zName;
		sxu32 nByte;
		sxu32 mask;
	} aByRef[] = {
		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */
		{ "settype",                7, 1u<<0 },  /* &$var    (apArg[0]) */
		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */
		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */
		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */
		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */
		{ "flock",                  5, 1u<<2 },  /* &$would_block (apArg[2]) */
		{ "getopt",                 6, 1u<<2 },  /* &$rest_index (apArg[2]) */
		{ "is_callable",           11, 1u<<2 },  /* &$callable_name (apArg[2]) */
		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */
		{ "headers_sent",         12, (1u<<0)|(1u<<1) },  /* &$filename, &$line */
		{ "str_replace",           11, 1u<<3 },  /* &$count  (apArg[3]) */
		{ "str_ireplace",          12, 1u<<3 },  /* &$count  (apArg[3]) */
		{ "fsockopen",              9, (1u<<2)|(1u<<3) },  /* &$error_code, &$error_message */
		{ "pfsockopen",            10, (1u<<2)|(1u<<3) },  /* same */
		{ "stream_socket_client",  20, (1u<<1)|(1u<<2) },  /* &$error_code, &$error_message */
		{ "stream_socket_server",  20, (1u<<1)|(1u<<2) },  /* same pair */
		{ "stream_socket_accept",  20, 1u<<2 },            /* &$peer_name (apArg[2]) */
		{ "stream_select",         13, (1u<<0)|(1u<<1)|(1u<<2) }, /* &$read, &$write, &$except */
		{ "stream_socket_recvfrom",22, 1u<<3 },            /* &$address (apArg[3]) */
		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */
		{ "exec",                   4, (1u<<1)|(1u<<2) },  /* &$output, &$result_code */
		{ "system",                 6, 1u<<1 },  /* &$result_code (apArg[1]) */
		{ "passthru",               8, 1u<<1 },  /* &$result_code (apArg[1]) */
		/* A by-ref VARIADIC tail: every actual from the third on is one of
		 * sscanf()'s `&...$vars`, so each is created rather than read. */
		/* ext/openssl's out-params. Each is the argument php declares `&$x`;
		 * the certificate half adds its own rows beside these. */
		{ "openssl_encrypt",             15, 1u<<5 },  /* &$tag (apArg[5]) */
		{ "openssl_random_pseudo_bytes", 27, 1u<<1 },  /* &$strong_result */
		{ "openssl_pkey_export",         19, 1u<<1 },  /* &$output */
		{ "openssl_sign",                12, 1u<<1 },  /* &$signature */
		{ "openssl_private_encrypt",     23, 1u<<1 },  /* &$encrypted_data */
		{ "openssl_private_decrypt",     23, 1u<<1 },  /* &$decrypted_data */
		{ "openssl_public_encrypt",      22, 1u<<1 },  /* &$encrypted_data */
		{ "openssl_public_decrypt",      22, 1u<<1 },  /* &$decrypted_data */
		{ "openssl_seal",                12, (1u<<1)|(1u<<2)|(1u<<5) },
		{ "openssl_open",                12, 1u<<1 },  /* &$output */
		{ "openssl_x509_export",         19, 1u<<1 },  /* &$output */
		{ "openssl_csr_export",          18, 1u<<1 },  /* &$output */
		{ "openssl_csr_new",             15, 1u<<1 },  /* &$private_key */
		{ "openssl_pkcs12_export",       21, 1u<<1 },  /* &$output */
		{ "openssl_pkcs12_read",         19, 1u<<1 },  /* &$certificates */
		{ "openssl_pkcs7_read",          18, 1u<<1 },  /* &$certificates */
		{ "openssl_cms_read",            16, 1u<<1 },  /* &$certificates */
		/* ext/pcntl's out-params. `pcntl_signal_dispatch` has none; every
		 * other by-reference argument in the extension is one of these. */
		{ "pcntl_waitpid",         13, (1u<<1)|(1u<<3) }, /* &$status, &$resource_usage */
		{ "pcntl_wait",            10, (1u<<0)|(1u<<2) }, /* &$status, &$resource_usage */
		{ "pcntl_waitid",          12, (1u<<2)|(1u<<4) }, /* &$info,   &$resource_usage */
		{ "pcntl_sigprocmask",     17, 1u<<2 },           /* &$old_signals */
		{ "pcntl_sigwaitinfo",     17, 1u<<1 },           /* &$info */
		{ "pcntl_sigtimedwait",    18, 1u<<1 },           /* &$info */
		{ "sscanf",                 6, ~((1u<<2) - 1u) },
		{ "fscanf",                 6, ~((1u<<2) - 1u) },
	};
	sxu32 i;
	if( pName == 0 || pName->zString == 0 || pName->nByte == 0 ){
		return 0;
	}
	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){
		if( pName->nByte == aByRef[i].nByte
		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){
			return aByRef[i].mask;
		}
	}
	return 0;
}
/*
 * What may be passed by REFERENCE is decided from the argument's SHAPE, at compile
 * time, exactly as php decides it (zend_compile_args -> zend_is_variable).
 *
 * php sorts every actual argument into three buckets:
 *
 *   GEN_ARG_LVALUE   a variable, an element, a property, a static property. It has a
 *                    slot, so a by-ref parameter aliases it.
 *   GEN_ARG_TEMPCALL the result of a call or of `new`. It has no slot, but php cannot
 *                    know at compile time whether the callee returns a reference, so it
 *                    defers: E_NOTICE "Only variables should be passed by reference",
 *                    then it operates on the temporary.
 *   GEN_ARG_NONE     everything else — a literal, an operator/cast result, a class
 *                    constant, `@$x`, `$o?->p`, an assignment. Binding one to a by-ref
 *                    parameter is a catchable Error at the CALL.
 *
 * Deciding it from the argument's runtime memobj instead does not work and was silently
 * wrong in both directions: an arithmetic or concatenation result keeps its LEFT operand's
 * slot index, so `f($i + 1)` with `function f(&$x)` aliased and overwrote `$i`; and a
 * builtin's by-ref row saw only "no slot", which a call result has too.
 */
#define GEN_ARG_LVALUE   0
#define GEN_ARG_TEMPCALL 1
#define GEN_ARG_NONE     2
static int GenStateArgShape(ph7_expr_node *pNode)
{
	if( pNode == 0 ){
		return GEN_ARG_NONE;
	}
	if( pNode->pOp == 0 ){
		/* A leaf: only the `$…` family is a variable. Everything else the parser
		 * files here — a literal, an array/list constructor, a closure, `match`,
		 * `clone` — is a temporary. */
		return pNode->xCode == PH7_CompileVariable ? GEN_ARG_LVALUE : GEN_ARG_NONE;
	}
	switch( pNode->pOp->iOp ){
	case EXPR_OP_SUBSCRIPT: /* $a[k], and any base: php accepts g()[0] and C::m()[0] */
	case EXPR_OP_ARROW:     /* $o->p */
		return GEN_ARG_LVALUE;
	case EXPR_OP_DC:
		/* `C::$s` is a static property (an lvalue); `C::K` is a class constant and
		 * `C::CASE` an enum case, neither of which php will bind. The right operand
		 * tells them apart. */
		return ( pNode->pRight && pNode->pRight->pOp == 0
		      && pNode->pRight->xCode == PH7_CompileVariable )
			? GEN_ARG_LVALUE : GEN_ARG_NONE;
	case EXPR_OP_FUNC_CALL:
	case EXPR_OP_NEW:
		return GEN_ARG_TEMPCALL;
	case EXPR_OP_REF:
		/* `take($q = &$p)`: a reference ASSIGNMENT hands back the reference it
		 * made, so php passes it on to a by-ref parameter and all three names end
		 * up aliasing one slot. A plain `$q = $p` does not -- php's ASSIGN yields
		 * a temporary where ASSIGN_REF yields the VAR -- which is why only this
		 * one arm moves. */
		return GEN_ARG_LVALUE;
	default:
		/* Includes `?->` (php: "Cannot use nullsafe operator in write context"),
		 * `@$x`, `$q = …`, `clone $o` and every arithmetic/logical operator. */
		return GEN_ARG_NONE;
	}
}
/*
 * Recover the bare global-builtin name from a call's callee node.
 *
 * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and
 * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP
 * then one identifier) — both resolve to the global builtin. A deeper-qualified
 * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is
 * returned for it. pEnd is exclusive (one past the last name token). Returns
 * {NULL,0} in *pOut when the callee is not a plain global function name.
 */
static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)
{
	SyToken *p, *pEnd;
	pOut->zString = 0;
	pOut->nByte = 0;
	if( pLeft == 0 || pLeft->pStart == 0 || pLeft->pEnd == 0 ){
		return;
	}
	p = pLeft->pStart;
	pEnd = pLeft->pEnd;
	/* Optional single leading namespace separator (absolute path). */
	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){
		p++;
	}
	if( p >= pEnd || (p->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		return;
	}
	/* Must be a single component: nothing follows the name token. */
	if( p + 1 != pEnd ){
		return;
	}
	*pOut = p->sData;
}
/*
 * Is this expression node the bare variable `$this`?
 */
PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode)
{
	SyToken *pTok;
	if( pNode == 0 || pNode->pOp != 0 || pNode->xCode != PH7_CompileVariable ){
		return 0;
	}
	pTok = pNode->pStart;
	if( pTok == 0 || pNode->pEnd == 0 || pNode->pEnd < &pTok[2] ){
		return 0;
	}
	return (pTok[0].nType & PH7_TK_DOLLAR) != 0
		&& (pTok[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) != 0
		&& pTok[1].sData.nByte == sizeof("this")-1
		&& SyMemcmp((const void *)pTok[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0;
}
/*
 * TRUE when codegen is inside a real FUNCTION body — php's
 * `CG(active_op_array)->function_name`. A synthetic block (a match() arm's
 * throw-fixup) carries no ph7_vm_func and is not a scope.
 */
static int GenStateInFunction(ph7_gen_state *pGen)
{
	GenBlock *pBlock = pGen->pCurrent;
	while( pBlock ){
		if( (pBlock->iFlags & GEN_BLOCK_FUNC) && pBlock->pUserData ){
			return 1;
		}
		pBlock = pBlock->pParent;
	}
	return 0;
}
/*
 * php's SPECIALIZED builtins — the list behind `Cannot use result of built-in
 * function in write context`.
 *
 * The wording says "built-in function" but the rule is not about builtins: php
 * refuses the write when the call was compiled to an OPCODE of its own rather
 * than a real call, because a specialized opcode leaves a TMP where a call
 * leaves a VAR (`zend_separate_if_call_and_write`). So `strlen("x")[0] = 1` and
 * `count([1])[0] = 1` are compile fatals while `array_values([1])[0] = 2`,
 * `str_split("ab")[0] = "z"` and `get_object_vars($o)["k"] = 2` all RUN — the
 * difference being php's `zend_try_compile_special_func_ex` table, reproduced
 * here name for name with the ARITY each entry demands.
 *
 * Six of php's names are deliberately absent, and only the last pair is a
 * simplification — the other four are not refusals of php's at all:
 *   `chr`/`ord` gate on BP_VAR_R, so they are never special in a WRITE context;
 *   `call_user_func`/`call_user_func_array` emit a REAL call, so their result is
 *     a VAR and php does not refuse a write through it either;
 *   `in_array` and `array_slice` gate on the CONTENTS of a literal array
 *     argument and on a `func_get_args()`-shaped first argument — value-dependent
 *     shapes no program writes through, left out under §10. Leaving them out
 *     ACCEPTS where php refuses, which is the direction that keeps running a
 *     program php runs.
 * Verified by sweeping every internal function of both engines at arities 0-3:
 * the two specialized sets are identical, 27 names at the same arities.
 */
#define SPECFN_LITERAL_ARG0 0x01 /* php gives up unless argument #1 is a literal */
#define SPECFN_ANY_ARGS     0x02 /* …and `assert` is decided BEFORE php's unpack/named
                                  * bail, so it stays special even for `assert(...$a)` */
#define SPECFN_IN_FUNC      0x04 /* php's gate reads CG(active_op_array)->function_name:
                                  * at GLOBAL scope it emits a real call, whose runtime
                                  * Error ("cannot be called from the global scope") is
                                  * what the program actually gets */
#define SPECFN_FORMAT_ARG0  0x08 /* …and `sprintf` also needs php's format arithmetic
                                  * (implies SPECFN_LITERAL_ARG0) */
static const struct {
	const char *zName;
	int nMinArg;   /* inclusive */
	int nMaxArg;   /* inclusive; -1 = variadic */
	int iFlags;
} aSpecialFunc[] = {
	{ "strlen",           1,  1, 0 },
	{ "is_null",          1,  1, 0 },  { "is_bool",          1,  1, 0 },
	{ "is_long",          1,  1, 0 },  { "is_int",           1,  1, 0 },
	{ "is_integer",       1,  1, 0 },  { "is_float",         1,  1, 0 },
	{ "is_double",        1,  1, 0 },  { "is_string",        1,  1, 0 },
	{ "is_array",         1,  1, 0 },  { "is_object",        1,  1, 0 },
	{ "is_resource",      1,  1, 0 },  { "is_scalar",        1,  1, 0 },
	{ "boolval",          1,  1, 0 },  { "intval",           1,  1, 0 },
	{ "floatval",         1,  1, 0 },  { "doubleval",        1,  1, 0 },
	{ "strval",           1,  1, 0 },
	{ "count",            1,  1, 0 },  { "sizeof",           1,  1, 0 },
	{ "get_class",        0,  1, 0 },  { "get_called_class", 0,  0, 0 },
	{ "gettype",          1,  1, 0 },
	{ "func_num_args",    0,  0, SPECFN_IN_FUNC },
	{ "func_get_args",    0,  0, SPECFN_IN_FUNC },
	{ "array_key_exists", 2,  2, 0 },
	{ "defined",          1,  1, SPECFN_LITERAL_ARG0 },
	{ "sprintf",          1, -1, SPECFN_LITERAL_ARG0|SPECFN_FORMAT_ARG0 },
	/* php compiles assert() to its own opcode pair "independently of compiler
	 * flags", in zend_compile_call BEFORE the special-func table is consulted —
	 * so every arity counts and an unpacked argument does not exempt it. */
	{ "assert",           0, -1, SPECFN_ANY_ARGS },
};
/*
 * TRUE when this call node is one php compiles to an opcode of its own, so a
 * write THROUGH its result is php's built-in-function refusal. pName is the
 * callee's bare global name, already resolved by GenStateCallBuiltinName.
 */
static int GenStateCallIsSpecialized(ph7_gen_state *pGen,ph7_expr_node *pCall,SyString *pName)
{
	ph7_expr_node **apArg;
	sxu32 nArg, n;
	sxu32 i;
	if( pName->nByte < 1 ){
		return 0;
	}
	apArg = (ph7_expr_node **)SySetBasePtr(&pCall->aNodeArgs);
	nArg = SySetUsed(&pCall->aNodeArgs);
	for( i = 0 ; i < SX_ARRAYSIZE(aSpecialFunc) ; ++i ){
		SyString sEntry;
		SyStringInitFromBuf(&sEntry,aSpecialFunc[i].zName,SyStrlen(aSpecialFunc[i].zName));
		if( sEntry.nByte != pName->nByte
		 || SyStrnicmp(sEntry.zString,pName->zString,pName->nByte) != 0 ){
			continue;
		}
		if( (int)nArg < aSpecialFunc[i].nMinArg
		 || (aSpecialFunc[i].nMaxArg >= 0 && (int)nArg > aSpecialFunc[i].nMaxArg) ){
			return 0;
		}
		/* php bails out of the whole table when any argument unpacks or is named
		 * (`zend_args_contain_unpack_or_named`), so `strlen(...$a)[0] = 1` RUNS. */
		if( (aSpecialFunc[i].iFlags & SPECFN_ANY_ARGS) == 0 ){
			for( n = 0 ; n < nArg ; ++n ){
				if( apArg[n] && (apArg[n]->iFlags & (EXPR_NODE_SPREAD|EXPR_NODE_NAMED_ARG)) ){
					return 0;
				}
			}
		}
		if( (aSpecialFunc[i].iFlags & SPECFN_IN_FUNC) && !GenStateInFunction(pGen) ){
			return 0;
		}
		/* `defined` and `sprintf` specialize only over a LITERAL first argument;
		 * php gives up on a computed one and emits an ordinary call. */
		if( aSpecialFunc[i].iFlags & SPECFN_LITERAL_ARG0 ){
			if( nArg < 1 || apArg[0] == 0 || apArg[0]->pOp != 0
			 || apArg[0]->pStart == 0
			 || (apArg[0]->pStart->nType & (PH7_TK_SSTR|PH7_TK_DSTR)) == 0 ){
				return 0;
			}
		}
		if( (aSpecialFunc[i].iFlags & SPECFN_FORMAT_ARG0) && nArg >= 1 && apArg[0] ){
			/* php's own sprintf gate, and it is arithmetic: a format under 256
			 * bytes carrying nothing but `%s`, `%d` and `%%`, with exactly one
			 * VALUE per placeholder. `sprintf("a","b")` fails it (no placeholder,
			 * one value) and compiles to an ordinary call, which is why the write
			 * through it RUNS. */
			const SyString *pFmt = &apArg[0]->pStart->sData;
			sxu32 nPlace = 0, k;
			if( pFmt->nByte >= 256 ){
				return 0;
			}
			/* An escape or an interpolation makes php's argument something other
			 * than a plain literal; leave those to the ordinary call. */
			for( k = 0 ; k < pFmt->nByte ; ++k ){
				if( pFmt->zString[k] == '\\'
				 || ((apArg[0]->pStart->nType & PH7_TK_DSTR)
				  && (pFmt->zString[k] == '$' || pFmt->zString[k] == '{')) ){
					return 0;
				}
			}
			for( k = 0 ; k < pFmt->nByte ; ++k ){
				if( pFmt->zString[k] != '%' ){
					continue;
				}
				if( k + 1 >= pFmt->nByte ){
					return 0; /* a trailing '%' */
				}
				k++;
				if( pFmt->zString[k] == 's' || pFmt->zString[k] == 'd' ){
					nPlace++;
				}else if( pFmt->zString[k] != '%' ){
					return 0; /* any other conversion */
				}
			}
			if( nPlace != nArg - 1 ){
				return 0;
			}
		}
		return 1;
	}
	return 0;
}
/*
 * The two write-target rules php decides at COMPILE time, in one place because
 * every write site has to make both of them.
 *
 * **`$this`** is not a variable a program may re-point: php refuses the
 * assignment, the reference bind, a foreach/list target and `unset()` where they
 * are WRITTEN. PHL performed all of them, so `$this = 5;` inside a method
 * replaced the receiver with an int for the rest of the call and every later
 * `$this->x` failed somewhere else entirely.
 *
 * **A temporary** cannot be written THROUGH: `(new A)->p = 1` and `"s"->p = 1`
 * modify an object/value that no longer exists after the statement, so php
 * refuses the whole chain — every write kind, including `+=`, `++`, `=&` and
 * `unset()`. The base of the access chain decides: a variable and a userland
 * CALL are writable (`f()->p = 1` is php-legal), a `new`, a literal and any
 * other computed value are not, and an internal function's result gets php's own
 * separate wording — which is what `(clone $o)->p = 1` is, `clone` being a
 * function in php 8.5.
 *
 * **A call is writable THROUGH but not writable INTO.** The distinction is
 * php's, and it is made in two different places: `zend_compile_var_inner` lets
 * a call be the base of a chain, while `zend_ensure_writable_variable` refuses
 * the call when it is the target ITSELF, with a wording that says which kind of
 * call it was. So `f()[0] = 5` compiles and `f() = 5` does not. The one write
 * site that does not ask the second question is the SOURCE of `=&`, which is
 * why `$r =& f()` is a runtime notice rather than a compile error.
 */
PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int iCtx)
{
	ph7_expr_node *pBase = pTarget;
	const char *zMsg = 0;
	sxi32 rc;
	if( pTarget == 0 ){
		return SXRET_OK;
	}
	if( PH7_ExprNodeIsThis(pTarget) && (iCtx & (PH7_WTC_REFSRC|PH7_WTC_RMW)) == 0 ){
		/* Only as the TARGET. php refuses `$this = …`, `$this =& …`, a
		 * foreach/list target and `unset($this)` -- but the SOURCE of a `=&` is
		 * compiled in write context WITHOUT zend_ensure_writable_variable, and
		 * that is the function that holds the $this rule. So `$t =& $this` binds
		 * the receiver, and so do `$a[] =& $this`, `$this->p =& $this` and
		 * `self::$s =& $this`; a $this that has no object behind it is the
		 * ordinary RUNTIME "Using $this when not in object context". Refusing the
		 * source here cost react/promise's `$target =& $this` -- Composer's whole
		 * async download layer.
		 *
		 * And only for an ASSIGNMENT. php makes this rule in the assignment
		 * compiler, so a READ-MODIFY-WRITE (`$this += 1`, `$this .= "x"`,
		 * `$this++`) compiles and raises the ordinary operand error at run time
		 * (`Unsupported operand types: C + int`, `Cannot increment C`) -- which
		 * this engine already words for any other object. */
		zMsg = (iCtx & PH7_WTC_UNSET) ? "Cannot unset $this" : "Cannot re-assign $this";
	}else if( pTarget->pOp && pTarget->pOp->iOp == EXPR_OP_FUNC_CALL
	       && (iCtx & PH7_WTC_REFSRC) == 0 ){
		/* The target is the call itself (`f() = 5`, `f()++`, `unset(f())`,
		 * `foreach (… as f())`). php names the kind of call: a METHOD callee —
		 * `$o->m()`, `C::m()` — reports "method", everything else "function".
		 * A PARENTHESISED member callee is php's variable-invocation
		 * (`($o->p)()` calls the property's VALUE), which is an ordinary
		 * function call, exactly the distinction the OP_CALL codegen makes. */
		int bMethod = pTarget->pLeft
			&& pTarget->pLeft->pOp
			&& (pTarget->pLeft->pOp->iOp == EXPR_OP_ARROW
			 || pTarget->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW
			 || pTarget->pLeft->pOp->iOp == EXPR_OP_DC)
			&& (pTarget->pLeft->iFlags & EXPR_NODE_PARENS) == 0;
		zMsg = bMethod
			? "Can't use method return value in write context"
			: "Can't use function return value in write context";
	}else if( PH7_ExprContainsNullsafe(pTarget) ){
		/* php asks this AFTER the call question (`$o?->m()++` is a method return
		 * value, not a nullsafe chain) and BEFORE the base one (`(new A)?->p = 1`
		 * is the nullsafe refusal, not the temporary). A reference SOURCE has its
		 * own sentence for it. The `=`/`+=`/`unset()`/foreach paths screened this
		 * themselves; `++`/`--`, `??=` and `array(&…)` did not, so `$o?->p++` ran. */
		zMsg = (iCtx & PH7_WTC_REFSRC)
			? "Cannot take reference of a nullsafe chain"
			: "Can't use nullsafe operator in write context";
	}else{
		/* Walk to the base of the access chain; the links themselves are writable. */
		while( pBase && pBase->pOp ){
			if( pBase->pOp->iOp == EXPR_OP_DC && !PH7_ExprNodeIsClassConst(pBase) ){
				/* A `::` left operand is a CLASS reference, not a value — `C::$s = 1`
				 * and even `(new C)::$s = 1` write class-level storage that outlives
				 * any temporary, so the chain stops being about a base here. A `::`
				 * naming a CONSTANT is not storage, though: `A::K[0] = 5` subscripts
				 * a COPY, so it falls through to the computed-base verdict below —
				 * php's "Cannot use temporary expression in write context", where
				 * PHL wrote into the copy and answered nothing. */
				return SXRET_OK;
			}
			if( pBase->pOp->iOp != EXPR_OP_ARROW && pBase->pOp->iOp != EXPR_OP_NULLSAFE_ARROW
			 && pBase->pOp->iOp != EXPR_OP_SUBSCRIPT ){
				break;
			}
			pBase = pBase->pLeft;
		}
		if( pBase == 0 || pBase == pTarget ){
			/* No chain: a non-variable target of its own is the caller's business
			 * (php reports its parse error / "Assignments can only happen to
			 * writable values" there, and so does PHL). */
			return SXRET_OK;
		}
		if( pBase->pOp == 0 ){
			if( pBase->xCode != PH7_CompileVariable ){
				zMsg = "Cannot use temporary expression in write context";
			}
		}else if( pBase->pOp->iOp == EXPR_OP_FUNC_CALL ){
			/* php refuses a write through the result of a call it SPECIALIZED into
			 * an opcode — see aSpecialFunc above. The old test asked whether the
			 * name was a host function AT ALL, which would have refused every
			 * builtin (php specializes 28 of them), and asked it of the CALL node
			 * where GenStateCallBuiltinName wants the CALLEE node — so it never
			 * matched anything and `clone` below was the only arm that ever fired.
			 * The name table IS the resolution here: php looks the callee up in a
			 * function table that is fully populated at compile time, and PHL's is
			 * not — the ~650 core builtins register in PH7_VmMakeReady, which runs
			 * AFTER compilation (see the redeclaration guard near the top of this
			 * file), so hHostFunction has no `strlen` to find. */
			SyString sName;
			GenStateCallBuiltinName(pBase->pLeft,&sName);
			if( GenStateCallIsSpecialized(&(*pGen),pBase,&sName) ){
				zMsg = "Cannot use result of built-in function in write context";
			}
		}else if( pBase->pOp->iOp == EXPR_OP_CLONE ){
			/* php 8.5 implements `clone` AS a function, so a write through its result
			 * takes the internal-function wording rather than the temporary one. */
			zMsg = "Cannot use result of built-in function in write context";
		}else{
			/* `new`, and every other computed base. */
			zMsg = "Cannot use temporary expression in write context";
		}
	}
	if( zMsg == 0 ){
		return SXRET_OK;
	}
	rc = PH7_GenCompileError(&(*pGen),E_ERROR,
		pTarget->pStart ? pTarget->pStart->nLine : 0,"%s",zMsg);
	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_INVALID;
}
/*
 * What emitting a call's ARGUMENT LIST decided, handed back to the CALL codegen.
 * The arguments are emitted from their own routine because php evaluates them
 * AFTER the callee has been resolved, so this runs between the callee's emission
 * and the OP_CALL — see GenStateEmitCallArgs.
 */
typedef struct GenCallArgs GenCallArgs;
struct GenCallArgs {
	sxi32 iP1;      /* OP_CALL.iP1: the compile-time argument count */
	sxu32 iP2;      /* OP_CALL.iP2: 1 if any argument unpacks (`...$a`) */
	void *p3;       /* OP_CALL.p3: the VmCallArgMap, which may ALREADY carry the callee's
	                 * namespace qualification — the callee is emitted first now */
	int bFcc;       /* First-class callable `f(...)`: no arguments, OP_LOAD_FCC follows */
	int bAnySpread; /* Any `...` argument (iP2 says the same; kept for the shape masks) */
};
static sxi32 GenStateEmitCallArgs(ph7_gen_state *pGen,ph7_expr_node *pNode,sxi32 iFlags,
	GenCallArgs *pArgs);
/*
 * TRUE when the `instanceof` SUBJECT that just compiled into the instruction
 * stream starting at nFirst is what zend calls IS_CONST -- the shape whose
 * whole expression its compiler folds to FALSE, without ever compiling the
 * class operand.
 *
 * php decides this from its own constant FOLDER: `zend_compile_expr` on the
 * subject comes back IS_CONST for a literal, for `null`/`true`/`false`, for an
 * engine constant, for an array literal, and for arithmetic or concatenation
 * over any of those -- and `5 instanceof $x` is then false whatever $x holds,
 * while `$v instanceof $x` with the same 5 in $v reaches the runtime opcode and
 * is refused when $x is neither an object nor a string. The two spellings really
 * do answer differently, so OP_IS_A's screen has to be told which one it is.
 *
 * PHL has no constant folder, so the question is asked of the INSTRUCTIONS the
 * subject compiled to: a run built only from LITERAL loads and pure value
 * operators is a constant expression, and anything that reads a variable, names
 * a constant, calls something or touches an object is not. php's own folder
 * reaches two shapes further -- an ENGINE constant (`PHP_EOL`) and a builtin
 * call it ct-evaluates (`strlen("a")`) -- where php answers false and this
 * refuses; PLAN.md §7.2 records the pair under the constant-folding family.
 */
static int GenStateInstanceofFoldsLhs(ph7_gen_state *pGen,sxu32 nFirst)
{
	sxu32 n;
	sxu32 nLen = PH7_VmInstrLength(pGen->pVm);
	for( n = nFirst ; n < nLen ; ++n ){
		VmInstr *pIn = PH7_VmGetInstr(pGen->pVm,n);
		if( pIn == 0 ){
			return 0;
		}
		switch( pIn->iOp ){
		case PH7_OP_LOADC:
			/* A LOADC that still carries EXPAND is a NAME the runtime resolves --
			 * a constant -- and that is exactly what php does NOT fold: its
			 * compiler substitutes only the engine's own persistent constants, so
			 * a userland `const OBJ = new C();` reaches the runtime opcode and
			 * `OBJ instanceof C` is a real question. Folding it answered FALSE for
			 * every constant that holds an object. */
			if( pIn->iP1 & PH7_LOADC_EXPAND ){
				return 0;
			}
			break;
		case PH7_OP_LOAD_MAP:
		case PH7_OP_CAT:
		case PH7_OP_CVT_INT: case PH7_OP_CVT_STR: case PH7_OP_CVT_REAL:
		case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC: case PH7_OP_CVT_NULL:
		case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT: case PH7_OP_LNOT:
		case PH7_OP_MUL: case PH7_OP_DIV: case PH7_OP_MOD: case PH7_OP_POW:
		case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_SHL: case PH7_OP_SHR:
		case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:
		case PH7_OP_SPACESHIP: case PH7_OP_EQ: case PH7_OP_NEQ:
		case PH7_OP_TEQ: case PH7_OP_TNE:
		case PH7_OP_BAND: case PH7_OP_BXOR: case PH7_OP_BOR:
			break;
		default:
			return 0;
		}
	}
	return nLen > nFirst;
}
/*
 * Generate bytecode for a given expression tree.
 * If something goes wrong while generating bytecode
 * for the expression tree (A very unlikely scenario)
 * this function takes care of generating the appropriate
 * error message.
 */
/*
 * php's `zend_is_variable_or_call`: what may sit on the right of a destructuring
 * assignment whose target list binds BY REFERENCE. A variable, a property, a
 * static property, a subscript and a CALL can each hand a slot over; an array
 * literal, a string, `new`, and any computed value cannot, and php refuses those
 * at compile time rather than binding to a temporary.
 */
static int GenStateNodeIsRefSource(ph7_expr_node *pNode)
{
	if( pNode == 0 ){
		return 0;
	}
	if( pNode->pOp ){
		return pNode->pOp->iOp == EXPR_OP_SUBSCRIPT
		    || pNode->pOp->iOp == EXPR_OP_ARROW
		    || pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW
		    || pNode->pOp->iOp == EXPR_OP_DC
		    || pNode->pOp->iOp == EXPR_OP_FUNC_CALL;
	}
	return pNode->xCode == PH7_CompileVariable;
}
/*
 * Is this call node's callee the `isset` KEYWORD itself?
 *
 * isset() is a language construct, not a name: it reaches the call path as a
 * keyword token whose literal the compiler canonicalizes to "isset" (see
 * compile_node.c). A keyword used as a MEMBER name is excluded here for the same
 * reason it is excluded there — `$o->isset(...)` names a method — and so is any
 * callee that is an operator node rather than a bare literal.
 */
static int GenStateCalleeIsIsset(ph7_expr_node *pCallee)
{
	SyString *pName;
	if( pCallee == 0 || pCallee->pOp != 0 || pCallee->pStart == 0 ){
		return 0;
	}
	if( (pCallee->pStart->nType & PH7_TK_KEYWORD) == 0
	 || (pCallee->pStart->nType & PH7_TK_MEMBER_NAME) ){
		return 0;
	}
	pName = &pCallee->pStart->sData;
	return pName->nByte == sizeof("isset")-1
		&& SyStrnicmp(pName->zString,"isset",sizeof("isset")-1) == 0;
}
static sxi32 GenStateEmitExprCode(
	ph7_gen_state *pGen,  /* Code generator state */
	ph7_expr_node *pNode, /* Root of the expression tree */
	sxi32 iFlags /* Control flags */
	)
{
	VmInstr *pInstr;
	sxu32 nJmpIdx;
	sxi32 iP1 = 0;
	sxu32 iP2 = 0;
	void *p3  = 0;
	sxi32 iVmOp;
	sxi32 rc;
	int bIsChainOp = 0; /* Set below once we know pNode->pOp */
	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */
	sxu32 nRhsNsBase = 0;
	sxi32 iRhsFlags = 0; /* control flags the RIGHT operand is compiled under */
	sxu32 nLhsFirst = 0; /* instruction index the LEFT operand starts at */
	/* Consumed here so it describes THIS node only — the direct operand of a `new` —
	 * and never travels down into the operand's own sub-expressions. */
	int bNewCallee = (iFlags & EXPR_FLAG_NEW_CALLEE) != 0;
	iFlags &= ~EXPR_FLAG_NEW_CALLEE;
	if( pNode->xCode ){
		SyToken *pTmpIn,*pTmpEnd;
		/* Compile node */
		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);
		rc = pNode->xCode(&(*pGen),iFlags);
		RE_SWAP_DELIMITER(pGen);
		return rc;
	}
	if( pNode->pOp == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,
			"Invalid expression node,PH7 is aborting compilation");
		return SXERR_ABORT;
	}
	iVmOp = pNode->pOp->iVmOp;
	if( iVmOp == PH7_OP_STORE_REF && pNode->pLeft && PH7_ExprNodeIsThis(pNode->pLeft) ){
		/* `$t =& $this` is a VALUE assignment. php's `$this` is not a slot a
		 * reference can name -- the receiver lives in the frame's own field, not
		 * in a variable -- so the bind quietly degrades to a copy of the object
		 * HANDLE: `$t` gets a slot of its own, `$t->v = 9` still reaches the same
		 * object (that is identity, not reference), and `$t = 5` leaves `$this`
		 * an object. Binding the slot instead let a write through the alias
		 * REPLACE the receiver for the rest of the call. The operands were
		 * swapped in parse.c, so the source is pLeft. */
		iVmOp = PH7_OP_STORE;
	}
	if( iVmOp == PH7_OP_CVT_NULL ){
		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the
		 * whole compile); keep emitting so expression codegen stays aligned
		 * and later errors are still reported. */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,
			"The (unset) cast is no longer supported");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){
		sxu32 nJmp = 0;
		sxu32 nNcNsBase;
		VmInstr *pInstrFix;
		/* Null coalescing assignment requires a custom compile order: the LHS
		 * target (pRight for prec-18 right-assoc ops) must be evaluated first
		 * so we can short-circuit the RHS when LHS is non-null. Pass
		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the
		 * stack slot carries a writable nIdx. */
		if( pNode->pRight ){
			/* `$a[] ??= v` READS its target before deciding to write, so php refuses the
			 * append form anywhere in that target's chain (`$a[][0] ??= v` too), even
			 * though the same tag makes every other `[]` on this path a legal write
			 * target. Only the container chain is walked — a `[]` inside an INDEX
			 * expression is an ordinary read and the subscript codegen refuses it. */
			ph7_expr_node *pTgt = pNode->pRight;
			while( pTgt && pTgt->pOp && (pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT
			      || pTgt->pOp->iOp == EXPR_OP_ARROW || pTgt->pOp->iOp == EXPR_OP_DC) ){
				if( pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT && SySetUsed(&pTgt->aNodeArgs) < 1 ){
					break;
				}
				pTgt = pTgt->pLeft;
			}
			if( pTgt && pTgt->pOp && pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT
			 && SySetUsed(&pTgt->aNodeArgs) < 1 ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,
					pNode->pRight->pStart ? pNode->pRight->pStart->nLine : 0,
					"Cannot use [] for reading");
				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;
			}
			/* …and only THEN the write-target rules, php's order: `strval(1)[] ??= 3`
			 * is the append refusal, not the specialized-builtin one. `??=` compiles
			 * its own way and so never reached this check at all, which is why
			 * `(new A)->p ??= 3` and `"lit"->p->q ??= 3` used to run. */
			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,0);
			if( rc != SXRET_OK ){
				return rc;
			}
			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);
			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags|EXPR_FLAG_LOAD_IDX_STORE|EXPR_FLAG_MEMBER_WRITE);
			if( rc != SXRET_OK ){
				return rc;
			}
			GenStatePatchNullsafeJumps(pGen, nNcNsBase);
			/* Optimisation: if the outermost LHS access is a subscript, demote
			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +
			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On
			 * the common "already set" path the upcoming NULLC_JMP will skip
			 * the store, so the parent array does not need to be copied at
			 * all. Inner levels of a nested LHS keep iP2=1 so the separation
			 * cascade for the actual write path stays correct. */
			pInstrFix = PH7_VmPeekInstr(pGen->pVm);
			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){
				pInstrFix->iP2 = 3;
			}
		}
		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);
		/* Compile the RHS value (pLeft for prec-18 right-assoc). */
		if( pNode->pLeft ){
			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);
			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);
			if( rc != SXRET_OK ){
				return rc;
			}
			GenStatePatchNullsafeJumps(pGen, nNcNsBase);
		}
		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);
		/* Patch the short-circuit jump to land after the store. */
		if( nJmp > 0 ){
			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);
			if( pInstrFix ){
				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);
			}
		}
		return SXRET_OK;
	}
	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){
		sxu32 nJz,nJmp;
		sxu32 nTernaryNsBase;
		/* Ternary operator require special handling */
		/* Phase#1: Compile the condition */
		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);
		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Ternary is not a chain operator: any nullsafe jumps emitted while
		 * compiling the condition must short-circuit to the end of the
		 * condition expression, not leak past the ternary. */
		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);
		nJz = nJmp = 0; /* cc -O6 warning */
		if( pNode->pLeft ){
			/* Standard ternary: (expr) ? (then) : (else) */
			/* Phase#2: Emit the false jump (pops condition) */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);
			/* Phase#3: Compile the 'then' expression  */
			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);
			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);
			if( rc != SXRET_OK ){
				return rc;
			}
			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);
		}else{
			/* Elvis operator: (expr) ?: (else)
			 * Duplicate condition so original value is the 'then' result.
			 * JZ consumes the copy; original stays on stack if truthy. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);
		}
		/* Phase#4: Emit the unconditional jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);
		/* Phase#5: Fix the false jump now the jump destination is resolved. */
		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);
		if( pInstr ){
			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);
		}
		if( !pNode->pLeft ){
			/* Elvis operator: discard the falsy condition value before evaluating 'else' */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
		/* Phase#6: Compile the 'else' expression */
		if( pNode->pRight ){
			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);
			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);
			if( rc != SXRET_OK ){
				return rc;
			}
			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);
		}
		if( nJmp > 0 ){
			/* Phase#7: Fix the unconditional jump */
			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);
			if( pInstr ){
				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);
			}
		}
		/* All done */
		return SXRET_OK;
	}
	if( pNode->pOp->iOp == EXPR_OP_PIPE ){
		/* PHP 8.5 pipe: `$lhs |> $rhs` invokes the RHS callable with the LHS
		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the
		 * argument) first, then the RHS callable, then emit a one-argument
		 * OP_CALL — the same stack shape the function-call path builds (the
		 * argument sits below the callee). The RHS is any callable expression:
		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an
		 * `[obj,method]` pair, or a callable string. */
		sxu32 nPipeNsBase;
		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE|EXPR_FLAG_MEMBER_WRITE
			|EXPR_FLAG_MEMBER_REFSRC|EXPR_FLAG_RDONLY_LOAD);
		if( pNode->pLeft == 0 || pNode->pRight == 0 ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,
				"'|>': Missing operand");
			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;
		}
		/* Argument: the LHS value. */
		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);
		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);
		if( rc != SXRET_OK ){
			return rc;
		}
		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);
		/* Callable: the RHS. */
		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);
		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);
		if( rc != SXRET_OK ){
			return rc;
		}
		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);
		/* Invoke the callable with the single piped argument. */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);
		return SXRET_OK;
	}
	/*
	 * php compiles the multi-operand `isset($a, $b, ...)` as a short-circuit CHAIN --
	 * `isset($a) && isset($b) && ...` -- so nothing after the first operand that is not
	 * set is ever evaluated. isset() is a host function here, and a call evaluates every
	 * argument before dispatching, so the later operands ran for real: the ordinary
	 * `isset($info['k'], $data[$info['k']])` answered false through an `Undefined array
	 * key` warning and a null-offset deprecation php never raises, and
	 * `isset($a['no'], $b[side()])` CALLED side(). Doctrine's hydrator guards its
	 * discriminator lookup in exactly that shape, so every hydrated row of every query
	 * carried two diagnostics php does not.
	 *
	 * Emit the chain the compiler owes: one SINGLE-operand isset() per argument, joined
	 * by a keep-the-value JZ to the end (the false it left IS the answer) and a POP on
	 * the fall-through. Each link is the ordinary call path below, re-entered with the
	 * argument set narrowed to one node, so every operand keeps the exact isset context
	 * it already had -- LOAD_IDX iP2=4, the quiet intermediates of an access chain,
	 * ArrayAccess::offsetExists -- and only the ORDER changes. A spread or named operand
	 * opts out: php refuses both in this position, and neither maps to one link.
	 */
	if( iVmOp == PH7_OP_CALL && SySetUsed(&pNode->aNodeArgs) > 1
	 && GenStateCalleeIsIsset(pNode->pLeft) ){
		ph7_expr_node **apIsset = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);
		sxu32 nIsset = SySetUsed(&pNode->aNodeArgs);
		SySet sSaved = pNode->aNodeArgs;
		SySet sJz;
		sxu32 n;
		int bPlain = 1;
		for( n = 0 ; n < nIsset ; ++n ){
			if( apIsset[n] == 0
			 || (apIsset[n]->iFlags & (EXPR_NODE_SPREAD|EXPR_NODE_NAMED_ARG)) ){
				bPlain = 0;
				break;
			}
		}
		if( bPlain ){
			SySetInit(&sJz,&pGen->pVm->sAllocator,sizeof(sxu32));
			rc = SXRET_OK;
			for( n = 0 ; n < nIsset ; ++n ){
				pNode->aNodeArgs.pBase = (void *)&apIsset[n];
				pNode->aNodeArgs.nUsed = 1;
				pNode->aNodeArgs.nSize = 1;
				pNode->aNodeArgs.nCursor = 0;
				rc = GenStateEmitExprCode(&(*pGen),pNode,iFlags);
				pNode->aNodeArgs = sSaved;
				if( rc != SXRET_OK ){
					break;
				}
				if( n + 1 < nIsset ){
					sxu32 nJz = 0;
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,
						1 /* keep the false on the stack: it is the answer */,0,0,&nJz);
					SySetPut(&sJz,(const void *)&nJz);
					/* Truthy link: drop it and ask the next operand. */
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
				}
			}
			if( rc == SXRET_OK ){
				sxu32 *aJz = (sxu32 *)SySetBasePtr(&sJz);
				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);
				for( n = 0 ; n < SySetUsed(&sJz) ; ++n ){
					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,aJz[n]);
					if( pFix ){
						pFix->iP2 = nEnd;
					}
				}
			}
			SySetRelease(&sJz);
			return rc;
		}
	}
	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);
	nLhsFirst = PH7_VmInstrLength(pGen->pVm);
	/* Generate code for the left tree */
	if( pNode->pLeft ){
		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);
		GenCallArgs sArgs;
		int bArgsEmitted = 0;
		sxu32 nNewClassInstr = 0; /* index+1 of a `new` operand's class-name push */
		SyZero(&sArgs,sizeof(sArgs));
		{
			/* The unset() target is the OUTERMOST access. When the intermediate container — the left
			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /
			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is
			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.
			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to
			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the
			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate
			 * in `isset($o->a->b)`, which the suppression modes mirror. */
			sxi32 iLeftFlags = iFlags;
			/* The LHS chain whose subscript reads must be QUIET (LOAD_IDX iP2=8):
			 * `??`'s left operand, and an isset()/empty() chain's intermediate
			 * links -- php reads both silently and for the value. */
			sxu32 nQuietLhsFirst = PH7_VmInstrLength(pGen->pVm);
			int bQuietLhs = 0;
			/* D1 commit 2: a deferred element/property call arg records its lvalue chain, but
			 * that chain must be CONTIGUOUS. Only propagate DEFER_ARG to the base when the base
			 * is itself a continuable lvalue — a plain variable (an undefined base auto-defers),
			 * another subscript, or a `->` member. If the base is anything else (most importantly
			 * a method CALL, e.g. `$o->items()->prop` or `$r->attributes->item(0)->nodeName`),
			 * strip DEFER so that intermediate read is a NORMAL read, not a record-mode carrier. */
			if( iLeftFlags & EXPR_FLAG_DEFER_ARG ){
				int bContinuable = pNode->pLeft
					&& ( (pNode->pLeft->pOp == 0 && pNode->pLeft->xCode == PH7_CompileVariable)
					  || (pNode->pLeft->pOp != 0 && (pNode->pLeft->pOp->iOp == EXPR_OP_SUBSCRIPT
					                              || pNode->pLeft->pOp->iOp == EXPR_OP_ARROW)) );
				if( !bContinuable ){
					iLeftFlags &= ~EXPR_FLAG_DEFER_ARG;
				}
			}
			/*
			 * An isset()/empty() CHAIN reads its intermediate links for their
			 * VALUE, not for a truth. php walks `isset($o->a->b)` by fetching
			 * `$o->a` in BP_VAR_IS mode -- silent, but a real read that runs
			 * __isset AND THEN __get (or offsetExists and then offsetGet) --
			 * and only the LAST link answers the isset question. PHL gave every
			 * link the terminal context, so the intermediate pushed a bool and
			 * the final `->b` was a property of `true`: `isset($model->rel->id)`
			 * was FALSE for every class with accessors, and so was
			 * `isset($container['k']['j'])` over ArrayAccess -- a silently wrong
			 * guard, not a diagnostic.
			 *
			 * The intermediate context is `??`'s (PH7_MEMBER_COALESCE for a
			 * member, LOAD_IDX iP2=8 for a subscript, patched over the emitted
			 * range below), which is exactly "silent, and the value": EMPTY's
			 * would read a shade differently, since a class declaring __get with
			 * no __isset is read through __get for an intermediate link and is
			 * NOT for a terminal isset()/empty().
			 */
			if( (iLeftFlags & (EXPR_FLAG_LOAD_IDX_ISSET|EXPR_FLAG_LOAD_IDX_EMPTY))
				&& pNode->pOp && pNode->pLeft && pNode->pLeft->pOp
				&& GEN_IS_ACCESS_OP(pNode->pOp->iOp)
				&& GEN_IS_ACCESS_OP(pNode->pLeft->pOp->iOp) ){
				iLeftFlags &= ~(EXPR_FLAG_LOAD_IDX_ISSET|EXPR_FLAG_LOAD_IDX_EMPTY);
				iLeftFlags |= EXPR_FLAG_MEMBER_COALESCE|EXPR_FLAG_QUIET_VAR;
				bQuietLhs = 1;
			}
			if( pNode->pLeft && pNode->pLeft->pOp
				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW
					|| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW
					|| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){
				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;
			}else if( iLeftFlags & EXPR_FLAG_LOAD_IDX_UNSET ){
				/* A SUBSCRIPT intermediate of an unset chain (`unset($a['k']['n'])`) keeps
				 * the unset context — it must COW-separate the parent and must NOT vivify a
				 * missing key — but it is a READ of the container, not an unset of it. The
				 * UNSET_BASE context says exactly that: `$a['k']` is loaded, where the
				 * plain unset context would have removed the ELEMENT (and, for an
				 * ArrayAccess base, called offsetUnset() on the intermediate key). */
				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;
				iLeftFlags |= EXPR_FLAG_LOAD_IDX_UNSET_BASE;
			}
			/* Only the OUTERMOST access of a reference SOURCE is the reference fetch;
			 * every container under it is php's ordinary write base (`$r =& $o->arr['k']`
			 * creates `arr` the way `$o->arr['k'] = v` does). So the flag never travels
			 * down as itself -- it decays to the write-lvalue flag, which the strip just
			 * below then applies its own `->`-intermediate rule to. */
			if( iLeftFlags & EXPR_FLAG_MEMBER_REFSRC ){
				iLeftFlags &= ~EXPR_FLAG_MEMBER_REFSRC;
				iLeftFlags |= EXPR_FLAG_MEMBER_WRITE;
			}
			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the
			 * write target of an assignment and flows through a SUBSCRIPT to its base member
			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its
			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create
			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never
			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=`
			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */
			if( pNode->pOp
				&& (pNode->pOp->iOp == EXPR_OP_ARROW
					|| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW
					|| pNode->pOp->iOp == EXPR_OP_DC) ){
				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;
			}
			/* `++`/`--` mutate their operand in place — the operand is a write
			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the
			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked
			 * properties throw php's Indirect-modification Error, missing ones
			 * auto-vivify). The prec-18 site below handles the assign family;
			 * `++`/`--` are unary, their operand is pLeft. */
			if( pNode->pOp
				&& (pNode->pOp->iVmOp == PH7_OP_INCR || pNode->pOp->iVmOp == PH7_OP_DECR) ){
				/* `(new A)->p++` writes through a temporary exactly as `= 1` does --
				 * but a `$this++` is a read-modify-write php leaves to run time. */
				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,PH7_WTC_RMW);
				if( rc != SXRET_OK ){
					return rc;
				}
				iLeftFlags |= EXPR_FLAG_LOAD_IDX_STORE | EXPR_FLAG_MEMBER_WRITE
					| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;
			}
			/* The SOURCE of a `=&` (pLeft, the operands having been swapped in
			 * parse.c) is compiled in WRITE context by php too —
			 * `zend_compile_var(source, BP_VAR_W, 1)` — which is what makes
			 * `$r =& $undef` and `$r =& $a[5]` CREATE the thing they bind to,
			 * silently. PHL READ it, so both warned about what was missing and then
			 * refused the bind outright, leaving $r undefined as well. No
			 * RMW_LOAD: a bind does not read the source's value, and no
			 * MEMBER_WRITE: a handler-backed native property has no pointer for
			 * php to hand out either, so `$r =& $iv->s` must keep taking the
			 * read COPY it takes in php. */
			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_REF ){
				iLeftFlags |= EXPR_FLAG_LOAD_IDX_STORE | EXPR_FLAG_MEMBER_REFSRC;
			}
			/* A destructuring target list that binds BY REFERENCE reads its SOURCE
			 * in write context for the same reason: the bind must reach the thing
			 * the source NAMES. That is what keeps `[&$t] = $undef;` from warning
			 * about what it is on the point of creating. */
			if( iVmOp == PH7_OP_STORE && pNode->pRight && pNode->pRight->pStart
			 && (pNode->pRight->xCode == PH7_CompileList
			  || pNode->pRight->xCode == PH7_CompileShortList)
			 && PH7_GenStateListSpanHasRef(pNode->pRight->pStart,pNode->pRight->pEnd) ){
				iLeftFlags |= EXPR_FLAG_LOAD_IDX_STORE | EXPR_FLAG_MEMBER_REFSRC;
			}
			/* `??` reads its LEFT operand in isset-context: an undefined or
			 * UNINITIALIZED typed PROPERTY must yield the default rather than a
			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes
			 * the silent-lookup path (iP2 = ISSET), which still loads a present
			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means
			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —
			 * that path is already handled correctly by OP_NULLC. */
			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){
				/* php reads the ENTIRE left operand of `??` in isset-context: no
				 * "Undefined variable" for `$x ?? d` OR for the base of
				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it
				 * sits in the chain. */
				iLeftFlags |= EXPR_FLAG_QUIET_VAR;
				bQuietLhs = 1;
				if( pNode->pLeft->pOp
					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW
						|| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW
						|| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){
					/* A member-access LHS additionally takes OP_MEMBER's SILENT
					 * lookup so an uninitialized typed property yields the default
					 * instead of an Error. It used to borrow isset()'s context for
					 * that, which is the same mistake the comment below records for
					 * subscripts: silence is shared, but isset() context makes every
					 * ACCESSOR answer a truth, and `$o->p ?? d` needs the accessor's
					 * VALUE — so `??` has its own member context. A SUBSCRIPT LHS
					 * still takes neither: LOAD_IDX's ISSET mode means offsetExists
					 * (a bool), while `$o[$k] ?? d` needs the offsetGet value —
					 * OP_NULLC already handles that path. */
					iLeftFlags |= EXPR_FLAG_MEMBER_COALESCE;
				}
			}
			if( iVmOp == PH7_OP_ERR_CTRL ){
				/* '@' must suppress the diagnostics raised WHILE its operand runs, so
				 * open the window here; the trailing emit below closes it (iP1 = 0). */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);
			}
			if( iVmOp == PH7_OP_NEW ){
				/* Mark the direct operand so a call node under it (`new C($a)`) keeps the
				 * emission order OP_NEW is assembled from — see the bNewCallee branch above. */
				iLeftFlags |= EXPR_FLAG_NEW_CALLEE;
			}
			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags|EXPR_FLAG_RDONLY_LOAD);
			if( rc == SXRET_OK && bQuietLhs ){
				/* Mark EVERY subscript read in the quiet left chain (iP2=8).
				 * Peeking at the next instruction only catches the OUTERMOST
				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;
				 * and a forward scan would be unsound, since an unrelated sibling
				 * access can sit immediately before a coalesce (`f($a['k'],
				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks
				 * the range it just emitted. Write-context codes (1/3/5) belong to
				 * `??=` and keep their meaning. */
				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);
				sxu32 nAt;
				for( nAt = nQuietLhsFirst ; nAt < nEnd ; ++nAt ){
					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);
					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){
						pFix->iP2 = 8;
					}
				}
			}
		}
		if( rc != SXRET_OK ){
			return rc;
		}
		if( !bIsChainOp ){
			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree
			 * target the end of that LHS chain, which is right here. */
			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);
		}
		if( iVmOp == PH7_OP_CALL ){
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr ){
				if ( pInstr->iOp == PH7_OP_LOADC ){
					sxu32 nOrig = (sxu32)pInstr->iP2;
					sxu32 nQual;
					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;
					/* Prevent constant expansion but preserve the absolute flag
					 * so the later NEW handler (if any) can see it. */
					pInstr->iP1 &= ~PH7_LOADC_EXPAND;
					/* Namespace-qualify the function name for CALL, unless the
					 * literal is absolute (`\Foo(...)`). Only check function
					 * imports — class imports must NOT affect function
					 * resolution. For `new Foo()`, the CALL handler fires
					 * before NEW; we store the original literal index in the
					 * CALL instruction's iP2 so the NEW handler can recover
					 * the unqualified name and re-qualify with class imports. */
					if( bAbsolute ){
						pInstr->iP2 = (sxi32)nOrig;
					}else{
						int fromImport = 0;
						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);
						pInstr->iP2 = (sxi32)nQual;
						if( nQual != nOrig ){
							/* Record the original literal index in the arg map
							 * (NOT in the CALL's iP2 — that is the hasSpread
							 * flag) so the NEW handler can recover the
							 * unqualified name and re-qualify with CLASS
							 * imports. */
							if( p3 == 0 ){
								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(
									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));
								if( pMap ){
									SyZero(pMap, sizeof(VmCallArgMap));
									p3 = (void *)pMap;
								}
							}
							if( p3 ){
								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;
								if( !fromImport ){
									/* Mark as namespace-qualified */
									((VmCallArgMap *)p3)->bIsNamespaced = 1;
								}
							}
						}
					}
				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */
						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))
					|| pInstr->iOp == PH7_OP_NEW ){
					/* Method call,flag that. But NOT when the callee was an explicitly
					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes
					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER
					 * must stay a plain property READ (leaving the callable on the stack for
					 * OP_CALL to invoke) rather than being rewritten into a method-name
					 * resolution — the parens are exactly what distinguishes `($o->p)()` from
					 * the method call `$o->p()`. */
					pInstr->iP2 = 1;
					/* This OP_MEMBER is where php SCREENS a method call -- an undefined
					 * or inaccessible method, a class that is not there -- and php
					 * reports that refusal at the line the CALL BEGINS on. The emitter
					 * stamped it with the token the generator was standing on, which
					 * for a call spanning several lines is its closing ')'. Same rule,
					 * same source, as the OP_CALL/OP_NEW stamp further down. */
					if( pInstr->iOp == PH7_OP_MEMBER && pNode->pStart ){
						pInstr->nLine = pNode->pStart->nLine;
					}
					/* A static call with a DYNAMIC method name (`C::$m(...)`): the
					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3
					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the
					 * method name is the variable's VALUE. Rebuild the sequence
					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the
					 * dynamic name is read off the stack, matching the instance
					 * (`$o->$m()`) path. iP1==1 marks a static member. */
					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){
						void *pDynName = pInstr->p3;
						(void)PH7_VmPopInstr(pGen->pVm);
						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);
						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);
					}
				}
			}
			/* The callee is resolved; NOW emit the arguments. php's order — the callee
			 * first, at its INIT_FCALL / INIT_METHOD_CALL, and the arguments only after
			 * it — is what makes `(new C)->priv(boom())` report php's `Call to private
			 * method` instead of whatever the argument threw, and what keeps `$o?->m(f())`
			 * from running `f()` on a null receiver. It also puts the callee in reach of
			 * the argument ops one opcode EARLIER than OP_CALL.
			 *
			 * The stack that leaves here is therefore [callee][args…] — the mirror of the
			 * layout OP_CALL's whole dispatch is written against (the method-name pair
			 * below the arguments, the spread runs counted down from the top, the
			 * deferred-argument re-walk). OP_ROT_CALLEE turns the region back over just
			 * before the call, so nothing downstream of it changes. */
			if( !bArgsEmitted ){
				int bTwoSlot;
				sArgs.p3 = p3; /* the namespace map built just above, if any */
				/* A METHOD callee leaves TWO slots — [receiver][method name] — which
				 * OP_CALL reads as one callee (the receiver answers $this and the
				 * late-static-binding class); anything else leaves one. The instruction
				 * just emitted is what decides it. */
				pInstr = PH7_VmPeekInstr(pGen->pVm);
				bTwoSlot = pInstr && pInstr->iOp == PH7_OP_MEMBER
					&& pInstr->iP2 == PH7_MEMBER_METHOD
					/* …unless the member NAME was folded into p3 rather than pushed:
					 * that shape pushes the target alone, so the op leaves one slot,
					 * which is the same distinction vm_ops_oo.c makes before popping. */
					&& pInstr->p3 == 0;
				/* Screen the callee HERE, where php screens it: an undefined function, a
				 * callable string/array naming nothing, a value that is not callable at
				 * all. A METHOD callee needs none — its OP_MEMBER just did it, against
				 * the entry it chose. An FCC needs none either: OP_LOAD_FCC runs the same
				 * screen, and nothing runs before it. And with NO arguments the call
				 * itself is already the first thing to happen, so there is nothing to
				 * order and no reason to pay for a second resolution. */
				{
					ph7_expr_node **apCallArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);
					sxi32 nCallArg = (sxi32)SySetUsed(&pNode->aNodeArgs);
					int bNodeFcc = nCallArg == 1 && apCallArg[0]
						&& (apCallArg[0]->iFlags & EXPR_NODE_FCC);
					if( bNewCallee ){
						/* A `new`'s operand: the screen is OP_NEW itself, run with no
						 * arguments on the stack (iP1 = -1). It asks every refusal the
						 * real pass asks and leaves the class name standing, so the two
						 * cannot disagree. Record where that push is — the NEW codegen
						 * used to find it one instruction behind the trailing OP_CALL,
						 * and the argument list now sits in between. */
						nNewClassInstr = PH7_VmInstrLength(pGen->pVm);
						if( nCallArg > 0 ){
							PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,-1,0,0,0);
						}
					}else if( nCallArg > 0 && !bTwoSlot && !bNodeFcc ){
						/* This screen is where a `Call to undefined function` is
						 * raised for a call that HAS arguments, and php reports such a
						 * call at the line the CALLEE is written on -- not at the
						 * closing parenthesis, which is where the emitter's token
						 * cursor has reached by now. The callee's own load is the
						 * instruction immediately behind (`pInstr`, peeked just above
						 * for bTwoSlot), so its line is the one to carry. A call with
						 * no arguments needs nothing: OP_CALL itself is then the first
						 * thing to run and already reports the callee's line. */
						sxu32 nInitIdx = PH7_VmInstrLength(pGen->pVm);
						sxu32 nCalleeLine = pNode->pStart
							? pNode->pStart->nLine : (pInstr ? pInstr->nLine : 0);
						PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL_INIT,0,
							(p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0,0,0);
						if( nCalleeLine ){
							VmInstr *pInitInstr = PH7_VmGetInstr(pGen->pVm,nInitIdx);
							if( pInitInstr ){
								pInitInstr->nLine = nCalleeLine;
							}
						}
					}
				}
				rc = GenStateEmitCallArgs(&(*pGen),pNode,iFlags,&sArgs);
				if( rc != SXRET_OK ){
					return rc;
				}
				iP1 = sArgs.iP1;
				iP2 = sArgs.iP2;
				p3  = sArgs.p3;
				bFcc = sArgs.bFcc;
				if( iP1 > 0 || iP2 ){
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_ROT_CALLEE,iP1,
						(iP2 ? PH7_ROT_SPREAD : 0) | (bTwoSlot ? PH7_ROT_TWOSLOT : 0),0,0);
				}
				if( bNewCallee && nNewClassInstr > 0 ){
					if( p3 == 0 ){
						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(
							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));
						if( pMap ){
							SyZero(pMap,sizeof(VmCallArgMap));
							p3 = (void *)pMap;
						}
					}
					if( p3 ){
						((VmCallArgMap *)p3)->nNewClassInstr = nNewClassInstr;
					}
				}
			}
		}else if( iVmOp == PH7_OP_LOAD_IDX ){
			ph7_expr_node **apNode;
			sxi32 n;
			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE
				|EXPR_FLAG_LOAD_IDX_ISSET|EXPR_FLAG_LOAD_IDX_UNSET
				|EXPR_FLAG_LOAD_IDX_UNSET_BASE
				|EXPR_FLAG_LOAD_IDX_EMPTY|EXPR_FLAG_MEMBER_WRITE
				|EXPR_FLAG_MEMBER_COALESCE
				|EXPR_FLAG_QUIET_VAR|EXPR_FLAG_RMW_LOAD|EXPR_FLAG_DEFER_ARG);
			/* Recurse and generate bytecodes for array index */
			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);
			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){
				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);
				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);
				if( rc != SXRET_OK ){
					return rc;
				}
				/* Each subscript index is an independent nullsafe scope. */
				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);
			}
			if( SySetUsed(&pNode->aNodeArgs) > 0 ){
				iP1 = 1; /* Node have an index associated with it */
			}else{
				/* `[]` names the element a WRITE is about to create, so php allows it
				 * only where a write lands: an assignment target (plain, compound,
				 * `=&`, a list()/foreach target) and a by-reference argument. Every
				 * other placement is a COMPILE error there — PHL accepted them all and
				 * answered NULL after a PH7-worded notice, so `$x = $a[];`,
				 * `isset($a[])` and `unset($a[][0])` were silent no-ops on source php
				 * refuses to run. A call ARGUMENT is the one shape php also leaves to
				 * runtime (it cannot know the parameter's by-ref-ness at compile time),
				 * which is what DEFER_ARG marks. */
				if( iFlags & (EXPR_FLAG_LOAD_IDX_UNSET|EXPR_FLAG_LOAD_IDX_UNSET_BASE) ){
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,
						pNode->pStart ? pNode->pStart->nLine : 0,
						"Cannot use [] for unsetting");
					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;
				}
				if( (iFlags & (EXPR_FLAG_LOAD_IDX_STORE|EXPR_FLAG_DEFER_ARG)) == 0 ){
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,
						pNode->pStart ? pNode->pStart->nLine : 0,
						"Cannot use [] for reading");
					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;
				}
			}
			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){
				/* offsetExists for ArrayAccess; peek-only for arrays */
				iP2 = 4;
			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){
				/* offsetUnset for ArrayAccess; for an array, remove the ELEMENT. */
				iP2 = 5;
			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET_BASE ){
				/* An unset chain's intermediate container: read it, but with the
				 * unset context's COW-separate and no-vivify rules. */
				iP2 = 10;
			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){
				/* offsetExists+offsetGet for ArrayAccess so empty() can
				 * short-circuit on missing keys without invoking offsetGet
				 * unnecessarily; peek-only for arrays (same as iP2=0). */
				iP2 = 6;
			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){
				/* Create an empty entry when the desired index is not found.
				 * A read-modify-write target (`$a[k] += v`, `$a[k]++`) creates it
				 * the same way but READS it first, so php warns about the missing
				 * key before seeding it — the RMW context says which of the two
				 * this is (VM_IDX_CTX_RMW). The flag rides the whole LHS chain, so
				 * an intermediate level gets it too, as php's BP_VAR_RW fetch does. */
				iP2 = (iFlags & EXPR_FLAG_RMW_LOAD) ? VM_IDX_CTX_RMW : 1;
			}else if( iFlags & EXPR_FLAG_DEFER_ARG ){
				/* D1 commit 2: deferred by-ref/by-value element arg. Behaves as a read but,
				 * on a lookup miss, records the lvalue path instead of warning; OP_CALL
				 * re-walks it in vivify (by-ref) or read+warn (by-value) mode. */
				iP2 = 9;
			}
		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){
			/* POP the left node — its answer is dropped exactly as a statement's is,
			 * so a #[\NoDiscard] callee warns for it too (php warns for every
			 * element of a `for` clause list, not just the last). */
			GenStateMarkDiscardedCall(&(*pGen));
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
	}
	rc = SXRET_OK;
	nJmpIdx = 0;
	/* For :: (static member access), namespace-qualify the class name (left operand).
	 * The left child was just compiled; its LOADC is the last instruction.
	 * Skip self/static/parent — these are keywords, not class names. */
	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){
		pInstr = PH7_VmPeekInstr(pGen->pVm);
		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){
			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);
			int isSpecial = 0;
			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){
				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);
				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);
				if( (n == 4 && SyMemcmp(z,"self",4) == 0) ||
					(n == 6 && SyMemcmp(z,"static",6) == 0) ||
					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){
					isSpecial = 1;
				}
			}
			pInstr->iP1 = 0;
			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the
			 * global class, so it must NOT be re-qualified with the current namespace.
			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.
			 * (Multi-component `\A\B::m` already resolves absolutely because its
			 * literal keeps a backslash; the single-component `\Closure` lost it.) */
			{
				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart
					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));
				if( !isSpecial && !bAbsolute ){
					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);
				}
			}
			/* Foo::class — resolve at compile time. The LOADC already holds the
			 * namespace-qualified name. self/static/parent need runtime resolution. */
			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){
				SyToken *pRightTok = pNode->pRight->pStart;
				if( (pRightTok->nType & PH7_TK_KEYWORD) &&
				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){
					return SXRET_OK;
				}
			}
		}
	}
	if( iVmOp == PH7_OP_IS_A && nLhsFirst < PH7_VmInstrLength(pGen->pVm)
	 && GenStateInstanceofFoldsLhs(&(*pGen),nLhsFirst) ){
		/* php never even compiles the class operand when the SUBJECT of `instanceof`
		 * is a compile-time constant: zend_compile_instanceof folds the whole
		 * expression to FALSE the moment its left operand comes back IS_CONST, so
		 * `5 instanceof $x` is false whatever $x holds -- while `$v instanceof $x`
		 * with the same 5 in $v reaches the runtime opcode and is refused when $x is
		 * neither an object nor a string. The two spellings really do answer
		 * differently, so the screen added to OP_IS_A has to be told which one this
		 * is, and GenStateInstanceofFoldsLhs reads it off the subject's own
		 * instructions. */
		ph7_value *pFalse;
		sxu32 nFalseIdx;
		/* The subject's own instructions go with it: php frees the folded operand
		 * (zend_do_free) rather than leaving it to be computed and dropped. */
		while( PH7_VmInstrLength(pGen->pVm) > nLhsFirst ){
			(void)PH7_VmPopInstr(pGen->pVm);
		}
		pFalse = PH7_ReserveConstObj(pGen->pVm,&nFalseIdx);
		if( pFalse == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromBool(pGen->pVm,pFalse,0);
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,(sxi32)nFalseIdx,0,0);
		return SXRET_OK;
	}
	/* Generate code for the right tree */
	if( pNode->pRight ){
		if( iVmOp == PH7_OP_LAND ){
			/* Emit the false jump so we can short-circuit the logical and */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);
		}else if (iVmOp == PH7_OP_LOR ){
			/* Emit the true jump so we can short-circuit the logical or*/
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);
		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){
			/* Null coalescing: if LHS is not null, jump past RHS */
			iVmOp = 0; /* No binary operator to emit */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);
		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){
			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit
			 * the entire containing postfix chain to null. The jump target is
			 * patched later by the innermost non-chain ancestor (or by
			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack
			 * when taken; otherwise falls through, leaving the object on stack
			 * so the PH7_OP_MEMBER that follows can consume it. */
			sxu32 nNsJmp = 0;
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);
			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);
		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */
			|| pNode->pOp->iOp == EXPR_OP_REF /* `=&` reference bind */ ){
			/* The lvalue is the RIGHT operand (prec-18 ops are right-associative; `=&`
			 * swaps its operands in parse.c so its target is pRight too). Mark it a write
			 * target so a missing base (the container of a subscript-write, or a bare
			 * `$o->p`) is auto-created — PHP auto-vivifies on a plain write AND on a `=&`
			 * bind (`$a[0] =& $x` creates $a as [0 => &$x], it does not warn). */
			/* php's compile-time write-target rules first ($this, a temporary base,
			 * the call that is the target itself). A COMPOUND assignment is a
			 * read-modify-write: php's $this rule does not reach it. */
			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,
				(iVmOp == PH7_OP_STORE || pNode->pOp->iOp == EXPR_OP_REF)
					? 0 : PH7_WTC_RMW);
			if( rc != SXRET_OK ){
				return rc;
			}
			if( pNode->pOp->iOp == EXPR_OP_REF ){
				/* php compiles a reference SOURCE in write context too
				 * (`zend_compile_var(source, BP_VAR_W, 1)`), so `$r =& (new A)->p`
				 * and `$r =& (clone $o)->p` are the same two compile refusals a
				 * write to them would be. Only the call-as-target question is not
				 * asked here: `$r =& f()` is legal. The operands were swapped in
				 * parse.c, so the source is pLeft. */
				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,PH7_WTC_REFSRC);
				if( rc != SXRET_OK ){
					return rc;
				}
			}
			iFlags |= EXPR_FLAG_LOAD_IDX_STORE | EXPR_FLAG_MEMBER_WRITE;
			if( iVmOp != PH7_OP_STORE && pNode->pOp->iOp != EXPR_OP_REF ){
				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so
				 * php warns when it is undefined and then seeds it; a plain `=` and a
				 * `=&` rebind write without reading the target and stay silent. */
				iFlags |= EXPR_FLAG_RMW_LOAD;
			}
		}
		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);
		/* The RIGHT operand is never the reference source: for an assignment it is the
		 * TARGET (the operands were swapped), and for a member access it is the property
		 * NAME -- and a name written as an expression (`$r =& $o->{$a->b}`) would
		 * otherwise be compiled as a write-context fetch of its own. */
		iRhsFlags = iFlags & ~EXPR_FLAG_MEMBER_REFSRC;
		if( iVmOp == PH7_OP_STORE && pNode->pRight
		 && (pNode->pRight->xCode == PH7_CompileList
		  || pNode->pRight->xCode == PH7_CompileShortList) ){
			/* A destructuring target list may bind BY REFERENCE (`[&$t] = $src`),
			 * and php asks at COMPILE time whether the source can hold one --
			 * `zend_is_variable_or_call`, which takes a variable, a property, a
			 * static property, a subscript and a CALL, and refuses everything else
			 * with `Cannot assign reference to non referenceable value`. The list
			 * body cannot ask: by the time it emits a bind the source is an
			 * anonymous value on the stack. Carry the answer to it. */
			sxi8 bSavedSrcRef = pGen->bListSrcNotRef;
			pGen->bListSrcNotRef = (sxi8)!GenStateNodeIsRefSource(pNode->pLeft);
			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iRhsFlags|EXPR_FLAG_RDONLY_LOAD);
			pGen->bListSrcNotRef = bSavedSrcRef;
		}else{
			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iRhsFlags|EXPR_FLAG_RDONLY_LOAD);
		}
		if( !bIsChainOp ){
			/* Non-chain parent: RHS nullsafe chain ends here, before the
			 * operator instruction is emitted. */
			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);
		}
		if( iVmOp == PH7_OP_STORE ){
			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList ||
				pNode->pRight->xCode == PH7_CompileShortList) ){
				/* list()/[] destructuring handles assignment internally via LOAD_LIST;
				 * suppress the STORE instruction entirely.  This check uses the node's
				 * compile handler rather than peeking at the last opcode, because nested
				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the
				 * outer LOAD_LIST, which would fool an opcode-based check.
				 */
				iVmOp = 0;
			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){
				if(pInstr->iOp == PH7_OP_MEMBER ){
					/* Perform a member store operation [i.e: $this->x = 50] */
					iP2 = 1;
				}else{
					if( pInstr->iOp == PH7_OP_LOAD_IDX ){
						/* Transform the STORE instruction to STORE_IDX instruction */
						iVmOp = PH7_OP_STORE_IDX;
						iP1 = pInstr->iP1;
					}else{
						p3 = pInstr->p3;
					}
					/* POP the last dynamic load instruction */
					(void)PH7_VmPopInstr(pGen->pVm);
				}
			}
		}else if( iVmOp == PH7_OP_STORE_REF ){
			/* php records at COMPILE time whether the reference SOURCE was written
			 * as a CALL (ZEND_RETURNS_FUNCTION), so the bind can raise `Only
			 * variables should be assigned by reference` when the callee turns out
			 * not to return by reference. It is the direct call only: `$r =& f()`
			 * warns where `$r =& f()[0]` and `$r =& f()->p` are silent. The operands
			 * were swapped in parse.c, so the source is pLeft. */
			if( pNode->pLeft && pNode->pLeft->pOp
			 && pNode->pLeft->pOp->iOp == EXPR_OP_FUNC_CALL ){
				iP1 |= PH7_STOREREF_CALLSRC;
			}
			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its
			 * OP_MEMBER in place (it resolves + stashes the target slot at
			 * runtime), unlike the variable/array shapes which fold their load
			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){
				/* Tag the member as a reference target and flag STORE_REF (iP2=1)
				 * to take the member-rebind path in the VM. */
				pInstr->iP2 = PH7_MEMBER_REF_TARGET;
				iP2 = 1;
			}else{
				pInstr = PH7_VmPopInstr(pGen->pVm);
				if( pInstr ){
					if( pInstr->iOp == PH7_OP_LOAD_IDX ){
						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]
						 * We have to convert the STORE_REF instruction into STORE_IDX_REF
						 */
						iVmOp = PH7_OP_STORE_IDX_REF;
						iP1 = pInstr->iP1 | (iP1 & PH7_STOREREF_CALLSRC);
						iP2 = pInstr->iP2;
						p3  = pInstr->p3;
					}else{
						p3 = pInstr->p3;
					}
				}
			}
		}
	}
	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0
		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){
		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the
		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */
		iVmOp = 0;
	}
	if( iVmOp > 0 ){
		if( iVmOp == PH7_OP_INCR || iVmOp == PH7_OP_DECR ){
			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){
				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */
				iP1 = 1;
			}
		}else if( iVmOp == PH7_OP_NEW ){
			/* Namespace-qualify the class name for NEW */ {
				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);
				VmInstr *pCallInstr = 0;
				if( pPeek && pPeek->iOp == PH7_OP_CALL ){
					VmCallArgMap *pNewMap = (VmCallArgMap *)pPeek->p3;
					pCallInstr = pPeek;
					/* The class-name push sits one instruction back only when this `new`
					 * takes no arguments; with an argument list the reorder puts the whole
					 * list (and its screen and rotation) in between, so the call node
					 * recorded where the push is. */
					pPeek = (pNewMap && pNewMap->nNewClassInstr > 0)
						? PH7_VmGetInstr(pGen->pVm,pNewMap->nNewClassInstr - 1)
						: PH7_VmPeekNextInstr(pGen->pVm);
				}
				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){
					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;
					sxu32 nLitForClass;
					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;
					/* If the CALL handler qualified the name with FUNCTION
					 * imports, recover the original literal (recorded in the
					 * arg map — OP_CALL's iP2 is the hasSpread flag, and
					 * misreading it as a literal index made `new C(...$args)`
					 * fatal with "Class ' ' is not defined") and re-qualify
					 * with class imports. */
					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){
						nLitForClass = pCallNsMap->nOrigNameLit - 1;
					}else{
						nLitForClass = (sxu32)pPeek->iP2;
					}
					pPeek->iP1 = 0;
					if( !bAbsolute ){
						/* self/static/parent are resolved at runtime against the
						 * current class — never namespace-qualify them (else
						 * `new self` in namespace N becomes "N\self"). Mirrors the
						 * instanceof (IS_A) guard below. */
						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);
						int isSpecialNew = 0;
						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){
							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);
							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);
							if( (n == 4 && SyMemcmp(z,"self",4) == 0) ||
								(n == 6 && SyMemcmp(z,"static",6) == 0) ||
								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){
								isSpecialNew = 1;
							}
						}
						if( isSpecialNew ){
							pPeek->iP2 = (sxi32)nLitForClass;
						}else{
							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);
						}
					}else{
						pPeek->iP2 = (sxi32)nLitForClass;
					}
				}
			}
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr && pInstr->iOp == PH7_OP_CALL ){
				VmInstr *pPrev;
				int bPrevMember;
				pPrev = PH7_VmPeekNextInstr(pGen->pVm);
				/* "Was the callee a MEMBER access?" — which, once the reorder puts a
				 * rotation between the callee and its call, is the question the rotation
				 * already answers (a method callee is the two-slot one). */
				bPrevMember = pPrev && (pPrev->iOp == PH7_OP_ROT_CALLEE
					? (pPrev->iP2 & PH7_ROT_TWOSLOT) != 0
					: pPrev->iOp == PH7_OP_MEMBER);
				if( !bPrevMember ){
					/* Pop the call instruction, preserve named-arg map and
					 * the hasSpread flag (OP_NEW consumes the spread
					 * accumulator exactly like OP_CALL would have). */
					iP1 = pInstr->iP1;
					iP2 = pInstr->iP2;
					if( pInstr->p3 ){
						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */
					}
					(void)PH7_VmPopInstr(pGen->pVm);
				}
			}
		}else if( iVmOp == PH7_OP_IS_A ){
			/* instanceof: right operand is a class name, not a constant.
			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){
				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);
				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;
				int isSpecialIs = 0;
				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){
					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);
					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);
					if( (n == 4 && SyMemcmp(z,"self",4) == 0) ||
						(n == 6 && SyMemcmp(z,"static",6) == 0) ||
						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){
						isSpecialIs = 1;
					}
				}
				pInstr->iP1 = 0;
				if( !isSpecialIs && !bAbsolute ){
					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);
				}
			}
		}else if( iVmOp == PH7_OP_MEMBER){
			/* Prevent constant expansion for member/property names.
			 * The right child (member name) was just compiled — its LOADC
			 * should not trigger constant lookup. */
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){
				pInstr->iP1 = 0;
			}
			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){
				/* Static member access,remember that */
				iP1 = 1;
				pInstr = PH7_VmPeekInstr(pGen->pVm);
				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){
					p3 = pInstr->p3;
					/* A `$`-form (`C::$s`, `C::$$x`, `C::${$e}`) is a STATIC PROPERTY
					 * access, never a constant. A LITERAL name folds into p3 (non-zero)
					 * and the exec side reads it there; a DYNAMIC name (`$$x`/`${$e}`)
					 * leaves p3==0 with the computed name on the stack — the SAME shape
					 * as a bareword constant `C::C`. Mark iP1=2 so exec still routes it
					 * to the property table (hAttr), not the constant table (hConst). */
					if( p3 == 0 ){
						iP1 = 2;
					}
					(void)PH7_VmPopInstr(pGen->pVm);
				}
			}
			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()
			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the
			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same
			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */
			if( iP2 == PH7_MEMBER_READ ){
				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){
					iP2 = PH7_MEMBER_UNSET;
				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){
					iP2 = PH7_MEMBER_ISSET;
				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){
					iP2 = PH7_MEMBER_EMPTY;
				}else if( iFlags & EXPR_FLAG_MEMBER_COALESCE ){
					iP2 = PH7_MEMBER_COALESCE;
				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){
					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */
					iP2 = PH7_MEMBER_WRITE;
				}else if( iFlags & EXPR_FLAG_DEFER_ARG ){
					/* D1 commit 2: deferred by-ref/by-value property arg ($o->p). */
					iP2 = PH7_MEMBER_DEFPATH;
				}
			}
		}
		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of
		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack
		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we
		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves
		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */
		if( bFcc ){
			iVmOp = PH7_OP_LOAD_FCC;
			/* php's global fallback applies to a first-class callable exactly as it does
			 * to the call it stands for: inside a namespace, `strlen(...)` is the global
			 * function when the current namespace has none. The callee's literal was
			 * namespace-qualified above and the arg map that records it is dropped here
			 * (an FCC has no arguments), so carry the one bit the resolution needs in the
			 * instruction itself — without it `strlen(...)` in a namespaced file was
			 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */
			iP2 = (p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0;
			p3 = 0;
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr && pInstr->iOp == PH7_OP_MEMBER && pInstr->iP2 == PH7_MEMBER_METHOD ){
				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name
				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD
				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC
				 * sees the [target, method-name] pair the iP1=2 handler expects. */
				void *pMemberName = pInstr->p3;
				(void)PH7_VmPopInstr(pGen->pVm);
				if( pMemberName ){
					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);
				}
				iP1 = 2;
			}else{
				/* Only a METHOD member is the callee's NAME. A parenthesised PROPERTY read
				 * (`($o->cb)(...)`, `(C::$cb)(...)`) is php's variable-invocation: the member
				 * op stays, its VALUE is the callable, and this is the iP1=1 wrap. Dropping it
				 * here read the property NAME as a method name and answered
				 * `Call to undefined method H::cb()` for a closure the object was holding —
				 * the CALL codegen above already made the distinction (it leaves the member a
				 * plain read for a parenthesised callee) and this branch undid it. */
				iP1 = 1;
			}
		}
		/* Tag CALL/NEW sites with the caller file's strict_types flag.
		 * This is the primary emit path for user-visible calls. */
		if( iVmOp == PH7_OP_CALL || iVmOp == PH7_OP_NEW ){
			p3 = GenStateAttachStrictFlag(pGen,p3);
		}
		/* Finally,emit the VM instruction associated with this operator */
		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);
		if( iVmOp == PH7_OP_MEMBER && iP2 == PH7_MEMBER_READ
		 && (iFlags & EXPR_FLAG_MEMBER_REFSRC) ){
			/* The reference SOURCE keeps its READ mode -- php hands back a copy for a
			 * handler-backed property, and dispatches __get for an overloaded one -- and
			 * carries the write-context marker beside it (see VmInstr::bRefSrc). */
			VmInstr *pRefSrc = PH7_VmPeekInstr(pGen->pVm);
			if( pRefSrc ){
				pRefSrc->bRefSrc = 1;
			}
		}
		if( (iVmOp == PH7_OP_CALL || iVmOp == PH7_OP_NEW) && pNode->pStart ){
			/* A call's own line is where it BEGINS, not where its argument list
			 * closes. The emitter stamps every instruction with the token the
			 * generator is standing on, which for a call is the ')' -- so a call
			 * written across several lines went into the backtrace at its LAST one
			 * and php records its first. */
			VmInstr *pCallInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pCallInstr ){
				pCallInstr->nLine = pNode->pStart->nLine;
			}
		}
	}
	if( nJmpIdx > 0 ){
		/* Fix short-circuited jumps now the destination is resolved */
		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);
		if( pInstr ){
			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);
		}
	}
	return rc;
}
/*
 * Emit a call's ARGUMENT LIST, and decide everything about it the OP_CALL then carries:
 * the count, the unpack flag, the named-argument / assert-source / argument-shape map.
 *
 * Split out of GenStateEmitExprCode because php resolves a callee BEFORE it evaluates
 * the arguments, so this now runs AFTER the callee sub-tree has been emitted (the one
 * exception is a `new`'s constructor list — see EXPR_FLAG_NEW_CALLEE). It is otherwise
 * the same code, and reads only the node: nothing here inspects the instructions the
 * callee left behind.
 *
 * pArgs->p3 may arrive non-NULL — the callee's own namespace qualification builds the
 * VmCallArgMap first now — and every allocation site below reuses it.
 */
static sxi32 GenStateEmitCallArgs(
	ph7_gen_state *pGen,  /* Code generator state */
	ph7_expr_node *pNode, /* The call node */
	sxi32 iFlags,         /* Control flags of the call site */
	GenCallArgs *pArgs    /* OUT: what the OP_CALL needs */
	)
{
	void *p3 = pArgs->p3;
	sxi32 iP1 = 0;
	sxu32 iP2 = 0;
	int bFcc = 0;
	sxi32 rc;
	ph7_expr_node **apNode;
	int hasSpread = 0;
	int hasNamed = 0;
	sxu32 byRefMask = 0;
	sxi32 nArgs;
	sxi32 n;
	int bAnySpread = 0;
	/* Recurse and generate bytecodes for function arguments */
	apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);
	nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);
	/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.
	 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we
	 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */
	if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){
		bFcc = 1;
		nArgs = 0;
	}
	/* Validate argument order like php: no positional argument after a
	 * named one OR after unpacking, and `name: ...$x` is a parse error. */
	{
		int seenNamed = 0;
		int seenSpread = 0;
		for( n = 0; n < nArgs; ++n ){
			if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){
				bAnySpread = 1;
				seenSpread = 1;
				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){
					rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[n]->pStart->nLine,
						"syntax error, unexpected token \"...\"");
					return SXERR_SYNTAX;
				}
				if( seenNamed ){
					/* The mirror of the positional-after-named rule: php refuses the
					 * UNPACK too, and at compile time. Without it `f(x: 1, ...$a)` ran
					 * and reported whatever the runtime binder made of the flattened
					 * list -- a different sentence, raised too late, on a program php
					 * never starts. */
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,
						"Cannot use argument unpacking after named arguments");
					return SXERR_SYNTAX;
				}
			}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){
				seenNamed = 1;
				hasNamed = 1;
			}else if( seenNamed ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,
					"Cannot use positional argument after named argument");
				return SXERR_SYNTAX;
			}else if( seenSpread ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,
					"Cannot use positional argument after argument unpacking");
				return SXERR_SYNTAX;
			}
		}
	}
	/* Read-only load */
	iFlags |= EXPR_FLAG_RDONLY_LOAD;
	/* Route subscript-argument LOAD_IDX through a special iP2 code
	 * for the language constructs `isset` and `empty` so ArrayAccess
	 * objects dispatch to the right method (offsetExists for both;
	 * empty also needs offsetGet to evaluate emptiness on hits). */
	if( pNode->pLeft && pNode->pLeft->pStart ){
		SyString *pCallName = &pNode->pLeft->pStart->sData;
		int bIsset = pCallName->nByte == 5
			&& SyStrnicmp(pCallName->zString,"isset",5) == 0;
		int bEmpty = pCallName->nByte == 5
			&& SyStrnicmp(pCallName->zString,"empty",5) == 0;
		/* isset()/empty() are language CONSTRUCTS, not functions: php parses
		 * their argument list in the grammar and a missing operand is a parse
		 * error on the ')'. They compile through this ordinary call loop, which
		 * never checked arity, so `empty()` quietly evaluated to true and
		 * `isset()` to false. (empty() also takes exactly one operand in php,
		 * unlike isset(), which is variadic.) */
		if( (bIsset || bEmpty) && nArgs < 1 ){
			/* php names the ')' itself as the unexpected token, so point at the
			 * node's last token rather than pGen->pIn (which has already moved
			 * past the call to the statement's ';'). */
			SyToken *pTok = pNode->pEnd;
			if( pTok && pTok > pNode->pStart && (pTok->nType & PH7_TK_RPAREN) == 0 ){
				pTok--;
			}
			PH7_GenSyntaxError(&(*pGen),pTok,0);
			return SXERR_ABORT;
		}
		if( bIsset ){
			iFlags |= EXPR_FLAG_LOAD_IDX_ISSET;
		}else if( bEmpty ){
			iFlags |= EXPR_FLAG_LOAD_IDX_EMPTY;
		}
		/* Auto-vivify by-reference out-params of known builtins so an
		 * undefined variable argument (e.g. preg_match($p,$s,$m) with
		 * $m never assigned) gets a real memobj slot for the builtin to
		 * write back through. Skipped when spread/named args are present:
		 * the compile-time positional index no longer maps to the
		 * runtime apArg[] slot (and spread elements can't be by-ref). */
		if( !bAnySpread && !hasNamed ){
			SyString sBuiltin;
			GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);
			byRefMask = GenStateByRefBuiltinMask(&sBuiltin);
		}
	}
	for( n = 0 ; n < nArgs ; ++n ){
		sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);
		sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE|EXPR_FLAG_MEMBER_WRITE
			|EXPR_FLAG_MEMBER_REFSRC);
		/* For a by-ref argument position, drop the read-only flag so the
		 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and
		 * set write-context so a subscript target (preg_match($p,$s,$a['k']))
		 * auto-vivifies its element and exposes a writable memobj slot for the
		 * builtin to write back through. A plain $var target is unaffected
		 * (iP1=0 either way).
		 *
		 * A PROPERTY target is the one shape this eager path cannot express, so it
		 * is left to the deferred one below: what php's `FETCH_OBJ_W` does to
		 * `$o->p` is not what an ASSIGNMENT does to it — a missing property is
		 * CREATED, an overloaded one takes `Indirect modification of overloaded
		 * property` and is passed by VALUE (rather than reaching `__set`), and a
		 * non-object base is the catchable `Attempt to modify property`. The
		 * deferred resolver already encodes all of that (VmBindPropByRef) and
		 * already reads a host function's by-ref mask, so routing the property
		 * shapes through it is what makes `preg_match($p, $s, $this->matches)` —
		 * the ordinary spelling — write anything at all. */
		if( n < 31 && (byRefMask & (1u<<n))
		 && !GenStateArgHasPropertyStep(apNode[n]) ){
			iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;
			iArgFlags |= EXPR_FLAG_LOAD_IDX_STORE;
		}
		/* D1: a plain `$var` argument may bind to a by-ref parameter whose signature
		 * is unknown at compile time (forward reference, dynamic call, or method
		 * dispatch — e.g. PHPUnit's `willReturnReference($undef)`). We used to clear
		 * the read-only flag here so an undefined variable vivified a real slot the
		 * by-ref write-back could reach — but that also invented the variable as NULL
		 * in the caller when the parameter turned out by-VALUE, and suppressed php's
		 * `Undefined variable $x` warning. Instead mark it DEFERRED: OP_LOAD leaves an
		 * undefined variable uncreated and carries a lazy-lvalue marker, and OP_CALL
		 * materializes it ONLY for a by-ref parameter once the callee is resolved
		 * (VmResolveDeferredArgs). Excludes isset()/empty()/unset(), which compile
		 * through this same call loop but must NEVER create their operand, and
		 * spread args (whose elements have no positional index of their own). A NAMED
		 * arg defers too: it binds to the formal its NAME picks, which the resolver
		 * looks up through the call's own argument map — excluding it left
		 * `r(x: $a['new'])` warning `Undefined array key` and passing NULL where php
		 * creates the element for the by-ref parameter `$x`.
		 *
		 * D1 commit 2: the same reasoning extends to an array-element ($a["k"]) or
		 * property ($o->p) argument — a by-ref user-function parameter must vivify the
		 * element/property, a by-value one must warn and NOT vivify. Those nodes carry a
		 * subscript/arrow operator (pOp != 0). The DEFER flag rides down to the base LOAD
		 * (undefined base auto-defers via commit 1) and to the LOAD_IDX/MEMBER, which
		 * record the lvalue path on a lookup miss. Static `::` and nullsafe `?->` stay
		 * eager. */
		if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET|EXPR_FLAG_LOAD_IDX_EMPTY|EXPR_FLAG_LOAD_IDX_UNSET
		               |EXPR_FLAG_MEMBER_COALESCE)) == 0
		 && (iArgFlags & EXPR_FLAG_RDONLY_LOAD) /* not a known builtin by-ref slot (kept eager above) */
		 && (apNode[n]->iFlags & EXPR_NODE_SPREAD) == 0
		 && ( (apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable)
		   || (apNode[n]->pOp != 0 && (apNode[n]->pOp->iOp == EXPR_OP_SUBSCRIPT
		                            || apNode[n]->pOp->iOp == EXPR_OP_ARROW)) ) ){
			iArgFlags |= EXPR_FLAG_DEFER_ARG;
		}
		rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Each argument is an independent nullsafe scope. */
		GenStatePatchNullsafeJumps(pGen, nArgNsBase);
		if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){
			/* Emit spread opcode to unpack this array argument. iP1 marks a
			 * source php will unpack BY REFERENCE: only a plain `$var` (php
			 * fetches every other shape — `$a[0]`, `$o->p`, `C::$s`, a cast, a
			 * call — as an R-value, so a by-ref parameter binds its elements in
			 * a temporary and the write-back is invisible). The expander needs
			 * the distinction because it carries each element's slot for the
			 * by-ref binder; without it `r(...$a[0])` wrote through to the real
			 * element, which php leaves alone. */
			PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD,
				(apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable) ? 1 : 0,
				0, 0, 0);
			hasSpread = 1;
		}
	}
	/* Total number of given arguments */
	iP1 = nArgs;
	iP2 = hasSpread;
	/* Build VmCallArgMap if named arguments are present.
	 * Deep-copy name strings so they survive token stream cleanup. */
	if( hasNamed ){
		sxu32 nStrBytes = 0;
		char *zBuf;
		for( n = 0; n < nArgs; ++n ){
			if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){
				nStrBytes += (sxu32)apNode[n]->sArgName.nByte;
			}
		}
		{
		sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;
		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(
			&pGen->pVm->sAllocator, mapSize);
		if( pMap ){
			SyZero(pMap, mapSize);
			/* The names need their own contiguous allocation, so this map REPLACES
			 * whatever the callee's namespace qualification built -- and it has to
			 * carry that map's findings across. Dropping them lost the ORIGINAL
			 * name literal, which is the only thing the `new` codegen can
			 * re-qualify with CLASS imports: `new Imported(x: 1)` then resolved
			 * against the current namespace and the class was not found, while
			 * the same `new` with positional arguments worked. */
			if( p3 ){
				VmCallArgMap *pPrior = (VmCallArgMap *)p3;
				pMap->nOrigNameLit = pPrior->nOrigNameLit;
				pMap->bIsNamespaced = pPrior->bIsNamespaced;
				pMap->nNewClassInstr = pPrior->nNewClassInstr;
				pMap->bStrict = pPrior->bStrict;
				/* Nothing else holds it: it is attached to no instruction yet. */
				SyMemBackendFree(&pGen->pVm->sAllocator,pPrior);
			}
			pMap->bHasNamed = 1;
			pMap->nTotal = (sxu32)nArgs;
			pMap->aNames = (SyString *)&pMap[1];
			zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */
			for( n = 0; n < nArgs; ++n ){
				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){
					sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;
					SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);
					SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);
					zBuf += nb;
				}
				/* else: aNames[n] remains {NULL, 0} for positional */
			}
			p3 = (void *)pMap;
		}
		}
	}
	/* assert(): php's compiler keeps a copy of the assertion's AST and a
	 * failing assert reports its rendered SOURCE (`assert(1 == 2)`), not the
	 * evaluated value. Render the first argument's token span
	 * into the call map so vm_builtin_assert can echo it. Only a DIRECT
	 * unqualified/absolute call qualifies — matching php, an indirect call
	 * (call_user_func, a callable variable) has no source text and its
	 * AssertionError carries an empty message. A spread first argument is
	 * skipped (its span is the unpacked array, not the assertion). */
	if( nArgs >= 1 && !bFcc
	 && (apNode[0]->iFlags & EXPR_NODE_SPREAD) == 0 ){
		SyString sCallee;
		GenStateCallBuiltinName(pNode->pLeft,&sCallee);
		if( sCallee.nByte == sizeof("assert")-1
		 && SyStrnicmp(sCallee.zString,"assert",sizeof("assert")-1) == 0 ){
			/* An operator root's pStart/pEnd name only the operator token
			 * (`1 == 2` roots at `==`); the subtree walk recovers the whole
			 * raw extent, re-adding parens the grouping pass consumed. */
			SyToken *pSpanIn = 0;
			SyToken *pSpanEnd = 0;
			SyBlob sSrc;
			PH7_ExprSubtreeSpan(apNode[0],&pSpanIn,&pSpanEnd);
			SyBlobInit(&sSrc,&pGen->pVm->sAllocator);
			if( pSpanIn && pSpanEnd && pSpanIn < pSpanEnd ){
				if( apNode[0]->iFlags & EXPR_NODE_NAMED_ARG ){
					/* php renders the name too: `assert(assertion: 1 == 2)`. */
					SyBlobAppend(&sSrc,apNode[0]->sArgName.zString,apNode[0]->sArgName.nByte);
					SyBlobAppend(&sSrc,": ",2);
				}
				PH7_GenRenderAssertSpan(pGen,pSpanIn,pSpanEnd,&sSrc);
			}
			if( SyBlobLength(&sSrc) > 0 ){
				char *zDup = (char *)SyMemBackendDup(&pGen->pVm->sAllocator,
					SyBlobData(&sSrc),SyBlobLength(&sSrc));
				if( zDup ){
					if( p3 == 0 ){
						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(
							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));
						if( pMap ){
							SyZero(pMap,sizeof(VmCallArgMap));
							p3 = (void *)pMap;
						}
					}
					if( p3 ){
						SyStringInitFromBuf(&((VmCallArgMap *)p3)->sAssertSrc,
							zDup,SyBlobLength(&sSrc));
					}
				}
			}
			SyBlobRelease(&sSrc);
		}
	}
	/* Record each argument's compile-time SHAPE so the by-ref binders can
	 * refuse a non-variable where php refuses it — at the CALL, before the
	 * callee's ZPP runs. Skipped when the call SPREADS (one compile-time
	 * argument becomes N runtime slots, so the positions no longer line up)
	 * or when it carries more arguments than the masks can hold; a call
	 * without the flag keeps the old runtime nIdx test. Named arguments are
	 * fine: they change which FORMAL a slot binds to, not the slot's index. */
	if( !bAnySpread && nArgs > 0 && nArgs <= 31 && !bFcc ){
		sxu32 nNonLval = 0;
		sxu32 nTempCall = 0;
		for( n = 0 ; n < nArgs ; ++n ){
			int iShape = GenStateArgShape(apNode[n]);
			if( iShape == GEN_ARG_NONE ){
				nNonLval |= (1u << n);
			}else if( iShape == GEN_ARG_TEMPCALL ){
				nTempCall |= (1u << n);
			}
		}
		if( p3 == 0 ){
			VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(
				&pGen->pVm->sAllocator,sizeof(VmCallArgMap));
			if( pMap ){
				SyZero(pMap,sizeof(VmCallArgMap));
				p3 = (void *)pMap;
			}
		}
		if( p3 ){
			((VmCallArgMap *)p3)->bArgShapes = 1;
			((VmCallArgMap *)p3)->nNonLvalMask = nNonLval;
			((VmCallArgMap *)p3)->nTempCallMask = nTempCall;
		}
	}
	pArgs->iP1 = iP1;
	pArgs->iP2 = iP2;
	pArgs->p3  = p3;
	pArgs->bFcc = bFcc;
	pArgs->bAnySpread = bAnySpread;
	return SXRET_OK;
}
/*
 * Compile a PHP expression.
 * According to the PHP language reference manual:
 *  Expressions are the most important building stones of PHP.
 *  In PHP, almost anything you write is an expression.
 *  The simplest yet most accurate way to define an expression
 *  is "anything that has a value".
 * If something goes wrong while compiling the expression,this
 * function takes care of generating the appropriate error
 * message.
 */
/*
 * Does this expression tree contain a comma OPERATOR node?
 *
 * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so
 * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.
 * php 8 has no comma operator: its grammar only allows comma-separated
 * expression LISTS inside for(...) clauses (call arguments, array literals and
 * list() are split by the parser, never by this node). Accepting it changes the
 * meaning of source php rejects, which §10 classes as a bug — so every context
 * except for() now reports php's parse error.
 */
static int GenStateTreeHasComma(ph7_expr_node *pNode)
{
	ph7_expr_node **apArg;
	sxu32 n;
	if( pNode == 0 ){
		return 0;
	}
	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){
		return 1;
	}
	if( GenStateTreeHasComma(pNode->pLeft) || GenStateTreeHasComma(pNode->pRight)
	 || GenStateTreeHasComma(pNode->pCond) ){
		return 1;
	}
	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);
	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){
		if( GenStateTreeHasComma(apArg[n]) ){
			return 1;
		}
	}
	return 0;
}
PH7_PRIVATE sxi32 PH7_CompileExpr(
	ph7_gen_state *pGen, /* Code generator state */
	sxi32 iFlags,        /* Control flags */
	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */
	)
{
	ph7_expr_node *pRoot;
	SySet sExprNode;
	SyToken *pEnd;
	sxi32 nExpr;
	sxi32 iNest;
	sxi32 rc;
	sxu32 nNullsafeBase;
	/* Initialize worker variables */
	nExpr = 0;
	pRoot = 0;
	/* Any nullsafe jumps still pending belong to an outer scope; isolate
	 * this expression so its `?->` short-circuits don't leak out. */
	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);
	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));
	SySetAlloc(&sExprNode,0x10);
	rc = SXRET_OK;
	/* Delimit the expression */
	pEnd = pGen->pIn;
	iNest = 0;
	while( pEnd < pGen->pEnd ){
		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){
			/* Ticket 1433-30: Annonymous/Closure functions body */
			iNest++;
		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){
			iNest--;
		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){
			if( iNest <= 0 ){
				break;
			}
		}
		pEnd++;
	}
	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){
		SyToken *pEnd2 = pGen->pIn;
		iNest = 0;
		/* Stop at the first comma */
		while( pEnd2 < pEnd ){
			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/|PH7_TK_OSB/*'['*/|PH7_TK_LPAREN/*'('*/) ){
				iNest++;
			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/|PH7_TK_CSB/*']'*/|PH7_TK_RPAREN/*')'*/)){
				iNest--;
			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){
				if( iNest <= 0 ){
					break;
				}
			}
			pEnd2++;
		}
		if( pEnd2 <pEnd ){
			pEnd = pEnd2;
		}
	}
	if( pEnd > pGen->pIn ){
		SyToken *pTmp = pGen->pEnd;
		/* Swap delimiter */
		pGen->pEnd = pEnd;
		/* Try to get an expression tree */
		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);
		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1
		 && GenStateTreeHasComma(pRoot) ){
			/* php has no comma operator outside a for() clause */
			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,
				"syntax error, unexpected token \",\"");
			pGen->pEnd = pTmp;
			if( rc == SXERR_ABORT ){
				SySetRelease(&sExprNode);
				return SXERR_ABORT;
			}
			pGen->pIn = pEnd;
			SySetRelease(&sExprNode);
			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);
			return SXRET_OK;
		}
		if( rc == SXRET_OK && pRoot ){
			rc = SXRET_OK;
			if( xTreeValidator ){
				/* Call the upper layer validator callback */
				rc = xTreeValidator(&(*pGen),pRoot);
			}
			if( rc != SXERR_ABORT ){
				/* Generate code for the given tree */
				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);
				/* Patch any unresolved nullsafe jumps emitted by this
				 * expression so they short-circuit to its end. */
				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);
			}
			nExpr = 1;
		}
		/* Release the whole tree */
		PH7_ExprFreeTree(&(*pGen),&sExprNode);
		/* Synchronize token stream */
		pGen->pEnd = pTmp;
		pGen->pIn  = pEnd;
		if( rc == SXERR_ABORT ){
			SySetRelease(&sExprNode);
			return SXERR_ABORT;
		}
	}
	SySetRelease(&sExprNode);
	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;
}
/*
 * Return a pointer to the node construct handler associated
 * with a given node type [i.e: string,integer,float,...].
 */
PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)
{
	if( nNodeType & PH7_TK_NUM ){
		/* Numeric literal: Either real or integer */
		return PH7_CompileNumLiteral;
	}else if( nNodeType & PH7_TK_DSTR ){
		/* Double quoted string */
		return PH7_CompileString;
	}else if( nNodeType & PH7_TK_SSTR ){
		/* Single quoted string */
		return PH7_CompileSimpleString;
	}else if( nNodeType & PH7_TK_HEREDOC ){
		/* Heredoc */
		return PH7_CompileHereDoc;
	}else if( nNodeType & PH7_TK_NOWDOC ){
		/* Nowdoc */
		return PH7_CompileNowDoc;
	}else if( nNodeType & PH7_TK_BSTR ){
		/* Backtick quoted string */
		return PH7_CompileBacktic;
	}
	return 0;
}
/*
 * Tree validator for unset() arguments — php's write-target rules, then its
 * "Can't use nullsafe operator in write context", then the grammar: `unset()`
 * takes a `variable`, so `unset(GK)`, `unset("s")` and `unset(A::K)` are php
 * PARSE errors where PHL let them reach the VM and answer with a PH7-ism.
 */
static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)
{
	sxi32 rc;
	rc = GenStateWriteTargetCheck(&(*pGen),pNode,PH7_WTC_UNSET);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( PH7_ExprContainsNullsafe(pNode) ){
		rc = PH7_GenCompileError(pGen,E_ERROR,
			pNode ? pNode->pStart->nLine : 1,
			"Can't use nullsafe operator in write context");
		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;
	}
	if( pNode && PH7_ExprIsModifiableValue(pNode) == FALSE ){
		return PH7_ExprOperandNotAVariable(pGen,pNode);
	}
	return SXRET_OK;
}
/*
 * Compile an unset() statement.
 * unset($var, $arr[$key], ...);
 * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that
 * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the
 * parent array before extracting the element to unset.
 */
static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pEnd,*pNext = 0;
	sxu32 nIdx = 0;
	SyString sName;
	sxi32 rc;
	/* Jump the 'unset' keyword */
	pGen->pIn++;
	/* Save delimiter */
	pTmp = pGen->pEnd;
	/* Skip optional opening parenthesis and find the matching close */
	pEnd = pTmp; /* Default: scan to statement end */
	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){
		/* Find matching ')' — start scanning AFTER the '(' */
		SyToken *pClose;
		pGen->pIn++;   /* Skip '(' */
		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);
		pEnd = pClose; /* Stop at ')' */
	}
	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);
	/* Resolve the 'unset' builtin name once */
	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){
		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
		if( pObj == 0 ){
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);
		GenStateInstallLiteral(&(*pGen),pObj,nIdx);
	}
	/* Compile each comma-separated argument */
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){
		if( pGen->pIn < pNext ){
			/*
			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a
			 * single NAME binding, which the unset() builtin cannot express — all it ever
			 * receives is the value's slot index, and unsetting the slot destroys whatever
			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and
			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which
			 * already removes just the element/property.
			 */
			if( &pGen->pIn[2] == pNext
				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)
				&& (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) ){
				SyString *pVarName;
				/* php refuses `unset($this)` where it is written. The tree validator
				 * cannot see it — this fast path never builds a tree. */
				if( pGen->pIn[1].sData.nByte == sizeof("this")-1
				 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,
				             (const void *)"this",sizeof("this")-1) == 0 ){
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
						"Cannot unset $this");
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					/* php stops compiling at its own fatal; this generator carries
					 * on to a budget of fifteen, so leave the cursor PAST the whole
					 * `unset(...)` rather than on the operand it refused. Resuming
					 * there re-read the closing ')' as a statement of its own and
					 * printed an "Unmatched ')'" under the fatal that php never
					 * reaches. */
					pGen->pIn = pEnd;
					if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){
						pGen->pIn++;
					}
					pGen->pEnd = pTmp;
					return SXERR_SYNTAX;
				}
				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);
				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));
				if( zDup == 0 || pVarName == 0 ){
					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
						"Fatal, PH7 is running out of memory");
					return SXERR_ABORT;
				}
				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);
				pGen->pIn = pNext;
				if( pGen->pIn < pEnd ){
					pGen->pIn++; /* Jump the trailing comma */
				}
				continue;
			}
			pGen->pEnd = pNext;
			rc = PH7_CompileExpr(&(*pGen),
				EXPR_FLAG_RDONLY_LOAD|EXPR_FLAG_LOAD_IDX_UNSET,
				GenStateUnsetValidator);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( rc != SXERR_EMPTY ){
				/* Emit call for this single argument */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
			}
		}
		/* Jump trailing commas */
		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){
			pNext++;
		}
		pGen->pIn = pNext;
	}
	/* Skip past the closing ')' if present */
	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){
		pGen->pIn++;
	}
	/* Restore token stream */
	pGen->pEnd = pTmp;
	return SXRET_OK;
}
/*
 * PHP Language construct table.
 */
static const LangConstruct aLangConstruct[] = {
	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */
	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */
	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */
	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */
	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */
	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */
	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */
	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */
	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */
	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */
	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */
	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */
	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */
	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */
	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */
	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */
	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */
	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */
	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */
	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */
	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */
	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */
	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */
	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */
};
/*
 * Return a pointer to the statement handler routine associated
 * with a given PHP keyword [i.e: if,for,while,...].
 */
static ProcLangConstruct GenStateGetStatementHandler(
	sxu32 nKeywordID,   /* Keyword  ID*/
	SyToken *pLookahed  /* Look-ahead token */
	)
{
	sxu32 n = 0;
	for(;;){
		if( n >= SX_ARRAYSIZE(aLangConstruct) ){
			break;
		}
		if( aLangConstruct[n].nID == nKeywordID ){
			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){
				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;
				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){
					/* 'static' (class context),return null */
					return 0;
				}
			}
			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed
				&& (pLookahed->nType & PH7_TK_KEYWORD)
				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){
				/* 'static fn(...)' arrow function — compile as expression */
				return 0;
			}
			/* Return a pointer to the handler.
			*/
			return aLangConstruct[n].xConstruct;
		}
		n++;
	}
	if( pLookahed ){
		if(nKeywordID == PH7_TKWRD_INTERFACE && PH7_IsClassNameToken(pLookahed) ){
			return PH7_CompileClassInterface;
		}else if(nKeywordID == PH7_TKWRD_CLASS && PH7_IsClassNameToken(pLookahed) ){
			return PH7_CompileClass;
		}else if(nKeywordID == PH7_TKWRD_TRAIT && PH7_IsClassNameToken(pLookahed) ){
			return PH7_CompileTrait;
		}
		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly
		 * combined — are routed via GenStateStartsModifiedClass in the chunk
		 * compiler, which can scan the whole modifier run (the lookahead here is
		 * a single token and cannot see past `final readonly …`). */
	}
	/* Not a language construct */
	return 0;
}
/*
 * Which words may NAME a class, an interface, a trait or an enum, and which may
 * not — php has two separate answers and this file used to have one.
 *
 * php's SCANNER decides the first: a reserved keyword is not an identifier, so
 * `class list {}` and `class callable {}` are parse errors. Its COMPILER decides
 * the second, for a handful of words the scanner does hand over as identifiers:
 * `zend_is_reserved_class_name` refuses `int`, `bool`, `void`, `null`, `self`
 * and their neighbours with `Cannot use "X" as a class name as it is reserved`.
 *
 * PHL's keyword set is not php's, which is where the divergence came from in both
 * directions. `integer` and `boolean` are CAST words here and identifiers in php,
 * so `class Integer extends Base {}` — phpseclib writes exactly that, three times
 * — did not compile at all. And `void`, `never`, `null`, `false`, `true`, `mixed`
 * and `iterable` arrive as plain identifiers here, so declaring a class with one
 * of those names SUCCEEDED where php refuses.
 */
static int GenStateNameIs(const SyString *pName,const char *zWord)
{
	sxu32 n = (sxu32)SyStrlen(zWord);
	/* Length FIRST: the token's bytes point into the source and are not
	 * NUL-terminated, so a shorter name must never be compared over its end. */
	return pName->nByte == n && SyStrnicmp(pName->zString,zWord,n) == 0;
}
static int GenStateClassNameKeywordOk(const SyString *pName)
{
	/* The only two words PHL lexes as keywords that php lets name a class. */
	return GenStateNameIs(pName,"integer") || GenStateNameIs(pName,"boolean");
}
PH7_PRIVATE int PH7_IsClassNameToken(const SyToken *pTok)
{
	if( pTok == 0 ){
		return 0;
	}
	if( pTok->nType & PH7_TK_KEYWORD ){
		return GenStateClassNameKeywordOk(&pTok->sData);
	}
	if( (pTok->nType & PH7_TK_ID) == 0 ){
		return 0;
	}
	if( pTok->nType & PH7_TK_OP ){
		/* php's alpha-stream operators — `and`, `or`, `xor`, `new`, `clone`,
		 * `instanceof` — are keywords in its scanner and cannot be identifiers.
		 * They reach here carrying both flags, and were accepted as names. */
		return 0;
	}
	{
		/* Two more php keywords that PHL treats as context-sensitive identifiers. */
		static const char *const azNo[] = { "callable", "readonly" };
		sxu32 i;
		for( i = 0 ; i < SX_ARRAYSIZE(azNo) ; ++i ){
			if( GenStateNameIs(&pTok->sData,azNo[i]) ){
				return 0;
			}
		}
	}
	return 1;
}
/*
 * php's zend_is_reserved_class_name: a word its scanner DOES hand over as an
 * identifier but its compiler refuses to name a class with. The check is
 * case-insensitive and the refusal quotes the name as WRITTEN.
 */
PH7_PRIVATE int PH7_IsReservedClassName(const SyString *pName)
{
	static const char *const azReserved[] = {
		"bool", "int", "float", "string", "null", "false", "true", "void",
		"never", "iterable", "object", "mixed", "self", "parent", "static"
	};
	sxu32 i;
	for( i = 0 ; i < SX_ARRAYSIZE(azReserved) ; ++i ){
		if( GenStateNameIs(pName,azReserved[i]) ){
			return 1;
		}
	}
	return 0;
}
/*
 * Check if the given keyword is in fact a PHP language construct.
 * Return TRUE on success. FALSE otheriwse.
 */
static int GenStateisLangConstruct(sxu32 nKeyword)
{
	int rc;
	rc = PH7_IsLangConstruct(nKeyword,TRUE);
	if( rc == FALSE ){
		if( nKeyword == PH7_TKWRD_SELF || nKeyword == PH7_TKWRD_PARENT || nKeyword == PH7_TKWRD_STATIC
			|| nKeyword == PH7_TKWRD_YIELD
			/* `match` is an EXPRESSION, and php takes an expression statement made of
			 * one: `match (true) { ... };` is how a dispatch table is written when the
			 * answer is not wanted. Without this the statement dispatcher refused the
			 * keyword outright, and Doctrine's DQL parser -- which dispatches its tree
			 * walkers exactly that way -- did not compile. */
			|| nKeyword == PH7_TKWRD_MATCH
			/*|| nKeyword == PH7_TKWRD_CLASS || nKeyword == PH7_TKWRD_FINAL || nKeyword == PH7_TKWRD_EXTENDS
			  || nKeyword == PH7_TKWRD_ABSTRACT || nKeyword == PH7_TKWRD_INTERFACE
			  || nKeyword == PH7_TKWRD_PUBLIC || nKeyword == PH7_TKWRD_PROTECTED
			  || nKeyword == PH7_TKWRD_PRIVATE || nKeyword == PH7_TKWRD_IMPLEMENTS
			*/
			){
				rc = TRUE;
		}
	}
	return rc;
}
/*
 * Compile a PHP chunk.
 * If something goes wrong while compiling the PHP chunk,this function
 * takes care of generating the appropriate error message.
 */
/*
 * Update pGen->sPendingDoc for the statement whose first token is
 * pGen->pIn: when a docblock trivia is keyed to that token's index in
 * the chunk token set it becomes the pending docblock. An existing
 * pending docblock is LEFT in place otherwise: Zend keeps the last-seen
 * doc comment until a declaration consumes it, so a docblock survives
 * intervening non-declaration statements.
 */
PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)
{
	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);
	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);
	sxu32 nT = SySetUsed(&pGen->aTrivia);
	sxu32 nIdx, n;
	if( nT < 1 || pGen->pTokenSet == 0
	 || pGen->pIn < pBase || pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){
		/* Re-tokenized substream (string interpolation, synthesized code):
		 * indexes do not map to the sidecar */
		return;
	}
	nIdx = (sxu32)(pGen->pIn - pBase);
	/* Attributes must be adjacent to their declaration (unlike docblocks):
	 * reset at every boundary, then collect the groups keyed to this token. */
	SySetReset(&pGen->aPendingAttrs);
	for( n = 0 ; n < nT ; n++ ){
		if( aT[n].nTokIdx != nIdx ){
			continue;
		}
		if( aT[n].iKind == PH7_TRIVIA_DOC ){
			pGen->sPendingDoc = aT[n].sText;
		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){
			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);
		}
	}
}
/*
 * Hand the pending docblock (if any) to a declaration: duplicate it into
 * the VM allocator (the raw script buffer dies after compilation) and
 * clear the pending slot so sibling declarations do not inherit it.
 */
PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)
{
	char *zDup;
	if( SyStringLength(&pGen->sPendingDoc) < 1 ){
		return;
	}
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));
	if( zDup ){
		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));
	}
	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);
}
/*
 * Compile one recorded #[...] attribute group (the span between the group
 * delimiters) into ph7_attribute records appended to pOut. The span is
 * duplicated into the VM allocator FIRST (compiled bytecode and interned
 * names may point into the token text, which must outlive the raw script
 * buffer), then re-tokenized on its own. Each argument expression compiles
 * with the container-swap idiom into its own OP_DONE-terminated set,
 * evaluated lazily at ReflectionAttribute time (PHP semantics).
 */
static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)
{
	SySet *pToken;
	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;
	char *zSpan;
	sxi32 rc = SXRET_OK;
	if( SyStringLength(&pTrivia->sText) < 1 ){
		return SXRET_OK;
	}
	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,
		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));
	if( zSpan == 0 ){
		return SXRET_OK;
	}
	/* The token set must outlive compilation too: interned operands may
	 * reference token payloads. Pool-allocated, never released — bounded by
	 * the number of attribute declarations in the program. */
	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));
	if( pToken == 0 ){
		return SXRET_OK;
	}
	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));
	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);
	pIn = (SyToken *)SySetBasePtr(pToken);
	pEnd = &pIn[SySetUsed(pToken)];
	pSavedIn = pGen->pIn;
	pSavedEnd = pGen->pEnd;
	while( pIn < pEnd ){
		ph7_attribute sAttr;
		SyBlob sFQN;
		int bAbsolute = 0;
		SyZero(&sAttr,sizeof(sAttr));
		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));
		sAttr.nLine = pIn->nLine;
		if( pIn->nType & PH7_TK_NSSEP ){
			bAbsolute = 1;
			pIn++;
		}
		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
		/* `#[namespace\Attr]` — the current namespace, absolute from there. */
		if( !bAbsolute && GenStateNsRelPrefix(pGen,&pIn,pEnd,&sFQN) ){
			bAbsolute = 1;
		}
		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) ){
			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);
			pIn++;
			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){
				SyBlobAppend(&sFQN,"\\",1);
				pIn++;
				continue;
			}
			break;
		}
		if( SyBlobLength(&sFQN) < 1 ){
			/* Malformed group: stop quietly (the group was inert trivia before
			 * this feature; never turn it into a new fatal) */
			SyBlobRelease(&sFQN);
			break;
		}
		/* Resolve to an FQN: absolute names verbatim; else use-import alias,
		 * else current-namespace prefix (PHP attribute name resolution) */
		{
			const char *zName = (const char *)SyBlobData(&sFQN);
			sxu32 nName = SyBlobLength(&sFQN);
			char *zDup = 0;
			if( !bAbsolute ){
				/* An attribute name resolves exactly the way every other class name
				 * does -- the LEADING segment through the use imports, else the
				 * current-namespace prefix. This looked the WHOLE qualified string up
				 * in the import table, which can never match a single-segment alias,
				 * and then prefixed the namespace anyway: `use Vv as Rule;` with
				 * `#[Rule\\A]` asked for `App\\Rule\\A` and got
				 * `Attribute class ... not found`. It is the same mistake
				 * GenStateResolveName was written to fix for the other name positions,
				 * so it is that function's job here too. */
				SyBlob sTmp;
				SyString sRaw;
				SyStringInitFromBuf(&sRaw,zName,nName);
				SyBlobInit(&sTmp,&pGen->pVm->sAllocator);
				GenStateResolveName(&(*pGen),&sRaw,&sTmp);
				if( SyBlobLength(&sTmp) > 0 ){
					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));
					if( zDup ){
						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));
					}
				}
				SyBlobRelease(&sTmp);
			}
			if( SyStringLength(&sAttr.sName) < 1 ){
				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);
				if( zDup ){
					SyStringInitFromBuf(&sAttr.sName,zDup,nName);
				}
			}
		}
		SyBlobRelease(&sFQN);
		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){
			SyToken *pArgsEnd;
			pIn++;
			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);
			while( pIn < pArgsEnd ){
				SyToken *pArgStart = pIn, *pArgStop = pIn;
				sxi32 iDepth = 0;
				ph7_attr_arg sArgRec;
				while( pArgStop < pArgsEnd ){
					if( pArgStop->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
						iDepth++;
					}else if( pArgStop->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
						iDepth--;
					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){
						break;
					}
					pArgStop++;
				}
				SyZero(&sArgRec,sizeof(sArgRec));
				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID|PH7_TK_KEYWORD))
				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){
					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,
						pArgStart->sData.zString,pArgStart->sData.nByte);
					if( zN ){
						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);
					}
					pArgStart += 2;
				}
				if( pArgStart < pArgStop ){
					SySet *pInstrContainer;
					const char *zCErr;
					pGen->pIn = pArgStart;
					pGen->pEnd = pArgStop;
					/* An attribute argument is a constant expression -- php applies the
					 * same rules it applies to a class constant, `new` excepted (an
					 * attribute argument takes one). This is the argument's own rule,
					 * not the malformed-group case a few lines up, so it IS a fatal. */
					zCErr = PH7_GenStateConstExprError(pGen,1);
					if( zCErr ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,"%s",zCErr);
						pGen->pIn = pSavedIn;
						pGen->pEnd = pSavedEnd;
						return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
					}
					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);
					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);
					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
					if( rc == SXERR_ABORT ){
						pGen->pIn = pSavedIn;
						pGen->pEnd = pSavedEnd;
						return SXERR_ABORT;
					}
					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);
				}
				pIn = pArgStop;
				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){
					pIn++;
				}
			}
			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;
		}
		SySetPut(pOut,(const void *)&sAttr);
		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){
			pIn++;
			continue;
		}
		break;
	}
	pGen->pIn = pSavedIn;
	pGen->pEnd = pSavedEnd;
	return SXRET_OK;
}
/*
 * Hand the pending attribute groups (if any) to a declaration: compile
 * every recorded group into pOut and clear the pending list.
 */
PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)
{
	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);
	sxu32 n;
	sxi32 rc;
	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){
		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	SySetReset(&pGen->aPendingAttrs);
	return SXRET_OK;
}
/*
 * Compile the attribute groups keyed to the given token (a parameter's
 * first token inside a signature) into pOut. Parameters are parsed from
 * the main token stream, so the sidecar indexes map directly.
 */
PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)
{
	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);
	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);
	sxu32 nT = SySetUsed(&pGen->aTrivia);
	sxu32 nIdx, n;
	sxi32 rc;
	if( nT < 1 || pGen->pTokenSet == 0
	 || pTok < pBase || pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){
		return SXRET_OK;
	}
	nIdx = (sxu32)(pTok - pBase);
	for( n = 0 ; n < nT ; n++ ){
		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){
			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
	}
	return SXRET_OK;
}
/*
 * ---------------------------------------------------------------------------
 * Where php's OWN attributes may be written.
 *
 * php's seven internal attribute classes each carry a target mask and a
 * validator, and the engine runs them where the declaration COMPILES: a
 * misplaced `#[\Attribute]`, `#[\Override]` or `#[\NoDiscard]` is a fatal at
 * the line it sits on, before anything else in the file runs. A USERLAND
 * attribute is different — php checks its mask only when someone asks for it,
 * at `newInstance()` — so this table is closed on purpose and unknown names go
 * unchecked, which is php's behaviour and not an omission.
 *
 * The masks are the same seven the classes declare (see VmInstallAttributes);
 * they are repeated here because the compiler runs before any class exists.
 * None of the seven is IS_REPEATABLE, so a second one is php's own refusal.
 * ---------------------------------------------------------------------------
 */
/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */
static const char *const azGenAttrTarget[] = {
	"class","function","method","property","class constant","parameter","constant"
};
static const struct {
	const char *zName;
	int iMask;
} aGenInternalAttr[] = {
	{ "Attribute",              1  },
	{ "Deprecated",             87 },
	{ "AllowDynamicProperties", 1  },
	{ "SensitiveParameter",     32 },
	{ "ReturnTypeWillChange",   4  },
	{ "Override",               12 },
	{ "NoDiscard",              6  },
};
/*
 * The extra validator php gives three of them, asked only once the target is
 * known to be a CLASS: the mask says "a class" and these say WHICH kinds.
 * Answers php's noun for the refused kind, or 0 when the class is acceptable.
 */
static const char * GenStateAttrClassRefusal(const char *zAttr,sxi32 iFlags)
{
	/* zAttr is a row of aGenInternalAttr, so an exact compare is the whole test. */
	int bAttr = SyStrncmp(zAttr,"Attribute",sizeof("Attribute")) == 0;
	int bDyn  = SyStrncmp(zAttr,"AllowDynamicProperties",sizeof("AllowDynamicProperties")) == 0;
	int bDep  = SyStrncmp(zAttr,"Deprecated",sizeof("Deprecated")) == 0;
	if( !bAttr && !bDyn && !bDep ){
		return 0;
	}
	if( iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }
	if( iFlags & PH7_CLASS_ENUM ){ return "enum"; }
	if( iFlags & PH7_CLASS_TRAIT ){
		/* php 8.5 DOES mark a deprecated trait; the other two refuse one. */
		return bDep ? 0 : "trait";
	}
	if( bAttr ){
		/* An attribute class must be instantiable. */
		return (iFlags & PH7_CLASS_ABSTRACT) ? "abstract class" : 0;
	}
	if( bDyn ){
		/* A readonly class has no dynamic property to allow. */
		return (iFlags & PH7_CLASS_READONLY) ? "readonly class" : 0;
	}
	return "class";   /* #[\Deprecated] on any other class kind */
}
/*
 * Validate one declaration's attribute set against php's placement rules.
 *
 * iTarget is the single Attribute::TARGET_* bit php NAMES for this declaration
 * and iAccept the mask it accepts, which differ in exactly one place: a PROMOTED
 * constructor parameter is a parameter and a property both, so it takes either
 * bit while still reporting "parameter". pClassName/iClassFlags describe the
 * subject when the target is a class (0 and 0 otherwise).
 */
PH7_PRIVATE sxi32 GenStateCheckAttrPlacement(ph7_gen_state *pGen,SySet *pAttrs,
	int iTarget,int iAccept,const SyString *pClassName,sxi32 iClassFlags)
{
	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);
	sxu32 n,k;
	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){
		SyString *pName = &aAttr[n].sName;
		sxu32 iRow;
		for( iRow = 0 ; iRow < SX_ARRAYSIZE(aGenInternalAttr) ; ++iRow ){
			if( pName->nByte == (sxu32)SyStrlen(aGenInternalAttr[iRow].zName)
			 && SyStrnicmp(pName->zString,aGenInternalAttr[iRow].zName,pName->nByte) == 0 ){
				break;
			}
		}
		if( iRow >= SX_ARRAYSIZE(aGenInternalAttr) ){
			continue;   /* a userland attribute: judged at newInstance(), not here */
		}
		if( (aGenInternalAttr[iRow].iMask & iAccept) == 0 ){
			SyBlob sAllowed;
			int iBit;
			sxi32 rc;
			SyBlobInit(&sAllowed,&pGen->pVm->sAllocator);
			for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ; iBit++ ){
				if( (aGenInternalAttr[iRow].iMask & (1 << iBit)) == 0 ){
					continue;
				}
				if( SyBlobLength(&sAllowed) > 0 ){
					SyBlobAppend(&sAllowed,", ",sizeof(", ")-1);
				}
				SyBlobAppend(&sAllowed,azGenAttrTarget[iBit],
					(sxu32)SyStrlen(azGenAttrTarget[iBit]));
			}
			SyBlobAppend(&sAllowed,"",sizeof(char));   /* NUL for the %s below */
			for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ; iBit++ ){
				if( iTarget == (1 << iBit) ){
					break;
				}
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,
				"Attribute \"%z\" cannot target %s (allowed targets: %s)",pName,
				iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ? azGenAttrTarget[iBit] : "",
				SyBlobData(&sAllowed));
			SyBlobRelease(&sAllowed);
			return rc;
		}
		/* ...then repetition, which is what php checks second: the FIRST of a
		 * misplaced pair reports its target instead. */
		for( k = 0 ; k < n ; ++k ){
			if( aAttr[k].sName.nByte == pName->nByte
			 && SyStrnicmp(aAttr[k].sName.zString,pName->zString,pName->nByte) == 0 ){
				return PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,
					"Attribute \"%z\" must not be repeated",pName);
			}
		}
		if( iTarget == 1 && pClassName ){
			const char *zRefused = GenStateAttrClassRefusal(aGenInternalAttr[iRow].zName,
				iClassFlags);
			if( zRefused ){
				return PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,
					"Cannot apply #[\\%s] to %s %z",aGenInternalAttr[iRow].zName,
					zRefused,pClassName);
			}
		}
	}
	return SXRET_OK;
}
/*
 * php 8.5's `(void)` cast is a STATEMENT prefix, not an expression operator:
 * `$x = (void) f();` and `return (void) f();` are parse errors there too, and
 * the only thing it does is say that dropping the answer is DELIBERATE, which
 * silences a #[\NoDiscard] callee. The lexer already assembled the three tokens
 * into one (PH7_TK_VOID_CAST); this consumes it and answers 1.
 */
PH7_PRIVATE int GenStateTakeVoidCast(ph7_gen_state *pGen)
{
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_VOID_CAST) ){
		pGen->pIn++;
		return 1;
	}
	return 0;
}
/*
 * php's grammar takes a `(void)` cast at the head of an expression STATEMENT and
 * at the head of each element of a `for` clause list — `for ((void) f(), $i = 0;
 * $i < 1; $i++, (void) g())` is all valid, while `for ($i = (void) f();;)` is
 * not. The statement head is consumed by GenStateTakeVoidCast; a clause is one
 * expression with comma operators in it, so its element heads are marked HERE,
 * before it compiles: the token becomes the no-op cast operator parse.c declares,
 * and every other `(void)` in the clause stays unrecognized, which is php's own
 * refusal. Nothing is moved or removed — the token stream is shared with the
 * rest of the file.
 */
PH7_PRIVATE int GenStateEnableClauseVoidCasts(ph7_gen_state *pGen,int bLastToo)
{
	SyToken *pTok = pGen->pIn,*pLastMark = 0;
	int iDepth = 0,bHead = 1,bCommaAfter = 0;
	for( ; pTok < pGen->pEnd ; pTok++ ){
		if( pTok->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
			iDepth++;
		}else if( pTok->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
			iDepth--;
		}else if( iDepth == 0 && (pTok->nType & PH7_TK_SEMI) ){
			/* The three clauses share one token range (only the post one is
			 * delimited), so this scan stops where its own clause does. */
			break;
		}else if( iDepth == 0 && (pTok->nType & PH7_TK_COMMA) ){
			bHead = 1;
			bCommaAfter = 1;
			continue;
		}else if( iDepth == 0 && bHead && (pTok->nType & PH7_TK_VOID_CAST) ){
			pTok->nType |= PH7_TK_OP;
			pTok->pUserData = (void *)PH7_ExprExtractOperator(&pTok->sData,0);
			pLastMark = pTok;
			bCommaAfter = 0;
		}
		bHead = 0;
	}
	/* The CONDITION clause's last element is the condition VALUE, so php refuses a
	 * `(void)` on that one and only that one: `for (;(void) f();)` is a parse error
	 * where `for (;(void) f(), $i < 1;)` is fine. */
	if( !bLastToo && pLastMark && !bCommaAfter ){
		pLastMark->nType &= ~(sxu32)PH7_TK_OP;
		pLastMark->pUserData = 0;
		return 1;
	}
	return 0;
}
/*
 * The statement is about to throw its expression's value away. When that value
 * came straight out of a CALL, mark the call: php's !RETURN_VALUE_USED, which is
 * what a #[\NoDiscard] callee reads. `f() + 1;` drops the ADD's result, not the
 * call's, so only the last instruction is looked at.
 */
PH7_PRIVATE void GenStateMarkDiscardedCall(ph7_gen_state *pGen)
{
	VmInstr *pInstr = PH7_VmPeekInstr(pGen->pVm);
	if( pInstr && pInstr->iOp == PH7_OP_CALL ){
		pInstr->bDiscard = 1;
	}
}
/* TRUE when the cursor has run past the LAST token of a chunk that met the end of
 * the file. A statement slice can end early (a single statement inside a `for`
 * header), so `pIn >= pEnd` alone is not the question. */
static int GenStateAtChunkEof(ph7_gen_state *pGen)
{
	SyToken *pBase;
	if( !pGen->bChunkAtEof || pGen->pTokenSet == 0 ){
		return 0;
	}
	pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);
	return pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)];
}
/*
 * php's grammar wants a TERMINATOR after a statement, and the end of the file is
 * not one: `<?php echo "a"` is `syntax error, unexpected end of file, expecting
 * "," or ";"` there, while PHL ran it and exited 0. (A `?>` IS a terminator, which
 * is why `<?php echo "a" ?>` is legal in both engines and why the check only
 * applies to a chunk that met the end of the FILE.)
 *
 * A statement that ends in `}` -- a block, a declaration, a braced control
 * structure -- needs nothing, and neither does one whose `;` the loop just
 * stepped over; everything else was left unfinished.
 *
 * Answers the "expecting" clause php names for the statement that ran out, or 0
 * when php names none. php reports the set its parser was in, which for a
 * statement is decided by the KEYWORD it opened with -- the comma-list statements
 * may take another element, `return`/`break`/`continue`/`goto`/`unset` and a
 * do-while may not, `namespace` still wants its block -- except when the last
 * token consumed was an alternative-syntax `end*`, whose own `;` is what is
 * missing.
 */
static const char * GenStateEofExpecting(SyToken *pStmt,SyToken *pLast)
{
	sxu32 nKw;
	if( pLast && (pLast->nType & PH7_TK_KEYWORD) ){
		nKw = (sxu32)SX_PTR_TO_INT(pLast->pUserData);
		if( nKw == PH7_TKWRD_ENDIF || nKw == PH7_TKWRD_ENDWHILE || nKw == PH7_TKWRD_ENDFOR
		 || nKw == PH7_TKWRD_END4EACH || nKw == PH7_TKWRD_ENDSWITCH ){
			return "\";\"";
		}
	}
	if( pStmt == 0 || (pStmt->nType & PH7_TK_KEYWORD) == 0 ){
		return 0;
	}
	nKw = (sxu32)SX_PTR_TO_INT(pStmt->pUserData);
	if( nKw == PH7_TKWRD_ECHO || nKw == PH7_TKWRD_GLOBAL || nKw == PH7_TKWRD_STATIC
	 || nKw == PH7_TKWRD_CONST || nKw == PH7_TKWRD_USE ){
		return "\",\" or \";\"";
	}
	if( nKw == PH7_TKWRD_RETURN || nKw == PH7_TKWRD_BREAK || nKw == PH7_TKWRD_CONTINUE
	 || nKw == PH7_TKWRD_GOTO || nKw == PH7_TKWRD_UNSET || nKw == PH7_TKWRD_DO ){
		return "\";\"";
	}
	if( nKw == PH7_TKWRD_NAMESPACE ){
		return "\"{\"";
	}
	return 0;
}
/* Does this script spell `__halt_compiler` at all, in any case? A cheap scan
 * that keeps the token pass below off every ordinary file. */
static int GenStateMentionsHalt(const char *zIn,sxu32 nIn)
{
	static const char zWord[] = "__halt_compiler";
	sxu32 nWord = (sxu32)sizeof(zWord)-1;
	sxu32 i;
	for( i = 0 ; i + nWord <= nIn ; ++i ){
		if( zIn[i] != '_' ){
			continue;
		}
		if( SyStrnicmp(&zIn[i],zWord,nWord) == 0 ){
			return 1;
		}
	}
	return 0;
}
/* Is this token the `__halt_compiler` identifier? It is not a keyword in this
 * lexer (the generated table takes nothing longer than twelve bytes), so it
 * arrives as an ordinary identifier and is recognised by NAME -- case
 * insensitively, as php's own scanner does. */
static int GenStateIsHaltCompiler(SyToken *pTok,SyToken *pEnd)
{
	return pTok < pEnd
	    && (pTok->nType & PH7_TK_ID)
	    && pTok->sData.nByte == sizeof("__halt_compiler")-1
	    && SyStrnicmp(pTok->sData.zString,"__halt_compiler",sizeof("__halt_compiler")-1) == 0;
}
/*
 * The pre-scan behind `__COMPILER_HALT_OFFSET__`: find the halt statement in
 * whichever PHP chunk holds it and remember the byte just past its `;`. The
 * chunks are tokenized a second time here -- the compile below tokenizes each
 * one as it reaches it -- because the constant's value has to be known before
 * the first statement compiles. Only a file that spells the identifier gets
 * here at all.
 *
 * A `__halt_compiler` in a scope php refuses is still found: the statement
 * compiler raises php's fatal when it reaches it, and the offset is never read.
 */
static void GenStateScanHaltOffset(ph7_gen_state *pGen,SySet *pRawToken,const char *zFileBase)
{
	SyToken *pRaw = (SyToken *)SySetBasePtr(pRawToken);
	SyToken *pRawEnd = &pRaw[SySetUsed(pRawToken)];
	SySet aTok,aTriv;
	SySetInit(&aTok,&pGen->pVm->sAllocator,sizeof(SyToken));
	SySetInit(&aTriv,&pGen->pVm->sAllocator,sizeof(SyToken));
	for( ; pRaw < pRawEnd && pGen->bHaltSeen == 0 ; pRaw++ ){
		SyToken *pTok,*pEnd;
		if( (pRaw->nType & PH7_TOKEN_PHP) == 0 ){
			continue;
		}
		SySetReset(&aTok);
		SySetReset(&aTriv);
		PH7_TokenizePHP(SyStringData(&pRaw->sData),SyStringLength(&pRaw->sData),
			pRaw->nLine,&aTok,&aTriv);
		pTok = (SyToken *)SySetBasePtr(&aTok);
		pEnd = &pTok[SySetUsed(&aTok)];
		for( ; pTok < pEnd ; pTok++ ){
			if( !GenStateIsHaltCompiler(pTok,pEnd) ){
				continue;
			}
			if( pTok + 3 < pEnd
			 && (pTok[1].nType & PH7_TK_LPAREN)
			 && (pTok[2].nType & PH7_TK_RPAREN)
			 && (pTok[3].nType & PH7_TK_SEMI) ){
				const char *zSemi = SyStringData(&pTok[3].sData);
				if( zSemi > zFileBase ){
					pGen->nHaltOffset = (sxu32)((zSemi - zFileBase) + 1);
					pGen->bHaltSeen = 1;
				}
			}
			break;
		}
	}
	SySetRelease(&aTok);
	SySetRelease(&aTriv);
}
/*
 * ---------------------------------------------------------------------------
 * `__halt_compiler();`
 *
 * php's scanner STOPS at it: the rest of the file is not code and is never
 * output either, which is what lets a .phar carry a binary archive in the bytes
 * behind its stub. Three rules come with it, all php's:
 *
 *   - it is only legal at the OUTERMOST scope -- inside a function, a class or
 *     even a plain `if` block it is a compile-time fatal, not a parse error;
 *   - the parentheses and the semicolon are part of the construct, and php's
 *     parser names what it wanted when one is missing;
 *   - `__COMPILER_HALT_OFFSET__` expands to the byte just past that `;`.
 *
 * It is NOT a keyword in this lexer (the generated table takes nothing longer
 * than twelve bytes), so it arrives as an ordinary identifier in statement
 * position and is recognised by name -- case-insensitively, as php does.
 * ---------------------------------------------------------------------------
 */
static sxi32 GenStateCompileHaltCompiler(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	if( pGen->pCurrent != &pGen->sGlobal ){
		/* php's own sentence: a FATAL rather than a parse error, and one its
		 * PARSER makes -- so it prints no stack trace under it. */
		pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;
		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"__HALT_COMPILER() can only be used from the outermost scope");
	}
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		return PH7_GenSyntaxError(&(*pGen),
			pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");
	}
	/* php's scanner ran out INSIDE the parentheses: it names the `(` it never
	 * closed rather than the token it wanted, and reports it where the INPUT
	 * ends rather than where the `(` is. */
#define PHL_HALT_EOF_LINE (pGen->bChunkAtEof && pGen->nChunkEofLine > nLine \
	? pGen->nChunkEofLine : nLine)
	if( pGen->pIn >= pGen->pEnd ){
		return PH7_GenCompileError(&(*pGen),E_PARSE,PHL_HALT_EOF_LINE,
			"Unclosed '(' on line %u",nLine);
	}
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){
		return pGen->pIn >= pGen->pEnd
			? PH7_GenCompileError(&(*pGen),E_PARSE,PHL_HALT_EOF_LINE,
				"Unclosed '(' on line %u",nLine)
			: PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\")\"");
	}
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		return PH7_GenSyntaxError(&(*pGen),
			pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");
	}
	pGen->pIn++;
	/* Nothing after it is code, in this chunk or in any that follows. */
	pGen->pIn = pGen->pEnd;
	pGen->bHalted = 1;
#undef PHL_HALT_EOF_LINE
	return SXRET_OK;
}
PH7_PRIVATE sxi32 GenStateCompileChunk(
	ph7_gen_state *pGen, /* Code generator state */
	sxi32 iFlags         /* Compile flags */
	)
{
	ProcLangConstruct xCons;
	sxi32 rc;
	rc = SXRET_OK; /* Prevent compiler warning */
	for(;;){
		int bStmtIsDeclare = 0;
		/* Whether php's grammar wants a TERMINATOR after this statement: a block, a
		 * declaration and a LABEL end themselves, everything else -- an expression
		 * statement included, even one that ends in the `}` of a closure or a match
		 * -- has to be closed. */
		int bStmtWantsSemi = 1;
		SyToken *pStmtStart;
		if( pGen->pIn >= pGen->pEnd ){
			/* No more input to process */
			break;
		}
		pStmtStart = pGen->pIn; /* The keyword this statement opened with, for the
		                         * end-of-input check below */
		/* Bind a directly-preceding docblock to this statement */
		GenStateSetPendingDoc(&(*pGen));
		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){
			/* php: a statement-position attribute group must be followed by a
			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a
			 * parse error, never a silent discard. `static`/`fn`/`function`
			 * cover bare closure-expression statements; `readonly`/`enum` are
			 * context-sensitive IDs handled by the modified-class/enum scans. */
			int bAttrTarget = 0;
			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)
			 || GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){
				bAttrTarget = 1;
			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){
				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
				if( nKw == PH7_TKWRD_FUNCTION || nKw == PH7_TKWRD_CLASS
				 || nKw == PH7_TKWRD_INTERFACE || nKw == PH7_TKWRD_TRAIT
				 || nKw == PH7_TKWRD_ABSTRACT || nKw == PH7_TKWRD_FINAL
				 || nKw == PH7_TKWRD_CONST || nKw == PH7_TKWRD_STATIC
				 || nKw == PH7_TKWRD_FN ){
					bAttrTarget = 1;
				}
			}
			if( !bAttrTarget ){
				rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,
					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",
					&pGen->pIn->sData);
				if( rc == SXERR_ABORT ){
					break;
				}
				SySetReset(&pGen->aPendingAttrs);
			}
		}
		/* Peek to detect a top-level `declare` so the strict_types lock
		 * below doesn't fire before the directive has a chance to run. */
		if( pGen->pIn->nType & PH7_TK_KEYWORD ){
			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nPeek == PH7_TKWRD_DECLARE ){
				bStmtIsDeclare = 1;
			}
		}
		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){
			/* Any non-declare top-level statement locks the strict_types
			 * directive: it's now too late for declare(strict_types=1). */
			pGen->bStrictTypesLocked = 1;
		}
		if( GenStateIsHaltCompiler(pGen->pIn,pGen->pEnd) ){
			/* Everything from here on is DATA. php's own scanner stops in exactly
			 * the same place, which is what lets a .phar carry its archive in the
			 * bytes after its stub. */
			rc = GenStateCompileHaltCompiler(&(*pGen));
			break;
		}
		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){
			/* Compile block */
			bStmtWantsSemi = 0;
			rc = PH7_CompileBlock(&(*pGen),0);
			if( rc == SXERR_ABORT ){
				break;
			}
		}else{
			xCons = 0;
			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){
				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled
				 * here rather than the keyword-only dispatcher because `readonly`
				 * is a context-sensitive ID and combos need a full-run scan. */
				xCons = PH7_CompileClassModifiers;
			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){
				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,
				 * so it is detected here rather than the keyword dispatcher. */
				xCons = PH7_CompileEnum;
			}else if( GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){
				/* A statement that STARTS with php's `namespace\X` name operator
				 * (`namespace\Cee::m();`) is an expression, not a namespace
				 * DECLARATION — the glued `\` is what tells the two apart. */
				xCons = 0;
			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){
				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
				/* Try to extract a language construct handler */
				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);
				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){
					rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,
						"Syntax error: Unexpected keyword '%z'",
						&pGen->pIn->sData);
					if( rc == SXERR_ABORT ){
						break;
					}
					/* Synchronize with the first semi-colon and avoid compiling
					 * this erroneous statement.
					 */
					xCons = PH7_ErrorRecover;
				}
			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)
				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){
				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */
				xCons = PH7_CompileLabel;
			}
			if( xCons == 0 ){
				/* Assume an expression an try to compile it. A leading php 8.5
				 * `(void)` cast is consumed here — statement head is one of the two
				 * places its grammar takes one — and says the answer is dropped
				 * DELIBERATELY, so the call below is not marked. */
				int bVoid = GenStateTakeVoidCast(&(*pGen));
				if( bVoid && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){
					/* php's grammar wants an expression after the cast: `(void);`
					 * is `syntax error, unexpected token ";"` there. */
					rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,
						"syntax error, unexpected token \";\"");
				}else{
					rc = PH7_CompileExpr(&(*pGen),0,0);
					if( rc != SXERR_EMPTY ){
						if( !bVoid ){
							GenStateMarkDiscardedCall(&(*pGen));
						}
						/* Pop l-value */
						PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
					}
				}
			}else{
				/* Go compile the sucker */
				rc = xCons(&(*pGen));
				if( xCons == PH7_CompileLabel
				 || ( pGen->pTokenSet
				   && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet)
				   && (pGen->pIn[-1].nType & PH7_TK_CCB/*'}'*/) ) ){
					/* A label, and any construct that ends with its own block, close
					 * themselves. (An EXPRESSION statement never does, which is why
					 * this asks the construct and not just the last token: the `}` of
					 * `$f = function () {}` is not a terminator.) */
					bStmtWantsSemi = 0;
				}
			}
			if( rc == SXERR_ABORT ){
				/* Request to abort compilation */
				break;
			}
		}
		/* Ignore trailing semi-colons ';' */
		{
			/* Terminated when a `;` is sitting there for the loop to step over, or
			 * when the construct consumed its own (the alternative-syntax bodies
			 * take the `;` after their `endif`/`endwhile`/… themselves). */
			int bTerminated = (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI)) != 0;
			if( !bTerminated && pGen->pTokenSet
			 && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet)
			 && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){
				bTerminated = 1;
			}
			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){
				pGen->pIn++;
			}
			if( !bTerminated && bStmtWantsSemi && pGen->nErr < 1 && GenStateAtChunkEof(&(*pGen))
			 && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ){
				/* (GenStateAtChunkEof already answered no for a NULL token set.) */
				/* Ran out of input with the statement still open. */
				rc = PH7_GenSyntaxError(&(*pGen),0,GenStateEofExpecting(pStmtStart,&pGen->pIn[-1]));
				if( rc == SXERR_ABORT ){
					break;
				}
			}
		}
		if( iFlags & PH7_COMPILE_SINGLE_STMT ){
			/* Compile a single statement and return */
			break;
		}
		/* LOOP ONE */
		/* LOOP TWO */
		/* LOOP THREE */
		/* LOOP FOUR */
	}
	/* Return compilation status */
	return rc;
}
/*
 * TRUE when the SOURCE bytes of a double-quoted string interpolate -- `$name`,
 * `${`, or `{$` -- which is what decides whether php's scanner produced ONE
 * string token for it or an opening quote followed by parts. A backslash escapes
 * whatever follows it, so `"\\$b"` does not interpolate.
 */
static int GenStateDqInterpolates(SyString *pStr)
{
	const unsigned char *z = (const unsigned char *)pStr->zString;
	const unsigned char *zEnd = &z[pStr->nByte];
	while( z < zEnd ){
		if( z[0] == '\\' ){
			z += 2;
			continue;
		}
		if( z[0] == '$' && &z[1] < zEnd
		 && (z[1] == '{' || z[1] >= 0x80 || SyisAlpha(z[1]) || z[1] == '_') ){
			return 1;
		}
		if( z[0] == '{' && &z[1] < zEnd && z[1] == '$' ){
			return 1;
		}
		z++;
	}
	return 0;
}
/*
 * Compile a Raw PHP chunk.
 * If something goes wrong while compiling the PHP chunk,this function
 * takes care of generating the appropriate error message.
 */
static sxi32 PH7_CompilePHP(
	ph7_gen_state *pGen,  /* Code generator state */
	SySet *pTokenSet,     /* Token set */
	int is_expr           /* TRUE if we are dealing with a simple expression */
	)
{
	SyToken *pScript = pGen->pRawIn; /* Script to compile */
	sxi32 rc;
	/* Reset the token set (and its trivia sidecar) */
	SySetReset(&(*pTokenSet));
	SySetReset(&pGen->aTrivia);
	/* Mark as the default token set */
	pGen->pTokenSet = &(*pTokenSet);
	/* Advance the stream cursor */
	pGen->pRawIn++;
	/* Tokenize the PHP chunk first */
	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);
	/* The raw tokenizer marked whether this chunk was closed by a `?>`; only one
	 * that met the end of the FILE can leave a statement unterminated. */
	pGen->bChunkAtEof = (sxi8)(SX_PTR_TO_INT(pScript->pUserData) == 0);
	/* Point to the head and tail of the token stream. */
	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);
	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];
	/* php REMOVED `(real)` in its SCANNER, so the refusal belongs to the chunk and
	 * not to the expression the cast sits in: `strlen(real)` -- where the token is
	 * never an operator at all -- reports the same sentence, and reports it as a
	 * PARSE error rather than the fatal `(unset)` gets from the compiler. */
	{
		SyToken *pTok;
		sxi32 nBraceOpen = 0;
		for( pTok = pGen->pIn ; pTok < pGen->pEnd ; pTok++ ){
			if( pTok->nType & PH7_TK_OCB ){
				nBraceOpen++;
			}else if( pTok->nType & PH7_TK_CCB ){
				nBraceOpen--;
			}
		}
		if( nBraceOpen > 0 ){
			/* A `{` this chunk never closes: php's parser reports THAT at the end of
			 * the file, ahead of any statement it left open and ahead of a string or
			 * heredoc the scanner was still inside. Stand the end-of-input questions
			 * down and let the block compiler say its own `Unclosed '{'`. */
			pGen->bChunkAtEof = 0;
		}
		for( pTok = pGen->pIn ; pTok < pGen->pEnd ; pTok++ ){
			if( (pTok->nType & PH7_TK_OP) && pTok->sData.nByte == sizeof("(real)")-1
			 && SyMemcmp((const void *)pTok->sData.zString,(const void *)"(real)",sizeof("(real)")-1) == 0 ){
				return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,
					"The (real) cast has been removed, use (float) instead");
			}
			if( (pTok->nType & PH7_TK_UNTERM)
			 && (pTok->nType & (PH7_TK_DSTR|PH7_TK_HEREDOC|PH7_TK_NOWDOC))
			 && pGen->bChunkAtEof == 0 ){
				/* php's parser reaches the end of file with the string still open and
				 * reports the UNCLOSED BRACE first -- the scanner is mid-interpolation
				 * there and has nothing of its own to say. (A single-quoted string and
				 * a block comment do: their sentences win over the brace, which is why
				 * only these three yield.) Leave it to the compile below. */
				continue;
			}
			if( pTok->nType & PH7_TK_UNTERM ){
				/* A quote, heredoc or block comment the input ran out under. php
				 * refuses the file for each; this used to take the rest of it as the
				 * lexeme's body and RUN the program (`<?php echo 'a` printed `a`).
				 * The wording is php's own per shape -- its scanner reports what it
				 * was still waiting for. */
				if( pTok->nType & PH7_TK_SSTR ){
					/* php's single-quoted scanner hands the parser the CONTENT it
					 * had read, and the parser names that. */
					return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,
						"syntax error, unexpected string content \"%z\"",&pTok->sData);
				}
				if( pTok->nType & PH7_TK_DSTR ){
					const char *zExp = pTok->sData.nByte < 1
						? "variable or string content or \"${\" or \"{$\""
						: (GenStateDqInterpolates(&pTok->sData) ? 0
						                                       : "variable or \"${\" or \"{$\"");
					return zExp
						? PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,
							"syntax error, unexpected end of file, expecting %s",zExp)
						: PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,
							"syntax error, unexpected end of file");
				}
				if( pTok->nType & (PH7_TK_HEREDOC|PH7_TK_NOWDOC) ){
					/* An EMPTY body, or one that interpolates, leaves php's parser
					 * with nothing to expect but the end it just met. */
					int bSet = pTok->sData.nByte > 0
						&& !((pTok->nType & PH7_TK_HEREDOC) && GenStateDqInterpolates(&pTok->sData));
					return bSet
						? PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,
							"syntax error, unexpected end of file, "
							"expecting variable or heredoc end or \"${\" or \"{$\"")
						: PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,
							"syntax error, unexpected end of file");
				}
				/* A block comment, whose sentence names where it began. */
				return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,
					"Unterminated comment starting line %u",pTok->nLine);
			}
		}
	}
	if( is_expr ){
		rc = SXERR_EMPTY;
		if( pGen->pIn < pGen->pEnd ){
			/* A simple expression,compile it */
			rc = PH7_CompileExpr(pGen,0,0);
		}
		/* Emit the DONE instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
		return SXRET_OK;
	}
	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){
		static const sxu32 nKeyID = PH7_TKWRD_ECHO;
		/*
		 * Shortcut syntax for the 'echo' language construct.
		 * According to the PHP reference manual:
		 *  echo() also has a shortcut syntax, where you can
		 *  immediately follow
		 *  the opening tag with an equals sign as follows:
		 *  <?= 4+5?> is the same as <?echo 4+5?>
		 * Symisc extension:
		 *   This short syntax works with all PHP opening
		 *   tags unlike the default PHP engine that handle
		 *   only short tag.
		 */
		/* Ticket 1433-009: Emulate the 'echo' call */
		pGen->pIn->nType = PH7_TK_KEYWORD;
		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);
		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);
		/* This synthesized echo is compiled as an EXPRESSION, which is otherwise a
		 * parse error; allow it for the duration of this one compile. */
		pGen->nExprEchoOk++;
		rc = PH7_CompileExpr(pGen,0,0);
		pGen->nExprEchoOk--;
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( rc != SXERR_EMPTY ){
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
		return SXRET_OK;
	}
	/* Compile the PHP chunk */
	rc = GenStateCompileChunk(pGen,0);
	/* Fix exceptions jumps */
	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));
	/* Fix gotos now, the jump destination is resolved */
	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){
		rc = SXERR_ABORT;
	}
	/* Reset container */
	SySetReset(&pGen->aGoto);
	SySetReset(&pGen->aLabel);
	SySetReset(&pGen->aNullsafeJmp);
	/* Compilation result */
	return rc;
}
/*
 * Compile a raw chunk. The raw chunk can contain PHP code embedded
 * in HTML, XML and so on. This function handle all the stuff.
 * This is the only compile interface exported from this file.
 */
PH7_PRIVATE sxi32 PH7_CompileScript(
	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */
	SyString *pScript,  /* Script to compile */
	sxi32 iFlags        /* Compile flags */
	)
{
	SySet aPhpToken,aRawToken;
	ph7_gen_state *pCodeGen;
	ph7_value *pRawObj;
	sxu32 nObjIdx;
	sxi32 nRawObj;
	int is_expr;
	sxi8 bSavedStrict;
	sxi8 bSavedStrictLocked;
	sxi8 bSavedHalted,bSavedHaltSeen;
	sxu32 nSavedHaltOffset;
	const char *zSavedScriptBase;
	const char *zFileBase;
	SyToken *pSavedIn,*pSavedEnd;
	sxi32 rc;
	sxu32 nBaseLine = 1;
	if( pScript->nByte < 1 ){
		/* Nothing to compile */
		return PH7_OK;
	}
	/* Kept before the shebang skip below: php counts __COMPILER_HALT_OFFSET__
	 * from the first byte on DISK, shebang line included. */
	zFileBase = pScript->zString;
	/* php skips a "#!" shebang on the first line of a CLI script: consume it
	 * (including its newline) so it is not echoed as inline text, and bump the
	 * base line to 2 so the code below still reports php-matching line numbers. */
	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){
		const char *z = pScript->zString;
		const char *zEnd = &z[pScript->nByte];
		while( z < zEnd && z[0] != '\n' ){ z++; }
		if( z < zEnd ){ z++; } /* consume the newline too */
		pScript->nByte -= (sxu32)(z - pScript->zString);
		pScript->zString = z;
		nBaseLine = 2;
		if( pScript->nByte < 1 ){
			return PH7_OK;
		}
	}
	/* Each compiled file has its own strict_types scope. Save the outer
	 * file's flags so include/require restore them on return. */
	pCodeGen = &pVm->sCodeGen;
	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any
	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp
	 * each instruction's source line, and instructions are still emitted after this
	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught
	 * immediately. Save the caller's cursor and restore it on the way out, so an
	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */
	pSavedIn = pCodeGen->pIn;
	pSavedEnd = pCodeGen->pEnd;
	bSavedStrict = pCodeGen->bStrictTypes;
	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;
	pCodeGen->bStrictTypes = 0;
	pCodeGen->bStrictTypesLocked = 0;
	/* The halt is per-FILE too, and an include compiles inside its includer. */
	bSavedHalted = pCodeGen->bHalted;
	bSavedHaltSeen = pCodeGen->bHaltSeen;
	nSavedHaltOffset = pCodeGen->nHaltOffset;
	zSavedScriptBase = pCodeGen->zScriptBase;
	pCodeGen->bHalted = 0;
	pCodeGen->bHaltSeen = 0;
	pCodeGen->nHaltOffset = 0;
	pCodeGen->zScriptBase = zFileBase;
	/* Initialize the tokens containers */
	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));
	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));
	SySetAlloc(&aPhpToken,0xc0);
	is_expr = 0;
	if( iFlags & PH7_PHP_ONLY ){
		SyToken sTmp;
		/* PHP only: -*/
		sTmp.nLine = 1;
		sTmp.nType = PH7_TOKEN_PHP;
		sTmp.pUserData = 0;
		SyStringDupPtr(&sTmp.sData,pScript);
		SySetPut(&aRawToken,(const void *)&sTmp);
		if( iFlags & PH7_PHP_EXPR ){
			/* A simple PHP expression */
			is_expr = 1;
		}
	}else{
		/* Tokenize raw text */
		SySetAlloc(&aRawToken,32);
		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);
	}
	/* Where the end of INPUT sits. php reports `unexpected end of file` at the line
	 * the file ENDS on, which is past the last token whenever anything follows it --
	 * a trailing newline always does -- and the chunk a statement was cut off in
	 * does not carry that: the raw splitter hands it the source up to its last byte
	 * of code, newline excluded. Counted here, where the whole script is still in
	 * hand, and read back by PH7_GenSyntaxError's end-of-file branch. */
	{
		sxu32 i;
		pCodeGen->nChunkEofLine = nBaseLine;
		for( i = 0 ; i < pScript->nByte ; ++i ){
			if( pScript->zString[i] == '\n' ){
				pCodeGen->nChunkEofLine++;
			}
		}
	}
	/* Process high-level tokens */
	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);
	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];
	/*
	 * Where `__halt_compiler();` sits, decided BEFORE anything compiles: the
	 * constant it defines may be read ahead of the statement that sets it (and
	 * in an earlier chunk than the one holding it), exactly as it may under php,
	 * whose compiler registers the constant for the whole file. The scan costs a
	 * second tokenization of every PHP chunk, so it only runs when the file
	 * contains the identifier at all -- which no ordinary program does.
	 */
	if( GenStateMentionsHalt(pScript->zString,pScript->nByte) ){
		GenStateScanHaltOffset(pCodeGen,&aRawToken,zFileBase);
	}
	rc = PH7_OK;
	if( is_expr ){
		/* Compile the expression */
		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);
		goto cleanup;
	}
	nObjIdx = 0;
	/* Start the compilation process */
	for(;;){
		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){
			break; /* No more tokens to process */
		}
		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){
			/* Compile the PHP chunk */
			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);
			if( rc == SXERR_ABORT ){
				break;
			}
			if( pCodeGen->bHalted ){
				/* `__halt_compiler();` -- the rest of the FILE is data, inline
				 * text between later chunks included. */
				break;
			}
			continue;
		}
		/* Raw chunk: [i.e: HTML, XML, etc.] */
		nRawObj = 0;
		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){
			/* Consume the raw chunk without any processing */
			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);
			if( pRawObj == 0 ){
				rc = SXERR_MEM;
				break;
			}
			/* Mark as constant and emit the load constant instruction */
			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);
			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);
			++nRawObj;
			pCodeGen->pRawIn++; /* Next chunk */
		}
		if( nRawObj > 0 ){
			/* Emit the consume instruction */
			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);
		}
	}
cleanup:
	/* Drop the cursor into the token set BEFORE the set is freed (see above). */
	pCodeGen->pIn = pSavedIn;
	pCodeGen->pEnd = pSavedEnd;
	SySetRelease(&aRawToken);
	SySetRelease(&aPhpToken);
	/* Restore outer file's strict_types scope */
	pCodeGen->bStrictTypes = bSavedStrict;
	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;
	/* ...and its halt state. */
	pCodeGen->bHalted = bSavedHalted;
	pCodeGen->bHaltSeen = bSavedHaltSeen;
	pCodeGen->nHaltOffset = nSavedHaltOffset;
	pCodeGen->zScriptBase = zSavedScriptBase;
	return rc;
}
/*
 * Utility routines.Initialize the code generator.
 */
PH7_PRIVATE sxi32 PH7_InitCodeGenerator(
	ph7_vm *pVm,       /* Target VM */
	ProcConsumer xErr, /* Error log consumer callabck  */
	void *pErrData     /* Last argument to xErr() */
	)
{
	ph7_gen_state *pGen = &pVm->sCodeGen;
	/* Zero the structure */
	SyZero(pGen,sizeof(ph7_gen_state));
	/* Initial state */
	pGen->pVm  = &(*pVm);
	pGen->xErr = xErr;
	pGen->pErrData = pErrData;
	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));
	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));
	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));
	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));
	pGen->nLoopId = pGen->nCurLoopId = 0;
	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));
	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));
	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));
	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);
	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);
	/* Error log buffer */
	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);
	SyBlobInit(&pGen->sFirstErr,&pVm->sAllocator);
	/* General purpose working buffer */
	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);
	/* Namespace state */
	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);
	GenStateInitUseImports(&(*pGen),&(*pVm));
	GenStateInitSeenSymbols(&(*pGen),&(*pVm));
	/* Create the global scope */
	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);
	/* Point to the global scope */
	pGen->pCurrent = &pGen->sGlobal;
	return SXRET_OK;
}
/*
 * Utility routines. Reset the code generator to it's initial state.
 */
PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(
	ph7_vm *pVm,       /* Target VM */
	ProcConsumer xErr, /* Error log consumer callabck  */
	void *pErrData     /* Last argument to xErr() */
	)
{
	ph7_gen_state *pGen = &pVm->sCodeGen;
	GenBlock *pBlock,*pParent;
	/* Reset state */
	SySetReset(&pGen->aLabel);
	SySetReset(&pGen->aGoto);
	SySetReset(&pGen->aNullsafeJmp);
	SySetReset(&pGen->aTrivia);
	SySetReset(&pGen->aPendingAttrs);
	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);
	SyBlobRelease(&pGen->sErrBuf);
	SyBlobRelease(&pGen->sFirstErr);
	SyBlobRelease(&pGen->sWorker);
	SyBlobRelease(&pGen->sNamespace);
	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);
	GenStateResetUseImports(&(*pGen),&(*pVm));
	/* A fresh compile unit has declared nothing yet. */
	GenStateResetSeenSymbols(&(*pGen),&(*pVm));
	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.
	 * They intern variable names and literal strings that are referenced by
	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).
	 * Releasing them would either leak the interned strings or require freeing
	 * memory still in use.  The entries use pool memory but are bounded by the
	 * number of unique names, which is acceptable. */
	/* Point to the global scope */
	pBlock = pGen->pCurrent;
	while( pBlock->pParent != 0 ){
		pParent = pBlock->pParent;
		GenStateFreeBlock(pBlock);
		pBlock = pParent;
	}
	pGen->xErr = xErr;
	pGen->pErrData = pErrData;
	pGen->pCurrent = &pGen->sGlobal;
	pGen->pRawIn = pGen->pRawEnd = 0;
	pGen->pIn = pGen->pEnd = 0;
	pGen->nErr = 0;
	pGen->nFatal = 0;
	pGen->nFirstErrLine = 0;
	pGen->bParseThrows = 0;
	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;
	/* Clear the class-body context (a prior compile aborted mid-class-body would
	 * otherwise leave these live for the next eval/include on this VM). */
	pGen->pCurClass = 0;
	pGen->iInMemberDefault = 0;
	return SXRET_OK;
}
/*
 * Save the code generator's compile-position state and hand the live generator a
 * fresh, empty one for a NESTED compilation unit.
 *
 * A require/include normally runs at execution time, when no compile is in flight,
 * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an
 * autoload can fire in the MIDDLE of compiling a class: resolving `class Child
 * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose
 * body `require`s Base's file — a nested compile while the outer one is mid-token.
 * PH7_ResetCodeGenerator would then blow away the outer compile's cursor
 * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes
 * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common
 * in every real framework: Composer autoloads parent classes on demand.)
 *
 * This snapshots the position/scope fields into *pSaved (a caller-stack
 * ph7_gen_state used purely as storage) and re-initializes the live generator's
 * position containers to FRESH, empty ones WITHOUT releasing the outer's (the
 * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE
 * and shared — compiled bytecode interns names/literals into them and they grow
 * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old
 * copy of those headers after a nested grow would use-after-free the bucket array.
 */
PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)
{
	ph7_gen_state *pGen = &pVm->sCodeGen;
	/* Shallow-copy every field; the position containers below are then replaced
	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */
	*pSaved = *pGen;
	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));
	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));
	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));
	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));
	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));
	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));
	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));
	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);
	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);
	SyBlobInit(&pGen->sFirstErr,&pVm->sAllocator);
	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);
	GenStateInitUseImports(&(*pGen),&(*pVm));
	GenStateInitSeenSymbols(&(*pGen),&(*pVm));
	/* Fresh global scope for the nested unit (address of the embedded sGlobal is
	 * stable, so any outer block still parented to it stays valid across restore). */
	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);
	pGen->pCurrent = &pGen->sGlobal;
	pGen->pIn = pGen->pEnd = 0;
	pGen->pRawIn = pGen->pRawEnd = 0;
	pGen->pTokenSet = 0;
	pGen->nErr = 0;
	pGen->nFatal = 0;
	pGen->nFirstErrLine = 0;
	pGen->bParseThrows = 0;
	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;
	pGen->nLoopId = pGen->nCurLoopId = 0;
	pGen->nCommaExprOk = 0;
	pGen->zClauseCloser = 0;
	pGen->bInGenerator = 0;
	pGen->bStrictTypes = 0;
	pGen->bStrictTypesLocked = 0;
	/* The nested unit is a fresh top-level compile: it is not lexically inside the
	 * outer's class body nor its member default, so a __TRAIT__ in the nested file
	 * must not inherit the outer's trait. (Restore below carries the outer's values
	 * back, so only the nested unit sees these zeros.) */
	pGen->pCurClass = 0;
	pGen->iInMemberDefault = 0;
	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);
	pGen->xErr = xErr;
	pGen->pErrData = pErrData;
}
/*
 * Restore the outer compile-position state saved by PH7_CompilerSaveState,
 * releasing the nested unit's position containers first. The shared
 * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried
 * forward, NOT rolled back to the snapshot's stale headers.
 */
PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)
{
	ph7_gen_state *pGen = &pVm->sCodeGen;
	GenBlock *pBlock,*pParent;
	SyHash hVar,hLiteral,hNumLiteral;
	/* Free any nested blocks left open (e.g. an aborted nested compile), then the
	 * nested global block's own fixup sets. */
	pBlock = pGen->pCurrent;
	while( pBlock && pBlock->pParent != 0 ){
		pParent = pBlock->pParent;
		GenStateFreeBlock(pBlock);
		pBlock = pParent;
	}
	GenStateReleaseBlock(&pGen->sGlobal);
	/* Release the nested unit's position containers. */
	SySetRelease(&pGen->aLabel);
	SySetRelease(&pGen->aGoto);
	SySetRelease(&pGen->aNullsafeJmp);
	SySetRelease(&pGen->aLoopParent);
	SySetRelease(&pGen->aScope);
	SySetRelease(&pGen->aTrivia);
	SySetRelease(&pGen->aPendingAttrs);
	SyBlobRelease(&pGen->sWorker);
	SyBlobRelease(&pGen->sErrBuf);
	SyBlobRelease(&pGen->sFirstErr);
	SyBlobRelease(&pGen->sNamespace);
	SyHashRelease(&pGen->hUseImports);
	SyHashRelease(&pGen->hUseFuncImports);
	SyHashRelease(&pGen->hUseConstImports);
	GenStateReleaseSeenSymbols(&(*pGen));
	/* Preserve the (possibly grown) shared intern tables across the restore. */
	hVar = pGen->hVar;
	hLiteral = pGen->hLiteral;
	hNumLiteral = pGen->hNumLiteral;
	*pGen = *pSaved;
	pGen->hVar = hVar;
	pGen->hLiteral = hLiteral;
	pGen->hNumLiteral = hNumLiteral;
}
/*
 * Raise php's parse error for an unexpected token: E_PARSE with the exact text
 * php's parser prints, e.g.
 *
 *   syntax error, unexpected token ";", expecting "{"
 *   syntax error, unexpected identifier "invalid", expecting "("
 *   syntax error, unexpected end of file
 *
 * php names the token by CLASS, not just by text: an identifier, a variable and
 * a number each get their own noun, while everything else (keywords, operators,
 * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail
 * — pass NULL when the site cannot say what it wanted (php often can't either).
 * pTok == NULL means the input ran out: "unexpected end of file".
 *
 * PHL's hand-written recursive-descent parser has no bison expectation sets, so
 * a site can only claim an "expecting" clause it genuinely knows; every clause
 * emitted here was verified against php 8.5.7 for the construct in question.
 */
/*
 * Rebuild the `<<<LABEL` marker of a heredoc/nowdoc token from the source the
 * token's BODY points into: the header always sits immediately above it. Answers
 * FALSE when no marker is found within reach, in which case the caller falls back
 * to the generic noun rather than guessing.
 */
static int GenStateHeredocMarker(SyString *pBody,SyString *pOut)
{
	const unsigned char *z = (const unsigned char *)pBody->zString;
	const unsigned char *zLabelEnd;
	/* Walk the header BACKWARDS from the body, which begins one byte past the
	 * terminator of the marker's own line: line terminator, trailing blanks, the
	 * closing quote, the LABEL, the opening quote, leading blanks, `<<<`. Every
	 * step stops on a byte the next step owns, so the walk cannot leave the
	 * header -- `<` is not a label byte and a label is what sits above the body. */
	z--;
	if( z[0] == '\n' ){
		z--;
		if( z[0] == '\r' ){
			z--;
		}
	}
	while( z[0] == ' ' || z[0] == '\t' ){
		z--;
	}
	if( z[0] == '"' || z[0] == '\'' ){
		z--;
	}
	zLabelEnd = &z[1];
	while( z[0] >= 0x80 || SyisAlphaNum(z[0]) || z[0] == '_' ){
		z--;
	}
	if( zLabelEnd == &z[1] ){
		return 0; /* No label: not a header this routine can read back */
	}
	if( z[0] == '"' || z[0] == '\'' ){
		z--;
	}
	while( z[0] == ' ' || z[0] == '\t' ){
		z--;
	}
	if( !(z[0] == '<' && z[-1] == '<' && z[-2] == '<') ){
		return 0;
	}
	/* php's token text runs from `<<<` to the end of the LABEL -- the opening
	 * quote of a nowdoc is inside it, the closing one is not. */
	SyStringInitFromBuf(pOut,&z[-2],(sxu32)(zLabelEnd - &z[-2]));
	return 1;
}
PH7_PRIVATE sxi32 PH7_GenSyntaxError(
	ph7_gen_state *pGen,   /* Code generator state */
	SyToken *pTok,         /* Offending token, or NULL for end of file */
	const char *zExpecting /* ", expecting <this>" tail, or NULL */
	)
{
	/* php's noun for the offending token. An ALPHA-stream operator (`and`, `or`,
	 * `xor`, `new`, `clone`, `instanceof`) is lexed ID|OP here but php calls it a
	 * TOKEN, like every other reserved word — only a real identifier gets the
	 * "identifier" noun. */
	const char *zNoun = "token";
	sxu32 nLine;
	if( pTok == 0 && pGen->pTokenSet ){
		/* The caller ran out of tokens inside its own slice — but a statement's slice stops
		 * BEFORE its terminator, so the token php actually names (typically the ';') is the
		 * one sitting just past the slice, still inside the chunk's token stream. Reach for
		 * it before concluding "end of file". */
		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);
		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];
		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){
			pTok = pGen->pEnd;
		}
	}
	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);
	if( pTok == 0 && pGen->bChunkAtEof && pGen->nChunkEofLine > nLine ){
		/* End of INPUT is reported where it sits, not where the last token ended. */
		nLine = pGen->nChunkEofLine;
	}
	if( pTok == 0 ){
		return PH7_GenCompileError(pGen,E_PARSE,nLine,
			zExpecting ? "syntax error, unexpected end of file, expecting %s"
			           : "syntax error, unexpected end of file",
			zExpecting);
	}
	if( (pTok->nType & PH7_TK_ID) && (pTok->nType & PH7_TK_OP) == 0 ){
		zNoun = "identifier";
	}else if( pTok->nType & PH7_TK_DOLLAR ){
		zNoun = "variable";
		/* The '$' is its own token and carries only "$" as text; the NAME is the
		 * token after it. php names the whole variable, so `$x` was being reported
		 * as the nameless `variable "$"`. Stitch the two back together. */
		if( pGen->pTokenSet ){
			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);
			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];
			SyToken *pName = &pTok[1];
			if( pTok >= pBase && pName < pStreamEnd
				&& (pName->nType & (PH7_TK_ID|PH7_TK_KEYWORD))
				&& pName->sData.nByte > 0 ){
				SyBlobReset(&pGen->sWorker);
				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));
				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);
				{
					SyString sVar;
					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),
						SyBlobLength(&pGen->sWorker));
					if( zExpecting ){
						return PH7_GenCompileError(pGen,E_PARSE,nLine,
							"syntax error, unexpected %s \"%z\", expecting %s",
							zNoun,&sVar,zExpecting);
					}
					return PH7_GenCompileError(pGen,E_PARSE,nLine,
						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);
				}
			}
		}
	}else if( pTok->nType & PH7_TK_INTEGER ){
		zNoun = "integer";
	}else if( pTok->nType & PH7_TK_REAL ){
		/* php's noun, which is not the type name: `float` is what the CAST is
		 * called, `floating-point number` what a stray literal is called. */
		zNoun = "floating-point number";
	}else if( pTok->nType & (PH7_TK_SSTR|PH7_TK_DSTR) ){
		/* php names a string literal by the QUOTE it was written with, and prints
		 * the SOURCE bytes between the quotes -- escapes unresolved, which is what
		 * the token already holds here. A double-quoted string that INTERPOLATES is
		 * not one token in php at all: its scanner emits the opening quote on its
		 * own, so the parser has nothing to quote and the noun stands alone. */
		if( (pTok->nType & PH7_TK_DSTR) && GenStateDqInterpolates(&pTok->sData) ){
			if( zExpecting ){
				return PH7_GenCompileError(pGen,E_PARSE,nLine,
					"syntax error, unexpected double-quote mark, expecting %s",zExpecting);
			}
			return PH7_GenCompileError(pGen,E_PARSE,nLine,
				"syntax error, unexpected double-quote mark");
		}
		zNoun = (pTok->nType & PH7_TK_SSTR) ? "single-quoted string" : "double-quoted string";
	}else if( pTok->nType & (PH7_TK_HEREDOC|PH7_TK_NOWDOC) ){
		/* php names the OPENING marker -- `<<<EOT`, or `<<<'EOT` for a nowdoc, the
		 * closing quote dropped because the token text ends at the label -- and
		 * reports it on the line AFTER the marker's, its scanner having consumed
		 * that line's terminator before the token is handed over. The token here
		 * carries the BODY, so the marker is read back off the source it points
		 * into. */
		SyString sMark;
		if( GenStateHeredocMarker(&pTok->sData,&sMark) ){
			if( zExpecting ){
				return PH7_GenCompileError(pGen,E_PARSE,nLine + 1,
					"syntax error, unexpected heredoc start \"%z\", expecting %s",&sMark,zExpecting);
			}
			return PH7_GenCompileError(pGen,E_PARSE,nLine + 1,
				"syntax error, unexpected heredoc start \"%z\"",&sMark);
		}
	}
	if( zExpecting ){
		return PH7_GenCompileError(pGen,E_PARSE,nLine,
			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);
	}
	return PH7_GenCompileError(pGen,E_PARSE,nLine,
		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);
}
/*
 * Generate a compile-time error message.
 * If the error count limit is reached (usually 15 error message)
 * this function return SXERR_ABORT.In that case upper-layers must
 * abort compilation immediately.
 */
/*
 * php's `Stack trace:` block under a compile-time FATAL: the activations that are
 * live at the refusal -- the enclosing functions, and the include/require/eval that
 * loaded the unit being compiled -- then the `#N {main}` marker. A parse error gets
 * none: that one is the parser's own refusal and php reports it as an E_PARSE.
 *
 * A refusal raised while the VM is still INITIALIZING is the main script's own
 * compile: there is no runtime state to walk (and no object pool to build the array
 * in), and php's answer there is the bare bottom marker.
 */
PH7_PRIVATE void PH7_GenAppendFatalTrace(ph7_vm *pVm,SyBlob *pOut,int iTraceKind)
{
	ph7_value *pTrace;
	if( pVm == 0 || iTraceKind == PH7_FATAL_TRACE_NONE ){
		return;
	}
	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);
	if( pVm->nMagic == PH7_VM_INIT ){
		SyBlobAppend(pOut,"#0 {main}",sizeof("#0 {main}")-1);
		return;
	}
	pTrace = ph7_new_array(&(*pVm));
	if( pTrace == 0 ){
		SyBlobAppend(pOut,"#0 {main}",sizeof("#0 {main}")-1);
		return;
	}
	/* php's fatal trace carries no argument list. Nor, unless this is one of the
	 * refusals php makes at RUN time, the include/require/eval that loaded the unit
	 * being compiled: php raises a compile error before it pushes that activation. */
	VmBuildBacktrace(&(*pVm),0x2 | (iTraceKind == PH7_FATAL_TRACE_RUNTIME ? 0 : 0x4),0,pTrace);
	PH7_VmTraceToString(&(*pVm),pTrace,TRUE,pOut);
	ph7_release_value(&(*pVm),pTrace);
}
PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)
{
	SyBlob *pWorker = &pGen->sErrBuf;
	const char *zErr = "Error";
	SyString *pFile;
	va_list ap;
	sxi32 rc;
	/* Reset the working buffer. NOT when nobody is logging: there the buffer is a
	 * one-message store eval() reads its ParseError text out of, and php stops at
	 * the FIRST error where this generator carries on to a budget of fifteen -- so
	 * resetting handed eval the LAST message. `eval('echo 1 foo;')` reported
	 * `unexpected token ";"`, the synchronizer's own complaint, where php names the
	 * `identifier "foo"` it choked on. */
	if( pGen->xErr ){
		SyBlobReset(pWorker);
	}
	/* Peek the processed file path if available */
	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);
	if( nErrType == E_ERROR || nErrType == E_PARSE ){
		/* Increment the error counter. A PARSE error is every bit as fatal as an
		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting
		 * only E_ERROR let a parse error print its diagnostic and then fall through
		 * into execution with a 0 exit status. */
		pGen->nErr++;
		if( nErrType == E_ERROR ){
			/* php's E_COMPILE_ERROR: an uncatchable fatal, where a parse error is a
			 * catchable ParseError. The include path reads this to tell them apart. */
			pGen->nFatal++;
		}
		if( pGen->nErr == 1 ){
			/* Keep the FIRST refusal's bare text and line: it is the one php reports,
			 * and it is the message an include's ParseError carries. */
			va_list apF;
			SyBlobReset(&pGen->sFirstErr);
			va_start(apF,zFormat);
			SyBlobFormatAp(&pGen->sFirstErr,zFormat,apF);
			va_end(apF);
			pGen->nFirstErrLine = nLine;
		}else{
			/* php stops at the first one. This generator recovers and carries on so
			 * that the rest of the unit is still walked (a later pass needs the
			 * symbols), but everything it says after the first refusal is its own
			 * recovery talking -- and printing it put diagnostics on the user's
			 * screen that php, having stopped, never reaches.
			 *
			 * The recovery is still bounded, and now SILENTLY: the old limit
			 * announced itself with a `Error count limit reached` line of PH7's own
			 * invention, which no php prints and which would land on top of the one
			 * diagnostic php does. The unit has already failed and its first message
			 * is already recorded, so there is nothing left to say. */
			return (pGen->nErr > 15) ? SXERR_ABORT : SXRET_OK;
		}
	}
	if( nErrType == E_PARSE && pGen->bParseThrows ){
		/* An include/require unit: php's parser throws a ParseError rather than
		 * printing, and the text only reaches the screen if nobody catches it.
		 * The caller (VmEvalChunk) raises it from sFirstErr. */
		return SXRET_OK;
	}
	if( pGen->xErr == 0 ){
		/* No consumer — but keep the BARE message in the error buffer so a caller
		 * that needs the text can read it back. eval() compiles with logging off
		 * (a parse error there is php's catchable ParseError, not a printed
		 * diagnostic) and needs exactly this string for the exception message. The
		 * first message stands: everything after it is this generator's recovery
		 * talking, and php never got that far. */
		if( SyBlobLength(pWorker) < 1 ){
			va_start(ap,zFormat);
			SyBlobFormatAp(pWorker,zFormat,ap);
			va_end(ap);
		}
		return SXRET_OK;
	}
	switch(nErrType){
	case E_ERROR:   zErr = "Fatal error"; break;
	case E_WARNING: zErr = "Warning";     break;
	case E_PARSE:   zErr = "Parse error"; break;
	case E_NOTICE:  zErr = "Notice";      break;
	case E_USER_ERROR:   zErr = "User error";   break;
	case E_USER_WARNING: zErr = "User warning"; break;
	case E_USER_NOTICE:  zErr = "User notice";  break;
	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;
	default:
		break;
	}
	rc = SXRET_OK;
	/* Format: PHP <severity>:  <message> in <file> on line <line> */
	SyBlobAppend(pWorker,"PHP ",4);
	SyBlobFormat(pWorker,"%s:  ",zErr);
	va_start(ap,zFormat);
	SyBlobFormatAp(pWorker,zFormat,ap);
	va_end(ap);
	if( pFile ){
		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);
	}
	if( nErrType == E_ERROR ){
		PH7_GenAppendFatalTrace(pGen->pVm,pWorker,pGen->iFatalTrace);
	}
	/* iFatalTrace is a ONE-SHOT: a site that raises one of php's non-compiler
	 * refusals sets it just before the call and this consumes it, so no site has to
	 * remember to put it back and none of them can leak it onto a later refusal. */
	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;
	/* Append a new line */
	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));
	if( SyBlobLength(pWorker) > 0 ){
		/* Consume the generated error message */
		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);
	}
	return rc;
}
