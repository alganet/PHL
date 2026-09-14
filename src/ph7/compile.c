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
static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)
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
/*
 * Return TRUE if c is a valid digit for the given numeric base.
 *   base 16 => SyisHex (0-9, a-f, A-F)
 *   base  2 => 0 or 1
 *   base 10 => SyisDigit (0-9, also used for octal literals which share the
 *              decimal scan in the lexer)
 */
static int GenStateIsBaseDigit(int c, int base)
{
	if( base == 16 ){ return SyisHex(c); }
	if( base == 2 ){ return c == '0' || c == '1'; }
	return SyisDigit(c);
}
/*
 * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4
 * underscore separator so the caller can report the malformed portion with
 * the exact wording PHP uses:
 *
 *   syntax error, unexpected identifier "X"
 *
 * The lexer guarantees that every underscore it consumed as a separator is
 * surrounded by valid base digits; anything else sits in the trailing run
 * absorbed by the lexer specifically to let this validator see and report
 * it. That invariant means the malformed span is exactly [bad .. nByte) —
 * no forward rescan needed.
 *
 * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;
 * returns 0 when it is well-formed.
 */
static int GenStateFindBadNumericSeparator(
	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)
{
	const char *z = pRaw->zString;
	sxu32 n = pRaw->nByte;
	int base = 10;
	sxu32 i, start;
	if( n < 2 ) return 0;
	if( z[0] == '0' && (z[1] == 'x' || z[1] == 'X') ){
		base = 16;
	}else if( z[0] == '0' && (z[1] == 'b' || z[1] == 'B') ){
		base = 2;
	}
	for( i = 0; i < n; ++i ){
		if( z[i] != '_' ) continue;
		if( i > 0 && i + 1 < n
			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)
			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){
			continue; /* well-placed separator */
		}
		/* First misplaced underscore — the lexer already absorbed the full
		 * malformed tail, so it runs from here to the end of the token. */
		start = i;
		if( start > 0 && (z[start-1] == 'x' || z[start-1] == 'X'
			|| z[start-1] == 'b' || z[start-1] == 'B') ){
			start--; /* include the base letter for 0x_... / 0b_... */
		}
		*pBadStart = &z[start];
		*pBadLen = n - start;
		return 1;
	}
	return 0;
}
/*
 * Emit the shared "syntax error, unexpected identifier" parse error when a
 * numeric-literal token contains a misplaced PHP 7.4 separator. Returns
 * SXRET_OK when the token is well-formed; on error propagates whatever
 * PH7_GenCompileError returned (SXERR_ABORT when the error count is
 * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned
 * so callers can bail from the current construct).
 */
PH7_PRIVATE sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)
{
	const char *zBad = 0;
	sxu32 nBad = 0;
	SyString sBad;
	sxi32 rc;
	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){
		return SXRET_OK;
	}
	SyStringInitFromBuf(&sBad, zBad, nBad);
	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,
		"syntax error, unexpected identifier \"%z\"", &sBad);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXERR_SYNTAX;
}
/*
 * Strip PHP 7.4 numeric literal separators (underscores between digits) from
 * a numeric token's text and yield a SyString suitable for the low-level
 * converters (SyStrToInt64 / SyStrToReal / etc.).
 *
 * Fast path: if the token contains no '_', *pOut aliases pToken with no copy
 * and *pzAlloc is set to NULL.
 * Stack path: if the cleaned bytes fit in zScratch, they are written there
 * and *pzAlloc is set to NULL.
 * Heap path: for literals larger than the scratch buffer, a fresh buffer is
 * allocated from pAlloc, returned via *pzAlloc, and must be released by the
 * caller with SyMemBackendFree once the converter is done.
 *
 * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which
 * case *pOut is left untouched and the caller must not read it).
 */
PH7_PRIVATE sxi32 GenStateStripNumericSeparators(
	SyMemBackend *pAlloc,
	const SyString *pToken,
	char *zScratch, sxu32 nScratch,
	SyString *pOut, char **pzAlloc)
{
	sxu32 i, j;
	int hasUnderscore = 0;
	char *zBuf;
	*pzAlloc = 0;
	for( i = 0; i < pToken->nByte; ++i ){
		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }
	}
	if( !hasUnderscore ){
		SyStringDupPtr(pOut, pToken);
		return SXRET_OK;
	}
	if( pToken->nByte <= nScratch ){
		zBuf = zScratch;
	}else{
		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);
		if( zBuf == 0 ){
			return SXERR_ABORT;
		}
		*pzAlloc = zBuf;
	}
	j = 0;
	for( i = 0; i < pToken->nByte; ++i ){
		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }
	}
	SyStringInitFromBuf(pOut, zBuf, j);
	return SXRET_OK;
}
/*
 * Compile a numeric [i.e: integer or real] literal.
 * Notes on the integer type.
 *  According to the PHP language reference manual
 *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)
 *  or binary (base 2) notation, optionally preceded by a sign (- or +).
 *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal
 *  notation precede the number with 0x. To use binary notation precede the number with 0b.
 * Symisc eXtension to the integer type.
 *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine
 *  where the size of an integer is platform-dependent.That is,the size of an integer
 *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms
 *  [i.e: either 32bit or 64bit].
 *  For more information on this powerfull extension please refer to the official
 *  documentation.
 */
/*
 * Determine whether an integer literal token exceeds the signed 64-bit range.
 * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->
 * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or
 * dropping digits. pNum is the separator-stripped token (unsigned; the sign of
 * a "-1" is a separate unary operator). Base detection mirrors
 * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the
 * float value is accumulated into *pReal (dv = dv*base + digit); for decimal
 * *pbDecimal is set so the caller reuses strtod on the token for a
 * correctly-rounded value. Returns FALSE (value fits) for anything it cannot
 * confidently classify, so the int path stays in charge.
 *
 * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact
 * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit
 * doubling). Octal/binary overflow values can differ from php by the low bit(s):
 * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's
 * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a
 * residual; matching php exactly would need a port of those functions.
 */
static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)
{
	const char *z = pNum->zString;
	const char *zEnd = z + pNum->nByte;
	const char *p, *q;
	int n;
	*pbDecimal = FALSE;
	if( z >= zEnd ){
		return FALSE;
	}
	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' || z[1] == 'X') ){
		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */
		p = z + 2;
		while( p < zEnd && p[0] == '0' ){ p++; }
		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }
		if( n < 16 || (n == 16 && SyHexToint(p[0]) < 8) ){
			return FALSE;
		}
		{ ph7_real dv = 0;
		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){
			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);
		  }
		  *pReal = dv;
		}
		return TRUE;
	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' || z[1] == 'B') ){
		/* Binary: INT64_MAX needs 63 significant bits. */
		p = z + 2;
		while( p < zEnd && p[0] == '0' ){ p++; }
		for( q = p, n = 0; q < zEnd && (q[0] == '0' || q[0] == '1'); q++ ){ n++; }
		if( n <= 63 ){
			return FALSE;
		}
		{ ph7_real dv = 0;
		  for( q = p; q < zEnd && (q[0] == '0' || q[0] == '1'); q++ ){
			dv = dv * 2 + (ph7_real)(q[0] - '0');
		  }
		  *pReal = dv;
		}
		return TRUE;
	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' || z[1] == 'O') ){
		/* PHP 8.1 explicit octal 0o/0O: 21 significant octal digits fit in int64. */
		p = z + 2;
		while( p < zEnd && p[0] == '0' ){ p++; }
		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }
		if( n <= 21 ){
			return FALSE;
		}
		{ ph7_real dv = 0;
		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){
			dv = dv * 8 + (ph7_real)(q[0] - '0');
		  }
		  *pReal = dv;
		}
		return TRUE;
	}else if( z[0] == '0' ){
		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the
		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1
		 * "0o" marker ends the run and leaves it to the int path (as today). */
		p = z;
		while( p < zEnd && p[0] == '0' ){ p++; }
		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }
		if( n <= 21 ){
			return FALSE;
		}
		{ ph7_real dv = 0;
		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){
			dv = dv * 8 + (ph7_real)(q[0] - '0');
		  }
		  *pReal = dv;
		}
		return TRUE;
	}
	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that
	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)
	 * for php-exact rounding. */
	p = z;
	while( p < zEnd && p[0] == '0' ){ p++; }
	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }
	if( n > 19 || (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){
		*pbDecimal = TRUE;
		return TRUE;
	}
	return FALSE;
}
static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyToken *pToken = pGen->pIn; /* Raw token */
	sxu32 nIdx = 0;
	char zScratch[GEN_NUM_SCRATCH];
	char *zAlloc = 0;
	SyString sNum;
	sxi32 rc;
	SXUNUSED(iCompileFlag); /* cc warning */
	rc = GenStateValidateNumericSeparator(pGen, pToken);
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,
		zScratch, sizeof(zScratch), &sNum, &zAlloc);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	if( pToken->nType & PH7_TK_INTEGER ){
		ph7_value *pObj;
		sxi64 iValue;
		ph7_real rOverflow = 0;
		int bDecimalOverflow = 0;
		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){
			/* Literal exceeds the signed 64-bit range: PHP represents it as a
			 * float instead of wrapping/dropping digits. */
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
				return SXERR_ABORT;
			}
			if( bDecimalOverflow ){
				/* strtod on the decimal token yields php-exact rounding. */
				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);
				PH7_MemObjToReal(pObj);
			}else{
				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);
			}
		}else{
			iValue = PH7_TokenValueToInt64(&sNum);
			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);
			if( pObj == 0 ){
				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
				return SXERR_ABORT;
			}
			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);
		}
	}else{
		/* Real number */
		ph7_value *pObj;
		/* Reserve a new constant */
		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
		if( pObj == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);
		PH7_MemObjToReal(pObj);
	}
	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a single quoted string.
 * According to the PHP language reference manual:
 *
 *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).
 *   To specify a literal single quote, escape it with a backslash (\). To specify a literal
 *   backslash, double it (\\). All other instances of backslash will be treated as a literal
 *   backslash: this means that the other escape sequences you might be used to, such as \r
 *   or \n, will be output literally as specified rather than having any special meaning.
 *
 */
PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */
	const char *zIn,*zCur,*zEnd;
	ph7_value *pObj;
	sxu32 nIdx;
	sxi32 bHasEsc;
	nIdx = 0; /* Prevent compiler warning */
	/* Delimit the string */
	zIn  = pStr->zString;
	zEnd = &zIn[pStr->nByte];
	if( zIn >= zEnd ){
		/* Empty string constant: just use the pre‑allocated index from the VM
		 * rather than reserving a new object each time. */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);
		return SXRET_OK;
	}
	/* A single-quoted literal whose raw source holds a backslash unescapes to a
	 * value that differs from that source (\\ -> \, \' -> '). The literal cache
	 * keys FIND on the raw source text but INSTALL on the unescaped value, so
	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'
	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,
	 * value \\) and load two backslashes. Only cache literals whose value equals
	 * their source, i.e. those with no backslash to unescape. */
	bHasEsc = 0;
	{
		const char *zScan;
		for( zScan = zIn ; zScan < zEnd ; zScan++ ){
			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }
		}
	}
	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){
		/* Already processed,emit the load constant instruction
		 * and return.
		 */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
		return SXRET_OK;
	}
	/* Reserve a new constant */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
		SXUNUSED(iCompileFlag); /* cc warning */
		return SXERR_ABORT;
	}
	PH7_MemObjInitFromString(pGen->pVm,pObj,0);
	/* Compile the node */
	for(;;){
		if( zIn >= zEnd ){
			/* End of input */
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '\\' ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append raw contents*/
			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));
		}
		zIn++;
		if( zIn < zEnd ){
			if( zIn[0] == '\\' ){
				/* A literal backslash */
				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));
			}else if( zIn[0] == '\'' ){
				/* A single quote */
				PH7_MemObjStringAppend(pObj,"'",sizeof(char));
			}else{
				/* verbatim copy */
				zIn--;
				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);
				zIn++;
			}
		}
		/* Advance the stream cursor */
		zIn++;
	}
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	if( !bHasEsc && pStr->nByte < 1024 ){
		/* Install in the literal table (only when value == source; see above) */
		GenStateInstallLiteral(pGen,pObj,nIdx);
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.
 *
 * When the lexer matched the closing marker with leading whitespace on its
 * own line, it stored the indent count in pGen->pIn->pUserData. The marker's
 * indent prefix bytes sit immediately after the stripped body (at
 * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the
 * original source buffer — the buffer is stable through compilation.
 *
 * For each body line, we remove exactly `nIndent` leading bytes that must
 * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)
 * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:
 *   - "Invalid body indentation level (expecting an indentation level of
 *     at least N)" — line too short, or first differing byte is not
 *     whitespace.
 *   - "Invalid indentation - tabs and spaces cannot be mixed" — first
 *     differing byte is whitespace but differs from the marker prefix.
 */
static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)
{
	SyString *pIn = &pGen->pIn->sData;
	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
	const char *zPrefix;
	const char *z, *zEnd;
	char *zBuf, *zDst;
	if( nIndent == 0 ){
		/* Legacy column-0 marker: zero-copy fast path */
		*pOut = *pIn;
		return SXRET_OK;
	}
	/* Recover the marker indent prefix from the original source buffer.
	 * Skip the terminator the lexer stripped: one '\n' plus an optional
	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the
	 * lexer stripped nothing, so this offset is one byte past the true
	 * marker-indent start. That is harmless — the strip loop below never
	 * runs (z == zEnd), and zPrefix is never dereferenced. */
	zPrefix = pIn->zString + pIn->nByte;
	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){
		zPrefix += 2;
	}else{
		zPrefix += 1;
	}
	/* Allocate scratch buffer sized to the original body (always enough). */
	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);
	if( zBuf == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	zDst = zBuf;
	z = pIn->zString;
	zEnd = z + pIn->nByte;
	while( z < zEnd ){
		const char *zLine = z;
		sxu32 nLine;
		int bEmpty;
		while( z < zEnd && z[0] != '\n' ){
			z++;
		}
		nLine = (sxu32)(z - zLine);
		bEmpty = (nLine == 0) || (nLine == 1 && zLine[0] == '\r');
		if( !bEmpty ){
			sxu32 i;
			if( nLine < nIndent ){
				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
					"Invalid body indentation level (expecting an indentation level of at least %u)",
					nIndent);
				return SXERR_ABORT;
			}
			for( i = 0; i < nIndent; i++ ){
				if( zLine[i] != zPrefix[i] ){
					unsigned char c = (unsigned char)zLine[i];
					if( c == ' ' || c == '\t' ){
						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
							"Invalid indentation - tabs and spaces cannot be mixed");
					}else{
						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
							"Invalid body indentation level (expecting an indentation level of at least %u)",
							nIndent);
					}
					return SXERR_ABORT;
				}
			}
			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);
			zDst += nLine - nIndent;
		}else if( nLine == 1 ){
			/* Preserve the stray '\r' on an otherwise empty line */
			*zDst++ = '\r';
		}
		if( z < zEnd ){
			*zDst++ = '\n';
			z++;
		}
	}
	pOut->zString = zBuf;
	pOut->nByte = (sxu32)(zDst - zBuf);
	return SXRET_OK;
}
/*
 * Compile a nowdoc string.
 * According to the PHP language reference manual:
 *
 *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.
 *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.
 *  The construct is ideal for embedding PHP code or other large blocks of text without the
 *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>
 *  construct, in that it declares a block of text which is not for parsing.
 *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier
 *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc
 *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance
 *  of the closing identifier.
 */
PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString sStripped;
	SyString *pStr;
	ph7_value *pObj;
	sxu32 nIdx;
	sxi32 rc;
	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);
	if( rc != SXRET_OK ){
		return rc;
	}
	pStr = &sStripped;
	nIdx = 0; /* Prevent compiler warning */
	if( pStr->nByte <= 0 ){
		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made
		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);
		return SXRET_OK;
	}
	/* Reserve a new constant */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");
		SXUNUSED(iCompileFlag); /* cc warning */
		return SXERR_ABORT;
	}
	/* No processing is done here, simply a memcpy() operation */
	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.
 * According to the PHP language reference manual
 *   When a string is specified in double quotes or with heredoc,variables are parsed within it.
 *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most
 *  common and convenient. It provides a way to embed a variable, an array value, or an object
 *  property in a string with a minimum of effort.
 *  Simple syntax
 *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible
 *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify
 *   the end of the name.
 *   Similarly, an array index or an object property can be parsed. With array indices, the closing
 *   square bracket (]) marks the end of the index. The same rules apply to object properties
 *   as to simple variables.
 *  Complex (curly) syntax
 *   This isn't called complex because the syntax is complex, but because it allows for the use
 *   of complex expressions.
 *   Any scalar variable, array element or object property with a string representation can be
 *   included via this syntax. Simply write the expression the same way as it would appear outside
 *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only
 *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$
 */
static sxi32 GenStateProcessStringExpression(
	ph7_gen_state *pGen, /* Code generator state */
	sxu32 nLine,         /* Line number */
	const char *zIn,     /* Raw expression */
	const char *zEnd     /* End of the expression */
	)
{
	SyToken *pTmpIn,*pTmpEnd;
	SySet sToken;
	sxi32 rc;
	/* Initialize the token set */
	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));
	/* Preallocate some slots */
	SySetAlloc(&sToken,0x08);
	/* Tokenize the text */
	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);
	/* Swap delimiter */
	pTmpIn  = pGen->pIn;
	pTmpEnd = pGen->pEnd;
	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);
	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Restore token stream */
	pGen->pIn  = pTmpIn;
	pGen->pEnd = pTmpEnd;
	/* Release the token set */
	SySetRelease(&sToken);
	/* Compilation result */
	return rc;
}
/*
 * Reserve a new constant for a double quoted/heredoc string.
 */
static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)
{
	ph7_value *pConstObj;
	sxu32 nIdx = 0;
	/* Reserve a new constant */
	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pConstObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");
		return 0;
	}
	(*pCount)++;
	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	return pConstObj;
}
/*
 * Compile a double quoted/heredoc string.
 * According to the PHP language reference manual
 * Heredoc
 *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier
 *  is provided, then a newline. The string itself follows, and then the same identifier again
 *  to close the quotation.
 *  The closing identifier must begin in the first column of the line. Also, the identifier must
 *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric
 *  characters and underscores, and must start with a non-digit character or underscore.
 *  Warning
 *  It is very important to note that the line with the closing identifier must contain
 *  no other characters, except possibly a semicolon (;). That means especially that the identifier
 *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.
 *  It's also important to realize that the first character before the closing identifier must
 *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.
 *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.
 *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing
 *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before
 *  the end of the current file, a parse error will result at the last line.
 *  Heredocs can not be used for initializing class properties.
 * Double quoted
 *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:
 *  Escaped characters Sequence 	Meaning
 *  \n linefeed (LF or 0x0A (10) in ASCII)
 *  \r carriage return (CR or 0x0D (13) in ASCII)
 *  \t horizontal tab (HT or 0x09 (9) in ASCII)
 *  \v vertical tab (VT or 0x0B (11) in ASCII)
 *  \e escape (ESC or 0x1B (27) in ASCII)
 *  \f form feed (FF or 0x0C (12) in ASCII)
 *  \\ backslash
 *  \$ dollar sign
 *  \" double-quote
 *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,
 *      which silently overflows to fit in a byte (e.g. "\400" === "\000")
 *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation
 *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,
 *      which will be output to the string as that codepoint's UTF-8 representation
 * As in single quoted strings, escaping any other character will result in the backslash being printed too.
 * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)
 * The most important feature of double-quoted strings is the fact that variable names will be expanded.
 * See string parsing for details.
 */
/*
 * Line number of an escape sequence inside the string body being compiled:
 * the token's line plus every newline before the escape (php reports the
 * escape's own line, not the string's opening line). A heredoc body starts
 * on the line after the '<<<' marker, hence the +1.
 */
static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)
{
	const char *z = pGen->pIn->sData.zString;
	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);
	for( ; z < zPos ; z++ ){
		if( z[0] == '\n' ){
			nLine++;
		}
	}
	return nLine;
}
/* bHeredoc: php strips the backslash from '\"' only when '"' is the active
 * quote character; a heredoc has none, so '\"' stays verbatim there. */
static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)
{
	SyString *pStr = &pGen->pIn->sData; /* Raw token value */
	const char *zIn,*zCur,*zEnd;
	ph7_value *pObj = 0;
	sxi32 iCons;
	sxi32 rc;
	/* Delimit the string */
	zIn  = pStr->zString;
	zEnd = &zIn[pStr->nByte];
	if( zIn >= zEnd ){
		/* Empty string: use the shared constant reserved at VM initialization.
		 * This avoids creating a new literal for every occurrence and keeps the
		 * literal table from growing when many "" literals appear in the source.
		 */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);
		return SXRET_OK;
	}
	zCur = 0;
	/* Compile the node */
	iCons = 0;
	for(;;){
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '\\'  ){
			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){
				break;
			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&
				(((unsigned char)zIn[1] >= 0xc0 || SyisAlpha(zIn[1]) || zIn[1] == '{' || zIn[1] == '_')) ){
					break;
			}
			zIn++;
		}
		if( zIn > zCur ){
			if( pObj == 0 ){
				pObj = GenStateNewStrObj(&(*pGen),&iCons);
				if( pObj == 0 ){
					return SXERR_ABORT;
				}
			}
			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));
		}
		if( zIn >= zEnd ){
			break;
		}
		if( zIn[0] == '\\' ){
			const char *zPtr = 0;
			sxu32 n;
			zIn++;
			if( pObj == 0 ){
				pObj = GenStateNewStrObj(&(*pGen),&iCons);
				if( pObj == 0 ){
					return SXERR_ABORT;
				}
			}
			if( zIn >= zEnd ){
				/* Lone backslash at the very end of the body: php keeps it */
				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));
				break;
			}
			n = sizeof(char); /* size of conversion */
			switch( zIn[0] ){
			case '$':
				/* Dollar sign */
				PH7_MemObjStringAppend(pObj,"$",sizeof(char));
				break;
			case '\\':
				/* A literal backslash */
				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));
				break;
			case 'e':
				/* Escape (ESC) ASCII code 27 */
				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));
				break;
			case 'f':
				/* Form-feed (FF)[ctrl+l] ASCII code 12 */
				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));
				break;
			case 'n':
				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */
				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));
				break;
			case 'r':
				/* Carriage return (CR)[ctrl+m] ASCII code 13 */
				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));
				break;
			case 't':
				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */
				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));
				break;
			case 'v':
				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */
				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));
				break;
			case '"':
				if( bHeredoc ){
					/* No active quote char in a heredoc: php keeps \" verbatim */
					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);
				}else{
					/* Double quote */
					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));
				}
				break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7': {
				/* \[0-7]{1,3}: a character in octal notation. A value above \377
				 * warns and wraps to the low byte, matching php 8. */
				int c = 0;
				char cOut;
				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){
					if( zPtr >= zEnd || zPtr[0] < '0' || zPtr[0] > '7' ){
						break;
					}
					c = c * 8 + (zPtr[0] - '0');
				}
				if( c > 0xFF ){
					SyString sSeq;
					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));
					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),
						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);
					c &= 0xFF;
				}
				cOut = (char)c; /* value byte, independent of host endianness */
				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));
				n = (sxu32)(zPtr-zIn);
				break;
			}
			case 'x':
				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){
					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */
					int c = SyHexToint(zIn[1]);
					char cOut;
					n += sizeof(char);
					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){
						c = (c << 4) + SyHexToint(zIn[2]);
						n += sizeof(char);
					}
					cOut = (char)c; /* value byte, independent of host endianness */
					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));
				}else{
					/* Not an escape: keep the backslash, as php does */
					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);
				}
				break;
			case 'u':
				if( &zIn[1] < zEnd && zIn[1] == '{'
				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){
					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).
					 * php encodes surrogates verbatim, so the only invalid value
					 * is > U+10FFFF; malformed/empty braces are a compile error.
					 * "\u{$..." is excluded above: php treats it as a literal \u
					 * followed by {$...} curly interpolation. */
					sxu32 nCp = 0;
					zPtr = &zIn[2];
					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){
						if( nCp <= 0x10FFFF ){
							/* stop accumulating once out of range: keeps a long
							 * digit run from wrapping sxu32 */
							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);
						}
						zPtr++;
					}
					if( zPtr == &zIn[2] || zPtr >= zEnd || zPtr[0] != '}' ){
						/* Error recorded (nErr>0 fails the whole compile); consume the
						 * malformed sequence so later errors are still reported. */
						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),
							"Invalid UTF-8 codepoint escape sequence");
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						n = (sxu32)(zPtr-zIn);
						if( zPtr < zEnd && zPtr[0] == '}' ){
							n += sizeof(char);
						}
						break;
					}
					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */
					if( nCp > 0x10FFFF ){
						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),
							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						break;
					}
					{
						char zUtf[4];
						sxu8 *zOut = (sxu8 *)zUtf;
						SX_WRITE_UTF8(zOut,nCp);
						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));
					}
				}else{
					/* Not an escape: keep the backslash, as php does */
					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);
				}
				break;
			default:
				/* Unrecognized escape: keep the backslash, as php does.
				 * zIn[-1] is the backslash itself, so both bytes are contiguous
				 * in the source buffer — one batched append. */
				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);
				break;
			}
			/* Advance the stream cursor */
			zIn += n;
			continue;
		}
		if( zIn[0] == '{' ){
			/* Curly syntax */
			const char *zExpr;
			sxi32 iNest = 1;
			zIn++;
			zExpr = zIn;
			/* Synchronize with the next closing curly braces */
			while( zIn < zEnd ){
				if( zIn[0] == '{' ){
					/* Increment nesting level */
					iNest++;
				}else if(zIn[0] == '}' ){
					/* Decrement nesting level */
					iNest--;
					if( iNest <= 0 ){
						break;
					}
				}
				zIn++;
			}
			/* Process the expression */
			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( rc != SXERR_EMPTY ){
				++iCons;
			}
			if( zIn < zEnd ){
				/* Jump the trailing curly */
				zIn++;
			}
		}else{
			/* Simple syntax */
			const char *zExpr = zIn;
			/* Assemble variable name */
			for(;;){
				/* Jump leading dollars */
				while( zIn < zEnd && zIn[0] == '$' ){
					zIn++;
				}
				for(;;){
					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) || zIn[0] == '_' ) ){
						zIn++;
					}
					if((unsigned char)zIn[0] >= 0xc0 ){
						/* UTF-8 stream */
						zIn++;
						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){
							zIn++;
						}
						continue;
					}
					break;
				}
				if( zIn >= zEnd ){
					break;
				}
				if( zIn[0] == '[' ){
					sxi32 iSquare = 1;
					zIn++;
					while( zIn < zEnd ){
						if( zIn[0] == '[' ){
							iSquare++;
						}else if (zIn[0] == ']' ){
							iSquare--;
							if( iSquare <= 0 ){
								break;
							}
						}
						zIn++;
					}
					if( zIn < zEnd ){
						zIn++;
					}
					break;
				}else if(zIn[0] == '{' ){
					sxi32 iCurly = 1;
					zIn++;
					while( zIn < zEnd ){
						if( zIn[0] == '{' ){
							iCurly++;
						}else if (zIn[0] == '}' ){
							iCurly--;
							if( iCurly <= 0 ){
								break;
							}
						}
						zIn++;
					}
					if( zIn < zEnd ){
						zIn++;
					}
					break;
				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){
					/* Member access operator '->' */
					zIn += 2;
				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){
					/* Static member access operator '::' */
					zIn += 2;
				}else{
					break;
				}
			}
			/*
			 * "$a[name]" — php's SIMPLE syntax takes an unquoted subscript as the string key
			 * 'name', never as a constant. PH7 handed "$a[name]" straight to the expression
			 * compiler, where the bare word only resolved because an unknown constant used to
			 * fall back to its own name as a string. With undefined constants now a real
			 * Error, quote the key here so the simple syntax keeps meaning what php means.
			 * A numeric ($a[0]) or variable ($a[$k]) subscript is already unambiguous.
			 */
			{
				const char *zBr = zExpr;
				while( zBr < zIn && zBr[0] != '[' ){
					zBr++;
				}
				if( zBr < zIn && zIn[-1] == ']' ){
					const char *zKey = &zBr[1];
					const char *zKeyEnd = &zIn[-1];
					const char *zScan = zKey;
					int bBare = (zKey < zKeyEnd) && !SyisDigit(zKey[0]);
					while( bBare && zScan < zKeyEnd ){
						if( !SyisAlphaNum(zScan[0]) && zScan[0] != '_' ){
							bBare = 0;
						}
						zScan++;
					}
					if( bBare ){
						SyBlob sSub;
						SyBlobInit(&sSub,&pGen->pVm->sAllocator);
						SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));
						SyBlobAppend(&sSub,"['",2);
						SyBlobAppend(&sSub,zKey,(sxu32)(zKeyEnd - zKey));
						SyBlobAppend(&sSub,"']",2);
						rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,
							(const char *)SyBlobData(&sSub),
							(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));
						SyBlobRelease(&sSub);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						if( rc != SXERR_EMPTY ){
							++iCons;
						}
						pObj = 0;
						continue;
					}
				}
			}
			/*
			 * "${...}" string interpolation (every form: ${name}, ${expr}, ${$x}) was
			 * DEPRECATED by php 8.2 in favor of the canonical "{$...}". PHL targets php's
			 * *non-deprecated* surface, so it is a hard parse error here — never silently
			 * rewritten. The canonical "{$var}" reaches this compiler by a different path
			 * and is unaffected.
			 */
			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' ){
				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,
					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");
				return SXERR_ABORT;
			}
			/* Process the expression */
			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( rc != SXERR_EMPTY ){
				++iCons;
			}
		}
		/* Invalidate the previously used constant */
		pObj = 0;
	}/*for(;;)*/
	if( iCons > 1 ){
		/* Concatenate all compiled constants */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a double quoted string.
 *  See the block-comment above for more information.
 */
PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxi32 rc;
	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);
	SXUNUSED(iCompileFlag); /* cc warning */
	/* Compilation result */
	return rc;
}
/*
 * Compile a Heredoc string.
 *  See the block-comment above for more information.
 */
PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString sOrig, sStripped;
	sxi32 rc;
	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Temporarily swap in the dedented body so GenStateCompileString
	 * (which reads pGen->pIn->sData directly) sees the stripped content.
	 * Restore before returning so downstream code that references pIn is
	 * unaffected, including on the error path. */
	sOrig = pGen->pIn->sData;
	pGen->pIn->sData = sStripped;
	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);
	pGen->pIn->sData = sOrig;
	SXUNUSED(iCompileFlag); /* cc warning */
	return rc;
}
/*
 * Compile an array entry whether it is a key or a value.
 *  Notes on array entries.
 *  According to the PHP language reference manual
 *  An array can be created by the array() language construct.
 *  It takes as parameters any number of comma-separated key => value pairs.
 *  array(  key =>  value
 *    , ...
 *    )
 *  A key may be either an integer or a string. If a key is the standard representation
 *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while
 *  "08" will be interpreted as "08"). Floats in key are truncated to integer.
 *  The indexed and associative array types are the same type in PHP, which can both
 *  contain integer and string indices.
 *  A value can be any PHP type.
 *  If a key is not specified for a value, the maximum of the integer indices is taken
 *  and the new key will be that value plus 1. If a key that already has an assigned value
 *  is specified, that value will be overwritten.
 */
PH7_PRIVATE sxi32 GenStateCompileArrayEntry(
	ph7_gen_state *pGen, /* Code generator state */
	SyToken *pIn,        /* Token stream */
	SyToken *pEnd,       /* End of the token stream */
	sxi32 iFlags,        /* Compilation flags */
	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */
	)
{
	SyToken *pTmpIn,*pTmpEnd;
	sxi32 rc;
	/* Swap token stream */
	SWAP_DELIMITER(pGen,pIn,pEnd);
	/* Compile the expression*/
	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);
	/* Restore token stream */
	RE_SWAP_DELIMITER(pGen);
	return rc;
}
/*
 * Expression tree validator callback for the 'array' language construct.
 * Return SXRET_OK if the tree is valid. Any other return value indicates
 * an invalid expression tree and this function will generate the appropriate
 * error message.
 * See the routine responible of compiling the array language construct
 * for more inforation.
 */
static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	sxi32 rc = SXRET_OK;
	if( pRoot->pOp ){
		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&
			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */
			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){
			/* Unexpected expression */
			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");
			if( rc != SXERR_ABORT ){
				rc = SXERR_INVALID;
			}
		}
	}else if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);
		if( rc != SXERR_ABORT ){
			rc = SXERR_INVALID;
		}
	}
	return rc;
}
/*
 * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's
 * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside
 * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or
 * inside a match() {...} arm — none of which are key/value separators. Returns a
 * pointer to the '=>' token, or pEnd if the entry has no top-level separator.
 */
PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)
{
	SyToken *pCur = pStart;
	sxi32 iNest = 0;
	while( pCur < pEnd ){
		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){
			return pCur;
		}
		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.
		 * The '=>' inside an arrow function introduces the expression body,
		 * not an entry separator. Skip past the signature.
		 */
		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){
			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);
			SyToken *pFn = pCur;
			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd
				&& (pCur[1].nType & PH7_TK_KEYWORD)
				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){
				pFn = &pCur[1];
				nKw = PH7_TKWRD_FN;
			}
			if( nKw == PH7_TKWRD_FN ){
				pCur = pFn + 1; /* past 'fn' */
				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){
					pCur++;
				}
				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){
					pCur++;
					PH7_DelimitNestedTokens(pCur,pEnd,
						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);
					if( pCur < pEnd ){
						pCur++;
					}
				}
				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){
					pCur++;
					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)
						&& pCur->sData.nByte == 1
						&& pCur->sData.zString[0] == '?' ){
						pCur++;
					}
					if( pCur < pEnd
						&& (pCur->nType & (PH7_TK_KEYWORD|PH7_TK_ID)) ){
						pCur++;
					}
				}
				/* The rest of the entry is the arrow-function body — no outer
				 * key to extract. */
				return pEnd;
			}
			/* Match expression (PHP 8.0): the '=>' inside match arms is not an
			 * entry separator. Skip past the full match span. */
			if( nKw == PH7_TKWRD_MATCH ){
				pCur++; /* past 'match' */
				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){
					pCur++;
					PH7_DelimitNestedTokens(pCur,pEnd,
						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);
					if( pCur < pEnd ){
						pCur++;
					}
				}
				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){
					pCur++;
					PH7_DelimitNestedTokens(pCur,pEnd,
						PH7_TK_OCB,PH7_TK_CCB,&pCur);
					if( pCur < pEnd ){
						pCur++;
					}
				}
				continue;
			}
		}
		if( pCur->nType & (PH7_TK_LPAREN/*'('*/|PH7_TK_OSB/*'['*/|PH7_TK_OCB/*'{'*/) ){
			iNest++;
		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/|PH7_TK_CSB/*']'*/|PH7_TK_CCB/*'}'*/) ){
			/* Don't worry about mismatched brackets here, the expression
			 * parser will shortly detect any syntax error. */
			iNest--;
		}
		pCur++;
	}
	return pEnd;
}
/*
 * Compile the body of an array literal (shared by array() and short syntax []).
 * Assumes pGen->pIn points to the first content token and pGen->pEnd points
 * one past the last content token (i.e. the delimiters have been excluded).
 */
static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)
{
	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */
	SyToken *pKey,*pCur;
	sxi32 iEmitRef = 0;
	sxi32 iSpread = 0;
	sxi32 nPair = 0;
	sxi32 rc;
	xValidator = 0;
	for(;;){
		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma
		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just
		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma
		 * is legal and is handled by the loop exiting on the next pass. */
		{
			int nSkip = 0;
			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){
				nSkip++;
				pGen->pIn++;
			}
			if( nSkip > 1 || (nSkip > 0 && nPair < 1) ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,
					"Cannot use empty array elements in arrays");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
		}
		pCur = pGen->pIn;
		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){
			/* No more entry to process */
			break;
		}
		if( pCur >= pGen->pIn ){
			continue;
		}
		/* Compile the key if available */
		pKey = pCur;
		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);
		rc = SXERR_EMPTY;
		if( pCur < pGen->pIn ){
			if( pKey == pCur ){
				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects
				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting
				 * source php refuses. (The `else if` below could never see this: the arrow
				 * IS found here, so control never reached it.)
				 * php names the literal's own closer, so short syntax expects ']'. */
				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))
					? "\"]\"" : "\")\"";
				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			if( &pCur[1] >= pGen->pIn ){
				/* `array(1 => )`: php names the token that SHOULD have started the value —
				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0
				 * makes the helper reach for the token past this entry's slice. */
				rc = PH7_GenSyntaxError(&(*pGen),0,0);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			/* Compile the expression holding the key */
			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,
				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			pCur++; /* Jump the '=>' operator */
		}else{
			/* Reset back the cursor and point to the entry value */
			pCur = pKey;
		}
		if( rc == SXERR_EMPTY ){
			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key
			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);
		}
		if( pCur->nType & PH7_TK_AMPER /*'&'*/){
			/* Insertion by reference, [i.e: $a = array(&$x);] */
			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */
			iEmitRef = 1;
			pCur++; /* Jump the '&' token */
			if( pCur >= pGen->pIn ){
				/* Missing value */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
		}
		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with
		 * string-key support since PHP 8.1). The parser strips the '...' inside
		 * ExprExtractNode; we only need to know it's there so we can emit
		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the
		 * resulting hashmap rather than insert it as a scalar entry. */
		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;
		if( iSpread && (rc != SXERR_EMPTY || iEmitRef) ){
			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the
			 * '...' token cannot follow either '=>' or '&' inside an array
			 * literal. Emit the same Parse-error wording PHP uses so the
			 * output is engine-portable. */
			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,
				"syntax error, unexpected token \"...\"");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXRET_OK;
		}
		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an
		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,
		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)
		 * instead of a read-only load — which also keeps the undefined-key
		 * warning (a read-only diagnostic) from false-firing here. */
		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,
			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE
			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,
			xValidator);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( iSpread ){
			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);
		}else if( iEmitRef ){
			/* Emit the load reference instruction */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);
		}
		xValidator = 0;
		iEmitRef = 0;
		iSpread = 0;
		nPair++;
	}
	/* Emit the load map instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile the 'array' language construct.
 *	 According to the PHP language reference manual
 *   An array in PHP is actually an ordered map. A map is a type that associates
 *   values to keys. This type is optimized for several different uses; it can
 *   be treated as an array, list (vector), hash table (an implementation of a map)
 *   dictionary, collection, stack, queue, and probably more. As array values can be
 *   other arrays, trees and multidimensional arrays are also possible.
 */
PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */
	pGen->pIn += 2;
	pGen->pEnd--;
	SXUNUSED(iCompileFlag);
	return GenStateCompileArrayBody(pGen);
}
/*
 * Compile the PHP 8.5 clone(...) call form:
 *   clone($object)                          -> identical to the `clone $object` operator
 *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the
 *                                              property updates as scope-aware writes
 *   clone(object: $o, withProperties: [..]) -> the named-argument spelling
 * Codegen: compile the object argument and emit OP_CLONE (which clones and runs
 * __clone()); if a withProperties argument is present, compile it and emit
 * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),
 * honouring visibility / readonly-set-scope / typed-property enforcement in the
 * calling scope. The parser (ExprExtractNode) delimited this node's tokens as
 * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.
 */
PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyToken *pIn,*pEnd,*pNext;
	SyToken *pObjStart = 0,*pObjEnd = 0;
	SyToken *pUpdStart = 0,*pUpdEnd = 0;
	int nArg = 0;
	sxi32 rc;
	SXUNUSED(iCompileFlag);
	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */
	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */
	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */
	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */
	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){
		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,
			"clone(...) first-class callable form is not yet supported");
	}
	/* Split the (at most two) comma-separated arguments, tolerating named labels. */
	while( pIn < pEnd ){
		SyToken *pArgStart,*pArgEnd,*pName = 0;
		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){
			break;
		}
		pArgStart = pIn;
		pArgEnd   = pNext;
		/* Named-argument label: <ID|keyword> ':' expr. A single ':' is PH7_TK_COLON;
		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */
		if( (pArgEnd - pArgStart) >= 2
			&& (pArgStart[0].nType & (PH7_TK_ID|PH7_TK_KEYWORD))
			&& (pArgStart[1].nType & PH7_TK_COLON) ){
			pName = pArgStart;
			pArgStart += 2;
		}
		if( pName ){
			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:`
			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */
			if( pName->sData.nByte == sizeof("object")-1
				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){
				pObjStart = pArgStart; pObjEnd = pArgEnd;
			}else if( pName->sData.nByte == sizeof("withProperties")-1
				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){
				pUpdStart = pArgStart; pUpdEnd = pArgEnd;
			}else{
				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,
					"Unknown named parameter $%z",&pName->sData);
			}
		}else if( nArg == 0 ){
			pObjStart = pArgStart; pObjEnd = pArgEnd;
		}else if( nArg == 1 ){
			pUpdStart = pArgStart; pUpdEnd = pArgEnd;
		}else{
			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,
				"clone() expects at most 2 arguments");
		}
		nArg++;
		pIn = pNext;
		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){
			pIn++; /* step over the argument separator */
		}
	}
	if( pObjStart == 0 || pObjStart >= pObjEnd ){
		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
			"clone() expects at least 1 argument, 0 given");
	}
	/* Object argument -> clone (+ __clone()). */
	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);
	/* Property updates (evaluated after __clone runs). */
	if( pUpdStart && pUpdStart < pUpdEnd ){
		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);
	}
	return SXRET_OK;
}
/*
 * Compile a short array literal using the PHP 5.4 bracket syntax.
 * [1, 2, 3] is equivalent to array(1, 2, 3).
 * ['key' => 'value'] is equivalent to array('key' => 'value').
 */
PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	/* Jump the leading '[', exclude trailing ']'. */
	pGen->pIn++;
	pGen->pEnd--;
	SXUNUSED(iCompileFlag);
	return GenStateCompileArrayBody(pGen);
}
/*
 * Expression tree validator callback for the 'list' language construct.
 * Return SXRET_OK if the tree is valid. Any other return value indicates
 * an invalid expression tree and this function will generate the appropriate
 * error message.
 * See the routine responible of compiling the list language construct
 * for more inforation.
 */
static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	sxi32 rc = SXRET_OK;
	if( pRoot->pOp ){
		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */
			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){
				/* Unexpected expression */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
					"Assignments can only happen to writable values");
				if( rc != SXERR_ABORT ){
					rc = SXERR_INVALID;
				}
		}
	}else if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
			"Assignments can only happen to writable values");
		if( rc != SXERR_ABORT ){
			rc = SXERR_INVALID;
		}
	}
	return rc;
}
/*
 * Compile the 'list' language construct.
 *  According to the PHP language reference
 *  list(): Assign variables as if they were an array.
 *  list() is used to assign a list of variables in one operation.
 *  Description
 *   array list (mixed $varname [, mixed $... ] )
 *   Like array(), this is not really a function, but a language construct.
 *   list() is used to assign a list of variables in one operation.
 *  Parameters
 *   $varname: A variable.
 *  Return Values
 *   The assigned array.
 */
/* Nested list entry recorded during first pass of list body compilation */
struct NestedListEntry {
	sxi32 nIndex;        /* Position in the outer list (0-based) */
	SyToken *pStart;     /* Token range: start of nested construct */
	SyToken *pEnd;       /* Token range: past closing delimiter */
	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */
};
/*
 * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where
 * every entry has the form `keyExpr => target`. The source array is on the stack
 * top on entry and remains there on exit, mirroring the positional LOAD_LIST
 * path so the caller's teardown is unchanged. For each entry: DUP the source,
 * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,
 * like a normal subscript read), then assign the fetched value to the target — a
 * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a
 * normal assignment (the value sits below the lvalue-load, exactly as in
 * GenStateEmitExprCode where the assignment RHS precedes the LHS load).
 */
static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)
{
	SyToken *pNext;
	sxi32 rc;
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){
		SyToken *pArrow,*pTarget;
		/* Split `keyExpr => target` at the top-level '=>' */
		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);
		pTarget = &pArrow[1];
		if( pArrow <= pGen->pIn || pTarget >= pNext ){
			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects
			 * both. Reject rather than silently emitting unbalanced bytecode. */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
				"Cannot use empty array entries in keyed array assignment");
			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
		}
		/* DUP the source array (it is on the stack top) */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);
		/* Compile the key expression; it is pushed above the DUP'd source */
		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].
		 * iP2=7 is the keyed-destructuring read context: an array source reads like
		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;
		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),
		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"
		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);
		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)
			|| ( (pTarget->nType & PH7_TK_KEYWORD)
				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){
			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].
			 * Treat source[key] as the inner body's source, then drop the
			 * leftover it leaves behind (mirrors the positional nested path). */
			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;
			SyToken *pSavedIn = pGen->pIn;
			SyToken *pSavedEnd = pGen->pEnd;
			pGen->pIn = pTarget;
			pGen->pEnd = pNext;
			rc = isShort ? PH7_CompileShortList(&(*pGen),0)
			             : PH7_CompileList(&(*pGen),0);
			pGen->pIn = pSavedIn;
			pGen->pEnd = pSavedEnd;
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}else{
			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]
			 * is already on the stack as the value; compiling the target appends
			 * its lvalue-load, which we fold into a STORE just as a normal
			 * assignment does. */
			VmInstr *pInstr;
			sxi32 iVmOp = PH7_OP_STORE;
			sxi32 iP1 = 0, iP2 = 0;
			void *p3 = 0;
			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,
				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);
			if( rc != SXRET_OK ){
				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
			}
			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){
				if( pInstr->iOp == PH7_OP_MEMBER ){
					iP2 = 1; /* member store: keep MEMBER, store value below it */
				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){
					iVmOp = PH7_OP_STORE_IDX;
					iP1 = pInstr->iP1;
					(void)PH7_VmPopInstr(pGen->pVm);
				}else{
					p3 = pInstr->p3; /* named store: $v = value */
					(void)PH7_VmPopInstr(pGen->pVm);
				}
			}
			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);
			/* STORE leaves the assigned value on the stack top; drop it so the
			 * source array is back on top for the next entry. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
		pGen->pIn = &pNext[1];
	}
	return SXRET_OK;
}
/*
 * Shared body for list() and short list [...] compilation.
 * Assumes pGen->pIn and pGen->pEnd are already positioned past
 * the opening delimiter and before the closing delimiter.
 */
static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)
{
	SySet sNested; /* Dynamically-sized container of NestedListEntry */
	SyToken *pNext;
	SyToken *pClassifyIn;
	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;
	sxi32 nExpr;
	sxi32 rc;
	/* First pass: classify entries as keyed (`k => v`), positional, or empty
	 * skip slots ([,]). A list level must be entirely keyed or entirely
	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed
	 * list. */
	pClassifyIn = pGen->pIn;
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){
		if( pGen->pIn >= pNext ){
			nEmpty++;
		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){
			nKeyed++;
		}else{
			nPositional++;
		}
		pGen->pIn = &pNext[1];
	}
	pGen->pIn = pClassifyIn;
	if( nKeyed > 0 && nEmpty > 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"Cannot use empty array entries in keyed array assignment");
		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
	}
	if( nKeyed > 0 && nPositional > 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"Cannot mix keyed and unkeyed array entries in assignments");
		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
	}
	if( nKeyed > 0 ){
		return GenStateCompileKeyedListBody(pGen);
	}
	nExpr = 0;
	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){
		if( pGen->pIn < pNext ){
			/* Check for nested list() */
			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&
				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){
				/* Record this nested list for post-processing */
				SyToken *pListEnd = 0;
				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){
					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);
				}
				if( pListEnd ){
					struct NestedListEntry sEntry;
					sEntry.nIndex = nExpr;
					sEntry.pStart = pGen->pIn;
					sEntry.pEnd = pListEnd + 1;
					sEntry.isShort = 0;
					SySetPut(&sNested,(const void *)&sEntry);
				}
				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
			}else if( pGen->pIn->nType & PH7_TK_OSB ){
				/* Nested short destructuring [...] */
				SyToken *pBracketEnd = 0;
				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);
				if( pBracketEnd ){
					struct NestedListEntry sEntry;
					sEntry.nIndex = nExpr;
					sEntry.pStart = pGen->pIn;
					sEntry.pEnd = pBracketEnd + 1;
					sEntry.isShort = 1;
					SySetPut(&sNested,(const void *)&sEntry);
				}
				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
			}else{
				/* Compile the expression holding the variable */
				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);
				if( rc != SXRET_OK ){
					SySetRelease(&sNested);
					return SXRET_OK;
				}
			}
		}else{
			/* Empty entry,load NULL */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);
		}
		nExpr++;
		/* Advance the stream cursor */
		pGen->pIn = &pNext[1];
	}
	/* Emit the LOAD_LIST instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);
	/* After LOAD_LIST, the source array is still on the stack top.
	 * For each nested entry, emit code to extract the sub-array
	 * at the corresponding index and recursively destructure it.
	 */
	if( SySetUsed(&sNested) > 0 ){
		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);
		sxu32 i;
		for(i = 0; i < SySetUsed(&sNested); i++){
			SyToken *pSavedIn = pGen->pIn;
			SyToken *pSavedEnd = pGen->pEnd;
			ph7_value *pIdx;
			sxu32 nConstIdx;
			/* DUP the source array (it's on stack top) */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);
			/* Push the integer index for this nested entry */
			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);
			if( pIdx == 0 ){
				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");
				SySetRelease(&sNested);
				return SXERR_ABORT;
			}
			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);
			/* LOAD_IDX: pop index, replace DUP'd source with source[index].
			 * iP2=2 signals the VM to emit an "Undefined array key" warning
			 * when the key is missing (PHP-compatible list destructuring).
			 */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);
			/* Recursively compile the inner list */
			pGen->pIn = apNested[i].pStart;
			pGen->pEnd = apNested[i].pEnd;
			if( apNested[i].isShort ){
				rc = PH7_CompileShortList(&(*pGen),0);
			}else{
				rc = PH7_CompileList(&(*pGen),0);
			}
			pGen->pIn = pSavedIn;
			pGen->pEnd = pSavedEnd;
			if( rc == SXERR_ABORT ){
				SySetRelease(&sNested);
				return SXERR_ABORT;
			}
			/* Pop the leftover source[index] from the inner LOAD_LIST */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
	}
	SySetRelease(&sNested);
	/* Node successfully compiled */
	return SXRET_OK;
}
PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */
	pGen->pIn += 2;
	pGen->pEnd--;
	SXUNUSED(iCompileFlag);
	return GenStateCompileListBody(pGen);
}
PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	/* Jump the leading '[', exclude trailing ']'. */
	pGen->pIn++;
	pGen->pEnd--;
	SXUNUSED(iCompileFlag);
	return GenStateCompileListBody(pGen);
}
/* Forward declarations */
/*
 * Compile an annoynmous function or a closure.
 * According to the PHP language reference
 *  Anonymous functions, also known as closures, allow the creation of functions
 *  which have no specified name. They are most useful as the value of callback
 *  parameters, but they have many other uses. Closures can also be used as
 *  the values of variables; Assigning a closure to a variable uses the same
 *  syntax as any other assignment, including the trailing semicolon:
 *  Example Anonymous function variable assignment example
 * <?php
 * $greet = function($name)
 * {
 *    printf("Hello %s\r\n", $name);
 * };
 * $greet('World');
 * $greet('PHP');
 * ?>
 * Note that the implementation of annoynmous function and closure under
 * PH7 is completely different from the one used by the zend engine.
 */
PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */
	char zName[512];         /* Unique lambda name */
	static int iCnt = 1;     /* There is no worry about thread-safety here,because only
							  * one thread is allowed to compile the script.
						      */
	SyString sName;
	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia
	                              * is keyed to this ['static'] 'function' token */
	sxu32 nKwLine;
	sxi32 iFlags = 0;
	sxu32 nLen;
	sxi32 rc;
	SXUNUSED(iCompileFlag); /* cc warning */

	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */
	if( (pGen->pIn->nType & PH7_TK_KEYWORD)
		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){
		/* Static closure: no $this auto-capture, bind refused */
		iFlags |= VM_FUNC_STATIC_CL;
		pGen->pIn++; /* Jump the 'static' keyword */
	}
	pGen->pIn++; /* Jump the 'function' keyword */
	if( pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD) ){
		pGen->pIn++;
	}
	/* Generate a unique name */
	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	/* Make sure the generated name is unique */
	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){
		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	}
	SyStringInitFromBuf(&sName,zName,nLen);
	/* Compile the lambda body */
	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( pAnnonFunc ){
		pAnnonFunc->nLine = nKwLine;
		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia
		 * sidecar keys them to the closure's first keyword token. */
		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for
	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);
	 * the handler wraps either in a Closure instance. */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Add a free variable to the arrow function's closure environment, unless
 * it is 'this' (handled separately), is shadowed by a parameter at any
 * enclosing arrow level, or has already been captured.
 */
static sxi32 GenStateArrowAddCapture(
	ph7_gen_state *pGen,
	ph7_vm_func *pFunc,
	const char *zName,
	sxu32 nByte,
	SyString *aShadow,
	sxu32 nShadow)
{
	ph7_vm_func_closure_env sEnv;
	ph7_vm_func_closure_env *aEnv;
	sxu32 n, nEnv;
	char *zDup;
	if( nByte == 0 ){
		return SXRET_OK;
	}
	if( nByte == sizeof("this")-1
		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){
		return SXRET_OK;
	}
	for( n = 0 ; n < nShadow ; n++ ){
		if( SyStringLength(&aShadow[n]) == nByte
			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){
			return SXRET_OK;
		}
	}
	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);
	nEnv = SySetUsed(&pFunc->aClosureEnv);
	for( n = 0 ; n < nEnv ; n++ ){
		if( SyStringLength(&aEnv[n].sName) == nByte
			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){
			return SXRET_OK;
		}
	}
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);
	if( zDup == 0 ){
		return SXERR_ABORT;
	}
	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
	sEnv.iFlags = 0;
	sEnv.nIdx = SXU32_HIGH;
	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);
	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);
	return SXRET_OK;
}
/*
 * Walk the raw body of a double-quoted string or heredoc, extracting every
 * unescaped $<identifier> reference. The semantics mirror the "simple
 * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,
 * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.
 */
static sxi32 GenStateArrowScanInterpolatedString(
	ph7_gen_state *pGen,
	ph7_vm_func *pFunc,
	const char *zIn,
	const char *zEnd,
	SyString *aShadow,
	sxu32 nShadow)
{
	sxi32 rc;
	while( zIn < zEnd ){
		if( zIn[0] == '\\' ){
			zIn++;
			if( zIn < zEnd ){
				zIn++;
			}
			continue;
		}
		if( zIn[0] == '$' && &zIn[1] < zEnd
			&& ((unsigned char)zIn[1] >= 0xc0
				|| SyisAlpha(zIn[1]) || zIn[1] == '_') ){
			const char *zName;
			zIn++; /* skip '$' */
			zName = zIn;
			while( zIn < zEnd ){
				unsigned char c = (unsigned char)zIn[0];
				if( c >= 0xc0 ){
					zIn++;
					while( zIn < zEnd
						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){
						zIn++;
					}
					continue;
				}
				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){
					break;
				}
				zIn++;
			}
			if( zIn > zName ){
				rc = GenStateArrowAddCapture(pGen,pFunc,zName,
					(sxu32)(zIn - zName),aShadow,nShadow);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			continue;
		}
		zIn++;
	}
	return SXRET_OK;
}
/*
 * Scan the body token range of an arrow function for free-variable
 * references and record them in pFunc's closure environment. Handles:
 *   - plain $<id> pairs
 *   - variables inside "..." and heredocs (via interpolation scan)
 *   - nested arrow functions: descends into the inner body with the inner
 *     parameters added to the shadow list, so a variable referenced by a
 *     nested arrow that is not the inner's parameter is captured by the
 *     OUTER (enabling transitive capture), while the inner's own params
 *     are never mistakenly captured.
 */
static sxi32 GenStateArrowCaptureScan(
	ph7_gen_state *pGen,
	ph7_vm_func *pFunc,
	SyToken *pStart,
	SyToken *pEnd,
	SyString *aShadow,
	sxu32 nShadow)
{
	SyToken *pScan = pStart;
	sxi32 rc;
	while( pScan < pEnd ){
		if( pScan->nType & (PH7_TK_DSTR|PH7_TK_HEREDOC) ){
			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,
				pScan->sData.zString,
				pScan->sData.zString + pScan->sData.nByte,
				aShadow,nShadow);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			pScan++;
			continue;
		}
		if( pScan->nType & PH7_TK_KEYWORD ){
			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);
			SyToken *pFnKw = pScan;
			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd
				&& (pScan[1].nType & PH7_TK_KEYWORD)
				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){
				pFnKw = &pScan[1];
				nKw = PH7_TKWRD_FN;
			}
			if( nKw == PH7_TKWRD_FN ){
				SyToken *pInnerSigStart;
				SyToken *pInnerSigEnd;
				SyToken *pInnerBodyEnd;
				SyString *aInnerShadow;
				sxu32 nInnerShadow;
				sxu32 nInnerParamMax;
				SyToken *p;
				int iNestInner;
				pScan = pFnKw + 1; /* past 'fn' */
				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){
					pScan++;
				}
				if( pScan >= pEnd || (pScan->nType & PH7_TK_LPAREN) == 0 ){
					pScan++;
					continue;
				}
				pInnerSigStart = ++pScan; /* past '(' */
				PH7_DelimitNestedTokens(pScan,pEnd,
					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);
				if( pInnerSigEnd >= pEnd ){
					pScan = pEnd;
					continue;
				}
				/* Build an augmented shadow list: inherited + inner params */
				nInnerParamMax = 0;
				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){
					if( p->nType & PH7_TK_DOLLAR ){
						nInnerParamMax++;
					}
				}
				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(
					&pGen->pVm->sAllocator,
					sizeof(SyString) * (nShadow + nInnerParamMax + 1));
				if( aInnerShadow == 0 ){
					return SXERR_ABORT;
				}
				nInnerShadow = 0;
				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){
					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];
				}
				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){
					if( (p->nType & PH7_TK_DOLLAR) == 0 ){
						continue;
					}
					if( &p[1] >= pInnerSigEnd ){
						break;
					}
					if( (p[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
						continue;
					}
					aInnerShadow[nInnerShadow++] = p[1].sData;
				}
				pScan = &pInnerSigEnd[1]; /* past ')' */
				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){
					pScan++;
					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)
						&& pScan->sData.nByte == 1
						&& pScan->sData.zString[0] == '?' ){
						pScan++;
					}
					if( pScan < pEnd
						&& (pScan->nType & (PH7_TK_KEYWORD|PH7_TK_ID)) ){
						pScan++;
					}
				}
				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){
					pScan++; /* past '=>' */
				}
				pInnerBodyEnd = pScan;
				iNestInner = 0;
				while( pInnerBodyEnd < pEnd ){
					if( iNestInner == 0 && (pInnerBodyEnd->nType &
						(PH7_TK_COMMA|PH7_TK_SEMI|PH7_TK_RPAREN
						 |PH7_TK_CSB|PH7_TK_CCB)) ){
						break;
					}
					if( pInnerBodyEnd->nType &
						(PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
						iNestInner++;
					}else if( pInnerBodyEnd->nType &
						(PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
						iNestInner--;
					}
					pInnerBodyEnd++;
				}
				/* Scan the inner arrow's default-parameter VALUES as part of
				 * the outer's body: a default value is evaluated at call time
				 * in the outer frame, so any free variable it references is
				 * an outer capture. We must NOT scan the parameter-name
				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)
				 * or those names leak into the outer's closure environment.
				 *
				 * Walk the signature argument-by-argument, splitting on
				 * top-level commas, and for each argument scan only the token
				 * range after the '=' sign. */
				{
					SyToken *pArgStart = pInnerSigStart;
					while( pArgStart < pInnerSigEnd ){
						SyToken *pArgEnd = pArgStart;
						SyToken *pEq = 0;
						int iNestArg = 0;
						while( pArgEnd < pInnerSigEnd ){
							if( iNestArg == 0
								&& (pArgEnd->nType & PH7_TK_COMMA) ){
								break;
							}
							if( pArgEnd->nType &
								(PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
								iNestArg++;
							}else if( pArgEnd->nType &
								(PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
								iNestArg--;
							}
							if( pEq == 0 && iNestArg == 0
								&& (pArgEnd->nType & PH7_TK_EQUAL) ){
								pEq = pArgEnd;
							}
							pArgEnd++;
						}
						if( pEq && (pEq + 1) < pArgEnd ){
							rc = GenStateArrowCaptureScan(pGen,pFunc,
								pEq + 1,pArgEnd,aShadow,nShadow);
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
						}
						pArgStart = pArgEnd;
						if( pArgStart < pInnerSigEnd
							&& (pArgStart->nType & PH7_TK_COMMA) ){
							pArgStart++;
						}
					}
				}
				rc = GenStateArrowCaptureScan(pGen,pFunc,
					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				pScan = pInnerBodyEnd;
				continue;
			}
		}
		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){
			pScan++;
			continue;
		}
		{
			/* Walk past variable-variable chains ($$x) to the base name. */
			SyToken *pDollar = pScan;
			while( &pDollar[1] < pEnd
				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){
				pDollar++;
			}
			if( &pDollar[1] >= pEnd ){
				break;
			}
			if( (pDollar[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
				pScan = pDollar + 1;
				continue;
			}
			rc = GenStateArrowAddCapture(pGen,pFunc,
				pDollar[1].sData.zString,pDollar[1].sData.nByte,
				aShadow,nShadow);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			pScan = pDollar + 2;
		}
	}
	return SXRET_OK;
}
/*
 * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr
 * Arrow functions are always closures that auto-capture enclosing-scope
 * variables by value. The body is a single expression that acts as an
 * implicit return. Unless prefixed with 'static', the enclosing object's
 * $this is also made available.
 */
PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	ph7_vm_func *pFunc;
	ph7_vm_func_closure_env sEnv;
	GenBlock *pBlock;
	SySet *pInstrContainer;
	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */
	SyToken *pBodyStart;   /* First token after '=>' */
	SyToken *pBodyEnd;     /* Token just past the last body token */
	SyToken *pSavedEnd;
	ph7_vm_func_arg *aArgs;
	char zName[512];
	static int iCnt = 1;
	char *zDup;
	SyToken *pTokKw;
	sxu32 nLen;
	sxu32 nLine;
	sxi32 iFlags = 0;
	int bStatic = 0;
	sxi32 rc;
	sxu32 n;
	SXUNUSED(iCompileFlag); /* cc warning */

	nLine = pGen->pIn->nLine;
	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */
	pTokKw = pGen->pIn;
	/* Optional 'static' prefix */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)
		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){
		bStatic = 1;
		iFlags |= VM_FUNC_STATIC_CL;
		pGen->pIn++;
	}
	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0
		|| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Arrow function: expected 'fn' keyword");
		return SXERR_SYNTAX;
	}
	pGen->pIn++; /* Jump 'fn' */
	/* Optional '&' — return by reference */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){
		iFlags |= VM_FUNC_REF_RETURN;
		pGen->pIn++;
	}
	/* Expect '(' */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		if( pGen->pIn < pGen->pEnd ){
			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,
				"syntax error, unexpected %s \"%z\", expecting \"(\"",
				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);
		}else{
			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,
				"syntax error, unexpected end of file, expecting \"(\"");
		}
		return SXERR_SYNTAX;
	}
	pGen->pIn++; /* Jump '(' */
	/* Delimit the parameter list */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);
	if( pSigEnd >= pGen->pEnd ){
		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,
			"syntax error, unexpected end of file, expecting \")\"");
		return SXERR_SYNTAX;
	}
	/* Allocate the function state */
	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));
	if( pFunc == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	/* Generate a unique lambda name */
	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){
		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	}
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);
	if( zDup == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);
	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */
	pFunc->nLine = nLine;
	/* Expression-position attributes (`$f = #[A] fn () => …`) */
	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Collect function arguments */
	if( pGen->pIn < pSigEnd ){
		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Point past ')' and parse optional return type */
	pGen->pIn = &pSigEnd[1];
	rc = GenStateParseReturnType(pGen,pFunc);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}else if( rc == SXERR_SYNTAX ){
		return SXERR_SYNTAX;
	}
	/* Expect '=>' */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){
		if( pGen->pIn < pGen->pEnd ){
			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,
				"syntax error, unexpected %s \"%z\", expecting \"=>\"",
				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);
		}else{
			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,
				"syntax error, unexpected end of file, expecting \"=>\"");
		}
		return SXERR_SYNTAX;
	}
	pGen->pIn++; /* Jump '=>' */
	pBodyStart = pGen->pIn;
	pBodyEnd = pGen->pEnd;
	/* Build the initial shadow list from the arrow's own parameters, then
	 * recursively collect free-variable references from the body. The scan
	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow
	 * functions with proper parameter shadowing for transitive capture. */
	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);
	{
		SyString *aShadow = 0;
		sxu32 nShadow = SySetUsed(&pFunc->aArgs);
		if( nShadow > 0 ){
			aShadow = (SyString *)SyMemBackendPoolAlloc(
				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);
			if( aShadow == 0 ){
				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
					"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			for( n = 0 ; n < nShadow ; n++ ){
				aShadow[n] = aArgs[n].sName;
			}
		}
		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,
			aShadow,nShadow);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Unless declared static, auto-capture $this so arrow functions used
	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the
	 * captured value is silently dropped when the enclosing scope has no
	 * $this. */
	if( !bStatic ){
		char *zThisDup;
		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);
		if( zThisDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
				"Fatal, PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
		sEnv.iFlags = VM_FUNC_ARG_IGNORE;
		sEnv.nIdx = SXU32_HIGH;
		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);
		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);
	}
	/* Arrow functions are always closures */
	pFunc->iFlags |= VM_FUNC_CLOSURE;
	/* Compile the body expression as an implicit return */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED|GEN_BLOCK_FUNC,
		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"PH7 engine is running out-of-memory");
		return SXERR_ABORT;
	}
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);
	pSavedEnd = pGen->pEnd;
	pGen->pIn = pBodyStart;
	pGen->pEnd = pBodyEnd;
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* The cursor stopped just past the body expression */
	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;
	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.
	 * Any throw-expression inside the body needs a valid jump target and a
	 * stack-balanced exit path — point its fixup at a separate OP_DONE with
	 * p1=0 emitted below, which does not pop the (absent) return value. */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	GenStateLeaveBlock(&(*pGen),0);
	/* Restore cursors; caller will re-synchronize via the node's pEnd */
	pGen->pIn = pBodyEnd;
	pGen->pEnd = pSavedEnd;
	/* Emit the load-closure instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);
	return SXRET_OK;
}
/*
 * Compile a single arm's expression range into a freshly-allocated
 * sub-bytecode container. The caller supplies the token range [pStart, pEnd).
 * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the
 * expression's value.
 */
static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,
	SyToken *pStart,SyToken *pStop,SySet *pOut)
{
	SySet *pInstrContainer;
	SyToken *pTmpIn,*pTmpEnd;
	GenBlock *pArmBlock;
	sxi32 rc;
	pTmpIn  = pGen->pIn;
	pTmpEnd = pGen->pEnd;
	pGen->pIn  = pStart;
	pGen->pEnd = pStop;
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);
	/* Enter a local FUNC block so any throw-expression fixups register on it
	 * (and not on an outer try/catch whose instruction indices live in a
	 * different bytecode container). We resolve those fixups to a trailing
	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates
	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED|GEN_BLOCK_FUNC,
		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);
	if( rc != SXRET_OK ){
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
		pGen->pIn  = pTmpIn;
		pGen->pEnd = pTmpEnd;
		return SXERR_ABORT;
	}
	rc = PH7_CompileExpr(&(*pGen),0,0);
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
	GenStateLeaveBlock(&(*pGen),0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	pGen->pIn  = pTmpIn;
	pGen->pEnd = pTmpEnd;
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( rc == SXERR_EMPTY ){
		return SXERR_EMPTY;
	}
	return SXRET_OK;
}
/*
 * Compile a PHP 8.0 match expression:
 *     match(subject){ cond_list => result, ..., default => result }
 * Match is an expression — on exit the match result is on top of the stack.
 * Strict comparison (===) is used between the subject and each condition.
 * No fallthrough. If no arm matches and no default is present, a fatal
 * Uncaught UnhandledMatchError is raised at runtime.
 */
/*
 * Emit a parse error for match and propagate SXERR_ABORT if the error
 * count limit has been reached. Otherwise returns SXERR_SYNTAX so the
 * caller can bail out of the current expression.
 */
static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)
{
	va_list ap;
	sxi32 rc;
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);
	va_start(ap,zFmt);
	SyBlobFormatAp(&sMsg,zFmt,ap);
	va_end(ap);
	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */
	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));
	SyBlobRelease(&sMsg);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXERR_SYNTAX;
}
/*
 * Scan a top-level token range inside a match body, stopping at the first
 * token whose type is in stopMask (not counting nested parens/brackets/braces).
 * Returns the stop token pointer (or pEnd if none found).
 */
static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)
{
	SyToken *pCur = pStart;
	int iNest = 0;
	while( pCur < pEnd ){
		if( pCur->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
			iNest++;
		}else if( pCur->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
			iNest--;
		}else if( iNest == 0 && (pCur->nType & stopMask) ){
			return pCur;
		}
		pCur++;
	}
	return pEnd;
}
PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	ph7_match *pMatch;
	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;
	int bHasDefault = 0;
	sxu32 nLine;
	sxi32 rc;
	SXUNUSED(iCompileFlag);
	nLine = pGen->pIn->nLine;
	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */
	/* Expect '(' */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		return GenStateMatchError(pGen,nLine,
			"syntax error, unexpected %s, expecting \"(\"",
			pGen->pIn < pGen->pEnd ? "token" : "end of file");
	}
	pGen->pIn++; /* Jump '(' */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);
	if( pSubjEnd >= pGen->pEnd ){
		return GenStateMatchError(pGen,nLine,
			"syntax error, unexpected end of file, expecting \")\"");
	}
	if( pGen->pIn >= pSubjEnd ){
		return GenStateMatchError(pGen,nLine,
			"syntax error, unexpected \")\", expecting match subject");
	}
	/* Compile subject inline — result stays on the caller's operand stack */
	pSavedEnd = pGen->pEnd;
	pGen->pEnd = pSubjEnd;
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	pGen->pEnd = pSavedEnd;
	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */
	/* Expect '{' */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_OCB) == 0 ){
		return GenStateMatchError(pGen,
			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,
			"syntax error, expecting \"{\" after match subject");
	}
	pGen->pIn++; /* Jump '{' */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);
	if( pBodyEnd >= pGen->pEnd ){
		return GenStateMatchError(pGen,nLine,
			"syntax error, unexpected end of file, expecting \"}\"");
	}
	/* Allocate ph7_match container */
	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));
	if( pMatch == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	SyZero(pMatch,sizeof(ph7_match));
	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));
	/* Iterate arms */
	while( pGen->pIn < pBodyEnd ){
		ph7_match_arm sArm;
		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;
		sxu32 nArmLine = pGen->pIn->nLine;
		SyZero(&sArm,sizeof(ph7_match_arm));
		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));
		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));
		/* 'default' arm? */
		if( (pGen->pIn->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){
			if( bHasDefault ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,
					"Match expressions may only contain one default arm");
				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;
			}
			sArm.bDefault = 1;
			bHasDefault = 1;
			pGen->pIn++;
			if( pGen->pIn >= pBodyEnd || (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){
				return GenStateMatchError(pGen,nArmLine,
					"syntax error, expecting \"=>\" after 'default'");
			}
			pGen->pIn++; /* Jump '=>' */
		}else{
			/* Condition list: cond (',' cond)* '=>' */
			pCondStart = pGen->pIn;
			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,
				PH7_TK_ARRAY_OP|PH7_TK_COMMA);
			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){
				SySet sCondBc;
				if( pCondStart >= pArrow ){
					return GenStateMatchError(pGen,nArmLine,
						"syntax error, empty match condition expression");
				}
				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));
				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				SySetPut(&sArm.aConds,(const void *)&sCondBc);
				pCondStart = &pArrow[1]; /* Skip ',' */
				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,
					PH7_TK_ARRAY_OP|PH7_TK_COMMA);
			}
			if( pArrow >= pBodyEnd || (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){
				return GenStateMatchError(pGen,nArmLine,
					"syntax error, expecting \"=>\" in match arm");
			}
			if( pCondStart >= pArrow ){
				return GenStateMatchError(pGen,nArmLine,
					"syntax error, empty match condition expression");
			}
			{
				SySet sCondBc;
				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));
				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				SySetPut(&sArm.aConds,(const void *)&sCondBc);
			}
			pGen->pIn = &pArrow[1]; /* Jump '=>' */
		}
		/* Compile result expression: up to top-level ',' or body end */
		pResStart = pGen->pIn;
		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);
		if( pResStart >= pResEnd ){
			return GenStateMatchError(pGen,nArmLine,
				"syntax error, expected expression after \"=>\"");
		}
		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn = pResEnd;
		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){
			pGen->pIn++; /* Skip trailing ',' */
		}
		SySetPut(&pMatch->aArms,(const void *)&sArm);
	}
	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);
	return SXRET_OK;
}
/*
 * Compile a backtick quoted string.
 */
static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SXUNUSED(iCompileFlag);
	/*
	 * The backtick (`) operator was DEPRECATED by php (shell_exec() is the replacement).
	 * PHL targets php's *non-deprecated* surface, so it is a hard parse error — never
	 * compiled to a shell_exec() call.
	 */
	PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,
		"syntax error, the backtick (`) operator was removed, use shell_exec() instead");
	return SXERR_ABORT;
}
/*
 * Compile a function [i.e: die(),exit(),include(),...] which is a langauge
 * construct.
 */
PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString *pName;
	sxu32 nKeyID;
	sxi32 rc;
	/* Name of the language construct [i.e: echo,die...]*/
	pName = &pGen->pIn->sData;
	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
	pGen->pIn++; /* Jump the language construct keyword */
	if( nKeyID == PH7_TKWRD_ECHO ){
		SyToken *pTmp,*pNext = 0;
		/* Compile arguments one after one */
		pTmp = pGen->pEnd;
		/* Symisc eXtension to the PHP programming language:
		 * 'echo' can be used in the context of a function which
		 *  mean that the following expression is valid:
		 *      fopen('file.txt','r') or echo "IO error";
		 */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);
		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){
			if( pGen->pIn < pNext ){
				pGen->pEnd = pNext;
				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				if( rc != SXERR_EMPTY ){
					/* Ticket 1433-008: Optimization #1: Consume input directly
					 * without the overhead of a function call.
					 * This is a very powerful optimization that improve
					 * performance greatly.
					 */
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);
				}
			}
			/* Jump trailing commas */
			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){
				pNext++;
			}
			pGen->pIn = pNext;
		}
		/* Restore token stream */
		pGen->pEnd = pTmp;
	}else{
		sxi32 nArg = 0;
		sxu32 nIdx = 0;
		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if(rc != SXERR_EMPTY ){
			nArg = 1;
		}
		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){
			ph7_value *pObj;
			/* Emit the call instruction */
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");
				SXUNUSED(iCompileFlag); /* cc warning */
				return SXERR_ABORT;
			}
			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);
			/* Install in the literal table */
			GenStateInstallLiteral(&(*pGen),pObj,nIdx);
		}
		/* Emit the call instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a node holding a variable declaration.
 * According to the PHP language reference
 *  Variables in PHP are represented by a dollar sign followed by the name of the variable.
 *  The variable name is case-sensitive.
 *  Variable names follow the same rules as other labels in PHP. A valid variable name starts
 *  with a letter or underscore, followed by any number of letters, numbers, or underscores.
 *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'
 *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).
 *  Note: $this is a special variable that can't be assigned.
 *  By default, variables are always assigned by value. That is to say, when you assign an expression
 *  to a variable, the entire value of the original expression is copied into the destination variable.
 *  This means, for instance, that after assigning one variable's value to another, changing one of those
 *  variables will have no effect on the other. For more information on this kind of assignment, see
 *  the chapter on Expressions.
 *  PHP also offers another way to assign values to variables: assign by reference. This means that
 *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original
 *  variable. Changes to the new variable affect the original, and vice versa.
 *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which
 *  is being assigned (the source variable).
 */
PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxu32 nLineLocal = pGen->pIn->nLine;
	sxi32 iVv;
	sxi32 iP1;
	void *p3;
	sxi32 rc;
	iVv = -1; /* Variable variable counter */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){
		pGen->pIn++;
		iVv++;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD|PH7_TK_OCB/*'{'*/)) == 0 ){
		/* Invalid variable name */
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	p3  = 0;
	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){
		/* Dynamic variable creation */
		pGen->pIn++;  /* Jump the open curly */
		pGen->pEnd--; /* Ignore the trailing curly */
		if( pGen->pIn >= pGen->pEnd ){
			/* Empty expression */
			{
			/* php names the offending token and, for an empty "${}", stops there:
			 * the "expecting" tail only appears when something could still follow. */
			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;
			PH7_GenSyntaxError(&(*pGen),pBad,
				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");
			}
			return SXRET_OK;
		}
		/* Compile the expression holding the variable name */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc == SXERR_EMPTY ){
			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);
			return SXRET_OK;
		}
	}else{
		SyHashEntry *pEntry;
		SyString *pName;
		char *zName = 0;
		/* Extract variable name */
		pName = &pGen->pIn->sData;
		/* Advance the stream cursor */
		pGen->pIn++;
		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);
		if( pEntry == 0 ){
			/* Duplicate name */
			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
			if( zName == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			/* Install in the hashtable */
			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);
		}else{
			/* Name already available */
			zName = (char *)pEntry->pUserData;
		}
		p3 = (void *)zName;
	}
	iP1 = 0;
	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){
		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){
			/* Read-only load.In other words do not create the variable if inexistant */
			iP1 = 1;
		}
	}
	/* Emit the load instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);
	while( iVv > 0 ){
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);
		iVv--;
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Load a literal.
 */
static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)
{
	SyToken *pToken = pGen->pIn;
	ph7_value *pObj;
	SyString *pStr;
	sxu32 nIdx;
	/* Extract token value */
	pStr = &pToken->sData;
	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved
	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain
	 * identifier — the parser flagged its token so the whole value-folding chain is
	 * skipped and it falls through to the ordinary string-literal emit below. */
	if( pToken->nType & PH7_TK_MEMBER_NAME ){
		/* fall through to the plain-string literal path */
	}else if( pStr->nByte == sizeof("NULL") - 1 ){
		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){
			/* NULL constant are always indexed at 0 */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
			return SXRET_OK;
		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){
			/* TRUE constant are always indexed at 1 */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);
			return SXRET_OK;
		}
	}else if (pStr->nByte == sizeof("FALSE") - 1 &&
		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){
			/* FALSE constant are always indexed at 2 */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);
			return SXRET_OK;
	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&
		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){
			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);
			/* Emit the load constant instruction */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
			return SXRET_OK;
	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&
		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) ||
		(pStr->nByte == sizeof("__DIR__") - 1 &&
		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){
			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the
			 * file being compiled (where the token is written), NOT the runtime
			 * execution file. A function defined in a.php reporting __FILE__ must say
			 * a.php even when called from b.php — php semantics, and what Composer's
			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)
			 * relies on. The runtime-constant path returned the caller's file. */
			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */
			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			if( pFile && pFile->nByte > 0 ){
				if( bDir ){
					const char *zDir;
					int nLen;
					SyString sDir;
					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);
					SyStringInitFromBuf(&sDir,zDir,nLen);
					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);
				}else{
					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);
				}
			}else{
				SyString sMem;
				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);
				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);
			}
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
			return SXRET_OK;
	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&
		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){
			/* __NAMESPACE__ magic constant: resolved at compile time */
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			if( SyBlobLength(&pGen->sNamespace) > 0 ){
				SyString sNs;
				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);
			}else{
				PH7_MemObjInitFromString(pGen->pVm,pObj,0);
			}
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
			return SXRET_OK;
	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&
		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) ||
		(pStr->nByte == sizeof("__METHOD__") - 1 &&
		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){
			GenBlock *pBlock = pGen->pCurrent;
			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */
			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){
				/* Point to the upper block */
				pBlock = pBlock->pParent;
			}
			if( pBlock == 0 ){
				/* Called in the global scope,load NULL */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
			}else{
				/* Extract the target function/method */
				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;
				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */
				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
				if( pObj == 0 ){
					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
					return SXERR_ABORT;
				}
				/*
				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain
				 * function name inside a plain function (php does not answer "" there —
				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and
				 * unqualified in every method).
				 */
				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){
					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;
					SyBlob sQual;
					SyString sOut;
					SyBlobInit(&sQual,&pGen->pVm->sAllocator);
					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);
					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));
					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);
					SyBlobRelease(&sQual);
				}else{
					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);
				}
				/* Emit the load constant instruction */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
			}
			return SXRET_OK;
	}
	/* Query literal table */
	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){
		ph7_value *pLitObj;
		/* Unknown literal,install it in the literal table */
		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
		if( pLitObj == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);
		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);
	}
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);
	return SXRET_OK;
}
/*
 * Resolve a namespace path or simply load a literal.
 * If the token stream contains namespace separators (backslashes),
 * assemble them into a single literal string (e.g. "Foo\Bar\Baz").
 * Otherwise, load the simple literal directly.
 */
static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)
{
	sxi32 rc;
	if( pGen->pIn >= pGen->pEnd ){
		return SXRET_OK;
	}
	/* Check if this is a multi-token namespace path */
	if( pGen->pIn < &pGen->pEnd[-1] ){
		/* Multiple tokens: assemble the full path into sWorker */
		SyBlob *pWorker = &pGen->sWorker;
		int isAbsolute = 0;
		SyBlobReset(pWorker);
		/* Check for leading backslash (absolute path) */
		if( pGen->pIn->nType & PH7_TK_NSSEP ){
			isAbsolute = 1;
			pGen->pIn++; /* Skip leading backslash */
		}
		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved
		 * against use-imports below — php resolves `A\B\C` by mapping the leading
		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends
		 * the current namespace when `A` matches no import. Blindly prefixing the
		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */
		{
			SyBlob sRaw;
			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);
			while( pGen->pIn <= &pGen->pEnd[-1] ){
				if( pGen->pIn->nType & PH7_TK_NSSEP ){
					SyBlobAppend(&sRaw,"\\",1);
				}else{
					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);
				}
				if( pGen->pIn == &pGen->pEnd[-1] ){
					pGen->pIn++;
					break;
				}
				pGen->pIn++;
			}
			if( isAbsolute ){
				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */
			}else{
				const char *zRaw = (const char *)SyBlobData(&sRaw);
				sxu32 nRaw = SyBlobLength(&sRaw);
				sxu32 nFirst = 0;
				SyHashEntry *pNsImp;
				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }
				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);
				if( pNsImp ){
					/* Leading segment is an imported alias: substitute its FQN. */
					const char *zFQN = (const char *)pNsImp->pUserData;
					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));
					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);
				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){
					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
					SyBlobAppend(pWorker,"\\",1);
					SyBlobAppend(pWorker,zRaw,nRaw);
				}else{
					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */
				}
			}
			SyBlobRelease(&sRaw);
		}
		if( SyBlobLength(pWorker) > 0 ){
			ph7_value *pObj;
			SyString sPath;
			sxu32 nIdx;
			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));
			/* Install in the literal table */
			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){
				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
				if( pObj == 0 ){
					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
					return SXERR_ABORT;
				}
				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);
				GenStateInstallLiteral(&(*pGen),pObj,nIdx);
			}
			/* Emit the load constant instruction.
			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.
			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,
				isAbsolute ? (PH7_LOADC_EXPAND|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,
				nIdx,0,0);
			return SXRET_OK;
		}
	}
	/* Single-token literal: load directly */
	rc = GenStateLoadLiteral(&(*pGen));
	return rc;
}
/*
 * Compile a literal which is an identifier(name) for a simple value.
 */
/*
 * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of
 * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument
 * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...`
 * appeared outside a call argument list — a syntax error (PHP rejects it likewise).
 */
PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SXUNUSED(iCompileFlag);
	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,
		"Cannot use the first-class callable syntax '...' here");
	return SXERR_SYNTAX;
}
PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxi32 rc;
	rc = GenStateResolveNamespaceLiteral(&(*pGen));
	if( rc != SXRET_OK ){
		SXUNUSED(iCompileFlag); /* cc warning */
		return rc;
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
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
				if( pCallName->nByte == 5
				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){
					iFlags |= EXPR_FLAG_LOAD_IDX_ISSET;
				}else if( pCallName->nByte == 5
				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){
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
				iLeftFlags |= EXPR_FLAG_LOAD_IDX_STORE | EXPR_FLAG_MEMBER_WRITE;
			}
			/* `??` reads its LEFT operand in isset-context: an undefined or
			 * UNINITIALIZED typed PROPERTY must yield the default rather than a
			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes
			 * the silent-lookup path (iP2 = ISSET), which still loads a present
			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means
			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —
			 * that path is already handled correctly by OP_NULLC. */
			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC
				&& pNode->pLeft && pNode->pLeft->pOp
				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW
					|| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW
					|| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){
				iLeftFlags |= EXPR_FLAG_LOAD_IDX_ISSET;
			}
			if( iVmOp == PH7_OP_ERR_CTRL ){
				/* '@' must suppress the diagnostics raised WHILE its operand runs, so
				 * open the window here; the trailing emit below closes it (iP1 = 0). */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);
			}
			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);
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
				|EXPR_FLAG_LOAD_IDX_EMPTY|EXPR_FLAG_MEMBER_WRITE);
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
		}
		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);
		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);
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
		rc = PH7_CompileExpr(pGen,0,0);
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
		/* No available error consumer,return immediately */
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
