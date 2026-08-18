/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.
 * That is, routines defined in this file takes a stream of tokens and output
 * PH7 bytecode instructions.
 */
/* Forward declaration */
typedef struct LangConstruct LangConstruct;
typedef struct JumpFixup     JumpFixup;
typedef struct Label         Label;
/* Block [i.e: set of statements] control flags */
#define GEN_BLOCK_LOOP        0x001    /* Loop block [i.e: for,while,...] */
#define GEN_BLOCK_PROTECTED   0x002    /* Protected block */
#define GEN_BLOCK_COND        0x004    /* Conditional block [i.e: if(condition){} ]*/
#define GEN_BLOCK_FUNC        0x008    /* Function body */
#define GEN_BLOCK_GLOBAL      0x010    /* Global block (always set)*/
#define GEN_BLOC_NESTED_FUNC  0x020    /* Nested function body */
#define GEN_BLOCK_EXPR        0x040    /* Expression */
#define GEN_BLOCK_STD         0x080    /* Standard block */
#define GEN_BLOCK_EXCEPTION   0x100    /* Exception block [i.e: try{ } }*/
#define GEN_BLOCK_SWITCH      0x200    /* Switch statement */
/*
 * Each label seen in the input is recorded in an instance
 * of the following structure.
 * A label is a target point [i.e: a jump destination] that is specified
 * by an identifier followed by a colon.
 * Example
 *  LABEL:
 *		echo "hello\n";
 */
struct Label
{
	ph7_vm_func *pFunc;  /* Compiled function where the label was declared.NULL otherwise */
	sxu32 nJumpDest;     /* Jump destination */
	SyString sName;      /* Label name */
	sxu32 nLine;         /* Line number this label occurs */
	sxu8 bRef;           /* True if the label was referenced */
};
/*
 * Compilation of some PHP constructs such as if, for, while, the logical or
 * (||) and logical and (&&) operators in expressions requires the
 * generation of forward jumps.
 * Since the destination PC target of these jumps isn't known when the jumps
 * are emitted, we record each forward jump in an instance of the following
 * structure. Those jumps are fixed later when the jump destination is resolved.
 */
struct JumpFixup
{
	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */
	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */
	/* The following fields are only used by the goto statement */
	SyString sLabel;    /* Label name */
	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */
	sxu32 nLine;        /* Track line number */
};
/*
 * Each language construct is represented by an instance
 * of the following structure.
 */
struct LangConstruct
{
	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */
	ProcLangConstruct xConstruct;  /* C function implementing the language construct */
};
/* Compilation flags */
#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */
/* Token stream synchronization macros */
#define SWAP_TOKEN_STREAM(GEN,START,END)\
	pTmp  = GEN->pEnd;\
	pGen->pIn  = START;\
	pGen->pEnd = END
#define UPDATE_TOKEN_STREAM(GEN)\
	if( GEN->pIn < pTmp ){\
	    GEN->pIn++;\
	}\
	GEN->pEnd = pTmp
#define SWAP_DELIMITER(GEN,START,END)\
	pTmpIn  = GEN->pIn;\
	pTmpEnd = GEN->pEnd;\
	GEN->pIn = START;\
	GEN->pEnd = END
#define RE_SWAP_DELIMITER(GEN)\
	GEN->pIn  = pTmpIn;\
	GEN->pEnd = pTmpEnd
/* Flags related to expression compilation */
#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */
#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */
#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */
#define EXPR_FLAG_LOAD_IDX_ISSET    0x008 /* LOAD_IDX argument is the LHS of isset() — emit iP2=4 (offsetExists) */
#define EXPR_FLAG_LOAD_IDX_UNSET    0x010 /* LOAD_IDX argument is the LHS of unset() — emit iP2=5 (offsetUnset) */
#define EXPR_FLAG_LOAD_IDX_EMPTY    0x020 /* LOAD_IDX argument is the LHS of empty() — emit iP2=6 (offsetExists+offsetGet) */
#define EXPR_FLAG_MEMBER_WRITE      0x040 /* Sub-tree is the write lvalue of an assignment: tag a target
                                           * OP_MEMBER iP2=PH7_MEMBER_WRITE so the VM auto-creates a missing
                                           * property (e.g. `$o->arr[$k] = v`, `$o->p ??= v`). Propagated
                                           * from the precedence-18 lvalue through SUBSCRIPT to the base
                                           * member; stripped when descending into an intermediate `->`
                                           * container (the container is read, not the write target). */
/* Forward declaration */
static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));
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
static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)
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
static sxi32 GenStateEnterBlock(
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
static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)
{
	GenBlock *pBlock = pGen->pCurrent;
	if( pBlock == 0 ){
		/* No more block to pop */
		return SXERR_EMPTY;
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
 * Emit a forward jump.
 * Notes on forward jumps
 *  Compilation of some PHP constructs such as if,for,while and the logical or
 *  (||) and logical and (&&) operators in expressions requires the
 *  generation of forward jumps.
 *  Since the destination PC target of these jumps isn't known when the jumps
 *  are emitted, we record each forward jump in an instance of the following
 *  structure. Those jumps are fixed later when the jump destination is resolved.
 */
static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)
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
static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)
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
static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)
{
	JumpFixup *pJump,*aJumps;
	Label *pLabel,*aLabel;
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
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			continue;
		}
		/* Make sure the target label is reachable */
		if( pLabel->pFunc != pJump->pFunc ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);
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
	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);
	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){
		if( aLabel[n].bRef == FALSE ){
			/* Emit a warning */
			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,
				"Label '%z' is defined but not referenced",&aLabel[n].sName);
		}
	}
	return SXRET_OK;
}
/*
 * Check if a given token value is installed in the literal table.
 */
static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)
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
static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)
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
static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)
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
static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);
static void GenStateSetPendingDoc(ph7_gen_state *pGen);
static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut);
static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);
static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut);
static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut);
static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);
static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);
/* Forward decl: union type parser is defined later in this file. */
static sxi32 GenStateParseUnionTypeDecl(
	ph7_gen_state *pGen,
	sxu32 *pnType,
	SyString *pClass,
	SySet *pAlts,
	sxi32 *piTypeFlags,
	SyString *pTypeText,
	int iNullableFlag,
	int iUnionFlag,
	int bAllowVoid,
	sxu32 nLine
);
static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);
static const char * TokenTypeName(sxu32 nType);
/*
 * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical
 * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble
 * separators is ~80 chars) fits comfortably, so the fast path never touches
 * the heap. The language itself imposes no upper bound on the length of a
 * well-formed literal — the stripper falls back to a VM-allocator buffer
 * for anything larger, so correctness is preserved even for pathological
 * inputs like a thousand-digit number.
 */
#define GEN_NUM_SCRATCH 128
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
static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)
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
static sxi32 GenStateStripNumericSeparators(
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
	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){
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
	if( pStr->nByte < 1024 ){
		/* Install in the literal table */
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
		/* Empty string,load NULL */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
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
static sxi32 GenStateCompileArrayEntry(
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
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
				"array(): Expecting a variable/array member/function call after reference operator '&'");
			if( rc != SXERR_ABORT ){
				rc = SXERR_INVALID;
			}
		}
	}else if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
			"array(): Expecting a variable after reference operator '&'");
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
static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)
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
		/* Jump leading commas */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){
			pGen->pIn++;
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
			if( &pCur[1] >= pGen->pIn ){
				/* Missing value */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");
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
		}else if( pKey == pCur ){
			/* Key is omitted,emit a warning */
			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");
			pCur++; /* Jump the '=>' operator */
		}else{
			/* Reset back the cursor and point to the entry value */
			pCur = pKey;
		}
		if( rc == SXERR_EMPTY ){
			/* No available key,load NULL */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);
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
		/* Compile indice value */
		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);
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
					"list(): Expecting a variable not an expression");
				if( rc != SXERR_ABORT ){
					rc = SXERR_INVALID;
				}
		}
	}else if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
			"list(): Expecting a variable not an expression");
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
static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);
static int GenStateIsReservedConstant(SyString *pName);
static int GenStateIsReadonly(SyToken *pTok);
static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);
static sxi32 GenStateSetVisFlag(sxi32 nKw);
static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,
	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);
static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);
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
	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.
	 * If you want this feature,please contact symisc systems via contact@symisc.net
	 */
	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,
		"Command line invocation is disabled in the current release of the PH7(%s) engine",
		ph7_lib_version()
		);
	/* Load NULL */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
	SXUNUSED(iCompileFlag); /* cc warning */
	/* Node successfully compiled */
	return SXRET_OK;
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
		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");
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
			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");
			return SXRET_OK;
		}
		/* Compile the expression holding the variable name */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc == SXERR_EMPTY ){
			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");
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
	/* Deal with the reserved literals [i.e: null,false,true,...] first */
	if( pStr->nByte == sizeof("NULL") - 1 ){
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
				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){
					/* Not a class method,Load null */
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
				}else{
					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
					if( pObj == 0 ){
						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
						return SXERR_ABORT;
					}
					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);
					/* Emit the load constant instruction */
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
				}
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
		/* For relative qualified names in a namespace, prepend the NS */
		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){
			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
			SyBlobAppend(pWorker,"\\",1);
		}
		/* Collect all path components */
		while( pGen->pIn <= &pGen->pEnd[-1] ){
			if( pGen->pIn->nType & PH7_TK_NSSEP ){
				SyBlobAppend(pWorker,"\\",1);
			}else{
				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);
			}
			if( pGen->pIn == &pGen->pEnd[-1] ){
				pGen->pIn++;
				break;
			}
			pGen->pIn++;
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
static int GenStateIsReservedConstant(SyString *pName)
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
 * Compile the 'const' statement.
 * According to the PHP language reference
 *  A constant is an identifier (name) for a simple value. As the name suggests, that value
 *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).
 *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.
 *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts
 *  with a letter or underscore, followed by any number of letters, numbers, or underscores.
 *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*
 *  Syntax
 *  You can define a constant by using the define()-function or by using the const keyword outside
 *  a class definition. Once a constant is defined, it can never be changed or undefined.
 *  You can get the value of a constant by simply specifying its name. Unlike with variables
 *  you should not prepend a constant with a $. You can also use the function constant() to read
 *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()
 *  to get a list of all defined constants.
 *
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the constant while the zend engine
 *  would allow only simple scalar value.
 *  Example
 *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine
 *    Refer to the official documentation for more information on this feature.
 */
static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)
{
	SySet *pConsCode,*pInstrContainer;
	sxu32 nLineLocal = pGen->pIn->nLine;
	SyString *pName;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'const' keyword */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_SSTR|PH7_TK_DSTR|PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		/* Invalid constant name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek constant name */
	pName = &pGen->pIn->sData;
	/* Make sure the constant name isn't reserved */
	if( GenStateIsReservedConstant(pName) ){
		/* Reserved constant */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++;
	if(pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){
		/* Invalid statement*/
		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++; /*Jump the equal sign */
	/* Allocate a new constant value container */
	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));
	if( pConsCode == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
	/* Swap bytecode container */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);
	/* Compile constant value */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	SySetSetUserData(pConsCode,pGen->pVm);
	/* Register the constant with namespace-qualified name */
	{
		SyBlob sFQN;
		SyString sFQNStr;
		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
		GenStateBuildFQN(pGen,pName,&sFQN);
		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));
		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,
			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);
		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){
			/* php 8.5: attributes on `const` statements — attach the pending
			 * groups to the registered constant record for Reflection. */
			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,
				SyBlobData(&sFQN),SyBlobLength(&sFQN));
			if( pCEntry ){
				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;
				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){
					SyBlobRelease(&sFQN);
					return SXERR_ABORT;
				}
			}
		}
		SyBlobRelease(&sFQN);
	}
	if( rc != SXRET_OK ){
		SySetRelease(pConsCode);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);
	}
	return SXRET_OK;
Synchronize:
	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */
	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the 'continue' statement.
 * According to the PHP language reference
 *  continue is used within looping structures to skip the rest of the current loop iteration
 *  and continue execution at the condition evaluation and then the beginning of the next
 *  iteration.
 *  Note: Note that in PHP the switch statement is considered a looping structure for
 *  the purposes of continue.
 *  continue accepts an optional numeric argument which tells it how many levels
 *  of enclosing loops it should skip to the end of.
 *  Note:
 *   continue 0; and continue 1; is the same as running continue;.
 */
/*
 * Emit PH7_OP_POP_EXCEPTION for each exception block between the current
 * block and the target loop block. This ensures finally blocks run when
 * break/continue crosses a try boundary.
 *
 * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):
 * those are compiled into separate bytecode containers executed via VmLocalExec,
 * so we must not emit POP_EXCEPTION for the parent try from inside them.
 */
static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)
{
	GenBlock *pBlock = pGen->pCurrent;
	int nInlineTry = 0;
	while( pBlock && pBlock != pTarget ){
		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){
			if( pBlock->pUserData ){
				/* A try block with an exception context. In a generator its catch/finally
				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that
				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.
				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */
				if( pGen->bInGenerator ){
					nInlineTry++;
				}else{
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);
				}
			}else{
				/* A catch/finally block compiled into a separate bytecode container
				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */
				break;
			}
		}
		pBlock = pBlock->pParent;
	}
	return nInlineTry;
}
static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)
{
	GenBlock *pLoop; /* Target loop */
	sxi32 iLevel;    /* How many nesting loop to skip */
	sxu32 nLineLocal;
	sxi32 rc;
	nLineLocal = pGen->pIn->nLine;
	iLevel = 0;
	/* Jump the 'continue' keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){
		/* optional numeric argument which tells us how many levels
		 * of enclosing loops we should skip to the end of.
		 */
		char zScratch[GEN_NUM_SCRATCH];
		char *zAlloc = 0;
		SyString sNum;
		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( rc == SXRET_OK ){
			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,
				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);
			if( rc != SXRET_OK ){
				return SXERR_ABORT;
			}
			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);
			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
		}
		if( iLevel < 2 ){
			iLevel = 0;
		}
		pGen->pIn++; /* Jump the optional numeric argument */
	}
	/* Point to the target loop */
	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);
	if( pLoop == 0 ){
		/* Illegal continue */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}else{
		sxu32 nInstrIdx = 0;
		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */
		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);
		/* ROOT C: in a generator, a break/continue crossing inline trys must run their
		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */
		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;
		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){
			/* According to the PHP language reference manual
			 *  Note that unlike some other languages, the continue statement applies to switch
			 *  and acts similar to break. If you have a switch inside a loop and wish to continue
			 *  to the next iteration of the outer loop, use continue 2.
			 */
			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);
			if( rc == SXRET_OK ){
				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);
			}
		}else{
			/* Emit the unconditional jump to the beginning of the target loop */
			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);
			if( pLoop->bPostContinue == TRUE ){
				JumpFixup sJumpFix;
				/* Post-continue */
				sJumpFix.nJumpType = PH7_OP_JMP;
				sJumpFix.nInstrIdx = nInstrIdx;
				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);
			}
		}
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Not so fatal,emit a warning only */
		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");
	}
	/* Statement successfully compiled */
	return SXRET_OK;
}
/*
 * Compile the 'break' statement.
 * According to the PHP language reference
 *  break ends execution of the current for, foreach, while, do-while or switch
 *  structure.
 *  break accepts an optional numeric argument which tells it how many nested
 *  enclosing structures are to be broken out of.
 */
static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)
{
	GenBlock *pLoop; /* Target loop */
	sxi32 iLevel;    /* How many nesting loop to skip */
	sxi32 rc;
	iLevel = 0;
	/* Jump the 'break' keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){
		/* optional numeric argument which tells us how many levels
		 * of enclosing loops we should skip to the end of.
		 */
		char zScratch[GEN_NUM_SCRATCH];
		char *zAlloc = 0;
		SyString sNum;
		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( rc == SXRET_OK ){
			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,
				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);
			if( rc != SXRET_OK ){
				return SXERR_ABORT;
			}
			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);
			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
		}
		if( iLevel < 2 ){
			iLevel = 0;
		}
		pGen->pIn++; /* Jump the optional numeric argument */
	}
	/* Extract the target loop */
	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);
	if( pLoop == 0 ){
		/* Illegal break */
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}else{
		sxu32 nInstrIdx;
		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */
		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);
		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */
		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);
		if( rc == SXRET_OK ){
			/* Fix the jump later when the jump destination is resolved */
			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);
		}
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Not so fatal,emit a warning only */
		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");
	}
	/* Statement successfully compiled */
	return SXRET_OK;
}
/*
 * Compile or record a label.
 *  A label is a target point that is specified by an identifier followed by a colon.
 * Example
 *  goto LABEL;
 *   echo 'Foo';
 *  LABEL:
 *   echo 'Bar';
 */
static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)
{
	GenBlock *pBlock;
	Label sLabel;
	/* Make sure the label does not occur inside a loop or a try{}catch(); block */
	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP|GEN_BLOCK_EXCEPTION,0);
	if( pBlock ){
		sxi32 rc;
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}else{
		SyString *pTarget = &pGen->pIn->sData;
		char *zDup;
		/* Initialize label fields */
		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);
		/* Duplicate label name */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);
		sLabel.bRef  = FALSE;
		sLabel.nLine = pGen->pIn->nLine;
		pBlock = pGen->pCurrent;
		while( pBlock ){
			if( pBlock->iFlags & (GEN_BLOCK_FUNC|GEN_BLOCK_EXCEPTION) ){
				break;
			}
			/* Point to the upper block */
			pBlock = pBlock->pParent;
		}
		if( pBlock ){
			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;
		}else{
			sLabel.pFunc = 0;
		}
		/* Insert in label set */
		SySetPut(&pGen->aLabel,(const void *)&sLabel);
	}
	pGen->pIn += 2; /* Jump the label name and the semi-colon*/
	return SXRET_OK;
}
/*
 * Compile the so hated 'goto' statement.
 * You've probably been taught that gotos are bad, but this sort
 * of rewriting  happens all the time, in fact every time you run
 * a compiler it has to do this.
 * According to the PHP language reference manual
 *   The goto operator can be used to jump to another section in the program.
 *   The target point is specified by a label followed by a colon, and the instruction
 *   is given as goto followed by the desired target label. This is not a full unrestricted goto.
 *   The target label must be within the same file and context, meaning that you cannot jump out
 *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop
 *   or switch structure. You may jump out of these, and a common use is to use a goto in place
 *   of a multi-level break
 */
static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)
{
	JumpFixup sJump;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'goto' keyword */
	if( pGen->pIn >= pGen->pEnd ){
		/* Missing label */
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	if( (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_ID)) == 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}else{
		SyString *pTarget = &pGen->pIn->sData;
		GenBlock *pBlock;
		char *zDup;
		/* Prepare the jump destination */
		sJump.nJumpType = PH7_OP_JMP;
		sJump.nLine = pGen->pIn->nLine;
		/* Duplicate label name */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);
		pBlock = pGen->pCurrent;
		while( pBlock ){
			if( pBlock->iFlags & (GEN_BLOCK_FUNC|GEN_BLOCK_EXCEPTION) ){
				break;
			}
			/* Point to the upper block */
			pBlock = pBlock->pParent;
		}
		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
		}
		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){
			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;
		}else{
			sJump.pFunc = 0;
		}
		/* Emit the unconditional jump */
		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){
			SySetPut(&pGen->aGoto,(const void *)&sJump);
		}
	}
	pGen->pIn++; /* Jump the label name */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");
	}
	/* Statement successfully compiled */
	return SXRET_OK;
}
/*
 * Point to the next PHP chunk that will be processed shortly.
 * Return SXRET_OK on success. Any other return value indicates
 * failure.
 */
static sxi32 GenStateNextChunk(ph7_gen_state *pGen)
{
	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */
	sxu32 nRawObj;
	sxu32 nObjIdx;
	/* Consume raw chunks verbatim without any processing until we get
	 * a PHP block.
	 */
Consume:
	nRawObj = nObjIdx = 0;
	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){
		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);
		if( pRawObj == 0 ){
			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		/* Mark as constant and emit the load constant instruction */
		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);
		++nRawObj;
		pGen->pRawIn++; /* Next chunk */
	}
	if( nRawObj > 0 ){
		/* Emit the consume instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);
	}
	if( pGen->pRawIn < pGen->pRawEnd ){
		SySet *pTokenSet = pGen->pTokenSet;
		/* Reset the token set (and its trivia sidecar) */
		SySetReset(pTokenSet);
		SySetReset(&pGen->aTrivia);
		/* Tokenize input */
		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),
			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);
		/* Point to the fresh token stream */
		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);
		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];
		/* Advance the stream cursor */
		pGen->pRawIn++;
		/* TICKET 1433-011 */
		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){
			static const sxu32 nKeyID = PH7_TKWRD_ECHO;
			sxi32 rc;
			/* Refer to TICKET 1433-009  */
			pGen->pIn->nType = PH7_TK_KEYWORD;
			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);
			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);
			rc = PH7_CompileExpr(pGen,0,0);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}else if( rc != SXERR_EMPTY ){
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
			}
			goto Consume;
		}
	}else{
		/* No more chunks to process */
		pGen->pIn = pGen->pEnd;
		return SXERR_EOF;
	}
	return SXRET_OK;
}
/*
 * Compile a PHP block.
 * A block is simply one or more PHP statements and expressions to compile
 * optionally delimited by braces {}.
 * Return SXRET_OK on success. Any other return value indicates failure
 * and this function takes care of generating the appropriate error
 * message.
 */
static sxi32 PH7_CompileBlock(
	ph7_gen_state *pGen, /* Code generator state */
	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */
	)
{
	sxi32 rc;
	sxu32 nLine;
	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){
		nLine = pGen->pIn->nLine;
		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);
		if( rc != SXRET_OK ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
		/* Compile until we hit the closing braces '}' */
		for(;;){
			if( pGen->pIn >= pGen->pEnd ){
				rc = GenStateNextChunk(&(*pGen));
				if (rc == SXERR_ABORT ){
			 	   return SXERR_ABORT;
				}
				if( rc == SXERR_EOF ){
					/* No more token to process. Missing closing braces */
					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");
					break;
				}
			}
			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){
				/* Closing braces found,break immediately*/
				pGen->pIn++;
				break;
			}
			/* Compile a single statement */
			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		GenStateLeaveBlock(&(*pGen),0);
	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){
		pGen->pIn++;
		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);
		if( rc != SXRET_OK ){
			return SXERR_ABORT;
		}
		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */
		for(;;){
			if( pGen->pIn >= pGen->pEnd ){
				rc = GenStateNextChunk(&(*pGen));
				if (rc == SXERR_ABORT ){
			 	   return SXERR_ABORT;
				}
				if( rc == SXERR_EOF || pGen->pIn >= pGen->pEnd ){
					/* No more token to process */
					if( rc == SXERR_EOF ){
						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,
							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");
					}
					break;
				}
			}
			if( pGen->pIn->nType & PH7_TK_KEYWORD ){
				sxi32 nKwrd;
				/* Keyword found */
				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
				if( nKwrd == nKeywordEnd ||
					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE || nKwrd == PH7_TKWRD_ELIF)) ){
						/* Delimiter keyword found,break */
						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){
							pGen->pIn++; /*  endif;endswitch... */
						}
						break;
				}
			}
			/* Compile a single statement */
			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		GenStateLeaveBlock(&(*pGen),0);
	}else{
		/* Compile a single statement */
		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Jump trailing semi-colons ';' */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the gentle 'while' statement.
 * According to the PHP language reference
 *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.
 *  The basic form of a while statement is:
 *  while (expr)
 *   statement
 *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)
 *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression
 *  is checked each time at the beginning of the loop, so even if this value changes during
 *  the execution of the nested statement(s), execution will not stop until the end of the iteration
 *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while
 *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.
 *  Like with the if statement, you can group multiple statements within the same while loop by surrounding
 *  a group of statements with curly braces, or by using the alternate syntax:
 *  while (expr):
 *    statement
 *   endwhile;
 */
static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)
{
	GenBlock *pWhileBlock = 0;
	SyToken *pTmp,*pEnd = 0;
	sxu32 nFalseJump;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'while' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Delimit the condition */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	/* Synchronize pointers */
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	/* Emit the false jump */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);
	/* Save the instruction index so we can fix it later when the jump destination is resolved */
	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);
	/* Compile the loop body */
	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Emit the unconditional jump to the start of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid
	 * compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the ugly do..while() statement.
 * According to the PHP language reference
 *  do-while loops are very similar to while loops, except the truth expression is checked
 *  at the end of each iteration instead of in the beginning. The main difference from regular
 *  while loops is that the first iteration of a do-while loop is guaranteed to run
 *  (the truth expression is only checked at the end of the iteration), whereas it may not
 *  necessarily run with a regular while loop (the truth expression is checked at the beginning
 *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution
 *  would end immediately).
 *  There is just one syntax for do-while loops:
 *  <?php
 *  $i = 0;
 *  do {
 *   echo $i;
 *  } while ($i > 0);
 * ?>
 */
static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pEnd = 0;
	GenBlock *pDoBlock = 0;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'do' keyword */
	pGen->pIn++;
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Deffer 'continue;' jumps until we compile the block */
	pDoBlock->bPostContinue = TRUE;
	rc = PH7_CompileBlock(&(*pGen),0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( pGen->pIn < pGen->pEnd ){
		nLine = pGen->pIn->nLine;
	}
	if( pGen->pIn >= pGen->pEnd || pGen->pIn->nType != PH7_TK_KEYWORD ||
		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){
			/* Missing 'while' statement */
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto Synchronize;
	}
	/* Jump the 'while' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	/* Delimit the condition */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Fix post-continue jumps now the jump destination is resolved */
	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){
		JumpFixup *aPost;
		VmInstr *pInstr;
		sxu32 nJumpDest;
		sxu32 n;
		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);
		nJumpDest = PH7_VmInstrLength(pGen->pVm);
		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){
			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);
			if( pInstr ){
				/* Fix */
				pInstr->iP2 = nJumpDest;
			}
		}
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	/* Emit the true jump to the beginning of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid
	 * compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the complex and powerful 'for' statement.
 * According to the PHP language reference
 *  for loops are the most complex loops in PHP. They behave like their C counterparts.
 *  The syntax of a for loop is:
 *  for (expr1; expr2; expr3)
 *   statement
 *  The first expression (expr1) is evaluated (executed) once unconditionally at
 *  the beginning of the loop.
 *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to
 *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates
 *  to FALSE, the execution of the loop ends.
 *  At the end of each iteration, expr3 is evaluated (executed).
 *  Each of the expressions can be empty or contain multiple expressions separated by commas.
 *  In expr2, all expressions separated by a comma are evaluated but the result is taken
 *  from the last part. expr2 being empty means the loop should be run indefinitely
 *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might
 *  think, since often you'd want to end the loop using a conditional break statement instead
 *  of using the for truth expression.
 */
static sxi32 PH7_CompileFor(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pPostStart,*pEnd = 0;
	GenBlock *pForBlock = 0;
	sxu32 nFalseJump;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'for' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	/* Delimit the init-expr;condition;post-expr */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		/* Synchronize */
		pGen->pIn = pEnd;
		if( pGen->pIn < pGen->pEnd ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Compile initialization expressions if available */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Pop operand lvalues */
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}else if( rc != SXERR_EMPTY ){
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
	}
	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
			"for: Expected ';' after initialization expressions");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Jump the trailing ';' */
	pGen->pIn++;
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Deffer continue jumps */
	pForBlock->bPostContinue = TRUE;
	/* Compile the condition */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}else if( rc != SXERR_EMPTY ){
		/* Emit the false jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);
		/* Save the instruction index so we can fix it later when the jump destination is resolved */
		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);
	}
	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
			"for: Expected ';' after conditionals expressions");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Jump the trailing ';' */
	pGen->pIn++;
	/* Save the post condition stream */
	pPostStart = pGen->pIn;
	/* Compile the loop body */
	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */
	pGen->pEnd = pTmp;
	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Fix post-continue jumps */
	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){
		JumpFixup *aPost;
		VmInstr *pInstr;
		sxu32 nJumpDest;
		sxu32 n;
		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);
		nJumpDest = PH7_VmInstrLength(pGen->pVm);
		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){
			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);
			if( pInstr ){
				/* Fix jump */
				pInstr->iP2 = nJumpDest;
			}
		}
	}
	/* compile the post-expressions if available */
	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){
		pPostStart++;
	}
	if( pPostStart < pEnd ){
		SyToken *pTmpIn,*pTmpEnd;
		SWAP_DELIMITER(pGen,pPostStart,pEnd);
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( pGen->pIn < pGen->pEnd ){
			/* Syntax error */
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			return SXRET_OK;
		}
		RE_SWAP_DELIMITER(pGen);
		if( rc == SXERR_ABORT ){
			/* Expression handler request an operation abort [i.e: Out-of-memory] */
			return SXERR_ABORT;
		}else if( rc != SXERR_EMPTY){
			/* Pop operand lvalue */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
	}
	/* Emit the unconditional jump to the start of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
}
/* Expression tree validator callback used by the 'foreach' statement.
 * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]
 * are allowed.
 */
static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */
	if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
			"foreach: Expecting a variable name");
		if( rc != SXERR_ABORT ){
			rc = SXERR_INVALID;
		}
	}
	return rc;
}
/*
 * Compile the 'foreach' statement.
 * According to the PHP language reference
 *  The foreach construct simply gives an easy way to iterate over arrays. foreach works
 *  only on arrays (and objects), and will issue an error when you try to use it on a variable
 *  with a different data type or an uninitialized variable. There are two syntaxes; the second
 *  is a minor but useful extension of the first:
 *  foreach (array_expression as $value)
 *    statement
 *  foreach (array_expression as $key => $value)
 *   statement
 *  The first form loops over the array given by array_expression. On each loop, the value
 *  of the current element is assigned to $value and the internal array pointer is advanced
 *  by one (so on the next loop, you'll be looking at the next element).
 *  The second form does the same thing, except that the current element's key will be assigned
 *  to the variable $key on each loop.
 *  Note:
 *  When foreach first starts executing, the internal array pointer is automatically reset to the
 *  first element of the array. This means that you do not need to call reset() before a foreach loop.
 *  Note:
 *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array
 *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during
 *  or after the foreach without resetting it.
 *  You can easily modify array's elements by preceding $value with &. This will assign reference instead
 *  of copying the value.
 */
static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)
{
	SyToken *pCur,*pTmp,*pEnd = 0;
	SyToken *pListStart = 0,*pListEnd = 0;
	GenBlock *pForeachBlock = 0;
	ph7_foreach_info *pInfo;
	sxu32 nFalseJump;
	VmInstr *pInstr;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'foreach' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Delimit the expression */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		/* Synchronize */
		pGen->pIn = pEnd;
		if( pGen->pIn < pGen->pEnd ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Compile the array expression */
	pCur = pGen->pIn;
	while( pCur < pEnd ){
		if( pCur->nType & PH7_TK_KEYWORD ){
			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);
			if( nKeywrd == PH7_TKWRD_AS ){
				/* Break with the first 'as' found */
				break;
			}
		}
		/* Advance the stream cursor */
		pCur++;
	}
	if( pCur <= pGen->pIn ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"foreach: Missing array/object expression");
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pCur;
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pCur ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pCur++; /* Jump the 'as' keyword */
	pGen->pIn = pCur;
	if( pGen->pIn >= pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Create the foreach context */
	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));
	if( pInfo == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");
		return SXERR_ABORT;
	}
	/* Zero the structure */
	SyZero(pInfo,sizeof(ph7_foreach_info));
	/* Initialize structure fields */
	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));
	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed
	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner
	 * '=>'. */
	pCur = GenStateFindTopLevelArrow(pCur,pEnd);
	if( pCur < pEnd ){
		/* Compile the expression holding the key name */
		if( pGen->pIn >= pCur ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");
			if( rc == SXERR_ABORT ){
				/* Don't worry about freeing memory, everything will be released shortly */
				return SXERR_ABORT;
			}
		}else{
			pGen->pEnd = pCur;
			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);
			if( rc == SXERR_ABORT ){
				/* Don't worry about freeing memory, everything will be released shortly */
				return SXERR_ABORT;
			}
			pInstr = PH7_VmPopInstr(pGen->pVm);
			if( pInstr->p3 ){
				/* Record key name */
				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));
			}
			pInfo->iFlags |= PH7_4EACH_STEP_KEY;
		}
		pGen->pIn = &pCur[1]; /* Jump the arrow */
	}
	pGen->pEnd = pEnd;
	if( pGen->pIn >= pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){
		pGen->pIn++;
		/* Pass by reference  */
		pInfo->iFlags |= PH7_4EACH_STEP_REF;
	}
	/* Check if the value target is list() */
	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){
		/* foreach ($arr as list($a, $b)) — list unpacking.
		 * Save the list() token range; we'll compile it after FOREACH_STEP.
		 */
		static int iForeachListCnt = 0;
		char zTmp[128];
		sxu32 nLen;
		char *zDup;
		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);
		/* Save list() token boundaries */
		pListStart = pGen->pIn;
		/* Advance past list(...) — validate parentheses */
		pGen->pIn++; /* Jump 'list' keyword */
		if( pGen->pIn >= pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,
				"foreach: Expected '(' after 'list'");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		pGen->pIn++; /* Jump '(' */
		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);
		if( pListEnd >= pEnd ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"foreach: Missing closing ')' after list");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		pGen->pIn = &pListEnd[1]; /* Past ')' */
		pListEnd = pGen->pIn;
		pInfo->iFlags |= PH7_4EACH_STEP_LIST;
	}else if( pGen->pIn->nType & PH7_TK_OSB ){
		/* foreach ($arr as [$a, $b]) — short list unpacking.
		 * Save the [...] token range; we'll compile it after FOREACH_STEP.
		 */
		static int iForeachShortListCnt = 0;
		char zTmp[128];
		sxu32 nLen;
		char *zDup;
		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);
		/* Save [...] token boundaries */
		pListStart = pGen->pIn;
		/* Advance past [...] */
		pGen->pIn++; /* Jump '[' */
		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);
		if( pListEnd >= pEnd ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"foreach: Missing closing ']' after short list");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		pGen->pIn = &pListEnd[1]; /* Past ']' */
		pListEnd = pGen->pIn;
		pInfo->iFlags |= PH7_4EACH_STEP_LIST;
	}else{
		/* Compile the expression holding the value name */
		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
		pInstr = PH7_VmPopInstr(pGen->pVm);
		if( pInstr->p3 ){
			/* Record value name */
			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));
		}
	}
	/* Emit the 'FOREACH_INIT' instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);
	/* Save the instruction index so we can fix it later when the jump destination is resolved */
	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);
	/* Record the first instruction to execute */
	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);
	/* Emit the FOREACH_STEP instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);
	/* Save the instruction index so we can fix it later when the jump destination is resolved */
	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);
	/* If list() unpacking, emit bytecode to destructure the temp variable */
	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){
		SyToken *pSavedIn,*pSavedEnd;
		/* Load the temporary variable holding the current value onto the stack.
		 * The LOAD_LIST handler expects the array below the variable entries.
		 */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);
		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.
		 * We position the tokens at the construct start so the appropriate compiler
		 * picks up the delimiter and the variable names inside.
		 */
		pSavedIn = pGen->pIn;
		pSavedEnd = pGen->pEnd;
		pGen->pIn = pListStart;
		pGen->pEnd = pListEnd;
		if( pListStart->nType & PH7_TK_OSB ){
			rc = PH7_CompileShortList(&(*pGen),0);
		}else{
			rc = PH7_CompileList(&(*pGen),0);
		}
		pGen->pIn = pSavedIn;
		pGen->pEnd = pSavedEnd;
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
	}
	/* Compile the loop body */
	pGen->pIn = &pEnd[1];
	pGen->pEnd = pTmp;
	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	/* Emit the unconditional jump to the start of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid
	 * compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the infamous if/elseif/else if/else statements.
 * According to the PHP language reference
 *  The if construct is one of the most important features of many languages PHP included.
 *  It allows for conditional execution of code fragments. PHP features an if structure
 *  that is similar to that of C:
 *  if (expr)
 *   statement
 *  else construct:
 *   Often you'd want to execute a statement if a certain condition is met, and a different
 *   statement if the condition is not met. This is what else is for. else extends an if statement
 *   to execute a statement in case the expression in the if statement evaluates to FALSE.
 *   For example, the following code would display a is greater than b if $a is greater than
 *   $b, and a is NOT greater than b otherwise.
 *   The else statement is only executed if the if expression evaluated to FALSE, and if there
 *   were any elseif expressions - only if they evaluated to FALSE as well
 *  elseif
 *   elseif, as its name suggests, is a combination of if and else. Like else, it extends
 *   an if statement to execute a different statement in case the original if expression evaluates
 *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif
 *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger
 *   than b, a equal to b or a is smaller than b:
 *   <?php
 *    if ($a > $b) {
 *     echo "a is bigger than b";
 *    } elseif ($a == $b) {
 *     echo "a is equal to b";
 *    } else {
 *     echo "a is smaller than b";
 *    }
 *    ?>
 */
static sxi32 PH7_CompileIf(ph7_gen_state *pGen)
{
	SyToken *pToken,*pTmp,*pEnd = 0;
	GenBlock *pCondBlock = 0;
	sxu32 nJumpIdx;
	sxu32 nKeyID;
	sxi32 rc;
	/* Jump the 'if' keyword */
	pGen->pIn++;
	pToken = pGen->pIn;
	/* Create the conditional block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Process as many [if/else if/elseif/else] blocks as we can */
	for(;;){
		if( pToken >= pGen->pEnd || (pToken->nType & PH7_TK_LPAREN) == 0 ){
			/* Syntax error */
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		/* Jump the left parenthesis '(' */
		pToken++;
		/* Delimit the condition */
		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
		if( pToken >= pEnd || (pEnd->nType & PH7_TK_RPAREN) == 0 ){
			/* Syntax error */
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		/* Swap token streams */
		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);
		/* Compile the condition */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		/* Update token stream */
		while(pGen->pIn < pEnd ){
			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);
			pGen->pIn++;
		}
		pGen->pIn  = &pEnd[1];
		pGen->pEnd = pTmp;
		if( rc == SXERR_ABORT ){
			/* Expression handler request an operation abort [i.e: Out-of-memory] */
			return SXERR_ABORT;
		}
		/* Emit the false jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);
		/* Save the instruction index so we can fix it later when the jump destination is resolved */
		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);
		/* Compile the body */
		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			break;
		}
		/* Ensure that the keyword ID is 'else if' or 'else' */
		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( (nKeyID & (PH7_TKWRD_ELSE|PH7_TKWRD_ELIF)) == 0 ){
			break;
		}
		/* Emit the unconditional jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);
		/* Save the instruction index so we can fix it later when the jump destination is resolved */
		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);
		if( nKeyID & PH7_TKWRD_ELSE ){
			pToken = &pGen->pIn[1];
			if( pToken >= pGen->pEnd || (pToken->nType & PH7_TK_KEYWORD) == 0 ||
				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){
					break;
			}
			pGen->pIn++; /* Jump the 'else' keyword */
		}
		pGen->pIn++; /* Jump the 'elseif/if' keyword */
		/* Synchronize cursors */
		pToken = pGen->pIn;
		/* Fix the false jump */
		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));
	} /* For(;;) */
	/* Fix the false jump */
	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){
			/* Compile the else block */
			pGen->pIn++;
			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);
			if( rc == SXERR_ABORT ){

				return SXERR_ABORT;
			}
	}
	nJumpIdx = PH7_VmInstrLength(pGen->pVm);
	/* Fix all unconditional jumps now the destination is resolved */
	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);
	/* Release the conditional block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the global construct.
 * According to the PHP language reference
 *  In PHP global variables must be declared global inside a function if they are going
 *  to be used in that function.
 *  Example #1 Using global
 *  <?php
 *   $a = 1;
 *   $b = 2;
 *   function Sum()
 *   {
 *    global $a, $b;
 *    $b = $a + $b;
 *   }
 *   Sum();
 *   echo $b;
 *  ?>
 *  The above script will output 3. By declaring $a and $b global within the function
 *  all references to either variable will refer to the global version. There is no limit
 *  to the number of global variables that can be manipulated by a function.
 */
static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pNext = 0;
	sxi32 nExpr;
	sxi32 rc;
	/* Jump the 'global' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_SEMI) ){
		/* Nothing to process */
		return SXRET_OK;
	}
	pTmp = pGen->pEnd;
	nExpr = 0;
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){
		if( pGen->pIn < pNext ){
			pGen->pEnd = pNext;
			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}else{
				pGen->pIn++;
				if( pGen->pIn >= pGen->pEnd ){
					/* Emit a warning */
					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");
				}else{
					rc = PH7_CompileExpr(&(*pGen),0,0);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}else if(rc != SXERR_EMPTY ){
						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);
						if( pLast && pLast->iOp == PH7_OP_LOADC ){
							/* Variable name, not a constant */
							pLast->iP1 = 0;
						}
						nExpr++;
					}
				}
			}
		}
		/* Next expression in the stream */
		pGen->pIn = pNext;
		/* Jump trailing commas */
		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){
			pGen->pIn++;
		}
	}
	/* Restore token stream */
	pGen->pEnd = pTmp;
	if( nExpr > 0 ){
		/* Emit the uplink instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);
	}
	return SXRET_OK;
}
/*
 * Compile the return statement.
 * According to the PHP language reference
 *  If called from within a function, the return() statement immediately ends execution
 *  of the current function, and returns its argument as the value of the function call.
 *  return() will also end the execution of an eval() statement or script file.
 *  If called from the global scope, then execution of the current script file is ended.
 *  If the current script file was include()ed or require()ed, then control is passed back
 *  to the calling file. Furthermore, if the current script file was include()ed, then the value
 *  given to return() will be returned as the value of the include() call. If return() is called
 *  from within the main script file, then script execution end.
 *  Note that since return() is a language construct and not a function, the parentheses
 *  surrounding its arguments are not required. It is common to leave them out, and you actually
 *  should do so as PHP has less work to do in this case.
 *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.
 */
static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)
{
	sxi32 nRet = 0; /* TRUE if there is a return value */
	sxi32 rc;
	sxu32 nLine = pGen->pIn->nLine;
	GenBlock *pFuncBlock = pGen->pCurrent;
	/* A `never`-returning function must not contain a `return` statement at all
	 * (PHP compile error), with or without a value. Find the enclosing function
	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is
	 * recorded (nErr>0 fails the whole compile); the statement is still consumed
	 * normally below so token processing stays consistent. */
	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){
		pFuncBlock = pFuncBlock->pParent;
	}
	if( pFuncBlock && pFuncBlock->pUserData
	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){
		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,
			"A never-returning function must not return");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Jump the 'return' keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Compile the expression */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if(rc != SXERR_EMPTY ){
			nRet = 1;
		}
	}
	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every
	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the
	 * live aException stack). With no enclosing try the action materializes immediately, so
	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */
	if( pGen->bInGenerator ){
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);
		return SXRET_OK;
	}
	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this
	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with
	 * bReturnPropagates), the VM must return from the enclosing function rather
	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),
	 * so the VM can tell a real `return` from the body simply ending. */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);
	return SXRET_OK;
}
/*
 * Compile a yield expression.
 * Called from the expression code generator when a yield node is encountered.
 * Handles: yield, yield $value, yield $key => $value
 * The yield expression evaluates to the value passed via Generator::send().
 */
PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)
{
	SyToken *pTmp, *pSplit;
	sxi32 iP1 = 0; /* 1 if value present */
	sxi32 iP2 = 0; /* 1 if key => value */
	sxi32 rc;
	(void)iCompileFlag;
	/* pGen->pIn points to 'yield' keyword, skip it */
	pGen->pIn++;
	/* Now pGen->pIn points to the first token after 'yield'
	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */
	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a
	 * contextual identifier, not a keyword; a variable named $from lexes as
	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)
		&& pGen->pIn->sData.nByte == 4
		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){
		pGen->pIn++; /* Skip 'from' */
		rc = PH7_CompileExpr(pGen, 0, 0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( rc == SXERR_EMPTY ){
			rc = PH7_GenCompileError(pGen, E_ERROR,
				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,
				"Missing expression after 'yield from'");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);
		return SXRET_OK;
	}
	if( pGen->pIn >= pGen->pEnd ){
		/* Bare yield — no value */
		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);
		return SXRET_OK;
	}
	/* Scan for '=>' at nesting level 0 to detect key => value syntax */
	pSplit = 0;
	{
		SyToken *pCur = pGen->pIn;
		sxi32 nNest = 0;
		while( pCur < pGen->pEnd ){
			if( pCur->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
				nNest++;
			}else if( pCur->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
				nNest--;
			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){
				pSplit = pCur;
				break;
			}
			pCur++;
		}
	}
	pTmp = pGen->pEnd;
	if( pSplit ){
		/* yield $key => $value */
		pGen->pEnd = pSplit;
		rc = PH7_CompileExpr(pGen, 0, 0);
		if( rc == SXERR_ABORT ) return SXERR_ABORT;
		pGen->pIn = pSplit + 1; /* Skip '=>' */
		pGen->pEnd = pTmp;
		rc = PH7_CompileExpr(pGen, 0, 0);
		if( rc == SXERR_ABORT ) return SXERR_ABORT;
		iP1 = 1;
		iP2 = 1;
	}else{
		/* yield $value */
		rc = PH7_CompileExpr(pGen, 0, 0);
		if( rc == SXERR_ABORT ) return SXERR_ABORT;
		if( rc != SXERR_EMPTY ){
			iP1 = 1;
		}
	}
	pGen->pEnd = pTmp;
	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);
	return SXRET_OK;
}
/*
 * Compile the die/exit language construct.
 * The role of these constructs is to terminate execution of the script.
 * Shutdown functions will always be executed even if exit() is called.
 */
static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)
{
	sxi32 nExpr = 0;
	sxi32 rc;
	/* Jump the die/exit keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Compile the expression */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if(rc != SXERR_EMPTY ){
			nExpr = 1;
		}
	}
	/* Emit the HALT instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);
	return SXRET_OK;
}
/*
 * Compile the 'echo' language construct.
 */
static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pNext = 0;
	sxi32 rc;
	/* Jump the 'echo' keyword */
	pGen->pIn++;
	/* Compile arguments one after one */
	pTmp = pGen->pEnd;
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){
		if( pGen->pIn < pNext ){
			pGen->pEnd = pNext;
			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}else if( rc != SXERR_EMPTY ){
				/* Emit the consume instruction */
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
	return SXRET_OK;
}
/*
 * Compile the static statement.
 * According to the PHP language reference
 *  Another important feature of variable scoping is the static variable.
 *  A static variable exists only in a local function scope, but it does not lose its value
 *  when program execution leaves this scope.
 *  Static variables also provide one way to deal with recursive functions.
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the static variable while
 *  the zend engine would allow only simple scalar value.
 *  Example
 *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine
 *    Refer to the official documentation for more information on this feature.
 */
static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)
{
	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */
	ph7_vm_func *pFunc;             /* Enclosing function */
	GenBlock *pBlock;
	SyString *pName;
	char *zDup;
	sxu32 nLine;
	sxi32 rc;
	/* `static function () {}` / `static fn () =>` at statement position is an
	 * EXPRESSION statement (a bare static closure), not a static-variable
	 * declaration — hand it to the expression compiler (php accepts it). */
	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)
	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION
	  || SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXERR_EMPTY ){
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
		return SXRET_OK;
	}
	/* Jump the static keyword */
	nLine = pGen->pIn->nLine;
	pGen->pIn++;
	/* Extract the enclosing function if any */
	pBlock = pGen->pCurrent;
	while( pBlock ){
		if( pBlock->iFlags & GEN_BLOCK_FUNC){
			break;
		}
		/* Point to the upper block */
		pBlock = pBlock->pParent;
	}
	if( pBlock == 0 ){
		/* Static statement,called outside of a function body,treat it as a simple variable. */
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		/* Compile the expression holding the variable */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXERR_EMPTY ){
			/* Emit the POP instruction */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
		return SXRET_OK;
	}
	pFunc = (ph7_vm_func *)pBlock->pUserData;
	/* Make sure we are dealing with a valid statement */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 || &pGen->pIn[1] >= pGen->pEnd ||
		(pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
	}
	pGen->pIn++;
	/* Extract variable name */
	pName = &pGen->pIn->sData;
	pGen->pIn++; /* Jump the var name */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_EQUAL/*'='*/)) == 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);
		goto Synchronize;
	}
	/* Initialize the structure describing the static variable */
	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
	sStatic.nIdx = SXU32_HIGH; /* Not yet created */
	/* Duplicate variable name */
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zDup == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);
	/* Check if we have an expression to compile */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){
		SySet *pInstrContainer;
		/* TICKET 1433-014: Symisc extension to the PHP programming language
		 * Static variable can take any complex expression including function
		 * call as their initialization value.
		 * Example:
		 *		static $var = foo(1,4+5,bar());
		 */
		pGen->pIn++; /* Jump the equal '=' sign */
		/* Swap bytecode container */
		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);
		/* Compile the expression */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		/* Emit the done instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
		/* Restore default bytecode container */
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	}
	/* Finally save the compiled static variable in the appropriate container */
	SySetPut(&pFunc->aStatic,(const void *)&sStatic);
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous
	 * statement.
	 */
	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the var statement.
 * Symisc Extension:
 *      var statement can be used outside of a class definition.
 */
static sxi32 PH7_CompileVar(ph7_gen_state *pGen)
{
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'var' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");
		/* Synchronize with the first semi-colon */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){
			pGen->pIn++;
		}
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}else{
		/* Compile the expression */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXERR_EMPTY ){
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
	}
	return SXRET_OK;
}
/*
 * Namespace-qualify a literal in-place for CALL/NEW instructions.
 * Resolution: use imports -> current NS prefix. The VM handles global fallback.
 * Only rewrites unqualified names (no backslash) when a namespace is active.
 */
/*
 * Namespace-qualify a name for CALL/NEW/instanceof instructions.
 * Instead of mutating the interned literal (which would corrupt the literal
 * hash and any shared references), this creates a new literal entry with the
 * qualified name and updates the instruction's operand index.
 *
 * Resolution order:
 *   1. Check the given import table (pImports) — matches even outside namespaces.
 *   2. If no import matches and a namespace is active, prepend the current NS.
 *   3. Otherwise return the original literal index unchanged.
 *
 * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution
 * came from an import (step 1) and 0 otherwise.
 * Returns the (possibly new) literal index.
 */
static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)
{
	ph7_value *pLit;
	const char *zLit;
	SyString sQualified;
	sxu32 nLit;
	sxu32 k;
	sxu32 nNewIdx;
	int hasNsSep;
	SyHashEntry *pImport;
	ph7_value *pNew;
	if( pFromImport ){
		*pFromImport = 0;
	}
	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);
	if( !pLit || !(pLit->iFlags & MEMOBJ_STRING) || SyBlobLength(&pLit->sBlob) == 0 ){
		return nOrigIdx;
	}
	zLit = (const char *)SyBlobData(&pLit->sBlob);
	nLit = (sxu32)SyBlobLength(&pLit->sBlob);
	/* Skip if already qualified (contains backslash) */
	hasNsSep = 0;
	for( k = 0; k < nLit; k++ ){
		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }
	}
	if( hasNsSep ){
		return nOrigIdx;
	}
	/* Check use imports first (works even outside namespaces) */
	SyBlobReset(&pGen->sWorker);
	pImport = SyHashGet(pImports,(const void *)zLit,nLit);
	if( pImport ){
		const char *zFQN = (const char *)pImport->pUserData;
		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));
		if( pFromImport ){
			*pFromImport = 1;
		}
	}else{
		if( SyBlobLength(&pGen->sNamespace) == 0 ){
			return nOrigIdx; /* Not in a namespace and no import match */
		}
		/* Prepend current namespace */
		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
		SyBlobAppend(&pGen->sWorker,"\\",1);
		SyBlobAppend(&pGen->sWorker,zLit,nLit);
	}
	/* Look up or create a new literal for the qualified name */
	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));
	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){
		return nNewIdx; /* Already interned */
	}
	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);
	if( pNew == 0 ){
		return nOrigIdx; /* OOM, fall back to original */
	}
	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);
	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);
	return nNewIdx;
}
/*
 * Resolve a class/function name at compile time through use imports and current namespace.
 * Writes the resolved FQN into pOut. Caller must release pOut.
 */
static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)
{
	SyHashEntry *pImport;
	/* Check use imports first */
	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);
	if( pImport ){
		const char *zFQN = (const char *)pImport->pUserData;
		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));
		return;
	}
	/* Prepend current namespace if active */
	if( SyBlobLength(&pGen->sNamespace) > 0 ){
		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
		SyBlobAppend(pOut,"\\",1);
	}
	SyBlobAppend(pOut,pName->zString,pName->nByte);
}
/*
 * Build a fully-qualified name by prepending the current namespace to a short name.
 * If no namespace is active, pOut receives a copy of the short name.
 * The caller must release pOut when done.
 */
static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)
{
	if( SyBlobLength(&pGen->sNamespace) > 0 ){
		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
		SyBlobAppend(pOut,"\\",1);
	}
	SyBlobAppend(pOut,pName->zString,pName->nByte);
}
/*
 * Compile a namespace statement
 * According to the PHP language reference manual
 *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.
 *  This can be seen as an abstract concept in many places. For example, in any operating system
 *  directories serve to group related files, and act as a namespace for the files within them.
 *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other
 *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt
 *  file outside of the /home/greg directory, we must prepend the directory name to the file name using
 *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the
 *  programming world.
 *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications
 *  encounter when creating re-usable code elements such as classes or functions:
 *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party
 *  classes/functions/constants.
 *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving
 *  readability of source code.
 *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.
 *  Here is an example of namespace syntax in PHP:
 *       namespace my\name; // see "Defining Namespaces" section
 *       class MyClass {}
 *       function myfunction() {}
 *       const MYCONST = 1;
 *       $a = new MyClass;
 *       $c = new \my\name\MyClass;
 *       $a = strlen('hi');
 *       $d = namespace\MYCONST;
 *       $d = __NAMESPACE__ . '\MYCONST';
 *       echo constant($d);
 * NOTE
 *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT
 *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.
 */
/*
 * Return a PHP-style type name for a token, used in parse error messages.
 */
static const char * TokenTypeName(sxu32 nType)
{
	if( nType & PH7_TK_INTEGER ){ return "integer"; }
	if( nType & PH7_TK_REAL ){ return "float"; }
	if( nType & (PH7_TK_DSTR|PH7_TK_SSTR|PH7_TK_HEREDOC|PH7_TK_NOWDOC) ){ return "string"; }
	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }
	if( nType & PH7_TK_ID ){ return "identifier"; }
	if( nType & PH7_TK_DOLLAR ){ return "variable"; }
	return "token";
}
static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)
{
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	pGen->pIn++; /* Jump the 'namespace' keyword */
	/* Reset namespace and clear previous use imports */
	SyBlobReset(&pGen->sNamespace);
	SyHashRelease(&pGen->hUseImports);
	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);
	SyHashRelease(&pGen->hUseFuncImports);
	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);
	SyHashRelease(&pGen->hUseConstImports);
	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);
	if( pGen->pIn >= pGen->pEnd ){
		/* Global namespace (bare "namespace;") */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);
		return SXRET_OK;
	}
	if( pGen->pIn->nType & PH7_TK_SEMI ){
		/* namespace; — switch to global namespace */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);
		return SXRET_OK;
	}
	if( pGen->pIn->nType & PH7_TK_OCB ){
		/* namespace { } — global namespace block */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);
		return SXRET_OK;
	}
	/* Collect the namespace path: namespace Foo\Bar\Baz */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP|PH7_TK_ID|PH7_TK_KEYWORD)) ){
		if( pGen->pIn->nType & PH7_TK_NSSEP ){
			/* Append backslash separator */
			if( SyBlobLength(&pGen->sNamespace) > 0 ){
				SyBlobAppend(&pGen->sNamespace,"\\",1);
			}
		}else{
			/* Append identifier */
			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);
		}
		pGen->pIn++;
	}
	/* Emit a runtime namespace switch so the VM tracks the active namespace
	 * at the correct program counter, not just the last one compiled. */
	{
		char *zNsDup = 0;
		if( SyBlobLength(&pGen->sNamespace) > 0 ){
			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
		}
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,
			"syntax error, unexpected %s \"%z\", expecting \"{\"",
			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return SXRET_OK;
}
/*
 * Compile the 'use' statement
 * According to the PHP language reference manual
 *  The ability to refer to an external fully qualified name with an alias or importing
 *  is an important feature of namespaces. This is similar to the ability of unix-based
 *  filesystems to create symbolic links to a file or to a directory.
 *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name
 *  aliasing an interface name, and aliasing a namespace name. Note that importing
 *  a function or constant is not supported.
 *  In PHP, aliasing is accomplished with the 'use' operator.
 * NOTE
 *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT
 *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.
 */
static sxi32 PH7_CompileUse(ph7_gen_state *pGen)
{
	sxu32 nLine;
	sxi32 rc;
	SyBlob sPath;
	SyString sAlias;
	SyToken *pLast;
	char *zDup;
	int iUseType; /* 0=class, 1=function, 2=const */
	SyHash *pGenHash;   /* Compile-time import table */
	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */
	nLine = pGen->pIn->nLine;
	pGen->pIn++; /* Jump the 'use' keyword */
	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */
	iUseType = 0;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));
		if( nKey == PH7_TKWRD_FUNCTION ){
			iUseType = 1;
			pGen->pIn++;
		}else if( nKey == PH7_TKWRD_CONST ){
			iUseType = 2;
			pGen->pIn++;
		}
	}
	/* Select target hash tables based on import type */
	switch( iUseType ){
		case 1:
			pGenHash = &pGen->hUseFuncImports;
			pVmHash = 0; /* Function imports resolved at compile time only */
			break;
		case 2:
			pGenHash = &pGen->hUseConstImports;
			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */
			break;
		default:
			pGenHash = &pGen->hUseImports;
			pVmHash = &pGen->pVm->hUseImports;
			break;
	}
	SyBlobInit(&sPath,&pGen->pVm->sAllocator);
	/* Process one or more use declarations separated by commas */
	for(;;){
		if( pGen->pIn >= pGen->pEnd ){
			break;
		}
		SyBlobReset(&sPath);
		pLast = 0;
		/* Collect the full namespace path */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP|PH7_TK_ID)) ){
			if( pGen->pIn->nType & PH7_TK_ID ){
				pLast = pGen->pIn;
				if( SyBlobLength(&sPath) > 0 ){
					SyBlobAppend(&sPath,"\\",1);
				}
				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);
			}
			pGen->pIn++;
		}
		if( pLast == 0 ){
			/* Empty path */
			break;
		}
		/* Default alias is the last component of the path */
		sAlias = pLast->sData;
		/* Check for explicit alias: use Foo\Bar as Baz */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)
			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){
			pGen->pIn++; /* Jump 'as' */
			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){
				sAlias = pGen->pIn->sData;
				pGen->pIn++;
			}
		}
		/* Check for duplicate import alias (per-type) */
		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
				"Cannot use %.*s as %z because the name is already in use",
				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);
			if( rc == SXERR_ABORT ){
				SyBlobRelease(&sPath);
				return SXERR_ABORT;
			}
		}
		/* Register the import: alias -> FQN.
		 * Strings are allocated from the VM pool allocator and freed
		 * when the entire VM is released. SyHashRelease does not free
		 * user-data, but pool memory is reclaimed in bulk at shutdown. */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));
		if( zDup ){
			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);
			if( pVmHash ){
				/* Class imports: populate VM table directly (class resolution
				 * is compile-time only, the VM copy is kept for legacy reasons). */
				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);
				if( zAliasDup ){
					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);
				}
			}
			if( iUseType == 2 ){
				/* Const imports: emit a runtime instruction so imports are
				 * namespace-scoped (NSSWITCH clears the VM table). */
				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);
				if( zAliasDup ){
					/* Encode alias length in iP1, alias string in p3 is not enough —
					 * we need both alias and FQN.  Pack them: iP1=alias length,
					 * iP2 unused, p3 points to a two-pointer struct. */
					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);
					if( azPair ){
						azPair[0] = zAliasDup;
						azPair[1] = zDup;
						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);
					}
				}
			}
		}
		/* Check for comma (multiple use declarations) */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){
			pGen->pIn++;
		}else{
			break;
		}
	}
	SyBlobRelease(&sPath);
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",
			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return SXRET_OK;
}
/*
 * Compile the stupid 'declare' language construct.
 *
 * According to the PHP language reference manual.
 *  The declare construct is used to set execution directives for a block of code.
 *  The syntax of declare is similar to the syntax of other flow control constructs:
 *  declare (directive)
 *   statement
 * The directive section allows the behavior of the declare block to be set.
 *  Currently only two directives are recognized: the ticks directive and the encoding directive.
 * The statement part of the declare block will be executed - how it is executed and what side
 * effects occur during execution may depend on the directive set in the directive block.
 * The declare construct can also be used in the global scope, affecting all code following
 * it (however if the file with declare was included then it does not affect the parent file).
 * <?php
 * // these are the same:
 * // you can use this:
 * declare(ticks=1) {
 *   // entire script here
 * }
 * // or you can use this:
 * declare(ticks=1);
 * // entire script here
 * ?>
 *
 * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.
 */
/*
 * Match a directive name against a known literal (case-insensitive).
 */
static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)
{
	return SyStringLength(pName) == nWant
	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;
}

static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	SyToken *pBodyEnd = 0;
	SyToken *pBodyStart;
	SyToken *pCursor;
	int bHasStrictTypes;
	int bBlockForm;
	int bPlacementOk;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'declare' keyword */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchro;
	}
	pGen->pIn++; /* Jump the left parenthesis */
	pBodyStart = pGen->pIn;
	/* Delimit the directive body (between the outer '(' and its matching ')'). */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);
	if( pBodyEnd >= pGen->pEnd ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)
	 * now delimits the comma-separated directive list. */
	pGen->pIn = &pBodyEnd[1];
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_OCB/*'{'*/)) == 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;
	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );
	bHasStrictTypes = 0;
	/* First pass: scan directive names to detect any strict_types occurrence.
	 * PHP applies strict_types placement and block-form rules as long as the
	 * directive appears anywhere in the list, before validating values. */
	pCursor = pBodyStart;
	while( pCursor < pBodyEnd ){
		if( (pCursor->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) != 0 ){
			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){
				bHasStrictTypes = 1;
				break;
			}
		}
		pCursor++;
	}
	if( bHasStrictTypes && bBlockForm ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"strict_types declaration must not use block mode");
		if( rc == SXERR_ABORT ) return SXERR_ABORT;
		return SXRET_OK;
	}
	if( bHasStrictTypes && !bPlacementOk ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"strict_types declaration must be the very first statement in the script");
		if( rc == SXERR_ABORT ) return SXERR_ABORT;
		return SXRET_OK;
	}
	/* Second pass: iterate comma-separated directives and apply each. */
	pCursor = pBodyStart;
	while( pCursor < pBodyEnd ){
		SyToken *pNameTok;
		SyToken *pEqTok;
		SyToken *pValTok;
		SyString *pDirName;
		int bIsStrict;
		int iStrictValue;
		pNameTok = pCursor;
		if( (pNameTok->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"declare: Expecting a directive name");
			if( rc == SXERR_ABORT ) return SXERR_ABORT;
			return SXRET_OK;
		}
		pEqTok = pNameTok + 1;
		if( pEqTok >= pBodyEnd || (pEqTok->nType & PH7_TK_EQUAL) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"declare: Expecting '=' after directive name");
			if( rc == SXERR_ABORT ) return SXERR_ABORT;
			return SXRET_OK;
		}
		pValTok = pEqTok + 1;
		if( pValTok >= pBodyEnd ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"declare: Expecting value after '='");
			if( rc == SXERR_ABORT ) return SXERR_ABORT;
			return SXRET_OK;
		}
		pDirName = &pNameTok->sData;
		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);
		if( bIsStrict ){
			/* strict_types value must be a literal 0 or 1 (integer). PHP
			 * distinguishes non-literal (bareword) from other bad values. */
			if( (pValTok->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) != 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"declare(strict_types) value must be a literal");
				if( rc == SXERR_ABORT ) return SXERR_ABORT;
				return SXRET_OK;
			}
			iStrictValue = -1;
			if( pValTok->nType & PH7_TK_INTEGER ){
				const char *zv = SyStringData(&pValTok->sData);
				sxu32 nv = SyStringLength(&pValTok->sData);
				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;
				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;
			}
			if( iStrictValue != 0 && iStrictValue != 1 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"strict_types declaration must have 0 or 1 as its value");
				if( rc == SXERR_ABORT ) return SXERR_ABORT;
				return SXRET_OK;
			}
			pGen->bStrictTypes = (sxi8)iStrictValue;
		}else{
			/* Other directives (ticks, encoding, or unknown) remain no-ops —
			 * preserve the legacy notice so callers relying on the old
			 * behavior don't regress. */
			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,
				"the declare construct is a no-op in the current release of the PH7(%s) engine",
				ph7_lib_version()
				);
		}
		pCursor = pValTok + 1;
		/* Consume separating comma (or end). */
		if( pCursor < pBodyEnd ){
			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"declare: Expecting ',' or ')' after directive value");
				if( rc == SXERR_ABORT ) return SXERR_ABORT;
				return SXRET_OK;
			}
			pCursor++;
		}
	}
	/* Declares never lock the first-statement rule: PHP allows another
	 * declare(strict_types) to follow immediately, or a declare(ticks)
	 * to precede strict_types. Only non-declare statements lock. */
	return SXRET_OK;
Synchro:
	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_OCB/*'{'*/)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Process default argument values. That is,a function may define C++-style default value
 * as follows:
 * function makecoffee($type = "cappuccino")
 * {
 *   return "Making a cup of $type.\n";
 * }
 * Symisc eXtension.
 *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous
 *      functions,array member,..] unlike the zend which would allow only single scalar value.
 *      Example: Work only with PH7,generate error under zend
 *      function test($a = 'Hello'.'World: '.rand_str(3))
 *      {
 *       var_dump($a);
 *      }
 *     //call test without args
 *      test();
 * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)
 *      Example:
 *           function a(string $a){} function b(int $a,string $c,float $d){}
 * 3 -) Function overloading!!
 *      Example:
 *      function foo($a) {
 *   	  return $a.PHP_EOL;
 *	    }
 *	    function foo($a, $b) {
 *   	  return $a + $b;
 *	    }
 *	    echo foo(5); // Prints "5"
 *	    echo foo(5, 2); // Prints "7"
 *      // Same arg
 *	   function foo(string $a)
 *	   {
 *	     echo "a is a string\n";
 *	     var_dump($a);
 *	   }
 *	  function foo(int $a)
 *	  {
 *	    echo "a is integer\n";
 *	    var_dump($a);
 *	  }
 *	  function foo(array $a)
 *	  {
 * 	    echo "a is an array\n";
 * 	    var_dump($a);
 *	  }
 *	  foo('This is a great feature'); // a is a string [first foo]
 *	  foo(52); // a is integer [second foo]
 *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]
 * Please refer to the official documentation for more information on the powerful extension
 * introduced by the PH7 engine.
 */
static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)
{
	SyToken *pTmpIn,*pTmpEnd;
	SySet *pInstrContainer;
	sxi32 rc;
	/* Swap token stream */
	SWAP_DELIMITER(pGen,pIn,pEnd);
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);
	/* Compile the expression holding the argument value */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	RE_SWAP_DELIMITER(pGen);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXRET_OK;
}
/*
 * Collect function arguments one after one.
 * According to the PHP language reference manual.
 * Information may be passed to functions via the argument list, which is a comma-delimited
 * list of expressions.
 * PHP supports passing arguments by value (the default), passing by reference
 * and default argument values. Variable-length argument lists are also supported,
 * see also the function references for func_num_args(), func_get_arg(), and func_get_args()
 * for more information.
 * Example #1 Passing arrays to functions
 * <?php
 * function takes_array($input)
 * {
 *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];
 * }
 * ?>
 * Making arguments be passed by reference
 * By default, function arguments are passed by value (so that if the value of the argument
 * within the function is changed, it does not get changed outside of the function).
 * To allow a function to modify its arguments, they must be passed by reference.
 * To have an argument to a function always passed by reference, prepend an ampersand (&)
 * to the argument name in the function definition:
 * Example #2 Passing function parameters by reference
 * <?php
 * function add_some_extra(&$string)
 * {
 *   $string .= 'and something extra.';
 * }
 * $str = 'This is a string, ';
 * add_some_extra($str);
 * echo $str;    // outputs 'This is a string, and something extra.'
 * ?>
 *
 * PH7 have introduced powerful extension including full type hinting,function overloading
 * complex agrument values.Please refer to the official documentation for more information
 * on these extension.
 */
static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)
{
	ph7_vm_func_arg sArg; /* Current processed argument */
	SyToken *pIn;  /* Token stream */
	SyBlob sSig;         /* Function signature */
	char *zDup;          /* Copy of argument name */
	sxi32 rc;

	pIn = pGen->pIn;
	SyBlobInit(&sSig,&pGen->pVm->sAllocator);
	/* Process arguments one after one */
	for(;;){
		if( pIn >= pEnd ){
			/* No more arguments to process */
			break;
		}
		SyZero(&sArg,sizeof(ph7_vm_func_arg));
		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));
		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));
		SyStringInitFromBuf(&sArg.sTypeName,0,0);
		/* Parameter #[...] attributes: the group precedes the parameter's
		 * first token inside the main token stream */
		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* Parse optional visibility + readonly modifiers (constructor property
		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility
		 * keyword and/or `readonly` is present; `readonly` may appear on either
		 * side of the visibility keyword (`public readonly T $x`,
		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */
		{
			int bReadonly = 0, bVisSeen = 0;
			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;
			sxi32 iSetVisFlag = 0;
			int nSetTok;
			sxi32 nSetVis;
			if( pIn < pEnd && GenStateIsReadonly(pIn) ){
				bReadonly = 1;
				pIn++;
			}
			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);
			if( nSetVis ){
				/* Leading `private(set)` etc: promoted with a public read side */
				iSetVisFlag = GenStateSetVisFlag(nSetVis);
				bVisSeen = 1;
				pIn += nSetTok;
				if( pIn < pEnd && GenStateIsReadonly(pIn) ){
					bReadonly = 1;
					pIn++;
				}
			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){
				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);
				if( nKw == PH7_TKWRD_PUBLIC || nKw == PH7_TKWRD_PROTECTED || nKw == PH7_TKWRD_PRIVATE ){
					bVisSeen = 1;
					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE
						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED
						: PH7_CLASS_PROT_PUBLIC;
					pIn++;
					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);
					if( nSetVis ){
						/* `public private(set) T $x` promoted form */
						iSetVisFlag = GenStateSetVisFlag(nSetVis);
						pIn += nSetTok;
					}
					if( pIn < pEnd && GenStateIsReadonly(pIn) ){
						bReadonly = 1;
						pIn++;
					}
				}
			}
			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){
				sArg.iFlags |= VM_FUNC_ARG_PRIV_SET;
			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){
				sArg.iFlags |= VM_FUNC_ARG_PROT_SET;
			}
			if( bVisSeen || bReadonly ){
				if( !bCtorCtx ){
					if( bAbstractCtx ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,
							"Cannot declare promoted property in an abstract constructor");
					}else{
						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,
							"Cannot declare promoted property outside a constructor");
					}
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					return SXERR_SYNTAX;
				}
				sArg.iFlags |= VM_FUNC_ARG_PROMOTED;
				sArg.iPromoteVis = iVis;
				if( bReadonly ){
					sArg.iFlags |= VM_FUNC_ARG_READONLY;
				}
			}
		}
		/* Parse optional type hint (single, nullable shorthand, or union) */
		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0
			&& (pIn->nType & PH7_TK_AMPER) == 0
			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){
			sxu32 nLineLocal = pIn->nLine;
			sxi32 iTFlags = 0;
			pGen->pIn = pIn;
			rc = GenStateParseUnionTypeDecl(
				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,
				&iTFlags, &sArg.sTypeName,
				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,
				/* bAllowVoid */ 0,
						nLineLocal);
			pIn = pGen->pIn;
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}else if( rc == SXERR_CORRUPT ){
				/* Error already reported by GenStateParseUnionTypeDecl */
				return SXERR_SYNTAX;
			}else if( rc == SXERR_SYNTAX ){
				if( pIn < pEnd ){
					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,
						"syntax error, unexpected token \"%z\", expecting variable",
						&pIn->sData);
				}else{
					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,
						"syntax error, unexpected end of file");
				}
				return SXERR_SYNTAX;
			}
			sArg.iFlags |= iTFlags;
		}
		if( pIn >= pEnd ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");
			return rc;
		}
		if( pIn->nType & PH7_TK_AMPER ){
			/* Pass by reference,record that */
			sArg.iFlags |= VM_FUNC_ARG_BY_REF;
			pIn++;
		}
		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){
			/* Variadic parameter: ...$args */
			sArg.iFlags |= VM_FUNC_ARG_VARIADIC;
			pIn++;
		}
		if( pIn >= pEnd || (pIn->nType & PH7_TK_DOLLAR) == 0 || &pIn[1] >= pEnd || (pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			/* Invalid argument */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");
			return rc;
		}
		pIn++; /* Jump the dollar sign */
		/* Copy argument name */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));
		pIn++;
		if( pIn < pEnd ){
			if( pIn->nType & PH7_TK_EQUAL ){
				SyToken *pDefend;
				sxi32 iNest = 0;
				pIn++; /* Jump the equal sign */
				pDefend = pIn;
				/* Process the default value associated with this argument */
				while( pDefend < pEnd ){
					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){
						break;
					}
					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/|PH7_TK_OCB/*'{'*/|PH7_TK_OSB/*[*/) ){
						/* Increment nesting level */
						iNest++;
					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/|PH7_TK_CCB/*'}'*/|PH7_TK_CSB/*]*/) ){
						/* Decrement nesting level */
						iNest--;
					}
					pDefend++;
				}
				if( pIn >= pDefend ){
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");
					return rc;
				}
				/* Process default value */
				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);
				if( rc != SXRET_OK ){
					return rc;
				}
				/* PHP rule: a typed parameter whose default is the literal `null`
				 * (`C $c = null`, `int $x = null`, `A|B $x = null`) is implicitly
				 * nullable — an explicit null is accepted even though the type isn't
				 * written `?T`. Detect the single-token `null` default here so the VM
				 * arg-type check lets null through. */
				if( (sArg.nType > 0 || (sArg.iFlags & VM_FUNC_ARG_UNION))
					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0
					&& &pIn[1] == pDefend
					&& pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)
					&& pIn->sData.nByte == sizeof("null")-1
					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){
					sArg.iFlags |= VM_FUNC_ARG_NULLABLE;
				}
				/* Point beyond the default value */
				pIn = pDefend;
			}
			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);
				return rc;
			}
			pIn++; /* Jump the trailing comma */
		}
		/* Append argument signature */
		if( sArg.nType > 0 ){
			if( SyStringLength(&sArg.sClass) > 0 ){
				/* Class name — prefix with 'o' so generic object hint is a prefix match */
				int marker = 'o';
				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));
				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));
			}else{
				int c;
				c = 'n'; /* cc warning */
				/* Type leading character */
				switch(sArg.nType){
				case MEMOBJ_HASHMAP:
					/* Hashmap aka 'array' */
					c = 'h';
					break;
				case MEMOBJ_INT:
					/* Integer */
					c = 'i';
					break;
				case MEMOBJ_BOOL:
					/* Bool */
					c = 'b';
					break;
				case MEMOBJ_REAL:
					/* Float */
					c = 'f';
					break;
				case MEMOBJ_STRING:
					/* String */
					c = 's';
					break;
				case MEMOBJ_OBJ:
					/* Object */
					c = 'o';
					break;
				default:
					break;
				}
				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));
			}
		}else{
			/* No type is associated with this parameter which mean
			 * that this function is not condidate for overloading.
			 */
			SyBlobRelease(&sSig);
		}
		/* Save in the argument set */
		SySetPut(&pFunc->aArgs,(const void *)&sArg);
	}
	if( SyBlobLength(&sSig) > 0 ){
		/* Save function signature */
		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));
	}
	return SXRET_OK;
}
/*
 * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested
 * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to
 * the enclosing function. Returns the token just past the nested construct.
 */
static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)
{
	sxi32 iParen = 0;
	pIn++; /* past 'function'/'fn' */
	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a
	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a
	 * ';' at paren-depth 0 (an abstract/interface method has no body). */
	while( pIn < pEnd ){
		sxu32 t = pIn->nType;
		if( t & PH7_TK_LPAREN ){ iParen++; }
		else if( t & PH7_TK_RPAREN ){ iParen--; }
		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }
		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }
		pIn++;
	}
	if( pIn >= pEnd ){ return pIn; }
	/* pIn at the body '{' — skip the balanced brace block. */
	{
		sxi32 d = 0;
		while( pIn < pEnd ){
			sxu32 t = pIn->nType;
			if( t & PH7_TK_OCB ){ d++; }
			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }
			pIn++;
		}
	}
	return pIn;
}
/*
 * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening
 * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a
 * generator)? Nested function/closure bodies are skipped so their yields don't count.
 * Used to gate inline try/catch/finally compilation: only generators need it (so a
 * `yield` inside a catch/finally can suspend); every other function keeps the legacy
 * detached-mini-program path untouched.
 */
/*
 * Case-insensitive match of a (possibly '\'-prefixed) name against the
 * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,
 * mixed, object.
 */
static int GenStateGenRetNameOk(const char *zName,sxu32 nName)
{
	static const struct { const char *zName; sxu32 nLen; } aOk[] = {
		{"Generator",9},{"Iterator",8},{"Traversable",11},
		{"iterable",8},{"mixed",5},{"object",6}
	};
	sxu32 i;
	if( nName > 0 && zName[0] == '\\' ){
		zName++;
		nName--;
	}
	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){
		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){
			return 1;
		}
	}
	return 0;
}
/*
 * One atom of a generator's declared return type: is it a supertype of
 * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,
 * mixed and object (nullability is irrelevant — it only widens). A class
 * atom is accepted when its raw name matches OR its use-import/namespace
 * resolution (GenStateResolveName) matches — so `use Generator as Gen;
 * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:
 * the parser strips a leading `\`, so inside `namespace Foo;` a
 * fully-qualified `\Generator` (php: accept) and a bare `Generator`
 * (php: reject as Foo\Generator) are indistinguishable here — we accept
 * both rather than fatal on valid code (a recorded divergence).
 */
static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)
{
	if( nType == MEMOBJ_OBJ ){
		return 1; /* bare `object` */
	}
	if( nType != SXU32_HIGH ){
		return 0; /* scalar/array/void/never/null/... */
	}
	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){
		return 1;
	}
	/* Not a whitelist name as written — try the compile-time resolution
	 * (use-import aliases; namespace prefix). `use Iterator as It;` must
	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,
	 * matching php (a subinterface is not a SUPERtype of Generator). */
	{
		SyBlob sFQN;
		int bOk;
		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
		GenStateResolveName(pGen,pName,&sFQN);
		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));
		SyBlobRelease(&sFQN);
		return bOk;
	}
}
/*
 * php 8: a generator function may only declare a return type that is a
 * supertype of Generator, alone or as a union alternative; an intersection
 * group qualifies only if every member does. Anything else is php's exact
 * compile-time fatal "Generator return type must be a supertype of
 * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the
 * canonical-order sReturnTypeName). Without this check the declared type
 * used to leak into the BODY's completion OP_DONE via the ctx resume paths
 * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).
 */
static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)
{
	int bOk = 0;
	sxu32 nLine;
	sxi32 rc;
	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){
		return SXRET_OK; /* untyped: nothing to validate */
	}
	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){
		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);
		sxu32 n = SySetUsed(&pFunc->aReturnUnion);
		sxu32 i,j;
		for( i = 0; i < n && !bOk; i++ ){
			int bGroupOk;
			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){
				continue; /* group already judged at its first member (ids are contiguous) */
			}
			bGroupOk = 1;
			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){
				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){
					bGroupOk = 0;
					break;
				}
			}
			bOk = bGroupOk;
		}
	}else{
		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);
	}
	if( bOk ){
		return SXRET_OK;
	}
	/* This validator runs at the end of GenStateCompileFuncBody, after the
	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a
	 * token of this stream — its line is the function's closing brace. php
	 * reports the SIGNATURE line instead; the drift is the §3.7 error-
	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */
	nLine = pGen->pIn[-1].nLine;
	{
		SyString sGiven = pFunc->sReturnTypeName;
		if( sGiven.nByte < 1 ){
			sGiven = pFunc->sReturnClass;
		}
		if( sGiven.nByte < 1 ){
			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the
			 * rendered type text, so sReturnTypeName arrives empty for them —
			 * name them here (the root fix belongs to that renderer, §3.7). */
			const char *zScalar =
				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :
				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";
			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));
		}
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Generator return type must be a supertype of Generator, %z given",&sGiven);
	}
	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
}
static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)
{
	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */
	SyToken *pEnd = pGen->pEnd;
	sxi32 iDepth = 0;
	int bStarted = 0;
	while( pIn < pEnd ){
		sxu32 t = pIn->nType;
		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }
		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }
		if( t & PH7_TK_KEYWORD ){
			int kw = SX_PTR_TO_INT(pIn->pUserData);
			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }
			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }
			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */
		}
		pIn++;
	}
	return FALSE;
}
/*
 * Compile function [i.e: standard function, annonymous function or closure ] body.
 * Return SXRET_OK on success. Any other return value indicates failure
 * and this routine takes care of generating the appropriate error message.
 */
static sxi32 GenStateCompileFuncBody(
	ph7_gen_state *pGen,  /* Code generator state */
	ph7_vm_func *pFunc    /* Function state */
	)
{
	SySet *pInstrContainer; /* Instruction container */
	GenBlock *pBlock;
	sxu32 nGotoOfft;
	sxi32 rc;
	/* Attach the new function */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	nGotoOfft = SySetUsed(&pGen->aGoto);
	/* Swap bytecode containers */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);
	/* Emit constructor property promotion prologue:
	 *   $this->NAME = $NAME;
	 * for each promoted parameter. Runtime typed-property store enforcement
	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */
	{
		sxu32 nArg = SySetUsed(&pFunc->aArgs);
		sxu32 i;
		for( i = 0; i < nArg; i++ ){
			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);
			char *zSrc;
			sxu32 nSrc,nName;
			SySet sToken;
			SyToken *pTmpIn,*pTmpEnd;
			sxi32 rcPromote;
			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){
				continue;
			}
			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.
			 * Tokens keep pointers into this buffer (identifier names are not
			 * copied), so it must outlive the function — never free it. The
			 * buffer is null-terminated because PH7_OP_LOAD reads the variable
			 * name via SyStrlen() on the token's sData pointer. */
			nName = SyStringLength(&pArg->sName);
			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;
			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);
			if( zSrc == 0 ){
				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
				GenStateLeaveBlock(&(*pGen),0);
				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			{
				char *z = zSrc;
				SyMemcpy("$this->",z,sizeof("$this->")-1);
				z += sizeof("$this->")-1;
				SyMemcpy(SyStringData(&pArg->sName),z,nName);
				z += nName;
				SyMemcpy(" = $",z,sizeof(" = $")-1);
				z += sizeof(" = $")-1;
				SyMemcpy(SyStringData(&pArg->sName),z,nName);
				z += nName;
				*z = 0;
			}
			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));
			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);
			pTmpIn = pGen->pIn;
			pTmpEnd = pGen->pEnd;
			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);
			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];
			rcPromote = PH7_CompileExpr(&(*pGen),0,0);
			pGen->pIn = pTmpIn;
			pGen->pEnd = pTmpEnd;
			SySetRelease(&sToken);
			if( rcPromote == SXERR_ABORT ){
				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
				GenStateLeaveBlock(&(*pGen),0);
				return SXERR_ABORT;
			}
			/* Discard the assignment result — this is a statement expression. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
	}
	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling
	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally
	 * suspends correctly). Saved/restored so a nested non-generator closure inside a
	 * generator — and vice versa — is classified independently. */
	{
		sxi8 bSavedGen = pGen->bInGenerator;
		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));
		/* Compile the body */
		PH7_CompileBlock(&(*pGen),0);
		pGen->bInGenerator = bSavedGen;
	}
	/* Fix exception jumps now the destination is resolved */
	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));
	/* Emit the final return if not yet done */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
	/* Fix gotos jumps now the destination is resolved */
	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){
		rc = SXERR_ABORT;
	}
	SySetTruncate(&pGen->aGoto,nGotoOfft);
	/* Restore the default container */
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	/* Leave function block */
	GenStateLeaveBlock(&(*pGen),0);
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	/* Scan for yield opcodes to detect generator functions */
	{
		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);
		sxu32 i;
		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){
			if( aInstr[i].iOp == PH7_OP_YIELD || aInstr[i].iOp == PH7_OP_YIELD_FROM ){
				pFunc->iFlags |= VM_FUNC_GENERATOR;
				break;
			}
		}
	}
	if( pFunc->iFlags & VM_FUNC_GENERATOR ){
		/* php-exact definition-time check; see the helper's block comment. */
		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){
			return SXERR_ABORT;
		}
	}
	/* All done, function body compiled */
	return SXRET_OK;
}
/*
 * Compile a PHP function whether is a Standard or Annonymous function.
 * According to the PHP language reference manual.
 *  Function names follow the same rules as other labels in PHP. A valid function name
 *  starts with a letter or underscore, followed by any number of letters, numbers, or
 *  underscores. As a regular expression, it would be expressed thus:
 *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.
 *  Functions need not be defined before they are referenced.
 *  All functions and classes in PHP have the global scope - they can be called outside
 *  a function even if they were defined inside and vice versa.
 *  It is possible to call recursive functions in PHP. However avoid recursive function/method
 *  calls with over 32-64 recursion levels.
 *
 * PH7 have introduced powerful extension including full type hinting, function overloading,
 * complex agrument values and more. Please refer to the official documentation for more information
 * on these extension.
 */
/*
 * Case-insensitive comparison for type names (PHP type names are case-insensitive).
 */
static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)
{
	sxu32 i;
	for( i = 0; i < n; i++ ){
		int a = zA[i], b = zB[i];
		if( a >= 'A' && a <= 'Z' ) a += 0x20;
		if( b >= 'A' && b <= 'Z' ) b += 0x20;
		if( a != b ) return a - b;
	}
	return 0;
}
/*
 * Internal type-atom kinds used during union type parsing.
 * Negative values are sentinels that never collide with MEMOBJ_* bitmasks
 * (which are positive bit values stored in sxu32).
 */
#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */
#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */
#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */

/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in
 * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array
 * below lives on the parser stack, so the cost is bounded: ~1 KiB. */

typedef struct PhlTypeAtom PhlTypeAtom;
struct PhlTypeAtom {
	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */
	SyString sClass;   /* class name when nType == SXU32_HIGH */
	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */
	sxu32 nCanon;
	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),
	                    * distinct groups are ORed; pure unions use one atom per group */
};

/*
 * Parse a single type atom (one alternative of a union, or a complete
 * single type). Recognises scalar keywords, `array`, `object`, `null`,
 * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).
 * pGen->pIn must point at the first token of the atom; on success it
 * is advanced past the atom. The previous nullable `?` prefix must
 * already be consumed by the caller.
 */
static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)
{
	SyToken *pIn = pGen->pIn;
	SyZero(pOut, sizeof(*pOut));
	SyStringInitFromBuf(&pOut->sClass, 0, 0);
	if( pIn >= pGen->pEnd ){
		return SXERR_SYNTAX;
	}
	/* Optional leading namespace separator '\' on FQN class types */
	if( pIn->nType & PH7_TK_NSSEP ){
		pIn++;
		if( pIn >= pGen->pEnd ){
			return SXERR_SYNTAX;
		}
	}
	if( (pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		return SXERR_SYNTAX;
	}
	if( pIn->nType & PH7_TK_KEYWORD ){
		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));
		if( nKey & PH7_TKWRD_ARRAY ){
			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;
		}else if( nKey & PH7_TKWRD_BOOL ){
			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;
		}else if( nKey & PH7_TKWRD_INT ){
			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;
		}else if( nKey & PH7_TKWRD_STRING ){
			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;
		}else if( nKey & PH7_TKWRD_FLOAT ){
			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;
		}else if( nKey & PH7_TKWRD_OBJECT ){
			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;
		}else if( nKey == PH7_TKWRD_SELF || nKey == PH7_TKWRD_PARENT
				|| nKey == PH7_TKWRD_STATIC ){
			pOut->nType = SXU32_HIGH;
			pOut->sClass = pIn->sData;
		}else{
			return SXERR_SYNTAX;
		}
		pIn++;
	}else{
		/* Identifier — `null`, `void`, `never`, or class name (possibly
		 * namespaced as a\b\c). Match the well-known names case-insensitively. */
		SyString *pT = &pIn->sData;
		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){
			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;
			pIn++;
		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){
			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;
			pIn++;
		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){
			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;
			pIn++;
		}else{
			/* Class / interface name; consume namespace path a\b\c */
			SyToken *pFirst = pIn;
			SyToken *pLast = pIn;
			pOut->nType = SXU32_HIGH;
			pOut->sClass = pIn->sData;
			pIn++;
			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)
				&& (pIn[1].nType & PH7_TK_ID) ){
				pLast = &pIn[1];
				pIn += 2;
			}
			if( pLast != pFirst ){
				const char *zFirst = pFirst->sData.zString;
				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;
				pOut->sClass.zString = zFirst;
				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);
			}
		}
	}
	pGen->pIn = pIn;
	return SXRET_OK;
}

/*
 * Build the canonical PHP-formatted type text into pBlob from a list of
 * atoms. Order matches PHP's `zend_type` rendering:
 *   classes (in declaration order) | object | array | string | int | float | bool [| null]
 * If exactly one non-null atom is present and bNullable is true, the
 * shorthand `?T` form is emitted instead of `T|null`.
 */
static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)
{
	int i;
	int nNonNull = 0;
	int bAnyIntersection = 0;
	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];
	sxu32 nMaxGroup = 0;
	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;
	for( i = 0; i < nAtoms; i++ ){
		if( aAtoms[i].nType != UTA_NULL_FLAG ){
			nNonNull++;
			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){
				aGroupCount[aAtoms[i].nGroup]++;
				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;
			}
		}
	}
	for( i = 0; i < nAtoms; i++ ){
		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){
			bAnyIntersection = 1;
			break;
		}
	}
	if( bAnyIntersection ){
		/* Intersection / DNF rendering, in declaration (group) order: each group's
		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the
		 * whole type has more than one group (so a standalone `A&B` stays bare). */
		sxu32 g, nGroups = 0;
		int bFirstGroup = 1;
		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }
		for( g = 0; g <= nMaxGroup; g++ ){
			int bFirstMember = 1;
			int bWrap;
			if( aGroupCount[g] == 0 ) continue;
			/* Wrap a ≥2-member group in `()` whenever it shares the type with any
			 * other alternative — another group OR a trailing `null` (which is not
			 * counted in nGroups). So `A&B` stays bare but `(A&B)|null` keeps its
			 * parens, matching PHP's canonical text. */
			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 || bNullable));
			if( !bFirstGroup ) SyBlobAppend(pBlob, "|", 1);
			if( bWrap ) SyBlobAppend(pBlob, "(", 1);
			for( i = 0; i < nAtoms; i++ ){
				if( aAtoms[i].nType == UTA_NULL_FLAG || aAtoms[i].nGroup != g ) continue;
				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);
				if( aAtoms[i].nType == SXU32_HIGH ){
					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);
				}else{
					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);
				}
				bFirstMember = 0;
			}
			if( bWrap ) SyBlobAppend(pBlob, ")", 1);
			bFirstGroup = 0;
		}
		if( bNullable ){
			SyBlobAppend(pBlob, "|", 1);
			SyBlobAppend(pBlob, "null", 4);
		}
		return;
	}
	if( nNonNull == 1 && bNullable ){
		/* Shorthand: ?T */
		for( i = 0; i < nAtoms; i++ ){
			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;
			SyBlobAppend(pBlob, "?", 1);
			if( aAtoms[i].nType == SXU32_HIGH ){
				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);
			}else{
				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);
			}
			return;
		}
	}
	{
		int bFirst = 1;
		/* 1) Classes in declaration order */
		for( i = 0; i < nAtoms; i++ ){
			if( aAtoms[i].nType == SXU32_HIGH ){
				if( !bFirst ) SyBlobAppend(pBlob, "|", 1);
				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);
				bFirst = 0;
			}
		}
		/* 2) Built-ins in canonical order */
		{
			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,
				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };
			int k;
			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){
				for( i = 0; i < nAtoms; i++ ){
					if( aAtoms[i].nType == aOrder[k] ){
						if( !bFirst ) SyBlobAppend(pBlob, "|", 1);
						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);
						bFirst = 0;
						break;
					}
				}
			}
		}
		/* 3) null suffix */
		if( bNullable ){
			if( !bFirst ) SyBlobAppend(pBlob, "|", 1);
			SyBlobAppend(pBlob, "null", 4);
		}
	}
}

/*
 * Parse one `|`-separated part of a type declaration into aAtoms[*pnAtoms..],
 * tagging each appended atom with group id iGroup. A part is one of:
 *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or
 *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.
 * On return *pnMembers is the number of atoms in this part and *pbParen records
 * whether it was parenthesized.
 *
 * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is
 * resolved by a one-token lookahead: `&` continues the intersection only when it
 * is followed by a type atom (namespace separator / identifier / keyword);
 * otherwise it belongs to a by-ref parameter marker and the part ends, leaving
 * the `&` for the caller (compile.c param loop) to consume.
 */
static sxi32 GenStateParsePart(
	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,
	int *pnMembers, int *pbParen, sxu32 nLine)
{
	sxi32 rc;
	int nMembers = 0;
	int bParen = 0;
	*pnMembers = 0;
	*pbParen = 0;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){
		bParen = 1;
		pGen->pIn++; /* skip '(' */
	}
	for(;;){
		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){
			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,
				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);
		if( rc != SXRET_OK ){
			return rc;
		}
		aAtoms[*pnAtoms].nGroup = iGroup;
		(*pnAtoms)++;
		nMembers++;
		/* Continue the intersection while `&` is followed by another type atom. */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){
			SyToken *pNext = &pGen->pIn[1];
			if( pNext < pGen->pEnd
			 && (pNext->nType & (PH7_TK_NSSEP|PH7_TK_ID|PH7_TK_KEYWORD)) ){
				pGen->pIn++; /* skip '&' */
				continue;
			}
		}
		break;
	}
	if( bParen ){
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){
			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,
				"Malformed DNF type: expecting ')'");
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
		pGen->pIn++; /* skip ')' */
		if( nMembers < 2 ){
			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,
				"Parenthesized type must be an intersection of at least two types");
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
	}
	*pnMembers = nMembers;
	*pbParen = bParen;
	return SXRET_OK;
}

/*
 * Parse an entire (possibly union) type declaration starting at pGen->pIn.
 *
 * Outputs:
 *   *pnType, *pClass — single-type fast path: filled when there is exactly
 *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or
 *     SXU32_HIGH for a class.  pClass receives the duplicated class name.
 *   *pAlts            — populated only when this is a true union (≥2
 *     non-null alternatives, OR ≥1 class+null union, etc). The set must
 *     already be initialized by the caller (allocator set, etc).
 *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE
 *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.
 *     The two flag values are passed in via iNullableFlag/iUnionFlag.
 *   *pTypeText        — duplicated canonical type text for error messages.
 *
 * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or
 * SXERR_ABORT on fatal compile errors.
 */
static sxi32 GenStateParseUnionTypeDecl(
	ph7_gen_state *pGen,
	sxu32 *pnType,
	SyString *pClass,
	SySet *pAlts,
	sxi32 *piTypeFlags,
	SyString *pTypeText,
	int iNullableFlag,
	int iUnionFlag,
	int bAllowVoid,
	sxu32 nLine
){
	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];
	int nAtoms = 0;
	int bShortNullable = 0;
	int bExplicitNull = 0;
	sxi32 rc;
	*pnType = 0;
	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);
	*piTypeFlags = 0;
	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);

	if( pGen->pIn >= pGen->pEnd ){
		return SXRET_OK;
	}
	/* Optional `?` shorthand prefix */
	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1
	 && pGen->pIn->sData.zString[0] == '?' ){
		bShortNullable = 1;
		pGen->pIn++;
		if( pGen->pIn >= pGen->pEnd ){
			return SXERR_SYNTAX;
		}
	}
	/* Parse the first part (a single atom, a bare top-level intersection, or a
	 * parenthesized DNF intersection), then any further `|`-separated parts. Each
	 * part is one OR-group; atoms within an intersection share the group id. */
	{
		int nMembers, bParen;
		sxu32 iGroup = 0;
		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Subsequent parts separated by `|`. A bare (unparenthesized) intersection
		 * is legal only as the sole part; once a `|` makes this a union every part
		 * must be a single type or a parenthesized intersection (`A&B|C` is invalid,
		 * write `(A&B)|C`). The loop-top check rejects a bare intersection followed
		 * by `|`; the after-loop check rejects one as the trailing part of a union. */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)
			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '|' ){
			if( bShortNullable ){
				/* Match PHP's wording — `?T|X` is rejected as a parse error.
				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error
				 * already reported" so callers skip their own error emission. */
				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,
					"syntax error, unexpected token \"|\", expecting variable");
				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;
			}
			if( nMembers >= 2 && !bParen ){
				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,
					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");
				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
			}
			pGen->pIn++; /* skip `|` */
			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
		if( iGroup > 0 && nMembers >= 2 && !bParen ){
			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,
				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
	}
	/* Validation pass.
	 *
	 * Order matters: the union-membership checks for void/never run *before*
	 * the duplicate scan, and `void` standalone-ness is checked *before* the
	 * `?void` check below — reordering them would let `?void` slip through.
	 */
	{
		int i, j;
		int bHasNonNull = 0;
		int bAnyIntersection = 0;
		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];
		/* Tally how many atoms each OR-group holds; a group of ≥2 is an
		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */
		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;
		for( i = 0; i < nAtoms; i++ ){
			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;
		}
		for( i = 0; i < nAtoms; i++ ){
			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }
		}
		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must
		 * be written `(A&B)|null` (handled by the explicit-null DNF path). */
		if( bShortNullable && bAnyIntersection ){
			PH7_GenCompileError(pGen, E_ERROR, nLine,
				"Nullable intersection types are not supported; use (A&B)|null instead");
			return SXERR_SYNTAX;
		}
		for( i = 0; i < nAtoms; i++ ){
			/* Intersection members must be class/interface types (PHP rejects
			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/
			 * `true`/`false` in an intersection). */
			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){
				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);
				if( bClassLike ){
					SyString *pC = &aAtoms[i].sClass;
					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)
					 || (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)
					 || (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)
					 || (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){
						bClassLike = 0;
					}
				}
				if( !bClassLike ){
					const char *zName; sxu32 nName;
					if( aAtoms[i].nType == SXU32_HIGH ){
						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;
					}else{
						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;
					}
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"Type %.*s cannot be part of an intersection type",
						(int)nName, zName);
					return SXERR_SYNTAX;
				}
			}
			if( aAtoms[i].nType == UTA_VOID_FLAG ){
				if( nAtoms > 1 ){
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"Void can only be used as a standalone type");
					return SXERR_SYNTAX;
				}
				if( !bAllowVoid ){
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"void cannot be used here");
					return SXERR_SYNTAX;
				}
				if( bShortNullable ){
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"Void type cannot be nullable");
					return SXERR_SYNTAX;
				}
			}
			if( aAtoms[i].nType == UTA_NEVER_FLAG ){
				/* `never` is a bottom type usable only as a standalone RETURN
				 * type (never = the function does not return). Mirrors the void
				 * validation above; accepted here and enforced at compile time
				 * (explicit `return` banned) and run time (fall-off TypeError). */
				if( nAtoms > 1 || bShortNullable ){
					/* `?never` is `never|null`, a union — PHP reports it the
					 * same as any other non-standalone use. */
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"never can only be used as a standalone type");
					return SXERR_SYNTAX;
				}
				if( !bAllowVoid ){
					/* Return-only: params call with bAllowVoid=0. */
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"never cannot be used as a parameter type");
					return SXERR_SYNTAX;
				}
			}
			if( aAtoms[i].nType == UTA_NULL_FLAG ){
				bExplicitNull = 1;
			}else{
				bHasNonNull = 1;
			}
			/* Duplicate detection. Flag a repeat only within the same group
			 * (intersection dup `A&A`) or between two singleton groups (union dup
			 * `int|int` / `A|A`); a class appearing in two distinct intersection
			 * groups (`(A&B)|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF
			 * subsumption — e.g. `(A&B)|A` — is deferred.) */
			for( j = 0; j < i; j++ ){
				int bDup = 0;
				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);
				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1
				                   && aGroupCount[aAtoms[j].nGroup] == 1);
				if( !bSameGroup && !bBothSingleton ) continue;
				if( aAtoms[i].nType == aAtoms[j].nType ){
					if( aAtoms[i].nType == SXU32_HIGH ){
						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte
						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,
								aAtoms[j].sClass.zString,
								aAtoms[i].sClass.nByte) == 0 ){
							bDup = 1;
						}
					}else{
						bDup = 1;
					}
				}
				if( bDup ){
					const char *zName;
					sxu32 nName;
					if( aAtoms[i].nType == SXU32_HIGH ){
						zName = aAtoms[i].sClass.zString;
						nName = aAtoms[i].sClass.nByte;
					}else{
						zName = aAtoms[i].zCanon;
						nName = aAtoms[i].nCanon;
					}
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"Duplicate type %.*s is redundant", (int)nName, zName);
					return SXERR_SYNTAX;
				}
			}
		}
		if( !bHasNonNull && bExplicitNull ){
			if( bShortNullable ){
				/* `?null` is not a valid type — PHP rejects the shorthand. */
				PH7_GenCompileError(pGen, E_ERROR, nLine,
					"Null can not be used as a standalone type");
				return SXERR_SYNTAX;
			}
			/* Bare `null` standalone type (PHP 8.2): represent it as the null
			 * type flag so enforcement accepts only null. The single-type fast
			 * path below leaves *pnType untouched when there is no non-null
			 * atom, so set it here. */
			*pnType = MEMOBJ_NULL;
		}
	}
	/* Compute nullability flag */
	if( bShortNullable || bExplicitNull ){
		*piTypeFlags |= iNullableFlag;
	}
	/* Build canonical type text */
	if( pTypeText ){
		SyBlob sBlob;
		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);
		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,
			(bShortNullable || bExplicitNull) ? 1 : 0);
		if( SyBlobLength(&sBlob) > 0 ){
			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));
			if( zDup ){
				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));
			}
		}
		SyBlobRelease(&sBlob);
	}
	/* Decide single-type vs union storage. A "union" is anything with more
	 * than one non-null atom, OR a single class atom + null. Single scalar
	 * + null collapses to the existing nullable single-type fast path. */
	{
		int nNonNull = 0;
		int iNonNullIdx = -1;
		int i;
		for( i = 0; i < nAtoms; i++ ){
			if( aAtoms[i].nType != UTA_NULL_FLAG ){
				nNonNull++;
				iNonNullIdx = i;
			}
		}
		if( nNonNull <= 1 ){
			/* Fast path: store as single type. */
			if( iNonNullIdx >= 0 ){
				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];
				if( pA->nType == SXU32_HIGH ){
					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
						pA->sClass.zString, pA->sClass.nByte);
					if( zDup == 0 ) return SXERR_ABORT;
					*pnType = SXU32_HIGH;
					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);
				}else if( pA->nType == UTA_VOID_FLAG ){
					*pnType = MEMOBJ_VOID;
				}else if( pA->nType == UTA_NEVER_FLAG ){
					*pnType = MEMOBJ_NEVER;
				}else{
					*pnType = pA->nType;
				}
			}
		}else{
			/* True union — populate the alts set, leave *pnType = 0. */
			*piTypeFlags |= iUnionFlag;
			for( i = 0; i < nAtoms; i++ ){
				ph7_type_alt sAlt;
				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;
				SyZero(&sAlt, sizeof(sAlt));
				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */
				if( aAtoms[i].nType == SXU32_HIGH ){
					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);
					if( zDup == 0 ) return SXERR_ABORT;
					sAlt.nType = SXU32_HIGH;
					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);
				}else{
					sAlt.nType = aAtoms[i].nType;
					SyStringInitFromBuf(&sAlt.sClass, 0, 0);
				}
				SySetPut(pAlts, (const void *)&sAlt);
			}
		}
	}
	return SXRET_OK;
}

/*
 * Parse a return type declaration (`: type`) after a function/method signature.
 * pGen->pIn should point to the token after `)`.
 * Sets pFunc->nReturnType and pFunc->sReturnClass.
 * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,
 *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,
 *          and union types `: T|U`.
 */
static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)
{
	sxi32 iFlags = 0;
	sxi32 rc;
	sxu32 nLine;
	pFunc->nReturnType = 0;
	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);
	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);
	/* Reset ALL declared-return-type state, not just the scalar fields: this
	 * parser can legitimately run twice for one closure (legacy pre-use colon
	 * position + the php post-use position). Leaving stale union alternatives
	 * or the nullable flag behind merges two declarations — enforcement then
	 * honored a wiped `: int|string` over the real `: bool`. */
	SySetReset(&pFunc->aReturnUnion);
	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_COLON) == 0 ){
		return SXRET_OK;
	}
	pGen->pIn++; /* Skip ':' */
	if( pGen->pIn >= pGen->pEnd ){
		return SXRET_OK;
	}
	nLine = pGen->pIn->nLine;
	rc = GenStateParseUnionTypeDecl(
		pGen,
		&pFunc->nReturnType,
		&pFunc->sReturnClass,
		&pFunc->aReturnUnion,
		&iFlags,
		&pFunc->sReturnTypeName,
		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored
		                          * in aReturnUnion, so the func carries it explicitly */
		/* iUnionFlag */ 0,
		/* bAllowVoid */ 1,
		nLine);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( rc == SXERR_CORRUPT ){
		/* Error already reported */
		return SXERR_SYNTAX;
	}
	if( rc == SXERR_SYNTAX ){
		if( pGen->pIn < pGen->pEnd ){
			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,
				"syntax error, unexpected token \"%z\" in return type declaration",
				&pGen->pIn->sData);
		}else{
			PH7_GenCompileError(pGen, E_PARSE, nLine,
				"syntax error, unexpected end of file in return type declaration");
		}
		return SXERR_SYNTAX;
	}
	pFunc->iFlags |= (iFlags & VM_FUNC_RETURN_NULLABLE);
	return SXRET_OK;
}

static sxi32 GenStateCompileFunc(
	ph7_gen_state *pGen, /* Code generator state */
	SyString *pName,     /* Function name. NULL otherwise */
	sxi32 iFlags,        /* Control flags */
	int bHandleClosure,  /* TRUE if we are dealing with a closure */
	ph7_vm_func **ppFunc /* OUT: function state */
	)
{
	ph7_vm_func *pFunc;
	SyToken *pEnd;
	sxu32 nLine;
	char *zName;
	sxi32 rc;
	/* Extract line number */
	nLine = pGen->pIn->nLine;
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	/* Delimit the function signature */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		pGen->pIn = pGen->pEnd;
		return SXRET_OK;
	}
	/* Create the function state */
	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));
	if( pFunc == 0 ){
		goto OutOfMem;
	}
	/* Build the function name, prepending namespace if active */
	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){
		SyBlob sFQN;
		sxu32 nLen;
		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
		SyBlobAppend(&sFQN,"\\",1);
		SyBlobAppend(&sFQN,pName->zString,pName->nByte);
		nLen = (sxu32)SyBlobLength(&sFQN);
		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);
		SyBlobRelease(&sFQN);
		if( zName == 0 ){
			goto OutOfMem;
		}
		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);
	}else{
		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
		if( zName == 0 ){
			goto OutOfMem;
		}
		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);
	}
	/* Fallback start line (the '(' token); callers that know the line of the
	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */
	pFunc->nLine = nLine;
	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( pGen->pIn < pEnd ){
		/* Collect function arguments */
		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
	}
	/* Point past ')' and parse optional return type ': type' */
	pGen->pIn = &pEnd[1];
	{
		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);
		if( rcRt == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rcRt == SXERR_SYNTAX ){
			return SXERR_SYNTAX;
		}
	}
	if( bHandleClosure ){
		ph7_vm_func_closure_env sEnv;
		int got_this = 0; /* TRUE if $this have been seen */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){
				sxu32 nLineLocal = pGen->pIn->nLine;
				/* Closure,record environment variable */
				pGen->pIn++;
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */
				/* Compile until we hit the first closing parenthesis */
				while( pGen->pIn < pGen->pEnd ){
					int iFlagsLocal = 0;
					if( pGen->pIn->nType & PH7_TK_RPAREN ){
						pGen->pIn++; /* Jump the closing parenthesis */
						break;
					}
					nLineLocal = pGen->pIn->nLine;
					if( pGen->pIn->nType & PH7_TK_AMPER ){
						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry
						 * to the variable's memory slot instead of copying its value. */
						iFlagsLocal = VM_FUNC_ARG_BY_REF;
						pGen->pIn++;
					}
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 || &pGen->pIn[1] >= pGen->pEnd
						|| (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,
								"Closure: Unexpected token. Expecting a variable name");
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
							/* Find the closing parenthesis */
							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){
								pGen->pIn++;
							}
							if(pGen->pIn < pGen->pEnd){
								pGen->pIn++;
							}
							break;
							/* TICKET 1433-95: No need for the else block below.*/
					}else{
						SyString *pNameLocal;
						char *zDup;
						/* Duplicate variable name */
						pNameLocal = &pGen->pIn[1].sData;
						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);
						if( zDup ){
							/* Zero the structure */
							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
							sEnv.iFlags = iFlagsLocal;
							sEnv.nIdx = SXU32_HIGH;
							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);
							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&
								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){
									got_this = 1;
							}
							/* Save imported variable */
							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);
						}else{
							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
							 return SXERR_ABORT;
						}
					}
					pGen->pIn += 2; /* $ + variable name or any other unexpected token */
					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){
						/* Ignore trailing commas */
						pGen->pIn++;
					}
				}
				/* php 7.1+: the return type follows the use clause —
				 * `function (...) use (...) : int {`. Gated on the colon:
				 * GenStateParseReturnType resets the type fields at entry,
				 * so an unconditional call would wipe a type parsed at the
				 * legacy pre-use position. */
				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){
					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);
					if( rcRt2 == SXERR_ABORT ){
						return SXERR_ABORT;
					}else if( rcRt2 == SXERR_SYNTAX ){
						return SXERR_SYNTAX;
					}
				}
		}
		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){
			/* Make the $this variable [Current processed Object (class instance)]
			 * available to the closure environment — for EVERY non-static
			 * anonymous function, use list or not (php binds $this to any
			 * closure declared in a method; pre-fix only `use (...)` closures
			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of
			 * a global-scope closure is silently dropped at install. A static
			 * closure never binds $this (php). */
			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */
			sEnv.nIdx = SXU32_HIGH;
			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);
			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);
		}
		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){
			/* Mark as closure */
			pFunc->iFlags |= VM_FUNC_CLOSURE;
		}
	}
	/* Compile the body */
	rc = GenStateCompileFuncBody(&(*pGen),pFunc);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* The cursor sits just past the body's closing brace */
	pFunc->nEndLine = pGen->pIn[-1].nLine;
	if( ppFunc ){
		*ppFunc = pFunc;
	}
	rc = SXRET_OK;
	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){
		/* Finally register the function */
		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);
	}
	if( rc == SXRET_OK ){
		return SXRET_OK;
	}
	/* Fall through if something goes wrong */
OutOfMem:
	/* If the supplied memory subsystem is so sick that we are unable to allocate
	 * a tiny chunk of memory, there is no much we can do here.
	 */
	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");
	return SXERR_ABORT;
}
/*
 * Compile a standard PHP function.
 *  Refer to the block-comment above for more information.
 */
static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)
{
	SyString *pName;
	sxi32 iFlags;
	sxu32 nKwLine;
	sxu32 nLine;
	sxi32 rc;

	nLine = pGen->pIn->nLine;
	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */
	pGen->pIn++; /* Jump the 'function' keyword */
	iFlags = 0;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){
		/* Return by reference,remember that */
		iFlags |= VM_FUNC_REF_RETURN;
		/* Jump the '&' token */
		pGen->pIn++;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		/* Invalid function name */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* Sychronize with the next semi-colon or braces*/
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	pName = &pGen->pIn->sData;
	nLine = pGen->pIn->nLine;
	/* Jump the function name */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		/* Sychronize with the next semi-colon or '{' */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Compile function body */
	{
		ph7_vm_func *pFuncState = 0;
		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);
		if( pFuncState ){
			/* Reflection getStartLine(): line of the 'function' keyword */
			pFuncState->nLine = nKwLine;
		}
	}
	return rc;
}
/*
 * Extract the visibility level associated with a given keyword.
 * According to the PHP language reference manual
 *  Visibility:
 *  The visibility of a property or method can be defined by prefixing
 *  the declaration with the keywords public, protected or private.
 *  Class members declared public can be accessed everywhere.
 *  Members declared protected can be accessed only within the class
 *  itself and by inherited and parent classes. Members declared as private
 *  may only be accessed by the class that defines the member.
 */
static sxi32 GetProtectionLevel(sxi32 nKeyword)
{
	if( nKeyword == PH7_TKWRD_PRIVATE ){
		return PH7_CLASS_PROT_PRIVATE;
	}else if( nKeyword == PH7_TKWRD_PROTECTED ){
		return PH7_CLASS_PROT_PROTECTED;
	}
	/* Assume public by default */
	return PH7_CLASS_PROT_PUBLIC;
}
/*
 * Compile a class constant.
 * According to the PHP language reference manual
 *  Class Constants
 *   It is possible to define constant values on a per-class basis remaining
 *   the same and unchangeable. Constants differ from normal variables in that
 *   you don't use the $ symbol to declare or use them.
 *   The value must be a constant expression, not (for example) a variable,
 *   a property, a result of a mathematical operation, or a function call.
 *   It's also possible for interfaces to have constants.
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the constant while
 *  the zend engine would allow only simple scalar value.
 *  Example:
 *   class Test{
 *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call
 *   };
 *   var_dump(TEST::MyConst);
 *   Refer to the official documentation for more information on the powerful extension
 *   introduced by the PH7 engine to the OO subsystem.
 */
/*
 * Decide whether a typed class constant (PHP 8.3) declares a type before its
 * name. The classic untyped form is `const NAME = value` — a single name-like
 * token immediately followed by '='. Anything else with a leading type token
 * (`const int X`, `const ?int X`, `const A|B X`, `const \Ns\Foo X`) declares a
 * type. We only commit to the type-parse when the shape is unambiguous so the
 * untyped path never runs (and never trips the type parser's diagnostics).
 */
static int GenStateClassConstHasType(ph7_gen_state *pGen)
{
	SyToken *p0, *p1;
	if( pGen->pIn >= pGen->pEnd ){
		return 0;
	}
	p0 = pGen->pIn;
	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */
	if( p0->nType & PH7_TK_NSSEP ){
		return 1;
	}
	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){
		return 1;
	}
	/* A name-like first token begins a type only when followed by another
	 * name (the constant name) or a union separator '|'. Followed by '=',
	 * ';' or ',' it is the constant name itself (untyped). */
	if( p0->nType & (PH7_TK_ID|PH7_TK_KEYWORD) ){
		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;
		if( p1 ){
			if( p1->nType & (PH7_TK_ID|PH7_TK_KEYWORD|PH7_TK_NSSEP) ){
				return 1;
			}
			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '|' ){
				return 1;
			}
		}
	}
	return 0;
}
/*
 * TRUE when the class-constant initializer starting at pGen->pIn is a bare real
 * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).
 * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a
 * whole-valued real MEMOBJ_REAL|MEMOBJ_INT, so the runtime flag test would wrongly
 * accept it as an int. The literal shape is the only reliable signal that separates
 * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).
 * Peek only; never consumes tokens.
 */
static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)
{
	SyToken *p = pGen->pIn;
	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1
		&& (p->sData.zString[0] == '-' || p->sData.zString[0] == '+') ){
		p++; /* skip leading unary sign(s) */
	}
	if( p >= pGen->pEnd || (p->nType & PH7_TK_REAL) == 0 ){
		return 0; /* not a real literal (int literal, cast, call, ...) */
	}
	p++;
	/* Must be the WHOLE initializer: the next token ends this constant. */
	return ( p >= pGen->pEnd || (p->nType & (PH7_TK_SEMI|PH7_TK_COMMA)) ) ? 1 : 0;
}
/*
 * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).
 * A `new` that immediately follows one of these is a member name (`A::new`,
 * `$o->new`), not a `new` expression.
 */
static int GenStateTokenIsMemberOp(const SyToken *p)
{
	sxi32 iOp;
	if( (p->nType & PH7_TK_OP) == 0 || p->pUserData == 0 ){
		return 0;
	}
	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;
	return ( iOp == EXPR_OP_DC || iOp == EXPR_OP_ARROW || iOp == EXPR_OP_NULLSAFE_ARROW );
}
/*
 * Return TRUE if the initializer starting at the current token contains a `new`
 * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,
 * interface-constant and (instance/static) property-default initializers
 * ("New expressions are not supported in this context") while still allowing it
 * in global constants, parameter defaults and static-local initializers (which
 * are compiled by different functions and left untouched). The scan is
 * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)
 * is still caught and an inner comma does not end the scan prematurely; only a
 * `,` / `;` at depth 0 terminates the initializer.
 *
 * A `new` inside a nested closure / arrow-function is NOT part of this constant
 * expression (it runs when the closure is later invoked), so PHP permits it — a
 * `static function(){ return new X(); }` is a valid constant expression. The scan
 * therefore skips over any `function`/`fn` construct rather than descending into
 * it. A `new` used as a member name (`A::new`) is likewise ignored.
 */
static int GenStateInitHasNewExpr(ph7_gen_state *pGen)
{
	SyToken *p = pGen->pIn;
	int iDepth = 0;
	while( p < pGen->pEnd ){
		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI|PH7_TK_COMMA)) ){
			break; /* end of this initializer */
		}
		if( (p->nType & PH7_TK_KEYWORD)
			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION
				|| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){
			/* Skip the whole closure/arrow-fn (signature defaults + body): any
			 * `new` in there is deferred to call time, not part of this const
			 * expression. */
			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );
			p++;
			if( bArrow ){
				/* fn(params) => expr : skip to the end of the current element (a
				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */
				int iBase = iDepth;
				while( p < pGen->pEnd ){
					if( p->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
						iDepth++;
					}else if( p->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
						if( iDepth <= iBase ){
							break; /* closes an enclosing group, not the fn's own */
						}
						iDepth--;
					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI|PH7_TK_COMMA)) ){
						break;
					}
					p++;
				}
			}else{
				/* function(params)[use(...)][: type] { body } : skip the signature
				 * up to the body '{' (a '{' at closure-local depth 0, so a
				 * `new class{}` default inside the parens is not mistaken for it),
				 * then skip the balanced brace block. */
				int iLocal = 0;
				while( p < pGen->pEnd ){
					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){
						break; /* body brace */
					}
					if( p->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
						iLocal++;
					}else if( p->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
						if( iLocal > 0 ){
							iLocal--;
						}
					}
					p++;
				}
				if( p < pGen->pEnd ){
					int iBrace = 0; /* p is on the body '{' */
					while( p < pGen->pEnd ){
						if( p->nType & PH7_TK_OCB ){
							iBrace++;
						}else if( p->nType & PH7_TK_CCB ){
							iBrace--;
							if( iBrace == 0 ){
								p++;
								break;
							}
						}
						p++;
					}
				}
			}
			continue;
		}
		if( p->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
			iDepth++;
		}else if( p->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
			if( iDepth > 0 ){
				iDepth--;
			}
		}else if( (p->nType & PH7_TK_OP) && p->pUserData
			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){
			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID|PH7_TK_OP)
			 * whose pUserData is the operator instance, not a keyword id. Ignore a
			 * `new` used as a member name (`A::new`/`$o->new`). */
			if( p == pGen->pIn || !GenStateTokenIsMemberOp(&p[-1]) ){
				return 1;
			}
		}
		p++;
	}
	return 0;
}
/*
 * Copy a parsed declared type onto a freshly created class attribute (property,
 * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come
 * straight from GenStateParseUnionTypeDecl; for a union the alternatives are
 * shared from pAlts — their class-name SyStrings are VM-allocator owned and
 * outlive the temporary set, so multiple attrs in a multi-declaration chain may
 * share the same backing.
 */
static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,
	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)
{
	pAttr->nType = nType;
	pAttr->sClass = *pClass;
	pAttr->sTypeName = *pTypeName;
	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){
		sxu32 i;
		for( i = 0; i < SySetUsed(pAlts); i++ ){
			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);
			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);
		}
	}
}
static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)
{
	sxu32 nLine = pGen->pIn->nLine;
	SySet *pInstrContainer;
	ph7_class_attr *pCons;
	SyString *pName;
	sxi32 rc;
	sxu32 nType = 0;
	SyString sTypeClass;
	SyString sTypeText;
	SySet aUnionAlts;
	sxi32 iTypeFlags = 0;
	SyStringInitFromBuf(&sTypeClass,0,0);
	SyStringInitFromBuf(&sTypeText,0,0);
	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));
	/* Extract visibility level */
	iProtection = GetProtectionLevel(iProtection);
	/* Mark as constant */
	iFlags |= PH7_CLASS_ATTR_CONSTANT;
	pGen->pIn++; /* Jump the 'const' keyword */
	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and
	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */
	if( GenStateClassConstHasType(pGen) ){
		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,
			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);
		/* On abort the whole compilation tears down and the VM allocator (which
		 * backs aUnionAlts) is released, so abort paths below don't free it —
		 * matching the rest of this function; only the recoverable Synchronize
		 * and success paths release. */
		if( rc == SXERR_CORRUPT ){
			/* Error already reported by GenStateParseUnionTypeDecl */
			goto Synchronize;
		}else if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXRET_OK ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Invalid type for class constant inside class '%z'",&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		iTypeFlags |= PH7_CLASS_ATTR_TYPED;
	}
loop:
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
		/* Invalid constant name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek constant name */
	pName = &pGen->pIn->sData;
	/* Make sure the constant name isn't reserved */
	if( GenStateIsReservedConstant(pName) ){
		/* Reserved constant name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */
	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){
		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,
			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,
			"Class constant %z::%z cannot have type %z",nLine);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXRET_OK ){
			goto Synchronize;
		}
	}
	/* Advance the stream cursor */
	pGen->pIn++;
	if(pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){
		/* Invalid declaration */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++; /* Jump the equal sign */
	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant
	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid
	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the
	 * literal shape here, at definition time, matching PHP's eager fatal. */
	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)
		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"Cannot use float as value for class constant %z::%z of type %z",
			&pClass->sName,pName,&sTypeText);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface
	 * constant initializer ("New expressions are not supported in this context").
	 * Reject it at definition time, matching PHP's compile-time fatal. */
	if( GenStateInitHasNewExpr(pGen) ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"New expressions are not supported in this context");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Allocate a new class attribute */
	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags|iTypeFlags);
	if( pCons ){
		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);
		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	if( pCons == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){
		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);
	}
	/* Swap bytecode container */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);
	/* Compile constant value.
	 */
	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);
	if( rc == SXERR_EMPTY ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	/* All done,install the constant */
	rc = PH7_ClassInstallAttr(pClass,pCons);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){
		/* Multiple constants declarations [i.e: const min=-1,max = 10] */
		pGen->pIn++; /* Jump the comma */
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){
				pTok--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Unexpected token '%z',expecting constant declaration inside class '%z'",
				&pTok->sData,&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}else{
			if( pGen->pIn->nType & PH7_TK_ID ){
				goto loop;
			}
		}
	}
	SySetRelease(&aUnionAlts);
	return SXRET_OK;
Synchronize:
	SySetRelease(&aUnionAlts);
	/* Synchronize with the first semi-colon */
	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){
		pGen->pIn++;
	}
	return SXERR_CORRUPT;
}
/*
 * complie a class attribute or Properties in the PHP jargon.
 * According to the PHP language reference manual
 *  Properties
 *  Class member variables are called "properties". You may also see them referred
 *  to using other terms such as "attributes" or "fields", but for the purposes
 *  of this reference we will use "properties". They are defined by using one
 *  of the keywords public, protected, or private, followed by a normal variable
 *  declaration. This declaration may include an initialization, but this initialization
 *  must be a constant value--that is, it must be able to be evaluated at compile time
 *  and must not depend on run-time information in order to be evaluated.
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the attribute while
 *  the zend engine would allow only simple scalar value.
 *  Example:
 *   class Test{
 *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call
 *   };
 *   var_dump(TEST::myVar);
 *   Refer to the official documentation for more information on the powerful extension
 *   introduced by the PH7 engine to the OO subsystem.
 */
/*
 * Lookahead: return TRUE if the tokens starting at pStart look like a typed
 * property declaration — i.e. an optional '?', optional '\', one or more
 * ID/keyword tokens (possibly separated by '\' for namespace paths), followed
 * by a '$'. This is used by the class-body dispatcher to decide whether to
 * route into the typed-attribute path vs. fall through to method/const/etc.
 */
static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)
{
	SyToken *p = pStart;
	int bFirst = 1;
	if( p >= pEnd ) return 0;
	/* Optional nullable `?` shorthand. */
	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){
		p++;
		if( p >= pEnd ) return 0;
	}
	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.
	 * One or more `|`-separated parts; each part is either a parenthesized
	 * intersection `( … )` or an atom optionally followed by a bare `&`
	 * intersection. We only need to land on the `$` to classify the member. */
	for(;;){
		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){
			/* Parenthesized DNF group — skip to the matching `)`. */
			p++;
			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }
			if( p >= pEnd ) return 0;
			p++; /* skip ')' */
		}else{
			/* A type atom: optional `\`, an identifier/keyword, namespace path,
			 * then any `&`-joined intersection members. */
			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }
			if( p >= pEnd || (p->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
				return 0;
			}
			/* Reject class-body modifier keywords that aren't types (only on the
			 * first atom; visibility is already consumed, but static/final/abstract
			 * may still appear at the initial dispatch site). */
			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){
				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));
				if( k == PH7_TKWRD_FUNCTION || k == PH7_TKWRD_VAR || k == PH7_TKWRD_CONST
				 || k == PH7_TKWRD_STATIC || k == PH7_TKWRD_FINAL || k == PH7_TKWRD_ABSTRACT ){
					return 0;
				}
			}
			p++;
			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) ){
				p += 2;
			}
			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)
				&& (p[1].nType & (PH7_TK_NSSEP|PH7_TK_ID|PH7_TK_KEYWORD)) ){
				p++; /* skip '&' */
				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }
				if( p >= pEnd || (p->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ) return 0;
				p++;
				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) ){
					p += 2;
				}
			}
		}
		bFirst = 0;
		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1
			&& p->sData.zString[0] == '|' ){
			p++; /* next `|`-separated part */
			continue;
		}
		break;
	}
	if( p >= pEnd ) return 0;
	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;
}

/*
 * Parse an optional property type hint starting at pGen->pIn. On return,
 * pGen->pIn points at the '$' token if a type was present (or is unchanged
 * if not). Recognized forms:
 *   ?Type, array, bool, int, float, string, object,
 *   self, parent, \Ns\ClassName, ClassName
 * The 'iterable' pseudo-type is not yet supported and is rejected earlier
 * by GenStateCompileClassAttr along with void/never/mixed/callable.
 * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX
 * on unrecoverable error.
 *
 * When a type is parsed:
 *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)
 *   *pClass is set to the class name (for class types)
 *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE
 *   *pTypeText is set to the original text span of the type
 * Otherwise they are left unchanged (so multi-decl reuse works).
 */
static sxi32 GenStateParsePropertyType(
	ph7_gen_state *pGen,
	sxu32 *pnType,
	SyString *pClass,
	sxi32 *piTypeFlags,
	SyString *pTypeText,
	SySet *pAlts
){
	sxi32 iFlags = 0;
	sxi32 rc;
	if( pGen->pIn >= pGen->pEnd ){
		return SXRET_OK;
	}
	/* If the first token is '$', there's no type */
	if( pGen->pIn->nType & PH7_TK_DOLLAR ){
		return SXRET_OK;
	}
	rc = GenStateParseUnionTypeDecl(
		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,
		PH7_CLASS_ATTR_NULLABLE,
		PH7_CLASS_ATTR_UNION,
		/* bAllowVoid */ 0,
		pGen->pIn->nLine);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Verify next token is '$' (start of property name) */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
		return SXERR_SYNTAX;
	}
	*piTypeFlags = iFlags | PH7_CLASS_ATTR_TYPED;
	return SXRET_OK;
}

/*
 * Return TRUE if a parsed type atom — identified by (nType, sClass) as
 * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP
 * forbids on properties. `callable`, `mixed`, and `iterable` are parsed
 * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they
 * are not recognized scalar keywords; `void` and `never` are rejected
 * by the type parser itself before reaching here.
 *
 * On TRUE, *pzName / *pnName point at a static canonical spelling for
 * use in the error message.
 */
static int GenStateIsDisallowedPropertyAtom(
	sxu32 nType,
	const SyString *pClass,
	const char **pzName,
	sxu32 *pnName)
{
	const char *z;
	sxu32 n;
	if( nType != SXU32_HIGH || pClass == 0 || pClass->nByte == 0 ){
		return 0;
	}
	z = pClass->zString;
	n = pClass->nByte;
	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){
		*pzName = "callable"; *pnName = 8; return 1;
	}
	/* `mixed` (any value) and `iterable` (= array|Traversable) are valid PHP
	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via
	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */
	return 0;
}

/*
 * Validate a parsed class-member type (property, promoted parameter or class
 * constant) — the main atom plus any union alternatives — against the
 * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string
 * taking three %z arguments (class name, member name, full canonical type text),
 * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have
 * type T" vs "Class constant C::X cannot have type T").
 *
 * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection
 * (error already emitted), or SXERR_ABORT on error-count overflow.
 */
static sxi32 GenStateValidateMemberType(
	ph7_gen_state *pGen,
	ph7_class *pClass,
	const SyString *pMemberName,
	sxu32 nType,
	const SyString *pTypeClass,
	const SyString *pTypeText,
	SySet *pUnionAlts,
	const char *zErrFmt,
	sxu32 nLine)
{
	const char *zBad = 0;
	sxu32 nBad = 0;
	SyString sFallback;
	const SyString *pBad;
	sxi32 rc;
	int bDisallowed = 0;
	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){
		bDisallowed = 1;
	}else if( pUnionAlts ){
		sxu32 i;
		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){
			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);
			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){
				bDisallowed = 1;
				break;
			}
		}
	}
	if( !bDisallowed ){
		return SXRET_OK;
	}
	/* Prefer the full canonical type text (PHP prints `callable|int` for
	 * a union, not just the offending atom). Fall back to the atom's own
	 * canonical spelling if the type text is unavailable. */
	if( pTypeText && SyStringLength(pTypeText) > 0 ){
		pBad = pTypeText;
	}else{
		SyStringInitFromBuf(&sFallback,zBad,nBad);
		pBad = &sFallback;
	}
	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
		zErrFmt,
		&pClass->sName,pMemberName,pBad);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXERR_SYNTAX;
}
/*
 * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not
 * reserve `readonly` (it remains valid as a method/function name), so it is
 * matched as a plain identifier in the class-member modifier position rather
 * than promoted to a lexer keyword.
 */
static int GenStateIsReadonly(SyToken *pTok)
{
	return (pTok->nType & PH7_TK_ID)
		&& pTok->sData.nByte == sizeof("readonly")-1
		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;
}
/*
 * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)`
 * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id
 * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present
 * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).
 */
static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)
{
	*pnTok = 0;
	if( &pTok[3] < pEnd
	 && (pTok->nType & PH7_TK_KEYWORD)
	 && (pTok[1].nType & PH7_TK_LPAREN)
	 && (pTok[2].nType & (PH7_TK_ID|PH7_TK_KEYWORD))
	 && pTok[2].sData.nByte == sizeof("set")-1
	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0
	 && (pTok[3].nType & PH7_TK_RPAREN) ){
		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);
		if( nKw == PH7_TKWRD_PUBLIC || nKw == PH7_TKWRD_PRIVATE || nKw == PH7_TKWRD_PROTECTED ){
			*pnTok = 4;
			return nKw;
		}
	}
	return 0;
}
/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */
static sxi32 GenStateSetVisFlag(sxi32 nKw)
{
	if( nKw == PH7_TKWRD_PRIVATE ){
		return PH7_CLASS_ATTR_PRIVATE_SET;
	}
	if( nKw == PH7_TKWRD_PROTECTED ){
		return PH7_CLASS_ATTR_PROTECTED_SET;
	}
	return PH7_CLASS_ATTR_PUBLIC_SET;
}
static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class_attr *pAttr;
	SyString *pName;
	sxi32 rc;
	sxu32 nType = 0;
	SyString sTypeClass;
	SyString sTypeText;
	SySet aUnionAlts;
	sxi32 iTypeFlags = 0;
	SyStringInitFromBuf(&sTypeClass,0,0);
	SyStringInitFromBuf(&sTypeText,0,0);
	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));
	/* In a readonly class (PHP 8.2) every declared instance property is readonly;
	 * the per-property readonly rules below then apply uniformly (a static or
	 * untyped property, or one with a default, raises the same PHP-exact fatal). */
	if( pClass->iFlags & PH7_CLASS_READONLY ){
		iFlags |= PH7_CLASS_ATTR_READONLY;
	}
	/* Extract visibility level */
	iProtection = GetProtectionLevel(iProtection);
	/* Parse optional type hint (typed properties, PHP 7.4+) */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);
		if( rc == SXERR_CORRUPT ){
			/* Error already reported by GenStateParseUnionTypeDecl */
			goto Synchronize;
		}else if( rc == SXERR_SYNTAX ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Invalid property type or declaration near '%z'",
				&pGen->pIn->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}else if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
loop:
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++; /* Jump the dollar sign */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_ID)) == 0 ){
		/* Invalid attribute name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek attribute name */
	pName = &pGen->pIn->sData;
	/* Advance the stream cursor */
	pGen->pIn++;
	if(pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/|PH7_TK_SEMI/*';'*/|PH7_TK_COMMA/*','*/)) == 0 ){
		/* Invalid declaration */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and
	 * the read visibility must not be narrower than the set visibility. */
	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET|PH7_CLASS_ATTR_PROTECTED_SET|PH7_CLASS_ATTR_PUBLIC_SET) ){
		const char *zAvErr = 0;
		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE
			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED
			: PH7_CLASS_PROT_PUBLIC;
		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
			zAvErr = "Property with asymmetric visibility %z::$%z must have type";
		}else if( iProtection > iSetLevel ){
			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";
		}
		if( zAvErr ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
	}
	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and
	 * cannot carry a default value. PHP-exact diagnostics. */
	if( iFlags & PH7_CLASS_ATTR_READONLY ){
		const char *zRoErr = 0;
		if( iFlags & PH7_CLASS_ATTR_STATIC ){
			zRoErr = "Static property %z::$%z cannot be readonly";
		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
			zRoErr = "Readonly property %z::$%z must have type";
		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){
			zRoErr = "Readonly property %z::$%z cannot have default value";
		}
		if( zRoErr ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
	}
	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main
	 * type atom or any union alternative. void/never are already rejected
	 * by the type parser. */
	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){
		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,
			&sTypeText,
			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,
			"Property %z::$%z cannot have type %z",nLine);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXRET_OK ){
			goto Synchronize;
		}
	}
	/* Reject redeclaration (catches clash with an earlier promoted property). */
	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"Cannot redeclare %z::$%z",&pClass->sName,pName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default
	 * initializer ("New expressions are not supported in this context"). Reject it
	 * here, before allocating the attribute, matching PHP's compile-time fatal and
	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips
	 * it and reads the initializer non-destructively); no '=' means no default, so
	 * the helper stops at the ';'/',' and returns 0. */
	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"New expressions are not supported in this context");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Allocate a new class attribute */
	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags|iTypeFlags);
	if( pAttr ){
		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);
		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	if( pAttr == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){
		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);
	}
	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){
		SySet *pInstrContainer;
		pGen->pIn++; /*Jump the equal sign */
		/* Swap bytecode container */
		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);
		/* Compile attribute value.
		 */
		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);
		if( rc == SXERR_EMPTY ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		/* Emit the done instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	}
	/* All done,install the attribute */
	rc = PH7_ClassInstallAttr(pClass,pAttr);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){
		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */
		pGen->pIn++; /* Jump the comma */
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){
				pTok--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Unexpected token '%z',expecting attribute declaration inside class '%z'",
				&pTok->sData,&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}else{
			if( pGen->pIn->nType & PH7_TK_DOLLAR ){
				goto loop;
			}
		}
	}
	SySetRelease(&aUnionAlts);
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){
		pGen->pIn++;
	}
	SySetRelease(&aUnionAlts);
	return SXERR_CORRUPT;
}
/*
 * Compile a class method.
 *
 * Refer to the official documentation for more information
 * on the powerful extension introduced by the PH7 engine
 * to the OO subsystem such as full type hinting,method
 * overloading and many more.
 */
static sxi32 GenStateCompileClassMethod(
	ph7_gen_state *pGen, /* Code generator state */
	sxi32 iProtection,   /* Visibility level */
	sxi32 iFlags,        /* Configuration flags */
	int doBody,          /* TRUE to process method body */
	ph7_class *pClass    /* Class this method belongs */
	)
{
	sxu32 nLine = pGen->pIn->nLine;
	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */
	ph7_class_method *pMeth;
	sxi32 iFuncFlags;
	SyString *pName;
	SyToken *pEnd;
	sxi32 rc;
	/* Extract visibility level */
	iProtection = GetProtectionLevel(iProtection);
	pGen->pIn++; /* Jump the 'function' keyword */
	iFuncFlags = 0;
	if( pGen->pIn >= pGen->pEnd ){
		/* Invalid method name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){
		/* Return by reference,remember that */
		iFuncFlags |= VM_FUNC_REF_RETURN;
		/* Jump the '&' token */
		pGen->pIn++;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		/* Invalid method name */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek method name */
	pName = &pGen->pIn->sData;
	nLine = pGen->pIn->nLine;
	/* Jump the method name */
	pGen->pIn++;
	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){
		/* Abstract method */
		if( iProtection == PH7_CLASS_PROT_PRIVATE ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Access type for abstract method '%z::%z' cannot be 'private'",
				&pClass->sName,pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		/* Assemble method signature only */
		doBody = FALSE;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Allocate a new class_method instance */
	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);
	if( pMeth == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	pMeth->sFunc.nLine = nKwLine;
	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	pEnd = 0; /* cc warning */
	/* Delimit the method signature */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	{
		int bIsCtor = 0;
		int bAbstractCtor = 0;
		if( (pName->nByte == sizeof("__construct") - 1
				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)
		 || SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){
			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){
				bAbstractCtor = 1;
			}else{
				bIsCtor = 1;
			}
		}
		if( pGen->pIn < pEnd ){
			/* Collect method arguments */
			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
	}
	/* Point past ')' and parse optional return type ': type' */
	pGen->pIn = &pEnd[1];
	{
		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);
		if( rcRt == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rcRt == SXERR_SYNTAX ){
			goto Synchronize;
		}
	}
	/* Install promoted constructor properties as class attributes. Runtime
	 * property init/typecheck is handled by the generic typed-property path
	 * since we mint real ph7_class_attr entries. */
	{
		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);
		sxu32 i;
		for( i = 0; i < nArg; i++ ){
			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);
			ph7_class_attr *pAttr;
			sxi32 iAttrFlags = 0;
			int bArgTyped;
			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){
				continue;
			}
			/* "typed" = a single type or class name, OR a union/intersection,
			 * which leaves nType=0 / empty sClass and stores its alts in
			 * aUnionAlts. Used both to validate the type and to mark the attr. */
			bArgTyped = pArg->nType > 0 || SyStringLength(&pArg->sClass) > 0
			         || (pArg->iFlags & VM_FUNC_ARG_UNION);
			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"Cannot declare variadic promoted property");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto Synchronize;
			}
			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)
			 * that GenStateCompileClassAttr rejects — including when they
			 * appear as an alternative of a union type. */
			if( bArgTyped ){
				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,
					pArg->nType,&pArg->sClass,&pArg->sTypeName,
					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,
					"Property %z::$%z cannot have type %z",nLine);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}else if( rc != SXRET_OK ){
					goto Synchronize;
				}
			}
			/* Reject duplicate property (explicit property declared earlier with same name). */
			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto Synchronize;
			}
			if( bArgTyped ){
				iAttrFlags |= PH7_CLASS_ATTR_TYPED;
			}
			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){
				iAttrFlags |= PH7_CLASS_ATTR_NULLABLE;
			}
			if( pArg->iFlags & VM_FUNC_ARG_UNION ){
				iAttrFlags |= PH7_CLASS_ATTR_UNION;
			}
			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) || (pClass->iFlags & PH7_CLASS_READONLY) ){
				/* A readonly promoted property must be typed (PHP 8.1); in a
				 * readonly class (8.2) every promoted property is readonly too. */
				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto Synchronize;
				}
				iAttrFlags |= PH7_CLASS_ATTR_READONLY;
			}
			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET|VM_FUNC_ARG_PROT_SET) ){
				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */
				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Property with asymmetric visibility %z::$%z must have type",
						&pClass->sName,&pArg->sName);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto Synchronize;
				}
				iAttrFlags |= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)
					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;
			}
			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);
			if( pAttr == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
				return SXERR_ABORT;
			}
			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){
				pAttr->nType = pArg->nType;
				pAttr->sClass = pArg->sClass;
				pAttr->sTypeName = pArg->sTypeName;
				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){
					sxu32 k;
					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){
						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);
						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);
					}
				}
			}
			rc = PH7_ClassInstallAttr(pClass,pAttr);
			if( rc != SXRET_OK ){
				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
				return SXERR_ABORT;
			}
		}
	}
	if( doBody ){
		/* Compile method body */
		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* The cursor sits just past the body's closing brace */
		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;
	}else{
		/* Abstract/interface method: declaration ends at the ';' */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){
			pMeth->sFunc.nEndLine = pGen->pIn->nLine;
		}
		/* Only method signature is allowed */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Expected ';' after method signature '%z'",pName);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
				return SXERR_CORRUPT;
			}
	}
	/* All done,install the method */
	rc = PH7_ClassInstallMethod(pClass,pMeth);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){
		pGen->pIn++;
	}
	return SXERR_CORRUPT;
}
/*
 * Compile an object interface.
 *  According to the PHP language reference manual
 *   Object Interfaces:
 *   Object interfaces allow you to create code which specifies which methods
 *   a class must implement, without having to define how these methods are handled.
 *   Interfaces are defined using the interface keyword, in the same way as a standard
 *   class, but without any of the methods having their contents defined.
 *   All methods declared in an interface must be public, this is the nature of an interface.
 */
static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class *pClass,*pBase;
	SyToken *pEnd,*pTmp;
	SyString *pName;
	sxi32 nKwrd;
	sxi32 rc;
	/* Jump the 'interface' keyword */
	pGen->pIn++;
	/* Extract interface name */
	pName = &pGen->pIn->sData;
	/* Advance the stream cursor */
	pGen->pIn++;
	/* Build FQN and obtain a raw class */ {
		SyBlob sFQN;
		SyString sFQNStr;
		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
		GenStateBuildFQN(pGen,pName,&sFQN);
		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));
		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);
		SyBlobRelease(&sFQN);
	}
	if( pClass == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */
	pClass->iFlags |= PH7_CLASS_INTERFACE;
	/* Assume no base class is given */
	pBase = 0;
	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){
			SyBlob sResolved;
			SyString sBaseName;
			sxu32 nRefLine;
			/* Extract base interface */
			pGen->pIn++;
			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;
			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
				SyBlobRelease(&sResolved);
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",
					pName);
				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			pBase = PH7_VmExtractClass(pGen->pVm,
				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);
			SyStringInitFromBuf(&sBaseName,
				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
			/* Only interfaces is allowed */
			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){
				pBase = pBase->pNextName;
			}
			if( pBase == 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,
					"Nonexistent base interface '%z'",&sBaseName);
				if( rc == SXERR_ABORT ){
					SyBlobRelease(&sResolved);
					return SXERR_ABORT;
				}
			}
			SyBlobRelease(&sResolved);
		}
	}
	if( pGen->pIn >= pGen->pEnd  || (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pGen->pIn++; /* Jump the leading curly brace */
	pEnd = 0; /* cc warning */
	/* Delimit the interface body */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* The delimiter token is the interface body's closing brace */
	pClass->nEndLine = pEnd->nLine;
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Start the parse process
	 * Note (According to the PHP reference manual):
	 *  Only constants and function signatures(without body) are allowed.
	 *  Only 'public' visibility is allowed.
	 */
	for(;;){
		/* Jump leading/trailing semi-colons */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){
			pGen->pIn++;
		}
		if( pGen->pIn >= pGen->pEnd ){
			/* End of interface body */
			break;
		}
		/* Bind a directly-preceding docblock to this member */
		GenStateSetPendingDoc(&(*pGen));
		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",
				&pGen->pIn->sData,pName);
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto done;
		}
		/* Extract the current keyword */
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).
			 * Peek ahead to distinguish constant vs method and extract the member name. */
			const char *zKind = "member";
			SyString *pMemberName = 0;
			if( (pGen->pIn + 1) < pGen->pEnd ){
				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);
				if( nNext == PH7_TKWRD_CONST ){
					zKind = "constant";
					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){
						pMemberName = &(pGen->pIn + 2)->sData;
					}
				}else if( nNext == PH7_TKWRD_FUNCTION ){
					zKind = "method";
					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){
						pMemberName = &(pGen->pIn + 2)->sData;
					}
				}
			}
			if( pMemberName ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);
			}else{
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
					"Access type for interface %s must be public",zKind);
			}
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto done;
		}
		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Expecting method signature or constant declaration inside interface '%z'",pName);
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto done;
		}
		if( nKwrd == PH7_TKWRD_PUBLIC ){
			/* Advance the stream cursor */
			pGen->pIn++;
			if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
					"Expecting method signature inside interface '%z'",pName);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
				goto done;
			}
			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
					"Expecting method signature or constant declaration inside interface '%z'",pName);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
				goto done;
			}
		}
		if( nKwrd == PH7_TKWRD_CONST ){
			/* Parse constant */
			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);
			if( rc != SXRET_OK ){
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto done;
			}
		}else{
			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */
			if( nKwrd == PH7_TKWRD_STATIC ){
				/* Static method,record that */
				iFlags |= PH7_CLASS_ATTR_STATIC;
				/* Advance the stream cursor */
				pGen->pIn++;
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0
					|| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Expecting method signature inside interface '%z'",pName);
						if( rc == SXERR_ABORT ){
							/* Error count limit reached,abort immediately */
							return SXERR_ABORT;
						}
						goto done;
				}
			}
			/* Process method signature (no body for interface methods) */
			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);
			if( rc != SXRET_OK ){
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto done;
			}
		}
	}
	/* Install the interface */
	rc = PH7_VmInstallClass(pGen->pVm,pClass);
	if( rc == SXRET_OK && pBase ){
		/* Inherit from the base interface */
		rc = PH7_ClassInterfaceInherit(pClass,pBase);
	}
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
done:
	/* Point beyond the interface body */
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	return PH7_OK;
}
/*
 * Compile a user-defined class.
 * According to the PHP language reference manual
 *  class
 *  Basic class definitions begin with the keyword class, followed by a class
 *  name, followed by a pair of curly braces which enclose the definitions
 *  of the properties and methods belonging to the class.
 *  The class name can be any valid label which is a not a PHP reserved word.
 *  A valid class name starts with a letter or underscore, followed by any number
 *  of letters, numbers, or underscores. As a regular expression, it would be expressed
 *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.
 *  A class may contain its own constants, variables (called "properties"), and functions
 *  (called "methods").
 */
/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */
typedef struct TraitUseEntry TraitUseEntry;
struct TraitUseEntry {
	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */
	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */
	SyToken *pResolvEnd;       /* End of resolution block tokens */
};
/*
 * Validate that methods implementing interface contracts have compatible
 * signatures: public visibility and at least as many parameters as declared.
 */
static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)
{
	ph7_class **apIface;
	sxu32 nIface,i;
	sxi32 rc;
	if( pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT) ){
		return SXRET_OK;
	}
	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);
	nIface = SySetUsed(&pClass->aInterface);
	for(i = 0; i < nIface; i++){
		ph7_class *pIface = apIface[i];
		SyHashEntry *pEntry;
		SyHashResetLoopCursor(&pIface->hMethod);
		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){
			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;
			ph7_class_method *pImplMeth;
			SyString *pMName = &pIfaceMeth->sFunc.sName;
			/* Find the implementing method in the class */
			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);
			if( pImplMeth == 0 || (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){
				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */
			}
			/* Check visibility: interface methods must be implemented as public */
			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){
				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,
					"Access level to %z::%z() must be public (as in class %z)",
					&pClass->sName,pMName,&pIface->sName);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			/* Check parameter compatibility: implementation must accept at least as many
			 * required parameters. Extra parameters are allowed only if they have defaults.
			 */
			{
				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);
				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);
				int sigError = 0;
				if( nImplArgs < nIfaceArgs ){
					sigError = 1;
				}else if( nImplArgs > nIfaceArgs ){
					/* Extra parameters must all have default values */
					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);
					sxu32 k;
					for(k = nIfaceArgs; k < nImplArgs; k++){
						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){
							sigError = 1;
							break;
						}
					}
				}
				if( sigError ){
					SyBlob sImplSig, sIfaceSig;
					ph7_vm_func_arg *aArgs;
					sxu32 j;
					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);
					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);
					/* Build implementing method signature */
					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);
					for(j = 0; j < nImplArgs; j++){
						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);
						SyBlobAppend(&sImplSig,"$",1);
						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);
					}
					/* Build interface method signature */
					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);
					for(j = 0; j < nIfaceArgs; j++){
						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);
						SyBlobAppend(&sIfaceSig,"$",1);
						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);
					}
					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,
						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",
						&pClass->sName,pMName,
						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),
						&pIface->sName,pMName,
						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));
					SyBlobRelease(&sImplSig);
					SyBlobRelease(&sIfaceSig);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
			}
		}
	}
	return SXRET_OK;
}
/*
 * Check that a concrete class has no remaining abstract methods.
 * If it does, emit a PHP-compatible fatal error listing them all.
 */
static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)
{
	ph7_class_method *pMeth;
	SyHashEntry *pEntry;
	sxu32 nAbstract;
	SyBlob sMsg;
	sxi32 rc;
	/* Abstract classes, interfaces, and traits may have unimplemented methods */
	if( pClass->iFlags & (PH7_CLASS_ABSTRACT|PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT) ){
		return SXRET_OK;
	}
	/* Count abstract methods */
	nAbstract = 0;
	SyHashResetLoopCursor(&pClass->hMethod);
	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){
		pMeth = (ph7_class_method *)pEntry->pUserData;
		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){
			nAbstract++;
		}
	}
	if( nAbstract == 0 ){
		return SXRET_OK;
	}
	/* Build the error message listing all abstract methods with origins */
	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);
	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "
		"be declared abstract or implement the remaining method%s (",
		&pClass->sName,nAbstract,
		(nAbstract > 1 ? "s" : ""),
		(nAbstract > 1 ? "s" : ""));
	/* Second pass: list methods with origins */
	{
		sxu32 nListed = 0;
		SyHashResetLoopCursor(&pClass->hMethod);
		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){
			ph7_class *pOrigin = 0;
			SyString *pMName;
			pMeth = (ph7_class_method *)pEntry->pUserData;
			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){
				continue;
			}
			pMName = &pMeth->sFunc.sName;
			if( nListed > 0 ){
				SyBlobAppend(&sMsg,", ",2);
			}
			/* Find the origin of this abstract method.
			 * PHP priority: interfaces (walking ancestors and interface
			 * inheritance chains) take precedence for interface-declared
			 * methods. Abstract class methods only win when the class
			 * itself declared the abstract method (not inherited from
			 * an interface). Trait methods are adopted into the using
			 * class's namespace.
			 */
			{
				ph7_class **apIface;
				ph7_class **apTrait;
				ph7_class *pWalk;
				sxu32 i;
				/* 1. Check parent chain for a natively-declared abstract method
				 * (one that was written in the class body, not inherited from an
				 * interface). PHP attributes origin to the declaring class.
				 */
				if( pClass->pBase ){
					pWalk = pClass->pBase;
					while( pWalk ){
						ph7_class_method *pParentMeth;
						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);
						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){
							/* Exclude methods that came from an interface anywhere
							 * in this class's ancestor chain.
							 */
							int fromIface = 0;
							ph7_class *pAnc = pWalk;
							while( pAnc ){
								ph7_class **apPI;
								sxu32 j;
								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);
								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){
									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){
										fromIface = 1;
										break;
									}
								}
								if( fromIface ) break;
								pAnc = pAnc->pBase;
							}
							if( !fromIface ){
								pOrigin = pWalk;
								break;
							}
						}
						pWalk = pWalk->pBase;
					}
				}
				/* 2. Check interfaces on class and all ancestors, walking
				 * each interface's own parent chain for the deepest origin.
				 */
				if( !pOrigin ){
					pWalk = pClass;
					while( pWalk && !pOrigin ){
						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);
						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){
							ph7_class *pIface = apIface[i];
							ph7_class *pDeepest = 0;
							while( pIface ){
								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){
									pDeepest = pIface;
								}
								pIface = pIface->pBase;
							}
							if( pDeepest ){
								pOrigin = pDeepest;
								break;
							}
						}
						pWalk = pWalk->pBase;
					}
				}
				/* 3. Trait methods are adopted into the class namespace in PHP */
				if( !pOrigin ){
					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);
					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){
						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){
							pOrigin = pClass;
							break;
						}
					}
				}
			}
			if( pOrigin ){
				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);
			}else{
				/* Origin is the class itself (trait method adopted into class namespace) */
				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);
			}
			nListed++;
		}
	}
	SyBlobAppend(&sMsg,")",1);
	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",
		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));
	SyBlobRelease(&sMsg);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXRET_OK;
}
/*
 * Parse a class/interface name reference from the current token stream.
 * Handles an optional leading '\' (absolute) and multi-segment namespaced
 * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn
 * (which must be an initialized, empty SyBlob) and advances pGen->pIn past
 * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if
 * the stream has no valid name at the current position (pGen->pIn is left
 * untouched in that case so the caller can produce its own diagnostic).
 */
static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)
{
	int isAbsolute = 0;
	SyToken *pStart = pGen->pIn;
	SyBlob sName;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){
		isAbsolute = 1;
		pGen->pIn++;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		pGen->pIn = pStart;
		return SXERR_INVALID;
	}
	SyBlobInit(&sName,&pGen->pVm->sAllocator);
	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);
	pGen->pIn++;
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&
		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) ){
		SyBlobAppend(&sName,"\\",1);
		pGen->pIn++;
		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);
		pGen->pIn++;
	}
	if( isAbsolute ){
		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));
	}else{
		SyString sRaw;
		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));
		GenStateResolveName(pGen,&sRaw,pFqn);
	}
	SyBlobRelease(&sName);
	return SXRET_OK;
}
/*
 * Return TRUE if pInterface is Throwable or transitively extends Throwable.
 * Walks both the interface `extends` chain (pBase) and any parent-interface
 * set (aInterface). Depth is counted for every traversal step — recursion
 * through aInterface *and* sibling iteration through pBase — so a cycle in
 * either direction cannot run unbounded.
 */
#define PH7_THROWABLE_WALK_MAX_DEPTH 64
static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)
{
	ph7_class **apParent;
	sxu32 n;
	while( pInterface ){
		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){
			return FALSE;
		}
		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&
			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){
			return TRUE;
		}
		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);
		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){
			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){
				return TRUE;
			}
		}
		pInterface = pInterface->pBase;
		iDepth++;
	}
	return FALSE;
}
static int GenStateInterfaceIsThrowable(ph7_class *pInterface)
{
	return GenStateInterfaceIsThrowableAt(pInterface,0);
}
/*
 * Return TRUE if pBase is (or transitively extends) the Exception or Error
 * base class. Used to enforce that user classes can only acquire Throwable
 * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.
 */
static int GenStateClassIsExceptionOrError(ph7_class *pBase)
{
	while( pBase ){
		if( pBase->sName.nByte == sizeof("Exception")-1 &&
			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){
			return TRUE;
		}
		if( pBase->sName.nByte == sizeof("Error")-1 &&
			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){
			return TRUE;
		}
		pBase = pBase->pBase;
	}
	return FALSE;
}
/*
 * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).
 * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT|ENUMCASE) whose
 * aByteCode holds the BACKING value expression for backed enums (empty for pure
 * enums). The case's runtime value — the singleton instance — is materialized
 * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy
 * backing-value type/duplicate checks. Declaration order is recorded in
 * pClass->aEnumCases for cases().
 */
static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)
{
	sxu32 nLine = pGen->pIn->nLine;
	SySet *pInstrContainer;
	ph7_class_attr *pCase;
	SyString *pName;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'case' keyword */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"Invalid enum case name inside enum '%z'",&pClass->sName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pName = &pGen->pIn->sData;
	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */
	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
			"Cannot redefine class constant %z::%z",&pClass->sName,pName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,
		PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_ENUMCASE);
	if( pCase == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	pGen->pIn++; /* Jump the case name */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){
		if( pClass->nEnumBacking == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		pGen->pIn++; /* Jump the equal sign */
		/* Compile the backing value expression into the case's own container
		 * (same technique as class constants). */
		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);
		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);
		if( rc == SXERR_EMPTY ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Empty value for enum case %z::%z",&pClass->sName,pName);
		}
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}else{
		if( pClass->nEnumBacking != 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Case %z of backed enum %z must have a value",pName,&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
	}
	rc = PH7_ClassInstallAttr(pClass,pCase);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	SySetPut(&pClass->aEnumCases,(const void *)&pCase);
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){
		pGen->pIn++;
	}
	return SXERR_CORRUPT;
}
/*
 * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,
 * plus from()/tryFrom() for backed enums. Each is an ordinary public static
 * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the
 * enum's FQN embedded as a literal — the same forwarder pattern the
 * Generator/Fiber/Reflection builtins use. The source buffer is owned by the
 * VM allocator and never freed: tokens (method and parameter names) keep
 * pointers into it (see the constructor-promotion precedent above).
 */
static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)
{
	SyToken *pSaveIn,*pSaveEnd;
	const char *zBack;
	SySet sToken;
	char *zSrc;
	sxu32 nSrc,nMax;
	sxi32 rc = SXRET_OK;
	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")
		+ 3*SyStringLength(&pClass->sName) + 64;
	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);
	if( zSrc == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";
	if( pClass->nEnumBacking != 0 ){
		nSrc = SyBufferFormat(zSrc,nMax,
			"function cases(){return __phl_enum_cases('%z');}"
			"function from(%s $value){return __phl_enum_from('%z',$value);}"
			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",
			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);
	}else{
		nSrc = SyBufferFormat(zSrc,nMax,
			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);
	}
	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));
	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);
	pSaveIn = pGen->pIn;
	pSaveEnd = pGen->pEnd;
	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);
	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];
	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){
		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);
	}
	pGen->pIn = pSaveIn;
	pGen->pEnd = pSaveEnd;
	SySetRelease(&sToken);
	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;
}
/*
 * Magic methods an enum may not declare (php 8.1, zend_enum.c list —
 * __call/__callStatic/__invoke stay allowed).
 */
static const char *azEnumBannedMagic[] = {
	"__construct","__destruct","__clone","__get","__set","__isset","__unset",
	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"
};
/*
 * Enum post-body validation + synthesis: reject declared properties (including
 * trait-imported ones) and banned magic methods, install the readonly `name`
 * (and, for backed enums, `value`) instance properties the case singletons
 * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application
 * and before the class is installed.
 */
static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)
{
	SyHashEntry *pEntry;
	sxi32 rc;
	sxu32 n;
	/* php: "Enum %s cannot include properties" */
	SyHashResetLoopCursor(&pClass->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,
				"Enum %z cannot include properties",&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			break;
		}
	}
	/* php: "Enum %s cannot include magic method %s" */
	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){
		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],
			SyStrlen(azEnumBannedMagic[n])) != 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
	}
	/* Install the case-singleton instance properties: readonly `name` (every
	 * enum) and `value` (backed only). Materialization (vm.c) fills them and
	 * clears the readonly write-once latch; user writes then raise php's
	 * "Cannot modify readonly property" through the normal store path. */
	{
		static const SyString sNameProp = { "name",sizeof("name")-1 };
		static const SyString sValueProp = { "value",sizeof("value")-1 };
		ph7_class_attr *pAttr;
		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,
			PH7_CLASS_ATTR_READONLY|PH7_CLASS_ATTR_TYPED);
		if( pAttr == 0 ){
			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		pAttr->nType = MEMOBJ_STRING;
		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);
		PH7_ClassInstallAttr(pClass,pAttr);
		if( pClass->nEnumBacking != 0 ){
			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,
				PH7_CLASS_ATTR_READONLY|PH7_CLASS_ATTR_TYPED);
			if( pAttr == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
				return SXERR_ABORT;
			}
			pAttr->nType = pClass->nEnumBacking;
			if( pClass->nEnumBacking == MEMOBJ_INT ){
				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);
			}else{
				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);
			}
			PH7_ClassInstallAttr(pClass,pAttr);
		}
	}
	return GenStateCompileEnumMethods(&(*pGen),pClass);
}
/*
 * Compile a class declaration, named or anonymous.
 *
 * For a named class pAnonName is 0 and the class name is read from the token
 * stream. For an anonymous class (`new class(args) extends B implements I {…}`)
 * pAnonName carries the synthesized class name, the optional constructor
 * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to
 * compile, and no name token is expected. Everything after the header (extends/
 * implements, body, install) is shared by both paths.
 */
static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,
	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class *pClass,*pBase;
	SyToken *pEnd,*pTmp;
	sxi32 iProtection;
	SySet aInterfaces;
	SySet aUseEntries;
	sxi32 iAttrflags;
	SyString *pName;
	sxi32 nKwrd;
	sxi32 rc;
	/* Jump the 'class' keyword */
	pGen->pIn++;
	if( pAnonName ){
		/* Anonymous class: no name token. Capture the optional constructor
		 * '(args)' range for the caller (which always supplies the out-params),
		 * then use the synthesized name. */
		*ppArgStart = *ppArgEnd = 0;
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){
			pGen->pIn++; /* Jump '(' */
			*ppArgStart = pGen->pIn;
			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,
				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);
			pGen->pIn = *ppArgEnd;
			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */
		}
		pName = pAnonName;
		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);
	}else{
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
			/* Syntax error */
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			/* Synchronize with the first semi-colon or curly braces */
			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/|PH7_TK_SEMI/*';'*/)) == 0 ){
				pGen->pIn++;
			}
			return SXRET_OK;
		}
		/* Extract class name */
		pName = &pGen->pIn->sData;
		/* Advance the stream cursor */
		pGen->pIn++;
		/* Build FQN and obtain a raw class */ {
			SyBlob sFQN;
			SyString sFQNStr;
			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
			GenStateBuildFQN(pGen,pName,&sFQN);
			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));
			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);
			SyBlobRelease(&sFQN);
		}
	}
	if( pClass == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd
		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){
		/* Backed enum: `enum Name: int|string` (PHP 8.1) */
		pGen->pIn++; /* Jump ':' */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){
			pClass->nEnumBacking = MEMOBJ_INT;
			pGen->pIn++;
		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){
			pClass->nEnumBacking = MEMOBJ_STRING;
			pGen->pIn++;
		}else{
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){ pTok--; }
			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,
				"Enum backing type must be int or string, %z given",&pTok->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){
				pGen->pIn++; /* Skip the bogus type token */
			}
		}
	}
	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* implemented interfaces and per-use-statement trait containers */
	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));
	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));
	/* Assume a standalone class */
	pBase = 0;
	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){
			SyBlob sResolved;
			SyString sBaseName;
			sxu32 nRefLine;
			if( iFlags & PH7_CLASS_ENUM ){
				/* php parse-fatals here (enums have no inheritance) */
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
					"Enum %z cannot extend a class",&pClass->sName);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			pGen->pIn++; /* Advance past 'extends' */
			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;
			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
				SyBlobRelease(&sResolved);
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"Expected 'class_name' after 'extends' keyword inside class '%z'",
					pName);
				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			pBase = PH7_VmExtractClass(pGen->pVm,
				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);
			SyStringInitFromBuf(&sBaseName,
				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
			/* Interfaces are not allowed */
			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){
				pBase = pBase->pNextName;
			}
			if( pBase == 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,
					"Nonexistent base class '%z'",&sBaseName);
				if( rc == SXERR_ABORT ){
					SyBlobRelease(&sResolved);
					return SXERR_ABORT;
				}
			}else{
				if( pBase->iFlags & PH7_CLASS_ENUM ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Class %z cannot extend enum %z",pName,&pBase->sName);
					if( rc == SXERR_ABORT ){
						SyBlobRelease(&sResolved);
						return SXERR_ABORT;
					}
					pBase = 0; /* Never inherit from an enum */
				}else if( pBase->iFlags & PH7_CLASS_FINAL ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);
					if( rc == SXERR_ABORT ){
						SyBlobRelease(&sResolved);
						return SXERR_ABORT;
					}
				}
			}
			SyBlobRelease(&sResolved);
			if( iFlags & PH7_CLASS_ENUM ){
				pBase = 0; /* Error already reported: enums have no base class */
			}
		}
		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){
			ph7_class *pInterface;
			/* Interface implementation */
			pGen->pIn++; /* Advance the stream cursor */
			for(;;){
				SyBlob sResolved;
				SyString sIntName;
				sxu32 nRefLine;
				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;
				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
					SyBlobRelease(&sResolved);
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",
						pName);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					break;
				}
				pInterface = PH7_VmExtractClass(pGen->pVm,
					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);
				SyStringInitFromBuf(&sIntName,
					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
				/* Only interfaces are allowed */
				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){
					pInterface = pInterface->pNextName;
				}
				if( pInterface == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,
						"Nonexistent base interface '%z'",&sIntName);
					if( rc == SXERR_ABORT ){
						SyBlobRelease(&sResolved);
						return SXERR_ABORT;
					}
				}else{
					/* Reject user classes that try to implement Throwable
					 * directly (or via an interface that extends Throwable)
					 * unless they already extend Exception or Error.
					 * Exception and Error themselves are compiled from the
					 * built-in library and are exempt by FQN — a namespaced
					 * `Foo\Exception` is a different class and not exempt. */
					SyString *pFqn = &pClass->sName;
					int bIsExceptionOrError =
						(pFqn->nByte == sizeof("Exception")-1 &&
						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) ||
						(pFqn->nByte == sizeof("Error")-1 &&
						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);
					if( GenStateInterfaceIsThrowable(pInterface) &&
						!GenStateClassIsExceptionOrError(pBase) &&
						!bIsExceptionOrError ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Class %z cannot implement interface Throwable, extend Exception or Error instead",
							&pClass->sName);
						if( rc == SXERR_ABORT ){
							SyBlobRelease(&sResolved);
							return SXERR_ABORT;
						}
						/* Skip registration so the follow-up abstract-method
						 * check does not produce a duplicate fatal. */
					}else{
						SySetPut(&aInterfaces,(const void *)&pInterface);
					}
				}
				SyBlobRelease(&sResolved);
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){
					break;
				}
				pGen->pIn++;/* Jump the comma */
			}
		}
	}
	if( pGen->pIn >= pGen->pEnd  || (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pGen->pIn++; /* Jump the leading curly brace */
	pEnd = 0; /* cc warning */
	/* Delimit the class body */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* The delimiter token is the class body's closing brace */
	pClass->nEndLine = pEnd->nLine;
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */
	pClass->iFlags |= iFlags;
	/* Start the parse process */
	for(;;){
		/* Jump leading/trailing semi-colons */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){
			pGen->pIn++;
		}
		if( pGen->pIn >= pGen->pEnd ){
			/* End of class body */
			break;
		}
		/* Bind a directly-preceding docblock to this member */
		GenStateSetPendingDoc(&(*pGen));
		if( (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR)) == 0
			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",
				&pGen->pIn->sData,pName);
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto done;
		}
		/* Assume public visibility */
		iProtection = PH7_TKWRD_PUBLIC;
		iAttrflags = 0;
		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so
		 * it may precede the visibility keyword: `readonly public int $x`,
		 * `readonly int $x`. The visibility branch below also accepts it after
		 * the visibility keyword (`public readonly int $x`). */
		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){
			int bMod = 0;
			iAttrflags |= PH7_CLASS_ATTR_READONLY;
			pGen->pIn++; /* Jump the 'readonly' modifier */
			/* If a visibility/static modifier follows, let the dispatch below
			 * handle it; otherwise this is `readonly Type $x` (implicit public)
			 * and we compile it directly — the type may be a keyword (int/array)
			 * that the generic keyword dispatch would misread as a method. */
			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);
				bMod = ( k == PH7_TKWRD_PUBLIC || k == PH7_TKWRD_PRIVATE
					|| k == PH7_TKWRD_PROTECTED || k == PH7_TKWRD_STATIC );
			}
			if( !bMod ){
				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
				if( rc != SXRET_OK ){
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto done;
				}
				continue;
			}
		}
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
			/* Extract the current keyword */
			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){
				/* Enum case declaration: `case NAME [= value];` */
				rc = GenStateCompileEnumCase(&(*pGen),pClass);
				if( rc != SXRET_OK ){
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto done;
				}
				continue;
			}
			if( nKwrd == PH7_TKWRD_USE ){
				/* Trait use: use TraitA, TraitB [{ ... }]; */
				TraitUseEntry sUse;
				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));
				sUse.pResolvStart = sUse.pResolvEnd = 0;
				pGen->pIn++; /* Jump the 'use' keyword */
				for(;;){
					ph7_class *pTrait;
					SyString *pTraitName;
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Expected trait name after 'use' inside class '%z'",pName);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						break;
					}
					pTraitName = &pGen->pIn->sData;
					/* Resolve trait name through namespace/imports */ {
						SyBlob sResolved;
						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
						GenStateResolveName(pGen,pTraitName,&sResolved);
						pTrait = PH7_VmExtractClass(pGen->pVm,
							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);
						SyBlobRelease(&sResolved);
					}
					/* Only traits are allowed */
					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){
						pTrait = pTrait->pNextName;
					}
					if( pTrait == 0 ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"'%z' is not a trait",pTraitName);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
					}else{
						SySetPut(&sUse.aTraits,(const void *)&pTrait);
					}
					pGen->pIn++; /* Advance past trait name */
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){
						break;
					}
					pGen->pIn++; /* Jump the comma */
				}
				/* Expect semicolon or opening brace (for conflict resolution) */
				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){
					SyToken *pBlock;
					pGen->pIn++; /* Jump '{' */
					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);
					sUse.pResolvStart = pGen->pIn;
					sUse.pResolvEnd = pBlock;
					if( pBlock < pGen->pEnd ){
						pGen->pIn = &pBlock[1]; /* Skip past '}' */
					}else{
						pGen->pIn = pGen->pEnd;
					}
				}
				SySetPut(&aUseEntries,(const void *)&sUse);
				/* The semicolon will be consumed by the outer loop */
				continue;
			}
			if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
				int nSetTok;
				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);
				if( nSetVis ){
					/* Leading `private(set)`/`protected(set)` with no read
					 * visibility: the read side defaults to public (php 8.4). */
					iAttrflags |= GenStateSetVisFlag(nSetVis);
					pGen->pIn += nSetTok;
				}else{
					iProtection = nKwrd;
					pGen->pIn++; /* Jump the visibility token */
					/* Optional asymmetric set-visibility after the read
					 * visibility: `public private(set) int $x`. */
					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);
					if( nSetVis ){
						iAttrflags |= GenStateSetVisFlag(nSetVis);
						pGen->pIn += nSetTok;
					}
				}
				/* Optional `readonly` after the visibility: `public readonly int $x`,
				 * `public private(set) readonly int $x`. */
				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){
					iAttrflags |= PH7_CLASS_ATTR_READONLY;
					pGen->pIn++; /* Jump the 'readonly' modifier */
				}
				if( pGen->pIn >= pGen->pEnd
					|| (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR|PH7_TK_ID|PH7_TK_OP|PH7_TK_NSSEP|PH7_TK_LPAREN)) == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",
						&pGen->pIn->sData,pName);
					if( rc == SXERR_ABORT ){
						/* Error count limit reached,abort immediately */
						return SXERR_ABORT;
					}
					goto done;
				}
				if( pGen->pIn->nType & PH7_TK_DOLLAR ){
					/* Attribute declaration (untyped) */
					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
					if( rc != SXRET_OK ){
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						goto done;
					}
					continue;
				}
				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){
					/* Typed attribute declaration (PHP 7.4+) */
					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
					if( rc != SXRET_OK ){
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						goto done;
					}
					continue;
				}
				/* Extract the keyword */
				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			}
			if( nKwrd == PH7_TKWRD_CONST ){
				/* Process constant declaration */
				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);
				if( rc != SXRET_OK ){
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto done;
				}
			}else{
				if( nKwrd == PH7_TKWRD_STATIC ){
					/* Static method or attribute,record that */
					iAttrflags |= PH7_CLASS_ATTR_STATIC;
					pGen->pIn++; /* Jump the static keyword */
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
						int nSetTok;
						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);
						if( nSetVis ){
							/* `static private(set) int $x` — read side stays public */
							iAttrflags |= GenStateSetVisFlag(nSetVis);
							pGen->pIn += nSetTok;
						}else{
							/* Extract the keyword */
							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
							if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
								iProtection = nKwrd;
								pGen->pIn++; /* Jump the visibility token */
								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);
								if( nSetVis ){
									iAttrflags |= GenStateSetVisFlag(nSetVis);
									pGen->pIn += nSetTok;
								}
							}
						}
					}
					/* `readonly` after `static` (an invalid combination): detect it so the
					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather
					 * than a generic "expecting method" parse error. */
					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){
						iAttrflags |= PH7_CLASS_ATTR_READONLY;
						pGen->pIn++; /* Jump the 'readonly' modifier */
					}
					if( pGen->pIn >= pGen->pEnd
						|| (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR|PH7_TK_ID|PH7_TK_OP|PH7_TK_NSSEP|PH7_TK_LPAREN)) == 0 ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",
							&pGen->pIn->sData,pName);
						if( rc == SXERR_ABORT ){
							/* Error count limit reached,abort immediately */
							return SXERR_ABORT;
						}
						goto done;
					}
					if( pGen->pIn->nType & PH7_TK_DOLLAR ){
						/* Attribute declaration */
						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
						if( rc != SXRET_OK ){
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
							goto done;
						}
						continue;
					}
					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){
						/* Typed static attribute declaration */
						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
						if( rc != SXRET_OK ){
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
							goto done;
						}
						continue;
					}
					/* Extract the keyword */
					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){
					/* Abstract method,record that */
					iAttrflags |= PH7_CLASS_ATTR_ABSTRACT;
					/* Mark the whole class as abstract */
					pClass->iFlags |= PH7_CLASS_ABSTRACT;
					/* Advance the stream cursor */
					pGen->pIn++;
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
						if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
							iProtection = nKwrd;
							pGen->pIn++; /* Jump the visibility token */
						}
					}
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){
							/* Static method */
							iAttrflags |= PH7_CLASS_ATTR_STATIC;
							pGen->pIn++; /* Jump the static keyword */
					}
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ||
						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){
							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",
								&pGen->pIn->sData,pName);
							if( rc == SXERR_ABORT ){
								/* Error count limit reached,abort immediately */
								return SXERR_ABORT;
							}
							goto done;
					}
					nKwrd = PH7_TKWRD_FUNCTION;
				}else if( nKwrd == PH7_TKWRD_FINAL ){
					/* final method ,record that */
					iAttrflags |= PH7_CLASS_ATTR_FINAL;
					pGen->pIn++; /* Jump the final keyword */
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
						/* Extract the keyword */
						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
						if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
							iProtection = nKwrd;
							pGen->pIn++; /* Jump the visibility token */
						}
					}
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){
							/* final class constant (PHP 8.1). iAttrflags already carries
							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a
							 * child class is compiled (PH7_ClassInherit). */
							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);
							if( rc != SXRET_OK ){
								if( rc == SXERR_ABORT ){
									return SXERR_ABORT;
								}
								goto done;
							}
							continue;
					}
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){
							/* Static method */
							iAttrflags |= PH7_CLASS_ATTR_STATIC;
							pGen->pIn++; /* Jump the static keyword */
					}
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ||
						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){
							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",
								&pGen->pIn->sData,pName);
							if( rc == SXERR_ABORT ){
								/* Error count limit reached,abort immediately */
								return SXERR_ABORT;
							}
							goto done;
					}
					nKwrd = PH7_TKWRD_FUNCTION;
				}
				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){
					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
						"Unexpected token '%z',Expecting method declaration inside class '%z'",
							&pGen->pIn->sData,pName);
						if( rc == SXERR_ABORT ){
							/* Error count limit reached,abort immediately */
							return SXERR_ABORT;
						}
						goto done;
				}
				if( nKwrd == PH7_TKWRD_VAR ){
					pGen->pIn++; /* Jump the 'var' keyword */
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Expecting attribute declaration after 'var' keyword");
						if( rc == SXERR_ABORT ){
							/* Error count limit reached,abort immediately */
							return SXERR_ABORT;
						}
						goto done;
					}
					/* Attribute declaration */
					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
				}else{
					/* Process method declaration */
					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);
				}
				if( rc != SXRET_OK ){
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto done;
				}
			}
		}else{
			/* Attribute declaration */
			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
			if( rc != SXRET_OK ){
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto done;
			}
		}
	}
	/* Apply collected traits (per use-statement) before installing the class.
	 * Each use-statement carries its own set of traits and optional resolution block.
	 */
	{
		TraitUseEntry *apUse;
		sxu32 nU;
		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);
		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){
			TraitUseEntry *pUse = &apUse[nU];
			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);
			sxu32 nTraits = SySetUsed(&pUse->aTraits);
			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;
			sxu32 nT;
			if( !hasResolution ){
				/* No conflict resolution block: use standard trait application */
				for( nT = 0 ; nT < nTraits ; nT++ ){
					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);
					if( rc != SXRET_OK ){
						break;
					}
				}
			}else{
				/* With resolution block: copy attributes, record traits,
				 * then use the block to resolve method conflicts.
				 */
				SyToken *pR;
				for( nT = 0 ; nT < nTraits ; nT++ ){
					ph7_class *pTR = apTrait[nT];
					ph7_class_attr *pAR;
					SyHashEntry *pER;
					SyString *pNR;
					SyHashResetLoopCursor(&pTR->hAttr);
					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){
						pAR = (ph7_class_attr *)pER->pUserData;
						pNR = &pAR->sName;
						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){
							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);
						}
					}
					SySetPut(&pClass->aTrait,(const void *)&pTR);
				}
				/* Pass 1: process insteadof rules to install winning methods */
				pR = pUse->pResolvStart;
				while( pR < pUse->pResolvEnd ){
					SyString sTrait,sMethod;
					ph7_class *pSrcTrait;
					ph7_class_method *pMeth;
					sxi32 nRKwrd;
					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }
					if( pR >= pUse->pResolvEnd ) break;
					SyStringInitFromBuf(&sTrait,"",0);
					SyStringInitFromBuf(&sMethod,"",0);
					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }
					sMethod = pR->sData;
					pR++;
					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){
						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;
						if( pOp && pOp->iOp == EXPR_OP_DC ){
							sTrait = sMethod;
							pR++;
							if( pR >= pUse->pResolvEnd || (pR->nType & PH7_TK_ID) == 0 ) break;
							sMethod = pR->sData;
							pR++;
						}
					}
					if( pR >= pUse->pResolvEnd || (pR->nType & PH7_TK_KEYWORD) == 0 ){
						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }
						continue;
					}
					nRKwrd = SX_PTR_TO_INT(pR->pUserData);
					pR++;
					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){
						pSrcTrait = 0;
						for( nT = 0 ; nT < nTraits ; nT++ ){
							SyString *pTN = &apTrait[nT]->sName;
							if( pTN->nByte >= sTrait.nByte &&
								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){
								pSrcTrait = apTrait[nT];
								break;
							}
						}
						if( pSrcTrait ){
							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);
							if( pMeth ){
								SyString *pMN = &pMeth->sFunc.sName;
								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){
									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);
								}
							}
						}
					}
					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }
				}
				/* Install remaining non-conflicting methods from this use's traits */
				for( nT = 0 ; nT < nTraits ; nT++ ){
					ph7_class_method *pMR;
					SyHashEntry *pER;
					SyString *pNR;
					SyHashResetLoopCursor(&apTrait[nT]->hMethod);
					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){
						pMR = (ph7_class_method *)pER->pUserData;
						pNR = &pMR->sFunc.sName;
						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){
							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);
						}
					}
				}
				/* Pass 2: process as rules (aliases and visibility changes) */
				pR = pUse->pResolvStart;
				while( pR < pUse->pResolvEnd ){
					SyString sTrait,sMethod,sAlias;
					ph7_class *pSrcTrait;
					ph7_class_method *pMeth;
					int hasQual = 0;
					sxi32 nRKwrd;
					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }
					if( pR >= pUse->pResolvEnd ) break;
					SyStringInitFromBuf(&sTrait,"",0);
					SyStringInitFromBuf(&sMethod,"",0);
					SyStringInitFromBuf(&sAlias,"",0);
					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }
					sMethod = pR->sData;
					pR++;
					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){
						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;
						if( pOp && pOp->iOp == EXPR_OP_DC ){
							sTrait = sMethod;
							hasQual = 1;
							pR++;
							if( pR >= pUse->pResolvEnd || (pR->nType & PH7_TK_ID) == 0 ) break;
							sMethod = pR->sData;
							pR++;
						}
					}
					if( pR >= pUse->pResolvEnd || (pR->nType & PH7_TK_KEYWORD) == 0 ){
						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }
						continue;
					}
					nRKwrd = SX_PTR_TO_INT(pR->pUserData);
					pR++;
					if( nRKwrd == PH7_TKWRD_AS ){
						sxi32 iNewVis = -1;
						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){
							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);
							if( nAK == PH7_TKWRD_PUBLIC || nAK == PH7_TKWRD_PROTECTED || nAK == PH7_TKWRD_PRIVATE ){
								iNewVis = nAK;
								pR++;
							}
						}
						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){
							sAlias = pR->sData;
							pR++;
						}
						pMeth = 0;
						if( hasQual ){
							pSrcTrait = 0;
							for( nT = 0 ; nT < nTraits ; nT++ ){
								SyString *pTN = &apTrait[nT]->sName;
								if( pTN->nByte >= sTrait.nByte &&
									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){
									pSrcTrait = apTrait[nT];
									break;
								}
							}
							if( pSrcTrait ){
								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);
							}
						}else{
							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);
						}
						if( pMeth ){
							if( sAlias.nByte > 0 ){
								/* Create a shallow copy of the method struct for the alias
								 * so it can carry its own visibility without affecting the original.
								 */
								ph7_class_method *pAlias;
								char *zAliasDup;
								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));
								if( pAlias ){
									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));
									if( iNewVis >= 0 ){
										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;
										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;
										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;
									}
									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);
									if( zAliasDup ){
										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);
									}
								}
							}else if( iNewVis >= 0 ){
								/* Visibility-only change (no alias name): also needs a copy */
								ph7_class_method *pCopy;
								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));
								if( pCopy ){
									SyString *pMN = &pMeth->sFunc.sName;
									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));
									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;
									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;
									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;
									/* Replace the method in the class hash */
									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);
									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);
								}
							}
						}
						SXUNUSED(hasQual);
					}
					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }
				}
			}
			SySetRelease(&pUse->aTraits);
		}
	}
	if( pClass->iFlags & PH7_CLASS_ENUM ){
		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.
		 * Runs after trait application so trait-imported properties are caught. */
		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);
		if( rc == SXERR_ABORT ){
			SySetRelease(&aUseEntries);
			SySetRelease(&aInterfaces);
			return SXERR_ABORT;
		}
	}
	/* Install the class */
	rc = PH7_VmInstallClass(pGen->pVm,pClass);
	if( rc == SXRET_OK ){
		ph7_class **apInterface;
		sxu32 n;
		if( pBase ){
			/* Inherit from base class and mark as a subclass */
			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);
		}
		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);
		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){
			/* Implements one or more interface */
			rc = PH7_ClassImplement(pClass,apInterface[n]);
			if( rc != SXRET_OK ){
				break;
			}
		}
		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:
		 * every enum satisfies `instanceof UnitEnum` implicitly. */
		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){
			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);
			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){
				pIntf = pIntf->pNextName;
			}
			if( pIntf ){
				PH7_ClassImplement(pClass,pIntf);
			}
			if( pClass->nEnumBacking != 0 ){
				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);
				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){
					pIntf = pIntf->pNextName;
				}
				if( pIntf ){
					PH7_ClassImplement(pClass,pIntf);
				}
			}
		}
		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).
		 * Skip interfaces/traits and classes that already implement it explicitly. */
		if( rc == SXRET_OK
		 && (pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT)) == 0
		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){
			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,
				"Stringable",sizeof("Stringable")-1,FALSE,0);
			if( pStringable ){
				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);
				sxu32 nImpl = SySetUsed(&pClass->aInterface);
				sxu32 i;
				int bAlready = 0;
				for( i = 0 ; i < nImpl ; i++ ){
					if( apImpl[i] == pStringable ){
						bAlready = 1;
						break;
					}
				}
				if( !bAlready ){
					PH7_ClassImplement(pClass,pStringable);
				}
			}
		}
		/* Validate interface method signatures (visibility and parameter count) */
		if( rc == SXRET_OK ){
			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);
			if( rcCheck == SXERR_ABORT ){
				SySetRelease(&aUseEntries);
				SySetRelease(&aInterfaces);
				return SXERR_ABORT;
			}
		}
		/* Check for unimplemented abstract methods in concrete classes */
		if( rc == SXRET_OK ){
			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);
			if( rcCheck == SXERR_ABORT ){
				SySetRelease(&aUseEntries);
				SySetRelease(&aInterfaces);
				return SXERR_ABORT;
			}
		}
	}
	SySetRelease(&aUseEntries);
	SySetRelease(&aInterfaces);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
done:
	/* Point beyond the class body */
	pGen->pIn = &pEnd[1];
	pGen->pEnd = pTmp;
	return PH7_OK;
}
/* Compile a named class declaration (the common case). */
static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)
{
	return GenStateCompileClassEx(pGen,iFlags,0,0,0);
}
/*
 * Compile an anonymous class expression: `new class(args) extends B implements I
 * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,
 * compile + install the class body once (at compile time, like every other
 * class), then emit the instantiation — push the constructor arguments, load the
 * synthesized class name, and OP_NEW. The class is installed once per source
 * site, matching PHP's one-class-per-anonymous-site semantics.
 */
PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	char zName[128];         /* Synthesized class name */
	static int iCnt = 1;     /* Single-threaded compile: no locking needed */
	SyString sName;
	SyToken *pArgStart,*pArgEnd;
	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia
	                              * is keyed to this 'class' token */
	ph7_value *pObj;
	sxu32 nLine = pGen->pIn->nLine;
	sxu32 nIdx,nLen;
	sxi32 nArg,rc;
	SXUNUSED(iCompileFlag);
	/* Generate a unique anonymous-class name (collision-checked) */
	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);
	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){
		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);
	}
	SyStringInitFromBuf(&sName,zName,nLen);
	/* Compile + install the class body; capture the constructor '(args)' range.
	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the
	 * delimited construct; GenStateCompileClassEx restores both on success. */
	pArgStart = pArgEnd = 0;
	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);
	if( rc != SXRET_OK ){
		return rc;
	}
	{
		/* Expression-position attributes (`new #[A] class {…}`) */
		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);
		if( pAnonClass
		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Emit the instantiation. OP_NEW expects the class name on the stack top
	 * with the constructor arguments beneath it, so push the args first. */
	nArg = 0;
	if( pArgStart < pArgEnd ){
		SyToken *pSavedIn = pGen->pIn;
		SyToken *pSavedEnd = pGen->pEnd;
		SyToken *pArgNext;
		pGen->pIn = pArgStart;
		pGen->pEnd = pArgEnd;
		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){
			if( pGen->pIn < pArgNext ){
				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);
				if( rc == SXERR_ABORT ){
					pGen->pIn = pSavedIn;
					pGen->pEnd = pSavedEnd;
					return SXERR_ABORT;
				}
				nArg++;
			}
			pGen->pIn = &pArgNext[1];
		}
		pGen->pIn = pSavedIn;
		pGen->pEnd = pSavedEnd;
	}
	/* Load the synthesized class name */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	/* Instantiate: pops the name + nArg arguments, runs __construct */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);
	return SXRET_OK;
}
/*
 * Compile a user-defined abstract class.
 *  According to the PHP language reference manual
 *   PHP 5 introduces abstract classes and methods. Classes defined as abstract
 *   may not be instantiated, and any class that contains at least one abstract
 *   method must also be abstract. Methods defined as abstract simply declare
 *   the method's signature - they cannot define the implementation.
 *   When inheriting from an abstract class, all methods marked abstract in the parent's
 *   class declaration must be defined by the child; additionally, these methods must be
 *   defined with the same (or a less restricted) visibility. For example, if the abstract
 *   method is defined as protected, the function implementation must be defined as either
 *   protected or public, but not private. Furthermore the signatures of the methods must
 *   match, i.e. the type hints and the number of required arguments must be the same.
 *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures
 *   could differ.
 */
/*
 * Recognize a class-declaration modifier token: the `final`/`abstract` keywords
 * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag
 * receives the corresponding PH7_CLASS_* bit.
 */
static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)
{
	if( pTok->nType & PH7_TK_KEYWORD ){
		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);
		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }
		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }
	}
	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }
	return FALSE;
}
/*
 * Advance *ppIn over a leading run of class modifiers, returning the combined
 * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated
 * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.
 * This stays side-effect-free so it can be used for speculative look-ahead.
 */
static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)
{
	SyToken *pIn = *ppIn,*pDup = 0;
	sxi32 iFlags = 0,iFlag;
	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){
		if( (iFlags & iFlag) && pDup == 0 ){
			pDup = pIn;
		}
		iFlags |= iFlag;
		pIn++;
	}
	*ppIn = pIn;
	if( ppDup ){ *ppDup = pDup; }
	return iFlags;
}
/*
 * Test whether the token stream starts a *modified* class declaration: a run of
 * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated
 * by the `class` keyword. Requiring at least one modifier leaves a bare
 * `class`/`interface`/`trait` (and any expression that merely starts with
 * `readonly`) to their existing handlers.
 */
static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)
{
	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);
	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)
		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;
}
/*
 * Compile a class declaration carrying one or more leading modifiers
 * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving
 * the cursor on the `class` keyword for GenStateCompileClass, and rejects a
 * repeated modifier (`final final class`) or the mutually-exclusive
 * `abstract`+`final` pair, like PHP.
 */
static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)
{
	SyToken *pDup;
	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);
	sxi32 rc;
	if( pDup ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,
			"Multiple %z modifiers are not allowed",&pDup->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	if( (iFlags & (PH7_CLASS_FINAL|PH7_CLASS_ABSTRACT))
		== (PH7_CLASS_FINAL|PH7_CLASS_ABSTRACT) ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
			"Cannot use the final modifier on an abstract class");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return GenStateCompileClass(&(*pGen),iFlags);
}
/*
 * Compile a user-defined trait.
 *  Traits are similar to classes, but only intended to group functionality
 *  in a fine-grained and consistent way. It is not possible to instantiate
 *  a Trait on its own. Traits cannot extend or implement.
 */
static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class *pClass;
	SyToken *pEnd,*pTmp;
	sxi32 iProtection;
	sxi32 iAttrflags;
	SyString *pName;
	sxi32 nKwrd;
	sxi32 rc;
	/* Jump the 'trait' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB|PH7_TK_SEMI)) == 0 ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Extract trait name */
	pName = &pGen->pIn->sData;
	pGen->pIn++;
	/* Build FQN and obtain a raw class */ {
		SyBlob sFQN;
		SyString sFQNStr;
		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
		GenStateBuildFQN(pGen,pName,&sFQN);
		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));
		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);
		SyBlobRelease(&sFQN);
	}
	if( pClass == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Traits cannot extend or implement; expect opening brace directly */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_OCB) == 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pGen->pIn++; /* Jump the leading curly brace */
	pEnd = 0;
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);
	if( pEnd >= pGen->pEnd ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* The delimiter token is the trait body's closing brace */
	pClass->nEndLine = pEnd->nLine;
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */
	pClass->iFlags |= PH7_CLASS_TRAIT;
	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */
	for(;;){
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){
			pGen->pIn++;
		}
		if( pGen->pIn >= pGen->pEnd ){
			break;
		}
		/* Bind a directly-preceding docblock to this member */
		GenStateSetPendingDoc(&(*pGen));
		if( (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR)) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",
				&pGen->pIn->sData,pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto done;
		}
		iProtection = PH7_TKWRD_PUBLIC;
		iAttrflags = 0;
		if( pGen->pIn->nType & PH7_TK_KEYWORD ){
			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKwrd == PH7_TKWRD_USE ){
				/* Trait uses another trait: use OtherTrait; */
				pGen->pIn++; /* Jump 'use' */
				for(;;){
					ph7_class *pUsedTrait;
					SyString *pUsedName;
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Expected trait name after 'use' inside trait '%z'",pName);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						break;
					}
					pUsedName = &pGen->pIn->sData;
					{
						SyBlob sResolved;
						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
						GenStateResolveName(pGen,pUsedName,&sResolved);
						pUsedTrait = PH7_VmExtractClass(pGen->pVm,
							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);
						SyBlobRelease(&sResolved);
					}
					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){
						pUsedTrait = pUsedTrait->pNextName;
					}
					if( pUsedTrait == 0 ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"'%z' is not a trait",pUsedName);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
					}else{
						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);
					}
					pGen->pIn++;
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){
						break;
					}
					pGen->pIn++;
				}
				continue;
			}
			if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
				iProtection = nKwrd;
				pGen->pIn++;
				if( pGen->pIn >= pGen->pEnd
					|| (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR|PH7_TK_ID|PH7_TK_OP|PH7_TK_NSSEP|PH7_TK_LPAREN)) == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",
						&pGen->pIn->sData,pName);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto done;
				}
				if( pGen->pIn->nType & PH7_TK_DOLLAR ){
					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
					if( rc != SXRET_OK ){
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						goto done;
					}
					continue;
				}
				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){
					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
					if( rc != SXRET_OK ){
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						goto done;
					}
					continue;
				}
				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			}
			if( nKwrd == PH7_TKWRD_CONST ){
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
					"Traits cannot have constants");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto done;
			}else{
				if( nKwrd == PH7_TKWRD_STATIC ){
					iAttrflags |= PH7_CLASS_ATTR_STATIC;
					pGen->pIn++;
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
						if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
							iProtection = nKwrd;
							pGen->pIn++;
						}
					}
					if( pGen->pIn >= pGen->pEnd
						|| (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR|PH7_TK_ID|PH7_TK_OP|PH7_TK_NSSEP|PH7_TK_LPAREN)) == 0 ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",
							&pGen->pIn->sData,pName);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						goto done;
					}
					if( pGen->pIn->nType & PH7_TK_DOLLAR ){
						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
						if( rc != SXRET_OK ){
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
							goto done;
						}
						continue;
					}
					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){
						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
						if( rc != SXRET_OK ){
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
							goto done;
						}
						continue;
					}
					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){
					iAttrflags |= PH7_CLASS_ATTR_ABSTRACT;
					pGen->pIn++;
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
						if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
							iProtection = nKwrd;
							pGen->pIn++;
						}
					}
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ||
						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",
							&pGen->pIn->sData,pName);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						goto done;
					}
					nKwrd = PH7_TKWRD_FUNCTION;
				}
				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){
					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
						"Unexpected token '%z',Expecting method declaration inside trait '%z'",
						&pGen->pIn->sData,pName);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto done;
				}
				if( nKwrd == PH7_TKWRD_VAR ){
					pGen->pIn++;
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Expecting attribute declaration after 'var' keyword");
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						goto done;
					}
					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
				}else{
					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);
				}
				if( rc != SXRET_OK ){
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto done;
				}
			}
		}else{
			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
			if( rc != SXRET_OK ){
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto done;
			}
		}
	}
	/* Install the trait */
	rc = PH7_VmInstallClass(pGen->pVm,pClass);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
done:
	/* Point beyond the trait body */
	pGen->pIn = &pEnd[1];
	pGen->pEnd = pTmp;
	return PH7_OK;
}
/*
 * Compile a user-defined class.
 *  According to the PHP language reference manual
 *   Basic class definitions begin with the keyword class, followed
 *   by a class name, followed by a pair of curly braces which enclose
 *   the definitions of the properties and methods belonging to the class.
 *   A class may contain its own constants, variables (called "properties")
 *   and functions (called "methods").
 */
static sxi32 PH7_CompileClass(ph7_gen_state *pGen)
{
	sxi32 rc;
	rc = GenStateCompileClass(&(*pGen),0);
	return rc;
}
/*
 * Return TRUE if the token stream starts an enum declaration (PHP 8.1):
 * the context-sensitive identifier `enum` (not a reserved word — it stays
 * valid as a function/constant name, like `readonly`) directly followed by
 * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression
 * meaning; `enum Name` can never start a valid expression.
 */
static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)
{
	return (pIn->nType & PH7_TK_ID)
		&& pIn->sData.nByte == sizeof("enum")-1
		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0
		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);
}
/*
 * Compile an enum declaration (PHP 8.1). An enum is a final class carrying
 * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton
 * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum
 * are implemented implicitly (GenStateCompileClassEx handles the specifics).
 */
static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)
{
	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM|PH7_CLASS_FINAL);
}
/*
 * Exception handling.
 *  According to the PHP language reference manual
 *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded
 *    in a try block, to facilitate the catching of potential exceptions. Each try must have
 *    at least one corresponding catch block. Multiple catch blocks can be used to catch
 *    different classes of exceptions. Normal execution (when no exception is thrown within
 *    the try block, or when a catch matching the thrown exception's class is not present)
 *    will continue after that last catch block defined in sequence. Exceptions can be thrown
 *    (or re-thrown) within a catch block.
 *    When an exception is thrown, code following the statement will not be executed, and PHP
 *    will attempt to find the first matching catch block. If an exception is not caught, a PHP
 *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has
 *    been defined with set_exception_handler().
 *    The thrown object must be an instance of the Exception class or a subclass of Exception.
 *    Trying to throw an object that is not will result in a PHP Fatal Error.
 */
/*
 * Expression tree validator callback associated with the 'throw' statement.
 * Return SXRET_OK if the tree form a valid expression.Any other error
 * indicates failure.
 */
static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	sxi32 rc = SXRET_OK;
	if( pRoot->pOp ){
		switch( pRoot->pOp->iOp ){
		case EXPR_OP_NEW:            /* new Exception() */
		case EXPR_OP_ARROW:          /* $obj->prop */
		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */
		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */
		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */
		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */
			break;
		default:
			/* Runtime will still reject non-Throwable values; the set above
			 * covers the common shapes and gives a friendlier compile error
			 * for obvious mistakes like `throw 5`. */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
				"throw: Expecting an exception class instance");
			if( rc != SXERR_ABORT ){
				rc = SXERR_INVALID;
			}
			break;
		}
	}else if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
			"throw: Expecting an exception class instance");
		if( rc != SXERR_ABORT ){
			rc = SXERR_INVALID;
		}
	}
	return rc;
}
/*
 * Compile a 'throw' statement.
 * throw: This is how you trigger an exception.
 * Each "throw" block must have at least one "catch" block associated with it.
 */
static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	GenBlock *pBlock;
	sxu32 nIdx;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'throw' keyword */
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);
	if( rc == SXERR_EMPTY ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pBlock = pGen->pCurrent;
	/* Point to the top most function or try block and emit the forward jump */
	while(pBlock->pParent){
		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION|GEN_BLOCK_FUNC) ){
			break;
		}
		/* Point to the parent block */
		pBlock = pBlock->pParent;
	}
	/* Emit the throw instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);
	/* Emit the jump */
	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);
	return SXRET_OK;
}
/*
 * Compile a PHP 8.0 'throw' expression.
 * Called from the expression code generator when a 'throw' keyword is
 * encountered in an expression context (e.g. `$x ?? throw new E()`).
 * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;
 * the validator guarantees the operand is a valid exception target.
 */
PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)
{
	sxu32 nLine = pGen->pIn->nLine;
	GenBlock *pBlock;
	sxu32 nIdx;
	sxi32 rc;
	(void)iCompileFlag;
	pGen->pIn++; /* Skip 'throw' */
	if( pGen->pIn >= pGen->pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"throw: Expecting an exception class instance");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( rc == SXERR_EMPTY ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"throw: Expecting an exception class instance");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Walk up to nearest exception/function block for the jump target */
	pBlock = pGen->pCurrent;
	while( pBlock->pParent ){
		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION|GEN_BLOCK_FUNC) ){
			break;
		}
		pBlock = pBlock->pParent;
	}
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);
	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);
	return SXRET_OK;
}
/*
 * ROOT C: parse a single `catch (A | B $e)` header (no body) into an
 * ph7_exception_block. On success pGen->pIn is positioned at the catch body's
 * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body
 * compilation to the caller (which emits it inline). Returns SXRET_OK, or a
 * compile error propagated from the parser.
 */
static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)
{
	SyString sClassName;
	SyToken *pToken;
	SyString *pName;
	char *zDup;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'catch' keyword */
	SyZero(pCatch,sizeof(ph7_exception_block));
	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));
	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }
		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",
			TokenTypeName(pToken->nType),&pToken->sData);
		return SXERR_INVALID;
	}
	pGen->pIn++; /* '(' */
	for(;;){
		SyBlob sResolved;
		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
			SyBlobRelease(&sResolved);
			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }
			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",
				TokenTypeName(pToken->nType),&pToken->sData);
			return SXERR_INVALID;
		}
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));
		SyBlobRelease(&sResolved);
		if( zDup == 0 ){ return SXERR_ABORT; }
		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);
		if( rc != SXRET_OK ){ return SXERR_ABORT; }
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&
			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '|' ){
			pGen->pIn++; continue;
		}
		break;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ||
		&pGen->pIn[1] >= pGen->pEnd || (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }
		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",
			TokenTypeName(pToken->nType),&pToken->sData);
		return SXERR_INVALID;
	}
	pGen->pIn++; /* '$' */
	pName = &pGen->pIn->sData;
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zDup == 0 ){ return SXERR_ABORT; }
	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){
		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }
		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",
			TokenTypeName(pToken->nType),&pToken->sData);
		return SXERR_INVALID;
	}
	pGen->pIn++; /* ')' */
	return SXRET_OK;
}
/*
 * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode
 * container. Used only for generator bodies so a `yield` inside a catch/finally
 * suspends correctly (the legacy path runs them via a detached VmLocalExec whose
 * pc/stack a generator resume cannot restore). Layout (see the block comment on
 * VmThrowException):
 *
 *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame
 *    <try body>
 *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)
 *    JMP  -> finally|end
 *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e
 *    <catch body>
 *    JMP  -> finally|end
 *    ... more catches ...
 *  Lfin: <finally body>
 *    END_FINALLY p3=pExc               ; dispatch pending action
 *  Lend:
 */
static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)
{
	sxu32 nLine = pGen->pIn->nLine;
	GenBlock *pTry;
	VmInstr *pInstr;
	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;
	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */
	sxi32 rc;
	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));
	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);
	if( rc != SXRET_OK ){ return SXERR_ABORT; }
	pTry->pUserData = pException;
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);
	pGen->pIn++; /* Jump the 'try' keyword */
	rc = PH7_CompileBlock(&(*pGen),0);
	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }
	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));
	iLpop = PH7_VmInstrLength(pGen->pVm);
	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */
	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);
	if( pInstr ){ pInstr->iP2 = iLpop; }
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);
	GenStateLeaveBlock(&(*pGen),0);
	/* Normal-completion jump -> finally or end (target fixed after layout) */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);
	/* Catch clauses (inline) */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){
		sxu32 k = 0;
		for(;;){
			ph7_exception_block sCatch;
			GenBlock *pCatchBlk;
			sxu32 idxJmp = 0;
			if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0
				|| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){
				break;
			}
			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);
			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }
			if( rc != SXRET_OK ){ return SXERR_INVALID; }
			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);
			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);
			if( rc != SXRET_OK ){ return SXERR_ABORT; }
			/* Tag the catch block with its try so a break/continue leaving the catch counts
			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch
			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */
			pCatchBlk->pUserData = pException;
			rc = PH7_CompileBlock(&(*pGen),0);
			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }
			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));
			GenStateLeaveBlock(&(*pGen),0);
			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a
			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);
			SySetPut(&aCatchJmp,(const void *)&idxJmp);
			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);
			if( rc != SXRET_OK ){ return SXERR_ABORT; }
			k++;
		}
	}
	/* Finally (inline) */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){
		GenBlock *pFinBlk;
		pGen->pIn++; /* Jump 'finally' */
		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);
		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);
		if( rc != SXRET_OK ){ return SXERR_ABORT; }
		rc = PH7_CompileBlock(&(*pGen),0);
		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }
		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));
		GenStateLeaveBlock(&(*pGen),0);
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);
		pException->iHasFinally = 1;
	}
	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);
	pException->iInlined = 1;
	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */
	{
		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;
		sxu32 *aJ; sxu32 n;
		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);
		if( pInstr ){ pInstr->iP2 = iTarget; }
		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);
		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){
			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);
			if( pInstr ){ pInstr->iP2 = iTarget; }
		}
	}
	SySetRelease(&aCatchJmp);
	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");
	}
	return SXRET_OK;
}
/*
 * Compile a 'catch' block.
 * Catch: A "catch" block retrieves an exception and creates
 * an object containing the exception information.
 */
static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_exception_block sCatch;
	SySet *pInstrContainer;
	SyString sClassName;
	GenBlock *pCatch;
	SyToken *pToken;
	SyString *pName;
	char *zDup;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'catch' keyword */
	/* Zero the structure */
	SyZero(&sCatch,sizeof(ph7_exception_block));
	/* Initialize fields */
	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));
	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){
			/* Unexpected token,break immediately */
			pToken = pGen->pIn;
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,
				"syntax error, unexpected %s \"%z\"",
				TokenTypeName(pToken->nType),&pToken->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_INVALID;
	}
	/* Extract the exception class(es) — supports multi-catch: catch (A | B $e) */
	pGen->pIn++; /* Jump the left parenthesis '(' */
	for(;;){
		SyBlob sResolved;
		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
			SyBlobRelease(&sResolved);
			pToken = pGen->pIn;
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,
				"syntax error, unexpected %s \"%z\"",
				TokenTypeName(pToken->nType),&pToken->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_INVALID;
		}
		/* Persist the FQN beyond this function — aClasses outlives the
		 * transient SyBlob allocation. */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));
		SyBlobRelease(&sResolved);
		if( zDup == 0 ){
			goto Mem;
		}
		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);
		if( rc != SXRET_OK ){
			goto Mem;
		}
		/* Check for '|' (multi-catch separator) */
		if( pGen->pIn < pGen->pEnd &&
			(pGen->pIn->nType & PH7_TK_OP) &&
			pGen->pIn->sData.nByte == 1 &&
			pGen->pIn->sData.zString[0] == '|' ){
			pGen->pIn++; /* Consume the '|' */
			continue;
		}
		break;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ ||
		&pGen->pIn[1] >= pGen->pEnd || (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			/* Unexpected token,break immediately */
			pToken = pGen->pIn;
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,
				"syntax error, unexpected %s \"%z\"",
				TokenTypeName(pToken->nType),&pToken->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_INVALID;
	}
	pGen->pIn++; /* Jump the dollar sign */
	/* Duplicate instance name */
	pName = &pGen->pIn->sData;
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zDup == 0 ){
		goto Mem;
	}
	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){
		/* Unexpected token,break immediately */
		pToken = pGen->pIn;
		if( pToken >= pGen->pEnd ){
			pToken--;
		}
		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,
			"syntax error, unexpected %s \"%z\"",
			TokenTypeName(pToken->nType),&pToken->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXERR_INVALID;
	}
	/* Compile the block */
	pGen->pIn++; /* Jump the right parenthesis */
	/* Create the catch block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Swap bytecode container */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);
	/* Compile the block */
	PH7_CompileBlock(&(*pGen),0);
	/* Fix forward jumps now the destination is resolved  */
	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));
	/* Emit the DONE instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
	/* Leave the block */
	GenStateLeaveBlock(&(*pGen),0);
	/* Restore the default container */
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	/* Install the catch block */
	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);
	if( rc != SXRET_OK ){
		goto Mem;
	}
	return SXRET_OK;
Mem:
	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
	return SXERR_ABORT;
}
/*
 * Compile a 'try' block.
 * A function using an exception should be in a "try" block.
 * If the exception does not trigger, the code will continue
 * as normal. However if the exception triggers, an exception
 * is "thrown".
 */
static sxi32 PH7_CompileTry(ph7_gen_state *pGen)
{
	ph7_exception *pException;
	sxu32 nLine = pGen->pIn->nLine;
	GenBlock *pTry;
	sxu32 nJmpIdx;
	sxi32 rc;
	/* Create the exception container */
	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));
	if( pException == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,
			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	/* Zero the structure */
	SyZero(pException,sizeof(ph7_exception));
	/* Initialize fields */
	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));
	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));
	pException->iHasFinally = 0;
	pException->iFinallyDone = 0;
	pException->pVm = pGen->pVm;
	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a
	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.
	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,
	 * VmThrowException pc-redirect, return/break-through-finally threading, generator
	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet
	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */
	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){
		return PH7_CompileTryInline(&(*pGen),pException);
	}
	/* Create the try block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Store exception pointer so break/continue can emit POP_EXCEPTION */
	pTry->pUserData = pException;
	/* Emit the 'LOAD_EXCEPTION' instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);
	/* Fix the jump later when the destination is resolved */
	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);
	pGen->pIn++; /* Jump the 'try' keyword */
	/* Compile the block */
	rc = PH7_CompileBlock(&(*pGen),0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Fix forward jumps now the destination is resolved */
	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));
	/* Emit the 'POP_EXCEPTION' instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);
	/* Leave the block */
	GenStateLeaveBlock(&(*pGen),0);
	/* Compile catch block(s) — at least one catch or finally is required */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){
		/* Compile one or more catch blocks */
		for(;;){
			if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0
				|| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){
					break;
			}
			rc = PH7_CompileCatch(&(*pGen),pException);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
	}
	/* Compile optional finally block */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){
		SySet *pInstrContainer;
		GenBlock *pFinBlock;
		pGen->pIn++; /* Jump the 'finally' keyword */
		/* Create the finally block for jump fixup bookkeeping */
		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);
		if( rc != SXRET_OK ){
			return SXERR_ABORT;
		}
		/* Swap bytecode container */
		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);
		/* Compile the finally body */
		rc = PH7_CompileBlock(&(*pGen),0);
		if( rc == SXERR_ABORT ){
			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
			return SXERR_ABORT;
		}
		/* Fix forward jumps now the destination is resolved */
		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));
		/* Emit DONE to terminate the finally block */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
		/* Leave the block */
		GenStateLeaveBlock(&(*pGen),0);
		/* Restore the default container */
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
		pException->iHasFinally = 1;
	}
	/* Must have at least one catch or finally */
	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Cannot use try without catch or finally");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return SXRET_OK;
}
/*
 * Compile a switch block.
 *  (See block-comment below for more information)
 */
static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)
{
	sxi32 rc = SXRET_OK;
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_COLON/*':'*/)) == 0 ){
		/* Unexpected token */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pGen->pIn++;
	/* First instruction to execute in this block. */
	*pBlockStart = PH7_VmInstrLength(pGen->pVm);
	/* Compile the block until we hit a case/default/endswitch keyword
	 * or the '}' token */
	for(;;){
		if( pGen->pIn >= pGen->pEnd ){
			/* No more input to process */
			break;
		}
		rc = SXRET_OK;
		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){
				if( iTokenDelim != PH7_TK_CCB ){
					/* Unexpected token */
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",
						&pGen->pIn->sData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					/* FALL THROUGH */
				}
				rc = SXERR_EOF;
				break;
			}
		}else{
			sxi32 nKwrd;
			/* Extract the keyword */
			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKwrd == PH7_TKWRD_CASE || nKwrd == PH7_TKWRD_DEFAULT ){
				break;
			}
			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){
				if( iTokenDelim != PH7_TK_KEYWORD ){
					/* Unexpected token */
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",
						&pGen->pIn->sData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					/* FALL THROUGH */
				}
				/* Block compiled */
				break;
			}
		}
		/* Compile block */
		rc = PH7_CompileBlock(&(*pGen),0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return rc;
}
/*
 * Compile a case eXpression.
 *  (See block-comment below for more information)
 */
static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)
{
	SySet *pInstrContainer;
	SyToken *pEnd,*pTmp;
	sxi32 iNest = 0;
	sxi32 rc;
	/* Delimit the expression */
	pEnd = pGen->pIn;
	while( pEnd < pGen->pEnd ){
		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){
			/* Increment nesting level */
			iNest++;
		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){
			/* Decrement nesting level */
			iNest--;
		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_COLON/*;'*/) && iNest < 1 ){
			break;
		}
		pEnd++;
	}
	if( pGen->pIn >= pEnd ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	/* Update token stream */
	pGen->pIn  = pEnd;
	pGen->pEnd = pTmp;
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXRET_OK;
}
/*
 * Compile the smart switch statement.
 * According to the PHP language reference manual
 *  The switch statement is similar to a series of IF statements on the same expression.
 *  In many occasions, you may want to compare the same variable (or expression) with many
 *  different values, and execute a different piece of code depending on which value it equals to.
 *  This is exactly what the switch statement is for.
 *  Note: Note that unlike some other languages, the continue statement applies to switch and acts
 *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration
 *  of the outer loop, use continue 2.
 *  Note that switch/case does loose comparision.
 *  It is important to understand how the switch statement is executed in order to avoid mistakes.
 *  The switch statement executes line by line (actually, statement by statement).
 *  In the beginning, no code is executed. Only when a case statement is found with a value that
 *  matches the value of the switch expression does PHP begin to execute the statements.
 *  PHP continues to execute the statements until the end of the switch block, or the first time
 *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.
 *  In a switch statement, the condition is evaluated only once and the result is compared to each
 *  case statement. In an elseif statement, the condition is evaluated again. If your condition
 *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.
 *  The statement list for a case can also be empty, which simply passes control into the statement
 *  list for the next case.
 *  The case expression may be any expression that evaluates to a simple type, that is, integer
 *  or floating-point numbers and strings.
 */
static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)
{
	GenBlock *pSwitchBlock;
	SyToken *pTmp,*pEnd;
	ph7_switch *pSwitch;
	sxu32 nToken;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'switch' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	pEnd = 0; /* cc warning */
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP|GEN_BLOCK_SWITCH,
		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Delimit the condition */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"Switch: Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	if( pGen->pIn >= pGen->pEnd || &pGen->pIn[1] >= pGen->pEnd ||
		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/|PH7_TK_COLON/*:*/)) == 0 ){
			pTmp = pGen->pIn;
			if( pTmp >= pGen->pEnd ){
				pTmp--;
			}
			/* Unexpected token */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
	}
	/* Set the delimiter token */
	if( pGen->pIn->nType & PH7_TK_COLON ){
		nToken = PH7_TK_KEYWORD;
		/* Stop compilation when the 'endswitch;' keyword is seen */
	}else{
		nToken = PH7_TK_CCB; /* '}' */
	}
	pGen->pIn++; /* Jump the leading curly braces/colons */
	/* Create the switch blocks container */
	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));
	if( pSwitch == 0 ){
		/* Abort compilation */
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	/* Zero the structure */
	SyZero(pSwitch,sizeof(ph7_switch));
	/* Initialize fields */
	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));
	/* Emit the switch instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);
	/* Compile case blocks */
	for(;;){
		sxu32 nKwrd;
		if( pGen->pIn >= pGen->pEnd ){
			/* No more input to process */
			break;
		}
		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			if( nToken != PH7_TK_CCB || (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){
				/* Unexpected token */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",
					&pGen->pIn->sData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				/* FALL THROUGH */
			}
			/* Block compiled */
			break;
		}
		/* Extract the keyword */
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){
			if( nToken != PH7_TK_KEYWORD ){
				/* Unexpected token */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",
					&pGen->pIn->sData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				/* FALL THROUGH */
			}
			/* Block compiled */
			break;
		}
		if( nKwrd == PH7_TKWRD_DEFAULT ){
			/*
			 * Accroding to the PHP language reference manual
			 *  A special case is the default case. This case matches anything
			 *  that wasn't matched by the other cases.
			 */
			if( pSwitch->nDefault > 0 ){
				/* Default case already compiled */
				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			pGen->pIn++; /* Jump the 'default' keyword */
			/* Compile the default block */
			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);
			if( rc == SXERR_ABORT){
				return SXERR_ABORT;
			}else if( rc == SXERR_EOF ){
				break;
			}
		}else if( nKwrd == PH7_TKWRD_CASE ){
			ph7_case_expr sCase;
			/* Standard case block */
			pGen->pIn++; /* Jump the 'case' keyword */
			/* initialize the structure */
			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
			/* Compile the case expression */
			rc = GenStateCompileCaseExpr(pGen,&sCase);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			/* Compile the case block */
			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);
			/* Insert in the switch container */
			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);
			if( rc == SXERR_ABORT){
				return SXERR_ABORT;
			}else if( rc == SXERR_EOF ){
				break;
			}
		}else{
			/* Unexpected token */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",
				&pGen->pIn->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			break;
		}
	}
	/* Fix all jumps now the destination is resolved */
	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);
	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	if( pGen->pIn < pGen->pEnd ){
		/* Jump the trailing curly braces or the endswitch keyword*/
		pGen->pIn++;
	}
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
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
		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */
		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */
		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */
		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */
		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */
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
				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ || pInstr->iOp == PH7_OP_NEW ){
					/* Method call,flag that */
					pInstr->iP2 = 1;
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
			if( !isSpecial ){
				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);
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
						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);
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
static sxi32 PH7_CompileExpr(
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
static void GenStateSetPendingDoc(ph7_gen_state *pGen)
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
static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)
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
static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)
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
static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)
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
static sxi32 GenStateCompileChunk(
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
	sxi32 rc;
	if( pScript->nByte < 1 ){
		/* Nothing to compile */
		return PH7_OK;
	}
	/* Each compiled file has its own strict_types scope. Save the outer
	 * file's flags so include/require restore them on return. */
	pCodeGen = &pVm->sCodeGen;
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
		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);
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
	if( nErrType == E_ERROR ){
		/* Increment the error counter */
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
