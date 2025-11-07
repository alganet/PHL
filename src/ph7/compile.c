/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PH7_AMALGAMATION
#include "ph7int.h"
#endif
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
/* Forward declaration */
static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);
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
static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyToken *pToken = pGen->pIn; /* Raw token */
	sxu32 nIdx = 0;
	if( pToken->nType & PH7_TK_INTEGER ){
		ph7_value *pObj;
		sxi64 iValue;
		iValue = PH7_TokenValueToInt64(&pToken->sData);
		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);
		if( pObj == 0 ){
			SXUNUSED(iCompileFlag); /* cc warning */
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);
	}else{
		/* Real number */
		ph7_value *pObj;
		/* Reserve a new constant */
		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
		if( pObj == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromString(pGen->pVm,pObj,&pToken->sData);
		PH7_MemObjToReal(pObj);
	}
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
		/* Empty string,load NULL */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
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
static sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */
	ph7_value *pObj;
	sxu32 nIdx;
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
	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);
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
 *  \f form feed (FF or 0x0C (12) in ASCII)
 *  \\ backslash
 *  \$ dollar sign
 *  \" double-quote
 *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation
 *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation
 * As in single quoted strings, escaping any other character will result in the backslash being printed too.
 * The most important feature of double-quoted strings is the fact that variable names will be expanded.
 * See string parsing for details.
 */
static sxi32 GenStateCompileString(ph7_gen_state *pGen)
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
		/* Empty string,load NULL */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
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
			if( zIn >= zEnd ){
				break;
			}
			if( pObj == 0 ){
				pObj = GenStateNewStrObj(&(*pGen),&iCons);
				if( pObj == 0 ){
					return SXERR_ABORT;
				}
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
			case 'a':
				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */
				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));
				break;
			case 'b':
				/* Backspace (BS)[ctrl+h] ASCII code 8 */
				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));
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
			case '\'':
				/* Single quote */
				PH7_MemObjStringAppend(pObj,"'",sizeof(char));
				break;
			case '"':
				/* Double quote */
				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));
				break;
			case '0':
				/* NUL byte */
				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));
				break;
			case 'x':
				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){
					int c;
					/* Hex digit */
					c = SyHexToint(zIn[1]) << 4;
					if( &zIn[2] < zEnd ){
						c +=  SyHexToint(zIn[2]);
					}
					/* Output char */
					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));
					n += sizeof(char) * 2;
				}else{
					/* Output literal character  */
					PH7_MemObjStringAppend(pObj,"x",sizeof(char));
				}
				break;
			case 'o':
				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){
					/* Octal digit stream */
					int c;
					c = 0;
					zIn++;
					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){
						if( zPtr >= zEnd || (unsigned char)zPtr[0] >= 0xc0 || !SyisDigit(zPtr[0]) || (zPtr[0] - '0') > 7 ){
							break;
						}
						c = c * 8 + (zPtr[0] - '0');
					}
					if ( c > 0 ){
						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));
					}
					n = (sxu32)(zPtr-zIn);
				}else{
					/* Output literal character  */
					PH7_MemObjStringAppend(pObj,"o",sizeof(char));
				}
				break;
			default:
				/* Output without a slash */
				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));
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
	rc = GenStateCompileString(&(*pGen));
	SXUNUSED(iCompileFlag); /* cc warning */
	/* Compilation result */
	return rc;
}
/*
 * Compile a Heredoc string.
 *  See the block-comment above for more information.
 */
static sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxi32 rc;
	rc = GenStateCompileString(&(*pGen));
	SXUNUSED(iCompileFlag); /* cc warning */
	/* Compilation result */
	return SXRET_OK;
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
	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */
	SyToken *pKey,*pCur;
	sxi32 iEmitRef = 0;
	sxi32 nPair = 0;
	sxi32 iNest;
	sxi32 rc;
	/* Jump the 'array' keyword,the leading left parenthesis and the trailing parenthesis.
	 */
	pGen->pIn += 2;
	pGen->pEnd--;
	xValidator = 0;
	SXUNUSED(iCompileFlag); /* cc warning */
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
		iNest = 0;
		while( pCur < pGen->pIn ){
			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){
				break;
			}
			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){
				iNest++;
			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){
				/* Don't worry about mismatched parenthesis here,the expression 
				 * parser will shortly detect any syntax error.
				 */
				iNest--;
			}
			pCur++;
		}
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
		/* Compile indice value */
		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( iEmitRef ){
			/* Emit the load reference instruction */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);
		}
		xValidator = 0;
		iEmitRef = 0;
		nPair++;
	}
	/* Emit the load map instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);
	/* Node successfully compiled */
	return SXRET_OK;
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
PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyToken *pNext;
	sxi32 nExpr;
	sxi32 rc;
	nExpr = 0;
	/* Jump the 'list' keyword,the leading left parenthesis and the trailing parenthesis */
	pGen->pIn += 2;
	pGen->pEnd--;
	SXUNUSED(iCompileFlag); /* cc warning */
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){
		if( pGen->pIn < pNext ){
			/* Compile the expression holding the variable */
			rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);
			if( rc != SXRET_OK ){
				/* Do not bother compiling this expression, it's broken anyway */
				return SXRET_OK;
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
	/* Node successfully compiled */
	return SXRET_OK;
}
/* Forward declaration */
static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);
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
	ph7_vm_func *pAnnonFunc; /* Annonymous function body */
	char zName[512];         /* Unique lambda name */
	static int iCnt = 1;     /* There is no worry about thread-safety here,because only
							  * one thread is allowed to compile the script.
						      */
	ph7_value *pObj;
	SyString sName;
	sxu32 nIdx;
	sxu32 nLen;
	sxi32 rc;

	pGen->pIn++; /* Jump the 'function' keyword */
	if( pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD) ){
		pGen->pIn++;
	}
	/* Reserve a constant for the lambda */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");
		SXUNUSED(iCompileFlag); /* cc warning */
		return SXERR_ABORT;
	}
	/* Generate a unique name */
	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	/* Make sure the generated name is unique */
	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){
		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	}
	SyStringInitFromBuf(&sName,zName,nLen);
	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);
	/* Compile the lambda body */
	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){
		/* Emit the load closure instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);
	}else{
		/* Emit the load constant instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	}
	/* Node successfully compiled */
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
		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);
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
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);
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
	sxu32 nLine = pGen->pIn->nLine;
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
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid variable name");
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
			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid variable name");
			return SXRET_OK;
		}
		/* Compile the expression holding the variable name */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc == SXERR_EMPTY ){
			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing variable name");
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
				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
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
		ph7_value *pObj;
		/* Unknown literal,install it in the literal table */
		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
		if( pObj == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromString(pGen->pVm,pObj,&pToken->sData);
		GenStateInstallLiteral(&(*pGen),pObj,nIdx);
	}
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);
	return SXRET_OK;
}
/*
 * Resolve a namespace path or simply load a literal:
 * As of this version namespace support is disabled. If you need
 * a working version that implement namespace,please contact
 * symisc systems via contact@symisc.net
 */
static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)
{
	int emit = 0;
	sxi32 rc;
	while( pGen->pIn < &pGen->pEnd[-1] ){
		/* Emit a warning */
		if( !emit ){
			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,
				"Namespace support is disabled in the current release of the PH7(%s) engine",
				ph7_lib_version()
				);
			emit = 1;
		}
		pGen->pIn++; /* Ignore the token */
	}
	/* Load literal */
	rc = GenStateLoadLiteral(&(*pGen));	
	return rc;
}
/*
 * Compile a literal which is an identifier(name) for a simple value.
 */
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
	sxu32 nLine = pGen->pIn->nLine;
	SyString *pName;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'const' keyword */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_SSTR|PH7_TK_DSTR|PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		/* Invalid constant name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"const: Invalid constant name");
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
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"const: Cannot redeclare a reserved constant '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++;
	if(pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){
		/* Invalid statement*/
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"const: Expected '=' after constant name");
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
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
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
	/* Register the constant */
	rc = PH7_VmRegisterConstant(pGen->pVm,pName,PH7_VmExpandConstantValue,pConsCode);
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
static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)
{
	GenBlock *pLoop; /* Target loop */
	sxi32 iLevel;    /* How many nesting loop to skip */
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	iLevel = 0;
	/* Jump the 'continue' keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){
		/* optional numeric argument which tells us how many levels
		 * of enclosing loops we should skip to the end of. 
		 */
		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);
		if( iLevel < 2 ){
			iLevel = 0;
		}
		pGen->pIn++; /* Jump the optional numeric argument */
	}
	/* Point to the target loop */
	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);
	if( pLoop == 0 ){
		/* Illegal continue */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"A 'continue' statement may only be used within a loop or switch");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}else{
		sxu32 nInstrIdx = 0;
		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){
			/* According to the PHP language reference manual
			 *  Note that unlike some other languages, the continue statement applies to switch 
			 *  and acts similar to break. If you have a switch inside a loop and wish to continue
			 *  to the next iteration of the outer loop, use continue 2. 
			 */
			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);
			if( rc == SXRET_OK ){
				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);
			}
		}else{
			/* Emit the unconditional jump to the beginning of the target loop */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);
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
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	iLevel = 0;
	/* Jump the 'break' keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){
		/* optional numeric argument which tells us how many levels
		 * of enclosing loops we should skip to the end of. 
		 */
		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);
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
		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);
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
		/* Reset the token set */
		SySetReset(pTokenSet);
		/* Tokenize input */
		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),
			pGen->pRawIn->nLine,pTokenSet);
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
	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){
		sxu32 nLine = pGen->pIn->nLine;
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
	/* Check if we have a key field */
	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){
		pCur++;
	}
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
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);
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
	sxu32 nLine = pGen->pIn->nLine;
	sxi32 rc;
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
static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'namespace' keyword */
	if( pGen->pIn >= pGen->pEnd ||
		(pGen->pIn->nType & (PH7_TK_NSSEP|PH7_TK_ID|PH7_TK_KEYWORD|PH7_TK_SEMI/*';'*/|PH7_TK_OCB/*'{'*/)) == 0 ){
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){
				pTok--;
			}
			/* Unexpected token */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Namespace: Unexpected token '%z'",&pTok->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
	}
	/* Ignore the path */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP/*'\'*/|PH7_TK_ID|PH7_TK_KEYWORD)) ){
		pGen->pIn++;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_OCB/*'{'*/)) == 0 ){
		/* Unexpected token */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Namespace: Unexpected token '%z',expecting ';' or '{'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Emit a warning */
	PH7_GenCompileError(&(*pGen),E_WARNING,nLine,
		"Namespace support is disabled in the current release of the PH7(%s) engine",ph7_lib_version());
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
	sxu32 nLine = pGen->pIn->nLine;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'use' keyword */
	/* Assemeble one or more real namespace path */
	for(;;){
		if( pGen->pIn >= pGen->pEnd ){
			break;
		}
		/* Ignore the path */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP|PH7_TK_ID))  ){
			pGen->pIn++;
		}
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA/*','*/) ){
			pGen->pIn++; /* Jump the comma and process the next path */
		}else{
			break;
		}
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){
		pGen->pIn++; /* Jump the 'as' keyword */
		/* Compile one or more aliasses */
		for(;;){
			if( pGen->pIn >= pGen->pEnd ){
				break;
			}
			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP|PH7_TK_ID)) ){
				pGen->pIn++;
			}
			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA/*','*/) ){
				pGen->pIn++; /* Jump the comma and process the next alias */
			}else{
				break;
			}
		}
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){
		/* Unexpected token */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"use statement: Unexpected token '%z',expecting ';'",
			&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Emit a notice */
	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,
		"Namespace support is disabled in the current release of the PH7(%s) engine",
		ph7_lib_version()
		);
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
static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	SyToken *pEnd = 0; /* cc warning */
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
	/* Delimit the directive */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);
	if( pEnd >= pGen->pEnd ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Update the cursor */
	pGen->pIn = &pEnd[1];
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_OCB/*'{'*/)) == 0  ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */
	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */
		"the declare construct is a no-op in the current release of the PH7(%s) engine",
		ph7_lib_version()
		);
	/*All done */
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
static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)
{
	ph7_vm_func_arg sArg; /* Current processed argument */
	SyToken *pCur,*pIn;  /* Token stream */
	SyBlob sSig;         /* Function signature */
	char *zDup;          /* Copy of argument name */
	sxi32 rc;

	pIn = pGen->pIn;
	pCur = 0;
	SyBlobInit(&sSig,&pGen->pVm->sAllocator);
	/* Process arguments one after one */
	for(;;){
		if( pIn >= pEnd ){
			/* No more arguments to process */
			break;
		}
		SyZero(&sArg,sizeof(ph7_vm_func_arg));
		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
		if( pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD) ){
			if( pIn->nType & PH7_TK_KEYWORD ){
				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));
				if( nKey & PH7_TKWRD_ARRAY ){
					sArg.nType = MEMOBJ_HASHMAP;
				}else if( nKey & PH7_TKWRD_BOOL ){
					sArg.nType = MEMOBJ_BOOL;
				}else if( nKey & PH7_TKWRD_INT ){
					sArg.nType = MEMOBJ_INT;
				}else if( nKey & PH7_TKWRD_STRING ){
					sArg.nType = MEMOBJ_STRING;
				}else if( nKey & PH7_TKWRD_FLOAT ){
					sArg.nType = MEMOBJ_REAL;
				}else{
					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,
						"Invalid argument type '%z',Automatic cast will not be performed",
						&pIn->sData);
				}
			}else{
				SyString *pName = &pIn->sData; /* Class name */
				char *zDup;
				/* Argument must be a class instance,record that*/
				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
				if( zDup ){
					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */
					SyStringInitFromBuf(&sArg.sClass,zDup,pName->nByte);
				}
			}
			pIn++;
		}
		if( pIn >= pEnd ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");
			return rc;
		}
		if( pIn->nType & PH7_TK_AMPER ){
			/* Pass by reference,record that */
			sArg.iFlags = VM_FUNC_ARG_BY_REF;
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
				/* Class name */
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
	/* Compile the body */
	PH7_CompileBlock(&(*pGen),0);
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
	/* function ID */
	zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zName == 0 ){
		/* Don't worry about freeing memory, everything will be released shortly */
		goto OutOfMem;
	}
	/* Initialize the function state */
	PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);
	if( pGen->pIn < pEnd ){
		/* Collect function arguments */
		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
	}
	/* Compile function body */
	pGen->pIn = &pEnd[1];
	if( bHandleClosure ){
		ph7_vm_func_closure_env sEnv;
		int got_this = 0; /* TRUE if $this have been seen */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) 
			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){
				sxu32 nLine = pGen->pIn->nLine;
				/* Closure,record environment variable */
				pGen->pIn++;
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Closure: Unexpected token. Expecting a left parenthesis '('");
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */
				/* Compile until we hit the first closing parenthesis */
				while( pGen->pIn < pGen->pEnd ){
					int iFlags = 0;
					if( pGen->pIn->nType & PH7_TK_RPAREN ){
						pGen->pIn++; /* Jump the closing parenthesis */
						break;
					}
					nLine = pGen->pIn->nLine;
					if( pGen->pIn->nType & PH7_TK_AMPER ){
						/* Pass by reference,record that */
						PH7_GenCompileError(pGen,E_WARNING,nLine,
							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"
							);
						iFlags = VM_FUNC_ARG_BY_REF;
						pGen->pIn++;
					}
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 || &pGen->pIn[1] >= pGen->pEnd
						|| (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
							rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
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
						SyString *pName; 
						char *zDup;
						/* Duplicate variable name */
						pName = &pGen->pIn[1].sData;
						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
						if( zDup ){
							/* Zero the structure */
							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
							sEnv.iFlags = iFlags;
							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
							SyStringInitFromBuf(&sEnv.sName,zDup,pName->nByte);
							if( !got_this && pName->nByte == sizeof("this")-1 && 
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
				if( !got_this ){
					/* Make the $this variable [Current processed Object (class instance)]  
					 * available to the closure environment.
					 */
					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */
					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);
					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);
				}
				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){
					/* Mark as closure */
					pFunc->iFlags |= VM_FUNC_CLOSURE;
				}
		}
	}
	/* Compile the body */
	rc = GenStateCompileFuncBody(&(*pGen),pFunc);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
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
	sxu32 nLine;
	sxi32 rc;

	nLine = pGen->pIn->nLine;
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
	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);
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
static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)
{
	sxu32 nLine = pGen->pIn->nLine;
	SySet *pInstrContainer;
	ph7_class_attr *pCons;
	SyString *pName;
	sxi32 rc;
	/* Extract visibility level */
	iProtection = GetProtectionLevel(iProtection);
	pGen->pIn++; /* Jump the 'const' keyword */
loop:
	/* Mark as constant */
	iFlags |= PH7_CLASS_ATTR_CONSTANT;
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
	/* Allocate a new class attribute */
	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);
	if( pCons == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
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
	return SXRET_OK;
Synchronize:
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
static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class_attr *pAttr;
	SyString *pName;
	sxi32 rc;
	/* Extract visibility level */
	iProtection = GetProtectionLevel(iProtection);
loop:
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
	/* Allocate a new class attribute */
	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);
	if( pAttr == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
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
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){
		pGen->pIn++;
	}
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
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID)) == 0 ){
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
	if( pGen->pIn < pEnd ){
		/* Collect method arguments */
		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Point beyond method signature */
	pGen->pIn = &pEnd[1];
	if( doBody ){
		/* Compile method body */
		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}else{
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
	/* Obtain a raw class */
	pClass = PH7_NewRawClass(pGen->pVm,pName,nLine);
	if( pClass == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	/* Mark as an interface */
	pClass->iFlags = PH7_CLASS_INTERFACE;
	/* Assume no base class is given */
	pBase = 0;
	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){
			SyString *pBaseName;
			/* Extract base interface */
			pGen->pIn++;
			if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
				/* Syntax error */
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",
					pName);
				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			pBaseName = &pGen->pIn->sData;
			pBase = PH7_VmExtractClass(pGen->pVm,pBaseName->zString,pBaseName->nByte,FALSE,0);
			/* Only interfaces is allowed */
			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){
				pBase = pBase->pNextName;
			}
			if( pBase == 0 ){
				/* Inexistant interface */
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
			}
			/* Advance the stream cursor */
			pGen->pIn++;
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
			/* Emit a warning and switch to public visibility */
			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");
			nKwrd = PH7_TKWRD_PUBLIC;
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
			sxi32 iFlags = 0;
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
			/* Process method signature */
			rc = GenStateCompileClassMethod(&(*pGen),0,FALSE/* Only method signature*/,iFlags,pClass);
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
static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class *pClass,*pBase;
	SyToken *pEnd,*pTmp;
	sxi32 iProtection;
	SySet aInterfaces;
	sxi32 iAttrflags;
	SyString *pName;
	sxi32 nKwrd;
	sxi32 rc;
	/* Jump the 'class' keyword */
	pGen->pIn++;
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
	/* Obtain a raw class */
	pClass = PH7_NewRawClass(pGen->pVm,pName,nLine);
	if( pClass == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	/* implemented interfaces container */
	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));
	/* Assume a standalone class */
	pBase = 0;
	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
		SyString *pBaseName;
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){
			pGen->pIn++; /* Advance the stream cursor */
			if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
				/* Syntax error */
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"Expected 'class_name' after 'extends' keyword inside class '%z'",
					pName);
				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			/* Extract base class name */
			pBaseName = &pGen->pIn->sData;
			/* Perform the query */
			pBase = PH7_VmExtractClass(pGen->pVm,pBaseName->zString,pBaseName->nByte,FALSE,0);
			/* Interfaces are not allowed */
			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){
				pBase = pBase->pNextName;
			}
			if( pBase == 0 ){
				/* Inexistant base class */
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
			}else{
				if( pBase->iFlags & PH7_CLASS_FINAL ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);
					if( rc == SXERR_ABORT ){
						/* Error count limit reached,abort immediately */
						return SXERR_ABORT;
					}
				}
			}
			/* Advance the stream cursor */
			pGen->pIn++;
		}
		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){
			ph7_class *pInterface;
			SyString *pIntName;
			/* Interface implementation */
			pGen->pIn++; /* Advance the stream cursor */
			for(;;){
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
					/* Syntax error */
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",
						pName);
					if( rc == SXERR_ABORT ){
						/* Error count limit reached,abort immediately */
						return SXERR_ABORT;
					}
					break;
				}
				/* Extract interface name */
				pIntName = &pGen->pIn->sData;
				/* Make sure the interface is already defined */
				pInterface = PH7_VmExtractClass(pGen->pVm,pIntName->zString,pIntName->nByte,FALSE,0);
				/* Only interfaces are allowed */
				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){
					pInterface = pInterface->pNextName;
				}
				if( pInterface == 0 ){
					/* Inexistant interface */
					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);
					if( rc == SXERR_ABORT ){
						/* Error count limit reached,abort immediately */
						return SXERR_ABORT;
					}
				}else{
					/* Register interface */
					SySetPut(&aInterfaces,(const void *)&pInterface);
				}
				/* Advance the stream cursor */
				pGen->pIn++;
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
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Set the inherited flags */
	pClass->iFlags = iFlags;
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
		if( (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR)) == 0 ){
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
		if( pGen->pIn->nType & PH7_TK_KEYWORD ){
			/* Extract the current keyword */
			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
				iProtection = nKwrd;
				pGen->pIn++; /* Jump the visibility token */
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR)) == 0 ){
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
						/* Extract the keyword */
						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
						if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
							iProtection = nKwrd;
							pGen->pIn++; /* Jump the visibility token */
						}
					}
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR)) == 0 ){
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
	}
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
static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)
{
	sxi32 rc;
	pGen->pIn++; /* Jump the 'abstract' keyword */
	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);
	return rc;
}
/*
 * Compile a user-defined final class.
 *  According to the PHP language reference manual
 *    PHP 5 introduces the final keyword, which prevents child classes from overriding
 *    a method by prefixing the definition with final. If the class itself is being defined
 *    final then it cannot be extended. 
 */
static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)
{
	sxi32 rc;
	pGen->pIn++; /* Jump the 'final' keyword */
	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);
	return rc;
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
		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */
			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){
			/* Unexpected expression */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
				"throw: Expecting an exception class instance");
			if( rc != SXERR_ABORT ){
				rc = SXERR_INVALID;
			}
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
 * Compile a 'catch' block.
 * Catch: A "catch" block retrieves an exception and creates
 * an object containing the exception information.
 */
static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_exception_block sCatch;
	SySet *pInstrContainer;
	GenBlock *pCatch;
	SyToken *pToken;
	SyString *pName;
	char *zDup;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'catch' keyword */
	/* Zero the structure */
	SyZero(&sCatch,sizeof(ph7_exception_block));
	/* Initialize fields */
	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ||
		&pGen->pIn[1] >= pGen->pEnd || (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			/* Unexpected token,break immediately */
			pToken = pGen->pIn;
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,
				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_INVALID;
	}
	/* Extract the exception class */
	pGen->pIn++; /* Jump the left parenthesis '(' */
	/* Duplicate class name */
	pName = &pGen->pIn->sData;
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zDup == 0 ){
		goto Mem;
	}
	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ ||
		&pGen->pIn[1] >= pGen->pEnd || (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			/* Unexpected token,break immediately */
			pToken = pGen->pIn;
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,
				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);
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
		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,
			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);
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
	pException->pVm = pGen->pVm;
	/* Create the try block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
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
	/* Compile the catch block */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 || 
		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){
				pTok--; /* Point back */
			}
			/* Unexpected token */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTok->nLine,
				"Try: Unexpected token '%z',expecting 'catch' block",&pTok->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXRET_OK;
	}
	/* Compile one or more catch blocks */
	for(;;){
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0
			|| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){
				/* No more blocks */
				break;
		}
		/* Compile the catch block */
		rc = PH7_CompileCatch(&(*pGen),pException);
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
	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){
		sxu32 nJz,nJmp;
		/* Ternary operator require special handling */
		/* Phase#1: Compile the condition */
		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);
		if( rc != SXRET_OK ){
			return rc;
		}
		nJz = nJmp = 0; /* cc -O6 warning */
		/* Phase#2: Emit the false jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);
		if( pNode->pLeft ){
			/* Phase#3: Compile the 'then' expression  */
			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
		/* Phase#4: Emit the unconditional jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);
		/* Phase#5: Fix the false jump now the jump destination is resolved. */
		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);
		if( pInstr ){
			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);
		}
		/* Phase#6: Compile the 'else' expression */
		if( pNode->pRight ){
			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);
			if( rc != SXRET_OK ){
				return rc;
			}
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
	/* Generate code for the left tree */
	if( pNode->pLeft ){
		if( iVmOp == PH7_OP_CALL ){
			ph7_expr_node **apNode;
			sxi32 n;
			/* Recurse and generate bytecodes for function arguments */
			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);
			/* Read-only load */
			iFlags |= EXPR_FLAG_RDONLY_LOAD;
			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){
				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);
				if( rc != SXRET_OK ){
					return rc;
				}
			}
			/* Total number of given arguments */
			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);
			/* Remove stale flags now */
			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;
		}
		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( iVmOp == PH7_OP_CALL ){
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr ){
				if ( pInstr->iOp == PH7_OP_LOADC ){
					/* Prevent constant expansion */
					pInstr->iP1 = 0;
				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ || pInstr->iOp == PH7_OP_NEW ){
					/* Method call,flag that */
					pInstr->iP2 = 1;
				}
			}
		}else if( iVmOp == PH7_OP_LOAD_IDX ){
			ph7_expr_node **apNode;
			sxi32 n;
			/* Recurse and generate bytecodes for array index */
			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);
			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){
				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);
				if( rc != SXRET_OK ){
					return rc;
				}
			}
			if( SySetUsed(&pNode->aNodeArgs) > 0 ){
				iP1 = 1; /* Node have an index associated with it */
			}
			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){
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
	/* Generate code for the right tree */
	if( pNode->pRight ){
		if( iVmOp == PH7_OP_LAND ){
			/* Emit the false jump so we can short-circuit the logical and */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);
		}else if (iVmOp == PH7_OP_LOR ){
			/* Emit the true jump so we can short-circuit the logical or*/
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);
		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){
			iFlags |= EXPR_FLAG_LOAD_IDX_STORE;
		}
		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);
		if( iVmOp == PH7_OP_STORE ){
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr ){
				if( pInstr->iOp == PH7_OP_LOAD_LIST ){
					/* Hide the STORE instruction */
					iVmOp = 0;
				}else if(pInstr->iOp == PH7_OP_MEMBER ){
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
	if( iVmOp > 0 ){
		if( iVmOp == PH7_OP_INCR || iVmOp == PH7_OP_DECR ){
			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){
				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */
				iP1 = 1;
			}
		}else if( iVmOp == PH7_OP_NEW ){
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr && pInstr->iOp == PH7_OP_CALL ){
				VmInstr *pPrev;
				pPrev = PH7_VmPeekNextInstr(pGen->pVm);
				if( pPrev == 0 || pPrev->iOp != PH7_OP_MEMBER ){
					/* Pop the call instruction */
					iP1 = pInstr->iP1;
					(void)PH7_VmPopInstr(pGen->pVm);
				}
			}
		}else if( iVmOp == PH7_OP_MEMBER){
			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){
				/* Static member access,remember that */
				iP1 = 1;
				pInstr = PH7_VmPeekInstr(pGen->pVm);
				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){
					p3 = pInstr->p3;
					(void)PH7_VmPopInstr(pGen->pVm);
				}
			}
		}
		/* Finally,emit the VM instruction associated with this operator */
		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);
		if( nJmpIdx > 0 ){
			/* Fix short-circuited jumps now the destination is resolved */
			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);
			if( pInstr ){
				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);
			}
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
	/* Initialize worker variables */
	nExpr = 0;
	pRoot = 0;
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
	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  }   /* declare statement */
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
		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){
				return PH7_CompileAbstractClass;
		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){
				return PH7_CompileFinalClass;
		}
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
static sxi32 GenStateCompileChunk(
	ph7_gen_state *pGen, /* Code generator state */
	sxi32 iFlags         /* Compile flags */
	)
{
	ProcLangConstruct xCons;
	sxi32 rc;
	rc = SXRET_OK; /* Prevent compiler warning */
	for(;;){
		if( pGen->pIn >= pGen->pEnd ){
			/* No more input to process */
			break;
		}
		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){
			/* Compile block */
			rc = PH7_CompileBlock(&(*pGen),0);
			if( rc == SXERR_ABORT ){
				break;
			}
		}else{
			xCons = 0;
			if( pGen->pIn->nType & PH7_TK_KEYWORD ){
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
	/* Reset the token set */
	SySetReset(&(*pTokenSet));
	/* Mark as the default token set */
	pGen->pTokenSet = &(*pTokenSet);
	/* Advance the stream cursor */
	pGen->pRawIn++;
	/* Tokenize the PHP chunk first */
	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));
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
	sxi32 rc;
	if( pScript->nByte < 1 ){
		/* Nothing to compile */
		return PH7_OK;
	}
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
	pCodeGen = &pVm->sCodeGen;
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
	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);
	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);
	/* Error log buffer */
	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);
	/* General purpose working buffer */
	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);
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
	SyBlobRelease(&pGen->sErrBuf);
	SyBlobRelease(&pGen->sWorker);
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
	if( pFile && pGen->xErr ){
		/* Append file name */
		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);
		SyBlobAppend(pWorker,(const void *)": ",sizeof(": ")-1);
	}
	if( nErrType == E_ERROR ){
		/* Increment the error counter */
		pGen->nErr++;
		if( pGen->nErr > 15 ){
			/* Error count limit reached */
			if( pGen->xErr ){
				SyBlobFormat(pWorker,"%u Error count limit reached,PH7 is aborting compilation\n",nLine);
				if( SyBlobLength(pWorker) > 0 ){
					/* Consume the generated error message */
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
	/* Format the error message */
	SyBlobFormat(pWorker,"%u %s: ",nLine,zErr);
	va_start(ap,zFormat);
	SyBlobFormatAp(pWorker,zFormat,ap);
	va_end(ap);
	/* Append a new line */
	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));
	if( SyBlobLength(pWorker) > 0 ){
		/* Consume the generated error message */
		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);
	}
	return rc;
}