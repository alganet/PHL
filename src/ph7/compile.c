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
 * Check if the given name refer to a valid label.
 * Return SXRET_OK and write a pointer to that label on success.
 * Any other return value indicates no such label.
 */
static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)
{
	Label *aLabel;
	sxu32 n;
	/* Perform a linear scan on the label table */
	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);
	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){
		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){
			/* Jump destination found */
			aLabel[n].bRef = TRUE;
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
	/* NOTE: a userland function shadowing a C builtin (e.g. `function strlen(){}`)
	 * is NOT caught here — the C builtins register in PH7_VmMakeReady, after user
	 * code has compiled, so hHostFunction is still empty at this point. Prelude
	 * functions (ini_get, ...) and every builtin CLASS compile earlier and ARE
	 * guarded. Redeclaring a C builtin function stays a known divergence. */
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
	/* Insert in the jump fixup table */
	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);
	return rc;
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
		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);
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
		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);
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
		/* Make sure the target label is reachable */
		if( pLabel->pFunc != pJump->pFunc ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		/* Fix the jump now the destination is resolved */
		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);
		if( pInstr ){
			pInstr->iP2 = pLabel->nJumpDest;
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
		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */
		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */
		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */
		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */
		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */
		{ "fsockopen",              9, (1u<<2)|(1u<<3) },  /* &$error_code, &$error_message */
		{ "pfsockopen",            10, (1u<<2)|(1u<<3) },  /* same */
		{ "stream_socket_client",  20, (1u<<1)|(1u<<2) },  /* &$error_code, &$error_message */
		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */
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
 * Generate bytecode for a given expression tree.
 * If something goes wrong while generating bytecode
 * for the expression tree (A very unlikely scenario)
 * this function takes care of generating the appropriate
 * error message.
 */
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
		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE|EXPR_FLAG_MEMBER_WRITE|EXPR_FLAG_RDONLY_LOAD);
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
	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);
	/* Generate code for the left tree */
	if( pNode->pLeft ){
		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);
		if( iVmOp == PH7_OP_CALL ){
			ph7_expr_node **apNode;
			int hasSpread = 0;
			int hasNamed = 0;
			int bAnySpread = 0;
			sxu32 byRefMask = 0;
			sxi32 nArgs;
			sxi32 n;
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
							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,
								"syntax error, unexpected token \"...\"");
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
				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE|EXPR_FLAG_MEMBER_WRITE);
				/* For a by-ref argument position, drop the read-only flag so the
				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and
				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))
				 * auto-vivifies its element and exposes a writable memobj slot for the
				 * builtin to write back through. A plain $var target is unaffected
				 * (iP1=0 either way). */
				if( n < 31 && (byRefMask & (1u<<n)) ){
					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;
					iArgFlags |= EXPR_FLAG_LOAD_IDX_STORE;
				}
				/* Slice 21: a plain `$var` argument may bind to a USER-function by-ref
				 * parameter whose signature is unknown at compile time (forward
				 * reference, dynamic call, or method dispatch — e.g. PHPUnit's
				 * `willReturnReference($undef)`). Reserve a real memobj slot for it so an
				 * UNDEFINED variable vivifies and the by-ref write-back reaches the caller
				 * (php). A by-value parameter still receives a copy; the only divergence
				 * is that an undefined variable passed BY VALUE is created as NULL in the
				 * caller (recorded in NEWPLAN §2). Excludes isset()/empty()/unset(), which
				 * compile through this same call loop but must NEVER create their operand,
				 * and named/spread args (positional-index and by-ref semantics don't apply). */
				if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET|EXPR_FLAG_LOAD_IDX_EMPTY|EXPR_FLAG_LOAD_IDX_UNSET)) == 0
				 && apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable
				 && (apNode[n]->iFlags & (EXPR_NODE_NAMED_ARG|EXPR_NODE_SPREAD)) == 0 ){
					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;
				}
				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);
				if( rc != SXRET_OK ){
					return rc;
				}
				/* Each argument is an independent nullsafe scope. */
				GenStatePatchNullsafeJumps(pGen, nArgNsBase);
				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){
					/* Emit spread opcode to unpack this array argument */
					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);
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
			/* Remove stale flags now */
			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;
		}
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
			sxu32 nNullcLhsFirst = PH7_VmInstrLength(pGen->pVm);
			int bNullcLhs = 0;
			if( pNode->pLeft && pNode->pLeft->pOp
				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW
					|| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW
					|| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){
				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;
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
				iLeftFlags |= EXPR_FLAG_LOAD_IDX_STORE | EXPR_FLAG_MEMBER_WRITE
					| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;
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
				bNullcLhs = 1;
				if( pNode->pLeft->pOp
					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW
						|| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW
						|| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){
					/* A member-access LHS additionally takes OP_MEMBER's silent
					 * lookup (iP2 = ISSET) so an uninitialized typed property
					 * yields the default instead of an Error. A SUBSCRIPT LHS must
					 * NOT: LOAD_IDX's ISSET mode means offsetExists (a bool), while
					 * `$o[$k] ?? d` needs the offsetGet value — OP_NULLC already
					 * handles that path. */
					iLeftFlags |= EXPR_FLAG_LOAD_IDX_ISSET;
				}
			}
			if( iVmOp == PH7_OP_ERR_CTRL ){
				/* '@' must suppress the diagnostics raised WHILE its operand runs, so
				 * open the window here; the trailing emit below closes it (iP1 = 0). */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);
			}
			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags|EXPR_FLAG_RDONLY_LOAD);
			if( rc == SXRET_OK && bNullcLhs ){
				/* Mark EVERY subscript read in the `??` left chain quiet (iP2=8).
				 * Peeking at the next instruction only catches the OUTERMOST
				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;
				 * and a forward scan would be unsound, since an unrelated sibling
				 * access can sit immediately before a coalesce (`f($a['k'],
				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks
				 * the range it just emitted. Write-context codes (1/3/5) belong to
				 * `??=` and keep their meaning. */
				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);
				sxu32 nAt;
				for( nAt = nNullcLhsFirst ; nAt < nEnd ; ++nAt ){
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
		}else if( iVmOp == PH7_OP_LOAD_IDX ){
			ph7_expr_node **apNode;
			sxi32 n;
			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE
				|EXPR_FLAG_LOAD_IDX_ISSET|EXPR_FLAG_LOAD_IDX_UNSET
				|EXPR_FLAG_LOAD_IDX_EMPTY|EXPR_FLAG_MEMBER_WRITE
				|EXPR_FLAG_QUIET_VAR|EXPR_FLAG_RMW_LOAD);
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
			}
			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){
				/* offsetExists for ArrayAccess; peek-only for arrays */
				iP2 = 4;
			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){
				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays
				 * so the trailing unset() builtin can drop the slot. */
				iP2 = 5;
			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){
				/* offsetExists+offsetGet for ArrayAccess so empty() can
				 * short-circuit on missing keys without invoking offsetGet
				 * unnecessarily; peek-only for arrays (same as iP2=0). */
				iP2 = 6;
			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){
				/* Create an empty entry when the desired index is not found */
				iP2 = 1;
			}
		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){
			/* POP the left node */
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
		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){
			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write
			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is
			 * auto-created — PHP auto-vivifies on write. */
			iFlags |= EXPR_FLAG_LOAD_IDX_STORE | EXPR_FLAG_MEMBER_WRITE;
			if( iVmOp != PH7_OP_STORE ){
				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so
				 * php warns when it is undefined and then seeds it; a plain `=`
				 * writes without reading and stays silent. */
				iFlags |= EXPR_FLAG_RMW_LOAD;
			}
		}
		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);
		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags|EXPR_FLAG_RDONLY_LOAD);
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
						iP1 = pInstr->iP1;
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
					pCallInstr = pPeek;
					pPeek = PH7_VmPeekNextInstr(pGen->pVm);
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
				pPrev = PH7_VmPeekNextInstr(pGen->pVm);
				if( pPrev == 0 || pPrev->iOp != PH7_OP_MEMBER ){
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
				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){
					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */
					iP2 = PH7_MEMBER_WRITE;
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
			iP2 = 0;
			p3 = 0;
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){
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
 * Tree validator for unset() arguments — rejects any `?->` node in
 * the argument expression with PHP's "Can't use nullsafe operator
 * in write context" parse error.
 */
static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)
{
	sxi32 rc;
	if( !PH7_ExprContainsNullsafe(pNode) ){
		return SXRET_OK;
	}
	rc = PH7_GenCompileError(pGen,E_PARSE,
		pNode ? pNode->pStart->nLine : 1,
		"Can't use nullsafe operator in write context");
	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;
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
		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){
			return PH7_CompileClassInterface;
		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){
			return PH7_CompileClass;
		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){
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
				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);
				if( pImp ){
					const char *zFqn = (const char *)pImp->pUserData;
					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));
					if( zDup ){
						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));
					}
				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){
					SyBlob sTmp;
					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);
					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
					SyBlobAppend(&sTmp,"\\",1);
					SyBlobAppend(&sTmp,zName,nName);
					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));
					if( zDup ){
						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));
					}
					SyBlobRelease(&sTmp);
				}
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
					pGen->pIn = pArgStart;
					pGen->pEnd = pArgStop;
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
		if( pGen->pIn >= pGen->pEnd ){
			/* No more input to process */
			break;
		}
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
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
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
		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){
			/* Compile block */
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
			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){
				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
				/* Try to extract a language construct handler */
				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);
				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){
					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
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
				/* Assume an expression an try to compile it */
				rc = PH7_CompileExpr(&(*pGen),0,0);
				if(  rc != SXERR_EMPTY ){
					/* Pop l-value */
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
				}
			}else{
				/* Go compile the sucker */
				rc = xCons(&(*pGen));
			}
			if( rc == SXERR_ABORT ){
				/* Request to abort compilation */
				break;
			}
		}
		/* Ignore trailing semi-colons ';' */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){
			pGen->pIn++;
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
	/* Point to the head and tail of the token stream. */
	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);
	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];
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
	SyToken *pSavedIn,*pSavedEnd;
	sxi32 rc;
	sxu32 nBaseLine = 1;
	if( pScript->nByte < 1 ){
		/* Nothing to compile */
		return PH7_OK;
	}
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
	/* Process high-level tokens */
	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);
	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];
	rc = PH7_OK;
	if( is_expr ){
		/* Compile the expression */
		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);
		goto cleanup;
	}
	nObjIdx = 0;
	/* Each compilation unit starts in the global namespace.
	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,
	 * preventing namespace bleeding across include()d files. */
	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);
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
	pGen->nLoopId = pGen->nCurLoopId = 0;
	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));
	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));
	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));
	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);
	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);
	/* Error log buffer */
	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);
	/* General purpose working buffer */
	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);
	/* Namespace state */
	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);
	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);
	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);
	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);
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
	SyBlobRelease(&pGen->sWorker);
	SyBlobRelease(&pGen->sNamespace);
	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);
	SyHashRelease(&pGen->hUseImports);
	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);
	SyHashRelease(&pGen->hUseFuncImports);
	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);
	SyHashRelease(&pGen->hUseConstImports);
	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);
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
	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));
	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));
	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);
	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);
	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);
	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);
	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);
	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);
	/* Fresh global scope for the nested unit (address of the embedded sGlobal is
	 * stable, so any outer block still parented to it stays valid across restore). */
	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);
	pGen->pCurrent = &pGen->sGlobal;
	pGen->pIn = pGen->pEnd = 0;
	pGen->pRawIn = pGen->pRawEnd = 0;
	pGen->pTokenSet = 0;
	pGen->nErr = 0;
	pGen->nLoopId = pGen->nCurLoopId = 0;
	pGen->nCommaExprOk = 0;
	pGen->zClauseCloser = 0;
	pGen->bInGenerator = 0;
	pGen->bStrictTypes = 0;
	pGen->bStrictTypesLocked = 0;
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
	SySetRelease(&pGen->aTrivia);
	SySetRelease(&pGen->aPendingAttrs);
	SyBlobRelease(&pGen->sWorker);
	SyBlobRelease(&pGen->sErrBuf);
	SyBlobRelease(&pGen->sNamespace);
	SyHashRelease(&pGen->hUseImports);
	SyHashRelease(&pGen->hUseFuncImports);
	SyHashRelease(&pGen->hUseConstImports);
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
PH7_PRIVATE sxi32 PH7_GenSyntaxError(
	ph7_gen_state *pGen,   /* Code generator state */
	SyToken *pTok,         /* Offending token, or NULL for end of file */
	const char *zExpecting /* ", expecting <this>" tail, or NULL */
	)
{
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
	if( pTok == 0 ){
		return PH7_GenCompileError(pGen,E_PARSE,nLine,
			zExpecting ? "syntax error, unexpected end of file, expecting %s"
			           : "syntax error, unexpected end of file",
			zExpecting);
	}
	if( pTok->nType & PH7_TK_ID ){
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
		zNoun = "float";
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
PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)
{
	SyBlob *pWorker = &pGen->sErrBuf;
	const char *zErr = "Error";
	SyString *pFile;
	va_list ap;
	sxi32 rc;
	/* Reset the working buffer */
	SyBlobReset(pWorker);
	/* Peek the processed file path if available */
	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);
	if( nErrType == E_ERROR || nErrType == E_PARSE ){
		/* Increment the error counter. A PARSE error is every bit as fatal as an
		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting
		 * only E_ERROR let a parse error print its diagnostic and then fall through
		 * into execution with a 0 exit status. */
		pGen->nErr++;
		if( pGen->nErr > 15 ){
			/* Error count limit reached */
			if( pGen->xErr ){
				SyBlobAppend(pWorker,"PHP ",4);
				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");
				if( pFile ){
					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);
				}
				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));
				if( SyBlobLength(pWorker) > 0 ){
					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);
				}
			}
			/* Abort immediately */
			return SXERR_ABORT;
		}
	}
	if( pGen->xErr == 0 ){
		/* No consumer — but keep the BARE message in the error buffer so a caller
		 * that needs the text can read it back. eval() compiles with logging off
		 * (a parse error there is php's catchable ParseError, not a printed
		 * diagnostic) and needs exactly this string for the exception message. */
		va_start(ap,zFormat);
		SyBlobFormatAp(pWorker,zFormat,ap);
		va_end(ap);
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
	/* Append a new line */
	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));
	if( SyBlobLength(pWorker) > 0 ){
		/* Consume the generated error message */
		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);
	}
	return rc;
}
